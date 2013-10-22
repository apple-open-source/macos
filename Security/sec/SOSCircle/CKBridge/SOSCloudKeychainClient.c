/*
 * Copyright (c) 2012 Apple Computer, Inc. All Rights Reserved.
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

/*
    SOSCloudTransport.c -  Implementation of the transport layer from CKBridge to SOSAccount/SOSCircle
    These are the exported functions from CloudKeychainProxy
*/

/*
    This XPC service is essentially just a proxy to iCloud KVS, which exists since
    the main security code cannot link against Foundation.
    
    See sendTSARequestWithXPC in tsaSupport.c for how to call the service
    
    The client of an XPC service does not get connection events, nor does it
    need to deal with transactions.
*/

#include <AssertMacros.h>

#include <xpc/xpc.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFXPCBridge.h>
#include <sysexits.h>
#include <syslog.h>
#include <CoreFoundation/CFUserNotification.h>

#include <utilities/debugging.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecXPCError.h>

#include "SOSCloudKeychainConstants.h"
#include "SOSCloudKeychainClient.h"

static CFStringRef sErrorDomain = CFSTR("com.apple.security.sos.transport.error");

#define SOSCKCSCOPE "sync"

// MARK: ---------- SOSCloudTransport ----------

/* SOSCloudTransport, a statically initialized transport singleton. */
static SOSCloudTransportRef sTransport = NULL;

static SOSCloudTransportRef SOSCloudTransportCreateXPCTransport(void);

void SOSCloudKeychainSetTransport(SOSCloudTransportRef transport) {
    sTransport = transport;
}

/* Return the singleton cloud transport instance. */
static SOSCloudTransportRef SOSCloudTransportDefaultTransport(void)
{
    static dispatch_once_t sTransportOnce;
    dispatch_once(&sTransportOnce, ^{
        if (!sTransport)
            SOSCloudKeychainSetTransport(SOSCloudTransportCreateXPCTransport());
    });
    return sTransport;
}


// MARK: ----- utilities -----

static CFErrorRef makeError(CFIndex which)
{
    CFDictionaryRef userInfo = NULL;
    return CFErrorCreate(kCFAllocatorDefault, sErrorDomain, which, userInfo);
}

// MARK: ----- DEBUG Utilities -----

//------------------------------------------------------------------------------------------------
//          DEBUG only
//------------------------------------------------------------------------------------------------

static void describeXPCObject(char *prefix, xpc_object_t object)
{
//#ifndef NDEBUG
    // This is useful for debugging.
    if (object)
    {
        char *desc = xpc_copy_description(object);
        secdebug(SOSCKCSCOPE, "%s%s\n", prefix, desc);
        free(desc);
    }
    else
        secdebug(SOSCKCSCOPE, "%s<NULL>\n", prefix);
//#endif
}

static void describeXPCType(char *prefix, xpc_type_t xtype)
{
    // Add others as necessary, e.g. XPC_TYPE_DOUBLE
#ifndef NDEBUG
    // This is useful for debugging.
    char msg[256]={0,};
    if (XPC_TYPE_CONNECTION == xtype)
        strcpy(msg, "XPC_TYPE_CONNECTION");
    else if (XPC_TYPE_ERROR == xtype)
        strcpy(msg, "XPC_TYPE_ERROR");
    else if  (XPC_TYPE_DICTIONARY == xtype)
        strcpy(msg, "XPC_TYPE_DICTIONARY");
    else
        strcpy(msg, "<unknown>");

    secdebug(SOSCKCSCOPE, "%s type:%s\n", prefix, msg);
#endif
}

// MARK: ---------- SOSXPCCloudTransport ----------

typedef struct SOSXPCCloudTransport *SOSXPCCloudTransportRef;
struct SOSXPCCloudTransport
{
    struct SOSCloudTransport transport;
    xpc_connection_t serviceConnection;
    dispatch_queue_t xpc_queue;
};

