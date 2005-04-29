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
 * @header CPluginConfig
 */

#include <sys/stat.h>				// for file and dir stat calls
#include <syslog.h>					// for syslog()
#include <CoreFoundation/CFDictionary.h>

#include "CPluginConfig.h"
#include "CServerPlugin.h"
#include "CPluginList.h"
#include "ServerControl.h"
#include "CFile.h"
#include "CLog.h"

extern	bool			gServerOS;
extern  uInt32			gRefCountWarningLimit;
extern  uInt32			gDelayFailedLocalAuthReturnsDeltaInSeconds;
extern	uInt32			gMaxHandlerThreadCount;

//--------------------------------------------------------------------------------------------------
//	* CPluginConfig ()
//
//--------------------------------------------------------------------------------------------------

CPluginConfig::CPluginConfig ( void )
{
	fPlistRef		= nil;
	fDictRef		= nil;
} // CPluginConfig


//--------------------------------------------------------------------------------------------------
//	* ~CPluginConfig ()
//
//--------------------------------------------------------------------------------------------------

CPluginConfig::~CPluginConfig()
{
	if ( fPlistRef != nil )
	{
		::CFRelease( fPlistRef );
		fPlistRef = nil;
	}
} // ~CPluginConfig



//--------------------------------------------------------------------------------------------------
//	* Initialize ()
//
//--------------------------------------------------------------------------------------------------

