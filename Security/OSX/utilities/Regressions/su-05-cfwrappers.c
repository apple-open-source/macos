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

#include "utilities_regressions.h"

#define kCFWrappersTestCount 25

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

int
su_05_cfwrappers(int argc, char *const *argv) {
    plan_tests(kCFWrappersTestCount);

    test_null();
    CFDataRef data = CFDataCreate(kCFAllocatorDefault, NULL, 0);
    test_object(data);
    CFReleaseNull(data);
    ok(data == NULL, "data is NULL now");
    return 0;
}
