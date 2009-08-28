/*
    File:       IrIASServer.c

    Contains:   Implementation of the TIASServer class

*/

//#include "IrGlue.h"                   // includes CommErrors.h

#include "IrIASServer.h"
#include "IrIASService.h"
#include "IrEvent.h"
#include "IrLSAPConn.h"
#include "CBufferSegment.h"
#include "IrGlue.h"
#include "IrDALog.h"


#if (hasTracing > 0 && hasIASServerTracing > 0)

enum IrIASServerTraceCodes
{
    kNullEvent = 1,
    kDestroy,
    kInit,
    kUnexpectedEvent,
    kLogNextState,

    kResettingToListenEvent,

    kListenRequestEvent,
    kListenReplyEvent,
    kAcceptRequestEvent,
    kAcceptReplyEvent,
    kDisconnectRequestEvent,
    kDisconnectReplyEvent,
    kGetDataRequestEvent,
    kGetDataReplyEvent,
    kPutDataRequestEvent,
    kPutDataReplyEvent,

    kSendResponseEvent,
    kParseInputEvent,
    kParseRequestEvent,
    
    kEnqueueEvent,
    kDequeueEventStart,
    kDequeueEventEnd

};

EventTraceCauseDesc TraceEvents[] = {
    {kNullEvent,                    "iasserver: create obj="},
    {kDestroy,                      "iasserver: destroy obj="},
    {kInit,                         "iasserver: init"},
    {kUnexpectedEvent,              "iasserver: unexpected event"},
    {kLogNextState,                 "iasserver: nextstate, result=,event="},

    {kResettingToListenEvent,       "iasserver: resetting to listen"},

    {kListenRequestEvent,           "iasserver: listen request"},
    {kListenReplyEvent,             "iasserver: listen reply"},
    {kAcceptRequestEvent,           "iasserver: accept request"},
    {kAcceptReplyEvent,             "iasserver: accept reply"},
    {kDisconnectRequestEvent,       "iasserver: disconnect request"},
    {kDisconnectReplyEvent,         "iasserver: disconnect reply"},
    {kGetDataRequestEvent,          "iasserver: get data request"},
    {kGetDataReplyEvent,            "iasserver: get data reply"},
    {kPutDataRequestEvent,          "iasserver: put data request"},
    {kPutDataReplyEvent,            "iasserver: put data reply"},

    {kSendResponseEvent,            "iasserver: send ias response"},
    {kParseInputEvent,              "iasserver: parse ias input"},
    {kParseRequestEvent,            "iasserver: parse ias request"},
    
    {kEnqueueEvent,                 "iasserver: Event Queued"},
    {kDequeueEventStart,            "iasserver: Event Start"},
    {kDequeueEventEnd,              "iasserver: Event End"}
};


#define XTRACE(x, y, z) IrDALogAdd ( x, y, ((uintptr_t)z & 0xffff), TraceEvents, true)
#else

#define XTRACE(x, y, z) ((void)0)
#endif



//--------------------------------------------------------------------------------
#define super TIrStream
    OSDefineMetaClassAndStructors(TIASServer, TIrStream);
//--------------------------------------------------------------------------------

//--------------------------------------------------------------------------------
//      TIASServer
//--------------------------------------------------------------------------------
/*static*/
TIASServer *
TIASServer::tIASServer(TIrGlue* irda, TIASService *nameService)
{
    TIASServer *obj = new TIASServer;
    XTRACE(kNullEvent, 0, obj);
	
    if (obj && !obj->Init(irda, nameService)) {
	obj->release();
	obj = nil;
    }
    return obj;
}


//--------------------------------------------------------------------------------
//      free
//--------------------------------------------------------------------------------
void
TIASServer::free()
{
    XTRACE(kDestroy, 0, this);
    
#define FREE(x) { if (x) { (x)->release(); x = nil; } }

    FREE(fLSAPConn);                    // free the lsap connection
    
    if (fRequestReply) {
	fIrDA->ReleaseEventBlock(fRequestReply);
	fRequestReply = nil;
    }
    
    // Delete the buffer
    if (fGetPutBuffer) {
	fGetPutBuffer->Delete();        // jdg: new style buffer release
	fGetPutBuffer = nil;
    }
    super::free();

} // TIASServer::free


