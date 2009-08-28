/*
    File:       IrStream.cpp

    Contains:   Implementation of the TIrStream base class


*/

#include "IrStream.h"
#include "CList.h"
#include "IrEvent.h"

#if (hasTracing > 0 && hasIrStreamTracing > 0)

enum {
    kLogCreate = 1,
    kLogFree,
    kLogFreeQueue,
    kLogInit,
    kLogInit2,
    kLogInit3,
    kLogNewEventList,
    kLogRetainEventList,
    kLogEnqueue,
    kLogEnqueueThis,
    kLogEventRecordRun,
    
    kLogCleanup1,
    kLogCleanup2,
    
    kGenericEnqueue,
    kGenericDequeueStart,
    kGenericDequeueEnd,


				    // the order of these must match enum in IrEvent.h
    kLogEventNamesStart,            // 0
    
    kLogNameOutputComplete,         // 1
    kLogNameInputComplete,          // 2
    
    kLogNameDiscoverRequest,        // 3
    kLogNameDiscoverReply,          // 4
    
    kLogNameConnectRequest,         // 5
    kLogNameConnectReply,           // 6
    
    kLogNameListenRequest,          // 7
    kLogNameListenReply,            // 8
    
    kLogNameAcceptRequest,          // 9
    kLogNameAcceptReply,            // a
    
    kLogNameGetDataRequest,         // b
    kLogNameGetDataReply,           // c
    
    kLogNamePutDataRequest,         // d
    kLogNamePutDataReply,           // e
    
    kLogNameLookupRequest,          // f
    kLogNameLookupReply,            // 10
    
    kLogNameCancelGetRequest,       // 11
    kLogNameCancelGetReply,         // 12
    
    kLogNameCancelPutRequest,       // 13
    kLogNameCancelPutReply,         // 14
    
    kLogNameReleaseRequest,         // 15
    kLogNameReleaseReply,           // 16
    
    kLogNameDisconnectRequest,      // 17
    kLogNameDisconnectReply,        // 18
    
    kLogNameLocalBusy,              // 19
    kLogNameLocalBusyClear,         // 1a
    
    kLogNameLapTimers,              // 1b
    kLogNameMediaBusyTimer,         // 1c
    kLogNameSlotTimer,              // 1d
    kLogNameQueryTimer,             // 1e
    kLogNameBackoffTimer,           // 1f
    kLogNameFinalTimer,             // 20
    kLogNamePollTimer,              // 21
    kLogNameWatchdogTimer,          // 22
    kLogNameTurnaroundTimer,        // 23
    kLogNameLastLapTimer,           // 24
    
    kLogNameLMPTimers,              // 25
    kLogNameConnWatchdogTimer,      // 26
    kLogNameIdleDisconnectTimer,    // 27
    kLogNameLastLMPTimer,           // 28
    
    kLogNameChangeSpeedComplete     // 29
    

};

