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

#include <AssertMacros.h>

#import <Foundation/Foundation.h>
#import "CKKSItem.h"
#import "CKKSSIV.h"

#include <utilities/SecDb.h>
#include <securityd/SecDbItem.h>
#include <securityd/SecItemSchema.h>

#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>

@implementation CKKSItem

- (instancetype) initWithCKRecord: (CKRecord*) record {
    if(self = [super initWithCKRecord: record]) {
    }
    return self;
}

- (instancetype) initCopyingCKKSItem: (CKKSItem*) item {
    if(self = [super initWithCKRecordType: item.ckRecordType encodedCKRecord:item.encodedCKRecord zoneID:item.zoneID]) {
        _uuid = item.uuid;
        _parentKeyUUID = item.parentKeyUUID;
        _generationCount = item.generationCount;
        _encitem = item.encitem;
        _wrappedkey = item.wrappedkey;
        _encver = item.encver;

        _plaintextPCSServiceIdentifier = item.plaintextPCSServiceIdentifier;
        _plaintextPCSPublicKey         = item.plaintextPCSPublicKey;
        _plaintextPCSPublicIdentity    = item.plaintextPCSPublicIdentity;
    }
    return self;
}

- (instancetype) initWithUUID: (NSString*) uuid
                parentKeyUUID: (NSString*) parentKeyUUID
                       zoneID: (CKRecordZoneID*) zoneID
{
    return [self initWithUUID:uuid
                parentKeyUUID:parentKeyUUID
                       zoneID:zoneID
              encodedCKRecord:nil
                      encItem:nil
                   wrappedkey:nil
              generationCount:0
                       encver:CKKSItemEncryptionVersionNone];
}

- (instancetype) initWithUUID: (NSString*) uuid
                parentKeyUUID: (NSString*) parentKeyUUID
                       zoneID: (CKRecordZoneID*) zoneID
                      encItem: (NSData*) encitem
                   wrappedkey: (CKKSWrappedAESSIVKey*) wrappedkey
              generationCount: (NSUInteger) genCount
                       encver: (NSUInteger) encver
{
    return [self initWithUUID:uuid
                parentKeyUUID:parentKeyUUID
                       zoneID:zoneID
              encodedCKRecord:nil
                      encItem:encitem
                   wrappedkey:wrappedkey
              generationCount:genCount
                       encver:encver];
}

- (instancetype) initWithUUID: (NSString*) uuid
                parentKeyUUID: (NSString*) parentKeyUUID
                       zoneID: (CKRecordZoneID*)zoneID
              encodedCKRecord: (NSData*) encodedrecord
                      encItem: (NSData*) encitem
                   wrappedkey: (CKKSWrappedAESSIVKey*) wrappedkey
              generationCount: (NSUInteger) genCount
                       encver: (NSUInteger) encver
{
    return [self initWithUUID:uuid
                parentKeyUUID:parentKeyUUID
                       zoneID:zoneID
              encodedCKRecord:encodedrecord
                      encItem:encitem
                   wrappedkey:wrappedkey
              generationCount:genCount
                       encver:encver
plaintextPCSServiceIdentifier:nil
        plaintextPCSPublicKey:nil
   plaintextPCSPublicIdentity:nil];
}

- (instancetype) initWithUUID: (NSString*) uuid
                parentKeyUUID: (NSString*) parentKeyUUID
                       zoneID: (CKRecordZoneID*)zoneID
              encodedCKRecord: (NSData*) encodedrecord
                      encItem: (NSData*) encitem
                   wrappedkey: (CKKSWrappedAESSIVKey*) wrappedkey
              generationCount: (NSUInteger) genCount
                       encver: (NSUInteger) encver
plaintextPCSServiceIdentifier: (NSNumber*) pcsServiceIdentifier
        plaintextPCSPublicKey: (NSData*) pcsPublicKey
   plaintextPCSPublicIdentity: (NSData*) pcsPublicIdentity
{
    if(self = [super initWithCKRecordType: SecCKRecordItemType encodedCKRecord:encodedrecord zoneID:zoneID]) {
        _uuid = uuid;
        _parentKeyUUID = parentKeyUUID;
        _generationCount = genCount;
        self.encitem = encitem;
        _wrappedkey = wrappedkey;
        _encver = encver;

        _plaintextPCSServiceIdentifier = pcsServiceIdentifier;
        _plaintextPCSPublicKey = pcsPublicKey;
        _plaintextPCSPublicIdentity = pcsPublicIdentity;
    }

    return self;
}

