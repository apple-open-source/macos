/*
    File:       IrDiscovery.cpp

    Contains:   xxx put contents here xxx
*/
#include "IrDiscovery.h"
#include "CList.h"
#include "IrDscInfo.h"
#include "IrGlue.h"
#include "IrLMP.h"
#include "CListIterator.h"

#if (hasTracing > 0 && hasIrDiscoveryTracing > 0)

enum IrDiscoverTraceCodes
{
    kLogNew = 1,
    kLogFree,
    kLogInit,
    kLogNextState,
    kLogExtDiscStart,
    kLogDiscoveryStart,
    kLogDiscoverComplete,
    kIrExtDiscoverComplete,
    kLogDiscoverDeleteList,
    
    kEnqueueEvent,
    kDequeueEventStart,
    kDequeueEventEnd

};

static
EventTraceCauseDesc IrDiscoverEvents[] = {
    {kLogNew,                   "IrDiscovery: new object="},
    {kLogFree,                  "IrDiscovery: free object"},
    {kLogInit,                  "IrDiscovery: init"},
    {kLogNextState,             "IrDiscovery: next state, event=, state="},
    {kLogExtDiscStart,          "IrDiscovery: Ext start, numSlots="},
    {kLogDiscoveryStart,        "IrDiscovery: start, slots=, state="},
    {kLogDiscoverComplete,      "IrDiscovery: complete, obj="},
    {kIrExtDiscoverComplete,    "IrDiscovery: Ext complete count="},
    {kLogDiscoverDeleteList,    "IrDiscovery: delete discovered list, all="},

    {kEnqueueEvent,             "IrDiscovery: Event Queued"},
    {kDequeueEventStart,        "IrDiscovery: Event Start"},
    {kDequeueEventEnd,          "IrDiscovery: Event End"}
};

#define XTRACE(x, y, z) IrDALogAdd( x, y, (uintptr_t)z & 0xffff, IrDiscoverEvents, true )

#else
    #define XTRACE(x, y, z) ((void)0)
#endif

enum {
    kMaxDiscoverListSize = 16           // Limit the discovered list size
};

#define super TIrStream
    OSDefineMetaClassAndStructors(CIrDiscovery, TIrStream);

/*static*/
CIrDiscovery *
CIrDiscovery::cIrDiscovery(TIrGlue * glue)
{
    CIrDiscovery *obj = new CIrDiscovery;
    
    XTRACE(kLogNew, 0, obj);

    if (obj && !obj->Init(glue)) {
	obj->release();
	obj = nil;
    }
    return obj;
}

void
CIrDiscovery::free()
{
    //int Review_Event_Usage;     // release events on the pendng discover list?
    
    XTRACE(kLogFree, 0, this);

    DeleteDiscoveredDevicesList();      // free the objects on the list

#define FREE(x) { if (x) { (x)->release(); x = nil; }}  
    FREE(fDiscoveredDevices);
    FREE(fPendingDiscoverList);
    FREE(fMyDscInfo);

    
    super::free();
    return;
}


Boolean
CIrDiscovery::Init(TIrGlue * glue)
{
    XTRACE(kLogInit, 0, this);
    
    fState  = kDiscoverIdle;
    fPendingDiscoverList = nil;
    fDiscoveredDevices = nil;
    fMyDscInfo = nil;
	    

#if (hasTracing > 0 && hasIrDiscoveryTracing > 0)
    if (!super::Init(glue, IrDiscoverEvents, kEnqueueEvent)) return false;
#else
    if (!super::Init(glue)) return false;
#endif


    fPendingDiscoverList = CList::cList();
    require(fPendingDiscoverList, Fail);
    
    fDiscoveredDevices = CList::cList();
    require(fDiscoveredDevices, Fail);
    
    fMyDscInfo = TIrDscInfo::tIrDscInfo();
    require(fMyDscInfo, Fail);
    
    fMyDscInfo->SetNickname("Mac/OS-X");                    // hostname unavailable to kext's.  foo.
    fMyDscInfo->SetServiceHints(kDevInfoHintComputer);      // clients can now add/delete from this set
    
    return true;
    
Fail:

    if (fPendingDiscoverList) {
	fPendingDiscoverList->release();
	fPendingDiscoverList = nil;
    }
    
    if (fDiscoveredDevices) {
	fDiscoveredDevices->release();
	fDiscoveredDevices = nil;
    }
    
    if (fMyDscInfo) {
	fMyDscInfo->release();
	fMyDscInfo = nil;
    }
    return false;
}


