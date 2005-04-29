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
 * @header CLDAPv3Plugin
 * LDAP v3 plugin definitions to work with LDAP v3 Plugin implementation
 * that interfaces with Directory Services.
 */

#ifndef __CLDAPv3Plugin_h__
#define __CLDAPv3Plugin_h__	1

#include <stdio.h>

#include "CBuff.h"
#include "CDataBuff.h"
#include "CAttributeList.h"
#include "SharedConsts.h"
#include "PluginData.h"
#include "CServerPlugin.h"
#include "DSMutexSemaphore.h"

#include "CLDAPNode.h"
#include "CLDAPv3Configs.h"	//used to read the XML config data for each user defined LDAP server

#include <lber.h>
#include <ldap.h>

#define MaxDefaultLDAPNodeCount 4

#define kMinTimeToWaitAfterNetworkTransitionForIdleTasksToProceed		60*USEC_PER_SEC

typedef struct {
    int				msgId;				//LDAP session call handle mainly used for searches
    LDAPMessage	   *pResult;			//LDAP message last result used for continued searches
    uInt32			fRecNameIndex;		//index used to cycle through all requested Rec Names
    uInt32			fRecTypeIndex;		//index used to cycle through all requested Rec Types
    uInt32			fTotalRecCount;		//count of all retrieved records
    uInt32			fLimitRecSearch;	//client specified limit of number of records to return
    void			*fAuthHndl;
	void			*fAuthHandlerProc;
	char			*fAuthAuthorityData;
	char			*fAuthKerberosID;
    tContextData	fPassPlugContinueData;
} sLDAPContinueData;

typedef sInt32 (*LDAPv3AuthAuthorityHandlerProc) (tDirNodeReference inNodeRef,
											tDataNodePtr inAuthMethod,
											sLDAPContextData* inContext,
											sLDAPContinueData** inOutContinueData,
											tDataBufferPtr inAuthData,
											tDataBufferPtr outAuthData,
											bool inAuthOnly,
											char* inAuthAuthorityData,
											char* inKerberosId,
                                            CLDAPNode& inLDAPSessionMgr,
											const char *inRecordType );

class CLDAPv3Plugin : public CServerPlugin
{
public:
						CLDAPv3Plugin				(	FourCharCode inSig, const char *inName );
	virtual			   ~CLDAPv3Plugin				(	void );

	virtual sInt32		Validate					( 	const char *inVersionStr,
                                                        const uInt32 inSignature );
	virtual sInt32		Initialize					( 	void );
	//virtual sInt32		Configure					( 	void );
	virtual sInt32		SetPluginState				( 	const uInt32 inState );
	virtual sInt32		PeriodicTask				( 	void );
	virtual sInt32		ProcessRequest				( 	void *inData );
	//virtual sInt32		Shutdown					( 	void );
    
	static	void		ContextDeallocProc			(	void* inContextData );
	static	void		ContinueDeallocProc			(	void *inContinueData );

	static sInt32		DoBasicAuth					(	tDirNodeReference inNodeRef,
                                                        tDataNodePtr inAuthMethod, 
                                                        sLDAPContextData* inContext, 
                                                        sLDAPContinueData** inOutContinueData, 
                                                        tDataBufferPtr inAuthData, 
                                                        tDataBufferPtr outAuthData, 
                                                        bool inAuthOnly,
                                                        char* inAuthAuthorityData,
														char* inKerberosId,
                                                        CLDAPNode& inLDAPSessionMgr,
														const char *inRecordType );

	sInt32				SetAttributeValueForDN		(   sLDAPContextData *pContext,
														char *inDN,
														const char *inRecordType,
														char *inAttrType,
														char **inValues );

	sInt32				TryPWSPasswordSet			(   tDirNodeReference inNodeRef,
														uInt32 inAuthMethodCode,
														sLDAPContextData *pContext,
														tDataBufferPtr inAuthBuffer,
														const char *inRecordType);

