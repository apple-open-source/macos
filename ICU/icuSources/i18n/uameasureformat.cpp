/*
*****************************************************************************************
* Copyright (C) 2014 Apple Inc. All Rights Reserved.
*****************************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

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

static MeasureUnit * createObjectForMeasureUnit(UAMeasureUnit unit, UErrorCode* status )
{
    MeasureUnit * munit = NULL;
    switch (unit) {
        case UAMEASUNIT_ACCELERATION_G_FORCE:   munit = MeasureUnit::createGForce(*status);      break;

        case UAMEASUNIT_ANGLE_DEGREE:           munit = MeasureUnit::createDegree(*status);      break;
        case UAMEASUNIT_ANGLE_ARC_MINUTE:       munit = MeasureUnit::createArcMinute(*status);   break;
        case UAMEASUNIT_ANGLE_ARC_SECOND:       munit = MeasureUnit::createArcSecond(*status);   break;

        case UAMEASUNIT_AREA_SQUARE_METER:      munit = MeasureUnit::createSquareMeter(*status);     break;
        case UAMEASUNIT_AREA_SQUARE_KILOMETER:  munit = MeasureUnit::createSquareKilometer(*status); break;
        case UAMEASUNIT_AREA_SQUARE_FOOT:       munit = MeasureUnit::createSquareFoot(*status);      break;
        case UAMEASUNIT_AREA_SQUARE_MILE:       munit = MeasureUnit::createSquareMile(*status);      break;
        case UAMEASUNIT_AREA_ACRE:              munit = MeasureUnit::createAcre(*status);            break;
        case UAMEASUNIT_AREA_HECTARE:           munit = MeasureUnit::createHectare(*status);         break;

        case UAMEASUNIT_DURATION_YEAR:          munit = MeasureUnit::createYear(*status);        break;
        case UAMEASUNIT_DURATION_MONTH:         munit = MeasureUnit::createMonth(*status);       break;
        case UAMEASUNIT_DURATION_WEEK:          munit = MeasureUnit::createDay(*status);         break;
        case UAMEASUNIT_DURATION_DAY:           munit = MeasureUnit::createWeek(*status);        break;
        case UAMEASUNIT_DURATION_HOUR:          munit = MeasureUnit::createHour(*status);        break;
        case UAMEASUNIT_DURATION_MINUTE:        munit = MeasureUnit::createMinute(*status);      break;
        case UAMEASUNIT_DURATION_SECOND:        munit = MeasureUnit::createSecond(*status);      break;
        case UAMEASUNIT_DURATION_MILLISECOND:   munit = MeasureUnit::createMillisecond(*status); break;

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

        case UAMEASUNIT_MASS_GRAM:              munit = MeasureUnit::createGram(*status);        break;
        case UAMEASUNIT_MASS_KILOGRAM:          munit = MeasureUnit::createKilogram(*status);    break;
        case UAMEASUNIT_MASS_OUNCE:             munit = MeasureUnit::createOunce(*status);       break;
        case UAMEASUNIT_MASS_POUND:             munit = MeasureUnit::createPound(*status);       break;
        case UAMEASUNIT_MASS_STONE:             munit = MeasureUnit::createStone(*status);       break;

        case UAMEASUNIT_POWER_WATT:             munit = MeasureUnit::createWatt(*status);        break;
        case UAMEASUNIT_POWER_KILOWATT:         munit = MeasureUnit::createKilowatt(*status);    break;
        case UAMEASUNIT_POWER_HORSEPOWER:       munit = MeasureUnit::createHorsepower(*status);  break;

        case UAMEASUNIT_PRESSURE_HECTOPASCAL:   munit = MeasureUnit::createHectopascal(*status); break;
        case UAMEASUNIT_PRESSURE_INCH_HG:       munit = MeasureUnit::createInchHg(*status);      break;
        case UAMEASUNIT_PRESSURE_MILLIBAR:      munit = MeasureUnit::createMillibar(*status);    break;

        case UAMEASUNIT_SPEED_METER_PER_SECOND:   munit = MeasureUnit::createMeterPerSecond(*status);   break;
        case UAMEASUNIT_SPEED_KILOMETER_PER_HOUR: munit = MeasureUnit::createKilometerPerHour(*status); break;
        case UAMEASUNIT_SPEED_MILE_PER_HOUR:      munit = MeasureUnit::createMilePerHour(*status);      break;

        case UAMEASUNIT_TEMPERATURE_CELSIUS:    munit = MeasureUnit::createCelsius(*status);     break;
        case UAMEASUNIT_TEMPERATURE_FAHRENHEIT: munit = MeasureUnit::createFahrenheit(*status);  break;

        case UAMEASUNIT_VOLUME_LITER:           munit = MeasureUnit::createLiter(*status);          break;
        case UAMEASUNIT_VOLUME_CUBIC_KILOMETER: munit = MeasureUnit::createCubicKilometer(*status); break;
        case UAMEASUNIT_VOLUME_CUBIC_MILE:      munit = MeasureUnit::createCubicMile(*status);      break;

        case UAMEASUNIT_ENERGY_JOULE:           munit = MeasureUnit::createJoule(*status);          break;
        case UAMEASUNIT_ENERGY_KILOJOULE:       munit = MeasureUnit::createKilojoule(*status);      break;
        case UAMEASUNIT_ENERGY_CALORIE:         munit = MeasureUnit::createCalorie(*status);        break;
        case UAMEASUNIT_ENERGY_KILOCALORIE:     munit = MeasureUnit::createKilocalorie(*status);    break;
        case UAMEASUNIT_ENERGY_FOODCALORIE:     munit = MeasureUnit::createFoodcalorie(*status);    break;

        default: *status = U_ILLEGAL_ARGUMENT_ERROR; break;
    }
    return munit;
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
    MeasureUnit * munit = createObjectForMeasureUnit(unit, status);
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
            MeasureUnit * munit = createObjectForMeasureUnit(measures[i].unit, status);
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
    FieldPosition pos(0);
    ((MeasureFormat*)measfmt)->formatMeasures(measureObjs, measureCount, res, pos, *status);
    for (i = 0; i < kMeasuresMax; i++) {
        delete measurePtrs[i];
    }
    return res.extract(result, resultCapacity, *status);
}


#endif /* #if !UCONFIG_NO_FORMATTING */
