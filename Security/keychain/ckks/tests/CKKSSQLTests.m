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
#import "keychain/ckks/CKKSOutgoingQueueEntry.h"
#import "keychain/ckks/CKKSZoneStateEntry.h"
#import "keychain/ckks/CKKSDeviceStateEntry.h"
#import "keychain/ckks/CKKSRateLimiter.h"

#include <securityd/SecItemServer.h>

@interface CloudKitKeychainSQLTests : CloudKitMockXCTest
@end

@implementation CloudKitKeychainSQLTests

+ (void)setUp {
    [super setUp];
}

- (void)setUp {
    SecCKKSDisable();
    [super setUp];
}

- (void)tearDown {
    [super tearDown];
    SecCKKSResetSyncing();
}

- (void)addTestZoneEntries {
    CKKSOutgoingQueueEntry* one = [[CKKSOutgoingQueueEntry alloc] initWithCKKSItem:
                                   [[CKKSItem alloc] initWithUUID:[[NSUUID UUID] UUIDString]
                                                    parentKeyUUID:[[NSUUID UUID] UUIDString]
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
                                                           zoneID:self.testZoneID
                                                          encItem:[@"nonsense" dataUsingEncoding:NSUTF8StringEncoding]
                                                       wrappedkey:[[CKKSWrappedAESSIVKey alloc]initWithBase64: @"KFfL58XtugiYNoD859EjG0StfrYd6eakm0CQrgX7iO+DEo4kio3WbEeA1kctCU0GaeTGsRFpbdy4oo6jXhVu7cZqB0svhUPGq55aGnszUjI="]
                                                  generationCount:0
                                                           encver:0]
                                                                            action:SecCKKSActionModify
                                                                             state:SecCKKSStateError
                                                                         waitUntil:nil
                                                                       accessGroup:@"nope"];

    NSError* error = nil;
    [one saveToDatabase:&error];
    [two saveToDatabase: &error];
    [three saveToDatabase: &error];
    XCTAssertNil(error, "no error saving ZoneStateEntries to database");
}

