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

#include "CLDAPv3Configs.h"
#include "CLog.h"
#include "CLDAPPlugInPrefs.h"
#include "CLDAPNodeConfig.h"
#include "CLDAPConnection.h"
#include "BaseDirectoryPlugin.h"
#include "CCachePlugin.h"
#include <DirectoryServiceCore/DSUtils.h>
#include <DirectoryService/DirServicesPriv.h>
#include <SystemConfiguration/SCDynamicStoreCopyDHCPInfo.h>

#include <unistd.h>
#include <sys/stat.h>			//used for mkdir and stat
#include <syslog.h>				//error logging

extern bool				gServerOS;
extern bool				gDHCPLDAPEnabled;
extern CFRunLoopRef		gPluginRunLoop;
extern CCachePlugin		*gCacheNode;

#pragma mark -
#pragma mark CLDAPv3Configs Class

CLDAPv3Configs::CLDAPv3Configs( UInt32 inSignature ) : 
	fNodeConfigMapMutex("CLDAPv3Configs::fNodeConfigMapMutex"), fXMLConfigLock("CLDAPv3Configs::fXMLConfigLock")
{
	fPlugInSignature = inSignature;
	fDHCPLDAPServers = NULL;
	
	fNodeRegistrationEvent.ResetEvent();
	
	// we watch for changes from the configuration stating the DHCP LDAP is enabled from the Search node
	CFStringRef				key			= CFSTR(kDSStdNotifyDHCPConfigStateChanged);
	bool					bWatching	= false;
	SCDynamicStoreContext	scContext	= { 0, this, NULL, NULL, NULL };

	CFArrayRef notifyKeys = CFArrayCreate( kCFAllocatorDefault, (const void **)&key, 1, &kCFTypeArrayCallBacks );
	SCDynamicStoreRef store = SCDynamicStoreCreate( kCFAllocatorDefault, CFSTR("CLDAPv3Configs::CLDAPv3Configs"), DHCPLDAPConfigNotification,
												    &scContext );
	if ( store != NULL && notifyKeys != NULL )
	{
		SCDynamicStoreSetNotificationKeys( store, notifyKeys, NULL );
		
		CFRunLoopSourceRef rls = SCDynamicStoreCreateRunLoopSource( kCFAllocatorDefault, store, 0 );
		if ( rls != NULL )
		{
			CFRunLoopAddSource( gPluginRunLoop, rls, kCFRunLoopDefaultMode );
			bWatching = true;
		}
	}
	
	if ( bWatching )
		DbgLog( kLogPlugin, "CLDAPv3Configs::CLDAPv3Configs - is now watching for DHCP LDAP configuration changes from Search Node" );
	else
		syslog( LOG_ALERT, "LDAPv3 - was unable to watch for DHCP LDAP configuration changes from Search Node" );
	
	DSCFRelease( notifyKeys );
	DSCFRelease( store );
} // CLDAPv3Configs

CLDAPv3Configs::~CLDAPv3Configs ( void )
{
	// let's clear the map
	fNodeConfigMapMutex.WaitLock();
	
	for ( LDAPNodeConfigMapI configIter = fNodeConfigMap.begin(); configIter != fNodeConfigMap.end(); configIter++ )
		DSRelease( configIter->second );
	
	fNodeConfigMap.clear();
	
	fNodeConfigMapMutex.SignalLock();
	
} // ~CLDAPv3Configs

#pragma mark -
#pragma mark Node registrations

void CLDAPv3Configs::RegisterAllNodes( void )
{
	static pthread_mutex_t onlyOneAtTime = PTHREAD_MUTEX_INITIALIZER;
	
	if ( pthread_mutex_trylock(&onlyOneAtTime) == 0 )
	{
		fNodeRegistrationEvent.ResetEvent();

		CFDataRef	xmlData = NULL;
		
		if ( ReadXMLConfig(&xmlData) == eDSNoErr )
			InitializeWithXML( xmlData );
		
		DSCFRelease( xmlData );

		fNodeRegistrationEvent.PostEvent();

		pthread_mutex_unlock( &onlyOneAtTime );
	}
}

void CLDAPv3Configs::UnregisterAllNodes( void )
{
	fNodeConfigMapMutex.WaitLock();
	
	for ( LDAPNodeConfigMapI iter = fNodeConfigMap.begin(); iter != fNodeConfigMap.end(); iter++ )
	{
		tDataListPtr ldapName = dsBuildListFromStringsPriv( "LDAPv3", iter->first.c_str(), NULL );
		if ( ldapName != NULL )
		{
			CServerPlugin::_UnregisterNode( fPlugInSignature, ldapName );
			dsDataListDeallocatePriv( ldapName );
			DSFree( ldapName );
		}
	}
	
	fNodeConfigMapMutex.SignalLock();
}

#pragma mark -
#pragma mark Reading or Updating current config

CFDataRef CLDAPv3Configs::CopyLiveXMLConfig( void )
{
	CFDataRef				combinedConfigDataRef	= NULL;
	CFMutableDictionaryRef	configDict				= NULL;
	CFMutableArrayRef		dhcpConfigArray			= CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
	CFDataRef				configXML				= NULL;
	
	// we grab the config lock and the node lock since we are dealing with the configuration
	fXMLConfigLock.WaitLock();
	
	fNodeConfigMapMutex.WaitLock();
	
	if ( ReadXMLConfig(&configXML) == eDSNoErr )
	{
		configDict = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault,
																			   configXML,
																			   kCFPropertyListMutableContainersAndLeaves,
																			   NULL );
		
		// this should never fail since we verify it in the ReadXMLConfig
		if ( configDict != NULL )
		{
			for ( LDAPNodeConfigMapI configIter = fNodeConfigMap.begin(); configIter != fNodeConfigMap.end(); configIter++ )
			{
				CLDAPNodeConfig *pConfig = configIter->second;
				if ( pConfig->fDHCPLDAPServer == true )
				{
					CFDictionaryRef curConfigDict = pConfig->GetConfiguration();
					if ( curConfigDict != NULL )
					{
						CFArrayAppendValue( dhcpConfigArray, curConfigDict );
						DSCFRelease( curConfigDict );
					}
				}
			}
		}
	}
	
	fNodeConfigMapMutex.SignalLock();
	
	fXMLConfigLock.SignalLock();
	
	if ( CFArrayGetCount(dhcpConfigArray) > 0 )
		CFDictionaryAddValue( configDict, CFSTR(kXMLDHCPConfigArrayKey), dhcpConfigArray );
		
	combinedConfigDataRef = CFPropertyListCreateXMLData( kCFAllocatorDefault, configDict );
	
	DSCFRelease( configXML );
	DSCFRelease( dhcpConfigArray );
	DSCFRelease( configDict );
	
	return combinedConfigDataRef;
}

