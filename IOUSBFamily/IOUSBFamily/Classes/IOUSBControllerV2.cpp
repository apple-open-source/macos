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


extern IOReturn CheckForDisjointDescriptor(IOUSBCommand *command, UInt16 maxPacketSize);

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
	
    return (true);
}


bool 
IOUSBControllerV2::start( IOService * provider )
{
    
    if( !super::start(provider))
        return (false);

	// allocate a thread_call structure - code shared by EHCI and UHCI
	_returnIsochDoneQueueThread = thread_call_allocate((thread_call_func_t)ReturnIsochDoneQueueEntry, (thread_call_param_t)this);
	if ( !_returnIsochDoneQueueThread )
	{
		USBError(1, "IOUSBControllerV2[%p]::start - could not allocate thread callout function.",  this);
		return false;
	}
	
	return true;
}



void
IOUSBControllerV2::free()
{
	
    if (_returnIsochDoneQueueThread)
    {
        thread_call_cancel(_returnIsochDoneQueueThread);
        thread_call_free(_returnIsochDoneQueueThread);
    }
	
	// should I do an IOFree on the expansion data?
	
    super::free();
}





void
IOUSBControllerV2::clearTTHandler( OSObject *	target,
                                   void *	parameter,
                                   IOReturn	status,
                                   UInt32	bufferSizeRemaining)
{
    IOUSBController *	me = (IOUSBController *)target;
    IOUSBCommand *	command = (IOUSBCommand *)parameter;
    UInt8		sent, back, todo;
    UInt8		hubAddr = command->GetAddress();
    
    USBLog(5,"clearTTHandler: status (0x%x)", status);
	
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
	
    clearCommand->SetUseTimeStamp(false);
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
		USBLog(1, "%s[%p]::ClearTT - error 0x%x returned from ControlTransaction", getName(), this, err);
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
	
    USBLog(5,"%s[%p]::DOHSHubMaintenance, command: %ld, flags: %ld", me->getName(), me, command, flags);
	
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
	
    USBLog(5,"%s[%p]::DOSetTestMode, mode: %ld, port: %ld", me->getName(), me, mode, port);
	
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
    return kIOReturnUnsupported;			// not implemented
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
    return kIOReturnUnsupported;			// not implemented
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
    IOReturn	 	err = kIOReturnSuccess;
    IOUSBCommand 	*command;
    IOUSBCompletion 	nullCompletion;
    IOUSBCompletion 	theCompletion;
    IOUSBCompletionAction 	theAction;
    int			i;
	
    USBLog(7, "%s[%p]::ReadV2 - reqCount = %ld", getName(), this, reqCount);
	
    // Validate its a inny pipe and that there is a buffer
    if ((endpoint->direction != kUSBIn) || !buffer || (buffer->getLength() < reqCount))
    {
        USBLog(5, "%s[%p]::ReadV2 - direction is not kUSBIn (%d), No Buffer, or buffer length < reqCount (%ld < %ld). Returning kIOReturnBadArgument(0x%x)", getName(), this, endpoint->direction,  buffer->getLength(), reqCount, kIOReturnBadArgument);
        return kIOReturnBadArgument;
    }
	
    if ((endpoint->transferType != kUSBBulk) && (noDataTimeout || completionTimeout))
    {
        USBLog(5, "%s[%p]::ReadV2 - Pipe is NOT kUSBBulk (%d) AND specified a timeout (%ld, %ld).  Returning kIOReturnBadArgument(0x%x)", getName(), this, endpoint->transferType, noDataTimeout, completionTimeout, kIOReturnBadArgument);
        return kIOReturnBadArgument;							// timeouts only on bulk pipes
    }
	
    // Validate the completion
    if (!completion)
    {
        USBLog(5, "%s[%p]::ReadV2 - No Completion routine.  Returning kIOReturnNoCompletion(0x%x)", getName(), this, kIOReturnNoCompletion);
        return kIOReturnNoCompletion;
    }
	
    // Validate the command gate
    if (!_commandGate)
    {
        USBLog(5, "%s[%p]::ReadV2 - Could not get _commandGate.  Returning kIOReturnInternalError(0x%x)", getName(), this, kIOReturnInternalError);
        return kIOReturnInternalError;
    }
	
    // allocate the command
    command = (IOUSBCommand *)_freeUSBCommandPool->getCommand(false);
	
    // If we couldn't get a command, increase the allocation and try again
    //
    if ( command == NULL )
    {
        IncreaseCommandPool();
		
        command = (IOUSBCommand *)_freeUSBCommandPool->getCommand(false);
        if ( command == NULL )
        {
            USBLog(3,"%s[%p]::ReadV2 Could not get a IOUSBCommand",getName(),this);
            return kIOReturnNoResources;
        }
    }
	
    theCompletion.target = completion->target;
    theCompletion.action = (IOUSBCompletionAction)completion->action;
    theCompletion.parameter = completion->parameter;
	
	// Set up a flag indicating that we have a synchronous request in this command
	//
    if (  (UInt32) completion->action == (UInt32) &IOUSBSyncCompletion )
		command->SetIsSyncTransfer(true);
	else
		command->SetIsSyncTransfer(false);
	
    command->SetUseTimeStamp(true);
    command->SetSelector(READ);
    command->SetRequest(0);            	// Not a device request
    command->SetAddress(address);
    command->SetEndpoint(endpoint->number);
    command->SetDirection(kUSBIn);
    command->SetType(endpoint->transferType);
    command->SetBuffer(buffer);
    command->SetReqCount(reqCount);
    command->SetClientCompletion(theCompletion);
    command->SetNoDataTimeout(noDataTimeout);
    command->SetCompletionTimeout(completionTimeout);
    for (i=0; i < 10; i++)
        command->SetUIMScratch(i, 0);
	
    nullCompletion.target = (void *) NULL;
    nullCompletion.action = (IOUSBCompletionAction) NULL;
    nullCompletion.parameter = (void *) NULL;
    command->SetDisjointCompletion(nullCompletion);
	
    err = CheckForDisjointDescriptor(command, endpoint->maxPacketSize);
    if (kIOReturnSuccess == err)
	{
        err = _commandGate->runAction(DoIOTransfer, command);
		
		// If we have a sync request, then we always return the command after the DoIOTransfer.  If it's an async request, we only return it if 
		// we get an immediate error
		//
		if ( command->GetIsSyncTransfer() ||  (!command->GetIsSyncTransfer() && (kIOReturnSuccess != err)) )
		{
			_freeUSBCommandPool->returnCommand(command);
		}
	}
	else
	{
		// CheckFordDisjoint returned an error, so free up the comand
		//
		_freeUSBCommandPool->returnCommand(command);
	}
	
    return err;
}




