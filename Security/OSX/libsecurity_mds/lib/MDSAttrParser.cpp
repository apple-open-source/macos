/*
 * Copyright (c) 2000-2001,2011-2014 Apple Inc. All Rights Reserved.
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
   File:      MDSAttrParser.cpp

   Contains:  Classes to parse XML plists and fill in MDS DBs with the
              attributes found there.  

   Copyright (c) 2001,2011-2014 Apple Inc. All Rights Reserved.
*/

#include "MDSAttrParser.h"
#include "MDSAttrUtils.h"
#include "MDSDictionary.h"
#include <security_utilities/logging.h>
#include <Security/mds_schema.h>

namespace Security
{

MDSAttrParser::MDSAttrParser(
	const char *bundlePath,
	MDSSession &dl,
	CSSM_DB_HANDLE objectHand,
	CSSM_DB_HANDLE cdsaDirHand) :
		mBundle(NULL),
		mPath(NULL),
		mDl(dl),
		mObjectHand(objectHand),
		mCdsaDirHand(cdsaDirHand),
		mGuid(NULL),
		mDefaults(NULL)
{
	/* Only task here is to cook up a CFBundle for the specified path */
	size_t pathLen = strlen(bundlePath);
	CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL,
		(unsigned char *)bundlePath,
		pathLen,
		false);
	if(url == NULL) {
		Syslog::alert("CFURLCreateFromFileSystemRepresentation(%s) failure", mPath);
		CssmError::throwMe(CSSMERR_DL_INVALID_DB_NAME);
	}
	
	/* FIXME - this leaks 28 bytes each time thru, even though we CFRelease the
	 * mBundle in out destructor. I think this is a CF leak. */
	mBundle = CFBundleCreate(NULL, url);
	CFRelease(url);
	if(mBundle == NULL) {
		Syslog::alert("CFBundleCreate(%s) failure", mPath);
		CssmError::throwMe(CSSMERR_DL_INVALID_DB_NAME);
	}
	mPath = new char[pathLen + 1];
	strcpy(mPath, bundlePath);
}

MDSAttrParser::~MDSAttrParser()
{
	CF_RELEASE(mBundle);
	delete [] mPath;
	delete [] mGuid;
}

/*********************
 Main public function.

Parsing bundle {
	get all *.mdsinfo files;
	for each mdsinfo {
		get contents of that file as dictionary;
		switch (ModuleType) {
		case CSSM:
			parse this mdsinfo --> MDS_OBJECT_RECORDTYPE, MDS_CDSADIR_CSSM_RECORDTYPE;
			break;
		case Plugin:
			parse this info --> MDS_OBJECT_RECORDTYPE, MDS_CDSADIR_COMMON_RECORDTYPE;
		case PluginInfo:
			recordType = lookup("MdsRecordType");
			dispatch to recordtype-specific parsing;
		}
	}
}
************/ 

void MDSAttrParser::parseAttrs(CFStringRef subdir)
{
	/* get all *.mdsinfo files */
	CFArrayRef bundleInfoFiles = CFBundleCopyResourceURLsOfType(mBundle,
		CFSTR(MDS_INFO_TYPE),
		subdir);
	if(bundleInfoFiles == NULL) {
		Syslog::alert("MDSAttrParser: no mdsattr files for %s", mPath);
		return;
	}
	assert(CFGetTypeID(bundleInfoFiles) == CFArrayGetTypeID());
	
	/* process each .mdsinfo file */
	CFIndex numFiles = CFArrayGetCount(bundleInfoFiles);
	for(CFIndex i=0; i<numFiles; i++) {
		/* get filename as CFURL */
		CFURLRef infoUrl = NULL;
		
		infoUrl = reinterpret_cast<CFURLRef>(
			CFArrayGetValueAtIndex(bundleInfoFiles, i));
		if(infoUrl == NULL) {
			MPDebug("MDSAttrParser: CFBundleCopyResourceURLsOfType screwup 1");
			continue;
		}
		if(CFGetTypeID(infoUrl) != CFURLGetTypeID()) {
			MPDebug("MDSAttrParser: CFBundleCopyResourceURLsOfType screwup 2");
			continue;
		}
		
		// @@@  Workaround for 4234967: skip any filename beginning with "._"
		CFStringRef lastComponent = CFURLCopyLastPathComponent(infoUrl);
		if (lastComponent) {
			CFStringRef resFilePfx = CFSTR("._");
			// setting the search length and location like this permits, 
			// e.g., ".foo.mdsinfo" to be valid
			CFIndex resFilePfxLen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(resFilePfx), kCFStringEncodingUTF8);
			CFRange range = CFRangeMake(0, resFilePfxLen);
			Boolean skip = CFStringFindWithOptions(lastComponent, 
												   resFilePfx, 
												   range,
												   0/*options*/,
												   NULL/*returned substr*/);
			CFRelease(lastComponent);
			if (skip == true) {
				Syslog::warning("MDSAttrParser: ignoring resource file");
				continue;
			}
		}
		
		parseFile(infoUrl, subdir);
	} /* for each mdsinfo */
	CF_RELEASE(bundleInfoFiles);
}

