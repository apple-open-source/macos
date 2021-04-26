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

#import "keychain/ckks/CKKSStates.h"
#import "keychain/categories/NSError+UsefulConstructors.h"

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
            ckkserror_global("currentkey", "created a CKKSCurrentKey with a nil currentKeyUUID. Why?");
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
    if(![record.recordType isEqualToString: SecCKRecordCurrentKeyType]) {
        @throw [NSException
                exceptionWithName:@"WrongCKRecordTypeException"
                reason:[NSString stringWithFormat: @"CKRecordType (%@) was not %@", record.recordType, SecCKRecordCurrentKeyType]
                userInfo:nil];
    }

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
        ckkserror_global("currentkey", "No current key UUID in record! How/why? %@", record);
    }
}

#pragma mark - Load from database

+ (instancetype _Nullable)fromDatabase:(CKKSKeyClass*)keyclass zoneID:(CKRecordZoneID*)zoneID error:(NSError * __autoreleasing *)error
{
    return [self fromDatabaseWhere: @{@"keyclass": keyclass, @"ckzone":zoneID.zoneName} error: error];
}

+ (instancetype _Nullable)tryFromDatabase:(CKKSKeyClass*)keyclass zoneID:(CKRecordZoneID*)zoneID error:(NSError * __autoreleasing *)error
{
    return [self tryFromDatabaseWhere: @{@"keyclass": keyclass, @"ckzone":zoneID.zoneName} error: error];
}

