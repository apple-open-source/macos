/*
 * These functions implement the KClient 3.0 API.
 * See KClient30-API.html.
 *
 * $Header: /cvs/kfm/KerberosFramework/KClient/Sources/KClient/KClient.cp,v 1.30 2003/02/26 04:08:54 lxs Exp $
 */

#include "KClientSession.h"
#include "KClientPrincipal.h"
#include "StPointerReturnValue.h"

const	char	kVersionString[]	= "KClient 3.0";

// Helper functions for error handling
inline bool IsKerberos4Error (OSStatus	err) {
	return (err >= kcFirstKerberosError && err <= kcLastKerberosError);
}

inline bool IsKClientLoginError (OSStatus	err) {
	return (err >= klFirstError && err <= klLastError);
}

inline OSStatus RemapProfileError (OSStatus /* err */) {
	return kcErrInvalidPreferences;
}

//
// KClient API functions
// 
// Most of these call through to their private implementation, and
// wrap appropriate exception handling and argument decoding around
// the internal API
//

OSStatus KClientGetVersion (
	UInt16*				outMajorVersion,
	UInt16*				outMinorVersion,
	const char**		outVersionString)
{
	if (outMajorVersion != NULL) {
        *outMajorVersion = 3;
	}
	if (outMinorVersion != NULL) {
        *outMinorVersion = 0;
    }
	if (outVersionString != NULL) {    
        *outVersionString = kVersionString;
	}
	return noErr;
}


OSStatus
KClientNewClientSession (
	KClientSession*			outSession)
{
	OSStatus err;
	
	BeginShieldedTry_ {
		StKClientSession 			session (new KClientSessionPriv ());
		StRawKClientSession	result = outSession;
		*result = session;
		err = noErr;
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = kcErrNoMemory;
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == kcNoError)
	         || (err == kcErrInvalidPreferences)
	         || (err == kcErrBadParam)
	         || (err == kcErrNoMemory));

	return err;
}

/*---------------------------------------------------------------------------------------------------*/
OSStatus
KClientNewServerSession (
	KClientSession*			outSession,
	KClientPrincipal		inServicePrincipal)
{
	OSStatus err = noErr;
	
	BeginShieldedTry_ {
		StKClientPrincipal 			service (inServicePrincipal);
		StKClientSession 			session (new KClientSessionPriv (service));
		StRawKClientSession	result = outSession;
		*result = session;
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = kcErrNoMemory;
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == kcNoError)
	         || (err == kcErrInvalidPreferences)
	         || (err == kcErrBadParam)
	         || (err == kcErrInvalidPrincipal)
	         || (err == kcErrNoMemory));

	return err;
}

/*---------------------------------------------------------------------------------------------------*/
OSStatus
KClientDisposeSession (
	KClientSession			inSession)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientSession 		session (inSession);
		KClientSessionPriv&		sessionPriv = session;

		delete &sessionPriv;
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == kcNoError)
	         || (err == kcErrInvalidSession));

	return err;
}

/*---------------------------------------------------------------------------------------------------*/
OSStatus KClientGetClientPrincipal (
	KClientSession		inSession,
	KClientPrincipal*	outPrincipal)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientSession 				session (inSession);
		StKClientPrincipal 				principal (
			new UPrincipal (session -> GetClientPrincipal ().Clone ()));
		StRawKClientPrincipal	result = outPrincipal;
		
		*result = principal;
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = kcErrNoMemory;
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == kcNoError)
	         || (err == kcErrInvalidPreferences)
	         || (err == kcErrBadParam)
	         || (err == kcErrInvalidSession)
	         || (err == kcErrNoClientPrincipal)
	         || (err == kcErrNoMemory));

	return err;
}
		
/*---------------------------------------------------------------------------------------------------*/
OSStatus KClientSetClientPrincipal (
	KClientSession		inSession,
	KClientPrincipal	inPrincipal)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientSession session (inSession);
		StKClientPrincipal principal (inPrincipal);
		
		session -> SetClientPrincipal (principal);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = kcErrNoMemory;
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == kcNoError)
	         || (err == kcErrInvalidPreferences)
	         || (err == kcErrInvalidSession)
	         || (err == kcErrInvalidPrincipal)
	         || (err == kcErrNoMemory));

	return err;
}
	