void MDSAttrParser::parseFile(CFURLRef infoUrl, CFStringRef subdir)
{
	CFStringRef infoType = NULL;
	
	/* Get contents of mdsinfo file as dictionary */
	MDSDictionary mdsDict(infoUrl, subdir, mPath);
	/* Make sure we set all possible MDS values before checking for GUID */
	mdsDict.setDefaults(mDefaults);
	if (mGuid == NULL) {
		CFStringRef guid = (CFStringRef)mdsDict.lookup("ModuleID", true, CFStringGetTypeID());
		if (guid) {
			CFIndex copylen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(guid), kCFStringEncodingUTF8) + 1/*nul terminator*/;
			mGuid = new char[copylen];
			if (false == CFStringGetCString(guid, mGuid, copylen, kCFStringEncodingUTF8)) {
				logFileError("Error copying GUID", infoUrl, NULL, NULL);
				delete [] mGuid;
				mGuid = NULL;
				CssmError::throwMe(CSSM_ERRCODE_MDS_ERROR);
			}
		}
		else {
			logFileError("No GUID associated with plugin?", infoUrl, NULL, NULL);
			CssmError::throwMe(CSSM_ERRCODE_MDS_ERROR);
		}
	}
	
	MPDebug("Parsing mdsinfo file %s", mdsDict.fileDesc());
	
	/* Determine what kind of info file this is and dispatch accordingly */
	infoType = (CFStringRef)mdsDict.lookup(CFSTR(MDS_INFO_FILE_TYPE),
		true, CFStringGetTypeID());
	if(infoType == NULL) {
		logFileError("Malformed MDS Info file", infoUrl, NULL, NULL);
		CssmError::throwMe(CSSM_ERRCODE_MDS_ERROR);
	}
	
	/* be robust here, errors in these low-level routines do not affect
	 * the rest of our task */
	try {
		if(CFStringCompare(infoType, CFSTR(MDS_INFO_FILE_TYPE_CSSM), 0) 
				== kCFCompareEqualTo) {
			parseCssmInfo(&mdsDict);
		}
		else if(CFStringCompare(infoType, CFSTR(MDS_INFO_FILE_TYPE_PLUGIN), 0) 
				== kCFCompareEqualTo) {
			parsePluginCommon(&mdsDict);
		}
		else if(CFStringCompare(infoType, CFSTR(MDS_INFO_FILE_TYPE_RECORD), 0) 
				== kCFCompareEqualTo) {
			parsePluginSpecific(&mdsDict);
		}
		else {
			logFileError("Malformed MDS Info file", infoUrl, NULL, NULL);
		}
	}
	catch(...) {
	
	}
}

