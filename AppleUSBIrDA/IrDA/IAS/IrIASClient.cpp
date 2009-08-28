/*
    File:       IrIASClient.c

    Contains:   Implementation of the TIASClient class

*/

#include "IrGlue.h"                 // includes CommErrors.h
#include "IrIASClient.h"
#include "IrIASService.h"
#include "IrLSAPConn.h"
#include "CBufferSegment.h"
#include "IrDALog.h"

#if (hasTracing > 0 && hasIASClientTracing > 0)
enum IrIASClientTraceCodes
{
    kLogNew = 1,
    kLogInit,
    kLogFree,
    kUnexpectedEvent,

    kConnectRequestEvent,
    kConnectReplyEvent,
    kDisconnectRequestEvent,
    kDisconnectReplyEvent,
    kLookupRequestEvent,
    kLookupReplyEvent,
    kGetDataRequestEvent,
    kGetDataReplyEvent,
    kPutDataRequestEvent,
    kPutDataReplyEvent,

    kSendRequestEvent,
    kParseInputEvent,
    kLogParseReplyEvent,
    
    kEnqueueEvent,
    kDequeueEventStart,
    kDequeueEventEnd

};

static
EventTraceCauseDesc TraceEvents[] = {
    {kLogNew,                       "iasclient: create, obj="},
    {kLogInit,                      "iasclient: init"},
    {kLogFree,                      "iasclient: free, obj="},
    {kUnexpectedEvent,              "iasclient: unexpected event"},

    {kConnectRequestEvent,          "iasclient: connect request"},
    {kConnectReplyEvent,            "iasclient: connect reply"},
    {kDisconnectRequestEvent,       "iasclient: disconnect request"},
    {kDisconnectReplyEvent,         "iasclient: disconnect reply"},
    {kLookupRequestEvent,           "iasclient: lookup request"},
    {kLookupReplyEvent,             "iasclient: lookup reply"},
    {kGetDataRequestEvent,          "iasclient: get data request"},
    {kGetDataReplyEvent,            "iasclient: get data reply"},
    {kPutDataRequestEvent,          "iasclient: put data request"},
    {kPutDataReplyEvent,            "iasclient: put data reply"},

    {kSendRequestEvent,             "iasclient: send ias request"},
    {kParseInputEvent,              "iasclient: parse ias input"},
    {kLogParseReplyEvent,           "iasclient: parse ias reply, lkupstatus="},

    {kEnqueueEvent,                 "iasclient: Event Queued"},
    {kDequeueEventStart,            "iasclient: Event Start"},
    {kDequeueEventEnd,              "iasclient: Event End"}
};

#define XTRACE(x, y, z) IrDALogAdd ( x, y, ((uintptr_t)z & 0xffff), TraceEvents, true)
#else
#define XTRACE(x, y, z) ((void)0)
#endif

//--------------------------------------------------------------------------------
#define super TIrStream
OSDefineMetaClassAndStructors(TIASClient, TIrStream)
//--------------------------------------------------------------------------------

//--------------------------------------------------------------------------------
//      tIASClient
//--------------------------------------------------------------------------------
/*static*/
TIASClient *
TIASClient::tIASClient(TIrGlue* irda, TIrStream* client)
{
    TIASClient *obj = new TIASClient;
    
    XTRACE(kLogNew, 0, obj);
    if (obj && !obj->Init(irda, client)) {
	obj->release();
	obj = nil;
    }
    return obj;
}


//--------------------------------------------------------------------------------
//      free
//--------------------------------------------------------------------------------
void TIASClient::free()
{
    XTRACE(kLogFree, 0, this);

#define FREE(x) { if (x) { (x)->release(); x = nil; } }

    FREE(fLSAPConn);
    FREE(fAttribute);
    
    if (fRequestReply) {
	fIrDA->ReleaseEventBlock(fRequestReply);
	fRequestReply = nil;
    }
	
    // Delete the buffer
    if (fGetPutBuffer) {
	fGetPutBuffer->Delete();            // jdg: new style free the buffer 
	fGetPutBuffer = nil;
    }
    
    super::free();

} // TIASClient::~TIASClient


