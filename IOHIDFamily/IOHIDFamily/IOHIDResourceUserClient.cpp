/*
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 2008 Apple, Inc.  All Rights Reserved.
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

#include "IOHIDResourceUserClient.h"
#include <IOKit/IOLib.h>

/****************************************************************************/
/*								Defines										*/
/****************************************************************************/
#define super IOUserClient


/****************************************************************************/
/*					Default constructors/destructor							*/
/****************************************************************************/
OSDefineMetaClassAndStructors( IOHIDResourceDeviceUserClient, super )


/****************************************************************************/
/*								MIG calls									*/
/****************************************************************************/
const IOExternalMethodDispatch IOHIDResourceDeviceUserClient::_methods[kIOHIDResourceDeviceUserClientMethodCount] = {
	{   // kIOHIDResourceDeviceUserClientMethodCreate
		(IOExternalMethodAction) &IOHIDResourceDeviceUserClient::_createDevice,
		0, -1, /* 1 struct input : the report descriptor */
		0, 0
	},
	{   // kIOHIDResourceDeviceUserClientMethodTerminate
		(IOExternalMethodAction) &IOHIDResourceDeviceUserClient::_terminateDevice,
		0, 0,
		0, 0
	},
	{   // kIOHIDResourceDeviceUserClientMethodHandleReport
		(IOExternalMethodAction) &IOHIDResourceDeviceUserClient::_handleReport,
		0, -1, /* 1 struct input : the buffer */
		0, 0
	}
};



#pragma mark -
#pragma mark Methods
/****************************************************************************/
/*									Init									*/
/****************************************************************************/
bool IOHIDResourceDeviceUserClient::initWithTask(task_t owningTask, void * security_id, UInt32 type)
{
    if (!super::initWithTask(owningTask, security_id, type)) {
		IOLog("%s failed\n", __FUNCTION__);
		return false;
	}
	
	_device = NULL;

	return true;
}

/****************************************************************************/
/*									Start									*/
/****************************************************************************/
bool IOHIDResourceDeviceUserClient::start(IOService * provider)
{
	if (!super::start(provider)) {
		IOLog("%s failed\n", __FUNCTION__);
		return false;
	}	

    _owner = (IOHIDResource *) provider;
	_device = NULL;

	return true;
}

/****************************************************************************/
/*								MIG dispatch								*/
/****************************************************************************/
IOReturn IOHIDResourceDeviceUserClient::externalMethod(
                                            uint32_t                    selector, 
                                            IOExternalMethodArguments * arguments,
                                            IOExternalMethodDispatch *  dispatch, 
                                            OSObject *                  target, 
                                            void *                      reference)
{
    if (selector < (uint32_t) kIOHIDResourceDeviceUserClientMethodCount)
    {
        dispatch = (IOExternalMethodDispatch *) &_methods[selector];
        if (!target)
            target = this;
    }

	return super::externalMethod(selector, arguments, dispatch, target, reference);
}

/****************************************************************************/
/*									Helper									*/
/****************************************************************************/
IOMemoryDescriptor * IOHIDResourceDeviceUserClient::createMemoryDescriptorFromInputArguments(
                                            IOExternalMethodArguments * arguments)
{
    IOMemoryDescriptor * report = NULL;
    
    if ( arguments->structureInputDescriptor ) {
        report = arguments->structureInputDescriptor;
        report->retain();
    } else {
        report = IOMemoryDescriptor::withAddress((void *)arguments->structureInput, arguments->structureInputSize, kIODirectionOut);
    }
    
    return report;
}


/****************************************************************************/
/*									Get/Set									*/
/****************************************************************************/
IOService *IOHIDResourceDeviceUserClient::getService(void)
{
	return _owner;
}

/****************************************************************************/
/*									Close									*/
/****************************************************************************/
IOReturn IOHIDResourceDeviceUserClient::clientClose(void)
{
    terminateDevice();
    terminate();
	return kIOReturnSuccess;
}



