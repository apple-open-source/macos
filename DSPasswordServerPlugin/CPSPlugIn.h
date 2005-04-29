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
	
	#include "sasl.h"
	#include "saslutil.h"
	
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

#include "sasl.h"
#include "AuthFile.h"
#include "CReplicaFile.h"
#include "CPSUtilities.h"
#include "CPSPluginDefines.h"

#define kUserIDLength						34
#define kMaxOpenNodesBeforeQuickClose		100
#define kSendQuit							true


#define kChangePassPaddedBufferSize			512
#define kOneKBuffer							1024


#define Throw_NULL(A,B)			if ((A)==NULL) throw((sInt32)B)

#if 1
#define DEBUGLOG(A,args...)		CShared::LogIt( 0x0F, (A), ##args )
#else
#define DEBUGLOG(A,args...)		
#endif

enum {
	kAuthDIGEST_MD5_Reauth		= 153,
	kAuthPull					= 156,
	kAuthPush					= 157,
	kAuthProcessNoReply			= 158,
	kAuthNTLMv2SessionKey		= 165
};

typedef enum NewUserParamListType {
	kNewUserParamsNone,
	kNewUserParamsPolicy,
	kNewUserParamsPrincipalName,
	kNewUserParamsPrincipalNameAndPolicy
};

class CPSPlugIn : public CDSServerModule
{
public:
						CPSPlugIn					(	void );
	virtual			   ~CPSPlugIn					(	void );

	virtual sInt32		Validate					(	const char *inVersionStr,
														const uInt32 inSignature );
	virtual sInt32		Initialize					(	void );
	virtual sInt32		ProcessRequest				(	void *inData );
	virtual sInt32		SetPluginState				(	const uInt32 inState );
	static void			ContinueDeallocProc			(	void* inContinueData );
	static void			ContextDeallocProc			(	void* inContextData );
	
protected:
	void				WakeUpRequests				(	void );
	void				WaitForInit					(	void );
	sInt32				HandleRequest				(	void *inData );
	sInt32				ReleaseContinueData			(	sReleaseContinueData *inData );
    sInt32				OpenDirNode					(	sOpenDirNode *inData );
    sInt32				CloseDirNode				(	sCloseDirNode *inData );
	sInt32				GetDirNodeInfo				(	sGetDirNodeInfo *inData );
	sInt32				HandleFirstContact			(	sPSContextData *inContext,
														const char *inIP,
														const char *inUserKeyHash,
														const char *inUserKeyStr = NULL,
														bool inSecondTime = false );
	sInt32				BeginServerSession			(	sPSContextData *inContext, int inSock, const char *inUserKeyHash );
	static sInt32		EndServerSession			(	sPSContextData *inContext, bool inSendQuit = false );
    sInt32				GetRSAPublicKey				(	sPSContextData *inContext, char *inData = NULL );
    bool				RSAPublicKeysEqual			(	const char *rsaKeyStr1, const char *rsaKeyStr2 );
	sInt32				DoRSAValidation				(	sPSContextData *inContext, const char *inUserKey );
	sInt32				SetupSecureSyncSession		(	sPSContextData *inContext );
	static bool			SecureSyncSessionIsSetup	(	sPSContextData *inContext );
	sInt32				GetAttributeEntry			(	sGetAttributeEntry *inData );
	sInt32				GetAttributeValue			(	sGetAttributeValue *inData );
	uInt32				CalcCRC						(	char *inStr );
    static sInt32		CleanContextData			(	sPSContextData *inContext );
    sPSContextData	   *MakeContextData				(	void );
	sInt32				CloseAttributeList			(	sCloseAttributeList *inData );
	sInt32				CloseAttributeValueList		(	sCloseAttributeValueList *inData );
    sInt32				GetStringFromAuthBuffer		(	tDataBufferPtr inAuthData,
                                                        int stringNum,
                                                        char **outString );
	sInt32				Get2StringsFromAuthBuffer	(	tDataBufferPtr inAuthData, char **outString1, char **outString2 );
	
    sInt32				GetDataFromAuthBuffer		(	tDataBufferPtr inAuthData,
                                                        int nodeNum,
                                                        unsigned char **outData,
                                                        long *outLen );
    
	void				UpdateCachedPasswordOnChange (   sPSContextData *inContext,
														const char *inChangedUser,
														const char *inPassword,
														long inPasswordLen );
	
	sInt32				DoAuthentication			(	sDoDirNodeAuth *inData );

	// ------------------------------------------------------------------------------------------------

