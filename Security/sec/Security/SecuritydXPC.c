//
//  SecuritydXPC.c
//  sec
//
//  Created by Mitch Adler on 11/16/12.
//  Copyright (c) 2012-2013 Apple Inc. All rights reserved.
//

#include "SecuritydXPC.h"
#include <ipc/securityd_client.h>
#include <utilities/SecCFError.h>
#include <utilities/SecDb.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/der_plist.h>

// TODO Shorten these string values to save ipc bandwidth.
const char *kSecXPCKeyOperation = "operation";
const char *kSecXPCKeyResult = "status";
const char *kSecXPCKeyError = "error";
const char *kSecXPCKeyPeerInfos = "peer-infos";
const char *kSecXPCKeyUserLabel = "userlabel";
const char *kSecXPCKeyBackup = "backup";
const char *kSecXPCKeyKeybag = "keybag";
const char *kSecXPCKeyUserPassword = "password";
const char *kSecXPCKeyQuery = "query";
const char *kSecXPCKeyAttributesToUpdate = "attributesToUpdate";
const char *kSecXPCKeyDomain = "domain";
const char *kSecXPCKeyDigest = "digest";
const char *kSecXPCKeyCertificate = "cert";
const char *kSecXPCKeySettings = "settings";
const char *kSecXPCKeyOTAFileDirectory = "path";
const char *kSecXPCLimitInMinutes = "limitMinutes";

//
// XPC Functions for both client and server.
//


CFStringRef SOSCCGetOperationDescription(enum SecXPCOperation op)
{
    switch (op) {
        case sec_item_add_id:
            return CFSTR("add");
        case sec_item_copy_matching_id:
            return CFSTR("copy_matching");
        case sec_item_update_id:
            return CFSTR("update");
        case sec_item_delete_id:
            return CFSTR("delete");
        case sec_trust_store_contains_id:
            return CFSTR("trust_store_contains");
        case sec_trust_store_set_trust_settings_id:
            return CFSTR("trust_store_set_trust_settings");
        case sec_trust_store_remove_certificate_id:
            return CFSTR("trust_store_remove_certificate");
        case sec_delete_all_id:
            return CFSTR("delete_all");
        case sec_trust_evaluate_id:
            return CFSTR("trust_evaluate");
        case sec_keychain_backup_id:
            return CFSTR("keychain_backup");
        case sec_keychain_restore_id:
            return CFSTR("keychain_restore");
        case sec_keychain_sync_update_id:
            return CFSTR("keychain_sync_update");
        case sec_keychain_backup_syncable_id:
            return CFSTR("keychain_backup_syncable");
        case sec_keychain_restore_syncable_id:
            return CFSTR("keychain_restore_syncable");
        case sec_ota_pki_asset_version_id:
            return CFSTR("ota_pki_asset_version");
        case kSecXPCOpTryUserCredentials:
            return CFSTR("TryUserCredentials");
        case kSecXPCOpSetUserCredentials:
            return CFSTR("SetUserCredentials");
        case kSecXPCOpCanAuthenticate:
            return CFSTR("CanAuthenticate");
        case kSecXPCOpPurgeUserCredentials:
            return CFSTR("PurgeUserCredentials");
        case kSecXPCOpDeviceInCircle:
            return CFSTR("DeviceInCircle");
        case kSecXPCOpRequestToJoin:
            return CFSTR("RequestToJoin");
        case kSecXPCOpResetToOffering:
            return CFSTR("ResetToOffering");
        case kSecXPCOpResetToEmpty:
            return CFSTR("ResetToEmpty");
        case kSecXPCOpRemoveThisDeviceFromCircle:
            return CFSTR("RemoveThisDevice");
        case kSecXPCOpBailFromCircle:
            return CFSTR("BailFromCircle");
        case kSecXPCOpAcceptApplicants:
            return CFSTR("AcceptApplicants");
        case kSecXPCOpRejectApplicants:
            return CFSTR("RejectApplicants");
        case kSecXPCOpCopyApplicantPeerInfo:
            return CFSTR("ApplicantPeerInfo");
        case kSecXPCOpCopyPeerPeerInfo:
            return CFSTR("PeerPeerInfo");
        case kSecXPCOpCopyConcurringPeerPeerInfo:
            return CFSTR("ConcurringPeerPeerInfo");
        case kSecXPCOpOTAGetEscrowCertificates:
            return CFSTR("OTAGetEscrowCertificates");
		case kSecXPCOpOTAPKIGetNewAsset:
			return CFSTR("sec_ota_pki_get_new_asset");
        case kSecXPCOpProcessSyncWithAllPeers:
            return CFSTR("ProcessSyncWithAllPeers");
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

bool SecXPCDictionarySetDataOptional(xpc_object_t message, const char *key, CFDataRef data, CFErrorRef *error) {
    return !data || SecXPCDictionarySetData(message, key, data, error);
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
    der = der_decode_plist(kCFAllocatorDefault, kCFPropertyListImmutable,
                                          &cfobject, error, der, der_end);
    if (der != der_end) {
        SecError(errSecParam, error, CFSTR("trailing garbage after der decoded object for key %s"), key);
        CFReleaseNull(cfobject);
    }
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
