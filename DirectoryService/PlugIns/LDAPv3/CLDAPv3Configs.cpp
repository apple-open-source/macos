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
 * @header CLDAPv3Configs
 * Code to parse a XML file and place the contents into a table of structs.
 */

#include <string.h>				//used for strcpy, etc.
#include <stdlib.h>				//used for malloc
#include <sys/types.h>
#include <sys/stat.h>			//used for mkdir and stat
#include <syslog.h>				//error logging

#include "CLDAPv3Configs.h"
#include "CLog.h"
#include "DSLDAPUtils.h"

#include "DirServices.h"
#include "DirServicesUtils.h"
#include "DirServicesConst.h"

// --------------------------------------------------------------------------------
//	* CLDAPv3Configs
// --------------------------------------------------------------------------------

CLDAPv3Configs::CLDAPv3Configs ( void )
{
	pConfigTable			= nil;
	fConfigTableLen			= 0;
	fXMLData				= nil;
	pXMLConfigLock			= new DSMutexSemaphore();
} // CLDAPv3Configs


// --------------------------------------------------------------------------------
//	* ~CLDAPv3Configs ()
// --------------------------------------------------------------------------------

CLDAPv3Configs::~CLDAPv3Configs ( void )
{
    uInt32				iTableIndex	= 0;
    sLDAPConfigData	   *pConfig		= nil;

	//need to cleanup the config table ie. the internals
    for (iTableIndex=0; iTableIndex<fConfigTableLen; iTableIndex++)
    {
        pConfig = (sLDAPConfigData *)pConfigTable->GetItemData( iTableIndex );
        if (pConfig != nil)
        {
            // delete the contents of sLDAPConfigData here
            // not checking the return status of the clean here
			// since don't plan to continue
            CleanLDAPConfigData( pConfig );
            // delete the sLDAPConfigData itself
            delete( pConfig );
            pConfig = nil;
            // remove the table entry
            pConfigTable->RemoveItem( iTableIndex );
        }
    }
    fConfigTableLen = 0;
    if ( pConfigTable != nil)
    {
        delete ( pConfigTable );
        pConfigTable = nil;
    }
	
	if (pXMLConfigLock != nil)
	{
		delete(pXMLConfigLock);
		pXMLConfigLock = nil;
	}
	
	if (fXMLData != nil)
	{
		CFRelease(fXMLData);
		fXMLData = nil;
	}

} // ~CLDAPv3Configs


// --------------------------------------------------------------------------------
//	* Init (CPlugInRef, uInt32)
// --------------------------------------------------------------------------------

sInt32 CLDAPv3Configs::Init ( CPlugInRef *inConfigTable, uInt32 &inConfigTableLen )
{

	sInt32				siResult	= eDSNoErr;
	sLDAPConfigData	   *pConfig		= nil;
	uInt32				sIndex		= 0;
    uInt32				iTableIndex	= 0;

	//Init is set up so that if it is called initially or by a custom call
	//it will keep on adding and deleting configs as required
	//if (inConfigTableLen != 0)
	//{
			//fConfigTableLen = inConfigTableLen;
	//}
	if ( inConfigTable == nil )
	{
			inConfigTable = new CPlugInRef( nil );
	}

	pConfigTable = inConfigTable;

	//check for Generic node which has server name "unknown"
	if (!CheckForConfig((char *)"unknown", sIndex))
	{
		//build a default config entry that can be used when no config exists
		pConfig = MakeLDAPConfigData((char *)"Generic",(char *)"unknown",15,2,120,120,389,false, 0, 0, false, false, false, nil, true, nil );
		pConfigTable->AddItem( fConfigTableLen, pConfig );
		fConfigTableLen++;
	}

	XMLConfigLock();
	//read the XML Config file
	if (fXMLData != nil)
	{
		CFRelease(fXMLData);
		fXMLData = nil;
	}
	siResult = ReadXMLConfig();
	XMLConfigUnlock();
	
	//check if XML file was read
	if (siResult == eDSNoErr)
	{
		//need to set the Updated flag to false so that nodes will get Unregistered
		//if a config no longer exists for that entry
		//this needs to be done AFTER it is verified that a XML config file exists
		//if (inConfigTableLen != 0)
		//{
			//need to cycle through the config table
			for (iTableIndex=0; iTableIndex<fConfigTableLen; iTableIndex++)
			{
				pConfig = (sLDAPConfigData *)pConfigTable->GetItemData( iTableIndex );
				if (pConfig != nil)
				{
					pConfig->bUpdated = false;
				}
			}
		//}
	
		//set up the config table
		XMLConfigLock();
		siResult = ConfigLDAPServers();
		XMLConfigUnlock();
	}
	
	//set/update the number of configs in the table
	inConfigTableLen = fConfigTableLen;

	return( siResult );

} // Init


// ---------------------------------------------------------------------------
//	* ReadXMLConfig
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Configs::ReadXMLConfig ( void )
{
	sInt32					siResult				= eDSNoErr;
	CFURLRef				configFileURL			= NULL;
	CFURLRef				configFileCorruptedURL	= NULL;
	CFDataRef				xmlData					= NULL;
	struct stat				statResult;
	bool					bReadFile				= false;
	bool					bCorruptedFile			= false;
	bool					bWroteFile				= false;
	CFMutableDictionaryRef	configDict				= NULL;
	sInt32					errorCode				= 0;
	CFStringRef				sCorruptedPath			= NULL;
	char				   *filenameString			= "/Library/Preferences/DirectoryService/DSLDAPv3PlugInConfig.plist";

//Config data is read from a XML file
//KW eventually use Version from XML file to check against the code here?
//Steps in the process:
//1- see if the file exists
//2- if it exists then try to read it
//3- if existing file is corrupted then rename it and save it while creating a new default file
//4- if file doesn't exist then create a new default file - make sure directories exist/if not create them
	
	while ( !bReadFile )
	{
		//step 1- see if the file exists
		//if not then make sure the directories exist or create them
		//then write the file
		siResult = ::stat( filenameString, &statResult );
		
		CFStringRef sPath = CFStringCreateWithCString( kCFAllocatorDefault, filenameString, kCFStringEncodingUTF8 );
		
		configFileURL = ::CFURLCreateWithFileSystemPath( kCFAllocatorDefault, sPath, kCFURLPOSIXPathStyle, false );
		CFRelease( sPath );
		
		//if file does not exist, let's make sure the directories are there
		if (siResult != eDSNoErr)
		{
			// file does not exist so checking directory path to enable write of a new file
			CreatePrefDirectory();
			
			//create a new dictionary for the file
			configDict = CFDictionaryCreateMutable( kCFAllocatorDefault,
													0,
													&kCFTypeDictionaryKeyCallBacks,
													&kCFTypeDictionaryValueCallBacks );
			
			CFDictionarySetValue( configDict, CFSTR( kXMLLDAPVersionKey ), CFSTR( "DSLDAPv3PlugIn Version 1.5" ) );
			
			DBGLOG( kLogPlugin, "Created a new LDAP XML config file since it did not exist" );
			//convert the dict into a XML blob
			xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, configDict);
			
			//write the XML to the config file
			siResult = CFURLWriteDataAndPropertiesToResource( configFileURL,
																xmlData,
																NULL,
																&errorCode);
			::chmod( filenameString, 0600 );
			//KW check the error code and the result?
			
			CFRelease(configDict);
			configDict = nil;
			CFRelease(xmlData);
			xmlData = nil;
			
		} // file does not exist so creating one
		chmod( filenameString, S_IRUSR | S_IWUSR );
		// Read the XML property list file
		bReadFile = CFURLCreateDataAndPropertiesFromResource(
																kCFAllocatorDefault,
																configFileURL,
																&xmlData,          // place to put file data
																NULL,           
																NULL,
																&siResult);
					
	} // while (!bReadFile)
		
	if (bReadFile)
	{
		fXMLData = xmlData;
		//check if this XML blob is a property list and can be made into a dictionary
		if (!VerifyXML())
		{
			char	*corruptPath = "/Library/Preferences/DirectoryService/DSLDAPv3PlugInConfigCorrupted.plist";
			
			//if it is not then say the file is corrupted and save off the corrupted file
			DBGLOG( kLogPlugin, "LDAP XML config file is corrupted" );
			bCorruptedFile = true;
			//here we need to make a backup of the file - why? - because

			// Append the subpath.
			sCorruptedPath = ::CFStringCreateWithCString( kCFAllocatorDefault, corruptPath, kCFStringEncodingUTF8 );

			// Convert it into a CFURL.
			configFileCorruptedURL = ::CFURLCreateWithFileSystemPath( kCFAllocatorDefault, sCorruptedPath, kCFURLPOSIXPathStyle, false );
			CFRelease( sCorruptedPath ); // build with Create so okay to dealloac here
			sCorruptedPath = nil;

			//write the XML to the corrupted copy of the config file
			bWroteFile = CFURLWriteDataAndPropertiesToResource( configFileCorruptedURL,
																xmlData,
																NULL,
																&errorCode);
			::chmod( corruptPath, 0600 );
			//KW check the error code and the result?
			
			CFRelease(xmlData);
			xmlData = nil;
		}
	}
	else //existing file is unreadable
	{
		DBGLOG( kLogPlugin, "LDAP XML config file is unreadable" );
		bCorruptedFile = true;
		//siResult = eDSPlugInConfigFileError; // not an error since we will attempt to recover
	}
        
	if (bCorruptedFile)
	{
		//create a new dictionary for the file
		configDict = CFDictionaryCreateMutable( kCFAllocatorDefault,
												0,
												&kCFTypeDictionaryKeyCallBacks,
												&kCFTypeDictionaryValueCallBacks );
		
		CFDictionarySetValue( configDict, CFSTR( kXMLLDAPVersionKey ), CFSTR( "DSLDAPv3PlugIn Version 1.5" ) );
		
		DBGLOG( kLogPlugin, "Writing a new LDAP XML config file" );
		//convert the dict into a XML blob
		xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, configDict);
		
		//assume that the XML blob is good since we created it here
		fXMLData = xmlData;

		//write the XML to the config file
		siResult = CFURLWriteDataAndPropertiesToResource( configFileURL,
															xmlData,
															NULL,
															&errorCode);
		if (filenameString != nil)
		{
			::chmod( filenameString, 0600 );
		}
		//KW check the error code and the result?
		
		CFRelease(configDict);
		configDict = nil;
	}
	
	// if we have a config now, let's convert let's look to see if there is a sV2Config to convert
	if( fXMLData )
	{
		// if we converted....
		if( ConvertLDAPv2Config() )
		{
			//write the XML to the config file
			siResult = CFURLWriteDataAndPropertiesToResource( configFileURL,
													 fXMLData,
													 NULL,
													 &errorCode);
			if (filenameString != nil)
			{
				::chmod( filenameString, 0600 );
			}
		}
	}
		
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
    
    return( siResult );

} // ReadXMLConfig

// ---------------------------------------------------------------------------
//	* WriteXMLConfig
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Configs::WriteXMLConfig ( void )
{
	sInt32					siResult			= eDSNoErr;
	CFURLRef				configFileURL		= NULL;
	bool					bWroteFile			= false;
	char					string[ PATH_MAX ];
	struct stat				statResult;
	sInt32					errorCode			= 0;
	char				   *filenameString			= "/Library/Preferences/DirectoryService/DSLDAPv3PlugInConfig.plist";

	//Config data is written to a XML file
	//Steps in the process:
	//1- see if the file exists
	//2- if it exists then overwrite it
	//3- rename existing file and save it while creating a new file
	//4- if file doesn't exist then create a new default file - make sure directories exist/if not create them
	//make sure file permissions are root only

	// Get the local library search path -- only expect a single one
	// count down here if more that the Local directory is specified
	// ie. in Local ( or user's home directory ).
	// for now reality is that there is NO countdown
	while (!bWroteFile)
	{
		//step 1- see if the file exists
		//if not then make sure the directories exist or create them
		//then write the file
		siResult = ::stat( filenameString, &statResult );
		
		//if file does not exist, let's make sure the directories are there
		if (siResult != eDSNoErr)
		{
			CreatePrefDirectory();
		} // file does not exist so checking directory path to enable write of a new file

		CFStringRef sPath = CFStringCreateWithCString( kCFAllocatorDefault, filenameString, kCFStringEncodingUTF8 );
		
		configFileURL = ::CFURLCreateWithFileSystemPath( kCFAllocatorDefault, sPath, kCFURLPOSIXPathStyle, false );
		CFRelease( sPath );
		
		//now write the updated file
		if (fXMLData != nil)
		{
			//write the XML to the config file
			bWroteFile = CFURLWriteDataAndPropertiesToResource( configFileURL,
															fXMLData,
															NULL,
															&errorCode);
			::chmod( filenameString, 0600 );
			//check the error code and the result?
		}

		CFRelease(configFileURL); // seems okay to dealloc since Create used and done with it now
		configFileURL = nil;
	} // while (( iPath-- ) && (!bWroteFile))

	if (bWroteFile)
	{
		DBGLOG( kLogPlugin, "Have written the LDAP XML config file:" );
		DBGLOG1( kLogPlugin, "%s", string );
		siResult = eDSNoErr;
	}
	else
	{
		DBGLOG( kLogPlugin, "LDAP XML config file has NOT been written" );
		DBGLOG( kLogPlugin, "Update to LDAP Config File Failed" );
		siResult = eDSPlugInConfigFileError;
	}
		
	return( siResult );

} // WriteXMLConfig

// ---------------------------------------------------------------------------
//	* AddToConfig
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Configs::AddToConfig ( CFDataRef inXMLData )
{
	sInt32					siResult			= eDSCorruptBuffer;
    CFStringRef				errorString			= NULL;
    CFPropertyListRef		configPropertyList	= NULL;
	CFMutableDictionaryRef	configDict			= NULL;
	char				   *tmpBuff				= nil;
	CFIndex					cfBuffSize			= 1024;
	CFStringRef				cfStringRef			= NULL;
	CFBooleanRef			cfBool				= false;
	unsigned char			cfNumBool			= false;
	CFNumberRef				cfNumber			= 0;
	char				   *server				= nil;
    char				   *mapSearchBase 		= nil;
	int						portNumber			= 389;
	bool					bIsSSL				= false;
	bool					bServerMappings		= false;
	bool					bUseConfig			= false;
	bool					bReferrals			= true; // default to true
	int						opencloseTO			= 15;
	int						idleTO				= 2;
	int						delayRebindTry		= 120;
	int						searchTO			= 120;
    CFPropertyListRef		xConfigPropertyList	= NULL;
	CFMutableDictionaryRef	xConfigDict			= NULL;
	CFMutableArrayRef		cfMutableArrayRef	= NULL;
	CFDataRef				xmlBlob				= NULL;

	if (inXMLData != nil)
	{
		// extract the config dictionary from the XML data.
		configPropertyList = CFPropertyListCreateFromXMLData( kCFAllocatorDefault,
					inXMLData,
					kCFPropertyListMutableContainers, //could use kCFPropertyListImmutable
					&errorString);
		
		if (configPropertyList != nil )
		{
			//make the propertylist a dict
			if ( CFDictionaryGetTypeID() == CFGetTypeID( configPropertyList ) )
			{
				configDict = (CFMutableDictionaryRef) configPropertyList;
			}
			
			if (configDict != nil)
			{
				//make sure the make default flag is set in the config
				CFDictionarySetValue(configDict, CFSTR( kXMLMakeDefLDAPFlagKey ), kCFBooleanTrue);

				//let's first go ahead and add this data to the actual config XML tied to the config file
				if (fXMLData != nil)
				{
					// extract the config dictionary from the XML data.
					xConfigPropertyList = CFPropertyListCreateFromXMLData( kCFAllocatorDefault,
								fXMLData,
								kCFPropertyListMutableContainers, //could use kCFPropertyListImmutable
								&errorString);
					
					if (xConfigPropertyList != nil )
					{
						//make the propertylist a dict
						if ( CFDictionaryGetTypeID() == CFGetTypeID( xConfigPropertyList ) )
						{
							xConfigDict = (CFMutableDictionaryRef) xConfigPropertyList;
							if (xConfigDict != nil)
							{
								if ( CFDictionaryContainsKey( xConfigDict, CFSTR( kXMLConfigArrayKey ) ) )
								{
									cfMutableArrayRef = (CFMutableArrayRef)CFDictionaryGetValue( xConfigDict, CFSTR( kXMLConfigArrayKey ) );
									//simply add the new to the old here
									CFArrayAppendValue(cfMutableArrayRef, configDict);
								}
								else //we need to make the first entry here
								{
									cfMutableArrayRef = CFArrayCreateMutable( kCFAllocatorDefault, NULL, &kCFTypeArrayCallBacks);
									CFArrayAppendValue(cfMutableArrayRef, configDict);
									CFDictionarySetValue( xConfigDict, CFSTR( kXMLConfigArrayKey ), cfMutableArrayRef );
									CFRelease(cfMutableArrayRef);
								}
								
								//convert the dict into a XML blob
								xmlBlob = CFPropertyListCreateXMLData( kCFAllocatorDefault, xConfigDict);
								//replace the XML data blob
								SetXMLConfig(xmlBlob);
								//release this reference here
								CFRelease(xmlBlob);
								xmlBlob = nil;
							}
						}
						CFRelease(xConfigPropertyList);
					}
				}
				
				if ( CFDictionaryContainsKey( configDict, CFSTR( kXMLEnableUseFlagKey ) ) )
				{
					//assume that the extracted strings will be significantly less than 1024 characters
					tmpBuff = (char *)::calloc(1, 1024);
					
					cfBool = (CFBooleanRef)CFDictionaryGetValue( configDict, CFSTR( kXMLEnableUseFlagKey ) );
					if (cfBool != nil)
					{
						bUseConfig = CFBooleanGetValue( cfBool );
						//CFRelease( cfBool ); // no since pointer only from Get
					}
					//continue if this configuration was enabled by the user
					if ( bUseConfig )
					{
						//Enable Use flag is NOT provided to the configTable
						//retrieve all the others for the configTable
						
						if ( CFDictionaryContainsKey( configDict, CFSTR( kXMLServerKey ) ) )
						{
							cfStringRef = (CFStringRef)CFDictionaryGetValue( configDict, CFSTR( kXMLServerKey ) );
							if ( cfStringRef != nil )
							{
								if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
								{
									::memset(tmpBuff,0,1024);
									if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
									{
										server = (char *)::calloc(1+strlen(tmpBuff),1);
										::strcpy(server, tmpBuff);
									}
								}
								//CFRelease(cfStringRef); // no since pointer only from Get
							}
						}
			
						if ( CFDictionaryContainsKey( configDict, CFSTR( kXMLIsSSLFlagKey ) ) )
						{
							cfBool= (CFBooleanRef)CFDictionaryGetValue( configDict, CFSTR( kXMLIsSSLFlagKey ) );
							if (cfBool != nil)
							{
								bIsSSL = CFBooleanGetValue( cfBool );
								//CFRelease( cfBool ); // no since pointer only from Get
								if (bIsSSL)
								{
									portNumber = 636; // default for SSL ie. if no port given below
								}
							}
						}
			
						if ( CFDictionaryContainsKey( configDict, CFSTR( kXMLOpenCloseTimeoutSecsKey ) ) )
						{
							cfNumber = (CFNumberRef)CFDictionaryGetValue( configDict, CFSTR( kXMLOpenCloseTimeoutSecsKey ) );
							if ( cfNumber != nil )
							{
								cfNumBool = CFNumberGetValue(cfNumber, kCFNumberIntType, &opencloseTO);
								//CFRelease(cfNumber); // no since pointer only from Get
							}
						}
			
						if ( CFDictionaryContainsKey( configDict, CFSTR( kXMLIdleTimeoutMinsKey ) ) )
						{
							cfNumber = (CFNumberRef)CFDictionaryGetValue( configDict, CFSTR( kXMLIdleTimeoutMinsKey ) );
							if ( cfNumber != nil )
							{
								cfNumBool = CFNumberGetValue(cfNumber, kCFNumberIntType, &idleTO);
								//CFRelease(cfNumber); // no since pointer only from Get
							}
						}
			
						if ( CFDictionaryContainsKey( configDict, CFSTR( kXMLDelayedRebindTrySecsKey ) ) )
						{
							cfNumber = (CFNumberRef)CFDictionaryGetValue( configDict, CFSTR( kXMLDelayedRebindTrySecsKey ) );
							if ( cfNumber != nil )
							{
								cfNumBool = CFNumberGetValue(cfNumber, kCFNumberIntType, &delayRebindTry);
								//CFRelease(cfNumber); // no since pointer only from Get
							}
						}
			
						if ( CFDictionaryContainsKey( configDict, CFSTR( kXMLSearchTimeoutSecsKey ) ) )
						{
							cfNumber = (CFNumberRef)CFDictionaryGetValue( configDict, CFSTR( kXMLSearchTimeoutSecsKey ) );
							if ( cfNumber != nil )
							{
								cfNumBool = CFNumberGetValue(cfNumber, kCFNumberIntType, &searchTO);
								//CFRelease(cfNumber); // no since pointer only from Get
							}
						}

						if ( CFDictionaryContainsKey( configDict, CFSTR( kXMLPortNumberKey ) ) )
						{
							cfNumber = (CFNumberRef)CFDictionaryGetValue( configDict, CFSTR( kXMLPortNumberKey ) );
							if ( cfNumber != nil )
							{
								cfNumBool = CFNumberGetValue(cfNumber, kCFNumberIntType, &portNumber);
								//CFRelease(cfNumber); // no since pointer only from Get
							}
						}
			
						if ( CFDictionaryContainsKey( configDict, CFSTR( kXMLServerMappingsFlagKey ) ) )
						{
							cfBool = (CFBooleanRef)CFDictionaryGetValue( configDict, CFSTR( kXMLServerMappingsFlagKey ) );
							if (cfBool != nil)
							{
								bServerMappings = CFBooleanGetValue( cfBool );
								//CFRelease( cfBool ); // no since pointer only from Get
							}
						}
			
						if ( CFDictionaryContainsKey( configDict, CFSTR( kXMLMapSearchBase ) ) )
						{
							cfStringRef = (CFStringRef)CFDictionaryGetValue( configDict, CFSTR( kXMLMapSearchBase ) );
							if ( cfStringRef != nil )
							{
								if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
								{
									::memset(tmpBuff,0,1024);
									if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
									{
										mapSearchBase = (char *)::calloc(1+strlen(tmpBuff),1);
										::strcpy(mapSearchBase, tmpBuff);
									}
								}
								//CFRelease(cfStringRef); // no since pointer only from Get
							}
						}
						
						if( cfBool = (CFBooleanRef) CFDictionaryGetValue( configDict, CFSTR(kXMLReferralFlagKey) ) )
						{
							if( CFGetTypeID(cfBool) == CFBooleanGetTypeID() )
							{
								bReferrals = CFBooleanGetValue( cfBool );
							}
						}
							
						siResult = MakeServerBasedMappingsLDAPConfig( server, mapSearchBase, opencloseTO, idleTO, delayRebindTry, searchTO, portNumber, bIsSSL, true, bReferrals );
						
						if ( server != nil ) 
						{
							free( server );
							server = nil;
						}
						if ( mapSearchBase != nil ) 
						{
							free( mapSearchBase );
							mapSearchBase = nil;
						}
						
					}// if ( bUseConfig )
					
					//free up the tmpBuff
					delete( tmpBuff );
			
				}// if kXMLEnableUseFlagKey set
			}
			CFRelease(configPropertyList);
		}
	}
	
	return(siResult);
}


