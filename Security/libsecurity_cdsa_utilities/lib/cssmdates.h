/*
 * Copyright (c) 2000-2004,2006 Apple Computer, Inc. All Rights Reserved.
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
// Manage the Tower of Babel of CSSM dates and times.
//
#ifndef _H_CSSMDATES
#define _H_CSSMDATES

#include <security_utilities/utilities.h>
#include <security_cdsa_utilities/cssmdata.h>
#include <CoreFoundation/CFDate.h>


namespace Security {


//
// A PodWrapper for CSSM_DATE
//
class CssmDate : public PodWrapper<CssmDate, CSSM_DATE> {
public:
    CssmDate() { }
    CssmDate(const char *y, const char *m, const char *d);
    CssmDate(int y, int m, int d);
    
    const char *years() const	{ return reinterpret_cast<const char *>(Year); }
    const char *months() const	{ return reinterpret_cast<const char *>(Month); }
    const char *days() const	{ return reinterpret_cast<const char *>(Day); }
    char *years()				{ return reinterpret_cast<char *>(Year); }
    char *months()				{ return reinterpret_cast<char *>(Month); }
    char *days()				{ return reinterpret_cast<char *>(Day); }
    
    int year() const;
    int month() const;
    int day() const;
    
private:
    static void assign(char *dest, int width, const char *src);
};

inline bool operator == (const CSSM_DATE &d1, const CSSM_DATE &d2)
{ return !memcmp(&d1, &d2, sizeof(d1)); }

inline bool operator != (const CSSM_DATE &d1, const CSSM_DATE &d2)
{ return !memcmp(&d1, &d2, sizeof(d1)); }


//
// Yet another CSSM date/time format is CSSM_TIMESTRING. This is
// defined as "char *", just so you can't use the type system
// to keep things sane, so we can't really PodWrap it the usual way.
// What *were* they thinking?
// The format is allegedly "yyyymmddhhmmss", and the standard says
// nothing about trailing null characters.
//


//
// A unified date-and-time object.
// This is based on CFDate objects and converts to various CSSM
// inspired formats.
//
class CssmUniformDate {
public:
    CssmUniformDate()	{ }
    
    // convert to/from CFDateRef
    CssmUniformDate(CFDateRef ref);
    operator CFDateRef() const;
    
	// convert to/from CFAbsoluteTime
	CssmUniformDate(CFAbsoluteTime ct) : mTime(ct) { }
	operator CFAbsoluteTime() const { return mTime; }
	
    // convert to/from CSSM_DATE
    CssmUniformDate(const CssmDate &src);
    operator CssmDate () const;
    
    // convert to/from DATA format (1999-06-30_15:05:39 form)
    CssmUniformDate(const CSSM_DATA &src);
    void convertTo(CssmOwnedData &data) const;
    
    // convert to/from CSSM_TIMESTRING format (19990630150539)
    CssmUniformDate(const char *src);
    void convertTo(char *dest, size_t length) const;

    // native comparisons
    bool operator < (const CssmUniformDate &other) const	{ return mTime < other.mTime; }
    bool operator == (const CssmUniformDate &other) const	{ return mTime == other.mTime; }
    bool operator > (const CssmUniformDate &other) const	{ return mTime > other.mTime; }
    bool operator <= (const CssmUniformDate &other) const	{ return mTime <= other.mTime; }
    bool operator >= (const CssmUniformDate &other) const	{ return mTime >= other.mTime; }
    bool operator != (const CssmUniformDate &other) const	{ return mTime != other.mTime; }

private:
    void setFromString(const char *src, const char *format, size_t fieldLength);

private:
    CFAbsoluteTime mTime;
};


} // end namespace Security

#endif //_H_CSSMDATES
