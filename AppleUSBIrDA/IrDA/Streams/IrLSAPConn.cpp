/*
    File:       IrLSAPConn.cpp

    Contains:   Implementation of the TIrLSAPConn class


*/

#include "IrLSAPConn.h"
#include "IrGlue.h"
#include "IrLMP.h"
#include "CList.h"
#include "CListIterator.h"
#include "CBufferSegment.h"
#include "IrDALog.h"

#if (hasTracing > 0 && hasLSAPConnTracing > 0)

enum IrLSAPConnTraceCodes
{
    kNullEvent = 1,
    kDestroy,
    kDeInit,
    kUnexpectedEvent,

    kDiscConnectEvent,
    kDiscListenEvent,
    kDiscDiscRequestEvent,
    kDiscLeftoversReplyEvent,

    kConnectPendConnectReplyEvent,
    kConnectPendPutReplyEvent,
    kConnectPendDiscRequestEvent,
    kConnectPendDiscReplyEvent,

    kConnectGetReplyEvent,
    kConnectWatchdogTimeoutEvent,
    kConnectDisconnectRequestEvent,
    kConnectDisconnectReplyEvent,

    kListenPendListenReplyEvent,
    kListenPendDiscRequestEvent,
    kListenPendDiscReplyEvent,

    kListenGetReplyEvent,
    kListenPutReplyEvent,
    kListenDisconnectRequestEvent,
    kListenDisconnectReplyEvent,

    kAcceptConnectRequestEvent,
    kRejectConnectRequestEvent,
    kAccRejPutReplyEvent,
    kAccRejDisconnectReplyEvent,

    kDTRPutRequestEvent,
    kDTRGetRequestEvent,
    kDTRPutReplyEvent,
    kDTRGetReplyEvent,
    kDTRCancelPutRequestEvent,
    kDTRCancelGetRequestEvent,
    kDTRCancelPutReplyEvent,
    kDTRCancelGetReplyEvent,
    kDTRDisconnectRequestEvent,
    kDTRDisconnectReplyEvent,
    kDTRReleaseRequestEvent,
    
    kMyData,
    kDataForMe,
    kDataWaitingForMe,
    kLogRejectingMyData,
    
    kGetMyId,           //
    
    kLogDiscPendingEvent,
    kLogDiscPendingClient,
    kLogDiscPendingRejectRequest,
    kLogDiscPendingMiscReply,
    kLogDiscPendingConnLstnDone,
    kLogDiscPendingInternalDisc,
    kLogDiscPendingWatchDog,
    kLogDiscPendingDiscDone,
    kLogDiscPendingRequeue,
    
    kEnqueueEvent,
    kDequeueEventStart,
    kDequeueEventEnd

};

static
EventTraceCauseDesc IrLSAPConnTraceEvents[] = {
    {kNullEvent,                    "IrLSAPConn: null event"},
    {kDestroy,                      "IrLSAPConn: destroy obj="},
    {kDeInit,                       "IrLSAPConn: DeInit"},
    {kUnexpectedEvent,              "IrLSAPConn: unexpected event"},

    {kDiscConnectEvent,             "IrLSAPConn: disc connect request"},
    {kDiscListenEvent,              "IrLSAPConn: disc listen request"},
    {kDiscDiscRequestEvent,         "IrLSAPConn: disc disconnect request"},
    {kDiscLeftoversReplyEvent,      "IrLSAPConn: disc leftovers reply"},

    {kConnectPendConnectReplyEvent, "IrLSAPConn: conn pend got LAP reply"},
    {kConnectPendPutReplyEvent,     "IrLSAPConn: conn pend put LM-PDU conn request"},
    {kConnectPendDiscRequestEvent,  "IrLSAPConn: conn pend disconnect request"},
    {kConnectPendDiscReplyEvent,    "IrLSAPConn: conn pend disconnect reply"},

    {kConnectGetReplyEvent,         "IrLSAPConn: connect got LM-PDU conn reply"},
    {kConnectWatchdogTimeoutEvent,  "IrLSAPConn: connect watchdog timed out"},
    {kConnectDisconnectRequestEvent,"IrLSAPConn: connect disconnect request"},
    {kConnectDisconnectReplyEvent,  "IrLSAPConn: connect disconnect reply"},

    {kListenPendListenReplyEvent,   "IrLSAPConn: lstn pend got LAP reply"},
    {kListenPendDiscRequestEvent,   "IrLSAPConn: lstn pend disconnect request"},
    {kListenPendDiscReplyEvent,     "IrLSAPConn: lstn pend disconnect reply"},

    {kListenGetReplyEvent,          "IrLSAPConn: listen got LM-PDU conn request"},
    {kListenPutReplyEvent,          "IrLSAPConn: listen put LM-PDU disc reply"},
    {kListenDisconnectRequestEvent, "IrLSAPConn: listen disconnect request"},
    {kListenDisconnectReplyEvent,   "IrLSAPConn: listen disconnect reply"},

    {kAcceptConnectRequestEvent,    "IrLSAPConn: accept connect request"},
    {kRejectConnectRequestEvent,    "IrLSAPConn: reject connect request"},
    {kAccRejPutReplyEvent,          "IrLSAPConn: acc/rej put LM-PDU conn/disc reply"},
    {kAccRejDisconnectReplyEvent,   "IrLSAPConn: acc/rej disconnect reply"},

    {kDTRPutRequestEvent,           "IrLSAPConn: DTR put request"},
    {kDTRGetRequestEvent,           "IrLSAPConn: DTR get request"},
    {kDTRPutReplyEvent,             "IrLSAPConn: DTR put reply"},
    {kDTRGetReplyEvent,             "IrLSAPConn: DTR get reply"},
    {kDTRCancelPutRequestEvent,     "IrLSAPConn: DTR cancel put request"},
    {kDTRCancelGetRequestEvent,     "IrLSAPConn: DTR cancel get request"},
    {kDTRCancelPutReplyEvent,       "IrLSAPConn: DTR cancel put reply"},
    {kDTRCancelGetReplyEvent,       "IrLSAPConn: DTR cancel get reply"},
    {kDTRDisconnectRequestEvent,    "IrLSAPConn: DTR disconnect request"},
    {kDTRDisconnectReplyEvent,      "IrLSAPConn: DTR disconnect reply"},
    {kDTRReleaseRequestEvent,       "IrLSAPConn: DTR release request"},
    
    {kMyData,                       "IrLSAPConn: checking packet header"},
    {kDataForMe,                    "IrLSAPConn: data for this LSAP" },
    {kDataWaitingForMe,             "IrLSAPConn: data waiting for this LSAP" },
    {kLogRejectingMyData,           "IrLSAPConn: rejecting listen attempt, state="},

    {kGetMyId,                      "IrLSAPConn: get my id. obj=, id="},
    {kLogDiscPendingEvent,          "IrLSAPConn: disconnect pending, 0, event"},
    {kLogDiscPendingClient,         "IrLSAPConn: disconnect pending, client=="},
    {kLogDiscPendingRejectRequest,  "IrLSAPConn: disconnect pending, rejecting request"},
    {kLogDiscPendingMiscReply,      "IrLSAPConn: disconnect pending, reply. event, pendevent"},
    {kLogDiscPendingConnLstnDone,   "IrLSAPConn: disconnect pending, connlstn done"},
    {kLogDiscPendingInternalDisc,   "IrLSAPConn: disconnect pending, internal discon (pending event, result)"},
    {kLogDiscPendingWatchDog,       "IrLSAPConn: disconnect pending, watchdog timer"},
    {kLogDiscPendingDiscDone,       "IrLSAPConn: disconnect pending, disconnect complete, result="},
    {kLogDiscPendingRequeue,        "IrLSAPConn: disconnect pending, requeue event"},


    {kEnqueueEvent,                 "IrLSAPConn: Event Queued"},
    {kDequeueEventStart,            "IrLSAPConn: Event Start"},
    {kDequeueEventEnd,              "IrLSAPConn: Event End"}
};

