#include <Gestalt.h>
#include <Math64.h>
#include <DriverServices.h>
#include <Multiprocessing.h>

#include <UMemoryMgr.h>
#include <LString.h>
#include <KerberosSupport/Utilities.h>
#include "AEClassicWorkaround.h"

#include "CredentialsCacheInternal.h"

#include "ContextDataCallStubs.h"
#include "CCacheDataCallStubs.h"
#include "CredentialsDataCallStubs.h"

#include "ClassicProtocol.h"
#include "UniqueGlobally.h"

#include "ClassicStub.h"

#include "CredentialsData.h"
#include "CCacheData.h"
#include "ContextData.h"

#if CCache_ContainsSharedStaticData
CCIUInt32	CCIClassicStub::sLastSeqNo = 0;
bool		CCIClassicStub::sDiffsHaveBeenInitialized = false;
CCIUInt32	CCIClassicStub::sServerID = 0;
#endif

ProcessSerialNumber CCIClassicStub::sServerPSN;

CCIClassicStub::CCIClassicStub ()
{
}

// Send message to server
void CCIClassicStub::SendCCacheAE (
	Ptr				inEventData,
	Size			inEventDataSize,
	CCIUInt32		inEventID,
	bool			inWait,
	Handle&			outReply,
	AEIdleUPP		inIdleProc) const
{
	OSErr	err;
	AppleEvent	event = {typeNull, NULL};
	AppleEvent	reply = {typeNull, NULL};
	
	try {
		// Initialize the AE
		MakeAppleEvent (event);
		err = AEPutParamPtr (&event, ccClassic_Key_MessageID, typeMagnitude, &inEventID, sizeof (inEventID));
		if (err != noErr) {
			DebugThrow_ (CCIException (ccErrServerUnavailable));
		}

		// Put params in the AE
		err = AEPutParamPtr (&event, ccClassic_Key_Message, ccClassic_CCacheAEType, inEventData, inEventDataSize);
		if (err != noErr) {
			DebugThrow_ (CCIException (ccErrServerUnavailable));
		}

		// Send AE		
		SendAEToServer (event, reply, inWait, inIdleProc);
		
		// If we are receiving the reply right away, unmangle the reply
		if (inWait) {
			ExtractReplyMessage (reply, outReply);
		}

		// Cleanup
		if (event.dataHandle != NULL)
			AEDisposeDesc (&event);
		if (reply.dataHandle != NULL)
			AEDisposeDesc (&reply);

	} catch (...) {
		if (event.dataHandle != NULL)
			AEDisposeDesc (&event);
		if (reply.dataHandle != NULL)
			AEDisposeDesc (&reply);
			
		throw;
	}

}

// Unmangle server reply
void CCIClassicStub::ExtractReplyMessage (
	const	AppleEvent&	inReplyEvent,
			Handle&		outReplyMessage)
{
	DescType	type;
	Size		size;

	// Get the AE param from the AE reply
	OSErr err = AEGetParamPtr (&inReplyEvent, ccClassic_Key_Message, typeWildCard, &type, NULL, 0, &size);
	if (err != noErr) {
		DebugThrow_ (CCIException (ccErrServerUnavailable));
	}
	
	// Create a new handle
	StHandleBlock	replyData (size, false, true);

	// Failure to allocate
	if (replyData.Get () == NULL) {
		DebugThrow_ (CCIException (ccErrNoMem));
	}
	
	// Lock the reply data
	StHandleLocker	lock (replyData);
	
	// Copy the data into the new handle
	err = AEGetParamPtr (&inReplyEvent, ccClassic_Key_Message, typeWildCard, &type, *replyData, size, &size);
	if (err != noErr) {
		DebugThrow_ (CCIException (ccErrServerUnavailable));
	}
	
	// Extract the data handle from the stack-based object and return it
	outReplyMessage = replyData.Get ();
	replyData.Release ();
}

// Make a new AE to send to the server
void CCIClassicStub::MakeAppleEvent (
	AppleEvent&		outAppleEvent) const
{
	AEAddressDesc	serverAddress = {typeNull, NULL};
	
	try {
		// Create an appropriate server address
		GetServerAddress (serverAddress);

		// Create the event
		OSErr err = AECreateAppleEvent (ccClassic_EventClass, ccClassic_EventID,
			&serverAddress, kAutoGenerateReturnID, kAnyTransactionID, &outAppleEvent);
		if (err != noErr) {
			DebugThrow_ (CCIException (ccErrServerUnavailable));
		}
		
		if (serverAddress.dataHandle != NULL)
			AEDisposeDesc (&serverAddress);
	} catch (...) {
		if (serverAddress.dataHandle != NULL)
			AEDisposeDesc (&serverAddress);
		
		throw;
	}
}

