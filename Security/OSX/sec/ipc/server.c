/*
 * Copyright (c) 2007-2017 Apple Inc.  All Rights Reserved.
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

#include <Security/SecureObjectSync/SOSPeerInfoDER.h>
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSCloudCircleInternal.h>
#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSPeerInfoCollections.h>
#include <Security/SecureObjectSync/SOSControlServer.h>
#include <Security/SecBase.h>
#include <Security/SecBasePriv.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecEntitlements.h>
#include <Security/SecInternal.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecPolicy.h>
#include <Security/SecPolicyInternal.h>
#include <Security/SecTask.h>
#include <Security/SecTrustInternal.h>
#include <Security/SecuritydXPC.h>
#include <securityd/OTATrustUtilities.h>
#include <securityd/SOSCloudCircleServer.h>
#include <securityd/SecItemBackupServer.h>
#include <securityd/SecItemServer.h>
#include <securityd/SecLogSettingsServer.h>
#include <securityd/SecOTRRemote.h>
#include <securityd/SecTrustServer.h>
#include <securityd/SecTrustStoreServer.h>
#include <securityd/iCloudTrace.h>
#include <securityd/spi.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecDb.h>
#include <utilities/SecIOFormat.h>
#include <utilities/SecXPCError.h>
#include <utilities/debugging.h>
#include <utilities/SecInternalReleasePriv.h>
#include <utilities/der_plist_internal.h>
#include <utilities/der_plist.h>
#include <securityd/personalization.h>
#include <securityd/SecPinningDb.h>
#include <securityd/SFKeychainControlManager.h>

#include <keychain/ckks/CKKS.h>
#include <keychain/ckks/CKKSControlServer.h>
#include "keychain/ot/OctagonControlServer.h"

#include <AssertMacros.h>
#include <CoreFoundation/CFXPCBridge.h>
#include <CoreFoundation/CoreFoundation.h>
// <rdar://problem/22425706> 13B104+Roots:Device never moved past spinner after using approval to ENABLE icdp

#if TARGET_OS_OSX
#include <Security/SecTaskPriv.h>
#include <login/SessionAgentStatusCom.h>
#endif
#include <asl.h>
#include <bsm/libbsm.h>
#include <ipc/securityd_client.h>
#include <libkern/OSAtomic.h>
#include <mach/mach.h>
#include <mach/message.h>
#include <notify.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <syslog.h>
#include <xpc/private.h>
#include <xpc/xpc.h>

#include <ipc/server_security_helpers.h>
#include <ipc/server_entitlement_helpers.h>

#if TARGET_OS_OSX
#include <sandbox.h>
#include <pwd.h>
#include <err.h>
#endif

static bool accessGroupPermitted(SecurityClient* client, CFArrayRef accessGroups, CFStringRef accessGroup) {
    /* NULL accessGroups is wildcard. */
    if (!accessGroups)
        return true;
    /* Make sure we have a string. */
    if (!isString(accessGroup))
        return false;

    /* Having the special accessGroup "*" allows access to all accessGroups. */
    CFRange range = { 0, CFArrayGetCount(accessGroups) };
    if (range.length &&
        (CFArrayContainsValue(accessGroups, range, accessGroup) ||
         CFArrayContainsValue(accessGroups, range, CFSTR("*"))))
        return true;

    return false;
}

static bool extractAccessGroup(SecurityClient *client, CFStringRef requestedAgrp, CFStringRef *resolvedAgrp, CFErrorRef *error) {
    bool ok = true;

    /* Access group sanity checking.
     Similar to accessGroupsAllows, but ignores accessGroupIsNetworkExtensionAndClientIsEntitled */
    CFArrayRef accessGroups = client->accessGroups;
    CFStringRef agrp = requestedAgrp;
    /* Having the special accessGroup "*" allows access to all accessGroups. */
    if (CFArrayContainsValue(accessGroups, CFRangeMake(0,CFArrayGetCount(accessGroups)), CFSTR("*")))
        accessGroups = NULL;

    if (requestedAgrp) {
        /* The user specified an explicit access group, validate it. */
        if (!accessGroupPermitted(client, accessGroups, requestedAgrp))
            ok = SecError(errSecMissingEntitlement, error, CFSTR("explicit accessGroup %@ not in client access %@"), requestedAgrp, accessGroups);
    } else {
        // We are using an implicit access group
        //  Add it as if the user specified it as an attribute.
        agrp = (CFStringRef)CFArrayGetValueAtIndex(client->accessGroups, 0);
    }

    if (agrp && resolvedAgrp)
        *resolvedAgrp = agrp;
    return ok;
}

static void with_label_and_password(xpc_object_t message, void (^action)(CFStringRef label, CFDataRef password)) {
    const char *label_utf8 = xpc_dictionary_get_string(message, kSecXPCKeyUserLabel);
    
    if (label_utf8) {   // Anything we would do here requires a user label
        size_t password_length = 0;
        const void *password_data = xpc_dictionary_get_data(message, kSecXPCKeyUserPassword, &password_length);
        
        CFDataRef user_password = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, password_data, password_length, kCFAllocatorNull);
        CFStringRef user_label = CFStringCreateWithCString(kCFAllocatorDefault, label_utf8, kCFStringEncodingUTF8);
        
        action(user_label, user_password);

        CFReleaseNull(user_password);
        CFReleaseNull(user_label);
    }
}

static void with_label_and_password_and_dsid(xpc_object_t message, void (^action)(CFStringRef label, CFDataRef password, CFStringRef dsid)) {
    const char *label_utf8 = xpc_dictionary_get_string(message, kSecXPCKeyUserLabel);
    
    if (label_utf8) {   // Anything we would do here requires a user label
        size_t password_length = 0;
        const void *password_data = xpc_dictionary_get_data(message, kSecXPCKeyUserPassword, &password_length);
        const char *xdsid = xpc_dictionary_get_string(message, kSecXPCKeyDSID);
        
        CFDataRef user_password = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, password_data, password_length, kCFAllocatorNull);
        CFStringRef user_label = CFStringCreateWithCString(kCFAllocatorDefault, label_utf8, kCFStringEncodingUTF8);
        CFStringRef dsid = CFStringCreateWithCString(kCFAllocatorDefault, xdsid, kCFStringEncodingUTF8);
        
        action(user_label, user_password, dsid);
        
        CFReleaseNull(dsid);
        CFReleaseNull(user_password);
        CFReleaseNull(user_label);
    }
}

static void with_label_and_number(xpc_object_t message, void (^action)(CFStringRef label, uint64_t number)) {
    const char *label_utf8 = xpc_dictionary_get_string(message, kSecXPCKeyViewName);

    if (label_utf8) {   // Anything we would do here requires a user label
        const int64_t number = xpc_dictionary_get_int64(message, kSecXPCKeyViewActionCode);
        CFStringRef user_label = CFStringCreateWithCString(kCFAllocatorDefault, label_utf8, kCFStringEncodingUTF8);

        action(user_label, number);
        CFReleaseNull(user_label);
    }
}

static CFArrayRef SecXPCDictionaryCopyPeerInfoArray(xpc_object_t dictionary, const char *key, CFErrorRef *error) {
    return CreateArrayOfPeerInfoWithXPCObject(xpc_dictionary_get_value(dictionary, key), error);
}

static CFDataRef SecXPCDictionaryCopyCFDataRef(xpc_object_t message, const char *key, CFErrorRef *error) {
    CFDataRef retval = NULL;
    const uint8_t *bytes = NULL;
    size_t len = 0;
    
    bytes = xpc_dictionary_get_data(message, key, &len);
    require_action_quiet(bytes, errOut, SOSCreateError(kSOSErrorBadKey, CFSTR("missing CFDataRef info"), NULL, error));
    retval = CFDataCreate(NULL, bytes, len);
    require_action_quiet(retval, errOut, SOSCreateError(kSOSErrorBadKey, CFSTR("could not allocate CFDataRef info"), NULL, error));
errOut:
    return retval;
}

static CFSetRef CreateCFSetRefFromXPCObject(xpc_object_t xpcSetDER, CFErrorRef* error) {
    CFSetRef retval = NULL;
    require_action_quiet(xpcSetDER, errOut, SecCFCreateErrorWithFormat(kSecXPCErrorUnexpectedNull, sSecXPCErrorDomain, NULL, error, NULL, CFSTR("Unexpected Null Set to decode")));

    require_action_quiet(xpc_get_type(xpcSetDER) == XPC_TYPE_DATA, errOut, SecCFCreateErrorWithFormat(kSecXPCErrorUnexpectedType, sSecXPCErrorDomain, NULL, error, NULL, CFSTR("xpcSetDER not data, got %@"), xpcSetDER));

    const uint8_t* der = xpc_data_get_bytes_ptr(xpcSetDER);
    const uint8_t* der_end = der + xpc_data_get_length(xpcSetDER);
    der = der_decode_set(kCFAllocatorDefault, kCFPropertyListMutableContainersAndLeaves, &retval, error, der, der_end);
    if (der != der_end) {
        SecError(errSecDecode, error, CFSTR("trailing garbage at end of SecAccessControl data"));
        goto errOut;
    }
    return retval;
errOut:
    CFReleaseNull(retval);
    return NULL;
}

static SOSPeerInfoRef SecXPCDictionaryCopyPeerInfo(xpc_object_t message, const char *key, CFErrorRef *error) {
    size_t length = 0;
    const uint8_t *der = xpc_dictionary_get_data(message, key, &length);

    return SecRequirementError(der != NULL, error, CFSTR("No data for key %s"), key) ? SOSPeerInfoCreateFromDER(kCFAllocatorDefault, error, &der, der + length) : NULL;
}

