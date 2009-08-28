
#ifndef _IRDATIMEREVENTSOURCE
#define _IRDATIMEREVENTSOURCE

#include <IOKit/IOTimerEventSource.h>

// subclass from standard IOTimerEventSource to
// add a workaround for cancelTimer not being
// reliable in the face of workloop gates.

class IrDATimerEventSource : public IOTimerEventSource
{
    OSDeclareDefaultStructors(IrDATimerEventSource)

public:
    typedef void (*Action)(OSObject *owner, IrDATimerEventSource *sender);

    static IrDATimerEventSource *timerEventSource(OSObject *inOwner, Action inAction);
    Boolean SafeCancelTimeout();
};

#endif /* !_IRDATIMEREVENTSOURCE */
