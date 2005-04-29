/*
 * CCICCacheDataAEStubs.cp
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/ClassicImplementations/CredsDataClassicIntf.cp,v 1.4 2003/03/17 20:48:01 lxs Exp $
 */

#include "CredsDataClassicIntf.h"
#include "CredsDataMachIPCStubs.h"

#include "FlattenCredentials.h"

CCICredentialsDataClassicInterface::CCICredentialsDataClassicInterface (
	CCIUInt32			inEventID, 
	const AppleEvent*	inEvent,
	AppleEvent*			outReply):
	
	CCIClassicInterface (inEventID, inEvent, outReply) {
}

CCICredentialsDataClassicInterface::~CCICredentialsDataClassicInterface () {
}

void
CCICredentialsDataClassicInterface::HandleEvent () {
	ExtractMessage ();

	switch (mEventID) {
		default:
			#warning do error
			;
			
	}
	
	PrepareReply ();
}