static CFSetRef SecXPCSetCreateFromXPCDictionaryElement(xpc_object_t event, const char *key) {
    CFErrorRef error = NULL;
    xpc_object_t object = xpc_dictionary_get_value(event, key);
    CFSetRef retval = NULL;
    if(object) retval = CreateCFSetRefFromXPCObject(object, &error);
    CFReleaseNull(error);
    return retval;
}

static inline
void xpc_dictionary_set_and_consume_CFArray(xpc_object_t xdict, const char *key, CF_CONSUMED CFArrayRef cf_array) {
    if (cf_array) {
        xpc_object_t xpc_array = _CFXPCCreateXPCObjectFromCFObject(cf_array);
        xpc_dictionary_set_value(xdict, key, xpc_array);
        xpc_release(xpc_array);
    }
    CFReleaseNull(cf_array);
}

static inline
bool xpc_dictionary_set_and_consume_PeerInfoArray(xpc_object_t xdict, const char *key, CF_CONSUMED CFArrayRef cf_array, CFErrorRef *error) {
    bool success = true;
    if (cf_array) {
        xpc_object_t xpc_array = CreateXPCObjectWithArrayOfPeerInfo(cf_array, error);
        if (xpc_array) {
            xpc_dictionary_set_value(xdict, key, xpc_array);
            xpc_release(xpc_array);
        } else {
            success = false;
        }
    }
    CFReleaseNull(cf_array);
    return success;
}

static CFDataRef
SecDataCopyMmapFileDescriptor(int fd, void **mem, size_t *size, CFErrorRef *error)
{
    struct stat sb;
    if (fstat(fd, &sb) < 0) {
        return NULL;
    }

    *size = (size_t)sb.st_size;
    if ((off_t)*size != sb.st_size) {
        return NULL;
    }

    *mem = mmap(NULL, *size, PROT_READ, MAP_SHARED, fd, 0);
    if (*mem == MAP_FAILED) {
        return NULL;
    }

    return CFDataCreateWithBytesNoCopy(NULL, *mem, *size, kCFAllocatorNull);
}

static bool
SecDataWriteFileDescriptor(int fd, CFDataRef data)
{
    CFIndex count = CFDataGetLength(data);
    const uint8_t *ptr = CFDataGetBytePtr(data);
    bool writeResult = false;

    while (count) {
        ssize_t ret = write(fd, ptr, count);
        if (ret <= 0)
            break;
        count -= ret;
        ptr += ret;
    }
    if (count == 0)
        writeResult = true;

    return writeResult;
}


// Returns error if entitlement isn't present.
static bool
EntitlementPresentAndTrue(uint64_t op, SecTaskRef clientTask, CFStringRef entitlement, CFErrorRef *error)
{
    if (!SecTaskGetBooleanValueForEntitlement(clientTask, entitlement)) {
        SecError(errSecMissingEntitlement, error, CFSTR("%@: %@ lacks entitlement %@"), SOSCCGetOperationDescription((enum SecXPCOperation)op), clientTask, entitlement);
        return false;
    }
    return true;
}

// Per <rdar://problem/13315020> Disable the entitlement check for "keychain-cloud-circle"
//  we disable entitlement enforcement. However, we still log so we know who needs the entitlement
static bool
EntitlementPresentOrWhine(uint64_t op, SecTaskRef clientTask, CFStringRef entitlement, CFErrorRef *error)
{
    if (!SecTaskGetBooleanValueForEntitlement(clientTask, entitlement))
        secnotice("serverxpc", "%@: %@ lacks entitlement %@", SOSCCGetOperationDescription((enum SecXPCOperation)op), clientTask, entitlement);

    return true;
}


static bool
EntitlementAbsentOrFalse(uint64_t op, SecTaskRef clientTask, CFStringRef entitlement, CFErrorRef *error)
{
    if (SecTaskGetBooleanValueForEntitlement(clientTask, entitlement)) {
        SecError(errSecNotAvailable, error, CFSTR("%@: %@ has entitlement %@"), SOSCCGetOperationDescription((enum SecXPCOperation) op), clientTask, entitlement);
        return false;
    }
    return true;
}

