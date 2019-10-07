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

#import "CloudKitMockXCTest.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSControl.h"
#import "keychain/ckks/CKKSCurrentKeyPointer.h"
#import "keychain/ckks/CKKSItem.h"
#import "keychain/ckks/tests/CKKSMockSOSPresentAdapter.h"
#import "keychain/ot/proto/generated_source/OTAccountMetadataClassC.h"
#import "keychain/ot/OTCuttlefishAccountStateHolder.h"
#include "OSX/sec/Security/SecItemShim.h"

NS_ASSUME_NONNULL_BEGIN

@class CKKSKey;
@class CKKSCurrentKeyPointer;

@interface ZoneKeys : CKKSCurrentKeySet
@property CKKSKey* rolledTLK;

- (instancetype)initLoadingRecordsFromZone:(FakeCKZone*)zone;
@end

/*
 * Builds on the CloudKit mock infrastructure and adds keychain helper methods.
 */

@interface CloudKitKeychainSyncingMockXCTest : CloudKitMockXCTest

@property CKKSControl* ckksControl;
@property OTCuttlefishAccountStateHolder *accountMetaDataStore;
@property (nullable) id mockCKKSKeychainBackedKey;

@property (nullable) NSError* keychainFetchError;

// A single trusted SOSPeer, but without any CKKS keys
@property CKKSSOSPeer* remoteSOSOnlyPeer;

// Set this to false after calling -setUp if you want to initialize the views yourself
@property bool automaticallyBeginCKKSViewCloudKitOperation;

// Fill this in before allowing initialization to use your own mock instead of a default stub
@property id suggestTLKUpload;

@property NSMutableSet<CKKSKeychainView*>* ckksViews;
@property NSMutableSet<CKRecordZoneID*>* ckksZones;
@property (nullable) NSMutableDictionary<CKRecordZoneID*, ZoneKeys*>* keys;

// Pass in an oldTLK to wrap it to the new TLK; otherwise, pass nil
- (ZoneKeys*)createFakeKeyHierarchy:(CKRecordZoneID*)zoneID oldTLK:(CKKSKey* _Nullable)oldTLK;
- (void)saveFakeKeyHierarchyToLocalDatabase:(CKRecordZoneID*)zoneID;
- (void)putFakeKeyHierarchyInCloudKit:(CKRecordZoneID*)zoneID;
- (void)saveTLKMaterialToKeychain:(CKRecordZoneID*)zoneID;
- (void)deleteTLKMaterialFromKeychain:(CKRecordZoneID*)zoneID;
- (void)saveTLKMaterialToKeychainSimulatingSOS:(CKRecordZoneID*)zoneID;
- (void)putFakeDeviceStatusInCloudKit:(CKRecordZoneID*)zoneID;
- (void)putFakeDeviceStatusInCloudKit:(CKRecordZoneID*)zoneID
                             zonekeys:(ZoneKeys*)zonekeys;

- (void)putFakeOctagonOnlyDeviceStatusInCloudKit:(CKRecordZoneID*)zoneID;
- (void)putFakeOctagonOnlyDeviceStatusInCloudKit:(CKRecordZoneID*)zoneID
                                        zonekeys:(ZoneKeys*)zonekeys;

- (void)SOSPiggyBackAddToKeychain:(NSDictionary*)piggydata;
- (NSMutableDictionary*)SOSPiggyBackCopyFromKeychain;
- (NSMutableArray<NSData*>*)SOSPiggyICloudIdentities;

// Octagon is responsible for telling CKKS that it's trusted.
// But, in these tests, use these to pretend that SOS is the only trust source around.
- (void)beginSOSTrustedOperationForAllViews;
- (void)beginSOSTrustedViewOperation:(CKKSKeychainView*)view;
- (void)endSOSTrustedOperationForAllViews;
- (void)endSOSTrustedViewOperation:(CKKSKeychainView*)view;

- (void)putTLKShareInCloudKit:(CKKSKey*)key
                         from:(id<CKKSSelfPeer>)sharingPeer
                           to:(id<CKKSPeer>)receivingPeer
                       zoneID:(CKRecordZoneID*)zoneID;
- (void)putTLKSharesInCloudKit:(CKKSKey*)key from:(CKKSSOSSelfPeer*)sharingPeer zoneID:(CKRecordZoneID*)zoneID;
- (void)putSelfTLKSharesInCloudKit:(CKRecordZoneID*)zoneID;
- (void)saveTLKSharesInLocalDatabase:(CKRecordZoneID*)zoneID;