#define XTRACE(x, y, z) IrDALogAdd( x, y, (uintptr_t)z & 0xffff, IrLSAPConnTraceEvents, true)
#else
#define XTRACE(x, y, z) ((void)0)
#endif

#define GetLMP  (fIrDA->GetLMP())

#define super TIrStream
    OSDefineMetaClassAndStructors(TLSAPConn, TIrStream);

//--------------------------------------------------------------------------------
//      TLSAPConn
//--------------------------------------------------------------------------------
/*static*/
TLSAPConn *
TLSAPConn::tLSAPConn(TIrGlue* irda, TIrStream* client)
{
    TLSAPConn *obj = new TLSAPConn;
    
    XTRACE(kNullEvent, 0, obj);
    
    if (obj && !obj->Init(irda, client)) {
	obj->release();
	obj = nil;
    }
    return obj;
}


//--------------------------------------------------------------------------------
//      free
//--------------------------------------------------------------------------------
void
TLSAPConn::free()
{
    XTRACE(kDestroy, 0, this);
    // Free things allocated by TLSAPConn
    // Release my lsapId if it was obtained
    if ((fMyLSAPId != kInvalidLSAPId) && (fMyLSAPId != kNameServerLSAPId)) {
	fIrDA->ReleaseLSAPId(fMyLSAPId);
    }
    fMyLSAPId = kInvalidLSAPId;

    if (fPendingRequests) {     // cleanup pending event list
	fPendingRequests->release();
	fPendingRequests = nil;
    }

    super::free();
} // TLSAPConn::free


//--------------------------------------------------------------------------------
//      Init
//--------------------------------------------------------------------------------
Boolean TLSAPConn::Init(TIrGlue* irda, TIrStream* client)
{

    fState = kLSAPConnDisconnected;
    fConnecting = false;
    fClient = nil;
    fResult = noErr;
    
    fPendConnLstn = nil;
    fConnLstnUserData = nil;
    fGetData = nil;
    fGetOffset = fGetLength = 0;
    
    fMyLSAPId = fPeerLSAPId = kInvalidLSAPId;
    fDevAddr = fLSAPId = 0;
    
    fPendingRequests = nil;
    fWatchdogTimerActive = false;
    fWatchdogTimerCount = 0;


#if (hasTracing > 0 && hasLSAPConnTracing > 0)
    if (!super::Init(irda, IrLSAPConnTraceEvents, kEnqueueEvent)) return false;
#else
    if (!super::Init(irda)) return false;
#endif
    
    // All confirms/indications directed back to the client
    fClient = client;
    
    fPendingRequests = CList::cList();
    require(fPendingRequests, Fail);

    return true;

Fail:
    
    return false;
    
} // TLSAPConn::Init


//--------------------------------------------------------------------------------
//      AssignId
//--------------------------------------------------------------------------------
void TLSAPConn::AssignId(ULong id)
{
    XASSERT(fMyLSAPId == kInvalidLSAPId);
    fMyLSAPId = (UByte)id;

} // TLSAPConn::AssignId


//--------------------------------------------------------------------------------
//      GetMyLSAPId
//--------------------------------------------------------------------------------
UByte TLSAPConn::GetMyLSAPId(void)
{
    XTRACE(kGetMyId, 0, this);
    XTRACE(kGetMyId, 0, fMyLSAPId);
    return fMyLSAPId;
};



//--------------------------------------------------------------------------------
//      NextState
//--------------------------------------------------------------------------------
void TLSAPConn::NextState(ULong event)
{
    switch (fState) {
	case kLSAPConnDisconnected:
	    HandleDisconnectedStateEvent(event);
	    break;

	case kLSAPConnConnectPending:
	    HandleConnectPendingStateEvent(event);
	    break;

	case kLSAPConnConnect:
	    HandleConnectStateEvent(event);
	    break;

	case kLSAPConnListenPending:
	    HandleListenPendingStateEvent(event);
	    break;

	case kLSAPConnListen:
	    HandleListenStateEvent(event);
	    break;

	case kLSAPConnAccept:
	    HandleAcceptStateEvent(event);
	    break;

	case kLSAPConnDataTransferReady:
	    HandleDataTransferReadyStateEvent(event);
	    break;

	case kLSAPDisconnectPending:
	    HandleDisconnectPendingStateEvent(event);
	    break;


	default:
	    DebugLog("TLSAPConn::NextState: bad fState");
	    break;
    }

} // TLSAPConn::NextState


