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
	File:		cssmdatetime.cpp

	Contains:	CSSM date and time utilities for the Mac

	Written by:	The Hindsight team

	Copyright:	© 1997-2000 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

	To Do:
*/

#ifdef __MWERKS__
#define _CPP_CSSM_DATE_TIME_UTILS
#endif

#include "cssmdatetime.h"

#include <string.h>
#include <stdio.h>
#include <Security/utilities.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/CFTimeZone.h>
#include <ctype.h>

namespace Security
{

namespace CSSMDateTimeUtils
{

#define MAX_TIME_STR_LEN  	30
#define UTC_TIME_STRLEN  13
#define GENERALIZED_TIME_STRLEN  15


void
GetCurrentMacLongDateTime(SInt64 &outMacDate)
{
	CFTimeZoneRef timeZone = CFTimeZoneCopyDefault();
	CFAbsoluteTime absTime = CFAbsoluteTimeGetCurrent();
	absTime += CFTimeZoneGetSecondsFromGMT(timeZone, absTime);
	CFRelease(timeZone);
	outMacDate = SInt64(double(absTime + kCFAbsoluteTimeIntervalSince1904));
}

void
TimeStringToMacSeconds (const CSSM_DATA &inUTCTime, UInt32 &ioMacDate)
{
    SInt64 ldt;
    TimeStringToMacLongDateTime(inUTCTime, ldt);
    ioMacDate = UInt32(ldt);
}

/*
 * Given a CSSM_DATA containing either a UTC-style or "generalized time"
 * time string, convert to 32-bit Mac time in seconds.
 * Returns nonzero on error. 
 */
void
TimeStringToMacLongDateTime (const CSSM_DATA &inUTCTime, SInt64 &outMacDate)
{
	char 		szTemp[5];
	unsigned 	len;
	int 		isUtc;
	sint32 		x;
	sint32 		i;
	char 		*cp;

	CFGregorianDate date;
	::memset( &date, 0, sizeof(date) );

	if ((inUTCTime.Data == NULL) || (inUTCTime.Length == 0))
    {
    	MacOSError::throwMe(paramErr);
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
            MacOSError::throwMe(paramErr);
  	}

  	cp = (char *)inUTCTime.Data;
  	
	/* check that all characters except last are digits */
    for(i=0; i<(sint32)(len - 1); i++) {
		if ( !(isdigit(cp[i])) ) {
            MacOSError::throwMe(paramErr);
		}
	}

  	/* check last character is a 'Z' */
  	if(cp[len - 1] != 'Z' )	{
        MacOSError::throwMe(paramErr);
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
            MacOSError::throwMe(paramErr);
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
        MacOSError::throwMe(paramErr);
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
        MacOSError::throwMe(paramErr);
	}
  	//tmp->tm_mday = x;
  	date.day = x;

	/* HOUR */
	szTemp[0] = *cp++;
	szTemp[1] = *cp++;
	szTemp[2] = '\0';
	x = atoi( szTemp );
	if((x > 23) || (x < 0)) {
        MacOSError::throwMe(paramErr);
	}
	//tmp->tm_hour = x;
	date.hour = x;

  	/* MINUTE */
	szTemp[0] = *cp++;
	szTemp[1] = *cp++;
	szTemp[2] = '\0';
	x = atoi( szTemp );
	if((x > 59) || (x < 0)) {
        MacOSError::throwMe(paramErr);
	}
  	//tmp->tm_min = x;
  	date.minute = x;

  	/* SECOND */
	szTemp[0] = *cp++;
	szTemp[1] = *cp++;
	szTemp[2] = '\0';
  	x = atoi( szTemp );
	if((x > 59) || (x < 0)) {
        MacOSError::throwMe(paramErr);
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

	outMacDate = SInt64(double(absTime + kCFAbsoluteTimeIntervalSince1904));
}

void MacSecondsToTimeString(UInt32 inMacDate, UInt32 inLength, void *outData)
{
    SInt64 ldt = SInt64(UInt64(inMacDate));
    MacLongDateTimeToTimeString(ldt, inLength, outData);
}

void MacLongDateTimeToTimeString(const SInt64 &inMacDate,
                                        UInt32 inLength, void *outData)
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
        MacOSError::throwMe(paramErr);
}

}; // end namespace CSSMDateTimeUtils

} // end namespace Security
