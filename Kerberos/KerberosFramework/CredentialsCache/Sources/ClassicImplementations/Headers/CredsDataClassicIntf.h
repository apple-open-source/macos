/*
 * CCICCacheDataStubs.h
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/ClassicImplementations/Headers/CredsDataClassicIntf.h,v 1.3 2003/03/17 20:48:03 lxs Exp $
 */

#pragma once

#include "ClassicInterface.h"

#include "ClassicProtocol.h"

class CCICredentialsDataClassicInterface:
	public CCIClassicInterface {
	public:
		CCICredentialsDataClassicInterface (
			CCIUInt32			inEventID,
			const AppleEvent*	inEvent,
			AppleEvent*			outReply);
		virtual ~CCICredentialsDataClassicInterface ();
		
		void HandleEvent ();

	private:
		void GetCredentialsVersion ();
		void Compare ();
		void FlattenCredentials ();

		// Disallowed
		CCICredentialsDataClassicInterface ();
		CCICredentialsDataClassicInterface (const CCICredentialsDataClassicInterface&);
		CCICredentialsDataClassicInterface& operator = (const CCICredentialsDataClassicInterface&);
};