//--------------------------------------------------------------------------------
//      HandleDisconnectedStateEvent
//--------------------------------------------------------------------------------
void TLSAPConn::HandleDisconnectedStateEvent(ULong event)
{
    switch (event) {
	case kIrConnectRequestEvent:
	case kIrListenRequestEvent:
	    {
		XTRACE(event == kIrConnectRequestEvent ? kDiscConnectEvent : kDiscListenEvent,  fMyLSAPId, 0);
		// Forward the connect/listen request to IrLAPConn (via LMP station control)
		TIrConnLstnRequest* connLstnReq = (TIrConnLstnRequest*)GetCurrentEvent();

		// Queue request for LMP
		PassRequestToLMP();

		// Remember original request (to distinguish internal/external disconnect replies)
		SaveCurrentRequest();

		// Keep track of original connect request so IrLAPConn can reply
		XASSERT(fPendConnLstn == nil);
		fPendConnLstn = GetCurrentEvent();

		// Remember the user data buffer until needed by GetControlFrame or PutControlFrame
		fConnLstnUserData = connLstnReq->fData;

		if (event == kIrConnectRequestEvent) {
		    fConnecting = true;

		    // For connect, remember lsap id we're connecting to
		    fPeerLSAPId = connLstnReq->fLSAPId;

		    // Wait for bind complete
		    fState = kLSAPConnConnectPending;
		}
		else {
		    fConnecting = false;

		    // For listen, set lsap id to pending connect
		    fPeerLSAPId = kPendingConnectLSAPId;

		    // Wait for bind complete
		    fState = kLSAPConnListenPending;
		}
	    }
	    break;

	case kIrDisconnectRequestEvent:
	    {
		XTRACE(kDiscDiscRequestEvent, 0, 0);
		// We're not connected - return reply back to client
		TIrDisconnectRequest* disconnectReq = (TIrDisconnectRequest*)GetCurrentEvent();
		disconnectReq->fEvent = kIrDisconnectReplyEvent;
		fClient->EnqueueEvent(disconnectReq);
	    }
	    break;

	case kIrGetDataRequestEvent:
	case kIrPutDataRequestEvent:
	    {
		// Throw any get/put requests that were in the pipeline back with an error
		TIrEvent* getPutReq = (TIrEvent*)GetCurrentEvent();
		XTRACE(kDiscLeftoversReplyEvent, event, 0);
		XTRACE(kDiscLeftoversReplyEvent, 0, fClient);   // testing
		getPutReq->fEvent = (UByte)RequestIdToReplyId(getPutReq->fEvent);
		getPutReq->fResult = kIrDAErrNotConnected;  // ***FIXME: Better result code?
		fClient->EnqueueEvent(getPutReq);
	    }
	    break;

	case kIrGetDataReplyEvent:
	case kIrPutDataReplyEvent:
	case kIrDisconnectReplyEvent:
	    {
		check(event != kIrDisconnectReplyEvent);     // jdg: shouldn't be getting disconnect complete anymore
		// Some requests unwind after going to the disconnect state
		// Pass the replies up to the client
		TIrEvent* reply = (TIrEvent*)GetCurrentEvent();
		XTRACE(kDiscLeftoversReplyEvent, event, reply->fPendEvent);
		XTRACE(kDiscLeftoversReplyEvent, 0, fClient);   // testing
		reply->fEvent = (UByte)RequestIdToReplyId(reply->fPendEvent);
		fClient->EnqueueEvent(reply);
	    }
	    break;

	default:
	    XTRACE(kUnexpectedEvent, fState, event);
	    DebugLog("TLSAPConn::HandleDisconnectedStateEvent: bad event");
	    break;
    }

} // TLSAPConn::HandleDisconnectedStateEvent


//--------------------------------------------------------------------------------
//      HandleConnectPendingStateEvent
//--------------------------------------------------------------------------------
void TLSAPConn::HandleConnectPendingStateEvent(ULong event)
{
    switch (event) {
	case kIrConnectReplyEvent:
	    {
		TIrConnLstnReply* connectReply = (TIrConnLstnReply*)GetCurrentEvent();
		XTRACE(kConnectPendConnectReplyEvent, 0, connectReply->fResult);
		if (connectReply->fResult != noErr) {
		    // Connect failed - unbind from LAPConn.
		    DisconnectStart(connectReply->fResult);     // need to take this lsapconn off lapconn's list
		    //fPendConnLstn = nil;                      // jdg
		    //fState = kLSAPConnDisconnected;               // jdg added
		    //fClient->EnqueueEvent(GetCurrentEvent()); // jdg added
		}
		else {
		    // JDG: SAVE SOME USEFUL INFO from the connect reply event record!
		    fDevAddr = connectReply->fDevAddr;  // restored to event record during listen complete
		    fLSAPId  = connectReply->fLSAPId;
		    
		    // Connect succeeded - send LM-PDU connect request
		    PutControlFrame(kLMPDUConnectRequest, 0);
		}
	    }
	    break;

	case kIrPutDataReplyEvent:
	    {
		TIrPutReply* putReply = (TIrPutReply*)GetCurrentEvent();
		XTRACE(kConnectPendPutReplyEvent, 0, putReply->fResult);
		// The LM-PDU connect request has been sent and received by peer device
		if (putReply->fResult != noErr) {
		    // Put failed (perhaps IrLAP disconnected) - unbind from LAPConn.
		    DisconnectStart(putReply->fResult);
		}
		else {
		    // Start watchdog timer
		    StartConnectTimer();
		    // Now wait for the LM-PDU connect reply
		    GetControlFrame();
		    fState = kLSAPConnConnect;
		}
	    }
	    break;

	case kIrDisconnectRequestEvent:
	    {
		XTRACE(kConnectPendDiscRequestEvent, 0, 0);
		// Remember original request (to distinguish internal/external disconnect replies)
		SaveCurrentRequest();
		fState = kLSAPDisconnectPending;            // JDG: we're on the way out, change state
		// Pass disconnect on
		PassRequestToLMP();
	    }
	    break;

	case kIrDisconnectReplyEvent:
	    {
		check(event != kIrDisconnectReplyEvent); // jdg: shouldn't be getting disconnect complete anymore
		fState = kLSAPConnDisconnected;
		if (InternalDisconnectRequest()) {
		    XTRACE(kConnectPendDiscReplyEvent, 0, fResult);
		    ConnLstnComplete(fResult);
		}
		else {
		    XTRACE(kConnectPendDiscReplyEvent, 0, 0);
		    fPendConnLstn = nil;                        // jdg: this is done by ConnLstnComplete, need it here too
		    fClient->EnqueueEvent(GetCurrentEvent());
		}
	    }
	    break;

	default:
	    XTRACE(kUnexpectedEvent, fState, event);
	    DebugLog("TLSAPConn::HandleConnectPendingStateEvent: bad event");
	    break;
    }

} // TLSAPConn::HandleConnectPendingStateEvent


