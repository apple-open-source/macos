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

#import <XCTest/XCTest.h>
#import <Security/Security.h>
#import <Security/SecItemPriv.h>
#import "CloudKitMockXCTest.h"

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSKey.h"
#import "keychain/ckks/CKKSIncomingQueueEntry.h"
#import "keychain/ckks/CKKSOutgoingQueueEntry.h"
#import "keychain/ckks/CKKSIncomingQueueEntry.h"
#import "keychain/ckks/CKKSZoneStateEntry.h"
#import "keychain/ckks/CKKSDeviceStateEntry.h"
#import "keychain/ckks/CKKSRateLimiter.h"

#include "featureflags/affordance_featureflags.h"
#include "keychain/securityd/SecItemServer.h"

@interface CloudKitKeychainSQLTests : CloudKitMockXCTest
@end

@implementation CloudKitKeychainSQLTests

+ (void)setUp {
    [super setUp];
}

- (void)setUp {
    SecCKKSDisable();
    KCSharingSetChangeTrackingEnabled(false);
    [super setUp];
}

- (void)tearDown {
    [super tearDown];
    KCSharingClearChangeTrackingEnabledOverride();
    SecCKKSResetSyncing();
}

- (void)addTestZoneEntries {
    CKKSOutgoingQueueEntry* one = [[CKKSOutgoingQueueEntry alloc] initWithCKKSItem:
                                   [[CKKSItem alloc] initWithUUID:[[NSUUID UUID] UUIDString]
                                                    parentKeyUUID:[[NSUUID UUID] UUIDString]
                                                        contextID:CKKSMockCloudKitContextID
                                                           zoneID:self.testZoneID
                                                          encItem:[@"nonsense" dataUsingEncoding:NSUTF8StringEncoding]
                                                       wrappedkey:[[CKKSWrappedAESSIVKey alloc]initWithBase64: @"KFfL58XtugiYNoD859EjG0StfrYd6eakm0CQrgX7iO+DEo4kio3WbEeA1kctCU0GaeTGsRFpbdy4oo6jXhVu7cZqB0svhUPGq55aGnszUjI="]
                                                  generationCount:0
                                                           encver:0]
                                                                        action:SecCKKSActionAdd
                                                                         state:SecCKKSStateError
                                                                     waitUntil:nil
                                                                   accessGroup:@"nope"];


    CKKSOutgoingQueueEntry* two = [[CKKSOutgoingQueueEntry alloc] initWithCKKSItem:
                                   [[CKKSItem alloc] initWithUUID:[[NSUUID UUID] UUIDString]
                                                    parentKeyUUID:[[NSUUID UUID] UUIDString]
                                                        contextID:CKKSMockCloudKitContextID
                                                           zoneID:self.testZoneID
                                                          encItem:[@"nonsense" dataUsingEncoding:NSUTF8StringEncoding]
                                                       wrappedkey:[[CKKSWrappedAESSIVKey alloc]initWithBase64: @"KFfL58XtugiYNoD859EjG0StfrYd6eakm0CQrgX7iO+DEo4kio3WbEeA1kctCU0GaeTGsRFpbdy4oo6jXhVu7cZqB0svhUPGq55aGnszUjI="]
                                                  generationCount:0
                                                           encver:0]
                                                                            action:SecCKKSActionAdd
                                                                             state:SecCKKSStateNew
                                                                         waitUntil:nil
                                                                       accessGroup:@"nope"];

    CKKSOutgoingQueueEntry* three = [[CKKSOutgoingQueueEntry alloc] initWithCKKSItem:
                                   [[CKKSItem alloc] initWithUUID:[[NSUUID UUID] UUIDString]
                                                    parentKeyUUID:[[NSUUID UUID] UUIDString]
                                                        contextID:CKKSMockCloudKitContextID
                                                           zoneID:self.testZoneID
                                                          encItem:[@"nonsense" dataUsingEncoding:NSUTF8StringEncoding]
                                                       wrappedkey:[[CKKSWrappedAESSIVKey alloc]initWithBase64: @"KFfL58XtugiYNoD859EjG0StfrYd6eakm0CQrgX7iO+DEo4kio3WbEeA1kctCU0GaeTGsRFpbdy4oo6jXhVu7cZqB0svhUPGq55aGnszUjI="]
                                                  generationCount:0
                                                           encver:0]
                                                                            action:SecCKKSActionModify
                                                                             state:SecCKKSStateError
                                                                         waitUntil:nil
                                                                       accessGroup:@"nope"];

    [CKKSSQLDatabaseObject performCKKSTransaction:^CKKSDatabaseTransactionResult {
        NSError* error = nil;
        [one saveToDatabase:&error];
        [two saveToDatabase: &error];
        [three saveToDatabase: &error];
        XCTAssertNil(error, "no error saving ZoneStateEntries to database");
        return CKKSDatabaseTransactionCommit;
    }];
}

