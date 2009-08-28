/*
    File:       IrLMP.cpp

    Contains:   Implementation of the TIrLMP class

*/

#include "IrGlue.h"
#include "IrLMP.h"
#include "IrLAP.h"
#include "IrLAPConn.h"
#include "CList.h"
#include "CListIterator.h"
#include "IrDiscovery.h"
#include "IrDscInfo.h"

#if (hasTracing > 0 && hasLMPTracing > 0)

enum IrLMPTraceCodes
{
    kCreate = 1,
    kInit,
    kLogFree,
    kUnexpectedEvent,

    kStnCntlReadyDiscoverEvent,
    kStnCntlReadyConnectEvent,
    kStnCntlReadyListenEvent,
    kStnCntlReadyDiscRequestEvent,
    kStnCntlReadyDiscReplyEvent,

    kStnCntlDiscoverDiscoverEvent,
    kStnCntlResolveDiscoverEvent,
    
    kLogLMPResetEntry,
    kLogLMPResetExit,
    kLogFillInLMPDUHeader,
    kLogStartOneSecTicker,
    
    kEnqueueEvent,
    kDequeueEventStart,
    kDequeueEventEnd
};

EventTraceCauseDesc IrLMPTraceEvents[] = {
    {kCreate,                       "irlmp: create obj="},
    {kInit,                         "irlmp: init, lmp="},
    {kLogFree,                      "irlmp: free"},
    {kUnexpectedEvent,              "irlmp: unexpected event"},

    {kStnCntlReadyDiscoverEvent,    "irlmp: Ready discover request"},
    {kStnCntlReadyConnectEvent,     "irlmp: Ready connect request"},
    {kStnCntlReadyListenEvent,      "irlmp: Ready listen request"},
    {kStnCntlReadyDiscRequestEvent, "irlmp: Ready disconnect request"},
    {kStnCntlReadyDiscReplyEvent,   "irlmp: Ready disconnect reply"},

    {kStnCntlDiscoverDiscoverEvent, "irlmp: Discv discover reply"},
    {kStnCntlResolveDiscoverEvent,  "irlmp: Reslv discover reply"},
    
    {kLogLMPResetEntry,             "irlmp: reset entry"},
    {kLogLMPResetExit,              "irlmp: reset finished"},
    {kLogFillInLMPDUHeader,         "irlmp: Fill In LMP PDU Header"},
    {kLogStartOneSecTicker,         "irlmp: StartOneSecTicker"},
    
    {kEnqueueEvent,                 "irlmp: Event Queued"},
    {kDequeueEventStart,            "irlmp: Event Start"},
    {kDequeueEventEnd,              "irlmp: Event End"}
};

#define XTRACE(x, y, z) IrDALogAdd( x, y, (uintptr_t)z & 0xffff, IrLMPTraceEvents, true )

#else
#define XTRACE(x, y, z) ((void)0)
#endif

#define GetLAP          (fIrDA->GetLAP())
#define GetLAPConn      (fIrDA->GetLAPConn())
#define GetDiscovery    (fIrDA->GetDiscovery())

//--------------------------------------------------------------------------------
#define super TIrStream
    OSDefineMetaClassAndStructors(TIrLMP, TIrStream);

//--------------------------------------------------------------------------------
//      TIrLMP
//--------------------------------------------------------------------------------
/*static*/
TIrLMP * TIrLMP::tIrLMP(TIrGlue* irda)
{
    TIrLMP *obj = new TIrLMP;
    
    XTRACE(kCreate, 0, obj);

    if (obj && !obj->Init(irda)) {
	obj->release();
	obj = nil;
    }
    return obj;
}

void TIrLMP::free(void)
{
    XTRACE(kLogFree, 0, this);
    
    if (fPendingRequests) {     // cleanup pending event list
	fPendingRequests->release();
	fPendingRequests = nil;
    }
    
    super::free();
}