//--------------------------------------------------------------------------------
//      HandleConnectStateEvent
//--------------------------------------------------------------------------------
void TLSAPConn::HandleConnectStateEvent(ULong event)
{
    switch (event) {
	case kIrGetDataReplyEvent:
	    {
		TIrGetReply* getReply = (TIrGetReply*)GetCurrentEvent();
		XTRACE(kConnectGetReplyEvent, getReply->fCtrlOpCode, getReply->fResult);
		// Cancel watchdog timer
		StopConnectTimer();
		if (getReply->fResult != noErr) {
		    // Get failed (perhaps IrLAP disconnected) - unbind from LAPConn.
		    DisconnectStart(getReply->fResult);
		}
		else {
		    switch(getReply->fCtrlOpCode) {
			case kLMPDUConnectRequest:
			    DisconnectStart(kIrDAErrGeneric /* FIXME: kIrErrConnectionRace */);
			    break;

			case kLMPDUDisconnectEvent:
			    // ***FIXME: Translate fCtrlInfo to result code?
			    DisconnectStart(kIrDAErrGeneric /* FIXME: kIrErrSomeError */);
			    break;

			case kLMPDUConnectReply:
			    ConnLstnComplete(noErr);
			    fState = kLSAPConnDataTransferReady;
			    break;

			default:
			    DebugLog("TLSAPConn::HandleConnectStateEvent: unexpected ctrl opcode");
			    break;
		    }
		}
	    }
	    break;

	case kIrConnWatchdogExpiredEvent:
	    {
		XTRACE(kConnectWatchdogTimeoutEvent, 0, 0);
		// Cancel watchdog timer
		StopConnectTimer();
		
		// JDG: I don't think we can just grab our pending listen event, since it's been turned
		// into a 'get' event request and passed onto LMP and LAPConn and LAP
		//DisconnectStart(kIRErrGeneric /* FIXME: kIrErrNonResponsivePeer */, (TIrDisconnectRequest*)fPendConnLstn);
		if (1) {        // let's allocate a new event block for a disconnect request
		    TIrDisconnectRequest    *disconnectRequest;
		    disconnectRequest = (TIrDisconnectRequest*)fIrDA->GrabEventBlock(kIrDisconnectRequestEvent, sizeof(TIrDisconnectRequest));
		    check(disconnectRequest);           // better have one available
		    if (disconnectRequest) {
			// we're going to use the fPendEvent field as a flag to disconnect complete that
			// this event is one that we've allocated ourselves, not to be confused with a
			// client disconnect request, or another internally generated disconnect request
			// and one that should have the event record freed!
			disconnectRequest->fPendEvent = kIrConnWatchdogExpiredEvent;    // mark as ours 
			DisconnectStart(kIrDAErrGeneric, disconnectRequest);
			// note - in disconnect reply, it frees the event record fPendingEvent is watchdog
		    }
		}
	    }
	    break;

	case kIrDisconnectRequestEvent:
	    XTRACE(kConnectDisconnectRequestEvent, 0, 0);
	    // Cancel watchdog timer
	    StopConnectTimer();
	    // Remember original request (to distinguish internal/external disconnect replies)
	    SaveCurrentRequest();
	    fState = kLSAPDisconnectPending;            // JDG: we're on the way out, change state
	    // Pass disconnect on
	    PassRequestToLMP();
	    break;

	case kIrDisconnectReplyEvent:
	    check(event != kIrDisconnectReplyEvent); // jdg: shouldn't be getting disconnect complete anymore
	    fState = kLSAPConnDisconnected;
	    if (InternalDisconnectRequest()) {
		XTRACE(kConnectDisconnectReplyEvent, 0, fResult);
		ConnLstnComplete(fResult);
	    }
	    else {
		XTRACE(kConnectDisconnectReplyEvent, 0, 0);
		fPendConnLstn = nil;                        // jdg: this is done by ConnLstnComplete, need it here too
		fClient->EnqueueEvent(GetCurrentEvent());
	    }
	    break;

	default:
	    XTRACE(kUnexpectedEvent, fState, event);
	    DebugLog("TLSAPConn::HandleConnectStateEvent: bad event");
	    break;
    }

} // TLSAPConn::HandleConnectStateEvent


//--------------------------------------------------------------------------------
//      HandleListenPendingStateEvent
//--------------------------------------------------------------------------------
void TLSAPConn::HandleListenPendingStateEvent(ULong event)
{
    switch (event) {
	case kIrListenReplyEvent:
	    {
		TIrConnLstnReply* listenReply = (TIrConnLstnReply*)GetCurrentEvent();
		XTRACE(kListenPendListenReplyEvent, fMyLSAPId, listenReply->fResult);
		if (listenReply->fResult != noErr) {
		    // Listen failed - unbind from LAPConn.
		    DisconnectStart(listenReply->fResult);  // jdg . may be broken
		    /////////////////////////////////////////////////////////////////////////////
		    //fPendConnLstn = nil;                      // jdg added
		    //fState = kLSAPConnDisconnected;               // jdg added
		    //fClient->EnqueueEvent(GetCurrentEvent()); // jdg added
		    
		}
		else {
		    // JDG: SAVE SOME USEFUL INFO from the listen reply event record!
		    fDevAddr = listenReply->fDevAddr;   // restored to event record during listen complete
		    fLSAPId  = listenReply->fLSAPId;
		    
		    // Wait for an LM-PDU connect request
		    GetControlFrame();
		    fState = kLSAPConnListen;
		}
	    }
	    break;

	case kIrDisconnectRequestEvent:
	    XTRACE(kListenPendDiscRequestEvent, 0, 0);
	    // Remember original request (to distinguish internal/external disconnect replies)
	    SaveCurrentRequest();
	    fState = kLSAPDisconnectPending;            // JDG: we're on the way out, change state
	    // Pass disconnect on
	    PassRequestToLMP();
	    break;

	case kIrDisconnectReplyEvent:
	    check(event != kIrDisconnectReplyEvent); // jdg: shouldn't be getting disconnect complete anymore
	    fState = kLSAPConnDisconnected;
	    if (InternalDisconnectRequest()) {
		XTRACE(kListenPendDiscReplyEvent, 1, fResult);
		// jdg: well if it's an internal disconnect, why bother the lsap owner with an
		// abort?  let's just requeue it and let it work later
		ConnLstnComplete(fResult);
		if (0) {                // needs more testing
		    TIrConnLstnRequest *req = (TIrConnLstnRequest*)GetCurrentEvent();
		    check(req->fPendEvent == kIrListenRequestEvent);
		    req->fEvent = req->fPendEvent;      // restore original request
		    fPendConnLstn = nil;                // reset pending
		    this->EnqueueEvent(req);            // requeue back for another try
		}
	    }
	    else {
		XTRACE(kListenPendDiscReplyEvent, 2, 0);
		if (1) {            // jdg: new code to clean up pending listen request
		    TIrConnLstnReply* reply = (TIrConnLstnReply*)fPendConnLstn;
		    XASSERT(fPendConnLstn != nil);      // there should be a pending listen at this point
		    reply->fEvent = (UByte)RequestIdToReplyId(reply->fPendEvent);   // turn into listen complete
		    reply->fResult = kIrDAErrRequestCanceled;
		    fClient->EnqueueEvent(reply);
		}
		fPendConnLstn = nil;                        // jdg: this is done by ConnLstnComplete, need it here too
		fClient->EnqueueEvent(GetCurrentEvent());   // jdg: or we can't do another listen/connect request
	    }
	    break;

	default:
	    XTRACE(kUnexpectedEvent, fState, event);
	    DebugLog("TLSAPConn::HandleListenPendingStateEvent: bad event");
	    break;
    }

} // TLSAPConn::HandleListenPendingStateEvent


