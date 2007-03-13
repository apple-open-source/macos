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

#pragma mark Includes

#include <string.h>				//used for strcpy, etc.
#include <stdlib.h>				//used for malloc
#include <sys/types.h>
#include <sys/stat.h>			//used for mkdir and stat
#include <syslog.h>				//error logging

#include "GetMACAddress.h"
#include "CLDAPv3Configs.h"
#include "CLog.h"
#include "DSLDAPUtils.h"

#include "DirServices.h"
#include "DirServicesUtils.h"
#include "DirServicesConst.h"

#include <unistd.h>

#pragma mark -
#pragma mark Statics

LDAPConfigDataMap   CLDAPv3Configs::fConfigMap;
DSMutexSemaphore	CLDAPv3Configs::fConfigMapMutex;

#pragma mark -
#pragma mark sLDAPConfigData Struct Members

sLDAPConfigData::sLDAPConfigData(	char *inUIname, char *inNodeName,
									char *inServerName, int inOpenCloseTO,
									int inIdleTO, int inDelayRebindTry,
									int inSearchTO, int inPortNum,
									bool inUseSecure, char *inAccount,
									char *inPassword, char *inKerberosId,
									bool inMakeDefLDAP, bool inServerMappings,
									bool inIsSSL, char *inMapSearchBase,
									int inSecurityLevel, int inSecurityLevelLoc, bool inReferrals,
									bool inLDAPv2ReadOnly, bool inDNSReplicas )
{
	fUIName = (inUIname ? strdup(inUIname) : NULL );
	fNodeName = (inNodeName ? strdup(inNodeName) : NULL);
	fServerName = (inServerName ? strdup(inServerName) : NULL);
	
	fOpenCloseTimeout = inOpenCloseTO;
	fIdleTimeout = inIdleTO;
	fDelayRebindTry = inDelayRebindTry;
	
	fServerPassword = (inPassword ? strdup(inPassword) : NULL );

	fSearchTimeout = inSearchTO;

	fServerAccount = (inAccount ? strdup(inAccount) : NULL );

	fServerPort = inPortNum;
	bSecureUse = inUseSecure;
	bUseAsDefaultLDAP = inMakeDefLDAP;
	bGetServerMappings = bServerMappings = inServerMappings;
	bIsSSL = inIsSSL;
	bLDAPv2ReadOnly = inLDAPv2ReadOnly;
	bReferrals = inReferrals;
	fMapSearchBase = (inMapSearchBase ? strdup(inMapSearchBase) : NULL );
	fSecurityLevel = inSecurityLevel;
	fSecurityLevelLoc = inSecurityLevelLoc;
	
	fKerberosId = (inKerberosId ? strdup(inKerberosId) : NULL );
	
	// leave all other variables alone...
	bUpdated = true;

	fReplicaHosts = nil;
	fObjectClassSchema = nil;

	bBuildReplicaList = bGetSecuritySettings = true;
	bOCBuilt = bAvail = false;
	
	fReplicaHostnames = fWriteableHostnames = nil;
	fRecordAttrMapDict = NULL;
	fRecordTypeMapCFArray = nil;
	fAttrTypeMapCFArray = nil;
	fSASLmethods = nil;
	
	fConfigLock = new DSMutexSemaphore();		

	fRefCount = 0;
	bMarkToDelete = false;
	bDNSReplicas = inDNSReplicas;
}

sLDAPConfigData::sLDAPConfigData( void )
{
	fNodeName = fUIName = fServerName = NULL;
	
	fOpenCloseTimeout = kLDAPDefaultOpenCloseTimeoutInSeconds;
	fIdleTimeout = 2;
	fDelayRebindTry = kLDAPDefaultRebindTryTimeoutInSeconds;
	fSearchTimeout = kLDAPDefaultSearchTimeoutInSeconds;

	fKerberosId = fServerPassword = fServerAccount = NULL;

	fServerPort = LDAP_PORT;
	bSecureUse = bUseAsDefaultLDAP = bServerMappings = bIsSSL = bLDAPv2ReadOnly = false;
	fSecurityLevel = fSecurityLevelLoc = kSecNoSecurity;
	
	bUpdated = true;
	
	fMapSearchBase = NULL;
	
	fReplicaHosts = nil;
	fObjectClassSchema = nil;
	
	bReferrals = bBuildReplicaList = bGetSecuritySettings = true;
	bGetServerMappings = bOCBuilt = bAvail = false;
	
	fReplicaHostnames = fWriteableHostnames = nil;
	fRecordAttrMapDict = NULL;
	fRecordTypeMapCFArray = nil;
	fAttrTypeMapCFArray = nil;
	fSASLmethods = nil;
	
	fConfigLock = new DSMutexSemaphore();
	
	fRefCount = 0;
	bMarkToDelete = false;
	bDNSReplicas = false;
}
		
// give sLDAPConfigData a destructor
sLDAPConfigData::~sLDAPConfigData(void)
{
	// we should wait if this config is in use... until we can get a lock...
	fConfigLock->Wait();
	
	DSDelete( fUIName );
	DSDelete( fNodeName );
	DSDelete( fServerName );
	DSDelete( fReplicaHosts );
	DSCFRelease( fReplicaHostnames );
	DSCFRelease( fWriteableHostnames );
	DSDelete( fServerPassword );
	DSDelete( fServerAccount );
	
	if (fObjectClassSchema != nil)
	{
		ObjectClassMapCI iter = fObjectClassSchema->begin();
		
		while( iter != fObjectClassSchema->end() )
		{
			//need this since we have a structure here and not a class
			iter->second->fParentOCs.clear();
			iter->second->fOtherNames.clear();
			iter->second->fRequiredAttrs.clear();
			iter->second->fAllowedAttrs.clear();
			
			delete iter->second;
			
			iter++;
		}
		fObjectClassSchema->clear();

		DSDelete( fObjectClassSchema );
	}
	
	DSCFRelease( fRecordAttrMapDict );
	DSCFRelease( fRecordTypeMapCFArray );
	DSCFRelease( fAttrTypeMapCFArray );
	DSDelete( fMapSearchBase );
	DSCFRelease( fSASLmethods );
	DSDelete( fKerberosId );
	DSDelete( fConfigLock );
}

#pragma mark -
#pragma mark CLDAPv3Configs Class

// --------------------------------------------------------------------------------
//	* CLDAPv3Configs
// --------------------------------------------------------------------------------

CLDAPv3Configs::CLDAPv3Configs ( void )
{
	fXMLData				= nil;
	pXMLConfigLock			= new DSMutexSemaphore();
} // CLDAPv3Configs


// --------------------------------------------------------------------------------
//	* ~CLDAPv3Configs ()
// --------------------------------------------------------------------------------

CLDAPv3Configs::~CLDAPv3Configs ( void )
{
	// let's clear the map
	fConfigMapMutex.Wait();
	
	LDAPConfigDataMapI  configIter = fConfigMap.begin();
	while( configIter != fConfigMap.end() )
	{
		DSDelete( configIter->second );
		configIter++;
	}
	fConfigMap.clear();
	
	fConfigMapMutex.Signal();
	
	DSDelete( pXMLConfigLock );
	DSCFRelease( fXMLData );

} // ~CLDAPv3Configs


// --------------------------------------------------------------------------------
//	* Init (CPlugInRef, uInt32)
// --------------------------------------------------------------------------------

sInt32 CLDAPv3Configs::Init( void )
{
	sInt32				siResult	= eDSNoErr;
	sLDAPConfigData	   *pConfig		= nil;

	XMLConfigLock();
	
	DSCFRelease( fXMLData );

	//read the XML Config file
	siResult = ReadXMLConfig();
	
	XMLConfigUnlock();

	//check if XML file was read
	if (siResult == eDSNoErr)
	{
		//need to set the Updated flag to false so that nodes will get Unregistered
		//if a config no longer exists for that entry
		//this needs to be done AFTER it is verified that a XML config file exists

		fConfigMapMutex.Wait();
		
		LDAPConfigDataMapI configIter = fConfigMap.begin();
		
		//need to cycle through the config table
		while( configIter != fConfigMap.end() )
		{
			pConfig = configIter->second;
			if (pConfig != nil)
			{
				pConfig->bUpdated = false;
			}
			configIter++;
		}
		
		fConfigMapMutex.Signal();
	
		//set up the config table
		siResult = ConfigLDAPServers();
	}
	
	return( siResult );

} // Init

#pragma mark -
#pragma mark Config File Operations

// ---------------------------------------------------------------------------
//	* CreatePrefFilename
// ---------------------------------------------------------------------------

char *CLDAPv3Configs::CreatePrefFilename( void )
{
	char		*filenameString = NULL;
	CFStringRef cfENetAddr		= NULL;

	// this routine is used during reading and writing to ensure we use a specific config for this
	// computer if it exists
	
	GetMACAddress( &cfENetAddr, NULL, false );

	if( cfENetAddr )
	{
		uInt32 linkAddrLen = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfENetAddr), kCFStringEncodingUTF8) + 1;
		char	*filenameBase   = "/Library/Preferences/DirectoryService/DSLDAPv3PlugInConfig.";
		int		baseLen			= strlen( filenameBase );
		char	*tempFilename   = (char *) calloc( baseLen + linkAddrLen + 6 + 1, 1 ); // ".plist" + NULL
		struct stat	statResult;
		
		// faster than doing strcpy since strcpy calls bcopy
		bcopy( filenameBase, tempFilename, baseLen );
		CFStringGetCString( cfENetAddr, tempFilename + baseLen, linkAddrLen+1, kCFStringEncodingUTF8 );
		strcat( tempFilename, ".plist" );
		
		DSCFRelease(cfENetAddr);
		if( ::stat(tempFilename, &statResult) == 0 )
		{
			filenameString = tempFilename;
		}
		else
		{
			DBGLOG1( kLogPlugin, "CLDAPv3Configs: Could not find a computer specific configuration file %s", tempFilename );
			DSFreeString(tempFilename);
		}
	}

	return ( filenameString ? filenameString : strdup("/Library/Preferences/DirectoryService/DSLDAPv3PlugInConfig.plist") );
} //CreatePrefFilename

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
	char				   *filenameString			= CreatePrefFilename();
	CFStringRef				sPath					= NULL;

//Config data is read from a XML file
//KW eventually use Version from XML file to check against the code here?
//Steps in the process:
//1- see if the file exists
//2- if it exists then try to read it
//3- if existing file is corrupted then rename it and save it while creating a new default file
//4- if file doesn't exist then create a new default file - make sure directories exist/if not create them
	
	//step 1- see if the file exists
	//if not then make sure the directories exist or create them
	//then write the file
	siResult = ::stat( filenameString, &statResult );
	
	sPath = CFStringCreateWithCString( kCFAllocatorDefault, filenameString, kCFStringEncodingUTF8 );
	if (sPath != NULL)
	{
		//create URL always
		configFileURL = ::CFURLCreateWithFileSystemPath( kCFAllocatorDefault, sPath, kCFURLPOSIXPathStyle, false );
		DSCFRelease( sPath );
		
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
			
			DBGLOG( kLogPlugin, "CLDAPv3Configs: Created a new LDAP XML config file since it did not exist" );
			//convert the dict into a XML blob
			xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, configDict);
			
			if ( (configFileURL != NULL) && (xmlData != NULL) )
			{
				//write the XML to the config file
				siResult = CFURLWriteDataAndPropertiesToResource(	configFileURL,
																	xmlData,
																	NULL,
																	&errorCode);
			}
			
			DSCFRelease(configDict);
			DSCFRelease(xmlData);
			
		} // file does not exist so creating one
		
		if ( (siResult == eDSNoErr) && (configFileURL != NULL) ) //either stat or new write was successful
		{
			chmod( filenameString, S_IRUSR | S_IWUSR );
			// Read the XML property list file
			bReadFile = CFURLCreateDataAndPropertiesFromResource(	kCFAllocatorDefault,
																	configFileURL,
																	&xmlData,          // place to put file data
																	NULL,           
																	NULL,
																	&siResult);
		}
	} // if (sPath != NULL)
	
	if (bReadFile)
	{
		fXMLData = xmlData;
		//check if this XML blob is a property list and can be made into a dictionary
		if (!VerifyXML())
		{
			char	*corruptPath = "/Library/Preferences/DirectoryService/DSLDAPv3PlugInConfigCorrupted.plist";
			
			//if it is not then say the file is corrupted and save off the corrupted file
			DBGLOG( kLogPlugin, "CLDAPv3Configs: LDAP XML config file is corrupted" );
			bCorruptedFile = true;
			//here we need to make a backup of the file - why? - because

			// Append the subpath.
			sCorruptedPath = ::CFStringCreateWithCString( kCFAllocatorDefault, corruptPath, kCFStringEncodingUTF8 );

			if (sCorruptedPath != NULL)
			{
				// Convert it into a CFURL.
				configFileCorruptedURL = ::CFURLCreateWithFileSystemPath( kCFAllocatorDefault, sCorruptedPath, kCFURLPOSIXPathStyle, false );
				DSCFRelease( sCorruptedPath ); // build with Create so okay to dealloac here
				if (configFileCorruptedURL != NULL)
				{
					//write the XML to the corrupted copy of the config file
					bWroteFile = CFURLWriteDataAndPropertiesToResource( configFileCorruptedURL,
																		xmlData,
																		NULL,
																		&errorCode);
					if (bWroteFile)
					{
						chmod( corruptPath, S_IRUSR | S_IWUSR );
					}
				}
			}
			
			DSCFRelease(xmlData);
		}
	}
	else //existing file is unreadable
	{
		DBGLOG( kLogPlugin, "CLDAPv3Configs: LDAP XML config file is unreadable" );
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
		
		DBGLOG( kLogPlugin, "CLDAPv3Configs: Writing a new LDAP XML config file" );
		//convert the dict into a XML blob
		xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, configDict);
		
		//assume that the XML blob is good since we created it here
		fXMLData = xmlData;

		if ( (configFileURL != NULL) && (xmlData != NULL) )
		{
			//write the XML to the config file
			siResult = CFURLWriteDataAndPropertiesToResource( configFileURL,
																xmlData,
																NULL,
																&errorCode);
			if (siResult == eDSNoErr)
			{
				chmod( filenameString, S_IRUSR | S_IWUSR );
			}
		}
		
		DSCFRelease(configDict);
	}
	
	// if we have a config now, let's convert let's look to see if there is a sV2Config to convert
	if( fXMLData != NULL )
	{
		// if we converted....
		if( ConvertLDAPv2Config() )
		{
			//write the XML to the config file
			siResult = CFURLWriteDataAndPropertiesToResource( configFileURL,
													 fXMLData,
													 NULL,
													 &errorCode);
			if (siResult == eDSNoErr)
			{
				chmod( filenameString, S_IRUSR | S_IWUSR );
			}
		}
	}
		
	DSCFRelease(configFileURL); // seems okay to dealloc since Create used and done with it now
    
	DSCFRelease(configFileCorruptedURL); // seems okay to dealloc since Create used and done with it now
	
	DSFreeString( filenameString );
    
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
	struct stat				statResult;
	sInt32					errorCode			= 0;
	char				   *filenameString		= CreatePrefFilename();

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
		
		XMLConfigLock();
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
		XMLConfigUnlock();

		CFRelease(configFileURL); // seems okay to dealloc since Create used and done with it now
		configFileURL = nil;
	} // while (( iPath-- ) && (!bWroteFile))

	if (bWroteFile)
	{
		DBGLOG( kLogPlugin, "CLDAPv3Configs: Have written the LDAP XML config file:" );
		DBGLOG1( kLogPlugin, "CLDAPv3Configs:   %s", filenameString );
		siResult = eDSNoErr;
	}
	else
	{
		DBGLOG( kLogPlugin, "CLDAPv3Configs: LDAP XML config file has NOT been written" );
		DBGLOG( kLogPlugin, "CLDAPv3Configs: Update to LDAP Config File Failed" );
		siResult = eDSPlugInConfigFileError;
	}
	
	DSFreeString( filenameString );
		
	return( siResult );

} // WriteXMLConfig

