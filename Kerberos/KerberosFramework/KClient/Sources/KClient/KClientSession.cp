/*
 * KClientSession
 *
 * Implementation of KClient session abstraction
 *
 * $Header$
 */

#include "KClientSession.h"
#include "KClientLoginIntf.h"
#include "KClientKerberosIntf.h"
#include "KClientCCacheIntf.h"
#include "KClientAddress.h"

// New client session
KClientSessionPriv::KClientSessionPriv ():
	mMagic (class_ID),
	mSessionType (session_Client),
	mLocalAddress (),
	mRemoteAddress (),
	mClientPrincipal (),
	mServerPrincipal (),
	mHaveSessionKey (false),
	mHaveChecksum (false),
	mChecksum (0),
	mCCache (),
	mCCacheContext (),
	mServerAuthenticatorData (nil)
{
	memset (&mSessionKey, 0, sizeof (mSessionKey));
	memset (&mKeySchedule, 0, sizeof (mKeySchedule));
}

// New server session
KClientSessionPriv::KClientSessionPriv (
	const UPrincipal&	inServicePrincipal):

	mMagic (class_ID),
	mSessionType (session_Server),
	mLocalAddress (),
	mRemoteAddress (),
	mClientPrincipal (),
	mServerPrincipal (inServicePrincipal.Clone ()),
	mHaveSessionKey (false),
	mHaveChecksum (false),
	mChecksum (0),
	mCCache (),
	mCCacheContext (),
	mServerAuthenticatorData (nil)
{
	memset (&mSessionKey, 0, sizeof (mSessionKey));
	memset (&mKeySchedule, 0, sizeof (mKeySchedule));
}


KClientSessionPriv::~KClientSessionPriv ()
{
}
	
const KClientAddressPriv&
KClientSessionPriv::GetLocalAddress () const
{
	return mLocalAddress;
}

void
KClientSessionPriv::SetLocalAddress (
	const KClientAddressPriv&	inLocalAddress)
{
	mLocalAddress = inLocalAddress;
}

const KClientAddressPriv&
KClientSessionPriv::GetRemoteAddress () const
{
	return mRemoteAddress;
}

void
KClientSessionPriv::SetRemoteAddress (
	const KClientAddressPriv&	inLocalAddress)
{
	mRemoteAddress = inLocalAddress;
}

const UPrincipal&
KClientSessionPriv::GetClientPrincipal () const
{
	return mClientPrincipal;
}

void
KClientSessionPriv::SetClientPrincipal (
	const UPrincipal& inPrincipal)
{
	mClientPrincipal = inPrincipal.Clone ();
	mCCache.Release ();
}

const UPrincipal&
KClientSessionPriv::GetServerPrincipal () const
{
	return mServerPrincipal;
}

void
KClientSessionPriv::SetServerPrincipal (
	const UPrincipal& inPrincipal)
{
	mServerPrincipal = inPrincipal.Clone ();
	memset (&mSessionKey, 0, sizeof (mSessionKey));
	memset (&mKeySchedule, 0, sizeof (mKeySchedule));
	mHaveSessionKey = 0;
}

void
KClientSessionPriv::GetExpirationTime (
	UInt32&						outExpiration)
{
	if (mSessionType == session_Client) {
		KClientLoginInterface::GetTicketExpiration (mClientPrincipal, outExpiration);
	} else {
		if (mServerAuthenticatorData == nil) {
			DebugThrow_ (KClientError (kcErrNoClientPrincipal));
		}
		outExpiration = mServerAuthenticatorData -> time_sec + 5 * 60 * mServerAuthenticatorData -> life;
	}
}

void
KClientSessionPriv::GetTicketForService (
			UInt32				inChecksum,
			void*				outBuffer,
			UInt32&				ioBufferLength)
{
	UInt32 checksum = inChecksum;
	// If the checksum is 0, generate a random one
	if (checksum == 0)
		checksum = ::PseudoRandom (outBuffer);

	// Login makes an implicit change to the state of the session, but we don't 
	// want that change to be permanent unless everything succeeds. So, on failure,
	// we restore the state
	StLoginHelper		loginHelper (this);
	
	try {
		Login (loginHelper);
		KClientKerberosInterface::GetTicketForService (GetServerPrincipal (),
			checksum, outBuffer, ioBufferLength);
		// Need to remember the checksum to verify the ticket later
		SetChecksum (checksum);
		UpdateSessionKey (loginHelper);
	} catch (...) {
		loginHelper.RestoreState ();
		throw;
	}
}	

void
KClientSessionPriv::Login ()
{
	// See comment in  GetTicketForService
	StLoginHelper	loginHelper (this);
	try {
		Login (loginHelper);
	} catch (...) {
		loginHelper.RestoreState ();
		throw;
	}
}

