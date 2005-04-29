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

#ifndef __CLDAPv3Configs_h__
#define __CLDAPv3Configs_h__	1

#include <stdio.h>
#include <map>
#include <set>
#include <string>
#include <list>
#include <sys/types.h>	//for getaddrinfo
#include <sys/socket.h>	//for getaddrinfo
#include <netdb.h>		//for getaddrinfo

#include <lber.h>
#include <ldap.h>

#include <CoreFoundation/CoreFoundation.h>		//for CF classes and property lists - XML config data

#include "PrivateTypes.h"
#include "CPlugInRef.h"			// config data table
#include "DSLDAPUtils.h"		// for timeout values

using namespace std;

//XML label tags
#define	kXMLLDAPVersionKey			"LDAP PlugIn Version"
#define kXMLConfigArrayKey			"LDAP Server Configs"
#define kXMLDHCPConfigArrayKey		"LDAP DHCP Server Configs"
#define kXMLServerConfigKey			"LDAP Server Config"

#define kXMLEnableUseFlagKey		"Enable Use"
#define kXMLUserDefinedNameKey		"UI Name"
#define kXMLNodeName				"Node Name"

#define kXMLOpenCloseTimeoutSecsKey	"OpenClose Timeout in seconds"
#define kXMLIdleTimeoutMinsKey		"Idle Timeout in minutes"
#define kXMLDelayedRebindTrySecsKey	"Delay Rebind Try in seconds"
#define kXMLPortNumberKey			"Port Number"
#define kXMLSearchTimeoutSecsKey	"Search Timeout in seconds"
#define kXMLSecureUseFlagKey		"Secure Use"
#define kXMLServerKey				"Server"
#define kXMLServerAccountKey		"Server Account"
#define kXMLServerPasswordKey		"Server Password"
#define kXMLKerberosId				"Kerberos Id"
#define kXMLUseDNSReplicasFlagKey	"Use DNS replicas"

// New Directory Binding functionality --------------
//

// kXMLBoundDirectoryKey => indicates the computer is bound to this directory.
//         This prevents them from changing:  server account, password, 
//         secure use, and port number.  It also means the config cannot  
//         be deleted, without unbinding.
#define kXMLBoundDirectoryKey			"Bound Directory"

// macosxodpolicy config Record flags
//
// These new flags are for determining config-record settings..
#define kXMLDirectoryBindingKey			"Directory Binding"

// Dictionary of keys
#define kXMLConfiguredSecurityKey		"Configured Security Level"
#define kXMLSupportedSecurityKey		"Supported Security Level"
#define kXMLLocalSecurityKey			"Local Security Level"

// Keys for above Dictionaries
#define kXMLSecurityBindingRequired		"Binding Required"
#define kXMLSecurityNoClearTextAuths	"No ClearText Authentications"
#define kXMLSecurityManInTheMiddle		"Man In The Middle"
#define kXMLSecurityPacketSigning		"Packet Signing"
#define kXMLSecurityPacketEncryption	"Packet Encryption"

// Corresponding bit flags for quick checks..
#define kSecNoSecurity			0
#define kSecDisallowCleartext	(1<<0)
#define kSecManInMiddle			(1<<1)
#define kSecPacketSigning		(1<<2)
#define kSecPacketEncryption	(1<<3)

#define kSecSecurityMask		(kSecDisallowCleartext | kSecManInMiddle | kSecPacketSigning | kSecPacketEncryption)

//
// End New Directory Binding functionality ---------------

#define kXMLStdMapUseFlagKey				"Standard Map Use"
#define kXMLDefaultAttrTypeMapArrayKey		"Default Attribute Type Map"
#define kXMLDefaultRecordTypeMapArrayKey	"Default Record Type Map"
#define kXMLAttrTypeMapArrayKey				"Attribute Type Map"
#define kXMLRecordTypeMapArrayKey			"Record Type Map"
#define kXMLReplicaHostnameListArrayKey		"Replica Hostname List"
#define kXMLWriteableHostnameListArrayKey	"Writeable Hostname List"
#define kXMLNativeMapArrayKey				"Native Map"
#define kXMLStdNameKey						"Standard Name"
#define kXMLSearchBase						"Search Base"
#define kXMLOneLevelSearchScope				"One Level Search Scope"
#define kXMLObjectClasses					"Object Classes"
#define kXMLGroupObjectClasses				"Group Object Classes"
#define kXMLMakeDefLDAPFlagKey				"Default LDAP Search Path"
#define kXMLServerMappingsFlagKey			"Server Mappings"
#define kXMLIsSSLFlagKey					"SSL"
#define kXMLLDAPv2ReadOnlyKey				"LDAPv2 Read Only"
#define kXMLMapSearchBase					"Map Search Base"
#define kXMLReferralFlagKey					"LDAP Referrals"
//kXMLMakeDefLDAPFlagKey => indicates the server config is a Default LDAP Node
//kXMLServerMappingsFlagKey => indicates the server config comes directly from the LDAP
//                                server itself and can only be written with authentication

