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

#import "keychain/ot/OTEstablishOperation.h"
#import "keychain/ot/OTCuttlefishAccountStateHolder.h"
#import "keychain/ot/OTFetchCKKSKeysOperation.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/ckks/CKKSCurrentKeyPointer.h"
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/SecureObjectSync/SOSAccount.h"

#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/ObjCImprovements.h"

@interface OTEstablishOperation ()
@property OTOperationDependencies* operationDependencies;

@property OctagonState* ckksConflictState;

@property NSOperation* finishedOp;
@end

@implementation OTEstablishOperation
@synthesize intendedState = _intendedState;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                   ckksConflictState:(OctagonState*)ckksConflictState
                          errorState:(OctagonState*)errorState
{
    if((self = [super init])) {
        _operationDependencies = dependencies;

        _intendedState = intendedState;
        _nextState = errorState;
        _ckksConflictState = ckksConflictState;
    }
    return self;
}

- (void)groupStart
{
    secnotice("octagon", "Beginning an establish operation");

    WEAKIFY(self);

    self.finishedOp = [NSBlockOperation blockOperationWithBlock:^{
        STRONGIFY(self);
        secnotice("octagon", "Finishing an establish operation with %@", self.error ?: @"no error");
    }];
    [self dependOnBeforeGroupFinished:self.finishedOp];

    // First, interrogate CKKS views, and see when they have a TLK proposal.
    OTFetchCKKSKeysOperation* fetchKeysOp = [[OTFetchCKKSKeysOperation alloc] initWithDependencies:self.operationDependencies
                                                                                     refetchNeeded:NO];
    [self runBeforeGroupFinished:fetchKeysOp];

    CKKSResultOperation* proceedWithKeys = [CKKSResultOperation named:@"establish-with-keys"
                                                            withBlock:^{
                                                                STRONGIFY(self);
                                                                [self proceedWithKeys:fetchKeysOp.viewKeySets
                                                                     pendingTLKShares:fetchKeysOp.pendingTLKShares];
                                                            }];

    [proceedWithKeys addDependency:fetchKeysOp];
    [self runBeforeGroupFinished:proceedWithKeys];
}

- (void)proceedWithKeys:(NSArray<CKKSKeychainBackedKeySet*>*)viewKeySets pendingTLKShares:(NSArray<CKKSTLKShare*>*)pendingTLKShares
{
    WEAKIFY(self);

    NSArray<NSData*>* publicSigningSPKIs = nil;
    if(self.operationDependencies.sosAdapter.sosEnabled) {
        NSError* sosPreapprovalError = nil;
        publicSigningSPKIs = [OTSOSAdapterHelpers peerPublicSigningKeySPKIsForCircle:self.operationDependencies.sosAdapter error:&sosPreapprovalError];

        if(publicSigningSPKIs) {
            secnotice("octagon-sos", "SOS preapproved keys are %@", publicSigningSPKIs);
        } else {
            secnotice("octagon-sos", "Unable to fetch SOS preapproved keys: %@", sosPreapprovalError);
        }

    } else {
        secnotice("octagon-sos", "SOS not enabled; no preapproved keys");
    }

    NSError* persistError = nil;
    BOOL persisted = [self.operationDependencies.stateHolder persistOctagonJoinAttempt:OTAccountMetadataClassC_AttemptedAJoinState_ATTEMPTED error:&persistError];
    if(!persisted || persistError) {
        secerror("octagon: failed to save 'attempted join' state: %@", persistError);
    }

    secnotice("octagon-ckks", "Beginning establish with keys: %@", viewKeySets);
    [self.operationDependencies.cuttlefishXPCWrapper establishWithContainer:self.operationDependencies.containerName
                                                                    context:self.operationDependencies.contextID
                                                                   ckksKeys:viewKeySets
                                                                  tlkShares:pendingTLKShares
                                                            preapprovedKeys:publicSigningSPKIs
                                                                      reply:^(NSString * _Nullable peerID,
                                                                              NSArray<CKRecord*>* _Nullable keyHierarchyRecords,
                                                                              TPSyncingPolicy* _Nullable syncingPolicy,
                                                                              NSError * _Nullable error) {
            STRONGIFY(self);

            [[CKKSAnalytics logger] logResultForEvent:OctagonEventEstablishIdentity hardFailure:true result:error];
            if(error) {
                secerror("octagon: Error calling establish: %@", error);

                if ([error isCuttlefishError:CuttlefishErrorKeyHierarchyAlreadyExists]) {
                    secnotice("octagon-ckks", "A CKKS key hierarchy is out of date; moving to '%@'", self.ckksConflictState);
                    self.nextState = self.ckksConflictState;
                } else {
                    self.error = error;
                }
                [self runBeforeGroupFinished:self.finishedOp];
                return;
            }

            self.peerID = peerID;

            NSError* localError = nil;
            BOOL persisted = [self.operationDependencies.stateHolder persistAccountChanges:^OTAccountMetadataClassC * _Nonnull(OTAccountMetadataClassC * _Nonnull metadata) {
                metadata.trustState = OTAccountMetadataClassC_TrustState_TRUSTED;
                metadata.peerID = peerID;
                [metadata setTPSyncingPolicy:syncingPolicy];
                return metadata;
            } error:&localError];

            if(!persisted || localError) {
                secnotice("octagon", "Couldn't persist results: %@", localError);
                self.error = localError;
            } else {
                self.nextState = self.intendedState;
            }

            [self.operationDependencies.viewManager setCurrentSyncingPolicy:syncingPolicy];

            // Tell CKKS about our shiny new records!
            for (id key in self.operationDependencies.viewManager.views) {
                CKKSKeychainView* view = self.operationDependencies.viewManager.views[key];
                secnotice("octagon-ckks", "Providing records to %@", view);
                [view receiveTLKUploadRecords: keyHierarchyRecords];
            }
            [self runBeforeGroupFinished:self.finishedOp];
        }];
}

@end

#endif // OCTAGON