/*---------------------------------------------------------------------------------------------------*/
OSStatus KClientGetServerPrincipal (
	KClientSession		inSession,
	KClientPrincipal*	outPrincipal)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientSession session (inSession);
		StKClientPrincipal principal (
			new UPrincipal (session -> GetServerPrincipal ().Clone ()));
		StRawKClientPrincipal	result = outPrincipal;
		
		*result = principal;
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = kcErrNoMemory;
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == kcNoError)
	         || (err == kcErrInvalidPreferences)
	         || (err == kcErrBadParam)
	         || (err == kcErrInvalidSession)
	         || (err == kcErrNoServerPrincipal)
	         || (err == kcErrNoMemory));

	return err;
}
		
/*---------------------------------------------------------------------------------------------------*/
OSStatus KClientSetServerPrincipal (
	KClientSession		inSession,
	KClientPrincipal	inPrincipal)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientSession session (inSession);
		StKClientPrincipal principal (inPrincipal);
		
		session -> SetServerPrincipal (principal);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = kcErrNoMemory;
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == kcNoError)
	         || (err == kcErrInvalidPreferences)
	         || (err == kcErrInvalidSession)
	         || (err == kcErrInvalidPrincipal)
	         || (err == kcErrNoMemory));

	return err;
}
	
/*---------------------------------------------------------------------------------------------------*/
OSStatus KClientGetLocalAddress (
	KClientSession		inSession,
	KClientAddress*		outAddress)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientSession 			session (inSession);
		StRawKClientAddress	result = outAddress;
		
		*result = session -> GetLocalAddress ();
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = kcErrNoMemory;
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == kcNoError)
	         || (err == kcErrInvalidPreferences)
	         || (err == kcErrBadParam)
	         || (err == kcErrInvalidSession)
	         || (err == kcErrNoLocalAddress)
	         || (err == kcErrNoMemory));

	return err;
}
		
/*---------------------------------------------------------------------------------------------------*/
OSStatus KClientSetLocalAddress (
	KClientSession			inSession,
	const KClientAddress*	inAddress)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientSession session (inSession);
		
		session -> SetLocalAddress (*inAddress);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = kcErrNoMemory;
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == kcNoError)
	         || (err == kcErrInvalidPreferences)
	         || (err == kcErrInvalidSession)
	         || (err == kcErrInvalidAddress)
	         || (err == kcErrNoMemory));

	return err;
}
		
/*---------------------------------------------------------------------------------------------------*/
OSStatus KClientGetRemoteAddress (
	KClientSession		inSession,
	KClientAddress*		outAddress)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientSession session (inSession);
		StRawKClientAddress	result = outAddress;
		
		*result = session -> GetRemoteAddress ();
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = kcErrNoMemory;
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == kcNoError)
	         || (err == kcErrInvalidPreferences)
	         || (err == kcErrBadParam)
	         || (err == kcErrInvalidSession)
	         || (err == kcErrNoLocalAddress)
	         || (err == kcErrNoMemory));

	return err;
}
		
/*---------------------------------------------------------------------------------------------------*/
OSStatus KClientSetRemoteAddress (
	KClientSession			inSession,
	const KClientAddress*	inAddress)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientSession session (inSession);
		
		session -> SetRemoteAddress (*inAddress);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = kcErrNoMemory;
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == kcNoError)
	         || (err == kcErrInvalidPreferences)
	         || (err == kcErrInvalidSession)
	         || (err == kcErrInvalidAddress)
	         || (err == kcErrNoMemory));

	return err;
}
		
/*---------------------------------------------------------------------------------------------------*/
OSStatus KClientGetSessionKey (
	KClientSession		inSession,
	KClientKey*			outKey)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientSession session (inSession);
		StRawKClientKey	result = outKey;
		
		*result = session -> GetSessionKey ();
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == kcNoError)
	         || (err == kcErrInvalidPreferences)
	         || (err == kcErrInvalidSession)
	         || (err == kcErrBadParam)
	         || (err == kcErrNoServiceKey)
	         || IsKClientLoginError (err));

	return err;
}

