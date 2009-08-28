/*
    File:       CTimer.cpp

    Contains:   Timer object


*/

#include "CTimer.h"
#include "IrDALog.h"

#if defined(hasTracing) && defined(hasCTimerTracing)

enum CTimerTraceCodes
{
    kLogNew = 1,
    kLogInit,
    kLogFree1,
    kLogFree2,
    kStartTimerDelay,
    kStartTimerSignal,
    
    kLogStopTimer1,
    kLogStopTimer2,
    kLogStopTimer3,
    kLogStopTimer4,
    kLogStopTimer5,
    
    kLogTimeout1,
    kLogTimeout2,
    kLogTimeout3,
    kLogTimeout4,
    kLogTimeout5,
    kLogGrimReaper
};

static
EventTraceCauseDesc TraceEvents[] = {
    {kLogNew,                   "CTimer: new, obj="},
    {kLogInit,                  "CTimer: init, fTimerSrc="},
    {kLogFree1,                 "CTimer: free, obj="},
    {kLogFree2,                 "CTimer: free, fTimerSrc="},
    {kStartTimerDelay,          "CTimer: Start timer, delay="},
    {kStartTimerSignal,         "CTimer: Start timer, signal=, new retain="},
    
    {kLogStopTimer1,            "CTimer: Stop timer, obj="},
    {kLogStopTimer2,            "CTimer: Stop timer, idle so nop.  retain stays="},
    {kLogStopTimer3,            "CTimer: Stop timer, cancel worked, retain before release="},
    {kLogStopTimer4,            "CTimer: Stop timer, cancel failed, retain still="},
    {kLogStopTimer5,            "CTimer: Stop timer, exit, retain count="},
    
    {kLogTimeout1,              "CTimer: timeout called, obj="},
    {kLogTimeout2,              "CTimer: timeout called, retain="},
    {kLogTimeout3,              "CTimer: timeout called, owner="},
    {kLogTimeout4,              "CTimer: timeout called, sender="},
    {kLogTimeout5,              "CTimer: timeout back from calling owner"},
    {kLogGrimReaper,            "CTimer: grim reaper"}
    
};
    #define XTRACE(x, y, z) IrDALogAdd ( x, y, (uintptr_t)z & 0xffff, TraceEvents, true )
#else
    #define XTRACE(x, y, z) ((void)0)
#endif


#define super OSObject
    OSDefineMetaClassAndStructors(CTimer, OSObject);
    
CTimer *
CTimer::cTimer(IOWorkLoop *work, OSObject *owner, Action callback)
{
    CTimer * obj = new CTimer;
    
    XTRACE(kLogNew, 0, obj);

    if (obj && !obj->init(work, owner, callback)) {
	obj->release();
	obj = nil;
    }
    return obj;
}

Boolean CTimer::init(IOWorkLoop *work, OSObject *owner, Action callback)
{
    IOReturn rc;
    
    fTimerSrc = nil;
    
    require(work, Failed);
    require(owner, Failed);
    require(callback, Failed);
	
    if (!super::init()) return false;
    
    fOwner    = owner;                  // save backpointer
    fCallback = callback;               // save the real callback, we're going to get it first
    fWorkLoop = work;                   // save workloop for free later
//  fTimerSrc = IOTimerEventSource::timerEventSource(owner, callback);      // make a timer
    fTimerSrc = IrDATimerEventSource::timerEventSource(this, &CTimer::timeout);
    require(fTimerSrc, Failed);
    
    fBusy = false;
    
    fGrimReaper = thread_call_allocate(grim_reaper, this);
    require(fGrimReaper, Failed);
	
    rc = work->addEventSource(fTimerSrc);
    require(rc == kIOReturnSuccess, Failed);
    
    XTRACE(kLogInit, 0, fTimerSrc);
    return true;
	    
Failed:
    return false;
}

//
// free - this should now be impossible to have invoked while a timer is pending.
// 

