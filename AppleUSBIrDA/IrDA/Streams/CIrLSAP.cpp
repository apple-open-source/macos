/*
    File:       CIrLSAP.c

    Contains:   IrDA client routines for TinyTP (and others)


*/


#include "CIrLSAP.h"
#include "IrIASClient.h"
#include "IrIASService.h"
#include "IrGlue.h"
#include "IrLSAPConn.h"
#include "CBufferSegment.h"
#include "IrDALog.h"

#if (hasTracing > 0 && hasIrLSAPTracing > 0)

enum IrLSAPTraceCodes
{
    kLogNew = 1,
    kLogFree,
    kLogInit,

    kUnexpectedEvent,
    
    kDiscoverStartEvent,
    kDiscoverCompleteEvent,
    kLogDiscoverCompleteErr,

    kLookupStartEvent,
    kNSClientConnectedEvent,
    kNSClientLookupReplyEvent,
    kNSClientLookupDisconnectEvent,
    kLookupCompleteEvent,
    
    kConnectStartEvent,
    kConnectCompleteEvent,

    kListenStartEvent,
    kListenCompleteEvent,
    kLogListenCompleteErr,
    
    kAcceptStartEvent,
    kAcceptCompleteEvent,

    kGetStartEvent,
    kGetCompleteEvent,
    
    kPutStartEvent,
    kPutCompleteEvent,

    kDisconnectStartEvent,
    kDisconnectCompleteEvent,
    
    kGetBackFromGrabEvent,
    kGetAboutToEnqueueEvent,
    kGetBackFromEnqueue,
    kCheckCBOk,
    kCheckCBBad,
    kLSAPEventProcess,
    
    kCancelPuts,
    kCancelGets,
    kCancelPutsComplete,
    kCancelGetsComplete,
    
    kEnqueueEvent,
    kDequeueEventStart,
    kDequeueEventEnd

};

static
EventTraceCauseDesc gTraceEvents[] = {
    {kLogNew,                       "CIrLSAP: new obj="},
    {kLogFree,                      "CIrLSAP: free obj="},
    {kLogInit,                      "CIrLSAP: init, obj=,lsapid="},

    {kUnexpectedEvent,              "CIrLSAP: unexpected event"},

    {kDiscoverStartEvent,           "CIrLSAP: Discover request"},
    {kDiscoverCompleteEvent,        "CIrLSAP: Discover reply"},
    {kLogDiscoverCompleteErr,       "CirLSAP: Discover reply ERROR"},

    {kLookupStartEvent,             "CIrLSAP: Lookup request"},
    {kNSClientConnectedEvent,       "CIrLSAP: Lookup IAS Client Connected"},
    {kNSClientLookupReplyEvent,     "CIrLSAP: Lookup IAS Client Reply"},
    {kNSClientLookupDisconnectEvent,"CIrLSAP: Lookup IAS Client Disconnected"},
    {kLookupCompleteEvent,          "CIrLSAP: Lookup complete"},

    {kConnectStartEvent,            "CIrLSAP: Connect request"},
    {kConnectCompleteEvent,         "CIrLSAP: Connect reply"},

    {kListenStartEvent,             "CIrLSAP: Listen request"},
    {kListenCompleteEvent,          "CIrLSAP: Listen reply"},
    {kLogListenCompleteErr,         "CIrLSAP: Listen reply ERROR"},
    
    {kAcceptStartEvent,             "CIrLSAP: Accept request"},
    {kAcceptCompleteEvent,          "CIrLSAP: Accept reply"},

    {kGetStartEvent,                "CIrLSAP: Get request"},
    {kGetCompleteEvent,             "CIrLSAP: Get reply"},
    {kPutStartEvent,                "CIrLSAP: Put request"},
    {kPutCompleteEvent,             "CIrLSAP: Put reply"},

    {kDisconnectStartEvent,         "CIrLSAP: Disconnect request"},
    {kDisconnectCompleteEvent,      "CIrLSAP: Disconnect complete"},

    {kGetBackFromGrabEvent,         "CIrLSAP: Get req back from grab event block"},
    {kGetAboutToEnqueueEvent,       "CIrLSAP: Get req about to enqueue event"},
    {kGetBackFromEnqueue,           "CIrLSAP: Get req back from enqueue event"},
    {kCheckCBOk,                    "CIrLSAP: check of CB looks ok"},
    {kCheckCBBad,                   "CIrLSAP: check of CB failed!"},
    {kLSAPEventProcess,             "CIrLSAP: Next State, event, state" },
    
    {kCancelPuts,                   "CIrLSAP: cancel pending puts"},
    {kCancelGets,                   "CIrLSAP: cancel pending gets"},
    {kCancelPutsComplete,           "CIrLSAP: cancel puts done"},
    {kCancelGetsComplete,           "CIrLSAP: cancel gets done"},
    
    {kEnqueueEvent,                 "CIrLSAP: Event Queued"},       // these 3 need to stay in order
    {kDequeueEventStart,            "CIrLSAP: Event Start"},
    {kDequeueEventEnd,              "CIrLSAP: Event End"}



};

