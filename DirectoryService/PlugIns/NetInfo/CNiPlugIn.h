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
#include <set>			//STL set class
#include <string>		//STL string class
#include <netinfo/ni.h>
#include <openssl/aes.h>

#include <PasswordServer/AuthFile.h>

#include "DirServicesTypes.h"
#include "DirServicesConst.h"
#include "PrivateTypes.h"
#include "PluginData.h"
#include "DSMutexSemaphore.h"

#include "libCdsaCrypt.h"
#include "digestmd5.h"

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
#define		kHashCramLength						32
#define		kHashSaltedSHA1Length				24
#define		kHashRecoverableLength				512

#define		kHashOffsetToNT						(0)
#define		kHashOffsetToLM						(kHashShadowOneLength)
#define		kHashOffsetToSHA1					(kHashOffsetToLM + kHashShadowOneLength)
#define		kHashOffsetToCramMD5				(kHashShadowBothLength + kHashSecureLength)
#define		kHashOffsetToSaltedSHA1				(kHashOffsetToCramMD5 + kHashCramLength)
#define		kHashOffsetToRecoverable			(kHashOffsetToSaltedSHA1 + kHashSaltedSHA1Length)

#define		kHashTotalLength					(kHashShadowBothLength + kHashSecureLength + \
												 kHashCramLength + kHashSaltedSHA1Length + \
												 kHashRecoverableLength)
#define		kHashShadowBothHexLength			64
#define		kHashOldHexLength					104
#define		kHashTotalHexLength					(kHashTotalLength * 2)

// --------------------------------------------------------------------------------
//	More Hash Defines
#define		kAESVector							"qawe ptajilja;sdqawe ptajilja;sd"
#define		kShadowHashDirPath					"/var/db/shadow/hash/"
#define		kShadowHashOldDirPath				"/var/db/samba/hash/"
#define		kShadowHashStateFileSuffix			".state"
#define		kShadowHashRecordName				"shadowhash"
#define		kShadowHashNTLMv2Length				16
#define		kLocalCachedUserHashList			"HASHLIST:<SALTED-SHA1>"

// -- enum's -------------------------------------

enum eNipThreadSig {
	kNiPlugInThreadSignatrue	=	'NiPi'
};

enum eBuffType {
	kRecordListType		=	'RecL'
};

enum {
	kNiPluginHashLM							= 0x0001,
	kNiPluginHashNT							= 0x0002,
	kNiPluginHashSHA1						= 0x0004,		/* deprecated */
	kNiPluginHashCRAM_MD5					= 0x0008,
	kNiPluginHashSaltedSHA1					= 0x0010,
	kNiPluginHashRecoverable				= 0x0020,
	kNiPluginHashSecurityTeamFavorite		= 0x0040,
	
	kNiPluginHashDefaultSet					= kNiPluginHashSaltedSHA1,
	kNiPluginHashWindowsSet					= kNiPluginHashNT | kNiPluginHashSaltedSHA1 | kNiPluginHashLM,
	kNiPluginHashDefaultServerSet			= kNiPluginHashNT | kNiPluginHashSaltedSHA1 | kNiPluginHashLM | kNiPluginHashCRAM_MD5 | kNiPluginHashRecoverable,
	kNiPluginHashHasReadConfig				= 0x8000
};

extern uInt32		gNodeRefID;

