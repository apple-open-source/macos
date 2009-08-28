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

#include "CLDAPNodeConfig.h"
#include "BaseDirectoryPlugin.h"
#include "CLDAPPluginPrefs.h"
#include "CDSPluginUtils.h"
#include "CCachePlugin.h"
#include "CLDAPv3Configs.h"
#include "CLDAPConnection.h"
#include "DNSLookups.h"

#include <DirectoryServiceCore/CLog.h>
#include <Kerberos/krb5.h>
#include <Kerberos/gssapi.h>
#include <Kerberos/gssapi_krb5.h>
#include <PasswordServer/AuthFile.h>
//#include <arpa/inet.h>	// inet_ntop
#include <netinet/in.h>	// struct sockaddr_in
#include <ifaddrs.h>
#include <fcntl.h>
#include <ldap_private.h>
#include <sasl/sasl.h>
#include <syslog.h>

extern	CCachePlugin		*gCacheNode;
extern	DSMutexSemaphore	*gKerberosMutex;

#define OCSEPCHARS		" '()$"
#define IsOnlyBitSet(a,b)   (((a) & (b)) && ((a) ^ (b)) == 0) 

struct saslDefaults
{
	const char *authcid;
	const char *password;
	char *authzid;
};

#pragma mark -
#pragma mark Class Definition

CLDAPNodeConfig::CLDAPNodeConfig( CLDAPv3Configs *inConfig, const char *inNodeName, CFStringRef inUUID ) 
	: fMutex("CLDAPNodeConfig::fMutex"), fMappingsLock("CLDAPNodeConfig:fMappingsLock")
{
	InitializeVariables();
	
	fNodeName = strdup( inNodeName );
	fConfigUUID = (CFStringRef) CFRetain( inUUID );
	fConfigObject = inConfig;

	char		*cStr		= NULL;
	const char	*uuidStr	= BaseDirectoryPlugin::GetCStringFromCFString( fConfigUUID, &cStr );

	DbgLog( kLogPlugin, "CLDAPNodeConfig::CLDAPNodeConfig - New configured node %s - %s created", fNodeName, uuidStr );
	DSFree( cStr );
}

CLDAPNodeConfig::CLDAPNodeConfig( CLDAPv3Configs *inConfig, const char *inLDAPURL, bool inDHCPLDAPServer )
{
	LDAPURLDesc		*ludpp = NULL;
	
	InitializeVariables();
	
	// we generate a UUID that way we can remove a config at any given time as a new DHCP one is received
	CFUUIDRef cfUUID = CFUUIDCreate( kCFAllocatorDefault );
	fConfigUUID = CFUUIDCreateString( kCFAllocatorDefault, cfUUID );
	DSCFRelease( cfUUID );
	
	fServerMappings = true; // default to ServerMappings
	fDHCPLDAPServer = inDHCPLDAPServer;
	fNodeIsLDAPURL = true;
	fConfigObject = inConfig;
	fAvailable = true;

	// this should never fail as it should have been called before coming in here
	if ( ldap_is_ldapi_url(inLDAPURL) )
	{
		fNodeName = strdup( inLDAPURL );
		fGetReplicas = false;
		fReplicaList.push_back( new CLDAPReplicaInfo(fNodeName) );
	}
	else if ( ldap_url_parse(inLDAPURL, &ludpp) == LDAP_SUCCESS )
	{
		fNodeName = (inDHCPLDAPServer ? strdup(ludpp->lud_host) : strdup(inLDAPURL));
		fServerName = strdup( ludpp->lud_host );
		fServerPort = ludpp->lud_port;
		fMapSearchBase = (ludpp->lud_dn != NULL ? strdup(ludpp->lud_dn) : NULL);
		fIsSSL = (ludpp->lud_port == LDAPS_PORT);
		
		ldap_free_urldesc( ludpp );
		ludpp = NULL;

		fReadReplicas = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
		fWriteReplicas = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
	}
	
	char		*cStr		= NULL;
	const char	*uuidStr	= BaseDirectoryPlugin::GetCStringFromCFString( fConfigUUID, &cStr );
	
	DbgLog( kLogPlugin, "CLDAPNodeConfig::CLDAPNodeConfig - New %s node %s - %s created", (inDHCPLDAPServer ? "DHCP" : "URL"), 
		    fNodeName, uuidStr );
	DSFree( cStr );	
}

CLDAPNodeConfig::~CLDAPNodeConfig( void )
{
	fMutex.WaitLock();

	char		*cStr		= NULL;
	const char	*uuidStr	= BaseDirectoryPlugin::GetCStringFromCFString( fConfigUUID, &cStr );
	
	DbgLog( kLogPlugin, "CLDAPNodeConfig::~CLDAPNodeConfig - Node %s - %s - Deleted no longer in use", fNodeName, uuidStr );
	DSFree( cStr );
	
	DSCFRelease( fConfigUUID );
	DSFree( fNodeName );

	for ( ListOfReplicasI iter = fReplicaList.begin(); iter != fReplicaList.end(); iter++ )
		(*iter)->Release();
	
	fReplicaList.clear();

	DSFree( fServerAccount );
	DSFree( fServerKerberosID );
	DSFree( fServerPassword );
	
	DSFree( fMapSearchBase );
	DSFree( fServerName );
	DSFree( fConfigUIName );

	dispatch_cancel( fDynamicRefreshTimer );
	dispatch_release( fDynamicRefreshTimer );
	
	DSCFRelease( fNormalizedMappings );
	DSCFRelease( fRecordTypeMapArray );
	DSCFRelease( fAttrTypeMapArray );
	
	DSCFRelease( fReadReplicas );
	DSCFRelease( fWriteReplicas );
	DSCFRelease( fDeniedSASLMethods );
	
	if ( fObjectClassSchema != NULL )
	{
		ObjectClassMapCI iter = fObjectClassSchema->begin();
		
		while ( iter != fObjectClassSchema->end() )
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
	
	if ( fReachabilityRef != NULL )
	{
		SCNetworkReachabilityUnscheduleFromRunLoop( fReachabilityRef, CFRunLoopGetMain(), kCFRunLoopDefaultMode );
		DSCFRelease( fReachabilityRef );
	}

	fMutex.SignalLock();
}

#pragma mark -
#pragma mark Establish an LDAP connection

LDAP *CLDAPNodeConfig::EstablishConnection( CLDAPReplicaInfo **inOutReplicaInfo, bool inWriteable, const char *inLDAPUsername, 
										    const char *inKerberosID, const char *inPassword, void *inCallback, void *inParam, 
										    tDirStatus *outStatus )
{
	LDAP	*pTempLD = NULL;
	
	(*outStatus) = (inWriteable ? eDSAuthMasterUnreachable : eDSCannotAccessSession);

	if ( ldap_is_ldapi_url(fNodeName) )
	{
		(*outStatus) = eDSAuthMethodNotSupported;
	}
	else
	{
		pTempLD = InternalEstablishConnection( inOutReplicaInfo, inWriteable, inCallback, inParam );
		if ( pTempLD != NULL )
		{
			(*outStatus) = AuthenticateUsingCredentials( pTempLD, (*inOutReplicaInfo), inLDAPUsername, inKerberosID, inPassword );
			if ( (*outStatus) != eDSNoErr )
			{
				ldap_unbind_ext_s( pTempLD, NULL, NULL );
				pTempLD = NULL;
			}
		}
	}

	return pTempLD;
}

LDAP *CLDAPNodeConfig::EstablishConnection( CLDAPReplicaInfo **inOutReplicaInfo, bool inWriteable, const char *inKerberosCache, void *inCallback, 
										    void *inParam, tDirStatus *outStatus )
{
	LDAP	*pTempLD = NULL;
	
	(*outStatus) = (inWriteable ? eDSAuthMasterUnreachable : eDSCannotAccessSession);

	if ( ldap_is_ldapi_url(fNodeName) )
	{
		(*outStatus) = eDSAuthMethodNotSupported;
	}
	else
	{
		pTempLD = InternalEstablishConnection( inOutReplicaInfo, inWriteable, inCallback, inParam );
		
		if ( pTempLD != NULL )
		{
			(*outStatus) = AuthenticateUsingKerberos( pTempLD, (*inOutReplicaInfo), inKerberosCache );
			if ( (*outStatus) != eDSNoErr )
			{
				ldap_unbind_ext_s( pTempLD, NULL, NULL );
				pTempLD = NULL;
			}
		}
	}
	
	return pTempLD;
}

LDAP *CLDAPNodeConfig::EstablishConnection( CLDAPReplicaInfo **inOutReplicaInfo, bool inWriteable, void *inCallback, void *inParam, 
										    tDirStatus *outStatus )
{
	LDAP *pTempLD	= NULL;
	
	(*outStatus) = (inWriteable ? eDSAuthMasterUnreachable : eDSCannotAccessSession);
	
	fMutex.WaitLock();
	bool bSecureUse = fSecureUse;
	fMutex.SignalLock();

	if ( bSecureUse == true )
	{
		// we have to dupe these credentials because they can be freed in Establish connection due to mutex unlock for Kerberos
		fMutex.WaitLock();
		char *pServerAccount = (fServerAccount != NULL ? strdup(fServerAccount) : NULL);
		char *pKerberosID = (fServerKerberosID != NULL ? strdup(fServerKerberosID) : NULL);
		char *pPassword = (fServerPassword != NULL ? strdup(fServerPassword) : NULL);
		fMutex.SignalLock();
		
		pTempLD = EstablishConnection( inOutReplicaInfo, inWriteable, pServerAccount, pKerberosID, pPassword, inCallback, 
									   inParam, outStatus );
		
		DSFree( pServerAccount );
		DSFree( pKerberosID );
		DSFreePassword( pPassword );
	}
	else
	{
		if ( ldap_is_ldapi_url(fNodeName) )
		{
			fMutex.WaitLock();

			// we don't do this in internal establish because it's meant for network connections
			CLDAPReplicaInfo *replica = *(fReplicaList.begin());
			
			(*outStatus) = eDSCannotAccessSession;
			
			// just use the replica code to do the work so we can get server mappings
			pTempLD = replica->VerifiedServerConnection( 10, true, NULL, NULL );
			if ( pTempLD != NULL )
			{
				if ( RetrieveServerMappings(pTempLD, replica) == true )
				{
					(*outStatus) = eDSNoErr;
					(*inOutReplicaInfo) = replica->Retain();
				}

				if ( (*outStatus) != eDSNoErr )
				{
					ldap_unbind_ext_s( pTempLD, NULL, NULL );
					pTempLD = NULL;
				}
			}

			fMutex.SignalLock();
		}
		else
		{
			pTempLD = InternalEstablishConnection( inOutReplicaInfo, inWriteable, inCallback, inParam );
		}
	}
	
	if ( pTempLD != NULL )
		(*outStatus) = eDSNoErr;

	return pTempLD;
}

bool CLDAPNodeConfig::UpdateDynamicData( LDAP *inLD, CLDAPReplicaInfo *inReplica )
{
	bool	bUpdated	= false;
	
	// should never happen, but safety
	if ( inLD == NULL || inReplica == NULL )
		return false;
	
	fMutex.WaitLock();

	// we always get server mappings first
	if ( fGetServerMappings == true )
		RetrieveServerMappings( inLD, inReplica );
	
	// now we get security settings
	if ( fGetSecuritySettings == true && RetrieveServerSecuritySettings(inLD, inReplica) == true )
		bUpdated = true;
	
	// now we get the replicas
	if ( fGetReplicas == true )
	{
		CFMutableArrayRef	readList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
		CFMutableArrayRef	writeList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
		
		if ( RetrieveServerReplicaList(inLD, readList, writeList) == true )
			bUpdated = true;
		
		// look at altServer Entries
		if ( GetReplicaListFromAltServer(inLD, readList) == eDSNoErr )
			bUpdated = true;
		
		// look at DNS entries last, DNS will make order of entries based on DNS results
		if ( fDNSReplicas == true )
		{
			if( GetReplicaListFromDNS(readList) == eDSNoErr )
				bUpdated = true;
		}
		else
		{
			DbgLog( kLogPlugin, "CLDAPNodeConfig::GetReplicaListFromDNS - Node %s - Skipped - disabled in configuration", fNodeName );
		}
		
		// if we got some replicas refresh our list
		if ( CFArrayGetCount(readList) > 0 )
		{
			DSCFRelease( fReadReplicas );
			fReadReplicas = readList;
			readList = NULL;
			
			DSCFRelease( fWriteReplicas );
			fWriteReplicas = writeList;
			writeList = NULL;
			
			if ( fServerName != NULL )
			{
				// ensure the listed host is listed first as our preferred
				CFStringRef cfPreferred = CFStringCreateWithCString( kCFAllocatorDefault, fServerName, kCFStringEncodingUTF8 );
				CFArrayInsertValueAtIndex( fReadReplicas, 0, cfPreferred );
				DSCFRelease( cfPreferred );
			}
			
			// let's toss out our old replica list as we don't know if replicas were removed
			for ( ListOfReplicasI iter = fReplicaList.begin(); iter != fReplicaList.end(); iter++ )
				(*iter)->Release();
			
			fReplicaList.clear();
			
			DbgLog( kLogPlugin, "CLDAPNodeConfig::UpdateDynamicData - Node %s - emptying current replica list in favor of new list",
				    fNodeName );
			
			// now rebuild the list
			BuildReplicaList();
			
			LaunchKerberosAutoConfigTool();
		}
		
		DSCFRelease( readList );
		DSCFRelease( writeList );
	}
	
	fMutex.SignalLock();
	
	return bUpdated;
}

void CLDAPNodeConfig::ReinitializeReplicaList( void )
{
	// this throws out all known replicas and starts with the original config
	if ( fServerName != NULL )
	{
		fMutex.WaitLock();
		
		DbgLog( kLogPlugin, "CLDAPNodeConfig::ReinitializeReplicaList - Node %s - emptying current replica list", fNodeName );
		
		// let's toss out our old replica list as we don't know if replicas were removed
		for ( ListOfReplicasI iter = fReplicaList.begin(); iter != fReplicaList.end(); iter++ )
			(*iter)->Release();
		
		fReplicaList.clear();
		
		// set our flag to re-read replica list
		fGetReplicas = true;
		
		// just remove all replicas, we'll rediscover shortly
		CFArrayRemoveAllValues( fReadReplicas );
		CFArrayRemoveAllValues( fWriteReplicas );
		
		// need a replica list
		CFStringRef cfServerName = CFStringCreateWithCString( kCFAllocatorDefault, fServerName, kCFStringEncodingUTF8 );
		CFArrayAppendValue( fReadReplicas, cfServerName );
		DSCFRelease( cfServerName );
		
		BuildReplicaList();
		
		fMutex.SignalLock();
	}
}

#pragma mark -
#pragma mark Attribute and Record type mapping

char *CLDAPNodeConfig::MapRecToSearchBase( const char *inRecType, int inIndex, bool *outOCGroup, CFArrayRef *outOCListCFArray, ber_int_t *outScope )
{
	char			   *outResult	= NULL;
	UInt32				uiNativeLen	= sizeof(kDSNativeRecordTypePrefix) - 1;
	UInt32				uiStdLen	= sizeof(kDSStdRecordTypePrefix) - 1;
	
	//idea here is to use the inIndex to request a specific map
	//if inIndex is 1 then the first map will be returned
	//if inIndex is >= 1 and <= totalCount then that map will be returned
	//if inIndex is <= 0 then NULL will be returned
	//if inIndex is > totalCount NULL will be returned
	//note the inIndex will reference the inIndexth entry ie. start at 1
	//caller can increment the inIndex starting at one and get maps until NULL is returned
	
	if ( inRecType != NULL && inIndex > 0 )
	{
		uint32_t	uiStrLen = strlen( inRecType );
		
		// First look for native record type, ensure it is long enough to compare
		if ( uiStrLen >= uiNativeLen && strncmp(inRecType, kDSNativeRecordTypePrefix, uiNativeLen) == 0 )
		{
			// Make sure we have data past the prefix
			if ( uiStrLen > uiNativeLen && inIndex == 1 )
			{
				outResult = strdup( inRecType + uiNativeLen );
				DbgLog( kLogPlugin, "CLDAPNodeConfig::MapRecToSearchBase - Warning native record type <%s> is being used", outResult );
			}
		}
		else if ( uiStrLen >= uiStdLen && strncmp(inRecType, kDSStdRecordTypePrefix, uiStdLen) == 0 )
		{
			//now deal with the standard mappings
			outResult = ExtractRecMap( inRecType, inIndex, outOCGroup, outOCListCFArray, outScope );
		}
		else if ( inIndex == 1 )
		{
			//passthrough since we don't know what to do with it
			//and we assume the DS API client knows that the LDAP server
			//can handle this record type

			outResult = strdup( inRecType );
			DbgLog( kLogPlugin, "CLDAPNodeConfig::MapRecToSearchBase - Warning native record type with no provided prefix <%s> is being used",
				   outResult );
		}//passthrough map
		
	}// ( ( inRecType != NULL ) && (inIndex > 0) )
	
	return( outResult );
	
} // MapRecToSearchBase

char *CLDAPNodeConfig::MapAttrToLDAPType( const char *inRecType, const char *inAttrType, int inIndex, bool bSkipLiteralMappings )
{
	char		*outResult	= NULL;
	UInt32		uiStrLen	= 0;
	UInt32		uiNativeLen	= sizeof(kDSNativeAttrTypePrefix) - 1;
	UInt32		uiStdLen	= sizeof(kDSStdAttrTypePrefix) - 1;
	
	//idea here is to use the inIndex to request a specific map
	//if inIndex is 1 then the first map will be returned
	//if inIndex is >= 1 and <= totalCount then that map will be returned
	//if inIndex is <= 0 then NULL will be returned
	//if inIndex is > totalCount NULL will be returned
	//note the inIndex will reference the inIndexth entry ie. start at 1
	//caller can increment the inIndex starting at one and get maps until NULL is returned
	
	if ( inAttrType != NULL && inIndex > 0 )
	{
		uiStrLen = strlen( inAttrType );
		
		// First look for native attribute type
		if ( strncmp(inAttrType, kDSNativeAttrTypePrefix, uiNativeLen) == 0 )
		{
			// Make sure we have data past the prefix
			if ( uiStrLen > uiNativeLen && inIndex == 1 )
			{
				outResult = strdup( inAttrType + uiNativeLen );
				DbgLog( kLogPlugin, "CLDAPNodeConfig::MapAttrToLDAPType - Warning Native attribute type <%s> is being used", outResult );
			}
		}
		else if ( strncmp(inAttrType, kDSStdAttrTypePrefix, uiStdLen) == 0 )
		{
			if ( bSkipLiteralMappings == false )
			{
				outResult = ExtractAttrMap( inRecType, inAttrType, inIndex );
			}
			else
			{
				while( 1 )
				{
					outResult = ExtractAttrMap( inRecType, inAttrType, inIndex );
					if ( outResult == NULL )
						break;
					
					if ( outResult[0] == '#' )
					{
						DSFree( outResult );
					}
					else
					{
						break;
					}

					inIndex++;
				}
			}
		}
		else
		{
			//passthrough since we don't know what to do with it
			//and we assume the DS API client knows that the LDAP server
			//can handle this attribute type
			if ( inIndex == 1 )
			{
				outResult = strdup( inAttrType );
				DbgLog( kLogPlugin, "CLDAPNodeConfig::MapAttrToLDAPType - Warning Native attribute type with no provided prefix <%s> is being used", 
					    outResult );
			}
		}
	}
	
	return( outResult );
	
} // MapAttrToLDAPType

char **CLDAPNodeConfig::MapAttrToLDAPTypeArray( const char *inRecType, const char *inAttrType )
{
	char		**outResult	= NULL;
	UInt32		uiStrLen	= 0;
	UInt32		uiNativeLen	= sizeof(kDSNativeAttrTypePrefix) - 1;
	UInt32		uiStdLen	= sizeof(kDSStdAttrTypePrefix) - 1;
	int			countNative	= 0;
	
	if ( inAttrType != NULL )
	{
		uiStrLen = strlen( inAttrType );
		
		// First look for native attribute type
		if ( strncmp(inAttrType, kDSNativeAttrTypePrefix, uiNativeLen) == 0 )
		{
			// Make sure we have data past the prefix
			if ( uiStrLen > uiNativeLen )
			{
				uiStrLen = uiStrLen - uiNativeLen;
				outResult = (char **) calloc( 2, sizeof( char * ) );
				(*outResult) = strdup( inAttrType + uiNativeLen );
				DbgLog( kLogPlugin, "CLDAPNodeConfig::MapAttrToLDAPTypeArray - Warning Native attribute type <%s> is being used", *outResult );
			}
		}
		else if ( strncmp(inAttrType, kDSStdAttrTypePrefix, uiStdLen) == 0 )
		{
			fMutex.WaitLock();
			
			// need to know number of native maps first to alloc the outResult
			countNative = AttrMapsCount( inRecType, inAttrType );
			if ( countNative > 0 )
			{
				int		aIndex		= 1;
				int		usedIndex	= 0;

				outResult = (char **) calloc( countNative+1, sizeof(char *) );
				while ( 1 )
				{
					char *singleMap = ExtractAttrMap( inRecType, inAttrType, aIndex );
					if ( singleMap == NULL )
						break;
					
					if ( singleMap[0] != '#' )
					{
						outResult[usedIndex] = singleMap; //usedIndex is zero based
						usedIndex++;
						singleMap = NULL;
					}
					else
					{
						break;
					}

					DSFree( singleMap );
					aIndex++;
				}
			}
			
			fMutex.SignalLock();
			
		}
		else
		{
			//passthrough since we don't know what to do with it and we assume the DS API client knows that the LDAP server
			//can handle this attribute type
			outResult = (char **) calloc( 2, sizeof(char *) );
			(*outResult) = strdup( inAttrType );
			DbgLog( kLogPlugin, "CLDAPNodeConfig::MapAttrToLDAPTypeArray - Warning Native attribute type with no provided prefix <%s> is being used",
				    *outResult );
		}
	}
	
	return outResult;
}

char *CLDAPNodeConfig::ExtractRecMap( const char *inRecType, int inIndex, bool *outOCGroup, CFArrayRef *outOCListCFArray,
									  ber_int_t* outScope )
{
	char	*outResult	= NULL;

	fMappingsLock.WaitLock();

	// the map dictionary is normalized, no need to check types of values
	if ( (fNormalizedMappings != NULL) && (inRecType != NULL) )
	{
		
		CFStringRef		cfRecTypeRef	= CFStringCreateWithCStringNoCopy( kCFAllocatorDefault, inRecType, kCFStringEncodingUTF8, kCFAllocatorNull );
		CFDictionaryRef cfRecordMap		= NULL;
		
		if ( cfRecTypeRef != NULL )
			cfRecordMap = (CFDictionaryRef) CFDictionaryGetValue( fNormalizedMappings, cfRecTypeRef );
		
		// if we got a map, we can continue..
		if ( cfRecordMap != NULL )
		{
			CFArrayRef		cfNativeArray	= (CFArrayRef) CFDictionaryGetValue( cfRecordMap, CFSTR(kXMLNativeMapArrayKey) );
			
			if ( inIndex <= CFArrayGetCount(cfNativeArray) )
			{
				CFDictionaryRef cfCurrentMap	= (CFDictionaryRef) CFArrayGetValueAtIndex( cfNativeArray, inIndex-1 );
				CFStringRef		searchBase		= (CFStringRef) CFDictionaryGetValue( cfCurrentMap, CFSTR(kXMLSearchBase) );
				
				if ( searchBase != NULL )
				{
					UInt32 uiLength = (UInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(searchBase), kCFStringEncodingUTF8 ) + 1;
					outResult = (char *) calloc( sizeof(char), uiLength );
					CFStringGetCString( searchBase, outResult, uiLength, kCFStringEncodingUTF8 );
				}
				
				//now deal with the objectclass entries if appropriate
				if ( outOCListCFArray != NULL && outOCGroup != NULL )
				{
					*outOCGroup = 0;
					
					CFArrayRef objectClasses = (CFArrayRef)CFDictionaryGetValue( cfCurrentMap, CFSTR(kXMLObjectClasses) );
					if ( objectClasses != NULL )
					{
						CFStringRef groupOCString = (CFStringRef) CFDictionaryGetValue( cfCurrentMap, CFSTR(kXMLGroupObjectClasses) );
						if ( groupOCString != NULL && CFStringCompare( groupOCString, CFSTR("AND"), 0 ) == kCFCompareEqualTo )
							*outOCGroup = 1;
						*outOCListCFArray = CFArrayCreateCopy(kCFAllocatorDefault, objectClasses);
					}
				}
				
				if ( outScope != NULL )
				{
					CFBooleanRef cfBoolRef = (CFBooleanRef) CFDictionaryGetValue( cfCurrentMap, CFSTR(kXMLOneLevelSearchScope) );
					
					if ( cfBoolRef != NULL && CFBooleanGetValue(cfBoolRef) )
						*outScope = LDAP_SCOPE_ONELEVEL;
					else
						*outScope = LDAP_SCOPE_SUBTREE;
				}
			} //inIndex <= CFArrayGetCount(cfNativeArray)
		}
		
		DSCFRelease( cfRecTypeRef );
		
	} // if (fNormalizedMappings != NULL) ie. a dictionary of Record Maps exists

	fMappingsLock.SignalLock();

	return( outResult );

} // ExtractRecMap

char *CLDAPNodeConfig::ExtractAttrMap( const char *inRecType, const char *inAttrType, int inIndex )
{
	char	*outResult	= NULL;

	fMappingsLock.WaitLock();

	// dictionaries are normalized, everything will exist as expected
	if ( (fNormalizedMappings != NULL) && (inRecType != NULL) && (inAttrType != NULL) && (inIndex >= 1) )
	{
		CFDictionaryRef cfRecordMap		= NULL;
		CFStringRef		cfRecTypeRef	= CFStringCreateWithCString(kCFAllocatorDefault, inRecType, kCFStringEncodingUTF8);
		
		if ( cfRecTypeRef != NULL )
		{
			cfRecordMap = (CFDictionaryRef) CFDictionaryGetValue( fNormalizedMappings, cfRecTypeRef );
		
			// if we got a map, we can continue..
			if ( cfRecordMap != NULL )
			{
				CFDictionaryRef	cfAttrMapDictRef	= (CFDictionaryRef) CFDictionaryGetValue( cfRecordMap, CFSTR(kXMLAttrTypeMapDictKey) );
				
				// if a specific map is available..
				if ( cfAttrMapDictRef != NULL )
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
					
							UInt32 uiLength = (UInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(nativeMapString), kCFStringEncodingUTF8 ) + 1;
							outResult = (char *) calloc( sizeof(char), uiLength );
							CFStringGetCString( nativeMapString, outResult, uiLength, kCFStringEncodingUTF8 );
						}
						
						DSCFRelease(cfAttrTypeRef);
					}
				}
			}//if ( cfRecordMap != NULL )
			
			DSCFRelease(cfRecTypeRef);
		}
	} // if (inRecordTypeMapCFArray != NULL) ie. an array of Record Maps exists
	
	fMappingsLock.SignalLock();

	return( outResult );

} // ExtractAttrMap