void
KClientSessionPriv::Login (
	const char*			inPassword)
{
	// See comment in  GetTicketForService
	StLoginHelper	loginHelper (this);
	try {
		Login (loginHelper, inPassword);
	} catch (...) {
		loginHelper.RestoreState ();
		throw;
	}
}

void
KClientSessionPriv::KeyFileLogin ()
{
	// See comment in  GetTicketForService
	StLoginHelper	loginHelper (this);
	try {
		KeyFileLogin (loginHelper);
	} catch (...) {
		loginHelper.RestoreState ();
		throw;
	}
}

void
KClientSessionPriv::Login (
	StLoginHelper&		inLoginHelper)
{
	// Save the state of the current default principal so that it can be
	// restored on failure (see comment in GetTicketForService)
	inLoginHelper.SaveState ();
	UpdateCCache (KClientLoginInterface::AcquireInitialTickets (inLoginHelper.GetClientPrincipal ()));
	UpdateClientPrincipal ();
}

void
KClientSessionPriv::Login (
	StLoginHelper&		inLoginHelper,
	const char*			inPassword)
{
	// Save the state of the current default principal so that it can be
	// restored on failure (see comment in GetTicketForService)
	inLoginHelper.SaveState ();
	UpdateCCache (KClientLoginInterface::AcquireInitialTicketsWithPassword (inLoginHelper.GetClientPrincipal (), inPassword));
	UpdateClientPrincipal ();
}

void
KClientSessionPriv::KeyFileLogin (
	StLoginHelper&		inLoginHelper)
{
	// Save the state of the current default principal so that it can be
	// restored on failure (see comment in GetTicketForService)
	inLoginHelper.SaveState ();
	KClientKerberosInterface::AcquireInitialTicketsFromKeyFile (inLoginHelper.GetClientPrincipal (), GetServerPrincipal (), mKeyFile);
	UpdateCCache (tkt_string ());
	UpdateClientPrincipal ();
}

void
KClientSessionPriv::Logout ()
{
	KClientLoginInterface::Logout (GetClientPrincipal ());
}

const KClientKey&
KClientSessionPriv::GetSessionKey () const
{
	if (!mHaveSessionKey)
		DebugThrow_ (KClientLogicError (kcErrNoSessionKey));
	return mSessionKey;
}

void
KClientSessionPriv::GetAuthenticatorForService (
	UInt32					inChecksum,
	const char*				inApplicationVersion,
	void*					outBuffer,
	UInt32&					ioBufferLength)
{
	UInt32 checksum = inChecksum;
	if (checksum == 0)
		checksum = ::PseudoRandom (outBuffer);
		
	// Login makes an implicit change to the state of the session, but we don't 
	// want that change to be permanent unless everything succeeds. So, on failure,
	// we restore the state
	StLoginHelper		loginHelper (this);
	
	try {
		Login (loginHelper);
		KClientKerberosInterface::GetAuthenticatorForService (GetServerPrincipal (),
			checksum, inApplicationVersion, outBuffer, ioBufferLength);
		// Need to remember the checksum to verify the ticket later
		SetChecksum (checksum);
		UpdateSessionKey (loginHelper);
	} catch (...) {
		loginHelper.RestoreState ();
		throw;
	}
}

void
KClientSessionPriv::VerifyEncryptedServiceReply (
	const void*				inBuffer,
	UInt32					inBufferLength)
{		
	KClientKerberosInterface::VerifyEncryptedServiceReply (inBuffer, inBufferLength, GetSessionKey (),
		GetKeySchedule (), GetLocalAddress (), GetRemoteAddress (), GetChecksum ());
}

void
KClientSessionPriv::VerifyProtectedServiceReply (
	const void*				inBuffer,
	UInt32					inBufferLength)
{		
	KClientKerberosInterface::VerifyProtectedServiceReply (inBuffer, inBufferLength, GetSessionKey (),
		GetLocalAddress (), GetRemoteAddress (), GetChecksum ());
}

void
KClientSessionPriv::Encrypt (
	const void*				inPlainBuffer,
	UInt32					inPlainBufferLength,
	void*					outEncryptedBuffer,
	UInt32&					ioEncryptedBufferLength)
{
	KClientKerberosInterface::Encrypt (inPlainBuffer, inPlainBufferLength, GetSessionKey (), GetKeySchedule (),
		GetLocalAddress (), GetRemoteAddress (), outEncryptedBuffer, ioEncryptedBufferLength);
}

