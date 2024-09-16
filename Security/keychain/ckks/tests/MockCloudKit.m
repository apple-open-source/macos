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

#if OCTAGON

#import "keychain/ckks/tests/MockCloudKit.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSRecordHolder.h"
#import "keychain/ckks/CKKSReachabilityTracker.h"

#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>
#include <security_utilities/debugging.h>
#import <Foundation/Foundation.h>
#import <Foundation/NSDistributedNotificationCenter.h>

NSString* const CKKSMockCloudKitContextID = @"ckks_mock_cloudkit_contextid";

@implementation FakeCKOperation

- (BOOL)isFinishingOnCallbackQueue
{
    return NO;
}
@end

@implementation FakeCKModifyRecordZonesOperation
@synthesize database = _database;
@synthesize recordZonesToSave = _recordZonesToSave;
@synthesize recordZoneIDsToDelete = _recordZoneIDsToDelete;
@synthesize modifyRecordZonesCompletionBlock = _modifyRecordZonesCompletionBlock;
@synthesize group = _group;

- (CKOperationConfiguration*)configuration {
    return _configuration;
}

- (void)setConfiguration:(CKOperationConfiguration*)configuration {
    if(configuration) {
        _configuration = configuration;
    } else {
        _configuration = [[CKOperationConfiguration alloc] init];
    }
}

- (instancetype)initWithRecordZonesToSave:(nullable NSArray<CKRecordZone *> *)recordZonesToSave recordZoneIDsToDelete:(nullable NSArray<CKRecordZoneID *> *)recordZoneIDsToDelete {
    if(self = [super init]) {
        _recordZonesToSave = recordZonesToSave;
        _recordZoneIDsToDelete = recordZoneIDsToDelete;
        _modifyRecordZonesCompletionBlock = nil;

        _recordZonesSaved = nil;
        _recordZoneIDsDeleted = nil;
        _creationError = nil;

        __weak __typeof(self) weakSelf = self;
        self.completionBlock = ^{
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            if(!strongSelf) {
                ckkserror_global("ckks", "received callback for released object");
                return;
            }

            strongSelf.modifyRecordZonesCompletionBlock(strongSelf.recordZonesSaved, strongSelf.recordZoneIDsDeleted, strongSelf.creationError);
        };
    }
    return self;
}

-(void)main {
    // Create the zones we want; delete the ones we don't
    // No error handling whatsoever
    FakeCKDatabase* ckdb = [FakeCKModifyRecordZonesOperation ckdb];

    NSError* possibleError = [FakeCKModifyRecordZonesOperation shouldFailModifyRecordZonesOperation];
    if(possibleError) {
        self.creationError = possibleError;
        return;
    }

    for(CKRecordZone* zone in self.recordZonesToSave) {
        bool skipCreation = false;
        FakeCKZone* fakezone = ckdb[zone.zoneID];
        if(fakezone.failCreationSilently) {
            // Don't report an error, but do delete the zone
            ckdb[zone.zoneID] = nil;
            skipCreation = true;

        } else if(fakezone.creationError) {

            // Not the best way to do this, but it's an error
            // Needs fixing if you want to support multiple zone failures
            self.creationError = fakezone.creationError;

            // 'clear' the error
            ckdb[zone.zoneID] = nil;
            skipCreation = true;

        } else if(fakezone) {
            // Don't remake the zone, but report to the client that it was created
            skipCreation = true;
        }

        if(!skipCreation) {
            // Create the zone:
            ckksnotice_global("ckks", "Creating zone %@", zone);
            ckdb[zone.zoneID] = [[FakeCKZone alloc] initZone: zone.zoneID];
        }

        if(!self.recordZonesSaved) {
            self.recordZonesSaved = [[NSMutableArray alloc] init];
        }
        [self.recordZonesSaved addObject:zone];
    }

    for(CKRecordZoneID* zoneID in self.recordZoneIDsToDelete) {
        FakeCKZone* zone = ckdb[zoneID];

        if(zone) {
            // The zone exists.
            [FakeCKModifyRecordZonesOperation ensureZoneDeletionAllowed:zone];

            ckdb[zoneID] = nil;

            if(!self.recordZoneIDsDeleted) {
                self.recordZoneIDsDeleted = [[NSMutableArray alloc] init];
            }
            [self.recordZoneIDsDeleted addObject:zoneID];

        } else {
            // The zone does not exist! CloudKit will tell us that the deletion failed.
            if(!self.creationError) {
                self.creationError = [[NSError alloc] initWithDomain:CKErrorDomain code:CKErrorPartialFailure userInfo:@{
                    CKErrorRetryAfterKey: @(0.2),
                }];
            }

            // There really should be a better way to do this...
            NSMutableDictionary* newDictionary = [self.creationError.userInfo mutableCopy] ?: [NSMutableDictionary dictionary];
            NSMutableDictionary* newPartials = newDictionary[CKPartialErrorsByItemIDKey] ?: [NSMutableDictionary dictionary];
            newPartials[zoneID] = [[NSError alloc] initWithDomain:CKErrorDomain code:CKErrorZoneNotFound
                                                         userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat:@"Mock CloudKit: zone '%@' not found", zoneID.zoneName]}];
            newDictionary[CKPartialErrorsByItemIDKey] = newPartials;

            self.creationError = [[NSError alloc] initWithDomain:self.creationError.domain code:self.creationError.code userInfo:newDictionary];
        }
    }
}

