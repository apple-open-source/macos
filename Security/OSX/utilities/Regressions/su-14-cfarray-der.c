/*
 * Copyright (c) 2012,2014 Apple Inc. All Rights Reserved.
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


#include "utilities/der_plist.h"
#include "utilities/der_plist_internal.h"

#include "utilities/SecCFRelease.h"
#include "utilities/array_size.h"

#include <CoreFoundation/CoreFoundation.h>

#include "utilities_regressions.h"

#define kMaxResultSize 1024

struct test_case {
    CFTypeRef elements[10];
    size_t    encoded_size;
    uint8_t   encoded[20];
};

static struct test_case test_cases[] =
{
    { .elements = { CFSTR("First") },
        .encoded_size = 9, .encoded = { 0x30, 0x07, 0x0C, 0x05, 0x46, 0x69, 0x72, 0x73, 0x74, }, },
    { .elements = { CFSTR("First"), CFSTR("SECOND") },
        .encoded_size = 17, .encoded = { 0x30, 0x0F, 0x0C, 0x05, 0x46, 0x69, 0x72, 0x73, 0x74, 0x0C, 0x06, 0x53, 0x45, 0x43, 0x4F, 0x4E, 0x44, }, },
    { .elements = { },
        .encoded_size = 2, .encoded = { 0x30, 0x00, } },
};

#define kTestsPerTestCase 8
static void one_test(const struct test_case * thisCase)
{
    CFIndex element_count = 0;
    while (element_count < array_size(thisCase->elements) && thisCase->elements[element_count] != NULL)
        ++element_count;

    CFArrayRef testValue = CFArrayCreate(NULL, (const void **)&thisCase->elements, element_count, &kCFTypeArrayCallBacks);

    uint8_t buffer[kMaxResultSize];
    uint8_t* buffer_end = buffer + sizeof(buffer);
    
    uint8_t* encoded = der_encode_plist(testValue, NULL, buffer, buffer_end);

    ok(encoded != NULL &&
       (thisCase->encoded_size == (buffer_end - encoded)) &&
       (memcmp(encoded, thisCase->encoded, thisCase->encoded_size) == 0));

    encoded = der_encode_array(testValue, NULL, buffer, buffer_end);

    ok(encoded != NULL &&
       (thisCase->encoded_size == (buffer_end - encoded)) &&
       (memcmp(encoded, thisCase->encoded, thisCase->encoded_size) == 0));

#if 0
    printf(".encoded_size = %d, .encoded = { ", (buffer_end - encoded));
    for(int c = 0; c < (buffer_end - encoded); ++c)
        printf("0x%02X, ", encoded[c]);
    printf("},\n");
#endif

    CFArrayRef decoded = NULL;
    
    const uint8_t* decode_end = der_decode_array(NULL, kCFPropertyListMutableContainersAndLeaves,
                                                 &decoded, NULL, encoded, buffer_end);
    
    ok(decode_end == buffer_end, "didn't decode whole buffer");
    ok((decoded != NULL) && CFEqual(decoded, testValue), "Didn't make equal value.");

    CFTypeRef decoded_type = NULL;

    decode_end = der_decode_plist(NULL, kCFPropertyListMutableContainersAndLeaves,
                                  &decoded_type, NULL, encoded, buffer_end);

    ok(decode_end == buffer_end, "didn't decode whole buffer");
    ok((decoded != NULL) && CFEqual(decoded_type, testValue), "Didn't make equal value.");

    ok(der_sizeof_array(testValue, NULL) == thisCase->encoded_size, "Size correct.");
    ok(der_sizeof_plist(testValue, NULL) == thisCase->encoded_size, "Size correct.");

    CFReleaseNull(decoded);
    CFReleaseNull(decoded_type);
    
    CFReleaseNull(testValue);
}

#define kTestCount (array_size(test_cases) * kTestsPerTestCase)
static void tests(void)
{
    for (int testnumber = 0; testnumber < array_size(test_cases); ++testnumber)
        one_test(test_cases + testnumber);
}

int su_14_cfarray_der(int argc, char *const *argv)
{
    plan_tests(kTestCount);
    tests();

    return 0;
}