char **CLDAPv3Configs::GetDHCPBasedLDAPNodes( UInt32 *outCount )
{
	UInt32				counter		= 0;
	CLDAPNodeConfig	   *pConfig		= NULL;
	char				**outList	= NULL;
	
	fNodeConfigMapMutex.WaitLock();
	
	// find the number of entries that match
	for ( LDAPNodeConfigMapI configIter = fNodeConfigMap.begin(); configIter != fNodeConfigMap.end(); configIter++ )
	{
		if ( configIter->second->fDHCPLDAPServer == true )
			counter++;
	}
	
	// set count return value
	(*outCount) = counter;
	
	// now if we had some create a return value
	if ( counter > 0 )
	{
		outList = (char **) calloc( counter + 1, sizeof(char*) );
		
		//now fill the string list
		counter = 0;
		for ( LDAPNodeConfigMapI configIter = fNodeConfigMap.begin(); configIter != fNodeConfigMap.end(); configIter++ )
		{
			pConfig = configIter->second;
			if ( pConfig->fDHCPLDAPServer == true )
			{
				char *theDHCPNodeName = (char *) calloc( 1, sizeof(kLDAPv3Str) + strlen(pConfig->fNodeName) );
				strcpy( theDHCPNodeName, kLDAPv3Str );
				strcat( theDHCPNodeName, pConfig->fNodeName );
				outList[counter] = theDHCPNodeName;
				counter++;
			}
		}
	}
	
	fNodeConfigMapMutex.SignalLock();
	
	return outList;
}

SInt32 CLDAPv3Configs::NewXMLConfig( CFDataRef inXMLData )
{
	if ( inXMLData != NULL )
	{
		CFRetain( inXMLData ); // need to retain it since pointer can change

		if ( VerifyXML(&inXMLData) )
		{
			if ( WriteXMLConfig(inXMLData) == eDSNoErr )
				InitializeWithXML( inXMLData );
		}

		CFRelease( inXMLData );
	}
	
	return eDSNoErr;
}

SInt32 CLDAPv3Configs::AddToXMLConfig( CFDataRef inXMLData )
{
	SInt32					siResult			= eDSCorruptBuffer;
	CFMutableDictionaryRef	configDict			= NULL;
	CFMutableDictionaryRef	xConfigDict			= NULL;
	
	if ( inXMLData != NULL )
	{
		// extract the config dictionary from the XML data.
		configDict = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault,
																			   inXMLData,
																			   kCFPropertyListMutableContainers,
																			   NULL );
		
		if ( configDict != NULL && CFDictionaryGetTypeID() == CFGetTypeID(configDict) )
		{
			// now read our XML config
			CFDataRef	ourXMLData	= NULL;
			
			//let's first go ahead and add this data to the actual config XML tied to the config file
			if ( ReadXMLConfig(&ourXMLData) == eDSNoErr )
			{
				// extract the config dictionary from the XML data.
				xConfigDict = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault,
																						ourXMLData,
																						kCFPropertyListMutableContainers,
																						NULL );
				
				if ( xConfigDict != NULL && CFDictionaryGetTypeID() == CFGetTypeID(xConfigDict) )
				{
					CFMutableArrayRef cfMutableArrayRef;
					
					cfMutableArrayRef = (CFMutableArrayRef) CFDictionaryGetValue( xConfigDict, CFSTR(kXMLConfigArrayKey) );
					if ( cfMutableArrayRef != NULL )
					{
						CFArrayAppendValue( cfMutableArrayRef, configDict );
					}
					else //we need to make the first entry here
					{
						cfMutableArrayRef = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
						CFArrayAppendValue( cfMutableArrayRef, configDict);
						CFDictionarySetValue( xConfigDict, CFSTR(kXMLConfigArrayKey), cfMutableArrayRef );
						DSCFRelease( cfMutableArrayRef );
					}
					
					//convert the dict into a XML blob
					CFDataRef xmlBlob = CFPropertyListCreateXMLData( kCFAllocatorDefault, xConfigDict );
					if ( xmlBlob != NULL )
					{
						NewXMLConfig( xmlBlob );
						DSCFRelease( xmlBlob );
					}
					
					siResult = eDSNoErr;
				}
				
				DSCFRelease( xConfigDict );
			}
		}
		
		DSCFRelease( configDict );
	}
	
	return siResult;
}

CFMutableDictionaryRef CLDAPv3Configs::FindMatchingUUIDAndName( CFDictionaryRef inConfig, const char *inNodeName, CFStringRef inUUID )
{
	CFArrayRef	cfConfigArray	= (CFArrayRef) CFDictionaryGetValue( inConfig, CFSTR(kXMLConfigArrayKey) );
	CFIndex		iCount			= (cfConfigArray != NULL ? CFArrayGetCount(cfConfigArray) : 0);
	CFStringRef	cfNodeName		= CFStringCreateWithCStringNoCopy( kCFAllocatorDefault, inNodeName, kCFStringEncodingUTF8, 
																   kCFAllocatorNull );

	// let's find the exact config
	CFMutableDictionaryRef	nodeConfigDict = NULL;
	
	for ( CFIndex ii = 0; ii < iCount; ii++ )
	{
		CFMutableDictionaryRef	cfTempDict	= (CFMutableDictionaryRef) CFArrayGetValueAtIndex( cfConfigArray, ii );
		bool					bNameMatch	= false;
		
		if ( cfTempDict == NULL )
			continue;
		
		CFStringRef	cfUUID = (CFStringRef) CFDictionaryGetValue( cfTempDict, CFSTR(kXMLConfigurationUUID) );
		if ( cfUUID != NULL && CFStringCompare(cfUUID, inUUID, 0) == kCFCompareEqualTo )
		{
			// first look for nodeName, then look at server name
			CFStringRef cfString = (CFStringRef) CFDictionaryGetValue( cfTempDict, CFSTR(kXMLNodeName) );
			if ( cfString != NULL && CFStringCompare(cfString, cfNodeName, 0) == kCFCompareEqualTo )
			{
				bNameMatch = true;
			}
			else
			{
				cfString = (CFStringRef) CFDictionaryGetValue( cfTempDict, CFSTR(kXMLServerKey) );
				if ( cfString != NULL && CFStringCompare(cfString, cfNodeName, 0) == kCFCompareEqualTo )
				{
					bNameMatch = true;
				}
			}
		}
		
		// Now look for UUID match, if it doesn't match, configs changed
		if ( bNameMatch == true )
		{
			nodeConfigDict = cfTempDict;
			break;
		}
	}
	
	DSCFRelease( cfNodeName );
	
	return nodeConfigDict;
}