+(FakeCKDatabase*) ckdb {
    // Shouldn't ever be called: must be mocked out.
    @throw [NSException exceptionWithName:NSInternalInconsistencyException
                                   reason:[NSString stringWithFormat:@"+ckdb[] must be mocked out for use"]
                                 userInfo:nil];
}

+ (NSError* _Nullable)shouldFailModifyRecordZonesOperation
{
    // Should be mocked out!
    return nil;
}

+(void)ensureZoneDeletionAllowed:(FakeCKZone*)zone {
    // Shouldn't ever be called; will be mocked out
    (void)zone;
}
@end

@implementation FakeCKModifySubscriptionsOperation
@synthesize database = _database;
@synthesize group = _group;
@synthesize subscriptionsToSave = _subscriptionsToSave;
@synthesize subscriptionIDsToDelete = _subscriptionIDsToDelete;
@synthesize modifySubscriptionsCompletionBlock = _modifySubscriptionsCompletionBlock;

- (CKOperationConfiguration*)configuration {
    return _configuration;
}

- (void)setConfiguration:(CKOperationConfiguration*)configuration {
    if(configuration) {
        _configuration = configuration;
    } else {
        _configuration = [[CKOperationConfiguration alloc] init];
    }
}

- (instancetype)initWithSubscriptionsToSave:(nullable NSArray<CKSubscription *> *)subscriptionsToSave subscriptionIDsToDelete:(nullable NSArray<NSString *> *)subscriptionIDsToDelete {
    if(self = [super init]) {
        _subscriptionsToSave = subscriptionsToSave;
        _subscriptionIDsToDelete = subscriptionIDsToDelete;
        _modifySubscriptionsCompletionBlock = nil;

        __weak __typeof(self) weakSelf = self;
        self.completionBlock = ^{
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            if(!strongSelf) {
                ckkserror_global("ckks", "received callback for released object");
                return;
            }

            strongSelf.modifySubscriptionsCompletionBlock(strongSelf.subscriptionsSaved, strongSelf.subscriptionIDsDeleted, strongSelf.subscriptionError);
        };
    }
    return self;
}

-(void)main {
    FakeCKDatabase* ckdb = [FakeCKModifySubscriptionsOperation ckdb];

    // Are these CKRecordZoneSubscription? Who knows!
    for(CKRecordZoneSubscription* subscription in self.subscriptionsToSave) {
        FakeCKZone* fakezone = ckdb[subscription.zoneID];

        if(!fakezone) {
            // This is an error: the zone doesn't exist
            ckksnotice("fakeck", subscription.zoneID, "failing subscription for missing zone");
            self.subscriptionError = [[NSError alloc] initWithDomain:CKErrorDomain
                                                                code:CKErrorPartialFailure
                                                            userInfo:@{
                CKErrorRetryAfterKey: @(0.2),
                CKPartialErrorsByItemIDKey:
                    @{subscription.zoneID:[[NSError alloc] initWithDomain:CKErrorDomain
                                                                     code:CKErrorZoneNotFound
                                                                 userInfo:@{}]}
            }];

        } else if(fakezone.subscriptionError) {
            ckksnotice("fakeck", subscription.zoneID, "failing subscription with injected error %@", fakezone.subscriptionError);
            // Not the best way to do this, but it's an error
            // Needs fixing if you want to support multiple zone failures
            self.subscriptionError = fakezone.subscriptionError;

            // 'clear' the error
            fakezone.subscriptionError = nil;
        } else if(fakezone.persistentSubscriptionError) {
            ckksnotice("fakeck", subscription.zoneID, "failing subscription with injected persistent error %@", fakezone.persistentSubscriptionError);
            // Not the best way to do this, but it's an error
            // Needs fixing if you want to support multiple zone failures
            self.subscriptionError = fakezone.persistentSubscriptionError;

            // do not clear the error

        } else {
            ckksnotice("fakeck", subscription.zoneID, "Successfully subscribed to zone");
            if(!self.subscriptionsSaved) {
                self.subscriptionsSaved = [[NSMutableArray alloc] init];
            }
            [self.subscriptionsSaved addObject:subscription];
        }
    }

    for(NSString* subscriptionID in self.subscriptionIDsToDelete) {
        secnotice("fakeck", "Successfully deleted subscription: %@", subscriptionID);
        if(!self.subscriptionIDsDeleted) {
            self.subscriptionIDsDeleted = [[NSMutableArray alloc] init];
        }

        [self.subscriptionIDsDeleted addObject:subscriptionID];
    }
}

+(FakeCKDatabase*) ckdb {
    // Shouldn't ever be called: must be mocked out.
    @throw [NSException exceptionWithName:NSInternalInconsistencyException
                                   reason:[NSString stringWithFormat:@"+ckdb[] must be mocked out for use"]
                                 userInfo:nil];
}
@end

@implementation FakeCKFetchRecordZoneChangesOperation
@synthesize database = _database;
@synthesize recordZoneIDs = _recordZoneIDs;
@synthesize configurationsByRecordZoneID = _configurationsByRecordZoneID;

@synthesize fetchAllChanges = _fetchAllChanges;
@synthesize recordChangedBlock = _recordChangedBlock;

