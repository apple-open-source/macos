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

#define kMaxResultSize 16

#define kTestsPerTestCase 8
static void one_test(CFBooleanRef value, size_t der_size, const uint8_t *expected_der)
{

    uint8_t buffer[kMaxResultSize];
    uint8_t *buffer_end = buffer + sizeof(buffer);

    uint8_t* encoded = der_encode_plist(value, NULL, buffer, buffer_end);

    ok(encoded != NULL &&
       (der_size == (buffer_end - encoded)) &&
       (memcmp(encoded, expected_der, der_size) == 0));

    encoded = der_encode_boolean(value, NULL, buffer, buffer_end);

    ok(encoded != NULL &&
       (der_size == (buffer_end - encoded)) &&
       (memcmp(encoded, expected_der, der_size) == 0));

#if 0
    printf(".size = %d, .res = { ", (buffer_end - encoded));
    for(int c = 0; c < (buffer_end - encoded); ++c)
        printf("0x%02X, ", encoded[c]);
    printf("},\n");
#endif

    CFBooleanRef decoded = NULL;
    const uint8_t* decode_end = der_decode_boolean(NULL, kCFPropertyListMutableContainersAndLeaves,
                                                   &decoded, NULL, encoded, buffer_end);

    ok(decode_end == buffer_end);
    ok((decoded != NULL) && CFEqual(decoded, value));

    CFPropertyListRef decoded_type = NULL;
    decode_end = der_decode_plist(NULL, kCFPropertyListMutableContainersAndLeaves,
                                  &decoded_type, NULL, encoded, buffer_end);

    ok(decode_end == buffer_end);
    ok((decoded != NULL) && CFEqual(decoded_type, value));

    ok(der_sizeof_boolean(value, NULL) == der_size);
    ok(der_sizeof_plist(value, NULL) == der_size);

    CFReleaseNull(decoded);
    CFReleaseNull(decoded_type);
}

#define kTestCount (2 * kTestsPerTestCase)
static void tests(void)
{
    const uint8_t der_true[] = { 0x01, 0x01, 0x01 };

    one_test(kCFBooleanTrue, sizeof(der_true), der_true);
    
    const uint8_t der_false[] = { 0x01, 0x01, 0x00 };
    
    one_test(kCFBooleanFalse, sizeof(der_false), der_false);
}

int su_12_cfboolean_der(int argc, char *const *argv)
{
    plan_tests(kTestCount);
    tests();

    return 0;
}
