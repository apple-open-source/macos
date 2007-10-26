/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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


#include "PrivateTypes.h"
#include "CLDAPPlugInPrefs.h"
#include <sys/stat.h>
#include <PasswordServer/CPSUtilities.h>
#include "CLog.h"
#include "GetMACAddress.h"
#include "DSUtils.h"

#define kServiceTypeList		"afpserver,cifs,ftp,imap,host,HTTP,http,ipp,ldap,nfs,pop,smtp,smb,vnc,xgrid,XMPP,xmpp,vpn"

DSMutexSemaphore *gLDAPPrefsMutex = nil;


CLDAPPlugInPrefs::CLDAPPlugInPrefs()
{
	if ( gLDAPPrefsMutex == NULL )
		gLDAPPrefsMutex = new DSMutexSemaphore("CLDAPPluginPrefs::gLDAPPrefsMutex");
	
	mPrefs.version = CFSTR(kDSLDAPPrefs_CurrentVersion);
	mPrefs.configs = NULL;
	strlcpy( mPrefs.services, kServiceTypeList, sizeof(mPrefs.services) );
	
	if ( this->Load() != 0 ) {
		this->Save();
		DbgLog( kLogPlugin, "LDAPPlugInPrefs: Created a new LDAP XML config file because it did not exist" );
	}
}

CLDAPPlugInPrefs::~CLDAPPlugInPrefs()
{
	DSCFRelease( mPrefs.version );
	DSCFRelease( mPrefs.configs );
}

void
CLDAPPlugInPrefs::GetPrefs( DSPrefs *inOutPrefs )
{
	if ( inOutPrefs )
		memcpy( inOutPrefs, &mPrefs, sizeof(mPrefs) );
}

void
CLDAPPlugInPrefs::SetPrefs( DSPrefs *inPrefs )
{
	if ( inPrefs->version ) CFRetain( inPrefs->version );
	if ( inPrefs->configs ) CFRetain( inPrefs->configs );
	DSCFRelease( mPrefs.version );
	DSCFRelease( mPrefs.configs );
	
	mPrefs.version = inPrefs->version;
	mPrefs.configs = inPrefs->configs;
	
	strlcpy( mPrefs.services, inPrefs->services, sizeof(mPrefs.services) );
}


CFDataRef
CLDAPPlugInPrefs::GetPrefsXML( void )
{
	CFDictionaryRef prefsDict = this->LoadXML();
	CFDataRef xmlData = NULL;
	
	if ( prefsDict != NULL )
	{
		xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, (CFPropertyListRef)prefsDict );					
		CFRelease( prefsDict );
	}
	
	return xmlData;
}


// private
int
CLDAPPlugInPrefs::Load( void )
{
	int err = 0;
	CFStringRef serviceListString = NULL;
	CFDictionaryRef prefsDict = this->LoadXML();
	CFStringRef versionString = NULL;
	CFArrayRef configsArray = NULL;
	char serviceStr[256];
	
	if ( prefsDict != NULL )
	{
		DSCFRelease( mPrefs.version );
		DSCFRelease( mPrefs.configs );
		
		versionString = (CFStringRef) CFDictionaryGetValue( prefsDict, CFSTR(kDSLDAPPrefs_LDAPPlugInVersion) );
		if ( versionString != NULL ) {
			CFRetain( versionString );
			mPrefs.version = versionString;
		}
		
		configsArray = (CFArrayRef) CFDictionaryGetValue( prefsDict, CFSTR(kDSLDAPPrefs_LDAPServerConfigs) );
		if ( configsArray != NULL ) {
			CFRetain( configsArray );
			mPrefs.configs = configsArray;
		}
		
		serviceListString = (CFStringRef) CFDictionaryGetValue( prefsDict, CFSTR(kDSLDAPPrefs_ServicePrincipalTypes) );
		if ( serviceListString != NULL &&
			 CFStringGetCString(serviceListString, serviceStr, sizeof(serviceStr), kCFStringEncodingUTF8) )
		{
			strlcpy( mPrefs.services, serviceStr, sizeof(mPrefs.services) );
			
			// for install of interim Leopard builds, add vnc (everyone should have ldap by now)
			if ( strstr(serviceStr, "vnc") == NULL ) {
				strlcpy( mPrefs.services, kServiceTypeList, sizeof(mPrefs.services) );
				err = -1;
			}
		}
		
		if ( mPrefs.version == NULL || serviceListString == NULL )
			err = -1;
		
		CFRelease( prefsDict );
	}
	
	return err;
}


