//
//  usdatadumper.c
//  cintltst
//
//  Created by Richard Gillam on 11/1/21.
//  Copyright © 2021 Apple. All rights reserved.
//

#include "cintltst.h"

#include "cmemory.h"
#include "cstring.h"
#include "unicode/uameasureformat.h"
#include "unicode/uatimeunitformat.h"
#include "unicode/udat.h"
#include "unicode/udateintervalformat.h"
#include "unicode/udatpg.h"
#include "unicode/uloc.h"
#include "unicode/ulistformatter.h"
#include "unicode/upluralrules.h"
#include "unicode/ustring.h"
#include <stdio.h> // for sprintf()

//#define RUN_DATA_DUMPER true

void DumpLocaleData(void);
void dumpSingleLocale(const char* localeID);
void dumpStdDateTimeFormats(const char* localeID);
void dumpSkeletonBasedDateTimeFormats(const char* localeID);
void dumpDateIntervalFormats(const char* localeID);
void dumpUnitFormats(const char* localeID);
void dumpDurationPatterns(const char* localeID);
void dumpListFormats(const char* localeID);
void addDataDumper(TestNode** root);

void addDataDumper(TestNode** root)
{
#if RUN_DATA_DUMPER
    addTest(root, &DumpLocaleData, "uadatadumper/DumpLocaleData");
#endif /* RUN_DATA_DUMPER */
}

static const char* languages[] = {
    "ar", "bg", "ca", "cs", "da", "de", "el", "en", "es", "fi", "fr", "he", "hi", "hr",
    "hu", "id", "it", "ja", "kk", "ko", "ms", "nl", "nb", "pl", "pt", "ro", "ru", "sk",
    "sv", "th", "tr", "uk", "vi", "yue", "yue_Hans", "zh", "zh_Hant"
};

static const char* calendars[] = {
    "gregorian", "buddhist", "japanese", "chinese", "hebrew", "islamic", "islamic-umalqura"
};

void DumpLocaleData() {
    printf("\n");   // so the first line doesn't print with the cintltest stuff
    
    int32_t langIndex = 0;
    const char* language = languages[langIndex];
    size_t langLen = uprv_strlen(language);
    int32_t numLocales = uloc_countAvailable();
    
    for (int32_t i = 0; i < numLocales; i++) {
        const char* localeID = uloc_getAvailable(i);
        size_t locLen = uprv_strlen(localeID);
        
        while (uprv_strncmp(localeID, language, (langLen <= locLen) ? langLen : locLen) > 0) {
            if (++langIndex < UPRV_LENGTHOF(languages)) {
                language = languages[langIndex];
                langLen = uprv_strlen(language);
            } else {
                break;
            }
        }
        if (langIndex >= UPRV_LENGTHOF(languages)) {
            break;
        }
        
        if (locLen >= langLen && uprv_strncmp(localeID, language, langLen) == 0 && (localeID[langLen] == '\0' || localeID[langLen] == '_')) {
            dumpSingleLocale(localeID);

            char tmpLocaleID[ULOC_FULLNAME_CAPACITY];
            for (int32_t j = 0; j < UPRV_LENGTHOF(calendars); j++) {
                sprintf(tmpLocaleID, "%s@calendar=%s", localeID, calendars[j]);
                dumpSingleLocale(tmpLocaleID);
            }
        }
    }
}

void dumpSingleLocale(const char* localeID) {
    printf("==================================================================================\n");
    printf("LocaleID: %s\n", localeID);
    
    dumpStdDateTimeFormats(localeID);
    dumpSkeletonBasedDateTimeFormats(localeID);
    dumpDateIntervalFormats(localeID);
    dumpUnitFormats(localeID);
    dumpDurationPatterns(localeID);
    dumpListFormats(localeID);
}