- (BOOL)isEqual: (id) object {
    if(![object isKindOfClass:[CKKSItem class]]) {
        return NO;
    }

    CKKSItem* obj = (CKKSItem*) object;

    return ([self.uuid isEqual: obj.uuid] &&
            [self.parentKeyUUID isEqual: obj.parentKeyUUID] &&
            [self.zoneID isEqual: obj.zoneID] &&
            ((self.encitem == nil && obj.encitem == nil) || ([self.encitem isEqual: obj.encitem])) &&
            [self.wrappedkey isEqual: obj.wrappedkey] &&
            self.generationCount == obj.generationCount &&
            self.encver == obj.encver &&
            true) ? YES : NO;
}

#pragma mark - CKRecord handling

- (NSString*) CKRecordName {
    return self.uuid;
}

- (void) setFromCKRecord: (CKRecord*) record {
    if(![record.recordType isEqual: SecCKRecordItemType]) {
        @throw [NSException
                exceptionWithName:@"WrongCKRecordTypeException"
                reason:[NSString stringWithFormat: @"CKRecordType (%@) was not %@", record.recordType, SecCKRecordItemType]
                userInfo:nil];
    }

    [self setStoredCKRecord:record];

    _uuid = [[record recordID] recordName];
    self.parentKeyUUID = [record[SecCKRecordParentKeyRefKey] recordID].recordName;
    self.encitem = record[SecCKRecordDataKey];
    self.wrappedkey = [[CKKSWrappedAESSIVKey alloc] initWithBase64: record[SecCKRecordWrappedKeyKey]];
    self.generationCount = [record[SecCKRecordGenerationCountKey] unsignedIntegerValue];
    self.encver = [record[SecCKRecordEncryptionVersionKey] unsignedIntegerValue];

    self.plaintextPCSServiceIdentifier = record[SecCKRecordPCSServiceIdentifier];
    self.plaintextPCSPublicKey         = record[SecCKRecordPCSPublicKey];
    self.plaintextPCSPublicIdentity    = record[SecCKRecordPCSPublicIdentity];
}

+ (void)setOSVersionInRecord: (CKRecord*) record {
     record[SecCKRecordHostOSVersionKey] = SecCKKSHostOSVersion();
}

- (CKRecord*) updateCKRecord: (CKRecord*) record zoneID: (CKRecordZoneID*) zoneID {
    if(![record.recordType isEqual: SecCKRecordItemType]) {
        @throw [NSException
                exceptionWithName:@"WrongCKRecordTypeException"
                reason:[NSString stringWithFormat: @"CKRecordType (%@) was not %@", record.recordType, SecCKRecordItemType]
                userInfo:nil];
    }

    // Items must have a wrapping key.
    record[SecCKRecordParentKeyRefKey] = [[CKReference alloc] initWithRecordID: [[CKRecordID alloc] initWithRecordName: self.parentKeyUUID zoneID: zoneID] action: CKReferenceActionValidate];

    [CKKSItem setOSVersionInRecord: record];

    record[SecCKRecordDataKey] = self.encitem;
    record[SecCKRecordWrappedKeyKey] = [self.wrappedkey base64WrappedKey];
    record[SecCKRecordGenerationCountKey] = [NSNumber numberWithInteger:self.generationCount];
    // TODO: if the record's generation count is already higher than ours, that's a problem.
    record[SecCKRecordEncryptionVersionKey] = [NSNumber numberWithInteger:self.encver];

    // Add unencrypted fields
    record[SecCKRecordPCSServiceIdentifier] = self.plaintextPCSServiceIdentifier;
    record[SecCKRecordPCSPublicKey]         = self.plaintextPCSPublicKey;
    record[SecCKRecordPCSPublicIdentity]    = self.plaintextPCSPublicIdentity;

    return record;
}


