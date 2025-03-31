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
#include "unicode/simpletz.h"
#include "uhash.h"
#include "ucln_in.h"

// --- The cache --
static icu::UMutex astroLock;
static icu::CalendarAstronomer *gHinduCalendarAstro = NULL;

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
    5 + 461.0 / 9000.0
};

U_CDECL_BEGIN
static UBool calendar_hindu_cleanup(void) {
    if (gHinduCalendarAstro) {
        delete gHinduCalendarAstro;
        gHinduCalendarAstro = NULL;
    }
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

UBool HinduLunarCalendar::isLeapDay() const {
    return leapDay;
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
    date -= RD_OFFSET;
    double critical = gHinduCalendarAstro->hinduSunrise(date, ujjainOffset(), UJJAIN.latitude.degrees);
    int32_t day = gHinduCalendarAstro->hinduLunarDayFromMoment(critical);
    bool leapDay = (day == gHinduCalendarAstro->hinduLunarDayFromMoment(
                    gHinduCalendarAstro->hinduSunrise(date - 1, ujjainOffset(), UJJAIN.latitude.degrees)));

    double lastNewMoon = gHinduCalendarAstro->hinduNewMoonBefore(critical);
    double nextNewMoon = gHinduCalendarAstro->hinduNewMoonBefore(floor(lastNewMoon) + 35);
    int32_t solarMonth = gHinduCalendarAstro->hinduZodiac(lastNewMoon);
    bool leapMonth = (solarMonth == gHinduCalendarAstro->hinduZodiac(nextNewMoon));
    int32_t month = (solarMonth + 1) % 12;
    if (month == 0) month = 12;
    int32_t year = gHinduCalendarAstro->hinduCalendarYear((month <= 2) ? date + 180 : date) - HINDU_LUNAR_ERA;
    
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

    double approx = HINDU_SIDEREAL_YEAR * (year + HINDU_LUNAR_ERA + (month - 1) / 12);
    int32_t s = floor(approx - HINDU_SIDEREAL_YEAR * mod3((gHinduCalendarAstro->hinduSolarLongitude(approx) / 360.0) - ((month - 1) / 12.0), -0.5, 0.5));

    int32_t k = gHinduCalendarAstro->hinduLunarDayFromMoment(s + gHinduCalendarAstro->hr(6));
    int32_t est = s - day + ((k < 3 || k >= 27) ? k : (((hinduLunarFromFixed(s - 15)).month != month || (leapMonth && !(hinduLunarFromFixed(s - 15)).month)) ? mod3(k, -15, 15) : mod3(k, 15, 45)));
    int32_t tau = est - mod3(gHinduCalendarAstro->hinduLunarDayFromMoment(est + gHinduCalendarAstro->hr(6)) - day, -15, 15);
    
    int32_t date = day;
    
    for (int32_t i = tau - 1; i <= 30; ++i) {
        if (gHinduCalendarAstro->hinduLunarDayFromMoment(gHinduCalendarAstro->hinduSunrise(i, ujjainOffset(), UJJAIN.latitude.degrees)) == day
            || gHinduCalendarAstro->hinduLunarDayFromMoment(gHinduCalendarAstro->hinduSunrise(i, ujjainOffset(), UJJAIN.latitude.degrees)) == amod(day + 1, 30)) {
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
    double mina = gHinduCalendarAstro->hinduSolarLongitudeAtOrAfter(330.0, jan1);
    double newMoon = gHinduCalendarAstro->hinduLunarDayAtOrAfter(1.0, mina);
    int32_t hDay = static_cast<int32_t>(floor(newMoon));
    double critical = gHinduCalendarAstro->hinduSunrise(hDay, ujjainOffset(), UJJAIN.latitude.degrees);

    return hDay + ((newMoon < critical)
                   || (gHinduCalendarAstro->hinduLunarDayFromMoment(
                            gHinduCalendarAstro->hinduSunrise(hDay + 1, ujjainOffset(), UJJAIN.latitude.degrees)) == 2) ?
                            0 : 1);
}
    
// Calendar parts

int32_t HinduLunarCalendar::gregorianNewYear(int32_t gYear) const {
	return gYear + HINDU_EPOCH_YEAR; 
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
};


/**
* @draft ICU 2.4
*/
int32_t HinduLunarCalendar::handleGetLimit(UCalendarDateFields field, ELimitType limitType) const {
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
    int32_t year;
    if (newestStamp(UCAL_ERA, UCAL_YEAR, kUnset) <= fStamp[UCAL_EXTENDED_YEAR]) {
        year = internalGet(UCAL_EXTENDED_YEAR, 1); // Default to year 1
    } else {
        int32_t cycle = internalGet(UCAL_ERA, 1) - 1; // 0-based cycle
        // adjust to the instance specific epoch
        year = cycle * 60 + internalGet(UCAL_YEAR, 1) - (fEpochYear - HINDU_EPOCH_YEAR);
    }
    return year;
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
    int32_t thisStart = handleComputeMonthStart(extendedYear, month, true, status) -
        kEpochStartAsJulianDay + 1; // Julian day -> local days
    int32_t nextStart = gHinduCalendarAstro->hinduNewMoonBefore(thisStart + SYNODIC_GAP);
    return nextStart - thisStart;
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
void HinduLunarCalendar::handleComputeFields(int32_t julianDay, UErrorCode &/*status*/) {
	HinduDate hinduDate = hinduLunarFromFixed(julianDay);
	
	internalSet(UCAL_YEAR, hinduDate.year);
	internalSet(UCAL_MONTH, hinduDate.month);
	internalSet(UCAL_IS_LEAP_MONTH, hinduDate.leapMonth ? 1 : 0);
	internalSet(UCAL_DAY_OF_MONTH, hinduDate.day);
    leapDay = hinduDate.leapDay ? true : false;
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
 * given month in the given extended year.
 *
 * <p>Note: This method reads the IS_LEAP_MONTH field to determine
 * whether the given month is a leap month.
 * @param eyear the extended year
 * @param month the zero-based month.  The month is also determined
 * by reading the IS_LEAP_MONTH field.
 * @return the Julian day number of the day before the first
 * day of the given month and year
 * @stable ICU 2.8
 */
int64_t HinduLunarCalendar::handleComputeMonthStart(int32_t eyear, int32_t month, UBool useMonth, UErrorCode& status) const {

    HinduLunarCalendar *nonConstThis = (HinduLunarCalendar*)this; // cast away const

    // If the month is out of range, adjust it into range, and
    // modify the extended year value accordingly.
    if (month < 0 || month > 11) {
        double m = month;
        eyear += (int32_t)ClockMath::floorDivide(m, 12.0, &m);
        month = (int32_t)m;
    }

    int32_t gyear = eyear + fEpochYear - 1; // Gregorian year
    int32_t theNewYear = hinduLunarNewYear(gyear);
    int32_t newMoon = gHinduCalendarAstro->hinduNewMoonBefore(theNewYear + month * 29);

    int32_t julianDay = newMoon + kEpochStartAsJulianDay;

    // Save fields for later restoration
    int32_t saveMonth = internalGet(UCAL_MONTH);
    int32_t saveIsLeapMonth = internalGet(UCAL_IS_LEAP_MONTH);

    // Ignore IS_LEAP_MONTH field if useMonth is false
    int32_t isLeapMonth = useMonth ? saveIsLeapMonth : 0;

    status = U_ZERO_ERROR;
    nonConstThis->computeGregorianFields(julianDay, status);
    if (U_FAILURE(status))
        return 0;

    if (month != internalGet(UCAL_MONTH) ||
        isLeapMonth != internalGet(UCAL_IS_LEAP_MONTH)) {
        newMoon = gHinduCalendarAstro->hinduNewMoonBefore(newMoon + SYNODIC_GAP);
        julianDay = newMoon + kEpochStartAsJulianDay;
    }

    nonConstThis->internalSet(UCAL_MONTH, saveMonth);
    nonConstThis->internalSet(UCAL_IS_LEAP_MONTH, saveIsLeapMonth);

    return julianDay - 1;

	return 0;
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
            int32_t day = get(UCAL_JULIAN_DAY, status) + amount * HINDU_SYNODIC_MONTH;
            if (U_FAILURE(status)) break;
            handleComputeFields(day, status);
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
                    moon1 = gHinduCalendarAstro->hinduNewMoonBefore(moon1);
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
        (isLeapMonthBetween(newMoon1, gHinduCalendarAstro->hinduNewMoonBefore(newMoon2 - SYNODIC_GAP)));
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

void HinduSolarCalendar::handleComputeFields(int32_t julianDay, UErrorCode &/*status*/) {
    HinduDate hinduDate = hinduSolarFromFixed(julianDay);
    
    internalSet(UCAL_YEAR, hinduDate.year);
    internalSet(UCAL_MONTH, hinduDate.month);
    internalSet(UCAL_DAY_OF_MONTH, hinduDate.day);
}

// Function to compute the Hindu (Orissa) solar date equivalent to a fixed date
HinduDate HinduSolarCalendar::hinduSolarFromFixed(int32_t date) const {
    date -= RD_OFFSET;
    double critical = gHinduCalendarAstro->hinduSunrise(date + 1, ujjainOffset(), UJJAIN.latitude.degrees);

    int32_t  month = gHinduCalendarAstro->hinduZodiac(critical);
    int32_t  year = gHinduCalendarAstro->hinduCalendarYear(critical) - HINDU_SOLAR_ERA;
    // Approximate date, 3 days before the start of the mean month
    double approx = date - 3.0 - fmod(floor(gHinduCalendarAstro->hinduSolarLongitude(critical)), 30);
    int32_t  start = nextMonthStart(critical, (int)approx);
    int32_t  day = (int)(date - start + 1);
    
    return HinduDate{year, month, 0, day, 0};
}


int HinduSolarCalendar::nextMonthStart(double critical, int approx) const {
    int i = approx;
    int month = gHinduCalendarAstro->hinduZodiac(critical);  // The month we're looking for
    while (gHinduCalendarAstro->hinduZodiac(gHinduCalendarAstro->hinduSunrise(i + 1, ujjainOffset(), UJJAIN.latitude.degrees)) != month) {
        i++;
    }
    return i;
}

HinduSolarCalendar* HinduSolarCalendar::clone() const {
    return new HinduSolarCalendar(*this);
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

/*****************************************************************************
 * Hindu Solar Bangla Calendar
 *****************************************************************************/
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

/*****************************************************************************
 * Hindu Solar Malayalam Calendar
 *****************************************************************************/
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

/*****************************************************************************
 * Hindu Solar Odia Calendar
 *****************************************************************************/
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
