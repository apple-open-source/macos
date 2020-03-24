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

#import "CKKSKeychainView.h"

#include <utilities/SecDb.h>
#include "keychain/securityd/SecDbItem.h"
#include "keychain/securityd/SecItemSchema.h"

#import <CloudKit/CloudKit.h>
#import "CKKSIncomingQueueEntry.h"
#import "CKKSItemEncrypter.h"
#import "CKKSSIV.h"

@implementation CKKSIncomingQueueEntry

- (NSString*)description {
    return [NSString stringWithFormat: @"<%@(%@): %@ %@ (%@)>",
            NSStringFromClass([self class]),
            self.item.zoneID.zoneName,
            self.action,
            self.item.uuid,
            self.state];
}

- (instancetype) initWithCKKSItem:(CKKSItem*) item
                           action:(NSString*) action
                            state:(NSString*) state {
    if(self = [super init]) {
        _item = item;
        _action = action;
        _state = state;
    }

    return self;
}

#pragma mark - Property access to underlying CKKSItem

-(NSString*)uuid {
    return self.item.uuid;
}

-(void)setUuid:(NSString *)uuid {
    self.item.uuid = uuid;
}

#pragma mark - Database Operations

+ (instancetype) fromDatabase: (NSString*) uuid zoneID:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error {
    return [self fromDatabaseWhere: @{@"UUID": CKKSNilToNSNull(uuid), @"ckzone":CKKSNilToNSNull(zoneID.zoneName)} error: error];
}

+ (instancetype) tryFromDatabase: (NSString*) uuid zoneID:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error {
    return [self tryFromDatabaseWhere: @{@"UUID": CKKSNilToNSNull(uuid), @"ckzone":CKKSNilToNSNull(zoneID.zoneName)} error: error];
}

+ (NSArray<CKKSIncomingQueueEntry*>*)fetch:(ssize_t)n
                            startingAtUUID:(NSString*)uuid
                                     state:(NSString*)state
                                    zoneID:(CKRecordZoneID*)zoneID
                                     error: (NSError * __autoreleasing *) error {
    NSMutableDictionary* whereDict = [@{@"state": CKKSNilToNSNull(state), @"ckzone":CKKSNilToNSNull(zoneID.zoneName)} mutableCopy];
    if(uuid) {
        whereDict[@"UUID"] = [CKKSSQLWhereValue op:CKKSSQLWhereComparatorGreaterThan value:uuid];
    }
    return [self fetch:n
                 where:whereDict
               orderBy:@[@"UUID"]
                 error:error];
}


#pragma mark - CKKSSQLDatabaseObject methods

+ (NSString*)sqlTable {
    return @"incomingqueue";
}

+ (NSArray<NSString*>*)sqlColumns {
    return [[CKKSItem sqlColumns] arrayByAddingObjectsFromArray: @[@"action", @"state"]];
}

- (NSDictionary<NSString*,NSString*>*)whereClauseToFindSelf {
    return [self.item whereClauseToFindSelf];
}

- (NSDictionary<NSString*,NSString*>*)sqlValues {
    NSMutableDictionary* values = [[self.item sqlValues] mutableCopy];
    values[@"action"] = self.action;
    values[@"state"] = self.state;
    return values;
}


+ (instancetype)fromDatabaseRow:(NSDictionary<NSString *, CKKSSQLResult*>*) row {
    return [[CKKSIncomingQueueEntry alloc] initWithCKKSItem: [CKKSItem fromDatabaseRow: row]
                                                     action:row[@"action"].asString
                                                      state:row[@"state"].asString];
}

+ (NSDictionary<NSString*,NSNumber*>*)countsByStateInZone:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error {
    NSMutableDictionary* results = [[NSMutableDictionary alloc] init];

    [CKKSSQLDatabaseObject queryDatabaseTable: [[self class] sqlTable]
                                        where: @{@"ckzone": CKKSNilToNSNull(zoneID.zoneName)}
                                      columns: @[@"state", @"count(rowid)"]
                                      groupBy: @[@"state"]
                                      orderBy:nil
                                        limit: -1
                                   processRow: ^(NSDictionary<NSString*, CKKSSQLResult*>* row) {
                                       results[row[@"state"].asString] = row[@"count(rowid)"].asNSNumberInteger;
                                   }
                                        error: error];
    return results;
}

+ (NSInteger)countByState:(CKKSItemState *)state zone:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error {
    __block NSInteger result = -1;

    [CKKSSQLDatabaseObject queryDatabaseTable: [[self class] sqlTable]
                                        where: @{@"ckzone": CKKSNilToNSNull(zoneID.zoneName), @"state": state }
                                      columns: @[@"count(*)"]
                                      groupBy: nil
                                      orderBy: nil
                                        limit: -1
                                   processRow: ^(NSDictionary<NSString*, CKKSSQLResult*>* row) {
                                       result = row[@"count(*)"].asNSInteger;
                                   }
                                        error: error];
    return result;
}

@end

#endif