#define kXMLAttrTypeMapDictKey				"Attribute Type Map"

typedef list<string>					listOfStrings;
typedef listOfStrings::const_iterator	listOfStringsCI;

typedef set<string>						AttrSet;
typedef AttrSet::const_iterator			AttrSetCI;

struct sObjectClassSchema {
	AttrSet			fParentOCs;			//hierarchy parents
	AttrSet			fOtherNames;		//other names of same OC
	AttrSet			fRequiredAttrs;		//required attributes
	AttrSet			fAllowedAttrs;		//allowed other attributes
	uInt16			fType;				//0=>Abstract, 1=>Structural, 2=>Auxiliary
	uInt32			fDummy;
};

typedef map<string,sObjectClassSchema*>		ObjectClassMap;
typedef ObjectClassMap::const_iterator		ObjectClassMapCI;

struct sReplicaInfo
{
	addrinfo	   *fAddrInfo;		//addrinfo struct
	bool			bWriteable;		//host is writable with proper authentication
	bool			bUsedLast;		//host was last used replica
	CFStringRef		hostname;		//hostname from config or replica list
	sReplicaInfo   *fNext;			//next struct in linked list
	
	// constructors/destructors
	sReplicaInfo( void )
	{
		fAddrInfo = nil;
		bUsedLast = bWriteable = false;
		hostname = nil;
		fNext = nil;
	}
	~sReplicaInfo( void )
	{
		if ( fNext != nil )
		{
			// we have items on our list.  Isolate each one and delete
			sReplicaInfo*	replicaToIsolateAndDelete = fNext;
			
			while ( replicaToIsolateAndDelete != nil )
			{
				sReplicaInfo*	nextReplicaPtr = replicaToIsolateAndDelete->fNext;
				replicaToIsolateAndDelete->fNext = NULL; // so that we stop recursion
				DSDelete(replicaToIsolateAndDelete);
				replicaToIsolateAndDelete = nextReplicaPtr;
			}
		}
		
		if( fAddrInfo )
		{
			freeaddrinfo( fAddrInfo );
			fAddrInfo = nil;
		}
		DSCFRelease( hostname );
	}
	sReplicaInfo *lastUsed( void )
	{
		sReplicaInfo *pLastUsed = nil;
		sReplicaInfo *curReplicaInfo = this;
		
		while( curReplicaInfo )
		{
			if( curReplicaInfo->bUsedLast )
			{
				pLastUsed = curReplicaInfo;
				break;
			}
			else
			{
				curReplicaInfo = curReplicaInfo->fNext;
			}
		}

		return pLastUsed;
	}
	void resetLastUsed( void )  // this resets the last used
	{
		sReplicaInfo *curReplicaInfo = this;
		
		while( curReplicaInfo )
		{
			curReplicaInfo->bUsedLast = false;
			curReplicaInfo = curReplicaInfo->fNext;
		}
	}
};

//LDAPServer config data structure
struct sLDAPConfigData
{
	char			   *fUIName;			//LDAP defined name for the LDAP server
	char			   *fNodeName;			//LDAP defined Node Name for the LDAP server
	char			   *fServerName;		//LDAP defined LDAP Server fqdn
	int					fServerPort;		//LDAP server port ie. default is 389 - SSL default port is 636
	
	sReplicaInfo	   *fReplicaHosts;		//list of LDAP replica hosts
	CFMutableArrayRef	fReplicaHostnames;	//list of all LDAP replica hostnames
	CFMutableArrayRef	fWriteableHostnames;//list of writeable LDAP replica hostnames
	bool				bBuildReplicaList;	//set to indicate the LDAP Replica list needs to be built
	
