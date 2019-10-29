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

#import <XCTest/XCTest.h>
#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>

#import <Security/SecureObjectSync/SOSViews.h>
#import <utilities/SecFileLocations.h>
#import "keychain/securityd/SecItemServer.h"
#if NO_SERVER
#include "keychain/securityd/spi.h"
#endif

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSCurrentKeyPointer.h"
#import "keychain/ckks/CKKSMirrorEntry.h"
#import "keychain/ckks/CKKSItemEncrypter.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/ckks/tests/MockCloudKit.h"

@interface CKKSCloudKitTests : XCTestCase

@property NSOperationQueue *operationQueue;
@property CKContainer *container;
@property CKDatabase *database;
@property CKKSKeychainView *kcv;
@property NSString *zoneName;
@property CKRecordZoneID *zoneID;
@property NSDictionary *remoteItems;
@property NSInteger queueTimeout;

@end

// TODO: item modification should up gencount, check this

@implementation CKKSCloudKitTests

#if OCTAGON

#pragma mark Setup

+ (void)setUp {
    SecCKKSResetSyncing();
    SecCKKSTestsEnable();
    SecCKKSSetReduceRateLimiting(true);
    [super setUp];

#if NO_SERVER
    securityd_init_local_spi();
#endif
}

- (void)setUp {
    self.remoteItems = nil;
    self.queueTimeout = 900;    // CloudKit can be *very* slow, and some tests upload a lot of items indeed
    NSString *containerName = [NSString stringWithFormat:@"com.apple.test.p01.B.com.apple.security.keychain.%@", [[NSUUID new] UUIDString]];
    self.container = [CKContainer containerWithIdentifier:containerName];

    SecCKKSTestResetFlags();
    SecCKKSTestSetDisableSOS(true);

    self.operationQueue = [NSOperationQueue new];

    CKKSCloudKitClassDependencies* cloudKitClassDependencies = [[CKKSCloudKitClassDependencies alloc] initWithFetchRecordZoneChangesOperationClass:[CKFetchRecordZoneChangesOperation class]
                                                                                                                        fetchRecordsOperationClass:[CKFetchRecordsOperation class]
                                                                                                                               queryOperationClass:[CKQueryOperation class]
                                                                                                                 modifySubscriptionsOperationClass:[CKModifySubscriptionsOperation class]
                                                                                                                   modifyRecordZonesOperationClass:[CKModifyRecordZonesOperation class]
                                                                                                                                apsConnectionClass:[APSConnection class]
                                                                                                                         nsnotificationCenterClass:[NSNotificationCenter class]
                                                                                                              nsdistributednotificationCenterClass:[NSDistributedNotificationCenter class]
                                                                                                                                     notifierClass:[FakeCKKSNotifier class]];

    CKKSViewManager* manager = [[CKKSViewManager alloc] initWithContainerName:containerName
                                                                       usePCS:SecCKKSContainerUsePCS
                                                                   sosAdapter:nil
                                                    cloudKitClassDependencies:cloudKitClassDependencies];
    [CKKSViewManager resetManager:false setTo:manager];

    // Make a new fake keychain
    NSString* smallName = [self.name componentsSeparatedByString:@" "][1];
    smallName = [smallName stringByReplacingOccurrencesOfString:@"]" withString:@""];

    NSString* tmp_dir = [NSString stringWithFormat: @"/tmp/%@.%X", smallName, arc4random()];
    [[NSFileManager defaultManager] createDirectoryAtPath:[NSString stringWithFormat: @"%@/Library/Keychains", tmp_dir] withIntermediateDirectories:YES attributes:nil error:NULL];

    SetCustomHomeURLString((__bridge CFStringRef) tmp_dir);
    SecKeychainDbReset(NULL);
    // Actually load the database.
    kc_with_dbt(true, NULL, ^bool (SecDbConnectionRef dbt) { return false; });

    self.zoneName = @"keychain";
    self.zoneID = [[CKRecordZoneID alloc] initWithZoneName:self.zoneName ownerName:CKCurrentUserDefaultName];
    self.kcv = [[CKKSViewManager manager] findOrCreateView:@"keychain"];
}

