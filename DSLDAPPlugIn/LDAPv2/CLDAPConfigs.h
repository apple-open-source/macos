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
 * @header CLDAPConfigs
 * Code to parse a XML config file.
 */

#ifndef __CLDAPConfigs_h__
#define __CLDAPConfigs_h__	1

#include <stdio.h>

#include <CoreFoundation/CoreFoundation.h>		//for CF classes and property lists - XML config data

#include <DirectoryServiceCore/PrivateTypes.h>
#include <DirectoryServiceCore/CPlugInRef.h>			// config data table

//XML label tags
#define	kXMLLDAPVersionKey			"LDAP PlugIn Version"
#define kXMLConfigArrayKey			"LDAP Server Configs"

#define kXMLEnableUseFlagKey		"Enable Use"
#define kXMLUserDefinedNameKey		"UI Name"

#define kXMLOpenCloseTimeoutSecsKey	"OpenClose Timeout in seconds"
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
#define kXMLNativeMapArrayKey				"Native Map"
#define kXMLStdNameKey						"Standard Name"

//string with pointer struct
typedef struct sPtrString {
	char		   *fName;		//string name
	sPtrString	   *pNext;		//next string
} sPtrString;

//Mapping strings tuple
typedef struct sMapTuple {
	char		   *fStandard;	//standard string
	sPtrString	   *fNative;	//native string(s)
	sMapTuple	   *pNext;		//next Map Tuple
} sMapTuple;

//LDAPServer config data structure
typedef struct sLDAPConfigData {
	char			   *fName;				//LDAP defined name for the LDAP server
	sMapTuple		   *pAttributeMapTuple;	//Pointer to an attribute map
	char			   *fServerName;		//LDAP domain name ie. ldap.apple.com
	bool				bUseStdMapping;		//flag determining use of standard mappings
	sMapTuple		   *pRecordMapTuple;	//Pointer to a record map
	int					fOpenCloseTimeout;	//Open and Close timeout in seconds
	char			   *fServerPassword;	//LDAP server password
	int					fSearchTimeout;		//Search timeout in seconds
	char			   *fServerAccount;		//LDAP server account id
	int					fServerPort;		//LDAP server port ie. default is 389
	bool				bSecureUse;			//flag determing LDAP use with secure auth's
	bool				bAvail;				//flag determining whether the LDAP server
											//   connection is available
	bool				bUpdated;			//flag used to determine updates to the configuration
} sLDAPConfigData;

class CLDAPConfigs
{
public:
						CLDAPConfigs		( void );
	sInt32				Init				( CPlugInRef *inConfigTable,
											uInt32 &inConfigTableLen,
											sMapTuple **inStdAttributeMapTuple,
											sMapTuple **inStdRecordMapTuple );
	virtual			   ~CLDAPConfigs		( void );
	sInt32				CleanLDAPConfigData ( sLDAPConfigData *inConfig );
	sInt32				SetXMLConfig		( CFDataRef xmlData );
	CFDataRef			GetXMLConfig		( void );
	sInt32				WriteXMLConfig		( void );

protected:
	bool				VerifyXML ( void );
	sInt32				BuildDefaultStdAttributeMap
											( void );
	sInt32				BuildDefaultStdRecordMap
											( void );
	sInt32				CleanMapTuple		( sMapTuple *inMapTuple );
	sLDAPConfigData	   *MakeLDAPConfigData	( char *inName, char *inServerName, bool inUseStd,
											int inOpenCloseTO, int inSearchTO, int inPortNum,
											bool inUseSecure, char *inAccount, char *inPassword );
	sInt32				ConfigLDAPServers	( void );
	char			   *GetVersion			( CFDictionaryRef configDict );
	CFArrayRef			GetConfigArray		( CFDictionaryRef configDict );
	CFArrayRef			GetRecordTypeMapArray
											( CFDictionaryRef configDict );
	CFArrayRef			GetAttributeTypeMapArray
											( CFDictionaryRef configDict );
	CFArrayRef			GetNativeTypeMapArray
											( CFDictionaryRef configDict );
	CFArrayRef			GetDefaultRecordTypeMapArray
											( CFDictionaryRef configDict );
	CFArrayRef			GetDefaultAttrTypeMapArray
											( CFDictionaryRef configDict );
	sInt32				MakeLDAPConfig		( CFDictionaryRef ldapDict, sInt32 inIndex );
	sInt32				BuildLDAPMap		( sLDAPConfigData *inConfig, CFDictionaryRef ldapDict );
	sMapTuple		   *BuildMapTuple		( CFArrayRef inArray );
	bool				CheckForConfig		( char *inServerName, uInt32 &inConfigTableIndex);
	sInt32				ReadXMLConfig		( void );
	sInt32				AddDefaultArrays	( CFMutableDictionaryRef inDict );
	
private:
		CPlugInRef	   *pConfigTable;
        sMapTuple	   *pStdAttributeMapTuple;
        sMapTuple	   *pStdRecordMapTuple;
		uInt32			fConfigTableLen;
		CFDataRef		fXMLData;


};

#endif	// __CLDAPConfigs_h__
