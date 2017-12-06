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

#import "keychain/ckks/CKKSCurrentItemPointer.h"

#if OCTAGON

@implementation CKKSCurrentItemPointer

- (instancetype)initForIdentifier:(NSString*)identifier
                  currentItemUUID:(NSString*)currentItemUUID
                            state:(CKKSProcessedState*)state
                           zoneID:(CKRecordZoneID*)zoneID
                  encodedCKRecord: (NSData*) encodedrecord
{
    if(self = [super initWithCKRecordType: SecCKRecordCurrentItemType encodedCKRecord:encodedrecord zoneID:zoneID]) {
        _state = state;
        _identifier = identifier;
        _currentItemUUID = currentItemUUID;
    }
    return self;
}

- (NSString*)description {
    return [NSString stringWithFormat:@"<CKKSCurrentItemPointer(%@) %@: %@>", self.zoneID.zoneName, self.identifier, self.currentItemUUID];
}

#pragma mark - CKKSCKRecordHolder methods

- (NSString*) CKRecordName {
    return self.identifier;
}

- (CKRecord*)updateCKRecord: (CKRecord*) record zoneID: (CKRecordZoneID*) zoneID {
    // The record name should already match identifier...
    if(![record.recordID.recordName isEqualToString: self.identifier]) {
        @throw [NSException
                exceptionWithName:@"WrongCKRecordNameException"
                reason:[NSString stringWithFormat: @"CKRecord name (%@) was not %@", record.recordID.recordName, self.identifier]
                userInfo:nil];
    }

    // Set the parent reference
    record[SecCKRecordItemRefKey] = [[CKReference alloc] initWithRecordID: [[CKRecordID alloc] initWithRecordName: self.currentItemUUID zoneID: zoneID]
                                                                   action: CKReferenceActionNone];
    return record;
}

- (bool)matchesCKRecord: (CKRecord*) record {
    if(![record.recordType isEqualToString: SecCKRecordCurrentItemType]) {
        return false;
    }

    if(![record.recordID.recordName isEqualToString: self.identifier]) {
        return false;
    }

    if(![[record[SecCKRecordItemRefKey] recordID].recordName isEqualToString: self.currentItemUUID]) {
        return false;
    }

    return true;
}

- (void)setFromCKRecord: (CKRecord*) record {
    if(![record.recordType isEqualToString: SecCKRecordCurrentItemType]) {
        @throw [NSException
                exceptionWithName:@"WrongCKRecordTypeException"
                reason:[NSString stringWithFormat: @"CKRecordType (%@) was not %@", record.recordType, SecCKRecordCurrentItemType]
                userInfo:nil];
    }

    [self setStoredCKRecord:record];

    self.identifier = (CKKSKeyClass*) record.recordID.recordName;
    self.currentItemUUID = [record[SecCKRecordItemRefKey] recordID].recordName;
}

#pragma mark - Load from database

+ (instancetype)fromDatabase:(NSString*)identifier state:(CKKSProcessedState*)state zoneID:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error {
    return [self fromDatabaseWhere: @{@"identifier":identifier, @"state":state, @"ckzone":zoneID.zoneName} error: error];
}

+ (instancetype)tryFromDatabase:(NSString*)identifier state:(CKKSProcessedState*)state zoneID:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error {
    return [self tryFromDatabaseWhere: @{@"identifier":identifier, @"state":state, @"ckzone":zoneID.zoneName} error: error];
}

+ (NSArray<CKKSCurrentItemPointer*>*)remoteItemPointers: (CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error {
    return [self allWhere: @{@"state":  SecCKKSProcessedStateRemote, @"ckzone":zoneID.zoneName} error:error];
}

+ (NSArray<CKKSCurrentItemPointer*>*)allInZone:(CKRecordZoneID*)zoneID error:(NSError * __autoreleasing *)error {
    return [self allWhere: @{@"ckzone":zoneID.zoneName} error:error];
}

+ (bool)deleteAll:(CKRecordZoneID*)zoneID error:(NSError * __autoreleasing *)error {
    bool ok = [CKKSSQLDatabaseObject deleteFromTable:[self sqlTable] where: @{@"ckzone":zoneID.zoneName} connection:nil error: error];

    if(ok) {
        secdebug("ckksitem", "Deleted all %@", self);
    } else {
        secdebug("ckksitem", "Couldn't delete all %@: %@", self, error ? *error : @"unknown");
    }
    return ok;
}

#pragma mark - CKKSSQLDatabaseObject methods

+ (NSString*)sqlTable {
    return @"currentitems";
}

+ (NSArray<NSString*>*)sqlColumns {
    return @[@"identifier", @"currentItemUUID", @"state", @"ckzone", @"ckrecord"];
}

- (NSDictionary<NSString*,NSString*>*) whereClauseToFindSelf {
    return @{@"identifier": self.identifier, @"ckzone":self.zoneID.zoneName, @"state":self.state};
}

- (NSDictionary<NSString*,NSString*>*)sqlValues {
    return @{@"identifier": self.identifier,
             @"currentItemUUID": CKKSNilToNSNull(self.currentItemUUID),
             @"state": CKKSNilToNSNull(self.state),
             @"ckzone":  CKKSNilToNSNull(self.zoneID.zoneName),
             @"ckrecord": CKKSNilToNSNull([self.encodedCKRecord base64EncodedStringWithOptions:0]),
             };
}

+ (instancetype)fromDatabaseRow: (NSDictionary*) row {
    return [[CKKSCurrentItemPointer alloc] initForIdentifier:row[@"identifier"]
                                             currentItemUUID:CKKSNSNullToNil(row[@"currentItemUUID"])
                                                       state:CKKSNSNullToNil(row[@"state"])
                                                      zoneID:[[CKRecordZoneID alloc] initWithZoneName: row[@"ckzone"] ownerName:CKCurrentUserDefaultName]
                                             encodedCKRecord:[[NSData alloc] initWithBase64EncodedString: row[@"ckrecord"] options:0]];
}

@end

#endif // OCTAGON

