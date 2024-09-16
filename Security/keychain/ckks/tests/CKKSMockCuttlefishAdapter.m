#import <SecurityFoundation/SecurityFoundation.h>

#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>

#import "keychain/ckks/CKKSCuttlefishAdapter.h"
#import "keychain/ckks/tests/CKKSMockCuttlefishAdapter.h"
#import "keychain/ckks/tests/MockCloudKit.h"
#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"

@implementation CKKSMockCuttlefishAdapter

- (instancetype)init:(NSMutableDictionary<CKRecordZoneID*, FakeCKZone*>*)fakeCKZones
            zoneKeys:(NSMutableDictionary<CKRecordZoneID*, ZoneKeys*>*)zoneKeys {
    
    if((self = [super init])) {
        _fakeCKZones = fakeCKZones;
        _zoneKeys = zoneKeys;
    }
    return self;
}

- (void)fetchCurrentItem:(TPSpecificUser* __nullable)activeAccount
                   items:(nonnull NSArray<CuttlefishCurrentItemSpecifier *> *)items
                   reply:(nonnull void (^)(NSArray<CuttlefishCurrentItem *> * _Nullable, NSArray<CKRecord *> * _Nullable, NSError * _Nullable))reply
{
    NSMutableArray<CuttlefishCurrentItem*>* cuttlefishCurrentItems = [NSMutableArray array];
    NSMutableSet<CKRecord*>* syncKeys = [NSMutableSet set];
    
    for (CuttlefishCurrentItemSpecifier* item in items) {
        
        // Grab zone
        FakeCKZone* zone = nil;
        for (CKRecordZoneID* zoneID in self.fakeCKZones) {
            if ([zoneID.zoneName isEqualToString:item.zoneID]) {
                zone = self.fakeCKZones[zoneID];
                break;
            }
        }
        
        if (zone == nil) {
            reply(nil, nil, [NSError errorWithDomain:CKKSErrorDomain code:CKKSNoSuchView userInfo:@{@"description": [NSString stringWithFormat:@"No such zone (%@)",item.zoneID]}]);
            return;
        }
        
        CKRecordZoneID* recordZoneID = [[CKRecordZoneID alloc] initWithZoneName:item.zoneID ownerName:CKCurrentUserDefaultName];
        
        CKRecordID* currentItemRecordID = [[CKRecordID alloc] initWithRecordName:item.itemPtrName zoneID:recordZoneID];
        CKRecord* currentItemRecord = zone.currentDatabase[currentItemRecordID];
        
        if (currentItemRecord == nil) {
            reply(nil, nil, [NSError errorWithDomain:CKKSErrorDomain code:CKKSNoSuchRecord userInfo:@{@"description": [NSString stringWithFormat:@"No such currentitem record (%@)", item]}]);
            return;
        }
        
        CKRecordID* itemRecordID = [[CKRecordID alloc] initWithRecordName: [currentItemRecord[SecCKRecordItemRefKey] recordID].recordName zoneID:recordZoneID];
        CKRecord* itemRecord = zone.currentDatabase[itemRecordID];
        
        if (itemRecord == nil) {
            reply(nil, nil, [NSError errorWithDomain:CKKSErrorDomain code:CKKSNoSuchRecord userInfo:@{@"description": [NSString stringWithFormat:@"No such item record for current item pointer (%@)", item]}]);
            return;
        }
        
        [cuttlefishCurrentItems addObject:[[CuttlefishCurrentItem alloc] init:item item:itemRecord]];
        
        // Add all zone keys
        [syncKeys addObject:[self.zoneKeys[recordZoneID].tlk CKRecordWithZoneID:recordZoneID]];
        [syncKeys addObject:[self.zoneKeys[recordZoneID].classA CKRecordWithZoneID:recordZoneID]];
        [syncKeys addObject:[self.zoneKeys[recordZoneID].classC CKRecordWithZoneID:recordZoneID]];
    }

    reply(cuttlefishCurrentItems, [syncKeys allObjects], nil);
    
}

- (void)fetchPCSIdentityByKey:(TPSpecificUser* __nullable)activeAccount
                  pcsservices:(nonnull NSArray<CuttlefishPCSServiceIdentifier *> *)pcsservices
                        reply:(nonnull void (^)(NSArray<CuttlefishPCSIdentity *> * _Nullable, NSArray<CKRecord *> * _Nullable, NSError * _Nullable))reply
{
    
    NSMutableArray<CuttlefishPCSIdentity*>* pcsIdentities = [NSMutableArray array];
    NSMutableSet<CKRecord*>* syncKeys = [NSMutableSet set];
    
    for (CuttlefishPCSServiceIdentifier* pcsService in pcsservices) {
        
        // Grab zone
        FakeCKZone* zone = nil;
        for (CKRecordZoneID* zoneID in self.fakeCKZones) {
            if ([zoneID.zoneName isEqualToString:pcsService.zoneID]) {
                zone = self.fakeCKZones[zoneID];
                break;
            }
        }
        
        if (zone == nil) {
            reply(nil, nil, [NSError errorWithDomain:CKKSErrorDomain code:CKKSNoSuchView userInfo:@{@"description": [NSString stringWithFormat:@"No such zone (%@)",pcsService.zoneID]}]);
            return;
        }
        
        // Find record with matching service number and public key
        CKRecord* pcsIdentityRecord = nil;
        for (CKRecord* record in [zone.currentDatabase allValues]) {
            if (([record.recordType isEqualToString:SecCKRecordItemRefKey])
                && ([record[SecCKRecordPCSServiceIdentifier] isEqual: pcsService.PCSServiceID])
                && ([record[SecCKRecordPCSPublicKey] isEqual: pcsService.PCSPublicKey])) {
                pcsIdentityRecord = record;
                break;
            }
        }
        
        if (pcsIdentityRecord == nil) {
            reply(nil, nil, [NSError errorWithDomain:CKKSErrorDomain code:CKKSNoSuchRecord userInfo:@{@"description": [NSString stringWithFormat:@"No such item record (%@)", pcsService]}]);
            return;
        }
        
        // Return service + item
        [pcsIdentities addObject:[[CuttlefishPCSIdentity alloc] init:pcsService item:pcsIdentityRecord]];

        CKRecordZoneID* recordZoneID = [[CKRecordZoneID alloc] initWithZoneName:pcsService.zoneID ownerName:CKCurrentUserDefaultName];

        // Add all zone keys
        [syncKeys addObject:[self.zoneKeys[recordZoneID].tlk CKRecordWithZoneID:recordZoneID]];
        [syncKeys addObject:[self.zoneKeys[recordZoneID].classA CKRecordWithZoneID:recordZoneID]];
        [syncKeys addObject:[self.zoneKeys[recordZoneID].classC CKRecordWithZoneID:recordZoneID]];
    }
    
    reply(pcsIdentities, [syncKeys allObjects], nil);
}

@end