	sInt32				DoAuthMethodNewUser			(   sDoDirNodeAuth *inData,
														sPSContextData *inContext,
														bool inWithPolicy,
														tDataBufferPtr outBuf );
	sInt32				DoAuthMethodListReplicas	(   sDoDirNodeAuth *inData,
														sPSContextData *inContext,
														tDataBufferPtr outBuf );
    sInt32				DoAuthMethodPull			(	sDoDirNodeAuth *inData,
														sPSContextData *inContext,
														tDataBufferPtr outBuf );
    sInt32				DoAuthMethodPush			(	sDoDirNodeAuth *inData,
														sPSContextData *inContext,
														tDataBufferPtr outBuf );
	sInt32				DoAuthMethodNTUserSessionKey(	sDoDirNodeAuth *inData,
														sPSContextData *inContext,
														tDataBufferPtr outBuf );
	sInt32				DoAuthMethodMSChapChangePass(	sDoDirNodeAuth *inData, sPSContextData *inContext );
	sInt32				DoAuthMethodEncryptToUser	(	sDoDirNodeAuth *inData,
														sPSContextData *inContext,
														tDataBufferPtr outBuf );
	sInt32				DoAuthMethodDecrypt			(	sDoDirNodeAuth *inData,
														sPSContextData *inContext,
														tDataBufferPtr outBuf );
	sInt32				DoAuthMethodSetHash			(	sDoDirNodeAuth *inData, sPSContextData *inContext, const char *inCommandStr );
	sInt32				DoAuthMethodNTLMv2SessionKey(   sDoDirNodeAuth *inData, sPSContextData *inContext, tDataBufferPtr outBuf );
	
	// ------------------------------------------------------------------------------------------------
	
	sInt32				GetReplicaListFromServer	(   sPSContextData *inContext,
														char **outData,
														unsigned long *outDataLen );
	sInt32				UseCurrentAuthenticationIfPossible
													(	sPSContextData *inContext,
														const char *inUserName,
														UInt32 inAuthMethod,
														Boolean *inOutHasValidAuth );
	
	sInt32				PackStepBuffer				(	const char *inArg1,
														bool inUseBuffPlus4, 
														const char *inArg2,
														const char *inArg3,
														const char *inArg4,
														tDataBufferPtr inOutDataBuffer );
	
	sInt32				UnpackUsernameAndPassword	(	sPSContextData *inContext,
                                                        uInt32 uiAuthMethod,
                                                        tDataBufferPtr inAuthBuf,
                                                        char **outUserName,
                                                        char **outPassword,
                                                        long *outPasswordLen,
                                                        char **outChallenge );
														
	sInt32				UnpackUsernameAndPasswordDefault
													(	tDataBufferPtr inAuthBuf,
                                                        char **outUserName,
                                                        char **outPassword,
                                                        long *outPasswordLen );
														
    sInt32				GetAuthMethodConstant		(	sPSContextData *inContext, 
                                                        tDataNode *inData,
														uInt32 *outAuthMethod,
                                                        char *outNativeAuthMethodSASLName );
                                                        
    bool				RequiresSASLAuthentication	(	uInt32 inAuthMethodConstant );
	
    sInt32				GetAuthMethodSASLName		(	uInt32 inAuthMethodConstant,
                                                        bool inAuthOnly,
                                                        char *outMechName,
														bool *outMethodCanSetPassword );
    
	void				GetAuthMethodFromSASLName	(	const char *inMechName,
														char *outDSType );
	
	sInt32				DoSASLNew					(	sPSContextData *inContext,
														sPSContinueData *inContinue );
	
	sInt32				DoSASLAuth					(	sPSContextData *inContext,
														char *userName,
														const char *password,
														long inPasswordLen,
														const char *inChallenge,
														const char *inMechName,
														sDoDirNodeAuth *inData,
														char **outStepData );
		
	sInt32				DoSASLTwoWayRandAuth		(	sPSContextData *inContext,
														const char *userName,
														const char *inMechName,
														sDoDirNodeAuth *inData );
	
	static
	PWServerError		SendFlushReadWithMutex		( sPSContextData *inContext,
														const char *inCommandStr,
														const char *inArg1Str,
														const char *inArg2Str,
														char *inOutBuf,
														unsigned long inBufLen );
	
	sInt32				GetServerListFromDSDiscovery(	CFMutableArrayRef inOutServerList );
	sInt32				PWSErrToDirServiceError		(	PWServerError inError );
	sInt32				SASLErrToDirServiceError	(	int inSASLError );
	sInt32				PolicyErrToDirServiceError	(	int inPolicyError );
	
	sInt32				DoPlugInCustomCall			(	sDoPlugInCustomCall *inData );
	
private:
	uInt32				fState;
	uInt32				fSignature;
	sInt32				fOpenNodeCount;
	bool				fCalledSASLInit;
};

#endif	// __CPSPlugIn_h__
