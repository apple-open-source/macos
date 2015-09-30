/*
 * Copyright (c) 2000-2001,2011-2012,2014 Apple Inc. All Rights Reserved.
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
 * tpTime.c - cert related time functions
 *
 */
 
#include "tpTime.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>

/*
 * Given a string containing either a UTC-style or "generalized time"
 * time string, convert to a CFDateRef. Returns nonzero on
 * error. 
 */
int timeStringToCfDate(
	const char			*str,
	unsigned			len,
	CFDateRef			*cfDate)
{
	char 		szTemp[5];
	bool 		isUtc = false;			// 2-digit year
	bool		isLocal = false;		// trailing timezone offset
	bool		isCssmTime = false;		// no trailing 'Z'
	bool		noSeconds = false;
	int             x;
	unsigned 	i;
	char 		*cp;
	CFGregorianDate		gd;
	CFTimeZoneRef		timeZone;
	CFTimeInterval		gmtOff = 0;
	
	if((str == NULL) || (len == 0) || (cfDate == NULL)) {
    	return 1;
  	}
  	
  	/* tolerate NULL terminated or not */
  	if(str[len - 1] == '\0') {
  		len--;
  	}
  	switch(len) {
		case UTC_TIME_NOSEC_LEN:		// 2-digit year, no seconds, not y2K compliant
			isUtc = true;
			noSeconds = true;
			break;
  		case UTC_TIME_STRLEN:			// 2-digit year, not Y2K compliant
  			isUtc = true;
  			break;
		case CSSM_TIME_STRLEN:
			isCssmTime = true;
			break;
  		case GENERALIZED_TIME_STRLEN:	// 4-digit year
  			break;
 		case LOCALIZED_UTC_TIME_STRLEN:	// "YYMMDDhhmmssThhmm" (where T=[+,-])
			isUtc = 1;
			// deliberate fallthrough
		case LOCALIZED_TIME_STRLEN:		// "YYYYMMDDhhmmssThhmm" (where T=[+,-])
			isLocal = 1;
			break;
 		default:						// unknown format 
  			return 1;
  	}
  	
  	cp = (char *)str;
  	
	/* check that all characters except last (or timezone indicator, if localized) are digits */
	for(i=0; i<(len - 1); i++) {
		if ( !(isdigit(cp[i])) )
			if ( !isLocal || !(cp[i]=='+' || cp[i]=='-') )
				return 1;
	}

  	/* check last character is a 'Z' or digit as appropriate */
	if(isCssmTime || isLocal) {
		if(!isdigit(cp[len - 1])) {
			return 1;
		}
	}
	else {
		if(cp[len - 1] != 'Z' )	{
			return 1;
		}
	}
	
  	/* YEAR */
	szTemp[0] = *cp++;
	szTemp[1] = *cp++;
	if(!isUtc) {
		/* two more digits */
		szTemp[2] = *cp++;
		szTemp[3] = *cp++;
		szTemp[4] = '\0';
	}
	else { 
		szTemp[2] = '\0';
	}
	x = atoi( szTemp );
	if(isUtc) {
		/* 
		 * 2-digit year. 
		 *   0  <= year <  50 : assume century 21
		 *   50 <= year <  70 : illegal per PKIX
		 *   ...though we allow this as of 10/10/02...dmitch
		 *   70 <  year <= 99 : assume century 20
		 */
		if(x < 50) {
			x += 2000;
		}
		/*
		else if(x < 70) {
			return 1;
		}
		*/
		else {
			/* century 20 */
			x += 1900;			
		}
	}
	gd.year = x;

  	/* MONTH */
	szTemp[0] = *cp++;
	szTemp[1] = *cp++;
	szTemp[2] = '\0';
	x = atoi( szTemp );
	/* in the string, months are from 1 to 12 */
	if((x > 12) || (x <= 0)) {
    	return 1;
	}
	gd.month = x;

 	/* DAY */
	szTemp[0] = *cp++;
	szTemp[1] = *cp++;
	szTemp[2] = '\0';
	x = atoi( szTemp );
	/* 1..31 in both formats */
	if((x > 31) || (x <= 0)) {
		return 1;
	}
	gd.day = x;

	/* HOUR */
	szTemp[0] = *cp++;
	szTemp[1] = *cp++;
	szTemp[2] = '\0';
	x = atoi( szTemp );
	if((x > 23) || (x < 0)) {
		return 1;
	}
	gd.hour = x;

  	/* MINUTE */
	szTemp[0] = *cp++;
	szTemp[1] = *cp++;
	szTemp[2] = '\0';
	x = atoi( szTemp );
	if((x > 59) || (x < 0)) {
		return 1;
	}
	gd.minute = x;

  	/* SECOND */
	if(noSeconds) {
		gd.second = 0;
	}
	else {
		szTemp[0] = *cp++;
		szTemp[1] = *cp++;
		szTemp[2] = '\0';
		x = atoi( szTemp );
		if((x > 59) || (x < 0)) {
			return 1;
		}
		gd.second = x;
	}
	
	if (isLocal) {
		/* ZONE INDICATOR */
		switch(*cp++) {
			case '+':
				gmtOff = 1;
				break;
			case '-':
				gmtOff = -1;
				break;
			default:
				return 1;
		}
	  	/* ZONE HH OFFSET */
		szTemp[0] = *cp++;
		szTemp[1] = *cp++;
		szTemp[2] = '\0';
		x = atoi( szTemp ) * 60 * 60;
		gmtOff *= x;
	  	/* ZONE MM OFFSET */
		szTemp[0] = *cp++;
		szTemp[1] = *cp++;
		szTemp[2] = '\0';
		x = atoi( szTemp ) * 60;
		if(gmtOff < 0) {
			gmtOff -= x;
		}
		else {
			gmtOff += x;
		}
	}
	timeZone = CFTimeZoneCreateWithTimeIntervalFromGMT(NULL, gmtOff);
	if (!timeZone) {
		return 1;
	}
	*cfDate = CFDateCreate(NULL, CFGregorianDateGetAbsoluteTime(gd, timeZone));
	CFRelease(timeZone);
	return 0;
}

