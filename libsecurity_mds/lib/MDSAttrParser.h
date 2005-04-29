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
   File:      MDSAttrParser.h

   Contains:  Classes to parse XML plists and fill in MDS DBs with the
              attributes found there.  

   Copyright: (c) 2001 Apple Computer, Inc., all rights reserved.
*/

#ifndef _MDS_ATTR_PARSER_H_
#define _MDS_ATTR_PARSER_H_  1

#include <Security/cssmtype.h>
#include "MDSSession.h"
#include "MDSDictionary.h"
#include "MDSAttrStrings.h"
#include <CoreFoundation/CoreFoundation.h>

/*
 * Hard-coded strings, which we attempt to keep to a minimum
 */
 
/* extension of a bundle's MDS files */
#define MDS_INFO_TYPE				"mdsinfo"

/* key in an MDS info file determining whether it's for CSSM, plugin, or
 * Plugin-specific MDS record type */
#define MDS_INFO_FILE_TYPE			"MdsFileType"

/* Values for MDS_INFO_FILE_TYPE */
#define MDS_INFO_FILE_TYPE_CSSM		"CSSM"
#define MDS_INFO_FILE_TYPE_PLUGIN	"PluginCommon"
#define MDS_INFO_FILE_TYPE_RECORD	"PluginSpecific"
 
/* For MDS_INFO_FILE_TYPE_RECORD files, this key is used to find the 
 * CSSM_DB_RECORDTYPE associated with the file's info. */
#define MDS_INFO_FILE_RECORD_TYPE	"MdsRecordType"

/* key for file description string, for debugging and documentation (since 
 * PropertyListEditor does not support comments) */
#define MDS_INFO_FILE_DESC			"MdsFileDescription"


namespace Security
{

/*
 * The purpose of the MDSAttrParser class is to process a set of plist files
 * in a specified bundle or framework, parsing them to create data which 
 * is written to a pair of open DBs. Each plist file represents the bundle's
 * entries for one or more MDS relations. Typically a bundle will have 
 * multiple plist files. 
 */

/* base class for all parsers */
class MDSAttrParser
{
public:
	MDSAttrParser(
		const char *bundlePath,
		MDSSession &dl,
		CSSM_DB_HANDLE objectHand,
		CSSM_DB_HANDLE cdsaDirHand);
	virtual ~MDSAttrParser();
	
	/* the bulk of the work */
	void parseAttrs(CFStringRef subdir = NULL);
	
	/* parse a single file, by path URL */
	void parseFile(CFURLRef theFileUrl, CFStringRef subdir = NULL);
	
	void setDefaults(const MDS_InstallDefaults *defaults) { mDefaults = defaults; }
	
private:
	void logFileError(
		const char *op,
		CFURLRef file,	
		CFStringRef errStr,		// optional if you have it
		SInt32 *errNo);			// optional if you have it
		
	/*
	 * Parse a CSSM info file.
	 */
	void parseCssmInfo(
		MDSDictionary *theDict);
		
	/*
	 * Parse a Plugin Common info file.
	 */
	void parsePluginCommon(
		MDSDictionary *theDict);
		
	/*
	 * Parse a Plugin-specific file.
	 */
	void parsePluginSpecific(
		MDSDictionary *theDict);
		
	/*
	 * Given an open dictionary (representing a parsed XML file), create
	 * an MDS_OBJECT_RECORDTYPE record and add it to mObjectHand. This is
	 * used by both parseCssmInfo and parsePluginCommon.
	 */
	void parseObjectRecord(
		MDSDictionary *dict);
		
	/*
	 * Given an open dictionary and a RelationInfo defining a schema, fetch all
	 * attributes associated with the specified schema from the dictionary
	 * and write them to specified DB.
	 */
	void parseMdsRecord(
		MDSDictionary	 			*mdsDict,
		const RelationInfo 			*relInfo,
		CSSM_DB_HANDLE				dbHand);

	/*
	 * Special case handlers for MDS_CDSADIR_CSP_CAPABILITY_RECORDTYPE and
	 * MDS_CDSADIR_TP_OIDS_RECORDTYPE.
	 */
	void parseCspCapabilitiesRecord(
		MDSDictionary 				*mdsDict);
	void parseTpPolicyOidsRecord(
		MDSDictionary 				*mdsDict);

private:
	/* could be Security.framework or a loadable bundle anywhere */
	CFBundleRef		mBundle;
	char			*mPath;
	
	/* a DL session and two open DBs - one for object directory, one for 
	 * CDSA directory */
	MDSSession		&mDl;
	CSSM_DB_HANDLE 	mObjectHand;
	CSSM_DB_HANDLE 	mCdsaDirHand;
	
	// Guid/SSID defaults
	const MDS_InstallDefaults *mDefaults;
};


} // end namespace Security

#endif /* _MDS_ATTR_PARSER_H_ */