- (void)testCKKSOutgoingQueueEntry {
    NSString* testUUID = @"157A3171-0677-451B-9EAE-0DDC4D4315B0";
    NSUUID* testKeyUUID = [[NSUUID alloc] init];

    NSError * nserror;
    __block CFErrorRef error = NULL;

    CKKSOutgoingQueueEntry* shouldFail = [CKKSOutgoingQueueEntry fromDatabase:testUUID
                                                                        state:SecCKKSStateInFlight
                                                                    contextID:CKKSMockCloudKitContextID
                                                                       zoneID:self.testZoneID
                                                                        error: &nserror];
    XCTAssertNil(shouldFail, "Can't find a nonexisting object");
    XCTAssertNotNil(nserror, "NSError exists when things break");

    __weak __typeof(self) weakSelf = self;
    kc_with_dbt(true, &error, ^bool (SecDbConnectionRef dbconn) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        XCTAssertNotNil(strongSelf, "called while self still exists");

        NSString * sql = @"insert INTO outgoingqueue (UUID, parentKeyUUID, ckzone, action, state, accessgroup, gencount, encitem, wrappedkey, encver,contextID) VALUES (?,?,?,?,?,?,?,?,?,?,?);";
        SecDbPrepare(dbconn, (__bridge CFStringRef) sql, &error, ^void (sqlite3_stmt *stmt) {
            SecDbBindText(stmt, 1, [testUUID UTF8String], strlen([testUUID UTF8String]), NULL, &error);
            SecDbBindText(stmt, 2, [[testKeyUUID UUIDString] UTF8String], strlen([[testKeyUUID UUIDString] UTF8String]), NULL, &error);
            SecDbBindObject(stmt, 3, (__bridge CFStringRef) @"testzone", &error);
            SecDbBindText(stmt, 4, "newitem", strlen("newitem"), NULL, &error);
            SecDbBindText(stmt, 5, "unprocessed", strlen("unprocessed"), NULL, &error);
            SecDbBindText(stmt, 6, "com.apple.access", strlen("com.apple.access"), NULL, &error);
            SecDbBindText(stmt, 7, "0", strlen("0"), NULL, &error);
            SecDbBindText(stmt, 8, "bm9uc2Vuc2UK", strlen("bm9uc2Vuc2UK"), NULL, &error);
            SecDbBindObject(stmt, 9, CFSTR("KFfL58XtugiYNoD859EjG0StfrYd6eakm0CQrgX7iO+DEo4kio3WbEeA1kctCU0GaeTGsRFpbdy4oo6jXhVu7cZqB0svhUPGq55aGnszUjI="), &error);
            SecDbBindText(stmt, 10, "0", strlen("0"), NULL, &error);
            SecDbBindObject(stmt, 11, (__bridge CFStringRef)CKKSMockCloudKitContextID, &error);


            SecDbStep(dbconn, stmt, &error, ^(bool *stop) {
                // don't do anything, I guess?
            });

            XCTAssertNil((__bridge NSError*)error, @"no error occurred while adding row to database");

            CFReleaseNull(error);
        });
        XCTAssertNil((__bridge NSError*)error, @"no error occurred preparing sql");

        CFReleaseNull(error);
        return true;
    });

    // Create another oqe with different values
    CKKSItem* baseitem = [[CKKSItem alloc] initWithUUID: [[NSUUID UUID] UUIDString]
                                          parentKeyUUID:[[NSUUID UUID] UUIDString]
                                              contextID:CKKSMockCloudKitContextID
                                                 zoneID:self.testZoneID
                                                encItem:[@"nonsense" dataUsingEncoding:NSUTF8StringEncoding]
                                             wrappedkey:[[CKKSWrappedAESSIVKey alloc]initWithBase64: @"KFfL58XtugiYNoD859EjG0StfrYd6eakm0CQrgX7iO+DEo4kio3WbEeA1kctCU0GaeTGsRFpbdy4oo6jXhVu7cZqB0svhUPGq55aGnszUjI="]
                                        generationCount:0
                                                 encver:0];
    CKKSOutgoingQueueEntry* other = [[CKKSOutgoingQueueEntry alloc] initWithCKKSItem:baseitem
                                                                              action:SecCKKSActionAdd
                                                                               state:SecCKKSStateError
                                                                           waitUntil:[NSDate date]
                                                                         accessGroup:@"nope"];
    [CKKSSQLDatabaseObject performCKKSTransaction:^CKKSDatabaseTransactionResult {
        NSError* saveError = nil;
        [other saveToDatabase:&saveError];
        XCTAssertNil(saveError, "no error occurred saving to database");
        return CKKSDatabaseTransactionCommit;
    }];

    CKKSOutgoingQueueEntry * oqe = [CKKSOutgoingQueueEntry fromDatabase:testUUID
                                                                  state:@"unprocessed"
                                                              contextID:CKKSMockCloudKitContextID
                                                                 zoneID:self.testZoneID
                                                                  error: &nserror];
    XCTAssertNil(nserror, "no error occurred creating from database");

    XCTAssertNotNil(oqe, "load outgoing queue entry from database");
    XCTAssertEqualObjects(oqe.state, @"unprocessed", "state matches what was in the DB");

    oqe.item.parentKeyUUID = @"not a parent key either";
    oqe.action = @"null";
    oqe.state = @"savedtocloud";
    oqe.accessgroup = @"com.evil.access";
    oqe.item.generationCount = (NSInteger) 1;
    oqe.item.base64encitem = @"bW9yZW5vbnNlbnNlCg==";
    oqe.item.encver = 1;

    [CKKSSQLDatabaseObject performCKKSTransaction:^CKKSDatabaseTransactionResult {
        NSError* saveError = nil;
        [oqe saveToDatabase:&saveError];
        XCTAssertNil(saveError, "no error occurred saving to database");
        return CKKSDatabaseTransactionCommit;
    }];

    CKKSOutgoingQueueEntry * oqe2 = [CKKSOutgoingQueueEntry fromDatabase:testUUID
                                                                   state:@"savedtocloud"
                                                               contextID:CKKSMockCloudKitContextID
                                                                  zoneID:self.testZoneID
                                                                   error: &nserror];
    XCTAssertNil(nserror, "no error occurred");

    XCTAssertEqualObjects(oqe2.item.parentKeyUUID, @"not a parent key either", @"parent key uuid persisted through db save and load");
    XCTAssertEqualObjects(oqe2.item.zoneID       , self.testZoneID           , @"zone id persisted through db save and load");
    XCTAssertEqualObjects(oqe2.action            , @"null"                   , @"action persisted through db save and load");
    XCTAssertEqualObjects(oqe2.state             , @"savedtocloud"           , @"state persisted through db save and load");
    XCTAssertEqual(       oqe2.waitUntil         , nil                       , @"no date when none given");
    XCTAssertEqualObjects(oqe2.accessgroup       , @"com.evil.access"        , @"accessgroup persisted through db save and load");
    XCTAssertEqual(       oqe2.item.generationCount, (NSUInteger) 1          , @"generationCount persisted through db save and load");
    XCTAssertEqualObjects(oqe2.item.base64encitem, @"bW9yZW5vbnNlbnNlCg=="   , @"encitem persisted through db save and load");
    XCTAssertEqual(       oqe2.item.encver,     1                            , @"encver persisted through db save and load");
    XCTAssertEqualObjects([oqe2.item.wrappedkey base64WrappedKey], @"KFfL58XtugiYNoD859EjG0StfrYd6eakm0CQrgX7iO+DEo4kio3WbEeA1kctCU0GaeTGsRFpbdy4oo6jXhVu7cZqB0svhUPGq55aGnszUjI=",
                                                                        @"wrapped key persisted through db save and load");

    // Test 'all' methods
    NSArray<CKKSOutgoingQueueEntry*>* oqes = [CKKSOutgoingQueueEntry all:&nserror];
    XCTAssertNil(nserror, "no error occurred");
    XCTAssertNotNil(oqes, "receive oqes from database");
    XCTAssert([oqes count] == 2, "received 2 oqes from all");

    NSArray<CKKSOutgoingQueueEntry*>* oqeswhere = [CKKSOutgoingQueueEntry allWhere: @{@"state": @"savedtocloud"}  error:&nserror];
    XCTAssertNil(nserror, "no error occurred");
    XCTAssertNotNil(oqeswhere, "receive oqes from database");
    XCTAssert([oqeswhere count] == 1, "received 1 oqe from allWhere");

    // Test row deletion
    nserror = nil;


    [CKKSSQLDatabaseObject performCKKSTransaction:^CKKSDatabaseTransactionResult {
        NSError* deleteError = nil;
        [oqe2 deleteFromDatabase:&deleteError];
        XCTAssertNil(deleteError, "no error occurred deleting existing item");
        return CKKSDatabaseTransactionCommit;
    }];

    oqe2 = [CKKSOutgoingQueueEntry fromDatabase:testUUID
                                          state:@"savedtocloud"
                                      contextID:CKKSMockCloudKitContextID
                                         zoneID:self.testZoneID
                                          error:&nserror];
    XCTAssertNil(oqe2, "Can't find a nonexisting object");
    XCTAssertNotNil(nserror, "NSError exists when things break");

    // Test loading other
    nserror = nil;
    CKKSOutgoingQueueEntry* other2 = [CKKSOutgoingQueueEntry fromDatabase:other.item.uuid
                                                                    state:SecCKKSStateError
                                                                contextID:CKKSMockCloudKitContextID
                                                                   zoneID:self.testZoneID
                                                                    error:&nserror];
    XCTAssertNil(nserror, "No error loading other2 from database");
    XCTAssertNotNil(other2, "Able to re-load other.");
    XCTAssertEqualObjects(other, other2, "loaded object is equal to object");
}