void MDSAttrParser::logFileError(
	const char *op,
	CFURLRef fileUrl,
	CFStringRef errStr,		// optional if you have it
	SInt32 *errNo)			// optional if you have it
{
	CFStringRef urlStr = CFURLGetString(fileUrl);
	const char *cUrlStr = CFStringGetCStringPtr(urlStr, kCFStringEncodingUTF8);
	char* stringBuffer = NULL;
    
    if (cUrlStr == NULL)
    {
        CFIndex maxLen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(urlStr), kCFStringEncodingUTF8) + 1;
        stringBuffer = (char*) malloc(maxLen);
        CFStringGetCString(urlStr, stringBuffer, maxLen, kCFStringEncodingUTF8);
        cUrlStr = stringBuffer;
    }
    
	if(errStr) {
        const char *cerrStr = CFStringGetCStringPtr(errStr, kCFStringEncodingUTF8);
        char* sbuf2 = NULL;
        
        if (cerrStr == NULL)
        {
            CFIndex maxLen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(errStr), kCFStringEncodingUTF8) + 1;
            sbuf2 = (char*) malloc(maxLen);
            CFStringGetCString(urlStr, sbuf2, maxLen, kCFStringEncodingUTF8);
            cUrlStr = sbuf2;
        }
    
		Syslog::alert("MDS: %s: bundle %s url %s: error %s",
			op, mPath, cUrlStr, cerrStr);
        
        if (sbuf2 != NULL)
        {
            free(sbuf2);
        }
	}
	else {
		Syslog::alert("MDS: %s: bundle %s url %s: error %d",
			op, mPath, cUrlStr, (int)(errNo ? *errNo : 0));
	}
    
    if (stringBuffer != NULL)
    {
        free(stringBuffer);
    }
}
	 
/*
 * Parse a CSSM info file.
 */	
void MDSAttrParser::parseCssmInfo(
	MDSDictionary *mdsDict)
{
	/* first get object info */
	parseObjectRecord(mdsDict);
	
	/* now CSSM relation */
	const RelationInfo *relationInfo = 
		MDSRecordTypeToRelation(MDS_CDSADIR_CSSM_RECORDTYPE);
	assert(relationInfo != NULL);
	parseMdsRecord(mdsDict, relationInfo, mCdsaDirHand);
}
	
/*
 * Parse a PluginCommon file.
 */
void MDSAttrParser::parsePluginCommon(
	MDSDictionary *mdsDict)
{
	
	/* first get object info */
	parseObjectRecord(mdsDict);
	
	/* now common relation */
	const RelationInfo *relationInfo = 
		MDSRecordTypeToRelation(MDS_CDSADIR_COMMON_RECORDTYPE);
	assert(relationInfo != NULL);
	parseMdsRecord(mdsDict, relationInfo, mCdsaDirHand);
}

/*
 * Parse a Plugin Specific file.
 */
void MDSAttrParser::parsePluginSpecific(
	MDSDictionary *mdsDict)
{
	/* determine record type from the file itself */
	CFStringRef recordTypeStr = 
		(CFStringRef)mdsDict->lookup(MDS_INFO_FILE_RECORD_TYPE,
			true, CFStringGetTypeID());
	if(recordTypeStr == NULL) {
		MPDebug("%s: no %s record found\n", mdsDict->fileDesc(),
			MDS_INFO_FILE_RECORD_TYPE);
		return;
	}

	/* convert to a known schema */
	const char *recordTypeCStr = MDSCFStringToCString(recordTypeStr);
	const RelationInfo *relationInfo = MDSRecordTypeNameToRelation(recordTypeCStr);
	if(relationInfo == NULL) {
		Syslog::alert("MDS file %s has unsupported record type %s", 
			mdsDict->fileDesc(), recordTypeCStr);
		MPDebug("MDS file %s has unsupported record type %s", 
			mdsDict->fileDesc(), recordTypeCStr);
		delete [] recordTypeCStr;
		return;
	}
	MPDebug("Parsing MDS file %s, recordType %s", mdsDict->fileDesc(), recordTypeCStr);
	delete [] recordTypeCStr;
	
	/* handle special cases here */
	switch(relationInfo->DataRecordType) {
		case MDS_CDSADIR_CSP_CAPABILITY_RECORDTYPE:
			parseCspCapabilitiesRecord(mdsDict);
			break;
		case MDS_CDSADIR_TP_OIDS_RECORDTYPE:
			parseTpPolicyOidsRecord(mdsDict);
			break;
		default:
			/* all (normal) linear schema */
			parseMdsRecord(mdsDict, relationInfo, mCdsaDirHand);
	}
}


/*
 * Given an open MDSDictionary, create an MDS_OBJECT_RECORDTYPE record and 
 * add it to mObjectHand. Used when parsing both CSSM records and MOduleCommon
 * records. 
 */
