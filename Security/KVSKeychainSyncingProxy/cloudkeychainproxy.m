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
//  main.m
//  ckd-xpc
//
//

/*
    This XPC service is essentially just a proxy to iCloud KVS, which exists since
    the main security code cannot link against Foundation.
    
    See sendTSARequestWithXPC in tsaSupport.c for how to call the service
    
    send message to app with xpc_connection_send_message
    
    For now, build this with:
    
        ~rc/bin/buildit .  --rootsDirectory=/var/tmp -noverify -offline -target CloudKeychainProxy

    and install or upgrade with:
        
        darwinup install /var/tmp/sec.roots/sec~dst
        darwinup upgrade /var/tmp/sec.roots/sec~dst
 
    You must use darwinup during development to update system caches
*/

//------------------------------------------------------------------------------------------------

#include <AssertMacros.h>
#include <TargetConditionals.h>

#import <Foundation/Foundation.h>
#import <Security/Security.h>
#import <utilities/SecCFRelease.h>
#import <xpc/xpc.h>
#import <xpc/private.h>
#import <CoreFoundation/CFXPCBridge.h>
#import <sysexits.h>
#import <syslog.h>
#import <CommonCrypto/CommonDigest.h>
#include <utilities/SecXPCError.h>
#include <utilities/SecCFError.h>

#include <utilities/SecFileLocations.h>

#import "CKDKVSProxy.h"
#import "CKDSecuritydAccount.h"
#import "CKDKVSStore.h"
#import "CKDAKSLockMonitor.h"
#import "SOSCloudKeychainConstants.h"

#include <utilities/simulatecrash_assert.h>


#define PROXYXPCSCOPE "xpcproxy"

@interface CloudKeychainProxy : NSObject
-(id _Nullable) init;

@property (nonatomic, retain) UbiqitousKVSProxy *proxyID;
@property (nonatomic, retain) xpc_connection_t listener;
@property (nonatomic, retain) dispatch_source_t sigterm_source;
@property (nonatomic, retain) NSURL *registrationFileName;

+ (CloudKeychainProxy *) sharedObject;
- (void) cloudkeychainproxy_peer_dictionary_handler: (const xpc_connection_t) peer forEvent: (xpc_object_t) event;

@end

static void cloudkeychainproxy_event_handler(xpc_connection_t peer)
{
    if (xpc_get_type(peer) != XPC_TYPE_CONNECTION) {
        secinfo(PROXYXPCSCOPE, "expected XPC_TYPE_CONNECTION");
        return;
    }

    xpc_object_t ent = xpc_connection_copy_entitlement_value(peer, "com.apple.CloudKeychainProxy.client");
    if (ent == NULL || xpc_get_type(ent) != XPC_TYPE_BOOL || xpc_bool_get_value(ent) != true) {
        secnotice(PROXYXPCSCOPE, "cloudkeychainproxy_event_handler: rejected client %d", xpc_connection_get_pid(peer));
        xpc_connection_cancel(peer);
        return;
    }

    xpc_connection_set_target_queue(peer, [[CloudKeychainProxy sharedObject].proxyID ckdkvsproxy_queue]);
    xpc_connection_set_event_handler(peer, ^(xpc_object_t event)
    {
        // We could handle other peer events (e.g.) disconnects,
        // but we don't keep per-client state so there is no need.
        if (xpc_get_type(event) == XPC_TYPE_DICTIONARY) {
            [[CloudKeychainProxy sharedObject]   cloudkeychainproxy_peer_dictionary_handler: peer forEvent: event];
        }
    });
    
    // This will tell the connection to begin listening for events. If you
    // have some other initialization that must be done asynchronously, then
    // you can defer this call until after that initialization is done.
    xpc_connection_resume(peer);
}

static void finalize_connection(void *not_used) {
    secinfo(PROXYXPCSCOPE, "finalize_connection");
    [[CloudKeychainProxy sharedObject].proxyID synchronizeStore];
    xpc_transaction_end();
}

@implementation CloudKeychainProxy

static CFStringRef kRegistrationFileName = CFSTR("com.apple.security.cloudkeychainproxy3.keysToRegister.plist");