#define XTRACE(x, y, z) IrDALogAdd( x, y, (uintptr_t)z & 0xffff, gTraceEvents, true )

#else
    #define XTRACE(x, y, z) ((void)0)
#endif


#define super TIrStream
    OSDefineMetaClassAndAbstractStructors(CIrLSAP, TIrStream);
    

void
CIrLSAP::free()
{
    TIASService  *nameService;      // glue's name service (if any)

    XTRACE(kLogFree, 0, this);
    
    if (fIrDA) {
	nameService = fIrDA->GetNameService();      // get existing name service
	if (nameService) {                          // if it exists, let's un-register outselves from IAS
	    nameService->RemoveAttribute(fClassName, fAttrName, kIASDeleteAttribute);
	}
    }
    
    if (fDscInfo)
	fDscInfo->RemoveServiceHints(fHints);
    
    if (fLSAP) {                // glue made this for us, but we need to release it
	fLSAP->release();
	fLSAP = nil;
    }
    else {                      // else lsapconn never alloc'd so WE need to release the lsap id
	if (fIrDA)                              // sanity check
	    fIrDA->ReleaseLSAPId(fMyLSAPId);    // cause lsapconn delete is what normally frees our lsap id
    }
	
    if (fNameClient) {                  // jdg: delete name client when we go away
	fNameClient->release();
	fNameClient = nil;
    }
    
    super::free();
}


Boolean CIrLSAP::Init(TIrGlue *irda, UInt32 desiredLSAPId, UInt8 * className, UInt8 * attributeName, ULong hints)
{   
    IrDAErr err;
	
    XTRACE(kLogInit, 0, this);
    XTRACE(kLogInit, 0, desiredLSAPId);

    fState          = kIrLSAPDisconnected;
    fPeerAddr       = 0;
    fMyLSAPId       = kAssignDynamicLSAPId;
    fPeerLSAPId     = 0;
    
    fClassName[0]   = 0;                        // ClassName is null string
    fAttrName[0]    = 0;                        // attr name to null string
    fConnectClassName[0] = 0;
    fAttributeName[0] = 0;                      // peer names
    fHints          = 0;

    fConnected      = false;

    fDiscoverCount = 0;
    fDscInfo        = nil;
    
    fNameClient     = nil;
    fLSAP           = nil;
    fDiscovery      = nil;
    fPendingDisconnect  = false;
    fDisconnectRequest = nil;       // jdg: disconnect request event we alloc'd
	
    
#if (hasTracing > 0 && hasIrLSAPTracing > 0)
    if (!super::Init(irda, gTraceEvents, kEnqueueEvent)) return false;
#else
    if (!super::Init(irda)) return false;
#endif
	
    fDiscovery = fIrDA->GetDiscovery();         // get the discovery stream client from glue
    require(fDiscovery, Fail);                  // this could be deferred until later, but ...
    
    fDscInfo = fDiscovery->GetDiscoveryInfo();  // get our own discovery info
    require(fDscInfo, Fail);
    
    fHints = hints;                             // remember our ("new") hint bits for clear later
    fDscInfo->SetServiceHints(fHints);          // add our hits to the system's hint bits
    
    fMyLSAPId = desiredLSAPId;
	
    if (className)
	strlcpy((char *)&fClassName[0], (char *)className, sizeof(fClassName));
    if (attributeName)
	strlcpy((char * )&fAttrName[0], (char *)attributeName, sizeof(fAttrName));

    
    // FIXME - this should only be done if we set up a listener
    err = fIrDA->RegisterMyNameAndLSAPId(fClassName, fAttrName, &fMyLSAPId);
    nrequire(err, Fail);
    
    XTRACE(kLogInit, 1, fMyLSAPId);
    
    return true;
    
Fail:
    return false;

} // CIrLSAP::InitCIrLSAP

#pragma mark Client Accessors
//--------------- Client Accessors ---------------

void CIrLSAP::SetPeerLAPAddr( UInt32 addr )
{
    if( ! fConnected )          // This only makes sense before a connection is
	fPeerAddr = addr;       // established.
}


#pragma mark Client Methods
//--------------- Client Methods ---------------

//--------------------------------------------------------------------------------------------
//
//  Discover:   Initiates a LAP XID Discovery
//
//--------------------------------------------------------------------------------------------
IrDAErr CIrLSAP::Discover( UInt32 slots )
{
    TIrDiscoverRequest * request;
    
    XTRACE(kDiscoverStartEvent, 0, this);
    
    if (GetState() != kIrLSAPDisconnected) {    // sanity check
	XTRACE(kDiscoverStartEvent, 0xffff, GetState());
	return kIrDAErrWrongState;                  // bail if invalid state
    }
    
    request = (TIrDiscoverRequest *)fIrDA->GrabEventBlock(kIrDiscoverRequestEvent, sizeof(TIrDiscoverRequest));
    require( request, Fail_NewDiscoverRequest );
    
    request->fNumSlots  = slots;
    request->fClient    = this;
    
    SetState( kIrLSAPDiscoverStart);
    fDiscovery->EnqueueEvent( request );
	
    return noErr;
    
Fail_NewDiscoverRequest:
    return kIrDAErrNoMemory;
    
} // CIrLSAP::Discover

