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
 * @header CPSPlugIn
 */

#ifndef __CPSPlugIn_h__
#define __CPSPlugIn_h__	1

extern "C" {
	#include <stdio.h>
	#include <string.h>		//used for strcpy, etc.
	#include <stdlib.h>		//used for malloc
	
	#include <stdarg.h>
	#include <ctype.h>
	#include <sysexits.h>
	#include <errno.h>
	
	#ifdef HAVE_UNISTD_H
	#include <unistd.h>
	#endif
	
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <netdb.h>
	
	#include <openssl/bn.h>
	#include <openssl/blowfish.h>
	#include <openssl/md5.h>
	#include <openssl/rc4.h>
	#include <openssl/rc5.h>
	
	#include <sasl/sasl.h>
	#include <sasl/saslutil.h>
	
	#include "key.h"
};
#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesUtils.h>
#include <DirectoryService/DirServicesConst.h>

#include <DirectoryServiceCore/CBuff.h>
#include <DirectoryServiceCore/CDataBuff.h>
#include <DirectoryServiceCore/CAttributeList.h>
#include <DirectoryServiceCore/SharedConsts.h>
#include <DirectoryServiceCore/PluginData.h>
#include <DirectoryServiceCore/CDSServerModule.h>
#include <DirectoryServiceCore/CSharedData.h>

#include <CoreFoundation/CoreFoundation.h>

#include "AuthFile.h"
#include "CReplicaFile.h"
#include "CPSUtilities.h"
#include "CPSPluginDefines.h"
#include "CAuthParams.h"

#define kUserIDLength						34
#define kMaxOpenNodesBeforeQuickClose		100
#define kSendQuit							true


#define kChangePassPaddedBufferSize			512
#define kOneKBuffer							1024


#define Throw_NULL(A,B)			if ((A)==NULL) throw((SInt32)B)
#define Return_if_NULL(A,B)		if ((A)==NULL) return((tDirStatus)B)

#if 1
#define DEBUGLOG(A,args...)		CShared::LogIt( 0x0F, (A), ##args )
#else
#define DEBUGLOG(A,args...)		
#endif

enum {
	kAuthDIGEST_MD5_Reauth			= 153,
	kAuthPull						= 156,
	kAuthPush						= 157,
	kAuthProcessNoReply				= 158,
	kAuthNTLMv2SessionKey			= 165,
	kAuthGetStats					= 167,
	kAuthGetChangeList				= 168
};

typedef enum NewUserParamListType {
	kNewUserParamsNone,
	kNewUserParamsPolicy
};

typedef struct MethodMapEntry {
	const char *saslName;
	const char *odName;
} MethodMapEntry;

class CPSPlugIn : public CDSServerModule
{
public:
						CPSPlugIn					(	void );
	virtual			   ~CPSPlugIn					(	void );

	virtual SInt32		Validate					(	const char *inVersionStr,
														const UInt32 inSignature );
	virtual SInt32		Initialize					(	void );
	virtual SInt32		ProcessRequest				(	void *inData );
	virtual SInt32		SetPluginState				(	const UInt32 inState );
	static void			ContinueDeallocProc			(	void* inContinueData );
	static void			ContextDeallocProc			(	void* inContextData );
	static void			ReleaseCloseWaitConnections	(	void* inContextData );

protected:
	void				WakeUpRequests				(	void );
	void				WaitForInit					(	void );
	SInt32				HandleRequest				(	void *inData );
	SInt32				ReleaseContinueData			(	sReleaseContinueData *inData );
    SInt32				OpenDirNode					(	sOpenDirNode *inData );
    SInt32				CloseDirNode				(	sCloseDirNode *inData );
	SInt32				GetDirNodeInfo				(	sGetDirNodeInfo *inData );
	SInt32				HandleFirstContact			(	sPSContextData *inContext,
														const char *inIP,
														const char *inUserKeyHash,
														const char *inUserKeyStr = NULL,
														bool inSecondTime = false );
	SInt32				BeginServerSession			(	sPSContextData *inContext, int inSock, const char *inUserKeyHash );
	static SInt32		EndServerSession			(	sPSContextData *inContext, bool inSendQuit = false );
	SInt32				GetSASLMechListFromString	(	const char *inSASLList, AuthMethName **mechList, int *mechCount );
    SInt32				GetRSAPublicKey				(	sPSContextData *inContext, char *inData = NULL );
 	SInt32				DoRSAValidation				(	sPSContextData *inContext, const char *inUserKey );
	SInt32				SetupSecureSyncSession		(	sPSContextData *inContext );
	static bool			SecureSyncSessionIsSetup	(	sPSContextData *inContext );
	SInt32				GetAttributeEntry			(	sGetAttributeEntry *inData );
	SInt32				GetAttributeValue			(	sGetAttributeValue *inData );
	UInt32				CalcCRC						(	char *inStr );
    static SInt32		CleanContextData			(	sPSContextData *inContext );
    sPSContextData	   *MakeContextData				(	void );
	SInt32				CloseAttributeList			(	sCloseAttributeList *inData );
	SInt32				CloseAttributeValueList		(	sCloseAttributeValueList *inData );
    SInt32				GetStringFromAuthBuffer		(	tDataBufferPtr inAuthData,
                                                        int stringNum,
                                                        char **outString );
	SInt32				Get2StringsFromAuthBuffer	(	tDataBufferPtr inAuthData, char **outString1, char **outString2 );
	
