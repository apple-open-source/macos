/*
 * Copyright (c) 2015-2016 Apple Inc. All Rights Reserved.
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

#include <AssertMacros.h>

#include "SecAppleAnchorPriv.h"
#include "SecInternalReleasePriv.h"
#include "SecCFWrappers.h"
#include <Security/SecCertificatePriv.h>

static CFDictionaryRef getAnchors(void);

static bool testAppleAnchorsAllowed(SecAppleTrustAnchorFlags flags) {
    if (!(flags & kSecAppleTrustAnchorFlagsIncludeTestAnchors)) {
        /* user does not want test anchors */
        return false;
    }
    if (SecIsInternalRelease() ||
        flags & kSecAppleTrustAnchorFlagsAllowNonProduction) {
        /* device allows test anchors */
        return true;
    }
    return false;
}

bool
SecIsAppleTrustAnchorData(CFDataRef cert,
			  SecAppleTrustAnchorFlags flags)
{
    CFDictionaryRef anchors = NULL;
    CFTypeRef value = NULL;
    bool res = false;

    anchors = getAnchors();
    require(anchors, fail);

    value = CFDictionaryGetValue(anchors, cert);
    require_quiet(value, fail);

    require(isBoolean(value), fail);

    if (testAppleAnchorsAllowed(flags)) {
        res = true;
    } else {
        res = CFBooleanGetValue(value);
    }

 fail:
    return res;
}


bool
SecIsAppleTrustAnchor(SecCertificateRef cert,
                      SecAppleTrustAnchorFlags flags)
{
    CFDataRef data;
    bool res = false;

    data = SecCertificateCopySHA256Digest(cert);
    require(data, fail);

    res = SecIsAppleTrustAnchorData(data, flags);
    
fail:
    CFReleaseNull(data);
    return res;
}

/* subject:/C=US/O=Apple Inc./OU=Apple Certification Authority/CN=Apple Root CA */
/* SKID: 2B:D0:69:47:94:76:09:FE:F4:6B:8D:2E:40:A6:F7:47:4D:7F:08:5E */
/* Not Before: Apr 25 21:40:36 2006 GMT, Not After : Feb  9 21:40:36 2035 GMT */
/* Signature Algorithm: sha1WithRSAEncryption */
static const unsigned char AppleRootCAHash[32] = {
    0xb0, 0xb1, 0x73, 0x0e, 0xcb, 0xc7, 0xff, 0x45, 0x05, 0x14, 0x2c, 0x49, 0xf1, 0x29, 0x5e, 0x6e,
    0xda, 0x6b, 0xca, 0xed, 0x7e, 0x2c, 0x68, 0xc5, 0xbe, 0x91, 0xb5, 0xa1, 0x10, 0x01, 0xf0, 0x24
};

/* subject:/CN=Apple Root CA - G2/OU=Apple Certification Authority/O=Apple Inc./C=US */
/* SKID: C4:99:13:6C:18:03:C2:7B:C0:A3:A0:0D:7F:72:80:7A:1C:77:26:8D */
/* Not Before: Apr 30 18:10:09 2014 GMT, Not After : Apr 30 18:10:09 2039 GMT */
/* Signature Algorithm: sha384WithRSAEncryption */
static const unsigned char AppleRootG2Hash[32] = {
    0xc2, 0xb9, 0xb0, 0x42, 0xdd, 0x57, 0x83, 0x0e, 0x7d, 0x11, 0x7d, 0xac, 0x55, 0xac, 0x8a, 0xe1,
    0x94, 0x07, 0xd3, 0x8e, 0x41, 0xd8, 0x8f, 0x32, 0x15, 0xbc, 0x3a, 0x89, 0x04, 0x44, 0xa0, 0x50
};

/* subject:/CN=Apple Root CA - G3/OU=Apple Certification Authority/O=Apple Inc./C=US */
/* SKID: BB:B0:DE:A1:58:33:88:9A:A4:8A:99:DE:BE:BD:EB:AF:DA:CB:24:AB */
/* Not Before: Apr 30 18:19:06 2014 GMT, Not After : Apr 30 18:19:06 2039 GMT */
/* Signature Algorithm: ecdsa-with-SHA38 */
static const unsigned char AppleRootG3Hash[32] = {
    0x63, 0x34, 0x3a, 0xbf, 0xb8, 0x9a, 0x6a, 0x03, 0xeb, 0xb5, 0x7e, 0x9b, 0x3f, 0x5f, 0xa7, 0xbe,
    0x7c, 0x4f, 0x5c, 0x75, 0x6f, 0x30, 0x17, 0xb3, 0xa8, 0xc4, 0x88, 0xc3, 0x65, 0x3e, 0x91, 0x79
};

