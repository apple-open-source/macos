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
 * SOSCloudTransport.c -  Implementation of the transport layer from CKBridge to SOSAccount/SOSCircle
 */
/*
    This XPC service is essentially just a proxy to iCloud KVS, which exists since
    the main security code cannot link against Foundation.
    
    See sendTSARequestWithXPC in tsaSupport.c for how to call the service
    
    The client of an XPC service does not get connection events, nor does it
    need to deal with transactions.
*/

#include <SOSCloudTransport.h>
#include <utilities/SecCFError.h>
#include <dispatch/dispatch.h>
#include <stdlib.h>

static CFStringRef sErrorDomain = CFSTR("com.apple.security.sos.transport.error");

enum
{
    kSOSObjectMallocFailed = 1,
    kAddDuplicateEntry,
    kSOSObjectNotFoundError = 1,
    kSOSObjectCantBeConvertedToXPCObject,
    kSOSOUnexpectedConnectionEvent,
    kSOSOUnexpectedXPCEvent,
    kSOSConnectionNotOpen
};

/* SOSCloudTransport, a statically initialized transport singleton. */
struct SOSCloudTransport
{
    struct CloudTransport t;
    unsigned version;
    CFStringRef kvsID;
    dispatch_once_t once;
};

#pragma mark ---------- Communication with XPC ----------


//------------------------------------------------------------------------------------------------

#include <AssertMacros.h>

#include <xpc/xpc.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFXPCBridge.h>
#include <sysexits.h>
#include <syslog.h>

#include <utilities/debugging.h>
#include <utilities/SecCFWrappers.h>

#include "CKConstants.h"

#define zsecdebug(format...)  secerror(format)

static xpc_connection_t serviceConnection = NULL;
static dispatch_queue_t xpc_queue = NULL;
static CloudKeychainReplyBlock itemsChangedBlock;

static bool handle_xpc_event(const xpc_connection_t peer, xpc_object_t event);
static bool xpc_event_filter(const xpc_connection_t peer, xpc_object_t event, CFErrorRef *error);

// extern CFTypeRef _CFXPCCreateCFObjectFromXPCObject(xpc_object_t xo);
// extern xpc_object_t _CFXPCCreateXPCObjectFromCFObject(CFTypeRef cf);

// Debug
static void describeXPCObject(char *prefix, xpc_object_t object);
static void describeXPCType(char *prefix, xpc_type_t xtype);



#define WANTXPCREPLY 0

#pragma mark ----- utilities -----

static CFErrorRef makeError(CFIndex which)
{
    CFDictionaryRef userInfo = NULL;
    return CFErrorCreate(kCFAllocatorDefault, sErrorDomain, which, userInfo);
}

#pragma mark ----- SPI -----

static void initXPCConnection()
{
    zsecdebug("initXPCConnection\n");
    
    xpc_queue = dispatch_queue_create(xpcServiceName, DISPATCH_QUEUE_SERIAL);

    serviceConnection = xpc_connection_create_mach_service(xpcServiceName, xpc_queue, 0);

//    serviceConnection = xpc_connection_create(xpcServiceName, xpc_queue);
    zsecdebug("serviceConnection: %p\n", serviceConnection);

    xpc_connection_set_event_handler(serviceConnection, ^(xpc_object_t event)
    {
        zsecdebug("xpc_connection_set_event_handler\n");
        handle_xpc_event(serviceConnection, event);
    });

    xpc_connection_resume(serviceConnection);
    xpc_retain(serviceConnection);
}

#if 0   // unused
static void closeXPCConnection()
{
    zsecdebug("closeXPCConnection\n");
    xpc_release(serviceConnection);
}
#endif

static void setItemsChangedBlock(CloudKeychainReplyBlock icb)
{
    if (icb != itemsChangedBlock)
    {
        if (itemsChangedBlock)
            Block_release(itemsChangedBlock);
        itemsChangedBlock = icb;
        Block_copy(itemsChangedBlock);
    }
}

// typedef void (^CloudKeychainReplyBlock)(CFDictionaryRef returnedValues, CFErrorRef error);