char *CLDAPNodeConfig::ExtractStdAttrName( char *inRecType, int &inputIndex )
{
	char	*outResult	= NULL;
	
	fMappingsLock.WaitLock();

	// this routine gets the next standard attribute at an index for a given record type... 
	if ( (fNormalizedMappings != NULL) && (inRecType != NULL) && (inputIndex >= 1) )
	{
		CFDictionaryRef cfRecordMap		= NULL;
		CFStringRef		cfRecTypeRef	= CFStringCreateWithCString(kCFAllocatorDefault, inRecType, kCFStringEncodingUTF8);
		
		if ( cfRecTypeRef != NULL )
		{
			cfRecordMap = (CFDictionaryRef) CFDictionaryGetValue( fNormalizedMappings, cfRecTypeRef );
		
			// if we got a map, we can continue..
			if ( cfRecordMap != NULL )
			{
				//now we can retrieve the map dictionary
				CFDictionaryRef cfAttrMapDict	= (CFDictionaryRef) CFDictionaryGetValue( cfRecordMap, CFSTR( kXMLAttrTypeMapDictKey ) );
					
				// now we have to get values & keys so we can step through them...
				// get the native map array of labels next
				if (cfAttrMapDict != NULL)
				{
					CFIndex iTotalEntries	= CFDictionaryGetCount( cfAttrMapDict );
					
					if ( inputIndex <= iTotalEntries )
					{
						CFStringRef	*keys = (CFStringRef *) calloc( iTotalEntries, sizeof(CFStringRef) );
						if ( keys != NULL )
						{
							CFDictionaryGetKeysAndValues( cfAttrMapDict, (const void **)keys, NULL );
							
							UInt32 uiLength = (UInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(keys[inputIndex - 1]), kCFStringEncodingUTF8 ) + 1;
							outResult = (char *) calloc( sizeof(char), uiLength );
							CFStringGetCString( keys[inputIndex - 1], outResult, uiLength, kCFStringEncodingUTF8 );

							DSFree( keys );
						}
					}
				}
			}
		
			DSCFRelease(cfRecTypeRef);
		}
	} // if (inRecordTypeMapCFArray != NULL) ie. an array of Record Maps exists

	fMappingsLock.SignalLock();

	return( outResult );

} // ExtractStdAttr

int CLDAPNodeConfig::AttrMapsCount( const char *inRecType, const char *inAttrType )
{
	int	outCount	= 0;

	fMappingsLock.WaitLock();

	// dictionaries are normalized, everything will exist as expected
	if ( (fNormalizedMappings != NULL) && (inRecType != NULL) && (inAttrType != NULL) )
	{
		CFStringRef		cfRecTypeRef	= CFStringCreateWithCString(kCFAllocatorDefault, inRecType, kCFStringEncodingUTF8);
		CFDictionaryRef cfRecordMap		= NULL;
		
		if ( cfRecTypeRef != NULL )
			cfRecordMap = (CFDictionaryRef) CFDictionaryGetValue( fNormalizedMappings, cfRecTypeRef );
		
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
					DSCFRelease( cfAttrTypeRef );
				}
			}
		}
		DSCFRelease( cfRecTypeRef );
		
	} // if (fNormalizedMappings != NULL) ie. an array of Record Maps exists

	fMappingsLock.SignalLock();

	return( outCount );

} // AttrMapsCount

CFDictionaryRef	CLDAPNodeConfig::CopyNormalizedMappings( void )
{
	CFDictionaryRef	cfReturnValue = NULL;
	
	fMappingsLock.WaitLock();
	if ( fNormalizedMappings != NULL )
		cfReturnValue = CFDictionaryCreateCopy( kCFAllocatorDefault, fNormalizedMappings );
	fMappingsLock.SignalLock();
	
	return cfReturnValue;
}

void CLDAPNodeConfig::GetReqAttrListForObjectList( CLDAPConnection *inLDAPConnection, listOfStrings &inObjectClassList, 
                                                   listOfStrings &outReqAttrsList )
{
	// if we still haven't gotten the schema, let's look again since we are trying to add something and need to know the hierarchy
	if ( fObjectClassSchema == NULL ) {
		
		// lock LDAP session calls fMutex lock, so we can't grab lock before hand let RetrieveServerSchema handle the lock
		LDAP *aHost = inLDAPConnection->LockLDAPSession();
		if ( aHost != NULL ) {
			RetrieveServerSchema( aHost );
			inLDAPConnection->UnlockLDAPSession( aHost, false );
		}
	}
	
	fMutex.WaitLock();
	
	if ( fObjectClassSchema != NULL )
	{
		//now we look at the objectclass list provided by the user and expand it fully with the hierarchy info
		for (listOfStringsCI iter = inObjectClassList.begin(); iter != inObjectClassList.end(); ++iter)
		{
			//const char *aString = (*iter).c_str();
			if ( fObjectClassSchema->count(*iter) != 0 )
			{
				ObjectClassMap::iterator mapIter = fObjectClassSchema->find(*iter);
				for (AttrSetCI parentIter = mapIter->second->fParentOCs.begin(); parentIter != mapIter->second->fParentOCs.end(); ++parentIter)
				{
					bool addObjectClassName = true;
					for (listOfStringsCI dupIter = inObjectClassList.begin(); dupIter != inObjectClassList.end(); ++dupIter)
					{
						if (*dupIter == *parentIter) //already in the list
						{
							addObjectClassName = false;
							break;
						}
					}
					if (addObjectClassName)
					{
						inObjectClassList.push_back(*parentIter);
					}
				}
			}
		}
		
		//now that we have the objectclass list we can build a similar list of the required creation attributes
		for (listOfStringsCI iter = inObjectClassList.begin(); iter != inObjectClassList.end(); ++iter)
		{
			if (fObjectClassSchema->count(*iter) != 0)
			{
				ObjectClassMap::iterator mapIter = fObjectClassSchema->find(*iter);
				for (AttrSetCI reqIter = mapIter->second->fRequiredAttrs.begin(); reqIter != mapIter->second->fRequiredAttrs.end(); ++reqIter)
				{
					if (	(*reqIter != "objectClass") &&					// explicitly added already
						(*reqIter != "nTSecurityDescriptor") &&			// exclude nTSecurityDescriptor special AD attr type
						(*reqIter != "objectCategory") &&				// exclude objectCategory special AD attr type
						(*reqIter != "instanceType") )					// exclude instanceType special AD attr type
					{
						bool addReqAttr = true;
						for (listOfStringsCI dupIter = outReqAttrsList.begin(); dupIter != outReqAttrsList.end(); ++dupIter)
						{
							if (*dupIter == *reqIter) //already in the list
							{
								addReqAttr = false;
								break;
							}
						}
						if (addReqAttr)
						{
							outReqAttrsList.push_back(*reqIter);
						}
					}
					if (*reqIter == "nTSecurityDescriptor")		//For AD LDAP we force the addition of the sAMAccountName
					{
						string nameString("sAMAccountName");
						outReqAttrsList.push_back(nameString);
					}
				}
			}
		}
	}
	
	fMutex.SignalLock();
}

#pragma mark -
#pragma mark Filter routines

char *CLDAPNodeConfig::BuildLDAPQueryFilter( char *inConstAttrType, const char *inConstAttrName, tDirPatternMatch patternMatch, 
											 bool useWellKnownRecType, const char *inRecType, char *inNativeRecType, bool inbOCANDGroup, 
											 CFArrayRef inOCSearchList )   
{
    char				   *queryFilter			= NULL;
	UInt32					matchType			= eDSExact;
	char				   *nativeAttrType		= NULL;
	UInt32					recNameLen			= 0;
	int						numAttributes		= 1;
	CFMutableStringRef		cfStringRef			= NULL;
	CFMutableStringRef		cfQueryStringRef	= NULL;
	char				   *escapedName			= NULL;
	UInt32					escapedIndex		= 0;
	UInt32					originalIndex		= 0;
	bool					bOnceThru			= false;
	UInt32					offset				= 3;
	UInt32					callocLength		= 0;
	bool					objClassAdded		= false;
	bool					bGetAllDueToLiteralMapping = false;
	int						aOCSearchListCount	= 0;
	
	cfQueryStringRef = CFStringCreateMutable( kCFAllocatorDefault, 0 );
	
	//build the objectclass filter prefix condition if available and then set objClassAdded
	//before the original filter on name and native attr types
	//use the inConfigTableIndex to access the mapping config

	//check for NULL and then check if this is a standard type so we have a chance there is an objectclass mapping
	if ( (inRecType != NULL) && (inNativeRecType != NULL) )
	{
		if ( strncmp(inRecType, kDSStdRecordTypePrefix, sizeof(kDSStdRecordTypePrefix)-1) == 0 )
		{
			//determine here whether or not there are any objectclass mappings to include
			if (inOCSearchList != NULL)
			{
				//here we extract the object class strings
				//combine using "&" if inbOCANDGroup otherwise use "|"
				aOCSearchListCount = CFArrayGetCount(inOCSearchList);
				for ( int iOCIndex = 0; iOCIndex < aOCSearchListCount; iOCIndex++ )
				{
					CFStringRef	ocString;
					ocString = (CFStringRef)::CFArrayGetValueAtIndex( inOCSearchList, iOCIndex );
					if (ocString != NULL)
					{
						if (!objClassAdded)
						{
							objClassAdded = true;
							if (inbOCANDGroup)
							{
								CFStringAppendCString(cfQueryStringRef,"(&", kCFStringEncodingUTF8);
							}
							else
							{
								CFStringAppendCString(cfQueryStringRef,"(&(|", kCFStringEncodingUTF8);
							}
						}
						
						CFStringAppendCString(cfQueryStringRef, "(objectclass=", kCFStringEncodingUTF8);

						// do we need to escape any of the characters internal to the CFString??? like before
						// NO since "*, "(", and ")" are not legal characters for objectclass names
						CFStringAppend(cfQueryStringRef, ocString);
						
						CFStringAppendCString(cfQueryStringRef, ")", kCFStringEncodingUTF8);
					}
				}// loop over the objectclasses CFArray
				if (CFStringGetLength(cfQueryStringRef) != 0)
				{
					if (!inbOCANDGroup)
					{
						//( so PB bracket completion works
						CFStringAppendCString(cfQueryStringRef,")", kCFStringEncodingUTF8);
					}
				}
			}
		}// if (strncmp(inRecType,kDSStdRecordTypePrefix,strlen(kDSStdRecordTypePrefix)) == 0)
	}// if ( (inRecType != NULL) && (inNativeRecType != NULL) )
	
	//check for case of NO objectclass mapping BUT also no inConstAttrName meaning we want to have
	//the result of (objectclass=*)
	if ( (CFStringGetLength(cfQueryStringRef) == 0) && (inConstAttrName == NULL) )
	{
		CFStringAppendCString(cfQueryStringRef,"(&(objectclass=*)", kCFStringEncodingUTF8);
		objClassAdded = true;
	}
	
	//here we decide if this is eDSCompoundExpression or eDSiCompoundExpression so that we special case this
	if (	(patternMatch == eDSCompoundExpression) ||
			(patternMatch == eDSiCompoundExpression) )
	{ //KW right now it is always case insensitive
		
		CFStringRef cfTempString = ParseCompoundExpression( inConstAttrName, inRecType );
		if (cfTempString != NULL)
		{
			CFStringAppend(cfQueryStringRef, cfTempString);
			DSCFRelease(cfTempString);
		}
			
	}
	else
	{
		//first check to see if input not NULL
		if (inConstAttrName != NULL)
		{
			recNameLen = strlen(inConstAttrName);
			escapedName = (char *)::calloc(1, 3 * recNameLen + 1);
			// assume at most all characters will be escaped
			while (originalIndex < recNameLen)
			{
				switch (inConstAttrName[originalIndex])
				{
					// add \ escape character then hex representation of original character
					case '*':
						escapedName[escapedIndex] = '\\';
						++escapedIndex;
						escapedName[escapedIndex] = '2';
						++escapedIndex;
						escapedName[escapedIndex] = 'a';
						++escapedIndex;                        
						break;
					case '(':
						escapedName[escapedIndex] = '\\';
						++escapedIndex;
						escapedName[escapedIndex] = '2';
						++escapedIndex;
						escapedName[escapedIndex] = '8';
						++escapedIndex;                        
						break;
					case ')':
						escapedName[escapedIndex] = '\\';
						++escapedIndex;
						escapedName[escapedIndex] = '2';
						++escapedIndex;
						escapedName[escapedIndex] = '9';
						++escapedIndex;                        
						break;
					case '\\':
						escapedName[escapedIndex] = '\\';
						++escapedIndex;
						escapedName[escapedIndex] = '5';
						++escapedIndex;
						escapedName[escapedIndex] = 'c';
						++escapedIndex;                        
						break;
					default:
						escapedName[escapedIndex] = inConstAttrName[originalIndex];
						++escapedIndex;
						break;
				}
				++originalIndex;
			}
			
			//assume that the query is "OR" based ie. meet any of the criteria
			cfStringRef = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, CFSTR("(|"));
			
			//get the first mapping
			numAttributes = 1;
			//KW Note that kDS1RecordName is single valued so we are using kDSNAttrRecordName
			//as a multi-mapped std type which will easily lead to multiple values
			nativeAttrType = MapAttrToLDAPType( inRecType, inConstAttrType, numAttributes, false );
			//would throw if first NULL since no more will be found otherwise proceed until NULL
			//however simply set to default LDAP native in this case
			//ie. we are trying regardless if kDSNAttrRecordName is mapped or not
			//whether or not "cn" is a good choice is a different story
			if (nativeAttrType == NULL) //only for first mapping
			{
				nativeAttrType = strdup("cn");
			}
			
			while ( nativeAttrType != NULL )
			{
				if (nativeAttrType[0] == '#') //literal mapping
				{
					if (strlen(nativeAttrType) > 1)
					{
						if (DoesThisMatch(escapedName, nativeAttrType+1, patternMatch))
						{
							if (CFStringGetLength(cfQueryStringRef) == 0)
							{
								CFStringAppendCString(cfQueryStringRef,"(&(objectclass=*)", kCFStringEncodingUTF8);
								objClassAdded = true;
							}
							bGetAllDueToLiteralMapping = true;
							free(nativeAttrType);
							nativeAttrType = NULL;
							continue;
						}
					}
				}
				else
				{
					if (bOnceThru)
					{
						if (useWellKnownRecType)
						{
							//need to allow for condition that we want only a single query
							//that uses only a single native type - use the first one - 
							//for perhaps an open record or a write to a specific record
							bOnceThru = false;
							break;
						}
						offset = 0;
					}
					matchType = (UInt32) (patternMatch);
					switch (matchType)
					{
				//		case eDSAnyMatch:
				//			cout << "Pattern match type of <eDSAnyMatch>" << endl;
				//			break;
						case eDSStartsWith:
						case eDSiStartsWith:
							CFStringAppendCString(cfStringRef,"(", kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef, nativeAttrType, kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef,"=", kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef, escapedName, kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef,"*)", kCFStringEncodingUTF8);
							bOnceThru = true;
							break;
						case eDSEndsWith:
						case eDSiEndsWith:
							CFStringAppendCString(cfStringRef,"(", kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef, nativeAttrType, kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef,"=*", kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef, escapedName, kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef,")", kCFStringEncodingUTF8);
							bOnceThru = true;
							break;
						case eDSContains:
						case eDSiContains:
							CFStringAppendCString(cfStringRef,"(", kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef, nativeAttrType, kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef,"=*", kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef, escapedName, kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef,"*)", kCFStringEncodingUTF8);
							bOnceThru = true;
							break;
						case eDSWildCardPattern:
						case eDSiWildCardPattern:
							//assume the inConstAttrName is wild
							CFStringAppendCString(cfStringRef,"(", kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef, nativeAttrType, kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef,"=", kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef, inConstAttrName, kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef,")", kCFStringEncodingUTF8);
							bOnceThru = true;
							break;
						case eDSRegularExpression:
						case eDSiRegularExpression:
							//assume inConstAttrName replaces entire wild expression
							CFStringAppendCString(cfStringRef, inConstAttrName, kCFStringEncodingUTF8);
							bOnceThru = true;
							break;
						case eDSExact:
						case eDSiExact:
						default:
							CFStringAppendCString(cfStringRef,"(", kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef, nativeAttrType, kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef,"=", kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef, escapedName, kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef,")", kCFStringEncodingUTF8);
							bOnceThru = true;
							break;
					} // switch on matchType
				}
				//cleanup nativeAttrType if needed
				if (nativeAttrType != NULL)
				{
					free(nativeAttrType);
					nativeAttrType = NULL;
				}
				numAttributes++;
				//get the next mapping
				nativeAttrType = MapAttrToLDAPType( inRecType, inConstAttrType, numAttributes, false );
			} // while ( nativeAttrType != NULL )
	
			if (!bGetAllDueToLiteralMapping)
			{
				if (cfStringRef != NULL)
				{
					// building search like "sn=name"
					if (offset == 3)
					{
						CFRange	aRangeToDelete;
						aRangeToDelete.location = 1;
						aRangeToDelete.length = 2;			
						CFStringDelete(cfStringRef, aRangeToDelete);
					}
					//building search like "(|(sn=name)(cn=name))"
					else
					{
						CFStringAppendCString(cfStringRef,")", kCFStringEncodingUTF8);
					}
					
					CFStringAppend(cfQueryStringRef, cfStringRef);
				}
			}
			
			if (cfStringRef != NULL)
			{
				CFRelease(cfStringRef);
				cfStringRef = NULL;
			}

			if (escapedName != NULL)
			{
				free(escapedName);
				escapedName = NULL;
			}
	
		} // if inConstAttrName not NULL
	}
	if (objClassAdded)
	{
		CFStringAppendCString(cfQueryStringRef,")", kCFStringEncodingUTF8);
	}
	
	//here we make the char * output in queryfilter
	if (CFStringGetLength(cfQueryStringRef) != 0)
	{
		callocLength = (UInt32) CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfQueryStringRef), kCFStringEncodingUTF8) + 1;
		queryFilter = (char *) calloc(1, callocLength);
		CFStringGetCString( cfQueryStringRef, queryFilter, callocLength, kCFStringEncodingUTF8 );
	}

	if (cfQueryStringRef != NULL)
	{
		CFRelease(cfQueryStringRef);
		cfQueryStringRef = NULL;
	}
	
	return (queryFilter);
}

