/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All Rights Reserved.
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

/* 
 * simpleprefs.cpp - plist support for a bare bones Preferences implementation,
 *                 using only Darwin-avaialble CoreFoundation classes.
 */
 
#include "simpleprefs.h"
#include "errors.h"
#include <sys/param.h>
#include <stdlib.h>
#include <assert.h>
#include <stdexcept>
#include <security_utilities/debugging.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFURLAccess.h>
#include <CoreFoundation/CFPropertyList.h>
#include <sys/stat.h>

#define prefsDebug(args...)		secdebug("simpleprefs", ## args)

#define kSecUserPrefsDir			"Library/Preferences"		/* relative to $HOME */
#define kSecSystemPrefsDir			"/Library/Preferences"

#pragma mark ----- (immutable) Dictionary -----

static void pathForDomain(const char *domain, Dictionary::UserOrSystem userSys, std::string &path)
{
	path.clear();
	if(userSys == Dictionary::US_User) {
		const char *home = getenv("HOME");
		if(home == NULL) {
			home = "";
		}
		path = std::string(home) + "/" + kSecUserPrefsDir + "/" + domain + ".plist";
	}
	else {
		path = std::string(kSecSystemPrefsDir) + "/" + domain + ".plist";
	}
}

static bool FileExists(const char* s)
{
	// this isn't very efficient, either, but orders are to get rid of exceptions...
	struct stat st;
	int result = stat(s, &st);
	return result == 0;
}

// use factory functions to create the dictionaries so that we can test for the presence of the dictionaries
// without throwing
Dictionary* Dictionary::CreateDictionary(const char* path)
{
	if (!FileExists(path))
	{
		return NULL;
	}
	else
	{
		return new Dictionary(path);
	}
}

Dictionary* Dictionary::CreateDictionary(const char* domain, UserOrSystem userSys, bool loose)
{
	std::string path;
	pathForDomain(domain, userSys, path);
	bool exists = FileExists(path.c_str());
	if (!loose && !exists)
	{
		return NULL;
	}
	
	if (!exists)
	{
		return new Dictionary();
	}
	
	return new Dictionary(path.c_str());
}

Dictionary::Dictionary() : mDict(NULL)
{
}

Dictionary::Dictionary(
	const char		*path)
		: mDict(NULL)
{
	initFromFile(path);
}

Dictionary::Dictionary(
	CFDictionaryRef	dict)
		: mDict(dict)
{
	if (mDict)
		CFRetain(mDict);
}

Dictionary::~Dictionary()
{
	if(mDict) {
		CFRelease(mDict);
	}
}

/* basic lookup */
const void *Dictionary::getValue(
	CFStringRef key)
{
	return CFDictionaryGetValue(dict(), key);
}
	
/* lookup, value must be CFString (we check) */
CFStringRef Dictionary::getStringValue(
	CFStringRef	key)
{
	CFStringRef val = (CFStringRef)CFDictionaryGetValue(dict(), key);
	if(val == NULL) {
		return NULL;
	}
	if(CFGetTypeID(val) != CFStringGetTypeID()) {
		return NULL;
	}
	return val;
}
	
/* lookup, value must be CFData (we check) */
CFDataRef Dictionary::getDataValue(
	CFStringRef	key)
{
	CFDataRef val = (CFDataRef)CFDictionaryGetValue(dict(), key);
	if(val == NULL) {
		return NULL;
	}
	if(CFGetTypeID(val) != CFDataGetTypeID()) {
		return NULL;
	}
	return val;
}
	
/* lookup, value must be CFDictionary (we check) */
CFDictionaryRef Dictionary::getDictValue(
	CFStringRef key)
{
	CFDictionaryRef val = (CFDictionaryRef)CFDictionaryGetValue(dict(), key);
	if(val == NULL) {
		return NULL;
	}
	if(CFGetTypeID(val) != CFDictionaryGetTypeID()) {
		return NULL;
	}
	return val;
}
	
/* 
 * Lookup, value is a dictionary, we return value as Dictionary 
 * if found, else return NULL.
 */
Dictionary *Dictionary::copyDictValue(
	CFStringRef key)
{
	CFDictionaryRef cfDict = getDictValue(key);
	if(cfDict == NULL) {
		return NULL;
	}
	Dictionary *rtnDict = new Dictionary(cfDict);
	/*
	 * mDict has one ref count
	 * cfDict has one ref count 
	 */
	return rtnDict;
}

