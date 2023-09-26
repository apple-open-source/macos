// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
* Copyright (c) 2004-2016, International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
* Author: Alan Liu
* Created: April 26, 2004
* Since: ICU 3.0
**********************************************************************
*/
#include "utypeinfo.h" // for 'typeid' to work

#include "unicode/measunit.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/uenum.h"
#include "unicode/errorcode.h"
#include "ustrenum.h"
#include "cstring.h"
#include "uassert.h"
#include "measunit_impl.h"

U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(MeasureUnit)

// All code between the "Start generated code" comment and
// the "End generated code" comment is auto generated code
// and must not be edited manually. For instructions on how to correctly
// update this code, refer to:
// https://icu.unicode.org/design/formatting/measureformat/updating-measure-unit
//
// Start generated code for measunit.cpp

// Maps from Type ID to offset in gSubTypes.
static const int32_t gOffsets[] = {
    0,
    2,
    7,
    17,
    27,
    31,
    332,
    343,
    360,
    364,
    373,
    376,
    380,
    388,
    410,
    414,
    429,
    430,
    436,
    446,
    450,
    454,
    456,
    490
};

static const int32_t kCurrencyOffset = 5;

// Must be sorted alphabetically.
static const char * const gTypes[] = {
    "acceleration",
    "angle",
    "area",
    "concentr",
    "consumption",
    "currency",
    "digital",
    "duration",
    "electric",
    "energy",
    "force",
    "frequency",
    "graphics",
    "length",
    "light",
    "mass",
    "none",
    "power",
    "pressure",
    "speed",
    "temperature",
    "torque",
    "volume"
};

// Must be grouped by type and sorted alphabetically within each type.
static const char * const gSubTypes[] = {
    "g-force",
    "meter-per-square-second",
    "arc-minute",
    "arc-second",
    "degree",
    "radian",
    "revolution",
    "acre",
    "dunam",
    "hectare",
    "square-centimeter",
    "square-foot",
    "square-inch",
    "square-kilometer",
    "square-meter",
    "square-mile",
    "square-yard",
    "item",
    "karat",
    "milligram-ofglucose-per-deciliter",
    "milligram-per-deciliter",
    "millimole-per-liter",
    "mole",
    "percent",
    "permille",
    "permillion",
    "permyriad",
    "liter-per-100-kilometer",
    "liter-per-kilometer",
    "mile-per-gallon",
    "mile-per-gallon-imperial",
    "ADP",
    "AED",
    "AFA",
    "AFN",
    "ALK",
    "ALL",
    "AMD",
    "ANG",
    "AOA",
    "AOK",
    "AON",
    "AOR",
    "ARA",
    "ARP",
    "ARS",
    "ARY",
    "ATS",
    "AUD",
    "AWG",
    "AYM",
    "AZM",
    "AZN",
    "BAD",
    "BAM",
    "BBD",
    "BDT",
    "BEC",
    "BEF",
    "BEL",
    "BGJ",
    "BGK",
    "BGL",
    "BGN",
    "BHD",
    "BIF",
    "BMD",
    "BND",
    "BOB",
    "BOP",
    "BOV",
    "BRB",
    "BRC",
    "BRE",
    "BRL",
    "BRN",
    "BRR",
    "BSD",
    "BTN",
    "BUK",
    "BWP",
    "BYB",
    "BYN",
    "BYR",
    "BZD",
    "CAD",
    "CDF",
    "CHC",
    "CHE",
    "CHF",
    "CHW",
    "CLF",
    "CLP",
    "CNY",
    "COP",
    "COU",
    "CRC",
    "CSD",
    "CSJ",
    "CSK",
    "CUC",
    "CUP",
    "CVE",
    "CYP",
    "CZK",
    "DDM",
    "DEM",
    "DJF",
    "DKK",
    "DOP",
    "DZD",
    "ECS",
    "ECV",
    "EEK",
    "EGP",
    "ERN",
    "ESA",
    "ESB",
    "ESP",
    "ETB",
    "EUR",
    "FIM",
    "FJD",
    "FKP",
    "FRF",
    "GBP",
    "GEK",
    "GEL",
    "GHC",
    "GHP",
    "GHS",
    "GIP",
    "GMD",
    "GNE",
    "GNF",
    "GNS",
    "GQE",
    "GRD",
    "GTQ",
    "GWE",
    "GWP",
    "GYD",
    "HKD",
    "HNL",
    "HRD",
    "HRK",
    "HTG",
    "HUF",
    "IDR",
    "IEP",
    "ILP",
    "ILR",
    "ILS",
    "INR",
    "IQD",
    "IRR",
    "ISJ",
    "ISK",
    "ITL",
    "JMD",
    "JOD",
    "JPY",
    "KES",
    "KGS",
    "KHR",
    "KMF",
    "KPW",
    "KRW",
    "KWD",
    "KYD",
    "KZT",
    "LAJ",
    "LAK",
    "LBP",
    "LKR",
    "LRD",
    "LSL",
    "LSM",
    "LTL",
    "LTT",
    "LUC",
    "LUF",
    "LUL",
    "LVL",
    "LVR",
    "LYD",
    "MAD",
    "MDL",
    "MGA",
    "MGF",
    "MKD",
    "MLF",
    "MMK",
    "MNT",
    "MOP",
    "MRO",
    "MRU",
    "MTL",
    "MTP",
    "MUR",
    "MVQ",
    "MVR",
    "MWK",
    "MXN",
    "MXP",
    "MXV",
    "MYR",
    "MZE",
    "MZM",
    "MZN",
    "NAD",
    "NGN",
    "NIC",
    "NIO",
    "NLG",
    "NOK",
    "NPR",
    "NZD",
    "OMR",
    "PAB",
    "PEH",
    "PEI",
    "PEN",
    "PES",
    "PGK",
    "PHP",
    "PKR",
    "PLN",
    "PLZ",
    "PTE",
    "PYG",
    "QAR",
    "RHD",
    "ROK",
    "ROL",
    "RON",
    "RSD",
    "RUB",
    "RUR",
    "RWF",
    "SAR",
    "SBD",
    "SCR",
    "SDD",
    "SDG",
    "SDP",
    "SEK",
    "SGD",
    "SHP",
    "SIT",
    "SKK",
    "SLE",
    "SLL",
    "SOS",
    "SRD",
    "SRG",
    "SSP",
    "STD",
    "STN",
    "SUR",
    "SVC",
    "SYP",
    "SZL",
    "THB",
    "TJR",
    "TJS",
    "TMM",
    "TMT",
    "TND",
    "TOP",
    "TPE",
    "TRL",
    "TRY",
    "TTD",
    "TWD",
    "TZS",
    "UAH",
    "UAK",
    "UGS",
    "UGW",
    "UGX",
    "USD",
    "USN",
    "USS",
    "UYI",
    "UYN",
    "UYP",
    "UYU",
    "UYW",
    "UZS",
    "VEB",
    "VED",
    "VEF",
    "VES",
    "VNC",
    "VND",
    "VUV",
    "WST",
    "XAF",
    "XAG",
    "XAU",
    "XBA",
    "XBB",
    "XBC",
    "XBD",
    "XCD",
    "XDR",
    "XEU",
    "XOF",
    "XPD",
    "XPF",
    "XPT",
    "XSU",
    "XTS",
    "XUA",
    "XXX",
    "YDD",
    "YER",
    "YUD",
    "YUM",
    "YUN",
    "ZAL",
    "ZAR",
    "ZMK",
    "ZMW",
    "ZRN",
    "ZRZ",
    "ZWC",
    "ZWD",
    "ZWL",
    "ZWN",
    "ZWR",
    "bit",
    "byte",
    "gigabit",
    "gigabyte",
    "kilobit",
    "kilobyte",
    "megabit",
    "megabyte",
    "petabyte",
    "terabit",
    "terabyte",
    "century",
    "day",
    "day-person",
    "decade",
    "hour",
    "microsecond",
    "millisecond",
    "minute",
    "month",
    "month-person",
    "nanosecond",
    "quarter",
    "second",
    "week",
    "week-person",
    "year",
    "year-person",
    "ampere",
    "milliampere",
    "ohm",
    "volt",
    "british-thermal-unit",
    "calorie",
    "electronvolt",
    "foodcalorie",
    "joule",
    "kilocalorie",
    "kilojoule",
    "kilowatt-hour",
    "therm-us",
    "kilowatt-hour-per-100-kilometer",
    "newton",
    "pound-force",
    "gigahertz",
    "hertz",
    "kilohertz",
    "megahertz",
    "dot",
    "dot-per-centimeter",
    "dot-per-inch",
    "em",
    "megapixel",
    "pixel",
    "pixel-per-centimeter",
    "pixel-per-inch",
    "astronomical-unit",
    "centimeter",
    "decimeter",
    "earth-radius",
    "fathom",
    "foot",
    "furlong",
    "inch",
    "kilometer",
    "light-year",
    "meter",
    "micrometer",
    "mile",
    "mile-scandinavian",
    "millimeter",
    "nanometer",
    "nautical-mile",
    "parsec",
    "picometer",
    "point",
    "solar-radius",
    "yard",
    "candela",
    "lumen",
    "lux",
    "solar-luminosity",
    "carat",
    "dalton",
    "earth-mass",
    "grain",
    "gram",
    "kilogram",
    "microgram",
    "milligram",
    "ounce",
    "ounce-troy",
    "pound",
    "solar-mass",
    "stone",
    "ton",
    "tonne",
    "",
    "gigawatt",
    "horsepower",
    "kilowatt",
    "megawatt",
    "milliwatt",
    "watt",
    "atmosphere",
    "bar",
    "hectopascal",
    "inch-ofhg",
    "kilopascal",
    "megapascal",
    "millibar",
    "millimeter-ofhg",
    "pascal",
    "pound-force-per-square-inch",
    "kilometer-per-hour",
    "knot",
    "meter-per-second",
    "mile-per-hour",
    "celsius",
    "fahrenheit",
    "generic",
    "kelvin",
    "newton-meter",
    "pound-force-foot",
    "acre-foot",
    "barrel",
    "bushel",
    "centiliter",
    "cubic-centimeter",
    "cubic-foot",
    "cubic-inch",
    "cubic-kilometer",
    "cubic-meter",
    "cubic-mile",
    "cubic-yard",
    "cup",
    "cup-metric",
    "deciliter",
    "dessert-spoon",
    "dessert-spoon-imperial",
    "dram",
    "drop",
    "fluid-ounce",
    "fluid-ounce-imperial",
    "gallon",
    "gallon-imperial",
    "hectoliter",
    "jigger",
    "liter",
    "megaliter",
    "milliliter",
    "pinch",
    "pint",
    "pint-metric",
    "quart",
    "quart-imperial",
    "tablespoon",
    "teaspoon"
};

// Shortcuts to the base unit in order to make the default constructor fast
static const int32_t kBaseTypeIdx = 16;
static const int32_t kBaseSubTypeIdx = 0;

MeasureUnit *MeasureUnit::createGForce(UErrorCode &status) {
    return MeasureUnit::create(0, 0, status);
}

MeasureUnit MeasureUnit::getGForce() {
    return MeasureUnit(0, 0);
}

MeasureUnit *MeasureUnit::createMeterPerSecondSquared(UErrorCode &status) {
    return MeasureUnit::create(0, 1, status);
}

MeasureUnit MeasureUnit::getMeterPerSecondSquared() {
    return MeasureUnit(0, 1);
}

MeasureUnit *MeasureUnit::createArcMinute(UErrorCode &status) {
    return MeasureUnit::create(1, 0, status);
}

MeasureUnit MeasureUnit::getArcMinute() {
    return MeasureUnit(1, 0);
}

MeasureUnit *MeasureUnit::createArcSecond(UErrorCode &status) {
    return MeasureUnit::create(1, 1, status);
}

MeasureUnit MeasureUnit::getArcSecond() {
    return MeasureUnit(1, 1);
}

MeasureUnit *MeasureUnit::createDegree(UErrorCode &status) {
    return MeasureUnit::create(1, 2, status);
}

MeasureUnit MeasureUnit::getDegree() {
    return MeasureUnit(1, 2);
}

MeasureUnit *MeasureUnit::createRadian(UErrorCode &status) {
    return MeasureUnit::create(1, 3, status);
}

MeasureUnit MeasureUnit::getRadian() {
    return MeasureUnit(1, 3);
}

MeasureUnit *MeasureUnit::createRevolutionAngle(UErrorCode &status) {
    return MeasureUnit::create(1, 4, status);
}

MeasureUnit MeasureUnit::getRevolutionAngle() {
    return MeasureUnit(1, 4);
}

MeasureUnit *MeasureUnit::createAcre(UErrorCode &status) {
    return MeasureUnit::create(2, 0, status);
}

MeasureUnit MeasureUnit::getAcre() {
    return MeasureUnit(2, 0);
}

MeasureUnit *MeasureUnit::createDunam(UErrorCode &status) {
    return MeasureUnit::create(2, 1, status);
}

MeasureUnit MeasureUnit::getDunam() {
    return MeasureUnit(2, 1);
}

MeasureUnit *MeasureUnit::createHectare(UErrorCode &status) {
    return MeasureUnit::create(2, 2, status);
}

MeasureUnit MeasureUnit::getHectare() {
    return MeasureUnit(2, 2);
}

MeasureUnit *MeasureUnit::createSquareCentimeter(UErrorCode &status) {
    return MeasureUnit::create(2, 3, status);
}

MeasureUnit MeasureUnit::getSquareCentimeter() {
    return MeasureUnit(2, 3);
}

MeasureUnit *MeasureUnit::createSquareFoot(UErrorCode &status) {
    return MeasureUnit::create(2, 4, status);
}

MeasureUnit MeasureUnit::getSquareFoot() {
    return MeasureUnit(2, 4);
}

MeasureUnit *MeasureUnit::createSquareInch(UErrorCode &status) {
    return MeasureUnit::create(2, 5, status);
}

MeasureUnit MeasureUnit::getSquareInch() {
    return MeasureUnit(2, 5);
}

MeasureUnit *MeasureUnit::createSquareKilometer(UErrorCode &status) {
    return MeasureUnit::create(2, 6, status);
}

MeasureUnit MeasureUnit::getSquareKilometer() {
    return MeasureUnit(2, 6);
}

MeasureUnit *MeasureUnit::createSquareMeter(UErrorCode &status) {
    return MeasureUnit::create(2, 7, status);
}

MeasureUnit MeasureUnit::getSquareMeter() {
    return MeasureUnit(2, 7);
}

