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
    __weak __typeof(self) weakSelf = self;

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
    [ckks dispatchSync: ^bool{
        if(self.cancelled) {
            ckksnotice("ckksresync", ckks, "CKKSSynchronizeOperation cancelled, quitting");
            return false;
        }

        ckks.lastSynchronizeOperation = self;

        uint32_t steps = 7;

        ckksinfo("ckksresync", ckks, "Beginning resynchronize (attempt %u)", self.restartCount);

        CKOperationGroup* operationGroup = [CKOperationGroup CKKSGroupWithName:@"ckks-resync"];

        // Step 1
        CKKSOutgoingQueueOperation* outgoingOp = [ckks processOutgoingQueue: operationGroup];
        outgoingOp.name = [NSString stringWithFormat: @"resync-step%u-outgoing", self.restartCount * steps + 1];
        [self dependOnBeforeGroupFinished:outgoingOp];

        // Step 2
        CKKSFetchAllRecordZoneChangesOperation* fetchOp = [[CKKSFetchAllRecordZoneChangesOperation alloc] initWithContainer:ckks.container
                                                                                                                 fetchClass:ckks.fetchRecordZoneChangesOperationClass
                                                                                                                     clients:@[ckks]
                                                                                                               fetchReasons:[NSSet setWithObject:CKKSFetchBecauseResync]
                                                                                                                 apnsPushes:nil
                                                                                                                forceResync:true
                                                                                                           ckoperationGroup:operationGroup];
        fetchOp.name = [NSString stringWithFormat: @"resync-step%u-fetch", self.restartCount * steps + 2];
        [fetchOp addSuccessDependency: outgoingOp];
        [self runBeforeGroupFinished: fetchOp];

        // Step 3
        CKKSIncomingQueueOperation* incomingOp = [[CKKSIncomingQueueOperation alloc] initWithCKKSKeychainView:ckks errorOnClassAFailure:true];
        incomingOp.name = [NSString stringWithFormat: @"resync-step%u-incoming", self.restartCount * steps + 3];
        [incomingOp addSuccessDependency:fetchOp];
        [self runBeforeGroupFinished:incomingOp];

        // Now, get serious:

        // Step 4
        CKKSFetchAllRecordZoneChangesOperation* fetchAllOp = [[CKKSFetchAllRecordZoneChangesOperation alloc] initWithContainer:ckks.container
                                                                                                                    fetchClass:ckks.fetchRecordZoneChangesOperationClass
                                                                                                                        clients:@[ckks]
                                                                                                                   fetchReasons:[NSSet setWithObject:CKKSFetchBecauseResync]
                                                                                                                    apnsPushes:nil
                                                                                                                   forceResync:true
                                                                                                               ckoperationGroup:operationGroup];
        fetchAllOp.resync = true;
        fetchAllOp.name = [NSString stringWithFormat: @"resync-step%u-fetchAll", self.restartCount * steps + 4];
        [fetchAllOp addSuccessDependency: incomingOp];
        [self runBeforeGroupFinished: fetchAllOp];

        // Step 5
        CKKSIncomingQueueOperation* incomingResyncOp = [[CKKSIncomingQueueOperation alloc] initWithCKKSKeychainView:ckks errorOnClassAFailure:true];
        incomingResyncOp.name = [NSString stringWithFormat: @"resync-step%u-incoming", self.restartCount * steps + 5];
        [incomingResyncOp addSuccessDependency: fetchAllOp];
        [self runBeforeGroupFinished:incomingResyncOp];

        // Step 6
        CKKSScanLocalItemsOperation* scan = [[CKKSScanLocalItemsOperation alloc] initWithCKKSKeychainView:ckks ckoperationGroup:operationGroup];
        scan.name = [NSString stringWithFormat: @"resync-step%u-scan", self.restartCount * steps + 6];
        [scan addSuccessDependency: incomingResyncOp];
        [self runBeforeGroupFinished: scan];

        // Step 7:
        CKKSResultOperation* restart = [[CKKSResultOperation alloc] init];
        restart.name = [NSString stringWithFormat: @"resync-step%u-consider-restart", self.restartCount * steps + 7];
        [restart addExecutionBlock:^{
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            if(!strongSelf) {
                secerror("ckksresync: received callback for released object");
                return;
            }

            if(scan.recordsFound > 0) {
                if(strongSelf.restartCount >= 3) {
                    // we've restarted too many times. Fail and stop.
                    ckkserror("ckksresync", ckks, "restarted synchronization too often; Failing");
                    strongSelf.error = [NSError errorWithDomain:@"securityd"
                                                         code:2
                                                     userInfo:@{NSLocalizedDescriptionKey: @"resynchronization restarted too many times; churn in database?"}];
                } else {
                    // restart the sync operation.
                    strongSelf.restartCount += 1;
                    ckkserror("ckksresync", ckks, "restarting synchronization operation due to new local items");
                    [strongSelf groupStart];
                }
            }
        }];

        [restart addSuccessDependency: scan];
        [self runBeforeGroupFinished: restart];

        return true;
    }];
}

@end;

#endif
