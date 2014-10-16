/*
******************************************************************************
* Copyright (C) 2003-2013, International Business Machines Corporation
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
static UMutex astroLock = U_MUTEX_INITIALIZER;  // pod bay door lock
static icu::CalendarCache *gMonthCache = NULL;
static icu::CalendarAstronomer *gIslamicCalendarAstro = NULL;

U_CDECL_BEGIN
static UBool calendar_islamic_cleanup(void) {
    if (gMonthCache) {
        delete gMonthCache;
        gMonthCache = NULL;
    }
    if (gIslamicCalendarAstro) {
        delete gIslamicCalendarAstro;
        gIslamicCalendarAstro = NULL;
    }
    return TRUE;
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


static const int32_t UMALQURA_YEAR_START = 1318;
static const int32_t UMALQURA_YEAR_END = 1480;

static const int UMALQURA_MONTHLENGTH[] = {
    //* 1318 -1322 */ "1011 0110 1010", "1001 0111 0101", "0100 1010 1110", "1010 0100 1111", "0101 0010 1011",
                            0x0B6A,           0x0975,           0x04AE,           0x0A4F,           0x052B,
    //* 1323 -1327 */ "0110 1001 0101", "0110 1100 1010", "1010 1101 0101", "0010 1101 0110", "1001 0101 1011",
                            0x0695,           0x06CA,           0x0AD5,           0x02D6,           0x095B,
    //* 1328 -1332 */ "1001 0010 1101", "1100 1001 0101", "1101 0100 1010", "1101 1001 0101", "0110 1101 0010",
                            0x092D,           0x0C95,           0x0D4A,           0x0D95,           0x025B,
    //* 1333 -1337 */ "1010 1101 0101", "0101 0101 1010", "1010 1010 1011", "0101 0010 1011", "0110 1010 0101",
                            0x0AD5,           0x055A,           0x0AAB,           0x052B,           0x06A5,
    //* 1338 -1342 */ "0111 0101 0010", "1011 1010 1001", "0011 0111 0100", "1010 1011 0110", "0101 0101 0110",
                            0x0752,           0x0BA9,           0x0374,           0x0AB6,           0x0556,
    //* 1343 -1347 */ "1010 1010 1010", "1101 0101 0010", "1011 1010 0100", "1011 1101 0010", "1010 1110 1010",
                            0x0AAA,           0x0D52,           0x0BA4,           0x0BD2,           0x0AEA,
    //* 1348 -1352 */ "0010 1101 1100", "1001 0110 1101", "1001 0010 1110", "1010 1010 0110", "1101 0101 0100",
                            0x02DC,           0x096D,           0x092E,           0x0AA6,           0x0D54,
    //* 1353 -1357 */ "1011 1010 0101", "0101 1011 0100", "1001 1011 0110", "1001 0011 0111", "0100 1001 1011",
                            0x0BA5,           0x05B4,           0x09B6,           0x0937,           0x049B,
    //* 1358 -1362 */ "1010 0100 1011", "1011 0010 0101", "1011 0101 0100", "1011 0110 1010", "0101 0110 1010",
                            0x0A4B,           0x0B25,           0x0B54,           0x0B6A,           0x056A,
    //* 1363 -1367 */ "1010 1010 1011", "1010 0101 0101", "1101 0010 0101", "1110 1001 0010", "1110 1100 1001",
                            0x0AAB,           0x0A55,           0x0D25,           0x0E92,           0x0EC9,
    //* 1368 -1372 */ "0110 1101 0100", "1010 1110 1010", "0101 0110 1011", "0100 1010 1011", "1001 0100 1011",
                            0x06D4,           0x0ADA,           0x056B,           0x04AB,           0x094B,
    //* 1373 -1377 */ "1011 0100 1001", "1011 1010 0100", "1011 1011 0010", "0101 1011 0101", "0010 1011 1010",
                            0x0B49,           0x0BA4,           0x0BB2,           0x05B5,           0x02BA,
    //* 1378 -1382 */ "1001 0101 1011", "0100 1010 1011", "0101 0101 0101", "0110 1011 0010", "0101 1011 0100",
                            0x095B,           0x04AB,           0x0555,           0x06B2,           0x05B4,
    //* 1383 -1387 */ "1001 1101 1010", "1001 0110 1110", "0100 1010 1110", "1010 0101 0110", "1101 0010 1010",
                            0x09DA,           0x096E,           0x04AE,           0x0A56,           0x0D2A,
    //* 1388 -1392 */ "1101 0101 0100", "1101 1011 0010", "1010 1011 0101", "0010 1101 1010", "1001 0101 1011",
                            0x0D54,           0x0DB2,           0x0AB5,           0x02DA,           0x095B,
    //* 1393 -1397 */ "1001 0010 1011", "1010 1001 0101", "1011 0100 1001", "1011 0110 0100", "1011 0111 0001",
                            0x092B,           0x0A95,           0x0B49,           0x0B64,           0x0B71,
    //* 1398 -1402 */ "0101 1011 0100", "1010 1011 0101", "1010 1001 0110", "1101 0100 1010", "1110 1001 0010",
                            0x05B4,           0x0AB5,           0x0A96,           0x0B4A,           0x0E92,
    //* 1403 -1407 */ "1110 1100 1001", "0110 1101 0100", "1010 1110 1001", "1010 1010 1101", "0101 0101 0101",
                            0x0EC9,           0x06D4,           0x0AE9,           0x0AAD,           0x0555,
    //* 1408 -1412 */ "1010 1010 0101", "1011 0101 0010", "1101 1010 0100", "1101 1011 0010", "1001 1011 1010",
                            0x0AA5,           0x0B52,           0x0DA4,           0x0DB2,           0x09BA,
    //* 1413 -1417 */ "0100 1011 1010", "1010 0101 1011", "0101 0010 1101", "1010 1010 0101", "1010 1101 0100",
                            0x04BA,           0x0A5B,           0x052D,           0x0AA5,           0x0AD4,
    //* 1418 -1422 */ "1010 1110 1010", "0101 0101 1100", "0100 1011 1101", "0010 0011 1101", "1001 0001 1101",
                            0x0AEA,           0x055C,           0x04BD,           0x023D,           0x091D,
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
    //* 1478 -1480 */ "0010 1011 0110", "1001 0101 0110", "1010 1010 1010"
                            0x02B6,           0x0956,           0x0AAA
};

