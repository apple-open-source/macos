/*
 * KClientSession
 *
 * Abstraction for a KClient session
 *
 * $Header$
 */
 
#pragma once

#include <Kerberos/KClientTypes.h>
#include "KClientCCacheIntf.h"
#include "KClientAddress.h"
#include "KClientFile.h"

// KClientSession is the main class used by KClient. It represents the state of a connection 
// between an application and KClient, similar to a krb5 context. Using a KClientSession,
// the callers of KClient can do the important Kerberos stuff they need to do. 
// These calls mostly have a 1-1 corespondence to the public API, so see the API docs
// for most explanations

class KClientSessionPriv {
	public: 
										KClientSessionPriv ();

										KClientSessionPriv (
											const UPrincipal&	inServerPrincipal);
											
										~KClientSessionPriv ();
										
		const	UPrincipal&	GetClientPrincipal () const;
		void							SetClientPrincipal (
											const UPrincipal&	inPrincipal);
		const	UPrincipal&	GetServerPrincipal () const;
		void							SetServerPrincipal (
											const UPrincipal&	inPrincipal);
		const	KClientAddressPriv&		GetLocalAddress () const;
		void							SetLocalAddress (
											const KClientAddressPriv&	inAddress);
		const	KClientAddressPriv&		GetRemoteAddress () const;
		void							SetRemoteAddress (
											const KClientAddressPriv&	inAddress);
		const	KClientKey&				GetSessionKey () const;
		void							SetKeyFile (
											const KClientFile&			inKeyFile);
		void							GetExpirationTime (
											UInt32&						outExpiration);
		void							Login ();
		void							Login (
											const char*					inPassword);
		void							KeyFileLogin ();
		void							Logout ();
		const	KClientKey&				GetServiceKey (
											UInt32&						inKeyVersion);
		void							AddServiceKey (
											UInt32						inKeyVersion,
											const KClientKey&			inKey);
		void							GetTicketForService (
											UInt32						inChecksum,
											void*						outTicket,
											UInt32&						outBufferSize);	
		void							GetAuthenticatorForService (
											UInt32						inChecksum,
											const char*					inApplicationVersion,
											void*						outBuffer,
											UInt32&						ioBufferLength);
		void							VerifyEncryptedServiceReply (
											const void*					inBuffer,
											UInt32						inBufferLength);
		void							VerifyProtectedServiceReply (
											const void*					inBuffer,
											UInt32						inBufferLength);
		void							VerifyAuthenticator (
											const void*					inBuffer,
											UInt32						inBufferLength);
		void							GetEncryptedServiceReply (
											void*						outBuffer,
											UInt32&						ioBufferLength);
		void							GetProtectedServiceReply (
											void*						outBuffer,
											UInt32&						ioBufferLength);
		void							Encrypt (
											const void*					inPlainBuffer,
											UInt32						inPlainBufferLength,
											void*						outEncryptedBuffer,
											UInt32&						ioEncryptedBufferLength);										
		void							Decrypt (
											void*						inEncryptedBuffer,
											UInt32						inEncryptedBufferLength,
											UInt32&						outPlainBufferOffset,
											UInt32&						outPlainBufferLength);										
		void							ProtectIntegrity (
											const void*					inPlainBuffer,
											UInt32						inPlainBufferLength,
											void*						outProtectedBuffer,
											UInt32&						ioPtorectedBufferLength);
		void							VerifyIntegrity (
											void*						inProtectedBuffer,
											UInt32						inProtectedBufferLength,
											UInt32&						outPlainBufferOffset,
											UInt32&						outPlainBufferLength);
		UCCache							GetCCacheReference ();
		const UCCache&					GetCCache ();

		enum {
			class_ID = FOUR_CHAR_CODE ('Sssn')
		};
		
	private:
		enum ESessionType {
			session_Client,
			session_Server
		};
		
		UInt32					mMagic;
		
		ESessionType			mSessionType;

		KClientAddressPriv		mLocalAddress;
		KClientAddressPriv		mRemoteAddress;
		UPrincipal				mClientPrincipal;
		UPrincipal				mServerPrincipal;
		
		Boolean					mHaveSessionKey;
		KClientKey				mSessionKey;
		KClientKeySchedule		mKeySchedule;

		Boolean					mHaveChecksum;		
		UInt32					mChecksum;
		
		UCCache					mCCache;
		KClientCCacheInterface	mCCacheContext;
		
		KClientFilePriv			mKeyFile;
		
		AUTH_DAT*				mServerAuthenticatorData;

		// This helper state is used to avoid changing session state across
		// calls to Login in some cases -- see GetTicketForService and
		// GetAuthenticatorForService
		class StLoginHelper {
			public:
				StLoginHelper (
					KClientSessionPriv*		inSession);
					
				void SaveState ();
				void RestoreState ();
				
				const UPrincipal&		GetClientPrincipal () const { return mSavedClientPrincipal; }
			
			private:
				UPrincipal				mSavedClientPrincipal;
				UCCache					mSavedCCache;
				KClientSessionPriv*		mSession;
		};
		
		friend class StLoginHelper;
			
		// Private helper functions
		void							Login (
											StLoginHelper&				inLoginHelper);
		void							Login (
											StLoginHelper&				inLoginHelper,
											const char*					inPassword);
		void							KeyFileLogin (
											StLoginHelper&				inLoginHelper);

		void							UpdateCCache (
											const std::string& inCCacheName);
		void							UpdateSessionKey (
												StLoginHelper&			inLoginHelper);
		void							UpdateClientPrincipal ();
		void							SetChecksum (
											UInt32						inChecksum);
		UInt32							GetChecksum () const;
		void							SetSessionKey (
											const KClientKey&			inSessionKey);
		const KClientKeySchedule&		GetKeySchedule () const;
		const KClientFile&				GetKeyFile () const { return mKeyFile; }
		
		friend class StKClientSession;
};

//
// This class is used to safely convert a KClient session as exposed in the public API
// to an instance of the KClientSessionPriv class
class StKClientSession {
	public:
									StKClientSession (
										KClientSession			inSession);

									StKClientSession (
										KClientSessionPriv*		inSession);
										
									~StKClientSession ();
		
				KClientSessionPriv* operator -> ();
		const 	KClientSessionPriv* operator -> () const;
		
									operator KClientSession ();
									operator KClientSessionPriv& ();
									
	private:
		KClientSessionPriv*		mSession;
		std::string				mDefaultCCache;
		
		Boolean ValidateSession () const; 
};

// Pseudo-random number generator helpers
UInt32
PseudoRandom (
	UInt32				inSeed);

UInt32
PseudoRandom (
	void*				inSeed);
