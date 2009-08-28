/*
 * Copyright (c) 2002 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. 
 * Please obtain a copy of the License at http://www.apple.com/publicsource
 * and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights 
 * and limitations under the License.
 */
 
/*
 * cuTimeStr.h = Time string utilities.
 */
 
#ifndef	_TIME_STR_H_
#define _TIME_STR_H_

#include <time.h>
#include <Security/x509defs.h>

#define UTC_TIME_NOSEC_LEN			11
#define UTC_TIME_STRLEN				13
#define CSSM_TIME_STRLEN			14		/* no trailing 'Z' */
#define GENERALIZED_TIME_STRLEN		15

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Given a string containing either a UTC-style or "generalized time"
 * time string, convert to a struct tm (in GMT/UTC). Returns nonzero on
 * error. 
 */
int cuTimeStringToTm(
	const char			*str,
	unsigned			len,
	struct tm			*tmp);

typedef enum {
	TIME_UTC,
	TIME_CSSM,
	TIME_GEN
} timeSpec;

/*
 * Return an APP_MALLOCd time string, specified format and time relative
 * to 'now' in seconds.
 */
char *cuTimeAtNowPlus(
	int 				secFromNow, 
	timeSpec 			spec);

/*
 * Convert a CSSM_X509_TIME, which can be in any of three forms (UTC,
 * generalized, or CSSM_TIMESTRING) into a CSSM_TIMESTRING. Caller
 * must free() the result. Returns NULL if x509time is badly formed. 
 */
char *cuX509TimeToCssmTimestring(
	const CSSM_X509_TIME 	*x509Time,
	unsigned				*rtnLen);		// for caller's convenience

#ifdef	__cplusplus
}
#endif

#endif	/* _TIME_STR_H_ */
