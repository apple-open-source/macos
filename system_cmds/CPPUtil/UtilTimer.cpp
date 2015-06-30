//
//  UtilTimer.cpp
//  CPPUtil
//
//  Created by James McIlree on 10/9/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#include "CPPUtil.h"

BEGIN_UTIL_NAMESPACE

static mach_timebase_info_data_t timebase_info;

Timer::Timer(const char* message) : _message(message) {
	// C++ guarantees that static variable initialization is thread safe.
	// We don't actually care what the returned value is, we just want to init timebase_info
	// The pragma prevents spurious warnings.
	static kern_return_t blah = mach_timebase_info(&timebase_info);
#pragma unused(blah)

	_start = AbsTime::now(); // Do this after the initialization check.
}

Timer::~Timer()
{
	_end = AbsTime::now();
	printf("%s: %5.5f seconds\n", _message.c_str(), (double)(_end - _start).nano_time().value() / (double)NANOSECONDS_PER_SECOND);
}

END_UTIL_NAMESPACE