typedef struct {
	void	   *fDomain;					//netinfo domain handle ONLY used for authenticated open
	char	   *fDomainName;				//nodename coming back from SafeOpen and used for MetaNodeLocation
	char	   *fDomainPath;				//domain path that is built used by SafeOpen and RetrieveNIDomain
	char	   *fRecType;					//record type
	char	   *fRecName;					//record name
	ni_id		dirID;						//used for an open record
	uInt32		offset;						//buffer parsing offset
	bool		fDontUseSafeClose;			//clean up authenticated ni domains separately
	uid_t		fUID;
	uid_t		fEffectiveUID;
	char*		fAuthenticatedUserName;		//record name used for authenticated connection
	bool		bIsLocal;					//defining a local domain
	bool		bDidWriteOperation;			//defining a write operation was carried out
    
    tDirReference fPWSRef;
    tDirNodeReference fPWSNodeRef;
	
    tDirReference fLocalCacheRef;
    tDataList *fLocalCacheNetNode;
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
                                                    const char* inAuthAuthorityData,
													const char* inGUIDString,
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
											  const char* inAuthAuthorityData = NULL,
											  const char* inGUIDString = NULL,
											  const char* inNativeRecType = "users" );
											  
	static sInt32 DoShadowHashAuth			( tDirNodeReference inNodeRef,
											  tDataNodePtr inAuthMethod, 
											  sNIContextData* inContext, 
											  sNIContinueData** inOutContinueData, 
											  tDataBufferPtr inAuthData, 
											  tDataBufferPtr outAuthData, 
											  bool inAuthOnly,
											  bool isSecondary, 
											  const char* inAuthAuthorityData,
											  const char* inGUIDString,
											  const char* inNativeRecType = "users" );
											  
	static sInt32 DoPasswordServerAuth		( tDirNodeReference inNodeRef,
											  tDataNodePtr inAuthMethod, 
											  sNIContextData* inContext, 
											  sNIContinueData** inOutContinueData, 
											  tDataBufferPtr inAuthData, 
											  tDataBufferPtr outAuthData, 
											  bool inAuthOnly,
											  bool isSecondary,
											  const char* inAuthAuthorityData,
											  const char* inGUIDString = NULL,
											  const char* inNativeRecType = "users" );
											  
	static sInt32 DoLocalCachedUserAuth		( tDirNodeReference inNodeRef,
											  tDataNodePtr inAuthMethod, 
											  sNIContextData* inContext, 
											  sNIContinueData** inOutContinueData, 
											  tDataBufferPtr inAuthData, 
											  tDataBufferPtr outAuthData, 
											  bool inAuthOnly,
											  bool isSecondary,
											  const char* inAuthAuthorityData,
											  const char* inGUIDString = NULL,
											  const char* inNativeRecType = "users" );
											  
	static sInt32 DoDisabledAuth			( tDirNodeReference inNodeRef,
											  tDataNodePtr inAuthMethod, 
											  sNIContextData* inContext, 
											  sNIContinueData** inOutContinueData, 
											  tDataBufferPtr inAuthData, 
											  tDataBufferPtr outAuthData, 
											  bool inAuthOnly,
											  bool isSecondary,
											  const char* inAuthAuthorityData,
											  const char* inGUIDString = NULL,
											  const char* inNativeRecType = "users" );