OSMetaClassDefineReservedUsed(IOUSBControllerV2,  8);
IOReturn
IOUSBControllerV2::UIMCreateIsochEndpoint(short functionAddress, short endpointNumber, UInt32 maxPacketSize, UInt8 direction, USBDeviceAddress	highSpeedHub, int highSpeedPort, UInt8 interval)
{
	// this is the "default implementation of UIMCreateIsochEndpoint for UIMs which don't implement it.
	// In those cases the interval parameter is ignored
	return UIMCreateIsochEndpoint(functionAddress, endpointNumber, maxPacketSize, direction, highSpeedHub, highSpeedPort);
}



OSMetaClassDefineReservedUsed(IOUSBControllerV2,  9);
IOUSBControllerIsochEndpoint*
IOUSBControllerV2::AllocateIsochEP()
{
	USBError(1, "IOUSBControllerV2[%p]::AllocateIsochEP - should be overriden in a subclass", this);
	return NULL;
}



OSMetaClassDefineReservedUsed(IOUSBControllerV2,  10);
IOReturn
IOUSBControllerV2::DeallocateIsochEP(IOUSBControllerIsochEndpoint* pEP)
{
	USBLog(4, "%s[%p]::DeallocateIsochEP (%p)",getName(), this, pEP);
    pEP->nextEP = _freeIsochEPList;
    _freeIsochEPList = pEP;
    return kIOReturnSuccess;
}



