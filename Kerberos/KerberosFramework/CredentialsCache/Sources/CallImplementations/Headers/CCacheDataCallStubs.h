/*
 * CCICCacheDataStubs.h
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/CallImplementations/Headers/CCacheDataCallStubs.h,v 1.7 2001/09/24 16:32:40 meeroh Exp $
 */

#pragma once

#include <string>
#include <vector>
#include <strstream>

#include "CCache.internal.h"
#include "CCICredentialsDataCallStubs.h"

#if CCache_v2_compat
#include <CredentialsCache/CredentialsCache2.h>
#endif

#include "CCICCache.h"

class CCICCacheDataCallStub:
	public CCICCache {
	public:

		CCICCacheDataCallStub (
			CCIUniqueID		inCCache,
			CCIInt32		inAPIVersion);

		~CCICCacheDataCallStub ();
		
		void Destroy ();
		
		void SetDefault ();
		
		CCIUInt32	GetCredentialsVersion ();
		
		std::string	GetPrincipal (
			CCIUInt32				inVersion);
			
		std::string	GetName ();
			
		void		SetPrincipal (
			CCIUInt32				inVersion,
			const std::string&		inPrincipal);
			
#if CCache_v2_compat
		void		CompatSetPrincipal (
			CCIUInt32				inVersion,
			const std::string&		inPrincipal);
#endif
			
		void		StoreConvertedCredentials (
			const cc_credentials_union*			inCredentials);
			
		void		StoreFlattenedCredentials (
			std::strstream&						inCredentials);
			
#if CCache_v2_compat
		void		CompatStoreConvertedCredentials (
			const cred_union&			inCredentials);

		void		CompatStoreFlattenedCredentials (
			std::strstream&						inCredentials);
#endif			

		void		RemoveCredentials (
			const CCICredentials&		inCredentials);
			
		CCITime		GetLastDefaultTime ();
		
		CCITime		GetChangeTime ();
		
		void		Move (
			CCICCache&					inCCache);
			
		CCILockID	Lock ();
		
		void		Unlock (
			CCILockID					inLock);
			
		bool Compare (const CCICCache& inCompareTo) const;

		void		GetCredentialsIDs (
				std::vector <CCIObjectID>&	outCredenitalsIDs) const;
			
	private:
		// Disallowed
		CCICCacheDataCallStub (const CCICCacheDataCallStub&);
		CCICCacheDataCallStub& operator = (const CCICCacheDataCallStub&);
};

namespace CallImplementations {
	typedef	CCICCacheDataCallStub	CCacheDataStub;
}