void CTimer::free()
{
    XTRACE(kLogFree1, 0, this);
    Boolean ok;
    
    check(fBusy == false);
    
    if (fGrimReaper) {
	ok = thread_call_cancel(fGrimReaper);
	check(ok == false);         // should *never* have work!
	thread_call_free(fGrimReaper);
	fGrimReaper = nil;
    }
    
    if (fTimerSrc) {
	XTRACE(kLogFree2, 0, fTimerSrc);
	ok = fTimerSrc->SafeCancelTimeout();
	check(ok == false);         // should *not* have worked (no timer pending)
	
	fWorkLoop->removeEventSource(fTimerSrc);
	
	fTimerSrc->release();
	fTimerSrc = nil;
    }
	
    super::free();
}

//
// Start a timer to fire up after a while.
// Note the retain on self to prevent zombie
// timers getting invoked after they've been freed.
//

void CTimer::StartTimer(TTimeout delay, UInt32 sig)
{
    IOReturn rc;
    
    require(fTimerSrc, Fail);

    XTRACE (kStartTimerDelay,  delay>>16, delay);

    if (fBusy) StopTimer();			// cleanup
    require(fBusy == false, Fail);                  // sanity
    
    fBusy = true;                                   // mark timer as in-use
    this->retain();                                 // retain to prevent dangling
	
    fSig = sig;
    
    if (delay < 0)                                  // microsecond timer
	rc = fTimerSrc->setTimeoutUS(-delay);
    else
	rc = fTimerSrc->setTimeoutMS(delay);        // millisecond timer
	
    check(rc == kIOReturnSuccess);
    XTRACE (kStartTimerSignal, sig, this->getRetainCount());
    
Fail:
    return;
}

void CTimer::StopTimer()
{
    Boolean ok;
    XTRACE(kLogStopTimer1, 0, this);
    
    if (fBusy == false) {                       // stopped, but we're already stopped (nop)
	XTRACE(kLogStopTimer2, 0, this->getRetainCount());
	return;
    }
    
    if (fTimerSrc) {
	ok = fTimerSrc->SafeCancelTimeout();    // returns ok if it worked
	if (ok) {                               // if cancel timer worked, then
	    XTRACE(kLogStopTimer3, 0, this->getRetainCount());
	    fBusy = false;                      // mark timer as available and                  
	    this->release();                    // we can release self safely
	} else {
	    XTRACE(kLogStopTimer4, 0, this->getRetainCount());
	}
    }
    XTRACE(kLogStopTimer5, 0, this->getRetainCount());
    return;
}

void
CTimer::timeout(OSObject *owner, IrDATimerEventSource *sender)
{
    CTimer  *obj;
    
    XTRACE(kLogTimeout1, 0, owner);
    obj = OSDynamicCast(CTimer, owner);
    require(obj, Fail);
    require(obj->fOwner, Fail);
    require(obj->fCallback, Fail);
    require(obj->fBusy, Fail);
    
    XTRACE(kLogTimeout2, 0, obj->getRetainCount());
    
    if (obj->getRetainCount() == 1) {           // if it's down to one, then we're been deleted but for our start timer
	thread_call_enter1(obj->fGrimReaper, 0);
	return;
    }
    obj->fBusy = false;
    obj->release(); 
    // temp debugging
    XTRACE(kLogTimeout3, 0, obj->fOwner);
    XTRACE(kLogTimeout4, 0, sender);
    (*obj->fCallback)(obj->fOwner, sender);
    XTRACE(kLogTimeout5, 0xffff, 0xffff);
    
Fail:
    return;
}

/*static*/
void CTimer::grim_reaper(thread_call_param_t param0, thread_call_param_t param1)
{
    CTimer *obj;
    
    XTRACE(kLogGrimReaper, 0x1111, 0x1111);
    
    obj = OSDynamicCast(CTimer, (OSObject *)param0);
    require(obj, Fail);
    require(obj->fBusy, Fail);
    
    obj->fBusy = false;
    obj->release();

    XTRACE(kLogGrimReaper, 0xffff, 0xffff);
    return;
    
Fail:
    XTRACE(kLogGrimReaper, 0xdead, 0xbeef);
    return;
}
