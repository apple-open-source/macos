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
   File:      MDSDictionary.cpp

   Contains:  Internal representation of one MDS info file in the form of 
              a CFDictionary. 

   Copyright: (c) 2001 Apple Computer, Inc., all rights reserved.
*/

#include "MDSDictionary.h"
#include "MDSAttrParser.h"
#include "MDSAttrUtils.h"
#include <security_utilities/logging.h>
#include <security_utilities/cfutilities.h>

namespace Security
{

/* heavyweight constructor from file */
MDSDictionary::MDSDictionary(
	CFURLRef fileUrl,
	CFStringRef subdir,
	const char *fullPath)		// could get from fileUrl, but very messy!
	: mDict(NULL),
	  mWeOwnDict(false),
	  mUrlPath(NULL),
	  mFileDesc(NULL),
	  mSubdir(subdir),
	  mDefaults(NULL)
{
	CFDataRef dictData = NULL;
	CFStringRef cfErr = NULL;
	
	assert(fileUrl != NULL);
	mUrlPath = MDSCopyCstring(fullPath);
	MPDebug("Creating MDSDictionary from %s", mUrlPath);
	
	/* Load data from URL */
	SInt32 uerr;
	Boolean brtn = CFURLCreateDataAndPropertiesFromResource(
		NULL,
		fileUrl,
		&dictData,
		NULL,		// properties
		NULL,		// desiredProperties
		&uerr);
	if(!brtn) {
		Syslog::alert("Error reading MDS file %s: %d", mUrlPath, uerr);
		CssmError::throwMe(CSSMERR_CSSM_MDS_ERROR);
	}
	
	/* if it's not a dictionary, we don't want it */
	mDict = reinterpret_cast<CFDictionaryRef>(
		CFPropertyListCreateFromXMLData(NULL,
			dictData,
			kCFPropertyListImmutable,
			&cfErr));
	CFRelease(dictData);
	if(mDict == NULL) {
		Syslog::alert("Malformed MDS file %s (1)", mUrlPath);
		CssmError::throwMe(CSSMERR_CSSM_MDS_ERROR);
	}
	
	/* henceforth we must release this dictionary */
	mWeOwnDict = true;
	if(CFGetTypeID(mDict) != CFDictionaryGetTypeID()) {
		Syslog::alert("Malformed MDS file %s (2)", mUrlPath);
		CssmError::throwMe(CSSMERR_CSSM_MDS_ERROR);
	}
	CF_RELEASE(cfErr);
	
	/* get file description for error logging and debugging */
	CFStringRef cfStr = (CFStringRef)lookup(CFSTR(MDS_INFO_FILE_DESC), 
		true, CFStringGetTypeID());
	if(cfStr) {
		CFIndex len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfStr), kCFStringEncodingUTF8) + 1/*nul terminator*/;
		mFileDesc = new char[len];
		if(mFileDesc) {
			CFStringGetCString(cfStr, mFileDesc, len, 
				kCFStringEncodingUTF8);
		}
	}
}

/* lightweight constructor from existing CFDictionary */
MDSDictionary::MDSDictionary(CFDictionaryRef theDict)
	: mDict(theDict),
	  mWeOwnDict(false),
	  mUrlPath(NULL),
	  mFileDesc(NULL),
	  mDefaults(NULL)
{
	/* note caller owns and releases the dictionary */ 
	if(mDict == NULL) {
		MPDebug("Malformed MDS file (3)");
		CssmError::throwMe(CSSMERR_CSSM_MDS_ERROR);
	}
	if(CFGetTypeID(mDict) != CFDictionaryGetTypeID()) {
		MPDebug("Malformed MDS file (4)");
		CssmError::throwMe(CSSMERR_CSSM_MDS_ERROR);
	}
}

MDSDictionary::~MDSDictionary()
{
	if(mWeOwnDict) {
		CF_RELEASE(mDict);
	}
	mDict = NULL;
	delete [] mUrlPath;
	delete [] mFileDesc;
}