MeasureUnit *MeasureUnit::createSquareMile(UErrorCode &status) {
    return MeasureUnit::create(2, 8, status);
}

MeasureUnit MeasureUnit::getSquareMile() {
    return MeasureUnit(2, 8);
}

MeasureUnit *MeasureUnit::createSquareYard(UErrorCode &status) {
    return MeasureUnit::create(2, 9, status);
}

MeasureUnit MeasureUnit::getSquareYard() {
    return MeasureUnit(2, 9);
}

MeasureUnit *MeasureUnit::createItem(UErrorCode &status) {
    return MeasureUnit::create(3, 0, status);
}

MeasureUnit MeasureUnit::getItem() {
    return MeasureUnit(3, 0);
}

MeasureUnit *MeasureUnit::createKarat(UErrorCode &status) {
    return MeasureUnit::create(3, 1, status);
}

MeasureUnit MeasureUnit::getKarat() {
    return MeasureUnit(3, 1);
}

MeasureUnit *MeasureUnit::createMilligramOfglucosePerDeciliter(UErrorCode &status) {
    return MeasureUnit::create(3, 2, status);
}

MeasureUnit MeasureUnit::getMilligramOfglucosePerDeciliter() {
    return MeasureUnit(3, 2);
}

MeasureUnit *MeasureUnit::createMilligramPerDeciliter(UErrorCode &status) {
    return MeasureUnit::create(3, 3, status);
}

MeasureUnit MeasureUnit::getMilligramPerDeciliter() {
    return MeasureUnit(3, 3);
}

MeasureUnit *MeasureUnit::createMillimolePerLiter(UErrorCode &status) {
    return MeasureUnit::create(3, 4, status);
}

MeasureUnit MeasureUnit::getMillimolePerLiter() {
    return MeasureUnit(3, 4);
}

MeasureUnit *MeasureUnit::createMole(UErrorCode &status) {
    return MeasureUnit::create(3, 5, status);
}

MeasureUnit MeasureUnit::getMole() {
    return MeasureUnit(3, 5);
}

MeasureUnit *MeasureUnit::createPercent(UErrorCode &status) {
    return MeasureUnit::create(3, 6, status);
}

MeasureUnit MeasureUnit::getPercent() {
    return MeasureUnit(3, 6);
}

MeasureUnit *MeasureUnit::createPermille(UErrorCode &status) {
    return MeasureUnit::create(3, 7, status);
}

MeasureUnit MeasureUnit::getPermille() {
    return MeasureUnit(3, 7);
}

MeasureUnit *MeasureUnit::createPartPerMillion(UErrorCode &status) {
    return MeasureUnit::create(3, 8, status);
}

MeasureUnit MeasureUnit::getPartPerMillion() {
    return MeasureUnit(3, 8);
}

MeasureUnit *MeasureUnit::createPermyriad(UErrorCode &status) {
    return MeasureUnit::create(3, 9, status);
}

MeasureUnit MeasureUnit::getPermyriad() {
    return MeasureUnit(3, 9);
}

MeasureUnit *MeasureUnit::createLiterPer100Kilometers(UErrorCode &status) {
    return MeasureUnit::create(4, 0, status);
}

MeasureUnit MeasureUnit::getLiterPer100Kilometers() {
    return MeasureUnit(4, 0);
}

MeasureUnit *MeasureUnit::createLiterPerKilometer(UErrorCode &status) {
    return MeasureUnit::create(4, 1, status);
}

MeasureUnit MeasureUnit::getLiterPerKilometer() {
    return MeasureUnit(4, 1);
}

MeasureUnit *MeasureUnit::createMilePerGallon(UErrorCode &status) {
    return MeasureUnit::create(4, 2, status);
}

MeasureUnit MeasureUnit::getMilePerGallon() {
    return MeasureUnit(4, 2);
}

MeasureUnit *MeasureUnit::createMilePerGallonImperial(UErrorCode &status) {
    return MeasureUnit::create(4, 3, status);
}

MeasureUnit MeasureUnit::getMilePerGallonImperial() {
    return MeasureUnit(4, 3);
}

MeasureUnit *MeasureUnit::createBit(UErrorCode &status) {
    return MeasureUnit::create(6, 0, status);
}

MeasureUnit MeasureUnit::getBit() {
    return MeasureUnit(6, 0);
}

MeasureUnit *MeasureUnit::createByte(UErrorCode &status) {
    return MeasureUnit::create(6, 1, status);
}

MeasureUnit MeasureUnit::getByte() {
    return MeasureUnit(6, 1);
}

MeasureUnit *MeasureUnit::createGigabit(UErrorCode &status) {
    return MeasureUnit::create(6, 2, status);
}

MeasureUnit MeasureUnit::getGigabit() {
    return MeasureUnit(6, 2);
}

MeasureUnit *MeasureUnit::createGigabyte(UErrorCode &status) {
    return MeasureUnit::create(6, 3, status);
}

MeasureUnit MeasureUnit::getGigabyte() {
    return MeasureUnit(6, 3);
}

MeasureUnit *MeasureUnit::createKilobit(UErrorCode &status) {
    return MeasureUnit::create(6, 4, status);
}

MeasureUnit MeasureUnit::getKilobit() {
    return MeasureUnit(6, 4);
}

MeasureUnit *MeasureUnit::createKilobyte(UErrorCode &status) {
    return MeasureUnit::create(6, 5, status);
}

MeasureUnit MeasureUnit::getKilobyte() {
    return MeasureUnit(6, 5);
}

MeasureUnit *MeasureUnit::createMegabit(UErrorCode &status) {
    return MeasureUnit::create(6, 6, status);
}

MeasureUnit MeasureUnit::getMegabit() {
    return MeasureUnit(6, 6);
}

MeasureUnit *MeasureUnit::createMegabyte(UErrorCode &status) {
    return MeasureUnit::create(6, 7, status);
}

MeasureUnit MeasureUnit::getMegabyte() {
    return MeasureUnit(6, 7);
}

MeasureUnit *MeasureUnit::createPetabyte(UErrorCode &status) {
    return MeasureUnit::create(6, 8, status);
}

MeasureUnit MeasureUnit::getPetabyte() {
    return MeasureUnit(6, 8);
}

MeasureUnit *MeasureUnit::createTerabit(UErrorCode &status) {
    return MeasureUnit::create(6, 9, status);
}

MeasureUnit MeasureUnit::getTerabit() {
    return MeasureUnit(6, 9);
}

MeasureUnit *MeasureUnit::createTerabyte(UErrorCode &status) {
    return MeasureUnit::create(6, 10, status);
}

MeasureUnit MeasureUnit::getTerabyte() {
    return MeasureUnit(6, 10);
}

MeasureUnit *MeasureUnit::createCentury(UErrorCode &status) {
    return MeasureUnit::create(7, 0, status);
}

MeasureUnit MeasureUnit::getCentury() {
    return MeasureUnit(7, 0);
}

MeasureUnit *MeasureUnit::createDay(UErrorCode &status) {
    return MeasureUnit::create(7, 1, status);
}

MeasureUnit MeasureUnit::getDay() {
    return MeasureUnit(7, 1);
}

MeasureUnit *MeasureUnit::createDayPerson(UErrorCode &status) {
    return MeasureUnit::create(7, 2, status);
}

MeasureUnit MeasureUnit::getDayPerson() {
    return MeasureUnit(7, 2);
}

MeasureUnit *MeasureUnit::createDecade(UErrorCode &status) {
    return MeasureUnit::create(7, 3, status);
}

MeasureUnit MeasureUnit::getDecade() {
    return MeasureUnit(7, 3);
}

MeasureUnit *MeasureUnit::createHour(UErrorCode &status) {
    return MeasureUnit::create(7, 4, status);
}

MeasureUnit MeasureUnit::getHour() {
    return MeasureUnit(7, 4);
}

MeasureUnit *MeasureUnit::createMicrosecond(UErrorCode &status) {
    return MeasureUnit::create(7, 5, status);
}

MeasureUnit MeasureUnit::getMicrosecond() {
    return MeasureUnit(7, 5);
}

MeasureUnit *MeasureUnit::createMillisecond(UErrorCode &status) {
    return MeasureUnit::create(7, 6, status);
}

MeasureUnit MeasureUnit::getMillisecond() {
    return MeasureUnit(7, 6);
}

MeasureUnit *MeasureUnit::createMinute(UErrorCode &status) {
    return MeasureUnit::create(7, 7, status);
}

MeasureUnit MeasureUnit::getMinute() {
    return MeasureUnit(7, 7);
}

MeasureUnit *MeasureUnit::createMonth(UErrorCode &status) {
    return MeasureUnit::create(7, 8, status);
}

MeasureUnit MeasureUnit::getMonth() {
    return MeasureUnit(7, 8);
}

MeasureUnit *MeasureUnit::createMonthPerson(UErrorCode &status) {
    return MeasureUnit::create(7, 9, status);
}

MeasureUnit MeasureUnit::getMonthPerson() {
    return MeasureUnit(7, 9);
}

MeasureUnit *MeasureUnit::createNanosecond(UErrorCode &status) {
    return MeasureUnit::create(7, 10, status);
}

MeasureUnit MeasureUnit::getNanosecond() {
    return MeasureUnit(7, 10);
}

MeasureUnit *MeasureUnit::createQuarter(UErrorCode &status) {
    return MeasureUnit::create(7, 11, status);
}

MeasureUnit MeasureUnit::getQuarter() {
    return MeasureUnit(7, 11);
}

MeasureUnit *MeasureUnit::createSecond(UErrorCode &status) {
    return MeasureUnit::create(7, 12, status);
}

MeasureUnit MeasureUnit::getSecond() {
    return MeasureUnit(7, 12);
}

MeasureUnit *MeasureUnit::createWeek(UErrorCode &status) {
    return MeasureUnit::create(7, 13, status);
}

MeasureUnit MeasureUnit::getWeek() {
    return MeasureUnit(7, 13);
}

MeasureUnit *MeasureUnit::createWeekPerson(UErrorCode &status) {
    return MeasureUnit::create(7, 14, status);
}

MeasureUnit MeasureUnit::getWeekPerson() {
    return MeasureUnit(7, 14);
}

MeasureUnit *MeasureUnit::createYear(UErrorCode &status) {
    return MeasureUnit::create(7, 15, status);
}

MeasureUnit MeasureUnit::getYear() {
    return MeasureUnit(7, 15);
}

MeasureUnit *MeasureUnit::createYearPerson(UErrorCode &status) {
    return MeasureUnit::create(7, 16, status);
}

MeasureUnit MeasureUnit::getYearPerson() {
    return MeasureUnit(7, 16);
}

MeasureUnit *MeasureUnit::createAmpere(UErrorCode &status) {
    return MeasureUnit::create(8, 0, status);
}

MeasureUnit MeasureUnit::getAmpere() {
    return MeasureUnit(8, 0);
}

MeasureUnit *MeasureUnit::createMilliampere(UErrorCode &status) {
    return MeasureUnit::create(8, 1, status);
}

MeasureUnit MeasureUnit::getMilliampere() {
    return MeasureUnit(8, 1);
}

MeasureUnit *MeasureUnit::createOhm(UErrorCode &status) {
    return MeasureUnit::create(8, 2, status);
}

MeasureUnit MeasureUnit::getOhm() {
    return MeasureUnit(8, 2);
}

MeasureUnit *MeasureUnit::createVolt(UErrorCode &status) {
    return MeasureUnit::create(8, 3, status);
}

MeasureUnit MeasureUnit::getVolt() {
    return MeasureUnit(8, 3);
}

MeasureUnit *MeasureUnit::createBritishThermalUnit(UErrorCode &status) {
    return MeasureUnit::create(9, 0, status);
}

MeasureUnit MeasureUnit::getBritishThermalUnit() {
    return MeasureUnit(9, 0);
}

MeasureUnit *MeasureUnit::createCalorie(UErrorCode &status) {
    return MeasureUnit::create(9, 1, status);
}

MeasureUnit MeasureUnit::getCalorie() {
    return MeasureUnit(9, 1);
}

MeasureUnit *MeasureUnit::createElectronvolt(UErrorCode &status) {
    return MeasureUnit::create(9, 2, status);
}

MeasureUnit MeasureUnit::getElectronvolt() {
    return MeasureUnit(9, 2);
}

MeasureUnit *MeasureUnit::createFoodcalorie(UErrorCode &status) {
    return MeasureUnit::create(9, 3, status);
}

MeasureUnit MeasureUnit::getFoodcalorie() {
    return MeasureUnit(9, 3);
}

MeasureUnit *MeasureUnit::createJoule(UErrorCode &status) {
    return MeasureUnit::create(9, 4, status);
}

MeasureUnit MeasureUnit::getJoule() {
    return MeasureUnit(9, 4);
}

MeasureUnit *MeasureUnit::createKilocalorie(UErrorCode &status) {
    return MeasureUnit::create(9, 5, status);
}

MeasureUnit MeasureUnit::getKilocalorie() {
    return MeasureUnit(9, 5);
}

MeasureUnit *MeasureUnit::createKilojoule(UErrorCode &status) {
    return MeasureUnit::create(9, 6, status);
}

MeasureUnit MeasureUnit::getKilojoule() {
    return MeasureUnit(9, 6);
}

MeasureUnit *MeasureUnit::createKilowattHour(UErrorCode &status) {
    return MeasureUnit::create(9, 7, status);
}

MeasureUnit MeasureUnit::getKilowattHour() {
    return MeasureUnit(9, 7);
}

MeasureUnit *MeasureUnit::createThermUs(UErrorCode &status) {
    return MeasureUnit::create(9, 8, status);
}

MeasureUnit MeasureUnit::getThermUs() {
    return MeasureUnit(9, 8);
}

MeasureUnit *MeasureUnit::createKilowattHourPer100Kilometer(UErrorCode &status) {
    return MeasureUnit::create(10, 0, status);
}

MeasureUnit MeasureUnit::getKilowattHourPer100Kilometer() {
    return MeasureUnit(10, 0);
}

MeasureUnit *MeasureUnit::createNewton(UErrorCode &status) {
    return MeasureUnit::create(10, 1, status);
}

MeasureUnit MeasureUnit::getNewton() {
    return MeasureUnit(10, 1);
}

MeasureUnit *MeasureUnit::createPoundForce(UErrorCode &status) {
    return MeasureUnit::create(10, 2, status);
}

MeasureUnit MeasureUnit::getPoundForce() {
    return MeasureUnit(10, 2);
}

MeasureUnit *MeasureUnit::createGigahertz(UErrorCode &status) {
    return MeasureUnit::create(11, 0, status);
}

