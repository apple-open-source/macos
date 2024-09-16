/********************************************************************
 * Copyright (c) 2015-2016, Apple Inc. All Rights Reserved.
 ********************************************************************/
/* C API TEST FOR APPLE MEASUREFORMAT C WRAPPER */

#include "unicode/utypes.h"

#if U_PLATFORM_IS_DARWIN_BASED || U_PLATFORM_IS_LINUX_BASED || U_PLATFORM == U_PF_BSD || U_PLATFORM == U_PF_SOLARIS
#include <unistd.h>
#endif

#if !UCONFIG_NO_FORMATTING

#include "unicode/uameasureformat.h"
#include "unicode/ustring.h"
#include "unicode/uloc.h"
#include "cintltst.h"
#include "cmemory.h"
#include "cstring.h"

static void TestUAMeasureFormat(void);
static void TestUAMeasFmtOpenAllLocs(void);
static void TestUAGetUnitsForUsage(void);
static void TestUAGetCategoryForUnit(void);
static void TestMeasurementSystemOverride(void);
static void TestRgSubtag(void);

void addMeasureFormatTest(TestNode** root);

#define TESTCASE(x) addTest(root, &x, "tsformat/cmeasureformattest/" #x)

void addMeasureFormatTest(TestNode** root)
{
    TESTCASE(TestUAMeasureFormat);
    TESTCASE(TestUAMeasFmtOpenAllLocs);
    TESTCASE(TestUAGetUnitsForUsage);
    TESTCASE(TestUAGetCategoryForUnit);
    TESTCASE(TestMeasurementSystemOverride);
    TESTCASE(TestRgSubtag);
}

typedef struct {
    UAMeasureUnit unit;
    float         value;
    const char *  expectFmt_wide_2;
    const char *  expectFmt_wide_0;
    const char *  expectFmt_shrt_X;
    const char *  expectFmt_shrt_1;
    const char *  expectFmt_narr_0;
    const char *  expectFmt_numr_0;
    int32_t       beginInt_wide_0;
    int32_t       endInt_wide_0;
    int32_t       beginInt_numr_0;
    int32_t       endInt_numr_0;
} SingleUnitFormat;

typedef struct {
    UAMeasureUnit unit;
    const char *  expectName_wide;
    const char *  expectName_shrt;
    const char *  expectName_narr;
} SingleUnitName;

typedef struct {
    const UAMeasure* measures;
    int32_t          measureCount;
    const char *     expectFmt_wide_2;
    const char *     expectFmt_wide_0;
    const char *     expectFmt_shrt_X;
    const char *     expectFmt_shrt_1;
    const char *     expectFmt_shrtr_1;
    const char *     expectFmt_narr_0;
    const char *     expectFmt_numr_0;
    const char *     expectFmt_numr_2;
    int32_t          ranges_wide_2[8][3];
    int32_t          ranges_shrtr_1[8][3];
    int32_t          ranges_numr_0[8][3];
    int32_t          ranges_numr_2[8][3];
} MultipleUnitFormat;

typedef struct {
    const UAMeasureUnit*   units;
    int32_t                unitCount;
    UAMeasureNameListStyle listStyle;
    const char *           expectName_wide;
    const char *           expectName_shrt;
    const char *           expectName_narr;
} MultipleUnitName;

typedef struct {
    const char *               locale;
    const SingleUnitFormat *   singleUnitFormatTests;   // may be NULL
    const SingleUnitName *     singleUnitNameTests;     // may be NULL
    const MultipleUnitFormat * multipleUnitFormatTests; // may be NULL
    const MultipleUnitName *   multipleUnitNameTests;   // may be NULL
} LocaleWidthNumFmtItem;

static const SingleUnitFormat en_singFmt[] = {
//    unit                                 value   wide_2                       wide_0                    shrt_X              shrt_1            narr_0         numr_0       wide_0  narr_0
    { UAMEASUNIT_DURATION_MINUTE,           0.0,   "0.00 minutes",              "0 minutes",              "0 min",            "0.0 min",        "0m",          "0m",        0,1,    0,1   },
    { UAMEASUNIT_DURATION_MINUTE,           1.0,   "1.00 minutes",              "1 minute",               "1 min",            "1.0 min",        "1m",          "1m",        0,1,    0,1   },
    { UAMEASUNIT_DURATION_MINUTE,           5.25,  "5.25 minutes",              "5 minutes",              "5.25 min",         "5.2 min",        "5m",          "5m",        0,1,    0,1   },
    { UAMEASUNIT_DURATION_DAY,              5.25,  "5.25 days",                 "5 days",                 "5.25 days",        "5.2 days",       "5d",          "5d",        0,1,    0,1   },
    { UAMEASUNIT_DURATION_WEEK,             5.25,  "5.25 weeks",                "5 weeks",                "5.25 wks",         "5.2 wks",        "5w",          "5w",        0,1,    0,1   },
    { UAMEASUNIT_DURATION_CENTURY,         37.203, "37.20 centuries",           "37 centuries",           "37.203 c",         "37.2 c",         "37c",         "37c",       0,2,    0,2   },
    { UAMEASUNIT_LENGTH_CENTIMETER,        37.203, "37.20 centimeters",         "37 centimeters",         "37.203 cm",        "37.2 cm",        "37cm",        "37cm",      0,2,    0,2   },
    { UAMEASUNIT_SPEED_KILOMETER_PER_HOUR, 37.203, "37.20 kilometers per hour", "37 kilometers per hour", "37.203 km/h",      "37.2 km/h",      "37km/h",      "37km/h",    0,2,    0,2   },
    { UAMEASUNIT_TEMPERATURE_CELSIUS,      37.203, "37.20 degrees Celsius",     "37 degrees Celsius",     "37.203\\u00B0C",   "37.2\\u00B0C",   "37\\u00B0C",  "37\\u00B0", 0,2,    0,2   },
    { UAMEASUNIT_TEMPERATURE_FAHRENHEIT,   37.203, "37.20 degrees Fahrenheit",  "37 degrees Fahrenheit",  "37.203\\u00B0F",   "37.2\\u00B0F",   "37\\u00B0",   "37\\u00B0", 0,2,    0,2   },
    { UAMEASUNIT_TEMPERATURE_GENERIC,      37.203, "37.20 degrees",             "37 degrees",             "37.203\\u00B0",    "37.2\\u00B0",    "37\\u00B0",   "37\\u00B0", 0,2,    0,2   },
    { UAMEASUNIT_TEMPERATURE_GENERIC,       1.000, "1.00 degrees",              "1 degree",               "1\\u00B0",         "1.0\\u00B0",     "1\\u00B0",    "1\\u00B0",  0,1,    0,1   }, // rdar://65008672
    { UAMEASUNIT_VOLUME_LITER,             37.203, "37.20 liters",              "37 liters",              "37.203 L",         "37.2 L",         "37L",         "37L",       0,2,    0,2   },
    { UAMEASUNIT_ENERGY_FOODCALORIE,       37.203, "37.20 Calories",            "37 Calories",            "37.203 Cal",       "37.2 Cal",       "37Cal",       "37Cal",     0,2,    0,2   },
    { UAMEASUNIT_ENERGY_JOULE,             37.203, "37.20 joules",              "37 joules",              "37.203 J",         "37.2 J",         "37J",         "37J",       0,2,    0,2   },
    { UAMEASUNIT_DIGITAL_MEGABYTE,         37.203, "37.20 megabytes",           "37 megabytes",           "37.203 MB",        "37.2 MB",        "37MB",        "37MB",      0,2,    0,2   },
    { UAMEASUNIT_CONCENTRATION_MILLIGRAM_PER_DECILITER,           120.0, "120.00 milligrams per deciliter", "120 milligrams per deciliter", "120 mg/dL", "120.0 mg/dL", "120mg/dL", "120mg/dL", 0,3, 0,3   },
    { UAMEASUNIT_CONCENTRATION_MILLIGRAM_OFGLUCOSE_PER_DECILITER, 120.0, "120.00 milligrams per deciliter", "120 milligrams per deciliter", "120 mg/dL", "120.0 mg/dL", "120mg/dL", "120mg/dL", 0,3, 0,3   },
    { (UAMeasureUnit)0, 0, NULL, NULL, NULL, NULL, NULL, NULL }
};

static const SingleUnitFormat en_GB_singFmt[] = {
//    unit                                 value   wide_2                       wide_0                    shrt_X              shrt_1            narr_0         numr_0       wide_0  narr_0
    { UAMEASUNIT_DURATION_MINUTE,           0.0,   "0.00 minutes",              "0 minutes",              "0 min",            "0.0 min",        "0m",          "0m",        0,1,    0,1   },
    { UAMEASUNIT_DURATION_MINUTE,           1.0,   "1.00 minutes",              "1 minute",               "1 min",            "1.0 min",        "1m",          "1m",        0,1,    0,1   },
    { UAMEASUNIT_DURATION_MINUTE,           5.25,  "5.25 minutes",              "5 minutes",              "5.25 min",         "5.2 min",        "5m",          "5m",        0,1,    0,1   },
    { UAMEASUNIT_LENGTH_CENTIMETER,        37.203, "37.20 centimetres",         "37 centimetres",         "37.203 cm",        "37.2 cm",        "37cm",        "37cm",      0,2,    0,2   },
    { UAMEASUNIT_TEMPERATURE_CELSIUS,      37.203, "37.20 degrees Celsius",     "37 degrees Celsius",     "37.203\\u00B0C",   "37.2\\u00B0C",   "37\\u00B0",   "37\\u00B0", 0,2,    0,2   },
    { UAMEASUNIT_TEMPERATURE_FAHRENHEIT,   37.203, "37.20 degrees Fahrenheit",  "37 degrees Fahrenheit",  "37.203\\u00B0F",   "37.2\\u00B0F",   "37\\u00B0F",  "37\\u00B0", 0,2,    0,2   },
    { UAMEASUNIT_TEMPERATURE_GENERIC,      37.203, "37.20 degrees",             "37 degrees",             "37.203\\u00B0",    "37.2\\u00B0",    "37\\u00B0",   "37\\u00B0", 0,2,    0,2   },
    { UAMEASUNIT_VOLUME_LITER,             37.203, "37.20 litres",              "37 litres",              "37.203 l",         "37.2 l",         "37l",         "37l",       0,2,    0,2   },
    { UAMEASUNIT_ENERGY_FOODCALORIE,       37.203, "37.20 Calories",            "37 Calories",            "37.203 Cal",       "37.2 Cal",       "37Cal",       "37Cal",     0,2,    0,2   },
    { (UAMeasureUnit)0, 0, NULL, NULL, NULL, NULL, NULL, NULL }
};

static const SingleUnitFormat en_AU_singFmt[] = {
//    unit                                 value   wide_2                       wide_0                    shrt_X              shrt_1            narr_0         numr_0       wide_0  narr_0
    { UAMEASUNIT_ENERGY_KILOCALORIE,       1.0,    "1.00 kilocalories",         "1 kilocalorie",          "1 kcal",           "1.0 kcal",       "1kcal",       "1kcal",     0,1,    0,1   },
    { UAMEASUNIT_ENERGY_KILOCALORIE,       37.203, "37.20 kilocalories",        "37 kilocalories",        "37.203 kcal",      "37.2 kcal",      "37kcal",      "37kcal",    0,2,    0,2   },
    { UAMEASUNIT_ENERGY_FOODCALORIE,       1.0,    "1.00 Calories",             "1 Calorie",              "1 Cal",            "1.0 Cal",        "1Cal",        "1Cal",      0,1,    0,1   },
    { UAMEASUNIT_ENERGY_FOODCALORIE,       37.203, "37.20 Calories",            "37 Calories",            "37.203 Cal",       "37.2 Cal",       "37Cal",       "37Cal",     0,2,    0,2   },
    { (UAMeasureUnit)0, 0, NULL, NULL, NULL, NULL, NULL, NULL }
};

static const SingleUnitFormat fr_singFmt[] = {
//    unit                                 value   wide_2                           wide_0                        shrt_X              shrt_1            narr_0         numr_0      wide_0  narr_0
    { UAMEASUNIT_DURATION_MINUTE,           0.0,   "0,00 minute",                   "0 minute",                   "0\\u00A0min",      "0,0\\u00A0min",  "0min",        "0min",      0,1,    0,1   },
    { UAMEASUNIT_DURATION_MINUTE,           1.0,   "1,00 minute",                   "1 minute",                   "1\\u00A0min",      "1,0\\u00A0min",  "1min",        "1min",      0,1,    0,1   },
    { UAMEASUNIT_DURATION_MINUTE,           5.25,  "5,25 minutes",                  "5 minutes",                  "5,25\\u00A0min",   "5,2\\u00A0min",  "5min",        "5min",      0,1,    0,1   },
    { UAMEASUNIT_LENGTH_CENTIMETER,        37.203, "37,20\\u00A0centim\\u00E8tres", "37\\u00A0centim\\u00E8tres", "37,203\\u202Fcm",  "37,2\\u202Fcm",  "37\\u202Fcm",  "37\\u202Fcm", 0,2,    0,2   },
    { UAMEASUNIT_TEMPERATURE_CELSIUS,      37.203, "37,20\\u00A0degr\\u00E9s Celsius","37\\u00A0degr\\u00E9s Celsius", "37,203\\u202F\\u00B0C", "37,2\\u202F\\u00B0C",  "37\\u00B0C",  "37\\u00B0", 0,2,    0,2   },
    { UAMEASUNIT_TEMPERATURE_FAHRENHEIT,   37.203, "37,20\\u00A0degr\\u00E9s Fahrenheit", "37\\u00A0degr\\u00E9s Fahrenheit", "37,203\\u202F\\u00B0F",  "37,2\\u202F\\u00B0F",  "37\\u00B0F",  "37\\u00B0", 0,2,    0,2   },
    { UAMEASUNIT_TEMPERATURE_GENERIC,      37.203, "37,20\\u00A0degr\\u00E9s",      "37\\u00A0degr\\u00E9s",      "37,203\\u00B0",    "37,2\\u00B0",    "37\\u00B0",   "37\\u00B0", 0,2,    0,2   },
    { UAMEASUNIT_VOLUME_LITER,             37.203, "37,20\\u00A0litres",            "37\\u00A0litres",            "37,203\\u202Fl",   "37,2\\u202Fl",   "37\\u202Fl",  "37\\u202Fl", 0,2,    0,2   },
    { UAMEASUNIT_ENERGY_FOODCALORIE,       37.203, "37,20\\u00A0kilocalories",      "37\\u00A0kilocalories",      "37,203 kcal",      "37,2 kcal",      "37kcal","37kcal", 0,2, 0,2   },
    { (UAMeasureUnit)0, 0, NULL, NULL, NULL, NULL, NULL, NULL }
};

static const SingleUnitFormat fr_CA_singFmt[] = {
//    unit                                 value   wide_2                           wide_0                        shrt_X              shrt_1            narr_0         numr_0      wide_0  narr_0
    { UAMEASUNIT_DURATION_MINUTE,           0.0,   "0,00 minute",                   "0 minute",                   "0 min",            "0,0 min",        "0m",          "0m",        0,1,    0,1   },
    { UAMEASUNIT_DURATION_MINUTE,           1.0,   "1,00 minute",                   "1 minute",                   "1 min",            "1,0 min",        "1m",          "1m",        0,1,    0,1   },
    { UAMEASUNIT_DURATION_MINUTE,           5.25,  "5,25 minutes",                  "5 minutes",                  "5,25 min",         "5,2 min",        "5m",          "5m",        0,1,    0,1   },
    { UAMEASUNIT_LENGTH_CENTIMETER,        37.203, "37,20 centim\\u00E8tres",       "37 centim\\u00E8tres",       "37,203 cm",        "37,2 cm",        "37\\u202Fcm", "37\\u202Fcm", 0,2,    0,2   },
    { UAMEASUNIT_TEMPERATURE_CELSIUS,      37.203, "37,20 degr\\u00E9s Celsius",    "37 degr\\u00E9s Celsius",    "37,203\\u00A0\\u00B0C", "37,2\\u00A0\\u00B0C", "37\\u00A0\\u00B0C", "37\\u00B0", 0,2, 0,2 },
    { UAMEASUNIT_TEMPERATURE_FAHRENHEIT,   37.203, "37,20 degr\\u00E9s Fahrenheit", "37 degr\\u00E9s Fahrenheit", "37,203\\u00A0\\u00B0F", "37,2\\u00A0\\u00B0F", "37\\u00B0F", "37\\u00B0", 0,2, 0,2 },
    { UAMEASUNIT_TEMPERATURE_GENERIC,      37.203, "37,20\\u00B0",                  "37\\u00B0",                  "37,203\\u00B0",    "37,2\\u00B0",    "37\\u00B0",   "37\\u00B0", 0,2,    0,2   },
    { UAMEASUNIT_VOLUME_LITER,             37.203, "37,20 litres",                  "37 litres",                  "37,203\\u202FL",         "37,2\\u202FL",         "37L",         "37L",       0,2,    0,2   },
    { UAMEASUNIT_ENERGY_FOODCALORIE,       37.203, "37,20 calories",                "37 calories",                "37,203 cal",       "37,2 cal",       "37 cal",      "37 cal",    0,2,    0,2   },
    { (UAMeasureUnit)0, 0, NULL, NULL, NULL, NULL, NULL, NULL }
};