//--------------------------------------------------------------------------------
//      Init
//--------------------------------------------------------------------------------
Boolean TIASServer::Init(TIrGlue* irda, TIASService *nameService)
{
    XTRACE(kInit, 0, this);
    //int Listen_Start_Commented_Out;

    fOpCode         = kIASOpUnassigned;
    fReceiveState   = kIASServerReceiveStart;
    
    fNameService = nil;
    fLSAPConn = nil;
    fRequestReply = nil;
    fGetPutBuffer = nil;


    // Init IrStream
#if (hasTracing > 0 && hasIASServerTracing > 0)
    if (!super::Init(irda, TraceEvents, kEnqueueEvent)) return false;
#else
    if (!super::Init(irda)) return false;
#endif
    
    // save name db service
    fNameService = nameService;

    // need an lsapconn
    fLSAPConn = TLSAPConn::tLSAPConn(irda, this);
    require(fLSAPConn, Fail);

    fRequestReply = fIrDA->GrabEventBlock();        // get an event block for us (wait til later?)
    require(fRequestReply, Fail);

    // Allocate, init a buffer segment
    fGetPutBuffer = CBufferSegment::New(kIASServerBufferSize);
    require(fGetPutBuffer, Fail);

    // Claim the well-known NameServer LSAP id
    fLSAPConn->AssignId(kNameServerLSAPId);

    // Start everything off
    ////// ListenStart();

    return true;

Fail:
    return false;

} // TIASServer::Init



//--------------------------------------------------------------------------------
//      NextState
//--------------------------------------------------------------------------------
void TIASServer::NextState(ULong event)
{
    TIrEvent* reqOrReply = GetCurrentEvent();

    XTRACE(kLogNextState, reqOrReply->fResult, event);

    require(reqOrReply == (TIrEvent *)fRequestReply, Fail);

    if (reqOrReply->fResult != noErr) {     // if previous request failed and
	if (reqOrReply->fEvent != kIrDisconnectReplyEvent &&    // neither a disconnect or listen reply
	    reqOrReply->fEvent != kIrListenReplyEvent) {            // then let's do a disconnect to clean up
	    XTRACE(kDisconnectRequestEvent, 0, this);
	    reqOrReply->fEvent = kIrDisconnectRequestEvent;     // request a disconnect
	    fLSAPConn->EnqueueEvent(reqOrReply);
	    return;
	}
    }

    switch (event) {
	case kIrDisconnectReplyEvent:               // disconnect finished, start up a listen again
	    XTRACE(kResettingToListenEvent, 0, 0);
	    // fall through
	    
	case kIrListenRequestEvent:
	    ListenStart();                          // xtrace inside ListenStart()
	    break;

	case kIrListenReplyEvent:
	    XTRACE(kListenReplyEvent, 0, this);
	    if (reqOrReply->fResult == noErr) {         // if listen worked
		XTRACE(kAcceptRequestEvent, 0, 0);
		// Send the listen reply back as the accept
		TIrConnLstnRequest* acceptRequest = (TIrConnLstnRequest*)GetCurrentEvent();
		acceptRequest->fEvent = kIrAcceptRequestEvent;
		fLSAPConn->EnqueueEvent(acceptRequest);
	    }
	    else {                                  // if it failed, listen again
		ListenStart();
	    }
	    break;

	case kIrAcceptReplyEvent:
	    XTRACE(kAcceptReplyEvent, 0, this);
	    GetStart();
	    break;

	case kIrPutDataReplyEvent:
	    XTRACE(kPutDataReplyEvent, 0, this);
	    GetStart();
	    break;

	case kIrGetDataReplyEvent:
	    XTRACE(kGetDataReplyEvent, 0, this);
	    ParseInput();
	    break;

	default:
	    XTRACE(kUnexpectedEvent, 0, event);
	    DebugLog("TIASServer::NextState: unknown event");
	    break;
    }
Fail:
    return;
} // TIASServer::NextState