//--------------------------------------------------------------------------------
//      NextState
//--------------------------------------------------------------------------------
void CIrDiscovery::NextState( ULong event )
{
    XTRACE(kLogNextState, event, fState);
    
    require(event == kIrDiscoverRequestEvent || event == kIrDiscoverReplyEvent || event == kIrDisconnectReplyEvent, Fail);
    
    switch (event) {
	case kIrDiscoverRequestEvent:
	    DiscoverStart();
	    break;

	case kIrDiscoverReplyEvent:
	    HandleDiscoverComplete();
	    break;

	case kIrDisconnectReplyEvent:
//          HandleDisconnectComplete( nil );
	    break;
    }

Fail:
    return;
    
} // CIrDiscovery::NextState



//--------------------------------------------------------------------------------
//      DiscoverStart
//--------------------------------------------------------------------------------
void CIrDiscovery::DiscoverStart()
{
    TIrDiscoverRequest * request = (TIrDiscoverRequest *)this->GetCurrentEvent();
    IrDAErr err = noErr;
    
    XTRACE(kLogDiscoveryStart, request->fNumSlots, fState);
    
    require(request->fClient, Fail);
    
    if( request->fNumSlots > kMaxDiscoverSlots )
	err = errDiscoveryTooManySlots;                 // Range check the slot count
		
    if( err ) {                                         // Bounce it back to the client
	request->fResult    = err;                      
	request->fEvent     = kIrDiscoverReplyEvent;
	request->fClient->EnqueueEvent( request );
	return;
    }   
	
    if( request->fNumSlots == 0 )                   // Use the default
	request->fNumSlots = kDiscoverDefaultSlotCount;
	    
    // Can I do a discovery ?
    if( fState == kDiscoverActive ) {       // There's a pending discover.  Push on the list
	if( fPendingDiscoverList )          // Queue up the request
	    fPendingDiscoverList->InsertLast( request );    
	return;
    }
	
    // If there is a LAP connection then respond to client with the current
    // discovery info.
    
    if( fIrDA->IsLAPConnected() ) {
	// Remove every response except the connected device's
	request->fResult                = errDiscoveryInConnection;
	request->fDiscoveredDevices     = fDiscoveredDevices;
	request->fEvent =  kIrDiscoverReplyEvent;           // jdg, it's a reply event, not a request
	request->fClient->EnqueueEvent( request );
    }
    // Issue request to LMP to perform a XID discovery
    else {
	fState = kDiscoverActive;                   // Set flag we are in progress
	DeleteDiscoveredDevicesList();              // JDG: reset list of discovered devices
	request->fDiscoveredDevices = fDiscoveredDevices;
	
	// All done - until discover completes
	    
	// Note: fMyDscInfo fields initialized by caller (oh yeah???????)
	fIrDA->GetLMP()->EnqueueEvent( request );
    }
    return;
    
Fail:
    // free event?
    return;
} // CIrDiscovery::DiscoverStart

