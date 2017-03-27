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


#include <Security/SecuritydXPC.h>
#include <Security/SecCFAllocator.h>
#include <ipc/securityd_client.h>
#include <utilities/SecCFError.h>
#include <utilities/SecDb.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/der_plist.h>

// TODO Shorten these string values to save ipc bandwidth.
const char *kSecXPCKeyOperation = "operation";
const char *kSecXPCKeyResult = "status";
const char *kSecXPCKeyError = "error";
const char *kSecXPCKeyClientToken = "client";
const char *kSecXPCKeyPeerInfoArray = "peer-infos";
const char *kSecXPCKeyPeerInfo = "peer-info";
const char *kSecXPCKeyUserLabel = "userlabel";
const char *kSecXPCKeyBackup = "backup";
const char *kSecXPCKeyKeybag = "keybag";
const char *kSecXPCKeyUserPassword = "password";
const char *kSecXPCKeyDSID = "dsid";
const char *kSecXPCKeyQuery = "query";
const char *kSecXPCKeyAttributesToUpdate = "attributesToUpdate";
const char *kSecXPCKeyDomain = "domain";
const char *kSecXPCKeyDigest = "digest";
const char *kSecXPCKeyCertificate = "cert";
const char *kSecXPCKeySettings = "settings";
const char *kSecXPCKeyOTAFileDirectory = "path";
const char *kSecXPCLimitInMinutes = "limitMinutes";
const char *kSecXPCPublicPeerId = "publicPeerId"; // Public peer id
const char *kSecXPCOTRSession = "otrsess"; // OTR session bytes
const char *kSecXPCData = "data"; // Data to process
const char *kSecXPCOTRReady = "otrrdy"; // OTR ready for messages
const char *kSecXPCKeyDeviceID = "deviceID";
const char *kSecXPCKeySendIDSMessage = "sendIDSMessageCommand";
const char *kSecXPCKeyIDSMessage = "idsMessage";
const char *kSecXPCKeyViewName = "viewname";
const char *kSecXPCKeyViewActionCode = "viewactioncode";
const char *kSecXPCKeyHSA2AutoAcceptInfo = "autoacceptinfo";
const char *kSecXPCKeyString = "cfstring";
const char *kSecXPCKeyArray = "cfarray";
const char *kSecXPCKeySet = "cfset";
const char *kSecXPCKeySet2 = "cfset2";
const char *kSecXPCKeyNewPublicBackupKey = "newPublicBackupKey";
const char *kSecXPCKeyRecoveryPublicKey = "RecoveryPublicKey";
const char *kSecXPCKeyIncludeV0 = "includeV0";
const char *kSecXPCKeyReason = "reason";
const char *kSecXPCKeyEnabledViewsKey = "enabledViews";
const char *kSecXPCKeyDisabledViewsKey = "disabledViews";
const char *kSecXPCKeyEscrowLabel = "escrow";
const char *kSecXPCKeyTriesLabel = "tries";
const char *kSecXPCKeyFileDescriptor = "fileDescriptor";
const char *kSecXPCKeyAccessGroups = "accessGroups";
const char *kSecXPCKeyClasses = "classes";


//
// XPC Functions for both client and server.
//