protected:
	sInt32		OpenDirNode					( sOpenDirNode *inData );
	sInt32		CloseDirNode				( sCloseDirNode *inData );

	sInt32		GetDirNodeInfo				( sGetDirNodeInfo *inData );
	sInt32		GetAttributeEntry			( sGetAttributeEntry *inData );
	sInt32		GetRecordList				( sGetRecordList *inData );
	sInt32		GetRecordEntry				( sGetRecordEntry *inData );
	sInt32		CreateRecord				( sCreateRecord *inData );
	sInt32		OpenRecord					( sOpenRecord *inData );
	sInt32		GetRecRefInfo				( sGetRecRefInfo *inData );
	sInt32		CloseRecord					( sCloseRecord *inData );
	sInt32		SetRecordName				( sSetRecordName *inData );
	sInt32		SetRecordType				( sSetRecordType *inData );
	sInt32		DeleteRecord				( sDeleteRecord *inData );
	sInt32		AddAttribute				( sAddAttribute *inData );
	sInt32		GetAttributeValue			( sGetAttributeValue *inData );
	sInt32		GetRecAttribInfo			( sGetRecAttribInfo *inData );
	sInt32		GetRecordAttributeValueByID	( sGetRecordAttributeValueByID *inData );
	sInt32		GetRecAttrValueByIndex		( sGetRecordAttributeValueByIndex *inData );
	sInt32		GetRecAttrValueByValue		( sGetRecordAttributeValueByValue *inData );
	sInt32		DoAuthentication			( sDoDirNodeAuth *inData );
	sInt32		DoAuthenticationOnRecordType	( sDoDirNodeAuthOnRecordType *inData );
	sInt32		RecordHasAuthAuthority		( const char *inRecordName, const char *inRecType, sNIContextData *inContext, const char *inTag, ni_id *outDirID );
	
	sInt32		DoAttributeValueSearch		( sDoAttrValueSearchWithData *inData );
	sInt32		DoMultipleAttributeValueSearch		( sDoMultiAttrValueSearchWithData *inData );
	sInt32		DoPlugInCustomCall			( sDoPlugInCustomCall *inData );
	sInt32		ReleaseContinueData			( sReleaseContinueData *inData );
	sInt32		RemoveAttribute				( sRemoveAttribute *inData );
	sInt32		AddAttributeValue			( sAddAttributeValue *inData );
	sInt32		RemoveAttributeValue		( sRemoveAttributeValue *inData );
	sInt32		SetAttributeValue			( sSetAttributeValue *inData );
	sInt32		SetAttributeValues			( sSetAttributeValues *inData );
	sInt32		CloseAttributeList			( sCloseAttributeList *inData );
	sInt32		CloseAttributeValueList		( sCloseAttributeValueList *inData );

	sInt32		GetAllRecords				(	const char *inRecType,
												char *inNativeRecType,
												CAttributeList *inAttrTypeList,
												sNIContinueData *inContinue,
												sNIContextData *inContext,
												bool inAttrOnly,
												CBuff *inBuff,
												uInt32 &outRecCount );
	sInt32		GetTheseRecords				(	char *inConstRecName,
												const char *inConstRecType,
												char *inNativeRecType,
												tDirPatternMatch inPattMatch,
												CAttributeList *inAttrTypeList,
												sNIContextData *inContext,
												bool inAttrOnly,
												CBuff *inBuff,
												sNIContinueData *inContinue,
												uInt32 &outRecCount );
	sInt32		GetTheseAttributes			(	CAttributeList *inAttrTypeList,
												ni_proplist *inPropList,
												bool inAttrOnly,
												void *inDomain,
												char *inDomainName,
												const char *inRecType,
												sInt32 &outCnt );
	sInt32		FindAllRecords				(	const char *inNI_RecType,
												const char *inDS_RecType,
												CAttributeList *inPattMatchList,
												tDirPatternMatch inHow,
												CAttributeList *inAttrTypeList,
												bool inAttrInfoOnly,
												sNIContinueData *inContext,
												sNIContextData *inContext,
												CBuff *inBuff,
												uInt32 &outRecCount );
	sInt32		FindTheseRecords   			(	const char *inNI_RecType,
												const char *inDS_RecType,
												const char *inAttrType,
												CAttributeList *inPattMatchList,
												tDirPatternMatch inHow,
												CAttributeList *inAttrTypeList,
												bool inAttrInfoOnly,
												sNIContinueData *inContext,
												sNIContextData *inContext,
												CBuff *inBuff,
												uInt32 &outRecCount );
	tDataList*  FindNodeForSearchPolicyAuthUser
											(   const char *userName );

