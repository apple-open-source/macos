/*
 * Copyright (c) 2003 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please 
 * obtain a copy of the License at http://www.apple.com/publicsource and 
 * read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER 
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. 
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 */

/*
 * cputime.h - high resolution timing module
 *
 * This module uses a highly machine-dependent mechanism to get timestamps
 * directly from CPU registers, without the overhead of a system call. The
 * timestamps are exported as type CPUTime and you should not concern yourself
 * with exactly what that is. 
 *
 * We provide routines to convert a difference between two CPUTimes as a double,
 * in seconds, milliseconds, and microseconds. Th
 *
 * The cost (time) of getting a timestamp (via CPUTimeRead()) generally takes
 * two or fewer times the resolution period, i.e., less than 80 ns on a 100 MHz
 * bus machine, often 40 ns.
 * 
 * The general usage of this module is as follows:
 *
 * {
 *		set up test scenario;
 *		CPUTime startTime = CPUTimeRead();
 *		...critical timed code here...
 *		CPUTime endTime = CPUTimeRead();
 * 		double elapsedMilliseconds = CPUTimeDeltaMs(startTime, endTime);
 * }
 *
 * It's crucial to place the CPUTimeDelta*() call OUTSIDE of the critical timed
 * area. It's really cheap to snag the timestamps, but it's not at all cheap
 * to convert the difference between two timestamps to a double. 
 */
 
#ifndef	_CPUTIME_H_
#define _CPUTIME_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <mach/mach_time.h>


typedef uint64_t CPUTime;

/*
 * Obtain machine-dependent, high resolution, cheap-to-read timestamp.
 */
#define CPUTimeRead() mach_absolute_time()

/*
 * Convert difference between two CPUTimes into various units.
 * Implemented as separate functions to preserve as much precision as possible
 * before required machine-dependent "divide by clock frequency".
 */
extern double CPUTimeDeltaSec(CPUTime from, CPUTime to);	// seconds
extern double CPUTimeDeltaMs(CPUTime from, CPUTime to);		// milliseconds
extern double CPUTimeDeltaUs(CPUTime from, CPUTime to);		// microseconds

/*
 * Calculate the average of an array of doubles. The lowest and highest values
 * are discarded if there are more than two samples. Typically used to get an
 * average of a set of values returned from CPUTimeDelta*().
 */
double CPUTimeAvg(
	const double *array,
	unsigned arraySize);

#ifdef __cplusplus
}
#endif

#endif	/* _CPUTIME_H_ */

