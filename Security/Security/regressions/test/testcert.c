/*
 * Copyright (c) 2009,2012-2014 Apple Inc. All Rights Reserved.
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
 *
 * testcert.c
 */

#include <TargetConditionals.h>

#if TARGET_OS_IPHONE

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecItem.h>
#include <Security/SecCertificateRequest.h>
#include <Security/SecInternal.h>
#include <utilities/array_size.h>

#include <AssertMacros.h>

#include "testcert.h"

static inline CF_RETURNS_RETAINED CFMutableArrayRef maa(CFMutableArrayRef array CF_CONSUMED, CFTypeRef a CF_CONSUMED) {
	CFMutableArrayRef ma = array;
	if (!ma)
		ma = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
	if (ma) {
		CFArrayAppendValue(ma, a);
	}
    CFRelease(a);
	return ma;
}


CF_RETURNS_RETAINED
CFArrayRef test_cert_string_to_subject(CFStringRef subject)
{
	CFMutableArrayRef subject_array = NULL;
	char buffer[1024];

	if (!CFStringGetCString(subject, buffer, sizeof(buffer), kCFStringEncodingASCII))
		goto out;

	char *s = buffer, *e = NULL;
	while ( (e = strchr(s, ',')) || (e = strchr(s, '\0')) ) {
		if (s == e)
			break;
		if (*e && (*(e-1) == '\\'))
			continue;
		char *k;
		while ((k = strchr(s, '=')) &&
				(*(k-1) == '\\'));
		if ( ((k - s) > 0) && ((e - k) > 1) ) {
			CFStringRef key = CFStringCreateWithBytes(kCFAllocatorDefault, (uint8_t *)s, k - s, kCFStringEncodingASCII, false);
			CFStringRef value = CFStringCreateWithBytes(kCFAllocatorDefault, (uint8_t *)k + 1, e - k - 1, kCFStringEncodingASCII, false);
			subject_array = maa(subject_array, maa(NULL, maa(maa(NULL, key), value)));
		}
        if (*e == '\0')
            break;
		s = e + 1;
	}

out:
	return subject_array;
}


static void test_cert_key_usage(CFMutableDictionaryRef extensions_dict, unsigned int key_usage)
{
	int key_usage_int = key_usage;
	CFNumberRef key_usage_num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &key_usage_int);
	CFDictionarySetValue(extensions_dict, kSecCertificateKeyUsage, key_usage_num);
	CFRelease(key_usage_num);
}


static void test_cert_path_length(CFMutableDictionaryRef extensions_dict, unsigned int path_length)
{
	int path_len_int = path_length;
	CFNumberRef path_len_num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &path_len_int);
	CFDictionarySetValue(extensions_dict, kSecCSRBasicContraintsPathLen, path_len_num);
	CFRelease(path_len_num);
}


SecIdentityRef test_cert_create_root_certificate(CFStringRef subject, SecKeyRef public_key, SecKeyRef private_key)
{
	SecCertificateRef ca_cert = NULL;
	SecIdentityRef ca_identity = NULL;
    CFMutableDictionaryRef extensions = NULL;

	CFArrayRef ca_subject = NULL;
	require(ca_subject = test_cert_string_to_subject(subject), out);
	extensions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	test_cert_key_usage(extensions, kSecKeyUsageKeyCertSign | kSecKeyUsageCRLSign);
	test_cert_path_length(extensions, 0);
	ca_cert = SecGenerateSelfSignedCertificate(ca_subject, extensions, public_key, private_key);
	if (private_key && ca_cert)
		ca_identity = SecIdentityCreate(kCFAllocatorDefault, ca_cert, private_key);

out:
	CFReleaseSafe(extensions);
	CFReleaseSafe(ca_subject);
	CFReleaseSafe(ca_cert);

	return ca_identity;
}

SecCertificateRef test_cert_issue_certificate(SecIdentityRef ca_identity,
	SecKeyRef public_key, CFStringRef subject,
	unsigned int serial_no, unsigned int key_usage)
{
	SecCertificateRef cert = NULL;
	CFArrayRef cert_subject = NULL;
	CFDataRef serialno = NULL;
	CFMutableDictionaryRef extensions = NULL;

	unsigned int serial = htonl(serial_no);
	unsigned int serial_length = sizeof(serial);
	uint8_t *serial_non_zero = (uint8_t*)&serial;
	while (!*serial_non_zero && serial_length)
		{ serial_non_zero++; serial_length--; }
    serialno = CFDataCreate(kCFAllocatorDefault,
		serial_non_zero, serial_length);
	require(cert_subject = test_cert_string_to_subject(subject), out);
	//extensions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	//require(extensions, out);
	//test_cert_key_usage(extensions, key_usage);

	cert = SecIdentitySignCertificate(ca_identity, serialno,
	    public_key, cert_subject, NULL);

out:
	CFReleaseSafe(extensions);
	CFReleaseSafe(cert_subject);
	CFReleaseSafe(serialno);

	return cert;
}

OSStatus
test_cert_generate_key(uint32_t key_size_in_bits, CFTypeRef sec_attr_key_type,
                       SecKeyRef *private_key, SecKeyRef *public_key)
{
	CFNumberRef key_size = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &key_size_in_bits);
	const void *keygen_keys[] = { kSecAttrKeyType, kSecAttrKeySizeInBits };
	const void *keygen_vals[] = { sec_attr_key_type, key_size };
	CFDictionaryRef parameters = CFDictionaryCreate(kCFAllocatorDefault,
                                                    keygen_keys, keygen_vals, array_size(keygen_vals),
                                                    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFRelease(key_size);

	return SecKeyGeneratePair(parameters, public_key, private_key);
}

#endif /* TARGET_OS_IPHONE */