- (void)testOverwriteCKKSIncomingQueueEntry {
    NSError* error = nil;

    CKKSItem* baseitem = [[CKKSItem alloc] initWithUUID: [[NSUUID UUID] UUIDString]
                                          parentKeyUUID:[[NSUUID UUID] UUIDString]
                                              contextID:CKKSMockCloudKitContextID
                                                 zoneID:self.testZoneID
                                                encItem:[@"nonsense" dataUsingEncoding:NSUTF8StringEncoding]
                                             wrappedkey:[[CKKSWrappedAESSIVKey alloc]initWithBase64: @"KFfL58XtugiYNoD859EjG0StfrYd6eakm0CQrgX7iO+DEo4kio3WbEeA1kctCU0GaeTGsRFpbdy4oo6jXhVu7cZqB0svhUPGq55aGnszUjI="]
                                        generationCount:0
                                                 encver:0];
    CKKSIncomingQueueEntry* delete = [[CKKSIncomingQueueEntry alloc] initWithCKKSItem:baseitem
                                                                               action:SecCKKSActionDelete
                                                                                state:SecCKKSStateNew];
    [CKKSSQLDatabaseObject performCKKSTransaction:^CKKSDatabaseTransactionResult {
        NSError* saveError = nil;
        [delete saveToDatabase:&saveError];
        XCTAssertNil(saveError, "no error occurred saving delete IQE to database");
        return CKKSDatabaseTransactionCommit;
    }];

    NSArray<CKKSIncomingQueueEntry*>* entries = [CKKSIncomingQueueEntry all:&error];
    XCTAssertNil(error, "Should be no error fetching alll IQEs");
    XCTAssertEqual(entries.count, 1u, "Should be one entry");
    XCTAssertEqualObjects(entries[0].action, SecCKKSActionDelete, "Should have delete as an action");

    CKKSIncomingQueueEntry* add = [[CKKSIncomingQueueEntry alloc] initWithCKKSItem:baseitem
                                                                            action:SecCKKSActionAdd
                                                                             state:SecCKKSStateNew];
    [CKKSSQLDatabaseObject performCKKSTransaction:^CKKSDatabaseTransactionResult {
        NSError* saveError = nil;
        [add saveToDatabase:&saveError];
        XCTAssertNil(saveError, "no error occurred saving add IQE to database");
        return CKKSDatabaseTransactionCommit;
    }];

    entries = [CKKSIncomingQueueEntry all:&error];
    XCTAssertNil(error, "Should be no error fetching alll IQEs");
    XCTAssertEqual(entries.count, 1u, "Should be one entry");
    XCTAssertEqualObjects(entries[0].action, SecCKKSActionAdd, "Should have add as an action");
}

