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


//================================================================================================
//
//   Headers
//
//================================================================================================
//
#include <IOKit/usb/IOUSBController.h>
#include <IOKit/usb/IOUSBControllerV2.h>
#include <IOKit/usb/IOUSBLog.h>


//================================================================================================
//
//   Local Definitions
//
//================================================================================================
//
#define super IOUSBController

// Copied from IOUSBController
enum {
    kSetupSent  = 0x01,
    kDataSent   = 0x02,
    kStatusSent = 0x04,
    kSetupBack  = 0x10,
    kDataBack   = 0x20,
    kStatusBack = 0x40
};

#define USEDYNAMICCOMMAND 1
//================================================================================================
//
//   IOKit Constructors and Destructors
//
//================================================================================================
//
OSDefineMetaClass( IOUSBControllerV2, IOUSBController )
OSDefineAbstractStructors(IOUSBControllerV2, IOUSBController)


//================================================================================================
//
//   IOUSBControllerV2 Methods
//
//================================================================================================
//

bool 
IOUSBControllerV2::init(OSDictionary * propTable)
{
    if (!super::init(propTable))  return false;
    
    // allocate our expansion data
    if (!_v2ExpansionData)
    {
	_v2ExpansionData = (V2ExpansionData *)IOMalloc(sizeof(V2ExpansionData));
	if (!_v2ExpansionData)
	    return false;
	bzero(_v2ExpansionData, sizeof(V2ExpansionData));
    }
	
    // Use other controller INIT routine to override this.
    // This needs to be set before start.
    _controllerSpeed = kUSBDeviceSpeedHigh;
        
    return (true);
}

void IOUSBControllerV2::clearTTHandler( OSObject *target,
//void clearTTHandler( OSObject *target,
		    void *parameter,
                    IOReturn	status,
                    UInt32	bufferSizeRemaining)
{
    IOUSBController *	me = (IOUSBController *)target;
    IOUSBCommand 	*command = (IOUSBCommand *)parameter;
    UInt8		sent, back, todo;
    UInt8		hubAddr = command->GetAddress();
    
    USBLog(5,"clearTTHandler: status (%lx)", status);
//    USBLog(1,"clearTTHandler this:%p, command:%p", me, command);

    sent = (command->GetStage() & 0x0f) << 4;
    back = command->GetStage() & 0xf0;
    todo = sent ^ back; /* thats xor */

    if((todo & kSetupBack) != 0)
    {
	USBLog(2,"clearTTHandler: Setup comming back to us, check and forget");
        command->SetStage(command->GetStage() | kSetupBack);
    }
    else
    {
        command->SetStage(0);
	USBLog(5, "%s[%p]::clearTTHandler - about to free IOUSBDevRequest (%p)", me->getName(), me, command->GetRequest());
	IOFree(command->GetRequest(), sizeof(IOUSBDevRequest));
#if USEDYNAMICCOMMAND
	USBLog(5,"clearTTHandler: We've already seen the setup, deallocate command (%p)", command);
	me->_freeUSBCommandPool->returnCommand(command);   
#else
	USBLog(5,"clearTTHandler: We've already seen the setup, using static command");
#endif
    }
    if (status != kIOReturnSuccess)
    {
	USBLog(1, "%s[%p]::clearTTHandler - error response from hub, clearing hub endpoint stall", me->getName(), me);
	me->UIMClearEndpointStall(hubAddr, 0, kUSBAnyDirn);
    }
}