//================================ Helper methods ================================


//--------------------------------------------------------------------------------
//      ParseInput
//--------------------------------------------------------------------------------
void TIASServer::ParseInput()
{
    UByte ctrlByte;
    Boolean lastFrame;
    Boolean ackedFrame;
    UByte iasReturnCode;
    TIASAttribute* iasEntry = nil;

    // An operation frame has been received - parse it and decide what to do with it

    fGetPutBuffer->Seek(0, kPosBeg);
    ctrlByte = fGetPutBuffer->Get();
    lastFrame = ctrlByte & kIASFrameLstBit;
    ackedFrame = ctrlByte & kIASFrameAckBit;

    XTRACE(kParseInputEvent, ctrlByte, fReceiveState);

    switch(fReceiveState) {
	case kIASServerReceiveStart:
	    if (ackedFrame) {
		// Must be an ack from my previous response (or some other bogus data)
		// Keep looking
	    }
	    else {
		fOpCode = ctrlByte & kIASFrameOpCodeMask;
		if (lastFrame) {
		    if (fOpCode == kIASOpGetValueByClass) {
			iasEntry = ParseRequest(iasReturnCode);
		    }
		    else {
			iasEntry = nil;
			iasReturnCode = kIASRetUnsupported;
		    }
		}
		else {
		    fReceiveState = kIASServerReceiveWaitFinal;
		}
	    }
	    break;

	case kIASServerReceiveWaitFinal:
	    // I didn't accept the request, so all I want to do is get the
	    // final frame of the request so I can reject it.
	    XASSERT(!ackedFrame);
	    if (lastFrame) {
		// I don't really care if they sent an ack w/final, so ignore it
		ackedFrame = false;

		// Return no such class for too large get value by class requests
		// Return unsupported for all other requests
		iasEntry = nil;
		iasReturnCode = fOpCode == kIASOpGetValueByClass ? kIASRetNoSuchClass : kIASRetUnsupported;
	    }
	    break;

	default:
	    break;
    }

    // Either respond to the current request or continue accepting more of the request
    if (lastFrame && !ackedFrame) {
	// Reply to the request
	SendResponse(iasReturnCode, iasEntry);

	// Reset the receive state
	fOpCode = kIASOpUnassigned;
	fReceiveState = kIASServerReceiveStart;
    }
    else if (fReceiveState == kIASServerReceiveWaitFinal) {
	// Ack the frame I don't want/care about
	fGetPutBuffer->Seek(0, kPosBeg);
	fGetPutBuffer->Put(fOpCode | kIASFrameAckBit);
	PutStart();
    }
    else {
	// Post another get
	GetStart();
    }

} // TIASServer::ParseInput


//--------------------------------------------------------------------------------
//      ParseRequest
//--------------------------------------------------------------------------------
TIASAttribute* TIASServer::ParseRequest(UByte& iasReturnCode)
{
    TIASClass* classItem;
    TIASAttribute* attrItem;
    //UChar theString[kIASMaxClassOrAttrStrLen+1];
    UChar classString[kIASMaxClassOrAttrStrLen+1];
    UChar attrString[kIASMaxClassOrAttrStrLen+1];

    // Prepare return for ill-formed class string or class not found
    iasReturnCode = kIASRetNoSuchClass;

    // Get the class string
    if (!GotAValidString((UChar*)classString)) {
	XTRACE(kParseRequestEvent, 4, 0);
	return nil;
    }

    // Look up the class
    classItem = fNameService->FindClass((const UChar*)classString);
    if (classItem == nil) {
	XTRACE(kParseRequestEvent, 3, 0);
	return nil;
    }

    // Prepare return for ill-formed attribute string or attribute not found
    iasReturnCode = kIASRetNoSuchAttribute;

    // Get the attribute string
    if (!GotAValidString((UChar*)attrString)) {
	XTRACE(kParseRequestEvent, 2, 0);
	return nil;
    }

    // Look up the attribute
    attrItem = classItem->FindAttribute((const UChar*)attrString);
    if (attrItem == nil) {
	//DebugPrintf("ias server, class '%s' attr '%s' not found",
	//                                  classString, attrString);
	XTRACE(kParseRequestEvent, 1, 0);
	return nil;
    }

    // Found the attribute
    iasReturnCode = kIASRetOkay;
    XTRACE(kParseRequestEvent, 0, 0);
    return attrItem;

} // TIASServer::ParseRequest


