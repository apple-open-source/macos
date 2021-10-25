/*
 * Copyright (c) 2019 - 2020 Apple Inc. All Rights Reserved.
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

#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/OctagonTrust/OTCustodianRecoveryKey.h"
#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/OTClientStateMachine.h"
#import "keychain/ot/OTCreateCustodianRecoveryKeyOperation.h"
#import "keychain/ot/OTCuttlefishContext.h"
#import "keychain/ot/OTFetchCKKSKeysOperation.h"
#import "keychain/ot/ObjCImprovements.h"

#include <Security/SecPasswordGenerate.h>

@interface OTCreateCustodianRecoveryKeyOperation ()
@property OTOperationDependencies* deps;
@property NSUUID* uuid;
@property NSOperation* finishOp;
@end

@implementation OTCreateCustodianRecoveryKeyOperation

- (instancetype)initWithUUID:(NSUUID *_Nullable)uuid dependencies:(OTOperationDependencies*)dependencies
{
    if((self = [super init])) {
        _uuid = uuid ?: [[NSUUID alloc] init];
        _deps = dependencies;
    }
    return self;
}

- (void)groupStart
{
    self.finishOp = [[NSOperation alloc] init];
    [self dependOnBeforeGroupFinished:self.finishOp];
    
    NSString* salt = nil;

    NSError *authKitError = nil;
    NSString *altDSID = [self.deps.authKitAdapter primaryiCloudAccountAltDSID:&authKitError];

    if (altDSID) {
        salt = altDSID;
    } else {
        secnotice("octagon", "authkit doesn't know about the altdsid, using stored value: %@", authKitError);

        NSError* accountError = nil;
        OTAccountMetadataClassC* account = [self.deps.stateHolder loadOrCreateAccountMetadata:&accountError];

        if(account && !accountError) {
            secnotice("octagon", "retrieved account, altdsid is: %@", account.altDSID);
            salt = account.altDSID;
        } else {
            if (accountError == nil) {
                accountError = [NSError errorWithDomain:(__bridge NSString *)kSecErrorDomain code:errSecInternalError userInfo:nil];
            }
            secerror("failed to retrieve account object: %@", accountError);
            self.error = accountError;
            [self runBeforeGroupFinished:self.finishOp];
            return;
        }
    }

    WEAKIFY(self);

    OTFetchCKKSKeysOperation* fetchKeysOp = [[OTFetchCKKSKeysOperation alloc] initWithDependencies:self.deps
                                                                                     refetchNeeded:NO];
    [self runBeforeGroupFinished:fetchKeysOp];

    CKKSResultOperation* proceedWithKeys = [CKKSResultOperation named:@"setting-recovery-tlks"
                                                            withBlock:^{
                                                                STRONGIFY(self);
                                                                [self proceedWithKeys:fetchKeysOp.viewKeySets salt:salt];
                                                            }];

    [proceedWithKeys addDependency:fetchKeysOp];
    [self runBeforeGroupFinished:proceedWithKeys];
}

- (void)proceedWithKeys:(NSArray<CKKSKeychainBackedKeySet*>*)viewKeySets salt:(NSString*)salt
{
    WEAKIFY(self);
    
    CFErrorRef cferr = NULL;
    NSString *recoveryString = CFBridgingRelease(SecPasswordGenerate(kSecPasswordTypeiCloudRecovery, &cferr, NULL));
    if (recoveryString == NULL) {
        secerror("octagon: failed to create string: %@", (__bridge id)cferr);
        self.error = CFBridgingRelease(cferr);
        [self runBeforeGroupFinished:self.finishOp];
        return;
    }

    [self.deps.cuttlefishXPCWrapper createCustodianRecoveryKeyWithContainer:self.deps.containerName
                                                                    context:self.deps.contextID
                                                                recoveryKey:recoveryString
                                                                       salt:salt
                                                                   ckksKeys:viewKeySets
                                                                       uuid:self.uuid
                                                                       kind:TPPBCustodianRecoveryKey_Kind_RECOVERY_KEY
                                                                      reply:^(NSArray<CKRecord*>* _Nullable keyHierarchyRecords,
                                                                              TrustedPeersHelperCustodianRecoveryKey *_Nullable crk,
                                                                              NSError * _Nullable error) {
            STRONGIFY(self);
            [[CKKSAnalytics logger] logResultForEvent:OctagonEventCustodianRecoveryKey hardFailure:true result:error];
            if(error){
                secerror("octagon: Error create custodian recovery key: %@", error);
                self.error = error;
                [self runBeforeGroupFinished:self.finishOp];
            } else {
                secnotice("octagon", "successfully created custodian recovery key");

                NSError *error = nil;
                NSUUID *uuid = [[NSUUID alloc] initWithUUIDString:crk.uuid];
                if (uuid == nil) {
                    secerror("octagon: failed to parse UUID from TPH: %@", crk.uuid);
                    self.error = [NSError errorWithDomain:OctagonErrorDomain
                                                     code:OctagonErrorBadUUID
                                              description:@"Failed to parse UUID from TPH"];
                    [self runBeforeGroupFinished:self.finishOp];
                    return;
                }
                self.crk = [[OTCustodianRecoveryKey alloc] initWithUUID:uuid
                                                         recoveryString:recoveryString
                                                                  error:&error];
                if (self.crk == nil) {
                    secerror("octagon: failed to create crk: %@", error);
                    self.error = error;
                    [self runBeforeGroupFinished:self.finishOp];
                    return;
                }

                secnotice("octagon-ckks", "Providing createCustodianRecoveryKey() records to %@", self.deps.ckks);
                [self.deps.ckks receiveTLKUploadRecords:keyHierarchyRecords];

                [self runBeforeGroupFinished:self.finishOp];
            }
        }];
}

@end

#endif // OCTAGON