// ---------------------------------------------------------------------------
//	* SetXMLConfig
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Configs::SetXMLConfig ( CFDataRef xmlData )
{
	CFDataRef currentXMLData = fXMLData;
	
	fXMLData = xmlData;
	if (VerifyXML())
	{
		if (currentXMLData != nil)
		{
			CFRelease(currentXMLData);
			currentXMLData = nil;
		}
		CFRetain(fXMLData);
		return eDSNoErr;
	}
	else
	{
		// go back to what we had
		fXMLData = currentXMLData;
		
		return eDSInvalidPlugInConfigData;
	}
}


// ---------------------------------------------------------------------------
//	* GetXMLConfig
// ---------------------------------------------------------------------------

CFDataRef CLDAPv3Configs::CopyXMLConfig ( void )
{
	CFDataRef				combinedConfigDataRef	= NULL;
	CFMutableDictionaryRef	configDict				= NULL;
	CFStringRef				errorString				= NULL;
	CFArrayRef				configArray				= NULL;
	CFIndex					configArrayCount		= 0;
	CFMutableArrayRef		dhcpConfigArray			= NULL;
    uInt32					index					= 0;
    sLDAPConfigData*		pConfig					= nil;

	// Object is to loop over our pConfigTable and see if we have any DHCP entries.
	// If we do, we want to incorporate them into the user defined config data.
	// If not, we will just retain fXMLData and return that.

	for (index=0; index<fConfigTableLen; index++)
	{
		pConfig = (sLDAPConfigData *)pConfigTable->GetItemData( index );
		if (pConfig != nil)
		{
			if (pConfig->bUseAsDefaultLDAP)		// is the current configuration possibly from DHCP? (Need to check against fXMLData table too)
			{
				bool			isCurrentConfInXMLData = false;
				CFStringRef		curConfigServerName = CFStringCreateWithCString( NULL, pConfig->fServerName, kCFStringEncodingUTF8 );
				
				if ( configDict == NULL )
				{
					configDict = (CFMutableDictionaryRef)CFPropertyListCreateFromXMLData(	kCFAllocatorDefault,
																							fXMLData,
																							kCFPropertyListMutableContainers,	// we want this mutable so we can add DHCP services
																							&errorString);
					if ( configDict == NULL )
					{
						char	errBuf[1024];
						CFStringGetCString( errorString, errBuf, sizeof(errBuf), kCFStringEncodingUTF8 );
						syslog(LOG_ERR,"DSLDAPv3PlugIn: [%s] LDAP server config could not be read.", errBuf);
						
						CFRelease( curConfigServerName );
						curConfigServerName = NULL;						
						break;
					}
					
					if ( CFDictionaryGetTypeID() != CFGetTypeID( configDict ) )
					{
						syslog(LOG_ERR,"DSLDAPv3PlugIn: LDAP server config could not be read as it was not in the correct format!");
						
						CFRelease( configDict );
						configDict = NULL;
						CFRelease( curConfigServerName );
						curConfigServerName = NULL;						
						break;
					}
					
					configArray = (CFArrayRef)CFDictionaryGetValue( configDict, CFSTR(kXMLConfigArrayKey) );
					
					if ( configArray != NULL )					
						configArrayCount = CFArrayGetCount( configArray );
				}
								
				for ( CFIndex i=0; i<configArrayCount; i++ )
				{
					CFStringRef		indexedServerName = (CFStringRef)CFDictionaryGetValue( (CFDictionaryRef)CFArrayGetValueAtIndex( configArray, i ), CFSTR(kXMLServerKey) );
					
					// KA we should revisit this when we support having multiple configs per server.  At the moment we only publish one node
					// per server address configured.
					if ( CFStringCompare( curConfigServerName, indexedServerName, 0 ) == kCFCompareEqualTo )
					{
						isCurrentConfInXMLData = true;
						break;
					}
				}
				
				if ( !isCurrentConfInXMLData )
				{
					// we have found a configuration that was generated via DHCP, we need to add this to dhcpConfigArray
					if ( dhcpConfigArray == NULL )
						dhcpConfigArray = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
					
					CFMutableDictionaryRef		curConfigDict = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
					CFNumberRef					curConfigPort = CFNumberCreate( NULL, kCFNumberIntType, &(pConfig->fServerPort) );
					CFStringRef					curConfigUIName = CFStringCreateWithCString( NULL, pConfig->fName, kCFStringEncodingUTF8  );
					CFNumberRef					curConfigOpenCloseTimeOut = CFNumberCreate( NULL, kCFNumberIntType, &(pConfig->fOpenCloseTimeout) );
					CFNumberRef					curConfigSearchTimeOut = CFNumberCreate( NULL, kCFNumberIntType, &(pConfig->fSearchTimeout) );
					
					CFDictionaryAddValue( curConfigDict, CFSTR(kXMLUserDefinedNameKey), curConfigUIName );
					CFDictionaryAddValue( curConfigDict, CFSTR(kXMLServerKey), curConfigServerName );
					
					if ( pConfig->fRecordTypeMapCFArray != NULL )
						CFDictionaryAddValue( curConfigDict, CFSTR(kXMLRecordTypeMapArrayKey), pConfig->fRecordTypeMapCFArray );
					
					if ( pConfig->fAttrTypeMapCFArray != NULL )
						CFDictionaryAddValue( curConfigDict, CFSTR(kXMLAttrTypeMapArrayKey), pConfig->fAttrTypeMapCFArray );
						
					if ( pConfig->fReplicaHostnames != NULL)
						CFDictionaryAddValue( curConfigDict, CFSTR(kXMLReplicaHostnameListArrayKey), pConfig->fReplicaHostnames );
						
					if ( pConfig->fWriteableHostnames != NULL)
						CFDictionaryAddValue( curConfigDict, CFSTR(kXMLWriteableHostnameListArrayKey), pConfig->fWriteableHostnames );
						
					CFDictionaryAddValue( curConfigDict, CFSTR(kXMLPortNumberKey), curConfigPort );
					CFDictionaryAddValue( curConfigDict, CFSTR(kXMLOpenCloseTimeoutSecsKey), curConfigOpenCloseTimeOut );
					CFDictionaryAddValue( curConfigDict, CFSTR(kXMLSearchTimeoutSecsKey), curConfigSearchTimeOut );
					CFDictionaryAddValue( curConfigDict, CFSTR(kXMLIsSSLFlagKey), (pConfig->bIsSSL)?kCFBooleanTrue:kCFBooleanFalse );
					CFDictionaryAddValue( curConfigDict, CFSTR(kXMLMakeDefLDAPFlagKey), kCFBooleanTrue );
					CFDictionaryAddValue( curConfigDict, CFSTR(kXMLEnableUseFlagKey), kCFBooleanTrue );
					CFDictionaryAddValue( curConfigDict, CFSTR(kXMLServerMappingsFlagKey), (pConfig->bServerMappings)?kCFBooleanTrue:kCFBooleanFalse );
					CFDictionaryAddValue( curConfigDict, CFSTR(kXMLReferralFlagKey), (pConfig->bReferrals ? kCFBooleanTrue : kCFBooleanFalse) );
					
					CFArrayAppendValue( dhcpConfigArray, curConfigDict );
					
					CFRelease( curConfigSearchTimeOut );
					CFRelease( curConfigOpenCloseTimeOut );
					CFRelease( curConfigUIName );
					CFRelease( curConfigPort );
					CFRelease( curConfigDict );
				}
				
				CFRelease( curConfigServerName );
				curConfigServerName = NULL;
			}
		}
	}
	
	if ( dhcpConfigArray == NULL )
	{
		combinedConfigDataRef = fXMLData;
		CFRetain( combinedConfigDataRef );
	}
	else
	{
		CFDictionaryAddValue( configDict, CFSTR(kXMLDHCPConfigArrayKey), dhcpConfigArray );
		
		combinedConfigDataRef = CFPropertyListCreateXMLData( NULL, configDict );
	}
	
	if ( dhcpConfigArray )
		CFRelease( dhcpConfigArray );
	
	if ( configDict )
		CFRelease( configDict );
		
	return combinedConfigDataRef;
}


// ---------------------------------------------------------------------------
//	* VerifyXML
// ---------------------------------------------------------------------------

bool CLDAPv3Configs::VerifyXML ( void )
{
    bool			verified		= false;
    CFStringRef			errorString;
    CFPropertyListRef		configPropertyList;
//    char				   *configVersion		= nil;
//KW need to add in check on the version string

    if (fXMLData != nil)
    {
        // extract the config dictionary from the XML data.
        configPropertyList = CFPropertyListCreateFromXMLData( kCFAllocatorDefault,
                                fXMLData,
                                kCFPropertyListImmutable, 
                               &errorString);
        if (configPropertyList != nil )
        {
            //make the propertylist a dict
            if ( CFDictionaryGetTypeID() == CFGetTypeID( configPropertyList ) )
            {
                    verified = true;
            }
            CFRelease(configPropertyList);
			configPropertyList = nil;
        }
    }
    
    return( verified);
    
} // VerifyXML

// --------------------------------------------------------------------------------
//	* UpdateLDAPConfigWithServerMappings
// --------------------------------------------------------------------------------

sInt32 CLDAPv3Configs::UpdateLDAPConfigWithServerMappings ( char *inServer, char *inMapSearchBase, int inPortNumber, bool inIsSSL, bool inMakeDefLDAP, bool inReferrals, LDAP *inServerHost)
{
	sInt32		siResult	= eDSNoErr;
	CFDataRef	ourXMLData	= nil;
	CFDataRef	newXMLData	= nil;
	
	ourXMLData = RetrieveServerMappings( inServer, inMapSearchBase, inPortNumber, inIsSSL, inReferrals, inServerHost );
	if (ourXMLData != nil)
	{
		//here we will make sure that the server location and port/SSL in the XML data is the same as given above
		//we also make sure that the MakeDefLDAPFlag is set so that this gets added to the Automatic search policy
		newXMLData = VerifyAndUpdateServerLocation(inServer, inPortNumber, inIsSSL, inMakeDefLDAP, ourXMLData); //don't check return
		
		if (newXMLData != nil)
		{
			CFRelease(ourXMLData);
			ourXMLData = newXMLData;
			newXMLData = nil;
		}
		siResult = AddLDAPServer(ourXMLData);
		CFRelease(ourXMLData);
		
		if (siResult != eDSNoErr)
		{
			syslog(LOG_INFO,"DSLDAPv3PlugIn: [%s] LDAP server config not updated with server mappings due to server mappings format error.", inServer);
		}
	}
	else
	{
		syslog(LOG_INFO,"DSLDAPv3PlugIn: [%s] LDAP server config not updated with server mappings due to server mappings error.", inServer);
		siResult = eDSCannotAccessSession;
	}
	
	return(siResult);

} // UpdateLDAPConfigWithServerMappings


// ---------------------------------------------------------------------------
//	* ConfigLDAPServers
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Configs::ConfigLDAPServers ( void )
{
	sInt32					siResult			= eDSNoErr;
	CFStringRef				errorString			= NULL;
	CFPropertyListRef		configPropertyList	= NULL;
	CFMutableDictionaryRef	configDict			= NULL;
	CFArrayRef				cfArrayRef			= NULL;
	CFIndex					cfConfigCount		= 0;
	CFDataRef				xmlData				= NULL;
	char				   *configVersion		= nil;

	try
	{	
		if (fXMLData != nil)
		{
			// extract the config dictionary from the XML data.
			configPropertyList = CFPropertyListCreateFromXMLData( kCFAllocatorDefault,
						fXMLData,
						kCFPropertyListMutableContainers, //could also use kCFPropertyListImmutable, kCFPropertyListMutableContainers
					   &errorString);
			
			if (configPropertyList != nil )
			{
				//make the propertylist a dict
				if ( CFDictionaryGetTypeID() == CFGetTypeID( configPropertyList ) )
				{
					configDict = (CFMutableDictionaryRef) configPropertyList;
				}
				
				if (configDict != nil)
				{
					//get version, defaults mappings and array of LDAP server configs
					
					//config file version
					configVersion = GetVersion(configDict);
					if ( configVersion == nil ) throw( (sInt32)eDSVersionMismatch ); //KW need eDSPlugInConfigFileError
					if (configVersion != nil)
					{
					
						DBGLOG( kLogPlugin, "Have successfully read the LDAP XML config file" );

                        //if config file is up to date with latest default mappings then use them
                        if (strcmp(configVersion,"DSLDAPv3PlugIn Version 1.5") == 0)
                        {
                        }
                        else
                        {
                            // update the version
                            // replace the default mappings in the configDict with the generated standard ones
                            // write the config file out to pick up the generated default mappings
                            
                            //remove old and add proper version
                            CFDictionaryRemoveValue( configDict, CFSTR( kXMLLDAPVersionKey ) );
                            CFDictionarySetValue( configDict, CFSTR( kXMLLDAPVersionKey ), CFSTR( "DSLDAPv3PlugIn Version 1.5" ) );

                            //convert the dict into a XML blob
                            xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, configDict);

                            //replace the XML data blob
                            siResult = SetXMLConfig(xmlData);
							
							//release this reference here
							CFRelease(xmlData);
							xmlData = nil;
							                            
                            //write the file out to save the change
                            if (siResult == eDSNoErr)
                            {
                                WriteXMLConfig();
                            }
                        }
						//array of LDAP server configs
						cfArrayRef = nil;
						cfArrayRef = GetConfigArray(configDict);
						if (cfArrayRef != nil)
						{
							//now we can retrieve each config
							cfConfigCount = ::CFArrayGetCount( cfArrayRef );
							//if (cfConfigCount == 0)
							//assume that this file has no Servers in it
							//and simply proceed forward ie. no Node will get registered from data in this file
							
							//loop through the configs
							//use iConfigIndex for the access to the cfArrayRef
							//use fConfigTableLen for the index to add to the table since we add at the end
							for (sInt32 iConfigIndex = 0; iConfigIndex < cfConfigCount; iConfigIndex++)
							{
								CFDictionaryRef		serverConfigDict	= nil;
								//CFDictionaryRef		suppliedServerDict	= nil;
								serverConfigDict = (CFDictionaryRef)::CFArrayGetValueAtIndex( cfArrayRef, iConfigIndex );
								if ( serverConfigDict != nil )
								{
/*
									//here we check the serverConfigDict if it indicates server mappings
									suppliedServerDict = CheckForServerMappings(serverConfigDict);
									if (suppliedServerDict != nil)
									{
										siResult = MakeLDAPConfig(suppliedServerDict, fConfigTableLen);
										CFRelease(suppliedServerDict);
										suppliedServerDict = nil;
									}
									else
*/
									{
										siResult = MakeLDAPConfig(serverConfigDict, fConfigTableLen);
									}
								}
							} // loop over configs
							
							//CFRelease( cfArrayRef ); // no since pointer only from Get
							
						} // if (cfArrayRef != nil) ie. an array of LDAP configs exists
						delete(configVersion);
						
					}//if (configVersion != nil)
					
					// don't release the configDict since it is the cast configPropertyList
					
				}//if (configDict != nil)
				
				CFRelease(configPropertyList); // built from Create on XML data so okay to dealloc here
				configPropertyList = nil;
	
			}//if (configPropertyList != nil )
		} // fXMLData != nil
		
	} // try
	catch ( sInt32 err )
	{
		siResult = err;
		if (configPropertyList != nil)
		{
			CFRelease(configPropertyList); // built from Create on XML data so okay to dealloc here
			configPropertyList = nil;
		}
	}
	return( siResult );

} // ConfigLDAPServers