//--------------------------------------------------------------------------------
//      GotAValidString
//--------------------------------------------------------------------------------
Boolean TIASServer::GotAValidString(UChar* theString)
{
    // In this case, a valid string is one that is less than kIASMaxClassOrAttrStrLen
    // and is contained entirely within the current receive buffer.
    UByte nameLength;

    // Get the class string length
    nameLength = fGetPutBuffer->Get();
    if (nameLength > kIASMaxClassOrAttrStrLen) return false;

    // Get the class string
    if (fGetPutBuffer->Getn(theString, nameLength) != nameLength) return false;

    // Null terminate the string
    theString[nameLength] = 0;

    return true;

} // TIASServer::GotAValidString


//--------------------------------------------------------------------------------
//      SendResponse
//--------------------------------------------------------------------------------
void TIASServer::SendResponse(UByte iasReturnCode, TIASAttribute* attrEntry)
{
    XTRACE(kSendResponseEvent, iasReturnCode, 0);

    // Do the header common to all types of responses
    fGetPutBuffer->Seek(0, kPosBeg);
    fGetPutBuffer->Put(fOpCode | kIASFrameLstBit);
    fGetPutBuffer->Put(iasReturnCode);

    // Have the attribute object and its elements add their data to the buffer
    if (iasReturnCode == kIASRetOkay) {
	XASSERT(attrEntry != nil);
	attrEntry->AddInfoToBuffer(fGetPutBuffer);
    }

    PutStart();

} // TIASServer::SendResponse


//--------------------------------------------------------------------------------
//      ListenStart
//--------------------------------------------------------------------------------
void TIASServer::ListenStart()
{
    XTRACE(kListenRequestEvent, 0, this);

    TIrConnLstnRequest* listenRequest = (TIrConnLstnRequest*)fRequestReply;
    listenRequest->fEvent = kIrListenRequestEvent;
    listenRequest->fResult = noErr;
    listenRequest->fDevAddr = 0;
    listenRequest->fLSAPId = 0;
    listenRequest->fMyQOS = fIrDA->GetMyQOS();
    listenRequest->fPeerQOS = fIrDA->GetPeerQOS();
    listenRequest->fData = nil;
    fLSAPConn->EnqueueEvent(listenRequest);

    fReceiveState = kIASServerReceiveStart;

} // TIASServer::ListenStart


//--------------------------------------------------------------------------------
//      GetStart
//--------------------------------------------------------------------------------
void TIASServer::GetStart()
{
    XTRACE(kGetDataRequestEvent, 0, this);

    TIrGetRequest* getRequest = (TIrGetRequest*)fRequestReply;
    getRequest->fEvent = kIrGetDataRequestEvent;
    getRequest->fData = fGetPutBuffer;
    getRequest->fOffset = 0;
    getRequest->fLength = fGetPutBuffer->GetSize();
    fLSAPConn->EnqueueEvent(getRequest);

} // TIASServer::GetStart


//--------------------------------------------------------------------------------
//      PutStart
//--------------------------------------------------------------------------------
void TIASServer::PutStart()
{
    XTRACE(kPutDataRequestEvent, 0, this);

    TIrPutRequest* putRequest = (TIrPutRequest*)fRequestReply;
    putRequest->fEvent = kIrPutDataRequestEvent;
    putRequest->fData = fGetPutBuffer;
    putRequest->fOffset = 0;
    putRequest->fLength = fGetPutBuffer->Position();
    fLSAPConn->EnqueueEvent(putRequest);

} // TIASServer::PutStart

