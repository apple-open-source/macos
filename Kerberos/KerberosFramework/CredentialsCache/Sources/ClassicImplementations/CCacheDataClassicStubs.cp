/*
 * CCICCacheDataClassicStubs.cp
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/ClassicImplementations/CCacheDataClassicStubs.cp,v 1.5 2003/03/17 20:46:39 lxs Exp $
 */

#include <PP_Prefix.h>
#include "CCacheDataClassicStubs.h"
#include "ClassicProtocol.h"

#include "FlattenCredentials.h"

#include <UAppleEventsMgr.h>
CCICCacheDataClassicStub::CCICCacheDataClassicStub (
	CCIUniqueID	inCCache,
	CCIInt32	inAPIVersion):
	CCICCacheDataCallStub (inCCache, inAPIVersion) {
}

CCICCacheDataClassicStub::~CCICCacheDataClassicStub () {
}

// Destroy a ccache
void
CCICCacheDataClassicStub::Destroy () {

	for (;;) {
		try {
			Reset (ccClassic_CCache_Destroy);

			mSendBuffer.Put (GetLastSeqNo ());

			mSendBuffer.Put (GetCCacheID ());

			SendMessage ();
			return;
		} catch (CCIException& e) {
			if (e.Error () != ccClassic_Err_YellowServerRestarted)
				throw;
		}
	}
}

// Make the ccache default
void
CCICCacheDataClassicStub::SetDefault () {

	for (;;) {
		try {
			Reset (ccClassic_CCache_SetDefault);

			mSendBuffer.Put (GetLastSeqNo ());

			mSendBuffer.Put (GetCCacheID ());

			SendMessage ();
			return;
		} catch (CCIException& e) {
			if (e.Error () != ccClassic_Err_YellowServerRestarted)
				throw;
		}
	}
}

// Set the principal
void
CCICCacheDataClassicStub::SetPrincipal (
	CCIUInt32				inVersion,
	const std::string&		inPrincipal) {

	for (;;) {
		try {
			Reset (ccClassic_CCache_SetPrincipal);

			mSendBuffer.Put (GetLastSeqNo ());

			mSendBuffer.Put (GetCCacheID ());
			mSendBuffer.Put (inVersion);
			mSendBuffer.Put (inPrincipal);

			SendMessage ();
			return;
		} catch (CCIException& e) {
			if (e.Error () != ccClassic_Err_YellowServerRestarted)
				throw;
		}
	}
}
	
#if CCache_v2_compat
// Set the principal without removing creds (for v2 compat)
void
CCICCacheDataClassicStub::CompatSetPrincipal (
	CCIUInt32				inVersion,
	const std::string&		inPrincipal) {

	for (;;) {
		try {
			Reset (ccClassic_CCache_CompatSetPrincipal);

			mSendBuffer.Put (GetLastSeqNo ());

			mSendBuffer.Put (GetCCacheID ());
			mSendBuffer.Put (inVersion);
			mSendBuffer.Put (inPrincipal);

			SendMessage ();
			return;
		} catch (CCIException& e) {
			if (e.Error () != ccClassic_Err_YellowServerRestarted)
				throw;
		}
	}
}
#endif	

// Store credentials
void
CCICCacheDataClassicStub::StoreConvertedCredentials (
	const cc_credentials_union*		inCredentials) {
	
	for (;;) {
		try {
			Reset (ccClassic_CCache_StoreConvertedCredentials);
			
			mSendBuffer.Put (GetLastSeqNo ());

			std::strstream	flatCredentials;
			flatCredentials << *inCredentials << std::ends;

			mSendBuffer.Put (GetCCacheID ());
			
			CCIUInt32	length = flatCredentials.pcount ();
			mSendBuffer.Put (length);
			mSendBuffer.PutData (flatCredentials.str (), length);
			flatCredentials.freeze ();

			SendMessage ();
			return;
		} catch (CCIException& e) {
			if (e.Error () != ccClassic_Err_YellowServerRestarted)
				throw;
		}
	}
}

#if CCache_v2_compat
// Store v2-style creds
void
CCICCacheDataClassicStub::CompatStoreConvertedCredentials (
	const cred_union&		inCredentials) {
	
	for (;;) {
		try {
			Reset (ccClassic_CCache_CompatStoreConvertedCredentials);

			mSendBuffer.Put (GetLastSeqNo ());

			std::strstream	flatCredentials;
			flatCredentials << inCredentials << std::ends;

		    mSendBuffer.Put (GetCCacheID ());

			CCIUInt32	length = flatCredentials.pcount ();
			mSendBuffer.Put (length);
			mSendBuffer.PutData (flatCredentials.str (), length);
			flatCredentials.freeze ();

			SendMessage ();
			return;
		} catch (CCIException& e) {
			if (e.Error () != ccClassic_Err_YellowServerRestarted)
				throw;
		}
	}
}

#endif

// Remove creds	
void		
CCICCacheDataClassicStub::RemoveCredentials (
	const CCICredentials&	inCredentials) {
	
	for (;;) {
		try {
			Reset (ccClassic_CCache_RemoveCredentials);
			
			mSendBuffer.Put (GetLastSeqNo ());

			mSendBuffer.Put (GetCCacheID ());
			mSendBuffer.Put (inCredentials.GetCredentialsID ());

			SendMessage ();
			return;
		} catch (CCIException& e) {
			if (e.Error () != ccClassic_Err_YellowServerRestarted)
				throw;
		}
	}
}
	
// Move ccache contents to a new ccache
void		
CCICCacheDataClassicStub::Move (
	CCICCache&		inCCache) {
	
	for (;;) {
		try {
			Reset (ccClassic_CCache_Move);

			mSendBuffer.Put (GetLastSeqNo ());

			mSendBuffer.Put (GetCCacheID ());
			mSendBuffer.Put (inCCache.GetCCacheID ());

			SendMessage ();
			return;
		} catch (CCIException& e) {
			if (e.Error () != ccClassic_Err_YellowServerRestarted)
				throw;
		}
	}
}
