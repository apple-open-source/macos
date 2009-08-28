/*
    File:       IrGlue.cpp

    Contains:   Implementation of clients to IrDA glue


*/

#include "IrGlue.h"
#include "IrLAP.h"
#include "IrLMP.h"
#include "IrLAPConn.h"
#include "IrEvent.h"
#include "CTimer.h"
#include "IrDiscovery.h"
#include "CIrDevice.h"
#include "IrQOS.h"
#include "IrDALog.h"
#include "IrStream.h"
#include "IrIASService.h"
#include "IrIASServer.h"
#include "IrLSAPConn.h"
#include "IrDAUserClient.h"

#include "AppleIrDA.h"

#define private static

#if (hasTracing > 0 && hasIrGlueTracing > 0)

enum IrGlueTraceCodes
{
    kLogNew = 1,
    kLogInit,
    kLogFree,
    
    kLogReadComplete,
    kLogTransmitComplete,
    kLogSetSpeedComplete,
    kLogRunQueue,

    kObtainLSAPIdEvent,
    kReleaseLSAPIdEvent,

    kConnectStartEvent,
    kListenStartEvent,

    kAllocateEventBlock,
    kGrabEventBlock,
    kReleaseEventBlock,
    kDeleteEventBlock,

    kStartTerminateEntry,
    kStartTerminateExit,
    
    kLogStartTimer,
    kLogStopTimer,
    kLogTimerNotifier,
    kLogTimerNotifier1,
    kLogTimerNotifier2,
    kLogTimerNotifierDone,
    kLogTimerComplete
};

private
EventTraceCauseDesc IrGlueTraceEvents[] = {
    {kLogNew,                       "irglue: Create, obj="},
    {kLogInit,                      "irglue: Init"},
    {kLogFree,                      "irglue: Free"},
    
    {kLogReadComplete,              "irglue: read complete"},
    {kLogTransmitComplete,          "irglue: transmit complete"},
    {kLogSetSpeedComplete,          "irglue: set speed complete"},
    {kLogRunQueue,                  "irglue: run event queue"},

    {kObtainLSAPIdEvent,            "irglue: Obtain LSAP Id"},
    {kReleaseLSAPIdEvent,           "irglue: Release LSAP Id"},

    {kConnectStartEvent,            "irglue: Connect request, myid, peerid, deviceaddr"},
    {kListenStartEvent,             "irglue: Listen request"},

    {kAllocateEventBlock,           "irglue: Allocate Event Block"},
    {kGrabEventBlock,               "irglue: Grab Event Block"},
    {kReleaseEventBlock,            "irglue: Release Event Block"},
    {kDeleteEventBlock,             "irglue: Delete Event Block"},

    {kStartTerminateEntry,          "irglue: Disconnected - start"},
    {kStartTerminateExit,           "irglue: Disconnected - done"},
    
    {kLogStartTimer,                "irglue: Start timer delay, #"},
    {kLogStopTimer,                 "irglue: Stop timer #"},
    {kLogTimerNotifier,             "irglue: timer notifier, id="},
    {kLogTimerNotifier1,                "irglue: timer notifier, owner="},
    {kLogTimerNotifier2,                "irglue: timer notifier, timesrc="},
    {kLogTimerNotifierDone,         "irglue: timer notifier finished"},
    {kLogTimerComplete,             "irglue: timer complete, signature="}
    
};

#define XTRACE(x, y, z) IrDALogAdd (x, y, (uintptr_t)z & 0xffff, IrGlueTraceEvents, true )
#else
    #define XTRACE(x, y, z) ((void)0)
#endif

static void TimerNotifier(OSObject *owner, IrDATimerEventSource *sender);

//
// Todo: move this to a more public spot if they are needed elsewhere
//
const UInt8 kIASDeviceClassStr[]        = "Device";
const UInt8 kIASDeviceNameAttrStr[]     = "DeviceName";
const UInt8 kIASDeviceName[]            = "Macintosh";
const UInt8 kIASLMPSupportAttrStr[]     = "IrLMPSupport";
const UInt8 kIASLMPSupportValue[]       = { 0x01, 0x00, 0x00};  // Version 1, no additional IAS support, no additional LMP support

//--------------------------------------------------------------------------------

#define super OSObject
    OSDefineMetaClassAndStructors(TIrGlue, OSObject);


