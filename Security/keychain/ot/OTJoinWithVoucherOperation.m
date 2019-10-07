/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#if OCTAGON

#import <utilities/debugging.h>

#import <CloudKit/CloudKit_Private.h>

#import "keychain/ot/OTJoinWithVoucherOperation.h"
#import "keychain/ot/OTOperationDependencies.h"
#import "keychain/ot/OTFetchCKKSKeysOperation.h"
#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/ckks/CloudKitCategories.h"

#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/ObjCImprovements.h"
#import "keychain/ot/OTStates.h"

@interface OTJoinWithVoucherOperation ()
@property OTOperationDependencies* deps;

@property OctagonState* ckksConflictState;

@property NSOperation* finishedOp;
@property int retries;
@property int maxRetries;
@property int delay;
@property CKKSNearFutureScheduler* retrySched;
@end

@implementation OTJoinWithVoucherOperation

@synthesize intendedState = _intendedState;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                   ckksConflictState:(OctagonState*)ckksConflictState
                          errorState:(OctagonState*)errorState
                         voucherData:(NSData*)voucherData
                          voucherSig:(NSData*)voucherSig
                     preapprovedKeys:(NSArray<NSData *>*)preapprovedKeys
{
    if((self = [super init])) {
        _deps = dependencies;

        _retries = 0;
        _maxRetries = 5;
        _delay = 1;

        _intendedState = intendedState;
        _nextState = errorState;
        _ckksConflictState = ckksConflictState;

        _voucherData = voucherData;
        _voucherSig = voucherSig;
        _preapprovedKeys = preapprovedKeys;
    }
    return self;
}

- (void)groupStart
{
    secnotice("octagon", "joining");

    self.finishedOp = [[NSOperation alloc] init];
    [self dependOnBeforeGroupFinished:self.finishedOp];

    WEAKIFY(self);

    OTFetchCKKSKeysOperation* fetchKeysOp = [[OTFetchCKKSKeysOperation alloc] initWithDependencies:self.deps];
    [self runBeforeGroupFinished:fetchKeysOp];

    CKKSResultOperation* proceedWithKeys = [CKKSResultOperation named:@"vouch-with-keys"
                                                            withBlock:^{
                                                                STRONGIFY(self);
                                                                [self proceedWithKeys:fetchKeysOp.viewKeySets
                                                                     pendingTLKShares:fetchKeysOp.pendingTLKShares];
                                                            }];

    [proceedWithKeys addDependency:fetchKeysOp];
    [self runBeforeGroupFinished:proceedWithKeys];
}

- (BOOL)isRetryable:(NSError* _Nonnull)error {
    return [error isCuttlefishError:CuttlefishErrorTransactionalFailure];
}

- (int)retryDelay:(NSError* _Nonnull)error {
    NSError* underlyingError = error.userInfo[NSUnderlyingErrorKey];
    int ret = self->_delay;
    if (underlyingError) {
        id tmp = underlyingError.userInfo[@"retryafter"];
        if ([tmp isKindOfClass:[NSNumber class]]) {
            ret = [(NSNumber*)tmp intValue];
        }
    }
    ret = MAX(MIN(ret, 32), self->_delay);
    self->_delay *= 2;
    return ret;
}

- (void)proceedWithKeys:(NSArray<CKKSKeychainBackedKeySet*>*)viewKeySets pendingTLKShares:(NSArray<CKKSTLKShare*>*)pendingTLKShares
{
    WEAKIFY(self);

    [[self.deps.cuttlefishXPC remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        STRONGIFY(self);
        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
        [[CKKSAnalytics logger] logRecoverableError:error forEvent:OctagonEventJoinWithVoucher withAttributes:NULL];
        self.error = error;
        [self runBeforeGroupFinished:self.finishedOp];

    }] joinWithContainer:self.deps.containerName
                 context:self.deps.contextID
             voucherData:self.voucherData
              voucherSig:self.voucherSig
                ckksKeys:viewKeySets
               tlkShares:pendingTLKShares
         preapprovedKeys:self.preapprovedKeys
                   reply:^(NSString * _Nullable peerID, NSArray<CKRecord*>* keyHierarchyRecords, NSError * _Nullable error) {
        if(error){
            secerror("octagon: Error joining with voucher: %@", error);
            [[CKKSAnalytics logger] logRecoverableError:error forEvent:OctagonEventJoinWithVoucher withAttributes:NULL];

            if (self.retries < self.maxRetries && [self isRetryable:error]) {
                ++self.retries;
                if (!self.retrySched) {
                    self.retrySched = [[CKKSNearFutureScheduler alloc] initWithName:@"cuttlefish-join-retry"
                                                                              delay:1*NSEC_PER_SEC
                                                                   keepProcessAlive:true
                                                          dependencyDescriptionCode:CKKSResultDescriptionNone
                                                                              block:^{
                                                                                  CKKSResultOperation* proceedWithKeys = [CKKSResultOperation named:@"vouch-with-keys"
                                                                                                                                          withBlock:^{
                                                                                                                                              STRONGIFY(self);
                                                                                                                                              secnotice("octagon", "retrying (%d/%d) join", self.retries, self->_maxRetries);
                                                                                                                                              [self proceedWithKeys:viewKeySets
                                                                                                                                                   pendingTLKShares:pendingTLKShares];
                                                                                                                                          }];
                                                                                  STRONGIFY(self);
                                                                                  [self runBeforeGroupFinished:proceedWithKeys];
                                                                              }];
                }
                int delay_s = [self retryDelay:error];
                [self.retrySched waitUntil:delay_s*NSEC_PER_SEC];
                [self.retrySched trigger];
                return;
            }

            // IF this is a CKKS conflict error, don't retry
            if ([error isCuttlefishError:CuttlefishErrorKeyHierarchyAlreadyExists]) {
                secnotice("octagon-ckks", "A CKKS key hierarchy is out of date; going to state '%@'", self.ckksConflictState);
                self.nextState = self.ckksConflictState;
            } else {
                self.error = error;
            }
        } else {
            self.peerID = peerID;

            [[CKKSAnalytics logger] logSuccessForEventNamed:OctagonEventJoinWithVoucher];

            NSError* localError = nil;
            BOOL persisted = [self.deps.stateHolder persistAccountChanges:^OTAccountMetadataClassC * _Nonnull(OTAccountMetadataClassC * _Nonnull metadata) {
                metadata.trustState = OTAccountMetadataClassC_TrustState_TRUSTED;
                metadata.peerID = peerID;
                return metadata;
            } error:&localError];
            if(!persisted || localError) {
                secnotice("octagon", "Couldn't persist results: %@", localError);
                self.error = localError;
            } else {
                secerror("octagon: join successful");
                self.nextState = self.intendedState;
            }

            // Tell CKKS about our shiny new records!
            for (id key in self.deps.viewManager.views) {
                CKKSKeychainView* view = self.deps.viewManager.views[key];
                secnotice("octagon-ckks", "Providing join() records to %@", view);
                [view receiveTLKUploadRecords: keyHierarchyRecords];
            }
        }
        [self runBeforeGroupFinished:self.finishedOp];
    }];
}

@end

#endif // OCTAGON
