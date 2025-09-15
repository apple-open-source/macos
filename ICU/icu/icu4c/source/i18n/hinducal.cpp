// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 ******************************************************************************
 * Copyright (C) 2007-2023, International Business Machines Corporation
 * and others. All Rights Reserved.
 ******************************************************************************
 *
 * File HINDUCAL.CPP
 *
 *
 *
 *****************************************************************************
 */

#include "hinducal.h"

#if !UCONFIG_NO_FORMATTING

#include "umutex.h"
#include <float.h>
#include "gregoimp.h" // Math
#include "astro.h" // CalendarAstronomer
#include "unicode/gregocal.h"
#include "unicode/simpletz.h"
#include "uhash.h"
#include "ucln_in.h"

// --- The cache --
static icu::UMutex astroLock;

// Lazy Creation & Access synchronized by class CalendarCache with a mutex.
static icu::CalendarCache *gHinduCalendarWinterSolsticeCache = NULL;
static icu::CalendarCache *gHinduCalendarNewYearCache = NULL;

static icu::TimeZone *gHinduCalendarZoneAstroCalc = NULL;
static icu::UInitOnce gHinduCalendarZoneAstroCalcInitOnce {};

static icu::Location makeLocation(const icu::Angle& latitude, const icu::Angle& longitude, int32_t meters, double hours) {
    return {latitude, longitude, meters, hours};
}

// Define the Hindu location (Ujjain) as a constant
static const icu::Location UJJAIN = {
    {23, 9, 0},
    {75, 46, 0},
    0,
    (5 + 461.0 / 9000.0) / 24.0
};

U_CDECL_BEGIN
static UBool calendar_hindu_cleanup(void) {
    if (gHinduCalendarWinterSolsticeCache) {
        delete gHinduCalendarWinterSolsticeCache;
        gHinduCalendarWinterSolsticeCache = NULL;
    }
    if (gHinduCalendarNewYearCache) {
        delete gHinduCalendarNewYearCache;
        gHinduCalendarNewYearCache = NULL;
    }
    if (gHinduCalendarZoneAstroCalc) {
        delete gHinduCalendarZoneAstroCalc;
        gHinduCalendarZoneAstroCalc = NULL;
    }
    gHinduCalendarZoneAstroCalcInitOnce.reset();
    return true;
}
U_CDECL_END

U_NAMESPACE_BEGIN


// Implementation of the HinduLunarCalendar class

///////////////////////////////////////////////////////////////
// Constructors
///////////////////////////////////////////////////////////////


HinduLunarCalendar* HinduLunarCalendar::clone() const {
    return new HinduLunarCalendar(*this);
}

HinduLunarCalendar::HinduLunarCalendar(const Locale& aLocale, UErrorCode& success)
:   Calendar(TimeZone::forLocaleOrDefault(aLocale), aLocale, success),
    isLeapYear(false),
    hinduLocation(UJJAIN),
    fEpochYear(HINDU_EPOCH_YEAR),
    fZoneAstroCalc(getHinduCalZoneAstroCalc())
{
    setTimeInMillis(getNow(), success); // Call this again now that the vtable is set up properly.
}

HinduLunarCalendar::HinduLunarCalendar(const Locale& aLocale, int32_t epochYear,
                                const TimeZone* zoneAstroCalc, UErrorCode &success)
:   Calendar(TimeZone::forLocaleOrDefault(aLocale), aLocale, success),
    isLeapYear(false),
    hinduLocation(UJJAIN),
    fEpochYear(epochYear),
    fZoneAstroCalc(zoneAstroCalc)
{
    setTimeInMillis(getNow(), success); // Call this again now that the vtable is set up properly.
}

HinduLunarCalendar::HinduLunarCalendar(const HinduLunarCalendar& other) : Calendar(other) {
    isLeapYear = other.isLeapYear;
    fEpochYear = other.fEpochYear;
    fZoneAstroCalc = other.fZoneAstroCalc;
    hinduLocation = other.hinduLocation;
}

HinduLunarCalendar::~HinduLunarCalendar()
{
}

const char *HinduLunarCalendar::getType() const {
    return "hindu-lunar";
}

static void U_CALLCONV initHinduCalZoneAstroCalc() {
    gHinduCalendarZoneAstroCalc = new SimpleTimeZone(HINDU_OFFSET, UNICODE_STRING_SIMPLE("Asia/Kolkata") );
    ucln_i18n_registerCleanup(UCLN_I18N_HINDU_CALENDAR, calendar_hindu_cleanup);
}

const TimeZone* HinduLunarCalendar::getHinduCalZoneAstroCalc(void) const {
    umtx_initOnce(gHinduCalendarZoneAstroCalcInitOnce, &initHinduCalZoneAstroCalc);
    return gHinduCalendarZoneAstroCalc;
}



///////////////////////////////////////////////////////////////
//      Astronomy helpers
///////////////////////////////////////////////////////////////

// Ctors

// hindu-lunar-date
HinduDate HinduLunarCalendar::hinduDate(int32_t year, int32_t month, bool leapMonth, int32_t day, bool leapDay) const {
    return {year, month, leapMonth, day, leapDay};
}

// hindu-lunar-from-fixed
HinduDate HinduLunarCalendar::hinduLunarFromFixed(int32_t date) const {
    date -= RD_OFFSET + 1;
    double critical = CalendarAstronomer::hinduSunrise(date, ujjainOffset(), UJJAIN.latitude.degrees);
    int32_t day = CalendarAstronomer::hinduLunarDayFromMoment(critical);
    bool leapDay = (day == CalendarAstronomer::hinduLunarDayFromMoment(
                    CalendarAstronomer::hinduSunrise(date - 1, ujjainOffset(), UJJAIN.latitude.degrees)));

    double lastNewMoon = CalendarAstronomer::hinduNewMoonBefore(critical);
    double nextNewMoon = CalendarAstronomer::hinduNewMoonBefore(floor(lastNewMoon) + 35);
    int32_t solarMonth = CalendarAstronomer::hinduZodiac(lastNewMoon);
    bool leapMonth = (solarMonth == CalendarAstronomer::hinduZodiac(nextNewMoon));
    int32_t month = (solarMonth + 1) % 12;
    int32_t year = CalendarAstronomer::hinduCalendarYear((month <= 2) ? date + 180 : date) - HINDU_LUNAR_ERA;
    // rdar://147055320
    // Calculations up to this point gives us a date for Hindu lunisolar calendars based on Amanta and Vikram era
    // More information about Amanta vs Purnimanta can be found here https://www.drikpanchang.com/faq/faq-ans8.html
    // A month in both variants has 30 days: 14 days in waxing fortnight (paksa), 14 days waning fortnight, the new moon and the full moon
    // As a month in Amanta ends on new moon and a month in Purimanta ends on full moon, there is 15 days of difference, with Amanta being behind.
    //      Vikram - Purnimanta
    //      Gujarati, Kannada, Marathi, Telugu - Amanta
    // Second adjustment is for eras
    // Vikram era starts in 57 BCE, and Saka starts on 78 CE
    //      Vikram, Gujarati - Vikram
    //      Kannada, Marathi, Telugu - Saka
    // Following adjustment is accounting for those differences
    // rdar://151394973 For leap months Amanta and Purnimanta aligns
    if (getHinduCalendarType() == PURNIMANTA && !leapMonth) {
        if (day <= AMANTA_OFFSET) {
            day += AMANTA_OFFSET;
        }
        else {
            // roll up a month
            day -= AMANTA_OFFSET;
            month++;            
            if (month == 12) {
                // roll up a year
                month = 0;
                year++;
            }
        }
    }

    year = year + getYearOffset() - VIKRAM_OFFSET;
  
    // rdar://145905328 offset adjustments for incorrect calculations
    if (CalendarAstronomer::hinduVikramOffset(date) != 0) {
        day += CalendarAstronomer::hinduVikramOffset(date);
        if (day == 31) {
            day = 1;
            month++;
            if (month == 12) {
                month = 0;
                year++;
            }
        }
        else if (day == 0) {
            day = 30;
            month--;
            if (month == -1) {
                month = 11;
                year--;
            }
        }
    }
    if (CalendarAstronomer::updateRepeatedDay(date) == 1) {
        leapDay = !leapDay;
    }
    
    // rdar://156472909 ([New Calendars]: GJ: Luck23A304: Incorrect date is marked as a new year in Gujarati alternative calendar)
    month += getMonthOffset();
    if (month < 0) {
        month += 12;
        --year;
    } else if (month > 11) {
        month -= 12;
        ++year;
    }

    return HinduDate{year, month, leapMonth, day, leapDay};
}

// hindu-fullmoon-from-fixed
HinduDate HinduLunarCalendar::hinduFullmoonFromFixed(int32_t date) const {
    HinduDate lDate = hinduLunarFromFixed(date);
    int32_t year = lDate.year;
    int32_t month = lDate.month; // Assuming "hindu-lunar-month" refers to the full-moon scheme
    bool leapMonth = lDate.leapMonth;
    int32_t day = lDate.day;
    bool leapDay = lDate.leapDay;

    int32_t m = (day >= 16) ? (hinduLunarFromFixed(date + 20)).month : month;

    return HinduDate{year, m, leapMonth, day, leapDay};
}

// APIs

// Set Location

void HinduLunarCalendar::setLocation(const Angle& latitude, const Angle& longitude, int32_t meters, double hours) {
	hinduLocation = makeLocation(latitude, longitude, meters, hours);
}

