/*
 * Cache.h
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/Headers/CCache.h,v 1.17 2003/03/17 20:46:16 lxs Exp $
 */
 

#include "Credentials.h"

#include "Internalize.h"
#include "Internal.h"
#include "Magic.h"

#if CCache_v2_compat
#include "CredentialsCache2.h"
#endif

#pragma once

class CCECCache {
	public:
		static cc_int32 Release (
			cc_ccache_t				inCCache);
			
		static cc_int32 SetDefault (
			cc_ccache_t				inCCache);
			
		static cc_int32 GetCredentialsVersion (
			cc_ccache_t				inCCache,
			cc_uint32*				outVersion);
			
		static cc_int32 GetName (
			cc_ccache_t				inCCache,
			cc_string_t*			outName);
			
		static cc_int32 GetPrincipal (
			cc_ccache_t				inCCache,
			cc_uint32				inVersion,
			cc_string_t*			outPrincipal);
			
		static cc_int32 SetPrincipal (
			cc_ccache_t				inCCache,
			cc_uint32				inVersion,
			const char*				inPrincipal);
			
		static cc_int32 StoreCredentials (
			cc_ccache_t					inCCache,
			const cc_credentials_union*	inCreds);
			
		static cc_int32 RemoveCredentials (
			cc_ccache_t					inCCache,
			cc_credentials_t			inCreds);

		static cc_int32 Destroy (
			cc_ccache_t				inCCache);
			
		static cc_int32 NewCredentialsIterator (
			cc_ccache_t					inCCache,
			cc_credentials_iterator_t*	outIter);

		static cc_int32 GetLastDefaultTime (
			cc_ccache_t				inCCache,
			cc_time_t*				outTime);
			
		static cc_int32 GetChangeTime (
			cc_ccache_t				inCCache,
			cc_time_t*				outTime);
			
		static cc_int32 Lock (
			cc_ccache_t				inCCache,
			cc_uint32				inLockType,
			cc_uint32				inBlock);
			
		static cc_int32 Unlock (
			cc_ccache_t				inCCache);
			
		static cc_int32 Compare (
			cc_ccache_t				inCCache,
			cc_ccache_t				inCompareTo,
			cc_uint32*				outEqual);
			
		static cc_int32 Move (
			cc_ccache_t				inSource,
			cc_ccache_t				inDestination);
            
        static cc_int32 GetKDCTimeOffset (
            cc_ccache_t				inCCache,
            cc_int32				inVersion,
            cc_time_t				*outTimeOffset);
            
        static cc_int32 SetKDCTimeOffset (
            cc_ccache_t				inCCache,
            cc_int32				inVersion,
            cc_time_t				inTimeOffset);

        static cc_int32 ClearKDCTimeOffset (
            cc_ccache_t				inCCache,
            cc_int32				inVersion);
            
			
	private:
		// Disallowed
		CCECCache ();
		CCECCache (const CCECCache&);
		CCECCache& operator = (const CCECCache&);
};
		

class CCICCache:
	public CCIMagic <CCICCache>,
	public CCIInternal <CCICCache, cc_ccache_d> {

	public:
		enum {
			class_ID = FOUR_CHAR_CODE ('CChe'),
			invalidObject = ccErrInvalidCCache
		};
		
						CCICCache (
							CCIUniqueID							inCCache,
							CCIInt32							inAPIVersion);
							
		virtual			~CCICCache ()
						{
						}

		virtual	void	Destroy () = 0;
		
		virtual	void	SetDefault () = 0;
		
		virtual	CCIUInt32
						GetCredentialsVersion () = 0;
		
		virtual	std::string
						GetPrincipal (
							CCIUInt32							inVersion) = 0;
			
		virtual	std::string
						GetName () = 0;
			
		virtual	void	SetPrincipal (
							CCIUInt32							inVersion,
							const std::string&					inPrincipal) = 0;
			
		// Set the principal, v2-style (delete credentials)
		// Only used for v2 compat
#if CCache_v2_compat
		virtual	void	CompatSetPrincipal (
							CCIUInt32							inVersion,
							const std::string&					inPrincipal) = 0;
#endif
		
		// Store v3 credentials (with lame v4 lifetime format)	
		virtual	void	StoreCredentials (
							const cc_credentials_union*			inCredentials);
			
		// Store v4 credentials (with 32-bit v4 lifetime format)
		virtual	void	StoreConvertedCredentials (
							const cc_credentials_union*			inCredentials) = 0;
			
		
#if CCache_v2_compat
		// Store v3 credentials (with lame v4 lifetime format) using older struct
		virtual	void	CompatStoreCredentials (
							const cred_union&					inCredentials);

		// Store v4 credentials (with 32-bit v4 lifetime format) using older struct
		virtual	void	CompatStoreConvertedCredentials (
							const cred_union&					inCredentials) = 0;
#endif			

		virtual	void	RemoveCredentials (
							const CCICredentials&				inCredentials) = 0;
                	
		virtual	CCITime	
						GetLastDefaultTime () = 0;
		
		virtual	CCITime	
						GetChangeTime () = 0;
		
		virtual	void	Move (
							CCICCache&							inCCache) = 0;
			
		virtual	CCILockID
						Lock () = 0;
		
		virtual	void	Unlock (
							CCILockID							inLock) = 0;
			
		virtual	bool
						Compare (
							const CCICCache&					inCompareTo) const = 0;

		virtual	void	GetCredentialsIDs (
							std::vector <CCIObjectID>&			outCredenitalsIDs) const = 0;
			
        virtual	CCITime
                        GetKDCTimeOffset (
                            CCIUInt32							inVersion) const = 0;
                            
        virtual void
                        SetKDCTimeOffset (
                            CCIUInt32							inVersion,
                            CCITime								inTimeOffset) = 0;

        virtual void
                        ClearKDCTimeOffset (
                            CCIUInt32							inVersion) = 0;

		const	CCIUniqueID&
						GetCCacheID () const
						{
							return mCCacheID;
						}
		
#if CCache_v2_compat
				void	CompatSetVersion (
							CCIUInt32							inVersion);

				CCIUInt32
						CompatGetVersion () const
						{
							return mVersion;
						}
#endif
			
				CCIInt32
						GetAPIVersion () const
						{
							return mAPIVersion;
						}

	private:
#if CCache_v2_compat
		CCIUInt32					mVersion;		// v4 or v5
#endif
		CCIInt32					mAPIVersion;	// CCAPI version
		
		CCIUniqueID					mCCacheID;
		
		void		Validate ();

		static const	cc_ccache_f	sFunctionTable;

		friend class StInternalize <CCICCache, cc_ccache_d>;
		friend class CCIInternal <CCICCache, cc_ccache_d>;

		// Disallowed
		CCICCache ();
		CCICCache (const CCICCache&);
		CCICCache& operator = (const CCICCache&);
};

typedef StInternalize <CCICCache, cc_ccache_d>	StCCache;