static const SingleUnitFormat zh_Hant_singFmt[] = {
//    unit                                 value   wide_2                           wide_0                        shrt_X                  shrt_1                narr_0              numr_0            wide_0  narr_0
    { UAMEASUNIT_DURATION_MINUTE,           5.25,  "5.25\\u5206\\u9418",            "5\\u5206\\u9418",            "5.25\\u5206\\u9418",   "5.2\\u5206\\u9418",  "5\\u5206",         "5\\u5206",         0,1,  0,1 },
    { UAMEASUNIT_LENGTH_CENTIMETER,        37.203, "37.20\\u516C\\u5206",           "37\\u516C\\u5206",           "37.203\\u516C\\u5206", "37.2\\u516C\\u5206", "37\\u516C\\u5206", "37\\u516C\\u5206", 0,2,  0,2 },
    { UAMEASUNIT_TEMPERATURE_GENERIC,      37.203, "37.20\\u00B0",                  "37\\u00B0",                  "37.203\\u00B0",        "37.2\\u00B0",        "37\\u00B0",        "37\\u00B0",        0,2,  0,2 },
    { UAMEASUNIT_ANGLE_DEGREE,             37.203, "37.20\\u5EA6",                  "37\\u5EA6",                  "37.203\\u00B0",        "37.2\\u00B0",        "37\\u00B0",        "37\\u00B0",        0,2,  0,2 },
    { (UAMeasureUnit)0, 0, NULL, NULL, NULL, NULL, NULL, NULL }
};

static const SingleUnitFormat cs_singFmt[] = {
//    unit                                 value   wide_2                           wide_0                        shrt_X              shrt_1            narr_0         numr_0      wide_0  narr_0
    { UAMEASUNIT_DURATION_MINUTE,           0.0,   "0,00 minuty",                   "0 minut",                    "0\\u00A0min",  "0,0\\u00A0min",  "0\\u00A0m",   "0\\u00A0m",      0,1,    0,1   }, // other for integer, else many
    { UAMEASUNIT_DURATION_MINUTE,           1.0,   "1,00 minuty",                   "1 minuta",                   "1\\u00A0min",  "1,0\\u00A0min",  "1\\u00A0m",   "1\\u00A0m",      0,1,    0,1   }, // one for integer, else many
    { UAMEASUNIT_DURATION_MINUTE,           2.0,   "2,00 minuty",                   "2 minuty",                   "2\\u00A0min",  "2,0\\u00A0min",  "2\\u00A0m",   "2\\u00A0m",      0,1,    0,1   }, // few for integer, else many
    { UAMEASUNIT_DURATION_MINUTE,           8.5,   "8,50 minuty",                   "8 minut",                  "8,5\\u00A0min",  "8,5\\u00A0min",  "8\\u00A0m",   "8\\u00A0m",      0,1,    0,1   }, // other for integer, else many
    { (UAMeasureUnit)0, 0, NULL, NULL, NULL, NULL, NULL, NULL }
};

static const SingleUnitFormat da_singFmt[] = {
//    unit                                 value   wide_2                           wide_0                        shrt_X              shrt_1            narr_0         numr_0      wide_0  narr_0
    { UAMEASUNIT_SPEED_KILOMETER_PER_HOUR,  1.1,   "1,10 kilometer i timen",        "1 kilometer i timen",        "1,1 km/t.",        "1,1 km/t.",      "1 km/t",      "1 km/t",   0,1,    0,1   }, // other for integer, else many
    { UAMEASUNIT_SPEED_KILOMETER_PER_HOUR,  2.0,   "2,00 kilometer i timen",        "2 kilometer i timen",        "2 km/t.",          "2,0 km/t.",      "2 km/t",      "2 km/t",   0,1,    0,1   }, // other for integer, else many
    { (UAMeasureUnit)0, 0, NULL, NULL, NULL, NULL, NULL, NULL }
};

static const SingleUnitFormat fi_singFmt[] = {
//    unit                                    value   wide_2                           wide_0                        shrt_X              shrt_1            narr_0         numr_0      wide_0  narr_0
    { UAMEASUNIT_DURATION_HOUR,               37.203, "37,20 tuntia",               "37 tuntia",             "37,203 t",              "37,2 t",                 "37 t",       "37 t",      0,2,    0,2 },
    { UAMEASUNIT_DURATION_MINUTE,             37.203, "37,20 minuuttia",            "37 minuuttia",          "37,203 min",            "37,2 min",            "37 min",       "37 min",      0,2,    0,2 },
    { UAMEASUNIT_LENGTH_METER ,               37.203, "37,20 metri\\u00e4",         "37 metri\\u00e4",       "37,203 m",              "37,2 m",                 "37\\u202fm","37\\u202fm",  0,2,    0,2 },
    { UAMEASUNIT_LENGTH_KILOMETER,            37.203, "37,20 kilometri\\u00e4",     "37 kilometri\\u00e4",   "37,203 km",           "37,2 km",               "37\\u202fkm",  "37\\u202fkm", 0,2,    0,2 },
    { (UAMeasureUnit)0, 0, NULL, NULL, NULL, NULL, NULL, NULL }
};

static const SingleUnitFormat nl_singFmt[] = {
//    unit                                 value   wide_2                       wide_0                  shrt_X                   shrt_1                 narr_0        numr_0       wide_0  narr_0
    { UAMEASUNIT_TEMPERATURE_CELSIUS,      37.203, "37,20 graden Celsius",      "37 graden Celsius",    "37,203\\u00A0\\u00B0C", "37,2\\u00A0\\u00B0C", "37\\u00B0",  "37\\u00B0", 0,2,    0,2 },
    { UAMEASUNIT_TEMPERATURE_FAHRENHEIT,   37.203, "37,20 graden Fahrenheit",   "37 graden Fahrenheit", "37,203\\u00A0\\u00B0F", "37,2\\u00A0\\u00B0F", "37\\u00B0F", "37\\u00B0", 0,2,    0,2 },
    { (UAMeasureUnit)0, 0, NULL, NULL, NULL, NULL, NULL, NULL }
};

static const SingleUnitFormat he_singFmt[] = {
//    unit                                 value   wide_2                                      wide_0                                   shrt_X                                       shrt_1                                     narr_0                                   numr_0                                   wide_0  narr_0
    { UAMEASUNIT_LENGTH_KILOMETER,         37.203, "37.20\\u202f\\u05e7\\u05f4\\u05de",        "37\\u202f\\u05e7\\u05f4\\u05de",        "37.203\\u202f\\u05e7\\u05f4\\u05de",        "37.2\\u202f\\u05e7\\u05f4\\u05de",        "37\\u202f\\u05e7\\u05f4\\u05de",        "37\\u202f\\u05e7\\u05f4\\u05de",        0,2,    0,2 },
    { UAMEASUNIT_LENGTH_MILE,              37.203, "37.20\\u202f\\u05de\\u05d9\\u05d9\\u05dc", "37\\u202f\\u05de\\u05d9\\u05d9\\u05dc", "37.203\\u202f\\u05de\\u05d9\\u05d9\\u05dc", "37.2\\u202f\\u05de\\u05d9\\u05d9\\u05dc", "37\\u202f\\u05de\\u05d9\\u05d9\\u05dc", "37\\u202f\\u05de\\u05d9\\u05d9\\u05dc", 0,2,    0,2 },
    { (UAMeasureUnit)0, 0, NULL, NULL, NULL, NULL, NULL, NULL }
};

static const SingleUnitFormat hi_singFmt[] = {
//    unit                                 value   wide_2                                                                                                              wide_0                                                                                                           shrt_X           shrt_1          narr_0       numr_0      wide_0  narr_0
    { UAMEASUNIT_TEMPERATURE_CELSIUS,      37.203, "37.20 \\u0921\\u093F\\u0917\\u094D\\u0930\\u0940 \\u0938\\u0947\\u0932\\u094D\\u0938\\u093F\\u092F\\u0938",        "37 \\u0921\\u093F\\u0917\\u094D\\u0930\\u0940 \\u0938\\u0947\\u0932\\u094D\\u0938\\u093F\\u092F\\u0938",        "37.203\\u2103", "37.2\\u2103", "37\\u2103", "37\\u00B0", 0,2,    0,2 },
    { UAMEASUNIT_TEMPERATURE_FAHRENHEIT,   37.203, "37.20 \\u0921\\u093F\\u0917\\u094D\\u0930\\u0940 \\u092B\\u093C\\u0947\\u0930\\u0928\\u0939\\u093E\\u0907\\u091F", "37 \\u0921\\u093F\\u0917\\u094D\\u0930\\u0940 \\u092B\\u093C\\u0947\\u0930\\u0928\\u0939\\u093E\\u0907\\u091F", "37.203\\u2109", "37.2\\u2109", "37\\u2109", "37\\u00B0", 0,2,    0,2 },
    { UAMEASUNIT_TEMPERATURE_GENERIC,      37.203, "37.20\\u00B0",                                                                                                     "37\\u00B0",                                                                                                     "37.203\\u00B0", "37.2\\u00B0", "37\\u00B0", "37\\u00B0", 0,2,    0,2 },
    { (UAMeasureUnit)0, 0, NULL, NULL, NULL, NULL, NULL, NULL }
};

static const SingleUnitFormat it_singFmt[] = {
//    unit                                 value   wide_2                       wide_0                  shrt_X                   shrt_1                 narr_0        numr_0       wide_0  narr_0
    { UAMEASUNIT_DURATION_HOUR,            37.203, "37,20 ore",                 "37 ore",               "37,203 h",              "37,2 h", 				"37 h",       "37 h",      0,2,    0,2 },
    { UAMEASUNIT_DURATION_MINUTE,          37.203, "37,20 minuti",              "37 minuti",            "37,203 min",            "37,2 min",            "37 min",     "37 min",    0,2,    0,2 },
    { (UAMeasureUnit)0, 0, NULL, NULL, NULL, NULL, NULL, NULL }
};

static const SingleUnitFormat nb_singFmt[] = {
//    unit                                 value   wide_2                       wide_0                  shrt_X                   shrt_1                 narr_0        numr_0       wide_0  narr_0
    { UAMEASUNIT_DURATION_HOUR,            37.203, "37,20 timer",               "37 timer",             "37,203 t",              "37,2 t", 				"37\\u202Ft",       "37\\u202Ft",      0,2,    0,2 },
    { UAMEASUNIT_DURATION_MINUTE,          37.203, "37,20 minutter",            "37 minutter",          "37,203 min",            "37,2 min",            "37\\u202Fm",       "37\\u202Fm",      0,2,    0,2 },
    { UAMEASUNIT_LENGTH_INCH,              1.0,    "1,00 tomme",                "1 tomme",              "1 to",                  "1,0 to",              "1 to",       "1 to",      0,1,    0,1 },
    { UAMEASUNIT_LENGTH_INCH,              37.203, "37,20 tommer",              "37 tommer",            "37,203 to",             "37,2 to",             "37 to",      "37 to",     0,2,    0,2 },
    { (UAMeasureUnit)0, 0, NULL, NULL, NULL, NULL, NULL, NULL }
};

static const SingleUnitFormat pl_singFmt[] = {
//    unit                  value  wide_2        wide_0            shrt_X            shrt_1       narr_0   numr_0   wide_0 narr_0
    // rdar://84870068
    { UAMEASUNIT_DURATION_DAY,  1, "1,00 dnia",  "1 dzie\\u0144",  "1 dzie\\u0144",  "1,0 dnia",  "1 d.",  "1 d.",  0,1,   0,1 },
    { UAMEASUNIT_DURATION_DAY,  2, "2,00 dnia",  "2 dni",          "2 dni",          "2,0 dnia",  "2 dn.", "2 dn.", 0,1,   0,1 },
    { (UAMeasureUnit)0, 0, NULL, NULL, NULL, NULL, NULL, NULL }
};

static const SingleUnitFormat ro_singFmt[] = {
//    unit                                 value   wide_2                       wide_0                  shrt_X                   shrt_1                 narr_0        numr_0       wide_0  narr_0
    { UAMEASUNIT_DURATION_HOUR,            37.203, "37,20 ore",                 "37 de ore",            "37,203 ore",            "37,2 ore", 			"37 h",       "37 h",      0,2,    0,2 },
    { UAMEASUNIT_DURATION_MINUTE,          37.203, "37,20 minute",              "37 de minute",         "37,203 min.",           "37,2 min.",           "37 m",       "37 m",      0,2,    0,2 },
    { (UAMeasureUnit)0, 0, NULL, NULL, NULL, NULL, NULL, NULL }
};

static const SingleUnitFormat ru_singFmt[] = {
//    unit                                 value wide_2                                                             wide_0                                                          shrt_X                      shrt_1                        narr_0                      numr_0                      wide_0  narr_0
    { UAMEASUNIT_DURATION_DAY,             2.0,	 "2,00 \\u0434\\u043D\\u044F",               				    	"2 \\u0434\\u043D\\u044F",    		           			        "2 \\u0434\\u043D",         "2,0 \\u0434\\u043D",  	      "2 \\u0434\\u002E",         "2 \\u0434\\u002E",         0,1,    0,1 }, // rdar://107186120
    { UAMEASUNIT_SPEED_MILE_PER_HOUR,      1.0,  "1,00 \\u043C\\u0438\\u043B\\u0438 \\u0432 \\u0447\\u0430\\u0441", "1 \\u043C\\u0438\\u043B\\u044F \\u0432 \\u0447\\u0430\\u0441", "1 \\u043C\\u0438/\\u0447", "1,0 \\u043C\\u0438/\\u0447", "1 \\u043C\\u0438/\\u0447", "1 \\u043C\\u0438/\\u0447", 0,1,    0,1 },
    { UAMEASUNIT_SPEED_MILE_PER_HOUR,      2.0,  "2,00 \\u043C\\u0438\\u043B\\u0438 \\u0432 \\u0447\\u0430\\u0441", "2 \\u043C\\u0438\\u043B\\u0438 \\u0432 \\u0447\\u0430\\u0441", "2 \\u043C\\u0438/\\u0447", "2,0 \\u043C\\u0438/\\u0447", "2 \\u043C\\u0438/\\u0447", "2 \\u043C\\u0438/\\u0447", 0,1,    0,1 },
    { UAMEASUNIT_SPEED_MILE_PER_HOUR,      5.0,  "5,00 \\u043C\\u0438\\u043B\\u0438 \\u0432 \\u0447\\u0430\\u0441", "5 \\u043C\\u0438\\u043B\\u044C \\u0432 \\u0447\\u0430\\u0441", "5 \\u043C\\u0438/\\u0447", "5,0 \\u043C\\u0438/\\u0447", "5 \\u043C\\u0438/\\u0447", "5 \\u043C\\u0438/\\u0447", 0,1,    0,1 },
    { (UAMeasureUnit)0, 0, NULL, NULL, NULL, NULL, NULL, NULL }
};

static const SingleUnitFormat uk_singFmt[] = {
//    unit                                 value   wide_2                                                     wide_0                                           shrt_X                      shrt_1                        narr_0                     numr_0                    wide_0  narr_0
    { UAMEASUNIT_LENGTH_FOOT,               1.0,   "1,00 \\u0444\\u0443\\u0442\\u0430",                       "1 \\u0444\\u0443\\u0442",                       "1 \\u0444\\u0442",         "1,0 \\u0444\\u0442",         "1\\u0444\\u0442",        "1\\u0444\\u0442",  0,1,    0,1 },
    { UAMEASUNIT_LENGTH_FOOT,               10.0,  "10,00 \\u0444\\u0443\\u0442\\u0430",                      "10 \\u0444\\u0443\\u0442\\u0456\\u0432",        "10 \\u0444\\u0442",        "10,0 \\u0444\\u0442",        "10\\u0444\\u0442",       "10\\u0444\\u0442", 0,2,    0,2 },
    { UAMEASUNIT_DURATION_HOUR,             10.0,  "10,00 \\u0433\\u043e\\u0434\\u0438\\u043d\\u0438",        "10 \\u0433\\u043e\\u0434\\u0438\\u043d",        "10 \\u0433\\u043e\\u0434", "10,0 \\u0433\\u043e\\u0434", "10\\u202f\\u0433",        "10\\u202f\\u0433", 0,2,    0,2 },
    { UAMEASUNIT_DURATION_MINUTE,           10.0,  "10,00 \\u0445\\u0432\\u0438\\u043b\\u0438\\u043d\\u0438", "10 \\u0445\\u0432\\u0438\\u043b\\u0438\\u043d", "10 \\u0445\\u0432",        "10,0 \\u0445\\u0432",        "10\\u202f\\u0445\\u0432", "10\\u202f\\u0445\\u0432", 0,2,    0,2 },
    { UAMEASUNIT_DURATION_SECOND,           10.0,  "10,00 \\u0441\\u0435\\u043a\\u0443\\u043d\\u0434\\u0438", "10 \\u0441\\u0435\\u043a\\u0443\\u043d\\u0434", "10 \\u0441",               "10,0 \\u0441",               "10\\u202f\\u0441",        "10\\u202f\\u0441", 0,2,    0,2 },
    { (UAMeasureUnit)0, 0, NULL, NULL, NULL, NULL, NULL, NULL }
};

