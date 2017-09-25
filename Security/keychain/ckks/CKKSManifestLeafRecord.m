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

#import "CKKSManifestLeafRecord.h"
#import "CKKSManifest.h"
#import "CKKSItem.h"
#import "utilities/der_plist.h"
#import <SecurityFoundation/SFDigestOperation.h>
#import <CloudKit/CloudKit.h>

@interface CKKSManifestLeafRecord () {
    NSString* _uuid;
    NSMutableDictionary* _recordDigestDict;
    NSString* _zoneName;
    NSData* _derData;
    NSData* _digestValue;
}

@property (nonatomic, readonly) NSString* zoneName;
@property (nonatomic, readonly) NSData* derData;

- (void)clearDigest;

@end

@interface CKKSEgoManifestLeafRecord ()

@property (nonatomic, readonly) NSMutableDictionary* recordDigestDict;

@end

static NSData* NodeDERData(NSDictionary* recordDigestDict, NSError** error)
{
    return (__bridge_transfer NSData*)CFPropertyListCreateDERData(NULL, (__bridge CFPropertyListRef)recordDigestDict, (CFErrorRef*)(void*)error);
}

static NSDictionary* RecordDigestDictFromDER(NSData* data, NSError** error)
{
    return (__bridge_transfer NSDictionary*)CFPropertyListCreateWithDERData(NULL, (__bridge CFDataRef)data, 0, NULL, (CFErrorRef*)(void*)error);
}

@implementation CKKSManifestLeafRecord

@synthesize zoneName = _zoneName;
@synthesize recordDigestDict = _recordDigestDict;

+ (BOOL)recordExistsForID:(NSString*)recordID
{
    __block BOOL result = NO;
    
    [CKKSSQLDatabaseObject queryDatabaseTable:self.sqlTable where:@{@"UUID" : recordID} columns:@[@"UUID"] groupBy:nil orderBy:nil limit:1 processRow:^(NSDictionary* row) {
        result = YES;
    } error:nil];
    
    return result;
}

+ (NSString*)leafUUIDForRecordID:(NSString*)recordID
{
    NSArray* components = [recordID componentsSeparatedByString:@":-:"];
    return components.count > 1 ? components[1] : recordID;
}

+ (instancetype)leafRecordForID:(NSString*)recordID error:(NSError* __autoreleasing *)error
{
    return [self fromDatabaseWhere:@{@"UUID" : [self leafUUIDForRecordID:recordID]} error:error];
}

+ (instancetype)tryLeafRecordForID:(NSString*)recordID error:(NSError* __autoreleasing *)error
{
    return [self tryFromDatabaseWhere:@{@"UUID" : [self leafUUIDForRecordID:recordID]} error:error];
}


+ (instancetype)leafRecordForPendingRecord:(CKKSManifestPendingLeafRecord*)pendingRecord
{
    return [[self alloc] initWithUUID:pendingRecord.uuid digest:pendingRecord.digestValue recordDigestDict:pendingRecord.recordDigestDict zone:pendingRecord.zoneName encodedRecord:pendingRecord.encodedCKRecord];
}

+ (instancetype)fromDatabaseRow:(NSDictionary*)row
{
    NSString* zone = row[@"ckzone"];
    NSString* uuid = row[@"UUID"];
    
    NSString* digestBase64String = row[@"digest"];
    NSData* digest = [digestBase64String isKindOfClass:[NSString class]] ? [[NSData alloc] initWithBase64EncodedString:digestBase64String options:0] : nil;
    
    NSString* encodedRecordBase64String = row[@"ckrecord"];
    NSData* encodedRecord = [encodedRecordBase64String isKindOfClass:[NSString class]] ? [[NSData alloc] initWithBase64EncodedString:encodedRecordBase64String options:0] : nil;
    
    NSString* entryDigestBase64String = row[@"entryDigests"];
    NSData* entryDigestData = [entryDigestBase64String isKindOfClass:[NSString class]] ? [[NSData alloc] initWithBase64EncodedString:row[@"entryDigests"] options:0] : nil;
    NSDictionary* entryDigestsDict = entryDigestData ? RecordDigestDictFromDER(entryDigestData, nil) : @{};
    
    return [[self alloc] initWithUUID:uuid digest:digest recordDigestDict:entryDigestsDict zone:zone encodedRecord:encodedRecord];
}

