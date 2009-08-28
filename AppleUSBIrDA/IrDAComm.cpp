/* IrDAComm.h - Start up IrDA and the IrComm client */

#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOCommandGate.h>
#include "IrDAComm.h"
#include "IrGlue.h"
#include "IrComm.h"
#include "CTimer.h"
#include "IrDALog.h"

#if (hasTracing > 0 && hasIrDACommTracing > 0)

enum tracecodes
{
    kLogNew = 1,
    kLogFree,
    kLogInit,
    kLogStop,
    kLogStop1,
    kLogStopThread,
    
    kLogTxBufferAvailable,
    kLogWrite,
    kLogReadComplete,
    kLogReturnCredit,
    kLogConnectionStatus,
    kLogTransmitComplete,
    kLogSetSpeedComplete,
    kLogBackEnable,
    
    kLogTimer,
    kLogTimerFinished,
    
    kLogXmitCompleteErr,
    kLogReturnCreditErr,
    
    kLogDoSomething,
    kLogStateChange
};

static
EventTraceCauseDesc gTraceEvents[] = {
    {kLogNew,               "IrDAComm: new, obj="},
    {kLogFree,              "IrDAComm: free, obj="},
    {kLogInit,              "IrDAComm: init, obj="},
    {kLogStop,              "IrDAComm: stop, obj="},
    {kLogStop1,             "IrDAComm: stop waiting for disconnect"},
    {kLogStopThread,        "IrDAComm: stop thread"},
    
    {kLogTxBufferAvailable, "IrDAComm: tx buffer available"},
    {kLogWrite,             "IrDAComm: write, state=,length="},
    {kLogReadComplete,      "IrDAComm: pkt read complete, length="},
    {kLogReturnCredit,      "IrDAComm: return credit, bytecount="},
    {kLogConnectionStatus,  "IrDAComm: new connection status, state=, connected="},
    {kLogTransmitComplete,  "IrDAComm: pkt transmit complete, worked="},
    {kLogSetSpeedComplete,  "IrDAComm: set speed complete"},
    {kLogBackEnable,        "IrDAComm: back enable, write active="},
    
    {kLogTimer,             "IrDAComm: timer routine entry, state="},
    {kLogTimerFinished,     "IrDAComm: timer routine exit"},
    
    {kLogXmitCompleteErr,   "IrDAComm: ERROR. transmit complete while stack is active"},
    {kLogReturnCreditErr,   "IrDAComm: ERROR. return credit while stack is active"},
    
    {kLogDoSomething,       "IrDAComm: run command in gate, cmd code="},
    {kLogStateChange,       "IrDAComm: state change entry, event=, current state="}
};

#define XTRACE(x, y, z) IrDALogAdd ( x, y, (uintptr_t)z & 0xffff, gTraceEvents, true)
#else
#define XTRACE(x, y, z) ((void)0)
#endif

extern "C" void timeoutRoutine(OSObject *owner, IOTimerEventSource *sender);

enum {          // command codes for DoSomething
    cmdTxBufferAvailable,
    cmdWrite,
    cmdReturnCredit,
    cmdStop,
    cmdStopEvent,
    cmdReadComplete,
    cmdXmitComplete,
    cmdSetSpeedComplete
};

/****************************************************************************************************/
#define super OSObject

    OSDefineMetaClassAndStructors(IrDAComm, OSObject);
    
    
//
// factory create
//
/*static*/
IrDAComm *
IrDAComm::irDAComm(AppleIrDASerial *driver, AppleIrDA *appleirda)
{
    IrDAComm *obj = new IrDAComm;   // create an IrDAComm object
    
    XTRACE(kLogNew, 0, obj);
    
    if (obj && !obj->init(driver, appleirda)) {
	obj->release();
	obj = nil;
    }
    
    return obj;
}