void CLDAPv3Configs::UpdateSecurityPolicyForUUID( const char *inNodeName, CFStringRef inUUID, CFDictionaryRef inConfiguredSecPolicy, 
												  CFDictionaryRef inSupportedSecLevel )
{
	CFMutableDictionaryRef	configDict		= NULL;
	CFMutableDictionaryRef	nodeConfigDict	= NULL;
	CFDataRef				configXML		= NULL;
	
	if ( inNodeName == NULL || inUUID == NULL ) return;
	
	// let's find the config for the incoming node first for all the following updates
	fXMLConfigLock.WaitLock();
	
	if ( ReadXMLConfig(&configXML) == eDSNoErr )
	{
		configDict = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault,
																			   configXML,
																			   kCFPropertyListMutableContainersAndLeaves,
																			   NULL );
		
		if ( configDict != NULL && CFGetTypeID(configDict) == CFDictionaryGetTypeID() )
		{
			nodeConfigDict = FindMatchingUUIDAndName( configDict, inNodeName, inUUID );
		}
		
		if ( nodeConfigDict != NULL )
		{
			bool		bChangedConfig	= false;
			bool		bChangedPolicy	= false;
			
			CFDictionaryRef	cfSupported = (CFDictionaryRef) CFDictionaryGetValue( nodeConfigDict, CFSTR(kXMLSupportedSecurityKey) );
			if ( inSupportedSecLevel != NULL )
			{
				if ( cfSupported == NULL || CFEqual(cfSupported, inSupportedSecLevel) == false )
				{
					CFDictionarySetValue( nodeConfigDict, CFSTR(kXMLSupportedSecurityKey), inSupportedSecLevel );
					bChangedConfig = true;
				}
			}
			else if ( cfSupported != NULL )
			{
				CFDictionaryRemoveValue( nodeConfigDict, CFSTR(kXMLSupportedSecurityKey) );
				bChangedPolicy = true;
			}
			
			CFDictionaryRef cfConfigured = (CFDictionaryRef) CFDictionaryGetValue( nodeConfigDict, CFSTR(kXMLConfiguredSecurityKey) );
			if ( inConfiguredSecPolicy != NULL )
			{
				if ( cfConfigured == NULL || CFEqual(cfConfigured, inConfiguredSecPolicy) == false )
				{
					CFDictionarySetValue( nodeConfigDict, CFSTR(kXMLConfiguredSecurityKey), inConfiguredSecPolicy );
					bChangedConfig = true;
					bChangedPolicy = true;
				}
			}
			else if ( cfConfigured != NULL )
			{
				CFDictionaryRemoveValue( nodeConfigDict, CFSTR(kXMLConfiguredSecurityKey) );
				bChangedConfig = true;
				bChangedPolicy = true;
			}
			
			if ( bChangedConfig )
			{
				// Now update our Config file on disk and in memory
				CFDataRef aXMLData = CFPropertyListCreateXMLData( kCFAllocatorDefault, configDict );
				if ( aXMLData != NULL )
				{
					WriteXMLConfig( aXMLData );
					DSCFRelease( aXMLData );
				}
			}
			
			if ( bChangedPolicy )
			{
				DbgLog( kLogPlugin, "CLDAPv3Configs::UpdateSecurityPolicyForUUID - [%s] Updated Security Policies from Directory.", inNodeName );
				syslog( LOG_ALERT, "LDAPv3: [%s] Updated Security Policies from Directory.", inNodeName );
			}			
		}
	}
	
	fXMLConfigLock.SignalLock();

	DSCFRelease( configXML );
	DSCFRelease( configDict );
}

void CLDAPv3Configs::UpdateReplicaListForUUID( const char *inNodeName, CFStringRef inUUID, CFArrayRef inReplicaHostnames, 
											   CFArrayRef inWriteableHostnames )
{
	CFMutableDictionaryRef	configDict		= NULL;
	CFMutableDictionaryRef	nodeConfigDict	= NULL;
	CFDataRef				configXML		= NULL;
	
	if ( inNodeName == NULL || inUUID == NULL ) return;
	
	// let's find the config for the incoming node first for all the following updates
	fXMLConfigLock.WaitLock();
	
	if ( ReadXMLConfig(&configXML) == eDSNoErr )
	{
		configDict = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault,
																			  configXML,
																			  kCFPropertyListMutableContainersAndLeaves,
																			  NULL );
		
		if ( configDict != NULL && CFGetTypeID(configDict) == CFDictionaryGetTypeID() )
		{
			nodeConfigDict = FindMatchingUUIDAndName( configDict, inNodeName, inUUID );
		}
		
		if ( nodeConfigDict != NULL )
		{
			// now insert the new replica list
			CFArrayRef	cfRepArrayRef	= NULL;
			bool		bUpdated		= false;
			
			cfRepArrayRef = (CFArrayRef) CFDictionaryGetValue( nodeConfigDict, CFSTR(kXMLReplicaHostnameListArrayKey) );
			if ( inReplicaHostnames != NULL )
			{
				if ( cfRepArrayRef == NULL || CFEqual(inReplicaHostnames, cfRepArrayRef) == false )
				{
					CFDictionarySetValue( nodeConfigDict, CFSTR(kXMLReplicaHostnameListArrayKey), inReplicaHostnames );
					bUpdated = true;
				}
			}
			else if ( cfRepArrayRef != NULL )
			{
				CFDictionaryRemoveValue( nodeConfigDict, CFSTR( kXMLReplicaHostnameListArrayKey ) );
				bUpdated = true;
			}

			cfRepArrayRef = (CFArrayRef) CFDictionaryGetValue( nodeConfigDict, CFSTR(kXMLWriteableHostnameListArrayKey) );
			if ( inWriteableHostnames != NULL )
			{
				if ( cfRepArrayRef == NULL || CFEqual(inWriteableHostnames, cfRepArrayRef) == false )
				{
					CFDictionarySetValue( nodeConfigDict, CFSTR(kXMLWriteableHostnameListArrayKey), inWriteableHostnames );
					bUpdated = true;
				}
			}
			else if ( cfRepArrayRef != NULL )
			{
				CFDictionaryRemoveValue( nodeConfigDict, CFSTR( kXMLWriteableHostnameListArrayKey ) );
				bUpdated = true;
			}
			
			if ( bUpdated )
			{
				CFDataRef aXMLData = CFPropertyListCreateXMLData( kCFAllocatorDefault, configDict );
				if ( aXMLData != NULL )
				{
					WriteXMLConfig( aXMLData );
					DSCFRelease( aXMLData );
				}
				
				DbgLog( kLogPlugin, "CLDAPv3Configs::UpdateReplicaListForUUID - [%s] Updated replica list from Directory.", inNodeName );
			}
		}
	}

	DSCFRelease( configDict );
	DSCFRelease( configXML );
	
	fXMLConfigLock.SignalLock();
}

