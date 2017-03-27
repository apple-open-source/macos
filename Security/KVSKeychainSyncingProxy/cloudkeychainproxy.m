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


void finalize_connection(void *not_used);
void handle_connection_event(const xpc_connection_t peer);
static void cloudkeychainproxy_peer_dictionary_handler(const xpc_connection_t peer, xpc_object_t event);

static bool operation_put_dictionary(xpc_object_t event);
static bool operation_get_v2(xpc_connection_t peer, xpc_object_t event);

int ckdproxymain(int argc, const char *argv[]);

#define PROXYXPCSCOPE "xpcproxy"

static void describeXPCObject(char *prefix, xpc_object_t object)
{
//#ifndef NDEBUG
    // This is useful for debugging.
    if (object)
    {
        char *desc = xpc_copy_description(object);
        secdebug(PROXYXPCSCOPE, "%s%s\n", prefix, desc);
        free(desc);
    }
    else
        secdebug(PROXYXPCSCOPE, "%s<NULL>\n", prefix);

//#endif
}

static NSObject *CreateNSObjectForCFXPCObjectFromKey(xpc_object_t xdict, const char * _Nonnull key)
{
    xpc_object_t xObj = xpc_dictionary_get_value(xdict, key);

    if (!xObj) {
        return nil;
    }

    return (__bridge_transfer NSObject *)(_CFXPCCreateCFObjectFromXPCObject(xObj));
}

static NSArray<NSString*> *CreateArrayOfStringsForCFXPCObjectFromKey(xpc_object_t xdict, const char * _Nonnull key) {
    NSObject * possibleArray = CreateNSObjectForCFXPCObjectFromKey(xdict, key);

    if (![possibleArray isNSArray__])
        return nil;

    __block bool onlyStrings = true;
    [(NSArray*) possibleArray enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
        if (![obj isNSString__]) {
            *stop = true;
            onlyStrings = false;
        }
    }];

    return onlyStrings ? (NSArray<NSString*>*) possibleArray : nil;
}

static CFStringRef kRegistrationFileName = CFSTR("com.apple.security.cloudkeychainproxy3.keysToRegister.plist");

static UbiqitousKVSProxy *SharedProxy(void) {
    static UbiqitousKVSProxy *sProxy = NULL;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sProxy = [UbiqitousKVSProxy withAccount: [CKDSecuritydAccount securitydAccount]
                                          store: [CKDKVSStore kvsInterface]
                                    lockMonitor: [CKDAKSLockMonitor monitor]
                                    persistence: (NSURL *)CFBridgingRelease(SecCopyURLForFileInPreferencesDirectory(kRegistrationFileName))];
    });

    return sProxy;
}

static void sendAckResponse(const xpc_connection_t peer, xpc_object_t event) {
    xpc_object_t replyMessage = xpc_dictionary_create_reply(event);
    if (replyMessage)   // Caller wanted an ACK, so give one
    {
        xpc_dictionary_set_string(replyMessage, kMessageKeyValue, "ACK");
        xpc_connection_send_message(peer, replyMessage);
    }
}

