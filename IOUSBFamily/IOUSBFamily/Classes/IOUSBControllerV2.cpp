/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.  
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <IOKit/usb/IOUSBController.h>
#include <IOKit/usb/IOUSBControllerV2.h>
#include <IOKit/usb/IOUSBLog.h>


#define super IOUSBController

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClass( IOUSBControllerV2, IOUSBController )
OSDefineAbstractStructors(IOUSBControllerV2, IOUSBController)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOUSBControllerV2::OpenPipe(USBDeviceAddress address, UInt8 speed,
						Endpoint *endpoint)
{
    return _commandGate->runAction(DoCreateEP, (void *)(UInt32)address,
			(void *)(UInt32)speed, endpoint);
}


IOReturn 
IOUSBControllerV2::DoCreateEP(OSObject *owner,
                           void *arg0, void *arg1,
                           void *arg2, void *arg3)
{
    IOUSBControllerV2 *me = (IOUSBControllerV2 *)owner;
    UInt8 address = (UInt8)(UInt32)arg0;
    UInt8 speed = (UInt8)(UInt32)arg1;
    Endpoint *endpoint = (Endpoint *)arg2;
    IOReturn err;

    USBLog(5,"%s[%p]::DoCreateEP, high speed ancestor hub:%d, port:%d", me->getName(), me, me->_highSpeedHub[address], me->_highSpeedPort[address]);

    switch (endpoint->transferType)
    {
        case kUSBInterrupt:
            err = me->UIMCreateInterruptEndpoint(address,
                                             endpoint->number,
                                             endpoint->direction,
                                             speed,
                                             endpoint->maxPacketSize,
                                             endpoint->interval,
                                             me->_highSpeedHub[address],
                                             me->_highSpeedPort[address]);
            break;

        case kUSBBulk:
            err = me->UIMCreateBulkEndpoint(address,
                                        endpoint->number,
                                        endpoint->direction,
                                        speed,
                                        endpoint->maxPacketSize,
                                        me->_highSpeedHub[address],
                                        me->_highSpeedPort[address]);
            break;

        case kUSBControl:
            err = me->UIMCreateControlEndpoint(address,
                                           endpoint->number,
                                           endpoint->maxPacketSize,
                                           speed,
                                           me->_highSpeedHub[address],
                                           me->_highSpeedPort[address]);
            break;

        case kUSBIsoc:
            err = me->UIMCreateIsochEndpoint(address,
                                        endpoint->number,
                                        endpoint->maxPacketSize,
                                        endpoint->direction,
                                        me->_highSpeedHub[address],
                                        me->_highSpeedPort[address]);
            break;

        default:
            err = kIOReturnBadArgument;
            break;
    }
    return (err);
}


IOReturn 
IOUSBControllerV2::CreateDevice(	IOUSBDevice 		*newDevice,
                                USBDeviceAddress	deviceAddress,
                                UInt8		 	maxPacketSize,
                                UInt8			speed,
                                UInt32			powerAvailable,
                                USBDeviceAddress		hub,
                                int      port)
{
    USBLog(5,"%s[%p]::CreateDevice, new method called with hub:%d, port:%d", getName(), this, hub, port);
    
    if(speed != kUSBDeviceSpeedHigh)
    {
        if(_highSpeedHub[hub] == 0)	// this is the first non high speed device in this chain
        {
            _highSpeedHub[deviceAddress] = hub;
            _highSpeedPort[deviceAddress] = port;
        }
        else
        {
            _highSpeedHub[deviceAddress] = _highSpeedHub[hub];
            _highSpeedPort[deviceAddress] = _highSpeedPort[hub];
        }
    }
    else
    {
        _highSpeedHub[deviceAddress] = 0;
        _highSpeedPort[deviceAddress] = 0;
    }

    USBLog(5,"%s[%p]::CreateDevice, high speed ancestor hub:%d, port:%d",getName(), this, _highSpeedHub[deviceAddress], _highSpeedPort[deviceAddress]);
    
    return (super::CreateDevice(newDevice, deviceAddress, maxPacketSize, speed, powerAvailable));
}

IOReturn 
IOUSBControllerV2::ConfigureDeviceZero(UInt8 maxPacketSize, UInt8 speed, USBDeviceAddress hub, int port)
{
    USBLog(5,"%s[%p]::ConfigureDeviceZero, new method called with hub:%d, port:%d", getName(), this, hub, port);

    if(speed != kUSBDeviceSpeedHigh)
    {
        if(_highSpeedHub[hub] == 0)	// this is the first non high speed device in this chain
        {
            _highSpeedHub[0] = hub;
            _highSpeedPort[0] = port;
        }
        else
        {
            _highSpeedHub[0] = _highSpeedHub[hub];
            _highSpeedPort[0] = _highSpeedPort[hub];
        }
    }
    else
    {
        _highSpeedHub[0] = 0;
        _highSpeedPort[0] = 0;
    }

    USBLog(5, "%s[%p]::CreateDevice, high speed ancestor hub:%d, port:%d", getName(), this, _highSpeedHub[0], _highSpeedPort[0]);
    
    return (super::ConfigureDeviceZero(maxPacketSize, speed));
}




