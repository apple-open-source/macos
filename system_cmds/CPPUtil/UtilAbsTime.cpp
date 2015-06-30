//
//  UtilAbsTime.cpp
//  CPPUtil
//
//  Created by James McIlree on 4/14/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#include "CPPUtil.h"

BEGIN_UTIL_NAMESPACE

const AbsTime AbsTime::BEGINNING_OF_TIME = AbsTime(0ULL);
const AbsTime AbsTime::END_OF_TIME = AbsTime(UINT64_MAX);

AbsTime AbsTime::now() {
    return AbsTime(mach_absolute_time());
}

NanoTime AbsTime::nano_time() const {
    mach_timebase_info_data_t timebase_info;
    mach_timebase_info(&timebase_info);
    return NanoTime(_time * timebase_info.numer  / timebase_info.denom);
}

NanoTime AbsTime::nano_time(mach_timebase_info_data_t timebase_info) const {
    return NanoTime(_time * timebase_info.numer  / timebase_info.denom);
}

END_UTIL_NAMESPACE
