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
#include "BaseDirectoryPlugin.h"
#include "DSMutexSemaphore.h"
#include "CDSAuthParams.h"
#include <mach/clock_types.h>	//used for USEC_PER_SEC

#include "CLDAPConnectionManager.h"
#include "CLDAPv3Configs.h"	//used to read the XML config data for each user defined LDAP server
#include "CLDAPBindData.h"
#include "DSAPIWrapper.h"

#include <lber.h>
#include <ldap.h>

#define MaxDefaultLDAPNodeCount		4
#define kLDAPReplicaHintFilePath	"/var/run/DirectoryService.ldap-replicas.plist"

#define kMinTimeToWaitAfterNetworkTransitionForIdleTasksToProceed		60*USEC_PER_SEC

typedef SInt32 (*LDAPv3AuthAuthorityHandlerProc) (tDirNodeReference inNodeRef,
											tDataNodePtr inAuthMethod,
											sLDAPContextData* inContext,
											sLDAPContinueData** inOutContinueData,
											tDataBufferPtr inAuthData,
											tDataBufferPtr outAuthData,
											bool inAuthOnly,
											char* inAuthAuthorityData,
											char* inKerberosId,
                                            CLDAPConnectionManager *inLDAPSessionMgr,
											const char *inRecordType );

// used for maps using Tag as key
struct LDAPv3AuthTagStruct
{
	string version;
	string authData;
	
	LDAPv3AuthTagStruct( char *inVersion, char *inAuthData )
	{
		if( inVersion ) version = inVersion;
		if( inAuthData) authData = inAuthData;
	}
};


class CLDAPv3Plugin : public BaseDirectoryPlugin
{
	public:
							CLDAPv3Plugin				( FourCharCode inSig, const char *inName );
		virtual			   ~CLDAPv3Plugin				( void );
		
		virtual SInt32		Validate					( const char *inVersionStr, const UInt32 inSignature );
		virtual SInt32		Initialize					( void );
		virtual SInt32		SetPluginState				( const UInt32 inState );
		virtual SInt32		PeriodicTask				( void );
		virtual SInt32		ProcessRequest				( void *inData );
		
		static	void		ContinueDeallocProc			( void *inContinueData );

		static SInt32		DoBasicAuth					( tDirNodeReference inNodeRef,
															tDataNodePtr inAuthMethod, 
															sLDAPContextData* inContext, 
															sLDAPContinueData** inOutContinueData, 
															tDataBufferPtr inAuthData, 
															tDataBufferPtr outAuthData, 
															bool inAuthOnly,
															char* inAuthAuthorityData,
															char* inKerberosId,
															CLDAPConnectionManager *inLDAPSessionMgr,
															const char *inRecordType );

		tDirStatus			SetAttributeValueForDN		( sLDAPContextData *pContext,
															char *inDN,
															const char *inRecordType,
															const char *inAttrType,
															const char **inValues );

		SInt32				TryPWSPasswordSet			( tDirNodeReference inNodeRef,
															UInt32 inAuthMethodCode,
															sLDAPContextData *pContext,
															tDataBufferPtr inAuthBuffer,
															const char *inRecordType);

		char*				GetPWSAuthData				( char **inAAArray, UInt32 inAACount );
		
		static SInt32		DoPasswordServerAuth		( tDirNodeReference inNodeRef,
															tDataNodePtr inAuthMethod, 
															sLDAPContextData* inContext, 
															sLDAPContinueData** inOutContinueData, 
															tDataBufferPtr inAuthData, 
															tDataBufferPtr outAuthData, 
															bool inAuthOnly,
															char* inAuthAuthorityData,
															char* inKerberosId,
															CLDAPConnectionManager *inLDAPSessionMgr,
															const char *inRecordType );	
		static SInt32		DoKerberosAuth				( tDirNodeReference inNodeRef,
															tDataNodePtr inAuthMethod,
															sLDAPContextData* inContext,
															sLDAPContinueData** inOutContinueData,
															tDataBufferPtr inAuthData,
															tDataBufferPtr outAuthData,
															bool inAuthOnly,
															char* inAuthAuthorityData,
															char* inKerberosId,
															CLDAPConnectionManager *inLDAPSessionMgr,
															const char *inRecordType );

