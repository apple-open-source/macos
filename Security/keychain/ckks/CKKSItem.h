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

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSSQLDatabaseObject.h"
#import "keychain/ckks/CKKSRecordHolder.h"
#include <utilities/SecDb.h>
#include <securityd/SecDbItem.h>

#ifndef CKKSItem_h
#define CKKSItem_h

#if OCTAGON

#import <CloudKit/CloudKit.h>

@class CKKSWrappedAESSIVKey;


// Helper base class that includes UUIDs and key information
@interface CKKSItem : CKKSCKRecordHolder {

}

@property (copy) NSString* uuid;
@property (copy) NSString* parentKeyUUID;
@property (copy) NSData* encitem;

@property (getter=base64Item, setter=setBase64Item:) NSString* base64encitem;

@property (copy) CKKSWrappedAESSIVKey* wrappedkey;
@property NSUInteger generationCount;
@property enum SecCKKSItemEncryptionVersion encver;

@property NSNumber* plaintextPCSServiceIdentifier;
@property NSData* plaintextPCSPublicKey;
@property NSData* plaintextPCSPublicIdentity;

// Used for item encryption and decryption. Attempts to be future-compatible for new CloudKit record fields with an optional olditem field, which may contain a CK record. Any fields in that record that we don't understand will be added to the authenticated data dictionary.
- (NSDictionary<NSString*, NSData*>*)makeAuthenticatedDataDictionaryUpdatingCKKSItem:(CKKSItem*) olditem encryptionVersion:(SecCKKSItemEncryptionVersion)encversion;


- (instancetype) initWithCKRecord: (CKRecord*) record;
- (instancetype) initCopyingCKKSItem: (CKKSItem*) item;

// Use this one if you really don't have any more information
- (instancetype) initWithUUID: (NSString*) uuid
                parentKeyUUID: (NSString*) parentKeyUUID
                       zoneID: (CKRecordZoneID*) zoneID;

// Use this one if you don't have a CKRecord yet
- (instancetype) initWithUUID: (NSString*) uuid
                parentKeyUUID: (NSString*) parentKeyUUID
                       zoneID: (CKRecordZoneID*) zoneID
                      encItem: (NSData*) encitem
                   wrappedkey: (CKKSWrappedAESSIVKey*) wrappedkey
              generationCount: (NSUInteger) genCount
                       encver: (NSUInteger) encver;

- (instancetype) initWithUUID: (NSString*) uuid
                parentKeyUUID: (NSString*) parentKeyUUID
                       zoneID: (CKRecordZoneID*)zoneID
              encodedCKRecord: (NSData*) encodedrecord
                      encItem: (NSData*) encitem
                   wrappedkey: (CKKSWrappedAESSIVKey*) wrappedkey
              generationCount: (NSUInteger) genCount
                       encver: (NSUInteger) encver;

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
   plaintextPCSPublicIdentity: (NSData*) pcsPublicIdentity;

// Convenience function: set the upload version for this record to be the current OS version
+ (void)setOSVersionInRecord: (CKRecord*) record;


@end

@interface CKKSSQLDatabaseObject (CKKSZoneExtras)
// Convenience function: get all UUIDs of this type
+ (NSArray<NSString*>*) allUUIDs:  (NSError * __autoreleasing *) error;

// Convenience function: get all objects in this particular zone
+ (NSArray*) all:(CKRecordZoneID*) zoneID error: (NSError * __autoreleasing *) error;

// Convenience function: delete all records of this type with this zoneID
+ (bool) deleteAll:(CKRecordZoneID*) zoneID error: (NSError * __autoreleasing *) error;
@end

#endif
#endif /* CKKSItem_H */