//--------------------------------------------------------------------------------
//      Init
//--------------------------------------------------------------------------------
Boolean TIrLMP::Init(TIrGlue* irda)
{
    XTRACE(kInit, 0, this);
    
    
    fState = kIrLMPReady;
    fTimerClients = 0;

    fNumAddrConflicts = 0;
    bzero(fAddrConflicts, sizeof(fAddrConflicts));

    fPendingRequests = nil;
		

#if (hasTracing > 0 && hasLMPTracing > 0)
    if (!super::Init(irda, IrLMPTraceEvents, kEnqueueEvent)) return false;
#else
    if (!super::Init(irda)) return false;
#endif
	
    
    fPendingRequests = CList::cList();      // make pending requests list
    require(fPendingRequests, Fail);

    return true;

Fail:

    return false;
    
} // TIrLMP::Init


//--------------------------------------------------------------------------------
//      Reset
//--------------------------------------------------------------------------------
void TIrLMP::Reset()
{
    XTRACE(kLogLMPResetEntry, 0, 0);
    // This is only intended for orderly reset (see IrGlue.c DisconnectComplete).

    fState = kIrLMPReady;
    fTimerClients = 0;

    if (GetLAPConn != nil) {
	GetLAPConn->Reset();
    }
    XTRACE(kLogLMPResetExit, 0, 0);

} // TIrLMP::Reset



//--------------------------------------------------------------------------------
//      NextState
//
//      Station Control state transitions (IrLMP 3.5.2.3.1)
//      Only supports Ready, Discover, and Resolve Address states
//--------------------------------------------------------------------------------
void TIrLMP::NextState(ULong event)
{
    switch (fState) {
	case kIrLMPReady:
	    HandleReadyStateEvent(event);
	    break;

	case kIrLMPDiscover:
	    HandleDiscoverStateEvent(event);
	    break;

	case kIrLMPResolveAddress:
	    HandleResolveAddressStateEvent(event);
	    break;

	default:
	    DebugLog("TIrLMP::NextState: bad fState");
	    break;
    }

} // TIrLMP::NextState


//--------------------------------------------------------------------------------
//      HandleReadyStateEvent
//
//      Station Control Ready state transitions (IrLMP 3.5.2.3.2)
//      Not all states are supported (see above comment)
//--------------------------------------------------------------------------------
void TIrLMP::HandleReadyStateEvent(ULong event)
{
    switch (event) {
	case kIrDiscoverRequestEvent:
	    {
		XTRACE(kStnCntlReadyDiscoverEvent, 0, 0);
		TIrDiscoverRequest* discoverRequest = (TIrDiscoverRequest*)GetCurrentEvent();
		discoverRequest->fConflictDevAddr = kIrLAPBroadcastDevAddr;
		fState = kIrLMPDiscover;
		GetLAP->EnqueueEvent(discoverRequest);
	    }
	    break;

	case kIrConnectRequestEvent:
	    XTRACE(kStnCntlReadyConnectEvent, 0, 0);
	    // Forward connect request to LAPConn
	    GetLAPConn->EnqueueEvent(GetCurrentEvent());
	    break;

	case kIrListenRequestEvent:
	    XTRACE(kStnCntlReadyListenEvent, 0, 0);
	    // Forward listen request to LAPConn
	    GetLAPConn->EnqueueEvent(GetCurrentEvent());
	    break;

	case kIrConnectReplyEvent:
	case kIrListenReplyEvent:
	case kIrGetDataRequestEvent:
	case kIrCancelGetRequestEvent:
	    // Forward these to IrLAPConn
	    GetLAPConn->EnqueueEvent(GetCurrentEvent());
	    break;

	case kIrPutDataRequestEvent:
	case kIrCancelPutRequestEvent:
	    // Forward these to IrLAP
	    GetLAP->EnqueueEvent(GetCurrentEvent());
	    break;

	case kIrDisconnectRequestEvent:
	    // If request came from IrDA (IrGlue) pass it directly to IrLAP
	    //if (((TIrDisconnectEvent*)GetCurrentEvent())->fLSAPConn == nil) {
	    //  XTRACE(kStnCntlReadyDiscRequestEvent, 0, 0);
	    //  GetLAP->EnqueueEvent(GetCurrentEvent());
	    //}

	    // else (request came from an LSAPConn), pass it on to IrLAPConn
	    //else {
		XTRACE(kStnCntlReadyDiscRequestEvent, 1, 0);
		GetLAPConn->EnqueueEvent(GetCurrentEvent());
	    //}
	    break;

	case kIrDisconnectReplyEvent:
	    // If request came from IrDA (IrGlue) pass it back
	    //if (((TIrDisconnectEvent*)GetCurrentEvent())->fLSAPConn == nil) {
	    //  XTRACE(kStnCntlReadyDiscReplyEvent, 0, 0);
	    //  fIrDA->EnqueueEvent(GetCurrentEvent());
	    //}

	    // else (request came from an LSAPConn), pass it on to IrLAPConn
	    //else {
		XTRACE(kStnCntlReadyDiscReplyEvent, 1, 0);
		GetLAPConn->EnqueueEvent(GetCurrentEvent());
	    //}
	    break;
	    
	case kIrDiscoverReplyEvent:                 // jdg: this can happen now if we're doing
	    HandleDiscoverStateEvent(event);        // lots of discovers while connected and
	    break;                                  // then get an async disconnect (lmp reset)

	default:
	    XTRACE(kUnexpectedEvent, fState, event);
	    DebugLog("TIrLMP::HandleReadyStateEvent: bad event");
	    break;
    }

} // TIrLMP::HandleReadyStateEvent


