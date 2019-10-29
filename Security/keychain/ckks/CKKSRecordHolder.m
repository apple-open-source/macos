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

#if OCTAGON

#include <AssertMacros.h>

#import <Foundation/Foundation.h>
#import <Foundation/NSKeyedArchiver_Private.h>
#import "CKKSItem.h"
#import "CKKSSIV.h"

#include <utilities/SecDb.h>
#include "keychain/securityd/SecDbItem.h"
#include "keychain/securityd/SecItemSchema.h"

#import <CloudKit/CloudKit.h>


@implementation CKKSCKRecordHolder

- (instancetype) initWithCKRecord: (CKRecord*) record {
    if(self = [super init]) {
        _zoneID = record.recordID.zoneID;
        [self setFromCKRecord:record];
    }
    return self;
}

- (instancetype)initWithCKRecordType: (NSString*) recordType encodedCKRecord: (NSData*) encodedCKRecord zoneID:(CKRecordZoneID*)zoneID {
    if(self = [super init]) {
        _zoneID = zoneID;
        _ckRecordType = recordType;
        _encodedCKRecord = encodedCKRecord;

        if(self.encodedCKRecord && ![self.storedCKRecord.recordID.zoneID isEqual: self.zoneID]) {
            secerror("ckks: mismatching zone ids in a single record: %@ and %@", self.zoneID, self.storedCKRecord.recordID.zoneID);
        }
    }
    return self;
}

- (CKRecord*) storedCKRecord {
    if(!_encodedCKRecord) {
        return nil;
    }
    NSKeyedUnarchiver *coder = [[NSKeyedUnarchiver alloc] initForReadingFromData:_encodedCKRecord error:nil];
    CKRecord* ckRecord = [[CKRecord alloc] initWithCoder:coder];
    [coder finishDecoding];

    return ckRecord;
}

- (void) setStoredCKRecord: (CKRecord*) ckRecord {
    if(!ckRecord) {
        _encodedCKRecord = nil;
        return;
    }
    self.zoneID = ckRecord.recordID.zoneID;
    self.ckRecordType = ckRecord.recordType;

    NSKeyedArchiver *archiver = [[NSKeyedArchiver alloc] initRequiringSecureCoding:YES];
    [ckRecord encodeWithCoder:archiver];
    _encodedCKRecord = archiver.encodedData;
}

- (CKRecord*) CKRecordWithZoneID: (CKRecordZoneID*) zoneID {
    CKRecordID* recordID = [[CKRecordID alloc] initWithRecordName: [self CKRecordName] zoneID: zoneID];
    CKRecord* record = nil;

    if(self.encodedCKRecord == nil) {
        record = [[CKRecord alloc] initWithRecordType:self.ckRecordType recordID:recordID];
    } else {
        record = self.storedCKRecord;
    }

    [self updateCKRecord:record zoneID:zoneID];

    self.storedCKRecord = record;
    return record;
}

- (NSString*) CKRecordName {
    @throw [NSException exceptionWithName:NSInternalInconsistencyException
                                   reason:[NSString stringWithFormat:@"%@ must override %@", NSStringFromClass([self class]), NSStringFromSelector(_cmd)]
                                 userInfo:nil];
}
- (CKRecord*) updateCKRecord: (CKRecord*) record zoneID: (CKRecordZoneID*) zoneID {
    @throw [NSException exceptionWithName:NSInternalInconsistencyException
                                   reason:[NSString stringWithFormat:@"%@ must override %@", NSStringFromClass([self class]), NSStringFromSelector(_cmd)]
                                 userInfo:nil];
}
- (void) setFromCKRecord: (CKRecord*) record {
    @throw [NSException exceptionWithName:NSInternalInconsistencyException
                                   reason:[NSString stringWithFormat:@"%@ must override %@", NSStringFromClass([self class]), NSStringFromSelector(_cmd)]
                                 userInfo:nil];
}
- (bool) matchesCKRecord: (CKRecord*) record {
    @throw [NSException exceptionWithName:NSInternalInconsistencyException
                                   reason:[NSString stringWithFormat:@"%@ must override %@", NSStringFromClass([self class]), NSStringFromSelector(_cmd)]
                                 userInfo:nil];
}

- (instancetype)copyWithZone:(NSZone *)zone {
    CKKSCKRecordHolder *rhCopy = [super copyWithZone:zone];
    rhCopy->_zoneID = _zoneID;
    rhCopy->_ckRecordType = _ckRecordType;
    rhCopy->_encodedCKRecord = _encodedCKRecord;
    return rhCopy;
}
@end

#endif
