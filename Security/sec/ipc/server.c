/*
 * Copyright (c) 2007-2013 Apple Inc.  All Rights Reserved.
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

#include <mach/mach.h>
#include <mach/message.h>

#include <stdlib.h>
#include <sys/queue.h>

#include <Security/SecInternal.h>
#include <Security/SecBasePriv.h>
#include <Security/SecItemPriv.h> /* For SecItemDeleteAll */
#include <Security/SecCertificatePriv.h>
#include <Security/SecPolicyInternal.h>
#include <CoreFoundation/CoreFoundation.h>

#include <asl.h>
#include <syslog.h>
#include <bsm/libbsm.h>
#include <utilities/SecIOFormat.h>
#include <utilities/debugging.h>

#include "securityd_client.h"

#include <securityd/SecItemServer.h>
#include <securityd/SecTrustServer.h>
#include <securityd/SecTrustStoreServer.h>
#include <securityd/spi.h>
#include <Security/SecTask.h>

#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFError.h>
#include <utilities/SecXPCError.h>

// TODO: Make this include work on both platforms.
#if TARGET_OS_EMBEDDED
#include <Security/SecEntitlements.h>
#else
#define kSecEntitlementKeychainSyncUpdates CFSTR("keychain-sync-updates")
#define kSecEntitlementKeychainCloudCircle CFSTR("keychain-cloud-circle")
/* defines from <Security/SecTask.h> */
/* defines from <Security/SecEntitlements.h> */
#if TARGET_OS_EMBEDDED
#define kSecEntitlementApplicationIdentifier CFSTR("application-identifier")
#else
#define kSecEntitlementApplicationIdentifier CFSTR("com.apple.application-identifier")
#endif
#define kSecEntitlementGetTaskAllow CFSTR("get-task-allow")
#define kSecEntitlementKeychainAccessGroups CFSTR("keychain-access-groups")
#define kSecEntitlementModifyAnchorCertificates CFSTR("modify-anchor-certificates")
#define kSecEntitlementDebugApplications CFSTR("com.apple.springboard.debugapplications")
#define kSecEntitlementOpenSensitiveURL CFSTR("com.apple.springboard.opensensitiveurl")
#define kSecEntitlementWipeDevice CFSTR("com.apple.springboard.wipedevice")
#define kSecEntitlementRemoteNotificationConfigure CFSTR("com.apple.remotenotification.configure")
#define kSecEntitlementMigrateKeychain CFSTR("migrate-keychain")
#define kSecEntitlementRestoreKeychain CFSTR("restore-keychain")
#define kSecEntitlementSyncKeychain CFSTR("sync-keychain")
#endif

#include <Security/SecuritydXPC.h>

#include <libkern/OSAtomic.h>

#include <CoreFoundation/CFXPCBridge.h>

#include <xpc/xpc.h>
#include <xpc/private.h>
#include <xpc/connection_private.h>
#include <AssertMacros.h>
#include <SecureObjectSync/SOSCloudCircle.h>

#include <securityd/SOSCloudCircleServer.h>
#include <SecureObjectSync/SOSCloudCircleInternal.h>
#include <sys/sysctl.h>

// For SecError
#include <utilities/SecDb.h>

#include <securityd/OTATrustUtilities.h>


#if TARGET_IPHONE_SIMULATOR
#define CHECK_ENTITLEMENTS 0
#else
#define CHECK_ENTITLEMENTS 1
#endif

