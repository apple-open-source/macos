/*
    File:       IrLAPConn.c

    Contains:   Implementation of the TIrLAPConn class


*/

#include "IrLAPConn.h"
#include "IrLSAPConn.h"
#include "IrGlue.h"
#include "IrLAP.h"
#include "CList.h"
#include "CListIterator.h"
#include "IrDALog.h"

#if (hasTracing > 0 && hasLAPConnTracing > 0)

enum IrLAPConnTraceCodes
{
    kNullEvent = 1,
    kDestroy,
    kInit,
    kDeInit,
    kUnexpectedEvent,
    kLogStateEvent,

    kStandbyConnLstnRequestEvent,
    kStandbyDisconnectRequestEvent,
    kStandbyDisconnectReplyEvent,
    kStandbyDisconnectRequeue,

    kPendingConnLstnRequestEvent,
    kPendingConnLstnDeferRequest,           // jdg
    kPendingConnLstnReplyEvent,
    kPendingDisconnectRequestEvent,
    kPendingDisconnectReplyEvent,
    kPendingDisconnectRequeue,              // jdg

    kActiveConnLstnRequestEvent,
    kActiveConnLstnDeferRequest,            // jdg
    kActiveGetDataRequestEvent,
    kActiveCancelGetRequestEvent,
    kActiveDisconnectRequestEvent,
    kActiveDisconnectReplyEvent,
    kActiveDisconnectRequeue,               // jdg

    kDemuxInvalidHeaderEvent,
    kDemuxGetPendingEvent,
    kDemuxReplyPostedEvent,
    kDemuxReplyPostedEvent2,
    kDemuxNoReceiverEvent,
    kDemuxReleaseBufferEvent,

    kCancelPendingGetReqEvent,
    kCleanupPendingRcvdBufEvent,

    kWantToAdd,
    kAddingToLSAPConnList,
    kAddingLsapToList,

    kLogStartIdleDisconnectTimer,
    kLogStopIdleDisconnectTimer,
    kLogDoIdleDisconnect,
    
    kLogConnWatchDogFired,
    kLogIdleDisconnectFired,

    kLogReset,
    kLogResetLsapConn,
    kLogResetGetRequest,
    kLogResetGetReply,
    kLogResetPendingReq,

    kLogDemux,
    kLogDemuxCheckingGets1,
    kLogDemuxCheckingGets2,
    kLogAddingGetRequest1,
    kLogAddingGetRequest2,
    kLogAddingGetRequest3,
    
    kLogCleanupPendingGetRequestsAndRepliesEntry,
    kLogCleanupPendingGetRequestsAndReplies2,
    kLogCleanupPendingGetRequestsAndReplies3,

    kLogCancelPendingGetRequestsEntry,
    kLogCancelPendingGetRequests,
    kLogCancelPendingGetRequestsExit,
    
    kLogFillInLMPDUHeader1,
    kLogFillInLMPDUHeader2,

    kEnqueueEvent,
    kDequeueEventStart,
    kDequeueEventEnd
};

EventTraceCauseDesc IrLAPConnTraceEvents[] = {
    {kNullEvent,                    "irlapconn: create obj="},
    {kDestroy,                      "irlapconn: destroy obj="},
    {kInit,                         "irlapconn: init"},
    {kDeInit,                       "irlapconn: deinit"},
    {kUnexpectedEvent,              "irlapconn: unexpected event"},
    {kLogStateEvent,                "irlapconn: next state, state=, event="},

    {kStandbyConnLstnRequestEvent,  "irlapconn: standby conn/lstn request"},
    {kStandbyDisconnectRequestEvent,"irlapconn: standby disconnect request"},
    {kStandbyDisconnectReplyEvent,  "irlapconn: standby disconnect reply"},
    {kStandbyDisconnectRequeue,     "irlapconn: standby requeue pending requests"},     // jdg

    {kPendingConnLstnRequestEvent,  "irlapconn: uconnect conn/lstn request"},
    {kPendingConnLstnDeferRequest,  "irlapconn: uconnect conn/lstn defer request"},     // jdg
    {kPendingConnLstnReplyEvent,    "irlapconn: uconnect conn/lstn reply"},
    {kPendingDisconnectRequestEvent,"irlapconn: uconnect disconnect request"},
    {kPendingDisconnectReplyEvent,  "irlapconn: uconnect disconnect reply"},
    {kPendingDisconnectRequeue,     "irlapconn: uconnect requeue pending request"},

    {kActiveConnLstnRequestEvent,   "irlapconn: active conn/lstn request"},
    {kActiveConnLstnDeferRequest,   "irlapconn: active conn/lstn defer request"},       // jdg
    {kActiveGetDataRequestEvent,    "irlapconn: active get data request"},
    {kActiveCancelGetRequestEvent,  "irlapconn: active cancel get request"},
    {kActiveDisconnectRequestEvent, "irlapconn: active disconnect request"},
    {kActiveDisconnectReplyEvent,   "irlapconn: active disconnect reply"},
    {kActiveDisconnectRequeue,      "irlapconn: active requeue pending request"},

    {kDemuxInvalidHeaderEvent,      "irlapconn: invalid header"},
    {kDemuxGetPendingEvent,         "irlapconn: found get pending, event rec="},
    {kDemuxReplyPostedEvent,        "irlapconn: found lsapconn, no get. buf="},
    {kDemuxReplyPostedEvent2,       "irlapconn: found lsapconn, no get. lsapconn="},
    {kDemuxNoReceiverEvent,         "irlapconn: no receiver"},
    {kDemuxReleaseBufferEvent,      "irlapconn: release buffer"},

    {kCancelPendingGetReqEvent,     "irlapconn: cancel pending get request, lsapconn="},
    {kCleanupPendingRcvdBufEvent,   "irlapconn: cleaning up pending recd buffer"},

    {kWantToAdd,                    "irlapconn: ** want to add lsapconn to list"},
    {kAddingToLSAPConnList,         "irlapconn: ** add/del to fLSAPConnList, add=, id="},
    {kAddingLsapToList,             "irlapconn: ** add/del of lsap="},

    {kLogStartIdleDisconnectTimer,  "irlapconn: starting idle disconnect timer"},
    {kLogStopIdleDisconnectTimer,   "irlapconn: stoppng idle disconnect timer"},
    {kLogDoIdleDisconnect,          "irlapconn: doing idle disconnect"},
    
    {kLogConnWatchDogFired,         "irlapconn: conn watchdog timer fired"},
    {kLogIdleDisconnectFired,       "irlapconn: idle disconnect timer fired"},

    {kLogReset,                     "irlapconn: reset. fConnected, fState"},
    {kLogResetLsapConn,             "irlapconn: reset. lsapconn="},
    {kLogResetGetRequest,           "irlapconn: reset. getrequest="},
    {kLogResetGetReply,             "irlapconn: reset. getreply="},
    {kLogResetPendingReq,           "irlapconn: reset. pending request"},

    {kLogDemux,                     "irlapconn: demux input packet, buf="},
    {kLogDemuxCheckingGets1,        "irlapconn: demux, pending get count="},
    {kLogDemuxCheckingGets2,        "irlapconn: demux, checking event rec="},
    {kLogAddingGetRequest1,         "irlapconn: saving get req, event Rec="},
    {kLogAddingGetRequest2,         "irlapconn: saving get req, lsapconn="},
    {kLogAddingGetRequest3,         "irlapconn: saving get req, qlen=, lsapid="},

    {kLogCleanupPendingGetRequestsAndRepliesEntry,  "irlapconn: CleanupPendingGetRequestsAndReplies entry, lsapConn="},
    {kLogCleanupPendingGetRequestsAndReplies2,      "irlapconn: CleanupPendingGetRequestsAndReplies 2"},
    {kLogCleanupPendingGetRequestsAndReplies3,      "irlapconn: CleanupPendingGetRequestsAndReplies, replyBuffer="},
    
    {kLogCancelPendingGetRequestsEntry,         "irlapconn: CancelPendingGetRequestsEntry"},
    {kLogCancelPendingGetRequests,              "irlapconn: CancelPendingGetRequests"},
    {kLogCancelPendingGetRequestsExit,          "irlapconn: CancelPendingGetRequestsExit"},
    
    {kLogFillInLMPDUHeader1,        "irlapconn: putRequest"},
    {kLogFillInLMPDUHeader2,        "irlapconn: buffer"},


    {kEnqueueEvent,                 "irlapconn: Event Queued"},
    {kDequeueEventStart,            "irlapconn: Event Start"},
    {kDequeueEventEnd,              "irlapconn: Event End"}
};

