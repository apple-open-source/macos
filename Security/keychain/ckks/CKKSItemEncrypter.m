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

#include <AssertMacros.h>
#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
#import <CoreFoundation/CFPropertyList_Private.h>

#include "keychain/securityd/SecItemSchema.h"

#import "CKKSItemEncrypter.h"
#import "CKKSKeychainView.h"

#import "CKKSOutgoingQueueEntry.h"
#import "CKKSKey.h"
#import "CKKSItem.h"

#if OCTAGON

@implementation CKKSItemEncrypter

// Make it harder to distinguish password lengths especially when password is short.
+ (NSData *)padData:(NSData * _Nonnull)input blockSize:(NSUInteger)blockSize additionalBlock:(BOOL)extra {
    // Must apply padding otherwise unpadding will choke
    if (blockSize == 0) {
        blockSize = 1;
        secwarning("CKKS padding function received invalid blocksize 0, using 1 instead");
    }
    NSMutableData *data = [NSMutableData dataWithData:input];
    NSUInteger paddingLength = blockSize - (data.length % blockSize);
	// Short password as determined by caller: make sure there's at least one block of padding
	if (extra) {
		paddingLength += blockSize;
	}
    [data appendData:[NSMutableData dataWithLength:paddingLength]];
    uint8_t *bytes = [data mutableBytes];
    bytes[data.length - paddingLength] = CKKS_PADDING_MARK_BYTE;
    return data;
}

+ (NSData *)removePaddingFromData:(NSData * _Nonnull)input {
    size_t len = input.length;
    uint8_t const *bytes = [input bytes];
    size_t idx = len;
    while (idx > 0) {
        idx -= 1;
        switch(bytes[idx]) {
            case 0:
                continue;
            case CKKS_PADDING_MARK_BYTE:
                return [input subdataWithRange:NSMakeRange(0, idx)];
            default:
                return nil;
        }
    }
    return nil;
}

+ (CKKSItem*)encryptCKKSItem:(CKKSItem*)baseitem
              dataDictionary:(NSDictionary *)dict
            updatingCKKSItem:(CKKSItem*)olditem
                   parentkey:(CKKSKey *)parentkey
                    keyCache:(CKKSMemoryKeyCache* _Nullable)keyCache
                       error:(NSError * __autoreleasing *) error
{
    CKKSKey *itemkey = nil;

    // If we're updating a CKKSItem, extract its dictionary and overlay our new one on top
    if(olditem) {
        NSMutableDictionary* oldDictionary = [[CKKSItemEncrypter decryptItemToDictionary:olditem keyCache:keyCache error:error] mutableCopy];
        if(!oldDictionary) {
            ckkserror("ckme", olditem.zoneID, "Couldn't decrypt old CKMirror entry: %@", (error ? *error : @"null error passed in"));
            return nil;
        }

        // Use oldDictionary as a base, and copy in the objects from dict
        [oldDictionary addEntriesFromDictionary: dict];
        // and replace the dictionary with the new one
        dict = oldDictionary;
    }

    // generate a new key and wrap it
    itemkey = [CKKSKey randomKeyWrappedByParent: parentkey error:error];
    if(!itemkey) {
        return nil;
    }

    // Prepare the authenticated data dictionary. It includes the wrapped key, so prepare that first
    CKKSItem* encryptedItem = [[CKKSItem alloc] initCopyingCKKSItem: baseitem];
    encryptedItem.parentKeyUUID = parentkey.uuid;
    encryptedItem.wrappedkey = itemkey.wrappedkey;

    // if we're updating a v2 item, use v2
    if(olditem && olditem.encver == CKKSItemEncryptionVersion2 && (int)CKKSItemEncryptionVersion2 >= (int)currentCKKSItemEncryptionVersion) {
        encryptedItem.encver = CKKSItemEncryptionVersion2;
    } else {
        encryptedItem.encver = currentCKKSItemEncryptionVersion;
    }

    // Copy over the old item's CKRecord, updated for the new item data
    if(olditem.storedCKRecord) {
        encryptedItem.storedCKRecord = [encryptedItem updateCKRecord:olditem.storedCKRecord zoneID:olditem.storedCKRecord.recordID.zoneID];
    }

    NSDictionary<NSString*, NSData*>*  authenticatedData = [encryptedItem makeAuthenticatedDataDictionaryUpdatingCKKSItem: olditem encryptionVersion:encryptedItem.encver];

    encryptedItem.encitem = [CKKSItemEncrypter encryptDictionary: dict key:itemkey.aessivkey authenticatedData:authenticatedData error:error];
    if(!encryptedItem.encitem) {
        return nil;
    }

    return encryptedItem;
}

