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
#include <cstdarg>
#include <vector>


namespace Security {


ModuleNexus<CFEmptyArray> cfEmptyArray;

CFEmptyArray::CFEmptyArray()
{
	mArray = CFArrayCreate(NULL, NULL, 0, NULL);
}


//
// Turn a C(++) string into a CFURLRef indicating a file: path
//
CFURLRef makeCFURL(const char *s, bool isDirectory, CFURLRef base)
{
	if (base)
		return CFURLCreateWithFileSystemPathRelativeToBase(NULL,
			CFTempString(s), kCFURLPOSIXPathStyle, isDirectory, base);
	else
		return CFURLCreateWithFileSystemPath(NULL,
			CFTempString(s), kCFURLPOSIXPathStyle, isDirectory);
}

CFURLRef makeCFURL(CFStringRef s, bool isDirectory, CFURLRef base)
{
	if (base)
		return CFURLCreateWithFileSystemPathRelativeToBase(NULL, s, kCFURLPOSIXPathStyle, isDirectory, base);
	else
		return CFURLCreateWithFileSystemPath(NULL, s, kCFURLPOSIXPathStyle, isDirectory);
}


//
// CFMallocData objects
//
CFMallocData::operator CFDataRef ()
{
	CFDataRef result = makeCFDataMalloc(mData, mSize);
	if (!result)
		CFError::throwMe();
	mData = NULL;	// release ownership
	return result;
}


//
// Make CFDictionaries from stuff
//
CFDictionaryRef makeCFDictionary(unsigned count, ...)
{
	CFTypeRef keys[count], values[count];
	va_list args;
	va_start(args, count);
	for (unsigned n = 0; n < count; n++) {
		keys[n] = va_arg(args, CFTypeRef);
		values[n] = va_arg(args, CFTypeRef);
	}
	va_end(args);
	return CFDictionaryCreate(NULL, (const void **)keys, (const void **)values, count,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

CFMutableDictionaryRef makeCFMutableDictionary()
{
	if (CFMutableDictionaryRef r = CFDictionaryCreateMutable(NULL, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks))
		return r;
	CFError::throwMe();
}

CFMutableDictionaryRef makeCFMutableDictionary(unsigned count, ...)
{
	CFMutableDictionaryRef dict = makeCFMutableDictionary();
	if (count > 0) {
		va_list args;
		va_start(args, count);
		for (unsigned n = 0; n < count; n++) {
			CFTypeRef key = va_arg(args, CFTypeRef);
			CFTypeRef value = va_arg(args, CFTypeRef);
			CFDictionaryAddValue(dict, key, value);
		}
		va_end(args);
	}
	return dict;
}

CFMutableDictionaryRef makeCFMutableDictionary(CFDictionaryRef dict)
{
	if (CFMutableDictionaryRef r = CFDictionaryCreateMutableCopy(NULL, 0, dict))
		return r;
	CFError::throwMe();
}

CFDictionaryRef makeCFDictionaryFrom(CFDataRef data)
{
	if (data) {
		CFPropertyListRef plist = CFPropertyListCreateFromXMLData(NULL, data,
			kCFPropertyListImmutable, NULL);
		if (plist && CFGetTypeID(plist) != CFDictionaryGetTypeID())
			CFError::throwMe();
		return CFDictionaryRef(plist);
	} else
		return NULL;
	
}

CFDictionaryRef makeCFDictionaryFrom(const void *data, size_t length)
{
	return makeCFDictionaryFrom(CFTempData(data, length).get());
}


//
// Turn a CFString into a UTF8-encoded C++ string.
// If release==true, the argument will be CFReleased even in case of error.
//
string cfString(CFStringRef str)
{
	if (!str)
		return "";
	// quick path first
	if (const char *s = CFStringGetCStringPtr(str, kCFStringEncodingUTF8)) {
		return s;
	}
	
	// need to extract into buffer
	string ret;
	CFIndex length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8);
	std::vector<char> buffer;
	buffer.resize(length + 1);
	if (CFStringGetCString(str, &buffer[0], length + 1, kCFStringEncodingUTF8))
		ret = &buffer[0];
	return ret;
}

string cfStringRelease(CFStringRef inStr)
{
	CFRef<CFStringRef> str(inStr);
	return cfString(str);
}

string cfString(CFURLRef inUrl)
{
	if (!inUrl)
		CFError::throwMe();
	
	UInt8 buffer[PATH_MAX+1];
	if (CFURLGetFileSystemRepresentation(inUrl, true, buffer, sizeof(buffer)))
		return string(reinterpret_cast<char *>(buffer));
	else
		CFError::throwMe();
}
    
string cfStringRelease(CFURLRef inUrl)
{
	CFRef<CFURLRef> bundle(inUrl);
	return cfString(bundle);
}
    
string cfString(CFBundleRef inBundle)
{
	if (!inBundle)
		CFError::throwMe();
	return cfStringRelease(CFBundleCopyBundleURL(inBundle));
}

string cfStringRelease(CFBundleRef inBundle)
{
	CFRef<CFBundleRef> bundle(inBundle);
	return cfString(bundle);
}

    
string cfString(CFTypeRef it, OSStatus err)
{
	if (it == NULL)
		MacOSError::throwMe(err);
	CFTypeID id = CFGetTypeID(it);
	if (id == CFStringGetTypeID())
		return cfString(CFStringRef(it));
	else if (id == CFURLGetTypeID())
		return cfString(CFURLRef(it));
	else if (id == CFBundleGetTypeID())
		return cfString(CFBundleRef(it));
	else
		return cfString(CFCopyDescription(it), true);
}


//
// CFURLAccess wrappers for specific purposes
//
CFDataRef cfLoadFile(CFURLRef url)
{
	assert(url);
	CFDataRef data;
	SInt32 error;
	if (CFURLCreateDataAndPropertiesFromResource(NULL, url,
		&data, NULL, NULL, &error)) {
		return data;
	} else {
		secdebug("cfloadfile", "failed to fetch %s error=%d", cfString(url).c_str(), int(error));
		return NULL;
	}
}


//
// CFArray creators
//
CFArrayRef makeCFArray(CFIndex count, ...)
{
	CFTypeRef elements[count];
	va_list args;
	va_start(args, count);
	for (CFIndex n = 0; n < count; n++)
		elements[n] = va_arg(args, CFTypeRef);
	va_end(args);
	return CFArrayCreate(NULL, elements, count, &kCFTypeArrayCallBacks);
}

CFMutableArrayRef makeCFMutableArray(CFIndex count, ...)
{
	CFMutableArrayRef array = CFArrayCreateMutable(NULL, count, &kCFTypeArrayCallBacks);
	va_list args;
	va_start(args, count);
	for (CFIndex n = 0; n < count; n++)
		CFArrayAppendValue(array, va_arg(args, CFTypeRef));
	va_end(args);
	return array;
}


}	// end namespace Security
