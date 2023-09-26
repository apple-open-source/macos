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

#import "keychain/ckks/CKKSDeleteCurrentItemPointersOperation.h"

#import "keychain/ckks/CKKSCurrentItemPointer.h"
#import "keychain/ot/ObjCImprovements.h"

@interface CKKSDeleteCurrentItemPointersOperation ()
@property (nullable) CKModifyRecordsOperation* modifyRecordsOperation;
@property (nullable) CKOperationGroup* ckoperationGroup;

@property CKKSOperationDependencies* deps;

@property (nonnull) NSString* accessGroup;

@property (nonnull) NSArray<NSString*>* identifiers;
@end

@implementation CKKSDeleteCurrentItemPointersOperation

- (instancetype)initWithCKKSOperationDependencies:(CKKSOperationDependencies*)operationDependencies
                                        viewState:(CKKSKeychainViewState*)viewState
                                      accessGroup:(NSString*)accessGroup
                                      identifiers:(NSArray<NSString*>*)identifiers
                                 ckoperationGroup:(CKOperationGroup* _Nullable)ckoperationGroup
{
    if ((self = [super init])) {
        _deps = operationDependencies;
        _viewState = viewState;
        _accessGroup = accessGroup;
        _identifiers = identifiers;
        _ckoperationGroup = ckoperationGroup;
    }
    return self;
}

- (void)groupStart
{
    WEAKIFY(self);
#if TARGET_OS_TV
    [self.deps.personaAdapter prepareThreadForKeychainAPIUseForPersonaIdentifier: nil];
#endif
    [self.deps.databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult {
        if(self.cancelled) {
            ckksnotice("ckkscurrent", self.viewState.zoneID, "CKKSDeleteCurrentItemPointersOperation cancelled, quitting");
            return CKKSDatabaseTransactionRollback;
        }

        ckksnotice("ckkscurrent", self.viewState.zoneID, "Deleting current item pointers (%lu)", (unsigned long)self.identifiers.count);

        NSMutableArray<CKRecordID*>* recordIDsToDelete = [[NSMutableArray alloc] init];
        for (NSString* identifier in self.identifiers) {
            NSString* recordName = [NSString stringWithFormat:@"%@-%@", self.accessGroup, identifier];
            CKRecordID* recordID = [[CKRecordID alloc] initWithRecordName:recordName zoneID:self.viewState.zoneID];
            [recordIDsToDelete addObject:recordID];
        }

        // Start a CKModifyRecordsOperation to delete current item pointers
        NSBlockOperation* modifyComplete = [[NSBlockOperation alloc] init];
        modifyComplete.name = @"deleteCurrentItemPointers-modifyRecordsComplete";
        [self dependOnBeforeGroupFinished:modifyComplete];

        self.modifyRecordsOperation = [[CKModifyRecordsOperation alloc] initWithRecordsToSave:nil recordIDsToDelete:recordIDsToDelete];
        self.modifyRecordsOperation.atomic = YES;
        // We're likely rolling a PCS identity, or creating a new one. User cares.
        self.modifyRecordsOperation.configuration.isCloudKitSupportOperation = YES;

        if(SecCKKSHighPriorityOperations()) {
            // This operation might be needed during CKKS/Manatee bringup, which affects the user experience. Bump our priority to get it off-device and unblock Manatee access.
            self.modifyRecordsOperation.qualityOfService = NSQualityOfServiceUserInitiated;
        }

        self.modifyRecordsOperation.group = self.ckoperationGroup;

        self.modifyRecordsOperation.perRecordDeleteBlock = ^(CKRecordID* recordID, NSError* error) {
            STRONGIFY(self);

            if(!error) {
                ckksnotice("ckkscurrent", self.viewState.zoneID, "Current pointer delete successful for %@", recordID.recordName);
            } else {
                ckkserror("ckkscurrent", self.viewState.zoneID, "error on row: %@ %@", error, recordID);
            }
        };

        self.modifyRecordsOperation.modifyRecordsCompletionBlock = ^(NSArray<CKRecord *> *savedRecords, NSArray<CKRecordID *> *deletedRecordIDs, NSError *ckerror) {
            STRONGIFY(self);
            id<CKKSDatabaseProviderProtocol> databaseProvider = self.deps.databaseProvider;

            if(ckerror) {
                ckkserror("ckkscurrent", self.viewState.zoneID, "CloudKit returned an error: %@", ckerror);
                self.error = ckerror;

                [self.operationQueue addOperation:modifyComplete];
                return;
            }

            __block NSError* error = nil;

            [databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
                for(CKRecordID* recordID in deletedRecordIDs) {
                    if(![CKKSCurrentItemPointer intransactionRecordDeleted:recordID contextID:self.deps.contextID resync:false error:&error]) {
                        ckkserror("ckkscurrent", self.viewState.zoneID, "Couldn't delete current item pointer for %@ from database: %@", recordID.recordName, error);
                        self.error = error;
                    }

                    // Schedule a 'view changed' notification
                    [self.viewState.notifyViewChangedScheduler trigger];
                }
                return CKKSDatabaseTransactionCommit;
            }];

            self.error = error;
            [self.operationQueue addOperation:modifyComplete];
        };

        [self dependOnBeforeGroupFinished: self.modifyRecordsOperation];
        [self.deps.ckdatabase addOperation:self.modifyRecordsOperation];

        return CKKSDatabaseTransactionCommit;
    }];
}

@end

#endif // OCTAGON