static void securityd_xpc_dictionary_handler(const xpc_connection_t connection, xpc_object_t event) {
    xpc_type_t type = xpc_get_type(event);
    __block CFErrorRef error = NULL;
    xpc_object_t xpcError = NULL;
    xpc_object_t replyMessage = NULL;
    CFDataRef  clientAuditToken = NULL;
    CFArrayRef domains = NULL;
    SecurityClient client = {
        .task = NULL,
        .accessGroups = NULL,
        .musr = NULL,
        .uid = xpc_connection_get_euid(connection),
        .allowSystemKeychain = false,
        .allowSyncBubbleKeychain = false,
        .isNetworkExtension = false,
        .canAccessNetworkExtensionAccessGroups = false,
    };

    secdebug("serverxpc", "entering");
    if (type == XPC_TYPE_DICTIONARY) {
        // TODO: Find out what we're dispatching.
        replyMessage = xpc_dictionary_create_reply(event);

        uint64_t operation = xpc_dictionary_get_uint64(event, kSecXPCKeyOperation);

        audit_token_t auditToken = {};
        xpc_connection_get_audit_token(connection, &auditToken);
        clientAuditToken = CFDataCreate(kCFAllocatorDefault, (const UInt8*)&auditToken, sizeof(auditToken));

        fill_security_client(&client, xpc_connection_get_euid(connection), auditToken);

#if TARGET_OS_IOS
        if (operation == sec_add_shared_web_credential_id || operation == sec_copy_shared_web_credential_id) {
            domains = SecTaskCopySharedWebCredentialDomains(client.task);
        }
#endif
        secinfo("serverxpc", "XPC [%@] operation: %@ (%" PRIu64 ")", client.task, SOSCCGetOperationDescription((enum SecXPCOperation)operation), operation);

        switch (operation)
            {
            case sec_item_add_id:
            {
                if (EntitlementAbsentOrFalse(sec_item_add_id, client.task, kSecEntitlementKeychainDeny, &error)) {
                    CFDictionaryRef query = SecXPCDictionaryCopyDictionary(event, kSecXPCKeyQuery, &error);
                    if (query) {
                        // Check for any entitlement-required attributes
                        bool entitlementsCorrect = true;
                        if(CFDictionaryGetValue(query, kSecAttrDeriveSyncIDFromItemAttributes) ||
                           CFDictionaryGetValue(query, kSecAttrPCSPlaintextServiceIdentifier) ||
                           CFDictionaryGetValue(query, kSecAttrPCSPlaintextPublicKey) ||
                           CFDictionaryGetValue(query, kSecAttrPCSPlaintextPublicIdentity)) {
                            entitlementsCorrect = EntitlementPresentAndTrue(sec_item_add_id, client.task, kSecEntitlementPrivateCKKSPlaintextFields, &error);
                        }
                        if (entitlementsCorrect && CFDictionaryGetValue(query, kSecAttrSysBound)) {
                            entitlementsCorrect = EntitlementPresentAndTrue(sec_item_add_id, client.task, kSecEntitlementPrivateSysBound, &error);
                        }

                        CFTypeRef result = NULL;
                        if(entitlementsCorrect) {
                            if (_SecItemAdd(query, &client, &result, &error) && result) {
                                SecXPCDictionarySetPList(replyMessage, kSecXPCKeyResult, result, &error);
                                CFReleaseNull(result);
                            }
                        }
                        CFReleaseNull(query);
                    }
                    break;
                }
            }
            case sec_item_copy_matching_id:
            {
                if (EntitlementAbsentOrFalse(sec_item_add_id, client.task, kSecEntitlementKeychainDeny, &error)) {
                    CFDictionaryRef query = SecXPCDictionaryCopyDictionary(event, kSecXPCKeyQuery, &error);
                    if (query) {
                        CFTypeRef result = NULL;
                        if (_SecItemCopyMatching(query, &client, &result, &error) && result) {
                            SecXPCDictionarySetPList(replyMessage, kSecXPCKeyResult, result, &error);
                            CFReleaseNull(result);
                        }
                        CFReleaseNull(query);
                    }
                    break;
                }
            }
            case sec_item_update_id:
            {
                if (EntitlementAbsentOrFalse(sec_item_update_id, client.task, kSecEntitlementKeychainDeny, &error)) {
                    CFDictionaryRef query = SecXPCDictionaryCopyDictionary(event, kSecXPCKeyQuery, &error);
                    if (query) {
                        CFDictionaryRef attributesToUpdate = SecXPCDictionaryCopyDictionary(event, kSecXPCKeyAttributesToUpdate, &error);
                        if (attributesToUpdate) {
                            // Check for any entitlement-required attributes
                            bool entitlementsCorrect = true;
                            if(CFDictionaryGetValue(query, kSecAttrDeriveSyncIDFromItemAttributes) ||
                               CFDictionaryGetValue(attributesToUpdate, kSecAttrPCSPlaintextServiceIdentifier) ||
                               CFDictionaryGetValue(attributesToUpdate, kSecAttrPCSPlaintextPublicKey) ||
                               CFDictionaryGetValue(attributesToUpdate, kSecAttrPCSPlaintextPublicIdentity)) {
                                entitlementsCorrect = EntitlementPresentAndTrue(sec_item_update_id, client.task, kSecEntitlementPrivateCKKSPlaintextFields, &error);
                            }
                            if (entitlementsCorrect && CFDictionaryGetValue(query, kSecAttrSysBound)) {
                                entitlementsCorrect = EntitlementPresentAndTrue(sec_item_update_id, client.task, kSecEntitlementPrivateSysBound, &error);
                            }

                            if(entitlementsCorrect) {
                                bool result = _SecItemUpdate(query, attributesToUpdate, &client, &error);
                                xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, result);
                            }
                            CFReleaseNull(attributesToUpdate);
                        }
                        CFReleaseNull(query);
                    }
                }
                break;
            }
            case sec_item_delete_id:
            {
                if (EntitlementAbsentOrFalse(sec_item_add_id, client.task, kSecEntitlementKeychainDeny, &error)) {
                    CFDictionaryRef query = SecXPCDictionaryCopyDictionary(event, kSecXPCKeyQuery, &error);
                    if (query) {
                        bool result = _SecItemDelete(query, &client, &error);
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, result);
                        CFReleaseNull(query);
                    }
                }
                break;
            }
            case sec_item_update_token_items_id:
            {
                if (EntitlementAbsentOrFalse(sec_item_add_id, client.task, kSecEntitlementKeychainDeny, &error)) {
                    CFStringRef tokenID =  SecXPCDictionaryCopyString(event, kSecXPCKeyString, &error);
                    CFArrayRef attributes = SecXPCDictionaryCopyArray(event, kSecXPCKeyQuery, &error);
                    if (tokenID) {
                        bool result = _SecItemUpdateTokenItems(tokenID, attributes, &client, &error);
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, result);
                    }
                    CFReleaseNull(tokenID);
                    CFReleaseNull(attributes);
                }
                break;
            }
            case sec_delete_all_id:
            {
                bool retval = false;
#if TARGET_OS_IPHONE
                /* buddy is temporary allowed to do this */
                CFStringRef applicationIdentifier = SecTaskCopyApplicationIdentifier(client.task);
                bool isBuddy = applicationIdentifier &&
                    CFEqual(applicationIdentifier, CFSTR("com.apple.purplebuddy"));

                if (isBuddy || EntitlementPresentAndTrue(operation, client.task, kSecEntitlementPrivateDeleteAll, &error))
                {
                    retval = _SecItemDeleteAll(&error);
                }
#endif
                xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, retval);
                break;
            }
            case sec_keychain_backup_id:
            {
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementRestoreKeychain, &error)) {
                    CFDataRef keybag = NULL, passcode = NULL;
                    if (SecXPCDictionaryCopyDataOptional(event, kSecXPCKeyKeybag, &keybag, &error)) {
                        if (SecXPCDictionaryCopyDataOptional(event, kSecXPCKeyUserPassword, &passcode, &error)) {
                            bool emcs = SecXPCDictionaryGetBool(event, kSecXPCKeyEMCSBackup, NULL);
                            CFDataRef backup = _SecServerKeychainCreateBackup(&client, keybag, passcode, emcs, &error);
                            if (backup) {
                                int fd = SecXPCDictionaryDupFileDescriptor(event, kSecXPCKeyFileDescriptor, NULL);
                                if (fd < 0) {
                                    SecXPCDictionarySetData(replyMessage, kSecXPCKeyResult, backup, &error);
                                } else {
                                    bool writeResult = SecDataWriteFileDescriptor(fd, backup);
                                    if (close(fd) != 0)
                                        writeResult = false;
                                    if (!writeResult)
                                        SecError(errSecIO, &error, CFSTR("Failed to write backup file: %d"), errno);
                                    SecXPCDictionarySetBool(replyMessage, kSecXPCKeyResult, writeResult, NULL);
                                }
                                CFRelease(backup);
                            }
                            CFReleaseSafe(passcode);
                        }
                        CFReleaseSafe(keybag);
                    }
                }
                break;
            }
            case sec_keychain_restore_id:
            {
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementRestoreKeychain, &error)) {
                    CFDataRef backup = NULL;
                    void *mem = NULL;
                    size_t size = 0;

                    int fd = SecXPCDictionaryDupFileDescriptor(event, kSecXPCKeyFileDescriptor, NULL);
                    if (fd != -1) {
                        backup = SecDataCopyMmapFileDescriptor(fd, &mem, &size, &error);
                    } else {
                        backup = SecXPCDictionaryCopyData(event, kSecXPCKeyBackup, &error);
                    }
                    if (backup) {
                        CFDataRef keybag = SecXPCDictionaryCopyData(event, kSecXPCKeyKeybag, &error);
                        if (keybag) {
                            CFDataRef passcode = NULL;
                            if (SecXPCDictionaryCopyDataOptional(event, kSecXPCKeyUserPassword, &passcode, &error)) {
                                bool result = _SecServerKeychainRestore(backup, &client, keybag, passcode, &error);
                                xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, result);
                                CFReleaseSafe(passcode);
                            }
                        }
                        CFReleaseNull(keybag);
                    }
                    CFReleaseNull(backup);
                    if (fd != -1)
                        close(fd);
                    if (mem) {
                        munmap(mem, size);
                    }
                }
                break;
            }
            case sec_keychain_backup_keybag_uuid_id:
            {
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementRestoreKeychain, &error)) {
                    CFDataRef backup = NULL;
                    CFStringRef uuid = NULL;
                    void *mem = NULL;
                    size_t size = 0;

                    int fd = SecXPCDictionaryDupFileDescriptor(event, kSecXPCKeyFileDescriptor, NULL);
                    if (fd != -1) {
                        backup = SecDataCopyMmapFileDescriptor(fd, &mem, &size, &error);
                        if (backup)
                            uuid = _SecServerBackupCopyUUID(backup, &error);
                    }
                    if (uuid)
                        SecXPCDictionarySetString(replyMessage, kSecXPCKeyResult, uuid, &error);

                    CFReleaseNull(backup);
                    if (fd != -1)
                        close(fd);
                    if (mem) {
                        munmap(mem, size);
                    }
                    CFReleaseNull(uuid);
                }
                break;
            }
            case sec_keychain_sync_update_message_id:
            {
                CFDictionaryRef updates = SecXPCDictionaryCopyDictionary(event, kSecXPCKeyQuery, &error);
                if (updates) {
                    CFArrayRef result = _SecServerKeychainSyncUpdateMessage(updates, &error);
                    SecXPCDictionarySetPList(replyMessage, kSecXPCKeyResult, result, &error);
                    CFReleaseNull(result);
                }
                CFReleaseNull(updates);
                break;
            }
            case sec_keychain_backup_syncable_id:
            {
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementRestoreKeychain, &error)) {
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
                            CFReleaseNull(keybag);
                        }
                        CFReleaseSafe(oldbackup);
                    }
                }
                break;
            }
            case sec_keychain_restore_syncable_id:
            {
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementRestoreKeychain, &error)) {
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
                            CFReleaseNull(keybag);
                        }
                        CFReleaseNull(backup);
                    }
                }
                break;
            }
            case sec_item_backup_copy_names_id:
            {
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementRestoreKeychain, &error)) {
                    CFArrayRef names = SecServerItemBackupCopyNames(&error);
                    SecXPCDictionarySetPListOptional(replyMessage, kSecXPCKeyResult, names, &error);
                    CFReleaseSafe(names);
                }
                break;
            }
            case sec_item_backup_handoff_fd_id:
            {
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementRestoreKeychain, &error)) {
                    CFStringRef backupName = SecXPCDictionaryCopyString(event, kSecXPCKeyBackup, &error);
                    int fd = -1;
                    if (backupName) {
                        fd = SecServerItemBackupHandoffFD(backupName, &error);
                        CFRelease(backupName);
                    }
                    SecXPCDictionarySetFileDescriptor(replyMessage, kSecXPCKeyResult, fd, &error);
                    if (fd != -1)
                        close(fd);
                }
                break;
            }
            case sec_item_backup_set_confirmed_manifest_id:
            {
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementRestoreKeychain, &error)) {
                    CFDataRef keybagDigest = NULL;
                    if (SecXPCDictionaryCopyDataOptional(event, kSecXPCKeyKeybag, &keybagDigest, &error)) {
                        CFDataRef manifest = NULL;
                        if (SecXPCDictionaryCopyDataOptional(event, kSecXPCData, &manifest, &error)) {
                            CFStringRef backupName = SecXPCDictionaryCopyString(event, kSecXPCKeyBackup, &error);
                            if (backupName) {
                                bool result = SecServerItemBackupSetConfirmedManifest(backupName, keybagDigest, manifest, &error);
                                CFRelease(backupName);
                                xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, result);
                            }
                            CFReleaseSafe(manifest);
                        }
                    }
                    CFReleaseNull(keybagDigest);
                }
                break;
            }
            case sec_item_backup_restore_id:
            {
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementRestoreKeychain, &error)) {
                    bool result = false;
                    CFStringRef backupName = SecXPCDictionaryCopyString(event, kSecXPCKeyBackup, &error);
                    if (backupName) {
                        CFStringRef peerID = NULL;
                        if (SecXPCDictionaryCopyStringOptional(event, kSecXPCKeyDigest, &peerID, &error)) {
                            CFDataRef keybag = SecXPCDictionaryCopyData(event, kSecXPCKeyKeybag, &error);
                            if (keybag) {
                                CFDataRef secret = SecXPCDictionaryCopyData(event, kSecXPCKeyUserPassword, &error);
                                if (secret) {
                                    CFDataRef backup = SecXPCDictionaryCopyData(event, kSecXPCData, &error);
                                    if (backup) {
                                        result = SecServerItemBackupRestore(backupName, peerID, keybag, secret, backup, &error);
                                        CFRelease(backup);
                                    }
                                    CFRelease(secret);
                                }
                                CFRelease(keybag);
                            }
                            CFReleaseSafe(peerID);
                        }
                        CFRelease(backupName);
                    }
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, result);
                }
                break;
            }
            case sec_add_shared_web_credential_id:
            {
#if TARGET_OS_IOS && !TARGET_OS_BRIDGE
                CFDictionaryRef query = SecXPCDictionaryCopyDictionary(event, kSecXPCKeyQuery, &error);
                if (query) {
                    CFTypeRef result = NULL;

                    CFStringRef appID = (client.task) ? SecTaskCopyApplicationIdentifier(client.task) : NULL;
                    if (_SecAddSharedWebCredential(query, &client, &auditToken, appID, domains, &result, &error) && result) {
                        SecXPCDictionarySetPList(replyMessage, kSecXPCKeyResult, result, &error);
                        CFReleaseNull(result);
                    }
                    CFReleaseSafe(appID);
                    CFReleaseNull(query);
                }
#else
                SecXPCDictionarySetPList(replyMessage, kSecXPCKeyResult, kCFBooleanFalse, &error);
#endif
                break;
            }
            case sec_copy_shared_web_credential_id:
            {
#if TARGET_OS_IOS && !TARGET_OS_BRIDGE
                CFDictionaryRef query = SecXPCDictionaryCopyDictionary(event, kSecXPCKeyQuery, &error);
                if (query) {
                    CFTypeRef result = NULL;
                    CFStringRef appID = (client.task) ? SecTaskCopyApplicationIdentifier(client.task) : NULL;
                    if (_SecCopySharedWebCredential(query, &client, &auditToken, appID, domains, &result, &error) && result) {
                        SecXPCDictionarySetPList(replyMessage, kSecXPCKeyResult, result, &error);
                        CFReleaseNull(result);
                    }
                    CFReleaseSafe(appID);
                    CFReleaseNull(query);
                }
#else
                SecXPCDictionarySetPList(replyMessage, kSecXPCKeyResult, kCFBooleanFalse, &error);
#endif
                break;
            }
            case sec_get_log_settings_id:
            {
                CFPropertyListRef currentList = SecCopyLogSettings_Server(&error);
                if (currentList) {
                    SecXPCDictionarySetPList(replyMessage, kSecXPCKeyResult, currentList, &error);
                }
                CFReleaseSafe(currentList);
                break;
            }
            case sec_set_xpc_log_settings_id:
            {
                CFPropertyListRef newSettings = SecXPCDictionaryCopyPList(event, kSecXPCKeyQuery, &error);
                if (newSettings) {
                    SecSetXPCLogSettings_Server(newSettings, &error);
                }
                xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, true);
                CFReleaseNull(newSettings);
                break;
            }
            case sec_set_circle_log_settings_id:
            {
                CFPropertyListRef newSettings = SecXPCDictionaryCopyPList(event, kSecXPCKeyQuery, &error);
                if (newSettings) {
                    SecSetCircleLogSettings_Server(newSettings, &error);
                }
                xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, true);
                CFReleaseNull(newSettings);
                break;
            }
            case sec_otr_session_create_remote_id:
            {
                CFDataRef publicPeerId = NULL;
                if (SecXPCDictionaryCopyDataOptional(event, kSecXPCPublicPeerId, &publicPeerId, &error)) {
                    CFDataRef otrSession = _SecOTRSessionCreateRemote(publicPeerId, &error);
                    if (otrSession) {
                        SecXPCDictionarySetData(replyMessage, kSecXPCKeyResult, otrSession, &error);
                        CFReleaseNull(otrSession);
                    }
                    CFReleaseSafe(publicPeerId);
                }
                break;
            }
            case sec_otr_session_process_packet_remote_id:
            {
                CFDataRef sessionData = NULL, inputPacket = NULL, outputSessionData = NULL, outputPacket = NULL;
                bool readyForMessages = false;
                if (SecXPCDictionaryCopyDataOptional(event, kSecXPCOTRSession, &sessionData, &error)) {
                    if (SecXPCDictionaryCopyDataOptional(event, kSecXPCData, &inputPacket, &error)) {
                        bool result = _SecOTRSessionProcessPacketRemote(sessionData, inputPacket, &outputSessionData, &outputPacket, &readyForMessages, &error);
                        if (result) {
                            SecXPCDictionarySetData(replyMessage, kSecXPCOTRSession, outputSessionData, &error);
                            SecXPCDictionarySetData(replyMessage, kSecXPCData, outputPacket, &error);
                            xpc_dictionary_set_bool(replyMessage, kSecXPCOTRReady, readyForMessages);
                            CFReleaseNull(outputSessionData);
                            CFReleaseNull(outputPacket);
                        }
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, result);

                        CFReleaseSafe(inputPacket);
                    }
                    CFReleaseSafe(sessionData);
                }
                break;
            }
            case kSecXPCOpTryUserCredentials:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    with_label_and_password_and_dsid(event, ^(CFStringRef label, CFDataRef password, CFStringRef dsid) {
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                                SOSCCTryUserCredentials_Server(label, password, dsid, &error));
                    });
                }
                break;
            case kSecXPCOpSetUserCredentials:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    with_label_and_password(event, ^(CFStringRef label, CFDataRef password) {
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                                SOSCCSetUserCredentials_Server(label, password, &error));
                    });
                }
                break;
            case kSecXPCOpSetUserCredentialsAndDSID:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    with_label_and_password_and_dsid(event, ^(CFStringRef label, CFDataRef password, CFStringRef dsid) {
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                                SOSCCSetUserCredentialsAndDSID_Server(label, password, dsid, &error));
                    });
                }
                break;
            case kSecXPCOpView:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    with_label_and_number(event, ^(CFStringRef view, uint64_t actionCode) {
                        xpc_dictionary_set_int64(replyMessage, kSecXPCKeyResult,
                                                 SOSCCView_Server(view, (SOSViewActionCode)actionCode, &error));
                    });
                }
                break;
            case kSecXPCOpViewSet:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    CFSetRef enabledViews = SecXPCSetCreateFromXPCDictionaryElement(event, kSecXPCKeyEnabledViewsKey);
                    CFSetRef disabledViews = SecXPCSetCreateFromXPCDictionaryElement(event, kSecXPCKeyDisabledViewsKey);
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, SOSCCViewSet_Server(enabledViews, disabledViews));
                    CFReleaseNull(enabledViews);
                    CFReleaseNull(disabledViews);
                }
                break;
            case kSecXPCOpSecurityProperty:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    with_label_and_number(event, ^(CFStringRef property, uint64_t actionCode) {
                        xpc_dictionary_set_int64(replyMessage, kSecXPCKeyResult,
                                                 SOSCCSecurityProperty_Server(property, (SOSSecurityPropertyActionCode)actionCode, &error));
                    });
                }
                break;
            case kSecXPCOpCanAuthenticate:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            SOSCCCanAuthenticate_Server(&error));
                }
                break;
            case kSecXPCOpPurgeUserCredentials:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            SOSCCPurgeUserCredentials_Server(&error));
                }
                break;
            case kSecXPCOpDeviceInCircle:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_int64(replyMessage, kSecXPCKeyResult,
                                             SOSCCThisDeviceIsInCircle_Server(&error));
                }
                break;
            case kSecXPCOpRequestToJoin:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            SOSCCRequestToJoinCircle_Server(&error));
                }
                break;
            case kSecXPCOpAccountHasPublicKey:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            SOSCCAccountHasPublicKey_Server(&error));
                }
                break;
            case kSecXPCOpAccountIsNew:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            SOSCCAccountIsNew_Server(&error));
                }
                break;
            case kSecXPCOpRequestToJoinAfterRestore:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            SOSCCRequestToJoinCircleAfterRestore_Server(&error));
                }
                break;
            case kSecXPCOpRequestEnsureFreshParameters:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            SOSCCRequestEnsureFreshParameters_Server(&error));
                }
                break;
            case kSecXPCOpGetAllTheRings:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                   CFStringRef ringDescriptions = SOSCCGetAllTheRings_Server(&error);
                    xpc_object_t xpc_dictionary = _CFXPCCreateXPCObjectFromCFObject(ringDescriptions);
                    xpc_dictionary_set_value(replyMessage, kSecXPCKeyResult, xpc_dictionary);
                    xpc_release(xpc_dictionary);
                }
                break;
            case kSecXPCOpApplyToARing:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    CFStringRef ringName = SecXPCDictionaryCopyString(event, kSecXPCKeyString, &error);
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, SOSCCApplyToARing_Server(ringName, &error));
                    CFReleaseNull(ringName);
                }
                break;
            case kSecXPCOpWithdrawlFromARing:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    CFStringRef ringName = SecXPCDictionaryCopyString(event, kSecXPCKeyString, &error);
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, SOSCCWithdrawlFromARing_Server(ringName, &error));
                    CFReleaseNull(ringName);
                }
                break;
            case kSecXPCOpRingStatus:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    CFStringRef ringName = SecXPCDictionaryCopyString(event, kSecXPCKeyString, &error);
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, SOSCCRingStatus_Server(ringName, &error));
                    CFReleaseNull(ringName);
                }
                break;
            case kSecXPCOpEnableRing:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    CFStringRef ringName = SecXPCDictionaryCopyString(event, kSecXPCKeyString, &error);
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, SOSCCEnableRing_Server(ringName, &error));
                    CFReleaseNull(ringName);
                }
                break;
            case kSecXPCOpRequestDeviceID:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    CFStringRef deviceID = SOSCCCopyDeviceID_Server(&error);
                    if (deviceID) {
                        SecXPCDictionarySetString(replyMessage, kSecXPCKeyResult, deviceID, &error);
                    }
                    CFReleaseNull(deviceID);
                }
                break;
            case kSecXPCOpSetDeviceID:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    CFStringRef IDS = SecXPCDictionaryCopyString(event, kSecXPCKeyDeviceID, &error);
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, SOSCCSetDeviceID_Server(IDS, &error));
                    CFReleaseNull(IDS);
                }
                break;
            case kSecXPCOpHandleIDSMessage:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    CFDictionaryRef IDS = SecXPCDictionaryCopyDictionary(event, kSecXPCKeyIDSMessage, &error);
                    xpc_dictionary_set_int64(replyMessage, kSecXPCKeyResult, SOSCCHandleIDSMessage_Server(IDS, &error));
                    CFReleaseNull(IDS);
                }
                break;
            case kSecXPCOpClearKVSPeerMessage:
                    if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                        CFStringRef peerID = SecXPCDictionaryCopyString(event, kSecXPCKeyDeviceID, &error);
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, SOSCCClearPeerMessageKeyInKVS_Server(peerID, &error));
                        CFReleaseNull(peerID);
                    }
                    break;
            case kSecXPCOpSendIDSMessage:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    CFStringRef message = SecXPCDictionaryCopyString(event, kSecXPCKeySendIDSMessage, &error);
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, SOSCCIDSServiceRegistrationTest_Server(message, &error));
                    CFReleaseNull(message);
                }
                break;
            case kSecXPCOpSyncWithKVSPeer:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    CFStringRef peerID = SecXPCDictionaryCopyString(event, kSecXPCKeyDeviceID, &error);
                    CFDataRef message = SecXPCDictionaryCopyData(event, kSecXPCKeyIDSMessage, &error);
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, SOSCCRequestSyncWithPeerOverKVS_Server(peerID, message, &error));
                    CFReleaseNull(message);
                    CFReleaseNull(peerID);
                }
                break;
            case kSecXPCOpSyncWithKVSPeerIDOnly:
                    if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                        CFStringRef peerID = SecXPCDictionaryCopyString(event, kSecXPCKeyDeviceID, &error);
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, SOSCCRequestSyncWithPeerOverKVSUsingIDOnly_Server(peerID, &error));
                        CFReleaseNull(peerID);
                    }
                    break;
            case kSecXPCOpPingTest:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    CFStringRef message = SecXPCDictionaryCopyString(event, kSecXPCKeySendIDSMessage, &error);
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, SOSCCIDSPingTest_Server(message, &error));
                    CFReleaseNull(message);
                }
                break;
            case kSecXPCOpIDSDeviceID:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, SOSCCIDSDeviceIDIsAvailableTest_Server(&error));
                }
                break;
			case kSecXPCOpAccountSetToNew:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, SOSCCAccountSetToNew_Server(&error));
                }
				break;
            case kSecXPCOpResetToOffering:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            SOSCCResetToOffering_Server(&error));
                }
                break;
            case kSecXPCOpResetToEmpty:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            SOSCCResetToEmpty_Server(&error));
                }
                break;
            case kSecXPCOpRemoveThisDeviceFromCircle:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            SOSCCRemoveThisDeviceFromCircle_Server(&error));
                }
                break;
            case kSecXPCOpRemovePeersFromCircle:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    CFArrayRef applicants = SecXPCDictionaryCopyPeerInfoArray(event, kSecXPCKeyPeerInfoArray, &error);
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            SOSCCRemovePeersFromCircle_Server(applicants, &error));
                    CFReleaseNull(applicants);
                }
                break;
            case kSecXPCOpLoggedOutOfAccount:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            SOSCCLoggedOutOfAccount_Server(&error));
                }
                break;
            case kSecXPCOpBailFromCircle:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    uint64_t limit_in_seconds = xpc_dictionary_get_uint64(event, kSecXPCLimitInMinutes);
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            SOSCCBailFromCircle_Server(limit_in_seconds, &error));
                }
                break;
            case kSecXPCOpAcceptApplicants:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_object_t xapplicants = xpc_dictionary_get_value(event, kSecXPCKeyPeerInfoArray);
                    CFArrayRef applicants = CreateArrayOfPeerInfoWithXPCObject(xapplicants, &error); //(CFArrayRef)(_CFXPCCreateCFObjectFromXPCObject(xapplicants));
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            (applicants && SOSCCAcceptApplicants_Server(applicants, &error)));
                    CFReleaseSafe(applicants);
                }
                break;
            case kSecXPCOpRejectApplicants:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_object_t xapplicants = xpc_dictionary_get_value(event, kSecXPCKeyPeerInfoArray);
                    CFArrayRef applicants = CreateArrayOfPeerInfoWithXPCObject(xapplicants, &error); //(CFArrayRef)(_CFXPCCreateCFObjectFromXPCObject(xapplicants));
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            (applicants && SOSCCRejectApplicants_Server(applicants, &error)));
                    CFReleaseSafe(applicants);
                }
                break;
            case kSecXPCOpSetNewPublicBackupKey:
                {
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementRestoreKeychain, &error)) {
                        CFDataRef publicBackupKey = SecXPCDictionaryCopyData(event, kSecXPCKeyNewPublicBackupKey, &error);
                        SOSPeerInfoRef peerInfo = SOSCCSetNewPublicBackupKey_Server(publicBackupKey, &error);
                        CFDataRef peerInfoData = peerInfo ? SOSPeerInfoCopyEncodedData(peerInfo, kCFAllocatorDefault, &error) : NULL;
                        CFReleaseNull(peerInfo);
                        if (peerInfoData) {
                            xpc_object_t xpc_object = _CFXPCCreateXPCObjectFromCFObject(peerInfoData);
                            xpc_dictionary_set_value(replyMessage, kSecXPCKeyResult, xpc_object);
                            xpc_release(xpc_object);
                        }
                        CFReleaseNull(peerInfoData);
                        CFReleaseSafe(publicBackupKey);

                    }
                }
                break;
            case kSecXPCOpRegisterRecoveryPublicKey:
                {
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementRestoreKeychain, &error)) {
                        CFDataRef recovery_key = SecXPCDictionaryCopyData(event, kSecXPCKeyRecoveryPublicKey, &error);
                        uint8_t zero = 0;
                        CFDataRef nullData = CFDataCreate(kCFAllocatorDefault, &zero, 1); // token we send if we really wanted to send NULL
                        if(CFEqual(recovery_key, nullData)) {
                            CFReleaseNull(recovery_key);
                        }
                        CFReleaseNull(nullData);
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, SOSCCRegisterRecoveryPublicKey_Server(recovery_key, &error));
                        CFReleaseNull(recovery_key);
                    }
                }
                break;
            case kSecXPCOpGetRecoveryPublicKey:
                {
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementRestoreKeychain, &error)) {
                        xpc_object_t xpc_recovery_object = NULL;
                        CFDataRef recovery = SOSCCCopyRecoveryPublicKey(&error);
                        if(recovery)
                            xpc_recovery_object = _CFXPCCreateXPCObjectFromCFObject(recovery);

                        xpc_dictionary_set_value(replyMessage, kSecXPCKeyResult, xpc_recovery_object);
                        CFReleaseNull(recovery);
                    }
                }
                break;
            case kSecXPCOpSetBagForAllSlices:
                {
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementRestoreKeychain, &error)) {
                        CFDataRef backupSlice = SecXPCDictionaryCopyData(event, kSecXPCKeyKeybag, &error);
                        bool includeV0 = xpc_dictionary_get_bool(event, kSecXPCKeyIncludeV0);
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, backupSlice && SOSCCRegisterSingleRecoverySecret_Server(backupSlice, includeV0, &error));
                        CFReleaseSafe(backupSlice);
                    }
                }
                break;
            case kSecXPCOpCopyApplicantPeerInfo:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_and_consume_PeerInfoArray(replyMessage, kSecXPCKeyResult,
                                                                 SOSCCCopyApplicantPeerInfo_Server(&error),
                                                                 &error);
                }
                break;
            case kSecXPCOpCopyValidPeerPeerInfo:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_and_consume_PeerInfoArray(replyMessage, kSecXPCKeyResult,
                                                                 SOSCCCopyValidPeerPeerInfo_Server(&error),
                                                                 &error);
                }
                break;
            case kSecXPCOpValidateUserPublic:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    bool trusted = SOSCCValidateUserPublic_Server(&error);
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, trusted);
                }
                break;
            case kSecXPCOpCopyNotValidPeerPeerInfo:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_and_consume_PeerInfoArray(replyMessage, kSecXPCKeyResult,
                                                                 SOSCCCopyNotValidPeerPeerInfo_Server(&error),
                                                                 &error);
                }
                break;
            case kSecXPCOpCopyGenerationPeerInfo:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_and_consume_CFArray(replyMessage, kSecXPCKeyResult,
                                                           SOSCCCopyGenerationPeerInfo_Server(&error));
                }
                break;
            case kSecXPCOpCopyRetirementPeerInfo:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_and_consume_PeerInfoArray(replyMessage, kSecXPCKeyResult,
                                                                 SOSCCCopyRetirementPeerInfo_Server(&error),
                                                                 &error);
                }
                break;
            case kSecXPCOpCopyViewUnawarePeerInfo:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_and_consume_PeerInfoArray(replyMessage, kSecXPCKeyResult,
                                                                 SOSCCCopyViewUnawarePeerInfo_Server(&error),
                                                                 &error);
                }
                break;
            case kSecXPCOpCopyAccountData:
                {
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                        xpc_object_t xpc_account_object = NULL;
                        CFDataRef accountData = SOSCCCopyAccountState_Server(&error);
                        if(accountData)
                            xpc_account_object = _CFXPCCreateXPCObjectFromCFObject(accountData);
                        
                        xpc_dictionary_set_value(replyMessage, kSecXPCKeyResult, xpc_account_object);
                        CFReleaseNull(accountData);
                    }
                    break;
                }
            case kSecXPCOpDeleteAccountData:
                {
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                        bool status = SOSCCDeleteAccountState_Server(&error);
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, status);
                    }
                    break;
                }
            case kSecXPCOpCopyEngineData:
                {
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {

                        xpc_object_t xpc_engine_object = NULL;
                        CFDataRef engineData = SOSCCCopyEngineData_Server(&error);
                        if(engineData)
                            xpc_engine_object = _CFXPCCreateXPCObjectFromCFObject(engineData);

                        xpc_dictionary_set_value(replyMessage, kSecXPCKeyResult, xpc_engine_object);
                        CFReleaseNull(engineData);

                    }
                    break;
                }
            case kSecXPCOpDeleteEngineData:
                {
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                        bool status = SOSCCDeleteEngineState_Server(&error);
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, status);
                    }
                    break;
                }
            case kSecXPCOpCopyEngineState:
                {
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                        CFArrayRef array = SOSCCCopyEngineState_Server(&error);
                        CFDataRef derData = NULL;

                        require_quiet(array, done);
                        derData = CFPropertyListCreateDERData(kCFAllocatorDefault, array, &error);

                        require_quiet(derData, done);
                        xpc_dictionary_set_data(replyMessage, kSecXPCKeyResult, CFDataGetBytePtr(derData), CFDataGetLength(derData));
                    done:
                        CFReleaseNull(derData);
                        CFReleaseNull(array);
                    }
                }
                break;
            case kSecXPCOpCopyPeerPeerInfo:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_and_consume_PeerInfoArray(replyMessage, kSecXPCKeyResult,
                                                                 SOSCCCopyPeerPeerInfo_Server(&error),
                                                                 &error);
                }
                break;
            case kSecXPCOpCopyConcurringPeerPeerInfo:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_and_consume_PeerInfoArray(replyMessage, kSecXPCKeyResult,
                                                                 SOSCCCopyConcurringPeerPeerInfo_Server(&error),
                                                                 &error);
                }
                break;
            case kSecXPCOpCopyMyPeerInfo:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    SOSPeerInfoRef peerInfo = SOSCCCopyMyPeerInfo_Server(&error);
                    CFDataRef peerInfoData = peerInfo ? SOSPeerInfoCopyEncodedData(peerInfo, kCFAllocatorDefault, &error) : NULL;
                    CFReleaseNull(peerInfo);
                    if (peerInfoData) {
                        xpc_object_t xpc_object = _CFXPCCreateXPCObjectFromCFObject(peerInfoData);
                        xpc_dictionary_set_value(replyMessage, kSecXPCKeyResult, xpc_object);
                        xpc_release(xpc_object);
                    }
                    CFReleaseNull(peerInfoData);
                }
                break;
            case kSecXPCOpGetLastDepartureReason:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_int64(replyMessage, kSecXPCKeyResult,
                                             SOSCCGetLastDepartureReason_Server(&error));
                }
                break;
			case kSecXPCOpSetLastDepartureReason:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    int32_t reason = (int32_t) xpc_dictionary_get_int64(event, kSecXPCKeyReason);
                    xpc_dictionary_set_int64(replyMessage, kSecXPCKeyResult,
                                             SOSCCSetLastDepartureReason_Server(reason, &error));
                }
                break;
            case kSecXPCOpProcessSyncWithPeers:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainSyncUpdates, &error)) {
                    CFSetRef peers = SecXPCDictionaryCopySet(event, kSecXPCKeySet, &error);
                    CFSetRef backupPeers = SecXPCDictionaryCopySet(event, kSecXPCKeySet2, &error);
                    if (peers && backupPeers) {
                        CFSetRef result = SOSCCProcessSyncWithPeers_Server(peers, backupPeers, &error);
                        if (result) {
                            SecXPCDictionarySetPList(replyMessage, kSecXPCKeyResult, result, &error);
                        }
                        CFReleaseNull(result);
                    }
                    CFReleaseNull(peers);
                    CFReleaseNull(backupPeers);
                }
                break;
            case kSecXPCOpProcessSyncWithAllPeers:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainSyncUpdates, &error)) {
                    xpc_dictionary_set_int64(replyMessage, kSecXPCKeyResult,
                                             SOSCCProcessSyncWithAllPeers_Server(&error));
                }
                break;
            case soscc_EnsurePeerRegistration_id:
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainSyncUpdates, &error)) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            SOSCCProcessEnsurePeerRegistration_Server(&error));
                }
                break;
            case kSecXPCOpCopyIncompatibilityInfo:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    CFStringRef iis = SOSCCCopyIncompatibilityInfo_Server(&error);
                    SecXPCDictionarySetString(replyMessage, kSecXPCKeyResult, iis, &error);
                    CFReleaseSafe(iis);
                }
                break;
            case kSecXPCOpRollKeys:
                {
                    bool force = xpc_dictionary_get_bool(event, "force");
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                                 _SecServerRollKeys(force, &client, &error));
                }
                break;
            case kSecXPCOpWaitForInitialSync:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                            SOSCCWaitForInitialSync_Server(&error));
                }
                break;

            case kSecXPCOpCopyYetToSyncViews:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    CFArrayRef array = SOSCCCopyYetToSyncViewsList_Server(&error);
                    if (array) {
                        xpc_object_t xpc_array = _CFXPCCreateXPCObjectFromCFObject(array);
                        xpc_dictionary_set_value(replyMessage, kSecXPCKeyResult, xpc_array);
                        xpc_release(xpc_array);
                    }
                    CFReleaseNull(array);
                }
                break;
            case kSecXPCOpSetEscrowRecord:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    CFStringRef escrow_label = SecXPCDictionaryCopyString(event, kSecXPCKeyEscrowLabel, &error);
                    uint64_t tries = xpc_dictionary_get_int64(event, kSecXPCKeyTriesLabel);

                    bool result =  SOSCCSetEscrowRecord_Server(escrow_label, tries, &error);
                    if (result) {
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, result);
                    }
                    CFReleaseNull(escrow_label);
                }
                break;
            case kSecXPCOpGetEscrowRecord:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    CFDictionaryRef record =  SOSCCCopyEscrowRecord_Server(&error);
                    if (record) {
                        xpc_object_t xpc_dictionary = _CFXPCCreateXPCObjectFromCFObject(record);
                        xpc_dictionary_set_value(replyMessage, kSecXPCKeyResult, xpc_dictionary);
                        xpc_release(xpc_dictionary);
                    }
                    CFReleaseNull(record);
                }
                break;
            case kSecXPCOpCopyBackupInformation:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    CFDictionaryRef record =  SOSCCCopyBackupInformation_Server(&error);
                    if (record) {
                        xpc_object_t xpc_dictionary = _CFXPCCreateXPCObjectFromCFObject(record);
                        xpc_dictionary_set_value(replyMessage, kSecXPCKeyResult, xpc_dictionary);
                        xpc_release(xpc_dictionary);
                    }
                    CFReleaseNull(record);
                }
                break;

            case kSecXPCOpCheckPeerAvailability:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, SOSCCCheckPeerAvailability_Server(&error));
                }
                break;
                    
                    
            case kSecXPCOpIsThisDeviceLastBackup:
                if (EntitlementPresentOrWhine(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, SOSCCkSecXPCOpIsThisDeviceLastBackup_Server(&error));
                }
                break;
            case kSecXPCOpPeersHaveViewsEnabled:
                {
                    CFArrayRef viewSet = SecXPCDictionaryCopyArray(event, kSecXPCKeyArray, &error);
                    if (viewSet) {
                        CFBooleanRef result = SOSCCPeersHaveViewsEnabled_Server(viewSet, &error);
                        if (result != NULL) {
                            xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, result != kCFBooleanFalse);
                        }
                    }
                    CFReleaseNull(viewSet);
                }
                break;

            case kSecXPCOpWhoAmI:
                {
                    if (client.musr)
                        xpc_dictionary_set_data(replyMessage, "musr", CFDataGetBytePtr(client.musr), CFDataGetLength(client.musr));
                    xpc_dictionary_set_bool(replyMessage, "system-keychain", client.allowSystemKeychain);
                    xpc_dictionary_set_bool(replyMessage, "syncbubble-keychain", client.allowSyncBubbleKeychain);
                    xpc_dictionary_set_bool(replyMessage, "network-extension", client.isNetworkExtension);
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, true);
                }
                break;
            case kSecXPCOpTransmogrifyToSyncBubble:
                {
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementPrivateKeychainSyncBubble, &error)) {
#if TARGET_OS_IOS
                        uid_t uid = (uid_t)xpc_dictionary_get_int64(event, "uid");
                        CFArrayRef services = SecXPCDictionaryCopyArray(event, "services", &error);
                        bool res = false;
                        if (uid && services) {
                            res = _SecServerTransmogrifyToSyncBubble(services, uid, &client, &error);
                        }
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, res);
                        CFReleaseNull(services);
