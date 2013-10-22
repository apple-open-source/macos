//
//  su-13-cfnumber-der.c
//  utilities
//
//  Created by Mitch Adler on 6/20/12.
//  Copyright (c) 2012 Apple Inc. All rights reserved.
//

#include "utilities/der_plist.h"
#include "utilities/der_plist_internal.h"

#include "utilities/SecCFRelease.h"
#include "utilities/array_size.h"

#include <CoreFoundation/CoreFoundation.h>

#include "utilities_regressions.h"

#define kMaxResultSize 30

struct test_case {
    long long value;
    size_t    encoded_size;
    uint8_t   encoded[20];
};

static struct test_case test_cases[] =
{
    { .value = 0, .encoded_size = 3,    .encoded = { 0x02, 0x01, 0x00 }, },
    { .value = 1, .encoded_size = 3, .encoded = { 0x02, 0x01, 0x01 }, },
    { .value = 128, .encoded_size = 4, .encoded = { 0x02, 0x02, 0x00, 0x80 }, },
    { .value = -1, .encoded_size = 3, .encoded = { 0x02, 0x01, 0xFF }, },
    { .value = -129, .encoded_size = 4, .encoded = { 0x02, 0x02, 0xFF, 0x7F }, },
    { .value = 1000, .encoded_size = 4, .encoded = { 0x02, 0x02, 0x03, 0xE8, }, },
    { .value = 65280, .encoded_size = 5, .encoded = { 0x02, 0x03, 0x00, 0xFF, 0x00 }, },
    { .value = 41234576, .encoded_size = 6, .encoded = { 0x02, 0x04, 0x02, 0x75, 0x30, 0x90 }, },
    { .value = -412343576, .encoded_size = 6, .encoded = { 0x02, 0x04, 0xE7, 0x6C, 0x22, 0xE8 }, },
};

#define kTestsPerTestCase 8
static void one_test(const struct test_case * thisCase)
{
    uint8_t buffer[kMaxResultSize];
    uint8_t* buffer_end = buffer + sizeof(buffer);
    
    CFNumberRef initialValue = CFNumberCreate(NULL, kCFNumberLongLongType, &thisCase->value);

    uint8_t* encoded = der_encode_plist(initialValue, NULL, buffer, buffer_end);

    ok(encoded != NULL &&
       (thisCase->encoded_size == (buffer_end - encoded)) &&
       (memcmp(encoded, thisCase->encoded, thisCase->encoded_size) == 0));

    encoded = der_encode_number(initialValue, NULL, buffer, buffer_end);

    ok(encoded != NULL &&
       (thisCase->encoded_size == (buffer_end - encoded)) &&
       (memcmp(encoded, thisCase->encoded, thisCase->encoded_size) == 0));

#if 0
    printf(".size = %d, .res = { ", (buffer_end - encoded));
    for(int c = 0; c < (buffer_end - encoded); ++c)
        printf("0x%02X, ", encoded[c]);
    printf("},\n");
#endif

    CFNumberRef decoded = NULL;
    
    const uint8_t* decode_end = der_decode_number(NULL, kCFPropertyListMutableContainersAndLeaves,
                                                  &decoded, NULL, encoded, buffer_end);
    
    ok(decode_end == buffer_end, "didn't decode whole buffer");
    ok((decoded != NULL) && CFEqual(decoded, initialValue), "Didn't make equal value.");

    CFPropertyListRef decoded_type = NULL;

    decode_end = der_decode_plist(NULL, kCFPropertyListMutableContainersAndLeaves,
                                  &decoded_type, NULL, encoded, buffer_end);

    ok(decode_end == buffer_end, "didn't decode whole buffer");
    ok((decoded != NULL) && CFEqual(decoded_type, initialValue), "Didn't make equal value.");

    ok(der_sizeof_number(initialValue, NULL) == thisCase->encoded_size, "Size correct.");
    ok(der_sizeof_plist(initialValue, NULL) == thisCase->encoded_size, "Size correct.");

    CFReleaseNull(decoded);
    CFReleaseNull(decoded_type);
}

#define kTestCount (array_size(test_cases) * kTestsPerTestCase)
static void tests(void)
{
    for (int testnumber = 0; testnumber < array_size(test_cases); ++testnumber)
        one_test(test_cases + testnumber);
}

int su_13_cfnumber_der(int argc, char *const *argv)
{
    plan_tests(kTestCount);
    tests();

    return 0;
}
