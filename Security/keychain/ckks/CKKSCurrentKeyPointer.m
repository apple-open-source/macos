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

#import "CKKSCurrentKeyPointer.h"

#if OCTAGON

@implementation CKKSCurrentKeyPointer

- (instancetype)initForClass:(CKKSKeyClass*)keyclass
              currentKeyUUID:(NSString*)currentKeyUUID
                      zoneID:(CKRecordZoneID*)zoneID
             encodedCKRecord: (NSData*) encodedrecord
{
    if(self = [super initWithCKRecordType: SecCKRecordCurrentKeyType encodedCKRecord:encodedrecord zoneID:zoneID]) {
        _keyclass = keyclass;
        _currentKeyUUID = currentKeyUUID;

        if(self.currentKeyUUID == nil) {
            secerror("ckkscurrentkey: created a CKKSCurrentKey with a nil currentKeyUUID. Why?");
        }
    }
    return self;
}

- (NSString*)description {
    return [NSString stringWithFormat:@"<CKKSCurrentKeyPointer(%@) %@: %@>", self.zoneID.zoneName, self.keyclass, self.currentKeyUUID];
}

- (instancetype)copyWithZone:(NSZone*)zone {
    CKKSCurrentKeyPointer* copy = [super copyWithZone:zone];
    copy.keyclass = [self.keyclass copyWithZone:zone];
    copy.currentKeyUUID = [self.currentKeyUUID copyWithZone:zone];
    return copy;
}
- (BOOL)isEqual: (id) object {
    if(![object isKindOfClass:[CKKSCurrentKeyPointer class]]) {
        return NO;
    }

    CKKSCurrentKeyPointer* obj = (CKKSCurrentKeyPointer*) object;

    return ([self.zoneID isEqual: obj.zoneID] &&
            ((self.currentKeyUUID == nil && obj.currentKeyUUID == nil) || [self.currentKeyUUID isEqual: obj.currentKeyUUID]) &&
            ((self.keyclass == nil && obj.keyclass == nil)             || [self.keyclass isEqual:obj.keyclass]) &&
            YES) ? YES : NO;
}

#pragma mark - CKKSCKRecordHolder methods

- (NSString*) CKRecordName {
    return self.keyclass;
}

- (CKRecord*) updateCKRecord: (CKRecord*) record zoneID: (CKRecordZoneID*) zoneID {
    // The record name should already match keyclass...
    if(![record.recordID.recordName isEqualToString: self.keyclass]) {
        @throw [NSException
                exceptionWithName:@"WrongCKRecordNameException"
                reason:[NSString stringWithFormat: @"CKRecord name (%@) was not %@", record.recordID.recordName, self.keyclass]
                userInfo:nil];
    }

    // Set the parent reference
    record[SecCKRecordParentKeyRefKey] = [[CKReference alloc] initWithRecordID: [[CKRecordID alloc] initWithRecordName: self.currentKeyUUID zoneID: zoneID] action: CKReferenceActionNone];
    return record;
}

- (bool) matchesCKRecord: (CKRecord*) record {
    if(![record.recordType isEqualToString: SecCKRecordCurrentKeyType]) {
        return false;
    }

    if(![record.recordID.recordName isEqualToString: self.keyclass]) {
        return false;
    }

    if(![[record[SecCKRecordParentKeyRefKey] recordID].recordName isEqualToString: self.currentKeyUUID]) {
        return false;
    }

    return true;
}

- (void) setFromCKRecord: (CKRecord*) record {
    if(![record.recordType isEqualToString: SecCKRecordCurrentKeyType]) {
        @throw [NSException
                exceptionWithName:@"WrongCKRecordTypeException"
                reason:[NSString stringWithFormat: @"CKRecordType (%@) was not %@", record.recordType, SecCKRecordCurrentKeyType]
                userInfo:nil];
    }

    [self setStoredCKRecord:record];

    // TODO: verify this is a real keyclass
    self.keyclass = (CKKSKeyClass*) record.recordID.recordName;
    self.currentKeyUUID = [record[SecCKRecordParentKeyRefKey] recordID].recordName;

    if(self.currentKeyUUID == nil) {
        secerror("ckkscurrentkey: No current key UUID in record! How/why? %@", record);
    }
}

#pragma mark - Load from database

+ (instancetype) fromDatabase: (CKKSKeyClass*) keyclass zoneID:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error {
    return [self fromDatabaseWhere: @{@"keyclass": keyclass, @"ckzone":zoneID.zoneName} error: error];
}

+ (instancetype) tryFromDatabase: (CKKSKeyClass*) keyclass zoneID:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error {
    return [self tryFromDatabaseWhere: @{@"keyclass": keyclass, @"ckzone":zoneID.zoneName} error: error];
}

+ (instancetype) forKeyClass: (CKKSKeyClass*) keyclass withKeyUUID: (NSString*) keyUUID zoneID:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error {
    NSError* localerror = nil;
    CKKSCurrentKeyPointer* current = [self tryFromDatabase: keyclass zoneID:zoneID error: &localerror];
    if(localerror) {
        if(error) {
            *error = localerror;
        }
        return nil;
    }

    if(current) {
        current.currentKeyUUID = keyUUID;
        return current;
    }

    return [[CKKSCurrentKeyPointer alloc] initForClass: keyclass currentKeyUUID: keyUUID zoneID:zoneID encodedCKRecord:nil];
}

