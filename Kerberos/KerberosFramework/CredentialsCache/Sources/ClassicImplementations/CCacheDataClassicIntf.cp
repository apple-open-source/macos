 /*
 * CCICCacheDataAEStubs.cp
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/ClassicImplementations/CCacheDataClassicIntf.cp,v 1.5 2003/03/17 20:46:33 lxs Exp $
 */

#include "CCacheDataClassicIntf.h"
#include "CCacheDataMachIPCStubs.h"
#include "CredsDataMachIPCStubs.h"

#include "FlattenCredentials.h"

CCICCacheDataClassicInterface::CCICCacheDataClassicInterface (
	CCIUInt32			inEventID,
	const AppleEvent*	inEvent,
	AppleEvent*			outReply):
	
	CCIClassicInterface (inEventID, inEvent, outReply) {
}

CCICCacheDataClassicInterface::~CCICCacheDataClassicInterface () {
}

void
CCICCacheDataClassicInterface::HandleEvent () {

	switch (mEventID) {
		case ccClassic_CCache_Destroy:
			Destroy ();
			break;
		
		case ccClassic_CCache_SetDefault:
			SetDefault ();
			break;

		case ccClassic_CCache_SetPrincipal:
			SetPrincipal ();
			break;

#if CCache_v2_compat
		case ccClassic_CCache_CompatSetPrincipal:
			CompatSetPrincipal ();
			break;
#endif

		case ccClassic_CCache_StoreConvertedCredentials:
			StoreCredentials ();
			break;

#if CCache_v2_compat
		case ccClassic_CCache_CompatStoreConvertedCredentials:
			StoreCredentials ();
			break;
#endif

		case ccClassic_CCache_RemoveCredentials:
			RemoveCredentials ();
			break;

		case ccClassic_CCache_Move:
			Move ();
			break;

		default:
			#warning do error
			;
			
	}
}

void CCICCacheDataClassicInterface::Destroy ()
{
	CCIResult	result = ccNoError;

	try {
		ExtractMessage ();

		CCIUInt32	serverID;
		mReceiveBuffer.Get (serverID);
                CheckServerID (serverID);

		CCIUInt32	seqNo;
		mReceiveBuffer.Get (seqNo);

		CCIUniqueID	ccacheID;
		mReceiveBuffer.Get (ccacheID);
		
		CCICCacheDataMachIPCStub ccache (ccacheID, ccapi_version_4);
		ccache.Destroy ();
		mSendBuffer.Put (result);
	
		AddDiffsToReply (mSendBuffer, serverID, seqNo);
		
		PrepareReply ();
		return;

	} CatchForClassicReturn_ (result);

        mSendBuffer.Reset ();
	mSendBuffer.Put (result);
	PrepareReply ();
}

void CCICCacheDataClassicInterface::SetDefault ()
{
	CCIResult	result = ccNoError;
	try {
		ExtractMessage ();

		CCIUInt32	serverID;
		mReceiveBuffer.Get (serverID);
                CheckServerID (serverID);

		CCIUInt32	seqNo;
		mReceiveBuffer.Get (seqNo);

		CCIUniqueID	ccacheID;
		mReceiveBuffer.Get (ccacheID);
		
		CCICCacheDataMachIPCStub ccache (ccacheID, ccapi_version_4);
		ccache.SetDefault ();

		mSendBuffer.Put (result);

		AddDiffsToReply (mSendBuffer, serverID, seqNo);
		
		PrepareReply ();
		return;

	} CatchForClassicReturn_ (result);

        mSendBuffer.Reset ();
	mSendBuffer.Put (result);
	PrepareReply ();
		
}

void CCICCacheDataClassicInterface::SetPrincipal ()
{
	CCIResult	result = ccNoError;
	try {
		ExtractMessage ();

		CCIUInt32	serverID;
		mReceiveBuffer.Get (serverID);
                CheckServerID (serverID);

		CCIUInt32	seqNo;
		mReceiveBuffer.Get (seqNo);

		CCIUniqueID	ccacheID;
		mReceiveBuffer.Get (ccacheID);

		CCIUInt32	version;
		mReceiveBuffer.Get (version);
		
		std::string	principal;
		mReceiveBuffer.Get (principal);
		
		CCICCacheDataMachIPCStub ccache (ccacheID, ccapi_version_4);
		ccache.SetPrincipal (version, principal);
		mSendBuffer.Put (result);

		AddDiffsToReply (mSendBuffer, serverID, seqNo);
		
		PrepareReply ();
		return;

	} CatchForClassicReturn_ (result);

        mSendBuffer.Reset ();
	mSendBuffer.Put (result);
	PrepareReply ();
		
}

