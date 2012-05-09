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
	CFGregorianDate gdate;
	struct tm tm_temp;
	time_t clock;
	
	/* parse the RFC 850, RFC 1123, and asctime formatted date/time CFString to get the Gregorian date */
	finish = _CFGregorianDateCreateWithBytes(kCFAllocatorDefault, bytes, length, &gdate, NULL);
	require_action(finish != bytes, _CFGregorianDateCreateWithBytes, clock = -1);
	
	/* copy the Gregorian date into struct tm */
	memset(&tm_temp, 0, sizeof(struct tm));
	tm_temp.tm_sec = (int)gdate.second;
	tm_temp.tm_min = gdate.minute;
	tm_temp.tm_hour = gdate.hour;
	tm_temp.tm_mday = gdate.day;
	tm_temp.tm_mon = gdate.month - 1;
	tm_temp.tm_year = gdate.year - 1900;
	
	clock = timegm(&tm_temp);
	
_CFGregorianDateCreateWithBytes:
	
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
	CFGregorianDate gdate;
	struct tm tm_temp;
	time_t clock;
	
	/* parse the RFC 850, RFC 1123, and asctime formatted date/time CFString to get the Gregorian date */
	count = _CFGregorianDateCreateWithString(kCFAllocatorDefault, str, &gdate, NULL);
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

/*****************************************************************************/
