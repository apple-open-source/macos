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


#include "utilities/der_plist.h"
#include "utilities/der_plist_internal.h"
#include "utilities/der_date.h"

#include "utilities/SecCFRelease.h"
#include "utilities/array_size.h"
#include <utilities/SecCFWrappers.h>

#include <CoreFoundation/CoreFoundation.h>
#include <corecrypto/ccder.h>
#include <dispatch/dispatch.h>

#include "utilities_regressions.h"

#define kMaxResultSize 100

struct test_case {
    CFAbsoluteTime value;
    char      *expected;
};

static struct test_case test_cases[] =
{
    { .value = 0.0,                     .expected = "20010101000000Z", },
    { .value = 1.0,                     .expected = "20010101000001Z", },
    { .value = 0.1,                     .expected = "20010101000000.1Z", },
    { .value = 0.0001,                  .expected = "20010101000000.0001Z", },
    { .value = 128.0,                   .expected = "20010101000208Z", },
    { .value = -1.0,                    .expected = "20001231235959Z", },
    { .value = -0.9,                    .expected = "20001231235959.1Z", },
    { .value = -129.0,                  .expected = "20001231235751Z", },
    { .value = 1000.0,                  .expected = "20010101001640Z", },
    { .value = 65280.0,                 .expected = "20010101180800Z", },
    { .value = 41234576.0,              .expected = "20020423060256Z", },
    { .value = -412343576.0,            .expected = "19871208120704Z", },
    { .value = 381778873.238063,        .expected = "20130205174113.238063Z", },
    { .value = 381778873.638063,        .expected = "20130205174113.638063Z", },
    { .value = 1400603.141,             .expected = "20010117050323.141Z", },
    { .value = -412343576.238063,       .expected = "19871208120703.761937Z", },
    { .value = -412343576.638063,       .expected = "19871208120703.361937Z", },
    { .value = 5.000539014,             .expected = "20010101000005.000539014Z", },
    { .value = -5.00053901,             .expected = "20001231235954.99946099Z", },
    { .value = 0.000539014,             .expected = "20010101000000.000539014Z", },
    { .value = -0.00053901,             .expected = "20001231235959.99946099Z", },
    { .value = 11031400603.141,         .expected = "23500729055643.141Z", }, // Michael's 400th birthday
    // Pedantic tests within 1 second of the epoch.
    { .value = 0.000100120234,          .expected = "20010101000000.000100120234Z", },
    { .value =                  0.00000000000000000000000000000000000000000010012654182354326,
      .expected = "20010101000000.00000000000000000000000000000000000000000010012654182354326Z", },
    { .value =                 -0.00000000000000000000000000000000000000000010012654182354326,
      .expected = "20001231235959.99999999999999999999999999999999999999999989987345817645674Z", },
    { .value = 0.0001234522366234637,   .expected = "20010101000000.0001234522366234637Z", },
};

static CFStringRef string_create_with_hex(const uint8_t* start, const uint8_t* end) {
    CFMutableStringRef s = CFStringCreateMutable(NULL, 0);
    CFStringAppendFormat(s, 0, CFSTR(".size = %" PRIdPTR ", .res = { "), (intptr_t)(end - start));
    if (start)
        while (start < end)
            CFStringAppendFormat(s, 0, CFSTR("0x%02X, "), *start++);
    CFStringAppend(s, CFSTR("},\n"));
    return s;
}

static bool ok_der_date_is(int testnumber, const char *expected, const uint8_t *der, const uint8_t *der_end) {
    size_t elen = strlen(expected);
    size_t dlen;
    const uint8_t *body = ccder_decode_tl(CCDER_GENERALIZED_TIME, &dlen, der, der_end);
    if (!body) {
        return fail("[%d] encoded date %@ expected %s not a generalized time", testnumber, string_create_with_hex(der, der_end), expected);
    } else if (body + dlen != der_end) {
        return fail("[%d] Trailing garbage in encoded string after generalized time got: %@ expected: %s", testnumber, string_create_with_hex(der, der_end), expected);
    } else if (dlen != elen) {
        return fail("[%d] encoded date len %zu != %zu got: %.*s expected: %s", testnumber, dlen , elen, (int)dlen, (char *)body, expected);
    } else if (memcmp(body, expected, elen)) {
        return fail("[%d] encoded got: %.*s expected: %s", testnumber, (int)dlen, (char *)body, expected);
    } else {
        return pass("[%d] properly encoded %s", testnumber, expected);
    }
}