#if CHECK_ENTITLEMENTS
static CFStringRef SecTaskCopyStringForEntitlement(SecTaskRef task,
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

static CFArrayRef SecTaskCopyArrayOfStringsForEntitlement(SecTaskRef task,
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

#endif /* CHECK_ENTITLEMENTS */

static CFArrayRef SecTaskCopyAccessGroups(SecTaskRef task) {
#if CHECK_ENTITLEMENTS
    CFStringRef appID = SecTaskCopyStringForEntitlement(task,
        kSecEntitlementApplicationIdentifier);
    CFArrayRef groups = SecTaskCopyArrayOfStringsForEntitlement(task,
        kSecEntitlementKeychainAccessGroups);
    if (appID) {
        if (groups) {
            CFMutableArrayRef nGroups = CFArrayCreateMutableCopy(
                CFGetAllocator(groups), CFArrayGetCount(groups) + 1, groups);
            CFArrayAppendValue(nGroups, appID);
            CFRelease(groups);
            groups = nGroups;
        } else {
            groups = CFArrayCreate(CFGetAllocator(task),
                (const void **)&appID, 1, &kCFTypeArrayCallBacks);
        }
        CFRelease(appID);
    }
#else
    CFArrayRef groups = SecAccessGroupsGetCurrent();
    if (groups)
        CFRetain(groups);
#endif
    return groups;
}

static bool SecTaskGetBooleanValueForEntitlement(SecTaskRef task,
    CFStringRef entitlement) {
#if CHECK_ENTITLEMENTS
    CFStringRef canModify = (CFStringRef)SecTaskCopyValueForEntitlement(task,
        entitlement, NULL);
    if (!canModify)
        return false;
    CFTypeID canModifyType = CFGetTypeID(canModify);
    bool ok = (CFBooleanGetTypeID() == canModifyType) && CFBooleanGetValue((CFBooleanRef)canModify);
    CFRelease(canModify);
    return ok;
#else
    return true;
#endif /* !CHECK_ENTITLEMENTS */
}

static void with_label_and_password(xpc_object_t message, void (^action)(CFStringRef label, CFDataRef password)) {
    const char *label_utf8 = xpc_dictionary_get_string(message, kSecXPCKeyUserLabel);

    size_t password_length = 0;
    const void *password_data = xpc_dictionary_get_data(message, kSecXPCKeyUserPassword, &password_length);

    CFDataRef user_password = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, password_data, password_length, kCFAllocatorNull);
    CFStringRef user_label = CFStringCreateWithCString(kCFAllocatorDefault, label_utf8, kCFStringEncodingUTF8);

    action(user_label, user_password);

    CFReleaseNull(user_password);
    CFReleaseNull(user_label);
}

static bool SecXPCDictionarySetChainOptional(xpc_object_t message, const char *key, SecCertificatePathRef path, CFErrorRef *error) {
    if (!path)
        return true;
    xpc_object_t xpc_chain = SecCertificatePathCopyXPCArray(path, error);
    if (!xpc_chain)
        return false;

    xpc_dictionary_set_value(message, key, xpc_chain);
    xpc_release(xpc_chain);
    return true;
}

static SecCertificateRef SecXPCDictionaryCopyCertificate(xpc_object_t message, const char *key, CFErrorRef *error) {
    size_t length = 0;
    const void *bytes = xpc_dictionary_get_data(message, key, &length);
    if (bytes) {
        SecCertificateRef certificate = SecCertificateCreateWithBytes(kCFAllocatorDefault, bytes, length);
        if (certificate)
            return certificate;
        SecError(errSecDecode, error, CFSTR("object for key %s failed to create certificate from data"), key);
    } else {
        SecError(errSecParam, error, CFSTR("object for key %s missing"), key);
    }
    return NULL;
}

static bool SecXPCDictionaryCopyCertificates(xpc_object_t message, const char *key, CFArrayRef *certificates, CFErrorRef *error) {
    xpc_object_t xpc_certificates = xpc_dictionary_get_value(message, key);
    if (!xpc_certificates)
        return SecError(errSecAllocate, error, CFSTR("no certs for key %s"), key);
    *certificates = SecCertificateXPCArrayCopyArray(xpc_certificates, error);
    return *certificates;
}

static bool SecXPCDictionaryCopyCertificatesOptional(xpc_object_t message, const char *key, CFArrayRef *certificates, CFErrorRef *error) {
    xpc_object_t xpc_certificates = xpc_dictionary_get_value(message, key);
    if (!xpc_certificates) {
        *certificates = NULL;
        return true;
    }
    *certificates = SecCertificateXPCArrayCopyArray(xpc_certificates, error);
    return *certificates;
}

static bool SecXPCDictionaryCopyPoliciesOptional(xpc_object_t message, const char *key, CFArrayRef *policies, CFErrorRef *error) {
    xpc_object_t xpc_policies = xpc_dictionary_get_value(message, key);
    if (!xpc_policies) {
        if (policies)
            *policies = NULL;
        return true;
    }
    *policies = SecPolicyXPCArrayCopyArray(xpc_policies, error);
    return *policies != NULL;
}

static SecTrustStoreRef SecXPCDictionaryGetTrustStore(xpc_object_t message, const char *key, CFErrorRef *error) {
    SecTrustStoreRef ts = NULL;
    CFStringRef domain = SecXPCDictionaryCopyString(message, key, error);
    if (domain) {
        ts = SecTrustStoreForDomainName(domain, error);
        CFRelease(domain);
    }
    return ts;
}

static bool SecXPCDictionaryGetDouble(xpc_object_t message, const char *key, double *pvalue, CFErrorRef *error) {
    *pvalue = xpc_dictionary_get_double(message, key);
    if (*pvalue == NAN) {
        return SecError(errSecParam, error, CFSTR("object for key %s bad double"), key);
    }
    return true;
}

static void securityd_xpc_dictionary_handler(const xpc_connection_t connection, xpc_object_t event) {
	xpc_type_t type = xpc_get_type(event);
    __block CFErrorRef error = NULL;
    xpc_object_t xpcError = NULL;
    xpc_object_t replyMessage = NULL;
    SecTaskRef clientTask = NULL;
    CFArrayRef accessGroups = NULL;

    secdebug("serverxpc", "entering");
    if (type == XPC_TYPE_DICTIONARY) {
        // TODO: Find out what we're dispatching.
        replyMessage = xpc_dictionary_create_reply(event);

        uint64_t operation = xpc_dictionary_get_uint64(event, kSecXPCKeyOperation);
		secdebug("serverxpc", "operation: %@ (%" PRIu64 ")", SOSCCGetOperationDescription((enum SecXPCOperation)operation), operation);

        bool hasEntitlement;
#if 1 // CHECK_ENTITLEMENTS
        audit_token_t auditToken = {};
        xpc_connection_get_audit_token(connection, &auditToken);
        clientTask = SecTaskCreateWithAuditToken(kCFAllocatorDefault, auditToken);
        accessGroups = SecTaskCopyAccessGroups(clientTask);

        // operations before kSecXPCOpTryUserCredentials don't need this entitlement.
        hasEntitlement = (operation < kSecXPCOpTryUserCredentials) ||
            (clientTask && SecTaskGetBooleanValueForEntitlement(clientTask, kSecEntitlementKeychainCloudCircle));
#else
        clientTask = NULL;
        hasEntitlement = true;
#endif

         // Per <rdar://problem/13315020> Disable the entitlement check for "keychain-cloud-circle"
         //  we disable entitlement enforcement. However, we still log so we know who needs the entitlement

         if (!hasEntitlement) {
            CFErrorRef entitlementError = NULL;
            SecError(errSecMissingEntitlement, &entitlementError, CFSTR("%@: %@ lacks entitlement %@"), SOSCCGetOperationDescription((enum SecXPCOperation)operation), clientTask, kSecEntitlementKeychainCloudCircle);
            secnotice("serverxpc", "MissingEntitlement: %@", entitlementError);
            CFReleaseSafe(entitlementError);
        }

        if (true) {
            switch (operation)
            {
            case sec_item_add_id:
            {
                CFDictionaryRef query = SecXPCDictionaryCopyDictionary(event, kSecXPCKeyQuery, &error);
                if (query) {
                    CFTypeRef result = NULL;
                    if (_SecItemAdd(query, accessGroups, &result, &error) && result) {
                        SecXPCDictionarySetPList(replyMessage, kSecXPCKeyResult, result, &error);
                        CFRelease(result);
                    }
                    CFRelease(query);
                }
                break;
            }
            case sec_item_copy_matching_id:
            {
                CFDictionaryRef query = SecXPCDictionaryCopyDictionary(event, kSecXPCKeyQuery, &error);
                if (query) {
                    CFTypeRef result = NULL;
                    if (_SecItemCopyMatching(query, accessGroups, &result, &error) && result) {
                        SecXPCDictionarySetPList(replyMessage, kSecXPCKeyResult, result, &error);
                        CFRelease(result);
                    }
                    CFRelease(query);
                }
                break;
            }
            case sec_item_update_id:
            {
                CFDictionaryRef query = SecXPCDictionaryCopyDictionary(event, kSecXPCKeyQuery, &error);
                if (query) {
                    CFDictionaryRef attributesToUpdate = SecXPCDictionaryCopyDictionary(event, kSecXPCKeyAttributesToUpdate, &error);
                    if (attributesToUpdate) {
                        bool result = _SecItemUpdate(query, attributesToUpdate, accessGroups, &error);
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, result);
                        CFRelease(attributesToUpdate);
                    }
                    CFRelease(query);
                }
                break;
            }
            case sec_item_delete_id:
            {
                CFDictionaryRef query = SecXPCDictionaryCopyDictionary(event, kSecXPCKeyQuery, &error);
                if (query) {
                    bool result = _SecItemDelete(query, accessGroups, &error);
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, result);
                    CFRelease(query);
                }
                break;
            }
            case sec_trust_store_contains_id:
            {
                SecTrustStoreRef ts = SecXPCDictionaryGetTrustStore(event, kSecXPCKeyDomain, &error);
                if (ts) {
                    CFDataRef digest = SecXPCDictionaryCopyData(event, kSecXPCKeyDigest, &error);
                    if (digest) {
                        bool contains;
                        if (SecTrustStoreContainsCertificateWithDigest(ts, digest, &contains, &error))
                            xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, contains);
                        CFRelease(digest);
                    }
                }
                break;
            }
            case sec_trust_store_set_trust_settings_id:
            {
                SecTrustStoreRef ts = SecXPCDictionaryGetTrustStore(event, kSecXPCKeyDomain, &error);
                if (ts) {
                    SecCertificateRef certificate = SecXPCDictionaryCopyCertificate(event, kSecXPCKeyCertificate, &error);
                    if (certificate) {
                        CFTypeRef trustSettingsDictOrArray = NULL;
                        if (SecXPCDictionaryCopyPListOptional(event, kSecXPCKeySettings, &trustSettingsDictOrArray, &error)) {
                            bool result = _SecTrustStoreSetTrustSettings(ts, certificate, trustSettingsDictOrArray, &error);
                            xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, result);
                            CFReleaseSafe(trustSettingsDictOrArray);
                        }
                        CFRelease(certificate);
                    }
                }
                break;
            }
            case sec_trust_store_remove_certificate_id:
            {
                SecTrustStoreRef ts = SecXPCDictionaryGetTrustStore(event, kSecXPCKeyDomain, &error);
                if (ts) {
                    CFDataRef digest = SecXPCDictionaryCopyData(event, kSecXPCKeyDigest, &error);
                    if (digest) {
                        bool result = SecTrustStoreRemoveCertificateWithDigest(ts, digest, &error);
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, result);
                        CFRelease(digest);
                    }
                }
                break;
            }
            case sec_delete_all_id:
                xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, _SecItemDeleteAll(&error));
                break;
            case sec_trust_evaluate_id:
            {
                CFArrayRef certificates = NULL, anchors = NULL, policies = NULL;
                bool anchorsOnly = xpc_dictionary_get_bool(event, kSecTrustAnchorsOnlyKey);
                double verifyTime;
                if (SecXPCDictionaryCopyCertificates(event, kSecTrustCertificatesKey, &certificates, &error) &&
                    SecXPCDictionaryCopyCertificatesOptional(event, kSecTrustAnchorsKey, &anchors, &error) &&
                    SecXPCDictionaryCopyPoliciesOptional(event, kSecTrustPoliciesKey, &policies, &error) &&
                    SecXPCDictionaryGetDouble(event, kSecTrustVerifyDateKey, &verifyTime, &error)) {
                    // If we have no error yet, capture connection and reply in block and properly retain them.
                    xpc_retain(connection);
                    CFRetainSafe(clientTask);

                    // Clear replyMessage so we don't send a synchronous reply.
                    xpc_object_t asyncReply = replyMessage;
                    replyMessage = NULL;

                    SecTrustServerEvaluateBlock(certificates, anchors, anchorsOnly, policies, verifyTime, accessGroups, ^(SecTrustResultType tr, CFArrayRef details, CFDictionaryRef info, SecCertificatePathRef chain, CFErrorRef replyError) {
                        // Send back reply now
                        if (replyError) {
                            CFRetain(replyError);
                        } else {
                            xpc_dictionary_set_int64(asyncReply, kSecTrustResultKey, tr);
                            SecXPCDictionarySetPListOptional(asyncReply, kSecTrustDetailsKey, details, &replyError) &&
                            SecXPCDictionarySetPListOptional(asyncReply, kSecTrustInfoKey, info, &replyError) &&
                            SecXPCDictionarySetChainOptional(asyncReply, kSecTrustChainKey, chain, &replyError);
                        }
                        if (replyError) {
                            secdebug("ipc", "%@ %@ %@", clientTask, SOSCCGetOperationDescription((enum SecXPCOperation)operation), replyError);
                            xpc_object_t xpcReplyError = SecCreateXPCObjectWithCFError(replyError);
                            if (xpcReplyError) {
                                xpc_dictionary_set_value(asyncReply, kSecXPCKeyError, xpcReplyError);
                                xpc_release(xpcReplyError);
                            }
                            CFRelease(replyError);
                        } else {
                            secdebug("ipc", "%@ %@ reponding %@", clientTask, SOSCCGetOperationDescription((enum SecXPCOperation)operation), asyncReply);
                        }

                        xpc_connection_send_message(connection, asyncReply);
                        xpc_release(asyncReply);
                        xpc_release(connection);
                        CFReleaseSafe(clientTask);
                    });
                }
                CFReleaseSafe(policies);
                CFReleaseSafe(anchors);
                CFReleaseSafe(certificates);
                break;
            }
            case sec_keychain_backup_id:
            {
                CFDataRef keybag = NULL, passcode = NULL;
                if (SecXPCDictionaryCopyDataOptional(event, kSecXPCKeyKeybag, &keybag, &error)) {
                    if (SecXPCDictionaryCopyDataOptional(event, kSecXPCKeyUserPassword, &passcode, &error)) {
                        CFDataRef backup = _SecServerKeychainBackup(keybag, passcode, &error);
                        if (backup) {
                            SecXPCDictionarySetData(replyMessage, kSecXPCKeyResult, backup, &error);
                            CFRelease(backup);
                        }
                        CFReleaseSafe(passcode);
                    }
                    CFReleaseSafe(keybag);
                }
                break;
            }
            case sec_keychain_restore_id:
            {
                CFDataRef backup = SecXPCDictionaryCopyData(event, kSecXPCKeyBackup, &error);
                if (backup) {
                    CFDataRef keybag = SecXPCDictionaryCopyData(event, kSecXPCKeyKeybag, &error);
                    if (keybag) {
                        CFDataRef passcode = NULL;
                        if (SecXPCDictionaryCopyDataOptional(event, kSecXPCKeyUserPassword, &passcode, &error)) {
                            bool result = _SecServerKeychainRestore(backup, keybag, passcode, &error);
                            xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, result);
                            CFReleaseSafe(passcode);
                        }
                        CFRelease(keybag);
                    }
                    CFRelease(backup);
                }
                break;
            }
            case sec_keychain_sync_update_id:
            {
                CFDictionaryRef updates = SecXPCDictionaryCopyDictionary(event, kSecXPCKeyQuery, &error);
                if (updates) {
                    bool result = _SecServerKeychainSyncUpdate(updates, &error);
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, result);
                    CFRelease(updates);
                }
                break;
            }
            case sec_keychain_backup_syncable_id:
            {
                CFDictionaryRef oldbackup = NULL;
                if (SecXPCDictionaryCopyDictionaryOptional(event, kSecXPCKeyBackup, &oldbackup, &error)) {
                    CFDataRef keybag = SecXPCDictionaryCopyData(event, kSecXPCKeyKeybag, &error);
                    if (keybag) {
                        CFDataRef passcode = NULL;
                        if (SecXPCDictionaryCopyDataOptional(event, kSecXPCKeyUserPassword, &passcode, &error)) {
                            CFDictionaryRef newbackup = _SecServerBackupSyncable(oldbackup, keybag, passcode, &error);
                            if (newbackup) {
                                SecXPCDictionarySetPList(replyMessage, kSecXPCKeyResult, newbackup, &error);
                                CFRelease(newbackup);
                            }
                            CFReleaseSafe(passcode);
                        }
                        CFRelease(keybag);
                    }
                    CFReleaseSafe(oldbackup);
                }
                break;
            }
            case sec_keychain_restore_syncable_id:
            {
                CFDictionaryRef backup = SecXPCDictionaryCopyDictionary(event, kSecXPCKeyBackup, &error);
                if (backup) {
                    CFDataRef keybag = SecXPCDictionaryCopyData(event, kSecXPCKeyKeybag, &error);
                    if (keybag) {
                        CFDataRef passcode = NULL;
                        if (SecXPCDictionaryCopyDataOptional(event, kSecXPCKeyUserPassword, &passcode, &error)) {
                            bool result = _SecServerRestoreSyncable(backup, keybag, passcode, &error);
                            xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, result);
                            CFReleaseSafe(passcode);
                        }
                        CFRelease(keybag);
                    }
                    CFRelease(backup);
                }
                break;
            }
            case sec_ota_pki_asset_version_id:
                xpc_dictionary_set_int64(replyMessage, kSecXPCKeyResult,
                                         SecOTAPKIGetCurrentAssetVersion(&error));
                break;
            case kSecXPCOpTryUserCredentials:
                with_label_and_password(event, ^(CFStringRef label, CFDataRef password) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            SOSCCTryUserCredentials_Server(label, password, &error));
                });
                break;
            case kSecXPCOpSetUserCredentials:
                with_label_and_password(event, ^(CFStringRef label, CFDataRef password) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            SOSCCSetUserCredentials_Server(label, password, &error));
                });
                break;
            case kSecXPCOpCanAuthenticate:
                xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                        SOSCCCanAuthenticate_Server(&error));
                break;
            case kSecXPCOpPurgeUserCredentials:
                xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                        SOSCCPurgeUserCredentials_Server(&error));
                break;
            case kSecXPCOpDeviceInCircle:
                xpc_dictionary_set_int64(replyMessage, kSecXPCKeyResult,
                                         SOSCCThisDeviceIsInCircle_Server(&error));
                break;
            case kSecXPCOpRequestToJoin:
                xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                        SOSCCRequestToJoinCircle_Server(&error));
                break;
            case kSecXPCOpRequestToJoinAfterRestore:
                xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                        SOSCCRequestToJoinCircleAfterRestore_Server(&error));
                break;
            case kSecXPCOpResetToOffering:
                xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                        SOSCCResetToOffering_Server(&error));
                break;
            case kSecXPCOpResetToEmpty:
                xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                        SOSCCResetToEmpty_Server(&error));
                break;
            case kSecXPCOpRemoveThisDeviceFromCircle:
                xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                        SOSCCRemoveThisDeviceFromCircle_Server(&error));
                break;
            case kSecXPCOpBailFromCircle:
                {
                uint64_t limit_in_seconds = xpc_dictionary_get_uint64(event, kSecXPCLimitInMinutes);
                xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                        SOSCCBailFromCircle_Server(limit_in_seconds, &error));
                }
                break;
            case kSecXPCOpAcceptApplicants:
                {
                    xpc_object_t xapplicants = xpc_dictionary_get_value(event, kSecXPCKeyPeerInfos);
                    CFArrayRef applicants = CreateArrayOfPeerInfoWithXPCObject(xapplicants, &error); //(CFArrayRef)(_CFXPCCreateCFObjectFromXPCObject(xapplicants));
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            (applicants && SOSCCAcceptApplicants_Server(applicants, &error)));
                    CFReleaseSafe(applicants);
                }
                break;
            case kSecXPCOpRejectApplicants:
                {
                    xpc_object_t xapplicants = xpc_dictionary_get_value(event, kSecXPCKeyPeerInfos);
                    CFArrayRef applicants = CreateArrayOfPeerInfoWithXPCObject(xapplicants, &error); //(CFArrayRef)(_CFXPCCreateCFObjectFromXPCObject(xapplicants));
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            (applicants && SOSCCRejectApplicants_Server(applicants, &error)));
                    CFReleaseSafe(applicants);
                }
                break;
            case kSecXPCOpCopyApplicantPeerInfo:
                {
                    CFArrayRef array = SOSCCCopyApplicantPeerInfo_Server(&error);
                    if (array) {
                        xpc_object_t xpc_array = CreateXPCObjectWithArrayOfPeerInfo(array, &error);
                        xpc_dictionary_set_value(replyMessage, kSecXPCKeyResult, xpc_array);
                        xpc_release(xpc_array);
                    }
                    CFReleaseNull(array);
                }
                break;
            case kSecXPCOpCopyPeerPeerInfo:
                {
                    CFArrayRef array = SOSCCCopyPeerPeerInfo_Server(&error);
                    if (array) {
                        xpc_object_t xpc_array = CreateXPCObjectWithArrayOfPeerInfo(array, &error);
                        xpc_dictionary_set_value(replyMessage, kSecXPCKeyResult, xpc_array);
                        xpc_release(xpc_array);
                    }
                    CFReleaseNull(array);
                }
                break;
            case kSecXPCOpCopyConcurringPeerPeerInfo:
                {
                    CFArrayRef array = SOSCCCopyConcurringPeerPeerInfo_Server(&error);
                    if (array) {
                        xpc_object_t xpc_array = CreateXPCObjectWithArrayOfPeerInfo(array, &error);
                        xpc_dictionary_set_value(replyMessage, kSecXPCKeyResult, xpc_array);
                        xpc_release(xpc_array);
                    }
                    CFReleaseNull(array);
                }
                break;
            case kSecXPCOpGetLastDepartureReason:
                xpc_dictionary_set_int64(replyMessage, kSecXPCKeyResult,
                                         SOSCCGetLastDepartureReason_Server(&error));
                    break;
            case kSecXPCOpProcessSyncWithAllPeers:
                xpc_dictionary_set_int64(replyMessage, kSecXPCKeyResult,
                                         SOSCCProcessSyncWithAllPeers_Server(&error));
                    break;
            case kSecXPCOpCopyIncompatibilityInfo:
                xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                        SOSCCCopyIncompatibilityInfo_Server(&error));
                break;
			case kSecXPCOpOTAGetEscrowCertificates:
				{
					CFArrayRef array = SecOTAPKICopyCurrentEscrowCertificates(&error);
					if (array) {
						xpc_object_t xpc_array = _CFXPCCreateXPCObjectFromCFObject(array);
						xpc_dictionary_set_value(replyMessage, kSecXPCKeyResult, xpc_array);
	                    xpc_release(xpc_array);
					}
					CFReleaseNull(array);
				}
				break;
			case kSecXPCOpOTAPKIGetNewAsset:
				 xpc_dictionary_set_int64(replyMessage, kSecXPCKeyResult,
	                                         SecOTAPKISignalNewAsset(&error));
				break;
           default:
                break;
            }
        }

        if (error)
        {
            if(SecErrorGetOSStatus(error) == errSecItemNotFound)
                secdebug("ipc", "%@ %@ %@", clientTask, SOSCCGetOperationDescription((enum SecXPCOperation)operation), error);
            else
                secerror("%@ %@ %@", clientTask, SOSCCGetOperationDescription((enum SecXPCOperation)operation), error);

            xpcError = SecCreateXPCObjectWithCFError(error);
            xpc_dictionary_set_value(replyMessage, kSecXPCKeyError, xpcError);
        } else if (replyMessage) {
            secdebug("ipc", "%@ %@ reponding %@", clientTask, SOSCCGetOperationDescription((enum SecXPCOperation)operation), replyMessage);
        }
    } else {
        SecCFCreateErrorWithFormatAndArguments(kSecXPCErrorUnexpectedType, sSecXPCErrorDomain, NULL, &error, 0, CFSTR("Messages expect to be xpc dictionary, got: %@"), event);
        secerror("%@: returning error: %@", clientTask, error);
        xpcError = SecCreateXPCObjectWithCFError(error);
        replyMessage = xpc_create_reply_with_format(event, "{%string: %value}", kSecXPCKeyError, xpcError);
    }

    if (replyMessage) {
        xpc_connection_send_message(connection, replyMessage);
        xpc_release(replyMessage);
    }
    if (xpcError)
        xpc_release(xpcError);
    CFReleaseSafe(error);
    CFReleaseSafe(accessGroups);
    CFReleaseSafe(clientTask);
}

