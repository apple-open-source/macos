/*
 * Copyright (c) 2012,2014 Apple Inc. All Rights Reserved.
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
//  ckdxpcclient.h
//  ckd-xpc
//

#include <xpc/xpc.h>
#include <utilities/debugging.h>
#include "SOSCloudKeychainClient.h"

#ifndef	_CKDXPC_CLIENT_H_
#define _CKDXPC_CLIENT_H_

//#ifndef __CKDXPC_CLIENT_PRIVATE_INDIRECT__
//#error "Please #include "SOSCloudKeychainClient.h" instead of this file directly."
//#endif /* __CKDXPC_CLIENT_PRIVATE_INDIRECT__ */

__BEGIN_DECLS

void initXPCConnection(void);
void closeXPCConnection(void);
void setItemsChangedBlock(CloudKeychainReplyBlock icb);

void putValuesWithXPC(CFDictionaryRef values, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);
void getValuesFromKVS(CFArrayRef keysToGet, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);

void registerKeysForKVS(CFArrayRef keysToGet, CFStringRef clientIdentifier, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);
void unregisterKeysForKVS(CFArrayRef keysToGet, CFStringRef clientIdentifier, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);

void synchronizeKVS(dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);
void clearAll(dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock);

// Debug

void clearStore(void);

__END_DECLS

#endif	/* _CKDXPC_CLIENT_H_ */

