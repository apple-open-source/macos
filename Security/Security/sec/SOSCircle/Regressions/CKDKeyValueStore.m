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
//  CKDKeyValueStore.m
//  sec
//

#import "CKDKeyValueStore.h"
#import "CKDPersistentState.h"

/*
    pseudo-code
    
    g = [CKDKeyValueStore defaultStore:mystoreID];
    [g setObject:obj forKey:@"foo"];
    [g synchronize];
*/

#define DONTUSENOTIFICATIONS true

static const int verboseCKDKVSDebugging = false;

#ifndef NDEBUG
    #define pdebug(format...) \
        do {  \
            if (verboseCKDKVSDebugging) \
                NSLog(format);  \
        } while (0)
#else
    //empty
    #define pdebug(format...)
#endif


extern CFStringRef kCKDKVSRemoteStoreID;
CFStringRef kCKDKVSRemoteStoreID = CFSTR("REMOTE");
CFStringRef kCKDAWSRemoteStoreID = CFSTR("AWS");

NSString * const kCKDKVSWhoChangedItemKey = @"WhoChangedItemKey";

static NSString * const ourNSUbiquitousKeyValueStoreDidChangeExternallyNotification = @"ourNSUbiquitousKeyValueStoreDidChangeExternallyNotification";

// MARK: ----- CKDKeyValueStore -----

@implementation CKDKeyValueStore

- (id)initWithIdentifier:(NSString *)identifier itemsChangedBlock:(CloudItemsChangedBlock)itemsChangedBlock
{
    if (self = [super init])
    {
        self.delegate = self;
        self.identifier = identifier;
        self->localKVS = true;
        
        // copy blocks onto heap
        itemsChangedCallback = Block_copy(itemsChangedBlock);
        
         [[NSNotificationCenter defaultCenter] addObserver: self
            selector: @selector (iCloudAccountAvailabilityChanged:)
            name: NSUbiquityIdentityDidChangeNotification
            object: nil];
        
        [[NSNotificationCenter defaultCenter] addObserver:self 
            selector:@selector(cloudChanged:)
            name:ourNSUbiquitousKeyValueStoreDidChangeExternallyNotification
            object:nil];
    }
    return self;
}

+ (CKDKeyValueStore *)defaultStore:(NSString *)identifier itemsChangedBlock:(CloudItemsChangedBlock)itemsChangedBlock
{
    return [CKDKeyValueStoreCollection defaultStore:identifier itemsChangedBlock:itemsChangedBlock];
}

- (BOOL)synchronize
{
    BOOL value = NO;
    value = [CKDKeyValueStoreCollection enqueueSyncWithReply];

    return value;
}

- (BOOL)isLocalKVS
{
    return YES;
}

- (id)objectForKey:(NSString *)aKey
{
    pdebug(@"retrieving value for key \"%@\"", aKey);
    id value = [CKDKeyValueStoreCollection enqueueWithReply:aKey];
    pdebug(@"retrieved value for key \"%@\": %@", aKey, value);
    return value;
}

- (void)setObject:(id)anObject forKey:(NSString *)aKey
{
    pdebug(@"setting value for key \"%@\"", aKey);
    [CKDKeyValueStoreCollection enqueueWrite:anObject forKey:aKey from:self.identifier];
}

- (void)removeObjectForKey:(NSString *)aKey
{
    pdebug(@"removing value for key \"%@\"", aKey);
    [CKDKeyValueStoreCollection enqueueWrite:NULL forKey:aKey from:self.identifier];
}

- (NSDictionary *)dictionaryRepresentation
{
    pdebug(@"retrieving dictionaryRepresentation");
    id value = [CKDKeyValueStoreCollection enqueueWithReply:NULL];
    pdebug(@"retrieved dictionaryRepresentation: %@", value);
    return value;
}

- (void)setDictionaryRepresentation:(NSMutableDictionary *)initialValue
{
    // DEBUG
    [CKDKeyValueStoreCollection enqueueWrite:initialValue forKey:NULL from:self.identifier];
}

- (void)clearPersistentStores
{
}

+ (CFStringRef)remoteStoreID
{
    return kCKDKVSRemoteStoreID;
}

// MARK: ----- copied from real kvs -----

