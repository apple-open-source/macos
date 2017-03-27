//
//  CKDKVSStore.m
//  Security
//
//  Created by Mitch Adler on 5/15/16.
//
//

#import "CKDKVSStore.h"
#import "CKDKVSProxy.h"

#include "SOSCloudKeychainConstants.h"
#include <utilities/debugging.h>

#import <Foundation/NSUbiquitousKeyValueStore.h>
#import <Foundation/NSUbiquitousKeyValueStore_Private.h>
#import "SyncedDefaults/SYDConstants.h"
#include <os/activity.h>

//KVS error codes
#define UPDATE_RESUBMIT 4

@interface CKDKVSStore ()
@property (readwrite, weak) UbiqitousKVSProxy* proxy;
@property (readwrite) NSUbiquitousKeyValueStore* cloudStore;
@end

@implementation CKDKVSStore

+ (instancetype)kvsInterface {
    return [[CKDKVSStore alloc] init];
}

- (instancetype)init {
    self = [super init];

    self->_cloudStore = [NSUbiquitousKeyValueStore defaultStore];
    self->_proxy = nil;

    if (!self.cloudStore) {
        secerror("NO NSUbiquitousKeyValueStore defaultStore!!!");
        return nil;
    }

    return self;
}

- (void) connectToProxy: (UbiqitousKVSProxy*) proxy {
    _proxy = proxy;

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(kvsStoreChangedAsync:)
                                                 name:NSUbiquitousKeyValueStoreDidChangeExternallyNotification
                                               object:nil];

}

- (void)setObject:(id)obj forKey:(NSString*)key {
    secdebug("kvsdebug", "%@ key %@ set to: %@ from: %@", self, key, obj, [self.cloudStore objectForKey:key]);
    [self.cloudStore setObject:obj forKey:key];
}

- (NSDictionary<NSString *, id>*) copyAsDictionary {
    return [self.cloudStore dictionaryRepresentation];
}

- (void)addEntriesFromDictionary:(NSDictionary<NSString*, NSObject*> *)otherDictionary {
    [otherDictionary enumerateKeysAndObjectsUsingBlock:^(NSString * _Nonnull key, NSObject * _Nonnull obj, BOOL * _Nonnull stop) {
        [self setObject:obj forKey:key];
    }];
}

- (id)objectForKey:(NSString*)key {
    return [self.cloudStore objectForKey:key];
}

- (void)removeObjectForKey:(NSString*)key {
    return [self.cloudStore removeObjectForKey:key];
}

- (void)removeAllObjects {
    [[[[self.cloudStore dictionaryRepresentation] allKeys] copy] enumerateObjectsUsingBlock:^(NSString * _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
        [self.cloudStore removeObjectForKey:obj];
    }];
}

- (void)pushWrites {
    [[self cloudStore] synchronize];
}

// Runs on the same thread that posted the notification, and that thread _may_ be the
// kdkvsproxy_queue (see 30470419). Avoid deadlock by bouncing through global queue.
- (void)kvsStoreChangedAsync:(NSNotification *)notification
{
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        [self kvsStoreChanged:notification];
    });
}