#define XTRACE(x, y, z) IrDALogAdd( x, y, (uintptr_t)z & 0xffff, IrLAPConnTraceEvents, true )
#else
#define XTRACE(x, y, z) ((void)0)
#endif

#define GetLAP  (fIrDA->GetLAP())

#define super TIrStream
	OSDefineMetaClassAndStructors(TIrLAPConn, TIrStream);

//--------------------------------------------------------------------------------
//      tIrLAPConn
//--------------------------------------------------------------------------------
/*static*/
TIrLAPConn *
TIrLAPConn::tIrLAPConn(TIrGlue* irda)
{
	TIrLAPConn *obj = new TIrLAPConn;
	XTRACE(kNullEvent, 0, obj);

	if (obj && !obj->Init(irda)) {
		obj->release();
		obj = nil;
	}
	return obj;
}



//--------------------------------------------------------------------------------
//      free
//--------------------------------------------------------------------------------
void
TIrLAPConn::free()
{
    XTRACE(kDestroy, 0, this);
    
    // Free things allocated by TIrLAPConn
    StopIdleDisconnectTimer();              // make sure the timer is off

#define FREE(x) { if (x) { (x)->release(); x = nil; } }
    
    FREE(fLSAPConnList);                // Free the LSAPConn list
    FREE(fPendingGetRequests);          // Free the pending get requests list
    FREE(fUnmatchedGetReplys);          // Free the unmatched get replys list
    FREE(fPendingRequests);             // Free the pending event list


    super::free();
    
} // TIrLAPConn::free


//--------------------------------------------------------------------------------
//      Init
//--------------------------------------------------------------------------------
Boolean TIrLAPConn::Init(TIrGlue* irda)
{
    XTRACE(kInit, 0, this);
    
    
    fState = kIrLAPConnStandby;
    fConnected = false;
    fPeerDevAddr = 0;
    fLSAPConnList = nil;
    fPendingGetRequests = nil;
    fUnmatchedGetReplys = nil;
    fPendingRequests = nil;
    
    fDisconnectPending = false;
    
    
    // Init IrStream
#if (hasTracing > 0 && hasLAPConnTracing > 0)
     if (!super::Init(irda, IrLAPConnTraceEvents, kEnqueueEvent)) return false;
#else
    if (!super::Init(irda)) return false;
#endif


    // Init LSAPConn list
    fLSAPConnList = CList::cList();
    require(fLSAPConnList, Fail);

    // Init the pending get requests list
    fPendingGetRequests = CList::cList();
    require(fPendingGetRequests, Fail);

    // Init the received (but unmatched) get replies
    fUnmatchedGetReplys = CList::cList();
    require(fUnmatchedGetReplys, Fail);

    // Init the list of deferred requests
    fPendingRequests = CList::cList();
    require(fPendingRequests, Fail);
    
    return true;

Fail:

    return false;

} // TIrLAPConn::Init


//--------------------------------------------------------------------------------
//      Reset
//--------------------------------------------------------------------------------
void TIrLAPConn::Reset()
{
    
    XTRACE(kLogReset, fConnected, fState);
    
    // This will force other asserts in HandleStandbyStateEvent which check
    // to see that all of the other fields were properly reset at disconnect.

    fState          = kIrLAPConnStandby;    // Using Reset for Aync disconnect.  There
    fConnected      = false;                // will be no disconnect reply (for now).
    fPeerDevAddr    = 0;
    
    StopIdleDisconnectTimer();              // make sure the timer is off
    
    //
    // JDG: let's loop over our lsap conn list and purge everything.
    // this should pick up pending get requests and pending get replies
    // but for sanities sake, we'll check them too after this iteration.
    //
    if (fLSAPConnList && !fLSAPConnList->Empty()) {     // get rid of all pending listen/connects
	int index;
	TLSAPConn* lsapConn;
	
	XTRACE(kLogResetLsapConn, 1, fLSAPConnList->GetArraySize());
	for (index = fLSAPConnList->GetArraySize() - 1; index >= 0 ; index--) {
	    lsapConn = (TLSAPConn*)fLSAPConnList->At(index);
	    XTRACE(kLogResetLsapConn, 0, lsapConn);
	    // Complete pending get requests and delete any received buffers intended for this conn
	    CleanupPendingGetRequestsAndReplies(lsapConn, errCancel);
	}
	while (!fLSAPConnList->Empty())
	    fLSAPConnList->RemoveLast();
	XTRACE(kLogResetLsapConn, 0xffff, 0xffff);
    }
    
    if (fPendingGetRequests && !fPendingGetRequests->Empty()) {     // Cancel all pending Get requests
	CListIterator *iter = CListIterator::cListIterator(fPendingGetRequests);
	TIrGetRequest *getRequest;
	
	DebugLog("IrLapConn: reset pending get request. how?"); // shouldn't get here
	for (getRequest = (TIrGetRequest*)iter->FirstItem();
	     iter->More(); getRequest = (TIrGetRequest*)iter->NextItem()) {
	//for (getRequest = OSDynamicCast(TIrGetRequest, (OSObject *)iter->FirstItem());
	//   iter->More(); getRequest = OSDynamicCast(TIrGetRequest, (OSObject *)iter->NextItem())) {
	    XTRACE(kLogResetGetRequest, 0, getRequest);
	    // This is somewhat inefficient, since Cleanup will loop through the pending list
	    // again to match the LSAP.  But that's ok.
	    this->CleanupPendingGetRequestsAndReplies( getRequest->fLSAPConn, errCancel);
	}
	iter->release();
    }

    // fUnmatchedGetReplys is a list of CBufferSegments that have come in, discard them
    if (fUnmatchedGetReplys && !fUnmatchedGetReplys->Empty()) {
	CListIterator *iter = CListIterator::cListIterator(fUnmatchedGetReplys);
	CBufferSegment* replyBuffer;
	
	DebugLog("IrLapConn: reset pending get replies. how?"); // shouldn't get here
	for (replyBuffer = (CBufferSegment*)iter->FirstItem();
	     iter->More(); replyBuffer = (CBufferSegment*)iter->NextItem()) {
	    XTRACE(kLogResetGetReply, 0, replyBuffer);
	    GetLAP->ReleaseInputBuffer(replyBuffer);    // give the buffer back to lap
	}
	while (!fUnmatchedGetReplys->Empty())
	    fUnmatchedGetReplys->RemoveLast();
	    
	iter->release();
    }
		
    // if we had pendingRequests, they were waiting for a disconnect, do 'em now!
    if (fPendingRequests && !fPendingRequests->Empty()) {
	CListIterator *iter = CListIterator::cListIterator(fPendingRequests);
	for (TIrEvent* request = (TIrEvent*)iter->FirstItem();
	     iter->More(); request = (TIrEvent*)iter->NextItem()) {
		XTRACE(kLogResetPendingReq, 0, request);
		check(request->fEvent == kIrListenRequestEvent || request->fEvent == kIrConnectRequestEvent);
		this->EnqueueEvent(request);
	}
	while (!fPendingRequests->Empty())
	    fPendingRequests->RemoveLast();
	    
	iter->release();
    }
    fDisconnectPending = false;         // safe to connect/listen again

    // Whew.  I *think* we've fully reset LapConn.  

} // TIrLAPConn::Reset



