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

#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSGroupOperation.h"
#import "keychain/ckks/CKKSLocalSynchronizeOperation.h"
#import "keychain/ckks/CKKSFetchAllRecordZoneChangesOperation.h"
#import "keychain/ckks/CKKSScanLocalItemsOperation.h"
#import "keychain/ckks/CKKSMirrorEntry.h"
#import "keychain/ckks/CKKSIncomingQueueEntry.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/categories/NSError+UsefulConstructors.h"

#if OCTAGON

@interface CKKSLocalSynchronizeOperation ()
@property int32_t restartCount;
@end

@implementation CKKSLocalSynchronizeOperation

- (instancetype)init {
    return nil;
}
- (instancetype)initWithCKKSKeychainView:(CKKSKeychainView*)ckks
{
    if(self = [super init]) {
        _ckks = ckks;
        _restartCount = 0;

        [self addNullableDependency:ckks.holdLocalSynchronizeOperation];
    }
    return self;
}

- (void)groupStart {
    __weak __typeof(self) weakSelf = self;

    /*
     * A local synchronize is very similar to a CloudKit synchronize, but it won't cause any (non-essential)
     * CloudKit operations to occur.
     *
     *  1. Finish processing the outgoing queue. You can't be in-sync with cloudkit if you have an update that hasn't propagated.
     *  2. Process anything in the incoming queue as normal.
     *          (Note that this might require the keybag to be unlocked.)
     *
     *  3. Take every item in the CKMirror, and check for its existence in the local keychain. If not present, add to the incoming queue.
     *  4. Process the incoming queue again.
     *  5. Scan the local keychain for items which exist locally but are not in CloudKit. Upload them.
     *  6. If there are any such items in 4, restart the sync.
     */

    CKKSKeychainView* ckks = self.ckks;

    // Synchronous, on some thread. Get back on the CKKS queue for SQL thread-safety.
    [ckks dispatchSync: ^bool{
        if(self.cancelled) {
            ckksnotice("ckksresync", ckks, "CKKSSynchronizeOperation cancelled, quitting");
            return false;
        }

        //ckks.lastLocalSynchronizeOperation = self;

        uint32_t steps = 5;

        ckksinfo("ckksresync", ckks, "Beginning local resynchronize (attempt %u)", self.restartCount);

        CKOperationGroup* operationGroup = [CKOperationGroup CKKSGroupWithName:@"ckks-resync-local"];

        // Step 1
        CKKSOutgoingQueueOperation* outgoingOp = [ckks processOutgoingQueue: operationGroup];
        outgoingOp.name = [NSString stringWithFormat: @"resync-step%u-outgoing", self.restartCount * steps + 1];
        [self dependOnBeforeGroupFinished:outgoingOp];

        // Step 2
        CKKSIncomingQueueOperation* incomingOp = [[CKKSIncomingQueueOperation alloc] initWithCKKSKeychainView:ckks errorOnClassAFailure:true];
        incomingOp.name = [NSString stringWithFormat: @"resync-step%u-incoming", self.restartCount * steps + 2];
        [incomingOp addSuccessDependency:outgoingOp];
        [self runBeforeGroupFinished:incomingOp];

        // Step 3:
        CKKSResultOperation* reloadOp = [[CKKSReloadAllItemsOperation alloc] initWithCKKSKeychainView:ckks];
        reloadOp.name = [NSString stringWithFormat: @"resync-step%u-reload", self.restartCount * steps + 3];
        [self runBeforeGroupFinished:reloadOp];

        // Step 4
        CKKSIncomingQueueOperation* incomingResyncOp = [[CKKSIncomingQueueOperation alloc] initWithCKKSKeychainView:ckks errorOnClassAFailure:true];
        incomingResyncOp.name = [NSString stringWithFormat: @"resync-step%u-incoming-again", self.restartCount * steps + 4];
        [incomingResyncOp addSuccessDependency: reloadOp];
        [self runBeforeGroupFinished:incomingResyncOp];

        // Step 5
        CKKSScanLocalItemsOperation* scan = [[CKKSScanLocalItemsOperation alloc] initWithCKKSKeychainView:ckks ckoperationGroup:operationGroup];
        scan.name = [NSString stringWithFormat: @"resync-step%u-scan", self.restartCount * steps + 5];
        [scan addSuccessDependency: incomingResyncOp];
        [self runBeforeGroupFinished: scan];

        // Step 6
        CKKSResultOperation* restart = [[CKKSResultOperation alloc] init];
        restart.name = [NSString stringWithFormat: @"resync-step%u-consider-restart", self.restartCount * steps + 6];
        [restart addExecutionBlock:^{
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            if(!strongSelf) {
                secerror("ckksresync: received callback for released object");
                return;
            }

            NSError* error = nil;
            NSArray<NSString*>* iqes = [CKKSIncomingQueueEntry allUUIDs:ckks.zoneID error:&error];
            if(error) {
                ckkserror("ckksresync", ckks, "Couldn't fetch IQEs: %@", error);
            }

            if(scan.recordsFound > 0 || iqes.count > 0) {
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

#pragma mark - CKKSReloadAllItemsOperation

@implementation CKKSReloadAllItemsOperation

- (instancetype)init {
    return nil;
}
- (instancetype)initWithCKKSKeychainView:(CKKSKeychainView*)ckks
{
    if(self = [super init]) {
        _ckks = ckks;
    }
    return self;
}

- (void)main {
    CKKSKeychainView* strongCKKS = self.ckks;

    [strongCKKS dispatchSync: ^bool{
       NSError* error = nil;
       NSArray<CKKSMirrorEntry*>* mirrorItems = [CKKSMirrorEntry all:strongCKKS.zoneID error:&error];

       if(error) {
           ckkserror("ckksresync", strongCKKS, "Couldn't fetch mirror items: %@", error);
           self.error = error;
           return false;
       }

       // Reload all entries back into the local keychain
       // We _could_ scan for entries, but that'd be expensive
       // In 36044942, we used to store only the CKRecord system fields in the ckrecord. To work around this, make a whole new CKRecord from the item.
       for(CKKSMirrorEntry* ckme in mirrorItems) {
           CKRecord* ckmeRecord = [ckme.item CKRecordWithZoneID:strongCKKS.zoneID];
           if(!ckmeRecord) {
               ckkserror("ckksresync", strongCKKS, "Couldn't make CKRecord for item: %@", ckme);
               continue;
           }

           [strongCKKS _onqueueCKRecordChanged:ckmeRecord resync:true];
       }

       return true;
    }];
}
@end
#endif

