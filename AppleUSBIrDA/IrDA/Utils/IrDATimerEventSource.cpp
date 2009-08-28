
#include <IOKit/IOLib.h>
#include "IrDATimerEventSource.h"

#include "IrDALog.h"
#include "IrDADebugging.h"

// todo: merge with ctimer object

#if (hasTracing > 0) 

enum IrLogCodes
{
    kLogNew = 1,
    kLogFree,
    kLogCancel
};

static
EventTraceCauseDesc IrLogEvents[] = {
    {kLogNew,               "IrDATimer: new, obj="},
    {kLogFree,              "IrDATimer: free, obj="},
    {kLogCancel,            "IrDATimer: safe cancel, cancel worked="}
};

#define XTRACE(x, y, z) IrDALogAdd( x, y, (uintptr_t)z & 0xffff, IrLogEvents, true )

#else
#define XTRACE(x,y,z)   ((void)0)
#endif

#define super IOTimerEventSource

    OSDefineMetaClassAndStructors(IrDATimerEventSource, IOTimerEventSource);
    
//
// returns true if the cancel worked, else it
// wasn't found to remove.
//
Boolean IrDATimerEventSource::SafeCancelTimeout()
{
    Boolean cancel_ok;
    
    AbsoluteTime_to_scalar(&abstime) = 0;
    // thread_call_cancel returns true if it was able to dequeue the thread
    cancel_ok = thread_call_cancel((thread_call_t) calloutEntry);
    XTRACE(kLogCancel, 0, cancel_ok);
    return cancel_ok;
}

/* static */
IrDATimerEventSource *
IrDATimerEventSource::timerEventSource(OSObject *inOwner, Action inAction)
{
    IrDATimerEventSource *me = new IrDATimerEventSource;
    XTRACE(kLogNew, 0, me);
    
    // note: we don't have an init, so pass it up to IOTimerEventSource
    if (me && !me->init(inOwner, (super::Action)inAction)) {
	me->free();
	return 0;
    }

    return me;
}