- (void)tearDown {
    self.remoteItems = nil;
    [[CKKSViewManager manager] clearView:@"keychain"];
    SecCKKSTestResetFlags();
}

+ (void)tearDown {
    SecCKKSResetSyncing();
}

#pragma mark Helpers

- (BOOL)waitForEmptyOutgoingQueue:(CKKSKeychainView *)view {
    [view processOutgoingQueue:[CKOperationGroup CKKSGroupWithName:@"waitForEmptyOutgoingQueue"]];
    NSInteger secondsToWait = self.queueTimeout;
    while (true) {
        if ([view outgoingQueueEmpty:nil]) {
            return YES;
        }
        [NSThread sleepForTimeInterval:1];
        if (--secondsToWait % 60 == 0) {
            long minutesWaited = (self.queueTimeout - secondsToWait)/60;
            NSLog(@"Waiting %ld minute%@ for empty outgoingQueue", minutesWaited, minutesWaited > 1 ? @"s" : @"");
        }
        if (secondsToWait <= 0) {
            XCTFail(@"Timed out waiting for '%@' OutgoingQueue to become empty", view);
            NSLog(@"Giving up waiting for empty outgoingQueue");
            return NO;
        }

    }
}

- (void)startCKKSSubsystem {
    // TODO: we removed this mechanism, but haven't tested to see if these tests still succeed
}

- (NSMutableDictionary *)fetchLocalItems {
    NSDictionary *query = @{(id)kSecClass               : (id)kSecClassGenericPassword,
                            (id)kSecAttrSynchronizable  : (id)kCFBooleanTrue,
                            (id)kSecReturnAttributes    : (id)kCFBooleanTrue,
                            (id)kSecReturnData          : (id)kCFBooleanTrue,
                            (id)kSecMatchLimit          : (id)kSecMatchLimitAll,
                            };
    CFTypeRef cfresults;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef) query, &cfresults);
    XCTAssert(status == errSecSuccess || status == errSecItemNotFound, @"retrieved zero or more local items");
    if (status == errSecItemNotFound) {
        return [NSMutableDictionary new];
    } else if (status != errSecSuccess) {
        return nil;
    }

    NSArray *results = CFBridgingRelease(cfresults);

    NSMutableDictionary *ret = [NSMutableDictionary new];
    for (NSMutableDictionary *item in results) {
        ret[item[@"UUID"]] = item;
    }

    return ret;
}

- (NSMutableDictionary *)fetchRemoteItems {
    CKFetchRecordZoneChangesConfiguration *options = [CKFetchRecordZoneChangesConfiguration new];
    options.previousServerChangeToken = nil;

    CKFetchRecordZoneChangesOperation *op = [[CKFetchRecordZoneChangesOperation alloc] initWithRecordZoneIDs:@[self.zoneID] configurationsByRecordZoneID:@{self.zoneID : options}];
    op.configuration.automaticallyRetryNetworkFailures = NO;
    op.configuration.discretionaryNetworkBehavior = CKOperationDiscretionaryNetworkBehaviorNonDiscretionary;
    op.configuration.isCloudKitSupportOperation = YES;
    op.configuration.container = self.container;

    __block NSMutableDictionary *data = [NSMutableDictionary new];
    __block NSUInteger synckeys = 0;
    __block NSUInteger currkeys = 0;
    op.recordChangedBlock = ^(CKRecord *record) {
        if ([record.recordType isEqualToString:SecCKRecordItemType]) {
            data[record.recordID.recordName] = [self decryptRecord:record];
        } else if ([record.recordType isEqualToString:SecCKRecordIntermediateKeyType]) {
            synckeys += 1;
        } else if ([record.recordType isEqualToString:SecCKRecordCurrentKeyType]) {
            currkeys += 1;
        } else {
            XCTFail(@"Encountered unexpected item %@", record);
        }
    };

    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    op.recordZoneFetchCompletionBlock = ^(CKRecordZoneID * _Nonnull recordZoneID,
                                          CKServerChangeToken * _Nullable serverChangeToken,
                                          NSData * _Nullable clientChangeTokenData,
                                          BOOL moreComing,
                                          NSError * _Nullable recordZoneError) {
        XCTAssertNil(recordZoneError, @"No error in recordZoneFetchCompletionBlock");
        if (!moreComing) {
            dispatch_semaphore_signal(sema);
        }
    };
    
    [op start];
    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
    
    // These are new, fresh zones for each test. There should not be old keys yet.
    if (synckeys != 3) {XCTFail(@"Unexpected number of synckeys: %lu", (unsigned long)synckeys);}
    if (currkeys != 3) {XCTFail(@"Unexpected number of current keys: %lu", (unsigned long)currkeys);}

    self.remoteItems = data;
    return data;
}