CFStringRef CLDAPNodeConfig::ParseCompoundExpression( const char *inConstAttrName, const char *inRecType )
{
	if ( inConstAttrName == NULL || inRecType == NULL )
		return NULL;
		
	const int	kMatchOr			= 1;
	const int	kMatchAnd			= 2;
	const int	kMatchNot			= 3;
	char		*workingString		= strdup( inConstAttrName );
	int			iIndex				= 0;
	int			iDepth				= 0;
	int			iExpression			= 0;	// 0 = LH, 1 = RH
	int			iMatchType[128];			// we allow up to 128 depth, a bit extreme, but safe
	char		*pExpression[2];
	char		*pWorkingBuffer		= NULL;
	bool		bExpand				= false;
	bool		bBadSearch			= true;
	char		currChar;
		
	// we'll base our LH/RH on our filterlen, since it can't be any longer than that
	int iFilterLen = strlen( workingString );
	pExpression[0] = (char *) malloc( iFilterLen );
	pExpression[1] = (char *) malloc( iFilterLen );

	CFMutableStringRef outExpression = CFStringCreateMutable( kCFAllocatorDefault, 0 );
	
	while ( (currChar = workingString[iIndex]) != '\0' )
	{
		bool	bAddCharToOut	= true;

		switch (currChar)
		{
			case '(':
				if ( (++iDepth) > 127 ) goto done;
				iExpression = 0;
				pWorkingBuffer = pExpression[0];
				break;
			case ')':
				if ( (--iDepth) < 0 ) goto done;
				if ( pWorkingBuffer != NULL )
				{
					(*pWorkingBuffer) = '\0'; // terminate current
					bExpand = true;
				}
				break;
			case '&':
				iMatchType[iDepth] = kMatchAnd;
				break;
			case '|':
				iMatchType[iDepth] = kMatchOr;
				break;
			case '!':
				iMatchType[iDepth] = kMatchNot;
				break;
			case '\\': // escaped, need to keep the escape and the next char
				if ( pWorkingBuffer == NULL ) goto done;
				(*pWorkingBuffer) = currChar;
				pWorkingBuffer++;
				currChar = workingString[++iIndex];
				if ( currChar == '\0' ) goto done;
				(*pWorkingBuffer) = currChar;
				pWorkingBuffer++;
				bAddCharToOut = false;
				break;
			case '=':
				if ( pWorkingBuffer == NULL ) goto done;
				(*pWorkingBuffer) = '\0'; // terminate current
				pWorkingBuffer = pExpression[1];
				iExpression = 1;	// move to the right hand expression
				bAddCharToOut = false;
				break;
			default:
				if ( pWorkingBuffer == NULL ) goto done;
				(*pWorkingBuffer) = currChar;
				pWorkingBuffer++;
				bAddCharToOut = false;
				break;
		}
		
		if ( bExpand )
		{
			char **attrNames	= NULL;
			
			// if we don't have a LH and RH, it's a bad expression
			if ( iExpression != 1 ) goto done;
			
			if ( strncasecmp( pExpression[0], kDSStdAttrTypePrefix, sizeof(kDSStdAttrTypePrefix)-1 ) == 0 )
			{
				attrNames = MapAttrToLDAPTypeArray( inRecType, pExpression[0] );
			}
			else if ( strncasecmp( pExpression[0], kDSNativeAttrTypePrefix, sizeof(kDSNativeAttrTypePrefix)-1 ) == 0 )
			{
				attrNames = (char **) calloc( 2, sizeof(char *) );
				attrNames[0] = strdup( pExpression[0] + (sizeof(kDSNativeAttrTypePrefix)-1) );
			}
			else // otherwise take it as is treated as native value
			{
				attrNames = (char **) calloc( 2, sizeof(char *) );
				attrNames[0] = strdup( pExpression[0] );
			}
			
			if ( attrNames != NULL && attrNames[0] != NULL )
			{
				int	iCount	= 0;
				
				for ( char **tempNames = attrNames; (*tempNames) != NULL; iCount++, tempNames++ );
				
				// if this is a multi-map, more work to do
				if ( iCount > 1 )
				{
					CFStringAppend( outExpression, CFSTR("|") );
					
					for ( int ii = 0; ii < iCount; ii ++ )
					{
						// pExpression[1] can be UTF, so can't use a single appendFormat with %s.
						CFStringAppendFormat( outExpression, 0, CFSTR("(%s="), attrNames[ii] );
						CFStringAppendCString( outExpression, pExpression[1], kCFStringEncodingUTF8 );
						CFStringAppend( outExpression, CFSTR(")") );
					}
				}
				else
				{
					// pExpression[1] can be UTF, so can't use a single appendFormat with %s.
					CFStringAppendFormat( outExpression, 0, CFSTR("%s="), attrNames[0] );
					CFStringAppendCString( outExpression, pExpression[1], kCFStringEncodingUTF8 );
				}
			}
			else if ( iMatchType[iDepth] == kMatchAnd )
			{
				// explicitly looking for &, need to bail, no mapping
				goto done;
			}
			else
			{
				// remove the "(" and don't add the ")"
				CFStringDelete( outExpression, CFRangeMake(CFStringGetLength(outExpression)-1, 1) );
				bAddCharToOut = false;
			}
			
			DSFreeStringList( attrNames );
			
			// reset and start next round
			pWorkingBuffer = NULL;
			bExpand = false;
			iExpression = 0;
		}
		
		if ( bAddCharToOut )
		{
			char	cBuffer[2]	= { currChar, 0 };

			CFStringAppendCString( outExpression, cBuffer, kCFStringEncodingUTF8 );
		}
		
		iIndex++;
	}

	// if we finished our loop and we have iDepth of 0, we have a complete search
	if ( iDepth == 0 )
		bBadSearch = false;
	
done:
	if ( bBadSearch )
		DSCFRelease( outExpression );
	
	DSFree( workingString );
	DSFree( pExpression[0] );
	DSFree( pExpression[1] );
	
	return( outExpression );

} // ParseCompoundExpression

#pragma mark -

char *CLDAPNodeConfig::CopyUIName( void )
{
	char	*returnValue = NULL;
	
	fMutex.WaitLock();
	if ( fConfigUIName != NULL )
		returnValue = strdup( fConfigUIName );
	fMutex.SignalLock();
	
	return returnValue;
}

char *CLDAPNodeConfig::CopyMapSearchBase( void )
{
	char	*returnValue = NULL;
	
	fMutex.WaitLock();
	if ( fMapSearchBase != NULL )
		returnValue = strdup( fMapSearchBase );
	fMutex.SignalLock();
	
	return returnValue;
}

bool CLDAPNodeConfig::CopyCredentials( char **outUsername, char **outKerberosID, char **outPassword )
{
	fMutex.WaitLock();
	if ( fSecureUse )
	{
		if ( fServerAccount != NULL )
			(*outUsername) = strdup( fServerAccount );
		
		if ( fServerKerberosID != NULL )
			(*outKerberosID) = strdup( fServerKerberosID );

		if ( fServerPassword != NULL )
			(*outPassword) = strdup( fServerPassword );
	}
	fMutex.SignalLock();
	
	return fSecureUse;
}

void CLDAPNodeConfig::NetworkTransition( void )
{
	OSAtomicCompareAndSwap32Barrier( false, true, &fGetReplicas );

	// need to protect this one, we can't atomic swap it
	fMutex.WaitLock();
	fLastFailedCheck = 0.0;
	fMutex.SignalLock();
}

bool CLDAPNodeConfig::CheckIfFailed( void )
{
	bool	bNameResolves	= true;
	bool	bReturnValue	= true;
	
	fMutex.WaitLock();
	
	// if we have a reachability ref, it must be a name, so lets see if it resolves
	if ( fReachabilityRef != NULL && fAvailable == false && fServerName != NULL )
	{
		struct addrinfo *addrList = NULL;
		
		if ( getaddrinfo(fServerName, NULL, NULL, &addrList) == 0 )
		{
			OSAtomicCompareAndSwap32Barrier( false, true, &fAvailable );
			freeaddrinfo( addrList );
		}
		else
		{
			OSAtomicCompareAndSwap32Barrier( true, false, &fAvailable );
			DbgLog( kLogPlugin, "CLDAPNodeConfig::CheckIfFailed - name %s - does not resolve assuming no connectivity", fServerName );
			bNameResolves = false;
		}
	}
	fMutex.SignalLock();
	
	if ( bNameResolves == true )
	{
		bool				bForceCheck		= false;
		CLDAPReplicaInfo	*tempReplica	= NULL;

		// force a check if it is time
		fMutex.WaitLock();
		CFAbsoluteTime timeCheck = CFAbsoluteTimeGetCurrent();
		if ( timeCheck - fLastFailedCheck > fDelayRebindTry )
		{
			fLastFailedCheck = timeCheck;
			bForceCheck = true;
		}
		fMutex.SignalLock();
		
		LDAP *pTempLD = FindSuitableReplica( &tempReplica, bForceCheck, false, NULL, NULL );
		if ( pTempLD != NULL )
		{
			DSRelease( tempReplica );
			// we don't keep this connection it was just exploratory
			ldap_unbind_ext_s( pTempLD, NULL, NULL );
			pTempLD = NULL;
			
			bReturnValue = false;
		}
	}
	
	return bReturnValue;
}

#pragma mark -
#pragma mark Callbacks

void CLDAPNodeConfig::ReachabilityCallback( SCNetworkReachabilityRef inTarget, SCNetworkConnectionFlags inFlags, void *inInfo )
{
	((CLDAPNodeConfig *) inInfo)->ReachabilityNotification( inFlags );
}

#pragma mark -
#pragma mark Private Routines

void CLDAPNodeConfig::ReachabilityNotification( SCNetworkConnectionFlags inFlags )
{
	if ( (inFlags & kSCNetworkFlagsReachable) == 0 || (inFlags & kSCNetworkFlagsConnectionRequired) != 0 )
		OSAtomicCompareAndSwap32Barrier( true, false, &fAvailable );
	else
		OSAtomicCompareAndSwap32Barrier( false, true, &fAvailable );

	DbgLog( kLogPlugin, "CLDAPNodeConfig::ReachabilityNotification - Node: %s - %s", fNodeName,  
		    (fAvailable ? "resolves - enabled" : "does not resolve - disabled") );
}

void CLDAPNodeConfig::SetLDAPOptions( LDAP *inLDAP )
{
	int version = LDAP_VERSION3;
	ldap_set_option( inLDAP, LDAP_OPT_PROTOCOL_VERSION, &version );

	if ( fIsSSL )
	{
		int ldapOptVal = LDAP_OPT_X_TLS_HARD;
		ldap_set_option( inLDAP, LDAP_OPT_X_TLS, &ldapOptVal );
	}
	
	ldap_set_option( inLDAP, LDAP_OPT_REFERRALS, (fReferrals ? LDAP_OPT_ON : LDAP_OPT_OFF) );
	
	if ( (fSecurityLevel & kSecPacketEncryption) != 0 )
		ldap_set_option( inLDAP, LDAP_OPT_X_SASL_SECPROPS, (void *) "minssf=56" );
	else if ( (fSecurityLevel & kSecPacketSigning) != 0 )
		ldap_set_option( inLDAP, LDAP_OPT_X_SASL_SECPROPS, (void *) "minssf=1" );
}

void CLDAPNodeConfig::InitializeVariables( void )
{
	fConfigUUID = NULL;
	fNodeName = NULL;
	fDHCPLDAPServer = false;
	fNodeIsLDAPURL = false;
	
	fIsSSL = false;
	fIdleMaxCount = 2;
	fSearchTimeout = kLDAPDefaultSearchTimeoutInSeconds;
	fOpenCloseTimeout = kLDAPDefaultOpenCloseTimeoutInSeconds;
	fDelayRebindTry = kLDAPDefaultRebindTryTimeoutInSeconds;
	fAvailable = false;
	fSecureUse = false;
	fSecurityLevel = kSecNoSecurity;
	
	fConfigDeleted = false;
	
	fConfigObject = NULL;
	
	fServerAccount = NULL;
	fServerKerberosID = NULL;
	fServerPassword = NULL;
	
	fMapSearchBase = NULL;
	fServerName = NULL;
	fConfigUIName = NULL;
	
	fGetServerMappings = true;
	fGetReplicas = true;
	fGetSecuritySettings = true;
	
	fDynamicRefreshTimer = dispatch_source_timer_create( DISPATCH_TIMER_INTERVAL, 
														 (24ull * 3600ull * NSEC_PER_SEC), 
														 60ull * NSEC_PER_SEC,
														 NULL,
														 dispatch_get_concurrent_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT), 
														 ^(dispatch_source_t ds) {
															 if ( dispatch_source_get_error(ds, NULL) == 0 ) {
																 fGetReplicas = true;
																 fGetSecuritySettings = true;
																 
																 if ( fServerMappings == true )
																	 fGetServerMappings = true;
																 
																 DbgLog( kLogPlugin, "CLDAPNodeConfig::RefreshDynamicData - Node: %s - all dynamically discovered data will be refreshed", fNodeName );
															 }
														 } );
	
	fNormalizedMappings = NULL;
	fRecordTypeMapArray = NULL;
	fAttrTypeMapArray = NULL;

	fReadReplicas = NULL;
	fWriteReplicas = NULL;
	fDeniedSASLMethods = NULL;

	fObjectClassSchema = NULL;
	
	fReachabilityRef = NULL;
	
	fServerPort = LDAP_PORT;
	fReferrals = false;
	fEnableUse = true;
	fDNSReplicas = false;
	
	fServerMappings = false;
	
	fLocalSecurityLevel = kSecNoSecurity;
	fLastFailedCheck = 0.0;
}

void CLDAPNodeConfig::ClearSockList( int *inSockList, int inSockCount, bool inClose )
{
	for (int iCount = 0; iCount < inSockCount; iCount++)
	{
		if ( inClose == true && inSockList[iCount] >= 0 )
			close( inSockList[iCount] );
		
		inSockList[iCount] = -1;
	}
}

bool CLDAPNodeConfig::IsLocalAddress( struct addrinfo *addrInfo )
{
    struct ifaddrs *ifa_list = NULL, *ifa = NULL;
	bool			bReturn = false;
    
    if ( getifaddrs(&ifa_list) == 0 )
	{
		for( ifa = ifa_list; ifa; ifa = ifa->ifa_next )
		{
			for ( struct addrinfo *tempAddr = addrInfo; tempAddr != NULL; tempAddr = tempAddr->ai_next )
			{
				if ( ifa->ifa_addr->sa_family == tempAddr->ai_addr->sa_family )
				{
					if ( ifa->ifa_addr->sa_family == AF_INET )
					{
						struct sockaddr_in *interface = (struct sockaddr_in *) ifa->ifa_addr;
						struct sockaddr_in *check = (struct sockaddr_in *) tempAddr->ai_addr;
						
						if ( interface->sin_addr.s_addr == check->sin_addr.s_addr )
						{
							bReturn = true;
							break;
						}
					}
					
					if ( ifa->ifa_addr->sa_family == AF_INET6 )
					{
						struct sockaddr_in6 *interface = (struct sockaddr_in6 *)ifa->ifa_addr;
						struct sockaddr_in6 *check = (struct sockaddr_in6 *)tempAddr->ai_addr;
						
						if ( memcmp(&interface->sin6_addr, &check->sin6_addr, sizeof(struct in6_addr)) == 0 )
						{
							bReturn = true;
							break;
						}
					}
				}
			}
		}
		
		freeifaddrs(ifa_list);
	}
	
	return bReturn;
}

