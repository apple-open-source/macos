/*
 * CCICredentialsDataCallStub.h
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/CallImplementations/Headers/CredentialsDataCallStubs.h,v 1.5 2001/09/24 16:33:27 meeroh Exp $
 */

#pragma once

#if CCache_v2_compat
#include <CredentialsCache/CredentialsCache2.h>
#endif

#include "CCache.internal.h"

#include "CCICredentials.h"

class CCICredentialsDataCallStub:
	public CCICredentials {
	public:
		CCICredentialsDataCallStub (
			CCIUniqueID		inCredentials,
			CCIInt32		inAPIVersion,
			bool			inInitialize = true);

		~CCICredentialsDataCallStub ();
		
		CCIUInt32	GetCredentialsVersion ();
		
		bool Compare (const CCICredentials& inCompareTo) const;

		void	CopyV4Credentials (
			cc_credentials_v4_t&		outCredentials) const;
		
		void	CopyV5Credentials (
			cc_credentials_v5_t&		outCredentials) const;
		
#if CCache_v2_compat
		void	CompatCopyV4Credentials (
			cc_credentials_v4_compat&	outCredentials) const;
		
		void	CompatCopyV5Credentials (
			cc_credentials_v5_compat&	outCredentials) const;
#endif
		
	private:
		// Disallowed
		CCICredentialsDataCallStub ();
		CCICredentialsDataCallStub (const CCICredentialsDataCallStub&);
		CCICredentialsDataCallStub& operator = (const CCICredentialsDataCallStub&);
};

namespace CallImplementations {
	typedef	CCICredentialsDataCallStub	CredentialsDataStub;
}

