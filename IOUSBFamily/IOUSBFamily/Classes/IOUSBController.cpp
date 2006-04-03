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
#include <libkern/OSByteOrder.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSData.h>

#include <IOKit/assert.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOCommandPool.h>
#include <IOKit/IOTimerEventSource.h>

#include <IOKit/usb/IOUSBController.h>
#include <IOKit/usb/IOUSBControllerV2.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/USBSpec.h>
#include <IOKit/usb/IOUSBRootHubDevice.h>
#include <IOKit/usb/IOUSBLog.h>
#include <IOKit/usb/IOUSBWorkLoop.h>
#include <IOKit/pccard/IOPCCard.h>

//================================================================================================
//
//   Local Definitions
//
//================================================================================================
#define kUSBSetup 		kUSBNone
#define kAppleCurrentAvailable	"AAPL,current-available"
#define kUSBBusID		"AAPL,bus-id"

#define super IOUSBBus

enum {
    kSetupSent  = 0x01,
    kDataSent   = 0x02,
    kStatusSent = 0x04,
    kSetupBack  = 0x10,
    kDataBack   = 0x20,
    kStatusBack = 0x40
};

enum {
    kSizeOfCommandPool = 50,
    kSizeOfIsocCommandPool = 50,
    kSizeToIncrementCommandPool = 50,
    kSizeToIncrementIsocCommandPool = 50
    
};

struct IOUSBSyncCompletionTarget
{
    IOUSBController *	controller;		// Used to access our object in the completion
    void *		flag;					// Contains the variable used to sleep/wake the threads
};

typedef struct IOUSBSyncCompletionTarget IOUSBSyncCompletionTarget;

//================================================================================================
//
//   Globals (static member variables)
//
//================================================================================================
#define kUSBSetup 		kUSBNone
#define kAppleCurrentAvailable	"AAPL,current-available"
#define kUSBBusID		"AAPL,bus-id"
#define	kMaxNumberUSBBusses	256

// These are really a static member variable (system wide global)
//
IOUSBLog		*IOUSBController::_log;				
const IORegistryPlane	*IOUSBController::gIOUSBPlane = 0;
UInt32			IOUSBController::_busCount;
bool			IOUSBController::gUsedBusIDs[kMaxNumberUSBBusses];


//================================================================================================
//
//   Callback routines to implement synchronous requests
//
//================================================================================================
//

void IOUSBSyncCompletion(   void *	target,
                            void * 	parameter,
                            IOReturn	status,
                            UInt32	bufferSizeRemaining)
{
    IOCommandGate * 	commandGate;
    IOUSBController *	me;

    IOUSBSyncCompletionTarget	*syncTarget = (IOUSBSyncCompletionTarget*)target;
    if ( !syncTarget )
    {
        USBError(1,"IOUSBController::IOUSBSyncCompletion syncTarget is NULL");
        return;
    }
    
    me = syncTarget->controller;
    if ( !me )
    {
        USBError(1,"IOUSBController::IOUSBSyncCompletion controller is NULL");
        return;
    }
    
    commandGate = me->GetCommandGate();
    if ( !commandGate )
    {
        USBError(1,"IOUSBController::IOUSBSyncCompletion commandGate is NULL");
        return;
    }
    
    if(parameter != NULL) {
        *(UInt32 *)parameter -= bufferSizeRemaining;
    }

    // Wake it up
    //
    // USBLog(6,"%s[%p]::IOUSBSyncCompletion calling commandWakeUp (%p,%p,%p)", me->getName(), me, syncTarget, syncTarget->controller, syncTarget->flag);
    commandGate->commandWakeup(syncTarget->flag,  true);
}

void IOUSBSyncIsoCompletion(void *		target,
                               void * 		parameter,
                               IOReturn		status,
                               IOUSBIsocFrame *	pFrames)
{
    IOCommandGate * 	commandGate;
    IOUSBController *	me;

    IOUSBSyncCompletionTarget	*syncTarget = (IOUSBSyncCompletionTarget*)target;
    if ( !syncTarget )
    {
        USBError(1,"IOUSBController::IOUSBSyncIsoCompletion syncTarget is NULL");
        return;
    }

    me = syncTarget->controller;
    if ( !me )
    {
        USBError(1,"IOUSBController::IOUSBSyncIsoCompletion controller is NULL");
        return;
    }

    commandGate = me->GetCommandGate();
    if ( !commandGate )
    {
        USBError(1,"IOUSBController::IOUSBSyncIsoCompletion commandGate is NULL");
        return;
    }
    
    // Wake it up
    // USBLog(6,"%s[%p]::IOUSBSyncIsoCompletion calling commandWakeUp (%p,%p,%p)",me->getName(), me, syncTarget, syncTarget->controller, syncTarget->flag);
    commandGate->commandWakeup(syncTarget->flag,  true);
}



//================================================================================================
//
//   IOKit Constructors and Destructors
//
//================================================================================================
//
OSDefineMetaClass( IOUSBController, IOUSBBus )
OSDefineAbstractStructors(IOUSBController, IOUSBBus)


//================================================================================================
//
//   IOUSBController Methods
//
//================================================================================================
//
bool 
IOUSBController::init(OSDictionary * propTable)
{
    if (!super::init(propTable))  return false;
    
    // allocate our expansion data
    if (!_expansionData)
    {
	_expansionData = (ExpansionData *)IOMalloc(sizeof(ExpansionData));
	if (!_expansionData)
	    return false;
	bzero(_expansionData, sizeof(ExpansionData));
    }

    _watchdogTimerActive = false;
    _pcCardEjected = false;
    
    // Use other controller INIT routine to override this.
    // This needs to be set before start.
    _controllerSpeed = kUSBDeviceSpeedFull;	
        
    
    return (true);
}



bool 
IOUSBController::start( IOService * provider )
{
    int			i;
    IOReturn	err = kIOReturnSuccess;
	bool		commandGateAdded = false;
    
    if( !super::start(provider))
        return (false);

    do {

        if (!_log)
            _log = IOUSBLog::usblog();

        /*
         * Initialize the Workloop and Command gate
         */
        _workLoop = IOUSBWorkLoop::workLoop();
        if (!_workLoop)
        {
            USBError(1,"%s: unable to create workloop", getName());
            break;
        }

        _commandGate = IOCommandGate:: commandGate(this, NULL);
        if (!_commandGate)
        {
            USBError(1,"%s: unable to create command gate", getName());
            break;
        }

        if (_workLoop->addEventSource(_commandGate) != kIOReturnSuccess)
        {
            USBError(1,"%s: unable to add command gate", getName());
            break;
        }
        
		commandGateAdded = true;
		
	_freeUSBCommandPool = IOCommandPool::withWorkLoop(_workLoop);
        if (!_freeUSBCommandPool)
        {
            USBError(1,"%s: unable to create free command pool", getName());
            break;
        }

	_freeUSBIsocCommandPool = IOCommandPool::withWorkLoop(_workLoop);
        if (!_freeUSBIsocCommandPool)
        {
            USBError(1,"%s: unable to create free command pool", getName());
            break;
        }
        
        _watchdogUSBTimer = IOTimerEventSource::timerEventSource(this, WatchdogTimer);
        if (!_watchdogUSBTimer)
        {
            USBError(1, "IOUSBController::start - no watchdog timer");
            break;
        }
        if (_workLoop->addEventSource(_watchdogUSBTimer) != kIOReturnSuccess)
        {
            USBError(1, "IOUSBController::start - unable to add watchdog timer event source");
            break;
        }

        // allocate a thread_call structure
        //
        _terminatePCCardThread = thread_call_allocate((thread_call_func_t)TerminatePCCard, (thread_call_param_t)this);
        if ( !_terminatePCCardThread )
        {
            USBError(1, "[%p] %s could not allocate thread functions.  Aborting start", this, getName());
        }
        
        for (i = 1; i < kUSBMaxDevices; i++)
        {
            _addressPending[i] = false;
        }

        // allocate 50 (kSizeOfCommandPool) commands of each type
        //
	for (i=0; i < kSizeOfCommandPool; i++)
	{
	    IOUSBCommand *command = IOUSBCommand::NewCommand();
	    if (command)
		_freeUSBCommandPool->returnCommand(command);
	}
        _currentSizeOfCommandPool = kSizeOfCommandPool;
	
        for (i=0; i < kSizeOfIsocCommandPool; i++)
	{
	    IOUSBIsocCommand *icommand = IOUSBIsocCommand::NewCommand();
	    if (icommand)
		_freeUSBIsocCommandPool->returnCommand(icommand);
        }
        _currentSizeOfIsocCommandPool = kSizeOfIsocCommandPool;
        
        PMinit();
        provider->joinPMtree(this);
//        IOPMRegisterDevice(pm_vars->ourName,this);	// join the power management tree
        
        /*
         * Initialize the UIM
         */
        if (UIMInitialize(provider) != kIOReturnSuccess)
        {
            USBError(1,"%s: unable to initialize UIM", getName());
            break;
        }

        /*
         * Initialize device zero
         */
	_devZeroLock = false;
            
        if (!gIOUSBPlane)
        {
            gIOUSBPlane = IORegistryEntry::makePlane(kIOUSBPlane);
            if ( gIOUSBPlane == 0 )
                USBError(1,"%s: unable to create IOUSB plane");
        }

        err = CreateRootHubDevice( provider, &_rootHubDevice );
        if ( err != kIOReturnSuccess )
            break;
        
        // Match a driver for the rootHubDevice
        //
        _rootHubDevice->registerService();
        
        _watchdogTimerActive = true;
        _watchdogUSBTimer->setTimeoutMS(kUSBWatchdogTimeoutMS);
        
        return true;

    } while (false);

    if (_commandGate)	
	{
		if (commandGateAdded && _workLoop )
	        _workLoop->removeEventSource( _commandGate );
		
		_commandGate->release();
		_commandGate = NULL;
	}
	
    if (_workLoop)	
	{
		_workLoop->release();
		_workLoop = NULL;
	}
	
    return( false );
}



IOWorkLoop *
IOUSBController::getWorkLoop() const
{
    return _workLoop;
}



/*
 * CreateDevice:
 *  This method just creates the device so we can minimally talk to it, e.g.
 *  get it's descriptor.  After this method, the device is not ready for
 *  prime time.
 */