+ (NSArray<CKKSCurrentKeyPointer*>*)all:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error {
    return [self allWhere:@{@"ckzone":zoneID.zoneName} error:error];
}

+ (bool) deleteAll:(CKRecordZoneID*) zoneID error: (NSError * __autoreleasing *) error {
    bool ok = [CKKSSQLDatabaseObject deleteFromTable:[self sqlTable] where: @{@"ckzone":zoneID.zoneName} connection:nil error: error];

    if(ok) {
        secdebug("ckksitem", "Deleted all %@", self);
    } else {
        secdebug("ckksitem", "Couldn't delete all %@: %@", self, error ? *error : @"unknown");
    }
    return ok;
}

#pragma mark - CKKSSQLDatabaseObject methods

+ (NSString*) sqlTable {
    return @"currentkeys";
}

+ (NSArray<NSString*>*) sqlColumns {
    return @[@"keyclass", @"currentKeyUUID", @"ckzone", @"ckrecord"];
}

- (NSDictionary<NSString*,NSString*>*) whereClauseToFindSelf {
    return @{@"keyclass": self.keyclass, @"ckzone":self.zoneID.zoneName};
}

- (NSDictionary<NSString*,NSString*>*) sqlValues {
    return @{@"keyclass": self.keyclass,
             @"currentKeyUUID": CKKSNilToNSNull(self.currentKeyUUID),
             @"ckzone":  CKKSNilToNSNull(self.zoneID.zoneName),
             @"ckrecord": CKKSNilToNSNull([self.encodedCKRecord base64EncodedStringWithOptions:0]),
             };
}

+ (instancetype) fromDatabaseRow: (NSDictionary*) row {
    return [[CKKSCurrentKeyPointer alloc] initForClass: row[@"keyclass"]
                                        currentKeyUUID: [row[@"currentKeyUUID"] isEqual: [NSNull null]] ? nil : row[@"currentKeyUUID"]
                                                zoneID: [[CKRecordZoneID alloc] initWithZoneName: row[@"ckzone"] ownerName:CKCurrentUserDefaultName]
                                       encodedCKRecord: [[NSData alloc] initWithBase64EncodedString: row[@"ckrecord"] options:0]];
}

@end

@implementation CKKSCurrentKeySet
-(instancetype)init {
    if((self = [super init])) {
    }

    return self;
}
-(instancetype)initForZone:(CKRecordZoneID*)zoneID {
    if((self = [super init])) {
        NSError* error = nil;
        _currentTLKPointer    = [CKKSCurrentKeyPointer tryFromDatabase: SecCKKSKeyClassTLK zoneID:zoneID error:&error];
        _currentClassAPointer = [CKKSCurrentKeyPointer tryFromDatabase: SecCKKSKeyClassA   zoneID:zoneID error:&error];
        _currentClassCPointer = [CKKSCurrentKeyPointer tryFromDatabase: SecCKKSKeyClassC   zoneID:zoneID error:&error];

        _tlk    = _currentTLKPointer.currentKeyUUID    ? [CKKSKey tryFromDatabase:_currentTLKPointer.currentKeyUUID    zoneID:zoneID error:&error] : nil;
        _classA = _currentClassAPointer.currentKeyUUID ? [CKKSKey tryFromDatabase:_currentClassAPointer.currentKeyUUID zoneID:zoneID error:&error] : nil;
        _classC = _currentClassCPointer.currentKeyUUID ? [CKKSKey tryFromDatabase:_currentClassCPointer.currentKeyUUID zoneID:zoneID error:&error] : nil;

        _tlkShares = [CKKSTLKShare allForUUID:_currentTLKPointer.currentKeyUUID zoneID:zoneID error:&error];

        _error = error;

    }

    return self;
}
-(NSString*)description {
    if(self.error) {
        return [NSString stringWithFormat:@"<CKKSCurrentKeySet: %@:%@ %@:%@ %@:%@ %@>",
                self.currentTLKPointer.currentKeyUUID, self.tlk,
                self.currentClassAPointer.currentKeyUUID, self.classA,
                self.currentClassCPointer.currentKeyUUID, self.classC,
                self.error];

    } else {
        return [NSString stringWithFormat:@"<CKKSCurrentKeySet: %@:%@ %@:%@ %@:%@>",
                self.currentTLKPointer.currentKeyUUID, self.tlk,
                self.currentClassAPointer.currentKeyUUID, self.classA,
                self.currentClassCPointer.currentKeyUUID, self.classC];
    }
}
- (instancetype)copyWithZone:(NSZone*)zone {
    CKKSCurrentKeySet* copy = [[[self class] alloc] init];
    copy.currentTLKPointer = [self.currentTLKPointer copyWithZone:zone];
    copy.currentClassAPointer = [self.currentClassAPointer copyWithZone:zone];
    copy.currentClassCPointer = [self.currentClassCPointer copyWithZone:zone];
    copy.tlk = [self.tlk copyWithZone:zone];
    copy.classA = [self.classA copyWithZone:zone];
    copy.classC = [self.classC copyWithZone:zone];

    copy.error = [self.error copyWithZone:zone];
    return copy;
}
@end

#endif // OCTAGON