static bool handle_xpc_event(const xpc_connection_t peer, xpc_object_t event)
{
    CFErrorRef localError = NULL;
    zsecdebug(">>>>> handle_connection_event via event_handler <<<<<\n");
    bool result = false;
    if ((result = xpc_event_filter(peer, event, &localError)))
    {
        const char *operation = xpc_dictionary_get_string(event, kMessageKeyOperation);
        if (!operation || strcmp(operation, kMessageOperationItemChanged))  // some op we don't care about
        {
            zsecdebug("operation: %s", operation);
            return result;
        }
        
        xpc_object_t xrv = xpc_dictionary_get_value(event, kMessageKeyValue);
        if (!xrv)
        {
            zsecdebug("xrv null for kMessageKeyValue");
            return result;
        }
        describeXPCObject("xrv", xrv);
        
        CFDictionaryRef returnedValues = _CFXPCCreateCFObjectFromXPCObject(xrv);
        zsecdebug("returnedValues: %@", returnedValues);

        if (itemsChangedBlock)
            itemsChangedBlock(returnedValues, localError);
    }
    CFReleaseSafe(localError);

    return result;
}

static bool xpc_event_filter(const xpc_connection_t peer, xpc_object_t event, CFErrorRef *error)
{
    // return true if the type is XPC_TYPE_DICTIONARY (and therefore something more to process)
    zsecdebug("handle_connection_event\n");
    xpc_type_t xtype = xpc_get_type(event);
    describeXPCType("handle_xpc_event", xtype);
    if (XPC_TYPE_CONNECTION == xtype)
    {
        zsecdebug("handle_xpc_event: XPC_TYPE_CONNECTION (unexpected)");
        // The client of an XPC service does not get connection events
        // For nwo, we log this and keep going
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
        zsecdebug("default: xpc error: %s\n", xpc_dictionary_get_string(event, XPC_ERROR_KEY_DESCRIPTION));
        if (error)
            *error = makeError(kSOSOUnexpectedConnectionEvent); // FIX
    }
    else
    if (XPC_TYPE_DICTIONARY == xtype)
    {
        zsecdebug("received dictionary event %p\n", event);
        return true;
    }
    else
    {
        zsecdebug("default: unexpected connection event %p\n", event);
        describeXPCObject("handle_xpc_event: obj : ", event);
        if (error)
            *error = makeError(kSOSOUnexpectedXPCEvent);
    }
    return false;
}

static void talkWithKVS(xpc_object_t message, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    secerror("start");
    __block CFErrorRef error = NULL;
    __block CFTypeRef object = NULL;
    
    dispatch_block_t callback = ^{
            secerror("callback");
            if (replyBlock)
                replyBlock(object, error);
    //        if (object)
     //           CFRelease(object);
            if (error)
            {
                secerror("callback error: %@", error);
     //           CFRelease(error);
            }
            dispatch_release(processQueue);
        };
    
    require_action(serviceConnection, xit, error = makeError(kSOSConnectionNotOpen));
    require_action(message, xit, error = makeError(kSOSObjectNotFoundError));
    dispatch_retain(processQueue);
    secerror("xpc_connection_send_message_with_reply called");
    
    Block_copy(callback);   // TODO -- this should not be needed, I think
    
//#if !WANTXPCREPLY
 //   xpc_connection_send_message(serviceConnection, message);            // Send message; don't want a reply
//#else
    xpc_connection_send_message_with_reply(serviceConnection, message, xpc_queue, ^(xpc_object_t reply)
        {
            secerror("xpc_connection_send_message_with_reply handler called back");
            if (xpc_event_filter(serviceConnection, reply, &error) && reply)
            {
                describeXPCObject("getValuesFromKVS: reply : ", reply);
                xpc_object_t xrv = xpc_dictionary_get_value(reply, kMessageKeyValue);
                if (xrv)
                {
                    describeXPCObject("talkWithKVS: xrv: ", xrv);
                    /*
                        * The given XPC object must be one that was previously returned by
                        * _CFXPCCreateXPCMessageWithCFObject().
                    */
                    object = _CFXPCCreateCFObjectFromXPCObject(xrv);   // CF object is retained; release in callback
                    secerror("converted CF object: %@", object);
                }
                else
                    secerror("missing value reply");
            }
            dispatch_async(processQueue, callback);
        });
//#endif

//sleep(5);   // DEBUG DEBUG FIX
 //   xpc_release(message);
    return;
    
xit:
    secerror("talkWithKVS error: %@", error);
    if (replyBlock)
        dispatch_async(processQueue, callback);
}