static bool xpc_event_filter(const xpc_connection_t peer, xpc_object_t event, CFErrorRef *error)
{
    // return true if the type is XPC_TYPE_DICTIONARY (and therefore something more to process)
    secdebug(SOSCKCSCOPE, "handle_connection_event\n");
    xpc_type_t xtype = xpc_get_type(event);
    describeXPCType("handle_xpc_event", xtype);
    if (XPC_TYPE_CONNECTION == xtype)
    {
        secdebug(SOSCKCSCOPE, "handle_xpc_event: XPC_TYPE_CONNECTION (unexpected)");
        // The client of an XPC service does not get connection events
        // For now, we log this and keep going
        describeXPCObject("handle_xpc_event: XPC_TYPE_CONNECTION, obj : ", event);
#if 0
        if (error)
            *error = makeError(kSOSOUnexpectedConnectionEvent); // FIX
        assert(true);
#endif
    }
    else
    if (XPC_TYPE_ERROR == xtype)
    {
#ifndef NDEBUG
        const char *estr = xpc_dictionary_get_string(event, XPC_ERROR_KEY_DESCRIPTION);
#endif
        secdebug(SOSCKCSCOPE, "default: xpc error: %s\n", estr);
#if 0   // just log for now
        CFStringRef errStr = CFStringCreateWithCString(kCFAllocatorDefault, estr, kCFStringEncodingUTF8);
        CFMutableDictionaryRef userInfo = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (errStr)
            CFDictionaryAddValue(userInfo, kCFErrorLocalizedDescriptionKey, errStr);
        if (error)
            *error = CFErrorCreate(kCFAllocatorDefault, sErrorDomain, kSOSOXPCErrorEvent, userInfo);
        CFReleaseSafe(errStr);
        CFReleaseSafe(userInfo);
#endif
    }
    else
    if (XPC_TYPE_DICTIONARY == xtype)
    {
        secdebug(SOSCKCSCOPE, "received dictionary event %p\n", event);
        return true;
    }
    else
    {
        secdebug(SOSCKCSCOPE, "default: unexpected connection event %p\n", event);
        describeXPCObject("handle_xpc_event: obj : ", event);
        if (error)
            *error = makeError(kSOSOUnexpectedXPCEvent);
    }
    return false;
}

static bool handle_xpc_event(SOSXPCCloudTransportRef transport, xpc_object_t event)
{
    CFErrorRef localError = NULL;
    // See <rdar://problem/14566253>
    secerror(">>>>> handle_connection_event via event_handler <<<<<, WTF?");
    bool result = false;
    if ((result = xpc_event_filter(transport->serviceConnection, event, &localError)))
    {
        const char *operation = xpc_dictionary_get_string(event, kMessageKeyOperation);
        if (!operation || strcmp(operation, kMessageOperationItemChanged))  // some op we don't care about
        {
            secdebug(SOSCKCSCOPE, "operation: %s", operation);
            return result;
        }

        xpc_object_t xrv = xpc_dictionary_get_value(event, kMessageKeyValue);
        if (!xrv)
        {
            secdebug(SOSCKCSCOPE, "xrv null for kMessageKeyValue");
            return result;
        }
        describeXPCObject("xrv", xrv);

        CFDictionaryRef returnedValues = _CFXPCCreateCFObjectFromXPCObject(xrv);
        secdebug(SOSCKCSCOPE, "returnedValues: %@", returnedValues);

        SOSCloudKeychainHandleUpdate(returnedValues);

        CFReleaseNull(returnedValues);
    }
    CFReleaseSafe(localError);

    return result;
}

static void SOSXPCCloudTransportInit(SOSXPCCloudTransportRef transport)
{
    secdebug(SOSCKCSCOPE, "initXPCConnection\n");

    transport->xpc_queue = dispatch_queue_create(xpcServiceName, DISPATCH_QUEUE_SERIAL);

    transport->serviceConnection = xpc_connection_create_mach_service(xpcServiceName, transport->xpc_queue, 0);

    secdebug(SOSCKCSCOPE, "serviceConnection: %p\n", transport->serviceConnection);

    xpc_connection_set_event_handler(transport->serviceConnection, ^(xpc_object_t event)
    {
        secdebug(SOSCKCSCOPE, "xpc_connection_set_event_handler\n");
        handle_xpc_event(transport, event);
    });

    xpc_connection_resume(transport->serviceConnection);
    xpc_retain(transport->serviceConnection);
}