+ (NSDictionary*) decryptItemToDictionaryVersionNone: (CKKSItem*) item error: (NSError * __autoreleasing *) error {
    return [NSPropertyListSerialization propertyListWithData:item.encitem
                                                     options:(NSPropertyListReadOptions)kCFPropertyListSupportedFormatBinary_v1_0
                                                      format:nil
                                                       error:error];
}

// Note that the only difference between v1 and v2 is the authenticated data selection, so we can happily pass encver along
+ (NSDictionary*)decryptItemToDictionaryVersion1or2:(CKKSItem*)item
                                           keyCache:(CKKSMemoryKeyCache* _Nullable)keyCache
                                              error:(NSError * __autoreleasing *)error
{
    NSDictionary* authenticatedData = nil;

    CKKSAESSIVKey* itemkey = nil;
    CKKSKey* key = nil;

    if(keyCache) {
        key = [keyCache loadKeyForUUID:item.parentKeyUUID zoneID:item.zoneID error:error];
    } else {
        key = [CKKSKey loadKeyWithUUID:item.parentKeyUUID zoneID:item.zoneID error:error];
    }

    if(!key) {
        return nil;
    }

    itemkey = [key unwrapAESKey:item.wrappedkey error:error];
    if(!itemkey) {
        return nil;
    }

    // Prepare the authenticated data dictionary (pass the item as the futureproofed object, so we'll authenticate any new fields in this particular item).
    authenticatedData = [item makeAuthenticatedDataDictionaryUpdatingCKKSItem:item encryptionVersion:item.encver];

    NSDictionary* result = [self decryptDictionary: item.encitem key:itemkey authenticatedData:authenticatedData error:error];
    if(!result) {
        ckkserror("item", item.zoneID, "ckks: couldn't decrypt item %@", *error);
    }
    return result;
}

+ (NSDictionary*)decryptItemToDictionary:(CKKSItem*) item
                                keyCache:(CKKSMemoryKeyCache* _Nullable)keyCache
                                   error:(NSError * __autoreleasing *)error
{
    switch (item.encver) {
        case CKKSItemEncryptionVersion1:
            return [CKKSItemEncrypter decryptItemToDictionaryVersion1or2:item keyCache:keyCache error:error];
        case CKKSItemEncryptionVersion2:
            return [CKKSItemEncrypter decryptItemToDictionaryVersion1or2:item keyCache:keyCache error:error];
        case CKKSItemEncryptionVersionNone: /* version 0 was no encrypted, no longer supported */
        default:
        {
            NSError *localError = [NSError errorWithDomain:@"securityd"
                                                      code:1
                                                  userInfo:@{NSLocalizedDescriptionKey:
                                                                 [NSString stringWithFormat:@"Unrecognized encryption version: %lu", (unsigned long)item.encver]}];
            ckkserror("item", item.zoneID, "decryptItemToDictionary failed: %@", localError);
            if (error) {
                *error = localError;
            }
            return nil;
        }
    }
}

+ (NSData*)encryptDictionary:(NSDictionary*)dict
                         key:(CKKSAESSIVKey*)key
           authenticatedData:(NSDictionary<NSString*, NSData*>*)ad
                       error:(NSError * __autoreleasing *)error
{
    NSData* data = [NSPropertyListSerialization dataWithPropertyList:dict
                                                              format:NSPropertyListBinaryFormat_v1_0
                                                             options:0
                                                               error:error];
    if(!data) {
        return nil;
    }
    // Hide password length. Apply extra padding to short passwords.
	data = [CKKSItemEncrypter padData:data
							blockSize:SecCKKSItemPaddingBlockSize
					  additionalBlock:[(NSData *)dict[@"v_Data"] length] < SecCKKSItemPaddingBlockSize];

    return [key encryptData: data authenticatedData: ad error: error];
}

+ (NSDictionary*)decryptDictionary: (NSData*) encitem key: (CKKSAESSIVKey*) aeskey authenticatedData: (NSDictionary<NSString*, NSData*>*) ad  error: (NSError * __autoreleasing *) error {
    NSData* plaintext = [aeskey decryptData: encitem authenticatedData: ad error: error];
    if(!plaintext) {
        return nil;
    }
    plaintext = [CKKSItemEncrypter removePaddingFromData:plaintext];
    if(!plaintext) {
        if (error) *error = [NSError errorWithDomain:@"securityd"
                                                code:errSecInvalidData
                                            userInfo:@{NSLocalizedDescriptionKey: @"Could not remove padding from decrypted item: malformed data"}];
        return nil;
    }
    return [NSPropertyListSerialization propertyListWithData:plaintext
                                                    options:(NSPropertyListReadOptions)kCFPropertyListSupportedFormatBinary_v1_0
                                                     format:nil
                                                      error:error];
}

@end

#endif