// ---------------------------------------------------------------------------
//	* SetXMLConfig
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Configs::SetXMLConfig ( CFDataRef xmlData )
{
	CFDataRef   currentXMLData  = NULL;
	sInt32		siResult		= eDSInvalidPlugInConfigData;

// this should be an atomic set and write wrt the fXMLData attribute and the XML mutex
	XMLConfigLock();
	
	//let us check if we even need to write this ie. has it changed?
	
	currentXMLData  = fXMLData;
	
	// we need to retain here because VerifyXML could change the data and release this copy
	CFRetain( xmlData );
	
	fXMLData = xmlData;

	if (VerifyXML())
	{
		DSCFRelease( currentXMLData );

		siResult = WriteXMLConfig();
	}
	else
	{
		// need to release it here cause we didn't use it
		CFRelease( xmlData ); 

		// go back to what we had
		fXMLData = currentXMLData;
	}

	XMLConfigUnlock();

	return(siResult);
}// SetXMLConfig


// ---------------------------------------------------------------------------
//	* CopyXMLConfig
// ---------------------------------------------------------------------------

CFDataRef CLDAPv3Configs::CopyXMLConfig ( void )
{
	CFDataRef				combinedConfigDataRef	= NULL;
	CFStringRef				errorString				= NULL;
	CFMutableDictionaryRef	configDict				= NULL;
	CFArrayRef				configArray				= NULL;
	CFIndex					configArrayCount		= 0;
	CFMutableArrayRef		dhcpConfigArray			= NULL;
    sLDAPConfigData*		pConfig					= nil;

	// Object is to loop over our pConfigTable and see if we have any DHCP entries.
	// If we do, we want to incorporate them into the user defined config data.
	// If not, we will just retain fXMLData and return that.

	fConfigMapMutex.Wait();
	
	LDAPConfigDataMapI configIter = fConfigMap.begin();

	//need to cycle through the config table
	while( configIter != fConfigMap.end() )
	{
		pConfig = configIter->second;
		if (pConfig != nil && pConfig->bUseAsDefaultLDAP ) // is the current configuration possibly from DHCP? (Need to check against fXMLData table too)
		{
			bool			isCurrentConfInXMLData = false;
			CFStringRef		curConfigServerName = CFStringCreateWithCString( NULL, pConfig->fServerName, kCFStringEncodingUTF8 );
			
			if ( configDict == NULL )
			{
				XMLConfigLock();
				configDict = (CFMutableDictionaryRef)CFPropertyListCreateFromXMLData(	kCFAllocatorDefault,
																						fXMLData,
																						kCFPropertyListMutableContainers,	// we want this mutable so we can add DHCP services
																						NULL);
				XMLConfigUnlock();
				if ( configDict == NULL )
				{
					char	errBuf[1024];
					CFStringGetCString( errorString, errBuf, sizeof(errBuf), kCFStringEncodingUTF8 );
					syslog(LOG_ALERT,"DSLDAPv3PlugIn: [%s] LDAP server config could not be read.", errBuf);
					
					CFRelease( curConfigServerName );
					curConfigServerName = NULL;						
					break;
				}
				
				if ( CFDictionaryGetTypeID() != CFGetTypeID( configDict ) )
				{
					syslog(LOG_ALERT,"DSLDAPv3PlugIn: LDAP server config could not be read as it was not in the correct format!");
					
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
				CFStringRef					curConfigUIName = CFStringCreateWithCString( NULL, pConfig->fUIName, kCFStringEncodingUTF8  );
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
				CFDictionaryAddValue( curConfigDict, CFSTR(kXMLLDAPv2ReadOnlyKey), (pConfig->bLDAPv2ReadOnly)?kCFBooleanTrue:kCFBooleanFalse );
				CFDictionaryAddValue( curConfigDict, CFSTR(kXMLMakeDefLDAPFlagKey), kCFBooleanTrue );
				CFDictionaryAddValue( curConfigDict, CFSTR(kXMLEnableUseFlagKey), kCFBooleanTrue );
				CFDictionaryAddValue( curConfigDict, CFSTR(kXMLServerMappingsFlagKey), (pConfig->bServerMappings)?kCFBooleanTrue:kCFBooleanFalse );
				CFDictionaryAddValue( curConfigDict, CFSTR(kXMLReferralFlagKey), (pConfig->bReferrals)?kCFBooleanTrue:kCFBooleanFalse );
				
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
		
		configIter++;
	}
	
	fConfigMapMutex.Signal();
	
	if ( dhcpConfigArray == NULL )
	{
		XMLConfigLock();
		//don't need to make an actual copy here since we do a retain
		combinedConfigDataRef = fXMLData;
		CFRetain( combinedConfigDataRef );
		XMLConfigUnlock();
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
} //CopyXMLConfig


// ---------------------------------------------------------------------------
//	* VerifyXML
// ---------------------------------------------------------------------------

bool CLDAPv3Configs::VerifyXML ( void )
{
    bool						verified		= false;
    CFMutableDictionaryRef		configPropertyList;
//    char				   *configVersion		= nil;
//KW need to add in check on the version string

	XMLConfigLock();
	
    if (fXMLData != nil)
    {
        // extract the config dictionary from the XML data.
        configPropertyList = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault,
                                fXMLData,
                                kCFPropertyListMutableContainersAndLeaves, 
                                NULL);
        if (configPropertyList != nil )
        {
			bool	bUpdated = false;
			
            //make the propertylist a dict
            if ( CFDictionaryGetTypeID() == CFGetTypeID( configPropertyList ) )
            {
				verified = true;
				
				// let's verify individually added items exist with defaults
				// so people can change the file
				CFArrayRef	cfServerList = (CFArrayRef) CFDictionaryGetValue( configPropertyList, CFSTR(kXMLConfigArrayKey) );
				
				if( cfServerList != NULL && CFGetTypeID(cfServerList) == CFArrayGetTypeID() )
				{
					CFIndex	iCount = CFArrayGetCount( cfServerList );
					
					for( CFIndex ii = 0; ii < iCount; ii++ )
					{
						CFMutableDictionaryRef	cfConfig = (CFMutableDictionaryRef) CFArrayGetValueAtIndex( cfServerList, ii );
						
						// ensure it is a Dictionary so we can add keys as necessary to the configuration
						if( CFGetTypeID(cfConfig) == CFDictionaryGetTypeID() )
						{
							// look for the DNSReplica key in the configuration
							if( CFDictionaryContainsKey( cfConfig, CFSTR(kXMLUseDNSReplicasFlagKey) ) == false )
							{
								CFDictionarySetValue( cfConfig, CFSTR(kXMLUseDNSReplicasFlagKey), kCFBooleanFalse );
								bUpdated = true;
							}
						}
						else
						{
							// if it was not a dictionary, then this isn't a valid configuration
							verified = false;
						}
					}
				}
            }

			// if the dictionary was updated, then we need to make the XML back to data and replace the value there.
			if( bUpdated && verified )
			{
				CFDataRef	xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, configPropertyList );
				
				if( xmlData != NULL )
				{
					CFRelease( fXMLData );
					fXMLData = xmlData;
				}
			}
            DSCFRelease(configPropertyList);
        }
    }
    
	XMLConfigUnlock();
	
    return( verified );
    
} // VerifyXML

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

void CLDAPv3Configs::VerifyKerberosForRealm( char *inRealmName, char *inServer )
{
	// This will ensure the requested realm exists in the kerberos file, if not add it
	struct stat	statResult;
	char	*pExistingConfig	= NULL;
	char	pServer[255] = { 0, };
	
	// let's resolve the incoming name into a host name in case it is a dotted ip address
	hostent *hostEntry = gethostbyname( inServer );
	if ( hostEntry != NULL )
	{
		hostEntry = gethostbyaddr( hostEntry->h_addr_list[0], hostEntry->h_length, hostEntry->h_addrtype );
		if ( hostEntry != NULL )
		{
			// we only continue if we have a name, but let's copy in case someone calls gethostbyaddr
			strlcpy( pServer, hostEntry->h_name, 255 );
	
	// check to see if the file exists and it's size, so we can read it
	if ( stat("/Library/Preferences/edu.mit.Kerberos", &statResult) == 0 )
	{
		FILE *pFile = fopen( "/Library/Preferences/edu.mit.Kerberos", "r" );
		if (pFile != NULL)
		{
			pExistingConfig = (char *) calloc( sizeof(char), statResult.st_size + 768);
			fread( pExistingConfig, statResult.st_size, sizeof(char), pFile );
			fclose( pFile );
		}
	}
	
	// see if we are missing the realm in the file and fix it
	if ( pExistingConfig != NULL && strstr(pExistingConfig, inRealmName) == NULL )
	{
		char *pRealmSection = strstr(pExistingConfig, "[realms]");
		if ( pRealmSection != NULL )
		{
			char	newSection[512] = { 0, };
			int		newSectionLen	= 0;
			
					snprintf( newSection, 512, "\t%s = {\n\t\tkdc = %s\n\t}\n", inRealmName, pServer );
			newSectionLen = strlen( newSection );

			// move past the end-of-line
			pRealmSection = strchr( pRealmSection, '\n' ) + 1;
			
			// now insert our stuff here, cause it's easier
			bcopy( pRealmSection, pRealmSection + newSectionLen, strlen(pRealmSection) );
			bcopy( newSection, pRealmSection, newSectionLen );
			
			// look for [domain_realm] section and add this server to it so we don't have a realm mixture issue..
			char *pDomRealmSection = strstr(pExistingConfig, "[domain_realm]");
			if ( pDomRealmSection != NULL )
			{
				char	newSection2[256] = { 0, };
				int		newSection2Len	= 0;
				
				// move past the end-of-line
				pDomRealmSection = strchr( pDomRealmSection, '\n' ) + 1;
						snprintf( newSection2, 256, "\t%s = %s\n", pServer, inRealmName );				
				newSection2Len = strlen( newSection2 );

				// now insert our stuff here, cause it's easier
				bcopy( pDomRealmSection, pDomRealmSection + newSection2Len, strlen(pDomRealmSection) );
				bcopy( newSection2, pDomRealmSection, newSection2Len );
			}
			else
			{
				strcat( pExistingConfig, "[domain_realm]\n\t" );
						strcat( pExistingConfig, pServer );
				strcat( pExistingConfig, " = " );
				strcat( pExistingConfig, inRealmName );
				strcat( pExistingConfig, "\n" );
			}
			
			// now write the file out..
			FILE *pFile = fopen( "/Library/Preferences/edu.mit.Kerberos", "w" );
			if( pFile != NULL )
			{
				fwrite( pExistingConfig, 1, strlen(pExistingConfig), pFile );
				fclose( pFile );
				DBGLOG1( kLogPlugin, "CLDAPv3Configs: Updated edu.mit.Kerberos file with realm %s.", inRealmName );
			}
			else
			{
				syslog( LOG_INFO, "CLDAPv3Configs: Unable to update edu.mit.Kerberos file with realm %s.", inRealmName );
				DBGLOG1( kLogPlugin, "CLDAPv3Configs: Unable to update edu.mit.Kerberos file with realm %s.", inRealmName );
			}
		}
		else
		{
			// file doesn't have a realm section, must not be valid file
			DBGLOG1( kLogPlugin, "CLDAPv3Configs: edu.mit.Kerberos does not contain [realms] section.", inRealmName );
			DSFreeString( pExistingConfig ); // free so kerberosautoconfig is launched
		}
	}
	
	// if we don't have a configuration file, or it was an invalid configuraiton file, create one
	if ( pExistingConfig == NULL )
	{
		// just run kerberosautoconfig
		register int childPID = -1;
		int nStatus;
		
		DBGLOG1( kLogPlugin, "CLDAPv3Configs: Launching kerberosautoconfig because %s realm is missing.", inRealmName );
		switch ( childPID = ::fork() )
		{
			case -1:
				syslog( LOG_INFO, "CLDAPv3Configs: Couldn't launch kerberosautoconfig fork failed." );
				DBGLOG( kLogPlugin, "CLDAPv3Configs: Couldn't launch kerberosautoconfig fork failed." );
				break;
			case 0:
				// in child
						execl( "/sbin/kerberosautoconfig", "kerberosautoconfig", "-r", inRealmName, "-m", pServer, NULL );
				_exit(0);
				break;
			default:
				// wait until the child goes away
				while( ::waitpid( childPID, &nStatus, 0 ) == -1 && errno != ECHILD );
				break;
		}
	}
		}
	}
	DSFreeString( pExistingConfig );
}

#pragma mark -
#pragma mark Config Map Access Routines

// ---------------------------------------------------------------------------
//	* DeleteConfigFromMap
// ---------------------------------------------------------------------------

void CLDAPv3Configs::DeleteConfigFromMap( char *inConfigNodename )
{
	sLDAPConfigData    *pConfig			= nil;
	bool				bDeleteConfig   = false;

	if (inConfigNodename == NULL) return;
	
	fConfigMapMutex.Wait();
	
	LDAPConfigDataMapI  configIter = fConfigMap.find( string(inConfigNodename) );

	if( configIter != fConfigMap.end() )
	{
		pConfig = configIter->second;
		
		//remove it from the map always here
		fConfigMap.erase( string(inConfigNodename) );
		if (pConfig->fRefCount <= 0)
		{
			// ref count is zero implying no one else is using it
			bDeleteConfig = true;
		}
		else
		{
			pConfig->bMarkToDelete = true;
		}
	}
	fConfigMapMutex.Signal();

	if ( bDeleteConfig )
	{
		DSDelete( pConfig );
	}

}// DeleteConfigFromMap

// ---------------------------------------------------------------------------
//	* ConfigWithNodeNameLock
// ---------------------------------------------------------------------------

sLDAPConfigData *CLDAPv3Configs::ConfigWithNodeNameLock( char *inConfigName )
{
	// we lock the config in case we are modifying when someone is trying to use a config...
	// collisions are still possible since the config can still be updated while in use
	sLDAPConfigData		*pConfig = nil;
	
	if( inConfigName )
	{
		fConfigMapMutex.Wait();
		
		LDAPConfigDataMapI  configIter = fConfigMap.find( string(inConfigName) );

		if( configIter != fConfigMap.end() )
		{
			pConfig = configIter->second;
			
			pConfig->fRefCount++;
		}
		
		fConfigMapMutex.Signal();
		
		if (pConfig != NULL)
		{
			pConfig->fConfigLock->Wait();
		}
	}
	
	return pConfig;
} // ConfigWithNodeNameLock

// ---------------------------------------------------------------------------
//	* ConfigUnlock
// ---------------------------------------------------------------------------

void CLDAPv3Configs::ConfigUnlock( sLDAPConfigData *inConfig )
{
	bool				bDeleteConfig   = false;

	if (inConfig != NULL)
	{
		fConfigMapMutex.Wait();
		
		inConfig->fRefCount--;
		if ( (inConfig->bMarkToDelete) && (inConfig->fRefCount <= 0) )
		{
			// remove it now since marked to delete and ref count is zero implying no one else is using it
			//erase likely redundant here
			fConfigMap.erase( string(inConfig->fNodeName) );
			bDeleteConfig = true;
		}
		fConfigMapMutex.Signal();

		inConfig->fConfigLock->Signal();
		
		if ( bDeleteConfig )
		{
			DSDelete( inConfig );
		}
	}
}// ConfigUnlock

#pragma mark -
#pragma mark Updating Configuration

// ---------------------------------------------------------------------------
//	* UpdateReplicaList
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Configs::UpdateReplicaList(char *inServerName, CFMutableArrayRef inReplicaHostnames, CFMutableArrayRef inWriteableHostnames)
{
	sInt32					siResult			= eDSNoErr;
	CFPropertyListRef		configPropertyList	= NULL;
	CFMutableDictionaryRef	configDict			= NULL;
	CFArrayRef				cfArrayRef			= NULL;
	CFIndex					cfConfigCount		= 0;
	CFDataRef				xmlData				= NULL;
	bool					bDoWrite			= false;

	XMLConfigLock();
	
	if (fXMLData != nil)
	{
		// extract the config dictionary from the XML data.
		configPropertyList = CFPropertyListCreateFromXMLData( kCFAllocatorDefault,
					fXMLData,
					kCFPropertyListMutableContainersAndLeaves, //could also use kCFPropertyListImmutable, kCFPropertyListMutableContainers
					NULL);
		
		XMLConfigUnlock();
	
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
				cfArrayRef = (CFArrayRef) CFDictionaryGetValue( configDict, CFSTR( kXMLConfigArrayKey ) );
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
							CFStringRef aString = (CFStringRef)CFDictionaryGetValue( serverDict, CFSTR( kXMLServerKey ) );
							if ( aString != nil && CFGetTypeID( aString ) == CFStringGetTypeID() )
							{
								CFStringRef aServerName = CFStringCreateWithCString( NULL, inServerName, kCFStringEncodingUTF8 );
								if (CFStringCompare(aString, aServerName, 0) == kCFCompareEqualTo)
								{
									//now insert the new replica list
									//get the replica arrays to remove the old ones
									CFArrayRef cfRepArrayRef = NULL;
									cfRepArrayRef = (CFArrayRef) CFDictionaryGetValue( serverDict, CFSTR( kXMLReplicaHostnameListArrayKey ) );
									if (cfRepArrayRef != NULL)
									{
										CFDictionaryRemoveValue( serverDict, CFSTR( kXMLReplicaHostnameListArrayKey ) );
									}
									if( inReplicaHostnames )
									{
										CFDictionarySetValue( serverDict, CFSTR( kXMLReplicaHostnameListArrayKey ), (CFArrayRef)inReplicaHostnames );
									}
									bDoWrite = true;

									cfRepArrayRef = (CFArrayRef) CFDictionaryGetValue( serverDict, CFSTR( kXMLWriteableHostnameListArrayKey ) );
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
									}
									CFRelease(aServerName);
									break; //found the correct server config and quit looking regardless if updated or not
								}
								CFRelease(aServerName);
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
	else
	{
		XMLConfigUnlock();
	}
		
	return( siResult );

} // UpdateReplicaList

// --------------------------------------------------------------------------------
//	* UpdateConfigWithSecuritySettings
// --------------------------------------------------------------------------------

sInt32 CLDAPv3Configs::UpdateConfigWithSecuritySettings( char *inNodeName, sLDAPConfigData *inConfig, LDAP *inLD )
{
	sInt32			siResult		= eDSNodeNotFound;	
	LDAPMessage		*pLDAPResult	= NULL;
	timeval			stTimeout		= { 20, 0 };	// timeout of 20 seconds
	bool			bChangedPolicy	= false;
	bool			bChangedConfig	= false;
	CFMutableDictionaryRef cfXMLDict = NULL;
	
	if( inNodeName == NULL ) return eDSNullParameter; // sometimes we don't have a nodename for some reason

	// let's find the config for the incoming node first for all the following updates
	XMLConfigLock();
	if (fXMLData != NULL)
	{
		cfXMLDict = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, fXMLData, kCFPropertyListMutableContainersAndLeaves, NULL );
	}
	XMLConfigUnlock();
	
	CFArrayRef				cfConfigArray = (CFArrayRef) CFDictionaryGetValue( cfXMLDict, CFSTR(kXMLConfigArrayKey) );
	CFIndex					iCount = cfConfigArray ? CFArrayGetCount( cfConfigArray ) : 0;
	CFMutableDictionaryRef  cfConfigDict = NULL;
	CFStringRef				cfServerName = CFStringCreateWithCString( kCFAllocatorDefault, inNodeName, kCFStringEncodingUTF8 );
	
	// let's find the exact config
	for( CFIndex ii = 0; ii < iCount; ii++ )
	{
		CFMutableDictionaryRef cfTempDict = (CFMutableDictionaryRef) CFArrayGetValueAtIndex( cfConfigArray, ii );
		
		// TBD --- Need to get the Node Name key in the future...
		CFStringRef cfServer = (CFStringRef) CFDictionaryGetValue( cfTempDict, CFSTR(kXMLServerKey) );
		
		if( CFStringCompare(cfServer, cfServerName, 0) == kCFCompareEqualTo )
		{
			cfConfigDict = cfTempDict;
			break;
		}
	}
	
	DSCFRelease( cfServerName );
	
	if( cfConfigDict )
	{
		// let's get the first attribute map for this one
		char	*pAttribute = ExtractAttrMap( kDSStdRecordTypeConfig, kDS1AttrXMLPlist, inConfig->fRecordAttrMapDict, 1 );	
		
		if( pAttribute != NULL )
		{
			// we're just going to search for this object, we don't care what type it is at this point.. TBD??
			int iLDAPRetCode = ldap_search_ext_s( inLD, inConfig->fMapSearchBase, LDAP_SCOPE_SUBTREE, "(cn=macosxodpolicy)", NULL, false, NULL, NULL, &stTimeout, 0, &pLDAPResult );
			
			if( iLDAPRetCode == LDAP_SUCCESS )
			{
				berval  **pBValues  = NULL;
				
				if( (pBValues = ldap_get_values_len(inLD, pLDAPResult, pAttribute)) != NULL )
				{
					// good, we have a security settings config record...  Let's use the config we just got from the server
					CFDataRef cfXMLData = CFDataCreate( kCFAllocatorDefault, (UInt8 *)(pBValues[0]->bv_val), pBValues[0]->bv_len );
					
					if( cfXMLData != NULL )
					{
						// extract the config dictionary from the XML data and this is our new configuration
						CFMutableDictionaryRef cfTempDict = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, cfXMLData, kCFPropertyListMutableContainersAndLeaves, NULL );
						
						// make sure we got a dictionary and it has something in it...
						if( cfTempDict )
						{
							// set the new security level if they are different
							CFDictionaryRef cfConfigPolicy = (CFDictionaryRef) CFDictionaryGetValue( cfTempDict, CFSTR(kXMLConfiguredSecurityKey) );
							uInt32 iSecurityLevel = inConfig->fSecurityLevelLoc | CalculateSecurityPolicy( cfConfigPolicy );
							
							if( inConfig->fSecurityLevel != iSecurityLevel )
							{
								inConfig->fSecurityLevel = iSecurityLevel;
								bChangedPolicy = bChangedConfig = true;
							}
							
							CFBooleanRef cfBindingActive = (CFBooleanRef) CFDictionaryGetValue( cfTempDict, CFSTR(kXMLDirectoryBindingKey) );
							if( cfBindingActive != NULL && CFGetTypeID(cfBindingActive) == CFBooleanGetTypeID() )
							{
								CFDictionarySetValue( cfConfigDict, CFSTR(kXMLDirectoryBindingKey), cfBindingActive );
								bChangedConfig = true; // we don't care about policy changes for binding a this point
							}
							
							CFRelease( cfTempDict );
							cfTempDict = NULL;
						}
						
						CFRelease( cfXMLData ); // let's release it, we're done with it
						cfXMLData = NULL;
					}
					
					ldap_value_free_len( pBValues );
					pBValues = NULL;
				}
			}
			
			if( pLDAPResult )
			{
				ldap_msgfree( pLDAPResult );
				pLDAPResult = NULL;
			}
			free( pAttribute );
		}
		
		// let's update the supported security settings from the SupportedSASLMethods in the Config
		if( inConfig->fSASLmethods && CFArrayGetCount(inConfig->fSASLmethods) )
		{
			// Start the dictionary for the Security Levels supported
			CFMutableDictionaryRef cfSupportedSecLevel = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
			
			// Store the dictionary in the configuration..
			CFDictionarySetValue( cfConfigDict, CFSTR(kXMLSupportedSecurityKey), cfSupportedSecLevel );
			
			// if we have SSL, we have some capabilities already..
			if( inConfig->bIsSSL )
			{
				CFDictionarySetValue( cfSupportedSecLevel, CFSTR(kXMLSecurityNoClearTextAuths), kCFBooleanTrue );
				CFDictionarySetValue( cfSupportedSecLevel, CFSTR(kXMLSecurityPacketEncryption), kCFBooleanTrue );
			}
			
			CFRange		stRange = CFRangeMake( 0, CFArrayGetCount(inConfig->fSASLmethods) );
			
			// we need to verify with CLDAPNode what types are supported..... TBD
			if( CFArrayContainsValue( inConfig->fSASLmethods, stRange, CFSTR("CRAM-MD5")) )
			{
				CFDictionarySetValue( cfSupportedSecLevel, CFSTR(kXMLSecurityNoClearTextAuths), kCFBooleanTrue );
			}
			
			if( CFArrayContainsValue( inConfig->fSASLmethods, stRange, CFSTR("GSSAPI")) )
			{
				CFDictionarySetValue( cfSupportedSecLevel, CFSTR(kXMLSecurityNoClearTextAuths), kCFBooleanTrue );
				CFDictionarySetValue( cfSupportedSecLevel, CFSTR(kXMLSecurityManInTheMiddle), kCFBooleanTrue );
				CFDictionarySetValue( cfSupportedSecLevel, CFSTR(kXMLSecurityPacketSigning), kCFBooleanTrue );
				CFDictionarySetValue( cfSupportedSecLevel, CFSTR(kXMLSecurityPacketEncryption), kCFBooleanTrue );
			}
			bChangedConfig = true;
			
			DSCFRelease(cfSupportedSecLevel);
		}
		else if( CFDictionaryContainsKey(cfConfigDict, CFSTR(kXMLSupportedSecurityKey)) )
		{
			CFDictionaryRemoveValue( cfConfigDict, CFSTR(kXMLSupportedSecurityKey) );
			bChangedConfig = true;
		}
		
		if( bChangedConfig )
		{
			// Now update our Config file on disk and in memory
			CFDataRef aXMLData = (CFDataRef) CFPropertyListCreateXMLData( kCFAllocatorDefault, cfXMLDict );
			
			SetXMLConfig( aXMLData );
			
			DSCFRelease( aXMLData );
			
			if( bChangedPolicy )
			{
                siResult = eDSNoErr; // only set this when security settings changed since that is when it matters
                DBGLOG1( kLogPlugin, "CLDAPv3Configs: [%s] Updated Security Policies from Directory.", inConfig->fNodeName );
				syslog(LOG_ALERT,"LDAPv3: [%s] Updated Security Policies from Directory.", inConfig->fNodeName);
			}
		}
	}
	
	// let's reset the flag so we don't keep checking
	inConfig->bGetSecuritySettings = false;
	
	DSCFRelease( cfXMLDict );
	
	return siResult;
} // UpdateConfigWithSecuritySettings

// --------------------------------------------------------------------------------
//	* UpdateLDAPConfigWithServerMappings
// --------------------------------------------------------------------------------

sInt32 CLDAPv3Configs::UpdateLDAPConfigWithServerMappings ( char *inServer, char *inMapSearchBase, int inPortNumber, bool inIsSSL, bool inLDAPv2ReadOnly, bool inMakeDefLDAP, bool inReferrals, LDAP *inServerHost)
{
	sInt32		siResult	= eDSNoErr;
	CFDataRef	ourXMLData	= nil;
	CFDataRef	newXMLData	= nil;
	
	ourXMLData = RetrieveServerMappings( inServer, inMapSearchBase, inPortNumber, inIsSSL, inLDAPv2ReadOnly, inServerHost );
	if (ourXMLData != nil)
	{
		//here we will make sure that the server location and port/SSL in the XML data is the same as given above
		//we also make sure that the MakeDefLDAPFlag is set so that this gets added to the Automatic search policy
		newXMLData = VerifyAndUpdateServerLocation(inServer, inPortNumber, inIsSSL, inLDAPv2ReadOnly, inMakeDefLDAP, ourXMLData); //don't check return
		
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
			syslog(LOG_ALERT,"DSLDAPv3PlugIn: [%s] LDAP server config not updated with server mappings due to server mappings format error.", inServer);
		}
	}
	else
	{
		syslog(LOG_ALERT,"DSLDAPv3PlugIn: [%s] LDAP server config not updated with server mappings due to server mappings error.", inServer);
		siResult = eDSCannotAccessSession;
	}
	
	return(siResult);

} // UpdateLDAPConfigWithServerMappings