MeasureUnit MeasureUnit::getGigahertz() {
    return MeasureUnit(11, 0);
}

MeasureUnit *MeasureUnit::createHertz(UErrorCode &status) {
    return MeasureUnit::create(11, 1, status);
}

MeasureUnit MeasureUnit::getHertz() {
    return MeasureUnit(11, 1);
}

MeasureUnit *MeasureUnit::createKilohertz(UErrorCode &status) {
    return MeasureUnit::create(11, 2, status);
}

MeasureUnit MeasureUnit::getKilohertz() {
    return MeasureUnit(11, 2);
}

MeasureUnit *MeasureUnit::createMegahertz(UErrorCode &status) {
    return MeasureUnit::create(11, 3, status);
}

MeasureUnit MeasureUnit::getMegahertz() {
    return MeasureUnit(11, 3);
}

MeasureUnit *MeasureUnit::createDot(UErrorCode &status) {
    return MeasureUnit::create(12, 0, status);
}

MeasureUnit MeasureUnit::getDot() {
    return MeasureUnit(12, 0);
}

MeasureUnit *MeasureUnit::createDotPerCentimeter(UErrorCode &status) {
    return MeasureUnit::create(12, 1, status);
}

MeasureUnit MeasureUnit::getDotPerCentimeter() {
    return MeasureUnit(12, 1);
}

MeasureUnit *MeasureUnit::createDotPerInch(UErrorCode &status) {
    return MeasureUnit::create(12, 2, status);
}

MeasureUnit MeasureUnit::getDotPerInch() {
    return MeasureUnit(12, 2);
}

MeasureUnit *MeasureUnit::createEm(UErrorCode &status) {
    return MeasureUnit::create(12, 3, status);
}

MeasureUnit MeasureUnit::getEm() {
    return MeasureUnit(12, 3);
}

MeasureUnit *MeasureUnit::createMegapixel(UErrorCode &status) {
    return MeasureUnit::create(12, 4, status);
}

MeasureUnit MeasureUnit::getMegapixel() {
    return MeasureUnit(12, 4);
}

MeasureUnit *MeasureUnit::createPixel(UErrorCode &status) {
    return MeasureUnit::create(12, 5, status);
}

MeasureUnit MeasureUnit::getPixel() {
    return MeasureUnit(12, 5);
}

MeasureUnit *MeasureUnit::createPixelPerCentimeter(UErrorCode &status) {
    return MeasureUnit::create(12, 6, status);
}

MeasureUnit MeasureUnit::getPixelPerCentimeter() {
    return MeasureUnit(12, 6);
}

MeasureUnit *MeasureUnit::createPixelPerInch(UErrorCode &status) {
    return MeasureUnit::create(12, 7, status);
}

MeasureUnit MeasureUnit::getPixelPerInch() {
    return MeasureUnit(12, 7);
}

MeasureUnit *MeasureUnit::createAstronomicalUnit(UErrorCode &status) {
    return MeasureUnit::create(13, 0, status);
}

MeasureUnit MeasureUnit::getAstronomicalUnit() {
    return MeasureUnit(13, 0);
}

MeasureUnit *MeasureUnit::createCentimeter(UErrorCode &status) {
    return MeasureUnit::create(13, 1, status);
}

MeasureUnit MeasureUnit::getCentimeter() {
    return MeasureUnit(13, 1);
}

MeasureUnit *MeasureUnit::createDecimeter(UErrorCode &status) {
    return MeasureUnit::create(13, 2, status);
}

MeasureUnit MeasureUnit::getDecimeter() {
    return MeasureUnit(13, 2);
}

MeasureUnit *MeasureUnit::createEarthRadius(UErrorCode &status) {
    return MeasureUnit::create(13, 3, status);
}

MeasureUnit MeasureUnit::getEarthRadius() {
    return MeasureUnit(13, 3);
}

MeasureUnit *MeasureUnit::createFathom(UErrorCode &status) {
    return MeasureUnit::create(13, 4, status);
}

MeasureUnit MeasureUnit::getFathom() {
    return MeasureUnit(13, 4);
}

MeasureUnit *MeasureUnit::createFoot(UErrorCode &status) {
    return MeasureUnit::create(13, 5, status);
}

MeasureUnit MeasureUnit::getFoot() {
    return MeasureUnit(13, 5);
}

MeasureUnit *MeasureUnit::createFurlong(UErrorCode &status) {
    return MeasureUnit::create(13, 6, status);
}

MeasureUnit MeasureUnit::getFurlong() {
    return MeasureUnit(13, 6);
}

MeasureUnit *MeasureUnit::createInch(UErrorCode &status) {
    return MeasureUnit::create(13, 7, status);
}

MeasureUnit MeasureUnit::getInch() {
    return MeasureUnit(13, 7);
}

MeasureUnit *MeasureUnit::createKilometer(UErrorCode &status) {
    return MeasureUnit::create(13, 8, status);
}

MeasureUnit MeasureUnit::getKilometer() {
    return MeasureUnit(13, 8);
}

MeasureUnit *MeasureUnit::createLightYear(UErrorCode &status) {
    return MeasureUnit::create(13, 9, status);
}

MeasureUnit MeasureUnit::getLightYear() {
    return MeasureUnit(13, 9);
}

MeasureUnit *MeasureUnit::createMeter(UErrorCode &status) {
    return MeasureUnit::create(13, 10, status);
}

MeasureUnit MeasureUnit::getMeter() {
    return MeasureUnit(13, 10);
}

MeasureUnit *MeasureUnit::createMicrometer(UErrorCode &status) {
    return MeasureUnit::create(13, 11, status);
}

MeasureUnit MeasureUnit::getMicrometer() {
    return MeasureUnit(13, 11);
}

MeasureUnit *MeasureUnit::createMile(UErrorCode &status) {
    return MeasureUnit::create(13, 12, status);
}

MeasureUnit MeasureUnit::getMile() {
    return MeasureUnit(13, 12);
}

MeasureUnit *MeasureUnit::createMileScandinavian(UErrorCode &status) {
    return MeasureUnit::create(13, 13, status);
}

MeasureUnit MeasureUnit::getMileScandinavian() {
    return MeasureUnit(13, 13);
}

MeasureUnit *MeasureUnit::createMillimeter(UErrorCode &status) {
    return MeasureUnit::create(13, 14, status);
}

MeasureUnit MeasureUnit::getMillimeter() {
    return MeasureUnit(13, 14);
}

MeasureUnit *MeasureUnit::createNanometer(UErrorCode &status) {
    return MeasureUnit::create(13, 15, status);
}

MeasureUnit MeasureUnit::getNanometer() {
    return MeasureUnit(13, 15);
}

MeasureUnit *MeasureUnit::createNauticalMile(UErrorCode &status) {
    return MeasureUnit::create(13, 16, status);
}

MeasureUnit MeasureUnit::getNauticalMile() {
    return MeasureUnit(13, 16);
}

MeasureUnit *MeasureUnit::createParsec(UErrorCode &status) {
    return MeasureUnit::create(13, 17, status);
}

MeasureUnit MeasureUnit::getParsec() {
    return MeasureUnit(13, 17);
}

MeasureUnit *MeasureUnit::createPicometer(UErrorCode &status) {
    return MeasureUnit::create(13, 18, status);
}

MeasureUnit MeasureUnit::getPicometer() {
    return MeasureUnit(13, 18);
}

MeasureUnit *MeasureUnit::createPoint(UErrorCode &status) {
    return MeasureUnit::create(13, 19, status);
}

MeasureUnit MeasureUnit::getPoint() {
    return MeasureUnit(13, 19);
}

MeasureUnit *MeasureUnit::createSolarRadius(UErrorCode &status) {
    return MeasureUnit::create(13, 20, status);
}

MeasureUnit MeasureUnit::getSolarRadius() {
    return MeasureUnit(13, 20);
}

MeasureUnit *MeasureUnit::createYard(UErrorCode &status) {
    return MeasureUnit::create(13, 21, status);
}

MeasureUnit MeasureUnit::getYard() {
    return MeasureUnit(13, 21);
}

MeasureUnit *MeasureUnit::createCandela(UErrorCode &status) {
    return MeasureUnit::create(14, 0, status);
}

MeasureUnit MeasureUnit::getCandela() {
    return MeasureUnit(14, 0);
}

MeasureUnit *MeasureUnit::createLumen(UErrorCode &status) {
    return MeasureUnit::create(14, 1, status);
}

MeasureUnit MeasureUnit::getLumen() {
    return MeasureUnit(14, 1);
}

MeasureUnit *MeasureUnit::createLux(UErrorCode &status) {
    return MeasureUnit::create(14, 2, status);
}

MeasureUnit MeasureUnit::getLux() {
    return MeasureUnit(14, 2);
}

MeasureUnit *MeasureUnit::createSolarLuminosity(UErrorCode &status) {
    return MeasureUnit::create(14, 3, status);
}

MeasureUnit MeasureUnit::getSolarLuminosity() {
    return MeasureUnit(14, 3);
}

MeasureUnit *MeasureUnit::createCarat(UErrorCode &status) {
    return MeasureUnit::create(15, 0, status);
}

MeasureUnit MeasureUnit::getCarat() {
    return MeasureUnit(15, 0);
}

MeasureUnit *MeasureUnit::createDalton(UErrorCode &status) {
    return MeasureUnit::create(15, 1, status);
}

MeasureUnit MeasureUnit::getDalton() {
    return MeasureUnit(15, 1);
}

MeasureUnit *MeasureUnit::createEarthMass(UErrorCode &status) {
    return MeasureUnit::create(15, 2, status);
}

MeasureUnit MeasureUnit::getEarthMass() {
    return MeasureUnit(15, 2);
}

MeasureUnit *MeasureUnit::createGrain(UErrorCode &status) {
    return MeasureUnit::create(15, 3, status);
}

MeasureUnit MeasureUnit::getGrain() {
    return MeasureUnit(15, 3);
}

MeasureUnit *MeasureUnit::createGram(UErrorCode &status) {
    return MeasureUnit::create(15, 4, status);
}

MeasureUnit MeasureUnit::getGram() {
    return MeasureUnit(15, 4);
}

MeasureUnit *MeasureUnit::createKilogram(UErrorCode &status) {
    return MeasureUnit::create(15, 5, status);
}

MeasureUnit MeasureUnit::getKilogram() {
    return MeasureUnit(15, 5);
}

MeasureUnit *MeasureUnit::createMetricTon(UErrorCode &status) {
    return MeasureUnit::create(15, 14, status);
}

MeasureUnit MeasureUnit::getMetricTon() {
    return MeasureUnit(15, 14);
}

MeasureUnit *MeasureUnit::createMicrogram(UErrorCode &status) {
    return MeasureUnit::create(15, 6, status);
}

MeasureUnit MeasureUnit::getMicrogram() {
    return MeasureUnit(15, 6);
}

MeasureUnit *MeasureUnit::createMilligram(UErrorCode &status) {
    return MeasureUnit::create(15, 7, status);
}

MeasureUnit MeasureUnit::getMilligram() {
    return MeasureUnit(15, 7);
}

MeasureUnit *MeasureUnit::createOunce(UErrorCode &status) {
    return MeasureUnit::create(15, 8, status);
}

MeasureUnit MeasureUnit::getOunce() {
    return MeasureUnit(15, 8);
}

MeasureUnit *MeasureUnit::createOunceTroy(UErrorCode &status) {
    return MeasureUnit::create(15, 9, status);
}

MeasureUnit MeasureUnit::getOunceTroy() {
    return MeasureUnit(15, 9);
}

MeasureUnit *MeasureUnit::createPound(UErrorCode &status) {
    return MeasureUnit::create(15, 10, status);
}

MeasureUnit MeasureUnit::getPound() {
    return MeasureUnit(15, 10);
}

MeasureUnit *MeasureUnit::createSolarMass(UErrorCode &status) {
    return MeasureUnit::create(15, 11, status);
}

MeasureUnit MeasureUnit::getSolarMass() {
    return MeasureUnit(15, 11);
}

MeasureUnit *MeasureUnit::createStone(UErrorCode &status) {
    return MeasureUnit::create(15, 12, status);
}

MeasureUnit MeasureUnit::getStone() {
    return MeasureUnit(15, 12);
}

MeasureUnit *MeasureUnit::createTon(UErrorCode &status) {
    return MeasureUnit::create(15, 13, status);
}

MeasureUnit MeasureUnit::getTon() {
    return MeasureUnit(15, 13);
}

MeasureUnit *MeasureUnit::createTonne(UErrorCode &status) {
    return MeasureUnit::create(15, 14, status);
}

MeasureUnit MeasureUnit::getTonne() {
    return MeasureUnit(15, 14);
}

MeasureUnit *MeasureUnit::createGigawatt(UErrorCode &status) {
    return MeasureUnit::create(17, 0, status);
}

MeasureUnit MeasureUnit::getGigawatt() {
    return MeasureUnit(17, 0);
}

MeasureUnit *MeasureUnit::createHorsepower(UErrorCode &status) {
    return MeasureUnit::create(17, 1, status);
}

MeasureUnit MeasureUnit::getHorsepower() {
    return MeasureUnit(17, 1);
}

MeasureUnit *MeasureUnit::createKilowatt(UErrorCode &status) {
    return MeasureUnit::create(17, 2, status);
}

MeasureUnit MeasureUnit::getKilowatt() {
    return MeasureUnit(17, 2);
}

MeasureUnit *MeasureUnit::createMegawatt(UErrorCode &status) {
    return MeasureUnit::create(17, 3, status);
}

MeasureUnit MeasureUnit::getMegawatt() {
    return MeasureUnit(17, 3);
}

MeasureUnit *MeasureUnit::createMilliwatt(UErrorCode &status) {
    return MeasureUnit::create(17, 4, status);
}

MeasureUnit MeasureUnit::getMilliwatt() {
    return MeasureUnit(17, 4);
}

MeasureUnit *MeasureUnit::createWatt(UErrorCode &status) {
    return MeasureUnit::create(17, 5, status);
}

MeasureUnit MeasureUnit::getWatt() {
    return MeasureUnit(17, 5);
}

MeasureUnit *MeasureUnit::createAtmosphere(UErrorCode &status) {
    return MeasureUnit::create(18, 0, status);
}

MeasureUnit MeasureUnit::getAtmosphere() {
    return MeasureUnit(18, 0);
}

MeasureUnit *MeasureUnit::createBar(UErrorCode &status) {
    return MeasureUnit::create(18, 1, status);
}

MeasureUnit MeasureUnit::getBar() {
    return MeasureUnit(18, 1);
}