#else
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, false);
#endif
                    }
                }
                break;
            case kSecXPCOpTransmogrifyToSystemKeychain:
                {
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementPrivateKeychainMigrateSystemKeychain, &error)) {
#if TARGET_OS_IOS
                        bool res = _SecServerTransmogrifyToSystemKeychain(&client, &error);
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, res);
#else
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, false);
#endif

                    }
                }
                break;
            case kSecXPCOpDeleteUserView:
                {
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementPrivateKeychainMigrateSystemKeychain, &error)) {
                        bool res = false;
#if TARGET_OS_IOS
                        uid_t uid = (uid_t)xpc_dictionary_get_int64(event, "uid");
                        if (uid) {
                            res = _SecServerDeleteMUSERViews(&client, uid, &error);
                        }
#endif
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, res);

                    }
                }
                break;
            case kSecXPCOpWrapToBackupSliceKeyBagForView:
                {
                    CFStringRef viewname = SecXPCDictionaryCopyString(event, kSecXPCKeyViewName, &error);
                    if(viewname) {
                        CFDataRef plaintext = SecXPCDictionaryCopyData(event, kSecXPCData, &error);
                        if (plaintext) {
                            CFDataRef ciphertext = NULL;
                            CFDataRef bskbEncoded = NULL;

                            bool result = SOSWrapToBackupSliceKeyBagForView_Server(viewname, plaintext, &ciphertext, &bskbEncoded, &error);
                            xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, result);

                            if(!error && result) {
                                if(ciphertext) {
                                    xpc_dictionary_set_data(replyMessage, kSecXPCData, CFDataGetBytePtr(ciphertext), CFDataGetLength(ciphertext));
                                }
                                if(bskbEncoded) {
                                    xpc_dictionary_set_data(replyMessage, kSecXPCKeyKeybag, CFDataGetBytePtr(bskbEncoded), CFDataGetLength(bskbEncoded));
                                }
                            }
                            CFReleaseSafe(ciphertext);
                            CFReleaseSafe(bskbEncoded);
                        }
                        CFReleaseSafe(plaintext);
                    }
                    CFReleaseNull(viewname);
                }
                break;
            case kSecXPCOpCopyApplication:
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementCircleJoin, &error)) {
                        SOSPeerInfoRef peerInfo = SOSCCCopyApplication_Server(&error);
                        CFDataRef peerInfoData = peerInfo ? SOSPeerInfoCopyEncodedData(peerInfo, kCFAllocatorDefault, &error) : NULL;
                        CFReleaseNull(peerInfo);
                        if (peerInfoData) {
                            xpc_object_t xpc_object = _CFXPCCreateXPCObjectFromCFObject(peerInfoData);
                            xpc_dictionary_set_value(replyMessage, kSecXPCKeyResult, xpc_object);
                            xpc_release(xpc_object);
                        }
                        CFReleaseNull(peerInfoData);
                    }
                break;
            case kSecXPCOpCopyCircleJoiningBlob:
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementCircleJoin, &error)) {
                        CFDataRef appBlob = SecXPCDictionaryCopyCFDataRef(event, kSecXPCData, &error);
                        SOSPeerInfoRef applicant = SOSPeerInfoCreateFromData(kCFAllocatorDefault, &error, appBlob);
                        CFDataRef pbblob = SOSCCCopyCircleJoiningBlob_Server(applicant, &error);
                        if (pbblob) {
                            xpc_object_t xpc_object = _CFXPCCreateXPCObjectFromCFObject(pbblob);
                            xpc_dictionary_set_value(replyMessage, kSecXPCKeyResult, xpc_object);
                            xpc_release(xpc_object);
                        }
                        CFReleaseNull(pbblob);
                        CFReleaseNull(applicant);
                        CFReleaseNull(appBlob);
                    }
                break;
            case kSecXPCOpCopyInitialSyncBlob:
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementCircleJoin, &error)) {
                        CFDataRef initialblob = SOSCCCopyInitialSyncData_Server(&error);
                        if (initialblob) {
                            xpc_object_t xpc_object = _CFXPCCreateXPCObjectFromCFObject(initialblob);
                            xpc_dictionary_set_value(replyMessage, kSecXPCKeyResult, xpc_object);
                            xpc_release(xpc_object);
                        }
                        CFReleaseNull(initialblob);
                    }
                    break;
            case kSecXPCOpJoinWithCircleJoiningBlob:
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementCircleJoin, &error)) {
                        CFDataRef joiningBlob = SecXPCDictionaryCopyCFDataRef(event, kSecXPCData, &error);
                        uint64_t version = xpc_dictionary_get_uint64(event, kSecXPCVersion);
                        bool retval = SOSCCJoinWithCircleJoiningBlob_Server(joiningBlob, (PiggyBackProtocolVersion) version, &error);
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, retval);
                        CFReleaseNull(joiningBlob);
                    }
                    break;
                    
            case kSecXPCOpKVSKeyCleanup:
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {                        
                        bool retval = SOSCCCleanupKVSKeys_Server(&error);
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, retval);
                    }
                    break;
            case kSecXPCOpPopulateKVS:
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainCloudCircle, &error)) {
                        bool retval = SOSCCTestPopulateKVSWithBadKeys_Server(&error);
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, retval);
                    }
                    break;
            case kSecXPCOpMessageFromPeerIsPending:
                {
                    SOSPeerInfoRef peer = SecXPCDictionaryCopyPeerInfo(event, kSecXPCKeyPeerInfo, &error);
                    if (peer) {
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                                SOSCCMessageFromPeerIsPending_Server(peer, &error));
                    }
                    CFReleaseNull(peer);
                    break;
                }
            case kSecXPCOpSendToPeerIsPending:
                {
                    SOSPeerInfoRef peer = SecXPCDictionaryCopyPeerInfo(event, kSecXPCKeyPeerInfo, &error);
                    if (peer) {
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult,
                                                SOSCCSendToPeerIsPending(peer, &error));
                    }
                    CFReleaseNull(peer);
                    break;
                }
            case sec_delete_items_with_access_groups_id:
                {
                    bool retval = false;
#if TARGET_OS_IPHONE
                    if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementPrivateUninstallDeletion, &error)) {
                        CFArrayRef accessGroups = SecXPCDictionaryCopyArray(event, kSecXPCKeyAccessGroups, &error);

                        if (accessGroups) {
                            retval = _SecItemServerDeleteAllWithAccessGroups(accessGroups, &client, &error);
                        }
                        CFReleaseNull(accessGroups);
                    }
