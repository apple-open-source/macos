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
- (void)addOneToOutGoing;

- (NSObject*)objectForKey:(NSString*)key;

- (void)setObject:(id)obj forKey:(NSString*)key;
- (void)addEntriesFromDictionary:(NSDictionary<NSString*, NSObject*> *)otherDictionary;

- (void)removeObjectForKey:(NSString*)key;
- (void)removeAllObjects;

- (NSDictionary<NSString *, id>*) copyAsDictionary;

- (void)pushWrites:(NSArray<NSString*>*)keys requiresForceSync:(BOOL)requiresForceSync;
- (BOOL)pullUpdates:(NSError**) failure;

- (void)kvsStoreChanged: (NSNotification*) notification;

@end