- (BOOL)compareLocalItem:(NSDictionary *)lhs remote:(NSDictionary *)rhs {
    if ([lhs[@"cdat"] compare: rhs[@"cdat"]] != NSOrderedSame) {XCTFail(@"Creation date differs"); return NO;}
    if ([lhs[@"mdat"] compare: rhs[@"mdat"]] != NSOrderedSame) {XCTFail(@"Modification date differs"); return NO;}
    if (![lhs[@"agrp"] isEqualToString:rhs[@"agrp"]]) {XCTFail(@"Access group differs"); return NO;}
    if (![lhs[@"acct"] isEqualToString:rhs[@"acct"]]) {XCTFail(@"Account differs"); return NO;}
    if (![lhs[@"v_Data"] isEqualToData:rhs[@"v_Data"]]) {XCTFail(@"Data differs"); return NO;}
    // class for lhs is already genp due to copymatching query
    if (![rhs[@"class"] isEqualToString:@"genp"]) {XCTFail(@"Class not genp for remote item"); return NO;}
    return YES;
}

- (BOOL)compareLocalKeychainWithCloudKitState {
    BOOL success = YES;
    NSMutableDictionary *localItems = [self fetchLocalItems];
    if (localItems == nil) {XCTFail(@"Received nil for localItems"); return NO;}
    NSMutableDictionary *remoteItems = [self fetchRemoteItems];
    if (remoteItems == nil) {XCTFail(@"Received nil for remoteItems"); return NO;}

    for (NSString *uuid in localItems.allKeys) {
        if (remoteItems[uuid] == nil) {
            XCTFail(@"account %@ item %@ not present in remote", localItems[uuid][@"acct"], localItems[uuid]);
            success = NO;
            continue;
        }
        if (![self compareLocalItem:localItems[uuid] remote:remoteItems[uuid]]) {
            XCTFail(@"local item %@ matches remote item %@", localItems[uuid], remoteItems[uuid]);
            success = NO;
        }
        [remoteItems removeObjectForKey:uuid];
    }
    if ([remoteItems count]) {
        XCTFail(@"No remote items present not found in local, %@", remoteItems);
        return NO;
    }
    return success;
}

