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

#include <UserNotification/KUNCUserNotifications.h>

#include <IOKit/IOKitKeys.h>
#include <IOKit/usb/USBSpec.h>  // USBHub.h should include this file
#include <IOKit/usb/USBHub.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBControllerV2.h>
#include <IOKit/usb/IOUSBLog.h>
#include "AppleUSBHubPort.h"

#define super OSObject
#define self this


OSDefineMetaClassAndStructors(AppleUSBHubPort, OSObject)

static portStatusChangeVector defaultPortVectors[kNumChangeHandlers] =
{
    { 0, kHubPortOverCurrent,	kUSBHubPortOverCurrentChangeFeature },
    { 0, kHubPortBeingReset, 	kUSBHubPortResetChangeFeature },
    { 0, kHubPortSuspend,	kUSBHubPortSuspendChangeFeature },
    { 0, kHubPortEnabled,	kUSBHubPortEnableChangeFeature },
    { 0, kHubPortConnection,	kUSBHubPortConnectionChangeFeature },
};



IOReturn 
AppleUSBHubPort::init( AppleUSBHub *parent, int portNum, UInt32 powerAvailable, bool captive )
{
    _hub		= parent;
    _bus		= parent->_bus;
    _hubDesc		= &parent->_hubDescriptor;
    _portNum		= portNum;
    _portDevice		= 0;
    _portPowerAvailable	= powerAvailable;
    _captive 		= captive;
    _state 		= hpsNormal;
    _retryPortStatus	= false;
    _statusChangedThreadActive	= false;
    _initThreadActive	= false;
    _inCommandSleep	= false;
    _attachRetry 	= 0;
    _devZeroCounter	= 0;
    
    if (!_hub || !_bus || !_hubDesc || (portNum < 1) || (portNum > 64))
        return kIOReturnError;
    
    _runLock = IOLockAlloc();
    if (!_runLock)
        return kIOReturnError;
        
    _initThread = thread_call_allocate((thread_call_func_t)PortInitEntry, (thread_call_param_t)this);
    if (!_initThread)
    {
        IOLockFree(_runLock);
        return kIOReturnError;
    }
    
    _portStatusChangedHandlerThread = thread_call_allocate((thread_call_func_t)PortStatusChangedHandlerEntry, (thread_call_param_t)this);
    
    if (!_portStatusChangedHandlerThread)
    {
        thread_call_free(_initThread);
        IOLockFree(_runLock);
        return kIOReturnError;
    }
        
    InitPortVectors();
    
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBHubPort::start(void)
{

    USBLog(5, "AppleUSBHubPort[%p]::start: forking init thread", this);
    retain();				// since we are about to schedule on a new thread
    thread_call_enter(_initThread);
    USBLog(5, "AppleUSBHubPort[%p]::start: fork complete", this);

    return kIOReturnSuccess;
}



void 
AppleUSBHubPort::stop(void)
{
    // Ugh.  This could get nasty.  What if stop is called while
    // a thread is adding a device?  I think we need to start tracking
    // states.  And we might need a flag to tell us where it's because
    // the device has been unplugged, or the system is shutting down.

    USBLog(5, "AppleUSBHubPort[%p]::stop called, _devZero = (%d).", this, _devZero);


    if ( _statusChangedThreadActive || _initThreadActive)
    {
        UInt32 retries = 0;
	IOWorkLoop *myWL = NULL;
	IOCommandGate *gate = NULL;
	
	if (_bus)
	    myWL = _bus->getWorkLoop();
	    
	if (!myWL)
	{
	    USBLog(2, "AppleUSBHubPort[%p]::stop called, no workloop.", this);
	}
	else
	{
	    gate = _bus->GetCommandGate();
	    if (!gate)
	    {
		USBLog(2, "AppleUSBHubPort[%p]::stop - i got the WL but there is no gate.", this);
	    }
	    if (myWL->onThread())
	    {
		USBLog(2, "AppleUSBHubPort[%p]::stop - i am on the main thread. DANGER AHEAD.", this);
	    }
	}

        while ( retries < 600 && ( _statusChangedThreadActive || _initThreadActive) )
        {
	    if (!myWL || !gate || myWL->onThread() || !myWL->inGate())
	    {
		IOSleep( 100 );
	    }
	    else
	    {
		USBLog(2, "AppleUSBHubPort[%p]::stop - trying command sleep (%d/%d).", this,_statusChangedThreadActive, _initThreadActive);
		_inCommandSleep = true;
		if (_statusChangedThreadActive)
		    gate->commandSleep(&_statusChangedThreadActive, THREAD_UNINT);
		else if (_initThreadActive)
		    gate->commandSleep(&_initThreadActive, THREAD_UNINT);
		_inCommandSleep = false;
		USBLog(2, "AppleUSBHubPort[%p]::stop - returned from command sleep (%d,%d)!!", this, _statusChangedThreadActive, _initThreadActive);
	    }
            retries++;
        }
    }
    if ( _statusChangedThreadActive || _initThreadActive)
    {
	USBLog(2, "AppleUSBHubPort[%p]::stop - not quiesced - just returning", this);
	return;
    }
    
   if (_devZero)
    {
	_bus->ReleaseDeviceZero();
	_devZero = false;
    }
    RemoveDevice();

    if (_initThread)
    {
        thread_call_cancel(_initThread);
        thread_call_free(_initThread);
        _initThread = 0;
    }
        
    if (_portStatusChangedHandlerThread)
    {
        thread_call_cancel(_portStatusChangedHandlerThread);
        thread_call_free(_portStatusChangedHandlerThread);
        _portStatusChangedHandlerThread = 0;
    }
    
}



void 
AppleUSBHubPort::PortInitEntry(OSObject *target)
{
    AppleUSBHubPort	*me = OSDynamicCast(AppleUSBHubPort, target);
    
    if (!me)
        return;
    me->PortInit();
    me->release();
}


void 
AppleUSBHubPort::PortInit()
{
    IOUSBHubPortStatus	status;
    IOReturn		err;
    
    USBLog(5, "***** AppleUSBHubPort[%p]::PortInit - port %d on hub %p beginning INIT (getting _runLock)", this, _portNum, _hub);
    _initThreadActive = true;
    IOLockLock(_runLock);
    
    // turn on Power to the port
    USBLog(5, "***** AppleUSBHubPort[%p]::PortInit - port %d on hub %p enabling port power", this, _portNum, _hub);
    if ((err = _hub->SetPortFeature(kUSBHubPortPowerFeature, _portNum)))
    {
        USBLog(3, "***** AppleUSBHubPort[%p]::PortInit - port %d on hub %p could not (err = %x) enable port power", this, _portNum, _hub, err);
       FatalError(err, "setting port power");
        goto errorExit;
    }

    // non captive devices will come in through the status change handler
    if (!_captive)
    {
        USBLog(5, "***** AppleUSBHubPort[%p]::PortInit - port %d on hub %p non-captive device - leaving PortInit", this, _portNum, _hub);
        goto errorExit;
    }
        
    // wait for the power on good time
    USBLog(5, "***** AppleUSBHubPort[%p]::PortInit - port %d on hub %p waiting %d ms for power on", this, _portNum, _hub, _hubDesc->powerOnToGood * 2);
    IOSleep(_hubDesc->powerOnToGood * 2);

    USBLog(5, "***** AppleUSBHubPort[%p]::PortInit - port %d on hub %p about to get port status #1", this, _portNum, _hub);
    if ((err = _hub->GetPortStatus(&status, _portNum)))
    {
        USBLog(3, "***** AppleUSBHubPort[%p]::PortInit - port %d on hub %p could not get (err = %x) port status #1", this, _portNum, _hub, err);
        FatalError(err, "getting port status (2)");
        goto errorExit;
    }

    USBLog(5, "***** AppleUSBHubPort[%p]::PortInit - port %d on hub %p - status(%8x), change(%8x) bits detected", this, _portNum, _hub, status.statusFlags, status.changeFlags);

    // we now have port status 
    if (status.changeFlags & kHubPortConnection)
    {
        USBLog(5, "***** AppleUSBHubPort[%p]::PortInit - port %d on hub %p - clearing connection change feature", this, _portNum, _hub);
        if ((err = _hub->ClearPortFeature(kUSBHubPortConnectionChangeFeature, _portNum)))
        {
            USBLog(3, "***** AppleUSBHubPort[%p]::PortInit - port %d on hub %p could not (err = %x) clear connection change", this, _portNum, _hub, err);
            FatalError(err, "clearing port connection change");
            goto errorExit;
        }

        // We should now be in the disconnected state 
        // Do a port request on current port 
        USBLog(5, "***** AppleUSBHubPort[%p]::PortInit - port %d on hub %p about to get port status #2", this, _portNum, _hub);
        if ((err = _hub->GetPortStatus(&status, _portNum)))
        {
            USBLog(3, "***** AppleUSBHubPort[%p]::PortInit - port %d on hub %p could not (err = %x) get port status #2", this, _portNum, _hub, err);
            FatalError(err, "getting port status (3)");
            goto errorExit;
        }
    }

    if (status.statusFlags & kHubPortConnection)
    {
        // We have a connection on this port
        USBLog(5, "***** AppleUSBHubPort[%p]::PortInit - port %d on hub %p device detected calling AddDevice", this, _portNum, _hub);
        if ((err = AddDevice()))
            FatalError(err, "adding device");
    }

errorExit:
	
    USBLog(5, "***** AppleUSBHubPort[%p]::PortInit - port %d on hub %p - done - releasing _runLock", this, _portNum, _hub);
    IOLockUnlock(_runLock);
    _initThreadActive = false;
    if (_inCommandSleep)
    {
	IOCommandGate *gate = NULL;
	if (_bus)
	    gate = _bus->GetCommandGate();
	if (gate)
	{
	    USBLog(2,"AppleUSBHubPort[%p]::PortInit -  calling commandWakeup", this);
	    gate->commandWakeup(&_initThreadActive, true);
	}
    }
}



IOReturn 
AppleUSBHubPort::AddDevice(void)
{
    IOReturn		err = kIOReturnSuccess;
    bool		checkingForDeadHub = false;
    IOUSBHubPortStatus	status;

    USBLog(5, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub %p - start", this, _portNum, _hub);
    do
    {
         // Indicate that we are dealing with device zero, still
        if ( !_devZero )
        {
            USBLog(5, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub %p - bus %p - acquiring dev zero lock", this, _portNum, _hub, _bus);
            _devZero = AcquireDeviceZero();
            if (!_devZero)
            {
           	USBLog(2, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub %p - bus %p - unable to get devZero lock", this, _portNum, _hub, _bus);
		FatalError(/*FIXME*/0, "acquiring device zero");
                break;
            }
        }
        else
        {
            USBLog(5, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub %p - bus %p - already owned devZero lock", this, _portNum, _hub, _bus);
        }

        USBLog(5, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub %p - resetting port", this, _portNum, _hub);
        SetPortVector(&AppleUSBHubPort::AddDeviceResetChangeHandler, kHubPortBeingReset);
        err = _hub->SetPortFeature(kUSBHubPortResetFeature, _portNum);
        if (err)
        {
            USBLog(3, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub %p - unable (err = %x) to reset port", this, _portNum, _hub, err);
	    
	    // Used to call "FatalError" here, but we don't want the message being displayed
	    if (_portDevice != 0)
	    {
		USBLog(2,"AppleUSBHubPort: Removing %s from port %d", _portDevice->getName(), _portNum);
		RemoveDevice();	
	    }

	    if (err == kIOUSBTransactionTimeout)
	    {
		_hub->CallCheckForDeadHub();			// pull the plug if it isn't already
		checkingForDeadHub = true;
	    }
            break;
        }
        USBLog(5, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub %p - sleeping 100 ms", this, _portNum, _hub);
        IOSleep(100);

    } while(false);

    if (err && _devZero)
    {
        USBLog(3, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub %p- bus %p - got error (%x) - releasing devZero lock", this, _portNum, _hub, _bus, err);
        // Need to disable the port before releasing the lock
        //
        
	if (!checkingForDeadHub)
	{
	    if ( (err = _hub->ClearPortFeature(kUSBHubPortEnableFeature, _portNum)) )
	    {
		// If we get an error at this point, it probably means that the hub went away.  Note the error but continue to 
		// release the devZero lock.
		//
		USBLog(5, "***** AppleUSBHubPort[%p]::AddDevice  ClearPortFeature for Port %d returned 0x%x", this, _portNum, err);
		FatalError(err, "clearing port feature");
		if (err == kIOUSBTransactionTimeout)
		{
		    _hub->CallCheckForDeadHub();			// pull the plug if it isn't already
		    checkingForDeadHub = true;
		}
	    }
	}
        
        _bus->ReleaseDeviceZero();
        _devZero = false;
        
        // put it back to the default if there was an error
        SetPortVector(&AppleUSBHubPort::DefaultResetChangeHandler, kHubPortBeingReset);
    }

    USBLog(5, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub %p - (err = %x) done - returning .", this, _portNum, _hub, err);
    return(err);
}



void 
AppleUSBHubPort::RemoveDevice(void)
{
    bool			ok;
    const IORegistryPlane 	* usbPlane;
    IOUSBDevice			*cachedPortDevice;


    if (_portDevice)
    {
        // Cache our port device so that we can set it to NULL so we don't reenter
        // this code (due to above check)
        //
        cachedPortDevice = _portDevice;
        _portDevice = NULL;

        USBLog(3, "AppleUSBHubPort[%p]::RemoveDevice start (%s)", this, cachedPortDevice->getName());
        usbPlane = cachedPortDevice->getPlane(kIOUSBPlane);

        if ( usbPlane )
            cachedPortDevice->detachAll(usbPlane);

        cachedPortDevice->terminate(kIOServiceRequired);
        cachedPortDevice->release();
    }

    InitPortVectors();
}


IOReturn 
AppleUSBHubPort::SuspendPort( bool suspend)
{
    IOReturn 		err = kIOReturnSuccess;
    IOUSBHubPortStatus	status;
      
    USBLog(5, "AppleUSBHubPort[%p]::SuspendPort(%d) for port %d", this, suspend, _portNum);

    do {
        
        // If resuming, need to check that the port was suspended to begin with
        //
        err = _hub->GetPortStatus(&status, _portNum);
        if (kIOReturnSuccess != err)
        {
            USBLog(3,"AppleUSBHubPort[%p]::SuspendPort Could not get Port Status: 0x%x", this, err);
            break;
        }

        if (!suspend && !(status.statusFlags & kHubPortSuspend) )
        {
            // We were trying to resume but the port was not supended.  Just ignore the
            // request.
            break;
        }
        
        // OK, set up the handler for the set/clear suspend feature
        //
        SetPortVector(&AppleUSBHubPort::HandleSuspendPortHandler, kHubPortSuspend);

        if (suspend)
        {
            err = _hub->SetPortFeature(kUSBHubPortSuspendFeature, _portNum);
            if ( err != kIOReturnSuccess )
            {
                // Root Hub failed for lucent bug
                //
                // Callback device with the resume message
                USBLog(3, "AppleUSBHubPort[%p]::SuspendPort Could not SetPortFeature (%d) (kUSBHubPortSuspendFeature): (0x%x)", this, _portNum, err);
                if ( _portDevice )
                    _portDevice->message(kIOUSBMessagePortHasBeenResumed, NULL, 0);

            }
            // Send a message to our device indicating that we completed the SetPortFeature
            if ( _portDevice )
                _portDevice->message(kIOUSBMessagePortHasBeenSuspended, NULL, &err);
        }
        else
        {
            err = _hub->ClearPortFeature(kUSBHubPortSuspendFeature, _portNum);
            if ( err != kIOReturnSuccess )
            {
                USBLog(3, "AppleUSBHubPort[%p]::SuspendPort Could not ClearPortFeature (%d) (kUSBHubPortSuspendFeature): (0x%x)", this, _portNum, err);
            }
        }


    } while (false);

    if ( err != kIOReturnSuccess )
    {
        // Set the handler back to default
        //
        SetPortVector(&AppleUSBHubPort::DefaultSuspendChangeHandler, kHubPortSuspend);
    }
    
    return err;
}

IOReturn
AppleUSBHubPort::ReEnumeratePort(UInt32 options)
{
    USBLog(5,"AppleUSBHubPort[%p]::ReEnumeratePort -- reenumerating port %d, options 0x%x",this, _portNum, options);

    // Test to see if bit31 is set, and if so, set up a flag to indicate that we need to wait 100ms after a reset and before
    // talking to the device (Bluetooth workaround)
    if ( (options & (1<<31)) )
    {
        USBLog(5,"AppleUSBHubPort[%p]::ReEnumeratePort -- reenumerating port %d, options 0x%x",this, _portNum, options);
        _extraResetDelay = true;
    }
    
    // First, since we are going to reenumerate, we need to remove the device
    // and then add it again
    //
    RemoveDevice();

    return AddDevice();
}

IOReturn 
AppleUSBHubPort::ClearTT(bool multiTTs, UInt32 options)
{
IOReturn 		err = kIOReturnSuccess;
UInt8 	 deviceAddress;  //<<0
UInt8	 endpointNum;    //<<8
UInt8 	 endpointType;	 //<<16 // As split transaction. 00 Control, 10 Bulk
UInt8 	 IN;		 //<<24 // Direction, 1 = IN, 0 = OUT};
UInt32 opts= options;

UInt16 wValue, wIndex;
IOUSBDevRequest request;

    deviceAddress = options & 0xff;
    options >>= 8;
    
    endpointNum = options & 0xff;
    options >>= 8;
    
    endpointType = options & 0xff;
    options >>= 8;
    
    IN = options & 0xff;
    options >>= 8;

/* 
3..0 Endpoint Number
10..4 Device Address
12..11 Endpoint Type
14..13 Reserved, must be zero
15 Direction, 1 = IN, 0 = OUT
*/
    wValue = 0;
    wValue = endpointNum & 0xf;
    wValue |= (deviceAddress & 0x7f) << 4;
    wValue |= (endpointType & 0x3) << 11;
    wValue |= (IN & 0x1) << 15;
    
    if(multiTTs)
    {
	wIndex = _portNum;
    }
    else
    {
	wIndex = 1;
    }
    
    request.bmRequestType = 0x23;
    request.bRequest = 8;
    request.wValue = wValue;
    request.wIndex = wIndex;
    request.wLength = 0;
    request.pData = NULL;
    request.wLenDone = 0;

    err = _hub->DoDeviceRequest(&request);
    USBLog(5,"AppleUSBHubPort[%p]::ClearTT -- port %d, options:%lX, wValue:%X, wIndex:%X, IOReturn: %LX",this, _portNum, opts, wValue, wIndex, err);
    
    if( (err == kIOReturnSuccess) && (endpointType == 0) )	// Control endpoint.
    {
	wValue ^= (1 << 15);	// Flip direction bit and do it again.
	request.wValue = wValue;
	err = _hub->DoDeviceRequest(&request);
	USBLog(5,"AppleUSBHubPort[%p]::ClearTT -- do it again for control transactions, wValue:%X, IOReturn: %LX",this, wValue, err);
    }
    
    return err;
}

IOReturn 
AppleUSBHubPort::ResetPort()
{
    IOReturn 		err = kIOReturnSuccess;
    IOUSBHubPortStatus	status;

    USBLog(5, "AppleUSBHubPort[%p]::ResetPort for port %d", this, _portNum);
    do {
        // First, we need to make sure that we can acquire the devZero lock.  If we don't then we have
        // no business trying to reset the port.
        //
        _devZero = AcquireDeviceZero();
        if (!_devZero)
        {
            USBLog(3, "AppleUSBHubPort[%p]::ResetPort for port %d could not get devZero lock", this, _portNum);
            err = kIOReturnCannotLock;
            break;
        }

        // Check to see if the port is suspended and if so, clear that feature
        //
        err = _hub->GetPortStatus(&status, _portNum);
        if (kIOReturnSuccess != err)
        {
            USBLog(3, "AppleUSBHubPort[%p]::ResetPort for port %d could not get port status (0x%x)", this, _portNum, err);
            break;
        }

        if ( status.statusFlags & kHubPortSuspend)
        {
            err = _hub->ClearPortFeature(kUSBHubPortSuspendFeature, _portNum);
            if (kIOReturnSuccess != err)
            {
                USBLog(3, "AppleUSBHubPort[%p]::ResetPort Could not ClearPortFeature (%d) (kHubPortSuspend): (0x%x)", this, _portNum, err);
                break;
            }

            // Now, check to see if the suspend did get cleared:
            //
            err = _hub->GetPortStatus(&status, _portNum);
            if (kIOReturnSuccess != err)
            {
                USBLog(3, "AppleUSBHubPort[%p]::ResetPort for port %d could not get port status (0x%x)", this, _portNum, err);
                break;
            }

            if ( status.statusFlags & kHubPortSuspend)
            {
                USBError(1, "AppleUSBHubPort[%p]::ResetPort for port %d Port is still suspended after clearing it", this, _portNum);
           }
        }
    
        // OK, set our handler for a reset to the portReset handler and call the
        // hub to actually reset the port
        //
        SetPortVector(&AppleUSBHubPort::HandleResetPortHandler, kHubPortBeingReset);
        
        err = _hub->SetPortFeature(kUSBHubPortResetFeature, _portNum);
        if (err != kIOReturnSuccess)
        {
            USBLog(3, "AppleUSBHubPort[%p]::ResetPort Could not ClearPortFeature (%d) (kUSBHubPortResetFeature): (0x%x)", this, _portNum, err);

            // Return our vector to the default handler
            //
            SetPortVector(&AppleUSBHubPort::DefaultResetChangeHandler, kHubPortBeingReset);
            break;
        }

    } while (false);

    if (err == kIOReturnSuccess)
    {
        _bus->WaitForReleaseDeviceZero();
    }
    else if(_devZero)
    {
        _bus->ReleaseDeviceZero();
        _devZero = false;
    }

    return err;
}



void 
AppleUSBHubPort::FatalError(IOReturn err, char *str)
{
    USBError(1, "AppleUSBHubPort: Error 0x%x: %s", err, str);

    if (_portDevice != 0)
    {
        USBLog(2,"AppleUSBHubPort: Removing %s from port %d", _portDevice->getName(), _portNum);
        RemoveDevice();	
    }
}

static IOReturn DoCreateDevice(	IOUSBController  *bus,
                                IOUSBDevice 		*newDevice,
                                USBDeviceAddress	deviceAddress,
                                UInt8		 	maxPacketSize,
                                UInt8			speed,
                                UInt32			powerAvailable,
                                USBDeviceAddress		hub,
                                int      port)
{
IOUSBControllerV2 *v2Bus;

    v2Bus = OSDynamicCast(IOUSBControllerV2, bus);
    
    if(v2Bus != 0)
    {
        return(v2Bus->CreateDevice(newDevice, deviceAddress, maxPacketSize, speed, powerAvailable, hub, port));
    }
    else
    {
        return(bus->CreateDevice(newDevice, deviceAddress, maxPacketSize, speed, powerAvailable));
    }
}
	
static IOReturn DoConfigureDeviceZero(IOUSBController  *bus, UInt8 maxPacketSize, UInt8 speed, USBDeviceAddress hub, int port)
{
IOUSBControllerV2 *v2Bus;

    v2Bus = OSDynamicCast(IOUSBControllerV2, bus);
    
    if(v2Bus != 0)
    {
        return(v2Bus->ConfigureDeviceZero(maxPacketSize, speed, hub, port));
    }
    else
    {
        return(bus->ConfigureDeviceZero(maxPacketSize, speed));
    }
}



/**********************************************************************
 **
 ** CHANGE HANDLER FUNCTIONS
 **
 **********************************************************************/
IOReturn 
AppleUSBHubPort::AddDeviceResetChangeHandler(UInt16 changeFlags, UInt16 statusFlags)
{
    IOReturn		err = kIOReturnSuccess;
    IOReturn		err2 = kIOReturnSuccess;
    IOUSBDevice	*	usbDevice; 
    USBDeviceAddress 	address;
    UInt32		delay = 10;
    const IORegistryPlane 	* usbPlane;
    
    USBLog(5, "***** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d on hub %p - start", this, _portNum, _hub);

    if ( _extraResetDelay )
    {
        USBLog(5, "***** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - delaying 100ms workaround", this);
        IOSleep(100);
    }
    
    do
    {
        if (_state != hpsDeadDeviceZero)
        {
            
            // Before doing anything, check to see if the device is really there
            //
            if ( !(statusFlags & kHubPortConnection) )
            {
                // We don't have a connection on this port anymore.
                //
                USBLog(5, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler - port %d - device has gone away", this, _portNum);
                _state = hpsDeadDeviceZero;
                err = kIOReturnNoDevice;
                break;
            }

            // in the Mac OS 9 state machine (called resetChangeHandler) we skip states 1-3
            // if we are in DeadDeviceZero state (so that we end up setting the address )
            
            // MacOS 9 STATE 1
            //
            if (_portStatus.statusFlags & kHubPortBeingReset)
            {
                USBLog(5, "**1** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d on hub %p - leaving (kHubPortBeingReset)", this, _portNum, _hub);
                // we should never be here, just wait for another status change int
                break;
            }
            
            // If the device attached to this port misbehaved last time we tried to enumerate it, let's
            // relax the timing a little bit and give it more time.
            //
            if (_getDeviceDescriptorFailed)
            {
                delay = 300;
                USBLog(3, "**1** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d on hub %p - new delay %d", this, _portNum, _hub, delay);
            }
                
            // Now wait 10 ms (or 300ms -- see above) after reset
            //
            USBLog(5, "**1** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d on hub %p - delaying %d ms", this, _portNum, _hub, delay);
            IOSleep(delay);
            
            // Mac OS 9 state 2
            // macally iKey doesn't tell us until now what the device speed is.
            if (_portStatus.statusFlags & kHubPortLowSpeed)
            {
                _speed = kUSBDeviceSpeedLow;
                USBLog(5, "**2** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d on hub %p - found low speed device", this, _portNum, _hub);
            }
            else
            {
                if (_portStatus.statusFlags & kHubPortHighSpeed)
                {
                    _speed = kUSBDeviceSpeedHigh;
                    USBLog(5, "**2** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d on hub %p - found high speed device", this, _portNum, _hub);
                }
                else
                {
                    _speed = kUSBDeviceSpeedFull;
                    USBLog(5, "**2** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d on hub %p - found full speed device", this, _portNum, _hub);
                }
            }
                
    
            // 	Configure algorithm:   еее This is different than MacOS 9 ееее
            //  
            // 		- 	start with maxpacketsize of 8.
            // 			This is the smallest legal maxpacket, and the only legal size for
            // 			low-speed devices. The correct maxpacket size is in byte 8 of the
            // 			device descriptor, so even if the device sends back a bigger packet
            // 			(an overrun error) we should still get the correct value.
            // 		- 	get device descriptor.
            // 		- 	if we recieved the whole descriptor AND maxpacketsize is 64,
            //   		success, so continue on.
            // 		- 	if descriptor returns with a different maxpacketsize, then
            //   		reconfigure with the new one and try again.  Otherwise, 
            //   		reconfigure with 8 and try again.
            //
            USBLog(5, "**2** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d on hub %p - configuring dev zero", this, _portNum, _hub);
            err = DoConfigureDeviceZero(_bus, 8, _speed,  _hub->_device->GetAddress(), _portNum);
            
            // еее Ask Barry - it looks like we ignore this error in the 9 world (or should we fall through to set address?)

            // MacOS 9 STATE 3
            
            // If our GetDescriptor fails we will clear this flag
            //
            _getDeviceDescriptorFailed = true;
  
            // Now do a device request to find out what it is.  Some fast devices send back packets > 8 bytes to address 0.
            // We will attempt 5 times with a 30ms delay between each (that's  what we do on MacOS 9 )
            //
            bzero(&_desc, sizeof(_desc));
            USBLog(5, "**3** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d on hub %p - getting dev zero desc", this, _portNum, _hub);
            err = GetDevZeroDescriptorWithRetries();
            
            if ( err != kIOReturnSuccess )
            {
                USBLog(5, "**3** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d on hub %p - failed to get dev zero desc", this, _portNum, _hub);
                _getDeviceDescriptorFailed = true;
                _state = hpsDeadDeviceZero;
            }

            USBLog(5,"**3** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, using %d for maxPacketSize", this, _portNum, _desc.bMaxPacketSize0);            
        }

        // MacOS 9 STATE 4
        
        if(_setAddressFailed > 0)
        {
            // Last time we were here, the following set address failed, so give it some more time
            //
            // еее Why don't we add the -1 to setAddressFailed, as we do on 9?
            //
            delay = ((_setAddressFailed) * 30) + (_setAddressFailed * 3);
            USBLog(3, "**4** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, previous SetAddress failed, sleeping for %d milliseconds", this, _portNum, delay);
            IOSleep(delay);
        }


        // MacOS 9 STATE 5
        
        // (Note: The power stuff is passed in so we don't need to check it here, as we do in 9)
        //
        if ( err == kIOReturnSuccess )
            _getDeviceDescriptorFailed = false;
            
         _state = hpsSetAddress;
        
        // Create and address the device
        //
        usbDevice = _bus->MakeDevice( &address );
   		
    	if (usbDevice == NULL || address == 0)
    	{
            // Setting the Address failed
            // 
            USBLog(3,"**5** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, unable to set device %p to address %d - disabling port", this, _portNum, usbDevice, address );
 
            // OK, disable the port and try to add the device again
            //
            if ( (err = _hub->ClearPortFeature(kUSBHubPortEnableFeature, _portNum)) )
            {
                USBLog(3, "**5** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, unable (err = %x) to disable port", this, _portNum, err);
                FatalError(err, "clearing port feature");
                _bus->ReleaseDeviceZero();
                _devZero = false;
                _state = hpsSetAddressFailed;
                _portDevice = 0;
               return err;
            }
            
            _bus->ReleaseDeviceZero();
            _devZero = false;
            _state = hpsSetAddressFailed;
            
            return DetachDevice();
            
    	}
    	else
    	{	
            // Section 9.2.6.3 of the spec gives the device 2ms to recover from the SetAddress
            IOSleep( 2 );

            // Release devZero lock
            USBLog(5, "**5** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, Releasing DeviceZero after successful SetAddress to %d", this, _portNum, address);
            _bus->ReleaseDeviceZero();
            _devZero = false;
            _state = hpsNormal;
            
        }
        
        // MacOS 9 STATE 6
        
        if( _state == hpsDeadDeviceZero )
        {
            _setAddressFailed++;
            USBLog(3, "**6** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, setaddressfailed = %d, disabling port", this, _portNum, _setAddressFailed);
            
            // Note: we are intentionally not changing the value of err below
            //
            _hub->ClearPortFeature(kUSBHubPortEnableFeature, _portNum);
            
            // ее Not in 9 ее
            if (_devZero)
            {
                USBLog(3, "**6** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, releasing devZero lock", this, _portNum);
                _bus->ReleaseDeviceZero();
                _devZero = false;
            }
            
            // MacOS 9 STATE 7
            
            USBLog(3, "**7** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, setting state to hpsSetAddressFailed", this, _portNum);
            _state = hpsSetAddressFailed;
            
        }
        else
        {
            // MacOS 9 STATE 8
            
            // еее Why don't we add the -1 to setAddressFailed, as we do on 9?  Don't add it!  It will break
            // the fix for #2652091 (SetAddress failing).
            //
            delay = (_setAddressFailed * 30) + (_setAddressFailed * 3);
            if ( delay )
            {
                USBLog(3, "**8** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, sleeping for %d milliseconds", this, _portNum, delay);
                IOSleep(delay);
            }
        }

        // MacOS 9 STATE 9
        
        if( (err != kIOReturnSuccess) && (_state != hpsNormal) )
        {
            // An error setting the address, so go back and try resetting again
            //
            USBLog(3, "**9** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, err = %x, disabling port", this, _portNum, err);
            _retryPortStatus = true;
            
            SetPortVector(&AppleUSBHubPort::DefaultResetChangeHandler, kHubPortBeingReset);

            // ее╩Not in 9
            _hub->ClearPortFeature(kUSBHubPortEnableFeature, _portNum);
            
            // ее Not in 9 ее
            if (_devZero)
            {
                USBLog(3, "**9** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, releasing devZero lock", this, _portNum);
                _bus->ReleaseDeviceZero();
                _devZero = false;
            }

            USBLog(3, "**9** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, delaying 10 ms and calling AddDevice", this, _portNum);
            IOSleep(10); // ее╩Nine waits for only 1ms
            err = AddDevice();		
            return err;
        }

        _state = hpsNormal;
        
       err = DoCreateDevice(_bus, usbDevice, address, _desc.bMaxPacketSize0, _speed, _portPowerAvailable,
                                    _hub->_device->GetAddress(), _portNum);
        if ( !err )
        {
	    _portDevice = usbDevice;
        }
        else
        {
            USBLog(3, "**9** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler -  port %d, did NOT get _portDevice", this, _portNum);
            err = err2;
	    usbDevice->release();
	    usbDevice = NULL;
        }
        
	if (!_portDevice)
	{
	    // OOPS- The device went away from under us. Probably due to an unplug. Well, there is nothing more
	    // for us to do, so just return
            USBLog(3, "**9** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, _portDevice disappeared, cleaning up", this, _portNum);
            _retryPortStatus = true;

            SetPortVector(&AppleUSBHubPort::DefaultResetChangeHandler, kHubPortBeingReset);
            _hub->ClearPortFeature(kUSBHubPortEnableFeature, _portNum);
            // ее Not in 9 ее
            if (_devZero)
            {
                USBLog(3, "**9** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, releasing devZero lock", this, _portNum);
                _bus->ReleaseDeviceZero();
                _devZero = false;
            }

            USBLog(3, "**9** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, delaying 10 ms and calling AddDevice", this, _portNum);
            IOSleep(10); // ее╩Nine waits for only 1ms
            err = AddDevice();		
            return err;
	    // return kIOReturnSuccess;
	}
	
        // In MacOS 9, we attempt to get the full DeviceDescriptor at this point, if we had missed it earlier.  On X, we do NOT do this
        // because we would have failed the IOUSBDevice::start if we didn't get it.
        //

        USBLog(5, "**10** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler -  port %d, at addr: %d, Succesful", this, _portNum, address);
        // MacOS STATE 10
        //
        _attachRetry = 0;
        
        // Finally use the data gathered
        // link the new device as a child of my hub
        // USBLog(3, this, "attaching device (%s) to Port %d of hub (%s)\n", _portDevice->getName(), _portNum, _hub->_device->getName());
        
        usbPlane = _portDevice->getPlane(kIOUSBPlane);

        if ( usbPlane )
            _portDevice->attachToParent( _hub->_device, usbPlane);
        
        // Add properties to the device
        //
        _portDevice->setProperty("PortNum",_portNum,32);
        _portDevice->SetProperties();
        if ( IsCaptive() )
            _portDevice->setProperty("non-removable","yes");
       
        // register the NUB
        _portDevice->registerService();

    } while(false);

    if (err)
    {
        if (_devZero)
        {
            USBLog(3, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, err = %x, releasing devZero lock", this, _portNum, err);
            _hub->ClearPortFeature(kUSBHubPortEnableFeature, _portNum);
            _bus->ReleaseDeviceZero();
            _devZero = false;
        }
    }
    SetPortVector(&AppleUSBHubPort::DefaultResetChangeHandler, kHubPortBeingReset);
    USBLog(5, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, err = %x, ALL DONE", this, _portNum, err);
    return err;
}

IOReturn 
AppleUSBHubPort::HandleResetPortHandler(UInt16 changeFlags, UInt16 statusFlags)
{
    IOReturn		err = kIOReturnSuccess;
    UInt32		delay = 10;
    
    USBLog(5, "***** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d on hub %p - start", this, _portNum, _hub);
    SetPortVector(&AppleUSBHubPort::DefaultResetChangeHandler, kHubPortBeingReset);
    if ( _extraResetDelay )
    {
        USBLog(5, "***** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - delaying 100ms workaround", this);
        IOSleep(100);
    }

    do
    {
        if (_state != hpsDeadDeviceZero)
        {
            // in the Mac OS 9 state machine (called resetChangeHandler) we skip states 1-3
            // if we are in DeadDeviceZero state (so that we end up setting the address )
            
            // MacOS 9 STATE 1
            //
            if (_portStatus.statusFlags & kHubPortBeingReset)
            {
                USBLog(5, "**1** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d on hub %p - leaving (kHubPortBeingReset)", this, _portNum, _hub);
                // we should never be here, just wait for another status change int
                // OK, set our handler for a reset to the portReset handler
                //
                SetPortVector(&AppleUSBHubPort::HandleResetPortHandler, kHubPortBeingReset);
                return kIOReturnSuccess;
            }
            
            // If the device attached to this port misbehaved last time we tried to enumerate it, let's
            // relax the timing a little bit and give it more time.
            //
            if (_getDeviceDescriptorFailed)
            {
                delay = 300;
                USBLog(3, "**1** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d on hub %p - new delay %d", this, _portNum, _hub, delay);
            }
                
            // Now wait 10 ms (or 300ms -- see above) after reset
            //
            USBLog(5, "**1** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d on hub %p - delaying %d ms", this, _portNum, _hub, delay);
            IOSleep(delay);
            
            // Mac OS 9 state 2
            // macally iKey doesn't tell us until now what the device speed is.
            if (_portStatus.statusFlags & kHubPortLowSpeed)
            {
                _speed = kUSBDeviceSpeedLow;
                USBLog(5, "**2** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d on hub %p - found low speed device", this, _portNum, _hub);
            }
            else
            {
                if (_portStatus.statusFlags & kHubPortHighSpeed)
                {
                    _speed = kUSBDeviceSpeedHigh;
                    USBLog(5, "**2** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d on hub %p - found high speed device", this, _portNum, _hub);
                }
                else
                {
                    _speed = kUSBDeviceSpeedFull;
                    USBLog(5, "**2** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d on hub %p - found full speed device", this, _portNum, _hub);
                }
            }
                
   
            USBLog(5, "**2** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d on hub %p - configuring dev zero", this, _portNum, _hub);
            err = DoConfigureDeviceZero(_bus, 8, _speed, _hub->_device->GetAddress(), _portNum);
            
            
            // If our GetDescriptor fails we will clear this flag
            //
            _getDeviceDescriptorFailed = true;
  
            // Now do a device request to find out what it is.  Some fast devices send back packets > 8 bytes to address 0.
            // We will attempt 5 times with a 30ms delay between each (that's  what we do on MacOS 9 )
            //
            bzero(&_desc, sizeof(_desc));
            USBLog(5, "**3** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d on hub %p - getting dev zero desc", this, _portNum, _hub);
            err = GetDevZeroDescriptorWithRetries();
            
            if ( err != kIOReturnSuccess )
            {
                USBLog(5, "**3** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d on hub %p - failed to get dev zero desc", this, _portNum, _hub);
                _getDeviceDescriptorFailed = true;
                _state = hpsDeadDeviceZero;
            }

            USBLog(5,"**3** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d, using %d for maxPacketSize", this, _portNum, _desc.bMaxPacketSize0);            
        }

        // MacOS 9 STATE 4
        
        if(_setAddressFailed > 0)
        {
            // Last time we were here, the following set address failed, so give it some more time
            //
            delay = ((_setAddressFailed) * 30) + (_setAddressFailed * 3);
            USBLog(3, "**4** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d, previous SetAddress failed, sleeping for %d milliseconds", this, _portNum, delay);
            IOSleep(delay);
        }


        // MacOS 9 STATE 5
        
        // The power stuff is passed in so we don't need to check it here
        //
        if (!err)
            _getDeviceDescriptorFailed = false;
            
         _state = hpsSetAddress;
        
        // ReAddress the device (if it still exists)
        //
        if (_portDevice)
        {
            err = _bus->SetDeviceZeroAddress(_portDevice->GetAddress());
            if (err)
            {
                // Setting the Address failed
                // 
                USBLog(3,"**5** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d, unable to set device %p to address %d - disabling port", this, _portNum, _portDevice, _portDevice->GetAddress() );
    
                // OK, disable the port and try to add the device again
                //
                if ( (err = _hub->ClearPortFeature(kUSBHubPortEnableFeature, _portNum)) )
                {
                    USBLog(3, "**5** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d, unable (err = %x) to disable port", this, _portNum, err);
    
                    _bus->ReleaseDeviceZero();
                    _devZero = false;
                    _state = hpsSetAddressFailed;
    
                    // Now, we need to notify our client that the reset did not complete
                    //
                    if ( _portDevice)
                    {
                        USBLog(5, "AppleUSBHubPort[%p]::HandleResetPortHandler - port %d, Sending kIOUSBMessagePortHasBeenReset message (0x%x)", this, _portNum, err);
                        _portDevice->message(kIOUSBMessagePortHasBeenReset, NULL, &err);
                    }
    
                    // FatalError will remove the device if it exists
                    //
                    FatalError(err, "clearing port feature");
    
                    return err;
                }
                
                _bus->ReleaseDeviceZero();
                _devZero = false;
                _state = hpsSetAddressFailed;
    
                // Now, we need to notify our client that the reset did not complete
                //
                if ( _portDevice)
                {
                    err = kIOReturnNoDevice;
                    USBLog(5, "AppleUSBHubPort[%p]::HandleResetPortHandler - port %d, Sending kIOUSBMessagePortHasBeenReset message (0x%x)", this, _portNum, err);
                    _portDevice->message(kIOUSBMessagePortHasBeenReset, NULL, &err);
                }
                
                return DetachDevice();
                
            }
            else
            {	
                // Section 9.2.6.3 of the spec gives the device 2ms to recover from the SetAddress
                IOSleep( 2 );
    
                // Release devZero lock
                USBLog(5, "**5** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d, Releasing DeviceZero after successful SetAddress", this, _portNum);
                _bus->ReleaseDeviceZero();
                _devZero = false;
                _state = hpsNormal;
            }
        }
        
        // MacOS 9 STATE 6
        
        if( _state == hpsDeadDeviceZero )
        {
            _setAddressFailed++;
            USBLog(3, "**6** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d, setaddressfailed = %d, disabling port", this, _portNum, _setAddressFailed);
            
            // Note: we are intentionally not changing the value of err below
            //
            _hub->ClearPortFeature(kUSBHubPortEnableFeature, _portNum);
            
            // ее Not in 9 ее
            if (_devZero)
            {
                USBLog(3, "**6** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d, releasing devZero lock", this, _portNum);
                _bus->ReleaseDeviceZero();
                _devZero = false;
            }
            
            // MacOS 9 STATE 7
            
            USBLog(3, "**7** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d, setting state to hpsSetAddressFailed", this, _portNum);
            _state = hpsSetAddressFailed;
            
        }
        else
        {
            // MacOS 9 STATE 8
            
            // еее Why don't we add the -1 to setAddressFailed, as we do on 9?  Don't add it!  It will break
            // the fix for #2652091 (SetAddress failing).
            //
            delay = (_setAddressFailed * 30) + (_setAddressFailed * 3);
            if ( delay )
            {
                USBLog(3, "**8** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d, sleeping for %d milliseconds", this, _portNum, delay);
                IOSleep(delay);
            }
        }

        // MacOS 9 STATE 9

        _state = hpsNormal;
        
	if (!_portDevice)
	{
	    // OOPS- The device went away from under us. Probably due to an unplug. Well, there is nothing more
	    // for us to do, so just return
            //
            if (_devZero)
            {
                USBLog(3, "**9** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d, releasing devZero lock", this, _portNum);
                _bus->ReleaseDeviceZero();
                _devZero = false;
            }
            USBLog(3, "**9** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d, _portDevice disappeared, returning", this, _portNum);

            // Fall through to return the message to the hub
            //
	}
	
        // In MacOS 9, we attempt to get the full DeviceDescriptor at this point, if we had missed it earlier.  On X, we do this
        // when we create the device.  Need to figure out how best to do it.
        //
        
        // MacOS STATE 10
        //
        _attachRetry = 0;

    } while(false);

    if (err)
    {
        if (_devZero)
        {
            USBLog(3, "AppleUSBHubPort[%p]::HandleResetPortHandler - port %d, err = %x, releasing devZero lock", this, _portNum, err);
            _hub->ClearPortFeature(kUSBHubPortEnableFeature, _portNum);
            _bus->ReleaseDeviceZero();
            _devZero = false;
        }
    }

    // Send a message to the Hub device that the port has been reset.  The hub device will then message
    // any clients with the kIOUSBMessagePortHasBeenReset message
    //
    if (_portDevice)
    {
        _portDevice->message(kIOUSBMessagePortHasBeenReset, NULL, &err);
        USBLog(5, "AppleUSBHubPort[%p]::HandleResetPortHandler - port %d, Sending kIOUSBMessagePortHasBeenReset message (0x%x)", this, _portNum, err);
    }
    
    SetPortVector(&AppleUSBHubPort::DefaultResetChangeHandler, kHubPortBeingReset);
    USBLog(5, "AppleUSBHubPort[%p]::HandleResetPortHandler - port %d, err = %x, ALL DONE", this, _portNum, err);
    return err;
}

IOReturn
AppleUSBHubPort::HandleSuspendPortHandler(UInt16 changeFlags, UInt16 statusFlags)
{
    USBLog(3, "AppleUSBHubPort[%p]::HandleSuspendPortHandler for port %d, changeFlags: 0x%x", this, _portNum, changeFlags);
    SetPortVector(&AppleUSBHubPort::DefaultSuspendChangeHandler, kHubPortSuspend);

    // Send a message to the device that the port has been resumed
    //
    _portDevice->message(kIOUSBMessagePortHasBeenResumed, NULL, 0);
       
    return kIOReturnSuccess;
}

IOReturn 
AppleUSBHubPort::DefaultOverCrntChangeHandler(UInt16 changeFlags, UInt16 statusFlags)
{
    IOUSBHubDescriptor		hubDescriptor;
    bool			individualPortPower = FALSE;
    UInt16			characteristics;
    IOUSBHubPortStatus		portStatus;
    IOReturn			err;

    USBLog(5, "AppleUSBHubPort[%p]::DefaultOverCrntChangeHandler. Port %d", this,  _portNum );
    err = _hub->GetPortStatus(&portStatus, _portNum);

    if ( err == kIOReturnSuccess )
    {
        if ( (portStatus.statusFlags & kHubPortOverCurrent))
        {
            USBLog(1, "AppleUSBHubPort[%p]::DefaultOverCrntChangeHandler. OverCurrent condition in Port %d", this,  _portNum );
            hubDescriptor = _hub->GetCachedHubDescriptor();

            characteristics = USBToHostWord(hubDescriptor.characteristics);

            if ( (characteristics & 0x18) == 0x8 )
                individualPortPower = TRUE;

            DisplayOverCurrentNotice( individualPortPower );
        }
        else
        {
            // the OverCurrent status for this port has changed to zero.
            //
            USBLog(1, "AppleUSBHubPort[%p]::DefaultOverCrntChangeHandler. No OverCurrent condition. Ignoring. Port %d", this, _portNum );
        }
    }
    
    return err;
}


IOReturn 
AppleUSBHubPort::DefaultResetChangeHandler(UInt16 changeFlags, UInt16 statusFlags)
{
    USBLog(5, "AppleUSBHubPort[%p]::DefaultResetChangeHandler for port %d returning kIOReturnSuccess", this, _portNum);
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBHubPort::DefaultSuspendChangeHandler(UInt16 changeFlags, UInt16 statusFlags)
{
    USBLog(5, "AppleUSBHubPort[%p]::DefaultSuspendChangeHandler for port %d returning kIOReturnSuccess", this, _portNum);
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBHubPort::DefaultEnableChangeHandler(UInt16 changeFlags, UInt16 statusFlags)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBHubPortStatus	status;

    USBLog(5, "AppleUSBHubPort[%p]::DefaultEnableChangeHandler for port %d, changeFlags: 0x%x", this, _portNum, changeFlags);

    if ((err = _hub->GetPortStatus(&status, _portNum)))
    {
        FatalError(err, "getting port status (1)");
        return err;
    }

    if (!(status.statusFlags & kHubPortEnabled) &&
        !(changeFlags & kHubPortConnection))
    {
         USBLog( 3, "AppleUSBHubPort[%p]::DefaultEnableChangeHandler: port %d disabled. Device driver should reset itself port", this,  _portNum);
    }

    return err;
}



IOReturn 
AppleUSBHubPort::DefaultConnectionChangeHandler(UInt16 changeFlags, UInt16 statusFlags)
{
    IOReturn	err = kIOReturnSuccess;
    IOUSBHubPortStatus	status;

    USBLog(5, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler - handling port %d changes (%x).", this, _portNum, changeFlags);
    _connectionChangedState = 0;
    do
    {
        // Wait before asserting reset (USB 1.1, section 7.1.7.1)
        //
        if ( _getDeviceDescriptorFailed )
        {
            _connectionChangedState = 1;
            USBLog(3, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler port (%d) - previous enumeration failed - sleeping 300 ms", this, _portNum);
            IOSleep(300);
        }
        else
        {
            USBLog(5, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler port (%d) - waiting 100 ms before asserting reset", this, _portNum);
            _connectionChangedState = 2;
            IOSleep(100);
        }
        
        // If we get to here, there was a connection change
        // if we already have a device it must have been disconnected
        // at sometime. We should kill it before servicing a connect event
        
        // If we're still in hpsDeviceZero, it means that we are haven't addressed the device, so we need to release the devZero lock
        //
        if ( _devZero )
        {
            USBLog(5, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler - port %d - releasing devZero lock", this, _portNum);
            // _state = hpsNormal;
            _connectionChangedState = 3;
            _bus->ReleaseDeviceZero();
            _devZero = false;
        }
        
        if (_portDevice != 0 )
        {	
            _connectionChangedState = 4;
            USBLog(5, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler - port %d - found device (%p) removing", this, _portNum, _portDevice);
            RemoveDevice();	
            _connectionChangedState = 5;
        }
        else
        {
            _connectionChangedState = 6;
            USBLog(5, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler - port %d - no existing device found on port", this, _portNum);
        }

        // BT 23Jul98 Check port again after delay. Get bounced connections
        // Do a port status request on current port
        if ((err = _hub->GetPortStatus(&status, _portNum)))
        {
            _connectionChangedState = 7;
            _retryPortStatus = true;
            FatalError(err, "getting port status (5)");
            break;
        }

        _connectionChangedState = 8;
        USBLog(4, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler port %d status(%8x)/change(%8x) - no error from GetPortStatus", this, _portNum, status.statusFlags, status.changeFlags);
        if (status.changeFlags & kHubPortConnection)
        {
            _retryPortStatus = true;
            USBLog(5, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler port %d connection bounce", this, _portNum);
            break;
        }

        if (status.statusFlags & kHubPortConnection)
        {
            // We have a connection on this port. Attempt to add the device
            //
            USBLog(5, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler - port %d - device detected, calling AddDevice", this, _portNum);
            _state = hpsDeviceZero;
            _connectionChangedState = 9;
            err = AddDevice();
            _connectionChangedState = 10;
        }
        
    } while(false);


    USBLog(5, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler - port %d done, ending.", this, _portNum);
    return err;
}



void 
AppleUSBHubPort::PortStatusChangedHandlerEntry(OSObject *target)
{
    AppleUSBHubPort 	*me;
    
    if (!target)
    {
        USBLog(5, "AppleUSBHubPort::PortStatusChangedHandlerEntry - no target!");
        return;
    }
    
    me = OSDynamicCast(AppleUSBHubPort, target);
    
    if (!me)
    {
        USBLog(5, "AppleUSBHubPort::PortStatusChangedHandlerEntry - target is not really me!");
        return;
    }
        
    me->PortStatusChangedHandler();
    me->release();
}



void 
AppleUSBHubPort::PortStatusChangedHandler(void)
{
    int 	which;
    IOReturn	err = kIOReturnSuccess;
    bool	skipOverGetPortStatus = false;


    if (!IOLockTryLock(_runLock))
    {
        USBLog(5, "AppleUSBHubPort[%p]::PortStatusChangedHandler: port %d already in PSCH, setting _retryPortStatus to true", this, _portNum);
        _retryPortStatus = true;
        return;
    }
    
    USBLog(5, "AppleUSBHubPort[%p]::PortStatusChangedHandler: port %d obtained runLock", this, _portNum);

    // Indicate that our thread is running
    //
    _statusChangedState = 0;
    _statusChangedThreadActive = true;

    // Now, loop through each bit in the port status change and see if we need to handle it
    //
    do
    {
        if ( !skipOverGetPortStatus )
        {
            // Do a port status request on current port
            if ((err = _hub->GetPortStatus(&_portStatus, _portNum)))
            {
                
                _statusChangedState = 1;
                
                FatalError(err, "get status (first in port status change)");
                goto errorExit;
            }

            // If a PC Card has been ejected, we might receive 0xff as our status
            //
            if ( (_portStatus.statusFlags == 0xffff) && (_portStatus.changeFlags == 0xffff) )
            {
                err = kIOReturnNoDevice;
                goto errorExit;
            }
            

            _statusChangedState = 2;
            USBLog(5,"AppleUSBHubPort[%p]::PortStatusChangedHandler - port %d - status(%8x)/change(%8x) - clearing retryPortStatus", this, _portNum, _portStatus.statusFlags, _portStatus.changeFlags);
                        
            _retryPortStatus = false;
        }
        
        // First clear the change condition before we return.  This prevents
        // a race condition for handling the change.
        _statusChangedState = 3;
        for (which = 0; which < kNumChangeHandlers; which++)
        {
            // sometimes a change is reported but there really is
            // no change.  This will catch that.
            if (!(_portStatus.changeFlags & _changeHandler[which].bit))
                continue;
            
            USBLog(5, "AppleUSBHubPort[%p]::PortStatusChangedHandler - port %d - change %d clearing 0x%x feature.", this, _portNum, which, _changeHandler[which].clearFeature);
            _statusChangedState = 4;
            if ((err = _hub->ClearPortFeature(_changeHandler[which].clearFeature, _portNum)))
            {
                USBLog(3, "AppleUSBHubPort[%p]::PortStatusChangedHandler - port %d - error %x clearing 0x%x feature.", this, _portNum, err, _changeHandler[which].clearFeature);
                FatalError(err, "clear port vector bit feature");
                goto errorExit;
            }
            
            // Go and dispatch this bit (break out of for loop)
            //
            _statusChangedState = 5;
            break;
        }
        if ( which >= kNumChangeHandlers )
        {
            // Handled all changed handlers, get out of the while loop
            //
            break;
        }
            
        // Do a port status request on current port, after clearing the feature above.
        //
        _statusChangedState = 6;
        if ((err = _hub->GetPortStatus(&_portStatus, _portNum)))
        {
            USBLog(3, "AppleUSBHubPort[%p]::PortStatusChangedHandler: error %x getting port status", this, err);
            FatalError(err, "get status (second in port status change)");
            goto errorExit;
        }

        if ( (_portStatus.statusFlags == 0xffff) && (_portStatus.changeFlags == 0xffff) )
        {
            err = kIOReturnNoDevice;
            goto errorExit;
        }

        _statusChangedState = 7;
        USBLog(5, "AppleUSBHubPort[%p]::PortStatusChangedHandler - port %d - status(%8x) - change(%8x) - before call to (%d) handler function", this, _portNum, _portStatus.statusFlags, _portStatus.changeFlags, which);
            
        _statusChangedState = ((which+1) * 20) + 1;
        err = (this->*_changeHandler[which].handler)(_portStatus.changeFlags, _portStatus.statusFlags);
        _statusChangedState = ((which+1) * 20) + 2;
        USBLog(5,"AppleUSBHubPort[%p]::PortStatusChangedHandler - port %d - err (%x) on return from  call to (%d) handler function", this, _portNum, err, which);

        // Handle the error from the vector
        //
        _statusChangedState = 8;
        if (kIOReturnSuccess == err)
        {
            // Go deal with the next bit
            //
            if ( which == 4 || _retryPortStatus )
                skipOverGetPortStatus = false;
            else
                skipOverGetPortStatus = true;
                
            continue;
        }
        else
        {
            USBLog(3,"AppleUSBHubPort[%p]::PortStatusChangedHandler - port %d - error %x from (%d) handler", this, _portNum, err, which);
            break;
        }
    }
    while (true);

errorExit:
	
    if ( _devZero )
    {
        // We should disable the port here as well..
        //
        _bus->ReleaseDeviceZero();
        _devZero = false;
    }
        
    USBLog(5,"AppleUSBHubPort[%p]::PortStatusChangedHandler - port %d - err = %x - done, releasing _runLock", this, _portNum, err);
    IOLockUnlock(_runLock);
    _statusChangedThreadActive = false;
    if (_inCommandSleep)
    {
	IOCommandGate *gate = NULL;
	if (_bus)
	    gate = _bus->GetCommandGate();
	if (gate)
	{
	    USBLog(3,"AppleUSBHubPort[%p]::PortStatusChangedHandler -  calling commandWakeup", this);
	    gate->commandWakeup(&_statusChangedThreadActive, true);
	}
    }
	
}



bool 
AppleUSBHubPort::StatusChanged(void)
{
    if (!_portStatusChangedHandlerThread)
        return false;
        
    retain();				// since we are about to schedule on a new thread
    thread_call_enter(_portStatusChangedHandlerThread);
    
    return true;
}



void 
AppleUSBHubPort::InitPortVectors(void) 
{
    int vector;
    for (vector = 0; vector < kNumChangeHandlers; vector++)
    {
        _changeHandler[vector] = defaultPortVectors[vector];
        switch (defaultPortVectors[vector].bit)
        {
            case kHubPortOverCurrent:
                _changeHandler[vector].handler = &AppleUSBHubPort::DefaultOverCrntChangeHandler;
                break;
            case kHubPortBeingReset:
                _changeHandler[vector].handler = &AppleUSBHubPort::DefaultResetChangeHandler;
                break;
            case kHubPortSuspend:
                _changeHandler[vector].handler = &AppleUSBHubPort::DefaultSuspendChangeHandler;
                break;
            case kHubPortEnabled:
                _changeHandler[vector].handler = &AppleUSBHubPort::DefaultEnableChangeHandler;
                break;
            case kHubPortConnection:
                _changeHandler[vector].handler = &AppleUSBHubPort::DefaultConnectionChangeHandler;
                break;
        }
    }
}



void 
AppleUSBHubPort::SetPortVector(ChangeHandlerFuncPtr	routine,
                                 UInt32			condition)
{
    int vector;
    for(vector = 0; vector < kNumChangeHandlers; vector++)
    {
        if(condition == _changeHandler[vector].bit)
        {
            _changeHandler[vector].handler = routine;
        }
    }
}


IOReturn 
AppleUSBHubPort::ReleaseDevZeroLock()
{
    USBLog(5, "AppleUSBHubPort[%p]::ReleaseDevZeroLock devZero = %p", this, _devZero);

    if (_devZero)
    {
        (void) _hub->ClearPortFeature(kUSBHubPortEnableFeature, _portNum);
        
        USBError(1, "AppleUSBHubPort[%p]::ReleaseDevZeroLock()", this);
        _state = hpsNormal;
 
        if ( _devZero && _bus )
            _bus->ReleaseDeviceZero();
            
        _devZero = false;
        IOSleep(300);
        
        // Should we turn the power off and then back on?
    }
    
    return kIOReturnSuccess;
}
/*
    // OK, disable the port and try to add the device again.
    if ( err = _hub->ClearPortFeature(kUSBHubPortEnableFeature, _portNum))
    {
        FatalError(err, "clearing port feature");
        return err;
    }
    
    IOSleep(300);
    
    // Get the PortStatus to see if the device is still there.
    if ((err = _hub->GetPortStatus(&status, _portNum)))
    {
        FatalError(err, "getting port status (4)");
        return err;
    }
    
    if (status.statusFlags & kHubPortConnection)
    {
        // We have a connection on this port
        _retryPortStatus = true;
        IOLog("AppleUSBHubPort::ReleaseDevZeroLock: calling ResetPort()\n");
        err = ResetPort();
        return err;
    }
    else
    {
        return kIOReturnSuccess;
    }
}
*/

IOReturn
AppleUSBHubPort::DetachDevice()
{
    UInt32		delay = 0;
    IOUSBHubPortStatus	status;
    IOReturn 		err = kIOReturnSuccess;
    
    // The port should be disabled and the devZero lock released before we get here
    //
    USBLog(3, "AppleUSBHubPort[%p]::DetachDevice Port %d being detached", this, _portNum);

    // Increment our number of attach retries and see if we need to power off the port
    //
    _attachRetry++;
    
    if ( _attachRetry % 4 == 0 )
    {
        
        // This device is misbehaving a lot, wait  before attempting to enumerate it again:
        //
        delay = _attachRetry * 100;
        
        USBLog(2, "AppleUSBHubPort[%p]::DetachDevice (Port %d), attachRetry limit reached. delaying for %d milliseconds", this, _portNum, delay);
        IOSleep(delay);
        
        // Try power off and disabling the port
        //
        if ( (err = _hub->ClearPortFeature(kUSBHubPortPowerFeature, _portNum)) )
        {
            FatalError(err, "clearing port power feature");
            goto ErrorExit;
        }
        
        // Wait for before powering it back on.  Spec says to wait 100ms, we will
        // wait some more.
        //
        IOSleep(delay);
        
        if ( (err = _hub->SetPortFeature(kUSBHubPortPowerFeature, _portNum)) )
        {
            FatalError(err, "setting port power feature");
            goto ErrorExit;
        }
        
        // Since this device is misbehaving, wait before returning here
        //
        IOSleep(delay);
        
        _state = hpsDeadDeviceZero;
        
        err = kIOReturnNotResponding;
    }
    else
    {
        // Get the PortStatus to see if the device is still there.
        if ((err = _hub->GetPortStatus(&status, _portNum)))
        {
            FatalError(err, "getting port status (4)");
            goto ErrorExit;
        }
        
        // If the device is not here, then bail out
        //
        if ( !(status.statusFlags & kHubPortConnection) )
        {
            // We don't have a connection on this port anymore.
            //
            USBLog(5, "AppleUSBHubPort[%p]::DetachDevice - port %d - device has gone away", this, _portNum);
            _state = hpsDeadDeviceZero;
            err = kIOReturnNoDevice;
            goto ErrorExit;
        }

        IOSleep(300);
        
        // Get the PortStatus to see if the device is still there.
        if ((err = _hub->GetPortStatus(&status, _portNum)))
        {
            FatalError(err, "getting port status (4)");
            goto ErrorExit;
        }
        
        if (status.statusFlags & kHubPortConnection)
        {
            _hub->ClearPortFeature(kUSBHubPortEnableFeature, _portNum);

            // We have a connection on this port
            _retryPortStatus = true;
            SetPortVector(&AppleUSBHubPort::DefaultResetChangeHandler, kHubPortBeingReset);

            RemoveDevice();
            
            err = AddDevice();
        }
        else
        {
            err = kIOReturnSuccess;
        }
    }
    
ErrorExit:
    return err;
}

IOReturn
AppleUSBHubPort::GetDevZeroDescriptorWithRetries()
{
    UInt32 	delay = 30;
    UInt32 	retries = 4;
    IOReturn	err = kIOReturnSuccess;
    IOUSBHubPortStatus	status;
    
    do 
    {
        err = _bus->GetDeviceZeroDescriptor(&_desc, 8); 	// get the first 8 bytes
        
        // If the error is kIOReturnOverrun, we still received our 8 bytes, so signal no error. 
        //
        if ( err == kIOReturnOverrun )
        {
            err = kIOReturnSuccess;
            break;
        }
        
        if ( err )
        {
            // Let's make sure that the device is still here.  Maybe it has gone away and we won't process the notification 'cause we're in PSCH
            // Get the PortStatus to see if the device is still there.
            err = _hub->GetPortStatus(&status, _portNum);
            if (err != kIOReturnSuccess)
            {
                FatalError(err, "getting port status (4)");
                break;
            }
            
            if ( !(status.statusFlags & kHubPortConnection) )
            {
                // We don't have a connection on this port anymore.
                //
                USBLog(5, "AppleUSBHubPort[%p]::GetDevZeroDescriptorWithRetries - port %d - device has gone away", this, _portNum);
                _state = hpsDeadDeviceZero;
                err = kIOReturnNoDevice;
                break;
            }
            
            if ( retries == 2)
                delay = 3;
            else if ( retries == 1 )
                delay = 30;
                
            USBLog(3, "AppleUSBHubPort[%p]::GetDevZeroDescriptorWithRetries - port %d, err: %x - sleeping for %d milliseconds", this, _portNum, err, delay);
            IOSleep( delay );
            retries--;
        }
    }
    while ( err && retries > 0 );

    return err;
}

bool
AppleUSBHubPort::AcquireDeviceZero()
{
    IOReturn 	err;
    bool	devZero = false;
    
    err = _bus->AcquireDeviceZero();
    
    if ( err == kIOReturnSuccess )
        devZero = true;
    
    // We use the devZero counter to see "timestamp" each devZero acquisition.  That way we
    // can tell if the devZero that happens at time X is the same one as the one
    // at time 0.
    //
    if ( devZero )
        _devZeroCounter++;
    
    return devZero;
}

void
AppleUSBHubPort::DisplayOverCurrentNotice(bool individual)
{
    if ( individual )
        _hub->_device->DisplayUserNotification(kUSBIndividualOverCurrentNotificationType);
    else
        _hub->_device->DisplayUserNotification(kUSBGangOverCurrentNotificationType);

    return;
}

bool
AppleUSBHubPort::willTerminate( IOService * provider, IOOptionBits options )
{
    USBLog(3, "AppleUSBHubPort[%p]::willTerminate", this);

    ReleaseDevZeroLock();

    return false;
}