void MDSAttrParser::parseObjectRecord(
	MDSDictionary *mdsDict)
{
	assert(mdsDict != NULL);
	assert(mObjectHand != 0);
	parseMdsRecord(mdsDict, &kObjectRelation, mObjectHand);
	
}

/*
 * Given an open dictionary and a RelationInfo defining a schema, fetch all
 * attributes associated with the specified schema from the dictionary
 * and write them to specified DB.
 */
void MDSAttrParser::parseMdsRecord(
	MDSDictionary 				*mdsDict,
	const RelationInfo 			*relInfo,
	CSSM_DB_HANDLE				dbHand)
{
	assert(mdsDict != NULL);
	assert(relInfo != NULL);
	assert(dbHand != 0);
	
	/* 
	 * malloc an CSSM_DB_ATTRIBUTE_DATA array associated with specified schema.
	 */
	unsigned numSchemaAttrs = relInfo->NumberOfAttributes;
	CSSM_DB_ATTRIBUTE_DATA *dbAttrs = new CSSM_DB_ATTRIBUTE_DATA[numSchemaAttrs];
	
	/* 
	 * Grind thru the attributes in the specified schema. Do not assume the presence
	 * of any given attribute.
	 */
	uint32 foundAttrs = 0;	
	mdsDict->lookupAttributes(relInfo, dbAttrs, foundAttrs);
	
	/* write to the DB */
	MDSInsertRecord(dbAttrs, foundAttrs, relInfo->DataRecordType, mDl, dbHand);

	MDSFreeDbRecordAttrs(dbAttrs, foundAttrs);
	delete [] dbAttrs;
}

/*
 * Parse CSP capabilities. This is much more complicated than most records. 
 * The propertly list (*.mdsinfo) is set up like this:
 *
 * root(Dictionary) {
 *    ModuleID(String)
 *    SSID(Number)
 *    Capabilities(Array) {
 *       index 0(Dictionary) {
 *           AlgType(String)					-- CSSM_ALGID_SHA1
 *           ContextType(String)				-- CSSM_ALGCLASS_DIGEST
 *           UseeTag(String)					-- CSSM_USEE_NONE
 *           Description(String)				-- "SHA1 Digest"
 *           Attributes(Array)
 *              index 0(Dictionary)
 *                 AttributeType(String) 		-- CSSM_ATTRIBUTE_OUTPUT_SIZE
 *                 AttributeValue(Array) {
 *                    index 0(Number)			-- 20
 *                    ...
 *                 }
 *              index n ...
 *           }
 *       index n...
 *    }
 * }      
 *
 * The plist can specify multiple Capabilities, multiple Attributes for each
 * Capability, and multiple values for each Attribute. (Note that MULTI_UINT32
 * in the DB is represented in the plist as an Array of Numbers.) Each element 
 * of each Attributes array maps to one record in the DB. The GroupID attribute 
 * of a record is the index into the plist's Capabilities array. 
 */
