/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
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
