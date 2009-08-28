/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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

/*
 * Copyright (c) 1999-2002 Apple Computer, Inc.  All rights reserved.
 *
 * HISTORY
 * 27 May 99 wgulland created.
 *
 */

//#define IOASSERT 1	// Set to 1 to activate assert()

// public
#include <IOKit/firewire/IOFWCommand.h>
#include <IOKit/firewire/IOFireWireController.h>
#include <IOKit/firewire/IOFireWireNub.h>
#include <IOKit/firewire/IOLocalConfigDirectory.h>

// protected
#include <IOKit/firewire/IOFireWireLink.h>
#include <IOKit/firewire/IOFWUtils.h>

// system
#include <IOKit/assert.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommand.h>

OSDefineMetaClass( IOFWCommand, IOCommand )
OSDefineAbstractStructors(IOFWCommand, IOCommand)
OSMetaClassDefineReservedUsed(IOFWCommand, 0);
OSMetaClassDefineReservedUnused(IOFWCommand, 1);

#pragma mark -

// initWithController
//
//

bool IOFWCommand::initWithController(IOFireWireController *control)
{
	bool success = true;
	
	success = IOCommand::init();
	
	if( success )
	{
		fControl = control;
		fMembers = (IOFWCommand::MemberVariables*)IOMalloc( sizeof(MemberVariables) );
		if( fMembers == NULL )
			success = false;
	}
	
	// zero member variables
	
	if( success )
	{
		bzero( fMembers, sizeof(MemberVariables) );
		fMembers->fFlush = true;
	}
	
	return success;
}

// free
//
//

void IOFWCommand::free()
{	
	if( fMembers != NULL )
	{		
		// free member variables
		
		IOFree( fMembers, sizeof(MemberVariables) );
		fMembers = NULL;
	}
	
	IOCommand::free();
}

// submit
//
//

IOReturn IOFWCommand::submit(bool queue)
{
    IOReturn res;
    
//	IOLog( "IOFWCommand::submit\n" );
	
	IOWorkLoop * workLoop = fControl->getWorkLoop();
    if(workLoop->onThread() && fSync) {
        IOLog("Potential FireWire workloop deadlock!\n");
        IOLog("Naughty cmd is a %s\n", getMetaClass()->getClassName());
    }
    
    if(fSync) {
        fSyncWakeup = IOFWSyncer::create();
        if(!fSyncWakeup)
            return kIOReturnNoMemory;
    }
    
	// on an error path startExecution may release this 
	// command so we will retain it here
	
	retain();
	
	fControl->closeGate();
	IOFWCommand::fMembers->fSubmitTimeLatched = false;
    if( queue ) 
	{
        IOFWCmdQ &pendingQ = fControl->getPendingQ();
        IOFWCommand *prev = pendingQ.fTail;
        if( !prev ) 
		{
            setHead( pendingQ );
        }
        else 
		{
            insertAfter( *prev );
        }
        res = fStatus = kIOFireWirePending;
    }
    else 
	{
        res = fStatus = startExecution();
    }
    fControl->openGate();

    if(res == kIOReturnBusy || res == kIOFireWirePending)
        res = kIOReturnSuccess;
    if(fSync) 
	{
		if(res == kIOReturnSuccess)
		{
			res = fSyncWakeup->wait();
#if 0
			if (res) IOLog("%s %u: fSyncWakeup->wait returned %x\n", __FILE__, __LINE__, res) ;
#endif
		}
		else
		{
			fSyncWakeup->release();
			fSyncWakeup = NULL;
		}
	}

//	IOLog( "IOFWCommand::submit - res = 0x%08lx\n", res );
	
	if( fMembers->fFlush )
	{
		fControl->closeGate();

		fControl->fFWIM->flushWaitingPackets();
		
		fControl->openGate();
	}
	
	release();
	
    return res;
}

// startExecution
//
//

IOReturn IOFWCommand::startExecution()
{
	// latch the very first time we start to work on this command
	// so we can abort if the process takes to long
	if( !IOFWCommand::fMembers->fSubmitTimeLatched )
	{
		IOFWCommand::fMembers->fSubmitTimeLatched = true;
		IOFWGetAbsoluteTime( &(IOFWCommand::fMembers->fSubmitTime) );	// remember when we started
	}
	
	updateTimer();
    return execute();
}

// checkProgress
//
//

IOReturn IOFWCommand::checkProgress( void )
{
	IOReturn status = kIOReturnSuccess;		// all is well
	
	return status;
}

// complete
//
//

IOReturn IOFWCommand::complete(IOReturn status)
{
    // Remove from current queue
    removeFromQ();
    return fStatus = status;
}

// cancel
//
//

IOReturn IOFWCommand::cancel(IOReturn reason)
{
    IOReturn result = kIOReturnSuccess;
	
	// complete may release this command so we retain it here
	
	retain();
    
	fControl->closeGate();
    
	result = complete(reason);
    
	fControl->openGate();
    
	release();
	
	return result;
}

#pragma mark -

// setHead
//
//

