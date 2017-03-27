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

#include <AssertMacros.h>

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
#include <TargetConditionals.h>
#include "SOSCloudKeychainConstants.h"
#import <Security/SecureObjectSync/SOSInternal.h>

#import "KeychainSyncingOverIDSProxy+Throttle.h"
#import "KeychainSyncingOverIDSProxy+SendMessage.h"

int idsproxymain(int argc, const char *argv[]);

#define PROXYXPCSCOPE "idsproxy"

static void describeXPCObject(char *prefix, xpc_object_t object)
{
    // This is useful for debugging.
    if (object)
    {
        char *desc = xpc_copy_description(object);
        secdebug(PROXYXPCSCOPE, "%s%s\n", prefix, desc);
        free(desc);
    }
    else
        secdebug(PROXYXPCSCOPE, "%s<NULL>\n", prefix);

}

static void idskeychainsyncingproxy_peer_dictionary_handler(const xpc_connection_t peer, xpc_object_t event)
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
    
    
    if(operation && !strcmp(operation, kOperationGetDeviceID)){
        [[KeychainSyncingOverIDSProxy idsProxy] doSetIDSDeviceID];
        xpc_object_t replyMessage = xpc_dictionary_create_reply(event);
        xpc_dictionary_set_bool(replyMessage, kMessageKeyValue, true);
        xpc_connection_send_message(peer, replyMessage);
        secdebug(PROXYXPCSCOPE, "Set our IDS Device ID message sent");

    }
    else if (operation && !strcmp(operation, kOperationGetPendingMesages))
    {
        NSDictionary* messages = [[KeychainSyncingOverIDSProxy idsProxy] retrievePendingMessages];
        xpc_object_t xMessages = _CFXPCCreateXPCObjectFromCFObject((__bridge CFDictionaryRef)(messages));

        xpc_object_t replyMessage = xpc_dictionary_create_reply(event);
        xpc_dictionary_set_value(replyMessage, kMessageKeyValue, xMessages);

        xpc_connection_send_message(peer, replyMessage);
        secdebug(PROXYXPCSCOPE, "retrieved pending messages");

    }
    else if(operation && !strcmp(operation, kOperationSendDeviceList)) //IDS device availability check
    {
        xpc_object_t xidsDeviceList = xpc_dictionary_get_value(event, kMessageKeyValue);
        xpc_object_t xPeerID = xpc_dictionary_get_value(event, kMessageKeyPeerID);
        
        NSArray *idsList = (__bridge_transfer NSArray*)(_CFXPCCreateCFObjectFromXPCObject(xidsDeviceList));
        NSString *peerID = (__bridge_transfer NSString*)(_CFXPCCreateCFObjectFromXPCObject(xPeerID));
        
        bool isMessageArray = (CFGetTypeID((__bridge CFTypeRef)(idsList)) == CFArrayGetTypeID());
        bool isPeerIDString = (CFGetTypeID((__bridge CFTypeRef)(peerID)) == CFStringGetTypeID());
        
        require_quiet(isMessageArray, xit);
        require_quiet(isPeerIDString, xit);
        
        [[KeychainSyncingOverIDSProxy idsProxy] pingDevices:idsList peerID:peerID];
        
        xpc_object_t replyMessage = xpc_dictionary_create_reply(event);
        xpc_dictionary_set_bool(replyMessage, kMessageKeyValue, true);
        
        xpc_connection_send_message(peer, replyMessage);
        secdebug(PROXYXPCSCOPE, "IDS device list sent");
    }
    else if (operation && !strcmp(operation, kOperationSendFragmentedIDSMessage))
    {
        xpc_object_t xidsMessageData = xpc_dictionary_get_value(event, kMessageKeyValue);
        xpc_object_t xDeviceName = xpc_dictionary_get_value(event, kMessageKeyDeviceName);
        xpc_object_t xPeerID = xpc_dictionary_get_value(event, kMessageKeyPeerID);
        BOOL object = false;
        
        NSString *deviceName = (__bridge_transfer NSString*)(_CFXPCCreateCFObjectFromXPCObject(xDeviceName));
        NSString *peerID = (__bridge_transfer NSString*)(_CFXPCCreateCFObjectFromXPCObject(xPeerID));
        NSDictionary *messageDictionary = (__bridge_transfer NSDictionary*)(_CFXPCCreateCFObjectFromXPCObject(xidsMessageData));
        NSError *error = NULL;
        bool isNameString = (CFGetTypeID((__bridge CFTypeRef)(deviceName)) == CFStringGetTypeID());
        bool isPeerIDString = (CFGetTypeID((__bridge CFTypeRef)(peerID)) == CFStringGetTypeID());
        bool isMessageDictionary = (CFGetTypeID((__bridge CFTypeRef)(messageDictionary)) == CFDictionaryGetTypeID());

        require_quiet(isNameString, xit);
        require_quiet(isPeerIDString, xit);
        require_quiet(isMessageDictionary, xit);
        
        [[KeychainSyncingOverIDSProxy idsProxy] recordTimestampOfWriteToIDS: messageDictionary deviceName:deviceName peerID:peerID];
        NSDictionary *safeValues = [[KeychainSyncingOverIDSProxy idsProxy] filterForWritableValues:messageDictionary];
        
        if(safeValues != nil && [safeValues count] > 0){
            object = [[KeychainSyncingOverIDSProxy idsProxy] sendFragmentedIDSMessages:safeValues name:deviceName peer:peerID error:&error];
        }
        
        xpc_object_t replyMessage = xpc_dictionary_create_reply(event);
        xpc_dictionary_set_bool(replyMessage, kMessageKeyValue, object);
        
        if(error){
            xpc_object_t xerrobj = SecCreateXPCObjectWithCFError((__bridge CFErrorRef)(error));
            xpc_dictionary_set_value(replyMessage, kMessageKeyError, xerrobj);
        }
        xpc_connection_send_message(peer, replyMessage);
        secdebug(PROXYXPCSCOPE, "IDS message sent");
    }
    else if (operation && !strcmp(operation, kOperationSendIDSMessage)) //for IDS tests
    {
        xpc_object_t xidsMessageData = xpc_dictionary_get_value(event, kMessageKeyValue);
        xpc_object_t xDeviceName = xpc_dictionary_get_value(event, kMessageKeyDeviceName);
        xpc_object_t xPeerID = xpc_dictionary_get_value(event, kMessageKeyPeerID);
        BOOL object = false;
        
        NSString *deviceName = (__bridge_transfer NSString*)(_CFXPCCreateCFObjectFromXPCObject(xDeviceName));
        NSString *peerID = (__bridge_transfer NSString*)(_CFXPCCreateCFObjectFromXPCObject(xPeerID));
        NSDictionary *messageDictionary = (__bridge_transfer NSDictionary*)(_CFXPCCreateCFObjectFromXPCObject(xidsMessageData));
        NSError *error = NULL;
        bool isNameString = (CFGetTypeID((__bridge CFTypeRef)(deviceName)) == CFStringGetTypeID());
        bool isPeerIDString = (CFGetTypeID((__bridge CFTypeRef)(peerID)) == CFStringGetTypeID());
        bool isMessageDictionary = (CFGetTypeID((__bridge CFTypeRef)(messageDictionary)) == CFDictionaryGetTypeID());
        
        require_quiet(isNameString, xit);
        require_quiet(isPeerIDString, xit);
        require_quiet(isMessageDictionary, xit);
        
        [[KeychainSyncingOverIDSProxy idsProxy] recordTimestampOfWriteToIDS: messageDictionary deviceName:deviceName peerID:peerID];
        NSDictionary *safeValues = [[KeychainSyncingOverIDSProxy idsProxy] filterForWritableValues:messageDictionary];

        if(safeValues != nil && [safeValues count] > 0){
            NSString *localMessageIdentifier = [[NSUUID UUID] UUIDString];
            NSMutableDictionary* safeValuesCopy = [NSMutableDictionary dictionaryWithDictionary:safeValues];

            [safeValuesCopy setObject:localMessageIdentifier forKey:(__bridge NSString*)(kIDSMessageUniqueID)];

            if([[KeychainSyncingOverIDSProxy idsProxy] sendIDSMessage:safeValuesCopy name:deviceName peer:peerID])
            {
                object = true;
                NSString *useAckModel = [safeValuesCopy objectForKey:(__bridge NSString*)(kIDSMessageUsesAckModel)];
                if(object && [useAckModel compare:@"YES"] == NSOrderedSame){
                    secnotice("IDS Transport", "setting timer!");
                    [[KeychainSyncingOverIDSProxy idsProxy] setMessageTimer:localMessageIdentifier deviceID:deviceName message:safeValuesCopy];
                }
            }
            else{
                secerror("Could not send message");
            }

        }

        xpc_object_t replyMessage = xpc_dictionary_create_reply(event);
        xpc_dictionary_set_bool(replyMessage, kMessageKeyValue, object);
        
        if(error){
            xpc_object_t xerrobj = SecCreateXPCObjectWithCFError((__bridge CFErrorRef)(error));
            xpc_dictionary_set_value(replyMessage, kMessageKeyError, xerrobj);
        }
        xpc_connection_send_message(peer, replyMessage);
        secdebug(PROXYXPCSCOPE, "IDS message sent");
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

static void idskeychainsyncingproxy_peer_event_handler(xpc_connection_t peer, xpc_object_t event)
{
    describeXPCObject("peer: ", peer);
	xpc_type_t type = xpc_get_type(event);
	if (type == XPC_TYPE_ERROR) {
		if (event == XPC_ERROR_CONNECTION_INVALID) {
			// The client process on the other end of the connection has either
			// crashed or cancelled the connection. After receiving this error,
			// the connection is in an invalid state, and you do not need to
			// call xpc_connection_cancel(). Just tear down any associated state
			// here.
		} else if (event == XPC_ERROR_TERMINATION_IMMINENT) {
			// Handle per-connection termination cleanup.
		}
	} else {
		assert(type == XPC_TYPE_DICTIONARY);
		// Handle the message.
    //    describeXPCObject("dictionary:", event);
        dispatch_async(dispatch_get_main_queue(), ^{
            idskeychainsyncingproxy_peer_dictionary_handler(peer, event);
        });
	}
}

static void idskeychainsyncingproxy_event_handler(xpc_connection_t peer)
{
	// By defaults, new connections will target the default dispatch
	// concurrent queue.

    if (xpc_get_type(peer) != XPC_TYPE_CONNECTION)
    {
        secdebug(PROXYXPCSCOPE, "expected XPC_TYPE_CONNECTION");
        return;
    }

    xpc_connection_set_event_handler(peer, ^(xpc_object_t event)
    {
        idskeychainsyncingproxy_peer_event_handler(peer, event);
	});
	
	// This will tell the connection to begin listening for events. If you
	// have some other initialization that must be done asynchronously, then
	// you can defer this call until after that initialization is done.
	xpc_connection_resume(peer);
}

int idsproxymain(int argc, const char *argv[])
{
    secdebug(PROXYXPCSCOPE, "Starting IDSProxy");
    char *wait4debugger = getenv("WAIT4DEBUGGER");

    if (wait4debugger && !strcasecmp("YES", wait4debugger))
    {
        syslog(LOG_ERR, "Waiting for debugger");
        kill(getpid(), SIGTSTP);
    }

    // DISPATCH_TARGET_QUEUE_DEFAULT
	xpc_connection_t listener = xpc_connection_create_mach_service(xpcIDSServiceName, NULL, XPC_CONNECTION_MACH_SERVICE_LISTENER);
	xpc_connection_set_event_handler(listener, ^(xpc_object_t object){ idskeychainsyncingproxy_event_handler(object); });

    [KeychainSyncingOverIDSProxy idsProxy];

    if([[KeychainSyncingOverIDSProxy idsProxy].messagesInFlight count] > 0 &&
       [KeychainSyncingOverIDSProxy idsProxy].isIDSInitDone &&
       [KeychainSyncingOverIDSProxy idsProxy].sendRestoredMessages){
        [[KeychainSyncingOverIDSProxy idsProxy] sendPersistedMessagesAgain];
        [KeychainSyncingOverIDSProxy idsProxy].sendRestoredMessages = false;
    }

    // It looks to me like there is insufficient locking to allow a request to come in on the XPC connection while doing the initial all items.
    // Therefore I'm leaving the XPC connection suspended until that has time to process.
	xpc_connection_resume(listener);

    @autoreleasepool
    {
        secdebug(PROXYXPCSCOPE, "Starting mainRunLoop");
        NSRunLoop *runLoop = [NSRunLoop mainRunLoop];
        [runLoop run];
    }

    secdebug(PROXYXPCSCOPE, "Exiting KeychainSyncingOverIDSProxy");

    return EXIT_FAILURE;
}

int main(int argc, const char *argv[])
{
    return idsproxymain(argc, argv);
}