MeasureUnit *MeasureUnit::createHectopascal(UErrorCode &status) {
    return MeasureUnit::create(18, 2, status);
}

MeasureUnit MeasureUnit::getHectopascal() {
    return MeasureUnit(18, 2);
}

MeasureUnit *MeasureUnit::createInchHg(UErrorCode &status) {
    return MeasureUnit::create(18, 3, status);
}

MeasureUnit MeasureUnit::getInchHg() {
    return MeasureUnit(18, 3);
}

MeasureUnit *MeasureUnit::createKilopascal(UErrorCode &status) {
    return MeasureUnit::create(18, 4, status);
}

MeasureUnit MeasureUnit::getKilopascal() {
    return MeasureUnit(18, 4);
}

MeasureUnit *MeasureUnit::createMegapascal(UErrorCode &status) {
    return MeasureUnit::create(18, 5, status);
}

MeasureUnit MeasureUnit::getMegapascal() {
    return MeasureUnit(18, 5);
}

MeasureUnit *MeasureUnit::createMillibar(UErrorCode &status) {
    return MeasureUnit::create(18, 6, status);
}

MeasureUnit MeasureUnit::getMillibar() {
    return MeasureUnit(18, 6);
}

MeasureUnit *MeasureUnit::createMillimeterOfMercury(UErrorCode &status) {
    return MeasureUnit::create(18, 7, status);
}

MeasureUnit MeasureUnit::getMillimeterOfMercury() {
    return MeasureUnit(18, 7);
}

MeasureUnit *MeasureUnit::createPascal(UErrorCode &status) {
    return MeasureUnit::create(18, 8, status);
}

MeasureUnit MeasureUnit::getPascal() {
    return MeasureUnit(18, 8);
}

MeasureUnit *MeasureUnit::createPoundPerSquareInch(UErrorCode &status) {
    return MeasureUnit::create(18, 9, status);
}

MeasureUnit MeasureUnit::getPoundPerSquareInch() {
    return MeasureUnit(18, 9);
}

MeasureUnit *MeasureUnit::createKilometerPerHour(UErrorCode &status) {
    return MeasureUnit::create(19, 0, status);
}

MeasureUnit MeasureUnit::getKilometerPerHour() {
    return MeasureUnit(19, 0);
}

MeasureUnit *MeasureUnit::createKnot(UErrorCode &status) {
    return MeasureUnit::create(19, 1, status);
}

MeasureUnit MeasureUnit::getKnot() {
    return MeasureUnit(19, 1);
}

MeasureUnit *MeasureUnit::createMeterPerSecond(UErrorCode &status) {
    return MeasureUnit::create(19, 2, status);
}

MeasureUnit MeasureUnit::getMeterPerSecond() {
    return MeasureUnit(19, 2);
}

MeasureUnit *MeasureUnit::createMilePerHour(UErrorCode &status) {
    return MeasureUnit::create(19, 3, status);
}

MeasureUnit MeasureUnit::getMilePerHour() {
    return MeasureUnit(19, 3);
}

MeasureUnit *MeasureUnit::createCelsius(UErrorCode &status) {
    return MeasureUnit::create(20, 0, status);
}

MeasureUnit MeasureUnit::getCelsius() {
    return MeasureUnit(20, 0);
}

MeasureUnit *MeasureUnit::createFahrenheit(UErrorCode &status) {
    return MeasureUnit::create(20, 1, status);
}

MeasureUnit MeasureUnit::getFahrenheit() {
    return MeasureUnit(20, 1);
}

MeasureUnit *MeasureUnit::createGenericTemperature(UErrorCode &status) {
    return MeasureUnit::create(20, 2, status);
}

MeasureUnit MeasureUnit::getGenericTemperature() {
    return MeasureUnit(20, 2);
}

MeasureUnit *MeasureUnit::createKelvin(UErrorCode &status) {
    return MeasureUnit::create(20, 3, status);
}

MeasureUnit MeasureUnit::getKelvin() {
    return MeasureUnit(20, 3);
}

MeasureUnit *MeasureUnit::createNewtonMeter(UErrorCode &status) {
    return MeasureUnit::create(21, 0, status);
}

MeasureUnit MeasureUnit::getNewtonMeter() {
    return MeasureUnit(21, 0);
}

MeasureUnit *MeasureUnit::createPoundFoot(UErrorCode &status) {
    return MeasureUnit::create(21, 1, status);
}

MeasureUnit MeasureUnit::getPoundFoot() {
    return MeasureUnit(21, 1);
}

MeasureUnit *MeasureUnit::createAcreFoot(UErrorCode &status) {
    return MeasureUnit::create(22, 0, status);
}

MeasureUnit MeasureUnit::getAcreFoot() {
    return MeasureUnit(22, 0);
}

MeasureUnit *MeasureUnit::createBarrel(UErrorCode &status) {
    return MeasureUnit::create(22, 1, status);
}

MeasureUnit MeasureUnit::getBarrel() {
    return MeasureUnit(22, 1);
}

MeasureUnit *MeasureUnit::createBushel(UErrorCode &status) {
    return MeasureUnit::create(22, 2, status);
}

MeasureUnit MeasureUnit::getBushel() {
    return MeasureUnit(22, 2);
}

MeasureUnit *MeasureUnit::createCentiliter(UErrorCode &status) {
    return MeasureUnit::create(22, 3, status);
}

MeasureUnit MeasureUnit::getCentiliter() {
    return MeasureUnit(22, 3);
}

MeasureUnit *MeasureUnit::createCubicCentimeter(UErrorCode &status) {
    return MeasureUnit::create(22, 4, status);
}

MeasureUnit MeasureUnit::getCubicCentimeter() {
    return MeasureUnit(22, 4);
}

MeasureUnit *MeasureUnit::createCubicFoot(UErrorCode &status) {
    return MeasureUnit::create(22, 5, status);
}

MeasureUnit MeasureUnit::getCubicFoot() {
    return MeasureUnit(22, 5);
}

MeasureUnit *MeasureUnit::createCubicInch(UErrorCode &status) {
    return MeasureUnit::create(22, 6, status);
}

MeasureUnit MeasureUnit::getCubicInch() {
    return MeasureUnit(22, 6);
}

MeasureUnit *MeasureUnit::createCubicKilometer(UErrorCode &status) {
    return MeasureUnit::create(22, 7, status);
}

MeasureUnit MeasureUnit::getCubicKilometer() {
    return MeasureUnit(22, 7);
}

MeasureUnit *MeasureUnit::createCubicMeter(UErrorCode &status) {
    return MeasureUnit::create(22, 8, status);
}

MeasureUnit MeasureUnit::getCubicMeter() {
    return MeasureUnit(22, 8);
}

MeasureUnit *MeasureUnit::createCubicMile(UErrorCode &status) {
    return MeasureUnit::create(22, 9, status);
}

MeasureUnit MeasureUnit::getCubicMile() {
    return MeasureUnit(22, 9);
}

MeasureUnit *MeasureUnit::createCubicYard(UErrorCode &status) {
    return MeasureUnit::create(22, 10, status);
}

MeasureUnit MeasureUnit::getCubicYard() {
    return MeasureUnit(22, 10);
}

MeasureUnit *MeasureUnit::createCup(UErrorCode &status) {
    return MeasureUnit::create(22, 11, status);
}

MeasureUnit MeasureUnit::getCup() {
    return MeasureUnit(22, 11);
}

MeasureUnit *MeasureUnit::createCupMetric(UErrorCode &status) {
    return MeasureUnit::create(22, 12, status);
}

MeasureUnit MeasureUnit::getCupMetric() {
    return MeasureUnit(22, 12);
}

MeasureUnit *MeasureUnit::createDeciliter(UErrorCode &status) {
    return MeasureUnit::create(22, 13, status);
}

MeasureUnit MeasureUnit::getDeciliter() {
    return MeasureUnit(22, 13);
}

MeasureUnit *MeasureUnit::createDessertSpoon(UErrorCode &status) {
    return MeasureUnit::create(22, 14, status);
}

MeasureUnit MeasureUnit::getDessertSpoon() {
    return MeasureUnit(22, 14);
}

MeasureUnit *MeasureUnit::createDessertSpoonImperial(UErrorCode &status) {
    return MeasureUnit::create(22, 15, status);
}

MeasureUnit MeasureUnit::getDessertSpoonImperial() {
    return MeasureUnit(22, 15);
}

MeasureUnit *MeasureUnit::createDram(UErrorCode &status) {
    return MeasureUnit::create(22, 16, status);
}

MeasureUnit MeasureUnit::getDram() {
    return MeasureUnit(22, 16);
}

MeasureUnit *MeasureUnit::createDrop(UErrorCode &status) {
    return MeasureUnit::create(22, 17, status);
}

MeasureUnit MeasureUnit::getDrop() {
    return MeasureUnit(22, 17);
}

MeasureUnit *MeasureUnit::createFluidOunce(UErrorCode &status) {
    return MeasureUnit::create(22, 18, status);
}

MeasureUnit MeasureUnit::getFluidOunce() {
    return MeasureUnit(22, 18);
}

MeasureUnit *MeasureUnit::createFluidOunceImperial(UErrorCode &status) {
    return MeasureUnit::create(22, 19, status);
}

MeasureUnit MeasureUnit::getFluidOunceImperial() {
    return MeasureUnit(22, 19);
}

MeasureUnit *MeasureUnit::createGallon(UErrorCode &status) {
    return MeasureUnit::create(22, 20, status);
}

MeasureUnit MeasureUnit::getGallon() {
    return MeasureUnit(22, 20);
}

MeasureUnit *MeasureUnit::createGallonImperial(UErrorCode &status) {
    return MeasureUnit::create(22, 21, status);
}

MeasureUnit MeasureUnit::getGallonImperial() {
    return MeasureUnit(22, 21);
}

MeasureUnit *MeasureUnit::createHectoliter(UErrorCode &status) {
    return MeasureUnit::create(22, 22, status);
}

MeasureUnit MeasureUnit::getHectoliter() {
    return MeasureUnit(22, 22);
}

MeasureUnit *MeasureUnit::createJigger(UErrorCode &status) {
    return MeasureUnit::create(22, 23, status);
}

MeasureUnit MeasureUnit::getJigger() {
    return MeasureUnit(22, 23);
}

MeasureUnit *MeasureUnit::createLiter(UErrorCode &status) {
    return MeasureUnit::create(22, 24, status);
}

MeasureUnit MeasureUnit::getLiter() {
    return MeasureUnit(22, 24);
}

MeasureUnit *MeasureUnit::createMegaliter(UErrorCode &status) {
    return MeasureUnit::create(22, 25, status);
}

MeasureUnit MeasureUnit::getMegaliter() {
    return MeasureUnit(22, 25);
}

MeasureUnit *MeasureUnit::createMilliliter(UErrorCode &status) {
    return MeasureUnit::create(22, 26, status);
}

MeasureUnit MeasureUnit::getMilliliter() {
    return MeasureUnit(22, 26);
}

MeasureUnit *MeasureUnit::createPinch(UErrorCode &status) {
    return MeasureUnit::create(22, 27, status);
}

MeasureUnit MeasureUnit::getPinch() {
    return MeasureUnit(22, 27);
}

MeasureUnit *MeasureUnit::createPint(UErrorCode &status) {
    return MeasureUnit::create(22, 28, status);
}

MeasureUnit MeasureUnit::getPint() {
    return MeasureUnit(22, 28);
}

MeasureUnit *MeasureUnit::createPintMetric(UErrorCode &status) {
    return MeasureUnit::create(22, 29, status);
}

MeasureUnit MeasureUnit::getPintMetric() {
    return MeasureUnit(22, 29);
}

MeasureUnit *MeasureUnit::createQuart(UErrorCode &status) {
    return MeasureUnit::create(22, 30, status);
}

MeasureUnit MeasureUnit::getQuart() {
    return MeasureUnit(22, 30);
}

MeasureUnit *MeasureUnit::createQuartImperial(UErrorCode &status) {
    return MeasureUnit::create(22, 31, status);
}

MeasureUnit MeasureUnit::getQuartImperial() {
    return MeasureUnit(22, 31);
}

MeasureUnit *MeasureUnit::createTablespoon(UErrorCode &status) {
    return MeasureUnit::create(22, 32, status);
}

MeasureUnit MeasureUnit::getTablespoon() {
    return MeasureUnit(22, 32);
}

MeasureUnit *MeasureUnit::createTeaspoon(UErrorCode &status) {
    return MeasureUnit::create(22, 33, status);
}

MeasureUnit MeasureUnit::getTeaspoon() {
    return MeasureUnit(22, 33);
}

// End generated code for measunit.cpp

static int32_t binarySearch(
        const char * const * array, int32_t start, int32_t end, StringPiece key) {
    while (start < end) {
        int32_t mid = (start + end) / 2;
        int32_t cmp = StringPiece(array[mid]).compare(key);
        if (cmp < 0) {
            start = mid + 1;
            continue;
        }
        if (cmp == 0) {
            return mid;
        }
        end = mid;
    }
    return -1;
}

MeasureUnit::MeasureUnit() : MeasureUnit(kBaseTypeIdx, kBaseSubTypeIdx) {
}

MeasureUnit::MeasureUnit(int32_t typeId, int32_t subTypeId)
        : fImpl(nullptr), fSubTypeId(subTypeId), fTypeId(typeId) {
}

MeasureUnit::MeasureUnit(const MeasureUnit &other)
        : fImpl(nullptr) {
    *this = other;
}

MeasureUnit::MeasureUnit(MeasureUnit &&other) noexcept
        : fImpl(other.fImpl),
        fSubTypeId(other.fSubTypeId),
        fTypeId(other.fTypeId) {
    other.fImpl = nullptr;
}

MeasureUnit::MeasureUnit(MeasureUnitImpl&& impl)
        : fImpl(nullptr), fSubTypeId(-1), fTypeId(-1) {
    if (!findBySubType(impl.identifier.toStringPiece(), this)) {
        fImpl = new MeasureUnitImpl(std::move(impl));
    }
}

MeasureUnit &MeasureUnit::operator=(const MeasureUnit &other) {
    if (this == &other) {
        return *this;
    }
    if (fImpl != nullptr) {
        delete fImpl;
    }
    if (other.fImpl) {
        ErrorCode localStatus;
        fImpl = new MeasureUnitImpl(other.fImpl->copy(localStatus));
        if (!fImpl || localStatus.isFailure()) {
            // Unrecoverable allocation error; set to the default unit
            *this = MeasureUnit();
            return *this;
        }
    } else {
        fImpl = nullptr;
    }
    fTypeId = other.fTypeId;
    fSubTypeId = other.fSubTypeId;
    return *this;
}