/* subject:/C=US/O=Apple Inc./OU=Apple Certification Authority/CN=Test Apple Root CA */
/* SKID: 59:B8:2B:94:3A:1B:BA:F1:00:AE:EE:50:52:23:33:C9:59:C3:54:98 */
/* Not Before: Apr 22 02:15:48 2015 GMT, Not After : Feb  9 21:40:36 2035 GMT */
/* Signature Algorithm: sha1WithRSAEncryption */
static const unsigned char TestAppleRootCAHash[32] = {
    0x08, 0x47, 0x99, 0xfb, 0xa9, 0x9c, 0x06, 0x46, 0xe5, 0xcf, 0x0b, 0xf2, 0x73, 0x7f, 0x23, 0xa4,
    0x77, 0xe4, 0x98, 0x05, 0x5b, 0x9e, 0xf9, 0x0c, 0xdf, 0x40, 0xc2, 0x92, 0xfd, 0x46, 0x6c, 0xd7
};

/* subject:/CN=Test Apple Global Root CA/OU=Apple Certification Authority/O=Apple Inc./C=US */
/* SKID: 96:D3:56:5F:F8:49:C1:40:DF:3B:82:36:5F:09:75:EE:95:58:32:43 */
/* Not Before: Apr 22 02:43:57 2015 GMT, Not After : Dec 26 03:13:37 2040 GMT */
/* Signature Algorithm: ecdsa-with-SHA384 */
static const unsigned char TestAppleRootG2Hash[32] = {
    0x0c, 0x14, 0x3e, 0xab, 0x0e, 0xb9, 0x23, 0xbe, 0xa5, 0xc5, 0x3e, 0xe4, 0x24, 0xcf, 0xdb, 0x63,
    0xc6, 0xa9, 0xc2, 0x38, 0x0f, 0x6b, 0xf6, 0xbf, 0xb2, 0x62, 0xdd, 0x36, 0x92, 0x25, 0xfb, 0xea
};

/* subject:/CN=Test Apple Root CA - G3/OU=Apple Certification Authority/O=Apple Inc./C=US */
/* SKID: FC:46:D8:83:6C:1F:E6:F2:DC:DF:A7:99:17:AE:0B:44:67:17:1B:46 */
/* Not Before: Apr 22 03:17:44 2015 GMT, Not After : Dec 26 03:13:37 2040 GMT */
/* Signature Algorithm: ecdsa-with-SHA384 */
static const unsigned char TestAppleRootG3Hash[32] = {
    0xbe, 0x9f, 0x7d, 0x2b, 0x62, 0x81, 0x8b, 0xb0, 0xce, 0x6d, 0x7d, 0x73, 0x65, 0xcc, 0x9f, 0xbc,
    0xbe, 0xa4, 0x1b, 0x5a, 0xe1, 0xd4, 0xe9, 0xdd, 0xd5, 0x4c, 0x1b, 0x34, 0x9e, 0x7a, 0x2d, 0xa6
};

static void
addAnchor(CFMutableDictionaryRef anchors,
          const unsigned char *trustAnchor, size_t size,
          bool production)
{
    CFDataRef value = CFDataCreateWithBytesNoCopy(NULL, trustAnchor, size, kCFAllocatorNull);
    if (CFDictionaryGetValue(anchors, value))
        abort();
    CFDictionarySetValue(anchors, value, production ? kCFBooleanTrue : kCFBooleanFalse);
    CFReleaseSafe(value);
}


static CFDictionaryRef
getAnchors(void)
{
    static dispatch_once_t onceToken;
    static CFDictionaryRef anchors = NULL;
    dispatch_once(&onceToken, ^{
        CFMutableDictionaryRef temp;

        temp = CFDictionaryCreateMutableForCFTypes(NULL);
        addAnchor(temp, AppleRootCAHash, sizeof(AppleRootCAHash), true);
        addAnchor(temp, AppleRootG2Hash, sizeof(AppleRootG2Hash), true);
        addAnchor(temp, AppleRootG3Hash, sizeof(AppleRootG3Hash), true);
        addAnchor(temp, TestAppleRootCAHash, sizeof(TestAppleRootCAHash), false);
        addAnchor(temp, TestAppleRootG2Hash, sizeof(TestAppleRootG2Hash), false);
        addAnchor(temp, TestAppleRootG3Hash, sizeof(TestAppleRootG3Hash), false);


        anchors = temp;
    });
    return anchors;
}