// fixed-from-hindu-lunar
int32_t HinduLunarCalendar::fixedFromHinduLunar(HinduDate lDate) const {
    int32_t year = lDate.year;
    int32_t month = lDate.month;
    int32_t leapMonth = lDate.leapMonth;
    int32_t day = lDate.day;
    int32_t leapDay = lDate.leapDay;

    // rdar://156472909 ([New Calendars]: GJ: Luck23A304: Incorrect date is marked as a new year in Gujarati alternative calendar)
    month -= getMonthOffset();
    if (month < 0) {
        month += 12;
        --year;
    } else if (month > 11) {
        month -= 12;
        ++year;
    }

    double approx = HINDU_SIDEREAL_YEAR * (year + HINDU_LUNAR_ERA + (month - 1) / 12);
    int32_t s = floor(approx - HINDU_SIDEREAL_YEAR * mod3((CalendarAstronomer::hinduSolarLongitude(approx) / 360.0) - ((month - 1) / 12.0), -0.5, 0.5));

    int32_t k = CalendarAstronomer::hinduLunarDayFromMoment(s + CalendarAstronomer::hr(6));
    int32_t est = s - day + ((k < 3 || k >= 27) ? k : (((hinduLunarFromFixed(s - 15)).month != month || (leapMonth && !(hinduLunarFromFixed(s - 15)).month)) ? mod3(k, -15, 15) : mod3(k, 15, 45)));
    int32_t tau = est - mod3(CalendarAstronomer::hinduLunarDayFromMoment(est + CalendarAstronomer::hr(6)) - day, -15, 15);
    
    int32_t date = day;
    
    for (int32_t i = tau - 1; i <= 30; ++i) {
        if (CalendarAstronomer::hinduLunarDayFromMoment(CalendarAstronomer::hinduSunrise(i, ujjainOffset(), UJJAIN.latitude.degrees)) == day
            || CalendarAstronomer::hinduLunarDayFromMoment(CalendarAstronomer::hinduSunrise(i, ujjainOffset(), UJJAIN.latitude.degrees)) == amod(day + 1, 30)) {
            date = i;
        }
    }

    return leapDay ? date + 1 : date;
}

// hindu-day-count
int32_t HinduLunarCalendar::hinduDayCount(int32_t date) const {
    // Elapsed days (Ahargana) to date since Hindu epoch (KY).
    return date - HINDU_EPOCH_YEAR;
}

// hindu-lunar-new-year
int32_t HinduLunarCalendar::hinduLunarNewYear(int32_t gYear) const {
    double jan1 = gregorianNewYear(gYear);
    double mina = CalendarAstronomer::hinduSolarLongitudeAtOrAfter(330.0, jan1);
    double newMoon = CalendarAstronomer::hinduLunarDayAtOrAfter(1.0, mina);
    int32_t hDay = static_cast<int32_t>(floor(newMoon));
    double critical = CalendarAstronomer::hinduSunrise(hDay, ujjainOffset(), UJJAIN.latitude.degrees);

    return hDay + ((newMoon < critical)
                   || (CalendarAstronomer::hinduLunarDayFromMoment(
                            CalendarAstronomer::hinduSunrise(hDay + 1, ujjainOffset(), UJJAIN.latitude.degrees)) == 2) ?
                            0 : 1);
}
    
// Calendar parts

int32_t HinduLunarCalendar::gregorianNewYear(int32_t gYear) const {
    // // rdar://145903949 get the Gregorian year using Vikram offset
    UErrorCode status = U_ZERO_ERROR;
    GregorianCalendar gc(gYear, 0, 1, 0, 0, status);
    return gc.get(UCAL_JULIAN_DAY, status) - RD_OFFSET;
}

std::pair<int32_t,int32_t> HinduLunarCalendar::gregorianYearRange(int32_t gYear) const {
	return createInterval(gregorianNewYear(gYear), gregorianNewYear(gYear + 1));
}


double HinduLunarCalendar::ujjainOffset() const {
    return UJJAIN.longitude.degrees - hinduLocation.longitude.degrees;
}

///////////////////////////////////////////////////////////////
//      Math and algo helpers
///////////////////////////////////////////////////////////////

double HinduLunarCalendar::angle(int32_t d, int32_t m, double s) const {
    return static_cast<double>(d) + (static_cast<double>(m) + (s / 60.0)) / 60.0;
}


int32_t HinduLunarCalendar::amod(int32_t x, int32_t y) const {
    // The value of (x mod y) with y instead of 0.
    return (y + (x % y)) % y;
}

double HinduLunarCalendar::mod3(double x, double a, double b) const {
    // The value of x shifted into the range [a..b). Returns x if a=b.
    if (a == b) {
        return x;
    }
    return a + modLisp(x - a, b - a);
}

double HinduLunarCalendar::modLisp(double a, double b) const {
    // Simulating lisp modulo behavior which returns true modulo for negative numbers
    // C return negative remainder instead.
    // It is really (a > 0.0 ? fmod(a, b) : fmod(a + b, b);)
    // but fmod(a, b) is equal to fmod(a + b, b) anyway
    return fmod(a, b) > 0 ? fmod(a, b) : fmod(a + b, b);
}



// Vector helpers
double HinduLunarCalendar::getBegin(const std::pair<double, double>& range) const {
    return range.first;
}

double HinduLunarCalendar::getEnd(const std::pair<double, double>& range) const {
    return range.second;
}

bool HinduLunarCalendar::isInRange(double tee, const std::pair<double, double>& range) const {
    return (getBegin(range) <= tee) && (tee < getEnd(range));
}

std::vector<int32_t> HinduLunarCalendar::listRange(const std::vector<int32_t>& ell, const std::pair<int32_t, int32_t>& range) const {
    if (!ell.empty()) {
        auto r = listRange(std::vector<int32_t>(ell.begin() + 1, ell.end()), range);
        if (isInRange(ell.front(), range)) {
            r.insert(r.begin(), ell.front());
        }
        return r;
    } else {
        return {};
    }
}

std::pair<int32_t, int32_t> HinduLunarCalendar::createInterval(int32_t t0, int32_t t1) const {
    return {t0, t1};
}

///////////////////////////////////////////////////////////////
// Minimum / Maximum access functions
///////////////////////////////////////////////////////////////


static const int32_t LIMITS[UCAL_FIELD_COUNT][4] = {
    // Minimum  Greatest     Least    Maximum
    //           Minimum   Maximum
    {        0,        0,        0,        0}, // ERA
    { -5000000, -5000000,  5000000,  5000000}, // YEAR
    {        0,        0,       11,       11}, // MONTH
    {        1,        1,       52,       53}, // WEEK_OF_YEAR
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // WEEK_OF_MONTH
    {        1,        1,       29,       30}, // DAY_OF_MONTH
    {        1,        1,      353,      385}, // DAY_OF_YEAR
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
    { -5000000, -5000000,  5000000,  5000000}, // YEAR_WOY
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // DOW_LOCAL
    { -5000000, -5000000,  5000000,  5000000}, // EXTENDED_YEAR
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // JULIAN_DAY
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // MILLISECONDS_IN_DAY
    {        0,        0,        1,        1}, // IS_LEAP_MONTH
    {        0,        0,       11,       11}, // ORDINAL_MONTH
    {        0,        0,        1,        1}, // IS_REPEATED_DAY // rdar://138880732
};


/**
* @draft ICU 2.4
*/
int32_t HinduLunarCalendar::handleGetLimit(UCalendarDateFields field, ELimitType limitType) const {
    if (((field == UCAL_IS_LEAP_MONTH) || (field == UCAL_IS_REPEATED_DAY))
        && (getHinduCalendarType() == SOLAR)) {
        // solar calendars don't have leap months or repeated days (rdar://154212299)
        return -1;
    }
    return LIMITS[field][limitType];
}


///////////////////////////////////////////////////////////////
// Calendar framework
///////////////////////////////////////////////////////////////

/**
 * Implement abstract Calendar method to return the extended year
 * defined by the current fields.  This will use either the ERA and
 * YEAR field as the cycle and year-of-cycle, or the EXTENDED_YEAR
 * field as the continuous year count, depending on which is newer.
 * @stable ICU 2.8
 */
int32_t HinduLunarCalendar::handleGetExtendedYear(UErrorCode& status) {
    return internalGet(UCAL_YEAR, 1) - (fEpochYear - HINDU_EPOCH_YEAR);
}

/**
 * Override Calendar method to return the number of days in the given
 * extended year and month.
 *
 * <p>Note: This method also reads the IS_LEAP_MONTH field to determine
 * whether or not the given month is a leap month.
 * @stable ICU 2.8
 */
int32_t HinduLunarCalendar::handleGetMonthLength(int32_t extendedYear, int32_t month, UErrorCode& status) const {
    int64_t thisStart = handleComputeMonthStart(extendedYear, month, true, status); 
    int64_t nextStart = month == 11 ? handleComputeMonthStart(extendedYear + 1, 0, true, status) : handleComputeMonthStart(extendedYear, month + 1, true, status);
    int32_t length = nextStart - thisStart;
 
    if (length == 0) {
        // no new moon, so we need to find the start of the next month
         length = handleComputeMonthStart(extendedYear, month + 2, true, status) - thisStart;
    }
    if (length >= 58 && length <= 60)
    {
        // leap month, so we need to readjust the length
        length = length == 58 ? length - 29 : length - 30;
    }
    return length;
}

/**
 * Override Calendar to compute several fields specific to the Hindu
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
 * method is called.  The getGregorianXxx() methods return Gregorian
 * calendar equivalents for the given Julian day.
 *
 * <p>Compute the HinduLunarCalendar-specific field IS_LEAP_MONTH.
 * @stable ICU 2.8
 */