	static sInt32		DoPasswordServerAuth		(	tDirNodeReference inNodeRef,
                                                        tDataNodePtr inAuthMethod, 
                                                        sLDAPContextData* inContext, 
                                                        sLDAPContinueData** inOutContinueData, 
                                                        tDataBufferPtr inAuthData, 
                                                        tDataBufferPtr outAuthData, 
                                                        bool inAuthOnly,
                                                        char* inAuthAuthorityData,
														char* inKerberosId,
                                                        CLDAPNode& inLDAPSessionMgr,
														const char *inRecordType );	
	static sInt32		DoKerberosAuth				(	tDirNodeReference inNodeRef,
														tDataNodePtr inAuthMethod,
														sLDAPContextData* inContext,
														sLDAPContinueData** inOutContinueData,
														tDataBufferPtr inAuthData,
														tDataBufferPtr outAuthData,
														bool inAuthOnly,
														char* inAuthAuthorityData,
														char* inKerberosId,
														CLDAPNode& inLDAPSessionMgr,
														const char *inRecordType );
	void				ReInitForNetworkTransition	( 	void );

#define WaitForNetworkTransitionToFinish()	WaitForNetworkTransitionToFinishWithFunctionName(__PRETTY_FUNCTION__)
	static void			WaitForNetworkTransitionToFinishWithFunctionName
													(	const char* callerFunction );
	static bool			HandlingNetworkTransition	(	void ) { return fHandlingNetworkTransition; }
	static char		  **MapAttrListToLDAPTypeArray	(	char *inRecType,
														CAttributeList *inAttrTypeList,
														char *inConfigName );
	static sInt32		RebindLDAPSession			(	sLDAPContextData *inContext,
														CLDAPNode& inLDAPSessionMgr );
	static char		   *MapRecToLDAPType			(	const char *inRecType,
														char *inConfigName,
														int inIndex,
														bool *outOCGroup,
														CFArrayRef *outOCListCFArray,
														ber_int_t *outScope );
	static char			*MapAttrToLDAPType			(	const char *inRecType,
														const char *inAttrType,
														char *inConfigName,
														int inIndex,
														bool bSkipLiteralMappings = false );
	static char		   *BuildLDAPQueryFilter		(	char *inConstAttrType,
														const char *inConstAttrName,
														tDirPatternMatch patternMatch,
														char *inConfigName,
														bool useWellKnownRecType,
														const char *inRecType,
														char *inNativeRecType,
														bool inbOCANDGroup,
														CFArrayRef inOCSearchList );    
	static char		   *BuildLDAPQueryMultiFilter   (	char *inConstAttrType,
														char **inAttrNames,
														tDirPatternMatch patternMatch,
														char *inConfigName,
														bool useWellKnownRecType,
														const char *inRecType,
														char *inNativeRecType,
														bool inbOCANDGroup,
														CFArrayRef inOCSearchList );    

protected:
	void				WakeUpRequests				(	void );
	void				WaitForInit					(	void );
	sInt32				HandleRequest				(	void *inData );
    sInt32				OpenDirNode					(	sOpenDirNode *inData );
    sInt32				CloseDirNode				(	sCloseDirNode *inData );
	sInt32				GetDirNodeInfo				(	sGetDirNodeInfo *inData );
    sInt32				GetRecordList				(	sGetRecordList *inData );
    sInt32				GetAllRecords				(	char *inRecType,
														char *inNativeRecType,
														CAttributeList *inAttrTypeList,
														sLDAPContextData *inContext,
														sLDAPContinueData *inContinue,
														bool inAttrOnly,
														CBuff *inBuff,
														uInt32 &outRecCount,
														bool inbOCANDGroup,
														CFArrayRef inOCSearchList,
														ber_int_t inScope );
	sInt32				GetTheseRecords				(	char *inConstAttrType,
														char **inAttrNames,
														char *inConstRecType,
														char *inNativeRecType,
														tDirPatternMatch patternMatch,
														CAttributeList *inAttrTypeList,
														sLDAPContextData *inContext,
														sLDAPContinueData *inContinue,
														bool inAttrOnly,
														CBuff *inBuff, uInt32 &outRecCount,
														bool inbOCANDGroup,
														CFArrayRef inOCSearchList,
														ber_int_t inScope );
    char			   *GetRecordName 				(	char *inRecType,
														LDAPMessage	*inResult,
														sLDAPContextData *inContext,
														sInt32 &errResult );
    sInt32				GetRecordEntry				(	sGetRecordEntry *inData );
	sInt32				GetTheseAttributes			(	char *inRecType,
														CAttributeList *inAttrTypeList,
														LDAPMessage *inResult,
														bool inAttrOnly,
														sLDAPContextData *inContext,
														sInt32 &outCount,
														CDataBuff *inDataBuff );
	sInt32				GetAttributeEntry			(	sGetAttributeEntry *inData );
	sInt32				GetAttributeValue			(	sGetAttributeValue *inData );
	static char		  **MapAttrToLDAPTypeArray		(	const char *inRecType,
														const char *inAttrType,
														char *inConfigName );
	static sInt32		MapLDAPWriteErrorToDS		( 	sInt32 inLDAPError,
														sInt32 inDefaultError );
    static sInt32		CleanContextData			(	sLDAPContextData *inContext );
    sLDAPContextData   *MakeContextData				(	void );
	sInt32				OpenRecord					(	sOpenRecord *inData );
	sInt32				CloseRecord					(	sCloseRecord *inData );
	sInt32				FlushRecord					(	sFlushRecord *inData );
	sInt32				DeleteRecord				(	sDeleteRecord *inData, bool inDeleteCredentials = false );
	sInt32				CreateRecord				(	sCreateRecord *inData );
	sInt32				CreateRecordWithAttributes	(	tDirNodeReference inNodeRef,
														const char* inRecType,
														const char* inRecName,
														CFDictionaryRef inDict );
	sInt32				AddAttribute				(	sAddAttribute *inData );
	sInt32				AddAttributeValue			(	sAddAttributeValue *inData );
	sInt32				SetAttributes				(	unsigned long inRecRef, CFDictionaryRef inDict );
	sInt32				AddValue					(	uInt32 inRecRef,
														tDataNodePtr inAttrType,
														tDataNodePtr inAttrValue );
	sInt32				RemoveAttribute				(	sRemoveAttribute *inData );
	sInt32				RemoveAttributeValue		(	sRemoveAttributeValue *inData );
	sInt32				SetAttributeValue			(	sSetAttributeValue *inData );
	sInt32				SetAttributeValues			(	sSetAttributeValues *inData );
	sInt32				SetRecordName				(	sSetRecordName *inData );
	sInt32				ReleaseContinueData			(	sReleaseContinueData *inData );
	sInt32				CloseAttributeList			(	sCloseAttributeList *inData );
	sInt32				CloseAttributeValueList		(	sCloseAttributeValueList *inData );
	sInt32				GetRecRefInfo				(	sGetRecRefInfo *inData );
	sInt32				GetRecAttribInfo			(	sGetRecAttribInfo *inData );
	sInt32				GetRecAttrValueByIndex		(	sGetRecordAttributeValueByIndex *inData );
	sInt32				GetRecAttrValueByValue		(	sGetRecordAttributeValueByValue *inData );
	sInt32				GetRecordAttributeValueByID	(	sGetRecordAttributeValueByID *inData );
	char			   *GetNextStdAttrType			(	char *inRecType, char *inConfigName, int &inputIndex );
	sInt32				DoAttributeValueSearch		(	sDoAttrValueSearchWithData *inData );
	sInt32				DoMultipleAttributeValueSearch
													(	sDoMultiAttrValueSearchWithData *inData );
	bool				DoTheseAttributesMatch		(	sLDAPContextData *inContext,
														char *inAttrName,
														tDirPatternMatch	pattMatch,
														LDAPMessage		   *inResult);
	bool				DoAnyOfTheseAttributesMatch (	sLDAPContextData *inContext,
														char **inAttrNames,
														tDirPatternMatch	pattMatch,
														LDAPMessage		   *inResult);
	static bool			DoAnyMatch					(	const char		   *inString,
														char			  **inPatterns,
														tDirPatternMatch	inPattMatch );
	sInt32				FindTheseRecords			(	char *inConstAttrType,
                                                        char **inAttrNames,
                                                        char *inConstRecType,
                                                        char *inNativeRecType,
                                                        tDirPatternMatch patternMatch,
                                                        CAttributeList *inAttrTypeList,
                                                        sLDAPContextData *inContext,
                                                        sLDAPContinueData *inContinue,
                                                        bool inAttrOnly,
                                                        CBuff *inBuff,
                                                        uInt32 &outRecCount,
                                                        bool inbOCANDGroup,
                                                        CFArrayRef inOCSearchList,
														ber_int_t inScope );
                                                        