- (bool) matchesCKRecord: (CKRecord*) record {
    if(![record.recordType isEqual: SecCKRecordItemType]) {
        return false;
    }

    // We only really care about the data, the wrapped key, the generation count, and the parent key.
    // Note that since all of those things are included as authenticated data into the AES-SIV ciphertext, we could just
    // compare that. However, check 'em all.
    if(![record.recordID.recordName isEqualToString: self.uuid]) {
        secinfo("ckksitem", "UUID does not match");
        return false;
    }

    if(![[record[SecCKRecordParentKeyRefKey] recordID].recordName isEqualToString: self.parentKeyUUID]) {
        secinfo("ckksitem", "wrapping key reference does not match");
        return false;
    }

    if(![record[SecCKRecordGenerationCountKey] isEqual: [NSNumber numberWithInteger:self.generationCount]]) {
        secinfo("ckksitem", "SecCKRecordGenerationCountKey does not match");
        return false;
    }

    if(![record[SecCKRecordWrappedKeyKey] isEqual: [self.wrappedkey base64WrappedKey]]) {
        secinfo("ckksitem", "SecCKRecordWrappedKeyKey does not match");
        return false;
    }

    if(![record[SecCKRecordDataKey] isEqual: self.encitem]) {
        secinfo("ckksitem", "SecCKRecordDataKey does not match");
        return false;
    }

    // Compare plaintext records, too
    // Why is obj-c nullable equality so difficult?
    if(!((record[SecCKRecordPCSServiceIdentifier] == nil && self.plaintextPCSServiceIdentifier == nil) ||
          [record[SecCKRecordPCSServiceIdentifier] isEqual: self.plaintextPCSServiceIdentifier])) {
        secinfo("ckksitem", "SecCKRecordPCSServiceIdentifier does not match");
        return false;
    }

    if(!((record[SecCKRecordPCSPublicKey] == nil && self.plaintextPCSPublicKey == nil) ||
          [record[SecCKRecordPCSPublicKey] isEqual: self.plaintextPCSPublicKey])) {
        secinfo("ckksitem", "SecCKRecordPCSPublicKey does not match");
        return false;
    }

    if(!((record[SecCKRecordPCSPublicIdentity] == nil && self.plaintextPCSPublicIdentity == nil) ||
          [record[SecCKRecordPCSPublicIdentity] isEqual: self.plaintextPCSPublicIdentity])) {
        secinfo("ckksitem", "SecCKRecordPCSPublicIdentity does not match");
        return false;
    }

    return true;
}

// Generates the list of 'authenticated data' to go along with this item, and optionally adds in unknown, future fields received from CloudKit
- (NSDictionary<NSString*, NSData*>*)makeAuthenticatedDataDictionaryUpdatingCKKSItem:(CKKSItem*) olditem encryptionVersion:(SecCKKSItemEncryptionVersion)encversion {
    switch(encversion) {
        case CKKSItemEncryptionVersion1:
            return [self makeAuthenticatedDataDictionaryUpdatingCKKSItemEncVer1];
        case CKKSItemEncryptionVersion2:
            return [self makeAuthenticatedDataDictionaryUpdatingCKKSItemEncVer2:olditem];
        default:
            @throw [NSException
                    exceptionWithName:@"WrongEncryptionVersionException"
                    reason:[NSString stringWithFormat: @"%d is not a known encryption version", (int)encversion]
                    userInfo:nil];
    }
}

- (NSDictionary<NSString*, NSData*>*)makeAuthenticatedDataDictionaryUpdatingCKKSItemEncVer1 {
    NSMutableDictionary<NSString*, NSData*>* authenticatedData = [[NSMutableDictionary alloc] init];

    authenticatedData[@"UUID"] = [self.uuid dataUsingEncoding: NSUTF8StringEncoding];
    authenticatedData[SecCKRecordWrappedKeyKey] = [self.parentKeyUUID dataUsingEncoding: NSUTF8StringEncoding];

    uint64_t genCount64 = OSSwapHostToLittleConstInt64(self.generationCount);
    authenticatedData[SecCKRecordGenerationCountKey] = [NSData dataWithBytes:&genCount64 length:sizeof(genCount64)];

    uint64_t encver = OSSwapHostToLittleConstInt64((uint64_t)self.encver);
    authenticatedData[SecCKRecordEncryptionVersionKey] = [NSData dataWithBytes:&encver length:sizeof(encver)];

    // In v1, don't authenticate the plaintext PCS fields
    authenticatedData[SecCKRecordPCSServiceIdentifier] = nil;
    authenticatedData[SecCKRecordPCSPublicKey]         = nil;
    authenticatedData[SecCKRecordPCSPublicIdentity]    = nil;

    return authenticatedData;
}