LDAP *CLDAPNodeConfig::CheckWithSelect( fd_set &inSet, struct timeval *inCheckTime, int inCount, int *inSockList, 
									    CLDAPReplicaInfo **inReplicas, CLDAPReplicaInfo **outSelectedReplica, 
									    void *inCallback, void *inParam )
{
	LDAP		*pReturnLD	= NULL;
	fd_set		fdwrite;
	fd_set		fdread;

	// let's do our select to see if anything responded....
	FD_COPY( &inSet, &fdwrite );
	FD_COPY( &inSet, &fdread );
	
	// let's do a check to see if we've already gotten a response
	if ( select( FD_SETSIZE, NULL, &fdwrite, NULL, inCheckTime ) > 0 )
	{
		struct timeval	pollValue	= { 0, 0 };

		// now just poll the reads since we should have something
		select( FD_SETSIZE, &fdread, NULL, NULL, &pollValue );

		// now find the ones that match
		for ( int checkIter = 0; checkIter <= inCount; checkIter++ )
		{
			int aSockTemp = inSockList[checkIter];

			if ( aSockTemp == -1 ) continue;
			
			// if we have write, but no read
			if ( FD_ISSET(aSockTemp, &fdwrite) != 0 && FD_ISSET(aSockTemp, &fdread) == 0 )
			{
				pReturnLD = inReplicas[checkIter]->VerifiedServerConnection( 5, true, inCallback, inParam );
				if ( pReturnLD != NULL )
				{
					if ( (inReplicas[checkIter]->fSupportedSecurity & fSecurityLevel) == fSecurityLevel )
					{
						DbgLog( kLogPlugin, "CLDAPNodeConfig::CheckWithSelect - good socket to host %s from poll and verified LDAP", 
							   inReplicas[checkIter]->fIPAddress );
						(*outSelectedReplica) = inReplicas[checkIter]->Retain();
					}
					else
					{
						DbgLog( kLogPlugin, "CLDAPNodeConfig::CheckWithSelect - replica %s doesn't meet security requirements", 
							    inReplicas[checkIter]->fIPAddress );
						ldap_unbind_ext_s( pReturnLD, NULL, NULL );
						pReturnLD = NULL;
					}
				}
				else
				{
					DbgLog( kLogPlugin, "CLDAPNodeConfig::CheckWithSelect - good socket to host %s but failed check, clearing from poll", 
						    inReplicas[checkIter]->fIPAddress );
				}
				FD_CLR( aSockTemp, &inSet );
				break;
			}
			else if ( FD_ISSET(aSockTemp, &fdwrite) != 0 && FD_ISSET(aSockTemp, &fdread) != 0 )
			{
				// if we have a bad socket, we will always get an immediate response so let's remove it from the poll
				DbgLog( kLogPlugin, "CLDAPNodeConfig::CheckWithSelect - Quick Check Bad socket to host %s clearing from poll",
						inReplicas[checkIter]->fIPAddress );
				inReplicas[checkIter]->ConnectionFailed();
				FD_CLR( aSockTemp, &inSet );
				break;
			}
		}
	}
	
	return pReturnLD;
}

LDAP *CLDAPNodeConfig::FindSuitableReplica( CLDAPReplicaInfo **inOutReplicaInfo, bool inForceCheck, bool inWriteable, void *inCallback, void *inParam )
{
	LDAP				*pReturnLD			= NULL;
	int					maxSockets			= 64;		// no reason to check more than 64 replicas
	struct timeval		recvTimeoutVal		= { fOpenCloseTimeout, 0 };
	int					sockCount			= 0;
	int					fcntlFlags			= 0;
	int					sockList[64];
	CLDAPReplicaInfo   *replicaPointers[64];
	int					sockIter;
	bool				bTrySelect			= false;
	fd_set				fdset;

	if ( fAvailable == false && inForceCheck == false ) return NULL;
	
	// special case the ldapi URL here
	if ( ldap_is_ldapi_url(fNodeName) )
	{
		if ( (*inOutReplicaInfo) == NULL )
		{
			ListOfReplicasI replicaIter = fReplicaList.begin();
			if ( replicaIter != fReplicaList.end() )
				(*inOutReplicaInfo) = *replicaIter;
		}
		
		if ( (*inOutReplicaInfo) != NULL )
			return (*inOutReplicaInfo)->VerifiedServerConnection( 10, inForceCheck, inCallback, inParam );
		
		return NULL;
	}
	
	// No reason to check more than 64 replicas
	if ( maxSockets > 64 ) maxSockets = 64;
	
	bzero( replicaPointers, sizeof(replicaPointers) );
	ClearSockList( sockList, maxSockets, false );
	FD_ZERO( &fdset );
	
	fMutex.WaitLock();
	
	// if we have no replica, but we have a server name and our replica list is 0
	if ( (*inOutReplicaInfo) == NULL && fServerName != NULL && fReplicaList.size() == 0 )
	{
		if ( CFArrayGetCount(fReadReplicas) == 0 )
		{
			// need a replica list
			CFStringRef cfServerName = CFStringCreateWithCString( kCFAllocatorDefault, fServerName, kCFStringEncodingUTF8 );
			CFArrayAppendValue( fReadReplicas, cfServerName );
			DSCFRelease( cfServerName );
		}
		
		// ok we should be able to get something at this point
		BuildReplicaList();
	}	
	
	for ( ListOfReplicasI replicaIter = fReplicaList.begin(); replicaIter != fReplicaList.end(); replicaIter++ )
	{
		CLDAPReplicaInfo *replica = (*replicaIter);
		
		// skip the replica if we need a writeable one
		if ( inWriteable == true && replica->fbSupportsWrites == false ) continue;
		
		// if it is local, we should try it blocking because it will fail immediately..
		if ( IsLocalAddress(replica->fAddrInfo) )
		{
			pReturnLD = replica->VerifiedServerConnection( 5, true, inCallback, inParam );
			if ( pReturnLD != NULL )
			{
				// if the replica doesn't meet the minimum requirement
				if ( (replica->fSupportedSecurity & fSecurityLevel) == fSecurityLevel )
				{
					DbgLog( kLogPlugin, "CLDAPNodeConfig::FindSuitableReplica - Node %s - Using local address = %s for %s", fNodeName,
						    replica->fIPAddress, (inWriteable ? "write" : "read") );
					(*inOutReplicaInfo) = replica->Retain();
					break;
				}
				else
				{
					DbgLog( kLogPlugin, "CLDAPNodeConfig::FindSuitableReplica - Node %s - Replica <%s> doesn't meet security requirements", 
						    fNodeName, replica->fIPAddress );
					ldap_unbind_ext_s( pReturnLD, NULL, NULL );
					pReturnLD = NULL;
				}
			}
			else
			{
				DbgLog( kLogPlugin, "CLDAPNodeConfig::FindSuitableReplica - Node %s - Local address failed to connect = %s for %s", 
						fNodeName, replica->fIPAddress, (inWriteable ? "write" : "read") );
			}
		}
		else if ( replica->fReachable && (inForceCheck == true || inWriteable == true || replica->ShouldAttemptCheck()) )
		{
			// if someone is asking for a writeable replica, we will try it regardless
			replicaPointers[sockCount] = replica->Retain();
			sockCount++;
		}
	}
	
	fMutex.SignalLock();
	
	// if we didn't find any usable replicas, nothing to do
	if ( pReturnLD == NULL && sockCount > 0 )
	{
		// so let's go through all sockets until we finish and timeout and don't have an IP address
		for ( sockIter = 0; sockIter < sockCount && pReturnLD == NULL; sockIter++ )
		{
			CLDAPReplicaInfo	*replica	= replicaPointers[sockIter];
			struct addrinfo		*tmpAddress	= replica->fAddrInfo;
			
			// we should do a non-blocking socket...
			int aSock = socket( tmpAddress->ai_family, tmpAddress->ai_socktype, tmpAddress->ai_protocol );
			if ( aSock != -1 )
			{
				int		val = 1;
				
				setsockopt( aSock, SOL_SOCKET, SO_NOSIGPIPE, &val, sizeof(val) );
				setsockopt( aSock, SOL_SOCKET, SO_RCVTIMEO, &recvTimeoutVal, sizeof(recvTimeoutVal) );
				
				// set non-blocking now
				fcntlFlags = fcntl( aSock, F_GETFL, 0 );
				if ( fcntlFlags != -1 )
				{
					if ( fcntl(aSock, F_SETFL, fcntlFlags | O_NONBLOCK) != -1 )
					{
						// if this is a -1, then we add it to our select poll...
						if ( connect(aSock, tmpAddress->ai_addr, tmpAddress->ai_addrlen) == -1 )
						{
							sockList[sockIter] = aSock;
							FD_SET( aSock, &fdset );
							bTrySelect = true;
							
							DbgLog( kLogPlugin, "CLDAPNodeConfig::FindSuitableReplica - Node %s - Attempting Replica connect to %s for %s", 
									fNodeName, replica->fIPAddress, (inWriteable ? "write" : "read") );
						}
						else
						{
							// close the socket and try to get an LDAP pointer
							close( aSock );
							
							// only allow 5 seconds, it responded immediately before
							pReturnLD = replica->VerifiedServerConnection( 5, true, inCallback, inParam );
							if ( pReturnLD != NULL )
							{
								// if the replica doesn't meet the minimum requirement
								if ( (replica->fSupportedSecurity & fSecurityLevel) == fSecurityLevel )
								{
									DbgLog( kLogPlugin, "CLDAPNodeConfig::FindSuitableReplica - Node %s - Immediate response by %s for %s", 
											fNodeName, replica->fIPAddress, (inWriteable ? "write" : "read") );
									(*inOutReplicaInfo) = replica->Retain();
									break;
								}
								else
								{
									DbgLog( kLogPlugin, "CLDAPNodeConfig::FindSuitableReplica - Node %s - Replica <%s> doesn't meet security requirements for %s", 
										   fNodeName, replica->fIPAddress, (inWriteable ? "write" : "read") );
									ldap_unbind_ext_s( pReturnLD, NULL, NULL );
									pReturnLD = NULL;
								}
							}
							else
							{
								DbgLog( kLogPlugin, "CLDAPNodeConfig::FindSuitableReplica - Node %s - Immediate response by %s for %s - but failed check", 
										fNodeName, replica->fIPAddress, (inWriteable ? "write" : "read") );
							}
						}
					}
					else
					{
						close( aSock );
						DbgLog( kLogPlugin, "CLDAPNodeConfig::FindSuitableReplica - Node %s - Unable to do non-blocking connect for socket = %d",
								fNodeName, aSock );
					}
				}
				else
				{
					close( aSock );
					DbgLog( kLogPlugin, "CLDAPNodeConfig::FindSuitableReplica - Node %s - Unable to do get GETFL = %d", fNodeName, aSock );
				}
			}
			
			// if we have no LD and we are supposed to do the select loop
			if ( pReturnLD == NULL && bTrySelect == true )
			{
				struct timeval	quickCheck	= { 0, 5000 };   // 5 milliseconds
				
				pReturnLD = CheckWithSelect( fdset, &quickCheck, sockCount, sockList, replicaPointers, inOutReplicaInfo, inCallback, inParam );
			}
		}
		
		// let's do our polling.....
		int iterTry = 0;
		while ( pReturnLD == NULL && bTrySelect == true && iterTry++ < fOpenCloseTimeout ) 
		{
			struct timeval	recheckTimeoutVal	= { 1, 0 };

			pReturnLD = CheckWithSelect( fdset, &recheckTimeoutVal, sockCount, sockList, replicaPointers, inOutReplicaInfo, inCallback, inParam );
		}
	}
	
	// need to release all of the replica pointers since we retained them above
	for ( int ii = 0; ii < sockCount; ii++ )
		DSRelease( replicaPointers[ii] );
	
	// lets close all of our sockets
	ClearSockList( sockList, sockCount, true );
	
	if ( pReturnLD == NULL )
	{
		// if writeable replica was requested, we won't flag as unavailable
		if ( inWriteable == false )
			OSAtomicCompareAndSwap32Barrier( true, false, &fAvailable );
		
		for ( int ii = 0; ii < sockCount; ii++ )
		{
			if ( replicaPointers[ii] != NULL )
				replicaPointers[ii]->ConnectionFailed();
		}
		
		DbgLog( kLogPlugin, "CLDAPNodeConfig::FindSuitableReplica - Node %s - Could not establish connection for %s", fNodeName,
			    (inWriteable ? "write" : "read") );
	}
	else
	{
		OSAtomicCompareAndSwap32Barrier( false, true, &fAvailable );
		DbgLog( kLogPlugin, "CLDAPNodeConfig::FindSuitableReplica - Node %s - Established connection to %s for %s", 
			    fNodeName, (*inOutReplicaInfo)->fIPAddress, (inWriteable ? "write" : "read") );
	}
	
	return pReturnLD;
}

LDAP *CLDAPNodeConfig::InternalEstablishConnection( CLDAPReplicaInfo **inOutReplicaInfo, bool inWriteable, void *inCallback, void *inParam )
{
	LDAP	*pReturnLD	= NULL;
	
	fMutex.WaitLock();

	// if we have no replica, but we have a server name and our replica list is 0
	if ( (*inOutReplicaInfo) == NULL && fServerName != NULL && fReplicaList.size() == 0 )
	{
		if ( CFArrayGetCount(fReadReplicas) == 0 )
		{
			// need a replica list
			CFStringRef cfServerName = CFStringCreateWithCString( kCFAllocatorDefault, fServerName, kCFStringEncodingUTF8 );
			CFArrayAppendValue( fReadReplicas, cfServerName );
			DSCFRelease( cfServerName );
		}
		
		// ok we should be able to get something at this point
		BuildReplicaList();
	}
		
	DbgLog( kLogPlugin, "CLDAPNodeConfig::InternalEstablishConnection - Node %s - Connection requested for %s", fNodeName, 
		    (inWriteable ? "write" : "read") );
	
	// if we were given a replica, let's try it first
	if ( (*inOutReplicaInfo) != NULL )
	{
		// if we don't need writeable or it supports writes, let's check it
		if ( inWriteable == false || (*inOutReplicaInfo)->fbSupportsWrites == true )
		{
			DbgLog( kLogPlugin, "CLDAPNodeConfig::InternalEstablishConnection - Node %s - attempting previous replica with IP Address = %s for %s",
				   fNodeName, (*inOutReplicaInfo)->fIPAddress, (inWriteable ? "write" : "read") );

			pReturnLD = (*inOutReplicaInfo)->VerifiedServerConnection( 5, true, inCallback, inParam );
			if ( pReturnLD != NULL && ((*inOutReplicaInfo)->fSupportedSecurity & fSecurityLevel) != fSecurityLevel )
			{
				DbgLog( kLogPlugin, "CLDAPNodeConfig::InternalEstablishConnection - Node %s - Previous replica <%s> doesn't meet security requirements for %s", 
					    fNodeName, (*inOutReplicaInfo)->fIPAddress, (inWriteable ? "write" : "read") );
				ldap_unbind_ext_s( pReturnLD, NULL, NULL );
				pReturnLD = NULL;
			}
		}
		
		// if we have an LD, just return it since it works
		if ( pReturnLD != NULL )
		{
			DbgLog( kLogPlugin, "CLDAPNodeConfig::InternalEstablishConnection - Node %s - Previous replica with IP Address = %s responded for %s",
				    fNodeName, (*inOutReplicaInfo)->fIPAddress, (inWriteable ? "write" : "read") );
		}
		else
		{
			// must not be usable, release it and clear the pointer so we can check
			DSRelease( *inOutReplicaInfo );
		}
	}
	
searchAgain:	// we jump up here and re-do it all if we were getting replicas, mappings etc.
	
	if ( pReturnLD == NULL )
		pReturnLD = FindSuitableReplica( inOutReplicaInfo, false, inWriteable, inCallback, inParam );
	
	if ( pReturnLD != NULL && (fGetServerMappings == true || fGetSecuritySettings == true || fGetReplicas == true) )
	{
		bool	bDoAgain	= false;
		
		// first set all of our options
		SetLDAPOptions( pReturnLD );

		char *pServerAccount = NULL;
		char *pServerKerberosID = NULL;
		char *pServerPassword = NULL;
		bool bDynamicDataNeedsUpdate = false;
		
		// copy credentials returns fSecureUse so if it is false or AuthenticateCredentials succeeds we update dynamic data
		// we have to make copy because AuthenticateUsingCredentials might give up mutex for Kerberos
		if ( CopyCredentials(&pServerAccount, &pServerKerberosID, &pServerPassword) == false )
		{
			bDynamicDataNeedsUpdate = true;
		}
		else
		{
			// Drop the lock to avoid deadlock w/KDC when running on a server.
			fMutex.SignalLock();
			if ( AuthenticateUsingCredentials(pReturnLD, (*inOutReplicaInfo), pServerAccount, pServerKerberosID, pServerPassword) == eDSNoErr )
			{
				bDynamicDataNeedsUpdate = true;
			}
			fMutex.WaitLock();
		}

		if ( bDynamicDataNeedsUpdate )
		{
			bDoAgain = UpdateDynamicData( pReturnLD, (*inOutReplicaInfo) );
		}
		
		DSFree( pServerAccount );
		DSFree( pServerKerberosID );
		DSFreePassword( pServerPassword );
		
		if ( bDoAgain ) 
		{
			DSRelease( *inOutReplicaInfo )

			ldap_unbind_ext_s( pReturnLD, NULL, NULL );
			pReturnLD = NULL;

			DbgLog( kLogPlugin, "CLDAPNodeConfig::InternalEstablishConnection - Node %s - dynamic data was changed closing connecting and searching again", fNodeName );
			goto searchAgain;
		}
	}

	fMutex.SignalLock();

	return pReturnLD;
}

