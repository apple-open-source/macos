/*
*****************************************************************************************
* Copyright (C) 2014-2016 Apple Inc. All Rights Reserved.
*****************************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include <stdlib.h>
#include "unicode/uameasureformat.h"
#include "unicode/fieldpos.h"
#include "unicode/localpointer.h"
#include "unicode/numfmt.h"
#include "unicode/measunit.h"
#include "unicode/measure.h"
#include "unicode/measfmt.h"
#include "unicode/unistr.h"
#include "unicode/unum.h"
#include "unicode/umisc.h"
#include "unicode/ures.h"
#include "uresimp.h"
#include "ustr_imp.h"
#include "cstring.h"
#include "ulocimp.h"
#include "units_data.h"

U_NAMESPACE_USE


U_CAPI UAMeasureFormat* U_EXPORT2
uameasfmt_open( const char*          locale,
                UAMeasureFormatWidth width,
                UNumberFormat*       nfToAdopt,
                UErrorCode*          status )
{
    if (U_FAILURE(*status)) {
        return NULL;
    }
    UMeasureFormatWidth mfWidth = UMEASFMT_WIDTH_WIDE;
    switch (width) {
        case UAMEASFMT_WIDTH_WIDE:
            break;
        case UAMEASFMT_WIDTH_SHORT:
            mfWidth = UMEASFMT_WIDTH_SHORT; break;
        case UAMEASFMT_WIDTH_NARROW:
            mfWidth = UMEASFMT_WIDTH_NARROW; break;
        case UAMEASFMT_WIDTH_NUMERIC:
            mfWidth = UMEASFMT_WIDTH_NUMERIC; break;
        case UAMEASFMT_WIDTH_SHORTER:
            mfWidth = UMEASFMT_WIDTH_SHORTER; break;
        default:
            *status = U_ILLEGAL_ARGUMENT_ERROR; return NULL;
    }
    LocalPointer<MeasureFormat> measfmt( new MeasureFormat(Locale(locale), mfWidth, (NumberFormat*)nfToAdopt, *status) );
    if (U_FAILURE(*status)) {
        return NULL;
    }
    return (UAMeasureFormat*)measfmt.orphan();
}


U_CAPI void U_EXPORT2
uameasfmt_close(UAMeasureFormat *measfmt)
{
    delete (MeasureFormat*)measfmt;
}

U_CAPI int32_t U_EXPORT2
uameasfmt_format( const UAMeasureFormat* measfmt,
                  double            value,
                  UAMeasureUnit     unit,
                  UChar*            result,
                  int32_t           resultCapacity,
                  UErrorCode*       status )
{
    return uameasfmt_formatGetPosition(measfmt, value, unit, result,
                                        resultCapacity, NULL, status);
}

U_CAPI int32_t U_EXPORT2
uameasfmt_formatGetPosition( const UAMeasureFormat* measfmt,
                            double            value,
                            UAMeasureUnit     unit,
                            UChar*            result,
                            int32_t           resultCapacity,
                            UFieldPosition*   pos,
                            UErrorCode*       status )
{
    if (U_FAILURE(*status)) {
        return 0;
    }
    if ( ((result==NULL)? resultCapacity!=0: resultCapacity<0) ) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    if ( ((MeasureFormat*)measfmt)->getWidth() == UMEASFMT_WIDTH_NUMERIC &&
            (unit == UAMEASUNIT_TEMPERATURE_CELSIUS || unit == UAMEASUNIT_TEMPERATURE_FAHRENHEIT) ) {
        // Fix here until http://bugs.icu-project.org/trac/ticket/11593 is addressed
        unit = UAMEASUNIT_TEMPERATURE_GENERIC;
    }
    MeasureUnit * munit = MeasureUnit::createFromUAMeasureUnit(unit, status);
    if (U_FAILURE(*status)) {
        return 0;
    }
    LocalPointer<Measure> meas(new Measure(value, munit, *status));
    if (U_FAILURE(*status)) {
        return 0;
    }
    FieldPosition fp;
    if (pos != NULL) {
        int32_t field = pos->field;
        if (field < 0 || field >= UNUM_FIELD_COUNT) {
            *status = U_ILLEGAL_ARGUMENT_ERROR;
            return 0;
        }
        fp.setField(field);
    } else {
        fp.setField(FieldPosition::DONT_CARE);
    }
    Formattable fmt;
    fmt.adoptObject(meas.orphan());
    UnicodeString res;
    res.setTo(result, 0, resultCapacity);
    ((MeasureFormat*)measfmt)->format(fmt, res, fp, *status);
    if (pos != NULL) {
        pos->beginIndex = fp.getBeginIndex();
        pos->endIndex = fp.getEndIndex();
    }
    return res.extract(result, resultCapacity, *status);
}

enum { kMeasuresMax = 8 }; // temporary limit, will add allocation later as necessary

U_CAPI int32_t U_EXPORT2
uameasfmt_formatMultiple( const UAMeasureFormat* measfmt,
                          const UAMeasure*  measures,
                          int32_t           measureCount,
                          UChar*            result,
                          int32_t           resultCapacity,
                          UErrorCode*       status )
{
    return uameasfmt_formatMultipleForFields(measfmt, measures, measureCount,
                                             result, resultCapacity, NULL, status);
}

U_CAPI int32_t U_EXPORT2
uameasfmt_formatMultipleForFields( const UAMeasureFormat* measfmt,
                                   const UAMeasure*  measures,
                                   int32_t           measureCount,
                                   UChar*            result,
                                   int32_t           resultCapacity,
                                   UFieldPositionIterator* fpositer,
                                   UErrorCode*       status )
{
    if (U_FAILURE(*status)) {
        return 0;
    }
    if ( ((result==NULL)? resultCapacity!=0: resultCapacity<0) || measureCount <= 0 || measureCount > kMeasuresMax ) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    int32_t i;
    Measure * measurePtrs[kMeasuresMax];
    for (i = 0; i < kMeasuresMax && U_SUCCESS(*status); i++) {
        if (i < measureCount) {
            UAMeasureUnit unit = measures[i].unit;
            if ( ((MeasureFormat*)measfmt)->getWidth() == UMEASFMT_WIDTH_NUMERIC &&
                    (unit == UAMEASUNIT_TEMPERATURE_CELSIUS || unit == UAMEASUNIT_TEMPERATURE_FAHRENHEIT) ) {
                // Fix here until http://bugs.icu-project.org/trac/ticket/11593 is addressed
                unit = UAMEASUNIT_TEMPERATURE_GENERIC;
            }
            MeasureUnit * munit = MeasureUnit::createFromUAMeasureUnit(unit, status);
            measurePtrs[i] = new Measure(measures[i].value, munit, *status);
        } else {
            MeasureUnit * munit = MeasureUnit::createGForce(*status); // any unit will do
            measurePtrs[i] = new Measure(0, munit, *status);
        }
    }
    if (U_FAILURE(*status)) {
        while (i-- > 0) {
            delete measurePtrs[i];
        }
        return 0;
    }
    Measure measureObjs[kMeasuresMax] = { *measurePtrs[0], *measurePtrs[1], *measurePtrs[2], *measurePtrs[3],
                                          *measurePtrs[4], *measurePtrs[5], *measurePtrs[6], *measurePtrs[7] };
    UnicodeString res;
    res.setTo(result, 0, resultCapacity);
    ((MeasureFormat*)measfmt)->formatMeasures(measureObjs, measureCount, res, (FieldPositionIterator*)fpositer, *status);
    for (i = 0; i < kMeasuresMax; i++) {
        delete measurePtrs[i];
    }
    return res.extract(result, resultCapacity, *status);
}

U_CAPI int32_t U_EXPORT2
uameasfmt_getUnitName( const UAMeasureFormat* measfmt,
                       UAMeasureUnit unit,
                       UChar* result,
                       int32_t resultCapacity,
                       UErrorCode* status )
{
    if (U_FAILURE(*status)) {
        return 0;
    }
    if ( ((result==NULL)? resultCapacity!=0: resultCapacity<0) ) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    LocalPointer<const MeasureUnit> munit(MeasureUnit::createFromUAMeasureUnit(unit, status));
    if (U_FAILURE(*status)) {
        return 0;
    }
    UnicodeString res;
    res.setTo(result, 0, resultCapacity);
    ((MeasureFormat*)measfmt)->getUnitName(munit.getAlias(), res);
    if (res.isBogus()) {
        *status = U_MISSING_RESOURCE_ERROR;
        return 0;
    }
    return res.extract(result, resultCapacity, *status);
}

U_CAPI int32_t U_EXPORT2
uameasfmt_getMultipleUnitNames( const UAMeasureFormat* measfmt,
                                const UAMeasureUnit* units,
                                int32_t unitCount,
                                UAMeasureNameListStyle listStyle,
                                UChar* result,
                                int32_t resultCapacity,
                                UErrorCode* status )
{
    if (U_FAILURE(*status)) {
        return 0;
    }
    if ( ((result==NULL)? resultCapacity!=0: resultCapacity<0) || unitCount <= 0 || unitCount > kMeasuresMax ) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    int32_t i;
    const MeasureUnit * unitPtrs[kMeasuresMax];
    for (i = 0; i < unitCount && U_SUCCESS(*status); i++) {
        unitPtrs[i] = MeasureUnit::createFromUAMeasureUnit(units[i], status);
    }
    if (U_FAILURE(*status)) {
        while (i-- > 0) {
            delete unitPtrs[i];
        }
        return 0;
    }
    UnicodeString res;
    res.setTo(result, 0, resultCapacity);
    ((MeasureFormat*)measfmt)->getMultipleUnitNames(unitPtrs, unitCount, listStyle, res);
    for (i = 0; i < unitCount; i++) {
        delete unitPtrs[i];
    }
    if (res.isBogus()) {
        *status = U_MISSING_RESOURCE_ERROR;
        return 0;
    }
    return res.extract(result, resultCapacity, *status);
}

typedef icu::units::UnitPreferences UnitPreferences;
typedef icu::units::UnitPreference UnitPreference;

static const char* usageAliases[][2] = {
//    { "area/land-agricult",     "land" }, // falls back automatically to area/land; no case in the old data of this being different
//    { "area/land-commercl",     "land" }, // falls back automatically to area/land; no case in the old data of this being different
//    { "area/land-residntl",     "land" }, // falls back automatically to area/land; no case in the old data of this being different
    { "area/default",           "default[2]" },
    { "area/large",             "default[0]" },
    { "area/small",             "default[3]" },
    { "concentr/blood-glucose", "concentration/blood-glucose" },
    { "duration/music-track",   "media" },
    { "duration/person-age",    "year-duration/person-age[1]" }, // [0] is years, [1] is years and months, which is what the old data did
    { "duration/tv-program",    "media" },
    { "energy/large",           "default" }, // in the old data, energy/ would give us back "joule"-- nothing in the current stuff does
    { "energy/person-usage",    "food" },
    { "length/default",         "default[1]" },
    { "length/large",           "default[0]" },
    { "length/person-informal", "person-height[0]" },
    { "length/person-small",    "person" },
    { "length/road-small",      "road[3]" },
    { "length/small",           "default[2]" },
    { "length/visiblty-small",  "visiblty[1]" },
    { "mass/default",           "default[1]" },
    { "mass/large",             "default[0]" },
    { "mass/person-small",      "person[1]" },
    { "mass/small",             "default[2]" },
    { "power/default",          "default[3]" },
    { "power/large",            "default[2]" },
    { "power/small",            "default[4]" },
    { "speed/road-travel",      "default" },
    { "temperature/person"      "default" },
    { "volume/default",         "fluid[0]" },
    { "volume/large",           "default" }, // this is "cubic-kilometer" in the old data-- nothing in the new data produces this, so I went with "cubic-meter"
    { "volume/small",           "fluid[1]" },
    { "volume/vehicle-fuel",    "vehicle" }
};
enum { kUsageAliasCount = UPRV_LENGTHOF(usageAliases) };

// comparator for binary search of usageAliases
static int compareAppleMapElements(const void *key, const void *entry) {
    return uprv_strcmp((const char *)key, ((const char **)entry)[0]);
}

// internal function for uameasfmt_getUnitsForUsage()
static void resolveUsageAlias(CharString& category,
                              CharString& usage,
                              int32_t&    offset,
                              UErrorCode& status) {
    offset = 0;
    
    CharString categoryAndUsage(category, status);
    categoryAndUsage.append("/", status);
    if (usage.isEmpty()) {
        categoryAndUsage.append("default", status);
    } else {
        categoryAndUsage.append(usage, status);
    }
    
    if (U_SUCCESS(status)) {
        const char** entry = (const char**)bsearch(categoryAndUsage.data(), usageAliases, kUsageAliasCount, sizeof(usageAliases[0]), compareAppleMapElements);
        if (entry != nullptr) {
            usage.clear();
            usage.append(entry[1], status);
            if (usage.contains("/")) {
                CharString newCategoryAndUsage;
                newCategoryAndUsage.append(usage, status);
                char* newCategory = newCategoryAndUsage.data();
                char* slash = uprv_strchr(newCategory, '/');
                char* newUsage = slash + 1;
                *slash = '\0';
                category.clear();
                category.append(newCategory, status);
                usage.clear();
                usage.append(newUsage, status);
            }
            if (usage.contains("[")) {
                CharString usageAndOffset;
                usageAndOffset.append(usage, status);
                char* newUsage = usageAndOffset.data();
                char* openBracket = uprv_strchr(newUsage, '[');
                char* offsetStr = openBracket + 1;
                usage.truncate(openBracket - newUsage);
                offset = atoi(offsetStr);
            } else {
                offset = 0;
            }
        }
    }
}

// internal function for uameasfmt_getUnitsForUsage()
static void resolveLocaleRegion(const char* locale,
                                const char* category,
                                char* region,
                                UErrorCode* status) {
    if (U_FAILURE(*status)) {
        return;
    }
    
    const int32_t kKeyValueMax = 15;
    UErrorCode localStatus;
    UBool usedOverride = false;
    // First check for ms overrides, except in certain categories
    if (uprv_strcmp(category, "concentr") != 0 && uprv_strcmp(category, "duration") != 0) {
        char msValue[kKeyValueMax + 1];
        localStatus = U_ZERO_ERROR;
        int32_t msValueLen = uloc_getKeywordValue(locale, "measure", msValue, kKeyValueMax, &localStatus);
        if (U_FAILURE(localStatus) || msValueLen <= 2) {
            // I don't think an old-style locale ID with "ms" is technically legal, but continue to support it for backward compatibility
            localStatus = U_ZERO_ERROR;
            msValueLen = uloc_getKeywordValue(locale, "ms", msValue, kKeyValueMax, &localStatus);
        }
        if (U_SUCCESS(localStatus) && msValueLen> 2) {
            msValue[kKeyValueMax] = 0; // ensure termination
            if (uprv_strcmp(msValue, "metric") == 0) {
                uprv_strcpy(region, "001");
                usedOverride = true;
            } else if (uprv_strcmp(msValue, "ussystem") == 0) {
                uprv_strcpy(region, "US");
                usedOverride = true;
            } else if (uprv_strcmp(msValue, "uksystem") == 0) {
                uprv_strcpy(region, "GB");
                usedOverride = true;
            }
        }
    }
    if (!usedOverride) {
        (void)ulocimp_getRegionForSupplementalData(locale, true, region, ULOC_COUNTRY_CAPACITY, status);
    }
}

U_CAPI int32_t U_EXPORT2
uameasfmt_getUnitsForUsage( const char*     locale,
                            const char*     category,
                            const char*     usage,
                            UAMeasureUnit*  units,
                            int32_t         unitsCapacity,
                            UErrorCode*     status )
{
    CharString resolvedCategory(category, *status);
    CharString resolvedUsage(usage, *status);
    int32_t entryOffset;
    resolveUsageAlias(resolvedCategory, resolvedUsage, entryOffset, *status);
    
    // Get region to use; this has to be done after resolveUsageAlias
    char region[ULOC_COUNTRY_CAPACITY];
    resolveLocaleRegion(locale, resolvedCategory.data(), region, status);
    if (U_FAILURE(*status)) {
        return 0;
    }

    LocalPointer<UnitPreferences> prefsGetter(new UnitPreferences(*status), *status);
    if (U_FAILURE(*status)) {
        return 0;
    }
    
    // rdar://97937093 Integrate ICU 72: Update the following per changes in https://github.com/unicode-org/icu/pull/2182
    Locale regLoc("und", region);    
    auto prefs = prefsGetter->getPreferencesFor(resolvedCategory.data(), resolvedUsage.data(), regLoc, *status);
    if (U_FAILURE(*status) || prefs.length() <= 0) {
        return 0;
    }
    
    if (entryOffset >= prefs.length()) {
        // if the alias table gave us an offset off the end of the array, just use the last element in the array
        entryOffset = prefs.length() - 1;
    }
    MeasureUnit unit(MeasureUnit::forIdentifier(prefs[entryOffset]->unit.data(), *status));
    if (U_FAILURE(*status)) {
        return 0;
    } else {
        return unit.getUAMeasureUnits(units, unitsCapacity, *status);
    }
}

U_CAPI const char * U_EXPORT2
uameasfmt_getUnitCategory(UAMeasureUnit unit,
                          UErrorCode* status )
{
    if (U_FAILURE(*status)) {
        return NULL;
    }
    LocalPointer<const MeasureUnit> munit(MeasureUnit::createFromUAMeasureUnit(unit, status));
    if (U_FAILURE(*status)) {
        return NULL;
    }
    return munit->getType();
}

#endif /* #if !UCONFIG_NO_FORMATTING */
