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
 * @header CNiPlugIn
 */

#ifndef __CNiPlugIn_h__
#define __CNiPlugIn_h__		1

#include <map>			//STL map class
#include <string>		//STL string class
#include <netinfo/ni.h>

#include <PasswordServer/AuthFile.h>

#include "DirServicesTypes.h"
#include "DirServicesConst.h"
#include "PrivateTypes.h"
#include "PluginData.h"
#include "DSMutexSemaphore.h"

#include "libCdsaCrypt.h"

using namespace std;

typedef struct {
	double  lastTime;
	double  nowTime;
	uInt32  failCount;
} sHashAuthFailed;

typedef struct {
	sInt16 disabled;
	uInt16 failedLoginAttempts;
	uInt16 newPasswordRequired;
	struct tm creationDate;
	struct tm lastLoginDate;
	struct tm modDateOfPassword;
} sHashState;

typedef map<string, sHashAuthFailed*>	HashAuthFailedMap;
typedef HashAuthFailedMap::iterator		HashAuthFailedMapI;

class	CString;

// --------------------------------------------------------------------------------
//	Hash Length Constants
#define		kHashShadowChallengeLength			8
#define		kHashShadowKeyLength				21
#define		kHashShadowResponseLength			24
#define		kHashShadowOneLength				16
#define		kHashShadowBothLength				32
#define		kHashSecureLength					20
#define		kHashCramLength						16
#define		kHashTotalLength					68
#define		kHashShadowBothHexLength			64
#define		kHashOldHexLength					104
#define		kHashTotalHexLength					136


// -- enum's -------------------------------------

enum eNipThreadSig {
	kNiPlugInThreadSignatrue	=	'NiPi'
};

enum eBuffType {
	kRecrodListType		=	'RecL'
};


extern uInt32		gNodeRefID;

typedef struct {
	void	   *fDomain;
	char	   *fDomainName;
	char	   *fRecType;
	char	   *fRecName;
	ni_id		dirID;
	uInt32		offset;
	uInt32		index;
	bool		fDontUseSafeClose;
	uid_t		fUID;
	uid_t		fEffectiveUID;
	char*		fAuthenticatedUserName;
	bool		bIsLocal;
    
    tDirReference fPWSRef;
    tDirNodeReference fPWSNodeRef;
} sNIContextData;

typedef struct {
	uInt32				fAuthPass;
	uInt32				fLimitRecSearch;
	uInt32				fMultiMapIndex;
	uInt32				fRecNameIndex;
	uInt32				fRecTypeIndex;
	uInt32				fAllRecIndex;
	uInt32				fTotalRecCount;
	uInt32				fAttrIndex;
	tDataList		   *fAliasList;
	tDataList		   *fAliasAttribute;
	ni_entrylist		fNIEntryList;
	tDataBuffer		   *fDataBuff;
	void			   *fAuthHndl;
	void			   *fAuthHandlerProc;
	char			   *fAuthAuthorityData;
    tContextData		fPassPlugContinueData;
} sNIContinueData;

typedef sInt32 (*NetInfoAuthAuthorityHandlerProc) (	tDirNodeReference inNodeRef,
                                                    tDataNodePtr inAuthMethod,
                                                    sNIContextData* inContext,
                                                    sNIContinueData** inOutContinueData,
                                                    tDataBufferPtr inAuthData,
                                                    tDataBufferPtr outAuthData,
                                                    bool inAuthOnly,
													bool isSecondary,
                                                    char* inAuthAuthorityData,
													char* inGUIDString,
													const char* inNativeRecType );

class	CBuff;
class	CAttributeList;
class	CDataBuff;
class	CNodeRef;


class CNiPlugIn {
public:
				CNiPlugIn					( void );
	virtual	   ~CNiPlugIn					( void );

	sInt32		HandleRequest				( void *inData );
	static void	ContinueDeallocProc			( void *inContinueData );
	static void	ContextDeallocProc			( void* inContextData );
	static sInt32 DoBasicAuth				( tDirNodeReference inNodeRef,
											  tDataNodePtr inAuthMethod, 
											  sNIContextData* inContext, 
											  sNIContinueData** inOutContinueData, 
											  tDataBufferPtr inAuthData, 
											  tDataBufferPtr outAuthData, 
											  bool inAuthOnly,
											  bool isSecondary,
											  char* inAuthAuthorityData = NULL,
											  char* inGUIDString = NULL,
											  const char* inNativeRecType = "users" );
											  
