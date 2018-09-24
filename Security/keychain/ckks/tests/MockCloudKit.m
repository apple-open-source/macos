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
                secerror("ckks: received callback for released object");
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
            continue;
        }

        if(!skipCreation) {
            // Create the zone:
            secnotice("ckks", "Creating zone %@", zone);
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
            // The zone exists. Its deletion will succeed.
            [FakeCKModifyRecordZonesOperation ensureZoneDeletionAllowed:zone];
            ckdb[zoneID] = nil;

            if(!self.recordZoneIDsDeleted) {
                self.recordZoneIDsDeleted = [[NSMutableArray alloc] init];
            }
            [self.recordZoneIDsDeleted addObject:zoneID];
        } else {
            // The zone does not exist! CloudKit will tell us that the deletion failed.
            if(!self.creationError) {
                self.creationError = [[CKPrettyError alloc] initWithDomain:CKErrorDomain code:CKErrorPartialFailure userInfo:nil];
            }

            // There really should be a better way to do this...
            NSMutableDictionary* newDictionary = [self.creationError.userInfo mutableCopy] ?: [NSMutableDictionary dictionary];
            NSMutableDictionary* newPartials = newDictionary[CKPartialErrorsByItemIDKey] ?: [NSMutableDictionary dictionary];
            newPartials[zoneID] = [[CKPrettyError alloc] initWithDomain:CKErrorDomain code:CKErrorZoneNotFound
                                                               userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat:@"Mock CloudKit: zone '%@' not found", zoneID.zoneName]}];
            newDictionary[CKPartialErrorsByItemIDKey] = newPartials;

            self.creationError = [[CKPrettyError alloc] initWithDomain:self.creationError.domain code:self.creationError.code userInfo:newDictionary];
        }
    }
}

+(FakeCKDatabase*) ckdb {
    // Shouldn't ever be called: must be mocked out.
    @throw [NSException exceptionWithName:NSInternalInconsistencyException
                                   reason:[NSString stringWithFormat:@"+ckdb[] must be mocked out for use"]
                                 userInfo:nil];
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
                secerror("ckks: received callback for released object");
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
            self.subscriptionError = [[CKPrettyError alloc] initWithDomain:CKErrorDomain
                                                                      code:CKErrorPartialFailure
                                                                  userInfo:@{CKPartialErrorsByItemIDKey:
                                                                                 @{subscription.zoneID:[[CKPrettyError alloc] initWithDomain:CKErrorDomain
                                                                                                                                        code:CKErrorZoneNotFound
                                                                                                                                    userInfo:@{}]}
                                                                                                      }];

        } else if(fakezone.subscriptionError) {
            // Not the best way to do this, but it's an error
            // Needs fixing if you want to support multiple zone failures
            self.subscriptionError = fakezone.subscriptionError;

            // 'clear' the error
            fakezone.subscriptionError = nil;
        } else {
            if(!self.subscriptionsSaved) {
                self.subscriptionsSaved = [[NSMutableArray alloc] init];
            }
            [self.subscriptionsSaved addObject:subscription];
        }
    }

    for(NSString* subscriptionID in self.subscriptionIDsToDelete) {
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
    }
    return self;
}

