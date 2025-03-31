// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
* Copyright (C) 2003-2015, International Business Machines Corporation
* and others. All Rights Reserved.
******************************************************************************
*
* File ISLAMCAL.H
*
* Modification History:
*
*   Date        Name        Description
*   10/14/2003  srl         ported from java IslamicCalendar
*****************************************************************************
*/

#include "islamcal.h"

#if !UCONFIG_NO_FORMATTING

#include "umutex.h"
#include <float.h>
#include "gregoimp.h" // Math
#include "astro.h" // CalendarAstronomer
#include "uhash.h"
#include "ucln_in.h"
#include "uassert.h"

static const UDate HIJRA_MILLIS = -42521587200000.0;    // 7/16/622 AD 00:00

// Debugging
#ifdef U_DEBUG_ISLAMCAL
# include <stdio.h>
# include <stdarg.h>
static void debug_islamcal_loc(const char *f, int32_t l)
{
    fprintf(stderr, "%s:%d: ", f, l);
}

static void debug_islamcal_msg(const char *pat, ...)
{
    va_list ap;
    va_start(ap, pat);
    vfprintf(stderr, pat, ap);
    fflush(stderr);
}
// must use double parens, i.e.:  U_DEBUG_ISLAMCAL_MSG(("four is: %d",4));
#define U_DEBUG_ISLAMCAL_MSG(x) {debug_islamcal_loc(__FILE__,__LINE__);debug_islamcal_msg x;}
#else
#define U_DEBUG_ISLAMCAL_MSG(x)
#endif


// --- The cache --
// cache of months
static icu::CalendarCache *gMonthCache = nullptr;

U_CDECL_BEGIN
static UBool calendar_islamic_cleanup() {
    if (gMonthCache) {
        delete gMonthCache;
        gMonthCache = nullptr;
    }
    return true;
}
U_CDECL_END

U_NAMESPACE_BEGIN

// Implementation of the IslamicCalendar class

/**
 * Friday EPOC
 */
static const int32_t CIVIL_EPOC = 1948440; // CE 622 July 16 Friday (Julian calendar) / CE 622 July 19 (Gregorian calendar)

/**
  * Thursday EPOC
  */
static const int32_t ASTRONOMICAL_EPOC = 1948439; // CE 622 July 15 Thursday (Julian calendar)


static const int32_t UMALQURA_YEAR_START = 1300;
static const int32_t UMALQURA_YEAR_END = 1600;

