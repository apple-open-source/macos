/*
 * CCICredentialsDataMachIPCStub.h
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/MachIPCImplementations/Headers/CredsDataMachIPCStubs.h,v 1.8 2004/10/22 20:48:28 lxs Exp $
 */

#pragma once
#ifndef CCICredentialDataMachIPCStubs_h_
#define CCICredentialDataMachIPCStubs_h_

#include <mach/std_types.h>

#include "Credentials.h"
#include "MachIPCStub.h"

class CCICredentialsDataMachIPCStub:
    public CCICredentials,
    public CCIMachIPCStub {
	public:
		CCICredentialsDataMachIPCStub (
			CCIUniqueID		inCredentials,
			CCIInt32		inAPIVersion,
			bool			inInitialize = true);

		~CCICredentialsDataMachIPCStub ();
		
		CCIUInt32	GetCredentialsVersion ();
		
		bool Compare (const CCICredentials& inCompareTo) const;
                
		void	FlattenToStream (
                        std::ostream&			outFlatCredentials) const;
		
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
		CCICredentialsDataMachIPCStub ();
		CCICredentialsDataMachIPCStub (const CCICredentialsDataMachIPCStub&);
		CCICredentialsDataMachIPCStub& operator = (const CCICredentialsDataMachIPCStub&);
};

namespace MachIPCImplementations {
	typedef	CCICredentialsDataMachIPCStub	CredentialsDataStub;
}

#endif // CCICredentialDataMachIPCStubs_h_
