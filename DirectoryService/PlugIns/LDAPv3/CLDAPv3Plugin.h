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
                                            CLDAPv3Configs *inConfigFromXML,
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
                                                        CLDAPv3Configs *inConfigFromXML,
                                                        CLDAPNode& inLDAPSessionMgr,
														const char *inRecordType );
                                                        
	static sInt32		DoPasswordServerAuth		(	tDirNodeReference inNodeRef,
                                                        tDataNodePtr inAuthMethod, 
                                                        sLDAPContextData* inContext, 
                                                        sLDAPContinueData** inOutContinueData, 
                                                        tDataBufferPtr inAuthData, 
                                                        tDataBufferPtr outAuthData, 
                                                        bool inAuthOnly,
                                                        char* inAuthAuthorityData,
                                                        CLDAPv3Configs *inConfigFromXML,
                                                        CLDAPNode& inLDAPSessionMgr,
														const char *inRecordType );	
	void				ReInitForNetworkTransition	( 	void );
	static char		  **MapAttrListToLDAPTypeArray	(	char *inRecType,
														CAttributeList *inAttrTypeList,
														uInt32 inConfigTableIndex,
														CLDAPv3Configs *inConfigFromXML );
	static sInt32		RebindLDAPSession			(	sLDAPContextData *inContext,
														CLDAPNode& inLDAPSessionMgr,
														CLDAPv3Configs *inConfigFromXML );
	static char		   *MapRecToLDAPType			(	const char *inRecType,
														uInt32 inConfigTableIndex,
														int inIndex,
														bool *outOCGroup,
														CFArrayRef *outOCListCFArray,
														ber_int_t *outScope,
                                                        CLDAPv3Configs *inConfigFromXML );
	static char			*MapAttrToLDAPType			(	const char *inRecType,
														const char *inAttrType,
														uInt32 inConfigTableIndex,
														int inIndex,
                                                        CLDAPv3Configs *inConfigFromXML,
														bool bSkipLiteralMappings = false );
	static char		   *BuildLDAPQueryFilter		(	char *inConstAttrType,
														const char *inConstAttrName,
														tDirPatternMatch patternMatch,
														uInt32 inConfigTableIndex,
														bool useWellKnownRecType,
														const char *inRecType,
														char *inNativeRecType,
														bool inbOCANDGroup,
														CFArrayRef inOCSearchList,
                                                        CLDAPv3Configs *inConfigFromXML );    

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
	sInt32				GetTheseRecords				(	char *inConstRecName,
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
														uInt32 inConfigTableIndex,
                                                        CLDAPv3Configs *inConfigFromXML );
	static sInt32		MapLDAPWriteErrorToDS		( 	sInt32 inLDAPError,
														sInt32 inDefaultError );
	uInt32				CalcCRC						(	char *inStr );
    static sInt32		CleanContextData			(	sLDAPContextData *inContext );
    sLDAPContextData   *MakeContextData				(	void );
	sInt32				OpenRecord					(	sOpenRecord *inData );
	sInt32				CloseRecord					(	sCloseRecord *inData );
	sInt32				FlushRecord					(	sFlushRecord *inData );
	sInt32				DeleteRecord				(	sDeleteRecord *inData );
	sInt32				CreateRecord				(	sCreateRecord *inData );
	sInt32				AddAttribute				(	sAddAttribute *inData );
	sInt32				AddAttributeValue			(	sAddAttributeValue *inData );
	sInt32				AddValue					(	uInt32 inRecRef,
														tDataNodePtr inAttrType,
														tDataNodePtr inAttrValue );
	sInt32				RemoveAttribute				(	sRemoveAttribute *inData );
	sInt32				RemoveAttributeValue		(	sRemoveAttributeValue *inData );
	sInt32				SetAttributeValue			(	sSetAttributeValue *inData );
	sInt32				SetRecordName				(	sSetRecordName *inData );
	sInt32				ReleaseContinueData			(	sReleaseContinueData *inData );
	sInt32				CloseAttributeList			(	sCloseAttributeList *inData );
	sInt32				CloseAttributeValueList		(	sCloseAttributeValueList *inData );
	sInt32				GetRecRefInfo				(	sGetRecRefInfo *inData );
	sInt32				GetRecAttribInfo			(	sGetRecAttribInfo *inData );
	sInt32				GetRecAttrValueByIndex		(	sGetRecordAttributeValueByIndex *inData );
	sInt32				GetRecordAttributeValueByID	(	sGetRecordAttributeValueByID *inData );
	char			   *GetNextStdAttrType			(	char *inRecType, uInt32 inConfigTableIndex, int &inputIndex );
	sInt32				DoAttributeValueSearch		(	sDoAttrValueSearchWithData *inData );
	bool				DoTheseAttributesMatch		(	sLDAPContextData *inContext,
														char *inAttrName,
														tDirPatternMatch	pattMatch,
														LDAPMessage		   *inResult);
	static bool			DoesThisMatch				(	const char		   *inString,
														const char		   *inPatt,
														tDirPatternMatch	inPattMatch );
	sInt32				FindAllRecords              (	char *inConstAttrName,
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
	sInt32				FindTheseRecords			(	char *inConstAttrType,
                                                        char *inConstAttrName,
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
	sInt32				DoAuthenticationOnRecordType(	sDoDirNodeAuthOnRecordType *inData );
    bool				IsWriteAuthRequest			(	uInt32 uiAuthMethod );
    static sInt32		DoSetPassword	 			(	sLDAPContextData *inContext,
                                                        tDataBuffer *inAuthData,
                                                        CLDAPv3Configs *inConfigFromXML,
                                                        CLDAPNode& inLDAPSessionMgr,
														const char *inRecordType );
                                                        
    static sInt32		DoSetPasswordAsRoot 		(	sLDAPContextData *inContext,
                                                        tDataBuffer *inAuthData,
                                                        CLDAPv3Configs *inConfigFromXML,
                                                        CLDAPNode& inLDAPSessionMgr,
														const char *inRecordType );
                                                        
    static sInt32		DoChangePassword 			(	sLDAPContextData *inContext,
                                                        tDataBuffer *inAuthData,
                                                        CLDAPv3Configs *inConfigFromXML,
                                                        CLDAPNode& inLDAPSessionMgr,
														const char *inRecordType );

	static sInt32		GetAuthMethod				(	tDataNode *inData,
														uInt32 *outAuthMethod );
    
	static sInt32		PWSetReplicaData			(	sLDAPContextData *inContext,
														const char *inAuthorityData,
														CLDAPv3Configs *inConfigFromXML,
														CLDAPNode& inLDAPSessionMgr );
	
	static sInt32		RepackBufferForPWServer		(	tDataBufferPtr inBuff,
                                                        const char *inUserID,
                                                        unsigned long inUserIDNodeNum,
                                                        tDataBufferPtr *outBuff );
    
    static sInt32		PWOpenDirNode				(	tDirNodeReference fDSRef,
                                                        char *inNodeName,
                                                        tDirNodeReference *outNodeRef );

    static sInt32		GetAuthAuthority			(	sLDAPContextData *inContext,
                                                        const char *userName,
                                                        CLDAPv3Configs *inConfigFromXML,
                                                        CLDAPNode& inLDAPSessionMgr,
                                                        unsigned long *outAuthCount,
                                                        char **outAuthAuthority[],
														const char *inRecordType = kDSStdRecordTypeUsers );

    static sInt32		LookupAttribute				(	sLDAPContextData *inContext,
														const char *inRecordType,
                                                        const char *inRecordName,
                                                        const char *inAttribute,
                                                        CLDAPv3Configs *inConfigFromXML,
                                                        CLDAPNode& inLDAPSessionMgr,
                                                        unsigned long *outCount,
                                                        char **outData[] );
	
    static sInt32		ParseAuthAuthority			(	const char * inAuthAuthority,
                                                        char **outVersion,
                                                        char **outAuthTag,
                                                        char **outAuthData );

	static sInt32		DoUnixCryptAuth				(	sLDAPContextData *inContext,
														tDataBuffer *inAuthData,
                                                        CLDAPv3Configs *inConfigFromXML,
                                                        CLDAPNode& inLDAPSessionMgr,
														const char *inRecordType );
	static sInt32		DoClearTextAuth				(	sLDAPContextData *inContext,
														tDataBuffer *inAuthData,
														bool authCheckOnly,
                                                        CLDAPv3Configs *inConfigFromXML,
                                                        CLDAPNode& inLDAPSessionMgr,
														const char *inRecordType );
	static char		   *GetDNForRecordName			(	char* inRecName,
														sLDAPContextData *inContext,
                                                        CLDAPv3Configs *inConfigFromXML,
                                                        CLDAPNode& inLDAPSessionMgr,
														const char *inRecordType );
	sInt32				DoPlugInCustomCall			(	sDoPlugInCustomCall *inData );

	static sInt32		VerifyLDAPSession			(	sLDAPContextData *inContext,
														int inContinueMsgId,
                                                        CLDAPNode& inLDAPSessionMgr );

	sInt32				GetRecRefLDAPMessage		(	sLDAPContextData *inRecContext,
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
														uInt32 inConfigTableIndex,
                                                        CLDAPv3Configs *inConfigFromXML );
    
    static sInt32		GetUserNameFromAuthBuffer	(	tDataBufferPtr inAuthData,
                                                        unsigned long inUserNameIndex,
                                                        char **outUserName );
    
	LDAPv3AuthAuthorityHandlerProc GetLDAPv3AuthAuthorityHandler
													(	const char* inTag );
	void				HandleMultipleNetworkTransitionsForLDAP
													(   void );

private:
	bool				bDoNotInitTwiceAtStartUp;	//don't want setpluginstate call to init again at startup
	CFStringRef			fDHCPLDAPServersString;
	uInt32				fState;
	CLDAPv3Configs	   *pConfigFromXML;
	CLDAPNode			fLDAPSessionMgr;
	CFRunLoopRef		fServerRunLoop;
	double				fTimeToHandleNetworkTransition;
	DSMutexSemaphore	fCheckInitFlag;
	bool				fInitFlag;

};

#endif	// __CLDAPv3Plugin_h__
