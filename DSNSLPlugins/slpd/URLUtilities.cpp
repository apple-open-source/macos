/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
 
/*!
 *  @header URLUtilities
 *  Some simple URL utilities
*/

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "URLUtilities.h"

// carbon d6-d9 workarounds
#if TARGET_CARBON
	#define charisalpha(c)		( ((c) >= 'A' && (c) <= 'Z') || ((c) >= 'a' && (c) <= 'z') )
	#define charisdigit(c)		( (c) >= '0' && (c) <= '9' )
	#define charisprint(c)		( (c) >= ' ' )
	#define charisxdigit(c)		( charisdigit((c)) || ( (c) >= 'a' && (c) <= 'f' ) || ( (c) >= 'A' && (c) <= 'F' ) )
#else
	#define charisalpha			isalpha
	#define charisdigit			isdigit
	#define charisprint			isprint
	#define charisxdigit		isxdigit
#endif

/*********
 * IsURL *
 *********
 
 Determines if given string is a URL, and if so, returns the offset to "://<address>", which gives
 us the length of the service type string as well as a way to find the address string.
 
 - Update, changed this to look for ":/" instead since the url "afp:/at/... is valid"
*/

Boolean	IsURL( const char* theString, unsigned long theURLLength, char** svcTypeOffset )
{

	Boolean		foundURL=false;
	
	if ( AllLegalURLChars(theString, theURLLength) )
	{
		*svcTypeOffset = strstr(theString, ":/");			// look for the interesting tag chars
		if ( *svcTypeOffset != NULL)
			foundURL = true;
	}
	
	return foundURL;
	
}


/********************
 * AllLegalURLChars *
 ********************
 
 Returns whether all chars in given RR are legal for use in a URL.  Legal chars are the printables, except
 for space, ", < and >.  
*/

Boolean	AllLegalURLChars( const char* theString, unsigned long theURLLength )
{

	Boolean	isLegal = true;
	
	if ( theString )				// make sure we have a string to examine
	{		
		for (long i = theURLLength - 1; i>=0 && isLegal; i--)
			isLegal = IsLegalURLChar( theString[i] );
	}
	else
		isLegal = false;
	
	return isLegal;
	
}


/********************
 * IsLegalURLChar *
 ********************
 Returns whether a char is legal for use in a URL.  Legal chars are the printables, except
 for space, ", < and >.  
*/

Boolean IsLegalURLChar( const char theChar )
{
	return ( charisprint( theChar ) && ( theChar != ' ' && theChar != '\"' && theChar != '<' && theChar != '>' ) );
}


/*************************
 * GetServiceTypeFromURL *
 *************************
 
 Return the service type from the URL.  Format of the URL is:
 <service type>://<address>, so just grab the first part.
*/

void GetServiceTypeFromURL(	const char* readPtr,
							unsigned long theURLLength,
							char*	URLType )
{

	char*	curOffset;
	unsigned short	typeLen;
	
	if ( IsURL( readPtr, theURLLength, &curOffset))
	{
		typeLen = curOffset - readPtr;
		::memcpy(URLType, (unsigned char*)readPtr, typeLen);
		URLType[typeLen] = '\0';
	}
	else
	{
		URLType[0] = '\0';				// nothing here to find
	}

}