// ---------------------------------------------------------------------------
//	* VerifyAndUpdateServerLocation
// ---------------------------------------------------------------------------

// This is used to update an XML dictionary with the new server, port, ssl and makedef flag

CFDataRef CLDAPv3Configs::VerifyAndUpdateServerLocation( char *inServer, int inPortNumber, bool inIsSSL, bool inLDAPv2ReadOnly, bool inMakeDefLDAP, CFDataRef inXMLData )
{
	CFPropertyListRef		configPropertyList	= nil;
	CFMutableDictionaryRef	serverConfigDict	= nil;
	char				   *configVersion		= nil;
	int						portNumber			= 389;
	bool					bIsSSL				= false;
	bool					bLDAPv2ReadOnly		= false;
	CFStringRef				cfStringRef			= nil;
	bool					bUpdate				= false;
	CFBooleanRef			cfBool				= false;
	CFNumberRef				cfNumber			= 0;
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
																NULL);
		
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
				
					DBGLOG( kLogPlugin, "CLDAPv3Configs: Have successfully read the LDAP XML config data" );

					//if config data is up to date with latest default mappings then use them
					if (strcmp(configVersion,"DSLDAPv3PlugIn Version 1.5") == 0)
					{
						//now verify the inServer, inPortNumber and inIsSSL
						
						cfStringRef = (CFStringRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLServerKey ) );
						if ( cfStringRef != nil && CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
						{
							CFStringRef cfInServer = CFStringCreateWithCString(kCFAllocatorDefault, inServer, kCFStringEncodingUTF8);

							if( CFStringCompare(cfStringRef, cfInServer, 0) != kCFCompareEqualTo )
							{
								//replace the server value
								bUpdate = true;
								CFDictionaryReplaceValue(serverConfigDict, CFSTR( kXMLServerKey ), cfInServer);
								CFRelease(cfInServer);
								cfInServer = nil;
							}
						}
			
						cfNumber = (CFNumberRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLPortNumberKey ) );
						if ( cfNumber != nil )
						{
							CFNumberGetValue(cfNumber, kCFNumberIntType, &portNumber);
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
			
						cfBool= (CFBooleanRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLIsSSLFlagKey ) );
						if (cfBool != nil)
						{
							bIsSSL = CFBooleanGetValue( cfBool );
							if (bIsSSL != inIsSSL)
							{
								//replace the SSL flag
								bUpdate = true;
								CFDictionaryReplaceValue(serverConfigDict, CFSTR( kXMLIsSSLFlagKey ), (inIsSSL ? kCFBooleanTrue : kCFBooleanFalse) );									}
							//CFRelease( cfBool ); // no since pointer only from Get
						}
						
						cfBool= (CFBooleanRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLLDAPv2ReadOnlyKey ) );
						if (cfBool != nil)
						{
							bLDAPv2ReadOnly = CFBooleanGetValue( cfBool );
							if (bLDAPv2ReadOnly != inLDAPv2ReadOnly)
							{
								//replace the LDAPv2ReadOnly flag
								bUpdate = true;
								CFDictionaryReplaceValue(serverConfigDict, CFSTR( kXMLLDAPv2ReadOnlyKey ), (inLDAPv2ReadOnly ? kCFBooleanTrue : kCFBooleanFalse) );									}
							//CFRelease( cfBool ); // no since pointer only from Get
						}
						
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
						else
						{
							bUpdate = true;
							CFDictionarySetValue(serverConfigDict, CFSTR( kXMLServerMappingsFlagKey ), kCFBooleanTrue); // why True???? if not set..
						}
						
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
						else
						{
							bUpdate = true;
							CFDictionarySetValue(serverConfigDict, CFSTR( kXMLMakeDefLDAPFlagKey ), (inMakeDefLDAP ? kCFBooleanTrue : kCFBooleanFalse));
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
//	* ConfigLDAPServers
// ---------------------------------------------------------------------------

// This routine is used to initialize all configurations from the file.  
// This overrides anything already in memory, mappings, etc., if they are different..

sInt32 CLDAPv3Configs::ConfigLDAPServers ( void )
{
	sInt32					siResult			= eDSNoErr;
	CFPropertyListRef		configPropertyList	= NULL;
	CFMutableDictionaryRef	configDict			= NULL;
	CFArrayRef				cfArrayRef			= NULL;
	CFIndex					cfConfigCount		= 0;
	CFDataRef				xmlData				= NULL;
	char				   *configVersion		= nil;

	try
	{	
		XMLConfigLock();
		if (fXMLData != nil)
		{
			// extract the config dictionary from the XML data.
			configPropertyList = CFPropertyListCreateFromXMLData( kCFAllocatorDefault,
						fXMLData,
						kCFPropertyListMutableContainers, //could also use kCFPropertyListImmutable, kCFPropertyListMutableContainers
						NULL );
			XMLConfigUnlock();
			
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
					
						DBGLOG( kLogPlugin, "CLDAPv3Configs: Have successfully read the LDAP XML config file" );

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
                        }
						
						//array of LDAP server configs
						cfArrayRef = (CFArrayRef) CFDictionaryGetValue( configDict, CFSTR( kXMLConfigArrayKey ) );
						if (cfArrayRef != nil)
						{
							//now we can retrieve each config
							cfConfigCount = ::CFArrayGetCount( cfArrayRef );

							//loop through the configs
							//use iConfigIndex for the access to the cfArrayRef
							//use fConfigTableLen for the index to add to the table since we add at the end
							for (sInt32 iConfigIndex = 0; iConfigIndex < cfConfigCount; iConfigIndex++)
							{
								CFDictionaryRef serverConfigDict = (CFDictionaryRef)::CFArrayGetValueAtIndex( cfArrayRef, iConfigIndex );
								if ( serverConfigDict != nil )
								{
									siResult = MakeLDAPConfig( serverConfigDict, true );
								}
							} // loop over configs
							
						} // if (cfArrayRef != nil) ie. an array of LDAP configs exists
						delete(configVersion);
						
					}//if (configVersion != nil)
					
					// don't release the configDict since it is the cast configPropertyList
					
				}//if (configDict != nil)
				
			}//if (configPropertyList != nil )
		} // fXMLData != nil
		else
		{
			XMLConfigUnlock();
		}
		
	} // try
	catch ( sInt32 err )
	{
		siResult = err;
	}
	
	if (configPropertyList != nil)
	{
		CFRelease(configPropertyList);
		configPropertyList = nil;
	}

	return( siResult );

} // ConfigLDAPServers

