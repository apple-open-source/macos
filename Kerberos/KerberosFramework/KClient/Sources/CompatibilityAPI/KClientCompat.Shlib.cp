/*
 * KClientCompatLib implements parts of the old KClient API which can be
 * sanely mapped to the new API.
 *
 * $Header$
 */

#include <Kerberos/KClientCompat.h>
#include "KClientKerberosIntf.h"

// 
// KClient compatibility API
// Almost all the functions here just map old calls to new calls, with appropriate
// argument mangling
//

static void ExpirationTimeToTimeRemaining (UInt32*	ioTime);

static OSErr RemapKClientError (
	OSStatus	inError);
	
static OSStatus KClientKeyFileLocationFromFullPath (
	const char*		inFullPath,
	FSSpec*			outFileSpec,
	Boolean*		outUseDefault);

OSErr KClientVersionCompat (
	SInt16*						outMajorVersion,
	SInt16*						outMinorVersion,
	char*						outVersionString)
{
	const char*	versionString;
	UInt16	majorVersion;
	UInt16	minorVersion;
	
	OSStatus err = KClientGetVersion (&majorVersion, &minorVersion, &versionString);
	if (err == noErr) {
		strcpy (outVersionString, versionString);
		*outMajorVersion = (SInt16) majorVersion;
		*outMinorVersion = (SInt16) minorVersion;
	}
	
	return RemapKClientError (err);
}


OSErr KClientNewSessionCompat (
	KClientSessionInfo*			inSession,
	UInt32						inLocalAddress,
	UInt16						inLocalPort,
	UInt32						inRemoteAddress,
	UInt16						inRemotePort)
{
	KClientAddress	localAddress = {inLocalAddress, inLocalPort};
	KClientAddress	remoteAddress = {inRemoteAddress, inRemotePort};
	OSStatus err = KClientNewClientSession (inSession);
	if (err == noErr) {
		err = KClientSetLocalAddress (*inSession, &localAddress);
	}
	if (err == noErr) {
		err = KClientSetRemoteAddress (*inSession, &remoteAddress);
	}
	return RemapKClientError (err);
}
	
OSErr KClientDisposeSessionCompat (
	KClientSessionInfo*			inSession)
{
	return (OSErr) KClientDisposeSession (*inSession);
}
	
OSErr KClientGetTicketForServiceCompat (
	KClientSessionInfo*			inSession,
	char*						inService,
	void*						ioBuffer,
	UInt32*						ioBufferLength)
{
	UInt32				realBufferLength = (*ioBufferLength) - sizeof (UInt32);

	KClientPrincipal	service;
	OSStatus err = KClientV4StringToPrincipal (inService, &service);
	if (err == noErr) {
		err = KClientSetServerPrincipal (*inSession, service);
		KClientDisposePrincipal (service);
	}
	
	if (err == noErr) {
		err = KClientGetTicketForService (*inSession, 0,
			(char*) ioBuffer + sizeof (UInt32), &realBufferLength);
	}
	
	if (err == noErr) {
		*(UInt32*)ioBuffer = realBufferLength;
		*ioBufferLength = realBufferLength + 4;
	}
	
	return RemapKClientError (err);
}
	
OSErr KClientGetTicketForServiceWithChecksumCompat (
	KClientSessionInfo*			inSession,
	UInt32						inChecksum,
	char*						inService,
	void*						ioBuffer,
	UInt32*						ioBufferLength)
{
	UInt32				realBufferLength = (*ioBufferLength) - sizeof (UInt32);

	KClientPrincipal	service;
	OSStatus err = KClientV4StringToPrincipal (inService, &service);
	if (err == noErr) {
		err = KClientSetServerPrincipal (*inSession, service);
		KClientDisposePrincipal (service);
	}
	
	if (err == noErr) {
		err = KClientGetTicketForService (*inSession, inChecksum,
			(char*) ioBuffer + sizeof (UInt32), &realBufferLength);
	}
	
	if (err == noErr) {
		*(UInt32*)ioBuffer = realBufferLength;
		*ioBufferLength = realBufferLength + 4;
	}
	
	return RemapKClientError (err);
}
	
