/*
 * Copyright (c) 2000-2023 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 * 
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/types.h>
#include <pexpert/pexpert.h>
#include <IOKit/IOLib.h>

#include "hfs.h"
#include "hfs_dbg.h"
#include "FileMgrInternal.h"

/* 
 * gTimeZone should only be used for HFS volumes!
 * It is initialized when an HFS volume is mounted.
 */
struct timezone gTimeZone = {8*60,1};

/*
 * GetTimeUTC - get the current GMT Mac OS time
 * Account for either classic time (in seconds since 1/1/1904)
 * or expanded time (BSD epoch)
 *
 * called by the Catalog Manager when creating/updating HFS Plus records
 */
u_int32_t GetTimeUTC(bool expanded)
{
	struct timeval tv;
	microtime(&tv);
	uint32_t mac_time = tv.tv_sec;
	if (!expanded) {
		mac_time += MAC_GMT_FACTOR;
	}

	return mac_time;
}


/*
 * UTCToLocal - convert from Mac OS GMT time to Mac OS local time.
 * This should only be called for HFS Standard (not for HFS+) in normal runtime.
 *
 * However this function *is* used in newfs_hfs to convert the UTC time to
 * local time for writing into the volume header.
 */
u_int32_t UTCToLocal(u_int32_t utcTime)
{
	u_int32_t ltime = utcTime;
	
	if (ltime != 0) {
		ltime -= (gTimeZone.tz_minuteswest * 60);
	/*
	 * We no longer do DST adjustments here since we don't
	 * know if time supplied needs adjustment!
	 *
	 * if (gTimeZone.tz_dsttime)
	 *     ltime += 3600;
	 */
	}
    return (ltime);
}

/*
 * to_bsd_time:
 * convert from Mac OS classic time (seconds since 1/1/1904)
 * to BSD time (seconds since 1/1/1970), depending on if
 * expanded times are in use.
 */
time_t to_bsd_time(u_int32_t hfs_time, bool expanded)
{
	u_int32_t gmt = hfs_time;

	if (expanded) {
		/*
		 * If expanded times are in use, then we are using
		 * BSD time as native. Do not convert it. It is assumed
		 * to be a non-negative value.
		 */
		return (time_t) gmt;
	}

	if (gmt > MAC_GMT_FACTOR) {
		gmt -= MAC_GMT_FACTOR;
	}
	else {
		gmt = 0;	/* don't let date go negative! */
	}

	return (time_t)gmt;
}

/*
 * to_hfs_time:
 * convert from BSD time (seconds since 1/1/1970)
 * to Mac OS classic time (seconds since 1/1/1904), depending
 * on if expanded times are in use.
 */
u_int32_t to_hfs_time(time_t bsd_time, bool expanded)
{
	bool negative = (bsd_time < 0);
	u_int32_t hfs_time = (u_int32_t)bsd_time;

	if (expanded) {
		/*
		 * If expanded times are in use, then the BSD time
		 * is native. Do not convert it with the Mac factor.
		 * In this mode, zero is legitimate (now implying 1/1/1970).
		 *
		 * In addition, clip the timestamp to 0, ensuring that we treat
		 * the value as an unsigned int32.
		 */
		if (negative) {
			hfs_time = 0;
		}
		return hfs_time;
	}

	/* don't adjust zero - treat as uninitialized */
	if (hfs_time != 0) {
		hfs_time += MAC_GMT_FACTOR;
	}

	return (hfs_time);
}

void
DebugStr(
	const char * debuggerMsg
	)
{
    kprintf ("*** Mac OS Debugging Message: %s\n", debuggerMsg);
#if DEBUG
	Debugger(debuggerMsg);
#endif
}
