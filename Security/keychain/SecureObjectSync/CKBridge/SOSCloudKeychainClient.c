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

/*
    SOSCloudTransport.c -  Implementation of the transport layer from CKBridge to SOSAccount/SOSCircle
    These are the exported functions from CloudKeychainProxy
*/

/*
    This XPC service is essentially just a proxy to iCloud KVS, which exists since
    at one time the main security code could not link against Foundation.
    
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
#include <os/activity.h>
#include <CoreFoundation/CFUserNotification.h>
#include "keychain/SecureObjectSync/SOSInternal.h"
#include <notify.h>

#include <utilities/debugging.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecXPCError.h>

#include "SOSCloudKeychainConstants.h"
#include "SOSCloudKeychainClient.h"
#include "SOSUserKeygen.h"
#include "SecOTRSession.h"

#include <os/activity.h>
#include <os/state_private.h>


static CFStringRef sErrorDomain = CFSTR("com.apple.security.sos.transport.error");

#define SOSCKCSCOPE "sync"

// MARK: ---------- SOSCloudTransport ----------

static SOSCloudTransportRef SOSCloudTransportCreateXPCTransport(void);

void SOSCloudTransportGet(SOSCloudTransportRef transport, CFArrayRef keysToGet, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);



/* Return the singleton cloud transport instance. */
CFDictionaryRef SOSCloudCopyKVSState(void) {
    __block CFDictionaryRef retval = NULL;

    static dispatch_queue_t processQueue = NULL;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        processQueue = dispatch_queue_create("KVSStateCapture", DISPATCH_QUEUE_SERIAL);
    });

    if (processQueue == NULL)
        return NULL;
    
    dispatch_semaphore_t waitSemaphore = NULL;

    waitSemaphore = dispatch_semaphore_create(0);

    CloudKeychainReplyBlock replyBlock = ^ (CFDictionaryRef returnedValues, CFErrorRef error) {
        retval = returnedValues;
        if (retval) CFRetain(retval);
        dispatch_semaphore_signal(waitSemaphore);
    };

    SOSCloudKeychainGetAllObjectsFromCloud(processQueue, replyBlock);

    dispatch_semaphore_wait(waitSemaphore, DISPATCH_TIME_FOREVER);
    dispatch_release(waitSemaphore);

    return retval;
}


os_state_block_t kvsStateBlock = ^os_state_data_t(os_state_hints_t hints) {
    os_state_data_t retval = NULL;
    __block CFDictionaryRef kvsdict = NULL;
    CFDataRef serializedKVS = NULL;

    require_quiet(hints->osh_api == 3, errOut); // only grab on sysdiagnose or command lin
    
    kvsdict = SOSCloudCopyKVSState();

    require_quiet(kvsdict, errOut);
    serializedKVS = CFPropertyListCreateData(kCFAllocatorDefault, kvsdict, kCFPropertyListBinaryFormat_v1_0, 0, NULL);
    size_t statelen = CFDataGetLength(serializedKVS);
    retval = (os_state_data_t)calloc(1, OS_STATE_DATA_SIZE_NEEDED(statelen));
    require_quiet(retval, errOut);
    
    retval->osd_type = OS_STATE_DATA_SERIALIZED_NSCF_OBJECT;
    memcpy(retval->osd_data, CFDataGetBytePtr(serializedKVS), statelen);
    retval->osd_size = statelen;
    strcpy(retval->osd_title, "CloudCircle KVS Object");
errOut:
    CFReleaseNull(kvsdict);
    CFReleaseNull(serializedKVS);
    return retval;
};

static SOSCloudTransportRef defaultTransport = NULL;

void
SOSCloudTransportSetDefaultTransport(SOSCloudTransportRef transport)
{
    defaultTransport = transport;
}

static SOSCloudTransportRef SOSCloudTransportDefaultTransport(void)
{
    static dispatch_once_t sTransportOnce;
    dispatch_once(&sTransportOnce, ^{
        if (defaultTransport == NULL) {
            defaultTransport = SOSCloudTransportCreateXPCTransport();
            // provide state handler to sysdiagnose and logging
            os_state_add_handler(dispatch_get_global_queue(SOS_TRANSPORT_PRIORITY, 0), kvsStateBlock);
        }
    });
    return defaultTransport;
}


// MARK: ----- utilities -----