OSErr KClientLoginCompat (
	KClientSessionInfo*			inSession,
	KClientKey*					outPrivateKey)
{
	OSStatus err;
	BeginShieldedTry_ {
		err = KClientLogin (*inSession);

		cc_ccache_t		ccacheRef;
		if (err == noErr) {
			//	KClient 3.0 doesn't return the private key here
			//	so we need to pull it out of ccache
			err = KClientGetCCacheReference (*inSession, &ccacheRef);
		}
		
		if (err == noErr) {
			UCCache	ccache = UCCache (ccacheRef);

			UCredentialsIterator	iterator = ccache.NewCredentialsIterator (UPrincipal::kerberosV4);
			
			UCredentials	credentials;
			
			bool found = false;		
			while (iterator.Next (credentials)) {
				UPrincipal	principal = credentials.GetServicePrincipal ();
				if ((principal.GetName (UPrincipal::kerberosV4) == KRB_TICKET_GRANTING_TICKET) &&
				    (principal.GetInstance (UPrincipal::kerberosV4) == principal.GetRealm (UPrincipal::kerberosV4))) {
					credentials.GetV4SessionKey (outPrivateKey -> key);
					found = true;
					break;
				}
			}
			
			if (!found)
				err = paramErr;
		}
		
	} ShieldedCatch_ (std::bad_alloc&) {
		err = memFullErr;
	} EndShieldedTry_;
	
	return RemapKClientError (err);
}
	
OSErr KClientPasswordLoginCompat (
	KClientSessionInfo*			inSession,
	char*						inPassword,
	KClientKey*					outPrivateKey)
{
	OSStatus err;
	BeginShieldedTry_ {
		err = KClientPasswordLogin (*inSession, inPassword);

		cc_ccache_t		ccacheRef;
		if (err == noErr) {
			//	KClient 3.0 doesn't return the private key here
			//	so we need to pull it out of ccache
			err = KClientGetCCacheReference (*inSession, &ccacheRef);
		}
		
		if (err == noErr) {
			UCCache	ccache = UCCache (ccacheRef);

			UCredentialsIterator	iterator = ccache.NewCredentialsIterator (UPrincipal::kerberosV4);
			
			UCredentials	credentials;
	
			bool found = false;		
			while (iterator.Next (credentials)) {
				UPrincipal	principal = credentials.GetServicePrincipal ();
				if ((principal.GetName (UPrincipal::kerberosV4) == KRB_TICKET_GRANTING_TICKET) &&
				    (principal.GetInstance (UPrincipal::kerberosV4) == principal.GetRealm (UPrincipal::kerberosV4))) {
					credentials.GetV4SessionKey (outPrivateKey -> key);
					found = true;
					break;
				}
			}
			
			if (!found)
				err = paramErr;
		}
		
	} ShieldedCatch_ (std::bad_alloc&) {
		err = memFullErr;
	} EndShieldedTry_;
	
	return RemapKClientError (err);
}
	
OSErr KClientLogoutCompat (void)
{
	KClientSession session = nil;
	OSStatus err = KClientNewClientSession (&session);
	if (err == noErr) {
		err = KClientLogout (session);
	}
	
	if (session != nil) {
		err = KClientDisposeSession (session);
		Assert_ (err == noErr);
	}
	
	return RemapKClientError (err);
}

SInt16 KClientStatusCompat (void)
{
	KClientSession session = nil;
	OSStatus err = KClientNewClientSession (&session);
	UInt32	expirationTime;
	
	if (err == noErr) {
		err = KClientGetExpirationTime (session, &expirationTime);
	}
	
	if (err == noErr) {
		ExpirationTimeToTimeRemaining (&expirationTime);
	}
	
	if (session != nil) {
		err = KClientDisposeSession (session);
		Assert_ (err == noErr);
	}
	
	if (err != noErr) {
		return KClientNotLoggedIn;
	}
	
	return RemapKClientError (err);
}

OSErr KClientGetSessionKeyCompat (
	KClientSessionInfo*			inSession,
	KClientKey*					outSessionKey)
{
	return (OSErr) KClientGetSessionKey (*inSession, outSessionKey);
}
	
OSErr KClientEncryptCompat (
	KClientSessionInfo*			inSession,
	void*						inPlainBuffer,
	UInt32						inPlainBufferLength,
	void*						outEncryptedBuffer,
	UInt32*						outEncryptedBufferLength)
{
	// Turns out this is only an out parameter...
	UInt32	encryptedBufferLength = inPlainBufferLength + kKClientEncryptionOverhead;
	OSStatus err = KClientEncrypt (*inSession, inPlainBuffer, inPlainBufferLength,
		outEncryptedBuffer, &encryptedBufferLength);
	if (err == noErr) {
		*outEncryptedBufferLength = encryptedBufferLength;
	}
	return RemapKClientError (err);
}
	