IOReturn 
IOUSBController::CreateDevice(	IOUSBDevice 		*newDevice,
                                USBDeviceAddress	deviceAddress,
                                UInt8		 	maxPacketSize,
                                UInt8			speed,
                                UInt32			powerAvailable)
{

    USBLog(5,"%s: CreateDevice: addr=%d, speed=%s, power=%d", getName(), 
             deviceAddress, (speed == kUSBDeviceSpeedLow) ? "low" :  ((speed == kUSBDeviceSpeedFull) ? "full" : "high"), (int)powerAvailable*2);
    
    _addressPending[deviceAddress] = true;			// in case the INIT takes a long time
    do 
    {
        if (!newDevice->init(deviceAddress, powerAvailable, speed, maxPacketSize))
        {
            USBLog(3,"%s[%p]::CreateDevice device->init failed", getName(), this);
            break;
        }
        
        if (!newDevice->attach(this))
        {
            USBLog(3,"%s[%p]::CreateDevice device->attach failed", getName(), this);
            break;
        }
        
        if (!newDevice->start(this))
        {
            USBLog(3,"%s[%p]::CreateDevice device->start failed", getName(), this);
            newDevice->detach(this);
            break;
        }

        USBLog(2, "%s[%p]::CreateDevice - releasing pend on address %d", getName(), this, deviceAddress);
        _addressPending[deviceAddress] = false;

        return(kIOReturnSuccess);

    } while (false);

    // What do we do with the pending address here?  We should clear it and then
    // make sure that the caller to CreateDevice disables the port
    //
    _addressPending[deviceAddress] = false;

    return(kIOReturnNoMemory);
}


/*
 * DeviceRequest:
 * Queue up a low level device request.  It's very simple because the
 * device has done all the error checking.  Commands get allocated here and get
 * deallocated in the handlers.
 *
 */ 
IOReturn 
IOUSBController::DeviceRequest(IOUSBDevRequest *request, IOUSBCompletion *completion, USBDeviceAddress address, UInt8 ep)
{
    return DeviceRequest(request, completion, address, ep, ep ? 0 : kUSBDefaultControlNoDataTimeoutMS, ep ? 0 : kUSBDefaultControlCompletionTimeoutMS);
}



IOReturn 
IOUSBController::DeviceRequest(IOUSBDevRequestDesc *request, IOUSBCompletion *	completion, USBDeviceAddress address, UInt8 ep)
{
    return DeviceRequest(request, completion, address, ep, ep ? 0 : kUSBDefaultControlNoDataTimeoutMS, ep ? 0 : kUSBDefaultControlCompletionTimeoutMS);
}



USBDeviceAddress 
IOUSBController::GetNewAddress(void)
{
    int 	i;
    bool 	assigned[kUSBMaxDevices];
    OSIterator * clients;

    bzero(assigned, sizeof(assigned));

    clients = getClientIterator();

    if (clients)
    {
        OSObject *next;
        while( (next = clients->getNextObject()) )
        {
            IOUSBDevice *testIt = OSDynamicCast(IOUSBDevice, next);
            if (testIt)
            {
                assigned[testIt->GetAddress()] = true;
            }
        }
        clients->release();
    }

    // Add check to see if an address is pending attachment to the IOService
    //
    for (i = 1; i < kUSBMaxDevices; i++)
    {
        if ( _addressPending[i] == true )
        {
            assigned[i] = true;
        }
    }

    // If you want addresses to start at the top (127) and decrease, use the following:
    // for (i = kUSBMaxDevices-1; i > 0; i--)
    //
    for (i = 1; i < kUSBMaxDevices; i++)
    {
	if (!assigned[i])
        {
            return i;
        }
    }

    USBLog(2, "%s[%p]::GetNewAddress - ran out of new addresses!", getName(), this);
    return (0);	// No free device addresses!
}



/*
 * ControlPacket:
 *   Send a USB control packet which consists of at least two stages: setup
 * and status.   Optionally there can be multiple data stages.
 *
 */
IOReturn 
IOUSBController::ControlTransaction(IOUSBCommand *command)
{
    IOUSBDevRequest	*request = command->GetRequest();
    UInt8		direction = (request->bmRequestType >> kUSBRqDirnShift) & kUSBRqDirnMask;
    UInt8		endpoint = command->GetEndpoint();
    UInt16		wLength = request->wLength;
    IOReturn		err = kIOReturnSuccess;
    IOUSBCompletion	completion;
    IOMemoryDescriptor	*requestMemoryDescriptor;
    IOMemoryDescriptor	*bufferMemoryDescriptor = NULL;
    
    do
    {
        // Setup Stage

	completion = command->GetUSLCompletion();
	if(completion.action == NULL)
	{
	    completion.target    = (void *)this;
	    completion.action    = (IOUSBCompletionAction) &ControlPacketHandler;
	    completion.parameter = (void *)command;
	}
	else
	{
            USBLog(7,"%s[%p]::ControlTransaction new version, using V2 completion", getName(), this);
	}

	// the request is in Host format, so I need to convert the 16 bit fields to bus format
	request->wValue = HostToUSBWord(request->wValue);
	request->wIndex = HostToUSBWord(request->wIndex);
	request->wLength = HostToUSBWord(request->wLength);
	
	// set up the memory descriptor for the request
	requestMemoryDescriptor = IOMemoryDescriptor::withAddress(request, 8, kIODirectionOut);
	if (!requestMemoryDescriptor)
	{
	    err = kIOReturnNoMemory;
	    break;
	}
	    
	requestMemoryDescriptor->prepare();
	
	// set up the memory descriptor (if needed) for the buffer
	if (wLength && (request->pData != NULL) && (command->GetSelector() != DEVICE_REQUEST_DESC))
	{
	    bufferMemoryDescriptor = IOMemoryDescriptor::withAddress(request->pData, wLength, (direction == kUSBIn) ? kIODirectionIn : kIODirectionOut);
	    if (!bufferMemoryDescriptor)
	    {
		err = kIOReturnNoMemory;
		break;
	    }
	    bufferMemoryDescriptor->prepare();
	}

        command->SetDataRemaining(wLength);
        command->SetStage(kSetupSent);
        command->SetUSLCompletion(completion);
	command->SetStatus(kIOReturnSuccess);
	command->SetRequestMemoryDescriptor(requestMemoryDescriptor);
	command->SetBufferMemoryDescriptor(bufferMemoryDescriptor);
	command->SetMultiTransferTransaction(true);
	command->SetFinalTransferInTransaction(false);
        USBLog(7,"\t%s[%p]  Queueing Setup TD (dir=%d) packet=0x%08lx%08lx",getName(), this,
              direction, *(UInt32*)request, *((UInt32*)request+1));
        err = UIMCreateControlTransfer(
                    command->GetAddress(),	// functionAddress
                    endpoint,	 		// endpointNumber
                    command,   			// command
                    requestMemoryDescriptor,	// descriptor
                    true,			// bufferRounding
                    8,				// packet size
                    kUSBSetup);			// direction
        if (err)
        {
            USBError(1,"ControlTransaction: control packet 1 error 0x%x", err);
            break;
        }

        // Data Stage
        if (wLength && (request->pData != NULL))
        {
            USBLog(7,"\t%s[%p] Queueing Data TD (dir=%d, wLength=0x%x, pData=%lx)", getName(), this,
		direction, wLength, (UInt32)request->pData);
            command->SetStage(command->GetStage() | kDataSent);
            if(command->GetSelector() == DEVICE_REQUEST_DESC)
                err = UIMCreateControlTransfer(
			command->GetAddress(),		// functionAddress
			endpoint,	 		// endpointNumber
			command,   			// command
			command->GetBuffer(),		// buffer (already a memory desc)
			true,				// bufferRounding
			wLength,			// buffer size
			direction);			// direction
            else
                err = UIMCreateControlTransfer(
			command->GetAddress(),		// functionAddress
			endpoint,	 		// endpointNumber
			command,   			// command
			bufferMemoryDescriptor,		// buffer
			true,				// bufferRounding
			wLength,			// buffer size
			direction);			// direction
            if (err)
            {
                char theString[255]="";
                sprintf(theString, "ControlTransaction: control packet 2 error 0x%x", err);
                // panic(theString);
                USBError(1, theString);
                break;
            }
            
        }
        //else
            //direction = kUSBIn;
	direction = kUSBOut + kUSBIn - direction;		// swap direction

        // Status Stage
        USBLog(7,"\t%s[%p]  Queueing Status TD (dir=%d)", getName(), this, direction);
        command->SetStage(command->GetStage() | kStatusSent);
	command->SetFinalTransferInTransaction(true);
        err = UIMCreateControlTransfer(
			command->GetAddress(),		// functionAddress
			endpoint,	 		// endpointNumber
			command,   			// command
			(IOMemoryDescriptor *)NULL,	// buffer
			true,				// bufferRounding
			0,				// buffer size
			direction);			// direction
        if (err)
        {
            char theString[255]="";
            sprintf(theString, "%s[%p]::ControlTransaction: control packet 3 error 0x%x", getName(), this, err);
            USBError(1, theString);
            break;
        }
    } while(false);

    return(err);
}



/*
 * ControlPacketHandler:
 * Handle all three types of control packets and maintain what stage
 * we're at.  When we receive the last one, then call the clients
 * completion routine.
 */
