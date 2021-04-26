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

#import "keychain/ckks/CKKSKey.h"

#if OCTAGON

NS_ASSUME_NONNULL_BEGIN

//
// Usage Note:
//
//  This object transparently caches CKKSKey objects, with their key material
//  intact. Since those keys are loaded from the database, they remain
//  valid while you're in the database transaction where you loaded them.
//  To preverve this property, this cache must be destroyed before the end
//  of the database transaction in which it is created.
//

@interface CKKSMemoryKeyCache : NSObject

// An instance of a CKKSItemEncrypter also contains a cache of (loaded and ready) CKKSKeys
// Use these to access the cache
- (instancetype)init;
- (CKKSKey* _Nullable)loadKeyForUUID:(NSString*)keyUUID
                              zoneID:(CKRecordZoneID*)zoneID
                               error:(NSError**)error;
- (CKKSKey* _Nullable)currentKeyForClass:(CKKSKeyClass*)keyclass
                                  zoneID:(CKRecordZoneID*)zoneID
                                   error:(NSError *__autoreleasing*)error;

@end

NS_ASSUME_NONNULL_END

#endif
