/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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

//
//  CKDKVSProxy.m
//  ckd-xpc
//

#import <Foundation/NSUbiquitousKeyValueStore.h>
#import <Foundation/NSUbiquitousKeyValueStore_Private.h>
#import <Foundation/NSArray.h>
#import <Foundation/Foundation.h>

#import <Security/SecBasePriv.h>
#import <Security/SecItemPriv.h>
#import <utilities/debugging.h>
#import <notify.h>

#import "CKDKVSProxy.h"
#import "CKDPersistentState.h"
#import "CKDUserInteraction.h"

#import "SOSARCDefines.h"

#include <SecureObjectSync/SOSAccount.h>
#include <SecureObjectSync/SOSCloudCircleInternal.h>
#include <SecureObjectSync/SOSKVSKeys.h>

#include "SOSCloudKeychainConstants.h"

#include <utilities/SecAKSWrappers.h>
#include <utilities/SecCFRelease.h>

/*
 The total space available in your app’s iCloud key-value storage is 1 MB.
 The maximum number of keys you can specify is 1024, and the size limit for
 each value associated with a key is 1 MB. So, for example, if you store a
 single large value of 1 MB for a single key, that consumes your total
 available storage. If you store 1 KB of data for each key, you can use
 1,000 key-value pairs.
 */

static const char *kStreamName = "com.apple.notifyd.matching";

static NSString *kKeyKeyParameterKeys = @"KeyParameterKeys";
static NSString *kKeyCircleKeys = @"CircleKeys";
static NSString *kKeyMessageKeys = @"MessageKeys";


static NSString *kKeyAlwaysKeys = @"AlwaysKeys";
static NSString *kKeyFirstUnlockKeys = @"FirstUnlockKeys";
static NSString *kKeyUnlockedKeys = @"UnlockedKeys";
static NSString *kKeyPendingKeys = @"PendingKeys";
static NSString *kKeyUnsentChangedKeys = @"unsentChangedKeys";
static NSString *kKeyUnlockNotificationRequested = @"unlockNotificationRequested";
static NSString *kKeySyncWithPeersPending = @"SyncWithPeersPending";
static NSString *kKeyEnsurePeerRegistration = @"EnsurePeerRegistration";

enum
{
    kCallbackMethodSecurityd = 0,
    kCallbackMethodXPC = 1,
};

static const int64_t kMinSyncDelay = (NSEC_PER_MSEC * 500);         // 500ms minimum delay before a syncWithAllPeers call.
static const int64_t kMaxSyncDelay = (NSEC_PER_SEC * 5);            //   5s  maximun delay for a given request
static const int64_t kMinSyncInterval = (NSEC_PER_SEC * 15);        //  15s  minimum time between successive syncWithAllPeers calls.
static const int64_t kSyncTimerLeeway = (NSEC_PER_MSEC * 250);      // 250ms leeway for sync events.

@interface NSUbiquitousKeyValueStore (NSUbiquitousKeyValueStore_PrivateZ)
- (void) _synchronizeWithCompletionHandler:(void (^)(NSError *error))completionHandler;

/*
 // SPI For Security
 - (void) synchronizeWithCompletionHandler:(void (^)(NSError *error))completionHandler;
 */

@end

@implementation UbiqitousKVSProxy


- (void)persistState
{
    [SOSPersistentState setRegisteredKeys:[self exportKeyInterests]];
}

+   (UbiqitousKVSProxy *) sharedKVSProxy
{
    static UbiqitousKVSProxy *sharedKVSProxy;
    if (!sharedKVSProxy) {
        static dispatch_once_t onceToken;
        dispatch_once(&onceToken, ^{
            sharedKVSProxy = [[self alloc] init];
        });
    }
    return sharedKVSProxy;
}

- (id)init
{
    if (self = [super init])
    {
        secnotice("event", "%@ start", self);
        
        _calloutQueue = dispatch_queue_create("CKDCallout", DISPATCH_QUEUE_SERIAL);
        _freshParamsQueue = dispatch_queue_create("CKDFresh", DISPATCH_QUEUE_SERIAL);
        _syncTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
        dispatch_source_set_timer(_syncTimer, DISPATCH_TIME_FOREVER, DISPATCH_TIME_FOREVER, kSyncTimerLeeway);
        dispatch_source_set_event_handler(_syncTimer, ^{
            [self timerFired];
        });
        dispatch_resume(_syncTimer);
        
        [[NSNotificationCenter defaultCenter]
         addObserver: self
         selector: @selector (iCloudAccountAvailabilityChanged:)
         name: NSUbiquityIdentityDidChangeNotification
         object: nil];
        
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(cloudChanged:)
                                                     name:NSUbiquitousKeyValueStoreDidChangeExternallyNotification
                                                   object:nil];
        
        [self importKeyInterests: [SOSPersistentState registeredKeys]];
        
        // Register for lock state changes
        xpc_set_event_stream_handler(kStreamName, dispatch_get_main_queue(),
                                     ^(xpc_object_t notification){
                                         [self streamEvent:notification];
                                     });
        
        [self updateUnlockedSinceBoot];
        [self updateIsLocked];
        if (!_isLocked)
            [self keybagDidUnlock];
        
        secdebug(XPROXYSCOPE, "%@ done", self);
    }
    return self;
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%s%s%s%s%s%s%s%s%s%s%s>",
            _isLocked ? "L" : "U",
            _unlockedSinceBoot ? "B" : "-",
            _seenKVSStoreChange ? "K" : "-",
            _syncTimerScheduled ? "T" : "-",
            _syncWithPeersPending ? "s" : "-",
            _ensurePeerRegistration ? "e" : "-",
            [_pendingKeys count] ? "p" : "-",
            _inCallout ? "C" : "-",
            _shadowSyncWithPeersPending ? "S" : "-",
            _shadowEnsurePeerRegistration ? "E" : "-",
            [_shadowPendingKeys count] ? "P" : "-"];
}