/* lookup by either C string or CFStringRef - returns NULL on error */
const void *MDSDictionary::lookup(
	const char *key,
	bool checkType,
	CFTypeID type)
{
	CFStringRef cfKey = CFStringCreateWithCString(NULL,
		key,
		kCFStringEncodingUTF8);
	if(cfKey == NULL) {
		MPDebug("MDSDictionary::lookup: error creating CFString for key");
		return NULL;
	}
	const void *rtn = lookup(cfKey, checkType, type);
	CFRelease(cfKey);
	return rtn;

}

const void *MDSDictionary::lookup(
	CFStringRef key,
	bool checkType,
	CFTypeID type)
{
	assert(mDict != NULL);
	const void *rtn = CFDictionaryGetValue(mDict, key);
	if(rtn && checkType) {
		if(CFGetTypeID((CFTypeRef)rtn) != type) {
			return NULL;
		}
	}
	return rtn;
}

/*
 * Common means to perform a lookup in a dictionary given a C-string key and
 * placing the value - if present - in a CSSM_DB_ATTRIBUTE_DATA. Any errors
 * are only logged via MPDebug. Returns true if the value was found and 
 * successfully placed in supplied CSSM_DB_ATTRIBUTE_DATA.
 *
 * For now we assume that the key in the dictionary is the same as the key
 * in the DB to which we're writing. 
 *
 * We're also assuming that all DB keys are of format CSSM_DB_ATTRIBUTE_NAME_AS_STRING.
 */
