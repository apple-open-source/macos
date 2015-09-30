/*
 * Copyright (c) 2000-2004,2011-2014 Apple Inc. All Rights Reserved.
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
 *
 * cssmdatetime.cpp -- CSSM date and time utilities for the Mac
 */

#ifdef __MWERKS__
#define _CPP_CSSM_DATE_TIME_UTILS
#endif

#include "cssmdatetime.h"

#include <string.h>
#include <stdio.h>
#include <security_utilities/errors.h>
#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/CFTimeZone.h>
#include <ctype.h>
#include <stdlib.h>
#include <SecBase.h>
namespace Security
{

namespace CSSMDateTimeUtils
{

#define UTC_TIME_NOSEC_LEN			11
#define UTC_TIME_STRLEN				13
#define GENERALIZED_TIME_STRLEN		15
#define LOCALIZED_UTC_TIME_STRLEN	17
#define LOCALIZED_TIME_STRLEN		19
#define MAX_TIME_STR_LEN			30


void
GetCurrentMacLongDateTime(sint64 &outMacDate)
{
	CFTimeZoneRef timeZone = CFTimeZoneCopyDefault();
	CFAbsoluteTime absTime = CFAbsoluteTimeGetCurrent();
	absTime += CFTimeZoneGetSecondsFromGMT(timeZone, absTime);
	CFRelease(timeZone);
	outMacDate = sint64(double(absTime + kCFAbsoluteTimeIntervalSince1904));
}

void
TimeStringToMacSeconds (const CSSM_DATA &inUTCTime, uint32 &ioMacDate)
{
    sint64 ldt;
    TimeStringToMacLongDateTime(inUTCTime, ldt);
    ioMacDate = uint32(ldt);
}

/*
 * Given a CSSM_DATA containing either a UTC-style or "generalized time"
 * time string, convert to 32-bit Mac time in seconds.
 * Returns nonzero on error. 
 */
void
TimeStringToMacLongDateTime (const CSSM_DATA &inUTCTime, sint64 &outMacDate)
{
	char 		szTemp[5];
	size_t          len;
	int 		isUtc;
	sint32 		x;
	sint32 		i;
	char 		*cp;

	CFGregorianDate date;
	::memset( &date, 0, sizeof(date) );

	if ((inUTCTime.Data == NULL) || (inUTCTime.Length == 0))
    {
    	MacOSError::throwMe(errSecParam);
  	}

  	/* tolerate NULL terminated or not */
  	len = inUTCTime.Length;
  	if (inUTCTime.Data[len - 1] == '\0')
  		len--;

  	switch(len)
    {
  		case UTC_TIME_STRLEN:			// 2-digit year, not Y2K compliant
  			isUtc = 1;
  			break;
  		case GENERALIZED_TIME_STRLEN:	// 4-digit year
  			isUtc = 0;
  			break;
  		default:						// unknown format 
            MacOSError::throwMe(errSecParam);
  	}

  	cp = (char *)inUTCTime.Data;
  	
	/* check that all characters except last are digits */
    for(i=0; i<(sint32)(len - 1); i++) {
		if ( !(isdigit(cp[i])) ) {
            MacOSError::throwMe(errSecParam);
		}
	}

  	/* check last character is a 'Z' */
  	if(cp[len - 1] != 'Z' )	{
        MacOSError::throwMe(errSecParam);
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
		 *   0  <= year <= 50 : assume century 21
		 *   50 <  year <  70 : illegal per PKIX
		 *   70 <  year <= 99 : assume century 20
		 */
		if(x <= 50) {
			x += 100;
		}
		else if(x < 70) {
            MacOSError::throwMe(errSecParam);
		}
		/* else century 20, OK */

		/* bug fix... we need to end up with a 4-digit year! */
		x += 1900;
	}
  	/* by definition - tm_year is year - 1900 */
  	//tmp->tm_year = x - 1900;
  	date.year = x;

  	/* MONTH */
	szTemp[0] = *cp++;
	szTemp[1] = *cp++;
	szTemp[2] = '\0';
	x = atoi( szTemp );
	/* in the string, months are from 1 to 12 */
	if((x > 12) || (x <= 0)) {
        MacOSError::throwMe(errSecParam);
	}
	/* in a tm, 0 to 11 */
  	//tmp->tm_mon = x - 1;
  	date.month = x;

 	/* DAY */
	szTemp[0] = *cp++;
	szTemp[1] = *cp++;
	szTemp[2] = '\0';
	x = atoi( szTemp );
	/* 1..31 in both formats */
	if((x > 31) || (x <= 0)) {
        MacOSError::throwMe(errSecParam);
	}
  	//tmp->tm_mday = x;
  	date.day = x;

	/* HOUR */
	szTemp[0] = *cp++;
	szTemp[1] = *cp++;
	szTemp[2] = '\0';
	x = atoi( szTemp );
	if((x > 23) || (x < 0)) {
        MacOSError::throwMe(errSecParam);
	}
	//tmp->tm_hour = x;
	date.hour = x;

  	/* MINUTE */
	szTemp[0] = *cp++;
	szTemp[1] = *cp++;
	szTemp[2] = '\0';
	x = atoi( szTemp );
	if((x > 59) || (x < 0)) {
        MacOSError::throwMe(errSecParam);
	}
  	//tmp->tm_min = x;
  	date.minute = x;

  	/* SECOND */
	szTemp[0] = *cp++;
	szTemp[1] = *cp++;
	szTemp[2] = '\0';
  	x = atoi( szTemp );
	if((x > 59) || (x < 0)) {
        MacOSError::throwMe(errSecParam);
	}
  	//tmp->tm_sec = x;
  	date.second = x;

	CFTimeZoneRef timeZone = CFTimeZoneCreateWithTimeIntervalFromGMT(NULL, 0);
	CFAbsoluteTime absTime = CFGregorianDateGetAbsoluteTime(date, timeZone);
	CFRelease(timeZone);

	// Adjust abstime to local timezone
	timeZone = CFTimeZoneCopyDefault();
	absTime += CFTimeZoneGetSecondsFromGMT(timeZone, absTime);
	CFRelease(timeZone);

	outMacDate = sint64(double(absTime + kCFAbsoluteTimeIntervalSince1904));
}

void MacSecondsToTimeString(uint32 inMacDate, uint32 inLength, void *outData)
{
    sint64 ldt = sint64(uint64(inMacDate));
    MacLongDateTimeToTimeString(ldt, inLength, outData);
}

void MacLongDateTimeToTimeString(const sint64 &inMacDate,
                                        uint32 inLength, void *outData)
{
	// @@@ this code is close, but on the fringe case of a daylight savings time it will be off for a little while
	CFAbsoluteTime absTime = inMacDate - kCFAbsoluteTimeIntervalSince1904;

	// Remove local timezone component from absTime
	CFTimeZoneRef timeZone = CFTimeZoneCopyDefault();
	absTime -= CFTimeZoneGetSecondsFromGMT(timeZone, absTime);
	CFRelease(timeZone);

	timeZone = CFTimeZoneCreateWithTimeIntervalFromGMT(NULL, 0);
	CFGregorianDate date = CFAbsoluteTimeGetGregorianDate(absTime, timeZone);
	CFRelease(timeZone);

    if (inLength == 16)
    {
        sprintf((char *)(outData), "%04d%02d%02d%02d%02d%02dZ",
                int(date.year % 10000), date.month, date.day,
                date.hour, date.minute, int(date.second));
    }
    else if (inLength == 14)
    {
 		/* UTC - 2 year digits - code which parses this assumes that
		 * (2-digit) years between 0 and 49 are in century 21 */
        sprintf((char *)(outData), "%02d%02d%02d%02d%02d%02dZ",
                int(date.year % 100), date.month, date.day,
                date.hour, date.minute, int(date.second));
    }
    else
        MacOSError::throwMe(errSecParam);
}

void
CFDateToCssmDate(CFDateRef date, char *outCssmDate)
{
	CFTimeZoneRef timeZone = CFTimeZoneCreateWithTimeIntervalFromGMT(NULL, 0);
	CFGregorianDate gd = CFAbsoluteTimeGetGregorianDate(CFDateGetAbsoluteTime(date), timeZone);
	sprintf(outCssmDate, "%04d%02d%02d%02d%02d%02dZ", (int)gd.year, gd.month, gd.day, gd.hour, gd.minute, (unsigned int)gd.second);
	CFRelease(timeZone);
}

void
CssmDateToCFDate(const char *cssmDate, CFDateRef *outCFDate)
{
	CFTimeZoneRef timeZone = CFTimeZoneCreateWithTimeIntervalFromGMT(NULL, 0);
	CFGregorianDate gd;
	unsigned int year, month, day, hour, minute, second;
	sscanf(cssmDate, "%4d%2d%2d%2d%2d%2d", &year, &month, &day, &hour, &minute, &second);
	gd.year = year;
	gd.month = month;
	gd.day = day;
	gd.hour = hour;
	gd.minute = minute;
	gd.second = second;
	*outCFDate = CFDateCreate(NULL, CFGregorianDateGetAbsoluteTime(gd, timeZone));
	CFRelease(timeZone);
}

int
CssmDateStringToCFDate(const char *cssmDate, unsigned int len, CFDateRef *outCFDate)
{
	CFTimeZoneRef timeZone;
	CFGregorianDate gd;
	CFTimeInterval ti=0;
	char szTemp[5];
	unsigned isUtc=0, isLocal=0, i;
        int x;
	unsigned noSeconds=0;
	char *cp;

	if((cssmDate == NULL) || (len == 0) || (outCFDate == NULL))
    	return 1;
  	
  	/* tolerate NULL terminated or not */
  	if(cssmDate[len - 1] == '\0')
  		len--;

  	switch(len) {
		case UTC_TIME_NOSEC_LEN:		// 2-digit year, no seconds, not y2K compliant
   			isUtc = 1;
			noSeconds = 1;
  			break;
 		case UTC_TIME_STRLEN:			// 2-digit year, not Y2K compliant
  			isUtc = 1;
  			break;
  		case GENERALIZED_TIME_STRLEN:	// 4-digit year
			//isUtc = 0;
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
  	
  	cp = (char *)cssmDate;
  	
	/* check that all characters except last (or timezone indicator, if localized) are digits */
	for(i=0; i<(len - 1); i++) {
		if ( !(isdigit(cp[i])) )
			if ( !isLocal || !(cp[i]=='+' || cp[i]=='-') )
				return 1;
	}
	/* check last character is a 'Z', unless localized */
	if(!isLocal && cp[len - 1] != 'Z' )	{
		return 1;
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
		 *   70 <  year <= 99 : assume century 20
		 */
		if(x < 50) {
			x += 2000;
		}
		else if(x < 70) {
			return 1;
		}
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
		ti = (*cp++ == '+') ? 1 : -1;
	  	/* ZONE HH OFFSET */
		szTemp[0] = *cp++;
		szTemp[1] = *cp++;
		szTemp[2] = '\0';
		x = atoi( szTemp ) * 60 * 60;
		ti *= x;
	  	/* ZONE MM OFFSET */
		szTemp[0] = *cp++;
		szTemp[1] = *cp++;
		szTemp[2] = '\0';
		x = atoi( szTemp ) * 60;
		ti += ((ti < 0) ? (x*-1) : x);
	}
	timeZone = CFTimeZoneCreateWithTimeIntervalFromGMT(NULL, ti);
	if (!timeZone) return 1;
	*outCFDate = CFDateCreate(NULL, CFGregorianDateGetAbsoluteTime(gd, timeZone));
	CFRelease(timeZone);
	
	return 0;
}

}; // end namespace CSSMDateTimeUtils

} // end namespace Security