void MDSAttrParser::parseCspCapabilitiesRecord(
	MDSDictionary *mdsDict)
{
	/* 
	 * Malloc an attribute array big enough for the whole schema. We're going 
	 * to re-use this array every time we write a new record. Portions of 
	 * the array are invariant for some inner loops.
	 */ 
	const RelationInfo *topRelInfo = 
		MDSRecordTypeToRelation(MDS_CDSADIR_CSP_CAPABILITY_RECORDTYPE);
	assert(topRelInfo != NULL);
	uint32 numInAttrs = topRelInfo->NumberOfAttributes;
	CSSM_DB_ATTRIBUTE_DATA_PTR outAttrs = new CSSM_DB_ATTRIBUTE_DATA[numInAttrs];
	
	/* these attrs are only set once, then they remain invariant */
	uint32 numTopLevelAttrs;
	mdsDict->lookupAttributes(&CSPCapabilitiesDict1RelInfo, outAttrs, 
		numTopLevelAttrs);
		
	bool fetchedFromDisk = false;
	
	/* obtain Capabilities array */
	CFArrayRef capArray = (CFArrayRef)mdsDict->lookupWithIndirect("Capabilities",
		mBundle,
		CFArrayGetTypeID(),
		fetchedFromDisk);
	if(capArray == NULL) {
		/* well we did not get very far.... */
		MPDebug("parseCspCapabilitiesRecord: no (or bad) Capabilities");
		delete [] outAttrs;
		return;
	}
	
	/*
	 * Descend into Capabilities array. Each element is a dictionary defined 
	 * by CSPCapabilitiesDict2RelInfo.
	 */
	CFIndex capArraySize = CFArrayGetCount(capArray);
	CFIndex capDex;
	for(capDex=0; capDex<capArraySize; capDex++) {
		MPDebug("...parsing Capability %d", (int)capDex);
		CFDictionaryRef capDict = 
			(CFDictionaryRef)CFArrayGetValueAtIndex(capArray, capDex);
		if((capDict == NULL) || 
		   (CFGetTypeID(capDict) != CFDictionaryGetTypeID())) {
			MPDebug("parseCspCapabilitiesRecord: bad Capabilities element");
			break;
		}
		MDSDictionary capDictMds(capDict);
		
		/* 
		 * Append this dictionary's attributes to outAttrs, after the fixed
		 * attributes from CSPCapabilitiesDict1RelInfo.
		 */
		uint32 numCapDictAttrs;
		capDictMds.lookupAttributes(&CSPCapabilitiesDict2RelInfo,
			&outAttrs[numTopLevelAttrs],
			numCapDictAttrs);
		
		/*
		 * Append the GroupId attribute, which we infer from the current index 
		 * into Capabilitites.  However, thou shalt not use > 4-byte values 
         * for MDS, so convert from CFIndex first.  
		 */
        if (capDex > uint32(~0)) {
            MPDebug("parseCspCapabilitiesRecord: too large an index for MDS");
            break;
        }
        uint32 index32 = uint32(capDex);
		MDSRawValueToDbAttr(&index32, sizeof(index32), CSSM_DB_ATTRIBUTE_FORMAT_UINT32, 
			"GroupId", outAttrs[numTopLevelAttrs + numCapDictAttrs]);
		numCapDictAttrs++;	
		
		/* 
		 * Now descend into the array of this capability's attributes.
		 * Each element is a dictionary defined by
		 * by CSPCapabilitiesDict3RelInfo.
		 */
		CFArrayRef attrArray = (CFArrayRef)capDictMds.lookup("Attributes",
			true, CFArrayGetTypeID());
		if(attrArray == NULL) {
			MPDebug("parseCspCapabilitiesRecord: no (or bad) Attributes");
			break;
		}
		CFIndex attrArraySize = CFArrayGetCount(attrArray);
		CFIndex attrDex;
		for(attrDex=0; attrDex<attrArraySize; attrDex++) {
			MPDebug("   ...parsing Attribute %d", (int)attrDex);
			CFDictionaryRef attrDict = 
				(CFDictionaryRef)CFArrayGetValueAtIndex(attrArray, attrDex);
			if((attrDict == NULL) || 
			   (CFGetTypeID(attrDict) != CFDictionaryGetTypeID())) {
				MPDebug("parseCspCapabilitiesRecord: bad Attributes element");
				break;
			}
			MDSDictionary attrDictMds(attrDict);
			
			/* 
			 * Append this dictionary's attributes to outAttrs, after the fixed
			 * attributes from CSPCapabilitiesDict1RelInfo and this capability's
			 * CSPCapabilitiesDict2RelInfo.
			 */
			uint32 numAttrDictAttrs;
			attrDictMds.lookupAttributes(&CSPCapabilitiesDict3RelInfo,
				&outAttrs[numTopLevelAttrs + numCapDictAttrs],
				numAttrDictAttrs);
			
			/* write to DB */
			MDSInsertRecord(outAttrs,
				numTopLevelAttrs + numCapDictAttrs + numAttrDictAttrs,
				MDS_CDSADIR_CSP_CAPABILITY_RECORDTYPE, 
				mDl, 
				mCdsaDirHand);
				
			/* just free the attrs we allocated in this loop */
			MDSFreeDbRecordAttrs(&outAttrs[numTopLevelAttrs + numCapDictAttrs],
				numAttrDictAttrs);
		} 	/* for each attribute */
		/* just free the attrs we allocated in this loop */
		MDSFreeDbRecordAttrs(&outAttrs[numTopLevelAttrs], numCapDictAttrs);
	}		/* for each capability */
	
	MDSFreeDbRecordAttrs(outAttrs, numTopLevelAttrs);
	delete [] outAttrs;
	if(fetchedFromDisk) {
		CF_RELEASE(capArray);
	}
}

