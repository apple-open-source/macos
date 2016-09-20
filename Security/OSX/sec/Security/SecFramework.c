/*
 * Copyright (c) 2006-2010,2012-2014 Apple Inc. All Rights Reserved.
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

#ifdef STANDALONE
/* Allows us to build genanchors against the BaseSDK. */
#undef __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__
#undef __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__
#endif

#include "SecFramework.h"
#include <dispatch/dispatch.h>
#include <CoreFoundation/CFBundle.h>
#include <CoreFoundation/CFURLAccess.h>
#include <Security/SecRandom.h>
#include <CommonCrypto/CommonRandomSPI.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <utilities/debugging.h>
#include <utilities/SecCFWrappers.h>
#include <Security/SecBase.h>
#include <inttypes.h>

#if !(TARGET_IPHONE_SIMULATOR && defined(IPHONE_SIMULATOR_HOST_MIN_VERSION_REQUIRED) && IPHONE_SIMULATOR_HOST_MIN_VERSION_REQUIRED < 1090)
#include <sys/guarded.h>
#define USE_GUARDED_OPEN 1
#else
#define USE_GUARDED_OPEN 0
#endif


/* Security.framework's bundle id. */
static CFStringRef kSecFrameworkBundleID = CFSTR("com.apple.Security");

CFGiblisGetSingleton(CFBundleRef, SecFrameworkGetBundle, bundle,  ^{
    *bundle = CFRetainSafe(CFBundleGetBundleWithIdentifier(kSecFrameworkBundleID));
})

CFStringRef SecFrameworkCopyLocalizedString(CFStringRef key,
    CFStringRef tableName) {
    CFBundleRef bundle = SecFrameworkGetBundle();
    if (bundle)
        return CFBundleCopyLocalizedString(bundle, key, key, tableName);

    return CFRetainSafe(key);
}

CFURLRef SecFrameworkCopyResourceURL(CFStringRef resourceName,
	CFStringRef resourceType, CFStringRef subDirName) {
    CFURLRef url = NULL;
    CFBundleRef bundle = SecFrameworkGetBundle();
    if (bundle) {
        url = CFBundleCopyResourceURL(bundle, resourceName,
			resourceType, subDirName);
		if (!url) {
            secwarning("resource: %@.%@ in %@ not found", resourceName,
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
            secwarning("read: %ld", (long) error);
        }
        CFRelease(url);
    }

	return data;
}



const SecRandomRef kSecRandomDefault = NULL;

int SecRandomCopyBytes(SecRandomRef rnd, size_t count, uint8_t *bytes) {
    if (rnd != kSecRandomDefault)
        return errSecParam;
    return CCRandomCopyBytes(kCCRandomDefault, bytes, count);
}