void 
IOUSBController::ControlPacketHandler( OSObject * 	target,
                                            void *	parameter,
                                            IOReturn	status,
                                            UInt32	bufferSizeRemaining)
{
    IOUSBCommand 	*command = (IOUSBCommand *)parameter;
    IOUSBDevRequest	*request;
    UInt8		sent, back, todo;
    Boolean in = false;
    IOUSBController *	me = (IOUSBController *)target;

    USBLog(7,"%s[%p]::ControlPacketHandler lParam=%lx  status=0x%x bufferSizeRemaining=0x%x", me->getName(), me, 
	    (UInt32)parameter, status, (unsigned int)bufferSizeRemaining);

    if (command == 0)
        return;

    request = command->GetRequest();

    // on big endian systems, swap these fields back so the client isn't surprised
    request->wValue = USBToHostWord(request->wValue);
    request->wIndex = USBToHostWord(request->wIndex);
    request->wLength = USBToHostWord(request->wLength);

    sent = (command->GetStage() & 0x0f) << 4;
    back = command->GetStage() & 0xf0;
    todo = sent ^ back; /* thats xor */

    if ( status != kIOReturnSuccess )
        USBLog(5, "%s[%p]::ControlPacketHandler(FN: %d, EP:%d): Error: 0x%x, stage: 0x%x, todo: 0x%x", me->getName(), me, command->GetAddress(), command->GetEndpoint(), status, command->GetStage(), todo );

    if((todo & kSetupBack) != 0)
        command->SetStage(command->GetStage() | kSetupBack);
    else if((todo & kDataBack) != 0)
    {
        // This is the data transport phase, so this is the interesting one
        command->SetStage(command->GetStage() | kDataBack);
        command->SetDataRemaining(bufferSizeRemaining);
	in = (((request->bmRequestType >> kUSBRqDirnShift) & kUSBRqDirnMask) == kUSBIn);
    }
    else if((todo & kStatusBack) != 0)
    {
        command->SetStage(command->GetStage() | kStatusBack);
	in = (((request->bmRequestType >> kUSBRqDirnShift) & kUSBRqDirnMask) != kUSBIn);
    }
    else
        USBLog(5,"%s[%p]::ControlPacketHandler: Spare transactions, This seems to be harmless", me->getName(), me);

    back = command->GetStage() & 0xf0;
    todo = sent ^ back; /* thats xor */

    if (status != kIOReturnSuccess)
    {
        if ( (status != kIOUSBTransactionReturned) && (status != kIOReturnAborted) && ( status != kIOUSBTransactionTimeout) )
        {
            USBDeviceAddress	addr = command->GetAddress();
            UInt8		endpt = command->GetEndpoint();
	    IOUSBControllerV2   *v2Bus;
    
            command->SetStatus(status);

	    if (status == kIOUSBHighSpeedSplitError)
	    {
		USBLog(1,"%s[%p]::ControlPacketHandler - kIOUSBHighSpeedSplitError", me->getName(), me);
		v2Bus = OSDynamicCast(IOUSBControllerV2, me);
		if (v2Bus)
		{
		    USBLog(1,"%s[%p]::ControlPacketHandler - calling clear TT", me->getName(), me);
		    v2Bus->ClearTT(addr, endpt, in);
		}
		status = kIOReturnNotResponding;
	    }    

            USBLog(3,"%s[%p]:ControlPacketHandler error 0x%x occured on endpoint (%d).  todo = 0x%x (Clearing stall)", me->getName(), me, status, endpt, todo);
            
            // We used to only clear the endpoint (and hence return all transactions on that endpoint) stall for endpoint 0.  However, a
            // closer reading of the spec indicates that is OPTIONAL to have a control pipe stall so we now clear the stall for all control
            // pipes.  This will make sure that all the phases of a control transaction get returned if an error occurs.
            //
            me->UIMClearEndpointStall(addr, endpt, kUSBAnyDirn);
        }
        else if (command->GetStatus() == kIOReturnSuccess)
        {
            if ( status == kIOUSBTransactionReturned )
                command->SetStatus(kIOReturnAborted);
	    else
		command->SetStatus(status);
        }
    }
    
    if (todo == 0)
    {
        USBLog(7,"%s[%p]::ControlPacketHandler: transaction complete status=0x%x", me->getName(), me, status);
    
	// release my memory descriptors as needed
	if (command->GetRequestMemoryDescriptor())
	{
	    command->GetRequestMemoryDescriptor()->complete();
	    command->GetRequestMemoryDescriptor()->release();
	    command->SetRequestMemoryDescriptor(NULL);
	}
	
	if (command->GetBufferMemoryDescriptor())
	{
	    command->GetBufferMemoryDescriptor()->complete();
	    command->GetBufferMemoryDescriptor()->release();
	    command->SetBufferMemoryDescriptor(NULL);
	}

        // Don't report a status on a short packet
        //
       if ( status == kIOReturnUnderrun )
            command->SetStatus(kIOReturnSuccess);
	    
        if (command->GetStatus() != kIOReturnSuccess)
	{
	    USBLog(2, "%s[%p]::ControlPacketHandler, returning status of %x", me->getName(), me, command->GetStatus());
	}
        
        // Call the clients handler
        me->Complete(command->GetClientCompletion(), command->GetStatus(), command->GetDataRemaining());

		// Only return the command if this is NOT a synchronous request.  For Sync requests, we return it later
		//
		if ( !command->GetIsSyncTransfer() )
	me->_freeUSBCommandPool->returnCommand(command);
    }
    else
        USBLog(7,"%s[%p]::ControlPacketHandler: still more to come: todo=0x%x", me->getName(), me, todo);
        
}



/*
 * InterruptTransaction:
 *   Send a USB interrupt packet.
 *
 */
IOReturn 
IOUSBController::InterruptTransaction(IOUSBCommand *command)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBCompletion	completion;

    completion.target 	 = (void *)this;
    completion.action 	 = (IOUSBCompletionAction) &IOUSBController::InterruptPacketHandler;
    completion.parameter = (void *)command;

    command->SetUSLCompletion(completion);
    command->SetBufferRounding(true);
    
    err = UIMCreateInterruptTransfer(command);

    return(err);
}



void 
IOUSBController::InterruptPacketHandler(OSObject * target, void * parameter, IOReturn status, UInt32 bufferSizeRemaining)
{
    IOUSBCommand 	*command = (IOUSBCommand *)parameter;
    IOUSBController 	*me = (IOUSBController *)target;
    AbsoluteTime	timeStart, timeNow;
    UInt64       	timeElapsed;

    if (command == 0)
        return;

    USBLog(7,"%s[%p]::InterruptPacketHandler: addr %d:%d(%s) complete status=0x%x bufferSizeRemaining = %d (%d)",  me->getName(), me, command->GetAddress(), command->GetEndpoint(), command->GetDirection() == kUSBIn ? "in" : "out", status, (int)bufferSizeRemaining, command->GetReqCount());

    if ( status == kIOUSBTransactionReturned )
        status = kIOReturnAborted;
        
    // Write the status out to our IOUSBCommand so that the completion routine can access it
    //
    command->SetStatus(status);

    /* Call the clients handler */
    if ( command->GetUseTimeStamp() )
    {
        IOUSBCompletion			completion = command->GetClientCompletion();
        IOUSBCompletionWithTimeStamp	completionWithTimeStamp;

        // Copy the completion to a completion with time stamp
        //
        completionWithTimeStamp.target = completion.target;
        completionWithTimeStamp.parameter = completion.parameter;
        completionWithTimeStamp.action = (IOUSBCompletionActionWithTimeStamp) completion.action;
        
        me->CompleteWithTimeStamp( completionWithTimeStamp, status, bufferSizeRemaining, command->GetTimeStamp());
    }
    else
        me->Complete(command->GetClientCompletion(), status, bufferSizeRemaining);

    // Only return the command if this is NOT a synchronous request.  For Sync requests, we return it later
	//
	if ( !command->GetIsSyncTransfer() )
    me->_freeUSBCommandPool->returnCommand(command);
}



/*
 * BulkTransaction:
 *   Send a USB bulk packet.
 *
 */
IOReturn 
IOUSBController::BulkTransaction(IOUSBCommand *command)
{
    IOUSBCompletion	completion;
    IOReturn		err = kIOReturnSuccess;

    
    completion.target 	 = (void *)this;
    completion.action 	 = (IOUSBCompletionAction) &IOUSBController::BulkPacketHandler;
    completion.parameter = (void *)command;

    command->SetUSLCompletion(completion);
    command->SetBufferRounding(true);
    
    err = UIMCreateBulkTransfer(command);

    if (err)
        USBLog(3,"%s[%p]::BulkTransaction: error queueing bulk packet (0x%x)", getName(), this, err);

    return(err);
}



void 
IOUSBController::BulkPacketHandler(OSObject *target, void *parameter, IOReturn	status, UInt32 bufferSizeRemaining)
{
    IOUSBCommand 	*command = (IOUSBCommand *)parameter;
    IOUSBController	*me = (IOUSBController *)target;

    
    if (command == 0)
        return;

    USBLog(7,"%s[%p]::BulkPacketHandler: addr %d:%d(%s) complete status=0x%x bufferSizeRemaining = %d (%d)", me->getName(), me, command->GetAddress(), command->GetEndpoint(), command->GetDirection() == kUSBIn ? "in" : "out", status, (int)bufferSizeRemaining, command->GetReqCount());

     if ( status == kIOUSBTransactionReturned )
        status = kIOReturnAborted;

     command->SetStatus(status);

     /* Call the clients handler */
    me->Complete(command->GetClientCompletion(), status, bufferSizeRemaining);

    // Only return the command if this is NOT a synchronous request.  For Sync requests, we return it later
	//
	if ( !command->GetIsSyncTransfer() )
    me->_freeUSBCommandPool->returnCommand(command);
}



/*
 * Isoc Transaction:
 *   Send a Isochronous packet.
 *
 */
IOReturn 
IOUSBController::DoIsocTransfer(OSObject *owner, void *cmd,
                        void *, void *, void *)
{
    IOUSBController *		controller = (IOUSBController *)owner;
    IOUSBIsocCommand *		command = (IOUSBIsocCommand *) cmd;
    IOUSBIsocCompletion 	completion;
    IOReturn			kr = kIOReturnSuccess;

    if ( !controller || !command )
    {
        USBError(1,"IOUSBController[%p]::DoIsocTransfer -- bad controller or command (%p,%p)", controller, controller, command);
        return kIOReturnBadArgument;
    }
    
    // If we have a synchronous completion, then we need to sleep
    // this thread (unless we're on the workloop thread).  A synchronous completion will have IOUSBSyncCompletion
    // as our completion action.
    //
    completion = command->GetCompletion();
    
    if ( (UInt32) completion.action == (UInt32) &IOUSBSyncIsoCompletion )
    {
        IOUSBSyncCompletionTarget	syncTarget;
        bool				inCommandSleep = true;

        if ( controller->getWorkLoop()->onThread() )
        {
            USBError(1,"%s[%p]::DoIsocTransfer sync request on workloop thread.  Use async!", controller->getName(), controller);
            return kIOUSBSyncRequestOnWLThread;
        }

        // Fill out our structure that the completion routine will use to wake
        // the thread up.  Set this structure as the target for the completion
        //
        syncTarget.controller = controller;
        syncTarget.flag = &inCommandSleep;
        completion.target = &syncTarget;
        command->SetCompletion(completion);

        // Now, do the transaction and put the thread to sleep
        //
        kr = controller->IsocTransaction(command);

        // If we didn't get an immediate error, then put the thread to sleep and wait for it to wake up
        //
        if ( kr == kIOReturnSuccess )
        {
			IOCommandGate * 	commandGate = controller->GetCommandGate();

            //USBLog(3,"%s[%p]::DoIsocTransfer calling commandSleep (%p,%p,%p)", controller->getName(), controller, &syncTarget, syncTarget.controller, syncTarget.flag);
            kr = commandGate->commandSleep(&inCommandSleep, THREAD_UNINT);
            inCommandSleep = false;
            //USBLog(3,"%s[%p]::DoIsocTransfer woke up: 0x%x, 0x%x", controller->getName(), controller, command->GetStatus(), kr);

            // We need to return the result of the transfer here, not just the result of the commandSleep()
            //
           kr = command->GetStatus();
        }
    }
    else
    {
        kr = controller->IsocTransaction(command);
    }

    return kr;
}

