/*
    File:       IrEvent.cpp
    Contains:   Implementation of IrDATool to IrDA glue


*/

#include "IrEvent.h"
#include "CList.h"
#include "CListIterator.h"
#include "CBufferSegment.h"
#include "IrDALog.h"

#define private static

#if (hasTracing > 0 && hasIrEventTracing > 0)

enum TraceCodes
{
    kLogInitEventLists = 1,
    kLogDeleteEventList,
    
    kAllocateEventBlock,
    kGrabEventBlock,
    kReleaseEventBlock,
    
    kLogReleaseErr1,
    kLogReleaseErr2,
    kLogReleaseErr3,
    
    kLogGrabErr1,
    kLogGrabErr2,
    kLogGrabErr3
    /*
    kLogChecking,
    kLogCheckOk,
    kLogCheckFailed,
    
    kLogCList,
    kLogCListSize,
    kLogCListElemSize,
    kLogCListChunk,
    
    kLogCListAllocated,
    kLogCListBuffer,
    kLogCListIters,
    kLogCListContents
    */

};

private
EventTraceCauseDesc TraceEvents[] = {

    {kLogInitEventLists,            "IrEvent: Init event block lists"},
    {kLogDeleteEventList,           "IrEvent: Delete event list, contents=, count="},

    {kAllocateEventBlock,           "IrEvent: Allocate Event Block, list size="},
    {kGrabEventBlock,               "IrEvent: Grab Event Block"},
    {kReleaseEventBlock,            "IrEvent: Release Event Block"},
    
    {kLogReleaseErr1,               "IrEvent: Release ERROR, not on in use list, err="},
    {kLogReleaseErr2,               "IrEvent: Release ERROR, in use list="},
    {kLogReleaseErr3,               "IrEvent: Release ERROR, in use size, members="},

    {kLogGrabErr1,                  "IrEvent: Grab ERROR, failed to add to in-use list, err="},
    {kLogGrabErr2,                  "IrEvent: Grab ERROR, in use list="},
    {kLogGrabErr3,                  "IrEvent: Grab ERROR, in use size, members="}
    
    /*
    
    {kLogChecking,                  "IrEvent: checking event="},
    {kLogCheckOk,                   "IrEvent: check passed, index="},
    {kLogCheckFailed,               "IrEvent: check failed, size,members="},
    
    {kLogCList,                     "IrEvent: clist obj="},
    {kLogCListSize,                 "IrEvent: clist size="},
    {kLogCListElemSize,             "IrEvent: clist element size="},
    {kLogCListChunk,                "IrEvent: clist chunk size="},
    
    {kLogCListAllocated,            "IrEvent: clist allocated size="},
    {kLogCListBuffer,               "IrEvent: clist buffer="},
    {kLogCListIters,                "IrEvent: clist iters="},
    {kLogCListContents,             "IrEvent: clist data"}
    */
};

#define XTRACE(x, y, z) IrDALogAdd (x, y, (uintptr_t)z & 0xffff, TraceEvents, true )
#else
    #define XTRACE(x, y, z) ((void)0)
#endif

//
// review: make these static entries in the event record?
//
CList *gFreeEventList = nil;
CList *gInUseEventList = nil;
void DeleteEventListItems(CList *eventlist, Boolean check_contents);

//--------------------------------------------------------------------------------
#define super OSObject
    OSDefineMetaClassAndStructors(TIrEvent, OSObject);

//--------------------------------------------------------------------------------


//--------------------------------------------------------------------------------
//      InitEventLists
//--------------------------------------------------------------------------------
/* static */
IrDAErr
TIrEvent::InitEventLists()
{
    XTRACE(kLogInitEventLists, 0, 0);
    
    ncheck(gFreeEventList);
    ncheck(gInUseEventList);
    
    // sanity checks on sizes of overlaid classes
    // todo: switch to simple union.
    check(sizeof(TIrLargestEvent) >= sizeof(TIrEvent));
    check(sizeof(TIrLargestEvent) >= sizeof(TIrDiscoverEvent));
    check(sizeof(TIrLargestEvent) >= sizeof(TIrExtDiscoverEvent));
    check(sizeof(TIrLargestEvent) >= sizeof(TIrLSAPConnEvent));
    check(sizeof(TIrLargestEvent) >= sizeof(TIrConnectEvent));
    check(sizeof(TIrLargestEvent) >= sizeof(TIrDataXferEvent));
    check(sizeof(TIrLargestEvent) >= sizeof(TIrLookupEvent));
    
    // initial debugging
    /*
    XTRACE(kLogInitEventBlockList, sizeof(TIrLargestEvent), sizeof(TIrEvent));
    XTRACE(kLogInitEventBlockList, sizeof(TIrLargestEvent), sizeof(TIrDiscoverEvent));
    XTRACE(kLogInitEventBlockList, sizeof(TIrLargestEvent), sizeof(TIrExtDiscoverEvent));
    XTRACE(kLogInitEventBlockList, sizeof(TIrLargestEvent), sizeof(TIrLSAPConnEvent));
    XTRACE(kLogInitEventBlockList, sizeof(TIrLargestEvent), sizeof(TIrConnectEvent));
    XTRACE(kLogInitEventBlockList, sizeof(TIrLargestEvent), sizeof(TIrDataXferEvent));
    XTRACE(kLogInitEventBlockList, sizeof(TIrLargestEvent), sizeof(TIrLookupEvent));
    */
    
    // Init the lists of ir event block's
    gFreeEventList = CList::cList();
    require(gFreeEventList, Fail);
    
    gInUseEventList = CList::cList();
    require(gInUseEventList, Fail);

    return noErr;

Fail:
    if (gFreeEventList) gFreeEventList->release();
    gFreeEventList->release();

    if (gInUseEventList) gInUseEventList->release();
    gInUseEventList->release();

    return kIrDAErrNoMemory;

} // TIrEvent::InitEventLists