tDirStatus CLDAPNodeConfig::AuthenticateUsingCredentials( LDAP *inLDAP, CLDAPReplicaInfo *inReplica, const char *inLDAPUsername,
														  const char *inKerberosID, const char *inPassword )
{
	tDirStatus	siResult = eDSAuthFailed;
	
	// if we don't have an account... we don't care about cleartext
	if ( DSIsStringEmpty(inLDAPUsername) == true && DSIsStringEmpty(inPassword) == true )
		return eDSNoErr;
	
	const char *usePassword = DSIsStringEmpty(inPassword) ? kEmptyPasswordAltStr : inPassword;
	
	CFMutableArrayRef	cfSupportedMethods	= CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
	char				**dn				= ldap_explode_dn( inLDAPUsername, 1 );
	saslDefaults		defaults			= { 0 };

	// if this is a DN then let's use the DN... otherwise just use the inLDAPUsername, cause it isn't a DN
	if ( dn ) {
		defaults.authcid = dn[0];
		defaults.authzid = (char *) calloc( sizeof(char), strlen(inLDAPUsername) + 3 + 1 );
		strcpy( defaults.authzid, "dn:" );
		strcat( defaults.authzid, inLDAPUsername );
	} else {
		defaults.authcid = inLDAPUsername;
		defaults.authzid = (char *) calloc( sizeof(char), strlen(inLDAPUsername) + 2 + 1 );
		strcpy( defaults.authzid, "u:" );
		strcat( defaults.authzid, inLDAPUsername );
	}
	
	defaults.password = usePassword;
	
	fMutex.WaitLock();
	
	CFRange range = CFRangeMake( 0, CFArrayGetCount(fDeniedSASLMethods) );
	if ( CFArrayContainsValue(fDeniedSASLMethods, range, CFSTR("GSSAPI")) == false && 
		 inReplica->SupportsSASLMethod(CFSTR("GSSAPI")) )
	{
		CFArrayAppendValue( cfSupportedMethods, CFSTR("GSSAPI") );
	}

	// we can only do these methods if higher security is not requested
	if ( (IsOnlyBitSet(fSecurityLevel, kSecDisallowCleartext) || (fSecurityLevel & kSecSecurityMask) == 0) )
	{
		if ( CFArrayContainsValue(fDeniedSASLMethods, range, CFSTR("DIGEST-MD5")) == false &&
			 inReplica->SupportsSASLMethod(CFSTR("DIGEST-MD5")) )
		{
			CFArrayAppendValue( cfSupportedMethods, CFSTR("DIGEST-MD5") );
		}
		
		if ( CFArrayContainsValue(fDeniedSASLMethods, range, CFSTR("CRAM-MD5")) == false &&
			 inReplica->SupportsSASLMethod(CFSTR("CRAM-MD5")) )
		{
			CFArrayAppendValue( cfSupportedMethods, CFSTR("CRAM-MD5") );
		}
	}

	fMutex.SignalLock();
		
	// cfSupportedMethods contains only methods that are supported by the node
	CFIndex iCount = CFArrayGetCount( cfSupportedMethods );

	for ( CFIndex ii = 0; ii < iCount && siResult != eDSNoErr; ii++ )
	{
		CFStringRef cfMethod	= (CFStringRef) CFArrayGetValueAtIndex( cfSupportedMethods, ii );
		char		*cStr		= NULL;
		const char	*pMethod	= BaseDirectoryPlugin::GetCStringFromCFString( cfMethod, &cStr );
		
		DbgLog( kLogPlugin, "CLDAPNodeConfig::AuthenticateUsingCredentials - Attempting %s Authentication", pMethod );
		
		// we handle GSSAPI special since it 
		if ( strcmp(pMethod, "GSSAPI") == 0 )
		{
			char		*cacheName	= NULL;
			const char	*username	= defaults.authcid;
			
			if ( DSIsStringEmpty(inKerberosID) == false )
			{
				char *pRealm = strchr( inKerberosID, '@' );
				
				// if we have a realm
				if ( pRealm != NULL && pRealm != inKerberosID && pRealm[1] != '\0' )
					username = inKerberosID;

				// TODO:  how to properly deal with mixed realms for servers
			}
			
			if ( username != NULL ) {
				GetUserTGTIfNecessaryAndStore( username, usePassword, &cacheName );
			}
			
			if ( cacheName != NULL )
				siResult = AuthenticateUsingKerberos( inLDAP, inReplica, cacheName );
			else
				DbgLog( kLogPlugin, "CLDAPNodeConfig::AuthenticateUsingCredentials - Couldn't get Kerberos credentials for GSSAPI <%s>", username );
			
			DSFree( cacheName );
		}
		else
		{
			int ldapError = ldap_sasl_interactive_bind_s( inLDAP, NULL, pMethod, NULL, NULL, LDAP_SASL_QUIET, SASLInteract, &defaults );
			if ( ldapError == LDAP_SUCCESS )
			{
				DbgLog( kLogPlugin, "CLDAPNodeConfig::AuthenticateUsingCredentials - Successful %s Authentication for %s", pMethod, 
					   (defaults.authzid ? defaults.authzid : defaults.authcid) );
				siResult = eDSNoErr;
			}
			else if ( ldapError == LDAP_AUTH_METHOD_NOT_SUPPORTED )
			{
				DbgLog( kLogPlugin, "CLDAPNodeConfig::AuthenticateUsingCredentials - Failed %s Authentication, not supported", pMethod );
			}
			else
			{
				DbgLog( kLogPlugin, "CLDAPNodeConfig::AuthenticateUsingCredentials - Failed %s Authentication for %s error %d", pMethod, 
					   (defaults.authzid ? defaults.authzid : defaults.authcid), ldapError );
			}
		}
		
		DSFree( cStr );
	}
	
	// the auth failed and
	// packet signing and man-in-the-middle is off and
	//		we are using SSL or
	//		we allow clear text passwords
	if ( siResult == eDSAuthFailed && 
		 (fSecurityLevel & (kSecPacketSigning | kSecManInMiddle)) == 0 &&
		 ( fIsSSL == true || (fSecurityLevel & kSecDisallowCleartext) == 0 ) )
	{
		struct berval	cred		= { 0, (char *)"" };
		int				bindMsgId	= 0;
		
		DbgLog( kLogPlugin, "CLDAPNodeConfig::AuthenticateUsingCredentials - Attempting an LDAP bind auth check" );
		
		if ( inPassword != NULL )
		{
			cred.bv_val = (char *) usePassword;
			cred.bv_len = strlen( usePassword );
		}
		
		int ldapError = ldap_sasl_bind( inLDAP, inLDAPUsername, LDAP_SASL_SIMPLE, &cred, NULL, NULL, &bindMsgId );
		if ( ldapError == LDAP_SUCCESS )
		{
			struct timeval	timeout = { fOpenCloseTimeout, 0 };
			LDAPMessage		*result	= NULL;
			
			ldapError = ldap_result( inLDAP, bindMsgId, 0, &timeout, &result );
			switch ( ldapError )
			{
				case -1:
					inReplica->ConnectionFailed();
				case 0:
					siResult = eDSAuthMasterUnreachable;
					break;
				default:
					if ( ldap_result2error(inLDAP, result, 0) == LDAP_SUCCESS )
						siResult = eDSNoErr;
					break;
			}
			
			if ( result != NULL )
			{
				ldap_msgfree( result );
				result = NULL;
			}
		}
		
		DbgLog( kLogPlugin, "CLDAPNodeConfig::AuthenticateUsingCredentials - Cleartext authentication for %s %s", inLDAPUsername, 
			    (siResult == eDSNoErr ? "succceeded" : "failed") );
	}
	
	if ( dn != NULL )
	{
		ldap_value_free( dn );
		dn = NULL;
	}
		
	DSFree( defaults.authzid );
	DSCFRelease( cfSupportedMethods );
	
	return siResult;
}

tDirStatus CLDAPNodeConfig::AuthenticateUsingKerberos( LDAP *inLDAP, CLDAPReplicaInfo *inReplica, const char *inKerberosCache )
{
	tDirStatus		siResult			= eDSAuthFailed;
	saslDefaults	gssapiDefaults		= { 0 };
	krb5_context	krbContext			= NULL;
	krb5_ccache		krbCache			= NULL;
	krb5_error_code	retval				= 0;
	krb5_principal	principal			= NULL;
	char			*principalString	= NULL;
	OM_uint32		minor_status		= 0;
		
	if ( inLDAP == NULL || inReplica == NULL || inKerberosCache == NULL )
		return eParameterError;
	
	if ( inReplica->SupportsSASLMethod(CFSTR("GSSAPI")) == false )
		return eDSAuthMethodNotSupported;
	
	gKerberosMutex->WaitLock();

	do
	{
		retval = krb5_init_context( &krbContext );
		if ( retval ) break;
		
		retval = krb5_cc_resolve( krbContext, inKerberosCache, &krbCache);
		if ( retval ) break;
		
		retval = krb5_cc_get_principal( krbContext, krbCache, &principal );
		if ( retval ) break;
		
		retval = krb5_unparse_name( krbContext, principal, &principalString );
		if ( retval ) break;

		// let's set the GSS cache name for this next part
		gss_krb5_ccache_name( &minor_status, inKerberosCache, NULL );

		// now let's try to do the bind
		int ldapError = ldap_sasl_interactive_bind_s( inLDAP, NULL, "GSSAPI", NULL, NULL, LDAP_SASL_QUIET, SASLInteract, &gssapiDefaults );
		if ( ldapError == LDAP_SUCCESS )
		{
			DbgLog( kLogPlugin, "CLDAPNodeConfig::AuthenticateUsingKerberos - Successful GSSAPI Authentication for %s", principalString );
			siResult = eDSNoErr;
		}
		else if ( ldapError == LDAP_AUTH_METHOD_NOT_SUPPORTED )
		{
			DbgLog( kLogPlugin, "CLDAPNodeConfig::AuthenticateUsingKerberos - Failed GSSAPI Authentication, not supported" );
			siResult = eDSAuthMethodNotSupported;
		}
		else if ( ldapError == LDAP_OTHER || ldapError == LDAP_LOCAL_ERROR )
		{
			DbgLog( kLogPlugin, "CLDAPNodeConfig::AuthenticateUsingKerberos - Failed GSSAPI Authentication for %s", principalString );
		}
	} while ( 0 );
	
	if ( principalString != NULL )
	{
		krb5_free_unparsed_name( krbContext, principalString );
		principalString = NULL;
	}
	
	if ( principal != NULL )
	{
		krb5_free_principal( krbContext, principal );
		principal = NULL;
	}
	
	if ( krbCache != NULL )
	{
		// if the auth failed, let's destroy the cache instead..
		if ( siResult == eDSAuthFailed )
		{
			krb5_cc_destroy( krbContext, krbCache );
			krbCache = NULL;
		}
		else 
		{
			krb5_cc_close( krbContext, krbCache );
			krbCache = NULL;
		}
	}
	
	if ( krbContext != NULL )
	{
		krb5_free_context( krbContext );
		krbContext = NULL;
	}
	
	gKerberosMutex->SignalLock();
	
	return siResult;
}

#pragma mark -
#pragma mark Configuration Related

void CLDAPNodeConfig::DeleteConfiguration( void )
{
	if ( OSAtomicCompareAndSwap32Barrier(false, true, &fConfigDeleted) == true )
	{
		char	nodeName[256];
		
		strlcpy( nodeName, "/LDAPv3/", sizeof(nodeName) );
		strlcat( nodeName, fNodeName, sizeof(nodeName) );
		
		// TODO: need a call to empty just these node entries
	}
}

bool CLDAPNodeConfig::UpdateConfiguraton( CFDictionaryRef inServerConfig, bool inFromServer )
{
	CFStringRef		cfStringRef		= NULL;
	SInt32			prevEnableUse	= fEnableUse;
	
	// we only extract what we want out of the config
	cfStringRef = (CFStringRef) CFDictionaryGetValue( inServerConfig, CFSTR(kXMLLDAPVersionKey) );
	if ( cfStringRef != NULL && CFGetTypeID(cfStringRef) == CFStringGetTypeID() && 
		 CFStringCompare(cfStringRef, CFSTR(kDSLDAPPrefs_CurrentVersion), 0) != kCFCompareEqualTo )
	{
		char *cStr = NULL;
		const char *theVersion = BaseDirectoryPlugin::GetCStringFromCFString( cfStringRef, &cStr );
		
		DbgLog( kLogPlugin, "CLDAPNodeConfig::UpdateConfiguraton - server mappings version too old <%s> is not <%s>", theVersion, 
			    kDSLDAPPrefs_CurrentVersion );
		DSFree( cStr );
		return false;
	}
	
	GetSInt32FromDictionary( inServerConfig, CFSTR(kXMLOpenCloseTimeoutSecsKey), &fOpenCloseTimeout, kLDAPDefaultOpenCloseTimeoutInSeconds );
	
	int32_t	idleMinutes = 0;
	if ( GetSInt32FromDictionary(inServerConfig, CFSTR(kXMLIdleTimeoutMinsKey), &idleMinutes, 1) == true )
	{
		fIdleMaxCount = (idleMinutes * 2);
		OSMemoryBarrier();
	}
	
	GetSInt32FromDictionary( inServerConfig, CFSTR(kXMLDelayedRebindTrySecsKey), &fDelayRebindTry, kLDAPDefaultRebindTryTimeoutInSeconds );
	GetSInt32FromDictionary( inServerConfig, CFSTR(kXMLSearchTimeoutSecsKey), &fSearchTimeout, kLDAPDefaultSearchTimeoutInSeconds );
	GetSInt32FromDictionary( inServerConfig, CFSTR(kXMLUseDNSReplicasFlagKey), &fDNSReplicas, false );
	GetSInt32FromDictionary( inServerConfig, CFSTR(kXMLReferralFlagKey), &fReferrals, false );
	GetSInt32FromDictionary( inServerConfig, CFSTR(kXMLEnableUseFlagKey), &fEnableUse, true );
	
	fMappingsLock.WaitLock();
	
	// now we check the mappings
	DSCFRelease( fAttrTypeMapArray );
	CFArrayRef cfAttrArrayRef = (CFArrayRef) CFDictionaryGetValue( inServerConfig, CFSTR(kXMLAttrTypeMapArrayKey) );
	if ( cfAttrArrayRef != NULL && CFGetTypeID(cfAttrArrayRef) == CFArrayGetTypeID() && CFArrayGetCount(cfAttrArrayRef) > 0 )
		fAttrTypeMapArray = (CFArrayRef) CFRetain( cfAttrArrayRef );
	
	// now we will get the record map array and normalize it to speed access
	DSCFRelease( fRecordTypeMapArray );
	CFArrayRef cfRecordArrayRef = (CFArrayRef) CFDictionaryGetValue( inServerConfig, CFSTR(kXMLRecordTypeMapArrayKey) );
	if ( cfRecordArrayRef != NULL && CFGetTypeID(cfRecordArrayRef) == CFArrayGetTypeID() && CFArrayGetCount(cfRecordArrayRef) > 0 )
		fRecordTypeMapArray = (CFArrayRef) CFRetain( cfRecordArrayRef );

	// if we got a record map and attribute map, lets add them
	DSCFRelease( fNormalizedMappings );
	fNormalizedMappings = CreateNormalizedRecordAttrMap( cfRecordArrayRef, cfAttrArrayRef );
	
	fMappingsLock.SignalLock();

	fMutex.WaitLock();

	GetCStringFromDictionary( inServerConfig, CFSTR(kXMLMapSearchBase), &fMapSearchBase );
	
	// certain things only get updated if this didn't come from the server
	if ( inFromServer == false )
	{
		GetSInt32FromDictionary( inServerConfig, CFSTR(kXMLSecureUseFlagKey), &fSecureUse, false );
		
		GetCStringFromDictionary( inServerConfig, CFSTR(kXMLServerAccountKey), &fServerAccount );
		GetCStringFromDictionary( inServerConfig, CFSTR(kXMLServerPasswordKey), &fServerPassword );
		GetCStringFromDictionary( inServerConfig, CFSTR(kXMLKerberosId), &fServerKerberosID );
		GetCStringFromDictionary( inServerConfig, CFSTR(kXMLServerKey), &fServerName );
		GetCStringFromDictionary( inServerConfig, CFSTR(kXMLUserDefinedNameKey), &fConfigUIName );
		
		// don't let the server override anything that drives SSL.
		GetSInt32FromDictionary( inServerConfig, CFSTR(kXMLPortNumberKey), &fServerPort, LDAP_PORT );
		GetSInt32FromDictionary( inServerConfig, CFSTR(kXMLIsSSLFlagKey), &fIsSSL, false );

		if ( fReachabilityRef != NULL )
		{
			SCNetworkReachabilityUnscheduleFromRunLoop( fReachabilityRef, CFRunLoopGetMain(), kCFRunLoopDefaultMode );
			DSCFRelease( fReachabilityRef );
		}
		
		if ( fServerName != NULL )
		{
			char        buffer[16];
			uint32_t    tempFamily  = AF_UNSPEC;
			
			if ( inet_pton(AF_INET6, fServerName, buffer) == 1 )
				tempFamily = AF_INET6;
			if ( tempFamily == AF_UNSPEC && inet_pton(AF_INET, fServerName, buffer) == 1 )
				tempFamily = AF_INET;
			
			// must be a host name, so we will create a reachability callback
			if ( tempFamily == AF_UNSPEC )
			{
				fReachabilityRef = SCNetworkReachabilityCreateWithName( kCFAllocatorDefault, fServerName );
				if ( fReachabilityRef != NULL )
				{
					SCNetworkReachabilityContext	context	= { 0, this, NULL, NULL, NULL };
					
					// check reachability now because callback is not called if no changes have happened
					// we do this async to ensure we don't block initialization
					dispatch_async( dispatch_get_concurrent_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT),
								    ^(void) {
										SCNetworkConnectionFlags	::flags	= 0;
										
										if ( SCNetworkReachabilityGetFlags(fReachabilityRef, &flags) )
											ReachabilityNotification( flags );
									} );
					
					SCNetworkReachabilitySetCallback( fReachabilityRef, ReachabilityCallback, &context );
					SCNetworkReachabilityScheduleWithRunLoop( fReachabilityRef, CFRunLoopGetMain(), kCFRunLoopDefaultMode );
				}
			}
			else
			{
				// must be an IP address so we just set reachability to true
				OSAtomicCompareAndSwap32Barrier( false, true, &fAvailable );
			}
		}
		
		// if we don't have an account and password, turn off the flag
		if ( fServerAccount == NULL && fServerPassword == NULL )
			OSAtomicCompareAndSwap32Barrier( true, false, &fSecureUse );
		
		CFDictionaryRef	cfLocalPolicy = (CFDictionaryRef) CFDictionaryGetValue( inServerConfig, CFSTR(kXMLLocalSecurityKey) );
		if ( cfLocalPolicy != NULL && CFGetTypeID(cfLocalPolicy) == CFDictionaryGetTypeID() )
		{
			fLocalSecurityLevel = CalculateSecurityPolicy( cfLocalPolicy );
			fSecurityLevel |= fLocalSecurityLevel;	// we set our local policy bits right off the bat
			OSMemoryBarrier();
		}
		
		GetSInt32FromDictionary( inServerConfig, CFSTR(kXMLServerMappingsFlagKey), &fServerMappings, false );
		
		// if not server mappings based, let's switch getting the mappings
		if ( fServerMappings == false )
			OSAtomicCompareAndSwap32Barrier( true, false, &fGetServerMappings );
		
		DSCFRelease( fReadReplicas );
		DSCFRelease( fWriteReplicas );
		DSCFRelease( fDeniedSASLMethods );
		
		fReadReplicas = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
		fWriteReplicas = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
		fDeniedSASLMethods = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

		CFStringRef cfServerName = CFStringCreateWithCString( kCFAllocatorDefault, fServerName, kCFStringEncodingUTF8 );
		CFArrayAppendValue( fReadReplicas, cfServerName ); // this is our preferred so it's always listed first
		DSCFRelease( cfServerName );
		
		CFArrayRef replicas = (CFArrayRef) CFDictionaryGetValue( inServerConfig, CFSTR(kXMLReplicaHostnameListArrayKey) );
		if ( replicas != NULL )
			CFArrayAppendArray( fReadReplicas, replicas, CFRangeMake(0, CFArrayGetCount(replicas)) );
		
		replicas = (CFArrayRef) CFDictionaryGetValue( inServerConfig, CFSTR(kXMLWriteableHostnameListArrayKey) );
		if ( replicas != NULL )
			CFArrayAppendArray( fWriteReplicas, replicas, CFRangeMake(0, CFArrayGetCount(replicas)) );
		else
			CFArrayAppendArray( fWriteReplicas, fReadReplicas, CFRangeMake(0, CFArrayGetCount(fReadReplicas)) );
		
		CFArrayRef denied = (CFArrayRef) CFDictionaryGetValue( inServerConfig, CFSTR(kXMLDeniedSASLMethods) );
		if ( denied != NULL && CFArrayGetTypeID() == CFGetTypeID(denied) )
			CFArrayAppendArray( fDeniedSASLMethods, denied, CFRangeMake(0, CFArrayGetCount(denied)) );
	}
	
	fMutex.SignalLock();
	
	if ( gCacheNode != NULL && prevEnableUse != fEnableUse )
		gCacheNode->EmptyCacheEntryType( CACHE_ENTRY_TYPE_ALL );
	
	return true;
}

