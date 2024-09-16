// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 ****************************************************************************
 * Copyright (c) 1997-2014, International Business Machines Corporation and *
 * others. All Rights Reserved.                                             *
 ****************************************************************************
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#if APPLE_ICU_CHANGES
// rdar://125359625 (Provide access to ICU's C++ TimeZone API via C interface)

#include "unicode/uatimezone.h"

#include "cintltst.h"
#include "cmemory.h"

#include <stdio.h> // for sprintf()

static void TestDisplayName(void);
static void TestGetOffset(void);
static void TestGetOffsetFromLocal(void);
static void TestGetTimeZoneTransitionDate(void);

void addCTZTest(TestNode** root);

#define TESTCASE(x) addTest(root, &x, "tsformat/uatimezonetest/" #x)

void addCTZTest(TestNode** root)
{
    TESTCASE(TestDisplayName);
    TESTCASE(TestGetOffset);
    TESTCASE(TestGetOffsetFromLocal);
    TESTCASE(TestGetTimeZoneTransitionDate);
}

static void TestDisplayName(void) {
    typedef struct {
        const char* locale;
        UTimeZoneDisplayNameType type;
        const UChar* expectedResult;
    } TimeZoneNameTestCase;
    
    TimeZoneNameTestCase testCases[] = {
        { "en_US",  UTIMEZONE_SHORT,     u"PST" },
        { "en_US",  UTIMEZONE_LONG,      u"Pacific Standard Time" },
        { "en_US",  UTIMEZONE_SHORT_DST, u"PDT" },
        { "en_US",  UTIMEZONE_LONG_DST,  u"Pacific Daylight Time" },
        { "es_419", UTIMEZONE_SHORT,     u"GMT-8" },
        { "es_419", UTIMEZONE_LONG,      u"hora estándar del Pacífico" },
        { "es_419", UTIMEZONE_SHORT_DST, u"GMT-7" },
        { "es_419", UTIMEZONE_LONG_DST,  u"hora de verano del Pacífico" },
        { "en_GB",  UTIMEZONE_SHORT,     u"GMT-8" },
        { "en_GB",  UTIMEZONE_LONG,      u"Pacific Standard Time" },
        { "en_GB",  UTIMEZONE_SHORT_DST, u"GMT-7" },
        { "en_GB",  UTIMEZONE_LONG_DST,  u"Pacific Daylight Time" },
    };

    UErrorCode err = U_ZERO_ERROR;
    UTimeZone* tz = uatimezone_open(u"America/Los_Angeles", -1, &err);
    
    if (assertSuccess("Failed to create UTimeZone", &err)) {
        for (int32_t i = 0; i < UPRV_LENGTHOF(testCases); i++) {
            UChar name[200];
            char errorMessage[200];
            
            sprintf(errorMessage, "Wrong display name (%d, %s)", testCases[i].type, testCases[i].locale);
            uatimezone_getDisplayName(tz, testCases[i].type, testCases[i].locale, name, 200, &err);
            
            if (assertSuccess("Error getting display name", &err)) {
                assertUEquals(errorMessage, testCases[i].expectedResult, name);
            }
        }
        
        uatimezone_close(tz);
    }
}

