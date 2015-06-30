//
//  UtilNanoTime.cpp
//  CPPUtil
//
//  Created by James McIlree on 10/2/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#include "CPPUtil.h"

BEGIN_UTIL_NAMESPACE

AbsTime NanoTime::abs_time() const {
	mach_timebase_info_data_t timebase_info;
	mach_timebase_info(&timebase_info);
	return AbsTime(_time * timebase_info.denom / timebase_info.numer);
}

END_UTIL_NAMESPACE
