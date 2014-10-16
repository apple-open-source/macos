/*
 * Copyright (c) 2011 Apple Inc. All rights reserved.
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

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <CoreServices/CoreServicesPriv.h>
#include "webdav_utils.h"

/*****************************************************************************/

/*
 * DateBytesToTime parses the RFC 850, RFC 1123, and asctime formatted
 * date/time bytes and returns time_t. If the parse fails, this function
 * returns -1.
 */
time_t DateBytesToTime(			/* <- time_t value */
	const UInt8 *bytes,			/* -> pointer to bytes to parse */
	CFIndex length)				/* -> number of bytes to parse */
{
	const UInt8 *finish;
	Date gdate;
	struct tm tm_temp;
	time_t clock;
	
	/* parse the RFC 850, RFC 1123, and asctime formatted date/time CFString to get the Gregorian date */
	finish = CFGregorianDateCreateWithBytes(kCFAllocatorDefault, bytes, length, &gdate, NULL);
	require_action(finish != bytes, CFGregorianDateCreateWithBytes, clock = -1);
	
	/* copy the Gregorian date into struct tm */
	memset(&tm_temp, 0, sizeof(struct tm));
	tm_temp.tm_sec = (int)gdate.second;
	tm_temp.tm_min = gdate.minute;
	tm_temp.tm_hour = gdate.hour;
	tm_temp.tm_mday = gdate.day;
	tm_temp.tm_mon = gdate.month - 1;
	tm_temp.tm_year = gdate.year - 1900;
	
	clock = timegm(&tm_temp);
	
CFGregorianDateCreateWithBytes:
	
	return ( clock );
}

/*****************************************************************************/

/*
 * DateStringToTime parses the RFC 850, RFC 1123, and asctime formatted
 * date/time CFString and returns time_t. If the parse fails, this function
 * returns -1.
 */
time_t DateStringToTime(	/* <- time_t value; -1 if error */
		CFStringRef str)	/* -> CFString to parse */
{
	CFIndex count;
	Date gdate;
	struct tm tm_temp;
	time_t clock;
	
	/* parse the RFC 850, RFC 1123, and asctime formatted date/time CFString to get the Gregorian date */
	count = CFGregorianDateCreateWithString(kCFAllocatorDefault, str, &gdate, NULL);
	require_action(count != 0, _CFGregorianDateCreateWithString, clock = -1);
	
	/* copy the Gregorian date into struct tm */
	memset(&tm_temp, 0, sizeof(struct tm));
	tm_temp.tm_sec = (int)gdate.second;
	tm_temp.tm_min = gdate.minute;
	tm_temp.tm_hour = gdate.hour;
	tm_temp.tm_mday = gdate.day;
	tm_temp.tm_mon = gdate.month - 1;
	tm_temp.tm_year = gdate.year - 1900;
	
	clock = timegm(&tm_temp);
	
_CFGregorianDateCreateWithString:
	
	return ( clock );
}

/*****************************************************************************/

char* createUTF8CStringFromCFString(CFStringRef in_string)
{
	char* out_cstring = NULL;
	
	CFIndex bufSize;
	
	/* make sure we're not passed garbage */
	if ( in_string == NULL )
		return NULL;
	
	/* Add one to account for NULL termination. */
	bufSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(in_string) + 1, kCFStringEncodingUTF8);
	
	out_cstring = (char *)calloc(1, bufSize);
	
	/* Make sure malloc succeeded then convert cstring */
	if ( out_cstring == NULL ) 
		return NULL;
	
	if ( CFStringGetCString(in_string, out_cstring, bufSize, kCFStringEncodingUTF8) == FALSE ) {
		free(out_cstring);
		out_cstring = NULL;
	}
	
	return out_cstring;
}
/***************************************************************************************************************************************/

