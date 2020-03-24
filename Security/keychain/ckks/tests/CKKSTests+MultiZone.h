
#ifndef CKKSTests_MultiZone_h
#define CKKSTests_MultiZone_h

#if OCTAGON

#import "keychain/ckks/tests/CloudKitMockXCTest.h"
#import "keychain/ckks/tests/CloudKitKeychainSyncingMockXCTest.h"

@interface CloudKitKeychainSyncingMultiZoneTestsBase : CloudKitKeychainSyncingMockXCTest

@property CKRecordZoneID*      engramZoneID;
@property CKKSKeychainView*    engramView;
@property FakeCKZone*          engramZone;
@property (readonly) ZoneKeys* engramZoneKeys;

@property CKRecordZoneID*      manateeZoneID;
@property CKKSKeychainView*    manateeView;
@property FakeCKZone*          manateeZone;
@property (readonly) ZoneKeys* manateeZoneKeys;

@property CKRecordZoneID*      autoUnlockZoneID;
@property CKKSKeychainView*    autoUnlockView;
@property FakeCKZone*          autoUnlockZone;
@property (readonly) ZoneKeys* autoUnlockZoneKeys;

@property CKRecordZoneID*      healthZoneID;
@property CKKSKeychainView*    healthView;
@property FakeCKZone*          healthZone;
@property (readonly) ZoneKeys* healthZoneKeys;

@property CKRecordZoneID*      applepayZoneID;
@property CKKSKeychainView*    applepayView;
@property FakeCKZone*          applepayZone;
@property (readonly) ZoneKeys* applepayZoneKeys;

@property CKRecordZoneID*      homeZoneID;
@property CKKSKeychainView*    homeView;
@property FakeCKZone*          homeZone;
@property (readonly) ZoneKeys* homeZoneKeys;

@property CKRecordZoneID*      limitedZoneID;
@property CKKSKeychainView*    limitedView;
@property FakeCKZone*          limitedZone;
@property (readonly) ZoneKeys* limitedZoneKeys;

@property CKRecordZoneID*      passwordsZoneID;
@property CKKSKeychainView*    passwordsView;
@property FakeCKZone*          passwordsZone;
@property (readonly) ZoneKeys* passwordsZoneKeys;

- (void)saveFakeKeyHierarchiesToLocalDatabase;
- (void)putFakeDeviceStatusesInCloudKit;
- (void)putFakeKeyHierachiesInCloudKit;
- (void)saveTLKsToKeychain;
- (void)deleteTLKMaterialsFromKeychain;
- (void)waitForKeyHierarchyReadinesses;
- (void)expectCKKSTLKSelfShareUploads;

@end

#endif // OCTAGON
#endif /* CKKSTests_MultiZone_h */
