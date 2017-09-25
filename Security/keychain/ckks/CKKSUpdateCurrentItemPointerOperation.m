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

#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSOutgoingQueueEntry.h"
#import "keychain/ckks/CKKSIncomingQueueEntry.h"
#import "keychain/ckks/CKKSCurrentItemPointer.h"
#import "keychain/ckks/CKKSUpdateCurrentItemPointerOperation.h"
#import "keychain/ckks/CKKSManifest.h"

#import <CloudKit/CloudKit.h>

@interface CKKSUpdateCurrentItemPointerOperation ()
@property CKModifyRecordsOperation* modifyRecordsOperation;
@property CKOperationGroup* ckoperationGroup;

@property NSString* currentPointerIdentifier;
@property NSString* oldCurrentItemUUID;
@property NSString* currentItemUUID;
@end

@implementation CKKSUpdateCurrentItemPointerOperation

- (instancetype)initWithCKKSKeychainView:(CKKSKeychainView*) ckks
                          currentPointer:(NSString*)identifier
                             oldItemUUID:(NSString*)oldItemUUID
                             newItemUUID:(NSString*)newItemUUID
                        ckoperationGroup:(CKOperationGroup*)ckoperationGroup
{
    if((self = [super init])) {
        _ckks = ckks;

        _currentPointerIdentifier = identifier;
        _oldCurrentItemUUID = oldItemUUID;
        _currentItemUUID = newItemUUID;
        _ckoperationGroup = ckoperationGroup;
    }
    return self;
}