static const int UMALQURA_MONTHLENGTH[] = {
    //* 1300 -1302 */ "1010 1010 1010", "1101 0101 0100", "1110 1100 1001",
                            0x0AAA,           0x0D54,           0x0EC9,
    //* 1303 -1307 */ "0110 1101 0100", "0110 1110 1010", "0011 0110 1100", "1010 1010 1101", "0101 0101 0101",
                            0x06D4,           0x06EA,           0x036C,           0x0AAD,           0x0555,
    //* 1308 -1312 */ "0110 1010 1001", "0111 1001 0010", "1011 1010 1001", "0101 1101 0100", "1010 1101 1010",
                            0x06A9,           0x0792,           0x0BA9,           0x05D4,           0x0ADA,
    //* 1313 -1317 */ "0101 0101 1100", "1101 0010 1101", "0110 1001 0101", "0111 0100 1010", "1011 0101 0100",
                            0x055C,           0x0D2D,           0x0695,           0x074A,           0x0B54,
    //* 1318 -1322 */ "1011 0110 1010", "0101 1010 1101", "0100 1010 1110", "1010 0100 1111", "0101 0001 0111",
                            0x0B6A,           0x05AD,           0x04AE,           0x0A4F,           0x0517,
    //* 1323 -1327 */ "0110 1000 1011", "0110 1010 0101", "1010 1101 0101", "0010 1101 0110", "1001 0101 1011",
                            0x068B,           0x06A5,           0x0AD5,           0x02D6,           0x095B,
    //* 1328 -1332 */ "0100 1001 1101", "1010 0100 1101", "1101 0010 0110", "1101 1001 0101", "0101 1010 1100",
                            0x049D,           0x0A4D,           0x0D26,           0x0D95,           0x05AC,
    //* 1333 -1337 */ "1001 1011 0110", "0010 1011 1010", "1010 0101 1011", "0101 0010 1011", "1010 1001 0101",
                            0x09B6,           0x02BA,           0x0A5B,           0x052B,           0x0A95,
    //* 1338 -1342 */ "0110 1100 1010", "1010 1110 1001", "0010 1111 0100", "1001 0111 0110", "0010 1011 0110",
                            0x06CA,           0x0AE9,           0x02F4,           0x0976,           0x02B6,
    //* 1343 -1347 */ "1001 0101 0110", "1010 1100 1010", "1011 1010 0100", "1011 1101 0010", "0101 1101 1001",
                            0x0956,           0x0ACA,           0x0BA4,           0x0BD2,           0x05D9,
    //* 1348 -1352 */ "0010 1101 1100", "1001 0110 1101", "0101 0100 1101", "1010 1010 0101", "1011 0101 0010",
                            0x02DC,           0x096D,           0x054D,           0x0AA5,           0x0B52,
    //* 1353 -1357 */ "1011 1010 0101", "0101 1011 0100", "1001 1011 0110", "0101 0101 0111", "0010 1001 0111",
                            0x0BA5,           0x05B4,           0x09B6,           0x0557,           0x0297,
    //* 1358 -1362 */ "0101 0100 1011", "0110 1010 0011", "0111 0101 0010", "1011 0110 0101", "0101 0110 1010",
                            0x054B,           0x06A3,           0x0752,           0x0B65,           0x056A,
    //* 1363 -1367 */ "1010 1010 1011", "0101 0010 1011", "1100 1001 0101", "1101 0100 1010", "1101 1010 0101",
                            0x0AAB,           0x052B,           0x0C95,           0x0D4A,           0x0DA5,
    //* 1368 -1372 */ "0101 1100 1010", "1010 1101 0110", "1001 0101 0111", "0100 1010 1011", "1001 0100 1011",
                            0x05CA,           0x0AD6,           0x0957,           0x04AB,           0x094B,
    //* 1373 -1377 */ "1010 1010 0101", "1011 0101 0010", "1011 0110 1010", "0101 0111 0101", "0010 0111 0110",
                            0x0AA5,           0x0B52,           0x0B6A,           0x0575,           0x0276,
    //* 1378 -1382 */ "1000 1011 0111", "0100 0101 1011", "0101 0101 0101", "0101 1010 1001", "0101 1011 0100",
                            0x08B7,           0x045B,           0x0555,           0x05A9,           0x05B4,
    //* 1383 -1387 */ "1001 1101 1010", "0100 1101 1101", "0010 0110 1110", "1001 0011 0110", "1010 1010 1010",
                            0x09DA,           0x04DD,           0x026E,           0x0936,           0x0AAA,
    //* 1388 -1392 */ "1101 0101 0100", "1101 1011 0010", "0101 1101 0101", "0010 1101 1010", "1001 0101 1011",
                            0x0D54,           0x0DB2,           0x05D5,           0x02DA,           0x095B,
    //* 1393 -1397 */ "0100 1010 1011", "1010 0101 0101", "1011 0100 1001", "1011 0110 0100", "1011 0111 0001",
                            0x04AB,           0x0A55,           0x0B49,           0x0B64,           0x0B71,
    //* 1398 -1402 */ "0101 1011 0100", "1010 1011 0101", "1010 0101 0101", "1101 0010 0101", "1110 1001 0010",
                            0x05B4,           0x0AB5,           0x0A55,           0x0D25,           0x0E92,
    //* 1403 -1407 */ "1110 1100 1001", "0110 1101 0100", "1010 1110 1001", "1001 0110 1011", "0100 1010 1011",
                            0x0EC9,           0x06D4,           0x0AE9,           0x096B,           0x04AB,
    //* 1408 -1412 */ "1010 1001 0011", "1101 0100 1001", "1101 1010 0100", "1101 1011 0010", "1010 1011 1001",
                            0x0A93,           0x0D49,         0x0DA4,           0x0DB2,           0x0AB9,
    //* 1413 -1417 */ "0100 1011 1010", "1010 0101 1011", "0101 0010 1011", "1010 1001 0101", "1011 0010 1010",
                            0x04BA,           0x0A5B,           0x052B,           0x0A95,           0x0B2A,
    //* 1418 -1422 */ "1011 0101 0101", "0101 0101 1100", "0100 1011 1101", "0010 0011 1101", "1001 0001 1101",
                            0x0B55,           0x055C,           0x04BD,           0x023D,           0x091D,
    //* 1423 -1427 */ "1010 1001 0101", "1011 0100 1010", "1011 0101 1010", "0101 0110 1101", "0010 1011 0110",
                            0x0A95,           0x0B4A,           0x0B5A,           0x056D,           0x02B6,
    //* 1428 -1432 */ "1001 0011 1011", "0100 1001 1011", "0110 0101 0101", "0110 1010 1001", "0111 0101 0100",
                            0x093B,           0x049B,           0x0655,           0x06A9,           0x0754,
    //* 1433 -1437 */ "1011 0110 1010", "0101 0110 1100", "1010 1010 1101", "0101 0101 0101", "1011 0010 1001",
                            0x0B6A,           0x056C,           0x0AAD,           0x0555,           0x0B29,
    //* 1438 -1442 */ "1011 1001 0010", "1011 1010 1001", "0101 1101 0100", "1010 1101 1010", "0101 0101 1010",
                            0x0B92,           0x0BA9,           0x05D4,           0x0ADA,           0x055A,
    //* 1443 -1447 */ "1010 1010 1011", "0101 1001 0101", "0111 0100 1001", "0111 0110 0100", "1011 1010 1010",
                            0x0AAB,           0x0595,           0x0749,           0x0764,           0x0BAA,
    //* 1448 -1452 */ "0101 1011 0101", "0010 1011 0110", "1010 0101 0110", "1110 0100 1101", "1011 0010 0101",
                            0x05B5,           0x02B6,           0x0A56,           0x0E4D,           0x0B25,
    //* 1453 -1457 */ "1011 0101 0010", "1011 0110 1010", "0101 1010 1101", "0010 1010 1110", "1001 0010 1111",
                            0x0B52,           0x0B6A,           0x05AD,           0x02AE,           0x092F,
    //* 1458 -1462 */ "0100 1001 0111", "0110 0100 1011", "0110 1010 0101", "0110 1010 1100", "1010 1101 0110",
                            0x0497,           0x064B,           0x06A5,           0x06AC,           0x0AD6,
    //* 1463 -1467 */ "0101 0101 1101", "0100 1001 1101", "1010 0100 1101", "1101 0001 0110", "1101 1001 0101",
                            0x055D,           0x049D,           0x0A4D,           0x0D16,           0x0D95,
    //* 1468 -1472 */ "0101 1010 1010", "0101 1011 0101", "0010 1101 1010", "1001 0101 1011", "0100 1010 1101",
                            0x05AA,           0x05B5,           0x02DA,           0x095B,           0x04AD,
    //* 1473 -1477 */ "0101 1001 0101", "0110 1100 1010", "0110 1110 0100", "1010 1110 1010", "0100 1111 0101",
                            0x0595,           0x06CA,           0x06E4,           0x0AEA,           0x04F5,
    //* 1478 -1482 */ "0010 1011 0110", "1001 0101 0110", "1010 1010 1010", "1011 0101 0100", "1011 1101 0010",
                            0x02B6,           0x0956,           0x0AAA,           0x0B54,           0x0BD2,
    //* 1483 -1487 */ "0101 1101 1001", "0010 1110 1010", "1001 0110 1101", "0100 1010 1101", "1010 1001 0101",
                            0x05D9,           0x02EA,           0x096D,           0x04AD,           0x0A95,
    //* 1488 -1492 */ "1011 0100 1010", "1011 1010 0101", "0101 1011 0010", "1001 1011 0101", "0100 1101 0110",
                            0x0B4A,           0x0BA5,           0x05B2,           0x09B5,           0x04D6,
    //* 1493 -1497 */ "1010 1001 0111", "0101 0100 0111", "0110 1001 0011", "0111 0100 1001", "1011 0101 0101",
                            0x0A97,           0x0547,           0x0693,           0x0749,           0x0B55,
    //* 1498 -1508 */ "0101 0110 1010", "1010 0110 1011", "0101 0010 1011", "1010 1000 1011", "1101 0100 0110", "1101 1010 0011", "0101 1100 1010", "1010 1101 0110", "0100 1101 1011", "0010 0110 1011", "1001 0100 1011",
                            0x056A,           0x0A6B,           0x052B,           0x0A8B,           0x0D46,           0x0DA3,           0x05CA,           0x0AD6,           0x04DB,           0x026B,           0x094B,
    //* 1509 -1519 */ "1010 1010 0101", "1011 0101 0010", "1011 0110 1001", "0101 0111 0101", "0001 0111 0110", "1000 1011 0111", "0010 0101 1011", "0101 0010 1011", "0101 0110 0101", "0101 1011 0100", "1001 1101 1010",
                            0x0AA5,           0x0B52,           0x0B69,           0x0575,           0x0176,           0x08B7,           0x025B,           0x052B,           0x0565,           0x05B4,           0x09DA,
    //* 1520 -1530 */ "0100 1110 1101", "0001 0110 1101", "1000 1011 0110", "1010 1010 0110", "1101 0101 0010", "1101 1010 1001", "0101 1101 0100", "1010 1101 1010", "1001 0101 1011", "0100 1010 1011", "0110 0101 0011",
                            0x04ED,           0x016D,           0x08B6,           0x0AA6,           0x0D52,           0x0DA9,           0x05D4,           0x0ADA,           0x095B,           0x04AB,           0x0653,
    //* 1531 -1541 */ "0111 0010 1001", "0111 0110 0010", "1011 1010 1001", "0101 1011 0010", "1010 1011 0101", "0101 0101 0101", "1011 0010 0101", "1101 1001 0010", "1110 1100 1001", "0110 1101 0010", "1010 1110 1001",
                            0x0729,           0x0762,           0x0BA9,           0x05B2,           0x0AB5,           0x0555,           0x0B25,           0x0D92,           0x0EC9,           0x06D2,           0x0AE9,
    //* 1542 -1552 */ "0101 0110 1011", "0100 1010 1011", "1010 0101 0101", "1101 0010 1001", "1101 0101 0100", "1101 1010 1010", "1001 1011 0101", "0100 1011 1010", "1010 0011 1011", "0100 1001 1011", "1010 0100 1101",
                            0x056B,           0x04AB,           0x0A55,           0x0D29,           0x0D54,           0x0DAA,           0x09B5,           0x04BA,           0x0A3B,           0x049B,           0x0A4D,
    //* 1553 -1563 */ "1010 1010 1010", "1010 1101 0101", "0010 1101 1010", "1001 0101 1101", "0100 0101 1110", "1010 0010 1110", "1100 1001 1010", "1101 0101 0101", "0110 1011 0010", "0110 1011 1001", "0100 1011 1010",
                            0x0AAA,           0x0AD5,           0x02DA,           0x095D,           0x045E,           0x0A2E,           0x0C9A,           0x0D55,           0x06B2,           0x06B9,           0x04BA,
    //* 1564 -1574 */ "1010 0101 1101", "0101 0010 1101", "1010 1001 0101", "1011 0101 0010", "1011 1010 1000", "1011 1011 0100", "0101 1011 1001", "0010 1101 1010", "1001 0101 1010", "1011 0100 1010", "1101 1010 0100",
                            0x0A5D,           0x052D,           0x0A95,           0x0B52,           0x0BA8,           0x0BB4,           0x05B9,           0x02DA,           0x095A,           0x0B4A,           0x0DA4,
    //* 1575 -1585 */ "1110 1101 0001", "0110 1110 1000", "1011 0110 1010", "0101 0110 1101", "0101 0011 0101", "0110 1001 0101", "1101 0100 1010", "1101 1010 1000", "1101 1101 0100", "0110 1101 1010", "0101 0101 1011",
                            0x0ED1,           0x06E8,           0x0B6A,           0x056D,           0x0535,           0x0695,           0x0D4A,           0x0DA8,           0x0DD4,           0x06DA,           0x055B,
    //* 1586 -1596 */ "0010 1001 1101", "0110 0010 1011", "1011 0001 0101", "1011 0100 1010", "1011 1001 0101", "0101 1010 1010", "1010 1010 1110", "1001 0010 1110", "1100 1000 1111", "0101 0010 0111", "0110 1001 0101",
                            0x029D,           0x062B,           0x0B15,           0x0B4A,           0x0B95,           0x05AA,           0x0AAE,           0x092E,           0x0C8F,           0x0527,           0x0695,
    //* 1597 -1600 */ "0110 1010 1010", "1010 1101 0110", "0101 0101 1101", "0010 1001 1101", };
                            0x06AA,           0x0AD6,           0x055D,           0x029D
};