@synthesize recordWithIDWasDeletedBlock = _recordWithIDWasDeletedBlock;
@synthesize recordZoneChangeTokensUpdatedBlock = _recordZoneChangeTokensUpdatedBlock;
@synthesize recordZoneFetchCompletionBlock = _recordZoneFetchCompletionBlock;
@synthesize fetchRecordZoneChangesCompletionBlock = _fetchRecordZoneChangesCompletionBlock;

@synthesize deviceIdentifier = _deviceIdentifier;

@synthesize operationID = _operationID;
@synthesize resolvedConfiguration = _resolvedConfiguration;
@synthesize group = _group;

- (CKOperationConfiguration*)configuration {
    return _configuration;
}

- (void)setConfiguration:(CKOperationConfiguration*)configuration {
    if(configuration) {
        _configuration = configuration;
    } else {
        _configuration = [[CKOperationConfiguration alloc] init];
    }
}

- (instancetype)initWithRecordZoneIDs:(NSArray<CKRecordZoneID *> *)recordZoneIDs configurationsByRecordZoneID:(nullable NSDictionary<CKRecordZoneID *, CKFetchRecordZoneChangesConfiguration *> *)configurationsByRecordZoneID {
    if(self = [super init]) {
        _recordZoneIDs = recordZoneIDs;
        _configurationsByRecordZoneID = configurationsByRecordZoneID;

        _operationID = @"fake-operation-ID";
        _deviceIdentifier = @"ckkstests";
    }
    return self;
}

+ (bool)isNetworkReachable
{
    // For mocking purposes.
    return true;
}

- (NSMutableArray<CKRecordID*>*) obtainReverseSortedRecordIDs:(FakeCKZone*)zone
                                                   fetchToken:(FakeCKServerChangeToken* _Nullable)fetchToken
                                                 limitFetchTo:(FakeCKServerChangeToken* _Nullable)limitFetchTo
{
    NSDictionary<CKRecordID*, CKRecord*>* db = fetchToken ? zone.pastDatabases[fetchToken.token] : zone.currentDatabase;
    NSArray<CKRecordID*>* reversedRecordIDs = [db keysSortedByValueUsingComparator:^NSComparisonResult(CKRecord*  _Nonnull record1, CKRecord*  _Nonnull record2) {
        NSInteger etag1 = [record1.etag integerValue];
        NSInteger etag2 = [record2.etag integerValue];
        
        if (etag1 > etag2) {
            return NSOrderedAscending;
        } else if (etag1 < etag2) {
            return NSOrderedDescending;
        }
        return NSOrderedSame;
    }];
    
    NSMutableArray<CKRecordID*>* reversedRecordIDsMutable = [NSMutableArray arrayWithArray:reversedRecordIDs];
    if (limitFetchTo) {
        for (CKRecordID* recordIDToDelete in [zone.pastDatabases[limitFetchTo.token] allKeys]) {
            [reversedRecordIDsMutable removeObject:recordIDToDelete];
        }
    }

    return reversedRecordIDsMutable;
    
}

