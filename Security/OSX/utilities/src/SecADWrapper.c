/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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

#include "SecADWrapper.h"

#if TARGET_OS_EMBEDDED
#include <AggregateDictionary/ADClient.h>

static typeof(ADClientAddValueForScalarKey) *soft_ADClientAddValueForScalarKey = NULL;
static typeof(ADClientClearScalarKey) *soft_ADClientClearScalarKey = NULL;
static typeof(ADClientSetValueForScalarKey) *soft_ADClientSetValueForScalarKey = NULL;
static typeof(ADClientPushValueForDistributionKey) *soft_ADClientPushValueForDistributionKey = NULL;

static bool
setup(void)
{
    static dispatch_once_t onceToken;
    static CFBundleRef bundle = NULL;
    dispatch_once(&onceToken, ^{

        CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, CFSTR("/System/Library/PrivateFrameworks/AggregateDictionary.framework"), kCFURLPOSIXPathStyle, true);
        if (url == NULL)
            return;

        bundle = CFBundleCreate(kCFAllocatorDefault, url);
        CFRelease(url);
        if (bundle == NULL)
            return;

        soft_ADClientClearScalarKey = CFBundleGetFunctionPointerForName(bundle, CFSTR("ADClientClearScalarKey"));
        soft_ADClientSetValueForScalarKey = CFBundleGetFunctionPointerForName(bundle, CFSTR("ADClientSetValueForScalarKey"));
        soft_ADClientAddValueForScalarKey = CFBundleGetFunctionPointerForName(bundle, CFSTR("ADClientAddValueForScalarKey"));
        soft_ADClientPushValueForDistributionKey = CFBundleGetFunctionPointerForName(bundle, CFSTR("ADClientPushValueForDistributionKey"));

        if (soft_ADClientClearScalarKey == NULL ||
            soft_ADClientSetValueForScalarKey == NULL ||
            soft_ADClientAddValueForScalarKey == NULL ||
            soft_ADClientPushValueForDistributionKey == NULL)
        {
            CFRelease(bundle);
            bundle = NULL;
        }
    });
    return bundle != NULL;
}

void
SecADClearScalarKey(CFStringRef key)
{
    if (setup())
        soft_ADClientClearScalarKey(key);
}

void
SecADSetValueForScalarKey(CFStringRef key, int64_t value)
{
    if (setup())
        soft_ADClientSetValueForScalarKey(key, value);

}
void
SecADAddValueForScalarKey(CFStringRef key, int64_t value)
{
    if (setup())
        soft_ADClientAddValueForScalarKey(key, value);
}

void
SecADClientPushValueForDistributionKey(CFStringRef key, int64_t value)
{
    if (setup())
        soft_ADClientPushValueForDistributionKey(key, value);
}

#endif
