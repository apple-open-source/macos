/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 */

#include <IOKit/IOLib.h>
#include "MacRISC2CPU.h"
#include "Portable2004_UserClient.h"
#include "Portable2004_PlatformMonitor.h"

#define super IOUserClient

#if enableUserClientInterface

OSDefineMetaClassAndStructors(Portable2004Client, IOUserClient)


IOExternalMethod * Portable2004Client::getTargetAndMethodForIndex(IOService ** target, UInt32 index)
{
    
    static const IOExternalMethod sMethods[kNumberOfMethods] = 
    {
        {   // kUserClientOpen
            NULL,						 	// The IOService * will be determined at runtime below
            (IOMethod) &Portable2004Client::open,			// Method pointer
            kIOUCScalarIScalarO,					// Scalar Input, Scalar Output
            0,							 	// No scalar input values
            0							 	// No scalar output values
        },
        {   // kUserClientClose
            NULL,									// The IOService * will be determined at runtime below
            (IOMethod) &Portable2004Client::close,  // Method pointer
            kIOUCScalarIScalarO,				 	// Scalar Input, Scalar Output
            0,										// No scalar input values
            0										// No scalar output values
        },
        {   // kUserClient_DFS
            NULL,								// The IOService * will be determined at runtime below
            (IOMethod) &MacRISC2CPU::DFS,		// Method pointer
            kIOUCScalarIScalarO,				// Scalar Input, Scalar Output
            2,									// one scalar input values
            0									// No scalar output values
        },
        {   // kUserClient_DFSConst
            NULL,									// The IOService * will be determined at runtime below
            (IOMethod) &MacRISC2CPU::DFSCont,		// Method pointer
            kIOUCScalarIScalarO,				 	// Scalar Input, Scalar Output
            2,										// one scalar input values
            0										// No scalar output values
        },
        {   // kUserClient_DFSStopConst
            NULL,									// The IOService * will be determined at runtime below
            (IOMethod) &MacRISC2CPU::DFSStopCont,   // Method pointer
            kIOUCScalarIScalarO,				 	// Scalar Input, Scalar Output
            0,										// No scalar input values
            0										// No scalar output values
        },
        {   // kUserClient_GPUSpeedCont
            NULL,									// The IOService * will be determined at runtime below
            (IOMethod) &MacRISC2CPU::GPUCont,		// Method pointer
            kIOUCScalarIScalarO,					// Scalar Input, Scalar Output
            1,										// No scalar input values
            0										// No scalar output values
        },
        {   // kUserClient_GPUSpeedStopCont
            NULL,									// The IOService * will be determined at runtime below
            (IOMethod) &MacRISC2CPU::GPUStopCont,   // Method pointer
            kIOUCScalarIScalarO,					// Scalar Input, Scalar Output
            0,										// No scalar input values
            0										// No scalar output values
        },
        {   // kUserClient_vStep (old-style PMU switch)
            NULL,									// The IOService * will be determined at runtime below
            (IOMethod) &MacRISC2CPU::vStep,			// Method pointer
            kIOUCScalarIScalarO,					// Scalar Input, Scalar Output
            1,										// one scalar input values
            0										// No scalar output values
        },
        {   // kUserClient_vStepConst (old-style PMU switch)
            NULL,									// The IOService * will be determined at runtime below
            (IOMethod) &MacRISC2CPU::vStepCont,		// Method pointer
            kIOUCScalarIScalarO,				 	// Scalar Input, Scalar Output
            1,										// one scalar input values
            0										// No scalar output values
        },
        {   // kUserClient_vStepStopConst (old-style PMU switch)
            NULL,										// The IOService * will be determined at runtime below
            (IOMethod) &MacRISC2CPU::vStepStopCont,		// Method pointer
            kIOUCScalarIScalarO,						// Scalar Input, Scalar Output
            0,											// No scalar input values
            0											// No scalar output values
        },
        {   // kUserClient_SetGPUPower
            NULL,									// The IOService * will be determined at runtime below
            (IOMethod) &MacRISC2CPU::SetGPUPower,   // Method pointer
            kIOUCScalarIScalarO,					// Scalar Input, Scalar Output
            1,										// No scalar input values
            0										// No scalar output values
        },
    };

	// Make sure that the index of the function we're calling actually exists in the function table.
    if(index < (UInt32)kNumberOfMethods) 
    {
        if(index == kUserClientOpen || index == kUserClientClose)
        {
            // these methods exist in Portable2004Client, so the pointer is "this"
			*target = this;	   
        }
        else
        {
            // These methods exist in Portable2004_PlatformMonitor, so the pointed is our provider
			*target = fProvider;
        }
        
        return (IOExternalMethod *) &sMethods[index];
    } 
	else
	{
		IOLog("Portable2004Client:: FUNCTION DOES NOT EXIST - index = %ld\n", index);
		return NULL;
	}
}

bool Portable2004Client::initWithTask(task_t owningTask,void *security_id , UInt32 type) {
    IOLog("Portable2004Client::initWithTask()\n");
    
    if(!super::initWithTask(owningTask, security_id , type)) { return false; }
    if(!owningTask) { return false; }
	
    fTask = owningTask;
    fProvider = NULL;
    fDead = false;
        
    return true;
}

bool Portable2004Client::start(IOService * provider) {
    IOLog("Portable2004Client::start()\n");
    
    if(!super::start(provider)) { return false; }

    fProvider = OSDynamicCast(MacRISC2CPU, provider);
    
    if(!fProvider) { return false; }
        
    return true;
}

void Portable2004Client::stop(IOService * provider) {
    IOLog("Portable2004Client::stop()\n");
	super::stop(provider);
}

IOReturn Portable2004Client::open(void) {
    IOLog("Portable2004Client::open()\n");
    
    // if we don't have an fProvider, this user client isn't going to do much, so return kIOReturnNotAttached
    if(!fProvider) { return kIOReturnNotAttached; }
        
    // call fProvider->open, and if it fails, it means someone else has already opened the device
    if(!fProvider->open(this)) { return kIOReturnExclusiveAccess; }
	else { return kIOReturnSuccess; }
}

IOReturn Portable2004Client::close(void)
{
    IOLog("Portable2004Client::close()\n");
            
    // if we don't have an fProvider, then we can't really call the fProvider's close() function, so just return
    if(!fProvider) { return kIOReturnNotAttached; }
        
    // make sure the fProvider is open before we tell it to close
    if(fProvider->isOpen(this)) { fProvider->close(this); }
    
    return kIOReturnSuccess;
}

IOReturn Portable2004Client::clientClose(void)
{
    
    IOLog("Portable2004Client::clientClose()\n");
    
    // release my hold on my parent (if I have one)
    close();
    
    if(fTask) { fTask = NULL; }
    
    fProvider = NULL;
    terminate();
    
    // DON'T call super::clientClose, which just returns kIOReturnUnsupported
	return kIOReturnSuccess;
}

IOReturn Portable2004Client::clientDied(void)
{
    IOLog("Portable2004Client::clientDied()\n");

    fDead = true;
    
    // this just calls clientClose
    return super::clientDied();
}

#endif