- (void)main {
    // iterate through database, and return items that aren't in lastDatabase
    FakeCKDatabase* ckdb = [FakeCKFetchRecordZoneChangesOperation ckdb];
    if(self.recordZoneIDs.count == 0) {
        secerror("fakeck: No zones to fetch. Likely a bug?");
    }

    for(CKRecordZoneID* zoneID in self.recordZoneIDs) {
        FakeCKZone* zone = ckdb[zoneID];
        BOOL fetchNewestChangesFirst = NO;

        if(!zone) {
            // Only really supports a single zone failure
            ckksnotice("fakeck", zoneID, "Fetched for a missing zone %@", zoneID);
            NSError* zoneNotFoundError = [[NSError alloc] initWithDomain:CKErrorDomain
                                                                    code:CKErrorZoneNotFound
                                                                userInfo:@{}];
            NSError* error = [[NSError alloc] initWithDomain:CKErrorDomain
                                                        code:CKErrorPartialFailure
                                                    userInfo:@{CKPartialErrorsByItemIDKey: @{zoneID:zoneNotFoundError}}];

            if (self.blockAfterFetch) {
                self.blockAfterFetch();
            }
            self.fetchRecordZoneChangesCompletionBlock(error);
            return;
        }

        ++zone.fetchRecordZoneChangesOperationCount;
        [zone.fetchRecordZoneChangesTimestamps addObject: [NSDate date]];

        bool networkReachable = [FakeCKFetchRecordZoneChangesOperation isNetworkReachable];
        if (!networkReachable) {
            NSError *networkError = [NSError errorWithDomain:CKErrorDomain code:CKErrorNetworkFailure userInfo:NULL];
            self.fetchRecordZoneChangesCompletionBlock(networkError);
            return;
        }

        // Not precisely correct in the case of multiple zone fetches.
        NSError* mockError = [zone popFetchChangesError];
        if(mockError) {
            self.fetchRecordZoneChangesCompletionBlock(mockError);
            return;
        }

        // Extract the previousServerChangeToken used for syncing. If we have no previousServerChangeToken, this implies either 1) a fresh sync or 2) a resync, both of which will be done in reverse.
        FakeCKServerChangeToken *fetchTokenWithDirection = nil;
        if (self.configurationsByRecordZoneID[zoneID].previousServerChangeToken) {
            fetchTokenWithDirection = [FakeCKServerChangeToken decodeCKServerChangeToken:self.configurationsByRecordZoneID[zoneID].previousServerChangeToken];
        }
        ckksnotice("fakeck", zone.zoneID, "fetch token: %@", fetchTokenWithDirection);

        // Set direction of fetch either using previous fetch token, or we assume reverse sync with nil fetch token.
        fetchNewestChangesFirst = fetchTokenWithDirection ? !fetchTokenWithDirection.forward : YES;

        // Forward Sync
        if (!fetchNewestChangesFirst) {
            __block NSDictionary<CKRecordID*, CKRecord*>* lastDatabase = nil;
            __block NSDictionary<CKRecordID*, CKRecord*>* currentDatabase = nil;
            __block FakeCKServerChangeToken* currentChangeToken = nil;
            __block bool moreComing = false;

            __block NSError* opError = nil;

            dispatch_sync(zone.queue, ^{
                lastDatabase = fetchTokenWithDirection ? zone.pastDatabases[fetchTokenWithDirection.token] : nil;

                // You can fetch with the current change token; that's fine
                if([fetchTokenWithDirection.token isEqual:zone.currentChangeToken.token]) {
                    lastDatabase = zone.currentDatabase;
                }

                currentDatabase = zone.currentDatabase;
                currentChangeToken = zone.currentChangeToken;

                if (zone.limitFetchTo != nil) {
                    currentDatabase = zone.pastDatabases[zone.limitFetchTo.token];
                    currentChangeToken = zone.limitFetchTo;
                    zone.limitFetchTo = nil;
                    opError = zone.limitFetchError;
                    moreComing = true;
                }
            });

            ckksnotice("fakeck", zone.zoneID, "FakeCKFetchRecordZoneChangesOperation(%@): database is currently %@ change token %@ database then: %@", zone.zoneID, currentDatabase, fetchTokenWithDirection, lastDatabase);

            if(!lastDatabase && fetchTokenWithDirection) {
                ckksnotice("fakeck", zone.zoneID, "no database for this change token: failing fetch with 'CKErrorChangeTokenExpired'");
                self.fetchRecordZoneChangesCompletionBlock([[NSError alloc]
                                                            initWithDomain:CKErrorDomain
                                                            code:CKErrorPartialFailure userInfo:@{CKPartialErrorsByItemIDKey:
                                                                                                      @{zoneID:[[NSError alloc] initWithDomain:CKErrorDomain code:CKErrorChangeTokenExpired userInfo:@{}]}
                                                                                                }]);
                if (self.blockAfterFetch) {
                    self.blockAfterFetch();
                }
                return;
            }

            NSMutableDictionary<CKRecordID*, CKRecord*>* filteredCurrentDatabase = [currentDatabase mutableCopy];
            NSMutableDictionary<CKRecordID*, CKRecord*>* filteredLastDatabase = [lastDatabase mutableCopy];

            for(CKRecordID* recordID in zone.recordIDsToSkip) {
                filteredCurrentDatabase[recordID] = nil;
                filteredLastDatabase[recordID] = nil;
            }

            [filteredCurrentDatabase enumerateKeysAndObjectsUsingBlock:^(CKRecordID * _Nonnull recordID, CKRecord * _Nonnull record, BOOL * _Nonnull stop) {
                id last = [filteredLastDatabase objectForKey: recordID];
                if(!last || ![record isEqual:last]) {
                    self.recordChangedBlock(record);
                }
            }];

            // iterate through lastDatabase, and delete items that aren't in database
            [filteredLastDatabase enumerateKeysAndObjectsUsingBlock:^(CKRecordID * _Nonnull recordID, CKRecord * _Nonnull record, BOOL * _Nonnull stop) {
                id current = [filteredCurrentDatabase objectForKey: recordID];
                if(current == nil) {
                    self.recordWithIDWasDeletedBlock(recordID, [record recordType]);
                }
            }];

            // Record current serverChangeToken as previous serverChangeToken
            NSKeyedArchiver *encoder = [[NSKeyedArchiver alloc] initRequiringSecureCoding:YES];
            [currentChangeToken encodeWithCoder:encoder];
            CKServerChangeToken* encodedChangeToken = [[CKServerChangeToken alloc] initWithData:encoder.encodedData];
            self.recordZoneChangeTokensUpdatedBlock(zoneID, encodedChangeToken, nil);
            self.recordZoneFetchCompletionBlock(zoneID, encodedChangeToken, nil, moreComing, opError);
        }

        // Reverse sync
        else {
            __block NSMutableDictionary<CKRecordID*, CKRecord*>* currentDatabase = nil;
            __block FakeCKServerChangeToken* currentChangeToken = nil;
            __block bool moreComing = false;
            __block NSError* opError = nil;
            __block NSMutableArray<CKRecordID*>* sortedCKRecordIDs = nil;

            dispatch_sync(zone.queue, ^{

                currentChangeToken = [[FakeCKServerChangeToken alloc] initWithAttributes:zone.currentChangeToken.token forward:NO];

                // Limit the "latest" records returned to `limitFetchTo`
                if (zone.limitFetchTo != nil) {
                    currentChangeToken = zone.limitFetchTo;
                    zone.limitFetchTo = nil;
                    opError = zone.limitFetchError;
                    moreComing = true;
                }

                // Obtain the record IDs in the diff between previous fetch token and current change token
                sortedCKRecordIDs = [self obtainReverseSortedRecordIDs:zone fetchToken:fetchTokenWithDirection limitFetchTo:zone.limitFetchTo];
                currentDatabase = [[NSMutableDictionary alloc] init];
                for (CKRecordID* recordID in sortedCKRecordIDs) {
                    currentDatabase[recordID] = zone.currentDatabase[recordID];
                }

                ckksnotice("fakeck", zone.zoneID, "FakeCKFetchRecordZoneChangesOperation(%@): database is currently %@ change token %@", zone.zoneID, currentDatabase, fetchTokenWithDirection);

                if(fetchTokenWithDirection && !zone.pastDatabases[fetchTokenWithDirection.token]) {
                    ckksnotice("fakeck", zone.zoneID, "no previous state for this change token: failing fetch with 'CKErrorChangeTokenExpired'");
                    self.fetchRecordZoneChangesCompletionBlock([[NSError alloc]
                                                                initWithDomain:CKErrorDomain
                                                                code:CKErrorPartialFailure userInfo:@{CKPartialErrorsByItemIDKey:
                                                                                                          @{zoneID:[[NSError alloc] initWithDomain:CKErrorDomain code:CKErrorChangeTokenExpired userInfo:@{}]}
                                                                                                    }]);
                    return;
                }

                NSMutableDictionary<CKRecordID*, CKRecord*>* filteredCurrentDatabase = [currentDatabase mutableCopy];

                for(CKRecordID* recordID in zone.recordIDsToSkip) {
                    filteredCurrentDatabase[recordID] = nil;
                }

                // Zone additions and modifications should be processed the same way regardless of forward sync or reverse sync. Deletions don't call `recordWithIDWasDeletedBlock`.

                [filteredCurrentDatabase enumerateKeysAndObjectsUsingBlock:^(CKRecordID * _Nonnull recordID, CKRecord * _Nonnull record, BOOL * _Nonnull stop) {
                    self.recordChangedBlock(record);
                }];

                // If we no longer have more things coming and we are in reverse sync then we change to forward syncing
                if (!moreComing) {
                    currentChangeToken.forward = YES;
                }

                // Record current serverChangeToken as previous serverChangeToken
                NSKeyedArchiver *encoder = [[NSKeyedArchiver alloc] initRequiringSecureCoding:YES];
                [currentChangeToken encodeWithCoder:encoder];
                CKServerChangeToken* encodedChangeToken = [[CKServerChangeToken alloc] initWithData:encoder.encodedData];
                self.recordZoneChangeTokensUpdatedBlock(zoneID, encodedChangeToken, nil);
                self.recordZoneFetchCompletionBlock(zoneID, encodedChangeToken, nil, moreComing, opError);
            });
        }
    }

    if(self.blockAfterFetch) {
        self.blockAfterFetch();
    }

    self.fetchRecordZoneChangesCompletionBlock(nil);
}