//--------------------------------------------------------------------------------
//      NextState
//--------------------------------------------------------------------------------
void TIrLAPConn::NextState(ULong event)
{
    XTRACE(kLogStateEvent, fState, event);
    
    switch (fState) {
	case kIrLAPConnStandby:
	    HandleStandbyStateEvent(event);
	    break;

	case kIrLAPConnConnectOrListen:
	    HandleConnectOrListenStateEvent(event);
	    break;

	case kIrLAPConnActive:
	    HandleActiveStateEvent(event);
	    break;

	default:
	    DebugLog("TIrLAPConn::NextState: bad fState");
	    break;
    }

} // TIrLAPConn::NextState


//--------------------------------------------------------------------------------
//      HandleStandbyStateEvent
//--------------------------------------------------------------------------------
void TIrLAPConn::HandleStandbyStateEvent(ULong event)
{
    XASSERT(!fConnected);
    XASSERT(fPeerDevAddr == 0);

    switch (event) {
	case kIrConnectRequestEvent:
	case kIrListenRequestEvent:
	    {
		TIrConnLstnRequest* request = (TIrConnLstnRequest*)GetCurrentEvent();
		XTRACE(kStandbyConnLstnRequestEvent, event, 0);

		XTRACE(kWantToAdd, (uintptr_t)request->fLSAPConn>>16, request->fLSAPConn);

		// Add the lsapConn to the list of pending conns to connect/listen
		// jdg: the IAS server gets an error on it's listen and immediately issues another
		// listen request.  Since it didn't do a disconnect, it's LSAP is still on our list.
		if (!fLSAPConnList->Contains(request->fLSAPConn)) {
		    fLSAPConnList->InsertLast(request->fLSAPConn);
		}

		// Keep track of the device that we will be connected to
		if (event == kIrConnectRequestEvent) {
		    fPeerDevAddr = request->fDevAddr;
		}

		// Pass the request on to IrLAP
		// ***This can be dangerous if the connect can be cancelled separately
		// from all other pending requests and then the IrLAP will depend on a
		// buffer that no longer is "allocated".  In other words, maybe a separate
		// req buffer should be allocated for this purpose.
		fState = kIrLAPConnConnectOrListen;
		GetLAP->EnqueueEvent(request);
	    }
	    break;

	case kIrDisconnectRequestEvent:
	    {
		// Pass the disconnect on to IrLAP ********* JDG: nope, this is not what we want.  lap is already disconnected
		XTRACE(kStandbyDisconnectRequestEvent, 0, 0);
		
		if (1) {        // jdg testing
		    TIrDisconnectRequest* disconnectRequest = (TIrDisconnectRequest*)GetCurrentEvent();
		    // Remove this lsap from the lsap conn list -- if it's still there
		    if (fLSAPConnList->Contains(disconnectRequest->fLSAPConn)) {    // sigh, it doesn't always ...
			XTRACE(kAddingToLSAPConnList, 0, 
				disconnectRequest->fLSAPConn->GetMyLSAPId() << 8 | 99);
			XTRACE(kAddingLsapToList, 0, disconnectRequest->fLSAPConn);        // jdg
		    IrDAErr removeResult = fLSAPConnList->Remove(disconnectRequest->fLSAPConn);
		    ncheck(removeResult);
		    }
		}
		//fIrLAP->EnqueueEvent(GetCurrentEvent());      // no, don't do it -- jdg
		if (1) {        // jdg new
		    TIrDisconnectRequest* disconnectRequest = (TIrDisconnectRequest*)GetCurrentEvent();
		    disconnectRequest->fEvent = kIrDisconnectReplyEvent;        // turn into a reply
		    disconnectRequest->fResult = 0;
		    check(disconnectRequest->fLSAPConn);            // internal disconnect?
		    disconnectRequest->fLSAPConn->EnqueueEvent(disconnectRequest);  // send it back
		}
	    }       
	    break;

	case kIrDisconnectReplyEvent:
	    {
		// Use the request block for the reply
		TIrDisconnectReply* disconnectReply = (TIrDisconnectReply*)GetCurrentEvent();
		XTRACE(kStandbyDisconnectReplyEvent, 0, 0);
		// Already disconnected - nothing to do, except return the reply.
		//check(disconnectReply->fLSAPConn);
		if (disconnectReply->fLSAPConn == nil)              // if internally generated event
		    fIrDA->ReleaseEventBlock(disconnectReply);      // then release it here
		else
		disconnectReply->fLSAPConn->EnqueueEvent(disconnectReply);
		
		// jdg: if any deferred events, requeue them now
		if (fPendingRequests && !fPendingRequests->Empty()) {
		    CListIterator *iter = CListIterator::cListIterator(fPendingRequests);
		    for (TIrEvent* request = (TIrEvent*)iter->FirstItem();
			 iter->More(); request = (TIrEvent*)iter->NextItem()) {
			    XTRACE(kStandbyDisconnectRequeue, 0, request);
			    check(request->fEvent == kIrListenRequestEvent || request->fEvent == kIrConnectRequestEvent);
			    this->EnqueueEvent(request);
		    }
		    while (!fPendingRequests->Empty())
			fPendingRequests->RemoveLast();
			
		    iter->release();
		}
		fDisconnectPending = false;         // safe to connect/listen again
	    }
	    break;

	case kIrGetDataRequestEvent:            // if we get a get/put here, just reject it
	case kIrPutDataRequestEvent:
	    {
		XTRACE(kUnexpectedEvent, fState, event);
		TIrGetRequest* rq = (TIrGetRequest*)GetCurrentEvent();
		if (rq->fEvent == kIrGetDataRequestEvent)       // turn request into reply (could ++ it)
		    rq->fEvent = kIrGetDataReplyEvent;
		else
		    rq->fEvent = kIrPutDataReplyEvent;
		rq->fResult = kIrDAErrWrongState;           // better than crashing, but why are we here?
		check(rq->fLSAPConn);
		rq->fLSAPConn->EnqueueEvent(rq);            // return to sender, bad destination
	    }
	    break;
	    
	default:
	    XTRACE(kUnexpectedEvent, fState, event);
	    DebugLog("TIrLAPConn::HandleStandbyStateEvent: bad event");
	    break;
    }

} // TIrLAPConn::HandleStandbyStateEvent


