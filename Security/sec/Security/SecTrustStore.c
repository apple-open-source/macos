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

static CFStringRef kSecTrustStoreUserName = CFSTR("user");

SecTrustStoreRef SecTrustStoreForDomain(SecTrustStoreDomain domain) {
    CFStringRef domainName;
    if (domain == kSecTrustStoreDomainUser) {
        domainName = kSecTrustStoreUserName;
    } else {
        return NULL;
    }

    if (gSecurityd) {
        return gSecurityd->sec_trust_store_for_domain(domainName);
    } else {
        return (SecTrustStoreRef)domainName;
    }
}

Boolean SecTrustStoreContains(SecTrustStoreRef ts,
	SecCertificateRef certificate) {
    CFDataRef digest;
	bool contains = false;

	require(ts, errOut);
	require(digest = SecCertificateGetSHA1Digest(certificate), errOut);
    if (gSecurityd) {
        contains = gSecurityd->sec_trust_store_contains(ts, digest);
    } else {
        const void *values[] = {
            (const void *)ts,
            (const void *)digest
        };
        CFArrayRef in = CFArrayCreate(kCFAllocatorDefault, values,
            2, &kCFTypeArrayCallBacks);
        OSStatus status;
        if (in) {
            status = ServerCommandSendReceive(sec_trust_store_contains_id,
                in, NULL);
            CFRelease(in);
        } else {
            status = errSecAllocate;
        }
        contains = !status;
    }

errOut:
	return contains;
}

OSStatus SecTrustStoreSetTrustSettings(SecTrustStoreRef ts,
	SecCertificateRef certificate,
    CFTypeRef trustSettingsDictOrArray) {
    CFDataRef certificateData = NULL;
    OSStatus status = errSecParam;

    if (gSecurityd) {
        status = gSecurityd->sec_trust_store_set_trust_settings(ts, certificate, trustSettingsDictOrArray);
    } else {
        require(ts == (SecTrustStoreRef)kSecTrustStoreUserName, errOut);
        require(certificateData = SecCertificateCopyData(certificate), errOut);
        const void *values[] = {
            (const void *)certificateData,
            (const void *)trustSettingsDictOrArray
        };
        CFArrayRef in = CFArrayCreate(kCFAllocatorDefault, values,
            (trustSettingsDictOrArray ? 2 : 1), &kCFTypeArrayCallBacks);
        if (in) {
            status = ServerCommandSendReceive(sec_trust_store_set_trust_settings_id, in, NULL);
            CFRelease(in);
        } else {
            status = errSecAllocate;
        }
    }

errOut:
    CFReleaseSafe(certificateData);
	return status;
}

OSStatus SecTrustStoreRemoveCertificate(SecTrustStoreRef ts,
    SecCertificateRef certificate)
{
    CFDataRef digest;
    OSStatus status = errSecParam;

	require(digest = SecCertificateGetSHA1Digest(certificate), errOut);
    if (gSecurityd) {
        status = gSecurityd->sec_trust_store_remove_certificate(ts, digest);
    } else {
        require(ts == (SecTrustStoreRef)kSecTrustStoreUserName, errOut);
        status = ServerCommandSendReceive(sec_trust_store_remove_certificate_id, digest, NULL);
    }

errOut:
	return status;
}