//--------------------------------------------------------------------------------
//      HandleDiscoverComplete
//--------------------------------------------------------------------------------
void CIrDiscovery::HandleDiscoverComplete()
{
    TIrDiscoverReply    *   discoverReply = (TIrDiscoverReply*)GetCurrentEvent();
    TIrDiscoverReply    *   pendingRequest;
    
    XTRACE( kLogDiscoverComplete, 0, this);
    check( fDiscoveredDevices == discoverReply->fDiscoveredDevices );

    // Complete discover request (let caller pick a device to connect to)
    fState = kDiscoverIdle;

    if( discoverReply->fClient == this )
	this->HandleExtDiscoverComplete( discoverReply );
    else
	discoverReply->fClient->EnqueueEvent( discoverReply );
	
    if( fPendingDiscoverList->Count() ) {       // Notify all pendingrequestors if necessary
	CListIterator *iter = CListIterator::cListIterator(fPendingDiscoverList);
	require(iter, Fail);
	for( pendingRequest = ( TIrDiscoverReply * )iter->FirstItem(); iter->More(); 
	     pendingRequest = ( TIrDiscoverReply * )iter->NextItem() ) {
	     
	     // Its a response now.
	     pendingRequest->fEvent = kIrDiscoverReplyEvent;
	     
	     // Copy the results from the reply to each pending reply
	     pendingRequest->fResult            = discoverReply->fResult;
	     pendingRequest->fNumSlots          = discoverReply->fNumSlots;
	     pendingRequest->fConflictDevAddr   = discoverReply->fConflictDevAddr;
	     pendingRequest->fDiscoveredDevices = discoverReply->fDiscoveredDevices;
	     pendingRequest->fPassiveDiscovery  = discoverReply->fPassiveDiscovery;

	    // return pending request
	    if( pendingRequest->fClient == this )
		this->HandleExtDiscoverComplete( pendingRequest );
	    else
		pendingRequest->fClient->EnqueueEvent( pendingRequest );
	}
	iter->release();
    }
    // This response if complete so clear the pending list
    fPendingDiscoverList->RemoveAll();
    return;
    
Fail:
    return;
	
} // CIrDiscovery::HandleDiscoverComplete


//--------------------------------------------------------------------------------
//      ExtDiscoverStart
//--------------------------------------------------------------------------------
IrDAErr CIrDiscovery::ExtDiscoverStart(     UInt32                          numSlots)
					//ExtDiscoveryUserCallBackUPP       callback,
					//ExtDiscoveryBlock         *   userData        )
{
    TIrExtDiscoverRequest *request;

    XTRACE(kLogExtDiscStart, 0, numSlots);
	
    request = (TIrExtDiscoverRequest *)fIrDA->GrabEventBlock(0, 0);
    require(request, Fail);
    request->fEvent     = kIrDiscoverRequestEvent;
    request->fResult    = noErr;
    
    if( numSlots > kMaxDiscoverSlots )
	request->fNumSlots = kMaxDiscoverSlots;         // Limit the max slot count
    else
	request->fNumSlots = numSlots;
	
    //userData->count = 0;                              // Set it to a safe value
	
    request->fClient    = this;                         // Fill in the fields and
    //request->fCallBack    = callback;                     // send it off to myself
    //request->fUserData    = userData;                     
    
    this->EnqueueEvent(request);
    
    return noErr;

Fail:
    return errNoMemory;
    
} // CIrDiscovery::ExtDiscoverStart
    

//--------------------------------------------------------------------------------
//      DeleteDiscoveredDevicesList
//--------------------------------------------------------------------------------
void CIrDiscovery::DeleteDiscoveredDevicesList()
{
    XTRACE(kLogDiscoverDeleteList, 0, 0);
    
    if (fDiscoveredDevices) {
	while (true) {
	    TIrDscInfo *dscInfo = (TIrDscInfo *)fDiscoveredDevices->Last();
	    if (dscInfo != nil) {
		dscInfo->release();
		fDiscoveredDevices->RemoveLast();
	    }
	    else {
		break;
	    }
	}
    }

} // CIrDiscovery::DeleteDiscoveredDevicesList

