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

#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>
#import "keychain/ckks/CKKSDeviceStateEntry.h"
#import "keychain/ckks/CKKSKeychainView.h"
#include <Security/SecureObjectSync/SOSAccount.h>

@implementation CKKSDeviceStateEntry

- (instancetype)initForDevice:(NSString*)device
                    osVersion:(NSString*)osVersion
               lastUnlockTime:(NSDate*)lastUnlockTime
                 circlePeerID:(NSString*)circlePeerID
                 circleStatus:(SOSCCStatus)circleStatus
                     keyState:(CKKSZoneKeyState*)keyState
               currentTLKUUID:(NSString*)currentTLKUUID
            currentClassAUUID:(NSString*)currentClassAUUID
            currentClassCUUID:(NSString*)currentClassCUUID
                       zoneID:(CKRecordZoneID*)zoneID
              encodedCKRecord:(NSData*)encodedrecord
{
    if((self = [super initWithCKRecordType:SecCKRecordDeviceStateType
                           encodedCKRecord:encodedrecord
                                    zoneID:zoneID])) {
        _device = device;
        _osVersion = osVersion;
        _lastUnlockTime = lastUnlockTime;

        _circleStatus = circleStatus;
        _keyState = keyState;

        _circlePeerID = circlePeerID;

        _currentTLKUUID = currentTLKUUID;
        _currentClassAUUID = currentClassAUUID;
        _currentClassCUUID = currentClassCUUID;
    }
    return self;
}

#define kSOSCCErrorPositive 111

-(id)sosCCStatusToCKType:(SOSCCStatus)status {
    // kSOSCCError is -1, but without a size.
    // make it a special number
    if(status == kSOSCCError) {
        [NSNumber numberWithInt:kSOSCCErrorPositive];
    }
    return [NSNumber numberWithInt:status];
}

-(SOSCCStatus)cktypeToSOSCCStatus:(id)object {
    if(![object isKindOfClass:[NSNumber class]]) {
        return kSOSCCError;
    }
    NSNumber* number = (NSNumber*)object;

    uint32_t n = [number unsignedIntValue];

    switch(n) {
        case (uint32_t)kSOSCCInCircle:
            return kSOSCCInCircle;
        case (uint32_t)kSOSCCNotInCircle:
            return kSOSCCNotInCircle;
        case (uint32_t)kSOSCCRequestPending:
            return kSOSCCRequestPending;
        case (uint32_t)kSOSCCCircleAbsent:
            return kSOSCCCircleAbsent;
        case (uint32_t)kSOSCCErrorPositive: // Use the magic number
            return kSOSCCError;
        case (uint32_t)kSOSCCError: // And, if by some miracle, you end up with -1 as a uint32_t, accept that too
            return kSOSCCError;
        default:
            secerror("ckks: %d is not an SOSCCStatus?", n);
            return kSOSCCError;
    }
}

+(NSString*)nameFromCKRecordID:(CKRecordID*)recordID {
    // Strip off the prefix from the recordName
    NSString* prefix = @"ckid-";
    NSString* name = recordID.recordName;

    if ([name hasPrefix:prefix]) {
        name = [name substringFromIndex:[prefix length]];
    }
    return name;
}

-(NSString*)description {
    NSDate* updated = self.storedCKRecord.modificationDate;

    return [NSString stringWithFormat:@"<CKKSDeviceStateEntry(%@,%@,%@,%@,%@): %@ %@ %@ %@ %@ upd:%@>",
            self.device,
            self.circlePeerID,
            self.osVersion,
            self.lastUnlockTime,
            self.zoneID.zoneName,
            SOSAccountGetSOSCCStatusString(self.circleStatus),
            self.keyState,
            self.currentTLKUUID,
            self.currentClassAUUID,
            self.currentClassCUUID,
            updated ? updated : @"unknown"
            ];
}

- (BOOL)isEqual: (id) object {
    if(![object isKindOfClass:[CKKSDeviceStateEntry class]]) {
        return NO;
    }

    CKKSDeviceStateEntry* obj = (CKKSDeviceStateEntry*) object;

    return ([self.zoneID isEqual: obj.zoneID] &&
            ((self.device == nil && obj.device == nil)                       || [self.device isEqual: obj.device]) &&
            ((self.osVersion == nil && obj.osVersion == nil)                 || [self.osVersion isEqual:obj.osVersion]) &&
            ((self.lastUnlockTime == nil && obj.lastUnlockTime == nil)       || [self.lastUnlockTime isEqual:obj.lastUnlockTime]) &&
            ((self.circlePeerID == nil && obj.circlePeerID == nil)           || [self.circlePeerID isEqual: obj.circlePeerID]) &&
            (self.circleStatus == obj.circleStatus) &&
            ((self.keyState == nil && obj.keyState == nil)                   || [self.keyState isEqual: obj.keyState]) &&
            ((self.currentTLKUUID == nil && obj.currentTLKUUID == nil)       || [self.currentTLKUUID isEqual: obj.currentTLKUUID]) &&
            ((self.currentClassAUUID == nil && obj.currentClassAUUID == nil) || [self.currentClassAUUID isEqual: obj.currentClassAUUID]) &&
            ((self.currentClassCUUID == nil && obj.currentClassCUUID == nil) || [self.currentClassCUUID isEqual: obj.currentClassCUUID]) &&
            YES) ? YES : NO;
}