- (void)processAllItems
{
    NSDictionary *allItems = [self getAll];
    if (allItems)
    {
        secnotice("event", "%@ sending: %@", self, [[allItems allKeys] componentsJoinedByString: @" "]);
        [self processKeyChangedEvent:allItems];
    }
    else
        secdebug(XPROXYSCOPE, "%@ No items in KVS", self);
}

- (void)setItemsChangedBlock:(CloudItemsChangedBlock)itemsChangedBlock
{
    self->itemsChangedCallback = itemsChangedBlock;
}

- (void)dealloc
{
    secdebug(XPROXYSCOPE, "%@", self);
    [[NSNotificationCenter defaultCenter] removeObserver:self
                                                    name:NSUbiquitousKeyValueStoreDidChangeExternallyNotification object:nil];
    
    [[NSNotificationCenter defaultCenter] removeObserver:self
                                                    name:NSUbiquityIdentityDidChangeNotification object:nil];
}

// MARK: ----- Client Interface -----

- (void)setObject:(id)obj forKey:(id)key
{
    secdebug("keytrace", "%@ key: %@, obj: %@", self, key, obj);
    NSUbiquitousKeyValueStore *store = [self cloudStore];
    if (store)
    {
        id value = [store objectForKey:key];
        if (value)
            secdebug("keytrace", "%@ Current value (before set) for key %@ is: %@", self, key, value);
        else
            secdebug("keytrace", "%@ No current value for key %@", self, key);
    }
    else
        secdebug("keytrace", "Can't get store");
    
    [[self cloudStore] setObject:obj forKey:key];
    [self requestSynchronization:NO];
}

- (void)setObjectsFromDictionary:(NSDictionary *)values
{
    secdebug(XPROXYSCOPE, "%@ start: %lu values to put", self, (unsigned long)[values count]);
    
    NSUbiquitousKeyValueStore *store = [self cloudStore];
    if (store && values)
    {
        [values enumerateKeysAndObjectsUsingBlock: ^(id key, id obj, BOOL *stop)
         {
             if (obj == NULL || obj == [NSNull null])
                 [store removeObjectForKey:key];
             else
                 [store setObject:obj forKey:key];
         }];
        
        [self requestSynchronization:NO];
    }
    else
        secdebug(XPROXYSCOPE, "%@ NULL? store: %@, values: %@", self, store, values);
}

- (void)requestSynchronization:(bool)force
{
    if (force)
    {
        secdebug(XPROXYSCOPE, "%@ synchronize (forced)", self);
        [[self cloudStore] synchronize];
    }
    else
    {
        secdebug(XPROXYSCOPE, "%@ synchronize (soon)", self);
        [[self cloudStore] synchronize];
    }
}

- (NSUbiquitousKeyValueStore *)cloudStore
{
    NSUbiquitousKeyValueStore *iCloudStore = [NSUbiquitousKeyValueStore defaultStore];
    if (!iCloudStore) {
        secerror("%s %@ NO NSUbiquitousKeyValueStore defaultStore", kWAIT2MINID, self);
    }
    return iCloudStore;
}

/*
 Only call out to syncdefaultsd once every 5 seconds, since parameters can't change that
 fast and callers expect synchonicity.
 
 Since we don't actually get the values for the keys, just store off a timestamp.
*/

// try to synchronize asap, and invoke the handler on completion to take incoming changes.