- (void)groupStart {
    CKKSKeychainView* ckks = self.ckks;
    if(!ckks) {
        ckkserror("ckkscurrent", ckks, "no CKKS object");
        self.error = [NSError errorWithDomain:@"securityd" code:errSecInternalError userInfo:@{NSLocalizedDescriptionKey: @"no CKKS object"}];
        return;
    }

    __weak __typeof(self) weakSelf = self;

    [ckks dispatchSyncWithAccountQueue:^bool {
        if(self.cancelled) {
            ckksnotice("ckksscan", ckks, "CKKSUpdateCurrentItemPointerOperation cancelled, quitting");
            return false;
        }

        NSError* error = nil;

        // Ensure that there's no pending pointer update
        CKKSCurrentItemPointer* cipPending = [CKKSCurrentItemPointer tryFromDatabase:self.currentPointerIdentifier state:SecCKKSProcessedStateRemote zoneID:ckks.zoneID error:&error];
        if(cipPending) {
            self.error = [NSError errorWithDomain:@"securityd" code:errSecItemInvalidValue
                                         userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat:@"Update to current item pointer is pending."]}];
            ckkserror("ckkscurrent", ckks, "Attempt to set a new current item pointer when one exists: %@", self.error);
            return false;
        }

        CKKSCurrentItemPointer* cip = [CKKSCurrentItemPointer tryFromDatabase:self.currentPointerIdentifier state:SecCKKSProcessedStateLocal zoneID:ckks.zoneID error:&error];

        if(cip) {
            // Ensure that the itempointer matches the old item
            if(![cip.currentItemUUID isEqualToString: self.oldCurrentItemUUID]) {
                ckksnotice("ckkscurrent", ckks, "Caller's idea of the current item pointer for %@ doesn't match; rejecting change of current", cip);
                self.error = [NSError errorWithDomain:@"securityd" code:errSecItemInvalidValue
                                         userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat:@"Current pointer does not match given value of '%@', aborting", self.oldCurrentItemUUID]}];
                return false;
            }
            // Cool. Since you know what you're updating, you're allowed to update!
            cip.currentItemUUID = self.currentItemUUID;

        } else if(self.oldCurrentItemUUID) {
            // Error case: the client thinks there's a current pointer, but we don't have one
            ckksnotice("ckkscurrent", ckks, "Requested to update a current item pointer but one doesn't exist at %@; rejecting change of current", self.currentPointerIdentifier);
            self.error = [NSError errorWithDomain:@"securityd" code:errSecItemInvalidValue
                                     userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat:@"Current pointer does not match given value of '%@', aborting", self.oldCurrentItemUUID]}];
            return false;
        } else {
            // No current item pointer? How exciting! Let's make you a nice new one.
            cip = [[CKKSCurrentItemPointer alloc] initForIdentifier:self.currentPointerIdentifier currentItemUUID:self.currentItemUUID state:SecCKKSProcessedStateLocal zoneID:ckks.zoneID encodedCKRecord:nil];
            ckksnotice("ckkscurrent", ckks, "Creating a new current item pointer: %@", cip);
        }

        // Check if either item is currently in any sync queue, and fail if so
        NSArray* oqes = [CKKSOutgoingQueueEntry allUUIDs:&error];
        NSArray* iqes = [CKKSIncomingQueueEntry allUUIDs:&error];
        if([oqes containsObject:self.currentItemUUID] || [iqes containsObject:self.currentItemUUID]) {
            error = [NSError errorWithDomain:@"securityd" code:errSecItemInvalidValue
                                    userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat:@"New item(%@) is being synced; can't set current pointer.", self.currentItemUUID]}];
        }
        if([oqes containsObject: self.oldCurrentItemUUID] || [iqes containsObject:self.oldCurrentItemUUID]) {
            error = [NSError errorWithDomain:@"securityd" code:errSecItemInvalidValue
                                    userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat:@"Old item(%@) is being synced; can't set current pointer.", self.oldCurrentItemUUID]}];
        }

        if(error) {
            ckkserror("ckkscurrent", ckks, "Error attempting to update current item pointer %@: %@", self.currentPointerIdentifier, error);
            self.error = error;
            return false;
        }

        // Make sure the item is synced, though!
        CKKSMirrorEntry* ckme = [CKKSMirrorEntry fromDatabase:cip.currentItemUUID zoneID:ckks.zoneID error:&error];
        if(!ckme || error) {
            ckkserror("ckkscurrent", ckks, "Error attempting to set a current item pointer to an item that isn't synced: %@ %@", cip, ckme);
            // Why can't you put nulls in dictionary literals?
            if(error) {
                error = [NSError errorWithDomain:@"securityd"
                                            code:errSecItemNotFound
                                        userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat:@"No synced item matching (%@); can't set current pointer.", cip.currentItemUUID],
                                                   NSUnderlyingErrorKey:error,
                                                   }];
            } else {
                error = [NSError errorWithDomain:@"securityd"
                                            code:errSecItemNotFound
                                        userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat:@"No synced item matching (%@); can't set current pointer.", cip.currentItemUUID],
                                                   }];
            }
            self.error = error;
            return false;
        }

        if ([CKKSManifest shouldSyncManifests]) {
            [ckks.egoManifest setCurrentItemUUID:self.currentItemUUID forIdentifier:self.currentPointerIdentifier];
        }

        ckksnotice("ckkscurrent", ckks, "Saving new current item pointer %@", cip);

        NSMutableDictionary<CKRecordID*, CKRecord*>* recordsToSave = [[NSMutableDictionary alloc] init];
        CKRecord* record = [cip CKRecordWithZoneID:ckks.zoneID];
        recordsToSave[record.recordID] = record;

        if([CKKSManifest shouldSyncManifests]) {
            for(CKRecord* record in [ckks.egoManifest allCKRecordsWithZoneID:ckks.zoneID]) {
                recordsToSave[record.recordID] = record;
            }
        }

        // Start a CKModifyRecordsOperation to save this new/updated record.
        NSBlockOperation* modifyComplete = [[NSBlockOperation alloc] init];
        modifyComplete.name = @"updateCurrentItemPointer-modifyRecordsComplete";
        [self dependOnBeforeGroupFinished: modifyComplete];

        self.modifyRecordsOperation = [[CKModifyRecordsOperation alloc] initWithRecordsToSave:recordsToSave.allValues recordIDsToDelete:nil];
        self.modifyRecordsOperation.atomic = TRUE;
        self.modifyRecordsOperation.timeoutIntervalForRequest = 2;
        self.modifyRecordsOperation.qualityOfService = NSQualityOfServiceUtility;
        self.modifyRecordsOperation.savePolicy = CKRecordSaveIfServerRecordUnchanged;
        self.modifyRecordsOperation.group = self.ckoperationGroup;

        self.modifyRecordsOperation.perRecordCompletionBlock = ^(CKRecord *record, NSError * _Nullable error) {
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            __strong __typeof(strongSelf.ckks) blockCKKS = strongSelf.ckks;

            if(!error) {
                ckksnotice("ckkscurrent", blockCKKS, "Current pointer upload successful for %@: %@", record.recordID.recordName, record);
            } else {
                ckkserror("ckkscurrent", blockCKKS, "error on row: %@ %@", error, record);
            }
        };

        self.modifyRecordsOperation.modifyRecordsCompletionBlock = ^(NSArray<CKRecord *> *savedRecords, NSArray<CKRecordID *> *deletedRecordIDs, NSError *ckerror) {
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            __strong __typeof(strongSelf.ckks) strongCKKS = strongSelf.ckks;
            if(!strongSelf || !strongCKKS) {
                ckkserror("ckkscurrent", strongCKKS, "received callback for released object");
                strongSelf.error = [NSError errorWithDomain:@"securityd" code:errSecInternalError userInfo:@{NSLocalizedDescriptionKey: @"no CKKS object"}];
                [strongCKKS scheduleOperation: modifyComplete];
                return;
            }

            if(ckerror) {
                ckkserror("ckkscurrent", strongCKKS, "CloudKit returned an error: %@", ckerror);
                strongSelf.error = ckerror;

                [ckks dispatchSync:^bool {
                    return [ckks _onqueueCKWriteFailed:ckerror attemptedRecordsChanged:recordsToSave];
                }];

                [strongCKKS scheduleOperation: modifyComplete];
                return;
            }

            __block NSError* error = nil;

            [strongCKKS dispatchSync: ^bool{
                for(CKRecord* record in savedRecords) {
                    // Save the item records
                    if([record.recordType isEqualToString: SecCKRecordCurrentItemType]) {
                        if([cip matchesCKRecord: record]) {
                            cip.storedCKRecord = record;
                            [cip saveToDatabase:&error];
                            if(error) {
                                ckkserror("ckkscurrent", strongCKKS, "Couldn't save new current pointer to database: %@", error);
                            }
                        } else {
                            ckkserror("ckkscurrent", strongCKKS, "CloudKit record does not match saved record, ignoring: %@ %@", record, cip);
                        }
                    }
                    else if ([CKKSManifest shouldSyncManifests] && [record.recordType isEqualToString:SecCKRecordManifestType]) {
                        CKKSManifest* manifest = [[CKKSManifest alloc] initWithCKRecord:record];
                        [manifest saveToDatabase:&error];
                        if (error) {
                            ckkserror("ckkscurrent", strongCKKS, "Couldn't save %@ to manifest: %@", record.recordID.recordName, error);
                            strongSelf.error = error;
                        }
                    }

                    // Schedule a 'view changed' notification
                    [strongCKKS.notifyViewChangedScheduler trigger];
                }
                return true;
            }];

            strongSelf.error = error;
            [strongCKKS scheduleOperation: modifyComplete];
        };

        [self dependOnBeforeGroupFinished: self.modifyRecordsOperation];
        [ckks.database addOperation: self.modifyRecordsOperation];

        return true;
    }];
}

@end

#endif // OCTAGON
