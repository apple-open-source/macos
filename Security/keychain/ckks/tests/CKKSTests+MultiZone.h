
#ifndef CKKSTests_MultiZone_h
#define CKKSTests_MultiZone_h

#if OCTAGON

#import "keychain/ckks/tests/CloudKitMockXCTest.h"
#import "keychain/ckks/tests/CloudKitKeychainSyncingMockXCTest.h"
#import "keychain/ckks/CKKSPeer.h"

@interface CloudKitKeychainSyncingMultiZoneTestsBase : CloudKitKeychainSyncingMockXCTest

@property CKRecordZoneID*      engramZoneID;
@property CKKSKeychainViewState* engramView;
@property FakeCKZone*          engramZone;
@property (readonly) ZoneKeys* engramZoneKeys;

@property CKRecordZoneID*      manateeZoneID;
@property CKKSKeychainViewState* manateeView;
@property FakeCKZone*          manateeZone;
@property (readonly) ZoneKeys* manateeZoneKeys;

@property CKRecordZoneID*      autoUnlockZoneID;
@property CKKSKeychainViewState* autoUnlockView;
@property FakeCKZone*          autoUnlockZone;
@property (readonly) ZoneKeys* autoUnlockZoneKeys;

@property CKRecordZoneID*      healthZoneID;
@property CKKSKeychainViewState* healthView;
@property FakeCKZone*          healthZone;
@property (readonly) ZoneKeys* healthZoneKeys;

@property CKRecordZoneID*      applepayZoneID;
@property CKKSKeychainViewState* applepayView;
@property FakeCKZone*          applepayZone;
@property (readonly) ZoneKeys* applepayZoneKeys;

@property CKRecordZoneID*      homeZoneID;
@property CKKSKeychainViewState* homeView;
@property FakeCKZone*          homeZone;
@property (readonly) ZoneKeys* homeZoneKeys;

@property CKRecordZoneID*      mfiZoneID;
@property CKKSKeychainViewState* mfiView;
@property FakeCKZone*          mfiZone;
@property (readonly) ZoneKeys* mfiZoneKeys;

@property CKRecordZoneID*      limitedZoneID;
@property CKKSKeychainViewState* limitedView;
@property FakeCKZone*          limitedZone;
@property (readonly) ZoneKeys* limitedZoneKeys;

@property CKRecordZoneID*      passwordsZoneID;
@property CKKSKeychainViewState* passwordsView;
@property FakeCKZone*          passwordsZone;
@property (readonly) ZoneKeys* passwordsZoneKeys;

@property CKRecordZoneID*      ptaZoneID;
@property CKKSKeychainViewState* ptaView;
@property FakeCKZone*          ptaZone;

- (void)saveFakeKeyHierarchiesToLocalDatabase;
- (void)putFakeDeviceStatusesInCloudKit;
- (void)putFakeKeyHierachiesInCloudKit;
- (void)saveTLKsToKeychain;
- (void)deleteTLKMaterialsFromKeychain;
- (void)waitForKeyHierarchyReadinesses;
- (void)expectCKKSTLKSelfShareUploads;

- (void)putAllFakeDeviceStatusesInCloudKit;
- (void)putAllSelfTLKSharesInCloudKit:(id<CKKSSelfPeer>)sharingPeer;
- (void)putAllTLKSharesInCloudKitFrom:(id<CKKSSelfPeer>)sharingPeer to:(id<CKKSPeer>)receivingPeer;

@end

#endif // OCTAGON
#endif /* CKKSTests_MultiZone_h */