//--------------------------------------------------------------------------------
//      DeleteEventLists
//--------------------------------------------------------------------------------
/* static */
void
TIrEvent::DeleteEventLists(void)
{
    if (gFreeEventList) {           // free events off our free list
	DeleteEventListItems(gFreeEventList,  false);
	gFreeEventList->release();
	gFreeEventList = nil;
    }
    if (gInUseEventList) {          // free allocated events, and their contents too
	DeleteEventListItems(gInUseEventList, true);
	gInUseEventList->release();
	gInUseEventList = nil;
    }
} // DeleteEventLists

void
DeleteEventListItems(CList *eventlist, Boolean check_contents)
{
    XTRACE(kLogDeleteEventList, check_contents, eventlist->GetArraySize());

    if (eventlist->GetArraySize() > 0) {
	TIrEvent *event;

	// removing from end is much faster
	for (int index = eventlist->GetArraySize() - 1; index >= 0 ; index--) {
	    event = (TIrEvent*)eventlist->At(index);
	    eventlist->RemoveAt(index);
	    require(event, Fail);
	    XTRACE(kLogDeleteEventList, 0, event);
	    XTRACE(kLogDeleteEventList, 0, event->fEvent);
	    /** this appears to not work well ...
	    if (check_contents) {
		switch (event->fEvent) {
		    case kIrPutDataRequestEvent:
		    case kIrPutDataReplyEvent:
			{
			    TIrPutEvent *putevent = (TIrPutEvent *)event;
			    CBufferSegment *buf;
			    buf = OSDynamicCast(CBufferSegment, putevent->fData);
			    if (buf)
				IOLog("Could have freed 0x%lx\n", (UInt32) buf);
			    else
				IOLog("logic err in event/buf stuff\n");
			}
			break;
		}
	    }
	    ****/
	    event->release();
	}
    }
Fail:
    return;

} // DeleteEventListItems


//--------------------------------------------------------------------------------
//      GrabEventBlock
//--------------------------------------------------------------------------------
/*static*/
TIrEvent *
TIrEvent::GrabEventBlock(ULong event, ULong size)
{
#pragma unused(size)
    TIrEvent* eventBlock = nil;
    IrDAErr err;
    
    require(gFreeEventList, Fail_New_EventBlock);
    require(gInUseEventList, Fail_New_EventBlock);

    check( size <= sizeof( TIrLargestEvent ) );
	
    if (gFreeEventList->GetArraySize() > 0) {
	// Pull last one off list (more efficient that way)
	eventBlock = (TIrEvent *)gFreeEventList->Last();
	require(eventBlock, Fail_New_EventBlock);
	check(eventBlock->fAllocated == false);
	gFreeEventList->RemoveLast();
    }
    else {
	// List is empty, so allocate a new one
	XTRACE(kAllocateEventBlock, gInUseEventList->GetArraySize(), gFreeEventList->GetArraySize());
	eventBlock = TIrLargestEvent::tIrLargestEvent();
	require(eventBlock, Fail_New_EventBlock);
    }
    
    err = gInUseEventList->InsertLast(eventBlock);      // keep a list of allocated events
    ncheck(err);
    if (err) {
	XTRACE(kLogGrabErr1, err >> 16, err);
	XTRACE(kLogGrabErr2, 0, gInUseEventList);
	XTRACE(kLogGrabErr3, 0, gInUseEventList->GetArraySize());
	/*for (int index = 0; index < gInUseEventList->GetArraySize(); index++ ) {
	    ULong member;
	    member = (ULong)gInUseEventList->At(index);
	    XTRACE(kLogGrabErr3, member >> 16, member);
	}*/
    }
    //check(CheckAllocated(eventBlock));

    eventBlock->fEvent = (UByte)event;
    eventBlock->fClient = nil;
    eventBlock->fDest   = nil;
    eventBlock->fResult = noErr;
    eventBlock->fAllocated = true;

Fail_New_EventBlock:
    XTRACE( kGrabEventBlock, 0, eventBlock);

    return eventBlock;

} // TIrEvent::GrabEventBlock


