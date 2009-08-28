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

#ifndef _CLDAPNODECONFIG_H
#define _CLDAPNODECONFIG_H

#include "CObject.h"
#include "CLDAPReplicaInfo.h"
#include "CLDAPDefines.h"

#include <DirectoryServiceCore/DSMutexSemaphore.h>
#include <CoreFoundation/CoreFoundation.h>
#include <map>
#include <set>
#include <string>
#include <list>
#include <netdb.h>
#include <dispatch/dispatch.h>

using namespace std;

typedef list<CLDAPReplicaInfo *>		ListOfReplicas;
typedef ListOfReplicas::const_iterator	ListOfReplicasI;

typedef list<string>					listOfStrings;
typedef listOfStrings::const_iterator	listOfStringsCI;

typedef set<string>						AttrSet;
typedef AttrSet::const_iterator			AttrSetCI;

struct sObjectClassSchema {
	AttrSet			fParentOCs;			//hierarchy parents
	AttrSet			fOtherNames;		//other names of same OC
	AttrSet			fRequiredAttrs;		//required attributes
	AttrSet			fAllowedAttrs;		//allowed other attributes
	UInt16			fType;				//0=>Abstract, 1=>Structural, 2=>Auxiliary
};

typedef map<string,sObjectClassSchema*>		ObjectClassMap;
typedef ObjectClassMap::const_iterator		ObjectClassMapCI;

class CLDAPv3Configs;
class CLDAPConnection;

class CLDAPNodeConfig : public CObject<CLDAPNodeConfig>
{
	public:
		// if the UUID and name no longer match, then a new config with a new name and UUID are created by the config class
		CFStringRef	fConfigUUID;		// the UUID associated with this config to deal with conflicting names
		char		*fNodeName;			// official nodename ("example.com")
		int32_t		fDHCPLDAPServer;	// this node is a DHCP-based LDAP config
		int32_t		fNodeIsLDAPURL;		// this says to use fNodeName as the node and URI

		// these are all updated Atomically
		int32_t		fIsSSL;				// if SSL connections used this is set
		int32_t		fIdleMaxCount;		// user defined idle timeout in minutes times 2 based on 30 sec periodic task
		int32_t		fSearchTimeout;		// Search timeout in seconds
		int32_t		fOpenCloseTimeout;	// Open and Close timeout in seconds
		int32_t		fDelayRebindTry;	// Delay rebind try after bind failure in seconds
		int32_t		fAvailable;			// overall flag if this config is currently usable (used by reachability)
		int32_t		fSecureUse;			// flag determing LDAP use with secure auth's
		int32_t		fSecurityLevel;		// security level internal bit settings
	
		int32_t		fConfigDeleted;		// if the config is deleted
		int32_t		fEnableUse;			// node is enabled

	public:
						CLDAPNodeConfig			( CLDAPv3Configs *inConfig, const char *inNodeName, CFStringRef inUUID );
						CLDAPNodeConfig			( CLDAPv3Configs *inConfig, const char *inLDAPURL, bool inDHCPLDAPServer = true );
	
		// Various things operating on LDAP references
		LDAP			*EstablishConnection	( CLDAPReplicaInfo **inOutReplicaInfo, bool inWriteable, const char *inLDAPUsername, 
												  const char *inKerberosID, const char *inPassword, void *inCallback, void *inParam,
												  tDirStatus *outStatus);
		LDAP			*EstablishConnection	( CLDAPReplicaInfo **inOutReplicaInfo, bool inWriteable, const char *inKerberosCache,
												  void *inCallback, void *inParam, tDirStatus *outStatus );
		LDAP			*EstablishConnection	( CLDAPReplicaInfo **inOutReplicaInfo, bool inWriteable, void *inCallback, void *inParam, 
												  tDirStatus *outStatus );
	
		bool			UpdateDynamicData		( LDAP *inLD, CLDAPReplicaInfo *inReplica );
		void			ReinitializeReplicaList	( void );
	
