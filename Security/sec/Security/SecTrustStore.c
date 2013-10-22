/*
 * Copyright (c) 2007-2009 Apple Inc. All Rights Reserved.
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
 * SecTrustStore.c - CertificateSource API to a system root certificate store
 */
#include <Security/SecTrustStore.h>

#include <Security/SecCertificateInternal.h>
#include <Security/SecInternal.h>
#include <CoreFoundation/CFString.h>
#include <AssertMacros.h>
#include "securityd_client.h"
#include "SecuritydXPC.h"
#include "SecFramework.h"
#include <sys/stat.h>
#include <stdio.h>
#include <dirent.h>
#include "SecTrustPriv.h"
#include <utilities/SecCFError.h>
#include "utilities/SecDb.h"

static CFStringRef kSecTrustStoreUserName = CFSTR("user");

SecTrustStoreRef SecTrustStoreForDomain(SecTrustStoreDomain domain) {
    CFStringRef domainName;
    if (domain == kSecTrustStoreDomainUser) {
        domainName = kSecTrustStoreUserName;
    } else {
        return NULL;
    }

    if (gSecurityd) {
        return gSecurityd->sec_trust_store_for_domain(domainName, NULL);
    } else {
        return (SecTrustStoreRef)domainName;
    }
}

static bool string_data_to_bool_error(enum SecXPCOperation op, SecTrustStoreRef ts, CFDataRef digest, CFErrorRef *error)
{
    return securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *error) {
        return SecXPCDictionarySetString(message, kSecXPCKeyDomain, (CFStringRef)ts, error) &&
        SecXPCDictionarySetData(message, kSecXPCKeyDigest, digest, error);
    }, NULL);
}

static bool string_data_to_bool_bool_error(enum SecXPCOperation op, SecTrustStoreRef ts, CFDataRef digest, bool *result, CFErrorRef *error)
{
    return securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *error) {
        return SecXPCDictionarySetString(message, kSecXPCKeyDomain, (CFStringRef)ts, error) &&
        SecXPCDictionarySetData(message, kSecXPCKeyDigest, digest, error);
    }, ^bool(xpc_object_t response, CFErrorRef *error) {
        if (result)
            *result = xpc_dictionary_get_bool(response, kSecXPCKeyResult);
        return true;
    });
}

Boolean SecTrustStoreContains(SecTrustStoreRef ts,
	SecCertificateRef certificate) {
    CFDataRef digest;
    bool ok = false;
	__block bool contains = false;

	require(ts, errOut);
	require(digest = SecCertificateGetSHA1Digest(certificate), errOut);
    ok = (SecOSStatusWith(^bool (CFErrorRef *error) {
        return SECURITYD_XPC(sec_trust_store_contains, string_data_to_bool_bool_error, ts, digest, &contains, error);
    }) == errSecSuccess);

errOut:
	return ok && contains;
}

static bool SecXPCDictionarySetCertificate(xpc_object_t message, const char *key, SecCertificateRef certificate, CFErrorRef *error) {
    if (certificate) {
        xpc_dictionary_set_data(message, key, SecCertificateGetBytePtr(certificate),
                                SecCertificateGetLength(certificate));
        return true;
    }
    return SecError(errSecParam, error, CFSTR("NULL certificate"));
}


static bool string_cert_cftype_to_error(enum SecXPCOperation op, SecTrustStoreRef ts, SecCertificateRef certificate, CFTypeRef trustSettingsDictOrArray, CFErrorRef *error)
{
    return securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *error) {
        bool ok = false;
        ok = SecXPCDictionarySetString(message, kSecXPCKeyDomain, (CFStringRef)ts, error) &&
        SecXPCDictionarySetCertificate(message, kSecXPCKeyCertificate, certificate, error) &&
        (!trustSettingsDictOrArray || SecXPCDictionarySetPList(message, kSecXPCKeySettings, trustSettingsDictOrArray, error));
        return ok;
    }, NULL);
}

OSStatus SecTrustStoreSetTrustSettings(SecTrustStoreRef ts,
	SecCertificateRef certificate,
    CFTypeRef trustSettingsDictOrArray) {
    return SecOSStatusWith(^bool (CFErrorRef *error) {
        return SECURITYD_XPC(sec_trust_store_set_trust_settings, string_cert_cftype_to_error, ts, certificate, trustSettingsDictOrArray, error);
    });
}

OSStatus SecTrustStoreRemoveCertificate(SecTrustStoreRef ts,
    SecCertificateRef certificate)
{
    CFDataRef digest;
    OSStatus status = errSecParam;

	require(ts, errOut);
	require(digest = SecCertificateGetSHA1Digest(certificate), errOut);
    require(gSecurityd || ts == (SecTrustStoreRef)kSecTrustStoreUserName, errOut);
    status = SecOSStatusWith(^bool (CFErrorRef *error) {
        return SECURITYD_XPC(sec_trust_store_remove_certificate, string_data_to_bool_error, ts, digest, error);
    });

errOut:
	return status;
}


static CFIndex GetOTAAssetVersionNumber()
{
	CFIndex result = 0;
    int version = 0;

	if (errSecSuccess == SecTrustGetOTAPKIAssetVersionNumber(&version))
	{
		result = version;	
	}
 
    return result;
}



OSStatus SecTrustStoreGetSettingsVersionNumber(SecTrustSettingsVersionNumber* p_settings_version_number)
{
    OSStatus status = errSecParam;
    if (NULL == p_settings_version_number)
    {
        return status;
    }
    	
    CFIndex versionNumber = GetOTAAssetVersionNumber();
    *p_settings_version_number = (SecTrustSettingsVersionNumber)versionNumber;

    return errSecSuccess;
}
