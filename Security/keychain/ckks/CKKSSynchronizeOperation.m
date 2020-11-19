/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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

#import "CKKSKeychainView.h"
#import "CKKSGroupOperation.h"
#import "CKKSSynchronizeOperation.h"
#import "CKKSFetchAllRecordZoneChangesOperation.h"
#import "CKKSScanLocalItemsOperation.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/ot/ObjCImprovements.h"

#if OCTAGON

@interface CKKSSynchronizeOperation ()
@property int32_t restartCount;
@end

@implementation CKKSSynchronizeOperation

- (instancetype)init {
    return nil;
}
- (instancetype)initWithCKKSKeychainView:(CKKSKeychainView*)ckks {
    if(self = [super init]) {
        _ckks = ckks;
        _restartCount = 0;
    }
    return self;
}

- (void)groupStart {
    WEAKIFY(self);

    /*
     * Synchronizations (or resynchronizations) are complicated beasts. We will:
     *
     *  1. Finish processing the outgoing queue. You can't be in-sync with cloudkit if you have an update that hasn't propagated.
     *  2. Kick off a normal CloudKit fetch.
     *  3. Process the incoming queue as normal.
     *          (Note that this requires the keybag to be unlocked.)
     *
     * So far, this is normal operation. Now:
     *
     *  4. Start another CloudKit fetch, giving it the nil change tag. This fetches all objects in CloudKit.
     *    4a. Compare those objects against our local mirror. If any discrepancies, this is a bug.
     *    4b. All conflicts: CloudKit data wins.
     *    4c. All items we have that CloudKit doesn't: delete locally.
     *
     *  5. Process the incoming queue again. This should be empty.
     *  6. Scan the local keychain for items which exist locally but are not in CloudKit. Upload them.
     *  7. If there are any such items in 6, restart the sync.
     */

    // Promote to strong reference
    CKKSKeychainView* ckks = self.ckks;

    // Synchronous, on some thread. Get back on the CKKS queue for SQL thread-safety.
    [ckks dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
        if(self.cancelled) {
            ckksnotice("ckksresync", ckks, "CKKSSynchronizeOperation cancelled, quitting");
            return CKKSDatabaseTransactionRollback;
        }

        ckks.lastSynchronizeOperation = self;

        uint32_t steps = 5;

        ckksnotice("ckksresync", ckks, "Beginning resynchronize (attempt %u)", self.restartCount);

        CKOperationGroup* operationGroup = [CKOperationGroup CKKSGroupWithName:@"ckks-resync"];

        // Step 1
        CKKSFetchAllRecordZoneChangesOperation* fetchOp = [[CKKSFetchAllRecordZoneChangesOperation alloc] initWithContainer:ckks.container
                                                                                                                 fetchClass:ckks.cloudKitClassDependencies.fetchRecordZoneChangesOperationClass
                                                                                                                     clients:@[ckks]
                                                                                                               fetchReasons:[NSSet setWithObject:CKKSFetchBecauseResync]
                                                                                                                 apnsPushes:nil
                                                                                                                forceResync:true
                                                                                                           ckoperationGroup:operationGroup];
        fetchOp.name = [NSString stringWithFormat: @"resync-step%u-fetch", self.restartCount * steps + 1];
        [self runBeforeGroupFinished: fetchOp];

        // Step 2
        CKKSIncomingQueueOperation* incomingOp = [[CKKSIncomingQueueOperation alloc] initWithDependencies:ckks.operationDependencies
                                                                                                     ckks:ckks
                                                                                                intending:SecCKKSZoneKeyStateReady
                                                                                               errorState:SecCKKSZoneKeyStateUnhealthy
                                                                                     errorOnClassAFailure:true
                                                                                handleMismatchedViewItems:false];

        incomingOp.name = [NSString stringWithFormat: @"resync-step%u-incoming", self.restartCount * steps + 2];
        [incomingOp addSuccessDependency:fetchOp];
        [self runBeforeGroupFinished:incomingOp];

        // Step 3
        CKKSScanLocalItemsOperation* scan = [[CKKSScanLocalItemsOperation alloc] initWithDependencies:ckks.operationDependencies
                                                                                                 ckks:ckks
                                                                                            intending:SecCKKSZoneKeyStateReady
                                                                                           errorState:SecCKKSZoneKeyStateError
                                                                                     ckoperationGroup:operationGroup];
        scan.name = [NSString stringWithFormat: @"resync-step%u-scan", self.restartCount * steps + 3];
        [scan addSuccessDependency: incomingOp];
        [self runBeforeGroupFinished: scan];

        // Step 4
        CKKSOutgoingQueueOperation* outgoingOp = [ckks processOutgoingQueue: operationGroup];
        outgoingOp.name = [NSString stringWithFormat: @"resync-step%u-outgoing", self.restartCount * steps + 4];
        [self dependOnBeforeGroupFinished:outgoingOp];
        [outgoingOp addDependency:scan];

        // Step 5:
        CKKSResultOperation* restart = [[CKKSResultOperation alloc] init];
        restart.name = [NSString stringWithFormat: @"resync-step%u-consider-restart", self.restartCount * steps + 5];
        [restart addExecutionBlock:^{
            STRONGIFY(self);
            if(!self) {
                ckkserror("ckksresync", ckks, "received callback for released object");
                return;
            }

            if(scan.recordsFound > 0) {
                if(self.restartCount >= 3) {
                    // we've restarted too many times. Fail and stop.
                    ckkserror("ckksresync", ckks, "restarted synchronization too often; Failing");
                    self.error = [NSError errorWithDomain:@"securityd"
                                                         code:2
                                                     userInfo:@{NSLocalizedDescriptionKey: @"resynchronization restarted too many times; churn in database?"}];
                } else {
                    // restart the sync operation.
                    self.restartCount += 1;
                    ckkserror("ckksresync", ckks, "restarting synchronization operation due to new local items");
                    [self groupStart];
                }
            }
        }];

        [restart addSuccessDependency: outgoingOp];
        [self runBeforeGroupFinished: restart];

        return CKKSDatabaseTransactionCommit;
    }];
}

@end;

#endif