//--------------------------------------------------------------------------------
//      HandleListenStateEvent
//--------------------------------------------------------------------------------
void TLSAPConn::HandleListenStateEvent(ULong event)
{   
    switch (event) {
	case kIrGetDataReplyEvent:
	    {
		TIrGetReply *getReply = (TIrGetReply *) GetCurrentEvent();
		XTRACE(kListenGetReplyEvent, getReply->fCtrlOpCode, getReply->fResult);
		if (getReply->fResult != noErr) {
		    // Get failed (perhaps IrLAP disconnected) - unbind from LAPConn.
		    DisconnectStart(getReply->fResult);     // jdg - may be broken
		    //fPendConnLstn = nil;                      // jdg added
		    //fState = kLSAPConnDisconnected;               // jdg added
		    //fClient->EnqueueEvent(GetCurrentEvent()); // jdg added
		}
		else {
		    switch(getReply->fCtrlOpCode) {
			case kLMPDUDataEvent:
			case kLMPDUConnectReply:
			    PutControlFrame(kLMPDUDisconnectEvent, kIrDataSentOnDiscLSAPConn);
			    break;

			case kLMPDUConnectRequest:
			    {
				TIrConnLstnReply* listenReply = (TIrConnLstnReply*)GetCurrentEvent();
    
				// Do the LM-Listen reply
				listenReply->fLSAPId = fPeerLSAPId;
				ConnLstnComplete(noErr);
				fState = kLSAPConnAccept;
			    }
			    break;

			default:
			    DebugLog("TLSAPConn::HandleListenStateEvent: unexpected ctrl opcode");
			    break;
		    }
		}
	    }
	    break;

	case kIrPutDataReplyEvent:
	    {
		TIrPutReply *putReply = (TIrPutReply *) GetCurrentEvent();
		XTRACE(kListenPutReplyEvent, 0, putReply->fResult);
		// The LM-PDU disconnect event has been sent and received by peer device
		if (putReply->fResult != noErr) {
		    // Put failed (perhaps IrLAP disconnected) - unbind from LAPConn.
		    DisconnectStart(putReply->fResult);
		}
		else {
		    // Wait for an LM-PDU connect request (again)
		    GetControlFrame();
		}
	    }
	    break;

	case kIrDisconnectRequestEvent:
	    XTRACE(kListenDisconnectRequestEvent, 0, 0);
	    // Remember original request (to distinguish internal/external disconnect replies)
	    SaveCurrentRequest();
	    fState = kLSAPDisconnectPending;            // JDG: we're on the way out, change state
	    // Pass disconnect on
	    PassRequestToLMP();
	    break;

	case kIrDisconnectReplyEvent:
	    check(event != kIrDisconnectReplyEvent); // jdg: shouldn't be getting disconnect complete anymore
	    fState = kLSAPConnDisconnected;
	    if (InternalDisconnectRequest()) {
		XTRACE(kListenDisconnectReplyEvent, 0, fResult);
		ConnLstnComplete(fResult);
		if (0) {            // jdg: let's reissue the listen instead of aborting it
		    TIrConnLstnRequest *req = (TIrConnLstnRequest*)GetCurrentEvent();
		    //check(req->fPendEvent == kIrListenRequestEvent);
		    if (req->fPendEvent == kIrListenRequestEvent) { // if a listen pending
			req->fEvent = req->fPendEvent;      // restore original request
			fPendConnLstn = nil;                // reset pending
			this->EnqueueEvent(req);            // requeue back for another try
		    }
		}

	    }
	    else {
		XTRACE(kListenDisconnectReplyEvent, 0, 0);
		fPendConnLstn = nil;                        // jdg: this is done by ConnLstnComplete, need it here too
		fClient->EnqueueEvent(GetCurrentEvent());
	    }
	    break;

	default:
	    XTRACE(kUnexpectedEvent, fState, event);
	    DebugLog("TLSAPConn::HandleListenStateEvent: bad event");
	    break;
    }

} // TLSAPConn::HandleListenStateEvent


//--------------------------------------------------------------------------------
//      HandleAcceptStateEvent
//--------------------------------------------------------------------------------
void TLSAPConn::HandleAcceptStateEvent(ULong event)
{
    switch (event) {
	case kIrAcceptRequestEvent:
	    // Client accepts the incoming connection
	    XTRACE(kAcceptConnectRequestEvent, 0, 0);
	    // Remember the accept request (to distinguish internal/external disconnect replies)
	    SaveCurrentRequest();
	    // Remember the user data buffer until needed by PutControlFrame
	    fConnLstnUserData = ((TIrConnLstnRequest*)GetCurrentEvent())->fData;
	    // Send a LM-Listen reply
	    PutControlFrame(kLMPDUConnectReply, 0);
	    break;

	case kIrDisconnectRequestEvent:
	    // Client rejects the incoming connection
	    XTRACE(kRejectConnectRequestEvent, 0, 0);
	    // Remember the disconnect request (to distinguish internal/external disconnect replies)
	    SaveCurrentRequest();
	    // Send a Disconnect request
	    PutControlFrame(kLMPDUDisconnectEvent, kIrUserRequestedDisconnect);
	    break;

	case kIrPutDataReplyEvent:
	    XTRACE(kAccRejPutReplyEvent, 0, 0);
	    if (GetCurrentEvent()->fPendEvent == kIrAcceptRequestEvent) {
		// Finish up the accept of the connection
		// The LM-PDU connect reply has been sent and received by peer device
		// Now ready to send and receive data on this connection
		// Let the caller know by replying to the accept
		TIrPutReply* putReply = (TIrPutReply*)GetCurrentEvent();
		if (putReply->fResult != noErr) {
		    DisconnectStart(putReply->fResult);
		}
		else {
		    ConnLstnComplete(noErr);
		    fState = kLSAPConnDataTransferReady;
		}
	    }
	    else {
		XASSERT(GetCurrentEvent()->fPendEvent == kIrDisconnectRequestEvent);
		// Finish up the rejection of the connection
		// The LM-PDU disconnect has been sent and received by peer device
		// Now unbind from LAPConn.
		// Note: passing noErr since initiated by client.
		// Note: Do the disconnect even if error came back from put.
		DisconnectStart(noErr);
	    }
	    break;

	case kIrDisconnectReplyEvent:
	    check(event != kIrDisconnectReplyEvent); // jdg: shouldn't be getting disconnect complete anymore
	    XTRACE(kAccRejDisconnectReplyEvent, 0, fResult);
	    // LAPConn unbind complete.  Now complete the accept or disconnect request
	    ConnLstnComplete(fResult);
	    fState = kLSAPConnDisconnected;
	    break;

	default:
	    XTRACE(kUnexpectedEvent, fState, event);
	    DebugLog("TLSAPConn::HandleAcceptStateEvent: bad event");
	    break;
    }

} // TLSAPConn::HandleAcceptStateEvent