void dumpStdDateTimeFormats(const char* localeID) {
    const UDateFormatStyle dateFormats[] = {
        UDAT_FULL, UDAT_LONG, UDAT_MEDIUM, UDAT_SHORT,
        UDAT_NONE, UDAT_NONE, UDAT_NONE, UDAT_NONE,
        UDAT_FULL, UDAT_LONG, UDAT_MEDIUM, UDAT_SHORT
    };
    const UDateFormatStyle timeFormats[] = {
        UDAT_NONE, UDAT_NONE, UDAT_NONE, UDAT_NONE,
        UDAT_FULL, UDAT_LONG, UDAT_MEDIUM, UDAT_SHORT,
        UDAT_SHORT, UDAT_SHORT, UDAT_SHORT, UDAT_SHORT
    };
    const char* dateFormatNames[] = {
        "FULL", "LONG", "MEDIUM", "SHORT",
        "NONE", "NONE", "NONE", "NONE",
        "FULL", "LONG", "MEDIUM", "SHORT"
    };
    const char* timeFormatNames[] = {
        "NONE", "NONE", "NONE", "NONE",
        "FULL", "LONG", "MEDIUM", "SHORT",
        "SHORT", "SHORT", "SHORT", "SHORT"
    };
    
    UErrorCode err = U_ZERO_ERROR;
    UChar pattern[100];
    for (int32_t i = 0; i < UPRV_LENGTHOF(dateFormats); i++) {
        UDateFormat* df = udat_open(timeFormats[i], dateFormats[i], localeID, u"UTC", -1, NULL, 0, &err);
        udat_toPattern(df, false, pattern, 100, &err);
        
        if (U_SUCCESS(err)) {
            printf("  (%s, %s) => %s\n", dateFormatNames[i], timeFormatNames[i], austrdup(pattern));
        } else {
            printf("  (%s, %s) => %s\n", dateFormatNames[i], timeFormatNames[i], u_errorName(err));
        }
        udat_close(df);
    }
}

void dumpSkeletonBasedDateTimeFormats(const char* localeID) {
    const UChar* skeletons[] = {
        // this list is borrowed in from availFmtsDateTimChk.c-- do we need to test ALL of these?
        u"L", u"LLL", u"LLLL", u"M", u"MMM", u"MMMM",
        u"Md", u"MMMd", u"MMMMd", u"Mdd", u"MMdd", u"MMMdd", u"MMMMdd",
        u"MEEE", u"MMMEEE", u"MMMMEEEE", u"MEd", u"MMMEd", u"MMMMEd", u"MEdd", u"MMMEdd", u"MMMMEdd",
        u"MEEEd", u"MMMEEEd", u"MMMMEEEd", u"MEEEEd", u"MMMEEEEd", u"MMMMEEEEd", u"MEEEdd", u"MMMEEEdd", u"MMMMEEEdd",
        u"Ed", u"EEEd", u"EEEEd", u"Edd", u"EEEdd",
        u"ccc", u"cccc", u"EEE", u"EEEE", u"d", u"y", u"yyyy", u"Gy",
        u"yM", u"yMMM", u"yMMMM", u"UM", u"UMMM", u"yyM", u"yyMMM", u"yyMMMM",
        u"yyyyM", u"yyyyMMM", u"yyyyMMMM", u"yyyyLLLL",
        u"yMd", u"yMMMd", u"yMMMMd", u"UMd", u"UMMMd", u"yyMMdd", u"yyyyMd", u"yyyyMMMd", u"yyyyMMMMd",
        u"yMEd", u"yMMMEd", u"yMMMMEd", u"EEyMd", u"yyyyMEd", u"yyyyMMMEd", u"yyyyMMMMEd",
        u"yMEEEd", u"yMMMEEEd", u"yMMMEEEEd", u"yMMMMEEEd", u"yyyyMEEEd", u"yyyyMMMEEEd", u"yyyyMMMMEEEd", u"yMMMMEEEEd",
        u"GyMMM", u"GyMMMd", u"GGGGyMMMMd", u"GyMMMEd", u"GyM", u"GyMM", u"GyMd", u"GyyMMdd",
        u"GGGGGyM", u"GGGGGyMM", u"GGGGGyMd", u"GGGGGyyMMdd",
        u"h", u"hh", u"h a", u"H", u"HH", u"j", u"jj", u"ja",
        u"hm", u"hmm", u"hhmm", u"hmma", u"Hm", u"Hmm", u"HHmm", u"jm", u"jmm", u"jjmm", u"jmma",
        u"hms", u"hmmss", u"hhmmss", u"Hms", u"Hmmss", u"HHmmss", u"jms", u"jmmss", u"jjmmss", u"ms", u"mss", u"mmss",
        u"hmz", u"hmmz", u"hhmmz", u"Hmz", u"Hmmz", u"HHmmz", u"hmsz", u"hmmssz", u"hhmmssz", u"Hmsz", u"Hmmssz", u"HHmmssz",
        u"jmmssSSS", u"Ejmm", u"mm", u"Ehhmm", u"EHHmm", u"Ejjmm", u"EEEEjmm", u"EEEjmm", u"Jmm",
        // other combo forms used in iOS
        u"yyMEdjmma", u"yMMMEEEEdjmma", u"MMMEEEdjmm", u"EEEMMMdj"
    };

    UErrorCode err = U_ZERO_ERROR;
    UDateTimePatternGenerator* dtpg = udatpg_open(localeID, &err);
    UChar pattern[200];
    if (U_SUCCESS(err)) {
        for (int32_t i = 0; i < UPRV_LENGTHOF(skeletons); i++) {
            const UChar* skeleton = skeletons[i];
            udatpg_getBestPattern(dtpg, skeleton, -1, pattern, 200, &err);
            if (U_SUCCESS(err)) {
                printf("  %s => %s\n", austrdup(skeleton), austrdup(pattern));
            } else {
                printf("  %s => %s\n", austrdup(skeleton), u_errorName(err));
                err = U_ZERO_ERROR;
            }
        }
        udatpg_close(dtpg);
    } else {
        printf("  Error opening DateTimePatternGenerator for %s: %s\n", localeID, u_errorName(err));
    }
}