//------------------------------------------------------------------------------------
//	* RetrieveServerMappings
//------------------------------------------------------------------------------------

CFDataRef CLDAPv3Configs::RetrieveServerMappings ( char *inServer, char *inMapSearchBase, int inPortNumber, bool inIsSSL, bool inReferrals, LDAP *inServerHost )
{
	sInt32				siResult		= eDSNoErr;
	bool				bResultFound	= false;
    int					ldapMsgId		= -1;
	LDAPMessage		   *result			= nil;
	int					ldapReturnCode	= 0;
	char			   *attrs[2]		= {"description",NULL};
	BerElement		   *ber;
	struct berval	  **bValues;
	char			   *pAttr			= nil;
	LDAP			   *serverHost		= nil;
	CFDataRef			ourMappings		= nil;
	bool				bCleanHost		= false;

	if (inServerHost == nil)
	{
		if ( (inServer != nil) && (inPortNumber != 0) )
		{
			serverHost = ldap_init( inServer, inPortNumber );
			bCleanHost = true;
	
		} // if ( (inServer != nil) && (inPortNumber != 0) )
	}
	else
	{
		serverHost = inServerHost;
	}
	
		if (serverHost != nil)
		{
			if (inIsSSL)
			{
				int ldapOptVal = LDAP_OPT_X_TLS_HARD;
				ldap_set_option(serverHost, LDAP_OPT_X_TLS, &ldapOptVal);
			}
			
			ldap_set_option( serverHost, LDAP_OPT_REFERRALS, (inReferrals ? LDAP_OPT_ON : LDAP_OPT_OFF) );
			
			if (inMapSearchBase == nil)
			{
				ldapMsgId = ldap_search( serverHost, "", LDAP_SCOPE_SUBTREE, "(&(objectclass=*)(ou=macosxodconfig))", attrs, 0);
			}
			else
			{
				ldapMsgId = ldap_search( serverHost, inMapSearchBase, LDAP_SCOPE_SUBTREE, "(&(objectclass=*)(ou=macosxodconfig))", attrs, 0);
			}
			
			//here is the call to the LDAP server asynchronously which requires
			// host handle, search base, search scope(LDAP_SCOPE_SUBTREE for all), search filter,
			// attribute list (NULL for all), return attrs values flag
			// Note: asynchronous call is made so that a MsgId can be used for future calls
			// This returns us the message ID which is used to query the server for the results
			//TODO KW do we want a retry here?
			if ( ldapMsgId == -1 )
			{
				bResultFound = false;
			}
			else
			{
				bResultFound = true;
				//retrieve the actual LDAP record data for use internally
				//useful only from the read-only perspective
				struct	timeval	tv;
				tv.tv_sec	= 60;
				tv.tv_usec	= 0;
				ldapReturnCode = ldap_result(serverHost, ldapMsgId, 0, &tv, &result);
			}
			
			if ( ( bResultFound ) && ( ldapReturnCode == LDAP_RES_SEARCH_ENTRY ) )
			{
				//get the XML data here
				//parse the attributes in the result - should only be one ie. macosxodconfig
				for (	pAttr = ldap_first_attribute (serverHost, result, &ber );
							pAttr != NULL; pAttr = ldap_next_attribute(serverHost, result, ber ) )
				{
					if (( bValues = ldap_get_values_len (serverHost, result, pAttr )) != NULL)
					{					
						// should be only one value of the attribute
						if ( bValues[0] != NULL )
						{
							ourMappings = CFDataCreate(NULL,(UInt8 *)(bValues[0]->bv_val), bValues[0]->bv_len);
						}
						
						ldap_value_free_len(bValues);
					} // if bValues = ldap_get_values_len ...
												
					if (pAttr != nil)
					{
						ldap_memfree( pAttr );
					}
						
				} // for ( loop over ldap_next_attribute )
					
				if (ber != nil)
				{
					ber_free( ber, 0 );
				}
					
				ldap_msgfree( result );
				result = nil;

				siResult = eDSNoErr;
			}
			else if (ldapReturnCode == LDAP_TIMEOUT)
			{
				siResult = eDSServerTimeout;
				syslog(LOG_INFO,"DSLDAPv3PlugIn: Retrieval of Server Mappings for [%s] LDAP server has timed out.", inServer);
				if ( result != nil )
				{
					ldap_msgfree( result );
					result = nil;
				}
			}
			else
			{
				siResult = eDSRecordNotFound;
				syslog(LOG_INFO,"DSLDAPv3PlugIn: Server Mappings for [%s] LDAP server not found.", inServer);
				if ( result != nil )
				{
					ldap_msgfree( result );
					result = nil;
				}
			}
			DSSearchCleanUp(serverHost, ldapMsgId);
			if (bCleanHost)
			{
				ldap_unbind( serverHost );
			}
		} // if (serverHost != nil)

	return( ourMappings );

} // RetrieveServerMappings


//------------------------------------------------------------------------------------
//	* WriteServerMappings
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Configs::WriteServerMappings ( char* userName, char* password, CFDataRef inMappings )
{
	sInt32					siResult			= eDSNoErr;
	LDAP				   *serverHost			= nil;
	CFStringRef				errorString			= NULL;
	CFPropertyListRef		configPropertyList	= nil;
	CFDictionaryRef			serverConfigDict	= nil;
	char				   *server				= nil;
	int						portNumber			= 389;
	int						openCloseTO			= kLDAPDefaultOpenCloseTimeoutInSeconds;
	char				   *tmpBuff				= nil;
	CFIndex					cfBuffSize			= 1024;
	CFStringRef				cfStringRef			= nil;
	CFBooleanRef			cfBool				= nil;
	CFNumberRef				cfNumber			= nil;
	bool					cfNumBool			= false;
	char				   *mapSearchBase		= nil;
	bool					bIsSSL				= false;
    int						ldapReturnCode 		= 0;
	int						version				= -1;
    int						bindMsgId			= 0;
    LDAPMessage			   *result				= nil;
	char				   *ldapDNString		= nil;
	uInt32					ldapDNLength		= 0;
	char				   *ourXMLBlob			= nil;
	char				   *ouvals[2];
	char				   *mapvals[2];
	char				   *ocvals[3];
	LDAPMod					oumod;
	LDAPMod					mapmod;
	LDAPMod					ocmod;
	LDAPMod				   *mods[4];
	
	try
	{	
		if (inMappings != nil)
		{
			// extract the config dictionary from the XML data.
			configPropertyList = CFPropertyListCreateFromXMLData(	kCFAllocatorDefault,
																	inMappings,
																	kCFPropertyListImmutable,
																   &errorString);
			
			if (configPropertyList != nil )
			{
				//make the propertylist a dict
				if ( CFDictionaryGetTypeID() == CFGetTypeID( configPropertyList ) )
				{
					serverConfigDict = (CFDictionaryRef) configPropertyList;
				}
				
				if (serverConfigDict != nil)
				{					
					//assume that the extracted strings will be significantly less than 1024 characters
					tmpBuff = (char *)::calloc(1, 1024);
					
					// retrieve all the relevant values (mapsearchbase, IsSSL)
					// to enable server mapping write
					//need to get the server name first
					if ( CFDictionaryContainsKey( serverConfigDict, CFSTR( kXMLServerKey ) ) )
					{
						cfStringRef = (CFStringRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLServerKey ) );
						if ( cfStringRef != nil )
						{
							if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
							{
								::memset(tmpBuff,0,1024);
								if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
								{
									server = (char *)::calloc(1+strlen(tmpBuff),1);
									::strcpy(server, tmpBuff);
								}
							}
							//CFRelease(cfStringRef); // no since pointer only from Get
						}
					}
					if ( CFDictionaryContainsKey( serverConfigDict, CFSTR( kXMLOpenCloseTimeoutSecsKey ) ) )
					{
						cfNumber = (CFNumberRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLOpenCloseTimeoutSecsKey ) );
						if ( cfNumber != nil )
						{
							cfNumBool = CFNumberGetValue(cfNumber, kCFNumberIntType, &openCloseTO);
							//CFRelease(cfNumber); // no since pointer only from Get
						}
					}
					if ( CFDictionaryContainsKey( serverConfigDict, CFSTR( kXMLIsSSLFlagKey ) ) )
					{
						cfBool= (CFBooleanRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLIsSSLFlagKey ) );
						if (cfBool != nil)
						{
							bIsSSL = CFBooleanGetValue( cfBool );
							//CFRelease( cfBool ); // no since pointer only from Get
							if (bIsSSL)
							{
								portNumber = 636;
							}
						}
					}
					if ( CFDictionaryContainsKey( serverConfigDict, CFSTR( kXMLPortNumberKey ) ) )
					{
						cfNumber = (CFNumberRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLPortNumberKey ) );
						if ( cfNumber != nil )
						{
							cfNumBool = CFNumberGetValue(cfNumber, kCFNumberIntType, &portNumber);
							//CFRelease(cfNumber); // no since pointer only from Get
						}
					}
					
					if ( CFDictionaryContainsKey( serverConfigDict, CFSTR( kXMLMapSearchBase ) ) )
					{
						cfStringRef = (CFStringRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLMapSearchBase ) );
						if ( cfStringRef != nil )
						{
							if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
							{
								::memset(tmpBuff,0,1024);
								if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
								{
									mapSearchBase = (char *)::calloc(1+strlen(tmpBuff),1);
									::strcpy(mapSearchBase, tmpBuff);
								}
							}
							//CFRelease(cfStringRef); // no since pointer only from Get
						}
					}
					
					//free up the tmpBuff
					free( tmpBuff );
					tmpBuff = nil;
					// don't release the serverConfigDict since it is the cast configPropertyList
				}//if (serverConfigDict != nil)
				
				CFRelease(configPropertyList); // built from Create on XML data so okay to dealloc here
				configPropertyList = nil;
			}//if (configPropertyList != nil )

			serverHost = ldap_init( server, portNumber );
			
			if ( serverHost == nil ) throw( (sInt32)eDSCannotAccessSession );
			if ( bIsSSL )
			{
				int ldapOptVal = LDAP_OPT_X_TLS_HARD;
				ldap_set_option(serverHost, LDAP_OPT_X_TLS, &ldapOptVal);
			}
			
			/* LDAPv3 only */
			version = LDAP_VERSION3;
			ldap_set_option( serverHost, LDAP_OPT_PROTOCOL_VERSION, &version );

			bindMsgId = ldap_bind( serverHost, userName, password, LDAP_AUTH_SIMPLE );
			
			if (openCloseTO == 0)
			{
				ldapReturnCode = ldap_result(serverHost, bindMsgId, 0, NULL, &result);
			}
			else
			{
				struct	timeval	tv;
				tv.tv_sec		= openCloseTO;
				tv.tv_usec		= 0;
				ldapReturnCode	= ldap_result(serverHost, bindMsgId, 0, &tv, &result);
			}

			if ( ldapReturnCode == -1 )
			{
				throw( (sInt32)eDSCannotAccessSession );
			}
			else if ( ldapReturnCode == 0 )
			{
				// timed out, let's forget it
				ldap_unbind( serverHost );
				serverHost = NULL;
				throw( (sInt32)eDSCannotAccessSession );
			}
			else if ( ldap_result2error(serverHost, result, 1) != LDAP_SUCCESS )
			{
				throw( (sInt32)eDSCannotAccessSession );
			}			

			if ( (serverHost != nil) && (mapSearchBase != nil) )
			{
				//we use "ou" for the DN always:
				//"ou = macosxodconfig, mapSearchBase"
				ldapDNLength = 21 + strlen(mapSearchBase);
				ldapDNString = (char *)calloc(1, ldapDNLength + 1);
				strcpy(ldapDNString,"ou = macosxodconfig, ");
				strcat(ldapDNString,mapSearchBase);
			
				//attempt to delete what is there if anything
				ldapReturnCode = ldap_delete_s( serverHost, ldapDNString);
				if ( ( ldapReturnCode == LDAP_INSUFFICIENT_ACCESS ) || ( ldapReturnCode == LDAP_INVALID_CREDENTIALS ) )
				{
					siResult = eDSPermissionError;
				}
				else if ( ldapReturnCode == LDAP_NO_SUCH_OBJECT )
				{
					siResult = eDSRecordNotFound;
				}
				else if ( ldapReturnCode != LDAP_SUCCESS )
				{
					siResult = eDSBogusServer;
				}
				
				if ( (siResult == eDSRecordNotFound) || (siResult == eDSNoErr) )
				{
					//make the XML blob a manageable char*
					CFRange	aRange;
					aRange.location = 0;
					aRange.length = CFDataGetLength(inMappings);
					ourXMLBlob = (char *) calloc(1, aRange.length + 1);
					CFDataGetBytes( inMappings, aRange, (UInt8*)ourXMLBlob );

					//now attempt to create the record here
					//if it already exists then simply modify the attribute
					ouvals[0]			= "macosxodconfig";
					ouvals[1]			= NULL;
					oumod.mod_op		= 0;
					oumod.mod_type		= "ou";
					oumod.mod_values	= ouvals;
					mapvals[0]			= ourXMLBlob;
					mapvals[1]			= NULL;
					mapmod.mod_op		= 0;
					mapmod.mod_type		= "description";
					mapmod.mod_values	= mapvals;
					ocvals[0]			= "top";
					ocvals[1]			= "organizationalUnit";
					ocvals[2]			= NULL;
					ocmod.mod_op		= 0;
					ocmod.mod_type		= "objectclass";
					ocmod.mod_values	= ocvals;
					mods[0]				= &oumod;
					mods[1]				= &mapmod;
					mods[2]				= &ocmod;
					mods[3]				= NULL;
					ldapReturnCode = 0;
					siResult = eDSNoErr;
					ldapReturnCode = ldap_add_s( serverHost, ldapDNString, mods);
					if ( ( ldapReturnCode == LDAP_INSUFFICIENT_ACCESS ) || ( ldapReturnCode == LDAP_INVALID_CREDENTIALS ) )
					{
						siResult = eDSPermissionError;
					}
					else if ( ldapReturnCode == LDAP_ALREADY_EXISTS )
					{
						siResult = eDSRecordAlreadyExists;
					}
					else if ( ldapReturnCode == LDAP_NO_SUCH_OBJECT )
					{
						siResult = eDSRecordNotFound;
					}
					else if ( ldapReturnCode != LDAP_SUCCESS )
					{
						siResult = eDSBogusServer;
					}
				} //if ( (siResult == eDSRecordNotFound) || (siResult == eDSNoErr) )
			} // if ( (serverHost != nil) && (mapSearchBase != nil) )
		} // inMappings != nil
		
	} // try
	catch ( sInt32 err )
	{
		siResult = err;
		if (configPropertyList != nil)
		{
			CFRelease(configPropertyList); // built from Create on XML data so okay to dealloc here
			configPropertyList = nil;
		}
	}

	if ( serverHost != nil )
	{
		ldap_unbind( serverHost );
		serverHost = nil;
	}

	if ( mapSearchBase != nil ) 
	{
		free( mapSearchBase );
		mapSearchBase = nil;
	}
			
	if ( ourXMLBlob != nil ) 
	{
		free( ourXMLBlob );
		ourXMLBlob = nil;
	}
			
	if ( ldapDNString != nil ) 
	{
		free( ldapDNString );
		ldapDNString = nil;
	}
			
	return( siResult );

} // WriteServerMappings


//------------------------------------------------------------------------------------
//	* ReadServerMappings
//------------------------------------------------------------------------------------

CFDataRef CLDAPv3Configs::ReadServerMappings ( LDAP *serverHost, CFDataRef inMappings )
{
	sInt32					siResult			= eDSNoErr;
	CFStringRef				errorString;
	CFPropertyListRef		configPropertyList	= nil;
	CFMutableDictionaryRef	serverConfigDict	= nil;
	char				   *configVersion		= nil;
	char				   *tmpBuff				= nil;
	CFIndex					cfBuffSize			= 1024;
	CFStringRef				cfStringRef			= nil;
	CFBooleanRef			cfBool				= false;
	char				   *mapSearchBase		= nil;
	bool					bIsSSL				= false;
	bool					bServerMappings		= false;
	bool					bUseConfig			= false;
	unsigned char			cfNumBool			= false;
	CFNumberRef				cfNumber			= 0;
	char				   *server				= nil;
	int						portNumber			= 389;
	CFDataRef				outMappings			= nil;
	
	//takes in the partial XML config blob and extracts the mappings out of the server to return the true XML config blob
	try
	{	
		if (inMappings != nil)
		{
			// extract the config dictionary from the XML data.
			configPropertyList = CFPropertyListCreateFromXMLData(	kCFAllocatorDefault,
																	inMappings,
																	kCFPropertyListMutableContainers,
																	//could also use kCFPropertyListMutableContainers
																   &errorString);
			
			if (configPropertyList != nil )
			{
				//make the propertylist a dict
				if ( CFDictionaryGetTypeID() == CFGetTypeID( configPropertyList ) )
				{
					serverConfigDict = (CFMutableDictionaryRef) configPropertyList; //don't need mutable really
				}
				
				if (serverConfigDict != nil)
				{					
					//config data version
					configVersion = GetVersion(serverConfigDict);
					//TODO KW check for correct version? not necessary really since backward compatible?
					if ( configVersion == nil ) throw( (sInt32)eDSVersionMismatch ); //KW need eDSPlugInConfigFileError
					if (configVersion != nil)
					{
                        if (strcmp(configVersion,"DSLDAPv3PlugIn Version 1.5") == 0)
                        {

							//get relevant parameters out of dict
							if ( CFDictionaryContainsKey( serverConfigDict, CFSTR( kXMLEnableUseFlagKey ) ) )
							{
								//assume that the extracted strings will be significantly less than 1024 characters
								tmpBuff = (char *)::calloc(1, 1024);
								
								cfBool = (CFBooleanRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLEnableUseFlagKey ) );
								if (cfBool != nil)
								{
									bUseConfig = CFBooleanGetValue( cfBool );
									//CFRelease( cfBool ); // no since pointer only from Get
								}
								//continue if this configuration was enabled by the user
								//no error condition returned if this configuration is not used due to the enable use flag
								if ( bUseConfig )
								{
						
									if ( CFDictionaryContainsKey( serverConfigDict, CFSTR( kXMLServerMappingsFlagKey ) ) )
									{
										cfBool = (CFBooleanRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLServerMappingsFlagKey ) );
										if (cfBool != nil)
										{
											bServerMappings = CFBooleanGetValue( cfBool );
											//CFRelease( cfBool ); // no since pointer only from Get
										}
									}
						
									if (bServerMappings)
									{
										// retrieve all the relevant values (server, portNumber, mapsearchbase, IsSSL)
										// to enable server mapping write
										
										if ( CFDictionaryContainsKey( serverConfigDict, CFSTR( kXMLServerKey ) ) )
										{
											cfStringRef = (CFStringRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLServerKey ) );
											if ( cfStringRef != nil )
											{
												if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
												{
													::memset(tmpBuff,0,1024);
													if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
													{
														server = (char *)::calloc(1+strlen(tmpBuff),1);
														::strcpy(server, tmpBuff);
													}
												}
												//CFRelease(cfStringRef); // no since pointer only from Get
											}
										}
	
										if ( CFDictionaryContainsKey( serverConfigDict, CFSTR( kXMLIsSSLFlagKey ) ) )
										{
											cfBool= (CFBooleanRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLIsSSLFlagKey ) );
											if (cfBool != nil)
											{
												bIsSSL = CFBooleanGetValue( cfBool );
												//CFRelease( cfBool ); // no since pointer only from Get
												if (bIsSSL)
												{
													portNumber = 636; // default for SSL ie. if no port given below
												}
											}
										}
						
										if ( CFDictionaryContainsKey( serverConfigDict, CFSTR( kXMLPortNumberKey ) ) )
										{
											cfNumber = (CFNumberRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLPortNumberKey ) );
											if ( cfNumber != nil )
											{
												cfNumBool = CFNumberGetValue(cfNumber, kCFNumberIntType, &portNumber);
												//CFRelease(cfNumber); // no since pointer only from Get
											}
										}

										if ( CFDictionaryContainsKey( serverConfigDict, CFSTR( kXMLMapSearchBase ) ) )
										{
											cfStringRef = (CFStringRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLMapSearchBase ) );
											if ( cfStringRef != nil )
											{
												if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
												{
													::memset(tmpBuff,0,1024);
													if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
													{
														mapSearchBase = (char *)::calloc(1+strlen(tmpBuff),1);
														::strcpy(mapSearchBase, tmpBuff);
													}
												}
												//CFRelease(cfStringRef); // no since pointer only from Get
											}
										}
									}
						
								}// if ( bUseConfig )
		
								//free up the tmpBuff
								free( tmpBuff );
								tmpBuff = nil;
						
							}// if kXMLEnableUseFlagKey set
                        }                        
						free( configVersion );
						configVersion = nil;
						
					}//if (configVersion != nil)
					
					// don't release the serverConfigDict since it is the cast configPropertyList
					
				}//if (serverConfigDict != nil)
				
				CFRelease(configPropertyList); // built from Create on XML data so okay to dealloc here
				configPropertyList = nil;
	
			}//if (configPropertyList != nil )
			
			outMappings = RetrieveServerMappings( server, mapSearchBase, portNumber, bIsSSL, true );

		} // inMappings != nil
		
	} // try
	catch ( sInt32 err )
	{
		siResult = err;
		if (configPropertyList != nil)
		{
			CFRelease(configPropertyList); // built from Create on XML data so okay to dealloc here
			configPropertyList = nil;
		}
	}

	if ( server != nil ) 
	{
		free( server );
		server = nil;
	}
			
	if ( mapSearchBase != nil ) 
	{
		free( mapSearchBase );
		mapSearchBase = nil;
	}
			
	return( outMappings );

} // ReadServerMappings