+ (CloudKeychainProxy *) sharedObject {
    static CloudKeychainProxy *sharedCKP = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sharedCKP = [CloudKeychainProxy new];
    });
    
    return sharedCKP;
}

-(id _Nullable) init {
    if ((self = [super init])) {
        _registrationFileName = (NSURL *)CFBridgingRelease(SecCopyURLForFileInPreferencesDirectory(kRegistrationFileName));
        _proxyID = [UbiqitousKVSProxy withAccount: [CKDSecuritydAccount securitydAccount]
                                          store: [CKDKVSStore kvsInterface]
                                    lockMonitor: [CKDAKSLockMonitor monitor]
                                    persistence: _registrationFileName];
        
        _listener = xpc_connection_create_mach_service(kCKPServiceName, NULL, XPC_CONNECTION_MACH_SERVICE_LISTENER);
        xpc_connection_set_finalizer_f(_listener, finalize_connection);
        xpc_connection_set_event_handler(_listener, ^(xpc_object_t object){ cloudkeychainproxy_event_handler(object); });

        // It looks to me like there is insufficient locking to allow a request to come in on the XPC connection while doing the initial all items.
        // Therefore I'm leaving the XPC connection suspended until that has time to process.
        xpc_connection_resume(_listener);

        (void)signal(SIGTERM, SIG_IGN);
        _sigterm_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGTERM, 0, _proxyID.ckdkvsproxy_queue);
        dispatch_source_set_event_handler(_sigterm_source, ^{
            secnotice(PROXYXPCSCOPE, "exiting due to SIGTERM");
            xpc_transaction_exit_clean();
        });
        dispatch_activate(_sigterm_source);
    }
    return self;
}


- (void) describeXPCObject: (char *) prefix withObject: (xpc_object_t) object {
    if(object) {
        char *desc = xpc_copy_description(object);
        secinfo(PROXYXPCSCOPE, "%s%s", prefix, desc);
        free(desc);
    } else {
        secinfo(PROXYXPCSCOPE, "%s<NULL>", prefix);
    }
}

- (NSObject *) CreateNSObjectForCFXPCObjectFromKey: (xpc_object_t) xdict withKey: (const char * _Nonnull) key {
    xpc_object_t xObj = xpc_dictionary_get_value(xdict, key);
    if (!xObj) {
        return nil;
    }
    return (__bridge_transfer NSObject *)(_CFXPCCreateCFObjectFromXPCObject(xObj));
}

- (NSArray<NSString*> *) CreateArrayOfStringsForCFXPCObjectFromKey: (xpc_object_t) xdict withKey: (const char * _Nonnull) key {
    NSObject * possibleArray = [self CreateNSObjectForCFXPCObjectFromKey: xdict withKey: key];

    if (![possibleArray isNSArray__]) {
        return nil;
    }
    
    __block bool onlyStrings = true;
    [(NSArray*) possibleArray enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
        if (![obj isNSString__]) {
            *stop = true;
            onlyStrings = false;
        }
    }];

    return onlyStrings ? (NSArray<NSString*>*) possibleArray : nil;
}

- (void)  sendAckResponse: (const xpc_connection_t) peer forEvent: (xpc_object_t) event {
    xpc_object_t replyMessage = xpc_dictionary_create_reply(event);
    if (replyMessage) {  // Caller wanted an ACK, so give one
        xpc_dictionary_set_string(replyMessage, kMessageKeyValue, "ACK");
        xpc_connection_send_message(peer, replyMessage);
    }
}