IOReturn 
IOUSBController::DoLowLatencyIsocTransfer(OSObject *owner, void *cmd,
                        void *, void *, void *)
{
    IOUSBController *		controller = (IOUSBController *)owner;
    IOUSBIsocCommand *		command = (IOUSBIsocCommand *) cmd;
    IOUSBIsocCompletion 	completion;
    IOReturn			kr = kIOReturnSuccess;

    if ( !controller || !command )
    {
        USBError(1,"IOUSBController[%p]::DoLowLatencyIsocTransfer -- bad controller or command (%p,%p)", controller, controller, command);
        return kIOReturnBadArgument;
    }

    // If we have a synchronous completion, then we need to sleep
    // this thread (unless we're on the workloop thread).  A synchronous completion will have IOUSBSyncCompletion
    // as our completion action.
    //
    completion = command->GetCompletion();
    
    if ( (UInt32) completion.action == (UInt32) &IOUSBSyncIsoCompletion )
    {
        IOUSBSyncCompletionTarget	syncTarget;
        bool				inCommandSleep = true;

        if ( controller->getWorkLoop()->onThread() )
        {
            USBError(1,"%s[%p]::DoLowLatencyIsocTransfer sync request on workloop thread.  Use async!", controller->getName(), controller);
            return kIOUSBSyncRequestOnWLThread;
        }

        // Fill out our structure that the completion routine will use to wake
        // the thread up.  Set this structure as the target for the completion
        //
        syncTarget.controller = controller;
        syncTarget.flag = &inCommandSleep;
        completion.target = &syncTarget;
        command->SetCompletion(completion);

        // Now, do the transaction and put the thread to sleep
        //
        kr = controller->LowLatencyIsocTransaction(command);

        // If we didn't get an immediate error, then put the thread to sleep and wait for it to wake up
        //
        if ( kr == kIOReturnSuccess )
        {
			IOCommandGate * 	commandGate = controller->GetCommandGate();

            //USBLog(3,"%s[%p]::DoLowLatencyIsocTransfer calling commandSleep (%p,%p,%p)", controller->getName(), controller, &syncTarget, syncTarget.controller, syncTarget.flag);
            kr = commandGate->commandSleep(&inCommandSleep, THREAD_UNINT);
            inCommandSleep = false;
            //USBLog(3,"%s[%p]::DoLowLatencyIsocTransfer woke up: 0x%x, 0x%x", controller->getName(), controller, command->GetStatus(), kr);

            // We need to return the result of the transfer here, not just the result of the commandSleep()
            //
            kr = command->GetStatus();
        }
    }
    else
    {
        kr = controller->LowLatencyIsocTransaction(command);
    }

    return kr;
}



IOReturn 
IOUSBController::IsocTransaction(IOUSBIsocCommand *command)
{
    IOUSBIsocCompletion		completion;
    IOReturn				err = kIOReturnSuccess;


    completion.target 	 = (void *)this;
    completion.action 	 = (IOUSBIsocCompletionAction) &IOUSBController::IsocCompletionHandler;
    completion.parameter = (void *)command;

    err = UIMCreateIsochTransfer(   command->GetAddress(),		// functionAddress
									command->GetEndpoint(),		// endpointNumber
									completion,					// completion
									command->GetDirection(),	// direction
									command->GetStartFrame(),   // Start frame
									command->GetBuffer(),		// buffer
									command->GetNumFrames(),	// number of frames
									command->GetFrameList()		// transfer for each frame
									);

    if (err) {
        USBLog(3,"%s[%p]::IsocTransaction: error queueing isoc transfer (0x%x)", getName(), this, err);
    }
    return(err);
}



IOReturn 
IOUSBController::LowLatencyIsocTransaction(IOUSBIsocCommand *command)
{
    IOUSBIsocCompletion	completion;
    IOReturn		err = kIOReturnSuccess;


    completion.target 	 = (void *)this;
    completion.action 	 = (IOUSBIsocCompletionAction) &IOUSBController::IsocCompletionHandler;
    completion.parameter = (void *)command;

    err = UIMCreateIsochTransfer(   command->GetAddress(),									// functionAddress
									command->GetEndpoint(),									// endpointNumber
									completion,												// completion
									command->GetDirection(),								// direction
									command->GetStartFrame(),								// Start frame
									command->GetBuffer(),									// buffer
									command->GetNumFrames(),								// number of frames
									(IOUSBLowLatencyIsocFrame *) command->GetFrameList(),   // transfer for each frame
									command->GetUpdateFrequency()							// How often do we update frameList
									);

    if (err) {
        USBLog(3,"%s[%p]::IsocTransaction: error queueing isoc transfer (0x%x)", getName(), this, err);
    }
    return(err);
}



void 
IOUSBController::IsocCompletionHandler(OSObject *target,
                                        void * 	parameter,
                                        IOReturn	status,
                                        IOUSBIsocFrame	*pFrames)
{
    IOUSBIsocCommand 	*command = (IOUSBIsocCommand *)parameter;
    IOUSBController	*me = (IOUSBController *)target;

    if (command == 0)
        return;

	// Write the status to our command so that we can access it from our
	// commandWake, if this was a synchronous call
	//
    command->SetStatus(status);

    /* Call the clients handler */
    IOUSBIsocCompletion completion = command->GetCompletion();
    if (completion.action)  
	(*completion.action)(completion.target, completion.parameter, status, pFrames);

    // Only return the command if this is NOT a synchronous request.  For Sync requests, we return it later
	//
	if ( !command->GetIsSyncTransfer() )
    me->_freeUSBIsocCommandPool->returnCommand(command);
}



void
IOUSBController::WatchdogTimer(OSObject *target, IOTimerEventSource *source)
{
    IOUSBController*	me = OSDynamicCast(IOUSBController, target);
    IOReturn		err;

    if (!me || !source || me->isInactive() || me->_pcCardEjected )
    {
        me->_watchdogTimerActive = false;
        return;
    }
     
    // reset the clock
    err = source->setTimeoutMS(kUSBWatchdogTimeoutMS);
    if (err)
    {
        // do not remove the braces around this USBLog call, or the code will be wrong!
        USBError(1, "%s[%p]::WatchdogTime: error 0x%08x", me->getName(), me, err);
    }
    else
    {
        me->UIMCheckForTimeouts();
    }
    
}



IOReturn 
IOUSBController::DoIOTransfer(OSObject *owner,
                           void *cmd,
                           void *, void *, void *)
{
    IOUSBController *	controller = (IOUSBController *)owner;
    IOUSBCommand *	command = (IOUSBCommand *) cmd;
    IOReturn		err = kIOReturnSuccess;
    IOUSBCompletion 	completion;
    IOUSBCompletion 	disjointCompletion;
	IOCommandGate * 	commandGate;

    if ( !controller || !command )
    {
        USBError(1,"IOUSBController[%p]::DoIOTransfer -- bad controller or command (%p,%p)", controller, controller, command);
        return kIOReturnBadArgument;
    }

    if ( controller->_pcCardEjected )
    {
        USBLog(1, "%s[%p]::DoIOTransfer - trying to queue when PC Card ejected", controller->getName(), controller);
        return kIOReturnNotResponding;
    }

	commandGate = controller->GetCommandGate();

    // If we have a synchronous completion, then we need to sleep
    // this thread.  A synchronous completion will have
    //
    // 1. IOUSBSyncCompletion as our completion action OR
    // 2. IOUSBSyncCompletion as the disjointCompletion;
    //
    completion = command->GetClientCompletion();
    disjointCompletion = command->GetDisjointCompletion();

   // if ( ( (UInt32) completion.action == (UInt32) &IOUSBSyncCompletion) ||
   //      ( (UInt32) disjointCompletion.action == (UInt32) &IOUSBSyncCompletion) )
	if ( command->GetIsSyncTransfer() )
    {
        IOUSBSyncCompletionTarget	syncTarget;
        bool				inCommandSleep = true;

        if ( controller->getWorkLoop()->onThread() )
        {
            USBError(1,"%s[%p]::DoIOTransfer sync request on workloop thread.  Use async!", controller->getName(), controller);
            return kIOUSBSyncRequestOnWLThread;
        }

        // Fill out our structure that the completion routine will use to wake
        // the thread up.  Set this structure as the target for the completion
        //
        syncTarget.controller = controller;
        syncTarget.flag = &inCommandSleep;

        if ( (UInt32) completion.action == (UInt32) &IOUSBSyncCompletion )
        {
            completion.target = &syncTarget;
            command->SetClientCompletion(completion);
        }
        else
        {
            disjointCompletion.target = &syncTarget;
            command->SetDisjointCompletion(disjointCompletion);
        }
        
        // Now, do the transaction and put the thread to sleep
        //
        switch (command->GetType())
        {
            case kUSBInterrupt:
                err = controller->InterruptTransaction(command);

                // If we didn't get an immediate error, then put the thread to sleep and wait for it to wake up
                //
                if ( err == kIOReturnSuccess )
                {

                    //USBLog(6,"%s[%p]::DoIOTransfer(Interrupt) calling commandSleep (%p,%p,%p)", controller->getName(), controller, &syncTarget, syncTarget.controller, syncTarget.flag);
                    err = commandGate->commandSleep(&inCommandSleep, THREAD_UNINT);
                    inCommandSleep = false;
                    //USBLog(6,"%s[%p]::DoIOTransfer(Interrupt) woke up: 0x%x, 0x%x", controller->getName(), controller, command->GetStatus(), err);

                    // We need to return the result of the transfer here, not the result of the commandSleep()
                    //
                   err = command->GetStatus();
                }
                break;

            case kUSBBulk:
                err = controller->BulkTransaction(command);
                if ( err == kIOReturnSuccess )
                {
                    //USBLog(6,"%s[%p]::DoIOTransfer(Bulk) calling commandSleep (%p,%p,%p)", controller->getName(), controller, &syncTarget, syncTarget.controller, syncTarget.flag);
                    err = commandGate->commandSleep(&inCommandSleep, THREAD_UNINT);
                    inCommandSleep = false;
                    //USBLog(6,"%s[%p]::DoIOTransfer(Bulk) woke up: 0x%x, 0x%x", controller->getName(), controller, command->GetStatus(), err);

                    // We need to return the result of the transfer here, not just the result of the commandSleep()
                    //
                    err = command->GetStatus();
                }
                break;
                
            case kUSBIsoc:
                USBLog(3,"%s[%p]::DoIOTransfer  Isoc transactions not supported on non-isoc pipes!!", controller->getName(), controller);
                err = kIOReturnBadArgument;
                break;
                
            default:
                USBLog(3,"%s[%p]::DoIOTransfer  Unknown transaction type", controller->getName(), controller);
                err = kIOReturnBadArgument;
                break;
        }
        
    }
    else
    {
    	// It was NOT a synchronous call, so don't need to set any threads to sleep
    	//
        switch (command->GetType())
        {
            case kUSBInterrupt:
                err = controller->InterruptTransaction(command);
                break;
            case kUSBIsoc:
                USBLog(3,"%s[%p]::DoIOTransfer  Isoc transactions not supported on non-isoc pipes!!", controller->getName(), controller);
                err = kIOReturnBadArgument;
                break;
            case kUSBBulk:
                err = controller->BulkTransaction(command);
                break;
            default:
                USBLog(3,"%s[%p]::DoIOTransfer  Unknown transaction type", controller->getName(), controller);
                err = kIOReturnBadArgument;
                break;
        }
    }

    if (err)
    {
        // prior to 1.8.3f5, we would call the completion routine here. that seems
        // a mistake, so we don't do it any more
        USBError(1, "%s[%p]::DoIOTransfer - error 0x%x queueing request", controller->getName(), controller, err);
    }

    return err;
}



