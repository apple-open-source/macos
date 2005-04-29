/*
 * CCICredentialsDataClassicStub.h
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/ClassicImplementations/Headers/CredsDataClassicStubs.h,v 1.4 2003/03/17 20:48:09 lxs Exp $
 */

#pragma once

#include "ClassicStub.h"
#include "CredentialsDataCallStubs.h"

// CCICredentialsDataClassicStub is effectively equivalent to CCICredentialsDataCallStub
// because there are no write operations on CCICredentials
class CCICredentialsDataClassicStub:
	public CCICredentialsDataCallStub,
	public CCIClassicStub {
	
	public:
		CCICredentialsDataClassicStub (
			CCIUniqueID		inCredentials,
			CCIInt32		inAPIVersion,
			bool			inInitialize = true);

		~CCICredentialsDataClassicStub ();
		
	private:

		// Disallowed
		CCICredentialsDataClassicStub ();
		CCICredentialsDataClassicStub (const CCICredentialsDataClassicStub&);
		CCICredentialsDataClassicStub& operator = (const CCICredentialsDataClassicStub&);
};

namespace ClassicImplementations {
	typedef	CCICredentialsDataClassicStub	CredentialsDataStub;
}

