/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 
/*!
 *  @header URLUtilities
 *  dec'ls for using URL utilities
 */

#ifndef __URLUTILITIES__
#define __URLUTILITIES__
#include <string>

#if TARGET_CARBON
	#define string	std::string
#endif

#ifndef Boolean
typedef unsigned char                   Boolean;
#endif

#define	kFileURLDelimiter	'\r'				// delimits URL's within cache files

Boolean	IsURL( const char* theString, unsigned long theURLLength, char** svcTypeOffset );
Boolean	AllLegalURLChars( const char* theString, unsigned long theURLLength );
Boolean IsLegalURLChar( char theChar );

void GetServiceTypeFromURL(	const char* readPtr,
							unsigned long theURLLength,
							char*	URLType );		// URLType should be pointing at valid memory

#endif
