/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

// public
#import <IOKit/firewire/IOFireWireController.h>

// protected
#import <IOKit/firewire/IOFWWorkLoop.h>
#import <IOKit/firewire/IOFireWireLink.h>

// private
#import "IOFWQEventSource.h"
#include <IOKit/firewire/IOFWUtils.h>

// system
#import <IOKit/IOTimerEventSource.h>

#import "FWTracepoints.h"

// createTimeoutQ
//
//

IOReturn IOFireWireController::createTimeoutQ( void )
{
    // Create Timer Event source and queue event source,
    // do before power management so the PM code can access the workloop
    fTimer = IOTimerEventSource::timerEventSource( this, clockTick );
    if(!fTimer)
	{
		return false;
	}
	
    fTimeoutQ.fTimer = fTimer;
	
	fWorkLoop->addEventSource( fTimer );
	
	return kIOReturnSuccess;
}

// destroyTimeoutQ
//
//

void IOFireWireController::destroyTimeoutQ( void )
{
	fWorkLoop->removeEventSource(fTimer);
    fTimer->release();
}

// headChanged
//
//

void IOFireWireController::timeoutQ::headChanged(IOFWCommand *oldHead)
{

#if 0
    {
        IOFWCommand *t = fHead;
        if(oldHead)
            IOLog("IOFireWireController::timeoutQ::headChanged(%s:%p)\n",
                oldHead->getMetaClass()->getClassName(), oldHead);
        else
            IOLog("IOFireWireController::timeoutQ::headChanged(0)\n");
            
        while(t) {
            AbsoluteTime d = t->getDeadline();
            IOLog("%s:%p deadline %llx\n",
                t->getMetaClass()->getClassName(), t, AbsoluteTime_to_scalar(&d));
            t = t->getNext();
        }
    }
#endif
    
	if(!fHead) 
	{
        //IOLog("timeoutQ empty\n");
        fTimer->cancelTimeout();
    }
    else 
	{
        fTimer->wakeAtTime(fHead->getDeadline());
        //AbsoluteTime now;
        //IOFWGetAbsoluteTime(&now);
        //IOLog("timeoutQ waketime %lx:%lx (now %lx:%lx)\n",
        //        fHead->getDeadline().hi, fHead->getDeadline().lo, now.hi, now.lo);
    }

}

// busReset
//
//

void IOFireWireController::timeoutQ::busReset()
{

#if 0
    {
        IOFWCommand *t = fHead;
        if(oldHead)
            IOLog("IOFireWireController::timeoutQ::headChanged(%s:%p)\n",
                oldHead->getMetaClass()->getClassName(), oldHead);
        else
            IOLog("IOFireWireController::timeoutQ::headChanged(0)\n");
            
        while(t) {
            AbsoluteTime d = t->getDeadline();
            IOLog("%s:%p deadline %lx:%lx\n",
                t->getMetaClass()->getClassName(), t, d.hi, d.lo);
            t = t->getNext();
        }
    }
#endif

    IOFWCommand *cmd;
    cmd = fHead;
    while(cmd) 
	{
        IOFWCommand *next;
        next = cmd->getNext();
        if(cmd->cancelOnReset()) 
		{
			FWTrace( kFWTController, kTPControllerTimeoutQBusReset, (uintptr_t)(cmd->getFWIMRefCon()), (uintptr_t)cmd, 0, 0 );
            cmd->cancel(kIOFireWireBusReset);
        }
        cmd = next;
    }
	
}

// clockTick
//
//

void IOFireWireController::clockTick(OSObject *obj, IOTimerEventSource *src)
{
    IOFireWireController *me = (IOFireWireController *)obj;

    // Check the list of pending commands
    me->processTimeout(src);
}

// processTimeout
//
//

void IOFireWireController::processTimeout(IOTimerEventSource *src)
{
    // complete() might take significant time, enough to cause
    // a later command to timeout too, so we loop here until there is no timeout.
    while (fTimeoutQ.fHead) 
	{
        AbsoluteTime now, dead;
        IOFWGetAbsoluteTime(&now);

#if 0
        IOLog("processTimeout, time is %llx\n", AbsoluteTime_to_scalar(&now));
        {
            IOFWCommand *t = fTimeoutQ.fHead;
            while(t) {
                AbsoluteTime d = t->getDeadline();
                IOLog("%s:%p deadline %lx:%lx\n",
                    t->getMetaClass()->getClassName(), t, AbsoluteTime_to_scalar(&d));
                t = t->getNext();
            }
        }
#endif
        dead = fTimeoutQ.fHead->getDeadline();
        if(CMP_ABSOLUTETIME(&dead, &now) == 1)
            break;	// Command with earliest deadline is OK.

        // Make sure there isn't a packet waiting.
        fFWIM->handleInterrupts( NULL, 1 );
		fFWIM->flushWaitingPackets();

        // Which may have changed the queue - see if earliest deadline has changed.
        if(!fTimeoutQ.fHead)
            break;

        if(CMP_ABSOLUTETIME(&dead, &fTimeoutQ.fHead->getDeadline()) != 0)
            continue;

        //IOLog("Cmd 0x%x timing out\r", fTimeoutQ.fHead);

		FWTrace( kFWTController, kTPControllerTimeoutQProcessTimeout, (uintptr_t)fFWIM, (uintptr_t)(fTimeoutQ.fHead), 0, 0 );
		
        fTimeoutQ.fHead->cancel(kIOReturnTimeout);
    };
    
	if(fTimeoutQ.fHead) 
	{
        src->wakeAtTime(fTimeoutQ.fHead->getDeadline());
        //AbsoluteTime now;
        //IOFWGetAbsoluteTime(&now);
        //IOLog("processTimeout, timeoutQ waketime %lx:%lx (now %llx)\n",
        //        fTimeoutQ.fHead->getDeadline().hi, fTimeoutQ.fHead->getDeadline().lo, AbsoluteTime_to_scalar(&now));
    }
    else 
	{
        //IOLog("processTimeout, timeoutQ empty\n");
        src->cancelTimeout();
    }
}