// ---------------------------------------------------------------------------
//	* VerifyAndUpdateServerLocation
// ---------------------------------------------------------------------------

CFDataRef CLDAPv3Configs::VerifyAndUpdateServerLocation( char *inServer, int inPortNumber, bool inIsSSL, bool inMakeDefLDAP, CFDataRef inXMLData )
{
	CFStringRef				errorString			= nil;
	CFPropertyListRef		configPropertyList	= nil;
	CFMutableDictionaryRef	serverConfigDict	= nil;
	char				   *configVersion		= nil;
	char				   *server				= nil;
	int						portNumber			= 389;
	bool					bIsSSL				= false;
	char				   *tmpBuff				= nil;
	CFStringRef				cfStringRef			= nil;
	bool					bUpdate				= false;
	CFBooleanRef			cfBool				= false;
	CFNumberRef				cfNumber			= 0;
	CFIndex					cfBuffSize			= 1024;
	unsigned char			cfNumBool			= false;
	CFDataRef				outXMLData			= nil;
	bool					bIsSrvrMappings		= false;
	bool					bIsDefLDAP			= false;

	if (inXMLData != nil)
	{
		// extract the config dictionary from the XML data.
		configPropertyList = CFPropertyListCreateFromXMLData(	kCFAllocatorDefault,
																inXMLData,
																kCFPropertyListMutableContainers,
																//could also use kCFPropertyListImmutable
																&errorString);
		
		if (configPropertyList != nil )
		{
			//make the propertylist a dict
			if ( CFDictionaryGetTypeID() == CFGetTypeID( configPropertyList ) )
			{
				serverConfigDict = (CFMutableDictionaryRef) configPropertyList; //don't need mutable really
			}
			
			if (serverConfigDict != nil)
			{
				//get version, and the specific LDAP server config
				
				//config data version
				configVersion = GetVersion(serverConfigDict);
				//bail out of checking in this routine
				if ( configVersion == nil )
				{
					CFRelease(configPropertyList); // built from Create on XML data so okay to dealloc here
					configPropertyList = nil;
					return nil;
				}
				else
				{
				
					DBGLOG( kLogPlugin, "Have successfully read the LDAP XML config data" );

					//if config data is up to date with latest default mappings then use them
					if (strcmp(configVersion,"DSLDAPv3PlugIn Version 1.5") == 0)
					{
						//now verify the inServer, inPortNumber and inIsSSL
						
						if ( CFDictionaryContainsKey( serverConfigDict, CFSTR( kXMLServerKey ) ) )
						{
							cfStringRef = (CFStringRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLServerKey ) );
							if ( cfStringRef != nil )
							{
								if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
								{
									//assume that the extracted strings will be significantly less than 1024 characters
									tmpBuff = (char *)::calloc(1, 1024);
									if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
									{
										server = (char *)::calloc(1+strlen(tmpBuff),1);
										::strcpy(server, tmpBuff);
										if (strcmp(server,inServer) != 0)
										{
											//replace the server value
											bUpdate = true;
											cfStringRef = CFStringCreateWithCString(kCFAllocatorDefault, inServer, kCFStringEncodingUTF8);
											CFDictionaryReplaceValue(serverConfigDict, CFSTR( kXMLServerKey ), cfStringRef);
											CFRelease(cfStringRef);
											cfStringRef = nil;
										}
										free(server);
										server = nil;
									}
									free(tmpBuff);
									tmpBuff = nil;
								}
								//CFRelease(cfStringRef); // no since pointer only from Get
							}
						}
			
						if ( CFDictionaryContainsKey( serverConfigDict, CFSTR( kXMLPortNumberKey ) ) )
						{
							cfNumber = (CFNumberRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLPortNumberKey ) );
							if ( cfNumber != nil )
							{
								cfNumBool = CFNumberGetValue(cfNumber, kCFNumberIntType, &portNumber);
								if (portNumber != inPortNumber)
								{
									//replace the port number
									bUpdate = true;
									cfNumber = CFNumberCreate(NULL,kCFNumberIntType,&inPortNumber);
									CFDictionaryReplaceValue(serverConfigDict, CFSTR( kXMLPortNumberKey ), cfNumber);
									CFRelease(cfNumber);
									cfNumber = 0;
								}
								//CFRelease(cfNumber); // no since pointer only from Get
							}
						}
			
						if ( CFDictionaryContainsKey( serverConfigDict, CFSTR( kXMLIsSSLFlagKey ) ) )
						{
							cfBool= (CFBooleanRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLIsSSLFlagKey ) );
							if (cfBool != nil)
							{
								bIsSSL = CFBooleanGetValue( cfBool );
								if (bIsSSL != inIsSSL)
								{
									//replace the SSL flag
									bUpdate = true;
									if (inIsSSL)
									{
										cfBool = kCFBooleanTrue;
									}
									else
									{
										cfBool = kCFBooleanFalse;
									}
									CFDictionaryReplaceValue(serverConfigDict, CFSTR( kXMLIsSSLFlagKey ), cfBool);						
								}
								//CFRelease( cfBool ); // no since pointer only from Get
							}
						}
						
						if ( CFDictionaryContainsKey( serverConfigDict, CFSTR( kXMLServerMappingsFlagKey ) ) )
						{
							cfBool= (CFBooleanRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLServerMappingsFlagKey ) );
							if (cfBool != nil)
							{
								bIsSrvrMappings = CFBooleanGetValue( cfBool );
								if (!bIsSrvrMappings)
								{
									bUpdate = true;
									CFDictionaryReplaceValue(serverConfigDict, CFSTR( kXMLServerMappingsFlagKey ), kCFBooleanTrue);						
								}
								//CFRelease( cfBool ); // no since pointer only from Get
							}
						}
						else
						{
							bUpdate = true;
							CFDictionarySetValue(serverConfigDict, CFSTR( kXMLServerMappingsFlagKey ), kCFBooleanTrue);
						}
						
						if ( CFDictionaryContainsKey( serverConfigDict, CFSTR( kXMLMakeDefLDAPFlagKey ) ) )
						{
							cfBool = (CFBooleanRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLMakeDefLDAPFlagKey ) );
							if (cfBool != nil)
							{
								bIsDefLDAP = CFBooleanGetValue( cfBool );
								if (!bIsDefLDAP && inMakeDefLDAP)
								{
									bUpdate = true;
									CFDictionaryReplaceValue(serverConfigDict, CFSTR( kXMLMakeDefLDAPFlagKey ), kCFBooleanTrue);
								}
								else if (bIsDefLDAP && !inMakeDefLDAP)
								{
									bUpdate = true;
									CFDictionaryReplaceValue(serverConfigDict, CFSTR( kXMLMakeDefLDAPFlagKey ), kCFBooleanFalse);
								}
							}
						}
						else
						{
							bUpdate = true;
							if (inMakeDefLDAP)
							{
								CFDictionarySetValue(serverConfigDict, CFSTR( kXMLMakeDefLDAPFlagKey ), kCFBooleanTrue);
							}
							else
							{
								CFDictionarySetValue(serverConfigDict, CFSTR( kXMLMakeDefLDAPFlagKey ), kCFBooleanFalse);
							}
						}
			
						if (bUpdate)
						{
							//create a new XML blob
							outXMLData = CFPropertyListCreateXMLData( kCFAllocatorDefault, serverConfigDict);
						}
					}                        
					delete(configVersion);
				}//if (configVersion != nil)
				// don't release the serverConfigDict since it is the cast configPropertyList
			}//if (serverConfigDict != nil)
			
			CFRelease(configPropertyList); // built from Create on XML data so okay to dealloc here
			configPropertyList = nil;

		}//if (configPropertyList != nil )
	} // inXMLData != nil
		
	return( outXMLData );

} // VerifyAndUpdateServerLocation

// ---------------------------------------------------------------------------
//	* AddLDAPServer
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Configs::AddLDAPServer( CFDataRef inXMLData )
{
	sInt32					siResult			= eDSNoErr;
	CFStringRef				errorString			= nil;
	CFPropertyListRef		configPropertyList	= nil;
	CFMutableDictionaryRef	serverConfigDict	= nil;
	char				   *configVersion		= nil;

	try
	{	
		if (inXMLData != nil)
		{
			// extract the config dictionary from the XML data.
			configPropertyList = CFPropertyListCreateFromXMLData(	kCFAllocatorDefault,
																	inXMLData,
																	kCFPropertyListMutableContainers,
																	//could also use kCFPropertyListMutableContainers
																   &errorString);
			
			if (configPropertyList != nil )
			{
				//make the propertylist a dict
				if ( CFDictionaryGetTypeID() == CFGetTypeID( configPropertyList ) )
				{
					serverConfigDict = (CFMutableDictionaryRef) configPropertyList; //don't need mutable really
				}
				
				if (serverConfigDict != nil)
				{
					//get version, and the specific LDAP server config
					
					//config data version
					configVersion = GetVersion(serverConfigDict);
					//TODO KW check for correct version? not necessary really since backward compatible?
					if ( configVersion == nil )
					{
						syslog(LOG_INFO,"DSLDAPv3PlugIn: Obtained LDAP server mappings is missing the version string.");
						throw( (sInt32)eDSVersionMismatch ); //KW need eDSPlugInConfigFileError
					}
					if (configVersion != nil)
					{
					
						DBGLOG( kLogPlugin, "Have successfully read the LDAP XML config data" );

                        //if config data is up to date with latest default mappings then use them
                        if (strcmp(configVersion,"DSLDAPv3PlugIn Version 1.5") == 0)
                        {
							siResult = MakeLDAPConfig(serverConfigDict, fConfigTableLen, true);
                        }
						else
						{
							syslog(LOG_INFO,"DSLDAPv3PlugIn: Obtained LDAP server mappings contain incorrect version string [%s] instead of [DSLDAPv3PlugIn Version 1.5].", configVersion);
						}
						delete(configVersion);
						
					}//if (configVersion != nil)
					
					// don't release the serverConfigDict since it is the cast configPropertyList
					
				}//if (serverConfigDict != nil)
				
				CFRelease(configPropertyList); // built from Create on XML data so okay to dealloc here
				configPropertyList = nil;
	
			}//if (configPropertyList != nil )
		} // fXMLData != nil
		
	} // try
	catch ( sInt32 err )
	{
		siResult = err;
		if (configPropertyList != nil)
		{
			CFRelease(configPropertyList); // built from Create on XML data so okay to dealloc here
			configPropertyList = nil;
		}
	}
	return( siResult );

} // AddLDAPServer

// --------------------------------------------------------------------------------
//	* CheckForServerMappings
// --------------------------------------------------------------------------------

CFDictionaryRef CLDAPv3Configs::CheckForServerMappings ( CFDictionaryRef ldapDict )
{
	char			   *tmpBuff		= nil;
	CFIndex				cfBuffSize	= 1024;
	CFStringRef			cfStringRef	= nil;
	CFBooleanRef		cfBool		= false;
	unsigned char		cfNumBool	= false;
	CFNumberRef			cfNumber	= 0;
	char			   *server		= nil;
	char			   *mapSearchBase = nil;
	int					portNumber	= 389;
	bool				bIsSSL		= false;
	bool				bServerMappings	= false;
	bool				bUseConfig	= false;
	bool				bReferrals  = true;		// referrals by default
	CFDictionaryRef		outDict		= nil;
	CFStringRef			errorString;

	if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLEnableUseFlagKey ) ) )
	{
		//assume that the extracted strings will be significantly less than 1024 characters
		tmpBuff = (char *)::calloc(1, 1024);
		
		cfBool = (CFBooleanRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLEnableUseFlagKey ) );
		if (cfBool != nil)
		{
			bUseConfig = CFBooleanGetValue( cfBool );
			//CFRelease( cfBool ); // no since pointer only from Get
		}
		//continue if this configuration was enabled by the user
		//no error condition returned if this configuration is not used due to the enable use flag
		if ( bUseConfig )
		{

			if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLServerMappingsFlagKey ) ) )
			{
				cfBool = (CFBooleanRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLServerMappingsFlagKey ) );
				if (cfBool != nil)
				{
					bServerMappings = CFBooleanGetValue( cfBool );
					//CFRelease( cfBool ); // no since pointer only from Get
				}
			}

			if (bServerMappings)
			{
				//retrieve all the relevant values (servername, mapsearchbase, portnumber, IsSSL) to enable server mapping retrieval
				
				if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLServerKey ) ) )
				{
					cfStringRef = (CFStringRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLServerKey ) );
					if ( cfStringRef != nil )
					{
						if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
						{
							::memset(tmpBuff,0,1024);
							if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
							{
								server = (char *)::calloc(1+strlen(tmpBuff),1);
								::strcpy(server, tmpBuff);
							}
						}
						//CFRelease(cfStringRef); // no since pointer only from Get
					}
				}
	
				if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLIsSSLFlagKey ) ) )
				{
					cfBool= (CFBooleanRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLIsSSLFlagKey ) );
					if (cfBool != nil)
					{
						bIsSSL = CFBooleanGetValue( cfBool );
						//CFRelease( cfBool ); // no since pointer only from Get
						if (bIsSSL)
						{
							portNumber = 636; // default for SSL ie. if no port given below
						}
					}
				}

				if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLPortNumberKey ) ) )
				{
					cfNumber = (CFNumberRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLPortNumberKey ) );
					if ( cfNumber != nil )
					{
						cfNumBool = CFNumberGetValue(cfNumber, kCFNumberIntType, &portNumber);
						//CFRelease(cfNumber); // no since pointer only from Get
					}
				}
	
				if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLMapSearchBase ) ) )
				{
					cfStringRef = (CFStringRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLMapSearchBase ) );
					if ( cfStringRef != nil )
					{
						if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
						{
							::memset(tmpBuff,0,1024);
							if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
							{
								mapSearchBase = (char *)::calloc(1+strlen(tmpBuff),1);
								::strcpy(mapSearchBase, tmpBuff);
							}
						}
						//CFRelease(cfStringRef); // no since pointer only from Get
					}
				}
				
				if( cfBool = (CFBooleanRef)CFDictionaryGetValue( ldapDict, CFSTR(kXMLReferralFlagKey) ) )
				{
					if( CFGetTypeID( cfBool ) == CFBooleanGetTypeID() )
					{
						bReferrals = CFBooleanGetValue( cfBool );
					}
				}
				
				CFDataRef ourXMLData = nil;
				ourXMLData = RetrieveServerMappings( server, mapSearchBase, portNumber, bIsSSL, bReferrals );
				if (ourXMLData != nil)
				{
					CFPropertyListRef configPropertyList = nil;
					// extract the config dictionary from the XML data.
					configPropertyList = CFPropertyListCreateFromXMLData(	kCFAllocatorDefault,
																			ourXMLData,
																			kCFPropertyListImmutable,
																			//could also use kCFPropertyListMutableContainers
																		   &errorString);
					
					if (configPropertyList != nil )
					{
						//make the propertylist a dict
						if ( CFDictionaryGetTypeID() == CFGetTypeID( configPropertyList ) )
						{
							outDict = (CFDictionaryRef) configPropertyList;
						}
					}//if (configPropertyList != nil )
					
					CFRelease(ourXMLData);
					ourXMLData = nil;
				}

			}

			if ( server != nil ) 
			{
				free( server );
				server = nil;
			}
			if ( mapSearchBase != nil ) 
			{
				free( mapSearchBase );
				mapSearchBase = nil;
			}
			
		}// if ( bUseConfig )
		
		//free up the tmpBuff
		delete( tmpBuff );

	}// if kXMLEnableUseFlagKey set

	// return if nil or not
	return( outDict );

} // CheckForServerMappings


// --------------------------------------------------------------------------------
//	* MakeLDAPConfig
// --------------------------------------------------------------------------------