bool MDSDictionary::lookupToDbAttr(
	const char *key,
	CSSM_DB_ATTRIBUTE_DATA &attr,
	CSSM_DB_ATTRIBUTE_FORMAT attrFormat,
	const MDSNameValuePair *nameValues)	// optional for converting strings to numbers
{
	assert(mDict != NULL);
	assert(&attr != NULL);
	
	CFTypeRef	value;				// polymorphic dictionary value
	bool		ourRtn = false;
	const void 	*srcPtr = NULL;		// polymorphic raw source bytes
	size_t		srcLen = 0;
	uint32 		ival = 0;
	uint32		*ivalArray = NULL;
	uint32		numValues = 1;		// the default for MDSRawValueToDbAttr
	string		stringVal;

	value = (CFTypeRef)lookup(key);
	if(value == NULL) {
		return false;
	}
	CFTypeID valueType = CFGetTypeID(value);
	
	/* 
	 * We have the value; could be any type. Handle it based on caller's 
	 * CSSM_DB_ATTRIBUTE_FORMAT.
	 */
	switch(attrFormat) {
		case CSSM_DB_ATTRIBUTE_FORMAT_STRING:
		{
			if(valueType != CFStringGetTypeID()) {
				MPDebug("lookupToDbAttr: string format mismatch");
				break;
			}
			stringVal = cfString((CFStringRef)value, false);
			srcPtr = stringVal.c_str();
			srcLen = stringVal.size();
			if(srcLen) {
				ourRtn = true;
			}
			break;
		}
		case CSSM_DB_ATTRIBUTE_FORMAT_UINT32:
		{
			bool brtn = MDSCfTypeToUInt32(value, nameValues, key, ival, srcLen);
			if(!brtn) {
				MPDebug("MDS lookupToDbAttr: Bad number conversion");
				return false;
			}
			srcPtr = &ival;
			ourRtn = true;
			break;
		}	 
		case CSSM_DB_ATTRIBUTE_FORMAT_MULTI_UINT32:	
		{
			/* 
			 * This is expressed in the dictionary as an array of numbers. 
			 * as in CSSM_DB_ATTRIBUTE_FORMAT_UINT32, each number can be
			 * expressed as either a string or a number.
			 */
			if(valueType != CFArrayGetTypeID()) {
				/*
				 * Let's be extremely slick and allow one number here, either 
				 * in string or number form....
				 */
				bool brtn = MDSCfTypeToUInt32(value, nameValues, key, ival, srcLen);
				if(!brtn) {
					MPDebug("MDS lookupToDbAttr: Bad array element");
					return false;
				}
				srcPtr = &ival;
				ourRtn = true;
				break;
			}
			CFArrayRef cfArray = (CFArrayRef)value;
			numValues = CFArrayGetCount(cfArray);
			if(numValues == 0) {
				/* degenerate case, legal - right? Can AppleDatabase do this? */
				srcPtr = NULL;
				srcLen = 0;
				ourRtn = true;
				break;
			}
			
			/* 
			 * malloc an array of uint32s
			 * convert each element in cfArray to a uint32
			 * store as CSSM_DB_ATTRIBUTE_FORMAT_MULTI_UINT32
			 *
			 * Note this does not have to be endian independent; the MDS DBs
			 * are not portable across machines let alone platforms. 
			 */
			ivalArray = new uint32[numValues];
			unsigned dex;
			bool brtn;
			for(dex=0; dex<numValues; dex++) {
				CFTypeRef elmt = (CFTypeRef)CFArrayGetValueAtIndex(cfArray, dex);
				if(elmt == NULL) {
					MPDebug("MDS lookupToDbAttr: key %s: Bad array element (1)", key);
					delete [] ivalArray;
					return false;
				}
                size_t itemLen = 0;
				brtn =  MDSCfTypeToUInt32(elmt, nameValues, key, ivalArray[dex], itemLen);
				if(!brtn) {
					MPDebug("MDS lookupToDbAttr: key %s Bad element at index %d",
						key, dex);
					delete [] ivalArray;
					return false;
				}
                srcLen += itemLen;
			}
			srcPtr = ivalArray;
			ourRtn = true;
			/*
			 * FIXME - numValues as used by MDSRawValueToDbAttr and placed in 
			 * CSSM_DB_ATTRIBUTE_DATA.NumberOfValues, appears to need to be
			 * one even for MULTI_UINT32 format; the number of ints in inferred
			 * from Value.Length....
			 */
			numValues = 1;
			break;
		}
		case CSSM_DB_ATTRIBUTE_FORMAT_BLOB:			// CFData
		{
			if(valueType != CFDataGetTypeID()) {
				MPDebug("lookupToDbAttr: blob/CFData format mismatch");
				break;
			}
			CFDataRef cfData = (CFDataRef)value;
			srcLen = CFDataGetLength(cfData);
			srcPtr = CFDataGetBytePtr(cfData);
			ourRtn = true;
			break;
		}
		case CSSM_DB_ATTRIBUTE_FORMAT_SINT32:		// I don't think we support this
		default:
			MPDebug("lookupToDbAttr: bad attrForm(%d)", (int)attrFormat);
			return false;
	}
	if(ourRtn) {
		MDSRawValueToDbAttr(srcPtr, srcLen, attrFormat, key, attr, numValues);
	}
	if(ivalArray) {
		delete [] ivalArray;
	}
	return ourRtn;
}

/*
 * Given a RelationInfo and an array of CSSM_DB_ATTRIBUTE_DATAs, fill in 
 * the CSSM_DB_ATTRIBUTE_DATA array with as many fields as we can find in 
 * the dictionary. All fields are treated as optional. 
 */
void MDSDictionary::lookupAttributes(
	const RelationInfo 			*relInfo,
	CSSM_DB_ATTRIBUTE_DATA_PTR	outAttrs,		// filled in on return
	uint32						&numAttrs)		// RETURNED
{
	unsigned 						dex;
	const CSSM_DB_ATTRIBUTE_INFO 	*inAttr = relInfo->AttributeInfo;
	const MDSNameValuePair 			**nameValues	= relInfo->nameValues;

	assert(relInfo != NULL);
	numAttrs = 0;
	for(dex=0; dex<relInfo->NumberOfAttributes; dex++) {
		bool brtn;
		const MDSNameValuePair *nvp;
		
		/* the array itself, or any element in it, can be NULL */
		if(nameValues != NULL) {
			nvp = nameValues[dex];
		}
		else {
			nvp = NULL;
		}
		brtn = lookupToDbAttr(inAttr->Label.AttributeName,
			*outAttrs, 
			inAttr->AttributeFormat,
			nvp);
		if(brtn) {
			/* successfully added to dbAttrs */
			outAttrs++;
			numAttrs++;
		}
		inAttr++;		// regardless
	}
}