IOReturn 
IOUSBController::DoControlTransfer(OSObject *owner,
                           void *arg0, void *arg1,
                           void *arg2, void *arg3)
{
    IOUSBController *	controller = (IOUSBController *)owner;
    IOUSBCommand *	command = (IOUSBCommand *) arg0;
    IOUSBCompletion 	completion;
    IOUSBCompletion 	disjointCompletion;
    IOReturn		kr = kIOReturnSuccess;

    if ( !controller || !command )
    {
        USBError(1,"IOUSBController[%p]::DoControlTransfer -- bad controller or command (%p,%p)", controller, controller, command);
        return kIOReturnBadArgument;
    }
    
    if ( controller->_pcCardEjected )
    {
        USBLog(1, "%s[%p]::DoControlTransfer - trying to queue when PC Card ejected", controller->getName(), controller);
        return kIOReturnNotResponding;
    }

    // If we have a synchronous completion, then we need to sleep
    // this thread (unless we are on the workloop thread).  A synchronous completion will have
    //
    // 1. IOUSBSyncCompletion as our completion action OR
    // 2. IOUSBSyncCompletion as the disjointCompletion;
    //
    completion = command->GetClientCompletion();
    disjointCompletion = command->GetDisjointCompletion();

    if ( ( (UInt32) completion.action == (UInt32) &IOUSBSyncCompletion) ||
         ( (UInt32) disjointCompletion.action == (UInt32) &IOUSBSyncCompletion) )
    {
        IOUSBSyncCompletionTarget	syncTarget;
        bool				inCommandSleep = true;

        if ( controller->getWorkLoop()->onThread() )
        {
            USBError(1,"%s[%p]::DoControlTransfer sync request on workloop thread.  Use async!", controller->getName(), controller);
            return kIOUSBSyncRequestOnWLThread;
        }

        // Fill out our structure that the completion routine will use to wake
        // the thread up.  Set this structure as the target for the completion
        //
        syncTarget.controller = controller;
        syncTarget.flag = &inCommandSleep;

        if ( (UInt32) completion.action == (UInt32) &IOUSBSyncCompletion )
        {
            completion.target = &syncTarget;
            command->SetClientCompletion(completion);
        }
        else
        {
            disjointCompletion.target = &syncTarget;
            command->SetDisjointCompletion(disjointCompletion);
        }

        // Now, do the transaction and put the thread to sleep
        //
        kr = controller->ControlTransaction((IOUSBCommand *)arg0);

        // If we didn't get an immediate error, then put the thread to sleep and wait for it to wake up
        //
        if ( kr == kIOReturnSuccess )
        {
			IOCommandGate * 	commandGate = controller->GetCommandGate();

            //USBLog(6,"%s[%p]::DoControlTransfer calling commandSleep (%p,%p,%p)", controller->getName(), controller, &syncTarget, syncTarget.controller, syncTarget.flag);
            kr = commandGate->commandSleep(&inCommandSleep, THREAD_UNINT);
            inCommandSleep = false;
            //USBLog(6,"%s[%p]::DoControlTransfer woke up: 0x%x, 0x%x", controller->getName(), controller, command->GetStatus(), kr);

            // We need to return the result of the transfer here, not  the result of the commandSleep()
            //
          	kr = command->GetStatus();
        }
    }
    else
    {
        kr = controller->ControlTransaction((IOUSBCommand *)arg0);
    }

    return kr;
}



IOReturn 
IOUSBController::DoDeleteEP(OSObject *owner,
                           void *arg0, void *arg1,
                           void *arg2, void *arg3)
{
    IOUSBController *me = (IOUSBController *)owner;

    return me->UIMDeleteEndpoint((short)(UInt32) arg0, (short)(UInt32) arg1, (short)(UInt32) arg2);
}



IOReturn 
IOUSBController::DoAbortEP(OSObject *owner,
                           void *arg0, void *arg1,
                           void *arg2, void *arg3)
{
    IOUSBController *me = (IOUSBController *)owner;

    return me->UIMAbortEndpoint((short)(UInt32) arg0, (short)(UInt32) arg1, (short)(UInt32) arg2);
}



IOReturn 
IOUSBController::DoClearEPStall(OSObject *owner,
                           void *arg0, void *arg1,
                           void *arg2, void *arg3)
{
    IOUSBController *me = (IOUSBController *)owner;

    return me->UIMClearEndpointStall((short)(UInt32) arg0, (short)(UInt32) arg1, (short)(UInt32) arg2);
}



IOReturn 
IOUSBController::DoCreateEP(OSObject *owner,
                           void *arg0, void *arg1,
                           void *arg2, void *arg3)
{
    IOUSBController *me = (IOUSBController *)owner;
    UInt8 address = (UInt8)(UInt32) arg0;
    UInt8 speed = (UInt8)(UInt32) arg1;
    Endpoint *endpoint = (Endpoint *)arg2;
    IOReturn err;

    USBLog(7,"%s[%p]::DoCreateEP, no high speed ancestor", me->getName(), me);

    switch (endpoint->transferType)
    {
        case kUSBInterrupt:
            err = me->UIMCreateInterruptEndpoint(address,
                                             endpoint->number,
                                             endpoint->direction,
                                             speed,
                                             endpoint->maxPacketSize,
                                             endpoint->interval);
            break;

        case kUSBBulk:
            err = me->UIMCreateBulkEndpoint(address,
                                        endpoint->number,
                                        endpoint->direction,
                                        speed,
                                        endpoint->maxPacketSize);
            break;

        case kUSBControl:
            err = me->UIMCreateControlEndpoint(address,
                                           endpoint->number,
                                           endpoint->maxPacketSize,
                                           speed);
            break;

        case kUSBIsoc:
            err = me->UIMCreateIsochEndpoint(address,
                                        endpoint->number,
                                        endpoint->maxPacketSize,
                                        endpoint->direction);
            break;

        default:
            err = kIOReturnBadArgument;
            break;
    }
    return (err);
}


//
// This is a static method protected by the workLoop, since this is the only place in which we set and unset
// the _devZeroLock, it is safe. If we need it, we will do a commandSleep to release the workLoop lock
// until another thread is done with the _devZeroLock
//
IOReturn
IOUSBController::ProtectedDevZeroLock(OSObject *target, void* lock, void* arg2, void* arg3, void* arg4)
{
    IOUSBController	*me = (IOUSBController*)target;
	IOCommandGate * 	commandGate = me->GetCommandGate();
    
    USBLog(5, "%s[%p]::ProtectedAcquireDevZeroLock - about to %s device zero lock", me->getName(), me, lock ? "obtain" : "release");
    if (lock)
    {
	if (!me->_devZeroLock)
	{
	    USBLog(5, "%s[%p]::ProtectedAcquireDevZeroLock - not already locked - obtaining", me->getName(), me);
	    me->_devZeroLock = true;
	    return kIOReturnSuccess;
	}
	USBLog(5, "%s[%p]::ProtectedAcquireDevZeroLock - somebody already has it - running commandSleep", me->getName(), me);
	while (me->_devZeroLock)
	{
		commandGate->commandSleep(&me->_devZeroLock, THREAD_UNINT);
	    if (me->_devZeroLock)
		USBLog(5, "%s[%p]::ProtectedAcquireDevZeroLock - _devZeroLock still held - back to sleep", me->getName(), me);
	}
	me->_devZeroLock = true;
	return kIOReturnSuccess;
    }
    else
    {
	USBLog(5, "%s[%p]::ProtectedAcquireDevZeroLock - releasing lock", me->getName(), me);
	me->_devZeroLock = false;
	commandGate->commandWakeup(&me->_devZeroLock, true);
	USBLog(5, "%s[%p]::ProtectedAcquireDevZeroLock - wakeup done", me->getName(), me);
	return kIOReturnSuccess;
    }
}