static CFErrorRef makeError(CFIndex which)
{
    CFDictionaryRef userInfo = NULL;
    return CFErrorCreate(kCFAllocatorDefault, sErrorDomain, which, userInfo);
}

// MARK: ---------- SOSXPCCloudTransport ----------

typedef struct SOSXPCCloudTransport *SOSXPCCloudTransportRef;
struct SOSXPCCloudTransport
{
    struct SOSCloudTransport transport;
    xpc_connection_t serviceConnection;
    dispatch_queue_t xpc_queue;
};

static void teardownServiceConnection(SOSXPCCloudTransportRef transport)
{
    secnotice(SOSCKCSCOPE, "CKP Transport: tearing down xpc connection");
    dispatch_assert_queue(transport->xpc_queue);
    if (transport->serviceConnection) {
        xpc_release(transport->serviceConnection);
        transport->serviceConnection = NULL;
    }
}

static void setupServiceConnection(SOSXPCCloudTransportRef transport)
{
    secnotice(SOSCKCSCOPE, "CKP Transport: setting up xpc connection");
    dispatch_assert_queue(transport->xpc_queue);

    transport->serviceConnection = xpc_connection_create_mach_service(kCKPServiceName, transport->xpc_queue, 0);

    secdebug(SOSCKCSCOPE, "serviceConnection: %@", transport->serviceConnection);

    xpc_connection_set_event_handler(transport->serviceConnection, ^(xpc_object_t event) {
        secdebug(SOSCKCSCOPE, "CKP Transport, xpc_connection_set_event_handler");
        if(event == XPC_ERROR_CONNECTION_INVALID){
            secnotice(SOSCKCSCOPE, "CKP Transport: xpc connection invalid. Will tear down connection.");
            dispatch_async(transport->xpc_queue, ^{
                teardownServiceConnection(transport);
            });
        }
    });

    xpc_connection_activate(transport->serviceConnection);
}

static void SOSXPCCloudTransportInit(SOSXPCCloudTransportRef transport)
{
    secdebug(SOSCKCSCOPE, "initXPCConnection");
    
    transport->xpc_queue = dispatch_queue_create(kCKPServiceName, DISPATCH_QUEUE_SERIAL);
    
    dispatch_sync(transport->xpc_queue, ^{
        setupServiceConnection(transport);
    });

    // Any time a new session starts, reestablish the XPC connections.
    int token;
    notify_register_dispatch("com.apple.system.loginwindow.desktopUp", &token, transport->xpc_queue, ^(int token2) {
        secnotice(SOSCKCSCOPE, "CKP Transport: desktopUp happened, reestablishing xpc connections");
        teardownServiceConnection(transport);
    });
}

typedef void (^ProxyReplyBlock)(xpc_object_t reply);

static bool messageToProxy(SOSXPCCloudTransportRef transport, xpc_object_t message, CFErrorRef *error, dispatch_queue_t processQueue, ProxyReplyBlock replyBlock) {
    __block CFErrorRef connectionError = NULL;

    dispatch_sync(transport->xpc_queue, ^{
        if (transport->serviceConnection == NULL) {
            setupServiceConnection(transport);
        }

        if (transport->serviceConnection && message) {
            xpc_connection_send_message_with_reply(transport->serviceConnection, message, processQueue, replyBlock);
        } else {
            connectionError = makeError(kSOSConnectionNotOpen);
        }
    });
    return CFErrorPropagate(connectionError, error);
}

static void talkWithKVS(SOSXPCCloudTransportRef transport, xpc_object_t message, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    CFErrorRef messagingError = NULL;
    dispatch_retain(processQueue);
    bool messaged = messageToProxy(transport, message, &messagingError, transport->xpc_queue, ^(xpc_object_t reply) {
        CFErrorRef serverError = NULL;
        CFTypeRef object = NULL;

        if (xpc_get_type(reply) == XPC_TYPE_DICTIONARY) {
            xpc_object_t xrv = xpc_dictionary_get_value(reply, kMessageKeyValue);
            if (xrv) {
                object = _CFXPCCreateCFObjectFromXPCObject(xrv);
            } else {
                secerror("missing value reply");
            }

            xpc_object_t xerror = xpc_dictionary_get_value(reply, kMessageKeyError);
            if (xerror) {
                serverError = SecCreateCFErrorWithXPCObject(xerror);
            }
        } else {
            serverError = makeError(kSOSMessageInvalid);
            secerror("Odd reply from CloudKeychainProxy: %@: %@", reply, serverError);
        }
        dispatch_async(processQueue, ^{
            if (replyBlock) {
                replyBlock(object, serverError);
            }
            CFReleaseSafe(object);
            if (serverError) {
                secerror("callback error: %@", serverError);
                CFReleaseSafe(serverError);
            }
            dispatch_release(processQueue);
        });
    });

    if (!messaged) {
        secerror("talkWithKVS error: %@", messagingError);
        dispatch_async(processQueue, ^{
            if (replyBlock)
                replyBlock(NULL, messagingError);
            CFReleaseSafe(messagingError);
            dispatch_release(processQueue);
        });
    }
}

