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

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSCurrentKeyPointer.h"
#import "keychain/ckks/CKKSKey.h"
#import "keychain/ckks/CKKSOutgoingQueueEntry.h"
#import "keychain/ckks/CKKSReencryptOutgoingItemsOperation.h"
#import "keychain/ckks/CKKSItemEncrypter.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/ckks/CKKSAnalytics.h"
#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/analytics/CKKSPowerCollection.h"

#if OCTAGON

// Note: reencryption is not, strictly speaking, a CloudKit operation. However, we preserve this to pass it back to the outgoing queue operation we'll create
@interface CKKSReencryptOutgoingItemsOperation ()
@end

@implementation CKKSReencryptOutgoingItemsOperation
@synthesize nextState = _nextState;
@synthesize intendedState = _intendedState;

- (instancetype)init {
    return nil;
}
- (instancetype)initWithDependencies:(CKKSOperationDependencies*)dependencies
                                ckks:(CKKSKeychainView*)ckks
                       intendedState:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState
{
    if(self = [super init]) {
        _deps = dependencies;
        _ckks = ckks;

        _nextState = errorState;
        _intendedState = intendedState;

        [self addNullableDependency:ckks.keyStateReadyDependency];
        [self addNullableDependency:ckks.holdReencryptOutgoingItemsOperation];

        // We also depend on the key hierarchy being reasonable
        [self addNullableDependency:ckks.keyStateReadyDependency];
    }
    return self;
}

- (void)main
{
    CKKSKeychainView* ckks = self.ckks;

    [self.deps.databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
        if(self.cancelled) {
            ckksnotice("ckksreencrypt", self.deps.zoneID, "CKKSReencryptOutgoingItemsOperation cancelled, quitting");
            return CKKSDatabaseTransactionRollback;
        }

        ckks.lastReencryptOutgoingItemsOperation = self;

        NSError* error = nil;
        bool newItems = false;

        NSArray<CKKSOutgoingQueueEntry*>* oqes = [CKKSOutgoingQueueEntry allInState:SecCKKSStateReencrypt zoneID:self.deps.zoneID error:&error];
        if(error) {
            ckkserror("ckksreencrypt", self.deps.zoneID, "Error fetching oqes from database: %@", error);
            self.error = error;
            return CKKSDatabaseTransactionRollback;
        }

        [CKKSPowerCollection CKKSPowerEvent:kCKKSPowerEventReencryptOutgoing zone:self.deps.zoneID.zoneName count:oqes.count];

        for(CKKSOutgoingQueueEntry* oqe in oqes) {
            // If there's already a 'new' item replacing this one, drop the reencryption on the floor
            CKKSOutgoingQueueEntry* newOQE = [CKKSOutgoingQueueEntry tryFromDatabase:oqe.uuid state:SecCKKSStateNew zoneID:oqe.item.zoneID error:&error];
            if(error) {
                ckkserror("ckksreencrypt", self.deps.zoneID, "Couldn't load 'new' OQE to determine status: %@", error);
                self.error = error;
                error = nil;
                continue;
            }
            if(newOQE) {
                ckksnotice("ckksreencrypt", self.deps.zoneID, "Have a new OQE superceding %@ (%@), skipping", oqe, newOQE);
                // Don't use the state transition here, either, since this item isn't really changing states
                [oqe deleteFromDatabase:&error];
                if(error) {
                    ckkserror("ckksreencrypt", self.deps.zoneID, "Couldn't delete reencrypting OQE(%@) from database: %@", oqe, error);
                    self.error = error;
                    error = nil;
                    continue;
                }
                continue;
            }

            ckksnotice("ckksreencrypt", self.deps.zoneID, "Reencrypting item %@", oqe);

            NSDictionary* item = [CKKSItemEncrypter decryptItemToDictionary: oqe.item error:&error];
            if(error) {
                if ([error.domain isEqualToString:@"securityd"] && error.code == errSecItemNotFound) {
                    ckkserror("ckksreencrypt", self.deps.zoneID, "Couldn't find key in keychain; asking for reset: %@", error);
                    [ckks _onqueuePokeKeyStateMachine];
                    self.nextState = SecCKKSZoneKeyStateUnhealthy;
                } else {
                    ckkserror("ckksreencrypt", self.deps.zoneID, "Couldn't decrypt item %@: %@", oqe, error);
                }
                self.error = error;
                error = nil;
                continue;
            }

            // Pick a key whose class matches the keyclass that this item
            CKKSKey* originalKey = [CKKSKey fromDatabase: oqe.item.parentKeyUUID zoneID:self.deps.zoneID error:&error];
            if(error) {
                ckkserror("ckksreencrypt", self.deps.zoneID, "Couldn't fetch key (%@) for item %@: %@", oqe.item.parentKeyUUID, oqe, error);
                self.error = error;
                error = nil;
                continue;
            }

            CKKSKey* newkey = [CKKSKey currentKeyForClass: originalKey.keyclass zoneID:self.deps.zoneID error:&error];
            [newkey ensureKeyLoaded: &error];
            if(error) {
                ckkserror("ckksreencrypt", self.deps.zoneID, "Couldn't fetch the current key for class %@: %@", originalKey.keyclass, error);
                self.error = error;
                error = nil;
                continue;
            }

            CKKSMirrorEntry* ckme = [CKKSMirrorEntry tryFromDatabase:oqe.item.uuid zoneID:self.deps.zoneID error:&error];
            if(error) {
                ckkserror("ckksreencrypt", self.deps.zoneID, "Couldn't fetch ckme (%@) for item %@: %@", oqe.item.parentKeyUUID, oqe, error);
                self.error = error;
                error = nil;
                continue;
            }

            CKKSItem* encryptedItem = [CKKSItemEncrypter encryptCKKSItem:oqe.item
                                                          dataDictionary:item
                                                        updatingCKKSItem:ckme.item
                                                               parentkey:newkey
                                                                   error:&error];

            if(error) {
                ckkserror("ckksreencrypt", self.deps.zoneID, "Couldn't encrypt under the new key %@: %@", newkey, error);
                self.error = error;
                error = nil;
                continue;
            }

            CKKSOutgoingQueueEntry* replacement = [[CKKSOutgoingQueueEntry alloc] initWithCKKSItem:encryptedItem
                                                                                            action:oqe.action
                                                                                             state:SecCKKSStateNew
                                                                                         waitUntil:nil
                                                                                       accessGroup:oqe.accessgroup];

            // Don't use the CKKSKeychainView state change here, since we're doing a wholesale item swap.
            [oqe deleteFromDatabase:&error];
            [replacement saveToDatabase:&error];
            if(error) {
                ckkserror("ckksreencrypt", self.deps.zoneID, "Couldn't save newly-encrypted oqe %@: %@", replacement, error);
                self.error = error;
                error = nil;
                continue;
            }

            newItems = true;
        }

        CKKSAnalytics* logger = [CKKSAnalytics logger];
        if (self.error) {
            [logger logRecoverableError:error forEvent:CKKSEventProcessReencryption zoneName:self.deps.zoneID.zoneName withAttributes:nil];
        } else {
            [logger logSuccessForEvent:CKKSEventProcessReencryption zoneName:self.deps.zoneID.zoneName ];
        }

        if(newItems) {
            [ckks processOutgoingQueue:self.deps.ckoperationGroup];
        }
        self.nextState = self.intendedState;
        return true;
    }];
}

@end;

#endif