IOReturn 
IOUSBController::AcquireDeviceZero()
{
    IOReturn err = 0;
    Endpoint ep;
	IOCommandGate * 	commandGate = GetCommandGate();

    ep.number = 0;
    ep.transferType = kUSBControl;
    ep.maxPacketSize = 8;

    USBLog(6,"%s[%p]: Trying to acquire Device Zero", getName(), this);
    commandGate->runAction(ProtectedDevZeroLock, (void*)true);
    // IOTakeLock(_devZeroLock);

    USBLog(5,"%s[%p]: Acquired Device Zero", getName(), this);

//    err = OpenPipe(0, kUSBDeviceSpeedFull, &ep);
// BT we don't need to do this, it just confuses the UIM
// Paired with delete in ConfigureDeviceZero

    return(err);
}



void 
IOUSBController::ReleaseDeviceZero(void)
{
    IOReturn err = 0;
	IOCommandGate * 	commandGate = GetCommandGate();

    err = commandGate->runAction(DoDeleteEP, (void *)0, (void *)0, (void *)kUSBAnyDirn);
    err = commandGate->runAction(ProtectedDevZeroLock, (void*)false);

    USBLog(5,"%s[%p]:: Released Device Zero", getName(), this);

    return;
}



void 
IOUSBController::WaitForReleaseDeviceZero()
{
	IOCommandGate * 	commandGate = GetCommandGate();

    commandGate->runAction(ProtectedDevZeroLock, (void*)true);
    commandGate->runAction(ProtectedDevZeroLock, (void*)false);
}



IOReturn 
IOUSBController::ConfigureDeviceZero(UInt8 maxPacketSize, UInt8 speed)
{
    IOReturn	err = kIOReturnSuccess;
    Endpoint ep;

    ep.number = 0;
    ep.transferType = kUSBControl;
    ep.maxPacketSize = maxPacketSize;

    USBLog(6, "%s[%p]::ConfigureDeviceZero (maxPacketSize: %d, Speed: %d)", getName(), this, maxPacketSize, speed);

    // BT paired with OpenPipe in AcquireDeviceZero
    //
    err = OpenPipe(0, speed, &ep);

    return(err);
}


IOReturn 
IOUSBController::GetDeviceZeroDescriptor(IOUSBDeviceDescriptor *desc, UInt16 size)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevRequest	request;

    USBLog(6, "%s[%p]::GetDeviceZeroDescriptor (size: %d)", getName(), this, size);

    do
    {
        IOUSBCompletion			tap;

		// The action of IOUSBSyncCompletion will tell the USL that this is a sync transfer
		//
        tap.target = NULL;
        tap.action = &IOUSBSyncCompletion;
        tap.parameter = NULL;

        if (!desc)
        {
            err = kIOReturnBadArgument;
            break;
        }

        request.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice);
        request.bRequest = kUSBRqGetDescriptor;
        request.wValue = kUSBDeviceDesc << 8;
        request.wIndex = 0;
        request.wLength = size;
        request.pData = desc;

        err = DeviceRequest(&request, &tap, 0, 0);

    } while(false);

    if (err)
    {
        USBLog(3,"%s[%p]::GetDeviceZeroDescriptor Error: 0x%x", getName(), this, err);
    }
    
    return err;
}



IOReturn 
IOUSBController::SetDeviceZeroAddress(USBDeviceAddress address) 
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevRequest	request;

    USBLog(6, "%s[%p]::SetDeviceZeroAddress (%d)", getName(), this, address);

    do
    {
        IOUSBCompletion	tap;

		// The action of IOUSBSyncCompletion will tell the USL that this is a sync transfer
		//
        tap.target = NULL;
        tap.action = &IOUSBSyncCompletion;
        tap.parameter = NULL;

        
        request.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBStandard, kUSBDevice);
        request.bRequest = kUSBRqSetAddress;
        request.wValue = address;
        request.wIndex = 0;
        request.wLength = 0;
        request.pData = 0;

        err = DeviceRequest(&request, &tap, 0, 0);

    } while(false);

    if (err)
    {
        USBLog(6, "%s[%p]::SetDeviceZeroAddress Error: 0x%x", getName(), this, err);
    }
    
    return(err);
}



IOUSBDevice *
IOUSBController::MakeDevice(USBDeviceAddress *	address)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevice		*newDev;

    USBLog(6, "%s[%p]::MakeDevice", getName(), this);

    newDev = IOUSBDevice::NewDevice();
    
    if (newDev == NULL)
        return NULL;

    *address = GetNewAddress();
    if(*address == 0) 
    {
        USBLog(1, "%s[%p]::MakeDevice error getting address - releasing newDev", getName(), this);
		newDev->release();
		return NULL;
    }

    err = SetDeviceZeroAddress(*address);
    
    if (err)
    {
        USBLog(1, "%s[%p]::MakeDevice error setting address. err=0x%x device=%p - releasing device", getName(), this, err, newDev);
        *address = 0;
		newDev->release();
		return NULL;
    }
	
    return(newDev);
}



IOReturn 
IOUSBController::PolledRead(
        short				functionNumber,
        short				endpointNumber,
        IOUSBCompletion			clientCompletion,
        IOMemoryDescriptor *		CBP,
        bool				bufferRounding,
        UInt32				bufferSize)
{
    IOUSBCommand 	*command = (IOUSBCommand *)_freeUSBCommandPool->getCommand(false);
    IOUSBCompletion 	uslCompletion;
    int			i;

    // If we couldn't get a command, increase the allocation and try again
    //
    if ( command == NULL )
    {
        IncreaseCommandPool();
        
        command = (IOUSBCommand *)_freeUSBCommandPool->getCommand(false);
        if ( command == NULL )
        {
            USBLog(3,"%s[%p]::DeviceRequest Could not get a IOUSBCommand",getName(),this);
            return kIOReturnNoResources;
        }
    }

    command->SetUseTimeStamp(true);
    command->SetSelector(READ);
    command->SetRequest(0);            	// Not a device request
    command->SetAddress(functionNumber);
    command->SetEndpoint(endpointNumber);
    command->SetDirection(kUSBIn);
    command->SetType(kUSBInterrupt);
    command->SetBuffer(CBP);
    command->SetClientCompletion(clientCompletion);
    command->SetBufferRounding(bufferRounding);

    for (i=0; i < 10; i++)
	command->SetUIMScratch(i, 0);
	
    uslCompletion.target    = (void *)this;
    uslCompletion.action    = (IOUSBCompletionAction) &IOUSBController::InterruptPacketHandler;
    uslCompletion.parameter = (void *)command;
    command->SetUSLCompletion(uslCompletion);

   return UIMCreateInterruptTransfer(command);
}



IOReturn 
IOUSBController::message( UInt32 type, IOService * provider,  void * argument )
{
    IOReturn err = kIOReturnSuccess;
    cs_event_t	pccardevent;
    
    switch ( type )
    {
        case kIOMessageServiceIsTerminated:
            break;
            
        // Do we really need to return success from the following two messages?
        case kIOMessageCanDevicePowerOff:
        case kIOMessageDeviceWillPowerOff:
            break;
            
        case kIOPCCardCSEventMessage:
            pccardevent = (UInt32) argument;
            
            USBLog(5,"+%s[%p]: Received kIOPCCardCSEventMessage event %d",getName(),this, (UInt32) pccardevent);
            if ( pccardevent == CS_EVENT_CARD_REMOVAL )
            {
                _pcCardEjected = true;
                
                if ( _rootHubDevice )
                {
                    thread_call_enter(_terminatePCCardThread);
                }
            }
            USBLog(5,"-%s[%p]: Received kIOPCCardCSEventMessage event %d",getName(),this, (UInt32) pccardevent);
            break;
             
        default:
            err = kIOReturnUnsupported;
            break;
    }
    
    return err;
}

void 
IOUSBController::stop( IOService * provider )
{
    UInt32		i;
    IOUSBCommand *	command;
    UInt32		retries = 0;

    USBLog(5,"+%s[%p]::stop (0x%lx)", getName(), this, (UInt32) provider);
    
    // Wait for the watchdog timer to expire.  There doesn't seem to be any
    // way to cancel a timer that is already "executing".  cancelTimeout will
    // call thread_cancel which will only cancel if the thread is pending
    // We might wait up to 5 seconds here.
    //
    while ( retries < 600 && _watchdogTimerActive )
    {
        IOSleep(100);
        retries++;
    }
    
    // Finalize the UIM -- need to do it in the stop so that other devices
    // can still use the UIM data structures while they are being terminated (e.g. we 
    // can't do it in the terminate message
    //
    UIMFinalize();

    // Indicate that this busID is no longer used
    //
    gUsedBusIDs[_busNumber] = false;
    
    // If we are the last controller, then we need to remove the gIOUSBPlane
    //
    
    // Release the devZero lock:
    //
    // IOLockFree( _devZeroLock );
    
    // We need to tell PowerMgmt that we don't exist
    // anymore (akin to PMinit.  What about joinPMtree() -- do we need to remove from tree?
    //
    PMstop();

    
    // Dispose of all the commands in the command Pool -- note that we don't block trying
    // to get the command.  
    //
    for ( i = 0; i < _currentSizeOfCommandPool; i++ )
    {
        command = (IOUSBCommand *)_freeUSBCommandPool->getCommand(false);    
        if ( command )
            command->release();
    }
    
    for ( i = 0; i < _currentSizeOfIsocCommandPool; i++ )
    {
        command = (IOUSBCommand *)_freeUSBIsocCommandPool->getCommand(false);  
        if ( command )
            command->release();
    }

    if (_terminatePCCardThread)
    {
        thread_call_cancel(_terminatePCCardThread);
        thread_call_free(_terminatePCCardThread);
    }

    // Remove our workloop related stuff
    //
    if ( _freeUSBCommandPool )
    {
        _freeUSBCommandPool->release();
        _freeUSBCommandPool = NULL;
    }
    
    if ( _freeUSBIsocCommandPool )
    {
        _freeUSBIsocCommandPool->release();
        _freeUSBIsocCommandPool = NULL;
    }
    if ( _watchdogUSBTimer )
    {
        _workLoop->removeEventSource( _watchdogUSBTimer );
        _watchdogUSBTimer->release();
        _watchdogUSBTimer = NULL;
    }
        
    if ( _commandGate )
    {
		if ( _workLoop )
			_workLoop->removeEventSource( _commandGate );
        _commandGate->release();
        _commandGate = NULL;
    }
        
    if ( _workLoop )
    {
        _workLoop->release();
        _workLoop = NULL;
    }

    USBLog(5,"-%s[%p]::stop (0x%lx)", getName(), this, (UInt32) provider);
    
}