// ---------------------------------------------------------------------------
//	* AddToConfig
// ---------------------------------------------------------------------------

// this is used to add a Config on the fly

sInt32 CLDAPv3Configs::AddToConfig ( CFDataRef inXMLData )
{
	sInt32					siResult			= eDSCorruptBuffer;
    CFPropertyListRef		configPropertyList	= NULL;
	CFMutableDictionaryRef	configDict			= NULL;
	CFStringRef				cfStringRef			= NULL;
	CFBooleanRef			cfBool				= false;
	CFNumberRef				cfNumber			= 0;
	char				   *server				= nil;
    char				   *mapSearchBase 		= nil;
	int						portNumber			= 389;
	bool					bIsSSL				= false;
	bool					bLDAPv2ReadOnly		= false;
	bool					bServerMappings		= false;
	bool					bUseConfig			= false;
	bool					bReferrals			= true;
	int						opencloseTO			= kLDAPDefaultOpenCloseTimeoutInSeconds;
	int						idleTO				= 2;
	int						delayRebindTry		= kLDAPDefaultRebindTryTimeoutInSeconds;
	int						searchTO			= kLDAPDefaultSearchTimeoutInSeconds;
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
					NULL);
		
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

				XMLConfigLock();
				//let's first go ahead and add this data to the actual config XML tied to the config file
				if (fXMLData != nil)
				{
					// extract the config dictionary from the XML data.
					xConfigPropertyList = CFPropertyListCreateFromXMLData( kCFAllocatorDefault,
								fXMLData,
								kCFPropertyListMutableContainers, //could use kCFPropertyListImmutable
								NULL);
					XMLConfigUnlock();
					
					if (xConfigPropertyList != nil && CFDictionaryGetTypeID() == CFGetTypeID( xConfigPropertyList ) )
					{
						xConfigDict = (CFMutableDictionaryRef) xConfigPropertyList;

						if ( CFDictionaryContainsKey( xConfigDict, CFSTR( kXMLConfigArrayKey ) ) )
						{
							cfMutableArrayRef = (CFMutableArrayRef)CFDictionaryGetValue( xConfigDict, CFSTR( kXMLConfigArrayKey ) );
							//simply add the new to the old here
							CFArrayAppendValue(cfMutableArrayRef, configDict);
						}
						else //we need to make the first entry here
						{
							cfMutableArrayRef = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
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
					
					if( xConfigPropertyList )
					{
						CFRelease(xConfigPropertyList);
						xConfigPropertyList = NULL;
					}
				} // fXMLData != nil
				else
				{
					XMLConfigUnlock();
				}
				
				cfBool = (CFBooleanRef)CFDictionaryGetValue( configDict, CFSTR( kXMLEnableUseFlagKey ) );
				if (cfBool != nil)
				{
					bUseConfig = CFBooleanGetValue( cfBool );
				}
				
				//continue if this configuration was enabled by the user
				if ( bUseConfig )
				{
					//Enable Use flag is NOT provided to the configTable
					//retrieve all the others for the configTable
					
					cfStringRef = (CFStringRef)CFDictionaryGetValue( configDict, CFSTR( kXMLServerKey ) );
					if ( cfStringRef != nil && CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
					{
						uInt32 uiLength = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfStringRef), kCFStringEncodingUTF8 ) + 1;
						server = (char *) calloc( sizeof(char), uiLength );
						CFStringGetCString( cfStringRef, server, uiLength, kCFStringEncodingUTF8 );
					}
		
					cfBool= (CFBooleanRef)CFDictionaryGetValue( configDict, CFSTR( kXMLIsSSLFlagKey ) );
					if (cfBool != nil)
					{
						bIsSSL = CFBooleanGetValue( cfBool );

						if (bIsSSL)
						{
							portNumber = LDAPS_PORT; // default for SSL ie. if no port given below
						}
					}
					
					cfBool= (CFBooleanRef)CFDictionaryGetValue( configDict, CFSTR( kXMLLDAPv2ReadOnlyKey ) );
					if (cfBool != nil)
					{
						bLDAPv2ReadOnly = CFBooleanGetValue( cfBool );
					}
		
					cfNumber = (CFNumberRef)CFDictionaryGetValue( configDict, CFSTR( kXMLOpenCloseTimeoutSecsKey ) );
					if ( cfNumber != nil )
					{
						CFNumberGetValue(cfNumber, kCFNumberIntType, &opencloseTO);
					}
		
					cfNumber = (CFNumberRef)CFDictionaryGetValue( configDict, CFSTR( kXMLIdleTimeoutMinsKey ) );
					if ( cfNumber != nil )
					{
						CFNumberGetValue(cfNumber, kCFNumberIntType, &idleTO);
					}
		
					cfNumber = (CFNumberRef)CFDictionaryGetValue( configDict, CFSTR( kXMLDelayedRebindTrySecsKey ) );
					if ( cfNumber != nil )
					{
						CFNumberGetValue(cfNumber, kCFNumberIntType, &delayRebindTry);
					}
		
					cfNumber = (CFNumberRef)CFDictionaryGetValue( configDict, CFSTR( kXMLSearchTimeoutSecsKey ) );
					if ( cfNumber != nil )
					{
						CFNumberGetValue(cfNumber, kCFNumberIntType, &searchTO);
					}

					cfNumber = (CFNumberRef)CFDictionaryGetValue( configDict, CFSTR( kXMLPortNumberKey ) );
					if ( cfNumber != nil )
					{
						CFNumberGetValue(cfNumber, kCFNumberIntType, &portNumber);
					}
		
					cfBool = (CFBooleanRef)CFDictionaryGetValue( configDict, CFSTR( kXMLServerMappingsFlagKey ) );
					if (cfBool != nil)
					{
						bServerMappings = CFBooleanGetValue( cfBool );
					}
					
					cfBool = (CFBooleanRef)CFDictionaryGetValue( configDict, CFSTR( kXMLReferralFlagKey ) );
					if (cfBool != nil)
					{
						bReferrals = CFBooleanGetValue( cfBool );
					}
					
					cfStringRef = (CFStringRef)CFDictionaryGetValue( configDict, CFSTR( kXMLMapSearchBase ) );
					if ( cfStringRef != nil && CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
					{
						uInt32 uiLength = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfStringRef), kCFStringEncodingUTF8 ) + 1;
						mapSearchBase = (char *) calloc( sizeof(char), uiLength );
						CFStringGetCString( cfStringRef, mapSearchBase, uiLength, kCFStringEncodingUTF8 );
					}
						
					siResult = MakeServerBasedMappingsLDAPConfig( server, mapSearchBase, opencloseTO, idleTO, delayRebindTry, searchTO, portNumber, bIsSSL, true, bReferrals, bLDAPv2ReadOnly );
					
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
					
			}
			CFRelease(configPropertyList);
		}
	}
	
	return(siResult);
}