static void talkWithKVS(SOSXPCCloudTransportRef transport, xpc_object_t message, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    __block CFErrorRef error = NULL;
    __block CFTypeRef object = NULL;
    
    dispatch_block_t callback = ^{
            if (replyBlock)
                replyBlock(object, error);
            if(object)
                CFReleaseNull(object);
            if (error)
            {
                secerror("callback error: %@", error);
                CFReleaseNull(error);
            }
            dispatch_release(processQueue);
        };
    
    require_action(transport->serviceConnection, xit, error = makeError(kSOSConnectionNotOpen));
    require_action(message, xit, error = makeError(kSOSObjectNotFoundError));
    dispatch_retain(processQueue);
        
    xpc_connection_send_message_with_reply(transport->serviceConnection, message, transport->xpc_queue, ^(xpc_object_t reply)
        {
            if (xpc_event_filter(transport->serviceConnection, reply, &error) && reply)
            {
                describeXPCObject("getValuesFromKVS: reply : ", reply);
                if (error)
                    secerror("Error from xpc_event_filter: %@", error);
                xpc_object_t xrv = xpc_dictionary_get_value(reply, kMessageKeyValue);
                if (xrv)
                {
                    describeXPCObject("talkWithKVS: xrv: ", xrv);
                    /*
                        * The given XPC object must be one that was previously returned by
                        * _CFXPCCreateXPCMessageWithCFObject().
                    */
                    object = _CFXPCCreateCFObjectFromXPCObject(xrv);   // CF object is retained; release in callback
                    secnotice("talkwithkvs", "converted CF object: %@", object);
                }
                else
                    secerror("missing value reply");

                xpc_object_t xerror = xpc_dictionary_get_value(reply, kMessageKeyError);
                if (xerror)
                    error = SecCreateCFErrorWithXPCObject(xerror);  // use SecCFCreateErrorWithFormat?
            }
            dispatch_async(processQueue, callback);
        });
    return;
    
xit:
    secerror("talkWithKVS error: %@", error);
    if (replyBlock)
        dispatch_async(processQueue, callback);
    CFReleaseSafe(error);
}

// MARK: ---------- SOSXPCCloudTransport Client Calls ----------

/* Concrete function backend implementations. */
static void SOSCloudTransportSetItemsChangedBlock(SOSCloudTransportRef transport,
                                                  CloudItemsChangedBlock itemsChangedBlock) {
    if (transport->itemsChangedBlock != itemsChangedBlock)
    {
        if (transport->itemsChangedBlock)
            Block_release(transport->itemsChangedBlock);
        transport->itemsChangedBlock = Block_copy(itemsChangedBlock);
    }
}

/* Virtual function backend implementations. */
static void SOSCloudTransportPut(SOSCloudTransportRef transport, CFDictionaryRef values, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    SOSXPCCloudTransportRef xpcTransport = (SOSXPCCloudTransportRef)transport;
    secdebug(SOSCKCSCOPE, "%@", values);
    CFErrorRef error = NULL;
    xpc_object_t message = NULL;
    xpc_object_t xobject = NULL;
    require_action(values, xit, error = makeError(kSOSObjectNotFoundError));

    message = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(message, kMessageKeyVersion, kCKDXPCVersion);
    xpc_dictionary_set_string(message, kMessageKeyOperation, kOperationPUTDictionary);
    
    xobject = _CFXPCCreateXPCObjectFromCFObject(values);
    require_action(xobject, xit, error = makeError(kSOSObjectCantBeConvertedToXPCObject));
    xpc_dictionary_set_value(message, kMessageKeyValue, xobject);
    xpc_release(xobject);
    
    talkWithKVS(xpcTransport, message, processQueue, replyBlock);
    xpc_release(message);
    return;

xit:
    if (replyBlock)
        replyBlock(NULL, error);
    CFReleaseSafe(error);
}