- (id)initWithItemsChangedBlock:(CloudItemsChangedBlock)itemsChangedBlock
{
    if (self = [super init])
    {
        // copy blocks onto heap
        itemsChangedCallback = Block_copy(itemsChangedBlock);
        
         [[NSNotificationCenter defaultCenter] addObserver: self
            selector: @selector (iCloudAccountAvailabilityChanged:)
            name: NSUbiquityIdentityDidChangeNotification
            object: nil];
        
        [[NSNotificationCenter defaultCenter] addObserver:self 
            selector:@selector(cloudChanged:)
            name:ourNSUbiquitousKeyValueStoreDidChangeExternallyNotification
            object:nil];
    }
    return self;
}

- (void)dealloc
{

    [[NSNotificationCenter defaultCenter] removeObserver:self
        name:ourNSUbiquitousKeyValueStoreDidChangeExternallyNotification object:nil];

    [[NSNotificationCenter defaultCenter] removeObserver:self 
        name:NSUbiquityIdentityDidChangeNotification object:nil];

    Block_release(itemsChangedCallback);
    [super dealloc];
}

- (void)cloudChanged:(NSNotification*)notification
{
    /*
        Posted when the value of one or more keys in the local key-value store changed due to incoming data pushed from iCloud.
        This notification is sent only upon a change received from iCloud; it is not sent when your app sets a value.

        The user info dictionary can contain the reason for the notification as well as a list of which values changed, as follows:

        The value of the NSUbiquitousKeyValueStoreChangeReasonKey key, when present, indicates why the key-value store changed. 
        Its value is one of the constants in “Change Reason Values .” The value of the NSUbiquitousKeyValueStoreChangedKeysKey, 
        when present, is an array of strings, each the name of a key whose value changed. The notification object is the 
        NSUbiquitousKeyValueStore object whose contents changed.
        
        enum {
            NSUbiquitousKeyValueStoreServerChange NS_ENUM_AVAILABLE(10_7, 5_0),
            NSUbiquitousKeyValueStoreInitialSyncChange NS_ENUM_AVAILABLE(10_7, 5_0),
            NSUbiquitousKeyValueStoreQuotaViolationChange NS_ENUM_AVAILABLE(10_7, 5_0),
            NSUbiquitousKeyValueStoreAccountChange NS_ENUM_AVAILABLE(10_8, 6_0)
        };

    */
    
    NSDictionary *userInfo = [notification userInfo];
    NSNumber *reason = [userInfo objectForKey:NSUbiquitousKeyValueStoreChangeReasonKey];
    NSInteger reasonValue = -1;
    
    pdebug(@"cloudChanged notification: %@", notification);

    NSString *whoChangedIt = [userInfo objectForKey:kCKDKVSWhoChangedItemKey];
    
    if (self.identifier && whoChangedIt && [self.identifier isEqualToString:whoChangedIt])
    {
        pdebug(@"cloudChanged by us (%@), ignoring event", self.identifier);
        return;
    }
    
    if (reason)
    {
        reasonValue = [reason integerValue];
        NSArray *reasonStrings = [NSArray arrayWithObjects:@"Server", @"InitialSync", @"QuotaViolation", @"Account", @"unknown", nil];
        long ridx = (NSUbiquitousKeyValueStoreServerChange <= reasonValue && reasonValue <= NSUbiquitousKeyValueStoreAccountChange)?reasonValue : 5;
        pdebug(@"cloudChanged with reason %ld (%@ Change)", (long)reasonValue, [reasonStrings objectAtIndex:ridx]);
    }
    
    if ((reasonValue == NSUbiquitousKeyValueStoreServerChange) ||
        (reasonValue == NSUbiquitousKeyValueStoreInitialSyncChange))
    {
        NSArray *keysChangedInCloud = [userInfo objectForKey:NSUbiquitousKeyValueStoreChangedKeysKey];
        pdebug(@"keysChangedInCloud: %@", keysChangedInCloud);
        NSMutableDictionary *changedValues = [NSMutableDictionary dictionaryWithCapacity:0];
        [keysChangedInCloud enumerateObjectsUsingBlock:^(id obj, NSUInteger idx, BOOL *stop)
        {
            NSString *key = (NSString *)obj;
        //    itemChangedCallback(key, [self.store objectForKey:key]);
            id anObject = @"FIXME"; //[self.store objectForKey:key];
            [changedValues setObject:anObject forKey:key];

            pdebug(@"storeChanged updated value for %@", key);
        }];
        itemsChangedCallback((CFDictionaryRef)changedValues); // fix me *************************
    }
}