//--------------------------------------------------------------------------------
//      HandleDataTransferReadyStateEvent
//--------------------------------------------------------------------------------
void TLSAPConn::HandleDataTransferReadyStateEvent(ULong event)
{
    switch (event) {
	case kIrGetDataRequestEvent:
	    XTRACE(kDTRGetRequestEvent, 0, 0);
	    // Remember original request (to distinguish internal/external disconnect replies)
	    SaveCurrentRequest();
	    // Pass get request to station control (LMP)
	    GetDataFrame();
	    break;

	case kIrPutDataRequestEvent:
	    XTRACE(kDTRPutRequestEvent, 0, 0);
	    // Remember original request (to distinguish internal/external disconnect/put replies)
	    SaveCurrentRequest();
	    // Fill in opCode, dstSel, srcSel fields and pass request to station control (LMP)
	    PutDataFrame();
	    break;

	case kIrCancelGetRequestEvent:
	case kIrCancelPutRequestEvent:
	    XTRACE(event == kIrCancelGetRequestEvent ? kDTRCancelGetRequestEvent : kDTRCancelPutRequestEvent, 0, 0);
	    // Pass request to station control (LMP)
	    PassRequestToLMP();
	    break;

	case kIrReleaseRequestEvent:
	case kIrDisconnectRequestEvent:
	    XTRACE(event == kIrReleaseRequestEvent ? kDTRReleaseRequestEvent : kDTRDisconnectRequestEvent, 0, 0);
	    // Remember original request (to distinguish internal/external disconnect replies)
	    SaveCurrentRequest();
	    // Send a Disconnect request
	    fState = kLSAPDisconnectPending;            // JDG: we're on the way out, change state
	    PutControlFrame(kLMPDUDisconnectEvent, kIrUserRequestedDisconnect);
	    break;

	case kIrGetDataReplyEvent:
	    {
		TIrGetReply* getReply = (TIrGetReply*)GetCurrentEvent();
		XTRACE(kDTRGetReplyEvent, getReply->fCtrlOpCode, 0);
		if (getReply->fResult != noErr) {
		    // Check to see if get was cancelled (vs error from IrLAP)
		    if (getReply->fResult == kIrDAErrRequestCanceled) {
			// Pass get reply up to the client
			fClient->EnqueueEvent(getReply);
		    }
		    // Get failed (perhaps IrLAP disconnected) - unbind from LAPConn.
		    else {
			DisconnectStart(getReply->fResult);
		    }
		}
		else {
		    switch(getReply->fCtrlOpCode) {
			case kLMPDUDataEvent:
			    // Pass get reply up to the client
			    fClient->EnqueueEvent(getReply);
			    break;

			case kLMPDUConnectRequest:
			    // Reply with half open error
			    PutControlFrame(kLMPDUDisconnectEvent, kIrHalfOpen);
			    break;

			case kLMPDUDisconnectEvent:
			    // ***FIXME: Translate fCtrlInfo to result code?
			    DisconnectStart(kIrDAErrGeneric /* FIXME: kIrErrSomeError */);
			    break;

			default:
			    DebugLog("TLSAPConn::HandleDataTransferReadyStateEvent: unexpected ctrl opcode");
			    GetDataFrame(true /* re-"posting" the request*/);
			    break;
		    }
		}
	    }
	    break;

	case kIrPutDataReplyEvent:
	    {
		TIrPutReply* putReply = (TIrPutReply*)GetCurrentEvent();
		XTRACE(kDTRPutReplyEvent, 0, putReply->fResult);
		if (InternalPutRequest()) {
		    // If put reply was initiated by release or disconnect, send disconnect
		    if ((putReply->fPendEvent == kIrReleaseRequestEvent) || (putReply->fPendEvent == kIrDisconnectRequestEvent)) {
			DisconnectStart(noErr);
		    }
		    // Else put reply was in response to an unwanted received control frame (a connect)
		    else {
			XASSERT(putReply->fPendEvent == kIrGetDataRequestEvent);
			DisconnectStart(kIrDAErrGeneric /* FIXME: kIrErrHalfOpen */);
		    }
		}
		else {
		    // Pass put reply up to the client
		    fClient->EnqueueEvent(putReply);
		}
	    }
	    break;

	case kIrCancelGetReplyEvent:
	case kIrCancelPutReplyEvent:
	    XTRACE(event == kIrCancelGetReplyEvent ? kDTRCancelGetReplyEvent : kDTRCancelPutReplyEvent, 0, 0);
	    // Pass put reply up to the client
	    fClient->EnqueueEvent(GetCurrentEvent());
	    break;

	case kIrDisconnectReplyEvent:
	    check(event != kIrDisconnectReplyEvent); // jdg: shouldn't be getting disconnect complete anymore
	    if (InternalDisconnectRequest()) {
		// Finish up whichever request initiated the disconnect
		TIrEvent* reply = (TIrEvent*)GetCurrentEvent();
		XTRACE(kDTRDisconnectReplyEvent, 0, fResult);
		// Pass reply up to the client
		reply->fEvent = (UByte)RequestIdToReplyId(reply->fPendEvent);
		reply->fResult = fResult;
		fClient->EnqueueEvent(reply);
	    }
	    else {
		TIrDisconnectReply* disconnectReply;
		disconnectReply = (TIrDisconnectReply*)GetCurrentEvent();
		XTRACE(kDTRDisconnectReplyEvent, 0, disconnectReply->fResult);
		// Pass disconnect reply up to the client
		fClient->EnqueueEvent(disconnectReply);
	    }
	    fPendConnLstn = nil;                        // jdg: this is done by ConnLstnComplete, need it here too (?)
	    fState = kLSAPConnDisconnected;
	    break;

	default:
	    XTRACE(kUnexpectedEvent, fState, event);
	    DebugLog("TLSAPConn::HandleDataTransferReadyStateEvent: bad event");
	    break;
    }

} // TLSAPConn::HandleDataTransferReadyStateEvent