void CLDAPv3Configs::UpdateServerMappingsForUUID( const char *inNodeName, CFStringRef inUUID, CFArrayRef inAttrTypeMapArray, 
												  CFArrayRef inRecordTypeMapArray )
{
	CFMutableDictionaryRef	configDict		= NULL;
	CFMutableDictionaryRef	nodeConfigDict	= NULL;
	CFDataRef				configXML		= NULL;
	
	if ( inNodeName == NULL || inUUID == NULL ) return;
	
	// let's find the config for the incoming node first for all the following updates
	fXMLConfigLock.WaitLock();
	
	if ( ReadXMLConfig(&configXML) == eDSNoErr )
	{
		configDict = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault,
																			  configXML,
																			  kCFPropertyListMutableContainersAndLeaves,
																			  NULL );
		
		if ( configDict != NULL && CFGetTypeID(configDict) == CFDictionaryGetTypeID() )
		{
			nodeConfigDict = FindMatchingUUIDAndName( configDict, inNodeName, inUUID );
		}
		
		if ( nodeConfigDict != NULL )
		{
			// now insert the new replica list
			bool		bUpdated	= false;
			
			if ( inAttrTypeMapArray != NULL )
			{
				CFDictionarySetValue( nodeConfigDict, CFSTR(kXMLAttrTypeMapArrayKey), inAttrTypeMapArray );
				bUpdated = true;
			}
			else if ( CFDictionaryContainsKey(nodeConfigDict, CFSTR(kXMLAttrTypeMapArrayKey)) )
			{
				CFDictionaryRemoveValue( nodeConfigDict, CFSTR(kXMLAttrTypeMapArrayKey) );
				bUpdated = true;
			}
			
			if ( inRecordTypeMapArray != NULL )
			{
				CFDictionarySetValue( nodeConfigDict, CFSTR(kXMLRecordTypeMapArrayKey), inRecordTypeMapArray );
				bUpdated = true;
			}
			else if ( CFDictionaryContainsKey(nodeConfigDict, CFSTR(kXMLRecordTypeMapArrayKey)) )
			{
				CFDictionaryRemoveValue( nodeConfigDict, CFSTR(kXMLRecordTypeMapArrayKey) );
				bUpdated = true;
			}
			
			if ( bUpdated )
			{
				CFDataRef aXMLData = CFPropertyListCreateXMLData( kCFAllocatorDefault, configDict );
				if ( aXMLData != NULL )
				{
					WriteXMLConfig( aXMLData );
					DSCFRelease( aXMLData );
				}
				
				DbgLog( kLogPlugin, "CLDAPv3Configs::UpdateServerMappingsForUUID - [%s] Updated server mappings from Directory.", inNodeName );
			}
		}
	}
	
	DSCFRelease( configDict );
	DSCFRelease( configXML );
	
	fXMLConfigLock.SignalLock();
}

#pragma mark -
#pragma mark Get Connection for a node name

bool CLDAPv3Configs::LocalServerIsReplica( void )
{
	bool	bResult			= false;
	char	*fileContents	= NULL;
	
	try
	{
		CFile slapdConf("/etc/openldap/slapd.conf");
		if ( !slapdConf.is_open() )
			throw(-1);
		
		CFile slapdMacOSXConf;
		fileContents = (char*)calloc( 1, slapdConf.FileSize() + 1 );
		if ( fileContents != NULL )
		{
			slapdConf.Read( fileContents, slapdConf.FileSize() );
			if ((strncmp( fileContents, "updatedn", sizeof("updatedn") - 1 ) == 0)
				|| (strstr( fileContents, "\nupdatedn" ) != NULL))
			{
				bResult = true;
			}
			free( fileContents );
			fileContents = NULL;
		}
		
		if ( !bResult )
		{
			slapdMacOSXConf.open("/etc/openldap/slapd_macosxserver.conf");
			fileContents = (char*)calloc( 1, slapdMacOSXConf.FileSize() + 1 );
		}
		
		if (fileContents != NULL)
		{
			slapdMacOSXConf.Read( fileContents, slapdMacOSXConf.FileSize() );
			if ((strncmp( fileContents, "updatedn", sizeof("updatedn") - 1 ) == 0)
				|| (strstr( fileContents, "\nupdatedn" ) != NULL))
			{
				bResult = true;
			}
			free( fileContents );
			fileContents = NULL;
		}
		
	}
	catch ( ... )
	{
	}
	
	DSFree( fileContents );
	
	return bResult;
}

CLDAPConnection *CLDAPv3Configs::CreateConnectionForNode( const char *inNodeName )
{
	CLDAPConnection *pConnection	= NULL;
	
	fNodeConfigMapMutex.WaitLock();

	// search for in our configured node names first
	LDAPNodeConfigMapI iter = fNodeConfigMap.find( inNodeName );
	if ( iter != fNodeConfigMap.end() )
		pConnection = new CLDAPConnection( iter->second );
	
	// now look in our dynamic nodes
	if ( pConnection == NULL )
	{
		iter = fDynamicNodeConfigMap.find( inNodeName );
		if ( iter != fDynamicNodeConfigMap.end() )
			pConnection = new CLDAPConnection( iter->second );
	}
	
	// wasn't found, let's see if it is a ldapi or ldap URL
	if ( pConnection == NULL )
	{
		if ( ldap_is_ldapi_url(inNodeName) )
		{
			if ( gServerOS == true && LocalServerIsReplica() == false )
			{
				CLDAPNodeConfig	*newConfig = new CLDAPNodeConfig( NULL, inNodeName, false );
				if ( newConfig != NULL )
				{
					pConnection = new CLDAPConnection( newConfig );
					fDynamicNodeConfigMap[inNodeName] = newConfig->Retain();
					DSRelease( newConfig );
				}
			}
		}
		else if ( ldap_is_ldap_url(inNodeName) )
		{
			CLDAPNodeConfig	*newConfig = new CLDAPNodeConfig( NULL, inNodeName, false );
			if ( newConfig != NULL )
			{
				pConnection = new CLDAPConnection( newConfig );
				fDynamicNodeConfigMap[inNodeName] = newConfig->Retain();
				DSRelease( newConfig );
			}
		}
	}

	fNodeConfigMapMutex.SignalLock();
	
	return pConnection;
}

#pragma mark -
#pragma mark Writing Server mappings to a server

