/*
 * CCIContextDataClassicStubs.cp
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/ClassicImplementations/ContextDataClassicIntf.cp,v 1.5 2003/03/17 20:47:31 lxs Exp $
 */

#include "ContextDataClassicIntf.h"
#include "ClassicProtocol.h"
#include "ContextDataMachIPCStubs.h"

CCIContextDataClassicInterface::CCIContextDataClassicInterface (
	CCIUInt32			inEventID,
	const AppleEvent*	inEvent,
	AppleEvent*			outReply):
	
	CCIClassicInterface (inEventID, inEvent, outReply) {
}

CCIContextDataClassicInterface::~CCIContextDataClassicInterface () {
}

void
CCIContextDataClassicInterface::HandleEvent () {
	
	switch (mEventID) {
		case ccClassic_Context_CreateCCache:
			CreateCCache ();
			break;
			
		case ccClassic_Context_CreateDefaultCCache:
			CreateDefaultCCache ();
			break;
			
		case ccClassic_Context_CreateNewCCache:
			CreateNewCCache ();
			break;
                        
                case ccClassic_Context_SyncWithYellowCache:
                        SyncWithYellowCache ();
                        break;
                        
                case ccClassic_Context_FabricateInitialDiffs:
                        FabricateInitialDiffs ();
                        break;
			
		default:
			#warning do error
			;
	}
}

void CCIContextDataClassicInterface::CreateCCache ()
{
	CCIResult	result = ccNoError;

	try {
		ExtractMessage ();

		CCIUInt32	serverID;
		mReceiveBuffer.Get (serverID);
                CheckServerID (serverID);

		CCIUInt32	seqNo;
		mReceiveBuffer.Get (seqNo);
	
		CCIUniqueID	contextID;
		mReceiveBuffer.Get (contextID);
		
		std::string	name;
		mReceiveBuffer.Get (name);
		
		CCIUInt32	version;
		mReceiveBuffer.Get (version);
		
		std::string	principal;
		mReceiveBuffer.Get (principal);
		
		CCIContextDataMachIPCStub	context (contextID, ccapi_version_4);
		CCIUniqueID ccache = context.CreateCCache (name, version, principal);

		mSendBuffer.Put (result);

		AddDiffsToReply (mSendBuffer, serverID, seqNo);
		
		mSendBuffer.Put (ccache);

		PrepareReply ();
		return;

	} CatchForClassicReturn_ (result);

        mSendBuffer.Reset ();
	mSendBuffer.Put (result);
	PrepareReply ();
}

void CCIContextDataClassicInterface::CreateDefaultCCache ()
{
	CCIResult	result = ccNoError;

	try {
		ExtractMessage ();

		CCIUInt32	serverID;
		mReceiveBuffer.Get (serverID);
                CheckServerID (serverID);

		CCIUInt32	seqNo;
		mReceiveBuffer.Get (seqNo);

		CCIUniqueID	contextID;
		mReceiveBuffer.Get (contextID);
		
		CCIUInt32	version;
		mReceiveBuffer.Get (version);
		
		std::string	principal;
		mReceiveBuffer.Get (principal);
		
		CCIContextDataMachIPCStub	context (contextID, ccapi_version_4);
		CCIUniqueID ccache = context.CreateDefaultCCache (version, principal);
		mSendBuffer.Put (result);

		AddDiffsToReply (mSendBuffer, serverID, seqNo);
		
		mSendBuffer.Put (ccache);
		PrepareReply ();
		return;

	} CatchForClassicReturn_ (result);

        mSendBuffer.Reset ();
	mSendBuffer.Put (result);
	PrepareReply ();
}

void CCIContextDataClassicInterface::CreateNewCCache ()
{
	CCIResult	result = ccNoError;
	try {
		ExtractMessage ();

		CCIUInt32	serverID;
		mReceiveBuffer.Get (serverID);
                CheckServerID (serverID);

		CCIUInt32	seqNo;
		mReceiveBuffer.Get (seqNo);

		CCIUniqueID	contextID;
		mReceiveBuffer.Get (contextID);
		
		CCIUInt32	version;
		mReceiveBuffer.Get (version);
		
		std::string	principal;
		mReceiveBuffer.Get (principal);
		
		CCIContextDataMachIPCStub	context (contextID, ccapi_version_4);
		CCIUniqueID ccache = context.CreateNewCCache (version, principal);
		mSendBuffer.Put (result);

		AddDiffsToReply (mSendBuffer, serverID, seqNo);
		
		mSendBuffer.Put (ccache);
		PrepareReply ();
		return;

	} CatchForClassicReturn_ (result);

        mSendBuffer.Reset ();
	mSendBuffer.Put (result);
	PrepareReply ();
}

void
CCIContextDataClassicInterface::SyncWithYellowCache ()
{
	CCIResult	result = ccNoError;
	try {
		ExtractMessage ();

		CCIUInt32	serverID;
		mReceiveBuffer.Get (serverID);
                CheckServerID (serverID);

		CCIUInt32	seqNo;
		mReceiveBuffer.Get (seqNo);
                
                mSendBuffer.Put (result);
                
                AddDiffsToReply (mSendBuffer, serverID, seqNo);

		PrepareReply ();
		return;

	} CatchForClassicReturn_ (result);

        mSendBuffer.Reset ();
	mSendBuffer.Put (result);
	PrepareReply ();
}

void
CCIContextDataClassicInterface::FabricateInitialDiffs ()
{
    // This function provides the initial set of diffs which bootstraps 
    // the classic environment into the same state as the native
    // environment
    
	CCIResult	result = ccNoError;
	try {
		ExtractMessage ();

		CCIUInt32	serverID;
		mReceiveBuffer.Get (serverID);
                CheckServerID (serverID);

                mSendBuffer.Put (result);
                
                AddInitialDiffsToReply (mSendBuffer, serverID);

		PrepareReply ();
		return;

	} CatchForClassicReturn_ (result);
        
        mSendBuffer.Reset ();
	mSendBuffer.Put (result);
	PrepareReply ();
}
    