static const SingleUnitFormat vi_singFmt[] = {
//    unit                                 value   wide_2                       wide_0                  shrt_X          shrt_1              narr_0          numr_0          wide_0  narr_0
    { UAMEASUNIT_ENERGY_KILOCALORIE,       123.0,  "123,00 kil\\u00F4calo",     "123 kil\\u00F4calo",   "123 kcal",     "123,0 kcal",       "123 kcal",     "123 kcal",     0,3,    0,3 },
    { UAMEASUNIT_ENERGY_KILOJOULE,         123.0,  "123,00 kil\\u00F4jun",      "123 kil\\u00F4jun",    "123 kJ",       "123,0 kJ",         "123 kJ",       "123 kJ",       0,3,    0,3 },
    { (UAMeasureUnit)0, 0, NULL, NULL, NULL, NULL, NULL, NULL }
};

static const SingleUnitFormat zh_singFmt[] = {
//    unit                                 value   wide_2                                wide_0                             shrt_X                          shrt_1                        narr_0                      numr_0                      wide_0  narr_0
    { UAMEASUNIT_DIGITAL_GIGABYTE,         37.203, "37.20\\u5409\\u5B57\\u8282",         "37\\u5409\\u5B57\\u8282",         "37.203 GB",                    "37.2 GB",                    "37 GB",                    "37 GB",                    0,2,    0,2   },
    { UAMEASUNIT_SPEED_KILOMETER_PER_HOUR, 37.203, "37.20\\u516C\\u91CC/\\u5C0F\\u65F6", "37\\u516C\\u91CC/\\u5C0F\\u65F6", "37.203\\u516C\\u91CC/\\u65F6", "37.2\\u516C\\u91CC/\\u65F6", "37\\u516C\\u91CC/\\u65F6", "37\\u516C\\u91CC/\\u65F6", 0,2,    0,2   },
    { UAMEASUNIT_TEMPERATURE_FAHRENHEIT,   37.203, "37.20\\u534E\\u6C0F\\u5EA6",         "37\\u534E\\u6C0F\\u5EA6",         "37.203\\u00B0F",               "37.2\\u00B0F",               "37\\u00B0F",               "37\\u00B0",                0,2,    0,2   },
    { (UAMeasureUnit)0, 0, NULL, NULL, NULL, NULL, NULL, NULL }
};

static const SingleUnitFormat zh_Hant_HK_singFmt[] = {
//    unit                                 value   wide_2                                wide_0                             shrt_X              shrt_1            narr_0        numr_0       wide_0  narr_0
    { UAMEASUNIT_SPEED_KILOMETER_PER_HOUR, 37.203, "\\u6bcf\\u5c0f\\u664237.20\\u516c\\u91cc", "\\u6bcf\\u5c0f\\u664237\\u516c\\u91cc", "37.203\\u516c\\u91cc/\\u5c0f\\u6642",      "37.2\\u516c\\u91cc/\\u5c0f\\u6642",      "37km/h",     "37km/h",    3,5,    0,2   },
    { UAMEASUNIT_SPEED_MILE_PER_HOUR,      37.203, "\\u6bcf\\u5c0f\\u664237.20\\u82f1\\u91cc", "\\u6bcf\\u5c0f\\u664237\\u82f1\\u91cc", "37.203\\u54e9/\\u5c0f\\u6642",      "37.2\\u54e9/\\u5c0f\\u6642",      "37mph",     "37mph",    3,5,    0,2   },
    { (UAMeasureUnit)0, 0, NULL, NULL, NULL, NULL, NULL, NULL }
};

static const SingleUnitName en_singNam[] = {
//    unit                                 wide                   shrt         narr
    { UAMEASUNIT_DURATION_MINUTE,          "minutes",             "min",       "min"       },
    { UAMEASUNIT_DURATION_CENTURY,         "centuries",           "c",         "c"         },
    { UAMEASUNIT_LENGTH_CENTIMETER,        "centimeters",         "cm",        "cm"        },
    { UAMEASUNIT_SPEED_KILOMETER_PER_HOUR, "kilometers per hour", "km/hour",   "km/hr"     },
    { UAMEASUNIT_TEMPERATURE_CELSIUS,      "degrees Celsius",     "deg. C",    "\\u00B0C"  },
    { UAMEASUNIT_TEMPERATURE_FAHRENHEIT,   "degrees Fahrenheit",  "deg. F",    "\\u00B0F"  },
    { UAMEASUNIT_TEMPERATURE_GENERIC,      "degrees",             "deg.",      "deg."      },
    { UAMEASUNIT_VOLUME_LITER,             "liters",              "liters",    "liter"     },
    { UAMEASUNIT_ENERGY_FOODCALORIE,       "Calories",            "Cal",       "Cal"       },
    { UAMEASUNIT_ENERGY_JOULE,             "joules",              "joules",    "joule"     },
    { UAMEASUNIT_DIGITAL_MEGABYTE,         "megabytes",           "MByte",     "MB"        },
    { (UAMeasureUnit)0, NULL, NULL, NULL }
};

static const SingleUnitName en_GB_singNam[] = {
//    unit                                 wide                   shrt         narr
    { UAMEASUNIT_DURATION_MINUTE,          "minutes",             "min",       "min"       },
    { UAMEASUNIT_LENGTH_CENTIMETER,        "centimetres",         "cm",        "cm"        },
    { UAMEASUNIT_TEMPERATURE_CELSIUS,      "degrees Celsius",     "deg. C",    "\\u00B0C"  },
    { UAMEASUNIT_TEMPERATURE_FAHRENHEIT,   "degrees Fahrenheit",  "deg. F",    "\\u00B0F"  },
    { UAMEASUNIT_TEMPERATURE_GENERIC,      "degree",              "deg.",      "deg."      },
    { UAMEASUNIT_VOLUME_LITER,             "litres",              "litres",    "litre"     },
    { UAMEASUNIT_ENERGY_FOODCALORIE,       "Calories",            "Cal",       "Cal"       },
    { (UAMeasureUnit)0, NULL, NULL, NULL }
};

static const SingleUnitName zh_singNam[] = {
//    unit                                 wide                             shrt                             narr
    { UAMEASUNIT_DIGITAL_GIGABYTE,         "\\u5409\\u5B57\\u8282",         "GB",                            "GB" },
    { UAMEASUNIT_SPEED_KILOMETER_PER_HOUR, "\\u516C\\u91CC/\\u5C0F\\u65F6", "\\u516C\\u91CC/\\u5C0F\\u65F6", "\\u516C\\u91CC/\\u5C0F\\u65F6" },
    { UAMEASUNIT_TEMPERATURE_FAHRENHEIT,   "\\u534E\\u6C0F\\u5EA6",         "\\u00B0F",                      "\\u00B0F" },
    { (UAMeasureUnit)0, NULL, NULL, NULL }
};

static const SingleUnitName zh_Hant_singNam[] = {
//    unit                                 wide                            shrt              narr
    { UAMEASUNIT_TEMPERATURE_FAHRENHEIT,   "\\u83EF\\u6C0F\\u5EA6\\u6578", "\\u83EF\\u6C0F", "\\u00B0F"  },
    { (UAMeasureUnit)0, NULL, NULL, NULL }
};

static const SingleUnitName zh_Hant_HK_singNam[] = {
//    unit                                 wide                                   shrt                                   narr
    { UAMEASUNIT_TEMPERATURE_FAHRENHEIT,   "\\u83EF\\u6C0F\\u5EA6",               "\\u00B0F",                           "\\u00B0F"  },
    { UAMEASUNIT_SPEED_KILOMETER_PER_HOUR, "\\u516c\\u91cc\\u6bcf\\u5c0f\\u6642", "\\u516c\\u91cc\\u6bcf\\u5c0f\\u6642", "\\u516c\\u91cc\\u6bcf\\u5c0f\\u6642"          },
    { UAMEASUNIT_SPEED_MILE_PER_HOUR,      "\\u82f1\\u91cc\\u6bcf\\u5c0f\\u6642", "\\u82f1\\u91cc\\u6bcf\\u5c0f\\u6642", "\\u82f1\\u91cc\\u6bcf\\u5c0f\\u6642"          },
    { (UAMeasureUnit)0, NULL, NULL, NULL }
};

static const SingleUnitName fo_singNam[] = {
//    unit                               wide               shrt        narr
    { UAMEASUNIT_TEMPERATURE_CELSIUS,    "stig Celsius",    "\\u00B0C", "\\u00B0C" }, // rdar://115772075
    { UAMEASUNIT_TEMPERATURE_FAHRENHEIT, "stig Fahrenheit", "\\u00B0F", "\\u00B0F" }, // rdar://115772075
    { (UAMeasureUnit)0, NULL, NULL, NULL }
};

static const SingleUnitName nl_singNam[] = {
//    unit                               wide                 shrt        narr
    { UAMEASUNIT_TEMPERATURE_CELSIUS,    "graden Celsius",    "\\u00B0C", "\\u00B0C" }, // rdar://115772075
    { UAMEASUNIT_TEMPERATURE_FAHRENHEIT, "graden Fahrenheit", "\\u00B0F", "\\u00B0F" }, // rdar://115772075
    { (UAMeasureUnit)0, NULL, NULL, NULL }
};

static const SingleUnitName pl_singNam[] = {
//    unit                     wide                 shrt   narr
    { UAMEASUNIT_SPEED_KNOT,   "w\\u0119z\\u0142y", "w.",  "w."  },
    { UAMEASUNIT_DURATION_DAY, "dni",               "dni", "dzie\\u0144"  },  // rdar://84870068
    { (UAMeasureUnit)0, NULL, NULL, NULL }
};

static const UAMeasure meas_hrMnSc[] = { {37.3,UAMEASUNIT_DURATION_HOUR}, {12.1,UAMEASUNIT_DURATION_MINUTE},  {5.32,UAMEASUNIT_DURATION_SECOND} };
static const UAMeasure meas_hrMn[]   = { {37.3,UAMEASUNIT_DURATION_HOUR}, {12.1,UAMEASUNIT_DURATION_MINUTE}   };
static const UAMeasure meas_mCm[]    = { {37.3,UAMEASUNIT_LENGTH_METER},  {12.1,UAMEASUNIT_LENGTH_CENTIMETER} };
static const UAMeasure meas_cm[]     = { {12.1,UAMEASUNIT_LENGTH_CENTIMETER} };
static const UAMeasure meas_2hrMn[]  = { {2.0,UAMEASUNIT_DURATION_HOUR}, {12.1,UAMEASUNIT_DURATION_MINUTE}   };
static const UAMeasure meas_moDys[]  = { {1.0,UAMEASUNIT_DURATION_MONTH}, {2.0,UAMEASUNIT_DURATION_DAY}   };

static const MultipleUnitFormat en_multFmt[] = {
//    measures     count                       wide_2                                wide_0                             shrt_X                        shrt_1                       shrtr_1                 narr_0        numr_0      numr_2
    { meas_hrMnSc, UPRV_LENGTHOF(meas_hrMnSc), "37 hours, 12 minutes, 5.32 seconds", "37 hours, 12 minutes, 5 seconds", "37 hr, 12 min, 5.32 sec",    "37 hr, 12 min, 5.3 sec",    "37hr 12min 5.3sec",    "37h 12m 5s", "37:12:05", "37:12:05.32",
        /* ranges_wide_2: */                {{UAMEASUNIT_DURATION_HOUR,0,8},{UAMEASUNIT_DURATION_HOUR|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_DURATION_MINUTE,10,20},{UAMEASUNIT_DURATION_MINUTE|UAMEASFMT_NUMERIC_FIELD_FLAG,10,12},{UAMEASUNIT_DURATION_SECOND,22,34},{UAMEASUNIT_DURATION_SECOND|UAMEASFMT_NUMERIC_FIELD_FLAG,22,26}},
        /* ranges_shrtr_1: */               {{UAMEASUNIT_DURATION_HOUR,0,4},{UAMEASUNIT_DURATION_HOUR|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_DURATION_MINUTE, 5,10},{UAMEASUNIT_DURATION_MINUTE|UAMEASFMT_NUMERIC_FIELD_FLAG, 5, 7},{UAMEASUNIT_DURATION_SECOND,11,17},{UAMEASUNIT_DURATION_SECOND|UAMEASFMT_NUMERIC_FIELD_FLAG,11,14}},
        /* ranges_numr_0: */                {{UAMEASUNIT_DURATION_HOUR,0,2},{UAMEASUNIT_DURATION_HOUR|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_DURATION_MINUTE, 3, 5},{UAMEASUNIT_DURATION_MINUTE|UAMEASFMT_NUMERIC_FIELD_FLAG, 3, 5},{UAMEASUNIT_DURATION_SECOND, 6, 8},{UAMEASUNIT_DURATION_SECOND|UAMEASFMT_NUMERIC_FIELD_FLAG, 6, 8}},
        /* ranges_numr_2: */                {{UAMEASUNIT_DURATION_HOUR,0,2},{UAMEASUNIT_DURATION_HOUR|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_DURATION_MINUTE, 3, 5},{UAMEASUNIT_DURATION_MINUTE|UAMEASFMT_NUMERIC_FIELD_FLAG, 3, 5},{UAMEASUNIT_DURATION_SECOND, 6, 11},{UAMEASUNIT_DURATION_SECOND|UAMEASFMT_NUMERIC_FIELD_FLAG, 6, 11}} },
    { meas_hrMn,   UPRV_LENGTHOF(meas_hrMn),   "37 hours, 12.10 minutes",            "37 hours, 12 minutes",            "37 hr, 12.1 min",            "37 hr, 12.1 min",           "37hr 12.1min",         "37h 12m",    "37:12",    "37:12.10",
        /* ranges_wide_2: */                {{UAMEASUNIT_DURATION_HOUR,0,8},{UAMEASUNIT_DURATION_HOUR|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_DURATION_MINUTE,10,23},{UAMEASUNIT_DURATION_MINUTE|UAMEASFMT_NUMERIC_FIELD_FLAG,10,15}},
        /* ranges_shrtr_1: */               {{UAMEASUNIT_DURATION_HOUR,0,4},{UAMEASUNIT_DURATION_HOUR|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_DURATION_MINUTE, 5,12},{UAMEASUNIT_DURATION_MINUTE|UAMEASFMT_NUMERIC_FIELD_FLAG, 5, 9}},
        /* ranges_numr_0: */                {{UAMEASUNIT_DURATION_HOUR,0,2},{UAMEASUNIT_DURATION_HOUR|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_DURATION_MINUTE, 3, 5},{UAMEASUNIT_DURATION_MINUTE|UAMEASFMT_NUMERIC_FIELD_FLAG, 3, 5}},
        /* ranges_numr_2: */                {{UAMEASUNIT_DURATION_HOUR,0,2},{UAMEASUNIT_DURATION_HOUR|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_DURATION_MINUTE, 3, 8},{UAMEASUNIT_DURATION_MINUTE|UAMEASFMT_NUMERIC_FIELD_FLAG, 3, 8}} },
    { meas_mCm,    UPRV_LENGTHOF(meas_mCm),    "37 meters, 12.10 centimeters",       "37 meters, 12 centimeters",       "37 m, 12.1 cm",              "37 m, 12.1 cm",             "37m 12.1cm",           "37m 12cm",   "37m 12cm", "37m 12.10cm",
        /* ranges_wide_2: */                {{UAMEASUNIT_LENGTH_METER,0,9},{UAMEASUNIT_LENGTH_METER|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_LENGTH_CENTIMETER,11,28},{UAMEASUNIT_LENGTH_CENTIMETER|UAMEASFMT_NUMERIC_FIELD_FLAG,11,16}},
        /* ranges_shrtr_1: */               {{UAMEASUNIT_LENGTH_METER,0,3},{UAMEASUNIT_LENGTH_METER|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_LENGTH_CENTIMETER, 4,10},{UAMEASUNIT_LENGTH_CENTIMETER|UAMEASFMT_NUMERIC_FIELD_FLAG, 4, 8}},
        /* ranges_numr_0: */                {{UAMEASUNIT_LENGTH_METER,0,3},{UAMEASUNIT_LENGTH_METER|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_LENGTH_CENTIMETER, 4, 8},{UAMEASUNIT_LENGTH_CENTIMETER|UAMEASFMT_NUMERIC_FIELD_FLAG, 4, 6}},
        /* ranges_numr_2: */                {{UAMEASUNIT_LENGTH_METER,0,3},{UAMEASUNIT_LENGTH_METER|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_LENGTH_CENTIMETER, 4, 11},{UAMEASUNIT_LENGTH_CENTIMETER|UAMEASFMT_NUMERIC_FIELD_FLAG, 4, 9}} },
    { meas_cm,     UPRV_LENGTHOF(meas_cm),     "12.10 centimeters",                  "12 centimeters",                  "12.1 cm",                    "12.1 cm",                   "12.1cm",               "12cm",       "12cm",     "12.10cm",
        /* ranges_wide_2: */                {{UAMEASUNIT_LENGTH_CENTIMETER,0,17},{UAMEASUNIT_LENGTH_CENTIMETER|UAMEASFMT_NUMERIC_FIELD_FLAG,0,5}},
        /* ranges_shrtr_1: */               {{UAMEASUNIT_LENGTH_CENTIMETER,0, 6},{UAMEASUNIT_LENGTH_CENTIMETER|UAMEASFMT_NUMERIC_FIELD_FLAG,0,4}},
        /* ranges_numr_0: */                {{UAMEASUNIT_LENGTH_CENTIMETER,0, 4},{UAMEASUNIT_LENGTH_CENTIMETER|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2}},
        /* ranges_numr_2: */                {{UAMEASUNIT_LENGTH_CENTIMETER,0, 7},{UAMEASUNIT_LENGTH_CENTIMETER|UAMEASFMT_NUMERIC_FIELD_FLAG,0,5}} },
    { NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, {{0,0,0}}, {{0,0,0}}, {{0,0,0}}, {{0,0,0}} }
};