OSErr KClientDecryptCompat (
	KClientSessionInfo*			inSession,
	void*						inEncryptedBuffer,
	UInt32						inEncryptedBufferLength,
	UInt32*						outPlainBufferOffset,
	UInt32*						outPlainBufferLength)
{
	OSStatus err = KClientDecrypt (*inSession, inEncryptedBuffer, inEncryptedBufferLength,
		outPlainBufferOffset, outPlainBufferLength);
	return RemapKClientError (err);
}

OSErr KClientProtectIntegrityCompat (
	KClientSessionInfo*			inSession,
	void*						inPlainBuffer,
	UInt32						inPlainBufferLength,
	void*						outProtectedBuffer,
	UInt32*						outProtectedBufferLength)
{
	// Turns out this is only an out parameter...
	UInt32	protectedBufferLength = inPlainBufferLength + kKClientProtectionOverhead;
	OSStatus err = KClientProtectIntegrity (*inSession, inPlainBuffer, inPlainBufferLength,
		outProtectedBuffer, &protectedBufferLength);
	if (err == noErr) {
		*outProtectedBufferLength = protectedBufferLength;
	}
	return RemapKClientError (err);
}

OSErr KClientVerifyIntegrityCompat (
	KClientSessionInfo*			inSession,
	void*						inProtectedBuffer,
	UInt32						inProtectedBufferLength,
	UInt32*						outPlainBufferOffset,
	UInt32*						outPlainBufferLength)
{
	OSStatus err = KClientVerifyIntegrity (*inSession, inProtectedBuffer, inProtectedBufferLength,
		outPlainBufferOffset, outPlainBufferLength);
	return RemapKClientError (err);
}

OSErr KServerNewSessionCompat (
	KClientSessionInfo*			inSession,
	char*						inService,
	UInt32						inLocalAddress,
	UInt16						inLocalPort,
	UInt32						inRemoteAddress,
	UInt16						inRemotePort)
{
	KClientSession		session = nil;
	KClientAddress		localAddress = {inLocalAddress, inLocalPort};
	KClientAddress		remoteAddress = {inRemoteAddress, inRemotePort};
	KClientPrincipal	service;
	OSStatus err = KClientV4StringToPrincipal (inService, &service);
	if (err == noErr) {
		err = KClientNewServerSession (&session, service);
		KClientDisposePrincipal (service);
	}
	if ((err == noErr) && (inLocalAddress != 0)) {
		err = KClientSetLocalAddress (session, &localAddress);
	}
	
	if ((err == noErr) && (inRemoteAddress != 0)) {
		err = KClientSetRemoteAddress (session, &remoteAddress);
	}
	
	if ((err != noErr) && (session != nil)) {
		KClientDisposeSession (session);
	} else {
		*inSession = session;
	}
	return RemapKClientError (err);
}
	

OSErr KServerVerifyTicketCompat (
	KClientSessionInfo*			inSession,
	void*						inBuffer,
	char*						inFilename)
{
	KClientFile		keyFile;
	Boolean			useDefault;
	OSStatus err = KClientKeyFileLocationFromFullPath (inFilename, &keyFile, &useDefault);
	if ((err == noErr) && !useDefault) {
		err = KClientSetKeyFile (*inSession, &keyFile);
	}
	
	if (err == noErr) {
		err = KClientVerifyAuthenticator (*inSession, (char*)inBuffer + 4, *(UInt32*)inBuffer);
	}
	
	return RemapKClientError (err);
}
	
OSErr KServerGetReplyTicketCompat (
	KClientSessionInfo*			inSession,
	void*						outBuffer,
	UInt32*						ioBufferLength)
{
	UInt32	realBufferLength = (*ioBufferLength) - sizeof (UInt32);
	OSErr err = (OSErr) KClientGetEncryptedServiceReply (*inSession,
		(char*) outBuffer + 4, &realBufferLength);
	
	if (err == noErr) {
		*(UInt32*)outBuffer = realBufferLength;
		*ioBufferLength = realBufferLength + 4;
	}
	
	return RemapKClientError (err);
}
	
