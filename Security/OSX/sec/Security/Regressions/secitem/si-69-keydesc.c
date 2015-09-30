/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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


#include <Security/SecCMS.h>
#include <Security/SecItem.h>
#include <Security/SecKey.h>
#include <Security/SecItemPriv.h>
#include <Security/SecInternal.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecBasePriv.h>
#include <Security/SecRSAKey.h>
#include <Security/SecECKey.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecCertificateRequest.h>

#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonDigestSPI.h>

#include <corecrypto/ccec.h>

#include <CoreFoundation/CoreFoundation.h>
#include <stdlib.h>
#include <unistd.h>
#include "AssertMacros.h"
#include "Security_regressions.h"


unsigned char rsaPubKey[] = {
    0x30, 0x68, 0x02, 0x61, 0x00, 0xc0, 0x63, 0x42, 0xb4, 0xf0, 0x6f, 0x2c, 0xda, 0x71, 0xef, 0x9d, 0x9d, 0x3e, 0x3e, 0x93, 0xc9, 0xd4, 0x2e, 0xe5, 0x62, 0x32, 0x6a, 0xbb, 0xeb, 0x34, 0x57, 0xeb, 0x8a, 0xf8, 0x62, 0xf9, 0xc0, 0x1c, 0x14, 0x54, 0x99, 0x04, 0x6b, 0x19, 0x92, 0xdd, 0xa3, 0x7d, 0x8c, 0x86, 0x48, 0xc9, 0xa6, 0x03, 0x63, 0xf8, 0xab, 0x9c, 0xb9, 0x2b, 0x49, 0x4c, 0x53, 0xb1, 0x39, 0xf9, 0x4a, 0xbc, 0x8b, 0xfc, 0xea, 0x93, 0xb6, 0x2e, 0x41, 0x1f, 0x21, 0x50, 0xca, 0xf7, 0x99, 0xc7, 0x77, 0x73, 0x03, 0x14, 0x58, 0x51, 0x1a, 0xa7, 0x68, 0xe5, 0x8d, 0x68, 0x6b, 0xca, 0x33, 0x4f, 0xcc, 0x6b, 0x41, 0x02, 0x03, 0x01, 0x00, 0x01
};

unsigned char ecPubKey[] = {
    0x04, 0x01, 0x41, 0x34, 0x87, 0xfd, 0xe1, 0x51, 0x5d, 0x29, 0x12, 0x07, 0xc2, 0x57, 0x54, 0x19, 0xd2, 0xd9, 0x18, 0x95, 0x07, 0x17, 0x8a, 0xf7, 0x2d, 0x2b, 0xf9, 0xbc, 0xe6, 0x1b, 0xe7, 0x81, 0x35, 0x13, 0x5f, 0x1d, 0xfa, 0xed, 0x7e, 0x70, 0x2b, 0xcd, 0x01, 0xa0, 0xaa, 0x7f, 0xe4, 0x0f, 0x4e, 0x19, 0x56, 0xb0, 0x15, 0xfb, 0xd8, 0xc9, 0xe7, 0x48, 0xcf, 0xc7, 0x5e, 0xe8, 0xcc, 0x74, 0x34, 0x61, 0xa5, 0x01, 0x02, 0x67, 0x03, 0x16, 0xce, 0x3d, 0x31, 0x37, 0x9c, 0x0b, 0x03, 0x65, 0x94, 0xaa, 0xd0, 0x1d, 0xa9, 0x5a, 0xe3, 0x0a, 0xf9, 0x82, 0xef, 0x43, 0x75, 0x5b, 0x46, 0x52, 0x6c, 0x0a, 0x02, 0x3f, 0xc3, 0xd3, 0x42, 0x0d, 0xa7, 0x90, 0x8c, 0x4b, 0x15, 0x88, 0x89, 0x24, 0xed, 0x91, 0x0a, 0xa1, 0x20, 0x0d, 0x82, 0xed, 0x87, 0x8c, 0x98, 0x8e, 0xbe, 0xbc, 0xa3, 0xa7, 0xca, 0x50, 0x2d, 0x71, 0x73
};