MeasureUnit &MeasureUnit::operator=(MeasureUnit &&other) noexcept {
    if (this == &other) {
        return *this;
    }
    if (fImpl != nullptr) {
        delete fImpl;
    }
    fImpl = other.fImpl;
    other.fImpl = nullptr;
    fTypeId = other.fTypeId;
    fSubTypeId = other.fSubTypeId;
    return *this;
}

MeasureUnit *MeasureUnit::clone() const {
    return new MeasureUnit(*this);
}

MeasureUnit::~MeasureUnit() {
    if (fImpl != nullptr) {
        delete fImpl;
        fImpl = nullptr;
    }
}

const char *MeasureUnit::getType() const {
    // We have a type & subtype only if fTypeId is present.
    if (fTypeId == -1) {
        return "";
    }
    return gTypes[fTypeId];
}

const char *MeasureUnit::getSubtype() const {
    // We have a type & subtype only if fTypeId is present.
    if (fTypeId == -1) {
        return "";
    }
    return getIdentifier();
}

const char *MeasureUnit::getIdentifier() const {
    return fImpl ? fImpl->identifier.data() : gSubTypes[getOffset()];
}

bool MeasureUnit::operator==(const UObject& other) const {
    if (this == &other) {  // Same object, equal
        return true;
    }
    if (typeid(*this) != typeid(other)) { // Different types, not equal
        return false;
    }
    const MeasureUnit &rhs = static_cast<const MeasureUnit&>(other);
    return uprv_strcmp(getIdentifier(), rhs.getIdentifier()) == 0;
}

int32_t MeasureUnit::getAvailable(
        MeasureUnit *dest,
        int32_t destCapacity,
        UErrorCode &errorCode) {
    if (U_FAILURE(errorCode)) {
        return 0;
    }
    if (destCapacity < UPRV_LENGTHOF(gSubTypes)) {
        errorCode = U_BUFFER_OVERFLOW_ERROR;
        return UPRV_LENGTHOF(gSubTypes);
    }
    int32_t idx = 0;
    for (int32_t typeIdx = 0; typeIdx < UPRV_LENGTHOF(gTypes); ++typeIdx) {
        int32_t len = gOffsets[typeIdx + 1] - gOffsets[typeIdx];
        for (int32_t subTypeIdx = 0; subTypeIdx < len; ++subTypeIdx) {
            dest[idx].setTo(typeIdx, subTypeIdx);
            ++idx;
        }
    }
    U_ASSERT(idx == UPRV_LENGTHOF(gSubTypes));
    return UPRV_LENGTHOF(gSubTypes);
}

int32_t MeasureUnit::getAvailable(
        const char *type,
        MeasureUnit *dest,
        int32_t destCapacity,
        UErrorCode &errorCode) {
    if (U_FAILURE(errorCode)) {
        return 0;
    }
    int32_t typeIdx = binarySearch(gTypes, 0, UPRV_LENGTHOF(gTypes), type);
    if (typeIdx == -1) {
        return 0;
    }
    int32_t len = gOffsets[typeIdx + 1] - gOffsets[typeIdx];
    if (destCapacity < len) {
        errorCode = U_BUFFER_OVERFLOW_ERROR;
        return len;
    }
    for (int subTypeIdx = 0; subTypeIdx < len; ++subTypeIdx) {
        dest[subTypeIdx].setTo(typeIdx, subTypeIdx);
    }
    return len;
}

StringEnumeration* MeasureUnit::getAvailableTypes(UErrorCode &errorCode) {
    UEnumeration *uenum = uenum_openCharStringsEnumeration(
            gTypes, UPRV_LENGTHOF(gTypes), &errorCode);
    if (U_FAILURE(errorCode)) {
        uenum_close(uenum);
        return NULL;
    }
    StringEnumeration *result = new UStringEnumeration(uenum);
    if (result == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        uenum_close(uenum);
        return NULL;
    }
    return result;
}

bool MeasureUnit::findBySubType(StringPiece subType, MeasureUnit* output) {
    // Sanity checking kCurrencyOffset and final entry in gOffsets
    U_ASSERT(uprv_strcmp(gTypes[kCurrencyOffset], "currency") == 0);
    U_ASSERT(gOffsets[UPRV_LENGTHOF(gOffsets) - 1] == UPRV_LENGTHOF(gSubTypes));

    for (int32_t t = 0; t < UPRV_LENGTHOF(gOffsets) - 1; t++) {
        // Skip currency units
        if (t == kCurrencyOffset) {
            continue;
        }
        int32_t st = binarySearch(gSubTypes, gOffsets[t], gOffsets[t + 1], subType);
#if APPLE_ICU_CHANGES
// rdar:/
        if (st < 0) {
            // Ugly hack to deal with rdar://77037602 -- the code in serialize() in measunit_extra.cpp
            // normalizes "kilowatt-hour" to "hour-kilowatt", even though "kilowatt-hour" is the string
            // stored in gSubTypes.  Trying to fix that normalization was having too many side effects,
            // so I'm just "un-normalizing" the ID here.  If we run into more examples of this, we
            // can add a table of unit name aliases.    --rtg 4/23/21
            if (uprv_strcmp(subType.data(), "hour-kilowatt") == 0) {
                st = binarySearch(gSubTypes, gOffsets[t], gOffsets[t + 1], StringPiece("kilowatt-hour"));
            }
        }
#endif  // APPLE_ICU_CHANGES
        if (st >= 0) {
            output->setTo(t, st - gOffsets[t]);
            return true;
        }
    }
    return false;
}

MeasureUnit *MeasureUnit::create(int typeId, int subTypeId, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return NULL;
    }
    MeasureUnit *result = new MeasureUnit(typeId, subTypeId);
    if (result == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
    return result;
}

void MeasureUnit::initTime(const char *timeId) {
    int32_t result = binarySearch(gTypes, 0, UPRV_LENGTHOF(gTypes), "duration");
    U_ASSERT(result != -1);
    fTypeId = result;
    result = binarySearch(gSubTypes, gOffsets[fTypeId], gOffsets[fTypeId + 1], timeId);
    U_ASSERT(result != -1);
    fSubTypeId = result - gOffsets[fTypeId];
}

void MeasureUnit::initCurrency(StringPiece isoCurrency) {
    int32_t result = binarySearch(gTypes, 0, UPRV_LENGTHOF(gTypes), "currency");
    U_ASSERT(result != -1);
    fTypeId = result;
    result = binarySearch(
            gSubTypes, gOffsets[fTypeId], gOffsets[fTypeId + 1], isoCurrency);
    if (result == -1) {
        fImpl = new MeasureUnitImpl(MeasureUnitImpl::forCurrencyCode(isoCurrency));
        if (fImpl) {
            fSubTypeId = -1;
            return;
        }
        // malloc error: fall back to the undefined currency
        result = binarySearch(
            gSubTypes, gOffsets[fTypeId], gOffsets[fTypeId + 1], kDefaultCurrency8);
        U_ASSERT(result != -1);
    }
    fSubTypeId = result - gOffsets[fTypeId];
}

void MeasureUnit::setTo(int32_t typeId, int32_t subTypeId) {
    fTypeId = typeId;
    fSubTypeId = subTypeId;
    if (fImpl != nullptr) {
        delete fImpl;
        fImpl = nullptr;
    }
}

int32_t MeasureUnit::getOffset() const {
    if (fTypeId < 0 || fSubTypeId < 0) {
        return -1;
    }
    return gOffsets[fTypeId] + fSubTypeId;
}

MeasureUnitImpl MeasureUnitImpl::copy(UErrorCode &status) const {
    MeasureUnitImpl result;
    result.complexity = complexity;
    result.identifier.append(identifier, status);
    for (int32_t i = 0; i < singleUnits.length(); i++) {
        SingleUnitImpl *item = result.singleUnits.emplaceBack(*singleUnits[i]);
        if (!item) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return result;
        }
    }
    return result;
}

#if APPLE_ICU_CHANGES
// rdar:/
//--------------------------------------------------------------------------------------------------
// Apple additions