- (void)waitForSynchronization:(NSArray *)keys handler:(void (^)(NSDictionary *values, NSError *err))handler
{
    if (!keys)
    {
        NSError *err = [NSError errorWithDomain:(NSString *)NSPOSIXErrorDomain code:(NSInteger)ENOPROTOOPT userInfo:nil];
        secerrorq("%s RETURNING TO securityd: %@ param error; calling handler", kWAIT2MINID, self);
        handler(@{}, err);
        return;
    }

    NSUbiquitousKeyValueStore * store = [self cloudStore];

    secnoticeq("fresh", "%s Requesting freshness", kWAIT2MINID);
    dispatch_async(_freshParamsQueue, ^{
        // _freshParamsQueue
        __block NSError *failure = NULL;
        __block dispatch_time_t next_time = _nextFreshnessTime;
        dispatch_time_t now = dispatch_time(DISPATCH_TIME_NOW, 0);
        if (now > next_time) {
            dispatch_semaphore_t freshSemaphore = dispatch_semaphore_create(0);
            secnoticeq("fresh", "%s CALLING OUT TO syncdefaultsd SWCH: %@", kWAIT2MINID, self);
            [store synchronizeWithCompletionHandler:^(NSError *error) {
                if (error) {
                    failure = error;
                    secerrorq("%s RETURNING FROM syncdefaultsd SWCH: %@: %@", kWAIT2MINID, self, error);
                } else {
                    secnoticeq("fresh", "%s RETURNING FROM syncdefaultsd SWCH: %@", kWAIT2MINID, self);

                    [store synchronize]; // Per olivier in <rdar://problem/13412631>, sync before getting values
                    secnoticeq("fresh", "%s RETURNING FROM syncdefaultsd SYNC: %@", kWAIT2MINID, self);

                    // This hysteresis balances between failing to notice parameters from remote devices and
                    // pestering synceddefaultsd with clients many repeated requests to do freshness.
                    const uint64_t delayBeforeCallingAgainInSeconds = 5ull * NSEC_PER_SEC;
                    next_time  = dispatch_time(DISPATCH_TIME_NOW, delayBeforeCallingAgainInSeconds);
                }
                dispatch_semaphore_signal(freshSemaphore);
            }];
            dispatch_semaphore_wait(freshSemaphore, DISPATCH_TIME_FOREVER);
        }

        dispatch_async(dispatch_get_main_queue(), ^{
            _nextFreshnessTime = next_time;

            if (failure)
                handler(@{}, failure);
            else
                handler([self copyValues:[NSSet setWithArray:keys]], NULL);
        });

    });
}

- (void)removeObjectForKey:(NSString *)keyToRemove
{
    [[self cloudStore] removeObjectForKey:keyToRemove];
}

- (void)clearStore
{
    secdebug(XPROXYSCOPE, "%@ clearStore", self);
    NSDictionary *dict = [[self cloudStore] dictionaryRepresentation];
    NSArray *allKeys = [dict allKeys];
    NSMutableArray* nullKeys = [NSMutableArray array];
    
    _alwaysKeys = [NSMutableSet setWithArray:nullKeys];
    _firstUnlockKeys = [NSMutableSet setWithArray:nullKeys];
    _unlockedKeys = [NSMutableSet setWithArray:nullKeys];
    _keyParameterKeys = @{};
    _circleKeys = @{};
    _messageKeys = @{};

    [allKeys enumerateObjectsUsingBlock:^(id key, NSUInteger idx, BOOL *stop)
     {
         secdebug(XPROXYSCOPE, "%@ Clearing value for key %@", self, key);
         [[self cloudStore] removeObjectForKey:(NSString *)key];
     }];
    
    [self requestSynchronization:YES];
}


//
// MARK: ----- KVS key lists -----
//

- (id)get:(id)key
{
    return [[self cloudStore] objectForKey:key];
}

- (NSDictionary *)getAll
{
    return [[self cloudStore] dictionaryRepresentation];
}

- (NSDictionary*) exportKeyInterests
{
    return @{ kKeyAlwaysKeys:[_alwaysKeys allObjects],
              kKeyFirstUnlockKeys:[_firstUnlockKeys allObjects],
              kKeyUnlockedKeys:[_unlockedKeys allObjects],

#if 0
              kKeyKeyParameterKeys: _keyParameterKeys,
              kKeyMessageKeys : _messageKeys,
              kKeyCircleKeys : _circleKeys,
#endif
              kKeyPendingKeys:[_pendingKeys allObjects],
              kKeySyncWithPeersPending:[NSNumber numberWithBool:_syncWithPeersPending],
              kKeyEnsurePeerRegistration:[NSNumber numberWithBool:_ensurePeerRegistration]
              };
}

- (void) importKeyInterests: (NSDictionary*) interests
{
    _alwaysKeys = [NSMutableSet setWithArray: interests[kKeyAlwaysKeys]];
    _firstUnlockKeys = [NSMutableSet setWithArray: interests[kKeyFirstUnlockKeys]];
    _unlockedKeys = [NSMutableSet setWithArray: interests[kKeyUnlockedKeys]];

    _pendingKeys = [NSMutableSet setWithArray: interests[kKeyPendingKeys]];
    _syncWithPeersPending = [interests[kKeySyncWithPeersPending] boolValue];
    _ensurePeerRegistration = [interests[kKeyEnsurePeerRegistration] boolValue];
}

- (NSMutableSet *)copyAllKeys
{
    NSMutableSet *allKeys = [NSMutableSet setWithSet: _alwaysKeys];
    [allKeys unionSet: _firstUnlockKeys];
    [allKeys unionSet: _unlockedKeys];
    return allKeys;
}

