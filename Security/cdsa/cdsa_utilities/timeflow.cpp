/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


//
// timeflow - abstract view of the flow of time
//
#include "timeflow.h"
#include <sys/time.h>
#include <math.h>


namespace Security {
namespace Time {


//
// Get "now"
//
Absolute now()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + double(tv.tv_usec) / 1E6;
}


//
// OOL Conversions
//
Absolute::Absolute(const struct timeval &tv)
{ mValue = tv.tv_sec + double(tv.tv_usec) / 1E6; }

Absolute::Absolute(const struct timespec &tv)
{ mValue = tv.tv_sec + double(tv.tv_nsec) / 1E9; }

Absolute::operator struct timeval () const
{
    struct timeval tv;
    if (mValue > LONG_MAX) {
        tv.tv_sec = LONG_MAX;
        tv.tv_usec = 0;
    } else {
        tv.tv_sec = int32_t(mValue);
        double intPart;
        tv.tv_usec = int32_t(modf(mValue, &intPart) * 1E6);
    }
    return tv;
}

Absolute::operator struct timespec () const
{
    struct timespec ts;
    if (mValue > LONG_MAX) {
        ts.tv_sec = LONG_MAX;
        ts.tv_nsec = 0;
    } else {
        ts.tv_sec = time_t(mValue);
        double intPart;
        ts.tv_nsec = int32_t(modf(mValue, &intPart) * 1E9);
    }
    return ts;
}

struct timeval Interval::timevalInterval() const
{
    struct timeval tv;
    if (mValue > LONG_MAX) {
        tv.tv_sec = LONG_MAX;
        tv.tv_usec = 0;
    } else if (mValue < 0) {
        tv.tv_sec = tv.tv_usec = 0;
    } else {
        tv.tv_sec = int32_t(mValue);
        double intPart;
        tv.tv_usec = int32_t(modf(mValue, &intPart) * 1E6);
    }
    return tv;
}


//
// Estimate resolution at given time.
//
// BSD select(2) has theoretical microsecond resolution, but the underlying 
// Mach system deals with milliseconds, so we report that conservatively.
// Sometime in the future when the sun is near collapse, residual resolution
// of a double will drop under 1E-3, but we won't worry about that just yet.
//
Interval resolution(Absolute)
{
    return 0.001;
}


}	// end namespace Time
}	// end namespace Security