static
EventTraceCauseDesc TraceEvents[] = {
    {kLogCreate,            "IrStream: create, obj="},
    {kLogFree,              "IrStream: free, obj="},
    {kLogFreeQueue,         "IrStream: clist retain=, length="},
    {kLogInit,              "IrStream: init, obj="},
    {kLogInit2,             "IrStream: qtrace array="},
    {kLogInit3,             "IrStream: qtrace index="},
    {kLogNewEventList,      "IrStream: new clist, obj="},
    {kLogRetainEventList,   "IrStream: clist retain, new count="},
    {kLogEnqueue,           "IrStream: enqueue irevent="},
    {kLogEnqueueThis,       "IrStream: enqueued for obj="},
    {kLogEventRecordRun,    "IrStream: run of event record at"},
    
    {kLogCleanup1,          "IrStream: nuking event queued for us on queue"},
    {kLogCleanup2,          "IrStream: nuking event queued for us on fNextEvent"},
    
    {kGenericEnqueue,       "IrStream: Generic Enqueue Event, event="},
    {kGenericDequeueStart,  "IrStream: Generic Dequeue Event Start"},
    {kGenericDequeueEnd,    "IrStream: Generic Dequeue Event Complete"},
    
    {kLogEventNamesStart,       "IrStream: dummy"},                     // 0
    {kLogNameOutputComplete,    "IrStream: output complete event"},     // 1
    {kLogNameInputComplete,     "IrStream: input complete event"},      // 2
    
    {kLogNameDiscoverRequest,   "IrStream: discover request event"},    // 3
    {kLogNameDiscoverReply,     "IrStream: discover reply event"},      // 4
    
    {kLogNameConnectRequest,    "IrStream: connect request event"},     // 5
    {kLogNameConnectReply,      "IrStream: connect reply event"},       // 6
    
    {kLogNameListenRequest,     "IrStream: listen request event"},      // 7
    {kLogNameListenReply,       "IrStream: listen reply event"},        // 8
    
    {kLogNameAcceptRequest,     "IrStream: accept request event"},      // 9
    {kLogNameAcceptReply,       "IrStream: accept reply event"},        // a
    
    {kLogNameGetDataRequest,    "IrStream: get data request event"},    // b
    {kLogNameGetDataReply,      "IrStream: get data reply event"},      // c
    
    {kLogNamePutDataRequest,    "IrStream: put data request event"},    // d
    {kLogNamePutDataReply,      "IrStream: put data reply event"},      // e
	
    {kLogNameLookupRequest,     "IrStream: lookup request event"},      // f
    {kLogNameLookupReply,       "IrStream: lookup reply event"},        // 10
    
    {kLogNameCancelGetRequest,  "IrStream: cancel get request event"},  // 11
    {kLogNameCancelGetReply,    "IrStream: cancel get reply event"},    // 12
    
    {kLogNameCancelPutRequest,  "IrStream: cancel put request event"},  // 13
    {kLogNameCancelPutReply,    "IrStream: cancel put reply event"},    // 14

    {kLogNameReleaseRequest,    "IrStream: release request event"},     // 15
    {kLogNameReleaseReply,      "IrStream: release reply event"},       // 16
	
    {kLogNameDisconnectRequest, "IrStream: disconnect request event"},  // 17
    {kLogNameDisconnectReply,   "IrStream: disconnect reply event"},    // 18
    
    {kLogNameLocalBusy,         "IrStream: local busy event"},          // 19
    {kLogNameLocalBusyClear,    "IrStream: local busy clear event"},    // 1a
    
    {kLogNameLapTimers,         "IrStream: lap timer"},                 // 1b
    {kLogNameMediaBusyTimer,    "IrStream: media busy timer event"},    // 1c
    {kLogNameSlotTimer,         "IrStream: slot timer event"},          // 1d
    {kLogNameQueryTimer,        "IrStream: query timer event"},         // 1e
    {kLogNameBackoffTimer,      "IrStream: backoff timer event"},       // 1f
    {kLogNameFinalTimer,        "IrStream: final timer event"},         // 20
    {kLogNamePollTimer,         "IrStream: poll timer event"},          // 21
    {kLogNameWatchdogTimer,     "IrStream: watchdog timer event"},      // 22
    {kLogNameTurnaroundTimer,   "IrStream: turnaround timer event"},    // 23
    {kLogNameLastLapTimer,      "IrStream: last lap timer"},            // 24
    
    {kLogNameLMPTimers,             "IrStream: lmp timers"},                    // 25
    {kLogNameConnWatchdogTimer,     "IrStream: connection watchdog timer event"},   // 26
    {kLogNameIdleDisconnectTimer,   "IrStream: idle disconnect timer event"},       // 27
    {kLogNameLastLMPTimer,          "IrStream: last lap timer"},                    // 28
    
    {kLogNameChangeSpeedComplete,   "IrStream: changespeed complete event"}     // 29
    
};
    #define XTRACE(x, y, z) IrDALogAdd ( x, y, (uintptr_t)z & 0xffff, TraceEvents, true)
    #define QTRACE(x, y, z) IrDALogAdd ( x, y, (uintptr_t)z & 0xffff, fTraceArray, true)
    #define QTRACEArray(x, y, z, array) IrDALogAdd ( x, y, (uintptr_t)z & 0xffff, array, true)
    #define ETRACE(x, y, z) IrDALogAdd ((x + kLogEventNamesStart), y, (uintptr_t)z & 0xffff, TraceEvents, true)
