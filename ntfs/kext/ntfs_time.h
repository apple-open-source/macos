/*
 * ntfs_time.h - NTFS time conversion functions for the NTFS kernel driver.
 *
 * Copyright (c) 2006-2008 Anton Altaparmakov.  All Rights Reserved.
 * Portions Copyright (c) 2006-2008 Apple Inc.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution. 
 * 3. Neither the name of Apple Inc. ("Apple") nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ALTERNATIVELY, provided that this notice and licensing terms are retained in
 * full, this file may be redistributed and/or modified under the terms of the
 * GNU General Public License (GPL) Version 2, in which case the provisions of
 * that version of the GPL will apply to you instead of the license terms
 * above.  You can obtain a copy of the GPL Version 2 at
 * http://developer.apple.com/opensource/licenses/gpl-2.txt.
 */

#ifndef _OSX_NTFS_TIME_H
#define _OSX_NTFS_TIME_H

#include <sys/time.h>

#include "ntfs_endian.h"
#include "ntfs_types.h"

#define NTFS_TIME_OFFSET ((s64)(369 * 365 + 89) * 24 * 3600 * 10000000)

/**
 * utc2ntfs - convert OS X time to NTFS time
 * @ts:		OS X UTC time to convert to NTFS (little endian) time
 *
 * Convert the OS X UTC time @ts to its corresponding NTFS time and return that
 * in little endian format.
 *
 * OS X stores time in a struct timespec consisting of a time_t (long at
 * present) tv_sec and a long tv_nsec where tv_sec is the number of 1-second
 * intervals since 1st January 1970, 00:00:00 UTC and tv_nsec is the number of
 * 1-nano-second intervals since the value of tv_sec.
 *
 * NTFS uses Microsoft's standard time format which is stored in a s64 and is
 * measured as the number of 100-nano-second intervals since 1st January 1601,
 * 00:00:00 UTC.
 */
static inline sle64 utc2ntfs(const struct timespec ts)
{
	/*
	 * Convert the seconds to 100ns intervals, add the nano-seconds
	 * converted to 100ns intervals, and then add the NTFS time offset.
	 */
	return cpu_to_sle64((s64)ts.tv_sec * 10000000 + ts.tv_nsec / 100 +
			NTFS_TIME_OFFSET);
}

/**
 * ntfs_utc_current_time - get the current time in OS X time
 *
 * Get the current time from the OS X kernel, round it down to the nearest
 * 100-nano-second interval and return that in cpu format.
 */
static inline struct timespec ntfs_utc_current_time(void)
{
	struct timespec ts;

	nanotime(&ts);
	/* Round down to nearest 100-nano-second interval. */
	ts.tv_nsec -= ts.tv_nsec % 100;
	return ts;
}

/**
 * ntfs_current_time - get the current time in little endian NTFS format
 *
 * Get the current time from the OS X kernel, convert it to its corresponding
 * NTFS time and return that in little endian format.
 */
static inline sle64 ntfs_current_time(void)
{
	struct timespec ts;

	nanotime(&ts);
	return utc2ntfs(ts);
}

/**
 * ntfs2utc - convert NTFS time to OS X time
 * @time:	NTFS time (little endian) to convert to OS X UTC
 *
 * Convert the little endian NTFS time @time to its corresponding OS X UTC time
 * and return that in cpu format.
 *
 * OS X stores time in a struct timespec consisting of a time_t (long at
 * present) tv_sec and a long tv_nsec where tv_sec is the number of 1-second
 * intervals since 1st January 1970, 00:00:00 UTC without including leap
 * seconds and tv_nsec is the number of 1-nano-second intervals since the value
 * of tv_sec.
 *
 * NTFS uses Microsoft's standard time format which is stored in a s64 and is
 * measured as the number of 100 nano-second intervals since 1st January 1601,
 * 00:00:00 UTC.
 *
 * FIXME: There does not appear to be an asm optimized function in xnu to do
 * the division and return the remainder in one single step.  If there is or
 * one gets added at some point the below division and remainder determination
 * should be combined into a single step using it.
 */
static inline struct timespec ntfs2utc(const sle64 time)
{
	u64 t;
	struct timespec ts;

	/* Subtract the NTFS time offset. */
	t = (u64)(sle64_to_cpu(time) - NTFS_TIME_OFFSET);
	/*
	 * Convert the time to 1-second intervals and the remainder to
	 * 1-nano-second intervals.
	 */
	ts.tv_sec = t / 10000000;
	ts.tv_nsec = (t % 10000000) * 100;
	return ts;
}

#endif /* _OSX_NTFS_TIME_H */