// Make server address desc
void CCIClassicStub::GetServerAddress (
	AEAddressDesc&	outServerAddress) const
{
	// Always use a signature-based address
	OSType	appCreator = ccClassic_YellowServerSignature;
	OSErr err = AECreateDesc (typeApplSignature, (Ptr) &appCreator,
		sizeof (appCreator), &outServerAddress);

	if (err != noErr) {
		DebugThrow_ (CCIException (ccErrServerUnavailable));
	}
}

// Send an AE to the server
void CCIClassicStub::SendAEToServer (
	AppleEvent&	inAppleEvent,
	AppleEvent&	outReply,
	bool		inWait,
	AEIdleUPP	inIdleProc) const
{
	// Launch the server first
	LaunchYellowServer ();
	
	// Let the server launch
	EventRecord	event;
	WaitNextEvent (0, &event, 0, NULL);

	OSErr err;

	// Disable process switching to avoid eating suspend/resume
	// Use AESendWorkaround in utilities lib to work around some problems sending events from
	// Classic to yellow we see on Mac OS X 10.1.x
	if (inWait) {
		StDisableSwitching	disableSwitching;
		err = AESendWorkaround (&inAppleEvent, &outReply, kAEWaitReply | kAECanInteract | kAECanSwitchLayer,
			kAEHighPriority, kAEDefaultTimeout, inIdleProc, NULL);
	} else {
		err = AESendWorkaround (&inAppleEvent, &outReply, kAEQueueReply | kAECanInteract | kAECanSwitchLayer,
			kAEHighPriority, kAEDefaultTimeout, inIdleProc, NULL);
	}

	if (err != noErr) {
		DebugThrow_ (CCIException (ccErrServerUnavailable));
	}
}

// Verify if the cached PSN for the server is still valid (i.e. the server hasn't been
// killed or restarted)
bool CCIClassicStub::ServerPSNIsValid () {

	if ((sServerPSN.highLongOfPSN != 0) || (sServerPSN.lowLongOfPSN != 0)) {
		// We have a PSN that was valid at some point
		ProcessSerialNumber psn = sServerPSN;
		ProcessInfoRec		pir;
		pir.processInfoLength = sizeof (pir);
		pir.processName = NULL;
		pir.processAppSpec = NULL;
		OSErr err = GetProcessInformation (&psn, &pir);
		if (err == noErr) {
			// The PSN is valid
			return true;
		}
	}
	
	return false;
}

// Launch the server
void CCIClassicStub::LaunchYellowServer () {

	// First we check if the PSN we cached is valid. If it is, we
	// assume that the server is still there
	
	OSErr	err;
	bool psnIsValid = ServerPSNIsValid ();
		
	// We need to make the AppleEvent now because we use it in two different ways below
	
	if (!psnIsValid) {
		// If the PSN is not valid, we need to launch it
		// Before we launch it, we have to find it, and to do that 
		// we have to discover where the system is. We do that by looking
		// at where the Finder is, since there is no other way that I know
		// of to find the X volume from Classic
		
		static bool sHaveYellowVolume = false;
		static SInt16 sYellowVolumeRefNum = 0;
		
		if (!sHaveYellowVolume) {
			ProcessSerialNumber psn = {0, kNoProcess};
			
			ProcessInfoRec pir;
			FSSpec appSpec;
			pir.processInfoLength = sizeof (pir);
			pir.processAppSpec = &appSpec;
			pir.processName = NULL;
			
			while (GetNextProcess (&psn) == noErr) {
				if (GetProcessInformation (&psn, &pir) == noErr) {
					if ((pir.processSignature == 'MACS') && (pir.processType == 'FNDR')) {
						sHaveYellowVolume = true;
						sYellowVolumeRefNum = appSpec.vRefNum;
						break;
					}
				}
			}
		}
		
		if (!sHaveYellowVolume) {
			CCIDebugThrow_ (CCIException (ccErrServerUnavailable));
		}
		
		FSSpec	serverSpec;
		err = FSMakeFSSpec (sYellowVolumeRefNum, fsRtDirID, "\p:System:Library:Frameworks:Kerberos.framework:Versions:A:Servers:CCacheClassicServer.app:Contents:MacOS:CCacheClassicServer", &serverSpec);
		if (err != noErr) {
			CCIDebugThrow_ (CCIException (ccErrServerUnavailable));
		}
		
		// Note that if the server is already running, this will just return the existing PSN, which is exactly 
		// what we want
		LaunchParamBlockRec	launch;
		launch.launchBlockID = extendedBlock;
		launch.launchEPBLength = extendedBlockLen;
		launch.launchFileFlags = 0;
		launch.launchControlFlags = launchNoFileFlags | launchContinue | launchDontSwitch;
		launch.launchAppSpec = &serverSpec;
		launch.launchAppParameters = NULL;
		err = LaunchApplication (&launch);
		
		if (err == noErr) {
			sServerPSN = launch.launchProcessSN;
		} else {
			CCIDebugThrow_ (CCIException (ccErrServerUnavailable));
		}
		
		while (!ServerPSNIsValid ()) {
			EventRecord	event;
			WaitNextEvent (everyEvent, &event, 1, NULL);
		}
	}
}

