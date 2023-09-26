// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*****************************************************************************************
* Copyright (C) 2010-2011, International Business Machines
* Corporation and others. All Rights Reserved.
*****************************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/udateintervalformat.h"
#include "unicode/dtitvfmt.h"
#include "unicode/dtintrv.h"
#include "unicode/localpointer.h"
#include "unicode/timezone.h"
#include "unicode/locid.h"
#include "unicode/unistr.h"
#include "unicode/udisplaycontext.h"
#include "formattedval_impl.h"

U_NAMESPACE_USE


// Magic number: FDIV in ASCII
UPRV_FORMATTED_VALUE_CAPI_AUTO_IMPL(
    FormattedDateInterval,
    UFormattedDateInterval,
    UFormattedDateIntervalImpl,
    UFormattedDateIntervalApiHelper,
    udtitvfmt,
    0x46444956)


U_CAPI UDateIntervalFormat* U_EXPORT2
udtitvfmt_open(const char*  locale,
               const UChar* skeleton,
               int32_t      skeletonLength,
               const UChar* tzID,
               int32_t      tzIDLength,
               UErrorCode*  status)
{
    if (U_FAILURE(*status)) {
        return NULL;
    }
    if ((skeleton == NULL ? skeletonLength != 0 : skeletonLength < -1) ||
        (tzID == NULL ? tzIDLength != 0 : tzIDLength < -1)
    ) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }
    UnicodeString skel((UBool)(skeletonLength == -1), skeleton, skeletonLength);
    LocalPointer<DateIntervalFormat> formatter(
            DateIntervalFormat::createInstance(skel, Locale(locale), *status));
    if (U_FAILURE(*status)) {
        return NULL;
    }
    if(tzID != 0) {
        TimeZone *zone = TimeZone::createTimeZone(UnicodeString((UBool)(tzIDLength == -1), tzID, tzIDLength));
        if(zone == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
            return NULL;
        }
        formatter->adoptTimeZone(zone);
    }
    return (UDateIntervalFormat*)formatter.orphan();
}


U_CAPI void U_EXPORT2
udtitvfmt_close(UDateIntervalFormat *formatter)
{
    delete (DateIntervalFormat*)formatter;
}


U_CAPI int32_t U_EXPORT2
udtitvfmt_format(const UDateIntervalFormat* formatter,
                 UDate           fromDate,
                 UDate           toDate,
                 UChar*          result,
                 int32_t         resultCapacity,
                 UFieldPosition* position,
                 UErrorCode*     status)
{
    if (U_FAILURE(*status)) {
        return -1;
    }
    if (result == NULL ? resultCapacity != 0 : resultCapacity < 0) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    UnicodeString res;
    if (result != NULL) {
        // NULL destination for pure preflighting: empty dummy string
        // otherwise, alias the destination buffer (copied from udat_format)
        res.setTo(result, 0, resultCapacity);
    }
    FieldPosition fp;
    if (position != 0) {
        fp.setField(position->field);
    }

    DateInterval interval = DateInterval(fromDate,toDate);
    ((const DateIntervalFormat*)formatter)->format( &interval, res, fp, *status );
    if (U_FAILURE(*status)) {
        return -1;
    }
    if (position != 0) {
        position->beginIndex = fp.getBeginIndex();
        position->endIndex = fp.getEndIndex();
    }

    return res.extract(result, resultCapacity, *status);
}

#if APPLE_ICU_CHANGES
// rdar://
U_CAPI void U_EXPORT2
udtitvfmt_setAttribute(UDateIntervalFormat*             formatter,
                      UDateIntervalFormatAttribute      attr,
                      UDateIntervalFormatAttributeValue value,
                      UErrorCode*                       status)
{
    if (U_FAILURE(*status)) {
        return;
    }
    ((DateIntervalFormat*)formatter)->setAttribute( attr, value, *status );
}
#endif  // APPLE_ICU_CHANGES