/*---------------------------------------------------------------------------------------------------*/
OSStatus KClientGetExpirationTime (
	KClientSession		inSession,
	UInt32*				outExpiration)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientSession 	session (inSession);
		StRawUInt32	result = outExpiration;
		
		session -> GetExpirationTime (*result);
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == kcNoError)
	         || (err == kcErrInvalidPreferences)
	         || (err == kcErrInvalidSession)
	         || (err == kcErrBadParam)
	         || (err == kcErrNotLoggedIn));
	return err;
}
	
/*---------------------------------------------------------------------------------------------------*/
OSStatus KClientSetKeyFile (
	KClientSession		inSession,
	const KClientFile*	inKeyFile)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientSession session (inSession);
		session -> SetKeyFile (*inKeyFile);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = kcErrNoMemory;
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == kcNoError)
	         || (err == kcErrInvalidPreferences)
	         || (err == kcErrInvalidSession)
	         || (err == kcErrInvalidFile)
	         || (err == kcErrBadParam));
	return err;
}
	
/* Logging in and out (client) */
/*---------------------------------------------------------------------------------------------------*/
OSStatus KClientLogin (
	KClientSession		inSession)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientSession session (inSession);
		session -> Login ();
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = kcErrNoMemory;
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;

	AssertReturnValue_ ((err == kcNoError)
	         || (err == kcErrInvalidPreferences)
	         || (err == kcErrNoMemory)
	         || (err == kcErrUserCancelled)
	         || (err == kcErrInvalidPreferences)
	         || IsKClientLoginError (err));
	return err;
}

/*---------------------------------------------------------------------------------------------------*/
OSStatus KClientPasswordLogin (
	KClientSession		inSession,
	const char* 		inPassword)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientSession session (inSession);
		session -> Login (inPassword);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = kcErrNoMemory;
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == kcNoError)
	         || (err == kcErrInvalidPreferences)
	         || (err == kcErrNoMemory)
	         || (err == kcErrIncorrectPassword)
	         || (err == kcErrInvalidPreferences)
	         || IsKClientLoginError (err));
	return err;
}
	
/*---------------------------------------------------------------------------------------------------*/
OSStatus KClientKeyFileLogin (
	KClientSession		inSession)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientSession session (inSession);
		session -> KeyFileLogin ();
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = kcErrNoMemory;
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;
	
	#warning review the error list
	AssertReturnValue_ ((err == kcNoError)
	         || (err == kcErrInvalidPreferences)
	         || (err == kcErrNoMemory)
	         || (err == kcErrIncorrectPassword)
	         || (err == kcErrInvalidPreferences)
	         || IsKClientLoginError (err));
	return err;
}
	
/*---------------------------------------------------------------------------------------------------*/
OSStatus KClientLogout (
	KClientSession		inSession)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientSession session (inSession);
		session -> Logout ();
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = kcErrNoMemory;
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == kcNoError)
	         || (err == kcErrInvalidPreferences)
	         || (err == kcErrInvalidSession)
	         || (err == kcErrNoMemory)
	         || (err == kcErrIncorrectPassword)
	         || IsKClientLoginError (err));
	return err;
}
	
/* Accessing service keys (server) */

OSStatus KClientGetServiceKey (
	KClientSession		inSession,
	UInt32				inVersion,
	KClientKey*			outKey)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientSession		session (inSession);
		StRawKClientKey			keyResult = outKey;

		*keyResult = session -> GetServiceKey (inVersion);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = kcErrNoMemory;
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == kcNoError)
	         || (err == kcErrInvalidPreferences)
	         || (err == kcErrInvalidSession)
	         || (err == kcErrBadParam)
	         || (err == kcErrNoMemory)
	         || (err == kcErrKeyFileAccess));
	return err;
}

	
OSStatus KClientAddServiceKey (
	KClientSession		inSession,
	UInt32				inVersion,
	const KClientKey*	inKey)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientSession session (inSession);
		session -> AddServiceKey (inVersion, *inKey);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = kcErrNoMemory;
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == kcNoError)
	         || (err == kcErrInvalidPreferences)
	         || (err == kcErrInvalidSession)
	         || (err == kcErrBadParam)
	         || (err == kcErrNoMemory)
	         || (err == kcErrKeyFileAccess));
	return err;
}
	