void IrDAComm::free()
{
    IOWorkLoop *workloop;
    
    XTRACE(kLogFree, 0, this);
    
    this->Stop();       // make sure we're stopped before releasing memory

    if (fDriver) {
	workloop = fDriver->getWorkLoop();
	if (workloop) {
	    if (fGate)
		workloop->removeEventSource(fGate);
	    //if (fTimerSrc)
	    //  workloop->removeEventSource(fTimerSrc);
	}
    }

#define FREE(x) { if (x) { (x)->release(); x = nil; }}

    FREE(fGate);
    FREE(fTimer);
    FREE(fIrComm);          // free the ircomm object before the rest of irda ...
    FREE(fIrDA);
    
#undef FREE

#define THREAD_FREE(x) do { if (x) {               \
			    thread_call_cancel(x); \
			    thread_call_free(x);   \
			    x = NULL; } } while(0)
    
    THREAD_FREE(fStop_thread);
#undef THREAD_FREE
    
    super::free();      // we're done, call super
}
    

/****************************************************************************************************/
//
//      Method:     IrDAComm::Init
//
//      Inputs:     pointer to the usb irda driver
//
//      Outputs:    return code - true (initialized ok), false (it didn't)
//
//      Desc:       Initialize the IrDAComm class
//
/****************************************************************************************************/

bool IrDAComm::init(AppleIrDASerial *driver, AppleIrDA *appleirda)
{
    IOReturn rc;
    IOWorkLoop *workloop;
	
    XTRACE(kLogInit, 0, this);
#if (hasTracing > 0)
    DebugLog("log info at 0x%lx", (uintptr_t)IrDALogGetInfo());
#endif
    require(driver, Fail);

    fState = kIrDACommStateStart;
    fDriver = driver;
    //fTimerSrc = nil;
    fTimer = nil;
    fQoS = nil;
    fIrDA = nil;
    fIrComm = nil;
    fWriteBusy = false;
    fGate = nil;
    fStartCounter = 0;          // counter for initial connection attempts
    fStop_thread = nil;

    if (!super::init())
	return false;
    
    fQoS = driver->GetIrDAQoS();
    require(fQoS, Fail);
    
    workloop = fDriver->getWorkLoop();
    require(workloop, Fail);
    
    fStop_thread = thread_call_allocate(stop_thread, this);
    require(fStop_thread, Fail);
    
    fIrDA = TIrGlue::tIrGlue(fDriver, appleirda, workloop, fQoS);       // create irda stack
    require(fIrDA, Fail);
    
    fIrComm = IrComm::irComm(fIrDA, this);                                          // create an ircomm object
    require(fIrComm, Fail);
	
    fGate = IOCommandGate::commandGate(this, 0);    // create a new command gate for our access to IrDA
    require(fGate, Fail);
    
    rc = workloop->addEventSource(fGate);           // add it to the usb workloop
    require(rc == kIOReturnSuccess, Fail);

    fTimer = CTimer::cTimer(workloop, this, &IrDAComm::TimerRoutine);
    require(fTimer, Fail);
    
    fTimer->StartTimer(100, 0);                     // 100ms delay after init and then startup

    //fTimerSrc = IOTimerEventSource::timerEventSource ( driver, &::timeoutRoutine);    // create an io timer
    //require(fTimerSrc, Fail);
    //
    //rc = workloop->addEventSource(fTimerSrc);
    //require(rc == kIOReturnSuccess, Fail);
    //      
    //rc = fTimerSrc->setTimeoutMS(100);            // 100 ms delay after init'ing
    //require(rc == kIOReturnSuccess, Fail);

    return true;

Fail:
    return false;
    
} /* end Initialize */


/****************************************************************************************************/
//
//      Method:     IrDAComm::Stop
//
//      Inputs:     
//
//      Outputs:    return code - kIOReturnSuccess
//
//      Desc:       Stops the class (clean up etc.)
//
/****************************************************************************************************/