// ---------------------------------------------------------------------------
//	* AddLDAPServer
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Configs::AddLDAPServer( CFDataRef inXMLData )
{
	sInt32					siResult			= eDSNoErr;
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
																    NULL);
			
			if (configPropertyList != nil && CFDictionaryGetTypeID() == CFGetTypeID( configPropertyList ))
			{
				serverConfigDict = (CFMutableDictionaryRef) configPropertyList; //don't need mutable really
				
				//get version, and the specific LDAP server config
				
				//config data version
				configVersion = GetVersion(serverConfigDict);
				if ( configVersion == nil )
				{
					syslog(LOG_ALERT,"LDAPv3: Obtained LDAP server mappings is missing the version string.");
					throw( (sInt32)eDSVersionMismatch ); //KW need eDSPlugInConfigFileError
				}
				else
				{
					DBGLOG( kLogPlugin, "CLDAPv3Configs: Have successfully read the LDAP XML config data" );

					//if config data is up to date with latest default mappings then use them
					if (strcmp(configVersion,"DSLDAPv3PlugIn Version 1.5") == 0)
					{
						siResult = MakeLDAPConfig(serverConfigDict, false, true);
					}
					else
					{
						syslog(LOG_ALERT,"LDAPv3: Obtained LDAP server mappings contain incorrect version string [%s] instead of [DSLDAPv3PlugIn Version 1.5].", configVersion);
					}
					delete(configVersion);
					
				}//if (configVersion != nil)
			}//if (configPropertyList != nil )
		} // inXMLData != nil
		
	} // try
	catch ( sInt32 err )
	{
		siResult = err;
	}
	
	if (configPropertyList != nil)
	{
		CFRelease(configPropertyList); // built from Create on XML data so okay to dealloc here
		configPropertyList = nil;
	}

	return( siResult );

} // AddLDAPServer

#pragma mark -
#pragma mark ConfigData Manipulation Routines


// --------------------------------------------------------------------------------
//	* GetDefaultLDAPNodeStrings
// --------------------------------------------------------------------------------

char** CLDAPv3Configs::GetDefaultLDAPNodeStrings( uInt32 &count )
{
	uInt32				counter	= 0;
	sLDAPConfigData	   *pConfig	= nil;
	
	fConfigMapMutex.Wait();
		
	LDAPConfigDataMapI  configIter = fConfigMap.begin();

	//find the number of entries that match
	while( configIter != fConfigMap.end() )
	{
		pConfig = configIter->second;
		if ( (pConfig != nil) && (pConfig->bUseAsDefaultLDAP) && (pConfig->fServerName != nil) )
		{
			counter++;
		}
		configIter++;
	} // loop over config table entries
	
	//set count return value
	count = counter;
	char** outList = (char **)calloc(counter + 1, sizeof(char*));
	
	//now fill the string list
	configIter = fConfigMap.begin();
	counter = 0;
	while( configIter != fConfigMap.end() )
	{
		pConfig = configIter->second;
		if ( (pConfig != nil) && (pConfig->bUseAsDefaultLDAP) && (pConfig->fServerName != nil) )
		{
			char *theDHCPNodeName = (char *) calloc(1, sizeof("/LDAPv3/") + strlen(pConfig->fServerName));
			strcpy(theDHCPNodeName, "/LDAPv3/");
			strcat(theDHCPNodeName, pConfig->fServerName);
			outList[counter] = theDHCPNodeName;
			counter++;
		}
		
		configIter++;
	} // loop over config table entries
	
	fConfigMapMutex.Signal();
	
	return(outList);
} // GetDefaultLDAPNodeStrings


// --------------------------------------------------------------------------------
//	* SetAllConfigBuildReplicaFlagTrue
// --------------------------------------------------------------------------------

void CLDAPv3Configs::SetAllConfigBuildReplicaFlagTrue( void )
{
	fConfigMapMutex.Wait();
		
	LDAPConfigDataMapI  configIter = fConfigMap.begin();

	while( configIter != fConfigMap.end() )
	{
		sLDAPConfigData *pConfig = configIter->second;
		if (pConfig != nil)
		{
			pConfig->bBuildReplicaList = true;
		}// if config entry not nil
		
		configIter++;
	} // loop over config table entries
	
	fConfigMapMutex.Signal();
} // SetAllConfigBuildReplicaFlagTrue

uInt32 CLDAPv3Configs::CalculateSecurityPolicy(	CFDictionaryRef inConfiguration )
{
	CFBooleanRef	cfBool;
	uInt32			uiSecurityLevel = kSecNoSecurity;
	
	if ( inConfiguration != NULL && CFGetTypeID(inConfiguration) == CFDictionaryGetTypeID() )
	{
		if( (cfBool = (CFBooleanRef) CFDictionaryGetValue(inConfiguration, CFSTR(kXMLSecurityNoClearTextAuths))) && CFBooleanGetValue(cfBool) )
		{
			uiSecurityLevel |= kSecDisallowCleartext;
		}
		
		if( (cfBool = (CFBooleanRef) CFDictionaryGetValue(inConfiguration, CFSTR(kXMLSecurityManInTheMiddle))) && CFBooleanGetValue(cfBool) )
		{
			uiSecurityLevel |= kSecManInMiddle;
		}
		
		if( (cfBool = (CFBooleanRef) CFDictionaryGetValue(inConfiguration, CFSTR(kXMLSecurityPacketSigning))) && CFBooleanGetValue(cfBool) )
		{
			uiSecurityLevel |= kSecPacketSigning;
		}
		
		if( (cfBool = (CFBooleanRef) CFDictionaryGetValue(inConfiguration, CFSTR(kXMLSecurityPacketEncryption))) && CFBooleanGetValue(cfBool) )
		{
			uiSecurityLevel |= kSecPacketEncryption;
		}
	}
	
	return uiSecurityLevel;
}

// --------------------------------------------------------------------------------
//	* MakeLDAPConfig
// --------------------------------------------------------------------------------

sInt32 CLDAPv3Configs::MakeLDAPConfig( CFDictionaryRef ldapDict, bool inOverWriteAll, bool inServerMappingUpdate )
{
	sInt32				siResult	= eDSNoErr;
	CFStringRef			cfStringRef	= nil;
	CFDataRef			cfDataRef	= nil;
	CFBooleanRef		cfBool		= false;
	CFNumberRef			cfNumber	= nil;
	char			   *server		= nil;
    sLDAPConfigData	   *pConfig		= nil;
	bool				bUseConfig = false;
	
	cfBool = (CFBooleanRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLEnableUseFlagKey ) );
	if (cfBool != nil)
	{
		bUseConfig = CFBooleanGetValue( cfBool );
	}

	//continue if this configuration was enabled by the user
	//no error condition returned if this configuration is not used due to the enable use flag
	if ( bUseConfig )
	{
		//need to get the server name first
		cfStringRef = (CFStringRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLServerKey ) );
		if ( cfStringRef != nil && CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
		{
			uInt32 uiLength = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfStringRef), kCFStringEncodingUTF8 ) + 1;
			server = (char *) calloc( sizeof(char), uiLength );
			CFStringGetCString( cfStringRef, server, uiLength, kCFStringEncodingUTF8 );
		}
		
		// if we have a servername, there's a reason to continue..
		if( server != NULL && strlen(server) )
		{
			//Need to check here if the config already exists ie. the server name exists
			//if it does then assume that this will replace what was given before
			pConfig = ConfigWithNodeNameLock( server );

			// if we are NULL, we need a new entry
			if( pConfig == NULL )
			{
				pConfig = new sLDAPConfigData;
				
				pConfig->fServerName	= server;
				pConfig->fNodeName		= strdup( server );
				pConfig->fRefCount		= 1;
				
				fConfigMapMutex.Wait();
				fConfigMap[string(pConfig->fNodeName)] = pConfig;
				fConfigMapMutex.Signal();

				pConfig->fConfigLock->Wait();   // need to lock the new configuration.. we unlock below...

				server = NULL;   // set to NULL so we don't release later
			}
			
			// overwrite is used when we are initialized from the file
			// some data is still valid, so we don't reset the whole configuration
			if( inOverWriteAll )
			{
				// let's reset various things, since we are an existing entry at this point..
				pConfig->bIsSSL = false;
				pConfig->bLDAPv2ReadOnly = false;
				pConfig->fServerPort = LDAP_PORT;
				pConfig->fOpenCloseTimeout = kLDAPDefaultOpenCloseTimeoutInSeconds;
				pConfig->fIdleTimeout = 2;
				pConfig->fDelayRebindTry = kLDAPDefaultRebindTryTimeoutInSeconds;
				pConfig->fSearchTimeout = kLDAPDefaultSearchTimeoutInSeconds;
				pConfig->bSecureUse = false;
				pConfig->bUseAsDefaultLDAP = false;
				pConfig->bServerMappings = false;
				pConfig->bReferrals = true;
				pConfig->bDNSReplicas = false;
				DSDelete( pConfig->fUIName );
				DSDelete( pConfig->fServerAccount );
				DSDelete( pConfig->fServerPassword );
				DSDelete( pConfig->fKerberosId );
				DSDelete( pConfig->fMapSearchBase );
				DSCFRelease( pConfig->fRecordAttrMapDict );
				DSCFRelease( pConfig->fRecordTypeMapCFArray );
				DSCFRelease( pConfig->fAttrTypeMapCFArray );
				DSCFRelease( pConfig->fReplicaHostnames );
				DSCFRelease( pConfig->fWriteableHostnames );
			}
			
			// if it is a servermappingsUpdate, then we need to ensure the flag is always set and update accordingly
			if( inServerMappingUpdate )
			{
				pConfig->bServerMappings = true;
			}
			else
			{
				cfBool = (CFBooleanRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLServerMappingsFlagKey ) );
				if (cfBool != nil)
				{
					pConfig->bServerMappings = CFBooleanGetValue( cfBool );
				}
			}
			
			cfBool = (CFBooleanRef) CFDictionaryGetValue( ldapDict, CFSTR( kXMLIsSSLFlagKey ) );
			if (cfBool != nil)
			{
				pConfig->bIsSSL = CFBooleanGetValue( cfBool );
				if (pConfig->bIsSSL)
				{
					pConfig->fServerPort = LDAPS_PORT; // default for SSL ie. if no port given below
				}
			}
			
			cfBool = (CFBooleanRef) CFDictionaryGetValue( ldapDict, CFSTR( kXMLLDAPv2ReadOnlyKey ) );
			if (cfBool != nil)
			{
				pConfig->bLDAPv2ReadOnly = CFBooleanGetValue( cfBool );
			}
			
			cfNumber = (CFNumberRef) CFDictionaryGetValue( ldapDict, CFSTR( kXMLPortNumberKey ) );
			if ( cfNumber != nil )
			{
				CFNumberGetValue(cfNumber, kCFNumberIntType, &pConfig->fServerPort);
			}
			
			cfNumber = (CFNumberRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLOpenCloseTimeoutSecsKey ) );
			if ( cfNumber != nil )
			{
				CFNumberGetValue(cfNumber, kCFNumberIntType, &pConfig->fOpenCloseTimeout);
			}
			
			cfNumber = (CFNumberRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLIdleTimeoutMinsKey ) );
			if ( cfNumber != nil )
			{
				CFNumberGetValue(cfNumber, kCFNumberIntType, &pConfig->fIdleTimeout);
			}
			
			cfNumber = (CFNumberRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLDelayedRebindTrySecsKey ) );
			if ( cfNumber != nil )
			{
				CFNumberGetValue(cfNumber, kCFNumberIntType, &pConfig->fDelayRebindTry);
			}
			
			cfNumber = (CFNumberRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLSearchTimeoutSecsKey ) );
			if ( cfNumber != nil )
			{
				CFNumberGetValue(cfNumber, kCFNumberIntType, &pConfig->fSearchTimeout);
			}
			
			cfBool = (CFBooleanRef) CFDictionaryGetValue( ldapDict, CFSTR( kXMLUseDNSReplicasFlagKey ) );
			if( cfBool != NULL )
			{
				pConfig->bDNSReplicas = CFBooleanGetValue( cfBool );
			}
			
			//null strings are acceptable but not preferred
			//ie. the new char will be of length one and the strcpy will copy the "" - empty string
			
			cfStringRef = (CFStringRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLUserDefinedNameKey ) );
			if ( cfStringRef != nil && CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
			{
				DSDelete( pConfig->fUIName );

				uInt32 uiLength = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfStringRef), kCFStringEncodingUTF8 ) + 1;
				pConfig->fUIName = (char *) calloc( sizeof(char), uiLength );
				CFStringGetCString( cfStringRef, pConfig->fUIName, uiLength, kCFStringEncodingUTF8 );
			}
			
			// unless we are reading settings from file, we don't update credentials???
			if( inOverWriteAll )
			{
				cfBool= (CFBooleanRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLSecureUseFlagKey ) );
				if (cfBool != nil)
				{
					pConfig->bSecureUse = CFBooleanGetValue( cfBool );
				}
				
				cfStringRef = (CFStringRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLServerAccountKey ) );
				if ( cfStringRef != nil && CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
				{
					DSDelete( pConfig->fServerAccount );

					uInt32 uiLength = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfStringRef), kCFStringEncodingUTF8 ) + 1;
					pConfig->fServerAccount = (char *) calloc( sizeof(char), uiLength );
					CFStringGetCString( cfStringRef, pConfig->fServerAccount, uiLength, kCFStringEncodingUTF8 );
				}
				
				cfStringRef = (CFStringRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLServerPasswordKey ) );
				if ( cfStringRef != nil )
				{
					DSDelete( pConfig->fServerPassword );

					if ( CFGetTypeID( cfStringRef ) == CFDataGetTypeID() )
					{
						cfDataRef = (CFDataRef) cfStringRef;
						CFIndex passwordLen = CFDataGetLength(cfDataRef);
						pConfig->fServerPassword = (char *) calloc( sizeof(char), passwordLen + 1 );
						CFDataGetBytes( cfDataRef, CFRangeMake(0,passwordLen), (UInt8*)pConfig->fServerPassword );
					}
					else if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
					{
						uInt32 uiLength = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfStringRef), kCFStringEncodingUTF8 ) + 1;
						pConfig->fServerPassword = (char *) calloc( sizeof(char), uiLength );
						CFStringGetCString( cfStringRef, pConfig->fServerPassword, uiLength, kCFStringEncodingUTF8 );
					}
				}
				
				cfStringRef = (CFStringRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLKerberosId ) );
				if ( cfStringRef != nil && CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
				{
					DSDelete( pConfig->fKerberosId );

					uInt32 uiLength = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfStringRef), kCFStringEncodingUTF8 ) + 1;
					pConfig->fKerberosId = (char *) calloc( sizeof(char), uiLength );
					CFStringGetCString( cfStringRef, pConfig->fKerberosId, uiLength, kCFStringEncodingUTF8 );
				}

				if( cfBool = (CFBooleanRef)CFDictionaryGetValue( ldapDict, CFSTR(kXMLReferralFlagKey) ) )
				{
					if( CFGetTypeID( cfBool ) == CFBooleanGetTypeID() )
					{
						pConfig->bReferrals = CFBooleanGetValue( cfBool );
					}
				}
			}
			
			cfBool = (CFBooleanRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLMakeDefLDAPFlagKey ) );
			if (cfBool != nil)
			{
				pConfig->bUseAsDefaultLDAP = CFBooleanGetValue( cfBool );
			}
			
			cfStringRef = (CFStringRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLMapSearchBase ) );
			if ( cfStringRef != nil && CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
			{
				DSDelete( pConfig->fMapSearchBase );

				uInt32 uiLength = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfStringRef), kCFStringEncodingUTF8 ) + 1;
				pConfig->fMapSearchBase = (char *) calloc( sizeof(char), uiLength );
				CFStringGetCString( cfStringRef, pConfig->fMapSearchBase, uiLength, kCFStringEncodingUTF8 );
			}
			
			if( inServerMappingUpdate == false )
			{
				// if we have a configured security level, let's parse it
				CFDictionaryRef cfLocalPolicy = (CFDictionaryRef) CFDictionaryGetValue( ldapDict, CFSTR(kXMLLocalSecurityKey) );
				CFDictionaryRef cfConfigPolicy = (CFDictionaryRef) CFDictionaryGetValue( ldapDict, CFSTR(kXMLConfiguredSecurityKey) );
				pConfig->fSecurityLevelLoc = CalculateSecurityPolicy( cfLocalPolicy );
				pConfig->fSecurityLevel = pConfig->fSecurityLevelLoc | CalculateSecurityPolicy( cfConfigPolicy );
			}
			
			pConfig->bUpdated = true;
			
			//get the mappings from the config ldap dict
			BuildLDAPMap( pConfig, ldapDict, pConfig->bServerMappings );
			
			ConfigUnlock( pConfig );

		}// if( server != NULL && strlen(server) )

		DSDelete( server );

	}// if ( bUseConfig )

	// return if nil or not
	return( siResult );

} // MakeLDAPConfig


