/*
 * Copyright (c) 2015 Apple, Inc.  All Rights Reserved.
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

#include "IOAVLibUtil.h"

#include <AssertMacros.h>


static bool RegistryPathStringContainsPlane(CFStringRef path)
{
    CFRange range = CFStringFind(path, CFSTR(":"), 0);

    return ( range.location != kCFNotFound ) && ( ( range.location + range.length ) < CFStringGetLength(path) );
}

static CFStringRef CreateAbsoluteRegistryPathFromString(CFStringRef path, const io_name_t defaultPlane)
{
    if ( ! RegistryPathStringContainsPlane(path) )
        path = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%s:%@"), defaultPlane, path);
    else
        CFRetain(path);

    return path;
}

CFMutableDictionaryRef __IOAVClassMatching(const char * typeName, CFStringRef rootPath, IOAVLocation location, unsigned int unit)
{
#if defined(__arm__) || defined(__arm64__)
    CFMutableDictionaryRef  matching    = NULL;

    matching = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require(matching, exit);

    // filter by location, if specified
    if ( location < kIOAVLocationCount ) {
        CFStringRef locString   = NULL;

        locString = CFStringCreateWithCString(kCFAllocatorDefault, IOAVLocationString(location), kCFStringEncodingUTF8);
        require(locString, error);

        CFDictionarySetValue(matching, CFSTR(kIOAVLocationKey), locString);

        CFRelease(locString);
    }

    // filter by unit, if specified
    if ( unit != kIOAVUnitNone ) {
        CFNumberRef num = NULL;

        num = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &unit);
        require(num, error);

        CFDictionarySetValue(matching, CFSTR(kIOAVUnitKey), num);

        CFRelease(num);
    }

    // filter by root path, if specified
    if ( rootPath ) {
        CFMutableDictionaryRef parentMatching;
        CFStringRef path;

        path = CreateAbsoluteRegistryPathFromString(rootPath, kIODeviceTreePlane);
        require(path, error);

        parentMatching = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        require_action(parentMatching, error, CFRelease(path));

        CFDictionarySetValue(parentMatching, CFSTR(kIOPathMatchKey), path);
        CFRelease(path);

        CFDictionarySetValue(matching, CFSTR(kIOParentMatchKey), parentMatching);

        CFRelease(parentMatching);
    }

    // Append property to match
    {
        CFMutableDictionaryRef propertyDict = NULL;
        CFStringRef matchProp = NULL;

        propertyDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        require(propertyDict, error);

        matchProp = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%s%s"), typeName, kIOAVUserInterfaceSupportedKeySuffix);
        require_action(matchProp, error, CFRelease(propertyDict));

        CFDictionarySetValue(propertyDict, matchProp, kCFBooleanTrue);

        CFDictionarySetValue(matching, CFSTR(kIOPropertyMatchKey), propertyDict);

        CFRelease(propertyDict);
        CFRelease(matchProp);
    }

    goto exit;

error:
    CFRelease(matching);
    matching = NULL;

exit:
    return matching;
#else
    (void)typeName;
    (void)rootPath;
    (void)location;
    (void)unit;
    (void)CreateAbsoluteRegistryPathFromString;
    return NULL;
#endif // defined(__arm__) || defined(__arm64__)
}

CFTypeRef __IOAVCopyFirstMatchingIOAVObjectOfType(const char * typeName, IOAVTypeConstructor * typeConstructor, CFStringRef rootPath, IOAVLocation location, unsigned int unit)
{
    CFTypeRef               object      = NULL;
    io_service_t            service;

    service = IOServiceGetMatchingService(kIOMasterPortDefault, IOAVClassMatching(typeName, rootPath, location, unit));
    require(service, exit);

    object = typeConstructor(kCFAllocatorDefault, service);

exit:
    if ( service )
        IOObjectRelease(service);

    return object;
}