    SInt32				GetDataFromAuthBuffer		(	tDataBufferPtr inAuthData,
                                                        int nodeNum,
                                                        unsigned char **outData,
                                                        UInt32 *outLen );
    
	void				UpdateCachedPasswordOnChange (   sPSContextData *inContext,
														const char *inChangedUser,
														const char *inPassword,
														long inPasswordLen );
	
	int					GetSASLMechCount			(	sPSContextData *inContext );
	void				SASLInit					(	void );
	SInt32				DoAuthentication			(	sDoDirNodeAuth *inData );
	SInt32				DoAuthenticationSetup		(	sDoDirNodeAuth *inData );
	SInt32				DoAuthenticationResponse	(	sDoDirNodeAuth *inData, CAuthParams &pb );

	// ------------------------------------------------------------------------------------------------

	SInt32				DoAuthMethodNewUser			(   sDoDirNodeAuth *inData,
														sPSContextData *inContext,
														bool inWithPolicy,
														tDataBufferPtr outBuf );
	SInt32				DoAuthMethodNewComputer		(   sDoDirNodeAuth *inData,
														sPSContextData *inContext,
														tDataBufferPtr outBuf );
	SInt32				DoAuthMethodSetComputerAccountPassword
													(   sDoDirNodeAuth *inData,
														sPSContextData *inContext,
														tDataBufferPtr outBuf );
	SInt32				DoAuthMethodListReplicas	(   sDoDirNodeAuth *inData,
														sPSContextData *inContext,
														tDataBufferPtr outBuf );
    SInt32				DoAuthMethodPull			(	sDoDirNodeAuth *inData,
														sPSContextData *inContext,
														tDataBufferPtr outBuf );
    SInt32				DoAuthMethodPush			(	sDoDirNodeAuth *inData,
														sPSContextData *inContext,
														tDataBufferPtr outBuf );
	SInt32				DoAuthMethodNTUserSessionKey(	SInt32 inAuthenticatorAuthResult,
														UInt32 inAuthMethod,
														sDoDirNodeAuth *inData,
														sPSContextData *inContext,
														tDataBufferPtr outBuf );
	SInt32				DoAuthMethodMSChapChangePass(	sDoDirNodeAuth *inData, sPSContextData *inContext );
	SInt32				DoAuthMethodEncryptToUser	(	sDoDirNodeAuth *inData,
														sPSContextData *inContext,
														tDataBufferPtr outBuf );
	SInt32				DoAuthMethodDecrypt			(	sDoDirNodeAuth *inData,
														sPSContextData *inContext,
														tDataBufferPtr outBuf );
	SInt32				DoAuthMethodSetHash			(	sDoDirNodeAuth *inData, sPSContextData *inContext, const char *inCommandStr );
	SInt32				DoAuthMethodNTLMv2SessionKey(   SInt32 inAuthenticatorAuthResult,
														UInt32 inAuthMethod,
														sDoDirNodeAuth *inData,
														sPSContextData *inContext,
														tDataBufferPtr outBuf );
	SInt32				DoAuthMethodGetStats		(	sDoDirNodeAuth *inData, sPSContextData *inContext, tDataBufferPtr outBuf );
	SInt32				DoAuthMethodGetChangeList	(	sDoDirNodeAuth *inData, sPSContextData *inContext, tDataBufferPtr outBuf );
	
