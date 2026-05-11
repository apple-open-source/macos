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


#include <utilities/SecCFRelease.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecIOFormat.h>
#include <mach/mach_time.h>

#include "utilities_regressions.h"

#define kCFWrappersTestCount 31

static CFDataRef *testCopyDataPtr(void) {
    static CFDataRef sData = NULL;
    if (!sData)
        sData = CFDataCreate(kCFAllocatorDefault, NULL, 0);
    else
        CFRetain(sData);
    return &sData;
}

static void
test_object(CFDataRef data) {
    CFDataRef myData = CFRetainSafe(data);
    ok(CFEqual(myData, data), "");
    is(CFGetRetainCount(myData), 2, "");
    ok(CFReleaseNull(myData) == ((CFDataRef)(0)), "");
    is(myData, NULL, "");

    is(CFGetRetainCount(data), 1);
    CFRetainAssign(myData, data);
    is(CFGetRetainCount(data), 2);
    CFRetainAssign(myData, data);
    is(CFGetRetainCount(data), 2);
    CFRetainAssign(myData, NULL);
    is(CFGetRetainCount(data), 1);
    is(myData, NULL, "");

    CFDataRef *pData = testCopyDataPtr();
    is(CFGetRetainCount(*pData), 1);
    CFDataRef objects[10] = {}, *object = objects;
    *object = *pData;
    CFRetainAssign(*testCopyDataPtr(), *object++);
    is(CFGetRetainCount(*pData), 2, "CFRetainAssign evaluates it's first argument argument %" PRIdCFIndex " times", CFGetRetainCount(*pData) - 1);
    is(object - objects, 1, "CFRetainAssign evaluates it's second argument %td times", object - objects);

    is(CFGetRetainCount(data), 1);
    CFAssignRetained(myData, data);
    is(CFGetRetainCount(myData), 1);
}

static void
test_null(void) {
    CFTypeRef nullObject1 = NULL;
    CFTypeRef nullObject2 = NULL;

    nullObject1 = CFRetainSafe(NULL);

    is(nullObject1, NULL, "");
    is(CFReleaseNull(nullObject1), NULL, "CFReleaseNull(nullObject1) returned");
    is(nullObject1, NULL);
    is(CFReleaseSafe(nullObject1), NULL, "CFReleaseSafe(nullObject1) returned");
    is(CFReleaseSafe(NULL), NULL, "CFReleaseSafe(NULL)");
    is(CFReleaseNull(nullObject2), NULL, "CFReleaseNull(nullObject2) returned");
    is(nullObject2, NULL, "nullObject2 still NULL");

    CFRetainAssign(nullObject2, nullObject1);

    CFTypeRef *object, objects[10] = {};

    object = &objects[0];
    CFRetainSafe(*object++);
    is(object - objects, 1, "CFRetainSafe evaluates it's argument %td times", object - objects);

    object = &objects[0];
    CFReleaseSafe(*object++);
    is(object - objects, 1, "CFReleaseSafe evaluates it's argument %td times", object - objects);

    object = &objects[0];
    CFReleaseNull(*object++);
    is(object - objects, 1, "CFReleaseNull evaluates it's argument %td times", object - objects);
}

static void
test_hex_string(void) {
    // Empty data produces empty string
    {
        CFDataRef empty = CFDataCreate(kCFAllocatorDefault, NULL, 0);
        CFStringRef hex = CFDataCopyHexString(empty);
        ok(CFEqual(hex, CFSTR("")), "empty data yields empty hex string");
        CFReleaseNull(hex);
        CFReleaseNull(empty);
    }

    // Known byte sequence
    {
        const uint8_t bytes[] = { 0x00, 0xFF, 0xAB, 0xCD };
        CFDataRef data = CFDataCreate(kCFAllocatorDefault, bytes, sizeof(bytes));
        CFStringRef hex = CFDataCopyHexString(data);
        ok(CFEqual(hex, CFSTR("00FFABCD")), "known bytes encode correctly");
        CFReleaseNull(hex);
        CFReleaseNull(data);
    }

    // All 256 byte values: check length, first byte "00", last byte "FF"
    {
        uint8_t allbytes[256];
        for (int i = 0; i < 256; i++) allbytes[i] = (uint8_t)i;
        CFDataRef data = CFDataCreate(kCFAllocatorDefault, allbytes, 256);
        CFStringRef hex = CFDataCopyHexString(data);
        is(CFStringGetLength(hex), (CFIndex)512, "all-bytes hex string has length 512");
        CFStringRef first = CFStringCreateWithSubstring(kCFAllocatorDefault, hex, CFRangeMake(0, 2));
        ok(CFEqual(first, CFSTR("00")), "first byte encodes as 00");
        CFReleaseNull(first);
        CFStringRef last = CFStringCreateWithSubstring(kCFAllocatorDefault, hex, CFRangeMake(510, 2));
        ok(CFEqual(last, CFSTR("FF")), "last byte encodes as FF");
        CFReleaseNull(last);
        CFReleaseNull(hex);
        CFReleaseNull(data);
    }

    // Performance: 10 iterations over 100 KB must complete in under 1 second
    {
        const size_t kBigLen = 100 * 1024;
        uint8_t *bigbuf = malloc(kBigLen);
        for (size_t i = 0; i < kBigLen; i++) bigbuf[i] = (uint8_t)(i & 0xFF);
        CFDataRef bigdata = CFDataCreate(kCFAllocatorDefault, bigbuf, (CFIndex)kBigLen);
        free(bigbuf);

        uint64_t start = mach_absolute_time();
        for (int i = 0; i < 10; i++) {
            CFStringRef hex = CFDataCopyHexString(bigdata);
            CFReleaseNull(hex);
        }
        uint64_t elapsed = mach_absolute_time() - start;

        mach_timebase_info_data_t info;
        mach_timebase_info(&info);
        uint64_t elapsed_ns = elapsed * info.numer / info.denom;
        ok(elapsed_ns < 1000000000ULL,
           "performance: 10x100KB hex in under 1s (took %llu ms)", elapsed_ns / 1000000);

        CFReleaseNull(bigdata);
    }
}

int
su_05_cfwrappers(int argc, char *const *argv) {
    plan_tests(kCFWrappersTestCount);

    test_null();
    CFDataRef data = CFDataCreate(kCFAllocatorDefault, NULL, 0);
    test_object(data);
    CFReleaseNull(data);
    ok(data == NULL, "data is NULL now");
    test_hex_string();
    return 0;
}