/*
 * Parse TP Policy OIDs. 
 * The propertly list (*.mdsinfo) is set up like this:
 *
 * root(Dictionary) {
 *    ModuleID(String)
 *    SSID(Number)
 *    Policies(Array) {
 *       index 0(Dictionary) {
 *           OID(Data)							-- <092a8648 86f76364 0102>
 *           Value(Data)						-- optional, OID-specific 
 *       index n...
 *    }
 * }      
 *
 * The plist can specify multiple Policies. Each element of the Policies 
 * array maps to one record in the DB.  
 */
void MDSAttrParser::parseTpPolicyOidsRecord(
	MDSDictionary *mdsDict)
{
	/* 
	 * Malloc an attribute array big enough for the whole schema. We're going 
	 * to re-use this array every time we write a new record. Portions of 
	 * the array are invariant for some inner loops.
	 */ 
	const RelationInfo *topRelInfo = 
		MDSRecordTypeToRelation(MDS_CDSADIR_TP_OIDS_RECORDTYPE);
	assert(topRelInfo != NULL);
	uint32 numInAttrs = topRelInfo->NumberOfAttributes;
	CSSM_DB_ATTRIBUTE_DATA_PTR outAttrs = new CSSM_DB_ATTRIBUTE_DATA[numInAttrs];
	
	/* these attrs are only set once, then they remain invariant */
	uint32 numTopLevelAttrs;
	mdsDict->lookupAttributes(&TpPolicyOidsDict1RelInfo, outAttrs, 
		numTopLevelAttrs);
		
	/* obtain Policies array */
	CFArrayRef policyArray = (CFArrayRef)mdsDict->lookup("Policies",
		true, CFArrayGetTypeID());
	if(policyArray == NULL) {
		/* well we did not get very far.... */
		MPDebug("parseTpPolicyOidsRecord: no (or bad) Policies");
		delete [] outAttrs;
		return;
	}
	
	/*
	 * Descend into Policies array. Each element is a dictionary defined 
	 * by TpPolicyOidsDict2RelInfo.
	 */
	CFIndex policyArraySize = CFArrayGetCount(policyArray);
	CFIndex policyDex;
	for(policyDex=0; policyDex<policyArraySize; policyDex++) {
		MPDebug("...parsing Policy %d", (int)policyDex);
		CFDictionaryRef policyDict = 
			(CFDictionaryRef)CFArrayGetValueAtIndex(policyArray, policyDex);
		if((policyDict == NULL) || 
		   (CFGetTypeID(policyDict) != CFDictionaryGetTypeID())) {
			MPDebug("parseTpPolicyOidsRecord: bad Policies element");
			break;
		}
		MDSDictionary policyDictMds(policyDict);
		
		/* 
		 * Append this dictionary's attributes to outAttrs, after the fixed
		 * attributes from TpPolicyOidsDict1RelInfo.
		 */
		uint32 numPolicyDictAttrs;
		policyDictMds.lookupAttributes(&TpPolicyOidsDict2RelInfo,
			&outAttrs[numTopLevelAttrs],
			numPolicyDictAttrs);
		
			
		/* write to DB */
		MDSInsertRecord(outAttrs,
			numTopLevelAttrs + numPolicyDictAttrs,
			MDS_CDSADIR_TP_OIDS_RECORDTYPE, 
			mDl, 
			mCdsaDirHand);
			
		/* free the attrs allocated in this loop */
		MDSFreeDbRecordAttrs(outAttrs + numTopLevelAttrs, numPolicyDictAttrs);
	}		/* for each policy */
	MDSFreeDbRecordAttrs(outAttrs, numTopLevelAttrs);
	delete [] outAttrs;
}


} // end namespace Security
