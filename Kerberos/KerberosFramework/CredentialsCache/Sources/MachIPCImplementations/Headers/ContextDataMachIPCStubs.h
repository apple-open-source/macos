/*
 * CCIContextDataStubs.h
 *
 * $Header$
 */

#pragma once

#include <mach/std_types.h>

#include "Context.h"
#include "SharedStaticData.h"
#include "MachIPCStub.h"

// Warning: initialization order dependency
// We have to inherit from MacIPCStub first because otherwise the
// CCIInt32 inAPIVersion constructor doesn't work correctly.

class CCIContextDataMachIPCStub:
    public CCIMachIPCStub,
    public CCIContext {
	public:
		CCIContextDataMachIPCStub (
			CCIInt32			inAPIVersion);

		CCIContextDataMachIPCStub (
			CCIUniqueID			inContextID,
			CCIInt32			inAPIVersion);

		~CCIContextDataMachIPCStub ();
		
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
                
                CCIUniqueID
			GetGlobalContextID () const;
			
        // Override GetPort so we can get the context id for a new server
        virtual mach_port_t GetPort () const;
        virtual mach_port_t GetPortNoLaunch () const;

	private:
		// Disallowed
		CCIContextDataMachIPCStub ();
		CCIContextDataMachIPCStub (const CCIContextDataMachIPCStub&);
		CCIContextDataMachIPCStub& operator = (const CCIContextDataMachIPCStub&);
};

namespace MachIPCImplementations {
	typedef	CCIContextDataMachIPCStub	ContextDataStub;
}