+(FakeCKDatabase*) ckdb {
    // Shouldn't ever be called: must be mocked out.
    @throw [NSException exceptionWithName:NSInternalInconsistencyException
                                   reason:[NSString stringWithFormat:@"+ckdb[] must be mocked out for use"]
                                 userInfo:nil];
}
@end

@implementation FakeCKFetchRecordsOperation
@synthesize database = _database;
@synthesize group = _group;
@synthesize recordIDs = _recordIDs;
@synthesize desiredKeys = _desiredKeys;
@synthesize configuration = _configuration;

@synthesize perRecordProgressBlock = _perRecordProgressBlock;
@synthesize perRecordCompletionBlock = _perRecordCompletionBlock;

@synthesize fetchRecordsCompletionBlock = _fetchRecordsCompletionBlock;

- (instancetype)init {
    if((self = [super init])) {

    }
    return self;
}
- (instancetype)initWithRecordIDs:(NSArray<CKRecordID *> *)recordIDs {
    if((self = [super init])) {
        _recordIDs = recordIDs;
    }
    return self;
}

- (void)main {
    FakeCKDatabase* ckdb = [FakeCKFetchRecordsOperation ckdb];

    // Doesn't call the per-record progress block
    NSMutableDictionary<CKRecordID*, CKRecord*>* records = [NSMutableDictionary dictionary];
    NSError* operror = nil;

    for(CKRecordID* recordID in self.recordIDs) {
        CKRecordZoneID* zoneID = recordID.zoneID;
        FakeCKZone* zone = ckdb[zoneID];

        if(!zone) {
            ckksnotice("fakeck", zoneID, "Fetched for a missing zone %@", zoneID);
            NSError* zoneNotFoundError = [[NSError alloc] initWithDomain:CKErrorDomain
                                                                    code:CKErrorZoneNotFound
                                                                userInfo:@{}];
            NSError* error = [[NSError alloc] initWithDomain:CKErrorDomain
                                                        code:CKErrorPartialFailure
                                                    userInfo:@{CKPartialErrorsByItemIDKey: @{zoneID:zoneNotFoundError}}];

            // Not strictly right, but good enough for now
            self.fetchRecordsCompletionBlock(nil, error);
            return;
        }

        if([zone.recordIDsToSkip containsObject:recordID]) {
            secerror("fakeck: Skipping returning record as per test request: %@", recordID);
            continue;
        }

        CKRecord* record = zone.currentDatabase[recordID];
        if(record) {
            if(self.perRecordCompletionBlock) {
                self.perRecordCompletionBlock(record, recordID, nil);
            }
            records[recordID] = record;
        } else {
            secerror("fakeck: Should be an error fetching %@", recordID);

            if(!operror) {
                operror = [[NSError alloc] initWithDomain:CKErrorDomain code:CKErrorPartialFailure userInfo:nil];
            }

            // There really should be a better way to do this...
            NSMutableDictionary* newDictionary = [operror.userInfo mutableCopy] ?: [NSMutableDictionary dictionary];
            NSMutableDictionary* newPartials = newDictionary[CKPartialErrorsByItemIDKey] ?: [NSMutableDictionary dictionary];
            newPartials[recordID] = [[NSError alloc] initWithDomain:operror.domain code:CKErrorUnknownItem
                                                           userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat:@"Mock CloudKit: no record of %@", recordID]}];
            newDictionary[CKPartialErrorsByItemIDKey] = newPartials;

            operror = [[NSError alloc] initWithDomain:operror.domain code:operror.code userInfo:newDictionary];

            /// TODO: do this better
            if(self.perRecordCompletionBlock) {
                self.perRecordCompletionBlock(nil, recordID, newPartials[zoneID]);
            }
        }
    }

    if(self.fetchRecordsCompletionBlock) {
        self.fetchRecordsCompletionBlock(records, operror);
    }
}