//--------------------------------------------------------------------------------
//      TIrGlue
//--------------------------------------------------------------------------------
/*static*/
TIrGlue *
TIrGlue::tIrGlue(AppleIrDASerial *driver, AppleIrDA *appleirda, IOWorkLoop *workloop, USBIrDAQoS *qos)
{
    TIrGlue *obj = new TIrGlue;
    
    XTRACE(kLogNew, 0, obj);
    
    if (obj && !obj->init(driver, appleirda, workloop, qos)) {
	obj->release();
	obj = nil;
    }
    return obj;
}

Boolean TIrGlue::init(AppleIrDASerial *driver, AppleIrDA *appleirda, IOWorkLoop *workloop, USBIrDAQoS *qos)
{
    IrDAErr err;
    int i;
    
    XTRACE(kLogInit, 0, 0);
    
    fAppleIrDA = appleirda;
    
    bzero(fTimers, sizeof(fTimers));

    fLSAPIdsInUse = (UInt32) (1 << kNameServerLSAPId);      // name server (why doesn't it reserve it?)
    
    fIrLAP = nil;
    fIrLMP = nil;
    fIrLAPConn = nil;
    fIrDevice = nil;
    fIrDiscovery = nil;
    fMyQOS = fPeerQOS = nil;
    fNameServer = nil;
    fNameService = nil;
    
    fLastState = kIrDAStatusOff;
    
    if (!super::init()) return false;
    
    
    // Set up the the event block list
    err = TIrEvent::InitEventLists();
    nrequire(err, Fail);
    
    // Set up the three timers
    for (i = 0; i < kNumTimers; i++) {
	fTimers[i] = CTimer::cTimer(workloop, this, &::TimerNotifier);
	require (fTimers[i], Fail);
    }

    // and the two qos objects, lap needs these at init time
    fMyQOS =  TIrQOS::tIrQOS(qos);
    fPeerQOS = TIrQOS::tIrQOS(qos);

    // create LAP - todo: can now take the qos pointers out of the event requests
    fIrLAP = TIrLAP::tIrLAP(this, fMyQOS, fPeerQOS);
    require(fIrLAP, Fail);
    
    // create LMP
    fIrLMP = TIrLMP::tIrLMP(this);
    require(fIrLMP, Fail);

    // create LAPConn
    fIrLAPConn = TIrLAPConn::tIrLAPConn(this);
    require(fIrLAPConn, Fail);

    // create IrDevice
    fIrDevice = CIrDevice::cIrDevice(this, driver);
    require(fIrDevice, Fail);

    fNameService = TIASService::tIASService();      // make our IAS database
    require(fNameService, Fail);
    
    fNameServer = TIASServer::tIASServer(this, fNameService);
    require(fNameServer, Fail);

    // Create the discovery object
    fIrDiscovery = CIrDiscovery::cIrDiscovery(this);
    require(fIrDiscovery, Fail );
    
    // Finally, load the IAS database with a few defaults
    require(InitNameService(), Fail);
    
    fNameServer->ListenStart();         // start the ias server
    
    return true;
    
Fail:
    // we could free stuff here, but our real free routine will be
    // called when we're released, wait until then.
    
    XTRACE(kLogInit, 0xdead, 0xbeef);
    return false;

} // TIrGlue::Init


//--------------------------------------------------------------------------------
//      free
//--------------------------------------------------------------------------------
#define FREE(x) {if (x) { (x)->release(); x = nil; } }
void
TIrGlue::free()
{
    int i;
	
    XTRACE(kLogFree, 0, this);
    

    for (i = 0 ; i < kNumTimers; i++) {     // first, stop and free the timers
	FREE(fTimers[i]);
    }
    
    FREE(fMyQOS);
    FREE(fPeerQOS);
    FREE(fIrDiscovery);
    FREE(fIrDevice);
    FREE(fIrLAPConn);
    FREE(fIrLMP);
    FREE(fIrLAP);
    FREE(fNameServer);
    FREE(fNameService);

    TIrEvent::DeleteEventLists();       // release all the pending events (do last)
    
    super::free();

} // TIrGlue::free

void
TIrGlue::ReadComplete(UInt8 *buffer, UInt32 length)
{
    XTRACE(kLogReadComplete, length >> 16, length);
    
    if (fIrDevice)
	fIrDevice->ReadComplete(buffer, length);
}

void
TIrGlue::TransmitComplete(Boolean worked)
{
    XTRACE(kLogTransmitComplete, 0, worked);
    
    if (fIrDevice)
	fIrDevice->TransmitComplete(worked);
}