//--------------------------------------------------------------------------------
//      HandleDiscoverStateEvent
//--------------------------------------------------------------------------------
void TIrLMP::HandleDiscoverStateEvent(ULong event)
{
    switch (event) {
	case kIrDiscoverReplyEvent:
	    {
		TIrDiscoverReply* discoverReply = (TIrDiscoverReply*)GetCurrentEvent();
		// If returned with no error and addr conflicts, do addr conflict resolution
		if ((discoverReply->fResult == noErr) &&
		    (!discoverReply->fPassiveDiscovery) &&
		    (AddrConflicts(discoverReply->fDiscoveredDevices, true))) {
		    // Initiate the first of the resolution calls
		    XTRACE(kStnCntlDiscoverDiscoverEvent, 0, 1);
		    XASSERT(fNumAddrConflicts > 0);
		    // Reuse the reply for the request to resolve the address conflict.
		    discoverReply->fEvent = kIrDiscoverRequestEvent;
		    discoverReply->fConflictDevAddr = fAddrConflicts[--fNumAddrConflicts];
		    fState = kIrLMPResolveAddress;
		    GetLAP->EnqueueEvent(discoverReply);
		}
		else {
		    // Either some error or no addr conflicts so discovery is complete
		    XTRACE(kStnCntlDiscoverDiscoverEvent, 0, 2);
		    fState = kIrLMPReady;
		    GetDiscovery->EnqueueEvent(discoverReply);
		    // JDG: queue up any pending requests for us (via normal queue, sigh)
		    if (fPendingRequests && !fPendingRequests->Empty()) {
			CListIterator *iter = CListIterator::cListIterator(fPendingRequests);
			for (TIrEvent* request = (TIrEvent*)iter->FirstItem();
			     iter->More(); request = (TIrEvent*)iter->NextItem()) {
				XTRACE(kStnCntlDiscoverDiscoverEvent, request->fEvent, 3);
				// Send the reply to ourselves (sigh)
				this->EnqueueEvent(request);
			}
			// now that we've processed the list, empty it (backwards first for speed)
			while (!fPendingRequests->Empty())
			    fPendingRequests->RemoveLast();     // remove last til no more left
			iter->release();
		    }
		}
	    }
	    break;

	//case kIrDisconnectRequestEvent:
	//  // This is only expected from IrDA (IrGlue) as all discovering/address resolution
	//  // should be done before any connections are established.
	//  XASSERT(((TIrDisconnectRequest*)GetCurrentEvent())->fLSAPConn == nil);
	//  // Pass request on to IrLAP
	//  GetLAP->EnqueueEvent(GetCurrentEvent());
	//  break;

	//case kIrDisconnectReplyEvent:
	//  // Pass reply back to IrDA
	//  GetDiscovery->EnqueueEvent(GetCurrentEvent());
	//  break;

	case kIrConnectReplyEvent:          // JDG ADDED THESE for listen/disconnect/discover
	case kIrListenReplyEvent:
	case kIrGetDataRequestEvent:
	case kIrCancelGetRequestEvent:
	    // Forward these to IrLAPConn
	    GetLAPConn->EnqueueEvent(GetCurrentEvent());
	    break;

	case kIrConnectRequestEvent:            // jdg hacking
	case kIrListenRequestEvent:
	case kIrDisconnectRequestEvent:
	case kIrPutDataRequestEvent:
	case kIrCancelPutRequestEvent:
	    // save until we get back to ready state
	    fPendingRequests->InsertLast(GetCurrentEvent());
	    break;

	default:
	    XTRACE(kUnexpectedEvent, fState, event);
	    DebugLog("TIrLMP::HandleDiscoverStateEvent: bad event");
	    break;
    }

} // TIrLMP::HandleDiscoverStateEvent