- (void) cloudkeychainproxy_peer_dictionary_handler: (const xpc_connection_t) peer forEvent: (xpc_object_t) event {
    bool result = false;
    int err = 0;

    @autoreleasepool {

        require_action_string(xpc_get_type(event) == XPC_TYPE_DICTIONARY, xit, err = -51, "expected XPC_TYPE_DICTIONARY");

        const char *operation = xpc_dictionary_get_string(event, kMessageKeyOperation);
        require_action(operation, xit, result = false);

        // Check protocol version
        uint64_t version = xpc_dictionary_get_uint64(event, kMessageKeyVersion);
        secinfo(PROXYXPCSCOPE, "Reply version: %lld", version);
        require_action(version == kCKDXPCVersion, xit, result = false);

        // Operations
        secinfo(PROXYXPCSCOPE, "Handling %s operation", operation);


        if (!strcmp(operation, kOperationPUTDictionary))
        {
            [self operation_put_dictionary: event];
            [self sendAckResponse: peer forEvent: event];
        }
        else if (!strcmp(operation, kOperationGETv2))
        {
            [self operation_get_v2: peer forEvent: event];
            // operationg_get_v2 sends the response
        }
        else if (!strcmp(operation, kOperationClearStore))
        {
            [_proxyID clearStore];
            [self sendAckResponse: peer forEvent: event];
        }
        else if (!strcmp(operation, kOperationSynchronize))
        {
            [_proxyID synchronizeStore];
            [self sendAckResponse: peer forEvent: event];
        }
        else if (!strcmp(operation, kOperationSynchronizeAndWait))
        {
            xpc_object_t replyMessage = xpc_dictionary_create_reply(event);
            secnotice(XPROXYSCOPE, "%s XPC request: %s", kWAIT2MINID, kOperationSynchronizeAndWait);
            
            [_proxyID waitForSynchronization:^(__unused NSDictionary *values, NSError *error)
             {
                 secnotice(PROXYXPCSCOPE, "%s Result from [Proxy waitForSynchronization:]: %@", kWAIT2MINID, error);

                 if (replyMessage)   // Caller wanted an ACK, so give one
                 {
                     if (error)
                     {
                         xpc_object_t xerrobj = SecCreateXPCObjectWithCFError((__bridge CFErrorRef)(error));
                         xpc_dictionary_set_value(replyMessage, kMessageKeyError, xerrobj);
                     } else {
                         xpc_dictionary_set_string(replyMessage, kMessageKeyValue, "ACK");
                     }
                     xpc_connection_send_message(peer, replyMessage);
                 }
             }];
        }
        else if (!strcmp(operation, kOperationRegisterKeys))
        {
            xpc_object_t xkeysToRegisterDict = xpc_dictionary_get_value(event, kMessageKeyValue);

            xpc_object_t xKTRallkeys = xpc_dictionary_get_value(xkeysToRegisterDict, kMessageAllKeys);

            NSString* accountUUID = (NSString*) [self CreateNSObjectForCFXPCObjectFromKey:event withKey: kMessageKeyAccountUUID];

            if (![accountUUID isKindOfClass:[NSString class]]) {
                accountUUID = nil;
            }

            NSDictionary *KTRallkeys = (__bridge_transfer NSDictionary *)(_CFXPCCreateCFObjectFromXPCObject(xKTRallkeys));

            [_proxyID registerKeys: KTRallkeys forAccount: accountUUID];
            [self sendAckResponse: peer forEvent: event];

            secinfo(PROXYXPCSCOPE, "RegisterKeys message sent");
        }
        else if (!strcmp(operation, kOperationRemoveKeys))
        {
            xpc_object_t xkeysToRemoveDict = xpc_dictionary_get_value(event, kMessageKeyValue);
            
            NSString* accountUUID = (NSString*) [self CreateNSObjectForCFXPCObjectFromKey: event withKey: kMessageKeyAccountUUID];
            
            if (![accountUUID isKindOfClass:[NSString class]]) {
                accountUUID = nil;
            }
            
            NSArray *KTRallkeys = (__bridge_transfer NSArray *)(_CFXPCCreateCFObjectFromXPCObject(xkeysToRemoveDict));
            
            [_proxyID removeKeys:KTRallkeys forAccount:accountUUID];
            [self sendAckResponse: peer forEvent: event];

            secinfo(PROXYXPCSCOPE, "RemoveKeys message sent");
        }
        else if (!strcmp(operation, kOperationRequestSyncWithPeers))
        {

            NSArray<NSString*> * peerIDs = [self CreateArrayOfStringsForCFXPCObjectFromKey: event withKey: kMessageKeyPeerIDList];
            NSArray<NSString*> * backupPeerIDs = [self CreateArrayOfStringsForCFXPCObjectFromKey: event withKey: kMesssgeKeyBackupPeerIDList];

            require_action(peerIDs && backupPeerIDs, xit, (secnotice(XPROXYSCOPE, "Bad call to sync with peers"), result = false));

            [_proxyID requestSyncWithPeerIDs: peerIDs backupPeerIDs: backupPeerIDs];
            [self sendAckResponse: peer forEvent: event];

            secinfo(PROXYXPCSCOPE, "RequestSyncWithAllPeers reply sent");
        }
        else if (!strcmp(operation, kOperationHasPendingSyncWithPeer)) {
            NSString *peerID = (NSString*) [self CreateNSObjectForCFXPCObjectFromKey: event withKey: kMessageKeyPeerID];

            BOOL hasPending = [_proxyID hasSyncPendingFor: peerID];

            xpc_object_t replyMessage = xpc_dictionary_create_reply(event);
            if (replyMessage)
            {
                xpc_dictionary_set_bool(replyMessage, kMessageKeyValue, hasPending);
                xpc_connection_send_message(peer, replyMessage);
                secinfo(PROXYXPCSCOPE, "HasPendingSyncWithPeer reply sent");
            }
        }
        else if (!strcmp(operation, kOperationHasPendingKey)) {
            NSString *peerID = (NSString*) [self CreateNSObjectForCFXPCObjectFromKey: event withKey: kMessageKeyPeerID];

            BOOL hasPending = [_proxyID hasPendingKey: peerID];

            xpc_object_t replyMessage = xpc_dictionary_create_reply(event);
            if (replyMessage)
            {
                xpc_dictionary_set_bool(replyMessage, kMessageKeyValue, hasPending);
                xpc_connection_send_message(peer, replyMessage);
                secinfo(PROXYXPCSCOPE, "HasIncomingMessageFromPeer reply sent");
            }
        }
        else if (!strcmp(operation, kOperationRequestEnsurePeerRegistration))
        {
            [_proxyID requestEnsurePeerRegistration];
            [self sendAckResponse: peer forEvent: event];
            secinfo(PROXYXPCSCOPE, "RequestEnsurePeerRegistration reply sent");
        }
        else if (!strcmp(operation, kOperationFlush))
        {
            [_proxyID doAfterFlush:^{
                [self sendAckResponse: peer forEvent: event];
                secinfo(PROXYXPCSCOPE, "flush reply sent");
            }];
        }
        else if (!strcmp(operation, kOperationPerfCounters)) {
            [_proxyID perfCounters:^(NSDictionary *counters){
                xpc_object_t replyMessage = xpc_dictionary_create_reply(event);
                xpc_object_t object = _CFXPCCreateXPCObjectFromCFObject((__bridge CFTypeRef)counters);
                xpc_dictionary_set_value(replyMessage, kMessageKeyValue, object);
                xpc_connection_send_message(peer, replyMessage);
            }];
        }
        else
        {
            char *description = xpc_copy_description(event);
            secinfo(PROXYXPCSCOPE, "Unknown op=%s request from pid %d: %s", operation, xpc_connection_get_pid(peer), description);
            free(description);
        }
        result = true;
xit:
        if (!result) {
            [self describeXPCObject: "handle_operation fail: " withObject: event];
        }
    }
}

