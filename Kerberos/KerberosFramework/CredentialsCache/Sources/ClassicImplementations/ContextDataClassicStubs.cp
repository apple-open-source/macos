/*
 * CCIContextDataClassicStubs.cp
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/ClassicImplementations/ContextDataClassicStubs.cp,v 1.5 2003/03/17 20:47:36 lxs Exp $
 */

#include "ContextDataClassicStubs.h"

CCIContextDataClassicStub::CCIContextDataClassicStub (
	CCIUniqueID			inContextID,
	CCIInt32			inAPIVersion):
	CCIContextDataCallStub (inContextID, inAPIVersion) {
}

CCIContextDataClassicStub::CCIContextDataClassicStub (
	CCIInt32			inAPIVersion):
	CCIContextDataCallStub (GetGlobalContextID (), inAPIVersion) {
}

CCIContextDataClassicStub::~CCIContextDataClassicStub () {
}

// Create a new ccache
CCIUniqueID
CCIContextDataClassicStub::CreateCCache (
	const std::string&		inName,
	CCIUInt32				inVersion,
	const std::string&		inPrincipal) {

	for (;;) {
		try {
			Reset (ccClassic_Context_CreateCCache);

			mSendBuffer.Put (GetLastSeqNo ());

			mSendBuffer.Put (GetContextID ());
			mSendBuffer.Put (inName);
			mSendBuffer.Put (inVersion);
			mSendBuffer.Put (inPrincipal);
			
			SendMessage ();

			CCIUniqueID		ccache;
			mReceiveBuffer.Get (ccache);
			return ccache;
		} catch (CCIException& e) {
			if (e.Error () != ccClassic_Err_YellowServerRestarted)
				throw;
		}
	}
}

// Create the default ccache
CCIUniqueID
CCIContextDataClassicStub::CreateDefaultCCache (
	CCIUInt32				inVersion,
	const std::string&		inPrincipal) {

	for (;;) {
		try {
			Reset (ccClassic_Context_CreateDefaultCCache);

			mSendBuffer.Put (GetLastSeqNo ());

			mSendBuffer.Put (GetContextID ());
			mSendBuffer.Put (inVersion);
			mSendBuffer.Put (inPrincipal);

			SendMessage ();

			CCIUniqueID		ccache;
			mReceiveBuffer.Get (ccache);
			return ccache;
		} catch (CCIException& e) {
			if (e.Error () != ccClassic_Err_YellowServerRestarted)
				throw;
		}
	}
}

// Create a unique new ccache
CCIUniqueID
CCIContextDataClassicStub::CreateNewCCache (
	CCIUInt32				inVersion,
	const std::string&		inPrincipal) {

	for (;;) {
		try {
			Reset (ccClassic_Context_CreateNewCCache);

			mSendBuffer.Put (GetLastSeqNo ());
			
			mSendBuffer.Put (GetContextID ());
			mSendBuffer.Put (inVersion);
			mSendBuffer.Put (inPrincipal);

			SendMessage ();

			CCIUniqueID		ccache;
			mReceiveBuffer.Get (ccache);
			return ccache;
		} catch (CCIException& e) {
			if (e.Error () != ccClassic_Err_YellowServerRestarted)
				throw;
		}
	}
}