		// functions for updating/deleting the configuration
		void			DeleteConfiguration		( void );
		bool			UpdateConfiguraton		( CFDictionaryRef inServerConfig, bool inFromServer );
		CFDictionaryRef	GetConfiguration		( void );
	
		// some accessors for external use
		char			*CopyUIName				( void );
		char			*CopyMapSearchBase		( void );
		bool			CopyCredentials			( char **outUsername, char **outKerberosID, char **outPassword );
		
		// Attribute and Record type mapping
		char			*MapRecToSearchBase		( const char *inRecType, int inIndex, bool *outOCGroup, CFArrayRef *outOCListCFArray,
												  ber_int_t *outScope );
		char			*MapAttrToLDAPType		( const char *inRecType, const char *inAttrType, int inIndex, 
												  bool bSkipLiteralMappings = false );
		char			**MapAttrToLDAPTypeArray( const char *inRecType, const char *inAttrType );
	
		char			*ExtractRecMap			( const char *inRecType, int inIndex, bool *outOCGroup, CFArrayRef *outOCListCFArray,
												  ber_int_t* outScope );
		char			*ExtractAttrMap			( const char *inRecType, const char *inAttrType, int inIndex );
		char			*ExtractStdAttrName		( char *inRecType, int &inputIndex );
		int				AttrMapsCount			( const char *inRecType, const char *inAttrType );
		CFDictionaryRef	CopyNormalizedMappings	( void );
	
		// some object class stuff
		void			GetReqAttrListForObjectList	( CLDAPConnection *inLDAPConnection, listOfStrings &inObjectClassList,
													  listOfStrings &outReqAttrsList );
	
		// Filter routines
		char			*BuildLDAPQueryFilter	( char *inConstAttrType, const char *inConstAttrName, tDirPatternMatch patternMatch, 
												  bool useWellKnownRecType, const char *inRecType, char *inNativeRecType, bool inbOCANDGroup, 
												  CFArrayRef inOCSearchList );
		CFStringRef		ParseCompoundExpression	( const char *inConstAttrName, const char *inRecType );
	
		// network transition occurred, do whatever we want to happen at this point
		void			NetworkTransition		( void );
		bool			CheckIfFailed			( void );

		static void		ReachabilityCallback	( SCNetworkReachabilityRef inTarget, SCNetworkConnectionFlags inFlags, void *inInfo );
	
	private:
		DSMutexSemaphore	fMutex;
		CLDAPv3Configs		*fConfigObject;
	
	// -------  Protected by fMutex ------- 
		ListOfReplicas		fReplicaList;
	
		// used if fbSecureUse is set
		char				*fServerAccount;		// LDAP server account id
		char				*fServerKerberosID;		// this is the KerberosID from the configuration file if it exists
		char				*fServerPassword;		// LDAP server password
	
		char				*fMapSearchBase;		// Map Searchbase used for templates
		char				*fServerName;			// this is always the first one listed (preferred replica)
		char				*fConfigUIName;			// this is the UI name as listed in the config

		// these are rebuilt periodically
		int32_t				fGetServerMappings;		// set to indicate whether server mappings need to be retrieved
		int32_t				fGetReplicas;			// set to indicate whether replicas should be retrieved
		int32_t				fGetSecuritySettings;	// set to indicate whether latest security settings retrieved

		dispatch_source_t	fDynamicRefreshTimer;	// timer that fires for refreshing replicas, etc.

		DSSemaphore			fMappingsLock;
		CFDictionaryRef		fNormalizedMappings;	// in dictionary form so we can lookup faster..
		CFArrayRef			fRecordTypeMapArray;
		CFArrayRef			fAttrTypeMapArray;
	
		CFMutableArrayRef	fReadReplicas;
		CFMutableArrayRef	fWriteReplicas;
		CFMutableArrayRef	fDeniedSASLMethods;		// this disables particular SASL methods
	
		ObjectClassMap		*fObjectClassSchema;	// dictionary of object class schema
	