OSMetaClassDefineReservedUsed(IOUSBControllerV2,  6);
void 
IOUSBControllerV2::ClearTT(USBDeviceAddress fnAddress, UInt8 endpt, Boolean IN)
{
    UInt16 		wValue;
    IOUSBDevRequest 	*clearRequest;
    short 		hubAddress;
    IOUSBCommand 	*clearCommand;
    IOUSBCompletion	completion;
    int 		i;
    IOReturn 		err;

    USBLog(5,"+%s[%p]::ClearTT", getName(), this);
    hubAddress = _highSpeedHub[fnAddress];	// Address of its controlling hub.
    if(hubAddress == 0)	// Its not a high speed device, it doesn't need a clearTT
    {
	USBLog(1,"-%s[%p]::ClearTT high speed device, returning", getName(), this);
	return;
    }

#if USEDYNAMICCOMMAND
    clearCommand = (IOUSBCommand *)_freeUSBCommandPool->getCommand(false);
    if ( clearCommand == NULL )
    {
	IncreaseCommandPool();
	
	clearCommand = (IOUSBCommand *)_freeUSBCommandPool->getCommand(false);
	if ( clearCommand == NULL )
	{
	    USBLog(1,"%s[%p]::ClearTT Could not get a IOUSBCommand",getName(),this);
	    return;
	}
    }
    USBLog(6, "%s[%p]::ClearTT V2 got command (%p)", getName(), this, clearCommand);
#else
    clearCommand = _v2ExpansionData->ClearTTCommand;
    if ( clearCommand == NULL )
    {
	_v2ExpansionData->ClearTTCommand = (IOUSBCommand *)_freeUSBCommandPool->getCommand(false);
	clearCommand = _v2ExpansionData->ClearTTCommand;
	if ( clearCommand == NULL )
	{
	    USBLog(1,"%s[%p]::ClearTT Could not get a IOUSBCommand",getName(),this);
	    return;
	}
	USBLog(6,"%s[%p]::ClearTT gto static IOUSBCommand",getName(),this);
    }

    if(clearCommand->GetStage() != 0)
    {
	USBLog(1,"%s[%p]::ClearTT our command is in use, giving up",getName(),this);
	return;
    }

#endif

    
    clearRequest = (IOUSBDevRequest*)IOMalloc(sizeof(IOUSBDevRequest));
    if (!clearRequest)
    {
	USBLog(1,"%s[%p]::ClearTT Could not get a IOUSBDevRequest", getName(), this);
#if USEDYNAMICCOMMAND
	_freeUSBCommandPool->returnCommand(clearCommand);
#endif
	return;
    }
    USBLog(5, "%s[%p]::ClearTT - got IOUSBDevRequest (%p)", getName(), this, clearRequest);

    wValue = (endpt & 0xf) | ( (fnAddress & 0x7f) << 4);
    if(IN)
    {
	wValue  |= (1 << 15);
    }
    USBLog(5,"%s[%p]::ClearTT - V2 EP (%d) ADDR (%d) wValue (0x%x)", getName(), this, endpt, fnAddress, wValue);
/*		
3..0 Endpoint Number
10..4 Device Address
12..11 Endpoint Type	- Always controll == zero.
14..13 Reserved, must be zero
15 Direction, 1 = IN, 0 = OUT

Endpoint Type
00 Control
01 Isochronous
10 Bulk
11 Interrupt

*/

/* Request details largely copied from AppleUSBHubPort::ClearTT */

    clearRequest->bmRequestType = 0x23;
    clearRequest->bRequest = 8;
    clearRequest->wValue = wValue;
    if(_v2ExpansionData->_multiTT[hubAddress])
    {  // MultiTT hub needs port address here
	clearRequest->wIndex = _highSpeedPort[fnAddress];
    }
    else
    {  // Single TT hubs need 1 here
	clearRequest->wIndex = 1;
    }
    clearRequest->wLength = 0;
    clearRequest->pData = NULL;
    clearRequest->wLenDone = 0;

/* This copies large parts of IOUSBController::DeviceRequest, its not using IOUSBController::DeviceRequest */
/* Because we're already inside the lock and don't want to go through the gate again. */

    completion.target    = (void *)this;
    completion.action    = (IOUSBCompletionAction) &clearTTHandler;
    completion.parameter = clearCommand;
    clearCommand->SetUSLCompletion(completion);

    clearCommand->SetSelector(DEVICE_REQUEST);
    clearCommand->SetRequest(clearRequest);
    clearCommand->SetAddress(hubAddress);
    clearCommand->SetEndpoint(0);
    clearCommand->SetType(kUSBControl);
    clearCommand->SetBuffer(0); 			// no buffer for device requests
    clearCommand->SetClientCompletion(completion);
    clearCommand->SetNoDataTimeout(5000);
    clearCommand->SetCompletionTimeout(0);
    clearCommand->SetStage(0);
    
    for (i=0; i < 10; i++)
	clearCommand->SetUIMScratch(i, 0);

    err = ControlTransaction(clearCommand);	// Wait for completion? Or just fire and forget?
    if (err)
    {
	USBLog(1, "%s[%p]::ClearTT - error %p returned from ControlTransaction", getName(), this, err);
    }
}



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
	    USBLog(4, "%s[%p]::DoCreateEP - Creating Isoch EP with interval %d [raw %d]", me->getName(), me, (endpoint->interval == 4) ? 8 : 1, endpoint->interval);
	    // only support intervals of 1 and 8
            err = me->UIMCreateIsochEndpoint(address,
                                        endpoint->number,
                                        endpoint->maxPacketSize,
                                        endpoint->direction,
                                        me->_highSpeedHub[address],
                                        me->_highSpeedPort[address],
					(endpoint->interval == 4) ? 8 : 1);
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
    UInt8 multi;

    USBLog(5,"%s[%p]::DOHSHubMaintenance, command: %d, flags: %d", me->getName(), me, command, flags);

    multi = ((flags & kUSBHSHubFlagsMultiTT) != 0);
    me->_v2ExpansionData->_multiTT[highSpeedHub] = multi;
    USBLog(3,"%s[%p]::DOHSHubMaintenance hub at %d is multiTT:%d", me->getName(), me, highSpeedHub, me->_v2ExpansionData->_multiTT[highSpeedHub]);

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

OSMetaClassDefineReservedUsed(IOUSBControllerV2,  7);
IOReturn
IOUSBControllerV2::ReadV2(IOMemoryDescriptor *buffer, USBDeviceAddress address, Endpoint *endpoint, IOUSBCompletionWithTimeStamp *completion, UInt32 noDataTimeout, UInt32 completionTimeout, IOByteCount reqCount)
{
    return (kIOReturnUnsupported);
}




OSMetaClassDefineReservedUsed(IOUSBControllerV2,  8);
IOReturn
IOUSBControllerV2::UIMCreateIsochEndpoint(short functionAddress, short endpointNumber, UInt32 maxPacketSize, UInt8 direction, USBDeviceAddress	highSpeedHub, int highSpeedPort, UInt8 interval)
{
	// this is the "default implementation of UIMCreateIsochEndpoint for UIMs which don't implement it.
	// In those cases the interval parameter is ignored
	return UIMCreateIsochEndpoint(functionAddress, endpointNumber, maxPacketSize, direction, highSpeedHub, highSpeedPort);
}


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