/* Authenticating to a service (client) */

OSStatus KClientGetTicketForService (
	KClientSession		inSession,
	UInt32				inChecksum,
	void*				outBuffer,
	UInt32*				ioBufferLength)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientSession 	session (inSession);
		StRawUInt32	lengthResult = ioBufferLength;
		StRawBuffer	bufferResult = outBuffer;
		
		session -> GetTicketForService (inChecksum, *bufferResult, *lengthResult);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = kcErrNoMemory;
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == kcNoError)
	         || (err == kcErrInvalidPreferences)
	         || (err == kcErrInvalidSession)
	         || (err == kcErrBadParam)
	         || (err == kcErrBufferTooSmall)
	         || (err == kcErrInvalidPreferences)
	         || (err == kcErrNoMemory)
	         || IsKerberos4Error (err)
	         || IsKClientLoginError (err));
	return err;
}

	
OSStatus KClientGetAuthenticatorForService (
	KClientSession		inSession,
	UInt32				inChecksum,
	const char*			inApplicationVersion,
	void*				outBuffer,
	UInt32*				ioBufferLength)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientSession session (inSession);
		StRawUInt32	lengthResult = ioBufferLength;
		StRawBuffer	bufferResult = outBuffer;
		
		session -> GetAuthenticatorForService (inChecksum, inApplicationVersion, *bufferResult, *lengthResult);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = kcErrNoMemory;
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == kcNoError)
	         || (err == kcErrInvalidPreferences)
	         || (err == kcErrInvalidSession)
	         || (err == kcErrBadParam)
	         || (err == kcErrBufferTooSmall)
	         || (err == kcErrInvalidPreferences)
	         || (err == kcErrNoMemory)
	         || IsKerberos4Error (err)
	         || IsKClientLoginError (err));
	return err;
}

OSStatus KClientVerifyEncryptedServiceReply (
	KClientSession		inSession,
	const void*			inBuffer,
	UInt32				inBufferLength)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientSession	session (inSession);
		StRawConstBuffer	buffer = inBuffer;

		session -> VerifyEncryptedServiceReply (*buffer, inBufferLength);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = kcErrNoMemory;
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == kcNoError)
	         || (err == kcErrInvalidPreferences)
	         || (err == kcErrInvalidSession)
	         || (err == kcErrBadParam)
	         || (err == kcErrNoMemory)
	         || IsKerberos4Error (err));
	return err;
}
	
OSStatus KClientVerifyProtectedServiceReply (
	KClientSession		inSession,
	const void*			inBuffer,
	UInt32				inBufferLength)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientSession 	session (inSession);
		StRawConstBuffer	buffer = inBuffer;

		session -> VerifyProtectedServiceReply (*buffer, inBufferLength);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = kcErrNoMemory;
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == kcNoError)
	         || (err == kcErrInvalidPreferences)
	         || (err == kcErrInvalidSession)
	         || (err == kcErrBadParam)
	         || (err == kcErrNoMemory)
	         || IsKerberos4Error (err));
	return err;
}
	
/* Authenticating a client (server) */

OSStatus KClientVerifyAuthenticator (
	KClientSession		inSession,
	const void*			inBuffer,
	UInt32				inBufferLength)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientSession	session (inSession);
		StRawConstBuffer	buffer = inBuffer;

		session -> VerifyAuthenticator (*buffer, inBufferLength);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = kcErrNoMemory;
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;

	#warning handle	bad key file errors
	AssertReturnValue_ ((err == kcNoError)
	         || (err == kcErrInvalidPreferences)
	         || (err == kcErrNoSessionKey)
	         || (err == kcErrInvalidSession)
	         || (err == kcErrBadParam)
	         || (err == kcErrBufferTooSmall)
	         || (err == kcErrNoMemory)
	         || IsKerberos4Error (err));
	return err;
}
	