U_CAPI void U_EXPORT2
udtitvfmt_formatToResult(
                const UDateIntervalFormat* formatter,
                UDate           fromDate,
                UDate           toDate,
                UFormattedDateInterval* result,
                UErrorCode*     status) {
    if (U_FAILURE(*status)) {
        return;
    }
    auto* resultImpl = UFormattedDateIntervalApiHelper::validate(result, *status);
    DateInterval interval = DateInterval(fromDate,toDate);
    if (resultImpl != nullptr) {
        resultImpl->fImpl = reinterpret_cast<const DateIntervalFormat*>(formatter)
            ->formatToValue(interval, *status);
    }
}

#if APPLE_ICU_CHANGES
// rdar://
// Apple-specific @stable 67 stub
U_CAPI void U_EXPORT2
udtitvfmt_formatToResultA(
                const UDateIntervalFormat* formatter,
                UDate           fromDate,
                UDate           toDate,
                UFormattedDateInterval* result,
                UErrorCode*     status) {
    udtitvfmt_formatToResult(formatter, fromDate, toDate, result, status);
}
#endif  // APPLE_ICU_CHANGES

U_CAPI void U_EXPORT2
udtitvfmt_formatCalendarToResult(
                const UDateIntervalFormat* formatter,
                UCalendar*      fromCalendar,
                UCalendar*      toCalendar,
                UFormattedDateInterval* result,
                UErrorCode*     status) {
    if (U_FAILURE(*status)) {
        return;
    }
    auto* resultImpl = UFormattedDateIntervalApiHelper::validate(result, *status);
    if (resultImpl != nullptr) {
        resultImpl->fImpl = reinterpret_cast<const DateIntervalFormat*>(formatter)
            ->formatToValue(*(Calendar *)fromCalendar, *(Calendar *)toCalendar, *status);
    }
}

#if APPLE_ICU_CHANGES
// rdar://
// Apple-specific @stable 67 stub
U_CAPI void U_EXPORT2
udtitvfmt_formatCalendarToResultA(
                const UDateIntervalFormat* formatter,
                UCalendar*      fromCalendar,
                UCalendar*      toCalendar,
                UFormattedDateInterval* result,
                UErrorCode*     status) {
    udtitvfmt_formatCalendarToResult(formatter, fromCalendar, toCalendar, result, status);
}
#endif  // APPLE_ICU_CHANGES

U_CAPI void U_EXPORT2
udtitvfmt_setContext(UDateIntervalFormat* formatter,
                     UDisplayContext value,
                     UErrorCode* status) {
    if (U_FAILURE(*status)) {
        return;
    }
    reinterpret_cast<DateIntervalFormat*>(formatter)->setContext( value, *status );
}

U_CAPI UDisplayContext U_EXPORT2
udtitvfmt_getContext(const UDateIntervalFormat* formatter,
                     UDisplayContextType type,
                     UErrorCode* status) {
    if (U_FAILURE(*status)) {
        return (UDisplayContext)0;
    }
    return reinterpret_cast<const DateIntervalFormat*>(formatter)->getContext( type, *status );
}

#if APPLE_ICU_CHANGES
// rdar://
U_CAPI void U_EXPORT2
udtitvfmt_getPatternString(const UDateIntervalFormat* formatter,
                           const UChar* skeleton,
                           UCalendarDateFields field,
                           UChar* result,
                           int32_t resultCapacity,
                           UErrorCode* status) {
    const DateIntervalFormat* dif = reinterpret_cast<const DateIntervalFormat*>(formatter);
    UnicodeString pattern;
    
    dif->getDateIntervalInfo()->getIntervalPattern(UnicodeString(skeleton), field, pattern, *status);
    pattern.extract(result, resultCapacity, *status);
}
#endif  // APPLE_ICU_CHANGES

#endif /* #if !UCONFIG_NO_FORMATTING */