// MARK: ---------- SOSXPCCloudTransport Client Calls ----------

/* Concrete function backend implementations. */
static void SOSCloudTransportSetItemsChangedBlock(SOSCloudTransportRef transport,
                                                  CloudItemsChangedBlock itemsChangedBlock) {
    if (transport->itemsChangedBlock != itemsChangedBlock)
    {
        secnotice(SOSCKCSCOPE, "Changing itemsChangedBlock");
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
void SOSCloudTransportGet(SOSCloudTransportRef transport, CFArrayRef keysToGet, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
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

//
// Handles NULL by seting xpc_null.
static void SecXPCDictionarySetCFObject(xpc_object_t xdict, const char *key, CFTypeRef object)
{
    xpc_object_t xpc_obj = object ? _CFXPCCreateXPCObjectFromCFObject(object) : xpc_null_create();
    xpc_dictionary_set_value(xdict, key, xpc_obj);
    xpc_release(xpc_obj);
}

static void SOSCloudTransportUpdateKeys(SOSCloudTransportRef transport,
                                        CFDictionaryRef keys,
                                        CFStringRef accountUUID,
                                        dispatch_queue_t processQueue,
                                        CloudKeychainReplyBlock replyBlock)
{
    SOSXPCCloudTransportRef xpcTransport = (SOSXPCCloudTransportRef)transport;

    xpc_object_t xkeysOfInterest = xpc_dictionary_create(NULL, NULL, 0);
    SecXPCDictionarySetCFObject(xkeysOfInterest, kMessageAllKeys, keys);

    xpc_object_t message = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(message, kMessageKeyVersion, kCKDXPCVersion);
    xpc_dictionary_set_string(message, kMessageKeyOperation, kOperationRegisterKeys);
    xpc_dictionary_set_value(message, kMessageKeyValue, xkeysOfInterest);
    SecXPCDictionarySetCFObject(message, kMessageKeyAccountUUID, accountUUID);

    talkWithKVS(xpcTransport, message, processQueue, replyBlock);
    xpc_release(message);
    xpc_release(xkeysOfInterest);
}

static void SOSCloudTransportRemoveKeys(SOSCloudTransportRef transport,
                                        CFArrayRef keys,
                                        CFStringRef accountUUID,
                                        dispatch_queue_t processQueue,
                                        CloudKeychainReplyBlock replyBlock)
{
        SOSXPCCloudTransportRef xpcTransport = (SOSXPCCloudTransportRef)transport;
    
        xpc_object_t message = xpc_dictionary_create(NULL, NULL, 0);
        xpc_dictionary_set_uint64(message, kMessageKeyVersion, kCKDXPCVersion);
    
        xpc_dictionary_set_string(message, kMessageKeyOperation, kOperationRemoveKeys);
        SecXPCDictionarySetCFObject(message, kMessageKeyAccountUUID, accountUUID);
        SecXPCDictionarySetCFObject(message, kMessageKeyValue, keys);

        talkWithKVS(xpcTransport, message, processQueue, replyBlock);
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

static void SOSCloudTransportSyncAndWait(SOSCloudTransportRef transport, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    SOSXPCCloudTransportRef xpcTransport = (SOSXPCCloudTransportRef)transport;
    secnotice(SOSCKCSCOPE, "%s XPC request to CKD: %s", kWAIT2MINID, kOperationSynchronizeAndWait);

    xpc_object_t message = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(message, kMessageKeyVersion, kCKDXPCVersion);
    xpc_dictionary_set_string(message, kMessageKeyOperation, kOperationSynchronizeAndWait);

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

static void SOSCloudTransportRequestSyncWithPeers(SOSCloudTransportRef transport, CFArrayRef /* CFStringRef */ peers, CFArrayRef /* CFStringRef */ backupPeers, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    secdebug(SOSCKCSCOPE, "start");
    SOSXPCCloudTransportRef xpcTransport = (SOSXPCCloudTransportRef)transport;
    
    xpc_object_t xpcmessage = xpc_dictionary_create(NULL, NULL, 0);

    xpc_dictionary_set_uint64(xpcmessage, kMessageKeyVersion, kCKDXPCVersion);
    xpc_dictionary_set_string(xpcmessage, kMessageKeyOperation, kOperationRequestSyncWithPeers);

    SecXPCDictionarySetCFObject(xpcmessage, kMessageKeyPeerIDList, peers);
    SecXPCDictionarySetCFObject(xpcmessage, kMesssgeKeyBackupPeerIDList, backupPeers);

    talkWithKVS(xpcTransport, xpcmessage, processQueue, replyBlock);
    
    xpc_release(xpcmessage);
}


static bool SOSCloudTransportHasPendingKey(SOSCloudTransportRef transport, CFStringRef keyName, CFErrorRef* error)
{
    secdebug(SOSCKCSCOPE, "start");
    SOSXPCCloudTransportRef xpcTransport = (SOSXPCCloudTransportRef)transport;

    __block bool kvsHasMessage = false;

    xpc_object_t xpcmessage = xpc_dictionary_create(NULL, NULL, 0);

    xpc_dictionary_set_uint64(xpcmessage, kMessageKeyVersion, kCKDXPCVersion);
    xpc_dictionary_set_string(xpcmessage, kMessageKeyOperation, kOperationHasPendingKey);

    SecXPCDictionarySetCFObject(xpcmessage, kMessageKeyKey, keyName);

    dispatch_semaphore_t kvsWait = dispatch_semaphore_create(0);
    bool kvsSent = messageToProxy(xpcTransport, xpcmessage, error, dispatch_get_global_queue(SOS_TRANSPORT_PRIORITY, 0), ^(xpc_object_t reply) {
        kvsHasMessage = xpc_dictionary_get_bool(reply, kMessageKeyValue);
        dispatch_semaphore_signal(kvsWait);
    });

    if (kvsSent) {
        dispatch_semaphore_wait(kvsWait, DISPATCH_TIME_FOREVER);
    }

    dispatch_release(kvsWait);

    return kvsSent && kvsHasMessage;
}


static void SOSCloudTransportRequestEnsurePeerRegistration(SOSCloudTransportRef transport, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    secdebug(SOSCKCSCOPE, "start");
    SOSXPCCloudTransportRef xpcTransport = (SOSXPCCloudTransportRef)transport;

    xpc_object_t xpcmessage = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(xpcmessage, kMessageKeyVersion, kCKDXPCVersion);
    xpc_dictionary_set_string(xpcmessage, kMessageKeyOperation, kOperationRequestEnsurePeerRegistration);

    talkWithKVS(xpcTransport, xpcmessage, processQueue, replyBlock);

    xpc_release(xpcmessage);
}

static void SOSCloudTransportRequestPerfCounters(SOSCloudTransportRef transport, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    secdebug(SOSCKCSCOPE, "start");
    SOSXPCCloudTransportRef xpcTransport = (SOSXPCCloudTransportRef)transport;

    xpc_object_t xpcmessage = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(xpcmessage, kMessageKeyVersion, kCKDXPCVersion);
    xpc_dictionary_set_string(xpcmessage, kMessageKeyOperation, kOperationPerfCounters);

    talkWithKVS(xpcTransport, xpcmessage, processQueue, replyBlock);

    xpc_release(xpcmessage);
}


static void SOSCloudTransportFlush(SOSCloudTransportRef transport, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    secdebug(SOSCKCSCOPE, "start");
    SOSXPCCloudTransportRef xpcTransport = (SOSXPCCloudTransportRef)transport;

    xpc_object_t xpcmessage = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(xpcmessage, kMessageKeyVersion, kCKDXPCVersion);
    xpc_dictionary_set_string(xpcmessage, kMessageKeyOperation, kOperationFlush);

    talkWithKVS(xpcTransport, xpcmessage, processQueue, replyBlock);

    xpc_release(xpcmessage);
}

static SOSCloudTransportRef SOSCloudTransportCreateXPCTransport(void)
{
    SOSXPCCloudTransportRef st;
    st = calloc(1, sizeof(*st));
    st->transport.put = SOSCloudTransportPut;
    st->transport.updateKeys = SOSCloudTransportUpdateKeys;

    st->transport.get = SOSCloudTransportGet;
    st->transport.getAll = SOSCloudTransportGetAll;
    st->transport.synchronize = SOSCloudTransportSync;
    st->transport.synchronizeAndWait = SOSCloudTransportSyncAndWait;
    st->transport.clearAll = SOSCloudTransportClearAll;
    st->transport.requestSyncWithPeers = SOSCloudTransportRequestSyncWithPeers;
    st->transport.hasPendingKey = SOSCloudTransportHasPendingKey;
    st->transport.requestEnsurePeerRegistration = SOSCloudTransportRequestEnsurePeerRegistration;
    st->transport.requestPerfCounters = SOSCloudTransportRequestPerfCounters;
    st->transport.flush = SOSCloudTransportFlush;
    st->transport.removeKeys = SOSCloudTransportRemoveKeys;
    st->transport.itemsChangedBlock = Block_copy(^CFArrayRef(CFDictionaryRef changes) {
        secerror("Calling default itemsChangedBlock - fatal: %@", changes);
        assert(false);
        return NULL;
        });
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

void SOSCloudKeychainUpdateKeys(CFDictionaryRef keys, CFStringRef accountUUID, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    SOSCloudTransportRef cTransportRef = SOSCloudTransportDefaultTransport();
    if (cTransportRef)
        cTransportRef->updateKeys(cTransportRef, keys, accountUUID, processQueue, replyBlock);
}


void SOSCloudKeychainRemoveKeys(CFArrayRef keys, CFStringRef accountUUID, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    SOSCloudTransportRef cTransportRef = SOSCloudTransportDefaultTransport();
    if (cTransportRef)
        cTransportRef->removeKeys(cTransportRef, keys, accountUUID, processQueue, replyBlock);
}

CF_RETURNS_RETAINED CFArrayRef SOSCloudKeychainHandleUpdateMessage(CFDictionaryRef updates)
{
    CFArrayRef result = NULL;
    SOSCloudTransportRef cTransportRef = SOSCloudTransportDefaultTransport();
    if (cTransportRef->itemsChangedBlock)
        result = ((CloudItemsChangedBlock)cTransportRef->itemsChangedBlock)(updates);
    return result;
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

void SOSCloudKeychainSynchronizeAndWait(dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    SOSCloudTransportRef cTransportRef = SOSCloudTransportDefaultTransport();
    if (cTransportRef)
        cTransportRef->synchronizeAndWait(cTransportRef, processQueue, replyBlock);
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
    secnotice("circleOps", "SOSCloudKeychainClearAll called");
    if (cTransportRef)
        cTransportRef->clearAll(cTransportRef, processQueue, replyBlock);
}

void SOSCloudKeychainRequestSyncWithPeers(CFArrayRef /* CFStringRef */ peers, CFArrayRef /* CFStringRef */ backupPeers, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    SOSCloudTransportRef cTransportRef = SOSCloudTransportDefaultTransport();
    if (cTransportRef)
        cTransportRef->requestSyncWithPeers(cTransportRef, peers, backupPeers, processQueue, replyBlock);
}

bool SOSCloudKeychainHasPendingKey(CFStringRef keyName, CFErrorRef* error) {
    SOSCloudTransportRef cTransportRef = SOSCloudTransportDefaultTransport();

    return cTransportRef && cTransportRef->hasPendingKey(cTransportRef, keyName, error);
}

void SOSCloudKeychainRequestEnsurePeerRegistration(dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    SOSCloudTransportRef cTransportRef = SOSCloudTransportDefaultTransport();
    if (cTransportRef)
        cTransportRef->requestEnsurePeerRegistration(cTransportRef, processQueue, replyBlock);
}

void SOSCloudKeychainRequestPerfCounters(dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    SOSCloudTransportRef cTransportRef = SOSCloudTransportDefaultTransport();
    if (cTransportRef)
        cTransportRef->requestPerfCounters(cTransportRef, processQueue, replyBlock);
}


void SOSCloudKeychainFlush(dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    SOSCloudTransportRef cTransportRef = SOSCloudTransportDefaultTransport();
    if (cTransportRef)
        cTransportRef->flush(cTransportRef, processQueue, replyBlock);
}