MeasureUnit* MeasureUnit::createFromUAMeasureUnit(UAMeasureUnit unit, UErrorCode* status )
{
    MeasureUnit * munit = NULL;
    switch (unit) {
        case UAMEASUNIT_ACCELERATION_G_FORCE:   munit = MeasureUnit::createGForce(*status);      break;
        case UAMEASUNIT_ACCELERATION_METER_PER_SECOND_SQUARED:  munit = MeasureUnit::createMeterPerSecondSquared(*status); break;

        case UAMEASUNIT_ANGLE_DEGREE:           munit = MeasureUnit::createDegree(*status);      break;
        case UAMEASUNIT_ANGLE_ARC_MINUTE:       munit = MeasureUnit::createArcMinute(*status);   break;
        case UAMEASUNIT_ANGLE_ARC_SECOND:       munit = MeasureUnit::createArcSecond(*status);   break;
        case UAMEASUNIT_ANGLE_RADIAN:           munit = MeasureUnit::createRadian(*status);      break;
        case UAMEASUNIT_ANGLE_REVOLUTION:       munit = MeasureUnit::createRevolutionAngle(*status); break;

        case UAMEASUNIT_AREA_SQUARE_METER:      munit = MeasureUnit::createSquareMeter(*status);     break;
        case UAMEASUNIT_AREA_SQUARE_KILOMETER:  munit = MeasureUnit::createSquareKilometer(*status); break;
        case UAMEASUNIT_AREA_SQUARE_FOOT:       munit = MeasureUnit::createSquareFoot(*status);      break;
        case UAMEASUNIT_AREA_SQUARE_MILE:       munit = MeasureUnit::createSquareMile(*status);      break;
        case UAMEASUNIT_AREA_ACRE:              munit = MeasureUnit::createAcre(*status);            break;
        case UAMEASUNIT_AREA_HECTARE:           munit = MeasureUnit::createHectare(*status);         break;
        case UAMEASUNIT_AREA_SQUARE_CENTIMETER: munit = MeasureUnit::createSquareCentimeter(*status); break;
        case UAMEASUNIT_AREA_SQUARE_INCH:       munit = MeasureUnit::createSquareInch(*status);      break;
        case UAMEASUNIT_AREA_SQUARE_YARD:       munit = MeasureUnit::createSquareYard(*status);      break;
        case UAMEASUNIT_AREA_DUNAM:             munit = MeasureUnit::createDunam(*status);           break;

        case UAMEASUNIT_DURATION_YEAR:          munit = MeasureUnit::createYear(*status);        break;
        case UAMEASUNIT_DURATION_MONTH:         munit = MeasureUnit::createMonth(*status);       break;
        case UAMEASUNIT_DURATION_WEEK:          munit = MeasureUnit::createWeek(*status);        break;
        case UAMEASUNIT_DURATION_DAY:           munit = MeasureUnit::createDay(*status);         break;
        case UAMEASUNIT_DURATION_HOUR:          munit = MeasureUnit::createHour(*status);        break;
        case UAMEASUNIT_DURATION_MINUTE:        munit = MeasureUnit::createMinute(*status);      break;
        case UAMEASUNIT_DURATION_SECOND:        munit = MeasureUnit::createSecond(*status);      break;
        case UAMEASUNIT_DURATION_MILLISECOND:   munit = MeasureUnit::createMillisecond(*status); break;
        case UAMEASUNIT_DURATION_MICROSECOND:   munit = MeasureUnit::createMicrosecond(*status); break;
        case UAMEASUNIT_DURATION_NANOSECOND:    munit = MeasureUnit::createNanosecond(*status);  break;
        case UAMEASUNIT_DURATION_CENTURY:       munit = MeasureUnit::createCentury(*status);     break;
        case UAMEASUNIT_DURATION_YEAR_PERSON:   munit = MeasureUnit::createYearPerson(*status);  break;
        case UAMEASUNIT_DURATION_MONTH_PERSON:  munit = MeasureUnit::createMonthPerson(*status); break;
        case UAMEASUNIT_DURATION_WEEK_PERSON:   munit = MeasureUnit::createWeekPerson(*status);  break;
        case UAMEASUNIT_DURATION_DAY_PERSON:    munit = MeasureUnit::createDayPerson(*status);   break;
        case UAMEASUNIT_DURATION_DECADE:        munit = MeasureUnit::createDecade(*status);      break;
        case UAMEASUNIT_DURATION_QUARTER:       munit = MeasureUnit::createQuarter(*status);     break;

        case UAMEASUNIT_LENGTH_METER:           munit = MeasureUnit::createMeter(*status);       break;
        case UAMEASUNIT_LENGTH_CENTIMETER:      munit = MeasureUnit::createCentimeter(*status);  break;
        case UAMEASUNIT_LENGTH_KILOMETER:       munit = MeasureUnit::createKilometer(*status);   break;
        case UAMEASUNIT_LENGTH_MILLIMETER:      munit = MeasureUnit::createMillimeter(*status);  break;
        case UAMEASUNIT_LENGTH_PICOMETER:       munit = MeasureUnit::createPicometer(*status);   break;
        case UAMEASUNIT_LENGTH_FOOT:            munit = MeasureUnit::createFoot(*status);        break;
        case UAMEASUNIT_LENGTH_INCH:            munit = MeasureUnit::createInch(*status);        break;
        case UAMEASUNIT_LENGTH_MILE:            munit = MeasureUnit::createMile(*status);        break;
        case UAMEASUNIT_LENGTH_YARD:            munit = MeasureUnit::createYard(*status);        break;
        case UAMEASUNIT_LENGTH_LIGHT_YEAR:      munit = MeasureUnit::createLightYear(*status);   break;
        case UAMEASUNIT_LENGTH_DECIMETER:       munit = MeasureUnit::createDecimeter(*status);   break;
        case UAMEASUNIT_LENGTH_MICROMETER:      munit = MeasureUnit::createMicrometer(*status);  break;
        case UAMEASUNIT_LENGTH_NANOMETER:       munit = MeasureUnit::createNanometer(*status);   break;
        case UAMEASUNIT_LENGTH_NAUTICAL_MILE:   munit = MeasureUnit::createNauticalMile(*status); break;
        case UAMEASUNIT_LENGTH_FATHOM:          munit = MeasureUnit::createFathom(*status);      break;
        case UAMEASUNIT_LENGTH_FURLONG:         munit = MeasureUnit::createFurlong(*status);     break;
        case UAMEASUNIT_LENGTH_ASTRONOMICAL_UNIT: munit = MeasureUnit::createAstronomicalUnit(*status); break;
        case UAMEASUNIT_LENGTH_PARSEC:          munit = MeasureUnit::createParsec(*status);      break;
        case UAMEASUNIT_LENGTH_MILE_SCANDINAVIAN: munit = MeasureUnit::createMileScandinavian(*status); break;
        case UAMEASUNIT_LENGTH_POINT:           munit = MeasureUnit::createPoint(*status);       break;
        case UAMEASUNIT_LENGTH_SOLAR_RADIUS:    munit = MeasureUnit::createSolarRadius(*status); break;
        case UAMEASUNIT_LENGTH_EARTH_RADIUS:    munit = MeasureUnit::createEarthRadius(*status); break;

        case UAMEASUNIT_MASS_GRAM:              munit = MeasureUnit::createGram(*status);        break;
        case UAMEASUNIT_MASS_KILOGRAM:          munit = MeasureUnit::createKilogram(*status);    break;
        case UAMEASUNIT_MASS_OUNCE:             munit = MeasureUnit::createOunce(*status);       break;
        case UAMEASUNIT_MASS_POUND:             munit = MeasureUnit::createPound(*status);       break;
        case UAMEASUNIT_MASS_STONE:             munit = MeasureUnit::createStone(*status);       break;
        case UAMEASUNIT_MASS_MICROGRAM:         munit = MeasureUnit::createMicrogram(*status);   break;
        case UAMEASUNIT_MASS_MILLIGRAM:         munit = MeasureUnit::createMilligram(*status);   break;
        case UAMEASUNIT_MASS_TONNE:             munit = MeasureUnit::createTonne(*status);       break;
        case UAMEASUNIT_MASS_TON:               munit = MeasureUnit::createTon(*status);         break;
        case UAMEASUNIT_MASS_CARAT:             munit = MeasureUnit::createCarat(*status);       break;
        case UAMEASUNIT_MASS_OUNCE_TROY:        munit = MeasureUnit::createOunceTroy(*status);   break;
        case UAMEASUNIT_MASS_DALTON:            munit = MeasureUnit::createDalton(*status);      break;
        case UAMEASUNIT_MASS_EARTH_MASS:        munit = MeasureUnit::createEarthMass(*status);   break;
        case UAMEASUNIT_MASS_SOLAR_MASS:        munit = MeasureUnit::createSolarMass(*status);   break;
        case UAMEASUNIT_MASS_GRAIN:             munit = MeasureUnit::createGrain(*status);       break;

        case UAMEASUNIT_POWER_WATT:             munit = MeasureUnit::createWatt(*status);        break;
        case UAMEASUNIT_POWER_KILOWATT:         munit = MeasureUnit::createKilowatt(*status);    break;
        case UAMEASUNIT_POWER_HORSEPOWER:       munit = MeasureUnit::createHorsepower(*status);  break;
        case UAMEASUNIT_POWER_MILLIWATT:        munit = MeasureUnit::createMilliwatt(*status);   break;
        case UAMEASUNIT_POWER_MEGAWATT:         munit = MeasureUnit::createMegawatt(*status);    break;
        case UAMEASUNIT_POWER_GIGAWATT:         munit = MeasureUnit::createGigawatt(*status);    break;

        case UAMEASUNIT_PRESSURE_HECTOPASCAL:   munit = MeasureUnit::createHectopascal(*status); break;
        case UAMEASUNIT_PRESSURE_INCH_HG:       munit = MeasureUnit::createInchHg(*status);      break;
        case UAMEASUNIT_PRESSURE_MILLIBAR:      munit = MeasureUnit::createMillibar(*status);    break;
        case UAMEASUNIT_PRESSURE_MILLIMETER_OF_MERCURY:  munit = MeasureUnit::createMillimeterOfMercury(*status); break;
        case UAMEASUNIT_PRESSURE_POUND_PER_SQUARE_INCH: munit = MeasureUnit::createPoundPerSquareInch(*status);  break;
        case UAMEASUNIT_PRESSURE_ATMOSPHERE:    munit = MeasureUnit::createAtmosphere(*status);  break;
        case UAMEASUNIT_PRESSURE_KILOPASCAL:    munit = MeasureUnit::createKilopascal(*status);  break;
        case UAMEASUNIT_PRESSURE_MEGAPASCAL:    munit = MeasureUnit::createMegapascal(*status);  break;
        case UAMEASUNIT_PRESSURE_PASCAL:        munit = MeasureUnit::createPascal(*status);      break;
        case UAMEASUNIT_PRESSURE_BAR:           munit = MeasureUnit::createBar(*status);         break;

        case UAMEASUNIT_SPEED_METER_PER_SECOND:   munit = MeasureUnit::createMeterPerSecond(*status);   break;
        case UAMEASUNIT_SPEED_KILOMETER_PER_HOUR: munit = MeasureUnit::createKilometerPerHour(*status); break;
        case UAMEASUNIT_SPEED_MILE_PER_HOUR:      munit = MeasureUnit::createMilePerHour(*status);      break;
        case UAMEASUNIT_SPEED_KNOT:               munit = MeasureUnit::createKnot(*status);      break;

        case UAMEASUNIT_TEMPERATURE_CELSIUS:    munit = MeasureUnit::createCelsius(*status);     break;
        case UAMEASUNIT_TEMPERATURE_FAHRENHEIT: munit = MeasureUnit::createFahrenheit(*status);  break;
        case UAMEASUNIT_TEMPERATURE_KELVIN:     munit = MeasureUnit::createKelvin(*status);      break;
        case UAMEASUNIT_TEMPERATURE_GENERIC:    munit = MeasureUnit::createGenericTemperature(*status); break;

        case UAMEASUNIT_VOLUME_LITER:           munit = MeasureUnit::createLiter(*status);          break;
        case UAMEASUNIT_VOLUME_CUBIC_KILOMETER: munit = MeasureUnit::createCubicKilometer(*status); break;
        case UAMEASUNIT_VOLUME_CUBIC_MILE:      munit = MeasureUnit::createCubicMile(*status);      break;
        case UAMEASUNIT_VOLUME_MILLILITER:      munit = MeasureUnit::createMilliliter(*status);     break;
        case UAMEASUNIT_VOLUME_CENTILITER:      munit = MeasureUnit::createCentiliter(*status);     break;
        case UAMEASUNIT_VOLUME_DECILITER:       munit = MeasureUnit::createDeciliter(*status);      break;
        case UAMEASUNIT_VOLUME_HECTOLITER:      munit = MeasureUnit::createHectoliter(*status);     break;
        case UAMEASUNIT_VOLUME_MEGALITER:       munit = MeasureUnit::createMegaliter(*status);      break;
        case UAMEASUNIT_VOLUME_CUBIC_CENTIMETER: munit = MeasureUnit::createCubicCentimeter(*status); break;
        case UAMEASUNIT_VOLUME_CUBIC_METER:     munit = MeasureUnit::createCubicMeter(*status);     break;
        case UAMEASUNIT_VOLUME_CUBIC_INCH:      munit = MeasureUnit::createCubicInch(*status);      break;
        case UAMEASUNIT_VOLUME_CUBIC_FOOT:      munit = MeasureUnit::createCubicFoot(*status);      break;
        case UAMEASUNIT_VOLUME_CUBIC_YARD:      munit = MeasureUnit::createCubicYard(*status);      break;
        case UAMEASUNIT_VOLUME_ACRE_FOOT:       munit = MeasureUnit::createAcreFoot(*status);       break;
        case UAMEASUNIT_VOLUME_BUSHEL:          munit = MeasureUnit::createBushel(*status);         break;
        case UAMEASUNIT_VOLUME_TEASPOON:        munit = MeasureUnit::createTeaspoon(*status);       break;
        case UAMEASUNIT_VOLUME_TABLESPOON:      munit = MeasureUnit::createTablespoon(*status);     break;
        case UAMEASUNIT_VOLUME_FLUID_OUNCE:     munit = MeasureUnit::createFluidOunce(*status);     break;
        case UAMEASUNIT_VOLUME_CUP:             munit = MeasureUnit::createCup(*status);            break;
        case UAMEASUNIT_VOLUME_PINT:            munit = MeasureUnit::createPint(*status);           break;
        case UAMEASUNIT_VOLUME_QUART:           munit = MeasureUnit::createQuart(*status);          break;
        case UAMEASUNIT_VOLUME_GALLON:          munit = MeasureUnit::createGallon(*status);         break;
        case UAMEASUNIT_VOLUME_CUP_METRIC:      munit = MeasureUnit::createCupMetric(*status);      break;
        case UAMEASUNIT_VOLUME_PINT_METRIC:     munit = MeasureUnit::createPintMetric(*status);     break;
        case UAMEASUNIT_VOLUME_GALLON_IMPERIAL: munit = MeasureUnit::createGallonImperial(*status); break;
        case UAMEASUNIT_VOLUME_FLUID_OUNCE_IMPERIAL: munit = MeasureUnit::createFluidOunceImperial(*status); break;
        case UAMEASUNIT_VOLUME_BARREL:          munit = MeasureUnit::createBarrel(*status);         break;
        case UAMEASUNIT_VOLUME_DESSERT_SPOON:   munit = MeasureUnit::createDessertSpoon(*status);   break;
        case UAMEASUNIT_VOLUME_DESSERT_SPOON_IMPERIAL: munit = MeasureUnit::createDessertSpoonImperial(*status); break;
        case UAMEASUNIT_VOLUME_DRAM:            munit = MeasureUnit::createDram(*status);           break;
        case UAMEASUNIT_VOLUME_DROP:            munit = MeasureUnit::createDrop(*status);           break;
        case UAMEASUNIT_VOLUME_JIGGER:          munit = MeasureUnit::createJigger(*status);         break;
        case UAMEASUNIT_VOLUME_PINCH:           munit = MeasureUnit::createPinch(*status);          break;
        case UAMEASUNIT_VOLUME_QUART_IMPERIAL:  munit = MeasureUnit::createQuartImperial(*status);  break;

        case UAMEASUNIT_ENERGY_JOULE:           munit = MeasureUnit::createJoule(*status);          break;
        case UAMEASUNIT_ENERGY_KILOJOULE:       munit = MeasureUnit::createKilojoule(*status);      break;
        case UAMEASUNIT_ENERGY_CALORIE:         munit = MeasureUnit::createCalorie(*status);        break;
        case UAMEASUNIT_ENERGY_KILOCALORIE:     munit = MeasureUnit::createKilocalorie(*status);    break;
        case UAMEASUNIT_ENERGY_FOODCALORIE:     munit = MeasureUnit::createFoodcalorie(*status);    break;
        case UAMEASUNIT_ENERGY_KILOWATT_HOUR:   munit = MeasureUnit::createKilowattHour(*status);   break;
        case UAMEASUNIT_ENERGY_ELECTRONVOLT:    munit = MeasureUnit::createElectronvolt(*status);   break;
        case UAMEASUNIT_ENERGY_BRITISH_THERMAL_UNIT: munit = MeasureUnit::createBritishThermalUnit(*status); break;
        case UAMEASUNIT_ENERGY_THERM_US:        munit = MeasureUnit::createThermUs(*status);        break;

        case UAMEASUNIT_CONSUMPTION_LITER_PER_KILOMETER: munit = MeasureUnit::createLiterPerKilometer(*status); break;
        case UAMEASUNIT_CONSUMPTION_MILE_PER_GALLON:     munit = MeasureUnit::createMilePerGallon(*status);     break;
        case UAMEASUNIT_CONSUMPTION_LITER_PER_100_KILOMETERs: munit = MeasureUnit::createLiterPer100Kilometers(*status); break;
        case UAMEASUNIT_CONSUMPTION_MILE_PER_GALLON_IMPERIAL: munit = MeasureUnit::createMilePerGallonImperial(*status); break;

        case UAMEASUNIT_DIGITAL_BIT:            munit = MeasureUnit::createBit(*status);         break;
        case UAMEASUNIT_DIGITAL_BYTE:           munit = MeasureUnit::createByte(*status);        break;
        case UAMEASUNIT_DIGITAL_GIGABIT:        munit = MeasureUnit::createGigabit(*status);     break;
        case UAMEASUNIT_DIGITAL_GIGABYTE:       munit = MeasureUnit::createGigabyte(*status);    break;
        case UAMEASUNIT_DIGITAL_KILOBIT:        munit = MeasureUnit::createKilobit(*status);     break;
        case UAMEASUNIT_DIGITAL_KILOBYTE:       munit = MeasureUnit::createKilobyte(*status);    break;
        case UAMEASUNIT_DIGITAL_MEGABIT:        munit = MeasureUnit::createMegabit(*status);     break;
        case UAMEASUNIT_DIGITAL_MEGABYTE:       munit = MeasureUnit::createMegabyte(*status);    break;
        case UAMEASUNIT_DIGITAL_TERABIT:        munit = MeasureUnit::createTerabit(*status);     break;
        case UAMEASUNIT_DIGITAL_TERABYTE:       munit = MeasureUnit::createTerabyte(*status);    break;
        case UAMEASUNIT_DIGITAL_PETABYTE:       munit = MeasureUnit::createPetabyte(*status);    break;

        case UAMEASUNIT_ELECTRIC_AMPERE:        munit = MeasureUnit::createAmpere(*status);      break;
        case UAMEASUNIT_ELECTRIC_MILLIAMPERE:   munit = MeasureUnit::createMilliampere(*status); break;
        case UAMEASUNIT_ELECTRIC_OHM:           munit = MeasureUnit::createOhm(*status);         break;
        case UAMEASUNIT_ELECTRIC_VOLT:          munit = MeasureUnit::createVolt(*status);        break;

        case UAMEASUNIT_FREQUENCY_HERTZ:        munit = MeasureUnit::createHertz(*status);       break;
        case UAMEASUNIT_FREQUENCY_KILOHERTZ:    munit = MeasureUnit::createKilohertz(*status);   break;
        case UAMEASUNIT_FREQUENCY_MEGAHERTZ:    munit = MeasureUnit::createMegahertz(*status);   break;
        case UAMEASUNIT_FREQUENCY_GIGAHERTZ:    munit = MeasureUnit::createGigahertz(*status);   break;

        case UAMEASUNIT_LIGHT_LUX:              munit = MeasureUnit::createLux(*status);         break;
        case UAMEASUNIT_LIGHT_SOLAR_LUMINOSITY: munit = MeasureUnit::createSolarLuminosity(*status); break;
        case UAMEASUNIT_LIGHT_CANDELA:          munit = MeasureUnit::createCandela(*status);     break;
        case UAMEASUNIT_LIGHT_LUMEN:            munit = MeasureUnit::createLumen(*status);       break;

        case UAMEASUNIT_CONCENTRATION_KARAT:    munit = MeasureUnit::createKarat(*status);       break;
        case UAMEASUNIT_CONCENTRATION_MILLIGRAM_PER_DECILITER: munit = MeasureUnit::createMilligramOfglucosePerDeciliter(*status); break; // milligram-per-deciliter was renamed...
        case UAMEASUNIT_CONCENTRATION_MILLIMOLE_PER_LITER:     munit = MeasureUnit::createMillimolePerLiter(*status);     break;
        case UAMEASUNIT_CONCENTRATION_PART_PER_MILLION:        munit = MeasureUnit::createPartPerMillion(*status);        break;
        case UAMEASUNIT_CONCENTRATION_PERCENT:  munit = MeasureUnit::createPercent(*status);     break;
        case UAMEASUNIT_CONCENTRATION_PERMILLE: munit = MeasureUnit::createPermille(*status);    break;
        case UAMEASUNIT_CONCENTRATION_PERMYRIAD: munit = MeasureUnit::createPermyriad(*status);  break;
        case UAMEASUNIT_CONCENTRATION_MOLE:     munit = MeasureUnit::createMole(*status);        break;
        case UAMEASUNIT_CONCENTRATION_ITEM:     munit = MeasureUnit::createItem(*status);        break;
        case UAMEASUNIT_CONCENTRATION_MILLIGRAM_OFGLUCOSE_PER_DECILITER: munit = MeasureUnit::createMilligramOfglucosePerDeciliter(*status); break;

        case UAMEASUNIT_FORCE_NEWTON:           munit = MeasureUnit::createNewton(*status);      break;
        case UAMEASUNIT_FORCE_POUND_FORCE:      munit = MeasureUnit::createPoundForce(*status);  break;
        case UAMEASUNIT_FORCE_KILOWATT_HOUR_PER_100_KILOMETER: munit = MeasureUnit::createKilowattHourPer100Kilometer(*status); break;

        case UAMEASUNIT_TORQUE_NEWTON_METER:    munit = MeasureUnit::createNewtonMeter(*status); break;
        case UAMEASUNIT_TORQUE_POUND_FOOT:      munit = MeasureUnit::createPoundFoot(*status);   break;

        case UAMEASUNIT_GRAPHICS_EM:            munit = MeasureUnit::createEm(*status);         break;
        case UAMEASUNIT_GRAPHICS_PIXEL:         munit = MeasureUnit::createPixel(*status);      break;
        case UAMEASUNIT_GRAPHICS_MEGAPIXEL:     munit = MeasureUnit::createMegapixel(*status);  break;
        case UAMEASUNIT_GRAPHICS_PIXEL_PER_CENTIMETER: munit = MeasureUnit::createPixelPerCentimeter(*status); break;
        case UAMEASUNIT_GRAPHICS_PIXEL_PER_INCH:       munit = MeasureUnit::createPixelPerInch(*status);       break;
        case UAMEASUNIT_GRAPHICS_DOT_PER_CENTIMETER:   munit = MeasureUnit::createDotPerCentimeter(*status);   break;
        case UAMEASUNIT_GRAPHICS_DOT_PER_INCH :        munit = MeasureUnit::createDotPerInch(*status);         break;
        case UAMEASUNIT_GRAPHICS_DOT:           munit = MeasureUnit::createDot(*status);        break;

        default: *status = U_ILLEGAL_ARGUMENT_ERROR; break;
    }
    return munit;
}