-(void)testCKKSZoneStateEntrySQL {
    NSString* context1 = @"context1";
    CKKSZoneStateEntry* zse = [[CKKSZoneStateEntry alloc] initWithContextID:context1
                                                                   zoneName:@"sqltest"
                                                                zoneCreated:true
                                                             zoneSubscribed:true
                                                                changeToken:[@"nonsense" dataUsingEncoding:NSUTF8StringEncoding]
                                                      moreRecordsInCloudKit:YES
                                                                  lastFetch:[NSDate date]
                                                                   lastScan:[NSDate date]
                                                                  lastFixup:CKKSCurrentFixupNumber
                                                         encodedRateLimiter:nil];
    zse.rateLimiter = [[CKKSRateLimiter alloc] init];

    CKKSZoneStateEntry* zse2 = [[CKKSZoneStateEntry alloc] initWithContextID:@"other_context"
                                                                    zoneName:@"sqltest"
                                                                 zoneCreated:true
                                                              zoneSubscribed:true
                                                                 changeToken:[@"other_nonsense" dataUsingEncoding:NSUTF8StringEncoding]
                                                       moreRecordsInCloudKit:NO
                                                                   lastFetch:[NSDate date]
                                                                    lastScan:[NSDate date]
                                                                   lastFixup:1
                                                          encodedRateLimiter:nil];

    CKKSZoneStateEntry* zseClone = [[CKKSZoneStateEntry alloc] initWithContextID:context1
                                                                        zoneName:@"sqltest"
                                                                     zoneCreated:true
                                                                  zoneSubscribed:true
                                                                     changeToken:[@"nonsense" dataUsingEncoding:NSUTF8StringEncoding]
                                                           moreRecordsInCloudKit:YES
                                                                       lastFetch:zse.lastFetchTime
                                                                        lastScan:zse.lastLocalKeychainScanTime
                                                                       lastFixup:CKKSCurrentFixupNumber
                                                              encodedRateLimiter:zse.encodedRateLimiter];

    CKKSZoneStateEntry* zseDifferent = [[CKKSZoneStateEntry alloc] initWithContextID:context1
                                                                            zoneName:@"sqltest"
                                                                         zoneCreated:true
                                                                      zoneSubscribed:true
                                                                         changeToken:[@"allnonsense" dataUsingEncoding:NSUTF8StringEncoding]
                                                               moreRecordsInCloudKit:NO
                                                                           lastFetch:zse.lastFetchTime
                                                                            lastScan:zse.lastLocalKeychainScanTime
                                                                           lastFixup:CKKSCurrentFixupNumber
                                                                  encodedRateLimiter:zse.encodedRateLimiter];
    XCTAssertEqualObjects(zse, zseClone, "CKKSZoneStateEntry isEqual of equal objects seems okay");
    XCTAssertNotEqualObjects(zse, zseDifferent, "CKKSZoneStateEntry isEqual of nonequal objects seems okay");

    NSError* error = nil;
    CKKSZoneStateEntry* loaded = [CKKSZoneStateEntry tryFromDatabase:context1 zoneName:@"sqltest" error:&error];
    XCTAssertNil(error, "No error trying to load nonexistent record");
    XCTAssertNil(loaded, "No record saved in database");

    [CKKSSQLDatabaseObject performCKKSTransaction:^CKKSDatabaseTransactionResult {
        NSError* saveError = nil;
        [zse saveToDatabase:&saveError];
        XCTAssertNil(saveError, "no error occurred saving CKKSZoneStateEntry to database");

        [zse2 saveToDatabase:&saveError];
        XCTAssertNil(saveError, "no error occurred saving CKKSZoneStateEntry to database");
        return CKKSDatabaseTransactionCommit;
    }];

    loaded = [CKKSZoneStateEntry tryFromDatabase:context1 zoneName:@"sqltest" error:&error];
    XCTAssertNil(error, "No error trying to load saved record");
    XCTAssertNotNil(loaded, "CKKSZoneStateEntry came back out of database");

    XCTAssertEqualObjects(zse.ckzone,             loaded.ckzone,              "ckzone persisted through db save and load");
    XCTAssertEqual       (zse.ckzonecreated,      loaded.ckzonecreated,       "ckzonecreated persisted through db save and load");
    XCTAssertEqual       (zse.ckzonesubscribed,   loaded.ckzonesubscribed,    "ckzonesubscribed persisted through db save and load");
    XCTAssertEqualObjects(zse.encodedChangeToken, loaded.encodedChangeToken, "encodedChangeToken persisted through db save and load");

    secnotice("ckkstests", "zse.lastFetchTime: %@", zse.lastFetchTime);
    secnotice("ckkstests", "loaded.lastFetchTime: %@", loaded.lastFetchTime);

    secnotice("ckkstests", "equal?: %d", [zse.lastFetchTime isEqualToDate:loaded.lastFetchTime]);
    secnotice("ckkstests", "equal to seconds?: %d", [[NSCalendar currentCalendar] isDate:zse.lastFetchTime equalToDate: loaded.lastFetchTime toUnitGranularity:NSCalendarUnitSecond]);

    // We only compare to the minute level, as that's enough to test the save+load.
    XCTAssert([[NSCalendar currentCalendar] isDate:zse.lastFetchTime equalToDate: loaded.lastFetchTime toUnitGranularity:NSCalendarUnitMinute],
                                                                   "lastFetchTime persisted through db save and load");
    XCTAssert([[NSCalendar currentCalendar] isDate:zse.lastLocalKeychainScanTime equalToDate:loaded.lastLocalKeychainScanTime toUnitGranularity:NSCalendarUnitMinute],
              "lastLocalKeychainScanTime persisted through db save and load");

    {
        NSError* otherContextError = nil;
        CKKSZoneStateEntry* loadedOtherContext = [CKKSZoneStateEntry tryFromDatabase:@"missing_context" zoneName:@"sqltest" error:&otherContextError];
        XCTAssertNil(loadedOtherContext, "Should be no zone state entry for missing context");
        XCTAssertNil(otherContextError, "Should be no error loading no item");
    }

    {
        NSError* otherContextError = nil;
        CKKSZoneStateEntry* loadedOtherContext = [CKKSZoneStateEntry tryFromDatabase:@"other_context" zoneName:@"sqltest" error:&otherContextError];
        XCTAssertNotNil(loadedOtherContext, "Should be an zone state entry for other contextID");
        XCTAssertNil(otherContextError, "Should be no error loading no item");

        XCTAssertEqualObjects(zse2.ckzone,             loadedOtherContext.ckzone,              "ckzone persisted through db save and load");
        XCTAssertEqual       (zse2.ckzonecreated,      loadedOtherContext.ckzonecreated,       "ckzonecreated persisted through db save and load");
        XCTAssertEqual       (zse2.ckzonesubscribed,   loadedOtherContext.ckzonesubscribed,    "ckzonesubscribed persisted through db save and load");
        XCTAssertEqualObjects(zse2.encodedChangeToken, loadedOtherContext.encodedChangeToken, "encodedChangeToken persisted through db save and load");

        XCTAssertNotEqualObjects(loadedOtherContext, loaded, "Should not match object from other context");
    }
}