#endif
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, retval);
                }
                break;
            case sec_item_copy_parent_certificates_id:
                {
                    CFArrayRef results = NULL;
                    if(EntitlementPresentAndTrue(operation, client.task, kSecEntitlementPrivateCertificateAllAccess, &error)) {
                        CFDataRef issuer = SecXPCDictionaryCopyData(event, kSecXPCKeyNormalizedIssuer, &error);
                        CFArrayRef accessGroups = SecXPCDictionaryCopyArray(event, kSecXPCKeyAccessGroups, &error);
                        if (issuer && accessGroups) {
                            results = _SecItemCopyParentCertificates(issuer, accessGroups, &error);
                        }
                        CFReleaseNull(issuer);
                        CFReleaseNull(accessGroups);
                    }
                    SecXPCDictionarySetPListOptional(replyMessage, kSecXPCKeyResult, results, &error);
                    CFReleaseNull(results);
                }
                break;
            case sec_item_certificate_exists_id:
                {
                    bool result = false;
                    if(EntitlementPresentAndTrue(operation, client.task, kSecEntitlementPrivateCertificateAllAccess, &error)) {
                        CFDataRef issuer = SecXPCDictionaryCopyData(event, kSecXPCKeyNormalizedIssuer, &error);
                        CFDataRef serialNum = SecXPCDictionaryCopyData(event, kSecXPCKeySerialNumber, &error);
                        CFArrayRef accessGroups = SecXPCDictionaryCopyArray(event, kSecXPCKeyAccessGroups, &error);
                        if (issuer && serialNum && accessGroups) {
                            result = _SecItemCertificateExists(issuer, serialNum, accessGroups, &error);
                        }
                        CFReleaseNull(issuer);
                        CFReleaseNull(serialNum);
                        CFReleaseNull(accessGroups);
                    }
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, result);
                }
                break;
            case kSecXPCOpBackupKeybagAdd: {
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementBackupTableOperations, &error)) {
                    CFDataRef keybag = NULL, passcode = NULL;
                    if (SecXPCDictionaryCopyDataOptional(event, kSecXPCKeyUserPassword, &passcode, &error)) {
                        CFDataRef identifier = NULL;
                        CFDataRef pathinfo = NULL;   // really a CFURLRef
                        bool added = _SecServerBackupKeybagAdd(&client, passcode, &identifier, &pathinfo, &error);
                        if (added) {
                            added &= SecXPCDictionarySetDataOptional(replyMessage, kSecXPCKeyBackupKeybagIdentifier, identifier, &error);
                            added &= SecXPCDictionarySetDataOptional(replyMessage, kSecXPCKeyBackupKeybagPath, pathinfo, &error);
                            SecXPCDictionarySetBool(replyMessage, kSecXPCKeyResult, added, NULL);
                        } else {
                            xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, false);
                        }
                    }
                    CFReleaseSafe(passcode);
                    CFReleaseSafe(keybag);
                }
                break;
            }
            case kSecXPCOpBackupKeybagDelete: {
                // >>>
                if (EntitlementPresentAndTrue(operation, client.task, kSecEntitlementBackupTableOperations, &error)) {
                    bool deleted = false;
                    CFDictionaryRef query = SecXPCDictionaryCopyDictionary(event, kSecXPCKeyQuery, &error);
                    if (query) {
                        CFTypeRef matchLimit = CFDictionaryGetValue(query, kSecMatchLimit);
                        bool deleteAll = matchLimit && CFEqualSafe(matchLimit, kSecMatchLimitAll);
                        
                        if (deleteAll && !EntitlementPresentAndTrue(operation, client.task, kSecEntitlementBackupTableOperationsDeleteAll, &error)) {
                            // require special entitlement to delete all backup keybags
                        } else {
                            CFMutableDictionaryRef attributes = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, query);
                            CFStringRef requestedAgrp = CFDictionaryGetValue(attributes, kSecAttrAccessGroup);
                            CFStringRef resolvedAgrp = NULL;
                            if (client.musr) {
                                CFDictionarySetValue(attributes, kSecAttrMultiUser, client.musr);
                            }
                            if (extractAccessGroup(&client, requestedAgrp, &resolvedAgrp, &error)) {
                                if (resolvedAgrp) {
                                    CFDictionarySetValue(attributes, kSecAttrAccessGroup, resolvedAgrp);
                                }
                                deleted = _SecServerBackupKeybagDelete(attributes, deleteAll, &error);
                            }
                            CFReleaseNull(attributes);
                        }
                    }
                    xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, deleted);
                    CFReleaseNull(query);
                }
                break;
            }
            case kSecXPCOpKeychainControlEndpoint: {
                if(EntitlementPresentAndTrue(operation, client.task, kSecEntitlementKeychainControl, &error)) {
                    xpc_endpoint_t endpoint = SecServerCreateKeychainControlEndpoint();
                    if (endpoint) {
                        xpc_dictionary_set_value(replyMessage, kSecXPCKeyEndpoint, endpoint);
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, true);
                        xpc_release(endpoint);
                    } else {
                        xpc_dictionary_set_bool(replyMessage, kSecXPCKeyResult, false);
                    }
                }
                break;
            }
            default:
				break;
            }

        if (error)
        {
            if(SecErrorGetOSStatus(error) == errSecItemNotFound || isSOSErrorCoded(error, kSOSErrorPublicKeyAbsent))
                secdebug("ipc", "%@ %@ %@", client.task, SOSCCGetOperationDescription((enum SecXPCOperation)operation), error);
            else if (SecErrorGetOSStatus(error) == errSecAuthNeeded)
                secwarning("Authentication is needed %@ %@ %@", client.task, SOSCCGetOperationDescription((enum SecXPCOperation)operation), error);
            else
                secerror("%@ %@ %@", client.task, SOSCCGetOperationDescription((enum SecXPCOperation)operation), error);

            xpcError = SecCreateXPCObjectWithCFError(error);
            if (replyMessage) {
                xpc_dictionary_set_value(replyMessage, kSecXPCKeyError, xpcError);
            }
        } else if (replyMessage) {
            secdebug("ipc", "%@ %@ responding %@", client.task, SOSCCGetOperationDescription((enum SecXPCOperation)operation), replyMessage);
        }
    } else {
        SecCFCreateErrorWithFormat(kSecXPCErrorUnexpectedType, sSecXPCErrorDomain, NULL, &error, 0, CFSTR("Messages expect to be xpc dictionary, got: %@"), event);
        secerror("%@: returning error: %@", client.task, error);
        xpcError = SecCreateXPCObjectWithCFError(error);
        replyMessage = xpc_create_reply_with_format(event, "{%string: %value}", kSecXPCKeyError, xpcError);
    }

    if (replyMessage) {
        xpc_connection_send_message(connection, replyMessage);
        xpc_release(replyMessage);
    }
    if (xpcError)
        xpc_release(xpcError);
