/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All Rights Reserved.
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


#define DEBUGGING_LEVEL 0	// 1 = low; 2 = high; 3 = extreme
#define DEBUGLOG IOLog

#include <IOKit/IOWorkLoop.h>
#include <IOKit/usb/IOUSBWorkLoop.h>

#define super IOWorkLoop
OSDefineMetaClassAndStructors( IOUSBWorkLoop, IOWorkLoop )

IOUSBWorkLoop * IOUSBWorkLoop::workLoop(const char * controllerLocation)
{
    IOUSBWorkLoop *loop;
    
    loop = new IOUSBWorkLoop;
    if (!loop)
        return loop;
	
    if (!loop->init(controllerLocation)) 
    {
        loop->release();
        loop = NULL;
    }
    return loop;
}

bool
IOUSBWorkLoop::init (const char * controllerLocation)
{
#pragma unused(controllerLocation)
	
	return super::init( );
	
}

void IOUSBWorkLoop::free ( void )
{
	
	
	super::free ( );
	
}

void IOUSBWorkLoop::closeGate()
{
    IOWorkLoop::closeGate();
    // do not do this if we are on the actual workloop/interrupt thread
    if (fSleepToken ) 
    {
        IOReturn res;
		if (onThread())
		{
			IOLog("IOUSBWorkLoop::closeGate - interrupt Thread being held off");
		}
        do 
        {
            res = sleepGate(fSleepToken, THREAD_ABORTSAFE);
            if (res == kIOReturnSuccess)
                break;
            IOLog("sleepGate returned 0x%x\n", res);
        } while (true);
    }
}

bool IOUSBWorkLoop::tryCloseGate()
{
    bool ret;
    ret = IOWorkLoop::tryCloseGate();
    if (ret && fSleepToken) 
    {
        openGate();
        ret = false;
    }
    return ret;
}

IOReturn IOUSBWorkLoop::sleep(void *token)
{
    if (fSleepToken) 
    {
        DEBUGLOG("IOUSBWorkLoop::sleep: Already asleep: %p\n", token);
        return kIOReturnError;
    }
    fSleepToken = token;
    openGate();
    return kIOReturnSuccess;
}

IOReturn 
IOUSBWorkLoop::wake(void *token)
{
    if (fSleepToken != token) 
    {
        DEBUGLOG("IOUSBWorkLoop::wake: wrong token: %p<->%p\n", token, fSleepToken);
        return kIOReturnError;
    }
    IORecursiveLockLock(gateLock);
    fSleepToken = NULL;
    wakeupGate(token, false);
    return kIOReturnSuccess;
}

void  
IOUSBWorkLoop::CloseGate(void) 
{ 
    closeGate(); 
}

void  
IOUSBWorkLoop::OpenGate(void)  
{ 
    openGate(); 
}