OSStatus KClientGetEncryptedServiceReply (
	KClientSession		inSession,
	void*				outBuffer,
	UInt32*				ioBufferSize)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientSession	session (inSession);
		StRawBuffer			buffer = outBuffer;
		StRawUInt32			bufferSize = ioBufferSize;

		session -> GetEncryptedServiceReply (*buffer, *bufferSize);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = kcErrNoMemory;
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == kcNoError)
	         || (err == kcErrInvalidPreferences)
	         || (err == kcErrInvalidSession)
	         || (err == kcErrBadParam)
	         || (err == kcErrBufferTooSmall)
	         || (err == kcErrNoSessionKey)
	         || (err == kcErrNoMemory)
	         || IsKerberos4Error (err));
	return err;
}
	
OSStatus KClientGetProtectedServiceReply (
	KClientSession		inSession,
	void*				outBuffer,
	UInt32*				ioBufferSize)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientSession	session (inSession);
		StRawBuffer			buffer = outBuffer;
		StRawUInt32			bufferSize = ioBufferSize;

		session -> GetProtectedServiceReply (*buffer, *bufferSize);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = kcErrNoMemory;
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == kcNoError)
	         || (err == kcErrInvalidPreferences)
	         || (err == kcErrInvalidSession)
	         || (err == kcErrBadParam)
	         || (err == kcErrBufferTooSmall)
	         || (err == kcErrNoSessionKey)
	         || (err == kcErrNoMemory)
	         || IsKerberos4Error (err));
	return err;
}
	
/* Communicating between a server and a client */

OSStatus KClientEncrypt (
	KClientSession		inSession,
	const void*			inPlainBuffer,
	UInt32				inPlainBufferLength,
	void*				outEncryptedBuffer,
	UInt32*				ioEncryptedBufferLength)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientSession	session (inSession);
		StRawConstBuffer	plainBuffer = inPlainBuffer;
		StRawBuffer			encryptedBuffer = outEncryptedBuffer;
		StRawUInt32			encryptedBufferLength = ioEncryptedBufferLength;

		session -> Encrypt (*plainBuffer, inPlainBufferLength, *encryptedBuffer, *encryptedBufferLength);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = kcErrNoMemory;
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == kcNoError)
	         || (err == kcErrInvalidPreferences)
	         || (err == kcErrInvalidSession)
	         || (err == kcErrBadParam)
	         || (err == kcErrBufferTooSmall)
	         || (err == kcErrNoSessionKey)
	         || (err == kcErrNoMemory)
	         || IsKerberos4Error (err));
	return err;
}

OSStatus KClientDecrypt (
	KClientSession		inSession,
	void*				inEncryptedBuffer,
	UInt32				inEncryptedBufferLength,
	UInt32*				outPlainOffset,
	UInt32*				outPlainLength)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientSession	session (inSession);
		StRawBuffer			encryptedBuffer = inEncryptedBuffer;
		StRawUInt32			plainOffset = outPlainOffset;
		StRawUInt32			plainLength = outPlainLength;

		session -> Decrypt (*encryptedBuffer, inEncryptedBufferLength, *plainOffset, *plainLength);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = kcErrNoMemory;
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == kcNoError)
	         || (err == kcErrInvalidPreferences)
	         || (err == kcErrInvalidSession)
	         || (err == kcErrBadParam)
	         || (err == kcErrNoSessionKey)
	         || (err == kcErrNoMemory)
	         || IsKerberos4Error (err));
	return err;
}
	