void HinduLunarCalendar::handleComputeFields(int32_t julianDay, UErrorCode &status) {
	HinduDate hinduDate = hinduLunarFromFixed(julianDay);
	
	internalSet(UCAL_YEAR, hinduDate.year);
	internalSet(UCAL_MONTH, hinduDate.month);
	internalSet(UCAL_IS_LEAP_MONTH, hinduDate.leapMonth ? 1 : 0);
	internalSet(UCAL_DAY_OF_MONTH, hinduDate.day);
    internalSet(UCAL_IS_REPEATED_DAY, hinduDate.leapDay ? 1 : 0); // rdar://138880732

    // rdar://156472909 ([New Calendars]: GJ: Luck23A304: Incorrect date is marked as a new year in Gujarati alternative calendar)
    // For Gujarati calendar the start of the year needs to be adjusted for the month offset as it starts
    int32_t yearStartJulianDay = (int32_t)handleComputeMonthStart(hinduDate.year, -getMonthOffset(), false, status);
    internalSet(UCAL_DAY_OF_YEAR, julianDay - yearStartJulianDay);
}

/**
 * Field resolution table that incorporates IS_LEAP_MONTH.
 */
const UFieldResolutionTable HinduLunarCalendar::HINDU_DATE_PRECEDENCE[] =
{
    {
        { UCAL_DAY_OF_MONTH, kResolveSTOP },
        { UCAL_WEEK_OF_YEAR, UCAL_DAY_OF_WEEK, kResolveSTOP },
        { UCAL_WEEK_OF_MONTH, UCAL_DAY_OF_WEEK, kResolveSTOP },
        { UCAL_DAY_OF_WEEK_IN_MONTH, UCAL_DAY_OF_WEEK, kResolveSTOP },
        { UCAL_WEEK_OF_YEAR, UCAL_DOW_LOCAL, kResolveSTOP },
        { UCAL_WEEK_OF_MONTH, UCAL_DOW_LOCAL, kResolveSTOP },
        { UCAL_DAY_OF_WEEK_IN_MONTH, UCAL_DOW_LOCAL, kResolveSTOP },
        { UCAL_DAY_OF_YEAR, kResolveSTOP },
        { kResolveRemap | UCAL_DAY_OF_MONTH, UCAL_IS_LEAP_MONTH, kResolveSTOP },
        { kResolveSTOP }
    },
    {
        { UCAL_WEEK_OF_YEAR, kResolveSTOP },
        { UCAL_WEEK_OF_MONTH, kResolveSTOP },
        { UCAL_DAY_OF_WEEK_IN_MONTH, kResolveSTOP },
        { kResolveRemap | UCAL_DAY_OF_WEEK_IN_MONTH, UCAL_DAY_OF_WEEK, kResolveSTOP },
        { kResolveRemap | UCAL_DAY_OF_WEEK_IN_MONTH, UCAL_DOW_LOCAL, kResolveSTOP },
        { kResolveSTOP }
    },
    {{kResolveSTOP}}
};

/**
 * Override Calendar to add IS_LEAP_MONTH to the field resolution
 * table.
 * @stable ICU 2.8
 */
const UFieldResolutionTable* HinduLunarCalendar::getFieldResolutionTable() const {
    return HINDU_DATE_PRECEDENCE;
}

/**
 * Return the Julian day number of day before the first day of the
 * given month in the given extended year.  Subclasses should override
 * this method to implement their calendar system.
 * @param eyear the extended year
 * @param month the zero-based month, or 0 if useMonth is false
 * @param useMonth if false, compute the day before the first day of
 * the given year, otherwise, compute the day before the first day of
 * the given month
 * @param status Output param set to failure code on function return
 *          when this function fails.
 * @return the Julian day number of the day before the first
 * day of the given month and year
 * @stable ICU 2.8
 */
int64_t HinduLunarCalendar::handleComputeMonthStart(int32_t eyear, int32_t month, UBool useMonth, UErrorCode& status) const {
    // rdar://145903949 Rewrite and simplify month start calculation
    // Instead of using internal fields, we can get all information calculating julian dates by hand
    // If the month is out of range, adjust it into range, and
    // modify the extended year value accordingly.
    // rdar://156472909 ([New Calendars]: GJ: Luck23A304: Incorrect date is marked as a new year in Gujarati alternative calendar)
    month -= getMonthOffset();
    if (month < 0 || month > 11) {
        double m = month;
        eyear += (int32_t)ClockMath::floorDivide(m, 12.0, &m);
        month = (int32_t)m;
    }
    // rdar://151856444 Calculate year with correct offset for lunisolar calendars
    int32_t gyear = eyear - getYearOffset();
    int32_t date = hinduLunarNewYear(gyear);
    double lastNewMoon = date;
    
    if (useMonth) {
        // if useMonth is set, we need to find start of the month
        // as Hindu lunar calendar can have both extra month and skipped month, we need to move by one until we find it.
        int currMonth = 0;
        date += AMANTA_OFFSET;
                
        while (1) {
            double critical = CalendarAstronomer::hinduSunrise(date, ujjainOffset(), UJJAIN.latitude.degrees);
            lastNewMoon = CalendarAstronomer::hinduNewMoonBefore(critical);
            int32_t solarMonth = CalendarAstronomer::hinduZodiac(lastNewMoon);
            int32_t calculatedMonth = (solarMonth + 1) % 12;
            if (calculatedMonth >= month) {
                break;
            }
            date = lastNewMoon + 45;
        }
    }

    return lastNewMoon + RD_OFFSET;
}

int32_t HinduLunarCalendar::handleComputeOrdinalDay(int32_t monthStart) {
    UErrorCode status = U_ZERO_ERROR;
    
    // TODO: This method only calculates the dom within the month range
    // expand this to cover overflowing days, too
    if (internalGet(UCAL_DAY_OF_MONTH, status) < 0 || internalGet(UCAL_DAY_OF_MONTH, status) > 30 ||
        internalGet(UCAL_MONTH, status) < 0 || internalGet(UCAL_MONTH, status) > 11) {
        return internalGet(UCAL_DAY_OF_MONTH, status);
    }
    
    int32_t dom = internalGet(UCAL_DAY_OF_MONTH, status);
    int32_t month = internalGet(UCAL_MONTH, status);
    int32_t leapMonth = internalGet(UCAL_IS_LEAP_MONTH, status);
    int32_t year = internalGet(UCAL_YEAR, status);
    int32_t rd = internalGet(UCAL_IS_REPEATED_DAY, status);
    int32_t jd = monthStart + dom;
    int32_t rdOffset = 0;
    int32_t leapMonthOffset = 0;
    HinduDate hinduDate = hinduLunarFromFixed(jd);
    
    if (dom < 0 || month < 0 || year < 0) {
        return dom;
    }

    // rdar://151149828 For leap months we need to match UCAL_IS_LEAP_MONTH property
    while (leapMonth != hinduDate.leapMonth) {
        hinduDate = hinduLunarFromFixed(++jd);
        leapMonthOffset++;
    }
    
    if (year == hinduDate.year && month == hinduDate.month && dom == hinduDate.day) {
        if (rd == hinduDate.leapDay) {
            return leapMonthOffset + dom;
        }
        if (rd == 1 && hinduDate.leapDay == 0) {
            // target JD is not leapDay, while internals are expecting it to be
            // check out if the next day is the leap day
            // if true, return that day
            // if false that means UCAL_IS_REPEATED_DAY is set on non-repeated day, just return non-repeated day
            hinduDate = hinduLunarFromFixed(jd + 1);
            return dom == hinduDate.day && hinduDate.leapDay == 1 ? dom + 1 : dom;
        }
        else {
            // the only remaining posibility is rd == 0 && hinduDate.leapDay == 1
            // target JD is a leapDay, while internals are expecting it not to be
            // check out if the previous day is a non-leap day
            // this check should be trivial, as the presence of a leap day should always indicate non-leap same day a day before
            hinduDate = hinduLunarFromFixed(jd - 1);
            return dom == hinduDate.day && hinduDate.leapDay == 0 ? dom - 1 : dom;
        }
    }
    else if (hinduDate.day > dom) {
        // Year override happens when a repeated day is close to the end of the year
        while (hinduDate.year < year) {
            hinduDate = hinduLunarFromFixed(++jd);
        }
        if (hinduDate.month == month) {
            // calculated day is bigger than dom
            // this happened when we have a skipped day
            while (hinduDate.day > dom) {
                hinduDate = hinduLunarFromFixed(--jd);
            }
        }
        else if (hinduDate.month < month) {
            // month override happens when a repeated day is close to the end of a month
            // hinduDate.day = 30/29 hinduDate.month = x while dom = 1/2, month = x + 1
            while (hinduDate.month < month) {
                hinduDate = hinduLunarFromFixed(++jd);
            }
            // rdar://156712820 Match leap month property
            while (leapMonth != hinduDate.leapMonth) {
                hinduDate = hinduLunarFromFixed(++jd);
                leapMonthOffset++;
            }
            while (hinduDate.day < dom) {
                hinduDate = hinduLunarFromFixed(++jd);
            }
        }
        else {
            while (hinduDate.day < dom) {
                hinduDate = hinduLunarFromFixed(++jd);
            }
            while (hinduDate.day > dom) {
                hinduDate = hinduLunarFromFixed(--jd);
            }
        }
    }
    else if (hinduDate.day < dom) {
        // Year override happens when a repeated day is close to the end of the year
        while (hinduDate.year > year) {
            hinduDate = hinduLunarFromFixed(--jd);
        }
        if (hinduDate.month == month) {
            // calculated day is smaller than dom
            // this happened when we have a repeated day
            while (hinduDate.day < dom) {
                hinduDate = hinduLunarFromFixed(++jd);
            }
            if (rd == 1 && hinduDate.leapDay == 0) {
                // target JD is not leapDay, while internals are expecting it to be
                // check out if the next day is the leap day
                // if true, return that day
                // if false that means UCAL_IS_REPEATED_DAY is set on non-repeated day, just return non-repeated day
                hinduDate = hinduLunarFromFixed(jd + 1);
                rdOffset = dom == hinduDate.day && hinduDate.leapDay == 1 ? 1 : 0;
            }
        }
        else if (hinduDate.month > month) {
            // month override happens when a repeated day
            // hinduDate.day = 1/2 hinduDate.month = x while dom = 29/30, month = x - 1
            while (hinduDate.month > month) {
                hinduDate = hinduLunarFromFixed(--jd);
            }
            while (hinduDate.day > dom) {
                hinduDate = hinduLunarFromFixed(--jd);
            }
        }
        else {
            while (hinduDate.day < dom) {
                hinduDate = hinduLunarFromFixed(++jd);
            }
            while (hinduDate.day > dom) {
                hinduDate = hinduLunarFromFixed(--jd);
            }
        }
    }
    
    return jd - monthStart + rdOffset;
}