IOReturn IrDAComm::Stop(void)
{
    int i;
    IOReturn rc = kIOReturnSuccess;
    
    XTRACE(kLogStop, 0, this);
    require(fGate, Fail);
    require(fIrDA, Fail);           // sanity
    require(fDriver, Fail);         // sanity
    
    if (fState != kIrDACommStateStopped) {          // if not already stopped
	boolean_t bt;
	
	if (fDriver->getWorkLoop()->inGate())   {       // if we have the gate, just call it
	    rc = fGate->runAction(&DoSomething, (void *)cmdStopEvent, nil, nil, nil);       
	    check(rc == kIOReturnSuccess);
	}
	else {                                          // we don't have the gate, run stop in another thread and wait for it
	    bt = thread_call_enter(fStop_thread);       // run stop logic in another thread
	    check(bt == false);                         // true here means it was already running and we're confused

	    for (i = 0 ; i < 10; i++) {                 // max wait of a second (should be more than enough)
		XTRACE(kLogStop1, i, fState);
		if (fState == kIrDACommStateStopped &&              // if ircomm is stopped and the irlap link is down
		    fIrDA->IsLAPConnected() == false) break;        // then we're really stopped
		IOSleep(100);                                       // wait 1/10 of a second per state poll
	    }
	}
	check(fState == kIrDACommStateStopped);     // this will fail if in broken-beam, just debugging
	
	rc = fGate->runAction(&DoSomething, (void *)cmdStop, nil, nil, nil);
	check(rc == kIOReturnSuccess);
    }

Fail:
    fState = kIrDACommStateStopped;     // it's really stopped now, regardless of above
    XTRACE(kLogStop, 0xffff, 0xffff);

    return rc;
}

/****************************************************************************************************/
//
//      Method:     IrDAComm::TXBufferAvailable
//
//      Inputs:
//
//      Outputs:    size - number of bytes available for a write
//
//      Desc:       Returns the number of bytes IrDA can currently accept for a write
//
/****************************************************************************************************/

size_t IrDAComm::TXBufferAvailable()
{
    IOReturn rc;
    size_t  result = 0;
    
    XTRACE(kLogTxBufferAvailable, 0, 0);
    
    // we're getting called from outside the usb workloop, so run through our command gate
    
    if (fIrComm && fGate) {
	rc = fGate->runAction(&DoSomething, (void *)cmdTxBufferAvailable, &result);     
	check(rc == kIOReturnSuccess);
    }
	
    XTRACE(kLogTxBufferAvailable, 0xffff, result);
    return result;
	
} /* end TXBufferAvailable */

/****************************************************************************************************/
//
//      Method:     IrDAComm::Write
//
//      Inputs:     Buf - the data to be written, Length - the size of the data
//
//      Outputs:    size - number of bytes written
//
//      Desc:       Queue the data to be written by IrComm
//
/****************************************************************************************************/

size_t IrDAComm::Write(UInt8 *buf, size_t length)
{
    UInt32 result = length;
    IOReturn rc;
    
    XTRACE(kLogWrite, fState, length);

    // this is coming from outside our workloop, send through our gate
     
    if (fState == kIrDACommStateConnected && fIrComm && fGate) {
	rc = fGate->runAction(&DoSomething, (void *)cmdWrite, buf, (void *)length, &result);        
	check(rc == kIOReturnSuccess);
    }

    return result;          // pretend it worked if no connection (i.e. sink to /dev/null)

} /* end Write */

/****************************************************************************************************/
//
//      Method:     IrDAComm::ReadComplete
//
//      Inputs:     Frame - Incoming IrDA frame
//
//      Outputs:    Return code - kIOReturnSuccess or
//
//      Desc:       Process an incoming frame
//
/****************************************************************************************************/

IOReturn IrDAComm::ReadComplete(UInt8 *buf, size_t length)
{
    IOReturn rc = -1;

    XTRACE(kLogReadComplete, length >> 16, length);
    
    if (fGate && fIrComm && fIrDA) {
	rc = fGate->runAction(&DoSomething, (void *)cmdReadComplete, buf, (void *)length, nil);     
    }
    
    return rc;

} /* end ReadComplete */