int32_t getUmalqura_MonthLength(int32_t y, int32_t m) {
    int32_t mask = (int32_t) (0x01 << (11 - m));    // set mask for bit corresponding to month
    if((UMALQURA_MONTHLENGTH[y] & mask) == 0 )
        return 29;
    else
        return 30;

}

//-------------------------------------------------------------------------
// Constructors...
//-------------------------------------------------------------------------

const char *IslamicCalendar::getType() const {
    const char *sType = NULL;

    switch (cType) {
    case CIVIL:
        sType = "islamic-civil";
        break;
    case ASTRONOMICAL:
        sType = "islamic";
        break;
    case TBLA:
        sType = "islamic-tbla";
        break;
    case UMALQURA:
        sType = "islamic-umalqura";
        break;
    default:
        U_ASSERT(false); // out of range
        sType = "islamic";  // "islamic" is used as the generic type
        break;
    }
    return sType;
}

Calendar* IslamicCalendar::clone() const {
    return new IslamicCalendar(*this);
}

IslamicCalendar::IslamicCalendar(const Locale& aLocale, UErrorCode& success, ECalculationType type)
:   Calendar(TimeZone::createDefault(), aLocale, success),
cType(type)
{
    setTimeInMillis(getNow(), success); // Call this again now that the vtable is set up properly.
}

IslamicCalendar::IslamicCalendar(const IslamicCalendar& other) : Calendar(other), cType(other.cType) {
}

IslamicCalendar::~IslamicCalendar()
{
}

void IslamicCalendar::setCalculationType(ECalculationType type, UErrorCode &status)
{
    if (cType != type) {
        // The fields of the calendar will become invalid, because the calendar
        // rules are different
        UDate m = getTimeInMillis(status);
        cType = type;
        clear();
        setTimeInMillis(m, status);
    }
}