/**
 * Override Calendar to handle leap months properly.
 * @stable ICU 2.8
 */
void HinduLunarCalendar::add(UCalendarDateFields field, int32_t amount, UErrorCode& status) {
    switch (field) {
    case UCAL_MONTH:
    case UCAL_ORDINAL_MONTH:
        if (amount != 0) {
            // rdar://156473289 Update Julian day for Amanta calendars when adding months
            if (    getHinduCalendarType() == AMANTA ||
                    get(UCAL_YEAR, status) < HINDU_MONTH_START_TABLE_LOW ||
                    (get(UCAL_YEAR, status) + (get(UCAL_MONTH, status) + amount) / 12 > HINDU_MONTH_START_TABLE_HIGH)) {
                int32_t day = get(UCAL_JULIAN_DAY, status) + amount * HINDU_SYNODIC_MONTH;
                if (U_FAILURE(status)) break;
                handleComputeFields(day, status);
                
                set(UCAL_JULIAN_DAY, day);
                complete(status);
            }
            else {
                // rdar://151415578
                // Use precalculate months start for period of 1900-2100 (Gregorian calendar) or 1957-2156 Vikram calendar
                int32_t dom = get(UCAL_DAY_OF_MONTH, status);
                int32_t targetMonthStart = CalendarAstronomer::getHinduMonthStart(get(UCAL_YEAR, status), get(UCAL_MONTH, status) + amount);
                int32_t targetMonthLength = CalendarAstronomer::getHinduMonthStart(get(UCAL_YEAR, status), get(UCAL_MONTH, status) + amount + 1) - targetMonthStart;
                if (dom > targetMonthLength) {
                    // this really only happens if dom is 30 and the target month doesn't have day 30 (skipped)
                    dom = targetMonthLength;
                }
                
                set(UCAL_JULIAN_DAY, targetMonthStart + dom);
                complete(status);
            }
        }
        break;
    default:
        Calendar::add(field, amount, status);
        break;
    }
}

/**
 * Override Calendar to handle leap months properly.
 * @stable ICU 2.8
 */
void HinduLunarCalendar::add(EDateFields field, int32_t amount, UErrorCode& status) {
     add((UCalendarDateFields)field, amount, status);
}

/**
 * Override Calendar to handle leap months properly.
 * @stable ICU 2.8
 */
void HinduLunarCalendar::roll(UCalendarDateFields field, int32_t amount, UErrorCode& status) {
    switch (field) {
    case UCAL_MONTH:
    case UCAL_ORDINAL_MONTH:
        if (amount != 0) {
            int32_t dom = get(UCAL_DAY_OF_MONTH, status);
            if (U_FAILURE(status)) break;
            int32_t day = get(UCAL_JULIAN_DAY, status); // Get local day
            if (U_FAILURE(status)) break;
            int32_t moon = day - dom + 1; // New moon (start of this month)

            // Note throughout the following:  Months 12 and 1 are never
            // followed by a leap month (D&R p. 185).

            // Compute the adjusted month number m.  This is zero-based
            // value from 0..11 in a non-leap year, and from 0..12 in a
            // leap year.
            int32_t m = get(UCAL_MONTH, status); // 0-based month
            if (U_FAILURE(status)) break;
            if (isLeapYear) { // (member variable)
                if (get(UCAL_IS_LEAP_MONTH, status) == 1) {
                    ++m;
                } else {
                    // Check for a prior leap month.  (In the
                    // following, month 0 is the first month of the
                    // year.)  Month 0 is never followed by a leap
                    // month, and we know month m is not a leap month.
                    // moon1 will be the start of month 0 if there is
                    // no leap month between month 0 and month m;
                    // otherwise it will be the start of month 1.
                    int32_t moon1 = moon -
                        (int32_t) (CalendarAstronomer::SYNODIC_MONTH * (m - 0.5));
                    moon1 = CalendarAstronomer::hinduNewMoonBefore(moon1);
                    if (isLeapMonthBetween(moon1, moon)) {
                        ++m;
                    }
                }
                if (U_FAILURE(status)) break;
            }

            // Now do the standard roll computation on m, with the
            // allowed range of 0..n-1, where n is 12 or 13.
            int32_t n = isLeapYear ? 13 : 12; // Months in this year
            int32_t newM = (m + amount) % n;
            if (newM < 0) {
                newM += n;
            }
            // (newM - m) is delta for which we need to roll our date, which can be also negative
            // for example if we're rolling a month by 10 on a date 12/5/2081, it would be either 9/5/2081 or 10/5/2081 depending if there is a leap year
            int32_t targetDay = day + (newM - m) * HINDU_SYNODIC_MONTH;
            handleComputeFields(targetDay, status);
        }
        break;
    default:
        Calendar::roll(field, amount, status);
        break;
    }
}

void HinduLunarCalendar::roll(EDateFields field, int32_t amount, UErrorCode& status) {
     roll((UCalendarDateFields)field, amount, status);
}

int32_t HinduLunarCalendar::getYearOffset() const {
    return VIKRAM_OFFSET;
}

int32_t HinduLunarCalendar::getMonthOffset() const {
    return 0;
}

EHinduCalendarType HinduLunarCalendar::getHinduCalendarType() const {
    return AMANTA;
}

//------------------------------------------------------------------
// Time to fields
//------------------------------------------------------------------

/**
 * Return true if there is a leap month on or after month newMoon1 and
 * at or before month newMoon2.
 * @param newMoon1 days after January 1, 1970 0:00 astronomical base zone
 * of a new moon
 * @param newMoon2 days after January 1, 1970 0:00 astronomical base zone
 * of a new moon
 */
UBool HinduLunarCalendar::isLeapMonthBetween(int32_t newMoon1, int32_t newMoon2) const {

    return (newMoon2 >= newMoon1) &&
        (isLeapMonthBetween(newMoon1, CalendarAstronomer::hinduNewMoonBefore(newMoon2 - HINDU_SYNODIC_GAP)));
}

//------------------------------------------------------------------
// Fields to time
//------------------------------------------------------------------

UBool
HinduLunarCalendar::inDaylightTime(UErrorCode& status) const
{
    // copied from GregorianCalendar
    if (U_FAILURE(status) || !getTimeZone().useDaylightTime())
        return false;

    // Force an update of the state of the Calendar.
    ((HinduLunarCalendar*)this)->complete(status); // cast away const

    return (UBool)(U_SUCCESS(status) ? (internalGet(UCAL_DST_OFFSET) != 0) : false);
}

// default century

static UDate     gSystemDefaultCenturyStart       = DBL_MIN;
static int32_t   gSystemDefaultCenturyStartYear   = -1;
static icu::UInitOnce gSystemDefaultCenturyInitOnce {};


UBool HinduLunarCalendar::haveDefaultCentury() const
{
    return true;
}

UDate HinduLunarCalendar::defaultCenturyStart() const
{
    return internalGetDefaultCenturyStart();
}

int32_t HinduLunarCalendar::defaultCenturyStartYear() const
{
    return internalGetDefaultCenturyStartYear();
}

static void U_CALLCONV initializeSystemDefaultCentury()
{
    // initialize systemDefaultCentury and systemDefaultCenturyYear based
    // on the current time.  They'll be set to 80 years before
    // the current time.
    UErrorCode status = U_ZERO_ERROR;
    HinduLunarCalendar calendar(Locale("@calendar=hindu"),status);
    if (U_SUCCESS(status)) {
        calendar.setTime(Calendar::getNow(), status);
        calendar.add(UCAL_YEAR, -80, status);
        gSystemDefaultCenturyStart     = calendar.getTime(status);
        gSystemDefaultCenturyStartYear = calendar.get(UCAL_YEAR, status);
    }
    // We have no recourse upon failure unless we want to propagate the failure
    // out.
}

UDate
HinduLunarCalendar::internalGetDefaultCenturyStart() const
{
    // lazy-evaluate systemDefaultCenturyStart
    umtx_initOnce(gSystemDefaultCenturyInitOnce, &initializeSystemDefaultCentury);
    return gSystemDefaultCenturyStart;
}

int32_t
HinduLunarCalendar::internalGetDefaultCenturyStartYear() const
{
    // lazy-evaluate systemDefaultCenturyStartYear
    umtx_initOnce(gSystemDefaultCenturyInitOnce, &initializeSystemDefaultCentury);
    return    gSystemDefaultCenturyStartYear;
}


/*****************************************************************************
 * HinduSolarCalendar
 *****************************************************************************/
HinduSolarCalendar::HinduSolarCalendar(const Locale& aLocale, UErrorCode& success)
    : HinduLunarCalendar(aLocale, success)
{
}

HinduSolarCalendar::~HinduSolarCalendar()
{
}

const char *HinduSolarCalendar::getType() const {
    return "hindu-solar";
}