	#define WaitForNetworkTransitionToFinish()	WaitForNetworkTransitionToFinishWithFunctionName(__PRETTY_FUNCTION__)
		static void			WaitForNetworkTransitionToFinishWithFunctionName
														( const char* callerFunction );
		static char		  **MapAttrListToLDAPTypeArray	( char *inRecType,
															CAttributeList *inAttrTypeList,
															sLDAPContextData *inContext,
															char *inSearchAttr = NULL );
		inline static char	*MapRecToSearchBase			( sLDAPContextData *inContext,
														  const char *inRecType,
														  int inIndex,
														  bool *outOCGroup,
														  CFArrayRef *outOCListCFArray,
														  ber_int_t *outScope )
		{
			CLDAPNodeConfig *nodeConfig = inContext->fLDAPConnection->fNodeConfig;
			if ( nodeConfig != NULL ) 
				return nodeConfig->MapRecToSearchBase( inRecType, inIndex, outOCGroup, outOCListCFArray, outScope );
			return NULL;
		}
	
		inline static char	*MapAttrToLDAPType			( sLDAPContextData *inContext,
														  const char *inRecType,
														  const char *inAttrType,
														  int inIndex, 
														  bool bSkipLiteralMappings = false )
		{
			CLDAPNodeConfig *nodeConfig = inContext->fLDAPConnection->fNodeConfig;
			if ( nodeConfig != NULL ) 
				return nodeConfig->MapAttrToLDAPType( inRecType, inAttrType, inIndex, bSkipLiteralMappings );
			return NULL;
		}
	
		inline static char	**MapAttrToLDAPTypeArray	( sLDAPContextData *inContext,
														  const char *inRecType,
														  const char *inAttrType )
		{
			CLDAPNodeConfig *nodeConfig = inContext->fLDAPConnection->fNodeConfig;
			if ( nodeConfig != NULL ) 
				return nodeConfig->MapAttrToLDAPTypeArray( inRecType, inAttrType );
			return NULL;
		}
	
		inline static char	*ExtractRecMap				( sLDAPContextData *inContext,
														  const char *inRecType,
														  int inIndex,
														  bool *outOCGroup,
														  CFArrayRef *outOCListCFArray,
														  ber_int_t* outScope )
		{
			CLDAPNodeConfig *nodeConfig = inContext->fLDAPConnection->fNodeConfig;
			if ( nodeConfig != NULL ) 
				return nodeConfig->ExtractRecMap( inRecType, inIndex, outOCGroup, outOCListCFArray, outScope );
			return NULL;
		}
		
		inline static char	*ExtractAttrMap				( sLDAPContextData *inContext, 
														  const char *inRecType,
														  const char *inAttrType,
														  int inIndex )
		{
			CLDAPNodeConfig *nodeConfig = inContext->fLDAPConnection->fNodeConfig;
			if ( nodeConfig != NULL ) 
				return nodeConfig->ExtractAttrMap( inRecType, inAttrType, inIndex );
			return NULL;
		}
		
		inline static char	*ExtractStdAttrName			( sLDAPContextData *inContext,
														  char *inRecType,
														  int &inputIndex )
		{
			CLDAPNodeConfig *nodeConfig = inContext->fLDAPConnection->fNodeConfig;
			if ( nodeConfig != NULL ) 
				return nodeConfig->ExtractStdAttrName( inRecType, inputIndex );
			return NULL;
		}
	
		inline static int	AttrMapsCount				( sLDAPContextData *inContext,
														  const char *inRecType,
														  const char *inAttrType )
		{
			CLDAPNodeConfig *nodeConfig = inContext->fLDAPConnection->fNodeConfig;
			if ( nodeConfig != NULL ) 
				return nodeConfig->AttrMapsCount( inRecType, inAttrType );
			return NULL;
		}
	