//--------------------------------------------------------------------------------
//      HandleDisconnectPendingStateEvent
//--------------------------------------------------------------------------------
void TLSAPConn::HandleDisconnectPendingStateEvent(ULong event)
{
    TIrEvent* eventBlock = (TIrEvent*)GetCurrentEvent();
    //static int rejectCount = 0;               // temp debugging
    
    XTRACE(kLogDiscPendingEvent, 0, event);
    XTRACE(kLogDiscPendingClient, 0, fClient);
    
    switch (event) {
    
	case kIrDisconnectRequestEvent:     // stall these requests until we can handle them
	case kIrConnectRequestEvent:
	case kIrListenRequestEvent:
	    fPendingRequests->InsertLast(GetCurrentEvent());
	    break;

					    // we have a disconnect pending, don't do new requests
	case kIrAcceptRequestEvent:         // we have a disconnect pending, don't do new requests yet
	case kIrGetDataRequestEvent:
	case kIrPutDataRequestEvent:
	    // just send these back to the client
	    //rejectCount++;        // debugging
	    //XTRACE(kLogDiscPendingRejectRequest, rejectCount, event);
	    eventBlock->fEvent = (UByte)RequestIdToReplyId(eventBlock->fEvent);
	    eventBlock->fResult = kIrDAErrNotConnected;
	    fClient->EnqueueEvent(eventBlock);
	    //if (rejectCount == 20) DebugLog("lsapconn reject count hit 20");
	    break;

	    // convert event back to reply of original, who cares if it worked or not,
	    // we have a disconnect reply coming real soon now
	case kIrListenReplyEvent:
	case kIrConnectReplyEvent:
	    eventBlock->fResult = kIrDAErrNotConnected;         // lap listen/conn reply, but we're disconnecting
	    // continue on
	case kIrGetDataReplyEvent:
	case kIrPutDataReplyEvent:
	    XTRACE(kLogDiscPendingMiscReply, event, eventBlock->fPendEvent);
	    if (GetCurrentEvent() == fPendConnLstn) {
		XTRACE(kLogDiscPendingConnLstnDone, 0, fPendConnLstn);
		fPendConnLstn = nil;            // there isn't a listen/connect pending event anymore
	    }
	    
	    eventBlock->fEvent = (UByte)RequestIdToReplyId(eventBlock->fPendEvent);
	    // don't stop on the result, it might have worked, esp with 
	    // a lookup release request!
	    //eventBlock->fResult = kCommErrNotConnected;
	    
	    // oh my. if the client requested a disconnect, then the event is a putreply, even
	    // though the client requested a disconnect.  Sigh.  Let's rewrite this to use a
	    // different event record in the lsap/lmp/lap path than the one passed to us by
	    // the client!  Ok, let's see what's getting returned to the client ....
	    if (eventBlock->fEvent == kIrDisconnectReplyEvent       // client asked for a disconnect
		|| eventBlock->fEvent == kIrReleaseReplyEvent) {    // client asked for a disconnect via release
		check(fPendConnLstn == nil);        // should already have been cleared
		DisconnectStart(noErr);             // need to unbind from LAPConn's list of active LSAPConns
	    }
	    else
		fClient->EnqueueEvent(eventBlock);
	    break;
	

	case kIrDisconnectReplyEvent:
	    if (InternalDisconnectRequest()) {
		// Finish up whichever request initiated the disconnect
		XTRACE(kLogDiscPendingInternalDisc, eventBlock->fPendEvent, fResult);
		// Pass reply up to the client
		// but only if the event wasn't a disconnect generated by the watchdogtimer
		if (eventBlock->fPendEvent != kIrConnWatchdogExpiredEvent) {    // have a client's event block
		    eventBlock->fEvent = (UByte)RequestIdToReplyId(eventBlock->fPendEvent);
		    eventBlock->fResult = fResult;
		    fClient->EnqueueEvent(eventBlock);
		}
		else {          // else we have fPendEvent of kIrConnWatchdogExpiredEvent, so it's our event record
		    DebugLog("IrLSAPConn: just fyi, got disconnect reply after watchdog timeout");
		    XTRACE(kLogDiscPendingWatchDog, 0, 0);
		    fIrDA->ReleaseEventBlock(eventBlock);
		}
	    }
	    else {                              // original request was a disconnect request
		XTRACE(kLogDiscPendingDiscDone, 0, eventBlock->fResult);
		// Pass disconnect reply up to the client
		fClient->EnqueueEvent(eventBlock);
	    }
	    check(fPendConnLstn == nil);        // should already have been cleared
	    fState = kLSAPConnDisconnected;
	    
	    // Ok, now that we're cleanly disconnected (I hope), let's requeue all
	    // the pending requests that we've collected since the disconnect started
	    if (fPendingRequests && !fPendingRequests->Empty()) {
		TIrEvent* request;
		CListIterator *iter = CListIterator::cListIterator(fPendingRequests);
		for (request = (TIrEvent*)iter->FirstItem();
		     iter->More(); request = (TIrEvent*)iter->NextItem()) {
			XTRACE(kLogDiscPendingRequeue, 0, request->fEvent);
			this->EnqueueEvent(request);
		}
		while (!fPendingRequests->Empty())
		    fPendingRequests->RemoveLast();
		iter->release();
	    }

	    break;
	    
	default:
	    XTRACE(kUnexpectedEvent, fState, event);
	    DebugLog("TLSAPConn::HandleDisconnectPendingStateEvent: bad event");
	    break;
    }
} // HandleDisconnectPendingStateEvent

//================================ Helper methods ================================


//--------------------------------------------------------------------------------
//      SaveCurrentRequest
//--------------------------------------------------------------------------------
void TLSAPConn::SaveCurrentRequest()
{
    TIrEvent* request = GetCurrentEvent();

    // Store the original request in fPendEvent
    request->fPendEvent = request->fEvent;

} // TLSAPConn::SaveCurrentRequest


//--------------------------------------------------------------------------------
//      InternalDisconnectRequest?
//--------------------------------------------------------------------------------
Boolean TLSAPConn::InternalDisconnectRequest()
{
    // Is the current request *not* a disconnect request.  If its not, then this must
    // be a reply to an internally generated disconnect request sent to IrLMP.
    return GetCurrentEvent()->fPendEvent != kIrDisconnectRequestEvent;

} // TLSAPConn::InternalDisconnectRequest


//--------------------------------------------------------------------------------
//      InternalPutRequest?
//--------------------------------------------------------------------------------
Boolean TLSAPConn::InternalPutRequest()
{
    // Is the current request *not* a put request.  If its not, then this must
    // be a reply to an internally generated put request sent to IrLMP.
    return GetCurrentEvent()->fPendEvent != kIrPutDataRequestEvent;

} // TLSAPConn::InternalPutRequest


//--------------------------------------------------------------------------------
//      GetPendConnLstn
//--------------------------------------------------------------------------------
TIrEvent* TLSAPConn::GetPendConnLstn()
{
    // This is used by IrLAPConn so it can reply to pending conn/lstn reqs for each LSAP
    //XASSERT(fPendConnLstn != nil);     // jdg: sometimes it's already open and the pending conn/lstn is nil
    return fPendConnLstn;

} // TLSAPConn::GetPendConnLstn


//--------------------------------------------------------------------------------
//      PassRequestToLMP
//--------------------------------------------------------------------------------
void TLSAPConn::PassRequestToLMP()
{
    // All requests forwarded to LMP need to identify themselves for the reply
    TIrLSAPConnEvent* theEvent = (TIrLSAPConnEvent*)GetCurrentEvent();

    theEvent->fLSAPConn = this;
    GetLMP->EnqueueEvent(theEvent);

} // TLSAPConn::PassRequestToLMP


//--------------------------------------------------------------------------------
//      DisconnectStart
//--------------------------------------------------------------------------------
void TLSAPConn::DisconnectStart(IrDAErr result, TIrDisconnectRequest* discRequest)
{
    // JDG: review review review.
    fPendConnLstn = nil;                        // jdg: TEMP TEMP TEMP TEMP

    // Use current event block if no request block specified
    if (discRequest == nil) {
	discRequest = (TIrDisconnectRequest*)GetCurrentEvent();
    }

    discRequest->fEvent = kIrDisconnectRequestEvent;
    discRequest->fResult = kIrDAErrCancel;
    discRequest->fLSAPConn = this;

    // Save reason for disconnection for conn/lstn complete after disconnect reply
    fResult = result;

    fState = kLSAPDisconnectPending;        // JDG: we're on the way out
    
    GetLMP->EnqueueEvent(discRequest);

} // TLSAPConn::DisconnectStart