static const MultipleUnitFormat en_GB_multFmt[] = {
//    measures     count                       wide_2                                wide_0                             shrt_X                        shrt_1                       shrtr_1                 narr_0        numr_0      numr_2
    { meas_hrMnSc, UPRV_LENGTHOF(meas_hrMnSc), "37 hours, 12 minutes, 5.32 seconds", "37 hours, 12 minutes, 5 seconds", "37 hrs, 12 min, 5.32 secs",  "37 hrs, 12 min, 5.3 secs",  "37hrs 12min 5.3secs",  "37h 12m 5s", "37:12:05", "37:12:05.32",
        /* ranges_wide_2: */                {{UAMEASUNIT_DURATION_HOUR,0,8},{UAMEASUNIT_DURATION_HOUR|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_DURATION_MINUTE,10,20},{UAMEASUNIT_DURATION_MINUTE|UAMEASFMT_NUMERIC_FIELD_FLAG,10,12},{UAMEASUNIT_DURATION_SECOND,22,34},{UAMEASUNIT_DURATION_SECOND|UAMEASFMT_NUMERIC_FIELD_FLAG,22,26}},
        /* ranges_shrtr_1: */               {{UAMEASUNIT_DURATION_HOUR,0,5},{UAMEASUNIT_DURATION_HOUR|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_DURATION_MINUTE, 6,11},{UAMEASUNIT_DURATION_MINUTE|UAMEASFMT_NUMERIC_FIELD_FLAG, 6, 8},{UAMEASUNIT_DURATION_SECOND,12,19},{UAMEASUNIT_DURATION_SECOND|UAMEASFMT_NUMERIC_FIELD_FLAG,12,15}},
        /* ranges_numr_0: */                {{UAMEASUNIT_DURATION_HOUR,0,2},{UAMEASUNIT_DURATION_HOUR|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_DURATION_MINUTE, 3, 5},{UAMEASUNIT_DURATION_MINUTE|UAMEASFMT_NUMERIC_FIELD_FLAG, 3, 5},{UAMEASUNIT_DURATION_SECOND, 6, 8},{UAMEASUNIT_DURATION_SECOND|UAMEASFMT_NUMERIC_FIELD_FLAG, 6, 8}},
        /* ranges_numr_2: */                {{UAMEASUNIT_DURATION_HOUR,0,2},{UAMEASUNIT_DURATION_HOUR|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_DURATION_MINUTE, 3, 5},{UAMEASUNIT_DURATION_MINUTE|UAMEASFMT_NUMERIC_FIELD_FLAG, 3, 5},{UAMEASUNIT_DURATION_SECOND, 6, 11},{UAMEASUNIT_DURATION_SECOND|UAMEASFMT_NUMERIC_FIELD_FLAG, 6, 11}} },
    { meas_hrMn,   UPRV_LENGTHOF(meas_hrMn),   "37 hours, 12.10 minutes",            "37 hours, 12 minutes",            "37 hrs, 12.1 min",           "37 hrs, 12.1 min",          "37hrs 12.1min",        "37h 12m",    "37:12",    "37:12.10",
        /* ranges_wide_2: */                {{UAMEASUNIT_DURATION_HOUR,0,8},{UAMEASUNIT_DURATION_HOUR|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_DURATION_MINUTE,10,23},{UAMEASUNIT_DURATION_MINUTE|UAMEASFMT_NUMERIC_FIELD_FLAG,10,15}},
        /* ranges_shrtr_1: */               {{UAMEASUNIT_DURATION_HOUR,0,5},{UAMEASUNIT_DURATION_HOUR|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_DURATION_MINUTE, 6,13},{UAMEASUNIT_DURATION_MINUTE|UAMEASFMT_NUMERIC_FIELD_FLAG, 6,10}},
        /* ranges_numr_0: */                {{UAMEASUNIT_DURATION_HOUR,0,2},{UAMEASUNIT_DURATION_HOUR|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_DURATION_MINUTE, 3, 5},{UAMEASUNIT_DURATION_MINUTE|UAMEASFMT_NUMERIC_FIELD_FLAG, 3, 5}},
        /* ranges_numr_2: */                {{UAMEASUNIT_DURATION_HOUR,0,2},{UAMEASUNIT_DURATION_HOUR|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_DURATION_MINUTE, 3, 8},{UAMEASUNIT_DURATION_MINUTE|UAMEASFMT_NUMERIC_FIELD_FLAG, 3, 8}} },
    { meas_mCm,    UPRV_LENGTHOF(meas_mCm),    "37 metres, 12.10 centimetres",       "37 metres, 12 centimetres",       "37 m, 12.1 cm",              "37 m, 12.1 cm",             "37m 12.1cm",           "37m 12cm",   "37m 12cm", "37m 12.10cm",
        /* ranges_wide_2: */                {{UAMEASUNIT_LENGTH_METER,0,9},{UAMEASUNIT_LENGTH_METER|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_LENGTH_CENTIMETER,11,28},{UAMEASUNIT_LENGTH_CENTIMETER|UAMEASFMT_NUMERIC_FIELD_FLAG,11,16}},
        /* ranges_shrtr_1: */               {{UAMEASUNIT_LENGTH_METER,0,3},{UAMEASUNIT_LENGTH_METER|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_LENGTH_CENTIMETER, 4,10},{UAMEASUNIT_LENGTH_CENTIMETER|UAMEASFMT_NUMERIC_FIELD_FLAG, 4, 8}},
        /* ranges_numr_0: */                {{UAMEASUNIT_LENGTH_METER,0,3},{UAMEASUNIT_LENGTH_METER|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_LENGTH_CENTIMETER, 4, 8},{UAMEASUNIT_LENGTH_CENTIMETER|UAMEASFMT_NUMERIC_FIELD_FLAG, 4, 6}},
        /* ranges_numr_2: */                {{UAMEASUNIT_LENGTH_METER,0,3},{UAMEASUNIT_LENGTH_METER|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_LENGTH_CENTIMETER, 4, 11},{UAMEASUNIT_LENGTH_CENTIMETER|UAMEASFMT_NUMERIC_FIELD_FLAG, 4, 9}} },
    { meas_cm,     UPRV_LENGTHOF(meas_cm),     "12.10 centimetres",                  "12 centimetres",                  "12.1 cm",                    "12.1 cm",                   "12.1cm",               "12cm",       "12cm",     "12.10cm",
        /* ranges_wide_2: */                {{UAMEASUNIT_LENGTH_CENTIMETER,0,17},{UAMEASUNIT_LENGTH_CENTIMETER|UAMEASFMT_NUMERIC_FIELD_FLAG,0,5}},
        /* ranges_shrtr_1: */               {{UAMEASUNIT_LENGTH_CENTIMETER,0, 6},{UAMEASUNIT_LENGTH_CENTIMETER|UAMEASFMT_NUMERIC_FIELD_FLAG,0,4}},
        /* ranges_numr_0: */                {{UAMEASUNIT_LENGTH_CENTIMETER,0, 4},{UAMEASUNIT_LENGTH_CENTIMETER|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2}},
        /* ranges_numr_2: */                {{UAMEASUNIT_LENGTH_CENTIMETER,0, 7},{UAMEASUNIT_LENGTH_CENTIMETER|UAMEASFMT_NUMERIC_FIELD_FLAG,0,5}} },
    { NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, {{0,0,0}}, {{0,0,0}}, {{0,0,0}}, {{0,0,0}} }
};

static const MultipleUnitFormat he_multFmt[] = {
//    measures     count                       wide_2                                                                                  wide_0 
    { meas_2hrMn,  UPRV_LENGTHOF(meas_2hrMn),  "\\u05E9\\u05E2\\u05EA\\u05D9\\u05D9\\u05DD \\u05D5-12.10 \\u05D3\\u05E7\\u05D5\\u05EA", "\\u05E9\\u05E2\\u05EA\\u05D9\\u05D9\\u05DD \\u05D5-12 \\u05D3\\u05E7\\u05D5\\u05EA",
        /* shrt_X, shrt_1*/                    "\\u05E9\\u05E2\\u05EA\\u05D9\\u05D9\\u05DD \\u05D5-12.1 \\u05D3\\u05E7\\u05F3", "\\u05E9\\u05E2\\u05EA\\u05D9\\u05D9\\u05DD \\u05D5-12.1 \\u05D3\\u05E7\\u05F3",
        /* shrtr_1, narr_0, numr_0, numr_2*/   "\\u05E9\\u05E2\\u05EA\\u05D9\\u05D9\\u05DD \\u05D5-12.1 \\u05D3\\u05E7\\u05F3", "\\u05E9\\u05E2\\u05EA\\u05D9\\u05D9\\u05DD \\u05D5-12 \\u05D3\\u05E7\\u05F3", "2:12", "2:12.10",
        /* ranges_wide_2: */                {{UAMEASUNIT_DURATION_HOUR,0,6},{UAMEASUNIT_DURATION_MINUTE,9,19},{UAMEASUNIT_DURATION_MINUTE|UAMEASFMT_NUMERIC_FIELD_FLAG,9,14}},
        /* ranges_shrtr_1: */               {{UAMEASUNIT_DURATION_HOUR,0,6},{UAMEASUNIT_DURATION_MINUTE,9,17},{UAMEASUNIT_DURATION_MINUTE|UAMEASFMT_NUMERIC_FIELD_FLAG,9,13}},
        /* ranges_numr_0: */                {{UAMEASUNIT_DURATION_HOUR,0,1},{UAMEASUNIT_DURATION_HOUR|UAMEASFMT_NUMERIC_FIELD_FLAG,0,1},{UAMEASUNIT_DURATION_MINUTE,2,4},{UAMEASUNIT_DURATION_MINUTE|UAMEASFMT_NUMERIC_FIELD_FLAG,2,4}},
        /* ranges_numr_2: */                {{UAMEASUNIT_DURATION_HOUR,0,1},{UAMEASUNIT_DURATION_HOUR|UAMEASFMT_NUMERIC_FIELD_FLAG,0,1},{UAMEASUNIT_DURATION_MINUTE,2,7},{UAMEASUNIT_DURATION_MINUTE|UAMEASFMT_NUMERIC_FIELD_FLAG,2,7}} },
    { NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, {{0,0,0}}, {{0,0,0}}, {{0,0,0}}, {{0,0,0}} }
};

static const MultipleUnitFormat hi_multFmt[] = {
//  rdar://109458821 - tests below are various formats of Hindi representation of 37 hours, 12.10 minutes, which is 37 घं॰, 12.10 मि॰
//    measures     count                       wide_2                                                                                       wide_0 
    { meas_hrMn,   UPRV_LENGTHOF(meas_hrMn),   "37 \\u0918\\u0902\\u091f\\u0947 \\u0914\\u0930 12.10 \\u092e\\u093f\\u0928\\u091f",            "37 \\u0918\\u0902\\u091f\\u0947 \\u0914\\u0930 12 \\u092e\\u093f\\u0928\\u091f",            "37 \\u0918\\u0902\\u0970, 12.1 \\u092e\\u093f\\u0970",            "37 \\u0918\\u0902\\u0970, 12.1 \\u092e\\u093f\\u0970",           "37 \\u0918\\u0902\\u0970 12.1 \\u092e\\u093f\\u0970",         "37 \\u0918\\u0902\\u0970 12 \\u092e\\u093f\\u0970",    "37:12",    "37:12.10",
        /* ranges_wide_2: */                {{UAMEASUNIT_DURATION_HOUR,0,7},{UAMEASUNIT_DURATION_HOUR|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_DURATION_MINUTE,11,21},{UAMEASUNIT_DURATION_MINUTE|UAMEASFMT_NUMERIC_FIELD_FLAG,11,16}},
        /* ranges_shrtr_1: */               {{UAMEASUNIT_DURATION_HOUR,0,6},{UAMEASUNIT_DURATION_HOUR|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_DURATION_MINUTE, 7,15},{UAMEASUNIT_DURATION_MINUTE|UAMEASFMT_NUMERIC_FIELD_FLAG, 7,11}},
        /* ranges_numr_0: */                {{UAMEASUNIT_DURATION_HOUR,0,2},{UAMEASUNIT_DURATION_HOUR|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_DURATION_MINUTE, 3, 5},{UAMEASUNIT_DURATION_MINUTE|UAMEASFMT_NUMERIC_FIELD_FLAG, 3, 5}},
        /* ranges_numr_2: */                {{UAMEASUNIT_DURATION_HOUR,0,2},{UAMEASUNIT_DURATION_HOUR|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_DURATION_MINUTE, 3, 8},{UAMEASUNIT_DURATION_MINUTE|UAMEASFMT_NUMERIC_FIELD_FLAG, 3, 8}} },
//  rdar://109458821 - updated narrow format expected results
    { meas_moDys,  UPRV_LENGTHOF(meas_moDys),  "1 \\u092E\\u0939\\u0940\\u0928\\u093E \\u0914\\u0930 2.00 \\u0926\\u093F\\u0928", "1 \\u092E\\u0939\\u0940\\u0928\\u093E \\u0914\\u0930 2 \\u0926\\u093F\\u0928",
        /* shrt_X, shrt_1*/                    "1 \\u092E\\u093E\\u0939, 2 \\u0926\\u093F\\u0928",  "1 \\u092E\\u093E\\u0939, 2.0 \\u0926\\u093F\\u0928",
        /* shrtr_1, narr_0, numr_0, numr_2*/   "1 \\u092E\\u093E\\u0939 2.0 \\u0926\\u093F\\u0928",  "1 \\u092E\\u093E\\u0939 2 \\u0926\\u093F\\u0928",  "1 \\u092E\\u093E\\u0939 2 \\u0926\\u093F\\u0928",  "1 \\u092E\\u093E\\u0939 2.00 \\u0926\\u093F\\u0928",
        /* ranges_wide_2: */                {{UAMEASUNIT_DURATION_MONTH,0,7},{UAMEASUNIT_DURATION_MONTH|UAMEASFMT_NUMERIC_FIELD_FLAG,0,1},{UAMEASUNIT_DURATION_DAY,11,19},{UAMEASUNIT_DURATION_DAY|UAMEASFMT_NUMERIC_FIELD_FLAG,11,15}},
        /* ranges_shrtr_1: */               {{UAMEASUNIT_DURATION_MONTH,0,5},{UAMEASUNIT_DURATION_MONTH|UAMEASFMT_NUMERIC_FIELD_FLAG,0,1},{UAMEASUNIT_DURATION_DAY,6,13},{UAMEASUNIT_DURATION_DAY|UAMEASFMT_NUMERIC_FIELD_FLAG,6,9}},
        /* ranges_numr_0: */                {{UAMEASUNIT_DURATION_MONTH,0,5},{UAMEASUNIT_DURATION_MONTH|UAMEASFMT_NUMERIC_FIELD_FLAG,0,1},{UAMEASUNIT_DURATION_DAY,6,11},{UAMEASUNIT_DURATION_DAY|UAMEASFMT_NUMERIC_FIELD_FLAG,6,7}},
        /* ranges_numr_0: */                {{UAMEASUNIT_DURATION_MONTH,0,5},{UAMEASUNIT_DURATION_MONTH|UAMEASFMT_NUMERIC_FIELD_FLAG,0,1},{UAMEASUNIT_DURATION_DAY,6,14},{UAMEASUNIT_DURATION_DAY|UAMEASFMT_NUMERIC_FIELD_FLAG,6,10}} },
    { NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, {{0,0,0}}, {{0,0,0}}, {{0,0,0}}, {{0,0,0}} }
};