CFDictionaryRef
CLDAPPlugInPrefs::LoadXML( void )
{
	SInt32 errorCode = 0;
	CFDataRef xmlData = NULL;
	CFURLRef configFileURL = NULL;
	CFDictionaryRef prefsDict = NULL;

	gLDAPPrefsMutex->WaitLock();

	CFStringRef prefsFilePath = dsCreatePrefsFilename( kDSLDAPPrefsFilePath );
	if ( prefsFilePath != NULL )
	{
		if ( !CFStringGetCString(prefsFilePath, mPrefs.path, sizeof(mPrefs.path), kCFStringEncodingUTF8) )
			strlcpy( mPrefs.path, kDSLDAPPrefsFilePath, sizeof(mPrefs.path) );
		
		configFileURL = CFURLCreateWithFileSystemPath( kCFAllocatorDefault, (CFStringRef)prefsFilePath, kCFURLPOSIXPathStyle, false );
		if ( configFileURL != NULL )
		{
			if ( CFURLCreateDataAndPropertiesFromResource(kCFAllocatorDefault, configFileURL, &xmlData, NULL, NULL, &errorCode) )
				prefsDict = (CFDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, xmlData, kCFPropertyListImmutable, NULL );
		}
		
		CFRelease( prefsFilePath );
	}
	
	gLDAPPrefsMutex->SignalLock();
	
	DSCFRelease( xmlData );
	DSCFRelease( configFileURL );
	
	return prefsDict;
}


int
CLDAPPlugInPrefs::Save( void )
{
	SInt32 errorCode = 0;

	gLDAPPrefsMutex->WaitLock();

	int err = dsCreatePrefsDirectory();
	if ( err != 0 )
		return err;
	
	CFMutableDictionaryRef prefsDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

	CFDictionaryAddValue( prefsDict, CFSTR(kDSLDAPPrefs_LDAPPlugInVersion),
		mPrefs.version ? mPrefs.version : CFSTR(kDSLDAPPrefs_CurrentVersion) );
	
	if ( mPrefs.configs == NULL )
		mPrefs.configs = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
	
	if ( mPrefs.configs )
		CFDictionaryAddValue( prefsDict, CFSTR(kDSLDAPPrefs_LDAPServerConfigs), mPrefs.configs );
	
	CFStringRef serviceListString = CFStringCreateWithCString( kCFAllocatorDefault, mPrefs.services, kCFStringEncodingUTF8 );
	CFDictionaryAddValue( prefsDict, CFSTR(kDSLDAPPrefs_ServicePrincipalTypes), serviceListString );
	CFRelease( serviceListString );
	
	char *tempPrefsFileStr = GetTempFileName();
	if ( tempPrefsFileStr == NULL ) {
		CFRelease( prefsDict );
		return -1;
	}
	
	CFStringRef tempPrefsFilePath = dsCreatePrefsFilename( tempPrefsFileStr );
	if ( tempPrefsFilePath != NULL )
	{
		CFDataRef xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, (CFDictionaryRef)prefsDict );
		CFURLRef configFileURL = CFURLCreateWithFileSystemPath( kCFAllocatorDefault, (CFStringRef)tempPrefsFilePath, kCFURLPOSIXPathStyle, false );
		if ( xmlData != NULL && configFileURL != NULL )
		{
			//write the XML to the config file
			mode_t saved_mask = umask( 0600 );
			if ( CFURLWriteDataAndPropertiesToResource(configFileURL, xmlData, NULL, &errorCode) )
			{
				CFStringRef tempPrefsPath = CFURLCopyFileSystemPath( configFileURL, kCFURLPOSIXPathStyle );
				if ( tempPrefsPath != NULL )
				{
					char tempPrefsPathStr[PATH_MAX];
					if ( CFStringGetCString(tempPrefsPath, tempPrefsPathStr, sizeof(tempPrefsPathStr), kCFStringEncodingUTF8) )
					{
						if ( chmod(tempPrefsPathStr, S_IRUSR | S_IWUSR) == 0 )
						{
							char prefsPathStr[PATH_MAX];
							CFStringRef prefsPathString = dsCreatePrefsFilename( kDSLDAPPrefsFilePath );
							if ( prefsPathString != NULL )
							{
								if ( CFStringGetCString(prefsPathString, prefsPathStr, sizeof(prefsPathStr), kCFStringEncodingUTF8) )
									rename( tempPrefsPathStr, prefsPathStr );
								CFRelease( prefsPathString );
							}
						}
						else
						{
							CFURLDestroyResource( configFileURL, &errorCode );
							ErrLog( kLogPlugin, "LDAPPlugInPrefs: Unable to save LDAP client preferences because of a file permissions error" );
							DbgLog( kLogPlugin, "LDAPPlugInPrefs: Unable to save LDAP client preferences because of a file permissions error" );
						}
					}
					DSCFRelease( tempPrefsPath );
				}
			}
			else
			{
				ErrLog( kLogPlugin, "LDAPPlugInPrefs: Unable to save LDAP client preferences (error from CFURLWriteDataAndPropertiesToResource)" );
				DbgLog( kLogPlugin, "LDAPPlugInPrefs: Unable to save LDAP client preferences (error from CFURLWriteDataAndPropertiesToResource)" );
			}
			
			umask( saved_mask );
		}
		
		DSCFRelease( xmlData );
		DSCFRelease( configFileURL );		
		DSCFRelease( tempPrefsFilePath );
	}
	DSFreeString( tempPrefsFileStr );
	DSCFRelease( prefsDict );
	
	gLDAPPrefsMutex->SignalLock();
	
	return err;
}


char *
CLDAPPlugInPrefs::GetTempFileName( void )
{
	char *tempPrefsFileStr = strdup( kDSLDAPPrefsTempFilePath );
	if ( tempPrefsFileStr != NULL )
		mktemp( tempPrefsFileStr );
	
	return tempPrefsFileStr;
}