static const UAMeasureUnit indexToUAMeasUnit[] = {
    // UAMeasureUnit                                  // UAMeasUnit vals # MeasUnit.getIndex()
    //                                                                   # --- acceleration (0)
    UAMEASUNIT_ACCELERATION_G_FORCE,                  // (0 << 8) + 0,   # 0   g-force
    UAMEASUNIT_ACCELERATION_METER_PER_SECOND_SQUARED, // (0 << 8) + 1,   # 1   meter-per-square-second
    //                                                                   # --- angle (2)
    UAMEASUNIT_ANGLE_ARC_MINUTE,                      // (1 << 8) + 1,   # 2   arc-minute
    UAMEASUNIT_ANGLE_ARC_SECOND,                      // (1 << 8) + 2,   # 3   arc-second
    UAMEASUNIT_ANGLE_DEGREE,                          // (1 << 8) + 0,   # 4   degree
    UAMEASUNIT_ANGLE_RADIAN,                          // (1 << 8) + 3,   # 5   radian
    UAMEASUNIT_ANGLE_REVOLUTION,                      // (1 << 8) + 4,   # 6   revolution
    //                                                                   # --- area (7)
    UAMEASUNIT_AREA_ACRE,                             // (2 << 8) + 4,   # 7   acre
    UAMEASUNIT_AREA_DUNAM,                            // (2 << 8) + 9,   # 8   dunam
    UAMEASUNIT_AREA_HECTARE,                          // (2 << 8) + 5,   # 9   hectare
    UAMEASUNIT_AREA_SQUARE_CENTIMETER,                // (2 << 8) + 6,   # 10  square-centimeter
    UAMEASUNIT_AREA_SQUARE_FOOT,                      // (2 << 8) + 2,   # 11  square-foot
    UAMEASUNIT_AREA_SQUARE_INCH,                      // (2 << 8) + 7,   # 12  square-inch
    UAMEASUNIT_AREA_SQUARE_KILOMETER,                 // (2 << 8) + 1,   # 13  square-kilometer
    UAMEASUNIT_AREA_SQUARE_METER,                     // (2 << 8) + 0,   # 14  square-meter
    UAMEASUNIT_AREA_SQUARE_MILE,                      // (2 << 8) + 3,   # 15  square-mile
    UAMEASUNIT_AREA_SQUARE_YARD,                      // (2 << 8) + 8,   # 16  square-yard
    //                                                                   # --- concentr (17)
    UAMEASUNIT_CONCENTRATION_ITEM,                    // (18 << 8) + 8,  # 17  item
    UAMEASUNIT_CONCENTRATION_KARAT,                   // (18 << 8) + 0,  # 18  karat
    UAMEASUNIT_CONCENTRATION_MILLIGRAM_OFGLUCOSE_PER_DECILITER, // (18 << 8) + 9,  # 19  milligram-ofglucose-per-deciliter
    UAMEASUNIT_CONCENTRATION_MILLIGRAM_PER_DECILITER, // (18 << 8) + 1,  # 20  milligram-per-deciliter
    UAMEASUNIT_CONCENTRATION_MILLIMOLE_PER_LITER,     // (18 << 8) + 2,  # 21  millimole-per-liter
    UAMEASUNIT_CONCENTRATION_MOLE,                    // (18 << 8) + 7,  # 22  mole
    UAMEASUNIT_CONCENTRATION_PERCENT,                 // (18 << 8) + 4,  # 23  percent
    UAMEASUNIT_CONCENTRATION_PERMILLE,                // (18 << 8) + 5,  # 24  permille
    UAMEASUNIT_CONCENTRATION_PART_PER_MILLION,        // (18 << 8) + 3,  # 25  permillion
    UAMEASUNIT_CONCENTRATION_PERMYRIAD,               // (18 << 8) + 6,  # 26  permyriad
    //                                                                   # --- consumption (27)
    UAMEASUNIT_CONSUMPTION_LITER_PER_100_KILOMETERs,  // (13 << 8) + 2,  # 27  liter-per-100-kilometer
    UAMEASUNIT_CONSUMPTION_LITER_PER_KILOMETER,       // (13 << 8) + 0,  # 28  liter-per-kilometer
    UAMEASUNIT_CONSUMPTION_MILE_PER_GALLON,           // (13 << 8) + 1,  # 29  mile-per-gallon
    UAMEASUNIT_CONSUMPTION_MILE_PER_GALLON_IMPERIAL,  // (13 << 8) + 3,  # 30  mile-per-gallon-imperial
    //                                                                   # --- currency (31)
    //                                                                   # --- digital (31)
    UAMEASUNIT_DIGITAL_BIT,                           // (14 << 8) + 0,  # 31  bit
    UAMEASUNIT_DIGITAL_BYTE,                          // (14 << 8) + 1,  # 32  byte
    UAMEASUNIT_DIGITAL_GIGABIT,                       // (14 << 8) + 2,  # 33  gigabit
    UAMEASUNIT_DIGITAL_GIGABYTE,                      // (14 << 8) + 3,  # 34  gigabyte
    UAMEASUNIT_DIGITAL_KILOBIT,                       // (14 << 8) + 4,  # 35  kilobit
    UAMEASUNIT_DIGITAL_KILOBYTE,                      // (14 << 8) + 5,  # 36  kilobyte
    UAMEASUNIT_DIGITAL_MEGABIT,                       // (14 << 8) + 6,  # 37  megabit
    UAMEASUNIT_DIGITAL_MEGABYTE,                      // (14 << 8) + 7,  # 38  megabyte
    UAMEASUNIT_DIGITAL_PETABYTE,                      // (14 << 8) + 10, # 39  petabyte
    UAMEASUNIT_DIGITAL_TERABIT,                       // (14 << 8) + 8,  # 40  terabit
    UAMEASUNIT_DIGITAL_TERABYTE,                      // (14 << 8) + 9,  # 41  terabyte
    //                                                                   # --- duration (42)
    UAMEASUNIT_DURATION_CENTURY,                      // (4 << 8) + 10,  # 42  century
    UAMEASUNIT_DURATION_DAY,                          // (4 << 8) + 3,   # 43  day
    UAMEASUNIT_DURATION_DAY_PERSON,                   // (4 << 8) + 14,  # 44  day-person
    UAMEASUNIT_DURATION_DECADE,                       // (4 << 8) + 15,  # 45  decade
    UAMEASUNIT_DURATION_HOUR,                         // (4 << 8) + 4,   # 46  hour
    UAMEASUNIT_DURATION_MICROSECOND,                  // (4 << 8) + 8,   # 47  microsecond
    UAMEASUNIT_DURATION_MILLISECOND,                  // (4 << 8) + 7,   # 48  millisecond
    UAMEASUNIT_DURATION_MINUTE,                       // (4 << 8) + 5,   # 49  minute
    UAMEASUNIT_DURATION_MONTH,                        // (4 << 8) + 1,   # 50  month
    UAMEASUNIT_DURATION_MONTH_PERSON,                 // (4 << 8) + 12,  # 51  month-person
    UAMEASUNIT_DURATION_NANOSECOND,                   // (4 << 8) + 9,   # 52  nanosecond
    UAMEASUNIT_DURATION_QUARTER,                      // (4 << 8) + 16,  # 53  quarter
    UAMEASUNIT_DURATION_SECOND,                       // (4 << 8) + 6,   # 54  second
    UAMEASUNIT_DURATION_WEEK,                         // (4 << 8) + 2,   # 55  week
    UAMEASUNIT_DURATION_WEEK_PERSON,                  // (4 << 8) + 13,  # 56  week-person
    UAMEASUNIT_DURATION_YEAR,                         // (4 << 8) + 0,   # 57  year
    UAMEASUNIT_DURATION_YEAR_PERSON,                  // (4 << 8) + 11,  # 58  year-person
    //                                                                   # --- electric (58)
    UAMEASUNIT_ELECTRIC_AMPERE,                       // (15 << 8) + 0,  # 59  ampere
    UAMEASUNIT_ELECTRIC_MILLIAMPERE,                  // (15 << 8) + 1,  # 60  milliampere
    UAMEASUNIT_ELECTRIC_OHM,                          // (15 << 8) + 2,  # 61  ohm
    UAMEASUNIT_ELECTRIC_VOLT,                         // (15 << 8) + 3,  # 62  volt
    //                                                                   # --- energy (62)
    UAMEASUNIT_ENERGY_BRITISH_THERMAL_UNIT,           // (12 << 8) + 7,  # 63  british-thermal-unit
    UAMEASUNIT_ENERGY_CALORIE,                        // (12 << 8) + 0,  # 64  calorie
    UAMEASUNIT_ENERGY_ELECTRONVOLT,                   // (12 << 8) + 6,  # 65  electronvolt
    UAMEASUNIT_ENERGY_FOODCALORIE,                    // (12 << 8) + 1,  # 66  foodcalorie
    UAMEASUNIT_ENERGY_JOULE,                          // (12 << 8) + 2,  # 67  joule
    UAMEASUNIT_ENERGY_KILOCALORIE,                    // (12 << 8) + 3,  # 68  kilocalorie
    UAMEASUNIT_ENERGY_KILOJOULE,                      // (12 << 8) + 4,  # 69  kilojoule
    UAMEASUNIT_ENERGY_KILOWATT_HOUR,                  // (12 << 8) + 5,  # 70  kilowatt-hour
    UAMEASUNIT_ENERGY_THERM_US,                       // (12 << 8) + 8,  # 71  therm-us
    //                                                                   # --- force (7``)
    UAMEASUNIT_FORCE_KILOWATT_HOUR_PER_100_KILOMETER, // (19 << 8) + 2,  # 72  kilowatt-hour-per-100-kilometer
    UAMEASUNIT_FORCE_NEWTON,                          // (19 << 8) + 0,  # 73  newton
    UAMEASUNIT_FORCE_POUND_FORCE,                     // (19 << 8) + 1,  # 74  pound-force
    //                                                                   # --- frequency (74)
    UAMEASUNIT_FREQUENCY_GIGAHERTZ,                   // (16 << 8) + 3,  # 75  gigahertz
    UAMEASUNIT_FREQUENCY_HERTZ,                       // (16 << 8) + 0,  # 76  hertz
    UAMEASUNIT_FREQUENCY_KILOHERTZ,                   // (16 << 8) + 1,  # 77  kilohertz
    UAMEASUNIT_FREQUENCY_MEGAHERTZ,                   // (16 << 8) + 2,  # 78  megahertz
    //                                                                   # --- graphics (78)
    UAMEASUNIT_GRAPHICS_DOT,                          // (21 << 8) + 7,  # 79  dot
    UAMEASUNIT_GRAPHICS_DOT_PER_CENTIMETER,           // (21 << 8) + 5,  # 80  dot-per-centimeter
    UAMEASUNIT_GRAPHICS_DOT_PER_INCH,                 // (21 << 8) + 6,  # 81  dot-per-inch
    UAMEASUNIT_GRAPHICS_EM,                           // (21 << 8) + 0,  # 82  em
    UAMEASUNIT_GRAPHICS_MEGAPIXEL,                    // (21 << 8) + 2,  # 83  megapixel
    UAMEASUNIT_GRAPHICS_PIXEL,                        // (21 << 8) + 1,  # 84  pixel
    UAMEASUNIT_GRAPHICS_PIXEL_PER_CENTIMETER,         // (21 << 8) + 3,  # 85  pixel-per-centimeter
    UAMEASUNIT_GRAPHICS_PIXEL_PER_INCH,               // (21 << 8) + 4,  # 86  pixel-per-inch
    //                                                                   # --- length (86)
    UAMEASUNIT_LENGTH_ASTRONOMICAL_UNIT,              // (5 << 8) + 16,  # 87  astronomical-unit
    UAMEASUNIT_LENGTH_CENTIMETER,                     // (5 << 8) + 1,   # 88  centimeter
    UAMEASUNIT_LENGTH_DECIMETER,                      // (5 << 8) + 10,  # 89  decimeter
    UAMEASUNIT_LENGTH_EARTH_RADIUS,                   // (5 << 8) + 21,  # 90  earth-radius
    UAMEASUNIT_LENGTH_FATHOM,                         // (5 << 8) + 14,  # 91  fathom
    UAMEASUNIT_LENGTH_FOOT,                           // (5 << 8) + 5,   # 92  foot
    UAMEASUNIT_LENGTH_FURLONG,                        // (5 << 8) + 15,  # 93  furlong
    UAMEASUNIT_LENGTH_INCH,                           // (5 << 8) + 6,   # 94  inch
    UAMEASUNIT_LENGTH_KILOMETER,                      // (5 << 8) + 2,   # 95  kilometer
    UAMEASUNIT_LENGTH_LIGHT_YEAR,                     // (5 << 8) + 9,   # 96  light-year
    UAMEASUNIT_LENGTH_METER,                          // (5 << 8) + 0,   # 97  meter
    UAMEASUNIT_LENGTH_MICROMETER,                     // (5 << 8) + 11,  # 98  micrometer
    UAMEASUNIT_LENGTH_MILE,                           // (5 << 8) + 7,   # 99  mile
    UAMEASUNIT_LENGTH_MILE_SCANDINAVIAN,              // (5 << 8) + 18,  # 100 mile-scandinavian
    UAMEASUNIT_LENGTH_MILLIMETER,                     // (5 << 8) + 3,   # 101 millimeter
    UAMEASUNIT_LENGTH_NANOMETER,                      // (5 << 8) + 12,  # 102 nanometer
    UAMEASUNIT_LENGTH_NAUTICAL_MILE,                  // (5 << 8) + 13,  # 103 nautical-mile
    UAMEASUNIT_LENGTH_PARSEC,                         // (5 << 8) + 17,  # 104 parsec
    UAMEASUNIT_LENGTH_PICOMETER,                      // (5 << 8) + 4,   # 105 picometer
    UAMEASUNIT_LENGTH_POINT,                          // (5 << 8) + 19,  # 106 point
    UAMEASUNIT_LENGTH_SOLAR_RADIUS,                   // (5 << 8) + 20,  # 107 solar-radius
    UAMEASUNIT_LENGTH_YARD,                           // (5 << 8) + 8,   # 108 yard
    //                                                                   # --- light (108)
    UAMEASUNIT_LIGHT_CANDELA,                         // (17 << 8) + 2   # 109 candela
    UAMEASUNIT_LIGHT_LUMEN,                           // (17 << 8) + 3   # 110 lumen
    UAMEASUNIT_LIGHT_LUX,                             // (17 << 8) + 0,  # 111 lux
    UAMEASUNIT_LIGHT_SOLAR_LUMINOSITY,                // (17 << 8) + 1,  # 112 solar-luminosity
    //                                                                   # --- mass (112)
    UAMEASUNIT_MASS_CARAT,                            // (6 << 8) + 9,   # 113 carat
    UAMEASUNIT_MASS_DALTON,                           // (6 << 8) + 11,  # 114 dalton
    UAMEASUNIT_MASS_EARTH_MASS,                       // (6 << 8) + 12,  # 115 earth-mass
    UAMEASUNIT_MASS_GRAIN,                            // (6 << 8) + 14,  # 116 grain
    UAMEASUNIT_MASS_GRAM,                             // (6 << 8) + 0,   # 117 gram
    UAMEASUNIT_MASS_KILOGRAM,                         // (6 << 8) + 1,   # 118 kilogram
    UAMEASUNIT_MASS_MICROGRAM,                        // (6 << 8) + 5,   # 119 microgram
    UAMEASUNIT_MASS_MILLIGRAM,                        // (6 << 8) + 6,   # 120 milligram
    UAMEASUNIT_MASS_OUNCE,                            // (6 << 8) + 2,   # 121 ounce
    UAMEASUNIT_MASS_OUNCE_TROY,                       // (6 << 8) + 10,  # 122 ounce-troy
    UAMEASUNIT_MASS_POUND,                            // (6 << 8) + 3,   # 123 pound
    UAMEASUNIT_MASS_SOLAR_MASS,                       // (6 << 8) + 13,  # 124 solar-mass
    UAMEASUNIT_MASS_STONE,                            // (6 << 8) + 4,   # 125 stone
    UAMEASUNIT_MASS_TON,                              // (6 << 8) + 8,   # 126 ton
    UAMEASUNIT_MASS_TONNE,                            // (6 << 8) + 7,   # 127 tonne
    //                                                                   # --- none (127)
    UAMEASUNIT_CONCENTRATION_PERCENT,                 // BOGUS           # 128 base
    //                                                                   # --- power (128)
    UAMEASUNIT_POWER_GIGAWATT,                        // (7 << 8) + 5,   # 129 gigawatt
    UAMEASUNIT_POWER_HORSEPOWER,                      // (7 << 8) + 2,   # 130 horsepower
    UAMEASUNIT_POWER_KILOWATT,                        // (7 << 8) + 1,   # 131 kilowatt
    UAMEASUNIT_POWER_MEGAWATT,                        // (7 << 8) + 4,   # 132 megawatt
    UAMEASUNIT_POWER_MILLIWATT,                       // (7 << 8) + 3,   # 133 milliwatt
    UAMEASUNIT_POWER_WATT,                            // (7 << 8) + 0,   # 134 watt
    //                                                                   # --- pressure (134)
    UAMEASUNIT_PRESSURE_ATMOSPHERE,                   // (8 << 8) + 5,   # 135 atmosphere
    UAMEASUNIT_PRESSURE_BAR,                          // (8 << 8) + 9,   # 136 bar
    UAMEASUNIT_PRESSURE_HECTOPASCAL,                  // (8 << 8) + 0,   # 137 hectopascal
    UAMEASUNIT_PRESSURE_INCH_HG,                      // (8 << 8) + 1,   # 138 inch-ofhg
    UAMEASUNIT_PRESSURE_KILOPASCAL,                   // (8 << 8) + 6,   # 139 kilopascal
    UAMEASUNIT_PRESSURE_MEGAPASCAL,                   // (8 << 8) + 7,   # 140 megapascal
    UAMEASUNIT_PRESSURE_MILLIBAR,                     // (8 << 8) + 2,   # 141 millibar
    UAMEASUNIT_PRESSURE_MILLIMETER_OF_MERCURY,        // (8 << 8) + 3,   # 142 millimeter-ofhg
    UAMEASUNIT_PRESSURE_PASCAL,                       // (8 << 8) + 8,   # 143 pascal
    UAMEASUNIT_PRESSURE_POUND_PER_SQUARE_INCH,        // (8 << 8) + 4,   # 144 pound-force-per-square-inch
    //                                                                   # --- speed (144)
    UAMEASUNIT_SPEED_KILOMETER_PER_HOUR,              // (9 << 8) + 1,   # 145 kilometer-per-hour
    UAMEASUNIT_SPEED_KNOT,                            // (9 << 8) + 3,   # 146 knot
    UAMEASUNIT_SPEED_METER_PER_SECOND,                // (9 << 8) + 0,   # 147 meter-per-second
    UAMEASUNIT_SPEED_MILE_PER_HOUR,                   // (9 << 8) + 2,   # 148 mile-per-hour
    //                                                                   # --- temperature (148)
    UAMEASUNIT_TEMPERATURE_CELSIUS,                   // (10 << 8) + 0,  # 149 celsius
    UAMEASUNIT_TEMPERATURE_FAHRENHEIT,                // (10 << 8) + 1,  # 150 fahrenheit
    UAMEASUNIT_TEMPERATURE_GENERIC,                   // (10 << 8) + 3,  # 151 generic
    UAMEASUNIT_TEMPERATURE_KELVIN,                    // (10 << 8) + 2,  # 152 kelvin
    //                                                                   # --- torque (152)
    UAMEASUNIT_TORQUE_NEWTON_METER,                   // (20 << 8) + 0,  # 153 newton-meter
    UAMEASUNIT_TORQUE_POUND_FOOT,                     // (20 << 8) + 1,  # 154 pound-force-foot
    //                                                                   # --- volume (154)
    UAMEASUNIT_VOLUME_ACRE_FOOT,                      // (11 << 8) + 13, # 155 acre-foot
    UAMEASUNIT_VOLUME_BARREL,                         // (11 << 8) + 26, # 156 barrel
    UAMEASUNIT_VOLUME_BUSHEL,                         // (11 << 8) + 14, # 157 bushel
    UAMEASUNIT_VOLUME_CENTILITER,                     // (11 << 8) + 4,  # 158 centiliter
    UAMEASUNIT_VOLUME_CUBIC_CENTIMETER,               // (11 << 8) + 8,  # 159 cubic-centimeter
    UAMEASUNIT_VOLUME_CUBIC_FOOT,                     // (11 << 8) + 11, # 160 cubic-foot
    UAMEASUNIT_VOLUME_CUBIC_INCH,                     // (11 << 8) + 10, # 161 cubic-inch
    UAMEASUNIT_VOLUME_CUBIC_KILOMETER,                // (11 << 8) + 1,  # 162 cubic-kilometer
    UAMEASUNIT_VOLUME_CUBIC_METER,                    // (11 << 8) + 9,  # 163 cubic-meter
    UAMEASUNIT_VOLUME_CUBIC_MILE,                     // (11 << 8) + 2,  # 164 cubic-mile
    UAMEASUNIT_VOLUME_CUBIC_YARD,                     // (11 << 8) + 12, # 165 cubic-yard
    UAMEASUNIT_VOLUME_CUP,                            // (11 << 8) + 18, # 166 cup
    UAMEASUNIT_VOLUME_CUP_METRIC,                     // (11 << 8) + 22, # 167 cup-metric
    UAMEASUNIT_VOLUME_DECILITER,                      // (11 << 8) + 5,  # 168 deciliter
    UAMEASUNIT_VOLUME_DESSERT_SPOON,                  // (11 << 8) + 27, # 169 dessert-spoon
    UAMEASUNIT_VOLUME_DESSERT_SPOON_IMPERIAL,         // (11 << 8) + 28, # 170 dessert-spoon-imperial
    UAMEASUNIT_VOLUME_DRAM,                           // (11 << 8) + 29, # 171 dram
    UAMEASUNIT_VOLUME_DROP,                           // (11 << 8) + 30, # 172 drop
    UAMEASUNIT_VOLUME_FLUID_OUNCE,                    // (11 << 8) + 17, # 173 fluid-ounce
    UAMEASUNIT_VOLUME_FLUID_OUNCE_IMPERIAL,           // (11 << 8) + 25, # 174 fluid-ounce-imperial
    UAMEASUNIT_VOLUME_GALLON,                         // (11 << 8) + 21, # 175 gallon
    UAMEASUNIT_VOLUME_GALLON_IMPERIAL,                // (11 << 8) + 24, # 176 gallon-imperial
    UAMEASUNIT_VOLUME_HECTOLITER,                     // (11 << 8) + 6,  # 177 hectoliter
    UAMEASUNIT_VOLUME_JIGGER,                         // (11 << 8) + 31, # 178 jigger
    UAMEASUNIT_VOLUME_LITER,                          // (11 << 8) + 0,  # 179 liter
    UAMEASUNIT_VOLUME_MEGALITER,                      // (11 << 8) + 7,  # 180 megaliter
    UAMEASUNIT_VOLUME_MILLILITER,                     // (11 << 8) + 3,  # 181 milliliter
    UAMEASUNIT_VOLUME_PINCH,                          // (11 << 8) + 32, # 182 pinch
    UAMEASUNIT_VOLUME_PINT,                           // (11 << 8) + 19, # 183 pint
    UAMEASUNIT_VOLUME_PINT_METRIC,                    // (11 << 8) + 23, # 184 pint-metric
    UAMEASUNIT_VOLUME_QUART,                          // (11 << 8) + 20, # 185 quart
    UAMEASUNIT_VOLUME_QUART_IMPERIAL,                 // (11 << 8) + 33, # 186 quart-imperial
    UAMEASUNIT_VOLUME_TABLESPOON,                     // (11 << 8) + 16, # 187 tablespoon
    UAMEASUNIT_VOLUME_TEASPOON,                       // (11 << 8) + 15, # 188 teaspoon
};

