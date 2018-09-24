//
//  main.m
//  DeviceSimulator
//
//

#import <Foundation/Foundation.h>
#import <Foundation/NSXPCConnection_Private.h>
#import <SOSCircle/CKBridge/SOSCloudKeychainConstants.h>
#import <objc/runtime.h>
#import <utilities/debugging.h>

#import <securityd/SOSCloudCircleServer.h>
#import <Security/SecureObjectSync/SOSPeerInfo.h>
#import <Security/SecureObjectSync/SOSCloudCircleInternal.h>
#import <Security/SecureObjectSync/SOSViews.h>
#import <Security/SecureObjectSync/SOSInternal.h>

#import "DeviceSimulator.h"
#import "SOSCloudKeychainClient.h"
#import "MultiDeviceNetworkingProtocol.h"
#import "SecCFWrappers.h"
#import "spi.h"

struct SOSCloudTransport MDNTransport;

@class MDNetwork;

NSString *deviceInstance = NULL;
static NSString *deviceHomeDir = NULL;
static MDNetwork *deviceNetwork = NULL;

@interface MDNetwork : NSObject<MultiDeviceNetworkingProtocol,MultiDeviceNetworkingCallbackProtocol,NSXPCListenerDelegate>
@property NSXPCConnection *connection;
@property NSXPCListener *callbackListener;
@property dispatch_queue_t flushQueue;
@property NSMutableDictionary *pendingKeys;
@property NSSet *registeredKeys;
- (instancetype)initWithConnection:(NSXPCConnection *)connection;
@end

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wprotocol"
@implementation MDNetwork

- (instancetype)initWithConnection:(NSXPCConnection *)connection
{
    self = [super init];
    if (self) {
        self.connection = connection;
        self.callbackListener = [NSXPCListener anonymousListener];
        self.callbackListener.delegate = self;
        [self.callbackListener resume];

        __typeof(self) weakSelf = self;
        self.connection.invalidationHandler = ^{
            __typeof(self) strongSelf = weakSelf;
            [strongSelf.callbackListener invalidate];
            strongSelf.callbackListener = nil;

            exit(0);
        };

        self.flushQueue = dispatch_queue_create("MDNetwork.flushqueue", 0);
        self.pendingKeys = [NSMutableDictionary dictionary];
        self.registeredKeys = [NSSet set];

        [[self.connection synchronousRemoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
            NSLog(@"network register callback failed with: %@", error);
            //abort();
        }] MDNRegisterCallback:[self.callbackListener endpoint] complete:^void(NSDictionary *values, NSError *error) {
            ;
        }];

    }
    return self;
}

- (BOOL)listener:(NSXPCListener *)listener shouldAcceptNewConnection:(NSXPCConnection *)newConnection
{
    newConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(MultiDeviceNetworkingCallbackProtocol)];
    newConnection.exportedObject = self;
    [newConnection resume];
    return YES;
}

- (void)MDNCItemsChanged:(NSDictionary *)values complete:(MDNComplete)complete
{
    NSMutableDictionary *requestedKeys = [NSMutableDictionary dictionary];
    @synchronized(self.pendingKeys) {
        secnotice("MDN", "items update: %@ (already pending: %@)", values, self.pendingKeys);

        [self.pendingKeys addEntriesFromDictionary:values];
        for (NSString *key in self.registeredKeys) {
            id data = self.pendingKeys[key];
            if (data) {
                requestedKeys[key] = data;
                self.pendingKeys[key] = nil;
            }
        }
    }
    if (requestedKeys.count) {
        dispatch_async(self.flushQueue, ^{
            secnotice("MDN", "engine processing keys: %@", requestedKeys);
            NSArray *handled = CFBridgingRelease(SOSCCHandleUpdateMessage((__bridge CFDictionaryRef)requestedKeys));
            /*
             * Ok, our dear Engine might not have handled all messages.
             * So put them back unless there are new messages around that
             * have overwritten the previous message.
             */
            for (NSString *key in handled) {
                requestedKeys[key] = NULL;
            }
            if (requestedKeys.count) {
                @synchronized(self.pendingKeys) {
                    for (NSString *key in requestedKeys) {
                        if (self.pendingKeys[key] == nil) {
                            self.pendingKeys[key] = requestedKeys[key];
                        }
                    }
                }
            }
        });
    }
    complete(NULL, NULL);
}

