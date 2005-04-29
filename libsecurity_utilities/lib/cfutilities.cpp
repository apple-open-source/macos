/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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


//
// CoreFoundation related utilities
//
#include <security_utilities/cfutilities.h>
#include <security_utilities/errors.h>
#include <security_utilities/debugging.h>


namespace Security {


//
// Turn a C(++) string into a CFURLRef indicating a file: path
//
CFURLRef makeCFURL(const char *s, bool isDirectory)
{
	return CFURLCreateWithFileSystemPath(NULL,
		CFRef<CFStringRef>(makeCFString(s)),
		kCFURLPOSIXPathStyle, isDirectory);
}


//
// Turn a CFString into a UTF8-encoded C++ string.
// If release==true, the argument will be CFReleased even in case of error.
//
string cfString(CFStringRef inStr, bool release)
{
	if (!inStr)
		CFError::throwMe();
	CFRef<CFStringRef> str(inStr);	// hold ref
	if (!release)
		CFRetain(inStr);	// compensate for release on exit

	// NULL translates (cleanly) to empty
	if (str == NULL)
		return "";

	// quick path first
	if (const char *s = CFStringGetCStringPtr(str, kCFStringEncodingUTF8)) {
		return s;
	}
	
	// need to extract into buffer
	string ret;
	CFIndex length = CFStringGetLength(str);	// in 16-bit character units
	char *buffer = new char[6 * length + 1];	// pessimistic
	if (CFStringGetCString(str, buffer, 6 * length + 1, kCFStringEncodingUTF8))
		ret = buffer;
	delete[] buffer;
	return ret;
}

string cfString(CFURLRef inUrl, bool release)
{
	if (!inUrl)
		CFError::throwMe();
	CFRef<CFURLRef> url(inUrl);	// hold ref
	if (!release)
		CFRetain(inUrl);	// compensate for release on exit
	
	UInt8 buffer[PATH_MAX+1];
	if (CFURLGetFileSystemRepresentation(url, true, buffer, sizeof(buffer)))
		return string(reinterpret_cast<char *>(buffer));
	else
		CFError::throwMe();
}

string cfString(CFBundleRef inBundle, bool release)
{
	if (!inBundle)
		CFError::throwMe();
	CFRef<CFBundleRef> bundle(inBundle);
	if (!release)
		CFRetain(inBundle);	// compensate for release on exit
	
	return cfString(CFBundleCopyBundleURL(bundle), true);
}


//
// Get numbers ouf of a CFNumber
//
uint32_t cfNumber(CFNumberRef number)
{
	uint32_t value;
	if (CFNumberGetValue(number, kCFNumberSInt32Type, &value))
		return value;
	else
		CFError::throwMe();
}

uint32_t cfNumber(CFNumberRef number, uint32_t defaultValue)
{
	uint32_t value;
	if (CFNumberGetValue(number, kCFNumberSInt32Type, &value))
		return value;
	else
		return defaultValue;
}


}	// end namespace Security