OSStatus KClientProtectIntegrity (
	KClientSession		inSession,
	const void*			inPlainBuffer,
	UInt32				inPlainBufferLength,
	void*				outProtectedBuffer,
	UInt32*				ioProtectedBufferLength)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientSession	session (inSession);
		StRawConstBuffer	plainBuffer = inPlainBuffer;
		StRawBuffer			protectedBuffer = outProtectedBuffer;
		StRawUInt32			protectedBufferLength = ioProtectedBufferLength;

		session -> ProtectIntegrity (*plainBuffer, inPlainBufferLength, *protectedBuffer, *protectedBufferLength);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = kcErrNoMemory;
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == kcNoError)
	         || (err == kcErrInvalidPreferences)
	         || (err == kcErrInvalidSession)
	         || (err == kcErrBadParam)
	         || (err == kcErrBufferTooSmall)
	         || (err == kcErrNoSessionKey)
	         || (err == kcErrNoMemory)
	         || IsKerberos4Error (err));
	return err;
}

OSStatus KClientVerifyIntegrity (
	KClientSession		inSession,
	void*				inProtectedBuffer,
	UInt32				inProtectedBufferLength,
	UInt32*				outPlainOffset,
	UInt32*				outPlainLength)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientSession	session (inSession);
		StRawBuffer			protectedBuffer = inProtectedBuffer;
		StRawUInt32			plainOffset = outPlainOffset;
		StRawUInt32			plainLength = outPlainLength;

		session -> VerifyIntegrity (*protectedBuffer, inProtectedBufferLength, *plainOffset, *plainLength);
	} ShieldedCatch_ (std::bad_alloc& e) {
		err = kcErrNoMemory;
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == kcNoError)
	         || (err == kcErrInvalidPreferences)
	         || (err == kcErrInvalidSession)
	         || (err == kcErrBadParam)
	         || (err == kcErrNoSessionKey)
	         || (err == kcErrNoMemory)
	         || IsKerberos4Error (err));
	return err;
}
	
/* Getting to other APIs */

OSStatus KClientGetCCacheReference (
	KClientSession		inSession,
	cc_ccache_t*		outCCacheReference)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientSession session (inSession);
		*outCCacheReference = session -> GetCCacheReference ().Release ();
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} EndShieldedTry_;
	
	AssertReturnValue_ (err == noErr);
	return err;
}	

OSStatus KClientGetProfileHandle (
	KClientSession		inSession,
	profile_t*			outProfileHandle);

OSStatus KClientV4StringToPrincipal (
	const char*			inString,
	KClientPrincipal*	outPrincipal)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientPrincipal principal (new UPrincipal (UPrincipal::kerberosV4, inString));
		*outPrincipal = principal;
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == noErr)
	         || (err == kcErrInvalidPreferences));
	return err;
}

OSStatus KClientPrincipalToV4String (
	KClientPrincipal	inPrincipal,
	char*				outString)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientPrincipal principal (inPrincipal);
		std::string	string = principal -> GetString (UPrincipal::kerberosV4);
		strcpy (outString, string.c_str ());
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == noErr)
	         || (err == kcErrInvalidPreferences));
	return err;
}

OSStatus KClientPrincipalToV4Triplet (
	KClientPrincipal	inPrincipal,
	char*				outName,
	char*				outInstance,
	char*				outRealm)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientPrincipal principal (inPrincipal);
		std::string	name;
		std::string	instance;
		std::string	realm;
		principal -> GetTriplet (UPrincipal::kerberosV4, name, instance, realm);
		strcpy (outName, name.c_str ());
		strcpy (outInstance, instance.c_str ());
		strcpy (outRealm, realm.c_str ());
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == noErr)
	         || (err == kcErrInvalidPreferences));
	return err;
}

OSStatus
KClientDisposePrincipal (
	KClientPrincipal			inPrincipal)
{
	OSStatus err = noErr;
	BeginShieldedTry_ {
		StKClientPrincipal principal (inPrincipal);
		UPrincipal& principalPriv = principal;
		delete &principalPriv;
	} ShieldedCatch_ (KClientError& e) {
		err = e.ErrorCode ();
	} ShieldedCatch_ (UProfileRuntimeError& e) {
		err = RemapProfileError (e.Error ());
	} EndShieldedTry_;
	
	AssertReturnValue_ ((err == noErr)
	         || (err == kcErrInvalidPreferences));
	return err;
}
