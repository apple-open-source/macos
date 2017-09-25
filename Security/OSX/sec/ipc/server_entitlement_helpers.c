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

#include "server_entitlement_helpers.h"

#include <Security/SecTask.h>
#include <Security/SecTaskPriv.h>
#include <ipc/securityd_client.h>
#include <Security/SecEntitlements.h>
#include <Security/SecItem.h>
#include <utilities/SecCFRelease.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/debugging.h>

CFStringRef SecTaskCopyStringForEntitlement(SecTaskRef task,
                                            CFStringRef entitlement)
{
    CFStringRef value = (CFStringRef)SecTaskCopyValueForEntitlement(task,
                                                                    entitlement, NULL);
    if (value && CFGetTypeID(value) != CFStringGetTypeID()) {
        CFRelease(value);
        value = NULL;
    }

    return value;
}

CFArrayRef SecTaskCopyArrayOfStringsForEntitlement(SecTaskRef task,
                                                   CFStringRef entitlement)
{
    CFArrayRef value = (CFArrayRef)SecTaskCopyValueForEntitlement(task,
                                                                  entitlement, NULL);
    if (value) {
        if (CFGetTypeID(value) == CFArrayGetTypeID()) {
            CFIndex ix, count = CFArrayGetCount(value);
            for (ix = 0; ix < count; ++ix) {
                CFStringRef string = (CFStringRef)CFArrayGetValueAtIndex(value, ix);
                if (CFGetTypeID(string) != CFStringGetTypeID()) {
                    CFRelease(value);
                    value = NULL;
                    break;
                }
            }
        } else {
            CFRelease(value);
            value = NULL;
        }
    }

    return value;
}

CFStringRef SecTaskCopyApplicationIdentifier(SecTaskRef task) {
    return SecTaskCopyStringForEntitlement(task,
                                           kSecEntitlementApplicationIdentifier);
}
#if TARGET_OS_IOS
CFArrayRef SecTaskCopySharedWebCredentialDomains(SecTaskRef task) {
    return SecTaskCopyArrayOfStringsForEntitlement(task,
                                                   kSecEntitlementAssociatedDomains);
}
#endif
CFArrayRef SecTaskCopyAccessGroups(SecTaskRef task) {
    CFMutableArrayRef groups = NULL;
    CFArrayRef keychainAccessGroups = SecTaskCopyArrayOfStringsForEntitlement(task,
                                                                              kSecEntitlementKeychainAccessGroups);
    CFArrayRef appleSecurityApplicationGroups = SecTaskCopyArrayOfStringsForEntitlement(task,
                                                                                        kSecEntitlementAppleSecurityApplicationGroups);
    CFStringRef appID = SecTaskCopyApplicationIdentifier(task);
    CFIndex kagLen = keychainAccessGroups ? CFArrayGetCount(keychainAccessGroups) : 0;
    CFIndex asagLen = appleSecurityApplicationGroups ? CFArrayGetCount(appleSecurityApplicationGroups) : 0;
    bool entitlementsValidated = true;
    bool hasEntitlements = (kagLen + asagLen + (appID ? 1 : 0)) > 0;
#if TARGET_OS_OSX
    entitlementsValidated = SecTaskEntitlementsValidated(task);
    if ((appID || asagLen) && !entitlementsValidated) {
        CFReleaseNull(appID);
        asagLen = 0;
    }
#endif
    CFIndex len = kagLen + asagLen + (appID ? 1 : 0);
    // Always allow access to com.apple.token access group, unless entitlement validation explicitly failed.
    CFIndex tokenLen = (!hasEntitlements || entitlementsValidated) ? 1 : 0;
#if TARGET_OS_IPHONE
    if (len + tokenLen)
#endif
    {
        groups = CFArrayCreateMutable(kCFAllocatorDefault, len + tokenLen, &kCFTypeArrayCallBacks);
        if (kagLen)
            CFArrayAppendArray(groups, keychainAccessGroups, CFRangeMake(0, kagLen));
        if (appID)
            CFArrayAppendValue(groups, appID);
        if (asagLen)
            CFArrayAppendArray(groups, appleSecurityApplicationGroups, CFRangeMake(0, asagLen));
        if (tokenLen)
            CFArrayAppendValue(groups, kSecAttrAccessGroupToken);
#if TARGET_IPHONE_SIMULATOR
    } else {
        secwarning("No keychain access group specified while running in simulator, falling back to default set");
        groups = (CFMutableArrayRef)CFRetainSafe(SecAccessGroupsGetCurrent());
#endif
    }

    CFReleaseSafe(appID);
    CFReleaseSafe(keychainAccessGroups);
    CFReleaseSafe(appleSecurityApplicationGroups);
    return groups;
}