+ (NSArray<NSString*>*)sqlColumns
{
    return @[@"ckzone", @"UUID", @"digest", @"entryDigests", @"ckrecord"];
}

+ (NSString*)sqlTable
{
    return @"ckmanifest_leaf";
}

- (NSString*)description
{
    return [NSString stringWithFormat:@"%@ %@ records: %lu", [super description], self.uuid, (unsigned long)self.recordDigestDict.allValues.count];
}

- (instancetype)initWithCKRecord:(CKRecord*)record
{
    NSError* error = nil;
    NSData* derData = [[NSData alloc] initWithBase64EncodedString:record[SecCKRecordManifestLeafDERKey] options:0];
    NSDictionary<NSString*, NSData*>* recordDigestDict = RecordDigestDictFromDER(derData, &error);
    if (!recordDigestDict) {
        secerror("failed to decode manifest leaf node DER with error: %@", error);
        return nil;
    }
    
    NSData* digestData = [[NSData alloc] initWithBase64EncodedString:record[SecCKRecordManifestLeafDigestKey] options:0];
    return [self initWithUUID:[self.class leafUUIDForRecordID:record.recordID.recordName] digest:digestData recordDigestDict:recordDigestDict zone:record.recordID.zoneID.zoneName];
}

- (instancetype)initWithUUID:(NSString*)uuid digest:(NSData*)digest recordDigestDict:(NSDictionary<NSString*, NSData*>*)recordDigestDict zone:(NSString*)zone
{
    if (self = [super init]) {
        _uuid = uuid;
        _digestValue = digest;
        _recordDigestDict = [recordDigestDict mutableCopy];
        _zoneName = zone;
    }
    
    return self;
}

- (instancetype)initWithUUID:(NSString*)uuid digest:(NSData*)digest recordDigestDict:(NSDictionary<NSString*, NSData*>*)recordDigestDict zone:(NSString*)zone encodedRecord:(NSData*)encodedRecord
{
    if (self = [self initWithUUID:uuid digest:digest recordDigestDict:recordDigestDict zone:zone]) {
        self.encodedCKRecord = encodedRecord;
    }
    
    return self;
}

- (NSDictionary<NSString*, NSString*>*)sqlValues
{
    void (^addValueSafelyToDictionaryAndLogIfNil)(NSMutableDictionary*, NSString*, id) = ^(NSMutableDictionary* dictionary, NSString* key, id value) {
        if (!value) {
            value = [NSNull null];
            secerror("CKKSManifestLeafRecord: saving manifest leaf record to database but %@ is nil", key);
        }

        dictionary[key] = value;
    };

    NSMutableDictionary* sqlValues = [[NSMutableDictionary alloc] init];
    addValueSafelyToDictionaryAndLogIfNil(sqlValues, @"ckzone", _zoneName);
    addValueSafelyToDictionaryAndLogIfNil(sqlValues, @"UUID", _uuid);
    addValueSafelyToDictionaryAndLogIfNil(sqlValues, @"digest", [self.digestValue base64EncodedStringWithOptions:0]);
    addValueSafelyToDictionaryAndLogIfNil(sqlValues, @"entryDigests", [NodeDERData(_recordDigestDict, nil) base64EncodedStringWithOptions:0]);
    sqlValues[@"ckrecord"] = CKKSNilToNSNull([self.encodedCKRecord base64EncodedStringWithOptions:0]);

    return sqlValues;
}

- (NSDictionary<NSString*, NSString*>*)whereClauseToFindSelf
{
    return @{ @"ckzone" : CKKSNilToNSNull(_zoneName),
              @"UUID" : CKKSNilToNSNull(_uuid) };
}

