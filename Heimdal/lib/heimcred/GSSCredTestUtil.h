/*-
 * Copyright (c) 2013 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2013,2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>
#import "hc_err.h"
#import "common.h"
#import "heimbase.h"
#import "MockManagedAppManager.h"

NS_ASSUME_NONNULL_BEGIN

@interface GSSCredTestUtil : NSObject

#pragma mark -
#pragma mark peer

+ (struct peer *)createPeer:(NSString *)bundle identifier:(int)sid;
+ (struct peer *)createPeer:(NSString *)bundle callingBundleId:(NSString * _Nullable)callingApp identifier:(int)sid;
+ (void)freePeer:(struct peer * _Nullable)ptr;

#pragma mark -
#pragma mark create

+ (BOOL)createCredentialAndCache:(struct peer * _Nullable)peer name:(NSString*)clientName returningCacheUuid:(CFUUIDRef _Nonnull *_Nonnull)uuid;
+ (BOOL)createCredentialAndCache:(struct peer * _Nullable)peer name:(NSString*)clientName returningCredentialDictionary:(CFDictionaryRef _Nonnull *_Nonnull)dict;

+ (BOOL)createCredential:(struct peer * _Nullable)peer name:(NSString*)clientName attributes:(CFDictionaryRef  _Nullable)attributes returningUuid:(CFUUIDRef _Nonnull *_Nonnull) uuid;
+ (BOOL)createCredential:(struct peer * _Nullable)peer name:(NSString*)clientName attributes:(CFDictionaryRef _Nullable)attributes returningDictionary:(CFDictionaryRef _Nonnull *_Nonnull)dict;
+ (BOOL)executeCreateCred:(struct peer * _Nullable)peer forAttributes:(CFDictionaryRef)allAttrs returningDictionary:(CFDictionaryRef _Nullable * _Nonnull)dict;

+ (BOOL)createNTLMCredential:(struct peer * _Nullable)peer returningUuid:(CFUUIDRef _Nonnull *_Nonnull)uuid;
+ (BOOL)createNTLMCredential:(struct peer * _Nullable)peer returningDictionary:(CFDictionaryRef _Nonnull *_Nonnull)dict;
+ (BOOL)createNTLMCredential:(struct peer * _Nullable)peer attributes:(CFDictionaryRef _Nullable)attributes returningDictionary:(CFDictionaryRef _Nullable *_Nonnull)dict;

#pragma mark -
#pragma mark fetch

+ (BOOL)fetchCredential:(struct peer * _Nullable)peer uuid:(CFUUIDRef)uuid;
+ (BOOL)fetchCredential:(struct peer * _Nullable)peer uuid:(CFUUIDRef) uuid returningDictionary:(CFDictionaryRef _Nonnull *_Nonnull)dict;
+ (CFUUIDRef _Nullable)getDefaultCredential:(struct peer * _Nullable)peer CF_RETURNS_RETAINED;
+ (BOOL)fetchDefaultCredential:(struct peer *)peer returningName:(NSString * __autoreleasing *)name;

#pragma mark -
#pragma mark query

+ (NSUInteger)itemCount:(struct peer * _Nullable)peer;
+ (BOOL)queryAllKerberos:(struct peer * _Nullable)peer returningArray:(NSArray * _Nonnull __autoreleasing *_Nonnull)items;
+ (BOOL)queryAll:(struct peer * _Nullable)peer parentUUID:(CFUUIDRef)parentUUID returningArray:(NSArray * _Nonnull __autoreleasing *_Nonnull)items;
+ (BOOL)queryAll:(struct peer * _Nullable)peer type:(CFStringRef)type returningArray:(NSArray * _Nonnull __autoreleasing *_Nonnull)items;
+ (BOOL)queryAllCredentials:(struct peer * _Nullable)peer returningArray:(NSArray * _Nonnull __autoreleasing *_Nonnull)items;
+ (void)showStatus:(struct peer * _Nullable)peer;

#pragma mark -
#pragma mark update

+ (int64_t)setAttributes:(struct peer * _Nullable)peer uuid:(CFUUIDRef) uuid attributes:(CFDictionaryRef _Nonnull)attributes returningDictionary:(CFDictionaryRef _Nonnull * _Nullable)dict;

#pragma mark -
#pragma mark delete

+ (int64_t)delete:(struct peer * _Nullable)peer uuid:(CFUUIDRef)uuid;
+ (int64_t)deleteAll:(struct peer * _Nullable)peer dsid:(NSString *)dsid;

#pragma mark -
#pragma mark hold

+ (int64_t)hold:(struct peer * _Nullable)peer uuid:(CFUUIDRef)uuid;
+ (int64_t)unhold:(struct peer * _Nullable)peer uuid:(CFUUIDRef)uuid;

#pragma mark -
#pragma mark utility

+ (void)flushCache;

@end

NS_ASSUME_NONNULL_END
