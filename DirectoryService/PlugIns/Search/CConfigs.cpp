/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

/*!
 * @header CConfigs
 * Code to parse a XML config file.
 */

#include "CConfigs.h"
#include "CLog.h"

#include <SystemConfiguration/SystemConfiguration.h>

#include <string.h>				//used for strcpy, etc.
#include <stdlib.h>				//used for malloc
#include <sys/stat.h>			//used for mkdir and stat

#include "PrivateTypes.h"
#include "DSUtils.h"

// --------------------------------------------------------------------------------
//	* CConfigs
// --------------------------------------------------------------------------------

CConfigs::CConfigs ( void )
{
	fSearchPolicy						= kAutomaticSearchPolicy;
    fSearchNodeListLength				= 0;
    pSearchNodeList						= nil;
	fDirRef								= 0;
	fConfigDict							= nil;
	fSearchNodeConfigFileName			= nil;
	fSearchNodeConfigBackupFileName		= nil;
	fSearchNodeConfigCorruptedFileName	= nil;
	fXMLSearchPathVersionKeyString  = CFStringCreateWithCString( NULL, kXMLSearchPathVersionKey, kCFStringEncodingUTF8 );
	fXMLSearchPolicyKeyString		= CFStringCreateWithCString( NULL, kXMLSearchPolicyKey, kCFStringEncodingUTF8 );
	fXMLSearchPathArrayKeyString	= CFStringCreateWithCString( NULL, kXMLSearchPathArrayKey, kCFStringEncodingUTF8 );
	fXMLSearchDHCPLDAPString		= CFStringCreateWithCString( NULL, kXMLSearchDHCPLDAP, kCFStringEncodingUTF8 );
#if AUGMENT_RECORDS
	fXMLAugmentSearchKeyString		= CFStringCreateWithCString( NULL, kXMLAugmentSearchKey, kCFStringEncodingUTF8 );
	fXMLAugmentDirNodeNameKeyString	= CFStringCreateWithCString( NULL, kXMLAugmentDirNodeNameKey, kCFStringEncodingUTF8 );
	fXMLToBeAugmentedDirNodeNameKeyString = CFStringCreateWithCString( NULL, kXMLToBeAugmentedDirNodeNameKey, kCFStringEncodingUTF8 );
	fXMLAugmentAttrListDictKeyString= CFStringCreateWithCString( NULL, kXMLAugmentAttrListDictKey, kCFStringEncodingUTF8 );
	bAugmentSearch = false;
	fAugmentDirNodeName = nil;
	fAugmentAttrListDict = nil;
#endif
} // CConfigs


// --------------------------------------------------------------------------------
//	* ~CConfigs ()
// --------------------------------------------------------------------------------

CConfigs::~CConfigs ( void )
{
	//KW might consider cleanup of the fDirRef here
	DSCFRelease(fConfigDict);
	DSFreeString(fSearchNodeConfigFileName);
	DSFreeString(fSearchNodeConfigBackupFileName);
	DSFreeString(fSearchNodeConfigCorruptedFileName);
	DSCFRelease(fXMLSearchPathVersionKeyString);
	DSCFRelease(fXMLSearchPolicyKeyString);
	DSCFRelease(fXMLSearchPathArrayKeyString);
	DSCFRelease(fXMLSearchDHCPLDAPString);
#if AUGMENT_RECORDS
	DSCFRelease(fXMLAugmentSearchKeyString);
	DSFreeString(fAugmentDirNodeName);
	DSCFRelease(fXMLAugmentDirNodeNameKeyString);
	DSCFRelease(fXMLToBeAugmentedDirNodeNameKeyString);
	DSCFRelease(fXMLAugmentAttrListDictKeyString);
#endif
} // ~CConfigs


// --------------------------------------------------------------------------------
//	* Init (UInt32)
// --------------------------------------------------------------------------------