- (void)main {
    // iterate through database, and return items that aren't in lastDatabase
    FakeCKDatabase* ckdb = [FakeCKFetchRecordZoneChangesOperation ckdb];
    if(self.recordZoneIDs.count == 0) {
        secerror("fakeck: No zones to fetch. Likely a bug?");
    }

    for(CKRecordZoneID* zoneID in self.recordZoneIDs) {
        FakeCKZone* zone = ckdb[zoneID];
        if(!zone) {
            // Only really supports a single zone failure
            ckksnotice("fakeck", zoneID, "Fetched for a missing zone %@", zoneID);
            NSError* zoneNotFoundError = [[CKPrettyError alloc] initWithDomain:CKErrorDomain
                                                                          code:CKErrorZoneNotFound
                                                                      userInfo:@{}];
            NSError* error = [[CKPrettyError alloc] initWithDomain:CKErrorDomain
                                                              code:CKErrorPartialFailure
                                                          userInfo:@{CKPartialErrorsByItemIDKey: @{zoneID:zoneNotFoundError}}];

            self.fetchRecordZoneChangesCompletionBlock(error);
            return;
        }

        SCNetworkReachabilityFlags reachabilityFlags = [CKKSReachabilityTracker getReachabilityFlags:NULL];
        if ((reachabilityFlags & kSCNetworkReachabilityFlagsReachable) == 0) {
            NSError *networkError = [NSError errorWithDomain:CKErrorDomain code:CKErrorNetworkFailure userInfo:NULL];
            self.fetchRecordZoneChangesCompletionBlock(networkError);
            return;
        }

        // Not precisely correct in the case of multiple zone fetches. However, we don't currently do that, so it'll work for now.
        NSError* mockError = [zone popFetchChangesError];
        if(mockError) {
            self.fetchRecordZoneChangesCompletionBlock(mockError);
            return;
        }

        // Extract the database at the last time they asked
        CKServerChangeToken* fetchToken = self.configurationsByRecordZoneID[zoneID].previousServerChangeToken;
        __block NSMutableDictionary<CKRecordID*, CKRecord*>* lastDatabase = nil;
        __block NSDictionary<CKRecordID*, CKRecord*>* currentDatabase = nil;
        __block CKServerChangeToken* currentChangeToken = nil;

        dispatch_sync(zone.queue, ^{
            lastDatabase = fetchToken ? zone.pastDatabases[fetchToken] : nil;

            // You can fetch with the current change token; that's fine
            if([fetchToken isEqual:zone.currentChangeToken]) {
                lastDatabase = zone.currentDatabase;
            }

            currentDatabase = zone.currentDatabase;
            currentChangeToken = zone.currentChangeToken;
        });

        ckksnotice("fakeck", zone.zoneID, "FakeCKFetchRecordZoneChangesOperation(%@): database is currently %@ change token %@ database then: %@", zone.zoneID, currentDatabase, fetchToken, lastDatabase);

        if(!lastDatabase && fetchToken) {
            ckksnotice("fakeck", zone.zoneID, "no database for this change token: failing fetch with 'CKErrorChangeTokenExpired'");
            self.fetchRecordZoneChangesCompletionBlock([[CKPrettyError alloc]
                                                        initWithDomain:CKErrorDomain
                                                        code:CKErrorPartialFailure userInfo:@{CKPartialErrorsByItemIDKey:
                              @{zoneID:[[CKPrettyError alloc] initWithDomain:CKErrorDomain code:CKErrorChangeTokenExpired userInfo:@{}]}
                            }]);
            return;
        }

        [currentDatabase enumerateKeysAndObjectsUsingBlock:^(CKRecordID * _Nonnull recordID, CKRecord * _Nonnull record, BOOL * _Nonnull stop) {
            id last = [lastDatabase objectForKey: recordID];
            if(!last || ![record isEqual:last]) {
                self.recordChangedBlock(record);
            }
        }];

        // iterate through lastDatabase, and delete items that aren't in database
        [lastDatabase enumerateKeysAndObjectsUsingBlock:^(CKRecordID * _Nonnull recordID, CKRecord * _Nonnull record, BOOL * _Nonnull stop) {

            id current = [currentDatabase objectForKey: recordID];
            if(current == nil) {
                self.recordWithIDWasDeletedBlock(recordID, [record recordType]);
            }
        }];

        self.recordZoneChangeTokensUpdatedBlock(zoneID, currentChangeToken, nil);
        self.recordZoneFetchCompletionBlock(zoneID, currentChangeToken, nil, NO, nil);

        if(self.blockAfterFetch) {
            self.blockAfterFetch();
        }

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
            NSError* zoneNotFoundError = [[CKPrettyError alloc] initWithDomain:CKErrorDomain
                                                                          code:CKErrorZoneNotFound
                                                                      userInfo:@{}];
            NSError* error = [[CKPrettyError alloc] initWithDomain:CKErrorDomain
                                                              code:CKErrorPartialFailure
                                                          userInfo:@{CKPartialErrorsByItemIDKey: @{zoneID:zoneNotFoundError}}];

            // Not strictly right, but good enough for now
            self.fetchRecordsCompletionBlock(nil, error);
            return;
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
                operror = [[CKPrettyError alloc] initWithDomain:CKErrorDomain code:CKErrorPartialFailure userInfo:nil];
            }

            // There really should be a better way to do this...
            NSMutableDictionary* newDictionary = [operror.userInfo mutableCopy] ?: [NSMutableDictionary dictionary];
            NSMutableDictionary* newPartials = newDictionary[CKPartialErrorsByItemIDKey] ?: [NSMutableDictionary dictionary];
            newPartials[recordID] = [[CKPrettyError alloc] initWithDomain:operror.domain code:CKErrorUnknownItem
                                                                 userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat:@"Mock CloudKit: no record of %@", recordID]}];
            newDictionary[CKPartialErrorsByItemIDKey] = newPartials;

            operror = [[CKPrettyError alloc] initWithDomain:operror.domain code:operror.code userInfo:newDictionary];

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
        NSError* zoneNotFoundError = [[CKPrettyError alloc] initWithDomain:CKErrorDomain
                                                                      code:CKErrorZoneNotFound
                                                                  userInfo:@{}];
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