void
TIrGlue::SetSpeedComplete(Boolean worked)
{
    XTRACE(kLogSetSpeedComplete, 0, worked);
    
    if (fIrDevice)
	fIrDevice->SetSpeedComplete(worked);
}

void
TIrGlue::RunQueue(void)
{
    IrDAStatus status;
    
    XTRACE(kLogRunQueue, 0, 0);
    
    TIrStream::RunQueue();
    
    XTRACE(kLogRunQueue, 0x1111, 0x1111);
    
    GetIrDAStatus(&status);
    XTRACE(kLogRunQueue, 0x2222, 0x2222);

    if (fLastState != status.connectionState) {
	fLastState = status.connectionState;
	    XTRACE(kLogRunQueue, 0x3333, 0x3333);
	if (fAppleIrDA)
	    fAppleIrDA->messageClients(kIrDACallBack_Status, &fLastState, 1);
    }
    XTRACE(kLogRunQueue, 0xffff, 0xffff);
}

void
TIrGlue::Start()
{
    if (fIrDevice)
	fIrDevice->Start();
	
    fLastState = kIrDAStatusIdle;
    if (fAppleIrDA)
	fAppleIrDA->messageClients(kIrDACallBack_Status, &fLastState, 1);
}

void
TIrGlue::Stop()
{
    int i;
    
    if (fIrDevice)          // first stop trying to talk back to the device
	fIrDevice->Stop();
    
    // stop all the timers
    for (i = 0 ; i < kNumTimers; i++)
	StopTimer(i);
    
    Disconnected(true);     // reset lap and lmp and the qos objects

    fLastState = kIrDAStatusOff;
    if (fAppleIrDA)
	fAppleIrDA->messageClients(kIrDACallBack_Status, &fLastState, 1);
}

#pragma mark ---- Event blocks

// just layered over IrEvent static functions, used to be imp'd here

TIrEvent *
TIrGlue::GrabEventBlock(UInt32 event, UInt32 size)
{
    return TIrEvent::GrabEventBlock(event, size);
}

void
TIrGlue::ReleaseEventBlock(TIrEvent* reqBlock)
{
    return TIrEvent::ReleaseEventBlock(reqBlock);
}

#pragma mark ---- Timers

//--------------------------------------------------------------------------------
//      StartTimer
//--------------------------------------------------------------------------------
void TIrGlue::StartTimer(int id, TTimeout timeDelay, UInt32 refCon)
{
    XTRACE(kLogStartTimer, id, timeDelay);
    require(id < kNumTimers && id >= 0, Fail);
    
    if (fTimers[id])
	fTimers[id]->StartTimer(timeDelay, refCon);

Fail:
    return;
    
} // TIrGlue::StartTimer


//--------------------------------------------------------------------------------
//      StopTimer
//--------------------------------------------------------------------------------
void TIrGlue::StopTimer(int id)
{
    XTRACE(kLogStopTimer, 0, id);
    require(id < kNumTimers && id >= 0, Fail);
    
    if (fTimers[id])
	fTimers[id]->StopTimer();

Fail:
    return;
    
} // TIrGlue::StopTimer


//--------------------------------------------------------------------------------
//      TimerComplete
//--------------------------------------------------------------------------------
void TIrGlue::TimerComplete(UInt32 refCon)
{
    XTRACE(kLogTimerComplete, 0, refCon);
    
    if ((refCon >= kIrFirstIrLAPTimerEvent) && (refCon <= kIrLastIrLAPTimerEvent)) {
	fIrLAP->TimerComplete(refCon);      // send lap its timers
    }
    else
    if ((refCon >= kIrFirstIrLMPTimerEvent) && (refCon <= kIrLastIrLMPTimerEvent)) {
	fIrLMP->TimerComplete(refCon);
    }
    
    // Let's run the queue until it's empty now that a timer has fired and
    // we've probably queued up some work to do.
    RunQueue();
} // TIrGlue::TimerComplete


