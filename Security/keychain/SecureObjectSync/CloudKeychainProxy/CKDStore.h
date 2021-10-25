//
//  CKDStore.h
//  Security
//
//

#import <Foundation/Foundation.h>

@class UbiqitousKVSProxy;

@protocol CKDStore <NSObject>

- (void)connectToProxy: (UbiqitousKVSProxy*) proxy;

- (NSObject*)objectForKey:(NSString*)key;

- (void)setObject:(id)obj forKey:(NSString*)key;
- (void)addEntriesFromDictionary:(NSDictionary<NSString*, NSObject*> *)otherDictionary;

- (void)removeObjectForKey:(NSString*)key;
- (void)removeAllObjects;

- (NSDictionary<NSString *, id>*) copyAsDictionary;

- (void)pushWrites:(NSArray<NSString*>*)keys requiresForceSync:(BOOL)requiresForceSync;
- (BOOL)pullUpdates:(NSError**) failure;

- (void)perfCounters:(void(^)(NSDictionary *counters))callback;
- (void)addOneToOutGoing;

@end