void HinduSolarCalendar::handleComputeFields(int32_t julianDay, UErrorCode &status) {
    HinduDate hinduDate = hinduSolarFromFixedWithLookup(julianDay);
    // if the lookup tables don't include the requested day, hinduSolarFromFixedWithLookup()
    // will return a HinduDate with a negative month-- we use that as a sentinel value to
    // indicate that the table lookup failed
    if (hinduDate.month < 0) {
        hinduDate = hinduSolarFromFixed(julianDay);
    }
    
    internalSet(UCAL_YEAR, hinduDate.year);
    internalSet(UCAL_EXTENDED_YEAR, hinduDate.year);
    internalSet(UCAL_MONTH, hinduDate.month);
    internalSet(UCAL_ORDINAL_MONTH, hinduDate.month);
    internalSet(UCAL_DAY_OF_MONTH, hinduDate.day);

    int32_t yearStartJulianDay = (int32_t)handleComputeMonthStart(hinduDate.year, 0, false, status);
    internalSet(UCAL_DAY_OF_YEAR, julianDay - yearStartJulianDay);
}
  
int32_t HinduSolarCalendar::handleComputeOrdinalDay(int32_t monthStart) {
    // rdar://153018943
    // Bypassing HinduLunisolarCalendar::handleComputeOrdinalDay
    // hindusolar calendars need to calculate the ordinal day because it is not the same as UCAL_DAY_OF_MONTH due to leap and skipped days
    // solar calendars don't have those, so no need to do that
    // So, we will just default to Calendar's call
    return Calendar::handleComputeOrdinalDay(monthStart);
}

EHinduCalendarType HinduSolarCalendar::getHinduCalendarType() const {
    return SOLAR;
}

int32_t HinduSolarCalendar::handleGetLimit(UCalendarDateFields field, ELimitType limitType) const {
    switch (field) {
        case UCAL_DAY_OF_MONTH:
            switch (limitType) {
                case Calendar::UCAL_LIMIT_LEAST_MAXIMUM:
                    return 29;
                case Calendar::UCAL_LIMIT_MAXIMUM:
                    return 32;
                default:
                    return HinduLunarCalendar::handleGetLimit(field, limitType);
            }
        case UCAL_DAY_OF_YEAR:
            switch (limitType) {
                case Calendar::UCAL_LIMIT_LEAST_MAXIMUM:
                    return 365;
                case Calendar::UCAL_LIMIT_MAXIMUM:
                    return 366;
                default:
                    return HinduLunarCalendar::handleGetLimit(field, limitType);
            }
        default:
            return HinduLunarCalendar::handleGetLimit(field, limitType);
    }
}

int32_t HinduSolarCalendar::handleGetMonthLength(int32_t extendedYear, int32_t month, UErrorCode& status) const {
    // bypass the HinduLunarCalendar behavior and just use the default Calendar behavior
    return Calendar::handleGetMonthLength(extendedYear, month, status);
}

int64_t HinduSolarCalendar::handleComputeMonthStart(int32_t eyear, int32_t month, UBool useMonth, UErrorCode& status) const {
    if (!useMonth) {
        month = 0;
    }
    if (month < 0 || month > 11) {
        double tempMonth = month;
        eyear += (int32_t)ClockMath::floorDivide(tempMonth, 12.0, &tempMonth);
        month = (int32_t)tempMonth;
    }
    
    int32_t lookupTableBaseYear = getLookupTableBaseYear();
    const int32_t* lookupTable = getLookupTable();
    
    if (lookupTable != nullptr && eyear >= lookupTableBaseYear && eyear < lookupTableBaseYear + (getLookupTableLength() / 12)) {
        int32_t yearStartIndex = (eyear - lookupTableBaseYear) * 12;
        int32_t monthStartRD = lookupTable[yearStartIndex + month];
        return monthStartRD + RD_OFFSET;
    } else {
        return doHandleComputeMonthStart(eyear, month, status);
    }
}

HinduDate HinduSolarCalendar::hinduSolarFromFixedWithLookup(int32_t date) const {
    // For values within some distance of the current date, instead of using the Reingold/Dershowitz code,
    // we use a lookup table.  This is both for performance and to make sure we match the month lengths on
    // the Drikpanchang Web site, which doesn't always match Reingold/Dershowitz and which we're taking to
    // be authoritative
    date -= (RD_OFFSET + 1);
    const int32_t* lookupTable = getLookupTable();
    
    if (lookupTable != nullptr && date >= lookupTable[0] && date < lookupTable[getLookupTableLength() - 1]) {
        const int32_t* monthStartPtr = std::lower_bound(lookupTable, lookupTable + getLookupTableLength(), date);
        int32_t monthStartIndex = monthStartPtr - lookupTable;
        monthStartIndex += (*monthStartPtr == date) ? 0 : -1;
        
        int32_t year = getLookupTableBaseYear() + (monthStartIndex / 12);
        int32_t month = monthStartIndex % 12;
        int32_t day = (date - lookupTable[monthStartIndex]) + 1;
        
        return HinduDate{year, month, 0, day, 0};
    } else {
        // sentinel value to say that there either wasn't a lookup table or the date was outside the range it covers
        return HinduDate{0, -999, 0, 0, 0};
    }
}

HinduDate HinduSolarCalendar::hinduSolarFromFixed(int32_t date) const {
    // This code attempts to follow the "hindu-solar-from-fixed" function on pp. 348-349 of "Calendrical Calculations".
    // This algorithm is common to all the Hindu solar calendars; the only way they vary is in what point in time
    // during the day is the point that's compared to the sign of the zodiac to determine whether the day begins
    // a new month-- those calculations are factored out into getDayTransition(), which has separate implementations
    // for each of the solar calendars.
    // Each solar calendar also uses its own year numbering-- this is handled by the getYearOffset() method,
    // which everybody overrides.
    // The Tamil calendar also uses the same algorithm, except that it uses "astronomical" variants of all
    // of the astronomical calculations-- those calculations are factored out into the zodiac(), hinduCalendarYear(),
    // solarLongitude(), and siderealYear() method.  The Odia, Malayalam, and Bangla calendars all inherit the default
    // implementations of these methods, but the Tamil calendar overrides them.
    // The Malayalam calendar has the month names and year starts shifted by four months from the other calendars;
    // it overrides this method to adjust the month, but otherwise also uses this algorithm.
    date -= RD_OFFSET + 1;
    double critical = getDayTransition(date);

    int32_t  month = zodiac(critical);
    int32_t  year = hinduCalendarYear(critical) - HINDU_SOLAR_ERA;
    // Approximate date, 3 days before the start of the mean month
    double approx = date - 3.0 - fmod(floor(solarLongitude(critical)), 30);
    int32_t  start = nextMonthStart(critical, (int)approx);
    int32_t  day = (int)(date - start + 1);
    
    // rdar://147055826 - adjust era
    year = year - getYearOffset();
    
    // rdar://147055826 - month for Malayalam calendar is trailing by four    month = (month > 4) ? month - 4 : month
    // rdar://153144480 (Odia and Bangla calendars have the wrong month names and year starts)
    month += getMonthOffset();
    if (month < 0) {
        month += 12;
        --year;
    } else if (month > 11) {
        month -= 12;
        ++year;
    }
    
    return HinduDate{year, month, 0, day, 0};
}


int HinduSolarCalendar::nextMonthStart(double critical, int approx) const {
    // this function is an internal implementation detail of hinduSolarFromFixed(); none of the subclasses
    // override this to do something different
    int i = approx;
    int month = zodiac(critical);  // The month we're looking for
    while (zodiac(getDayTransition(i)) != month) {
        i++;
    }
    return i;
}

int64_t HinduSolarCalendar::doHandleComputeMonthStart(int32_t eyear, int32_t month, UErrorCode& status) const {
    // rdar://147055826 - month for Malayalam calendar is trailing by four    month = (month > 4) ? month - 4 : month
    // rdar://153144480 (Odia and Bangla calendars have the wrong month names and year starts)
    month -= getMonthOffset();
    if (month < 0) {
        month += 12;
        --eyear;
    } else if (month > 11) {
        month -= 12;
        ++eyear;
    }

    // this code is adapted from "fixed-from-hindu-solar" on p. 348 of "Calendrical Calculations".
    // As with hinduSolarFromFixed() above, all of the solar calendars use this algorithm, with the astronomical
    // stuff factored out into separate methods they DO override.  For details, see the comment in hinduSolarFromFixed().
    eyear += getYearOffset();
    int32_t start = std::floor((eyear + HINDU_SOLAR_ERA + ((double)month / 12)) * siderealYear()) + HINDU_EPOCH_YEAR;
    int32_t monthStartRD = start - 3;
    while (zodiac(getDayTransition(monthStartRD)) != month) {
        ++monthStartRD;
    }
    return monthStartRD + RD_OFFSET;
}

double HinduSolarCalendar::getDayTransition(int32_t date) const {
    // NOTE: This default implementation is here mainly so that the clone() method still compiles-- it
    // doesn't correspond to the actual day-transition time used by any current solar calendar.
    return (double)date;
}

// The implementation of these four methods is shared by the Odia, Bangla, and Malayalam calendars,
// but overridden by the Tamil calendar.  See pp. 248-249 of "Calendrical Calculations".
int32_t HinduSolarCalendar::zodiac(double critical) const {
    return CalendarAstronomer::hinduZodiac(critical);
}

int32_t HinduSolarCalendar::hinduCalendarYear(double critical) const {
    return CalendarAstronomer::hinduCalendarYear(critical);
}

double HinduSolarCalendar::solarLongitude(double critical) const {
    return CalendarAstronomer::hinduSolarLongitude(critical);
}

double HinduSolarCalendar::siderealYear() const {
    return HINDU_SIDEREAL_YEAR;
}

// rdar://151415578
void HinduSolarCalendar::add(UCalendarDateFields field, int32_t amount, UErrorCode& status) {
    // the solar calendars don't have leap days and leap months, so they can use the default
    // add() and roll() methods from Calendar instead of the HinduLunarCalendar versions
    Calendar::add(field, amount, status);
}