static void cloudkeychainproxy_peer_dictionary_handler(const xpc_connection_t peer, xpc_object_t event)
{
    bool result = false;
    int err = 0;

    require_action_string(xpc_get_type(event) == XPC_TYPE_DICTIONARY, xit, err = -51, "expected XPC_TYPE_DICTIONARY");

    const char *operation = xpc_dictionary_get_string(event, kMessageKeyOperation);
    require_action(operation, xit, result = false);

    // Check protocol version
    uint64_t version = xpc_dictionary_get_uint64(event, kMessageKeyVersion);
    secdebug(PROXYXPCSCOPE, "Reply version: %lld\n", version);
    require_action(version == kCKDXPCVersion, xit, result = false);

    // Operations
    secdebug(PROXYXPCSCOPE, "Handling %s operation", operation);


    if (operation && !strcmp(operation, kOperationPUTDictionary))
    {
        operation_put_dictionary(event);
        sendAckResponse(peer, event);
    }
    else if (operation && !strcmp(operation, kOperationGETv2))
    {
        operation_get_v2(peer, event);
        // operationg_get_v2 sends the response
    }
    else if (operation && !strcmp(operation, kOperationClearStore))
    {
        [SharedProxy() clearStore];
        sendAckResponse(peer, event);
    }
    else if (operation && !strcmp(operation, kOperationSynchronize))
    {
        [SharedProxy() synchronizeStore];
        sendAckResponse(peer, event);
    }
    else if (operation && !strcmp(operation, kOperationSynchronizeAndWait))
    {
        xpc_object_t replyMessage = xpc_dictionary_create_reply(event);
        secnotice(XPROXYSCOPE, "%s XPC request: %s", kWAIT2MINID, kOperationSynchronizeAndWait);
        
        [SharedProxy() waitForSynchronization:^(__unused NSDictionary *values, NSError *error)
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
    else if (operation && !strcmp(operation, kOperationRegisterKeys))
    {
        xpc_object_t xkeysToRegisterDict = xpc_dictionary_get_value(event, kMessageKeyValue);

        xpc_object_t xKTRallkeys = xpc_dictionary_get_value(xkeysToRegisterDict, kMessageAllKeys);

        NSString* accountUUID = (NSString*) CreateNSObjectForCFXPCObjectFromKey(event, kMessageKeyAccountUUID);

        if (![accountUUID isKindOfClass:[NSString class]]) {
            accountUUID = nil;
        }

        NSDictionary *KTRallkeys = (__bridge_transfer NSDictionary *)(_CFXPCCreateCFObjectFromXPCObject(xKTRallkeys));

        [SharedProxy() registerKeys: KTRallkeys forAccount: accountUUID];
        sendAckResponse(peer, event);

        secdebug(PROXYXPCSCOPE, "RegisterKeys message sent");
    }
    else if (operation && !strcmp(operation, kOperationRequestSyncWithPeers))
    {

        NSArray<NSString*> * peerIDs = CreateArrayOfStringsForCFXPCObjectFromKey(event, kMessageKeyPeerIDList);
        NSArray<NSString*> * backupPeerIDs = CreateArrayOfStringsForCFXPCObjectFromKey(event, kMesssgeKeyBackupPeerIDList);

        require_action(peerIDs && backupPeerIDs, xit, (secnotice(XPROXYSCOPE, "Bad call to sync with peers"), result = false));

        [SharedProxy() requestSyncWithPeerIDs: peerIDs backupPeerIDs: backupPeerIDs];
        sendAckResponse(peer, event);

        secdebug(PROXYXPCSCOPE, "RequestSyncWithAllPeers reply sent");
    }
    else if (operation && !strcmp(operation, kOperationHasPendingSyncWithPeer)) {
        NSString *peerID = (NSString*) CreateNSObjectForCFXPCObjectFromKey(event, kMessageKeyPeerID);

        BOOL hasPending = [SharedProxy() hasSyncPendingFor: peerID];

        xpc_object_t replyMessage = xpc_dictionary_create_reply(event);
        if (replyMessage)
        {
            xpc_dictionary_set_bool(replyMessage, kMessageKeyValue, hasPending);
            xpc_connection_send_message(peer, replyMessage);
            secdebug(PROXYXPCSCOPE, "HasPendingSyncWithPeer reply sent");
        }
    }
    else if (operation && !strcmp(operation, kOperationHasPendingKey)) {
        NSString *peerID = (NSString*) CreateNSObjectForCFXPCObjectFromKey(event, kMessageKeyPeerID);

        BOOL hasPending = [SharedProxy() hasPendingKey: peerID];

        xpc_object_t replyMessage = xpc_dictionary_create_reply(event);
        if (replyMessage)
        {
            xpc_dictionary_set_bool(replyMessage, kMessageKeyValue, hasPending);
            xpc_connection_send_message(peer, replyMessage);
            secdebug(PROXYXPCSCOPE, "HasIncomingMessageFromPeer reply sent");
        }
    }
    else if (operation && !strcmp(operation, kOperationRequestEnsurePeerRegistration))
    {
        [SharedProxy() requestEnsurePeerRegistration];
        sendAckResponse(peer, event);
        secdebug(PROXYXPCSCOPE, "RequestEnsurePeerRegistration reply sent");
    }
    else if (operation && !strcmp(operation, kOperationFlush))
    {
        [SharedProxy() doAfterFlush:^{
            sendAckResponse(peer, event);
            secdebug(PROXYXPCSCOPE, "flush reply sent");
        }];
    }
    else
    {
        char *description = xpc_copy_description(event);
        secdebug(PROXYXPCSCOPE, "Unknown op=%s request from pid %d: %s", operation, xpc_connection_get_pid(peer), description);
        free(description);
    }
    result = true;
xit:
    if (!result)
        describeXPCObject("handle_operation fail: ", event);
}

void finalize_connection(void *not_used)
{
    secdebug(PROXYXPCSCOPE, "finalize_connection");
    [SharedProxy() synchronizeStore];
	xpc_transaction_end();
}

static bool operation_put_dictionary(xpc_object_t event)
{
    // PUT a set of objects into the KVS store. Return false if error
    describeXPCObject("operation_put_dictionary event: ", event);
    xpc_object_t xvalue = xpc_dictionary_get_value(event, kMessageKeyValue);
    if (!xvalue) {
        return false;
    }

    NSObject* object = (__bridge_transfer NSObject*) _CFXPCCreateCFObjectFromXPCObject(xvalue);
    if (![object isKindOfClass:[NSDictionary<NSString*, NSObject*> class]]) {
        describeXPCObject("operation_put_dictionary unable to convert to CF: ", xvalue);
        return false;
    }

    [SharedProxy() setObjectsFromDictionary: (NSDictionary<NSString*, NSObject*> *)object];

    return true;
}

static bool operation_get_v2(xpc_connection_t peer, xpc_object_t event)
{
    // GET a set of objects from the KVS store. Return false if error    
    describeXPCObject("operation_get_v2 event: ", event);

    xpc_object_t replyMessage = xpc_dictionary_create_reply(event);
    if (!replyMessage)
    {
        secdebug(PROXYXPCSCOPE, "can't create replyMessage");
        assert(true);   //must have a reply handler
        return false;
    }
    xpc_object_t returnedValues = xpc_dictionary_create(NULL, NULL, 0);
    if (!returnedValues)
    {
        secdebug(PROXYXPCSCOPE, "can't create returnedValues");
        assert(true);   // must have a spot for the returned values
        return false;
    }

    xpc_object_t xvalue = xpc_dictionary_get_value(event, kMessageKeyValue);
    if (!xvalue)
    {
        secdebug(PROXYXPCSCOPE, "missing \"value\" key");
        return false;
    }
    
    xpc_object_t xkeystoget = xpc_dictionary_get_value(xvalue, kMessageKeyKeysToGet);
    if (xkeystoget)
    {
        secdebug(PROXYXPCSCOPE, "got xkeystoget");
        CFTypeRef keystoget = _CFXPCCreateCFObjectFromXPCObject(xkeystoget);
        if (!keystoget || (CFGetTypeID(keystoget)!=CFArrayGetTypeID()))     // not "getAll", this is an error of some kind
        {
            secdebug(PROXYXPCSCOPE, "can't convert keystoget or is not an array");
            CFReleaseSafe(keystoget);
            return false;
        }
        
        [(__bridge NSArray *)keystoget enumerateObjectsUsingBlock: ^ (id obj, NSUInteger idx, BOOL *stop)
        {
            NSString *key = (NSString *)obj;
            id object = [SharedProxy() objectForKey:key];
            secdebug(PROXYXPCSCOPE, "get: key: %@, object: %@", key, object);
            xpc_object_t xobject = object ? _CFXPCCreateXPCObjectFromCFObject((__bridge CFTypeRef)object) : xpc_null_create();
            xpc_dictionary_set_value(returnedValues, [key UTF8String], xobject);
            describeXPCObject("operation_get_v2: value from kvs: ", xobject);
        }];
    }
    else    // get all values from kvs
    {
        secdebug(PROXYXPCSCOPE, "get all values from kvs");
        NSDictionary *all = [SharedProxy() copyAsDictionary];
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

static void cloudkeychainproxy_event_handler(xpc_connection_t peer)
{
    if (xpc_get_type(peer) != XPC_TYPE_CONNECTION)
    {
        secdebug(PROXYXPCSCOPE, "expected XPC_TYPE_CONNECTION");
        return;
    }

    xpc_connection_set_target_queue(peer, [SharedProxy() ckdkvsproxy_queue]);
    xpc_connection_set_event_handler(peer, ^(xpc_object_t event)
    {
        describeXPCObject("peer: ", peer); // Only describes under debug

        // We could handle other peer events (e.g.) disconnects,
        // but we don't keep per-client state so there is no need.
        if (xpc_get_type(event) == XPC_TYPE_DICTIONARY) {
            cloudkeychainproxy_peer_dictionary_handler(peer, event);
        }
	});
	
	// This will tell the connection to begin listening for events. If you
	// have some other initialization that must be done asynchronously, then
	// you can defer this call until after that initialization is done.
	xpc_connection_resume(peer);
}

static void diagnostics(int argc, const char *argv[])
{
    @autoreleasepool
    {
        NSDictionary *all = [SharedProxy() copyAsDictionary];
        NSLog(@"All: %@",all);
    }
}

int ckdproxymain(int argc, const char *argv[])
{
    secdebug(PROXYXPCSCOPE, "Starting CloudKeychainProxy");
    char *wait4debugger = getenv("WAIT4DEBUGGER");

    if (wait4debugger && !strcasecmp("YES", wait4debugger))
    {
        syslog(LOG_ERR, "Waiting for debugger");
        kill(getpid(), SIGTSTP);
    }

    if (argc > 1) {
        diagnostics(argc, argv);
        return 0;
    }

    UbiqitousKVSProxy* proxyID = SharedProxy();

    if (proxyID) {  // nothing bad happened when initializing
        xpc_connection_t listener = xpc_connection_create_mach_service(xpcServiceName, NULL, XPC_CONNECTION_MACH_SERVICE_LISTENER);
        xpc_connection_set_event_handler(listener, ^(xpc_object_t object){ cloudkeychainproxy_event_handler(object); });

        // It looks to me like there is insufficient locking to allow a request to come in on the XPC connection while doing the initial all items.
        // Therefore I'm leaving the XPC connection suspended until that has time to process.
        xpc_connection_resume(listener);

        @autoreleasepool
        {
            secdebug(PROXYXPCSCOPE, "Starting mainRunLoop");
            NSRunLoop *runLoop = [NSRunLoop mainRunLoop];
            [runLoop run];
        }
    }

    secdebug(PROXYXPCSCOPE, "Exiting CloudKeychainProxy");

    return EXIT_FAILURE;
}

int main(int argc, const char *argv[])
{
    return ckdproxymain(argc, argv);
}