OSMetaClassDefineReservedUsed(IOUSBControllerV2,  11);
IOUSBControllerIsochEndpoint*
IOUSBControllerV2::FindIsochronousEndpoint(short								functionAddress,
										   short								endpointNumber,
										   short								direction,
										   IOUSBControllerIsochEndpoint			**ppEPBack)
{
    IOUSBControllerIsochEndpoint		*pEP, *pBack;
    
    pEP = _isochEPList;
    pBack = NULL;
    while (pEP)
    {
		if ((pEP->functionAddress == functionAddress)
			&& (pEP->endpointNumber == endpointNumber)
			&& (pEP->direction == direction))
			break;
		pBack = pEP;
		pEP = pEP->nextEP;
    }
    if (pEP && ppEPBack)
		*ppEPBack = pBack;
    return pEP;
}



OSMetaClassDefineReservedUsed(IOUSBControllerV2,  12);
IOUSBControllerIsochEndpoint*
IOUSBControllerV2::CreateIsochronousEndpoint(short					functionAddress,
											 short					endpointNumber,
											 short 					direction)
{
    IOUSBControllerIsochEndpoint*			pEP;
	int										i;
    
    USBLog(4, "+%s[%p]::CreateIsochronousEndpoint (%d:%d:%d)", getName(), this, functionAddress, endpointNumber, direction);
	pEP = _freeIsochEPList;
	if (!pEP)
	{
		pEP = AllocateIsochEP();
		USBLog(4, "%s[%p]::CreateIsochronousEndpoint  called AllocateIsochEP (%p)",getName(), this, pEP);
	}
	if (pEP)
	{
		_freeIsochEPList = pEP->nextEP;									// unlink from free list
		pEP->init();													// make sure to reinitialize it
		pEP->nextEP = _isochEPList;
		_isochEPList = pEP;
		pEP->functionAddress = functionAddress;
		pEP->endpointNumber = endpointNumber;
		pEP->direction = direction;
	}
    return pEP;
}



OSMetaClassDefineReservedUsed(IOUSBControllerV2,  13);
void
IOUSBControllerV2::PutTDonToDoList(IOUSBControllerIsochEndpoint* pED, IOUSBControllerIsochListElement *pTD)
{
    USBLog(7, "AppleUSBEHCI[%p]::PutTDonToDoList - pED (%p) pTD (%p) frameNumber(%Lx)", this, pED, pTD, pTD->_frameNumber);
    // Link TD into todo list
    if(pED->toDoList == NULL)
    {
		// as the head of a new list
		pED->toDoList = pTD;
    }
    else
    {
		// at the tail of the old list
		pED->toDoEnd->_logicalNext = pTD;
    }
    // no matter what we are the new tail
    pED->toDoEnd = pTD;
	pED->onToDoList++;
    pED->activeTDs++;
}



OSMetaClassDefineReservedUsed(IOUSBControllerV2,  14);
IOUSBControllerIsochListElement *
IOUSBControllerV2::GetTDfromToDoList(IOUSBControllerIsochEndpoint* pED)
{
    IOUSBControllerIsochListElement	*pTD;
    
	// Do not call USBLog here, as this can be called from AddIsocFramesToSchedule, which holds off preemption
    pTD = pED->toDoList;
    if (pTD)
    {
		if (pTD == pED->toDoEnd)
			pED->toDoList = pED->toDoEnd = NULL;
		else
			pED->toDoList = OSDynamicCast(IOUSBControllerIsochListElement, pTD->_logicalNext);
		// USBLog(7, "AppleUSBEHCI[%p]::GetTDfromToDoList - pED (%p) pTD (%p) frameNumber(%Lx)", this, pED, pTD, pTD->_frameNumber);
		pED->onToDoList--;
    }
    return pTD;
}