/* Get from KVS */
static void SOSCloudTransportGet(SOSCloudTransportRef transport, CFArrayRef keysToGet, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    SOSXPCCloudTransportRef xpcTransport = (SOSXPCCloudTransportRef)transport;
    secdebug(SOSCKCSCOPE, "%@", keysToGet);
    CFErrorRef error = NULL;
    xpc_object_t xkeysOfInterest = xpc_dictionary_create(NULL, NULL, 0);
    xpc_object_t xkeysToGet = keysToGet ? _CFXPCCreateXPCObjectFromCFObject(keysToGet) : xpc_null_create();

    require_action(xkeysToGet, xit, error = makeError(kSOSObjectNotFoundError));

    if (keysToGet)  // don't add if nulll; will call getall
        xpc_dictionary_set_value(xkeysOfInterest, kMessageKeyKeysToGet, xkeysToGet);

    xpc_object_t message = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(message, kMessageKeyVersion, kCKDXPCVersion);
    xpc_dictionary_set_string(message, kMessageKeyOperation, kOperationGETv2);
    xpc_dictionary_set_value(message, kMessageKeyValue, xkeysOfInterest);
    
    talkWithKVS(xpcTransport, message, processQueue, replyBlock);
    xpc_release(xkeysToGet);
    xpc_release(xkeysOfInterest);
    xpc_release(message);
    return;
    
xit:
    if(xkeysOfInterest)
        xpc_release(xkeysOfInterest);
    if(xkeysToGet)
        xpc_release(xkeysToGet);
    if (replyBlock)
        replyBlock(NULL, error);
    CFReleaseSafe(error);
}

static void SOSCloudTransportRegisterKeys(SOSCloudTransportRef transport, CFArrayRef keysToGet, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock, CloudItemsChangedBlock notificationBlock)
{
    SOSXPCCloudTransportRef xpcTransport = (SOSXPCCloudTransportRef)transport;
    secdebug(SOSCKCSCOPE, "%@", keysToGet);
    xpc_object_t xkeysOfInterest = xpc_dictionary_create(NULL, NULL, 0);
    xpc_object_t xkeysToRegister = keysToGet ? _CFXPCCreateXPCObjectFromCFObject(keysToGet) : xpc_null_create();
    xpc_dictionary_set_value(xkeysOfInterest, kMessageKeyKeysToGet, xkeysToRegister);

    xpc_object_t message = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(message, kMessageKeyVersion, kCKDXPCVersion);
    xpc_dictionary_set_string(message, kMessageKeyOperation, kOperationRegisterKeysAndGet);
    xpc_dictionary_set_value(message, kMessageKeyValue, xkeysOfInterest);

    SOSCloudTransportSetItemsChangedBlock(transport, notificationBlock);
    talkWithKVS(xpcTransport, message, processQueue, replyBlock);
    xpc_release(xkeysOfInterest);
    xpc_release(xkeysToRegister);
    xpc_release(message);
}

//
// Handles NULL by seting xpc_null.
static void SecXPCDictionarySetCFObject(xpc_object_t xdict, const char *key, CFTypeRef object)
{
    xpc_object_t xpc_obj = object ? _CFXPCCreateXPCObjectFromCFObject(object) : xpc_null_create();
    xpc_dictionary_set_value(xdict, key, xpc_obj);
    xpc_release(xpc_obj);
}

