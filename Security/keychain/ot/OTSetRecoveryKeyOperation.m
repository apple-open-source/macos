/*
 * Copyright (c) 2019 Apple Inc. All Rights Reserved.
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

#import "keychain/ot/OTSetRecoveryKeyOperation.h"
#import "keychain/ot/OTCuttlefishContext.h"
#import "keychain/ot/OTFetchCKKSKeysOperation.h"

#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/ObjCImprovements.h"

@interface OTSetRecoveryKeyOperation ()
@property OTOperationDependencies* deps;

@property NSOperation* finishOp;
@end

@implementation OTSetRecoveryKeyOperation

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                         recoveryKey:(NSString*)recoveryKey
{
    if((self = [super init])) {
        _deps = dependencies;
        _recoveryKey = recoveryKey;
    }
    return self;
}

- (void)groupStart
{
    self.finishOp = [[NSOperation alloc] init];
    [self dependOnBeforeGroupFinished:self.finishOp];

    NSString* altDSID = self.deps.activeAccount.altDSID;
    if(altDSID == nil) {
        secnotice("authkit", "No configured altDSID: %@", self.deps.activeAccount);
        self.error = [NSError errorWithDomain:OctagonErrorDomain
                                         code:OctagonErrorNoAppleAccount
                                  description:@"No altDSID configured"];
        [self runBeforeGroupFinished:self.finishOp];
        return;
    }

    NSString* salt = altDSID;

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

    [self.deps.cuttlefishXPCWrapper setRecoveryKeyWithSpecificUser:self.deps.activeAccount
                                                       recoveryKey:self.recoveryKey
                                                              salt:salt
                                                          ckksKeys:viewKeySets
                                                             reply:^(NSArray<CKRecord*>* _Nullable keyHierarchyRecords,
                                                                     NSError * _Nullable setError) {
        STRONGIFY(self);
        [[CKKSAnalytics logger] logResultForEvent:OctagonEventSetRecoveryKey hardFailure:true result:setError];
        if(setError){
            secerror("octagon: Error setting recovery key: %@", setError);
            self.error = setError;
            [self runBeforeGroupFinished:self.finishOp];
        } else {
            secnotice("octagon", "successfully set recovery key");

            secnotice("octagon-ckks", "Providing setRecoveryKey() records to %@", self.deps.ckks);
            [self.deps.ckks receiveTLKUploadRecords:keyHierarchyRecords];

            [self runBeforeGroupFinished:self.finishOp];
        }
    }];
}

@end

#endif // OCTAGON