/**
* Returns <code>true</code> if this object is using the fixed-cycle civil
* calendar, or <code>false</code> if using the religious, astronomical
* calendar.
* @draft ICU 2.4
*/
UBool IslamicCalendar::isCivil() {
    return (cType == CIVIL);
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

// table mapping extended year to julian day
// for year start in the Umm-Al-Qura calendar.
// 660 bytes
static const int32_t jdForUmAlQuraYrStart[] = {
//  jdIsUm     isYr
    466700, // 1318
    467055, // 1319
    467410, // 1320
    467764, // 1321
    468119, // 1322
    468473, // 1323
    468827, // 1324
    469181, // 1325
    469536, // 1326
    469890, // 1327
    470245, // 1328
    470599, // 1329
    470953, // 1330
    471307, // 1331
    471662, // 1332
    472016, // 1333
    472371, // 1334
    472725, // 1335
    473080, // 1336
    473434, // 1337
    473788, // 1338
    474142, // 1339
    474497, // 1340
    474851, // 1341
    475206, // 1342
    475560, // 1343
    475914, // 1344
    476268, // 1345
    476622, // 1346
    476977, // 1347
    477332, // 1348
    477686, // 1349
    478041, // 1350
    478395, // 1351
    478749, // 1352
    479103, // 1353
    479458, // 1354
    479812, // 1355
    480167, // 1356
    480522, // 1357
    480876, // 1358
    481230, // 1359
    481584, // 1360
    481938, // 1361
    482293, // 1362
    482647, // 1363
    483002, // 1364
    483356, // 1365
    483710, // 1366
    484064, // 1367
    484419, // 1368
    484773, // 1369
    485128, // 1370
    485483, // 1371
    485837, // 1372
    486191, // 1373
    486545, // 1374
    486899, // 1375
    487254, // 1376
    487609, // 1377
    487963, // 1378
    488318, // 1379
    488672, // 1380
    489026, // 1381
    489380, // 1382
    489734, // 1383
    490089, // 1384
    490444, // 1385
    490798, // 1386
    491152, // 1387
    491506, // 1388
    491860, // 1389
    492215, // 1390
    492570, // 1391
    492924, // 1392
    493279, // 1393
    493633, // 1394
    493987, // 1395
    494341, // 1396
    494695, // 1397
    495050, // 1398
    495404, // 1399
    495759, // 1400
    496113, // 1401
    496467, // 1402
    496821, // 1403
    497176, // 1404
    497530, // 1405
    497885, // 1406
    498240, // 1407
    498594, // 1408
    498948, // 1409
    499302, // 1410
    499656, // 1411
    500011, // 1412
    500366, // 1413
    500720, // 1414
    501075, // 1415
    501429, // 1416
    501783, // 1417
    502137, // 1418
    502492, // 1419
    502846, // 1420
    503201, // 1421
    503555, // 1422
    503909, // 1423
    504263, // 1424
    504617, // 1425
    504972, // 1426
    505327, // 1427
    505681, // 1428
    506036, // 1429
    506390, // 1430
    506744, // 1431
    507098, // 1432
    507452, // 1433
    507807, // 1434
    508161, // 1435
    508516, // 1436
    508870, // 1437
    509224, // 1438
    509578, // 1439
    509933, // 1440
    510287, // 1441
    510642, // 1442
    510996, // 1443
    511351, // 1444
    511705, // 1445
    512059, // 1446
    512413, // 1447
    512768, // 1448
    513123, // 1449
    513477, // 1450
    513831, // 1451
    514186, // 1452
    514540, // 1453
    514894, // 1454
    515249, // 1455
    515604, // 1456
    515958, // 1457
    516313, // 1458
    516667, // 1459
    517021, // 1460
    517375, // 1461
    517729, // 1462
    518084, // 1463
    518439, // 1464
    518793, // 1465
    519147, // 1466
    519501, // 1467
    519856, // 1468
    520210, // 1469
    520565, // 1470
    520919, // 1471
    521274, // 1472
    521628, // 1473
    521982, // 1474
    522336, // 1475
    522690, // 1476
    523045, // 1477
    523400, // 1478
    523754, // 1479
    524108, // 1480
};

/**
* Determine whether a year is a leap year in the Islamic civil calendar
*/
UBool IslamicCalendar::civilLeapYear(int32_t year)
{
    return (14 + 11 * year) % 30 < 11;
}

/**
* Return the day # on which the given year starts.  Days are counted
* from the Hijri epoch, origin 0.
*/
int32_t IslamicCalendar::yearStart(int32_t year) const{
    if (cType == CIVIL || cType == TBLA ||
        (cType == UMALQURA && (year < UMALQURA_YEAR_START || year > UMALQURA_YEAR_END))) 
    {
        // The following is similar to part of
        // Dershowitz & Reingold, 3rd ed., formula 6.3
        // adjusted to give day offset within current epoch.
        return (year-1)*354 + ClockMath::floorDivide((3+11*year),30);
    } else if(cType==ASTRONOMICAL){
        return trueMonthStart(12*(year-1));
    } else {
        return jdForUmAlQuraYrStart[year - UMALQURA_YEAR_START];
    }
}

/**
* Return the day # on which the given month starts.  Days are counted
* from the Hijri epoch, origin 0.
*
* @param year  The hijri year
* @param year  The hijri month, 0-based (assumed to be in range 0..11)
*/
int32_t IslamicCalendar::monthStart(int32_t year, int32_t month) const {
    if (cType == CIVIL || cType == TBLA) {
        // The following is similar to part of
        // Dershowitz & Reingold, 3rd ed., formula 6.3
        // adjusted for 0-based months and to give day offset within current epoch.
        // This does not handle months out of the range 0..11
        return (year-1)*354 + (int32_t)ClockMath::floorDivide((3+11*year),30) +
            (int32_t)uprv_ceil(29.5*month);
    } else if(cType==ASTRONOMICAL){
        return trueMonthStart(12*(year-1) + month);
    } else {
        int32_t ms = yearStart(year);
        for(int i=0; i< month; i++){
            ms+= handleGetMonthLength(year, i);
        }
        return ms;
    }
}

/**
* Find the day number on which a particular month of the true/lunar
* Islamic calendar starts.
*
* @param month The month in question, origin 0 from the Hijri epoch
*
* @return The day number on which the given month starts.
*/
int32_t IslamicCalendar::trueMonthStart(int32_t month) const
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t start = CalendarCache::get(&gMonthCache, month, status);

    if (start==0) {
        // Make a guess at when the month started, using the average length
        UDate origin = HIJRA_MILLIS 
            + uprv_floor(month * CalendarAstronomer::SYNODIC_MONTH) * kOneDay;

        // moonAge will fail due to memory allocation error
        double age = moonAge(origin, status);
        if (U_FAILURE(status)) {
            goto trueMonthStartEnd;
        }

        if (age >= 0) {
            // The month has already started
            do {
                origin -= kOneDay;
                age = moonAge(origin, status);
                if (U_FAILURE(status)) {
                    goto trueMonthStartEnd;
                }
            } while (age >= 0);
        }
        else {
            // Preceding month has not ended yet.
            do {
                origin += kOneDay;
                age = moonAge(origin, status);
                if (U_FAILURE(status)) {
                    goto trueMonthStartEnd;
                }
            } while (age < 0);
        }
        start = (int32_t)ClockMath::floorDivide((origin - HIJRA_MILLIS), (double)kOneDay) + 1;
        CalendarCache::put(&gMonthCache, month, start, status);
    }
trueMonthStartEnd :
    if(U_FAILURE(status)) {
        start = 0;
    }
    return start;
}