static bool SOSCloudTransportUpdateKeys(SOSCloudTransportRef transport,
                                        bool getNewKeysOnly,
                                        CFArrayRef alwaysKeys,
                                        CFArrayRef afterFirstUnlockKeys,
                                        CFArrayRef unlockedKeys,
                                        CFErrorRef *error)
{
    __block bool success = true;
    dispatch_queue_t processQueue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);

    CloudKeychainReplyBlock replyBlock = ^(CFDictionaryRef returnedValues, CFErrorRef returnedError)
    {
        if (returnedError) {
            success = false;
            if (error) {
                *error = returnedError;
                CFRetain(*error);
            }
        }
        CFReleaseSafe(returnedError);
    };

    SOSXPCCloudTransportRef xpcTransport = (SOSXPCCloudTransportRef)transport;

    xpc_object_t xkeysOfInterest = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_bool(xkeysOfInterest, kMessageKeyGetNewKeysOnly, getNewKeysOnly);
    SecXPCDictionarySetCFObject(xkeysOfInterest, kMessageKeyKeysToGet, alwaysKeys);
    SecXPCDictionarySetCFObject(xkeysOfInterest, kMessageKeyKeysRequireFirstUnlock, afterFirstUnlockKeys);
    SecXPCDictionarySetCFObject(xkeysOfInterest, kMessageKeyKeysRequiresUnlocked, unlockedKeys);

    xpc_object_t message = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(message, kMessageKeyVersion, kCKDXPCVersion);
    xpc_dictionary_set_string(message, kMessageKeyOperation, kOperationRegisterKeysAndGet);
    xpc_dictionary_set_value(message, kMessageKeyValue, xkeysOfInterest);
    
    talkWithKVS(xpcTransport, message, processQueue, replyBlock);
    xpc_release(message);
    xpc_release(xkeysOfInterest);
    
    return success;
}

static void SOSCloudTransportUnregisterKeys(SOSCloudTransportRef transport, CFArrayRef keysToUnregister, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    SOSXPCCloudTransportRef xpcTransport = (SOSXPCCloudTransportRef)transport;
    secdebug(SOSCKCSCOPE, "%@", keysToUnregister);
    xpc_object_t xkeysOfInterest = xpc_dictionary_create(NULL, NULL, 0);
    xpc_object_t xkeysToUnregister = keysToUnregister ? _CFXPCCreateXPCObjectFromCFObject(keysToUnregister) : xpc_null_create();
    xpc_dictionary_set_value(xkeysOfInterest, kMessageKeyKeysToGet, xkeysToUnregister);

    xpc_object_t message = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(message, kMessageKeyVersion, kCKDXPCVersion);
    xpc_dictionary_set_string(message, kMessageKeyOperation, kOperationUnregisterKeys);
    xpc_dictionary_set_value(message, kMessageKeyValue, xkeysOfInterest);

    talkWithKVS(xpcTransport, message, processQueue, replyBlock);
    xpc_release(xkeysOfInterest);
    xpc_release(xkeysToUnregister);
    xpc_release(message);
}

static void SOSCloudTransportGetAll(SOSCloudTransportRef transport, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    secdebug(SOSCKCSCOPE, "start");
    SOSCloudTransportGet(transport, NULL, processQueue, replyBlock);
}

static void SOSCloudTransportSync(SOSCloudTransportRef transport, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    SOSXPCCloudTransportRef xpcTransport = (SOSXPCCloudTransportRef)transport;
    secdebug(SOSCKCSCOPE, "start");
    xpc_object_t message = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(message, kMessageKeyVersion, kCKDXPCVersion);
    xpc_dictionary_set_string(message, kMessageKeyOperation, kOperationSynchronize);
    talkWithKVS(xpcTransport, message, processQueue, replyBlock);
    xpc_release(message);
}

static void SOSCloudTransportSyncAndWait(SOSCloudTransportRef transport, CFArrayRef keysToGet, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    SOSXPCCloudTransportRef xpcTransport = (SOSXPCCloudTransportRef)transport;
    secdebug(SOSCKCSCOPE, "%@", keysToGet);
    xpc_object_t xkeysOfInterest = xpc_dictionary_create(NULL, NULL, 0);

    xpc_object_t xkeysToRegister = keysToGet ? _CFXPCCreateXPCObjectFromCFObject(keysToGet) : xpc_null_create();
    xpc_dictionary_set_value(xkeysOfInterest, kMessageKeyKeysToGet, xkeysToRegister);
    xpc_release(xkeysToRegister);
    xkeysToRegister = NULL;

    xpc_object_t message = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(message, kMessageKeyVersion, kCKDXPCVersion);
    xpc_dictionary_set_string(message, kMessageKeyOperation, kOperationSynchronizeAndWait);
    xpc_dictionary_set_value(message, kMessageKeyValue, xkeysOfInterest);
    xpc_release(xkeysOfInterest);
    xkeysOfInterest = NULL;

    talkWithKVS(xpcTransport, message, processQueue, replyBlock);
    xpc_release(message);
}

