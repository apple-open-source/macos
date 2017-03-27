//
//  CKDSimulatedStore.m
//  Security
//

#import "CKDSimulatedStore.h"
#import "CKDKVSProxy.h"

#include "SOSCloudKeychainConstants.h"
#include <utilities/debugging.h>

#import "SyncedDefaults/SYDConstants.h"
#include <os/activity.h>

@interface CKDSimulatedStore ()
@property (readwrite, weak) UbiqitousKVSProxy* proxy;
@property (readwrite) NSMutableDictionary<NSString*, NSObject*>* data;
@end

@implementation CKDSimulatedStore

+ (instancetype)simulatedInterface {
    return [[CKDSimulatedStore alloc] init];
}

- (instancetype)init {
    self = [super init];

    self.proxy = nil;
    self.data = [NSMutableDictionary<NSString*, NSObject*> dictionary];

    return self;
}

- (void) connectToProxy: (UbiqitousKVSProxy*) proxy {
    _proxy = proxy;
}

- (void)setObject:(id)obj forKey:(NSString*)key {
    [self.data setValue: obj forKey: key];
}

- (NSDictionary<NSString *, id>*) copyAsDictionary {
    return self.data;
}

- (void)addEntriesFromDictionary:(NSDictionary<NSString*, NSObject*> *)otherDictionary {
    [otherDictionary enumerateKeysAndObjectsUsingBlock:^(NSString * _Nonnull key, NSObject * _Nonnull obj, BOOL * _Nonnull stop) {
        [self setObject:obj forKey:key];
    }];
}

- (id)objectForKey:(NSString*)key {
    return [self.data objectForKey:key];
}

- (void)removeObjectForKey:(NSString*)key {
    return [self.data removeObjectForKey:key];
}

- (void)removeAllObjects {
    [self.data removeAllObjects];
}

- (void)pushWrites {
}

- (BOOL) pullUpdates:(NSError **)failure
{
    return true;
}

- (void)remoteSetObject:(id)obj forKey:(NSString*)key
{
    [self.data setObject:obj forKey:key];

    if (self.proxy) {
        [self.proxy storeKeysChanged: [NSSet setWithObject:key] initial: NO];
    }
}

@end