static const MultipleUnitFormat uk_multFmt[] = {
//    measures     count                       wide_2                                                                                                    wide_0
    { meas_hrMnSc, UPRV_LENGTHOF(meas_hrMnSc), "37 \\u0433\\u043e\\u0434\\u0438\\u043d, 12 \\u0445\\u0432\\u0438\\u043b\\u0438\\u043d \\u0456 5,32 \\u0441\\u0435\\u043a\\u0443\\u043d\\u0434\\u0438",
        /* wide_0 */                           "37 \\u0433\\u043e\\u0434\\u0438\\u043d, 12 \\u0445\\u0432\\u0438\\u043b\\u0438\\u043d \\u0456 5 \\u0441\\u0435\\u043a\\u0443\\u043d\\u0434",
        /* shrt_X, shrt_1 */                   "37 \\u0433\\u043e\\u0434, 12 \\u0445\\u0432, 5,32 \\u0441", "37 \\u0433\\u043e\\u0434, 12 \\u0445\\u0432, 5,3 \\u0441",
        /* shrtr_1, narr_0 */                  "37 \\u0433\\u043e\\u0434 12 \\u0445\\u0432 5,3 \\u0441", "37\\u202f\\u0433 12\\u202f\\u0445\\u0432 5\\u202f\\u0441",
        /* numr_0, numr_2 */                   "37:12:05", "37:12:05,32",
        /* ranges_wide_2: */                {{UAMEASUNIT_DURATION_HOUR,0,8},{UAMEASUNIT_DURATION_HOUR|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_DURATION_MINUTE,10,19},{UAMEASUNIT_DURATION_MINUTE|UAMEASFMT_NUMERIC_FIELD_FLAG,10,12},{UAMEASUNIT_DURATION_SECOND,22,34},{UAMEASUNIT_DURATION_SECOND|UAMEASFMT_NUMERIC_FIELD_FLAG,22,26}},
        /* ranges_shrtr_1: */               {{UAMEASUNIT_DURATION_HOUR,0,6},{UAMEASUNIT_DURATION_HOUR|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_DURATION_MINUTE, 7,12},{UAMEASUNIT_DURATION_MINUTE|UAMEASFMT_NUMERIC_FIELD_FLAG, 7, 9},{UAMEASUNIT_DURATION_SECOND,13,18},{UAMEASUNIT_DURATION_SECOND|UAMEASFMT_NUMERIC_FIELD_FLAG,13,16}},
        /* ranges_numr_0: */                {{UAMEASUNIT_DURATION_HOUR,0,2},{UAMEASUNIT_DURATION_HOUR|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_DURATION_MINUTE, 3, 5},{UAMEASUNIT_DURATION_MINUTE|UAMEASFMT_NUMERIC_FIELD_FLAG, 3, 5},{UAMEASUNIT_DURATION_SECOND, 6, 8},{UAMEASUNIT_DURATION_SECOND|UAMEASFMT_NUMERIC_FIELD_FLAG, 6, 8}},
        /* ranges_numr_2: */                {{UAMEASUNIT_DURATION_HOUR,0,2},{UAMEASUNIT_DURATION_HOUR|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_DURATION_MINUTE, 3, 5},{UAMEASUNIT_DURATION_MINUTE|UAMEASFMT_NUMERIC_FIELD_FLAG, 3, 5},{UAMEASUNIT_DURATION_SECOND, 6, 11},{UAMEASUNIT_DURATION_SECOND|UAMEASFMT_NUMERIC_FIELD_FLAG, 6, 11}} },
    { meas_hrMn,   UPRV_LENGTHOF(meas_hrMn),   "37 \\u0433\\u043e\\u0434\\u0438\\u043d \\u0456 12,10 \\u0445\\u0432\\u0438\\u043b\\u0438\\u043d\\u0438", "37 \\u0433\\u043e\\u0434\\u0438\\u043d \\u0456 12 \\u0445\\u0432\\u0438\\u043b\\u0438\\u043d",
        /* shrt_X, shrt_1 */                   "37 \\u0433\\u043e\\u0434, 12,1 \\u0445\\u0432", "37 \\u0433\\u043e\\u0434, 12,1 \\u0445\\u0432",
        /* shrtr_1, narr_0 numr_0 numr_2 */    "37 \\u0433\\u043e\\u0434 12,1 \\u0445\\u0432", "37\\u202f\\u0433 12\\u202f\\u0445\\u0432",    "37:12",    "37:12,10",
        /* ranges_wide_2: */                {{UAMEASUNIT_DURATION_HOUR,0,8},{UAMEASUNIT_DURATION_HOUR|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_DURATION_MINUTE,11,24},{UAMEASUNIT_DURATION_MINUTE|UAMEASFMT_NUMERIC_FIELD_FLAG,11,16}},
        /* ranges_shrtr_1: */               {{UAMEASUNIT_DURATION_HOUR,0,6},{UAMEASUNIT_DURATION_HOUR|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_DURATION_MINUTE, 7,14},{UAMEASUNIT_DURATION_MINUTE|UAMEASFMT_NUMERIC_FIELD_FLAG, 7, 11}},
        /* ranges_numr_0: */                {{UAMEASUNIT_DURATION_HOUR,0,2},{UAMEASUNIT_DURATION_HOUR|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_DURATION_MINUTE, 3, 5},{UAMEASUNIT_DURATION_MINUTE|UAMEASFMT_NUMERIC_FIELD_FLAG, 3, 5}},
        /* ranges_numr_2: */                {{UAMEASUNIT_DURATION_HOUR,0,2},{UAMEASUNIT_DURATION_HOUR|UAMEASFMT_NUMERIC_FIELD_FLAG,0,2},{UAMEASUNIT_DURATION_MINUTE, 3, 8},{UAMEASUNIT_DURATION_MINUTE|UAMEASFMT_NUMERIC_FIELD_FLAG, 3, 8}} },
    { NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, {{0,0,0}}, {{0,0,0}}, {{0,0,0}}, {{0,0,0}} }
};

static const UAMeasureUnit unit_hrMnSc[] = { UAMEASUNIT_DURATION_HOUR, UAMEASUNIT_DURATION_MINUTE,  UAMEASUNIT_DURATION_SECOND };
static const UAMeasureUnit unit_hrMn[]   = { UAMEASUNIT_DURATION_HOUR, UAMEASUNIT_DURATION_MINUTE   };
static const UAMeasureUnit unit_mCm[]    = { UAMEASUNIT_LENGTH_METER,  UAMEASUNIT_LENGTH_CENTIMETER };

static const MultipleUnitName en_multNam[] = {
//    units        count                       listStyle                    wide                           shrt                    narr 
    { unit_hrMnSc, UPRV_LENGTHOF(unit_hrMnSc), UAMEASNAME_LIST_STANDARD,    "hours, minutes, and seconds", "hours, min, and secs", "hour, min, and sec" },
    { unit_hrMnSc, UPRV_LENGTHOF(unit_hrMnSc), UAMEASNAME_LIST_MATCHUNITS,  "hours, minutes, seconds",     "hours, min, secs",     "hour min sec"       },
    { unit_hrMn,   UPRV_LENGTHOF(unit_hrMn),   UAMEASNAME_LIST_STANDARD,    "hours and minutes",           "hours and min",        "hour and min"       },
    { unit_hrMn,   UPRV_LENGTHOF(unit_hrMn),   UAMEASNAME_LIST_MATCHUNITS,  "hours, minutes",              "hours, min",           "hour min"           },
    { unit_mCm,    UPRV_LENGTHOF(unit_mCm),    UAMEASNAME_LIST_STANDARD,    "meters and centimeters",      "meters and cm",        "meter and cm"       },
    { unit_mCm,    UPRV_LENGTHOF(unit_mCm),    UAMEASNAME_LIST_MATCHUNITS,  "meters, centimeters",         "meters, cm",           "meter cm"           },
    { NULL, 0, (UAMeasureNameListStyle)0, NULL, NULL, NULL }
};

static const MultipleUnitName en_GB_multNam[] = {
//    units        count                       listStyle                    wide                           shrt                    narr 
    { unit_hrMnSc, UPRV_LENGTHOF(unit_hrMnSc), UAMEASNAME_LIST_STANDARD,    "hours, minutes and seconds",  "hours, min and secs",  "hour, min and sec"  },
    { unit_hrMnSc, UPRV_LENGTHOF(unit_hrMnSc), UAMEASNAME_LIST_MATCHUNITS,  "hours, minutes, seconds",     "hours, min, secs",     "hour min sec"       },
    { unit_hrMn,   UPRV_LENGTHOF(unit_hrMn),   UAMEASNAME_LIST_STANDARD,    "hours and minutes",           "hours and min",        "hour and min"       },
    { unit_hrMn,   UPRV_LENGTHOF(unit_hrMn),   UAMEASNAME_LIST_MATCHUNITS,  "hours, minutes",              "hours, min",           "hour min"           },
    { unit_mCm,    UPRV_LENGTHOF(unit_mCm),    UAMEASNAME_LIST_STANDARD,    "metres and centimetres",      "metres and cm",        "m and cm"           },
    { unit_mCm,    UPRV_LENGTHOF(unit_mCm),    UAMEASNAME_LIST_MATCHUNITS,  "metres, centimetres",         "metres, cm",           "m cm"               },
    { NULL, 0, (UAMeasureNameListStyle)0, NULL, NULL, NULL }
};

static const MultipleUnitName ja_multNam[] = {
//    units        count                       listStyle                    wide                                                                                    shrt                                          narr 
    { unit_hrMnSc, UPRV_LENGTHOF(unit_hrMnSc), UAMEASNAME_LIST_STANDARD,    "\\u6642\\u9593\\u3001\\u5206\\u3001\\u79D2",                                           "\\u6642\\u9593\\u3001\\u5206\\u3001\\u79D2", "\\u6642\\u9593\\u3001\\u5206\\u3001\\u79D2" },
    { unit_hrMnSc, UPRV_LENGTHOF(unit_hrMnSc), UAMEASNAME_LIST_MATCHUNITS,  "\\u6642\\u9593 \\u5206 \\u79D2",                                                       "\\u6642\\u9593 \\u5206 \\u79D2",             "\\u6642\\u9593\\u5206\\u79D2"               },
    { unit_hrMn,   UPRV_LENGTHOF(unit_hrMn),   UAMEASNAME_LIST_STANDARD,    "\\u6642\\u9593\\u3001\\u5206",                                                         "\\u6642\\u9593\\u3001\\u5206",               "\\u6642\\u9593\\u3001\\u5206"               },
    { unit_hrMn,   UPRV_LENGTHOF(unit_hrMn),   UAMEASNAME_LIST_MATCHUNITS,  "\\u6642\\u9593 \\u5206",                                                               "\\u6642\\u9593 \\u5206",                     "\\u6642\\u9593\\u5206"                      },
    { unit_mCm,    UPRV_LENGTHOF(unit_mCm),    UAMEASNAME_LIST_STANDARD,    "\\u30E1\\u30FC\\u30C8\\u30EB\\u3001\\u30BB\\u30F3\\u30C1\\u30E1\\u30FC\\u30C8\\u30EB", "m\\u3001cm",                                 "m\\u3001cm"                                 },
    { unit_mCm,    UPRV_LENGTHOF(unit_mCm),    UAMEASNAME_LIST_MATCHUNITS,  "\\u30E1\\u30FC\\u30C8\\u30EB \\u30BB\\u30F3\\u30C1\\u30E1\\u30FC\\u30C8\\u30EB",       "m cm",                                       "mcm"                                        },
    { NULL, 0, (UAMeasureNameListStyle)0, NULL, NULL, NULL }
};

static const LocaleWidthNumFmtItem lwnItems[] = {
    // ============= TIER 0,1 and subocales
//    { "en",         en_singFmt,      en_singNam,      en_multFmt,      en_multNam      }, // en: try plural cases for 1, other
    { "en_GB",      en_GB_singFmt,   en_GB_singNam,   en_GB_multFmt,   en_GB_multNam   },
    { "en_AU",      en_AU_singFmt,   NULL,            NULL,            NULL            },
    { "de",         NULL,            NULL,            NULL,            NULL            }, // de: try plural cases for 1, other
    { "fr",         fr_singFmt,      NULL,            NULL,            NULL            }, // fr: try plural cases for 1, other
    { "fr_CA",      fr_CA_singFmt,   NULL,            NULL,            NULL            },
    { "ja",         NULL,            NULL,            NULL,            ja_multNam      }, // ja: try plural cases for other only
    { "zh_Hans",    zh_singFmt,      zh_singNam,      NULL,            NULL            }, // zh_Hans: try plural cases for other only

    // ============= TIER 2,3 and sublocales
    { "ar",         NULL,            NULL,            NULL,            NULL            }, // ar: try plural cases for 0, 1, 2, 3, 11, 100
    { "ca",         NULL,            NULL,            NULL,            NULL            }, // ca: try plural cases for 1, other
    { "cs",         cs_singFmt,      NULL,            NULL,            NULL            }, // cs: try plural cases for 1, 2, 8.5, 5
    { "da",         da_singFmt,      NULL,            NULL,            NULL            }, // da: try plural cases for 1, other
    { "el",         NULL,            NULL,            NULL,            NULL            }, // el: try plural cases for 1, other
    { "es",         NULL,            NULL,            NULL,            NULL            }, // es: try plural cases for 1, other
    { "es_MX",      NULL,            NULL,            NULL,            NULL            },
    { "fi",         fi_singFmt,      NULL,            NULL,            NULL            }, // fi: try plural cases for 1, other
    { "fo",         NULL,            fo_singNam,      NULL,            NULL            },
    { "he",         he_singFmt,      NULL,            he_multFmt,      NULL            }, // he: try plural cases for 1, 2, 20, 3
    { "hi",         hi_singFmt,      NULL,            hi_multFmt,      NULL            }, // hi: try plural cases for 1 (covers 0.0-1.0), other
    { "hr",         NULL,            NULL,            NULL,            NULL            }, // hr: try plural cases for 1, 2, 5
    { "hu",         NULL,            NULL,            NULL,            NULL            }, // hu: try plural cases for 1, other
    { "id",         NULL,            NULL,            NULL,            NULL            }, // id: try plural cases for other only
    { "it",         it_singFmt,      NULL,            NULL,            NULL            }, // it: try plural cases for 1, other
    { "ko",         NULL,            NULL,            NULL,            NULL            }, // ko: try plural cases for other only
    { "ms",         NULL,            NULL,            NULL,            NULL            }, // ms: try plural cases for other only
    { "nb",         nb_singFmt,      NULL,            NULL,            NULL            }, // nb: try plural cases for 1, other
    { "nl",         nl_singFmt,      nl_singNam,      NULL,            NULL            }, // nl: try plural cases for 1, other
    { "pl",         pl_singFmt,      pl_singNam,      NULL,            NULL            }, // pl: try plural cases for 1, 2, 5, 8.5
    { "pt",         NULL,            NULL,            NULL,            NULL            }, // pt: try plural cases for 1, other
    { "pt_PT",      NULL,            NULL,            NULL,            NULL            },
    { "ro",         ro_singFmt,      NULL,            NULL,            NULL            }, // ro: try plural cases for 1, 2, 20
    { "ru",         ru_singFmt,      NULL,            NULL,            NULL            }, // ru: try plural cases for 1, 2, 5, 8.5
    { "sk",         NULL,            NULL,            NULL,            NULL            }, // sk: try plural cases for 1, 2, 8.5, 5
    { "sv",         NULL,            NULL,            NULL,            NULL            }, // sv: try plural cases for 1, other
    { "th",         NULL,            NULL,            NULL,            NULL            }, // th: try plural cases for other only
    { "tr",         NULL,            NULL,            NULL,            NULL            }, // tr: try plural cases for 1, other
    { "uk",         uk_singFmt,      NULL,            uk_multFmt,      NULL            }, // uk: try plural cases for 1, 2, 5, 8.5
    { "vi",         vi_singFmt,      NULL,            NULL,            NULL            }, // vi: try plural cases for other only
    { "zh_Hant",    zh_Hant_singFmt, zh_Hant_singNam, NULL,            NULL            }, // zh_Hant: try plural cases for other only
    { "zh_Hant_HK", zh_Hant_HK_singFmt, zh_Hant_HK_singNam, NULL,      NULL            },

    // =============TERMINATOR
    { NULL, NULL, NULL, NULL, NULL }
};

enum { kUBufMax = 96, kBBufMax = 192 };