CFDictionaryRef	CLDAPNodeConfig::GetConfiguration( void )
{
	CFMutableDictionaryRef	curConfigDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFCopyStringDictionaryKeyCallBacks, 
																	   &kCFTypeDictionaryValueCallBacks );
	CFStringRef				cfStringRef;
	CFNumberRef				cfNumberRef;
	
	fMutex.WaitLock();
	
	cfNumberRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberSInt32Type, &fServerPort );
	if ( cfNumberRef != NULL )
	{
		CFDictionaryAddValue( curConfigDict, CFSTR(kXMLPortNumberKey), cfNumberRef );
		CFRelease( cfNumberRef );
	}
	
	if ( fConfigUIName != NULL )
	{
		cfStringRef = CFStringCreateWithCString( kCFAllocatorDefault, fConfigUIName, kCFStringEncodingUTF8 );
		if ( cfStringRef != NULL )
		{
			CFDictionaryAddValue( curConfigDict, CFSTR(kXMLUserDefinedNameKey), cfStringRef );
			CFRelease( cfStringRef );
		}
	}

	if ( fServerName != NULL )
	{
		cfStringRef = CFStringCreateWithCString( kCFAllocatorDefault, fServerName, kCFStringEncodingUTF8 );
		if ( cfStringRef != NULL )
		{
			CFDictionaryAddValue( curConfigDict, CFSTR(kXMLServerKey), cfStringRef );
			CFRelease( cfStringRef );
		}
	}
	
	cfNumberRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberSInt32Type, &fOpenCloseTimeout );
	if ( cfNumberRef != NULL )
	{
		CFDictionaryAddValue( curConfigDict, CFSTR(kXMLOpenCloseTimeoutSecsKey), cfNumberRef );
		CFRelease( cfNumberRef );
	}
	
	cfNumberRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberSInt32Type, &fSearchTimeout );
	if ( cfNumberRef != NULL )
	{
		CFDictionaryAddValue( curConfigDict, CFSTR(kXMLSearchTimeoutSecsKey), cfNumberRef );
		CFRelease( cfNumberRef );
	}
	
	if ( fReadReplicas != NULL)
		CFDictionaryAddValue( curConfigDict, CFSTR(kXMLReplicaHostnameListArrayKey), fReadReplicas );
	
	if ( fWriteReplicas != NULL)
		CFDictionaryAddValue( curConfigDict, CFSTR(kXMLWriteableHostnameListArrayKey), fWriteReplicas );
	
	CFDictionaryAddValue( curConfigDict, CFSTR(kXMLIsSSLFlagKey), fIsSSL ? kCFBooleanTrue : kCFBooleanFalse );
	CFDictionaryAddValue( curConfigDict, CFSTR(kXMLMakeDefLDAPFlagKey), fDHCPLDAPServer ? kCFBooleanTrue : kCFBooleanFalse );
	CFDictionaryAddValue( curConfigDict, CFSTR(kXMLEnableUseFlagKey), fEnableUse ? kCFBooleanTrue : kCFBooleanFalse );
	CFDictionaryAddValue( curConfigDict, CFSTR(kXMLServerMappingsFlagKey), fServerMappings ? kCFBooleanTrue : kCFBooleanFalse );
	CFDictionaryAddValue( curConfigDict, CFSTR(kXMLReferralFlagKey), fReferrals ? kCFBooleanTrue : kCFBooleanFalse );

	if ( fDeniedSASLMethods != NULL )
		CFDictionaryAddValue( curConfigDict, CFSTR(kXMLDeniedSASLMethods), fDeniedSASLMethods );
	
	fMutex.SignalLock();
	
	fMappingsLock.WaitLock();
	
	if ( fRecordTypeMapArray != NULL )
		CFDictionaryAddValue( curConfigDict, CFSTR(kXMLRecordTypeMapArrayKey), fRecordTypeMapArray );
	
	if ( fAttrTypeMapArray != NULL )
		CFDictionaryAddValue( curConfigDict, CFSTR(kXMLAttrTypeMapArrayKey), fAttrTypeMapArray );
	
	fMappingsLock.SignalLock();
	
	return curConfigDict;
}

bool CLDAPNodeConfig::RetrieveServerMappings( LDAP *inLDAP, CLDAPReplicaInfo *inReplica )
{
	CFArrayRef		cfSearchBases		= NULL;
	bool			bReturn				= false;
	
	//here we check if there is a provided mappings search base
	//otherwise we will attempt to retrieve possible candidates from the namingContexts of the LDAP server
	if ( DSIsStringEmpty(fMapSearchBase) )
	{
		cfSearchBases = inReplica->CopyNamingContexts();

		// flag it as failed and return false, we can't do anything without a naming context
		// this should hopefully never happen
		if ( cfSearchBases == NULL )
		{
			inReplica->ConnectionFailed();
			return false;
		}
	}
	else
	{
		CFStringRef	cfMapSearchBase = CFStringCreateWithCString( kCFAllocatorDefault, fMapSearchBase, kCFStringEncodingUTF8 );
		if ( cfMapSearchBase == NULL )
			return false;

		cfSearchBases = CFArrayCreate( kCFAllocatorDefault, (CFTypeRef *)&cfMapSearchBase, 1, &kCFTypeArrayCallBacks );
		DSCFRelease( cfMapSearchBase );
	}
	
	CFIndex		iCount		= CFArrayGetCount( cfSearchBases );
	char		*cStr		= NULL;
	CFDataRef	ourMappings	= NULL;
	
	for ( CFIndex ii = 0; ii < iCount && ourMappings == NULL; ii++ )
	{
		CFStringRef		cfSearchBase	= (CFStringRef) CFArrayGetValueAtIndex( cfSearchBases, ii );
		const char		*searchBase		= BaseDirectoryPlugin::GetCStringFromCFString( cfSearchBase, &cStr );
		struct timeval	timeout			= { kLDAPDefaultOpenCloseTimeoutInSeconds, 0 };	// use open/close since we are opening a session
		const char		*attrs[]		= { "description", NULL };
		LDAPMessage		*result			= NULL;

		int returnCode = ldap_search_ext_s( inLDAP, searchBase, LDAP_SCOPE_SUBTREE, "(&(objectclass=organizationalUnit)(ou=macosxodconfig))", 
										    (char **)attrs, FALSE, NULL, NULL, &timeout, 0, &result );
		if ( returnCode == LDAP_SUCCESS )
		{
			struct berval **bValues = ldap_get_values_len( inLDAP, result, "description" );
			if ( bValues != NULL )
			{					
				// should be only one value of the attribute
				if ( bValues[0] != NULL )
				{
					ourMappings = CFDataCreate( kCFAllocatorDefault, (UInt8 *)(bValues[0]->bv_val), bValues[0]->bv_len );
					DbgLog( kLogPlugin, "CLDAPNodeConfig::RetrieveServerMappings - Node %s - retrieved server mappings from %s - searchbase <%s>.", 
						    fNodeName, inReplica->fIPAddress, searchBase );
				}
				
				ldap_value_free_len( bValues );
			}
		}
		else if ( returnCode == LDAP_TIMEOUT || returnCode == LDAP_TIMELIMIT_EXCEEDED )
		{
			DbgLog( kLogPlugin, "CLDAPNodeConfig::RetrieveServerMappings - Node %s - timed out retrieving server mappings from %s.", 
				    fNodeName, inReplica->fIPAddress );
		}
		
		if ( result != NULL )
		{
			ldap_msgfree( result );
			result = NULL;
		}

		DSFree( cStr );
	}
	
	if ( ourMappings != NULL )
	{
		CFDictionaryRef serverConfigDict = (CFDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault,
																							  ourMappings,
																							  kCFPropertyListImmutable,
																							  NULL );
		if ( serverConfigDict != NULL && 
			 CFGetTypeID(serverConfigDict) == CFDictionaryGetTypeID() )
		{
			UpdateConfiguraton( serverConfigDict, true );
			
			if ( fConfigObject != NULL )
			{
				fMappingsLock.WaitLock();
				fConfigObject->UpdateServerMappingsForUUID( fNodeName, fConfigUUID, fAttrTypeMapArray, fRecordTypeMapArray );
				fMappingsLock.SignalLock();
			}
			
			bReturn = true;
		}
		
		DSCFRelease( serverConfigDict );
	}

	DSCFRelease( ourMappings );
	DSFree( cStr );
	DSCFRelease( cfSearchBases );

	// we attempted mappings, not trying again
	OSAtomicCompareAndSwap32Barrier( true, false, &fGetServerMappings );
	
	return bReturn;
}

bool CLDAPNodeConfig::RetrieveServerReplicaList( LDAP *inLDAP, CFMutableArrayRef outRepList, CFMutableArrayRef outWriteableList )
{
	struct berval	  **bValues			= NULL;
	char			   *nativeRecType	= NULL;
	bool				bOCANDGroup		= false;
	CFArrayRef			OCSearchList	= NULL;
	ber_int_t			scope			= LDAP_SCOPE_BASE;
	char			   *queryFilter		= NULL;
	char			   *repListAttr		= NULL;
	char			   *writeListAttr	= NULL;

	//search for the specific LDAP config record which may contain
	//the list of both read and write LDAP replica urls
	
	nativeRecType = MapRecToSearchBase( kDSStdRecordTypeConfig, 1, &bOCANDGroup, &OCSearchList, &scope );
	if ( nativeRecType == NULL )
	{
        DbgLog( kLogPlugin, "CLDAPNodeConfig::RetrieveServerReplicaList - No Config Record mapping to retrieve replica list." );
		OSAtomicCompareAndSwap32Barrier( true, false, &fGetReplicas );
		return false;
	}
	
	queryFilter = BuildLDAPQueryFilter(	(char *)kDSNAttrRecordName, (char *)"ldapreplicas", eDSExact, false, (char *)kDSStdRecordTypeConfig, nativeRecType, 
									    bOCANDGroup, OCSearchList );
	
	if ( queryFilter != NULL )
	{
		LDAPMessage		*result	= NULL;
		struct timeval	timeout = { fOpenCloseTimeout, 0 };

		// we do this synchronously
		int ldapReturnCode = ldap_search_ext_s(	inLDAP, nativeRecType, scope, queryFilter, NULL, 0, NULL, NULL, &timeout, 0, &result );
		if ( ldapReturnCode == LDAP_SUCCESS )
		{
			// get the replica list here
			// parse the attributes in the result - should only be two
			repListAttr = MapAttrToLDAPType( kDSStdRecordTypeConfig, kDSNAttrLDAPReadReplicas, 1 );
			if ( repListAttr != NULL && (bValues = ldap_get_values_len(inLDAP, result, repListAttr)) != NULL )
			{
				for ( int ii = 0; bValues[ii] != NULL; ii++ )
				{
					LDAPURLDesc	*ludpp	= NULL;
					
					if ( ldap_url_parse(bValues[ii]->bv_val, &ludpp) == LDAP_SUCCESS )
					{
						CFStringRef	cfServer = CFStringCreateWithCString( kCFAllocatorDefault, ludpp->lud_host, kCFStringEncodingUTF8 );
						if ( cfServer != NULL )
						{
							if ( CFArrayContainsValue(outRepList, CFRangeMake(0,CFArrayGetCount(outRepList)), cfServer ) == false)
								 CFArrayAppendValue( outRepList, cfServer );
							
							DSCFRelease( cfServer );
						}
						
						ldap_free_urldesc( ludpp );
						ludpp = NULL;
					}
				}
				
				ldap_value_free_len(bValues);
			}
			
			writeListAttr = MapAttrToLDAPType( kDSStdRecordTypeConfig, kDSNAttrLDAPWriteReplicas, 1 );
			if ( writeListAttr != NULL && (bValues = ldap_get_values_len(inLDAP, result, writeListAttr)) != NULL )
			{
				for ( int ii = 0; bValues[ii] != NULL; ii++ )
				{
					LDAPURLDesc	*ludpp	= NULL;
					
					if ( ldap_url_parse(bValues[ii]->bv_val, &ludpp) == LDAP_SUCCESS )
					{
						CFStringRef	cfServer = CFStringCreateWithCString( kCFAllocatorDefault, ludpp->lud_host, kCFStringEncodingUTF8 );
						if ( cfServer != NULL )
						{
							if ( CFArrayContainsValue(outWriteableList, CFRangeMake(0,CFArrayGetCount(outWriteableList)), cfServer ) == false)
								 CFArrayAppendValue( outWriteableList, cfServer );
							
							DSCFRelease( cfServer );
						}
						
						ldap_free_urldesc( ludpp );
						ludpp = NULL;
					}
				}
				
				ldap_value_free_len(bValues);
			}
		}
		
		if ( result != NULL )
		{
			ldap_msgfree( result );
			result = NULL;
		}
	}

	DSCFRelease( OCSearchList );
	DSFree( nativeRecType );
	DSFree( queryFilter );
	DSFree( repListAttr );
	DSFree( writeListAttr );
    
    if ( CFArrayGetCount(outRepList) > 0 )
	{
        DbgLog( kLogPlugin, "CLDAPNodeConfig::RetrieveServerReplicaList - Node %s - Found Config Record \"ldapreplicas\" that had Replica information.",
			    fNodeName );

		if ( fConfigObject != NULL )
			fConfigObject->UpdateReplicaListForUUID( fNodeName, fConfigUUID, outRepList, outWriteableList );
	}
    else
	{
        DbgLog( kLogPlugin, "CLDAPNodeConfig::RetrieveServerReplicaList - Node %s - No Config Record \"ldapreplicas\" with Replica information.",
			    fNodeName );
	}
	
	OSAtomicCompareAndSwap32Barrier( true, false, &fGetReplicas );
	
	return (CFArrayGetCount(outRepList) > 0);
}

bool CLDAPNodeConfig::RetrieveServerSecuritySettings( LDAP *inLDAP, CLDAPReplicaInfo *inReplica )
{
	LDAPMessage			*pLDAPResult			= NULL;
	timeval				stTimeout				= { fOpenCloseTimeout, 0 };
	bool				bChangedPolicy			= false;
	CFDictionaryRef		cfConfiguredSecPolicy	= NULL;
	
	// let's get the first attribute map for this one
	char *pAttribute = ExtractAttrMap( kDSStdRecordTypeConfig, kDS1AttrXMLPlist, 1 );	
	if ( pAttribute != NULL )
	{
		// we're just going to search for this object, we don't care what type it is at this point.. TBD??
		int ldapReturnCode = ldap_search_ext_s( inLDAP, fMapSearchBase, LDAP_SCOPE_SUBTREE, "(cn=macosxodpolicy)", NULL, false, 
											    NULL, NULL, &stTimeout, 0, &pLDAPResult );
		
		if ( ldapReturnCode == LDAP_SUCCESS )
		{
			berval  **pBValues  = NULL;
			
			if ( (pBValues = ldap_get_values_len(inLDAP, pLDAPResult, pAttribute)) != NULL )
			{
				// good, we have a security settings config record...  Let's use the config we just got from the server
				CFDataRef cfXMLData = CFDataCreate( kCFAllocatorDefault, (UInt8 *)(pBValues[0]->bv_val), pBValues[0]->bv_len );
				
				if ( cfXMLData != NULL )
				{
					// extract the config dictionary from the XML data and this is our new configuration
					CFMutableDictionaryRef cfTempDict;
					
					cfTempDict = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, cfXMLData, 
																						   kCFPropertyListMutableContainersAndLeaves, NULL );
					
					// make sure we got a dictionary and it has something in it...
					if ( cfTempDict )
					{
						// set the new security level if they are different
						CFDictionaryRef cfConfigPolicy = (CFDictionaryRef) CFDictionaryGetValue( cfTempDict, CFSTR(kXMLConfiguredSecurityKey) );
						int32_t			iSecurityLevel = fLocalSecurityLevel | CalculateSecurityPolicy( cfConfigPolicy );
						
						if ( fSecurityLevel != iSecurityLevel )
						{
							fSecurityLevel = iSecurityLevel;
							OSMemoryBarrier();
							bChangedPolicy = true;
						}

						cfConfiguredSecPolicy = CFDictionaryCreateCopy( kCFAllocatorDefault, cfConfigPolicy );

						DSCFRelease( cfTempDict );
					}
					
					DSCFRelease( cfXMLData ); // let's release it, we're done with it
				}
				
				ldap_value_free_len( pBValues );
				pBValues = NULL;
			}
		}
		
		if ( pLDAPResult )
		{
			ldap_msgfree( pLDAPResult );
			pLDAPResult = NULL;
		}
									
		DSFree( pAttribute );
	}
	
	// let's update the supported security settings from the SupportedSASLMethods in the Config
	CFArrayRef	cfSASLMethods = inReplica->CopySASLMethods();
	if ( cfSASLMethods != NULL && CFArrayGetCount(cfSASLMethods) > 0 )
	{
		// Start the dictionary for the Security Levels supported
		CFMutableDictionaryRef cfSupportedSecLevel = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, 
																			    &kCFTypeDictionaryValueCallBacks );
		
		// if we have SSL, we have some capabilities already..
		if ( fIsSSL )
		{
			CFDictionarySetValue( cfSupportedSecLevel, CFSTR(kXMLSecurityNoClearTextAuths), kCFBooleanTrue );
			CFDictionarySetValue( cfSupportedSecLevel, CFSTR(kXMLSecurityPacketEncryption), kCFBooleanTrue );
		}
		
		CFRange		stRange = CFRangeMake( 0, CFArrayGetCount(cfSASLMethods) );
		
		// we need to verify with CLDAPNode what types are supported..... TBD
		if ( CFArrayContainsValue( cfSASLMethods, stRange, CFSTR("CRAM-MD5")) )
			CFDictionarySetValue( cfSupportedSecLevel, CFSTR(kXMLSecurityNoClearTextAuths), kCFBooleanTrue );
		
		if ( CFArrayContainsValue( cfSASLMethods, stRange, CFSTR("GSSAPI")) )
		{
			CFDictionarySetValue( cfSupportedSecLevel, CFSTR(kXMLSecurityNoClearTextAuths), kCFBooleanTrue );
			CFDictionarySetValue( cfSupportedSecLevel, CFSTR(kXMLSecurityManInTheMiddle), kCFBooleanTrue );
			CFDictionarySetValue( cfSupportedSecLevel, CFSTR(kXMLSecurityPacketSigning), kCFBooleanTrue );
			CFDictionarySetValue( cfSupportedSecLevel, CFSTR(kXMLSecurityPacketEncryption), kCFBooleanTrue );
		}

		if ( fConfigObject != NULL )
			fConfigObject->UpdateSecurityPolicyForUUID( fNodeName, fConfigUUID, cfConfiguredSecPolicy, cfSupportedSecLevel );
		
		DSCFRelease( cfSupportedSecLevel );
		DSCFRelease( cfConfiguredSecPolicy );
	}

	DSCFRelease( cfSASLMethods );

	// let's reset the flag so we don't keep checking
	OSAtomicCompareAndSwap32Barrier( true, false, &fGetSecuritySettings );
	
	return bChangedPolicy;
}