sInt32 CLDAPv3Configs::MakeLDAPConfig( CFDictionaryRef ldapDict, sInt32 inIndex, bool inEnsureServerMappings )
{
	sInt32				siResult	= eDSNoErr;
	char			   *tmpBuff		= nil;
	CFIndex				cfBuffSize	= 1024;
	CFStringRef			cfStringRef	= nil;
	CFDataRef			cfDataRef	= nil;
	CFBooleanRef		cfBool		= false;
	unsigned char		cfNumBool	= false;
	CFNumberRef			cfNumber	= 0;
	char			   *uiName		= nil;
	char			   *server		= nil;
	char			   *account		= nil;
    char			   *mapSearchBase = nil;
	char			   *password	= nil;
	int					passwordLen	= 0;
	int					opencloseTO	= 15;
	int					idleTO		= 2;
	int					delayRebindTry = 120;
	int					searchTO	= 120;
	int					portNumber	= 389;
	bool				bIsSSL		= false;
	bool				bServerMappings	= false;
	bool				bMakeDefLDAP= false;
	bool				bUseSecure	= false;
	bool				bUseConfig	= false;
	bool				bReferrals  = true;		// default to referrals on
    sLDAPConfigData	   *pConfig		= nil;
    sLDAPConfigData	   *xConfig		= nil;
	uInt32				serverIndex = 0;
	bool				reuseEntry	= false;

	if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLEnableUseFlagKey ) ) )
	{
		//assume that the extracted strings will be significantly less than 1024 characters
		tmpBuff = (char *)::calloc(1, 1024);
		
		cfBool = (CFBooleanRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLEnableUseFlagKey ) );
		if (cfBool != nil)
		{
			bUseConfig = CFBooleanGetValue( cfBool );
			//CFRelease( cfBool ); // no since pointer only from Get
		}
		//continue if this configuration was enabled by the user
		//no error condition returned if this configuration is not used due to the enable use flag
		if ( bUseConfig )
		{
			//need to get the server name first
			if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLServerKey ) ) )
			{
				cfStringRef = (CFStringRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLServerKey ) );
				if ( cfStringRef != nil )
				{
					if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
					{
						::memset(tmpBuff,0,1024);
						if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
						{
							server = (char *)::calloc(1+strlen(tmpBuff),1);
							::strcpy(server, tmpBuff);
						}
					}
					//CFRelease(cfStringRef); // no since pointer only from Get
				}
			}

			//Need to check here if the config already exists ie. the server name exists
			//if it does then assume that this will replace what was given before
			
			if (CheckForConfig(server, serverIndex))
			{
				reuseEntry = true;
		        xConfig = (sLDAPConfigData *)pConfigTable->GetItemData( serverIndex );
/*
		        if (xConfig != nil)
		        {
		            // delete the contents of sLDAPConfigData here
		            // not checking the return status of the clean here
					// since we know xConfig is NOT nil going in
		            CleanLDAPConfigData( xConfig );
		            // delete the sLDAPConfigData itself
		            delete( xConfig );
		            xConfig = nil;
		            // remove the table entry
		            pConfigTable->RemoveItem( serverIndex );
		        }
*/
			}
			
			//Enable Use flag is NOT provided to the configTable
			//retrieve all the others for the configTable
			
			if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLIsSSLFlagKey ) ) )
			{
				cfBool= (CFBooleanRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLIsSSLFlagKey ) );
				if (cfBool != nil)
				{
					bIsSSL = CFBooleanGetValue( cfBool );
					//CFRelease( cfBool ); // no since pointer only from Get
					if (bIsSSL)
					{
						portNumber = 636; // default for SSL ie. if no port given below
					}
				}
			}

			if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLPortNumberKey ) ) )
			{
				cfNumber = (CFNumberRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLPortNumberKey ) );
				if ( cfNumber != nil )
				{
					cfNumBool = CFNumberGetValue(cfNumber, kCFNumberIntType, &portNumber);
					//CFRelease(cfNumber); // no since pointer only from Get
				}
			}
			if (inEnsureServerMappings)
			{
				bServerMappings = true;
			}
			else
			{
				if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLServerMappingsFlagKey ) ) )
				{
					cfBool = (CFBooleanRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLServerMappingsFlagKey ) );
					if (cfBool != nil)
					{
						bServerMappings = CFBooleanGetValue( cfBool );
						//CFRelease( cfBool ); // no since pointer only from Get
					}
				}
			}

			if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLOpenCloseTimeoutSecsKey ) ) )
			{
				cfNumber = (CFNumberRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLOpenCloseTimeoutSecsKey ) );
				if ( cfNumber != nil )
				{
					cfNumBool = CFNumberGetValue(cfNumber, kCFNumberIntType, &opencloseTO);
					//CFRelease(cfNumber); // no since pointer only from Get
				}
			}

			if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLIdleTimeoutMinsKey ) ) )
			{
				cfNumber = (CFNumberRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLIdleTimeoutMinsKey ) );
				if ( cfNumber != nil )
				{
					cfNumBool = CFNumberGetValue(cfNumber, kCFNumberIntType, &idleTO);
					//CFRelease(cfNumber); // no since pointer only from Get
				}
			}

			if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLDelayedRebindTrySecsKey ) ) )
			{
				cfNumber = (CFNumberRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLDelayedRebindTrySecsKey ) );
				if ( cfNumber != nil )
				{
					cfNumBool = CFNumberGetValue(cfNumber, kCFNumberIntType, &delayRebindTry);
					//CFRelease(cfNumber); // no since pointer only from Get
				}
			}

			if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLSearchTimeoutSecsKey ) ) )
			{
				cfNumber = (CFNumberRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLSearchTimeoutSecsKey ) );
				if ( cfNumber != nil )
				{
					cfNumBool = CFNumberGetValue(cfNumber, kCFNumberIntType, &searchTO);
					//CFRelease(cfNumber); // no since pointer only from Get
				}
			}

			if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLSecureUseFlagKey ) ) )
			{
				cfBool= (CFBooleanRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLSecureUseFlagKey ) );
				if (cfBool != nil)
				{
					bUseSecure = CFBooleanGetValue( cfBool );
					//CFRelease( cfBool ); // no since pointer only from Get
				}
			}

			//null strings are acceptable but not preferred
			//ie. the new char will be of length one and the strcpy will copy the "" - empty string
			if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLUserDefinedNameKey ) ) )
			{
				cfStringRef = (CFStringRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLUserDefinedNameKey ) );
				if ( cfStringRef != nil )
				{
					if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
					{
						::memset(tmpBuff,0,1024);
						if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
						{
							uiName = (char *)::calloc(1+strlen(tmpBuff),1);
							::strcpy(uiName, tmpBuff);
						}
					}
					//CFRelease(cfStringRef); // no since pointer only from Get
				}
			}

			if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLServerAccountKey ) ) )
			{
				cfStringRef = (CFStringRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLServerAccountKey ) );
				if ( cfStringRef != nil )
				{
					if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
					{
						::memset(tmpBuff,0,1024);
						if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
						{
							account = (char *)::calloc(1+strlen(tmpBuff),1);
							::strcpy(account, tmpBuff);
						}
					}
					//CFRelease(cfStringRef); // no since pointer only from Get
				}
			}

			if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLServerPasswordKey ) ) )
			{
				cfStringRef = (CFStringRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLServerPasswordKey ) );
				if ( cfStringRef != nil )
				{
					if ( CFGetTypeID( cfStringRef ) == CFDataGetTypeID() )
					{
						cfDataRef = (CFDataRef)cfStringRef;
						passwordLen = CFDataGetLength(cfDataRef);
						password = (char*)::calloc(1+passwordLen,1);
						CFDataGetBytes(cfDataRef, CFRangeMake(0,passwordLen), (UInt8*)password);
					}
					else if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
					{
						::memset(tmpBuff,0,1024);
						if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
						{
							password = (char *)::calloc(1+strlen(tmpBuff),1);
							::strcpy(password, tmpBuff);
						}
					}
					//CFRelease(cfStringRef); // no since pointer only from Get
				}
			}

			if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLMakeDefLDAPFlagKey ) ) )
			{
				cfBool = (CFBooleanRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLMakeDefLDAPFlagKey ) );
				if (cfBool != nil)
				{
					bMakeDefLDAP = CFBooleanGetValue( cfBool );
					//CFRelease( cfBool ); // no since pointer only from Get
				}
			}

            if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLMapSearchBase ) ) )
            {
                cfStringRef = (CFStringRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLMapSearchBase ) );
                if ( cfStringRef != nil )
                {
                    if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
                    {
                        ::memset(tmpBuff,0,1024);
                        if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
                        {
                            mapSearchBase = (char *)::calloc(1+strlen(tmpBuff),1);
                            ::strcpy(mapSearchBase, tmpBuff);
                        }
                    }
                    //CFRelease(cfStringRef); // no since pointer only from Get
                }
            }

			if( cfBool = (CFBooleanRef) CFDictionaryGetValue( ldapDict, CFSTR(kXMLReferralFlagKey) ) )
			{
				if( CFGetTypeID(cfBool) == CFBooleanGetTypeID() )
				{
					bReferrals = CFBooleanGetValue( cfBool );
				}
			}
						
			//setup the config table
			// MakeLDAPConfigData does not consume the strings passed in so we need to free them below
			if (reuseEntry)
			{
				pConfig = MakeLDAPConfigData( uiName, server, opencloseTO, idleTO, delayRebindTry, searchTO, portNumber, bUseSecure, account, password, bMakeDefLDAP, bServerMappings, bIsSSL, mapSearchBase, bReferrals, xConfig );
            }
			else
			{
				pConfig = MakeLDAPConfigData( uiName, server, opencloseTO, idleTO, delayRebindTry, searchTO, portNumber, bUseSecure, account, password, bMakeDefLDAP, bServerMappings, bIsSSL, mapSearchBase, bReferrals, nil );
			}
			//get the mappings from the config ldap dict
			BuildLDAPMap( pConfig, ldapDict, bServerMappings );
			
			if ( uiName != nil ) 
			{
				free( uiName );
				uiName = nil;
			}
			if ( server != nil ) 
			{
				free( server );
				server = nil;
			}
			if ( account != nil ) 
			{
				free( account );
				account = nil;
			}
			if ( password != nil ) 
			{
				free( password );
				password = nil;
			}
			if ( mapSearchBase != nil ) 
			{
				free( mapSearchBase );
				mapSearchBase = nil;
			}
			
			if (reuseEntry)
			{
				//pConfigTable->AddItem( serverIndex, pConfig ); //no longer removed above
			}
			else
			{
				pConfigTable->AddItem( inIndex, pConfig );
				fConfigTableLen++;
			}

		}// if ( bUseConfig )
		
		//free up the tmpBuff
		delete( tmpBuff );

	}// if kXMLEnableUseFlagKey set

	// return if nil or not
	return( siResult );

} // MakeLDAPConfig


// --------------------------------------------------------------------------------
//	* MakeServerBasedMappingsLDAPConfig
// --------------------------------------------------------------------------------

sInt32 CLDAPv3Configs::MakeServerBasedMappingsLDAPConfig ( char *inServer, char *inMapSearchBase, int inOpenCloseTO, int inIdleTO, int inDelayRebindTry, int inSearchTO, int inPortNumber, bool inIsSSL, bool inMakeDefLDAP, bool inReferrals )
{
	sInt32				siResult	= eDSNoErr;
	uInt32				serverIndex = 0;
	bool				reuseEntry	= false;
    sLDAPConfigData	   *xConfig		= nil;
    sLDAPConfigData	   *pConfig		= nil;

    //Need to check here if the config already exists ie. the server name exists
    //if it does then assume that this will replace what was given before
    
    if (CheckForConfig(inServer, serverIndex))
    {
        reuseEntry = true;
        xConfig = (sLDAPConfigData *)pConfigTable->GetItemData( serverIndex );
/*
        if (xConfig != nil)
        {
            // delete the contents of sLDAPConfigData here
            // not checking the return status of the clean here
            // since we know xConfig is NOT nil going in
            CleanLDAPConfigData( xConfig );
            // delete the sLDAPConfigData itself
            delete( xConfig );
            xConfig = nil;
            // remove the table entry
            pConfigTable->RemoveItem( serverIndex );
        }
*/
    }
    
    if (reuseEntry)
    {
		//setup the config table
		// MakeLDAPConfigData does not consume the strings passed in but them are arguments so don't need to free them below
		pConfig = MakeLDAPConfigData( inServer, inServer, inOpenCloseTO, inIdleTO, inDelayRebindTry, inSearchTO, inPortNumber, false, nil, nil, inMakeDefLDAP, true, inIsSSL, inMapSearchBase, inReferrals, xConfig );
        //pConfigTable->AddItem( serverIndex, pConfig ); //no longer removed above
    }
    else
    {
		//setup the config table
		// MakeLDAPConfigData does not consume the strings passed in but them are arguments so don't need to free them below
		pConfig = MakeLDAPConfigData( inServer, inServer, inOpenCloseTO, inIdleTO, inDelayRebindTry, inSearchTO, inPortNumber, false, nil, nil, inMakeDefLDAP, true, inIsSSL, inMapSearchBase, inReferrals, nil );
    
        pConfigTable->AddItem( fConfigTableLen, pConfig );
        fConfigTableLen++;
    }

	return( siResult );

} // MakeServerBasedMappingsLDAPConfig


// --------------------------------------------------------------------------------
//	* CheckForConfig
// --------------------------------------------------------------------------------

bool CLDAPv3Configs::CheckForConfig ( char *inServerName, uInt32 &inConfigTableIndex )
{
	bool				result 		= false;
    uInt32				iTableIndex	= 0;
    sLDAPConfigData	   *pConfig		= nil;

	if (inServerName != nil)
	{
		//need to cycle through the config table
		for (iTableIndex=0; iTableIndex<fConfigTableLen; iTableIndex++)
		{
			pConfig = (sLDAPConfigData *)pConfigTable->GetItemData( iTableIndex );
			if (pConfig != nil)
			{
				if (pConfig->fServerName != nil)
				{
					if (::strcmp(pConfig->fServerName, inServerName) == 0 )
					{
						result = true;
						inConfigTableIndex = iTableIndex;
						break;
					}
				}
			}
		}
	}
    
    return(result);
	
	
} // CheckForConfig


// --------------------------------------------------------------------------------
//	* BuildLDAPMap
// --------------------------------------------------------------------------------

sInt32 CLDAPv3Configs::BuildLDAPMap ( sLDAPConfigData *inConfig, CFDictionaryRef ldapDict, bool inServerMapppings )
{
	sInt32					siResult			= eDSNoErr; // used for?
	CFArrayRef				cfArrayRef			= nil;

	//check that array contains something if server mappings is specified ie. DirectoryAccess provides empty arrays
	if (inServerMapppings)
	{
		cfArrayRef = nil;
		cfArrayRef = GetRecordTypeMapArray(ldapDict);
		if ( (cfArrayRef != nil) && (CFGetTypeID( cfArrayRef ) == CFArrayGetTypeID()) && (CFArrayGetCount(cfArrayRef) > 0) )
		{
			//clean out the old server mappings if they exist
			if (inConfig->fRecordTypeMapCFArray != nil)
			{
				CFRelease(inConfig->fRecordTypeMapCFArray);
				inConfig->fRecordTypeMapCFArray	= 0;
			}
			inConfig->fRecordTypeMapCFArray = CFArrayCreateCopy(kCFAllocatorDefault, cfArrayRef);
		}
		
		cfArrayRef = nil;
		cfArrayRef = GetAttributeTypeMapArray(ldapDict);
		if ( (cfArrayRef != nil) && (CFGetTypeID( cfArrayRef ) == CFArrayGetTypeID()) && (CFArrayGetCount(cfArrayRef) > 0) )
		{
			//clean out the old server mappings if they exist
			if (inConfig->fAttrTypeMapCFArray != nil)
			{
				CFRelease(inConfig->fAttrTypeMapCFArray);
				inConfig->fAttrTypeMapCFArray	= 0;
			}
			inConfig->fAttrTypeMapCFArray = CFArrayCreateCopy(kCFAllocatorDefault, cfArrayRef);
		}
	}
	//always do this if server mappings are NOT specified
	else
	{
		cfArrayRef = nil;
		cfArrayRef = GetRecordTypeMapArray(ldapDict);
		if ( (cfArrayRef != nil) && (CFGetTypeID( cfArrayRef ) == CFArrayGetTypeID()) && (CFArrayGetCount(cfArrayRef) > 0) )
		{
			if( inConfig->fRecordTypeMapCFArray )
			{
				CFRelease( inConfig->fRecordTypeMapCFArray );
				inConfig->fRecordTypeMapCFArray = NULL;
			}
			inConfig->fRecordTypeMapCFArray = CFArrayCreateCopy(kCFAllocatorDefault, cfArrayRef);
		}
		
		cfArrayRef = nil;
		cfArrayRef = GetAttributeTypeMapArray(ldapDict);
		if ( (cfArrayRef != nil) && (CFGetTypeID( cfArrayRef ) == CFArrayGetTypeID()) && (CFArrayGetCount(cfArrayRef) > 0) )
		{
			if( inConfig->fAttrTypeMapCFArray )
			{
				CFRelease( inConfig->fAttrTypeMapCFArray );
				inConfig->fAttrTypeMapCFArray = NULL;
			}
			inConfig->fAttrTypeMapCFArray = CFArrayCreateCopy(kCFAllocatorDefault, cfArrayRef);
		}
	}
	
	cfArrayRef = nil;
	cfArrayRef = GetReplicaHostnameListArray(ldapDict);
	if ( (cfArrayRef != nil) && (CFGetTypeID( cfArrayRef ) == CFArrayGetTypeID()) && (CFArrayGetCount(cfArrayRef) > 0) )
	{
		//clean out the old replica host names before we replace it
		if (inConfig->fReplicaHostnames != nil)
		{
			CFRelease(inConfig->fReplicaHostnames);
			inConfig->fReplicaHostnames	= NULL;
		}
		inConfig->fReplicaHostnames = CFArrayCreateMutableCopy(kCFAllocatorDefault, NULL, cfArrayRef);
	}
	
	cfArrayRef = nil;
	cfArrayRef = GetWriteableHostnameListArray(ldapDict);
	if ( (cfArrayRef != nil) && (CFGetTypeID( cfArrayRef ) == CFArrayGetTypeID()) && (CFArrayGetCount(cfArrayRef) > 0) )
	{
		//clean out the old replica host names before we replace it
		if (inConfig->fWriteableHostnames != nil)
		{
			CFRelease(inConfig->fWriteableHostnames);
			inConfig->fWriteableHostnames	= NULL;
		}
		inConfig->fWriteableHostnames = CFArrayCreateMutableCopy(kCFAllocatorDefault, NULL, cfArrayRef);
	}
	
	return( siResult );

} // BuildLDAPMap


// --------------------------------------------------------------------------------
//	* GetVersion
// --------------------------------------------------------------------------------

char *CLDAPv3Configs::GetVersion ( CFDictionaryRef configDict )
{
	char			   *outVersion	= nil;
	CFStringRef			cfStringRef	= nil;
	char			   *tmpBuff		= nil;
	CFIndex				cfBuffSize	= 1024;

	if ( CFDictionaryContainsKey( configDict, CFSTR( kXMLLDAPVersionKey ) ) )
	{
		cfStringRef = (CFStringRef)CFDictionaryGetValue( configDict, CFSTR( kXMLLDAPVersionKey ) );
		if ( cfStringRef != nil )
		{
			if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
			{
				//assume that the extracted strings will be significantly less than 1024 characters
				tmpBuff = new char[1024];
				::memset(tmpBuff,0,1024);
				if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
				{
					outVersion = new char[1+strlen(tmpBuff)];
					::strcpy(outVersion, tmpBuff);
				}
				delete( tmpBuff );
			}
			//CFRelease( cfStringRef ); // no since pointer only from Get
		}
	}

	// return if nil or not
	return( outVersion );

} // GetVersion


