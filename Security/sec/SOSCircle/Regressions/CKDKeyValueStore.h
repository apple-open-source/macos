//
//  CKDKeyValueStore.h
//  sec
//
//  Created by John Hurley on 9/9/12.
//
//

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