UAMeasureUnit MeasureUnit::getUAMeasureUnit() const {
    int32_t index = getOffset();
    if (index < 0) {
        // this isn't really kosher, but I'm not sure what else to do
        return (UAMeasureUnit)(-1);
    }
    
    if (fTypeId > kCurrencyOffset) {
        index -= gOffsets[kCurrencyOffset + 1] - gOffsets[kCurrencyOffset];
    }
    
    if (index < UPRV_LENGTHOF(indexToUAMeasUnit)) {
        return indexToUAMeasUnit[index];
    } else {
        // this isn't really kosher, but I'm not sure what else to do
        return (UAMeasureUnit)(-1);
    }
}

int32_t MeasureUnit::getUAMeasureUnits(UAMeasureUnit* units, int32_t unitsCapacity, UErrorCode& status) const {
    if (getComplexity(status) == UMEASURE_UNIT_MIXED) {
        std::pair<LocalArray<MeasureUnit>, int32_t> splitResult = splitToSingleUnits(status);
        for (int32_t i = 0; i < unitsCapacity && i < splitResult.second; i++) {
            units[i] = splitResult.first[i].getUAMeasureUnit();
        }
        return splitResult.second;
    } else {
        // we want to decompose mixed units, but keep compound units together
        if (unitsCapacity >= 1) {
            units[0] = this->getUAMeasureUnit();
        }
        return 1;
    }
}
#endif  // APPLE_ICU_CHANGES

U_NAMESPACE_END

#endif /* !UNCONFIG_NO_FORMATTING */