//--------------------------------------------------------------------------------
//      PassiveDiscovery
//--------------------------------------------------------------------------------
void CIrDiscovery::PassiveDiscovery(TIrDscInfo * dscInfo)
{
    if (fDiscoveredDevices) {
//
//  Limit the list size to the predetermined size (16).
//
	SInt16  discCount = fDiscoveredDevices->Count();
	if (discCount > kMaxDiscoverListSize) {         // Trim it back so the new one
							// will fit.
	    SInt16 deleteCount = discCount - kMaxDiscoverListSize;
	    
	    while (deleteCount--) {                     
		TIrDscInfo* dscInfo = (TIrDscInfo *)fDiscoveredDevices->First();
		if (dscInfo != nil) {
		    dscInfo->release();
		    fDiscoveredDevices->RemoveFirst();
		}
	    }
	}
//
// Now scan the list for a duplicate entry.  The LAP address is used for the test
//
	TIrDscInfo *oldInfo;
	
	discCount = fDiscoveredDevices->Count();
	for( SInt16 index = 0; index < discCount; index++ ) {
	    oldInfo = ( TIrDscInfo *)fDiscoveredDevices->At(index);
	    if (oldInfo && (oldInfo->GetDeviceAddr() == dscInfo->GetDeviceAddr())) {
		fDiscoveredDevices->RemoveAt(index);
		oldInfo->release();
		break;			// found it in list, quit searching
	    }
	}
	fDiscoveredDevices->InsertFirst( dscInfo );         // jdg, was insertlast ...
    }
}

//--------------------------------------------------------------------------------
//      GetRemoteDeviceName
//--------------------------------------------------------------------------------
void CIrDiscovery::GetRemoteDeviceName( UInt32 lapAddr, UInt8 * name, int maxnamelen )
{
    TIrDscInfo  *   oldInfo;
    SInt16          discCount = fDiscoveredDevices->Count();
    Boolean         found = false;
    
    name[0] = 0;
    
    for( SInt16 index = 0; index < discCount; index++ ) {
	oldInfo = ( TIrDscInfo * )fDiscoveredDevices->At( index );
	if( oldInfo->GetDeviceAddr() == lapAddr ) {
	    oldInfo->GetNickname( name, maxnamelen );
	    found = true;
	    break;
	}
    }
    if(!found)  {
	name[0] = 0;
    }
}
    

//--------------------------------------------------------------------------------
//      HandleExtDiscoverComplete
//--------------------------------------------------------------------------------
void CIrDiscovery::HandleExtDiscoverComplete( TIrDiscoverReply * reply )
{
    //TIrExtDiscoverReply           *   myReply     = ( TIrExtDiscoverReply * )reply;
    //ExtDiscoveryBlock         *   userResults = ( ExtDiscoveryBlock * )myReply->fUserData;
    //ExtDiscoveryUserCallBackUPP       callback    = ( ExtDiscoveryUserCallBackUPP )myReply->fCallBack;
    //TIrDscInfo                    *   dscInfo;
    
    //userResults->count = fDiscoveredDevices->GetArraySize();
    XTRACE(kIrExtDiscoverComplete, 0, fDiscoveredDevices->GetArraySize());      // Log the count.

    /*****
    if( userResults->count > kMaxDiscoverSlots )                // Check if the count is within
	userResults->count = 0;                                 // bounds.  If not then something
								// bad has happened.  Bag it.
    userResults->passiveDiscovery   = myReply->fPassiveDiscovery > 0;

    // Copy the results into the caller's block.
    if( userResults->count > 0 ) {
	for( UInt16 index = 0; index < userResults->count; index++ ) {
	
	    dscInfo = ( TIrDscInfo * )fDiscoveredDevices->At( index );
	    userResults->discoverResult[index].serviceHints         = dscInfo->GetServiceHints();
	    userResults->discoverResult[index].addr                 = dscInfo->GetDeviceAddr();
	    dscInfo->GetNickname( userResults->discoverResult[index].name );
	}
    }
    reply->fEvent = nil;                                        // Free the event
    
    CallExtDiscoveryCallBackProc( callback, myReply->fResult );
    ****/
    
    TIrEvent::ReleaseEventBlock(reply);         // release the event

} // CIrDiscovery::HandleMyDiscoverComplete