const char *rsaKeyDescription = "<SecKeyRef algorithm id: 1, key type: RSAPublicKey, version: 3, block size: 768 bits, exponent: {hex: 10001, decimal: 65537}, modulus: C06342B4F06F2CDA71EF9D9D3E3E93C9D42EE562326ABBEB3457EB8AF862F9C01C145499046B1992DDA37D8C8648C9A60363F8AB9CB92B494C53B139F94ABC8BFCEA93B62E411F2150CAF799C77773031458511AA768E58D686BCA334FCC6B41";

const char *ecKeyDescription = "<SecKeyRef curve type: kSecECCurveSecp521r1, algorithm id: 3, key type: ECPublicKey, version: 3, block size: 528 bits, y: 73712D50CAA7A3BCBE8E988C87ED820D20A10A91ED248988154B8C90A70D42D3C33F020A6C52465B7543EF82F90AE35AA91DD0AA9465030B9C37313DCE1603670201, x: A5613474CCE85EC7CF48E7C9D8FB15B056194E0FE47FAAA001CD2B707EEDFA1D5F133581E71BE6BCF92B2DF78A17079518D9D2195457C20712295D51E1FD87344101";

static void testECKeyDesc() {

    SecKeyRef pubKey = NULL;
    CFStringRef pubRef = NULL;
    long pubLength = 0;

    pubKey = SecKeyCreateECPublicKey(kCFAllocatorDefault, ecPubKey, sizeof(ecPubKey), kSecKeyEncodingBytes);
    require_quiet( pubKey, fail);
    
    pubRef = CFCopyDescription(pubKey);
    require_quiet(pubRef, fail);

    pubLength = CFStringGetLength(pubRef)+1;
    char *publicDescription = (char*)malloc(pubLength);
    
    if(false == CFStringGetCString(pubRef, publicDescription, pubLength, kCFStringEncodingUTF8))
    {
	free(publicDescription);
	goto fail;
    }
    
    ok_status(strncmp(ecKeyDescription, publicDescription, strlen(ecKeyDescription)-17), "ec key description");
    free(publicDescription);

fail:
    CFReleaseSafe(pubRef);
    CFReleaseSafe(pubKey);
 
}

/*
 * tests the description for
 * RSA keypairs
 */
 

static void testRSAKeyDesc()
{   
    SecKeyRef pubKey = NULL;
    CFStringRef pubRef = NULL;
    long pubLength = 0;
    	
    pubKey = SecKeyCreateRSAPublicKey(kCFAllocatorDefault, rsaPubKey, sizeof(rsaPubKey), kSecKeyEncodingBytes);
    require_quiet( pubKey, fail);
    
    pubRef = CFCopyDescription(pubKey);
    require_quiet(pubRef, fail);
    
    pubLength = CFStringGetLength(pubRef)+1;
    char *publicDescription = (char*)malloc(pubLength);
    require_quiet(publicDescription != NULL, fail);

    if(false == CFStringGetCString(pubRef, publicDescription, pubLength, kCFStringEncodingUTF8))
    {
	free(publicDescription);
	goto fail;
    }
    
    ok_status(strncmp(rsaKeyDescription, publicDescription, strlen(rsaKeyDescription)-17), "rsa key descriptions don't match: %s %s", rsaKeyDescription, publicDescription);
    free(publicDescription);

fail:
    CFReleaseSafe(pubRef);
    CFReleaseSafe(pubKey);
        
}

static void tests(void)
{
    //test rsa public key description
    testRSAKeyDesc();
    
    //test ec public key description
    testECKeyDesc();

}

int si_69_keydesc(int argc, char *const *argv)
{

    plan_tests(2);
   
	tests();
    
	return 0;
}