-(void)registerAtTimeKeys:(NSDictionary*)keyparms
{
    NSArray *alwaysArray = [keyparms valueForKey: kKeyAlwaysKeys];
    NSArray *firstUnlockedKeysArray = [keyparms valueForKey: kKeyFirstUnlockKeys];
    NSArray *whenUnlockedKeysArray = [keyparms valueForKey: kKeyUnlockedKeys];
    
    if(alwaysArray)
        [_alwaysKeys unionSet: [NSMutableSet setWithArray: alwaysArray]];
    if(firstUnlockedKeysArray)
        [_firstUnlockKeys unionSet: [NSMutableSet setWithArray: firstUnlockedKeysArray]];
    if(whenUnlockedKeysArray)
        [_unlockedKeys unionSet: [NSMutableSet setWithArray: whenUnlockedKeysArray]];
}


- (void)registerKeys: (NSDictionary*)keys
{
    secdebug(XPROXYSCOPE, "registerKeys: keys: %@", keys);
    
    NSMutableSet *allOldKeys = [self copyAllKeys];

    NSDictionary *keyparms = [keys valueForKey: [NSString stringWithUTF8String: kMessageKeyParameter]];
    NSDictionary *circles = [keys valueForKey: [NSString stringWithUTF8String: kMessageCircle]];
    NSDictionary *messages = [keys valueForKey: [NSString stringWithUTF8String: kMessageMessage]];
    
    _alwaysKeys = [NSMutableSet set];
    _firstUnlockKeys = [NSMutableSet set];
    _unlockedKeys = [NSMutableSet set];
    
    _keyParameterKeys = keyparms;
    _circleKeys = circles;
    _messageKeys = messages;
    
    [self registerAtTimeKeys: _keyParameterKeys];
    [self registerAtTimeKeys: _circleKeys];
    [self registerAtTimeKeys: _messageKeys];

    NSMutableSet *allNewKeys = [self copyAllKeys];
    
    // Make sure keys we no longer care about are not pending
    [_pendingKeys intersectSet:allNewKeys];
    if (_shadowPendingKeys) {
        [_shadowPendingKeys intersectSet:allNewKeys];
    }

    // All new keys only is new keys (remove old keys)
    [allNewKeys minusSet:allOldKeys];

    // Mark new keys pending, they're new!
    NSMutableSet *newKeysForCurrentLockState = [self pendKeysAndGetNewlyPended:allNewKeys];

    [self persistState]; // Before we might call out, save our state so we recover if we crash
    
    [self intersectWithCurrentLockState: newKeysForCurrentLockState];
    // TODO: Don't processPendingKeysForCurrentLockState if none of the new keys have values.
    if ([newKeysForCurrentLockState count] != 0) {
        [self processPendingKeysForCurrentLockState];
    }
}

- (NSDictionary *)localNotification:(NSDictionary *)localNotificationDict outFlags:(int64_t *)outFlags
{
    __block bool done = false;
    __block int64_t returnedFlags = 0;
    __block NSDictionary *responses = NULL;
    typedef bool (^CKDUserInteractionBlock) (CFDictionaryRef responses);
    
    CKDUserInteraction *cui = [CKDUserInteraction sharedInstance];
    [cui requestShowNotification:localNotificationDict completion:^ bool (CFDictionaryRef userResponses, int64_t flags)
     {
         responses = [NSDictionary dictionaryWithDictionary:(__bridge NSDictionary *)userResponses];
         returnedFlags = flags;
         secdebug(XPROXYSCOPE, "%@ requestShowNotification: dict: %@, flags: %#llx", self, responses, returnedFlags);
         done = true;
         return true;
     }];
    
    // TODO: replace with e.g. dispatch calls to wait, or semaphore
    while (!done)
        sleep(1);
    if (outFlags)
    {
        secdebug(XPROXYSCOPE, "%@ outFlags: %#llx", self, returnedFlags);
        *outFlags = returnedFlags;
    }
    return responses;
}

- (void)saveToUbiquitousStore
{
    [self requestSynchronization:NO];
}

// MARK: ----- Event Handling -----

- (void)streamEvent:(xpc_object_t)notification
{
#if (!TARGET_IPHONE_SIMULATOR)
    const char *notificationName = xpc_dictionary_get_string(notification, "Notification");
    if (!notificationName) {
    } else if (strcmp(notificationName, kUserKeybagStateChangeNotification)==0) {
        return [self keybagStateChange];
    } else if (strcmp(notificationName, kCloudKeychainStorechangeChangeNotification)==0) {
        return [self kvsStoreChange];
    } else if (strcmp(notificationName, kNotifyTokenForceUpdate)==0) {
        // DEBUG -- Possibly remove in future
        return [self processAllItems];
    }
    const char *eventName = xpc_dictionary_get_string(notification, "XPCEventName");
    char *desc = xpc_copy_description(notification);
    secerror("%@ event: %s name: %s desc: %s", self, eventName, notificationName, desc);
    if (desc)
        free((void *)desc);
#endif
}