// --------------------------------------------------------------------------------
//	* GetConfigArray
// --------------------------------------------------------------------------------

CFArrayRef CLDAPv3Configs::GetConfigArray ( CFDictionaryRef configDict )
{
	CFArrayRef		cfArrayRef	= nil;

	if ( CFDictionaryContainsKey( configDict, CFSTR( kXMLConfigArrayKey ) ) )
	{
		cfArrayRef = (CFArrayRef)CFDictionaryGetValue( configDict, CFSTR( kXMLConfigArrayKey ) );
	}

	// return if nil or not
	return( cfArrayRef );

} // GetConfigArray


// --------------------------------------------------------------------------------
//	* GetDefaultRecordTypeMapArray
// --------------------------------------------------------------------------------

CFArrayRef CLDAPv3Configs::GetDefaultRecordTypeMapArray ( CFDictionaryRef configDict )
{
	CFArrayRef		cfArrayRef	= nil;

	if ( CFDictionaryContainsKey( configDict, CFSTR( kXMLDefaultRecordTypeMapArrayKey ) ) )
	{
		cfArrayRef = (CFArrayRef)CFDictionaryGetValue( configDict, CFSTR( kXMLDefaultRecordTypeMapArrayKey ) );
	}

	// return if nil or not
	return( cfArrayRef );

} // GetDefaultRecordTypeMapArray


// --------------------------------------------------------------------------------
//	* GetDefaultAttrTypeMapArray
// --------------------------------------------------------------------------------

CFArrayRef CLDAPv3Configs::GetDefaultAttrTypeMapArray ( CFDictionaryRef configDict )
{
	CFArrayRef		cfArrayRef	= nil;

	if ( CFDictionaryContainsKey( configDict, CFSTR( kXMLDefaultAttrTypeMapArrayKey ) ) )
	{
		cfArrayRef = (CFArrayRef)CFDictionaryGetValue( configDict, CFSTR( kXMLDefaultAttrTypeMapArrayKey ) );
	}

	// return if nil or not
	return( cfArrayRef );

} // GetDefaultAttrTypeMapArray


// --------------------------------------------------------------------------------
//	* GetReplicaHostnameListArray
// --------------------------------------------------------------------------------

CFArrayRef CLDAPv3Configs::GetReplicaHostnameListArray ( CFDictionaryRef configDict )
{
	CFArrayRef		cfArrayRef	= nil;

	if ( CFDictionaryContainsKey( configDict, CFSTR( kXMLReplicaHostnameListArrayKey ) ) )
	{
		cfArrayRef = (CFArrayRef)CFDictionaryGetValue( configDict, CFSTR( kXMLReplicaHostnameListArrayKey ) );
	}

	// return if nil or not
	return( cfArrayRef );

} // GetReplicaHostnameListArray

// --------------------------------------------------------------------------------
//	* GetWriteableHostnameListArray
// --------------------------------------------------------------------------------

CFArrayRef CLDAPv3Configs::GetWriteableHostnameListArray ( CFDictionaryRef configDict )
{
	CFArrayRef		cfArrayRef	= nil;

	if ( CFDictionaryContainsKey( configDict, CFSTR( kXMLWriteableHostnameListArrayKey ) ) )
	{
		cfArrayRef = (CFArrayRef)CFDictionaryGetValue( configDict, CFSTR( kXMLWriteableHostnameListArrayKey ) );
	}

	// return if nil or not
	return( cfArrayRef );

} // GetWriteableHostnameListArray

// --------------------------------------------------------------------------------
//	* GetRecordTypeMapArray
// --------------------------------------------------------------------------------

CFArrayRef CLDAPv3Configs::GetRecordTypeMapArray ( CFDictionaryRef configDict )
{
	CFArrayRef		cfArrayRef	= nil;

	if ( CFDictionaryContainsKey( configDict, CFSTR( kXMLRecordTypeMapArrayKey ) ) )
	{
		cfArrayRef = (CFArrayRef)CFDictionaryGetValue( configDict, CFSTR( kXMLRecordTypeMapArrayKey ) );
	}

	// return if nil or not
	return( cfArrayRef );

} // GetRecordTypeMapArray

// --------------------------------------------------------------------------------
//	* GetAttributeTypeMapArray
// --------------------------------------------------------------------------------

CFArrayRef CLDAPv3Configs::GetAttributeTypeMapArray ( CFDictionaryRef configDict )
{
	CFArrayRef		cfArrayRef	= nil;

	if ( CFDictionaryContainsKey( configDict, CFSTR( kXMLAttrTypeMapArrayKey ) ) )
	{
		cfArrayRef = (CFArrayRef)CFDictionaryGetValue( configDict, CFSTR( kXMLAttrTypeMapArrayKey ) );
	}

	// return if nil or not
	return( cfArrayRef );

} // GetAttributeTypeMapArray

// --------------------------------------------------------------------------------
//	* GetNativeTypeMapArray
// --------------------------------------------------------------------------------

CFArrayRef CLDAPv3Configs::GetNativeTypeMapArray ( CFDictionaryRef configDict )
{
	CFArrayRef		cfArrayRef	= nil;

	if ( CFDictionaryContainsKey( configDict, CFSTR( kXMLNativeMapArrayKey ) ) )
	{
		cfArrayRef = (CFArrayRef)CFDictionaryGetValue( configDict, CFSTR( kXMLNativeMapArrayKey ) );
	}

	// return if nil or not
	return( cfArrayRef );

} // GetNativeTypeMapArray

// ---------------------------------------------------------------------------
//	* MakeLDAPConfigData
// ---------------------------------------------------------------------------

sLDAPConfigData *CLDAPv3Configs::MakeLDAPConfigData (	char *inName, char *inServerName,
													int inOpenCloseTO, int inIdleTO, int inDelayRebindTry,
													int inSearchTO, int inPortNum,
													bool inUseSecure,
													char *inAccount, char *inPassword,
													bool inMakeDefLDAP,
													bool inServerMappings,
													bool inIsSSL,
                                                    char *inMapSearchBase,
													bool inReferrals,
													sLDAPConfigData *inLDAPConfigData )
{
	sInt32				siResult		= eDSNoErr;
    sLDAPConfigData	   *configOut		= nil;
	sReplicaInfo	   *replicaHosts	= nil;
	CFMutableArrayRef	replicaHostNames	= nil;
	CFMutableArrayRef	writeableHosts		= nil;
	CFMutableArrayRef   saslMethods			= nil;

	if (inServerName != nil) 
	{
		if (inLDAPConfigData != nil)
		{
			configOut = inLDAPConfigData;
			
			replicaHosts = inLDAPConfigData->fReplicaHosts;
			inLDAPConfigData->fReplicaHosts = nil;
			
			replicaHostNames = inLDAPConfigData->fReplicaHostnames;
			inLDAPConfigData->fReplicaHostnames = nil;
			
			writeableHosts = inLDAPConfigData->fWriteableHostnames;
			inLDAPConfigData->fWriteableHostnames = nil;
			
			saslMethods = inLDAPConfigData->fSASLmethods;
			inLDAPConfigData->fSASLmethods = nil;
		}
		else
		{
			configOut = (sLDAPConfigData *) calloc(1, sizeof(sLDAPConfigData));
		}
		if ( configOut != nil )
		{
			siResult = CleanLDAPConfigData(configOut, inServerMappings);
	
			if (inName != nil)
			{
				configOut->fName			= new char[1+::strlen( inName )];
				::strcpy(configOut->fName, inName);
			}
			
			configOut->fServerName			= new char[1+::strlen( inServerName )];
			::strcpy(configOut->fServerName, inServerName);
			
			// we should probably keep things we've already discovered
			configOut->fReplicaHosts		= replicaHosts;
			configOut->fReplicaHostnames	= replicaHostNames;
			configOut->fWriteableHostnames  = writeableHosts;
			
			configOut->fOpenCloseTimeout	= inOpenCloseTO;
			configOut->fIdleTimeout			= inIdleTO;
			configOut->fDelayRebindTry		= inDelayRebindTry;
			configOut->fSearchTimeout		= inSearchTO;
			configOut->fServerPort			= inPortNum;
			configOut->bSecureUse			= inUseSecure;
			configOut->bUpdated				= true;
			configOut->bUseAsDefaultLDAP	= inMakeDefLDAP;
			configOut->bServerMappings		= inServerMappings;
			configOut->bIsSSL				= inIsSSL;
			configOut->fSASLmethods			= saslMethods;
			configOut->bReferrals			= inReferrals;
			
			if (inAccount != nil)
			{
				configOut->fServerAccount	= new char[1+::strlen( inAccount )];
				::strcpy(configOut->fServerAccount, inAccount);
			}
			if (inPassword != nil)
			{
				configOut->fServerPassword	= new char[1+::strlen( inPassword )];
				::strcpy(configOut->fServerPassword, inPassword);
			}
            if (inMapSearchBase != nil)
            {
                configOut->fMapSearchBase		= strdup(inMapSearchBase);
				if ( inServerMappings )
				{
					configOut->bGetServerMappings	= true;
				}
            }
			configOut->bBuildReplicaList		= true;
		}
	} // if (inServerName != nil)

	return( configOut );

} // MakeLDAPConfigData


// ---------------------------------------------------------------------------
//	* CleanLDAPConfigData
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Configs::CleanLDAPConfigData ( sLDAPConfigData *inConfig, bool inServerMappings )
{
    sInt32				siResult 	= eDSNoErr;
	sReplicaInfo	   *repIter		= nil;
    
    if ( inConfig == nil )
    {
        siResult = eDSBadContextData; // KW want an eDSBadConfigData??
	}
    else
    {
        if (inConfig->fName != nil)
        {
            free( inConfig->fName );
        }
        if (inConfig->fServerName != nil)
        {
            free( inConfig->fServerName );
        }
        if (inConfig->fServerAccount != nil)
        {
            free( inConfig->fServerAccount );
        }
        if (inConfig->fMapSearchBase != nil)
        {
            free( inConfig->fMapSearchBase );
        }
        if (inConfig->fServerPassword != nil)
        {
            free( inConfig->fServerPassword );
        }
		inConfig->fName					= nil;
		inConfig->fServerName			= nil;
		inConfig->fServerAccount		= nil;
		inConfig->fServerPassword		= nil;
        inConfig->fMapSearchBase		= nil;
		if (!inServerMappings) //retain the mappings if obtained from server mappings mechanism ie. update lazily
		{
			if (inConfig->fRecordTypeMapCFArray != 0)
			{
				CFRelease(inConfig->fRecordTypeMapCFArray);
				inConfig->fRecordTypeMapCFArray	= 0;
			}
			if (inConfig->fAttrTypeMapCFArray != 0)
			{
				CFRelease(inConfig->fAttrTypeMapCFArray);
				inConfig->fAttrTypeMapCFArray	= 0;
			}
		}
		if (inConfig->fReplicaHostnames != 0)
		{
			CFRelease(inConfig->fReplicaHostnames);
			inConfig->fReplicaHostnames	= 0;
		}
		if (inConfig->fWriteableHostnames != 0)
		{
			CFRelease(inConfig->fWriteableHostnames);
			inConfig->fWriteableHostnames	= 0;
		}
		if (inConfig->fReplicaHosts != nil)
		{
			repIter = inConfig->fReplicaHosts;
			while( repIter != nil)
			{
				inConfig->fReplicaHosts = repIter->fNext;
				freeaddrinfo( repIter->fAddrInfo );
				if (repIter->hostname != NULL)
				{
					CFRelease(repIter->hostname);
				}
				free(repIter);
				repIter = inConfig->fReplicaHosts;
			}
		}
		inConfig->fOpenCloseTimeout		= 15;
		inConfig->fIdleTimeout			= 2;
		inConfig->fDelayRebindTry		= 120;
		inConfig->fSearchTimeout		= 120;
		inConfig->fServerPort			= 389;
		inConfig->bSecureUse			= false;
		inConfig->bAvail				= false;
		inConfig->bUpdated				= false;
		inConfig->bUseAsDefaultLDAP		= false;
		inConfig->bServerMappings		= false;
		inConfig->bIsSSL				= false;
		inConfig->bOCBuilt				= false;
		inConfig->bGetServerMappings	= false;
		inConfig->bBuildReplicaList		= false;
		inConfig->bReferrals			= true; // we follow referrals by default
		
		if( inConfig->fSASLmethods )
		{
			CFRelease( inConfig->fSASLmethods );
			inConfig->fSASLmethods = NULL;
		}
		if (inConfig->fObjectClassSchema != nil)
		{
			for (ObjectClassMapCI iter = inConfig->fObjectClassSchema->begin(); iter != inConfig->fObjectClassSchema->end(); ++iter)
			{
				//need this since we have a structure here and not a class
				iter->second->fParentOCs.clear();
				iter->second->fOtherNames.clear();
				iter->second->fRequiredAttrs.clear();
				iter->second->fAllowedAttrs.clear();
				delete(iter->second);
				inConfig->fObjectClassSchema->erase(iter->first);
			}
			inConfig->fObjectClassSchema->clear();
			delete(inConfig->fObjectClassSchema);
			inConfig->fObjectClassSchema = nil;
		}
        
   }

    return( siResult );

} // CleanLDAPConfigData

// ---------------------------------------------------------------------------
//	* ExtractRecMap
// ---------------------------------------------------------------------------