static void SOSCloudTransportClearAll(SOSCloudTransportRef transport, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    SOSXPCCloudTransportRef xpcTransport = (SOSXPCCloudTransportRef)transport;
    secdebug(SOSCKCSCOPE, "start");
    xpc_object_t message = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(message, kMessageKeyVersion, kCKDXPCVersion);
    xpc_dictionary_set_string(message, kMessageKeyOperation, kOperationClearStore);
    talkWithKVS(xpcTransport, message, processQueue, replyBlock);
    xpc_release(message);
}

static void SOSCloudTransportRemoveObjectForKey(SOSCloudTransportRef transport, CFStringRef keyToRemove, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    SOSXPCCloudTransportRef xpcTransport = (SOSXPCCloudTransportRef)transport;
    secdebug(SOSCKCSCOPE, "start");
    CFErrorRef error = NULL;
    xpc_object_t message = NULL;
    xpc_object_t xkeytoremove = NULL;
    
    require_action(keyToRemove, xit, error = makeError(kSOSObjectNotFoundError));

    message = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(message, kMessageKeyVersion, kCKDXPCVersion);
    xpc_dictionary_set_string(message, kMessageKeyOperation, kOperationRemoveObjectForKey);

    xkeytoremove = _CFXPCCreateXPCObjectFromCFObject(keyToRemove);
    require_action(xkeytoremove, xit, error = makeError(kSOSObjectCantBeConvertedToXPCObject));
    xpc_dictionary_set_value(message, kMessageKeyKey, xkeytoremove);
    xpc_release(xkeytoremove);

    talkWithKVS(xpcTransport, message, processQueue, replyBlock);
    xpc_release(message);
    return;

xit:
    if(xkeytoremove)
        xpc_release(xkeytoremove);
    if(message)
        xpc_release(message);
    if (replyBlock)
        replyBlock(NULL, error);
    CFReleaseSafe(error);
}

static void SOSCloudTransportLocalNotification(SOSCloudTransportRef transport, CFStringRef messageToUser, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    SOSXPCCloudTransportRef xpcTransport = (SOSXPCCloudTransportRef)transport;
    secdebug(SOSCKCSCOPE, "start");
    xpc_object_t xLocalNotificationDict = xpc_dictionary_create(NULL, NULL, 0);
    char *headerKey = CFStringToCString(kCFUserNotificationAlertHeaderKey);
    char *message = CFStringToCString(messageToUser);
    xpc_dictionary_set_string(xLocalNotificationDict, headerKey, message);

    xpc_object_t xpcmessage = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(xpcmessage, kMessageKeyVersion, kCKDXPCVersion);
    xpc_dictionary_set_string(xpcmessage, kMessageKeyOperation, kOperationUILocalNotification);
    xpc_dictionary_set_value (xpcmessage, kMessageKeyValue, xLocalNotificationDict);
    xpc_release(xLocalNotificationDict);

    talkWithKVS(xpcTransport, xpcmessage, processQueue, replyBlock);

    free(headerKey);
    free(message);
    xpc_release(xpcmessage);
}

