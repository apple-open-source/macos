//
//  CKDKVSStore.h
//

#import <Foundation/Foundation.h>

#import "CKDStore.h"
#import "CKDKVSProxy.h"

@interface CKDKVSStore : NSObject <CKDStore>

+ (instancetype)kvsInterface;
- (instancetype)init;

- (void)connectToProxy: (UbiqitousKVSProxy*) proxy;

- (NSObject*)objectForKey:(NSString*)key;

- (void)setObject:(id)obj forKey:(NSString*)key;
- (void)addEntriesFromDictionary:(NSDictionary<NSString*, NSObject*> *)otherDictionary;

- (void)removeObjectForKey:(NSString*)key;
- (void)removeAllObjects;

- (NSDictionary<NSString *, id>*) copyAsDictionary;

- (void)pushWrites;
- (BOOL)pullUpdates:(NSError**) failure;

- (void)kvsStoreChanged: (NSNotification*) notification;

@end
