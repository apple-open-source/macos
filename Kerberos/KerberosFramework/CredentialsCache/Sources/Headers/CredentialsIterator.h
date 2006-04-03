/*
 * CCICredentialsIterator.h
 *
 * $Header$
 */
 
#pragma once

#include <Kerberos/CredentialsCache.h>
#include "Magic.h"
#include "Internal.h"
#include "Internalize.h"

class CCECredentialsIterator {
	public:
		static cc_int32 Release (
			cc_credentials_iterator_t	inCCache);
			
		static cc_int32 Next (
			cc_credentials_iterator_t	inIterator,
			cc_credentials_t*			outCredentials);
			
	private:
		// Disallowed
		CCECredentialsIterator ();
		CCECredentialsIterator (const CCECredentialsIterator&);
		CCECredentialsIterator& operator = (const CCECredentialsIterator&);
};

class CCICCache;

class CCICredentialsIterator:
	public CCIMagic <CCICredentialsIterator>,
	public CCIInternal <CCICredentialsIterator, cc_credentials_iterator_d> {

	public:
		CCICredentialsIterator (
			const	CCICCache&		inCCache,
			CCIInt32				inAPIVersion);
			
		~CCICredentialsIterator ();
			
		bool
			HasMore () const;
			
		CCIUniqueID
			Next ();
			
#if CCache_v2_compat
		// Neeeded by v2 compat iterator implementation
		CCIUInt32
			CompatGetVersion () { return mVersion; }
#endif
			
			
		enum {
			class_ID = FOUR_CHAR_CODE ('CrIt'),
			invalidObject = ccErrInvalidCredentialsIterator
		};
		
		CCIInt32 GetAPIVersion () const { return mAPIVersion; }
		
	private:
		std::vector <CCIObjectID>				mIterationSet;
		std::vector <CCIObjectID>::iterator		mIterator;
		std::auto_ptr <CCICCache>				mCCache;
		CCILockID								mCCacheLock;
#if CCache_v2_compat
		CCIUInt32					mVersion;
#endif
		CCIInt32					mAPIVersion;
		
		void		Validate ();

		static const	cc_credentials_iterator_f	sFunctionTable;

		friend class StInternalize <CCICredentialsIterator, cc_credentials_iterator_d>;
		friend class CCIInternal <CCICredentialsIterator, cc_credentials_iterator_d>;

		// Disallowed
		CCICredentialsIterator (const CCICredentialsIterator&);
		CCICredentialsIterator& operator = (const CCICredentialsIterator&);
};

typedef StInternalize <CCICredentialsIterator, cc_credentials_iterator_d>	StCredentialsIterator;