//--------------------------------------------------------------------------------------------
//
//  LSAPLookup: Queries remote device for an LSAP id of requested service
//
//--------------------------------------------------------------------------------------------

IrDAErr CIrLSAP::LSAPLookup(UInt8 * className, UInt8 * attributeName, UInt32 remoteAddr)
{   
    
    TIrConnLstnRequest  *   connectRequest;

    XTRACE( kLookupStartEvent, remoteAddr >> 16, remoteAddr );

    if (GetState() != kIrLSAPDisconnected) {    // sanity check
	XTRACE(kLookupStartEvent, 0xffff, GetState());
	return kIrDAErrWrongState;                  // bail if invalid state
    }

    fPeerAddr = remoteAddr;         // FIXME Client should supply address with connect request
    
    // Save connect class name until we have connected to the peer devs name server
    strlcpy( ( char * )&fConnectClassName, ( const char * )className, sizeof(fConnectClassName));
    strlcpy( ( char * )&fAttributeName, ( const char * )attributeName, sizeof(fAttributeName));

    // Create, init name server client
    if (fNameClient == nil) {                       // jdg: if first time we've done a lookup
	fNameClient = TIASClient::tIASClient(fIrDA, this);  // make an IAS name client to use
	require( fNameClient, Fail_NewNameClient );
    }

    connectRequest = (TIrConnLstnRequest*)fIrDA->GrabEventBlock(kIrConnectRequestEvent, sizeof(TIrConnLstnRequest));
    require( connectRequest, Fail_NewConnectRequest );
    
    // Connect to name server on peer device
    connectRequest->fDevAddr    = remoteAddr;
    connectRequest->fMyQOS      = fIrDA->GetMyQOS();
    connectRequest->fPeerQOS    = fIrDA->GetPeerQOS();
    connectRequest->fData       = nil;
    connectRequest->fClient     = this;

    SetState( kIrLSAPLookupStart );                     // now doing a lookup
    fNameClient->EnqueueEvent(connectRequest);
    return noErr;

    // Out of memory error exit points

Fail_NewConnectRequest:
Fail_NewNameClient:
    return kIrDAErrNoMemory;
} // CIrLSAP::LSAPLookup


//--------------------------------------------------------------------------------------------
//
//  Connect:    Attempts to connect to remote device 
//
//--------------------------------------------------------------------------------------------

IrDAErr CIrLSAP::Connect( UInt32 remoteAddr, UInt32 lsapID, CBufferSegment *connectData )
{
    fPeerAddr = remoteAddr;
    return this->Connect(lsapID, connectData);
}

IrDAErr CIrLSAP::Connect( UInt32 lsapID, CBufferSegment *connectData )
{
    fPeerLSAPId = lsapID;               // Punch in the user's lsap id and then connect
    return this->Connect(connectData);
}

IrDAErr CIrLSAP::Connect( CBufferSegment *connectData )
{
    IrDAErr err;
    XTRACE( kConnectStartEvent, fPeerLSAPId, fPeerAddr );
    XTRACE( kConnectStartEvent, 0, this );
    
    if (GetState() != kIrLSAPDisconnected) {    // sanity check
	XTRACE(kConnectStartEvent, 0xffff, GetState());
	return kIrDAErrWrongState;                  // bail if invalid state
    }
    
    SetState(kIrLSAPConnectStart);
    // TODO --- allocate the lsapconn in our init and bypass glue here
    err = fIrDA->ConnectStart(  this, fMyLSAPId, fPeerAddr, fPeerLSAPId,
				    connectData, &fLSAP );
				    
    if (err != noErr) {                     // if connect start failed
	SetState(kIrLSAPDisconnected);      // not connecting, we're disconnected
    }
    return err;
    
} // CIrLSAP::Connect



//--------------------------------------------------------------------------------------------
//
//  DataPut:    Writes one packet of data.
//
//--------------------------------------------------------------------------------------------

IrDAErr CIrLSAP::DataPut( CBufferSegment * putBuffer )  
{
    TIrPutRequest *putRequest;
    XTRACE( kPutStartEvent, fMyLSAPId, fPeerLSAPId );
    XTRACE( kPutStartEvent, 0, this );
    XTRACE( kPutStartEvent, 0, putBuffer);
    
    if (GetState() != kIrLSAPConnected) {
	XTRACE(kPutStartEvent, 0xffff, GetState());
	return kIrDAErrWrongState;                  // bail if invalid state
    }
    check(fConnected == true);  // checks should all pass since we're connected
    check(putBuffer);
    check(fLSAP);
    
    fLastPutBuffer = putBuffer;         // temp debugging
    putRequest = ( TIrPutRequest * )fIrDA->GrabEventBlock(kIrPutDataRequestEvent, sizeof(TIrPutRequest));
    require( putRequest, AllocatePutRequestBlock );
    
    putRequest->fData   = putBuffer;
    putRequest->fOffset = 0;
    putRequest->fLength = putBuffer->GetSize();
    putRequest->fClient = this;
    fLSAP->EnqueueEvent( putRequest );
    return noErr;

AllocatePutRequestBlock:

    return kIrDAErrNoMemory;
} // CIrLSAP::DataPut