static void TestGetOffset(void) {
    typedef struct {
        UDate utcDate;
        UDate localDate;
        int32_t expectedRawOffset;
        int32_t expectedDstOffset;
    } TimeZoneOffsetTestCase;
    
    TimeZoneOffsetTestCase testCases[] = {
        { 1710027000000.0 /* Saturday, March 9, 2024, 3:30 PM PST */,    1709998200000.0, -8, 0 },
        { 1710063000000.0 /* Sunday, March 10, 2024, 1:30 AM PST */,     1710034200000.0, -8, 0 },
        // NOTE: This is the one case where the local offset is not the same as the UTC offset minus the
        // raw time zone offset-- we have to add an extra adjustment to account for the DST transition
        { 1710066600000.0 /* Sunday, March 10, 2024, 3:30 AM PDT */,     1710041400000.0, -8, 1 },
        { 1710109800000.0 /* Sunday, March 10, 2024, 3:30 PM PDT */,     1710081000000.0, -8, 1 },
        { 1710714600000.0 /* Sunday, March 17, 2024, 3:30 PM PDT */,     1710685800000.0, -8, 1 },
        { 1730586600000.0 /* Saturday, November 2, 2024, 3:30 PM PDT */, 1730557800000.0, -8, 1 },
        { 1730619000000.0 /* Sunday, November 3, 2024, 12:30 AM PDT */,  1730590200000.0, -8, 1 },
        { 1730622600000.0 /* Sunday, November 3, 2024, 1:30 AM PDT */,   1730593800000.0, -8, 1 },
        { 1730626200000.0 /* Sunday, November 3, 2024, 1:30 AM PST */,   1730597400000.0, -8, 0 },
        { 1730629800000.0 /* Sunday, November 3, 2024, 2:30 AM PST */,   1730601000000.0, -8, 0 },
        { 1730676600000.0 /* Sunday, November 3, 2024, 3:30 PM PST */,   1730647800000.0, -8, 0 },
    };
    UErrorCode err = U_ZERO_ERROR;
    UTimeZone* tz = uatimezone_open(u"America/Los_Angeles", -1, &err);

    if (assertSuccess("Failed to create UTimeZone", &err)) {
        const double kMillisInHour = 3600000.0;
        
        int32_t actualRawOffset;
        int32_t actualDstOffset;
        char errorMessage[200];

        for (int32_t i = 0; i < UPRV_LENGTHOF(testCases); i++) {
            uatimezone_getOffset(tz, testCases[i].utcDate, false, &actualRawOffset, &actualDstOffset, &err);
            if (assertSuccess("Failed to get offset", &err)) {
                sprintf(errorMessage, "Wrong raw offset for UTC date %f", testCases[i].utcDate);
                assertIntEquals(errorMessage, testCases[i].expectedRawOffset * kMillisInHour, actualRawOffset);

                sprintf(errorMessage, "Wrong DST offset for UTC date %f", testCases[i].utcDate);
                assertIntEquals(errorMessage, testCases[i].expectedDstOffset * kMillisInHour, actualDstOffset);
            }

            uatimezone_getOffset(tz, testCases[i].localDate, true, &actualRawOffset, &actualDstOffset, &err);
            if (assertSuccess("Failed to get offset", &err)) {
                sprintf(errorMessage, "Wrong raw offset for local date %f (UTC date %f)", testCases[i].localDate, testCases[i].utcDate);
                assertIntEquals(errorMessage, testCases[i].expectedRawOffset * kMillisInHour, actualRawOffset);

                sprintf(errorMessage, "Wrong DST offset for local date %f (UTC date %f)", testCases[i].localDate, testCases[i].utcDate);
                assertIntEquals(errorMessage, testCases[i].expectedDstOffset * kMillisInHour, actualDstOffset);
            }
        }

        uatimezone_close(tz);
    }
}

static void TestGetOffsetFromLocal(void) {
    typedef struct {
        UDate date;
        int32_t expectedRawOffset;
        int32_t expectedDstOffsetWithFormer;
        int32_t expectedDstOffsetWithLatter;
    } TimeZoneOffsetTestCase;
    
    TimeZoneOffsetTestCase testCases[] = {
        { 1709998200000.0, /* Saturday, March 9, 2024, 3:30 PM PST */    -8, 0, 0 },
        { 1710034200000.0, /* Sunday, March 10, 2024, 1:30 AM PST */     -8, 0, 0 },
        { 1710041400000.0, /* Sunday, March 10, 2024, 3:30 AM PDT */     -8, 1, 1 },
        { 1710081000000.0, /* Sunday, March 10, 2024, 3:30 PM PDT */     -8, 1, 1 },
        { 1710685800000.0, /* Sunday, March 17, 2024, 3:30 PM PDT */     -8, 1, 1 },
        { 1730557800000.0, /* Saturday, November 2, 2024, 3:30 PM PDT */ -8, 1, 1 },
        { 1730590200000.0, /* Sunday, November 3, 2024, 12:30 AM PDT */  -8, 1, 1 },
        { 1730593800000.0, /* Sunday, November 3, 2024, 1:30 AM PDT */   -8, 1, 1 },
        // this test case is in the repeated hour, making it the one test case where the
        // UCAL_TZ_LOCAL_FORMER/UCAL_TZ_LOCAL_LATTER distinction makes a difference
        { 1730597400000.0, /* Sunday, November 3, 2024, 1:30 AM PST */   -8, 1, 0 },
        { 1730601000000.0, /* Sunday, November 3, 2024, 2:30 AM PST */   -8, 0, 0 },
        { 1730647800000.0, /* Sunday, November 3, 2024, 3:30 PM PST */   -8, 0, 0 },
    };
    UErrorCode err = U_ZERO_ERROR;
    UTimeZone* tz = uatimezone_open(u"America/Los_Angeles", -1, &err);

    if (assertSuccess("Failed to create UTimeZone", &err)) {
        const double kMillisInHour = 3600000.0;
        
        int32_t actualRawOffset;
        int32_t actualDstOffset;
        char errorMessage[200];

        for (int32_t i = 0; i < UPRV_LENGTHOF(testCases); i++) {
            uatimezone_getOffsetFromLocal(tz, UCAL_TZ_LOCAL_FORMER, UCAL_TZ_LOCAL_FORMER, testCases[i].date,
                &actualRawOffset, &actualDstOffset, &err);
            if (assertSuccess("Failed to get offset", &err)) {
                sprintf(errorMessage, "Wrong raw offset for date %f and UCAL_TZ_LOCAL_FORMER", testCases[i].date);
                assertIntEquals(errorMessage, testCases[i].expectedRawOffset * kMillisInHour, actualRawOffset);

                sprintf(errorMessage, "Wrong DST offset for date %f and UCAL_TZ_LOCAL_FORMER", testCases[i].date);
                assertIntEquals(errorMessage, testCases[i].expectedDstOffsetWithFormer * kMillisInHour, actualDstOffset);
            }
            
            uatimezone_getOffsetFromLocal(tz, UCAL_TZ_LOCAL_LATTER, UCAL_TZ_LOCAL_LATTER, testCases[i].date,
                &actualRawOffset, &actualDstOffset, &err);
            if (assertSuccess("Failed to get offset", &err)) {
                sprintf(errorMessage, "Wrong raw offset for date %f and UCAL_TZ_LOCAL_LATTER", testCases[i].date);
                assertIntEquals(errorMessage, testCases[i].expectedRawOffset * kMillisInHour, actualRawOffset);

                sprintf(errorMessage, "Wrong DST offset for date %f and UCAL_TZ_LOCAL_LATTER", testCases[i].date);
                assertIntEquals(errorMessage, testCases[i].expectedDstOffsetWithLatter * kMillisInHour, actualDstOffset);
            }
        }

        uatimezone_close(tz);
    }
}