- (void)iCloudAccountAvailabilityChanged:(NSNotification*)notification
{
    /*
     Continuing this example, you’d then implement an iCloudAccountAvailabilityChanged: method that would:
     
     Call the ubiquityIdentityToken method and store its return value.
     Compare the new value to the previous value, to find out if the user logged out of their account or
     logged in to a different account. If the previously-used account is now unavailable, save the current
     state locally as needed, empty your iCloud-related data caches, and refresh all iCloud-related user interface elements.
     */
    id previCloudToken = currentiCloudToken;
    currentiCloudToken = [[NSFileManager defaultManager] ubiquityIdentityToken];
    if (previCloudToken != currentiCloudToken)
        secnotice("event", "%@ iCloud account changed!", self);
    else
        secnotice("event", "%@ %@", self, notification);
}

- (void)cloudChanged:(NSNotification*)notification
{
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
    
    secdebug(XPROXYSCOPE, "%@ cloudChanged notification: %@", self, notification);
    
    NSDictionary *userInfo = [notification userInfo];
    NSNumber *reason = [userInfo objectForKey:NSUbiquitousKeyValueStoreChangeReasonKey];
    if (reason) switch ([reason integerValue]) {
        case NSUbiquitousKeyValueStoreInitialSyncChange:
        case NSUbiquitousKeyValueStoreServerChange:
        {
            _seenKVSStoreChange = YES;
            NSSet *keysChangedInCloud = [NSSet setWithArray:[userInfo objectForKey:NSUbiquitousKeyValueStoreChangedKeysKey]];

            /* We are saying that we want to try processing a key no matter what,
             * *if* it has changed in the cloud. */
            [_pendingKeys minusSet:keysChangedInCloud];

            NSSet *keysOfInterestThatChanged = [self pendKeysAndGetPendingForCurrentLockState:keysChangedInCloud];
            NSMutableDictionary *changedValues = [self copyValues:keysOfInterestThatChanged];
            if ([reason integerValue] == NSUbiquitousKeyValueStoreInitialSyncChange)
                changedValues[(__bridge NSString*)kSOSKVSInitialSyncKey] =  @"true";
            
            secnotice("event", "%@ keysChangedInCloud: %@ keysOfInterest: %@", self, [[keysChangedInCloud allObjects] componentsJoinedByString: @" "], [[changedValues allKeys] componentsJoinedByString: @" "]);
            if ([changedValues count])
                [self processKeyChangedEvent:changedValues];
            break;
        }
        case NSUbiquitousKeyValueStoreQuotaViolationChange:
            seccritical("%@ event received NSUbiquitousKeyValueStoreQuotaViolationChange", self);
            break;
        case NSUbiquitousKeyValueStoreAccountChange:
            // The primary account changed. We do not get this for password changes on the same account
            secnotice("event", "%@ NSUbiquitousKeyValueStoreAccountChange", self);
            NSDictionary *changedValues = @{ (__bridge NSString*)kSOSKVSAccountChangedKey:  @"true" };
            [self processKeyChangedEvent:changedValues];
            break;
    }
}

- (void) doAfterFlush: (dispatch_block_t) block
{
    // Flush any pending communication to Securityd.

    dispatch_async(_calloutQueue, block);
}

- (void) calloutWith: (void(^)(NSSet *pending, bool syncWithPeersPending, bool ensurePeerRegistration, dispatch_queue_t queue, void(^done)(NSSet *handledKeys, bool handledSyncWithPeers, bool handledEnsurePeerRegistration))) callout
{
    // In CKDKVSProxy's serial queue
    dispatch_queue_t ckdkvsproxy_queue = dispatch_get_main_queue();

    _oldInCallout = YES;
    
    // dispatch_get_global_queue - well-known global concurrent queue
    // dispatch_get_main_queue   - default queue that is bound to the main thread
    xpc_transaction_begin();
    dispatch_async(_calloutQueue, ^{
        __block NSSet *myPending;
        __block bool mySyncWithPeersPending;
        __block bool myEnsurePeerRegistration;
        __block bool wasLocked;
        dispatch_sync(ckdkvsproxy_queue, ^{
            myPending = [_pendingKeys copy];
            mySyncWithPeersPending = _syncWithPeersPending;
            myEnsurePeerRegistration = _ensurePeerRegistration;
            wasLocked = _isLocked;

            _inCallout = YES;
            if (!_oldInCallout)
                secnotice("deaf", ">>>>>>>>>>> _oldInCallout is NO and we're heading in to the callout!");

            _shadowPendingKeys = [NSMutableSet set];
            _shadowSyncWithPeersPending = NO;
        });

        callout(myPending, mySyncWithPeersPending, myEnsurePeerRegistration, ckdkvsproxy_queue, ^(NSSet *handledKeys, bool handledSyncWithPeers, bool handledEnsurePeerRegistration) {
            secdebug("event", "%@ %s%s before callout handled: %s%s", self, mySyncWithPeersPending ? "S" : "s", myEnsurePeerRegistration ? "E" : "e", handledSyncWithPeers ? "S" : "s", handledEnsurePeerRegistration ? "E" : "e");
            
            // In CKDKVSProxy's serial queue
            _inCallout = NO;
            _oldInCallout = NO;
           
            // Update ensurePeerRegistration
            _ensurePeerRegistration = ((myEnsurePeerRegistration && !handledEnsurePeerRegistration) || _shadowEnsurePeerRegistration);
            
            _shadowEnsurePeerRegistration = NO;
            
            if(_ensurePeerRegistration && !_isLocked)
                [self doEnsurePeerRegistration];
            
            // Update SyncWithPeers stuff.
            _syncWithPeersPending = ((mySyncWithPeersPending && (!handledSyncWithPeers)) || _shadowSyncWithPeersPending);
            
            _shadowSyncWithPeersPending = NO;
            if (handledSyncWithPeers)
                _lastSyncTime = dispatch_time(DISPATCH_TIME_NOW, 0);
            
            // Update pendingKeys and handle them
            [_pendingKeys minusSet: handledKeys];
            bool hadShadowPendingKeys = [_shadowPendingKeys count];
            // Move away shadownPendingKeys first, because pendKeysAndGetPendingForCurrentLockState
            // will look at them. See rdar://problem/20733166.
            NSSet *oldShadowPendingKeys = _shadowPendingKeys;
            _shadowPendingKeys = nil;
            NSSet *filteredKeys = [self pendKeysAndGetPendingForCurrentLockState:oldShadowPendingKeys];

            // Write state to disk
            [self persistState];
            
            // Handle shadow pended stuff
            if (_syncWithPeersPending && !_isLocked)
                [self scheduleSyncRequestTimer];
            /* We don't want to call processKeyChangedEvent if we failed to
             handle pending keys and the device didn't unlock nor receive
             any kvs changes while we were in our callout.
             Doing so will lead to securityd and CloudKeychainProxy
             talking to each other forever in a tight loop if securityd
             repeatedly returns an error processing the same message.
             Instead we leave any old pending keys until the next event. */
            if (hadShadowPendingKeys || (!_isLocked && wasLocked))
                [self processKeyChangedEvent:[self copyValues:filteredKeys]];
            xpc_transaction_end();
        });
    });
}