//--------------------------------------------------------------------------------------------
//
//  DataGet:    Reads one packet of data.
//
//--------------------------------------------------------------------------------------------


IrDAErr CIrLSAP::DataGet( CBufferSegment * getBuffer )  
{
    TIrGetRequest *getRequest;
    XTRACE( kGetStartEvent, fMyLSAPId, fPeerLSAPId );
    XTRACE( kGetStartEvent, 0, this );

    if (GetState() != kIrLSAPConnected) {
	XTRACE(kGetStartEvent, 0xffff, GetState());
	return kIrDAErrWrongState;                  // bail if invalid state
    }
    check(getBuffer);
    
    getRequest = ( TIrGetRequest * )fIrDA->GrabEventBlock(kIrGetDataRequestEvent, sizeof(TIrGetRequest));   
    require( getRequest, AllocatePutRequestBlock );
	
    getRequest->fData   = getBuffer;
    getRequest->fOffset = 0;
    getRequest->fLength = getBuffer->GetSize();
    getRequest->fClient = this;
	
    fLSAP->EnqueueEvent( getRequest );
    
    return noErr;

AllocatePutRequestBlock:

    return kIrDAErrNoMemory;
} // CIrLSAP::DataGet



//--------------------------------------------------------------------------------------------
//
//  Listen: Waits for a connection attempt from a remote device
//
//--------------------------------------------------------------------------------------------

IrDAErr CIrLSAP::Listen(CBufferSegment *connectData)
{
    IrDAErr err;
    
    XTRACE( kListenStartEvent, 0, fMyLSAPId);
    XTRACE( kListenStartEvent, 0, this);
    XTRACE( kListenStartEvent, 0, connectData);

    if (GetState() != kIrLSAPDisconnected) {    // sanity check
	XTRACE(kListenStartEvent, 0xffff, GetState());
	return kIrDAErrWrongState;                  // bail if invalid state
    }
    
    fLastListenBuffer = connectData;        // temp debugging
    SetState( kIrLSAPListenStart );
    // TODO - get rid of glue
    err = fIrDA->ListenStart( this, fMyLSAPId, connectData, &fLSAP );   
    if (err != noErr) {                     // if listen start failed
	SetState(kIrLSAPDisconnected);      // not listening, we're disconnected
    }
    return err;
    
} // CIrLSAP::Listen



//--------------------------------------------------------------------------------------------
//
//  Accept: Accepts the remote devices connection attempt (or not)
//
//--------------------------------------------------------------------------------------------

IrDAErr CIrLSAP::Accept(CBufferSegment *connectData)
{
    TIrConnLstnRequest  *acceptRequest;

    XTRACE( kAcceptStartEvent, fMyLSAPId, fPeerLSAPId);
    XTRACE( kAcceptStartEvent, fPeerAddr>>16, fPeerAddr );
    XTRACE( kAcceptStartEvent, (uintptr_t)this>>16, this);

    if (GetState() != kIrLSAPListenComplete) {  // sanity check
	XTRACE(kAcceptStartEvent, 0xffff, GetState());
	return kIrDAErrWrongState;                  // bail if invalid state
    }
    
    acceptRequest = ( TIrConnLstnRequest*)  fIrDA->GrabEventBlock(  kIrAcceptRequestEvent,
							    sizeof( TIrConnLstnRequest ) );
    require( acceptRequest, Fail_NewAcceptRequest );

    SetState( kIrLSAPAcceptStart );
    acceptRequest->fData = connectData;
    fLSAP->EnqueueEvent( acceptRequest );

    return noErr;
    
Fail_NewAcceptRequest:
    //Disconnect();         // jdg: if out of events, disconnect request will fail too
    return kIrDAErrNoMemory;
    
} // CIrLSAP::Accept



//--------------------------------------------------------------------------------------------
//
//  DoDisconnect:   Disconnects the LSAP from remote
//
//--------------------------------------------------------------------------------------------

void CIrLSAP::Disconnect()
{
    //TIrDisconnectRequest *disconnectRequest;
    
    XTRACE( kDisconnectStartEvent, fMyLSAPId, fPeerLSAPId );
    XTRACE( kDisconnectStartEvent, 0, this );
    
    if (GetState() == kIrLSAPDisconnected) {        // if we're already disconnected
	check(fConnected == false);                 // sanity check
	XTRACE(kDisconnectStartEvent, 0xffff, GetState());
	DisconnectComplete();                       // !! virtual callback to client (hmm)
	return;                         // !! could do away w/above cb if we returned an err
    }
    
    // JDG: if we're in the middle of a discover or lookup, wait until they finish
    // before doing a disconnect
    if (GetState() == kIrLSAPDiscoverStart || GetState() == kIrLSAPLookupStart) {
	fPendingDisconnect = true;
	return;
    }
    
    // and if we've already issued a disconnect request on this, another
    // one is just a nop
    if (GetState() == kIrLSAPDisconnectStart)
	return;                         // should return an error here
    
    check(fDisconnectRequest == nil);   // shouldn't have a disconnect pending yet
    fDisconnectRequest = (TIrDisconnectRequest*) fIrDA->GrabEventBlock(kIrDisconnectRequestEvent,
								sizeof(TIrDisconnectRequest));
    if (fDisconnectRequest == nil)      // out of memory
	return;                         // should return an error code

    SetState( kIrLSAPDisconnectStart );
    fLSAP->EnqueueEvent(fDisconnectRequest);
    return;
    
} // CIrLSAP::Disconnect