//
// Sending back flow-control to the peer
//
void
IrDAComm::ReturnCredit(size_t byte_count)       // serial client has consumed count bytes of data
{
    IOReturn rc;
    
    XTRACE(kLogReturnCredit, byte_count >> 16, byte_count);
    
    // this is coming from outside our workloop, send to irda via our command gate
	    
    if (fState == kIrDACommStateConnected && fIrComm && fGate) {
	rc = fGate->runAction(&DoSomething, (void *)cmdReturnCredit, (void *)byte_count);
	check(rc == kIOReturnSuccess);
    }

    return;
}


void
IrDAComm::Transmit_Complete(Boolean worked)
{
    IOReturn rc;

    XTRACE(kLogTransmitComplete, 0, worked);
    
    if (fGate && fIrDA) {
	rc = fGate->runAction(&DoSomething, (void *)cmdXmitComplete, (void *)worked);
    }
    
    XTRACE(kLogTransmitComplete, 0xffff, 0xffff);
}

void
IrDAComm::SetSpeedComplete(Boolean worked)
{
    IOReturn rc;

    XTRACE(kLogSetSpeedComplete, 0, worked);
    
    if (fGate && fIrDA) {
	rc = fGate->runAction(&DoSomething, (void *)cmdSetSpeedComplete, (void *)worked);
    }
    
    XTRACE(kLogSetSpeedComplete, 0xffff, 0xffff);
}



//
// Called by IrCOMM to send data back to the pseudo tty
//
void
IrDAComm::IrCommDataRead(UInt8 *buf, UInt32 length)     // ircomm data to pass back to the tty
{
    if (fDriver)                                // if we're not stopped
	fDriver->Add_RXBytes(buf, length);      // error return?
    else
	DebugLog("IrDAComm data read but no driver");
}

//
// IrComm calls this when our tinytp peer has extended more credit
// to us .. i.e. previously blocked writes may now work.
//
void
IrDAComm::BackEnable(void)
{
    XTRACE(kLogBackEnable, 0, fWriteBusy);
    
    if (fDriver                     // if we're alive and ...
     /* && fWriteBusy */    )               // last time they asked we were busy, then
    // above 'optimization' test stopped the tx queue wheneveer we break up a too-big
    // write into several small writes.  radar 2890966
	    fDriver->SetUpTransmit();       // ask driver code to try to transmit again
}

//
// The state engine for keeping irda in a connect/listen loop.
//