// --------------------------------------------------------------------------------
//	* MakeServerBasedMappingsLDAPConfig
// --------------------------------------------------------------------------------

sInt32 CLDAPv3Configs::MakeServerBasedMappingsLDAPConfig ( char *inServer, char *inMapSearchBase, int inOpenCloseTO, int inIdleTO, int inDelayRebindTry, int inSearchTO, int inPortNumber, bool inIsSSL, bool inMakeDefLDAP, bool inReferrals, bool inLDAPv2ReadOnly )
{
	sInt32				siResult	= eDSNoErr;
    sLDAPConfigData	   *pConfig		= nil;

    //Need to check here if the config already exists ie. the server name exists
    //if it does then assume that this will replace what was given before
    
	// if we have a servername, there's a reason to continue..
	if( inServer != NULL && strlen(inServer) )
	{
		//Need to check here if the config already exists ie. the server name exists
		//if it does then assume that this will replace what was given before
		
		pConfig = ConfigWithNodeNameLock( inServer );
		if( pConfig == NULL )
		{
			pConfig = new sLDAPConfigData( inServer, inServer, inServer, inOpenCloseTO, inIdleTO, inDelayRebindTry, inSearchTO, inPortNumber, false, NULL, NULL, NULL, inMakeDefLDAP, true, inIsSSL, inMapSearchBase, kSecNoSecurity, kSecNoSecurity, inReferrals, inLDAPv2ReadOnly, false );
			
			fConfigMapMutex.Wait();
			fConfigMap[string(inServer)] = pConfig;
			fConfigMapMutex.Signal();
		}
		else
		{
			pConfig->bUpdated = true;
			ConfigUnlock( pConfig );
		}
	}

	return( siResult );

} // MakeServerBasedMappingsLDAPConfig

// --------------------------------------------------------------------------------
//	* BuildLDAPMap
// --------------------------------------------------------------------------------

sInt32 CLDAPv3Configs::BuildLDAPMap ( sLDAPConfigData *inConfig, CFDictionaryRef ldapDict, bool inServerMapppings )
{
	CFArrayRef			cfArrayRef		= NULL;

	// Create a copy of the array in a standard format so we don't have to do any checking later..
	cfArrayRef = (CFArrayRef) CFDictionaryGetValue( ldapDict, CFSTR( kXMLAttrTypeMapArrayKey ) );
	if ( cfArrayRef != NULL && CFGetTypeID(cfArrayRef) == CFArrayGetTypeID() && CFArrayGetCount(cfArrayRef) > 0 )
	{
		DSCFRelease( inConfig->fAttrTypeMapCFArray );
		inConfig->fAttrTypeMapCFArray = CFArrayCreateCopy(kCFAllocatorDefault, cfArrayRef);
	}

	// now we will get the record map array and normalize it to speed access
	cfArrayRef = (CFArrayRef) CFDictionaryGetValue(ldapDict, CFSTR(kXMLRecordTypeMapArrayKey));
	
	// if we got a record map and attribute map, lets add them
	CFDictionaryRef recordAttrMap = CreateNormalizedRecordAttrMap( cfArrayRef, inConfig->fAttrTypeMapCFArray );
	if( recordAttrMap != NULL )
	{
		DSCFRelease( inConfig->fRecordTypeMapCFArray );
		DSCFRelease( inConfig->fRecordAttrMapDict );
		
		inConfig->fRecordTypeMapCFArray = CFArrayCreateCopy( kCFAllocatorDefault, cfArrayRef ); // copy the map as all or part was good
		inConfig->fRecordAttrMapDict = recordAttrMap;
	}
	
	cfArrayRef = (CFArrayRef) CFDictionaryGetValue( ldapDict, CFSTR( kXMLReplicaHostnameListArrayKey ) );
	if ( (cfArrayRef != nil) && (CFGetTypeID( cfArrayRef ) == CFArrayGetTypeID()) && (CFArrayGetCount(cfArrayRef) > 0) )
	{
		//clean out the old replica host names before we replace it
		if (inConfig->fReplicaHostnames != nil)
		{
			CFRelease(inConfig->fReplicaHostnames);
			inConfig->fReplicaHostnames	= NULL;
		}
		inConfig->fReplicaHostnames = CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, cfArrayRef);
	}
	
	cfArrayRef = (CFArrayRef) CFDictionaryGetValue( ldapDict, CFSTR( kXMLWriteableHostnameListArrayKey ) );
	if ( (cfArrayRef != nil) && (CFGetTypeID( cfArrayRef ) == CFArrayGetTypeID()) && (CFArrayGetCount(cfArrayRef) > 0) )
	{
		//clean out the old replica host names before we replace it
		if (inConfig->fWriteableHostnames != nil)
		{
			CFRelease(inConfig->fWriteableHostnames);
			inConfig->fWriteableHostnames	= NULL;
		}
		inConfig->fWriteableHostnames = CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, cfArrayRef);
	}
	
	return( eDSNoErr );

} // BuildLDAPMap

// --------------------------------------------------------------------------------
//	* CreateNormalizedAttributeMap
// --------------------------------------------------------------------------------

