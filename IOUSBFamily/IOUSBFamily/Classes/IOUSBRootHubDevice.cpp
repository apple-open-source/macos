/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

#include "IOUSBRootHubDevice.h"

#define super	IOUSBDevice
#define self	this
#define DEBUGGING_LEVEL 0
#define DEBUGLOG IOLog

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors( IOUSBRootHubDevice, IOUSBDevice )

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOUSBRootHubDevice*
IOUSBRootHubDevice::NewRootHubDevice()
{
    return (new IOUSBRootHubDevice);
}


// intercept regular hub requests since the controller simulates the root hub
IOReturn 
IOUSBRootHubDevice::DeviceRequest(IOUSBDevRequest *request, IOUSBCompletion *completion)
{
    return DeviceRequest(request, 0, 0, completion);
}



OSMetaClassDefineReservedUsed(IOUSBRootHubDevice,  0);
IOReturn 
IOUSBRootHubDevice::DeviceRequest(IOUSBDevRequest *request, UInt32 noDataTimeout, UInt32 completionTimeout, IOUSBCompletion *completion)
{
    IOReturn		err = 0;
    UInt16		theRequest;
    UInt8		dType, dIndex;

    
    if (!request)
        return(kIOReturnBadArgument);

#if (DEBUGGING_LEVEL > 0)
    DEBUGLOG("%s: deviceRequest([%x,%x],[%x,%x],[%x,%lx])", getName(),
             request->bmRequestType,
             request->bRequest,
             request->wValue, request->wIndex, 
	     request->wLength, (UInt32)request->pData);
#endif

    theRequest = (request->bRequest << 8) | request->bmRequestType;

    switch (theRequest)
    {
        // Standard Requests
        //
        case kClearDeviceFeature:
            if (request->wIndex == 0)
                err = _controller->ClearRootHubFeature(request->wValue);
            else
                err = kIOReturnBadArgument;
            break;

        case kGetDescriptor:
            dType = request->wValue >> 8;
            dIndex = request->wValue & 0x00FF;
            switch (dType) {
                case kUSBDeviceDesc:
                    err = _controller->GetRootHubDeviceDescriptor((IOUSBDeviceDescriptor*)request->pData);
                    request->wLenDone = sizeof(IOUSBDeviceDescriptor);
                    break;

                case kUSBConfDesc:
                {
                    OSData *fullDesc = OSData::withCapacity(1024); // FIXME
                    UInt16 newLength;
                    
                    err = _controller->GetRootHubConfDescriptor(fullDesc);
                    newLength = fullDesc->getLength();
                    if (newLength < request->wLength)
                        request->wLength = newLength;
                    bcopy(fullDesc->getBytesNoCopy(), (char *)request->pData, request->wLength);
                    request->wLenDone = request->wLength;
                    fullDesc->free();
                    break;
                }

                case kUSBStringDesc:
                {
                    OSData *fullDesc = OSData::withCapacity(1024); // FIXME
                    UInt16 newLength;
                    
                    err = _controller->GetRootHubStringDescriptor((request->wValue & 0x00ff), fullDesc);
                    newLength = fullDesc->getLength();
                    if (newLength < request->wLength)
                        request->wLength = newLength;
                    bcopy(fullDesc->getBytesNoCopy(), (char *)request->pData, request->wLength);
                    request->wLenDone = request->wLength;
                    fullDesc->free();
                    break;
                }
                
                default:
                    err = kIOReturnBadArgument;
            }
            break;

        case kGetDeviceStatus:
            if ((request->wValue == 0) && (request->wIndex == 0) && (request->pData != 0))
            {
                *(UInt16*)(request->pData) = HostToUSBWord(1); // self-powered
                request->wLenDone = 2;
            }
            else
                err = kIOReturnBadArgument;
            break;

        case kSetAddress:
            if (request->wIndex == 0)
                err = _controller->SetHubAddress(request->wValue);
            else
                err = kIOReturnBadArgument;
            break;
                
        case kSetConfiguration:
            if (request->wIndex == 0)
                configuration = request->wValue;
            else
                err = kIOReturnBadArgument;
            break;

        case kSetDeviceFeature:
            if (request->wIndex == 0)
                err = _controller->SetRootHubFeature(request->wValue);
            else
                err = kIOReturnBadArgument;
            break;

        case kGetConfiguration:
            if ((request->wIndex == 0) && (request->pData != 0))
            {
                *(UInt8*)(request->pData) = configuration;
                request->wLenDone = 1;
            }
            else
                err = kIOReturnBadArgument;
            break;

        case kClearInterfaceFeature:
        case kClearEndpointFeature:
        case kGetInterface:
        case kGetInterfaceStatus:
        case kGetEndpointStatus:
        case kSetInterfaceFeature:
        case kSetEndpointFeature:
        case kSetDescriptor:
        case kSetInterface:
        case kSyncFrame:
            err = kIOReturnUnsupported;
            break;

        // Class Requests
        //
        case kClearHubFeature:
            if (request->wIndex == 0)
                err = _controller->ClearRootHubFeature(request->wValue);
            else
                err = kIOReturnBadArgument;
            break;

        case kClearPortFeature:
            err = _controller->ClearRootHubPortFeature(request->wValue, request->wIndex);
            break;

        case kGetPortState:
            if ((request->wValue == 0) && (request->pData != 0))
                err = _controller->GetRootHubPortState((UInt8 *)request->pData, request->wIndex);
            else
                err = kIOReturnBadArgument;
            break;

        case kGetHubDescriptor:
            if ((request->wValue == ((kUSBHubDescriptorType << 8) + 0)) && (request->pData != 0))
            {
                err = _controller->GetRootHubDescriptor((IOUSBHubDescriptor *)request->pData);
                request->wLenDone = sizeof(IOUSBHubDescriptor);
            }
            else
                err = kIOReturnBadArgument;
            break;

        case kGetHubStatus:
            if ((request->wValue == 0) && (request->wIndex == 0) && (request->pData != 0))
            {
                err = _controller->GetRootHubStatus((IOUSBHubStatus *)request->pData);
                request->wLenDone = sizeof(IOUSBHubStatus);
            }
            else
                err = kIOReturnBadArgument;
           break;

        case kGetPortStatus:
            if ((request->wValue == 0) && (request->pData != 0))
            {
                err = _controller->GetRootHubPortStatus((IOUSBHubPortStatus *)request->pData, request->wIndex);
                request->wLenDone = sizeof(IOUSBHubPortStatus);
            }
            else
                err = kIOReturnBadArgument;
            break;

        case kSetHubDescriptor:
            if (request->pData != 0)
                err = _controller->SetRootHubDescriptor((OSData *)request->pData);
            else
                err = kIOReturnBadArgument;
            break;

        case kSetHubFeature:
            if (request->wIndex == 0)
                err = _controller->SetRootHubFeature(request->wValue);
            else
                err = kIOReturnBadArgument;
            break;

        case kSetPortFeature:
            err = _controller->SetRootHubPortFeature(request->wValue, request->wIndex);
            break;

        default:
            err = kIOReturnBadArgument;

    }
    return(err);
}


OSMetaClassDefineReservedUnused(IOUSBRootHubDevice,  1);
OSMetaClassDefineReservedUnused(IOUSBRootHubDevice,  2);
OSMetaClassDefineReservedUnused(IOUSBRootHubDevice,  3);
OSMetaClassDefineReservedUnused(IOUSBRootHubDevice,  4);