static bool ok_date_equals(int testnumber, CFDateRef decoded, CFDateRef expected) {
    CFAbsoluteTime t1 = CFDateGetAbsoluteTime(decoded);
    CFAbsoluteTime t2 = CFDateGetAbsoluteTime(expected);
    if (-1.0 < t1 && t1 < 1.0) {
        // Dates near the epoch can't be off by more than 1 nanosecond.  Other dates should be exactly right.
        return ok((decoded != NULL) && fabs(t1 - t2) < 7e-18, "[%d] Delta too big %g %a != %a (%g != %g).", testnumber, fabs(t1 - t2), t1, t2, t1, t2);
    } else {
        return ok((decoded != NULL) && CFEqual(decoded, expected), "[%d] Didn't make equal value %a != %a (%g != %g).", testnumber, t1, t2, t1, t2);
    }
}

static CFCalendarRef sZuluCalendar = NULL;

static CFCalendarRef SecCFCalendarGetZulu() {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sZuluCalendar = CFCalendarCreateWithIdentifier(kCFAllocatorDefault, kCFGregorianCalendar);
        CFTimeZoneRef tz = CFTimeZoneCreateWithTimeIntervalFromGMT(kCFAllocatorDefault, 0.0);
        CFCalendarSetTimeZone(sZuluCalendar, tz);
        CFReleaseSafe(tz);
    });
    return sZuluCalendar;
}

static bool SecAbsoluteTimeGetGregorianDate(CFTimeInterval at, int *year, int *month, int *day, int *hour, int *minute, int *second, CFErrorRef *error) {
    // TODO: Remove CFCalendarDecomposeAbsoluteTime dependancy because CFTimeZoneCreateWithTimeIntervalFromGMT is expensive and requires filesystem access to timezone files when we are only doing zulu time anyway
    if (!CFCalendarDecomposeAbsoluteTime(SecCFCalendarGetZulu(), at, "yMdHms", year, month, day, hour, minute, second)) {
        SecCFDERCreateError(-1000, CFSTR("Failed to encode date."), 0, error);
        return false;
    }
    return true;
}

//                                                          &year, &month, &day, &hour, &minute, &second
CFTimeInterval referenceTimeDate = 416957062.807688;   //  2014    3       19      21      24      22
#define expectedYear 2014
#define expectedMonth 3
#define expectedDay 19
#define expectedHour 21
#define expectedMinute 24
#define expectedSecond 22

static bool parallelizeZulu(bool useSharedZuluCalendar, void(^action)(CFCalendarRef zuluCalendar, bool *STOP)) {
    // If useSharedZuluCalendar is false, NULL will be passed to zuluCalendar parameter of action
    int ix;
    static int kThreadLimit = 75000;                                        // on a J86, this took 49963 threads to fail
    static const int64_t kFailureTimeLimit = (NSEC_PER_SEC * 10);           // Assume failure after 10s
    dispatch_time_t failTime = dispatch_time(DISPATCH_TIME_NOW, kFailureTimeLimit);

    // This is a __block variable since it can get modified (due the the <rdar://problem/16372688>)
    __block CFCalendarRef zuluCalendar = useSharedZuluCalendar ? SecCFCalendarGetZulu() : NULL;
    __block bool stop = false;

    dispatch_group_t dgroup = dispatch_group_create();
    dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);

    for (ix=0;ix<kThreadLimit && !stop;ix++) {
        dispatch_group_enter(dgroup);
        dispatch_async(queue, ^{
            action(zuluCalendar, &stop);
            dispatch_group_leave(dgroup);
        });
    }
    dispatch_group_wait(dgroup, failTime);
    dispatch_release(dgroup);
    return !stop;
}

// We expect this to fail until this is fixed:
//  <rdar://problem/16372688> CFCalendarDecomposeAbsoluteTime is not thread safe
//
static void testWithUnguardedZuluCalendar() {
    const bool useSharedZuluCalendar = true;
    __block bool success = true;
    __block int successCount = 0;

    success &= parallelizeZulu(useSharedZuluCalendar, ^(CFCalendarRef zuluCalendar, bool *STOP) {
        int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
        bool matches = false;
        if (!SecAbsoluteTimeGetGregorianDate(referenceTimeDate, &year, &month, &day, &hour, &minute, &second, NULL))
            success = false;
        matches = year==expectedYear && month==expectedMonth && day==expectedDay &&
        hour==expectedHour && minute==expectedMinute && second==expectedSecond;
//      assert(matches);    // enable to catch crash
        if (matches)
            successCount++;
        else
            *STOP = true;
    });

    todo("<rdar://problem/16372688> CFCalendarDecomposeAbsoluteTime is not thread safe, not yet fixed");

    TODO: {
        ok(success,"unexpected result from SecAbsoluteTimeGetGregorianDate, failed, successes: %d", successCount);
    }
}

