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

#define XTRACE(x, y, z) IrDALogAdd( x, y, z, IrLogEvents, true )

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
    XTRACE(kLogNew, (int)me >> 16, (short)me);
    
    // note: we don't have an init, so pass it up to IOTimerEventSource
    if (me && !me->init(inOwner, (super::Action)inAction)) {
	me->free();
	return 0;
    }

    return me;
}