SInt32 CConfigs::Init ( const char *inSearchNodeConfigFilePrefix, UInt32 &outSearchPolicy )
{
	SInt32				siResult		= eDSNullParameter;
	CFMutableArrayRef   cfArrayRef		= NULL;
	CFIndex				cfConfigCount   = 0;
	CFMutableStringRef	cfSearchNode	= NULL;
	CFRange				cfRangeVal;
	struct stat			statResult;
	bool				bUpdateConfig   = false;
	CFStringRef			cfStringRef		= NULL;

	try
	{	
		if (inSearchNodeConfigFilePrefix != nil)
		{
			fSearchNodeConfigFileName = (char *) ::calloc(::strlen(inSearchNodeConfigFilePrefix) + ::strlen(".plist") + 1 , sizeof(char) );
			::strcpy(fSearchNodeConfigFileName, inSearchNodeConfigFilePrefix);
			::strcat(fSearchNodeConfigFileName, ".plist");
		
			fSearchNodeConfigBackupFileName = (char *) ::calloc(::strlen(inSearchNodeConfigFilePrefix) + ::strlen("Backup.plist") + 1 , sizeof(char) );
			::strcpy(fSearchNodeConfigBackupFileName, inSearchNodeConfigFilePrefix);
			::strcat(fSearchNodeConfigBackupFileName, "Backup.plist");
		
			fSearchNodeConfigCorruptedFileName = (char *) ::calloc(::strlen(inSearchNodeConfigFilePrefix) + ::strlen("Corrupted.plist") + 1 , sizeof(char) );
			::strcpy(fSearchNodeConfigCorruptedFileName, inSearchNodeConfigFilePrefix);
			::strcat(fSearchNodeConfigCorruptedFileName, "Corrupted.plist");

			siResult = eDSNoErr;
			
			//get the search policy
			siResult = ConfigSearchPolicy();
			if (siResult == eDSNoErr) //which it should always be
			{
				outSearchPolicy = fSearchPolicy;
			}
			
			//let's see if we need to convert any deprecated node names
			//ie. LDAPv2 to LDAPv3
			//ie. BSD Configuration Files to BSD
			//only do this if the deprecated plugins are not present
			cfArrayRef = nil;
			if ( CFDictionaryContainsKey( fConfigDict, fXMLSearchPathArrayKeyString ) )
			{
				cfArrayRef = (CFMutableArrayRef)CFDictionaryGetValue( fConfigDict, fXMLSearchPathArrayKeyString );
			}
			if (cfArrayRef != nil)
			{
				CFStringRef cfLDAPv2Prefix = CFStringCreateWithCString( NULL, "/LDAPv2/", kCFStringEncodingUTF8 );
				CFStringRef cfLDAPv3Prefix = CFStringCreateWithCString( NULL, "/LDAPv3/", kCFStringEncodingUTF8 );
				CFStringRef cfBSDOldPrefix = CFStringCreateWithCString( NULL, "/BSD Configuration Files/Local", kCFStringEncodingUTF8 );
				CFStringRef cfBSDNewPrefix = CFStringCreateWithCString( NULL, "/BSD/local", kCFStringEncodingUTF8 );
				cfConfigCount = ::CFArrayGetCount( cfArrayRef );
				for (SInt32 iConfigIndex = 0; iConfigIndex < cfConfigCount; iConfigIndex++)
				{
					cfSearchNode = (CFMutableStringRef)::CFArrayGetValueAtIndex( cfArrayRef, iConfigIndex );
					if ( cfSearchNode != nil )
					{
						if ( CFGetTypeID( cfSearchNode ) == CFStringGetTypeID() )
						{
							cfRangeVal = CFStringFind(cfSearchNode, cfLDAPv2Prefix, 0);
							if (cfRangeVal.location == 0)
							{
								if (stat( "/System/Library/Frameworks/DirectoryService.framework/Resources/Plugins/LDAPv2.dsplug", &statResult ) != eDSNoErr)
								{
									cfSearchNode = CFStringCreateMutableCopy(NULL, 0, cfSearchNode);
									//replace LDAPv2 with LDAPv3
									CFStringReplace(cfSearchNode, cfRangeVal, cfLDAPv3Prefix);
									CFArraySetValueAtIndex( cfArrayRef, iConfigIndex, cfSearchNode );
									CFRelease(cfSearchNode);
									bUpdateConfig = true;
								}
								continue;
							}
							cfRangeVal = CFStringFind(cfSearchNode, cfBSDOldPrefix, 0);
							if (cfRangeVal.location == 0)
							{
								if (stat( "/System/Library/Frameworks/DirectoryService.framework/Resources/Plugins/BSD Configuration Files.dsplug", &statResult ) != eDSNoErr)
								{
									cfSearchNode = CFStringCreateMutableCopy(NULL, 0, cfSearchNode);
									//replace BSD Configuration Files with BSD
									CFStringReplace(cfSearchNode, cfRangeVal, cfBSDNewPrefix);
									CFArraySetValueAtIndex( cfArrayRef, iConfigIndex, cfSearchNode );
									CFRelease(cfSearchNode);
									bUpdateConfig = true;
								}
								continue;
							}
						}
					}
				}
				CFRelease(cfLDAPv2Prefix);
				cfLDAPv2Prefix = NULL;
				CFRelease(cfLDAPv3Prefix);
				cfLDAPv3Prefix = NULL;
				CFRelease(cfBSDOldPrefix);
				cfBSDOldPrefix = NULL;
				CFRelease(cfBSDNewPrefix);
				cfBSDNewPrefix = NULL;
			}
			if (bUpdateConfig)
			{
				cfStringRef = CFStringCreateWithCString( NULL, "Search Node PlugIn Version 1.7", kCFStringEncodingUTF8 );
				CFDictionarySetValue( fConfigDict, fXMLSearchPathVersionKeyString, cfStringRef );
				CFRelease(cfStringRef);
				WriteConfig();
			}
		}
			
	} // try
	catch( SInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // Init

// --------------------------------------------------------------------------------
//	* GetCustom (void)
// --------------------------------------------------------------------------------

sSearchList *CConfigs:: GetCustom ( void )
{
	sSearchList	   *outList		= nil;
	SInt32			siResult	= eDSNoErr;

	//build the list and get the search policy
	//each time we call this we create a new one and assume that the old is owned by the previous caller
	pSearchNodeList = nil;
	siResult = ConfigList();
	
	if (siResult == eDSNoErr)
	{
		outList = pSearchNodeList;
	}
	
	//no need to cleanup the struct list ie. the internals
	//since caller will handle it
	
	//nil is returned if there is a failure
		
	return( outList );

} // GetCustom

// ---------------------------------------------------------------------------
//	* ConfigSearchPolicy
// ---------------------------------------------------------------------------

SInt32 CConfigs:: ConfigSearchPolicy ( void )
{
    SInt32					siResult				= eDSNoErr;
    SInt32					result					= eDSNoErr;
    CFStringRef				errorString;
    CFURLRef				configFileURL			= nil;
    CFURLRef				configFileCorruptedURL	= nil;
    CFDataRef				xmlData					= nil;
    CFPropertyListRef		configPropertyList		= nil;
    CFMutableDictionaryRef	configDict				= nil;
	bool					bFileOpSuccess			= false;
	bool					bWroteFile				= false;
	bool					bCorruptedFile			= false;
    char					string[ PATH_MAX ];
    char				   *configVersion			= nil;
	CFNumberRef				aSearchPolicy;
	struct stat				statResult;
	CFStringRef				cfStringRef				= nil;
	SInt32					errorCode				= 0;
	int						defaultSearchPolicy		= 1;
	CFStringRef				sBase					= nil;
	CFStringRef				sPath					= nil;
	CFStringRef				sCorruptedPath			= nil;
	bool					bUseXMLData				= false;
	char				   *filenameString			= nil;

	//Config data is read from a XML file OR created as default
	//KW eventually use Version from XML file to check against the code here?
	//Steps in the process:
	//1- see if the file exists
	//2- if it exists then try to read it otherwise just use created xmldata
	//3- if existing file is corrupted then rename it and save it while creating a new default file
	//4- if file doesn't exist then create a new default file - make sure directories exist/if not create them
    //make sure file permissions are root only
	//keep on going with a fConfigDict regardless whether file access works

	sPath = ::CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "/Library/Preferences/DirectoryService/%s" ), fSearchNodeConfigFileName );

	::memset(string,0,PATH_MAX);
	::CFStringGetCString( sPath, string, sizeof( string ), kCFStringEncodingUTF8 );
	DbgLog( kLogPlugin, "Checking for Search Node XML config file:" );
	DbgLog( kLogPlugin, "%s", string );
	
	filenameString = strdup(string);

	// Convert it back into a CFURL.
	configFileURL = ::CFURLCreateWithFileSystemPath( kCFAllocatorDefault, sPath, kCFURLPOSIXPathStyle, false );
	CFRelease( sPath ); // build with Create so okay to dealloac here
	sPath = nil;

	//step 1- see if the file exists
	//if not then make sure the directories exist or create them
	//then create a new file if necessary
	result = ::stat( string, &statResult );
	//if file does not exist
	if (result != eDSNoErr)
	{
		//move down the path from the system defined local directory and check if it exists
		//if not create it
		sPath = ::CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@/%s" ), sBase, "/Preferences" );
		::memset(string,0,PATH_MAX);
		::CFStringGetCString( sPath, string, sizeof( string ), kCFStringEncodingUTF8 );
		result = ::stat( string, &statResult );
		//if first sub directory does not exist
		if (result != eDSNoErr)
		{
			::mkdir( string , 0775 );
			::chmod( string, 0775 ); //above 0775 doesn't seem to work - looks like umask modifies it
		}
		CFRelease( sPath ); // build with Create so okay to dealloac here
		sPath = nil;
		//next subdirectory
		sPath = ::CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@/%s" ), sBase, "/Preferences/DirectoryService" );
		::memset(string,0,PATH_MAX);
		::CFStringGetCString( sPath, string, sizeof( string ),  kCFStringEncodingUTF8 );
		result = ::stat( string, &statResult );
		//if second sub directory does not exist
		if (result != eDSNoErr)
		{
			::mkdir( string , 0775 );
			::chmod( string, 0775 ); //above 0775 doesn't seem to work - looks like umask modifies it
		}
		CFRelease( sPath ); // build with Create so okay to dealloac here
		sPath = nil;

		//create a new dictionary for the file
		configDict = CFDictionaryCreateMutable( kCFAllocatorDefault,
												0,
												&kCFTypeDictionaryKeyCallBacks,
												&kCFTypeDictionaryValueCallBacks );

		cfStringRef = CFSTR("Search Node PlugIn Version 1.7");
		CFDictionarySetValue( configDict, fXMLSearchPathVersionKeyString, cfStringRef );
		//CFRelease(cfStringRef);
		// we don't need to release CFSTR() created strings that we didn't retain
		aSearchPolicy = CFNumberCreate(NULL,kCFNumberIntType,&defaultSearchPolicy);
		CFDictionarySetValue( configDict, fXMLSearchPolicyKeyString, aSearchPolicy ); // default NetInfo search policy CFNumber
		if ( aSearchPolicy != nil )
		{
			CFRelease( aSearchPolicy );
			aSearchPolicy = nil;
		}

		//convert the dict into a XML blob
		xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, configDict);
		bUseXMLData = true;

		//write the XML to the config file
		bFileOpSuccess = CFURLWriteDataAndPropertiesToResource( configFileURL,
																xmlData,
																NULL,
																&errorCode);
		::chmod( filenameString, 0600 );
		CFRelease(configDict);
		configDict = nil;
		//CFRelease(xmlData); //keeping this data for use below
	} // file does not exist so creating one
	else //try to read the existing file
	{
		// Read the XML property list file
		bFileOpSuccess = CFURLCreateDataAndPropertiesFromResource(	kCFAllocatorDefault,
																configFileURL,
																&xmlData,          // place to put file data
																NULL,
																NULL,
																&errorCode);
		if (!bFileOpSuccess) //if fails ensure xmlData ptr is nil
		{
			xmlData = nil;
		}
		else
		{
			bUseXMLData = true;
		}
	}

	if (xmlData != nil)
	{
		// extract the config dictionary from the XML data.
		configPropertyList = CFPropertyListCreateFromXMLData(	kCFAllocatorDefault,
																xmlData,
																kCFPropertyListMutableContainers, //could also use kCFPropertyListImmutable, kCFPropertyListMutableContainers
																&errorString);

		if (configPropertyList != nil )
		{

			DbgLog( kLogPlugin, "Have read Search Node XML config file:" );
			DbgLog( kLogPlugin, "%s", string );

			//make the propertylist a dict
			if ( CFDictionaryGetTypeID() == CFGetTypeID( configPropertyList ) )
			{
				configDict = (CFMutableDictionaryRef) configPropertyList;
			}

			if (configDict != nil)
			{
				//config file version
				configVersion = GetVersion(configDict);
				if (configVersion == nil ) //release the dict and assume corrupted file
				{
					CFRelease(configDict);
					configDict			= nil;
					configPropertyList	= nil;
				}
				else
				{
					fSearchPolicy	= GetSearchPolicy(configDict);
#if AUGMENT_RECORDS
					bAugmentSearch	= GetAugmentSearch(configDict);
					if (bAugmentSearch)
					{
						fAugmentDirNodeName = GetAugmentDirNodeName(configDict);
						fToBeAugmentedDirNodeName = GetToBeAugmentedDirNodeName(configDict);
						//retrieve the dictionary of attribute lists per record type
						//ie. dictinoary of arrays where the array key is the record type
						fAugmentAttrListDict = GetAugmentAttrListDict(configDict);
					}
#endif
					//set the member dict variable
					fConfigDict		= configDict;

					delete [] configVersion;
					configVersion = nil;
				}//if (configVersion != nil)
				// don't release the configDict since it is the cast configPropertyList
			}//if (configDict != nil)
		}//if (configPropertyList != nil )
		if ( configPropertyList == nil) //we have a corrupted file
		{
			DbgLog( kLogPlugin, "Search Node XML config file is corrupted" );
			DbgLog( kLogPlugin, "Using default NetInfo Search Policy" );
			bCorruptedFile = true;
			//here we need to make a backup of the file - why? - because

			// Append the subpath.
			sCorruptedPath = ::CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@/%s/%s" ), sBase, "/Preferences/DirectoryService", fSearchNodeConfigCorruptedFileName );

			// Convert it into a CFURL.
			configFileCorruptedURL = ::CFURLCreateWithFileSystemPath( kCFAllocatorDefault, sCorruptedPath, kCFURLPOSIXPathStyle, false );
			
			::memset(string,0,PATH_MAX);
			::CFStringGetCString( sCorruptedPath, string, sizeof( string ),  kCFStringEncodingUTF8 );
			CFRelease( sCorruptedPath ); // build with Create so okay to dealloac here
			sCorruptedPath = nil;

			//write the XML to the corrupted copy of the config file
			bWroteFile = CFURLWriteDataAndPropertiesToResource(	configFileCorruptedURL,
																xmlData,
																NULL,
																&errorCode);
			::chmod( string, 0600 );

		} //couldn't extract the property list out of the file or the version didn't exist
		CFRelease(xmlData); // probably okay to dealloc since Create used and no longer needed
		xmlData = nil;
	}
	else
	{
		DbgLog( kLogPlugin, "Search Node XML config file is not readable" );
		DbgLog( kLogPlugin, "Using default NetInfo Search Policy" );
		bCorruptedFile = true;
		//here we make no backup since unable to read it at all
	}
	if (bCorruptedFile)
	{
		//here we create a whole new file
		//create a new dictionary for the file
		configDict = CFDictionaryCreateMutable(	kCFAllocatorDefault,
												0,
												&kCFTypeDictionaryKeyCallBacks,
												&kCFTypeDictionaryValueCallBacks );

		cfStringRef = CFSTR("Search Node PlugIn Version 1.7");
		CFDictionarySetValue( configDict, fXMLSearchPathVersionKeyString, cfStringRef );
		//CFRelease(cfStringRef);
		// don't release CFSTR() string if not retained
		aSearchPolicy = CFNumberCreate(NULL,kCFNumberIntType,&defaultSearchPolicy);
		CFDictionarySetValue( configDict, fXMLSearchPolicyKeyString, aSearchPolicy ); // default NetInfo search policy CFNumber
		if ( aSearchPolicy != nil )
		{
			CFRelease( aSearchPolicy );
			aSearchPolicy = nil;
		}

		//convert the dict into a XML blob
		xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, configDict);

		CFRelease(configDict);
		
		if (sPath != nil)
		{
			CFRelease(sPath);
		}
		sPath = ::CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "/Library/Preferences/DirectoryService/%s" ), fSearchNodeConfigFileName );

		if (configFileURL != nil)
		{
			CFRelease(configFileURL);
		}
		// Convert it back into a CFURL.
		configFileURL = ::CFURLCreateWithFileSystemPath( kCFAllocatorDefault, sPath, kCFURLPOSIXPathStyle, false );
		::memset(string,0,PATH_MAX);
		::CFStringGetCString( sPath, string, sizeof( string ),  kCFStringEncodingUTF8 );

		//write the newly created XML to the config file
		bWroteFile = CFURLWriteDataAndPropertiesToResource(	configFileURL,
															xmlData,
															NULL,
															&errorCode);
		::chmod( string, 0600 );
		if (xmlData != nil)
		{
			configPropertyList = CFPropertyListCreateFromXMLData(	kCFAllocatorDefault,
																	xmlData,
																	kCFPropertyListMutableContainers, //could also use kCFPropertyListImmutable, kCFPropertyListMutableContainers
																	&errorString);

			if ( configPropertyList != nil )
			{
				DbgLog( kLogPlugin, "Using Newly Replaced Search Node XML config file:" );
				DbgLog( kLogPlugin, "%s", string );
				//make the propertylist a dict
				if ( CFDictionaryGetTypeID() == CFGetTypeID( configPropertyList ) )
				{
					configDict = (CFMutableDictionaryRef) configPropertyList;
				}

				if (configDict != nil)
				{
					//config file version
					configVersion = GetVersion(configDict);
					if (configVersion == nil ) //release the dict and assume corrupted file
					{
						CFRelease(configDict);
						configDict			= nil;
						configPropertyList	= nil;
					}
					else
					{
						fSearchPolicy	= GetSearchPolicy(configDict);

						//set the member dict variable
						fConfigDict		= configDict;

						delete [] configVersion;
						configVersion = nil;
					}//if (configVersion != nil)
					// don't release the configDict since it is the cast configPropertyList
				}//if (configDict != nil)
			}//if (configPropertyList != nil )
			CFRelease(xmlData);
			xmlData = nil;
		} // if (xmlData != nil)
	} // if (bCorruptedFile)

	if (configFileURL != nil)
	{
		CFRelease(configFileURL); // seems okay to dealloc since Create used and done with it now
		configFileURL = nil;
	}
	if (configFileCorruptedURL != nil)
	{
		CFRelease(configFileCorruptedURL); // seems okay to dealloc since Create used and done with it now
		configFileCorruptedURL = nil;
	}
	if (sBase != nil)
	{
		CFRelease(sBase); // built with Copy so okay to dealloc
		sBase = nil;
	}
	if (sPath != nil)
	{
		CFRelease(sPath);
		sPath = nil;
	}
	if (filenameString != nil)
	{
		free(filenameString);
		filenameString = nil;
	}

    return( siResult );

} // ConfigSearchPolicy