//--------------------------------------------------------------------------------
//      ReleaseEventBlock
//--------------------------------------------------------------------------------
/*static*/
void
TIrEvent::ReleaseEventBlock(TIrEvent * eventBlock)
{
    XTRACE( kReleaseEventBlock, 0, eventBlock);
    require(eventBlock, Fail);
    require(eventBlock->fAllocated == true, Fail);
    
    if (gInUseEventList) {          // take it off the in-use list
	IrDAErr err;
	err = gInUseEventList->Remove(eventBlock);
	ncheck(err);
	if (err) {
	    XTRACE(kLogReleaseErr1, err >> 16, err);
	    XTRACE(kLogReleaseErr2, 0, gInUseEventList);
	    XTRACE(kLogReleaseErr3, 0, gInUseEventList->GetArraySize());
	    /*
	    for (int index = 0; index < gInUseEventList->GetArraySize(); index++ ) {
		ULong member;
		member = (ULong)gInUseEventList->At(index);
		XTRACE(kLogReleaseErr3, member >> 16, member);
	    }*/
	}
    }
    
    eventBlock->fAllocated = false;
	    
    if (gFreeEventList) {           // add it to the free list
	gFreeEventList->InsertLast(eventBlock);
    }
    else {                          // this probably won't happen anymore ...
	eventBlock->release();
    }
    
    
Fail:
    return;
    
} // TIrEvent::ReleaseEventBlock

#if (hasTracing > 0 && hasIrEventTracing > 1)



#endif // #if hasIrEventTracing > 1


#undef super
#define super TIrEvent
    OSDefineMetaClassAndStructors(TIrLargestEvent, TIrEvent);
//--------------------------------------------------------------------------------
//      TIrLargestEvent::tIrLargestEvent
//--------------------------------------------------------------------------------
/*static*/
TIrLargestEvent *
TIrLargestEvent::tIrLargestEvent()
{
    TIrLargestEvent *obj;
    
    obj = new TIrLargestEvent;
    if (obj && !obj->init()) {
	obj->release();
	obj = nil;
    }
    return obj;
}


#pragma mark ------ Code recycle bin
#ifdef never

Boolean
TIrEvent::CheckAllocated(TIrEvent * eventBlock)
{
    int index;
    XTRACE(kLogChecking, (int)eventBlock >> 16, eventBlock);
    
    if (gInUseEventList) {
	DumpCList(gInUseEventList);     // dump the clist to xtrace buffer
	
	index = gInUseEventList->GetIdentityIndex(eventBlock);
	if (index >= 0) {
	    XTRACE(kLogCheckOk, 0, index);
	    return true;
	}
	else {
	    IOLog("Event check failed\n");
	    
	    XTRACE(kLogCheckFailed, 0xffff, 0xffff);
	    /*
	    for (int index = 0; index < gInUseEventList->GetArraySize(); index++ ) {
		ULong member;
		member = (ULong)gInUseEventList->At(index);
		XTRACE(kLogCheckFailed, member >> 16, member);
	    }*/
	    return false;
	}
    }
    return false;
}

void DumpCList(CList *list)
{
    XTRACE(kLogCList,           (int)list >> 16, list);
    XTRACE(kLogCListSize,       list->fSize >> 16, list->fSize);
    XTRACE(kLogCListElemSize,   list->fElementSize >> 16, list->fElementSize);
    XTRACE(kLogCListChunk,      list->fChunkSize >> 16, list->fChunkSize);
    
    XTRACE(kLogCListAllocated,  list->fAllocatedSize >> 16, list->fAllocatedSize);
    XTRACE(kLogCListBuffer,     (int)list->fArrayBlock >> 16, list->fArrayBlock);
    XTRACE(kLogCListIters,      (int)list->fIterator >> 16, list->fIterator);
    
    if (list->fElementSize == 4 && list->fArrayBlock) { // sanity
	int i;
	UInt32 *block = (UInt32 *)list->fArrayBlock;
	
	for (i = 0 ; i < list->fAllocatedSize; i++) {
	    UInt32 x;
	    x = *block++;
	    XTRACE(kLogCListContents, x >> 16, x);
	}
    }
}

#endif // never

