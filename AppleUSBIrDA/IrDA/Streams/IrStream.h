/*
    File:       IrStream.h

    Contains:   Interface to the TIrStream abstract class


*/

#ifndef __IRSTREAM_H
#define __IRSTREAM_H

#include "IrDATypes.h"
#include "IrDALog.h"

class TIrEvent;
class CList;
class TIrGlue;


//--------------------------------------------------------------------------------
//      TIrStream
//--------------------------------------------------------------------------------
class TIrStream : public OSObject
{
	    OSDeclareAbstractStructors(TIrStream);
public:

	    Boolean     Init(TIrGlue *irda, EventTraceCauseDesc *trace = nil, UInt16 index = 0);
	    void        free(void);
	    
	    // Enqueue a request (response) to this stream 
	    IrDAErr         EnqueueEvent(TIrEvent *eventBlock);
    
	    // Subclasses implement this to get events queued for them
	    virtual void    NextState(ULong event) = 0;

	    // Run the queue until it's empty, only one queue for all stream objects
	    static void     RunQueue();
	    
	    // Return the current event
	    TIrEvent        *GetCurrentEvent();

protected:
	    TIrGlue         *fIrDA;                         // most stream objects need to get back to glue

private:
	    static void         DequeueEvent();             // get the next event to run from fNextEvent or the fifo

	    static TIrEvent     *fCurrentEvent;             // event we're running (not on the queue)
	    static TIrEvent     *fNextEvent;                // slight optimization to keep next event off the fifo
	    static CList        *fPendingEventsList;        // add to front, take from last, event FIFO
		    
#if (hasTracing > 0 && hasIrStreamTracing > 0)
	    EventTraceCauseDesc *   fTraceArray;            // Used by funky qtrace macro
	    UInt32                  fTraceIndex;            // one per IrStream object
#endif

}; // TIrStream

inline TIrEvent *TIrStream::GetCurrentEvent(void) { return fCurrentEvent; }


#endif  /*  __IRSTREAM_H    */