OSMetaClassDefineReservedUsed(IOUSBControllerV2,  15);
void
IOUSBControllerV2::PutTDonDoneQueue(IOUSBControllerIsochEndpoint* pED, IOUSBControllerIsochListElement *pTD, bool checkDeferred)
{
    IOUSBControllerIsochListElement	*deferredTD;
	
	// Do not call USBLog here, as this can be called from AddIsocFramesToSchedule, which holds off preemption
    if (checkDeferred)
    {
		while (pED->deferredQueue && (pED->deferredQueue->_frameNumber < pTD->_frameNumber))
		{
			deferredTD = GetTDfromDeferredQueue(pED);
			PutTDonDoneQueue(pED, deferredTD, false);
		}
    }
    
    if(pED->doneQueue == NULL)
    {
		// as the head of a new list
		pED->doneQueue = pTD;
    }
    else
    {
		// at the tail of the old list
		pED->doneEnd->_logicalNext = pTD;
    }
    // and not matter what we are now the new tail
    pED->doneEnd = pTD;
	pED->onDoneQueue++;
	
	// if there are no TDs on the schedule, and no TDs on the toDO list, then we should clear out the deferred queue
	if (checkDeferred && !pED->scheduledTDs && !pED->toDoList)
	{
		deferredTD = GetTDfromDeferredQueue(pED);
		while (deferredTD)
		{
			PutTDonDoneQueue(pED, deferredTD, false);
			deferredTD = GetTDfromDeferredQueue(pED);
		}
	}
}



OSMetaClassDefineReservedUsed(IOUSBControllerV2,  16);
IOUSBControllerIsochListElement *
IOUSBControllerV2::GetTDfromDoneQueue(IOUSBControllerIsochEndpoint* pED)
{
    IOUSBControllerIsochListElement	*pTD;
    
    pTD = pED->doneQueue;
    if (pTD)
    {
		if (pTD == pED->doneEnd)
			pED->doneQueue = pED->doneEnd = NULL;
		else
			pED->doneQueue = OSDynamicCast(IOUSBControllerIsochListElement, pTD->_logicalNext);
		pED->onDoneQueue--;
		pED->activeTDs--;
    }
    return pTD;
}



OSMetaClassDefineReservedUsed(IOUSBControllerV2,  17);
void
IOUSBControllerV2::PutTDonDeferredQueue(IOUSBControllerIsochEndpoint* pED, IOUSBControllerIsochListElement *pTD)
{
	// Do not call USBLog here, as this can be called from AddIsocFramesToSchedule, which holds off preemption
	// USBLog(7, "AppleUSBEHCI[%p]::PutTDonDeferredQueue(%p, %p)", this, pED, pTD);
	
    if(pED->deferredQueue == NULL)
    {
		// as the head of a new list
		pED->deferredQueue = pTD;
    }
    else
    {
		// at the tail of the old list
		pED->deferredEnd->_logicalNext = pTD;
    }
    // and not matter what we are now the new tail
    pED->deferredEnd = pTD;
	pED->deferredTDs++;
	
}



OSMetaClassDefineReservedUsed(IOUSBControllerV2,  18);
IOUSBControllerIsochListElement *
IOUSBControllerV2::GetTDfromDeferredQueue(IOUSBControllerIsochEndpoint* pED)
{
    IOUSBControllerIsochListElement	*pTD;
    
    pTD = pED->deferredQueue;
    if (pTD)
    {
		if (pTD == pED->deferredEnd)
			pED->deferredQueue = pED->deferredEnd = NULL;
		else
			pED->deferredQueue = OSDynamicCast(IOUSBControllerIsochListElement, pTD->_logicalNext);
		pED->deferredTDs--;
    }
	USBLog(7, "AppleUSBEHCI[%p]::GetTDfromDeferredQueue(%p) returning %p", this, pED, pTD);
    return pTD;
}


