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
 * Copyright (c) 2002 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Tom Sherman
 *
 */

#include "AppleKeyswitch.h"

#define super IOService
OSDefineMetaClassAndStructors(AppleKeyswitch, IOService);

bool AppleKeyswitch::start(IOService *provider)
{
    UInt8 		val;
    OSData 		*tempOSData;
	IOReturn	status;

	DLOG("AppleKeyswitch::start entered.\n");

    if (!super::start(provider))
        return false;

    myWorkLoop = (IOWorkLoop *)getWorkLoop();
	if(myWorkLoop == NULL)
    {     
        IOLog("AppleKeyswitch::start failed to get WorkLoop.\n");
		return false;
    }
    
    interruptSource = IOInterruptEventSource::interruptEventSource(this, (IOInterruptEventAction)&AppleKeyswitch::interruptOccurred, provider, 0);
    if (interruptSource == NULL)
    {     
        IOLog("AppleKeyswitch::start failed to create interrupt event source.\n");
		return false;
    }

    status = myWorkLoop->addEventSource(interruptSource);
    if (status != kIOReturnSuccess)
    {
        IOLog("AppleKeyswitch::start failed to add interrupt event source to work loop.\n");
		return false;
    }

    // callPlatformFunction symbols
    keyLargo_safeWriteRegUInt8 = OSSymbol::withCString("keyLargo_safeWriteRegUInt8");
    keyLargo_safeReadRegUInt8 = OSSymbol::withCString("keyLargo_safeReadRegUInt8");

    // Get ExtIntGPIO where interrupt is coming in.
    tempOSData = OSDynamicCast(OSData, provider->getProperty("reg"));
	if (tempOSData == NULL)
    {
        IOLog("AppleKeyswitch::start failed to get 'reg' property.\n");     
		return false;
    }
    
    extIntGPIO = (UInt32*)tempOSData->getBytesNoCopy();

    // Configure the ExtIntGPIO for interrupts
    callPlatformFunction(keyLargo_safeWriteRegUInt8, false, (void *)(kKeyLargoGPIOBaseAddr + *extIntGPIO), (void *)0xFF, (void *)0x80, (void *)0);

    IOSleep(100);
    
    // Read initial keyswitch state
    callPlatformFunction(keyLargo_safeReadRegUInt8, false, (void *)(kKeyLargoGPIOBaseAddr + *extIntGPIO), (void *)&val, (void *)0, (void *)0);
    val = (val&0x2)>>1;

    if(val)
    {
        state = 0;
        setProperty("Keyswitch", false);
    }
    else
    {
        state = 1;
        setProperty("Keyswitch", true);
    }
    
    // Enable interrupt
    interruptSource->enable(); 

    registerService();

    return true;
}

void AppleKeyswitch::stop(IOService *provider)
{
    interruptSource->disable();
    myWorkLoop->removeEventSource(interruptSource);

    if (interruptSource != NULL)
    {
        interruptSource->release();
        interruptSource = NULL;
    }

    if (myWorkLoop != NULL)
    {
        myWorkLoop->release();
        myWorkLoop = NULL;
    }
    
    super::stop(provider);
}

void AppleKeyswitch::interruptOccurred(OSObject* obj, IOInterruptEventSource * source, int count)
{
    AppleKeyswitch *AppleKeyswitchPtr = (AppleKeyswitch *)obj;

    if (AppleKeyswitchPtr != NULL)
        AppleKeyswitchPtr->toggle(false);
    
    return;
}

void AppleKeyswitch::toggle(bool disableInts = false)
{
    UInt8 				i, val, total;

    val = total = 0;

    // Disable KeyLargo ExtInt_GPIO4 interrupts
    if (disableInts)
        interruptSource->disable(); 
    
    // Get 100 samples and determine state of keyswitch (debounce)
    for(i=0; i<=100; i++)
    {
        // get 100 samples
        callPlatformFunction(keyLargo_safeReadRegUInt8, false, (void *)(kKeyLargoGPIOBaseAddr + *extIntGPIO), (void *)&val, (void *)0, (void *)0);
        val = (val&0x2)>>1;
        total += val;
        IOSleep(1);
    }

    if(total>80)
        state = 1;            

    if(total<20)
        state = 0;

    if (state)
    {	
        state = 0;
      
        // set Keyswitch property and registerService
        setProperty("Keyswitch", false);
        registerService();
    }
    else
    {
        state = 1;
        
        // set Keyswitch property and registerService
        setProperty("Keyswitch", true);
        registerService();
    }
   
    // Enable KeyLargo ExtInt_GPIO4 interrupts
    if (disableInts)
        interruptSource->enable(); 

    return;
}
