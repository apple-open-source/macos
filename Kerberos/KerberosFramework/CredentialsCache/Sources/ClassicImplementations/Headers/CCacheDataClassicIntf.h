/*
 * CCICCacheDataStubs.h
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/ClassicImplementations/Headers/CCacheDataClassicIntf.h,v 1.3 2003/03/17 20:46:37 lxs Exp $
 */

#pragma once

#include "ClassicInterface.h"

#include "ClassicProtocol.h"

class CCICCacheDataClassicInterface:
	public CCIClassicInterface {
	public:
		CCICCacheDataClassicInterface (
			CCIUInt32			inEventID,
			const AppleEvent*	inEvent,
			AppleEvent*			outReply);
		virtual ~CCICCacheDataClassicInterface ();
		
		void HandleEvent ();

	private:
		void Destroy ();
		void SetDefault ();
		void GetCredentialsVersion ();
		void GetPrincipal ();
		void GetName ();
		void SetPrincipal ();
		void CompatSetPrincipal ();
		void StoreCredentials ();
		void CompatStoreCredentials ();
		void RemoveCredentials ();
		void GetLastDefaultTime ();
		void GetChangeTime ();
		void Move ();
		void Compare ();
		void GetCredentialsIDs ();

		// Disallowed
		CCICCacheDataClassicInterface ();
		CCICCacheDataClassicInterface (const CCICCacheDataClassicInterface&);
		CCICCacheDataClassicInterface& operator = (const CCICCacheDataClassicInterface&);
};