+(FakeCKDatabase*) ckdb {
    // Shouldn't ever be called: must be mocked out.
    @throw [NSException exceptionWithName:NSInternalInconsistencyException
                                   reason:[NSString stringWithFormat:@"+ckdb[] must be mocked out for use"]
                                 userInfo:nil];
}
@end


@implementation FakeCKQueryOperation
@synthesize query = _query;
@synthesize cursor = _cursor;
@synthesize zoneID = _zoneID;
@synthesize resultsLimit = _resultsLimit;
@synthesize desiredKeys = _desiredKeys;
@synthesize recordFetchedBlock = _recordFetchedBlock;
@synthesize queryCompletionBlock = _queryCompletionBlock;

- (instancetype)initWithQuery:(CKQuery *)query {
    if((self = [super init])) {
        _query = query;
    }
    return self;
}

- (void)main {
    FakeCKDatabase* ckdb = [FakeCKFetchRecordsOperation ckdb];

    FakeCKZone* zone = ckdb[self.zoneID];
    if(!zone) {
        ckksnotice("fakeck", self.zoneID, "Queried a missing zone %@", self.zoneID);

        // I'm really not sure if this is right, but...
        NSError* zoneNotFoundError = [[NSError alloc] initWithDomain:CKErrorDomain
                                                                code:CKErrorZoneNotFound
                                                            userInfo:@{
            CKErrorRetryAfterKey: @(0.2),
        }];
        self.queryCompletionBlock(nil, zoneNotFoundError);
        return;
    }

    NSMutableArray<CKRecord*>* matches = [NSMutableArray array];
    for(CKRecordID* recordID in zone.currentDatabase.keyEnumerator) {
        CKRecord* record = zone.currentDatabase[recordID];

        if([self.query.recordType isEqualToString: record.recordType] &&
           [self.query.predicate evaluateWithObject:record]) {

            [matches addObject:record];
            self.recordFetchedBlock(record);
        }
    }

    if(self.queryCompletionBlock) {
        // The query cursor will be non-null if there are more than self.resultsLimit classes. Don't implement this.
        self.queryCompletionBlock(nil, nil);
    }
}


+(FakeCKDatabase*) ckdb {
    // Shouldn't ever be called: must be mocked out.
    @throw [NSException exceptionWithName:NSInternalInconsistencyException
                                   reason:[NSString stringWithFormat:@"+ckdb[] must be mocked out for use"]
                                 userInfo:nil];
}
@end



// Do literally nothing
@implementation FakeAPSConnection
@synthesize delegate;

@synthesize enabledTopics;
@synthesize opportunisticTopics;
@synthesize darkWakeTopics;

- (id)initWithEnvironmentName:(NSString *)environmentName namedDelegatePort:(NSString*)namedDelegatePort queue:(dispatch_queue_t)queue {
    if(self = [super init]) {
    }
    return self;
}
@end

// Do literally nothing

@implementation FakeNSNotificationCenter
+ (instancetype)defaultCenter {
    return [[FakeNSNotificationCenter alloc] init];
}
- (void)addObserver:(id)observer selector:(SEL)aSelector name:(nullable NSNotificationName)aName object:(nullable id)anObject {
}
- (void)removeObserver:(id)observer {
}
@end

@implementation FakeNSDistributedNotificationCenter
+ (instancetype)defaultCenter
{
    return [[FakeNSDistributedNotificationCenter alloc] init];
}
- (void)addObserver:(id)observer selector:(SEL)aSelector name:(nullable NSNotificationName)aName object:(nullable id)anObject {
}
- (void)removeObserver:(id)observer {
}
- (void)postNotificationName:(NSNotificationName)name object:(nullable NSString *)object userInfo:(nullable NSDictionary *)userInfo options:(NSDistributedNotificationOptions)options
{
    [[NSNotificationCenter defaultCenter] postNotificationName:name object:object userInfo:userInfo];
}
@end


