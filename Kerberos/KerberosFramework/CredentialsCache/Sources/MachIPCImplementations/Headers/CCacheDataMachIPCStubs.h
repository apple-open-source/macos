/*
 * CCICCacheDataStubs.h
 *
 * $Header$
 */

#pragma once

#include <mach/std_types.h>

#include "CCache.h"
#include "Credentials.h"
#include "MachIPCStub.h"

class CCICCacheDataMachIPCStub:
    public CCICCache,
    public CCIMachIPCStub {
	public:
		CCICCacheDataMachIPCStub (
			CCIUniqueID		inCCache,
			CCIInt32		inAPIVersion);

		~CCICCacheDataMachIPCStub ();
		
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
			std::strstream&			inCredentials);
			
#if CCache_v2_compat
		void		CompatStoreConvertedCredentials (
			const cred_union&			inCredentials);
#endif			

		void		RemoveCredentials (
			const CCICredentials&	inCredentials);
			
		CCITime		GetLastDefaultTime ();
		
		CCITime		GetChangeTime ();
		
		void		Move (
			CCICCache&		inCCache);
			
		CCILockID	Lock ();
		
		void		Unlock (
			CCILockID					inLock);
			
		bool Compare (const CCICCache& inCompareTo) const;

		void		GetCredentialsIDs (
				std::vector <CCIObjectID>&	outCredenitalsIDs) const;
			
        CCITime		GetKDCTimeOffset (
                CCIUInt32				inVersion) const;


        void		SetKDCTimeOffset (
                CCIUInt32				inVersion,
                CCITime					inTimeOffset);

        void		ClearKDCTimeOffset (
                CCIUInt32				inVersion);

	private:
		// Disallowed
		CCICCacheDataMachIPCStub ();
		CCICCacheDataMachIPCStub (const CCICCacheDataMachIPCStub&);
		CCICCacheDataMachIPCStub& operator = (const CCICCacheDataMachIPCStub&);
};

namespace MachIPCImplementations {
	typedef	CCICCacheDataMachIPCStub	CCacheDataStub;
}