CFStringRef SOSCCGetOperationDescription(enum SecXPCOperation op)
{
    switch (op) {
        case kSecXPCOpAccountSetToNew:
            return CFSTR("AccountSetToNew");
        case kSecXPCOpOTAGetEscrowCertificates:
            return CFSTR("OTAGetEscrowCertificates");
        case kSecXPCOpOTAPKIGetNewAsset:
            return CFSTR("OTAPKIGetNewAsset");
        case kSecXPCOpSetHSA2AutoAcceptInfo:
            return CFSTR("SetHSA2AutoAcceptInfo");
        case kSecXPCOpAcceptApplicants:
            return CFSTR("AcceptApplicants");
        case kSecXPCOpApplyToARing:
            return CFSTR("ApplyToARing");
        case kSecXPCOpBailFromCircle:
            return CFSTR("BailFromCircle");
        case kSecXPCOpCanAuthenticate:
            return CFSTR("CanAuthenticate");
        case kSecXPCOpCopyApplicantPeerInfo:
            return CFSTR("CopyApplicantPeerInfo");
        case kSecXPCOpCopyConcurringPeerPeerInfo:
            return CFSTR("CopyConcurringPeerPeerInfo");
        case kSecXPCOpCopyEngineState:
            return CFSTR("CopyEngineState");
        case kSecXPCOpCopyGenerationPeerInfo:
            return CFSTR("CopyGenerationPeerInfo");
        case kSecXPCOpCopyIncompatibilityInfo:
            return CFSTR("CopyIncompatibilityInfo");
        case kSecXPCOpCopyMyPeerInfo:
            return CFSTR("CopyMyPeerInfo");
        case kSecXPCOpCopyNotValidPeerPeerInfo:
            return CFSTR("CopyNotValidPeerPeerInfo");
        case kSecXPCOpCopyPeerPeerInfo:
            return CFSTR("CopyPeerPeerInfo");
        case kSecXPCOpCopyRetirementPeerInfo:
            return CFSTR("CopyRetirementPeerInfo");
        case kSecXPCOpCopyValidPeerPeerInfo:
            return CFSTR("CopyValidPeerPeerInfo");
        case kSecXPCOpCopyViewUnawarePeerInfo:
            return CFSTR("CopyViewUnawarePeerInfo");
        case kSecXPCOpDeviceInCircle:
            return CFSTR("DeviceInCircle");
        case kSecXPCOpEnableRing:
            return CFSTR("EnableRing");
        case kSecXPCOpGetAllTheRings:
            return CFSTR("GetAllTheRings");
        case kSecXPCOpGetLastDepartureReason:
            return CFSTR("GetLastDepartureReason");
        case kSecXPCOpHandleIDSMessage:
            return CFSTR("HandleIDSMessage");
        case kSecXPCOpSyncWithKVSPeer:
            return CFSTR("SyncKVSPeer");
        case kSecXPCOpSyncWithIDSPeer:
            return CFSTR("SyncIDSPeer");
        case kSecXPCOpIDSDeviceID:
            return CFSTR("IDSDeviceID");
        case kSecXPCOpClearKVSPeerMessage:
            return CFSTR("kSecXPCOpClearKVSPeerMessage");
        case kSecXPCOpLoggedOutOfAccount:
            return CFSTR("LoggedOutOfAccount");
        case kSecXPCOpPingTest:
            return CFSTR("PingTest");
        case kSecXPCOpProcessSyncWithAllPeers:
            return CFSTR("ProcessSyncWithAllPeers");
        case kSecXPCOpProcessSyncWithPeers:
            return CFSTR("ProcessSyncWithPeers");
        case kSecXPCOpProcessUnlockNotification:
            return CFSTR("ProcessUnlockNotification");
        case kSecXPCOpPurgeUserCredentials:
            return CFSTR("PurgeUserCredentials");
        case kSecXPCOpRejectApplicants:
            return CFSTR("RejectApplicants");
        case kSecXPCOpRemoveThisDeviceFromCircle:
            return CFSTR("RemoveThisDeviceFromCircle");
        case kSecXPCOpRemovePeersFromCircle:
            return CFSTR("RemovePeersFromCircle");
        case kSecXPCOpRequestDeviceID:
            return CFSTR("RequestDeviceID");
        case kSecXPCOpRequestEnsureFreshParameters:
            return CFSTR("RequestEnsureFreshParameters");
        case kSecXPCOpRequestToJoin:
            return CFSTR("RequestToJoin");
        case kSecXPCOpRequestToJoinAfterRestore:
            return CFSTR("RequestToJoinAfterRestore");
        case kSecXPCOpResetToEmpty:
            return CFSTR("ResetToEmpty");
        case kSecXPCOpResetToOffering:
            return CFSTR("ResetToOffering");
        case kSecXPCOpRingStatus:
            return CFSTR("RingStatus");
        case kSecXPCOpRollKeys:
            return CFSTR("RollKeys");
        case kSecXPCOpSecurityProperty:
            return CFSTR("SecurityProperty");
        case kSecXPCOpSendIDSMessage:
            return CFSTR("SendIDSMessage");
        case kSecXPCOpSetBagForAllSlices:
            return CFSTR("SetBagForAllSlices");
        case kSecXPCOpSetDeviceID:
            return CFSTR("SetDeviceID");
        case kSecXPCOpSetLastDepartureReason:
            return CFSTR("SetLastDepartureReason");
        case kSecXPCOpSetNewPublicBackupKey:
            return CFSTR("SetNewPublicBackupKey");
        case kSecXPCOpSetUserCredentials:
            return CFSTR("SetUserCredentials");
        case kSecXPCOpSetUserCredentialsAndDSID:
            return CFSTR("SetUserCredentialsAndDSID");
        case kSecXPCOpTryUserCredentials:
            return CFSTR("TryUserCredentials");
        case kSecXPCOpValidateUserPublic:
            return CFSTR("ValidateUserPublic");
        case kSecXPCOpView:
            return CFSTR("View");
        case kSecXPCOpWithdrawlFromARing:
            return CFSTR("WithdrawlFromARing");
        case sec_add_shared_web_credential_id:
            return CFSTR("add_shared_web_credential");
        case sec_copy_shared_web_credential_id:
            return CFSTR("copy_shared_web_credential");
        case sec_delete_all_id:
            return CFSTR("delete_all");
        case sec_get_log_settings_id:
            return CFSTR("get_log_settings");
        case sec_item_add_id:
            return CFSTR("add");
        case sec_item_backup_copy_names_id:
            return CFSTR("backup_copy_names");
        case sec_item_backup_handoff_fd_id:
            return CFSTR("backup_handoff_fd");
        case sec_item_backup_restore_id:
            return CFSTR("backup_restore");
        case sec_item_backup_set_confirmed_manifest_id:
            return CFSTR("backup_set_confirmed_manifest");
        case sec_item_copy_matching_id:
            return CFSTR("copy_matching");
        case sec_item_delete_id:
            return CFSTR("delete");
        case sec_item_update_id: 
            return CFSTR("update");
        case sec_keychain_backup_id: 
            return CFSTR("keychain_backup");
        case sec_keychain_backup_syncable_id: 
            return CFSTR("keychain_backup_syncable");
        case sec_keychain_restore_id: 
            return CFSTR("keychain_restore");
        case sec_keychain_restore_syncable_id: 
            return CFSTR("keychain_restore_syncable");
        case sec_keychain_sync_update_message_id: 
            return CFSTR("keychain_sync_update_message");
        case sec_ota_pki_asset_version_id: 
            return CFSTR("ota_pki_asset_version");
        case sec_otr_session_create_remote_id: 
            return CFSTR("otr_session_create_remote");
        case sec_otr_session_process_packet_remote_id: 
            return CFSTR("otr_session_process_packet_remote");
        case sec_set_circle_log_settings_id: 
            return CFSTR("set_circle_log_settings");
        case sec_set_xpc_log_settings_id: 
            return CFSTR("set_xpc_log_settings");
        case sec_trust_evaluate_id: 
            return CFSTR("trust_evaluate");
        case sec_trust_store_contains_id: 
            return CFSTR("trust_store_contains");
        case sec_trust_store_remove_certificate_id: 
            return CFSTR("trust_store_remove_certificate");
        case sec_trust_store_set_trust_settings_id: 
            return CFSTR("trust_store_set_trust_settings");
        case sec_trust_store_copy_all_id:
            return CFSTR("trust_store_copy_all");
        case sec_trust_store_copy_usage_constraints_id:
            return CFSTR("trust_store_copy_usage_constraints");
        case soscc_EnsurePeerRegistration_id:
            return CFSTR("EnsurePeerRegistration");
        case kSecXPCOpSetEscrowRecord:
            return CFSTR("SetEscrowRecord");
        case kSecXPCOpGetEscrowRecord:
            return CFSTR("GetEscrowRecord");
        case kSecXPCOpWhoAmI:
            return CFSTR("WhoAmI");
        case kSecXPCOpTransmogrifyToSyncBubble:
            return CFSTR("TransmogrifyToSyncBubble");
        case kSecXPCOpWrapToBackupSliceKeyBagForView:
            return CFSTR("WrapToBackupSliceKeyBagForView");
        case kSecXPCOpCopyAccountData:
            return CFSTR("CopyAccountDataFromKeychain");
        case kSecXPCOpDeleteAccountData:
            return CFSTR("DeleteAccountDataFromKeychain");
        case kSecXPCOpCopyEngineData:
            return CFSTR("CopyEngineDataFromKeychain");
        case kSecXPCOpDeleteEngineData:
            return CFSTR("DeleteEngineDataFromKeychain");
        case sec_item_update_token_items_id:
            return CFSTR("UpdateTokenItems");
        case sec_delete_items_with_access_groups_id:
            return CFSTR("sec_delete_items_with_access_groups_id");
        case kSecXPCOpPeersHaveViewsEnabled:
            return CFSTR("kSecXPCOpPeersHaveViewsEnabled");
        case kSecXPCOpRegisterRecoveryPublicKey:
            return CFSTR("RegisterRecoveryPublicKey");
        case kSecXPCOpGetRecoveryPublicKey:
            return CFSTR("GetRecoveryPublicKey");
        case kSecXPCOpCopyBackupInformation:
            return CFSTR("CopyBackupInformation");
        case sec_device_is_internal_id:
            return CFSTR("DeviceIsInternal");
        case kSecXPCOpMessageFromPeerIsPending:
            return CFSTR("MessageFromPeerIsPending");
        case kSecXPCOpSendToPeerIsPending:
            return CFSTR("SendToPeerIsPending");
        default:
            return CFSTR("Unknown xpc operation");
    }
}