static void TestUAMeasureFormat()
{
    const LocaleWidthNumFmtItem * itemPtr;
    log_verbose("\nTesting uameasfmt functions\n");
    for (itemPtr = lwnItems; itemPtr->locale != NULL; itemPtr++) {
        UChar uget[kUBufMax];
        UChar uexp[kUBufMax];
        char  bget[kBBufMax];
        char  bexp[kBBufMax];
        int32_t ugetLen, uexpLen;
        UErrorCode status = U_ZERO_ERROR;
        UNumberFormat* numfmt_0;
        UNumberFormat* numfmt_1;
        UNumberFormat* numfmt_2;
        UAMeasureFormat* measfmt_wide_2;
        UAMeasureFormat* measfmt_wide_0;
        UAMeasureFormat* measfmt_shrt_X;
        UAMeasureFormat* measfmt_shrt_1;
        UAMeasureFormat* measfmt_shrtr_1;
        UAMeasureFormat* measfmt_narr_0;
        UAMeasureFormat* measfmt_numr_0;
        UAMeasureFormat* measfmt_numr_2;

        numfmt_0 = unum_open(UNUM_DECIMAL, NULL, 0, itemPtr->locale, NULL, &status);
        if ( U_FAILURE(status) ) {
            log_data_err("FAIL: unum_open(UNUM_DECIMAL,...) fails for locale %s: %s\n",
                         itemPtr->locale, u_errorName(status) );
            continue;
        }
        numfmt_1 = unum_clone(numfmt_0, &status);
        numfmt_2 = unum_clone(numfmt_0, &status);
        if ( U_FAILURE(status) ) {
            log_err("FAIL: unum_clone fails for locale %s: %s\n",
                         itemPtr->locale, u_errorName(status) );
            unum_close(numfmt_0);
            continue;
        }
        unum_setAttribute(numfmt_0, UNUM_MIN_FRACTION_DIGITS, 0);
        unum_setAttribute(numfmt_0, UNUM_MAX_FRACTION_DIGITS, 0);
        unum_setAttribute(numfmt_0, UNUM_ROUNDING_MODE, UNUM_ROUND_DOWN);
        unum_setAttribute(numfmt_1, UNUM_MIN_FRACTION_DIGITS, 1);
        unum_setAttribute(numfmt_1, UNUM_MAX_FRACTION_DIGITS, 1);
        unum_setAttribute(numfmt_1, UNUM_ROUNDING_MODE, UNUM_ROUND_DOWN);
        unum_setAttribute(numfmt_2, UNUM_MIN_FRACTION_DIGITS, 2);
        unum_setAttribute(numfmt_2, UNUM_MAX_FRACTION_DIGITS, 2);
        unum_setAttribute(numfmt_2, UNUM_ROUNDING_MODE, UNUM_ROUND_DOWN);

        measfmt_wide_2 = uameasfmt_open(itemPtr->locale, UAMEASFMT_WIDTH_WIDE,    unum_clone(numfmt_2, &status), &status); // wide, num fmt with 2 decimals
        measfmt_wide_0 = uameasfmt_open(itemPtr->locale, UAMEASFMT_WIDTH_WIDE,    unum_clone(numfmt_0, &status), &status); // wide, num fmt with 0 decimals
        measfmt_shrt_X = uameasfmt_open(itemPtr->locale, UAMEASFMT_WIDTH_SHORT,   NULL,                          &status); // short, default num fmt
        measfmt_shrt_1 = uameasfmt_open(itemPtr->locale, UAMEASFMT_WIDTH_SHORT,   unum_clone(numfmt_1, &status), &status); // short, num fmt with 1 decimal
        measfmt_shrtr_1 = uameasfmt_open(itemPtr->locale, UAMEASFMT_WIDTH_SHORTER,   unum_clone(numfmt_1, &status), &status); // shorter, num fmt with 1 decimal
        measfmt_narr_0 = uameasfmt_open(itemPtr->locale, UAMEASFMT_WIDTH_NARROW,  unum_clone(numfmt_0, &status), &status); // narrow, num fmt with 0 decimal
        measfmt_numr_0 = uameasfmt_open(itemPtr->locale, UAMEASFMT_WIDTH_NUMERIC, unum_clone(numfmt_0, &status), &status); // numeric, num fmt with 0 decimal
        measfmt_numr_2 = uameasfmt_open(itemPtr->locale, UAMEASFMT_WIDTH_NUMERIC, unum_clone(numfmt_2, &status), &status); // numeric, num fmt with 2 decimal

        if ( U_FAILURE(status) ) {
            log_data_err("FAIL: uameasfmt_open fails for locale %s, various widths and number formatters: %s\n",
                         itemPtr->locale, u_errorName(status) );
            unum_close(numfmt_0);
            unum_close(numfmt_1);
            unum_close(numfmt_2);
            continue;
        }
        
        if (itemPtr->singleUnitFormatTests != NULL) {
            const SingleUnitFormat * singFmtPtr;
            for (singFmtPtr = itemPtr->singleUnitFormatTests; singFmtPtr->expectFmt_wide_2 != 0; singFmtPtr++) {
                UFieldPosition fpos = { UNUM_INTEGER_FIELD, 0, 0 };
                // wide_2
                status = U_ZERO_ERROR;
                ugetLen = uameasfmt_formatGetPosition(measfmt_wide_2, singFmtPtr->value, singFmtPtr->unit, uget, kUBufMax, NULL, &status);
                if ( U_FAILURE(status) ) {
                    log_err("FAIL: uameasfmt_formatGetPosition locale %s, single format wide_2 for unit %d:%d, status %s\n",
                            itemPtr->locale, ((int)singFmtPtr->unit) >> 8, ((int)singFmtPtr->unit) & 0xFF, u_errorName(status));
                } else {
                    uexpLen = u_unescape(singFmtPtr->expectFmt_wide_2, uexp, kUBufMax);
                    if (ugetLen != uexpLen || u_strcmp(uget, uexp) != 0 ) {
                        u_strToUTF8(bexp, kBBufMax, NULL, uexp, uexpLen, &status);
                        u_strToUTF8(bget, kBBufMax, NULL, uget, ugetLen, &status);
                        log_err("FAIL: uameasfmt_formatGetPosition locale %s, single format wide_2 for unit %d:%d, expected \"%s\", got \"%s\"\n",
                                 itemPtr->locale, ((int)singFmtPtr->unit) >> 8, ((int)singFmtPtr->unit) & 0xFF, bexp, bget);
                    }
                }
                // wide_0
                status = U_ZERO_ERROR;
                fpos.beginIndex = -1;
                fpos.endIndex = -1;
                ugetLen = uameasfmt_formatGetPosition(measfmt_wide_0, singFmtPtr->value, singFmtPtr->unit, uget, kUBufMax, &fpos, &status);
                if ( U_FAILURE(status) ) {
                    log_err("FAIL: uameasfmt_formatGetPosition locale %s, single format wide_0 for unit %d:%d, status %s\n",
                            itemPtr->locale, ((int)singFmtPtr->unit) >> 8, ((int)singFmtPtr->unit) & 0xFF, u_errorName(status));
                } else {
                    uexpLen = u_unescape(singFmtPtr->expectFmt_wide_0, uexp, kUBufMax);
                    if (ugetLen != uexpLen || u_strcmp(uget, uexp) != 0 || fpos.beginIndex != singFmtPtr->beginInt_wide_0 || fpos.endIndex != singFmtPtr->endInt_wide_0) {
                        u_strToUTF8(bexp, kBBufMax, NULL, uexp, uexpLen, &status);
                        u_strToUTF8(bget, kBBufMax, NULL, uget, ugetLen, &status);
                        log_err("FAIL: uameasfmt_formatGetPosition locale %s, single format wide_0 for unit %d:%d,\n    expect \"%s\" int pos [%d:%d],\n    get    \"%s\" int pos [%d:%d]\n",
                                 itemPtr->locale, ((int)singFmtPtr->unit) >> 8, ((int)singFmtPtr->unit) & 0xFF,
                                 bexp, singFmtPtr->beginInt_wide_0, singFmtPtr->endInt_wide_0,
                                 bget, fpos.beginIndex, fpos.endIndex);
                    }
                }
                // shrt_X
                status = U_ZERO_ERROR;
                ugetLen = uameasfmt_formatGetPosition(measfmt_shrt_X, singFmtPtr->value, singFmtPtr->unit, uget, kUBufMax, NULL, &status);
                if ( U_FAILURE(status) ) {
                    log_err("FAIL: uameasfmt_formatGetPosition locale %s, single format shrt_X for unit %d:%d, status %s\n",
                            itemPtr->locale, ((int)singFmtPtr->unit) >> 8, ((int)singFmtPtr->unit) & 0xFF, u_errorName(status));
                } else {
                    uexpLen = u_unescape(singFmtPtr->expectFmt_shrt_X, uexp, kUBufMax);
                    if (ugetLen != uexpLen || u_strcmp(uget, uexp) != 0 ) {
                        u_strToUTF8(bexp, kBBufMax, NULL, uexp, uexpLen, &status);
                        u_strToUTF8(bget, kBBufMax, NULL, uget, ugetLen, &status);
                        log_err("FAIL: uameasfmt_formatGetPosition locale %s, single format shrt_X for unit %d:%d, expected \"%s\", got \"%s\"\n",
                                 itemPtr->locale, ((int)singFmtPtr->unit) >> 8, ((int)singFmtPtr->unit) & 0xFF, bexp, bget);
                    }
                }
                // shrt_1
                status = U_ZERO_ERROR;
                ugetLen = uameasfmt_formatGetPosition(measfmt_shrt_1, singFmtPtr->value, singFmtPtr->unit, uget, kUBufMax, NULL, &status);
                if ( U_FAILURE(status) ) {
                    log_err("FAIL: uameasfmt_formatGetPosition locale %s, single format shrt_1 for unit %d:%d, status %s\n",
                            itemPtr->locale, ((int)singFmtPtr->unit) >> 8, ((int)singFmtPtr->unit) & 0xFF, u_errorName(status));
                } else {
                    uexpLen = u_unescape(singFmtPtr->expectFmt_shrt_1, uexp, kUBufMax);
                    if (ugetLen != uexpLen || u_strcmp(uget, uexp) != 0 ) {
                        u_strToUTF8(bexp, kBBufMax, NULL, uexp, uexpLen, &status);
                        u_strToUTF8(bget, kBBufMax, NULL, uget, ugetLen, &status);
                        log_err("FAIL: uameasfmt_formatGetPosition locale %s, single format shrt_1 for unit %d:%d, expected \"%s\", got \"%s\"\n",
                                 itemPtr->locale, ((int)singFmtPtr->unit) >> 8, ((int)singFmtPtr->unit) & 0xFF, bexp, bget);
                    }
                }
                // narr_0
                status = U_ZERO_ERROR;
                ugetLen = uameasfmt_formatGetPosition(measfmt_narr_0, singFmtPtr->value, singFmtPtr->unit, uget, kUBufMax, NULL, &status);
                if ( U_FAILURE(status) ) {
                    log_err("FAIL: uameasfmt_formatGetPosition locale %s, single format narr_0 for unit %d:%d, status %s\n",
                            itemPtr->locale, ((int)singFmtPtr->unit) >> 8, ((int)singFmtPtr->unit) & 0xFF, u_errorName(status));
                } else {
                    uexpLen = u_unescape(singFmtPtr->expectFmt_narr_0, uexp, kUBufMax);
                    if (ugetLen != uexpLen || u_strcmp(uget, uexp) != 0 ) {
                        u_strToUTF8(bexp, kBBufMax, NULL, uexp, uexpLen, &status);
                        u_strToUTF8(bget, kBBufMax, NULL, uget, ugetLen, &status);
                        log_err("FAIL: uameasfmt_formatGetPosition locale %s, single format narr_0 for unit %d:%d, expected \"%s\", got \"%s\"\n",
                                 itemPtr->locale, ((int)singFmtPtr->unit) >> 8, ((int)singFmtPtr->unit) & 0xFF, bexp, bget);
                    }
                }
                // numr_0
                status = U_ZERO_ERROR;
                fpos.beginIndex = -1;
                fpos.endIndex = -1;
                ugetLen = uameasfmt_formatGetPosition(measfmt_numr_0, singFmtPtr->value, singFmtPtr->unit, uget, kUBufMax, &fpos, &status);
                if ( U_FAILURE(status) ) {
                    log_err("FAIL: uameasfmt_formatGetPosition locale %s, single format numr_0 for unit %d:%d, status %s\n",
                            itemPtr->locale, ((int)singFmtPtr->unit) >> 8, ((int)singFmtPtr->unit) & 0xFF, u_errorName(status));
                } else {
                    uexpLen = u_unescape(singFmtPtr->expectFmt_numr_0, uexp, kUBufMax);
                    if (ugetLen != uexpLen || u_strcmp(uget, uexp) != 0 || fpos.beginIndex != singFmtPtr->beginInt_numr_0 || fpos.endIndex != singFmtPtr->endInt_numr_0) {
                        u_strToUTF8(bexp, kBBufMax, NULL, uexp, uexpLen, &status);
                        u_strToUTF8(bget, kBBufMax, NULL, uget, ugetLen, &status);
                        log_err("FAIL: uameasfmt_formatGetPosition locale %s, single format numr_0 for unit %d:%d,\n    expect \"%s\" int pos [%d:%d],\n    get    \"%s\" int pos [%d:%d]\n",
                                 itemPtr->locale, ((int)singFmtPtr->unit) >> 8, ((int)singFmtPtr->unit) & 0xFF,
                                 bexp, singFmtPtr->beginInt_numr_0, singFmtPtr->endInt_numr_0,
                                 bget, fpos.beginIndex, fpos.endIndex);
                    }
                }
            }
        }
        
        if (itemPtr->singleUnitNameTests != NULL) {
            const SingleUnitName * singNamPtr;
            for (singNamPtr = itemPtr->singleUnitNameTests; singNamPtr->expectName_wide != 0; singNamPtr++) {
                // wide
                status = U_ZERO_ERROR;
                ugetLen = uameasfmt_getUnitName(measfmt_wide_2, singNamPtr->unit, uget, kUBufMax, &status);
                if ( U_FAILURE(status) ) {
                    log_err("FAIL: uameasfmt_getUnitName locale %s, single name wide for unit %d:%d, status %s\n",
                            itemPtr->locale, ((int)singNamPtr->unit) >> 8, ((int)singNamPtr->unit) & 0xFF, u_errorName(status));
                } else {
                    uexpLen = u_unescape(singNamPtr->expectName_wide, uexp, kUBufMax);
                    if (ugetLen != uexpLen || u_strcmp(uget, uexp) != 0 ) {
                        u_strToUTF8(bexp, kBBufMax, NULL, uexp, uexpLen, &status);
                        u_strToUTF8(bget, kBBufMax, NULL, uget, ugetLen, &status);
                        log_err("FAIL: uameasfmt_getUnitName locale %s, single name wide for unit %d:%d, expected \"%s\", got \"%s\"\n",
                                 itemPtr->locale, ((int)singNamPtr->unit) >> 8, ((int)singNamPtr->unit) & 0xFF, bexp, bget);
                    }
                }
                // shrt
                status = U_ZERO_ERROR;
                ugetLen = uameasfmt_getUnitName(measfmt_shrt_X, singNamPtr->unit, uget, kUBufMax, &status);
                if ( U_FAILURE(status) ) {
                    log_err("FAIL: uameasfmt_getUnitName locale %s, single name shrt for unit %d:%d, status %s\n",
                            itemPtr->locale, ((int)singNamPtr->unit) >> 8, ((int)singNamPtr->unit) & 0xFF, u_errorName(status));
                } else {
                    uexpLen = u_unescape(singNamPtr->expectName_shrt, uexp, kUBufMax);
                    if (ugetLen != uexpLen || u_strcmp(uget, uexp) != 0 ) {
                        u_strToUTF8(bexp, kBBufMax, NULL, uexp, uexpLen, &status);
                        u_strToUTF8(bget, kBBufMax, NULL, uget, ugetLen, &status);
                        log_err("FAIL: uameasfmt_getUnitName locale %s, single name shrt for unit %d:%d, expected \"%s\", got \"%s\"\n",
                                 itemPtr->locale, ((int)singNamPtr->unit) >> 8, ((int)singNamPtr->unit) & 0xFF, bexp, bget);
                    }
                }
                // narr
                status = U_ZERO_ERROR;
                ugetLen = uameasfmt_getUnitName(measfmt_narr_0, singNamPtr->unit, uget, kUBufMax, &status);
                if ( U_FAILURE(status) ) {
                    log_err("FAIL: uameasfmt_getUnitName locale %s, single name narr for unit %d:%d, status %s\n",
                            itemPtr->locale, ((int)singNamPtr->unit) >> 8, ((int)singNamPtr->unit) & 0xFF, u_errorName(status));
                } else {
                    uexpLen = u_unescape(singNamPtr->expectName_narr, uexp, kUBufMax);
                    if (ugetLen != uexpLen || u_strcmp(uget, uexp) != 0 ) {
                        u_strToUTF8(bexp, kBBufMax, NULL, uexp, uexpLen, &status);
                        u_strToUTF8(bget, kBBufMax, NULL, uget, ugetLen, &status);
                        log_err("FAIL: uameasfmt_getUnitName locale %s, single name narr for unit %d:%d, expected \"%s\", got \"%s\"\n",
                                 itemPtr->locale, ((int)singNamPtr->unit) >> 8, ((int)singNamPtr->unit) & 0xFF, bexp, bget);
                    }
                }
            }
        }
        
        if (itemPtr->multipleUnitFormatTests != NULL) {
            status = U_ZERO_ERROR;
            UFieldPositionIterator* fpositer = ufieldpositer_open(&status);
            if ( U_FAILURE(status) ) {
                log_err("FAIL: ufieldpositer_open, status %s\n", u_errorName(status));
            } else {
                const MultipleUnitFormat * multFmtPtr;
                for (multFmtPtr = itemPtr->multipleUnitFormatTests; multFmtPtr->measures != 0; multFmtPtr++) {
                    // wide_2
                    status = U_ZERO_ERROR;
                    ugetLen = uameasfmt_formatMultipleForFields(measfmt_wide_2, multFmtPtr->measures, multFmtPtr->measureCount, uget, kUBufMax, fpositer, &status);
                    if ( U_FAILURE(status) ) {
                        log_err("FAIL: uameasfmt_formatMultipleForFields locale %s, multiple format wide_2 for measureCount %d, status %s\n",
                                itemPtr->locale, multFmtPtr->measureCount, u_errorName(status));
                    } else {
                        uexpLen = u_unescape(multFmtPtr->expectFmt_wide_2, uexp, kUBufMax);
                        if (ugetLen != uexpLen || u_strcmp(uget, uexp) != 0 ) {
                            u_strToUTF8(bexp, kBBufMax, NULL, uexp, uexpLen, &status);
                            u_strToUTF8(bget, kBBufMax, NULL, uget, ugetLen, &status);
                            log_err("FAIL: uameasfmt_formatMultipleForFields locale %s, multiple format wide_2 for measureCount %d, expected \"%s\", got \"%s\"\n",
                                     itemPtr->locale, multFmtPtr->measureCount, bexp, bget);
                        } else {
                            // check fpositer
                            int32_t field, beginIndex, endIndex, count;
                            for (count = 0; (field = ufieldpositer_next(fpositer, &beginIndex, &endIndex)) >= 0; count++) {
                                if (field != multFmtPtr->ranges_wide_2[count][0] || beginIndex != multFmtPtr->ranges_wide_2[count][1] || endIndex != multFmtPtr->ranges_wide_2[count][2]) {
                                    log_err("FAIL: uameasfmt_formatMultipleForFields locale %s, multiple format wide_2 for measureCount %d,\n    expect field %0X range %d..%d,\n    get    field %0X range %d..%d\n",
                                            itemPtr->locale, multFmtPtr->measureCount,
                                            multFmtPtr->ranges_wide_2[count][0], multFmtPtr->ranges_wide_2[count][1], multFmtPtr->ranges_wide_2[count][2],
                                            field, beginIndex, endIndex);
                                    break;
                                }
                            }
                        }
                    }
                    // wide_0
                    status = U_ZERO_ERROR;
                    ugetLen = uameasfmt_formatMultipleForFields(measfmt_wide_0, multFmtPtr->measures, multFmtPtr->measureCount, uget, kUBufMax, NULL, &status);
                    if ( U_FAILURE(status) ) {
                        log_err("FAIL: uameasfmt_formatMultipleForFields locale %s, multiple format wide_0 for measureCount %d, status %s\n",
                                itemPtr->locale, multFmtPtr->measureCount, u_errorName(status));
                    } else {
                        uexpLen = u_unescape(multFmtPtr->expectFmt_wide_0, uexp, kUBufMax);
                        if (ugetLen != uexpLen || u_strcmp(uget, uexp) != 0 ) {
                            u_strToUTF8(bexp, kBBufMax, NULL, uexp, uexpLen, &status);
                            u_strToUTF8(bget, kBBufMax, NULL, uget, ugetLen, &status);
                            log_err("FAIL: uameasfmt_formatMultipleForFields locale %s, multiple format wide_0 for measureCount %d, expected \"%s\", got \"%s\"\n",
                                     itemPtr->locale, multFmtPtr->measureCount, bexp, bget);
                        }
                    }
                    // shrt_X
                    status = U_ZERO_ERROR;
                    ugetLen = uameasfmt_formatMultipleForFields(measfmt_shrt_X, multFmtPtr->measures, multFmtPtr->measureCount, uget, kUBufMax, NULL, &status);
                    if ( U_FAILURE(status) ) {
                        log_err("FAIL: uameasfmt_formatMultipleForFields locale %s, multiple format shrt_X for measureCount %d, status %s\n",
                                itemPtr->locale, multFmtPtr->measureCount, u_errorName(status));
                    } else {
                        uexpLen = u_unescape(multFmtPtr->expectFmt_shrt_X, uexp, kUBufMax);
                        if (ugetLen != uexpLen || u_strcmp(uget, uexp) != 0 ) {
                            u_strToUTF8(bexp, kBBufMax, NULL, uexp, uexpLen, &status);
                            u_strToUTF8(bget, kBBufMax, NULL, uget, ugetLen, &status);
                            log_err("FAIL: uameasfmt_formatMultipleForFields locale %s, multiple format shrt_X for measureCount %d, expected \"%s\", got \"%s\"\n",
                                     itemPtr->locale, multFmtPtr->measureCount, bexp, bget);
                        }
                    }
                    // shrt_1
                    status = U_ZERO_ERROR;
                    ugetLen = uameasfmt_formatMultipleForFields(measfmt_shrt_1, multFmtPtr->measures, multFmtPtr->measureCount, uget, kUBufMax, NULL, &status);
                    if ( U_FAILURE(status) ) {
                        log_err("FAIL: uameasfmt_formatMultipleForFields locale %s, multiple format shrt_1 for measureCount %d, status %s\n",
                                itemPtr->locale, multFmtPtr->measureCount, u_errorName(status));
                    } else {
                        uexpLen = u_unescape(multFmtPtr->expectFmt_shrt_1, uexp, kUBufMax);
                        if (ugetLen != uexpLen || u_strcmp(uget, uexp) != 0 ) {
                            u_strToUTF8(bexp, kBBufMax, NULL, uexp, uexpLen, &status);
                            u_strToUTF8(bget, kBBufMax, NULL, uget, ugetLen, &status);
                            log_err("FAIL: uameasfmt_formatMultipleForFields locale %s, multiple format shrt_1 for measureCount %d, expected \"%s\", got \"%s\"\n",
                                     itemPtr->locale, multFmtPtr->measureCount, bexp, bget);
                        }
                    }
                    // shrtr_1
                    status = U_ZERO_ERROR;
                    ugetLen = uameasfmt_formatMultipleForFields(measfmt_shrtr_1, multFmtPtr->measures, multFmtPtr->measureCount, uget, kUBufMax, fpositer, &status);
                    if ( U_FAILURE(status) ) {
                        log_err("FAIL: uameasfmt_formatMultipleForFields locale %s, multiple format shrtr_1 for measureCount %d, status %s\n",
                                itemPtr->locale, multFmtPtr->measureCount, u_errorName(status));
                    } else {
                        uexpLen = u_unescape(multFmtPtr->expectFmt_shrtr_1, uexp, kUBufMax);
                        if (ugetLen != uexpLen || u_strcmp(uget, uexp) != 0 ) {
                            u_strToUTF8(bexp, kBBufMax, NULL, uexp, uexpLen, &status);
                            u_strToUTF8(bget, kBBufMax, NULL, uget, ugetLen, &status);
                            log_err("FAIL: uameasfmt_formatMultipleForFields locale %s, multiple format shrtr_1 for measureCount %d, expected \"%s\", got \"%s\"\n",
                                     itemPtr->locale, multFmtPtr->measureCount, bexp, bget);
                        } else {
                            // check fpositer
                            int32_t field, beginIndex, endIndex, count;
                            for (count = 0; (field = ufieldpositer_next(fpositer, &beginIndex, &endIndex)) >= 0; count++) {
                                if (field != multFmtPtr->ranges_shrtr_1[count][0] || beginIndex != multFmtPtr->ranges_shrtr_1[count][1] || endIndex != multFmtPtr->ranges_shrtr_1[count][2]) {
                                    log_err("FAIL: uameasfmt_formatMultipleForFields locale %s, multiple format shrtr_1 for measureCount %d,\n    expect field %0X range %d..%d,\n    get    field %0X range %d..%d\n",
                                            itemPtr->locale, multFmtPtr->measureCount,
                                            multFmtPtr->ranges_shrtr_1[count][0], multFmtPtr->ranges_shrtr_1[count][1], multFmtPtr->ranges_shrtr_1[count][2],
                                            field, beginIndex, endIndex);
                                    break;
                                }
                            }
                        }
                    }
                    // narr_0
                    status = U_ZERO_ERROR;
                    ugetLen = uameasfmt_formatMultipleForFields(measfmt_narr_0, multFmtPtr->measures, multFmtPtr->measureCount, uget, kUBufMax, NULL, &status);
                    if ( U_FAILURE(status) ) {
                        log_err("FAIL: uameasfmt_formatMultipleForFields locale %s, multiple format narr_0 for measureCount %d, status %s\n",
                                itemPtr->locale, multFmtPtr->measureCount, u_errorName(status));
                    } else {
                        uexpLen = u_unescape(multFmtPtr->expectFmt_narr_0, uexp, kUBufMax);
                        if (ugetLen != uexpLen || u_strcmp(uget, uexp) != 0 ) {
                            u_strToUTF8(bexp, kBBufMax, NULL, uexp, uexpLen, &status);
                            u_strToUTF8(bget, kBBufMax, NULL, uget, ugetLen, &status);
                            log_err("FAIL: uameasfmt_formatMultipleForFields locale %s, multiple format narr_0 for measureCount %d, expected \"%s\", got \"%s\"\n",
                                     itemPtr->locale, multFmtPtr->measureCount, bexp, bget);
                        }
                    }
                    // numr_0
                    status = U_ZERO_ERROR;
                    ugetLen = uameasfmt_formatMultipleForFields(measfmt_numr_0, multFmtPtr->measures, multFmtPtr->measureCount, uget, kUBufMax, fpositer, &status);
                    if ( U_FAILURE(status) ) {
                        log_err("FAIL: uameasfmt_formatMultipleForFields locale %s, multiple format numr_0 for measureCount %d, status %s\n",
                                itemPtr->locale, multFmtPtr->measureCount, u_errorName(status));
                    } else {
                        uexpLen = u_unescape(multFmtPtr->expectFmt_numr_0, uexp, kUBufMax);
                        if (ugetLen != uexpLen || u_strcmp(uget, uexp) != 0 ) {
                            u_strToUTF8(bexp, kBBufMax, NULL, uexp, uexpLen, &status);
                            u_strToUTF8(bget, kBBufMax, NULL, uget, ugetLen, &status);
                            log_err("FAIL: uameasfmt_formatMultipleForFields locale %s, multiple format numr_0 for measureCount %d, expected \"%s\", got \"%s\"\n",
                                     itemPtr->locale, multFmtPtr->measureCount, bexp, bget);
                        } else {
                            // check fpositer
                            int32_t field, beginIndex, endIndex, count;
                            for (count = 0; (field = ufieldpositer_next(fpositer, &beginIndex, &endIndex)) >= 0; count++) {
                                if (field != multFmtPtr->ranges_numr_0[count][0] || beginIndex != multFmtPtr->ranges_numr_0[count][1] || endIndex != multFmtPtr->ranges_numr_0[count][2]) {
                                    log_err("FAIL: uameasfmt_formatMultipleForFields locale %s, multiple format numr_0 for measureCount %d,\n    expect field %0X range %d..%d,\n    get    field %0X range %d..%d\n",
                                            itemPtr->locale, multFmtPtr->measureCount,
                                            multFmtPtr->ranges_numr_0[count][0], multFmtPtr->ranges_numr_0[count][1], multFmtPtr->ranges_numr_0[count][2],
                                            field, beginIndex, endIndex);
                                    break;
                                }
                            }
                        }
                    }
                    // numr_2
                    status = U_ZERO_ERROR;
                    ugetLen = uameasfmt_formatMultipleForFields(measfmt_numr_2, multFmtPtr->measures, multFmtPtr->measureCount, uget, kUBufMax, fpositer, &status);
                    if ( U_FAILURE(status) ) {
                        log_err("FAIL: uameasfmt_formatMultipleForFields locale %s, multiple format numr_2 for measureCount %d, status %s\n",
                                itemPtr->locale, multFmtPtr->measureCount, u_errorName(status));
                    } else {
                        uexpLen = u_unescape(multFmtPtr->expectFmt_numr_2, uexp, kUBufMax);
                        if (ugetLen != uexpLen || u_strcmp(uget, uexp) != 0 ) {
                            u_strToUTF8(bexp, kBBufMax, NULL, uexp, uexpLen, &status);
                            u_strToUTF8(bget, kBBufMax, NULL, uget, ugetLen, &status);
                            log_err("FAIL: uameasfmt_formatMultipleForFields locale %s, multiple format numr_2 for measureCount %d, expected \"%s\", got \"%s\"\n",
                                     itemPtr->locale, multFmtPtr->measureCount, bexp, bget);
                        } else {
                            // check fpositer
                            int32_t field, beginIndex, endIndex, count;
                            for (count = 0; (field = ufieldpositer_next(fpositer, &beginIndex, &endIndex)) >= 0; count++) {
                                if (field != multFmtPtr->ranges_numr_2[count][0] || beginIndex != multFmtPtr->ranges_numr_2[count][1] || endIndex != multFmtPtr->ranges_numr_2[count][2]) {
                                    log_err("FAIL: uameasfmt_formatMultipleForFields locale %s, multiple format numr_2 for measureCount %d,\n    expect field %0X range %d..%d,\n    get    field %0X range %d..%d\n",
                                            itemPtr->locale, multFmtPtr->measureCount,
                                            multFmtPtr->ranges_numr_2[count][0], multFmtPtr->ranges_numr_2[count][1], multFmtPtr->ranges_numr_2[count][2],
                                            field, beginIndex, endIndex);
                                    break;
                                }
                            }
                        }
                    }
                }
                ufieldpositer_close(fpositer);
            }
        }

        if (itemPtr->multipleUnitNameTests != NULL) {
            const MultipleUnitName * multNamPtr;
            for (multNamPtr = itemPtr->multipleUnitNameTests; multNamPtr->units != 0; multNamPtr++) {
                // wide
                status = U_ZERO_ERROR;
                ugetLen = uameasfmt_getMultipleUnitNames(measfmt_wide_2, multNamPtr->units, multNamPtr->unitCount, multNamPtr->listStyle, uget, kUBufMax, &status);
                if ( U_FAILURE(status) ) {
                    log_err("FAIL: uameasfmt_getMultipleUnitNames locale %s, multiple name wide for unitCount %d, listStyle %d, status %s\n",
                            itemPtr->locale, multNamPtr->unitCount, multNamPtr->listStyle, u_errorName(status));
                } else {
                    uexpLen = u_unescape(multNamPtr->expectName_wide, uexp, kUBufMax);
                    if (ugetLen != uexpLen || u_strcmp(uget, uexp) != 0 ) {
                        u_strToUTF8(bexp, kBBufMax, NULL, uexp, uexpLen, &status);
                        u_strToUTF8(bget, kBBufMax, NULL, uget, ugetLen, &status);
                        log_err("FAIL: uameasfmt_getMultipleUnitNames locale %s, multiple name wide for unitCount %d, listStyle %d, expected \"%s\", got \"%s\"\n",
                                 itemPtr->locale, multNamPtr->unitCount, multNamPtr->listStyle, bexp, bget);
                    }
                }
                // shrt
                status = U_ZERO_ERROR;
                ugetLen = uameasfmt_getMultipleUnitNames(measfmt_shrt_X, multNamPtr->units, multNamPtr->unitCount, multNamPtr->listStyle, uget, kUBufMax, &status);
                if ( U_FAILURE(status) ) {
                    log_err("FAIL: uameasfmt_getMultipleUnitNames locale %s, multiple name shrt for unitCount %d, listStyle %d, status %s\n",
                            itemPtr->locale, multNamPtr->unitCount, multNamPtr->listStyle, u_errorName(status));
                } else {
                    uexpLen = u_unescape(multNamPtr->expectName_shrt, uexp, kUBufMax);
                    if (ugetLen != uexpLen || u_strcmp(uget, uexp) != 0 ) {
                        u_strToUTF8(bexp, kBBufMax, NULL, uexp, uexpLen, &status);
                        u_strToUTF8(bget, kBBufMax, NULL, uget, ugetLen, &status);
                        log_err("FAIL: uameasfmt_getMultipleUnitNames locale %s, multiple name shrt for unitCount %d, listStyle %d, expected \"%s\", got \"%s\"\n",
                                 itemPtr->locale, multNamPtr->unitCount, multNamPtr->listStyle, bexp, bget);
                    }
                }
                // narr
                status = U_ZERO_ERROR;
                ugetLen = uameasfmt_getMultipleUnitNames(measfmt_narr_0, multNamPtr->units, multNamPtr->unitCount, multNamPtr->listStyle, uget, kUBufMax, &status);
                if ( U_FAILURE(status) ) {
                    log_err("FAIL: uameasfmt_getMultipleUnitNames locale %s, multiple name narr for unitCount %d, listStyle %d, status %s\n",
                            itemPtr->locale, multNamPtr->unitCount, multNamPtr->listStyle, u_errorName(status));
                } else {
                    uexpLen = u_unescape(multNamPtr->expectName_narr, uexp, kUBufMax);
                    if (ugetLen != uexpLen || u_strcmp(uget, uexp) != 0 ) {
                        u_strToUTF8(bexp, kBBufMax, NULL, uexp, uexpLen, &status);
                        u_strToUTF8(bget, kBBufMax, NULL, uget, ugetLen, &status);
                        log_err("FAIL: uameasfmt_getMultipleUnitNames locale %s, multiple name narr for unitCount %d, listStyle %d, expected \"%s\", got \"%s\"\n",
                                 itemPtr->locale, multNamPtr->unitCount, multNamPtr->listStyle, bexp, bget);
                    }
                }
            }
        }
        
        uameasfmt_close(measfmt_wide_2);
        uameasfmt_close(measfmt_wide_0);
        uameasfmt_close(measfmt_shrt_X);
        uameasfmt_close(measfmt_shrt_1);
        uameasfmt_close(measfmt_shrtr_1);
        uameasfmt_close(measfmt_narr_0);
        uameasfmt_close(measfmt_numr_0);
        uameasfmt_close(measfmt_numr_2);
 
        unum_close(numfmt_0);
        unum_close(numfmt_1);
        unum_close(numfmt_2);
   }
   /* sleep to check leaks etc */
#if U_PLATFORM_IS_DARWIN_BASED || U_PLATFORM_IS_LINUX_BASED || U_PLATFORM == U_PF_BSD || U_PLATFORM == U_PF_SOLARIS
   sleep(8);
#endif
}

