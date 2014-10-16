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
 */

#include "si-64-ossl-cms/attached_no_data_signed_data.h"
#include "si-64-ossl-cms/attached_signed_data.h"
#include "si-64-ossl-cms/detached_content.h"
#include "si-64-ossl-cms/detached_signed_data.h"
#include "si-64-ossl-cms/signer.h"
#include "si-64-ossl-cms/privkey.h"

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecCMS.h>
#include <Security/SecRSAKey.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecIdentityPriv.h>
#include <utilities/SecCFWrappers.h>

#include <unistd.h>
#include <AssertMacros.h>

#include "Security_regressions.h"

/*
openssl req -new -newkey rsa:512 -x509 -nodes -subj "/O=foo/CN=bar" -out signer.pem
echo -n "hoi joh" > detached_content
openssl smime -sign -outform der -signer signer.pem -in detached_content -inkey privkey.pem -out detached_signed_data.der
openssl smime -nodetach -sign -outform der -signer test.pem -in detached_content -inkey privkey.pem -out attached_signed_data.der
openssl smime -nodetach -sign -outform der -signer test.pem -inkey privkey.pem -out attached_no_data_signed_data.der < /dev/null

xxd -i detached_content > detached_content.h
xxd -i attached_no_data_signed_data.der > attached_no_data_signed_data.h
xxd -i attached_signed_data.der > attached_signed_data.h
xxd -i detached_signed_data.der > detached_signed_data.h

openssl x509 -in test.pem -outform der -out signer.der
xxd -i signer.der > signer.h


attached difference:

  33 NDEF:       SEQUENCE {
    <06 09>
  35    9:         OBJECT IDENTIFIER data (1 2 840 113549 1 7 1)
    <A0 80>
  46 NDEF:         [0] {
    <24 80>
  48 NDEF:           OCTET STRING {
    <04 07>
  50    7:             OCTET STRING 'hoi joh'
    <00 00>
         :             }
    <00 00>
         :           }
    <00 00>
         :         }

  39   22:       SEQUENCE {
    <06 09>
  41    9:         OBJECT IDENTIFIER data (1 2 840 113549 1 7 1)
    <A0 09>
  52    9:         [0] {
    <04 07>
  54    7:           OCTET STRING 'hoi joh'
         :           }
         :         }

detached:

    <30 80>
  33 NDEF:       SEQUENCE {
    <06 09>
  35    9:         OBJECT IDENTIFIER data (1 2 840 113549 1 7 1)
    <00 00>
         :         }

    <30 0B>
  39   11:       SEQUENCE {
    <06 09>
  41    9:         OBJECT IDENTIFIER data (1 2 840 113549 1 7 1)
         :         }

attached empty:

    <30 80>
  33 NDEF:       SEQUENCE {
    <06 09>
  35    9:         OBJECT IDENTIFIER data (1 2 840 113549 1 7 1)
    <A0 80>
  46 NDEF:         [0] {
    <24 80>
  48 NDEF:           OCTET STRING {
    <00 00>
         :             }
    <00 00>
         :           }
    <00 00>
         :         }

    <30 0F>
  39   15:       SEQUENCE {
    <06 09>
  41    9:         OBJECT IDENTIFIER data (1 2 840 113549 1 7 1)
    <A0 02>
  52    2:         [0] {
    <04 00>
  54    0:           OCTET STRING
         :             Error: Object has zero length.
         :           }
         :         }


*/

#include <fcntl.h>
__unused static inline void write_data(const char * path, CFDataRef data)
{
    int data_file = open(path, O_CREAT|O_WRONLY|O_TRUNC, 0644);
    write(data_file, CFDataGetBytePtr(data), CFDataGetLength(data));
    close(data_file);
}

static void tests(void)
{
	CFDataRef attached_signed_data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, attached_signed_data_der, attached_signed_data_der_len, kCFAllocatorNull);
	CFDataRef detached_signed_data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, detached_signed_data_der, detached_signed_data_der_len, kCFAllocatorNull);
	CFDataRef attached_no_data_signed_data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, attached_no_data_signed_data_der, attached_no_data_signed_data_der_len, kCFAllocatorNull);
	CFDataRef detached_data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, detached_content, detached_content_len, kCFAllocatorNull);
	CFDataRef no_data = CFDataCreate(kCFAllocatorDefault, NULL, 0);
	SecPolicyRef policy = SecPolicyCreateBasicX509();
	SecTrustRef trust = NULL;

	ok_status(SecCMSVerifyCopyDataAndAttributes(attached_signed_data, NULL, policy, &trust, NULL, NULL), "verify attached data");
	CFRelease(trust);
	ok_status(SecCMSVerifyCopyDataAndAttributes(detached_signed_data, detached_data, policy, &trust, NULL, NULL), "verify detached data");
	CFRelease(trust);
	ok_status(SecCMSVerifyCopyDataAndAttributes(attached_no_data_signed_data, NULL, policy, &trust, NULL, NULL), "verify attached no data");
	CFRelease(trust);
	ok_status(SecCMSVerifyCopyDataAndAttributes(attached_no_data_signed_data, no_data, policy, &trust, NULL, NULL), "verify attached no data");
	CFRelease(trust);


    SecCertificateRef cert = NULL;
    SecKeyRef privKey = NULL;
    SecIdentityRef identity = NULL;

    isnt(cert = SecCertificateCreateWithBytes(NULL, signer_der, signer_der_len), NULL, "create certificate");
    isnt(privKey = SecKeyCreateRSAPrivateKey(NULL, privkey_der, privkey_der_len, kSecKeyEncodingPkcs1), NULL, "create private key");
    isnt(identity = SecIdentityCreate(NULL, cert, privKey), NULL, "create identity");
    CFReleaseSafe(privKey);

	CFMutableDataRef cms_data = CFDataCreateMutable(kCFAllocatorDefault, 0);
	ok_status(SecCMSCreateSignedData(identity, detached_data, NULL, NULL, cms_data), "create attached data");
	//write_data("/var/tmp/attached", cms_data);
	CFDataSetLength(cms_data, 0);
	CFDictionaryRef detached_cms_dict = CFDictionaryCreate(kCFAllocatorDefault, &kSecCMSSignDetached, (const void **)&kCFBooleanTrue, 1, NULL, NULL);
	ok_status(SecCMSCreateSignedData(identity, detached_data, detached_cms_dict, NULL, cms_data), "create attached data");
	CFRelease(detached_cms_dict);
	//write_data("/var/tmp/detached", cms_data);
	CFDataSetLength(cms_data, 0);
	ok_status(SecCMSCreateSignedData(identity, NULL, NULL, NULL, cms_data), "create attached data");
	//write_data("/var/tmp/empty_attached", cms_data);

	CFReleaseSafe(cms_data);
	CFReleaseSafe(cert);
	CFReleaseNull(identity);
	CFRelease(attached_signed_data);
	CFRelease(detached_signed_data);
	CFRelease(attached_no_data_signed_data);
	CFRelease(detached_data);
	CFRelease(no_data);
	CFRelease(policy);
}

int si_64_ossl_cms(int argc, char *const *argv)
{
	plan_tests(10);


	tests();

	return 0;
}
