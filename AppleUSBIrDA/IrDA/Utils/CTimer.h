/*
    File:       CTimer.h

*/

#ifndef __CTimer__
#define __CTimer__

#include "IrDATypes.h"
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTimerEventSource.h>
#include "IrDATimerEventSource.h"

class CTimer;

class CTimer : public OSObject
{
    OSDeclareDefaultStructors(CTimer);
    
public:
    typedef IrDATimerEventSource::Action    Action;         // defines CTimer::Action

    static CTimer *cTimer(IOWorkLoop *work, OSObject *owner, Action callback);  
    Boolean         init(IOWorkLoop *work, OSObject *owner, Action callback);
    void            free(void);
    
    void            StartTimer(TTimeout delay, UInt32 sig);     // ms delay if positive, else usec delay
    void            StopTimer(void);
    
    UInt32                  GetSignature(void);
    IrDATimerEventSource    *GetIOTimer(void);
    static void grim_reaper(thread_call_param_t param0, thread_call_param_t param1);

private:
    static void             timeout(OSObject *owner, IrDATimerEventSource *sender);
    UInt32                  fSig;
    IrDATimerEventSource    *fTimerSrc;     // the i/o kit timer
    IOWorkLoop              *fWorkLoop;     // workloop we run on
    OSObject                *fOwner;        // the owner (IrGlue usually)
    Action                  fCallback;      // the callback in our client
    thread_call_t           fGrimReaper;    // extra thread to nuke ourselves
    Boolean                 fBusy;          // true if timer is set
};

inline UInt32               CTimer::GetSignature() { return fSig; }
inline IrDATimerEventSource * CTimer::GetIOTimer()   { return fTimerSrc; }


#endif // __CTimer__
    
