#include "CredentialsCacheInternal.h"
#include "ClassicInterface.h"
#include "ClassicProtocol.h"

CCIClassicInterface::CCIClassicInterface (
	CCIUInt32			inEventID,
	const AppleEvent*	inEvent,
	AppleEvent*			outReply):
	mEventID (inEventID),
	mEvent (inEvent),
	mReply (outReply)
{
}

CCIClassicInterface::~CCIClassicInterface ()
{
}

void
CCIClassicInterface::AddDiffsToReply (
	CCIHandleBuffer&		ioBuffer,
        CCIUInt32			inServerID,
	CCIUInt32			inSeqNo)
{
	CCIResult err = __CredentialsCacheInternalGetDiffs (inServerID, inSeqNo, ioBuffer.GetHandle ());
	if (err != ccNoError) {
		CCIDebugThrow_ (CCIException (err));
	}
	ioBuffer.UpdateSize ();
	ioBuffer.SetOffset (GetHandleSize (ioBuffer.GetHandle ()));
}

void
CCIClassicInterface::AddInitialDiffsToReply (
	CCIHandleBuffer&	ioBuffer,
        CCIUInt32		inServerID)
{
	CCIResult err = __CredentialsCacheInternalGetInitialDiffs (ioBuffer.GetHandle (), inServerID);
	if (err != ccNoError) {
		CCIDebugThrow_ (CCIException (err));
	}
	ioBuffer.UpdateSize ();
	ioBuffer.SetOffset (GetHandleSize (ioBuffer.GetHandle ()));
}

void
CCIClassicInterface::ExtractMessage ()
{
	DescType	type;
	Size		size;

	OSErr err = AEGetParamPtr (mEvent, ccClassic_Key_Message, typeWildCard, &type, NULL, 0, &size);
	if (err != noErr) {
		CCIDebugThrow_ (CCIException (ccErrNoMem));
	}
	
	Handle	message = NewHandle (size);
	if (message == NULL) {
		CCIDebugThrow_ (CCIException (ccErrNoMem));
	}
	
	HLock (message);

	err = AEGetParamPtr (mEvent, ccClassic_Key_Message, typeWildCard, &type, *message, size, &size);
	if (err != noErr) {
		CCIDebugThrow_ (CCIException (ccErrNoMem));
	}
	
	HUnlock (message);
	mReceiveBuffer.AdoptHandle (message);
}

void
CCIClassicInterface::PrepareReply ()
{
	Handle	reply = mSendBuffer.GetHandle ();
	
	
	HLock (reply);
	OSErr err = AEPutParamPtr (mReply, ccClassic_Key_Message, typeWildCard, *reply, GetHandleSize (reply));
	HUnlock (reply);
}

void
CCIClassicInterface::CheckServerID (
    CCIUInt32	inServerID)
{
        cc_uint32	correct = 0;
	CCIResult err = __CredentialsCacheInternalCheckServerID (inServerID, &correct);
	if (err != ccNoError) {
		CCIDebugThrow_ (CCIException (err));
	}
        
        if (!correct) {
            CCIDebugThrow_ (CCIException (ccClassic_Err_YellowServerRestarted));
        }
}
        