#if AUGMENT_RECORDS
// ---------------------------------------------------------------------------
//	* UpdateAugmentDict
// ---------------------------------------------------------------------------

void CConfigs:: UpdateAugmentDict( CFDictionaryRef inDict )
{

	// update fConfigDict first
	CFDictionarySetValue(fConfigDict, fXMLAugmentSearchKeyString, (CFBooleanRef)CFDictionaryGetValue( inDict, fXMLAugmentSearchKeyString ));
	CFDictionarySetValue(fConfigDict, fXMLAugmentDirNodeNameKeyString, (CFBooleanRef)CFDictionaryGetValue( inDict, fXMLAugmentDirNodeNameKeyString ));
	CFDictionarySetValue(fConfigDict, fXMLToBeAugmentedDirNodeNameKeyString, (CFBooleanRef)CFDictionaryGetValue( inDict, fXMLToBeAugmentedDirNodeNameKeyString ));
	CFDictionarySetValue(fConfigDict, fXMLAugmentAttrListDictKeyString, (CFBooleanRef)CFDictionaryGetValue( inDict, fXMLAugmentAttrListDictKeyString ));
	WriteConfig();
	bAugmentSearch	= GetAugmentSearch(inDict);
	DSFreeString(fAugmentDirNodeName);
	fAugmentDirNodeName = GetAugmentDirNodeName(inDict);
	DSFreeString(fToBeAugmentedDirNodeName);
	fToBeAugmentedDirNodeName = GetToBeAugmentedDirNodeName(inDict);
	//retrieve the dictionary of attribute lists per record type
	//ie. dictionary of arrays where the array key is the record type
	//need to use fConfigDict after updating it above
	fAugmentAttrListDict = GetAugmentAttrListDict(fConfigDict);
}