static void putValuesWithXPC(CFDictionaryRef values, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    CFErrorRef error = NULL;

    require_action(values, xit, error = makeError(kSOSObjectNotFoundError));

    xpc_object_t message = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(message, kMessageKeyVersion, kCKDXPCVersion);
    xpc_dictionary_set_string(message, kMessageKeyOperation, kOperationPUTDictionary);
    
    xpc_object_t xobject = _CFXPCCreateXPCObjectFromCFObject(values);
    require_action(xobject, xit, error = makeError(kSOSObjectCantBeConvertedToXPCObject));
    xpc_dictionary_set_value(message, kMessageKeyValue, xobject);
    
    talkWithKVS(message, processQueue, replyBlock);
    return;

xit:
    if (replyBlock)
        replyBlock(NULL, error);
}

static void synchronizeKVS(dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    xpc_object_t message = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(message, kMessageKeyVersion, kCKDXPCVersion);
    xpc_dictionary_set_string(message, kMessageKeyOperation, kOperationSynchronize);
    talkWithKVS(message, processQueue, replyBlock);
}

static void clearAll(dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    xpc_object_t message = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(message, kMessageKeyVersion, kCKDXPCVersion);
    xpc_dictionary_set_string(message, kMessageKeyOperation, kOperationClearStore);
    talkWithKVS(message, processQueue, replyBlock);
}

static void getValuesFromKVS(CFArrayRef keysToGet, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    secerror("start");
    CFErrorRef error = NULL;
    xpc_object_t xkeysOfInterest = xpc_dictionary_create(NULL, NULL, 0);
    xpc_object_t xkeysToGet = keysToGet ? _CFXPCCreateXPCObjectFromCFObject(keysToGet) : xpc_null_create();

    require_action(xkeysToGet, xit, error = makeError(kSOSObjectNotFoundError));

    xpc_dictionary_set_value(xkeysOfInterest, kMessageKeyKeysToGet, xkeysToGet);

    xpc_object_t message = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(message, kMessageKeyVersion, kCKDXPCVersion);
    xpc_dictionary_set_string(message, kMessageKeyOperation, kOperationGETv2);
    xpc_dictionary_set_value(message, kMessageKeyValue, xkeysOfInterest);
    
    talkWithKVS(message, processQueue, replyBlock);

    xpc_release(message);
    return;
    
xit:
    if (replyBlock)
        replyBlock(NULL, error);
}

static void registerKeysForKVS(CFArrayRef keysToGet, CFStringRef clientIdentifier, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    secerror("start");

    xpc_object_t xkeysOfInterest = xpc_dictionary_create(NULL, NULL, 0);
    xpc_object_t xkeysToRegister = keysToGet ? _CFXPCCreateXPCObjectFromCFObject(keysToGet) : xpc_null_create();
    xpc_dictionary_set_value(xkeysOfInterest, kMessageKeyKeysToGet, xkeysToRegister);

    xpc_object_t message = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(message, kMessageKeyVersion, kCKDXPCVersion);
    xpc_dictionary_set_string(message, kMessageKeyOperation, kOperationRegisterKeysAndGet);
    xpc_dictionary_set_value(message, kMessageKeyValue, xkeysOfInterest);
    
    if (clientIdentifier)
    {
        char *clientid = CFStringToCString(clientIdentifier);
        if (clientid)
        {
            xpc_dictionary_set_string(message, kMessageKeyClientIdentifier, clientid);
            free(clientid);
        }
    }

    setItemsChangedBlock(replyBlock);
    talkWithKVS(message, processQueue, replyBlock);

    xpc_release(message);
}

static void unregisterKeysForKVS(CFArrayRef keysToUnregister, CFStringRef clientIdentifier, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
#if NO_SERVERz
    if (gCKD->unregisterKeys) {
        return gCKD->unregisterKeys(...);
    }
#endif

    secerror("start");

    xpc_object_t xkeysOfInterest = xpc_dictionary_create(NULL, NULL, 0);
    xpc_object_t xkeysToUnregister = keysToUnregister ? _CFXPCCreateXPCObjectFromCFObject(keysToUnregister) : xpc_null_create();
    xpc_dictionary_set_value(xkeysOfInterest, kMessageKeyKeysToGet, xkeysToUnregister);

    xpc_object_t message = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(message, kMessageKeyVersion, kCKDXPCVersion);
    xpc_dictionary_set_string(message, kMessageKeyOperation, kOperationUnregisterKeys);
    xpc_dictionary_set_value(message, kMessageKeyValue, xkeysOfInterest);
    
    if (clientIdentifier)
    {
        char *clientid = CFStringToCString(clientIdentifier);
        if (clientid)
        {
            xpc_dictionary_set_string(message, kMessageKeyClientIdentifier, clientid);
            free(clientid);
        }
    }

    talkWithKVS(message, processQueue, replyBlock);

    xpc_release(message);
}