		inline static char	*BuildLDAPQueryFilter		( sLDAPContextData *inContext,
														  char *inConstAttrType,
														  const char *inConstAttrName,
														  tDirPatternMatch patternMatch, 
														  bool useWellKnownRecType,
														  const char *inRecType,
														  char *inNativeRecType,
														  bool inbOCANDGroup, 
														  CFArrayRef inOCSearchList )	
		{
			CLDAPNodeConfig *nodeConfig = inContext->fLDAPConnection->fNodeConfig;
			if ( nodeConfig != NULL ) 
				return nodeConfig->BuildLDAPQueryFilter( inConstAttrType, inConstAttrName, patternMatch, 
														 useWellKnownRecType, inRecType, inNativeRecType, inbOCANDGroup, 
														 inOCSearchList );
			return NULL;
		}
	
		static char		   *BuildLDAPQueryMultiFilter   ( char *inConstAttrType,
														  char **inAttrNames,
														  tDirPatternMatch patternMatch,
														  sLDAPContextData *inContext,
														  bool useWellKnownRecType,
														  const char *inRecType,
														  char *inNativeRecType,
														  bool inbOCANDGroup,
														  CFArrayRef inOCSearchList );    
		void				WatchForDHCPChanges			( bool inFlag );

	protected:
		SInt32				HandleRequest				( void *inData );
		tDirStatus			GetAllRecords				( char *inRecType,
															char *inNativeRecType,
															CAttributeList *inAttrTypeList,
															sLDAPContextData *inContext,
															sLDAPContinueData *inContinue,
															bool inAttrOnly,
															CBuff *inBuff,
															UInt32 &outRecCount,
															bool inbOCANDGroup,
															CFArrayRef inOCSearchList,
															ber_int_t inScope );
		tDirStatus			GetTheseRecords				( char *inConstAttrType,
															char **inAttrNames,
															char *inConstRecType,
															char *inNativeRecType,
															tDirPatternMatch patternMatch,
															CAttributeList *inAttrTypeList,
															sLDAPContextData *inContext,
															sLDAPContinueData *inContinue,
															bool inAttrOnly,
															CBuff *inBuff, UInt32 &outRecCount,
															bool inbOCANDGroup,
															CFArrayRef inOCSearchList,
															ber_int_t inScope );
		char			   *GetRecordName 				( char *inRecType,
															LDAPMessage	*inResult,
															sLDAPContextData *inContext,
															tDirStatus &errResult );
		tDirStatus			GetTheseAttributes			( char *inRecType,
															CAttributeList *inAttrTypeList,
															LDAPMessage *inResult,
															bool inAttrOnly,
															sLDAPContextData *inContext,
															SInt32 &outCount,
															CDataBuff *inDataBuff );

		static tDirStatus	MapLDAPWriteErrorToDS		( 	SInt32 inLDAPError,
															tDirStatus inDefaultError );
		sLDAPContextData   *MakeContextData				( void );
		
		virtual void			NetworkTransition		( void );

		virtual CFDataRef		CopyConfiguration		( void );
		virtual bool			NewConfiguration		( const char *inData, UInt32 inLength );
		virtual bool			CheckConfiguration		( const char *inData, UInt32 inLength );
		virtual tDirStatus		HandleCustomCall		( sBDPINodeContext *pContext, sDoPlugInCustomCall *inData );
		virtual bool			IsConfigureNodeName		( CFStringRef inNodeName );
#ifndef __OBJC__
		virtual BDPIVirtualNode	*CreateNodeForPath		( CFStringRef inPath, uid_t inUID, uid_t inEffectiveUID );
#else
		virtual id<BDPIVirtualNode>	CreateNodeForPath	( CFStringRef inPath, uid_t inUID, uid_t inEffectiveUID );
#endif	
		virtual tDirStatus		DoPlugInCustomCall		( sDoPlugInCustomCall *inData );