void
IrDAComm::StateChange(int event)
{
    XTRACE(kLogStateChange, event, fState);
    
    require(fIrComm, Fail);
    require(fIrDA, Fail);
    
    switch (fState) {
    
	case kIrDACommStateStart:                   // Starting up and doing initial discovery/connect attempts
		switch (event) {
		
		    case kIrDACommEventTimer:                   // start off the state engine.
			check(fStartCounter == 0);              // sanity
			if (fStartCounter == 0) {
			    fIrDA->Start();                     // start irda stack up
			    fIrComm->TryConnect(1);             // and do the first discovery
			}
			break;
			
		    case kIrDACommEventConnected:               // we're connected!
			fState = kIrDACommStateConnected;
			break;
			
		    case kIrDACommEventDisconnected:            // initial discover/connect failed.  Try again 3 times
			if (fStartCounter++ < 3)
			    fIrComm->TryConnect(1);
			else
			    fState = kIrDACommStateIdle;        // could listen here, but main timer is about to fire soon
			break;
			
		    case kIrDACommEventStop:
			if (fStartCounter == 0)                 // if stopped before we really started
			    fState = kIrDACommStateStopped;     // then we're done w/out doing anything
			else {                                  // else we have a connect pending
			    fState = kIrDACommStateStopping2;   // wait for two callbacks
			    fIrComm->Disconnect();              // abort the connect attempt
			}
			break;
		}
		break;
		    
	case kIrDACommStateIdle:                    // not doing much
		switch (event) {
		
		    case kIrDACommEventTimer:               // time to wake up.  Let's try a discover/connect
			fState = kIrDACommStateConnecting;
			fIrComm->TryConnect(6);
			break;
			
		    case kIrDACommEventConnected:           // connected w/out my doing anything?
			DebugLog("logic error event=%d, state=%d", event, fState);
			break;
			
		    case kIrDACommEventDisconnected:        // disconnect when idle?  seems unlikley
			DebugLog("logic error event=%d, state=%d", event, fState);
			break;
			
		    case kIrDACommEventStop:                // stopped when we're idle
			fState = kIrDACommStateStopped;     // then we're done w/out doing anything
			break;

		}
		break;
		
	case kIrDACommStateConnecting:              // trying to connect
		switch (event) {
		
		    case kIrDACommEventTimer:                   // connect is stuck, likely in broken beam
			DebugLog("connect timing out");
			break;
			
		    case kIrDACommEventConnected:
			fState = kIrDACommStateConnected;       // we've connected!
			break;
			
		    case kIrDACommEventDisconnected:            // connect failed, switch to listen
			fState = kIrDACommStateListening;
			fIrComm->Listen();
			break;
			
		    case kIrDACommEventStop:                    // stop request with connect pending
			fState = kIrDACommStateStopping2;       // wait for two callbacks
			fIrComm->Disconnect();                  // abort the connect attempt
			break;
		}
		break;
		
	case kIrDACommStateListening:               // listening for a peer to discover and connect to us
		switch (event) {
		
		    case kIrDACommEventTimer:                           // Time to stop listening and try a connect again.
			fState = kIrDACommStateStoppingListen;          // Issue a disconnect to abort listen.  We'll get two
			fIrComm->Disconnect();                          // completes, one for listen stopped, and one for disconnect done.
			break;
			
		    case kIrDACommEventConnected:                       // the listen worked, we've got a peer!
			fState = kIrDACommStateConnected;
			break;
			
		    case kIrDACommEventDisconnected:                    // we can get a disconnect if the link bounces, just
			fIrComm->Listen();                              // just keep the listen going
			break;
			
		    case kIrDACommEventStop:                            // stop request when listen pending
			fState = kIrDACommStateStopping2;               // wait for two callbacks
			fIrComm->Disconnect();                          // abort the listen
			break;
		}
		break;
		
	case kIrDACommStateStoppingListen:          // waiting for listen abort to finish it's disconnect
		switch (event) {
		
		    case kIrDACommEventTimer:                           // stuck waiting for listen abort.  likely in broken beam
			DebugLog("stopping listen timer fired, are we stuck?");
			break;
			
		    case kIrDACommEventConnected:                       // connected?  have a disconnect pending.  wait for it.
			DebugLog("listen disconnect race condition");
			break;
			
		    case kIrDACommEventDisconnected:                    // listen aborted.  Now wait for the disconnect complete
			fState = kIrDACommStateDisconnecting;
			break;
			
		    case kIrDACommEventStop:                            // stopped after issuing disconnect to stop listen
			fState = kIrDACommStateStopping2;               // wait for two callbacks
			break;
		}
		break;
		
	case kIrDACommStateDisconnecting:           // waiting for a disconnect request to finish
		switch (event) {
		
		    case kIrDACommEventTimer:                           // stuck doing disconnect, most likely in broken beam
			DebugLog("disconnect timing out?");
			break;
			
		    case kIrDACommEventConnected:                       // connected?  race condition.  Expect a disconnect soon.
			DebugLog("disconnect race condition");
			break;
			
		    case kIrDACommEventDisconnected:                    // listen abort finished, let's try a connect
			fState = kIrDACommStateConnecting;              // switch from listen to connect
			fIrComm->TryConnect(6);                         // a reasonable number of discover slots after quick start
			break;

		    case kIrDACommEventStop:                            // stopped after issuing disconnect
			fState = kIrDACommStateStopping;                // wait for one more callback
			break;
			
		}
		break;
		
	case kIrDACommStateConnected:               // ircomm channel open, data can flow
		switch (event) {
		
		    case kIrDACommEventTimer:                           // nothing to do, we're a happy camper
			break;
			
		    case kIrDACommEventConnected:                       // connected again?  logic error
			DebugLog("logic error event=%d, state=%d", event, fState);
			break;
			
		    case kIrDACommEventDisconnected:                    // lost our connection
			fState = kIrDACommStateIdle;                    // wait for timer before trying to connect again
			break;
			
		    case kIrDACommEventStop:                            // stopped when we're up and running
			fState = kIrDACommStateStopping;                // wait for one callback
			fIrComm->Disconnect();                          // disconnect
			break;
		}
		break;
		
	case kIrDACommStateStopping2:
		switch (event) {
		    case kIrDACommEventTimer:                           // timer should be stopped
			DebugLog("logic error event=%d, state=%d", event, fState);
			break;
			
		    case kIrDACommEventConnected:                       // race condition
			fState = kIrDACommStateStopping;                // should disconnect "real soon" now
			break;
			
		    case kIrDACommEventDisconnected:
			fState = kIrDACommStateStopping;                // normal, listen/connect has aborted
			break;
			
		    case kIrDACommEventStop:
			DebugLog("logic error event=%d, state=%d", event, fState);
			break;
		}
		break;
		
	case kIrDACommStateStopping:
		switch (event) {
		    case kIrDACommEventTimer:
			DebugLog("logic error event=%d, state=%d", event, fState);
			break;
			
		    case kIrDACommEventConnected:
			DebugLog("logic error event=%d, state=%d", event, fState);
			break;
			
		    case kIrDACommEventDisconnected:
			fState = kIrDACommStateStopped;             // stop completed
			if (fIrDA->IsLAPConnected())                // if lap is connected, then
			    fIrDA->DoIdleDisconnect();              // don't wait 1 second, disconnect now
			break;
			
		    case kIrDACommEventStop:
			DebugLog("logic error event=%d, state=%d", event, fState);
			break;
		}
		break;
		
	case kIrDACommStateStopped:         // we're stopped, shouldn't be getting requests
		DebugLog("logic error event=%d, state=%d", event, fState);
		break;
		
    }
Fail:
    return;
}