IOReturn
IOUSBControllerV2::DOHSHubMaintenance(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3)
{
    IOUSBControllerV2 *me = (IOUSBControllerV2 *)owner;
    USBDeviceAddress highSpeedHub = (USBDeviceAddress)(UInt32)arg0;
    UInt32 command = (UInt32)arg1;
    UInt32 flags = (UInt32)arg2;

    USBLog(5,"%s[%p]::DOHSHubMaintenance, command: %d, flags: %d", me->getName(), me, command, flags);

    return me->UIMHubMaintenance(highSpeedHub, 0, command, flags);
}



IOReturn
IOUSBControllerV2::DOSetTestMode(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3)
{
    IOUSBControllerV2 *me = (IOUSBControllerV2 *)owner;
    UInt32 mode = (UInt32)arg0;
    UInt32 port = (UInt32)arg1;

    USBLog(5,"%s[%p]::DOSetTestMode, mode: %d, port: %d", me->getName(), me, mode, port);

    return me->UIMSetTestMode(mode, port);
}



OSMetaClassDefineReservedUsed(IOUSBControllerV2,  0);
IOReturn		
IOUSBControllerV2::AddHSHub(USBDeviceAddress highSpeedHub, UInt32 flags)
{
    return _commandGate->runAction(DOHSHubMaintenance, (void *)(UInt32)highSpeedHub,
			(void *)(UInt32)kUSBHSHubCommandAddHub, (void *)flags);
}



OSMetaClassDefineReservedUsed(IOUSBControllerV2,  1);
IOReturn 		
IOUSBControllerV2::UIMHubMaintenance(USBDeviceAddress highSpeedHub, UInt32 highSpeedPort, UInt32 command, UInt32 flags)
{
    return kIOReturnIPCError;			// not implemented
}



OSMetaClassDefineReservedUsed(IOUSBControllerV2,  2);
IOReturn		
IOUSBControllerV2::RemoveHSHub(USBDeviceAddress highSpeedHub)
{
    return _commandGate->runAction(DOHSHubMaintenance, (void *)(UInt32)highSpeedHub,
			(void *)(UInt32)kUSBHSHubCommandRemoveHub, NULL);
}

    

OSMetaClassDefineReservedUsed(IOUSBControllerV2,  3);
IOReturn		
IOUSBControllerV2::SetTestMode(UInt32 mode, UInt32 port)
{
    return _commandGate->runAction(DOSetTestMode, (void *)mode, (void *)port);
}

OSMetaClassDefineReservedUsed(IOUSBControllerV2,  4);
IOReturn 		
IOUSBControllerV2::UIMSetTestMode(UInt32 mode, UInt32 port)
{
    return kIOReturnIPCError;			// not implemented
}

OSMetaClassDefineReservedUsed(IOUSBControllerV2,  5);
UInt64
IOUSBControllerV2::GetMicroFrameNumber(void)
{
    return 0;			// not implemented
}

OSMetaClassDefineReservedUnused(IOUSBControllerV2,  6);
OSMetaClassDefineReservedUnused(IOUSBControllerV2,  7);
OSMetaClassDefineReservedUnused(IOUSBControllerV2,  8);
OSMetaClassDefineReservedUnused(IOUSBControllerV2,  9);
OSMetaClassDefineReservedUnused(IOUSBControllerV2,  10);
OSMetaClassDefineReservedUnused(IOUSBControllerV2,  11);
OSMetaClassDefineReservedUnused(IOUSBControllerV2,  12);
OSMetaClassDefineReservedUnused(IOUSBControllerV2,  13);
OSMetaClassDefineReservedUnused(IOUSBControllerV2,  14);
OSMetaClassDefineReservedUnused(IOUSBControllerV2,  15);
OSMetaClassDefineReservedUnused(IOUSBControllerV2,  16);
OSMetaClassDefineReservedUnused(IOUSBControllerV2,  17);
OSMetaClassDefineReservedUnused(IOUSBControllerV2,  18);
OSMetaClassDefineReservedUnused(IOUSBControllerV2,  19);
OSMetaClassDefineReservedUnused(IOUSBControllerV2,  20);
OSMetaClassDefineReservedUnused(IOUSBControllerV2,  21);
OSMetaClassDefineReservedUnused(IOUSBControllerV2,  22);
OSMetaClassDefineReservedUnused(IOUSBControllerV2,  23);
OSMetaClassDefineReservedUnused(IOUSBControllerV2,  24);
OSMetaClassDefineReservedUnused(IOUSBControllerV2,  25);
OSMetaClassDefineReservedUnused(IOUSBControllerV2,  26);
OSMetaClassDefineReservedUnused(IOUSBControllerV2,  27);
OSMetaClassDefineReservedUnused(IOUSBControllerV2,  28);
OSMetaClassDefineReservedUnused(IOUSBControllerV2,  29);