@interface FakeCKZone ()
@property NSMutableArray<NSError*>* fetchErrors;
@property int ctag;
@end

@implementation FakeCKZone
- (instancetype)initZone: (CKRecordZoneID*) zoneID {
    if(self = [super init]) {

        _zoneID = zoneID;
        _currentDatabase = [[NSMutableDictionary alloc] init];
        _pastDatabases = [[NSMutableDictionary alloc] init];
        _recordIDsToSkip = [NSMutableSet set];

        _fetchErrors = [[NSMutableArray alloc] init];

        _queue = dispatch_queue_create("fake-ckzone", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);

        _limitFetchTo = nil;
        _fetchRecordZoneChangesOperationCount = 0;
        _fetchRecordZoneChangesTimestamps = [[NSMutableArray alloc] init];
        _ctag = 0;

        dispatch_sync(_queue, ^{
            [self _onqueueRollChangeToken];
        });
    }
    return self;
}

- (NSString*)description {
    return [NSString stringWithFormat:@"<FakeCKZone(%@): %@>", self.zoneID, self.currentDatabase.allKeys];
}

- (void)_onqueueRollChangeToken {
    dispatch_assert_queue(self.queue);

    NSData* changeToken = [[[NSUUID UUID] UUIDString] dataUsingEncoding:NSUTF8StringEncoding];
    self.currentChangeToken = [[FakeCKServerChangeToken alloc] initWithAttributes:[[CKServerChangeToken alloc] initWithData: changeToken] forward:YES];
    self.ctag += 1;
}

- (void)addToZone: (CKKSCKRecordHolder*) item zoneID: (CKRecordZoneID*) zoneID {
    dispatch_sync(self.queue, ^{
        [self _onqueueAddToZone:item zoneID:zoneID];
    });
}

- (CKRecord*)_onqueueAddToZone:(CKKSCKRecordHolder*)item zoneID:(CKRecordZoneID*)zoneID {
    dispatch_assert_queue(self.queue);

    CKRecord* record = [item CKRecordWithZoneID: zoneID];

    secnotice("fake-cloudkit", "adding item to zone(%@): %@", zoneID.zoneName, item);
    secnotice("fake-cloudkit", "new record: %@", record);

    [self _onqueueAddToZone: record];

    // Save off the etag
    item.storedCKRecord = record;
    return record;
}

- (void)addToZone: (CKRecord*) record {
    dispatch_sync(self.queue, ^{
        [self _onqueueAddToZone:record];
    });
}

- (CKRecord*)_onqueueAddToZone:(CKRecord*)record {
    dispatch_assert_queue(self.queue);

    // Save off this current databse
    self.pastDatabases[self.currentChangeToken.token] = [self.currentDatabase mutableCopy];

    [self _onqueueRollChangeToken];

    record.etag = [@(self.ctag) stringValue];

    ckksnotice("fakeck", self.zoneID, "change tag: %@ %@", record.recordChangeTag, record.recordID);
    record.modificationDate = [NSDate date];
    self.currentDatabase[record.recordID] = record;
    return record;
}

- (NSError * _Nullable)errorFromSavingRecord:(CKRecord*) record {
    return [self errorFromSavingRecord:record otherConcurrentWrites:@[]];
}

- (NSError* _Nullable)errorFromSavingRecord:(CKRecord*)record otherConcurrentWrites:(NSArray<CKRecord*>*)concurrentRecords
{
    CKRecord* existingRecord = self.currentDatabase[record.recordID];

    // First, implement CKKS-specific server-side checks
    if([record.recordType isEqualToString:SecCKRecordCurrentKeyType]) {
        CKReference* parentKey = record[SecCKRecordParentKeyRefKey];

        CKRecord* existingParentKey = self.currentDatabase[parentKey.recordID];

        if(!existingParentKey) {
            bool foundConcurrentRecord = false;
            // Well, sure, but will this match anything we're uploading now?
            for(CKRecord* concurrent in concurrentRecords) {
                if([parentKey.recordID isEqual:concurrent.recordID]) {
                    foundConcurrentRecord = true;
                }
            }

            if(!foundConcurrentRecord) {
                ckksnotice("fakeck", self.zoneID, "bad sync key reference! Fail the write: %@ %@", record, existingRecord);

                return [FakeCKZone internalPluginError:@"CloudkitKeychainService" code:CKKSServerMissingRecord description:@"synckey record: record not found"];
            }
        }
    }
    //

    if(existingRecord && ![existingRecord.recordChangeTag isEqualToString: record.recordChangeTag]) {
        ckksnotice("fakeck", self.zoneID, "change tag mismatch! Fail the write: %@ %@", record, existingRecord);

        // TODO: doesn't yet support CKRecordChangedErrorAncestorRecordKey, since I don't understand it
        return [[NSError alloc] initWithDomain:CKErrorDomain code:CKErrorServerRecordChanged
                                      userInfo:@{CKRecordChangedErrorClientRecordKey:record,
                                                 CKRecordChangedErrorServerRecordKey:existingRecord}];
    }

    if(!existingRecord && record.etag != nil) {
        ckksnotice("fakeck", self.zoneID, "update to a record that doesn't exist! Fail the write: %@", record);
        return [[NSError alloc] initWithDomain:CKErrorDomain code:CKErrorUnknownItem
                                      userInfo:nil];
    }
    return nil;
}