	sInt32				DoAuthentication			(	sDoDirNodeAuth *inData );
	sInt32				DoAuthenticationOnRecordType(	sDoDirNodeAuthOnRecordType *inData, const char* inRecordType );
    bool				IsWriteAuthRequest			(	uInt32 uiAuthMethod );
    static sInt32		DoSetPassword	 			(	sLDAPContextData *inContext,
                                                        tDataBuffer *inAuthData,
														char *inKerberosId,
                                                        CLDAPNode& inLDAPSessionMgr,
														const char *inRecordType );
                                                        
    static sInt32		DoSetPasswordAsRoot 		(	sLDAPContextData *inContext,
                                                        tDataBuffer *inAuthData,
														char *inKerberosId,
                                                        CLDAPNode& inLDAPSessionMgr,
														const char *inRecordType );
                                                        
    static sInt32		DoChangePassword 			(	sLDAPContextData *inContext,
                                                        tDataBuffer *inAuthData,
														char *inKerberosId,
                                                        CLDAPNode& inLDAPSessionMgr,
														const char *inRecordType );
	
	static sInt32		PWSetReplicaData			(	sLDAPContextData *inContext,
														const char *inAuthorityData,
														CLDAPNode& inLDAPSessionMgr );
	
    static sInt32		GetAuthAuthority			(	sLDAPContextData *inContext,
                                                        const char *userName,
                                                        CLDAPNode& inLDAPSessionMgr,
                                                        unsigned long *outAuthCount,
                                                        char **outAuthAuthority[],
														const char *inRecordType = kDSStdRecordTypeUsers );