void dumpDateIntervalFormats(const char* localeID) {
    // NOTE: This might need work.  The udtitvfmt_getPatternString() method (which I added) just calls
    // through to DateIntervalInfo::getIntervalPattern(), and that function doesn't do any fallback or
    // transformation of the skeleton.  If the exact skeleton string doesn't exist in a locale's
    // intervalFormats resource, that function doesn't return it.  So "yMMMMd" doesn't work in Arabic
    // (but does in some other locales), and "jms" doesn't work because it needs to be transformed
    // into "hms" or "Hms".
    // I can probably re-cast this function and maybe the internal API it calls to work off of
    // fully-resolved skeleton strings and patterns, but I'm running out of time and hoping this
    // is an acceptable first approximation.
    const UChar* skeletons[] = {
        u"yMMMEd", u"yMMMEd", u"yMMMEd",
        u"yMMMd", u"yMMMd", u"yMMMd",
        u"yMd", u"yMd", u"yMd",
        u"hm", u"hm", u"hm",
        u"Hm", u"Hm"
    };
    const UCalendarDateFields fields[] = {
        UCAL_DAY_OF_MONTH, UCAL_MONTH, UCAL_YEAR,
        UCAL_DAY_OF_MONTH, UCAL_MONTH, UCAL_YEAR,
        UCAL_DAY_OF_MONTH, UCAL_MONTH, UCAL_YEAR,
        UCAL_MINUTE, UCAL_HOUR, UCAL_AM_PM,
        UCAL_MINUTE, UCAL_HOUR
    };
    const char* fieldNames[] = {
        "DAY_OF_MONTH", "MONTH", "YEAR",
        "DAY_OF_MONTH", "MONTH", "YEAR",
        "DAY_OF_MONTH", "MONTH", "YEAR",
        "MINUTE", "HOUR", "AM_PM",
        "MINUTE", "HOUR"
    };

    UErrorCode err = U_ZERO_ERROR;
    UDateIntervalFormat* dif = udtitvfmt_open(localeID, u"yMd", -1, u"UTC", -1, &err);
    UChar pattern[100];
    
    for (int32_t i = 0; i < UPRV_LENGTHOF(skeletons); i++) {
        udtitvfmt_getPatternString(dif, skeletons[i], fields[i], pattern, 100, &err);
        
        if (U_SUCCESS(err)) {
            printf("  (%s, %s) => %s\n", austrdup(skeletons[i]), fieldNames[i], austrdup(pattern));
        } else {
            printf("  (%s, %s) => %s\n", austrdup(skeletons[i]), fieldNames[i], u_errorName(err));
        }
    }
    
    udtitvfmt_close(dif);
}