#endif

// ---------------------------------------------------------------------------
//	* WriteConfig
// ---------------------------------------------------------------------------

SInt32 CConfigs:: WriteConfig ( void )
{
	SInt32					siResult			= eDSNoErr;
	CFURLRef				configFileURL;
	CFURLRef				configFileBackupURL;
	CFDataRef				xmlData;
	bool					bWroteFile			= false;
	bool					bReadFile			= false;
	char					string[ PATH_MAX ];
	struct stat				statResult;
	SInt32					errorCode			= 0;
	char				   *filenameString		= nil;

	//Config data is written to a XML file
	//Steps in the process:
	//1- see if the file exists
	//2- if it exists then overwrite it
	//KW 3- rename existing file and save it while creating a new file??
	//4- if file doesn't exist then create a new default file - make sure directories exist/if not create them

	//make sure file permissions are root only

	CFStringRef	sPath;

	sPath = ::CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "/Library/Preferences/DirectoryService/%s" ), fSearchNodeConfigFileName );

	::memset(string,0,PATH_MAX);
	::CFStringGetCString( sPath, string, sizeof( string ), kCFStringEncodingUTF8 );
	DbgLog( kLogPlugin, "Checking for Search Node XML config file:" );
	DbgLog( kLogPlugin, "%s", string );
	
	filenameString = strdup(string);

	// Convert it back into a CFURL.
	configFileURL = ::CFURLCreateWithFileSystemPath( kCFAllocatorDefault, sPath, kCFURLPOSIXPathStyle, false );
	CFRelease( sPath ); // build with Create so okay to dealloac here
	sPath = nil;

	//step 1- see if the file exists
	//if not then make sure the directories exist or create them
	//then write the file
	siResult = ::stat( string, &statResult );
	
	//if file exists then we make a backup copy - why? - because
	if (siResult == eDSNoErr)
	{
		CFStringRef	sBackupPath;

		sBackupPath = ::CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "/Library/Preferences/DirectoryService/%s" ), fSearchNodeConfigBackupFileName );

		// Convert it into a CFURL.
		configFileBackupURL = ::CFURLCreateWithFileSystemPath( kCFAllocatorDefault, sBackupPath, kCFURLPOSIXPathStyle, false );
		::memset(string,0,PATH_MAX);
		::CFStringGetCString( sBackupPath, string, sizeof( string ), kCFStringEncodingUTF8 );
		CFRelease( sBackupPath ); // build with Create so okay to dealloac here
		sBackupPath = nil;
		
		// Read the old XML property list file
		bReadFile = CFURLCreateDataAndPropertiesFromResource(
																kCFAllocatorDefault,
																configFileURL,
																&xmlData,          // place to put file data
																NULL,
																NULL,
																&siResult);
		//write the XML to the backup config file
		if (bReadFile)
		{

			bWroteFile = CFURLWriteDataAndPropertiesToResource( configFileBackupURL,
																xmlData,
																NULL,
																&errorCode);
			::chmod( string, 0600 );
			//KW check the error code and the result?

			CFRelease(xmlData);
		}
		if (configFileBackupURL != nil)
		{
			CFRelease(configFileBackupURL);
			configFileBackupURL = nil;
		}
	}
	//if file does not exist
	if (siResult != eDSNoErr)
	{
		siResult = eDSNoErr;
		//move down the path from the system defined local directory and check if it exists
		//if not create it
		sPath = ::CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%s" ), "/Library/Preferences" );
		::memset(string,0,PATH_MAX);
		::CFStringGetCString( sPath, string, sizeof( string ), kCFStringEncodingUTF8 );
		siResult = ::stat( string, &statResult );
		//if first sub directory does not exist
		if (siResult != eDSNoErr)
		{
			siResult = eDSNoErr;
			::mkdir( string , 0775 );
			::chmod( string, 0775 ); //above 0775 doesn't seem to work - looks like umask modifies it
		}
		CFRelease( sPath ); // build with Create so okay to dealloac here
		sPath = nil;
		//next subdirectory
		sPath = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%s" ), kDSLDAPPrefsDirPath );
		::memset(string,0,PATH_MAX);
		::CFStringGetCString( sPath, string, sizeof( string ), kCFStringEncodingUTF8 );
		siResult = ::stat( string, &statResult );
		//if second sub directory does not exist
		if (siResult != eDSNoErr)
		{
			siResult = eDSNoErr;
			::mkdir( string , 0775 );
			::chmod( string, 0775 ); //above 0775 doesn't seem to work - looks like umask modifies it
		}
		CFRelease( sPath ); // build with Create so okay to dealloac here
		sPath = nil;

	} // file does not exist so checking directory path to enable write of a new file

	//now write the updated file
	if (fConfigDict)
	{
		//convert the dict into a XML blob
		xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, fConfigDict);

		//write the XML to the config file
		bWroteFile = CFURLWriteDataAndPropertiesToResource( configFileURL,
														xmlData,
														NULL,
														&errorCode);
		::chmod( filenameString, 0600 );
		//KW check the error code and the result?

		CFRelease(xmlData);
	}

	if (configFileURL != nil)
	{
		CFRelease(configFileURL); // seems okay to dealloc since Create used and done with it now
		configFileURL = nil;
	}

	if (bWroteFile)
	{
		DbgLog( kLogPlugin, "Have written the Search Node XML config file:" );
		DbgLog( kLogPlugin, "%s", string );
		siResult = eDSNoErr;
	}
	else
	{
		DbgLog( kLogPlugin, "Search Node XML config file has NOT been written" );
		DbgLog( kLogPlugin, "Update to Custom Search Path Node List in Config File Failed" );
		siResult = eDSPlugInConfigFileError;
	}
	
	if (filenameString != nil)
	{
		free(filenameString);
		filenameString = nil;
	}
		
	return( siResult );

} // WriteConfig