void CLDAPNodeConfig::RetrieveServerSchema( LDAP *inLDAP )
{
	sObjectClassSchema	*aOCSchema		= NULL;
	bool				bSkipToTag		= true;
	char				*lineEntry		= NULL;
	char				*strtokContext	= NULL;
	LDAPMessage			*result			= NULL;
	int					ldapReturnCode	= 0;
	const char			*sattrs[]		= { "subschemasubentry", NULL };
	const char			*attrs[]		= { "objectclasses", NULL };
	char				*subschemaDN	= NULL;
	struct berval		**bValues		= NULL;
	struct timeval		timeout			= { fSearchTimeout, 0 };
	
	fMutex.WaitLock();
	
	if ( fObjectClassSchema != NULL ) {
		fMutex.SignalLock();
		return;
	}

	//at this point we can make the call to the LDAP server to determine the object class schema
	//then after building the ObjectClassMap we can assign it to the fObjectClassSchema
	ldapReturnCode = ldap_search_ext_s( inLDAP, "", LDAP_SCOPE_BASE, "(objectclass=*)", (char**)sattrs, 0, NULL, NULL, &timeout, 0, &result );
	if ( ldapReturnCode == LDAP_SUCCESS )
	{
		bValues = ldap_get_values_len( inLDAP, result, "subschemasubentry" );
		if ( bValues != NULL )
		{					
			// should be only one value of the attribute
			if ( bValues[0] != NULL )
				subschemaDN = strdup( bValues[0]->bv_val );
			
			ldap_value_free_len( bValues );
		}
	}
	
	if ( result != NULL )
	{
		ldap_msgfree( result );
		result = NULL;
	}
	
	if ( subschemaDN != NULL )
	{
		ldapReturnCode = ldap_search_ext_s( inLDAP, subschemaDN, LDAP_SCOPE_BASE, "(objectclass=subSchema)", (char**)attrs, 0, NULL, NULL, 
											&timeout, 0, &result );
		if ( ldapReturnCode == LDAP_SUCCESS )
		{
			bValues = ldap_get_values_len( inLDAP, result, "objectclasses" );
			if ( bValues != NULL )
			{
				ObjectClassMap *aOCClassMap = new ObjectClassMap;
				
				// for each value of the attribute we need to parse and add as an entry to the objectclass schema map
				for ( int i = 0; bValues[i] != NULL; i++ )
				{
					aOCSchema = NULL;
					DSFree( lineEntry );
					
					//here we actually parse the values
					lineEntry = strdup( bValues[i]->bv_val );
					
					//find the objectclass name
					char * aToken = strtok_r( lineEntry, OCSEPCHARS, &strtokContext );
					while ( aToken != NULL && strcmp(aToken,"NAME") != 0 )
					{
						aToken = strtok_r( NULL, OCSEPCHARS, &strtokContext );
					}
					
					if ( aToken != NULL )
					{
						aToken = strtok_r( NULL, OCSEPCHARS, &strtokContext );
						if (aToken != NULL)
						{
							//now use the NAME to create an entry
							//first check if that NAME is already present - unlikely
							if ( aOCClassMap->find( aToken ) == aOCClassMap->end() )
							{
								aOCSchema = new sObjectClassSchema;
								(*aOCClassMap)[aToken] = aOCSchema;
							}
						}
					}
					
					if (aOCSchema == NULL || aToken == NULL)
						continue;
					
					//here we have the NAME - at least one of them
					//now check if there are any more NAME values
					bSkipToTag = true;
					while ( bSkipToTag )
					{
						aToken = strtok_r( NULL, OCSEPCHARS, &strtokContext);
						if ( aToken == NULL )
							break;

						bSkipToTag = IsTokenNotATag( aToken );
						if ( bSkipToTag )
							aOCSchema->fOtherNames.insert( aOCSchema->fOtherNames.begin(), aToken );
					}
					
					if ( aToken == NULL )
						continue;
					
					if ( strcmp(aToken, "DESC") == 0 )
					{
						bSkipToTag = true;
						while ( bSkipToTag )
						{
							aToken = strtok_r( NULL, OCSEPCHARS, &strtokContext );
							if ( aToken == NULL )
								break;
							bSkipToTag = IsTokenNotATag( aToken );
						}
						if ( aToken == NULL )
							continue;
					}
					
					if (strcmp(aToken, "OBSOLETE") == 0)
					{
						bSkipToTag = true;
						while ( bSkipToTag )
						{
							aToken = strtok_r( NULL, OCSEPCHARS, &strtokContext );
							if ( aToken == NULL )
								break;
							bSkipToTag = IsTokenNotATag( aToken );
						}
						if ( aToken == NULL )
							continue;
					}
					
					if (strcmp(aToken, "SUP") == 0)
					{
						aToken = strtok_r( NULL, OCSEPCHARS, &strtokContext );
						if ( aToken == NULL )
							continue;

						aOCSchema->fParentOCs.insert( aOCSchema->fParentOCs.begin(), aToken );
						//get the other SUP entries
						bSkipToTag = true;
						while ( bSkipToTag )
						{
							aToken = strtok_r( NULL, OCSEPCHARS, &strtokContext );
							if ( aToken == NULL )
								break;

							bSkipToTag = IsTokenNotATag( aToken );
							if ( bSkipToTag )
								aOCSchema->fParentOCs.insert(aOCSchema->fParentOCs.begin(),aToken);
						}
						
						if ( aToken == NULL )
							continue;
					}
					
					if ( strcmp(aToken, "ABSTRACT") == 0 )
					{
						aOCSchema->fType = 0;
						aToken = strtok_r( NULL, OCSEPCHARS, &strtokContext );
						if ( aToken == NULL )
							continue;
					}
					
					if ( strcmp(aToken, "STRUCTURAL") == 0 )
					{
						aOCSchema->fType = 1;
						aToken = strtok_r( NULL, OCSEPCHARS, &strtokContext );
						if ( aToken == NULL )
							continue;
					}
					
					if ( strcmp(aToken, "AUXILIARY") == 0 )
					{
						aOCSchema->fType = 2;
						aToken = strtok_r( NULL, OCSEPCHARS, &strtokContext );
						if ( aToken == NULL )
							continue;
					}
					
					if ( strcmp(aToken, "MUST") == 0 )
					{
						aToken = strtok_r( NULL, OCSEPCHARS, &strtokContext );
						if ( aToken == NULL )
							continue;
						
						aOCSchema->fRequiredAttrs.insert( aOCSchema->fRequiredAttrs.begin(), aToken );
						
						//get the other MUST entries
						bSkipToTag = true;
						while ( bSkipToTag )
						{
							aToken = strtok_r( NULL, OCSEPCHARS, &strtokContext );
							if ( aToken == NULL )
								break;

							bSkipToTag = IsTokenNotATag( aToken );
							if ( bSkipToTag )
								aOCSchema->fRequiredAttrs.insert( aOCSchema->fRequiredAttrs.begin(), aToken );
						}
						
						if ( aToken == NULL )
							continue;
					}
					
					if ( strcmp(aToken, "MAY") == 0 )
					{
						aToken = strtok_r( NULL, OCSEPCHARS, &strtokContext );
						if ( aToken == NULL )
							continue;

						aOCSchema->fAllowedAttrs.insert( aOCSchema->fAllowedAttrs.begin(), aToken );
						//get the other MAY entries
						bSkipToTag = true;
						while ( bSkipToTag )
						{
							aToken = strtok_r( NULL, OCSEPCHARS, &strtokContext );
							if ( aToken == NULL )
								break;

							bSkipToTag = IsTokenNotATag( aToken );
							if ( bSkipToTag )
								aOCSchema->fAllowedAttrs.insert( aOCSchema->fAllowedAttrs.begin(), aToken );
						}
						
						if ( aToken == NULL )
							continue;
					}
				} // for each bValues[i]

				fObjectClassSchema = aOCClassMap;

				ldap_value_free_len( bValues );
				bValues = NULL;

				DSFree( lineEntry );
			}
		}
		else
		{
			DbgLog( kLogError, "Failed to retrieve LDAP server schema - LDAP error %d", ldapReturnCode );
		}
		
		if ( result != NULL )
		{
			ldap_msgfree( result );
			result = NULL;
		}
	}
	
	fMutex.SignalLock();

	DSFree( subschemaDN );
}

#pragma mark -
#pragma mark Support Routines

bool CLDAPNodeConfig::IsTokenNotATag( char *inToken )
{
	if ( inToken == NULL )
		return true;
	
	//check for first char in inToken as an uppercase letter in the following set
	//"NDOSAMX" since that will cover the following tags
	//NAME,DESC,OBSOLETE,SUP,ABSTRACT,STRUCTURAL,AUXILIARY,MUST,MAY,X-ORIGIN
	
	switch( *inToken )
	{
		case 'N':
		case 'D':
		case 'O':
		case 'S':
		case 'A':
		case 'M':
		case 'X':
			
			if (strcmp(inToken,"DESC") == 0)
				return false;
			
			if (strcmp(inToken,"SUP") == 0)
				return false;
			
			if (strlen(inToken) > 7)
			{
				if (strcmp(inToken,"OBSOLETE") == 0)
					return false;
				
				if (strcmp(inToken,"ABSTRACT") == 0)
					return false;
				
				if (strcmp(inToken,"STRUCTURAL") == 0)
					return false;
				
				if (strcmp(inToken,"AUXILIARY") == 0)
					return false;
				
				if (strcmp(inToken,"X-ORIGIN") == 0) //appears that iPlanet uses a non-standard tag ie. post RFC 2252
					return false;
			}
			
			if (strcmp(inToken,"MUST") == 0)
				return false;
			
			if (strcmp(inToken,"MAY") == 0)
				return false;
			
			if (strcmp(inToken,"NAME") == 0)
				return false;
			break;
		default:
			break;
	}
	
	return true;
}

bool CLDAPNodeConfig::GetSInt32FromDictionary( CFDictionaryRef inDictionary, CFStringRef inKey, int32_t *outValue, int32_t defaultValue )
{
	CFTypeRef	cfValue = CFDictionaryGetValue( inDictionary, inKey );
	
	if ( cfValue != NULL )
	{
		if ( CFGetTypeID(cfValue) == CFBooleanGetTypeID() )
		{
			(*outValue) = CFBooleanGetValue( (CFBooleanRef) cfValue );
			OSMemoryBarrier();
			return true;
		}
		else if ( CFGetTypeID(cfValue) == CFNumberGetTypeID() && 
				 CFNumberGetValue((CFNumberRef) cfValue, kCFNumberSInt32Type, outValue) == true )
		{
			OSMemoryBarrier();
			return true;
		}
	}

	(*outValue) = defaultValue;
	OSMemoryBarrier();

	return false;
}

bool CLDAPNodeConfig::GetCStringFromDictionary( CFDictionaryRef inDictionary, CFStringRef inKey, char **outValue )
{
	bool		bReturn	= false;
	CFStringRef	cfValue = (CFStringRef) CFDictionaryGetValue( inDictionary, inKey );
	
	if ( cfValue != NULL )
	{
		if ( CFGetTypeID(cfValue) == CFStringGetTypeID() )
		{
			char		*cStr		= NULL;
			const char	*tempValue	= BaseDirectoryPlugin::GetCStringFromCFString( cfValue, &cStr );
			
			if ( tempValue != NULL )
			{
				DSFree( (*outValue) );
				(*outValue) = strdup( tempValue );
				bReturn = true;
			}
			
			DSFree( cStr );
		}
		else if ( CFGetTypeID(cfValue) == CFDataGetTypeID() )
		{
			DSFree( (*outValue) );

			CFIndex tempLen = CFDataGetLength( (CFDataRef) cfValue );
			(*outValue) = (char *) calloc( tempLen + 1, sizeof(char) );
			CFDataGetBytes( (CFDataRef) cfValue, CFRangeMake(0, tempLen), (UInt8*) (*outValue) );
		}
	}
	
	return bReturn;
}

CFDictionaryRef CLDAPNodeConfig::CreateNormalizedAttributeMap( CFArrayRef inAttrMapArray, CFDictionaryRef inGlobalAttrMap )
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
				if ( iNativeMapCount == 0 )
					continue;
				
				// now let's loop through the current native maps
				CFMutableArrayRef	cfNewNativeMap = CFArrayCreateMutable( kCFAllocatorDefault, iNativeMapCount, &kCFTypeArrayCallBacks );
				for( CFIndex iNativeIndex = 0; iNativeIndex < iNativeMapCount; iNativeIndex++ )
				{
					CFStringRef cfStringRef = (CFStringRef) CFArrayGetValueAtIndex( cfNativeArray, iNativeIndex );
					
					// if it is a valid string like we expect, let's add it to the new list..
					if ( cfStringRef != NULL && CFGetTypeID(cfStringRef) == CFStringGetTypeID() )
					{
						CFArrayAppendValue( cfNewNativeMap, cfStringRef );
					}
				}

				// only add this if there is some values in the list it is key->array pair
				if ( CFArrayGetCount(cfNewNativeMap) != 0 )
				{
					CFDictionarySetValue( newAttrMapDict, cfStdName, cfNewNativeMap );
				}
				
				DSCFRelease( cfNewNativeMap );
			}
		}
	}
	
	// if we have a dictionary, but it is empty, let's release and NULL
	if ( newAttrMapDict != NULL && CFDictionaryGetCount(newAttrMapDict) == 0 )
		DSCFRelease( newAttrMapDict );
	
	return newAttrMapDict;
}