		SCNetworkReachabilityRef	fReachabilityRef;	// used to track reachability
		CFAbsoluteTime		fLastFailedCheck;
	
	// -------  End Protected ------- 
	
	// ------- Updated Atomically ------- 
		int32_t				fServerPort;			// LDAP server port ie. default is 389 - SSL default port is 636
		int32_t				fReferrals;				// flag used to determine if Referrals are enabled
		int32_t				fDNSReplicas;			// flag used to determine if DNS-based Replicas are enabled
	
		int32_t				fServerMappings;		// whether mappings are ldap server provided or not
	
		int32_t				fLocalSecurityLevel;	// local security level
	// -------  End Atomic Updatable ------- 
	
	private:
		virtual			~CLDAPNodeConfig				( void );
		void			RefreshDynamicData				( void );
		void			ReachabilityNotification		( SCNetworkConnectionFlags inFlags );
		void			SetLDAPOptions					( LDAP *inLDAP );
		void			InitializeVariables				( void );
		LDAP			*FindSuitableReplica			( CLDAPReplicaInfo **inOutReplicaInfo, bool inForceCheck, bool inWriteable, 
														  void *inCallback, void *inParam );
		LDAP			*InternalEstablishConnection	( CLDAPReplicaInfo **inOutReplicaInfo, bool inWriteable, void *inCallback, void *inParam );
		LDAP			*CheckWithSelect				( fd_set &inSet, struct timeval *inCheckTime, int inCount, int *inSockList, 
														  CLDAPReplicaInfo **inReplicas, CLDAPReplicaInfo **outSelectedReplica, void *inCallback,
														  void *inParam );
		void			ClearSockList					( int *inSockList, int inSockCount, bool inClose );
		bool			IsLocalAddress					( struct addrinfo *addrInfo );
		tDirStatus		AuthenticateUsingCredentials	( LDAP *inLDAP, CLDAPReplicaInfo *inReplica, const char *inLDAPUsername,
														  const char *inKerberosID, const char *inPassword );
		tDirStatus		AuthenticateUsingKerberos		( LDAP *inLDAP, CLDAPReplicaInfo *inReplica, const char *inKerberosCache );
		bool			IsTokenNotATag					( char *inToken );

		bool			RetrieveServerMappings			( LDAP *inLDAP, CLDAPReplicaInfo *inReplica );
		bool			RetrieveServerReplicaList		( LDAP *inLDAP, CFMutableArrayRef outRepList, CFMutableArrayRef outWriteableList );
		bool			RetrieveServerSecuritySettings	( LDAP *inLDAP, CLDAPReplicaInfo *inReplica );
		void			RetrieveServerSchema			( LDAP *inLDAP );
	
		void			MergeArraysRemovingDuplicates	( CFMutableArrayRef cfPrimaryArray, CFArrayRef cfArrayToAdd );
		SInt32			GetReplicaListFromDNS			( CFMutableArrayRef inOutRepList );
		SInt32			GetReplicaListFromAltServer		( LDAP *inHost, CFMutableArrayRef inOutRepList );
	
		bool			GetSInt32FromDictionary			( CFDictionaryRef inDictionary, CFStringRef inKey, int32_t *outValue, int32_t defaultValue );
		bool			GetCStringFromDictionary		( CFDictionaryRef inDictionary, CFStringRef inKey, char **outValue );

		CFDictionaryRef	CreateNormalizedAttributeMap	( CFArrayRef inAttrMapArray, CFDictionaryRef inGlobalAttrMap );
		CFDictionaryRef	CreateNormalizedRecordAttrMap	( CFArrayRef inRecMapArray, CFArrayRef inGlobalAttrMapArray );
		int32_t			CalculateSecurityPolicy			( CFDictionaryRef inConfiguration );
		bool			GetUserTGTIfNecessaryAndStore	( const char *inName, const char *inPassword, char **outCacheName );
		void			BuildReplicaList				( void );

		static int		SASLInteract					( LDAP *ld, unsigned flags, void *inDefaults, void *inInteract );
};

#endif