// ---------------------------------------------------------------------------
//	* ConfigList
// ---------------------------------------------------------------------------

SInt32 CConfigs:: ConfigList ( void )
{
	SInt32					siResult			= eDSNoErr;
	sSearchList			   *pSearchNode			= nil;
	sSearchList			   *pTailSearchNode		= nil;
	CFStringRef				cfSearchNode;
	CFArrayRef				cfArrayRef			= nil;
	CFIndex					cfConfigCount		= 0;
	char				   *tmpBuff				= nil;
	const CFIndex			cfBuffSize			= 1024;
	char				   *outSearchNode		= nil;


	try
	{
		//array of search nodes for search policy IF custom used
		//but retrieve anyways for possible future use
		cfArrayRef = nil;
		cfArrayRef = GetListArray(fConfigDict);
		if (cfArrayRef != nil)
		{
			//now we can retrieve each search node IN ORDER
			cfConfigCount = ::CFArrayGetCount( cfArrayRef );
			//if (cfConfigCount == 0)
			//assume that this file has no Servers in it
			//and simply proceed forward ie. no Search Nodes will be obtained from data in this file
							
			//loop through the array
            //use iConfigIndex for the access to the cfArrayRef
			for (SInt32 iConfigIndex = 0; iConfigIndex < cfConfigCount; iConfigIndex++)
			{
				cfSearchNode = (CFStringRef)::CFArrayGetValueAtIndex( cfArrayRef, iConfigIndex );
				if ( cfSearchNode != nil )
				{
					if ( CFGetTypeID( cfSearchNode ) == CFStringGetTypeID() )
					{
						//assume that the extracted strings will be significantly less than 1024 characters
						tmpBuff = new char[cfBuffSize];
						::memset(tmpBuff,0,cfBuffSize);
						if (CFStringGetCString(cfSearchNode, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
						{
							outSearchNode = new char[1+strlen(tmpBuff)];
							::strcpy(outSearchNode, tmpBuff);

							pSearchNode = MakeListData(outSearchNode);
							delete [] outSearchNode;

							if (pSearchNodeList == nil)
							{
								pSearchNodeList = pSearchNode;												
								pTailSearchNode = pSearchNodeList;
							}
							else
							{
								while(pTailSearchNode->fNext != nil)
								{
									pTailSearchNode = pTailSearchNode->fNext;
								}
								pTailSearchNode->fNext = pSearchNode;
							}
							pSearchNode = nil;
						}
						delete [] tmpBuff;
					}
				}
			} // loop over search nodes
							
			//CFRelease( cfArrayRef ); // no since pointer only from Get
							
		} // if (cfArrayRef != nil) ie. an array of search nodes exists
		
	} // try
	catch( SInt32 err )
	{
		siResult = err;
	}
	return( siResult );

} // ConfigList

// --------------------------------------------------------------------------------
//	* GetVersion
// --------------------------------------------------------------------------------

char *CConfigs::GetVersion ( CFDictionaryRef configDict )
{
	char			   *outVersion	= nil;
	CFStringRef			cfStringRef	= nil;
	char			   *tmpBuff		= nil;
	const CFIndex		cfBuffSize	= 1024;

	if ( CFDictionaryContainsKey( configDict, fXMLSearchPathVersionKeyString ) )
	{
		cfStringRef = (CFStringRef)CFDictionaryGetValue( configDict, fXMLSearchPathVersionKeyString );
		if ( cfStringRef != nil )
		{
			if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
			{
				//assume that the extracted strings will be significantly less than 1024 characters
				tmpBuff = new char[cfBuffSize];
				::memset(tmpBuff,0,cfBuffSize);
				if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingUTF8 ))
				{
					outVersion = new char[1+strlen(tmpBuff)];
					::strcpy(outVersion, tmpBuff);
				}
				delete [] tmpBuff;
			}
		}
	}

	// return if nil or not
	return( outVersion );

} // GetVersion


