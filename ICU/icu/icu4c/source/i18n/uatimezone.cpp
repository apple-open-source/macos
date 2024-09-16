// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 *******************************************************************************
 * Copyright (C) 1996-2015, International Business Machines Corporation and
 * others. All Rights Reserved.
 *******************************************************************************
 */

#include "unicode/utypes.h"

#if APPLE_ICU_CHANGES
// rdar://125359625 (Provide access to ICU's C++ TimeZone API via C interface)

#include "unicode/uatimezone.h"
#include "unicode/timezone.h"
#include "unicode/uloc.h"
#include "unicode/tztrans.h"
#include "unicode/simpletz.h"

#include "cstring.h"
#include "iso8601cal.h"
#include "ustrenum.h"
#include "ulist.h"
#include "ulocimp.h"

U_NAMESPACE_USE

// copied from calendar.cpp
static TimeZone*
_createTimeZone(const char16_t* zoneID, int32_t len, UErrorCode* ec) {
    TimeZone* zone = nullptr;
    if (ec != nullptr && U_SUCCESS(*ec)) {
        // Note that if zoneID is invalid, we get back GMT. This odd
        // behavior is by design and goes back to the JDK. The only
        // failure we will see is a memory allocation failure.
        int32_t l = (len<0 ? u_strlen(zoneID) : len);
        UnicodeString zoneStrID;
        zoneStrID.setTo((UBool)(len < 0), zoneID, l); /* temporary read-only alias */
        zone = TimeZone::createTimeZone(zoneStrID);
        if (zone == nullptr) {
            *ec = U_MEMORY_ALLOCATION_ERROR;
        }
    }
    return zone;
}

U_CAPI UTimeZone*  U_EXPORT2
uatimezone_open(  const char16_t*  zoneID,
                  int32_t       len,
                  UErrorCode*   status)
{
    if (U_FAILURE(*status)) {
        return nullptr;
    }
    
    if (zoneID==nullptr) {
        if (status != nullptr) *status = U_ILLEGAL_ARGUMENT_ERROR;
        return nullptr;
    }
    
    TimeZone *zone = _createTimeZone(zoneID, len, status);
    
    if (U_FAILURE(*status)) {
        return nullptr;
    }
    
    return (UTimeZone*)zone;
}

U_CAPI void U_EXPORT2
uatimezone_close(UTimeZone *tz)
{
    if (tz != nullptr) {
        delete (TimeZone*) tz;
    }
}

U_CAPI void U_EXPORT2
uatimezone_getOffset(const UTimeZone *tz,
                     UDate           date,
                     UBool           local,
                     int32_t*        rawOffset,
                     int32_t*        dstOffset,
                     UErrorCode*     status)
{
    if(status == 0 || U_FAILURE(*status)) {
        return;
    }
    
    ((TimeZone *)tz)->getOffset(date, local, *rawOffset, *dstOffset, *status);
}

U_CAPI void U_EXPORT2 
uatimezone_getOffsetFromLocal(const UTimeZone      *tz,
                              UTimeZoneLocalOption nonExistingTimeOpt,
                              UTimeZoneLocalOption duplicatedTimeOpt,
                              UDate                date,
                              int32_t*             rawOffset,
                              int32_t*             dstOffset,
                              UErrorCode*          status)
{
    if (U_FAILURE(*status)) {
        return;
    }
    const BasicTimeZone * btz = dynamic_cast<const BasicTimeZone *>((TimeZone *)tz);
    if (btz == nullptr) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    btz->getOffsetFromLocal(
        date, nonExistingTimeOpt, duplicatedTimeOpt,
        *rawOffset, *dstOffset, *status);
}

U_CAPI int32_t U_EXPORT2
uatimezone_getDisplayName(const UTimeZone*          tz,
                          UTimeZoneDisplayNameType  type,
                          const char                *locale,
                          UChar*                    result,
                          int32_t                   resultLength,
                          UErrorCode*               status)
{

    if(U_FAILURE(*status)) return -1;

    UnicodeString id;
    if (!(result == nullptr && resultLength == 0)) {
        // Null destination for pure preflighting: empty dummy string
        // otherwise, alias the destination buffer
        id.setTo(result, 0, resultLength);
    }

    switch(type) {
        case UTIMEZONE_LONG:
            ((TimeZone *)tz)->getDisplayName(false, TimeZone::LONG, Locale(locale), id);
            break;
            
        case UTIMEZONE_SHORT:
            ((TimeZone *)tz)->getDisplayName(false, TimeZone::SHORT, Locale(locale), id);
            break;
            
        case UTIMEZONE_LONG_DST:
            ((TimeZone *)tz)->getDisplayName(true, TimeZone::LONG, Locale(locale), id);
            break;
            
        case UTIMEZONE_SHORT_DST:
            ((TimeZone *)tz)->getDisplayName(true, TimeZone::SHORT, Locale(locale), id);
            break;
    }

    return id.extract(result, resultLength, *status);
}

U_CAPI UBool U_EXPORT2
uatimezone_getTimeZoneTransitionDate(const UTimeZone*        tz,
                                    UDate                   date,
                                    UTimeZoneTransitionType type,
                                    UDate*                  transition,
                                    UErrorCode* status)
{
    if (U_FAILURE(*status)) {
        return false;
    }
    const BasicTimeZone * btz = dynamic_cast<const BasicTimeZone *>((TimeZone *)tz);
    if (btz != nullptr && U_SUCCESS(*status)) {
        TimeZoneTransition tzt;
        UBool inclusive = (type == UCAL_TZ_TRANSITION_NEXT_INCLUSIVE || type == UCAL_TZ_TRANSITION_PREVIOUS_INCLUSIVE);
        UBool result = (type == UCAL_TZ_TRANSITION_NEXT || type == UCAL_TZ_TRANSITION_NEXT_INCLUSIVE)?
                        btz->getNextTransition(date, inclusive, tzt):
                        btz->getPreviousTransition(date, inclusive, tzt);
        if (result) {
            *transition = tzt.getTime();
            return true;
        }
    }
    return false;
}

#endif  // APPLE_ICU_CHANGES