	int					fOpenCloseTimeout;	//Open and Close timeout in seconds
	int					fIdleTimeout;		//Idle timeout in minutes - NOT USED if set to ZERO
	int					fSearchTimeout;		//Search timeout in seconds
	int					fDelayRebindTry;	//Delay rebind try after bind failure in seconds

	bool				bAvail;				//flag determining whether the LDAP server
											//   connection is available
	bool				bSecureUse;			//flag determing LDAP use with secure auth's
	
	char			   *fServerAccount;		//LDAP server account id
	char			   *fKerberosId;		// this is the KerberosID from the configuration file if it exists
	char			   *fServerPassword;	//LDAP server password

	bool				bServerMappings;	//whether mappings are ldap server provided or not
	bool				bIsSSL;				//if SSL connections used this is set
	bool				bLDAPv2ReadOnly;	//if LDAP server should be treated as v2 read only
	char			   *fMapSearchBase;		//map Searchbase used for templates
	uInt32				fSecurityLevel;		//Security Level internal bit settings
	uInt32				fSecurityLevelLoc;	//Local security level
	
	CFDictionaryRef		fRecordAttrMapDict; // in dictionary form so we can lookup faster..
	CFArrayRef			fRecordTypeMapCFArray;
	CFArrayRef			fAttrTypeMapCFArray;

	DSMutexSemaphore   *fConfigLock;		// lock for accessing this configuration

	bool				bGetServerMappings;	//set to indicate whether server mappings need to be retrieved
	bool				bGetSecuritySettings; // set to indicate whether latest security settings retrieved
	bool				bOCBuilt;			//flag to be set when OC Schema build already attempted
	bool				bUpdated;			//flag used to determine updates to the configuration
	bool				bReferrals;			//flag used to determine if Referrals are enabled
	bool				bDNSReplicas;		//flag used to determine if DNS-based Replicas are enabled

	ObjectClassMap	   *fObjectClassSchema;	//dictionary of object class schema
	CFMutableArrayRef   fSASLmethods;
	bool				bUseAsDefaultLDAP;	//this node will be used as one of the default
											//   LDAP nodes in the search policy
	int					fRefCount;			//ref count the use of this config for deletion safety
	bool				bMarkToDelete;		//indicates whether there was a request to delete this config

	// give sLDAPConfigData a construct and destructor
	sLDAPConfigData( void );
	~sLDAPConfigData( void );
	
	sLDAPConfigData(	char *inUIname, char *inNodeName,
						char *inServerName, int inOpenCloseTO,
						int inIdleTO, int inDelayRebindTry,
						int inSearchTO, int inPortNum,
						bool inUseSecure, char *inAccount,
						char *inPassword, char *inKerberosId,
						bool inMakeDefLDAP, bool inServerMappings,
						bool inIsSSL, char *inMapSearchBase,
						int inSecurityLevel, int inSecurityLevelLoc, bool inReferrals,
						bool inLDAPv2ReadOnly, bool inDNSReplicas );
};

typedef map<string,sLDAPConfigData *>		LDAPConfigDataMap;
typedef LDAPConfigDataMap::iterator			LDAPConfigDataMapI;