#if TARGET_OS_IPHONE
pthread_key_t taskThreadKey;
void secTaskDiagnoseEntitlements(CFArrayRef accessGroups) {
    SecTaskRef taskRef = pthread_getspecific(taskThreadKey);
    if (taskRef == NULL)
        return;

    CFErrorRef error = NULL;
    CFArrayRef entitlementNames = CFArrayCreateForCFTypes(NULL,
                                                          kSecEntitlementApplicationIdentifier,
                                                          kSecEntitlementKeychainAccessGroups,
                                                          kSecEntitlementAppleSecurityApplicationGroups,
                                                          NULL);
    CFDictionaryRef rawEntitlements = SecTaskCopyValuesForEntitlements(taskRef, entitlementNames, &error);
    CFReleaseNull(entitlementNames);

    // exclude some error types because they're accounted-for and not the reason we're here
    if (rawEntitlements == NULL && error) {
        CFErrorDomain domain = CFErrorGetDomain(error);
        if (domain && CFEqual(domain, kCFErrorDomainPOSIX)) {
            CFIndex c = CFErrorGetCode(error);
            int err = (int) c;

            switch (err) {
                case ESRCH:     // no such process (bad pid or process died)
                    return;
                default:
                    break;
            }
        }
    }

    uint32_t cs_flags = SecTaskGetCodeSignStatus(taskRef);
    CFStringRef identifier = SecTaskCopySigningIdentifier(taskRef, NULL);
    CFStringRef message = NULL;

    if (rawEntitlements == NULL) {    // NULL indicates failure-to-fetch (SecTask entitlements not initialized)
        message = CFStringCreateWithFormat(NULL, NULL, CFSTR("failed to fetch keychain client entitlements. task=%@ procid=%@ cs_flags=0x%08.8x error=%@"),
                                           taskRef, identifier, cs_flags, error);
        secerror("MISSING keychain entitlements: retrieve-entitlements error %@", error);
    } else {
        // non-NULL entitlement return => SecTaskCopyEntitlements succeeeded, no error
        // but note that kernel EINVAL => no entitlements, no error to deal with unsigned code
        message = CFStringCreateWithFormat(NULL, NULL, CFSTR("found no keychain client entitlements. task=%@ procid=%@ cs_flags=0x%08.8x"),
                                           taskRef, identifier, cs_flags);
        secerror("MISSING keychain entitlements: raw entitlement values: %@", rawEntitlements);
        secerror("MISSING keychain entitlements: original ag: %@", accessGroups);
        CFArrayRef newAccessGroups = SecTaskCopyAccessGroups(taskRef);
        secerror("MISSING keychain entitlements: newly parsed ag: %@", newAccessGroups);
        CFReleaseNull(newAccessGroups);
    }
    char buffer[1000] = "?";
    CFStringGetCString(message, buffer, sizeof(buffer), kCFStringEncodingUTF8);
    secerror("%s", buffer);
    __security_simulatecrash(message, __sec_exception_code_MissingEntitlements);

    CFReleaseNull(rawEntitlements);
    CFReleaseNull(message);
    CFReleaseNull(identifier);
    CFReleaseNull(error);
}
#endif

bool SecTaskGetBooleanValueForEntitlement(SecTaskRef task,
                                          CFStringRef entitlement) {
    CFTypeRef canModify = SecTaskCopyValueForEntitlement(task, entitlement, NULL);
    if (!canModify)
        return false;
    CFTypeID canModifyType = CFGetTypeID(canModify);
    bool ok = (CFBooleanGetTypeID() == canModifyType) && CFBooleanGetValue((CFBooleanRef)canModify);
    CFRelease(canModify);
    return ok;
}

