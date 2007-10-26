/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 2006 Apple Computer, Inc.  All Rights Reserved.
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

#include <libkern/OSByteOrder.h>
#include <IOKit/usb/IOUSBLog.h>

#include "IOUSBHubDevice.h"

/* Convert USBLog to use kprintf debugging */
#ifndef IOUSBHUBDEVICE_USE_KPRINTF
#define IOUSBHUBDEVICE_USE_KPRINTF 0
#endif

#if IOUSBHUBDEVICE_USE_KPRINTF
#undef USBLog
#undef USBError
void kprintf(const char *format, ...)
__attribute__((format(printf, 1, 2)));
#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= 5) { kprintf( FORMAT "\n", ## ARGS ) ; }
#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif

#define super	IOUSBDevice
#define self	this

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors( IOUSBHubDevice, IOUSBDevice )

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOUSBHubDevice*
IOUSBHubDevice::NewHubDevice()
{
	IOUSBHubDevice *me = new IOUSBHubDevice;
	
	if (!me)
		return NULL;
	
	if (!me->init())
	{
		me->release();
		me = NULL;
	}
	
	return me;
}



bool 
IOUSBHubDevice::init()
{
    if (!super::init())
        return false;

    return true;
}



bool
IOUSBHubDevice::start(IOService *provider)
{
	if (!super::start(provider))
		return false;
		
	if (!InitializeCharacteristics())
		return false;
	
	return true;
}



bool
IOUSBHubDevice::InitializeCharacteristics()
{
	UInt32			characteristics = 0;			// not a root hub, which will get overriden
	
	// for now, we are not attached to any bus and we are not the root hub, so we set all to 0
	SetHubCharacteristics(characteristics);
	return true;
}



void
IOUSBHubDevice::stop( IOService *provider )
{
	super::stop(provider);
}



void
IOUSBHubDevice::free()
{
	USBLog(1, "%s[%p]::free", getName(), this);
	if (_expansionData)
    {
        IOFree(_expansionData, sizeof(ExpansionData));
        _expansionData = NULL;
    }
    super::free();
}



void
IOUSBHubDevice::SetHubCharacteristics(UInt32 characteristics)
{
	_myCharacteristics = characteristics;
}



UInt32
IOUSBHubDevice::GetHubCharacteristics()
{
	return _myCharacteristics;
}



UInt32			
IOUSBHubDevice::GetMaxProvidedPower()
{
	return 500;
}



UInt32
IOUSBHubDevice::RequestProvidedPower(UInt32 requestedPower)
{
	if (requestedPower <= 500)
		return requestedPower;
	else
		return 500;
}



void
IOUSBHubDevice::SetPolicyMaker(IOUSBHubPolicyMaker *policyMaker)
{
	_myPolicyMaker = policyMaker;
}



IOUSBHubPolicyMaker *
IOUSBHubDevice::GetPolicyMaker(void)
{
	return _myPolicyMaker;
}



UInt32
IOUSBHubDevice::RequestExtraPower(UInt32 requestedPower)
{
	// currently this feature is only available on root hub devices, so this method will
	// be overriden by the IOUSBRootHubDevice class to implement this
	USBLog(2, "IOUSBHubDevice[%p]::RequestExtraPower - not a root hub - no extra power", this);
	return 0;
}



void
IOUSBHubDevice::ReturnExtraPower(UInt32 returnedPower)
{
	// currently this feature is only available on root hub devices, so this method will
	// be overriden by the IOUSBRootHubDevice class to implement this
	
	return;
}


OSMetaClassDefineReservedUnused(IOUSBHubDevice,  0);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  1);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  2);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  3);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  4);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  5);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  6);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  7);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  8);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  9);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  10);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  11);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  12);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  13);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  14);
OSMetaClassDefineReservedUnused(IOUSBHubDevice,  15);