//
// called by IrComm to let us know of major connection state changes
//
void
IrDAComm::ConnectionStatus(Boolean connected)
{
    static Boolean last_connected = false;      // cut down on the debug log noise
    
    XTRACE(kLogConnectionStatus, fState, connected);
    if (connected != last_connected) {
	DebugLog("connection status %d", connected);
	last_connected = connected;
    }
    check(fState != kIrDACommStateIdle);

    if (connected)  StateChange(kIrDACommEventConnected);
    else            StateChange(kIrDACommEventDisconnected);
}

//
// 5 second timer, keep connection attempts going.
//
/* static */
void
IrDAComm::TimerRoutine(OSObject *owner, IrDATimerEventSource *iotimer)
{ 
    IrDAComm *obj;
    
    XTRACE(kLogTimer, 0, 0);
    
    obj = OSDynamicCast(IrDAComm, owner);
    require(obj, Fail);
    
    XTRACE(kLogTimer, 0x1111, obj->fState);
    
    obj->StateChange(kIrDACommEventTimer);
		    
    if (obj->fIrDA) {                   // now run the irda event queue until it's done
	obj->fIrDA->RunQueue();
    }

    require(obj->fTimer, Fail);
    obj->fTimer->StartTimer(5 * 1000, 0);       // wake up again in 5 seconds

    XTRACE(kLogTimerFinished, 0, 0);
    return;

Fail:
    XTRACE(kLogTimerFinished, 0xdead, 0xbeef);
    return;
}

/****************************************************************************************************/
//
//      Method:     timeoutRoutine
//
//      Inputs:     object handle, timer source
//
//      Outputs:    none
//
//      Desc:       call the real C++ method to handle the timeout
//
/****************************************************************************************************/
/*
extern "C"
void
timeoutRoutine(OSObject *owner, IOTimerEventSource *sender)
{
    AppleIrDASerial *driver = (AppleIrDASerial *)owner;
    IrDAComm *me = driver->GetIrDAComm();
	
    me->TimerRoutine(sender);
}
*/