//--------------------------------------------------------------------------------
//      Init
//--------------------------------------------------------------------------------
Boolean TIASClient::Init(TIrGlue* irda, TIrStream* client)
{
    ULong myLSAPId;
    IrDAErr result;

    XTRACE(kLogInit, 0, this);

    fState = kIrIASClientDisconnected;
    fReceiveState = kIASClientReceiveReply;

    fClient = client;
    fLookupRequest = nil;
    
    fLSAPConn = nil;
    fRequestReply = nil;
    fGetPutBuffer = nil;
    fAttribute = nil;


    // Init IrStream
#if (hasTracing > 0 && hasIASClientTracing > 0)
    if (!super::Init(irda, TraceEvents, kEnqueueEvent)) return false;
#else
    if (!super::Init(irda)) return false;
#endif

    // New, init LSAPConn
    fLSAPConn = TLSAPConn::tLSAPConn(irda, this);
    require(fLSAPConn, Fail);

    // allocate an event block to use (defer until needed?)
    fRequestReply = irda->GrabEventBlock();
    require(fRequestReply, Fail);
    
    // Allocate, init the buffer segment
    fGetPutBuffer = CBufferSegment::New( kIASClientBufferSize );
    XREQUIRE(fGetPutBuffer, Fail);

    // Get and assign a dynamic lsapId to the connection
    myLSAPId = kAssignDynamicLSAPId;    // awful, rewrite
    result = irda->ObtainLSAPId(myLSAPId);
    XREQUIRENOT(result, Fail);
    fLSAPConn->AssignId(myLSAPId);

    return true;

Fail:
    return false;

} // TIASClient::Init


//--------------------------------------------------------------------------------
//      NextState
//--------------------------------------------------------------------------------
void TIASClient::NextState(ULong event)
{
    switch (fState) {
	case kIrIASClientDisconnected:
	    HandleDisconnectedStateEvent(event);
	    break;

	case kIrIASClientConnected:
	    HandleConnectedStateEvent(event);
	    break;

	default:
	    XTRACE(kUnexpectedEvent, 0, event);
	    DebugLog("TIASClient::NextState: bad fState");
	    break;
    }

} // TIASClient::NextState


//--------------------------------------------------------------------------------
//      HandleDisconnectedStateEvent
//--------------------------------------------------------------------------------
void TIASClient::HandleDisconnectedStateEvent(ULong event)
{
    switch (event) {
	case kIrConnectRequestEvent:
	    {
		XTRACE(kConnectRequestEvent, 0, 0);
		// Set the destination LSAPId and pass the request on to LSAPConn
		TIrConnLstnRequest* connectRequest = (TIrConnLstnRequest*)GetCurrentEvent();
		connectRequest->fLSAPId = kNameServerLSAPId;
		connectRequest->fData = nil;
		fLSAPConn->EnqueueEvent(connectRequest);
	    }
	    break;

	case kIrConnectReplyEvent:
	    {
		// Pass reply back to client - change state if connected
		TIrConnLstnReply* connectReply = (TIrConnLstnReply*)GetCurrentEvent();
		XTRACE(kConnectReplyEvent, 0, connectReply->fResult);
		if (connectReply->fResult == noErr) {
		    fState = kIrIASClientConnected;
		}
		fClient->EnqueueEvent(connectReply);
	    }
	    break;

	case kIrDisconnectRequestEvent:
	    {
		XTRACE(kDisconnectRequestEvent, 0, 0);
		// Pass the disconnect request to the lsapConn
		fLSAPConn->EnqueueEvent(GetCurrentEvent());
	    }
	    break;

	case kIrDisconnectReplyEvent:
	    XTRACE(kDisconnectReplyEvent, 0, 0);
	    // Already in disconnected state - just pass reply back to client
	    fClient->EnqueueEvent(GetCurrentEvent());
	    break;

	default:
	    DebugLog("TIASClient::HandleDisconnectedStateEvent: bad event");
	    break;
    }

} // TIASClient::HandleDisconnectedStateEvent