void HinduSolarCalendar::add(EDateFields field, int32_t amount, UErrorCode& status) {
    // the solar calendars don't have leap days and leap months, so they can use the default
    // add() and roll() methods from Calendar instead of the HinduLunarCalendar versions
     add((UCalendarDateFields)field, amount, status);
}

void HinduSolarCalendar::roll(UCalendarDateFields field, int32_t amount, UErrorCode& status) {
    // the solar calendars don't have leap days and leap months, so they can use the default
    // add() and roll() methods from Calendar instead of the HinduLunarCalendar versions
    Calendar::roll(field, amount, status);
}

HinduSolarCalendar* HinduSolarCalendar::clone() const {
    return new HinduSolarCalendar(*this);
}

int32_t HinduSolarCalendar::getYearOffset() const {
    return 0;
}

int32_t HinduSolarCalendar::getLookupTableBaseYear() const {
    return 0;
}

const int32_t* HinduSolarCalendar::getLookupTable() const {
    return nullptr;
}

int32_t HinduSolarCalendar::getLookupTableLength() const {
    return 0;
}

/*****************************************************************************
 * Hindu Lunisolar Vikram Calendar
 *****************************************************************************/
HinduLunisolarVikramCalendar::HinduLunisolarVikramCalendar(const Locale& aLocale, UErrorCode& success)
    : HinduLunarCalendar(aLocale, success)
{
}

HinduLunisolarVikramCalendar::~HinduLunisolarVikramCalendar()
{
}

const char *HinduLunisolarVikramCalendar::getType() const {
    return "vikram";
}

HinduLunisolarVikramCalendar* HinduLunisolarVikramCalendar::clone() const {
    return new HinduLunisolarVikramCalendar(*this);
}

int64_t HinduLunisolarVikramCalendar::handleComputeMonthStart(int32_t eyear, int32_t month, UBool useMonth, UErrorCode& status) const {
    if (eyear > 1957 && eyear < 2157) {
        return CalendarAstronomer::getHinduMonthStart(eyear, month);
    }
    
    return HinduLunarCalendar::handleComputeMonthStart(eyear, month, useMonth, status) - AMANTA_OFFSET;
}

int32_t HinduLunisolarVikramCalendar::getYearOffset() const  {
    return VIKRAM_OFFSET;
}

EHinduCalendarType HinduLunisolarVikramCalendar::getHinduCalendarType() const {
    return PURNIMANTA;
}

/*****************************************************************************
 * Hindu Lunisolar Marathi Calendar
 *****************************************************************************/
HinduLunisolarMarathiCalendar::HinduLunisolarMarathiCalendar(const Locale& aLocale, UErrorCode& success)
    : HinduLunarCalendar(aLocale, success)
{
}

HinduLunisolarMarathiCalendar::~HinduLunisolarMarathiCalendar()
{
}

const char *HinduLunisolarMarathiCalendar::getType() const {
    return "marathi";
}

HinduLunisolarMarathiCalendar* HinduLunisolarMarathiCalendar::clone() const {
    return new HinduLunisolarMarathiCalendar(*this);
}

int32_t HinduLunisolarMarathiCalendar::getYearOffset() const  {
    return SAKA_OFFSET;
}

/*****************************************************************************
 * Hindu Lunisolar Telugu Calendar
 *****************************************************************************/
HinduLunisolarTeluguCalendar::HinduLunisolarTeluguCalendar(const Locale& aLocale, UErrorCode& success)
    : HinduLunarCalendar(aLocale, success)
{
}

HinduLunisolarTeluguCalendar::~HinduLunisolarTeluguCalendar()
{
}

const char *HinduLunisolarTeluguCalendar::getType() const {
    return "telugu";
}

HinduLunisolarTeluguCalendar* HinduLunisolarTeluguCalendar::clone() const {
    return new HinduLunisolarTeluguCalendar(*this);
}

int32_t HinduLunisolarTeluguCalendar::getYearOffset() const  {
    return SAKA_OFFSET;
}

/*****************************************************************************
 * Hindu Lunisolar Gujarati Calendar
 *****************************************************************************/
HinduLunisolarGujaratiCalendar::HinduLunisolarGujaratiCalendar(const Locale& aLocale, UErrorCode& success)
    : HinduLunarCalendar(aLocale, success)
{
}

HinduLunisolarGujaratiCalendar::~HinduLunisolarGujaratiCalendar()
{
}

const char *HinduLunisolarGujaratiCalendar::getType() const {
    return "gujarati";
}

HinduLunisolarGujaratiCalendar* HinduLunisolarGujaratiCalendar::clone() const {
    return new HinduLunisolarGujaratiCalendar(*this);
}

int32_t HinduLunisolarGujaratiCalendar::getYearOffset() const  {
    return VIKRAM_OFFSET;
}

int32_t HinduLunisolarGujaratiCalendar::getMonthOffset() const {
    return -7;
}


/*****************************************************************************
 * Hindu Lunisolar Kannada Calendar
 *****************************************************************************/
HinduLunisolarKannadaCalendar::HinduLunisolarKannadaCalendar(const Locale& aLocale, UErrorCode& success)
    : HinduLunarCalendar(aLocale, success)
{
}

HinduLunisolarKannadaCalendar::~HinduLunisolarKannadaCalendar()
{
}

const char *HinduLunisolarKannadaCalendar::getType() const {
    return "kannada";
}

HinduLunisolarKannadaCalendar* HinduLunisolarKannadaCalendar::clone() const {
    return new HinduLunisolarKannadaCalendar(*this);
}

int32_t HinduLunisolarKannadaCalendar::getYearOffset() const  {
    return SAKA_OFFSET;
}

/*****************************************************************************
 * Hindu Solar Bangla Calendar
 *****************************************************************************/
// the first entry in kBanglaMonthStartTableBaseYear below is the RD number for the first day of this year
static const int32_t kBanglaMonthStartTableBaseYear = 1429;

// table of month starts for the Bangla calendar.  See comment on kTamilCalendarMonthStarts for more
// information
static const int32_t kBanglaCalendarMonthStarts[] = {
    738260, 738291, 738322, 738354, 738385, 738416, 738447, 738477, 738506, 738536, 738565, 738595, // 1429 (Gregorian 2022-23)
    738625, 738656, 738688, 738719, 738751, 738782, 738812, 738842, 738872, 738901, 738930, 738960, // 1430 (Gregorian 2023-24)
    738990, 739021, 739053, 739084, 739116, 739147, 739177, 739207, 739237, 739266, 739296, 739325, // 1431 (Gregorian 2024-25)
    739356, 739387, 739418, 739450, 739481, 739512, 739543, 739573, 739602, 739631, 739661, 739691, // 1432 (Gregorian 2025-26)
    739721, 739752, 739783, 739815, 739847, 739878, 739908, 739938, 739967, 739997, 740026, 740056, // 1433 (Gregorian 2026-27)
    740086, 740117, 740149, 740180, 740212, 740243, 740273, 740303, 740333, 740362, 740391, 740421, // 1434 (Gregorian 2027-28)
    740452, 740482, 740514, 740546, 740577, 740608, 740638, 740668, 740698, 740727, 740757, 740786, // 1435 (Gregorian 2028-29)
    740817, 740848, 740879, 740911, 740942, 740973, 741004, 741034, 741063, 741092, 741122, 741152, // 1436 (Gregorian 2029-30)
    741182
};

HinduSolarBanglaCalendar::HinduSolarBanglaCalendar(const Locale& aLocale, UErrorCode& success)
    : HinduSolarCalendar(aLocale, success)
{
}

HinduSolarBanglaCalendar::~HinduSolarBanglaCalendar()
{
}

const char *HinduSolarBanglaCalendar::getType() const {
    return "bangla";
}

HinduSolarBanglaCalendar* HinduSolarBanglaCalendar::clone() const {
    return new HinduSolarBanglaCalendar(*this);
}

double HinduSolarBanglaCalendar::getDayTransition(int32_t date) const {
    // this function attempts to mimic the behavior of "hindu-solar-from-fixed" on pp. 347-348 of
    // "Calendrical Calculations", with the modifications for Bangla described on p. 348 and p. 355.
    // Their text isn't terribly helpful: "According to the Bengal rule, midnight at the start of the
    // day is normally used unless the zodiac sign changes between 11:36 p.m. and 12:24 a.m., in which
    // case various special rules apply."  (Or, even better, on p. 355, where they just say "the Bengal
    // rule is more complicated." and leave it at that.)  Obviously more research is required to
    // figure out what the "various special rules" are, since R&D don't tell us, but since all of the
    // R&D code we've ported so far doesn't match Drik Panchang, it doesn't seem worth doing that
    // research now.  The code below implements the "midnight at the start of the day" part of the
    // description (or I think it does-- I got much better results with date + 0.5 than I did with either
    // date or date + 1, so maybe the raw RD value refers to noon?) and relies on the lookup table
    // and scraping from Drik Panchang to do better.
    // --rtg 6/3/25
    return (double)date + 0.5;
}

int32_t HinduSolarBanglaCalendar::getYearOffset() const {
    return BANGLA_OFFSET;
}

int32_t HinduSolarBanglaCalendar::getLookupTableBaseYear() const {
    return kBanglaMonthStartTableBaseYear;
}

const int32_t* HinduSolarBanglaCalendar::getLookupTable() const {
    return kBanglaCalendarMonthStarts;
}

int32_t HinduSolarBanglaCalendar::getLookupTableLength() const {
    return UPRV_LENGTHOF(kBanglaCalendarMonthStarts);
}

/*****************************************************************************
 * Hindu Solar Malayalam Calendar
 *****************************************************************************/