//--------------------------------------------------------------------------------
//      HandleResolveAddressStateEvent
//--------------------------------------------------------------------------------
void TIrLMP::HandleResolveAddressStateEvent(ULong event)
{
    switch (event) {
	case kIrDiscoverReplyEvent:
	    {
		XTRACE(kStnCntlResolveDiscoverEvent, 0, 0);
		TIrDiscoverReply* discoverReply = (TIrDiscoverReply*)GetCurrentEvent();

		// Any addr conflicts in current set of responses?  If so this rtn removes them
		(void)AddrConflicts(discoverReply->fDiscoveredDevices, false);

		// If an error was returned or we're finished resolving the original conflicts...
		if ((discoverReply->fResult != noErr) || (fNumAddrConflicts == 0)) {
		    fState = kIrLMPReady;
		    GetDiscovery->EnqueueEvent(discoverReply);
		}

		// Still some more conflicting addresses to resolve
		else {
		    XASSERT(fNumAddrConflicts > 0);
		    // Reuse the reply for the request to resolve the address conflict.
		    discoverReply->fConflictDevAddr = fAddrConflicts[--fNumAddrConflicts];
		    GetLAP->EnqueueEvent(discoverReply);
		}
	    }
	    break;

	case kIrDisconnectRequestEvent:
	    // This is only expected from IrDA (IrGlue) as all discovering/address resolution
	    // should be done before any connections are established.
	    XASSERT(((TIrDisconnectRequest*)GetCurrentEvent())->fLSAPConn == nil);
	    // Pass request on to IrLAP
	    GetLAP->EnqueueEvent(GetCurrentEvent());
	    break;

	case kIrDisconnectReplyEvent:
	    // Pass reply back to IrDA
	    GetDiscovery->EnqueueEvent(GetCurrentEvent());
	    break;

	default:
	    XTRACE(kUnexpectedEvent, fState, event);
	    DebugLog("TIrLMP::HandleResolveAddressStateEvent: bad event");
	    break;
    }

} // TIrLMP::HandleResolveAddressStateEvent


//================================ Helper methods ================================


