/*
 * CCICCacheDataStubs.h
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/ClassicImplementations/Headers/CCacheDataClassicStubs.h,v 1.4 2003/03/17 20:46:41 lxs Exp $
 */

#pragma once

#include "CredsDataClassicStubs.h"
#include "ClassicStub.h"
#include "CCacheDataCallStubs.h"

// Inherit from CCICCacheDataCallStub to get read-only functions and override
// write functions
class CCICCacheDataClassicStub:
	public CCICCacheDataCallStub,
	public CCIClassicStub {
	public:
		CCICCacheDataClassicStub (
			CCIUniqueID		inCCache,
			CCIInt32		inAPIVersion);

		~CCICCacheDataClassicStub ();

		// Destroy ccache		
		void Destroy ();
		
		// Make CCache default
		void SetDefault ();
		
		// Set principal
		void		SetPrincipal (
			CCIUInt32				inVersion,
			const std::string&		inPrincipal);
		
#if CCache_v2_compat
		// Set principal without destroying creds (for v2 compat)
		void		CompatSetPrincipal (
			CCIUInt32				inVersion,
			const std::string&		inPrincipal);
#endif

		// Store creds			
		void		StoreConvertedCredentials (
			const cc_credentials_union*			inCredentials);
			
#if CCache_v2_compat
		// Store v2-style creds
		void		CompatStoreConvertedCredentials (
			const cred_union&			inCredentials);
#endif			

		// Remove creds
		void		RemoveCredentials (
			const CCICredentials&	inCredentials);
			
		// Move contents to a new ccache
		void		Move (
			CCICCache&		inCCache);
			
	private:

		// Disallowed
		CCICCacheDataClassicStub ();
		CCICCacheDataClassicStub (const CCICCacheDataClassicStub&);
		CCICCacheDataClassicStub& operator = (const CCICCacheDataClassicStub&);
};

namespace ClassicImplementations {
	typedef	CCICCacheDataClassicStub	CCacheDataStub;
}

