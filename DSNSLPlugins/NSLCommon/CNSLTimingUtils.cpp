/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
/*
 *  CNSLTimingUtils.cpp
 *  DSNSLPlugins
 *
 *  Created by Kevin Arnold on Wed Aug 06 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 */
#include <mach/mach_time.h>	// for dsTimeStamp
#include <syslog.h>

#include "CNSLTimingUtils.h"

void SmartSleep( unsigned int microSeconds )
{
	// we want to use the select call with a timeout.  This is much more system friendly as it only
	// makes on the order of one context switch as opposed to system which makes around 6.
	struct timeval	tval;

	tval.tv_sec = microSeconds / 1000000;
	tval.tv_usec = microSeconds % 1000000;
	select(0, NULL, NULL, NULL, &tval);			// pass in zero descriptors, we are just interested in the timer
}

double dsTimestamp(void)
{
	static uint32_t	num		= 0;
	static uint32_t	denom	= 0;
	uint64_t		now;
	
	if (denom == 0) 
	{
		struct mach_timebase_info tbi;
		kern_return_t r;
		r = mach_timebase_info(&tbi);
		if (r != KERN_SUCCESS) 
		{
			syslog( LOG_ALERT, "Warning: mach_timebase_info FAILED! - error = %u\n", r);
			return 0;
		}
		else
		{
			num		= tbi.numer;
			denom	= tbi.denom;
		}
	}
	now = mach_absolute_time();
	
	return (double)(now * (double)num / denom / NSEC_PER_SEC);	// return seconds
//	return (double)(now * (double)num / denom / NSEC_PER_USEC);	// return microsecs
//	return (double)(now * (double)num / denom);	// return nanoseconds
}
