//
//  su-17-cfset-der.c
//  utilities
//
//  Created by Richard Murphy on 1/26/15.
//  Copyright © 2015 Apple Inc. All rights reserved.
//
/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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


#include "utilities_regressions.h"

#include "utilities/der_plist.h"
#include "utilities/der_plist_internal.h"

#include "utilities/SecCFRelease.h"
#include "utilities/array_size.h"

#include <CoreFoundation/CoreFoundation.h>


#define kMaxResultSize 1024

struct test_case {
    CFTypeRef values[10];
    size_t    encoded_size;
    uint8_t   encoded[256];
};

static struct test_case test_cases[] =
{
    {
        .values = { CFSTR("AValue"), },
        .encoded_size = 10, .encoded = { 0xD1, 0x08, 0x0C, 0x06, 0x41, 0x56, 0x61, 0x6C, 0x75, 0x65, },
    },{
        .values = { CFSTR("AValue"), CFSTR("BValue"), CFSTR("CValue"), CFSTR("DValue"), CFSTR("EValue"), CFSTR("FValue"), CFSTR("GValue"), CFSTR("HValue"), },
        .encoded_size = 66, .encoded = { 0xD1, 0x40, 0x0C, 0x06, 0x41, 0x56, 0x61, 0x6C, 0x75, 0x65, 0x0C, 0x06, 0x42, 0x56, 0x61, 0x6C, 0x75, 0x65, 0x0C, 0x06, 0x43, 0x56, 0x61, 0x6C, 0x75, 0x65, 0x0C, 0x06, 0x44, 0x56, 0x61, 0x6C, 0x75, 0x65, 0x0C, 0x06, 0x45, 0x56, 0x61, 0x6C, 0x75, 0x65, 0x0C, 0x06, 0x46, 0x56, 0x61, 0x6C, 0x75, 0x65, 0x0C, 0x06, 0x47, 0x56, 0x61, 0x6C, 0x75, 0x65, 0x0C, 0x06, 0x48, 0x56, 0x61, 0x6C, 0x75, 0x65, },
    },
};

#define kTestsPerSetTest 8
static void test_set(CFSetRef testValue, size_t expected_size, const uint8_t* expected_data)
{
    uint8_t buffer[kMaxResultSize];
    uint8_t* buffer_end = buffer + sizeof(buffer);
    
    uint8_t* encoded = der_encode_plist(testValue, NULL, buffer, buffer_end);
    
    ok(encoded != NULL &&
       (expected_size == (buffer_end - encoded)) &&
       (memcmp(encoded, expected_data, expected_size) == 0));
    
    encoded = der_encode_set(testValue, NULL, buffer, buffer_end);
    
    ok(encoded != NULL &&
       (expected_size == (buffer_end - encoded)) &&
       (memcmp(encoded, expected_data, expected_size) == 0));
    
#if 0
    printf(".encoded_size = %d, .encoded = { ", (int) (buffer_end - encoded));
    for(int c = 0; c < (buffer_end - encoded); ++c)
        printf("0x%02X, ", encoded[c]);
    printf("},\n");
#endif
    
    CFSetRef decoded = NULL;
    
    const uint8_t* decode_end = der_decode_set(NULL, kCFPropertyListMutableContainersAndLeaves,
                                                      &decoded, NULL, encoded, buffer_end);
    
    ok(decode_end == buffer_end, "didn't decode whole buffer");
    ok((decoded != NULL) && CFEqual(decoded, testValue), "Didn't make equal value.");
    
    CFPropertyListRef decoded_type = NULL;
    
    decode_end = der_decode_plist(NULL, kCFPropertyListMutableContainersAndLeaves,
                                  &decoded_type, NULL, encoded, buffer_end);
    
    ok(decode_end == buffer_end, "didn't decode whole buffer");
    ok((decoded != NULL) && CFEqual(decoded_type, testValue), "Didn't make equal value.");
    
    ok(der_sizeof_set(testValue, NULL) == expected_size, "Size correct.");
    ok(der_sizeof_plist(testValue, NULL) == expected_size, "Size correct.");
    
    CFReleaseNull(decoded);
    CFReleaseNull(decoded_type);
}


#define kTestsPerTestCase (kTestsPerSetTest)
static void one_test(const struct test_case * thisCase)
{
    CFIndex count = 0;
    while (count < array_size(thisCase->values) && thisCase->values[count] != NULL)
        ++count;
    
    CFSetRef testValue = CFSetCreate(NULL, (const void**)thisCase->values, count, &kCFTypeSetCallBacks);
    
    test_set(testValue, thisCase->encoded_size, thisCase->encoded);
    
    CFReleaseNull(testValue);
}

#define kTestCount (array_size(test_cases) * kTestsPerTestCase)
static void tests(void)
{
    for (int testnumber = 0; testnumber < array_size(test_cases); ++testnumber)
        one_test(&test_cases[testnumber]);
}

int su_17_cfset_der(int argc, char *const *argv)
{
    plan_tests(kTestCount);
    tests();
    
    return 0;
}