- (BOOL)updateGenericPassword:(NSString *)password account:(NSString *)account {
    NSDictionary *query = @{(id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                            (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
                            (id)kSecAttrAccount : account,
                            (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                            };
    NSDictionary *newpasswd = @{(id)kSecValueData : (id) [password dataUsingEncoding:NSUTF8StringEncoding]};

    return errSecSuccess == SecItemUpdate((__bridge CFDictionaryRef)query, (__bridge CFDictionaryRef)newpasswd);
}

- (BOOL)uploadRecords:(NSArray<CKRecord*>*)records {
    CKModifyRecordsOperation *op = [[CKModifyRecordsOperation alloc] initWithRecordsToSave:records recordIDsToDelete:nil];
    op.configuration.automaticallyRetryNetworkFailures = NO;
    op.configuration.discretionaryNetworkBehavior = CKOperationDiscretionaryNetworkBehaviorNonDiscretionary;
    op.configuration.isCloudKitSupportOperation = YES;
    op.configuration.container = self.container;
    
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    __block BOOL result = NO;
    op.modifyRecordsCompletionBlock = ^(NSArray<CKRecord *> *savedRecords,
                                        NSArray<CKRecordID *> *deletedRecordIDs,
                                        NSError *operationError) {
        XCTAssertNil(operationError, @"No error uploading records, %@", operationError);
        if (operationError == nil) {
            result = YES;
        }
        dispatch_semaphore_signal(sema);
    };

    [op start];
    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);

    return result;
}

#pragma mark Helpers Adapted from MockXCTest

- (CKRecord*)createFakeRecord: (CKRecordZoneID*)zoneID recordName:(NSString*)recordName {
    return [self createFakeRecord: zoneID recordName:recordName withAccount: nil];
}

- (CKRecord*)createFakeRecord: (CKRecordZoneID*)zoneID recordName:(NSString*)recordName withAccount: (NSString*) account {
    NSError* error = nil;

    /* Basically: @{
     @"acct"  : @"account-delete-me",
     @"agrp"  : @"com.apple.security.sos",
     @"cdat"  : @"2016-12-21 03:33:25 +0000",
     @"class" : @"genp",
     @"mdat"  : @"2016-12-21 03:33:25 +0000",
     @"musr"  : [[NSData alloc] init],
     @"pdmn"  : @"ak",
     @"sha1"  : [[NSData alloc] initWithBase64EncodedString: @"C3VWONaOIj8YgJjk/xwku4By1CY=" options:0],
     @"svce"  : @"",
     @"tomb"  : [NSNumber numberWithInt: 0],
     @"v_Data" : [@"data" dataUsingEncoding: NSUTF8StringEncoding],
     };
     TODO: this should be binary encoded instead of expanded, but the plist encoder should handle it fine */
    NSData* itemdata = [[NSData alloc] initWithBase64EncodedString:@"PD94bWwgdmVyc2lvbj0iMS4wIiBlbmNvZGluZz0iVVRGLTgiPz4KPCFET0NUWVBFIHBsaXN0IFBVQkxJQyAiLS8vQXBwbGUvL0RURCBQTElTVCAxLjAvL0VOIiAiaHR0cDovL3d3dy5hcHBsZS5jb20vRFREcy9Qcm9wZXJ0eUxpc3QtMS4wLmR0ZCI+CjxwbGlzdCB2ZXJzaW9uPSIxLjAiPgo8ZGljdD4KCTxrZXk+YWNjdDwva2V5PgoJPHN0cmluZz5hY2NvdW50LWRlbGV0ZS1tZTwvc3RyaW5nPgoJPGtleT5hZ3JwPC9rZXk+Cgk8c3RyaW5nPmNvbS5hcHBsZS5zZWN1cml0eS5zb3M8L3N0cmluZz4KCTxrZXk+Y2RhdDwva2V5PgoJPGRhdGU+MjAxNi0xMi0yMVQwMzozMzoyNVo8L2RhdGU+Cgk8a2V5PmNsYXNzPC9rZXk+Cgk8c3RyaW5nPmdlbnA8L3N0cmluZz4KCTxrZXk+bWRhdDwva2V5PgoJPGRhdGU+MjAxNi0xMi0yMVQwMzozMzoyNVo8L2RhdGU+Cgk8a2V5Pm11c3I8L2tleT4KCTxkYXRhPgoJPC9kYXRhPgoJPGtleT5wZG1uPC9rZXk+Cgk8c3RyaW5nPmFrPC9zdHJpbmc+Cgk8a2V5PnNoYTE8L2tleT4KCTxkYXRhPgoJQzNWV09OYU9JajhZZ0pqay94d2t1NEJ5MUNZPQoJPC9kYXRhPgoJPGtleT5zdmNlPC9rZXk+Cgk8c3RyaW5nPjwvc3RyaW5nPgoJPGtleT50b21iPC9rZXk+Cgk8aW50ZWdlcj4wPC9pbnRlZ2VyPgoJPGtleT52X0RhdGE8L2tleT4KCTxkYXRhPgoJWkdGMFlRPT0KCTwvZGF0YT4KPC9kaWN0Pgo8L3BsaXN0Pgo=" options:0];
    NSMutableDictionary * item = [[NSPropertyListSerialization propertyListWithData:itemdata
                                                                            options:0
                                                                             format:nil
                                                                              error:&error] mutableCopy];
    // Fix up dictionary
    item[@"agrp"] = @"com.apple.security.ckks";

    XCTAssertNil(error, "interpreted data as item");

    if(account) {
        [item setObject: account forKey: (__bridge id) kSecAttrAccount];
    }

    CKRecordID* ckrid = [[CKRecordID alloc] initWithRecordName:recordName zoneID:zoneID];
    return [self newRecord: ckrid withNewItemData: item];
}

- (CKRecord*)newRecord: (CKRecordID*) recordID withNewItemData:(NSDictionary*) dictionary {
    NSError* error = nil;
    CKKSKey* classCKey = [CKKSKey currentKeyForClass:SecCKKSKeyClassC zoneID:recordID.zoneID error:&error];
    XCTAssertNotNil(classCKey, "Have class C key for zone");

    CKKSItem* cipheritem = [CKKSItemEncrypter encryptCKKSItem:[[CKKSItem alloc] initWithUUID:recordID.recordName
                                                                               parentKeyUUID:classCKey.uuid
                                                                                      zoneID:recordID.zoneID]
                                               dataDictionary:dictionary
                                             updatingCKKSItem:nil
                                                    parentkey:classCKey
                                                        error:&error];

    CKKSOutgoingQueueEntry* ciphertext = [[CKKSOutgoingQueueEntry alloc] initWithCKKSItem:cipheritem
                                                                                   action:SecCKKSActionAdd
                                                                                    state:SecCKKSStateNew
                                                                                waitUntil:nil
                                                                              accessGroup:@"unused in this function"];
    XCTAssertNil(error, "encrypted item with class c key");

    CKRecord* ckr = [ciphertext.item CKRecordWithZoneID: recordID.zoneID];
    XCTAssertNotNil(ckr, "Created a CKRecord");
    return ckr;
}

- (NSDictionary*)decryptRecord: (CKRecord*) record {
    CKKSMirrorEntry* ckme = [[CKKSMirrorEntry alloc] initWithCKRecord: record];

    NSError* error = nil;

    NSDictionary* ret = [CKKSItemEncrypter decryptItemToDictionary:ckme.item error:&error];
    XCTAssertNil(error);
    XCTAssertNotNil(ret);
    return ret;
}

- (BOOL)addMultiplePasswords:(NSString *)password account:(NSString *)account amount:(NSUInteger)amount {
    while (amount > 0) {
        if (![self addGenericPassword:password account:[NSString stringWithFormat:@"%@%03lu", account, amount]]) {
            return NO;
        }
        amount -= 1;
    }
    return YES;
}

- (BOOL)deleteMultiplePasswords:(NSString *)account amount:(NSUInteger)amount {
    while (amount > 0) {
        if (![self deleteGenericPassword:[NSString stringWithFormat:@"%@%03lu", account, amount]]) {
            return NO;
        }
        amount -= 1;
    }
    return YES;
}

- (BOOL)addGenericPassword: (NSString*) password account: (NSString*) account viewHint: (NSString*) viewHint expecting: (OSStatus) status message: (NSString*) message {
    NSMutableDictionary* query = [@{
                                    (id)kSecClass : (id)kSecClassGenericPassword,
                                    (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                                    (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
                                    (id)kSecAttrAccount : account,
                                    (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                                    (id)kSecValueData : (id) [password dataUsingEncoding:NSUTF8StringEncoding],
                                    } mutableCopy];

    if(viewHint) {
        query[(id)kSecAttrSyncViewHint] = viewHint;
    }

    return status == SecItemAdd((__bridge CFDictionaryRef) query, NULL);
}

- (BOOL)addGenericPassword: (NSString*) password account: (NSString*) account expecting: (OSStatus) status message: (NSString*) message {
    return [self addGenericPassword:password account:account viewHint:nil expecting:errSecSuccess message:message];
}

- (BOOL)addGenericPassword: (NSString*) password account: (NSString*) account {
    return [self addGenericPassword:password account:account viewHint:nil expecting:errSecSuccess message:@"Add item to keychain"];
}

- (BOOL)addGenericPassword: (NSString*) password account: (NSString*) account viewHint:(NSString*)viewHint {
    return [self addGenericPassword:password account:account viewHint:viewHint expecting:errSecSuccess message:@"Add item to keychain with a viewhint"];
}

- (BOOL)deleteGenericPassword: (NSString*) account {
    NSDictionary* query = @{(id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecAttrAccount : account,
                            (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,};

    return errSecSuccess == SecItemDelete((__bridge CFDictionaryRef) query);
}

- (BOOL)findGenericPassword: (NSString*) account expecting: (OSStatus) status {
    NSDictionary *query = @{(id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                            (id)kSecAttrAccount : account,
                            (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                            (id)kSecMatchLimit : (id)kSecMatchLimitOne,};

    return status == SecItemCopyMatching((__bridge CFDictionaryRef) query, NULL);
}

- (void)checkGenericPassword: (NSString*) password account: (NSString*) account {
    NSDictionary *query = @{(id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                            (id)kSecAttrAccount : account,
                            (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                            (id)kSecMatchLimit : (id)kSecMatchLimitOne,
                            (id)kSecReturnData : (id)kCFBooleanTrue,
                            };
    CFTypeRef result = NULL;

    XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef) query, &result), "Finding item %@", account);
    XCTAssertNotNil((__bridge id)result, "Received an item");

    NSString* storedPassword = [[NSString alloc] initWithData: (__bridge NSData*) result encoding: NSUTF8StringEncoding];
    XCTAssertNotNil(storedPassword, "Password parsed as a password");

    XCTAssertEqualObjects(storedPassword, password, "Stored password matches received password");
}

#pragma mark Tests

- (void)testAddDelete {
    [self startCKKSSubsystem];
    [self.kcv waitForKeyHierarchyReadiness];

    XCTAssert([self addGenericPassword:@"data" account:@"ck-test-adddelete"], @"Added single item");
    
    [self waitForEmptyOutgoingQueue:self.kcv];
    XCTAssert([self compareLocalKeychainWithCloudKitState], @"testAdd: states match after add");
    
    XCTAssert([self deleteGenericPassword:@"ck-test-adddelete"], @"Deleted single item");
    
    [self waitForEmptyOutgoingQueue:self.kcv];
    XCTAssert([self compareLocalKeychainWithCloudKitState], @"testAdd: states match after delete");
}

- (void)testAddModDelete {
    [self startCKKSSubsystem];
    [self.kcv waitForKeyHierarchyReadiness];
    
    [self addGenericPassword:@"data" account:@"ck-test-addmoddelete"];
    
    [self waitForEmptyOutgoingQueue:self.kcv];
    XCTAssert([self compareLocalKeychainWithCloudKitState], @"testAddMod: states match after add");
    
    [self updateGenericPassword:@"otherdata" account:@"ck-test-addmoddelete"];
    
    [self waitForEmptyOutgoingQueue:self.kcv];
    XCTAssert([self compareLocalKeychainWithCloudKitState], @"testAddMod: states match after mod");
    
    [self deleteGenericPassword:@"ck-test-addmoddelete"];

    [self waitForEmptyOutgoingQueue:self.kcv];
    XCTAssert([self compareLocalKeychainWithCloudKitState], @"testAddMod: states match after del");
}

- (void)testAddModDeleteImmediate {
    [self startCKKSSubsystem];
    [self.kcv waitForKeyHierarchyReadiness];

    XCTAssert([self addGenericPassword:@"data" account:@"ck-test-addmoddeleteimmediate"], @"Added item");
    XCTAssert([self updateGenericPassword:@"otherdata" account:@"ck-test-addmoddeleteimmediate"], @"Modified item");
    XCTAssert([self deleteGenericPassword:@"ck-test-addmoddeleteimmediate"], @"Deleted item");
    
    [self waitForEmptyOutgoingQueue:self.kcv];
    [self.kcv waitForFetchAndIncomingQueueProcessing];
    XCTAssert([self compareLocalKeychainWithCloudKitState], @"testAddMod: states match after immediate add/mod/delete");
}

- (void)testReceive {
    [self startCKKSSubsystem];
    [self.kcv waitForKeyHierarchyReadiness];

    [self findGenericPassword:@"ck-test-receive" expecting:errSecItemNotFound];
    XCTAssert([self compareLocalKeychainWithCloudKitState], @"testReceive: states match before receive");

    CKRecord *record = [self createFakeRecord:self.zoneID recordName:@"b6050e4d-e7b7-4e4e-b318-825cacc34722" withAccount:@"ck-test-receive"];
    [self uploadRecords:@[record]];

    [self.kcv notifyZoneChange:nil];
    [self.kcv waitForFetchAndIncomingQueueProcessing];

    [self findGenericPassword:@"ck-test-receive" expecting:errSecSuccess];
    XCTAssert([self compareLocalKeychainWithCloudKitState], @"testReceive: states match after receive");
}

- (void)testReceiveColliding {
    [self startCKKSSubsystem];
    [self.kcv waitForKeyHierarchyReadiness];

    XCTAssert([self findGenericPassword:@"ck-test-receivecolliding" expecting:errSecItemNotFound], @"test item not yet in keychain");

    // Conflicting items! This test does not care how conflict gets resolved, just that state is consistent after syncing
    CKRecord *r1 = [self createFakeRecord:self.zoneID recordName:@"97576447-c6b8-47fe-8f00-64f5da49d538" withAccount:@"ck-test-receivecolliding"];
    CKRecord *r2 = [self createFakeRecord:self.zoneID recordName:@"c6b86447-9757-47fe-8f00-64f5da49d538" withAccount:@"ck-test-receivecolliding"];
    [self uploadRecords:@[r1,r2]];

    // poke CKKS since we won't have a real CK notification
    [self.kcv notifyZoneChange:nil];
    [self.kcv waitForFetchAndIncomingQueueProcessing];
    [self waitForEmptyOutgoingQueue:self.kcv];

    XCTAssert([self findGenericPassword:@"ck-test-receivecolliding" expecting:errSecSuccess], @"Item present after download");
    // This will also flag an issue if the two conflicting items persist
    XCTAssert([self compareLocalKeychainWithCloudKitState], @"Back in sync after processing incoming changes");
}

- (void)testAddMultipleDeleteAll {
    [self startCKKSSubsystem];
    [self.kcv waitForKeyHierarchyReadiness];
    
    XCTAssert([self addMultiplePasswords:@"data" account:@"ck-test-addmultipledeleteall" amount:5]);
    
    [self waitForEmptyOutgoingQueue:self.kcv];
    XCTAssert([self compareLocalKeychainWithCloudKitState], @"testAddMultipleDeleteAll: states match after adds");

    XCTAssert([self deleteMultiplePasswords:@"ck-test-addmultipledeleteall" amount:3]);

    [self waitForEmptyOutgoingQueue:self.kcv];
    XCTAssert([self compareLocalKeychainWithCloudKitState], @"testAddMultipleDeleteAll: states match after deletes");

    XCTAssert([self deleteGenericPassword:@"ck-test-addmultipledeleteall005"]);
    XCTAssert([self deleteGenericPassword:@"ck-test-addmultipledeleteall004"]);

    [self waitForEmptyOutgoingQueue:self.kcv];
    XCTAssert([self compareLocalKeychainWithCloudKitState], @"testAddMultipleDeleteAll: states match after deletes");
}

- (void)testAddLotsOfItems {
    [ self startCKKSSubsystem];
    [self.kcv waitForKeyHierarchyReadiness];
    
    XCTAssert([self addMultiplePasswords:@"data" account:@"ck-test-addlotsofitems" amount:250], @"Added a truckload of items");

    XCTAssert([self waitForEmptyOutgoingQueue:self.kcv], @"Completed upload within %ld seconds", (long)self.queueTimeout);
    XCTAssert([self compareLocalKeychainWithCloudKitState], @"testAddLotsOfItems: states match after adding tons of items");
    
    XCTAssert([self deleteMultiplePasswords:@"ck-test-addlotsofitems" amount:250], @"Got rid of a truckload of items");
    XCTAssert([self waitForEmptyOutgoingQueue:self.kcv], @"Completed deletions within %ld seconds",(long)self.queueTimeout);
    
    XCTAssert([self compareLocalKeychainWithCloudKitState], @"testAddLotsOfItems: states match after removing tons of items again");
}

#endif

@end
