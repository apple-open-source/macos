/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

using namespace std;

//XML label tags
#define	kXMLLDAPVersionKey			"LDAP PlugIn Version"
#define kXMLConfigArrayKey			"LDAP Server Configs"
#define kXMLDHCPConfigArrayKey		"LDAP DHCP Server Configs"
#define kXMLServerConfigKey			"LDAP Server Config"

#define kXMLEnableUseFlagKey		"Enable Use"
#define kXMLUserDefinedNameKey		"UI Name"

#define kXMLOpenCloseTimeoutSecsKey	"OpenClose Timeout in seconds"
#define kXMLIdleTimeoutMinsKey		"Idle Timeout in minutes"
#define kXMLDelayedRebindTrySecsKey	"Delay Rebind Try in seconds"
#define kXMLPortNumberKey			"Port Number"
#define kXMLSearchTimeoutSecsKey	"Search Timeout in seconds"
#define kXMLSecureUseFlagKey		"Secure Use"
#define kXMLServerKey				"Server"
#define kXMLServerAccountKey		"Server Account"
#define kXMLServerPasswordKey		"Server Password"

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
#define kXMLMapSearchBase					"Map Search Base"
//kXMLMakeDefLDAPFlagKey => indicates the server config is a Default LDAP Node
//kXMLServerMappingsFlagKey => indicates the server config comes directly from the LDAP
//                                server itself and can only be written with authentication

#define kLDAPDefaultOpenCloseTimeoutInSeconds   15
#define kLDAPDefaultSearchTimeoutInSeconds		120

typedef list<string>					listOfStrings;
typedef listOfStrings::const_iterator	listOfStringsCI;

typedef set<string>						AttrSet;
typedef AttrSet::const_iterator			AttrSetCI;

typedef struct sObjectClassSchema {
	AttrSet			fParentOCs;			//hierarchy parents
	AttrSet			fOtherNames;		//other names of same OC
	AttrSet			fRequiredAttrs;		//required attributes
	AttrSet			fAllowedAttrs;		//allowed other attributes
	uInt16			fType;				//0=>Abstract, 1=>Structural, 2=>Auxiliary
	uInt32			fDummy;
} sObjectClassSchema;

typedef map<string,sObjectClassSchema*>		ObjectClassMap;
typedef ObjectClassMap::const_iterator		ObjectClassMapCI;

typedef struct sReplicaInfo {
	struct addrinfo	   *fAddrInfo;		//addrinfo struct
	bool				bWriteable;		//host is writable with proper authentication
	bool				bUsedLast;		//host was last used replica
	CFStringRef			hostname;		//hostname from config or replica list
	sReplicaInfo	   *fNext;			//next struct in linked list
} sReplicaInfo;

//LDAPServer config data structure
typedef struct sLDAPConfigData {
	char			   *fName;				//LDAP defined name for the LDAP server
	char			   *fServerName;		//LDAP domain name ie. ldap.apple.com
	sReplicaInfo	   *fReplicaHosts;		//list of LDAP replica hosts
	CFMutableArrayRef	fReplicaHostnames;	//list of all LDAP replica hostnames
	CFMutableArrayRef	fWriteableHostnames;//list of writeable LDAP replica hostnames
	bool				bBuildReplicaList;	//set to indicate the LDAP Replica list needs to be built
	int					fOpenCloseTimeout;	//Open and Close timeout in seconds
	int					fIdleTimeout;		//Idle timeout in minutes - NOT USED if set to ZERO
	int					fDelayRebindTry;	//Delay rebind try after bind failure in seconds
	char			   *fServerPassword;	//LDAP server password
	int					fSearchTimeout;		//Search timeout in seconds
	char			   *fServerAccount;		//LDAP server account id
	int					fServerPort;		//LDAP server port ie. default is 389 - SSL default port is 636
	bool				bSecureUse;			//flag determing LDAP use with secure auth's
	bool				bAvail;				//flag determining whether the LDAP server
											//   connection is available
	bool				bUpdated;			//flag used to determine updates to the configuration
	ObjectClassMap	   *fObjectClassSchema;	//dictionary of object class schema
	bool				bOCBuilt;			//flag to be set when OC Schema build already attempted
	CFArrayRef			fRecordTypeMapCFArray;
	CFArrayRef			fAttrTypeMapCFArray;
	bool				bUseAsDefaultLDAP;	//this node will be used as one of the default
											//   LDAP nodes in the search policy
	bool				bServerMappings;	//whether mappings are ldap server provided or not
	bool				bIsSSL;				//if SSL connections used this is set
    char			   *fMapSearchBase;		
	bool				bGetServerMappings;	//set to indicate whether server mappings need to be retrieved
	CFMutableArrayRef   fSASLmethods;
} sLDAPConfigData;