- (void) kvsStoreChanged:(NSNotification *)notification {
    /*
     Posted when the value of one or more keys in the local key-value store
     changed due to incoming data pushed from iCloud. This notification is
     sent only upon a change received from iCloud; it is not sent when your
     app sets a value.

     The user info dictionary can contain the reason for the notification as
     well as a list of which values changed, as follows:

     The value of the NSUbiquitousKeyValueStoreChangeReasonKey key, when
     present, indicates why the key-value store changed. Its value is one of
     the constants in "Change Reason Values."

     The value of the NSUbiquitousKeyValueStoreChangedKeysKey, when present,
     is an array of strings, each the name of a key whose value changed. The
     notification object is the NSUbiquitousKeyValueStore object whose contents
     changed.

     NSUbiquitousKeyValueStoreInitialSyncChange is only posted if there is any
     local value that has been overwritten by a distant value. If there is no
     conflict between the local and the distant values when doing the initial
     sync (e.g. if the cloud has no data stored or the client has not stored
     any data yet), you'll never see that notification.

     NSUbiquitousKeyValueStoreInitialSyncChange implies an initial round trip
     with server but initial round trip with server does not imply
     NSUbiquitousKeyValueStoreInitialSyncChange.
     */
    os_activity_initiate("cloudChanged", OS_ACTIVITY_FLAG_DEFAULT, ^{
        secdebug(XPROXYSCOPE, "%@ KVS Remote changed notification: %@", self, notification);

        NSDictionary *userInfo = [notification userInfo];
        NSNumber *reason = [userInfo objectForKey:NSUbiquitousKeyValueStoreChangeReasonKey];
        NSArray *keysChangedArray = [userInfo objectForKey:NSUbiquitousKeyValueStoreChangedKeysKey];
        NSSet *keysChanged = keysChangedArray ? [NSSet setWithArray: keysChangedArray] : nil;

        if (reason) switch ([reason integerValue]) {
            case NSUbiquitousKeyValueStoreInitialSyncChange:
                [self.proxy storeKeysChanged: keysChanged initial: YES];
                break;

            case NSUbiquitousKeyValueStoreServerChange:
                [self.proxy storeKeysChanged: keysChanged initial: NO];
                break;

            case NSUbiquitousKeyValueStoreQuotaViolationChange:
                seccritical("Received NSUbiquitousKeyValueStoreQuotaViolationChange");
                break;

            case NSUbiquitousKeyValueStoreAccountChange:
                [self.proxy storeAccountChanged];
                break;

            default:
                secinfo("kvsstore", "ignoring unknown notification: %@", reason);
                break;
        }
    });
}

// try to synchronize asap, and invoke the handler on completion to take incoming changes.

static bool isResubmitError(NSError* error) {
    return error && (CFErrorGetCode((__bridge CFErrorRef) error) == UPDATE_RESUBMIT) &&
        (CFErrorGetDomain((__bridge CFErrorRef)error) == __SYDErrorKVSDomain);
}

- (BOOL) pullUpdates:(NSError **)failure
{
    __block NSError *tempFailure = nil;
    const int kMaximumTries = 10;
    int tryCount = 0;
    // Retry up to 10 times, since we're told this can fail and WE have to deal with it.

    dispatch_semaphore_t freshSemaphore = dispatch_semaphore_create(0);

    do {
        ++tryCount;
        secnoticeq("fresh", "%s CALLING OUT TO syncdefaultsd SWCH, try %d: %@", kWAIT2MINID, tryCount, self);

        [[self cloudStore] synchronizeWithCompletionHandler:^(NSError *error) {
            if (error) {
                tempFailure = error;
                secnotice("fresh", "%s RETURNING FROM syncdefaultsd SWCH: %@: %@", kWAIT2MINID, self, error);
            } else {
                secnotice("fresh", "%s RETURNING FROM syncdefaultsd SWCH: %@", kWAIT2MINID, self);
                [[self cloudStore] synchronize]; // Per olivier in <rdar://problem/13412631>, sync before getting values
                secnotice("fresh", "%s RETURNING FROM syncdefaultsd SYNC: %@", kWAIT2MINID, self);
            }
            dispatch_semaphore_signal(freshSemaphore);
        }];
        dispatch_semaphore_wait(freshSemaphore, DISPATCH_TIME_FOREVER);
    } while (tryCount < kMaximumTries && isResubmitError(tempFailure));

    if (isResubmitError(tempFailure)) {
        secerrorq("%s %d retry attempts to request freshness exceeded, failing", kWAIT2MINID, tryCount);
    }

    if (failure && (*failure == NULL)) {
        *failure = tempFailure;
    }

    return tempFailure == nil;
}

@end
