/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*!
 * @header CConfigs
 * Code to parse a XML config file.
 */

#include "CConfigs.h"
#include "CSharedData.h"

#include <CoreFoundation/CFPriv.h>		// used for ::CFCopySearchPathForDirectoriesInDomains
#include <SystemConfiguration/SystemConfiguration.h>

#include <string.h>				//used for strcpy, etc.
#include <stdlib.h>				//used for malloc
#include <sys/stat.h>			//used for mkdir and stat

#include "PrivateTypes.h"
#include "DSUtils.h"

#define kAllocatorDefault NULL


// --------------------------------------------------------------------------------
//	* CConfigs
// --------------------------------------------------------------------------------

CConfigs::CConfigs ( void )
{
	fSearchPolicy						= kNetInfoSearchPolicy;
    fSearchNodeListLength				= 0;
    pSearchNodeList						= nil;
	fDirRef								= 0;
	fConfigDict							= nil;
	fSearchNodeConfigFileName			= nil;
	fSearchNodeConfigBackupFileName		= nil;
	fSearchNodeConfigCorruptedFileName	= nil;
} // CConfigs


// --------------------------------------------------------------------------------
//	* ~CConfigs ()
// --------------------------------------------------------------------------------

CConfigs::~CConfigs ( void )
{
	//need to cleanup the struct list ie. the internals
	//NO NO these are created on demand and owned by the caller to GetCustom
	//KW should use local vars for this purpose
//	pList = pSearchNodeList;
//    while (pList != nil)
//    {
//			pDeleteList = pList;
//			pList = pList->fNext;		//assign to next BEFORE deleting current
//			CleanListData( pDeleteList );
//			delete( pDeleteList );
//			pDeleteList = nil;
//    }

	//KW might consider cleanup of the fDirRef here
	if (fConfigDict)
	{
		CFRelease(fConfigDict);
	}
	if (fSearchNodeConfigFileName != nil)
	{
		free(fSearchNodeConfigFileName);
		fSearchNodeConfigFileName = nil;
	}
	if (fSearchNodeConfigBackupFileName != nil)
	{
		free(fSearchNodeConfigBackupFileName);
		fSearchNodeConfigBackupFileName = nil;
	}
	if (fSearchNodeConfigCorruptedFileName != nil)
	{
		free(fSearchNodeConfigCorruptedFileName);
		fSearchNodeConfigCorruptedFileName = nil;
	}

} // ~CConfigs


// --------------------------------------------------------------------------------
//	* Init (uInt32)
// --------------------------------------------------------------------------------

sInt32 CConfigs::Init ( const char *inSearchNodeConfigFilePrefix, uInt32 &outSearchPolicy )
{

	sInt32	siResult = eDSNullParameter;
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
		}
			
	} // try
	catch( sInt32 err )
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
	sInt32			siResult	= eDSNoErr;

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