- (NSDictionary<NSString*, NSData*>*)makeAuthenticatedDataDictionaryUpdatingCKKSItemEncVer2:(CKKSItem*) olditem {
    NSMutableDictionary<NSString*, NSData*>* authenticatedData = [[NSMutableDictionary alloc] init];

    authenticatedData[@"UUID"] = [self.uuid dataUsingEncoding: NSUTF8StringEncoding];
    authenticatedData[SecCKRecordWrappedKeyKey] = [self.parentKeyUUID dataUsingEncoding: NSUTF8StringEncoding];

    uint64_t genCount64 = OSSwapHostToLittleConstInt64(self.generationCount);
    authenticatedData[SecCKRecordGenerationCountKey] = [NSData dataWithBytes:&genCount64 length:sizeof(genCount64)];

    uint64_t encver = OSSwapHostToLittleConstInt64((uint64_t)self.encver);
    authenticatedData[SecCKRecordEncryptionVersionKey] = [NSData dataWithBytes:&encver length:sizeof(encver)];

    // v2 authenticates the PCS fields too
    if(self.plaintextPCSServiceIdentifier) {
        uint64_t pcsServiceIdentifier = OSSwapHostToLittleConstInt64([self.plaintextPCSServiceIdentifier unsignedLongValue]);
        authenticatedData[SecCKRecordPCSServiceIdentifier] = [NSData dataWithBytes:&pcsServiceIdentifier length:sizeof(pcsServiceIdentifier)];
    }
    authenticatedData[SecCKRecordPCSPublicKey]         = self.plaintextPCSPublicKey;
    authenticatedData[SecCKRecordPCSPublicIdentity]    = self.plaintextPCSPublicIdentity;

    // Iterate through the fields in the old CKKSItem. If we don't recognize any of them, add them to the authenticated data.
    if(olditem) {
        CKRecord* record = olditem.storedCKRecord;
        if(record) {
            for(NSString* key in record.allKeys) {
                if([key isEqualToString:@"UUID"] ||
                   [key isEqualToString:SecCKRecordHostOSVersionKey] ||
                   [key isEqualToString:SecCKRecordDataKey] ||
                   [key isEqualToString:SecCKRecordWrappedKeyKey] ||
                   [key isEqualToString:SecCKRecordGenerationCountKey] ||
                   [key isEqualToString:SecCKRecordEncryptionVersionKey] ||
                   [key isEqualToString:SecCKRecordPCSServiceIdentifier] ||
                   [key isEqualToString:SecCKRecordPCSPublicKey] ||
                   [key isEqualToString:SecCKRecordPCSPublicIdentity]) {
                    // This version of CKKS knows about this data field. Ignore them with prejudice.
                    continue;
                }

                if([key hasPrefix:@"server_"]) {
                    // Ignore all fields prefixed by "server_"
                    continue;
                }

                id obj = record[key];

                // Skip CKReferences, NSArray, CLLocation, and CKAsset.
                if([obj isKindOfClass: [NSString class]]) {
                    // Add an NSString.
                    authenticatedData[key] = [obj dataUsingEncoding: NSUTF8StringEncoding];
                } else if([obj isKindOfClass: [NSData class]]) {
                    // Add an NSData
                    authenticatedData[key] = [obj copy];
                } else if([obj isKindOfClass:[NSDate class]]) {
                    // Add an NSDate
                    NSISO8601DateFormatter *formatter = [[NSISO8601DateFormatter alloc] init];
                    NSString* str = [formatter stringForObjectValue: obj];

                    authenticatedData[key] = [str dataUsingEncoding: NSUTF8StringEncoding];
                } else if([obj isKindOfClass: [NSNumber class]]) {
                    // Add an NSNumber
                    uint64_t n64 = OSSwapHostToLittleConstInt64([obj unsignedLongLongValue]);
                    authenticatedData[key] = [NSData dataWithBytes:&n64 length:sizeof(n64)];
                }
            }

        }
    }

    // TODO: add unauth'ed field name here

    return authenticatedData;
}

#pragma mark - Utility

- (NSString*)description {
    return [NSString stringWithFormat: @"<%@: %@>", NSStringFromClass([self class]), self.uuid];
}

- (NSString*)debugDescription {
    return [NSString stringWithFormat: @"<%@: %@ %p>", NSStringFromClass([self class]), self.uuid, self];
}

- (instancetype)copyWithZone:(NSZone *)zone {
    CKKSItem *itemCopy = [super copyWithZone:zone];
    itemCopy->_uuid = _uuid;
    itemCopy->_parentKeyUUID = _parentKeyUUID;
    itemCopy->_encitem = _encitem;
    itemCopy->_wrappedkey = _wrappedkey;
    itemCopy->_generationCount = _generationCount;
    itemCopy->_encver = _encver;
    return itemCopy;
}

#pragma mark - Getters/Setters

- (NSString*) base64Item {
    return [self.encitem base64EncodedStringWithOptions:0];
}

- (void) setBase64Item: (NSString*) base64Item {
    _encitem = [[NSData alloc] initWithBase64EncodedString: base64Item options:0];
}

#pragma mark - CKKSSQLDatabaseObject helpers