    static sInt32		LookupAttribute				(	sLDAPContextData *inContext,
														const char *inRecordType,
                                                        const char *inRecordName,
                                                        const char *inAttribute,
                                                        CLDAPNode& inLDAPSessionMgr,
                                                        unsigned long *outCount,
                                                        char **outData[] );
	
    static sInt32		ParseAuthAuthority			(	const char * inAuthAuthority,
                                                        char **outVersion,
                                                        char **outAuthTag,
                                                        char **outAuthData );

	static sInt32		DoUnixCryptAuth				(	sLDAPContextData *inContext,
														tDataBuffer *inAuthData,
                                                        CLDAPNode& inLDAPSessionMgr,
														const char *inRecordType );
	static sInt32		DoClearTextAuth				(	sLDAPContextData *inContext,
														tDataBuffer *inAuthData,
														char *inKerberosId,
														bool authCheckOnly,
                                                        CLDAPNode& inLDAPSessionMgr,
														const char *inRecordType );
	static char		   *GetDNForRecordName			(	char* inRecName,
														sLDAPContextData *inContext,
                                                        CLDAPNode& inLDAPSessionMgr,
														const char *inRecordType );
	sInt32				DoPlugInCustomCall			(	sDoPlugInCustomCall *inData );
	
	LDAP				*DoSimpleLDAPBind			(   char *pServer,
														bool bSSL,
														bool bLDAPv2ReadOnly,
														char *pUsername,
														char *pPassword,
														bool bNoCleartext = false,
														sInt32 *outFailureCode = NULL );

