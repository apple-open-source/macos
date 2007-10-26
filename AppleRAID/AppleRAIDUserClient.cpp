/*
 * Copyright (c) 2001-2007 Apple Inc. All rights reserved.
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
AppleRAIDUserClient::initWithTask(task_t owningTask, void *security_id, UInt32 type)
{
    IOLogUC("AppleRAIDUserClient::initWithTask()\n");

    if (!super::initWithTask(owningTask, security_id, type)) return false;
    
    if (!owningTask) return false;
    ucClient = owningTask;
    ucController = NULL;
        
    return true;
}


bool 
AppleRAIDUserClient::start(IOService * provider)
{
    IOLogUC("AppleRAIDUserClient::start()\n");
    
    if (!super::start(provider)) return false;
        
    ucController = OSDynamicCast(AppleRAID, provider);
    if (!ucController) return false;
        
    return true;
}


IOReturn 
AppleRAIDUserClient::clientClose(void)
{
    IOLogUC("AppleRAIDUserClient::clientClose()\n");
    
    closeController();
    
    ucClient = NULL;
    ucController = NULL;

    terminate();
    
    // Do not call super::clientClose, which just returns notSupported
    return kIOReturnSuccess;
}


IOReturn
AppleRAIDUserClient::externalMethod(uint32_t                    selector, 
				    IOExternalMethodArguments * arguments,
				    IOExternalMethodDispatch *  dispatch, 
				    OSObject *                  target, 
				    void *                      reference)
{
    IOLogUC("AppleRAIDUserClient::externalMethod(selector = %d)\n", (int)selector);
    
    static const IOExternalMethodDispatch sMethods[kAppleRAIDUserClientMethodsCount] = 
    {
        {   // 0 - kAppleRAIDClientOpen
            (IOExternalMethodAction) &AppleRAIDUserClient::openShim,
            0, 0,							// scalar in count, struct in size
	    0, 0							// scalar out count, struct out size 
        },
        {   // 1 - kAppleRAIDClientClose 
            (IOExternalMethodAction) &AppleRAIDUserClient::closeShim,
            0, 0,							// scalar in count, struct in size
	    0, 0							// scalar out count, struct out size
        },
        {   // 2 - kAppleRAIDGetListOfSets 
            (IOExternalMethodAction) &AppleRAIDUserClient::getListOfSetsShim,
            0, sizeof(UInt32),						// scalar in count, struct in size
	    0, 0xffffffff						// scalar out count, struct out size 
        },
	{   // 3 - kAppleRAIDGetSetProperties
            (IOExternalMethodAction) &AppleRAIDUserClient::getSetPropertiesShim,
            0, kAppleRAIDMaxUUIDStringSize,				// scalar in count, struct in size
	    0, 0xffffffff						// scalar out count, struct out size
        },
        {   // 4 - kAppleRAIDGetMemberProperties
            (IOExternalMethodAction) &AppleRAIDUserClient::getMemberPropertiesShim,
            0, kAppleRAIDMaxUUIDStringSize,				// scalar in count, struct in size
	    0, 0xffffffff						// scalar out count, struct out size
        },
        {   // 5 - kAppleRAIDUpdateSet
            (IOExternalMethodAction) &AppleRAIDUserClient::updateSetShim,
            0, 0xffffffff,						// scalar in count, struct in size
	    0, 0xffffffff						// scalar out count, struct out size
        },
        {   // 6 - kAppleLVMGetVolumesForGroup
            (IOExternalMethodAction) &AppleRAIDUserClient::getVolumesForGroupShim,
            0, kAppleRAIDMaxUUIDStringSize * 2,				// scalar in count, struct in size
	    0, 0xffffffff						// scalar out count, struct out size
        },
        {   // 7 - kAppleLVMGetVolumeProperties
            (IOExternalMethodAction) &AppleRAIDUserClient::getVolumePropertiesShim,
            0, kAppleRAIDMaxUUIDStringSize,				// scalar in count, struct in size
	    0, 0xffffffff						// scalar out count, struct out size
        },
        {   // 8 - kAppleLVMGetVolumeExtents
            (IOExternalMethodAction) &AppleRAIDUserClient::getVolumeExtentsShim,
            0, kAppleRAIDMaxUUIDStringSize,				// scalar in count, struct in size
	    0, 0xffffffff						// scalar out count, struct out size
        },
        {   // 9 - kAppleLVMUpdateLogicalVolume
            (IOExternalMethodAction) &AppleRAIDUserClient::updateLogicalVolumeShim,
            0, 0xffffffff,						// scalar in count, struct in size
	    0, 0xffffffff						// scalar out count, struct out size
        },
        {   // 10 - kAppleLVMDestroyLogicalVolume
            (IOExternalMethodAction) &AppleRAIDUserClient::destroyLogicalVolumeShim,
            0, 0xffffffff,						// scalar in count, struct in size
	    0, 0xffffffff						// scalar out count, struct out size
        }
    };

    // Make sure that the index of the function we're calling actually exists in the function table.
    if (selector < (uint32_t)kAppleRAIDUserClientMethodsCount) 
    {
	dispatch = (IOExternalMethodDispatch *) &sMethods[selector];
	target = this;	   
	return super::externalMethod(selector, arguments, dispatch, target, reference);
    }

    return kIOReturnBadArgument;
}


// ***********************************************************************************************************
// ***********************************************************************************************************
// ***********************************************************************************************************

IOReturn 
AppleRAIDUserClient::openController(void)
{
    IOLogUC("AppleRAIDUserClient::openController()\n");
    
    if (!ucController) return kIOReturnNotAttached;
        
    if (!ucController->open(this)) return kIOReturnExclusiveAccess;
        
    return kIOReturnSuccess;
}


IOReturn 
AppleRAIDUserClient::closeController(void)
{
    IOLogUC("AppleRAIDUserClient::closeController()\n");
            
    if (!ucController) return kIOReturnNotAttached;
        
    if (ucController->isOpen(this)) {
	ucController->close(this);
    }
        
    return kIOReturnSuccess;
}

#if 0
// XXX will need to support larger ool arguments someday

IOReturn
AppleRAIDUserClient::mapArguments(IOExternalMethodArguments * arguments, void ** in, uint32_t * inCount, void ** out, uint32_t * outCount)
{
    if (arguments->structureInputDescriptor) {

        IOMemoryDescriptor * mem = arguments->structureInputDescriptor;
        IOReturn ret = mem->map();  ///////////
        if (ret != kIOReturnSuccess) return ret;
	
	*in = mem->virtual;  ////////////
	*inCount = mem->getLength();
    } else {
        *in = arguments->structureInput;
	*inCount = arguments->structureInputSize;
    }
    if (arguments->structureOutputDescriptor) {

        IOMemoryDescriptor * mem = arguments->structureOutputDescriptor;
        IOReturn ret = mem->map();  ///////////
        if (ret != kIOReturnSuccess) return ret;

	*out = 0;  ////////
	*outCount = arguments->structureOutputSize;
    } else {
        *out = arguments->structureOutput;
	*outCount = arguments->structureOutputSize;
    }

    return IOReturnSuccess;
}

IOReturn
AppleRAIDUserClient::unmapArguments(IOExternalMethodArguments * arguments, void * out, uint32_t outCount)
{
}
#endif

IOReturn 
AppleRAIDUserClient::getListOfSets(IOExternalMethodArguments * arguments)
{
    IOLogUC("AppleRAIDUserClient::getListOfSets()\n");

    if (arguments->structureInputDescriptor) return kIOReturnBadArgument;
    if (arguments->structureOutputDescriptor) return kIOReturnBadArgument;
             
    if (!ucController || isInactive()) return kIOReturnNotAttached;

    UInt32 flags = *((UInt32 *)arguments->structureInput);
			       
    return ucController->getListOfSets(flags, (char *)arguments->structureOutput, &arguments->structureOutputSize);
}


IOReturn 
AppleRAIDUserClient::getSetProperties(IOExternalMethodArguments * arguments)
{
    IOLogUC("AppleRAIDUserClient::getSetProperties()\n");
    
    if (arguments->structureInputDescriptor) return kIOReturnBadArgument;
    if (arguments->structureOutputDescriptor) return kIOReturnBadArgument;
             
    if (!ucController || isInactive()) return kIOReturnNotAttached;

    return ucController->getSetProperties((char *)arguments->structureInput, arguments->structureInputSize,
					  (char *)arguments->structureOutput, &arguments->structureOutputSize);
}


IOReturn 
AppleRAIDUserClient::getMemberProperties(IOExternalMethodArguments * arguments)
{
    IOLogUC("AppleRAIDUserClient::getMemberProperties()\n");
    
    if (arguments->structureInputDescriptor) return kIOReturnBadArgument;
    if (arguments->structureOutputDescriptor) return kIOReturnBadArgument;
             
    if (!ucController || isInactive()) return kIOReturnNotAttached;

    return ucController->getMemberProperties((char *)arguments->structureInput, arguments->structureInputSize,
					     (char *)arguments->structureOutput, &arguments->structureOutputSize);
}


IOReturn 
AppleRAIDUserClient::updateSet(IOExternalMethodArguments * arguments)
{
    IOLogUC("AppleRAIDUserClient::updateSet()\n");
    
    if (arguments->structureInputDescriptor) return kIOReturnBadArgument;
    if (arguments->structureOutputDescriptor) return kIOReturnBadArgument;
             
    if (!ucController || isInactive()) return kIOReturnNotAttached;

    if (clientHasPrivilege(ucClient, kIOClientPrivilegeAdministrator)) return kIOReturnNotPrivileged;

    return ucController->updateSet((char *)arguments->structureInput, arguments->structureInputSize,
				   (char *)arguments->structureOutput, &arguments->structureOutputSize);
}

IOReturn 
AppleRAIDUserClient::getVolumesForGroup(IOExternalMethodArguments * arguments)
{
    IOLogUC("AppleRAIDUserClient::getVolumesForGroup()\n");
    
    if (arguments->structureInputDescriptor) return kIOReturnBadArgument;
    if (arguments->structureOutputDescriptor) return kIOReturnBadArgument;
             
    if (!ucController || isInactive()) return kIOReturnNotAttached;

    return ucController->getVolumesForGroup((char *)arguments->structureInput, arguments->structureInputSize,
					    (char *)arguments->structureOutput, &arguments->structureOutputSize);
}

IOReturn 
AppleRAIDUserClient::getVolumeProperties(IOExternalMethodArguments * arguments)
{
    IOLogUC("AppleRAIDUserClient::getVolumeProperties()\n");
    
    if (arguments->structureInputDescriptor) return kIOReturnBadArgument;
    if (arguments->structureOutputDescriptor) return kIOReturnBadArgument;
             
    if (!ucController || isInactive()) return kIOReturnNotAttached;

    return ucController->getVolumeProperties((char *)arguments->structureInput, arguments->structureInputSize,
					     (char *)arguments->structureOutput, &arguments->structureOutputSize);
}

IOReturn 
AppleRAIDUserClient::getVolumeExtents(IOExternalMethodArguments * arguments)
{
    IOLogUC("AppleRAIDUserClient::getVolumeExtentss()\n");
    
    if (arguments->structureInputDescriptor) return kIOReturnBadArgument;
    if (arguments->structureOutputDescriptor) return kIOReturnBadArgument;
             
    if (!ucController || isInactive()) return kIOReturnNotAttached;

    return ucController->getVolumeExtents((char *)arguments->structureInput, arguments->structureInputSize,
					  (char *)arguments->structureOutput, &arguments->structureOutputSize);
}

IOReturn 
AppleRAIDUserClient::updateLogicalVolume(IOExternalMethodArguments * arguments)
{
    IOLogUC("AppleRAIDUserClient::updateLogicalVolume()\n");
    
    if (arguments->structureInputDescriptor) return kIOReturnBadArgument;
    if (arguments->structureOutputDescriptor) return kIOReturnBadArgument;
             
    if (!ucController || isInactive()) return kIOReturnNotAttached;

    if (clientHasPrivilege(ucClient, kIOClientPrivilegeAdministrator)) return kIOReturnNotPrivileged;

    return ucController->updateLogicalVolume((char *)arguments->structureInput, arguments->structureInputSize,
					     (char *)arguments->structureOutput, &arguments->structureOutputSize);
    return 0;
}


IOReturn 
AppleRAIDUserClient::destroyLogicalVolume(IOExternalMethodArguments * arguments)
{
    IOLogUC("AppleRAIDUserClient::destroyLogicalVolume()\n");
    
    if (arguments->structureInputDescriptor) return kIOReturnBadArgument;
    if (arguments->structureOutputDescriptor) return kIOReturnBadArgument;
             
    if (!ucController || isInactive()) return kIOReturnNotAttached;

    if (clientHasPrivilege(ucClient, kIOClientPrivilegeAdministrator)) return kIOReturnNotPrivileged;

    return ucController->destroyLogicalVolume((char *)arguments->structureInput, arguments->structureInputSize,
					      (char *)arguments->structureOutput, &arguments->structureOutputSize);
    return 0;
}