- (bool) operation_put_dictionary: (xpc_object_t) event {
    // PUT a set of objects into the KVS store. Return false if error
    xpc_object_t xvalue = xpc_dictionary_get_value(event, kMessageKeyValue);
    if (!xvalue) {
        return false;
    }

    NSObject* object = (__bridge_transfer NSObject*) _CFXPCCreateCFObjectFromXPCObject(xvalue);
    if (![object isKindOfClass:[NSDictionary<NSString*, NSObject*> class]]) {
        [self describeXPCObject: "operation_put_dictionary unable to convert to CF: " withObject: xvalue];
        return false;
    }

    [_proxyID setObjectsFromDictionary: (NSDictionary<NSString*, NSObject*> *)object];

    return true;
}

- (bool) operation_get_v2: (xpc_connection_t) peer forEvent: (xpc_object_t) event {
    // GET a set of objects from the KVS store. Return false if error
    xpc_object_t replyMessage = xpc_dictionary_create_reply(event);
    if (!replyMessage)
    {
        secinfo(PROXYXPCSCOPE, "can't create replyMessage");
        assert(false);   //must have a reply handler
        return false;
    }
    xpc_object_t returnedValues = xpc_dictionary_create(NULL, NULL, 0);
    if (!returnedValues)
    {
        secinfo(PROXYXPCSCOPE, "can't create returnedValues");
        assert(false);   // must have a spot for the returned values
        return false;
    }

    xpc_object_t xvalue = xpc_dictionary_get_value(event, kMessageKeyValue);
    if (!xvalue)
    {
        secinfo(PROXYXPCSCOPE, "missing \"value\" key");
        return false;
    }
    
    xpc_object_t xkeystoget = xpc_dictionary_get_value(xvalue, kMessageKeyKeysToGet);
    if (xkeystoget)
    {
        secinfo(PROXYXPCSCOPE, "got xkeystoget");
        CFTypeRef keystoget = _CFXPCCreateCFObjectFromXPCObject(xkeystoget);
        if (!keystoget || (CFGetTypeID(keystoget)!=CFArrayGetTypeID()))     // not "getAll", this is an error of some kind
        {
            secinfo(PROXYXPCSCOPE, "can't convert keystoget or is not an array");
            CFReleaseSafe(keystoget);
            return false;
        }
        
        [(__bridge NSArray *)keystoget enumerateObjectsUsingBlock: ^ (id obj, NSUInteger idx, BOOL *stop)
        {
            NSString *key = (NSString *)obj;
            id object = [_proxyID objectForKey:key];
            secinfo(PROXYXPCSCOPE, "get: key: %@, object: %@", key, object);
            xpc_object_t xobject = object ? _CFXPCCreateXPCObjectFromCFObject((__bridge CFTypeRef)object) : xpc_null_create();
            xpc_dictionary_set_value(returnedValues, [key UTF8String], xobject);
        }];
    }
    else    // get all values from kvs
    {
        secinfo(PROXYXPCSCOPE, "get all values from kvs");
        NSDictionary *all = [_proxyID copyAsDictionary];
        [all enumerateKeysAndObjectsUsingBlock: ^ (id key, id obj, BOOL *stop)
        {
            xpc_object_t xobject = obj ? _CFXPCCreateXPCObjectFromCFObject((__bridge CFTypeRef)obj) : xpc_null_create();
            xpc_dictionary_set_value(returnedValues, [(NSString *)key UTF8String], xobject);
        }];
    }

    xpc_dictionary_set_uint64(replyMessage, kMessageKeyVersion, kCKDXPCVersion);
    xpc_dictionary_set_value(replyMessage, kMessageKeyValue, returnedValues);
    xpc_connection_send_message(peer, replyMessage);

    return true;
}


@end

static void diagnostics(int argc, const char *argv[]) {
    @autoreleasepool {
        NSDictionary *all = [[CloudKeychainProxy sharedObject].proxyID copyAsDictionary];
        NSLog(@"All: %@",all);
    }
}



int main(int argc, const char *argv[]) {
    secinfo(PROXYXPCSCOPE, "Starting CloudKeychainProxy");
    char *wait4debugger = getenv("WAIT4DEBUGGER");

    if (wait4debugger && !strcasecmp("YES", wait4debugger)) {
        syslog(LOG_ERR, "Waiting for debugger");
        kill(getpid(), SIGTSTP);
    }

    if (argc > 1) {
        diagnostics(argc, argv);
        return 0;
    }
    
    CloudKeychainProxy *ckp = nil;
    @autoreleasepool {
        ckp = [CloudKeychainProxy sharedObject];
    }
    
    if (ckp) {  // nothing bad happened when initializing
        secinfo(PROXYXPCSCOPE, "Starting mainRunLoop");
        NSRunLoop *runLoop = [NSRunLoop mainRunLoop];
        [runLoop run];
    }
    secinfo(PROXYXPCSCOPE, "Exiting CloudKeychainProxy");

    return EXIT_FAILURE;
}