#pragma mark ----- DEBUG Utilities -----

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
    zsecdebug("%s%s\n", prefix, desc);
    free(desc);
    }
    else
        zsecdebug("%s<NULL>\n", prefix);
//#endif
}

static void describeXPCType(char *prefix, xpc_type_t xtype)
{
    /*
        Add these as necessary:
        XPC_TYPE_ENDPOINT
        XPC_TYPE_NULL
        XPC_TYPE_BOOL
        XPC_TYPE_INT64
        XPC_TYPE_UINT64
        XPC_TYPE_DOUBLE
        XPC_TYPE_DATE
        XPC_TYPE_DATA
        XPC_TYPE_STRING
        XPC_TYPE_UUID
        XPC_TYPE_FD
        XPC_TYPE_SHMEM
        XPC_TYPE_ARRAY
    */
    
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

    zsecdebug("%s type:%s\n", prefix, msg);
#endif
}

#pragma mark ---------- SOSCloudTransport ----------

static void SOSCloudTransportPut(SOSCloudTransportRef transport, CFDictionaryRef valuesToPut, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    secerror("%@", valuesToPut);
    putValuesWithXPC(valuesToPut, processQueue, replyBlock);
}

static void SOSCloudTransportGet(SOSCloudTransportRef transport, CFArrayRef keysToGet, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
//    struct SOSCloudTransport *t = (struct SOSCloudTransport *)transport;
    /* Get from KVS */
    secerror("%@", keysToGet);
    getValuesFromKVS(keysToGet, processQueue, replyBlock);
}

static void SOSCloudTransportRegisterKeys(SOSCloudTransportRef transport, CFArrayRef keysToGet, CFStringRef clientIdentifier, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    /* RegisterKeys with KVS */
    registerKeysForKVS(keysToGet, clientIdentifier, processQueue, replyBlock);
}

static void SOSCloudTransportUnregisterKeys(SOSCloudTransportRef transport, CFArrayRef keysToUnregister, CFStringRef clientIdentifier, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    /* unregister from KVS */
    unregisterKeysForKVS(keysToUnregister, clientIdentifier, processQueue, replyBlock);
}

static void SOSCloudTransportGetAll(SOSCloudTransportRef transport, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    /* Get from KVS */
    getValuesFromKVS(NULL, processQueue, replyBlock);
}

static void SOSCloudTransportSync(SOSCloudTransportRef transport, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
//    struct SOSCloudTransport *t = (struct SOSCloudTransport *)transport;
    /* Sync KVS */
    synchronizeKVS(processQueue, replyBlock);
}

static void SOSCloudTransportClearAll(SOSCloudTransportRef transport, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
//    struct SOSCloudTransport *t = (struct SOSCloudTransport *)transport;
    /* clear KVS */
    clearAll(processQueue, replyBlock);
}

static void SOSCloudTransportSetItemsChangedBlock(SOSCloudTransportRef transport, CloudKeychainReplyBlock itemsChangedBlock)
{
//    struct SOSCloudTransport *t = (struct SOSCloudTransport *)transport;
    /* clear KVS */
    setItemsChangedBlock(itemsChangedBlock);
}

static void SOSCloudTransportInit(void *ctx)
{
    struct SOSCloudTransport *st = (struct SOSCloudTransport *)ctx;
    st->t.put = SOSCloudTransportPut;
    st->t.registerKeys = SOSCloudTransportRegisterKeys;
    st->t.unregisterKeys = SOSCloudTransportUnregisterKeys;
    st->t.get = SOSCloudTransportGet;
    st->t.getAll = SOSCloudTransportGetAll;
    st->t.synchronize = SOSCloudTransportSync;
    st->t.clearAll = SOSCloudTransportClearAll;
    st->t.setItemsChangedBlock = SOSCloudTransportSetItemsChangedBlock;
    
    if (st->kvsID)
        CFRetain(st->kvsID);
    
    st->version = 0;
    initXPCConnection();
}

SOSCloudTransportRef SOSCloudTransportDefaultTransport(CFStringRef kvsID, uint32_t options)
{
    static struct SOSCloudTransport cloudTransport = {};
    cloudTransport.kvsID = kvsID;
    dispatch_once_f(&cloudTransport.once, &cloudTransport, SOSCloudTransportInit);
    return &cloudTransport.t;
}