-(void)testRoundtripCKKSDeviceStateEntry {
    // Very simple test: can these objects roundtrip through the db?
    NSString* testUUID = @"157A3171-0677-451B-9EAE-0DDC4D4315B0";
    CKKSDeviceStateEntry* cdse = [[CKKSDeviceStateEntry alloc] initForDevice:testUUID
                                                                   contextID:CKKSMockCloudKitContextID
                                                                   osVersion:@"faux-version"
                                                              lastUnlockTime:nil
                                                               octagonPeerID:@"peerID"
                                                               octagonStatus:nil
                                                                circlePeerID:@"asdf"
                                                                circleStatus:kSOSCCInCircle
                                                                    keyState:SecCKKSZoneKeyStateReady
                                                              currentTLKUUID:@"tlk"
                                                           currentClassAUUID:@"classA"
                                                           currentClassCUUID:@"classC"
                                                                      zoneID:self.testZoneID
                                                             encodedCKRecord:nil];
    XCTAssertNotNil(cdse, "Constructor works");
    [CKKSSQLDatabaseObject performCKKSTransaction:^CKKSDatabaseTransactionResult {
        NSError* saveError = nil;
        [cdse saveToDatabase:&saveError];
        XCTAssertNil(saveError, "no error occurred saving cdse to database");
        return CKKSDatabaseTransactionCommit;
    }];

    NSError* loadError = nil;
    CKKSDeviceStateEntry* loadedCDSE = [CKKSDeviceStateEntry fromDatabase:testUUID
                                                                contextID:CKKSMockCloudKitContextID
                                                                   zoneID:self.testZoneID
                                                                    error:&loadError];
    XCTAssertNil(loadError, "No error loading CDSE");
    XCTAssertNotNil(loadedCDSE, "Received a CDSE back");

    XCTAssertEqualObjects(cdse, loadedCDSE, "Roundtripping CKKSDeviceStateEntry ends up with equivalent objects");
}

// disabled, as CKKS syncing is disabled in this class.
// To re-enable, need to add flags CKKS syncing to perform queue actions but not automatically start queue processing operations
-(void)disabledtestItemAddCreatesCKKSOutgoingQueueEntry {
    CFMutableDictionaryRef		attrs;
    CFDataRef					data;

    NSError* error;

    NSArray* oqes = [CKKSOutgoingQueueEntry all: &error];
    XCTAssertEqual([oqes count], 0ul, @"Nothing in outgoing queue");
    XCTAssertNil(error, @"No error loading queue");

    attrs = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
    CFDictionarySetValue( attrs, kSecClass, kSecClassGenericPassword );
    CFDictionarySetValue( attrs, kSecAttrAccessible, kSecAttrAccessibleAlwaysPrivate );
    CFDictionarySetValue( attrs, kSecAttrLabel, CFSTR( "TestLabel" ) );
    CFDictionarySetValue( attrs, kSecAttrDescription, CFSTR( "TestDescription" ) );
    CFDictionarySetValue( attrs, kSecAttrAccount, CFSTR( "TestAccount" ) );
    CFDictionarySetValue( attrs, kSecAttrService, CFSTR( "TestService" ) );
    CFDictionarySetValue( attrs, kSecAttrAccessGroup, CFSTR("com.apple.lakitu"));
    data = CFDataCreate( NULL, (const uint8_t *) "important data", strlen("important data"));
    CFDictionarySetValue( attrs, kSecValueData, data );
    CFRelease( data );

    XCTAssertEqual(SecItemAdd(attrs, NULL), errSecSuccess, @"Adding item works flawlessly");

    oqes = [CKKSOutgoingQueueEntry all: &error];
    XCTAssertEqual([oqes count], 1ul, @"Single entry in outgoing queue after adding item");
    XCTAssertNil(error, @"No error loading queue");

    CFDictionarySetValue( attrs, kSecAttrLabel, CFSTR( "TestLabel2" ) );
    CFDictionarySetValue( attrs, kSecAttrAccount, CFSTR( "TestAccount2" ) );
    CFDictionarySetValue( attrs, kSecAttrService, CFSTR( "TestService2" ) );
    XCTAssertEqual(SecItemAdd(attrs, NULL), errSecSuccess);
    CFRelease( attrs );

    oqes = [CKKSOutgoingQueueEntry all: &error];
    XCTAssertEqual([oqes count], 2ul, @"Two entries in outgoing queue after adding item");
    XCTAssertNil(error, @"No error loading queue");
}