CFDictionaryRef CLDAPv3Configs::CreateNormalizedAttributeMap( CFArrayRef inAttrMapArray, CFDictionaryRef inGlobalAttrMap )
{
	CFMutableDictionaryRef	newAttrMapDict	= NULL;
	CFIndex					iTotal			= 0;
	CFIndex					iGlobalMapTotal = 0;
	
	if ( inGlobalAttrMap != NULL && (iGlobalMapTotal = CFDictionaryGetCount(inGlobalAttrMap)) > 0 )
	{
		// let's start with our global dictionary and add to it
		newAttrMapDict = CFDictionaryCreateMutableCopy( kCFAllocatorDefault, 0, inGlobalAttrMap );
	}

	if ( inAttrMapArray != NULL && CFGetTypeID(inAttrMapArray) == CFArrayGetTypeID() && (iTotal = CFArrayGetCount(inAttrMapArray)) > 0 )
	{
		if ( newAttrMapDict == NULL )
		{
			newAttrMapDict = CFDictionaryCreateMutable( kCFAllocatorDefault, iTotal, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		}
		
		for ( CFIndex iMapIndex = 0; iMapIndex < iTotal; iMapIndex++ )
		{
			CFDictionaryRef attrMapDict = (CFDictionaryRef) CFArrayGetValueAtIndex( inAttrMapArray, iMapIndex );
			if ( attrMapDict != NULL && CFGetTypeID(attrMapDict) == CFDictionaryGetTypeID() )
			{
				CFStringRef cfStdName = (CFStringRef) CFDictionaryGetValue( attrMapDict, CFSTR( kXMLStdNameKey ) );
				if ( cfStdName == NULL || CFGetTypeID( cfStdName ) != CFStringGetTypeID() )
					continue;
				
				CFArrayRef cfNativeArray = (CFArrayRef) CFDictionaryGetValue( attrMapDict, CFSTR( kXMLNativeMapArrayKey ) );
				if ( cfNativeArray == NULL || CFGetTypeID(cfNativeArray) != CFArrayGetTypeID() )
					continue;
				
				CFIndex iNativeMapCount = CFArrayGetCount( cfNativeArray );
				if( iNativeMapCount == 0 )
					continue;
				
				// now let's loop through the current native maps
				CFMutableArrayRef	cfNewNativeMap = CFArrayCreateMutable( kCFAllocatorDefault, iNativeMapCount, &kCFTypeArrayCallBacks );
				for( CFIndex iNativeIndex = 0; iNativeIndex < iNativeMapCount; iNativeIndex++ )
				{
					CFStringRef cfStringRef = (CFStringRef) CFArrayGetValueAtIndex( cfNativeArray, iNativeIndex );
					
					// if it is a valid string like we expect, let's add it to the new list..
					if( cfStringRef != NULL && CFGetTypeID(cfStringRef) == CFStringGetTypeID() )
					{
						CFArrayAppendValue( cfNewNativeMap, cfStringRef );
					}
				}

				// only add this if there is some values in the list it is key->array pair
				if( CFArrayGetCount(cfNewNativeMap) != 0 )
				{
					CFDictionarySetValue( newAttrMapDict, cfStdName, cfNewNativeMap );
				}
				CFRelease( cfNewNativeMap );
				cfNewNativeMap = NULL;
			}
		}
	}
	
	// if we have a dictionary, but it is empty, let's release and NULL
	if( newAttrMapDict != NULL && CFDictionaryGetCount(newAttrMapDict) == 0 )
	{
		CFRelease( newAttrMapDict );
		newAttrMapDict = NULL;
	}
	
	return newAttrMapDict;
}//CreateNormalizedAttributeMap

// --------------------------------------------------------------------------------
//	* CreateNormalizedRecordAttrMap
// --------------------------------------------------------------------------------

CFDictionaryRef CLDAPv3Configs::CreateNormalizedRecordAttrMap( CFArrayRef inRecMapArray, CFArrayRef inGlobalAttrMapArray )
{
	// this routine verifies an LDAP map once so that we don't have to worry about it later..
	// building a new array map and returning it for use.
	CFMutableDictionaryRef	outRecMapDict	= NULL;
	CFIndex					iTotal			= 0;
	
	if ( inRecMapArray != NULL && CFGetTypeID(inRecMapArray) == CFArrayGetTypeID() && (iTotal = CFArrayGetCount(inRecMapArray)) > 0 )
	{
		// let's size the array up front since we know the maximum number we'll have
		outRecMapDict = CFDictionaryCreateMutable( kCFAllocatorDefault, iTotal, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		CFDictionaryRef cfGlobalAttrMap = CreateNormalizedAttributeMap( inGlobalAttrMapArray, NULL );
		
		// this loop will normalize the array to a standard format that we use going forward.
		for( CFIndex iMapIndex = 0; iMapIndex < iTotal; iMapIndex++ )
		{
			CFDictionaryRef recMapDict = (CFDictionaryRef) CFArrayGetValueAtIndex( inRecMapArray, iMapIndex );
			if ( recMapDict != NULL )
			{
				// let's be sure the StdNameKey exists and it is really a CFString, otherwise it is useless 
				CFStringRef cfStdName = (CFStringRef) CFDictionaryGetValue( recMapDict, CFSTR( kXMLStdNameKey ) );
				if ( cfStdName == NULL || CFGetTypeID( cfStdName ) != CFStringGetTypeID() )
					continue;
				
				// now let's extract the NativeMap and see what it is.. if it isn't a CFArray, it's not usable
				CFArrayRef cfNativeArray = (CFArrayRef) CFDictionaryGetValue( recMapDict, CFSTR( kXMLNativeMapArrayKey ) );
				if ( cfNativeArray == NULL || CFGetTypeID(cfNativeArray) != CFArrayGetTypeID() )
					continue;
				
				// the array can contain either a dictionary or a string, but we'll make them all dictionaries here..
				CFIndex iNativeMapCount = CFArrayGetCount( cfNativeArray );
				if( iNativeMapCount == 0 )
					continue;
				
				// create a new array with the maximum entries of the current array, since we shouldn't exceed
				CFMutableArrayRef cfNewNativeArray = CFArrayCreateMutable( kCFAllocatorDefault, iNativeMapCount, &kCFTypeArrayCallBacks );
				
				for( CFIndex iNativeIndex = 0; iNativeIndex < iNativeMapCount; iNativeIndex++ )
				{
					// we only have one map per record type
					CFMutableDictionaryRef	cfDictRef			= (CFMutableDictionaryRef) CFArrayGetValueAtIndex( cfNativeArray, iNativeIndex );
					CFMutableDictionaryRef	cfValidNativeDict	= NULL;
					
					// technically we can't have NULL's but just in case..
					if( CFGetTypeID(cfDictRef) == CFDictionaryGetTypeID() )
					{
						CFArrayRef cfObjectClasses = (CFArrayRef) CFDictionaryGetValue( cfDictRef, CFSTR(kXMLObjectClasses) );
						if( cfObjectClasses != NULL && CFGetTypeID(cfObjectClasses) != CFArrayGetTypeID() )
							continue;
						
						CFStringRef cfSearchBase = (CFStringRef) CFDictionaryGetValue( cfDictRef, CFSTR(kXMLSearchBase) );
						if( cfSearchBase != NULL && CFGetTypeID(cfSearchBase) != CFStringGetTypeID() )
							cfSearchBase = NULL; // don't use it..
						
						CFBooleanRef cfSearchScope = (CFBooleanRef) CFDictionaryGetValue( cfDictRef, CFSTR(kXMLOneLevelSearchScope) );
						if( cfSearchScope != NULL && CFGetTypeID(cfSearchScope) != CFBooleanGetTypeID() )
							cfSearchScope = NULL; // don't use it..
						
						CFStringRef cfGroupClasses = (CFStringRef) CFDictionaryGetValue( cfDictRef, CFSTR(kXMLGroupObjectClasses) );
						if( cfGroupClasses != NULL && CFGetTypeID(cfGroupClasses) != CFStringGetTypeID() )
							cfGroupClasses = NULL; // don't use it..
						
						// all that matters is whether or not we got object classes out of this..
						CFIndex	iObjClassCount = cfObjectClasses ? CFArrayGetCount( cfObjectClasses ) : 0;
						
						// not let's loop through and see if some aren't Strings, if so the list is bad, throw out
						for( CFIndex iObjClassIndex = 0; iObjClassIndex < iObjClassCount; iObjClassIndex++ )
						{
							if( CFGetTypeID(CFArrayGetValueAtIndex(cfObjectClasses, iObjClassIndex)) != CFStringGetTypeID() )
							{
								cfObjectClasses = NULL;
								break;
							}
						}
						
						// if we made it through the loop and still have cfObjectClasses then it is good, let's use it
						if( cfObjectClasses != NULL )
						{
							// let's allocate a dictionary to hold 4 items..
							cfValidNativeDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
							
							if( cfSearchBase != NULL )
								CFDictionarySetValue( cfValidNativeDict, CFSTR(kXMLSearchBase), cfSearchBase );
							if( cfSearchScope != NULL )
								CFDictionarySetValue( cfValidNativeDict, CFSTR(kXMLOneLevelSearchScope), cfSearchScope );
							if( cfGroupClasses != NULL )
								CFDictionarySetValue( cfValidNativeDict, CFSTR(kXMLGroupObjectClasses), cfGroupClasses );
							
							CFDictionarySetValue( cfValidNativeDict, CFSTR(kXMLObjectClasses), cfObjectClasses );
						}
					}
					else if ( CFGetTypeID(cfDictRef) == CFStringGetTypeID() )
					{
						CFArrayRef	cfObjectClass = CFArrayCreate( kCFAllocatorDefault, (const void **)&cfDictRef, 1, &kCFTypeArrayCallBacks );
						
						// only 1 entry.. the objectClasses
						cfValidNativeDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
						
						// don't set kXMLSearchBase, kXMLOneLevelSearchScope or kXMLGroupObjectClasses speeds access later
						CFDictionarySetValue( cfValidNativeDict, CFSTR(kXMLObjectClasses), cfObjectClass );
						
						CFRelease( cfObjectClass );
						cfObjectClass = NULL;
					}
					
					// if we ended up with a new dictionary, let's add it to the new array we created
					if( cfValidNativeDict != NULL )
					{
						// add it to the new array
						CFArrayAppendValue( cfNewNativeArray, cfValidNativeDict );
						
						// clean up the ValidNativeDict
						CFRelease( cfValidNativeDict );
						cfValidNativeDict = NULL;
					}
				}
				
				// if we ended up with a new array, then we need to put it in the outgoing dictionary accordingly
				if( CFArrayGetCount( cfNewNativeArray ) > 0 )
				{
					// maximum of 2 entries - Native Array and Attribute Map
					CFMutableDictionaryRef cfNewMap = CFDictionaryCreateMutable( kCFAllocatorDefault, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
					
					// now let's get a good copy of the attribute map list.. by normalizing with the global list
					CFArrayRef cfArray = (CFArrayRef) CFDictionaryGetValue( recMapDict, CFSTR(kXMLAttrTypeMapArrayKey) );
					if( cfArray != NULL )
					{
						CFDictionaryRef cfAttribMap = CreateNormalizedAttributeMap( cfArray, cfGlobalAttrMap );
						if( cfAttribMap != NULL )
						{
							CFDictionarySetValue( cfNewMap, CFSTR(kXMLAttrTypeMapDictKey), cfAttribMap );
							CFRelease( cfAttribMap );
							cfAttribMap = NULL;
						}
					}
					else if( cfGlobalAttrMap )
					{
						CFDictionarySetValue( cfNewMap, CFSTR(kXMLAttrTypeMapDictKey), cfGlobalAttrMap );
					}
					
					// add the native array back to the new map dictionary
					CFDictionarySetValue( cfNewMap, CFSTR(kXMLNativeMapArrayKey), cfNewNativeArray );
					
					// add the new map to the new outRecMapDict with dsStdName as the key
					CFDictionarySetValue( outRecMapDict, cfStdName, cfNewMap );
					
					DSCFRelease( cfNewMap );
				}
				
				// release it cause we are done with it
				DSCFRelease( cfNewNativeArray );
			}
		}
		
		// normalized global map was temporary....
		DSCFRelease( cfGlobalAttrMap );
	}
	
	// if we have a dictionary, but it is empty, let's release and NULL
	if( outRecMapDict != NULL && CFDictionaryGetCount(outRecMapDict) == 0 )
	{
		CFRelease( outRecMapDict );
		outRecMapDict = NULL;
	}
	
	return outRecMapDict;
} //CreateNormalizedRecordAttrMap

// --------------------------------------------------------------------------------
//	* GetVersion
// --------------------------------------------------------------------------------

char *CLDAPv3Configs::GetVersion ( CFDictionaryRef configDict )
{
	char			   *outVersion	= nil;
	CFStringRef			cfStringRef	= nil;

	cfStringRef = (CFStringRef)CFDictionaryGetValue( configDict, CFSTR( kXMLLDAPVersionKey ) );
	if ( cfStringRef != nil && CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
	{
		uInt32 uiLength = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfStringRef), kCFStringEncodingUTF8 ) + 1;
		outVersion = (char *) calloc( sizeof(char), uiLength );
		CFStringGetCString( cfStringRef, outVersion, uiLength, kCFStringEncodingUTF8 );
	}

	// return if nil or not
	return( outVersion );

} // GetVersion

#pragma mark -
#pragma mark Server Mappings Utility Functions

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
	
			// set some network level timeouts here
			SetNetworkTimeoutsForHost( serverHost, kLDAPDefaultNetworkTimeoutInSeconds );
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
			
			ldap_set_option( serverHost, LDAP_OPT_REFERRALS, (inReferrals?LDAP_OPT_ON:LDAP_OPT_OFF) );
			
			if (inMapSearchBase == nil)
			{
				ldapMsgId = ldap_search( serverHost, "", LDAP_SCOPE_SUBTREE, "(&(objectclass=organizationalUnit)(ou=macosxodconfig))", attrs, 0);
			}
			else
			{
				ldapMsgId = ldap_search( serverHost, inMapSearchBase, LDAP_SCOPE_SUBTREE, "(&(objectclass=organizationalUnit)(ou=macosxodconfig))", attrs, 0);
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
				tv.tv_sec	= kLDAPDefaultOpenCloseTimeoutInSeconds;
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
				syslog(LOG_ALERT,"DSLDAPv3PlugIn: Retrieval of Server Mappings for [%s] LDAP server has timed out.", inServer);
				if ( result != nil )
				{
					ldap_msgfree( result );
					result = nil;
				}
			}
			else
			{
				siResult = eDSRecordNotFound;
				syslog(LOG_ALERT,"DSLDAPv3PlugIn: Server Mappings for [%s] LDAP server not found.", inServer);
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
	CFPropertyListRef		configPropertyList	= nil;
	CFDictionaryRef			serverConfigDict	= nil;
	char				   *server				= nil;
	int						portNumber			= 389;
	int						openCloseTO			= kLDAPDefaultOpenCloseTimeoutInSeconds;
	CFStringRef				cfStringRef			= nil;
	CFBooleanRef			cfBool				= nil;
	CFNumberRef				cfNumber			= nil;
	char				   *mapSearchBase		= nil;
	bool					bIsSSL				= false;
	bool					bLDAPv2ReadOnly		= false;
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
																	NULL);
			
			if (configPropertyList != nil )
			{
				//make the propertylist a dict
				if ( CFDictionaryGetTypeID() == CFGetTypeID( configPropertyList ) )
				{
					serverConfigDict = (CFDictionaryRef) configPropertyList;
				}
				
				if (serverConfigDict != nil)
				{					
					// retrieve all the relevant values (mapsearchbase, IsSSL)
					// to enable server mapping write
					//need to get the server name first
					cfStringRef = (CFStringRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLServerKey ) );
					if ( cfStringRef != nil && CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
					{
						uInt32 uiLength = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfStringRef), kCFStringEncodingUTF8 ) + 1;
						server = (char *) calloc( sizeof(char), uiLength );
						CFStringGetCString( cfStringRef, server, uiLength, kCFStringEncodingUTF8 );
					}

					cfNumber = (CFNumberRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLOpenCloseTimeoutSecsKey ) );
					if ( cfNumber != nil )
					{
						CFNumberGetValue(cfNumber, kCFNumberIntType, &openCloseTO);
					}

					cfBool= (CFBooleanRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLIsSSLFlagKey ) );
					if (cfBool != nil)
					{
						bIsSSL = CFBooleanGetValue( cfBool );
						if (bIsSSL)
						{
							portNumber = LDAPS_PORT;
						}
					}

					cfBool= (CFBooleanRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLLDAPv2ReadOnlyKey ) );
					if (cfBool != nil)
					{
						bLDAPv2ReadOnly = CFBooleanGetValue( cfBool );
					}

					cfNumber = (CFNumberRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLPortNumberKey ) );
					if ( cfNumber != nil )
					{
						CFNumberGetValue(cfNumber, kCFNumberIntType, &portNumber);
					}
					
					cfStringRef = (CFStringRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLMapSearchBase ) );
					if ( cfStringRef != nil && CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
					{
						uInt32 uiLength = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfStringRef), kCFStringEncodingUTF8 ) + 1;
						mapSearchBase = (char *) calloc( sizeof(char), uiLength );
						CFStringGetCString( cfStringRef, mapSearchBase, uiLength, kCFStringEncodingUTF8 );
					}
					
					// don't release the serverConfigDict since it is the cast configPropertyList
				}//if (serverConfigDict != nil)
				
				CFRelease(configPropertyList); // built from Create on XML data so okay to dealloc here
				configPropertyList = nil;
			}//if (configPropertyList != nil )

			if (bLDAPv2ReadOnly) throw( (sInt32)eDSReadOnly); //if configured as LDAPv2 then read only error is returned
																//Directory Access should check internally before it ever makes the custom call that calls this

			serverHost = ldap_init( server, portNumber );
			
			if ( serverHost == nil ) throw( (sInt32)eDSCannotAccessSession );
			if ( bIsSSL )
			{
				int ldapOptVal = LDAP_OPT_X_TLS_HARD;
				ldap_set_option(serverHost, LDAP_OPT_X_TLS, &ldapOptVal);
			}
			
			// set some network level timeouts here
			SetNetworkTimeoutsForHost( serverHost, kLDAPDefaultNetworkTimeoutInSeconds );
			
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
	CFPropertyListRef		configPropertyList	= nil;
	CFMutableDictionaryRef	serverConfigDict	= nil;
	char				   *configVersion		= nil;
	CFStringRef				cfStringRef			= nil;
	CFBooleanRef			cfBool				= false;
	char				   *mapSearchBase		= nil;
	bool					bIsSSL				= false;
	bool					bServerMappings		= false;
	bool					bUseConfig			= false;
	bool					bReferrals			= true;
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
																    NULL);
			
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
							//assume that the extracted strings will be significantly less than 1024 characters
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
					
								cfBool = (CFBooleanRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLServerMappingsFlagKey ) );
								if (cfBool != nil)
								{
									bServerMappings = CFBooleanGetValue( cfBool );
									//CFRelease( cfBool ); // no since pointer only from Get
								}
					
								if (bServerMappings)
								{
									// retrieve all the relevant values (server, portNumber, mapsearchbase, IsSSL)
									// to enable server mapping write
									
									cfStringRef = (CFStringRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLServerKey ) );
									if ( cfStringRef != nil && CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
									{
										uInt32 uiLength = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfStringRef), kCFStringEncodingUTF8 ) + 1;
										server = (char *) calloc( sizeof(char), uiLength );
										CFStringGetCString( cfStringRef, server, uiLength, kCFStringEncodingUTF8 );
									}

									cfBool= (CFBooleanRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLIsSSLFlagKey ) );
									if (cfBool != nil)
									{
										bIsSSL = CFBooleanGetValue( cfBool );
										if (bIsSSL)
										{
											portNumber = LDAPS_PORT; // default for SSL ie. if no port given below
										}
									}

									cfBool= (CFBooleanRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLReferralFlagKey ) );
									if (cfBool != nil)
									{
										bReferrals = CFBooleanGetValue( cfBool );
									}
									
									cfNumber = (CFNumberRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLPortNumberKey ) );
									if ( cfNumber != nil )
									{
										CFNumberGetValue(cfNumber, kCFNumberIntType, &portNumber);
									}

									cfStringRef = (CFStringRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLMapSearchBase ) );
									if ( cfStringRef != nil && CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
									{
										uInt32 uiLength = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfStringRef), kCFStringEncodingUTF8 ) + 1;
										mapSearchBase = (char *) calloc( sizeof(char), uiLength );
										CFStringGetCString( cfStringRef, mapSearchBase, uiLength, kCFStringEncodingUTF8 );
									}
								}
					
							}// if ( bUseConfig )
                        }                        
						free( configVersion );
						configVersion = nil;
						
					}//if (configVersion != nil)
					
					// don't release the serverConfigDict since it is the cast configPropertyList
					
				}//if (serverConfigDict != nil)
				
				CFRelease(configPropertyList); // built from Create on XML data so okay to dealloc here
				configPropertyList = nil;
	
			}//if (configPropertyList != nil )
			
			outMappings = RetrieveServerMappings( server, mapSearchBase, portNumber, bIsSSL, bReferrals );

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