static void testDoWithZulu() {
    const bool useSharedZuluCalendar = false;
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    __block bool success = true;

    success &= parallelizeZulu(useSharedZuluCalendar, ^(CFCalendarRef zuluCalendar, bool *STOP) {
        SecCFCalendarDoWithZuluCalendar(^(CFCalendarRef zuluCalendar) {
            bool matches = false;
            success &= CFCalendarDecomposeAbsoluteTime(zuluCalendar, referenceTimeDate, "yMdHms", &year, &month, &day, &hour, &minute, &second);
            matches = year==expectedYear && month==expectedMonth && day==expectedDay &&
                hour==expectedHour && minute==expectedMinute && second==expectedSecond;
            *STOP = !matches;
        });
    });

    ok(success,"unexpected result from CFCalendarDecomposeAbsoluteTime");
}

#define kTestsPerTestCase 12
static void one_test(const struct test_case * thisCase, int testnumber)
{
    uint8_t buffer[kMaxResultSize];
    uint8_t* buffer_end = buffer + sizeof(buffer);

    CFDateRef initialValue = CFDateCreate(NULL, thisCase->value);
    CFErrorRef error = NULL;

    uint8_t* encoded = der_encode_plist(initialValue, &error, buffer, buffer_end);
    SKIP:
    {
        skip("der_encode_plist failed", 1,
             ok(encoded != NULL, "[%d] der_encode_plist failed: %@", testnumber, error));
        ok_der_date_is(testnumber, thisCase->expected, encoded, buffer_end);
    }
    CFReleaseNull(error);

    encoded = der_encode_date(initialValue, &error, buffer, buffer_end);
    SKIP:
    {
        skip("der_encode_date failed", 1,
             ok(encoded != NULL, "[%d] der_encode_date failed: %@", testnumber, error));
        ok_der_date_is(testnumber, thisCase->expected, encoded, buffer_end);
    }
    CFReleaseNull(error);

    CFDateRef decoded = NULL;
    const uint8_t* decode_end = der_decode_date(NULL, kCFPropertyListMutableContainers,
                                                  &decoded, &error, encoded, buffer_end);
    ok(error == NULL, "[%d] der_decode_date failed: %@", testnumber, error);
    CFReleaseNull(error);

    ok(decode_end == buffer_end, "[%d] didn't decode whole buffer", testnumber);
    ok_date_equals(testnumber, decoded, initialValue);

    CFPropertyListRef decoded_type = NULL;

    decode_end = der_decode_plist(NULL, kCFPropertyListMutableContainers,
                                  &decoded_type, &error, encoded, buffer_end);
    ok(error == NULL, "[%d] der_decode_plist failed: %@", testnumber, error);
    CFReleaseNull(error);

    ok(decode_end == buffer_end, "[%d] didn't decode whole buffer", testnumber);
    ok_date_equals(testnumber, decoded, initialValue);

    is(der_sizeof_date(initialValue, NULL), ccder_sizeof(CCDER_GENERALIZED_TIME, strlen(thisCase->expected)), "[%d] der_sizeof_date mismatch", testnumber);
    is(der_sizeof_plist(initialValue, NULL), ccder_sizeof(CCDER_GENERALIZED_TIME, strlen(thisCase->expected)), "[%d] der_sizeof_plist mismatch", testnumber);

    CFReleaseSafe(initialValue);
    CFReleaseNull(decoded);
    CFReleaseNull(decoded_type);
}

#define kTestCount (array_size(test_cases) * kTestsPerTestCase)
static void tests(void)
{
    for (int testnumber = 0; testnumber < array_size(test_cases); ++testnumber)
        one_test(test_cases + testnumber, testnumber);

    testWithUnguardedZuluCalendar();
    testDoWithZulu();
}

int su_16_cfdate_der(int argc, char *const *argv)
{
    plan_tests(kTestCount+2);
    tests();

    return 0;
}