//--------------------------------------------------------------------------------
//      HandleConnectOrListenStateEvent
//--------------------------------------------------------------------------------
void TIrLAPConn::HandleConnectOrListenStateEvent(ULong event)
{
    XASSERT(!fConnected);

    switch (event) {
	case kIrConnectRequestEvent:
	case kIrListenRequestEvent:
	    {
		TIrConnLstnRequest* request = (TIrConnLstnRequest*)GetCurrentEvent();
		XTRACE(kPendingConnLstnRequestEvent, event, request->fEvent);
	    
		if (fDisconnectPending) {           // oops, hold off on this request unti the disconnect is done
		    XTRACE(kPendingConnLstnDeferRequest, 0, GetCurrentEvent());
		    fPendingRequests->InsertLast(GetCurrentEvent());
		    check(GetCurrentEvent()->fEvent == kIrListenRequestEvent || GetCurrentEvent()->fEvent == kIrConnectRequestEvent);
		    return;
		}
	    
		// ***Here is a problem that probably will have to be dealt with when the
		// IrDA "stack" is used by a mux tool (i.e. multiple endpoints).  If a listen
		// was the first request to initiate a listen to LAP and then a connect request
		// followed, you need to cancel the pending LAP listen request and replace it
		// with a connect request.

		// Shouldn't be called if connecting to a different device, reject the request if it comes in that way
		if (event == kIrConnectRequestEvent) {
		    // This first one checks for the situation described above
		    //XASSERT(fPeerDevAddr != 0);
		    //XASSERT(fPeerDevAddr == request->fDevAddr);
									// JDG: if 1st was connect & this is going to diff device
		    if (fPeerDevAddr && (fPeerDevAddr != request->fDevAddr)) {  // then reject this connect request
			request->fEvent = kIrConnectReplyEvent;
			request->fResult = kIrDAErrGeneric;             // better err available?
			check(request->fLSAPConn);
			request->fLSAPConn->EnqueueEvent(request);      // return to sender, bad destination
			return;
		    }
		    
		    // JDG hacking  ... won't it ever stop?
		    // we have a connect request, was the first request a listen?  if so ....
		    // ask LAP to cancel the pending listen and then send in our connect request.
		    // Foo.  Bletch.  Ugh.
		    if (fPeerDevAddr == 0) {                // if 1st request was a listen ...
			if (GetLAP->CancelPendingListenRequest()) {     // if the lap cancel worked
			    fPeerDevAddr = request->fDevAddr;           // save the destination address
			    GetLAP->EnqueueEvent(request);              // then send connect req to lap
			    // continue and add the lsapconn to our list
			    // note: the listen request is still on our queue and will finish with the connect
			}
			else {      // the cancel of the listen failed, reject the connect request (or?)
			    request->fEvent = kIrConnectReplyEvent;
			    request->fResult = kIrDAErrGeneric;             // better err available?
			    check(request->fLSAPConn);
			    request->fLSAPConn->EnqueueEvent(request);      // return to sender, bad destination
			    return;
			}
		    }       // end first request a listen
		}       // end if connect request
		
		// Add the lsapConn to the list of pending conns to connect/listen
		XASSERT(!fLSAPConnList->IsEmpty());
		XASSERT(!fLSAPConnList->Contains(request->fLSAPConn));
		
		XTRACE(kAddingToLSAPConnList, 1, request->fLSAPConn->GetMyLSAPId() << 8 | 1);
		XTRACE(kAddingLsapToList, 0, request->fLSAPConn);
		fLSAPConnList->InsertLast(request->fLSAPConn);

		// No state change
	    }
	    break;

	case kIrConnectReplyEvent:
	case kIrListenReplyEvent:
	    {
		TIrConnLstnReply* reply = (TIrConnLstnReply*)GetCurrentEvent();
		XTRACE(kPendingConnLstnReplyEvent, event, reply->fResult);
		{
		    CListIterator *iter = CListIterator::cListIterator(fLSAPConnList);

		    // Complete all pending connect/listen requests that are in the list
		    for (TLSAPConn* lsapConn = (TLSAPConn*)iter->FirstItem();
			 iter->More(); lsapConn = (TLSAPConn*)iter->NextItem()) {
			TIrConnLstnReply* pendingReply = (TIrConnLstnReply*)lsapConn->GetPendConnLstn();
			//check(pendingReply);      // TEMP TEMP TEMP -- should track this down
			if (pendingReply) {
			    // ***Oops, this mess is because some pending buffers are used for the
			    // ***actual request and others aren't - see other note earlier in the file.
			    if (pendingReply && pendingReply->fEvent == kIrConnectRequestEvent) {
				pendingReply->fEvent = kIrConnectReplyEvent;
			    }
			    else if (pendingReply->fEvent == kIrListenRequestEvent){
				pendingReply->fEvent = kIrListenReplyEvent;
			    }
			    else {
				// Assuming that reply has been set by IrLAP.
				// I.e. this pendingReply == GetCurrentEvent()
				XASSERT((pendingReply->fEvent == kIrConnectReplyEvent) ||
					(pendingReply->fEvent == kIrListenReplyEvent));
			    }
			    pendingReply->fResult = reply->fResult;
			    pendingReply->fDevAddr = reply->fDevAddr;
			    lsapConn->EnqueueEvent(pendingReply);
			}
		    }
		    iter->release();
		    
		} // barney block

		// If connect succeeded, move to the active state (California?)
		if (reply->fResult == noErr) {
		    fPeerDevAddr = reply->fDevAddr; // For listen's benefit
		    fState = kIrLAPConnActive;
		    fConnected = true;
		}

		// If connect failed, clean up after responding to the requestors
		// query: shouldn't the cleanup pending be done first?  and isn't it a nop here?
		else {
		    // Removing from end is much faster (and keeps indices valid as items are removed)
		    XTRACE(kAddingToLSAPConnList, 0, 2);                                    // jdg
		    for (FastInt index = fLSAPConnList->GetArraySize() - 1; index >= 0 ; index--) {
			TLSAPConn* lsapConn = (TLSAPConn*)fLSAPConnList->At(index);
			// Remove this lsap from the lsap conn list
			XTRACE(kAddingToLSAPConnList, 0,  lsapConn->GetMyLSAPId() << 8 | 2);    // jdg
			XTRACE(kAddingLsapToList, 0, lsapConn);        // jdg
			fLSAPConnList->Remove(lsapConn);
			// Complete pending get requests and delete any received buffers intended for this conn
			CleanupPendingGetRequestsAndReplies(lsapConn, errCancel);
		    }
		    fPeerDevAddr = 0;
		    fState = kIrLAPConnStandby;
		}
	    }
	    break;

	case kIrDisconnectRequestEvent:
	    {
		TIrDisconnectRequest* disconnectRequest = (TIrDisconnectRequest*)GetCurrentEvent();
		XTRACE(kPendingDisconnectRequestEvent, 0, disconnectRequest->fLSAPConn);
    /*** this didn't work.  Just do a lap disconnect, and we'll return all the pending listen/connect
	 requests.  Anyone that want's to keep alive can re-issue.
      ... let's try again
    ***/
		// if the requesting lsapconn owns the event we sent to lap, do a lap disconnect
		// else we're free to just take it off our tables w/out bothering lap
		check(disconnectRequest->fLSAPConn);
		if (GetLAP->GetCurrentRequest() == disconnectRequest->fLSAPConn->GetPendConnLstn()) {
		    fDisconnectPending = true;                  // hold off on any new listen/connect requests
		    GetLAP->EnqueueEvent(disconnectRequest);    // tell lap to blow off the current listen/conn req
		}
		else {              // lap doesn't have the event from this lsapconn
	 
		    XTRACE(kAddingToLSAPConnList, 0, disconnectRequest->fLSAPConn->GetMyLSAPId() << 8 | 3);
		    XTRACE(kAddingLsapToList, 0, disconnectRequest->fLSAPConn);        // jdg
		    // Remove this lsap from the lsap conn list and
		    // complete pending get requests and delete any received buffers intended for this conn
		    (void) fLSAPConnList->Remove(disconnectRequest->fLSAPConn);
		    CleanupPendingGetRequestsAndReplies(disconnectRequest->fLSAPConn, errCancel);
		
		    XTRACE(kPendingDisconnectRequestEvent, 2,  (fLSAPConnList->IsEmpty()) << 1 );
			
			
		    // now finish off their original listen/connect request
		    if (disconnectRequest->fLSAPConn->GetPendConnLstn()) {      // if the event is there (should be)
			TIrConnLstnReply* pendingReply = (TIrConnLstnReply*)disconnectRequest->fLSAPConn->GetPendConnLstn();
			if (pendingReply->fEvent == kIrConnectRequestEvent) {
			    pendingReply->fEvent = kIrConnectReplyEvent;
			}
			else if (pendingReply->fEvent == kIrListenRequestEvent) {
				pendingReply->fEvent = kIrListenReplyEvent;
			}
			//else check(pendingReply->fEvent == 0x1234);     // force debugger
			pendingReply->fResult = errCancel;
			pendingReply->fLSAPConn->EnqueueEvent(pendingReply);
		    }
		    
		    // Use the request buffer for the reply
		    disconnectRequest->fEvent = kIrDisconnectReplyEvent;
		    disconnectRequest->fResult = errCancel;
		    disconnectRequest->fLSAPConn->EnqueueEvent(disconnectRequest);
		}
	    }
	    break;

	case kIrDisconnectReplyEvent:
	    {
		TIrDisconnectReply* disconnectReply = (TIrDisconnectReply*)GetCurrentEvent();
		XTRACE(kPendingDisconnectReplyEvent, 0, 0);
		fPeerDevAddr = 0;
		fConnected = false;
		fState = kIrLAPConnStandby;
		//check(disconnectReply->fLSAPConn);            // internal disconnect?
		if (disconnectReply->fLSAPConn == nil)          // if lapconn generated disconnect
		    fIrDA->ReleaseEventBlock(disconnectReply);      // then release it here
		else
		disconnectReply->fLSAPConn->EnqueueEvent(disconnectReply);
		
		// jdg: if any deferred events, requeue them now
		if (fPendingRequests && !fPendingRequests->Empty()) {
		    CListIterator *iter = CListIterator::cListIterator(fPendingRequests);
		    for (TIrEvent* request = (TIrEvent*)iter->FirstItem();
			 iter->More(); request = (TIrEvent*)iter->NextItem()) {
			    XTRACE(kPendingDisconnectRequeue, 0, request);
			    check(request->fEvent == kIrListenRequestEvent || request->fEvent == kIrConnectRequestEvent);
			    this->EnqueueEvent(request);
		    }
		    while (!fPendingRequests->Empty())
			fPendingRequests->RemoveLast();
			
		    iter->release();
		}
		fDisconnectPending = false;         // safe to connect/listen again
	    }
	    break;

	default:
	    XTRACE(kUnexpectedEvent, fState, event);
	    DebugLog("TIrLAPConn::HandleConnectOrListenStateEvent: bad event");
	    break;
    }

} // TIrLAPConn::HandleConnectOrListenStateEvent


