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
    
