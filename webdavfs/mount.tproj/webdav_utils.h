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

#ifndef webdavfs_webdav_utils_h
#define webdavfs_webdav_utils_h

/*
 * DateBytesToTime parses the RFC 850, RFC 1123, and asctime formatted
 * date/time bytes and returns time_t. If the parse fails, this function
 * returns a time_t set to -1.
 */
time_t DateBytesToTime(	
	const UInt8 *bytes,	/* -> pointer to bytes to parse */
	CFIndex length);	/* -> number of bytes to parse */

char* createUTF8CStringFromCFString(CFStringRef in_string);

/*
 * DateStringToTime parses the RFC 850, RFC 1123, and asctime formatted
 * date/time CFString and returns time_t. If the parse fails, this function
 * returns -1.
 */
time_t DateStringToTime(	/* <- time_t value; -1 if error */
		CFStringRef str);	/* -> CFString to parse */

#endif