	static sInt32 DoShadowHashAuth			( tDirNodeReference inNodeRef,
											  tDataNodePtr inAuthMethod, 
											  sNIContextData* inContext, 
											  sNIContinueData** inOutContinueData, 
											  tDataBufferPtr inAuthData, 
											  tDataBufferPtr outAuthData, 
											  bool inAuthOnly,
											  bool isSecondary, 
											  char* inAuthAuthorityData,
											  char* inGUIDString,
											  const char* inNativeRecType = "users",
											  bool inEnforcePolicy = true );
											  
	static sInt32 DoPasswordServerAuth		( tDirNodeReference inNodeRef,
											  tDataNodePtr inAuthMethod, 
											  sNIContextData* inContext, 
											  sNIContinueData** inOutContinueData, 
											  tDataBufferPtr inAuthData, 
											  tDataBufferPtr outAuthData, 
											  bool inAuthOnly,
											  bool isSecondary,
											  char* inAuthAuthorityData,
											  char* inGUIDString = NULL,
											  const char* inNativeRecType = "users" );
											  
	static sInt32 DoLocalCachedUserAuth		( tDirNodeReference inNodeRef,
											  tDataNodePtr inAuthMethod, 
											  sNIContextData* inContext, 
											  sNIContinueData** inOutContinueData, 
											  tDataBufferPtr inAuthData, 
											  tDataBufferPtr outAuthData, 
											  bool inAuthOnly,
											  bool isSecondary,
											  char* inAuthAuthorityData,
											  char* inGUIDString = NULL,
											  const char* inNativeRecType = "users" );
											  
	static sInt32 DoDisabledAuth			( tDirNodeReference inNodeRef,
											  tDataNodePtr inAuthMethod, 
											  sNIContextData* inContext, 
											  sNIContinueData** inOutContinueData, 
											  tDataBufferPtr inAuthData, 
											  tDataBufferPtr outAuthData, 
											  bool inAuthOnly,
											  bool isSecondary,
											  char* inAuthAuthorityData,
											  char* inGUIDString = NULL,
											  const char* inNativeRecType = "users" );

protected:
	sInt32		OpenDirNode						( sOpenDirNode *inData );
	sInt32		CloseDirNode					( sCloseDirNode *inData );

	sInt32		GetDirNodeInfo					( sGetDirNodeInfo *inData );
	sInt32		GetAttributeEntry				( sGetAttributeEntry *inData );
	sInt32		GetRecordList					( sGetRecordList *inData );
	sInt32		GetRecordEntry					( sGetRecordEntry *inData );
	sInt32		CreateRecord					( sCreateRecord *inData );
	sInt32		OpenRecord						( sOpenRecord *inData );
	sInt32		GetRecRefInfo					( sGetRecRefInfo *inData );
	sInt32		CloseRecord						( sCloseRecord *inData );
	sInt32		SetRecordName					( sSetRecordName *inData );
	sInt32		SetRecordType					( sSetRecordType *inData );
	sInt32		DeleteRecord					( sDeleteRecord *inData );
	sInt32		AddAttribute					( sAddAttribute *inData );
	sInt32		GetAttributeValue				( sGetAttributeValue *inData );
	sInt32		GetRecAttribInfo				( sGetRecAttribInfo *inData );
	sInt32		GetRecordAttributeValueByID		( sGetRecordAttributeValueByID *inData );
	sInt32		GetRecAttrValueByIndex			( sGetRecordAttributeValueByIndex *inData );
	sInt32		DoAuthentication				( sDoDirNodeAuth *inData );
	sInt32		DoAuthenticationOnRecordType	( sDoDirNodeAuthOnRecordType *inData );
	static sInt32		PasswordOkForPolicies   ( const char *inSpaceDelimitedPolicies, PWGlobalAccessFeatures *inGAccess, const char *inUsername, const char *inPassword );
	static sInt32		TestPolicies			( const char *inSpaceDelimitedPolicies, PWGlobalAccessFeatures *inGAccess, sHashState *inOutHashState, struct timespec *inModDateOfPassword, const char *inHashPath );
	static sInt32		GetShadowHashGlobalPolicies( sNIContextData *inContext, PWGlobalAccessFeatures *inOutGAccess );
	static sInt32		SetShadowHashGlobalPolicies( sNIContextData *inContext, PWGlobalAccessFeatures *inGAccess );
	static sInt32		GetStateFilePath		( const char *inHashPath, char **outStateFilePath );
	static int			ReadHashStateFile		( const char *inFilePath, sHashState *inOutHashState );
	static int			WriteHashStateFile		( const char *inFilePath, sHashState *inHashState );
	static sInt32		TestDisabledStatus		( PWAccessFeatures *inAccess, PWGlobalAccessFeatures *inGAccess, UInt32 inFailedLoginAttempts, struct tm *inCreationDate, struct tm *inLastLoginTime );
	sInt32		DoAttributeValueSearch			( sDoAttrValueSearchWithData *inData );
	sInt32		DoPlugInCustomCall				( sDoPlugInCustomCall *inData );
	sInt32		ReleaseContinueData				( sReleaseContinueData *inData );
	sInt32		RemoveAttribute					( sRemoveAttribute *inData );
	sInt32		AddAttributeValue				( sAddAttributeValue *inData );
	sInt32		RemoveAttributeValue			( sRemoveAttributeValue *inData );
	sInt32		SetAttributeValue				( sSetAttributeValue *inData );
	sInt32		CloseAttributeList				( sCloseAttributeList *inData );
	sInt32		CloseAttributeValueList			( sCloseAttributeValueList *inData );

