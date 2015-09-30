/*
 * Copyright (c) 2002-2004,2011,2014 Apple Inc. All Rights Reserved.
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
 * simpleprefs.h - plist support for a bare bones Preferences implementation,
 *                 using only Darwin-avaialble CoreFoundation classes.
 */
 
#ifndef _SECURITY_UTILITIES_SIMPLE_PREFS_H_
#define _SECURITY_UTILITIES_SIMPLE_PREFS_H_

#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFString.h>
#include <security_utilities/utilities.h>
#include <string>

namespace Security {

/* 
 * PropertyList compatible CFDictionary, nonmutable. All keys are CFStringRefs;
 * all values are CF objects.
 */
class Dictionary
{
	NOCOPY(Dictionary)

public:

	/* create from preferences file */
	typedef enum {
		US_User,
		US_System
	} UserOrSystem;
	
protected:
	/* make blank dictionary */
	Dictionary();
	
	/* create from arbitrary file */
	Dictionary(
		const char		*path);	
	
public:
	// factory functions for the dictionaries
	static Dictionary* CreateDictionary(const char* path);
	static Dictionary* CreateDictionary(const char* domain, UserOrSystem userSys, bool loose = false);

public:

	/* create from existing CFDictionary */
	Dictionary(
		CFDictionaryRef	dict);
		
	virtual ~Dictionary();
	
	/* basic lookup */
	const void *getValue(
		CFStringRef		key);
		
	/* lookup, value must be CFString (we check) */
	CFStringRef getStringValue(
		CFStringRef		key);
		
	/* lookup, value must be CFData (we check) */
	CFDataRef getDataValue(
		CFStringRef		key);
		
	/* lookup, value must be CFDictionary (we check) */
	CFDictionaryRef getDictValue(
		CFStringRef		key);

	/* 
	 * Lookup, value is a dictionary, we return value as Dictionary 
	 * if found, else return NULL.
	 */
	Dictionary *copyDictValue(
		CFStringRef		key);
		
	/* 
	 * boolean lookup, tolerate many different forms of value.
	 * Default if value not present is false.
	 */
	bool getBoolValue(
		CFStringRef		key);

	/* basic CF level accessors */
	CFDictionaryRef		dict()	{ return mDict; }
	CFIndex				count();
	
protected:
	void setDict(
		CFDictionaryRef	newDict);
	void initFromFile(
		const char		*path,
		bool			loose = false);	
		
	/* this might be a CFMutableDictionary...use accessors for proper typing */
	CFDictionaryRef		mDict;
};

/*
 * PropertyList compatible CFDictionary, mutable.
 */
class MutableDictionary : public Dictionary
{
	NOCOPY(MutableDictionary)
public:
	/* Create an empty mutable dictionary */
	MutableDictionary();
	
protected:
	/* create from arbitrary file */
	MutableDictionary(
		const char		*filename);	

public:

	static MutableDictionary* CreateMutableDictionary(const char* fileName);
	static MutableDictionary* CreateMutableDictionary(const char *domain, UserOrSystem userSys);

	/* 
	 * Create from existing CFDictionary (OR CFMutableDictionary).
	 * I don't see anyway the CF runtime will let us differentiate an 
	 * immutable from a mutable dictionary here, so caller has to tell us.
	 */
	MutableDictionary(
		CFDictionaryRef	dict,
		bool isMutable);
		
	virtual ~MutableDictionary();
	
	/* 
	 * Lookup, value must be CFDictionary (we check). We return a
	 * mutable copy, or if key not found, we return a new mutable dictionary.
	 * If you want a NULL return if it's not there, use getDictValue(). 
	 */
	CFMutableDictionaryRef getMutableDictValue(
		CFStringRef		key);
		
	/* 
	 * Lookup, value is a dictionary, we return a MutableDictionary, even if 
	 * no value found. 
	 */
	MutableDictionary *copyMutableDictValue(
		CFStringRef key);

	/* 
	 * Basic setter. Does a "replace if present, add if not present" op. 
	 */
	void setValue(
		CFStringRef		key,
		CFTypeRef		val);

	/* 
	 * Set key/value pair, data as CFData in the dictionary but passed 
	 * to us as CSSM_DATA.
	 */ 
	void setDataValue(
		CFStringRef		key,
		const void *valData,
		CFIndex valLength);

	/* remove key/value, if present; not an error if it's not */
	void removeValue(
		CFStringRef		key);
	
	/* write as XML property list, both return true on success */
	bool writePlistToFile(
		const char		*path);
	
	/* write XML property list to preferences file */
	bool writePlistToPrefs(
		const char		*domain,		// e.g., com.apple.security
		UserOrSystem	userSys);		// US_User  : ~/Library/Preferences/domain.plist
										// US_System: /Library/Preferences/domain.plist
										
	CFMutableDictionaryRef mutableDict()  { return (CFMutableDictionaryRef)dict(); }
	
private:
	/* replace mDict with a mutable copy */
	void makeMutable();
};

}	/* end namespace Security */

#endif	/* _SECURITY_UTILITIES_SIMPLE_PREFS_H_ */
