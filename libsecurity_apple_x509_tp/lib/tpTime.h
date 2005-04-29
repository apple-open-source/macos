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
#include <CoreFoundation/CoreFoundation.h>
#include <Security/cssmtype.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* lengths of time strings without trailing NULL */
#define UTC_TIME_NOSEC_LEN			11
#define UTC_TIME_STRLEN				13
#define CSSM_TIME_STRLEN			14		/* no trailing 'Z' */
#define GENERALIZED_TIME_STRLEN		15		
#define LOCALIZED_UTC_TIME_STRLEN	17
#define LOCALIZED_TIME_STRLEN		19

/*
 * Given a string containing either a UTC-style or "generalized time"
 * time string, convert to a CFDateRef. Returns nonzero on
 * error. 
 */
extern int timeStringToCfDate(
	const char			*str,
	unsigned			len,
	CFDateRef			*cfDate);

/*
 * Compare two times. Assumes they're both in GMT. Returns:
 * -1 if t1 <  t2
 *  0 if t1 == t2
 *  1 if t1 >  t2
 */
extern int compareTimes(
	CFDateRef 	t1,
	CFDateRef 	t2);
	
/*
 * Create a time string, in either UTC (2-digit) or or Generalized (4-digit)
 * year format. Caller mallocs the output string whose length is at least
 * (UTC_TIME_STRLEN+1), (GENERALIZED_TIME_STRLEN+1), or (CSSM_TIME_STRLEN+1)
 * respectively. Caller must hold tpTimeLock.
 */
typedef enum {
	TIME_UTC,
	TIME_GEN,
	TIME_CSSM
} TpTimeSpec;

void timeAtNowPlus(unsigned secFromNow,
	TpTimeSpec timeSpec,
	char *outStr);

/*
 * Convert a time string, which can be in any of three forms (UTC,
 * generalized, or CSSM_TIMESTRING) into a CSSM_TIMESTRING. Caller
 * mallocs the result, which must be at least (CSSM_TIME_STRLEN+1) bytes.
 * Returns nonzero if incoming time string is badly formed. 
 */
int tpTimeToCssmTimestring(
	const char 	*inStr,			// not necessarily NULL terminated
	unsigned	inStrLen,		// not including possible NULL
	char 		*outTime);		// caller mallocs

#ifdef	__cplusplus
}
#endif

#endif	/* _TP_TIME_H_*/