SInt32 CLDAPv3Configs::WriteServerMappings( char *userName, char *password, CFDataRef inMappings )
{
	SInt32					siResult			= eDSNoErr;
	LDAP				   *serverHost			= NULL;
	CFPropertyListRef		configPropertyList	= NULL;
	CFDictionaryRef			serverConfigDict	= NULL;
	char				   *server				= NULL;
	int						portNumber			= 389;
	int						openCloseTO			= kLDAPDefaultOpenCloseTimeoutInSeconds;
	CFStringRef				cfStringRef			= NULL;
	CFBooleanRef			cfBool				= NULL;
	CFNumberRef				cfNumber			= NULL;
	char				   *mapSearchBase		= NULL;
	bool					bIsSSL				= false;
	bool					bLDAPv2ReadOnly		= false;
    int						ldapReturnCode 		= 0;
	int						version				= -1;
    int						bindMsgId			= 0;
    LDAPMessage			   *result				= NULL;
	char				   *ldapDNString		= NULL;
	UInt32					ldapDNLength		= 0;
	char				   *ourXMLBlob			= NULL;
	char				   *ouvals[2];
	char				   *mapvals[2];
	char				   *ocvals[3];
	LDAPMod					oumod;
	LDAPMod					mapmod;
	LDAPMod					ocmod;
	LDAPMod				   *mods[4];
	
	try
	{	
		if (inMappings != NULL)
		{
			// extract the config dictionary from the XML data.
			configPropertyList = CFPropertyListCreateFromXMLData(	kCFAllocatorDefault,
																	inMappings,
																	kCFPropertyListImmutable,
																	NULL);
			
			if (configPropertyList != NULL )
			{
				//make the propertylist a dict
				if ( CFDictionaryGetTypeID() == CFGetTypeID( configPropertyList ) )
				{
					serverConfigDict = (CFDictionaryRef) configPropertyList;
				}
				
				if (serverConfigDict != NULL)
				{					
					// retrieve all the relevant values (mapsearchbase, IsSSL)
					// to enable server mapping write
					//need to get the server name first
					cfStringRef = (CFStringRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLServerKey ) );
					if ( cfStringRef != NULL && CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
					{
						UInt32 uiLength = (UInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfStringRef), kCFStringEncodingUTF8 ) + 1;
						server = (char *) calloc( sizeof(char), uiLength );
						CFStringGetCString( cfStringRef, server, uiLength, kCFStringEncodingUTF8 );
					}

					cfNumber = (CFNumberRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLOpenCloseTimeoutSecsKey ) );
					if ( cfNumber != NULL )
					{
						CFNumberGetValue(cfNumber, kCFNumberIntType, &openCloseTO);
					}

					cfBool= (CFBooleanRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLIsSSLFlagKey ) );
					if (cfBool != NULL)
					{
						bIsSSL = CFBooleanGetValue( cfBool );
						if (bIsSSL)
						{
							portNumber = LDAPS_PORT;
						}
					}

					cfBool= (CFBooleanRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLLDAPv2ReadOnlyKey ) );
					if (cfBool != NULL)
					{
						bLDAPv2ReadOnly = CFBooleanGetValue( cfBool );
					}

					cfNumber = (CFNumberRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLPortNumberKey ) );
					if ( cfNumber != NULL )
					{
						CFNumberGetValue(cfNumber, kCFNumberIntType, &portNumber);
					}
					
					cfStringRef = (CFStringRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLMapSearchBase ) );
					if ( cfStringRef != NULL && CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
					{
						UInt32 uiLength = (UInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfStringRef), kCFStringEncodingUTF8 ) + 1;
						mapSearchBase = (char *) calloc( sizeof(char), uiLength );
						CFStringGetCString( cfStringRef, mapSearchBase, uiLength, kCFStringEncodingUTF8 );
					}
					
					// don't release the serverConfigDict since it is the cast configPropertyList
				}//if (serverConfigDict != NULL)
				
				CFRelease(configPropertyList); // built from Create on XML data so okay to dealloc here
				configPropertyList = NULL;
			}//if (configPropertyList != NULL )

			if (bLDAPv2ReadOnly) throw( (SInt32)eDSReadOnly); //if configured as LDAPv2 then read only error is returned
																//Directory Utility should check internally before it ever makes the custom call that calls this
			serverHost = ldap_init( server, portNumber );
			if ( serverHost == NULL ) throw( (SInt32)eDSCannotAccessSession );
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
				throw( (SInt32)eDSCannotAccessSession );
			}
			else if ( ldapReturnCode == 0 )
			{
				// timed out, let's forget it
				ldap_unbind_ext( serverHost, NULL, NULL );
				serverHost = NULL;
				throw( (SInt32)eDSCannotAccessSession );
			}
			else if ( ldap_result2error(serverHost, result, 1) != LDAP_SUCCESS )
			{
				throw( (SInt32)eDSCannotAccessSession );
			}			

			if ( (serverHost != NULL) && (mapSearchBase != NULL) )
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
					DSFree( ourXMLBlob );
				} //if ( (siResult == eDSRecordNotFound) || (siResult == eDSNoErr) )
			} // if ( (serverHost != NULL) && (mapSearchBase != NULL) )
		} // inMappings != NULL
		
	} // try
	catch ( SInt32 err )
	{
		siResult = err;
		if (configPropertyList != NULL)
		{
			CFRelease(configPropertyList); // built from Create on XML data so okay to dealloc here
			configPropertyList = NULL;
		}
	}

	if ( serverHost != NULL )
	{
		ldap_unbind_ext( serverHost, NULL, NULL );
		serverHost = NULL;
	}

	if ( mapSearchBase != NULL ) 
	{
		free( mapSearchBase );
		mapSearchBase = NULL;
	}
			
	if ( ourXMLBlob != NULL ) 
	{
		free( ourXMLBlob );
		ourXMLBlob = NULL;
	}
			
	if ( ldapDNString != NULL ) 
	{
		free( ldapDNString );
		ldapDNString = NULL;
	}
			
	return( siResult );
}

#pragma mark -
#pragma mark Initialize with new XML data