- (void)addCKRecordToZone:(CKRecord*) record {
    if([self errorFromSavingRecord: record]) {
        ckksnotice("fakeck", self.zoneID, "change tag mismatch! Fail the write!");
    }

    [self addToZone: record];
}

- (void)deleteFromHistory:(CKRecordID*)recordID {
    for(NSMutableDictionary* pastDatabase in self.pastDatabases.objectEnumerator) {
        [pastDatabase removeObjectForKey:recordID];
    }
    [self.currentDatabase removeObjectForKey:recordID];
}


- (NSError*)deleteCKRecordIDFromZone:(CKRecordID*) recordID {
    // todo: fail somehow
    dispatch_sync(self.queue, ^{
        ckksnotice("fakeck", self.zoneID, "Change token before server-deleted record is : %@", self.currentChangeToken);

        self.pastDatabases[self.currentChangeToken.token] = [self.currentDatabase mutableCopy];
        [self _onqueueRollChangeToken];

        [self.currentDatabase removeObjectForKey: recordID];

        ckksnotice("fakeck", self.zoneID, "Change token after server-deleted record is : %@", self.currentChangeToken);
    });
    return nil;
}

- (void)failNextFetchWith: (NSError*) fetchChangesError {
    @synchronized(self.fetchErrors) {
        [self.fetchErrors addObject: fetchChangesError];
    }
}

- (NSError * _Nullable)popFetchChangesError {
    NSError* error = nil;
    @synchronized(self.fetchErrors) {
        if(self.fetchErrors.count > 0) {
            error = self.fetchErrors[0];
            [self.fetchErrors removeObjectAtIndex:0];
        }
    }
    return error;
}

+ (NSError*)internalPluginError:(NSString*)serverDomain code:(NSInteger)code description:(NSString*)desc
{
    // Note: uses SecCKKSContainerName, but that's probably okay
    NSError* extensionError = [[NSError alloc] initWithDomain:serverDomain
                                                         code:code
                                                     userInfo:@{
        CKErrorServerDescriptionKey: desc,
        NSLocalizedDescriptionKey: desc,
    }];
    NSError* internalError = [[NSError alloc] initWithDomain:CKUnderlyingErrorDomain
                                                        code:CKUnderlyingErrorPluginError
                                                    userInfo:@{CKErrorServerDescriptionKey: desc,
                                                               NSLocalizedDescriptionKey: desc,
                                                               NSUnderlyingErrorKey: extensionError,
                                                             }];
    NSError* error = [[NSError alloc] initWithDomain:CKErrorDomain
                                                code:CKErrorServerRejectedRequest
                                            userInfo:@{NSUnderlyingErrorKey: internalError,
                                                       CKErrorServerDescriptionKey: desc,
                                                       NSLocalizedDescriptionKey: desc,
                                                       CKContainerIDKey: SecCKKSContainerName,
                                                     }];

    return error;
}
@end

@implementation FakeCKKSNotifier
+(void)post:(NSString*)notification {
    if(notification) {
        // This isn't actually fake, but XCTest likes NSNotificationCenter a whole lot.
        // These notifications shouldn't escape this process, so it's perfect.
        ckksnotice_global("ckks", "sending fake NSNotification %@", notification);
        [[NSNotificationCenter defaultCenter] postNotificationName:notification object:nil];
    }
}
@end

@implementation FakeCKAccountInfo

- (NSUInteger)hash {
    return self.accountStatus ^ self.accountPartition ^ self.hasValidCredentials;
}

- (BOOL)isEqual:(id)object {
    if (self == object) return YES;
    if (![object isKindOfClass:[self class]]) return NO;
    
    FakeCKAccountInfo *other = (FakeCKAccountInfo *)object;
    
    return ((self.accountStatus == other.accountStatus) &&
            (self.accountPartition == other.accountPartition) &&
            (self.hasValidCredentials == other.hasValidCredentials));
}

@end

@implementation FakeCKServerChangeToken

- (instancetype)initWithAttributes:(CKServerChangeToken*)token
                           forward:(BOOL)forward {
    if ((self = [super init])) {
        _token = token;
        _forward = forward;
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder {

    if ((self = [super init])) {
        if (coder) {
            _token = [[CKServerChangeToken alloc] initWithData:[coder decodeObjectOfClass:[NSData class] forKey:@"token"]];
            _forward = [coder decodeBoolForKey:@"forward"];

        }
    }
    return self;
}

- (void)encodeWithCoder:(nonnull NSCoder *)coder {
    [coder encodeObject:self.token.data forKey:@"token"];
    [coder encodeBool:self.forward forKey:@"forward"];
}

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (instancetype)decodeCKServerChangeToken:(CKServerChangeToken*)token {
    NSKeyedUnarchiver* decoder = [[NSKeyedUnarchiver alloc] initForReadingFromData:token.data error:nil];
    return [[FakeCKServerChangeToken alloc] initWithCoder:decoder];
}

@end

#endif // OCTAGON