//
//
//static
IOReturn
IrDAComm::DoSomething(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4)
{
    IrDAComm *obj;
    uintptr_t cmd = (uintptr_t)arg1;
    
    XTRACE(kLogDoSomething, 0, (uintptr_t)arg1);        // log the command code
    
    obj = OSDynamicCast(IrDAComm, owner);
    require(obj, Fail);
    
    switch (cmd) {
	case cmdTxBufferAvailable:
			{
			    UInt32  count = 0;
			    UInt32  *result = (UInt32 *)arg2;
			    if (obj->fIrComm)
				count = obj->fIrComm->TxBufferAvailable();
			    obj->fWriteBusy = (count == 0);
			    if (result)
				*result = count;
			}
			break;
	
	case cmdWrite:
			{
			    UInt8   *buf    = (UInt8 *)arg2;
			    uintptr_t  length  = (uintptr_t)arg3;
			    UInt32  *result = (UInt32 *)arg4;
			    
			    if (result)
				*result = obj->fIrComm->Write(buf, length);
			}
			break;
			
	case cmdReturnCredit:
			{
			    uintptr_t  byte_count = (uintptr_t)arg2;
			    if (obj->fIrComm)
				obj->fIrComm->ReturnCredit(byte_count);
			}
			break;
			
	case cmdStop:
			{
			    if (obj->fTimer) {
				obj->fTimer->StopTimer();
				obj->fTimer->release();
				obj->fTimer = nil;
			    }
			    
			    if (obj->fIrDA) {
				obj->fIrDA->Stop();         // tell irda not to use the driver anymore
			    }
			}
			break;
			
	case cmdStopEvent:
			check(obj->fTimer);
			if (obj->fTimer) {
			    obj->fTimer->StopTimer();                   // stop the timer
			    obj->StateChange(kIrDACommEventStop);       // get the state engine to stop
			}
			if (obj->fIrDA) {                   // now run the irda event queue until it's done
			    obj->fIrDA->RunQueue();
			}
			break;
	
	case cmdReadComplete:
			{
			    UInt8   *buf    = (UInt8 *)arg2;
			    uintptr_t  length  = (uintptr_t)arg3;
			    if (obj->fIrDA) {
				obj->fIrDA->ReadComplete(buf, length);
				obj->fIrDA->RunQueue();
			    }
			}
			break;
			
	case cmdXmitComplete:
			{
			    bool    worked = (bool)arg2;
			    if (obj->fIrDA) {
				obj->fIrDA->TransmitComplete(worked);
				XTRACE(kLogTransmitComplete, 0x1111, 0x1111);
				obj->fIrDA->RunQueue();
				XTRACE(kLogTransmitComplete, 0x2222, 0x2222);
			    }
			}
			break;
			
	case cmdSetSpeedComplete:
			{
			    bool    worked = (bool)arg2;
			    if (obj->fIrDA) {
				obj->fIrDA->SetSpeedComplete(worked);
				XTRACE(kLogSetSpeedComplete, 0x1111, 0x1111);
				obj->fIrDA->RunQueue();
				XTRACE(kLogSetSpeedComplete, 0x2222, 0x2222);
			    }
			}
			break;
			
	default:
			check(0);
			break;      
    }
			
    return kIOReturnSuccess;

Fail:
    return kIOReturnBadArgument;
}

//
// return irda status to user-client
//
void
IrDAComm::GetIrDAStatus(IrDAStatus *status)
{
    if (fIrDA && status)
	fIrDA->GetIrDAStatus(status);
}

//
// return true if starting up (initial connection attempt)
//
bool
IrDAComm::Starting()
{
    return fState == kIrDACommStateStart;
}


void IrDAComm::stop_thread(thread_call_param_t param0, thread_call_param_t param1)
{
    IrDAComm *obj;
    IOReturn rc;
    
    XTRACE(kLogStopThread, 0, 0);
    
    require(param0, Fail);
    obj = OSDynamicCast(IrDAComm, (OSObject *)param0);
    require(obj, Fail);
    
    rc = obj->fGate->runAction(&DoSomething, (void *)cmdStopEvent, nil, nil, nil);      
    check(rc == kIOReturnSuccess);

Fail:
    return;
}
