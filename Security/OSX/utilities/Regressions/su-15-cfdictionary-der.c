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
    CFTypeRef keys[10];
    CFTypeRef values[10];
    size_t    encoded_size;
    uint8_t   encoded[256];
};

static struct test_case test_cases[] =
{
    { .keys = { CFSTR("First"), },
        .values = { CFSTR("First Value!"), },
        .encoded_size = 25, .encoded = { 0x31, 0x17, 0x30, 0x15, 0x0C, 0x05, 0x46, 0x69, 0x72, 0x73, 0x74, 0x0C, 0x0C, 0x46, 0x69, 0x72, 0x73, 0x74, 0x20, 0x56, 0x61, 0x6C, 0x75, 0x65, 0x21, },
    },
    { .keys = { CFSTR("First"), CFSTR("Second"), },
      .values = { CFSTR("First Value!"), CFSTR("A second value"), },
      .encoded_size = 51, .encoded = { 0x31, 0x31,
                                       0x30, 0x15, 0x0C, 0x05, 0x46, 0x69, 0x72, 0x73, 0x74, 0x0C, 0x0C, 0x46, 0x69, 0x72, 0x73, 0x74, 0x20, 0x56, 0x61, 0x6C, 0x75, 0x65, 0x21,
                                       0x30, 0x18, 0x0C, 0x06, 0x53, 0x65, 0x63, 0x6F, 0x6E, 0x64, 0x0C, 0x0E, 0x41, 0x20, 0x73, 0x65, 0x63, 0x6F, 0x6E, 0x64, 0x20, 0x76, 0x61, 0x6C, 0x75, 0x65, },
    },
};

#define kTestsPerDictionaryTest 8
static void test_dictionary(CFDictionaryRef testValue, size_t expected_size, const uint8_t* expected_data)
{
    uint8_t buffer[kMaxResultSize];
    uint8_t* buffer_end = buffer + sizeof(buffer);
    
    uint8_t* encoded = der_encode_plist(testValue, NULL, buffer, buffer_end);
    
    ok(encoded != NULL &&
       (expected_size == (buffer_end - encoded)) &&
       (memcmp(encoded, expected_data, expected_size) == 0));
    
    encoded = der_encode_dictionary(testValue, NULL, buffer, buffer_end);
    
    ok(encoded != NULL &&
       (expected_size == (buffer_end - encoded)) &&
       (memcmp(encoded, expected_data, expected_size) == 0));
    
#if 0
    printf(".encoded_size = %d, .encoded = { ", (buffer_end - encoded));
    for(int c = 0; c < (buffer_end - encoded); ++c)
        printf("0x%02X, ", encoded[c]);
    printf("},\n");
#endif
    
    CFDictionaryRef decoded = NULL;
    
    const uint8_t* decode_end = der_decode_dictionary(NULL, kCFPropertyListMutableContainersAndLeaves,
                                                      &decoded, NULL, encoded, buffer_end);
    
    ok(decode_end == buffer_end, "didn't decode whole buffer");
    ok((decoded != NULL) && CFEqual(decoded, testValue), "Didn't make equal value.");
    
    CFPropertyListRef decoded_type = NULL;
    
    decode_end = der_decode_plist(NULL, kCFPropertyListMutableContainersAndLeaves,
                                  &decoded_type, NULL, encoded, buffer_end);
    
    ok(decode_end == buffer_end, "didn't decode whole buffer");
    ok((decoded != NULL) && CFEqual(decoded_type, testValue), "Didn't make equal value.");
    
    ok(der_sizeof_dictionary(testValue, NULL) == expected_size, "Size correct.");
    ok(der_sizeof_plist(testValue, NULL) == expected_size, "Size correct.");
    
    CFReleaseNull(decoded);
    CFReleaseNull(decoded_type);
}


#define kTestsPerTestCase (1 + kTestsPerDictionaryTest)
static void one_test(const struct test_case * thisCase)
{
    CFIndex key_count = 0;
    while (key_count < array_size(thisCase->keys) && thisCase->keys[key_count] != NULL)
        ++key_count;
    
    CFIndex value_count = 0;
    while (value_count < array_size(thisCase->values) && thisCase->values[value_count] != NULL)
        ++value_count;
    
    ok(key_count == value_count);

    CFDictionaryRef testValue = CFDictionaryCreate(NULL, (const void**)thisCase->keys, (const void**)thisCase->values, key_count,
                                                   &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    test_dictionary(testValue, thisCase->encoded_size, thisCase->encoded);
    
    CFReleaseNull(testValue);
}

#define kTestCount (array_size(test_cases) * kTestsPerTestCase) + kTestsPerDictionaryTest
static void tests(void)
{
    for (int testnumber = 0; testnumber < array_size(test_cases); ++testnumber)
        one_test(test_cases + testnumber);
    
    
    // Big honking test case.
    CFMutableDictionaryRef dictionary = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    const uint8_t some[] = { 0x10, 0xFF, 0x00, 0x12, 0xA5 };
    CFDataRef someData = CFDataCreate(NULL, some, array_size(some));

    const void * arrayElements[] = { kCFBooleanFalse, someData, CFSTR("In Array"), };
    
    CFArrayRef arrayValue = CFArrayCreate(NULL, arrayElements, array_size(arrayElements), &kCFTypeArrayCallBacks);
    CFReleaseNull(someData);
    
    const uint8_t key[] = { 0xFC, 0xFF, 0xFA };
    CFDataRef dataKey = CFDataCreate(NULL, key, array_size(key));
    
    CFDictionaryAddValue(dictionary, dataKey, arrayValue);
    CFReleaseNull(dataKey);
    CFReleaseNull(arrayValue);
    
    int numberValueValue = 2313;
    CFNumberRef numberValue = CFNumberCreate(NULL, kCFNumberIntType, &numberValueValue);

    CFDictionaryAddValue(dictionary, CFSTR("Oh yeah"), kCFBooleanTrue);
    CFDictionaryAddValue(dictionary, kCFBooleanFalse, numberValue);
    CFReleaseNull(numberValue);

    int numberKeyValue = 2313;
    CFNumberRef numberKey = CFNumberCreate(NULL, kCFNumberIntType, &numberKeyValue);
    
    CFDictionaryAddValue(dictionary, numberKey, kCFBooleanTrue);
    CFReleaseNull(numberKey);
    
    uint8_t expected_result[] = { 0x31, 0x3D, 0x30, 0x07, 0x01, 0x01, 0x00, 0x02, 0x02, 0x09, 0x09, 0x30, 0x07, 0x02, 0x02, 0x09, 0x09, 0x01, 0x01, 0x01, 0x30, 0x0C, 0x0C, 0x07, 0x4F, 0x68, 0x20, 0x79, 0x65, 0x61, 0x68, 0x01, 0x01, 0x01, 0x30, 0x1B, 0x04, 0x03, 0xFC, 0xFF, 0xFA, 0x30, 0x14, 0x01, 0x01, 0x00, 0x04, 0x05, 0x10, 0xFF, 0x00, 0x12, 0xA5, 0x0C, 0x08, 0x49, 0x6E, 0x20, 0x41, 0x72, 0x72, 0x61, 0x79, };
    test_dictionary(dictionary, array_size(expected_result), expected_result);
    CFReleaseSafe(dictionary);
}

int su_15_cfdictionary_der(int argc, char *const *argv)
{
    plan_tests(kTestCount);
    tests();

    return 0;
}
