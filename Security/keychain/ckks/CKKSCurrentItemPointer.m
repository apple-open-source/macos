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
    if(![record.recordType isEqualToString: SecCKRecordCurrentItemType]) {
        @throw [NSException
                exceptionWithName:@"WrongCKRecordTypeException"
                reason:[NSString stringWithFormat: @"CKRecordType (%@) was not %@", record.recordType, SecCKRecordCurrentItemType]
                userInfo:nil];
    }

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

+ (instancetype)fromDatabaseRow:(NSDictionary<NSString *, CKKSSQLResult*>*) row {
    return [[CKKSCurrentItemPointer alloc] initForIdentifier:row[@"identifier"].asString
                                             currentItemUUID:row[@"currentItemUUID"].asString
                                                       state:(CKKSProcessedState*)row[@"state"].asString
                                                      zoneID:[[CKRecordZoneID alloc] initWithZoneName:row[@"ckzone"].asString ownerName:CKCurrentUserDefaultName]
                                             encodedCKRecord:row[@"ckrecord"].asBase64DecodedData];
}

+ (BOOL)intransactionRecordChanged:(CKRecord*)record resync:(BOOL)resync error:(NSError**)error
{
    if(resync) {
        NSError* ciperror = nil;
        CKKSCurrentItemPointer* localcip  = [CKKSCurrentItemPointer tryFromDatabase:record.recordID.recordName state:SecCKKSProcessedStateLocal  zoneID:record.recordID.zoneID error:&ciperror];
        CKKSCurrentItemPointer* remotecip = [CKKSCurrentItemPointer tryFromDatabase:record.recordID.recordName state:SecCKKSProcessedStateRemote zoneID:record.recordID.zoneID error:&ciperror];
        if(ciperror) {
            ckkserror("ckksresync", record.recordID.zoneID, "error loading cip: %@", ciperror);
        }
        if(!(localcip || remotecip)) {
            ckkserror("ckksresync", record.recordID.zoneID, "BUG: No current item pointer matching resynced CloudKit record: %@", record);
        } else if(! ([localcip matchesCKRecord:record] || [remotecip matchesCKRecord:record]) ) {
            ckkserror("ckksresync", record.recordID.zoneID, "BUG: Local current item pointer doesn't match resynced CloudKit record(s): %@ %@ %@", localcip, remotecip, record);
        } else {
            ckksnotice("ckksresync", record.recordID.zoneID, "Already know about this current item pointer, skipping update: %@", record);
            return YES;
        }
    }

    NSError* localerror = nil;
    CKKSCurrentItemPointer* cip = [[CKKSCurrentItemPointer alloc] initWithCKRecord:record];
    cip.state = SecCKKSProcessedStateRemote;

    bool saved = [cip saveToDatabase:&localerror];
    if(!saved || localerror) {
        ckkserror("currentitem", record.recordID.zoneID, "Couldn't save current item pointer to database: %@: %@ %@", cip, localerror, record);
        if(error) {
            *error = localerror;
        }
        return NO;
    }
    return YES;
}

+ (BOOL)intransactionRecordDeleted:(CKRecordID*)recordID resync:(BOOL)resync error:(NSError**)error
{
    NSError* localerror = nil;
    ckksinfo("currentitem", recordID.zoneID, "CloudKit notification: deleted current item pointer(%@): %@", SecCKRecordCurrentItemType, recordID);

    CKKSCurrentItemPointer* remote = [CKKSCurrentItemPointer tryFromDatabase:[recordID recordName] state:SecCKKSProcessedStateRemote zoneID:recordID.zoneID error:&localerror];
    if(localerror) {
        if(error) {
            *error = localerror;
        }

        ckkserror("currentitem", recordID.zoneID, "Failed to find remote CKKSCurrentItemPointer to delete %@: %@", recordID, localerror);
        return NO;
    }

    [remote deleteFromDatabase:&localerror];
    if(localerror) {
        if(error) {
            *error = localerror;
        }
        ckkserror("currentitem", recordID.zoneID, "Failed to delete remote CKKSCurrentItemPointer %@: %@", recordID, localerror);
        return NO;
    }

    CKKSCurrentItemPointer* local = [CKKSCurrentItemPointer tryFromDatabase:[recordID recordName] state:SecCKKSProcessedStateLocal zoneID:recordID.zoneID error:&localerror];
    if(localerror) {
        if(error) {
            *error = localerror;
        }
        ckkserror("currentitem", recordID.zoneID, "Failed to find local CKKSCurrentItemPointer %@: %@", recordID, localerror);
        return NO;
    }
    [local deleteFromDatabase:&localerror];
    if(localerror) {
        if(error) {
            *error = localerror;
        }
        ckkserror("currentitem", recordID.zoneID, "Failed to delete local CKKSCurrentItemPointer %@: %@", recordID, localerror);
        return NO;
    }

    ckksinfo("currentitem", recordID.zoneID, "CKKSCurrentItemPointer was deleted: %@ error: %@", recordID, localerror);

    if(error && localerror) {
        *error = localerror;
    }

    return (localerror == nil);
}

@end

#endif // OCTAGON