bool SecXPCDictionarySetPList(xpc_object_t message, const char *key, CFTypeRef object, CFErrorRef *error)
{
    if (!object)
        return SecError(errSecParam, error, CFSTR("object for key %s is NULL"), key);

    size_t size = der_sizeof_plist(object, error);
    if (!size)
        return false;
    uint8_t *der = malloc(size);
    uint8_t *der_end = der + size;
    uint8_t *der_start = der_encode_plist(object, error, der, der_end);
    if (!der_start) {
        free(der);
        return false;
    }

    assert(der == der_start);
    xpc_dictionary_set_data(message, key, der_start, der_end - der_start);
    free(der);
    return true;
}

bool SecXPCDictionarySetPListOptional(xpc_object_t message, const char *key, CFTypeRef object, CFErrorRef *error) {
    return !object || SecXPCDictionarySetPList(message, key, object, error);
}

bool SecXPCDictionarySetData(xpc_object_t message, const char *key, CFDataRef data, CFErrorRef *error)
{
    if (!data)
        return SecError(errSecParam, error, CFSTR("data for key %s is NULL"), key);

    xpc_dictionary_set_data(message, key, CFDataGetBytePtr(data), CFDataGetLength(data));
    return true;
}

bool SecXPCDictionarySetBool(xpc_object_t message, const char *key, bool value, CFErrorRef *error)
{
    xpc_dictionary_set_bool(message, key, value);
    return true;
}

