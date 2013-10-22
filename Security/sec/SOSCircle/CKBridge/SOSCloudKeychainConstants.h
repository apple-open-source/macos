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
    These constants are used by the XPC service and its clients.
*/

#ifndef	_CKDXPC_CONSTANTS_H_
#define _CKDXPC_CONSTANTS_H_

__BEGIN_DECLS

extern const char *xpcServiceName;

extern const char *kMessageKeyOperation;
extern const char *kMessageKeyKey;
extern const char *kMessageKeyValue;
extern const char *kMessageKeyError;
extern const char *kMessageKeyVersion;
extern const char *kMessageKeyGetNewKeysOnly;
extern const char *kMessageKeyKeysToGet;
extern const char *kMessageKeyKeysRequireFirstUnlock;
extern const char *kMessageKeyKeysRequiresUnlocked;
extern const char *kMessageOperationItemChanged;
extern const char *kOperationRemoveObjectForKey;
extern const char *kMessageKeyNotificationFlags;

extern const char *kOperationClearStore;
extern const char *kOperationSynchronize;
extern const char *kOperationSynchronizeAndWait;
extern const char *kOperationPUTDictionary;
extern const char *kOperationGETv2;
extern const char *kOperationRegisterKeysAndGet;
extern const char *kOperationUnregisterKeys;
extern const char *kMessageKeyClientIdentifier;

extern const uint64_t kCKDXPCVersion;

extern const char *kOperationUILocalNotification;
extern const char *kOperationSetParams;
extern const char *kOperationRequestSyncWithAllPeers;

extern const CFStringRef kParamCallbackMethod;
extern const CFStringRef kParamCallbackMethodSecurityd;
extern const CFStringRef kParamCallbackMethodXPC;

extern const char * const kCloudKeychainStorechangeChangeNotification;

extern const char *kNotifyTokenForceUpdate;

__END_DECLS

#endif	/* _CKDXPC_CONSTANTS_H_ */