#else
    #define XTRACE(x, y, z) ((void)0)
    #define QTRACE(x, y, z) ((void)0)
    #define QTRACEArray(x, y, z, array) ((void)0)
    #define ETRACE(x, y, z) ((void)0)
#endif

//--------------------------------------------------------------------------------
#define super OSObject
    OSDefineMetaClassAndAbstractStructors(TIrStream, OSObject);
//--------------------------------------------------------------------------------

//--------------------------------------------------------------------------------
// Define the static fields here, or the dynamic loader doesn't find them.
//--------------------------------------------------------------------------------
TIrEvent    *TIrStream::fCurrentEvent;          // event we're running (not on the queue)
TIrEvent    *TIrStream::fNextEvent;             // slight optimization to keep next event off the fifo
CList       *TIrStream::fPendingEventsList;     // add to front, take from last, event FIFO

//--------------------------------------------------------------------------------
//      free
//--------------------------------------------------------------------------------
void
TIrStream::free()
{
    int list_retain_count;

    XTRACE(kLogFree, 0, this);
    
    // should have a pending events list until the last stream is freed
    require(fPendingEventsList, Fail);
    
    // Free the pending events list
    list_retain_count = fPendingEventsList->getRetainCount();
    XTRACE(kLogFreeQueue, list_retain_count, fPendingEventsList->GetArraySize());

    fPendingEventsList->release();  // this stream doesn't need the list anymore
    
    if (list_retain_count == 1) {   // if retain count was one, then we just freed it
	fPendingEventsList = nil;
    }
    
    // this stream is going away, let's go over the (static) pending event list and
    // nuke anything that's queued up for us.  This doesn't normally happen unless
    // we're stopped in the middle of doing stuff.
    
    // first check the clist (which may have just been freed)
    if (fPendingEventsList != nil && fPendingEventsList->GetArraySize() > 0) {
	int index;
	// go over the list backwards to make deleting easier
	for (index = fPendingEventsList->GetArraySize()-1; index >= 0; index--) {
	    TIrEvent *event = (TIrEvent *)fPendingEventsList->At(index);
	    if (event && event->fDest == this) {
		XTRACE(kLogCleanup1, 0, event);
		fPendingEventsList->RemoveAt(index);        // just remove it from the list
	    }
	}
    }
    // then check fNextEvent
    if (fNextEvent && fNextEvent->fDest == this) {
	XTRACE(kLogCleanup2, 0, fNextEvent);
	// could call DequeueEvent, but I don't want to clobber fCurrentEvent
	if (fPendingEventsList) {
	    fNextEvent = (TIrEvent *)fPendingEventsList->Last();
	    if (fNextEvent) fPendingEventsList->RemoveLast();
	}
	else fNextEvent = nil;
    }
    
Fail:
    super::free();

} // free


//--------------------------------------------------------------------------------
//      Init
//--------------------------------------------------------------------------------
Boolean TIrStream::Init(TIrGlue *irda, EventTraceCauseDesc *trace, UInt16 index)
{
    XTRACE(kLogInit, 0, this);
    XTRACE(kLogInit2, 0, trace);
    XTRACE(kLogInit3, 0, index);
    
    if (!super::init()) return false;
    
    fIrDA = irda;                       // save this for all our derived classes (make static?)

    if (fPendingEventsList == nil) {    // if first IrStream to get init'd
	fPendingEventsList = CList::cList();    // create the pending event list
	require(fPendingEventsList, Fail);
	XTRACE(kLogNewEventList, 0, fPendingEventsList);
	check(fCurrentEvent == nil);
	check(fNextEvent == nil);
    }
    else {                              // if not the first stream, then incr the
	fPendingEventsList->retain();       // list's in-use count!
	XTRACE(kLogRetainEventList, 0, fPendingEventsList->getRetainCount());
    }
    
#if (hasTracing > 0 && hasIrStreamTracing > 0)
    if( trace != 0 ) {                  // support funky enqueue/dequeue logging
	fTraceIndex = index;            // index = enqueue, index+1 = dequeue start
	fTraceArray = trace;            // index+2 = dequeue
    }
    else {                              // Use the generic trace code if the subclass
	fTraceIndex = kGenericEnqueue;          // didn't explicitly set them
	fTraceArray = TraceEvents;              // generic if client not tracing
    }
#endif

    return true;

Fail:
    return false;

} // TIrStream::Init