#if AUGMENT_RECORDS
// --------------------------------------------------------------------------------
//	* AugmentDirNodeName
// --------------------------------------------------------------------------------

char *CConfigs::AugmentDirNodeName ( void )
{
	// return if nil or not
	return( fAugmentDirNodeName );

} // AugmentDirNodeName


// --------------------------------------------------------------------------------
//	* GetAugmentDirNodeName
// --------------------------------------------------------------------------------

char *CConfigs::GetAugmentDirNodeName ( CFDictionaryRef configDict )
{
	char			   *outName		= nil;
	CFStringRef			cfStringRef	= nil;
	char			   *tmpBuff		= nil;
	const CFIndex		cfBuffSize	= 1024;

	if ( CFDictionaryContainsKey( configDict, fXMLAugmentDirNodeNameKeyString ) )
	{
		cfStringRef = (CFStringRef)CFDictionaryGetValue( configDict, fXMLAugmentDirNodeNameKeyString );
		if ( cfStringRef != nil )
		{
			if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
			{
				//assume that the extracted strings will be significantly less than 1024 characters
				tmpBuff = new char[cfBuffSize];
				::memset(tmpBuff,0,cfBuffSize);
				if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingUTF8 ))
				{
					outName = new char[1+strlen(tmpBuff)];
					::strcpy(outName, tmpBuff);
				}
				delete [] tmpBuff;
			}
		}
	}

	// return if nil or not
	return( outName );

} // GetAugmentDirNodeName


