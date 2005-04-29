/*
 * Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
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

#include "AppleRAID.h"

#define super IOUserClient
OSDefineMetaClassAndStructors(AppleRAIDUserClient, IOUserClient);

    
bool
AppleRAIDUserClient::initWithTask(task_t owningTask,void *security_id , UInt32 type)
{
    IOLogUC("AppleRAIDUserClient::initWithTask()\n");
    
    if (!super::initWithTask(owningTask, security_id , type))
        return false;
    
    if (!owningTask)
	return false;
	
    fTask = owningTask;
    fProvider = NULL;
    fDead = false;
        
    return true;
}


bool 
AppleRAIDUserClient::start(IOService * provider)
{
    IOLogUC("AppleRAIDUserClient::start()\n");
    
    if(!super::start(provider))
        return false;
        
    fProvider = OSDynamicCast(AppleRAID, provider);
    
    if (!fProvider)
	return false;
        
    return true;
}


void 
AppleRAIDUserClient::stop(IOService * provider)
{
    IOLogUC("AppleRAIDUserClient::stop()\n");
    
    super::stop(provider);
}


IOReturn 
AppleRAIDUserClient::open(void)
{
    IOLogUC("AppleRAIDUserClient::open()\n");
    
    // If we don't have an fProvider, this user client isn't going to do much, so return kIOReturnNotAttached.
    if (!fProvider)
        return kIOReturnNotAttached;
        
    // Call fProvider->open, and if it fails, it means someone else has already opened the device.
    if (!fProvider->open(this))
	return kIOReturnExclusiveAccess;
        
    return kIOReturnSuccess;
}


IOReturn 
AppleRAIDUserClient::close(void)
{
    IOLogUC("AppleRAIDUserClient::close()\n");
            
    // If we don't have an fProvider, then we can't really call the fProvider's close() function, so just return.
    if (!fProvider)
        return kIOReturnNotAttached;
        
    // Make sure the fProvider is open before we tell it to close.
    if (fProvider->isOpen(this))
        fProvider->close(this);
    
    return kIOReturnSuccess;
}


IOReturn 
AppleRAIDUserClient::clientClose(void)
{
    IOLogUC("AppleRAIDUserClient::clientClose()\n");
    
    // release my hold on my parent (if I have one).
    close();
    
    if (fTask)
	fTask = NULL;
    
    fProvider = NULL;
    terminate();
    
    // DONT call super::clientClose, which just returns notSupported
    
    return kIOReturnSuccess;
}

IOReturn 
AppleRAIDUserClient::clientDied(void)
{
    IOReturn ret = kIOReturnSuccess;

    IOLogUC("AppleRAIDUserClient::clientDied()\n");

    fDead = true;
    
    // this just calls clientClose
    ret = super::clientDied();

    return ret;
}


IOReturn 
AppleRAIDUserClient::message(UInt32 type, IOService * provider,  void * argument)
{
    IOLogUC("AppleRAIDUserClient::message()\n");
    
    switch ( type )
    {
        default:
            break;
    }
    
    return super::message(type, provider, argument);
}


bool 
AppleRAIDUserClient::finalize(IOOptionBits options)
{
    bool ret;
    
    IOLogUC("AppleRAIDUserClient::finalize()\n");
    
    ret = super::finalize(options);
    
    return ret;
}


bool 
AppleRAIDUserClient::terminate(IOOptionBits options)
{
    bool ret;
    
    IOLogUC("AppleRAIDUserClient::terminate()\n");

    ret = super::terminate(options);
    
    return ret;
}


IOExternalMethod *
AppleRAIDUserClient::getTargetAndMethodForIndex(IOService ** target, UInt32 index)
{
    IOLogUC("AppleRAIDUserClient::getTargetAndMethodForIndex(index = %d)\n", (int)index);
    
    static const IOExternalMethod sMethods[kAppleRAIDUserClientMethodMaxCount] = 
    {
        {   // 0 - kAppleRAIDClientOpen
            NULL,						// The IOService * will be determined at runtime below.
            (IOMethod) &AppleRAIDUserClient::open,		// Method pointer.
            kIOUCScalarIScalarO,		       		// Scalar Input, Scalar Output.
            0,							// No scalar input values.
            0							// No scalar output values.
        },
        {   // 1 - kAppleRAIDClientClose 
            NULL,						// The IOService * will be determined at runtime below.
            (IOMethod) &AppleRAIDUserClient::close,		// Method pointer.
            kIOUCScalarIScalarO,				// Scalar Input, Scalar Output.
            0,							// No scalar input values.
            0							// No scalar output values.
        },
        {   // 2 - kAppleRAIDGetListOfSets 
            NULL,						// The IOService * will be determined at runtime below.
            (IOMethod) &AppleRAID::getListOfSets,		// Method pointer.
            kIOUCScalarIStructO,				// Scalar Input, Struct Output.
            1,							// flags to filter which sets to receive
            0xffffffff						// The size of the serialized array
        },
        {   // 3 - kAppleRAIDGetSetProperties
            NULL,						// The IOService * will be determined at runtime below.
            (IOMethod) &AppleRAID::getSetProperties,		// Method pointer.
            kIOUCStructIStructO,				// Struct Input, Struct Output.
            kAppleRAIDMaxUUIDStringSize,			// UUID string size
            0xffffffff						// The size of the serialized dictionary
        },
        {   // 4 - kAppleRAIDGetMemberProperties
            NULL,						// The IOService * will be determined at runtime below.
            (IOMethod) &AppleRAID::getMemberProperties,		// Method pointer.
            kIOUCStructIStructO,				// Struct Input, Struct Output.
            kAppleRAIDMaxUUIDStringSize,			// UUID string size
            0xffffffff						// The size of the serialized dictionary
        },
        {   // 5 - kAppleRAIDUpdateSet
            NULL,						// The IOService * will be determined at runtime below.
            (IOMethod) &AppleRAID::updateSet,			// Method pointer.
            kIOUCStructIStructO,				// Struct Input, Struct Output.
            0xffffffff,						// The size of serialized set info dictionary
            0xffffffff						// dummy size.
        }
    };
    
    
    // Make sure that the index of the function we're calling actually exists in the function table.
    if (index < (UInt32)kAppleRAIDUserClientMethodMaxCount) 
    {
        if (index == kAppleRAIDClientOpen || index == kAppleRAIDClientClose)
        {
            // These methods exist in AppleRAIDUserClient, so return a pointer to AppleRAIDUserClient.
            *target = this;	   
        }
        else
        {
            // These methods exist in the Controller, so return a pointer to AppleRAID.
            *target = fProvider;
        }
        
        return (IOExternalMethod *) &sMethods[index];
    }

    return NULL;
}


