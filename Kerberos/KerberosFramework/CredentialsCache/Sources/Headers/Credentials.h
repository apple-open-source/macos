/*
 * CCICredentials.h
 *
 * $Header$
 */
 
#pragma once

#include "CCacheUtil.h"
#include "Internalize.h"
#include "Internal.h"
#include "Magic.h"

class CCECredentials {
	public:
		static cc_int32		Release (
			cc_credentials_t			inCredentials);
			
		static cc_int32 Compare (
			cc_credentials_t			inCCache,
			cc_credentials_t			inCompareTo,
			cc_uint32*					outEqual);
			
	private:
		// Disallowed
		CCECredentials ();
		CCECredentials (const CCECredentials&);
		CCECredentials& operator = (const CCECredentials&);
};

class CCICredentials:
	public CCIMagic <CCICredentials>,
	public CCIInternal <CCICredentials, cc_credentials_d> {
	public:
		enum {
			class_ID = FOUR_CHAR_CODE ('Cred'),
			invalidObject = ccErrInvalidCredentials
		};
					
						CCICredentials (
							const CCIUniqueID&			inCredentials,
							CCIInt32					inAPIVersion);
		
		virtual			~CCICredentials ();
		
		virtual	void	Initialize ();
		
		virtual	CCIUInt32
						GetCredentialsVersion () = 0;
		
		virtual	bool
						Compare (
							const CCICredentials& 		inCompareTo) const = 0;

		const CCIUniqueID&
						GetCredentialsID () const
						{
							return mCredentialsID;
						}
		
		virtual	void	CopyV4Credentials (
							cc_credentials_v4_t&		outCredentials) const = 0;
		
		virtual	void	CopyV5Credentials (
							cc_credentials_v5_t&		outCredentials) const = 0;
		
#if CCache_v2_compat
		// Same as Copy*Credentials, but use v2 API structs
		virtual	void	CompatCopyV4Credentials (
							cc_credentials_v4_compat&	outCredentials) const = 0;
		
		virtual	void	CompatCopyV5Credentials (
							cc_credentials_v5_compat&	outCredentials) const = 0;
#endif

	private:
		cc_credentials_union		mCredentials;
		CCIUniqueID					mCredentialsID;
		CCIInt32					mAPIVersion;
	
		void Validate ();
		
		// Disallowed 
		CCICredentials ();
		
	const	static	cc_credentials_f	sFunctionTable;

	friend class StInternalize <CCICredentials, cc_credentials_d>;
	friend class CCIInternal <CCICredentials, cc_credentials_d>;
};

typedef StInternalize <CCICredentials, cc_credentials_d>		StCredentials;


// Helper class to initialize cc_credentials_v4_t from CCICredentials
class CCICredentialsV4:
	public cc_credentials_v4_t {
	
	public:
		CCICredentialsV4 (
			CCICredentials*			inCredentials,
			CCIInt32				inAPIVersion);
			
		~CCICredentialsV4 ();
			
};

// Helper class to initialize cc_credentials_v5_t from CCICredentials
class CCICredentialsV5:
	public cc_credentials_v5_t {
	
	public:
		CCICredentialsV5 (
			CCICredentials*			inCredentials);
		
		~CCICredentialsV5 ();
		
	private:
		void Cleanup ();
};

// Helper class to produce cred_union from CCICrendentials, used by v2 API
#if CCache_v2_compat
class CCICompatCredentials:
	public CCIMagic <CCICompatCredentials>,
	public CCIInternalWorkaround <CCICompatCredentials, cred_union> {
	public:
		enum {
			class_ID = FOUR_CHAR_CODE ('Crd2'),
			invalidObject = ccErrInvalidCredentials
		};
		
						CCICompatCredentials (
							const CCIUniqueID&			inCredentials,
							CCIInt32					inAPIVersion);
		
						~CCICompatCredentials ();
		
				CCIUInt32
						GetCredentialsVersion ()
						{
							return mCredentials -> GetCredentialsVersion ();
						}
		
		const CCIUniqueID&
						GetCredentialsID () const
						{
							return mCredentialsID;
						}
		
	private:
		void Validate ();
		
		CCIUniqueID						mCredentialsID;
		CCIInt32						mAPIVersion;
		
		// The "real" credentials live here
		std::auto_ptr <CCICredentials>	mCredentials;

	friend class StInternalize <CCICompatCredentials, cred_union>;
	friend class CCIInternalWorkaround <CCICompatCredentials, cred_union>;
};

typedef StInternalize <CCICompatCredentials, cred_union>		StCompatCredentials;

// Helper class to initialize cc_credentials_v4_compat from CCICredentials
class CCICompatCredentialsV4:
	public cc_credentials_v4_compat {
	
	public:
		CCICompatCredentialsV4 (
			CCICredentials*			inCredentials,
			CCIInt32				inAPIVersion);
			
		~CCICompatCredentialsV4 ();
			
};

// Helper class to initialize cc_credentials_v5_compat from CCICredentials
class CCICompatCredentialsV5:
	public cc_credentials_v5_compat {
	
	public:
		CCICompatCredentialsV5 (
			CCICredentials*			inCredentials);
		
		~CCICompatCredentialsV5 ();
		
	private:
		void Cleanup ();
};
#endif