sInt32 CConfigs:: ConfigSearchPolicy ( void )
{
    sInt32					siResult				= eDSNoErr;
    sInt32					result					= eDSNoErr;
    CFStringRef				errorString;
    CFURLRef				configFileURL			= nil;
    CFURLRef				configFileCorruptedURL	= nil;
    CFDataRef				xmlData					= nil;
    CFPropertyListRef		configPropertyList		= nil;
    CFMutableDictionaryRef	configDict				= nil;
	bool					bFileOpSuccess			= false;
	bool					bWroteFile				= false;
	bool					bCorruptedFile			= false;
    register CFIndex		iPath					= 0;
    CFArrayRef				aPaths					= nil;
    char					string[ PATH_MAX ];
    char				   *configVersion			= nil;
	CFNumberRef				aSearchPolicy;
	struct stat				statResult;
	CFStringRef				cfStringRef				= nil;
	sInt32					errorCode				= 0;
	int						defaultSearchPolicy		= 1;
	CFStringRef				sBase					= nil;
	CFStringRef				sPath					= nil;
	CFStringRef				sCorruptedPath			= nil;
	bool					bUseXMLData				= false;

	//Config data is read from a XML file OR created as default
	//KW eventually use Version from XML file to check against the code here?
	//Steps in the process:
	//1- see if the file exists
	//2- if it exists then try to read it otherwise just use created xmldata
	//3- if existing file is corrupted then rename it and save it while creating a new default file
	//4- if file doesn't exist then create a new default file - make sure directories exist/if not create them
    //make sure file permissions are root only
	//keep on going with a fConfigDict regardless whether file access works

	// Get the local library search path -- only expect a single one
	aPaths = ::CFCopySearchPathForDirectoriesInDomains( kCFLibraryDirectory, (CFSearchPathDomainMask)kCFLocalDomainMask, true );
	if ( aPaths != nil )
	{
		iPath = ::CFArrayGetCount( aPaths );
		if ( iPath != 0 )
		{
			// count down here if more that the Local directory is specified
			// ie. in Local ( or user's home directory ).
			// for now reality is that there is NO countdown
			while (( iPath-- ) && (!bUseXMLData))
			{
				configFileURL = (CFURLRef)::CFArrayGetValueAtIndex( aPaths, iPath );

				// Append the subpath and clean up the sBase if the while loop was used
				// since we can't clean at the bottom of the while since sBase might be used later in this routine
				if (sBase != nil)
				{
					CFRelease(sBase); // built with Copy last time in the while loop so okay to dealloc
					sBase = nil;
				}
				sBase = ::CFURLCopyFileSystemPath( configFileURL, kCFURLPOSIXPathStyle );
				sPath = ::CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@/%s/%s" ), sBase, "/Preferences/DirectoryService", fSearchNodeConfigFileName );

				::memset(string,0,PATH_MAX);
				::CFStringGetCString( sPath, string, sizeof( string ), kCFStringEncodingMacRoman );
CShared::LogIt( 0x0F, (char *)"Checking for Search Node XML config file:" );
				CShared::LogIt( 0x0F, string );

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
					::CFStringGetCString( sPath, string, sizeof( string ), kCFStringEncodingMacRoman );
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
					::CFStringGetCString( sPath, string, sizeof( string ),  kCFStringEncodingMacRoman );
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

					cfStringRef = CFSTR("Search Node PlugIn Version 1.2");
					CFDictionarySetValue( configDict, CFSTR( kXMLSearchPathVersionKey ), cfStringRef );
					//CFRelease(cfStringRef);
					// we don't need to release CFSTR() created strings that we didn't retain
					aSearchPolicy = CFNumberCreate(NULL,kCFNumberIntType,&defaultSearchPolicy);
					CFDictionarySetValue( configDict, CFSTR( kXMLSearchPolicyKey ), aSearchPolicy ); // default NetInfo search policy CFNumber
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
					CFRelease(configDict);
					configDict = nil;
					//CFRelease(xmlData); //keeping this data for use below
				} // file does not exist so creating one
				else //try to read the existing file
				{
					// Read the XML property list file
					bFileOpSuccess = CFURLCreateDataAndPropertiesFromResource(	kAllocatorDefault,
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

			} // while (( iPath-- ) && (!bUseXMLData))
		} // if ( iPath != 0 )
		CFRelease(aPaths); // seems okay since Created above
	}// if ( aPaths != nil )

	if (xmlData != nil)
	{
		// extract the config dictionary from the XML data.
		configPropertyList = CFPropertyListCreateFromXMLData(	kAllocatorDefault,
																xmlData,
																kCFPropertyListMutableContainers, //could also use kCFPropertyListImmutable, kCFPropertyListMutableContainers
																&errorString);

		if (configPropertyList != nil )
		{

			CShared::LogIt( 0x0F, (char *)"Have read Search Node XML config file:" );
			CShared::LogIt( 0x0F, string );

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

					free(configVersion);
					configVersion = nil;
				}//if (configVersion != nil)
				// don't release the configDict since it is the cast configPropertyList
			}//if (configDict != nil)
		}//if (configPropertyList != nil )
		if ( configPropertyList == nil) //we have a corrupted file
		{
			CShared::LogIt( 0x0F, (char *)"Search Node XML config file is corrupted" );
			CShared::LogIt( 0x0F, (char *)"Using default NetInfo Search Policy" );
			bCorruptedFile = true;
			//here we need to make a backup of the file - why? - because

			// Append the subpath.
			sCorruptedPath = ::CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@/%s/%s" ), sBase, "/Preferences/DirectoryService", fSearchNodeConfigCorruptedFileName );

			// Convert it into a CFURL.
			configFileCorruptedURL = ::CFURLCreateWithFileSystemPath( kCFAllocatorDefault, sCorruptedPath, kCFURLPOSIXPathStyle, false );
			CFRelease( sCorruptedPath ); // build with Create so okay to dealloac here
			sCorruptedPath = nil;

			//write the XML to the corrupted copy of the config file
			bWroteFile = CFURLWriteDataAndPropertiesToResource(	configFileCorruptedURL,
																xmlData,
																NULL,
																&errorCode);

		} //couldn't extract the property list out of the file or the version didn't exist
		CFRelease(xmlData); // probably okay to dealloc since Create used and no longer needed
		xmlData = nil;
	}
	else
	{
		CShared::LogIt( 0x0F, (char *)"Search Node XML config file is not readable" );
		CShared::LogIt( 0x0F, (char *)"Using default NetInfo Search Policy" );
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

		cfStringRef = CFSTR("Search Node PlugIn Version 1.2");
		CFDictionarySetValue( configDict, CFSTR( kXMLSearchPathVersionKey ), cfStringRef );
		//CFRelease(cfStringRef);
		// don't release CFSTR() string if not retained
		aSearchPolicy = CFNumberCreate(NULL,kCFNumberIntType,&defaultSearchPolicy);
		CFDictionarySetValue( configDict, CFSTR( kXMLSearchPolicyKey ), aSearchPolicy ); // default NetInfo search policy CFNumber
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
		sPath = ::CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%s/%s" ), sBase, "/Library/Preferences/DirectoryService", fSearchNodeConfigFileName );

		if (configFileURL != nil)
		{
			CFRelease(configFileURL);
		}
		// Convert it back into a CFURL.
		configFileURL = ::CFURLCreateWithFileSystemPath( kCFAllocatorDefault, sPath, kCFURLPOSIXPathStyle, false );

		//write the newly created XML to the config file
		bWroteFile = CFURLWriteDataAndPropertiesToResource(	configFileURL,
															xmlData,
															NULL,
															&errorCode);
		if (xmlData != nil)
		{
			configPropertyList = CFPropertyListCreateFromXMLData(	kAllocatorDefault,
																	xmlData,
																	kCFPropertyListMutableContainers, //could also use kCFPropertyListImmutable, kCFPropertyListMutableContainers
																	&errorString);

			if ( configPropertyList != nil )
			{
				CShared::LogIt( 0x0F, (char *)"Using Newly Replaced Search Node XML config file:" );
				CShared::LogIt( 0x0F, string );
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

						free(configVersion);
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

    return( siResult );

} // ConfigSearchPolicy

// ---------------------------------------------------------------------------
//	* WriteConfig
// ---------------------------------------------------------------------------

sInt32 CConfigs:: WriteConfig ( void )
{
	sInt32					siResult			= eDSNoErr;
	CFURLRef				configFileURL;
	CFURLRef				configFileBackupURL;
	CFDataRef				xmlData;
	bool					bWroteFile			= false;
	bool					bReadFile			= false;
	register CFIndex		iPath;
	CFArrayRef				aPaths				= nil;
	char					string[ PATH_MAX ];
	struct stat				statResult;
	sInt32					errorCode			= 0;

        //Config data is written to a XML file
        //Steps in the process:
        //1- see if the file exists
        //2- if it exists then overwrite it
        //KW 3- rename existing file and save it while creating a new file??
        //4- if file doesn't exist then create a new default file - make sure directories exist/if not create them

        //make sure file permissions are root only

	try
	{
            // Get the local library search path -- only expect a single one
            aPaths = ::CFCopySearchPathForDirectoriesInDomains( kCFLibraryDirectory, (CFSearchPathDomainMask)kCFLocalDomainMask, true );
            if ( aPaths != nil )
            {
                iPath = ::CFArrayGetCount( aPaths );
                if ( iPath != 0 )
                {
                    // count down here if more that the Local directory is specified
                    // ie. in Local ( or user's home directory ).
                    // for now reality is that there is NO countdown
                    while (( iPath-- ) && (!bWroteFile))
                    {
                        configFileURL = (CFURLRef)::CFArrayGetValueAtIndex( aPaths, iPath );
                        CFStringRef	sBase, sPath;

                        // Append the subpath.
                        sBase = ::CFURLCopyFileSystemPath( configFileURL, kCFURLPOSIXPathStyle );
                        sPath = ::CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@/%s/%s" ), sBase, "/Preferences/DirectoryService", fSearchNodeConfigFileName );

                        ::memset(string,0,PATH_MAX);
						::CFStringGetCString( sPath, string, sizeof( string ), kCFStringEncodingMacRoman );
CShared::LogIt( 0x0F, (char *)"Checking for Search Node XML config file:" );
                        CShared::LogIt( 0x0F, string );

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

                            // Append the subpath.
                            sBackupPath = ::CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@/%s/%s" ), sBase, "/Preferences/DirectoryService", fSearchNodeConfigBackupFileName );

                            // Convert it into a CFURL.
                            configFileBackupURL = ::CFURLCreateWithFileSystemPath( kCFAllocatorDefault, sBackupPath, kCFURLPOSIXPathStyle, false );
                            CFRelease( sBackupPath ); // build with Create so okay to dealloac here
                            sBackupPath = nil;
                            
                            // Read the old XML property list file
                            bReadFile = CFURLCreateDataAndPropertiesFromResource(
                                                                                    kAllocatorDefault,
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
                            sPath = ::CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@/%s" ), sBase, "/Preferences" );
                            ::memset(string,0,PATH_MAX);
                            ::CFStringGetCString( sPath, string, sizeof( string ), kCFStringEncodingMacRoman );
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
                            sPath = ::CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@/%s" ), sBase, "/Preferences/DirectoryService" );
                            ::memset(string,0,PATH_MAX);
							::CFStringGetCString( sPath, string, sizeof( string ), kCFStringEncodingMacRoman );
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
                                //KW check the error code and the result?

                                CFRelease(xmlData);
                            }

                        CFRelease( sBase ); // built with Copy so okay to dealloc
                        sBase = nil;

						if (configFileURL != nil)
						{
                        	CFRelease(configFileURL); // seems okay to dealloc since Create used and done with it now
							configFileURL = nil;
						}

                    } // while (( iPath-- ) && (!bWroteFile))
                } // if ( iPath != 0 )

                CFRelease(aPaths); // seems okay since Created above
            }// if ( aPaths != nil )

            
		if (bWroteFile)
		{
			CShared::LogIt( 0x0F, (char *)"Have written the Search Node XML config file:" );
			CShared::LogIt( 0x0F, string );
                    siResult = eDSNoErr;
		}
		else
		{
			CShared::LogIt( 0x0F, (char *)"Search Node XML config file has NOT been written" );
			CShared::LogIt( 0x0F, (char *)"Update to Custom Search Path Node List in Config File Failed" );
			siResult = eDSPlugInConfigFileError;
		}
		
	} // try
	catch( sInt32 err )
	{
		siResult = err;
	}
	return( siResult );

} // WriteConfig

// ---------------------------------------------------------------------------
//	* ConfigList
// ---------------------------------------------------------------------------

sInt32 CConfigs:: ConfigList ( void )
{
	sInt32					siResult			= eDSNoErr;
	sSearchList			   *pSearchNode			= nil;
	sSearchList			   *pTailSearchNode		= nil;
	CFStringRef				cfSearchNode;
	CFArrayRef				cfArrayRef			= nil;
	CFIndex					cfConfigCount		= 0;
	char				   *tmpBuff				= nil;
	CFIndex					cfBuffSize			= 1024;
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
			for (sInt32 iConfigIndex = 0; iConfigIndex < cfConfigCount; iConfigIndex++)
			{
				cfSearchNode = (CFStringRef)::CFArrayGetValueAtIndex( cfArrayRef, iConfigIndex );
				if ( cfSearchNode != nil )
				{
					if ( CFGetTypeID( cfSearchNode ) == CFStringGetTypeID() )
					{
						//assume that the extracted strings will be significantly less than 1024 characters
						tmpBuff = new char[1024];
						::memset(tmpBuff,0,1024);
						if (CFStringGetCString(cfSearchNode, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
						{
							outSearchNode = new char[1+strlen(tmpBuff)];
							::strcpy(outSearchNode, tmpBuff);

							pSearchNode = MakeListData(outSearchNode);
							delete(outSearchNode);

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
						delete( tmpBuff );
					}
				}
			} // loop over search nodes
							
			//CFRelease( cfArrayRef ); // no since pointer only from Get
							
		} // if (cfArrayRef != nil) ie. an array of search nodes exists
		
	} // try
	catch( sInt32 err )
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
	CFIndex				cfBuffSize	= 1024;

	if ( CFDictionaryContainsKey( configDict, CFSTR( kXMLSearchPathVersionKey ) ) )
	{
		cfStringRef = (CFStringRef)CFDictionaryGetValue( configDict, CFSTR( kXMLSearchPathVersionKey ) );
		if ( cfStringRef != nil )
		{
			if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
			{
				//assume that the extracted strings will be significantly less than 1024 characters
				tmpBuff = new char[1024];
				::memset(tmpBuff,0,1024);
				if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingUTF8 ))
				{
					outVersion = new char[1+strlen(tmpBuff)];
					::strcpy(outVersion, tmpBuff);
				}
				delete( tmpBuff );
			}
		}
	}

	// return if nil or not
	return( outVersion );

} // GetVersion


// --------------------------------------------------------------------------------
//	* GetSearchPolicy
// --------------------------------------------------------------------------------

uInt32 CConfigs:: GetSearchPolicy ( CFDictionaryRef configDict )
{
	uInt32				searchPolicy	= kNetInfoSearchPolicy; // default
	CFNumberRef			cfNumber		= 0;
	unsigned char		cfNumBool		= false;

	if ( CFDictionaryContainsKey( configDict, CFSTR( kXMLSearchPolicyKey ) ) )
	{
		cfNumber = (CFNumberRef)CFDictionaryGetValue( configDict, CFSTR( kXMLSearchPolicyKey ) );
		if ( cfNumber != nil )
		{
			cfNumBool = CFNumberGetValue(cfNumber, kCFNumberIntType, &searchPolicy);
			//CFRelease(cfNumber); // no since pointer only from Get
		}
	}

	return( searchPolicy );

} // GetSearchPolicy


// --------------------------------------------------------------------------------
//	* GetListArray
// --------------------------------------------------------------------------------

CFArrayRef CConfigs:: GetListArray ( CFDictionaryRef configDict )
{
	CFArrayRef		cfArrayRef	= nil;

	if ( CFDictionaryContainsKey( configDict, CFSTR( kXMLSearchPathArrayKey ) ) )
	{
		cfArrayRef = (CFArrayRef)CFDictionaryGetValue( configDict, CFSTR( kXMLSearchPathArrayKey ) );
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
		if ( CFDictionaryContainsKey( fConfigDict, CFSTR( kXMLSearchDHCPLDAP ) ) )
		{
			cfDict = (CFDictionaryRef)CFDictionaryGetValue( fConfigDict, CFSTR( kXMLSearchDHCPLDAP ) );
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
	bool dhcpLDAPEnabled = true;
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
				if ( CFDictionaryContainsKey( fConfigDict, CFSTR( kXMLSearchDHCPLDAP ) ) )
				{
					cfDict = (CFDictionaryRef)CFDictionaryGetValue( fConfigDict, CFSTR( kXMLSearchDHCPLDAP ) );
					if ( cfDict != 0 && CFGetTypeID(cfDict) == CFDictionaryGetTypeID() )
					{
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
		CFDictionarySetValue(fConfigDict, CFSTR( kXMLSearchDHCPLDAP ), dhcpLDAPdict);
	}
} // SetDHCPLDAPDictionary


// --------------------------------------------------------------------------------
//	* SetSearchPolicy
// --------------------------------------------------------------------------------

sInt32 CConfigs:: SetSearchPolicy ( uInt32 inSearchPolicy )
{
	CFNumberRef		cfNumber		= 0;
	sInt32			siResult		= eDSNoErr;

	fSearchPolicy = inSearchPolicy;
	if (fConfigDict)
	{
		cfNumber = CFNumberCreate(NULL,kCFNumberIntType,&inSearchPolicy);
		if ( CFDictionaryContainsKey( fConfigDict, CFSTR( kXMLSearchPolicyKey ) ) )
		{
			CFDictionaryReplaceValue(fConfigDict, CFSTR( kXMLSearchPolicyKey ), cfNumber);
		}
		else
		{
			CFDictionarySetValue(fConfigDict, CFSTR( kXMLSearchPolicyKey ), cfNumber);
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

sInt32 CConfigs:: SetListArray ( CFMutableArrayRef inCSPArray )
{
	sInt32			siResult	= eDSNoErr;

	if (fConfigDict)
	{
		if ( CFDictionaryContainsKey( fConfigDict, CFSTR( kXMLSearchPathArrayKey ) ) )
		{
			CFDictionaryReplaceValue(fConfigDict, CFSTR( kXMLSearchPathArrayKey ), (const void *) inCSPArray);
		}
            else
            {
                CFDictionarySetValue(fConfigDict, CFSTR( kXMLSearchPathArrayKey ), (const void *) inCSPArray);
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
	sInt32				siResult		= eDSNoErr;
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
				if ( siResult != eDSNoErr ) throw( (sInt32)eDSOpenFailed );
			}
			siResult = ::dsOpenDirNode( fDirRef, listOut->fDataList, &listOut->fNodeRef );
			if ( siResult != eDSNoErr )
			{
				CShared::LogIt( 0x0F, "Failed to open node: %s with error: %l", listOut->fNodeName, siResult );
				CShared::LogIt( 0x0F, "Will attempt to open again later?" );
				siResult = eDSNoErr;
			}
			else
			{
				CShared::LogIt( 0x0F, "  Node Reference = %l", listOut->fNodeRef );
				listOut->fOpened = true;
			}
			*/
    	}
	}
	catch( sInt32 err )
	{
		siResult = err;
	}
	
	return( listOut );

} // MakeListData


// ---------------------------------------------------------------------------
//	* CleanListData
// ---------------------------------------------------------------------------

sInt32 CConfigs::CleanListData ( sSearchList *inList )
{
    sInt32				siResult		= eDSNoErr;

    if ( inList != nil )
    {
        if (inList->fNodeName != nil)
        {
            delete ( inList->fNodeName );
        }
		inList->fOpened					= false;
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