- (NSString*)CKRecordName
{
    return [NSString stringWithFormat:@"ManifestLeafRecord:-:%@", _uuid];
}

- (NSString*)ckRecordType
{
    return SecCKRecordManifestLeafType;
}

- (CKRecord*)updateCKRecord:(CKRecord*)record zoneID:(CKRecordZoneID*)zoneID
{
    record[SecCKRecordManifestLeafDERKey] = [self.derData base64EncodedStringWithOptions:0];
    record[SecCKRecordManifestLeafDigestKey] = [self.digestValue base64EncodedStringWithOptions:0];
    return record;
}

- (void)setFromCKRecord:(CKRecord*)record
{
    NSError* error = nil;
    NSData* derData = [[NSData alloc] initWithBase64EncodedString:record[SecCKRecordManifestLeafDERKey] options:0];
    NSDictionary<NSString*, NSData*>* recordDigestDict = RecordDigestDictFromDER(derData, &error);
    if (!recordDigestDict || error) {
        secerror("failed to decode manifest leaf node DER with error: %@", error);
        return;
    }
    
    self.storedCKRecord = record;
    _uuid = [self.class leafUUIDForRecordID:[self.class leafUUIDForRecordID:record.recordID.recordName]];
    _digestValue = [[NSData alloc] initWithBase64EncodedString:record[SecCKRecordManifestLeafDigestKey] options:0];
    _recordDigestDict = [recordDigestDict mutableCopy];
}

- (bool)matchesCKRecord:(CKRecord*)record
{
    if (![record.recordType isEqualToString:SecCKRecordManifestLeafType]) {
        return false;
    }
    
    NSData* digestData = [[NSData alloc] initWithBase64EncodedString:record[SecCKRecordManifestLeafDigestKey] options:0];
    return [digestData isEqual:self.digestValue];
}

- (NSData*)derData
{
    if (!_derData) {
        NSError* error = nil;
        _derData = NodeDERData(_recordDigestDict, &error);
        NSAssert(!error, @"failed to encode manifest leaf node DER with error: %@", error);
    }
    
    return _derData;
}

- (NSData*)digestValue
{
    if (!_digestValue) {
        _digestValue = [SFSHA384DigestOperation digest:self.derData];
    }
    
    return _digestValue;
}

- (void)clearDigest
{
    _digestValue = nil;
    _derData = nil;
}

@end

@implementation CKKSManifestPendingLeafRecord

+ (NSString*)sqlTable
{
    return @"pending_manifest_leaf";
}

- (CKKSManifestLeafRecord*)commitToDatabaseWithError:(NSError**)error
{
    CKKSManifestLeafRecord* leafRecord = [CKKSManifestLeafRecord leafRecordForPendingRecord:self];
    if ([leafRecord saveToDatabase:error]) {
        [self deleteFromDatabase:error];
        return leafRecord;
    }
    else {
        return nil;
    }
}

@end

@implementation CKKSEgoManifestLeafRecord

@dynamic recordDigestDict;

+ (instancetype)newLeafRecordInZone:(NSString*)zone
{
    return [[self alloc] initWithUUID:[[NSUUID UUID] UUIDString] digest:nil recordDigestDict:@{} zone:zone];
}

- (void)addOrUpdateRecordUUID:(NSString*)uuid withEncryptedItemData:(NSData*)itemData
{
    self.recordDigestDict[uuid] = [SFSHA384DigestOperation digest:itemData];
    [self clearDigest];
}

- (void)addOrUpdateRecord:(CKRecord*)record
{
    [self addOrUpdateRecordUUID:record.recordID.recordName withEncryptedItemData:record[SecCKRecordDataKey]];
}

- (void)deleteItemWithUUID:(NSString*)uuid
{
    [self.recordDigestDict removeObjectForKey:uuid];
    [self clearDigest];
}

@end

#endif