void IOFWCommand::setHead( IOFWCmdQ &queue )
{
    IOFWCommand *oldHead;
    
	assert(fQueue == NULL);
    
	oldHead = queue.fHead;
    queue.fHead = this;
    fQueue = &queue;
    fQueuePrev = NULL;
    fQueueNext = oldHead;
    
	if(!oldHead)
        queue.fTail = this;
    else
        oldHead->fQueuePrev = this;
    
	queue.headChanged(oldHead);		// Tell queue about change
}

// insertAfter
//
//

void IOFWCommand::insertAfter( IOFWCommand &prev )
{
    IOFWCommand *next;
    
	assert(fQueue == NULL);
    
	next = prev.fQueueNext;
    fQueue = prev.fQueue;
    prev.fQueueNext = this;
    fQueuePrev = &prev;
    fQueueNext = next;
    
	if(!next)
        fQueue->fTail = this;
    else
        next->fQueuePrev = this;
}

// removeFromQ
//
//

void IOFWCommand::removeFromQ()
{
    // Remove from queue
    if(fQueue) 
	{
        IOFWCmdQ *queue = fQueue;
        IOFWCommand *oldHead = queue->fHead;
        
		if(fQueuePrev) 
		{
            assert(fQueuePrev->fQueueNext == this);
            fQueuePrev->fQueueNext = fQueueNext;
        }
        else 
		{
            // First in list.
            assert(queue->fHead == this);
            queue->fHead = fQueueNext;
        }
        
		if(fQueueNext) 
		{
            assert(fQueueNext->fQueuePrev == this);
            fQueueNext->fQueuePrev = fQueuePrev;
        }
        else 
		{
            // Last in list.
            assert(queue->fTail == this);
            queue->fTail = fQueuePrev;
        }
        
		fQueue = NULL;
        
		if(oldHead == this) 
		{
            queue->headChanged(this);
        }
    }
}

// updateTimer
//
//

void IOFWCommand::updateTimer()
{
    if(fTimeout) 
	{
        AbsoluteTime delta;
        clock_interval_to_absolutetime_interval(fTimeout, kMicrosecondScale, &delta);
        IOFWGetAbsoluteTime(&fDeadline);
        ADD_ABSOLUTETIME(&fDeadline, &delta);
        if(fQueue) 
		{
            IOFWCommand *oldHead = fQueue->fHead;
            IOFWCommand *next;
    
            // Now move command down list to keep list sorted
            next = fQueueNext;
            while(next) 
			{
                if(CMP_ABSOLUTETIME(&next->fDeadline, &fDeadline) == 1)
                    break;	// Next command's deadline still later than new deadline.
                next = next->fQueueNext;
            }
			
            if(next != fQueueNext) 
			{
                // Move this command from where it is to just before 'next'
                IOFWCommand *prev;
                
				if(fQueuePrev) 
				{
                    assert(fQueuePrev->fQueueNext == this);
                    fQueuePrev->fQueueNext = fQueueNext;
                }
                else 
				{
                    // First in list.
                    assert(fQueue->fHead == this);
                    fQueue->fHead = fQueueNext;
                }
                
				assert(fQueueNext);	// Can't be last already!
                assert(fQueueNext->fQueuePrev == this);
                fQueueNext->fQueuePrev = fQueuePrev;
    
                if(!next) 
				{
                    prev = fQueue->fTail;
                    fQueue->fTail = this;
                }
                else 
				{
                    prev = next->fQueuePrev;
                    next->fQueuePrev = this;
                }
    
                assert(prev);	// Must be a command to go after
                prev->fQueueNext = this;
                fQueuePrev = prev;
                fQueueNext = next;
            }
			
            // if the command was at the head, then either:
            // 1) it still is, but with a new, later, deadline
            // 2) it isn't, another command now is.
            // Either way, need to update the clock timeout.
            if(oldHead == this) {
                fQueue->headChanged(this);
            }
        }
        else 
		{
            // Not already on timeout queue
            IOFWCommand *prev;
            IOFWCmdQ &timeoutQ = fControl->getTimeoutQ();
            // add cmd to right place in list, which is sorted by deadline
            prev = timeoutQ.fTail;
            while(prev) 
			{
                if(CMP_ABSOLUTETIME(&prev->getDeadline(), &fDeadline) != 1)
                    break; // prev command's deadline is before new one, so insert here.
                prev = prev->getPrevious();
            }
    
            if(!prev) 
			{
                setHead(timeoutQ);
            }
            else 
			{
                insertAfter(*prev);
                /** DUMP Q **/
#if 0
                {
                    AbsoluteTime now, dead;
                    IOFWGetAbsoluteTime(&now);
                    IOLog("%s: insertAfter %s, time is %llx\n",
                        getMetaClass()->getClassName(), prev->getMetaClass()->getClassName(), AbsoluteTime_to_scalar(&now) );
                    {
                        IOFWCommand *t = timeoutQ.fHead;
                        while(t) 
						{
                            AbsoluteTime d = t->getDeadline();
                            IOLog("%s:%p deadline %llx\n",
                                t->getMetaClass()->getClassName(), t, AbsoluteTime_to_scalar(&d) );
                            t = t->getNext();
                        }
                    }
                }
#endif
            }
        }
    }
}
