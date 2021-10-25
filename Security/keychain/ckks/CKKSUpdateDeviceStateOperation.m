/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#include <utilities/SecInternalReleasePriv.h>
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSUpdateDeviceStateOperation.h"
#import "keychain/ckks/CKKSCurrentKeyPointer.h"
#import "keychain/ckks/CKKSKey.h"
#import "keychain/ckks/CKKSLockStateTracker.h"
#import "keychain/ckks/CKKSSQLDatabaseObject.h"
#import "keychain/ot/ObjCImprovements.h"

@interface CKKSUpdateDeviceStateOperation ()
@property CKOperationGroup* group;
@property bool rateLimit;
@end

@implementation CKKSUpdateDeviceStateOperation

- (instancetype)initWithOperationDependencies:(CKKSOperationDependencies*)operationDependencies
                                    rateLimit:(bool)rateLimit
                             ckoperationGroup:(CKOperationGroup*)group
{
    if((self = [super init])) {
        _deps = operationDependencies;
        _group = group;
        _rateLimit = rateLimit;
    }
    return self;
}

- (void)groupStart {
    CKKSAccountStateTracker* accountTracker = self.deps.accountStateTracker;
    if(!accountTracker) {
        ckkserror_global("ckksdevice", "no AccountTracker object");
        self.error = [NSError errorWithDomain:CKKSErrorDomain code:CKKSErrorUnexpectedNil userInfo:@{NSLocalizedDescriptionKey: @"no AccountTracker object"}];
        return;
    }

    WEAKIFY(self);

    // We must have the ck device ID to run this operation.
    if([accountTracker.ckdeviceIDInitialized wait:200*NSEC_PER_SEC]) {
        ckkserror_global("ckksdevice", "CK device ID not initialized, likely quitting");
    }

    NSString* ckdeviceID = accountTracker.ckdeviceID;
    if(!ckdeviceID) {
        ckkserror_global("ckksdevice", "CK device ID not initialized, quitting");
        self.error = [NSError errorWithDomain:CKKSErrorDomain
                                         code:CKKSNoCloudKitDeviceID
                                     userInfo:@{NSLocalizedDescriptionKey: @"CK device ID missing", NSUnderlyingErrorKey:CKKSNilToNSNull(accountTracker.ckdeviceIDError)}];
        return;
    }

    // We'd also really like to know the HSA2-ness of the world
    if([accountTracker.hsa2iCloudAccountInitialized wait:500*NSEC_PER_MSEC]) {
        ckkserror_global("ckksdevice", "Not quite sure if the account is HSA2 or not. Probably will quit?");
    }

    NSHashTable<CKDatabaseOperation*>* ckOperations = [NSHashTable weakObjectsHashTable];

    id<CKKSDatabaseProviderProtocol> databaseProvider = self.deps.databaseProvider;

    for(CKKSKeychainViewState* viewState in self.deps.activeManagedViews) {
        [databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
            NSError* error = nil;

            CKKSDeviceStateEntry* cdse = [CKKSDeviceStateEntry intransactionCreateDeviceStateForView:viewState
                                                                                      accountTracker:self.deps.accountStateTracker
                                                                                    lockStateTracker:self.deps.lockStateTracker
                                                                                               error:&error];
            if(error || !cdse) {
                ckkserror("ckksdevice", viewState.zoneID, "Error creating device state entry; quitting: %@", error);
                self.error = error;
                return CKKSDatabaseTransactionRollback;
            }

            if(self.rateLimit) {
                NSDate* lastUpdate = cdse.storedCKRecord.modificationDate;

                // Only upload this every 3 days (1 day for internal installs)
                NSDate* now = [NSDate date];
                NSDateComponents* offset = [[NSDateComponents alloc] init];
                if(SecIsInternalRelease()) {
                    [offset setHour:-23];
                } else {
                    [offset setHour:-3*24];
                }
                NSDate* deadline = [[NSCalendar currentCalendar] dateByAddingComponents:offset toDate:now options:0];

                if(lastUpdate == nil || [lastUpdate compare: deadline] == NSOrderedAscending) {
                    ckksnotice("ckksdevice", viewState.zoneID, "Not rate-limiting: last updated %@ vs %@", lastUpdate, deadline);
                } else {
                    ckksnotice("ckksdevice", viewState.zoneID, "Last update is within 3 days (%@); rate-limiting this operation", lastUpdate);
                    self.error =  [NSError errorWithDomain:CKKSErrorDomain
                                                      code:CKKSErrorRateLimited
                                                  userInfo:@{NSLocalizedDescriptionKey: @"Rate-limited the CKKSUpdateDeviceStateOperation"}];
                    return CKKSDatabaseTransactionRollback;
                }
            }

            ckksnotice("ckksdevice", viewState.zoneID, "Saving new device state %@", cdse);

            NSArray* recordsToSave = @[[cdse CKRecordWithZoneID:viewState.zoneID]];

            // Start a CKModifyRecordsOperation to save this new/updated record.
            NSBlockOperation* modifyComplete = [[NSBlockOperation alloc] init];
            modifyComplete.name = @"updateDeviceState-modifyRecordsComplete";
            [self dependOnBeforeGroupFinished: modifyComplete];

            CKModifyRecordsOperation* zoneModifyRecordsOperation = [[CKModifyRecordsOperation alloc] initWithRecordsToSave:recordsToSave recordIDsToDelete:nil];
            zoneModifyRecordsOperation.atomic = TRUE;
            zoneModifyRecordsOperation.qualityOfService = NSQualityOfServiceUtility;
            zoneModifyRecordsOperation.savePolicy = CKRecordSaveAllKeys; // Overwrite anything in CloudKit: this is our state now
            zoneModifyRecordsOperation.group = self.group;

            zoneModifyRecordsOperation.perRecordCompletionBlock = ^(CKRecord *record, NSError * _Nullable error) {
                if(!error) {
                    ckksnotice("ckksdevice", viewState.zoneID, "Device state record upload successful for %@: %@", record.recordID.recordName, record);
                } else {
                    ckkserror("ckksdevice", viewState.zoneID, "error on row: %@ %@", error, record);
                }
            };

            zoneModifyRecordsOperation.modifyRecordsCompletionBlock = ^(NSArray<CKRecord *> *savedRecords, NSArray<CKRecordID *> *deletedRecordIDs, NSError *ckerror) {
                STRONGIFY(self);

                if(ckerror) {
                    ckkserror("ckksdevice", viewState.zoneID, "CloudKit returned an error: %@", ckerror);
                    self.error = ckerror;
                    [self runBeforeGroupFinished:modifyComplete];
                    return;
                }

                __block NSError* error = nil;

                [self.deps.databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
                    for(CKRecord* record in savedRecords) {
                        // Save the item records
                        if([record.recordType isEqualToString: SecCKRecordDeviceStateType]) {
                            CKKSDeviceStateEntry* newcdse = [[CKKSDeviceStateEntry alloc] initWithCKRecord:record];
                            [newcdse saveToDatabase:&error];
                            if(error) {
                                ckkserror("ckksdevice", viewState.zoneID, "Couldn't save new device state(%@) to database: %@", newcdse, error);
                            }
                        }
                    }
                    return CKKSDatabaseTransactionCommit;
                }];

                self.error = error;
                [self runBeforeGroupFinished:modifyComplete];
            };

            [zoneModifyRecordsOperation linearDependencies:ckOperations];
            [self dependOnBeforeGroupFinished:zoneModifyRecordsOperation];
            [self.deps.ckdatabase addOperation:zoneModifyRecordsOperation];

            return CKKSDatabaseTransactionCommit;
        }];
    }
}

@end

#endif // OCTAGON
