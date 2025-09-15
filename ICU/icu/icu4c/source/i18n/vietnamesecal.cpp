/*
*****************************************************************************************
* Copyright (C) 2025 Apple Inc. All Rights Reserved.
*****************************************************************************************
*/

#include "vietnamesecal.h"

#if APPLE_ICU_CHANGES // rdar://24832944

#if !UCONFIG_NO_FORMATTING

#include "astro.h" // CalendarCache
#include "gregoimp.h" // Math
#include "uassert.h"
#include "ucln_in.h"
#include "umutex.h"
#include "unicode/rbtz.h"
#include "unicode/tzrule.h"
#include "unicode/simpletz.h"

// --- The cache --
// Lazy Creation & Access synchronized by class CalendarCache with a mutex.
static icu::CalendarCache *gWinterSolsticeCache = nullptr;
static icu::CalendarCache *gNewYearCache = nullptr;

// gAstronomerTimeZone
static icu::TimeZone *gAstronomerTimeZone = nullptr;
static icu::UInitOnce gAstronomerTimeZoneInitOnce {};

// same as for Chinese calendar (see also kVietnameseRelatedYearDiff below)
static const int32_t VIETNAMESE_EPOCH_YEAR = -2636; // Gregorian year

U_CDECL_BEGIN
static UBool calendar_vietnamese_cleanup() {
    if (gWinterSolsticeCache) {
        delete gWinterSolsticeCache;
        gWinterSolsticeCache = nullptr;
    }
    if (gNewYearCache) {
        delete gNewYearCache;
        gNewYearCache = nullptr;
    }

    if (gAstronomerTimeZone) {
        delete gAstronomerTimeZone;
        gAstronomerTimeZone = nullptr;
    }
    gAstronomerTimeZoneInitOnce.reset();
    return true;
}
U_CDECL_END

U_NAMESPACE_BEGIN

// Implementation of the VietnameseCalendar class

//-------------------------------------------------------------------------
// Constructors...
//-------------------------------------------------------------------------

static const TimeZone* getVietnameseAstronomerTimeZone(UErrorCode &status);

VietnameseCalendar::VietnameseCalendar(const Locale& aLocale, UErrorCode& success)
:   ChineseCalendar(aLocale, success)
{
}

VietnameseCalendar::VietnameseCalendar (const VietnameseCalendar& other) 
: ChineseCalendar(other)
{
}

VietnameseCalendar::~VietnameseCalendar()
{
}

VietnameseCalendar*
VietnameseCalendar::clone() const
{
    return new VietnameseCalendar(*this);
}

const char *VietnameseCalendar::getType() const { 
    return "vietnamese";
}

/**
 * The time zone used for performing astronomical computations for the
 * Vietnamese calendar.
 * 
 * Per "Calendrical Calculations" by Reingold & Dershowitz:
 * "The traditional Vietnamese calendar used today is the
 * Chinese calendar computed for Hanoi
 * (Vietnam Standard Time, U.T.  + 8 before 1968, U.T.  + 7 since  1968)"
 */
static void U_CALLCONV initAstronomerTimeZone(UErrorCode &status) {
    U_ASSERT(gAstronomerTimeZone == nullptr);
    const UDate millis1968[] = { static_cast<UDate>((1968 - 1970) * 365 * kOneDay) }; // some days of error is not a problem here

    LocalPointer<InitialTimeZoneRule> initialTimeZone(new InitialTimeZoneRule(
        UnicodeString(u"GMT+8"), 8*kOneHour, 0), status);

    LocalPointer<TimeZoneRule> ruleFrom1968(new TimeArrayTimeZoneRule(
        UnicodeString(u"Vietnam 1968-"), 7*kOneHour, 0, millis1968, 1, DateTimeRule::STANDARD_TIME), status);

    LocalPointer<RuleBasedTimeZone> zone(new RuleBasedTimeZone(
        UnicodeString(u"VIETNAM_ZONE"), initialTimeZone.orphan()), status); // adopts initialTimeZone

    if (U_FAILURE(status)) {
        return;
    }
    
    zone->addTransitionRule(ruleFrom1968.orphan(), status);

    zone->complete(status);

    if (U_SUCCESS(status)) {
        gAstronomerTimeZone = zone.orphan();
    }

    ucln_i18n_registerCleanup(UCLN_I18N_VIETNAMESE_CALENDAR, calendar_vietnamese_cleanup);

}

static const TimeZone* getVietnameseAstronomerTimeZone(UErrorCode &status) {
    umtx_initOnce(gAstronomerTimeZoneInitOnce, &initAstronomerTimeZone, status);
    return gAstronomerTimeZone;
}

// same as for Chinese calendar
constexpr uint32_t kVietnameseRelatedYearDiff = -2637;

int32_t VietnameseCalendar::getRelatedYear(UErrorCode &status) const
{
    int32_t year = get(UCAL_EXTENDED_YEAR, status);
    if (U_FAILURE(status)) {
        return 0;
    }
    if (uprv_add32_overflow(year, kVietnameseRelatedYearDiff, &year)) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    return year;
}

void VietnameseCalendar::setRelatedYear(int32_t year)
{
    // set extended year
    set(UCAL_EXTENDED_YEAR, year - kVietnameseRelatedYearDiff);
}

ChineseCalendar::Setting VietnameseCalendar::getSetting(UErrorCode& status) const {
  return { VIETNAMESE_EPOCH_YEAR,
    getVietnameseAstronomerTimeZone(status),
    &gWinterSolsticeCache, &gNewYearCache
  };
}

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(VietnameseCalendar)

U_NAMESPACE_END

#endif

#endif // APPLE_ICU_CHANGES