class CLDAPv3Configs
{
public:
						CLDAPv3Configs		(	void );
	sInt32				Init				(	CPlugInRef *inConfigTable,
												uInt32 &inConfigTableLen );
	virtual			   ~CLDAPv3Configs		(	void );
	sInt32				CleanLDAPConfigData (	sLDAPConfigData *inConfig,
												bool inServerMappings = false);
	sInt32				AddToConfig			(	CFDataRef xmlData );
	sInt32				SetXMLConfig		(	CFDataRef xmlData );
	CFDataRef			CopyXMLConfig		(	void );
	sInt32				WriteXMLConfig		(	void );
	char			   *ExtractRecMap		(	const char *inRecType,
												CFArrayRef inRecordTypeMapCFArray,
												int inIndex,
												bool *outOCGroup,
												CFArrayRef *outOCListCFArray,
												ber_int_t *outScope );
	char			   *ExtractAttrMap		(	const char *inRecType,
												const char *inAttrType,
												CFArrayRef inRecordTypeMapCFArray,
												CFArrayRef inAttrTypeMapCFArray,
												int inIndex );
	char			   *ExtractStdAttr		(	char *inRecType,
												CFArrayRef inRecordTypeMapCFArray,
												CFArrayRef inAttrTypeMapCFArray,
												int &inputIndex );
	int					AttrMapsCount		(	const char *inRecType,
												const char *inAttrType,
												CFArrayRef inRecordTypeMapCFArray,
												CFArrayRef inAttrTypeMapCFArray );
	sInt32				UpdateLDAPConfigWithServerMappings
											(	char *inServer,
												char *inMapSearchBase,
												int inPortNumber,
												bool inIsSSL,
												bool inMakeDefLDAP,
												LDAP *inServerHost = nil );
    sInt32				MakeServerBasedMappingsLDAPConfig
                                            (	char *inServer,
                                                char *inMapSearchBase,
                                                int inOpenCloseTO,
												int inIdleTO,
												int inDelayRebindTry,
												int inSearchTO,
												int inPortNumber,
                                                bool inIsSSL,
                                                bool inMakeDefLDAP );
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

protected:
	CFDataRef			RetrieveServerMappings
											(	char *inServer,
												char *inMapSearchBase,
												int inPortNumber,
												bool inIsSSL,
												LDAP *inServerHost = nil );
	CFDictionaryRef		CheckForServerMappings
											(	CFDictionaryRef ldapDict );
	char			   *ExtractAttrMapFromArray
											(	CFStringRef inAttrTypeRef,
												CFArrayRef inAttrTypeMapCFArray,
												int inIndex,
												bool *bNoRecSpecificAttrMap );
	int					AttrMapFromArrayCount
											(	CFStringRef inAttrTypeRef,
												CFArrayRef inAttrTypeMapCFArray,
												bool *bNoRecSpecificAttrMap );
	bool				VerifyXML			(	void );
	sLDAPConfigData	   *MakeLDAPConfigData	(	char *inName,
												char *inServerName,
												int inOpenCloseTO,
												int inIdleTO,
												int inDelayRebindTry,
												int inSearchTO,
												int inPortNum,
												bool inUseSecure,
												char *inAccount,
												char *inPassword,
												bool inMakeDefLDAP,
												bool inServerMappings,
												bool inIsSSL,
                                                char *inMapSearchBase,
												sLDAPConfigData *inLDAPConfigData = nil );
	sInt32				ConfigLDAPServers	(	void );
	sInt32				AddLDAPServer		( CFDataRef inXMLData );
	CFDataRef			VerifyAndUpdateServerLocation
											(	char *inServer,
												int inPortNumber,
												bool inIsSSL,
												bool inMakeDefLDAP,
												CFDataRef inXMLData );
	char			   *GetVersion			(	CFDictionaryRef configDict );
	CFArrayRef			GetConfigArray		(	CFDictionaryRef configDict );
	CFArrayRef			GetRecordTypeMapArray
											(	CFDictionaryRef configDict );
	CFArrayRef			GetAttributeTypeMapArray
											(	CFDictionaryRef configDict );
	CFArrayRef			GetNativeTypeMapArray
											(	CFDictionaryRef configDict );
	CFArrayRef			GetDefaultRecordTypeMapArray
											(	CFDictionaryRef configDict );
	CFArrayRef			GetDefaultAttrTypeMapArray
											(	CFDictionaryRef configDict );
	CFArrayRef			GetReplicaHostnameListArray 
											( CFDictionaryRef configDict );
	CFArrayRef			GetWriteableHostnameListArray 
											( CFDictionaryRef configDict );
	sInt32				MakeLDAPConfig		(	CFDictionaryRef ldapDict,
												sInt32 inIndex,
												bool inEnsureServerMappings = false );
	sInt32				BuildLDAPMap		(	sLDAPConfigData *inConfig,
												CFDictionaryRef ldapDict,
												bool inServerMapppings );
	bool				CheckForConfig		(	char *inServerName,
												uInt32 &inConfigTableIndex);
	sInt32				ReadXMLConfig		(	void );
	bool				ConvertLDAPv2Config (   void );
	bool				CreatePrefDirectory (   void );
	
private:
		CPlugInRef	   *pConfigTable;
		uInt32			fConfigTableLen;
		CFDataRef		fXMLData;
		DSMutexSemaphore	*pXMLConfigLock;


};

#endif	// __CLDAPv3Configs_h__
