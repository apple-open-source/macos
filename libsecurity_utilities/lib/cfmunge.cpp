/*
 * Copyright (c) 2006-2007 Apple Inc. All Rights Reserved.
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
// CoreFoundation building and parsing functions.
//
// These classes provide a printf/scanf-like interface to nested data structures
// of the Property List Subset of CoreFoundation.
//
#include "cfmunge.h"
#include <security_utilities/cfutilities.h>
#include <security_utilities/errors.h>

namespace Security {


//
// Format codes for consistency
//
#define F_ARRAY			'A'
#define F_BOOLEAN		'B'
#define F_DATA			'X'
#define F_DICTIONARY	'D'
#define F_OBJECT		'O'
#define F_STRING		'S'
#define F_NUMBER		'N'


//
// Initialize a CFMunge. We start out with the default CFAllocator, and
// we do not throw errors.
//
CFMunge::CFMunge(const char *fmt, va_list arg)
	: format(fmt), allocator(NULL), error(noErr)
{
	va_copy(args, arg);
}

CFMunge::~CFMunge()
{
	va_end(args);
}


//
// Skip whitespace and other fluff and deliver the next significant character.
//
char CFMunge::next()
{
	while (*format && (isspace(*format) || *format == ',')) ++format;
	return *format;
}


//
// Locate and consume an optional character
//
bool CFMunge::next(char c)
{
	if (next() == c) {
		++format;
		return true;
	} else
		return false;
}


//
// Process @? parameter specifications.
// The @ operator is used for side effects, and does not return a value.
//
bool CFMunge::parameter()
{
	switch (*++format) {
	case 'A':
		++format;
		allocator = va_arg(args, CFAllocatorRef);
		return true;
	case 'E':
		++format;
		error = va_arg(args, OSStatus);
		return true;
	default:
		return false;
	}
}


//
// The top constructor.
//
CFTypeRef CFMake::make()
{
	while (next() == '@')
		parameter();
	switch (next()) {
	case '\0':
		return NULL;
	case '{':
		return makedictionary();
	case '[':
		return makearray();
	case '\'':
		return makestring();
	case '%':
		return makeformat();
	case '#':
		return makespecial();
	case ']':
	case '}':
		assert(false);	// unexpected
		return NULL;	// error
	default:
		if (isdigit(*format))
			return makenumber();
		else if (isalpha(*format))
			return makestring();
		else {
			assert(false);
			return NULL;
		}
	}
}


CFTypeRef CFMake::makeformat()
{
	++format;
	switch (*format++) {
	case 'b':	// blob (pointer, length)
		{
			const void *data = va_arg(args, const void *);
			size_t length = va_arg(args, size_t);
			return CFDataCreate(allocator, (const UInt8 *)data, length);
		}
	case F_BOOLEAN:	// boolean (with int promotion)
		return va_arg(args, int) ? kCFBooleanTrue : kCFBooleanFalse;
	case 'd':
		return makeCFNumber(va_arg(args, int));
	case 's':
		return CFStringCreateWithCString(allocator, va_arg(args, const char *),
			kCFStringEncodingUTF8);
	case F_OBJECT:
		return CFRetain(va_arg(args, CFTypeRef));
	case 'u':
		return makeCFNumber(va_arg(args, unsigned int));
	default:
		assert(false);
		return NULL;
	}
}


CFTypeRef CFMake::makespecial()
{
	++format;
	switch (*format++) {
	case 'N':
		return kCFNull;
	case 't':
	case 'T':
		return kCFBooleanTrue;
	case 'f':
	case 'F':
		return kCFBooleanFalse;
	default:
		assert(false);
		return NULL;
	}
}


CFTypeRef CFMake::makenumber()
{
	double value = strtod(format, (char **)&format);
	return CFNumberCreate(allocator, kCFNumberDoubleType, &value);
}	


//
// Embedded strings can either be alphanumeric (only), or delimited with single quotes ''.
// No escapes are processed within such quotes. If you want arbitrary string values, use %s.
//
CFTypeRef CFMake::makestring()
{
	const char *start, *end;
	if (*format == '\'') {
		start = ++format;	// next quote
		if (!(end = strchr(format, '\''))) {
			assert(false);
			return NULL;
		}
		format = end + 1;
	} else {
		start = format;
		for (end = start + 1; isalnum(*end); ++end) ;
		format = end;
	}
	return CFStringCreateWithBytes(allocator,
		(const UInt8 *)start, end - start,
		kCFStringEncodingUTF8, false);
}


//
// Construct a CFDictionary
//
CFTypeRef CFMake::makedictionary()
{
	++format;	// next '{'
	next('!');	// indicates mutable (currently always true)
	CFMutableDictionaryRef dict;
	if (next('+')) { // {+%O, => copy dictionary argument, then proceed
		if (next('%') && next('O')) {
			CFDictionaryRef source = va_arg(args, CFDictionaryRef);
			dict = CFDictionaryCreateMutableCopy(allocator, NULL, source);
			if (next('}'))
				return dict;
		} else
			return NULL;	// bad syntax
	} else
		dict = CFDictionaryCreateMutable(allocator, 0,
			&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (add(dict))
		return dict;
	else {
		CFRelease(dict);
		return NULL;
	}
}

CFDictionaryRef CFMake::add(CFMutableDictionaryRef dict)
{
	while (next() != '}') {
		CFTypeRef key = make();
		if (key == NULL)
			return NULL;
		if (!next('=')) {
			CFRelease(key);
			return NULL;
		}
		if (CFTypeRef value = make()) {
			CFDictionaryAddValue(dict, key, value);
			CFRelease(key);
			CFRelease(value);
		} else {
			CFRelease(key);
			return NULL;
		}
	}
	++format;
	return dict;
}


CFDictionaryRef CFMake::addto(CFMutableDictionaryRef dict)
{
	if (next('{'))
		return add(dict);
	else {
		assert(false);
		return NULL;
	}
}


//
// Construct a CFArray
//
CFTypeRef CFMake::makearray()
{
	++format;	// next '['
	next('!');	// indicates mutable (currently always)
	CFMutableArrayRef array = makeCFMutableArray(0);
	while (next() != ']') {
		CFTypeRef value = make();
		if (value == NULL) {
			CFRelease(array);
			return NULL;
		}
		CFArrayAppendValue(array, value);
		CFRelease(value);
	}
	++format;
	return array;
}


//
// A CFScan processes its format by parsing through an existing CF object
// structure, matching and extracting values as directed. Note that CFScan
// is a structure (tree) scanner rather than a linear parser, and will happily
// parse out a subset of the input object graph.
//
class CFScan : public CFMake {
public:
	CFScan(const char *format, va_list args)
		: CFMake(format, args), suppress(false) { }
	
	bool scan(CFTypeRef obj);
	CFTypeRef dictpath(CFTypeRef obj);
	
protected:
	bool scandictionary(CFDictionaryRef obj);
	bool scanarray(CFArrayRef obj);
	bool scanformat(CFTypeRef obj);
	
	enum Typescan { fail = -1, more = 0, done = 1 };
	Typescan typescan(CFTypeRef obj, CFTypeID type);

	template <class Value>
	bool scannumber(CFTypeRef obj);
	
	template <class Type>
	void store(Type value);
	
	bool suppress;				// output suppression
};


//
// Master scan function
//
bool CFScan::scan(CFTypeRef obj)
{
	while (next() == '@')
		parameter();
	switch (next()) {
	case '\0':
		return true;	// done, okay
	case '{':
		if (obj && CFGetTypeID(obj) != CFDictionaryGetTypeID())
			return false;
		return scandictionary(CFDictionaryRef(obj));
	case '[':
		if (obj && CFGetTypeID(obj) != CFArrayGetTypeID())
			return false;
		return scanarray(CFArrayRef(obj));
	case '%':	// return this value in some form
		return scanformat(obj);
	case '=':	// match value
		{
			++format;
			CFTypeRef match = make();
			bool rc = CFEqual(obj, match);
			CFRelease(match);
			return rc;
		}
	case ']':
	case '}':
		assert(false);	// unexpected
		return false;
	default:
		assert(false);
		return false;
	}
}


//
// Primitive type-match helper.
// Ensures the object has the CF runtime type required, and processes
// the %?o format (return CFTypeRef) and %?n format (ignore value).
//
CFScan::Typescan CFScan::typescan(CFTypeRef obj, CFTypeID type)
{
	if (obj && CFGetTypeID(obj) != type)
		return fail;
	switch (*++format) {
	case F_OBJECT:	// return CFTypeRef
		++format;
		store<CFTypeRef>(obj);
		return done;
	case 'n':	// suppress assignment
		++format;
		return done;
	default:
		return more;
	}
}


//
// Store a value into the next varargs slot, unless output suppression is on.
//
template <class Type>
void CFScan::store(Type value)
{
	if (!suppress)
		*va_arg(args, Type *) = value;
}


//
// Convert a CFNumber to an external numeric form
//
template <class Value>
bool CFScan::scannumber(CFTypeRef obj)
{
	++format;	// consume format code
	if (!obj)
		return true; // suppressed, okay
	if (CFGetTypeID(obj) != CFNumberGetTypeID())
		return false;
	store<Value>(cfNumber<Value>(CFNumberRef(obj)));
	return true;
}


//
// Process % scan forms.
// This delivers the object value, scanf-style, somehow.
//
bool CFScan::scanformat(CFTypeRef obj)
{
	switch (*++format) {
	case F_OBJECT:
		store<CFTypeRef>(obj);
		return true;
	case F_ARRAY:	// %a*
		return typescan(obj, CFArrayGetTypeID()) == done;
	case F_BOOLEAN:
		if (Typescan rc = typescan(obj, CFBooleanGetTypeID()))
			return rc == done;
		switch (*format) {
		case 'f':	// %Bf - two arguments (value, &variable)
			{
				unsigned flag = va_arg(args, unsigned);
				unsigned *value = va_arg(args, unsigned *);
				if (obj == kCFBooleanTrue && !suppress)
					*value |= flag;
				return true;
			}
		default:	// %b - CFBoolean as int boolean
			store<int>(obj == kCFBooleanTrue);
			return true;
		}
	case F_DICTIONARY:
		return typescan(obj, CFDictionaryGetTypeID()) == done;
	case 'd':	// %d - int
		return scannumber<int>(obj);
	case F_NUMBER:
		return typescan(obj, CFNumberGetTypeID()) == done;
	case F_STRING:
	case 's':
		if (Typescan rc = typescan(obj, CFStringGetTypeID()))
			return rc == done;
		// %s
		store<std::string>(cfString(CFStringRef(obj)));
		return true;
	case 'u':
		return scannumber<unsigned int>(obj);
	case F_DATA:
		return typescan(obj, CFDataGetTypeID()) == done;
	default:
		assert(false);
		return false;
	}
}


bool CFScan::scandictionary(CFDictionaryRef obj)
{
	++format;	// skip '{'
	while (next() != '}') {
		bool optional = next('?');
		if (CFTypeRef key = make()) {
			bool oldSuppress = suppress;
			CFTypeRef elem = obj ? CFDictionaryGetValue(obj, key) : NULL;
			if (elem || optional) {
				suppress |= (elem == NULL);
				if (next('=')) {
					if (scan(elem)) {
						suppress = oldSuppress;	// restore
						CFRelease(key);
						continue;
					}
				}
			}
			CFRelease(key);
			return false;
		} else {
			assert(false);	// bad format
			return false;
		}
	}
	return true;
}


bool CFScan::scanarray(CFArrayRef obj)
{
	++format;	// skip '['
	CFIndex length = CFArrayGetCount(obj);
	for (int pos = 0; pos < length; ++pos) {
		if (next() == ']')
			return true;
		if (!scan(CFArrayGetValueAtIndex(obj, pos)))
			return false;
	}
	return false;	// array length exceeded
}


//
// Run down a "dictionary path", validating heavily.
//
CFTypeRef CFScan::dictpath(CFTypeRef obj)
{
	while (next()) {	// while we've got more text
		next('.');		// optional
		if (obj == NULL || CFGetTypeID(obj) != CFDictionaryGetTypeID())
			return NULL;
		CFTypeRef key = make();
		obj = CFDictionaryGetValue(CFDictionaryRef(obj), key);
		CFRelease(key);
	}
	return obj;
}


//
// The public functions
//
CFTypeRef cfmake(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	CFTypeRef result = CFMake(format, args).make();
	va_end(args);
	return result;
}

CFTypeRef vcfmake(const char *format, va_list args)
{
	return CFMake(format, args).make();
}

CFDictionaryRef cfadd(CFMutableDictionaryRef dict, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	CFDictionaryRef result = CFMake(format, args).addto(dict);
	va_end(args);
	return result;
}


bool cfscan(CFTypeRef obj, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	bool result = vcfscan(obj, format, args);
	va_end(args);
	return result;
}

bool vcfscan(CFTypeRef obj, const char *format, va_list args)
{
	return CFScan(format, args).scan(obj);
}


CFTypeRef cfget(CFTypeRef obj, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	CFTypeRef result = vcfget(obj, format, args);
	va_end(args);
	return result;
}

CFTypeRef vcfget(CFTypeRef obj, const char *format, va_list args)
{
	return CFScan(format, args).dictpath(obj);
}

}	// end namespace Security
