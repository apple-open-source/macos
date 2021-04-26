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
#include "keychain/SecureObjectSync/SOSAccount.h"

@implementation CKKSDeviceStateEntry

- (instancetype)initForDevice:(NSString*)device
                    osVersion:(NSString*)osVersion
               lastUnlockTime:(NSDate*)lastUnlockTime
                octagonPeerID:(NSString*)octagonPeerID
                octagonStatus:(OTCliqueStatusWrapper*)octagonStatus
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

        _octagonPeerID = octagonPeerID;
        _octagonStatus = octagonStatus;

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
        return [NSNumber numberWithInt:kSOSCCErrorPositive];
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
            ckkserror_global("ckks", "%d is not an SOSCCStatus?", n);
            return kSOSCCError;
    }
}

- (id)cliqueStatusToCKType:(OTCliqueStatusWrapper* _Nullable)status {
    if(!status) {
        return nil;
    }
    // kSOSCCError is -1, but without a size.
    // make it a special number
    if(status.status == CliqueStatusError) {
        return [NSNumber numberWithInt:kSOSCCErrorPositive];
    }
    return [NSNumber numberWithInt:(int)status.status];
}

- (OTCliqueStatusWrapper* _Nullable)cktypeToOTCliqueStatusWrapper:(id)object {
    if(object == nil) {
        return nil;
    }
    if(![object isKindOfClass:[NSNumber class]]) {
        return nil;
    }
    NSNumber* number = (NSNumber*)object;

    uint32_t n = [number unsignedIntValue];

    switch(n) {
        case (uint32_t)CliqueStatusIn:
            return [[OTCliqueStatusWrapper alloc] initWithStatus:CliqueStatusIn];
        case (uint32_t)CliqueStatusNotIn:
            return [[OTCliqueStatusWrapper alloc] initWithStatus:CliqueStatusNotIn];
        case (uint32_t)CliqueStatusPending:
            return [[OTCliqueStatusWrapper alloc] initWithStatus:CliqueStatusPending];
        case (uint32_t)CliqueStatusAbsent:
            return [[OTCliqueStatusWrapper alloc] initWithStatus:CliqueStatusAbsent];
        case (uint32_t)CliqueStatusNoCloudKitAccount:
            return [[OTCliqueStatusWrapper alloc] initWithStatus:CliqueStatusNoCloudKitAccount];
        case (uint32_t)kSOSCCErrorPositive: // Use the magic number
            return [[OTCliqueStatusWrapper alloc] initWithStatus:CliqueStatusError];
        default:
            ckkserror_global("ckks", "%d is not an OTCliqueStatus?", n);
            return [[OTCliqueStatusWrapper alloc] initWithStatus:CliqueStatusError];;
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

    return [NSString stringWithFormat:@"<CKKSDeviceStateEntry(%@,%@,%@,%@,%@,%@): %@ %@ %@ %@ %@ %@ upd:%@>",
            self.device,
            self.circlePeerID,
            self.octagonPeerID,
            self.osVersion,
            self.lastUnlockTime,
            self.zoneID.zoneName,
            SOSAccountGetSOSCCStatusString(self.circleStatus),
            self.octagonStatus ? OTCliqueStatusToString(self.octagonStatus.status) : @"CliqueMissing",
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
            ((self.octagonPeerID == nil && obj.octagonPeerID == nil)         || [self.octagonPeerID isEqual: obj.octagonPeerID]) &&
            ((self.octagonStatus == nil && obj.octagonStatus == nil)         || [self.octagonStatus isEqual: obj.octagonStatus]) &&
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

    record[SecCKRecordOctagonPeerID] = self.octagonPeerID;
    record[SecCKRecordOctagonStatus] = [self cliqueStatusToCKType:self.octagonStatus];

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

    if((!(self.octagonPeerID == nil && record[SecCKRecordOctagonPeerID] == nil)) &&
       ![record[SecCKRecordOctagonPeerID] isEqualToString:self.octagonPeerID]) {
        return false;
    }

    if((!(self.octagonStatus == nil && record[SecCKRecordOctagonStatus] == nil)) &&
       [self.octagonStatus isEqual: [self cktypeToOTCliqueStatusWrapper:record[SecCKRecordOctagonStatus]]]) {
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

    self.octagonPeerID = record[SecCKRecordOctagonPeerID];
    self.octagonStatus = [self cktypeToOTCliqueStatusWrapper:record[SecCKRecordOctagonStatus]];

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
    return @[@"device", @"ckzone", @"osversion", @"lastunlock",
             @"peerid", @"circlestatus",
             @"octagonpeerid", @"octagonstatus",
             @"keystate", @"currentTLK", @"currentClassA", @"currentClassC", @"ckrecord"];
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
             @"octagonpeerid": CKKSNilToNSNull(self.octagonPeerID),
             @"octagonstatus": CKKSNilToNSNull(self.octagonStatus ? OTCliqueStatusToString(self.octagonStatus.status) : nil),
             @"keystate":      CKKSNilToNSNull(self.keyState),
             @"currentTLK":    CKKSNilToNSNull(self.currentTLKUUID),
             @"currentClassA": CKKSNilToNSNull(self.currentClassAUUID),
             @"currentClassC": CKKSNilToNSNull(self.currentClassCUUID),
             @"ckrecord":      CKKSNilToNSNull([self.encodedCKRecord base64EncodedStringWithOptions:0]),
             };
}

+ (instancetype)fromDatabaseRow:(NSDictionary<NSString*, CKKSSQLResult*>*)row {
    OTCliqueStatusWrapper* octagonStatus = nil;
    NSString* octagonStatusString = row[@"octagonstatus"].asString;
    if(octagonStatusString) {
        octagonStatus = [[OTCliqueStatusWrapper alloc] initWithStatus:OTCliqueStatusFromString(octagonStatusString)];
    }

    return [[CKKSDeviceStateEntry alloc] initForDevice:row[@"device"].asString
                                             osVersion:row[@"osversion"].asString
                                        lastUnlockTime:row[@"lastunlock"].asISO8601Date
                                         octagonPeerID:row[@"octagonpeerid"].asString
                                         octagonStatus:octagonStatus
                                          circlePeerID:row[@"peerid"].asString
                                          circleStatus:SOSAccountGetSOSCCStatusFromString((__bridge CFStringRef) row[@"circlestatus"].asString)
                                              keyState:(CKKSZoneKeyState*)row[@"keystate"].asString
                                        currentTLKUUID:row[@"currentTLK"].asString
                                     currentClassAUUID:row[@"currentClassA"].asString
                                     currentClassCUUID:row[@"currentClassC"].asString
                                                zoneID:[[CKRecordZoneID alloc] initWithZoneName: row[@"ckzone"].asString ownerName:CKCurrentUserDefaultName]
                                       encodedCKRecord:row[@"ckrecord"].asBase64DecodedData
            ];
}

#pragma mark - CKKS interaction methods

+ (CKKSDeviceStateEntry* _Nullable)intransactionCreateDeviceStateForView:(CKKSKeychainViewState*)viewState
                                                          accountTracker:(CKKSAccountStateTracker*)accountTracker
                                                        lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
                                                                   error:(NSError**)error
{
    NSError* localerror = nil;

    CKKSAccountStatus hsa2Status = accountTracker.hsa2iCloudAccountStatus;

    // We must have an HSA2 iCloud account and a CloudKit account to even create one of these
    if(hsa2Status != CKKSAccountStatusAvailable ||
       accountTracker.currentCKAccountInfo.accountStatus != CKAccountStatusAvailable) {
        ckkserror("ckksdevice", viewState, "No iCloud account active: %@ hsa2 account:%@",
                  accountTracker.currentCKAccountInfo,
                  CKKSAccountStatusToString(hsa2Status));
        localerror = [NSError errorWithDomain:@"securityd"
                                         code:errSecInternalError
                                     userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat: @"No active HSA2 iCloud account: %@", accountTracker.currentCKAccountInfo]}];
        if(error) {
            *error = localerror;
        }
        return nil;
    }

    NSString* ckdeviceID = accountTracker.ckdeviceID;
    if(ckdeviceID == nil) {
        ckkserror("ckksdevice", viewState, "No CK device ID available; cannot make device state entry");
        localerror = [NSError errorWithDomain:CKKSErrorDomain
                                         code:CKKSNotLoggedIn
                                     userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat: @"No CK device ID: %@", accountTracker.currentCKAccountInfo]}];
        if(error) {
            *error = localerror;
        }
        return nil;
    }

    CKKSDeviceStateEntry* oldcdse = [CKKSDeviceStateEntry tryFromDatabase:ckdeviceID zoneID:viewState.zoneID error:&localerror];
    if(localerror) {
        ckkserror("ckksdevice", viewState, "Couldn't read old CKKSDeviceStateEntry from database: %@", localerror);
        if(error) {
            *error = localerror;
        }
        return nil;
    }

    // Find out what we think the current keys are
    CKKSCurrentKeyPointer* currentTLKPointer    = [CKKSCurrentKeyPointer tryFromDatabase: SecCKKSKeyClassTLK zoneID:viewState.zoneID error:&localerror];
    CKKSCurrentKeyPointer* currentClassAPointer = [CKKSCurrentKeyPointer tryFromDatabase: SecCKKSKeyClassA   zoneID:viewState.zoneID error:&localerror];
    CKKSCurrentKeyPointer* currentClassCPointer = [CKKSCurrentKeyPointer tryFromDatabase: SecCKKSKeyClassC   zoneID:viewState.zoneID error:&localerror];
    if(localerror) {
        // Things is broken, but the whole point of this record is to share the brokenness. Continue.
        ckkserror("ckksdevice", viewState, "Couldn't read current key pointers from database: %@; proceeding", localerror);
        localerror = nil;
    }

    CKKSKey* suggestedTLK       = currentTLKPointer.currentKeyUUID    ? [CKKSKey tryFromDatabase:currentTLKPointer.currentKeyUUID    zoneID:viewState.zoneID error:&localerror] : nil;
    CKKSKey* suggestedClassAKey = currentClassAPointer.currentKeyUUID ? [CKKSKey tryFromDatabase:currentClassAPointer.currentKeyUUID zoneID:viewState.zoneID error:&localerror] : nil;
    CKKSKey* suggestedClassCKey = currentClassCPointer.currentKeyUUID ? [CKKSKey tryFromDatabase:currentClassCPointer.currentKeyUUID zoneID:viewState.zoneID error:&localerror] : nil;

    if(localerror) {
        // Things is broken, but the whole point of this record is to share the brokenness. Continue.
        ckkserror("ckksdevice", viewState, "Couldn't read keys from database: %@; proceeding", localerror);
        localerror = nil;
    }

    // Check if we posess the keys in the keychain
    [suggestedTLK ensureKeyLoaded:&localerror];
    if(localerror && [lockStateTracker isLockedError:localerror]) {
        ckkserror("ckksdevice", viewState, "Device is locked; couldn't read TLK from keychain. Assuming it is present and continuing; error was %@", localerror);
        localerror = nil;
    } else if(localerror) {
        ckkserror("ckksdevice", viewState, "Couldn't read TLK from keychain. We do not have a current TLK. Error was %@", localerror);
        suggestedTLK = nil;
    }

    [suggestedClassAKey ensureKeyLoaded:&localerror];
    if(localerror && [lockStateTracker isLockedError:localerror]) {
        ckkserror("ckksdevice", viewState, "Device is locked; couldn't read ClassA key from keychain. Assuming it is present and continuing; error was %@", localerror);
        localerror = nil;
    } else if(localerror) {
        ckkserror("ckksdevice", viewState, "Couldn't read ClassA key from keychain. We do not have a current ClassA key. Error was %@", localerror);
        suggestedClassAKey = nil;
    }

    [suggestedClassCKey ensureKeyLoaded:&localerror];
    // class C keys are stored class C, so uh, don't check lock state.
    if(localerror) {
        ckkserror("ckksdevice", viewState, "Couldn't read ClassC key from keychain. We do not have a current ClassC key. Error was %@", localerror);
        suggestedClassCKey = nil;
    }

    // We'd like to have the circle peer ID. Give the account state tracker a fighting chance, but not having it is not an error
    // But, if the platform doesn't have SOS, don't bother
    if(OctagonPlatformSupportsSOS() && [accountTracker.accountCirclePeerIDInitialized wait:500*NSEC_PER_MSEC] != 0 && !accountTracker.accountCirclePeerID) {
        ckkserror("ckksdevice", viewState, "No SOS peer ID available");
    }

    // We'd also like the Octagon status
    if([accountTracker.octagonInformationInitialized wait:500*NSEC_PER_MSEC] != 0 && !accountTracker.octagonPeerID) {
        ckkserror("ckksdevice", viewState, "No octagon peer ID available");
    }

    // Reset the last unlock time to 'day' granularity in UTC
    NSCalendar* calendar = [NSCalendar calendarWithIdentifier:NSCalendarIdentifierISO8601];
    calendar.timeZone = [NSTimeZone timeZoneWithAbbreviation:@"UTC"];
    NSDate* lastUnlockDay = lockStateTracker.lastUnlockTime;
    lastUnlockDay = lastUnlockDay ? [calendar startOfDayForDate:lastUnlockDay] : nil;

    // We only really want the oldcdse for its encodedCKRecord, so make a new cdse here
    CKKSDeviceStateEntry* newcdse = [[CKKSDeviceStateEntry alloc] initForDevice:ckdeviceID
                                                                      osVersion:SecCKKSHostOSVersion()
                                                                 lastUnlockTime:lastUnlockDay
                                                                  octagonPeerID:accountTracker.octagonPeerID
                                                                  octagonStatus:accountTracker.octagonStatus
                                                                   circlePeerID:accountTracker.accountCirclePeerID
                                                                   circleStatus:accountTracker.currentCircleStatus.status
                                                                       keyState:viewState.zoneCKKSState
                                                                 currentTLKUUID:suggestedTLK.uuid
                                                              currentClassAUUID:suggestedClassAKey.uuid
                                                              currentClassCUUID:suggestedClassCKey.uuid
                                                                         zoneID:viewState.zoneID
                                                                encodedCKRecord:oldcdse.encodedCKRecord];
    return newcdse;
}