static void securityd_xpc_init()
{
    secdebug("serverxpc", "start");

    xpc_track_activity();
    xpc_connection_t listener = xpc_connection_create_mach_service(kSecuritydXPCServiceName, NULL, XPC_CONNECTION_MACH_SERVICE_LISTENER);
    if (!listener) {
        seccritical("security failed to register xpc listener, exiting");
        abort();
    }

    xpc_connection_set_event_handler(listener, ^(xpc_object_t connection) {
        if (xpc_get_type(connection) == XPC_TYPE_CONNECTION) {
            xpc_connection_set_event_handler(connection, ^(xpc_object_t event) {
                if (xpc_get_type(event) == XPC_TYPE_DICTIONARY) {
                    xpc_retain(connection);
                    xpc_retain(event);
                    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
                        securityd_xpc_dictionary_handler(connection, event);
                        xpc_release(event);
                        xpc_release(connection);
                    });
                }
            });
            xpc_connection_resume(connection);
        }
    });
    xpc_connection_resume(listener);
}

int main(int argc, char *argv[])
{
    char *wait4debugger = getenv("WAIT4DEBUGGER");
    if (wait4debugger && !strcasecmp("YES", wait4debugger)) {
		seccritical("SIGSTOPing self, awaiting debugger");
		kill(getpid(), SIGSTOP);
		asl_log(NULL, NULL, ASL_LEVEL_CRIT,
                "Again, for good luck (or bad debuggers)");
		kill(getpid(), SIGSTOP);
	}

    securityd_init_server();
    securityd_xpc_init();
    dispatch_main();
    return 0;
}

/* vi:set ts=4 sw=4 et: */