// Reset the buffers
void CCIClassicStub::Reset (
	OSType	inMessageType)
{
	mSendBuffer.Reset ();
	mSendBuffer.Put (sServerID);
	mEventID = inMessageType;
}

// Send the current message to the server and handle the reply
void CCIClassicStub::SendMessage ()
{
	// Get the initial diffs from the server before sending our message
	if (!sDiffsHaveBeenInitialized) {
		CCIClassicStub	stub;
		stub.InitializeDiffs ();
	}
	
	// Lock the send buffer
	StHandleLocker		lockMessage (mSendBuffer.GetHandle ());
	
	// Clear the receive buffer
	mReceiveBuffer.DisposeHandle ();
	
	Handle		reply;
	
	// Send the message
	SendCCacheAE (*mSendBuffer.GetHandle (), GetHandleSize (mSendBuffer.GetHandle ()), mEventID, true, reply, NULL);

	// Update hte receive buffer with the reply	
	mReceiveBuffer.AdoptHandle (reply);
	mReceiveBuffer.Reset ();
	
	// Read the error codef rom the reply buffer
	CCIResult	error;
	mReceiveBuffer.Get (error);

	if (error == noErr) {
		// Update the server ID
		mReceiveBuffer.Get (sServerID);	

		// When we send a message to the classic server, the
		// response we get includes the changes which we need to apply
		// to the ccache in Classic to make sure it's in sync
		if (!ApplyCCacheDifferences ()) {
			CCIDebugThrow_ (CCIException (ccErrServerUnavailable));
		}
	} else if (error == ccClassic_Err_YellowServerRestarted) {
		// This error means that someone restarted the yellow server
		// In that case, we have to reset our state completely
		ResetCCache ();

		// Ask the server to send us a completely new copy if the initial state		
		// The caller should retry when this error occurs
		CCIClassicStub	stub;
		stub.InitializeDiffs ();
		CCIDebugThrow_ (CCIException (error));
	} else {
		CCIDebugThrow_ (CCIException (error));
	}
}

#if CCI_DEBUG	
static std::string	sSeqNos;
#endif

bool CCIClassicStub::ApplyCCacheDifferences ()
{
#if CCI_DEBUG	
	sSeqNos.erase ();
#endif
	// Apply differences from the reply to the Classic copy of the cache
	
	CCIUInt32	blockType;

	bool result = true;

	// Read the block type from the reply
	mReceiveBuffer.Get (blockType);
	
	// If the next block is a diff, apply the diff
	while (blockType == ccClassic_DiffCookie) {
		result = ApplyOneCCacheDifference ();
		// Read the type of the next block
		mReceiveBuffer.Get (blockType);
	}
	
#if CCI_DEBUG	
	if (!result) {
		SignalPStr_ (LStr255 ("ApplyCCacheDifferences failed, ").Append (sSeqNos.c_str ()));
	}
#endif		
	
	// If the last diff was succesfully applied, that means that the
	// classic cache is up to date
	return result;
	
}

