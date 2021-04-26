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

    CKKSAccountStateTracker* accountTracker = ckks.accountTracker;
    if(!accountTracker) {
        ckkserror("ckksdevice", ckks, "no AccountTracker object");
        self.error = [NSError errorWithDomain:@"securityd" code:errSecInternalError userInfo:@{NSLocalizedDescriptionKey: @"no AccountTracker object"}];
        return;
    }

    WEAKIFY(self);

    // We must have the ck device ID to run this operation.
    if([accountTracker.ckdeviceIDInitialized wait:200*NSEC_PER_SEC]) {
        ckkserror("ckksdevice", ckks, "CK device ID not initialized, quitting");
        self.error = [NSError errorWithDomain:@"securityd" code:errSecInternalError userInfo:@{NSLocalizedDescriptionKey: @"CK device ID not initialized"}];
        return;
    }

    NSString* ckdeviceID = accountTracker.ckdeviceID;
    if(!ckdeviceID) {
        ckkserror("ckksdevice", ckks, "CK device ID not initialized, quitting");
        self.error = [NSError errorWithDomain:@"securityd"
                                         code:errSecInternalError
                                     userInfo:@{NSLocalizedDescriptionKey: @"CK device ID null", NSUnderlyingErrorKey:CKKSNilToNSNull(accountTracker.ckdeviceIDError)}];
        return;
    }

    // We'd also really like to know the HSA2-ness of the world
    if([accountTracker.hsa2iCloudAccountInitialized wait:500*NSEC_PER_MSEC]) {
        ckkserror("ckksdevice", ckks, "Not quite sure if the account isa HSA2 or not. Probably will quit?");
    }

    [ckks dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
        NSError* error = nil;

        CKKSDeviceStateEntry* cdse = [ckks _onqueueCurrentDeviceStateEntry:&error];
        if(error || !cdse) {
            ckkserror("ckksdevice", ckks, "Error creating device state entry; quitting: %@", error);
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
                ckksnotice("ckksdevice", ckks, "Not rate-limiting: last updated %@ vs %@", lastUpdate, deadline);
            } else {
                ckksnotice("ckksdevice", ckks, "Last update is within 3 days (%@); rate-limiting this operation", lastUpdate);
                self.error =  [NSError errorWithDomain:@"securityd"
                                                  code:errSecInternalError
                                              userInfo:@{NSLocalizedDescriptionKey: @"Rate-limited the CKKSUpdateDeviceStateOperation"}];
                return CKKSDatabaseTransactionRollback;
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
            STRONGIFY(self);
            CKKSKeychainView* blockCKKS = self.ckks;

            if(!error) {
                ckksnotice("ckksdevice", blockCKKS, "Device state record upload successful for %@: %@", record.recordID.recordName, record);
            } else {
                ckkserror("ckksdevice", blockCKKS, "error on row: %@ %@", error, record);
            }
        };

        self.modifyRecordsOperation.modifyRecordsCompletionBlock = ^(NSArray<CKRecord *> *savedRecords, NSArray<CKRecordID *> *deletedRecordIDs, NSError *ckerror) {
            STRONGIFY(self);
            CKKSKeychainView* strongCKKS = self.ckks;
            if(!self || !strongCKKS) {
                ckkserror("ckksdevice", strongCKKS, "received callback for released object");
                self.error = [NSError errorWithDomain:@"securityd" code:errSecInternalError userInfo:@{NSLocalizedDescriptionKey: @"no CKKS object"}];
                [self runBeforeGroupFinished:modifyComplete];
                return;
            }

            if(ckerror) {
                ckkserror("ckksdevice", strongCKKS, "CloudKit returned an error: %@", ckerror);
                self.error = ckerror;
                [self runBeforeGroupFinished:modifyComplete];
                return;
            }

            __block NSError* error = nil;

            [strongCKKS dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
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
                return CKKSDatabaseTransactionCommit;
            }];

            self.error = error;
            [self runBeforeGroupFinished:modifyComplete];
        };

        [self dependOnBeforeGroupFinished: self.modifyRecordsOperation];
        [ckks.operationDependencies.ckdatabase addOperation:self.modifyRecordsOperation];

        return CKKSDatabaseTransactionCommit;
    }];
}

@end

#endif // OCTAGON