class CLDAPv3Configs
{
public:
						CLDAPv3Configs		(	void );
	sInt32				Init				(	void );
	virtual			   ~CLDAPv3Configs		(	void );
	sInt32				AddToConfig			(	CFDataRef xmlData );
	sInt32				SetXMLConfig		(	CFDataRef xmlData );
	CFDataRef			CopyXMLConfig		(	void );
	sInt32				WriteXMLConfig		(	void );
	char			   *ExtractRecMap		(	const char *inRecType,
												CFDictionaryRef inRecordTypeMapCFDict,
												int inIndex,
												bool *outOCGroup,
												CFArrayRef *outOCListCFArray,
												ber_int_t *outScope );
	char			   *ExtractAttrMap		(	const char *inRecType,
												const char *inAttrType,
												CFDictionaryRef inRecordTypeMapCFDict,
												int inIndex );
	char			   *ExtractStdAttrName	(	char *inRecType,
												CFDictionaryRef inRecordTypeMapCFDict, 
												int &inputIndex );
	int					AttrMapsCount		(	const char *inRecType,
												const char *inAttrType,
												CFDictionaryRef inRecordTypeMapCFDict );
	sInt32				UpdateLDAPConfigWithServerMappings
											(	char *inServer,
												char *inMapSearchBase,
												int inPortNumber,
												bool inIsSSL,
												bool inLDAPv2ReadOnly,
												bool inMakeDefLDAP,
												bool inReferrals,
												LDAP *inServerHost = nil );
	sInt32				UpdateConfigWithSecuritySettings
											(   char *inConfigName,
												sLDAPConfigData *inConfig,
												LDAP *inLD );
    sInt32				MakeServerBasedMappingsLDAPConfig
                                            (	char *inServer,
                                                char *inMapSearchBase,
                                                int inOpenCloseTO,
												int inIdleTO,
												int inDelayRebindTry,
												int inSearchTO,
												int inPortNumber,
                                                bool inIsSSL,
                                                bool inMakeDefLDAP,
												bool inReferrals,
												bool inIsLDAPv2ReadOnly );
	sInt32				WriteServerMappings (	char* userName,
												char* password,
												CFDataRef inMappings );
	CFDataRef			ReadServerMappings	(	LDAP *serverHost,
												CFDataRef inMappings );
	void				XMLConfigLock		(	void );
	void				XMLConfigUnlock		(	void );
	sInt32				UpdateReplicaList	(	char *inServerName,
												CFMutableArrayRef inReplicaHostnames,
												CFMutableArrayRef inWriteableHostnames);
	void				DeleteConfigFromMap (   char *inConfigNodename );
	sLDAPConfigData    *ConfigWithNodeNameLock
											(   char *inConfigName );
	void				ConfigUnlock		(   sLDAPConfigData *inConfig );
	void				SetAllConfigBuildReplicaFlagTrue
											(   void );
	LDAPConfigDataMap & GetConfigMap		(   void ) { return fConfigMap; }
	void				ConfigMapWait		(   void ) { fConfigMapMutex.Wait(); }
	void				ConfigMapSignal		(   void ) { fConfigMapMutex.Signal(); }
	char			  **GetDefaultLDAPNodeStrings
											(	uInt32 &count );
	void				VerifyKerberosForRealm
											( char *inRealmName,
											  char *inServer );

protected:
	uInt32				CalculateSecurityPolicy
											(	CFDictionaryRef inConfiguration );
	CFDictionaryRef		CreateNormalizedAttributeMap
											(	CFArrayRef inAttrMapArray, 
												CFDictionaryRef inGlobalAttrMap );
	CFDictionaryRef		CreateNormalizedRecordAttrMap	
											(	CFArrayRef inRecMapArray, 
												CFArrayRef inGlobalAttrMapArray );
	CFDataRef			RetrieveServerMappings
											(	char *inServer,
												char *inMapSearchBase,
												int inPortNumber,
												bool inIsSSL,
												bool inReferrals,
												LDAP *inServerHost = nil );
	bool				VerifyXML			(	void );
	sInt32				ConfigLDAPServers	(	void );
	sInt32				AddLDAPServer		( CFDataRef inXMLData );
	CFDataRef			VerifyAndUpdateServerLocation
											(	char *inServer,
												int inPortNumber,
												bool inIsSSL,
												bool inLDAPv2ReadOnly,
												bool inMakeDefLDAP,
												CFDataRef inXMLData );
	char			   *GetVersion			(	CFDictionaryRef configDict );
	sInt32				MakeLDAPConfig		(	CFDictionaryRef ldapDict,
												bool inOverWriteAll = false, 
												bool inServerMappingUpdate = false );
	sInt32				BuildLDAPMap		(	sLDAPConfigData *inConfig,
												CFDictionaryRef ldapDict,
												bool inServerMapppings );
	sInt32				ReadXMLConfig		(	void );
	bool				ConvertLDAPv2Config (   void );
	bool				CreatePrefDirectory (   void );
	char			   *CreatePrefFilename  (   void );
	
private:
		static LDAPConfigDataMap	fConfigMap;
		static DSMutexSemaphore		fConfigMapMutex;
		CFDataRef			fXMLData;
		DSMutexSemaphore	*pXMLConfigLock;
};

#endif	// __CLDAPv3Configs_h__