/* 
 * boolean lookup, tolerate many different forms of value.
 * Default if value not present is false.
 */
bool Dictionary::getBoolValue(
	CFStringRef key)
{
	CFTypeRef val = CFDictionaryGetValue(dict(), key);
	if(val == NULL) {
		return false;
	}
	CFComparisonResult res;
	if(CFGetTypeID(val) == CFStringGetTypeID()) {
		res = CFStringCompare((CFStringRef)val, CFSTR("YES"), 
				kCFCompareCaseInsensitive);
		if(res == kCFCompareEqualTo) {
			return true;
		}
		else {
			return false;
		}
	}
	if(CFGetTypeID(val) == CFBooleanGetTypeID()) {
		return CFBooleanGetValue((CFBooleanRef)val) ? true : false;
	}
	if(CFGetTypeID(val) == CFNumberGetTypeID()) {
		char cval = 0;
		CFNumberGetValue((CFNumberRef)val, kCFNumberCharType, &cval);
		return (cval == 0) ? false : true;
	}
	return false;
}

CFIndex Dictionary::count()
{
	return CFDictionaryGetCount(dict());
}

void Dictionary::setDict(
	CFDictionaryRef	newDict)
{
	if(mDict != NULL)
		CFRelease(mDict);
	mDict = newDict;
	CFRetain(mDict);
}

/* fundamental routine to init from a plist file; throws a UnixError on error */
void Dictionary::initFromFile(
	const char *path,
	bool		loose /* = false */)
{
	if(mDict != NULL) {
		CFRelease(mDict);
		mDict = NULL;
	}
	CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)path, 
		strlen(path), false);
	if(url == NULL) {
        UnixError::throwMe(EIO);
	}
	
	CFDataRef fileData = NULL;
	CFPropertyListRef propList = NULL;
	CFStringRef errorString = NULL;
	SInt32 errorCode;
	
	Boolean success = CFURLCreateDataAndPropertiesFromResource(
		NULL,
		url,
		&fileData,   
		NULL,			// properties
		NULL,			// desiredProperties
		&errorCode);
	CFRelease(url);
	if(success) {
		propList = CFPropertyListCreateFromXMLData(
			NULL,
			fileData,
			kCFPropertyListImmutable,
			&errorString);
		if(propList != NULL) {
			/* 
			 * Note don't use setDict() here to avoid the extra
			 * refcount that would entail. We own the dictionary now.
			 */
			mDict = (CFDictionaryRef)propList;
		}
		else {
			success = false;
		}
	}
	if(fileData != NULL) {
		CFRelease(fileData);
	}
	if(errorString != NULL) {
		CFRelease(errorString);
	}
	if(!success) {
		if (loose)
			return;
		else
			UnixError::throwMe(EIO);
	}
}

#pragma mark ----- Mutable Dictionary -----

// factory functions
MutableDictionary* MutableDictionary::CreateMutableDictionary(const char* fileName)
{
	std::string path;
	
	if (!FileExists(path.c_str()))
	{
		return NULL;
	}
	else
	{
		return new MutableDictionary(path.c_str());
	}
}

MutableDictionary* MutableDictionary::CreateMutableDictionary(const char *domain, UserOrSystem userSys)
{
	std::string path;
	pathForDomain(domain, userSys, path);
	
	if (!FileExists(path.c_str()))
	{
		return NULL;
	}
	
	return new MutableDictionary(path.c_str());
}

/* Create an empty mutable dictionary */
MutableDictionary::MutableDictionary()
	: Dictionary((CFDictionaryRef)CFDictionaryCreateMutable(NULL, 0,
		&kCFCopyStringDictionaryKeyCallBacks, 
		&kCFTypeDictionaryValueCallBacks))
{
	/* lose one of those two retain counts.... */
	CFRelease(mDict);
}

MutableDictionary::MutableDictionary(
	const char		*filename)
		: Dictionary(filename)
{
	/* 
	 * Dictionary's contructor read the plist from disk. Now 
	 * replace that dictionary with a mutable copy.
	 */
	makeMutable();
}

/* 
 * Create from existing CFDictionary (OR CFMutableDictionary).
 * I don't see any way the CF runtime will let us differentiate an 
 * immutable from a mutable dictionary here, so caller has to tell us.
 */