- (void)testCKKSKey {
    CKKSKey* key = nil;
    NSString* testUUID = @"157A3171-0677-451B-9EAE-0DDC4D4315B0";
    NSString* testParentUUID = @"f5e7f20f-0885-48f9-b75d-9f0cfd2171b6";

    NSData* testCKRecord = [@"nonsense" dataUsingEncoding:NSUTF8StringEncoding];

    CKKSWrappedAESSIVKey* wrappedkey = [[CKKSWrappedAESSIVKey alloc] initWithBase64:@"KFfL58XtugiYNoD859EjG0StfrYd6eakm0CQrgX7iO+DEo4kio3WbEeA1kctCU0GaeTGsRFpbdy4oo6jXhVu7cZqB0svhUPGq55aGnszUjI="];

    NSError* error = nil;

    key = [CKKSKey fromDatabase:testUUID
                      contextID:CKKSMockCloudKitContextID
                         zoneID:self.testZoneID
                          error:&error];

    XCTAssertNil(key, "key does not exist yet");
    XCTAssertNotNil(error, "error exists when things go wrong");
    error = nil;

    key = [[CKKSKey alloc] initWithWrappedKeyData:wrappedkey.wrappedData
                                        contextID:CKKSMockCloudKitContextID
                                             uuid:testUUID
                                    parentKeyUUID:testParentUUID
                                         keyclass:SecCKKSKeyClassA
                                            state:SecCKKSProcessedStateLocal
                                           zoneID:self.testZoneID
                                  encodedCKRecord:testCKRecord
                                       currentkey:true];
    XCTAssertNotNil(key, "could create key");

    CKKSKeychainBackedKey* kbkey = [key getKeychainBackedKey:&error];
    XCTAssertNotNil(kbkey, "Should be able to extract a CKKSKeychainBackedKey from a just-created CKKS key");
    XCTAssertNil(error, "Should be no error extracting a keychain-backed key");

    [CKKSSQLDatabaseObject performCKKSTransaction:^CKKSDatabaseTransactionResult {
        NSError* saveError = nil;
        [key saveToDatabase:&saveError];
        XCTAssertNil(saveError, "no error occurred saving key to database");
        return CKKSDatabaseTransactionCommit;
    }];
    error = nil;

    CKKSKey* key2 = [CKKSKey fromDatabase:testUUID
                                contextID:CKKSMockCloudKitContextID
                                   zoneID:self.testZoneID
                                    error:&error];

    XCTAssertNil(error, "no error exists when loading key");
    XCTAssertNotNil(key2, "key was fetched properly");

    CKKSKeychainBackedKey* kbkey2 = [key getKeychainBackedKey:&error];
    XCTAssertNotNil(kbkey2, "Should be able to extract a CKKSKeychainBackedKey from a just-loaded CKKS key");
    XCTAssertNil(error, "Should be no error extracting a keychain-backed key");

    XCTAssertEqualObjects(key.uuid, key2.uuid, "key uuids match");
    XCTAssertEqualObjects(key.parentKeyUUID, key2.parentKeyUUID, "parent key uuids match");
    XCTAssertEqualObjects(key.state, key2.state, "key states match");
    XCTAssertEqualObjects(key.encodedCKRecord, key2.encodedCKRecord, "encodedCKRecord match");
    XCTAssertEqualObjects(key.wrappedKeyData, key2.wrappedKeyData, "wrappedKeyData matches");
    XCTAssertEqual(key.currentkey, key2.currentkey, "currentkey match");

    XCTAssertEqualObjects(kbkey.wrappedkey, kbkey2.wrappedkey, "wrapped keys match");
    XCTAssertEqualObjects(kbkey, kbkey2, "keychain-backed keys match");


    [CKKSSQLDatabaseObject performCKKSTransaction:^CKKSDatabaseTransactionResult {
        NSError* deleteError = nil;
        bool result = [CKKSKey deleteAllWithContextID:CKKSMockCloudKitContextID
                                               zoneID:self.testZoneID
                                                error:&deleteError];
        XCTAssertTrue(result, "Should be success deleting all keys");
        XCTAssertNil(deleteError, "Should be no error deleting all keys");
        return CKKSDatabaseTransactionCommit;
    }];

    NSError* loadError = nil;
    CKKSKey* key3 = [CKKSKey fromDatabase:testUUID
                                contextID:CKKSMockCloudKitContextID
                                   zoneID:self.testZoneID
                                    error:&loadError];
    
    XCTAssertNil(key3, "Should have no item after deletion");
    XCTAssertNotNil(loadError, "Should have an error trying to load a deleted key");
}