/*
 * Lookup with file-based indirection. Allows multiple mdsinfo files to share commmon
 * info from a separate plist file.
 *
 * Do a lookup for specified key. If not found, return NULL. If found:
 * {
 *		if type of value matches desiredType {
 * 			return the value;
 *		}
 *		else if type of value is string {
 *			if string starts with "file:" {
 *				attempt to read property list with that filename relative to 
 *					specified bundle;
 *				if CFType of that propList matches desiredType {
 *					return newly read propList;
 *				}
 *			}
 *		}
 *		...else return error;
 */
const CFPropertyListRef MDSDictionary::lookupWithIndirect(
	const char *key,
	CFBundleRef bundle,
	CFTypeID	desiredType,
	bool		&fetchedFromDisk)	// true --> caller must CFRelease the returned
									//     value
									// false -> it's part of this dictionary
{
	CFPropertyListRef ourRtn = NULL;
	CFDataRef dictData = NULL;
	CFStringRef cfErr = NULL;
	SInt32 uerr;
	Boolean brtn;
	
	
	assert(key != NULL);
	assert(bundle != NULL);
	
	fetchedFromDisk = false;
	
	/* basic local lookup */
	CFStringRef cfKey = CFStringCreateWithCString(NULL,
		key,
		kCFStringEncodingUTF8);
	if(cfKey == NULL) {
		MPDebug("CFStringCreateWithCString error");
		return NULL;
	}
	const void *rtn = CFDictionaryGetValue(mDict, cfKey);
	CFRelease(cfKey);
	if(rtn == NULL) {
		return NULL;
	}
	CFTypeID foundType = CFGetTypeID((CFTypeRef)rtn);
	if(foundType == desiredType) {
		/* found what we're looking for; done */
		return (CFPropertyListRef)rtn;
	}
	
	/* is it a string which starts with "file:"? */
	if(foundType != CFStringGetTypeID()) {
		return NULL;
	}
	const char *cVal = MDSCFStringToCString((CFStringRef)rtn);
	if(cVal == NULL) {
		MPDebug("MDSCFStringToCString error in lookupWithIndirect");
		return NULL;
	}
	if(strstr(cVal, "file:") != cVal) {
		delete [] cVal;
		return NULL;
	}
	/* delete [] cval on return */
	
	/* OK, this specifies a resource file in the bundle. Fetch it. */
	CFURLRef fileUrl = NULL;
	CFStringRef cfFileName = CFStringCreateWithCString(NULL,
		cVal + 5,
		kCFStringEncodingUTF8);
	if(cfFileName == NULL) {
		MPDebug("lookupWithIndirect: bad file name spec");
		goto abort;
	}
	fileUrl = CFBundleCopyResourceURL(bundle, 
                cfFileName, 
                NULL, 
                mSubdir);
	if(fileUrl == NULL) {
		MPDebug("lookupWithIndirect: file %s not found", cVal);
		goto abort;
	}

	MPDebug("Fetching indirect resource %s", cVal);
	
	/* Load data from URL */
	brtn = CFURLCreateDataAndPropertiesFromResource(
		NULL,
		fileUrl,
		&dictData,
		NULL,		// properties
		NULL,		// desiredProperties
		&uerr);
	if(!brtn) {
		MPDebug("lookupWithIndirect: error %d reading %s", (int)uerr, cVal);
		goto abort;
	}
	
	/* if it's not a property list, we don't want it */
	ourRtn = CFPropertyListCreateFromXMLData(NULL,
			dictData,
			kCFPropertyListImmutable,
			&cfErr);
	if(ourRtn == NULL) {
		MPDebug("lookupWithIndirect: %s malformed (not a prop list)", cVal);
		goto abort;
	}
	
	/* if it doesn't match the caller's spec, we don't want it */
	if(CFGetTypeID(ourRtn) != desiredType) {
		MPDebug("lookupWithIndirect: %s malformed (mismatch)", cVal);
		CF_RELEASE(ourRtn);
		ourRtn = NULL;
		goto abort;
	}

	MPDebug("lookupWithIndirect: resource %s FOUND", cVal);
	fetchedFromDisk = true;
	
abort:
	delete [] cVal;
	CF_RELEASE(cfFileName);
	CF_RELEASE(fileUrl);
	CF_RELEASE(dictData);
	CF_RELEASE(cfErr);
	return ourRtn;
}