		virtual tDirStatus		OpenDirNode				( sOpenDirNode *inData );
		virtual tDirStatus		CloseDirNode			( sCloseDirNode *inData );
		virtual tDirStatus		GetDirNodeInfo			( sGetDirNodeInfo *inData );
		
		virtual tDirStatus		GetRecordList			( sGetRecordList *inData );
		virtual tDirStatus		GetRecordEntry			( sGetRecordEntry *inData );
		virtual tDirStatus		CreateRecord			( sCreateRecord *inData );
		virtual tDirStatus		OpenRecord				( sOpenRecord *inData );
		virtual tDirStatus		CloseRecord				( sCloseRecord *inData );
		virtual tDirStatus		DeleteRecord			( sDeleteRecord *inData );
		virtual tDirStatus		DeleteRecord			( sDeleteRecord *inData, bool inDeleteCredentials );
		virtual tDirStatus		SetRecordName			( sSetRecordName *inData );
		virtual tDirStatus		FlushRecord				( sFlushRecord *inData );
		
		virtual tDirStatus		GetRecRefInfo			( sGetRecRefInfo *inData );
		virtual tDirStatus		GetRecAttribInfo		( sGetRecAttribInfo *inData );
		virtual tDirStatus		GetRecAttrValueByValue  ( sGetRecordAttributeValueByValue *inData );
		virtual tDirStatus		GetRecAttrValueByIndex	( sGetRecordAttributeValueByIndex *inData );
		virtual tDirStatus		GetRecAttrValueByID		( sGetRecordAttributeValueByID *inData );
		
		virtual tDirStatus		GetAttributeEntry		( sGetAttributeEntry *inData );
		virtual tDirStatus		AddAttribute 			( sAddAttribute *inData, const char *inRecTypeStr );
		virtual tDirStatus		RemoveAttribute			( sRemoveAttribute *inData, const char *inRecTypeStr );
		virtual tDirStatus		CloseAttributeList		( sCloseAttributeList *inData );
		
		virtual tDirStatus		GetAttributeValue		( sGetAttributeValue *inData );
		virtual tDirStatus		AddAttributeValue		( sAddAttributeValue *inData, const char *inRecTypeStr );
		virtual tDirStatus		SetAttributeValue		( sSetAttributeValue *inData, const char *inRecTypeStr );
		virtual tDirStatus		SetAttributeValues		( sSetAttributeValues *inData, const char *inRecTypeStr );
		virtual tDirStatus		RemoveAttributeValue	( sRemoveAttributeValue *inData, const char *inRecTypeStr );
		virtual tDirStatus		CloseAttributeValueList	( sCloseAttributeValueList *inData );
		
		virtual tDirStatus		DoAttributeValueSearch			( sDoAttrValueSearch *inData );
		virtual tDirStatus		DoAttributeValueSearchWithData	( sDoAttrValueSearchWithData *inData );
		virtual tDirStatus		ReleaseContinueData		( sReleaseContinueData *continueData );
		
		tDirStatus				CreateRecordWithAttributes	( tDirNodeReference inNodeRef,
															const char* inRecType,
															const char* inRecName,
															CFDictionaryRef inDict );
		bool				IsConfigRecordModify		( tRecordReference inRecRef );
		static void			IncrementChangeSeqNumber	( void );
		tDirStatus			SetAttributes				( UInt32 inRecRef, CFDictionaryRef inDict );
		tDirStatus			AddValue					( UInt32 inRecRef,
															tDataNodePtr inAttrType,
															tDataNodePtr inAttrValue );
		char			   *GetNextStdAttrType			( char *inRecType, sLDAPContextData *inContext, int &inputIndex );
		bool				DoTheseAttributesMatch		( sLDAPContextData *inContext,
															char *inAttrName,
															tDirPatternMatch	pattMatch,
															LDAPMessage		   *inResult);
		bool				DoAnyOfTheseAttributesMatch ( sLDAPContextData *inContext,
															char **inAttrNames,
															tDirPatternMatch	pattMatch,
															LDAPMessage		   *inResult);
		static bool			DoAnyMatch					( const char		   *inString,
															char			  **inPatterns,
															tDirPatternMatch	inPattMatch );
															
