/*
	File:		CPSPlugIn.h

	Contains:	PasswordServer plugin definitions

	Copyright:	© 2002 by Apple Computer, Inc., all rights reserved.

	NOT_FOR_OPEN_SOURCE <to be reevaluated at a later time>

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

#include "sasl.h"


#define kDHX_SASL_Name			"DHX"
#define kAuthNative_Priority	"DIGEST-MD5 CRAM-MD5 DHX TWOWAYRANDOM"

#define kUserIDLength			34
#define kMaxUserNameLength		255

enum {
	kAuthUnknownMethod		= 127,
	kAuthClearText			= 128,
	kAuthCrypt				= 129,
	kAuthSetPasswd			= 130,
	kAuthChangePasswd		= 131,
	kAuthAPOP				= 132,
	kAuth2WayRandom			= 133,
	kAuthNativeClearTextOK	= 134,
	kAuthNativeNoClearText	= 135,
	kAuthSMB_NT_Key			= 136,
	kAuthSMB_LM_Key			= 137,
    kAuthDIGEST_MD5			= 138,
    kAuthCRAM_MD5			= 139,
    
    kAuthGetPolicy			= 140,
    kAuthSetPolicy			= 141,
    kAuthGetGlobalPolicy	= 142,
    kAuthSetGlobalPolicy	= 143,
    kAuthGetUserName		= 144,
    kAuthSetUserName		= 145,
    kAuthGetUserData		= 146,
    kAuthSetUserData		= 147,
    kAuthDeleteUser			= 148,
    kAuthNewUser			= 149,
    
    kAuthSetPasswdAsRoot	= 150,
    kAuthGetIDByName		= 151,
    
	kAuth2WayRandomChangePass	= 152,
	kAuthDIGEST_MD5_Reauth		= 153,
	
	kAuthNativeMethod		= 154
};

// Reposonse Codes (used numerically)
enum {
    kAuthOK = 0,
    kAuthFail = -1,
    kAuthUserDisabled = -2,
    kAuthNeedAdminPrivs = -3,
    kAuthUserNotSet = -4,
    kAuthUserNotAuthenticated = -5,
    kAuthPasswordExpired = -6,
    kAuthPasswordNeedsChange = -7,
    kAuthPasswordNotChangeable = -8,
    kAuthPasswordTooShort = -9,
    kAuthPasswordTooLong = -10,
    kAuthPasswordNeedsAlpha = -11,
    kAuthPasswordNeedsDecimal = -12,
    kAuthMethodTooWeak = -13
};

typedef enum PWServerErrorType {
	kPolicyError,
	kSASLError,
	kConnectionError
} PWServerErrorType;

typedef struct PWServerError {
    int err;
    PWServerErrorType type;
} PWServerError;

typedef struct AuthMethName {
    char method[SASL_MECHNAMEMAX + 1];
} AuthMethName;

typedef struct AuthInfo {
    char username[kMaxUserNameLength + 1];
    char *password;
    long passwordLen;
    Boolean successfulAuth;
} AuthInfo;

// Context data structure
typedef struct sPSContextData {
	char	   *psName;										// domain or ip address of passwordserver
	char		psPort[10];									// port # of the password server
	uInt32		offset;										// offset for GetDirNodeInfo data extraction
	char		localaddr[NI_MAXHOST + NI_MAXSERV + 1];
	char		remoteaddr[NI_MAXHOST + NI_MAXSERV + 1];
    
    sasl_conn_t *conn;
    FILE *serverIn, *serverOut;
    int fd;
	sasl_callback_t callbacks[5];
    
	char *rsaPublicKeyStr;
    Key *rsaPublicKey;
	
    AuthMethName *mech;
    int mechCount;
    
    AuthInfo last;						// information for the current authorization
    AuthInfo nao;						// information for the last authorization that was not "auth-only"
    
} sPSContextData;


typedef struct {
	uInt32				fAuthPass;
	unsigned char*		fData;
	unsigned long		fDataLen;
	sasl_secret_t*		fSASLSecret;
	char				fUsername[kMaxUserNameLength + 1];
} sPSContinueData;


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
	
    virtual void		FillWithRandomData			(	char *inBuffer, uInt32 inLen );
    
protected:
	void				WakeUpRequests				(	void );
	void				WaitForInit					(	void );
	sInt32				HandleRequest				(	void *inData );
	sInt32				ReleaseContinueData			(	sReleaseContinueData *inData );
    sInt32				OpenDirNode					(	sOpenDirNode *inData );
    sInt32				CloseDirNode				(	sCloseDirNode *inData );
	sInt32				GetDirNodeInfo				(	sGetDirNodeInfo *inData );
    sInt32				ConnectToServer				(	sPSContextData *inContext );
	Boolean				Connected					(	sPSContextData *inContext );
    sInt32				GetRSAPublicKey				(	sPSContextData *inContext );
	sInt32				DoRSAValidation				(	sPSContextData *inContext, const char *inUserKey );
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
    sInt32				GetDataFromAuthBuffer		(	tDataBufferPtr inAuthData,
                                                        int nodeNum,
                                                        unsigned char **outData,
                                                        long *outLen );
    
	sInt32				DoAuthentication			(	sDoDirNodeAuth *inData );
    
    sInt32				UnpackUsernameAndPassword	(	sPSContextData *inContext,
                                                        uInt32 uiAuthMethod,
                                                        tDataBufferPtr inAuthBuf,
                                                        char **outUserName,
                                                        char **outPassword,
                                                        long *outPasswordLen,
                                                        char **outChallenge );
    
    void				StripRSAKey					(	char *inOutUserID );
	sInt32				GetAuthMethodConstant		(	sPSContextData *inContext, 
                                                        tDataNode *inData,
														uInt32 *outAuthMethod,
                                                        char *outNativeAuthMethodSASLName );
                                                        
    sInt32				GetAuthMethodSASLName		(	uInt32 inAuthMethodConstant,
                                                        bool inAuthOnly,
                                                        char *outMechName );
    
	void				GetAuthMethodFromSASLName	(	const char *inMechName,
														char *outDSType );
	
    sInt32				PWSErrToDirServiceError		(	PWServerError inError );
    sInt32				SASLErrToDirServiceError	(	int inSASLError );
    sInt32				PolicyErrToDirServiceError	(	int inPolicyError );
    
	sInt32				DoSASLAuth					(	sPSContextData *inContext,
														const char *userName,
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
	
	sInt32				DoPlugInCustomCall			(	sDoPlugInCustomCall *inData );
		
private:
	uInt32				fState;
	uInt32				fSignature;
    bool				fHasInitializedSASL;

};

#endif	// __CPSPlugIn_h__
