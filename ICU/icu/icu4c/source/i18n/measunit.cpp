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
    8,
    21,
    34,
    38,
    340,
    351,
    370,
    378,
    392,
    396,
    400,
    408,
    439,
    443,
    445,
    462,
    463,
    469,
    481,
    487,
    492,
    494,
    538
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
    "magnetic",
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
    "steradian",
    "acre",
    "bu-jp",
    "cho",
    "dunam",
    "hectare",
    "se-jp",
    "square-centimeter",
    "square-foot",
    "square-inch",
    "square-kilometer",
    "square-meter",
    "square-mile",
    "square-yard",
    "item",
    "karat",
    "katal",
    "milligram-ofglucose-per-deciliter",
    "millimole-per-liter",
    "mole",
    "ofglucose",
    "part",
    "part-per-1e6",
    "part-per-1e9",
    "percent",
    "permille",
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
    "ZWG",
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
    "fortnight",
    "hour",
    "microsecond",
    "millisecond",
    "minute",
    "month",
    "month-person",
    "nanosecond",
    "night",
    "quarter",
    "second",
    "week",
    "week-person",
    "year",
    "year-person",
    "ampere",
    "coulomb",
    "farad",
    "henry",
    "milliampere",
    "ohm",
    "siemens",
    "volt",
    "becquerel",
    "british-thermal-unit",
    "british-thermal-unit-it",
    "calorie",
    "calorie-it",
    "electronvolt",
    "foodcalorie",
    "gray",
    "joule",
    "kilocalorie",
    "kilojoule",
    "kilowatt-hour",
    "sievert",
    "therm-us",
    "kilogram-force",
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
    "chain",
    "decimeter",
    "earth-radius",
    "fathom",
    "foot",
    "furlong",
    "inch",
    "jo-jp",
    "ken",
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
    "ri-jp",
    "rin",
    "rod",
    "shaku-cloth",
    "shaku-length",
    "solar-radius",
    "sun",
    "yard",
    "candela",
    "lumen",
    "lux",
    "solar-luminosity",
    "tesla",
    "weber",
    "carat",
    "dalton",
    "earth-mass",
    "fun",
    "grain",
    "gram",
    "kilogram",
    "microgram",
    "milligram",
    "ounce",
    "ounce-troy",
    "pound",
    "slug",
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
    "gasoline-energy-density",
    "hectopascal",
    "inch-ofhg",
    "kilopascal",
    "megapascal",
    "millibar",
    "millimeter-ofhg",
    "ofhg",
    "pascal",
    "pound-force-per-square-inch",
    "beaufort",
    "kilometer-per-hour",
    "knot",
    "light-speed",
    "meter-per-second",
    "mile-per-hour",
    "celsius",
    "fahrenheit",
    "generic",
    "kelvin",
    "rankine",
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
    "cup-imperial",
    "cup-jp",
    "cup-metric",
    "deciliter",
    "dessert-spoon",
    "dessert-spoon-imperial",
    "dram",
    "drop",
    "fluid-ounce",
    "fluid-ounce-imperial",
    "fluid-ounce-metric",
    "gallon",
    "gallon-imperial",
    "hectoliter",
    "jigger",
    "koku",
    "kosaji",
    "liter",
    "megaliter",
    "milliliter",
    "osaji",
    "pinch",
    "pint",
    "pint-imperial",
    "pint-metric",
    "quart",
    "quart-imperial",
    "sai",
    "shaku",
    "tablespoon",
    "teaspoon",
    "to-jp"
};

// Shortcuts to the base unit in order to make the default constructor fast
static const int32_t kBaseTypeIdx = 17;
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

MeasureUnit *MeasureUnit::createSteradian(UErrorCode &status) {
    return MeasureUnit::create(1, 5, status);
}

MeasureUnit MeasureUnit::getSteradian() {
    return MeasureUnit(1, 5);
}

MeasureUnit *MeasureUnit::createAcre(UErrorCode &status) {
    return MeasureUnit::create(2, 0, status);
}

MeasureUnit MeasureUnit::getAcre() {
    return MeasureUnit(2, 0);
}

MeasureUnit *MeasureUnit::createBuJp(UErrorCode &status) {
    return MeasureUnit::create(2, 1, status);
}

MeasureUnit MeasureUnit::getBuJp() {
    return MeasureUnit(2, 1);
}

MeasureUnit *MeasureUnit::createCho(UErrorCode &status) {
    return MeasureUnit::create(2, 2, status);
}

MeasureUnit MeasureUnit::getCho() {
    return MeasureUnit(2, 2);
}

MeasureUnit *MeasureUnit::createDunam(UErrorCode &status) {
    return MeasureUnit::create(2, 3, status);
}

MeasureUnit MeasureUnit::getDunam() {
    return MeasureUnit(2, 3);
}

MeasureUnit *MeasureUnit::createHectare(UErrorCode &status) {
    return MeasureUnit::create(2, 4, status);
}

MeasureUnit MeasureUnit::getHectare() {
    return MeasureUnit(2, 4);
}

MeasureUnit *MeasureUnit::createSeJp(UErrorCode &status) {
    return MeasureUnit::create(2, 5, status);
}

MeasureUnit MeasureUnit::getSeJp() {
    return MeasureUnit(2, 5);
}

MeasureUnit *MeasureUnit::createSquareCentimeter(UErrorCode &status) {
    return MeasureUnit::create(2, 6, status);
}

MeasureUnit MeasureUnit::getSquareCentimeter() {
    return MeasureUnit(2, 6);
}

MeasureUnit *MeasureUnit::createSquareFoot(UErrorCode &status) {
    return MeasureUnit::create(2, 7, status);
}

MeasureUnit MeasureUnit::getSquareFoot() {
    return MeasureUnit(2, 7);
}

MeasureUnit *MeasureUnit::createSquareInch(UErrorCode &status) {
    return MeasureUnit::create(2, 8, status);
}

MeasureUnit MeasureUnit::getSquareInch() {
    return MeasureUnit(2, 8);
}

MeasureUnit *MeasureUnit::createSquareKilometer(UErrorCode &status) {
    return MeasureUnit::create(2, 9, status);
}

MeasureUnit MeasureUnit::getSquareKilometer() {
    return MeasureUnit(2, 9);
}

MeasureUnit *MeasureUnit::createSquareMeter(UErrorCode &status) {
    return MeasureUnit::create(2, 10, status);
}

MeasureUnit MeasureUnit::getSquareMeter() {
    return MeasureUnit(2, 10);
}

MeasureUnit *MeasureUnit::createSquareMile(UErrorCode &status) {
    return MeasureUnit::create(2, 11, status);
}

MeasureUnit MeasureUnit::getSquareMile() {
    return MeasureUnit(2, 11);
}

MeasureUnit *MeasureUnit::createSquareYard(UErrorCode &status) {
    return MeasureUnit::create(2, 12, status);
}

