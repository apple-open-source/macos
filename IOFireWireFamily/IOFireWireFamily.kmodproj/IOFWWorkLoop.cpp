/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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
 * 13 February 2001 wgulland created.
 *
 */

#define DEBUGGING_LEVEL 0	// 1 = low; 2 = high; 3 = extreme
#define DEBUGLOG IOLog

// protected
#import <IOKit/firewire/IOFWWorkLoop.h>

// system
#import <IOKit/IOWorkLoop.h>
#import <IOKit/IOLocksPrivate.h>

SInt32 IOFWWorkLoop::sLockGroupCount = 0;

OSDefineMetaClassAndStructors( IOFWWorkLoop, IOWorkLoop )

// workLoop
//
// factory method

IOFWWorkLoop * IOFWWorkLoop::workLoop()
{
    IOFWWorkLoop *loop;
    
    loop = OSTypeAlloc( IOFWWorkLoop );
    if( !loop )
        return loop;
		
    if( !loop->init() ) 
	{
        loop->release();
        loop = NULL;
    }
	
    return loop;
}

// init
//
//

bool IOFWWorkLoop::init( void )
{
	bool success = true;
	
	if( success )
	{
		// create a unique lock group for this instance of the FireWire workloop
		// this helps elucidate lock statistics
		
		SInt32	count = OSIncrementAtomic( &sLockGroupCount );
		char	name[64];
		
		snprintf( name, sizeof(name), "FireWire %d", (int)count );
		fLockGroup = lck_grp_alloc_init( name, LCK_GRP_ATTR_NULL );
		if( !fLockGroup )
		{
			success = false;
		}
	}
	
	if( success )
	{
		gateLock = IORecursiveLockAllocWithLockGroup( fLockGroup );
	}
	
	if( success )
	{
		fRemoveSourceDeferredSet = OSSet::withCapacity( 1 );
		if( fRemoveSourceDeferredSet == NULL  )
		{
			success = false;
		}
	}

	if( success )
	{
		success = IOWorkLoop::init();
	}
	
	return success;
}

// free
//
//

void IOFWWorkLoop::free( void )
{
	if( fLockGroup )
	{
		lck_grp_free( fLockGroup );
		fLockGroup = NULL;
	}
	
	if( fRemoveSourceDeferredSet )
	{
		fRemoveSourceDeferredSet->release();
		fRemoveSourceDeferredSet = NULL;
	}
	
	IOWorkLoop::free();	
}

// removeEventSource
//
//

IOReturn IOFWWorkLoop::removeEventSource(IOEventSource *toRemove)
{
	IOReturn status = kIOReturnSuccess;
	
	// the PM thread retains ioservices while fiddling with them
	// that means the PM thread may be the thread that releases the final retain on an ioservices
	// ioservices may remove and free event sources in their free routines
	// removing and freeing event sources grabs the workloop lock
	// if the PM has already put FireWire to sleep we would sleep any thread who grabs the workloop lock
	// if we sleep the PM thread then sleep hangs. that's bad.
	
	// if we could have a do over we should not sleep the entire workloop, but only those that belong
	// to the core FireWire services. at this point though there are likely too many drivers relying
	// on a full workloop sleep to change things safely. 
	
	// so for now we do these slightly crazy machinations to allow event source removal without sleeping the
	// calling thread
	
	// we only need to do this if the calling thread is the PM workloop, but I don't want to make
	// assumptions about PM internals that may change so I'll do this for all threads
	
	IOWorkLoop::closeGate();
	
	if( fRemoveSourceThread != NULL )
	{
		IOLog( "IOFWWorkLoop::removeEventSource - fRemoveSourceThread = (%p) != NULL\n", fRemoveSourceThread );
	}
	
	// remember who's removing the event source
	fRemoveSourceThread = IOThreadSelf();
	
	// if we're asleep
	if( fSleepToken )
	{
		// we can't let this object be freed after we return since freeing a command gate grabs the workloop lock
		// we will flush this set on wake
		fRemoveSourceDeferredSet->setObject( toRemove );
	}
	
	// do the actual removal, this will succeed since fRemoveSourceThread will be allowed to grab the lock
	status = IOWorkLoop::removeEventSource( toRemove );
	
	// forget the thread
	fRemoveSourceThread = NULL;
	
	IOWorkLoop::openGate();
	
	return status;
}

// closeGate
//
//

void IOFWWorkLoop::closeGate()
{
    IOWorkLoop::closeGate();
    if( fSleepToken && 
	    (fRemoveSourceThread != IOThreadSelf()) ) 
	{
        IOReturn res;
        do 
		{
            res = sleepGate( fSleepToken, THREAD_ABORTSAFE );
            if( res == kIOReturnSuccess )
                break;
            IOLog("sleepGate returned 0x%x\n", res);
        } 
		while( true );
    }
}

// tryCloseGate
//
//

bool IOFWWorkLoop::tryCloseGate()
{
    bool ret;
    ret = IOWorkLoop::tryCloseGate();
    if( ret && 
	    fSleepToken && 
	    (fRemoveSourceThread != IOThreadSelf()) ) 
	{
        openGate();
        ret = false;
    }
	
    return ret;
}

// sleep
//
//

IOReturn IOFWWorkLoop::sleep(void *token)
{
    if( fSleepToken )
	{
        DEBUGLOG( "IOFWWorkLoop::sleep: Already asleep: %p\n", token );
        return kIOReturnError;
    }
	
    fSleepToken = token;
    openGate();
    
	return kIOReturnSuccess;
}

// wake
//
//

IOReturn IOFWWorkLoop::wake(void *token)
{
    if( fSleepToken != token ) 
	{
        DEBUGLOG( "IOFWWorkLoop::wake: wrong token: %p<->%p\n", token, fSleepToken );
        return kIOReturnError;
    }
	
    IORecursiveLockLock( gateLock );
	
    fSleepToken = NULL;
	
	// delete any event sources that were removed during sleep
	fRemoveSourceDeferredSet->flushCollection();
	
	// wake up waiting threads
    wakeupGate( token, false );
    
	return kIOReturnSuccess;
}