//--------------------------------------------------------------------------------
//      HandleConnectedStateEvent
//--------------------------------------------------------------------------------
void TIASClient::HandleConnectedStateEvent(ULong event)
{
    switch (event) {
	case kIrLookupRequestEvent:
	    {
		XTRACE(kLookupRequestEvent, 0, 0);
		IrDAErr result = SendRequest();
		if (result != noErr) {
		    LookupComplete(result);
		}
	    }
	    break;

	case kIrPutDataReplyEvent:
	    {
		TIrPutReply* putReply = (TIrPutReply*)GetCurrentEvent();
		XTRACE(kPutDataReplyEvent, 0, putReply->fResult);
		if (putReply->fResult != noErr) {
		    // Complete lookup request if any errors
		    LookupComplete(putReply->fResult);
		}
		else {
		    GetStart();
		}
	    }
	    break;

	case kIrGetDataReplyEvent:
	    {
		TIrGetReply* getReply = (TIrGetReply*)GetCurrentEvent();
		XTRACE(kGetDataReplyEvent, 0, getReply->fResult);
		if (getReply->fResult != noErr) {
		    // Complete lookup request if any errors
		    LookupComplete(getReply->fResult);
		}
		else {
		    ParseInput();
		}
	    }
	    break;

	case kIrReleaseRequestEvent:
	case kIrDisconnectRequestEvent:
	    XTRACE(kDisconnectRequestEvent, 1, event);
	    // Pass the disconnect request to the lsapConn
	    fLSAPConn->EnqueueEvent(GetCurrentEvent());
	    break;

	case kIrReleaseReplyEvent:
	case kIrDisconnectReplyEvent:
	    XTRACE(kDisconnectReplyEvent, 1, event);
	    // Now we're disconnected again
	    fState = kIrIASClientDisconnected;
	    // Pass the disconnect reply to the client
	    fClient->EnqueueEvent(GetCurrentEvent());
	    // NOTE: Lookups in progress will be cleaned up.  The disconnect
	    // will force either the get or the put in progress to complete
	    // with an error.  When they complete with an error LookupComplete
	    // is called (see  above) and LookupComplete frees fAttribute if
	    // necessary and sends a reply back to the client.
	    break;

	default:
	    DebugLog("TIASClient::HandleConnectedStateEvent: bad event");
	    break;
    }

} // TIASClient::HandleConnectedStateEvent


//================================ Helper methods ================================


//--------------------------------------------------------------------------------
//      SendRequest
//--------------------------------------------------------------------------------
IrDAErr TIASClient::SendRequest()
{
    Size classNameLen;
    Size attrNameLen;
    TIrLookupRequest* lookupRequest = (TIrLookupRequest*)GetCurrentEvent();

    XTRACE(kSendRequestEvent, 0, 0);

    // Save the request so it can be replied to
    XASSERT(fLookupRequest == nil);
    fLookupRequest = lookupRequest;

    // Get lengths of class and attr strings
    classNameLen = strlen((const char*)(lookupRequest->fClassName));
    attrNameLen = strlen((const char*)(lookupRequest->fAttrName));

    // Validate that className and attrName strings fit in buffer provided
    XASSERT((classNameLen + attrNameLen + 3) <= kIASClientBufferSize);
    if ((classNameLen + attrNameLen + 3) > kIASClientBufferSize) {
	return kIrDAErrBadParameter;
    }

    // Fill out the request
    fGetPutBuffer->Seek(0, kPosBeg);
    fGetPutBuffer->Put(kIASOpGetValueByClass | kIASFrameLstBit);
    fGetPutBuffer->Put((UByte)classNameLen);
    fGetPutBuffer->Putn((const UByte *)lookupRequest->fClassName, classNameLen);
    fGetPutBuffer->Put((UByte)attrNameLen);
    fGetPutBuffer->Putn((const UByte *)lookupRequest->fAttrName, attrNameLen);

    PutStart();

    return noErr;

} // TIASClient::SendRequest