/*
 * Compare two times. Assumes they're both in GMT. Returns:
 * -1 if t1 <  t2
 *  0 if t1 == t2
 *  1 if t1 >  t2
 */
int compareTimes(
	CFDateRef 	t1,
	CFDateRef 	t2)
{
	switch(CFDateCompare(t1, t2, NULL)) {
		case kCFCompareLessThan:
			return -1;
		case kCFCompareEqualTo:
			return 0;
		case kCFCompareGreaterThan:
			return 1;
	}
	/* NOT REACHED */
	assert(0);
	return 0;
}

/*
 * Create a time string, in either UTC (2-digit) or or Generalized (4-digit)
 * year format. Caller mallocs the output string whose length is at least
 * (UTC_TIME_STRLEN+1), (GENERALIZED_TIME_STRLEN+1), or (CSSM_TIME_STRLEN+1)
 * respectively. Caller must hold tpTimeLock.
 */
void timeAtNowPlus(unsigned secFromNow, 
	TpTimeSpec timeSpec,
	char *outStr)
{
	struct tm utc;
	time_t baseTime;
	
	baseTime = time(NULL);
	baseTime += (time_t)secFromNow;
	utc = *gmtime(&baseTime);
	
	switch(timeSpec) {
		case TIME_UTC:
			/* UTC - 2 year digits - code which parses this assumes that
			 * (2-digit) years between 0 and 49 are in century 21 */
			if(utc.tm_year >= 100) {
				utc.tm_year -= 100;
			}
			sprintf(outStr, "%02d%02d%02d%02d%02d%02dZ",
				utc.tm_year /* + 1900 */, utc.tm_mon + 1,
				utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec);
			break;
		case TIME_GEN:
			sprintf(outStr, "%04d%02d%02d%02d%02d%02dZ",
				/* note year is relative to 1900, hopefully it'll have 
				* four valid digits! */
				utc.tm_year + 1900, utc.tm_mon + 1,
				utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec);
			break;
		case TIME_CSSM:
			sprintf(outStr, "%04d%02d%02d%02d%02d%02d",
				/* note year is relative to 1900, hopefully it'll have 
				* four valid digits! */
				utc.tm_year + 1900, utc.tm_mon + 1,
				utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec);
			break;
	}
}

/*
 * Convert a time string, which can be in any of three forms (UTC,
 * generalized, or CSSM_TIMESTRING) into a CSSM_TIMESTRING. Caller
 * mallocs the result, which must be at least (CSSM_TIME_STRLEN+1) bytes.
 * Returns nonzero if incoming time string is badly formed. 
 */
int tpTimeToCssmTimestring(
	const char 	*inStr,			// not necessarily NULL terminated
	unsigned	inStrLen,		// not including possible NULL
	char 		*outTime)
{
	if((inStrLen == 0) || (inStr == NULL)) {
		return 1;
	}
	outTime[0] = '\0';
	switch(inStrLen) {
		case UTC_TIME_STRLEN:
		{
			/* infer century and prepend to output */
			char tmp[3];
			int year;
			tmp[0] = inStr[0];
			tmp[1] = inStr[1];
			tmp[2] = '\0';
			year = atoi(tmp);
			
			/* 
			 *   0  <= year <  50 : assume century 21
			 *   50 <= year <  70 : illegal per PKIX
			 *   70 <  year <= 99 : assume century 20
			 */
			if(year < 50) {
				/* century 21 */
				strcpy(outTime, "20");
			}
			else if(year < 70) {
				return 1;
			}
			else {
				/* century 20 */
				strcpy(outTime, "19");
			}
			memmove(outTime + 2, inStr, inStrLen - 1);		// don't copy the Z
			break;
		}
		case CSSM_TIME_STRLEN:
			memmove(outTime, inStr, inStrLen);				// trivial case
			break;
		case GENERALIZED_TIME_STRLEN:
			memmove(outTime, inStr, inStrLen - 1);			// don't copy the Z
			break;
		
		default:
			return 1;
	}
	outTime[CSSM_TIME_STRLEN] = '\0';
	return 0;
}