void
KClientSessionPriv::Decrypt (
	void*					inEncryptedBuffer,
	UInt32					inEncryptedBufferLength,
	UInt32&					outPlainBufferOffset,
	UInt32&					outPlainBufferLength)
{
	KClientKerberosInterface::Decrypt (inEncryptedBuffer, inEncryptedBufferLength, GetSessionKey (), GetKeySchedule (),
		GetLocalAddress (), GetRemoteAddress (), outPlainBufferOffset, outPlainBufferLength);
}

void
KClientSessionPriv::ProtectIntegrity (
	const void*				inPlainBuffer,
	UInt32					inPlainBufferLength,
	void*					outProtectedBuffer,
	UInt32&					ioProtectedBufferLength)
{
	KClientKerberosInterface::ProtectIntegrity (inPlainBuffer, inPlainBufferLength, GetSessionKey (),
		GetLocalAddress (), GetRemoteAddress (), outProtectedBuffer, ioProtectedBufferLength);
}

void
KClientSessionPriv::VerifyIntegrity (
	void*					inProtectedBuffer,
	UInt32					inProtectedBufferLength,
	UInt32&					outPlainBufferOffset,
	UInt32&					outPlainBufferLength)
{
	KClientKerberosInterface::VerifyIntegrity (inProtectedBuffer, inProtectedBufferLength, GetSessionKey (),
		GetLocalAddress (), GetRemoteAddress (), outPlainBufferOffset, outPlainBufferLength);
}

void
KClientSessionPriv::VerifyAuthenticator (
	const void*				inBuffer,
	UInt32					inBufferLength)
{
	AUTH_DAT authenticatorData;

	KClientKerberosInterface::VerifyAuthenticator (GetServerPrincipal (), GetRemoteAddress (),
		inBuffer, inBufferLength, authenticatorData, GetKeyFile ());
	if (mServerAuthenticatorData == nil) {
		mServerAuthenticatorData = new AUTH_DAT;
	}
	*mServerAuthenticatorData = authenticatorData;
	mClientPrincipal = UPrincipal (
		UPrincipal::kerberosV4,
		mServerAuthenticatorData -> pname,
		mServerAuthenticatorData -> pinst,
		mServerAuthenticatorData -> prealm);
	
	SetSessionKey (*(KClientKey*) &mServerAuthenticatorData -> session);
}

void
KClientSessionPriv::GetEncryptedServiceReply (
	void*					inBuffer,
	UInt32&					ioBufferLength)
{
	KClientKerberosInterface::GetEncryptedServiceReply (GetChecksum () + 1, GetSessionKey (),
		GetKeySchedule (), GetLocalAddress (), GetRemoteAddress (), inBuffer, ioBufferLength);
}

void
KClientSessionPriv::GetProtectedServiceReply (
	void*					inBuffer,
	UInt32&					ioBufferLength)
{
	KClientKerberosInterface::GetProtectedServiceReply (GetChecksum () + 1, GetSessionKey (),
		GetLocalAddress (), GetRemoteAddress (), inBuffer, ioBufferLength);
}

void
KClientSessionPriv::AddServiceKey (
	UInt32					inVersion,
	const KClientKey&		inServiceKey)
{
	KClientKerberosInterface::AddServiceKey (GetKeyFile (), GetServerPrincipal (),
		inVersion, inServiceKey);
	SetSessionKey (inServiceKey);
}

// Update the session ccache to be the application default ccache
// Call this when this session should start using the tickets in the
// application default ccache
void
KClientSessionPriv::UpdateCCache (
	const std::string& inCCacheName)
{
	mCCache = mCCacheContext.Get ().OpenCCache (inCCacheName.c_str ());
	KClientKerberosInterface::SetDefaultCCache (inCCacheName);
}

// Update the session key from the credentials cache
void
KClientSessionPriv::UpdateSessionKey (
	StLoginHelper&			inLoginHelper)
{
	if (mSessionType == session_Client) {
		KClientKey	sessionKey;
		mCCache.GetCredentialsForService (GetServerPrincipal (), UPrincipal::kerberosV4).GetV4SessionKey (sessionKey.key);
		SetSessionKey (sessionKey);
	} else {
		UInt32	keyVersion = 0;
		GetServiceKey (keyVersion);
	}
}

// Freeze the session client principal to the current ccache principal
// Do not use SetClientPrincipal, because it resets the cache
void
KClientSessionPriv::UpdateClientPrincipal ()
{
	try {
		mClientPrincipal = mCCache.GetPrincipal (UPrincipal::kerberosV4);
	} catch (UCCacheLogicError& e) {
		if (e.Error () == ccErrBadCredentialsVersion) {
			// Loos like no v4 realm?
			throw KClientRuntimeError (kcErrInvalidPreferences);
		} else {
			throw;
		}
	}
}

void
KClientSessionPriv::SetChecksum (
	UInt32					inChecksum)
{
	mChecksum = inChecksum;
	mHaveChecksum = true;
}