OSErr KServerAddKeyCompat (
	KClientSessionInfo*			inSession,
	KClientKey*					inPrivateKey,
	char*						inService,
	SInt32						inVersion,
	char*						inFilename)
{
	KClientFile			keyFile;
	KClientPrincipal	service = nil;

	Boolean			useDefault;
	OSStatus err = KClientKeyFileLocationFromFullPath (inFilename, &keyFile, &useDefault);
	if ((err == noErr) && !useDefault) {
		err = KClientSetKeyFile (*inSession, &keyFile);
	}
	
	if (err == noErr) {
		err = KClientV4StringToPrincipal (inService, &service);
	}
	
	if (err == noErr) {
		err = KClientSetServerPrincipal (*inSession, service);
		KClientDisposePrincipal (service);
	}
	
	if (err == noErr) {
		err = KClientAddServiceKey (*inSession, (UInt32) inVersion, inPrivateKey);
	}
	
	return RemapKClientError (err);
}
	
OSErr KServerGetKeyCompat (
	KClientSessionInfo*			inSession,
	KClientKey*					outPrivateKey,
	char*						inService,
	SInt32						inVersion,
	char*						inFilename)
{
	KClientFile			keyFile;
	KClientPrincipal	service = nil;

	Boolean			useDefault;
	OSStatus err = KClientKeyFileLocationFromFullPath (inFilename, &keyFile, &useDefault);
	if ((err == noErr) && !useDefault) {
		err = KClientSetKeyFile (*inSession, &keyFile);
	}
	
	if (err == noErr) {
		err = KClientV4StringToPrincipal (inService, &service);
	}
	
	if (err == noErr) {
		err = KClientSetServerPrincipal (*inSession, service);
		KClientDisposePrincipal (service);
	}
	
	if (err == noErr) {
		err = KClientGetServiceKey (*inSession, (UInt32) inVersion, outPrivateKey);
	}
	
	return RemapKClientError (err);
}
	

OSErr KServerGetSessionTimeRemainingCompat (
	KClientSessionInfo*			inSession,
	SInt32*						outSeconds)
{
	OSErr err = (OSErr) KClientGetExpirationTime (*inSession, (UInt32*) outSeconds);
	if (err == noErr) {
		ExpirationTimeToTimeRemaining ((UInt32*) outSeconds);
	}
	
	return RemapKClientError (err);
}
	
OSErr KClientGetSessionUserNameCompat (
	KClientSessionInfo*			inSession,
	char*						outUserName,
	SInt16						inNameType)
{
	char	name	[ANAME_SZ];
	char	instance	[INST_SZ];
	char	realm	[REALM_SZ];
	
	char	localRealm [REALM_SZ];
	
	KClientPrincipal	principal = nil;
	OSStatus err = KClientGetClientPrincipal (*inSession, &principal);
	if (err == noErr) {
		err = KClientPrincipalToV4Triplet (principal, name, instance, realm);
	}

	if (err == noErr) {
		krb_get_lrealm (localRealm, 1);

		if ((inNameType == KClientLocalName) ||
			((inNameType == KClientCommonName) && (strcmp (localRealm, realm) == 0)))
			realm [0] = '\0';
		err = kname_unparse (outUserName, name, instance, (realm [0] == '\0') ? NULL : realm);
	}
	
	if (principal != nil) {
		KClientDisposePrincipal (principal);
	}
	
	return RemapKClientError (err);
}

OSErr KClientMakeSendAuthCompat (
	KClientSessionInfo*			inSession,
	char*						inService,
	void*						outBuffer,
	UInt32*						ioBufferLength,
	SInt32						inChecksum,
	char*						inApplicationVersion)
{
	KClientPrincipal	service;
	OSStatus err = KClientV4StringToPrincipal (inService, &service);
	if (err == noErr) {
		err = KClientSetServerPrincipal (*inSession, service);
		KClientDisposePrincipal (service);
	}
	
	if (err == noErr) {
		err = KClientGetAuthenticatorForService (*inSession, (UInt32) inChecksum,
			inApplicationVersion, (char*) outBuffer, ioBufferLength);
	}
	
	return RemapKClientError (err);
}
	
	
OSErr KClientVerifyReplyTicketCompat (
	KClientSessionInfo*			inSession,
	void*						inBuffer,
	UInt32*						ioBufferLength)
{
	OSErr err = (OSErr) KClientVerifyEncryptedServiceReply (*inSession,
		(char*) inBuffer + sizeof (UInt32), (*ioBufferLength) - sizeof (UInt32));

	if (err == noErr)
		*ioBufferLength = *(UInt32*) inBuffer;

	return RemapKClientError (err);
}
	