- (void) sendKeysCallout: (NSSet *(^)(NSSet* pending, NSError** error)) handleKeys {
    [self calloutWith: ^(NSSet *pending, bool syncWithPeersPending, bool ensurePeerRegistration, dispatch_queue_t queue, void(^done)(NSSet *, bool, bool)) {
        NSError* error = NULL;
        
        NSSet * handled = handleKeys(pending, &error);

        dispatch_async(queue, ^{
            if (!handled) {
                secerror("%@ ensurePeerRegistration failed: %@", self, error);
            }

            done(handled, NO, NO);
        });
    }];
}

- (void) doEnsurePeerRegistration
{
    [self calloutWith:^(NSSet *pending, bool syncWithPeersPending, bool ensurePeerRegistration, dispatch_queue_t queue, void(^done)(NSSet *, bool, bool)) {
        CFErrorRef error = NULL;
        bool handledEnsurePeerRegistration = SOSCCProcessEnsurePeerRegistration(&error);
        secerror("%@ ensurePeerRegistration called, %@ (%@)", self, handledEnsurePeerRegistration ? @"success" : @"failure", error);
        dispatch_async(queue, ^{
            if (!handledEnsurePeerRegistration) {
                secerror("%@ ensurePeerRegistration failed: %@", self, error);
            }
            
            done(nil, NO, handledEnsurePeerRegistration);
            CFReleaseSafe(error);
        });
    }];
}

- (void) doSyncWithAllPeers
{
    [self calloutWith:^(NSSet *pending, bool syncWithPeersPending, bool ensurePeerRegistration, dispatch_queue_t queue, void(^done)(NSSet *, bool, bool)) {
        CFErrorRef error = NULL;
        SyncWithAllPeersReason reason = SOSCCProcessSyncWithAllPeers(&error);
        dispatch_async(queue, ^{
            bool handledSyncWithPeers = NO;
            if (reason == kSyncWithAllPeersSuccess) {
                handledSyncWithPeers = YES;
                secnotice("event", "%@ syncWithAllPeers succeeded", self);
            } else if (reason == kSyncWithAllPeersLocked) {
                secnotice("event", "%@ syncWithAllPeers attempted while locked - waiting for unlock", self);
                handledSyncWithPeers = NO;
                [self updateIsLocked];
            } else if (reason == kSyncWithAllPeersOtherFail) {
                // Pretend we handled syncWithPeers, by pushing out the _lastSyncTime
                // This will cause us to wait for kMinSyncInterval seconds before
                // retrying, so we don't spam securityd if sync is failing
                secerror("%@ syncWithAllPeers %@, rescheduling timer", self, error);
                _lastSyncTime = dispatch_time(DISPATCH_TIME_NOW, 0);
            } else {
                secerror("%@ syncWithAllPeers %@, unknown reason: %d", self, error, reason);
            }
            
            done(nil, handledSyncWithPeers, false);
            CFReleaseSafe(error);
        });
    }];
}

- (void)timerFired
{
    secnotice("event", "%@ syncWithPeersPending: %d inCallout: %d isLocked: %d", self, _syncWithPeersPending, _inCallout, _isLocked);
    _syncTimerScheduled = NO;
    if (_syncWithPeersPending && !_inCallout && !_isLocked)
        [self doSyncWithAllPeers];
}

