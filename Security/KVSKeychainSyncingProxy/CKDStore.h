//
//  CKDStore.h
//  Security
//
//

#import <Foundation/Foundation.h>

@class UbiqitousKVSProxy;

@protocol CKDStore

- (void)connectToProxy: (UbiqitousKVSProxy*) proxy;

- (NSObject*)objectForKey:(NSString*)key;

- (void)setObject:(id)obj forKey:(NSString*)key;
- (void)addEntriesFromDictionary:(NSDictionary<NSString*, NSObject*> *)otherDictionary;

- (void)removeObjectForKey:(NSString*)key;
- (void)removeAllObjects;

- (NSDictionary<NSString *, id>*) copyAsDictionary;

- (void)pushWrites;
- (BOOL)pullUpdates:(NSError**) failure;

@end