//--------------------------------------------------------------------------------------------
//
//  CancelPuts: Aborts all pending puts.
//
//--------------------------------------------------------------------------------------------

IrDAErr CIrLSAP::CancelPuts( void ) 
{
    TIrCancelPutEvent *cancelRequest;
    XTRACE( kCancelPuts, 0, this);

    if (GetState() != kIrLSAPConnected) {   // if we're not connected
	XTRACE(kCancelPuts, 0xffff, GetState());
	return kIrDAErrWrongState;                  // bail if invalid state
    }
    
    cancelRequest = (TIrCancelPutEvent *)fIrDA->GrabEventBlock(kIrCancelPutRequestEvent,
							    sizeof(TIrCancelPutEvent));
    require(cancelRequest, AllocatePutRequestBlock);
    
    cancelRequest->fClient = this;
    fLSAP->EnqueueEvent(cancelRequest);

    return noErr;

AllocatePutRequestBlock:

    return kIrDAErrNoMemory;
    
}   // CIrLSAP::CancelPuts

//--------------------------------------------------------------------------------------------
//
//  CancelGets: Aborts all pending gets.
//
//--------------------------------------------------------------------------------------------

IrDAErr CIrLSAP::CancelGets( void ) 
{
    TIrCancelGetEvent *cancelRequest;
    
    XTRACE( kCancelGets, 0, this);

    if (GetState() != kIrLSAPConnected) {       // if we're not connected
	XTRACE(kCancelGets, 0xffff, GetState());
	return kIrDAErrWrongState;                  // bail if invalid state
    }
    
    cancelRequest = (TIrCancelGetEvent *)fIrDA->GrabEventBlock(kIrCancelGetRequestEvent,
							    sizeof(TIrCancelGetEvent));
    require(cancelRequest, AllocateRequestBlock);
    
    cancelRequest->fClient = this;
    fLSAP->EnqueueEvent(cancelRequest);

    return noErr;

AllocateRequestBlock:

    return kIrDAErrNoMemory;
    
}   // CIrLSAP::CancelGets


#pragma mark
//--------------- Internal Event Handlers ---------------

//--------------------------------------------------------------------------------------------
//
//      NextState
//
//--------------------------------------------------------------------------------------------
void CIrLSAP::NextState( UInt32 event )
{
    XTRACE( kLSAPEventProcess, ( UInt16 )event, GetState() );
    XTRACE( kLSAPEventProcess, 0, this);

    // The only overlap of state/events is ConnectReply, when the connect
    // could be either at our client's request or as part of a client
    // ias lookup sequence
    
    if (event == kIrConnectReplyEvent && GetState() == kIrLSAPLookupStart) {
	HandleNameServerConnectComplete();
	return;
    }

    switch (event) {
    
	case kIrDisconnectReplyEvent:
	    HandleDisconnectComplete();
	    break;
	    
	case kIrDiscoverReplyEvent:
	    HandleDiscoverComplete();
	    break;

	case kIrLookupReplyEvent:
	    HandleNameServerLookupComplete();
	    break;

	case kIrReleaseReplyEvent:
	    HandleNameServerReleaseComplete();
	    break;
		
	case kIrConnectReplyEvent:
	    HandleConnectComplete();
	    break;

	case kIrListenReplyEvent:
	    HandleListenComplete();
	    break;

	case kIrAcceptReplyEvent:
	    HandleAcceptComplete();
	    break;

	case kIrGetDataReplyEvent:
	    HandleDataGetComplete();
	    break;

	case kIrPutDataReplyEvent:
	    HandleDataPutComplete();
	    break;

	case kIrCancelGetReplyEvent:
	    HandleCancelGetComplete();
	    break;
	    
	case kIrCancelPutReplyEvent:
	    HandleCancelPutComplete();
	    break;
	
	default:
	    DebugLog("CIrLSAP::NextState: unexpected event");
	    break;
    }   // switch on event
    
} // CIrLSAP::NextState


	    
//--------------------------------------------------------------------------------
//
//      DiscoverComplete
//
//--------------------------------------------------------------------------------
void CIrLSAP::HandleDiscoverComplete()
{
    TIrDiscoverReply * reply = ( TIrDiscoverReply * )GetCurrentEvent();

    UInt32                  numFound;           // number of peers discovered
    TIrDscInfo          *   dscInfo;
    CList               *   fDiscoverList;
    
    if (reply->fResult == noErr) {          // if it worked
	check (GetState() == kIrLSAPDiscoverStart);
	if (GetState() != kIrLSAPDiscoverStart) {
	    XTRACE(kLogDiscoverCompleteErr, reply->fResult, GetState());
	}
    } else {                                // else it could have async disconnected (?)
	check (GetState() == kIrLSAPDiscoverStart ||
	       GetState() == kIrLSAPDisconnected);
	if (GetState() != kIrLSAPDiscoverStart && GetState() != kIrLSAPDisconnected)
	    XTRACE(kLogDiscoverCompleteErr, reply->fResult, GetState());
    }
    
    fDiscoverList = reply->fDiscoveredDevices;
    
    numFound = fDiscoverList->GetArraySize();
    XTRACE( kDiscoverCompleteEvent, numFound, reply->fResult );
    if( numFound > 0 ) {
	SInt32 index;
	for( index = 0; index < fDiscoverList->GetArraySize(); index++ ) {
	
	    dscInfo = ( TIrDscInfo * )fDiscoverList->At( index );
	    fDiscoverInfo[index].serviceHints   = dscInfo->GetServiceHints();
	    fDiscoverInfo[index].addr           = dscInfo->GetDeviceAddr();
	    dscInfo->GetNickname( fDiscoverInfo[index].name, sizeof(fDiscoverInfo[index].name) );
	}
    }
    SetState( kIrLSAPDisconnected );        // discover done, disconnected again
    DiscoverComplete( numFound, reply->fResult );       // virtual dispatch
    fIrDA->ReleaseEventBlock( reply );
    
    // If client asked for a disconnect while we were discovering, fake
    // a disconnect complete now
    if (fPendingDisconnect) {
	XTRACE( kDiscoverCompleteEvent, 0xffff, 0xffff);
	fPendingDisconnect = false;
	DisconnectComplete();           // virtual callback to client
    }
    
} // HandleDiscoverComplete


