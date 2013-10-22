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
 * SOSCloudKeychainClient.h -  Implementation of the transport layer from CKBridge to SOSAccount/SOSCircle
 */

/*!
    @header SOSCloudKeychainClient
    The functions provided in SOSCloudTransport.h provide an interface
    from CKBridge to SOSAccount/SOSCircle
 */

#ifndef _SOSCLOUDKEYCHAINCLIENT_H_
#define _SOSCLOUDKEYCHAINCLIENT_H_

#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFError.h>
#include <dispatch/dispatch.h>

__BEGIN_DECLS


// MARK: ---------- SOSCloudTransport ----------

enum
{
    kSOSObjectMallocFailed = 1,
    kAddDuplicateEntry,
    kSOSObjectNotFoundError = 1,
    kSOSObjectCantBeConvertedToXPCObject,
    kSOSOUnexpectedConnectionEvent,
    kSOSOXPCErrorEvent,
    kSOSOUnexpectedXPCEvent,
    kSOSConnectionNotOpen
};

typedef void (^CloudItemsChangedBlock)(CFDictionaryRef values);
typedef void (^CloudKeychainReplyBlock)(CFDictionaryRef returnedValues, CFErrorRef error);

/* SOSCloudTransport protocol.  */
typedef struct SOSCloudTransport *SOSCloudTransportRef;
struct SOSCloudTransport
{
    void (*put)(SOSCloudTransportRef transport, CFDictionaryRef valuesToPut, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);
    void (*registerKeys)(SOSCloudTransportRef transport, CFArrayRef keysToGet, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock, CloudItemsChangedBlock notificationBlock);
    bool (*updateKeys)(SOSCloudTransportRef transport, bool getNewKeysOnly, CFArrayRef alwaysKeys, CFArrayRef afterFirstUnlockKeys, CFArrayRef unlockedKeys, CFErrorRef *error);

    void (*unregisterKeys)(SOSCloudTransportRef transport, CFArrayRef keysToUnregister, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);

    // Debug calls
    void (*get)(SOSCloudTransportRef transport, CFArrayRef keysToGet, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);
    void (*getAll)(SOSCloudTransportRef transport, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);
    void (*synchronize)(SOSCloudTransportRef transport, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);
    void (*synchronizeAndWait)(SOSCloudTransportRef transport, CFArrayRef keysToGet, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);

    void (*clearAll)(SOSCloudTransportRef transport, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);
    void (*removeObjectForKey)(SOSCloudTransportRef transport, CFStringRef keyToRemove, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);
    void (*localNotification)(SOSCloudTransportRef transport, CFStringRef messageToUser, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);
    void (*setParams)(SOSCloudTransportRef transport, CFDictionaryRef paramsDict, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);
    void (*requestSyncWithAllPeers)(SOSCloudTransportRef transport, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);

    const void *itemsChangedBlock;
};

/* Call this function before calling any other function in this header to provide
   an alternate transport, the default transport talks to CloudKeychainProxy via xpc. */
void SOSCloudKeychainSetTransport(SOSCloudTransportRef transport);


/*!
    @function SOSCloudKeychainRegisterKeysAndGet
    @abstract Register a set of keys-of-interest and optionally return their current values
    @param keysToGet An array of CFStringRef keys to get/register
    @param processQueue The replyBlock will be called via dispatch_async on this queue
    @param replyBlock This will be called via dispatch_async
    @discussion The replyBlock will be called asynchronously with the current values of
    the registered keys. If an error occured, the error parameter to the replyBlock will
    be filled in with an error. The caller should call CFRetain on the returned dictionary.
 */
void SOSCloudKeychainRegisterKeysAndGet(CFArrayRef keysToGet, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock, CloudItemsChangedBlock notificationBlock);
bool SOSCloudKeychainUpdateKeys(bool getNewKeysOnly,
                                CFArrayRef alwaysKeys,
                                CFArrayRef afterFirstUnlockKeys,
                                CFArrayRef unlockedKeys,
                                CFErrorRef *error);
void SOSCloudKeychainUnRegisterKeys(CFArrayRef keysToUnregister, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);

void SOSCloudKeychainPutObjectsInCloud(CFDictionaryRef objects, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);

void SOSCloudKeychainSetItemsChangedBlock(CloudItemsChangedBlock itemsChangedBlock);

void SOSCloudKeychainUserNotification(CFStringRef messageToUser, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);

void SOSCloudKeychainHandleUpdate(CFDictionaryRef update);
void SOSCloudKeychainSynchronizeAndWait(CFArrayRef keysToGet, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);

// Debug only?

void SOSCloudKeychainGetObjectsFromCloud(CFArrayRef keysToGet, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);
void SOSCloudKeychainSynchronize(dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);
void SOSCloudKeychainClearAll(dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);
void SOSCloudKeychainRemoveObjectForKey(CFStringRef keyToRemove, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);
void SOSCloudKeychainGetAllObjectsFromCloud(dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);

void SOSCloudKeychainSetParams(CFDictionaryRef paramsDict, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);
void SOSCloudKeychainRequestSyncWithAllPeers(dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);

void SOSCloudKeychainSetCallbackMethodXPC(void);

__END_DECLS

#endif /* !_SOSCLOUDKEYCHAINCLIENT_H_ */