void dumpUnitFormats(const char* localeID) {
    // NOTES:
    // - Right now, this test uses the Apple-specific UAMeasureFormat class rather than the
    // new ICU NumberFormatter stuff, because that's what Foundation is using.  We may want
    // to change this to use the new NumberFormatter stuff (or add another test that does that)
    // sometime soon.
    // - This test works by formatting dummy values right now, rather than by trying to get
    // pattern strings or anything like that.  It also focuses on formatting single units,
    // not compound units, and doesn't test the units-for-usage stuff (just testing hard-coded units).
    const UAMeasureFormatWidth widths[] = { UAMEASFMT_WIDTH_WIDE, UAMEASFMT_WIDTH_SHORT, UAMEASFMT_WIDTH_SHORTER, UAMEASFMT_WIDTH_NARROW };
    const char* widthNames[] = { "WIDE", "SHORT", "SHORTER", "NARROW" };
    const UAMeasureUnit units[] = {
        UAMEASUNIT_DURATION_DAY, UAMEASUNIT_DURATION_HOUR, UAMEASUNIT_DURATION_MINUTE, UAMEASUNIT_DURATION_SECOND,
        UAMEASUNIT_LENGTH_MILE, UAMEASUNIT_LENGTH_YARD, UAMEASUNIT_LENGTH_FOOT, UAMEASUNIT_LENGTH_INCH,
        UAMEASUNIT_LENGTH_KILOMETER, UAMEASUNIT_LENGTH_METER, UAMEASUNIT_LENGTH_CENTIMETER,
        UAMEASUNIT_MASS_POUND, UAMEASUNIT_MASS_OUNCE, UAMEASUNIT_MASS_STONE,
        UAMEASUNIT_MASS_KILOGRAM, UAMEASUNIT_MASS_GRAM,
        UAMEASUNIT_TEMPERATURE_FAHRENHEIT, UAMEASUNIT_TEMPERATURE_CELSIUS,
        UAMEASUNIT_SPEED_MILE_PER_HOUR, UAMEASUNIT_SPEED_KILOMETER_PER_HOUR, UAMEASUNIT_SPEED_METER_PER_SECOND,
        UAMEASUNIT_PRESSURE_INCH_HG, UAMEASUNIT_PRESSURE_MILLIMETER_OF_MERCURY, UAMEASUNIT_PRESSURE_MILLIBAR,
        UAMEASUNIT_PRESSURE_HECTOPASCAL
    };
    const char* unitNames[] = {
        "DAY", "HOUR", "MINUTE", "SECOND",
        "MILE", "YARD", "FOOT", "INCH",
        "KILOMETER", "METER", "CENTIMETER",
        "POUND", "OUNCE", "STONE",
        "KILOGRAM", "GRAM",
        "FAHRENHEIT", "CELSIUS",
        "MILE_PER_HOUR", "KILOMETER_PER_HOUR", "METER_PER_SECOND",
        "INCH_HG", "MILLIMETER_OF_MERCURY", "MILLIBAR",
        "HECTOPASCAL"
    };

    UErrorCode err = U_ZERO_ERROR;
    UChar keywords[100]; // 10 arrays of 10
    double samples[10];
    int32_t numKeywords = 0;

    UPluralRules* plRules = uplrules_open(localeID, &err);
    UEnumeration* kwEnum = uplrules_getKeywords(plRules, &err);
    if (U_SUCCESS(err)) {
        const UChar* kw = uenum_unext(kwEnum, NULL, &err);
        while (kw != NULL && U_SUCCESS(err) && numKeywords < 10) {
            u_strcpy(&keywords[numKeywords * 10], kw);
            samples[numKeywords] = uplrules_getSampleForKeyword(plRules, kw, &err);
            ++numKeywords;
            
            kw = uenum_unext(kwEnum, NULL, &err);
        }
    }
    uenum_close(kwEnum);
    uplrules_close(plRules);

    UAMeasureFormat* formatters[UPRV_LENGTHOF(widths)];
    for (int32_t i = 0; i < UPRV_LENGTHOF(widths); i++) {
        formatters[i] = uameasfmt_open(localeID, widths[i], NULL, &err);
        if (U_FAILURE(err)) {
            printf("  Error opening measure formatter for %s(%s)\n", localeID, widthNames[i]);
            formatters[i] = NULL;
            err = U_ZERO_ERROR;
        }
    }

    UChar formattedValue[100];
    for (int32_t i = 0; i < UPRV_LENGTHOF(units); i++) {
        for (int32_t j = 0; j < UPRV_LENGTHOF(widths); j++) {
            if (formatters[j] != NULL) {
                for (int32_t k = 0; k < numKeywords; k++) {
                    uameasfmt_format(formatters[j], samples[k], units[i], formattedValue, 100, &err);
                    
                    if (U_SUCCESS(err)) {
                        printf("  (%s, %s, %s) => %s\n", unitNames[i], widthNames[j], austrdup(&keywords[k * 10]), austrdup(formattedValue));
                    } else {
                        printf("  (%s, %s, %s) => %s\n", unitNames[i], widthNames[j], austrdup(&keywords[k * 10]), u_errorName(err));
                        err = U_ZERO_ERROR;
                    }
                }
            }
        }
    }

    for (int32_t i = 0; i < UPRV_LENGTHOF(widths); i++) {
        if (formatters[i] != NULL) {
            uameasfmt_close(formatters[i]);
        }
    }
}

