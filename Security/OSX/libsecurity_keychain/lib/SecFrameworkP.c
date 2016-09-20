/*
 * Copyright (c) 2006-2015 Apple Inc. All Rights Reserved.
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
 * SecFramework.c - generic non API class specific functions
 */


#include "SecFrameworkP.h"
#include <pthread.h>
#include <CoreFoundation/CFBundle.h>
#include <CoreFoundation/CFURLAccess.h>
#if 0
#include "SecRandomP.h"
#endif
#include <CommonCrypto/CommonDigest.h>
#include <Security/SecAsn1Coder.h>
#include <Security/oidsalg.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <dlfcn.h>
#include <string.h>
#include <CoreFoundation/CFBundlePriv.h>

#include <utilities/debugging.h>

/* Security framework's own bundle used for localized string lookups. */
static CFBundleRef kSecFrameworkBundle;
static pthread_once_t kSecFrameworkBundleLookup = PTHREAD_ONCE_INIT;

static void SecFrameworkBundleLookup(void) {
	// figure out the path to our executable
	Dl_info info;
	dladdr("", &info);

	// make a file URL from the returned string
	CFURLRef urlRef = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8*) info.dli_fname, strlen(info.dli_fname), false);
	kSecFrameworkBundle = _CFBundleCreateWithExecutableURLIfLooksLikeBundle(NULL, urlRef);
	CFRelease(urlRef);

    if (kSecFrameworkBundle)
        CFRetain(kSecFrameworkBundle);
}

CFStringRef SecFrameworkCopyLocalizedString(CFStringRef key,
    CFStringRef tableName) {
    pthread_once(&kSecFrameworkBundleLookup, SecFrameworkBundleLookup);
    if (kSecFrameworkBundle) {
        return CFBundleCopyLocalizedString(kSecFrameworkBundle, key, key,
            tableName);
    }

    CFRetain(key);
    return key;
}

CFURLRef SecFrameworkCopyResourceURL(CFStringRef resourceName,
	CFStringRef resourceType, CFStringRef subDirName) {
    CFURLRef url = NULL;
    pthread_once(&kSecFrameworkBundleLookup, SecFrameworkBundleLookup);
    if (kSecFrameworkBundle) {
        url = CFBundleCopyResourceURL(kSecFrameworkBundle, resourceName,
			resourceType, subDirName);
		if (!url) {
            secinfo("SecFramework", "resource: %@.%@ in %@ not found", resourceName,
                resourceType, subDirName);
		}
    }

	return url;
}


CFDataRef SecFrameworkCopyResourceContents(CFStringRef resourceName,
	CFStringRef resourceType, CFStringRef subDirName) {
    CFURLRef url = SecFrameworkCopyResourceURL(resourceName, resourceType,
        subDirName);
	CFDataRef data = NULL;
    if (url) {
        SInt32 error;
        if (!CFURLCreateDataAndPropertiesFromResource(kCFAllocatorDefault,
            url, &data, NULL, NULL, &error)) {
            secinfo("SecFramework", "read: %d", (int)error);
        }
        CFRelease(url);
    }

	return data;
}

/* Return the SHA1 digest of a chunk of data as newly allocated CFDataRef. */
CFDataRef SecSHA1DigestCreate(CFAllocatorRef allocator,
	const UInt8 *data, CFIndex length) {
	CFMutableDataRef digest = CFDataCreateMutable(allocator,
		CC_SHA1_DIGEST_LENGTH);
	CFDataSetLength(digest, CC_SHA1_DIGEST_LENGTH);
	CC_SHA1(data, (CC_LONG)length, CFDataGetMutableBytePtr(digest));
	return digest;
}

#if 0
CFDataRef SecDigestCreate(CFAllocatorRef allocator,
    const SecAsn1Oid *algorithm, const SecAsn1Item *params,
	const UInt8 *data, CFIndex length) {
    unsigned char *(*digestFcn)(const void *data, CC_LONG len, unsigned char *md);
    CFIndex digestLen;

    if (SecAsn1OidCompare(algorithm, &CSSMOID_SHA1)) {
        digestFcn = CC_SHA1;
        digestLen = CC_SHA1_DIGEST_LENGTH;
    } else if (SecAsn1OidCompare(algorithm, &CSSMOID_SHA224)) {
        digestFcn = CC_SHA224;
        digestLen = CC_SHA224_DIGEST_LENGTH;
    } else if (SecAsn1OidCompare(algorithm, &CSSMOID_SHA256)) {
        digestFcn = CC_SHA256;
        digestLen = CC_SHA256_DIGEST_LENGTH;
    } else if (SecAsn1OidCompare(algorithm, &CSSMOID_SHA384)) {
        digestFcn = CC_SHA384;
        digestLen = CC_SHA384_DIGEST_LENGTH;
    } else if (SecAsn1OidCompare(algorithm, &CSSMOID_SHA512)) {
        digestFcn = CC_SHA512;
        digestLen = CC_SHA512_DIGEST_LENGTH;
    } else {
        return NULL;
    }

	CFMutableDataRef digest = CFDataCreateMutable(allocator, digestLen);
	CFDataSetLength(digest, digestLen);
	digestFcn(data, length, CFDataGetMutableBytePtr(digest));
	return digest;
}
#endif