	sInt32		GetAllRecords				(	char *inNativeRecType,
												CAttributeList *inAttrTypeList,
												sNIContinueData *inContinue,
												void *inDomain,
												char *inDomainName,
												bool inAttrOnly,
												CBuff *inBuff,
												uInt32 &outRecCount );
	sInt32		GetTheseRecords				(	char *inConstRecName,
												char *inConstRecType,
												char *inNativeRecType,
												tDirPatternMatch inPattMatch,
												CAttributeList *inAttrTypeList,
												void *inDomain,
												char *inDomainName,
												bool inAttrOnly,
												CBuff *inBuff,
												sNIContinueData *inContinue,
												uInt32 &outRecCount );
	sInt32		GetTheseAttributes			(	CAttributeList *inAttrTypeList,
												ni_proplist *inPropList,
												bool inAttrOnly,
												void *inDomain,
												char *inDomainName,
												sInt32 &outCnt );
	sInt32		FindAllRecords				(	const char *inNI_RecType,
												const char *inDS_RecType,
												const char *inPatt2Match,
												tDirPatternMatch inHow,
												CAttributeList *inAttrTypeList,
												bool inAttrInfoOnly,
												sNIContinueData *inContext,
												void *inDomain,
												char *inDomainName,
												CBuff *inBuff,
												uInt32 &outRecCount );
	sInt32		FindTheseRecords   			(	const char *inNI_RecType,
												const char *inDS_RecType,
												const char *inAttrType,
												const char *inPatt2Match,
												tDirPatternMatch inHow,
												CAttributeList *inAttrTypeList,
												bool inAttrInfoOnly,
												sNIContinueData *inContext,
												void *inDomain,
												char *inDomainName,
												CBuff *inBuff,
												uInt32 &outRecCount );
	tDataList*  FindNodeForSearchPolicyAuthUser
											(   const char *userName );

private:
	sInt32		DoCreateRecord				( void *inDomain, ni_id *inDir, char *inPathName );
	ni_status	DoCreateChild				( void *inDomain, ni_id *inDir, const ni_name inDirName );
	static sInt32   DoAddAttribute			( void *domain, ni_id *dir, const ni_name key, ni_namelist values );
	static sInt32	MigrateToShadowHash		( void *inDomain, ni_id *inNIDirID, ni_proplist *inNIPropList, const char *inUserName, const char *inPassword );
	
	static bool		IsWriteAuthRequest		(   uInt32 uiAuthMethod );
	static void		GenerateShadowHashes	(   const char *inPassword,
												long inPasswordLen,
												bool inUseBothHashes,
												unsigned char *outHashes,
												unsigned long *outHashTotalLength );
	static sInt32	GetUserPolicies			(   sNIContextData *inContext,
												const char *inNativeRecType,
												const char *inUsername,
												char **outPolicyStr );
	
	static sInt32   SetUserPolicies			(   sNIContextData *inContext,
												const char *inNativeRecType,
												const char *inUsername,
												const char *inPolicyStr );
	
	static sInt32   SetUserAAtoDisabled		(   sNIContextData *inContext,
												const char *inNativeRecType,
												const char *inUsername );
	
	static sInt32	ReadShadowHash			( const char *inUserName, char *inGUIDString, unsigned char outHashes[kHashTotalLength], struct timespec *outModTime = NULL, char **outUserHashPath = NULL );
	static sInt32	WriteShadowHash			( const char *inUserName, char *inGUIDString, unsigned char inHashes[kHashTotalLength] );
	static void		RemoveShadowHash		( const char *inUserName, char *inGUIDString, bool bShadowToo );
	static sInt32   ReadShadowHashAndStateFiles( const char *inUserName, char *inGUIDString, unsigned char outHashes[kHashTotalLength], struct timespec *outModTime, char **outUserHashPath, char **outStateFilePath, sHashState *inOutHashState );
	