#pragma mark -
#pragma mark Private functions (Driver communication)
/****************************************************************************/
/*									Open									*/
/****************************************************************************/
IOReturn IOHIDResourceDeviceUserClient::createDevice(
                                        IOHIDResourceDeviceUserClient * target, 
                                        void *                          reference, 
                                        IOExternalMethodArguments *     arguments)
{
	if (_device == NULL)  { // Report descriptor is static and thus can only be set on creation
        
        IOReturn                ret;
        IOMemoryDescriptor *    propertiesDesc  = NULL;
        OSDictionary *          properties      = NULL;
        
        // Let's deal with our device properties from data
        propertiesDesc = createMemoryDescriptorFromInputArguments(arguments);
        if ( !propertiesDesc ) {
            IOLog("%s failed : could not create descriptor\n", __FUNCTION__);
            return kIOReturnNoMemory;
        }

        ret = propertiesDesc->prepare();
        if ( ret == kIOReturnSuccess ) {
        
            void *         propertiesData;
            IOByteCount    propertiesLength;
            
            propertiesLength = propertiesDesc->getLength();
            if ( propertiesLength ) { 
                propertiesData = IOMalloc(propertiesLength);
            
                if ( propertiesData ) { 
                    OSObject * object;
                
                    propertiesDesc->readBytes(0, propertiesData, propertiesLength);

                    object = OSUnserializeXML((const char *)propertiesData);
                    properties = OSDynamicCast(OSDictionary, object);
                    if( !properties )
                        object->release();
                    
                    IOFree(propertiesData, propertiesLength);
                }
            
            }
            propertiesDesc->complete();
        }
        propertiesDesc->release();
        
        // If after all the unwrapping we have a dictionary, let's create the device
        if ( properties ) { 
            _device = IOHIDUserDevice::withProperties(properties);
            properties->release();
        }

        
	} else {/* We already have a device. Close it before opening a new one */
		IOLog("%s failed : _device already exists\n", __FUNCTION__);
		return kIOReturnInternalError;
	}

	if (_device == NULL) {
		IOLog("%s failed : _device is NULL\n", __FUNCTION__);
		return kIOReturnNoResources;
	}

	if (!_device->attach(this) || !_device->start(this)) {
		IOLog("%s attach or start failed\n", __FUNCTION__);
		
		_device->release();
		_device = NULL;
		return kIOReturnInternalError;
	}

    return kIOReturnSuccess;
}

IOReturn IOHIDResourceDeviceUserClient::_createDevice(
                                        IOHIDResourceDeviceUserClient * target, 
                                        void *                          reference, 
                                        IOExternalMethodArguments *     arguments)
{
	return target->createDevice(target, reference, arguments);
}

/****************************************************************************/
/*									handleReport                            */
/****************************************************************************/
IOReturn IOHIDResourceDeviceUserClient::handleReport(
                                        IOHIDResourceDeviceUserClient * target, 
                                        void *                          reference, 
                                        IOExternalMethodArguments *     arguments)
{
	if (_device == NULL) {
		IOLog("%s failed : device is NULL\n", __FUNCTION__);
		return kIOReturnNotOpen;
	}

	if (target != this) {
		IOLog("%s failed : this is not target\n", __FUNCTION__);
		return kIOReturnInternalError;
	}

    IOReturn                ret;
    IOMemoryDescriptor *    report;
    
    report = createMemoryDescriptorFromInputArguments(arguments);
    if ( !report ) {
		IOLog("%s failed : could not create descriptor\n", __FUNCTION__);
		return kIOReturnNoMemory;
	}

    ret = report->prepare();
    if ( ret == kIOReturnSuccess ) {
        ret = _device->handleReport(report);
        report->complete();
    }
    
    report->release();

    return ret;
}

IOReturn IOHIDResourceDeviceUserClient::_handleReport(IOHIDResourceDeviceUserClient	*target, 
											 void						*reference, 
											 IOExternalMethodArguments	*arguments)
{
	return target->handleReport(target, reference, arguments);
}


/****************************************************************************/
/*									Close									*/
/****************************************************************************/
IOReturn IOHIDResourceDeviceUserClient::terminateDevice()
{
	if (_device) {
		_device->stop(this);
		_device->release();
	}
	_device = NULL;

    return kIOReturnSuccess;
}

IOReturn IOHIDResourceDeviceUserClient::_terminateDevice(
                                        IOHIDResourceDeviceUserClient	*target, 
                                        void                            *reference, 
                                        IOExternalMethodArguments       *arguments)
{
	return target->terminateDevice();
}