/* Oh, ObjC, you are my friend */
- (void)forwardInvocation:(NSInvocation *)invocation
{
    struct objc_method_description desc = protocol_getMethodDescription(@protocol(MultiDeviceNetworkingProtocol), [invocation selector], true, true);
    if (desc.name == NULL) {
        [super forwardInvocation:invocation];
    } else {
        __block bool gogogo = true;
        id object = [self.connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
            gogogo = false;
            NSLog(@"network failed with: %@", error);
            //abort();
        }];
        if(gogogo) [invocation invokeWithTarget:object];
    }
}
@end
#pragma clang diagnostic pop

#define HANDLE_NO_NETWORK(_replyBlock) \
    if (deviceNetwork == NULL) { \
        replyBlock((__bridge CFDictionaryRef)@{}, (__bridge CFErrorRef)[NSError errorWithDomain:@"MDNNetwork" code:1 userInfo:NULL]); \
        return; \
    }


static void
DSCloudPut(SOSCloudTransportRef transport, CFDictionaryRef valuesToPut, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    @autoreleasepool {
        HANDLE_NO_NETWORK(replyBlock);
        secnotice("MDN", "CloudPut: %@", valuesToPut);

        [deviceNetwork MDNCloudPut:(__bridge NSDictionary *)valuesToPut complete:^(NSDictionary *returnedValues, NSError *error) {
            dispatch_async(processQueue, ^{
                replyBlock((__bridge CFDictionaryRef)returnedValues, (__bridge CFErrorRef)error);
            });
        }];
    }
}

static NSString *nKeyAlwaysKeys = @"AlwaysKeys";
static NSString *nKeyFirstUnlockKeys = @"FirstUnlockKeys";
static NSString *nKeyUnlockedKeys = @"UnlockedKeys";
static NSString *nMessageKeyParameter = @"KeyParameter";
static NSString *nMessageCircle = @"Circle";
static NSString *nMessageMessage = @"Message";


static void
DSCloudUpdateKeys(SOSCloudTransportRef transport, CFDictionaryRef cfkeys, CFStringRef accountUUID, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    /*
     * Currently doesn't deal with lock state, just smash (HULK!) them all together
     */
    @autoreleasepool {
        NSDictionary *keys = (__bridge NSDictionary *)cfkeys;
        NSMutableSet *newSet = [NSMutableSet set];

        @synchronized(deviceNetwork.pendingKeys) {
            for (NSString *type in @[ nMessageKeyParameter, nMessageCircle, nMessageMessage]) {
                NSDictionary *typeDict = keys[type];

                for (NSString *lockType in @[ nKeyAlwaysKeys, nKeyFirstUnlockKeys, nKeyUnlockedKeys]) {
                    NSArray *lockArray = typeDict[lockType];
                    if (lockArray) {
                        [newSet unionSet:[NSMutableSet setWithArray:lockArray]];
                    }
                }
            }
            deviceNetwork.registeredKeys = newSet;
        }
        /* update engine with stuff */
        [deviceNetwork MDNCItemsChanged:@{} complete:^(NSDictionary *returnedValues, NSError *error) {
            if (replyBlock)
                replyBlock((__bridge CFDictionaryRef)returnedValues, (__bridge CFErrorRef)error);
        }];
    }
}

static void
DSCloudGetDeviceID(SOSCloudTransportRef transport, CloudKeychainReplyBlock replyBlock)
{
    if (replyBlock)
        replyBlock((__bridge CFDictionaryRef)@{}, NULL);
}

// Debug calls
static void
DSCloudGet(SOSCloudTransportRef transport, CFArrayRef keysToGet, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    if (replyBlock)
        replyBlock((__bridge CFDictionaryRef)@{}, NULL);
}

static void
DSCloudGetAll(SOSCloudTransportRef transport, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    if (replyBlock)
        replyBlock((__bridge CFDictionaryRef)@{}, NULL);
}

static void
DSCloudsynchronize(SOSCloudTransportRef transport, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    if (replyBlock)
        replyBlock((__bridge CFDictionaryRef)@{}, NULL);
}

static void
DSCloudsynchronizeAndWait(SOSCloudTransportRef transport, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    @autoreleasepool {
        HANDLE_NO_NETWORK(replyBlock);

        [deviceNetwork MDNCloudsynchronizeAndWait:@{} complete:^(NSDictionary *returnedValues, NSError *error) {
            dispatch_async(processQueue, ^{
                replyBlock((__bridge CFDictionaryRef)returnedValues, (__bridge CFErrorRef)error);
            });
        }];
    }
}