/**
* Return the "age" of the moon at the given time; this is the difference
* in ecliptic latitude between the moon and the sun.  This method simply
* calls CalendarAstronomer.moonAge, converts to degrees, 
* and adjusts the result to be in the range [-180, 180].
*
* @param time  The time at which the moon's age is desired,
*              in millis since 1/1/1970.
*/
double IslamicCalendar::moonAge(UDate time, UErrorCode &status)
{
    double age = 0;

    umtx_lock(&astroLock);
    if(gIslamicCalendarAstro == NULL) {
        gIslamicCalendarAstro = new CalendarAstronomer();
        if (gIslamicCalendarAstro == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return age;
        }
        ucln_i18n_registerCleanup(UCLN_I18N_ISLAMIC_CALENDAR, calendar_islamic_cleanup);
    }
    gIslamicCalendarAstro->setTime(time);
    age = gIslamicCalendarAstro->getMoonAge();
    umtx_unlock(&astroLock);

    // Convert to degrees and normalize...
    age = age * 180 / CalendarAstronomer::PI;
    if (age > 180) {
        age = age - 360;
    }

    return age;
}

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
int32_t IslamicCalendar::handleGetMonthLength(int32_t extendedYear, int32_t month) const {

    int32_t length = 0;

    if (cType == CIVIL || cType == TBLA ||
        (cType == UMALQURA && (extendedYear<UMALQURA_YEAR_START || extendedYear>UMALQURA_YEAR_END)) ) {
        length = 29 + (month+1) % 2;
        if (month == DHU_AL_HIJJAH && civilLeapYear(extendedYear)) {
            length++;
        }
    } else if(cType == ASTRONOMICAL){
        month = 12*(extendedYear-1) + month;
        length =  trueMonthStart(month+1) - trueMonthStart(month) ;
    } else {
        length = getUmalqura_MonthLength(extendedYear - UMALQURA_YEAR_START, month);
    }
    return length;
}