//--------------------------------------------------------------------------------
//      HandleActiveStateEvent
//--------------------------------------------------------------------------------
void TIrLAPConn::HandleActiveStateEvent(ULong event)
{
    XASSERT(fConnected);
    XASSERT(fPeerDevAddr != 0);

    StopIdleDisconnectTimer();          // always (?) stop the idle timer, we're doing something!
    switch (event) {
	case kIrConnectRequestEvent:
	case kIrListenRequestEvent:
	    {
		TIrConnLstnRequest* request = (TIrConnLstnRequest*)GetCurrentEvent();
		XTRACE(kActiveConnLstnRequestEvent, event, request->fEvent);
		
		if (fDisconnectPending) {           // oops, hold off on this request unti the disconnect is done
		    XTRACE(kActiveConnLstnDeferRequest, 0, GetCurrentEvent());
		    fPendingRequests->InsertLast(GetCurrentEvent());
		    check(GetCurrentEvent()->fEvent == kIrListenRequestEvent || GetCurrentEvent()->fEvent == kIrConnectRequestEvent);
		    return;
		}

		// Shouldn't be called if connecting to a different device
		// jdg: this should reject the request, not punt.
		if (event == kIrConnectRequestEvent) {
		    //XASSERT(fPeerDevAddr == request->fDevAddr);
		    if (fPeerDevAddr != request->fDevAddr) {
			// FIXME -- should add logging here of both addresses to see why the confusion ...
			request->fEvent = kIrConnectReplyEvent;
			request->fResult = kIrDAErrGeneric;             // better err available?
			check(request->fLSAPConn);
			request->fLSAPConn->EnqueueEvent(request);      // return to sender, bad destination
			break;                                          // done with it now
		    }
		}

		// Add the lsapConn to the list of conns associated w/fPeerDevAddr
		XASSERT(!fLSAPConnList->Contains(request->fLSAPConn));
		XTRACE(kAddingToLSAPConnList, 1, request->fLSAPConn->GetMyLSAPId() << 8 | 4);                                                       // jdg
		XTRACE(kAddingLsapToList, 0, request->fLSAPConn);        // jdg
		fLSAPConnList->InsertLast(request->fLSAPConn);

		// Already connected, reply to the requestor
		request->fEvent = event == kIrConnectRequestEvent ? kIrConnectReplyEvent : kIrListenReplyEvent;
		request->fDevAddr = fPeerDevAddr;
		check(request->fLSAPConn);
		request->fLSAPConn->EnqueueEvent(request);

		// No state change
	    }
	    break;

	case kIrGetDataRequestEvent:
	    XTRACE(kActiveGetDataRequestEvent, 0, 0);
	    HandleGetDataRequest();
	    break;

	case kIrCancelGetRequestEvent:
	    {
		TIrCancelGetRequest* cancelGetRequest = (TIrCancelGetRequest*)GetCurrentEvent();
		XTRACE(kActiveCancelGetRequestEvent, 0, 0);
		CancelPendingGetRequests(cancelGetRequest->fLSAPConn, kIrDAErrRequestCanceled);
		// Use the request buffer for the reply
		cancelGetRequest->fEvent = kIrCancelGetReplyEvent;
		cancelGetRequest->fResult = noErr;
		check(cancelGetRequest->fLSAPConn);
		cancelGetRequest->fLSAPConn->EnqueueEvent(cancelGetRequest);
	    }
	    break;

	case kIrDisconnectRequestEvent:
	    {
		TIrDisconnectRequest* disconnectRequest = (TIrDisconnectRequest*)GetCurrentEvent();
		XTRACE(kActiveDisconnectRequestEvent, 0, 0);
		
		XTRACE(kAddingToLSAPConnList, 0, disconnectRequest->fLSAPConn->GetMyLSAPId() << 8 | 5);                                                     // jdg
		XTRACE(kAddingLsapToList, 0, disconnectRequest->fLSAPConn);        // jdg
		
		// Remove this lsap from the lsap conn list
		IrDAErr removeResult = fLSAPConnList->Remove(disconnectRequest->fLSAPConn);
		// Complete pending get requests and delete any received buffers intended for this conn
		CleanupPendingGetRequestsAndReplies(disconnectRequest->fLSAPConn, kIrDAErrGeneric);
		
		// Note: if I do a disconnect after getting a read complete, this gets called twice, once
		// for the disconnect request, and (I think) once to clean up the pending get request.
		///////////////////////////////////////////////////////////////////////////
		// start of ugly hack to kill off name server if it's all that's left
		//  this gets weird cause it's "the" pending event for LAP.  Grrr.
		if (removeResult == noErr && fLSAPConnList->GetArraySize() == 1) {  // if only one thing left on the list
		    TLSAPConn* lsapConn = (TLSAPConn*)fLSAPConnList->At(0);         // grab it
		    if (lsapConn->GetMyLSAPId() == kNameServerLSAPId) {             // sigh, if the name server
			TIrConnLstnReply* pendingReply = (TIrConnLstnReply*)lsapConn->GetPendConnLstn();    // get it's event
			if (pendingReply && 
			    (pendingReply->fEvent == kIrListenRequestEvent ||           // it was a listen request, but now that
			     pendingReply->fEvent == kIrGetDataRequestEvent)) {         // we're active, it's a get request
	    // NEW: let's start a timer here and do the lap disconnect after N seconds of 
	    // nothing going on ...
			    StartIdleDisconnectTimer();             // start the idle disconnect timer
		//
		// Nuke the name server's listen so we can do a LAP disconnect
		//
		//          check(pendingReply->fEvent == kIrGetDataRequestEvent);          // just making sure
		//          removeResult = fLSAPConnList->Remove(lsapConn);                 // get name server off our list
		//          CancelPendingGetRequests(lsapConn, errCancel);              // this will q up the get response
			}
		    }
		} 
		////////
		//if ((removeResult == noErr) && fLSAPConnList->IsEmpty() && fIrDA->Disconnecting())
		if (disconnectRequest->fEvent == kIrDisconnectRequestEvent) {       // if "the" event is still available
    ////////*** with the idle disconnect timer, we never request a lap disconnect here anymore
		    if (((removeResult == noErr) && fLSAPConnList->IsEmpty())) {
		    // If no more connections [and shutting down] - disconnect IrLAP
			fDisconnectPending = true;                  // hold off on any new listen/connect requests
		    GetLAP->EnqueueEvent(disconnectRequest);
		}
		else {
		    // Use the request buffer for the reply
		    disconnectRequest->fEvent = kIrDisconnectReplyEvent;
		    disconnectRequest->fResult = errCancel;
		    check(disconnectRequest->fLSAPConn);
		    disconnectRequest->fLSAPConn->EnqueueEvent(disconnectRequest);
		}
		}
		// else the event was a get and turned into a get reply already by CleanupPendingGetRequestsAndReplies 
	    }
	    break;

	case kIrDisconnectReplyEvent:
	    {
		TIrDisconnectReply* disconnectReply = (TIrDisconnectReply*)GetCurrentEvent();
		XTRACE(kActiveDisconnectReplyEvent, 0, 0);
		fPeerDevAddr = 0;
		fConnected = false;
		fState = kIrLAPConnStandby;
		
		// new - if this is a disconnect generated by our idle timer, then fLSAPConn will be nil
		if (disconnectReply->fLSAPConn == nil) {            // if we allocated the event
		    fIrDA->ReleaseEventBlock(disconnectReply);      // then release it here
		    //DebugLog(" disconnect due to idle");
		}
		else {      // else send the disconnect complete back to the lsap
		    disconnectReply->fLSAPConn->EnqueueEvent(disconnectReply);
		}
		
		// jdg: if any deferred events, requeue them now
		if (fPendingRequests && !fPendingRequests->Empty()) {
		    CListIterator *iter = CListIterator::cListIterator(fPendingRequests);
		    for (TIrEvent* request = (TIrEvent*)iter->FirstItem();
			 iter->More(); request = (TIrEvent*)iter->NextItem()) {
			    XTRACE(kActiveDisconnectRequeue, 0, request);
			    check(request->fEvent == kIrListenRequestEvent || request->fEvent == kIrConnectRequestEvent);
			    this->EnqueueEvent(request);
		    }
		    while (!fPendingRequests->Empty())
			fPendingRequests->RemoveLast();
		    
		    iter->release();
		}
		fDisconnectPending = false;         // safe to connect/listen again
	    }
	    break;

	case kIdleDisconnectEvent:                  // idle disconnect timer fired
	    {
		if (fLSAPConnList->GetArraySize() == 1) {               // if only one thing on the list
		    TLSAPConn* lsapConn = (TLSAPConn*)fLSAPConnList->At(0);         // grab ias server entry
		    if (lsapConn &&
			lsapConn->GetMyLSAPId() == kNameServerLSAPId) {             // ifthe name server
			TIrConnLstnReply* pendingReply = (TIrConnLstnReply*)lsapConn->GetPendConnLstn();    // get it's event
			if (pendingReply &&                                         // a listen is really a get
			    (pendingReply->fEvent == kIrGetDataRequestEvent)) {     // we're active, it's a get request
			    IrDAErr removeResult;
			    removeResult = fLSAPConnList->Remove(lsapConn);         // get name server off our list
			    CancelPendingGetRequests(lsapConn, errCancel);      // this will q up the get response

			    TIrDisconnectRequest* disconnectRequest = (TIrDisconnectRequest*)
						    fIrDA->GrabEventBlock(kIrDisconnectRequestEvent,
								    sizeof(TIrDisconnectRequest));
			    check(disconnectRequest);       // pretty sad if no events and idle!
			    if (disconnectRequest) {
				disconnectRequest->fLSAPConn = nil;     // lapconn generated event (not LSAPConn)
				fDisconnectPending = true;              // hold off on any new listen/connect requests
				GetLAP->EnqueueEvent(disconnectRequest);
			    }
			}
		    }
		}
	    }
	    break;

	default:
	    XTRACE(kUnexpectedEvent, fState, event);
	    DebugLog("TIrLAPConn::HandleActiveStateEvent: bad event");
	    break;
    }

} // TIrLAPConn::HandleActiveStateEvent