#pragma mark - Database Operations

+ (instancetype)fromDatabase:(NSString*)device zoneID:(CKRecordZoneID*)zoneID error:(NSError * __autoreleasing *)error {
    return [self fromDatabaseWhere: @{@"device":CKKSNilToNSNull(device), @"ckzone": CKKSNilToNSNull(zoneID.zoneName)} error:error];
}

+ (instancetype)tryFromDatabase:(NSString*)device zoneID:(CKRecordZoneID*)zoneID error:(NSError * __autoreleasing *)error {
    return [self tryFromDatabaseWhere: @{@"device":CKKSNilToNSNull(device), @"ckzone": CKKSNilToNSNull(zoneID.zoneName)} error:error];
}

+ (instancetype)tryFromDatabaseFromCKRecordID:(CKRecordID*)recordID error:(NSError * __autoreleasing *)error {
    return [self tryFromDatabaseWhere: @{@"device":CKKSNilToNSNull([self nameFromCKRecordID:recordID]), @"ckzone": CKKSNilToNSNull(recordID.zoneID.zoneName)} error:error];
}

+ (NSArray<CKKSDeviceStateEntry*>*)allInZone:(CKRecordZoneID*)zoneID error:(NSError * __autoreleasing *)error {
    return [self allWhere:@{@"ckzone": CKKSNilToNSNull(zoneID.zoneName)} error:error];
}

#pragma mark - CKKSCKRecordHolder methods

- (NSString*)CKRecordName {
    return [NSString stringWithFormat:@"ckid-%@", self.device];
}

- (CKRecord*)updateCKRecord: (CKRecord*) record zoneID: (CKRecordZoneID*) zoneID {
    if(![record.recordID.recordName isEqualToString: [self CKRecordName]]) {
        @throw [NSException
                exceptionWithName:@"WrongCKRecordNameException"
                reason:[NSString stringWithFormat: @"CKRecord name (%@) was not %@", record.recordID.recordName, [self CKRecordName]]
                userInfo:nil];
    }
    if(![record.recordType isEqualToString: SecCKRecordDeviceStateType]) {
        @throw [NSException
                exceptionWithName:@"WrongCKRecordTypeException"
                reason:[NSString stringWithFormat: @"CKRecordType (%@) was not %@", record.recordType, SecCKRecordDeviceStateType]
                userInfo:nil];
    }

    record[SecCKSRecordOSVersionKey] = self.osVersion;
    record[SecCKSRecordLastUnlockTime] = self.lastUnlockTime;

    record[SecCKRecordCircleStatus] = [self sosCCStatusToCKType: self.circleStatus];
    record[SecCKRecordKeyState] = CKKSZoneKeyToNumber(self.keyState);

    record[SecCKRecordCirclePeerID] = self.circlePeerID;

#define CKKeyRef(uuid) (!uuid ? nil : [[CKReference alloc] initWithRecordID:[[CKRecordID alloc] initWithRecordName:uuid \
                                       zoneID:self.zoneID] \
                                       action: CKReferenceActionNone])

    record[SecCKRecordCurrentTLK]    = CKKeyRef(self.currentTLKUUID);
    record[SecCKRecordCurrentClassA] = CKKeyRef(self.currentClassAUUID);
    record[SecCKRecordCurrentClassC] = CKKeyRef(self.currentClassCUUID);

    return record;
}

- (bool)matchesCKRecord: (CKRecord*) record {
    if(![record.recordType isEqualToString: SecCKRecordDeviceStateType]) {
        return false;
    }

    if(![record.recordID.recordName isEqualToString: [self CKRecordName]]) {
        return false;
    }

    if((!(self.lastUnlockTime == nil && record[SecCKSRecordLastUnlockTime] == nil)) &&
       ![record[SecCKSRecordLastUnlockTime] isEqual: self.lastUnlockTime]) {
        return false;
    }

    if((!(self.osVersion == nil && record[SecCKSRecordOSVersionKey] == nil)) &&
       ![record[SecCKSRecordOSVersionKey] isEqualToString: self.osVersion]) {
        return false;
    }

    if((!(self.circlePeerID == nil && record[SecCKRecordCirclePeerID] == nil)) &&
       ![record[SecCKRecordCirclePeerID] isEqualToString: self.circlePeerID]) {
        return false;
    }

    if([self cktypeToSOSCCStatus: record[SecCKRecordCircleStatus]] != self.circleStatus) {
        return false;
    }

    if(![CKKSZoneKeyRecover(record[SecCKRecordKeyState]) isEqualToString: self.keyState]) {
        return false;
    }

    if(![[[record[SecCKRecordCurrentTLK] recordID] recordName] isEqualToString: self.currentTLKUUID]) {
        return false;
    }
    if(![[[record[SecCKRecordCurrentClassA] recordID] recordName] isEqualToString: self.currentTLKUUID]) {
        return false;
    }
    if(![[[record[SecCKRecordCurrentClassC] recordID] recordName] isEqualToString: self.currentTLKUUID]) {
        return false;
    }

    return true;
}