//--------------------------------------------------------------------------------
//
//      DisconnectComplete
//
//--------------------------------------------------------------------------------
void CIrLSAP::HandleDisconnectComplete()
{
    TIrDisconnectReply * reply = ( TIrDisconnectReply * )GetCurrentEvent();
    
    //UInt32 enterState = GetState();               // Remember what the state was

    XTRACE( kDisconnectCompleteEvent, reply->fResult, GetState() );
    fConnected = false;
    SetState( kIrLSAPDisconnected );
    DisconnectComplete();                   // virtual.  cb to client
    
    // If someone else generated the disconnect then I don't want to release
    // the event block.  LMP will do it for me.  Most likely, the beam was broken
    // so LMP is notifing all the LSAPs of the disconnect.  It will release the
    // event when it is done.
    //if( enterState == kIrLSAPDisconnectStart )
    //  fIrDA->ReleaseEventBlock( reply );
    
    // JDG: if reply matches that of our disconnect request, free it.
    // note: disconnect start state isn't particularly reliable ...
    if (fDisconnectRequest == reply) {
	fIrDA->ReleaseEventBlock( reply );
	fDisconnectRequest = nil;
    }
	
} // CIrLSAP::HandleDisconnectComplete


#pragma mark ----- LSAP Lookup Engine ---
//--------------------------------------------------------------------------------
//
//      LSAPLookupComplete
//
//--------------------------------------------------------------------------------
void CIrLSAP::HandleLSAPLookupComplete()
{
    TIrLookupEvent  *reply = (TIrLookupEvent*)GetCurrentEvent();
    IrDAErr result = reply->fResult;
    XTRACE(kLookupCompleteEvent, result, fPeerLSAPId );
    
    if (result == noErr) {          // if it worked
	check(GetState() == kIrLSAPLookupStart);
    } else {                        // if it failed, we could have async disconnected
	check(GetState() == kIrLSAPLookupStart 
	      ||  GetState() == kIrLSAPDisconnected);
    }
    
    if (result == noErr) {
	fPeerLSAPId = reply->fPeerLSAPId;       // assume we want to talk to 'em!
    }
    else {
	fPeerLSAPId = 0;
    }
    SetState(kIrLSAPDisconnected);          // we're not connecting yet ...
    
    LSAPLookupComplete(result, fPeerLSAPId);        // virtual call back
    fIrDA->ReleaseEventBlock( reply );

    // If client asked for a disconnect while we were discovering, fake
    // a disconnect complete now
    if (fPendingDisconnect) {
	XTRACE(kLookupCompleteEvent, 0xffff, 0xffff);
	fPendingDisconnect = false;
	DisconnectComplete();           // virtual callback to client
    }
} // CIrLSAP::HandleLSAPLookupComplete