//--------------------------------------------------------------------------------
//      AddrConflicts
//--------------------------------------------------------------------------------
Boolean TIrLMP::AddrConflicts(CList* discoveredDevices, Boolean setAddrConflicts)
{
    // This looks for any address conflicts between my address and all of the addresses
    // represented in the discoveredDevices list.  Any discoveredDevices with
    // conflicting addresses are removed from the list of discoveredDevices.
    // If setAddrConflicts is true then the conflicting addresses are saved in
    // fAddrConflicts, which is then used for address resolution.

    ULong addrToCheck;
    ULong uniqueCount = 0;
    Boolean thisConflicts;
    Boolean conflicts = false;
    ULong uniqueAddrs[kMaxReturnedAddrs+1];     // Extra 1 for my address

    // Initialize unique addrs to my address to weed out conlicts with my address too.
    uniqueAddrs[uniqueCount++] = GetLAP->GetMyDevAddr();

    // Init addr conflict fields if setting addr conflict info
    if (setAddrConflicts) {
	fNumAddrConflicts = 0;
    }

    // No conflicts if no entries returned
    if (discoveredDevices->GetArraySize() > 0) {
	// Go backwards so entries don't shift down (and also faster if entry removed from end)
	for (FastInt index1 = discoveredDevices->GetArraySize() - 1; index1 >= 0 ; index1--) {
	    TIrDscInfo* discoveryInfo = (TIrDscInfo*)discoveredDevices->At(index1);
	    // Now check to see if this entries addr is already in the uniqueAddrs list
	    thisConflicts = false;
	    addrToCheck = discoveryInfo->GetDeviceAddr();
	    for (ULong index2 = 0; index2 < uniqueCount; index2++) {
		if (addrToCheck == uniqueAddrs[index2]) {
		    // There is an address conflict, maybe add to conflicts, remove it
		    conflicts = thisConflicts = true;
		    if (setAddrConflicts) {
			if (fNumAddrConflicts < kMaxAddrConflicts) {
			    fAddrConflicts[fNumAddrConflicts++] = addrToCheck;
			}
		    }
		    discoveredDevices->RemoveAt(index1);
		    discoveryInfo->release();
		    break;
		}
	    }
	    // If no conflicts then this address is also unique, add it to the unique addr list.
	    if (!thisConflicts) {
		if (uniqueCount < (kMaxReturnedAddrs+1)) {
		    uniqueAddrs[uniqueCount++] = addrToCheck;
		}
		else {
		    DebugLog("TIrLMP::AddrConflicts: too many unique addrs");
		}
	    }
	}
    }

    return conflicts;

} // TIrLMP::AddrConflicts


//--------------------------------------------------------------------------------
//      Demultiplexor
//--------------------------------------------------------------------------------
void TIrLMP::Demultiplexor(CBufferSegment* inputBuffer)
{
    // All of the real work is done in IrLAPConn - pass it on
    GetLAPConn->Demultiplexor(inputBuffer);

} // TIrLMP::Demultiplexor


//--------------------------------------------------------------------------------
//      FillInLMPDUHeader
//--------------------------------------------------------------------------------
ULong TIrLMP::FillInLMPDUHeader(TIrPutRequest* putRequest, UByte* buffer)
{
    XTRACE(kLogFillInLMPDUHeader, 0, 0);
    // All of the real work is done in IrLAPConn - pass it on
    return GetLAPConn->FillInLMPDUHeader(putRequest, buffer);

} // TIrLMP::FillInLMPDUHeader


//--------------------------------------------------------------------------------
//      StartOneSecTicker
//--------------------------------------------------------------------------------
void TIrLMP::StartOneSecTicker()
{
    XTRACE(kLogStartOneSecTicker, 0, fTimerClients);
    
    // NOTE: This is exclusively used for the LSAPConn connect watchdog timer(s)
    // I'm trying to keep it small and simple.  If there are other timers that
    // can take advantage of this 1 second tick counter then they may be able to
    // be piggy-backed on top of this timer.

    if (fTimerClients++ == 0) {
	fIrDA->StartTimer(kTimer_LMP, 1 * kSeconds, kIrConnWatchdogExpiredEvent);
    }

} // TIrLMP::StartOneSecTicker


//--------------------------------------------------------------------------------
//      StopOneSecTicker
//--------------------------------------------------------------------------------
void TIrLMP::StopOneSecTicker()
{
    // NOTE: This is exclusively used for the LSAPConn connect watchdog timer(s)

    if (fTimerClients > 0) {
	if (--fTimerClients == 0) {
	    fIrDA->StopTimer(kTimer_LMP);
	}
    }

} // TIrLMP::StopOneSecTicker


//--------------------------------------------------------------------------------
//      TimerComplete
//--------------------------------------------------------------------------------
void TIrLMP::TimerComplete(ULong refCon)
{
    // All of the real work is done in IrLAPConn - pass it on
    GetLAPConn->TimerComplete(refCon);

    // If one sec ticker is still active, retrigger the ticker
    if (refCon == kIrConnWatchdogExpiredEvent && fTimerClients > 0) {
	fIrDA->StartTimer(kTimer_LMP, 1 * kSeconds, kIrConnWatchdogExpiredEvent);
    }

} // TIrLMP::TimerComplete