sInt32 CPluginConfig::Initialize ( void )
{
	bool			bSuccess	= false;
	int				iResult		= 0;
	uInt32			uiDataSize	= 0;
	char		   *pData		= nil;
	CFile		   *pFile		= nil;
	struct stat		statbuf;
	CFDataRef		dataRef		= nil;
	CFStringRef		keyStrRef	= nil;
	unsigned char   cfNumBool	= false;
	CFNumberRef		cfNumber	= 0;
	

	try
	{
		// Does the config file exist
		iResult = ::stat( kConfigFilePath, &statbuf );
		if ( iResult == 0 )
		{
			// Attempt to get config info from file
			pFile = new CFile( kConfigFilePath );
			if ( pFile != nil )
			{
				if ( (pFile->is_open()) && (pFile->FileSize() > 0) )
				{
					// Allocate space for the file data
					pData = (char *)::malloc( pFile->FileSize() + 1 );
					if ( pData != nil )
					{
						// Read from the config file
						uiDataSize = pFile->ReadBlock( pData, pFile->FileSize() );
						dataRef = ::CFDataCreate( nil, (const uInt8 *)pData, uiDataSize );
						if ( dataRef != nil )
						{
							// Is it valid XML data
							fPlistRef = ::CFPropertyListCreateFromXMLData( kCFAllocatorDefault, dataRef, kCFPropertyListMutableContainersAndLeaves, nil );
							if ( fPlistRef != nil )
							{
								// Is it a plist type
								if ( ::CFDictionaryGetTypeID() == ::CFGetTypeID( fPlistRef ) )
								{
									fDictRef = (CFMutableDictionaryRef)fPlistRef;

									bSuccess = true;
								}
							}
							CFRelease( dataRef );
							dataRef = nil;
						}
						free( pData );
						pData = nil;
					}
				}
				
				delete( pFile );
				pFile = nil;
			}
		}

		if ( bSuccess == true && fDictRef != NULL )
		{
			// Check versioning here.  If file version is old, we need to convert to the latest
			CFStringRef		configVersion = (CFStringRef)CFDictionaryGetValue( fDictRef, CFSTR(kVersionKey) );
			
			if ( configVersion == NULL )
			{
				// this isn't properly configured, throw this away so we can start with a fresh default
				CFRelease( fDictRef );
				fDictRef = NULL;
				bSuccess = false;

				SRVRLOG( kLogApplication, "Plugin configuration file has no version, resetting to current defaults." );
			}
			else
			{
				Boolean		bUpgradedConfigVersion = false;
				
				CFRetain( configVersion );  // release at end
				
				if ( CFStringCompare( configVersion, CFSTR("1.1"), kCFCompareNumerically ) == kCFCompareLessThan )
				{
					// This is a pre 1.1 version file, upgrade to 1.1 defaults
					// First the version value
					CFDictionarySetValue( fDictRef, CFSTR(kVersionKey), CFSTR("1.1") );
					
					// Now the change, we want to force AppleTalk plugin to be active by default
					CFDictionarySetValue( fDictRef, CFSTR(kAppleTalkPluginKey), CFSTR(kActiveValue) );
					
					bUpgradedConfigVersion = true;
					SRVRLOG( kLogApplication, "Plugin configuration file upgraded to 1.1, AppleTalk plugin now Active." );
				}
/*				
				if ( CFStringCompare( configVersion, CFSTR("1.2"), kCFCompareNumerically ) == kCFCompareLessThan )
				{
					// This is a pre 1.2 version file, upgrade from 1.1 to 1.2 defaults
					...
					// This is a placeholder for future versioning changes.  If we always check the version, we can
					// do the same upgrades between versions so that whatever the current version state is will be
					// consistent.

					bUpgradedConfigVersion = true;
					SRVRLOG( kLogApplication, "Plugin configuration file upgraded to 1.2." );
				}
*/

				if ( bUpgradedConfigVersion == true )
				{
					SaveConfigData();
				}
				
				CFRelease( configVersion );  // release at end
			}
		}
		
		// Either the file didn't exist or we didn't have valid data in the file
		if ( bSuccess == false )
		{
			// Does the Jaguar Update file exist indicating this was an upgrade install?
			// We don't make AppleTalk inactive by default on an upgrade install
			iResult = ::stat( kJaguarUpdateFilePath, &statbuf );
			if (gServerOS)
			{
				if (iResult == 0)
				{
					uiDataSize = ::strlen( kServerDefaultUpgradeConfig );
					dataRef = ::CFDataCreate( nil, (const uInt8 *)kServerDefaultUpgradeConfig, uiDataSize );
				}
				else
				{
					uiDataSize = ::strlen( kServerDefaultConfig );
					dataRef = ::CFDataCreate( nil, (const uInt8 *)kServerDefaultConfig, uiDataSize );
				}
			}
			else
			{
				if (iResult == 0)
				{
					uiDataSize = ::strlen( kDefaultUpgradeConfig );
					dataRef = ::CFDataCreate( nil, (const uInt8 *)kDefaultUpgradeConfig, uiDataSize );
				}
				else
				{
					uiDataSize = ::strlen( kDefaultConfig );
					dataRef = ::CFDataCreate( nil, (const uInt8 *)kDefaultConfig, uiDataSize );
				}
			}
			if ( dataRef != nil )
			{
				fPlistRef = (CFMutableDictionaryRef)::CFPropertyListCreateFromXMLData( kCFAllocatorDefault, dataRef, kCFPropertyListMutableContainersAndLeaves, nil );
				if ( fPlistRef != nil )
				{
					if ( ::CFDictionaryGetTypeID() == ::CFGetTypeID( fPlistRef ) )
					{
						fDictRef = (CFMutableDictionaryRef)fPlistRef;

						//make the new Active Directory plugin InActive by default if not already installed and setup
						CFStringRef keyStrRef = nil;
						keyStrRef = ::CFStringCreateWithCString( kCFAllocatorDefault, "Active Directory", kCFStringEncodingMacRoman );
						if ( keyStrRef != nil )
						{
							bool bFound = false;
							bFound =::CFDictionaryContainsKey( fDictRef, keyStrRef );
							if ( bFound == false )
							{
								::CFDictionarySetValue( fDictRef, keyStrRef, CFSTR( kInactiveValue ) );
								SaveConfigData();
							}
							::CFRelease( keyStrRef );
							keyStrRef = nil;
						}

						bSuccess = true;
						
						//let's make sure we don't run into these non-config file problems again
						//ie. write the file since we have it figured out
						SaveConfigData();
					}
				}
				CFRelease( dataRef );
				dataRef = nil;
			}
		}

		if (fDictRef != nil)
		{
			keyStrRef = ::CFStringCreateWithCString( NULL, kTooManyReferencesWarningCount, kCFStringEncodingMacRoman );
			if ( keyStrRef != nil )
			{
				if ( CFDictionaryContainsKey( fDictRef, keyStrRef ) )
				{
					cfNumber = (CFNumberRef)CFDictionaryGetValue( fDictRef, keyStrRef );
					if ( cfNumber != nil )
					{
						cfNumBool = CFNumberGetValue(cfNumber, kCFNumberIntType, &gRefCountWarningLimit);
						//CFRelease(cfNumber); // no since pointer only from Get
					}
				}
				::CFRelease( keyStrRef );
				keyStrRef = nil;
			}
			keyStrRef = ::CFStringCreateWithCString( NULL, kDelayFailedLocalAuthReturnsDeltaInSeconds, kCFStringEncodingMacRoman );
			if ( keyStrRef != nil )
			{
				if ( CFDictionaryContainsKey( fDictRef, keyStrRef ) )
				{
					cfNumber = (CFNumberRef)CFDictionaryGetValue( fDictRef, keyStrRef );
					if ( cfNumber != nil )
					{
						cfNumBool = CFNumberGetValue(cfNumber, kCFNumberIntType, &gDelayFailedLocalAuthReturnsDeltaInSeconds);
						//CFRelease(cfNumber); // no since pointer only from Get
					}
				}
				::CFRelease( keyStrRef );
				keyStrRef = nil;
			}
			keyStrRef = ::CFStringCreateWithCString( NULL, kMaxHandlerThreadCount, kCFStringEncodingMacRoman );
			if ( keyStrRef != nil )
			{
				if ( CFDictionaryContainsKey( fDictRef, keyStrRef ) )
				{
					cfNumber = (CFNumberRef)CFDictionaryGetValue( fDictRef, keyStrRef );
					if ( cfNumber != nil )
					{
						cfNumBool = CFNumberGetValue(cfNumber, kCFNumberIntType, &gMaxHandlerThreadCount);
						//CFRelease(cfNumber); // no since pointer only from Get
						if (gMaxHandlerThreadCount < kMaxHandlerThreads)
						{
							gMaxHandlerThreadCount = kMaxHandlerThreads;
							syslog(LOG_ALERT,"Maximum handler thread count cannot be set less than %u", kMaxHandlerThreads);
						}
						else if (gMaxHandlerThreadCount > 256)
						{
							gMaxHandlerThreadCount = kMaxHandlerThreads;
							syslog(LOG_ALERT,"Maximum handler thread count cannot be set greater than 256 so resetting to default of %u", kMaxHandlerThreads);
						}
					}
				}
				::CFRelease( keyStrRef );
				keyStrRef = nil;
			}
		}
	}
	
	catch ( ... )
	{
	}

	return( noErr );

} // Initialize