/**
* Return the number of days in the given Islamic year
* @draft ICU 2.4
*/
int32_t IslamicCalendar::handleGetYearLength(int32_t extendedYear) const {
    if (cType == CIVIL || cType == TBLA ||
        (cType == UMALQURA && (extendedYear<UMALQURA_YEAR_START || extendedYear>UMALQURA_YEAR_END)) ) {
        return 354 + (civilLeapYear(extendedYear) ? 1 : 0);
    } else if(cType == ASTRONOMICAL){
        int32_t month = 12*(extendedYear-1);
        return (trueMonthStart(month + 12) - trueMonthStart(month));
    } else {
        int len = 0;
        for(int i=0; i<12; i++) {
            len += handleGetMonthLength(extendedYear, i);
        }
        return len;
    }
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
int32_t IslamicCalendar::handleComputeMonthStart(int32_t eyear, int32_t month, UBool /* useMonth */) const {
    // This may be called by Calendar::handleComputeJulianDay with months out of the range
    // 0..11. Need to handle that here since monthStart requires months in the range 0.11.
    if (month > 11) {
        eyear += (month / 12);
        month %= 12;
    } else if (month < 0) {
        month++;
        eyear += (month / 12) - 1;
        month = (month % 12) + 11;
    }
    return monthStart(eyear, month) + ((cType == TBLA)? ASTRONOMICAL_EPOC: CIVIL_EPOC) - 1;
}    

//-------------------------------------------------------------------------
// Functions for converting from milliseconds to field values
//-------------------------------------------------------------------------

/**
* @draft ICU 2.4
*/
int32_t IslamicCalendar::handleGetExtendedYear() {
    int32_t year;
    if (newerField(UCAL_EXTENDED_YEAR, UCAL_YEAR) == UCAL_EXTENDED_YEAR) {
        year = internalGet(UCAL_EXTENDED_YEAR, 1); // Default to year 1
    } else {
        year = internalGet(UCAL_YEAR, 1); // Default to year 1
    }
    return year;
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
    int32_t year, month, dayOfMonth, dayOfYear;
    int32_t startDate;
    int32_t days = julianDay - CIVIL_EPOC;

    if (cType == CIVIL || cType == TBLA) {
        if(cType == TBLA) {
            days = julianDay - ASTRONOMICAL_EPOC;
        }
        // Use the civil calendar approximation, which is just arithmetic.
        // The following is similar to part of
        // Dershowitz & Reingold, 3rd ed., formula 6.4
        year  = (int)ClockMath::floorDivide( (double)(30 * days + 10646) , 10631.0 );
        month = (int32_t)uprv_ceil((days - 29 - yearStart(year)) / 29.5 );
        month = month<11?month:11;
        startDate = monthStart(year, month);
    } else if(cType == ASTRONOMICAL){
        // Guess at the number of elapsed full months since the epoch
        int32_t months = (int32_t)uprv_floor((double)days / CalendarAstronomer::SYNODIC_MONTH);

        startDate = (int32_t)uprv_floor(months * CalendarAstronomer::SYNODIC_MONTH);

        double age = moonAge(internalGetTime(), status);
        if (U_FAILURE(status)) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        if ( days - startDate >= 25 && age > 0) {
            // If we're near the end of the month, assume next month and search backwards
            months++;
        }

        // Find out the last time that the new moon was actually visible at this longitude
        // This returns midnight the night that the moon was visible at sunset.
        while ((startDate = trueMonthStart(months)) > days) {
            // If it was after the date in question, back up a month and try again
            months--;
        }

        year = months / 12 + 1;
        month = months % 12;
    } else if(cType == UMALQURA) {
        int32_t umalquraStartdays = yearStart(UMALQURA_YEAR_START) ;
        if( days < umalquraStartdays){
                //Use Civil calculation
                year  = (int)ClockMath::floorDivide( (double)(30 * days + 10646) , 10631.0 );
                month = (int32_t)uprv_ceil((days - 29 - yearStart(year)) / 29.5 );
                month = month<11?month:11;
                startDate = monthStart(year, month);
            }else{
                int y =UMALQURA_YEAR_START-1, m =0;
                long d = 1;
                while(d > 0){ 
                    y++; 
                    d = days - yearStart(y) +1;
                    if(d == handleGetYearLength(y)){
                        m=11;
                        break;
                    }else if(d < handleGetYearLength(y) ){
                        int monthLen = handleGetMonthLength(y, m); 
                        m=0;
                        while(d > monthLen){
                            d -= monthLen;
                            m++;
                            monthLen = handleGetMonthLength(y, m);
                        }
                        break;
                    }
                }
                year = y;
                month = m;
            }
    } else { // invalid 'civil'
      U_ASSERT(false); // should not get here, out of range
      year=month=0;
    }

    dayOfMonth = (days - monthStart(year, month)) + 1;

    // Now figure out the day of the year.
    dayOfYear = (days - monthStart(year, 0)) + 1;


    internalSet(UCAL_ERA, 0);
    internalSet(UCAL_YEAR, year);
    internalSet(UCAL_EXTENDED_YEAR, year);
    internalSet(UCAL_MONTH, month);
    internalSet(UCAL_DAY_OF_MONTH, dayOfMonth);
    internalSet(UCAL_DAY_OF_YEAR, dayOfYear);       
}    

UBool
IslamicCalendar::inDaylightTime(UErrorCode& status) const
{
    // copied from GregorianCalendar
    if (U_FAILURE(status) || (&(getTimeZone()) == NULL && !getTimeZone().useDaylightTime())) 
        return FALSE;

    // Force an update of the state of the Calendar.
    ((IslamicCalendar*)this)->complete(status); // cast away const

    return (UBool)(U_SUCCESS(status) ? (internalGet(UCAL_DST_OFFSET) != 0) : FALSE);
}

/**
 * The system maintains a static default century start date and Year.  They are
 * initialized the first time they are used.  Once the system default century date 
 * and year are set, they do not change.
 */
static UDate           gSystemDefaultCenturyStart       = DBL_MIN;
static int32_t         gSystemDefaultCenturyStartYear   = -1;
static icu::UInitOnce  gSystemDefaultCenturyInit        = U_INITONCE_INITIALIZER;


UBool IslamicCalendar::haveDefaultCentury() const
{
    return TRUE;
}

UDate IslamicCalendar::defaultCenturyStart() const
{
    // lazy-evaluate systemDefaultCenturyStart
    umtx_initOnce(gSystemDefaultCenturyInit, &initializeSystemDefaultCentury);
    return gSystemDefaultCenturyStart;
}

int32_t IslamicCalendar::defaultCenturyStartYear() const
{
    // lazy-evaluate systemDefaultCenturyStartYear
    umtx_initOnce(gSystemDefaultCenturyInit, &initializeSystemDefaultCentury);
    return gSystemDefaultCenturyStartYear;
}


void U_CALLCONV
IslamicCalendar::initializeSystemDefaultCentury()
{
    // initialize systemDefaultCentury and systemDefaultCenturyYear based
    // on the current time.  They'll be set to 80 years before
    // the current time.
    UErrorCode status = U_ZERO_ERROR;
    IslamicCalendar calendar(Locale("@calendar=islamic-civil"),status);
    if (U_SUCCESS(status)) {
        calendar.setTime(Calendar::getNow(), status);
        calendar.add(UCAL_YEAR, -80, status);

        gSystemDefaultCenturyStart = calendar.getTime(status);
        gSystemDefaultCenturyStartYear = calendar.get(UCAL_YEAR, status);
    }
    // We have no recourse upon failure unless we want to propagate the failure
    // out.
}



UOBJECT_DEFINE_RTTI_IMPLEMENTATION(IslamicCalendar)

U_NAMESPACE_END

#endif

