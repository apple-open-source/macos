/*
	File:		CLDAPv3Configs.h

	Contains:	Code to parse a XML file and place the contents into a table of structs

	Copyright:	© 2001 by Apple Computer, Inc., all rights reserved.

	NOT_FOR_OPEN_SOURCE <to be reevaluated at a later time>

*/


#ifndef __CLDAPv3Configs_h__
#define __CLDAPv3Configs_h__	1

#include <stdio.h>
#include <map>
#include <set>
#include <string>

#include <LDAP/lber.h>
#include <LDAP/ldap.h>

#include <CoreFoundation/CoreFoundation.h>		//for CF classes and property lists - XML config data

#include <DirectoryServiceCore/PrivateTypes.h>
#include <DirectoryServiceCore/CPlugInRef.h>			// config data table

using namespace std;

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

//string with pointer struct
typedef struct sPtrString {
	char		   *fName;				//string name
	sPtrString	   *fSubNative;			//separate list of strings used with the fName string
	int				fGroupSubNative;	//how to combine the SubNative strings
										//really for objectClass use to determine association
										//0 => AND whereas 1 => OR in the search filter
	sPtrString	   *pNext;				//next string
} sPtrString;

//Mapping strings tuple
typedef struct sMapTuple {
	char		   *fStandard;			//standard string
	sPtrString	   *fNative;			//native string(s)
	sMapTuple	   *pNext;				//next Map Tuple
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
} sLDAPConfigData;

class CLDAPv3Configs
{
public:
						CLDAPv3Configs		(	void );
	sInt32				Init				(	CPlugInRef *inConfigTable,
												uInt32 &inConfigTableLen,
												sMapTuple **inStdAttributeMapTuple,
												sMapTuple **inStdRecordMapTuple );
	virtual			   ~CLDAPv3Configs		(	void );
	sInt32				CleanLDAPConfigData (	sLDAPConfigData *inConfig );
	sInt32				SetXMLConfig		(	CFDataRef xmlData );
	CFDataRef			GetXMLConfig		(	void );
	sInt32				WriteXMLConfig		(	void );
	char			   *ExtractRecMap		(	char *inRecType,
												CFArrayRef inRecordTypeMapCFArray,
												int inIndex,
												bool *outOCGroup,
												CFArrayRef *outOCListCFArray,
												ber_int_t *outScope );
	char			   *ExtractAttrMap		(	char *inRecType,
												char *inAttrType,
												CFArrayRef inRecordTypeMapCFArray,
												CFArrayRef inAttrTypeMapCFArray,
												int inIndex );
	char			   *ExtractStdAttr		(	char *inRecType,
												CFArrayRef inRecordTypeMapCFArray,
												CFArrayRef inAttrTypeMapCFArray,
												int &inputIndex );
	int					AttrMapsCount		(	char *inRecType,
												char *inAttrType,
												CFArrayRef inRecordTypeMapCFArray,
												CFArrayRef inAttrTypeMapCFArray );
	void				ConfigDHCPObtainedLDAPServer
											(	char *inServer,
												char *inMapSearchBase,
												int inPortNumber,
												bool inIsSSL,
												uInt32 &inConfigTableLen );
	sInt32				WriteServerMappings (	char* userName,
												char* password,
												CFDataRef inMappings );
	CFDataRef			ReadServerMappings	(	LDAP *serverHost,
												CFDataRef inMappings );
	void				XMLConfigLock		(	void );
	void				XMLConfigUnlock		(	void );

protected:
	CFDataRef			RetrieveServerMappings
											(	char *inServer,
												char *inMapSearchBase,
												int inPortNumber,
												bool inIsSSL );
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
	sInt32				BuildDefaultStdAttributeMap
											(	void );
	sInt32				BuildDefaultStdRecordMap
											(	void );
	sInt32				CleanMapTuple		(	sMapTuple *inMapTuple );
	sLDAPConfigData	   *MakeLDAPConfigData	(	char *inName,
												char *inServerName,
												bool inUseStd,
												int inOpenCloseTO,
												int inSearchTO,
												int inPortNum,
												bool inUseSecure,
												char *inAccount,
												char *inPassword,
												bool inMakeDefLDAP,
												bool inServerMappings,
												bool inIsSSL );
	sInt32				ConfigLDAPServers	(	void );
	sInt32				AddDefaultLDAPServer( CFDataRef inXMLData );
	CFDataRef			VerifyAndUpdateServerLocation
											(	char *inServer,
												int inPortNumber,
												bool inIsSSL,
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
	sInt32				MakeLDAPConfig		(	CFDictionaryRef ldapDict,
												sInt32 inIndex );
	sInt32				BuildLDAPMap		(	sLDAPConfigData *inConfig,
												CFDictionaryRef ldapDict );
	sMapTuple		   *BuildMapTuple		(	CFArrayRef inArray );
	bool				CheckForConfig		(	char *inServerName,
												uInt32 &inConfigTableIndex);
	sInt32				ReadXMLConfig		(	void );
	sInt32				AddDefaultArrays	(	CFMutableDictionaryRef inDict );
	
private:
		CPlugInRef	   *pConfigTable;
        sMapTuple	   *pStdAttributeMapTuple;
        sMapTuple	   *pStdRecordMapTuple;
		uInt32			fConfigTableLen;
		CFDataRef		fXMLData;
		DSMutexSemaphore	*pXMLConfigLock;


};

#endif	// __CLDAPv3Configs_h__
