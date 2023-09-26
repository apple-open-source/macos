/*
*****************************************************************************************
* Copyright (C) 2020 Apple Inc. All Rights Reserved.
*****************************************************************************************
*/

#ifndef UAMEASUREUNIT_H
#define UAMEASUREUNIT_H

#if !UCONFIG_NO_FORMATTING
#ifndef U_HIDE_DRAFT_API

/**
 * Measurement units
 * @draft ICU 54
 */
typedef enum UAMeasureUnit {
    UAMEASUNIT_ACCELERATION_G_FORCE = (0 << 8) + 0,
    UAMEASUNIT_ACCELERATION_METER_PER_SECOND_SQUARED = (0 << 8) + 1, // (CLDR 26, ICU-541)
    //
    UAMEASUNIT_ANGLE_DEGREE         = (1 << 8) + 0,
    UAMEASUNIT_ANGLE_ARC_MINUTE     = (1 << 8) + 1,
    UAMEASUNIT_ANGLE_ARC_SECOND     = (1 << 8) + 2,
    UAMEASUNIT_ANGLE_RADIAN         = (1 << 8) + 3,     // (CLDR 26, ICU-541)
    UAMEASUNIT_ANGLE_REVOLUTION     = (1 << 8) + 4,     // (CLDR 28, ICU-561.3)
    //
    UAMEASUNIT_AREA_SQUARE_METER     = (2 << 8) + 0,
    UAMEASUNIT_AREA_SQUARE_KILOMETER = (2 << 8) + 1,
    UAMEASUNIT_AREA_SQUARE_FOOT      = (2 << 8) + 2,
    UAMEASUNIT_AREA_SQUARE_MILE      = (2 << 8) + 3,
    UAMEASUNIT_AREA_ACRE             = (2 << 8) + 4,
    UAMEASUNIT_AREA_HECTARE          = (2 << 8) + 5,
    UAMEASUNIT_AREA_SQUARE_CENTIMETER = (2 << 8) + 6,   // (CLDR 26, ICU-541)
    UAMEASUNIT_AREA_SQUARE_INCH      = (2 << 8) + 7,    // (CLDR 26, ICU-541)
    UAMEASUNIT_AREA_SQUARE_YARD      = (2 << 8) + 8,    // (CLDR 26, ICU-541)
    UAMEASUNIT_AREA_DUNAM            = (2 << 8) + 9,    // (CLDR 35, ICU-641)
    //
    // (3 reserved for currency, handled separately)
    //
    UAMEASUNIT_DURATION_YEAR        = (4 << 8) + 0,
    UAMEASUNIT_DURATION_MONTH       = (4 << 8) + 1,
    UAMEASUNIT_DURATION_WEEK        = (4 << 8) + 2,
    UAMEASUNIT_DURATION_DAY         = (4 << 8) + 3,
    UAMEASUNIT_DURATION_HOUR        = (4 << 8) + 4,
    UAMEASUNIT_DURATION_MINUTE      = (4 << 8) + 5,
    UAMEASUNIT_DURATION_SECOND      = (4 << 8) + 6,
    UAMEASUNIT_DURATION_MILLISECOND = (4 << 8) + 7,
    UAMEASUNIT_DURATION_MICROSECOND = (4 << 8) + 8,     // (CLDR 26, ICU-541)
    UAMEASUNIT_DURATION_NANOSECOND  = (4 << 8) + 9,     // (CLDR 26, ICU-541)
    UAMEASUNIT_DURATION_CENTURY     = (4 << 8) + 10,    // (CLDR 28, ICU-561.3)
    UAMEASUNIT_DURATION_YEAR_PERSON = (4 << 8) + 11,    // (CLDR 35, ICU-641)
    UAMEASUNIT_DURATION_MONTH_PERSON = (4 << 8) + 12,   // (CLDR 35, ICU-641)
    UAMEASUNIT_DURATION_WEEK_PERSON = (4 << 8) + 13,    // (CLDR 35, ICU-641)
    UAMEASUNIT_DURATION_DAY_PERSON  = (4 << 8) + 14,    // (CLDR 35, ICU-641)
    UAMEASUNIT_DURATION_DECADE      = (4 << 8) + 15,    // (CLDR 36, ICU-661)
    UAMEASUNIT_DURATION_QUARTER     = (4 << 8) + 16,    // (CLDR 42, ICU-721)
    //
    UAMEASUNIT_LENGTH_METER         = (5 << 8) + 0,
    UAMEASUNIT_LENGTH_CENTIMETER    = (5 << 8) + 1,
    UAMEASUNIT_LENGTH_KILOMETER     = (5 << 8) + 2,
    UAMEASUNIT_LENGTH_MILLIMETER    = (5 << 8) + 3,
    UAMEASUNIT_LENGTH_PICOMETER     = (5 << 8) + 4,
    UAMEASUNIT_LENGTH_FOOT          = (5 << 8) + 5,
    UAMEASUNIT_LENGTH_INCH          = (5 << 8) + 6,
    UAMEASUNIT_LENGTH_MILE          = (5 << 8) + 7,
    UAMEASUNIT_LENGTH_YARD          = (5 << 8) + 8,
    UAMEASUNIT_LENGTH_LIGHT_YEAR    = (5 << 8) + 9,
    UAMEASUNIT_LENGTH_DECIMETER     = (5 << 8) + 10,    // (CLDR 26, ICU-541)
    UAMEASUNIT_LENGTH_MICROMETER    = (5 << 8) + 11,    // (CLDR 26, ICU-541)
    UAMEASUNIT_LENGTH_NANOMETER     = (5 << 8) + 12,    // (CLDR 26, ICU-541)
    UAMEASUNIT_LENGTH_NAUTICAL_MILE = (5 << 8) + 13,    // (CLDR 26, ICU-541)
    UAMEASUNIT_LENGTH_FATHOM        = (5 << 8) + 14,    // (CLDR 26, ICU-541)
    UAMEASUNIT_LENGTH_FURLONG       = (5 << 8) + 15,    // (CLDR 26, ICU-541)
    UAMEASUNIT_LENGTH_ASTRONOMICAL_UNIT = (5 << 8) + 16, // (CLDR 26, ICU-541)
    UAMEASUNIT_LENGTH_PARSEC        = (5 << 8) + 17,    // (CLDR 26, ICU-541)
    UAMEASUNIT_LENGTH_MILE_SCANDINAVIAN = (5 << 8) + 18, // (CLDR 28, ICU-561.3)
    UAMEASUNIT_LENGTH_POINT         = (5 << 8) + 19,    // (CLDR 31, ICU-590)
    UAMEASUNIT_LENGTH_SOLAR_RADIUS  = (5 << 8) + 20,    // (CLDR 35, ICU-641)
    UAMEASUNIT_LENGTH_EARTH_RADIUS  = (5 << 8) + 21,    // (CLDR 38, ICU-681)
    //
    UAMEASUNIT_MASS_GRAM            = (6 << 8) + 0,
    UAMEASUNIT_MASS_KILOGRAM        = (6 << 8) + 1,
    UAMEASUNIT_MASS_OUNCE           = (6 << 8) + 2,
    UAMEASUNIT_MASS_POUND           = (6 << 8) + 3,
    UAMEASUNIT_MASS_STONE           = (6 << 8) + 4,     // = 14 pounds / 6.35 kg, abbr "st", used in UK/Ireland for body weight (CLDR 26)
    UAMEASUNIT_MASS_MICROGRAM       = (6 << 8) + 5,     // (CLDR 26, ICU-541)
    UAMEASUNIT_MASS_MILLIGRAM       = (6 << 8) + 6,     // (CLDR 26, ICU-541)
    UAMEASUNIT_MASS_METRIC_TON      = (6 << 8) + 7,     // obsolete name, see MASS_TONNE (CLDR 26, ICU-541)
    UAMEASUNIT_MASS_TON             = (6 << 8) + 8,     // = "short ton", U.S. ton (CLDR 26, ICU-541)
    UAMEASUNIT_MASS_CARAT           = (6 << 8) + 9,     // (CLDR 26, ICU-541)
    UAMEASUNIT_MASS_OUNCE_TROY      = (6 << 8) + 10,    // (CLDR 26, ICU-541)
    UAMEASUNIT_MASS_DALTON          = (6 << 8) + 11,    // (CLDR 35, ICU-641)
    UAMEASUNIT_MASS_EARTH_MASS      = (6 << 8) + 12,    // (CLDR 35, ICU-641)
    UAMEASUNIT_MASS_SOLAR_MASS      = (6 << 8) + 13,    // (CLDR 35, ICU-641)
    UAMEASUNIT_MASS_GRAIN           = (6 << 8) + 14,    // (CLDR 38, ICU-681)
    UAMEASUNIT_MASS_TONNE           = UAMEASUNIT_MASS_METRIC_TON, // new name (CLDR 42, ICU-721)
    //
    UAMEASUNIT_POWER_WATT           = (7 << 8) + 0,
    UAMEASUNIT_POWER_KILOWATT       = (7 << 8) + 1,
    UAMEASUNIT_POWER_HORSEPOWER     = (7 << 8) + 2,
    UAMEASUNIT_POWER_MILLIWATT      = (7 << 8) + 3,     // (CLDR 26, ICU-541)
    UAMEASUNIT_POWER_MEGAWATT       = (7 << 8) + 4,     // (CLDR 26, ICU-541)
    UAMEASUNIT_POWER_GIGAWATT       = (7 << 8) + 5,     // (CLDR 26, ICU-541)
    //
    UAMEASUNIT_PRESSURE_HECTOPASCAL = (8 << 8) + 0,
    UAMEASUNIT_PRESSURE_INCH_HG     = (8 << 8) + 1,
    UAMEASUNIT_PRESSURE_MILLIBAR    = (8 << 8) + 2,
    UAMEASUNIT_PRESSURE_MILLIMETER_OF_MERCURY = (8 << 8) + 3, // (CLDR 26, ICU-541)
    UAMEASUNIT_PRESSURE_POUND_PER_SQUARE_INCH = (8 << 8) + 4, // (CLDR 26, ICU-541)
    UAMEASUNIT_PRESSURE_ATMOSPHERE      = (8 << 8) + 5,     // (CLDR 34, ICU-631)
    UAMEASUNIT_PRESSURE_KILOPASCAL      = (8 << 8) + 6,     // (CLDR 35, ICU-641)
    UAMEASUNIT_PRESSURE_MEGAPASCAL      = (8 << 8) + 7,     // (CLDR 35, ICU-641)
    UAMEASUNIT_PRESSURE_PASCAL          = (8 << 8) + 8,     // (CLDR 36, ICU-661)
    UAMEASUNIT_PRESSURE_BAR             = (8 << 8) + 9,     // (CLDR 36, ICU-661)
    //
    UAMEASUNIT_SPEED_METER_PER_SECOND   = (9 << 8) + 0,
    UAMEASUNIT_SPEED_KILOMETER_PER_HOUR = (9 << 8) + 1,
    UAMEASUNIT_SPEED_MILE_PER_HOUR      = (9 << 8) + 2,
    UAMEASUNIT_SPEED_KNOT               = (9 << 8) + 3,     // (CLDR 28, ICU-561.3)
    //
    UAMEASUNIT_TEMPERATURE_CELSIUS      = (10 << 8) + 0,
    UAMEASUNIT_TEMPERATURE_FAHRENHEIT   = (10 << 8) + 1,
    UAMEASUNIT_TEMPERATURE_KELVIN       = (10 << 8) + 2,    // (CLDR 26, ICU-541)
    UAMEASUNIT_TEMPERATURE_GENERIC      = (10 << 8) + 3,    // (CLDR 27, ICU-550.2)
    //
    UAMEASUNIT_VOLUME_LITER             = (11 << 8) + 0,
    UAMEASUNIT_VOLUME_CUBIC_KILOMETER   = (11 << 8) + 1,
    UAMEASUNIT_VOLUME_CUBIC_MILE        = (11 << 8) + 2,
    UAMEASUNIT_VOLUME_MILLILITER        = (11 << 8) + 3,    // (CLDR 26, ICU-541)
    UAMEASUNIT_VOLUME_CENTILITER        = (11 << 8) + 4,    // (CLDR 26, ICU-541)
    UAMEASUNIT_VOLUME_DECILITER         = (11 << 8) + 5,    // (CLDR 26, ICU-541)
    UAMEASUNIT_VOLUME_HECTOLITER        = (11 << 8) + 6,    // (CLDR 26, ICU-541)
    UAMEASUNIT_VOLUME_MEGALITER         = (11 << 8) + 7,    // (CLDR 26, ICU-541)
    UAMEASUNIT_VOLUME_CUBIC_CENTIMETER  = (11 << 8) + 8,    // (CLDR 26, ICU-541)
    UAMEASUNIT_VOLUME_CUBIC_METER       = (11 << 8) + 9,    // (CLDR 26, ICU-541)
    UAMEASUNIT_VOLUME_CUBIC_INCH        = (11 << 8) + 10,   // (CLDR 26, ICU-541)
    UAMEASUNIT_VOLUME_CUBIC_FOOT        = (11 << 8) + 11,   // (CLDR 26, ICU-541)
    UAMEASUNIT_VOLUME_CUBIC_YARD        = (11 << 8) + 12,   // (CLDR 26, ICU-541)
    UAMEASUNIT_VOLUME_ACRE_FOOT         = (11 << 8) + 13,   // (CLDR 26, ICU-541)
    UAMEASUNIT_VOLUME_BUSHEL            = (11 << 8) + 14,   // (CLDR 26, ICU-541)
    UAMEASUNIT_VOLUME_TEASPOON          = (11 << 8) + 15,   // (CLDR 26, ICU-541)
    UAMEASUNIT_VOLUME_TABLESPOON        = (11 << 8) + 16,   // (CLDR 26, ICU-541)
    UAMEASUNIT_VOLUME_FLUID_OUNCE       = (11 << 8) + 17,   // (CLDR 26, ICU-541)
    UAMEASUNIT_VOLUME_CUP               = (11 << 8) + 18,   // (CLDR 26, ICU-541)
    UAMEASUNIT_VOLUME_PINT              = (11 << 8) + 19,   // (CLDR 26, ICU-541)
    UAMEASUNIT_VOLUME_QUART             = (11 << 8) + 20,   // (CLDR 26, ICU-541)
    UAMEASUNIT_VOLUME_GALLON            = (11 << 8) + 21,   // (CLDR 26, ICU-541)
    UAMEASUNIT_VOLUME_CUP_METRIC        = (11 << 8) + 22,   // (CLDR 28, ICU-561.3)
    UAMEASUNIT_VOLUME_PINT_METRIC       = (11 << 8) + 23,   // (CLDR 28, ICU-561.3)
    UAMEASUNIT_VOLUME_GALLON_IMPERIAL   = (11 << 8) + 24,   // (CLDR 29, ICU-561.8+)
    UAMEASUNIT_VOLUME_FLUID_OUNCE_IMPERIAL = (11 << 8) + 25, // (CLDR 35, ICU-641)
    UAMEASUNIT_VOLUME_BARREL            = (11 << 8) + 26,   // (CLDR 35, ICU-641)
    UAMEASUNIT_VOLUME_DESSERT_SPOON     = (11 << 8) + 27,   // (CLDR 38, ICU-681)
    UAMEASUNIT_VOLUME_DESSERT_SPOON_IMPERIAL = (11 << 8) + 28, // (CLDR 38, ICU-681)
    UAMEASUNIT_VOLUME_DRAM              = (11 << 8) + 29,   // (CLDR 38, ICU-681)
    UAMEASUNIT_VOLUME_DROP              = (11 << 8) + 30,   // (CLDR 38, ICU-681)
    UAMEASUNIT_VOLUME_JIGGER            = (11 << 8) + 31,   // (CLDR 38, ICU-681)
    UAMEASUNIT_VOLUME_PINCH             = (11 << 8) + 32,   // (CLDR 38, ICU-681)
    UAMEASUNIT_VOLUME_QUART_IMPERIAL    = (11 << 8) + 33,   // (CLDR 38, ICU-681)
    //
    // new categories/values in CLDR 26
    //
    UAMEASUNIT_ENERGY_JOULE             = (12 << 8) + 2,
    UAMEASUNIT_ENERGY_KILOJOULE         = (12 << 8) + 4,
    UAMEASUNIT_ENERGY_CALORIE           = (12 << 8) + 0,    // chemistry "calories", abbr "cal"
    UAMEASUNIT_ENERGY_KILOCALORIE       = (12 << 8) + 3,    // kilocalories in general (chemistry, food), abbr "kcal"
    UAMEASUNIT_ENERGY_FOODCALORIE       = (12 << 8) + 1,    // kilocalories specifically for food; in US/UK/? "Calories" abbr "C", elsewhere same as "kcal"
    UAMEASUNIT_ENERGY_KILOWATT_HOUR     = (12 << 8) + 5,    // (ICU-541)
    UAMEASUNIT_ENERGY_ELECTRONVOLT      = (12 << 8) + 6,    // (CLDR 35, ICU-641)
    UAMEASUNIT_ENERGY_BRITISH_THERMAL_UNIT = (12 << 8) + 7, // (CLDR 35, ICU-641)
    UAMEASUNIT_ENERGY_THERM_US          = (12 << 8) + 8,    // (CLDR 36, ICU-661)
    //
    // new categories/values in CLDR 26 & ICU-541
    //
    UAMEASUNIT_CONSUMPTION_LITER_PER_KILOMETER = (13 << 8) + 0,
    UAMEASUNIT_CONSUMPTION_MILE_PER_GALLON     = (13 << 8) + 1,
    UAMEASUNIT_CONSUMPTION_LITER_PER_100_KILOMETERs = (13 << 8) + 2, // (CLDR 28, ICU-561.3)
    UAMEASUNIT_CONSUMPTION_MILE_PER_GALLON_IMPERIAL = (13 << 8) + 3, // (CLDR 29, ICU-561.8+)
    //
    UAMEASUNIT_DIGITAL_BIT              = (14 << 8) + 0,
    UAMEASUNIT_DIGITAL_BYTE             = (14 << 8) + 1,
    UAMEASUNIT_DIGITAL_GIGABIT          = (14 << 8) + 2,
    UAMEASUNIT_DIGITAL_GIGABYTE         = (14 << 8) + 3,
    UAMEASUNIT_DIGITAL_KILOBIT          = (14 << 8) + 4,
    UAMEASUNIT_DIGITAL_KILOBYTE         = (14 << 8) + 5,
    UAMEASUNIT_DIGITAL_MEGABIT          = (14 << 8) + 6,
    UAMEASUNIT_DIGITAL_MEGABYTE         = (14 << 8) + 7,
    UAMEASUNIT_DIGITAL_TERABIT          = (14 << 8) + 8,
    UAMEASUNIT_DIGITAL_TERABYTE         = (14 << 8) + 9,
    UAMEASUNIT_DIGITAL_PETABYTE         = (14 << 8) + 10,   // (CLDR 34, ICU-631)
    //
    UAMEASUNIT_ELECTRIC_AMPERE          = (15 << 8) + 0,
    UAMEASUNIT_ELECTRIC_MILLIAMPERE     = (15 << 8) + 1,
    UAMEASUNIT_ELECTRIC_OHM             = (15 << 8) + 2,
    UAMEASUNIT_ELECTRIC_VOLT            = (15 << 8) + 3,
    //
    UAMEASUNIT_FREQUENCY_HERTZ          = (16 << 8) + 0,
    UAMEASUNIT_FREQUENCY_KILOHERTZ      = (16 << 8) + 1,
    UAMEASUNIT_FREQUENCY_MEGAHERTZ      = (16 << 8) + 2,
    UAMEASUNIT_FREQUENCY_GIGAHERTZ      = (16 << 8) + 3,
    //
    UAMEASUNIT_LIGHT_LUX                = (17 << 8) + 0,
    UAMEASUNIT_LIGHT_SOLAR_LUMINOSITY   = (17 << 8) + 1,    // (CLDR 35, ICU-641)
    UAMEASUNIT_LIGHT_CANDELA            = (17 << 8) + 2,    // (CLDR 38, ICU 681)
    UAMEASUNIT_LIGHT_LUMEN              = (17 << 8) + 3,    // (CLDR 38, ICU-681)
    //
    // new categories/values in CLDR 29, ICU-561.8+
    //
    UAMEASUNIT_CONCENTRATION_KARAT      = (18 << 8) + 0,    // (CLDR 29, ICU-561.8+)
    UAMEASUNIT_CONCENTRATION_MILLIGRAM_PER_DECILITER = (18 << 8) + 1, // (CLDR 29, ICU-561.8+)
    UAMEASUNIT_CONCENTRATION_MILLIMOLE_PER_LITER = (18 << 8) + 2, // (CLDR 29, ICU-561.8+)
    UAMEASUNIT_CONCENTRATION_PART_PER_MILLION = (18 << 8) + 3, // (CLDR 29, ICU-561.8+)
    UAMEASUNIT_CONCENTRATION_PERCENT    = (18 << 8) + 4,    // (CLDR 34, ICU-631nn)
    UAMEASUNIT_CONCENTRATION_PERMILLE   = (18 << 8) + 5,    // (CLDR 34, ICU-631nn)
    UAMEASUNIT_CONCENTRATION_PERMYRIAD  = (18 << 8) + 6,    // (CLDR 35, ICU-641)
    UAMEASUNIT_CONCENTRATION_MOLE       = (18 << 8) + 7,    // (CLDR 35, ICU-641)
    UAMEASUNIT_CONCENTRATION_ITEM       = (18 << 8) + 8,    // (CLDR 40)
    UAMEASUNIT_CONCENTRATION_MILLIGRAM_OFGLUCOSE_PER_DECILITER = (18 << 8) + 9, // (CLDR 40)
    //
    // new categories/values in CLDR 35, ICU-641+
    //
    UAMEASUNIT_FORCE_NEWTON             = (19 << 8) + 0,    // (CLDR 35, ICU-641)
    UAMEASUNIT_FORCE_POUND_FORCE        = (19 << 8) + 1,    // (CLDR 35, ICU-641)
    UAMEASUNIT_FORCE_KILOWATT_HOUR_PER_100_KILOMETER = (19 << 8) + 2, // (CLDR 40)
    //
    UAMEASUNIT_TORQUE_NEWTON_METER      = (20 << 8) + 0,    // (CLDR 35, ICU-641)
    UAMEASUNIT_TORQUE_POUND_FOOT        = (20 << 8) + 1,    // (CLDR 35, ICU-641)
    //
    // new categories/values in CLDR 36, ICU-661+
    //
    UAMEASUNIT_GRAPHICS_EM              = (21 << 8) + 0,    // (CLDR 36, ICU-661)
    UAMEASUNIT_GRAPHICS_PIXEL           = (21 << 8) + 1,    // (CLDR 36, ICU-661)
    UAMEASUNIT_GRAPHICS_MEGAPIXEL       = (21 << 8) + 2,    // (CLDR 36, ICU-661)
    UAMEASUNIT_GRAPHICS_PIXEL_PER_CENTIMETER = (21 << 8) + 3, // (CLDR 36, ICU-661)
    UAMEASUNIT_GRAPHICS_PIXEL_PER_INCH  = (21 << 8) + 4,    // (CLDR 36, ICU-661)
    UAMEASUNIT_GRAPHICS_DOT_PER_CENTIMETER   = (21 << 8) + 5, // (CLDR 36, ICU-661)
    UAMEASUNIT_GRAPHICS_DOT_PER_INCH    = (21 << 8) + 6,    // (CLDR 36, ICU-661)
    UAMEASUNIT_GRAPHICS_DOT             = (21 << 8) + 7,    // (CLDR 38, ICU-681)
    //
} UAMeasureUnit;

#endif /* U_HIDE_DRAFT_API */
#endif /* #if !UCONFIG_NO_FORMATTING */

#endif /* #ifndef UAMEASUREFORMAT_H */
