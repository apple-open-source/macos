/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All Rights Reserved.
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


//
// CoreFoundation related utilities
//
#include <Security/cfutilities.h>
#include <Security/debugging.h>


namespace Security {


//
// Turn a CFString into a UTF8-encoded C++ string
//
string cfString(CFStringRef str)
{
	// NULL translates (cleanly) to empty
	if (str == NULL)
		return "";

	// quick path first
	if (const char *s = CFStringGetCStringPtr(str, kCFStringEncodingUTF8))
		return s;
	
	// need to extract into buffer
	string ret;
	CFIndex length = CFStringGetLength(str);	// in 16-bit character units
	char *buffer = new char[6 * length + 1];	// pessimistic
	if (CFStringGetCString(str, buffer, 6 * length + 1, kCFStringEncodingUTF8))
		ret = buffer;
	delete[] buffer;
	return ret;
}


}	// end namespace Security