- (void)testWhere {
    NSError* error = nil;

    NSData* testCKRecord = [@"nonsense" dataUsingEncoding:NSUTF8StringEncoding];

    CKKSKey* tlk = [[CKKSKey alloc] initSelfWrappedWithAESKey: [[CKKSAESSIVKey alloc] initWithBase64: @"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=="]
                                                    contextID:CKKSMockCloudKitContextID
                                                         uuid:@"8b2aeb7f-4af3-43e9-b6e6-70d5c728ebf7"
                                                     keyclass:SecCKKSKeyClassTLK
                                                        state: SecCKKSProcessedStateLocal
                                                       zoneID:self.testZoneID
                                              encodedCKRecord: testCKRecord
                                                   currentkey: true];

    [CKKSSQLDatabaseObject performCKKSTransaction:^CKKSDatabaseTransactionResult {
        NSError* saveError = nil;
        [tlk saveToDatabase:&saveError];
        XCTAssertNil(saveError, "no error occurred saving TLK to database");
        return CKKSDatabaseTransactionCommit;
    }];

    CKKSKey* wrappedKey = [CKKSKey randomKeyWrappedByParent:tlk keyclass:SecCKKSKeyClassC error:&error];
    // All CKKSKey objects must have some CK record associated with them
    wrappedKey.encodedCKRecord = testCKRecord;

    [CKKSSQLDatabaseObject performCKKSTransaction:^CKKSDatabaseTransactionResult {
        NSError* saveError = nil;
        [wrappedKey saveToDatabase:&saveError];
        XCTAssertNil(saveError, "no error occurred saving key to database");
        return CKKSDatabaseTransactionCommit;
    }];

    NSString* secondUUID = @"8b2aeb7f-0000-0000-0000-70d5c728ebf7";
    CKKSKey* secondtlk = [[CKKSKey alloc] initSelfWrappedWithAESKey:[[CKKSAESSIVKey alloc] initWithBase64: @"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=="]
                                                          contextID:CKKSMockCloudKitContextID
                                                               uuid:secondUUID
                                                           keyclass:SecCKKSKeyClassTLK
                                                              state:SecCKKSProcessedStateLocal
                                                             zoneID:self.testZoneID
                                                    encodedCKRecord:testCKRecord
                                                         currentkey:true];
    [CKKSSQLDatabaseObject performCKKSTransaction:^CKKSDatabaseTransactionResult {
        NSError* saveError = nil;
        XCTAssertTrue([secondtlk saveToDatabase:&saveError], "Second TLK saved to database");
        XCTAssertNil(saveError, "no error occurred saving second TLK to database");
        return CKKSDatabaseTransactionCommit;
    }];

    NSArray<CKKSKey*>* tlks = [CKKSKey allWhere: @{@"UUID": @"8b2aeb7f-4af3-43e9-b6e6-70d5c728ebf7"} error: &error];
    XCTAssertNotNil(tlks, "Returned some array from allWhere");
    XCTAssertNil(error, "no error back from allWhere");
    XCTAssertEqual([tlks count], 1ul, "Received one row (and expected one row)");

    NSArray<CKKSKey*>* selfWrapped = [CKKSKey allWhere: @{@"parentKeyUUID": [CKKSSQLWhereColumn op:CKKSSQLWhereComparatorEquals column:CKKSSQLWhereColumnNameUUID]} error: &error];
    XCTAssertNotNil(selfWrapped, "Returned some array from allWhere");
    XCTAssertNil(error, "no error back from allWhere");
    XCTAssertEqual([selfWrapped count], 2ul, "Should have recievied two rows");

    // Try using CKKSSQLWhereColumn alongside normal binds
    NSArray<CKKSKey*>* selfWrapped2 = [CKKSKey allWhere: @{@"parentKeyUUID": [CKKSSQLWhereColumn op:CKKSSQLWhereComparatorEquals column:CKKSSQLWhereColumnNameUUID],
                                                           @"uuid": @"8b2aeb7f-4af3-43e9-b6e6-70d5c728ebf7"}
                                                  error: &error];
    XCTAssertNotNil(selfWrapped2, "Returned some array from allWhere");
    XCTAssertNil(error, "no error back from allWhere");
    XCTAssertEqual([selfWrapped2 count], 1ul, "Received one row (and expected one row)");
    XCTAssertEqualObjects([selfWrapped2[0] uuid], @"8b2aeb7f-4af3-43e9-b6e6-70d5c728ebf7", "Should received first TLK UUID");

    NSArray<CKKSKey*>* selfWrapped3 = [CKKSKey allWhere: @{@"parentKeyUUID": [CKKSSQLWhereColumn op:CKKSSQLWhereComparatorEquals column:CKKSSQLWhereColumnNameUUID],
                                                           @"uuid": [CKKSSQLWhereValue op:CKKSSQLWhereComparatorNotEquals value:@"8b2aeb7f-4af3-43e9-b6e6-70d5c728ebf7"]}
                                                  error: &error];
    XCTAssertNotNil(selfWrapped3, "Returned some array from allWhere");
    XCTAssertNil(error, "no error back from allWhere");
    XCTAssertEqual([selfWrapped3 count], 1ul, "Should have received one rows");
    XCTAssertEqualObjects([selfWrapped3[0] uuid], secondUUID, "Should received second TLK UUID");

    NSArray<CKKSKey*>* whereFound = [CKKSKey allWhere: @{@"uuid": [[CKKSSQLWhereIn alloc] initWithValues:@[tlk.uuid, wrappedKey.uuid,  @"not-found"]]} error:&error];
    XCTAssertNil(error, "no error back from search");
    XCTAssertEqual([whereFound count], 2ul, "Should have received two rows");

    // Depending on the random UUID created for warppedKey, the two records may come back in either order
    if([tlk.uuid compare:wrappedKey.uuid] == NSOrderedAscending) {
        XCTAssertEqualObjects([whereFound[0] uuid], tlk.uuid, "Should received TLK UUID");
        XCTAssertEqualObjects([whereFound[1] uuid], wrappedKey.uuid, "Should received wrapped key UUID");
    } else {
        XCTAssertEqualObjects([whereFound[1] uuid], tlk.uuid, "Should received TLK UUID");
        XCTAssertEqualObjects([whereFound[0] uuid], wrappedKey.uuid, "Should received wrapped key UUID");
    }
}

- (void)testGroupBy {
    [self addTestZoneEntries];
    NSError* error = nil;

    __block NSMutableDictionary<NSString*, NSString*>* results = [[NSMutableDictionary alloc] init];
    NSDictionary* expectedResults = nil;

    [CKKSSQLDatabaseObject queryDatabaseTable: [CKKSOutgoingQueueEntry sqlTable]
                                        where: nil
                                      columns: @[@"action", @"count(rowid)"]
                                      groupBy: @[@"action"]
                                      orderBy:nil
                                        limit: -1
                                   processRow: ^(NSDictionary<NSString*, CKKSSQLResult*>* row) {
                                       results[row[@"action"].asString] = row[@"count(rowid)"].asString;
                                   }
                                        error: &error];

    XCTAssertNil(error, "no error doing group by query");
    expectedResults = @{
                      SecCKKSActionAdd: @"2",
                      SecCKKSActionModify: @"1"
                      };
    XCTAssertEqualObjects(results, expectedResults, "Recieved correct group by result");

    // Now test with a where clause:
    results = [[NSMutableDictionary alloc] init];
    [CKKSSQLDatabaseObject queryDatabaseTable: [CKKSOutgoingQueueEntry sqlTable]
                                        where: @{@"state": SecCKKSStateError}
                                      columns: @[@"action", @"count(rowid)"]
                                      groupBy: @[@"action"]
                                      orderBy:nil
                                        limit: -1
                                   processRow: ^(NSDictionary<NSString*, CKKSSQLResult*>* row) {
                                       results[row[@"action"].asString] = row[@"count(rowid)"].asString;
                                   }
                                        error: &error];

    XCTAssertNil(error, "no error doing where+group by query");
    expectedResults = @{
                      SecCKKSActionAdd: @"1",
                      SecCKKSActionModify: @"1"
                      };
    XCTAssertEqualObjects(results, expectedResults, "Recieved correct where+group by result");
}