	// ------------------------------------------------------------------------------------------------
	
	SInt32				SetServiceInfo				(	sDoDirNodeAuth *inData, sPSContextData *inContext );
	SInt32				GetReplicaListFromServer	(   sPSContextData *inContext,
														char **outData,
														unsigned long *outDataLen );
	SInt32				GetLargeReplyFromServer		(	const char *inCommand,
														sPSContextData *inContext,
														char **outData,
														unsigned long *outDataLen );
	SInt32				UseCurrentAuthenticationIfPossible
													(	sPSContextData *inContext,
														const char *inUserName,
														UInt32 inAuthMethod,
														Boolean *inOutHasValidAuth );
	
	SInt32				PackStepBuffer				(	const char *inArg1,
														bool inUseBuffPlus4, 
														const char *inArg2,
														const char *inArg3,
														const char *inArg4,
														tDataBufferPtr inOutDataBuffer );
	
	SInt32				UnpackUsernameAndPassword	(	sPSContextData *inContext,
                                                        UInt32 uiAuthMethod,
                                                        tDataBufferPtr inAuthBuf,
                                                        char **outUserName,
                                                        char **outPassword,
                                                        UInt32 *outPasswordLen,
                                                        char **outChallenge );
														
	SInt32				UnpackUsernameAndPasswordDefault
													(	tDataBufferPtr inAuthBuf,
                                                        char **outUserName,
                                                        char **outPassword,
                                                        UInt32 *outPasswordLen );
														
	SInt32				UnpackUsernameAndPasswordAtOffset
													(	tDataBufferPtr inAuthBuf,
														UInt32 inUserItem,
                                                        char **outUserName,
                                                        char **outPassword,
                                                        UInt32 *outPasswordLen );
														
    SInt32				GetAuthMethodConstant		(	sPSContextData *inContext, 
                                                        tDataNode *inData,
														UInt32 *outAuthMethod,
                                                        char *outNativeAuthMethodSASLName );
                                                        
    bool				RequiresSASLAuthentication	(	UInt32 inAuthMethodConstant );
	
    SInt32				GetAuthMethodSASLName		(	sPSContextData *inContext, 
														UInt32 inAuthMethodConstant,
                                                        bool inAuthOnly,
                                                        char *outMechName,
														bool *outMethodCanSetPassword );
    
	void				GetAuthMethodFromSASLName	(	const char *inMechName,
														char *outDSType );
	
	SInt32				DoSASLNew					(	sPSContextData *inContext,
														sPSContinueData *inContinue );
	
	SInt32				DoSASLAuth					(	sPSContextData *inContext,
														char *userName,
														const char *password,
														long inPasswordLen,
														const char *inChallenge,
														const char *inMechName,
														sDoDirNodeAuth *inData,
														char **outStepData );
		
	SInt32				DoSASLTwoWayRandAuth		(	sPSContextData *inContext,
														const char *userName,
														const char *inMechName,
														sDoDirNodeAuth *inData );
	
	tDirStatus			DoSASLPPSAuth				(	sPSContextData *inContext,
														const char *inUserName,
														const char *inMechData,
														long inMechDataLen,
														sDoDirNodeAuth *inData );
	
	static
	PWServerError		SendFlushReadWithMutex		( sPSContextData *inContext,
														const char *inCommandStr,
														const char *inArg1Str,
														const char *inArg2Str,
														char *inOutBuf,
														unsigned long inBufLen );
	SInt32				Reconnect					(	sPSContextData *inContext,
														const char *userName);
	PWServerError		SendFlushReadWithMutexWithRetry( sPSContextData *inContext,
														const char *userName,
														const char *inCommandStr,
														const char *inArg1Str,
														const char *inArg2Str,
														char *inOutBuf,
														unsigned long inBufLen );

	
	SInt32				GetServerListFromDSDiscovery(	CFMutableArrayRef inOutServerList );
	
	SInt32				DoPlugInCustomCall			(	sDoPlugInCustomCall *inData );
	
private:
	UInt32				fState;
	UInt32				fSignature;
	SInt32				fOpenNodeCount;
	bool				fCalledSASLInit;
	char				fSASLMechPriority[256];
};

#endif	// __CPSPlugIn_h__
