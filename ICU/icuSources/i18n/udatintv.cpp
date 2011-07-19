/*
*****************************************************************************************
* Copyright (C) 2010 Apple Inc. All Rights Reserved.
*****************************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/udatintv.h"
#include "unicode/dtitvfmt.h"
#include "unicode/dtintrv.h"
#include "unicode/locid.h"
#include "unicode/unistr.h"

U_NAMESPACE_USE


U_CAPI UDateIntervalFormat* U_EXPORT2
udatintv_open(const char*  locale,
              const UChar* skeleton,
              int32_t      skeletonLength,
              UErrorCode*  status)
{
    if (U_FAILURE(*status)) {
        return 0;
    }
    UnicodeString skel((UBool)(skeletonLength == -1), skeleton, skeletonLength);
    return (UDateIntervalFormat*)DateIntervalFormat::createInstance(skel, Locale(locale), *status);
}


U_CAPI void U_EXPORT2
udatintv_close(UDateIntervalFormat *datintv)
{
    delete (DateIntervalFormat*)datintv;
}


U_INTERNAL int32_t U_EXPORT2
udatintv_format(const UDateIntervalFormat* datintv,
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
    UnicodeString res;
    if (!(result==NULL && resultCapacity==0)) {
        // NULL destination for pure preflighting: empty dummy string
        // otherwise, alias the destination buffer (copied from udat_format)
        res.setTo(result, 0, resultCapacity);
    }
    FieldPosition fp;
    if (position != 0) {
        fp.setField(position->field);
    }

    DateInterval interval = DateInterval(fromDate,toDate);
    ((const DateIntervalFormat*)datintv)->format( &interval, res, fp, *status );
    if (U_FAILURE(*status)) {
        return -1;
    }
    if (position != 0) {
        position->beginIndex = fp.getBeginIndex();
        position->endIndex = fp.getEndIndex();
    }

    return res.extract(result, resultCapacity, *status);
}


#endif /* #if !UCONFIG_NO_FORMATTING */
