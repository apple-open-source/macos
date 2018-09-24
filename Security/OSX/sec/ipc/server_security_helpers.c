/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#include <pthread/pthread.h>

#include "server_security_helpers.h"
#include "server_entitlement_helpers.h"

#include <Security/SecTask.h>
#include <Security/SecTaskPriv.h>
#include <ipc/securityd_client.h>
#include <Security/SecEntitlements.h>
#include <Security/SecItem.h>
#include <utilities/SecCFRelease.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/debugging.h>
#include <securityd/SecDbQuery.h>

#if __has_include(<MobileKeyBag/MobileKeyBag.h>) && TARGET_HAS_KEYSTORE
#include <MobileKeyBag/MobileKeyBag.h>
#define HAVE_MOBILE_KEYBAG_SUPPORT 1
#endif

#if HAVE_MOBILE_KEYBAG_SUPPORT && TARGET_OS_EMBEDDED
static bool
device_is_multiuser(void)
{
    static dispatch_once_t once;
    static bool result;

    dispatch_once(&once, ^{
        CFDictionaryRef deviceMode = MKBUserTypeDeviceMode(NULL, NULL);
        CFTypeRef value = NULL;

        if (deviceMode && CFDictionaryGetValueIfPresent(deviceMode, kMKBDeviceModeKey, &value) && CFEqual(value, kMKBDeviceModeMultiUser)) {
            result = true;
        }
        CFReleaseNull(deviceMode);
    });

    return result;
}
#endif /* HAVE_MOBILE_KEYBAG_SUPPORT && TARGET_OS_EMBEDDED */

void fill_security_client(SecurityClient * client, const uid_t uid, audit_token_t auditToken) {
    if(!client) {
        return;
    }

    client->uid = uid;

#if HAVE_MOBILE_KEYBAG_SUPPORT && TARGET_OS_EMBEDDED

    if (device_is_multiuser()) {
        CFErrorRef error = NULL;

        client->inMultiUser = true;
        client->activeUser = MKBForegroundUserSessionID(&error);
        if (client->activeUser == -1 || client->activeUser == 0) {
            assert(0);
            client->activeUser = 0;
        }

        /*
         * If we are a edu mode user, and its not the active user,
         * then the request is coming from inside the syncbubble.
         *
         * otherwise we are going to execute the request as the
         * active user.
         */

        if (client->uid > 501 && (uid_t)client->activeUser != client->uid) {
            secinfo("serverxpc", "securityd client: sync bubble user");
            client->musr = SecMUSRCreateSyncBubbleUserUUID(client->uid);
            client->keybag = KEYBAG_DEVICE;
        } else {
            secinfo("serverxpc", "securityd client: active user");
            client->musr = SecMUSRCreateActiveUserUUID(client->activeUser);
            client->uid = (uid_t)client->activeUser;
            client->keybag = KEYBAG_DEVICE;
        }
    }
#endif

    client->task = SecTaskCreateWithAuditToken(kCFAllocatorDefault, auditToken);

    client->accessGroups = SecTaskCopyAccessGroups(client->task);

#if TARGET_OS_IPHONE
    client->allowSystemKeychain = SecTaskGetBooleanValueForEntitlement(client->task, kSecEntitlementPrivateSystemKeychain);
    client->isNetworkExtension = SecTaskGetBooleanValueForEntitlement(client->task, kSecEntitlementPrivateNetworkExtension);
    client->canAccessNetworkExtensionAccessGroups = SecTaskGetBooleanValueForEntitlement(client->task, kSecEntitlementNetworkExtensionAccessGroups);
#endif
#if HAVE_MOBILE_KEYBAG_SUPPORT && TARGET_OS_EMBEDDED
    if (client->inMultiUser) {
        client->allowSyncBubbleKeychain = SecTaskGetBooleanValueForEntitlement(client->task, kSecEntitlementPrivateKeychainSyncBubble);
    }
#endif
}