bool 
IOUSBController::finalize(IOOptionBits options)
{
    return(super::finalize(options));
}

void
IOUSBController::IncreaseCommandPool(void)
{
    int i;

    USBLog(3,"%s[%p]::IncreaseCommandPool Adding (%d) to Command Pool", getName(), this, kSizeToIncrementCommandPool);

    for (i = 0; i < kSizeToIncrementCommandPool; i++)
    {
        IOUSBCommand *command = IOUSBCommand::NewCommand();
        if (command)
            _freeUSBCommandPool->returnCommand(command);
    }
    _currentSizeOfCommandPool += kSizeToIncrementCommandPool;

}

void
IOUSBController::IncreaseIsocCommandPool(void)
{
    int i;

    USBLog(3,"%s[%p]::IncreaseIsocCommandPool Adding (%d) to Isoc Command Pool", getName(), this, kSizeToIncrementIsocCommandPool);
    
    for (i = 0; i < kSizeToIncrementIsocCommandPool; i++)
    {
        IOUSBIsocCommand *icommand = IOUSBIsocCommand::NewCommand();
        if (icommand)
            _freeUSBIsocCommandPool->returnCommand(icommand);
    }
    _currentSizeOfIsocCommandPool += kSizeToIncrementIsocCommandPool;
}

void
IOUSBController::Complete(IOUSBCompletion	completion,
                          IOReturn		status,
                          UInt32		actualByteCount)
{
    if (completion.action)  (*completion.action)(completion.target,
                                                 completion.parameter,
                                                 status,
                                                 actualByteCount);
}


void
IOUSBController::CompleteWithTimeStamp(IOUSBCompletionWithTimeStamp	completion,
                                       IOReturn		status,
                                       UInt32		actualByteCount,
                                       AbsoluteTime	timeStamp)
{
    if (completion.action)  (*completion.action)(completion.target,
                                                 completion.parameter,
                                                 status,
                                                 actualByteCount,
                                                 timeStamp);
}


IOCommandGate *
IOUSBController::GetCommandGate(void) 
{ 
    return _commandGate; 
}


void
IOUSBController::TerminatePCCard(OSObject *target)
{
    IOUSBController *	me = OSDynamicCast(IOUSBController, target);
    bool	ok;
    
    if (!me)
        return;

    USBLog(5,"%s[%p]::TerminatePCCard Terminating RootHub", me->getName(),me);
    ok = me->_rootHubDevice->terminate(kIOServiceRequired | kIOServiceSynchronous);
    if ( !ok )
        USBLog(3,"%s[%p]::TerminatePCCard Could not terminate RootHub device",me->getName());
    
    me->_rootHubDevice->detachAll(gIOUSBPlane);
    me->_rootHubDevice->release();
    me->_rootHubDevice = NULL;
}

//=============================================================================================
//
//  ParsePCILocation and ValueOfHexDigit
//
//	ParsePCILocation is used to get the device number and function number of our PCI device
//      from its IOKit location.  It takes a string formated in hex as XXXX,YYYY and will get
//      the device number from XXXX and function number from YYYY.  Ideally one would use sscanf
//	for this, but the kernel's sscanf is severly limited.
//
//=============================================================================================
//
#define ISDIGIT(c)	((c >= '0') && (c <= '9'))
#define ISHEXDIGIT(c)	(ISDIGIT(c) || ((c >= 'A') && (c <= 'F')) || ((c >= 'a') && (c <= 'f')))

int
IOUSBController::ValueOfHexDigit(char c)
{
    if ( ISDIGIT(c) )
        return c - '0';
    else
        if ( ( c >= 'A' && c <= 'F') )
            return c - 'A' + 0x0A;
    else
        return c - 'a' + 0xA;
}

void
IOUSBController::ParsePCILocation(const char *str, int *deviceNum, int *functionNum)
{
    int value;
    int i;

    *deviceNum = *functionNum = 0;
    
    for ( i = 0; i < 2; i++)
    {
        // If the first character is not a hex digit, then
        // we will just return;
        if ( !ISHEXDIGIT(*str) )
            break;

        // Parse through the string until we find a non-hex digit
        //
        value = 0;
        do {
            value = (value * 16) - ValueOfHexDigit(*str);
            str++;
        } while (ISHEXDIGIT(*str));

        if ( i == 0)
            *deviceNum = -value;
        else
            *functionNum = -value;

        // If there is no functionNum, just return
        //
        if ( *str == '\0')
            break;

        // There should be a "," between the two #'s, so skip it
        //
        str++;
    }
}


OSMetaClassDefineReservedUsed(IOUSBController,  0);
void IOUSBController::UIMCheckForTimeouts(void)
{
    // this would normally be a pure virtual method which must be implemented in the UIM, but
    // since it was not in the first UIM API, we implement a "NOP" version here
    return;
}


OSMetaClassDefineReservedUsed(IOUSBController,  1);
IOReturn
IOUSBController::UIMCreateControlTransfer(  short		functionNumber, 
					    short 		endpointNumber, 
					    IOUSBCommand* 	command, 
					    void*		CBP,
                                            bool		bufferRounding,
                                            UInt32		bufferSize,
                                            short		direction)
{
    // This would normally be a pure virtual function which is implemented only in the UIM. However, we didn't
    // do it that way in release 1.8 of IOUSBFamily. So to maintain binary compatibility, I am implementing it
    // in the controller class as a call to the old method. This method will be overriden with "new" UIMs which
    // will then have access to the IOUSBCommand data structure
    USBError(1, "IOUSBController::UIMCreateControlTransfer, got into wrong one");
    return UIMCreateControlTransfer(functionNumber, endpointNumber, command->GetUSLCompletion(), CBP, bufferRounding, bufferSize, direction);
}



OSMetaClassDefineReservedUsed(IOUSBController,  2);
IOReturn
IOUSBController::UIMCreateControlTransfer(   short			functionNumber,
                                             short			endpointNumber,
                                             IOUSBCommand*		command,
                                             IOMemoryDescriptor*	CBP,
                                             bool			bufferRounding,
                                             UInt32			bufferSize,
                                             short			direction)
{
    // This would normally be a pure virtual function which is implemented only in the UIM. However, we didn't
    // do it that way in release 1.8 of IOUSBFamily. So to maintain binary compatibility, I am implementing it
    // in the controller class as a call to the old method. This method will be overriden with "new" UIMs which
    // will then have access to the IOUSBCommand data structure
    USBError(1, "IOUSBController::UIMCreateControlTransfer, got into wrong one");
    return UIMCreateControlTransfer(functionNumber, endpointNumber, command->GetUSLCompletion(), CBP, bufferRounding, bufferSize, direction);
}



OSMetaClassDefineReservedUsed(IOUSBController,  3);
IOReturn
IOUSBController::UIMCreateBulkTransfer(IOUSBCommand* command)
{
    // This would normally be a pure virtual function which is implemented only in the UIM. However, we didn't
    // do it that way in release 1.8 of IOUSBFamily. So to maintain binary compatibility, I am implementing it
    // in the controller class as a call to the old method. This method will be overriden with "new" UIMs which
    // will then have access to the IOUSBCommand data structure
    return UIMCreateBulkTransfer(command->GetAddress(), 
				 command->GetEndpoint(),
				 command->GetUSLCompletion(), 
				 command->GetBuffer(), 
				 command->GetBufferRounding(), 
				 command->GetReqCount(),
				 command->GetDirection());
}



OSMetaClassDefineReservedUsed(IOUSBController,  4);
IOReturn
IOUSBController::UIMCreateInterruptTransfer(IOUSBCommand* command)
{
    // This would normally be a pure virtual function which is implemented only in the UIM. However, we didn't
    // do it that way in release 1.8 of IOUSBFamily. So to maintain binary compatibility, I am implementing it
    // in the controller class as a call to the old method. This method will be overriden with "new" UIMs which
    // will then have access to the IOUSBCommand data structure
    return UIMCreateInterruptTransfer(command->GetAddress(), 
					command->GetEndpoint(),
					command->GetUSLCompletion(), 
					command->GetBuffer(), 
					command->GetBufferRounding(), 
					command->GetReqCount(),
					command->GetDirection());
}



OSMetaClassDefineReservedUsed(IOUSBController,  5);
/*
 * DeviceRequest:
 * Queue up a low level device request.  It's very simple because the
 * device has done all the error checking.  Commands get allocated here and get
 * deallocated in the handlers.
 *
 */ 
IOReturn 
IOUSBController::DeviceRequest(IOUSBDevRequest *request, IOUSBCompletion *completion, USBDeviceAddress address, UInt8 ep, UInt32 noDataTimeout, UInt32 completionTimeout)
{
    IOUSBCommand *			command;
    IOReturn				err = kIOReturnSuccess; 
    IOUSBCompletion			nullCompletion;
	int						i;
	
	USBLog(7,"%s[%p]::DeviceRequest [%x,%x],[%x,%x],[%x,%lx]",getName(),this, 
             request->bmRequestType,
             request->bRequest,
             request->wValue,
             request->wIndex,
             request->wLength,
             (UInt32)request->pData);
	
	if ( GetCommandGate() == 0)
		return kIOReturnInternalError;
	
	// Allocate the command
	//
	command = (IOUSBCommand *)_freeUSBCommandPool->getCommand(false);
	
    // If we couldn't get a command, increase the allocation and try again
    //
    if ( command == NULL )
    {
        IncreaseCommandPool();
        
        command = (IOUSBCommand *)_freeUSBCommandPool->getCommand(false);
        if ( command == NULL )
        {
            USBLog(3,"%s[%p]::DeviceRequest Could not get a IOUSBCommand",getName(),this);
            return kIOReturnNoResources;
        }
    }

	// Set up a flag indicating that we have a synchronous request in this command
	//
    if (  (UInt32) completion->action == (UInt32) &IOUSBSyncCompletion )
		command->SetIsSyncTransfer(true);
	else
		command->SetIsSyncTransfer(false);

        command->SetUseTimeStamp(false);
        command->SetSelector(DEVICE_REQUEST);
        command->SetRequest(request);
        command->SetAddress(address);
	command->SetEndpoint(ep);
        command->SetType(kUSBControl);
        command->SetBuffer(0); // no buffer for device requests
        command->SetClientCompletion(*completion);
	command->SetNoDataTimeout(noDataTimeout);
	command->SetCompletionTimeout(completionTimeout);

	/* Set the USL completion to NULL, so the high speed controller can do its own thing. */
        nullCompletion.target = (void *) NULL;
        nullCompletion.action = (IOUSBCompletionAction) NULL;
        nullCompletion.parameter = (void *) NULL;
        command->SetUSLCompletion(nullCompletion);
        command->SetDisjointCompletion(nullCompletion);
	
	for (i=0; i < 10; i++)
	    command->SetUIMScratch(i, 0);
	    
	err = GetCommandGate()->runAction(DoControlTransfer, command);
	
	// If we have a sync request, then we always return the command after the DoControlTransfer.  If it's an async request, we only return it if 
	// we get an immediate error
	//
	if ( command->GetIsSyncTransfer() ||  (!command->GetIsSyncTransfer() && (kIOReturnSuccess != err)) )
	{
		_freeUSBCommandPool->returnCommand(command);
	}
	
    return err;
}