#if TARGET_OS_IPHONE
    pthread_setspecific(taskThreadKey, NULL);
#endif
    CFReleaseSafe(error);
    CFReleaseSafe(client.accessGroups);
    CFReleaseSafe(client.musr);
    CFReleaseSafe(client.task);
    CFReleaseSafe(domains);
    CFReleaseSafe(clientAuditToken);
}

static void securityd_xpc_init(const char *service_name)
{
#if TARGET_OS_IPHONE
    pthread_key_create(&taskThreadKey, NULL);
    SecTaskDiagnoseEntitlements = secTaskDiagnoseEntitlements;
#endif

    secdebug("serverxpc", "start");
    xpc_connection_t listener = xpc_connection_create_mach_service(service_name, NULL, XPC_CONNECTION_MACH_SERVICE_LISTENER);
    if (!listener) {
        seccritical("security failed to register xpc listener for %s, exiting", service_name);
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

#if OCTAGON
    xpc_activity_register("com.apple.securityd.daily", XPC_ACTIVITY_CHECK_IN, ^(xpc_activity_t activity) {
        xpc_activity_state_t activityState = xpc_activity_get_state(activity);
        if (activityState == XPC_ACTIVITY_STATE_RUN) {
            SecCKKS24hrNotification();
        }
    });
#endif
}


// <rdar://problem/22425706> 13B104+Roots:Device never moved past spinner after using approval to ENABLE icdp

#if TARGET_OS_EMBEDDED
static void securityd_soscc_lock_hack() {
	dispatch_queue_t		soscc_lock_queue = dispatch_queue_create("soscc_lock_queue", DISPATCH_QUEUE_PRIORITY_DEFAULT);
	int 					soscc_tok;

	// <rdar://problem/22500239> Prevent securityd from quitting while holding a keychain assertion
	// FIXME: securityd isn't currently registering for any other notifyd events. If/when it does,
	//        this code will need to be generalized / migrated away from just this specific purpose.
	xpc_set_event_stream_handler("com.apple.notifyd.matching", dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^(xpc_object_t object) {
		char *event_description = xpc_copy_description(object);
		secnotice("events", "%s", event_description);
		free(event_description);
	});

    secnotice("lockassertion", "notify_register_dispatch(kSOSCCHoldLockForInitialSync)");
    notify_register_dispatch(kSOSCCHoldLockForInitialSync, &soscc_tok, soscc_lock_queue, ^(int token __unused) {
        secnotice("lockassertion", "kSOSCCHoldLockForInitialSync: grabbing the lock");
        CFErrorRef error = NULL;

        uint64_t one_minute = 60ull;
        if(SecAKSLockUserKeybag(one_minute, &error)){
            // <rdar://problem/22500239> Prevent securityd from quitting while holding a keychain assertion
            xpc_transaction_begin();

            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, one_minute*NSEC_PER_SEC), soscc_lock_queue, ^{
                CFErrorRef localError = NULL;
                if(!SecAKSUnLockUserKeybag(&localError))
                    secerror("failed to unlock: %@", localError);
                CFReleaseNull(localError);
                xpc_transaction_end();
            });
		} else {
			secerror("Failed to take device lock assertion: %@", error);
		}
		CFReleaseNull(error);
		secnotice("lockassertion", "kSOSCCHoldLockForInitialSync => done");
	});
}
#endif