#pragma mark -
#pragma mark Extracting Maps from Configurations

// ---------------------------------------------------------------------------
//	* ExtractRecMap
// ---------------------------------------------------------------------------

char* CLDAPv3Configs::ExtractRecMap( const char *inRecType, CFDictionaryRef inRecordTypeMapCFDict, int inIndex, bool *outOCGroup, CFArrayRef *outOCListCFArray, ber_int_t* outScope )
{
	char	*outResult	= NULL;

	// the map dictionary is normalized, no need to check types of values
	if ( (inRecordTypeMapCFDict != NULL) && (inRecType != NULL) )
	{
		CFStringRef		cfRecTypeRef	= CFStringCreateWithCString( kCFAllocatorDefault, inRecType, kCFStringEncodingUTF8 );
		CFDictionaryRef cfRecordMap		= (CFDictionaryRef) CFDictionaryGetValue( inRecordTypeMapCFDict, cfRecTypeRef );
		
		// if we got a map, we can continue..
		if ( cfRecordMap != NULL )
		{
			CFArrayRef		cfNativeArray	= (CFArrayRef) CFDictionaryGetValue( cfRecordMap, CFSTR(kXMLNativeMapArrayKey) );
			
			if ( inIndex <= CFArrayGetCount(cfNativeArray) )
			{
				CFDictionaryRef cfCurrentMap	= (CFDictionaryRef) CFArrayGetValueAtIndex( cfNativeArray, inIndex-1 );
				CFStringRef		searchBase		= (CFStringRef) CFDictionaryGetValue( cfCurrentMap, CFSTR(kXMLSearchBase) );
				
				if (searchBase != NULL)
				{
					uInt32 uiLength = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(searchBase), kCFStringEncodingUTF8 ) + 1;
					outResult = (char *) calloc( sizeof(char), uiLength );
					CFStringGetCString( searchBase, outResult, uiLength, kCFStringEncodingUTF8 );
				}
				
				//now deal with the objectclass entries if appropriate
				if( outOCListCFArray != NULL && outOCGroup != NULL )
				{
					*outOCGroup = 0;
					
					CFArrayRef objectClasses = (CFArrayRef)CFDictionaryGetValue( cfCurrentMap, CFSTR( kXMLObjectClasses ) );
					if ( objectClasses != NULL )
					{
						CFStringRef groupOCString = (CFStringRef)CFDictionaryGetValue( cfCurrentMap, CFSTR( kXMLGroupObjectClasses ) );
						if ( groupOCString != NULL && CFStringCompare( groupOCString, CFSTR("AND"), 0 ) == kCFCompareEqualTo )
						{
							*outOCGroup = 1;
						}
						*outOCListCFArray = CFArrayCreateCopy(kCFAllocatorDefault, objectClasses);
					}// if (objectClasses != NULL)
				}// if(outOCListCFArray != NULL && outOCGroup != NULL)
				
				if (outScope != NULL)
				{
					CFBooleanRef cfBoolRef = (CFBooleanRef)CFDictionaryGetValue( cfCurrentMap, CFSTR( kXMLOneLevelSearchScope ) );
					
					if (cfBoolRef != NULL && CFBooleanGetValue(cfBoolRef))
					{
						*outScope = LDAP_SCOPE_ONELEVEL;
					}
					else
					{
						*outScope = LDAP_SCOPE_SUBTREE;
					}
				}// if (outScope != NULL)				
			} //inIndex <= CFArrayGetCount(cfNativeArray)
		} //cfRecordMap != NULL
		
		DSCFRelease(cfRecTypeRef);
		
	} // if (inRecordTypeMapCFDict != NULL) ie. a dictionary of Record Maps exists
	
	return( outResult );

} // ExtractRecMap


// ---------------------------------------------------------------------------
//	* ExtractAttrMap
// ---------------------------------------------------------------------------

char* CLDAPv3Configs::ExtractAttrMap( const char *inRecType, const char *inAttrType, CFDictionaryRef inRecordTypeMapCFDict, int inIndex )
{
	char	*outResult	= NULL;

	// dictionaries are normalized, everything will exist as expected
	if ( (inRecordTypeMapCFDict != NULL) && (inRecType != NULL) && (inAttrType != NULL) && (inIndex >= 1) )
	{
		CFDictionaryRef cfRecordMap		= NULL;
		CFStringRef		cfRecTypeRef	= CFStringCreateWithCString(kCFAllocatorDefault, inRecType, kCFStringEncodingUTF8);
		
		if ( cfRecTypeRef != NULL )
		{
			fConfigMapMutex.Wait();
			
			cfRecordMap = (CFDictionaryRef) CFDictionaryGetValue( inRecordTypeMapCFDict, cfRecTypeRef );
			
			// if we got a map, we can continue..
			if ( cfRecordMap != NULL )
			{
				CFDictionaryRef	cfAttrMapDictRef	= (CFDictionaryRef) CFDictionaryGetValue( cfRecordMap, CFSTR(kXMLAttrTypeMapDictKey) );
				
				// if a specific map is available..
				if( cfAttrMapDictRef != NULL )
				{
					CFArrayRef	cfMapArray		= NULL;
					CFStringRef	cfAttrTypeRef	= CFStringCreateWithCString( kCFAllocatorDefault, inAttrType, kCFStringEncodingUTF8 );
					
					if ( cfAttrTypeRef != NULL )
					{
						cfMapArray = (CFArrayRef) CFDictionaryGetValue( cfAttrMapDictRef, cfAttrTypeRef );
						
						// now let's see if our index is within our list of attributes..
						if ( cfMapArray != NULL && inIndex <= CFArrayGetCount(cfMapArray) )
						{
							CFStringRef	nativeMapString = (CFStringRef) CFArrayGetValueAtIndex( cfMapArray, inIndex - 1 );
							
							uInt32 uiLength = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(nativeMapString), kCFStringEncodingUTF8 ) + 1;
							outResult = (char *) calloc( sizeof(char), uiLength );
							CFStringGetCString( nativeMapString, outResult, uiLength, kCFStringEncodingUTF8 );
						}
						
						DSCFRelease(cfAttrTypeRef);
					}
				}
			}//if ( cfRecordMap != NULL )
			
			DSCFRelease(cfRecTypeRef);
			
			fConfigMapMutex.Signal();
		}
	} // if (inRecordTypeMapCFArray != nil) ie. an array of Record Maps exists
	
	return( outResult );

} // ExtractAttrMap


// ---------------------------------------------------------------------------
//	* ExtractStdAttr
// ---------------------------------------------------------------------------

char* CLDAPv3Configs::ExtractStdAttrName( char *inRecType, CFDictionaryRef inRecordTypeMapCFDict, int &inputIndex )
{
	char	*outResult	= NULL;
	
	// this routine gets the next standard attribute at an index for a given record type... 
	if ( (inRecordTypeMapCFDict != NULL) && (inRecType != NULL) && (inputIndex >= 1) )
	{
		CFDictionaryRef cfRecordMap		= NULL;
		CFStringRef		cfRecTypeRef	= CFStringCreateWithCString(kCFAllocatorDefault, inRecType, kCFStringEncodingUTF8);
		
		if ( cfRecTypeRef != NULL )
		{
			fConfigMapMutex.Wait();
			
			cfRecordMap = (CFDictionaryRef) CFDictionaryGetValue( inRecordTypeMapCFDict, cfRecTypeRef );
			
			// if we got a map, we can continue..
			if ( cfRecordMap != NULL )
			{
				//now we can retrieve the map dictionary
				CFDictionaryRef cfAttrMapDict	= (CFDictionaryRef) CFDictionaryGetValue( cfRecordMap, CFSTR( kXMLAttrTypeMapDictKey ) );
				
				// now we have to get values & keys so we can step through them...
				// get the native map array of labels next
				if (cfAttrMapDict != NULL)
				{
					CFIndex		iTotalEntries	= CFDictionaryGetCount( cfAttrMapDict );
					
					if ( inputIndex <= iTotalEntries )
					{
						CFStringRef	*keys = (CFStringRef *) calloc( iTotalEntries, sizeof(CFStringRef) );
						if ( keys != NULL )
						{
							CFDictionaryGetKeysAndValues( cfAttrMapDict, (const void **)keys, NULL );
							
							uInt32 uiLength = (uInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(keys[inputIndex - 1]), kCFStringEncodingUTF8 ) + 1;
							outResult = (char *) calloc( sizeof(char), uiLength );
							CFStringGetCString( keys[inputIndex - 1], outResult, uiLength, kCFStringEncodingUTF8 );
							
							DSFree( keys );
						}
					}
				}
			}
			
			CFRelease(cfRecTypeRef);
			
			fConfigMapMutex.Signal();
		}
	} // if (inRecordTypeMapCFArray != nil) ie. an array of Record Maps exists
	
	return( outResult );

} // ExtractStdAttr


// ---------------------------------------------------------------------------
//	* AttrMapsCount
// ---------------------------------------------------------------------------

int CLDAPv3Configs::AttrMapsCount( const char *inRecType, const char *inAttrType, CFDictionaryRef inRecordTypeMapCFDict )
{
	int	outCount	= 0;

	// dictionaries are normalized, everything will exist as expected
	if ( (inRecordTypeMapCFDict != NULL) && (inRecType != NULL) && (inAttrType != NULL) )
	{
		fConfigMapMutex.Wait();
		
		CFStringRef		cfRecTypeRef	= CFStringCreateWithCString(kCFAllocatorDefault, inRecType, kCFStringEncodingUTF8);
		CFDictionaryRef cfRecordMap		= (CFDictionaryRef) CFDictionaryGetValue( inRecordTypeMapCFDict, cfRecTypeRef );
		
		// if we got a map, we can continue..
		if ( cfRecordMap != NULL )
		{
			CFDictionaryRef	cfAttrMapDictRef	= (CFDictionaryRef) CFDictionaryGetValue( cfRecordMap, CFSTR(kXMLAttrTypeMapDictKey) );
			
			// if a specific map is available..
			if ( cfAttrMapDictRef != NULL )
			{
				CFArrayRef	cfMapArray		= NULL;
				CFStringRef	cfAttrTypeRef	= CFStringCreateWithCString(kCFAllocatorDefault, inAttrType, kCFStringEncodingUTF8);
				if ( cfAttrTypeRef != NULL )
				{
					cfMapArray = (CFArrayRef) CFDictionaryGetValue( cfAttrMapDictRef, cfAttrTypeRef );
				
					// now let's see if our index is within our list of attributes..
					if ( cfMapArray != NULL )
					{
						outCount = CFArrayGetCount( cfMapArray );
					}
					CFRelease(cfAttrTypeRef);
				}
			}
		}
		CFRelease(cfRecTypeRef);
		
		fConfigMapMutex.Signal();
	} // if (inRecordTypeMapCFDict != NULL) ie. an array of Record Maps exists
	
	return( outCount );

} // AttrMapsCount

#pragma mark -
#pragma mark Utility Function

// ---------------------------------------------------------------------------
//	* CreatePrefDirectory
// ---------------------------------------------------------------------------

bool CLDAPv3Configs::CreatePrefDirectory( void )
{
	char		*filenameString		= "/Library/Preferences/DirectoryService/DSLDAPv3PlugInConfig.plist";
	int			siResult			= eDSNoErr;
    struct stat statResult;
	
	DBGLOG( kLogPlugin, "CLDAPv3Configs: Checking for LDAP XML config file:" );
	DBGLOG1( kLogPlugin, "CLDAPv3Configs: %s", filenameString );
	
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
				
			XMLConfigLock();
	
			if (fXMLData != NULL)
			{
				sV3Config = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, fXMLData, kCFPropertyListMutableContainers, NULL );
			}
	
			XMLConfigUnlock();
			
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
								tV3ConfigEntries = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
								CFDictionarySetValue( sV3Config, tConfigKey, tV3ConfigEntries );
								CFRelease( tV3ConfigEntries );
							}
							
							// let's append the new config to the new list
							CFArrayAppendValue( tV3ConfigEntries, tV2ConfigEntry );
							
							// Now update our Config file on disk and in memory
							CFDataRef aXMLData = (CFDataRef) CFPropertyListCreateXMLData( kCFAllocatorDefault, sV3Config );
							
							SetXMLConfig(aXMLData);
							
							CFRelease(aXMLData);
							aXMLData = 0;

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