- (void)testCKKSOutgoingQueueEntry {
    NSString* testUUID = @"157A3171-0677-451B-9EAE-0DDC4D4315B0";
    NSUUID* testKeyUUID = [[NSUUID alloc] init];

    NSError * nserror;
    __block CFErrorRef error = NULL;

    CKKSOutgoingQueueEntry* shouldFail = [CKKSOutgoingQueueEntry fromDatabase:testUUID state:SecCKKSStateInFlight zoneID:self.testZoneID error: &nserror];
    XCTAssertNil(shouldFail, "Can't find a nonexisting object");
    XCTAssertNotNil(nserror, "NSError exists when things break");

    __weak __typeof(self) weakSelf = self;
    kc_with_dbt(true, &error, ^bool (SecDbConnectionRef dbconn) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        XCTAssertNotNil(strongSelf, "called while self still exists");

        NSString * sql = @"insert INTO outgoingqueue (UUID, parentKeyUUID, ckzone, action, state, accessgroup, gencount, encitem, wrappedkey, encver) VALUES (?,?,?,?,?,?,?,?,?,?);";
        SecDbPrepare(dbconn, (__bridge CFStringRef) sql, &error, ^void (sqlite3_stmt *stmt) {
            SecDbBindText(stmt, 1, [testUUID UTF8String], strlen([testUUID UTF8String]), NULL, &error);
            SecDbBindText(stmt, 2, [[testKeyUUID UUIDString] UTF8String], strlen([[testKeyUUID UUIDString] UTF8String]), NULL, &error);
            SecDbBindObject(stmt, 3, (__bridge CFStringRef) weakSelf.testZoneID.zoneName, &error);
            SecDbBindText(stmt, 4, "newitem", strlen("newitem"), NULL, &error);
            SecDbBindText(stmt, 5, "unprocessed", strlen("unprocessed"), NULL, &error);
            SecDbBindText(stmt, 6, "com.apple.access", strlen("com.apple.access"), NULL, &error);
            SecDbBindText(stmt, 7, "0", strlen("0"), NULL, &error);
            SecDbBindText(stmt, 8, "bm9uc2Vuc2UK", strlen("bm9uc2Vuc2UK"), NULL, &error);
            SecDbBindObject(stmt, 9, CFSTR("KFfL58XtugiYNoD859EjG0StfrYd6eakm0CQrgX7iO+DEo4kio3WbEeA1kctCU0GaeTGsRFpbdy4oo6jXhVu7cZqB0svhUPGq55aGnszUjI="), &error);
            SecDbBindText(stmt, 10, "0", strlen("0"), NULL, &error);

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
    [other saveToDatabase:&nserror];
    XCTAssertNil(nserror, "no error occurred saving to database");

    CKKSOutgoingQueueEntry * oqe = [CKKSOutgoingQueueEntry fromDatabase:testUUID state:@"unprocessed" zoneID:self.testZoneID error: &nserror];
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

    XCTAssertTrue([oqe saveToDatabase: &nserror], "saving to database");

    CKKSOutgoingQueueEntry * oqe2 = [CKKSOutgoingQueueEntry fromDatabase:testUUID state:@"savedtocloud" zoneID:self.testZoneID error: &nserror];
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
    [oqe2 deleteFromDatabase:&nserror];
    XCTAssertNil(nserror, "No NSError exists when deleting existing item");
    oqe2 = [CKKSOutgoingQueueEntry fromDatabase:testUUID state:@"savedtocloud" zoneID:self.testZoneID error: &nserror];
    XCTAssertNil(oqe2, "Can't find a nonexisting object");
    XCTAssertNotNil(nserror, "NSError exists when things break");

    // Test loading other
    nserror = nil;
    CKKSOutgoingQueueEntry* other2 = [CKKSOutgoingQueueEntry fromDatabase: other.item.uuid state:SecCKKSStateError zoneID:self.testZoneID error:&nserror];
    XCTAssertNil(nserror, "No error loading other2 from database");
    XCTAssertNotNil(other2, "Able to re-load other.");
    XCTAssertEqualObjects(other, other2, "loaded object is equal to object");
}

-(void)testCKKSZoneStateEntrySQL {
    CKKSZoneStateEntry* zse = [[CKKSZoneStateEntry alloc] initWithCKZone:@"sqltest"
                                                             zoneCreated:true
                                                          zoneSubscribed:true
                                                             changeToken:[@"nonsense" dataUsingEncoding:NSUTF8StringEncoding]
                                                               lastFetch:[NSDate date]
                                                               lastFixup:CKKSCurrentFixupNumber
                                                      encodedRateLimiter:nil];
    zse.rateLimiter = [[CKKSRateLimiter alloc] init];

    CKKSZoneStateEntry* zseClone = [[CKKSZoneStateEntry alloc] initWithCKZone:@"sqltest"
                                                                  zoneCreated:true
                                                               zoneSubscribed:true
                                                                  changeToken:[@"nonsense" dataUsingEncoding:NSUTF8StringEncoding]
                                                                    lastFetch:zse.lastFetchTime
                                                                    lastFixup:CKKSCurrentFixupNumber
                                                           encodedRateLimiter:zse.encodedRateLimiter];

    CKKSZoneStateEntry* zseDifferent = [[CKKSZoneStateEntry alloc] initWithCKZone:@"sqltest"
                                                                      zoneCreated:true
                                                                   zoneSubscribed:true
                                                                      changeToken:[@"allnonsense" dataUsingEncoding:NSUTF8StringEncoding]
                                                                        lastFetch:zse.lastFetchTime
                                                                        lastFixup:CKKSCurrentFixupNumber
                                                               encodedRateLimiter:zse.encodedRateLimiter];
    XCTAssertEqualObjects(zse, zseClone, "CKKSZoneStateEntry isEqual of equal objects seems sane");
    XCTAssertNotEqualObjects(zse, zseDifferent, "CKKSZoneStateEntry isEqual of nonequal objects seems sane");

    NSError* error = nil;
    CKKSZoneStateEntry* loaded = [CKKSZoneStateEntry tryFromDatabase: @"sqltest" error:&error];
    XCTAssertNil(error, "No error trying to load nonexistent record");
    XCTAssertNil(loaded, "No record saved in database");

    [zse saveToDatabase: &error];
    XCTAssertNil(error, "no error saving CKKSZoneStateEntry to database");

    loaded = [CKKSZoneStateEntry tryFromDatabase: @"sqltest" error:&error];
    XCTAssertNil(error, "No error trying to load saved record");
    XCTAssertNotNil(loaded, "CKKSZoneStateEntry came back out of database");

    XCTAssertEqualObjects(zse.ckzone,             loaded.ckzone,              "ckzone persisted through db save and load");
    XCTAssertEqual       (zse.ckzonecreated,      loaded.ckzonecreated,       "ckzonecreated persisted through db save and load");
    XCTAssertEqual       (zse.ckzonesubscribed,   loaded.ckzonesubscribed,    "ckzonesubscribed persisted through db save and load");
    XCTAssertEqualObjects(zse.encodedChangeToken, loaded.encodedChangeToken, "encodedChangeToken persisted through db save and load");

    XCTAssert([[NSCalendar currentCalendar] isDate:zse.lastFetchTime equalToDate: loaded.lastFetchTime toUnitGranularity:NSCalendarUnitSecond],
                                                                   "lastFetchTime persisted through db save and load");
}

-(void)testRoundtripCKKSDeviceStateEntry {
    // Very simple test: can these objects roundtrip through the db?
    NSString* testUUID = @"157A3171-0677-451B-9EAE-0DDC4D4315B0";
    CKKSDeviceStateEntry* cdse = [[CKKSDeviceStateEntry alloc] initForDevice:testUUID
                                                                   osVersion:@"faux-version"
                                                              lastUnlockTime:nil
                                                                circlePeerID:@"asdf"
                                                                circleStatus:kSOSCCInCircle
                                                                    keyState:SecCKKSZoneKeyStateReady
                                                              currentTLKUUID:@"tlk"
                                                           currentClassAUUID:@"classA"
                                                           currentClassCUUID:@"classC"
                                                                      zoneID:self.testZoneID
                                                             encodedCKRecord:nil];
    XCTAssertNotNil(cdse, "Constructor works");
    NSError* saveError = nil;
    [cdse saveToDatabase:&saveError];
    XCTAssertNil(saveError, "No error saving cdse to database");

    NSError* loadError = nil;
    CKKSDeviceStateEntry* loadedCDSE = [CKKSDeviceStateEntry fromDatabase:testUUID zoneID:self.testZoneID error:&loadError];
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

    key = [CKKSKey fromDatabase:testUUID zoneID:self.testZoneID error:&error];
    XCTAssertNil(key, "key does not exist yet");
    XCTAssertNotNil(error, "error exists when things go wrong");
    error = nil;

    key = [[CKKSKey alloc] initWithWrappedAESKey: wrappedkey
                                            uuid: testUUID
                                   parentKeyUUID:testParentUUID
                                        keyclass:SecCKKSKeyClassA
                                           state: SecCKKSProcessedStateLocal
                                          zoneID:self.testZoneID
                                 encodedCKRecord:testCKRecord
                                      currentkey:true];
    XCTAssertNotNil(key, "could create key");

    [key saveToDatabase: &error];
    XCTAssertNil(error, "could save key to database");
    error = nil;

    CKKSKey* key2 = [CKKSKey fromDatabase:testUUID zoneID:self.testZoneID error:&error];
    XCTAssertNil(error, "no error exists when loading key");
    XCTAssertNotNil(key2, "key was fetched properly");

    XCTAssertEqualObjects(key.uuid, key2.uuid, "key uuids match");
    XCTAssertEqualObjects(key.parentKeyUUID, key2.parentKeyUUID, "parent key uuids match");
    XCTAssertEqualObjects(key.state, key2.state, "key states match");
    XCTAssertEqualObjects(key.encodedCKRecord, key2.encodedCKRecord, "encodedCKRecord match");
    XCTAssertEqualObjects(key.wrappedkey, key2.wrappedkey, "wrapped keys match");
    XCTAssertEqual(key.currentkey, key2.currentkey, "currentkey match");
}

- (void)testWhere {
    NSError* error = nil;

    NSData* testCKRecord = [@"nonsense" dataUsingEncoding:NSUTF8StringEncoding];

    CKKSKey* tlk = [[CKKSKey alloc] initSelfWrappedWithAESKey: [[CKKSAESSIVKey alloc] initWithBase64: @"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=="]
                                                         uuid:@"8b2aeb7f-4af3-43e9-b6e6-70d5c728ebf7"
                                                     keyclass:SecCKKSKeyClassTLK
                                                        state: SecCKKSProcessedStateLocal
                                                       zoneID:self.testZoneID
                                              encodedCKRecord: testCKRecord
                                                   currentkey: true];
    XCTAssertTrue([tlk saveToDatabase: &error], "TLK saved to database");
    XCTAssertNil(error, "no error saving TLK to database");

    CKKSKey* wrappedKey = [[CKKSKey alloc] initWrappedBy: tlk
                                                  AESKey:[CKKSAESSIVKey randomKey]
                                                    uuid:@"157A3171-0677-451B-9EAE-0DDC4D4315B0"
                                                keyclass:SecCKKSKeyClassC
                                                   state: SecCKKSProcessedStateLocal
                                                  zoneID:self.testZoneID
                                         encodedCKRecord:testCKRecord
                                              currentkey:true];
    XCTAssertTrue([wrappedKey saveToDatabase: &error], "key saved to database");
    XCTAssertNil(error, "no error saving key to database");

    NSArray<CKKSKey*>* tlks = [CKKSKey allWhere: @{@"UUID": @"8b2aeb7f-4af3-43e9-b6e6-70d5c728ebf7"} error: &error];
    XCTAssertNotNil(tlks, "Returned some array from allWhere");
    XCTAssertNil(error, "no error back from allWhere");
    XCTAssertEqual([tlks count], 1ul, "Received one row (and expected one row)");

    NSArray<CKKSKey*>* selfWrapped = [CKKSKey allWhere: @{@"parentKeyUUID": [CKKSSQLWhereObject op:@"=" string:@"uuid"]} error: &error];
    XCTAssertNotNil(selfWrapped, "Returned some array from allWhere");
    XCTAssertNil(error, "no error back from allWhere");
    XCTAssertEqual([selfWrapped count], 1ul, "Received one row (and expected one row)");

    // Try using CKKSSQLWhereObject alongside normal binds
    NSArray<CKKSKey*>* selfWrapped2 = [CKKSKey allWhere: @{@"parentKeyUUID": [CKKSSQLWhereObject op:@"=" string:@"uuid"],
                                                           @"uuid": @"8b2aeb7f-4af3-43e9-b6e6-70d5c728ebf7"}
                                                  error: &error];
    XCTAssertNotNil(selfWrapped2, "Returned some array from allWhere");
    XCTAssertNil(error, "no error back from allWhere");
    XCTAssertEqual([selfWrapped2 count], 1ul, "Received one row (and expected one row)");
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
                                   processRow: ^(NSDictionary* row) {
                                       results[row[@"action"]] = row[@"count(rowid)"];
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
                                   processRow: ^(NSDictionary* row) {
                                       results[row[@"action"]] = row[@"count(rowid)"];
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

    __block NSMutableArray* rows = [[NSMutableArray alloc] init];

    [CKKSSQLDatabaseObject queryDatabaseTable: [CKKSOutgoingQueueEntry sqlTable]
                                        where: nil
                                      columns: @[@"action", @"uuid"]
                                      groupBy:nil
                                      orderBy:@[@"uuid"]
                                        limit:-1
                                   processRow: ^(NSDictionary* row) {
                                       [rows addObject:row];
                                   }
                                        error: &error];

    XCTAssertNil(error, "no error doing order by query");
    XCTAssertEqual(rows.count, 3u, "got three items");

    XCTAssertEqual([rows[0][@"uuid"] compare: rows[1][@"uuid"]], NSOrderedAscending, "first order is fine");
    XCTAssertEqual([rows[1][@"uuid"] compare: rows[2][@"uuid"]], NSOrderedAscending, "second order is fine");

    // Check that order-by + limit works to page
    __block NSString* lastUUID = nil;
    __block NSString* uuid = nil;
    uint64_t count = 0;

    while(count == 0 || uuid != nil) {
        uuid = nil;
        [CKKSSQLDatabaseObject queryDatabaseTable: [CKKSOutgoingQueueEntry sqlTable]
                                            where: lastUUID ? @{@"UUID": [CKKSSQLWhereObject op:@">" stringValue:lastUUID]} : nil
                                          columns: @[@"action", @"UUID"]
                                          groupBy:nil
                                          orderBy:@[@"uuid"]
                                            limit:1
                                       processRow: ^(NSDictionary* row) {
                                           XCTAssertNil(uuid, "Only one row returned");
                                           uuid = row[@"UUID"];
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
                                   processRow: ^(NSDictionary* row) {
                                       results[row[@"uuid"]] = row[@"action"];
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
                                   processRow: ^(NSDictionary* row) {
                                       results[row[@"uuid"]] = row[@"action"];
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
                                   processRow: ^(NSDictionary* row) {
                                       results[row[@"uuid"]] = row[@"action"];
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
                                   processRow: ^(NSDictionary* row) {
                                       results[row[@"uuid"]] = row[@"action"];
                                   }
                                        error: &error];

    XCTAssertNil(error, "no error doing limit+where query");
    XCTAssertEqual(results.count, 1u, "Received one element in where+limited query");
    results = [[NSMutableDictionary alloc] init];
}

@end

#endif