#if TARGET_OS_OSX

static char *
homedirPath(void)
{
    static char homeDir[PATH_MAX] = {};

    if (homeDir[0] == '\0') {
        struct passwd* pwd = getpwuid(getuid());
        if (pwd == NULL)
            return NULL;

        if (realpath(pwd->pw_dir, homeDir) == NULL) {
            strlcpy(homeDir, pwd->pw_dir, sizeof(homeDir));
        }
    }
    return homeDir;
}
#endif


int main(int argc, char *argv[])
{
    char *wait4debugger = getenv("WAIT4DEBUGGER");
    if (wait4debugger && !strcasecmp("YES", wait4debugger)) {
		seccritical("SIGSTOPing self, awaiting debugger");
		kill(getpid(), SIGSTOP);
		seccritical("Again, for good luck (or bad debuggers)");
		kill(getpid(), SIGSTOP);
	}

/* <rdar://problem/15792007> Users with network home folders are unable to use/save password for Mail/Cal/Contacts/websites
 Secd doesn't realize DB connections get invalidated when network home directory users logout
 and their home gets unmounted. Exit secd, start fresh when user logs back in.
*/
#if TARGET_OS_OSX
    int sessionstatechanged_tok;
    notify_register_dispatch(kSA_SessionStateChangedNotification, &sessionstatechanged_tok, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(int token __unused) {
        // we could be a process running as root.
        // However, since root never logs out this isn't an issue.
        if (SASSessionStateForUser(getuid()) == kSA_state_loggingout_pointofnoreturn) {
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 3ull*NSEC_PER_SEC), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
                xpc_transaction_exit_clean();
            });
        }
    });