// this is a static method - hence no slot
void
IOUSBControllerV2::ReturnIsochDoneQueueEntry(OSObject *target, thread_call_param_t endpointPtr)
{
    IOUSBControllerV2 *					me = OSDynamicCast(IOUSBControllerV2, target);
    IOUSBControllerIsochEndpoint* 	pEP = (IOUSBControllerIsochEndpoint*) endpointPtr;
	
    if (!me || !pEP)
        return;
	
    me->retain();
    me->ReturnIsochDoneQueue(pEP);
    me->release();
}



OSMetaClassDefineReservedUsed(IOUSBControllerV2,  19);
void 
IOUSBControllerV2::ReturnIsochDoneQueue(IOUSBControllerIsochEndpoint* pEP)
{
    IOUSBControllerIsochListElement		*pTD = GetTDfromDoneQueue(pEP);
    IOUSBIsocFrame						*pFrames = NULL;
		
    USBLog(7, "IOUSBControllerV2[%p]::ReturnIsocDoneQueue (%p)", this, pEP);
    if (pTD)
    {
		pFrames = pTD->_pFrames;
    }
	else
	{
		USBLog(7, "IOUSBControllerV2[%p]::ReturnIsocDoneQueue - no TDs to return", this);
	}
    while(pTD)
    {
		USBLog(7, "IOUSBControllerV2[%p]::ReturnIsocDoneQueue: TD %p", this, pTD);
		if( pTD->_completion.action != NULL)
		{
			IOUSBIsocCompletionAction 	pHandler;
			
			pHandler = pTD->_completion.action;
			pTD->_completion.action = NULL;
			if (pEP->accumulatedStatus == kIOUSBBufferUnderrunErr)
			{
				USBLog(1, "IOUSBControllerV2[%p]::ReturnIsocDoneQueue - kIOReturnBufferUnderrunErr (PCI issue perhaps)", this);
			}
			if (pEP->accumulatedStatus == kIOUSBBufferOverrunErr)
			{
				USBLog(1, "IOUSBControllerV2[%p]::ReturnIsocDoneQueue - kIOReturnBufferOverrunErr (PCI issue perhaps)", this);
			}
			if ((pEP->accumulatedStatus == kIOReturnOverrun) && (pEP->direction == kUSBIn))
			{
				USBLog(1, "IOUSBControllerV2[%p]::ReturnIsocDoneQueue - kIOReturnOverrun on IN - device babbling?", this);
			}
			USBLog(7, "IOUSBControllerV2[%p]::ReturnIsocDoneQueue- calling handler(target: %p, comp.param: %p, status: %p, pFrames: %p)", this,
				   pTD->_completion.target, pTD->_completion.parameter, (void*)pEP->accumulatedStatus, pFrames);
			(*pHandler) (pTD->_completion.target,  pTD->_completion.parameter, pEP->accumulatedStatus, pFrames);
			
			_activeIsochTransfers--;
			if ( _activeIsochTransfers < 0 )
			{
				USBLog(1, "IOUSBControllerV2[%p]::ReturnIsocDoneQueue - _activeIsochTransfers went negative (%d).  We lost one somewhere", this, (int)_activeIsochTransfers);
			}
			
			// if the accumulated status is aborted, then we need to keep that status until we are done
			// otherwise the status will be in the endpoint when we get to the callback case and will
			// be reset afterwards
			if (pEP->accumulatedStatus != kIOReturnAborted)
			{
				if (pEP->accumulatedStatus != kIOReturnSuccess && (pEP->accumulatedStatus != kIOReturnUnderrun) )
					USBLog(6, "IOUSBControllerV2[%p]::ReturnIsocDoneQueue - resetting status from 0x%x", this, pEP->accumulatedStatus);
				pEP->accumulatedStatus = kIOReturnSuccess;
			}
			pTD->Deallocate(this);
			pTD = GetTDfromDoneQueue(pEP);
			if (pTD)
				pFrames = pTD->_pFrames;
		}
		else
		{
			pTD->Deallocate(this);
			pTD = GetTDfromDoneQueue(pEP);
		}
    }
    
}



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

