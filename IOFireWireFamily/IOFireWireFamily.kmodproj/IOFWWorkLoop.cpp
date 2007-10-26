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
    
    loop = new IOFWWorkLoop;
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
	// create a unique lock group for this instance of the FireWire workloop
	// this helps elucidate lock statistics
	
	SInt32	count = OSIncrementAtomic( &sLockGroupCount );
	char	name[64];
	
	snprintf( name, sizeof(name), "FireWire %d", (int)count );
	fLockGroup = lck_grp_alloc_init( name, LCK_GRP_ATTR_NULL );
	if( fLockGroup )
	{
		gateLock = IORecursiveLockAllocWithLockGroup( fLockGroup );
	}
	
	return IOWorkLoop::init();
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
	
	IOWorkLoop::free();	
}

// closeGate
//
//

void IOFWWorkLoop::closeGate()
{
    IOWorkLoop::closeGate();
    if( fSleepToken ) 
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
    if( ret && fSleepToken ) 
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
    wakeupGate( token, false );
    
	return kIOReturnSuccess;
}