MeasureUnit MeasureUnit::getSquareYard() {
    return MeasureUnit(2, 12);
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

MeasureUnit *MeasureUnit::createKatal(UErrorCode &status) {
    return MeasureUnit::create(3, 2, status);
}

MeasureUnit MeasureUnit::getKatal() {
    return MeasureUnit(3, 2);
}

MeasureUnit *MeasureUnit::createMilligramOfglucosePerDeciliter(UErrorCode &status) {
    return MeasureUnit::create(3, 3, status);
}

MeasureUnit MeasureUnit::getMilligramOfglucosePerDeciliter() {
    return MeasureUnit(3, 3);
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

MeasureUnit *MeasureUnit::createOfglucose(UErrorCode &status) {
    return MeasureUnit::create(3, 6, status);
}

MeasureUnit MeasureUnit::getOfglucose() {
    return MeasureUnit(3, 6);
}

MeasureUnit *MeasureUnit::createPart(UErrorCode &status) {
    return MeasureUnit::create(3, 7, status);
}

MeasureUnit MeasureUnit::getPart() {
    return MeasureUnit(3, 7);
}

MeasureUnit *MeasureUnit::createPartPer1E6(UErrorCode &status) {
    return MeasureUnit::create(3, 8, status);
}

MeasureUnit MeasureUnit::getPartPer1E6() {
    return MeasureUnit(3, 8);
}

MeasureUnit *MeasureUnit::createPartPerMillion(UErrorCode &status) {
    return MeasureUnit::create(3, 8, status);
}

MeasureUnit MeasureUnit::getPartPerMillion() {
    return MeasureUnit(3, 8);
}

MeasureUnit *MeasureUnit::createPartPer1E9(UErrorCode &status) {
    return MeasureUnit::create(3, 9, status);
}

MeasureUnit MeasureUnit::getPartPer1E9() {
    return MeasureUnit(3, 9);
}

MeasureUnit *MeasureUnit::createPercent(UErrorCode &status) {
    return MeasureUnit::create(3, 10, status);
}

MeasureUnit MeasureUnit::getPercent() {
    return MeasureUnit(3, 10);
}

MeasureUnit *MeasureUnit::createPermille(UErrorCode &status) {
    return MeasureUnit::create(3, 11, status);
}

MeasureUnit MeasureUnit::getPermille() {
    return MeasureUnit(3, 11);
}

MeasureUnit *MeasureUnit::createPermyriad(UErrorCode &status) {
    return MeasureUnit::create(3, 12, status);
}

MeasureUnit MeasureUnit::getPermyriad() {
    return MeasureUnit(3, 12);
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

MeasureUnit *MeasureUnit::createFortnight(UErrorCode &status) {
    return MeasureUnit::create(7, 4, status);
}

MeasureUnit MeasureUnit::getFortnight() {
    return MeasureUnit(7, 4);
}

MeasureUnit *MeasureUnit::createHour(UErrorCode &status) {
    return MeasureUnit::create(7, 5, status);
}

MeasureUnit MeasureUnit::getHour() {
    return MeasureUnit(7, 5);
}

MeasureUnit *MeasureUnit::createMicrosecond(UErrorCode &status) {
    return MeasureUnit::create(7, 6, status);
}

MeasureUnit MeasureUnit::getMicrosecond() {
    return MeasureUnit(7, 6);
}

MeasureUnit *MeasureUnit::createMillisecond(UErrorCode &status) {
    return MeasureUnit::create(7, 7, status);
}

MeasureUnit MeasureUnit::getMillisecond() {
    return MeasureUnit(7, 7);
}

MeasureUnit *MeasureUnit::createMinute(UErrorCode &status) {
    return MeasureUnit::create(7, 8, status);
}

MeasureUnit MeasureUnit::getMinute() {
    return MeasureUnit(7, 8);
}

MeasureUnit *MeasureUnit::createMonth(UErrorCode &status) {
    return MeasureUnit::create(7, 9, status);
}

MeasureUnit MeasureUnit::getMonth() {
    return MeasureUnit(7, 9);
}

MeasureUnit *MeasureUnit::createMonthPerson(UErrorCode &status) {
    return MeasureUnit::create(7, 10, status);
}

MeasureUnit MeasureUnit::getMonthPerson() {
    return MeasureUnit(7, 10);
}

MeasureUnit *MeasureUnit::createNanosecond(UErrorCode &status) {
    return MeasureUnit::create(7, 11, status);
}

MeasureUnit MeasureUnit::getNanosecond() {
    return MeasureUnit(7, 11);
}

MeasureUnit *MeasureUnit::createNight(UErrorCode &status) {
    return MeasureUnit::create(7, 12, status);
}

MeasureUnit MeasureUnit::getNight() {
    return MeasureUnit(7, 12);
}

MeasureUnit *MeasureUnit::createQuarter(UErrorCode &status) {
    return MeasureUnit::create(7, 13, status);
}

MeasureUnit MeasureUnit::getQuarter() {
    return MeasureUnit(7, 13);
}

MeasureUnit *MeasureUnit::createSecond(UErrorCode &status) {
    return MeasureUnit::create(7, 14, status);
}

MeasureUnit MeasureUnit::getSecond() {
    return MeasureUnit(7, 14);
}

MeasureUnit *MeasureUnit::createWeek(UErrorCode &status) {
    return MeasureUnit::create(7, 15, status);
}

MeasureUnit MeasureUnit::getWeek() {
    return MeasureUnit(7, 15);
}

MeasureUnit *MeasureUnit::createWeekPerson(UErrorCode &status) {
    return MeasureUnit::create(7, 16, status);
}

MeasureUnit MeasureUnit::getWeekPerson() {
    return MeasureUnit(7, 16);
}

MeasureUnit *MeasureUnit::createYear(UErrorCode &status) {
    return MeasureUnit::create(7, 17, status);
}

MeasureUnit MeasureUnit::getYear() {
    return MeasureUnit(7, 17);
}

MeasureUnit *MeasureUnit::createYearPerson(UErrorCode &status) {
    return MeasureUnit::create(7, 18, status);
}

MeasureUnit MeasureUnit::getYearPerson() {
    return MeasureUnit(7, 18);
}

MeasureUnit *MeasureUnit::createAmpere(UErrorCode &status) {
    return MeasureUnit::create(8, 0, status);
}

MeasureUnit MeasureUnit::getAmpere() {
    return MeasureUnit(8, 0);
}

MeasureUnit *MeasureUnit::createCoulomb(UErrorCode &status) {
    return MeasureUnit::create(8, 1, status);
}

MeasureUnit MeasureUnit::getCoulomb() {
    return MeasureUnit(8, 1);
}

MeasureUnit *MeasureUnit::createFarad(UErrorCode &status) {
    return MeasureUnit::create(8, 2, status);
}

MeasureUnit MeasureUnit::getFarad() {
    return MeasureUnit(8, 2);
}

MeasureUnit *MeasureUnit::createHenry(UErrorCode &status) {
    return MeasureUnit::create(8, 3, status);
}

MeasureUnit MeasureUnit::getHenry() {
    return MeasureUnit(8, 3);
}

MeasureUnit *MeasureUnit::createMilliampere(UErrorCode &status) {
    return MeasureUnit::create(8, 4, status);
}

MeasureUnit MeasureUnit::getMilliampere() {
    return MeasureUnit(8, 4);
}

MeasureUnit *MeasureUnit::createOhm(UErrorCode &status) {
    return MeasureUnit::create(8, 5, status);
}

MeasureUnit MeasureUnit::getOhm() {
    return MeasureUnit(8, 5);
}

MeasureUnit *MeasureUnit::createSiemens(UErrorCode &status) {
    return MeasureUnit::create(8, 6, status);
}

MeasureUnit MeasureUnit::getSiemens() {
    return MeasureUnit(8, 6);
}

MeasureUnit *MeasureUnit::createVolt(UErrorCode &status) {
    return MeasureUnit::create(8, 7, status);
}

MeasureUnit MeasureUnit::getVolt() {
    return MeasureUnit(8, 7);
}

MeasureUnit *MeasureUnit::createBecquerel(UErrorCode &status) {
    return MeasureUnit::create(9, 0, status);
}

MeasureUnit MeasureUnit::getBecquerel() {
    return MeasureUnit(9, 0);
}

MeasureUnit *MeasureUnit::createBritishThermalUnit(UErrorCode &status) {
    return MeasureUnit::create(9, 1, status);
}

MeasureUnit MeasureUnit::getBritishThermalUnit() {
    return MeasureUnit(9, 1);
}

MeasureUnit *MeasureUnit::createBritishThermalUnitIt(UErrorCode &status) {
    return MeasureUnit::create(9, 2, status);
}

MeasureUnit MeasureUnit::getBritishThermalUnitIt() {
    return MeasureUnit(9, 2);
}

MeasureUnit *MeasureUnit::createCalorie(UErrorCode &status) {
    return MeasureUnit::create(9, 3, status);
}

MeasureUnit MeasureUnit::getCalorie() {
    return MeasureUnit(9, 3);
}

MeasureUnit *MeasureUnit::createCalorieIt(UErrorCode &status) {
    return MeasureUnit::create(9, 4, status);
}

MeasureUnit MeasureUnit::getCalorieIt() {
    return MeasureUnit(9, 4);
}

MeasureUnit *MeasureUnit::createElectronvolt(UErrorCode &status) {
    return MeasureUnit::create(9, 5, status);
}

MeasureUnit MeasureUnit::getElectronvolt() {
    return MeasureUnit(9, 5);
}

MeasureUnit *MeasureUnit::createFoodcalorie(UErrorCode &status) {
    return MeasureUnit::create(9, 6, status);
}

MeasureUnit MeasureUnit::getFoodcalorie() {
    return MeasureUnit(9, 6);
}

MeasureUnit *MeasureUnit::createGray(UErrorCode &status) {
    return MeasureUnit::create(9, 7, status);
}

MeasureUnit MeasureUnit::getGray() {
    return MeasureUnit(9, 7);
}

MeasureUnit *MeasureUnit::createJoule(UErrorCode &status) {
    return MeasureUnit::create(9, 8, status);
}

MeasureUnit MeasureUnit::getJoule() {
    return MeasureUnit(9, 8);
}

MeasureUnit *MeasureUnit::createKilocalorie(UErrorCode &status) {
    return MeasureUnit::create(9, 9, status);
}

MeasureUnit MeasureUnit::getKilocalorie() {
    return MeasureUnit(9, 9);
}

MeasureUnit *MeasureUnit::createKilojoule(UErrorCode &status) {
    return MeasureUnit::create(9, 10, status);
}

MeasureUnit MeasureUnit::getKilojoule() {
    return MeasureUnit(9, 10);
}

MeasureUnit *MeasureUnit::createKilowattHour(UErrorCode &status) {
    return MeasureUnit::create(9, 11, status);
}

MeasureUnit MeasureUnit::getKilowattHour() {
    return MeasureUnit(9, 11);
}

MeasureUnit *MeasureUnit::createSievert(UErrorCode &status) {
    return MeasureUnit::create(9, 12, status);
}

MeasureUnit MeasureUnit::getSievert() {
    return MeasureUnit(9, 12);
}

MeasureUnit *MeasureUnit::createThermUs(UErrorCode &status) {
    return MeasureUnit::create(9, 13, status);
}

MeasureUnit MeasureUnit::getThermUs() {
    return MeasureUnit(9, 13);
}

MeasureUnit *MeasureUnit::createKilogramForce(UErrorCode &status) {
    return MeasureUnit::create(10, 0, status);
}

MeasureUnit MeasureUnit::getKilogramForce() {
    return MeasureUnit(10, 0);
}

MeasureUnit *MeasureUnit::createKilowattHourPer100Kilometer(UErrorCode &status) {
    return MeasureUnit::create(10, 1, status);
}

MeasureUnit MeasureUnit::getKilowattHourPer100Kilometer() {
    return MeasureUnit(10, 1);
}

MeasureUnit *MeasureUnit::createNewton(UErrorCode &status) {
    return MeasureUnit::create(10, 2, status);
}

MeasureUnit MeasureUnit::getNewton() {
    return MeasureUnit(10, 2);
}

MeasureUnit *MeasureUnit::createPoundForce(UErrorCode &status) {
    return MeasureUnit::create(10, 3, status);
}

MeasureUnit MeasureUnit::getPoundForce() {
    return MeasureUnit(10, 3);
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

MeasureUnit *MeasureUnit::createChain(UErrorCode &status) {
    return MeasureUnit::create(13, 2, status);
}

MeasureUnit MeasureUnit::getChain() {
    return MeasureUnit(13, 2);
}

MeasureUnit *MeasureUnit::createDecimeter(UErrorCode &status) {
    return MeasureUnit::create(13, 3, status);
}

MeasureUnit MeasureUnit::getDecimeter() {
    return MeasureUnit(13, 3);
}

MeasureUnit *MeasureUnit::createEarthRadius(UErrorCode &status) {
    return MeasureUnit::create(13, 4, status);
}

MeasureUnit MeasureUnit::getEarthRadius() {
    return MeasureUnit(13, 4);
}

MeasureUnit *MeasureUnit::createFathom(UErrorCode &status) {
    return MeasureUnit::create(13, 5, status);
}

MeasureUnit MeasureUnit::getFathom() {
    return MeasureUnit(13, 5);
}

MeasureUnit *MeasureUnit::createFoot(UErrorCode &status) {
    return MeasureUnit::create(13, 6, status);
}

MeasureUnit MeasureUnit::getFoot() {
    return MeasureUnit(13, 6);
}

MeasureUnit *MeasureUnit::createFurlong(UErrorCode &status) {
    return MeasureUnit::create(13, 7, status);
}

MeasureUnit MeasureUnit::getFurlong() {
    return MeasureUnit(13, 7);
}

MeasureUnit *MeasureUnit::createInch(UErrorCode &status) {
    return MeasureUnit::create(13, 8, status);
}

MeasureUnit MeasureUnit::getInch() {
    return MeasureUnit(13, 8);
}

MeasureUnit *MeasureUnit::createJoJp(UErrorCode &status) {
    return MeasureUnit::create(13, 9, status);
}

MeasureUnit MeasureUnit::getJoJp() {
    return MeasureUnit(13, 9);
}

MeasureUnit *MeasureUnit::createKen(UErrorCode &status) {
    return MeasureUnit::create(13, 10, status);
}

MeasureUnit MeasureUnit::getKen() {
    return MeasureUnit(13, 10);
}

MeasureUnit *MeasureUnit::createKilometer(UErrorCode &status) {
    return MeasureUnit::create(13, 11, status);
}

MeasureUnit MeasureUnit::getKilometer() {
    return MeasureUnit(13, 11);
}

MeasureUnit *MeasureUnit::createLightYear(UErrorCode &status) {
    return MeasureUnit::create(13, 12, status);
}

MeasureUnit MeasureUnit::getLightYear() {
    return MeasureUnit(13, 12);
}

MeasureUnit *MeasureUnit::createMeter(UErrorCode &status) {
    return MeasureUnit::create(13, 13, status);
}

MeasureUnit MeasureUnit::getMeter() {
    return MeasureUnit(13, 13);
}

MeasureUnit *MeasureUnit::createMicrometer(UErrorCode &status) {
    return MeasureUnit::create(13, 14, status);
}

MeasureUnit MeasureUnit::getMicrometer() {
    return MeasureUnit(13, 14);
}

MeasureUnit *MeasureUnit::createMile(UErrorCode &status) {
    return MeasureUnit::create(13, 15, status);
}

MeasureUnit MeasureUnit::getMile() {
    return MeasureUnit(13, 15);
}

MeasureUnit *MeasureUnit::createMileScandinavian(UErrorCode &status) {
    return MeasureUnit::create(13, 16, status);
}

MeasureUnit MeasureUnit::getMileScandinavian() {
    return MeasureUnit(13, 16);
}

MeasureUnit *MeasureUnit::createMillimeter(UErrorCode &status) {
    return MeasureUnit::create(13, 17, status);
}

MeasureUnit MeasureUnit::getMillimeter() {
    return MeasureUnit(13, 17);
}

MeasureUnit *MeasureUnit::createNanometer(UErrorCode &status) {
    return MeasureUnit::create(13, 18, status);
}

MeasureUnit MeasureUnit::getNanometer() {
    return MeasureUnit(13, 18);
}

MeasureUnit *MeasureUnit::createNauticalMile(UErrorCode &status) {
    return MeasureUnit::create(13, 19, status);
}

MeasureUnit MeasureUnit::getNauticalMile() {
    return MeasureUnit(13, 19);
}

MeasureUnit *MeasureUnit::createParsec(UErrorCode &status) {
    return MeasureUnit::create(13, 20, status);
}

MeasureUnit MeasureUnit::getParsec() {
    return MeasureUnit(13, 20);
}

MeasureUnit *MeasureUnit::createPicometer(UErrorCode &status) {
    return MeasureUnit::create(13, 21, status);
}

MeasureUnit MeasureUnit::getPicometer() {
    return MeasureUnit(13, 21);
}

MeasureUnit *MeasureUnit::createPoint(UErrorCode &status) {
    return MeasureUnit::create(13, 22, status);
}

MeasureUnit MeasureUnit::getPoint() {
    return MeasureUnit(13, 22);
}

MeasureUnit *MeasureUnit::createRiJp(UErrorCode &status) {
    return MeasureUnit::create(13, 23, status);
}

MeasureUnit MeasureUnit::getRiJp() {
    return MeasureUnit(13, 23);
}

MeasureUnit *MeasureUnit::createRin(UErrorCode &status) {
    return MeasureUnit::create(13, 24, status);
}

MeasureUnit MeasureUnit::getRin() {
    return MeasureUnit(13, 24);
}

MeasureUnit *MeasureUnit::createRod(UErrorCode &status) {
    return MeasureUnit::create(13, 25, status);
}

MeasureUnit MeasureUnit::getRod() {
    return MeasureUnit(13, 25);
}

MeasureUnit *MeasureUnit::createShakuCloth(UErrorCode &status) {
    return MeasureUnit::create(13, 26, status);
}

MeasureUnit MeasureUnit::getShakuCloth() {
    return MeasureUnit(13, 26);
}

MeasureUnit *MeasureUnit::createShakuLength(UErrorCode &status) {
    return MeasureUnit::create(13, 27, status);
}

MeasureUnit MeasureUnit::getShakuLength() {
    return MeasureUnit(13, 27);
}

MeasureUnit *MeasureUnit::createSolarRadius(UErrorCode &status) {
    return MeasureUnit::create(13, 28, status);
}

MeasureUnit MeasureUnit::getSolarRadius() {
    return MeasureUnit(13, 28);
}

MeasureUnit *MeasureUnit::createSun(UErrorCode &status) {
    return MeasureUnit::create(13, 29, status);
}

MeasureUnit MeasureUnit::getSun() {
    return MeasureUnit(13, 29);
}

MeasureUnit *MeasureUnit::createYard(UErrorCode &status) {
    return MeasureUnit::create(13, 30, status);
}

MeasureUnit MeasureUnit::getYard() {
    return MeasureUnit(13, 30);
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

MeasureUnit *MeasureUnit::createTesla(UErrorCode &status) {
    return MeasureUnit::create(15, 0, status);
}

MeasureUnit MeasureUnit::getTesla() {
    return MeasureUnit(15, 0);
}

MeasureUnit *MeasureUnit::createWeber(UErrorCode &status) {
    return MeasureUnit::create(15, 1, status);
}

MeasureUnit MeasureUnit::getWeber() {
    return MeasureUnit(15, 1);
}

MeasureUnit *MeasureUnit::createCarat(UErrorCode &status) {
    return MeasureUnit::create(16, 0, status);
}

MeasureUnit MeasureUnit::getCarat() {
    return MeasureUnit(16, 0);
}

MeasureUnit *MeasureUnit::createDalton(UErrorCode &status) {
    return MeasureUnit::create(16, 1, status);
}

MeasureUnit MeasureUnit::getDalton() {
    return MeasureUnit(16, 1);
}

MeasureUnit *MeasureUnit::createEarthMass(UErrorCode &status) {
    return MeasureUnit::create(16, 2, status);
}

MeasureUnit MeasureUnit::getEarthMass() {
    return MeasureUnit(16, 2);
}

MeasureUnit *MeasureUnit::createFun(UErrorCode &status) {
    return MeasureUnit::create(16, 3, status);
}

MeasureUnit MeasureUnit::getFun() {
    return MeasureUnit(16, 3);
}

MeasureUnit *MeasureUnit::createGrain(UErrorCode &status) {
    return MeasureUnit::create(16, 4, status);
}

MeasureUnit MeasureUnit::getGrain() {
    return MeasureUnit(16, 4);
}

MeasureUnit *MeasureUnit::createGram(UErrorCode &status) {
    return MeasureUnit::create(16, 5, status);
}

MeasureUnit MeasureUnit::getGram() {
    return MeasureUnit(16, 5);
}

MeasureUnit *MeasureUnit::createKilogram(UErrorCode &status) {
    return MeasureUnit::create(16, 6, status);
}

MeasureUnit MeasureUnit::getKilogram() {
    return MeasureUnit(16, 6);
}

MeasureUnit *MeasureUnit::createMicrogram(UErrorCode &status) {
    return MeasureUnit::create(16, 7, status);
}

MeasureUnit MeasureUnit::getMicrogram() {
    return MeasureUnit(16, 7);
}

MeasureUnit *MeasureUnit::createMilligram(UErrorCode &status) {
    return MeasureUnit::create(16, 8, status);
}

MeasureUnit MeasureUnit::getMilligram() {
    return MeasureUnit(16, 8);
}

MeasureUnit *MeasureUnit::createOunce(UErrorCode &status) {
    return MeasureUnit::create(16, 9, status);
}

MeasureUnit MeasureUnit::getOunce() {
    return MeasureUnit(16, 9);
}

MeasureUnit *MeasureUnit::createOunceTroy(UErrorCode &status) {
    return MeasureUnit::create(16, 10, status);
}

MeasureUnit MeasureUnit::getOunceTroy() {
    return MeasureUnit(16, 10);
}

MeasureUnit *MeasureUnit::createPound(UErrorCode &status) {
    return MeasureUnit::create(16, 11, status);
}

MeasureUnit MeasureUnit::getPound() {
    return MeasureUnit(16, 11);
}

MeasureUnit *MeasureUnit::createSlug(UErrorCode &status) {
    return MeasureUnit::create(16, 12, status);
}

MeasureUnit MeasureUnit::getSlug() {
    return MeasureUnit(16, 12);
}

MeasureUnit *MeasureUnit::createSolarMass(UErrorCode &status) {
    return MeasureUnit::create(16, 13, status);
}

MeasureUnit MeasureUnit::getSolarMass() {
    return MeasureUnit(16, 13);
}

MeasureUnit *MeasureUnit::createStone(UErrorCode &status) {
    return MeasureUnit::create(16, 14, status);
}

MeasureUnit MeasureUnit::getStone() {
    return MeasureUnit(16, 14);
}

MeasureUnit *MeasureUnit::createTon(UErrorCode &status) {
    return MeasureUnit::create(16, 15, status);
}

MeasureUnit MeasureUnit::getTon() {
    return MeasureUnit(16, 15);
}

MeasureUnit *MeasureUnit::createTonne(UErrorCode &status) {
    return MeasureUnit::create(16, 16, status);
}

MeasureUnit MeasureUnit::getTonne() {
    return MeasureUnit(16, 16);
}

MeasureUnit *MeasureUnit::createMetricTon(UErrorCode &status) {
    return MeasureUnit::create(16, 16, status);
}

MeasureUnit MeasureUnit::getMetricTon() {
    return MeasureUnit(16, 16);
}

MeasureUnit *MeasureUnit::createGigawatt(UErrorCode &status) {
    return MeasureUnit::create(18, 0, status);
}

MeasureUnit MeasureUnit::getGigawatt() {
    return MeasureUnit(18, 0);
}

MeasureUnit *MeasureUnit::createHorsepower(UErrorCode &status) {
    return MeasureUnit::create(18, 1, status);
}

MeasureUnit MeasureUnit::getHorsepower() {
    return MeasureUnit(18, 1);
}

MeasureUnit *MeasureUnit::createKilowatt(UErrorCode &status) {
    return MeasureUnit::create(18, 2, status);
}

MeasureUnit MeasureUnit::getKilowatt() {
    return MeasureUnit(18, 2);
}

MeasureUnit *MeasureUnit::createMegawatt(UErrorCode &status) {
    return MeasureUnit::create(18, 3, status);
}

MeasureUnit MeasureUnit::getMegawatt() {
    return MeasureUnit(18, 3);
}

MeasureUnit *MeasureUnit::createMilliwatt(UErrorCode &status) {
    return MeasureUnit::create(18, 4, status);
}

MeasureUnit MeasureUnit::getMilliwatt() {
    return MeasureUnit(18, 4);
}

MeasureUnit *MeasureUnit::createWatt(UErrorCode &status) {
    return MeasureUnit::create(18, 5, status);
}

MeasureUnit MeasureUnit::getWatt() {
    return MeasureUnit(18, 5);
}

MeasureUnit *MeasureUnit::createAtmosphere(UErrorCode &status) {
    return MeasureUnit::create(19, 0, status);
}

MeasureUnit MeasureUnit::getAtmosphere() {
    return MeasureUnit(19, 0);
}

MeasureUnit *MeasureUnit::createBar(UErrorCode &status) {
    return MeasureUnit::create(19, 1, status);
}

MeasureUnit MeasureUnit::getBar() {
    return MeasureUnit(19, 1);
}

MeasureUnit *MeasureUnit::createGasolineEnergyDensity(UErrorCode &status) {
    return MeasureUnit::create(19, 2, status);
}

MeasureUnit MeasureUnit::getGasolineEnergyDensity() {
    return MeasureUnit(19, 2);
}

MeasureUnit *MeasureUnit::createHectopascal(UErrorCode &status) {
    return MeasureUnit::create(19, 3, status);
}

MeasureUnit MeasureUnit::getHectopascal() {
    return MeasureUnit(19, 3);
}

MeasureUnit *MeasureUnit::createInchHg(UErrorCode &status) {
    return MeasureUnit::create(19, 4, status);
}

MeasureUnit MeasureUnit::getInchHg() {
    return MeasureUnit(19, 4);
}

MeasureUnit *MeasureUnit::createKilopascal(UErrorCode &status) {
    return MeasureUnit::create(19, 5, status);
}

MeasureUnit MeasureUnit::getKilopascal() {
    return MeasureUnit(19, 5);
}

MeasureUnit *MeasureUnit::createMegapascal(UErrorCode &status) {
    return MeasureUnit::create(19, 6, status);
}

MeasureUnit MeasureUnit::getMegapascal() {
    return MeasureUnit(19, 6);
}

MeasureUnit *MeasureUnit::createMillibar(UErrorCode &status) {
    return MeasureUnit::create(19, 7, status);
}

MeasureUnit MeasureUnit::getMillibar() {
    return MeasureUnit(19, 7);
}

MeasureUnit *MeasureUnit::createMillimeterOfMercury(UErrorCode &status) {
    return MeasureUnit::create(19, 8, status);
}

MeasureUnit MeasureUnit::getMillimeterOfMercury() {
    return MeasureUnit(19, 8);
}

MeasureUnit *MeasureUnit::createOfhg(UErrorCode &status) {
    return MeasureUnit::create(19, 9, status);
}

MeasureUnit MeasureUnit::getOfhg() {
    return MeasureUnit(19, 9);
}

MeasureUnit *MeasureUnit::createPascal(UErrorCode &status) {
    return MeasureUnit::create(19, 10, status);
}

MeasureUnit MeasureUnit::getPascal() {
    return MeasureUnit(19, 10);
}

MeasureUnit *MeasureUnit::createPoundPerSquareInch(UErrorCode &status) {
    return MeasureUnit::create(19, 11, status);
}

MeasureUnit MeasureUnit::getPoundPerSquareInch() {
    return MeasureUnit(19, 11);
}

MeasureUnit *MeasureUnit::createBeaufort(UErrorCode &status) {
    return MeasureUnit::create(20, 0, status);
}

MeasureUnit MeasureUnit::getBeaufort() {
    return MeasureUnit(20, 0);
}

MeasureUnit *MeasureUnit::createKilometerPerHour(UErrorCode &status) {
    return MeasureUnit::create(20, 1, status);
}

MeasureUnit MeasureUnit::getKilometerPerHour() {
    return MeasureUnit(20, 1);
}

MeasureUnit *MeasureUnit::createKnot(UErrorCode &status) {
    return MeasureUnit::create(20, 2, status);
}

MeasureUnit MeasureUnit::getKnot() {
    return MeasureUnit(20, 2);
}

MeasureUnit *MeasureUnit::createLightSpeed(UErrorCode &status) {
    return MeasureUnit::create(20, 3, status);
}

MeasureUnit MeasureUnit::getLightSpeed() {
    return MeasureUnit(20, 3);
}

MeasureUnit *MeasureUnit::createMeterPerSecond(UErrorCode &status) {
    return MeasureUnit::create(20, 4, status);
}

MeasureUnit MeasureUnit::getMeterPerSecond() {
    return MeasureUnit(20, 4);
}

MeasureUnit *MeasureUnit::createMilePerHour(UErrorCode &status) {
    return MeasureUnit::create(20, 5, status);
}

MeasureUnit MeasureUnit::getMilePerHour() {
    return MeasureUnit(20, 5);
}

MeasureUnit *MeasureUnit::createCelsius(UErrorCode &status) {
    return MeasureUnit::create(21, 0, status);
}

MeasureUnit MeasureUnit::getCelsius() {
    return MeasureUnit(21, 0);
}

MeasureUnit *MeasureUnit::createFahrenheit(UErrorCode &status) {
    return MeasureUnit::create(21, 1, status);
}

MeasureUnit MeasureUnit::getFahrenheit() {
    return MeasureUnit(21, 1);
}

MeasureUnit *MeasureUnit::createGenericTemperature(UErrorCode &status) {
    return MeasureUnit::create(21, 2, status);
}

MeasureUnit MeasureUnit::getGenericTemperature() {
    return MeasureUnit(21, 2);
}

MeasureUnit *MeasureUnit::createKelvin(UErrorCode &status) {
    return MeasureUnit::create(21, 3, status);
}

MeasureUnit MeasureUnit::getKelvin() {
    return MeasureUnit(21, 3);
}

MeasureUnit *MeasureUnit::createRankine(UErrorCode &status) {
    return MeasureUnit::create(21, 4, status);
}

MeasureUnit MeasureUnit::getRankine() {
    return MeasureUnit(21, 4);
}

MeasureUnit *MeasureUnit::createNewtonMeter(UErrorCode &status) {
    return MeasureUnit::create(22, 0, status);
}

MeasureUnit MeasureUnit::getNewtonMeter() {
    return MeasureUnit(22, 0);
}

MeasureUnit *MeasureUnit::createPoundFoot(UErrorCode &status) {
    return MeasureUnit::create(22, 1, status);
}

MeasureUnit MeasureUnit::getPoundFoot() {
    return MeasureUnit(22, 1);
}

MeasureUnit *MeasureUnit::createAcreFoot(UErrorCode &status) {
    return MeasureUnit::create(23, 0, status);
}

MeasureUnit MeasureUnit::getAcreFoot() {
    return MeasureUnit(23, 0);
}

MeasureUnit *MeasureUnit::createBarrel(UErrorCode &status) {
    return MeasureUnit::create(23, 1, status);
}

MeasureUnit MeasureUnit::getBarrel() {
    return MeasureUnit(23, 1);
}

MeasureUnit *MeasureUnit::createBushel(UErrorCode &status) {
    return MeasureUnit::create(23, 2, status);
}

MeasureUnit MeasureUnit::getBushel() {
    return MeasureUnit(23, 2);
}

MeasureUnit *MeasureUnit::createCentiliter(UErrorCode &status) {
    return MeasureUnit::create(23, 3, status);
}

MeasureUnit MeasureUnit::getCentiliter() {
    return MeasureUnit(23, 3);
}

MeasureUnit *MeasureUnit::createCubicCentimeter(UErrorCode &status) {
    return MeasureUnit::create(23, 4, status);
}

MeasureUnit MeasureUnit::getCubicCentimeter() {
    return MeasureUnit(23, 4);
}

MeasureUnit *MeasureUnit::createCubicFoot(UErrorCode &status) {
    return MeasureUnit::create(23, 5, status);
}

MeasureUnit MeasureUnit::getCubicFoot() {
    return MeasureUnit(23, 5);
}

MeasureUnit *MeasureUnit::createCubicInch(UErrorCode &status) {
    return MeasureUnit::create(23, 6, status);
}

MeasureUnit MeasureUnit::getCubicInch() {
    return MeasureUnit(23, 6);
}

MeasureUnit *MeasureUnit::createCubicKilometer(UErrorCode &status) {
    return MeasureUnit::create(23, 7, status);
}

MeasureUnit MeasureUnit::getCubicKilometer() {
    return MeasureUnit(23, 7);
}

MeasureUnit *MeasureUnit::createCubicMeter(UErrorCode &status) {
    return MeasureUnit::create(23, 8, status);
}

MeasureUnit MeasureUnit::getCubicMeter() {
    return MeasureUnit(23, 8);
}

MeasureUnit *MeasureUnit::createCubicMile(UErrorCode &status) {
    return MeasureUnit::create(23, 9, status);
}

MeasureUnit MeasureUnit::getCubicMile() {
    return MeasureUnit(23, 9);
}

MeasureUnit *MeasureUnit::createCubicYard(UErrorCode &status) {
    return MeasureUnit::create(23, 10, status);
}

MeasureUnit MeasureUnit::getCubicYard() {
    return MeasureUnit(23, 10);
}

MeasureUnit *MeasureUnit::createCup(UErrorCode &status) {
    return MeasureUnit::create(23, 11, status);
}

MeasureUnit MeasureUnit::getCup() {
    return MeasureUnit(23, 11);
}

MeasureUnit *MeasureUnit::createCupImperial(UErrorCode &status) {
    return MeasureUnit::create(23, 12, status);
}

MeasureUnit MeasureUnit::getCupImperial() {
    return MeasureUnit(23, 12);
}

MeasureUnit *MeasureUnit::createCupJp(UErrorCode &status) {
    return MeasureUnit::create(23, 13, status);
}

MeasureUnit MeasureUnit::getCupJp() {
    return MeasureUnit(23, 13);
}

MeasureUnit *MeasureUnit::createCupMetric(UErrorCode &status) {
    return MeasureUnit::create(23, 14, status);
}

MeasureUnit MeasureUnit::getCupMetric() {
    return MeasureUnit(23, 14);
}

MeasureUnit *MeasureUnit::createDeciliter(UErrorCode &status) {
    return MeasureUnit::create(23, 15, status);
}

MeasureUnit MeasureUnit::getDeciliter() {
    return MeasureUnit(23, 15);
}

MeasureUnit *MeasureUnit::createDessertSpoon(UErrorCode &status) {
    return MeasureUnit::create(23, 16, status);
}

MeasureUnit MeasureUnit::getDessertSpoon() {
    return MeasureUnit(23, 16);
}

MeasureUnit *MeasureUnit::createDessertSpoonImperial(UErrorCode &status) {
    return MeasureUnit::create(23, 17, status);
}

MeasureUnit MeasureUnit::getDessertSpoonImperial() {
    return MeasureUnit(23, 17);
}

MeasureUnit *MeasureUnit::createDram(UErrorCode &status) {
    return MeasureUnit::create(23, 18, status);
}

MeasureUnit MeasureUnit::getDram() {
    return MeasureUnit(23, 18);
}

MeasureUnit *MeasureUnit::createDrop(UErrorCode &status) {
    return MeasureUnit::create(23, 19, status);
}

MeasureUnit MeasureUnit::getDrop() {
    return MeasureUnit(23, 19);
}

MeasureUnit *MeasureUnit::createFluidOunce(UErrorCode &status) {
    return MeasureUnit::create(23, 20, status);
}

MeasureUnit MeasureUnit::getFluidOunce() {
    return MeasureUnit(23, 20);
}

MeasureUnit *MeasureUnit::createFluidOunceImperial(UErrorCode &status) {
    return MeasureUnit::create(23, 21, status);
}

MeasureUnit MeasureUnit::getFluidOunceImperial() {
    return MeasureUnit(23, 21);
}

MeasureUnit *MeasureUnit::createFluidOunceMetric(UErrorCode &status) {
    return MeasureUnit::create(23, 22, status);
}

MeasureUnit MeasureUnit::getFluidOunceMetric() {
    return MeasureUnit(23, 22);
}

MeasureUnit *MeasureUnit::createGallon(UErrorCode &status) {
    return MeasureUnit::create(23, 23, status);
}

MeasureUnit MeasureUnit::getGallon() {
    return MeasureUnit(23, 23);
}

MeasureUnit *MeasureUnit::createGallonImperial(UErrorCode &status) {
    return MeasureUnit::create(23, 24, status);
}

MeasureUnit MeasureUnit::getGallonImperial() {
    return MeasureUnit(23, 24);
}

MeasureUnit *MeasureUnit::createHectoliter(UErrorCode &status) {
    return MeasureUnit::create(23, 25, status);
}

MeasureUnit MeasureUnit::getHectoliter() {
    return MeasureUnit(23, 25);
}

MeasureUnit *MeasureUnit::createJigger(UErrorCode &status) {
    return MeasureUnit::create(23, 26, status);
}

MeasureUnit MeasureUnit::getJigger() {
    return MeasureUnit(23, 26);
}

MeasureUnit *MeasureUnit::createKoku(UErrorCode &status) {
    return MeasureUnit::create(23, 27, status);
}

MeasureUnit MeasureUnit::getKoku() {
    return MeasureUnit(23, 27);
}

MeasureUnit *MeasureUnit::createKosaji(UErrorCode &status) {
    return MeasureUnit::create(23, 28, status);
}

MeasureUnit MeasureUnit::getKosaji() {
    return MeasureUnit(23, 28);
}

MeasureUnit *MeasureUnit::createLiter(UErrorCode &status) {
    return MeasureUnit::create(23, 29, status);
}

MeasureUnit MeasureUnit::getLiter() {
    return MeasureUnit(23, 29);
}

MeasureUnit *MeasureUnit::createMegaliter(UErrorCode &status) {
    return MeasureUnit::create(23, 30, status);
}

MeasureUnit MeasureUnit::getMegaliter() {
    return MeasureUnit(23, 30);
}

MeasureUnit *MeasureUnit::createMilliliter(UErrorCode &status) {
    return MeasureUnit::create(23, 31, status);
}

MeasureUnit MeasureUnit::getMilliliter() {
    return MeasureUnit(23, 31);
}

MeasureUnit *MeasureUnit::createOsaji(UErrorCode &status) {
    return MeasureUnit::create(23, 32, status);
}

MeasureUnit MeasureUnit::getOsaji() {
    return MeasureUnit(23, 32);
}

MeasureUnit *MeasureUnit::createPinch(UErrorCode &status) {
    return MeasureUnit::create(23, 33, status);
}

MeasureUnit MeasureUnit::getPinch() {
    return MeasureUnit(23, 33);
}

MeasureUnit *MeasureUnit::createPint(UErrorCode &status) {
    return MeasureUnit::create(23, 34, status);
}

MeasureUnit MeasureUnit::getPint() {
    return MeasureUnit(23, 34);
}

MeasureUnit *MeasureUnit::createPintImperial(UErrorCode &status) {
    return MeasureUnit::create(23, 35, status);
}

MeasureUnit MeasureUnit::getPintImperial() {
    return MeasureUnit(23, 35);
}

MeasureUnit *MeasureUnit::createPintMetric(UErrorCode &status) {
    return MeasureUnit::create(23, 36, status);
}

MeasureUnit MeasureUnit::getPintMetric() {
    return MeasureUnit(23, 36);
}

MeasureUnit *MeasureUnit::createQuart(UErrorCode &status) {
    return MeasureUnit::create(23, 37, status);
}

MeasureUnit MeasureUnit::getQuart() {
    return MeasureUnit(23, 37);
}

MeasureUnit *MeasureUnit::createQuartImperial(UErrorCode &status) {
    return MeasureUnit::create(23, 38, status);
}

MeasureUnit MeasureUnit::getQuartImperial() {
    return MeasureUnit(23, 38);
}

MeasureUnit *MeasureUnit::createSai(UErrorCode &status) {
    return MeasureUnit::create(23, 39, status);
}

MeasureUnit MeasureUnit::getSai() {
    return MeasureUnit(23, 39);
}

MeasureUnit *MeasureUnit::createShaku(UErrorCode &status) {
    return MeasureUnit::create(23, 40, status);
}

MeasureUnit MeasureUnit::getShaku() {
    return MeasureUnit(23, 40);
}

MeasureUnit *MeasureUnit::createTablespoon(UErrorCode &status) {
    return MeasureUnit::create(23, 41, status);
}

MeasureUnit MeasureUnit::getTablespoon() {
    return MeasureUnit(23, 41);
}

MeasureUnit *MeasureUnit::createTeaspoon(UErrorCode &status) {
    return MeasureUnit::create(23, 42, status);
}

MeasureUnit MeasureUnit::getTeaspoon() {
    return MeasureUnit(23, 42);
}

MeasureUnit *MeasureUnit::createToJp(UErrorCode &status) {
    return MeasureUnit::create(23, 43, status);
}

MeasureUnit MeasureUnit::getToJp() {
    return MeasureUnit(23, 43);
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
    if (!findBySubType(impl.identifier.data(), this)) {
        fImpl = new MeasureUnitImpl(std::move(impl));
    }
}

MeasureUnit &MeasureUnit::operator=(const MeasureUnit &other) {
    if (this == &other) {
        return *this;
    }
    delete fImpl;
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
    delete fImpl;
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
        return nullptr;
    }
    StringEnumeration *result = new UStringEnumeration(uenum);
    if (result == nullptr) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        uenum_close(uenum);
        return nullptr;
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
        return nullptr;
    }
    MeasureUnit *result = new MeasureUnit(typeId, subTypeId);
    if (result == nullptr) {
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
        UErrorCode status = U_ZERO_ERROR;
        fImpl = new MeasureUnitImpl(MeasureUnitImpl::forCurrencyCode(isoCurrency, status));
        if (fImpl != nullptr) {
            if (U_SUCCESS(status)) {
                fSubTypeId = -1;
                return;
            } else {
                delete fImpl;
                fImpl = nullptr;
            }
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
    result.identifier = identifier;
    if (result.identifier.isEmpty() != identifier.isEmpty()) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return result;
    }
    result.constantDenominator = constantDenominator;
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
        case UAMEASUNIT_ANGLE_STERADIAN:        munit = MeasureUnit::createSteradian(*status);   break;

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
        case UAMEASUNIT_AREA_BU_JP:             munit = MeasureUnit::createBuJp(*status);            break;
        case UAMEASUNIT_AREA_CHO:               munit = MeasureUnit::createCho(*status);             break;
        case UAMEASUNIT_AREA_SE_JP:             munit = MeasureUnit::createSeJp(*status);            break;

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
        case UAMEASUNIT_DURATION_NIGHT:         munit = MeasureUnit::createNight(*status);       break;
        case UAMEASUNIT_DURATION_FORTNIGHT:     munit = MeasureUnit::createFortnight(*status);   break;

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
        case UAMEASUNIT_LENGTH_CHAIN:           munit = MeasureUnit::createChain(*status);       break;
        case UAMEASUNIT_LENGTH_JO_JP:           munit = MeasureUnit::createJoJp(*status);        break;
        case UAMEASUNIT_LENGTH_KEN:             munit = MeasureUnit::createKen(*status);         break;
        case UAMEASUNIT_LENGTH_RI_JP:           munit = MeasureUnit::createRiJp(*status);        break;
        case UAMEASUNIT_LENGTH_RIN:             munit = MeasureUnit::createRin(*status);         break;
        case UAMEASUNIT_LENGTH_ROD:             munit = MeasureUnit::createRod(*status);         break;
        case UAMEASUNIT_LENGTH_SHAKU_CLOTH:     munit = MeasureUnit::createShakuCloth(*status);  break;
        case UAMEASUNIT_LENGTH_SHAKU_LENGTH:    munit = MeasureUnit::createShakuLength(*status); break;
        case UAMEASUNIT_LENGTH_SUN:             munit = MeasureUnit::createSun(*status);         break;

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
        case UAMEASUNIT_MASS_FUN:               munit = MeasureUnit::createFun(*status);         break;
        case UAMEASUNIT_MASS_SLUG:              munit = MeasureUnit::createSlug(*status);        break;

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
        case UAMEASUNIT_PRESSURE_OFHG:          munit = MeasureUnit::createOfhg(*status);        break;

        case UAMEASUNIT_SPEED_METER_PER_SECOND:   munit = MeasureUnit::createMeterPerSecond(*status);   break;
        case UAMEASUNIT_SPEED_KILOMETER_PER_HOUR: munit = MeasureUnit::createKilometerPerHour(*status); break;
        case UAMEASUNIT_SPEED_MILE_PER_HOUR:      munit = MeasureUnit::createMilePerHour(*status);      break;
        case UAMEASUNIT_SPEED_KNOT:               munit = MeasureUnit::createKnot(*status);      break;
        case UAMEASUNIT_SPEED_BEAUFORT:           munit = MeasureUnit::createBeaufort(*status);         break;
        case UAMEASUNIT_SPEED_LIGHT_SPEED:        munit = MeasureUnit::createLightSpeed(*status);       break;

        case UAMEASUNIT_TEMPERATURE_CELSIUS:    munit = MeasureUnit::createCelsius(*status);     break;
        case UAMEASUNIT_TEMPERATURE_FAHRENHEIT: munit = MeasureUnit::createFahrenheit(*status);  break;
        case UAMEASUNIT_TEMPERATURE_KELVIN:     munit = MeasureUnit::createKelvin(*status);      break;
        case UAMEASUNIT_TEMPERATURE_GENERIC:    munit = MeasureUnit::createGenericTemperature(*status); break;
        case UAMEASUNIT_TEMPERATURE_RANKINE:    munit = MeasureUnit::createRankine(*status);     break;

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
        case UAMEASUNIT_VOLUME_CUP_IMPERIAL:    munit = MeasureUnit::createCupImperial(*status);    break;
        case UAMEASUNIT_VOLUME_CUP_JP:          munit = MeasureUnit::createCupJp(*status);          break;
        case UAMEASUNIT_VOLUME_FLUID_OUNCE_METRIC: munit = MeasureUnit::createFluidOunceMetric(*status); break;
        case UAMEASUNIT_VOLUME_KOKU:            munit = MeasureUnit::createKoku(*status);           break;
        case UAMEASUNIT_VOLUME_KOSAJI:          munit = MeasureUnit::createKosaji(*status);         break;
        case UAMEASUNIT_VOLUME_OSAJI:           munit = MeasureUnit::createOsaji(*status);          break;
        case UAMEASUNIT_VOLUME_PINT_IMPERIAL:   munit = MeasureUnit::createPintImperial(*status);   break;
        case UAMEASUNIT_VOLUME_SAI:             munit = MeasureUnit::createSai(*status);            break;
        case UAMEASUNIT_VOLUME_SHAKU:           munit = MeasureUnit::createShaku(*status);          break;
        case UAMEASUNIT_VOLUME_TO_JP:           munit = MeasureUnit::createToJp(*status);           break;

        case UAMEASUNIT_ENERGY_JOULE:           munit = MeasureUnit::createJoule(*status);          break;
        case UAMEASUNIT_ENERGY_KILOJOULE:       munit = MeasureUnit::createKilojoule(*status);      break;
        case UAMEASUNIT_ENERGY_CALORIE:         munit = MeasureUnit::createCalorie(*status);        break;
        case UAMEASUNIT_ENERGY_KILOCALORIE:     munit = MeasureUnit::createKilocalorie(*status);    break;
        case UAMEASUNIT_ENERGY_FOODCALORIE:     munit = MeasureUnit::createFoodcalorie(*status);    break;
        case UAMEASUNIT_ENERGY_KILOWATT_HOUR:   munit = MeasureUnit::createKilowattHour(*status);   break;
        case UAMEASUNIT_ENERGY_ELECTRONVOLT:    munit = MeasureUnit::createElectronvolt(*status);   break;
        case UAMEASUNIT_ENERGY_BRITISH_THERMAL_UNIT: munit = MeasureUnit::createBritishThermalUnit(*status); break;
        case UAMEASUNIT_ENERGY_THERM_US:        munit = MeasureUnit::createThermUs(*status);        break;
        case UAMEASUNIT_ENERGY_BECQUEREL:       munit = MeasureUnit::createBecquerel(*status);      break;
        case UAMEASUNIT_ENERGY_BRITISH_THERMAL_UNIT_IT: munit = MeasureUnit::createBritishThermalUnitIt(*status); break;
        case UAMEASUNIT_ENERGY_CALORIE_IT:      munit = MeasureUnit::createCalorieIt(*status);      break;
        case UAMEASUNIT_ENERGY_GRAY:            munit = MeasureUnit::createGray(*status);           break;
        case UAMEASUNIT_ENERGY_SIEVERT:         munit = MeasureUnit::createSievert(*status);        break;

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
        case UAMEASUNIT_ELECTRIC_COULOMB:       munit = MeasureUnit::createCoulomb(*status);     break;
        case UAMEASUNIT_ELECTRIC_FARAD:         munit = MeasureUnit::createFarad(*status);       break;
        case UAMEASUNIT_ELECTRIC_HENRY:         munit = MeasureUnit::createHenry(*status);       break;
        case UAMEASUNIT_ELECTRIC_SIEMENS:       munit = MeasureUnit::createSiemens(*status);     break;

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
        case UAMEASUNIT_CONCENTRATION_MILLIGRAM_OFGLUCOSE_PER_DECILITER: munit = MeasureUnit::createMilligramOfglucosePerDeciliter(*status); break; // milligram-per-deciliter was renamed...
        case UAMEASUNIT_CONCENTRATION_MILLIMOLE_PER_LITER:     munit = MeasureUnit::createMillimolePerLiter(*status);     break;
        case UAMEASUNIT_CONCENTRATION_PART_PER_MILLION:        munit = MeasureUnit::createPartPer1E6(*status);        break;
        case UAMEASUNIT_CONCENTRATION_PERCENT:  munit = MeasureUnit::createPercent(*status);     break;
        case UAMEASUNIT_CONCENTRATION_PERMILLE: munit = MeasureUnit::createPermille(*status);    break;
        case UAMEASUNIT_CONCENTRATION_PERMYRIAD: munit = MeasureUnit::createPermyriad(*status);  break;
        case UAMEASUNIT_CONCENTRATION_MOLE:     munit = MeasureUnit::createMole(*status);        break;
        case UAMEASUNIT_CONCENTRATION_ITEM:     munit = MeasureUnit::createItem(*status);        break;
        case UAMEASUNIT_CONCENTRATION_KATAL:    munit = MeasureUnit::createKatal(*status);       break;
        case UAMEASUNIT_CONCENTRATION_OFGLUCOSE: munit = MeasureUnit::createOfglucose(*status);  break;
        case UAMEASUNIT_CONCENTRATION_PART:     munit = MeasureUnit::createPart(*status);        break;
        case UAMEASUNIT_CONCENTRATION_PART_PER_1E9: munit = MeasureUnit::createPartPer1E9(*status); break;

        case UAMEASUNIT_FORCE_NEWTON:           munit = MeasureUnit::createNewton(*status);      break;
        case UAMEASUNIT_FORCE_POUND_FORCE:      munit = MeasureUnit::createPoundForce(*status);  break;
        case UAMEASUNIT_FORCE_KILOWATT_HOUR_PER_100_KILOMETER: munit = MeasureUnit::createKilowattHourPer100Kilometer(*status); break;
        case UAMEASUNIT_FORCE_KILOGRAM_FORCE:   munit = MeasureUnit::createKilogramForce(*status); break;

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
        
        case UAMEASUNIT_MAGNETIC_TESLA:         munit = MeasureUnit::createTesla(*status);      break;
        case UAMEASUNIT_MAGNETIC_WEBER:         munit = MeasureUnit::createWeber(*status);      break;

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
    UAMEASUNIT_ANGLE_STERADIAN,                       // (1 << 8) + 5,   # 7   steradian
    //                                                                   # --- area (8)
    UAMEASUNIT_AREA_ACRE,                             // (2 << 8) + 4,   # 8   acre
    UAMEASUNIT_AREA_BU_JP,                            // (2 << 8) + 10,  # 9   bu-jp
    UAMEASUNIT_AREA_CHO,                              // (2 << 8) + 11,  # 10  cho
    UAMEASUNIT_AREA_DUNAM,                            // (2 << 8) + 9,   # 11  dunam
    UAMEASUNIT_AREA_HECTARE,                          // (2 << 8) + 5,   # 12  hectare
    UAMEASUNIT_AREA_SE_JP,                            // (2 << 8) + 12,  # 13  se-jp
    UAMEASUNIT_AREA_SQUARE_CENTIMETER,                // (2 << 8) + 6,   # 14  square-centimeter
    UAMEASUNIT_AREA_SQUARE_FOOT,                      // (2 << 8) + 2,   # 15  square-foot
    UAMEASUNIT_AREA_SQUARE_INCH,                      // (2 << 8) + 7,   # 16  square-inch
    UAMEASUNIT_AREA_SQUARE_KILOMETER,                 // (2 << 8) + 1,   # 17  square-kilometer
    UAMEASUNIT_AREA_SQUARE_METER,                     // (2 << 8) + 0,   # 18  square-meter
    UAMEASUNIT_AREA_SQUARE_MILE,                      // (2 << 8) + 3,   # 19  square-mile
    UAMEASUNIT_AREA_SQUARE_YARD,                      // (2 << 8) + 8,   # 20  square-yard
    //                                                                   # --- concentr (21)
    UAMEASUNIT_CONCENTRATION_ITEM,                    // (18 << 8) + 8,  # 21  item
    UAMEASUNIT_CONCENTRATION_KARAT,                   // (18 << 8) + 0,  # 22  karat
    UAMEASUNIT_CONCENTRATION_KATAL,                   // (18 << 8) + 11, # 23  katal
    UAMEASUNIT_CONCENTRATION_MILLIGRAM_OFGLUCOSE_PER_DECILITER, // (18 << 8) + 9,  # 24  milligram-ofglucose-per-deciliter
    UAMEASUNIT_CONCENTRATION_MILLIMOLE_PER_LITER,     // (18 << 8) + 2,  # 25  millimole-per-liter
    UAMEASUNIT_CONCENTRATION_MOLE,                    // (18 << 8) + 7,  # 26  mole
    UAMEASUNIT_CONCENTRATION_OFGLUCOSE,               // (18 << 8) + 10, # 27  ofglucose
    UAMEASUNIT_CONCENTRATION_PART,                    // (18 << 8) + 12, # 28  part
    UAMEASUNIT_CONCENTRATION_PART_PER_1E6,            // (18 << 8) + 13, # 29  part-per-1e6
    UAMEASUNIT_CONCENTRATION_PART_PER_1E9,            // (18 << 8) + 14, # 30  part-per-1e9
    UAMEASUNIT_CONCENTRATION_PERCENT,                 // (18 << 8) + 4,  # 31  percent
    UAMEASUNIT_CONCENTRATION_PERMILLE,                // (18 << 8) + 5,  # 32  permille
    UAMEASUNIT_CONCENTRATION_PERMYRIAD,               // (18 << 8) + 6,  # 33  permyriad
    //                                                                   # --- consumption (34)
    UAMEASUNIT_CONSUMPTION_LITER_PER_100_KILOMETERs,  // (13 << 8) + 2,  # 34  liter-per-100-kilometer
    UAMEASUNIT_CONSUMPTION_LITER_PER_KILOMETER,       // (13 << 8) + 0,  # 35  liter-per-kilometer
    UAMEASUNIT_CONSUMPTION_MILE_PER_GALLON,           // (13 << 8) + 1,  # 36  mile-per-gallon
    UAMEASUNIT_CONSUMPTION_MILE_PER_GALLON_IMPERIAL,  // (13 << 8) + 3,  # 37  mile-per-gallon-imperial
    //                                                                   # --- currency (38)
    //                                                                   # --- digital (38)
    UAMEASUNIT_DIGITAL_BIT,                           // (14 << 8) + 0,  # 38  bit
    UAMEASUNIT_DIGITAL_BYTE,                          // (14 << 8) + 1,  # 39  byte
    UAMEASUNIT_DIGITAL_GIGABIT,                       // (14 << 8) + 2,  # 40  gigabit
    UAMEASUNIT_DIGITAL_GIGABYTE,                      // (14 << 8) + 3,  # 41  gigabyte
    UAMEASUNIT_DIGITAL_KILOBIT,                       // (14 << 8) + 4,  # 42  kilobit
    UAMEASUNIT_DIGITAL_KILOBYTE,                      // (14 << 8) + 5,  # 43  kilobyte
    UAMEASUNIT_DIGITAL_MEGABIT,                       // (14 << 8) + 6,  # 44  megabit
    UAMEASUNIT_DIGITAL_MEGABYTE,                      // (14 << 8) + 7,  # 45  megabyte
    UAMEASUNIT_DIGITAL_PETABYTE,                      // (14 << 8) + 10, # 46  petabyte
    UAMEASUNIT_DIGITAL_TERABIT,                       // (14 << 8) + 8,  # 47  terabit
    UAMEASUNIT_DIGITAL_TERABYTE,                      // (14 << 8) + 9,  # 48  terabyte
    //                                                                   # --- duration (49)
    UAMEASUNIT_DURATION_CENTURY,                      // (4 << 8) + 10,  # 49  century
    UAMEASUNIT_DURATION_DAY,                          // (4 << 8) + 3,   # 50  day
    UAMEASUNIT_DURATION_DAY_PERSON,                   // (4 << 8) + 14,  # 51  day-person
    UAMEASUNIT_DURATION_DECADE,                       // (4 << 8) + 15,  # 52  decade
    UAMEASUNIT_DURATION_FORTNIGHT,                    // (4 << 8) + 18,  # 53  fortnight
    UAMEASUNIT_DURATION_HOUR,                         // (4 << 8) + 4,   # 54  hour
    UAMEASUNIT_DURATION_MICROSECOND,                  // (4 << 8) + 8,   # 55  microsecond
    UAMEASUNIT_DURATION_MILLISECOND,                  // (4 << 8) + 7,   # 56  millisecond
    UAMEASUNIT_DURATION_MINUTE,                       // (4 << 8) + 5,   # 57  minute
    UAMEASUNIT_DURATION_MONTH,                        // (4 << 8) + 1,   # 58  month
    UAMEASUNIT_DURATION_MONTH_PERSON,                 // (4 << 8) + 12,  # 59  month-person
    UAMEASUNIT_DURATION_NANOSECOND,                   // (4 << 8) + 9,   # 60  nanosecond
    UAMEASUNIT_DURATION_NIGHT,                        // (4 << 8) + 17,  # 61  night
    UAMEASUNIT_DURATION_QUARTER,                      // (4 << 8) + 16,  # 62  quarter
    UAMEASUNIT_DURATION_SECOND,                       // (4 << 8) + 6,   # 63  second
    UAMEASUNIT_DURATION_WEEK,                         // (4 << 8) + 2,   # 64  week
    UAMEASUNIT_DURATION_WEEK_PERSON,                  // (4 << 8) + 13,  # 65  week-person
    UAMEASUNIT_DURATION_YEAR,                         // (4 << 8) + 0,   # 66  year
    UAMEASUNIT_DURATION_YEAR_PERSON,                  // (4 << 8) + 11,  # 67  year-person
    //                                                                   # --- electric (68)
    UAMEASUNIT_ELECTRIC_AMPERE,                       // (15 << 8) + 0,  # 68  ampere
    UAMEASUNIT_ELECTRIC_COULOMB,                      // (15 << 8) + 4,  # 69  coulomb
    UAMEASUNIT_ELECTRIC_FARAD,                        // (15 << 8) + 5,  # 70  farad
    UAMEASUNIT_ELECTRIC_HENRY,                        // (15 << 8) + 6,  # 71  henry
    UAMEASUNIT_ELECTRIC_MILLIAMPERE,                  // (15 << 8) + 1,  # 72  milliampere
    UAMEASUNIT_ELECTRIC_OHM,                          // (15 << 8) + 2,  # 73  ohm
    UAMEASUNIT_ELECTRIC_SIEMENS,                      // (15 << 8) + 7,  # 74  siemens
    UAMEASUNIT_ELECTRIC_VOLT,                         // (15 << 8) + 3,  # 75  volt
    //                                                                   # --- energy (76)
    UAMEASUNIT_ENERGY_BECQUEREL,                      // (12 << 8) + 9,  # 76  becquerel
    UAMEASUNIT_ENERGY_BRITISH_THERMAL_UNIT,           // (12 << 8) + 7,  # 77  british-thermal-unit
    UAMEASUNIT_ENERGY_BRITISH_THERMAL_UNIT_IT,        // (12 << 8) + 10, # 78  british-thermal-unit-it
    UAMEASUNIT_ENERGY_CALORIE,                        // (12 << 8) + 0,  # 79  calorie
    UAMEASUNIT_ENERGY_CALORIE_IT,                     // (12 << 8) + 11, # 80  calorie-it
    UAMEASUNIT_ENERGY_ELECTRONVOLT,                   // (12 << 8) + 6,  # 81  electronvolt
    UAMEASUNIT_ENERGY_FOODCALORIE,                    // (12 << 8) + 1,  # 82  foodcalorie
    UAMEASUNIT_ENERGY_GRAY,                           // (12 << 8) + 12, # 83  gray
    UAMEASUNIT_ENERGY_JOULE,                          // (12 << 8) + 2,  # 84  joule
    UAMEASUNIT_ENERGY_KILOCALORIE,                    // (12 << 8) + 3,  # 85  kilocalorie
    UAMEASUNIT_ENERGY_KILOJOULE,                      // (12 << 8) + 4,  # 86  kilojoule
    UAMEASUNIT_ENERGY_KILOWATT_HOUR,                  // (12 << 8) + 5,  # 87  kilowatt-hour
    UAMEASUNIT_ENERGY_SIEVERT,                        // (12 << 8) + 13, # 88  sievert
    UAMEASUNIT_ENERGY_THERM_US,                       // (12 << 8) + 8,  # 89  therm-us
    //                                                                   # --- force (90)
    UAMEASUNIT_FORCE_KILOGRAM_FORCE,                  // (19 << 8) + 3,  # 90  kilogram-force
    UAMEASUNIT_FORCE_KILOWATT_HOUR_PER_100_KILOMETER, // (19 << 8) + 2,  # 91  kilowatt-hour-per-100-kilometer
    UAMEASUNIT_FORCE_NEWTON,                          // (19 << 8) + 0,  # 92  newton
    UAMEASUNIT_FORCE_POUND_FORCE,                     // (19 << 8) + 1,  # 93  pound-force
    //                                                                   # --- frequency (94)
    UAMEASUNIT_FREQUENCY_GIGAHERTZ,                   // (16 << 8) + 3,  # 94  gigahertz
    UAMEASUNIT_FREQUENCY_HERTZ,                       // (16 << 8) + 0,  # 95  hertz
    UAMEASUNIT_FREQUENCY_KILOHERTZ,                   // (16 << 8) + 1,  # 96  kilohertz
    UAMEASUNIT_FREQUENCY_MEGAHERTZ,                   // (16 << 8) + 2,  # 97  megahertz
    //                                                                   # --- graphics (98)
    UAMEASUNIT_GRAPHICS_DOT,                          // (21 << 8) + 7,  # 98  dot
    UAMEASUNIT_GRAPHICS_DOT_PER_CENTIMETER,           // (21 << 8) + 5,  # 99  dot-per-centimeter
    UAMEASUNIT_GRAPHICS_DOT_PER_INCH,                 // (21 << 8) + 6,  # 100 dot-per-inch
    UAMEASUNIT_GRAPHICS_EM,                           // (21 << 8) + 0,  # 101 em
    UAMEASUNIT_GRAPHICS_MEGAPIXEL,                    // (21 << 8) + 2,  # 102 megapixel
    UAMEASUNIT_GRAPHICS_PIXEL,                        // (21 << 8) + 1,  # 103 pixel
    UAMEASUNIT_GRAPHICS_PIXEL_PER_CENTIMETER,         // (21 << 8) + 3,  # 104 pixel-per-centimeter
    UAMEASUNIT_GRAPHICS_PIXEL_PER_INCH,               // (21 << 8) + 4,  # 105 pixel-per-inch
    //                                                                   # --- length (106)
    UAMEASUNIT_LENGTH_ASTRONOMICAL_UNIT,              // (5 << 8) + 16,  # 106 astronomical-unit
    UAMEASUNIT_LENGTH_CENTIMETER,                     // (5 << 8) + 1,   # 107 centimeter
    UAMEASUNIT_LENGTH_CHAIN,                          // (5 << 8) + 22,  # 108 chain
    UAMEASUNIT_LENGTH_DECIMETER,                      // (5 << 8) + 10,  # 109 decimeter
    UAMEASUNIT_LENGTH_EARTH_RADIUS,                   // (5 << 8) + 21,  # 110 earth-radius
    UAMEASUNIT_LENGTH_FATHOM,                         // (5 << 8) + 14,  # 111 fathom
    UAMEASUNIT_LENGTH_FOOT,                           // (5 << 8) + 5,   # 112 foot
    UAMEASUNIT_LENGTH_FURLONG,                        // (5 << 8) + 15,  # 113 furlong
    UAMEASUNIT_LENGTH_INCH,                           // (5 << 8) + 6,   # 114 inch
    UAMEASUNIT_LENGTH_JO_JP,                          // (5 << 8) + 23,  # 115 jo-jp
    UAMEASUNIT_LENGTH_KEN,                            // (5 << 8) + 24,  # 116 ken
    UAMEASUNIT_LENGTH_KILOMETER,                      // (5 << 8) + 2,   # 117 kilometer
    UAMEASUNIT_LENGTH_LIGHT_YEAR,                     // (5 << 8) + 9,   # 118 light-year
    UAMEASUNIT_LENGTH_METER,                          // (5 << 8) + 0,   # 119 meter
    UAMEASUNIT_LENGTH_MICROMETER,                     // (5 << 8) + 11,  # 120 micrometer
    UAMEASUNIT_LENGTH_MILE,                           // (5 << 8) + 7,   # 121 mile
    UAMEASUNIT_LENGTH_MILE_SCANDINAVIAN,              // (5 << 8) + 18,  # 122 mile-scandinavian
    UAMEASUNIT_LENGTH_MILLIMETER,                     // (5 << 8) + 3,   # 123 millimeter
    UAMEASUNIT_LENGTH_NANOMETER,                      // (5 << 8) + 12,  # 124 nanometer
    UAMEASUNIT_LENGTH_NAUTICAL_MILE,                  // (5 << 8) + 13,  # 125 nautical-mile
    UAMEASUNIT_LENGTH_PARSEC,                         // (5 << 8) + 17,  # 126 parsec
    UAMEASUNIT_LENGTH_PICOMETER,                      // (5 << 8) + 4,   # 127 picometer
    UAMEASUNIT_LENGTH_POINT,                          // (5 << 8) + 19,  # 128 point
    UAMEASUNIT_LENGTH_RI_JP,                          // (5 << 8) + 25,  # 129 ri-jp
    UAMEASUNIT_LENGTH_RIN,                            // (5 << 8) + 26,  # 130 rin
    UAMEASUNIT_LENGTH_ROD,                            // (5 << 8) + 27,  # 131 rod
    UAMEASUNIT_LENGTH_SHAKU_CLOTH,                    // (5 << 8) + 28,  # 132 shaku-cloth
    UAMEASUNIT_LENGTH_SHAKU_LENGTH,                   // (5 << 8) + 29,  # 133 shaku-length
    UAMEASUNIT_LENGTH_SOLAR_RADIUS,                   // (5 << 8) + 20,  # 134 solar-radius
    UAMEASUNIT_LENGTH_SUN,                            // (5 << 8) + 30,  # 135 sun
    UAMEASUNIT_LENGTH_YARD,                           // (5 << 8) + 8,   # 136 yard
    //                                                                   # --- light (137)
    UAMEASUNIT_LIGHT_CANDELA,                         // (17 << 8) + 2   # 137 candela
    UAMEASUNIT_LIGHT_LUMEN,                           // (17 << 8) + 3   # 138 lumen
    UAMEASUNIT_LIGHT_LUX,                             // (17 << 8) + 0,  # 139 lux
    UAMEASUNIT_LIGHT_SOLAR_LUMINOSITY,                // (17 << 8) + 1,  # 140 solar-luminosity
    //                                                                   # --- magnetic (141)
    UAMEASUNIT_MAGNETIC_TESLA,                        // (22 << 8) + 0,  # 141 tesla
    UAMEASUNIT_MAGNETIC_WEBER,                        // (22 << 8) + 1,  # 142 weber
    //                                                                   # --- mass (143)
    UAMEASUNIT_MASS_CARAT,                            // (6 << 8) + 9,   # 143 carat
    UAMEASUNIT_MASS_DALTON,                           // (6 << 8) + 11,  # 144 dalton
    UAMEASUNIT_MASS_EARTH_MASS,                       // (6 << 8) + 12,  # 145 earth-mass
    UAMEASUNIT_MASS_FUN,                              // (6 << 8) + 15,  # 146 fun
    UAMEASUNIT_MASS_GRAIN,                            // (6 << 8) + 14,  # 147 grain
    UAMEASUNIT_MASS_GRAM,                             // (6 << 8) + 0,   # 148 gram
    UAMEASUNIT_MASS_KILOGRAM,                         // (6 << 8) + 1,   # 149 kilogram
    UAMEASUNIT_MASS_MICROGRAM,                        // (6 << 8) + 5,   # 150 microgram
    UAMEASUNIT_MASS_MILLIGRAM,                        // (6 << 8) + 6,   # 151 milligram
    UAMEASUNIT_MASS_OUNCE,                            // (6 << 8) + 2,   # 152 ounce
    UAMEASUNIT_MASS_OUNCE_TROY,                       // (6 << 8) + 10,  # 153 ounce-troy
    UAMEASUNIT_MASS_POUND,                            // (6 << 8) + 3,   # 154 pound
    UAMEASUNIT_MASS_SLUG,                             // (6 << 8) + 16,  # 155 slug
    UAMEASUNIT_MASS_SOLAR_MASS,                       // (6 << 8) + 13,  # 156 solar-mass
    UAMEASUNIT_MASS_STONE,                            // (6 << 8) + 4,   # 157 stone
    UAMEASUNIT_MASS_TON,                              // (6 << 8) + 8,   # 158 ton
    UAMEASUNIT_MASS_TONNE,                            // (6 << 8) + 7,   # 159 tonne
    //                                                                   # --- none (160)
    UAMEASUNIT_CONCENTRATION_PERCENT,                 // BOGUS           # 160 base
    //                                                                   # --- power (161)
    UAMEASUNIT_POWER_GIGAWATT,                        // (7 << 8) + 5,   # 161 gigawatt
    UAMEASUNIT_POWER_HORSEPOWER,                      // (7 << 8) + 2,   # 162 horsepower
    UAMEASUNIT_POWER_KILOWATT,                        // (7 << 8) + 1,   # 163 kilowatt
    UAMEASUNIT_POWER_MEGAWATT,                        // (7 << 8) + 4,   # 164 megawatt
    UAMEASUNIT_POWER_MILLIWATT,                       // (7 << 8) + 3,   # 165 milliwatt
    UAMEASUNIT_POWER_WATT,                            // (7 << 8) + 0,   # 166 watt
    //                                                                   # --- pressure (167)
    UAMEASUNIT_PRESSURE_ATMOSPHERE,                   // (8 << 8) + 5,   # 167 atmosphere
    UAMEASUNIT_PRESSURE_BAR,                          // (8 << 8) + 9,   # 168 bar
    UAMEASUNIT_PRESSURE_GASOLINE_ENERGY_DENSITY,      // (8 << 8) + 10,  # 169 gasoline-energy-density
    UAMEASUNIT_PRESSURE_HECTOPASCAL,                  // (8 << 8) + 0,   # 170 hectopascal
    UAMEASUNIT_PRESSURE_INCH_HG,                      // (8 << 8) + 1,   # 171 inch-ofhg
    UAMEASUNIT_PRESSURE_KILOPASCAL,                   // (8 << 8) + 6,   # 172 kilopascal
    UAMEASUNIT_PRESSURE_MEGAPASCAL,                   // (8 << 8) + 7,   # 173 megapascal
    UAMEASUNIT_PRESSURE_MILLIBAR,                     // (8 << 8) + 2,   # 174 millibar
    UAMEASUNIT_PRESSURE_MILLIMETER_OF_MERCURY,        // (8 << 8) + 3,   # 175 millimeter-ofhg
    UAMEASUNIT_PRESSURE_OFHG,                         // (8 << 8) + 11,  # 176 ofhg
    UAMEASUNIT_PRESSURE_PASCAL,                       // (8 << 8) + 8,   # 177 pascal
    UAMEASUNIT_PRESSURE_POUND_PER_SQUARE_INCH,        // (8 << 8) + 4,   # 178 pound-force-per-square-inch
    //                                                                   # --- speed (179)
    UAMEASUNIT_SPEED_BEAUFORT,                        // (9 << 8) + 4.   $ 179 beaufort
    UAMEASUNIT_SPEED_KILOMETER_PER_HOUR,              // (9 << 8) + 1,   # 180 kilometer-per-hour
    UAMEASUNIT_SPEED_KNOT,                            // (9 << 8) + 3,   # 181 knot
    UAMEASUNIT_SPEED_LIGHT_SPEED,                     // (9 << 8) + 5,   # 182 light-speed
    UAMEASUNIT_SPEED_METER_PER_SECOND,                // (9 << 8) + 0,   # 183 meter-per-second
    UAMEASUNIT_SPEED_MILE_PER_HOUR,                   // (9 << 8) + 2,   # 184 mile-per-hour
    //                                                                   # --- temperature (185)
    UAMEASUNIT_TEMPERATURE_CELSIUS,                   // (10 << 8) + 0,  # 185 celsius
    UAMEASUNIT_TEMPERATURE_FAHRENHEIT,                // (10 << 8) + 1,  # 186 fahrenheit
    UAMEASUNIT_TEMPERATURE_GENERIC,                   // (10 << 8) + 3,  # 187 generic
    UAMEASUNIT_TEMPERATURE_KELVIN,                    // (10 << 8) + 2,  # 188 kelvin
    UAMEASUNIT_TEMPERATURE_RANKINE,                   // (10 << 8) + 4,  # 189 rankine
    //                                                                   # --- torque (190)
    UAMEASUNIT_TORQUE_NEWTON_METER,                   // (20 << 8) + 0,  # 190 newton-meter
    UAMEASUNIT_TORQUE_POUND_FOOT,                     // (20 << 8) + 1,  # 191 pound-force-foot
    //                                                                   # --- volume (192)
    UAMEASUNIT_VOLUME_ACRE_FOOT,                      // (11 << 8) + 13, # 192 acre-foot
    UAMEASUNIT_VOLUME_BARREL,                         // (11 << 8) + 26, # 193 barrel
    UAMEASUNIT_VOLUME_BUSHEL,                         // (11 << 8) + 14, # 194 bushel
    UAMEASUNIT_VOLUME_CENTILITER,                     // (11 << 8) + 4,  # 195 centiliter
    UAMEASUNIT_VOLUME_CUBIC_CENTIMETER,               // (11 << 8) + 8,  # 196 cubic-centimeter
    UAMEASUNIT_VOLUME_CUBIC_FOOT,                     // (11 << 8) + 11, # 197 cubic-foot
    UAMEASUNIT_VOLUME_CUBIC_INCH,                     // (11 << 8) + 10, # 198 cubic-inch
    UAMEASUNIT_VOLUME_CUBIC_KILOMETER,                // (11 << 8) + 1,  # 199 cubic-kilometer
    UAMEASUNIT_VOLUME_CUBIC_METER,                    // (11 << 8) + 9,  # 200 cubic-meter
    UAMEASUNIT_VOLUME_CUBIC_MILE,                     // (11 << 8) + 2,  # 201 cubic-mile
    UAMEASUNIT_VOLUME_CUBIC_YARD,                     // (11 << 8) + 12, # 202 cubic-yard
    UAMEASUNIT_VOLUME_CUP,                            // (11 << 8) + 18, # 203 cup
    UAMEASUNIT_VOLUME_CUP_IMPERIAL,                   // (11 << 8) + 34, # 204 cup-imperial
    UAMEASUNIT_VOLUME_CUP_JP,                         // (11 << 8) + 35, # 205 cup-jp
    UAMEASUNIT_VOLUME_CUP_METRIC,                     // (11 << 8) + 22, # 206 cup-metric
    UAMEASUNIT_VOLUME_DECILITER,                      // (11 << 8) + 5,  # 207 deciliter
    UAMEASUNIT_VOLUME_DESSERT_SPOON,                  // (11 << 8) + 27, # 208 dessert-spoon
    UAMEASUNIT_VOLUME_DESSERT_SPOON_IMPERIAL,         // (11 << 8) + 28, # 209 dessert-spoon-imperial
    UAMEASUNIT_VOLUME_DRAM,                           // (11 << 8) + 29, # 210 dram
    UAMEASUNIT_VOLUME_DROP,                           // (11 << 8) + 30, # 211 drop
    UAMEASUNIT_VOLUME_FLUID_OUNCE,                    // (11 << 8) + 17, # 212 fluid-ounce
    UAMEASUNIT_VOLUME_FLUID_OUNCE_IMPERIAL,           // (11 << 8) + 25, # 213 fluid-ounce-imperial
    UAMEASUNIT_VOLUME_FLUID_OUNCE_METRIC,             // (11 << 8) + 36, # 214 fluid-ounce-metric
    UAMEASUNIT_VOLUME_GALLON,                         // (11 << 8) + 21, # 215 gallon
    UAMEASUNIT_VOLUME_GALLON_IMPERIAL,                // (11 << 8) + 24, # 216 gallon-imperial
    UAMEASUNIT_VOLUME_HECTOLITER,                     // (11 << 8) + 6,  # 217 hectoliter
    UAMEASUNIT_VOLUME_JIGGER,                         // (11 << 8) + 31, # 218 jigger
    UAMEASUNIT_VOLUME_KOKU,                           // (11 << 8) + 37, # 219 koku
    UAMEASUNIT_VOLUME_KOSAJI,                         // (11 << 8) + 38, # 220 kosaji
    UAMEASUNIT_VOLUME_LITER,                          // (11 << 8) + 0,  # 221 liter
    UAMEASUNIT_VOLUME_MEGALITER,                      // (11 << 8) + 7,  # 222 megaliter
    UAMEASUNIT_VOLUME_MILLILITER,                     // (11 << 8) + 3,  # 223 milliliter
    UAMEASUNIT_VOLUME_OSAJI,                          // (11 << 8) + 39, # 224 osaji
    UAMEASUNIT_VOLUME_PINCH,                          // (11 << 8) + 32, # 225 pinch
    UAMEASUNIT_VOLUME_PINT,                           // (11 << 8) + 19, # 226 pint
    UAMEASUNIT_VOLUME_PINT_IMPERIAL,                  // (11 << 8) + 40, # 227 pint-imperial
    UAMEASUNIT_VOLUME_PINT_METRIC,                    // (11 << 8) + 23, # 228 pint-metric
    UAMEASUNIT_VOLUME_QUART,                          // (11 << 8) + 20, # 229 quart
    UAMEASUNIT_VOLUME_QUART_IMPERIAL,                 // (11 << 8) + 33, # 230 quart-imperial
    UAMEASUNIT_VOLUME_SAI,                            // (11 << 8) + 41, # 231 sai
    UAMEASUNIT_VOLUME_SHAKU,                          // (11 << 8) + 42, # 232 shaku
    UAMEASUNIT_VOLUME_TABLESPOON,                     // (11 << 8) + 16, # 233 tablespoon
    UAMEASUNIT_VOLUME_TEASPOON,                       // (11 << 8) + 15, # 234 teaspoon
    UAMEASUNIT_VOLUME_TO_JP,                          // (11 << 8) + 43, # 235 to-jp
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