	CFArrayRef			GetSASLMethods				(   LDAP *pLD );

	CFMutableArrayRef   GetLDAPAttributeFromResult  (   LDAP *pLD, 
														LDAPMessage *pMessage, 
														char *pAttribute );

	CFStringRef			GetServerInfoFromConfig		(   CFDictionaryRef inDict,
														char **outServer,
														bool *outSSL,
														bool *outLDAPv2ReadOnly,
														char **pUsername, 
														char **pPassword );

	CFMutableDictionaryRef
						GetXMLFromBuffer			(   tDataBufferPtr inBuffer );

	sInt32				PutXMLInBuffer				(   CFDictionaryRef inXMLDict, 
														tDataBufferPtr outBuffer );

	CFArrayRef			GetAttribFromRecordDict		(   CFDictionaryRef inDict,
														CFStringRef inAttribute,
														char **outAttribute = NULL,
														CFStringRef *outCFFirstAttribute = NULL );

	CFDictionaryRef		CreateMappingFromConfig		(   CFDictionaryRef inDict,
														CFStringRef inRecordType );
	
	bool				IsServerInConfig			(   CFDictionaryRef inConfig,
														CFStringRef inServerName,
														CFIndex *outIndex,
														CFMutableDictionaryRef *outConfig );
	
	char				*GenerateSearchString		(   CFDictionaryRef inRecordDict );

	sInt32				DoNewServerDiscovery		(   sDoPlugInCustomCall *inData );

	sInt32				DoNewServerVerifySettings   (   sDoPlugInCustomCall *inData );

	sInt32				DoNewServerGetConfig		(   sDoPlugInCustomCall *inData );

	sInt32				DoNewServerBind				(   sDoPlugInCustomCall *inData );

	sInt32				DoNewServerBind2			(   sDoPlugInCustomCall *inData );

	sInt32				DoNewServerSetup			(   sDoPlugInCustomCall *inData );
	
	sInt32				DoRemoveServer				(   sDoPlugInCustomCall *inData );
	
	static sInt32		VerifyLDAPSession			(	sLDAPContextData *inContext,
														int inContinueMsgId,
                                                        CLDAPNode& inLDAPSessionMgr );

	sInt32				GetRecRefLDAPMessage		(	sLDAPContextData *inRecContext,
														int &ldapMsgId,
														char **inAttrs,
														LDAPMessage **outResultMsg );
	bool				ParseNextDHCPLDAPServerString
													(	char **inServer,
														char **inSearchBase,
														int *inPortNumber,
														bool *inIsSSL,
														int inServerIndex );
    static CFMutableStringRef
                        ParseCompoundExpression		(	const char *inConstAttrName,
														const char *inRecType,
														char *inConfigName );
    
	LDAPv3AuthAuthorityHandlerProc GetLDAPv3AuthAuthorityHandler
													(	const char* inTag );
	void				HandleMultipleNetworkTransitionsForLDAP
													(   void );
	char*				MappingNativeVariableSubstitution
													(	char *inLDAPAttrType,
														sLDAPContextData   *inContext,
														LDAPMessage		   *inResult,
														sInt32&				outResult );
	char*				BuildEscapedRDN				(	const char *inLDAPRDN );
	char*				GetPWSIDforRecord			(	sLDAPContextData	*pContext,
														char				*inRecName,
														char				*inRecType );

private:
	static CLDAPNode	fLDAPSessionMgr;

	bool				bDoNotInitTwiceAtStartUp;	//don't want setpluginstate call to init again at startup
	CFStringRef			fDHCPLDAPServersString;
	CFStringRef			fPrevDHCPLDAPServersString;
	uInt32				fState;
	CFRunLoopRef		fServerRunLoop;
	static bool			fHandlingNetworkTransition;
	double				fTimeToHandleNetworkTransition;
	DSMutexSemaphore	fCheckInitFlag;
	bool				fInitFlag;

};

#endif	// __CLDAPv3Plugin_h__