static void
DSCloudRemoveObjectForKey(SOSCloudTransportRef transport, CFStringRef keyToRemove, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    if (keyToRemove == NULL) {
        dispatch_async(processQueue, ^{
            replyBlock(NULL, NULL);
        });
        return;
    }

    @autoreleasepool {
        HANDLE_NO_NETWORK(replyBlock);
        [deviceNetwork MDNCloudRemoveKeys:@[(__bridge NSString *)keyToRemove] complete:^(NSDictionary *returnedValues, NSError *error) {
            dispatch_async(processQueue, ^{
                replyBlock((__bridge CFDictionaryRef)returnedValues, (__bridge CFErrorRef)error);
            });
        }];
    }
}

static void DSCloudremoveKeys(SOSCloudTransportRef transport, CFArrayRef keys, CFStringRef accountUUID, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    @autoreleasepool {
        HANDLE_NO_NETWORK(replyBlock);
        [deviceNetwork MDNCloudRemoveKeys:(__bridge NSArray *)keys complete:^(NSDictionary *returnedValues, NSError *error) {
            dispatch_async(processQueue, ^{
                replyBlock((__bridge CFDictionaryRef)returnedValues, (__bridge CFErrorRef)error);
            });
        }];
    }
}

static void
DSCloudclearAll(SOSCloudTransportRef transport, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    @autoreleasepool {
        HANDLE_NO_NETWORK(replyBlock);
        [deviceNetwork MDNCloudRemoveKeys:NULL complete:^(NSDictionary *returnedValues, NSError *error) {
            dispatch_async(processQueue, ^{
                replyBlock((__bridge CFDictionaryRef)returnedValues, (__bridge CFErrorRef)error);
            });
        }];
    }
}

static bool
DSCloudhasPendingKey(SOSCloudTransportRef transport, CFStringRef keyName, CFErrorRef* error)
{
    bool status = false;
    @synchronized(deviceNetwork.pendingKeys) {
        status = deviceNetwork.pendingKeys[(__bridge NSString *)keyName] != nil;
    }
    return status;
}


static void
DSCloudrequestSyncWithPeers(SOSCloudTransportRef transport, CFArrayRef /* CFStringRef */ peers, CFArrayRef /* CFStringRef */ backupPeers, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    CFSetRef sPeers = CFSetCreateCopyOfArrayForCFTypes(peers);
    CFSetRef sBackupPeers = CFSetCreateCopyOfArrayForCFTypes(backupPeers);

    if (sPeers == NULL || sBackupPeers == NULL) {
        CFReleaseNull(sPeers);
        CFReleaseNull(sBackupPeers);
    } else {
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            CFErrorRef error = NULL;
            CFSetRef result = SOSCCProcessSyncWithPeers_Server(sPeers, sBackupPeers, &error);

            CFRelease(sPeers);
            CFRelease(sBackupPeers);
            CFReleaseNull(result);
            CFReleaseNull(error);
        });
    }
    if (replyBlock) {
        dispatch_async(processQueue, ^{
            replyBlock((__bridge CFDictionaryRef)@{}, NULL);
        });
    }
}

static bool
DSCloudhasPeerSyncPending(SOSCloudTransportRef transport, CFStringRef peerID, CFErrorRef* error)
{
    return false;
}

static void
DSCloudrequestEnsurePeerRegistration(SOSCloudTransportRef transport, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        CFErrorRef eprError = NULL;
        if (!SOSCCProcessEnsurePeerRegistration_Server(&eprError)) {
            secnotice("coder", "SOSCCProcessEnsurePeerRegistration failed with: %@", eprError);
        }
        CFReleaseNull(eprError);
        if (replyBlock)
            replyBlock((__bridge CFDictionaryRef)@{}, NULL);
    });
}

static void DSCloudrequestPerfCounters(SOSCloudTransportRef transport, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    if (replyBlock)
        replyBlock((__bridge CFDictionaryRef)@{}, NULL);
}

static void DSCloudflush(SOSCloudTransportRef transport, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    @autoreleasepool {
        HANDLE_NO_NETWORK(replyBlock);

        [deviceNetwork MDNCloudFlush:^(NSDictionary *returnedValues, NSError *error) {
            dispatch_async(deviceNetwork.flushQueue, ^{
                dispatch_async(processQueue, ^{
                    replyBlock((__bridge CFDictionaryRef)returnedValues, (__bridge CFErrorRef)error);
                });
            });
        }];
    }
}