//--------------------------------------------------------------------------------
//      TimerNotifier: Completion proc for all timers
//--------------------------------------------------------------------------------
private
void TimerNotifier(OSObject *owner, IrDATimerEventSource *iotimer)
{
    TIrGlue *obj;

    XTRACE(kLogTimerNotifier1, 0, owner);
    XTRACE(kLogTimerNotifier2, 0, iotimer);
	
    require(owner, Failed);
    require(iotimer, Failed);
    
    obj = OSDynamicCast(TIrGlue, owner);
    require(obj, Failed);
    
    for (int i = 0 ; i < kNumTimers; i++) {
	CTimer *ctimer = obj->GetTimer(i);
	if (ctimer && ctimer->GetIOTimer() == iotimer) {    // found our CTimer, finally (joke: only 3 of them)
	    XTRACE(kLogTimerNotifier, 0, i);
	    obj->TimerComplete(ctimer->GetSignature());     // call timeout routine
	    XTRACE(kLogTimerNotifierDone, 0xffff, 0xffff);
	    return;
	}
    }
    
Failed:
    XTRACE(kLogTimerNotifier, 0xdead, 0xbeef);
    return;
}

#pragma mark ---- Lap Async Disconnect

//--------------------------------------------------------------------------------
//      Disconnected
//--------------------------------------------------------------------------------
void TIrGlue::Disconnected(Boolean reset_lap)
{
    //int review_async_disconnect;        // check both kinds of disconnect and verify they work

    XTRACE(kStartTerminateEntry, 0, reset_lap);
    
    fMyQOS->Reset();
    fPeerQOS->Reset();
    
    if (reset_lap) {
	fIrLMP->Reset();            // clean up pending i/o
	fIrLAP->Reset();            // then release the buffers
    }
    XTRACE(kStartTerminateExit, 0, 0);
} // TIrGlue::Disconnected

//--------------------------------------------------------------------------------
//      IsLAPConnected (move?)
//--------------------------------------------------------------------------------
Boolean TIrGlue::IsLAPConnected(void)           // check with lap re our connection status
{
    require(fIrLAP, Fail);
    return fIrLAP->IsConnected();
Fail:
    return false;
} // TIrGlue::IsLAPConnected

//
// tell lapconn to disconnect now, don't wait for idle timer to fire
//
void TIrGlue::DoIdleDisconnect()
{
    require(fIrLAPConn, Fail);
    fIrLAPConn->DoIdleDisconnect();
Fail:
    return;
}

#pragma mark ---- Name and LSAPId Service


//--------------------------------------------------------------------------------
//      InitNameService
//--------------------------------------------------------------------------------
Boolean TIrGlue::InitNameService()
{
    IrDAErr err;

    // Add an entry for class "Device", attribute "DeviceName"
    // Set to string "Macintosh"
    err = fNameService->AddStringEntry(kIASDeviceClassStr, kIASDeviceNameAttrStr, kIASDeviceName, kIASCharSetAscii, 0);
    nrequire(err, Fail);
    
    // Add an entry for class "Device", attribute "IrLMPSupport"
    // Set to 0x01, 0x00, 0x00 (version 1, no extra ias support, no extra LMP support)
    err = fNameService->AddNBytesEntry(kIASDeviceClassStr, kIASLMPSupportAttrStr, kIASLMPSupportValue, sizeof(kIASLMPSupportValue));
    nrequire(err, Fail);

    return true;

Fail:
    return false;

} // TIrGlue::InitNameService

//--------------------------------------------------------------------------------
//      ObtainLSAPId
//--------------------------------------------------------------------------------
IrDAErr TIrGlue::ObtainLSAPId( UInt32 & desiredLSAPId )
{
    UByte lsapId;

    if (fLSAPIdsInUse == 0xFFFFFFFF) {
	DebugLog("TIrGlue::ObtainLSAPId: no more LSAP Ids available");
	return kIrDAErrResourceNotAvailable;
    }

    if (desiredLSAPId == kAssignDynamicLSAPId) {
	lsapId = (UByte)random() & 0x1F;    // Random starting point between 0 and 31
    }
    else {
	if (desiredLSAPId > 0x1F /*kLastValidLSAPId*/) {
	    DebugLog("TIrGlue::ObtainLSAPId: LSAP id out of range");
	    return kIrDAErrBadParameter;
	}
	lsapId = (UByte)desiredLSAPId;  // Try to get the one requested
    }

    while ((lsapId == 0) || (fLSAPIdsInUse & (1 << lsapId))) {
	lsapId = (lsapId + 1) & 0x1F;
    }

    if ((desiredLSAPId != kAssignDynamicLSAPId) && (lsapId != desiredLSAPId)) {
	// Wanted a specific lsapId, but its already being taken
	return kIrDAErrResourceNotAvailable;
    }

    desiredLSAPId = (ULong)lsapId;
    fLSAPIdsInUse |= (1 << lsapId);
    XTRACE(kObtainLSAPIdEvent, lsapId, fLSAPIdsInUse);

    return noErr;

} // TIrGlue::ObtainLSAPId