bool CCIClassicStub::ApplyOneCCacheDifference ()
{
	CCIUInt32	seqNo;

	// Get the sequence number of this diff	
	mReceiveBuffer.Get (seqNo);
	
	// If we already have this difference, don't apply it again, but return success
	if (seqNo < sLastSeqNo) {
#if CCI_DEBUG	
		sSeqNos += LStr255 (static_cast <SInt16> (seqNo)).Append ('a').ConstTextPtr ();
#endif
		return true;
	
	// If this diff is in future, refuse it (can't apply diffs out of order)
	} else if (seqNo != sLastSeqNo) {
#if CCI_DEBUG	
		sSeqNos += LStr255 (static_cast <SInt16> (seqNo)).Append ('f').ConstTextPtr ();
#endif
		return false;
	}

	// Read the difference type
	CCIUInt32	differenceType;
	mReceiveBuffer.Get (differenceType);

	// Do the appropriate thing with the diff data based on the type	
	switch (differenceType) {
		case ccClassic_Context_CreateCCache: {

			CCIUniqueID	contextID;
			mReceiveBuffer.Get (contextID);
			
			std::string	name;
			mReceiveBuffer.Get (name);
			
			CCIUInt32	version;
			mReceiveBuffer.Get (version);
			
			std::string	principal;
			mReceiveBuffer.Get (principal);
			
			CCIContextDataCallStub		context (contextID, ccapi_version_4);
			CCIUniqueID ccache = context.CreateCCache (name, version, principal);
			
			break;
		}

		case ccClassic_Context_CreateDefaultCCache: {

			CCIUniqueID	contextID;
			mReceiveBuffer.Get (contextID);
			
			CCIUInt32	version;
			mReceiveBuffer.Get (version);
			
			std::string	principal;
			mReceiveBuffer.Get (principal);
			
			CCIContextDataCallStub	context (contextID, ccapi_version_4);
			CCIUniqueID ccache = context.CreateDefaultCCache (version, principal);
			break;
		}

		case ccClassic_Context_CreateNewCCache: {

			CCIUniqueID	contextID;
			mReceiveBuffer.Get (contextID);
			
			CCIUInt32	version;
			mReceiveBuffer.Get (version);
			
			std::string	principal;
			mReceiveBuffer.Get (principal);
			
			CCIContextDataCallStub	context (contextID, ccapi_version_4);
			CCIUniqueID ccache = context.CreateNewCCache (version, principal);
			break;
		}
			
		case ccClassic_CCache_Destroy: {

			CCIUniqueID	ccacheID;
			mReceiveBuffer.Get (ccacheID);
	                
			CCICCacheDataCallStub ccache (ccacheID, ccapi_version_4);
			ccache.Destroy ();
			break;
		}

		case ccClassic_CCache_SetDefault: {

			CCIUniqueID	ccacheID;
			mReceiveBuffer.Get (ccacheID);
			
			CCICCacheDataCallStub ccache (ccacheID, ccapi_version_4);
			ccache.SetDefault ();
			break;
		}

		case ccClassic_CCache_SetPrincipal: {

			CCIUniqueID	ccacheID;
			mReceiveBuffer.Get (ccacheID);

			CCIUInt32	version;
			mReceiveBuffer.Get (version);
			
			std::string	principal;
			mReceiveBuffer.Get (principal);
			
			CCICCacheDataCallStub ccache (ccacheID, ccapi_version_4);
			ccache.SetPrincipal (version, principal);
			break;
		}

		case ccClassic_CCache_CompatSetPrincipal: {

			CCIUniqueID	ccacheID;
			mReceiveBuffer.Get (ccacheID);

			CCIUInt32	version;
			mReceiveBuffer.Get (version);
			
			std::string	principal;
			mReceiveBuffer.Get (principal);
			
			CCICCacheDataCallStub ccache (ccacheID, ccapi_version_4);
			ccache.CompatSetPrincipal (version, principal);
			break;
		}

		case ccClassic_CCache_StoreConvertedCredentials: {

			CCIUniqueID	ccacheID;
			mReceiveBuffer.Get (ccacheID);

			std::strstream	flatCredentials;
			mReceiveBuffer.Get (flatCredentials);
	                
			CCICCacheDataCallStub ccache (ccacheID, ccapi_version_4);
			ccache.StoreFlattenedCredentials (flatCredentials);
			break;
		}

		case ccClassic_CCache_CompatStoreConvertedCredentials: {

			CCIUniqueID	ccacheID;
			mReceiveBuffer.Get (ccacheID);

			std::strstream	flatCredentials;
			mReceiveBuffer.Get (flatCredentials);
			
			CCICCacheDataCallStub ccache (ccacheID, ccapi_version_4);
			ccache.CompatStoreFlattenedCredentials (flatCredentials);
			break;
		}

		case ccClassic_CCache_RemoveCredentials: {

			CCIUniqueID	ccacheID;
			mReceiveBuffer.Get (ccacheID);

			CCIUniqueID	credentialsID;
			mReceiveBuffer.Get (credentialsID);
			
			CCICCacheDataCallStub ccache (ccacheID, ccapi_version_4);
			CCICredentialsDataCallStub credentials (credentialsID, ccapi_version_4);
	                
			ccache.RemoveCredentials (credentials);
			break;
		}

		case ccClassic_CCache_Move: {

			CCIUniqueID	ccacheID;
			mReceiveBuffer.Get (ccacheID);
			
			CCIUniqueID	destinationID;
			mReceiveBuffer.Get (destinationID);
			
			CCICCacheDataCallStub ccache (ccacheID, ccapi_version_4);
			CCICCacheDataCallStub destination (destinationID, ccapi_version_4);
			ccache.Move (destination);
			break;
		}
		
		case ccClassic_CCache_SkipToID: {
			CCIUniqueID	nextID;
			mReceiveBuffer.Get (nextID);
			
			CCIUniqueGlobally <CCICCacheData>::SetNextGloballyUniqueID (nextID);
			break;
		}
		
		case ccClassic_Credentials_SkipToID: {
			CCIUniqueID	nextID;
			mReceiveBuffer.Get (nextID);
			
			CCIUniqueGlobally <CCICredentialsData>::SetNextGloballyUniqueID (nextID);
			break;
		}
		
		default:
			CCIAssert_ ("Unknown difference type in ApplyOneCCacheDifference.");
	}
	
	// Bump the sequence number
	IncrementSeqNo ();
	
#if CCI_DEBUG	
	sSeqNos += LStr255 (static_cast <SInt16> (seqNo)).Append ('t').ConstTextPtr ();
#endif
	return true;
}