private:
	sInt32			DoCreateRecord				( void *inDomain, ni_id *inDir, char *inPathName );
	ni_status		DoCreateChild				( void *inDomain, ni_id *inDir, const ni_name inDirName );
	static sInt32   DoAddAttribute				( void *domain, ni_id *dir, const ni_name key, ni_namelist values );
	static sInt32	MigrateToShadowHash			( void *inDomain, ni_id *inNIDirID, ni_proplist *inNIPropList, const char *inUserName, const char *inPassword, bool &outResetCache );
	
	static bool		IsWriteAuthRequest			(   uInt32 uiAuthMethod );
	
	static sInt32	LocalCachedUserReachable	( tDirNodeReference inNodeRef,
												  tDataNodePtr inAuthMethod, 
												  sNIContextData* inContext, 
												  sNIContinueData** inOutContinueData, 
												  tDataBufferPtr inAuthData, 
												  tDataBufferPtr outAuthData,
												  const char* inAuthAuthorityData,
												  const char* inGUIDString,
												  const char* inNativeRecType,
												  bool *inOutNodeReachable );
	
	static sInt32 DoLocalCachedUserAuthPhase2	( tDirNodeReference inNodeRef,
												  tDataNodePtr inAuthMethod, 
												  sNIContextData* inContext, 
												  sNIContinueData** inOutContinueData, 
												  tDataBufferPtr inAuthData, 
												  tDataBufferPtr outAuthData, 
												  bool inAuthOnly,
												  bool isSecondary,
												  const char* inAuthAuthorityData,
												  const char* inGUIDString,
												  const char* inNativeRecType,
												  bool inNetNodeReachable );
											  
	static sInt32	GetUserPolicies			(   sNIContextData *inContext,
												const char *inNativeRecType,
												const char *inUsername,
												sHashState *inState,
												char **outPolicyStr );
	
	static sInt32   SetUserPolicies			(   sNIContextData *inContext,
												const char *inNativeRecType,
												const char *inUsername,
												const char *inPolicyStr,
												sHashState *inOutHashState );
	
	static sInt32   SetUserAAtoDisabled		(   sNIContextData *inContext,
												const char *inNativeRecType,
												const char *inUsername );
	
	static sInt32	SetUserAuthAuthorityAsRoot
											(	sNIContextData *inContext,
												const char *inNativeRecType,
												const char *inUsername,
												const char *inAuthAuthority );
												
	static sInt32	ReadShadowHash			(	const char *inUserName,
												const char *inGUIDString,
												unsigned char outHashes[kHashTotalLength],
												struct timespec *outModTime,
												char **outUserHashPath,
												sInt32 *outHashDataLen,
												bool readHashes = true );
											
	static sInt32	WriteShadowHash			( const char *inUserName, const char *inGUIDString, unsigned char inHashes[kHashTotalLength] );
	static void		RemoveShadowHash		( const char *inUserName, const char *inGUIDString, bool bShadowToo );
	
	static sInt32   ReadShadowHashAndStateFiles(	const char *inUserName,
													const char *inGUIDString,
													unsigned char outHashes[kHashTotalLength],
													struct timespec *outModTime,
													char **outUserHashPath,
													char **outStateFilePath,
													sHashState *inOutHashState,
													sInt32 *outHashDataLen = NULL );
	
	static sInt32	ReadStateFile			(	const char *inUserName,
												const char *inGUIDString,
												struct timespec *outModTime,
												char **outUserHashPath,
												char **outStateFilePath,
												sHashState *inOutHashState,
												sInt32 *outHashDataLen );

    static sInt32	PWSetReplicaData		( sNIContextData *inContext, const char *inAuthorityData );
	
	static sInt32	DoSetPassword			( sNIContextData *inContext, tDataBuffer *inAuthData, const char *inNativeRecType );
	static sInt32	DoSetPasswordAsRoot		( sNIContextData *inContext, tDataBuffer *inAuthData, const char *inNativeRecType );
	static sInt32	DoChangePassword		( sNIContextData *inContext, tDataBuffer *inAuthData, bool isSecondary, const char *inNativeRecType );
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
	sInt32			VerifyPatternMatch		( const tDirPatternMatch inPatternMatch );
	
	static sInt32	IsValidRealname			( char *inRealname, void *inDomain, char **outRecName );
	static char*	GetUserNameForUID 		( uid_t inUserID, void *inDomain );

	NetInfoAuthAuthorityHandlerProc GetNetInfoAuthAuthorityHandler ( const char* inTag );
	
	static CString*	GetAuthTypeStr			( const char *inNativeAuthStr );
	static CString*	GetAuthString			( tDataNode *inData );

	static bool		IsTimRunning			( void );
	static sInt32	MapAuthResult			( sInt32 inAuthResult );

	static bool		DoAnyMatch				( const char *inString, CAttributeList *inPattMatchList, tDirPatternMatch inHow );
	static char*	BuildRegExp				( const char *inString );
	static char*	BuildMultiRegExp		( CAttributeList *inPattMatchList );

	void			MakeGood				( char *inStr, char *outStr );

	sNIContextData*	MakeContextData			( void );
    static sInt32	CleanContextData		( sNIContextData *inContext );
	sInt32			GetSubNodes				( sNIContextData *inContext, set<string> & nodeNames );

private:
	//KW have been extremely lucky up until now using these member vars in thread capable class
	//ie. the netinfo mutex has helped us do this in all but one function ie. GetRecRefInfo fixed now
	CDataBuff	   *fRecData;
	CDataBuff	   *fAttrData;
	CDataBuff	   *fTmpData;
	unsigned int	fHashList;
};

#endif