//================================ Helper methods ================================


//--------------------------------------------------------------------------------
//      HandleGetDataRequest
//--------------------------------------------------------------------------------
void TIrLAPConn::HandleGetDataRequest()
{
    Boolean matchFound = false;
    TIrGetRequest* getRequest = (TIrGetRequest*)GetCurrentEvent();

    // Data may have already arrived and is in the unmatched get replys list
    // If it is, then the get data request can complete right now
    CListIterator *iter = CListIterator::cListIterator(fUnmatchedGetReplys);
    for (CBufferSegment* replyBuffer = (CBufferSegment*)iter->FirstItem();
	 iter->More(); replyBuffer = (CBufferSegment*)iter->NextItem()) {
	TLMPDUHeader header;
	ULong headerLength;
	Boolean validFormat;

	validFormat = ExtractHeader(replyBuffer, header, headerLength);
	XASSERT(validFormat);

	if (DataDelivered(getRequest, header, headerLength, replyBuffer)) {
	    fUnmatchedGetReplys->Remove(replyBuffer);
	    matchFound = true;
	    break;
	}
    }
    iter->release();

    // Data is not in one of the buffers, add the request to the pending get requests.
    if (!matchFound) {
	XTRACE(kLogAddingGetRequest1, 0, getRequest);
	XTRACE(kLogAddingGetRequest2, 0, getRequest->fLSAPConn);
	fPendingGetRequests->InsertLast(getRequest);
	XTRACE(kLogAddingGetRequest3, fPendingGetRequests->Count(), getRequest->fLSAPConn->GetMyLSAPId());
    }

} // TIrLAPConn::HandleGetDataRequest