- (id)initWithEnvironmentName:(NSString *)environmentName namedDelegatePort:(NSString*)namedDelegatePort queue:(dispatch_queue_t)queue {
    if(self = [super init]) {
    }
    return self;
}

- (void)setEnabledTopics:(NSArray *)enabledTopics {
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

@interface FakeCKZone ()
@property NSMutableArray<NSError*>* fetchErrors;
@end

@implementation FakeCKZone
- (instancetype)initZone: (CKRecordZoneID*) zoneID {
    if(self = [super init]) {

        _zoneID = zoneID;
        _currentDatabase = [[NSMutableDictionary alloc] init];
        _pastDatabases = [[NSMutableDictionary alloc] init];

        _fetchErrors = [[NSMutableArray alloc] init];

        _queue = dispatch_queue_create("fake-ckzone", DISPATCH_QUEUE_SERIAL);

        dispatch_sync(_queue, ^{
            [self _onqueueRollChangeToken];
        });
    }
    return self;
}

- (void)_onqueueRollChangeToken {
    dispatch_assert_queue(self.queue);

    NSData* changeToken = [[[NSUUID UUID] UUIDString] dataUsingEncoding:NSUTF8StringEncoding];
    self.currentChangeToken = [[CKServerChangeToken alloc] initWithData: changeToken];
}

- (void)addToZone: (CKKSCKRecordHolder*) item zoneID: (CKRecordZoneID*) zoneID {
    dispatch_sync(self.queue, ^{
        [self _onqueueAddToZone:item zoneID:zoneID];
    });
}

- (void)_onqueueAddToZone:(CKKSCKRecordHolder*)item zoneID:(CKRecordZoneID*)zoneID {
    dispatch_assert_queue(self.queue);

    CKRecord* record = [item CKRecordWithZoneID: zoneID];
    [self _onqueueAddToZone: record];

    // Save off the etag
    item.storedCKRecord = record;
}

- (void)addToZone: (CKRecord*) record {
    dispatch_sync(self.queue, ^{
        [self _onqueueAddToZone:record];
    });
}

- (void)_onqueueAddToZone:(CKRecord*)record {
    dispatch_assert_queue(self.queue);

    // Save off this current databse
    self.pastDatabases[self.currentChangeToken] = [self.currentDatabase mutableCopy];

    [self _onqueueRollChangeToken];

    record.etag = [self.currentChangeToken description];
    ckksnotice("fakeck", self.zoneID, "change tag: %@ %@", record.recordChangeTag, record.recordID);
    record.modificationDate = [NSDate date];
    self.currentDatabase[record.recordID] = record;
}

- (NSError * _Nullable)errorFromSavingRecord:(CKRecord*) record {
    CKRecord* existingRecord = self.currentDatabase[record.recordID];
    if(existingRecord && ![existingRecord.recordChangeTag isEqualToString: record.recordChangeTag]) {
        ckksnotice("fakeck", self.zoneID, "change tag mismatch! Fail the write: %@ %@", record, existingRecord);

        // TODO: doesn't yet support CKRecordChangedErrorAncestorRecordKey, since I don't understand it
        return [[CKPrettyError alloc] initWithDomain:CKErrorDomain code:CKErrorServerRecordChanged
                                            userInfo:@{CKRecordChangedErrorClientRecordKey:record,
                                                       CKRecordChangedErrorServerRecordKey:existingRecord}];
    }

    if(!existingRecord && record.etag != nil) {
        ckksnotice("fakeck", self.zoneID, "update to a record that doesn't exist! Fail the write: %@", record);
        return [[CKPrettyError alloc] initWithDomain:CKErrorDomain code:CKErrorUnknownItem
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
        self.pastDatabases[self.currentChangeToken] = [self.currentDatabase mutableCopy];
        [self _onqueueRollChangeToken];

        [self.currentDatabase removeObjectForKey: recordID];
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
@end

@implementation FakeCKKSNotifier
+(void)post:(NSString*)notification {
    if(notification) {
        // This isn't actually fake, but XCTest likes NSNotificationCenter a whole lot.
        // These notifications shouldn't escape this process, so it's perfect.
        secnotice("ckks", "sending fake NSNotification %@", notification);
        [[NSNotificationCenter defaultCenter] postNotificationName:notification object:nil];
    }
}
@end

#endif // OCTAGON