void MDSDictionary::setDefaults(const MDS_InstallDefaults *defaults) 
{ 
	mDefaults = defaults;

	/*
	 * Save the values into (a new) mDict.  
	 */
	assert(mDict != NULL);
	CFMutableDictionaryRef tmpDict = CFDictionaryCreateMutableCopy(NULL, 0, mDict);
	if(tmpDict == NULL) {
		MPDebug("setDefaults: error copying old dictionary");
		CssmError::throwMe(CSSMERR_CSSM_MDS_ERROR);
	}

	CFStringRef tmpStr = NULL;

	/*
	 * CFDictionaryAddValue() does nothing if the requested key is already 
	 * present.  If you need to call setDefaults() more than once, you'll 
	 * have to add the code to remove the old key/value pairs first.  
	 */
	if(defaults) {
		if(defaults->guid) {
			tmpStr = CFStringCreateWithCString(NULL, defaults->guid, kCFStringEncodingUTF8);
			if(tmpStr) {
				CFDictionaryAddValue(tmpDict, CFSTR("ModuleID"), tmpStr);
				CFRelease(tmpStr);
			}
			else {
				MPDebug("setDefaults: error creating CFString for GUID");
				CssmError::throwMe(CSSMERR_CSSM_MDS_ERROR);
			}
		}

		CFNumberRef tmpNum = CFNumberCreate(NULL, kCFNumberIntType, &defaults->ssid);
		if(tmpNum) {
			CFDictionaryAddValue(tmpDict, CFSTR("SSID"), tmpNum);
			CFRelease(tmpNum);
		}
		else {
			MPDebug("setDefaults: error creating CFString for SSID");
			CssmError::throwMe(CSSMERR_CSSM_MDS_ERROR);
		}

		if(defaults->serial) {
			tmpStr = CFStringCreateWithCString(NULL, defaults->serial, kCFStringEncodingUTF8);
			if(tmpStr) {
				CFDictionaryAddValue(tmpDict, CFSTR("ScSerialNumber"), tmpStr);
				CFRelease(tmpStr);
			}
			else {
				MPDebug("setDefaults: error creating CFString for serial number");
				CssmError::throwMe(CSSMERR_CSSM_MDS_ERROR);
			}
		}

		if(defaults->printName) {
			tmpStr = CFStringCreateWithCString(NULL, defaults->printName, kCFStringEncodingUTF8);
			if(tmpStr) {
				CFDictionaryAddValue(tmpDict, CFSTR("ScDesc"), tmpStr);
				CFRelease(tmpStr);
			}
			else {
				MPDebug("setDefaults: error creating CFString for description");
				CssmError::throwMe(CSSMERR_CSSM_MDS_ERROR);
			}
		}
	}

	if(mUrlPath) {
		tmpStr = CFStringCreateWithCString(NULL, mUrlPath, kCFStringEncodingUTF8);
		if(tmpStr) {
			CFDictionaryAddValue(tmpDict, CFSTR("Path"), tmpStr);
			CFRelease(tmpStr);
		}
		else {
			MPDebug("setDefaults: error creating CFString for path");
			CssmError::throwMe(CSSMERR_CSSM_MDS_ERROR);
		}
	}
	CFDictionaryRef oldDict = mDict;
	mDict = CFDictionaryCreateCopy(NULL, tmpDict);
	if(mDict == NULL) {
		mDict = oldDict;	// first do no harm
		CFRelease(tmpDict);
		MPDebug("setDefaults: error creating new dictionary");
		CssmError::throwMe(CSSMERR_CSSM_MDS_ERROR);
	}
	CFRelease(oldDict);
	CFRelease(tmpDict);
}


} // end namespace Security