//--------------------------------------------------------------------------------
//      CleanupPendingGetRequestsAndReplies
//--------------------------------------------------------------------------------
void TIrLAPConn::CleanupPendingGetRequestsAndReplies(TLSAPConn* lsapConn, IrDAErr returnCode)
{
    XTRACE(kLogCleanupPendingGetRequestsAndRepliesEntry, 0, lsapConn);
    
    // Complete any pending get requests with an error.  Default is kIRErrGeneric
    CancelPendingGetRequests( lsapConn, returnCode );   // ***FIXME: Better error return?

    XTRACE(kLogCleanupPendingGetRequestsAndReplies2, 0, fUnmatchedGetReplys);

    // Free any pending received buffers for this lsap connection
    if (fUnmatchedGetReplys) {
	CListIterator *iter = CListIterator::cListIterator(fUnmatchedGetReplys);
	for (CBufferSegment* replyBuffer = (CBufferSegment*)iter->FirstItem();
	     iter->More(); replyBuffer = (CBufferSegment*)iter->NextItem()) {
	    TLMPDUHeader header;
	    ULong headerLength;

	    //Boolean validFormat = ExtractHeader(replyBuffer, header, headerLength);
	    Boolean validFormat;
	    
	    XTRACE(kLogCleanupPendingGetRequestsAndReplies3, 0, replyBuffer);
	    
	    validFormat = ExtractHeader(replyBuffer, header, headerLength);
	    XASSERT(validFormat);

	    // If this buffer is/was for the lsapConn being removed
	    if (lsapConn->YourData(header, true /*justChecking*/)) {
		XTRACE( kCleanupPendingRcvdBufEvent, 0, returnCode );
		fUnmatchedGetReplys->Remove(replyBuffer);
		// Release the buffer.
		GetLAP->ReleaseInputBuffer(replyBuffer);    // give the buffer back to lap
	    }
	}
	iter->release();
    }

} // TIrLAPConn::CleanupPendingGetRequestsAndReplies


//--------------------------------------------------------------------------------
//      CancelPendingGetRequests
//--------------------------------------------------------------------------------
void TIrLAPConn::CancelPendingGetRequests(TLSAPConn* lsapConn, IrDAErr returnCode)
{
    // Complete any pending get requests with an error
    XTRACE(kLogCancelPendingGetRequestsEntry, 0, lsapConn);
    
    check(lsapConn);
    if (fPendingGetRequests && !fPendingGetRequests->Empty()) {
	CListIterator *iter = CListIterator::cListIterator(fPendingGetRequests);
	for (TIrGetRequest* getRequest = (TIrGetRequest*)iter->FirstItem();
	     iter->More(); getRequest = (TIrGetRequest*)iter->NextItem()) {
	    if (getRequest->fLSAPConn == lsapConn) {
		XTRACE(kLogCancelPendingGetRequests, 0, lsapConn);
		fPendingGetRequests->Remove(getRequest);    // get this req off the list
		// Send the reply
		getRequest->fEvent = kIrGetDataReplyEvent;
		getRequest->fResult = returnCode;
		lsapConn->EnqueueEvent(getRequest);
		// Theoretically there should only be one outstanding get request for
		// this lsapConn, so you should be able to break out of the loop here.
		// But, who ever believed in theory besides Albert Einstein?
		// Could add some debug only logic and an assert to test this...
		//
		// jdg: I don't trust remove in the middle of an iterate, so let's
		// recurse and return instead of finishing the iteration.  Note this
		// only recurses when we've found and processed an event for this 
		// lsapconn, so we're not going to recurse forever.
		iter->release();            // we're done with this iteration engine
		CancelPendingGetRequests(lsapConn, returnCode);
		return;
	    }
	}
	iter->release();
    }
    XTRACE(kLogCancelPendingGetRequestsExit, 0, lsapConn);

} // TIrLAPConn::CancelPendingGetRequests


//--------------------------------------------------------------------------------
//      Demultiplexor
//--------------------------------------------------------------------------------
void TIrLAPConn::Demultiplexor(CBufferSegment* inputBuffer)
{
    TLMPDUHeader header;
    ULong headerLength;
    Boolean     validFormat;
    Boolean     matchFound = false;
	
    XTRACE(kLogDemux, 0, inputBuffer);
    
    validFormat = ExtractHeader(inputBuffer, header, headerLength);

    if (!validFormat || ((header.fOpCode & ~kLMPDUReplyFlag) == kLMPDUAccessModeRequest)) {
	XTRACE(kDemuxInvalidHeaderEvent, 0, 0);
	// We're responding.  Don't keep looking.
	matchFound = true;
	// Release the buffer
	GetLAP->ReleaseInputBuffer(inputBuffer);
	// Send a response for access requests (ignore the erroneous access confirms)
	if (validFormat && (header.fOpCode == kLMPDUAccessModeRequest) && (header.fMode <= kIrLMPExclusiveMode)) {
	    ReplyToInvalidFrame(header, kLMPDUAccessModeReply, kIrLMPDUControlUnsupported);
	}
    }

    // Some received data has arrived.  If a match can be found in the pending get requests
    // then that request can be completed and the buffer can be freed up for reuse.
    if (!matchFound) {
	XTRACE(kLogDemuxCheckingGets1, 0, fPendingGetRequests->Count());
	CListIterator *iter = CListIterator::cListIterator(fPendingGetRequests);
	for (TIrGetRequest* getRequest = (TIrGetRequest*)iter->FirstItem();
	     iter->More(); getRequest = (TIrGetRequest*)iter->NextItem()) {
	    XTRACE(kLogDemuxCheckingGets2, 0, getRequest);

	    if (DataDelivered(getRequest, header, headerLength, inputBuffer)) {
		fPendingGetRequests->Remove(getRequest);
		XTRACE(kDemuxGetPendingEvent, 0, getRequest);
		matchFound = true;
		break;
	    }
	}
	iter->release();
    }
	
    // No one is waiting for the data.  Is it a potential reply? - cache it until its requested.
    if (!matchFound) {
	CListIterator *iter = CListIterator::cListIterator(fLSAPConnList);
	for (TLSAPConn* lsapConn = (TLSAPConn*)iter->FirstItem();
	     iter->More(); lsapConn = (TLSAPConn*)iter->NextItem()) {
	    if (lsapConn->YourData(header, true /*justChecking*/)) {
		fUnmatchedGetReplys->InsertLast(inputBuffer);
		XTRACE(kDemuxReplyPostedEvent, 0, inputBuffer);
		XTRACE(kDemuxReplyPostedEvent2, 0, lsapConn);
		matchFound = true;
		break;
	    }
	}
	iter->release();
    }

    // No one connected that this could belong to.
    if (!matchFound) {
	XTRACE(kDemuxNoReceiverEvent, 0, 0);
	// Release the buffer
	GetLAP->ReleaseInputBuffer(inputBuffer);
	// Send a disconnect response
	UByte respCode;
	if (header.fOpCode == kLMPDUDataEvent) {
	    respCode = kIrDataSentOnDiscLSAPConn;
	}
	else if (header.fOpCode == kLMPDUConnectRequest) {
	    respCode = kIrNoAvailableLMMuxClient;
	}
	else {
	    respCode = kIrUserRequestedDisconnect;
	}
	ReplyToInvalidFrame(header, kLMPDUDisconnectEvent, respCode);
    }

} // TIrLAPConn::Demultiplexor


//--------------------------------------------------------------------------------
//      ReplyToInvalidFrame
//--------------------------------------------------------------------------------
void TIrLAPConn::ReplyToInvalidFrame(TLMPDUHeader& header, UByte replyOpCode, UByte replyInfo)
{
    TIrPutRequest* putRequest;

    // jdg: note that the check for nil fLSAPConn for this case is
    // in the PutComplete routine of IrLAP
    
    putRequest = (TIrPutRequest*)fIrDA->GrabEventBlock(kIrPutDataRequestEvent, sizeof(TIrPutRequest));
    // Ignore this if no memory to get request block
    if (putRequest != nil) {
	putRequest->fLSAPConn = nil;    // Don't respond to putRequest, just free the block
	putRequest->fData = nil;
	putRequest->fOffset = 0;
	putRequest->fLength = 0;
	putRequest->fDstLSAPId = header.fSrcLSAPId | kLMPDUControlFlag;
	putRequest->fSrcLSAPId = header.fDstLSAPId & ~kLMPDUControlFlag;
	putRequest->fCtrlOpCode = replyOpCode;
	putRequest->fCtrlInfo = replyInfo;
	GetLAP->EnqueueEvent(putRequest);
    }

} // TIrLAPConn::ReplyToInvalidFrame