const UInt8* CFGregorianDateCreateWithBytes(CFAllocatorRef alloc, const UInt8* bytes, CFIndex length, Date* date, CFTimeZoneRef* tz) {
	
	UInt8 buffer[256];					/* Any dates longer than this are not understood. */
	
	length = (length == 256) ? 255 : length;
	memmove(buffer, bytes, length);
	buffer[length] = '\0';				/* Guarantees every compare will fail if trying to index off the end. */
	
	memset(date, 0, sizeof(date[0]));
	if (tz) *tz = NULL;
	
	do {
		unsigned int i;
		CFIndex scan = 0;
		UInt8 c = buffer[scan];
		
		/* Skip leading whitespace */
		while (isspace(c))
			c = buffer[++scan];
		
		/* Check to see if there is a weekday up front. */
		if (!isdigit(c)) {
			
			for (i = 0; i < (sizeof(kDayStrs) / sizeof(kDayStrs[0])); i++) {
				if (!memcmp(kDayStrs[i], &buffer[scan], strlen(kDayStrs[i])))
					break;
			}
			
			if (i >=(sizeof(kDayStrs) / sizeof(kDayStrs[0])))
				break;
			
			scan += strlen(kDayStrs[i]);
			c = buffer[scan];
			
			while (isspace(c) || c == ',')
				c = buffer[++scan];
		}
		
		/* check for asctime where month comes first */
		if (!isdigit(c)) {
			
			for (i = 0; i < (sizeof(kMonthStrs) / sizeof(kMonthStrs[0])); i++) {
				if (!memcmp(kMonthStrs[i], &buffer[scan], strlen(kMonthStrs[i])))
					break;
			}
			
			if (i >= (sizeof(kMonthStrs) / sizeof(kMonthStrs[0])))
				break;
			
			date->month = (i % 12) + 1;
			
			scan += strlen(kMonthStrs[i]);
			c = buffer[scan];
			
			while (isspace(c))
				c = buffer[++scan];
			
			if (!isdigit(c))
				break;
		}
		
		/* Read the day of month */
		for (i = 0; isdigit(c) && (i < 2); i++) {
			date->day *= 10;
			date->day += c - '0';
			c = buffer[++scan];
		}
		
		while (isspace(c) || c == '-')
			c = buffer[++scan];
		
		/* Not asctime so now comes the month. */
		if (date->month == 0) {
			
			if (isdigit(c)) {
				for (i = 0; isdigit(c) && (i < 2); i++) {
					date->month *= 10;
					date->month += c - '0';
					c = buffer[++scan];
				}
			}
			else {
				for (i = 0; i < (sizeof(kMonthStrs) / sizeof(kMonthStrs[0])); i++) {
					if (!memcmp(kMonthStrs[i], &buffer[scan], strlen(kMonthStrs[i])))
						break;
				}
				
				if (i >= (sizeof(kMonthStrs) / sizeof(kMonthStrs[0])))
					break;
				
				date->month = (i % 12) + 1;
				
				scan += strlen(kMonthStrs[i]);
				c = buffer[scan];
			}
			
			while (isspace(c) || c == '-')
				c = buffer[++scan];
			
			/* Read the year */
			for (i = 0; isdigit(c) && (i < 4); i++) {
				date->year *= 10;
				date->year += c - '0';
				c = buffer[++scan];
			}
			
			while (isspace(c))
				c = buffer[++scan];
		}
		
		/* Read the hours */
		for (i = 0; isdigit(c) && (i < 2); i++) {
			date->hour *= 10;
			date->hour += c - '0';
			c = buffer[++scan];
		}
		
		if (c != ':')
			break;
		c = buffer[++scan];
		
		/* Read the minutes */
		for (i = 0; isdigit(c) && (i < 2); i++) {
			date->minute *= 10;
			date->minute += c - '0';
			c = buffer[++scan];
		}
		
		if (c == ':') {
			
			c = buffer[++scan];
			
			/* Read the seconds */
			for (i = 0; isdigit(c) && (i < 2); i++) {
				date->second *= 10;
				date->second += c - '0';
				c = buffer[++scan];
			}
			c = buffer[++scan];
		}
		
		/* If haven't read the year yet, now is the time. */
		if (date->year == 0) {
			
			while (isspace(c))
				c = buffer[++scan];
			
			/* Read the year */
			for (i = 0; isdigit(c) && (i < 4); i++) {
				date->year *= 10;
				date->year += c - '0';
				c = buffer[++scan];
			}
		}
		
		if (date->year && date->year < 100) {
			
			if (date->year < 70)
				date->year += 2000;		/* My CC is still using 2-digit years! */
			else
				date->year += 1900;		/* Bad 2 byte clients */
		}
		
		while (isspace(c))
			c = buffer[++scan];
		
		if (c && tz) {
			
			/* If it has absolute offset, read the hours and minutes. */
			if ((c == '+') || (c == '-')) {
				
				char sign = c;
				CFTimeInterval minutes = 0, offset = 0;
				
				c = buffer[++scan];
				
				/* Read the hours */
				for (i = 0; isdigit(c) && (i < 2); i++) {
					offset *= 10;
					offset += c - '0';
					c = buffer[++scan];
				}
				
				/* Read the minutes */
				for (i = 0; isdigit(c) && (i < 2); i++) {
					minutes *= 10;
					minutes += c - '0';
					c = buffer[++scan];
				}
				
				offset *= 60;
				offset += minutes;
				
				if (sign == '-') offset *= -60;
				else offset *= 60;
				
				*tz = CFTimeZoneCreateWithTimeIntervalFromGMT(alloc, offset);
			}
			
			/* If it's not GMT/UT time, need to parse the alpha offset. */
			
			else if (!strncmp((const char*)(&buffer[scan]), "U", 1)) {
				*tz = CFTimeZoneCreateWithTimeIntervalFromGMT(alloc, 0);
				scan += 1;
			}
			
			else if (!strncmp((const char*)(&buffer[scan]), "UT", 2)) {
				*tz = CFTimeZoneCreateWithTimeIntervalFromGMT(alloc, 0);
				scan += 2;
			}
			
			else if (!strncmp((const char*)(&buffer[scan]), "GMT", 3)) {
				*tz = CFTimeZoneCreateWithTimeIntervalFromGMT(alloc, 0);
				scan += 3;
			}
			
			else if (isalpha(c)) {
				
				UInt8 next = buffer[scan + 1];
				
				/* Check for military time. */
				if ((c != 'J') && (!next || isspace(next) || (next == '*'))) {
					
					if (c == 'Z')
						*tz = CFTimeZoneCreateWithTimeIntervalFromGMT(alloc, 0);
					
					else {
						
						// Maybe we want to uncomment all this and then pass offset to CFTimeZoneCreateWithTimeIntervalFromGMT() ?
						// RFC 2822 says that all these single-letter codes should be treated as "-0000" so maybe that's why we're passing 0
						/*
						 CFTimeInterval offset = (c < 'N') ? (c - 'A' + (c < 'K' ? 1 : 0)) : ('M' - c);
						 
						 offset *= 60;
						 
						 if (next == '*') {
						 scan++;
						 offset = (offset < 0) ? offset - 30 : offset + 30;
						 }
						 
						 offset *= 60;
						 */
						*tz = CFTimeZoneCreateWithTimeIntervalFromGMT(alloc, 0);
					}
				}
				
				else {
					
					for (i = 0; i < (sizeof(kUSTimeZones) / sizeof(kUSTimeZones[0])); i++) {
						
						if (!memcmp(kUSTimeZones[i], &buffer[scan], strlen(kUSTimeZones[i]))) {
							// the casts if i below to an int are needed otherwise the value passed as the time interval
							// is treated as an unsigned value (since i is now unsigned).  This caused the tz to be null
							// for valid US time zone names...
							*tz = CFTimeZoneCreateWithTimeIntervalFromGMT(alloc, (-8 + ((int)i >> 1) + ((int)i & 0x1)) * 3600);
							scan += strlen(kUSTimeZones[i]);
							
							break;
						}
					}
				}
			}
		}
		
		if (!DateIsValid(*date))
			break;
		
		return bytes + scan;
		
	} while (1);
	
	memset(date, 0, sizeof(date[0]));
	if (tz) {
		if (*tz) CFRelease(*tz);
		*tz = NULL;
	}
	
	return bytes;
}
/**************************************************************************************************************/