SInt32 CLDAPv3Configs::InitializeWithXML( CFDataRef inXMLData )
{
	SInt32					siResult			= eDSNoErr;
	CFMutableDictionaryRef	configDict			= NULL;
	bool					bConfigUpdated		= false;
	const char				*nodeName			= NULL;
	
	if ( inXMLData == NULL ) return eDSNullParameter;
	
	fNodeConfigMapMutex.WaitLock();
	
	configDict = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault,
																		   inXMLData,
																		   kCFPropertyListImmutable,
																		   NULL );
	
	if ( configDict != NULL && CFDictionaryGetTypeID() == CFGetTypeID(configDict) )
	{
		//get version, defaults mappings and array of LDAP server configs
		CFStringRef	cfVersion = (CFStringRef) CFDictionaryGetValue( configDict, CFSTR(kXMLLDAPVersionKey) );
		if ( cfVersion != NULL )
		{
			if ( CFStringCompare(cfVersion, CFSTR(kDSLDAPPrefs_CurrentVersion), 0) != kCFCompareEqualTo )
			{
				CFDictionarySetValue( configDict, CFSTR(kXMLLDAPVersionKey), CFSTR(kDSLDAPPrefs_CurrentVersion) );
				bConfigUpdated = true;
			}
			
			//array of LDAP server configs
			CFArrayRef cfArrayRef = (CFArrayRef) CFDictionaryGetValue( configDict, CFSTR(kXMLConfigArrayKey) );
			if ( cfArrayRef != NULL )
			{
				//now we can retrieve each config
				CFIndex				cfConfigCount = CFArrayGetCount( cfArrayRef );
				LDAPNodeConfigMap	newConfigMap;
				
				for ( CFIndex iConfigIndex = 0; iConfigIndex < cfConfigCount; iConfigIndex++ )
				{
					CFMutableDictionaryRef serverConfigDict = (CFMutableDictionaryRef) CFArrayGetValueAtIndex( cfArrayRef, iConfigIndex );
					if ( serverConfigDict != NULL )
					{
						// ensure we have UUIDs in all the entries
						CFStringRef	cfUUID = (CFStringRef) CFDictionaryGetValue( serverConfigDict, CFSTR(kXMLConfigurationUUID) );
						if ( cfUUID != NULL )
						{
							char			*cStr		= NULL;
							
							// now get the name from the config
							CFStringRef cfNodeName = (CFStringRef) CFDictionaryGetValue( serverConfigDict, CFSTR(kXMLNodeName) );
							if ( cfNodeName == NULL )
								cfNodeName = (CFStringRef) CFDictionaryGetValue( serverConfigDict, CFSTR(kXMLServerKey) );
							
							if ( cfNodeName != NULL )
								nodeName = BaseDirectoryPlugin::GetCStringFromCFString( cfNodeName, &cStr );
							
							if ( nodeName != NULL )
							{
								CLDAPNodeConfig *pConfig	= NULL;
								
								LDAPNodeConfigMapI iter = fNodeConfigMap.find( nodeName );
								if ( iter != NULL && iter != fNodeConfigMap.end() )
								{
									pConfig = iter->second;
									if ( pConfig->fDHCPLDAPServer == true || CFStringCompare(pConfig->fConfigUUID, cfUUID, 0) != kCFCompareEqualTo )
										pConfig = NULL;
									else
										pConfig->Retain(); // need to retain it because we'll be releasing from map
								}
								
								if ( pConfig == NULL )
								{
									pConfig = new CLDAPNodeConfig( this, nodeName, cfUUID );

									tDataListPtr pldapName = dsBuildListFromStringsPriv( "LDAPv3", nodeName, NULL );
									if ( pldapName != NULL )
									{
										CServerPlugin::_RegisterNode( fPlugInSignature, pldapName, kDirNodeType );
										dsDataListDeallocatePriv( pldapName );
										DSFree( pldapName );
									}
								}

								if ( pConfig != NULL )
								{
									pConfig->UpdateConfiguraton( serverConfigDict, false );
									newConfigMap[nodeName] = pConfig;
								}
							}
							DSFree( cStr );
						}
					}
				}
				
				// here we add DHCP configs if we have them after we've registered our normal nodes from XML
				if ( fDHCPLDAPServers != NULL )
				{
					bool	bNodesAdded	= false;
					
					CFIndex iCount = CFArrayGetCount( fDHCPLDAPServers );
					for ( CFIndex ii = 0; ii < iCount; ii++ )
					{
						char		*cStr		= NULL;
						CFStringRef	cfServer	= (CFStringRef) CFArrayGetValueAtIndex( fDHCPLDAPServers, ii );
						const char	*pServer	= BaseDirectoryPlugin::GetCStringFromCFString( cfServer, &cStr );
						
						if ( pServer == NULL )
							continue;
						
						CLDAPNodeConfig *pNewConfig = new CLDAPNodeConfig( this, pServer, true );
						if ( pNewConfig != NULL && pNewConfig->fNodeName )
						{
							// ensure we don't already have a config matching this node name
							// because local configured nodes override DHCP based
							if ( newConfigMap.find(pNewConfig->fNodeName) == newConfigMap.end() )
							{
								newConfigMap[pNewConfig->fNodeName] = pNewConfig->Retain();

								tDataListPtr pldapName = dsBuildListFromStringsPriv( "LDAPv3", pNewConfig->fNodeName, NULL );
								if ( pldapName != NULL )
								{
									bNodesAdded = true;
									CServerPlugin::_RegisterNode( fPlugInSignature, pldapName, kDirNodeType );
									dsDataListDeallocatePriv( pldapName );
									DSFree( pldapName );
								}
							}
							else
							{
								DbgLog( kLogPlugin, "CLDAPv3Configs::InitializeWithXML - DHCP Option 95 node %s will be deleted - have static node", 
									    pNewConfig->fNodeName );
							}
							
							DSRelease( pNewConfig );
						}
						
						DSFree( cStr );
					}
					
					if ( bNodesAdded && gCacheNode != NULL )
						gCacheNode->EmptyCacheEntryType( CACHE_ENTRY_TYPE_NEGATIVE );
				}
				
				// now lets release all the old ones
				for ( LDAPNodeConfigMapI iter = fNodeConfigMap.begin(); iter != fNodeConfigMap.end(); iter++ )
				{
					if ( newConfigMap.find(iter->first) == newConfigMap.end() )
					{
						// not in the new map, so unregister it
						tDataListPtr ldapName = dsBuildListFromStringsPriv( "LDAPv3", iter->first.c_str(), NULL );
						if ( ldapName != NULL )
						{
							if ( gCacheNode != NULL )
								gCacheNode->EmptyCacheEntryType( CACHE_ENTRY_TYPE_ALL );
							
							CServerPlugin::_UnregisterNode( fPlugInSignature, ldapName );
							dsDataListDeallocatePriv( ldapName );
							DSFree( ldapName );
						}

						char		*cStr		= NULL;
						const char	*uuidStr	= BaseDirectoryPlugin::GetCStringFromCFString( iter->second->fConfigUUID, &cStr );
						
						DbgLog( kLogPlugin, "CLDAPv3Configs::InitializeWithXML - Removing config for node %s - %s", iter->first.c_str(), uuidStr );
						DSFree( cStr );	
						
						iter->second->DeleteConfiguration();
					}

					iter->second->Release();
				}
				
				// now just assign the map
				fNodeConfigMap = newConfigMap;

				// need to notify the Search node to check everything again
				SCDynamicStoreRef store = SCDynamicStoreCreate( NULL, CFSTR("DirectoryService"), NULL, NULL );
				if ( store != NULL )
				{
					SCDynamicStoreNotifyValue( store, CFSTR(kDSStdNotifyDHCPOptionsAvailable) );
					DSCFRelease( store );
				}					

				DbgLog( kLogPlugin, "CLDAPv3Configs::InitializeWithXML - Have successfully added or updated Node configurations" );
			}
			
			siResult = eDSNoErr;
		}
		else
		{
			siResult = eDSVersionMismatch;
		}
	}
	
	DSCFRelease( configDict );

	fNodeConfigMapMutex.SignalLock();
	
	return siResult;
}

#pragma mark -
#pragma mark Other support functions

void CLDAPv3Configs::NetworkTransition( void )
{
	fNodeConfigMapMutex.WaitLock();
	
	for ( LDAPNodeConfigMapI iter = fNodeConfigMap.begin(); iter != fNodeConfigMap.end(); iter++ )
		iter->second->NetworkTransition();
	
	fNodeConfigMapMutex.SignalLock();
}

void CLDAPv3Configs::PeriodicTask( void )
{
	fNodeConfigMapMutex.WaitLock();
	
	for ( LDAPNodeConfigMapI iter = fDynamicNodeConfigMap.begin(); iter != fDynamicNodeConfigMap.end();  )
	{
		// if we are the only ones holding it, release it
		if ( iter->second->RetainCount() == 1 )
		{
			DbgLog( kLogPlugin, "CLDAPv3Configs::PeriodicTask - removing dynamic node %s from table - References: 0", iter->first.c_str() );
			iter->second->Release();
			fDynamicNodeConfigMap.erase( iter++ );
			continue;
		}
		
		iter++;
	}
	
	fNodeConfigMapMutex.SignalLock();
}

#pragma mark -
#pragma mark DHCP Option 95 stuff

void CLDAPv3Configs::DHCPLDAPConfigNotification( SCDynamicStoreRef cfStore, CFArrayRef changedKeys, void *inInfo )
{
	CLDAPv3Configs	*pConfig = (CLDAPv3Configs *) inInfo;
	
	DbgLog( kLogApplication, "CLDAPv3Configs::DHCPLDAPConfigNotification - Option 95 support changed to <%s> from Search Node", 
		    (gDHCPLDAPEnabled ? "on" : "off") );
	pConfig->WatchForDHCPPacket();
	if ( pConfig->CheckForDHCPPacket() )
		pConfig->RegisterAllNodes();
}

void CLDAPv3Configs::DHCPPacketStateNotification( SCDynamicStoreRef cfStore, CFArrayRef changedKeys, void *inInfo )
{
	CLDAPv3Configs	*pConfig = (CLDAPv3Configs *) inInfo;
	
	DbgLog( kLogApplication, "CLDAPv3Configs::DHCPPacketStateNotification - Looking for Option 95 in DHCP info" );
	if ( pConfig->CheckForDHCPPacket() )
		pConfig->RegisterAllNodes();
}