const KClientKeySchedule& 
KClientSessionPriv::GetKeySchedule() const
{
	if (!mHaveSessionKey)
		DebugThrow_ (KClientLogicError (kcErrNoSessionKey));
	return mKeySchedule;
}

UInt32
KClientSessionPriv::GetChecksum () const
{
	if (!mHaveChecksum)
		DebugThrow_ (KClientLogicError (kcErrNoChecksum));
	return mChecksum;
}

void
KClientSessionPriv::SetSessionKey (
	const KClientKey&		inSessionKey)
{
	mSessionKey = inSessionKey;
	mHaveSessionKey = true;
	des_key_sched (mSessionKey.key, mKeySchedule.keySchedule);
}

void
KClientSessionPriv::SetKeyFile (
	const KClientFile&				inKeyFile)
{
	mKeyFile = inKeyFile;
}

const KClientKey&
KClientSessionPriv::GetServiceKey (
			UInt32&					inVersion)
{
	KClientKey newKey;
	KClientKerberosInterface::GetServiceKey (mKeyFile, mServerPrincipal, inVersion, newKey);
	SetSessionKey (newKey);
	return GetSessionKey ();
}

UCCache
KClientSessionPriv::GetCCacheReference ()
{
	// Reopen the cache, the caller closes it
	return mCCacheContext.GetCCacheForPrincipal (mClientPrincipal);
}

const UCCache&
KClientSessionPriv::GetCCache () {
	// Check that the ccache is still valid, and if not, reset it
	try {
		if (mCCache.Get () != nil) 
			UPrincipal::EVersion __attribute__ ((unused)) version = mCCache.GetCredentialsVersion ();
	} catch (UCCacheRuntimeError&	e) {
		// Rethrow anything that's not ccErrCCacheNotFound
		if (e.Error () != ccErrCCacheNotFound) 
			throw;
			
		// Handle ccErrCCacheNotFound by setting mCCache to nil; it will get reestablished
		// and pointed to a real ccache the next time it's needed
		mCCache.Reset (nil);
		mHaveChecksum = mHaveSessionKey = false;
	}
	
	return mCCache;
}


// When we are setting up a session, set the default ccache
StKClientSession::StKClientSession (
	KClientSession					inSession):
	mDefaultCCache (KClientKerberosInterface::DefaultCCache ())
{
	mSession = reinterpret_cast <KClientSessionPriv*> (inSession);
	if (!ValidateSession ())
		DebugThrow_ (KClientLogicError (kcErrInvalidSession));
		
	const UCCache&	ccache = mSession -> GetCCache ();
	if (ccache.Get () != nil)
		KClientKerberosInterface::SetDefaultCCache (ccache.GetName ().CString ());
}

StKClientSession::StKClientSession (
	KClientSessionPriv*				inSession):
	mDefaultCCache (KClientKerberosInterface::DefaultCCache ())
{
	mSession = inSession;

	const UCCache&	ccache = mSession -> GetCCache ();

	if (ccache.Get () != nil)
		KClientKerberosInterface::SetDefaultCCache (ccache.GetName ().CString ());
}

StKClientSession::~StKClientSession ()
{
	KClientKerberosInterface::SetDefaultCCache (mDefaultCCache);
}
	
KClientSessionPriv* 
StKClientSession::operator -> ()
{
	return mSession;
}

const KClientSessionPriv* 
StKClientSession::operator -> () const
{
	return mSession;
}

StKClientSession::operator KClientSession ()
{
	return reinterpret_cast <KClientSession> (mSession);
}

StKClientSession::operator KClientSessionPriv& ()
{
	return *mSession;
}

Boolean
StKClientSession::ValidateSession () const
{
	return mSession -> mMagic == KClientSessionPriv::class_ID;
}

UInt32
PseudoRandom (
	UInt32				inSeed)
{
	static Boolean	gHaveSeed = false;

	if (!gHaveSeed) {
		srand (inSeed);
		gHaveSeed = true;
	}

	return (UInt32) rand ();
}

UInt32
PseudoRandom (
	void*				inSeed)
{
	return PseudoRandom ((UInt32) inSeed);
}

KClientSessionPriv::StLoginHelper::StLoginHelper (
	KClientSessionPriv*			inSession):
	
	mSession (inSession) {
}

void
KClientSessionPriv::StLoginHelper::SaveState () {
	mSavedClientPrincipal = mSession -> mClientPrincipal;
	mSavedCCache = mSession -> mCCache;
}

void
KClientSessionPriv::StLoginHelper::RestoreState () {
	mSession -> mClientPrincipal = mSavedClientPrincipal;
	mSession -> mCCache = mSavedCCache;
}
