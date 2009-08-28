/*
 * Copyright (c) 1998-2007 Apple Computer, Inc. All rights reserved.
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
/* IOFWSyncer.cpp created by wgulland on 2000-02-02 */

#include <IOKit/IOLib.h>

#include <IOKit/firewire/IOFWSyncer.h>

OSDefineMetaClassAndStructors(IOFWSyncer, OSObject)

IOFWSyncer * IOFWSyncer::create(bool twoRetains)
{
    IOFWSyncer * me = OSTypeAlloc( IOFWSyncer );

    if (me && !me->init(twoRetains)) {
        me->release();
        return 0;
    }

    return me;
}

bool IOFWSyncer::init(bool twoRetains)
{
    if (!OSObject::init())
        return false;

    if (!(guardLock = IOSimpleLockAlloc()) )
        return false;
	
    IOSimpleLockInit(guardLock);

    if(twoRetains)
	retain();

    fResult = kIOReturnSuccess;

    reinit();

    return true;
}

void IOFWSyncer::reinit()
{
    IOInterruptState is = IOSimpleLockLockDisableInterrupt(guardLock);
    threadMustStop = true;
    IOSimpleLockUnlockEnableInterrupt(guardLock, is);
}

void IOFWSyncer::free()
{
    // just in case a thread is blocked here:
    privateSignal();

    if (guardLock != NULL)
       IOSimpleLockFree(guardLock);

    OSObject::free();
}

IOReturn IOFWSyncer::wait(bool autoRelease)
{
    IOInterruptState is = IOSimpleLockLockDisableInterrupt(guardLock);

    if (threadMustStop) {
	assert_wait((void *) &threadMustStop, false);
    	IOSimpleLockUnlockEnableInterrupt(guardLock, is);
        thread_block(THREAD_CONTINUE_NULL);
    }
    else
        IOSimpleLockUnlockEnableInterrupt(guardLock, is);

    IOReturn result = fResult;	// Pick up before auto deleting!

    if(autoRelease)
	release();

    return result;
}

void IOFWSyncer::signal(IOReturn res, bool autoRelease)
{
    fResult = res;
    privateSignal();
    if(autoRelease)
	release();
}

void IOFWSyncer::privateSignal()
{
    if (threadMustStop) {
         IOInterruptState is = IOSimpleLockLockDisableInterrupt(guardLock);
         threadMustStop = false;
         thread_wakeup_one((void *) &threadMustStop);
         IOSimpleLockUnlockEnableInterrupt(guardLock, is);
    }
}