//--------------------------------------------------------------------------------
//      ParseInput
//--------------------------------------------------------------------------------
void TIASClient::ParseInput()
{
    UByte ctrlByte;
    Boolean lastFrame;
    Boolean ackedFrame;
    IrDAErr result;

    // A reply frame has been received - parse it and decide what to do with it

    fGetPutBuffer->Seek(0, kPosBeg);
    ctrlByte = fGetPutBuffer->Get();
    lastFrame = ctrlByte & kIASFrameLstBit;
    ackedFrame = ctrlByte & kIASFrameAckBit;

    XTRACE(kParseInputEvent, ctrlByte, fReceiveState);

    switch(fReceiveState) {
	case kIASClientReceiveReply:
	    if (ackedFrame) {
		// The peer device is acking (unnecessary/optionally) my single frame request
		// It should have the lst bit on
		XASSERT(lastFrame);
		// Keep waiting for the actual reply
		GetStart();
	    }
	    else {
		if (ctrlByte == (kIASOpGetValueByClass | kIASFrameLstBit)) {
		    result = ParseReply();
		    LookupComplete(result);
		}
		else if (lastFrame) {
		    LookupComplete(kIrDAErrGeneric);    // ***FIXME: Better error code
		}
		else {
		    fReceiveState = kIASClientReceiveWaitFinal;
		}
	    }
	    break;

	case kIASClientReceiveWaitFinal:
	    // I don't accept multi-frame replies, so all I want to do is get the
	    // final frame of the reply so I can complete the lookup request with an error.
	    XASSERT(!ackedFrame);
	    if (lastFrame) {
		// Reset the receive state
		fReceiveState = kIASClientReceiveReply;
		LookupComplete(kIrDAErrGeneric);    // ***FIXME: Better error code
	    }
	    break;

	default:
	    break;
    }

    // Ack the frame I don't want/care about
    if (fReceiveState == kIASClientReceiveWaitFinal) {
	fGetPutBuffer->Seek(0, kPosBeg);
	fGetPutBuffer->Put(kIASOpGetValueByClass | kIASFrameAckBit);
	PutStart();
    }

} // TIASClient::ParseInput


//--------------------------------------------------------------------------------
//      ParseReply
//--------------------------------------------------------------------------------
IrDAErr TIASClient::ParseReply()
{
    UByte lookupStatus;

    // Get the reply status code
    lookupStatus = fGetPutBuffer->Get();
    require(lookupStatus == kIASRetOkay, Fail);

    // Create an attribute and let the attribute extract the info
    check(fAttribute == nil);
    fAttribute = TIASAttribute::tIASAttribute(fGetPutBuffer);
    require(fAttribute, FailNoMem);

    return noErr;

Fail:
    XTRACE(kLogParseReplyEvent, 0, lookupStatus);
    
FailNoMem:                          // fix: better err return
    return kIrDAErrGeneric;
    
} // TIASClient::ParseReply


//--------------------------------------------------------------------------------
//      GetStart
//--------------------------------------------------------------------------------
void TIASClient::GetStart()
{
    XTRACE(kGetDataRequestEvent, 0, 0);

    TIrGetRequest* getRequest = (TIrGetRequest*)fRequestReply;
    getRequest->fEvent = kIrGetDataRequestEvent;
    getRequest->fData = fGetPutBuffer;
    getRequest->fOffset = 0;
    getRequest->fLength = fGetPutBuffer->GetSize();
    fLSAPConn->EnqueueEvent(getRequest);

} // TIASClient::GetStart


//--------------------------------------------------------------------------------
//      PutStart
//--------------------------------------------------------------------------------
void TIASClient::PutStart()
{
    XTRACE(kPutDataRequestEvent, 0, 0);

    TIrPutRequest* putRequest = (TIrPutRequest*)fRequestReply;
    putRequest->fEvent = kIrPutDataRequestEvent;
    putRequest->fData = fGetPutBuffer;
    putRequest->fOffset = 0;
    putRequest->fLength = fGetPutBuffer->Position();
    fLSAPConn->EnqueueEvent(putRequest);

} // TIASClient::PutStart


//--------------------------------------------------------------------------------
//      LookupComplete
//--------------------------------------------------------------------------------
void TIASClient::LookupComplete(IrDAErr result)
{
    XTRACE(kLookupReplyEvent, 0, result);

    // Cleanup fAttribute if any errors
    if ((result != noErr) && (fAttribute != nil)) {
	fAttribute->release();
	fAttribute = nil;
    }

    // Reply to the client
    TIrLookupReply* lookupReply = (TIrLookupReply*)fLookupRequest;
    lookupReply->fEvent = kIrLookupReplyEvent;
    lookupReply->fResult = result;
    lookupReply->fAttribute = fAttribute;

    // Reset my locals
    fLookupRequest = nil;
    fAttribute = nil;

    fClient->EnqueueEvent(lookupReply);

} // TIASClient::LookupComplete