static void TestGetTimeZoneTransitionDate(void) {
    typedef struct {
        UDate baseDate;
        UDate nextTransition;
        UDate previousTransition;
    } TimeZoneTransitionTestCase;
    
    TimeZoneTransitionTestCase testCases[] = {
        { 1718481443894.0 /* noon, June 15, 2024, PDT */,
            1730624400000.0 /* 1AM, November 3, 2024 */, 1710064800000.0 /* 3AM, March 3, 2024 */ },
        { 1735937977824.0 /* noon, January 3, 2025, PST */,
            1741514400000.0 /* 3AM, March 9, 2025 */,    1730624400000.0 /* 1AM, November 3, 2024 */ }
    };

    UErrorCode err = U_ZERO_ERROR;
    UTimeZone* tz = uatimezone_open(u"America/Los_Angeles", -1, &err);
    
    if (assertSuccess("Failed to create UTimeZone", &err)) {
        for (int32_t i = 0; i < UPRV_LENGTHOF(testCases); i++) {
            UDate transition;
            char errorMessage[200];
            bool success = uatimezone_getTimeZoneTransitionDate(tz,
                                                 testCases[i].baseDate,
                                                 UCAL_TZ_TRANSITION_NEXT,
                                                 &transition,
                                                 &err);
            sprintf(errorMessage, "Failed to find transition for base date %f in direction UCAL_TZ_TRANSITION_NEXT",
                    testCases[i].baseDate);
            if (assertSuccess(errorMessage, &err)) {
                sprintf(errorMessage, "No transition found for base date %f in direction UCAL_TZ_TRANSITION_NEXT", testCases[i].baseDate);
                assertTrue(errorMessage, success);
                sprintf(errorMessage, "Wrong transition date for base date %f in direction UCAL_TZ_TRANSITION_NEXT",
                        testCases[i].baseDate);
                assertDoubleEquals(errorMessage, testCases[i].nextTransition, transition);
            }
            
            err = U_ZERO_ERROR;
            success = uatimezone_getTimeZoneTransitionDate(tz,
                                                 testCases[i].baseDate,
                                                           UCAL_TZ_TRANSITION_PREVIOUS,
                                                 &transition,
                                                 &err);
            sprintf(errorMessage, "Failed to find transition for base date %f in direction UCAL_TZ_TRANSITION_PREVIOUS",
                    testCases[i].baseDate);
            if (assertSuccess(errorMessage, &err)) {
                sprintf(errorMessage, "No transition found for base date %f in direction UCAL_TZ_TRANSITION_PREVIOUS", testCases[i].baseDate);
                assertTrue(errorMessage, success);
                sprintf(errorMessage, "Wrong transition date for base date %f in direction UCAL_TZ_TRANSITION_PREVIOUS",
                        testCases[i].baseDate);
                assertDoubleEquals(errorMessage, testCases[i].previousTransition, transition);
            }
        }

        uatimezone_close(tz);
    }
}


#endif // APPLE_ICU_CHANGES

#endif /* #if !UCONFIG_NO_FORMATTING */