#endif

#if TARGET_OS_OSX
#define SECD_PROFILE_NAME "com.apple.secd"
    const char *homedir = homedirPath();
    if (homedir == NULL)
        errx(1, "failed to get home directory for secd");

    char *errorbuf = NULL;
    const char	*sandbox_params[] = {
        "_HOME", homedir,
        NULL
    };
    int32_t		rc;

    rc = sandbox_init_with_parameters(SECD_PROFILE_NAME, SANDBOX_NAMED, sandbox_params, &errorbuf);
    if (rc)
        err(1, "Failed to process in a sandbox: %d %s", rc, errorbuf);
#endif /* TARGET_OS_OSX */

    const char *serviceName = kSecuritydXPCServiceName;
    /* setup SQDLite before some other component have a chance to create a database connection */
    _SecDbServerSetup();

    securityd_init_server();
    securityd_xpc_init(serviceName);

    SecCreateSecuritydXPCServer();
    CKKSControlServerInitialize();
    SOSControlServerInitialize();
    OctagonControlServerInitialize();
    
	// <rdar://problem/22425706> 13B104+Roots:Device never moved past spinner after using approval to ENABLE icdp
#if TARGET_OS_EMBEDDED
	securityd_soscc_lock_hack();
#endif

    dispatch_main();
}

/* vi:set ts=4 sw=4 et: */