//--------------------------------------------------------------------------------------------------
//	* GetPluginState ()
//
//--------------------------------------------------------------------------------------------------

ePluginState CPluginConfig::GetPluginState ( const char *inPluginName )
{
	ePluginState	epsResult	= kActive;		// If we don't explicitly have a plugin turned off, it should be active
	bool			bFound		= false;
	CFStringRef		cfStringRef	= nil;
	char		   *pValue		= nil;
	CFStringRef		keyStrRef	= nil;

	if (  (fDictRef != nil) && (inPluginName != nil) )
	{
		keyStrRef = ::CFStringCreateWithCString( NULL, inPluginName, kCFStringEncodingMacRoman );
		if ( keyStrRef != nil )
		{
			bFound = ::CFDictionaryContainsKey( fDictRef, keyStrRef );
			if ( bFound == true )
			{
				cfStringRef = (CFStringRef)::CFDictionaryGetValue( fDictRef, keyStrRef );
				if ( cfStringRef != nil )
				{
					if ( ::CFGetTypeID( cfStringRef ) == ::CFStringGetTypeID() )
					{
						pValue = (char *)::CFStringGetCStringPtr( cfStringRef, kCFStringEncodingMacRoman );
						if ( pValue != nil )
						{
							if ( ::strcmp( pValue, kInactiveValue ) == 0 )
							{
								epsResult = kInactive;
							}
						}
					}
				}
			}
			::CFRelease( keyStrRef );
			keyStrRef = nil;
		}
	}

	return( epsResult );

} // GetPluginState