// --------------------------------------------------------------------------------
//	* ToBeAugmentedDirNodeName
// --------------------------------------------------------------------------------

char *CConfigs::ToBeAugmentedDirNodeName ( void )
{
	// return if nil or not
	return( fToBeAugmentedDirNodeName );

} // ToBeAugmentedDirNodeName


// --------------------------------------------------------------------------------
//	* GetToBeAugmentedDirNodeName
// --------------------------------------------------------------------------------

char *CConfigs::GetToBeAugmentedDirNodeName ( CFDictionaryRef configDict )
{
	char			   *outName		= nil;
	CFStringRef			cfStringRef	= nil;
	char			   *tmpBuff		= nil;
	const CFIndex		cfBuffSize	= 1024;

	if ( CFDictionaryContainsKey( configDict, fXMLToBeAugmentedDirNodeNameKeyString ) )
	{
		cfStringRef = (CFStringRef)CFDictionaryGetValue( configDict, fXMLToBeAugmentedDirNodeNameKeyString );
		if ( cfStringRef != nil )
		{
			if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
			{
				//assume that the extracted strings will be significantly less than 1024 characters
				tmpBuff = new char[cfBuffSize];
				::memset(tmpBuff,0,cfBuffSize);
				if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingUTF8 ))
				{
					outName = new char[1+strlen(tmpBuff)];
					::strcpy(outName, tmpBuff);
				}
				delete [] tmpBuff;
			}
		}
	}

	// return if nil or not
	return( outName );

} // GetToBeAugmentedDirNodeName

// --------------------------------------------------------------------------------
//	* AugmentSearch
// --------------------------------------------------------------------------------

bool CConfigs::AugmentSearch ( void )
{
	return( bAugmentSearch );

} // AugmentSearch


// --------------------------------------------------------------------------------
//	* GetAugmentSearch
// --------------------------------------------------------------------------------

bool CConfigs::GetAugmentSearch ( CFDictionaryRef configDict )
{
	CFBooleanRef	cfBool		= false;
	bool			bAugSearch	= false;

	if ( CFDictionaryContainsKey( configDict, fXMLAugmentSearchKeyString ) )
	{
		cfBool = (CFBooleanRef)CFDictionaryGetValue( configDict, fXMLAugmentSearchKeyString );
		if ( cfBool != nil )
		{
			bAugSearch = CFBooleanGetValue(cfBool);
			//CFRelease( cfBool ); // no since pointer only from Get
		}
	}

	return( bAugSearch );

} // GetAugmentSearch
#endif

// --------------------------------------------------------------------------------
//	* GetSearchPolicy
// --------------------------------------------------------------------------------

UInt32 CConfigs:: GetSearchPolicy ( CFDictionaryRef configDict )
{
	UInt32				searchPolicy	= kAutomaticSearchPolicy; // default
	CFNumberRef			cfNumber		= 0;
	unsigned char		cfNumBool		= false;

	if ( CFDictionaryContainsKey( configDict, fXMLSearchPolicyKeyString ) )
	{
		cfNumber = (CFNumberRef)CFDictionaryGetValue( configDict, fXMLSearchPolicyKeyString );
		if ( cfNumber != nil )
		{
			cfNumBool = CFNumberGetValue(cfNumber, kCFNumberIntType, &searchPolicy);
			//CFRelease(cfNumber); // no since pointer only from Get
		}
	}

	return( searchPolicy );

} // GetSearchPolicy


#if AUGMENT_RECORDS
// --------------------------------------------------------------------------------
//	* AugmentAttrListDict
// --------------------------------------------------------------------------------

CFDictionaryRef CConfigs::AugmentAttrListDict ( void )
{
	// return if nil or not
	return( fAugmentAttrListDict );

} // AugmentAttrListDict


// --------------------------------------------------------------------------------
//	* GetAugmentAttrListDict
// --------------------------------------------------------------------------------

CFDictionaryRef CConfigs::GetAugmentAttrListDict ( CFDictionaryRef configDict )
{
	CFDictionaryRef cfDictRef = (CFDictionaryRef) CFDictionaryGetValue( configDict, fXMLAugmentAttrListDictKeyString );
	if ( cfDictRef != NULL && CFGetTypeID(cfDictRef) != CFDictionaryGetTypeID() ) {
		cfDictRef = NULL;
	}

	// return if nil or not
	return cfDictRef;

} // GetAugmentAttrListDict
#endif

// --------------------------------------------------------------------------------
//	* GetListArray
// --------------------------------------------------------------------------------

CFArrayRef CConfigs:: GetListArray ( CFDictionaryRef configDict )
{
	CFArrayRef		cfArrayRef	= nil;

	if ( CFDictionaryContainsKey( configDict, fXMLSearchPathArrayKeyString ) )
	{
		cfArrayRef = (CFArrayRef)CFDictionaryGetValue( configDict, fXMLSearchPathArrayKeyString );
	}

	// return if nil or not
	return( cfArrayRef );

} // GetListArray


// --------------------------------------------------------------------------------
//	* GetDHCPLDAPDictionary
// --------------------------------------------------------------------------------

CFDictionaryRef CConfigs::GetDHCPLDAPDictionary ( )
{
	CFDictionaryRef		cfDict = 0;

	if (fConfigDict)
	{
		if ( CFDictionaryContainsKey( fConfigDict, fXMLSearchDHCPLDAPString ) )
		{
			cfDict = (CFDictionaryRef)CFDictionaryGetValue( fConfigDict, fXMLSearchDHCPLDAPString );
		}
	}

	return( cfDict );

} // GetDHCPLDAPDictionary


// --------------------------------------------------------------------------------
//	* IsDHCPLDAPEnabled
//    checks if we should use DHCP LDAP nodes for the current location
// --------------------------------------------------------------------------------