bool SecXPCDictionarySetString(xpc_object_t message, const char *key, CFStringRef string, CFErrorRef *error)
{
    if (!string)
        return SecError(errSecParam, error, CFSTR("string for key %s is NULL"), key);

    __block bool ok = true;
    CFStringPerformWithCString(string, ^(const char *utf8Str) {
        if (utf8Str)
            xpc_dictionary_set_string(message, key, utf8Str);
        else
            ok = SecError(errSecParam, error, CFSTR("failed to convert string for key %s to utf8"), key);
    });
    return ok;
}

bool SecXPCDictionarySetStringOptional(xpc_object_t message, const char *key, CFStringRef string, CFErrorRef *error) {
    return !string || SecXPCDictionarySetString(message, key, string, error);
}

bool SecXPCDictionarySetDataOptional(xpc_object_t message, const char *key, CFDataRef data, CFErrorRef *error) {
    return !data || SecXPCDictionarySetData(message, key, data, error);
}

bool SecXPCDictionarySetInt64(xpc_object_t message, const char *key, int64_t value, CFErrorRef *error) {
    xpc_dictionary_set_int64(message, key, value);
    return true;
}

bool SecXPCDictionarySetFileDescriptor(xpc_object_t message, const char *key, int fd, CFErrorRef *error) {
    xpc_dictionary_set_fd(message, key, fd);
    return true;
}

int SecXPCDictionaryDupFileDescriptor(xpc_object_t message, const char *key, CFErrorRef *error) {
    int fd = xpc_dictionary_dup_fd(message, key);
    if (fd < 0)
        SecError(errSecParam, error, CFSTR("missing fd for key %s"), key);

    return fd;
}

CFSetRef SecXPCDictionaryCopySet(xpc_object_t message, const char *key, CFErrorRef *error) {
    CFTypeRef obj = SecXPCDictionaryCopyPList(message, key, error);
    CFSetRef set = copyIfSet(obj, error);
    if (obj && !set) {
        CFStringRef description = CFCopyTypeIDDescription(CFGetTypeID(obj));
        SecError(errSecParam, error, CFSTR("object for key %s not set but %@"), key, description);
        CFReleaseNull(description);
    }
    CFReleaseNull(obj);
    return set;
}

CFArrayRef SecXPCDictionaryCopyArray(xpc_object_t message, const char *key, CFErrorRef *error) {
    CFTypeRef array = SecXPCDictionaryCopyPList(message, key, error);
    if (array) {
        CFTypeID type_id = CFGetTypeID(array);
        if (type_id != CFArrayGetTypeID()) {
            CFStringRef description = CFCopyTypeIDDescription(type_id);
            SecError(errSecParam, error, CFSTR("object for key %s not array but %@"), key, description);
            CFReleaseNull(description);
            CFReleaseNull(array);
        }
    }
    return (CFArrayRef)array;
}

bool SecXPCDictionaryCopyArrayOptional(xpc_object_t message, const char *key, CFArrayRef *parray, CFErrorRef *error) {
    if (!xpc_dictionary_get_value(message, key)) {
        *parray = NULL;
        return true;
    }
    *parray = SecXPCDictionaryCopyArray(message, key, error);
    return *parray;
}