void CCIClassicStub::InitializeDiffs ()
{
	// Get the initial set of diffs from the server
	Reset (ccClassic_Context_FabricateInitialDiffs);

	Handle			response;
	
	// Lock the send buffer	
	StHandleLocker 	lockMessage (mSendBuffer.GetHandle ());
	// Send the message
	SendCCacheAE (*mSendBuffer.GetHandle (), GetHandleSize (mSendBuffer.GetHandle ()),
		mEventID, true, response, NULL);
	
	// Setup the reply
	mReceiveBuffer.AdoptHandle (response);
	mReceiveBuffer.Reset ();
	
	// Read the error out of the reply
	CCIResult	error;
	mReceiveBuffer.Get (error);
	
	if (error != ccNoError) {
		CCIDebugThrow_ (CCIException (error));
	}
	
	// Update the server ID
	mReceiveBuffer.Get (sServerID);	
	
	// Apply the initial diffs
	if (!ApplyCCacheDifferences ()) {
		CCIDebugThrow_ (CCIException (ccErrServerUnavailable));
	}
	
	sDiffsHaveBeenInitialized = true;
	sLastSeqNo = 0;
}

// Reset the entire ccache to the initial state, in preparation for
// getting new initial diffs
void CCIClassicStub::ResetCCache ()
{
	CCIContextData*		context = CCIContextDataInterface::GetGlobalContext ();
	
	// Get list of CCache IDs from the global contex
	std::vector <CCIObjectID>	ccaches;
	context -> GetCCacheIDs (ccaches);
	
	std::vector <CCIObjectID>::iterator	i;
	
	// Destroy all ccaches
	for (i = ccaches.begin (); i != ccaches.end (); i++) {
		CCICCacheDataInterface	ccache (*i);
		ccache -> Destroy ();
	}
	
	// Reset globally unique IDs
	CCIUniqueGlobally <CCICCacheData>::SetNextGloballyUniqueID (0);
	CCIUniqueGlobally <CCICredentialsData>::SetNextGloballyUniqueID (0);

	// Reset classic stub data	
	sServerID = 0;
	sDiffsHaveBeenInitialized = false;
	sLastSeqNo = 0;
}

