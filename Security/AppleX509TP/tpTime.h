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


/*
 * tpTime.h - cert related time functions
 *
 * Written 10/10/2000 by Doug Mitchell.
 */
 
#ifndef	_TP_TIME_H_
#define _TP_TIME_H_

#include <time.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Given a string containing either a UTC-style or "generalized time"
 * time string, convert to a struct tm (in GMT/UTC). Returns nonzero on
 * error. 
 */
extern int timeStringToTm(
	const char			*str,
	unsigned			len,
	struct tm			*tmp);

/* return current GMT time as a struct tm */
extern void nowTime(
	struct tm		 	*now);

/*
 * Compare two times. Assumes they're both in GMT. Returns:
 * -1 if t1 <  t2
 *  0 if t1 == t2
 *  1 if t1 >  t2
 */
extern int compareTimes(
	const struct tm 	*t1,
	const struct tm 	*t2);
	
#ifdef	__cplusplus
}
#endif

#endif	/* _TP_TIME_H_*/