// Note that CKKSItems are not intended to be saved directly, and so CKKSItem does not implement sqlTable.
// You must subclass CKKSItem to have this work correctly, although you can call back up into this class to use these if you like.

+ (NSArray<NSString*>*)sqlColumns {
    return @[@"UUID", @"parentKeyUUID", @"ckzone", @"encitem", @"wrappedkey", @"gencount", @"encver", @"ckrecord",
             @"pcss", @"pcsk", @"pcsi"];
}

- (NSDictionary<NSString*,NSString*>*)whereClauseToFindSelf {
    return @{@"UUID": self.uuid, @"ckzone":self.zoneID.zoneName};
}

- (NSDictionary<NSString*,id>*)sqlValues {
    return @{@"UUID": self.uuid,
             @"parentKeyUUID": self.parentKeyUUID,
             @"ckzone":  CKKSNilToNSNull(self.zoneID.zoneName),
             @"encitem": self.base64encitem,
             @"wrappedkey": [self.wrappedkey base64WrappedKey],
             @"gencount": [NSNumber numberWithInteger:self.generationCount],
             @"encver": [NSNumber numberWithInteger:self.encver],
             @"ckrecord": CKKSNilToNSNull([self.encodedCKRecord base64EncodedStringWithOptions:0]),
             @"pcss": CKKSNilToNSNull(self.plaintextPCSServiceIdentifier),
             @"pcsk": CKKSNilToNSNull([self.plaintextPCSPublicKey base64EncodedStringWithOptions:0]),
             @"pcsi": CKKSNilToNSNull([self.plaintextPCSPublicIdentity base64EncodedStringWithOptions:0])};
}

+ (instancetype)fromDatabaseRow: (NSDictionary*) row {
    return [[CKKSItem alloc] initWithUUID:row[@"UUID"]
                            parentKeyUUID:row[@"parentKeyUUID"]
                                   zoneID:[[CKRecordZoneID alloc] initWithZoneName: row[@"ckzone"] ownerName:CKCurrentUserDefaultName]
                          encodedCKRecord:CKKSUnbase64NullableString(row[@"ckrecord"])
                                  encItem:CKKSUnbase64NullableString(row[@"encitem"])
                               wrappedkey:CKKSIsNull(row[@"wrappedkey"]) ? nil : [[CKKSWrappedAESSIVKey alloc] initWithBase64: row[@"wrappedkey"]]
                          generationCount:[row[@"gencount"] integerValue]
                                   encver:[row[@"encver"] integerValue]
            plaintextPCSServiceIdentifier:CKKSIsNull(row[@"pcss"]) ? nil : [NSNumber numberWithInteger: [row[@"pcss"] integerValue]]
                    plaintextPCSPublicKey:CKKSUnbase64NullableString(row[@"pcsk"])
               plaintextPCSPublicIdentity:CKKSUnbase64NullableString(row[@"pcsi"])
            ];
}

@end

#pragma mark - CK-Aware Database Helpers

@implementation CKKSSQLDatabaseObject (CKKSZoneExtras)

+ (NSArray<NSString*>*)allUUIDs:(CKRecordZoneID*)zoneID error:(NSError * __autoreleasing *)error {
    __block NSMutableArray* uuids = [[NSMutableArray alloc] init];

    [CKKSSQLDatabaseObject queryDatabaseTable: [self sqlTable]
                                        where:@{@"ckzone": CKKSNilToNSNull(zoneID.zoneName)}
                                      columns: @[@"UUID"]
                                      groupBy: nil
                                      orderBy:nil
                                        limit: -1
                                   processRow: ^(NSDictionary* row) {
                                       [uuids addObject: row[@"UUID"]];
                                   }
                                        error: error];
    return uuids;
}

+ (NSArray*) all:(CKRecordZoneID*) zoneID error: (NSError * __autoreleasing *) error {
    return [self allWhere: @{@"ckzone": CKKSNilToNSNull(zoneID.zoneName)} error:error];
}

+ (bool) deleteAll:(CKRecordZoneID*) zoneID error: (NSError * __autoreleasing *) error {
    bool ok = [CKKSSQLDatabaseObject deleteFromTable:[self sqlTable] where: @{@"ckzone":CKKSNilToNSNull(zoneID.zoneName)} connection:nil error: error];

    if(ok) {
        secdebug("ckksitem", "Deleted all %@", self);
    } else {
        secdebug("ckksitem", "Couldn't delete all %@: %@", self, error ? *error : @"unknown");
    }
    return ok;
}

@end

#endif