- (dispatch_time_t) nextSyncTime
{
    dispatch_time_t nextSync = dispatch_time(DISPATCH_TIME_NOW, kMinSyncDelay);
    
    // Don't sync again unless we waited at least kMinSyncInterval
    if (_lastSyncTime) {
        dispatch_time_t soonest = dispatch_time(_lastSyncTime, kMinSyncInterval);
        if (nextSync < soonest || _deadline < soonest) {
            secdebug("timer", "%@ backing off", self);
            return soonest;
        }
    }
    
    // Don't delay more than kMaxSyncDelay after the first request.
    if (nextSync > _deadline) {
        secdebug("timer", "%@ hit deadline", self);
        return _deadline;
    }
    
    // Bump the timer by kMinSyncDelay
    if (_syncTimerScheduled)
        secdebug("timer", "%@ bumped timer", self);
    else
        secdebug("timer", "%@ scheduled timer", self);
    
    return nextSync;
}

- (void)scheduleSyncRequestTimer
{
    dispatch_source_set_timer(_syncTimer, [self nextSyncTime], DISPATCH_TIME_FOREVER, kSyncTimerLeeway);
    _syncTimerScheduled = YES;
}

- (void)requestSyncWithAllPeers // secd calling SOSCCSyncWithAllPeers invokes this
{
#if !defined(NDEBUG)
    NSString *desc = [self description];
#endif
    
    if (!_syncWithPeersPending || (_inCallout && !_shadowSyncWithPeersPending))
        _deadline = dispatch_time(DISPATCH_TIME_NOW, kMaxSyncDelay);
    
    if (!_syncWithPeersPending) {
        _syncWithPeersPending = YES;
        [self persistState];
    }
    
    if (_inCallout)
        _shadowSyncWithPeersPending = YES;
    else if (!_isLocked)
        [self scheduleSyncRequestTimer];
    
    secdebug("event", "%@ %@", desc, self);
}

- (void)requestEnsurePeerRegistration // secd calling SOSCCSyncWithAllPeers invokes this
{
#if !defined(NDEBUG)
    NSString *desc = [self description];
#endif
    
    if (_inCallout) {
        _shadowEnsurePeerRegistration = YES;
    } else {
        _ensurePeerRegistration = YES;
        if (!_isLocked)
            [self doEnsurePeerRegistration];
        [self persistState];
    }
    
    secdebug("event", "%@ %@", desc, self);
}


- (BOOL) updateUnlockedSinceBoot
{
    CFErrorRef aksError = NULL;
    if (!SecAKSGetHasBeenUnlocked(&_unlockedSinceBoot, &aksError)) {
        secerror("%@ Got error from SecAKSGetHasBeenUnlocked: %@", self, aksError);
        CFReleaseSafe(aksError);
        return NO;
    }
    return YES;
}

- (BOOL) updateIsLocked
{
    CFErrorRef aksError = NULL;
    if (!SecAKSGetIsLocked(&_isLocked, &aksError)) {
        secerror("%@ Got error querying lock state: %@", self, aksError);
        CFReleaseSafe(aksError);
        return NO;
    }
    if (!_isLocked)
        _unlockedSinceBoot = YES;
    return YES;
}

- (void) keybagStateChange
{
    BOOL wasLocked = _isLocked;
    if ([self updateIsLocked]) {
        if (wasLocked == _isLocked)
            secdebug("event", "%@ still %s ignoring", self, _isLocked ? "locked" : "unlocked");
        else if (_isLocked)
            [self keybagDidLock];
        else
            [self keybagDidUnlock];
    }
}

- (void) keybagDidLock
{
    secnotice("event", "%@", self);
}

- (void) keybagDidUnlock
{
    secnotice("event", "%@", self);
    if (_ensurePeerRegistration) {
        [self doEnsurePeerRegistration];
    }
    
    // First send changed keys to securityd so it can proccess updates
    [self processPendingKeysForCurrentLockState];
    
    // Then, tickle securityd to perform a sync if needed.
    if (_syncWithPeersPending && !_syncTimerScheduled) {
        [self doSyncWithAllPeers];
    }
}

- (void) kvsStoreChange {
    if (!_seenKVSStoreChange) {
        _seenKVSStoreChange = YES; // Only do this once
        secnotice("event", "%@ received darwin notification before first NSNotification", self);
        // TODO This might not be needed if we always get the NSNotification
        // deleived even if we were launched due to a kvsStoreChange
        // Send all keys for current lock state to securityd so it can proccess them
        [self pendKeysAndGetNewlyPended: [self copyAllKeys]];
        [self processPendingKeysForCurrentLockState];
    } else {
        secdebug("event", "%@ ignored, waiting for NSNotification", self);
    }
}

//
// MARK: ----- Key Filtering -----
//