static void SOSCloudTransportSetParams(SOSCloudTransportRef transport, CFDictionaryRef paramsDict, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    secdebug(SOSCKCSCOPE, "start");
    SOSXPCCloudTransportRef xpcTransport = (SOSXPCCloudTransportRef)transport;
    
    xpc_object_t xParamsDict = paramsDict ? _CFXPCCreateXPCObjectFromCFObject(paramsDict) : xpc_null_create();
    
    xpc_object_t xpcmessage = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(xpcmessage, kMessageKeyVersion, kCKDXPCVersion);
    xpc_dictionary_set_string(xpcmessage, kMessageKeyOperation, kOperationSetParams);
    xpc_dictionary_set_value (xpcmessage, kMessageKeyValue, xParamsDict);
    xpc_release(xParamsDict);
    
    talkWithKVS(xpcTransport, xpcmessage, processQueue, replyBlock);
    
    xpc_release(xpcmessage);
}

static void SOSCloudTransportRequestSyncWithAllPeers(SOSCloudTransportRef transport, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    secdebug(SOSCKCSCOPE, "start");
    SOSXPCCloudTransportRef xpcTransport = (SOSXPCCloudTransportRef)transport;
    
    xpc_object_t xpcmessage = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(xpcmessage, kMessageKeyVersion, kCKDXPCVersion);
    xpc_dictionary_set_string(xpcmessage, kMessageKeyOperation, kOperationRequestSyncWithAllPeers);
    
    talkWithKVS(xpcTransport, xpcmessage, processQueue, replyBlock);
    
    xpc_release(xpcmessage);
}

static SOSCloudTransportRef SOSCloudTransportCreateXPCTransport(void)
{
    SOSXPCCloudTransportRef st;
    st = calloc(1, sizeof(*st));
    st->transport.put = SOSCloudTransportPut;
    st->transport.registerKeys = SOSCloudTransportRegisterKeys;
    st->transport.updateKeys = SOSCloudTransportUpdateKeys;
    st->transport.unregisterKeys = SOSCloudTransportUnregisterKeys;
    st->transport.get = SOSCloudTransportGet;
    st->transport.getAll = SOSCloudTransportGetAll;
    st->transport.synchronize = SOSCloudTransportSync;
    st->transport.synchronizeAndWait = SOSCloudTransportSyncAndWait;
    st->transport.clearAll = SOSCloudTransportClearAll;
    st->transport.removeObjectForKey = SOSCloudTransportRemoveObjectForKey;
    st->transport.localNotification = SOSCloudTransportLocalNotification;
    st->transport.setParams = SOSCloudTransportSetParams;
    st->transport.requestSyncWithAllPeers = SOSCloudTransportRequestSyncWithAllPeers;
    SOSXPCCloudTransportInit(st);
    return &st->transport;
}

// MARK: ---------- SOSCloudKeychain concrete client APIs ----------
void SOSCloudKeychainSetItemsChangedBlock(CloudItemsChangedBlock itemsChangedBlock)
{
    secdebug(SOSCKCSCOPE, "start");
    SOSCloudTransportSetItemsChangedBlock(SOSCloudTransportDefaultTransport(),
                                          itemsChangedBlock);
}

// MARK: ---------- SOSCloudKeychain virtual client APIs ----------

void SOSCloudKeychainPutObjectsInCloud(CFDictionaryRef objects, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    SOSCloudTransportRef cTransportRef = SOSCloudTransportDefaultTransport();
    if (cTransportRef)
        cTransportRef->put(cTransportRef, objects, processQueue, replyBlock);
}

void SOSCloudKeychainRegisterKeysAndGet(CFArrayRef keysToRegister, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock, CloudItemsChangedBlock notificationBlock)
{
    SOSCloudTransportRef cTransportRef = SOSCloudTransportDefaultTransport();
    if (cTransportRef)
        cTransportRef->registerKeys(cTransportRef, keysToRegister, processQueue, replyBlock, notificationBlock);
}

bool SOSCloudKeychainUpdateKeys(bool getNewKeysOnly,
                                CFArrayRef alwaysKeys,
                                CFArrayRef afterFirstUnlockKeys,
                                CFArrayRef unlockedKeys,
                                CFErrorRef *error)
{
    SOSCloudTransportRef cTransportRef = SOSCloudTransportDefaultTransport();
    if (cTransportRef)
        return cTransportRef->updateKeys(cTransportRef, getNewKeysOnly, alwaysKeys, afterFirstUnlockKeys, unlockedKeys, error);
    
    return false;
}

