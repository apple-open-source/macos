/*
 * Copyright (c) 2012,2014 Apple Inc. All Rights Reserved.
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
//#import "CKDKVSProxy.h"
#import "SOSCloudKeychainConstants.h"
#import "SOSCloudKeychainClient.h"

extern CFStringRef kCKDKVSRemoteStoreID;
extern CFStringRef kCKDAWSRemoteStoreID;

//--- protocol ---

@protocol CKDKVSDelegate <NSObject>

@required

- (id)objectForKey:(NSString *)aKey;
- (void)setObject:(id)anObject forKey:(NSString *)aKey;
- (void)removeObjectForKey:(NSString *)aKey;

- (NSDictionary *)dictionaryRepresentation;

- (BOOL)synchronize;

@optional
- (BOOL)isLocalKVS;
// DEBUG
- (void)setDictionaryRepresentation:(NSMutableDictionary *)initialValue;
- (void)clearPersistentStores;

@end

//--- interface ---

@interface CKDKeyValueStore : NSObject <CKDKVSDelegate>
{
    BOOL localKVS;
    BOOL persistStore;
    CloudItemsChangedBlock itemsChangedCallback;
}
 
@property (retain) id <CKDKVSDelegate> delegate;
@property (retain) NSString *identifier;
@property (retain) NSString *path;
 
- (BOOL)synchronize;
+ (CKDKeyValueStore *)defaultStore:(NSString *)identifier itemsChangedBlock:(CloudItemsChangedBlock)itemsChangedBlock;
- (id)initWithIdentifier:(NSString *)xidentifier itemsChangedBlock:(CloudItemsChangedBlock)itemsChangedBlock;

+ (CFStringRef)remoteStoreID;

- (id)initWithItemsChangedBlock:(CloudItemsChangedBlock)itemsChangedBlock;
- (void)cloudChanged:(NSNotification*)notification;

@end

@interface CKDKeyValueStoreCollection : NSObject
{
    dispatch_queue_t syncrequestqueue;
    NSMutableDictionary *store;
}
@property (retain) NSMutableDictionary *collection;

+ (id)sharedInstance;
+ (id <CKDKVSDelegate>)defaultStore:(NSString *)identifier itemsChangedBlock:(CloudItemsChangedBlock)itemsChangedBlock;
+ (void)enqueueWrite:(id)anObject forKey:(NSString *)aKey from:(NSString *)identifier;
+ (id)enqueueWithReply:(NSString *)aKey;
+ (BOOL)enqueueSyncWithReply;
+ (void)postItemChangedNotification:(NSString *)keyThatChanged from:(NSString *)identifier;
+ (void)postItemsChangedNotification:(NSArray *)keysThatChanged from:(NSString *)identifier;

@end