// Send the yellow server a message telling it to produce diffs
cc_int32 __CredentialsCacheInternalInitiateSyncWithYellowCache (void)
{
	try {
		// Create a stub used to send this message
		CCIClassicStub	stub;
		// Get initial diffs first if necessary
		if (!CCIClassicStub::sDiffsHaveBeenInitialized) {
			stub.Reset (ccClassic_Context_SyncWithYellowCache);
			stub.InitializeDiffs ();
		}
		
		stub.Reset (ccClassic_Context_SyncWithYellowCache);
	
		// Won't use this, there will be no response
		Handle	dummyResponse;

		// Put the last seq no in the message data
		stub.mSendBuffer.Put (CCIClassicStub::sLastSeqNo);
		
		// Lock the send buffer
		StHandleLocker		lockMessage (stub.mSendBuffer.GetHandle ());
		// Send the message
		CCIClassicStub::SendCCacheAE (*stub.mSendBuffer.GetHandle (), GetHandleSize (stub.mSendBuffer.GetHandle ()),
			stub.mEventID, false, dummyResponse, NULL);
		// No reply
		return ccNoError;
	} catch (CCIException& e) {
		return e.Error ();
	} catch (...) {
		return ccErrServerUnavailable;
	}
}

// Handle a reply received from the yellow server in response to __...InitiateSync...
cc_int32 __CredentialsCacheInternalCompleteSyncWithYellowCache (
	const AppleEvent*	inAppleEvent)
{
	try {
		// Extract the reply from the AppleEvent
		Handle	replyMessage;
		CCIClassicStub::ExtractReplyMessage (*inAppleEvent, replyMessage);
		
		// Create a new stub and tell it to use the reply
		CCIClassicStub		stub;
		stub.mReceiveBuffer.AdoptHandle (replyMessage);
		stub.mReceiveBuffer.Reset ();
	
		// Extract the error from the reply
		CCIResult	error;
		stub.mReceiveBuffer.Get (error);

		// This error means that someone restarted the yellow server
		// In that case, we have to reset our state completely
		// After we reset, just fall through and resync on the next call
		if (error == ccClassic_Err_YellowServerRestarted) {
			stub.ResetCCache ();
			error = ccErrServerUnavailable;
		}

		if (error != ccNoError) {
			CCIDebugThrow_ (CCIException (error));
		}

		// Update the server ID
		stub.mReceiveBuffer.Get (CCIClassicStub::sServerID);	
	
		// Apply the differences from the reply
		stub.ApplyCCacheDifferences ();
		
		return ccNoError;
	} catch (CCIException& e) {
		return e.Error ();
	} catch (...) {
		return ccErrServerUnavailable;
	}
}

// Sync with the yellow ccache (synchronously)
// Essentially the same as __...InitializeSync... followed by __...CompleteSync...
cc_int32 __CredentialsCacheInternalSyncWithYellowCache (
	AEIdleUPP		inIdleProc)
{
	try {
	
		CCIClassicStub	stub;
		// Initialize diffs if necessary
		if (!CCIClassicStub::sDiffsHaveBeenInitialized) {
			stub.Reset (ccClassic_Context_SyncWithYellowCache);
			stub.InitializeDiffs ();
		}
		
		
		stub.Reset (ccClassic_Context_SyncWithYellowCache);
	
		Handle	response;
		
		// Put the last seq no in the message
		stub.mSendBuffer.Put (CCIClassicStub::sLastSeqNo);
		
		// Lock the message
		StHandleLocker		lockMessage (stub.mSendBuffer.GetHandle ());
		// Send the message
		CCIClassicStub::SendCCacheAE (*stub.mSendBuffer.GetHandle (), GetHandleSize (stub.mSendBuffer.GetHandle ()),
			stub.mEventID, true, response, inIdleProc);

		// Get the reply
		stub.mReceiveBuffer.AdoptHandle (response);
		stub.mReceiveBuffer.Reset ();

		// Extract error from reply		
		CCIResult	error;
		stub.mReceiveBuffer.Get (error);

		if (error == noErr) {
			// Update server ID
			stub.mReceiveBuffer.Get (CCIClassicStub::sServerID);	

			// Apply diffs
			if (!stub.ApplyCCacheDifferences ()) {
				CCIDebugThrow_ (CCIException (ccErrServerUnavailable));
			}
		} else if (error == ccClassic_Err_YellowServerRestarted) {
			// This error means that someone restarted the yellow server
			// In that case, we have to reset our state completely
			stub.ResetCCache ();
			
			CCIClassicStub	stub;
			stub.InitializeDiffs ();
			CCIDebugThrow_ (CCIException (error));
		} else {
			CCIDebugThrow_ (CCIException (error));
		}

		return ccNoError;
	} catch (CCIException& e) {
		return e.Error ();
	} catch (...) {
		return ccErrServerUnavailable;
	}
}