bool CConfigs::IsDHCPLDAPEnabled ( )
{
	bool dhcpLDAPEnabled = false;
	CFDictionaryRef cfDict = 0;
	CFBooleanRef cfBool = 0;
	SCDynamicStoreRef sysConfigRef = 0;
	CFStringRef currentLocation = 0;

	if (fConfigDict)
	{
		sysConfigRef = SCDynamicStoreCreate(NULL,CFSTR("DirectoryService Search Node"),NULL,NULL);
		if (sysConfigRef != 0)
		{
			currentLocation = SCDynamicStoreCopyLocation(sysConfigRef);
			if (currentLocation != 0)
			{
				if ( CFDictionaryContainsKey( fConfigDict, fXMLSearchDHCPLDAPString ) )
				{
					cfDict = (CFDictionaryRef)CFDictionaryGetValue( fConfigDict, fXMLSearchDHCPLDAPString );
					if ( cfDict != 0 && CFGetTypeID(cfDict) == CFDictionaryGetTypeID() )
					{
						dhcpLDAPEnabled = false;
						cfBool = (CFBooleanRef)CFDictionaryGetValue( cfDict, currentLocation );
						if ( cfBool != 0 && CFGetTypeID(cfBool) == CFBooleanGetTypeID() )
						{
							dhcpLDAPEnabled = CFBooleanGetValue(cfBool);
						}
					}
				}
				CFRelease(currentLocation);
			}
			CFRelease(sysConfigRef);
		}
	}

	return( dhcpLDAPEnabled );

} // IsDHCPLDAPEnabled


// --------------------------------------------------------------------------------
//	* SetDHCPLDAPDictionary
// --------------------------------------------------------------------------------

void CConfigs::SetDHCPLDAPDictionary ( CFDictionaryRef dhcpLDAPdict )
{
	if (fConfigDict)
	{
		CFDictionarySetValue(fConfigDict, fXMLSearchDHCPLDAPString, dhcpLDAPdict);
	}
} // SetDHCPLDAPDictionary


// --------------------------------------------------------------------------------
//	* SetSearchPolicy
// --------------------------------------------------------------------------------

SInt32 CConfigs:: SetSearchPolicy ( UInt32 inSearchPolicy )
{
	CFNumberRef		cfNumber		= 0;
	SInt32			siResult		= eDSNoErr;

	fSearchPolicy = inSearchPolicy;
	if (fConfigDict)
	{
		cfNumber = CFNumberCreate(NULL,kCFNumberIntType,&inSearchPolicy);
		if ( CFDictionaryContainsKey( fConfigDict, fXMLSearchPolicyKeyString ) )
		{
			CFDictionaryReplaceValue(fConfigDict, fXMLSearchPolicyKeyString, cfNumber);
		}
		else
		{
			CFDictionarySetValue(fConfigDict, fXMLSearchPolicyKeyString, cfNumber);
		}
		if (cfNumber != 0)
		{
			CFRelease(cfNumber);
			cfNumber = 0;
		}
	}

	//KW need to error check this somehow
	return( siResult );

} // SetSearchPolicy


// --------------------------------------------------------------------------------
//	* SetListArray
// --------------------------------------------------------------------------------

SInt32 CConfigs:: SetListArray ( CFMutableArrayRef inCSPArray )
{
	SInt32			siResult	= eDSNoErr;

	if (fConfigDict)
	{
		if ( CFDictionaryContainsKey( fConfigDict, fXMLSearchPathArrayKeyString ) )
		{
			CFDictionaryReplaceValue(fConfigDict, fXMLSearchPathArrayKeyString, (const void *) inCSPArray);
		}
            else
            {
                CFDictionarySetValue(fConfigDict, fXMLSearchPathArrayKeyString, (const void *) inCSPArray);
            }
	}

	//KW need to error check this somehow
	return( siResult );

} // SetListArray


// ---------------------------------------------------------------------------
//	* MakeListData
// ---------------------------------------------------------------------------

sSearchList *CConfigs::MakeListData ( char *inNodeName )
{
	SInt32				siResult		= eDSNoErr;
    sSearchList		   *listOut			= nil;

	try
	{
    	listOut =  (sSearchList *) ::calloc( 1, sizeof(sSearchList));
    	if ( listOut != nil )
    	{
        	//do nothing with return here since we know this is new
        	//and we did a memset above
        	siResult = CleanListData(listOut);

	    	listOut->fNodeName = new char[1+::strlen(inNodeName)];
	    	::strcpy(listOut->fNodeName,inNodeName);

			listOut->fDataList = ::dsBuildFromPathPriv( listOut->fNodeName, "/" );

			//open the nodes lazily when they are actually needed
			/*
			if (fDirRef == 0)
			{
				siResult = ::dsOpenDirService( &fDirRef );
				if ( siResult != eDSNoErr ) throw( (SInt32)eDSOpenFailed );
			}
			siResult = ::dsOpenDirNode( fDirRef, listOut->fDataList, &listOut->fNodeRef );
			if ( siResult != eDSNoErr )
			{
				DbgLog( kLogPlugin, "Failed to open node: %s with error: %l", listOut->fNodeName, siResult );
				DbgLog( kLogPlugin, "Will attempt to open again later?" );
				siResult = eDSNoErr;
			}
			else
			{
				DbgLog( kLogPlugin, "  Node Reference = %l", listOut->fNodeRef );
				listOut->fOpened = true;
			}
			*/
    	}
	}
	catch( SInt32 err )
	{
		siResult = err;
	}
	
	return( listOut );

} // MakeListData


// ---------------------------------------------------------------------------
//	* CleanListData
// ---------------------------------------------------------------------------

SInt32 CConfigs::CleanListData ( sSearchList *inList )
{
    SInt32				siResult		= eDSNoErr;

    if ( inList != nil )
    {
		if ( inList->fNodeName != NULL ) {
			delete [] inList->fNodeName;
			inList->fNodeName = NULL;
		}
		
		inList->fOpened					= false;
		inList->fHasNeverOpened		= true;
		inList->fNodeReachable			= false;
		if (inList->fNodeRef != 0)
		{
			::dsCloseDirNode(inList->fNodeRef); // don't check error code
			inList->fNodeRef				= 0;
		}
		inList->fNext					= nil;
        if (inList->fDataList != nil)
        {
			dsDataListDeallocatePriv ( inList->fDataList );
			//need to free the header as well
			free( inList->fDataList );
			inList->fDataList = nil;
		}
        
   }

    return( siResult );

} // CleanListData