static void TestUAMeasFmtOpenAllLocs()
{
    int32_t iLoc, nLoc = uloc_countAvailable();
    for (iLoc = 0; iLoc <= nLoc; iLoc++) {
        const char *loc = (iLoc < nLoc)? uloc_getAvailable(iLoc): "xyz" /* something bogus */;
        if (loc != NULL) {
            UAMeasureFormat* measfmt;
            UErrorCode status;
            
            status = U_ZERO_ERROR;
            measfmt = uameasfmt_open(loc, UAMEASFMT_WIDTH_WIDE, NULL, &status);
            if ( U_SUCCESS(status) ) {
                uameasfmt_close(measfmt);
            } else {
                log_data_err("FAIL: uameasfmt_open fails for locale %-10s, width WIDE   : %s\n", loc, u_errorName(status) );
            }

            status = U_ZERO_ERROR;
            measfmt = uameasfmt_open(loc, UAMEASFMT_WIDTH_SHORT, NULL, &status);
            if ( U_SUCCESS(status) ) {
                uameasfmt_close(measfmt);
            } else {
                log_data_err("FAIL: uameasfmt_open fails for locale %-10s, width SHORT  : %s\n", loc, u_errorName(status) );
            }

            status = U_ZERO_ERROR;
            measfmt = uameasfmt_open(loc, UAMEASFMT_WIDTH_NARROW, NULL, &status);
            if ( U_SUCCESS(status) ) {
                uameasfmt_close(measfmt);
            } else {
                log_data_err("FAIL: uameasfmt_open fails for locale %-10s, width NARROW : %s\n", loc, u_errorName(status) );
            }


            status = U_ZERO_ERROR;
            measfmt = uameasfmt_open(loc, UAMEASFMT_WIDTH_NUMERIC, NULL, &status);
            if ( U_SUCCESS(status) ) {
                uameasfmt_close(measfmt);
            } else {
                log_data_err("FAIL: uameasfmt_open fails for locale %-10s, width NUMERIC: %s\n", loc, u_errorName(status) );
            }
        }
    }
}