// the first entry in kMalayalamCalendarMonthStarts below is the RD number for the first day of this year
static const int32_t kMalayalamMonthStartTableBaseYear = 1190;

// table of month starts for the Malayalam calendar.  See comment on kTamilCalendarMonthStarts for more
// information
static const int32_t kMalayalamCalendarMonthStarts[] = {
    735462, 735493, 735524, 735554, 735583, 735613, 735642, 735672, 735703, 735733, 735765, 735796, // 1190 (Gregorian 2014-15)
    735827, 735858, 735889, 735919, 735949, 735978, 736008, 736037, 736068, 736099, 736130, 736161, // 1191 (Gregorian 2015-16)
    736193, 736224, 736254, 736284, 736314, 736343, 736373, 736403, 736433, 736464, 736495, 736527, // 1192 (Gregorian 2016-17)
    736558, 736589, 736619, 736649, 736679, 736709, 736738, 736768, 736798, 736829, 736860, 736892, // 1193 (Gregorian 2017-18)
    736923, 736954, 736985, 737015, 737044, 737074, 737103, 737133, 737164, 737194, 737226, 737257, // 1194 (Gregorian 2018-19)
    737288, 737319, 737350, 737380, 737410, 737439, 737469, 737498, 737529, 737560, 737591, 737622, // 1195 (Gregorian 2019-20)
    737654, 737685, 737715, 737745, 737775, 737804, 737834, 737864, 737894, 737925, 737956, 737988, // 1196 (Gregorian 2020-21)
    738019, 738050, 738081, 738111, 738140, 738170, 738199, 738229, 738259, 738290, 738321, 738353, // 1197 (Gregorian 2021-22)
    738384, 738415, 738446, 738476, 738505, 738535, 738564, 738594, 738625, 738655, 738687, 738718, // 1198 (Gregorian 2022-23)
    738750, 738781, 738811, 738841, 738871, 738900, 738930, 738959, 738990, 739021, 739052, 739083, // 1199 (Gregorian 2023-24)
    739115, 739146, 739176, 739206, 739236, 739265, 739295, 739325, 739355, 739386, 739417, 739449, // 1200 (Gregorian 2024-25)
    739480, 739511, 739542, 739572, 739601, 739631, 739660, 739690, 739720, 739751, 739782, 739814, // 1201 (Gregorian 2025-26)
    739845, 739876, 739907, 739937, 739966, 739996, 740025, 740055, 740086, 740116, 740148, 740179, // 1202 (Gregorian 2026-27)
    740211, 740242, 740272, 740302, 740332, 740361, 740391, 740420, 740451, 740482, 740513, 740544, // 1203 (Gregorian 2027-28)
    740576, 740607, 740637, 740667, 740697, 740726, 740756, 740786, 740816, 740847, 740878, 740910, // 1204 (Gregorian 2028-29)
    740941, 740972, 741003, 741033, 741062, 741092, 741121, 741151, 741181, 741212, 741243, 741275, // 1205 (Gregorian 2029-30)
    741306, 741337, 741368, 741398, 741427, 741457, 741486, 741516, 741547, 741577, 741609, 741640, // 1206 (Gregorian 2030-31)
    741672, 741703, 741733, 741763, 741793, 741822, 741852, 741882, 741912, 741943, 741974, 742005, // 1207 (Gregorian 2031-32)
    742037, 742068, 742098, 742128, 742158, 742187, 742217, 742247, 742277, 742308, 742339, 742371, // 1208 (Gregorian 2032-33)
    742402, 742433, 742464, 742494, 742523, 742553, 742582, 742612, 742642, 742673, 742705, 742736, // 1209 (Gregorian 2033-34)
    742767, 742798, 742829, 742859, 742888, 742918, 742947, 742977, 743008, 743039, 743070, 743101, // 1210 (Gregorian 2034-35)
    743133
};

HinduSolarMalayalamCalendar::HinduSolarMalayalamCalendar(const Locale& aLocale, UErrorCode& success)
    : HinduSolarCalendar(aLocale, success)
{
}

HinduSolarMalayalamCalendar::~HinduSolarMalayalamCalendar()
{
}

const char *HinduSolarMalayalamCalendar::getType() const {
    return "malayalam";
}

HinduSolarMalayalamCalendar* HinduSolarMalayalamCalendar::clone() const {
    return new HinduSolarMalayalamCalendar(*this);
}

static const double kMalayalamDayTransitionTime = (13.0 + (12.0 / 60.0)) / 24.0; // the fraction through the day that 13:12 is

double HinduSolarMalayalamCalendar::getDayTransition(int32_t date) const {
    // this function attempts to mimic the behavior of "hindu-solar-from-fixed" on pp. 347-348 of
    // "Calendrical Calculations", with the modifications for Malayalam described on p. 355 and p. 348.
    // The text there isn't completely clear, they don't have a direct implementation of this
    // calendar in their Lisp code, and our results don't match Drik Panchang (see, for example,
    // the adjustment below for the month number), so we're kind of winging it, and there are a few
    // ad-hoc modifications to the Reingold/Dershowitz code to try to do a better job of matching
    // Drik Panchang
    return CalendarAstronomer::hinduStandardFromSundial(date + kMalayalamDayTransitionTime, ujjainOffset(), UJJAIN.latitude.degrees);
}

int32_t HinduSolarMalayalamCalendar::getYearOffset() const {
    return MALAYALAM_OFFSET;
}

int32_t HinduSolarMalayalamCalendar::getMonthOffset() const {
    // rdar://147055826 - month for Malayalam calendar is trailing by four    month = (month > 4) ? month - 4 : month
    return -4;
}

int32_t HinduSolarMalayalamCalendar::getLookupTableBaseYear() const {
    return kMalayalamMonthStartTableBaseYear;
}

const int32_t* HinduSolarMalayalamCalendar::getLookupTable() const {
    return kMalayalamCalendarMonthStarts;
}

int32_t HinduSolarMalayalamCalendar::getLookupTableLength() const {
    return UPRV_LENGTHOF(kMalayalamCalendarMonthStarts);
}

/*****************************************************************************
 * Hindu Solar Odia Calendar
 *****************************************************************************/
// the first entry in kOdiaCalendarMonthStarts below is the RD number for the first day of this year
static const int32_t kOdiaMonthStartTableBaseYear = 1421;

// table of month starts for the Odia calendar.  See comment on kTamilCalendarMonthStarts for more
// information
static const int32_t kOdiaCalendarMonthStarts[] = {
    735128, 735158, 735188, 735218, 735247, 735277, 735307, 735337, 735368, 735399, 735430, 735462, // 1421 (Gregorian 2013-14)
    735493, 735523, 735553, 735583, 735612, 735642, 735672, 735702, 735733, 735764, 735796, 735827, // 1422 (Gregorian 2014-15)
    735858, 735889, 735919, 735948, 735978, 736007, 736037, 736067, 736098, 736130, 736161, 736192, // 1423 (Gregorian 2015-16)
    736223, 736254, 736284, 736313, 736343, 736372, 736402, 736433, 736464, 736495, 736526, 736558, // 1424 (Gregorian 2016-17)
    736589, 736619, 736649, 736679, 736708, 736738, 736768, 736798, 736829, 736860, 736892, 736923, // 1425 (Gregorian 2017-18)
    736954, 736984, 737014, 737044, 737073, 737103, 737133, 737163, 737194, 737225, 737257, 737288, // 1426 (Gregorian 2018-19)
    737319, 737350, 737380, 737409, 737439, 737468, 737498, 737528, 737559, 737591, 737622, 737653, // 1427 (Gregorian 2019-20)
    737684, 737715, 737745, 737774, 737804, 737833, 737863, 737894, 737925, 737956, 737987, 738019, // 1428 (Gregorian 2020-21)
    738050, 738080, 738110, 738140, 738169, 738199, 738229, 738259, 738290, 738321, 738353, 738384, // 1429 (Gregorian 2021-22)
    738415, 738445, 738475, 738505, 738534, 738564, 738594, 738624, 738655, 738686, 738718, 738749, // 1430 (Gregorian 2022-23)
    738780, 738811, 738841, 738870, 738900, 738929, 738959, 738989, 739020, 739052, 739083, 739114, // 1431 (Gregorian 2023-24)
    739145, 739176, 739206, 739235, 739265, 739294, 739324, 739355, 739386, 739417, 739448, 739480, // 1432 (Gregorian 2024-25)
    739511, 739541, 739571, 739601, 739630, 739660, 739690, 739720, 739751, 739782, 739814, 739845, // 1433 (Gregorian 2025-26)
    739876, 739906, 739936, 739966, 739995, 740025, 740055, 740085, 740116, 740147, 740179, 740210, // 1434 (Gregorian 2026-27)
    740241, 740272, 740302, 740331, 740361, 740390, 740420, 740450, 740481, 740513, 740544, 740575, // 1435 (Gregorian 2027-28)
    740606, 740637, 740667, 740697, 740726, 740756, 740785, 740816, 740847, 740878, 740909, 740941, // 1436 (Gregorian 2028-29)
    740972, 741002, 741032, 741062, 741091, 741121, 741151, 741181, 741212, 741243, 741275, 741306, // 1437 (Gregorian 2029-30)
    741337, 741367, 741397, 741427, 741456, 741486, 741516, 741546, 741577, 741608, 741640, 741671, // 1438 (Gregorian 2030-31)
    741702, 741733, 741763, 741792, 741822, 741851, 741881, 741911, 741942, 741974, 742005, 742036, // 1439 (Gregorian 2031-32)
    742067, 742098, 742128, 742158, 742187, 742217, 742246, 742277, 742308, 742339, 742370, 742402, // 1440 (Gregorian 2032-33)
    742433, 742463, 742493, 742523, 742552, 742582, 742612, 742642, 742673, 742704, 742736, 742767, // 1441 (Gregorian 2033-34)
    742798
};