//--------------------------------------------------------------------------------------------------
//	* SetPluginState ()
//
//--------------------------------------------------------------------------------------------------

sInt32 CPluginConfig::SetPluginState ( const char *inPluginName, const ePluginState inPluginState )
{
	CFStringRef		keyStrRef		= nil;

	if ( (fDictRef != nil) && (inPluginName != nil) )
	{
		CServerPlugin*	plugin = gPlugins->GetPlugInPtr( inPluginName, false );		// don't load plugin if it isn't already...
		
		if ( plugin )
			plugin->SetPluginState( inPluginState );
			
		keyStrRef = ::CFStringCreateWithCString( NULL, inPluginName, kCFStringEncodingMacRoman );
		if ( keyStrRef != nil )
		{
			if ( inPluginState == kActive )
			{
				::CFDictionarySetValue( fDictRef, keyStrRef, CFSTR( kActiveValue ) );
			}
			else if ( inPluginState == kInactive )
			{
				::CFDictionarySetValue( fDictRef, keyStrRef, CFSTR( kInactiveValue ) );
			}

			::CFRelease( keyStrRef );
			keyStrRef = nil;
		}
	}

	return( noErr );

} // SetPluginState


//--------------------------------------------------------------------------------------------------
//	* SaveConfigData ()
//
//--------------------------------------------------------------------------------------------------

sInt32 CPluginConfig::SaveConfigData ( void )
{
	CFDataRef dataRef = nil;
	int result = 0;;
	struct stat	statResult;

	if ( fDictRef != nil )
	{
		// Get a new data ref with data in dictionary
		dataRef = ::CFPropertyListCreateXMLData( NULL, fDictRef );
		if ( dataRef != nil )
		{
			//step 1- see if the file exists
			//if not then make sure the directories exist or create them
			//then create a new file if necessary
			result = ::stat( kConfigFilePath, &statResult );
			//if file does not exist
			if (result != eDSNoErr)
			{
				//move down the path from the system defined local directory and check if it exists
				//if not create it
				result = ::stat( "/Library/Preferences", &statResult );
				//if first sub directory does not exist
				if (result != eDSNoErr)
				{
					::mkdir( "/Library/Preferences", 0775 );
					::chmod( "/Library/Preferences", 0775 ); //above 0775 doesn't seem to work - looks like umask modifies it
				}
				result = ::stat( "/Library/Preferences/DirectoryService", &statResult );
				//if second sub directory does not exist
				if (result != eDSNoErr)
				{
					::mkdir( "/Library/Preferences/DirectoryService", 0775 );
					::chmod( "/Library/Preferences/DirectoryService", 0775 ); //above 0775 doesn't seem to work - looks like umask modifies it
				}
			}

			UInt8 *pData = (UInt8*)::calloc( CFDataGetLength(dataRef), 1 );
			CFDataGetBytes(	dataRef, CFRangeMake(0,CFDataGetLength(dataRef)), pData );
			if ( (pData != nil) && (pData[0] != 0) )
			{
				try
				{
					CFile *pFile = new CFile( kConfigFilePath, true );
					if ( pFile != nil )
					{
						if ( pFile->is_open() )
						{
							pFile->seteof( 0 );
		
							pFile->write( pData, CFDataGetLength(dataRef) );
						}
						
						delete( pFile );
						pFile = nil;
						
						::chmod( kConfigFilePath, 0600 );
					}
				}
				catch ( ... )
				{
				}
				free(pData);
			}

			CFRelease( dataRef );
			dataRef = nil;
		}
	}

	return( noErr );

} // SaveConfigData
