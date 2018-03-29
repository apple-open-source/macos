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

@interface CKKSUpdateDeviceStateOperation ()
@property CKModifyRecordsOperation* modifyRecordsOperation;
@property CKOperationGroup* group;
@property bool rateLimit;
@end

@implementation CKKSUpdateDeviceStateOperation

- (instancetype)initWithCKKSKeychainView:(CKKSKeychainView*)ckks rateLimit:(bool)rateLimit ckoperationGroup:(CKOperationGroup*)group {
    if((self = [super init])) {
        _ckks = ckks;
        _group = group;
        _rateLimit = rateLimit;
    }
    return self;
}

- (void)groupStart {
    CKKSKeychainView* ckks = self.ckks;
    if(!ckks) {
        ckkserror("ckksdevice", ckks, "no CKKS object");
        self.error = [NSError errorWithDomain:@"securityd" code:errSecInternalError userInfo:@{NSLocalizedDescriptionKey: @"no CKKS object"}];
        return;
    }

    CKKSCKAccountStateTracker* accountTracker = ckks.accountTracker;
    if(!accountTracker) {
        ckkserror("ckksdevice", ckks, "no AccountTracker object");
        self.error = [NSError errorWithDomain:@"securityd" code:errSecInternalError userInfo:@{NSLocalizedDescriptionKey: @"no AccountTracker object"}];
        return;
    }

    __weak __typeof(self) weakSelf = self;

    // We must have the ck device ID to run this operation.
    if([accountTracker.ckdeviceIDInitialized wait:200*NSEC_PER_SEC]) {
        ckkserror("ckksdevice", ckks, "CK device ID not initialized, quitting");
        self.error = [NSError errorWithDomain:@"securityd" code:errSecInternalError userInfo:@{NSLocalizedDescriptionKey: @"CK device ID not initialized"}];
        return;
    }

    if(!accountTracker.ckdeviceID) {
        ckkserror("ckksdevice", ckks, "CK device ID not initialized, quitting");
        self.error = [NSError errorWithDomain:@"securityd"
                                         code:errSecInternalError
                                     userInfo:@{NSLocalizedDescriptionKey: @"CK device ID null", NSUnderlyingErrorKey:CKKSNilToNSNull(accountTracker.ckdeviceIDError)}];
        return;
    }

    [ckks dispatchSyncWithAccountKeys:^bool {
        NSError* error = nil;

        CKKSDeviceStateEntry* cdse = [ckks _onqueueCurrentDeviceStateEntry:&error];
        if(error) {
            ckkserror("ckksdevice", ckks, "Error creating device state entry; quitting: %@", error);
            return false;
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
                ckksnotice("ckksdevice", ckks, "Not rate-limiting: last updated %@ vs %@", lastUpdate, deadline);
            } else {
                ckksnotice("ckksdevice", ckks, "Last update is within 3 days (%@); rate-limiting this operation", lastUpdate);
                self.error =  [NSError errorWithDomain:@"securityd"
                                                  code:errSecInternalError
                                              userInfo:@{NSLocalizedDescriptionKey: @"Rate-limited the CKKSUpdateDeviceStateOperation"}];
                return false;
            }
        }

        ckksnotice("ckksdevice", ckks, "Saving new device state %@", cdse);

        NSArray* recordsToSave = @[[cdse CKRecordWithZoneID:ckks.zoneID]];

        // Start a CKModifyRecordsOperation to save this new/updated record.
        NSBlockOperation* modifyComplete = [[NSBlockOperation alloc] init];
        modifyComplete.name = @"updateDeviceState-modifyRecordsComplete";
        [self dependOnBeforeGroupFinished: modifyComplete];

        self.modifyRecordsOperation = [[CKModifyRecordsOperation alloc] initWithRecordsToSave:recordsToSave recordIDsToDelete:nil];
        self.modifyRecordsOperation.atomic = TRUE;
        self.modifyRecordsOperation.qualityOfService = NSQualityOfServiceUtility;
        self.modifyRecordsOperation.savePolicy = CKRecordSaveAllKeys; // Overwrite anything in CloudKit: this is our state now
        self.modifyRecordsOperation.group = self.group;

        self.modifyRecordsOperation.perRecordCompletionBlock = ^(CKRecord *record, NSError * _Nullable error) {
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            __strong __typeof(strongSelf.ckks) blockCKKS = strongSelf.ckks;

            if(!error) {
                ckksnotice("ckksdevice", blockCKKS, "Device state record upload successful for %@: %@", record.recordID.recordName, record);
            } else {
                ckkserror("ckksdevice", blockCKKS, "error on row: %@ %@", error, record);
            }
        };

        self.modifyRecordsOperation.modifyRecordsCompletionBlock = ^(NSArray<CKRecord *> *savedRecords, NSArray<CKRecordID *> *deletedRecordIDs, NSError *ckerror) {
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            __strong __typeof(strongSelf.ckks) strongCKKS = strongSelf.ckks;
            if(!strongSelf || !strongCKKS) {
                ckkserror("ckksdevice", strongCKKS, "received callback for released object");
                strongSelf.error = [NSError errorWithDomain:@"securityd" code:errSecInternalError userInfo:@{NSLocalizedDescriptionKey: @"no CKKS object"}];
                [strongSelf runBeforeGroupFinished:modifyComplete];
                return;
            }

            if(ckerror) {
                ckkserror("ckksdevice", strongCKKS, "CloudKit returned an error: %@", ckerror);
                strongSelf.error = ckerror;
                [strongSelf runBeforeGroupFinished:modifyComplete];
                return;
            }

            __block NSError* error = nil;

            [strongCKKS dispatchSync: ^bool{
                for(CKRecord* record in savedRecords) {
                    // Save the item records
                    if([record.recordType isEqualToString: SecCKRecordDeviceStateType]) {
                        CKKSDeviceStateEntry* newcdse = [[CKKSDeviceStateEntry alloc] initWithCKRecord:record];
                        [newcdse saveToDatabase:&error];
                        if(error) {
                            ckkserror("ckksdevice", strongCKKS, "Couldn't save new device state(%@) to database: %@", newcdse, error);
                        }
                    }
                }
                return true;
            }];

            strongSelf.error = error;
            [strongSelf runBeforeGroupFinished:modifyComplete];
        };

        [self dependOnBeforeGroupFinished: self.modifyRecordsOperation];
        [ckks.database addOperation: self.modifyRecordsOperation];

        return true;
    }];
}

@end

#endif // OCTAGON