void CCICCacheDataClassicInterface::CompatSetPrincipal ()
{
	CCIResult	result = ccNoError;
	try {
		ExtractMessage ();

		CCIUInt32	serverID;
		mReceiveBuffer.Get (serverID);
                CheckServerID (serverID);

		CCIUInt32	seqNo;
		mReceiveBuffer.Get (seqNo);

		CCIUniqueID	ccacheID;
		mReceiveBuffer.Get (ccacheID);

		CCIUInt32	version;
		mReceiveBuffer.Get (version);
		
		std::string	principal;
		mReceiveBuffer.Get (principal);
		
		CCICCacheDataMachIPCStub ccache (ccacheID, ccapi_version_4);
		ccache.CompatSetPrincipal (version, principal);
		mSendBuffer.Put (result);

		AddDiffsToReply (mSendBuffer, serverID, seqNo);
		
		PrepareReply ();
		return;

	} CatchForClassicReturn_ (result);

        mSendBuffer.Reset ();
	mSendBuffer.Put (result);
	PrepareReply ();
		
}

void CCICCacheDataClassicInterface::StoreCredentials ()
{
	CCIResult	result = ccNoError;
	try {
		ExtractMessage ();

		CCIUInt32	serverID;
		mReceiveBuffer.Get (serverID);
                CheckServerID (serverID);

		CCIUInt32	seqNo;
		mReceiveBuffer.Get (seqNo);

		CCIUniqueID	ccacheID;
		mReceiveBuffer.Get (ccacheID);

		std::strstream	flatCredentials;
		mReceiveBuffer.Get (flatCredentials);
                
		CCICCacheDataMachIPCStub ccache (ccacheID, ccapi_version_4);
		ccache.StoreFlattenedCredentials (flatCredentials);
		mSendBuffer.Put (result);

		AddDiffsToReply (mSendBuffer, serverID, seqNo);
		
		PrepareReply ();
		return;

	} CatchForClassicReturn_ (result);

        mSendBuffer.Reset ();
	mSendBuffer.Put (result);
	PrepareReply ();
		
}

void CCICCacheDataClassicInterface::CompatStoreCredentials ()
{
	CCIResult	result = ccNoError;
	try {
		ExtractMessage ();

		CCIUInt32	serverID;
		mReceiveBuffer.Get (serverID);
                CheckServerID (serverID);

		CCIUInt32	seqNo;
		mReceiveBuffer.Get (seqNo);

		CCIUniqueID	ccacheID;
		mReceiveBuffer.Get (ccacheID);

		std::strstream	flatCredentials;
		mReceiveBuffer.Get (flatCredentials);
		
		CCICCacheDataMachIPCStub ccache (ccacheID, ccapi_version_4);
		ccache.StoreFlattenedCredentials (flatCredentials);
		mSendBuffer.Put (result);

		AddDiffsToReply (mSendBuffer, serverID, seqNo);
		
		PrepareReply ();
		return;

	} CatchForClassicReturn_ (result);

        mSendBuffer.Reset ();
	mSendBuffer.Put (result);
	PrepareReply ();
		
}

void CCICCacheDataClassicInterface::RemoveCredentials ()
{
	CCIResult	result = ccNoError;
	try {
		ExtractMessage ();

		CCIUInt32	serverID;
		mReceiveBuffer.Get (serverID);
                CheckServerID (serverID);

		CCIUInt32	seqNo;
		mReceiveBuffer.Get (seqNo);

		CCIUniqueID	ccacheID;
		mReceiveBuffer.Get (ccacheID);

		CCIUniqueID	credentialsID;
		mReceiveBuffer.Get (credentialsID);
		
		CCICCacheDataMachIPCStub ccache (ccacheID, ccapi_version_4);
		CCICredentialsDataMachIPCStub credentials (credentialsID, ccapi_version_4);
                
		ccache.RemoveCredentials (credentials);
		mSendBuffer.Put (result);

		AddDiffsToReply (mSendBuffer, serverID, seqNo);
		
		PrepareReply ();
		return;

	} CatchForClassicReturn_ (result);

        mSendBuffer.Reset ();
	mSendBuffer.Put (result);
	PrepareReply ();
		
}

void CCICCacheDataClassicInterface::Move ()
{
	CCIResult	result = ccNoError;
	try {
		ExtractMessage ();

		CCIUInt32	serverID;
		mReceiveBuffer.Get (serverID);
                CheckServerID (serverID);

		CCIUInt32	seqNo;
		mReceiveBuffer.Get (seqNo);

		CCIUniqueID	ccacheID;
		mReceiveBuffer.Get (ccacheID);
		
		CCIUniqueID	destinationID;
		mReceiveBuffer.Get (destinationID);
		
		CCICCacheDataMachIPCStub ccache (ccacheID, ccapi_version_4);
		CCICCacheDataMachIPCStub destination (destinationID, ccapi_version_4);
		ccache.Move (destination);
		mSendBuffer.Put (result);

		AddDiffsToReply (mSendBuffer, serverID, seqNo);
		
		PrepareReply ();
		return;

	} CatchForClassicReturn_ (result);

        mSendBuffer.Reset ();
	mSendBuffer.Put (result);
	PrepareReply ();
		
}