@end

// MARK: ----- CKDKeyValueStoreCollection -----

@implementation CKDKeyValueStoreCollection

- (id)init
{
    if (self = [super init])
    {
        self.collection = [NSMutableDictionary dictionaryWithCapacity:0];
        self->syncrequestqueue = dispatch_queue_create("syncrequestqueue", DISPATCH_QUEUE_SERIAL);
        self->store = [NSMutableDictionary dictionaryWithCapacity:0];
    }
    return self;
}

// maybe should return (CKDKeyValueStore *), main thing is that it matches the protocol
+ (id <CKDKVSDelegate>)defaultStore:(NSString *)identifier itemsChangedBlock:(CloudItemsChangedBlock)itemsChangedBlock
{
    // look it up in the collection and return singleton
    if (identifier == NULL)
        return (id <CKDKVSDelegate>)[NSUbiquitousKeyValueStore defaultStore];

    CKDKeyValueStoreCollection *mall = [CKDKeyValueStoreCollection sharedInstance];
    id <CKDKVSDelegate> store =  mall.collection[identifier];
    if (!store)
    {
        store = [[CKDKeyValueStore alloc] initWithIdentifier:identifier itemsChangedBlock:itemsChangedBlock];
        mall->_collection[identifier] = store;
    }
    return store;

}

+ (id)sharedInstance
{
    static dispatch_once_t once;
    static CKDKeyValueStoreCollection *sharedStoreCollection;
    dispatch_once(&once, ^ { sharedStoreCollection = [[self alloc] init]; });
    return sharedStoreCollection;
}

+ (void)enqueueWrite:(id)anObject forKey:(NSString *)aKey from:(NSString *)identifier
{
    CKDKeyValueStoreCollection *mall = [CKDKeyValueStoreCollection sharedInstance];
    dispatch_async(mall->syncrequestqueue, ^void ()
    {
        if (aKey==NULL && (CFGetTypeID(anObject)==CFDictionaryGetTypeID()))
        {
            [mall->store setDictionary:anObject];
            [self postItemsChangedNotification:[anObject allKeys] from:identifier];
        }
        else
        {
            if (anObject)
                [mall->store setObject:anObject forKey:aKey];
            else
                [mall->store removeObjectForKey:aKey];
            [CKDKeyValueStoreCollection postItemChangedNotification:aKey from:identifier];
        }

    });
}

+ (id)enqueueWithReply:(NSString *)aKey
{
    __block id value = NULL;
    CKDKeyValueStoreCollection *mall = [CKDKeyValueStoreCollection sharedInstance];
    dispatch_sync(mall->syncrequestqueue,  ^void ()
    {
        value = (aKey==NULL)?mall->store:[mall->store objectForKey:aKey];
    });
    return value;
}

+ (BOOL)enqueueSyncWithReply
{
    // basically a barrier
    __block BOOL value = false;
    CKDKeyValueStoreCollection *mall = [CKDKeyValueStoreCollection sharedInstance];
    dispatch_sync(mall->syncrequestqueue,  ^void ()
    {
        value = true;
    });
    return value;
}

+ (void)postItemChangedNotification:(NSString *)keyThatChanged from:(NSString *)identifier
{
    // convenience routine when a single key changes
    NSArray *keysThatChanged = [NSArray arrayWithObject:keyThatChanged];
    [self postItemsChangedNotification:keysThatChanged from:identifier];
//  [keysThatChanged release];
}

+ (void)postItemsChangedNotification:(NSArray *)keysThatChanged from:(NSString *)identifier
{
    // add in array of keys plus the id of who changed it
    NSDictionary *aUserInfo = [NSDictionary dictionaryWithObjectsAndKeys:
        keysThatChanged, NSUbiquitousKeyValueStoreChangedKeysKey,
        @(NSUbiquitousKeyValueStoreServerChange), NSUbiquitousKeyValueStoreChangeReasonKey,
        identifier, kCKDKVSWhoChangedItemKey,
        nil];

    [[NSNotificationCenter defaultCenter] postNotificationName:ourNSUbiquitousKeyValueStoreDidChangeExternallyNotification
        object:nil userInfo:aUserInfo];
 //       NSArray *keysChangedInCloud = [userInfo objectForKey:NSUbiquitousKeyValueStoreChangedKeysKey];
}


@end