CFDictionaryRef CLDAPNodeConfig::CreateNormalizedRecordAttrMap( CFArrayRef inRecMapArray, CFArrayRef inGlobalAttrMapArray )
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
				if ( iNativeMapCount == 0 )
					continue;
				
				// create a new array with the maximum entries of the current array, since we shouldn't exceed
				CFMutableArrayRef cfNewNativeArray = CFArrayCreateMutable( kCFAllocatorDefault, iNativeMapCount, &kCFTypeArrayCallBacks );
				
				for( CFIndex iNativeIndex = 0; iNativeIndex < iNativeMapCount; iNativeIndex++ )
				{
					// we only have one map per record type
					CFMutableDictionaryRef	cfDictRef			= (CFMutableDictionaryRef) CFArrayGetValueAtIndex( cfNativeArray, iNativeIndex );
					CFMutableDictionaryRef	cfValidNativeDict	= NULL;
					
					// technically we can't have NULL's but just in case..
					if ( CFGetTypeID(cfDictRef) == CFDictionaryGetTypeID() )
					{
						CFArrayRef cfObjectClasses = (CFArrayRef) CFDictionaryGetValue( cfDictRef, CFSTR(kXMLObjectClasses) );
						if ( cfObjectClasses != NULL && CFGetTypeID(cfObjectClasses) != CFArrayGetTypeID() )
							continue;
						
						CFStringRef cfSearchBase = (CFStringRef) CFDictionaryGetValue( cfDictRef, CFSTR(kXMLSearchBase) );
						if ( cfSearchBase != NULL && CFGetTypeID(cfSearchBase) != CFStringGetTypeID() )
							cfSearchBase = NULL; // don't use it..
						
						CFBooleanRef cfSearchScope = (CFBooleanRef) CFDictionaryGetValue( cfDictRef, CFSTR(kXMLOneLevelSearchScope) );
						if ( cfSearchScope != NULL && CFGetTypeID(cfSearchScope) != CFBooleanGetTypeID() )
							cfSearchScope = NULL; // don't use it..
						
						CFStringRef cfGroupClasses = (CFStringRef) CFDictionaryGetValue( cfDictRef, CFSTR(kXMLGroupObjectClasses) );
						if ( cfGroupClasses != NULL && CFGetTypeID(cfGroupClasses) != CFStringGetTypeID() )
							cfGroupClasses = NULL; // don't use it..
						
						// all that matters is whether or not we got object classes out of this..
						CFIndex	iObjClassCount = cfObjectClasses ? CFArrayGetCount( cfObjectClasses ) : 0;
						
						// not let's loop through and see if some aren't Strings, if so the list is bad, throw out
						for( CFIndex iObjClassIndex = 0; iObjClassIndex < iObjClassCount; iObjClassIndex++ )
						{
							if ( CFGetTypeID(CFArrayGetValueAtIndex(cfObjectClasses, iObjClassIndex)) != CFStringGetTypeID() )
							{
								cfObjectClasses = NULL;
								break;
							}
						}
						
						// if we made it through the loop and still have cfObjectClasses then it is good, let's use it
						if ( cfObjectClasses != NULL )
						{
							// let's allocate a dictionary to hold 4 items..
							cfValidNativeDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
							
							if ( cfSearchBase != NULL )
								CFDictionarySetValue( cfValidNativeDict, CFSTR(kXMLSearchBase), cfSearchBase );
							if ( cfSearchScope != NULL )
								CFDictionarySetValue( cfValidNativeDict, CFSTR(kXMLOneLevelSearchScope), cfSearchScope );
							if ( cfGroupClasses != NULL )
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
						
						DSCFRelease( cfObjectClass );
					}
					
					// if we ended up with a new dictionary, let's add it to the new array we created
					if ( cfValidNativeDict != NULL )
					{
						// add it to the new array
						CFArrayAppendValue( cfNewNativeArray, cfValidNativeDict );
						
						// clean up the ValidNativeDict
						DSCFRelease( cfValidNativeDict );
					}
				}
				
				// if we ended up with a new array, then we need to put it in the outgoing dictionary accordingly
				if ( CFArrayGetCount( cfNewNativeArray ) > 0 )
				{
					// maximum of 2 entries - Native Array and Attribute Map
					CFMutableDictionaryRef cfNewMap = CFDictionaryCreateMutable( kCFAllocatorDefault, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
					
					// now let's get a good copy of the attribute map list.. by normalizing with the global list
					CFArrayRef cfArray = (CFArrayRef) CFDictionaryGetValue( recMapDict, CFSTR(kXMLAttrTypeMapArrayKey) );
					if ( cfArray != NULL )
					{
						CFDictionaryRef cfAttribMap = CreateNormalizedAttributeMap( cfArray, cfGlobalAttrMap );
						if ( cfAttribMap != NULL )
						{
							CFDictionarySetValue( cfNewMap, CFSTR(kXMLAttrTypeMapDictKey), cfAttribMap );
							DSCFRelease( cfAttribMap );
						}
					}
					else if ( cfGlobalAttrMap )
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
	if ( outRecMapDict != NULL && CFDictionaryGetCount(outRecMapDict) == 0 )
		DSCFRelease( outRecMapDict );
	
	return outRecMapDict;
}

int32_t CLDAPNodeConfig::CalculateSecurityPolicy( CFDictionaryRef inConfiguration )
{
	CFBooleanRef	cfBool;
	int32_t			iSecurityLevel = kSecNoSecurity;
	
	if ( inConfiguration != NULL && CFGetTypeID(inConfiguration) == CFDictionaryGetTypeID() )
	{
		if ( (cfBool = (CFBooleanRef) CFDictionaryGetValue(inConfiguration, CFSTR(kXMLSecurityNoClearTextAuths))) && CFBooleanGetValue(cfBool) )
			iSecurityLevel |= kSecDisallowCleartext;
		
		if ( (cfBool = (CFBooleanRef) CFDictionaryGetValue(inConfiguration, CFSTR(kXMLSecurityManInTheMiddle))) && CFBooleanGetValue(cfBool) )
			iSecurityLevel |= kSecManInMiddle;
		
		if ( (cfBool = (CFBooleanRef) CFDictionaryGetValue(inConfiguration, CFSTR(kXMLSecurityPacketSigning))) && CFBooleanGetValue(cfBool) )
			iSecurityLevel |= kSecPacketSigning;
		
		if ( (cfBool = (CFBooleanRef) CFDictionaryGetValue(inConfiguration, CFSTR(kXMLSecurityPacketEncryption))) && CFBooleanGetValue(cfBool) )
			iSecurityLevel |= kSecPacketEncryption;
	}
	
	return iSecurityLevel;
}

int CLDAPNodeConfig::SASLInteract( LDAP *ld, unsigned flags, void *inDefaults, void *inInteract )
{
	sasl_interact_t *interact = (sasl_interact_t *)inInteract;
	saslDefaults *defaults = (saslDefaults *) inDefaults;
	
	if ( ld == NULL ) return LDAP_PARAM_ERROR;
	
	while ( interact->id != SASL_CB_LIST_END )
	{
		const char *dflt = interact->defresult;
		
		switch( interact->id )
		{
			case SASL_CB_AUTHNAME:
				if ( defaults ) dflt = defaults->authcid;
				break;
			case SASL_CB_PASS:
				if ( defaults ) dflt = defaults->password;
				break;
			case SASL_CB_USER:
				if ( defaults ) dflt = defaults->authzid;
				break;
		}
		
		// if we don't have a value NULL it...
		if ( dflt && !(*dflt) ) 
			dflt = NULL;
		
		// if we have a return value or SASL_CB_USER
		if ( dflt || interact->id == SASL_CB_USER )
		{
			// we must either return something or an empty value otherwise....
			interact->result = ((dflt && *dflt) ? dflt : "");
			interact->len = strlen( (char *)interact->result );
		} else {
			return LDAP_OTHER;
		}
		
		interact++;
	}
	return LDAP_SUCCESS;
} 

bool CLDAPNodeConfig::GetUserTGTIfNecessaryAndStore( const char *inName, const char *inPassword, char **outCacheName )
{
	if ( inName == NULL || inPassword == NULL )
		return false;
	
	int				siResult			= eDSAuthFailed;
	krb5_context	krbContext			= NULL;
	krb5_principal	principal			= NULL;
	krb5_principal	cachePrinc			= NULL;
	krb5_ccache		krbCache			= NULL;
	krb5_error_code	retval				= 0;
	krb5_creds 		mcreds;
	krb5_creds		my_creds;
	bool			bNeedNewCred		= true;
	char			*principalString	= NULL;
	char			*pCacheName			= NULL;
	
	gKerberosMutex->WaitLock();
	
	do
	{
		retval = krb5_init_context( &krbContext );
		if ( retval != 0 ) break;
		
		retval = krb5_parse_name( krbContext, inName, &principal );
		if ( retval != 0 ) break;
		
		retval = krb5_unparse_name( krbContext, principal, &principalString );
		if ( retval != 0 ) break;
		
		pCacheName = (char *) malloc( sizeof("MEMORY:") + strlen(principalString) + 1 );
		strcpy( pCacheName, "MEMORY:" );
		strcat( pCacheName, principalString );
		
		// let's see if we already have a cache for this user..
		retval = krb5_cc_resolve( krbContext, pCacheName, &krbCache);
		if ( retval != 0 ) break;
		
		retval = krb5_cc_set_default_name( krbContext, pCacheName );
		if ( retval != 0 ) break;
		
		// Lets get the cache principal...
		retval = krb5_cc_get_principal(krbContext, krbCache, &cachePrinc);
		if ( retval == 0 )
		{
			// Now let's compare the principals to see if they match....
			if ( krb5_principal_compare(krbContext, principal, cachePrinc) )
			{
				// now let's retrieve the TGT and see if it is expired yet..
				memset( &mcreds, 0, sizeof(mcreds) );
				
				retval = krb5_copy_principal( krbContext, principal, &mcreds.client );
				if ( retval != 0 ) break;
				
				// let's build the principal for the TGT so we can pull it from the cache...
				retval = krb5_build_principal_ext( krbContext, &mcreds.server, principal->realm.length, principal->realm.data, 
												   KRB5_TGS_NAME_SIZE, KRB5_TGS_NAME, principal->realm.length, principal->realm.data, 0 );
				if ( retval != 0 ) break;
				
				// this retrieves the TGT from the cache if it exists, if it doesn't, then we will get a new credential below
				retval = krb5_cc_retrieve_cred( krbContext, krbCache, KRB5_TC_SUPPORTED_KTYPES, &mcreds, &my_creds );
				if ( retval == 0 )
				{
					krb5_int32 timeret = 0;
					
					krb5_timeofday( krbContext, &timeret );
					
					// let's check for 5 minutes ahead.. so we don't expire in the middle of doing something....
					timeret += 600;
					
					// if TGT is about to expire, let's just re-initialize the cache, and get a new one
					if ( timeret > my_creds.times.endtime )
					{
						krb5_cc_initialize( krbContext, krbCache, principal );
						DbgLog( kLogPlugin, "CLDAPNodeConfig::GetUserTGTIfNecessaryAndStore - Existing TGT expires shortly, initializing cache for user %s", principalString );
					}
					else
					{
						// otherwise, our credentials are fine, no need to get new ones..
						bNeedNewCred = false;
						DbgLog( kLogPlugin, "CLDAPNodeConfig::GetUserTGTIfNecessaryAndStore - Existing TGT for user %s is Valid", principalString );
					}
					
					krb5_free_cred_contents( krbContext, &my_creds );
				}
				
				krb5_free_cred_contents( krbContext, &mcreds );
			}
			else
			{
				DbgLog( kLogPlugin, "CLDAPNodeConfig::GetUserTGTIfNecessaryAndStore - Cache for user %s being initialized", principalString );
				
				retval = krb5_cc_initialize( krbContext, krbCache, principal );
				if ( retval != 0 ) break;
			}
		}
		else
		{
			DbgLog( kLogPlugin, "CLDAPNodeConfig::GetUserTGTIfNecessaryAndStore - Uninitialized cache available, initializing for user %s", principalString );
			
			retval = krb5_cc_initialize( krbContext, krbCache, principal );
			if ( retval != 0 ) break;
		}
		
		krb5_int32				startTime   = 0;
		krb5_get_init_creds_opt	options;
		
		memset( &my_creds, 0, sizeof(my_creds) );
		
		krb5_get_init_creds_opt_init( &options );
		krb5_get_init_creds_opt_set_forwardable( &options, 1 );
		krb5_get_init_creds_opt_set_proxiable( &options, 1 );
				
		// if we don't need new credentials, lets set the ticket life really short
		if ( bNeedNewCred == false )
		{
			krb5_get_init_creds_opt_set_tkt_life( &options, 300 ); // minimum is 5 minutes
			DbgLog( kLogPlugin, "CLDAPNodeConfig::GetUserTGTIfNecessaryAndStore - Getting TGT with short ticket life for verification only for user %s", principalString );
		}
		
		// we need to verify the password anyway...
		retval = krb5_get_init_creds_password( krbContext, &my_creds, principal, (char *)inPassword, NULL, 0, startTime, NULL, &options );
		if ( retval != 0 )
		{
			DbgLog( kLogPlugin, "CLDAPNodeConfig::GetUserTGTIfNecessaryAndStore - Error %d getting TGT", retval );
			break;
		}
		
		// if we needed new credentials, then we need to store them..
		if ( bNeedNewCred )
		{
			DbgLog( kLogPlugin, "CLDAPNodeConfig::GetUserTGTIfNecessaryAndStore - Storing credentials in Kerberos cache for user %s", principalString );
			retval = krb5_cc_store_cred( krbContext, krbCache, &my_creds);
		}
		else 
		{
			DbgLog( kLogPlugin, "CLDAPNodeConfig::GetUserTGTIfNecessaryAndStore - Valid credentials in Kerberos cache for user %s", principalString );
		}
		
		// No need to hold onto credentials here..
		krb5_free_cred_contents( krbContext, &my_creds );
		
		if ( retval == 0 )
			siResult = eDSNoErr;
		
	} while ( 0 );
	
	if ( outCacheName != NULL )
		(*outCacheName) = pCacheName;
	else
		DSDelete( pCacheName );
	
	if ( principalString )
	{
		krb5_free_unparsed_name( krbContext, principalString );
		principalString = NULL;
	}
	
	if ( principal )
	{
		krb5_free_principal( krbContext, principal );
		principal = NULL;
	}
	
	if ( cachePrinc )
	{
		krb5_free_principal( krbContext, cachePrinc );
		cachePrinc = NULL;
	}
	
	if ( krbCache )
	{
		// if the auth failed, let's destroy the cache instead..
		if ( siResult == eDSAuthFailed )
		{
			krb5_cc_destroy( krbContext, krbCache );
			krbCache = NULL;
		}
		else 
		{
			krb5_cc_close( krbContext, krbCache );
			krbCache = NULL;
		}
	}
	
	if ( krbContext )
	{
		krb5_free_context( krbContext );
		krbContext = NULL;
	}
	
	gKerberosMutex->SignalLock();
	
	return (siResult == eDSNoErr);
}

void CLDAPNodeConfig::BuildReplicaList( void )
{
	CFMutableArrayRef	tempList		= CFArrayCreateMutableCopy( kCFAllocatorDefault, 0, fReadReplicas );
	CFRange				rangeOfWrites	= CFRangeMake( 0, CFArrayGetCount(fWriteReplicas) );
	char				portString[32];
	char				serverName[512];
	struct addrinfo		hints;
	CFIndex				numReps;
	
	CFArrayAppendArray( tempList, fWriteReplicas, rangeOfWrites );
	numReps = CFArrayGetCount( tempList );
	
	bzero( &hints, sizeof(hints) );
	
	hints.ai_family		= PF_UNSPEC; //IPV4 or IPV6
	hints.ai_socktype	= SOCK_STREAM;
	
	sprintf( portString, "%d", fServerPort );
	
	for ( CFIndex ii = 0; ii < numReps; ii++ )
	{
		struct addrinfo	*addrList		= NULL;
		CFStringRef		replicaStrRef	= (CFStringRef) CFArrayGetValueAtIndex( tempList, ii );

		if ( CFStringGetCString(replicaStrRef, serverName, sizeof(serverName), kCFStringEncodingUTF8) == true)
		{
			if ( getaddrinfo(serverName, portString, &hints, &addrList) == 0 )
			{
				struct addrinfo *addrTemp	= addrList;
				bool			bWriteable	= CFArrayContainsValue( fWriteReplicas, rangeOfWrites, replicaStrRef );
				char			ipAddress[129];
				
				while ( addrTemp != NULL )
				{
					CLDAPReplicaInfo	*replicaInfo	= NULL;

					if ( addrTemp->ai_family == AF_INET && 
						 inet_ntop(AF_INET, (const void *)&(((struct sockaddr_in*)(addrTemp->ai_addr))->sin_addr), 
								   ipAddress, sizeof(ipAddress)) == NULL )
					{
						addrTemp = addrTemp->ai_next;
						continue;
					}
					else if ( addrTemp->ai_family == AF_INET6 && 
							  inet_ntop( AF_INET6, (const void *)&(((struct sockaddr_in6*)(addrTemp->ai_addr))->sin6_addr), 
										 ipAddress, sizeof(ipAddress) ) == NULL ) 
					{
						addrTemp = addrTemp->ai_next;
						continue;
					}
					
					// see if this IP address is in our replica list
					CFStringRef cfTempString = CFStringCreateWithCStringNoCopy( kCFAllocatorDefault, ipAddress, kCFStringEncodingUTF8, 
																			    kCFAllocatorNull );
					bool		bIPWriteable = (bWriteable ? bWriteable : CFArrayContainsValue(fWriteReplicas, rangeOfWrites, cfTempString));
					
					DSCFRelease( cfTempString );
					
					// see if we have it already
					for ( ListOfReplicasI iter = fReplicaList.begin(); iter != fReplicaList.end(); iter++ )
					{
						replicaInfo = (*iter);
						if ( strcmp(ipAddress, replicaInfo->fIPAddress) == 0 )
						{
							if ( replicaInfo->fbSupportsWrites != bIPWriteable )
							{
								replicaInfo->fbSupportsWrites = bIPWriteable;
								DbgLog( kLogPlugin, "CLDAPNodeConfig::BuildReplicaList - Node %s - changed replica %s to %s", fNodeName, ipAddress,
									   (bIPWriteable ? "read/write" : "read") );
							}
							break;
						}
						
						replicaInfo = NULL;
					}

					if ( replicaInfo == NULL )
					{
						replicaInfo = new CLDAPReplicaInfo( ipAddress, fServerPort, fIsSSL, bIPWriteable );
						if ( replicaInfo != NULL )
						{
							fReplicaList.push_back( replicaInfo );
							DbgLog( kLogPlugin, "CLDAPNodeConfig::BuildReplicaList - Node %s - new replica %s for %s", fNodeName, ipAddress,
								    (bIPWriteable ? "read/write" : "read") );
						}
					}
					
					addrTemp = addrTemp->ai_next;
				}
				
				freeaddrinfo( addrList );
			}
		}
	}
	
	DSCFRelease( tempList );
    
} // BuildReplicaInfoLinkList

void CLDAPNodeConfig::MergeArraysRemovingDuplicates( CFMutableArrayRef cfPrimaryArray, CFArrayRef cfArrayToAdd )
{
    CFIndex addCount    = CFArrayGetCount( cfArrayToAdd );
    CFRange cfRange     = CFRangeMake( 0, CFArrayGetCount(cfPrimaryArray) );
    
    for( CFIndex ii = 0; ii < addCount; ii++ )
    {
        CFStringRef cfString    = (CFStringRef) CFArrayGetValueAtIndex( cfArrayToAdd, ii );
        
        if( CFArrayContainsValue(cfPrimaryArray, cfRange, cfString) == false )
        {
            CFArrayAppendValue( cfPrimaryArray, cfString );
            cfRange.length++; // increase range cause we just added one
        }
    }
} //MergeArraysRemovingDuplicates

SInt32 CLDAPNodeConfig::GetReplicaListFromDNS( CFMutableArrayRef inOutRepList )
{
	SInt32				siResult		= eDSNoStdMappingAvailable;
	CFMutableArrayRef	serviceRecords  = NULL;
	CFMutableArrayRef	aRepList		= NULL; //no writeable replicas obtained here
	CFStringRef			aString			= NULL;
	
    // let's see if it is a hostname and not an IP address
	CFStringRef cfServerName = CFStringCreateWithCString( kCFAllocatorDefault, fNodeName, kCFStringEncodingUTF8 );
	CFArrayRef  cfComponents = CFStringCreateArrayBySeparatingStrings( kCFAllocatorDefault, cfServerName, CFSTR(".") );
	bool        bIsDNSname  = true;
	int         namePartCount = 1; // default to 1
	CFStringRef cfDomainSuffix = NULL;

	if( cfComponents != NULL )
	{
		namePartCount = CFArrayGetCount( cfComponents );
		
		// if we have 4 components, let's see if the last one is all numbers
		if( namePartCount == 4 && CFStringGetIntValue((CFStringRef) CFArrayGetValueAtIndex(cfComponents, 3)) != 0 )
		{
			bIsDNSname = false;
		}
		
		// if we are a DNS name, let's compose the suffix for this domain for later use..
		if( bIsDNSname && namePartCount > 2 )
		{
			cfDomainSuffix = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%@.%@"), CFArrayGetValueAtIndex(cfComponents, namePartCount-2), CFArrayGetValueAtIndex(cfComponents, namePartCount-1) );
		}
	}

	DSCFRelease( cfServerName );
	DSCFRelease( cfComponents );
	
	// if we have a DNS name and not a dotted IP address then there is something to do
	if( bIsDNSname )
	{
		// if we are using credentials we will use any server that is available from our primary DNS domain
		// if we have more than 2 parts to the server name and we have a domain suffix (i.e., server.domain.com vs. domain.com)
		if( fSecureUse == true && namePartCount > 2 && cfDomainSuffix != NULL )
		{
			CFStringRef         cfKey  = SCDynamicStoreKeyCreateNetworkGlobalEntity( kCFAllocatorDefault, kSCDynamicStoreDomainState, kSCEntNetDNS );
			SCDynamicStoreRef   cfStore = SCDynamicStoreCreate( kCFAllocatorDefault, CFSTR("DirectoryService"), NULL, NULL );
			CFDictionaryRef     cfDNSdomainsDict = (CFDictionaryRef) SCDynamicStoreCopyValue( cfStore, cfKey );
			
			DSCFRelease( cfKey );
			DSCFRelease( cfStore );
			
			CFArrayRef cfSearchArray = (CFArrayRef) CFDictionaryGetValue( cfDNSdomainsDict, kSCPropNetDNSSearchDomains );
			if( cfSearchArray != NULL && CFArrayGetCount( cfSearchArray ) )
			{
				CFStringRef cfFirstDomain = (CFStringRef) CFArrayGetValueAtIndex( cfSearchArray, 0 );
				
				// if the suffix of the server matches the configured search domain.. then we will use this search domain to find servers
				if( CFStringHasSuffix( cfFirstDomain, cfDomainSuffix ) )
				{
					UInt32 uiLength = (UInt32) CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfFirstDomain), kCFStringEncodingUTF8 ) + 1;
					char *domain = (char *) calloc( sizeof(char), uiLength );
					CFStringGetCString( cfFirstDomain, domain, uiLength, kCFStringEncodingUTF8 );

					DbgLog( kLogPlugin, "CLDAPNodeConfig::GetReplicaListFromDNS - Node %s - Checking computer's primary domain %s", fNodeName, domain );

					serviceRecords = getDNSServiceRecs( "ldap", domain );
					
					DSFreeString( domain );
				}
			}
			
			DSCFRelease( cfDNSdomainsDict );
		}

		DSCFRelease( cfDomainSuffix );

		// searchRecords is still NULL, we will search the _ldap._tcp.configuredserver.domain.com
		if( serviceRecords == NULL )
		{
			DbgLog( kLogPlugin, "CLDAPNodeConfig::GetReplicaListFromDNS - Node %s - Checking domain %s", fNodeName );
			serviceRecords = getDNSServiceRecs( "ldap", fNodeName );
		}

		// if we are using authentication and there are more than 2 pieces to the domain name
		//    let's search the subdomain under the server so "_ldap._tcp.domain.com"
		if( fSecureUse == true && namePartCount > 2 && serviceRecords == NULL )
		{
			char *subDomain = strchr( fNodeName, '.' ) + 1; // plus 1 to skip the dot itself
			
			if( subDomain )
			{
				DbgLog( kLogPlugin, "CLDAPNodeConfig::GetReplicaListFromDNS - Node %s - Checking domain %s", fNodeName, subDomain );
				serviceRecords = getDNSServiceRecs( "ldap", subDomain );
			}
		}
	}
	
	if ( serviceRecords != NULL )
	{
		aRepList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
		CFIndex totalCount = CFArrayGetCount(serviceRecords);
		for (CFIndex indexToCnt=0; indexToCnt < totalCount; indexToCnt++ )
		{
			aString = (CFStringRef)CFDictionaryGetValue( (CFDictionaryRef)::CFArrayGetValueAtIndex( serviceRecords, indexToCnt ), CFSTR("Host"));
			if ( aString != NULL );
			{
				CFArrayAppendValue(aRepList, aString);
			}
		}
		CFRelease(serviceRecords);
	}
	
	if ( aRepList != NULL )
	{
        // if new data found
        if( CFArrayGetCount(aRepList) > 0 )
        {
            DbgLog( kLogPlugin, "CLDAPNodeConfig::GetReplicaListFromDNS - Node %s - Found ldap replica servers in DNS service records.", fNodeName );
            
            // we merge the replica list to the DNS list, DNS is has priority
            MergeArraysRemovingDuplicates( aRepList, inOutRepList ); 
            
            CFArrayRemoveAllValues( inOutRepList );
            CFArrayAppendArray( inOutRepList, aRepList, CFRangeMake(0,CFArrayGetCount(aRepList)) );

            siResult = eDSNoErr;
        }
        else
        {
            DbgLog( kLogPlugin, "CLDAPNodeConfig::GetReplicaListFromDNS - Node %s - No ldap replica servers in DNS service records.", fNodeName );
        }
        
        CFRelease( aRepList );
        aRepList = NULL;
	} 
    else
    {
        DbgLog( kLogPlugin, "CLDAPNodeConfig::GetReplicaListFromDNS - Node %s - No ldap replica servers in DNS service records.", fNodeName );
    }
    
	return(siResult);
}//GetReplicaListFromDNS

SInt32 CLDAPNodeConfig::GetReplicaListFromAltServer( LDAP *inHost, CFMutableArrayRef inOutRepList )
{
	SInt32				siResult		= eDSRecordNotFound;
	LDAPMessage		   *result			= NULL;
	const char		   *attrs[2]		= { "altserver", NULL };
    CFMutableArrayRef   aRepList        = NULL;
	struct timeval		timeout			= { fOpenCloseTimeout, 0 };

	//search for the specific LDAP record altserver at the rootDSE which may contain
	//the list of LDAP replica urls
	int ldapReturnCode = ldap_search_ext_s( inHost, "", LDAP_SCOPE_BASE, "(objectclass=*)", (char **) attrs, 0, NULL, NULL, &timeout, 0, &result );
	if ( ldapReturnCode == LDAP_SUCCESS )
	{
        aRepList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
        
		struct berval **bValues = ldap_get_values_len (inHost, result, "altserver" );
		if ( bValues != NULL )
		{					
			// for each value of the attribute we need to parse and add as an entry to the outList
			for (int i = 0; bValues[i] != NULL; i++ )
			{
				if ( bValues[i] != NULL )
				{
					LDAPURLDesc		*ludpp = NULL;
					
					if ( ldap_url_parse( bValues[i]->bv_val, &ludpp) == LDAP_SUCCESS )
					{
						if ( ludpp->lud_port == LDAPS_PORT )
						{
							// if we are setup for SSL, we'll use it
							if ( fIsSSL == true )
							{
								CFStringRef cfString = CFStringCreateWithCString( kCFAllocatorDefault, ludpp->lud_host, kCFStringEncodingUTF8 );
								if ( cfString != NULL )
								{
									CFArrayAppendValue( aRepList, cfString );
									DSCFRelease( cfString );
								}
							}
						}
						
						ldap_free_urldesc( ludpp );
						ludpp = NULL;
					}

					// TODO: KW which of these are writeable?
					siResult = eDSNoErr;
				}
			}
			
			ldap_value_free_len(bValues);
		} // if bValues = ldap_get_values_len ...
	}
	
	if ( result != NULL )
	{
		ldap_msgfree( result );
		result = NULL;
	}
	
    if ( aRepList != NULL )
	{
        // if new data found
        if( CFArrayGetCount(aRepList) > 0 )
        {
            // merge the new list into the existing list
            DbgLog( kLogPlugin, "CLDAPNodeConfig::GetReplicaListFromAltServer - Node %s - Found some ldap replica servers in rootDSE altServer.",
				    fNodeName );
            MergeArraysRemovingDuplicates( inOutRepList, aRepList ); 
            siResult = eDSNoErr;
        }
        else
        {
            DbgLog( kLogPlugin, "CLDAPNodeConfig::GetReplicaListFromAltServer - Node %s - No ldap replica servers in rootDSE altServer.", fNodeName );
        }
	} 
    else
    {
        DbgLog( kLogPlugin, "CLDAPNodeConfig::GetReplicaListFromAltServer - Node %s - No ldap replica servers in rootDSE altServer.", fNodeName );
    }
	
	DSCFRelease( aRepList );
    	
	return siResult;

} // GetReplicaListFromAltServer