//--------------------------------------------------------------------------------
//      HandleNameServerConnectComplete
//--------------------------------------------------------------------------------
void CIrLSAP::HandleNameServerConnectComplete()
{
    TIrConnLstnReply    * connectReply  = ( TIrConnLstnReply * )GetCurrentEvent();
    TIrLookupRequest    * lookupRequest = ( TIrLookupRequest * )GetCurrentEvent();

    XTRACE( kNSClientConnectedEvent, 0, connectReply->fResult );
    check(GetState() == kIrLSAPLookupStart);

    // Complete request early if an error is returned
    if (connectReply->fResult != noErr) {
	this->HandleLSAPLookupComplete();       // virtual dispatch to client
    }
    else {
	// Issue a lookup to name server on peer device
	// Note: re-using the connect request block
	lookupRequest->fEvent       = kIrLookupRequestEvent;
	lookupRequest->fClassName   = fConnectClassName;        // Client supplied
	lookupRequest->fAttrName    = fAttributeName;           // Client supplied
	fNameClient->EnqueueEvent(lookupRequest);
    }

} // TIrGlue::HandleNameServerConnectComplete


//--------------------------------------------------------------------------------
//      HandleNameServerLookupComplete
//--------------------------------------------------------------------------------
void CIrLSAP::HandleNameServerLookupComplete()
{
    TIrLookupReply          * lookupReply = ( TIrLookupReply * )GetCurrentEvent();
    check(GetState() == kIrLSAPLookupStart);

    fPeerLSAPId = 0;

    if (lookupReply->fResult == noErr) {
	// Did we successfully get the LSAPId of the remote connection end?
	if( lookupReply->fAttribute ) {
	    TIASElement * element = ( TIASElement * )lookupReply->fAttribute->First();
	    if( element ) {
		if( element->GetInteger( &fPeerLSAPId ) != noErr ) {
		    fPeerLSAPId = 0;
		}
	    }
	    //delete lookupReply->fAttribute;
	    lookupReply->fAttribute->release();
	}
    }

    XTRACE( kNSClientLookupReplyEvent, fPeerLSAPId, lookupReply->fResult );

    TIrDisconnectRequest    * releaseRequest;
    // Release the name server connection (re-use the reply event block)
    releaseRequest = (TIrDisconnectRequest*)lookupReply;
    releaseRequest->fEvent = kIrReleaseRequestEvent;
    releaseRequest->fResult = noErr;
    fNameClient->EnqueueEvent(releaseRequest);
} // CIrLSAP::HandleNameServerLookupComplete


//--------------------------------------------------------------------------------
//      HandleNameServerReleaseComplete
//--------------------------------------------------------------------------------
void CIrLSAP::HandleNameServerReleaseComplete()
{
    TIrLookupReply * lookupReply = ( TIrLookupReply * )GetCurrentEvent();

    XTRACE( kNSClientLookupDisconnectEvent, 0, fPeerLSAPId );
    check(GetState() == kIrLSAPLookupStart);
    
    lookupReply->fEvent         = kIrLookupReplyEvent;  // Reply back to the client
    lookupReply->fPeerLSAPId    = fPeerLSAPId;
    this->HandleLSAPLookupComplete();
    
} // CIrLSAP::HandleNameServerReleaseComplete

    

#pragma mark ----- Data Handlers ---

//--------------------------------------------------------------------------------
//
//      ConnectComplete
//
//--------------------------------------------------------------------------------
void CIrLSAP::HandleConnectComplete()
{
    TIrConnLstnReply *reply = (TIrConnLstnReply *)GetCurrentEvent();
    IrDAErr result = reply->fResult;

    XTRACE( kConnectCompleteEvent, result, 0 );
    
    check( fLSAP == reply->fLSAPConn );

    //if (result == noErr) {            // if it worked
    //  check(GetState() == kIrLSAPConnectStart);       // this was failing ... sigh
    //} else {                      // if aborted, could be in a disconnect state too
	check(GetState() == kIrLSAPConnectStart ||
	      GetState() == kIrLSAPDisconnectStart ||
	      GetState() == kIrLSAPDisconnected);
    //}

    if( result == noErr ) {
	SetState( kIrLSAPConnected );
	fConnected = true;
    }
    else{
	SetState( kIrLSAPDisconnected );
	fConnected = false;
    }
    
    // "Bug" workaround.  Fixme?  We'd like the QOS values, but they're
    // not getting set by the lower layers.  Grab 'em from glue and 
    // stuff 'em in.
    reply->fPeerQOS = fIrDA->GetPeerQOS();
    reply->fMyQOS   = fIrDA->GetMyQOS();
    
    ConnectComplete(result, reply->fMyQOS, reply->fPeerQOS, (CBufferSegment *)reply->fData);
    fIrDA->ReleaseEventBlock( reply );
    
} // CIrLSAP::HandleConnectComplete



//--------------------------------------------------------------------------------
//
//      DataPutComplete
//
//--------------------------------------------------------------------------------
void CIrLSAP::HandleDataPutComplete()
{
    TIrPutReply * reply = ( TIrPutReply * )GetCurrentEvent();
    IrDAErr result = reply->fResult;
    
    XTRACE( kPutCompleteEvent, result, 0 ); 
    
    //if (fLastListenBuffer == reply->fData)    // this is how it was failing
    //  DebugPrintf("put buf done, %x %x %x", fLastListenBuffer, fLastPutBuffer, reply->fData);
    
    DataPutComplete(result, (CBufferSegment *)reply->fData);    // virtual callback
    fIrDA->ReleaseEventBlock( reply );
    
} // CIrLSAP::HandleDataPutComplete