bool CLDAPv3Configs::CheckForDHCPPacket( void )
{
	bool		bNewDHCPConfig	= false;
	CFArrayRef	cfNewServers	= NULL;
	
	fNodeConfigMapMutex.WaitLock();
	
	// look for DHCP options if it is enabled
	if ( gDHCPLDAPEnabled )
	{
		CFDictionaryRef ourDHCPInfo = SCDynamicStoreCopyDHCPInfo( NULL, NULL );
		if ( ourDHCPInfo != NULL )
		{
			CFDataRef ourDHCPServersData = DHCPInfoGetOptionData( ourDHCPInfo, 95 );
			if ( ourDHCPServersData != NULL )
			{
				CFStringRef ourDHCPString = CFStringCreateWithBytes( kCFAllocatorDefault,
																	 CFDataGetBytePtr(ourDHCPServersData),
																	 CFDataGetLength(ourDHCPServersData),
																	 kCFStringEncodingUTF8,
																	 false );
				if ( ourDHCPString != NULL )
				{
					cfNewServers = CFStringCreateArrayBySeparatingStrings( kCFAllocatorDefault, ourDHCPString, CFSTR(" ") );
					if ( fDHCPLDAPServers == NULL || CFEqual(cfNewServers, fDHCPLDAPServers) == false )
					{
						bNewDHCPConfig = true;
						DbgLog( kLogPlugin, "CLDAPv3Configs::CheckForDHCPPacket - New Option 95 DHCP information available" );
					}
					
					DSCFRelease( ourDHCPString );
				} 
			}
			
			DSCFRelease( ourDHCPInfo );
		}
	}
	
	if ( cfNewServers != NULL )
	{
		DSCFRelease( fDHCPLDAPServers );
		fDHCPLDAPServers = cfNewServers;
	}
	else if ( fDHCPLDAPServers != NULL )
	{
		DSCFRelease( fDHCPLDAPServers );
		DbgLog( kLogPlugin, "CLDAPv3Configs::CheckForDHCPPacket - clearing previous server list no longer available" );
		bNewDHCPConfig = true;
	}
	
	fNodeConfigMapMutex.SignalLock();
	
	return bNewDHCPConfig;
}

void CLDAPv3Configs::WatchForDHCPPacket( void )
{
	static CFRunLoopSourceRef	cfNotifyRLS	= NULL;
	static pthread_mutex_t		localMutex	= PTHREAD_MUTEX_INITIALIZER;
	
	pthread_mutex_lock( &localMutex );
	
	if (gDHCPLDAPEnabled && cfNotifyRLS == NULL)
	{
		// now let's register for DHCP change events cause we care about them
		SCDynamicStoreContext	context		= { 0, this, NULL, NULL, NULL };
		CFStringRef				dhcpKey		= SCDynamicStoreKeyCreateNetworkServiceEntity( kCFAllocatorDefault, kSCDynamicStoreDomainState, 
																						   kSCCompAnyRegex, kSCEntNetDHCP );
		CFStringRef				ipv4Key		= SCDynamicStoreKeyCreateNetworkGlobalEntity( kCFAllocatorDefault, kSCDynamicStoreDomainState, 
																						  kSCEntNetIPv4 );
		CFArrayRef				patterns	= CFArrayCreate( kCFAllocatorDefault, (CFTypeRef *)&dhcpKey, 1, &kCFTypeArrayCallBacks );
		CFArrayRef				keys		= CFArrayCreate( kCFAllocatorDefault, (CFTypeRef *)&ipv4Key, 1, &kCFTypeArrayCallBacks );
		
		SCDynamicStoreRef store = SCDynamicStoreCreate( kCFAllocatorDefault, CFSTR("CLDAPv3Configs::WatchForDHCPPacket"), 
													    DHCPPacketStateNotification, &context );
		if ( store != NULL )
		{
			SCDynamicStoreSetNotificationKeys( store, keys, patterns );
			cfNotifyRLS = SCDynamicStoreCreateRunLoopSource( NULL, store, 0 );
			if (cfNotifyRLS != NULL)
			{
				CFRunLoopAddSource( gPluginRunLoop, cfNotifyRLS, kCFRunLoopDefaultMode );
				DbgLog( kLogPlugin, "CLDAPv3Configs::WatchForDHCPPacket - watching for LDAP options in DHCP packets" );
			}
			else
			{
				syslog(LOG_ALERT, "CLDAPv3Configs::WatchForDHCPPacket - failed to create runloop source for DHCP Network Notifications");
			}
		}
		else
		{
			syslog(LOG_ALERT, "CLDAPv3Configs::WatchForDHCPPacket - failed to register for DHCP Network Notifications");
		}
		
		DSCFRelease( dhcpKey );
		DSCFRelease( ipv4Key );
		DSCFRelease( keys );
		DSCFRelease( patterns );
		DSCFRelease( store );
	}
	else if ( gDHCPLDAPEnabled == false && cfNotifyRLS != NULL )
	{
		CFRunLoopRemoveSource( gPluginRunLoop, cfNotifyRLS, kCFRunLoopDefaultMode );
		DSCFRelease( cfNotifyRLS );
		
		DbgLog( kLogPlugin, "CLDAPv3Configs::WatchForDHCPPacket - no longer watching for LDAP options in DHCP packets" );
	}
	
	pthread_mutex_unlock( &localMutex );
}

#pragma mark -
#pragma mark File Operations

SInt32 CLDAPv3Configs::ReadXMLConfig( CFDataRef *outXMLData )
{
	CFDataRef			xmlData			= NULL;
	SInt32				siResult		= eDSNoErr;
	bool				bCorruptedFile	= false;
	CLDAPPlugInPrefs	*prefsFile		= NULL;
	
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
	
	fXMLConfigLock.WaitLock();
	
	prefsFile = new CLDAPPlugInPrefs();
	if ( prefsFile != NULL )
	{
		xmlData = prefsFile->GetPrefsXML();
		if ( xmlData )
		{
			//check if this XML blob is a property list and can be made into a dictionary
			if ( VerifyXML(&xmlData) == false )
			{
				char	*corruptPath = "/Library/Preferences/DirectoryService/DSLDAPv3PlugInConfigCorrupted.plist";
				
				//if it is not then say the file is corrupted and save off the corrupted file
				DbgLog( kLogPlugin, "CLDAPv3Configs::ReadXMLConfig - LDAP XML config file is corrupted" );
				bCorruptedFile = true;
				//here we need to make a backup of the file - why? - because
				DSPrefs prefs = {0};
				prefsFile->GetPrefs( &prefs );
				rename( prefs.path, corruptPath );
			}
		}
		else //existing file is unreadable
		{
			DbgLog( kLogPlugin, "CLDAPv3Configs::ReadXMLConfig - LDAP XML config file is unreadable" );
			bCorruptedFile = true;
		}
		
		if ( bCorruptedFile )
		{
			// delete and re-create the object w/o the file present
			if ( prefsFile != NULL )
				delete prefsFile;
			prefsFile = new CLDAPPlugInPrefs();
			
			DbgLog( kLogPlugin, "CLDAPv3Configs::ReadXMLConfig - Writing a new LDAP XML config file" );
			prefsFile->Save();
		}
		
		// if we have a config now, let's convert let's look to see if there is a sV2Config to convert
		if ( xmlData != NULL )
		{
			// if we converted....
			if ( ConvertLDAPv2Config(&xmlData) )
			{
				DSPrefs prefs = {0};
				prefsFile->GetPrefs( &prefs );
				
				CFMutableDictionaryRef configPropertyList = 
				(CFMutableDictionaryRef) CFPropertyListCreateFromXMLData(
																		 kCFAllocatorDefault,
																		 xmlData,
																		 kCFPropertyListMutableContainersAndLeaves, 
																		 NULL);
				if ( configPropertyList != NULL )
				{
					prefs.configs = (CFArrayRef) CFDictionaryGetValue( configPropertyList, CFSTR(kDSLDAPPrefs_LDAPServerConfigs) );
					if ( prefs.configs != NULL ) {
						prefsFile->SetPrefs( &prefs );
						prefsFile->Save();
					}
					
					DSCFRelease( configPropertyList );
				}
			}
		}
		
		delete prefsFile;
	}
	
	if ( xmlData != NULL )
	{
		(*outXMLData) = xmlData;
		siResult = eDSNoErr;
	}
	
	fXMLConfigLock.SignalLock();
	
    return siResult;
}