//--------------------------------------------------------------------------------
//      ReleaseLSAPId
//--------------------------------------------------------------------------------
void TIrGlue::ReleaseLSAPId( UInt32 lsapId)
{
    check( lsapId != kNameServerLSAPId );
    check( lsapId < 32 );
    check( fLSAPIdsInUse & (1 << lsapId) );

    fLSAPIdsInUse &= ~(1 << lsapId);
    XTRACE( kReleaseLSAPIdEvent, lsapId, fLSAPIdsInUse );

} // TIrGlue::ReleaseLSAPId

//--------------------------------------------------------------------------------
//      RegisterMyNameAndLSAPId
//--------------------------------------------------------------------------------
IrDAErr TIrGlue::RegisterMyNameAndLSAPId(UInt8 * className, UInt8 * attrName, UInt32 * reqLSAP)
{
    IrDAErr     result = kIrDAErrWrongState;
    UInt32      tempLSAP = *reqLSAP;                    // Load the clients requested LSAP

    require(fNameService, Fail);
    require(className, Fail);
    require(attrName, Fail);
    
    // Obtain the lsapId
    result = ObtainLSAPId(tempLSAP);
    nrequire(result, Fail);
    
    *reqLSAP = tempLSAP;

    // Register my lsap-id with the name service
    result = fNameService->AddIntegerEntry(className, attrName, tempLSAP );
    nrequire(result, Fail_RegisterLSAPId);

    return noErr;

Fail_RegisterLSAPId:

    ReleaseLSAPId( ( UByte )tempLSAP );

Fail:
    return result;

} // TIrGlue::RegisterMyNameAndLSAPId



//======================== Start / Completion routines ===========================

#pragma mark ---- Connect and Listen
    
//--------------------------------------------------------------------------------
//      ConnectStart
//--------------------------------------------------------------------------------
IrDAErr TIrGlue::ConnectStart(  TIrStream   *   client,     // Caller
				UInt32          myLSAPId,   // Reseved by RegisterMyNameAndLSAPID
				UInt32          devAddr,    // Determined through LAP Discovery
				UInt32          peerLSAPId, // Determined through IAS Query 
				CBuffer     *   clientData,
				TLSAPConn   **  theLSAP     )   // Returned to caller
{
    TIrConnLstnRequest  *connectRequest = nil;
    TLSAPConn           *lsap = nil;

    XTRACE( kConnectStartEvent, ( myLSAPId<<8 ) + peerLSAPId, devAddr );
    
    connectRequest = (TIrConnLstnRequest*)GrabEventBlock(kIrConnectRequestEvent);
    require(connectRequest, Fail);

    if (*theLSAP == nil) {                              // if the caller doesn't already have an LSAPConn ...
	lsap = TLSAPConn::tLSAPConn(this, client);      // build one
	require(lsap, Fail_NewLSAPConn);
	
	// Assign the lsap-id for the LSAPConn (client already reseved it with RegisterMyNameAndLSAPID)
	lsap->AssignId(myLSAPId);
	
	*theLSAP = lsap;                // Give the lsap to the client
    }
    else
	lsap = *theLSAP;                // jdg: the caller is giving us the lsapconn from last time

//  IOLog("glue connnect qos baud is 0x%x\n", GetMyQOS()->SetBaudRate(k57600bps));  // DEBUGGING
    
    // Initiate the connect for my LSAPConn
    connectRequest->fDevAddr    = devAddr;
    connectRequest->fLSAPId     = (UInt8)peerLSAPId;
    connectRequest->fMyQOS      = GetMyQOS();
    connectRequest->fPeerQOS    = GetPeerQOS();
    connectRequest->fData       = clientData;
    connectRequest->fClient     = client;
    
    lsap->EnqueueEvent(connectRequest);

    return noErr;

    // Out of memory error exit points
Fail_NewLSAPConn:
    ReleaseEventBlock(connectRequest);
    
Fail:
    return kIrDAErrNoMemory;
} // TIrGlue::ConnectStart