char* CLDAPv3Configs::ExtractRecMap( const char *inRecType, CFArrayRef inRecordTypeMapCFArray, int inIndex, bool *outOCGroup, CFArrayRef *outOCListCFArray, ber_int_t* outScope )
{
	char				   *outResult			= nil;
	CFIndex					cfMapCount			= 0;
	CFIndex					cfNativeMapCount	= 0;
	sInt32					iMapIndex			= 0;
	CFStringRef				cfStringRef			= nil;
	CFStringRef				cfRecTypeRef		= nil;
	CFBooleanRef			cfBoolRef			= nil;
	char				   *tmpBuff				= nil;
	CFIndex					cfBuffSize			= 1024;
	CFArrayRef				cfNativeArrayRef	= nil;

	if ( (inRecordTypeMapCFArray != nil) && (inRecType != nil) )
	{
		cfRecTypeRef = CFStringCreateWithCString(kCFAllocatorDefault, inRecType, kCFStringEncodingUTF8);
		
		//now we can look for our Type mapping
		cfMapCount = ::CFArrayGetCount( inRecordTypeMapCFArray );
		if (cfMapCount != 0)
		{
			//loop through the maps
			for (iMapIndex = 0; iMapIndex < cfMapCount; iMapIndex++)
			{
				CFDictionaryRef		typeMapDict;
				typeMapDict = (CFDictionaryRef)::CFArrayGetValueAtIndex( inRecordTypeMapCFArray, iMapIndex );
				if ( typeMapDict != nil )
				{
					//retrieve the mappings
					// get the standard type label first
					if ( CFDictionaryContainsKey( typeMapDict, CFSTR( kXMLStdNameKey ) ) )
					{
						cfStringRef = (CFStringRef)CFDictionaryGetValue( typeMapDict, CFSTR( kXMLStdNameKey ) );
						if ( cfStringRef != nil )
						{
							if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
							{
								if (CFStringCompare(cfStringRef, cfRecTypeRef, 0) == kCFCompareEqualTo)
								{
									// found the std mapping
									// get the native map array of labels next
									cfNativeArrayRef = GetNativeTypeMapArray(typeMapDict);
									//now we need to determine for each array entry whether it is a string(searchbase)
									//or a dictionary(objectclass and searchbase)
									if (cfNativeArrayRef != nil)
									{
										//now we can retrieve each Native Type mapping to the given Standard type
										cfNativeMapCount = ::CFArrayGetCount( cfNativeArrayRef );
										//check here that we have a potential entry
										//ie. std type not nil and an entry in the native map array
										if (cfNativeMapCount != 0)
										{
											//get the inIndex 'th  Native Map
											if ( (inIndex >= 1) && (inIndex <= cfNativeMapCount) )
											{
												//assume that the std type extracted strings will be significantly less than 1024 characters
												tmpBuff = (char *) calloc(1, 1024);
			
												//determine whether the array entry is a string or a dictionary
												if (CFGetTypeID(CFArrayGetValueAtIndex( cfNativeArrayRef, inIndex-1 )) == CFStringGetTypeID())
												{
													CFStringRef	nativeMapString;
													nativeMapString = (CFStringRef)::CFArrayGetValueAtIndex( cfNativeArrayRef, inIndex-1 );
													if ( nativeMapString != nil )
													{
														if (CFStringGetCString(nativeMapString, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
														{
															outResult = (char *) calloc(1, 1+strlen(tmpBuff));
															::strcpy(outResult, tmpBuff);
														}
														//CFRelease(nativeMapString); // no since pointer only from Get
													}// if ( nativeMapString != nil )
													
													if ( outScope != nil )
													{
														*outScope = LDAP_SCOPE_SUBTREE;
													}

												}// array entry is a string ie. no ObjectClasses
												else //assume this is a dict since not a string
												{
													CFDictionaryRef subNativeDict;
													subNativeDict = (CFDictionaryRef)CFArrayGetValueAtIndex( cfNativeArrayRef, inIndex-1 );
													if (subNativeDict != nil)
													{
														if ( CFGetTypeID( subNativeDict ) == CFDictionaryGetTypeID() )
														{
															CFStringRef searchBase;
															searchBase = (CFStringRef)CFDictionaryGetValue( subNativeDict, CFSTR( kXMLSearchBase ) );
															if (searchBase != nil)
															{
																if ( CFGetTypeID( searchBase ) == CFStringGetTypeID() )
																{
																	::memset(tmpBuff,0,1024);
																	if (CFStringGetCString(searchBase, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
																	{
																		outResult = (char *) calloc(1, 1+strlen(tmpBuff));
																		::strcpy(outResult, tmpBuff);
																		
																		//now deal with the objectclass entries if appropriate
																		CFArrayRef objectClasses;
																		objectClasses = (CFArrayRef)CFDictionaryGetValue( subNativeDict, CFSTR( kXMLObjectClasses ) );
																		if ( (objectClasses != nil) && (outOCListCFArray != nil) && (outOCGroup != nil) )
																		{
																			if ( CFGetTypeID( objectClasses ) == CFArrayGetTypeID() )
																			{
																				*outOCGroup = 0;
																				CFStringRef groupOCString = nil;
																				groupOCString = (CFStringRef)CFDictionaryGetValue( subNativeDict, CFSTR( kXMLGroupObjectClasses ) );
																				if ( groupOCString != nil )
																				{
																					if ( CFGetTypeID( groupOCString ) == CFStringGetTypeID() )
																					{
																						if (CFStringCompare( groupOCString, CFSTR("AND"), 0 ) == kCFCompareEqualTo)
																						{
																							*outOCGroup = 1;
																						}
																					}
																				}
																				//make a copy of the CFArray of the objectClasses
																				*outOCListCFArray = CFArrayCreateCopy(kCFAllocatorDefault, objectClasses);
																			}// if ( CFGetTypeID( objectClasses ) == CFArrayGetTypeID() )
																		}// if (objectClasses != nil)
																	}// if (CFStringGetCString(searchBase, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
																}// if ( CFGetTypeID( searchBase ) == CFStringGetTypeID() )
															}
															if (outScope != nil)
															{
																cfBoolRef = (CFBooleanRef)CFDictionaryGetValue( subNativeDict, CFSTR( kXMLOneLevelSearchScope ) );
																if (cfBoolRef != nil)
																{
																	if (CFBooleanGetValue(cfBoolRef))
																	{
																		*outScope = LDAP_SCOPE_ONELEVEL;
																	}
																	else
																	{
																		*outScope = LDAP_SCOPE_SUBTREE;
																	}
																}
																else
																{
																	*outScope = LDAP_SCOPE_SUBTREE;
																}
															}
														}
													}
												}
												free(tmpBuff);
											}//get the correct indexed Native Map
											
										}// if (cfNativeMapCount != 0)
									}// if (cfNativeArrayRef != nil)
									//done so don't look for any more
									break;
								}
							}
							//CFRelease(cfStringRef); // no since pointer only from Get
						}
					}
					
					//CFRelease( typeMapDict ); // no since pointer only from Get
					
				}//if ( typeMapDict != nil )
				
			} // loop over std rec maps - break above takes us out of this loop
			
		} // if (cfMapCount != 0)
		
		CFRelease(cfRecTypeRef);
		
	} // if (inRecordTypeMapCFArray != nil) ie. an array of Record Maps exists
	
	return( outResult );

} // ExtractRecMap


// ---------------------------------------------------------------------------
//	* ExtractAttrMap
// ---------------------------------------------------------------------------

char* CLDAPv3Configs::ExtractAttrMap( const char *inRecType, const char *inAttrType, CFArrayRef inRecordTypeMapCFArray, CFArrayRef inAttrTypeMapCFArray, int inIndex )
{
	char				   *outResult				= nil;
	CFIndex					cfMapCount				= 0;
	sInt32					iMapIndex				= 0;
	CFStringRef				cfStringRef				= nil;
	CFStringRef				cfRecTypeRef			= nil;
	CFStringRef				cfAttrTypeRef			= nil;
	CFArrayRef				cfAttrMapArrayRef		= nil;
	bool					bNoRecSpecificAttrMap	= true;

	if ( (inRecordTypeMapCFArray != nil) && (inRecType != nil) && (inAttrType != nil) )
	{
		cfRecTypeRef	= CFStringCreateWithCString(kCFAllocatorDefault, inRecType, kCFStringEncodingUTF8);
		cfAttrTypeRef	= CFStringCreateWithCString(kCFAllocatorDefault, inAttrType, kCFStringEncodingUTF8);
		
		//now we can look for our Type mapping
		cfMapCount = ::CFArrayGetCount( inRecordTypeMapCFArray );
		if (cfMapCount != 0)
		{
			//loop through the maps
			for (iMapIndex = 0; iMapIndex < cfMapCount; iMapIndex++)
			{
				CFDictionaryRef		typeMapDict;
				typeMapDict = (CFDictionaryRef)::CFArrayGetValueAtIndex( inRecordTypeMapCFArray, iMapIndex );
				if ( typeMapDict != nil )
				{
					//retrieve the mappings
					// get the standard type label first
					if ( CFDictionaryContainsKey( typeMapDict, CFSTR( kXMLStdNameKey ) ) )
					{
						cfStringRef = (CFStringRef)CFDictionaryGetValue( typeMapDict, CFSTR( kXMLStdNameKey ) );
						if ( cfStringRef != nil )
						{
							if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
							{
								if (CFStringCompare(cfStringRef, cfRecTypeRef, 0) == kCFCompareEqualTo)
								{
									// found the std mapping
									// get the Attr map array for this std rec map
									cfAttrMapArrayRef = GetAttributeTypeMapArray(typeMapDict);
									outResult = ExtractAttrMapFromArray( cfAttrTypeRef, cfAttrMapArrayRef, inIndex, &bNoRecSpecificAttrMap );
									if (bNoRecSpecificAttrMap)
									{
										//here we search the COMMON attr maps if std attr type not found above
										outResult = ExtractAttrMapFromArray( cfAttrTypeRef, inAttrTypeMapCFArray, inIndex, &bNoRecSpecificAttrMap ); //here don't care about the return of bNoRecSpecificAttrMap
									}
									//done so don't look for any more
									break;
								}
							}
							//CFRelease(cfStringRef); // no since pointer only from Get
						}
					}
					//CFRelease( typeMapDict ); // no since pointer only from Get
				}//if ( typeMapDict != nil )
				
			} // loop over std rec maps - break above takes us out of this loop
			
		} // if (cfMapCount != 0)
		
		CFRelease(cfRecTypeRef);
		CFRelease(cfAttrTypeRef);
		
	} // if (inRecordTypeMapCFArray != nil) ie. an array of Record Maps exists
	
	return( outResult );

} // ExtractAttrMap


// ---------------------------------------------------------------------------
//	* ExtractAttrMapFromArray
// ---------------------------------------------------------------------------

char* CLDAPv3Configs::ExtractAttrMapFromArray( CFStringRef inAttrTypeRef, CFArrayRef inAttrTypeMapCFArray, int inIndex, bool *bNoRecSpecificAttrMap )
{
	char				   *outResult				= nil;
	CFIndex					cfAttrMapCount			= 0;
	CFIndex					cfNativeMapCount		= 0;
	sInt32					iAttrMapIndex			= 0;
	CFStringRef				cfAttrStringRef			= nil;
	char				   *tmpBuff					= nil;
	CFIndex					cfBuffSize				= 1024;
	CFArrayRef				cfNativeMapArrayRef		= nil;

	if ( (inAttrTypeRef != nil) && (inAttrTypeMapCFArray != nil) )
	{
		//now we search for the inAttrType
		cfAttrMapCount = ::CFArrayGetCount( inAttrTypeMapCFArray );
		//check here that we have a potential entry
		//ie. std type not nil and an entry in the native map array
		if (cfAttrMapCount != 0)
		{
			//loop through the Attr maps
			for (iAttrMapIndex = 0; iAttrMapIndex < cfAttrMapCount; iAttrMapIndex++)
			{
				CFDictionaryRef		typeAttrMapDict;
				typeAttrMapDict = (CFDictionaryRef)::CFArrayGetValueAtIndex( inAttrTypeMapCFArray, iAttrMapIndex );
				if ( typeAttrMapDict != nil )
				{
					//retrieve the mappings
					// get the standard Attr type label first
					if ( CFDictionaryContainsKey( typeAttrMapDict, CFSTR( kXMLStdNameKey ) ) )
					{
						cfAttrStringRef = (CFStringRef)CFDictionaryGetValue( typeAttrMapDict, CFSTR( kXMLStdNameKey ) );
						if ( cfAttrStringRef != nil )
						{
							if ( CFGetTypeID( cfAttrStringRef ) == CFStringGetTypeID() )
							{
								if (CFStringCompare(cfAttrStringRef, inAttrTypeRef, 0) == kCFCompareEqualTo)
								{
									*bNoRecSpecificAttrMap = false; //found a rec type map specific attr map
									
									// found the std Attr mapping
									// get the native map array for this std Attr map
									cfNativeMapArrayRef = GetNativeTypeMapArray(typeAttrMapDict);
									if (cfNativeMapArrayRef != nil)
									{
										//now we search for the inAttrType
										cfNativeMapCount = ::CFArrayGetCount( cfNativeMapArrayRef );
										
										if (cfNativeMapCount != 0)
										{
											//get the inIndex 'th  Native Map
											if ( (inIndex >= 1) && (inIndex <= cfNativeMapCount) )
											{
												//assume that the std type extracted strings will be significantly less than 1024 characters
												tmpBuff = (char *) calloc(1, 1024);
			
												//determine whether the array entry is a string
												if (CFGetTypeID(CFArrayGetValueAtIndex( cfNativeMapArrayRef, inIndex-1 )) == CFStringGetTypeID())
												{
													CFStringRef	nativeMapString;
													nativeMapString = (CFStringRef)::CFArrayGetValueAtIndex( cfNativeMapArrayRef, inIndex-1 );
													if ( nativeMapString != nil )
													{
														if (CFStringGetCString(nativeMapString, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
														{
															outResult = (char *) calloc(1, 1+strlen(tmpBuff));
															::strcpy(outResult, tmpBuff);
														}
														//CFRelease(nativeMapString); // no since pointer only from Get
													}// if ( nativeMapString != nil )
												}
												free(tmpBuff);
											}//get the correct indexed Native Map
										}// (cfNativeMapCount != 0)
									}// if (cfNativeMapArrayRef != nil)
									//done so don't look for any more
									break;
								}
							}
							//CFRelease(cfAttrStringRef); // no since pointer only from Get
						}
					}
					//CFRelease( typeAttrMapDict ); // no since pointer only from Get
				}//if ( typeAttrMapDict != nil )
			} //loop over the Attr maps
		}// if (cfAttrMapCount != 0)
	}// if (inAttrTypeMapCFArray != nil)

	return(outResult);
	
} // ExtractAttrMapFromArray


// ---------------------------------------------------------------------------
//	* ExtractStdAttr
// ---------------------------------------------------------------------------

char* CLDAPv3Configs::ExtractStdAttr( char *inRecType, CFArrayRef inRecordTypeMapCFArray, CFArrayRef inAttrTypeMapCFArray, int &inputIndex )
{
	char				   *outResult				= nil;
	CFIndex					cfMapCount				= 0;
	CFIndex					cfAttrMapCount			= 0;
	CFIndex					cfAttrMapCount2			= 0;
	sInt32					iMapIndex				= 0;
	CFStringRef				cfStringRef				= nil;
	CFStringRef				cfRecTypeRef			= nil;
	CFArrayRef				cfAttrMapArrayRef		= nil;
	bool					bUsedIndex				= false;
	char				   *tmpBuff					= nil;
	CFIndex					cfBuffSize				= 1024;
	int						inIndex					= inputIndex;

	if ( (inRecordTypeMapCFArray != nil) && (inRecType != nil) )
	{
		cfRecTypeRef	= CFStringCreateWithCString(kCFAllocatorDefault, inRecType, kCFStringEncodingUTF8);
		
		//now we can look for our Type mapping
		cfMapCount = ::CFArrayGetCount( inRecordTypeMapCFArray );
		if (cfMapCount != 0)
		{
			//loop through the maps
			for (iMapIndex = 0; iMapIndex < cfMapCount; iMapIndex++)
			{
				CFDictionaryRef		typeMapDict;
				typeMapDict = (CFDictionaryRef)::CFArrayGetValueAtIndex( inRecordTypeMapCFArray, iMapIndex );
				if ( typeMapDict != nil )
				{
					//retrieve the mappings
					// get the standard type label first
					if ( CFDictionaryContainsKey( typeMapDict, CFSTR( kXMLStdNameKey ) ) )
					{
						cfStringRef = (CFStringRef)CFDictionaryGetValue( typeMapDict, CFSTR( kXMLStdNameKey ) );
						if ( cfStringRef != nil )
						{
							if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
							{
								if (CFStringCompare(cfStringRef, cfRecTypeRef, 0) == kCFCompareEqualTo)
								{
									// found the std mapping
									// get the Attr map array for this std rec map
									cfAttrMapArrayRef = GetAttributeTypeMapArray(typeMapDict);
									if (cfAttrMapArrayRef != nil)
									{
										cfAttrMapCount = ::CFArrayGetCount( cfAttrMapArrayRef );
										if (cfAttrMapCount != 0)
										{
											//get the inIndex 'th  Native Map
											if ( (inIndex >= 1) && (inIndex <= cfAttrMapCount) )
											{
												bUsedIndex = true;
												//assume that the std type extracted strings will be significantly less than 1024 characters
												tmpBuff = (char *) calloc(1, 1024);
												
												//determine whether the array entry is a dict
												if (CFGetTypeID(CFArrayGetValueAtIndex( cfAttrMapArrayRef, inIndex-1 )) == CFDictionaryGetTypeID())
												{
													CFDictionaryRef	stdAttrTypeDict;
													stdAttrTypeDict = (CFDictionaryRef)CFArrayGetValueAtIndex( cfAttrMapArrayRef, inIndex-1 );
													if ( stdAttrTypeDict != nil )
													{
														if ( CFDictionaryContainsKey( stdAttrTypeDict, CFSTR( kXMLStdNameKey ) ) )
														{
															CFStringRef	attrMapString;
															attrMapString = (CFStringRef)CFDictionaryGetValue( stdAttrTypeDict, CFSTR( kXMLStdNameKey ) );
															if ( attrMapString != nil )
															{
																if (CFStringGetCString(attrMapString, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
																{
																	outResult = (char *) calloc(1, 1+strlen(tmpBuff));
																	::strcpy(outResult, tmpBuff);
																}
																//CFRelease(attrMapString); // no since pointer only from Get
															}// if ( attrMapString != nil )
														} //std attr name present
													} //std attr type dict present
												}
												free(tmpBuff);
											}//get the correct indexed Native Map
										}// (cfAttrMapCount != 0)
									}// if (cfAttrMapArrayRef != nil)
									
									while (!bUsedIndex)
									{
										bUsedIndex = true;
										if (inAttrTypeMapCFArray != nil)
										{
											CFIndex commonIndex = inIndex - cfAttrMapCount;
											cfAttrMapCount2 = ::CFArrayGetCount( inAttrTypeMapCFArray );
											if (cfAttrMapCount2 != 0)
											{
												//get the commonIndex 'th  Native Map
												if ( (commonIndex >= 1) && (commonIndex <= cfAttrMapCount2) )
												{
													//assume that the std type extracted strings will be significantly less than 1024 characters
													tmpBuff = (char *) calloc(1, 1024);
													
													//determine whether the array entry is a dict
													if (CFGetTypeID(CFArrayGetValueAtIndex( inAttrTypeMapCFArray, commonIndex-1 )) == CFDictionaryGetTypeID())
													{
														CFDictionaryRef	stdAttrTypeDict;
														stdAttrTypeDict = (CFDictionaryRef)CFArrayGetValueAtIndex( inAttrTypeMapCFArray, commonIndex-1 );
														if ( stdAttrTypeDict != nil )
														{
															if ( CFDictionaryContainsKey( stdAttrTypeDict, CFSTR( kXMLStdNameKey ) ) )
															{
																CFStringRef	attrMapString;
																attrMapString = (CFStringRef)CFDictionaryGetValue( stdAttrTypeDict, CFSTR( kXMLStdNameKey ) );
																if ( attrMapString != nil )
																{
																	bool bNoDuplicate = true;
																	//this is the Std Attr Name that we compare to if
																	//cfAttrMapCount is not zero ie. there were record specific attr maps that
																	//we do not wish to add to here
																	if ( (cfAttrMapArrayRef != NULL) && (cfAttrMapCount != 0) )
																	{
																		for (sInt32 aIndex = 0; aIndex < cfAttrMapCount; aIndex++)
																		{
																			//determine whether the array entry is a dict
																			if (CFGetTypeID(CFArrayGetValueAtIndex( cfAttrMapArrayRef, aIndex )) == CFDictionaryGetTypeID())
																			{
																				CFDictionaryRef	stdAttrTypeDict;
																				stdAttrTypeDict = (CFDictionaryRef)CFArrayGetValueAtIndex( cfAttrMapArrayRef, aIndex );
																				if ( stdAttrTypeDict != nil )
																				{
																					if ( CFDictionaryContainsKey( stdAttrTypeDict, CFSTR( kXMLStdNameKey ) ) )
																					{
																						CFStringRef	attrMapStringOld;
																						attrMapStringOld = (CFStringRef)CFDictionaryGetValue( stdAttrTypeDict, CFSTR( kXMLStdNameKey ) );
																						if ( attrMapStringOld != nil )
																						{
																							if (CFStringCompare(attrMapStringOld, attrMapString, 0) == kCFCompareEqualTo)
																							{
																								bNoDuplicate	= false;
																								bUsedIndex		= false;
																								inIndex++;
																								break;
																							}
																							//CFRelease(attrMapStringOld); // no since pointer only from Get
																						}// if ( attrMapStringOld != nil )
																					} //std attr name present
																				} //std attr type dict present
																			}
																		}//for (uInt32 aIndex = 0; aIndex < cfAttrMapCount; aIndex++)
																	}
																	if (bNoDuplicate)
																	{
																		if (CFStringGetCString(attrMapString, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
																		{
																			outResult = (char *) calloc(1, 1+strlen(tmpBuff));
																			::strcpy(outResult, tmpBuff);
																		}
																	}
																	//CFRelease(attrMapString); // no since pointer only from Get
																}// if ( attrMapString != nil )
															} //std attr name present
														} //std attr type dict present
													}
													free(tmpBuff);
												}//get the correct indexed Native Map
											}// (cfAttrMapCount2 != 0)
										}// if (inAttrTypeMapCFArray != nil)
									} //(!bUsedIndex)
									//done so don't look for any more
									break;
								}
							}
							//CFRelease(cfStringRef); // no since pointer only from Get
						}
					}
					//CFRelease( typeMapDict ); // no since pointer only from Get
				}//if ( typeMapDict != nil )
				
			} // loop over std rec maps - break above takes us out of this loop
			
		} // if (cfMapCount != 0)
		
		CFRelease(cfRecTypeRef);
		
	} // if (inRecordTypeMapCFArray != nil) ie. an array of Record Maps exists
	
	if (inIndex != inputIndex)
	{
		inputIndex = inIndex;
	}
	return( outResult );

} // ExtractStdAttr


// ---------------------------------------------------------------------------
//	* AttrMapsCount
// ---------------------------------------------------------------------------

int CLDAPv3Configs::AttrMapsCount( const char *inRecType, const char *inAttrType, CFArrayRef inRecordTypeMapCFArray, CFArrayRef inAttrTypeMapCFArray )
{
	int						outCount				= 0;
	CFIndex					cfMapCount				= 0;
	sInt32					iMapIndex				= 0;
	CFStringRef				cfStringRef				= nil;
	CFStringRef				cfRecTypeRef			= nil;
	CFStringRef				cfAttrTypeRef			= nil;
	CFArrayRef				cfAttrMapArrayRef		= nil;
	bool					bNoRecSpecificAttrMap	= true;

	if ( (inRecordTypeMapCFArray != nil) && (inRecType != nil) && (inAttrType != nil) )
	{
		cfRecTypeRef	= CFStringCreateWithCString(kCFAllocatorDefault, inRecType, kCFStringEncodingUTF8);
		cfAttrTypeRef	= CFStringCreateWithCString(kCFAllocatorDefault, inAttrType, kCFStringEncodingUTF8);
		
		//now we can look for our Type mapping
		cfMapCount = ::CFArrayGetCount( inRecordTypeMapCFArray );
		if (cfMapCount != 0)
		{
			//loop through the maps
			for (iMapIndex = 0; iMapIndex < cfMapCount; iMapIndex++)
			{
				CFDictionaryRef		typeMapDict;
				typeMapDict = (CFDictionaryRef)::CFArrayGetValueAtIndex( inRecordTypeMapCFArray, iMapIndex );
				if ( typeMapDict != nil )
				{
					//retrieve the mappings
					// get the standard type label first
					if ( CFDictionaryContainsKey( typeMapDict, CFSTR( kXMLStdNameKey ) ) )
					{
						cfStringRef = (CFStringRef)CFDictionaryGetValue( typeMapDict, CFSTR( kXMLStdNameKey ) );
						if ( cfStringRef != nil )
						{
							if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
							{
								if (CFStringCompare(cfStringRef, cfRecTypeRef, 0) == kCFCompareEqualTo)
								{
									// found the std mapping
									// get the Attr map array for this std rec map
									cfAttrMapArrayRef = GetAttributeTypeMapArray(typeMapDict);
									outCount = AttrMapFromArrayCount( cfAttrTypeRef, cfAttrMapArrayRef, &bNoRecSpecificAttrMap );
									if (bNoRecSpecificAttrMap)
									{
										//here we search the COMMON attr maps if std attr type not found above
										outCount = AttrMapFromArrayCount( cfAttrTypeRef, inAttrTypeMapCFArray, &bNoRecSpecificAttrMap ); //here don't care about the return of bNoRecSpecificAttrMap
									}
									//done so don't look for any more
									break;
								}
							}
							//CFRelease(cfStringRef); // no since pointer only from Get
						}
					}
					//CFRelease( typeMapDict ); // no since pointer only from Get
				}//if ( typeMapDict != nil )
				
			} // loop over std rec maps - break above takes us out of this loop
			
		} // if (cfMapCount != 0)
		
		CFRelease(cfRecTypeRef);
		CFRelease(cfAttrTypeRef);
		
	} // if (inRecordTypeMapCFArray != nil) ie. an array of Record Maps exists
	
	return( outCount );

} // AttrMapsCount


// ---------------------------------------------------------------------------
//	* AttrMapFromArrayCount
// ---------------------------------------------------------------------------

int CLDAPv3Configs::AttrMapFromArrayCount( CFStringRef inAttrTypeRef, CFArrayRef inAttrTypeMapCFArray, bool *bNoRecSpecificAttrMap )
{
	int						outCount				= 0;
	CFIndex					cfAttrMapCount			= 0;
	sInt32					iAttrMapIndex			= 0;
	CFStringRef				cfAttrStringRef			= nil;
	CFArrayRef				cfNativeMapArrayRef		= nil;

	if ( (inAttrTypeRef != nil) && (inAttrTypeMapCFArray != nil) )
	{
		//now we search for the inAttrType
		cfAttrMapCount = ::CFArrayGetCount( inAttrTypeMapCFArray );
		//check here that we have a potential entry
		//ie. std type not nil and an entry in the native map array
		if (cfAttrMapCount != 0)
		{
			//loop through the Attr maps
			for (iAttrMapIndex = 0; iAttrMapIndex < cfAttrMapCount; iAttrMapIndex++)
			{
				CFDictionaryRef		typeAttrMapDict;
				typeAttrMapDict = (CFDictionaryRef)::CFArrayGetValueAtIndex( inAttrTypeMapCFArray, iAttrMapIndex );
				if ( typeAttrMapDict != nil )
				{
					//retrieve the mappings
					// get the standard Attr type label first
					if ( CFDictionaryContainsKey( typeAttrMapDict, CFSTR( kXMLStdNameKey ) ) )
					{
						cfAttrStringRef = (CFStringRef)CFDictionaryGetValue( typeAttrMapDict, CFSTR( kXMLStdNameKey ) );
						if ( cfAttrStringRef != nil )
						{
							if ( CFGetTypeID( cfAttrStringRef ) == CFStringGetTypeID() )
							{
								if (CFStringCompare(cfAttrStringRef, inAttrTypeRef, 0) == kCFCompareEqualTo)
								{
									*bNoRecSpecificAttrMap = false; //found a rec type map specific attr map
									
									// found the std Attr mapping
									// get the native map array for this std Attr map
									cfNativeMapArrayRef = GetNativeTypeMapArray(typeAttrMapDict);
									if (cfNativeMapArrayRef != nil)
									{
										//now we search for the inAttrType
										outCount = ::CFArrayGetCount( cfNativeMapArrayRef );
									}// if (cfNativeMapArrayRef != nil)
									//done so don't look for any more
									break;
								}
							}
							//CFRelease(cfAttrStringRef); // no since pointer only from Get
						}
					}
					//CFRelease( typeAttrMapDict ); // no since pointer only from Get
				}//if ( typeAttrMapDict != nil )
			} //loop over the Attr maps
		}// if (cfAttrMapCount != 0)
	}// if (inAttrTypeMapCFArray != nil)

	return(outCount);
	
} // AttrMapFromArrayCount


void CLDAPv3Configs::XMLConfigLock( void )
{
	if (pXMLConfigLock != nil)
	{
		pXMLConfigLock->Wait();
	}
}

void CLDAPv3Configs::XMLConfigUnlock( void )
{
	if (pXMLConfigLock != nil)
	{
		pXMLConfigLock->Signal();
	}
}

// ---------------------------------------------------------------------------
//	* UpdateReplicaList
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Configs::UpdateReplicaList(char *inServerName, CFMutableArrayRef inReplicaHostnames, CFMutableArrayRef inWriteableHostnames)
{
	sInt32					siResult			= eDSNoErr;
	CFStringRef				errorString			= NULL;
	CFPropertyListRef		configPropertyList	= NULL;
	CFMutableDictionaryRef	configDict			= NULL;
	CFArrayRef				cfArrayRef			= NULL;
	CFIndex					cfConfigCount		= 0;
	CFDataRef				xmlData				= NULL;
	bool					bDoWrite			= false;

	if (fXMLData != nil)
	{
		// extract the config dictionary from the XML data.
		configPropertyList = CFPropertyListCreateFromXMLData( kCFAllocatorDefault,
					fXMLData,
					kCFPropertyListMutableContainersAndLeaves, //could also use kCFPropertyListImmutable, kCFPropertyListMutableContainers
					&errorString);
		
		if (configPropertyList != nil )
		{
			//make the propertylist a dict
			if ( CFDictionaryGetTypeID() == CFGetTypeID( configPropertyList ) )
			{
				configDict = (CFMutableDictionaryRef) configPropertyList;
			}
			
			if (configDict != nil)
			{
				//get array of LDAP server configs
				cfArrayRef = nil;
				cfArrayRef = GetConfigArray(configDict);
				if (cfArrayRef != nil)
				{
					//now we can retrieve the pertinent config
					cfConfigCount = ::CFArrayGetCount( cfArrayRef );
					//loop through the configs
					//use iConfigIndex for the access to the cfArrayRef
					for (sInt32 iConfigIndex = 0; iConfigIndex < cfConfigCount; iConfigIndex++)
					{
						CFMutableDictionaryRef serverDict	= nil;
						serverDict = (CFMutableDictionaryRef)::CFArrayGetValueAtIndex( cfArrayRef, iConfigIndex );
						if ( serverDict != nil )
						{
							CFStringRef aString = NULL;
							if ( CFDictionaryContainsKey( serverDict, CFSTR( kXMLServerKey ) ) )
							{
								aString = (CFStringRef)CFDictionaryGetValue( serverDict, CFSTR( kXMLServerKey ) );
								if ( aString != nil )
								{
									if ( CFGetTypeID( aString ) == CFStringGetTypeID() )
									{
										CFStringRef aServerName = CFStringCreateWithCString( NULL, inServerName, kCFStringEncodingUTF8 );
										if (CFStringCompare(aString, aServerName, 0) == kCFCompareEqualTo)
										{
											//now insert the new replica list
											//get the replica arrays to remove the old ones
											CFArrayRef cfRepArrayRef = NULL;
											cfRepArrayRef = GetReplicaHostnameListArray(serverDict);
											if (cfRepArrayRef != NULL)
											{
												CFDictionaryRemoveValue( serverDict, CFSTR( kXMLReplicaHostnameListArrayKey ) );
											}
											if( inReplicaHostnames )
											{
												CFDictionarySetValue( serverDict, CFSTR( kXMLReplicaHostnameListArrayKey ), (CFArrayRef)inReplicaHostnames );
											}
											bDoWrite = true;
											cfRepArrayRef = NULL;
											cfRepArrayRef = GetWriteableHostnameListArray(serverDict);
											if (cfRepArrayRef != NULL)
											{
												CFDictionaryRemoveValue( serverDict, CFSTR( kXMLWriteableHostnameListArrayKey ) );
											}
											if( inWriteableHostnames )
											{
												CFDictionarySetValue( serverDict, CFSTR( kXMLWriteableHostnameListArrayKey ), (CFArrayRef)inWriteableHostnames );
											}
											bDoWrite = true;
											if (bDoWrite)
											{
												//convert the dict into a XML blob
												xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, configDict);
												
												//replace the XML data blob
												siResult = SetXMLConfig(xmlData);
												
												//release this reference here
												CFRelease(xmlData);
												xmlData = nil;
																			
												//write the file out to save the change
												if (siResult == eDSNoErr)
												{
													WriteXMLConfig();
												}
											}
											CFRelease(aServerName);
											break; //found the correct server config and quit looking regardless if updated or not
										}
										CFRelease(aServerName);
									}
									//CFRelease(aString); // no since pointer only from Get
								}
							}
						}
					} // loop over configs
					
					//CFRelease( cfArrayRef ); // no since pointer only from Get
					
				} // if (cfArrayRef != nil) ie. an array of LDAP configs exists
				
				// don't release the configDict since it is the cast configPropertyList
			}//if (configDict != nil)
			
			CFRelease(configPropertyList); // built from Create on XML data so okay to dealloc here
			configPropertyList = nil;

		}//if (configPropertyList != nil )
	} // fXMLData != nil
		
	return( siResult );

} // UpdateReplicaList

// ---------------------------------------------------------------------------
//	* CreatePrefDirectory
// ---------------------------------------------------------------------------

bool CLDAPv3Configs::CreatePrefDirectory( void )
{
	char		*filenameString		= "/Library/Preferences/DirectoryService/DSLDAPv3PlugInConfig.plist";
	int			siResult			= eDSNoErr;
    struct stat statResult;
	
	DBGLOG( kLogPlugin, "Checking for LDAP XML config file:" );
	DBGLOG1( kLogPlugin, "%s", filenameString );
	
	//step 1- see if the file exists
	//if not then make sure the directories exist or create them
	//then create a new file if necessary
	siResult = ::stat( filenameString, &statResult );
	
	//if file does not exist
	if (siResult != eDSNoErr)
	{
		//move down the path from the system defined local directory and check if it exists
		//if not create it
		char *tempPath = "/Library/Preferences";
		siResult = ::stat( tempPath, &statResult );
		
		//if first sub directory does not exist
		if (siResult != eDSNoErr)
		{
			::mkdir( tempPath, 0775 );
			::chmod( tempPath, 0775 ); //above 0775 doesn't seem to work - looks like umask modifies it
		}
		
		//next subdirectory
		tempPath = "/Library/Preferences/DirectoryService";
		siResult = ::stat( tempPath, &statResult );
		//if second sub directory does not exist
		if (siResult != eDSNoErr)
		{
			::mkdir( tempPath, 0775 );
			::chmod( tempPath, 0775 ); //above 0775 doesn't seem to work - looks like umask modifies it
		}
	}
	
	return (siResult == eDSNoErr);
	
} //CreatePrefDirectory

// ---------------------------------------------------------------------------
//	* ConvertLDAPv2Config
// ---------------------------------------------------------------------------

bool CLDAPv3Configs::ConvertLDAPv2Config( void )
{
	struct stat				statResult;
	const char				*prefPath		= "/Library/Preferences/DirectoryService/DSLDAPPlugInConfig.clpi";
	bool					bReturn			= false;
	CFDataRef				sV2ConfigData	= NULL;
	CFMutableDictionaryRef  sV2Config		= NULL;
	CFMutableDictionaryRef  sV3Config		= NULL;
	
	// first let's see if the LDAPv2 Plugin does not exist before we try to convert the config.
	// if we have a path, and we can't stat anything, the plugin must not exist.
	if( stat("/System/Library/Frameworks/DirectoryService.framework/Resources/Plugins/LDAPv2.dsplug", &statResult) != 0 )
	{
		char		newName[PATH_MAX]	= { 0 };

		CFStringRef sPath = ::CFStringCreateWithCString( kCFAllocatorDefault, prefPath, kCFStringEncodingUTF8 );
		
		strcpy( newName, prefPath );
		strcat( newName, ".v3converted" );
	
		if( ::stat( prefPath, &statResult ) == 0 ) // must be a file...
		{
			// Convert it back into a CFURL.
			CFURLRef	sConfigFileURL   = ::CFURLCreateWithFileSystemPath( kCFAllocatorDefault, sPath, kCFURLPOSIXPathStyle, false );
			
			CFURLCreateDataAndPropertiesFromResource( kCFAllocatorDefault, sConfigFileURL, &sV2ConfigData, NULL, NULL, NULL );
	
			CFRelease( sConfigFileURL );
			sConfigFileURL = NULL;
	
			if( sV2ConfigData ) 
			{
				sV2Config = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, sV2ConfigData, kCFPropertyListMutableContainers, NULL );
				CFRelease( sV2ConfigData );
				sV2ConfigData = NULL;
			}
	
			sV3Config = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, fXMLData, kCFPropertyListMutableContainers, NULL );
	
			// if we have a sV2Config and a sV3Config
			if( sV2Config && sV3Config )
			{
				CFStringRef				tConfigKey			= CFSTR( kXMLConfigArrayKey );
				CFMutableArrayRef		tV3ConfigEntries	= (CFMutableArrayRef) CFDictionaryGetValue( sV3Config, tConfigKey );
				CFArrayRef				tV2ConfigEntries	= (CFArrayRef) CFDictionaryGetValue( sV2Config, tConfigKey );
				CFMutableDictionaryRef  tV2ConfigEntry		= NULL;
				
				if( tV2ConfigEntries )
				{
					CFIndex v2ConfigCount = CFArrayGetCount(tV2ConfigEntries);
					CFIndex v2ConfigIndex;
					
					for( v2ConfigIndex = 0; v2ConfigIndex < v2ConfigCount; v2ConfigIndex++ )
					{
						tV2ConfigEntry = (CFMutableDictionaryRef) CFArrayGetValueAtIndex( tV2ConfigEntries, v2ConfigIndex );
						
						if( tV2ConfigEntry )
						{
							// let's do the first value, if we have a hostname, let's make sure we don't already have one in V3 too.
							CFTypeRef   tObjectValue = CFDictionaryGetValue( tV2ConfigEntry, CFSTR(kXMLServerKey) );
							if( tObjectValue )
							{
								// if we have a current config...
								if( tV3ConfigEntries )
								{
									CFIndex		count = CFArrayGetCount( tV3ConfigEntries );
									CFIndex		index;
									
									for( index = 0; index < count; index++ )
									{
										CFDictionaryRef tServerConfig = (CFDictionaryRef) CFArrayGetValueAtIndex( tV3ConfigEntries, index );
										CFStringRef		tServer = (CFStringRef) CFDictionaryGetValue( tServerConfig, CFSTR(kXMLServerKey) );
										
										if( tServer && CFStringCompare(tServer, (CFStringRef) tObjectValue, kCFCompareCaseInsensitive) == kCFCompareEqualTo )
										{
											CFDictionarySetValue( tV2ConfigEntry, CFSTR(kXMLEnableUseFlagKey), kCFBooleanFalse );
										}
									}
								}
							}
							
							// Server Mappings
							CFDictionarySetValue( tV2ConfigEntry, CFSTR(kXMLServerMappingsFlagKey), kCFBooleanFalse );
							
							// default LDAP flag to false
							CFDictionarySetValue( tV2ConfigEntry, CFSTR(kXMLMakeDefLDAPFlagKey), kCFBooleanFalse );
							
							// UI Name - need to change it
							CFStringRef tKeyValue = CFSTR( kXMLUserDefinedNameKey );
							tObjectValue = CFDictionaryGetValue( tV2ConfigEntry, tKeyValue );
							if( tObjectValue )
							{
								CFStringRef sNewName = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%@ (from LDAPv2)"), tObjectValue );
								CFDictionarySetValue( tV2ConfigEntry, tKeyValue, sNewName );
								CFRelease( sNewName );
							}
							
							// now we need to convert each RecordType Map
							CFArrayRef  tRecMap = (CFArrayRef) CFDictionaryGetValue( tV2ConfigEntry, CFSTR(kXMLRecordTypeMapArrayKey) );
							if( tRecMap )
							{
								CFIndex		index;
								CFIndex		count   = CFArrayGetCount( tRecMap );
								
								for( index = 0; index < count; index++ )
								{
									CFMutableDictionaryRef  tRecordMapDict  = (CFMutableDictionaryRef) CFArrayGetValueAtIndex( tRecMap, index );
									CFMutableArrayRef		tNativeArray	= (CFMutableArrayRef) CFDictionaryGetValue( tRecordMapDict, CFSTR(kXMLNativeMapArrayKey) );
									
									// let's add a blank attribute map
									CFArrayRef  sBlankArray = CFArrayCreate( NULL, NULL, 0, &kCFTypeArrayCallBacks );
									CFDictionarySetValue( tRecordMapDict, CFSTR(kXMLAttrTypeMapArrayKey), sBlankArray );
									CFRelease( sBlankArray );
									
									// if we don't have a native array, let's create a blank array
									if( tNativeArray == NULL )
									{
										tNativeArray = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
										CFDictionarySetValue( tRecordMapDict, CFSTR(kXMLNativeMapArrayKey), tNativeArray );
										CFRelease( tNativeArray );
									}
									
									// new native dictionary to replace the old Array
									CFMutableDictionaryRef  sNewNativeDict   = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
									
									// we need to add Group Class to the list
									CFDictionarySetValue( sNewNativeDict, CFSTR(kXMLGroupObjectClasses), CFSTR("OR") );
									
									// the first element should be the old OU pointer, let's add it to the new dictionary
									if( CFArrayGetCount( tNativeArray ) > 0 )
									{
										CFDictionarySetValue( sNewNativeDict, CFSTR(kXMLSearchBase), CFArrayGetValueAtIndex(tNativeArray, 0) );
									}
									
									// Let's remove the previous values and add the new Dictionary in it's place
									CFArrayRemoveAllValues( tNativeArray );
									CFArrayAppendValue( tNativeArray, sNewNativeDict );
									
									CFRelease( sNewNativeDict );
								}
							}
							
							// if we didn't have any config entries, we need to create one to add it to
							if( tV3ConfigEntries == NULL )
							{
								tV3ConfigEntries = CFArrayCreateMutable( kCFAllocatorDefault, NULL, &kCFTypeArrayCallBacks);
								CFDictionarySetValue( sV3Config, tConfigKey, tV3ConfigEntries );
								CFRelease( tV3ConfigEntries );
							}
							
							// let's append the new config to the new list
							CFArrayAppendValue( tV3ConfigEntries, tV2ConfigEntry );
							
							// now let's add it to the current config
							if( fXMLData )
							{
								CFRelease( fXMLData );
								fXMLData = NULL;
							}
							
							fXMLData = (CFDataRef) CFPropertyListCreateXMLData( kCFAllocatorDefault, sV3Config );
							bReturn = true;
						}
					}
				}
			}
			
			// let's rename the old file to something so we don't convert again.
			rename( prefPath, newName );
		}
		
		CFRelease( sPath );
		sPath = NULL;
	}
			
	if( sV2Config )
	{
		CFRelease( sV2Config );
		sV2Config = NULL;
	}
	
	if( sV3Config )
	{
		CFRelease( sV3Config );
		sV3Config = NULL;
	}
	
	return bReturn;
} //ConvertLDAPv2Config