//--------------------------------------------------------------------------------
//      EnqueueEvent
//--------------------------------------------------------------------------------
IrDAErr TIrStream::EnqueueEvent(TIrEvent *eventBlock)
{
    UInt8   eventNum;
    require(eventBlock, Fail);
    
    eventNum = eventBlock->fEvent;
    
    QTRACE(fTraceIndex, (uintptr_t)this >> 16, this);
    ETRACE(eventNum, 0, eventNum);
    XTRACE(kLogEnqueue, 0, eventBlock);
    //XTRACE(kLogEnqueueThis, (int)this >>16, this);
    
    require(eventNum > 0 && eventNum <= kIrMaxEventNumber, Fail);

    eventBlock->fDest = this;           // single queue, so keep track of destination
    
    if (fNextEvent == nil) {
	fNextEvent = eventBlock;
    }
    else {
	// Check for cases where the same event block is being enqueued more than once
	// Although this won't catch cases where the same event block is put on the list
	check(eventBlock != fNextEvent);

	// Add the event to the pending events list
	fPendingEventsList->InsertFirst(eventBlock);
    }

    //XASSERT(fIrDA);
    //fIrDA->NextStateMachine(this);

    return noErr;

Fail:
    return errBadArg;

} // TIrStream::EnqueueEvent


//--------------------------------------------------------------------------------
//      DequeueEvent
//--------------------------------------------------------------------------------
/*static*/
void TIrStream::DequeueEvent()
{
    require(fPendingEventsList, Fail);
    
    // Next event becomes the current event
    fCurrentEvent = fNextEvent;

    // Take next event from the pending events list (if any)
    fNextEvent = (TIrEvent *)fPendingEventsList->Last();
    if (fNextEvent) {
	fPendingEventsList->RemoveLast();
    }

Fail:
    return;

} // TIrStream::DequeueEvent

//--------------------------------------------------------------------------------
//      RunQueue
//--------------------------------------------------------------------------------
/* static */
void TIrStream::RunQueue()
{
    TIrStream *dest;                // destination of the current event request/reply
#if (hasTracing > 0 && hasIrStreamTracing > 0)
    EventTraceCauseDesc *array;     // save pointer to subclases debug table
    UInt32              index;      // save index to use (let's us do this after "delete obj")
    UInt8           orig_event;     // saved event number
#endif
    
    while (true) {                  // single queue for all IrStream objects
	DequeueEvent();
	if (fCurrentEvent == nil) {     // we're done, seeya
	    break;
	}
	dest = fCurrentEvent->fDest;        // get back to individual IrStream object
	check(dest);

	check(fCurrentEvent->fEvent > 0 && fCurrentEvent->fEvent <= kIrMaxEventNumber);     
	
#if (hasTracing > 0 && hasIrStreamTracing > 0)
	orig_event = fCurrentEvent->fEvent;
	array = dest->fTraceArray;          // save these in case the object is deleted on us
	index = dest->fTraceIndex;          //  as a result of running the nextstate routine
#endif  
	QTRACEArray(index+1, (uintptr_t)dest >> 16, dest, array);  // object event start message
	ETRACE(orig_event, 0, orig_event);
	XTRACE(kLogEventRecordRun, 0, fCurrentEvent);
	
	dest->NextState(fCurrentEvent->fEvent);     // send the event to the stream object for processing!
	
	QTRACEArray(index+2, orig_event, fCurrentEvent->fEvent, array);
    }
    return;

} // TIrStream::ProcessNextEvent
