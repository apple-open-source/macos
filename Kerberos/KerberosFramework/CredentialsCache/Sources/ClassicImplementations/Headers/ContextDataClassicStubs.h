/*
 * CCIContextDataClassicStubs.h
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/ClassicImplementations/Headers/ContextDataClassicStubs.h,v 1.4 2003/03/17 20:47:38 lxs Exp $
 */

#pragma once

#include "SharedStaticData.h"

#include "ClassicStub.h"
#include "ContextDataCallStubs.h"

// Inherit from CCIContextDataCallStub to get all the read-only operations
// and override all the write operations
class CCIContextDataClassicStub:
	public CCIContextDataCallStub,
	public CCIClassicStub {
	public:
		CCIContextDataClassicStub (
			CCIInt32			inAPIVersion);

		CCIContextDataClassicStub (
			CCIUniqueID			inContextID,
			CCIInt32			inAPIVersion);

		~CCIContextDataClassicStub ();

		// Create a ccache		
		CCIUniqueID
			CreateCCache (
				const std::string&		inName,
				CCIUInt32				inVersion,
				const std::string&		inPrincipal);

		// Create default ccache
		CCIUniqueID
			CreateDefaultCCache (
				CCIUInt32				inVersion,
				const std::string&		inPrincipal);

		// Create a unique new ccache
		CCIUniqueID
			CreateNewCCache (
				CCIUInt32				inVersion,
				const std::string&		inPrincipal);
				
	private:

		// Disallowed
		CCIContextDataClassicStub ();
		CCIContextDataClassicStub (const CCIContextDataClassicStub&);
		CCIContextDataClassicStub& operator = (const CCIContextDataClassicStub&);
};

namespace AEImplementations {
	typedef	CCIContextDataClassicStub	ContextDataStub;
}