- (NSSet*) keysForCurrentLockState
{
    secdebug("keytrace", "%@ Filtering: unlockedSinceBoot: %d\n unlocked: %d\n, _keysOfInterest: %@", self, (int) _unlockedSinceBoot, (int) !_isLocked, [self exportKeyInterests]);
    
    NSMutableSet *currentStateKeys = [NSMutableSet setWithSet: _alwaysKeys];
    if (_unlockedSinceBoot)
        [currentStateKeys unionSet: _firstUnlockKeys];
    
    if (!_isLocked)
        [currentStateKeys unionSet: _unlockedKeys];
    
    return currentStateKeys;
}

- (NSMutableSet*) pendKeysAndGetNewlyPended: (NSSet*) keysToPend
{
    NSMutableSet *newlyPendedKeys = [keysToPend mutableCopy];
    [newlyPendedKeys minusSet: _pendingKeys];
    if (_shadowPendingKeys) {
        [newlyPendedKeys minusSet: _shadowPendingKeys];
    }
    
    [_pendingKeys unionSet:keysToPend];
    if (_shadowPendingKeys) {
        [_shadowPendingKeys unionSet:keysToPend];
    }
    
    return newlyPendedKeys;
}

- (void) intersectWithCurrentLockState: (NSMutableSet*) set
{
    [set intersectSet: [self keysForCurrentLockState]];
}

- (NSMutableSet*) pendingKeysForCurrentLockState
{
    NSMutableSet * result = [_pendingKeys mutableCopy];
    [self intersectWithCurrentLockState:result];
    return result;
}

- (NSMutableSet*) pendKeysAndGetPendingForCurrentLockState: (NSSet*) startingSet
{
    [self pendKeysAndGetNewlyPended: startingSet];
    
    return [self pendingKeysForCurrentLockState];
}

- (NSMutableDictionary *)copyValues:(NSSet*)keysOfInterest
{
    // Grab values from KVS.
    NSUbiquitousKeyValueStore *store = [self cloudStore];
    NSMutableDictionary *changedValues = [NSMutableDictionary dictionaryWithCapacity:0];
    [keysOfInterest enumerateObjectsUsingBlock:^(id obj, BOOL *stop)
     {
         NSString* key = (NSString*) obj;
         id objval = [store objectForKey:key];
         if (!objval) objval = [NSNull null];
         
         [changedValues setObject:objval forKey:key];
         secdebug(XPROXYSCOPE, "%@ storeChanged updated value for %@", self, key);
     }];
    return changedValues;
}

/*
 During RegisterKeys, separate keys-of-interest into three disjoint sets:
 - keys that we always want to be notified about; this means we can get the
 value at any time
 - keys that require the device to have been unlocked at least once
 - keys that require the device to be unlocked now
 
 Typically, the sets of keys will be:
 
 - Dk: alwaysKeys
 - Ck: firstUnlock
 - Ak: unlocked
 
 The caller is responsible for making sure that the keys in e.g. alwaysKeys are
 values that can be handled at any time (that is, not when unlocked)
 
 Each time we get a notification from ubiquity that keys have changed, we need to
 see if anything of interest changed. If we don't care, then done.
 
 For each key-of-interest that changed, we either notify the client that things
 changed, or add it to a pendingNotifications list. If the notification to the
 client fails, also add it to the pendingNotifications list. This pending list
 should be written to persistent storage and consulted any time we either get an
 item changed notification, or get a stream event signalling a change in lock state.
 
 We can notify the client either through XPC if a connection is set up, or call a
 routine in securityd to launch it.
 
 */

- (void)processKeyChangedEvent:(NSDictionary *)changedValues
{
    NSMutableDictionary* filtered = [NSMutableDictionary dictionary];
    
    NSMutableArray* nullKeys = [NSMutableArray array];
    // Remove nulls because we don't want them in securityd.
    [changedValues enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL *stop) {
        if (obj == [NSNull null])
            [nullKeys addObject:key];
        else{
            filtered[key] = obj;
        }
    }];
    if ([nullKeys count])
        [_pendingKeys minusSet: [NSSet setWithArray: nullKeys]];
    
    if([filtered count] != 0 ){
        [self sendKeysCallout:^NSSet *(NSSet *pending, NSError** error) {
            CFErrorRef cf_error = NULL;
            NSArray* handledMessage = (__bridge_transfer NSArray*) _SecKeychainSyncUpdateMessage((__bridge CFDictionaryRef)filtered, &cf_error);
            NSError *updateError = (__bridge_transfer NSError*)cf_error;
            if (error)
                *error = updateError;

            secnotice("keytrace", "%@ misc handled: %@ null: %@ pending: %@", self,
                      [handledMessage componentsJoinedByString: @" "],
                      [nullKeys componentsJoinedByString: @" "],
                      [[_pendingKeys allObjects] componentsJoinedByString: @" "]);
            
            return handledMessage ? [NSSet setWithArray: handledMessage] : nil;
        }];
    } else {
        secnotice("keytrace", "null: %@ pending: %@",
                  [nullKeys componentsJoinedByString: @" "],
                  [[_pendingKeys allObjects] componentsJoinedByString: @" "]);
    }
}

- (void) processPendingKeysForCurrentLockState
{
    [self processKeyChangedEvent: [self copyValues: [self pendingKeysForCurrentLockState]]];
}

@end


