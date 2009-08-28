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
// CoreFoundation building and parsing functions
//
#ifndef _H_CFMUNGE
#define _H_CFMUNGE

#include <security_utilities/cfutilities.h>
#include <CoreFoundation/CoreFoundation.h>
#include <cstdarg>

namespace Security {


//
// Common interface to Mungers.
// A CFMunge provides a one-pass, non-resettable scan through a format string,
// performing various actions on the way.
//
class CFMunge {
public:
	CFMunge(const char *fmt, va_list arg);
	~CFMunge();

protected:
	char next();
	bool next(char c);
	
	bool parameter();
	
protected:
	const char *format;
	va_list args;
	CFAllocatorRef allocator;
	OSStatus error;
};


//
// A CFMake is a CFMunge for making CF data structures.
//
class CFMake : public CFMunge {
public:
	CFMake(const char *fmt, va_list arg) : CFMunge(fmt, arg) { }
	
	CFTypeRef make();
	CFDictionaryRef addto(CFMutableDictionaryRef dict);
	
protected:
	CFTypeRef makedictionary();
	CFTypeRef makearray();
	CFTypeRef makenumber();
	CFTypeRef makestring();
	CFTypeRef makeformat();
	CFTypeRef makespecial();

	CFDictionaryRef add(CFMutableDictionaryRef dict);
};


//
// Make a CF object following a general recipe
//
CFTypeRef cfmake(const char *format, ...);
CFTypeRef vcfmake(const char *format, va_list args);

template <class CFType>
CFType cfmake(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	CFType result = CFType(vcfmake(format, args));
	va_end(args);
	return result;
}

CFDictionaryRef cfadd(CFMutableDictionaryRef dict, const char *format, ...);


//
// Parse out parts of a CF object following a general recipe.
// Cfscan returns false on error; cfget throws.
//
bool cfscan(CFTypeRef source, const char *format, ...);
bool vcfscan(CFTypeRef source, const char *format, va_list args);

CFTypeRef cfget(CFTypeRef source, const char *format, ...);
CFTypeRef vcfget(CFTypeRef source, const char *format, va_list args);

template <class CFType>
CFType cfget(CFTypeRef source, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	CFType result = CFType(vcfget(source, format, args));
	va_end(args);
	return CFTraits<CFType>::check(result) ? result : NULL;
}

template <class CFType>
class CFTemp : public CFRef<CFType> {
public:
	CFTemp(const char *format, ...)
	{
		va_list args;
		va_start(args, format);
		this->take(CFType(vcfmake(format, args)));
		va_end(args);
	}
};


}	// end namespace Security

#endif //_H_CFMUNGE
