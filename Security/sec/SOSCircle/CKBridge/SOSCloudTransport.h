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
 * SOSCloudTransport.h -  Implementation of the transport layer from CKBridge to SOSAccount/SOSCircle
 */

/*!
    @header SOSCloudTransport
    The functions provided in SOSCloudTransport.h provide an interface
    from CKBridge to SOSAccount/SOSCircle
 */

#ifndef _SOSCLOUDTRANSPORT_H_
#define _SOSCLOUDTRANSPORT_H_

#include <dispatch/dispatch.h>

#include "SOSCloudKeychainClient.h"

__BEGIN_DECLS

/* CKPTransport. */

/* CKPTransport protocol (not opaque). */
typedef struct CloudTransport *SOSCloudTransportRef;
struct CloudTransport
{
    void (*put)(SOSCloudTransportRef transport, CFDictionaryRef valuesToPut, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);
    
    void (*registerKeys)(SOSCloudTransportRef transport, CFArrayRef keysToGet, CFStringRef clientIdentifier, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);
    void (*unregisterKeys)(SOSCloudTransportRef transport, CFArrayRef keysToUnregister, CFStringRef clientIdentifier, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);
    void (*setItemsChangedBlock)(SOSCloudTransportRef transport, CloudKeychainReplyBlock icb);

    // Debug calls
    void (*get)(SOSCloudTransportRef transport, CFArrayRef keysToGet, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);
    void (*getAll)(SOSCloudTransportRef transport, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock); 
    void (*synchronize)(SOSCloudTransportRef transport, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);
    void (*clearAll)(SOSCloudTransportRef transport, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);
};

/* Return the singleton cloud transport instance. */
/* pass NULL for kvsID to use real KVS */
SOSCloudTransportRef SOSCloudTransportDefaultTransport(CFStringRef kvsID, uint32_t options);

__END_DECLS

#endif /* !_SOSCLOUDTRANSPORT_H_ */
