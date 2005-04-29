/*
 * CCICCacheDataStubs.h
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/ClassicImplementations/Headers/ContextDataClassicIntf.h,v 1.4 2003/03/17 20:47:33 lxs Exp $
 */

#pragma once

#include "ClassicInterface.h"

#include "ClassicProtocol.h"

class CCIContextDataClassicInterface:
	public CCIClassicInterface {
	public:
		CCIContextDataClassicInterface (
			CCIUInt32			inEventID,
			const AppleEvent*	inEvent,
			AppleEvent*			outReply);
		virtual ~CCIContextDataClassicInterface ();
		
		void HandleEvent ();

	private:
		void GetChangeTime ();
		void OpenCCache ();
		void OpenDefaultCCache ();
		void GetDefaultCCacheName ();
		void CreateCCache ();
		void CreateDefaultCCache ();
		void CreateNewCCache ();
		void Compare ();
		void GetCCacheIDs ();
		void GetGlobalContextID ();
                
                void SyncWithYellowCache ();

                void FabricateInitialDiffs ();

		// Disallowed
		CCIContextDataClassicInterface ();
		CCIContextDataClassicInterface (const CCIContextDataClassicInterface&);
		CCIContextDataClassicInterface& operator = (const CCIContextDataClassicInterface&);
};

