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
    This XPC service is essentially just a proxy to iCloud KVS, which exists since
    the main security code cannot link against Foundation.
    
    See sendTSARequestWithXPC in tsaSupport.c for how to call the service
    
    The client of an XPC service does not get connection events, nor does it
    need to deal with transactions.
*/

//------------------------------------------------------------------------------------------------


#include <CoreFoundation/CoreFoundation.h>
#include "SOSCloudKeychainConstants.h"

const uint64_t kCKDXPCVersion = 1;

// seems like launchd looks for the BundleIdentifier, not the name
const char *xpcServiceName = "com.apple.security.cloudkeychainproxy3";   //"CloudKeychainProxy";

const char *kMessageKeyOperation = "operation";
const char *kMessageKeyKey = "key";
const char *kMessageKeyValue = "value";
const char *kMessageKeyError = "error";
const char *kMessageKeyVersion = "version";
const char *kMessageKeyGetNewKeysOnly = "GetNewKeysOnly";
const char *kMessageKeyKeysToGet = "KeysToGet";
const char *kMessageKeyKeysRequireFirstUnlock = "KeysRequireFirstUnlock";
const char *kMessageKeyKeysRequiresUnlocked = "KeysRequiresUnlocked";
const char *kMessageKeyNotificationFlags = "NotificationFlags";

/* parameters within the dictionary */
const char *kMessageAlwaysKeys = "AlwaysKeys";
const char *kMessageFirstUnlocked = "FirstUnlockKeys";
const char *kMessageUnlocked = "UnlockedKeys";

const char *kMessageContext = "Context";
const char *kMessageAllKeys = "AllKeys";
const char *kMessageKeyParameter = "KeyParameter";
const char *kMessageCircle = "Circle";
const char *kMessageMessage = "Message";

const char *kMessageOperationItemChanged = "ItemChanged";

const char *kOperationClearStore = "ClearStore";
const char *kOperationSynchronize = "Synchronize";
const char *kOperationSynchronizeAndWait = "SynchronizeAndWait";

const char *kOperationFlush = "Flush";

const char *kOperationPUTDictionary = "PUTDictionary";
const char *kOperationGETv2 = "GETv2";
const char *kOperationRemoveObjectForKey = "RemoveObjectForKey";

const char *kOperationRegisterKeys = "RegisterKeys";

const char *kOperationUILocalNotification = "UILocalNotification";

const char *kOperationRequestSyncWithAllPeers = "requestSyncWithAllPeers";
const char *kOperationRequestEnsurePeerRegistration = "requestEnsurePeerRegistration";


/*
    The values for the KVS notification and KVS Store ID must be identical to the values
    in syncdefaultsd (SYDApplication.m). The notification string is used in two places:
    it is in our launchd plist (com.apple.security.cloudkeychainproxy.plist) as the
    LaunchEvents/com.apple.notifyd.matching key and is examined in code in the stream event handler.

    The KVS Store ID (_SYDRemotePreferencesStoreIdentifierKey in SYDApplication.m) must
    be in the entitlements. The bundle identifier is (com.apple.security.cloudkeychainproxy3)
    is used by installInfoForBundleIdentifiers in SYDApplication.m and is used to look up our
    daemon to figure out what store to use, etc.
*/

const char * const kCloudKeychainStorechangeChangeNotification = "com.apple.security.cloudkeychainproxy.kvstorechange3"; // was "com.apple.security.cloudkeychain.kvstorechange" for seeds

const char *kNotifyTokenForceUpdate = "com.apple.security.cloudkeychain.forceupdate";