//-------------------------------------------------------------------------
// Constructors...
//-------------------------------------------------------------------------

const char *IslamicCalendar::getType() const {
    return "islamic";
}

IslamicCalendar* IslamicCalendar::clone() const {
    return new IslamicCalendar(*this);
}

IslamicCalendar::IslamicCalendar(const Locale& aLocale, UErrorCode& success)
#if APPLE_ICU_CHANGES
// rdar://100197751 (QFA: Islamic Lunar Calendar Improvements)
:   Calendar(TimeZone::forLocaleOrDefault(aLocale), aLocale, success), fLatitude(0.0), fLongitude(0.0)
{
    setLocation(aLocale.getCountry());
#else
    :   Calendar(TimeZone::forLocaleOrDefault(aLocale), aLocale, success)
    {
#endif // APPLE_ICU_CHANGES

    setTimeInMillis(getNow(), success); // Call this again now that the vtable is set up properly.
}

IslamicCalendar::~IslamicCalendar()
{
}
//-------------------------------------------------------------------------
// Minimum / Maximum access functions
//-------------------------------------------------------------------------

// Note: Current IslamicCalendar implementation does not work
// well with negative years.

// TODO: In some cases the current ICU Islamic calendar implementation shows
// a month as having 31 days. Since date parsing now uses range checks based
// on the table below, we need to change the range for last day of month to
// include 31 as a workaround until the implementation is fixed.
static const int32_t LIMITS[UCAL_FIELD_COUNT][4] = {
    // Minimum  Greatest    Least  Maximum
    //           Minimum  Maximum
    {        0,        0,        0,        0}, // ERA
    {        1,        1,  5000000,  5000000}, // YEAR
    {        0,        0,       11,       11}, // MONTH
    {        1,        1,       50,       51}, // WEEK_OF_YEAR
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // WEEK_OF_MONTH
    {        1,        1,       29,       31}, // DAY_OF_MONTH - 31 to workaround for cal implementation bug, should be 30
    {        1,        1,      354,      355}, // DAY_OF_YEAR
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // DAY_OF_WEEK
    {       -1,       -1,        5,        5}, // DAY_OF_WEEK_IN_MONTH
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // AM_PM
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // HOUR
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // HOUR_OF_DAY
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // MINUTE
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // SECOND
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // MILLISECOND
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // ZONE_OFFSET
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // DST_OFFSET
    {        1,        1,  5000000,  5000000}, // YEAR_WOY
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // DOW_LOCAL
    {        1,        1,  5000000,  5000000}, // EXTENDED_YEAR
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // JULIAN_DAY
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // MILLISECONDS_IN_DAY
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // IS_LEAP_MONTH
    {        0,        0,       11,       11}, // ORDINAL_MONTH
};

/**
* @draft ICU 2.4
*/
int32_t IslamicCalendar::handleGetLimit(UCalendarDateFields field, ELimitType limitType) const {
    return LIMITS[field][limitType];
}

//-------------------------------------------------------------------------
// Assorted calculation utilities
//

namespace {

// we could compress this down more if we need to
static const int8_t umAlQuraYrStartEstimateFix[] = {
     0,  0, -1,  0, -1,  0,  0,  0,  0,  0, // 1300..
    -1,  0,  0,  0,  0,  0,  0,  0, -1,  0, // 1310..
     1,  0,  1,  1,  0,  0,  0,  0,  1,  0, // 1320..
     0,  0,  0,  0,  0,  0,  1,  0,  0,  0, // 1330..
     0,  0,  1,  0,  0, -1, -1,  0,  0,  0, // 1340..
     1,  0,  0, -1,  0,  0,  0,  1,  1,  0, // 1350..
     0,  0,  0,  0,  0,  0,  0, -1,  0,  0, // 1360..
     0,  1,  1,  0,  0, -1,  0,  1,  0,  1, // 1370..
     1,  0,  0, -1,  0,  1,  0,  0,  0, -1, // 1380..
     0,  1,  0,  1,  0,  0,  0, -1,  0,  0, // 1390..
     0,  0, -1, -1,  0, -1,  0,  1,  0,  0, // 1400..
     0, -1,  0,  0,  0,  1,  0,  0,  0,  0, // 1410..
     0,  1,  0,  0, -1, -1,  0,  0,  0,  1, // 1420..
     0,  0, -1, -1,  0, -1,  0,  0, -1, -1, // 1430..
     0, -1,  0, -1,  0,  0, -1, -1,  0,  0, // 1440..
     0,  0,  0,  0, -1,  0,  1,  0,  1,  1, // 1450..
     0,  0, -1,  0,  1,  0,  0,  0,  0,  0, // 1460..
     1,  0,  1,  0,  0,  0, -1,  0,  1,  0, // 1470..
     0, -1, -1,  0,  0,  0,  1,  0,  0,  0, // 1480..
     0,  0,  0,  0,  1,  0,  0,  0,  0,  0, // 1490..
     1,  0,  0, -1,  0,  0,  0,  1,  1,  0, // 1500..
     0, -1,  0,  1,  0,  1,  1,  0,  0,  0, // 1510..
     0,  1,  0,  0,  0, -1,  0,  0,  0,  1, // 1520..
     0,  0,  0, -1,  0,  0,  0,  0,  0, -1, // 1530..
     0, -1,  0,  1,  0,  0,  0, -1,  0,  1, // 1540..
     0,  1,  0,  0,  0,  0,  0,  1,  0,  0, // 1550..
    -1,  0,  0,  0,  0,  1,  0,  0,  0, -1, // 1560..
     0,  0,  0,  0, -1, -1,  0, -1,  0,  1, // 1570..
     0,  0, -1, -1,  0,  0,  1,  1,  0,  0, // 1580..
    -1,  0,  0,  0,  0,  1,  0,  0,  0,  0, // 1590..
     1 // 1600
};

/**
* Determine whether a year is a leap year in the Islamic civil calendar
*/
inline bool civilLeapYear(int32_t year) {
    return (14 + 11 * year) % 30 < 11;
}

#if APPLE_ICU_CHANGES
// rdar://100197751 (QFA: Islamic Lunar Calendar Improvements)
int32_t trueMonthStart(int32_t month, double longitude, double latitude, UErrorCode& status);
#else
int32_t trueMonthStart(int32_t month, UErrorCode& status);
#endif // APPLE_ICU_CHANGES

#if APPLE_ICU_CHANGES
// rdar://124719074 (Set default location for Islamic calendar based on the region)

struct RegionLocation {
  const char *region;
  double latitude;
  double longitude;
};


/* Location of a country is defined as a geographic mid-point
 * Source: https://developers.google.com/public-data/docs/canonical/countries_csv
 */
static const RegionLocation REGION_LOCATIONS[] = {
  { "AD",    42.5462,    1.6015 },
  { "AE",    23.4240,    53.8478 },
  { "AF",    33.939,    67.7099 },
  { "AG",    17.0608,    -61.7964 },
  { "AI",    18.2205,    -63.0686 },
  { "AL",    41.1533,    20.1683 },
  { "AM",    40.0690,    45.0381 },
  { "AN",    12.2260,    -69.0600 },
  { "AO",    -11.2026,    17.8738 },
  { "AQ",    -75.2509,    -0.0713 },
  { "AR",    -38.4160,    -63.6166 },
  { "AS",    -14.2709,    -170.1322 },
  { "AT",    47.5162,    14.5500 },
  { "AU",    -25.2743,    133.7751 },
  { "AW",    12.521,    -69.9683 },
  { "AZ",    40.1431,    47.5769 },
  { "BA",    43.9158,    17.6790 },
  { "BB",    13.1938,    -59.5431 },
  { "BD",    23.6849,    90.3563 },
  { "BE",    50.5038,    4.4699 },
  { "BF",    12.2383,    -1.5615 },
  { "BG",    42.7338,    25.485 },
  { "BH",    25.9304,    50.6377 },
  { "BI",    -3.3730,    29.9188 },
  { "BJ",    9.307,    2.3158 },
  { "BM",    32.3213,    -64.757 },
  { "BN",    4.5352,    114.7276 },
  { "BO",    -16.2901,    -63.5886 },
  { "BR",    -14.2350,    -51.925 },
  { "BS",    25.034,    -77.396 },
  { "BT",    27.5141,    90.4336 },
  { "BV",    -54.4231,    3.4131 },
  { "BW",    -22.3284,    24.6848 },
  { "BY",    53.7098,    27.9533 },
  { "BZ",    17.1898,    -88.497 },
  { "CA",    56.1303,    -106.3467 },
  { "CC",    -12.1641,    96.8709 },
  { "CD",    -4.0383,    21.7586 },
  { "CF",    6.6111,    20.9394 },
  { "CG",    -0.2280,    15.8276 },
  { "CH",    46.8181,    8.2275 },
  { "CI",    7.5399,    -5.547 },
  { "CK",    -21.2367,    -159.7776 },
  { "CL",    -35.6751,    -71.5429 },
  { "CM",    7.3697,    12.3547 },
  { "CN",    35.861,    104.1953 },
  { "CO",    4.5708,    -74.2973 },
  { "CR",    9.7489,    -83.7534 },
  { "CU",    21.5217,    -77.7811 },
  { "CV",    16.0020,    -24.0131 },
  { "CX",    -10.4475,    105.6904 },
  { "CY",    35.1264,    33.4298 },
  { "CZ",    49.8174,    15.4729 },
  { "DE",    51.1656,    10.4515 },
  { "DJ",    11.8251,    42.5902 },
  { "DK",    56.263,    9.5017 },
  { "DM",    15.4149,    -61.3709 },
  { "DO",    18.7356,    -70.1626 },
  { "DZ",    28.0338,    1.6596 },
  { "EC",    -1.8312,    -78.1834 },
  { "EE",    58.5952,    25.0136 },
  { "EG",    26.8205,    30.8024 },
  { "EH",    24.2155,    -12.8858 },
  { "ER",    15.1793,    39.7823 },
  { "ES",    40.4636,    -3.749 },
  { "ET",    9.1004,    40.4896 },
  { "FI",    61.924,    25.7481 },
  { "FJ",    -16.5781,    179.4144 },
  { "FK",    -51.7962,    -59.5236 },
  { "FM",    7.4255,    150.5508 },
  { "FO",    61.8926,    -6.9118 },
  { "FR",    46.2276,    2.2137 },
  { "GA",    -0.8036,    11.6094 },
  { "GB",    55.3780,    -3.4359 },
  { "GD",    12.2627,    -61.6041 },
  { "GE",    42.3154,    43.3568 },
  { "GF",    3.9338,    -53.1257 },
  { "GG",    49.4656,    -2.5852 },
  { "GH",    7.9465,    -1.0231 },
  { "GI",    36.1377,    -5.3453 },
  { "GL",    71.7069,    -42.6043 },
  { "GM",    13.4431,    -15.3101 },
  { "GN",    9.9455,    -9.6966 },
  { "GP",    16.9959,    -62.0676 },
  { "GQ",    1.6508,    10.2678 },
  { "GR",    39.0742,    21.8243 },
  { "GS",    -54.4295,    -36.5879 },
  { "GT",    15.7834,    -90.2307 },
  { "GU",    13.4443,    144.7937 },
  { "GW",    11.8037,    -15.1804 },
  { "GY",    4.8604,    -58.930 },
  { "GZ",    31.3546,    34.3088 },
  { "HK",    22.3964,    114.1094 },
  { "HM",    -53.081,    73.5041 },
  { "HN",    15.1999,    -86.2419 },
  { "HR",    45.1013,    15.2007 },
  { "HT",    18.9711,    -72.2852 },
  { "HU",    47.1624,    19.5033 },
  { "ID",    -0.7892,    113.9213 },
  { "IE",    53.412,    -8.243 },
  { "IL",    31.0460,    34.8516 },
  { "IM",    54.2361,    -4.5480 },
  { "IN",    20.5936,    78.962 },
  { "IO",    -6.3431,    71.8765 },
  { "IQ",    33.2231,    43.6792 },
  { "IR",    32.4279,    53.6880 },
  { "IS",    64.9630,    -19.0208 },
  { "IT",    41.871,    12.567 },
  { "JE",    49.2144,    -2.131 },
  { "JM",    18.1095,    -77.2975 },
  { "JO",    30.5851,    36.2384 },
  { "JP",    36.2048,    138.2529 },
  { "KE",    -0.0235,    37.9061 },
  { "KG",    41.204,    74.7660 },
  { "KH",    12.5656,    104.9909 },
  { "KI",    -3.3704,    -168.7340 },
  { "KM",    -11.8750,    43.8722 },
  { "KN",    17.3578,    -62.7829 },
  { "KP",    40.3398,    127.5100 },
  { "KR",    35.9077,    127.7669 },
  { "KW",    29.311,    47.4817 },
  { "KY",    19.5134,    -80.5669 },
  { "KZ",    48.0195,    66.9236 },
  { "LA",    19.856,    102.4954 },
  { "LB",    33.8547,    35.8622 },
  { "LC",    13.9094,    -60.9788 },
  { "LI",    47.1003,    9.5553 },
  { "LK",    7.8730,    80.7717 },
  { "LR",    6.4280,    -9.4294 },
  { "LS",    -29.6099,    28.2336 },
  { "LT",    55.1694,    23.8812 },
  { "LU",    49.8152,    6.1295 },
  { "LV",    56.8796,    24.6031 },
  { "LY",    26.3308,    17.2283 },
  { "MA",    31.7917,    -7.092 },
  { "MC",    43.7502,    7.4128 },
  { "MD",    47.4116,    28.3698 },
  { "ME",    42.7086,    19.374 },
  { "MG",    -18.7669,    46.8691 },
  { "MH",    7.1314,    171.1844 },
  { "MK",    41.6086,    21.7452 },
  { "ML",    17.5706,    -3.9961 },
  { "MM",    21.9139,    95.9562 },
  { "MN",    46.8624,    103.8466 },
  { "MO",    22.1987,    113.5438 },
  { "MP",    17.330,    145.384 },
  { "MQ",    14.6415,    -61.0241 },
  { "MR",    21.007,    -10.9408 },
  { "MS",    16.7424,    -62.1873 },
  { "MT",    35.9374,    14.3754 },
  { "MU",    -20.3484,    57.5521 },
  { "MV",    3.2027,    73.220 },
  { "MW",    -13.2543,    34.3015 },
  { "MX",    23.6345,    -102.5527 },
  { "MY",    4.2104,    101.9757 },
  { "MZ",    -18.6656,    35.5295 },
  { "NA",    -22.957,    18.490 },
  { "NC",    -20.9043,    165.6180 },
  { "NE",    17.6077,    8.0816 },
  { "NF",    -29.0408,    167.9547 },
  { "NG",    9.0819,    8.6752 },
  { "NI",    12.8654,    -85.2072 },
  { "NL",    52.1326,    5.2912 },
  { "NO",    60.4720,    8.4689 },
  { "NP",    28.3948,    84.1240 },
  { "NR",    -0.5227,    166.9315 },
  { "NU",    -19.0544,    -169.8672 },
  { "NZ",    -40.9005,    174.8859 },
  { "OM",    21.5125,    55.9232 },
  { "PA",    8.5379,    -80.7821 },
  { "PE",    -9.1899,    -75.0151 },
  { "PF",    -17.6797,    -149.4068 },
  { "PG",    -6.3149,    143.955 },
  { "PH",    12.8797,    121.7740 },
  { "PK",    30.3753,    69.3451 },
  { "PL",    51.9194,    19.1451 },
  { "PM",    46.9419,    -56.271 },
  { "PN",    -24.7036,    -127.4393 },
  { "PR",    18.2208,    -66.5901 },
  { "PS",    31.9521,    35.2331 },
  { "PT",    39.3998,    -8.2244 },
  { "PW",    7.514,    134.582 },
  { "PY",    -23.4425,    -58.4438 },
  { "QA",    25.3548,    51.1838 },
  { "RE",    -21.1151,    55.5363 },
  { "RO",    45.9431,    24.966 },
  { "RS",    44.0165,    21.0058 },
  { "RU",    61.524,    105.3187 },
  { "RW",    -1.9402,    29.8738 },
  { "SA",    23.8859,    45.0791 },
  { "SB",    -9.645,    160.1561 },
  { "SC",    -4.6795,    55.4919 },
  { "SD",    12.8628,    30.2176 },
  { "SE",    60.1281,    18.6435 },
  { "SG",    1.3520,    103.8198 },
  { "SH",    -24.1434,    -10.0306 },
  { "SI",    46.1512,    14.9954 },
  { "SJ",    77.5536,    23.6702 },
  { "SK",    48.6690,    19.6990 },
  { "SL",    8.4605,    -11.7798 },
  { "SM",    43.942,    12.4577 },
  { "SN",    14.4974,    -14.4523 },
  { "SO",    5.1521,    46.1996 },
  { "SR",    3.9193,    -56.0277 },
  { "ST",    0.186,    6.6130 },
  { "SV",    13.7941,    -88.896 },
  { "SY",    34.8020,    38.9968 },
  { "SZ",    -26.5225,    31.4658 },
  { "TC",    21.6940,    -71.7979 },
  { "TD",    15.4541,    18.7322 },
  { "TF",    -49.2803,    69.3485 },
  { "TG",    8.6195,    0.8247 },
  { "TH",    15.8700,    100.9925 },
  { "TJ",    38.8610,    71.2760 },
  { "TK",    -8.9673,    -171.8558 },
  { "TL",    -8.8742,    125.7275 },
  { "TM",    38.9697,    59.5562 },
  { "TN",    33.8869,    9.5374 },
  { "TO",    -21.1789,    -175.1982 },
  { "TR",    38.9637,    35.2433 },
  { "TT",    10.6918,    -61.2225 },
  { "TV",    -7.1095,    177.649 },
  { "TW",    23.697,    120.9605 },
  { "TZ",    -6.3690,    34.8888 },
  { "UA",    48.3794,    31.165 },
  { "UG",    1.3733,    32.2902 },
  { "US",    37.090,    -95.7128 },
  { "UY",    -32.5227,    -55.7658 },
  { "UZ",    41.3774,    64.5852 },
  { "VA",    41.9029,    12.4533 },
  { "VC",    12.9843,    -61.2872 },
  { "VE",    6.423,    -66.589 },
  { "VG",    18.4206,    -64.6399 },
  { "VI",    18.3357,    -64.8963 },
  { "VN",    14.0583,    108.2771 },
  { "VU",    -15.3767,    166.9591 },
  { "WF",    -13.7687,    -177.1560 },
  { "WS",    -13.7590,    -172.1046 },
  { "XK",    42.6026,    20.9029 },
  { "YE",    15.5527,    48.5163 },
  { "YT",    -12.8202,    45.1662 },
  { "ZA",    -30.5594,    22.9375 },
  { "ZM",    -13.1338,    27.8493 },
  { "ZW",    -19.0154,    29.1548 },
};

int getLocationId(const char* region, int start, int end) {
    if (start > end || region == nullptr) {
        return -1;
    }
    int mid = start + (end - start) / 2;
    int pos = strcmp(region, REGION_LOCATIONS[mid].region);
    if (pos == 0) {
        return mid;
    }
    else if (pos < 1) {
        return getLocationId(region, start, mid - 1);
    }
    else {
        return getLocationId(region, mid + 1, end);
    }
}
#endif // APPLE_ICU_CHANGES

} // namespace

/**
* Return the day # on which the given year starts.  Days are counted
* from the Hijri epoch, origin 0.
*/
int64_t IslamicCalendar::yearStart(int32_t year, UErrorCode& status) const {
#if APPLE_ICU_CHANGES
// rdar://100197751 (QFA: Islamic Lunar Calendar Improvements)
    return trueMonthStart(12*(year-1), fLongitude, fLatitude, status);
#else
    return trueMonthStart(12*(year-1), status);
#endif // APPLE_ICU_CHANGES
}

/**
* Return the day # on which the given month starts.  Days are counted
* from the Hijri epoch, origin 0.
*
* @param year  The hijri year
* @param month The hijri month, 0-based (assumed to be in range 0..11)
*/
int64_t IslamicCalendar::monthStart(int32_t year, int32_t month, UErrorCode& status) const {
    if (U_FAILURE(status)) {
        return 0;
    }
    int32_t temp;
    if (uprv_add32_overflow(year, -1, &temp) ||
        uprv_mul32_overflow(temp, 12, &temp) ||
        uprv_add32_overflow(temp, month, &month)) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

#if APPLE_ICU_CHANGES
// rdar://100197751 (QFA: Islamic Lunar Calendar Improvements)
    return trueMonthStart(month, fLongitude, fLatitude, status);
#else
    return trueMonthStart(month, status);
#endif // APPLE_ICU_CHANGES
}

namespace {
/**
 * Return the "age" of the moon at the given time; this is the difference
 * in ecliptic latitude between the moon and the sun.  This method simply
 * calls CalendarAstronomer.moonAge, converts to degrees,
 * and adjusts the resultto be in the range [-180, 180].
 *
 * @param time  The time at which the moon's age is desired,
 *             in millis since 1/1/1970.
 */
#if APPLE_ICU_CHANGES
// rdar://100197751 (QFA: Islamic Lunar Calendar Improvements)
double moonAge(UDate time, double longitude, double latitude);
#else
double moonAge(UDate time);
#endif // APPLE_ICU_CHANGES

/**
* Find the day number on which a particular month of the true/lunar
* Islamic calendar starts.
*
* @param month The month in question, origin 0 from the Hijri epoch
*
* @return The day number on which the given month starts.
*/
#if APPLE_ICU_CHANGES
// rdar://100197751 (QFA: Islamic Lunar Calendar Improvements)
int32_t trueMonthStart(int32_t month, double longitude, double latitude, UErrorCode& status) {
#else
int32_t trueMonthStart(int32_t month, UErrorCode& status) {
#endif // APPLE_ICU_CHANGES
    if (U_FAILURE(status)) {
        return 0;
    }
    ucln_i18n_registerCleanup(UCLN_I18N_ISLAMIC_CALENDAR, calendar_islamic_cleanup);
    int64_t start = CalendarCache::get(&gMonthCache, month, status);

    if (U_SUCCESS(status) && start==0) {
        // Make a guess at when the month started, using the average length
        UDate origin = HIJRA_MILLIS 
            + uprv_floor(month * CalendarAstronomer::SYNODIC_MONTH) * kOneDay;

        // moonAge will fail due to memory allocation error
#if APPLE_ICU_CHANGES
// rdar://100197751 (QFA: Islamic Lunar Calendar Improvements)
        double age = moonAge(origin, longitude, latitude);
#else
        double age = moonAge(origin);
#endif // APPLE_ICU_CHANGES

        if (age >= 0) {
            // The month has already started
            do {
                origin -= kOneDay;
#if APPLE_ICU_CHANGES
// rdar://100197751 (QFA: Islamic Lunar Calendar Improvements)
                age = moonAge(origin, longitude, latitude);
#else
                age = moonAge(origin);
#endif // APPLE_ICU_CHANGES
            } while (age >= 0);
        }
        else {
            // Preceding month has not ended yet.
            do {
                origin += kOneDay;
#if APPLE_ICU_CHANGES
// rdar://100197751 (QFA: Islamic Lunar Calendar Improvements)
                age = moonAge(origin, longitude, latitude);
#else
                age = moonAge(origin);
#endif // APPLE_ICU_CHANGES
            } while (age < 0);
        }
        start = ClockMath::floorDivideInt64(
            static_cast<int64_t>(static_cast<int64_t>(origin) - HIJRA_MILLIS), static_cast<int64_t>(kOneDay)) + 1;
        CalendarCache::put(&gMonthCache, month, start, status);
    }
    if(U_FAILURE(status)) {
        start = 0;
    }
    return start;
}

#if APPLE_ICU_CHANGES
// rdar://100197751 (QFA: Islamic Lunar Calendar Improvements)
double moonAge(UDate time, double longitude, double latitude) {
    // Convert to degrees and normalize...
    double age = CalendarAstronomer(time, longitude, latitude).getMoonAge() * 180 / CalendarAstronomer::PI;
#else
    double moonAge(UDate time) {
        // Convert to degrees and normalize...
        double age = CalendarAstronomer(time).getMoonAge() * 180 / CalendarAstronomer::PI;
#endif // APPLE_ICU_CHANGES
    if (age > 180) {
        age = age - 360;
    }

    return age;
}

}  // namespace

#if APPLE_ICU_CHANGES
// rdar://100197751 (QFA: Islamic Lunar Calendar Improvements)
/**
 * Set the geographic location in CalendarAstronomer object associated with IslamicCalendar
 *
 * @param #latitude
 * @param #longitude
 * @internal
 */
void IslamicCalendar::setLocation(double latitude, double longitude) {
    fLatitude = latitude;
    fLongitude = longitude;
}

/**
  * Set the location of this <code>CalendarAstronomer</code> object based on region
  * All astronomical calculations are performed relative to this location.
  *
  * @param #region
  * @internal
  */
void IslamicCalendar::setLocation(const char* region) {
    int id = getLocationId(region, 0, sizeof(REGION_LOCATIONS) / sizeof(RegionLocation));
    if (id == -1)  {
        setLocation(0.0, 0.0);
        return;
    }
    setLocation(REGION_LOCATIONS[id].latitude, REGION_LOCATIONS[id].longitude);
}

/**
 * Returns the latitude of the  location of this object
 *
 * @internal
 */
double IslamicCalendar::getLocationLatitude(){
    return fLatitude;
}


/**
 * Returns the longitude of the  location of this object
 *
 * @internal
 */
double IslamicCalendar::getLocationLongitude(){
    return fLongitude;
}
#endif // APPLE_ICU_CHANGES

//----------------------------------------------------------------------
// Calendar framework
//----------------------------------------------------------------------

/**
* Return the length (in days) of the given month.
*
* @param year  The hijri year
* @param year  The hijri month, 0-based
* @draft ICU 2.4
*/
int32_t IslamicCalendar::handleGetMonthLength(int32_t extendedYear, int32_t month,
                                              UErrorCode& status) const {
    month = 12*(extendedYear-1) + month;
#if APPLE_ICU_CHANGES
// rdar://100197751 (QFA: Islamic Lunar Calendar Improvements)
    int32_t len = trueMonthStart(month+1, fLongitude, fLatitude, status) - trueMonthStart(month, fLongitude, fLatitude, status) ;
#else
    int32_t len = trueMonthStart(month+1, status) - trueMonthStart(month, status) ;
#endif // APPLE_ICU_CHANGES
    if (U_FAILURE(status)) {
        return 0;
    }
    return len;
}

namespace {

#if APPLE_ICU_CHANGES
    // rdar://100197751 (QFA: Islamic Lunar Calendar Improvements)
int32_t yearLength(int32_t extendedYear, double longitude, double latitude, UErrorCode& status) {
    int32_t month = 12*(extendedYear-1);
    int32_t length = trueMonthStart(month + 12, longitude, latitude, status) - trueMonthStart(month, longitude, latitude, status);
#else
    int32_t yearLength(int32_t extendedYear, UErrorCode& status) {
        int32_t month = 12*(extendedYear-1);
    int32_t length = trueMonthStart(month + 12, status) - trueMonthStart(month, status);
#endif // APPLE_ICU_CHANGES
    if (U_FAILURE(status)) {
        return 0;
    }
    return length;
}

} // namepsace
/**
* Return the number of days in the given Islamic year
* @draft ICU 2.4
*/
int32_t IslamicCalendar::handleGetYearLength(int32_t extendedYear) const {
    UErrorCode status = U_ZERO_ERROR;
#if APPLE_ICU_CHANGES
// rdar://100197751 (QFA: Islamic Lunar Calendar Improvements)
    int32_t length = yearLength(extendedYear, fLongitude, fLatitude, status);
#else
    int32_t length = yearLength(extendedYear, status);
#endif // APPLE_ICU_CHANGES
   if (U_FAILURE(status)) {
        // fallback to normal Islamic calendar length 355 day a year if we
        // encounter error and cannot propagate.
        return 355;
    }
    return length;
}

//-------------------------------------------------------------------------
// Functions for converting from field values to milliseconds....
//-------------------------------------------------------------------------

// Return JD of start of given month/year
// Calendar says:
// Get the Julian day of the day BEFORE the start of this year.
// If useMonth is true, get the day before the start of the month.
// Hence the -1
/**
* @draft ICU 2.4
*/
int64_t IslamicCalendar::handleComputeMonthStart(int32_t eyear, int32_t month,
                                                 UBool /* useMonth */,
                                                 UErrorCode& status) const {
    if (U_FAILURE(status)) {
        return 0;
    }
    // This may be called by Calendar::handleComputeJulianDay with months out of the range
    // 0..11. Need to handle that here since monthStart requires months in the range 0.11.
    if (month > 11) {
        if (uprv_add32_overflow(eyear, (month / 12), &eyear)) {
            status = U_ILLEGAL_ARGUMENT_ERROR;
            return 0;
        }
        month %= 12;
    } else if (month < 0) {
        month++;
        if (uprv_add32_overflow(eyear, (month / 12) - 1, &eyear)) {
            status = U_ILLEGAL_ARGUMENT_ERROR;
            return 0;
        }
        month = (month % 12) + 11;
    }
    return monthStart(eyear, month, status) + getEpoc() - 1;
}

//-------------------------------------------------------------------------
// Functions for converting from milliseconds to field values
//-------------------------------------------------------------------------

/**
* @draft ICU 2.4
*/
int32_t IslamicCalendar::handleGetExtendedYear(UErrorCode& /* status */) {
    if (newerField(UCAL_EXTENDED_YEAR, UCAL_YEAR) == UCAL_EXTENDED_YEAR) {
        return internalGet(UCAL_EXTENDED_YEAR, 1); // Default to year 1
    }
    return internalGet(UCAL_YEAR, 1); // Default to year 1
}

/**
* Override Calendar to compute several fields specific to the Islamic
* calendar system.  These are:
*
* <ul><li>ERA
* <li>YEAR
* <li>MONTH
* <li>DAY_OF_MONTH
* <li>DAY_OF_YEAR
* <li>EXTENDED_YEAR</ul>
* 
* The DAY_OF_WEEK and DOW_LOCAL fields are already set when this
* method is called. The getGregorianXxx() methods return Gregorian
* calendar equivalents for the given Julian day.
* @draft ICU 2.4
*/
void IslamicCalendar::handleComputeFields(int32_t julianDay, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    int32_t days = julianDay - getEpoc();

    // Guess at the number of elapsed full months since the epoch
    int32_t month = static_cast<int32_t>(uprv_floor(static_cast<double>(days) / CalendarAstronomer::SYNODIC_MONTH));

    int32_t startDate = static_cast<int32_t>(uprv_floor(month * CalendarAstronomer::SYNODIC_MONTH));

#if APPLE_ICU_CHANGES
// rdar://100197751 (QFA: Islamic Lunar Calendar Improvements)
    double age = moonAge(internalGetTime(), fLongitude, fLatitude);
#else
    double age = moonAge(internalGetTime());
#endif // APPLE_ICU_CHANGES
    if ( days - startDate >= 25 && age > 0) {
        // If we're near the end of the month, assume next month and search backwards
        month++;
    }

    // Find out the last time that the new moon was actually visible at this longitude
    // This returns midnight the night that the moon was visible at sunset.
#if APPLE_ICU_CHANGES
// rdar://100197751 (QFA: Islamic Lunar Calendar Improvements)
    while ((startDate = trueMonthStart(month, fLongitude, fLatitude, status)) > days) {
#else
    while ((startDate = trueMonthStart(month, status)) > days) {
#endif // APPLE_ICU_CHANGES
        if (U_FAILURE(status)) {
            return;
        }
        // If it was after the date in question, back up a month and try again
        month--;
    }
    if (U_FAILURE(status)) {
        return;
    }

    int32_t year = month >=  0 ? ((month / 12) + 1) : ((month + 1 ) / 12);
    month = ((month % 12) + 12 ) % 12;
    int64_t dayOfMonth = (days - monthStart(year, month, status)) + 1;
    if (U_FAILURE(status)) {
        return;
    }
    if (dayOfMonth > INT32_MAX || dayOfMonth < INT32_MIN) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    // Now figure out the day of the year.
    int64_t dayOfYear = (days - monthStart(year, 0, status)) + 1;
    if (U_FAILURE(status)) {
        return;
    }
    if (dayOfYear > INT32_MAX || dayOfYear < INT32_MIN) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    internalSet(UCAL_ERA, 0);
    internalSet(UCAL_YEAR, year);
    internalSet(UCAL_EXTENDED_YEAR, year);
    internalSet(UCAL_MONTH, month);
    internalSet(UCAL_ORDINAL_MONTH, month);
    internalSet(UCAL_DAY_OF_MONTH, dayOfMonth);
    internalSet(UCAL_DAY_OF_YEAR, dayOfYear);
}

int32_t IslamicCalendar::getEpoc() const {
    return CIVIL_EPOC;
}

static int32_t gregoYearFromIslamicStart(int32_t year) {
    // ad hoc conversion, improve under #10752
    // rough est for now, ok for grego 1846-2138,
    // otherwise occasionally wrong (for 3% of years)
    int cycle, offset, shift = 0;
    if (year >= 1397) {
        cycle = (year - 1397) / 67;
        offset = (year - 1397) % 67;
        shift = 2*cycle + ((offset >= 33)? 1: 0);
    } else {
        cycle = (year - 1396) / 67 - 1;
        offset = -(year - 1396) % 67;
        shift = 2*cycle + ((offset <= 33)? 1: 0);
    }
    return year + 579 - shift;
}

int32_t IslamicCalendar::getRelatedYear(UErrorCode &status) const
{
    int32_t year = get(UCAL_EXTENDED_YEAR, status);
    if (U_FAILURE(status)) {
        return 0;
    }
    return gregoYearFromIslamicStart(year);
}

void IslamicCalendar::setRelatedYear(int32_t year)
{
    // ad hoc conversion, improve under #10752
    // rough est for now, ok for grego 1846-2138,
    // otherwise occasionally wrong (for 3% of years)
    int cycle, offset, shift = 0;
    if (year >= 1977) {
        cycle = (year - 1977) / 65;
        offset = (year - 1977) % 65;
        shift = 2*cycle + ((offset >= 32)? 1: 0);
    } else {
        cycle = (year - 1976) / 65 - 1;
        offset = -(year - 1976) % 65;
        shift = 2*cycle + ((offset <= 32)? 1: 0);
    }
    year = year - 579 + shift;
    set(UCAL_EXTENDED_YEAR, year);
}

IMPL_SYSTEM_DEFAULT_CENTURY(IslamicCalendar, "@calendar=islamic-civil")

bool
IslamicCalendar::inTemporalLeapYear(UErrorCode &status) const
{
    int32_t days = getActualMaximum(UCAL_DAY_OF_YEAR, status);
    if (U_FAILURE(status)) {
        return false;
    }
    return days == 355;
}

/*****************************************************************************
 * IslamicCivilCalendar
 *****************************************************************************/
IslamicCivilCalendar::IslamicCivilCalendar(const Locale& aLocale, UErrorCode& success)
    : IslamicCalendar(aLocale, success)
{
}

IslamicCivilCalendar::~IslamicCivilCalendar()
{
}

const char *IslamicCivilCalendar::getType() const {
    return "islamic-civil";
}

IslamicCivilCalendar* IslamicCivilCalendar::clone() const {
    return new IslamicCivilCalendar(*this);
}

/**
* Return the day # on which the given year starts.  Days are counted
* from the Hijri epoch, origin 0.
*/
int64_t IslamicCivilCalendar::yearStart(int32_t year, UErrorCode& /* status */) const {
    return 354LL * (year-1LL) + ClockMath::floorDivideInt64(3 + 11LL * year, 30LL);
}

/**
* Return the day # on which the given month starts.  Days are counted
* from the Hijri epoch, origin 0.
*
* @param year  The hijri year
* @param month The hijri month, 0-based (assumed to be in range 0..11)
*/
int64_t IslamicCivilCalendar::monthStart(int32_t year, int32_t month, UErrorCode& /*status*/) const {
    // This does not handle months out of the range 0..11
    return static_cast<int64_t>(
        uprv_ceil(29.5*month) + 354LL*(year-1LL) +
        ClockMath::floorDivideInt64(
             11LL*static_cast<int64_t>(year) + 3LL, 30LL));
}

/**
* Return the length (in days) of the given month.
*
* @param year  The hijri year
* @param year  The hijri month, 0-based
* @draft ICU 2.4
*/
int32_t IslamicCivilCalendar::handleGetMonthLength(int32_t extendedYear, int32_t month,
                                                   UErrorCode& /* status */) const {
    int32_t length = 29 + (month+1) % 2;
    if (month == DHU_AL_HIJJAH && civilLeapYear(extendedYear)) {
        length++;
    }
    return length;
}

/**
* Return the number of days in the given Islamic year
* @draft ICU 2.4
*/
int32_t IslamicCivilCalendar::handleGetYearLength(int32_t extendedYear) const {
    return 354 + (civilLeapYear(extendedYear) ? 1 : 0);
}

/**
* Override Calendar to compute several fields specific to the Islamic
* calendar system.  These are:
*
* <ul><li>ERA
* <li>YEAR
* <li>MONTH
* <li>DAY_OF_MONTH
* <li>DAY_OF_YEAR
* <li>EXTENDED_YEAR</ul>
* 
* The DAY_OF_WEEK and DOW_LOCAL fields are already set when this
* method is called. The getGregorianXxx() methods return Gregorian
* calendar equivalents for the given Julian day.
* @draft ICU 2.4
*/
void IslamicCivilCalendar::handleComputeFields(int32_t julianDay, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    int32_t days = julianDay - getEpoc();

    // Use the civil calendar approximation, which is just arithmetic
    int64_t year  =
        ClockMath::floorDivideInt64(30LL * days + 10646LL, 10631LL);
    int32_t month = static_cast<int32_t>(
        uprv_ceil((days - 29 - yearStart(year, status)) / 29.5 ));
    if (U_FAILURE(status)) {
        return;
    }
    month = month<11?month:11;

    int64_t dayOfMonth = (days - monthStart(year, month, status)) + 1;
    if (U_FAILURE(status)) {
        return;
    }
    if (dayOfMonth > INT32_MAX || dayOfMonth < INT32_MIN) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    // Now figure out the day of the year.
    int64_t dayOfYear = (days - monthStart(year, 0, status)) + 1;
    if (U_FAILURE(status)) {
        return;
    }
    if (dayOfYear > INT32_MAX || dayOfYear < INT32_MIN) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    internalSet(UCAL_ERA, 0);
    internalSet(UCAL_YEAR, year);
    internalSet(UCAL_EXTENDED_YEAR, year);
    internalSet(UCAL_MONTH, month);
    internalSet(UCAL_ORDINAL_MONTH, month);
    internalSet(UCAL_DAY_OF_MONTH, dayOfMonth);
    internalSet(UCAL_DAY_OF_YEAR, dayOfYear);
}
/*****************************************************************************
 * IslamicTBLACalendar
 *****************************************************************************/
IslamicTBLACalendar::IslamicTBLACalendar(const Locale& aLocale, UErrorCode& success)
    : IslamicCivilCalendar(aLocale, success)
{
}

IslamicTBLACalendar::~IslamicTBLACalendar()
{
}

const char *IslamicTBLACalendar::getType() const {
    return "islamic-tbla";
}

IslamicTBLACalendar* IslamicTBLACalendar::clone() const {
    return new IslamicTBLACalendar(*this);
}

int32_t IslamicTBLACalendar::getEpoc() const {
    return ASTRONOMICAL_EPOC;
}

/*****************************************************************************
 * IslamicUmalquraCalendar
 *****************************************************************************/
IslamicUmalquraCalendar::IslamicUmalquraCalendar(const Locale& aLocale, UErrorCode& success)
    : IslamicCivilCalendar(aLocale, success)
{
}

IslamicUmalquraCalendar::~IslamicUmalquraCalendar()
{
}

const char *IslamicUmalquraCalendar::getType() const {
    return "islamic-umalqura";
}

IslamicUmalquraCalendar* IslamicUmalquraCalendar::clone() const {
    return new IslamicUmalquraCalendar(*this);
}

/**
* Return the day # on which the given year starts.  Days are counted
* from the Hijri epoch, origin 0.
*/
int64_t IslamicUmalquraCalendar::yearStart(int32_t year, UErrorCode& status) const {
    if (year < UMALQURA_YEAR_START || year > UMALQURA_YEAR_END) {
        return IslamicCivilCalendar::yearStart(year, status);
    }
    year -= UMALQURA_YEAR_START;
    // rounded least-squares fit of the dates previously calculated from UMALQURA_MONTHLENGTH iteration
    int64_t yrStartLinearEstimate = static_cast<int64_t>(
        (354.36720 * static_cast<double>(year)) + 460322.05 + 0.5);
    // need a slight correction to some
    return yrStartLinearEstimate + umAlQuraYrStartEstimateFix[year];
}

/**
* Return the day # on which the given month starts.  Days are counted
* from the Hijri epoch, origin 0.
*
* @param year  The hijri year
* @param month The hijri month, 0-based (assumed to be in range 0..11)
*/
int64_t IslamicUmalquraCalendar::monthStart(int32_t year, int32_t month, UErrorCode& status) const {
    int64_t ms = yearStart(year, status);
    if (U_FAILURE(status)) {
        return 0;
    }
    for(int i=0; i< month; i++){
        ms+= handleGetMonthLength(year, i, status);
        if (U_FAILURE(status)) {
            return 0;
        }
    }
    return ms;
}

/**
* Return the length (in days) of the given month.
*
* @param year  The hijri year
* @param year  The hijri month, 0-based
*/
int32_t IslamicUmalquraCalendar::handleGetMonthLength(int32_t extendedYear, int32_t month,
                                                      UErrorCode& status) const {
    if (extendedYear<UMALQURA_YEAR_START || extendedYear>UMALQURA_YEAR_END) {
        return IslamicCivilCalendar::handleGetMonthLength(extendedYear, month, status);
    }
    int32_t length = 29;
    int32_t mask = static_cast<int32_t>(0x01 << (11 - month)); // set mask for bit corresponding to month
    int32_t index = extendedYear - UMALQURA_YEAR_START;
    if ((UMALQURA_MONTHLENGTH[index] & mask) != 0) {
        length++;
    }
    return length;
}

int32_t IslamicUmalquraCalendar::yearLength(int32_t extendedYear, UErrorCode& status) const {
    if (extendedYear<UMALQURA_YEAR_START || extendedYear>UMALQURA_YEAR_END) {
        return IslamicCivilCalendar::handleGetYearLength(extendedYear);
    }
    int length = 0;
    for(int i=0; i<12; i++) {
        length += handleGetMonthLength(extendedYear, i, status);
        if (U_FAILURE(status)) {
            return 0;
        }
    }
    return length;
}

/**
* Return the number of days in the given Islamic year
* @draft ICU 2.4
*/
int32_t IslamicUmalquraCalendar::handleGetYearLength(int32_t extendedYear) const {
    UErrorCode status = U_ZERO_ERROR;
    int32_t length = yearLength(extendedYear, status);
    if (U_FAILURE(status)) {
        // fallback to normal Islamic calendar length 355 day a year if we
        // encounter error and cannot propagate.
        return 355;
    }
    return length;
}

/**
* Override Calendar to compute several fields specific to the Islamic
* calendar system.  These are:
*
* <ul><li>ERA
* <li>YEAR
* <li>MONTH
* <li>DAY_OF_MONTH
* <li>DAY_OF_YEAR
* <li>EXTENDED_YEAR</ul>
* 
* The DAY_OF_WEEK and DOW_LOCAL fields are already set when this
* method is called. The getGregorianXxx() methods return Gregorian
* calendar equivalents for the given Julian day.
* @draft ICU 2.4
*/
void IslamicUmalquraCalendar::handleComputeFields(int32_t julianDay, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    int64_t year;
    int32_t month;
    int32_t days = julianDay - getEpoc();

    static int64_t kUmalquraStart = yearStart(UMALQURA_YEAR_START, status);
    if (U_FAILURE(status)) {
        return;
    }
    if (days < kUmalquraStart) {
        IslamicCivilCalendar::handleComputeFields(julianDay, status);
        return;
    }
    // Estimate a value y which is closer to but not greater than the year.
    // It is the inverse function of the logic inside
    // IslamicUmalquraCalendar::yearStart().
    year = ((static_cast<double>(days) - (460322.05 + 0.5)) / 354.36720) + UMALQURA_YEAR_START - 1;
    month = 0;
    int32_t d = 1;
    // need a slight correction to some
    while (d > 0) {
        d = days - yearStart(++year, status) + 1;
        int32_t length = yearLength(year, status);
        if (U_FAILURE(status)) {
            return;
        }
        if (d == length) {
            month = 11;
            break;
        }
        if (d < length){
            int32_t monthLen = handleGetMonthLength(year, month, status);
            for (month = 0;
                 d > monthLen;
                 monthLen = handleGetMonthLength(year, ++month, status)) {
                if (U_FAILURE(status)) {
                    return;
                }
                d -= monthLen;
            }
            break;
        }
    }

    int32_t dayOfMonth = monthStart(year, month, status);
    int32_t dayOfYear = monthStart(year, 0, status);
    if (U_FAILURE(status)) {
        return;
    }
    if (uprv_mul32_overflow(dayOfMonth, -1, &dayOfMonth) ||
        uprv_add32_overflow(dayOfMonth, days, &dayOfMonth) ||
        uprv_add32_overflow(dayOfMonth, 1, &dayOfMonth) ||
        // Now figure out the day of the year.
        uprv_mul32_overflow(dayOfYear, -1, &dayOfYear) ||
        uprv_add32_overflow(dayOfYear, days, &dayOfYear) ||
        uprv_add32_overflow(dayOfYear, 1, &dayOfYear)) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    internalSet(UCAL_ERA, 0);
    internalSet(UCAL_YEAR, year);
    internalSet(UCAL_EXTENDED_YEAR, year);
    internalSet(UCAL_MONTH, month);
    internalSet(UCAL_ORDINAL_MONTH, month);
    internalSet(UCAL_DAY_OF_MONTH, dayOfMonth);
    internalSet(UCAL_DAY_OF_YEAR, dayOfYear);
}
/*****************************************************************************
 * IslamicRGSACalendar
 *****************************************************************************/
IslamicRGSACalendar::IslamicRGSACalendar(const Locale& aLocale, UErrorCode& success)
    : IslamicCalendar(aLocale, success)
{
}

IslamicRGSACalendar::~IslamicRGSACalendar()
{
}

const char *IslamicRGSACalendar::getType() const {
    return "islamic-rgsa";
}

IslamicRGSACalendar* IslamicRGSACalendar::clone() const {
    return new IslamicRGSACalendar(*this);
}

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(IslamicCalendar)
UOBJECT_DEFINE_RTTI_IMPLEMENTATION(IslamicCivilCalendar)
UOBJECT_DEFINE_RTTI_IMPLEMENTATION(IslamicUmalquraCalendar)
UOBJECT_DEFINE_RTTI_IMPLEMENTATION(IslamicTBLACalendar)
UOBJECT_DEFINE_RTTI_IMPLEMENTATION(IslamicRGSACalendar)

U_NAMESPACE_END

#endif

