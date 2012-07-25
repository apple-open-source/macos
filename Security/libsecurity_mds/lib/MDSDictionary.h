/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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


/*
   File:      MDSDictionary.h

   Contains:  Internal representation of one MDS info file.  

   Copyright: (c) 2001 Apple Computer, Inc., all rights reserved.
*/

#ifndef _MDS_DICTIONARY_H_
#define _MDS_DICTIONARY_H_  1

#include <Security/cssmtype.h>
#include "MDSSession.h"
#include "MDSAttrStrings.h"
#include <CoreFoundation/CoreFoundation.h>

namespace Security
{

class MDSDictionary
{
public:
	/* heavyweight constructor from file */
	MDSDictionary(
		CFURLRef fileUrl,
		CFStringRef subdir,
		const char *fullPath);

	/* lightweight constructor from existing CFDictionary */
	MDSDictionary(
		CFDictionaryRef theDict);
	
	~MDSDictionary();
	
	/* 
	 * Lookup by either C string or CFStringRef. Optionally checks for
	 * CFTypeID of resulting value. Both return NULL on error (either key not
	 * found or wrong CFTypeID).
	 */
	const void *lookup(
		const char *key,
		bool checkType = false,		// since we really don't know if 0 is a valid type
		CFTypeID type = 0);
	const void *lookup(
		CFStringRef key,
		bool checkType = false,
		CFTypeID type = 0);
	
	/*
	 * Common means to perform a lookup in a dictionary given a C-string key and
	 * placing the value - if present - in a CSSM_DB_ATTRIBUTE_DATA. Any errors
	 * are only logged via MPDebug. Returns true if the value was found and 
	 * successfully placed in supplied CSSM_DB_ATTRIBUTE_DATA.
	 *
	 * For now we assume that the key in the dictionary is the same as the key
	 * in the DB to which we're writing. 
	 *
	 * A MDSNameValuePair array may be specified to facilitate conversion of 
	 * values which appears in the dictionary as strings but which are stored 
	 * in the DB as integers.
	 *
	 * We're also assuming that all DB keys are of format 
	 * CSSM_DB_ATTRIBUTE_NAME_AS_STRING.
	 */
	bool lookupToDbAttr(
		const char *key,
		CSSM_DB_ATTRIBUTE_DATA &attr,
		CSSM_DB_ATTRIBUTE_FORMAT attrFormat,
		const MDSNameValuePair *nameValues = NULL);

	/*
	 * Given a RelationInfo and an array of CSSM_DB_ATTRIBUTE_DATAs, fill in 
	 * the CSSM_DB_ATTRIBUTE_DATA array with as many fields as we can find in 
	 * the dictionary. All fields are treated as optional. 
	 */
	void lookupAttributes(
		const RelationInfo 			*relInfo,
		CSSM_DB_ATTRIBUTE_DATA_PTR	outAttrs,		// filled in on return
		uint32						&numAttrs);		// RETURNED
		
		CFDictionaryRef		dict() 		{ return mDict; }
		const char			*urlPath() 	{ return mUrlPath; }
		const char			*fileDesc() { return mFileDesc; }

	/*
	 * Lookup with file-based indirection. Allows multiple mdsinfo file to share 
	 * commmon info from a separate plist file.
	 */
	const CFPropertyListRef lookupWithIndirect(
		const char *key,
		CFBundleRef bundle,
		CFTypeID	desiredType,
		bool		&fetchedFromDisk);	// true --> caller must CFRelease the returned
										//     value
										// false -> it's part of this dictionary
	
	void setDefaults(const MDS_InstallDefaults *defaults);
	
private:
	CFDictionaryRef		mDict;
	bool				mWeOwnDict;
	char				*mUrlPath;
	char				*mFileDesc;
	CFStringRef			mSubdir;
	
	// default GUID/SSID to apply to all records as needed
	const MDS_InstallDefaults *mDefaults;
};

} // end namespace Security

#endif /* _MDS_DICTIONARY_H_ */
