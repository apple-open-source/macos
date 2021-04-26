/*
 * Copyright (c) 2020 Apple Inc. All Rights Reserved.
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

#import <Foundation/Foundation.h>

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSMemoryKeyCache.h"
#import "keychain/ckks/CKKSCurrentKeyPointer.h"

#if OCTAGON

@interface CKKSMemoryKeyCache ()
@property NSMutableDictionary<NSString*, CKKSKey*>* keyCache;
@end

@implementation CKKSMemoryKeyCache

- (instancetype)init
{
    if((self = [super init])) {
        _keyCache = [NSMutableDictionary dictionary];
    }
    return self;
}

- (CKKSKey* _Nullable)loadKeyForUUID:(NSString*)keyUUID zoneID:(CKRecordZoneID*)zoneID error:(NSError**)error
{
    CKKSKey* key = self.keyCache[keyUUID];
    if(key) {
        return key;
    }

    // Note: returns nil (and empties the cache) if there is an error
    key = [CKKSKey loadKeyWithUUID:keyUUID zoneID:zoneID error:error];
    self.keyCache[keyUUID] = key;
    return key;
}

- (CKKSKey* _Nullable)loadKeyForItem:(CKKSItem*)item error:(NSError**)error
{
    return [self loadKeyForUUID:item.parentKeyUUID zoneID:item.zoneID error:error];
}

- (CKKSKey* _Nullable)currentKeyForClass:(CKKSKeyClass*)keyclass
                                  zoneID:(CKRecordZoneID*)zoneID
                                   error:(NSError *__autoreleasing*)error
{
    // Load the CurrentKey record, and find the key for it
    CKKSCurrentKeyPointer* ckp = [CKKSCurrentKeyPointer fromDatabase:keyclass zoneID:zoneID error:error];
    if(!ckp) {
        return nil;
    }
    return [self loadKeyForUUID:ckp.currentKeyUUID zoneID:zoneID error:error];
}

@end

#endif