SInt32 CLDAPv3Configs::WriteXMLConfig( CFDataRef inXMLData )
{
	SInt32					siResult			= eDSNoErr;
	bool					bWroteFile			= false;
	
	fXMLConfigLock.WaitLock();
	
	CFMutableDictionaryRef configPropertyList = 
	(CFMutableDictionaryRef) CFPropertyListCreateFromXMLData(
															 kCFAllocatorDefault,
															 inXMLData,
															 kCFPropertyListMutableContainersAndLeaves, 
															 NULL);
	if ( configPropertyList != NULL )
	{
		CLDAPPlugInPrefs prefsFile;
		DSPrefs prefs = {0};
		prefsFile.GetPrefs( &prefs );
		
		prefs.configs = (CFArrayRef) CFDictionaryGetValue( configPropertyList, CFSTR(kDSLDAPPrefs_LDAPServerConfigs) );
		if ( prefs.configs != NULL ) {
			prefsFile.SetPrefs( &prefs );
			bWroteFile = (prefsFile.Save() == 0);
		}
		
		CFRelease( configPropertyList );
	}
	
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
	
	fXMLConfigLock.SignalLock();
	
	if (bWroteFile)
	{
		DbgLog( kLogPlugin, "CLDAPv3Configs::WriteXMLConfig - Have written the LDAP XML config file" );
		siResult = eDSNoErr;
	}
	else
	{
		DbgLog( kLogPlugin, "CLDAPv3Configs::WriteXMLConfig - LDAP XML config file has FAILED to been written" );
		siResult = eDSPlugInConfigFileError;
	}
	
	return siResult;
	
}

bool CLDAPv3Configs::VerifyXML( CFDataRef *inOutXMLData )
{
    bool					verified			= false;
    CFMutableDictionaryRef	configPropertyList	= NULL;
	
    if ( (*inOutXMLData) != NULL )
    {
        // extract the config dictionary from the XML data.
        configPropertyList = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault,
																					   (*inOutXMLData),
																					   kCFPropertyListMutableContainersAndLeaves, 
																					   NULL );
        if ( configPropertyList != NULL )
        {
			bool	bUpdated = false;
			
            //ensure the propertylist is a dict
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
							
							// see if a UUID was assigned yet, if not put one
							if ( CFDictionaryContainsKey( cfConfig, CFSTR(kXMLConfigurationUUID) ) == false )
							{
								CFUUIDRef cfUUID = CFUUIDCreate( kCFAllocatorDefault );
								CFStringRef cfString = CFUUIDCreateString( kCFAllocatorDefault, cfUUID );
								CFDictionarySetValue( cfConfig, CFSTR(kXMLConfigurationUUID), cfString );
								DSCFRelease( cfUUID );
								DSCFRelease( cfString );
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
				
				if ( xmlData != NULL )
				{
					CFRelease( *inOutXMLData );
					(*inOutXMLData) = xmlData;
				}
				
				if ( bUpdated )
				{
					DbgLog( kLogPlugin, "CLDAPv3Configs::VerifyXML - Configuration updated saving the file" );
					WriteXMLConfig( xmlData );
				}
			}
			
            DSCFRelease( configPropertyList );
        }
    }
    
    return verified;
}

bool CLDAPv3Configs::ConvertLDAPv2Config( CFDataRef *inOutXMLData )
{
	struct stat				statResult;
	const char				*prefPath		= "/Library/Preferences/DirectoryService/DSLDAPPlugInConfig.clpi";
	bool					bReturn			= false;
	CFDataRef				sV2ConfigData	= NULL;
	CFMutableDictionaryRef  sV2Config		= NULL;
	CFMutableDictionaryRef  sV3Config		= NULL;
	
	// first let's see if the LDAPv2 Plugin does not exist before we try to convert the config.
	// if we have a path, and we can't stat anything, the plugin must not exist.
	if ( stat("/System/Library/Frameworks/DirectoryService.framework/Resources/Plugins/LDAPv2.dsplug", &statResult) != 0 )
	{
		char		newName[PATH_MAX]	= { 0 };
		
		CFStringRef sPath = CFStringCreateWithCString( kCFAllocatorDefault, prefPath, kCFStringEncodingUTF8 );
		
		strcpy( newName, prefPath );
		strcat( newName, ".v3converted" );
		
		if( stat( prefPath, &statResult ) == 0 ) // must be a file...
		{
			// Convert it back into a CFURL.
			CFURLRef	sConfigFileURL   = CFURLCreateWithFileSystemPath( kCFAllocatorDefault, sPath, kCFURLPOSIXPathStyle, false );
			
			CFURLCreateDataAndPropertiesFromResource( kCFAllocatorDefault, sConfigFileURL, &sV2ConfigData, NULL, NULL, NULL );
			
			CFRelease( sConfigFileURL );
			sConfigFileURL = NULL;
			
			if( sV2ConfigData ) 
			{
				sV2Config = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, sV2ConfigData, 
																					  kCFPropertyListMutableContainers, NULL );
				CFRelease( sV2ConfigData );
				sV2ConfigData = NULL;
			}
			
			if ( (*inOutXMLData) != NULL )
			{
				sV3Config = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, (*inOutXMLData), 
																					  kCFPropertyListMutableContainers, NULL );
			}
			
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
							if ( aXMLData != NULL )
							{
								if ( (*inOutXMLData) != NULL )
									CFRelease( *inOutXMLData );
								
								(*inOutXMLData) = aXMLData;
							}
							
							bReturn = true;
						}
					}
				}
			}
			
			// let's rename the old file to something so we don't convert again.
			rename( prefPath, newName );
		}
		
		DSCFRelease( sPath );
	}
	
	DSCFRelease( sV2Config );
	DSCFRelease( sV3Config );
	
	return bReturn;
}