+ (instancetype _Nullable)forKeyClass:(CKKSKeyClass*)keyclass withKeyUUID:(NSString*)keyUUID zoneID:(CKRecordZoneID*)zoneID error:(NSError * __autoreleasing *)error
{
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

+ (instancetype)fromDatabaseRow:(NSDictionary<NSString*, CKKSSQLResult*>*)row {
    return [[CKKSCurrentKeyPointer alloc] initForClass:(CKKSKeyClass*)row[@"keyclass"].asString
                                        currentKeyUUID:row[@"currentKeyUUID"].asString
                                                zoneID:[[CKRecordZoneID alloc] initWithZoneName:row[@"ckzone"].asString ownerName:CKCurrentUserDefaultName]
                                       encodedCKRecord:row[@"ckrecord"].asBase64DecodedData];
}

+ (BOOL)intransactionRecordChanged:(CKRecord*)record
                            resync:(BOOL)resync
                       flagHandler:(id<OctagonStateFlagHandler>)flagHandler
                             error:(NSError**)error
{
    // Pull out the old CKP, if it exists
    NSError* ckperror = nil;
    CKKSCurrentKeyPointer* oldckp = [CKKSCurrentKeyPointer tryFromDatabase:((CKKSKeyClass*)record.recordID.recordName) zoneID:record.recordID.zoneID error:&ckperror];
    if(ckperror) {
        ckkserror("ckkskey", record.recordID.zoneID, "error loading ckp: %@", ckperror);
    }

    if(resync) {
        if(!oldckp) {
            ckkserror("ckksresync", record.recordID.zoneID, "BUG: No current key pointer matching resynced CloudKit record: %@", record);
        } else if(![oldckp matchesCKRecord:record]) {
            ckkserror("ckksresync", record.recordID.zoneID, "BUG: Local current key pointer doesn't match resynced CloudKit record: %@ %@", oldckp, record);
        } else {
            ckksnotice("ckksresync", record.recordID.zoneID, "Current key pointer has 'changed', but it matches our local copy: %@", record);
        }
    }

    NSError* localerror = nil;
    CKKSCurrentKeyPointer* currentkey = [[CKKSCurrentKeyPointer alloc] initWithCKRecord:record];

    bool saved = [currentkey saveToDatabase:&localerror];
    if(!saved || localerror != nil) {
        ckkserror("ckkskey", record.recordID.zoneID, "Couldn't save current key pointer to database: %@: %@", currentkey, localerror);
        ckksinfo("ckkskey", record.recordID.zoneID, "CKRecord was %@", record);

        if(error) {
            *error = localerror;
        }
        return NO;
    }

    if([oldckp matchesCKRecord:record]) {
        ckksnotice("ckkskey", record.recordID.zoneID, "Current key pointer modification doesn't change anything interesting; skipping reprocess: %@", record);
    } else {
        // We've saved a new key in the database; trigger a rekey operation.
        [flagHandler _onqueueHandleFlag:CKKSFlagKeyStateProcessRequested];
    }

    return YES;
}

@end

@implementation CKKSCurrentKeySet
-(instancetype)initForZoneName:(NSString*)zoneName {
    if((self = [super init])) {
        _viewName = zoneName;
    }

    return self;
}

+ (CKKSCurrentKeySet*)loadForZone:(CKRecordZoneID*)zoneID
{
    CKKSCurrentKeySet* set = [[CKKSCurrentKeySet alloc] initForZoneName:zoneID.zoneName];
    NSError* error = nil;

    set.currentTLKPointer    = [CKKSCurrentKeyPointer tryFromDatabase: SecCKKSKeyClassTLK zoneID:zoneID error:&error];
    set.currentClassAPointer = [CKKSCurrentKeyPointer tryFromDatabase: SecCKKSKeyClassA   zoneID:zoneID error:&error];
    set.currentClassCPointer = [CKKSCurrentKeyPointer tryFromDatabase: SecCKKSKeyClassC   zoneID:zoneID error:&error];

    set.tlk    = set.currentTLKPointer.currentKeyUUID    ? [CKKSKey tryFromDatabase:set.currentTLKPointer.currentKeyUUID    zoneID:zoneID error:&error] : nil;
    set.classA = set.currentClassAPointer.currentKeyUUID ? [CKKSKey tryFromDatabase:set.currentClassAPointer.currentKeyUUID zoneID:zoneID error:&error] : nil;
    set.classC = set.currentClassCPointer.currentKeyUUID ? [CKKSKey tryFromDatabase:set.currentClassCPointer.currentKeyUUID zoneID:zoneID error:&error] : nil;

    set.tlkShares = [CKKSTLKShareRecord allForUUID:set.currentTLKPointer.currentKeyUUID zoneID:zoneID error:&error];
    set.pendingTLKShares = nil;

    set.proposed = NO;

    set.error = error;

    return set;
}

-(NSString*)description {
    if(self.error) {
        return [NSString stringWithFormat:@"<CKKSCurrentKeySet(%@): %@:%@ %@:%@ %@:%@ new:%d %@>",
                self.viewName,
                self.currentTLKPointer.currentKeyUUID, self.tlk,
                self.currentClassAPointer.currentKeyUUID, self.classA,
                self.currentClassCPointer.currentKeyUUID, self.classC,
                self.proposed,
                self.error];

    } else {
        return [NSString stringWithFormat:@"<CKKSCurrentKeySet(%@): %@:%@ %@:%@ %@:%@ new:%d>",
                self.viewName,
                self.currentTLKPointer.currentKeyUUID, self.tlk,
                self.currentClassAPointer.currentKeyUUID, self.classA,
                self.currentClassCPointer.currentKeyUUID, self.classC,
                self.proposed];
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
    copy.proposed = self.proposed;

    copy.error = [self.error copyWithZone:zone];
    return copy;
}

- (CKKSKeychainBackedKeySet* _Nullable)asKeychainBackedSet:(NSError**)error
{
    if(!self.tlk.keycore ||
       !self.classA.keycore ||
       !self.classC.keycore) {
        if(error) {
            *error = [NSError errorWithDomain:CKKSErrorDomain
                                         code:CKKSKeysMissing
                                  description:@"unable to make keychain backed set; key is missing"];
        }
        return nil;
    }

    return [[CKKSKeychainBackedKeySet alloc] initWithTLK:self.tlk.keycore
                                                  classA:self.classA.keycore
                                                  classC:self.classC.keycore
                                               newUpload:self.proposed];
}
@end

#endif // OCTAGON