- (void)testOrderBy {
    [self addTestZoneEntries];
    NSError* error = nil;

    __block NSMutableArray<NSDictionary<NSString*, CKKSSQLResult*>*>* rows = [[NSMutableArray alloc] init];

    [CKKSSQLDatabaseObject queryDatabaseTable: [CKKSOutgoingQueueEntry sqlTable]
                                        where: nil
                                      columns: @[@"action", @"uuid"]
                                      groupBy:nil
                                      orderBy:@[@"uuid"]
                                        limit:-1
                                   processRow:^(NSDictionary<NSString*, CKKSSQLResult*>* row) {
                                       [rows addObject:row];
                                   }
                                        error: &error];

    XCTAssertNil(error, "no error doing order by query");
    XCTAssertEqual(rows.count, 3u, "got three items");

    XCTAssertEqual([rows[0][@"uuid"].asString compare: rows[1][@"uuid"].asString], NSOrderedAscending, "first order is fine");
    XCTAssertEqual([rows[1][@"uuid"].asString compare: rows[2][@"uuid"].asString], NSOrderedAscending, "second order is fine");

    // Check that order-by + limit works to page
    __block NSString* lastUUID = nil;
    __block NSString* uuid = nil;
    uint64_t count = 0;

    while(count == 0 || uuid != nil) {
        uuid = nil;
        [CKKSSQLDatabaseObject queryDatabaseTable: [CKKSOutgoingQueueEntry sqlTable]
                                            where: lastUUID ? @{@"UUID": [CKKSSQLWhereValue op:CKKSSQLWhereComparatorGreaterThan value:lastUUID]} : nil
                                          columns: @[@"action", @"UUID"]
                                          groupBy:nil
                                          orderBy:@[@"uuid"]
                                            limit:1
                                       processRow: ^(NSDictionary<NSString*, CKKSSQLResult*>* row) {
                                           XCTAssertNil(uuid, "Only one row returned");
                                           uuid = row[@"UUID"].asString;
                                       }
                                            error: &error];
        XCTAssertNil(error, "No error doing SQL");
        if(uuid && lastUUID) {
            XCTAssertEqual([lastUUID compare:uuid], NSOrderedAscending, "uuids returning in right order");
        }
        lastUUID = uuid;
        count += 1;
    }
    XCTAssertEqual(count, 4u, "Received 3 objects (and 1 nil)");
}

- (void)testLimit {
    [self addTestZoneEntries];
    NSError* error = nil;

    __block NSMutableDictionary<NSString*, NSString*>* results = [[NSMutableDictionary alloc] init];

    [CKKSSQLDatabaseObject queryDatabaseTable: [CKKSOutgoingQueueEntry sqlTable]
                                        where: nil
                                      columns: @[@"uuid", @"action"]
                                      groupBy: nil
                                      orderBy:nil
                                        limit: -1
                                   processRow: ^(NSDictionary<NSString*, CKKSSQLResult*>* row) {
                                       results[row[@"uuid"].asString] = row[@"action"].asString;
                                   }
                                        error: &error];

    XCTAssertNil(error, "no error doing vanilla query");
    XCTAssertEqual(results.count, 3u, "Received three elements in normal query");
    results = [[NSMutableDictionary alloc] init];

    [CKKSSQLDatabaseObject queryDatabaseTable: [CKKSOutgoingQueueEntry sqlTable]
                                        where: nil
                                      columns: @[@"uuid", @"action"]
                                      groupBy: nil
                                      orderBy:nil
                                        limit: 1
                                   processRow: ^(NSDictionary<NSString*, CKKSSQLResult*>* row) {
                                       results[row[@"uuid"].asString] = row[@"action"].asString;
                                   }
                                        error: &error];

    XCTAssertNil(error, "no error doing limit query");
    XCTAssertEqual(results.count, 1u, "Received one element in limited query");
    results = [[NSMutableDictionary alloc] init];

    // Now test with a where clause:
    results = [[NSMutableDictionary alloc] init];
    [CKKSSQLDatabaseObject queryDatabaseTable: [CKKSOutgoingQueueEntry sqlTable]
                                        where: @{@"state": SecCKKSStateError}
                                      columns: @[@"uuid", @"action"]
                                      groupBy: nil
                                      orderBy:nil
                                        limit: 3
                                   processRow: ^(NSDictionary<NSString*, CKKSSQLResult*>* row) {
                                       results[row[@"uuid"].asString] = row[@"action"].asString;
                                   }
                                        error: &error];

    XCTAssertNil(error, "no error doing limit+where query");
    XCTAssertEqual(results.count, 2u, "Received two elements in where+limited query");
    results = [[NSMutableDictionary alloc] init];

    results = [[NSMutableDictionary alloc] init];
    [CKKSSQLDatabaseObject queryDatabaseTable: [CKKSOutgoingQueueEntry sqlTable]
                                        where: @{@"state": SecCKKSStateError}
                                      columns: @[@"uuid", @"action"]
                                      groupBy: nil
                                      orderBy:nil
                                        limit: 1
                                   processRow: ^(NSDictionary<NSString*, CKKSSQLResult*>* row) {
                                       results[row[@"uuid"].asString] = row[@"action"].asString;
                                   }
                                        error: &error];

    XCTAssertNil(error, "no error doing limit+where query");
    XCTAssertEqual(results.count, 1u, "Received one element in where+limited query");
    results = [[NSMutableDictionary alloc] init];
}

- (void)testQuoting {
    XCTAssertEqualObjects([CKKSSQLDatabaseObject quotedString:@"hej"], @"hej", "no quote");
    XCTAssertEqualObjects([CKKSSQLDatabaseObject quotedString:@"hej'"], @"hej''", "single quote");
    XCTAssertEqualObjects([CKKSSQLDatabaseObject quotedString:@"'hej'"], @"''hej''", "two single quote");
    XCTAssertEqualObjects([CKKSSQLDatabaseObject quotedString:@"hej\""], @"hej\"", "double quote");
    XCTAssertEqualObjects([CKKSSQLDatabaseObject quotedString:@"\"hej\""], @"\"hej\"", "double quote");
    XCTAssertEqualObjects([CKKSSQLDatabaseObject quotedString:@"'\"hej\""], @"''\"hej\"", "double quote");
}


@end

#endif