- (void)setFromCKRecord: (CKRecord*) record {
    if(![record.recordType isEqualToString: SecCKRecordDeviceStateType]) {
        @throw [NSException
                exceptionWithName:@"WrongCKRecordTypeException"
                reason:[NSString stringWithFormat: @"CKRecordType (%@) was not %@", record.recordType, SecCKRecordDeviceStateType]
                userInfo:nil];
    }

    [self setStoredCKRecord:record];

    self.osVersion = record[SecCKSRecordOSVersionKey];
    self.lastUnlockTime = record[SecCKSRecordLastUnlockTime];
    self.device = [CKKSDeviceStateEntry nameFromCKRecordID: record.recordID];

    self.circlePeerID = record[SecCKRecordCirclePeerID];

    self.circleStatus = [self cktypeToSOSCCStatus:record[SecCKRecordCircleStatus]];
    self.keyState = CKKSZoneKeyRecover(record[SecCKRecordKeyState]);

    self.currentTLKUUID    = [[record[SecCKRecordCurrentTLK]    recordID] recordName];
    self.currentClassAUUID = [[record[SecCKRecordCurrentClassA] recordID] recordName];
    self.currentClassCUUID = [[record[SecCKRecordCurrentClassC] recordID] recordName];
}

#pragma mark - CKKSSQLDatabaseObject methods

+ (NSString*)sqlTable {
    return @"ckdevicestate";
}

+ (NSArray<NSString*>*)sqlColumns {
    return @[@"device", @"ckzone", @"osversion", @"lastunlock", @"peerid", @"circlestatus", @"keystate", @"currentTLK", @"currentClassA", @"currentClassC", @"ckrecord"];
}

- (NSDictionary<NSString*,NSString*>*)whereClauseToFindSelf {
    return @{@"device":self.device, @"ckzone":self.zoneID.zoneName};
}

- (NSDictionary<NSString*,NSString*>*)sqlValues {
    NSISO8601DateFormatter* dateFormat = [[NSISO8601DateFormatter alloc] init];

    return @{@"device":        self.device,
             @"ckzone":        CKKSNilToNSNull(self.zoneID.zoneName),
             @"osversion":     CKKSNilToNSNull(self.osVersion),
             @"lastunlock":    CKKSNilToNSNull(self.lastUnlockTime ? [dateFormat stringFromDate:self.lastUnlockTime] : nil),
             @"peerid":        CKKSNilToNSNull(self.circlePeerID),
             @"circlestatus":  (__bridge NSString*)SOSAccountGetSOSCCStatusString(self.circleStatus),
             @"keystate":      CKKSNilToNSNull(self.keyState),
             @"currentTLK":    CKKSNilToNSNull(self.currentTLKUUID),
             @"currentClassA": CKKSNilToNSNull(self.currentClassAUUID),
             @"currentClassC": CKKSNilToNSNull(self.currentClassCUUID),
             @"ckrecord":      CKKSNilToNSNull([self.encodedCKRecord base64EncodedStringWithOptions:0]),
             };
}

+ (instancetype)fromDatabaseRow:(NSDictionary*)row {
    NSISO8601DateFormatter* dateFormat = [[NSISO8601DateFormatter alloc] init];

    return [[CKKSDeviceStateEntry alloc] initForDevice:row[@"device"]
                                             osVersion:CKKSNSNullToNil(row[@"osversion"])
                                        lastUnlockTime:[row[@"lastunlock"] isEqual: [NSNull null]] ? nil : [dateFormat dateFromString: row[@"lastunlock"]]
                                          circlePeerID:CKKSNSNullToNil(row[@"peerid"])
                                          circleStatus:SOSAccountGetSOSCCStatusFromString((__bridge CFStringRef) CKKSNSNullToNil(row[@"circlestatus"]))
                                              keyState:CKKSNSNullToNil(row[@"keystate"])
                                        currentTLKUUID:CKKSNSNullToNil(row[@"currentTLK"])
                                     currentClassAUUID:CKKSNSNullToNil(row[@"currentClassA"])
                                     currentClassCUUID:CKKSNSNullToNil(row[@"currentClassC"])
                                                zoneID:[[CKRecordZoneID alloc] initWithZoneName: row[@"ckzone"] ownerName:CKCurrentUserDefaultName]
                                       encodedCKRecord:CKKSUnbase64NullableString(row[@"ckrecord"])
            ];
}

@end

#endif