		virtual tDirStatus	DoAuthentication			( sDoDirNodeAuth *inData, const char *inRecTypeStr,
															CDSAuthParams &inParams );
		
		SInt32				DoAuthenticationOnRecordType( sDoDirNodeAuthOnRecordType *inData, const char* inRecordType );
		bool				IsWriteAuthRequest			( UInt32 uiAuthMethod );
		static SInt32		DoSetPassword	 			( sLDAPContextData *inContext,
															tDataBuffer *inAuthData,
															char *inKerberosId,
															CLDAPConnectionManager *inLDAPSessionMgr,
															const char *inRecordType );
															
		static SInt32		DoSetPasswordAsRoot 		( sLDAPContextData *inContext,
															tDataBuffer *inAuthData,
															char *inKerberosId,
															CLDAPConnectionManager *inLDAPSessionMgr,
															const char *inRecordType );
															
		static SInt32		DoChangePassword 			( sLDAPContextData *inContext,
															tDataBuffer *inAuthData,
															char *inKerberosId,
															CLDAPConnectionManager *inLDAPSessionMgr,
															const char *inRecordType );
		static CFMutableDictionaryRef
							ConvertXMLStrToCFDictionary	( const char *xmlStr );

		static char *		ConvertCFDictionaryToXMLStr	( CFDictionaryRef inDict, size_t *outLength );

		static SInt32		PWSetReplicaData			( sLDAPContextData *inContext,
															const char *inAuthorityData,
															CLDAPConnectionManager *inLDAPSessionMgr );
		
		static SInt32		GetAuthAuthority			( sLDAPContextData *inContext,
															const char *userName,
															int inUserNameBufferLength,
															UInt32 *outAuthCount,
															char **outAuthAuthority[],
															const char *inRecordType = kDSStdRecordTypeUsers );

		static tDirStatus	LookupAttribute				( sLDAPContextData *inContext,
															const char *inRecordType,
															const char *inRecordName,
															const char *inAttribute,
															UInt32 *outCount,
															char **outData[] );
		
		static SInt32		DoUnixCryptAuth				( sLDAPContextData *inContext,
															tDataBuffer *inAuthData,
															CLDAPConnectionManager *inLDAPSessionMgr,
															const char *inRecordType );
		static SInt32		DoClearTextAuth				( sLDAPContextData *inContext,
															tDataBuffer *inAuthData,
															char *inKerberosId,
															bool authCheckOnly,
															CLDAPConnectionManager *inLDAPSessionMgr,
															const char *inRecordType );
		
		static bool			KDCHasNonLocalRealm			( sLDAPContextData *inContext );
		
		LDAP				*DoSimpleLDAPBind			( const char *pServer,
															bool bSSL,
															bool bLDAPv2ReadOnly,
															char *pUsername,
															char *pPassword,
															bool bNoCleartext = false,
															SInt32 *outFailureCode = NULL );

		CFArrayRef			GetSASLMethods				( LDAP *pLD );
		bool				OverlaySupportsUniqueNameEnforcement
														( const char *inServer, bool inSSL );
		
		CFMutableArrayRef   GetLDAPAttributeFromResult  ( LDAP *pLD, 
															LDAPMessage *pMessage, 
															const char *pAttribute );

		CFStringRef			GetServerInfoFromConfig		( CFDictionaryRef inDict,
															char **outServer,
															bool *outSSL,
															bool *outLDAPv2ReadOnly,
															char **pUsername, 
															char **pPassword );

		CFArrayRef			GetAttribFromRecordDict		( CFDictionaryRef inDict,
															CFStringRef inAttribute,
															char **outAttribute = NULL,
															CFStringRef *outCFFirstAttribute = NULL );

		CFDictionaryRef		CreateMappingFromConfig		( CFDictionaryRef inDict,
															CFStringRef inRecordType );
		