//--------------------------------------------------------------------------------
//      ListenStart
//--------------------------------------------------------------------------------
IrDAErr TIrGlue::ListenStart(   TIrStream   *   client,         // Caller
				UInt32          lsapId,         // Preallocated LSAP Id
				CBuffer     *   clientData,     // Data to pass with connect
				TLSAPConn   **  theLSAP     )   // The allocated LSAP
{
    TLSAPConn           *lsap;
    TIrConnLstnRequest  *listenRequest;

    XTRACE(kListenStartEvent, lsapId, 0);

    listenRequest = (TIrConnLstnRequest *)GrabEventBlock(kIrListenRequestEvent);
    require(listenRequest, Fail);

    if (*theLSAP == nil) {                              // if the caller doesn't already have an LSAPConn ...
	lsap = TLSAPConn::tLSAPConn(this, client);      // Create and init new LSAPConn
	require(lsap, Fail_NewLSAPConn);
    
							// Note: Caller has already registered with IAS
	lsap->AssignId( lsapId );                       // Assign the lsap-id for the LSAPConn
	
	*theLSAP = lsap;                                // return the new lsap to the client
    }
    else
	lsap = *theLSAP;                                // the caller is giving us the lsapconn from last time

    
    listenRequest->fMyQOS   = GetMyQOS();
    listenRequest->fPeerQOS = GetPeerQOS();
    listenRequest->fData    = clientData;
    
    lsap->EnqueueEvent( listenRequest );

    return noErr;

Fail_NewLSAPConn:                                   // Out of memory error exit points

    ReleaseEventBlock(listenRequest);

Fail:

    return kIrDAErrNoMemory;
} // TIrGlue::ListenStart

// statistics
void
TIrGlue::GetIrDAStatus(IrDAStatus *status)
{
    Boolean connected = false;
    Boolean discoverActive = false;
    Boolean brokenConnection = false;
    
    if (fIrDevice && fIrLAP && status) {
	fIrDevice->GetStatus(status);       // get the statistics record (ugh, we stomp on it here)

	connected           = fIrLAP->IsConnected();
	brokenConnection    = fIrLAP->InBrokenBeam();
	discoverActive      = fIrLAP->Discovering();
	
	if( brokenConnection )
	    status->connectionState = kIrDAStatusBrokenConnection;
	else
	    if( connected )
		status->connectionState = kIrDAStatusConnected;
	    else
		if( discoverActive )
		    status->connectionState = kIrDAStatusDiscoverActive;
		else
		    status->connectionState = kIrDAStatusIdle;
	
	status->connectionSpeed = fIrDevice->GetSpeed();
	fIrLAP->GetNickName(status->nickName, sizeof(status->nickName));
    }
}

#ifdef NOT_NOW
#pragma mark ------------------ NOT NOW ----------




extern "C" 
pascal void XDeferredTaskProc( void *parm );        // test test test test


//--------------------------------------------------------------------------------
//      Init
//--------------------------------------------------------------------------------
NewtonErr TIrGlue::Init()
{
    NewtonErr result;
    OSErr err;

    XTRACE(kInit, (int)this >> 16, this);
    
    gIrDAPrefs.Init();          // initialize the irda prefs from iqos resources
				// note this gets Reset each time the ir hardware is changed (ugh!)


    fMyQOS.Reset();                             // now that we have hardware, get matching QoS values
    fPeerQOS.Reset();
    
    result = InitEventBlockList();
    nrequire( result, Fail_Init );

    result = fIrLAP.Init(this, &fIrLMP);        // Initialize IrLAP -- FIX LAP TO NOT KEEP A LMP PTR
    nrequire( result, Fail_Init );

    result = fIrLMP.Init(this, &fIrLAP);        // Initialize IrLMP -- FIX LMP TO NOT KEEP A LAP PTR
    nrequire( result, Fail_Init );
    
    result = fIrDiscovery.Init( this );
    nrequire( result, Fail_Init );
    
    InitNameService();

    return noErr;

Fail_Init:

    DeInit();               // reset
    return -1;

} // TIrGlue::Init


//--------------------------------------------------------------------------------
//      DeInit
//--------------------------------------------------------------------------------
void TIrGlue::free(void)
{
    XTRACE(kDeInit, 0, 0);
    
    // let's stop the timers right away ...
    fTimer1.DeInit();
    fTimer2.DeInit();
    fTimer3.DeInit();


    // Free the event block list and any event blocks on it
    DeleteEventBlockList();

} // TIrGlue::DeInit


#endif // NOT_NOW