CFDataRef SecXPCDictionaryCopyData(xpc_object_t message, const char *key, CFErrorRef *error) {
    CFDataRef data = NULL;
    size_t size = 0;
    const uint8_t *bytes = xpc_dictionary_get_data(message, key, &size);
    if (!bytes) {
        SecError(errSecParam, error, CFSTR("no data for key %s"), key);
        return NULL;
    }

    data = CFDataCreate(kCFAllocatorDefault, bytes, size);
    if (!data)
        SecError(errSecParam, error, CFSTR("failed to create data for key %s"), key);

    return data;
}

bool SecXPCDictionaryGetBool(xpc_object_t message, const char *key, CFErrorRef *__unused error) {
    return xpc_dictionary_get_bool(message, key);
}

bool SecXPCDictionaryCopyDataOptional(xpc_object_t message, const char *key, CFDataRef *pdata, CFErrorRef *error) {
    size_t size = 0;
    if (!xpc_dictionary_get_data(message, key, &size)) {
        *pdata = NULL;
        return true;
    }
    *pdata = SecXPCDictionaryCopyData(message, key, error);
    return *pdata;
}

CFDictionaryRef SecXPCDictionaryCopyDictionary(xpc_object_t message, const char *key, CFErrorRef *error) {
    CFTypeRef dict = SecXPCDictionaryCopyPList(message, key, error);
    if (dict) {
        CFTypeID type_id = CFGetTypeID(dict);
        if (type_id != CFDictionaryGetTypeID()) {
            CFStringRef description = CFCopyTypeIDDescription(type_id);
            SecError(errSecParam, error, CFSTR("object for key %s not dictionary but %@"), key, description);
            CFReleaseNull(description);
            CFReleaseNull(dict);
        }
    }
    return (CFDictionaryRef)dict;
}

bool SecXPCDictionaryCopyDictionaryOptional(xpc_object_t message, const char *key, CFDictionaryRef *pdictionary, CFErrorRef *error) {
    if (!xpc_dictionary_get_value(message, key)) {
        *pdictionary = NULL;
        return true;
    }
    *pdictionary = SecXPCDictionaryCopyDictionary(message, key, error);
    return *pdictionary;
}

CFTypeRef SecXPCDictionaryCopyPList(xpc_object_t message, const char *key, CFErrorRef *error)
{
    CFTypeRef cfobject = NULL;
    size_t size = 0;
    const uint8_t *der = xpc_dictionary_get_data(message, key, &size);
    if (!der) {
        SecError(errSecParam, error, CFSTR("no object for key %s"), key);
        return NULL;
    }

    const uint8_t *der_end = der + size;
    /* use the sensitive allocator so that the dictionary is zeroized upon deallocation */
    const uint8_t *decode_end = der_decode_plist(SecCFAllocatorZeroize(), kCFPropertyListImmutable,
                                          &cfobject, error, der, der_end);
    if (decode_end != der_end) {
        SecError(errSecParam, error, CFSTR("trailing garbage after der decoded object for key %s"), key);
        CFReleaseNull(cfobject);
    }

    /* zeroize xpc value as it may have contained raw key material */
    cc_clear(size, (void *)der);

    return cfobject;
}

bool SecXPCDictionaryCopyPListOptional(xpc_object_t message, const char *key, CFTypeRef *pobject, CFErrorRef *error) {
    size_t size = 0;
    if (!xpc_dictionary_get_data(message, key, &size)) {
        *pobject = NULL;
        return true;
    }
    *pobject = SecXPCDictionaryCopyPList(message, key, error);
    return *pobject;
}

CFStringRef SecXPCDictionaryCopyString(xpc_object_t message, const char *key, CFErrorRef *error) {
    const char *string = xpc_dictionary_get_string(message, key);
    if (string) {
        CFStringRef result = CFStringCreateWithCString(kCFAllocatorDefault, string, kCFStringEncodingUTF8);
        if (!result) {
            SecError(errSecAllocate, error, CFSTR("object for key %s failed to convert %s to CFString"), key, string);
        }
        return result;
    } else {
        SecError(errSecParam, error, CFSTR("object for key %s not string"), key);
        return NULL;
    }
}

bool SecXPCDictionaryCopyStringOptional(xpc_object_t message, const char *key, CFStringRef *pstring, CFErrorRef *error) {
    if (!xpc_dictionary_get_value(message, key)) {
        *pstring = NULL;
        return true;
    }
    *pstring = SecXPCDictionaryCopyString(message, key, error);
    return *pstring;
}