CFIndex CFGregorianDateCreateWithString(CFAllocatorRef alloc, CFStringRef str, Date* date, CFTimeZoneRef* tz) {
	
	UInt8 buffer[256];					/* Any dates longer than this are not understood. */
	CFIndex length = CFStringGetLength(str);
	CFIndex result = 0;
	
	CFStringGetBytes(str, CFRangeMake(0, length), kCFStringEncodingASCII, 0, FALSE, buffer, sizeof(buffer), &length);
	
	if (length)
		result = CFGregorianDateCreateWithBytes(alloc, buffer, length, date, tz) - buffer;
	
	else {
		memset(date, 0, sizeof(date[0]));
		if (tz) *tz = NULL;
	}
	
	return result;
}

/************************************************/

Boolean IsLeapYear(SInt32 year) {
	
	int64_t y = (year + 1) % 400;    /* correct to nearest multiple-of-400 year, then find the remainder */
    if (y < 0)
		y = -y;
    return (0 == (y & 3) && 100 != y && 200 != y && 300 != y);
	
}
/**************************************************************************/

/* year arg is absolute year; Gregorian 2001 == year 0; 2001/1/1 = absolute date 0 */
Boolean DateIsValid(Date date) {
	bool leap = 0;
	if (date.year<=0 || date.month<1 || date.day<1 || date.month>12 || date.day>31)
		return false;
	if(date.hour < 0 || 23 < date.hour)
		return false;
	if(date.minute < 0 || 59 < date.minute)
		return false;
	if(date.second < 0.0 || 60.0 <= date.second)
		return false;
	leap = IsLeapYear(date.year - 2001);
	if ((daysInMonth[date.month] + (2 == date.month && leap)) < date.day) {
		return false;
	}
	return true;
}

/*************************************************************************/