HinduSolarOdiaCalendar::HinduSolarOdiaCalendar(const Locale& aLocale, UErrorCode& success)
    : HinduSolarCalendar(aLocale, success)
{
}

HinduSolarOdiaCalendar::~HinduSolarOdiaCalendar()
{
}

const char *HinduSolarOdiaCalendar::getType() const {
    return "odia";
}

HinduSolarOdiaCalendar* HinduSolarOdiaCalendar::clone() const {
    return new HinduSolarOdiaCalendar(*this);
}

double HinduSolarOdiaCalendar::getDayTransition(int32_t date) const {
    // this corresponds to the "Orissa rule" in fixed-from-hindu-solar on pp. 348-349 of "Calendrical Calculations"
    return CalendarAstronomer::hinduSunrise(date + 1, ujjainOffset(), UJJAIN.latitude.fractionalDegrees());
}

int32_t HinduSolarOdiaCalendar::getYearOffset() const {
    return ODIA_OFFSET;
}

int32_t HinduSolarOdiaCalendar::getMonthOffset() const {
    return 7;
}

int32_t HinduSolarOdiaCalendar::getLookupTableBaseYear() const {
    return kOdiaMonthStartTableBaseYear;
}

const int32_t* HinduSolarOdiaCalendar::getLookupTable() const {
    return kOdiaCalendarMonthStarts;
}

int32_t HinduSolarOdiaCalendar::getLookupTableLength() const {
    return UPRV_LENGTHOF(kOdiaCalendarMonthStarts);
}

/*****************************************************************************
 * Hindu Solar Tamil Calendar
 *****************************************************************************/
HinduSolarTamilCalendar::HinduSolarTamilCalendar(const Locale& aLocale, UErrorCode& success)
    : HinduSolarCalendar(aLocale, success)
{
}

HinduSolarTamilCalendar::~HinduSolarTamilCalendar()
{
}

const char *HinduSolarTamilCalendar::getType() const {
    return "tamil";
}

HinduSolarTamilCalendar* HinduSolarTamilCalendar::clone() const {
    return new HinduSolarTamilCalendar(*this);
}

// the first entry in kTamilCalendarMonthStarts below is the RD number for the first day of this year
static const int32_t kTamilMonthStartTableBaseYear = 1936;

// table of month starts for the Tamil calendar.  These are RD numbers and they are based
// on month lengths scraped manually from https://www.drikpanchang.com/tamil/tamil-month-panchangam.html.
// The implementation assumes the first entry is the first day of month 1 of kTamilMonthStartTableBaseYear,
// and that the length of the table is a multiple of 12, plus one (for the Tamil solar calendar, we know every
// year has 12 months).  Thus, the range of RD numbers covered by this table runs from first entry in this
// table to one less than the last entry in this table-- we use the Reingold/Dershowitz algorithm
// ("astro-hindu-solar-from-fixed") for dates outside that range.
static const int32_t kTamilCalendarMonthStarts[] = {
    735337, 735368, 735399, 735431, 735462, 735493, 735524, 735554, 735583, 735613, 735642, 735672, // 1936 (Gregorian 2014-15)
    735702, 735733, 735764, 735796, 735827, 735858, 735889, 735919, 735948, 735978, 736007, 736037, // 1937 (Gregorian 2015-16)
    736068, 736098, 736130, 736161, 736192, 736224, 736254, 736284, 736314, 736343, 736373, 736402, // 1938 (Gregorian 2016-17)
    736433, 736464, 736495, 736526, 736558, 736589, 736619, 736649, 736679, 736708, 736738, 736768, // 1939 (Gregorian 2017-18)
    736798, 736829, 736860, 736892, 736923, 736954, 736985, 737015, 737044, 737074, 737103, 737133, // 1940 (Gregorian 2018-19)
    737163, 737194, 737225, 737257, 737288, 737319, 737350, 737380, 737409, 737439, 737468, 737498, // 1941 (Gregorian 2019-20)
    737529, 737559, 737591, 737622, 737654, 737685, 737715, 737745, 737775, 737804, 737834, 737863, // 1942 (Gregorian 2020-21)
    737894, 737925, 737956, 737987, 738019, 738050, 738080, 738110, 738140, 738169, 738199, 738229, // 1943 (Gregorian 2021-22)
    738259, 738290, 738321, 738353, 738384, 738415, 738446, 738476, 738505, 738535, 738564, 738594, // 1944 (Gregorian 2022-23)
    738624, 738655, 738686, 738718, 738749, 738780, 738811, 738841, 738870, 738900, 738929, 738959, // 1945 (Gregorian 2023-24)
    738990, 739020, 739052, 739083, 739115, 739146, 739176, 739206, 739236, 739265, 739295, 739325, // 1946 (Gregorian 2024-25)
    739355, 739386, 739417, 739448, 739480, 739511, 739541, 739571, 739601, 739630, 739660, 739690, // 1947 (Gregorian 2025-26)
    739720, 739751, 739782, 739814, 739845, 739876, 739907, 739937, 739966, 739996, 740025, 740055, // 1948 (Gregorian 2026-26)
    740085, 740116, 740147, 740179, 740210, 740241, 740272, 740302, 740331, 740361, 740390, 740420, // 1949 (Gregorian 2027-28)
    740451, 740481, 740513, 740544, 740576, 740607, 740637, 740667, 740697, 740726, 740756, 740786, // 1950 (Gregorian 2028-29)
    740816, 740847, 740878, 740909, 740941, 740972, 741002, 741032, 741062, 741091, 741121, 741151, // 1951 (Gregorian 2029-30)
    741181, 741212, 741243, 741275, 741306, 741337, 741368, 741398, 741427, 741457, 741486, 741516, // 1952 (Gregorian 2030-31)
    741546, 741577, 741609, 741640, 741671, 741702, 741733, 741763, 741792, 741822, 741851, 741881, // 1953 (Gregorian 2031-32)
    741912, 741943, 741974, 742005, 742037, 742068, 742098, 742128, 742158, 742187, 742217, 742247, // 1954 (Gregorian 2032-33)
    742277, 742308, 742339, 742370, 742402, 742433, 742463, 742493, 742523, 742552, 742582, 742612, // 1955 (Gregorian 2033-34)
    742642, 742673, 742704, 742736, 742767, 742798, 742829, 742859, 742888, 742918, 742947, 742977, // 1956 (Gregorian 2034-35)
    743007
};

// There are two main versions of the Tamil calendar: Thiru Ganita and Vakyam.  Our implementation uses Thiru Ganita,
// which uses the "astronomical" variants of various astronomical functions instead of the standard versions-- these
// overrides make that change.
// The Vakyam calendar uses the same astronomical functions as the other solar calendars (i.e., it doesn't need to
// override zodiac(), hinduCalendarYear(), solarLongitude(), and siderealYear()), and would override getDayTransition()
// to call CalendarAstronomer::hinduSunset() instead of astroHinduSunset()
double HinduSolarTamilCalendar::getDayTransition(int32_t date) const {
    // The Tamil calendar uses sunset as the day-transition time (see p. 348 and pp. 354-355 of "Calendrical Calculations").
    return CalendarAstronomer::astroHinduSunset(date, UJJAIN.longitude.fractionalDegrees(), UJJAIN.latitude.fractionalDegrees(), UJJAIN.hours);}

int32_t HinduSolarTamilCalendar::zodiac(double critical) const {
    return CalendarAstronomer::siderealZodiac(critical);
}

int32_t HinduSolarTamilCalendar::hinduCalendarYear(double critical) const {
    return CalendarAstronomer::astroHinduCalendarYear(critical);
}

double HinduSolarTamilCalendar::solarLongitude(double critical) const {
    return CalendarAstronomer::siderealSolarLongitude(critical);
}

double HinduSolarTamilCalendar::siderealYear() const {
    return MEAN_SIDEREAL_YEAR;
}

int32_t HinduSolarTamilCalendar::getYearOffset() const {
    return TAMIL_OFFSET;
}

int32_t HinduSolarTamilCalendar::getLookupTableBaseYear() const {
    return kTamilMonthStartTableBaseYear;
}

const int32_t* HinduSolarTamilCalendar::getLookupTable() const {
    return kTamilCalendarMonthStarts;
}

int32_t HinduSolarTamilCalendar::getLookupTableLength() const {
    return UPRV_LENGTHOF(kTamilCalendarMonthStarts);
}


UOBJECT_DEFINE_RTTI_IMPLEMENTATION(HinduLunarCalendar)
UOBJECT_DEFINE_RTTI_IMPLEMENTATION(HinduSolarCalendar)
UOBJECT_DEFINE_RTTI_IMPLEMENTATION(HinduLunisolarVikramCalendar)
UOBJECT_DEFINE_RTTI_IMPLEMENTATION(HinduLunisolarMarathiCalendar)
UOBJECT_DEFINE_RTTI_IMPLEMENTATION(HinduLunisolarTeluguCalendar)
UOBJECT_DEFINE_RTTI_IMPLEMENTATION(HinduLunisolarGujaratiCalendar)
UOBJECT_DEFINE_RTTI_IMPLEMENTATION(HinduLunisolarKannadaCalendar)
UOBJECT_DEFINE_RTTI_IMPLEMENTATION(HinduSolarBanglaCalendar)
UOBJECT_DEFINE_RTTI_IMPLEMENTATION(HinduSolarMalayalamCalendar)
UOBJECT_DEFINE_RTTI_IMPLEMENTATION(HinduSolarTamilCalendar)
UOBJECT_DEFINE_RTTI_IMPLEMENTATION(HinduSolarOdiaCalendar)

U_NAMESPACE_END

#endif
