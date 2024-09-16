/*
 * Copyright (c) 2006-2017 Apple Inc. All Rights Reserved.
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
#include <CoreFoundation/CFBundle.h>
#include "utilities/SecCFWrappers.h"
#include <dispatch/dispatch.h>
#include <stdlib.h>
#include <string.h>

/* Security.framework's bundle id. */
#if TARGET_OS_IPHONE
CFStringRef kSecFrameworkBundleID = CFSTR("com.apple.Security");
#else
CFStringRef kSecFrameworkBundleID = CFSTR("com.apple.security");
#endif

static CFStringRef kSecurityFrameworkBundlePath = CFSTR("/System/Library/Frameworks/Security.framework");

CFGiblisGetSingleton(CFBundleRef, SecFrameworkGetBundle, bundle,  ^{
    CFStringRef bundlePath = NULL;
#if TARGET_OS_SIMULATOR
    char *simulatorRoot = getenv("SIMULATOR_ROOT");
    if (simulatorRoot) {
        bundlePath = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s%@"), simulatorRoot, kSecurityFrameworkBundlePath);
    }
#endif
    if (!bundlePath) {
        bundlePath = CFRetainSafe(kSecurityFrameworkBundlePath);
    }
    CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, bundlePath, kCFURLPOSIXPathStyle, true);
    *bundle = (url) ? CFBundleCreate(kCFAllocatorDefault, url) : NULL;
    CFReleaseSafe(url);
    CFReleaseSafe(bundlePath);
})

CFStringRef SecFrameworkCopyLocalizedString(CFStringRef key,
    CFStringRef tableName) {
    CFBundleRef bundle = SecFrameworkGetBundle();
    if (bundle)
        return CFBundleCopyLocalizedString(bundle, key, key, tableName);

    return CFRetainSafe(key);
}

Boolean SecFrameworkIsRunningInXcode(void) {
    static Boolean runningInXcode = false;
    static dispatch_once_t envCheckOnce = 0;
    dispatch_once(&envCheckOnce, ^{
        const char* envVar = getenv("NSUnbufferedIO");
        if (envVar != NULL && strcmp(envVar, "YES") == 0) {
            runningInXcode = true;
        }
    });
    return runningInXcode;
}