enum { kMeasureUnitMax = 3 };

typedef struct {
    const char*   locale;
    const char*   category;
    const char*   usage;
    int32_t       unitCount;
    UAMeasureUnit units[kMeasureUnitMax];
} UnitsForUsageItem;

static const UnitsForUsageItem unitsForUsageItems[] = {
    { "en_US",           "length",  "person",          1, { UAMEASUNIT_LENGTH_INCH } },
    { "en_US",           "length",  "person-informal", 2, { UAMEASUNIT_LENGTH_FOOT, UAMEASUNIT_LENGTH_INCH } },
    { "en_US",           "length",  "person-small",    1, { UAMEASUNIT_LENGTH_INCH } },
    { "fr_FR",           "length",  "person",          1, { UAMEASUNIT_LENGTH_CENTIMETER } },
    { "fr_FR",           "length",  "person-informal", 2, { UAMEASUNIT_LENGTH_METER, UAMEASUNIT_LENGTH_CENTIMETER } },
    { "fr_FR",           "length",  "person-small",    1, { UAMEASUNIT_LENGTH_CENTIMETER } },
    { "fr_FR@rg=USZZZZ", "length",  "person",          1, { UAMEASUNIT_LENGTH_INCH } },
    { "fr_FR@rg=USZZZZ", "length",  "person-informal", 2, { UAMEASUNIT_LENGTH_FOOT, UAMEASUNIT_LENGTH_INCH } },
    { "fr_FR@rg=USZZZZ", "length",  "person-small",    1, { UAMEASUNIT_LENGTH_INCH } },
    { "en_IN",           "pressure","baromtrc",        1, { UAMEASUNIT_PRESSURE_HECTOPASCAL } },
    { "es_MX",           "pressure","baromtrc",        1, { UAMEASUNIT_PRESSURE_MILLIMETER_OF_MERCURY } },
    { "fr_FR",           "pressure","baromtrc",        1, { UAMEASUNIT_PRESSURE_HECTOPASCAL } },
    { "pt"/*BR*/,        "pressure","baromtrc",        1, { UAMEASUNIT_PRESSURE_MILLIBAR } },
    { "en_US",           "length",  NULL,              1, { UAMEASUNIT_LENGTH_FOOT } },
    { "en_US",           "length",  "large",           1, { UAMEASUNIT_LENGTH_MILE } },
    { "en_US",           "length",  "small",           1, { UAMEASUNIT_LENGTH_INCH } },
    { "fr_FR",           "length",  NULL,              1, { UAMEASUNIT_LENGTH_METER } },
    { "fr_FR",           "length",  "large",           1, { UAMEASUNIT_LENGTH_KILOMETER } },
    { "fr_FR",           "length",  "small",           1, { UAMEASUNIT_LENGTH_CENTIMETER } },
    { "en_US",           "xxxxxxxx","yyyyyyyy",        0, { (UAMeasureUnit)0 } },
    // rdar://59634873 kcal: default
    { "fr_FR",           "energy",  "person-usage",    1, { UAMEASUNIT_ENERGY_KILOCALORIE } },
    { "es_ES",           "energy",  "person-usage",    1, { UAMEASUNIT_ENERGY_KILOCALORIE } },
    { "zh_CN",           "energy",  "person-usage",    1, { UAMEASUNIT_ENERGY_KILOCALORIE } },
    // rdar://59634873 kJ: AU NZ
    { "en_AU",           "energy",  "person-usage",    1, { UAMEASUNIT_ENERGY_KILOJOULE } },
    { "en_NZ",           "energy",  "person-usage",    1, { UAMEASUNIT_ENERGY_KILOJOULE } },
    // rdar://59634873 Cal: US CA
    { "en_US",           "energy",  "person-usage",    1, { UAMEASUNIT_ENERGY_FOODCALORIE } },
    { "en_CA",           "energy",  "person-usage",    1, { UAMEASUNIT_ENERGY_FOODCALORIE } },
    // rdar://59634873 Cal: others as requested by Loc
    { "he_IL",           "energy",  "person-usage",    1, { UAMEASUNIT_ENERGY_FOODCALORIE } },
    { "pt_BR",           "energy",  "person-usage",    1, { UAMEASUNIT_ENERGY_FOODCALORIE } },
    { "zh_TW",           "energy",  "person-usage",    1, { UAMEASUNIT_ENERGY_FOODCALORIE } },
    { "ar_SA",           "energy",  "person-usage",    1, { UAMEASUNIT_ENERGY_FOODCALORIE } },
    { "ar_AE",           "energy",  "person-usage",    1, { UAMEASUNIT_ENERGY_FOODCALORIE } },
    // tests for ms=
    { "en_US@ms=metric",   "length", "large",           1, { UAMEASUNIT_LENGTH_KILOMETER } },
    { "fr_FR@ms=ussystem", "length", "large",           1, { UAMEASUNIT_LENGTH_MILE } },
    { "en_GB",             "concentr", "blood-glucose", 1, { UAMEASUNIT_CONCENTRATION_MILLIMOLE_PER_LITER } },
    { "en_GB@ms=metric",   "concentr", "blood-glucose", 1, { UAMEASUNIT_CONCENTRATION_MILLIMOLE_PER_LITER } },
    { "en_US@ms=uksystem", "mass",   "person",          2, { UAMEASUNIT_MASS_STONE, UAMEASUNIT_MASS_POUND } },
    { "fr_FR",             "mass",   "person",          1, { UAMEASUNIT_MASS_KILOGRAM } },
    { "fr_FR@rg=USZZZZ",   "mass",   "person",          1, { UAMEASUNIT_MASS_POUND } },
    { "fr_FR@ms=uksystem;rg=USZZZZ", "mass", "person",  2, { UAMEASUNIT_MASS_STONE, UAMEASUNIT_MASS_POUND } },
    // rdar://73253930 consumption: US GB CA backward compatibility
    { "en_US",             "consumption", "vehicle-fuel", 1, { UAMEASUNIT_CONSUMPTION_MILE_PER_GALLON } },
    { "en_GB",             "consumption", "vehicle-fuel", 1, { UAMEASUNIT_CONSUMPTION_MILE_PER_GALLON_IMPERIAL } },
    { "en_CA",             "consumption", "vehicle-fuel", 1, { UAMEASUNIT_CONSUMPTION_MILE_PER_GALLON_IMPERIAL } },
    { "fr_FR",             "consumption", "vehicle-fuel", 1, { UAMEASUNIT_CONSUMPTION_LITER_PER_100_KILOMETERs } },
    { "pt_BR",             "consumption", "vehicle-fuel", 1, { UAMEASUNIT_CONSUMPTION_LITER_PER_KILOMETER } },
    // rdar://77037602 energy/default and energy/NULL
    { "en_US",           "energy",  "default",          1, { UAMEASUNIT_ENERGY_KILOWATT_HOUR } },
    { "en_US",           "energy",  NULL,               1, { UAMEASUNIT_ENERGY_KILOWATT_HOUR } },
    // rdar://77037877 default va NULL for various quantities
    { "en_US",           "length",  "default",          1, { UAMEASUNIT_LENGTH_FOOT } },
    { "en_US",           "length",  NULL,               1, { UAMEASUNIT_LENGTH_FOOT } },
    { "en_US",           "area",    "default",          1, { UAMEASUNIT_AREA_SQUARE_FOOT } },
    { "en_US",           "area",    NULL,               1, { UAMEASUNIT_AREA_SQUARE_FOOT } },
    { "en_US",           "mass",    "default",          1, { UAMEASUNIT_MASS_POUND } },
    { "en_US",           "mass",    NULL,               1, { UAMEASUNIT_MASS_POUND } },
    { "en_US",           "power",   "default",          1, { UAMEASUNIT_POWER_WATT } },
    { "en_US",           "power",   NULL,               1, { UAMEASUNIT_POWER_WATT } },
    { "en_US",           "volume",  "default",          1, { UAMEASUNIT_VOLUME_GALLON } },
    { "en_US",           "volume",  NULL,               1, { UAMEASUNIT_VOLUME_GALLON } },
    // rdar://47001274 ([NanoWeather]: UA: 18E165: Ukrainian would prefer m/s instead of km/h for wind speed)
    { "uk_UA",           "speed",   NULL,               1, { UAMEASUNIT_SPEED_KILOMETER_PER_HOUR } },
    { "uk_UA",           "speed",   "wind",             1, { UAMEASUNIT_SPEED_METER_PER_SECOND } },
    // rdar://105547568 (The wrong wind speed unit is returned from UnitSpeed(forLocale:usage:) when the Locale is assembled from Locale.Components)
    { "nn_NO",           "speed", "wind",               1, { UAMEASUNIT_SPEED_METER_PER_SECOND } },
    { "nn_NO@ms=metric", "speed", "wind",               1, { UAMEASUNIT_SPEED_METER_PER_SECOND } },
    // terminator
    { NULL, NULL, NULL, 0, { (UAMeasureUnit)0 } }
};

static void TestUAGetUnitsForUsage()
{
    const UnitsForUsageItem* itemPtr = unitsForUsageItems;
    for (; itemPtr->locale != NULL; itemPtr++) {
        UAMeasureUnit units[kMeasureUnitMax];
        UErrorCode status = U_ZERO_ERROR;
        int32_t unitsCount = uameasfmt_getUnitsForUsage(itemPtr->locale, itemPtr->category, itemPtr->usage,
                                                        units, kMeasureUnitMax, &status);
        if ( U_FAILURE(status) ) {
            if (itemPtr->unitCount != 0) {
                log_err("FAIL: uameasfmt_getUnitsForUsage locale %s, category %s/%s, status %s\n",
                        itemPtr->locale, itemPtr->category, itemPtr->usage, u_errorName(status));
            }
        } else if (itemPtr->unitCount == 0) {
            log_err("FAIL: uameasfmt_getUnitsForUsage locale %s, category %s/%s, expected failure, got status %s\n",
                    itemPtr->locale, itemPtr->category, itemPtr->usage, u_errorName(status));
        } else if (unitsCount != itemPtr->unitCount) {
            log_err("FAIL: uameasfmt_getUnitsForUsage locale %s, category %s/%s, expected count %d, got %d\n",
                    itemPtr->locale, itemPtr->category, itemPtr->usage, itemPtr->unitCount, unitsCount);
        } else if (units[0] != itemPtr->units[0] || (unitsCount == 2 && units[1] != itemPtr->units[1])) {
            log_err("FAIL: uameasfmt_getUnitsForUsage locale %s, category %s/%s, expected units x%04X ..., got x%04X ...\n",
                    itemPtr->locale, itemPtr->category, itemPtr->usage, itemPtr->units[0], units[0]);
        }
    }
}

typedef struct {
    UAMeasureUnit unit;
    const char*   category;
} CategoryForUnit;

static const CategoryForUnit categoryForUnitItems[] = {
    { UAMEASUNIT_VOLUME_LITER,                      "volume"   },
    { UAMEASUNIT_LENGTH_METER,                      "length"   },
    { UAMEASUNIT_PRESSURE_HECTOPASCAL,              "pressure" },
    { UAMEASUNIT_CONCENTRATION_MILLIMOLE_PER_LITER, "concentr" },
    { (UAMeasureUnit)0,                             NULL }
};

static void TestUAGetCategoryForUnit()
{
    const CategoryForUnit* itemPtr = categoryForUnitItems;
    for (; itemPtr->category != NULL; itemPtr++) {
        UErrorCode status = U_ZERO_ERROR;
        const char *category = uameasfmt_getUnitCategory(itemPtr->unit, &status);
        if ( U_FAILURE(status) ) {
            log_err("FAIL: uameasfmt_getUnitCategory unit %d:%d, got status %s\n",
                    ((int)itemPtr->unit) >> 8, ((int)itemPtr->unit) & 0xFF, u_errorName(status));
        } else if (category == NULL) {
            log_err("FAIL: uameasfmt_getUnitCategory unit %d:%d, got NULL return\n",
                    ((int)itemPtr->unit) >> 8, ((int)itemPtr->unit) & 0xFF);
        } else if (uprv_strcmp(category, itemPtr->category) != 0) {
            log_err("FAIL: uameasfmt_getUnitCategory unit %d:%d, expected %s, got %s\n",
                    ((int)itemPtr->unit) >> 8, ((int)itemPtr->unit) & 0xFF, itemPtr->category, category);
        }
    }
}

#include "stdio.h"  // for sprintf

static void TestMeasurementSystemOverride(void) {
    const char* localeIDs[] = {
        "en_US@xxx=y",    // use uloc_toLegacyKey() and uloc_toLegacyType() to construct locale ID
        "en-GB-u-ms-metric",
        "en-US-u-ms-metric",
        "en_GB@measure=metric",
        "en_US@measure=metric",
        "en_GB@ms=metric",
        "en_US@ms=metric"
    };
    UErrorCode err = U_ZERO_ERROR;
    UAMeasureUnit units[5];
    for (int32_t i = 0; i < UPRV_LENGTHOF(localeIDs); i++) {
        const char* localeID = localeIDs[i];
        char constructedLocale[200];
        if (uprv_strstr(localeID, "@xxx") != NULL) {
            uprv_strcpy(constructedLocale, localeIDs[i]);
            uloc_setKeywordValue("xxx", NULL, constructedLocale, 200, &err);
            uloc_setKeywordValue(uloc_toLegacyKey("ms"), uloc_toLegacyType("ms", "metric"), constructedLocale, 200, &err);
            localeID = constructedLocale;
        }
        if (uprv_strstr(localeID, "-u-")) {
            uloc_forLanguageTag(localeID, constructedLocale, 200, NULL, &err);
            localeID = constructedLocale;
        }
        
        int count = uameasfmt_getUnitsForUsage(localeID, "length", "default", units, 5, &err);
        char failureMessage[200];
        sprintf(failureMessage, "Wrong unit for %s", localeIDs[i]);
        
        assertIntEquals(failureMessage, 1, count);
        assertIntEquals(failureMessage, UAMEASUNIT_LENGTH_METER, units[0]);
    }
}

// rdar://107276400 and rdar://106566783
static void TestRgSubtag(void) {
    UChar* testData[] = {
        u"en_US",                        u"2", u"2 months",
        u"en_US@rg=USzzzz",              u"2", u"2 months",
        u"es_US",                        u"2", u"2 meses",
        u"es_MX@rg=USzzzz",              u"2", u"2 meses",
        u"ar_US@numbers=latn",           u"7", u"7 أشهر",
        u"ar_US@rg=USzzzz;numbers=latn", u"7", u"7 أشهر",
        u"ar_US",                        u"2", u"شهران",
        u"ar_SA@rg=USzzzz",              u"2", u"شهران",
        u"ru_US",                        u"2", u"2 месяца",
        u"ru_RU@rg=USzzzz",              u"2", u"2 месяца",
        u"zh_Hans_US",                   u"2", u"2个月",
        u"zh_CN@rg=USzzzz",              u"2", u"2个月",
        u"fr_US",                        u"2", u"2\u00a0mois",
        u"fr_FR@rg=USzzzz",              u"2", u"2\u00a0mois",
        u"ko_US",                        u"2", u"2개월",
        u"ko_KR@rg=USzzzz",              u"2", u"2개월",
        u"pt_BR_US",                     u"2", u"2 meses",
        u"pt_BR@rg=USzzzz",              u"2", u"2 meses",
        u"vi_US",                        u"2", u"2 tháng",
        u"vi_VN@rg=USzzzz",              u"2", u"2 tháng",
        u"zh_Hant_US",                   u"2", u"2個月",
        u"zh_TW@rg=USzzzz",              u"2", u"2個月",
    };
    
    UErrorCode err = U_ZERO_ERROR;
    UChar result[200];
    
    for (int32_t i = 0; i < UPRV_LENGTHOF(testData); i+= 3) {
        char* locale = austrdup(testData[i]);
        double amount = atof(austrdup(testData[i + 1]));
        UChar* expectedResult = testData[i + 2];
        
        err = U_ZERO_ERROR;
        UAMeasureFormat* mf = uameasfmt_open(locale, UAMEASFMT_WIDTH_WIDE, unum_open(UNUM_DECIMAL, NULL, 0, locale, NULL, &err), &err);
        
        char errorMessage[100];
        snprintf(errorMessage, 100, "Failed to create measurement formatter for %s", locale);
        if (assertSuccess(errorMessage, &err)) {
            uameasfmt_format(mf, amount, UAMEASUNIT_DURATION_MONTH, result, 200, &err);
            if (assertSuccess("Error formatting", &err)) {
                snprintf(errorMessage, 100, "Wrong result for %s", locale);
                assertUEquals(errorMessage, expectedResult, result);
            }
        }
        uameasfmt_close(mf);
    }
}

#endif /* #if !UCONFIG_NO_FORMATTING */