MutableDictionary::MutableDictionary(
	CFDictionaryRef	dict,
	bool isMutable)
		: Dictionary(dict)
{
	if(!isMutable) {
		makeMutable();
	}
}
	
MutableDictionary::~MutableDictionary()
{
	/* nothing for now */
}

/* 
 * Lookup, value must be CFDictionary (we check). We return a
 * mutable copy, or if key not found, we return a new mutable dictionary
 * with a ref count of one. 
 * If you want a NULL return if it's not there, use getDictValue(). 
 */
CFMutableDictionaryRef MutableDictionary::getMutableDictValue(
	CFStringRef key)
{
	CFDictionaryRef dict = getDictValue(key);
	if(dict == NULL) {
		prefsDebug("getMutableDictValue returning new empty dict; this %p", this);
		return CFDictionaryCreateMutable(NULL, 0,
					&kCFCopyStringDictionaryKeyCallBacks, 
					&kCFTypeDictionaryValueCallBacks);	
	}
	else {
		prefsDebug("getMutableDictValue returning copy; this %p", this);
		return CFDictionaryCreateMutableCopy(NULL, 0, dict);
	}
}
	
/* 
 * Lookup, value is a dictionary, we return a MutableDictionary, even if 
 * no value found. 
 */
MutableDictionary *MutableDictionary::copyMutableDictValue(
	CFStringRef key)
{
	CFMutableDictionaryRef cfDict = getMutableDictValue(key);
	assert(CFGetRetainCount(cfDict) == 1);
	MutableDictionary *rtnDict = new MutableDictionary(cfDict, true);
	CFRelease(cfDict);
	/* rtnDict->mDict now holds the only ref count */
	return rtnDict;
}

/* 
 * Basic setter. Does a replace if present, add if not present op. 
 */
void MutableDictionary::setValue(
	CFStringRef		key,
	CFTypeRef		val)
{
	CFDictionarySetValue(mutableDict(), key, val);
}

/* 
 * Set key/value pair, data as CFData in the dictionary but passed to us as CSSM_DATA.
 */ 
void MutableDictionary::setDataValue(
	CFStringRef		key,
	const void *valData, CFIndex valLength)
{
	CFDataRef cfVal = CFDataCreate(NULL, reinterpret_cast<const UInt8 *>(valData), valLength);
	setValue(key, cfVal);
	CFRelease(cfVal);
}

/* remove key/value, if present; not an error if it's not */
void MutableDictionary::removeValue(
	CFStringRef		key)
{
	CFDictionaryRemoveValue(mutableDict(), key);
}

/* write as XML property list, both return true on success */
bool MutableDictionary::writePlistToFile(
	const char	*path)
{
	assert(mDict != NULL);
	CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)path, 
		strlen(path), false);
	if(url == NULL) {
        UnixError::throwMe(EIO);
	}

	CFDataRef xmlData = CFPropertyListCreateXMLData(NULL, dict());
	bool ourRtn = false;
	SInt32 errorCode;
	if(xmlData == NULL) {
		goto errOut;
	}
	if(CFURLWriteDataAndPropertiesToResource(url, xmlData, NULL, &errorCode)) {
		ourRtn = true;
	}
errOut:
	if(url) {
		CFRelease(url);
	}
	if(xmlData) {
		CFRelease(xmlData);
	}
	return ourRtn;
}

/* write XML property list to preferences file */
bool MutableDictionary::writePlistToPrefs(
	const char		*domain,		// e.g., com.apple.security
	UserOrSystem	userSys)		// US_User  : ~/Library/Preferences/domain.plist
									// US_System: /Library/Preferences/domain.plist
{
	std::string path;
	pathForDomain(domain, userSys, path); 
	return writePlistToFile(path.c_str());
}

/* 
 * Called after Dictionary reads plist from file, resulting in an immutable
 * mDict. We replace that with a mutable copy.
 */
void MutableDictionary::makeMutable()
{
	CFMutableDictionaryRef mutDict = CFDictionaryCreateMutableCopy(NULL, 0, dict());
	if(mutDict == NULL) {
		throw std::bad_alloc();
	}
	setDict(mutDict);
	/* we own the dictionary now */
	CFRelease(mutDict);
}