//--------------------------------------------------------------------------------
//
//      ListenComplete
//
//--------------------------------------------------------------------------------
void CIrLSAP::HandleListenComplete()
{
    TIrConnLstnReply    *reply = (TIrConnLstnReply*)GetCurrentEvent();
    IrDAErr result = reply->fResult;

    if (result == noErr) {              // if it worked, should be in listen start state
	check(GetState() == kIrLSAPListenStart);
    } else {                            // else probably aborted by disconnect
	// FIXME -- ReUdo is hitting this check ... let's debugstr to see what's up
	// todo: we're getting here as a reject from discover start ... why?
	/*****/
#if defined(forDebug)
	int state = GetState();
	if (state != kIrLSAPListenStart &&
	    state != kIrLSAPDisconnectStart && state != kIrLSAPDisconnected)
	    DebugPrintf("CIrLSAP: listen complete %d, state %d", result, state);
	//****/
#endif  // forDebug
	//check(GetState() == kIrLSAPListenStart ||
	//    GetState() == kIrLSAPDisconnectStart ||
	//    GetState() == kIrLSAPDisconnected);
    }

    if (result == noErr) {              // if it worked, pull out peer info
	fPeerLSAPId = reply->fLSAPId;
	fPeerAddr   = reply->fDevAddr;
    }
    
    XTRACE( kListenCompleteEvent, fPeerLSAPId, fPeerAddr );
    XTRACE( kListenCompleteEvent, 0, this);
    
    // "Bug" workaround.  Fixme?  We'd like the QOS values, but they're
    // not getting set by the lower layers.  Grab 'em from glue and 
    // stuff 'em in.
    // UPDATE: this has almost been fixed but not verified.  Check out lsapconn's
    // clobbering of the listen request event record when reused as a GetRequest
    reply->fPeerQOS = fIrDA->GetPeerQOS();
    reply->fMyQOS   = fIrDA->GetMyQOS();
    
    if (result == noErr)
	SetState( kIrLSAPListenComplete );
    else
	SetState(kIrLSAPDisconnected);
    
    check(fLastListenBuffer == reply->fData);
    if (fLastListenBuffer != reply->fData) {
	XTRACE(kLogListenCompleteErr, 0, fLastListenBuffer);
	XTRACE(kLogListenCompleteErr, 0,      reply->fData);
    }
    
    ListenComplete( result, fPeerAddr, fPeerLSAPId, reply->fMyQOS, reply->fPeerQOS, (CBufferSegment *)reply->fData);
    fIrDA->ReleaseEventBlock( reply );
    
} // CIrLSAP::HandleListenComplete


//--------------------------------------------------------------------------------
//
//      AcceptComplete
//
//--------------------------------------------------------------------------------
void CIrLSAP::HandleAcceptComplete()
{
    TIrConnLstnReply *  reply = (TIrConnLstnReply*)GetCurrentEvent();
    IrDAErr         result = reply->fResult;

    XTRACE( kAcceptCompleteEvent, result, 0 );
    check(GetState() == kIrLSAPAcceptStart);

    if( result == noErr ) {
	SetState( kIrLSAPConnected );
	fConnected = true;
    }
    else {
	SetState( kIrLSAPDisconnected );
	fConnected = false;
    }
	
    AcceptComplete(result, (CBufferSegment *)reply->fData);
    fIrDA->ReleaseEventBlock( reply );
    
} // CIrLSAP::HandleAcceptComplete


//--------------------------------------------------------------------------------
//
//      DataGetComplete
//
//--------------------------------------------------------------------------------
void CIrLSAP::HandleDataGetComplete()
{
    TIrGetReply * reply = ( TIrGetReply * )GetCurrentEvent();
    IrDAErr     result = reply->fResult;
    
    XTRACE( kGetCompleteEvent, result, 0 );
	
    DataGetComplete(result, (CBufferSegment *)reply->fData);
    fIrDA->ReleaseEventBlock( reply );
    
} // CIrLSAP::HandleDataGetComplete

//--------------------------------------------------------------------------------
//
//      CancelPutComplete
//
//--------------------------------------------------------------------------------
void CIrLSAP::HandleCancelPutComplete()
{
    TIrCancelPutReply * reply = ( TIrCancelPutReply * )GetCurrentEvent();
    IrDAErr result = reply->fResult;
    
    XTRACE( kCancelPutsComplete, 0, 0 );    
    
    CancelPutsComplete(result);             // virtual callback
    fIrDA->ReleaseEventBlock( reply );
} // CIrLSAP::HandleCancelPutComplete

//--------------------------------------------------------------------------------
//
//      CancelGetComplete
//
//--------------------------------------------------------------------------------
void CIrLSAP::HandleCancelGetComplete()
{
    TIrCancelGetReply * reply = ( TIrCancelGetReply * )GetCurrentEvent();
    IrDAErr result = reply->fResult;
    
    XTRACE( kCancelGetsComplete, 0, 0 );    
    
    CancelGetsComplete(result);             // virtual callback
    fIrDA->ReleaseEventBlock( reply );
    
} // CIrLSAP::HandleCancelGetComplete