    static sInt32	RepackBufferForPWServer	(	tDataBufferPtr inBuff,
                                                const char *inUserID,
                                                unsigned long inUserIDNodeNum,
                                                tDataBufferPtr *outBuff );
    
    static sInt32	PWOpenDirNode			( tDirNodeReference fDSRef, char *inNodeName, tDirNodeReference *outNodeRef );
    static sInt32	PWSetReplicaData		( sNIContextData *inContext, const char *inAuthorityData );
	
	static sInt32	DoSetPassword			( sNIContextData *inContext, tDataBuffer *inAuthData, const char *inNativeRecType );
	static sInt32	DoSetPasswordAsRoot		( sNIContextData *inContext, tDataBuffer *inAuthData, const char *inNativeRecType );
	static sInt32	DoChangePassword		( sNIContextData *inContext, tDataBuffer *inAuthData, bool isSecondary, const char *inNativeRecType );
	static sInt32   Get2FromBuffer			( tDataBufferPtr inAuthData, tDataList **inOutDataList, char **inOutItemOne, char **inOutItemTwo, unsigned int *outItemCount );
	static sInt32	DoNodeNativeAuth		( sNIContextData *inContext, tDataBuffer *inAuthData, bool inAuthOnly, const char *inNativeRecType );
	static sInt32	DoUnixCryptAuth			( sNIContextData *inContext, tDataBuffer *inAuthData, bool inAuthOnly, const char *inNativeRecType );
	static sInt32	DoTimSMBAuth			( sNIContextData *inContext, tDataBuffer *inAuthData, uInt32 inWhichOne );
	static sInt32	DoTimMultiPassAuth		( tDirNodeReference inNodeRef, tDataNodePtr inAuthMethod,
											  sNIContextData *inContext, 
											  sNIContinueData** inOutContinueData, 
											  tDataBufferPtr inAuthData, tDataBufferPtr outAuthData, 
											  bool inAuthOnly );
	static sInt32	ValidateDigest			( sNIContextData *inContext, tDataBuffer *inAuthData, uInt32 inAuthMethod );
	static sInt32	AuthOpen				( sNIContextData *inContext, const char * inUserName, 
											  const char * inPassword, bool bIsEffectiveRoot = false );
	sInt32		VerifyPatternMatch			( const tDirPatternMatch inPatternMatch );

	static sInt32	IsValidRecordName		( const char *inRecName, const char *inRecType, 
											  void *inDomain, ni_id &outDirID );
	static sInt32	IsValidRealname			( char *inRealname, void *inDomain, char **outRecName );
	static bool		UserIsAdmin				( const char *inUserName, void *inDomain );
	static char*	GetUserNameForUID 		( uid_t inUserID, void *inDomain );
	static sInt32	GetUserNameFromAuthBuffer	( tDataBufferPtr inAuthData, unsigned long inUserNameIndex, char **outUserName );
	static sInt32	ParseLocalCacheUserData (	const char *inAuthData,
												char **outNodeName,
												char **outRecordName,
												char **outGUID );

	NetInfoAuthAuthorityHandlerProc GetNetInfoAuthAuthorityHandler ( const char* inTag );
	static char*	BuildRecordNamePath		( const char *inRecName, const char *inRecType );
	static sInt32	GetAuthMethod			( tDataNode *inData, uInt32 *outAuthMethod );

	static char*	MapRecToNetInfoType		( const char *inRecType );
	char*		MapNetInfoRecToDSType		( const char *inRecType );
	static char*	MapAttrToNetInfoType	( const char *inAttrType );
	static char*	MapNetInfoAttrToDSType	( const char *inAttrType );

	static CString*	GetAuthTypeStr			( const char *inNativeAuthStr );
	static CString*	GetAuthString			( tDataNode *inData );

	static bool		IsTimRunning			( void );
	static sInt32	MapNetInfoErrors		( sInt32 inNiError );
	static sInt32	MapAuthResult			( sInt32 inAuthResult );
	uInt32		CalcCRC						( const char *inStr );

	static bool		DoesThisMatch			( const char *inString, const char *inPatt, tDirPatternMatch inHow );
	static char*	BuildRegExp				( const char *inString );

	void		MakeGood					( char *inStr, char *outStr );

	sNIContextData*	MakeContextData			( void );
    static sInt32	CleanContextData		( sNIContextData *inContext );
	static char* 	BuildDomainPathFromName	( char* inDomainName );

private:
	//KW have been extremely lucky up until now using these member vars in thread capable class
	//ie. the netinfo mutex has helped us do this in all but one function ie. GetRecRefInfo fixed now
	CDataBuff	   *fRecData;
	CDataBuff	   *fAttrData;
	CDataBuff	   *fTmpData;
	
};

#endif