+ (BOOL)intransactionRecordChanged:(CKRecord*)record resync:(BOOL)resync error:(NSError**)error
{
    if(resync) {
        NSError* dserror = nil;
        CKKSDeviceStateEntry* cdse = [CKKSDeviceStateEntry tryFromDatabase:record.recordID.recordName zoneID:record.recordID.zoneID error:&dserror];
        if(dserror) {
            ckkserror("ckksresync", record.recordID.zoneID, "error loading cdse: %@", dserror);
        }
        if(!cdse) {
            ckkserror("ckksresync", record.recordID.zoneID, "BUG: No current device state entry matching resynced CloudKit record: %@", record);
        } else if(![cdse matchesCKRecord:record]) {
            ckkserror("ckksresync", record.recordID.zoneID, "BUG: Local current device state entry doesn't match resynced CloudKit record(s): %@ %@", cdse, record);
        } else {
            ckksnotice("ckksresync", record.recordID.zoneID, "Already know about this current item pointer, skipping update: %@", record);
            return YES;
        }
    }

    NSError* localerror = nil;
    CKKSDeviceStateEntry* cdse = [[CKKSDeviceStateEntry alloc] initWithCKRecord:record];
    bool saved = [cdse saveToDatabase:&localerror];
    if (!saved || localerror != nil) {
        ckkserror("ckksdevice", record.recordID.zoneID, "Failed to save device record to database: %@: %@ %@", cdse, localerror, record);
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
    ckksinfo("ckksdevice", recordID.zoneID, "CloudKit notification: deleted device state record(%@): %@", SecCKRecordDeviceStateType, recordID);

    CKKSDeviceStateEntry* cdse = [CKKSDeviceStateEntry tryFromDatabaseFromCKRecordID:recordID error:&localerror];
    [cdse deleteFromDatabase:&localerror];

    ckksinfo("ckksdevice", recordID.zoneID, "CKKSCurrentItemPointer(%@) was deleted: %@ error: %@", cdse, recordID, localerror);

    if(error && localerror) {
        *error = localerror;
    }

    return (localerror == nil);
}

@end

#endif