OSMetaClassDefineReservedUsed(IOUSBController,  6);
IOReturn 
IOUSBController::DeviceRequest(IOUSBDevRequestDesc *request, IOUSBCompletion *	completion, USBDeviceAddress address, UInt8 ep, UInt32 noDataTimeout, UInt32 completionTimeout)
{
    IOUSBCommand *			command;
    IOReturn				err = kIOReturnSuccess; 
    IOUSBCompletion			nullCompletion;
	int						i;
	
	USBLog(7,"%s[%p]::DeviceRequestDesc [%x,%x],[%x,%x],[%x,%lx]",getName(),this, 
		   request->bmRequestType,
		   request->bRequest,
		   request->wValue,
		   request->wIndex,
		   request->wLength,
		   (UInt32)request->pData);
	
	if ( GetCommandGate() == 0)
		return kIOReturnInternalError;
	
	// Allocate the command
	//
	command = (IOUSBCommand *)_freeUSBCommandPool->getCommand(false);
	
    // If we couldn't get a command, increase the allocation and try again
    //
    if ( command == NULL )
    {
        IncreaseCommandPool();
        
        command = (IOUSBCommand *)_freeUSBCommandPool->getCommand(false);
        if ( command == NULL )
        {
            USBLog(3,"%s[%p]::DeviceRequest Could not get a IOUSBCommand",getName(),this);
            return kIOReturnNoResources;
        }
    }

	// Set up a flag indicating that we have a synchronous request in this command
	//
    if (  (UInt32) completion->action == (UInt32) &IOUSBSyncCompletion )
		command->SetIsSyncTransfer(true);
	else
		command->SetIsSyncTransfer(false);

        command->SetUseTimeStamp(false);
        command->SetSelector(DEVICE_REQUEST_DESC);
	// IOUSBDevRequest and IOUSBDevRequestDesc are same except for
        // pData (void * or descriptor).
        command->SetRequest((IOUSBDevRequest *)request);
        command->SetAddress(address);
        command->SetEndpoint(ep);
        command->SetType(kUSBControl);
        command->SetBuffer(request->pData);
        command->SetClientCompletion(*completion);
	command->SetNoDataTimeout(noDataTimeout);
	command->SetCompletionTimeout(completionTimeout);

	/* Set the USL completion to NULL, so the high speed controller can do its own thing. */
        nullCompletion.target = (void *) NULL;
        nullCompletion.action = (IOUSBCompletionAction) NULL;
        nullCompletion.parameter = (void *) NULL;
        command->SetUSLCompletion(nullCompletion);
        command->SetDisjointCompletion(nullCompletion);
        
	for (i=0; i < 10; i++)
	    command->SetUIMScratch(i, 0);

	err = GetCommandGate()->runAction(DoControlTransfer, command);
	
	// If we have a sync request, then we always return the command after the DoControlTransfer.  If it's an async request, we only return it if 
	// we get an immediate error
	//
	if ( command->GetIsSyncTransfer() ||  (!command->GetIsSyncTransfer() && (kIOReturnSuccess != err)) )
	{
		_freeUSBCommandPool->returnCommand(command);
	}
	
    return err;
}



// in IOUSBController_Pipes.cpp
//OSMetaClassDefineReservedUsed(IOUSBController,  7);
//OSMetaClassDefineReservedUsed(IOUSBController,  8);

OSMetaClassDefineReservedUsed(IOUSBController,  9);
void
IOUSBController::free()
{
    //  This needs to be the LAST thing we do, as it disposes of our "fake" member
    //  variables.
    //
    if (_expansionData)
    {
        bzero(_expansionData, sizeof(ExpansionData));
	IOFree(_expansionData, sizeof(ExpansionData));
    }

    super::free();
}

// in AppleUSBOHCI_RootHub.cpp
// OSMetaClassDefineReservedUsed(IOUSBController,  10);

OSMetaClassDefineReservedUsed(IOUSBController,  11);
IOReturn
IOUSBController::CreateRootHubDevice( IOService * provider, IOUSBRootHubDevice ** rootHubDevice)
{

    IOUSBDeviceDescriptor	desc;
    OSObject			*appleCurrentProperty;
    UInt32			pseudoBus;
    IOReturn			err = kIOReturnSuccess;
    OSNumber * 			busNumberProp;
    UInt32			bus;
    UInt32			address;
    const char *		parentLocation;
    int				deviceNum = 0, functionNum = 0;
    SInt32			busIndex;

    
    /*
     * Create the root hub device
     */
    err = GetRootHubDeviceDescriptor( &desc );
    if ( err != kIOReturnSuccess)
    {
        USBError(1,"%s: unable to get root hub descriptor", getName());
        goto ErrorExit;
    }

    *rootHubDevice = IOUSBRootHubDevice::NewRootHubDevice();
    address = GetNewAddress();
    SetHubAddress( address );
    
    err = CreateDevice(*rootHubDevice, address, desc.bMaxPacketSize0, _controllerSpeed, kUSB500mAAvailable);
    if ( err != kIOReturnSuccess)
    {
        USBError(1,"%s: unable to create and initialize root hub device", getName());
	(*rootHubDevice)->release();
	*rootHubDevice = NULL;
        goto ErrorExit;
    }

    // Increment our global _busCount (# of USB Buses) and set the properties on
    // our provider for busNumber and locationID.  This is used by Apple System Profiler.  The _busCount is NOT
    // guaranteed by IOKit to be the same across reboots (as the loading of the USB Controller driver can happen
    // in any order), but for all intents and purposes it will be the same.  This was changed from using the provider's
    // location for part of the locationID because of problems with multifunction PC and PCI cards.
    //

    parentLocation = provider->getLocation();
    if ( parentLocation )
    {
        ParsePCILocation( parentLocation, &deviceNum, &functionNum );
    }
    
    // If our provider already has a "busNumber" property, then use that one for our location ID
    // if it hasn't been used already
    //
    busNumberProp = (OSNumber *) provider->getProperty("USBBusNumber");
    if ( busNumberProp )
    {
        bus = busNumberProp->unsigned32BitValue();
    }
    else
    {
        // Take the PCI device number and function number and combine to make an 8 bit
        // quantity, in binary:  dddddfff.  If the bus already exists, then just look for the next
        // available bus entry after that.
        //
        bus = ( ((functionNum & 0x7) << 5) | (deviceNum & 0x1f) );
        
        if ( gUsedBusIDs[bus] )
        {
            //
            USBError(1,"Bus %d already taken",bus);
            
            for ( busIndex = kMaxNumberUSBBusses - 1; busIndex >= 0; busIndex-- )
            {
                if ( !gUsedBusIDs[busIndex] )
                {
                    bus = busIndex;
                    break;
                }
            }
        }
    }
    
    // We have an entry we can use so claim it
    //
    gUsedBusIDs[bus] = true;
    pseudoBus = (bus & 0xff) << 24;
    provider->setProperty("USBBusNumber", bus, 32);
    provider->setProperty(kUSBDevicePropertyLocationID, pseudoBus, 32);

    //  Save a copy of our busNumber property in a field
    _busNumber = bus;
    
    // Set our locationID property for the root hub device.  Also, set the IOKit location
    // of the root hub device to be the same as the usb controller
    //
    (*rootHubDevice)->setProperty(kUSBDevicePropertyLocationID, pseudoBus, 32);
    (*rootHubDevice)->setLocation(provider->getLocation());
    (*rootHubDevice)->setLocation(provider->getLocation(), gIOUSBPlane);


    // 2505931 If the provider has an APPL,current-available property, then stick it in the new root hub device
    //
    appleCurrentProperty = provider->getProperty(kAppleCurrentAvailable);
    if (appleCurrentProperty)
        (*rootHubDevice)->setProperty(kAppleCurrentAvailable, appleCurrentProperty);

ErrorExit:

    return err;
    
}

// in IOUSBController_Pipes.cpp
// OSMetaClassDefineReservedUsed(IOUSBController,  12);
// OSMetaClassDefineReservedUsed(IOUSBController,  13);

// in AppleUSBOHCI_RootHub.cpp
// OSMetaClassDefineReservedUsed(IOUSBController,  14);

// in IOUSBController_Pipes.cpp
//OSMetaClassDefineReservedUsed(IOUSBController,  15);

// in AppleUSBOHCI_UIM.cpp
OSMetaClassDefineReservedUsed(IOUSBController,  16);
IOReturn 		
IOUSBController::UIMCreateIsochTransfer(	short						functionAddress,
											short						endpointNumber,
											IOUSBIsocCompletion			completion,
											UInt8						direction,
											UInt64						frameStart,
											IOMemoryDescriptor			*pBuffer,
											UInt32						frameCount,
											IOUSBLowLatencyIsocFrame	*pFrames,
											UInt32						updateFrequency)
{
    // This would normally be a pure virtual function which is implemented only in the UIM. However, 
    // too maintain binary compatibility, I am implementing it
    // in the controller class as a call that returns unimplemented. This method will be overriden with "new" UIMs.
    //
    return kIOReturnUnsupported;
}

OSMetaClassDefineReservedUnused(IOUSBController,  18);
OSMetaClassDefineReservedUnused(IOUSBController,  19);