void dumpDurationPatterns(const char* localeID) {
    const UATimeUnitTimePattern patternTypes[] = { UATIMEUNITTIMEPAT_HM, UATIMEUNITTIMEPAT_HMS, UATIMEUNITTIMEPAT_MS };
    const char* patternTypeNames[] = { "HM", "HMS", "MS" };
    
    UErrorCode err = U_ZERO_ERROR;
    UChar pattern[100];
    for (int32_t i = 0; i < UPRV_LENGTHOF(patternTypes); i++) {
        uatmufmt_getTimePattern(localeID, patternTypes[i], pattern, 100, &err);
        
        if (U_SUCCESS(err)) {
            printf("  %s => %s\n", patternTypeNames[i], austrdup(pattern));
        } else {
            printf("  %s => %s\n", patternTypeNames[i], u_errorName(err));
            err = U_ZERO_ERROR;
        }
    }
}

void dumpListFormats(const char* localeID) {
    const UListFormatterType types[] = { ULISTFMT_TYPE_AND, ULISTFMT_TYPE_UNITS, ULISTFMT_TYPE_UNITS };
    const char* typeNames[] = { "AND", "UNITS", "UNITS" };
    const UListFormatterWidth widths[] = { ULISTFMT_WIDTH_WIDE, ULISTFMT_WIDTH_SHORT, ULISTFMT_WIDTH_NARROW };
    const char* widthNames[] = { "WIDE", "SHORT", "NARROW" };
    const UChar* listItems[] = { u"a", u"b", u"c", u"d" };
    
    UErrorCode err = U_ZERO_ERROR;
    UListFormatter* lf = NULL;
    UChar list[200];
    for (int32_t i = 0; i < UPRV_LENGTHOF(types); i++) {
        lf = ulistfmt_openForType(localeID, types[i], widths[i], &err);
        if (U_SUCCESS(err)) {
            for (int32_t j = 1; j <= UPRV_LENGTHOF(listItems); j++) {
                ulistfmt_format(lf, listItems, NULL, j, list, 200, &err);
                
                if (U_SUCCESS(err)) {
                    printf("  (%s, %s, %d elements) => %s\n", typeNames[i], widthNames[i], j, austrdup(list));
                } else {
                    printf("  (%s, %s, %d elements) => %s\n", typeNames[i], widthNames[i], j, u_errorName(err));
                    err = U_ZERO_ERROR;
                }
            }
            ulistfmt_close(lf);
        } else {
            printf("  Error creating list format for (%s, %s)\n", typeNames[i], widthNames[i]);
            err = U_ZERO_ERROR;
        }
    }
}