void SOSCloudKeychainUnRegisterKeys(CFArrayRef keysToUnregister, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    SOSCloudTransportRef cTransportRef = SOSCloudTransportDefaultTransport();
    if (cTransportRef)
        cTransportRef->unregisterKeys(cTransportRef, keysToUnregister, processQueue, replyBlock);
}

void SOSCloudKeychainHandleUpdate(CFDictionaryRef updates)
{
    SOSCloudTransportRef cTransportRef = SOSCloudTransportDefaultTransport();
    if (cTransportRef->itemsChangedBlock)
        ((CloudItemsChangedBlock)cTransportRef->itemsChangedBlock)(updates);
}

void SOSCloudKeychainGetObjectsFromCloud(CFArrayRef keysToGet, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    SOSCloudTransportRef cTransportRef = SOSCloudTransportDefaultTransport();
    if (cTransportRef)
        cTransportRef->get(cTransportRef, keysToGet, processQueue, replyBlock);
}

void SOSCloudKeychainGetAllObjectsFromCloud(dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    SOSCloudTransportRef cTransportRef = SOSCloudTransportDefaultTransport();
    if (cTransportRef)
        cTransportRef->getAll(cTransportRef, processQueue, replyBlock);
}

void SOSCloudKeychainSynchronizeAndWait(CFArrayRef keysToGet, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    SOSCloudTransportRef cTransportRef = SOSCloudTransportDefaultTransport();
    if (cTransportRef)
        cTransportRef->synchronizeAndWait(cTransportRef, keysToGet, processQueue, replyBlock);
}

//DEBUG ONLY
void SOSCloudKeychainSynchronize(dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    SOSCloudTransportRef cTransportRef = SOSCloudTransportDefaultTransport();
    if (cTransportRef)
        cTransportRef->synchronize(cTransportRef, processQueue, replyBlock);
}

//DEBUG ONLY
void SOSCloudKeychainClearAll(dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    SOSCloudTransportRef cTransportRef = SOSCloudTransportDefaultTransport();
    if (cTransportRef)
        cTransportRef->clearAll(cTransportRef, processQueue, replyBlock);
}

void SOSCloudKeychainRemoveObjectForKey(CFStringRef keyToRemove, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    SOSCloudTransportRef cTransportRef = SOSCloudTransportDefaultTransport();
    if (cTransportRef)
        cTransportRef->removeObjectForKey(cTransportRef, keyToRemove, processQueue, replyBlock);
}

void SOSCloudKeychainUserNotification(CFStringRef messageToUser, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    SOSCloudTransportRef cTransportRef = SOSCloudTransportDefaultTransport();
    if (cTransportRef)
        cTransportRef->localNotification(cTransportRef, messageToUser, processQueue, replyBlock);
}

void SOSCloudKeychainSetParams(CFDictionaryRef paramsDict, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    SOSCloudTransportRef cTransportRef = SOSCloudTransportDefaultTransport();
    if (cTransportRef)
        cTransportRef->setParams(cTransportRef, paramsDict, processQueue, replyBlock);
}

void SOSCloudKeychainRequestSyncWithAllPeers(dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    SOSCloudTransportRef cTransportRef = SOSCloudTransportDefaultTransport();
    if (cTransportRef)
        cTransportRef->requestSyncWithAllPeers(cTransportRef, processQueue, replyBlock);
}

void SOSCloudKeychainSetCallbackMethodXPC(void)
{
    // Call this before making any other calls to CloudKeychainProxy
    CFDictionaryRef paramsDict = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
        kParamCallbackMethod, kParamCallbackMethodXPC, NULL);
    dispatch_queue_t processQueue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    SOSCloudKeychainSetParams(paramsDict, processQueue, ^(CFDictionaryRef returnedValues, CFErrorRef error)
        {
            secerror("set params called back");
        });
    CFReleaseSafe(paramsDict);
}