OSErr KClientVerifyUnencryptedReplyTicketCompat (
	KClientSessionInfo*			inSession,
	void*						inBuffer,
	UInt32*						ioBufferLength)
{
	OSErr err = (OSErr) KClientVerifyProtectedServiceReply (*inSession,
		(char*) inBuffer + sizeof (UInt32), (*ioBufferLength) - sizeof (UInt32));

	if (err == noErr)
		*ioBufferLength = *(UInt32*) inBuffer;

	return RemapKClientError (err);
}

void
ExpirationTimeToTimeRemaining (
	UInt32*						ioTime)
{
	time_t		currentTime = time (nil);
	*ioTime -= currentTime;
}

static OSErr RemapKClientError (
	OSStatus	inError)
{
	if ((inError >= kcFirstKerberosError) && (inError <= kcLastKerberosError)) {
		return (OSErr) (cKrbKerberosErrBlock - (inError - kcFirstKerberosError));
	} else if ((inError >= klFirstError) && (inError <= klLastError)) {
		switch (inError) {
			case klNoErr:
				return noErr;
			
			case klParameterErr:
				return paramErr;

			case klMemFullErr:
				return memFullErr;
				
			case klPrincipalDoesNotExistErr:
				return cKrbNotLoggedIn;
			
			case klSystemDefaultDoesNotExistErr:
				return cKrbNotLoggedIn;
				
			case klUserCanceledErr:
				return cKrbUserCancelled;
				
			case klNoRealmsErr:
			case klRealmDoesNotExistErr:
			case klPreferencesReadErr:
			case klPreferencesWriteErr:
				return cKrbConfigurationErr;
				
			case klNotInForegroundErr:
				// This happens when the ASIP UAM calls us while the login dialog is up
			case klDialogAlreadyExistsErr:
				return cKrbAppInBkgnd;

			case klBadPrincipalErr:
			case klV5InitializationFailedErr:
				/* There is nothign useful among the existing KClient error codes... */
				return paramErr;
			
			default:
				SignalPStr_ ("\pUnknown LoginLib error in RemapKClientError");
				return paramErr;
		}
	} else {
		switch (inError) {
			case kcNoError:
				return noErr;
			
			case kcErrNoMemory:
				return memFullErr;
			
			case kcErrBadParam:
				return paramErr;
			
			case kcErrInvalidSession:
				return cKrbInvalidSession;
			
			case kcErrNotLoggedIn:
				return cKrbNotLoggedIn;
			
			case kcErrUserCancelled:
				return cKrbUserCancelled;
				
			case kcErrBufferTooSmall:
				return memFullErr;
				
			case kcErrKeyFileAccess:
				return fnfErr;
			
			case kcErrFileNotFound:
				return fnfErr;
			
			case kcErrChecksumMismatch:
				return cKrbServerImposter;
				
			case kcErrInvalidPreferences:
				return cKrbConfigurationErr;
			
			case kcErrInvalidPrincipal:
			case kcErrInvalidAddress:
			case kcErrInvalidFile:
			case kcErrNoClientPrincipal:
			case kcErrNoServerPrincipal:
			case kcErrNoLocalAddress:
			case kcErrNoRemoteAddress:
			case kcErrNoSessionKey:
			case kcErrNoServiceKey:
				SignalPStr_ ("\pLogic error in KClientCompatLib");
				return paramErr;
				
			case kcErrIncorrectPassword:
				return cKrbKerberosErrBlock + INTK_BADPW;
				
			default:
				SignalPStr_ ("\pUnknown error in RemapKClientError");
				return paramErr;
		}
	}
}


OSStatus KClientKeyFileLocationFromFullPath (
	const char*		inFullPath,
	FSSpec*			outFileSpec,
	Boolean*		outUseDefault)
{
    OSStatus err = noErr;
    FSRef ref;
    
	if ((inFullPath == nil) || (inFullPath [0] == '\0')) {
		*outUseDefault = true;
		return noErr;
	}
	
	*outUseDefault = false;
    
    /* convert the path to an FSRef */
    if (err == noErr) {
        err = FSPathMakeRef ((const UInt8 *)inFullPath, &ref, false);
    }
    
    /* and then convert the FSRef to an FSSpec */
    if (err == noErr) {
        err = FSGetCatalogInfo (&ref, kFSCatInfoNone, NULL, NULL, outFileSpec, NULL);
    }
    
    return err;
}