- (void)saveClassKeyMaterialToKeychain:(CKRecordZoneID*)zoneID;

// Call this to fake out your test: all keys are created, saved in cloudkit, and saved locally (as if the key state machine had processed them)
- (void)createAndSaveFakeKeyHierarchy:(CKRecordZoneID*)zoneID;

- (void)rollFakeKeyHierarchyInCloudKit:(CKRecordZoneID*)zoneID;

- (NSArray<CKRecord*>*)putKeySetInCloudKit:(CKKSCurrentKeySet*)keyset;
- (void)performOctagonTLKUpload:(NSSet<CKKSKeychainView*>*)views;
- (void)performOctagonTLKUpload:(NSSet<CKKSKeychainView*>*)views afterUpload:(void (^_Nullable)(void))afterUpload;

- (NSDictionary*)fakeRecordDictionary:(NSString* _Nullable)account zoneID:(CKRecordZoneID*)zoneID;
- (CKRecord*)createFakeRecord:(CKRecordZoneID*)zoneID recordName:(NSString*)recordName;
- (CKRecord*)createFakeRecord:(CKRecordZoneID*)zoneID recordName:(NSString*)recordName withAccount:(NSString* _Nullable)account;
- (CKRecord*)createFakeRecord:(CKRecordZoneID*)zoneID
                   recordName:(NSString*)recordName
                  withAccount:(NSString* _Nullable)account
                          key:(CKKSKey* _Nullable)key;

- (CKKSItem*)newItem:(CKRecordID*)recordID withNewItemData:(NSDictionary*) dictionary key:(CKKSKey*)key;
- (CKRecord*)newRecord:(CKRecordID*)recordID withNewItemData:(NSDictionary*)dictionary;
- (CKRecord*)newRecord:(CKRecordID*)recordID withNewItemData:(NSDictionary*)dictionary key:(CKKSKey*)key;
- (NSDictionary*)decryptRecord:(CKRecord*)record;

// Do keychain things:
- (void)addGenericPassword:(NSString*)password account:(NSString*)account;
- (void)addGenericPassword:(NSString*)password account:(NSString*)account viewHint:(NSString* _Nullable)viewHint;
- (void)addGenericPassword:(NSString*)password
                   account:(NSString*)account
                  viewHint:(NSString* _Nullable)viewHint
                    access:(NSString*)access
                 expecting:(OSStatus)status
                   message:(NSString*)message;
- (void)addGenericPassword:(NSString*)password account:(NSString*)account expecting:(OSStatus)status message:(NSString*)message;

- (void)updateGenericPassword:(NSString*)newPassword account:(NSString*)account;
- (void)updateAccountOfGenericPassword:(NSString*)newAccount account:(NSString*)account;

- (void)checkNoCKKSData:(CKKSKeychainView*)view;

- (void)deleteGenericPassword:(NSString*)account;
- (void)deleteGenericPasswordWithoutTombstones:(NSString*)account;

- (void)findGenericPassword:(NSString*)account expecting:(OSStatus)status;
- (void)checkGenericPassword:(NSString*)password account:(NSString*)account;

- (void)createClassCItemAndWaitForUpload:(CKRecordZoneID*)zoneID account:(NSString*)account;
- (void)createClassAItemAndWaitForUpload:(CKRecordZoneID*)zoneID account:(NSString*)account;

// Pass the blocks created with these to expectCKModifyItemRecords to check if all items were encrypted with a particular class key
- (BOOL (^)(CKRecord*))checkClassABlock:(CKRecordZoneID*)zoneID message:(NSString*)message;
- (BOOL (^)(CKRecord*))checkClassCBlock:(CKRecordZoneID*)zoneID message:(NSString*)message;

- (BOOL (^)(CKRecord*))checkPasswordBlock:(CKRecordZoneID*)zoneID account:(NSString*)account password:(NSString*)password;

- (void)checkNSyncableTLKsInKeychain:(size_t)n;

// Returns an expectation that someone will send an NSNotification that this view changed
- (XCTestExpectation*)expectChangeForView:(NSString*)view;

// Establish an assertion that CKKS will cause a server extension error soon.
- (void)expectCKReceiveSyncKeyHierarchyError:(CKRecordZoneID*)zoneID;

// Add expectations that CKKS will upload a single TLK share
- (void)expectCKKSTLKSelfShareUpload:(CKRecordZoneID*)zoneID;

// Can't call OCMVerifyMock due to Swift? Use this.
- (void)verifyDatabaseMocks;
@end

NS_ASSUME_NONNULL_END

#endif /* OCTAGON */
