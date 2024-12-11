#ifndef CKKSMockCuttlefishAdapter_h
#define CKKSMockCuttlefishAdapter_h

#import <Foundation/Foundation.h>
#if OCTAGON

#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>

#import "keychain/ckks/tests/CloudKitKeychainSyncingMockXCTest.h"
#import "keychain/ckks/tests/CloudKitMockXCTest.h"
#import "keychain/ckks/tests/MockCloudKit.h"

#import "keychain/ckks/CKKSCuttlefishAdapter.h"
#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"

NS_ASSUME_NONNULL_BEGIN

@interface CKKSMockCuttlefishAdapter : NSObject <CKKSCuttlefishAdapterProtocol>

@property (nullable) NSMutableDictionary<CKRecordZoneID*, FakeCKZone*>* fakeCKZones;
@property (nullable) NSMutableDictionary<CKRecordZoneID*, ZoneKeys*>* zoneKeys;
@property (nullable) NSString* peerID;

- (instancetype)init:(NSMutableDictionary<CKRecordZoneID*, FakeCKZone*>*)fakeCKZones
            zoneKeys:(NSMutableDictionary<CKRecordZoneID*, ZoneKeys*>*)zoneKeys
              peerID:(NSString*)peerID;
@end

NS_ASSUME_NONNULL_END

#endif // OCTAGON

#endif