static void DSCloudcounters(SOSCloudTransportRef transport, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    if (replyBlock)
        replyBlock((__bridge CFDictionaryRef)@{}, NULL);
}

@interface ServiceDelegate : NSObject <NSXPCListenerDelegate>
@end

@implementation ServiceDelegate

- (BOOL)listener:(NSXPCListener *)listener shouldAcceptNewConnection:(NSXPCConnection *)newConnection {
    newConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(DeviceSimulatorProtocol)];
    
    DeviceSimulator *exportedObject = [DeviceSimulator new];
    exportedObject.conn  = newConnection;
    newConnection.exportedObject = exportedObject;
    
    [newConnection resume];
    
    return YES;
}

@end

void
boot_securityd(NSXPCListenerEndpoint *network)
{
    secLogDisable();
    securityd_init((__bridge CFURLRef)[NSURL URLWithString:deviceHomeDir]);

    NSXPCConnection *connection = [[NSXPCConnection alloc] initWithListenerEndpoint:network];
    connection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(MultiDeviceNetworkingProtocol)];
    [connection resume];

    deviceNetwork = [[MDNetwork alloc] initWithConnection:connection];

}

/*
 * Make sure each of th peers don't trample on each's others state
 */

@interface SOSCachedNotification (override)
@end

@implementation SOSCachedNotification (override)
+ (NSString *)swizzled_notificationName:(const char *)notificationName
{
    return [NSString stringWithFormat:@"%@-%@",
            [SOSCachedNotification swizzled_notificationName:notificationName], deviceInstance];
}

+ (void)load {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        Method orignal = class_getClassMethod(self, @selector(notificationName:));
        Method swizzled = class_getClassMethod(self, @selector(swizzled_notificationName:));
        method_exchangeImplementations(orignal, swizzled);
    });
}
@end


int main(int argc, const char *argv[])
{
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    
    deviceInstance = [[NSXPCListener _UUID] UUIDString];

    NSURL *tempPath = [[NSFileManager defaultManager] temporaryDirectory];
    deviceHomeDir = [[tempPath path] stringByAppendingPathComponent:deviceInstance];

    [[NSFileManager defaultManager] createDirectoryAtPath:deviceHomeDir
                              withIntermediateDirectories:NO
                                               attributes:NULL
                                                    error:NULL];

    MDNTransport.put = DSCloudPut;
    MDNTransport.updateKeys = DSCloudUpdateKeys;
    MDNTransport.getDeviceID = DSCloudGetDeviceID;
    MDNTransport.get = DSCloudGet;
    MDNTransport.getAll = DSCloudGetAll;
    MDNTransport.synchronize = DSCloudsynchronize;
    MDNTransport.synchronizeAndWait = DSCloudsynchronizeAndWait;
    MDNTransport.clearAll = DSCloudclearAll;
    MDNTransport.removeObjectForKey = DSCloudRemoveObjectForKey;
    MDNTransport.hasPendingKey = DSCloudhasPendingKey;
    MDNTransport.requestSyncWithPeers = DSCloudrequestSyncWithPeers;
    MDNTransport.hasPeerSyncPending = DSCloudhasPeerSyncPending;
    MDNTransport.requestEnsurePeerRegistration = DSCloudrequestEnsurePeerRegistration;
    MDNTransport.requestPerfCounters = DSCloudrequestPerfCounters;
    MDNTransport.flush = DSCloudflush;
    MDNTransport.itemsChangedBlock = CFBridgingRetain(^CFArrayRef(CFDictionaryRef values) {
        // default change block doesn't handle messages, keep em
        return CFBridgingRetain(@[]);
    });
    MDNTransport.removeKeys = DSCloudremoveKeys;
    MDNTransport.counters = DSCloudcounters;

    SOSCloudTransportSetDefaultTransport(&MDNTransport);

    // Create the delegate for the service.
    ServiceDelegate *delegate = [ServiceDelegate new];
    signal(SIGPIPE, SIG_IGN);

    // Set up the one NSXPCListener for this service. It will handle all incoming connections.
    NSXPCListener *listener = [NSXPCListener serviceListener];
    listener.delegate = delegate;
    
    // Resuming the serviceListener starts this service. This method does not return.
    [listener resume];
    return 0;
}