//--------------------------------------------------------------------------------
//      GetControlFrame
//--------------------------------------------------------------------------------
void TLSAPConn::GetControlFrame()
{
    TIrGetRequest* getRequest = (TIrGetRequest*)GetCurrentEvent();

    // Reset fConnLstnUserData (unhides all) to accept maximum conn user data
    if (fConnLstnUserData) {
	fConnLstnUserData->Reset();
    }

    getRequest->fEvent = kIrGetDataRequestEvent;
    getRequest->fResult = noErr;
    getRequest->fData = fConnLstnUserData;
    getRequest->fOffset = 0;
    getRequest->fLength = fConnLstnUserData ? fConnLstnUserData->GetSize() : 0;

    // Send get request to station control (LMP)
    PassRequestToLMP();

} // TLSAPConn::GetControlFrame


//--------------------------------------------------------------------------------
//      PutControlFrame
//--------------------------------------------------------------------------------
void TLSAPConn::PutControlFrame(UByte opCode, UByte info)
{
    CBuffer* data = nil;    // Default if not connect or connect reply
    TIrPutRequest* putRequest = (TIrPutRequest*)GetCurrentEvent();

    // Only send connect/accept userData if opCode is connect or connect reply.
    if ((opCode == kLMPDUConnectRequest) || (opCode == kLMPDUConnectReply)) {
	data = fConnLstnUserData;
    }

    XASSERT(fPeerLSAPId <= kLastValidLSAPId);

    putRequest->fEvent = kIrPutDataRequestEvent;
    putRequest->fResult = noErr;
    putRequest->fData = data;
    putRequest->fOffset = 0;
    putRequest->fLength = data ? data->GetSize() : 0;
    putRequest->fDstLSAPId = fPeerLSAPId;
    putRequest->fSrcLSAPId = fMyLSAPId;
    putRequest->fCtrlOpCode = opCode;
    putRequest->fCtrlInfo = info;

    // Send put request to station control (IrLMP)
    PassRequestToLMP();

} // TLSAPConn::PutControlFrame


//--------------------------------------------------------------------------------
//      GetDataFrame
//--------------------------------------------------------------------------------
void TLSAPConn::GetDataFrame(Boolean resend)
{
    TIrGetRequest* getRequest = (TIrGetRequest*)GetCurrentEvent();

    if (resend) {
	// Reset the fields from the original requests data
	getRequest->fEvent = kIrGetDataRequestEvent;
	getRequest->fResult = noErr;
	getRequest->fData = fGetData;
	getRequest->fOffset = fGetOffset;
	getRequest->fLength = fGetLength;
    }
    else {
	// Save the info needed to recreate the request for resends
	fGetData = getRequest->fData;
	fGetOffset = getRequest->fOffset;
	fGetLength = getRequest->fLength;
    }

    // Send get request to station control (LMP)
    PassRequestToLMP();

} // TLSAPConn::GetDataFrame


//--------------------------------------------------------------------------------
//      PutDataFrame
//--------------------------------------------------------------------------------
void TLSAPConn::PutDataFrame()
{
    TIrPutRequest* putRequest = (TIrPutRequest*)GetCurrentEvent();

    XASSERT(fPeerLSAPId <= kLastValidLSAPId);

    putRequest->fDstLSAPId = fPeerLSAPId;
    putRequest->fSrcLSAPId = fMyLSAPId;
    putRequest->fCtrlOpCode = kLMPDUDataEvent;
    putRequest->fCtrlInfo = 0;

    // Send put request to station control (IrLMP)
    PassRequestToLMP();

} // TLSAPConn::PutDataFrame


//--------------------------------------------------------------------------------
//      ConnLstnComplete
//--------------------------------------------------------------------------------
void TLSAPConn::ConnLstnComplete(IrDAErr result)
{
    TIrConnLstnReply* reply = (TIrConnLstnReply*)GetCurrentEvent();

#ifdef forDebug
    if ((reply->fPendEvent == kIrConnectRequestEvent) || (reply->fPendEvent == kIrListenRequestEvent)) {
	XASSERT(fPendConnLstn != nil);
    }
#endif
    fPendConnLstn = nil;
    if (reply->fPendEvent == kIrConnectRequestEvent)    // ONLY if connect response
	reply->fLSAPId = fLSAPId;                       // jdg: restore peer lsap id
	/// TODO review fLSAPID vs fPeerLSAPId

    reply->fEvent = (UByte)RequestIdToReplyId(reply->fPendEvent);
    reply->fResult = result;
    reply->fDevAddr = fDevAddr;     // jdg: restore peer address
    //reply->fLSAPId = fLSAPId;     // jdg: restore peer lsap id
    fClient->EnqueueEvent(reply);

} // TLSAPConn::ConnLstnComplete


//--------------------------------------------------------------------------------
//      YourData
//--------------------------------------------------------------------------------
Boolean TLSAPConn::YourData(TLMPDUHeader& header, Boolean justChecking)
{
    XTRACE( kMyData, header.fDstLSAPId, fMyLSAPId );
    if (header.fDstLSAPId == fMyLSAPId) {
	if (header.fSrcLSAPId == fPeerLSAPId) {
	    XTRACE( kDataForMe, header.fOpCode, header.fSrcLSAPId );
	    return true;
	}
	else if (fPeerLSAPId == kPendingConnectLSAPId) {
	    // Listen receives its connect request
	    // could be in listen or pending disconnect state ... jdg
	    XASSERT(fState == kLSAPConnListen || fState == kLSAPDisconnectPending);
	    //if (fState != kLSAPConnListen) {      // bogus,jdg:: could also be in disconnect pending or...
	    //  XTRACE(kLogRejectingMyData, 0, fState);
	    //  return false;                       // jdg: just drop it if we don't really want it
	    //}
	    XTRACE( kDataWaitingForMe, header.fOpCode,  header.fSrcLSAPId );
	    if (header.fOpCode == kLMPDUConnectRequest) {
		if (!justChecking) {
		    fPeerLSAPId = header.fSrcLSAPId;
		}
		return true;
	    }
	}
    }

    return false;

} // TLSAPConn::YourData


//--------------------------------------------------------------------------------
//      StartConnectTimer
//--------------------------------------------------------------------------------
void TLSAPConn::StartConnectTimer()
{
    fWatchdogTimerActive = true;
    fWatchdogTimerCount = 0;
    GetLMP->StartOneSecTicker();

} // TLSAPConn::StartConnectTimer


//--------------------------------------------------------------------------------
//      StopConnectTimer
//--------------------------------------------------------------------------------
void TLSAPConn::StopConnectTimer()
{
    fWatchdogTimerActive = false;
    GetLMP->StopOneSecTicker();

} // TLSAPConn::StopConnectTimer


//--------------------------------------------------------------------------------
//      OneSecTickerComplete
//--------------------------------------------------------------------------------
void TLSAPConn::OneSecTickerComplete()
{
    // Ignore the ticker complete if I'm not currently connecting
    if (!fWatchdogTimerActive) return;

    // Shouldn't be active unless we're connecting
    XASSERT(fState == kLSAPConnConnect);

    // Connect has timed out if reached the timeout count
    if (++fWatchdogTimerCount >= kWatchdogTimeoutCount) {
	NextState(kIrConnWatchdogExpiredEvent);
    }

} // TLSAPConn::OneSecTickerComplete

