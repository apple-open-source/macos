/*
 * CCIContextDataStubs.h
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/CallImplementations/Headers/ContextDataCallStubs.h,v 1.5 2001/09/24 16:33:12 meeroh Exp $
 */

#pragma once

#include <string>

#include "CCICCacheDataCallStubs.h"
#include "CCache.internal.h"

#include "CCISharedStaticData.h"

#include "CCIContext.h"

class CCIContextDataCallStub:
	public CCIContext {

	public:
		CCIContextDataCallStub (
			CCIInt32			inAPIVersion);

		CCIContextDataCallStub (
			CCIUniqueID			inContextID,
			CCIInt32			inAPIVersion);

		~CCIContextDataCallStub ();
		
		CCITime			GetChangeTime ();
		
		CCIUniqueID
			OpenCCache (
				const std::string&		inCCacheName);

		CCIUniqueID
			OpenDefaultCCache ();
	
		std::string
			GetDefaultCCacheName ();

		CCIUniqueID
			CreateCCache (
				const std::string&		inName,
				CCIUInt32				inVersion,
				const std::string&		inPrincipal);

		CCIUniqueID
			CreateDefaultCCache (
				CCIUInt32				inVersion,
				const std::string&		inPrincipal);

		CCIUniqueID
			CreateNewCCache (
				CCIUInt32				inVersion,
				const std::string&		inPrincipal);
				
		void GetCCacheIDs (
				std::vector <CCIObjectID>&		outCCacheIDs) const;
				
		CCILockID
			Lock ();
		
		void
			Unlock (
				CCILockID				inLock);
				
		bool Compare (const CCIContext& inCompareTo) const;

	protected:
		static CCIUniqueID GetGlobalContextID ();

	private:
		// Disallowed
		CCIContextDataCallStub ();
		CCIContextDataCallStub (const CCIContextDataCallStub&);
		CCIContextDataCallStub& operator = (const CCIContextDataCallStub&);
};

namespace CallImplementations {
	typedef	CCIContextDataCallStub	ContextDataStub;
}