		bool				IsServerInConfig			(   CFDictionaryRef inConfig,
															CFStringRef inServerName,
															CFIndex *outIndex,
															CFMutableDictionaryRef *outConfig );
		
		char				*GenerateSearchString		( CFDictionaryRef inRecordDict );

		SInt32				DoNewServerDiscovery		( sDoPlugInCustomCall *inData );

		SInt32				DoNewServerVerifySettings	( sDoPlugInCustomCall *inData );

		SInt32				DoNewServerGetConfig		( sDoPlugInCustomCall *inData );

		SInt32				DoNewServerBind				( sDoPlugInCustomCall *inData );

		SInt32				DoNewServerBind2			( sDoPlugInCustomCall *inData );
		tDirStatus			DoNewServerBind2a			( 	sDoPlugInCustomCall *inData,
															DSAPIWrapper &dsWrapper,
															CLDAPBindData &serverInfo,
															tRecordReference recRef,
															tRecordReference recRefHost,
															tRecordReference recRefLKDC,
															const char *pComputerID,
															const char *pCompPassword,
															const char *hostnameDollar,
															const char *localKDCRealmDollarStr );
	
		tDirStatus			DoNewServerBind2b			( sDoPlugInCustomCall *inData,
															DSAPIWrapper &dsWrapper,
															CLDAPBindData &serverInfo,
															tRecordReference recRef,
															tRecordReference recRefHost,
															tRecordReference recRefLKDC,
															const char *pComputerID,
															const char *pCompPassword,
															const char *hostnameDollar,
															const char *localKDCRealmDollarStr );	
	
		SInt32				DoNewServerSetup			( sDoPlugInCustomCall *inData );
		
		SInt32				DoRemoveServer				( sDoPlugInCustomCall *inData );

		tDirStatus			GetRecLDAPMessage			( sLDAPContextData *inRecContext,
															char **inAttrs,
															LDAPMessage **outResultMsg );
		bool				ParseNextDHCPLDAPServerString
														( char **inServer,
															char **inSearchBase,
															int *inPortNumber,
															bool *inIsSSL,
															int inServerIndex );

		LDAPv3AuthAuthorityHandlerProc GetLDAPv3AuthAuthorityHandler
														( const char* inTag );
		char*				MappingNativeVariableSubstitution
														( char *inLDAPAttrType,
															sLDAPContextData   *inContext,
															LDAPMessage		   *inResult,
															tDirStatus&				outResult );
		char*				GetPWSIDforRecord			(	sLDAPContextData	*pContext,
															const char			*inRecName,
															const char			*inRecType );
		tDirStatus			CheckAutomountNames			( const char		   *inRecType,
															char			  **inAttrValues,
															char			 ***outValues,
															char			 ***outMaps,
															UInt32			   *outCount );
		char*				CopyAutomountMapName		( sLDAPContextData	*inContext,
															LDAPMessage			*result );
		bool				DoesThisMatchAutomount		( sLDAPContextData   *inContext,
															UInt32				inCountMaps,
															char			  **inMaps,
															LDAPMessage		   *inResult );

		tDirStatus			GetOwnerGUID				( tDirReference				inDSRef,
														  tRecordReference			inRecRef,
														  const char				*refName,
														  tAttributeValueEntryPtr	*outAttrValEntryPtr );
	
		bool				OwnerGUIDsMatch				( tDirReference		inDSRef,
														  tRecordReference	inCompRecRef,
														  tRecordReference	inHostCompRecRef,
													 	  tRecordReference	inLDKCCompRecRef );
														
		tDirStatus			SetComputerRecordKerberosAuthority
														(	DSAPIWrapper &dsWrapper,
															char *inComputerPassword,
															const char *inRecType,
															const char *inRecName,
															CFMutableDictionaryRef inCFDict,
															char **outKerbIDStr );
	
	private:
		CLDAPConnectionManager	*fLDAPConnectionMgr;
		CLDAPv3Configs			*fConfigFromXML;
};

#endif	// __CLDAPv3Plugin_h__