//--------------------------------------------------------------------------------
//      ExtractHeader
//--------------------------------------------------------------------------------
Boolean TIrLAPConn::ExtractHeader(CBufferSegment* inputBuffer, TLMPDUHeader& header, ULong& length)
{
    ULong headerLength;

    // Need to reseek to 0 as this may be called multiple times
    inputBuffer->Seek(0, kPosBeg);

    // Get the header info
    headerLength = inputBuffer->Getn(&header.fDstLSAPId, sizeof(TLMPDUHeader));
    XASSERT(headerLength >= 2);
    if (headerLength < 2) {
	// LM-PDU header requires dst/src minimum
	return false;
    }
    else if (header.fDstLSAPId & kLMPDUControlFlag) {
	header.fDstLSAPId &= ~kLMPDUControlFlag;
	if (headerLength == 2) {
	    // Control header requires opcode minimum
	    return false;
	} else if (headerLength == 3) {
	    // Set unspecified info field to 0
	    header.fInfo = 0;
	}
	switch (header.fOpCode) {
	    case kLMPDUConnectRequest:
	    case kLMPDUConnectReply:
	    case kLMPDUDisconnectEvent:
		headerLength = Min(headerLength, 4);
		break;

	    case kLMPDUAccessModeRequest:
	    case kLMPDUAccessModeReply:
		break;

	    default:
		return false;
	}
    }
    else {
	headerLength = 2;
	header.fOpCode = kLMPDUDataEvent;
	header.fInfo = 0;
    }

    // Invalid lsap ids
    if ((header.fDstLSAPId > kLastValidLSAPId) || (header.fSrcLSAPId > kLastValidLSAPId)) {
	return false;
    }

    length = headerLength;
    return true;

} // TIrLAPConn::ExtractHeader


//--------------------------------------------------------------------------------
//      DataDelivered
//--------------------------------------------------------------------------------
Boolean TIrLAPConn::DataDelivered(TIrGetRequest* getRequest, TLMPDUHeader& header, ULong headerLength, CBufferSegment* dataBuffer)
{
    if (getRequest->fLSAPConn->YourData(header, false /*justChecking*/)) {
	ULong written = 0;

	UInt32 dataLength = dataBuffer->GetBufferSize() - headerLength;
	if (getRequest->fData && (dataLength > 0)) {
	    //if (getRequest->fLength < dataLength) {       // jdg
	    //  DebugPrintf("About to die, fLength=%d, datalength %d",
	    //      getRequest->fLength, dataLength);
	    //}
	    XASSERT(getRequest->fLength >= dataLength);
	    getRequest->fData->Seek(getRequest->fOffset, kPosBeg);
	    written = getRequest->fData->Putn(dataBuffer->GetBufferPtr() + headerLength, dataLength);
	    XASSERT(written == dataLength);
	}

	// Fill in the fields for the reply
	getRequest->fEvent = kIrGetDataReplyEvent;
	getRequest->fResult = noErr;
	getRequest->fLength = written;
	getRequest->fCtrlOpCode = header.fOpCode;
	getRequest->fCtrlInfo = header.fInfo;

	// Send the reply
	check(getRequest->fLSAPConn);
	getRequest->fLSAPConn->EnqueueEvent(getRequest);

	// Release the buffer
	XTRACE(kDemuxReleaseBufferEvent, 0, dataBuffer);
	GetLAP->ReleaseInputBuffer(dataBuffer);

	return true;
    }

    return false;

} // TIrLAPConn::DataDelivered


//--------------------------------------------------------------------------------
//      FillInLMPDUHeader
//--------------------------------------------------------------------------------
ULong TIrLAPConn::FillInLMPDUHeader(TIrPutRequest* putRequest, UByte* buffer)
{
    ULong infoLength;
    TLMPDUHeader* lmPDUHeader = (TLMPDUHeader*)buffer;
    
    XTRACE(kLogFillInLMPDUHeader1, 0, putRequest);
    XTRACE(kLogFillInLMPDUHeader2, 0, buffer);

    // Fill out the info
    lmPDUHeader->fDstLSAPId = putRequest->fDstLSAPId;
    lmPDUHeader->fSrcLSAPId = putRequest->fSrcLSAPId;

    if (putRequest->fCtrlOpCode != kLMPDUDataEvent) {
	lmPDUHeader->fDstLSAPId |= kLMPDUControlFlag;
	lmPDUHeader->fOpCode = putRequest->fCtrlOpCode;
	lmPDUHeader->fInfo = putRequest->fCtrlInfo;
	if (putRequest->fCtrlOpCode != kLMPDUAccessModeReply) {
	    infoLength = 4;
	}
	else {
	    // Access mode follows "header" for access mode reply frame
	    buffer[4] = kIrLMPMultiplexedMode;
	    infoLength = 5;
	}
    }
    else {
	infoLength = 2;
    }

    return infoLength;

} // TIrLAPConn::FillInLMPDUHeader

//--------------------------------------------------------------------------------
//      StartIdleDisconnectTimer
//--------------------------------------------------------------------------------
void TIrLAPConn::StartIdleDisconnectTimer()
{
    XTRACE(kLogStartIdleDisconnectTimer, 0, 0);
    // could just overload timer2, but this keeps sanity a little longer
    fIrDA->StartTimer(kTimer_LAPConn, 1 * kSeconds, kIdleDisconnectEvent);

} // TIrLAPConn::StartIdleDisconnectTimer


//--------------------------------------------------------------------------------
//      StopIdleDisconnectTimer
//--------------------------------------------------------------------------------
void TIrLAPConn::StopIdleDisconnectTimer()
{
    XTRACE(kLogStopIdleDisconnectTimer, 0, 0);
    if (fIrDA)                  // if init'd
	fIrDA->StopTimer(kTimer_LAPConn);
} // TIrLAPConn::StopIdleDisconnectTimer

//
// Do idle disconnect ... now, don't wait for the timer
// 
void TIrLAPConn::DoIdleDisconnect()
{
    XTRACE(kLogDoIdleDisconnect, 0, 0);
    
    StopIdleDisconnectTimer();              // first stop the real timer
    if (fState == kIrLAPConnActive)         // and if there is an active connect
	NextState(kIdleDisconnectEvent);    // do the idle disconnect logic
}

//--------------------------------------------------------------------------------
//      TimerComplete
//--------------------------------------------------------------------------------
void TIrLAPConn::TimerComplete(ULong refCon)
{
//#pragma   unused(refCon)
    XASSERT(refCon == kIrConnWatchdogExpiredEvent || refCon == kIdleDisconnectEvent);

    if (refCon == kIrConnWatchdogExpiredEvent) {    // one second timer has fired
	CListIterator *iter = CListIterator::cListIterator(fLSAPConnList);
	XTRACE(kLogConnWatchDogFired, 0, 0);
	
	// Let all of the active lsap conn's know that the timer has fired
	for (TLSAPConn* lsapConn = (TLSAPConn*)iter->FirstItem(); iter->More(); lsapConn = (TLSAPConn*)iter->NextItem()) {
	    lsapConn->OneSecTickerComplete();
	}
	iter->release();
    }
    else {
	XTRACE(kLogIdleDisconnectFired, 0, 0);
	if (refCon == kIdleDisconnectEvent)         // the idle disconnect timer has fired
	    if (fState == kIrLAPConnActive)         // just another sanity check
		NextState(refCon);
    }

} // TIrLAPConn::TimerComplete

