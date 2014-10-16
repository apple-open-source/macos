/*
 * Copyright (c) 2000-2004,2006,2011-2012,2014 Apple Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


//
// Manage the Tower of Babel of CSSM dates and times
//
#include <security_cdsa_utilities/cssmdates.h>
#include <security_cdsa_utilities/cssmerrors.h>
#include <Security/cssm.h>
#include <string>


//
// A (private) PODwrapper for CFGregorianDate
//
struct Gregorian : public PodWrapper<Gregorian, CFGregorianDate> {
    Gregorian() { }
    
    Gregorian(int y, int m, int d, int h = 0, int min = 0, double sec = 0)
    {
        year = y; month = m; day = d;
        hour = h; minute = min; second = sec;
    }
    
    Gregorian(CFAbsoluteTime ref)
    { static_cast<CFGregorianDate &>(*this) = CFAbsoluteTimeGetGregorianDate(ref, NULL); }
    
    operator CFAbsoluteTime () const
    { return CFGregorianDateGetAbsoluteTime(*this, NULL); }
};


//
// The CssmDate PODwrapper
//
CssmDate::CssmDate(const char *y, const char *m, const char *d)
{
    assign(years(), 4, y);
    assign(months(), 2, m);
    assign(days(), 2, d);
}

CssmDate::CssmDate(int y, int m, int d)
{
    // internal format is "yyyymmdd" (no null termination)
    char str[9];
    if (8 != snprintf(str, 9, "%4.4d%2.2d%2.2d", y, m, d))
        CssmError::throwMe(CSSM_ERRCODE_UNKNOWN_FORMAT);
    memcpy(this, str, 8);
}
    
int CssmDate::year() const
{ return atoi(string(years(), 4).c_str()); }

int CssmDate::month() const
{ return atoi(string(months(), 2).c_str()); }

int CssmDate::day() const
{ return atoi(string(days(), 2).c_str()); }

// right-adjust fill 
void CssmDate::assign(char *dest, int width, const char *src)
{
    // pick last width characters of src at most
    size_t len = strlen(src);
    if (len > width)
        CssmError::throwMe(CSSM_ERRCODE_UNKNOWN_FORMAT);
    memset(dest, '0', width - len);
    memcpy(dest + width - len, src, len);
}


//
// CssmUniformDate core functions
//


//
// Uniform conversions with CFDateRef
//
CssmUniformDate::CssmUniformDate(CFDateRef ref)
{
    mTime = CFDateGetAbsoluteTime(ref);
}

CssmUniformDate::operator CFDateRef() const
{
    return CFDateCreate(NULL, mTime);
}


//
// Uniform conversions with CssmDates
//
CssmUniformDate::CssmUniformDate(const CssmDate &date)
{
    mTime = CFGregorianDateGetAbsoluteTime(Gregorian(date.year(), date.month(), date.day()),
        NULL);
}

CssmUniformDate::operator CssmDate () const
{
    Gregorian greg(mTime);
    return CssmDate(greg.year, greg.month, greg.day);
}


//
// Uniform conversions with CssmData (1999-06-30_15:05:39 form)
//
CssmUniformDate::CssmUniformDate(const CSSM_DATA &inData)
{
    const CssmData &data = CssmData::overlay(inData);
    if (data.length() != 19)
        CssmError::throwMe(CSSM_ERRCODE_UNKNOWN_FORMAT);
    setFromString(reinterpret_cast<const char *>(inData.Data), "%ld-%d-%d_%d:%d:%lf", 19);
}

void CssmUniformDate::convertTo(CssmOwnedData &data) const
{
    Gregorian greg(mTime);
    char str[20];
    if (19 != snprintf(str, 20, "%4.4d-%2.2d-%2.2d_%2.2d:%2.2d:%2.2d",
        int(greg.year), greg.month, greg.day, greg.hour, greg.minute, int(greg.second)))
        CssmError::throwMe(CSSM_ERRCODE_UNKNOWN_FORMAT);
    data = CssmData(str, 19);
}


//
// Uniform conversions with CSSM_TIMESTRING (19990630150539 form)
//
CssmUniformDate::CssmUniformDate(const char *src)
{
    setFromString(src, "%4ld%2d%2d%2d%2d%2lf", 14);
}

void CssmUniformDate::convertTo(char *dst, size_t length) const
{
    if (length < 14)
        CssmError::throwMe(CSSMERR_CSSM_BUFFER_TOO_SMALL);
    Gregorian greg(mTime);
    char str[15];
    if (14 != snprintf(str, 15, "%4.4d%2.2d%2.2d%2.2d%2.2d%2.2d",
        int(greg.year), greg.month, greg.day, greg.hour, greg.minute, int(greg.second)))
        CssmError::throwMe(CSSM_ERRCODE_UNKNOWN_FORMAT);
    memcpy(dst, str, length == 14 ? 14 : 15);	// null terminate if there's room
}


//
// Generalized parse-from-string setup
//
void CssmUniformDate::setFromString(const char *src, const char *format, size_t fieldWidth)
{
    // use a stack buffer
    char str[20];
    assert(fieldWidth < sizeof(str));
    
    // make a copy with proper null terminator
    memcpy(str, src, fieldWidth);
    str[fieldWidth] = '\0';

    // parse (with limited checks for bad field formats)
    long year;
    int month, day, hour, minute;
    double second;
    if (6 != sscanf(str, format,
        &year, &month, &day, &hour, &minute, &second))
        CssmError::throwMe(CSSM_ERRCODE_UNKNOWN_FORMAT);

    // success
    mTime = Gregorian((int)year, month, day, hour, minute, second);
}
