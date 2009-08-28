/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright й 1998-2009 Apple Inc.  All rights reserved.
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
#include <IOKit/usb/IOUSBHubDevice.h>
#include <IOKit/usb/IOUSBControllerV2.h>
#include <IOKit/usb/IOUSBControllerV3.h>
#include <IOKit/usb/IOUSBLog.h>
#include "AppleUSBHubPort.h"
#include "USBTracepoints.h"

#define super OSObject

static CaptiveErrataListEntry  gErrataList[] = {
    { 0x05ac, 0x8000, 0x8FFF },		// All internal range devices
	{ 0x05ac, 0x020e, 0x021c }, 
	{ 0x05ac, 0x0223, 0x0226 },
	{ 0x05ac, 0x0229, 0x022b },
	{ 0x05ac, 0x022f, 0x022f },
	{ 0x05ac, 0x0230, 0x0238 },
	{ 0x0a5c, 0x4500, 0x4500 },		// Bluetooth Internal Pseudo-Hub
	{ 0x05ac, 0x0307, 0x0307 }, 
	{ 0x05ac, 0x0201, 0x0206 }, 
	{ 0x05ac, 0x9212, 0x9217 }, 
	
};

#define ERRATALISTLENGTH (sizeof(gErrataList)/sizeof(CaptiveErrataListEntry))

OSDefineMetaClassAndStructors(AppleUSBHubPort, OSObject)

static portStatusChangeVector defaultPortVectors[kNumChangeHandlers] =
{
    { 0, kHubPortOverCurrent,	kUSBHubPortOverCurrentChangeFeature },
    { 0, kHubPortBeingReset, 	kUSBHubPortResetChangeFeature },
    { 0, kHubPortSuspend,		kUSBHubPortSuspendChangeFeature },
    { 0, kHubPortEnabled,		kUSBHubPortEnableChangeFeature },
    { 0, kHubPortConnection,	kUSBHubPortConnectionChangeFeature },
};



IOReturn 
AppleUSBHubPort::init( AppleUSBHub *parent, int portNum, UInt32 powerAvailable, bool captive )
{
    _hub						= parent;
    _bus						= parent->_bus;
    _hubDesc					= &parent->_hubDescriptor;
    _portNum					= portNum;
    _portDevice					= NULL;
    _portPowerAvailable			= powerAvailable;
    _captive					= captive;
    _state						= hpsNormal;
    _retryPortStatus			= false;
    _statusChangedThreadActive	= false;
    _initThreadActive			= false;
    _inCommandSleep				= false;
    _attachRetry				= 0;
    _devZeroCounter				= 0;
	_attachMessageDisplayed		= false;
    _overCurrentNoticeDisplayed = false;
	_portPMState				= usbHPPMS_uninitialized;
	_resumePending				= false;
	_portResumeRecoveryTime		= kPortResumeRecoveryTime;
	clock_get_uptime(&_overCurrentNoticeTimeStamp);

    if (!_hub || !_bus || !_hubDesc || (portNum < 1) || (portNum > 64))
	{
		USBLog(2,"AppleUSBHubPort[%p]::init failure (Parent: %p, Bus: %p, Desc: %p, PortNum: %d", this, parent, _bus, _hubDesc, portNum); 
        return kIOReturnBadArgument;
    }
    _runLock = IOLockAlloc();
    if (!_runLock)
	{
		USBLog(2,"AppleUSBHubPort[%p]::init Could not allocate the _runLock", this);
        return kIOReturnNoMemory;
	}
	
    _initLock = IOLockAlloc();
    if (!_initLock)
	{
		USBLog(2,"AppleUSBHubPort[%p]::init Could not allocate the _initLock", this);
        IOLockFree(_runLock);
        return kIOReturnNoMemory;
	}
	
    _removeDeviceLock = IOLockAlloc();
    if (!_removeDeviceLock)
	{
		USBLog(2,"AppleUSBHubPort[%p]::init Could not allocate the _removeDeviceLock", this);
        IOLockFree(_runLock);
        IOLockFree(_initLock);
        return kIOReturnNoMemory;
	}
	
    _initThread = thread_call_allocate((thread_call_func_t)PortInitEntry, (thread_call_param_t)this);
    if (!_initThread)
    {
		USBLog(2,"AppleUSBHubPort[%p]::init Could not allocate the _initThread", this);
        IOLockFree(_runLock);
        IOLockFree(_initLock);
        IOLockFree(_removeDeviceLock);
        return kIOReturnNoMemory;
    }
    
    _portStatusChangedHandlerThread = thread_call_allocate((thread_call_func_t)PortStatusChangedHandlerEntry, (thread_call_param_t)this);
    
    if (!_portStatusChangedHandlerThread)
    {
		USBLog(2,"AppleUSBHubPort[%p]::init Could not allocate the _portStatusChangedHandlerThread", this);
        thread_call_free(_initThread);
        IOLockFree(_runLock);
        IOLockFree(_initLock);
        IOLockFree(_removeDeviceLock);
        return kIOReturnNoMemory;
    }
	
    InitPortVectors();
    
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBHubPort::start(void)
{

	if (_hub && _initThread)
	{
		USBLog(5, "AppleUSBHubPort[%p]::start: forking init thread", this);
		retain();								// since we are about to schedule on a new thread
		_hub->retain();
		 USBLog(6, "AppleUSBHubPort[%p]::start - calling RaisePowerState and IncrementOutstandingIO on hub[%p] port %d", this, _hub, _portNum);
		_hub->RaisePowerState();				// make sure that the hub is at a good power state until we are done with the init
		_hub->IncrementOutstandingIO();
		if ( thread_call_enter(_initThread) == TRUE )
		{
			 USBLog(6, "AppleUSBHubPort[%p]::start - calling DecrementOutstandingIO on hub[%p] port %d", this, _hub, _portNum);
			_hub->DecrementOutstandingIO();
			_hub->release();
			 USBLog(6, "AppleUSBHubPort[%p]::start - calling LowerPowerState on hub[%p] port %d", this, _hub, _portNum);
			_hub->LowerPowerState();
			release();
		}
		
		USBLog(5, "AppleUSBHubPort[%p]::start: fork complete", this);
	}
	else 
	{
		USBLog(1, "AppleUSBHubPort[%p]::start: missing _hub or _initThread. Bailing quietly.", this);
	}

    return kIOReturnSuccess;
}



void
AppleUSBHubPort::free(void)
{
	if (_runLock) 
	{
		IOLockFree(_runLock);
	    _runLock = NULL;
	}
	if (_initLock) 
	{
		IOLockFree(_initLock);
		_initLock = NULL;
	}
	if (_removeDeviceLock) 
	{
		IOLockFree(_removeDeviceLock);
		_removeDeviceLock = NULL;
	}
	super::free();
}


void 
AppleUSBHubPort::stop(void)
{
	IOReturn				err;
	IOUSBControllerV3		*v3Bus = OSDynamicCast(IOUSBControllerV3, _bus);
	
    USBLog(3, "AppleUSBHubPort[%p]::stop called, _devZero = (%d).", this, _devZero);
	
	
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
				IOReturn kr = kIOReturnSuccess;
				
				USBLog(2, "AppleUSBHubPort[%p]::stop - trying command sleep (%d/%d).", this, _statusChangedThreadActive, _initThreadActive);
				_inCommandSleep = true;
				if (_statusChangedThreadActive)
				{
					kr = gate->commandSleep(&_statusChangedThreadActive);
					if (kr != THREAD_AWAKENED)
					{
						USBLog(5,"AppleUSBHubPort[%p]::stop  _statusChangedThreadActive commandSleep returned %d", this, kr);
					}
				}
				else if (_initThreadActive)
				{
					kr = gate->commandSleep(&_initThreadActive);
					if (kr != THREAD_AWAKENED)
					{
						USBLog(5,"AppleUSBHubPort[%p]::stop  _initThreadActive commandSleep returned %d", this, kr);
					}
				}
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

	USBLog(3, "AppleUSBHubPort[%p]::stop - calling RemoveDevice", this);
	RemoveDevice();
	
	if (v3Bus && (v3Bus->IsControllerAvailable()) && !_hub->isInactive() && !_hub->_portSuspended)
	{
		// new: Try power off and disabling the port
		//
		USBLog(5, "AppleUSBHubPort[%p]::stop - removing port power", this);
		if ( (err = _hub->ClearPortFeature(kUSBHubPortPowerFeature, _portNum)) )
		{
			USBLog(1, "AppleUSBHubPort[%p]::stop - err (%p) from ClearPortFeature", this, (void*)err);
			USBTrace( kUSBTHubPort,  kTPHubPortStop, (uintptr_t)this, err, 0, 0 );
		}
	}

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
	me->_hub->release();
    me->release();
}


void 
AppleUSBHubPort::PortInit()
{
    IOUSBHubPortStatus	status;
    IOReturn		err;
    
    USBLog(5, "***** AppleUSBHubPort[%p]::PortInit - port %d on hub %p beginning INIT (getting _initLock)", this, _portNum, _hub);
    _initThreadActive = true;
    IOLockLock(_initLock);
    
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
	
    USBLog(5, "***** AppleUSBHubPort[%p]::PortInit - port %d on hub %p - status(%04x), change(%04x) bits detected", this, _portNum, _hub, status.statusFlags, status.changeFlags);
	
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
		USBTrace(kUSBTEnumeration, kTPEnumerationCallAddDevice, (uintptr_t)this, _portNum, _hub->_locationID, 0);
        if ((err = AddDevice()))
            FatalError(err, "adding device");
    }
	
errorExit:
		
	USBLog(5, "***** AppleUSBHubPort[%p]::PortInit - port %d on hub %p - done - releasing _initLock", this, _portNum, _hub);
    IOLockUnlock(_initLock);
    _initThreadActive = false;
	 USBLog(6, "AppleUSBHubPort[%p]::PortInit - calling LowerPowerState and DecrementOutstandingIO on hub[%p] port %d", this, _hub, _portNum);
 	_hub->LowerPowerState();
 	_hub->DecrementOutstandingIO();
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
    IOReturn			err = kIOReturnSuccess;
    bool				checkingForDeadHub = false;
    IOUSBHubPortStatus	status;
	int					i, j;
	bool				resetActive;

    USBLog(5, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub %p - start", this, _portNum, _hub);
	USBTrace_Start(kUSBTEnumeration, kTPEnumerationAddDevice, (uintptr_t)this, _portNum, _hub->_locationID, 0);
	// if we are about to change to off, restart, or sleep, then don't do this
	if ((_hub->_powerStateChangingTo < kIOUSBHubPowerStateLowPower) && (_hub->_powerStateChangingTo != kIOUSBHubPowerStateStable))
	{
		USBLog(5, "***** AppleUSBHubPort[%p]::AddDevice - hub[%p] changing to power state[%d] - aborting AddDevice", this, _hub, (int)_hub->_powerStateChangingTo);
	}
    else do
    {
         // Indicate that we are dealing with device zero, still
        if ( !_devZero )
        {
            USBLog(5, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub %p - bus %p - acquiring dev zero lock", this, _portNum, _hub, _bus);
            _devZero = AcquireDeviceZero();
            if (!_devZero)
            {
				USBLog(2, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub %p - bus %p - unable to get devZero lock", this, _portNum, _hub, _bus);
				FatalError(kIOReturnCannotLock, "acquiring device zero");
                break;
            }
        }
        else
        {
            USBLog(5, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub %p - bus %p - already owned devZero lock", this, _portNum, _hub, _bus);
        }

        USBLog(5, "***** AppleUSBHubPort[%p]::AddDevice - port %d @ 0x%x - resetting port", this, (uint32_t)_portNum, (uint32_t)_hub->_locationID);
		USBTrace(kUSBTEnumeration, kTPEnumerationResetPort, (uintptr_t)this, _portNum, _hub->_locationID, 0);
		
        SetPortVector(&AppleUSBHubPort::AddDeviceResetChangeHandler, kHubPortBeingReset);

		err = _hub->SetPortFeature(kUSBHubPortResetFeature, _portNum);
        if (err)
        {
			if ( err != kIOUSBDeviceNotHighSpeed)
            {
				USBLog(1, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub %p - unable (err = %x) to reset port (set feature (resetting port)", this, _portNum, _hub, err);
				USBTrace( kUSBTHubPort,  kTPHubPortAddDevice, _portNum, (uintptr_t)_hub, err, 1 );
			}
			
			if (_portDevice)
			{
				USBLog(1,"AppleUSBHubPort: Removing %s from port %d of Hub at 0x%x", _portDevice->getName(), _portNum, (uint32_t)_hub->_locationID);
				USBTrace( kUSBTHubPort,  kTPHubPortAddDevice,  _portNum, (uintptr_t)_hub, _hub->_locationID, 2 );
				RemoveDevice();	
			}
			
			if (err == kIOUSBTransactionTimeout)
			{
				_hub->CallCheckForDeadHub();			// pull the plug if it isn't already
				checkingForDeadHub = true;
			}
            break;
        }
		IOSleep(1);
        err = _hub->GetPortStatus(&status, _portNum);
		j=0;
		resetActive = false;
		
		if ((status.statusFlags & kHubPortBeingReset) || (status.changeFlags & kHubPortBeingReset) || (_state == hpsSetAddress))
			resetActive = true;
		
		while (j++ < 5 && !resetActive && !err)
		{
			i = 0;
			while ((i++ < 10) && !err)
			{
				err = _hub->GetPortStatus(&status, _portNum);
				if ((status.statusFlags & kHubPortBeingReset) || (status.changeFlags & kHubPortBeingReset) || (_state == hpsSetAddress))
				{
					resetActive = true;
					break;						// inner loop
				}
				USBLog(2, "AppleUSBHubPort[%p]::AddDevice - port[%d] not in reset after %d ms", this, _portNum, i);
				if (i == 10)
				{
					USBLog(2, "AppleUSBHubPort[%p]::AddDevice - retrying SetPortReset", this);
					err = _hub->SetPortFeature(kUSBHubPortResetFeature, _portNum);
				}
				IOSleep(1);
			}
		}

		if ( !resetActive)
		{
			USBLog(1, "AppleUSBHubPort[%p]::AddDevice - port[%d] not in reset after 5 retries", this, _portNum);
			USBTrace( kUSBTHubPort,  kTPHubPortAddDevice, (uintptr_t)this, _portNum, 0, 3 );
		}
		
        USBLog(5, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub %p - sleeping 100 ms - err[%p]", this, _portNum, _hub, (void*)err);
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
	USBTrace_End(kUSBTEnumeration, kTPEnumerationAddDevice, (uintptr_t)this, _portNum, _hub->_locationID, err);
	
    return(err);
}



void 
AppleUSBHubPort::RemoveDevice(void)
{
    bool					ok;
    const IORegistryPlane 	*usbPlane;
    IOUSBDevice				*cachedPortDevice;

	// synchronize access to the _portDevice and NULL it out
	IOLockLock(_removeDeviceLock);
	cachedPortDevice = _portDevice;
	_portDevice = NULL;
	IOLockUnlock(_removeDeviceLock);

    if (cachedPortDevice)
    {
		USBLog(4, "AppleUSBHubPort[%p]::RemoveDevice start (%s)", this, cachedPortDevice->getName());
		usbPlane = cachedPortDevice->getPlane(kIOUSBPlane);
		
		if ( _usingExtraPortPower )
		{
			USBLog(4, "AppleUSBHubPort[%p]::RemoveDevice  returning _portPowerAvailable to kUSB100mAAvailable", this);
			_portPowerAvailable = kUSB100mAAvailable;
			_usingExtraPortPower = false;
		}
		
		if ( usbPlane )
			cachedPortDevice->detachAll(usbPlane);

		if (_hub && !_hub->isInactive())
		{
			UInt32		retries = 100;
			USBLog(4, "AppleUSBHubPort[%p]::RemoveDevice - hub still active - terminating device[%p] synchronously", this, cachedPortDevice);
			// before issuing a Synchronous terminate, we need to make sure that the device is not busy
			while (cachedPortDevice->getBusyState() && retries--)
			{
				// wait up to 10 seconds for the device to get un-busy
				USBLog(2, "AppleUSBHubPort[%p]::RemoveDevice - device(%p)[%s] busy - waiting 100ms (retries remaining: %d)", this, cachedPortDevice, cachedPortDevice->getName(), (int)retries);
				IOSleep(100);
			}
			if (cachedPortDevice->getBusyState())
			{
				USBError(1, "AppleUSBHubPort: Port %d of Hub at %p about to terminate a busy device (%s) after waiting 10 seconds", _portNum, (void*)_hub->_locationID, cachedPortDevice->getName());
			}
			cachedPortDevice->terminate(kIOServiceRequired);
		}
		else
		{
			USBLog(4, "AppleUSBHubPort[%p]::RemoveDevice - hub no longer active - terminating device[%p] asynchronously", this, cachedPortDevice);
			cachedPortDevice->terminate(kIOServiceRequired);
		}

		USBLog(4, "AppleUSBHubPort[%p]::RemoveDevice releasing(%p)", this, cachedPortDevice);
		cachedPortDevice->release();

    }
	else
	{
        USBLog(3, "AppleUSBHubPort[%p]::RemoveDevice - looks like someone beat us to it", this);
	}

    InitPortVectors();
}

IOReturn 
AppleUSBHubPort::Suspend( bool fromDevice, bool portStatusSuspended )
{
	IOReturn status	= kIOReturnSuccess;

	if ( fromDevice and _portPMState == usbHPPMS_pm_suspended )
	{
		USBLog(3, "AppleUSBHubPort[%p]::Suspend - converting suspend state from pm_suspended to drvr_suspended portStatusSuspended(%d)", this, portStatusSuspended);
		if ( portStatusSuspended )
		{
			IOUSBControllerV3		*v3Bus = NULL;
			
			if (_hub)
			{
				if (_hub->_device)
					v3Bus = OSDynamicCast(IOUSBControllerV3, _hub->_device->GetBus());
				
				if (v3Bus && _portDevice)
				{
					USBLog(5, "AppleUSBHub[%p]::Suspend - Enabling endpoints for device at address (%d)", this, (int)_portDevice->GetAddress());
					status = v3Bus->EnableAddressEndpoints(_portDevice->GetAddress(), true);
					if (status)
					{
						USBLog(2, "AppleUSBHub[%p]::Suspend - EnableAddressEndpoints returned (%p)", this, (void*)status);
					}
				}
			}
		}
		else
		{
			USBError(1, "AppleUSBHub[%p]::Suspend - expected port to be suspended for conversion, but it is not!!", this);
		}
	}
	
	_portPMState = ( fromDevice ) ? usbHPPMS_drvr_suspended :  usbHPPMS_pm_suspended ;

	status = _hub->SetPortFeature(kUSBHubPortSuspendFeature, _portNum);
	if ( status != kIOReturnSuccess )
	{
		// Root Hub failed for lucent bug
		USBLog(3, "AppleUSBHubPort[%p]::Suspend Could not SetPortFeature (%d) (kUSBHubPortSuspendFeature): (0x%x)", this, _portNum, status);
	}
	else
	{
		// Wait for 3ms and if you are in gate, make sure you use command sleep with timeout.
		WaitForSuspendCommand( &_portPMState, 3 );
	}

	return status;
}

IOReturn 
AppleUSBHubPort::Resume( )
{
	IOReturn status = kIOReturnSuccess;
	
	USBLog(5, "AppleUSBHubPort[%p]::Resume - calling ClearPortFeature", this);
	status = _hub->ClearPortFeature(kUSBHubPortSuspendFeature, _portNum);
	
	if ( status != kIOReturnSuccess )
	{
		USBLog(3, "AppleUSBHubPort[%p]::Resume Could not ClearPortFeature (%d) (kUSBHubPortSuspendFeature): (0x%x)", this, _portNum, status);
		SetPortVector(&AppleUSBHubPort::DefaultSuspendChangeHandler, kHubPortSuspend);
	}
	else
	{
		// Set up a flag indicating that we are expecting a resume port status change
		_resumePending = true;
		USBLog(2, "AppleUSBHubPort[%p]::Resume - RESUME - calling RaisePowerState on hub[%p] port %d and setting _lowerPowerStateOnResume", this, _hub, _portNum);
		_hub->RaisePowerState();				// make sure that the hub is at a good power state until the resume is done
		
		if (_lowerPowerStateOnResume)
		{
			USBLog(1, "AppleUSBHubPort[%p]::Resume - _lowerPowerStateOnResume already set (hub %p port %d)- UNEXPECTED!", this, _hub, _portNum);
		}
		USBLog(5, "AppleUSBHubPort[%p]::Resume - setting _lowerPowerStateOnResume - (hub %p port %d)", this, _hub, _portNum);
		_lowerPowerStateOnResume = true;
		
		// Wait for 100ms and if you are in gate, make sure you use command sleep with timeout.
		WaitForSuspendCommand( &_portPMState, 100 );
	}
	
	return status;
}


void
AppleUSBHubPort::MessageDeviceClients( UInt32 type, void * argument, vm_size_t argSize )
{
	IOUSBDevice *cachedDevice = _portDevice;			// in case _portDevice goes away while we are messaging
	if ( cachedDevice )
	{
		cachedDevice->retain();
		cachedDevice->messageClients( type, argument, argSize );
		cachedDevice->release();
	}
}


IOReturn 
AppleUSBHubPort::SuspendPort( bool suspend, bool fromDevice )
{
    IOReturn			status = kIOReturnSuccess;
    IOUSBHubPortStatus	hubPortStatus;
	UInt32				resumeRetries = 10;
	
    USBLog(5, "AppleUSBHubPort[%p]::SuspendPort(%s) for port %d fromDevice(%s), _resumePending(%d), isInactive(%s)", this, suspend ? "suspend" : "resume", _portNum, fromDevice ? "true" : "false", _resumePending, _hub->isInactive() ? "true" : "false");

	// If there is a resume pending, then wait until it happens
	while ( _resumePending and resumeRetries > 0 )
	{
		IOSleep(10);
		USBLog(5, "AppleUSBHubPort[%p]::SuspendPort(%s) for port %d, waiting for _resumePending %d", this, suspend ? "suspend" : "resume", (uint32_t)_portNum, (uint32_t)resumeRetries);
		resumeRetries--;
	}
	
	if ( resumeRetries == 0 )
	{
		USBLog(2, "AppleUSBHubPort[%p]::SuspendPort(%s) for port %d, did not clear the resumePending flag", this, suspend ? "suspend" : "resume", _portNum);
	}
	
	if (_hub->isInactive())
	{
		// if the hub is inactive, it is probably unplugged, so we can resume a port for free
		if ( not suspend && (_portPMState == usbHPPMS_drvr_suspended) )
		{
			USBLog(3, "AppleUSBHubPort[%p]::SuspendPort(resume) on port(%d)- Inactive hub, doing for free!", this, _portNum);
			_portPMState = usbHPPMS_active;
		}
		else
		{
			USBLog(3, "AppleUSBHubPort[%p]::SuspendPort on port(%d) - not doing anything (%s) _portDevice(%p) _portPMState(%d) !", this, _portNum, suspend ? "suspend" : "resume", _portDevice, (int)_portPMState);
		}
	}
	else
	{
		// If resuming, need to check that the port was suspended to begin with
		//
		status = _hub->GetPortStatus(&hubPortStatus, _portNum);
		if ( status != kIOReturnSuccess )
		{
			USBLog(3,"AppleUSBHubPort[%p]::SuspendPort Could not get Port Status: 0x%x", this, status);
			return status;
		}
		
		USBLog(5, "AppleUSBHubPort[%p]::SuspendPort - GetPortStatus returned status[%p] change[%p]", this, (void*)hubPortStatus.statusFlags, (void*)hubPortStatus.changeFlags );
		
		// OK, set up the handler for the set/clear suspend feature
		USBLog(7, "AppleUSBHubPort[%p]::SuspendPort - setting vector to HandleSuspendPortHandler", this);
		SetPortVector(&AppleUSBHubPort::HandleSuspendPortHandler, kHubPortSuspend);
		
		if ( suspend )
		{
			status = Suspend( fromDevice, hubPortStatus.statusFlags & kHubPortSuspend );
		}
		else
		{
			if ( not (hubPortStatus.statusFlags & kHubPortSuspend) )
			{
				USBLog(5,"AppleUSBHubPort[%p]::SuspendPort Port was NOT suspended", this);
				status = kIOUSBDevicePortWasNotSuspended;
			}
			else
			{
				status = Resume();
			}
		}
	}
	
	return status;
}



IOReturn
AppleUSBHubPort::ReEnumeratePort(UInt32 options)
{
    USBLog(5,"AppleUSBHubPort[%p]::ReEnumeratePort -- reenumerating port %d, options 0x%x",this, (uint32_t)_portNum, (uint32_t)options);

    // Test to see if bit31 is set, and if so, set up a flag to indicate that we need to wait 100ms after a reset and before
    // talking to the device (Bluetooth workaround)
    if ( (options & kUSBAddExtraResetTimeMask) )
    {
        USBLog(5,"AppleUSBHubPort[%p]::ReEnumeratePort -- reenumerating port %d, options 0x%x",this, (uint32_t)_portNum, (uint32_t)options);
        _extraResetDelay = true;
    }
    else
    {
        _extraResetDelay = false;
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
	UInt8			deviceAddress;			//<<0
	UInt8			endpointNum;			//<<8
	UInt8			endpointType;			//<<16 // As split transaction. 00 Control, 10 Bulk
	UInt8			IN;						//<<24 // Direction, 1 = IN, 0 = OUT};
	UInt32			opts= options;
	
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
    USBLog(5,"AppleUSBHubPort[%p]::ClearTT -- port %d, options:%X, wValue:%X, wIndex:%X, IOReturn: 0x%x",this, (uint32_t)_portNum, (uint32_t)opts, wValue, wIndex, err);
    
    if( (err == kIOReturnSuccess) && (endpointType == 0) )	// Control endpoint.
    {
		wValue ^= (1 << 15);	// Flip direction bit and do it again.
		request.wValue = wValue;
		err = _hub->DoDeviceRequest(&request);
		USBLog(5,"AppleUSBHubPort[%p]::ClearTT -- do it again for control transactions, wValue:%X, IOReturn: 0x%x",this, wValue, err);
    }
    
    return err;
}

IOReturn 
AppleUSBHubPort::ResetPort()
{
    IOReturn 		err = kIOReturnSuccess;
    IOUSBHubPortStatus	status;
	bool				wait100ms = false;

    USBLog(5, "+AppleUSBHubPort[%p]::ResetPort for port %d", this, _portNum);
	if ( ShouldApplyDisconnectWorkaround() )
	{
		USBLog(5, "AppleUSBHubPort[%p]::ResetPort for port %d, will wait 500 ms", this, _portNum);
		IOSleep(500);
		wait100ms = true;
	}
	
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

#if 0
		// This code has been disabled for Leopard, as per:
		//
		// <rdar://problem/4449183> HUB Driver - Should be OK to reset a port even if it is suspended
		//
		// When enabled, we check to see if the port is suspended and if so, clear that feature
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
#endif

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

		if ( wait100ms )
		{
			USBLog(5, "AppleUSBHubPort[%p]::ResetPort for port %d, waiting for 100ms after reset", this, _portNum);
			IOSleep(100);
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

	// If we are returning an error, then we need to tell the IOUSBDevice that we had an error
    if (err && _portDevice)
    {
		IOUSBDevice *cachedDevice = _portDevice;			// in case _portDevice goes away while we are messaging
		
		if (cachedDevice)
		{
			USBLog(5, "AppleUSBHubPort[%p]::ResetPort - port %d, Sending kIOUSBMessagePortHasBeenReset message (0x%x)", this, _portNum, err);
			cachedDevice->retain();
			cachedDevice->message(kIOUSBMessagePortHasBeenReset, cachedDevice, &err);
			cachedDevice->release();
		}
    }
    USBLog(5, "-AppleUSBHubPort[%p]::ResetPort for port %d", this, _portNum);
    return err;
}



void 
AppleUSBHubPort::FatalError(IOReturn err, const char *str)
{
    // Don't USBError if we are a HS Root hub and the error is kIOUSBDeviceNotHighSpeed.  Prevents us from showing false errors in GM builds in the system.log
    //
    if (_hub->IsHSRootHub())
    {
		if (err != kIOUSBDeviceNotHighSpeed)
		{
			USBLog(1, "AppleUSBHubPort[%p]:FatalError - Port %d of Hub at 0x%x: error 0x%x: %s", this, (uint32_t)_portNum, (uint32_t)_hub->_locationID, err, str);
			USBTrace( kUSBTHubPort,  kTPHubPortFatalError, err, _portNum, _hub->_locationID, 1 );
		}
    }
    else
    {
		USBError(1, "AppleUSBHubPort[%p]::FatalError - Port %d of Hub at 0x%x reported error 0x%x while doing %s", this, (uint32_t)_portNum, (uint32_t)_hub->_locationID, err, str);
    }
    
    if (_portDevice)
    {
        USBLog(1,"AppleUSBHubPort[%p]::FatalError - Removing %s from port %d of Hub at 0x%x", this, _portDevice->getName(), _portNum, (uint32_t)_hub->_locationID);
		USBTrace( kUSBTHubPort,  kTPHubPortFatalError, (uintptr_t)_portDevice, _portNum, _hub->_locationID, 2 );
        RemoveDevice();	
    }
}

static IOReturn DoCreateDevice(	IOUSBController		*bus,
                                IOUSBDevice 		*newDevice,
                                USBDeviceAddress	deviceAddress,
                                UInt8				maxPacketSize,
                                UInt8				speed,
                                UInt32				powerAvailable,
                                USBDeviceAddress	hub,
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
    IOReturn				err = kIOReturnSuccess;
    IOReturn				err2 = kIOReturnSuccess;
    IOUSBDevice	*			usbDevice; 
    USBDeviceAddress		address;
    UInt32					delay = 10;
    const IORegistryPlane 	* usbPlane;
   
    USBLog(5, "***** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d on hub %p - start - status(0x%04x) change (0x%04x)", this, _portNum, _hub, (int)statusFlags, (int)changeFlags);
	USBTrace_Start(kUSBTEnumeration, kTPEnumerationAddDeviceResetChangeHandler, (uintptr_t)this, _portNum, _hub->_locationID, err);

    if ( _extraResetDelay )
    {
        USBLog(5, "***** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - delaying 100ms workaround", this);
        IOSleep(100);
        _extraResetDelay = false;
    }
    
    do
    {
        if (_state != hpsDeadDeviceZero)
        {
             if (changeFlags & kHubPortEnabled) 
            {
                // We don't have a connection on this port anymore.
                //
                USBLog(2, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d - enabled bit is set in the change flags", this, _portNum);
            }

           // Before doing anything, check to see if the device is really there
            //
            if ( !(statusFlags & kHubPortConnection) )
            {
                // We don't have a connection on this port anymore.
                //
                USBLog(5, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d - device has gone away", this, _portNum);
                _state = hpsDeadDeviceZero;
                err = kIOReturnNoDevice;
                break;
            }

            if (changeFlags & kHubPortConnection) 
            {
                // We don't have a connection on this port anymore.
                //
                USBLog(5, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d - device appears to have gone away and then come back", this, _portNum);
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
                USBLog(3, "**1** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d on hub %p - new delay %d", this, (uint32_t)_portNum, _hub, (uint32_t)delay);
            }
                
            // Now wait 10 ms (or 300ms -- see above) after reset
            //
            USBLog(5, "**1** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d on hub %p - delaying %d ms", this, (uint32_t)_portNum, _hub, (uint32_t)delay);
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
			
			// 4693694: configure HS devices with a MPS of 64, which is the spec for those device
            err = DoConfigureDeviceZero(_bus, (kUSBDeviceSpeedHigh == _speed) ? 64 : 8, _speed,  _hub->_device->GetAddress(), _portNum);
            
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
                USBLog(5, "**3** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d on hub %p - failed to get dev zero desc, detach'ing device", this, _portNum, _hub);
                _getDeviceDescriptorFailed = true;
                _state = hpsDeadDeviceZero;
				
				// OK, disable the port and try to add the device again
				//
				if ( (err = _hub->ClearPortFeature(kUSBHubPortEnableFeature, _portNum)) )
				{
					USBLog(3, "**3** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, unable (err = %x) to disable port", this, _portNum, err);
					FatalError(err, "clearing port feature");
					_bus->ReleaseDeviceZero();
					_devZero = false;
					_portDevice = NULL;
					return err;
				}
				
				_bus->ReleaseDeviceZero();
				_devZero = false;
				_state = hpsSetAddressFailed;
				
				return DetachDevice();
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
            USBLog(3, "**4** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, previous SetAddress failed, sleeping for %d milliseconds", this, (uint32_t)_portNum, (uint32_t)delay);
            IOSleep(delay);
        }


        // MacOS 9 STATE 5
        
        // (Note: The power stuff is passed in so we don't need to check it here, as we do in 9)
        //
        if ( err == kIOReturnSuccess )
            _getDeviceDescriptorFailed = false;
            
         _state = hpsSetAddress;
        
        // Create and address the device (which could be a hub device)
        //
		if (_desc.bDeviceClass == kUSBHubClass)
		{
			// Before making the device, make sure that this is not the 6th Hub in the bus, as that is illegal.  We use our locationID for this purpose.  If the next to last nibble is not 0, then it means
			// that we already have a 5th hub
			if ( (_hub->_locationID) & 0x000000F0 )
			{
				USBLog(1,"**5** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d of hub @ 0x%x,  we have a hub, but this would be the 6th hub in the bus, which is illegal.  Erroring out", this, _portNum, (uint32_t)_hub->_locationID );
				USBError(1,"A USB Hub (connected to the hub at 0x%x) has been plugged in but it will result in an illegal configuration.  The hub will not be enabled.", (uint32_t)_hub->_locationID);
				USBTrace( kUSBTHubPort,  kTPHubPortAddDeviceResetChangeHandler, (uintptr_t)this, _portNum, _hub->_locationID, 1 );
                _bus->ReleaseDeviceZero();
                _devZero = false;
				_portDevice = NULL;
				err = kIOReturnNoDevice;
				break;
			}	
			
			usbDevice = _bus->MakeHubDevice( &address );
		}
		else
			usbDevice = _bus->MakeDevice( &address );
   		
    	if (usbDevice == NULL || address == 0)
    	{
            // Setting the Address failed
            // 
            USBLog(1,"**5** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d of hub @ 0x%x, unable to set device %p to address %d - disabling port", this, _portNum, (uint32_t)_hub->_locationID, usbDevice, address );
			USBTrace( kUSBTHubPort,  kTPHubPortAddDeviceResetChangeHandler, _portNum, _hub->_locationID, (uintptr_t)usbDevice, address);
 
            // OK, disable the port and try to add the device again
            //
            if ( (err = _hub->ClearPortFeature(kUSBHubPortEnableFeature, _portNum)) )
            {
                USBLog(3, "**5** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, unable (err = %x) to disable port", this, _portNum, err);
                FatalError(err, "clearing port feature");
                _bus->ReleaseDeviceZero();
                _devZero = false;
                _state = hpsSetAddressFailed;
                _portDevice = NULL;
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
                USBLog(3, "**8** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, sleeping for %d milliseconds", this, (uint32_t)_portNum, (uint32_t)delay);
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
        
		err = DoCreateDevice(_bus, usbDevice, address, _desc.bMaxPacketSize0, _speed, _portPowerAvailable, _hub->_device->GetAddress(), _portNum);
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
		}
	
        // In MacOS 9, we attempt to get the full DeviceDescriptor at this point, if we had missed it earlier.  On X, we do NOT do this
        // because we would have failed the IOUSBDevice::start if we didn't get it.
        //

        USBLog(5, "**10** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler -  port %d, at addr: %d, Succesful", this, _portNum, address);
        // MacOS STATE 10
        //
        _attachRetry = 0;
		if ( _attachMessageDisplayed )
		{
			USBError(1,"[%p] The IOUSBFamily has successfully enumerated the device.", this);
			_attachMessageDisplayed = false;
        }
       
		// Finally use the data gathered to do some final setup for the device
		
        // link the new device as a child of my hub
        usbPlane = _portDevice->getPlane(kIOUSBPlane);
        if ( usbPlane )
		{
            _portDevice->attachToParent( _hub->_device, usbPlane);
		}
		else
		{
			USBLog(1, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - we could not get the kIOUSBPlane!!  Problems ahead", this);
			USBTrace( kUSBTHubPort,  kTPHubPortAddDeviceResetChangeHandler, (uintptr_t)this, (uintptr_t)usbPlane, 0, 2);
		}
        
        // Create properties in the device nub
        _portDevice->setProperty("PortNum",_portNum,32);
        _portDevice->SetProperties();
		
		// Let the nub know who it's HubPolicyMaker is
		_portDevice->SetHubParent(_hub);
		
        if ( IsCaptive() || IsCaptiveOverride(_portDevice->GetVendorID(), _portDevice->GetProductID()) )
            _portDevice->setProperty("non-removable","yes");

		// are we capable of handing out more power..
		if (_portPowerAvailable == kUSB100mAAvailable)
		{
			UInt32			extraPowerAllocated = 0;
			bool			keepExtraPower = false;
			
			USBLog(6, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - Checking to see if our device has 400mA extra", this);
			
			// See if we have an extra 400mA available to make this a 500mA port
			extraPowerAllocated = _portDevice->RequestExtraPower(kUSBPowerDuringWake, (kUSB500mAAvailable - kUSB100mAAvailable) * 2);
			
			if ( extraPowerAllocated < 400 )
			{
				if ( extraPowerAllocated > 0 )
				{
					USBLog(6, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - returning our extraPower because it is not enough (%d)", this, (uint32_t) extraPowerAllocated);
					_portDevice->ReturnExtraPower(kUSBPowerDuringWake, extraPowerAllocated);
				}
			}
			else
			{
				if (_desc.bDeviceClass == kUSBHubClass)
				{
					USBLog(5, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d we might use the extra power for hub device - hanging on to it", this, _portNum);
					_portDevice->setProperty("HubWakePowerReserved", extraPowerAllocated, 32);
					keepExtraPower = true;
				}
				else
				{
					int											numConfigs = _portDevice->GetNumConfigurations();
					int											i;
					const IOUSBConfigurationDescriptor *		cd = NULL;
					
					USBLog(6, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d we have extra port power - seeing if we need it with our %d configs", this, _portNum, numConfigs);
					for (i=0; i < numConfigs; i++)
					{
						cd = _portDevice->GetFullConfigurationDescriptor(i);
						if (cd && (cd->MaxPower > kUSB100mAAvailable))
						{
							USBLog(5, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d we might use the extra power for config[%d] - hanging on to it", this, _portNum, i);
							keepExtraPower = true;
							break;
						}
					}
				}
				
				if (keepExtraPower)
				{
					_usingExtraPortPower = true;
					_portPowerAvailable = kUSB500mAAvailable;
					_portDevice->SetBusPowerAvailable(_portPowerAvailable);
				}
				else
				{
					USBLog(2, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, returning extra power since I don't have a config which needs it", this, _portNum);
					_portDevice->ReturnExtraPower(kUSBPowerDuringWake, extraPowerAllocated);
				}
			}
		}
		
        // register the NUB
		USBLog(5, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, calling registerService for device %s", this, _portNum, _portDevice->getName() );
		USBTrace(kUSBTEnumeration, kTPEnumerationRegisterService, (uintptr_t)this, _portNum, _hub->_locationID, 0);
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
	USBTrace_End(kUSBTEnumeration, kTPEnumerationAddDeviceResetChangeHandler, (uintptr_t)this, _portNum, _hub->_locationID, err);

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
        USBLog(5, "***** AppleUSBHubPort[%p]::HandleResetPortHandler - delaying 100ms workaround", this);
        IOSleep(100);
        _extraResetDelay = false;
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
                USBLog(3, "**1** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d on hub %p - new delay %d", this, (uint32_t)_portNum, _hub, (uint32_t)delay);
            }
			
            // Now wait 10 ms (or 300ms -- see above) after reset
            //
            USBLog(5, "**1** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d on hub %p - delaying %d ms", this, (uint32_t)_portNum, _hub, (uint32_t)delay);
            IOSleep(delay);
            
            // Mac OS 9 state 2
            // macally iKey doesn't tell us until now what the device speed is.
            if (_portStatus.statusFlags & kHubPortLowSpeed)
            {
                _speed = kUSBDeviceSpeedLow;
                USBLog(5, "**2** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d on hub %p - found low speed device", this, (uint32_t)_portNum, _hub);
            }
            else
            {
                if (_portStatus.statusFlags & kHubPortHighSpeed)
                {
                    _speed = kUSBDeviceSpeedHigh;
                    USBLog(5, "**2** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d on hub %p - found high speed device", this, (uint32_t)_portNum, _hub);
                }
                else
                {
                    _speed = kUSBDeviceSpeedFull;
                    USBLog(5, "**2** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d on hub %p - found full speed device", this, (uint32_t)_portNum, _hub);
                }
            }
			
			
            USBLog(5, "**2** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d on hub %p - configuring dev zero", this, (uint32_t)_portNum, _hub);
			// 4693694: configure HS devices with a MPS of 64, which is the spec for those device
            err = DoConfigureDeviceZero(_bus, (kUSBDeviceSpeedHigh == _speed) ? 64 : 8, _speed, _hub->_device->GetAddress(), _portNum);
            
            
            // If our GetDescriptor fails we will clear this flag
            //
            _getDeviceDescriptorFailed = true;
			
            // Now do a device request to find out what it is.  Some fast devices send back packets > 8 bytes to address 0.
            // We will attempt 5 times with a 30ms delay between each (that's  what we do on MacOS 9 )
            //
            USBLog(5, "**3** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d on hub %p - getting dev zero desc", this, (uint32_t)_portNum, _hub);
            err = GetDevZeroDescriptorWithRetries();
            
            if ( err != kIOReturnSuccess )
            {
                USBLog(5, "**3** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d on hub %p - failed to get dev zero desc, detach'ing device", this, (uint32_t)_portNum, _hub);
                _getDeviceDescriptorFailed = true;
                _state = hpsDeadDeviceZero;
				
				// OK, disable the port and try to add the device again
				//
				if ( (err = _hub->ClearPortFeature(kUSBHubPortEnableFeature, _portNum)) )
				{
					USBLog(1, "**3** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d, unable (err = %x) to disable port", this, (uint32_t)_portNum, err);
					USBTrace( kUSBTHubPort,  kTPHubPortHandleResetPortHandler, (uintptr_t)this, _portNum, err, 1);
					FatalError(err, "clearing port feature");
					_bus->ReleaseDeviceZero();
					_devZero = false;
					_portDevice = NULL;
					return err;
				}
				
				_bus->ReleaseDeviceZero();
				_devZero = false;
				_state = hpsSetAddressFailed;
				
				return DetachDevice();
			}
			
            USBLog(5,"**3** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d, using %d for maxPacketSize", this, _portNum, _desc.bMaxPacketSize0);            
        }
		
        // MacOS 9 STATE 4
        
        if(_setAddressFailed > 0)
        {
            // Last time we were here, the following set address failed, so give it some more time
            //
            delay = ((_setAddressFailed) * 30) + (_setAddressFailed * 3);
            USBLog(3, "**4** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d, previous SetAddress failed, sleeping for %d milliseconds", this, (uint32_t)_portNum, (uint32_t)delay);
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
                USBLog(1,"**5** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d of hub @ 0x%x, unable to set device %p to address %d - disabling port", this, _portNum, (uint32_t)_hub->_locationID, _portDevice, _portDevice->GetAddress() );
				USBTrace( kUSBTHubPort,  kTPHubPortHandleResetPortHandler, (uintptr_t)this, _portNum, (uint32_t)_hub->_locationID, _portDevice->GetAddress());
				
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
						IOUSBDevice *cachedDevice = _portDevice;			// in case _portDevice goes away while we are messaging
						
						if (cachedDevice)
						{
							USBLog(5, "AppleUSBHubPort[%p]::HandleResetPortHandler - port %d, Sending kIOUSBMessagePortHasBeenReset message (0x%x)", this, _portNum, err);
							cachedDevice->retain();
							cachedDevice->message(kIOUSBMessagePortHasBeenReset, cachedDevice, &err);
							cachedDevice->release();
						}
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
					IOUSBDevice *cachedDevice = _portDevice;			// in case _portDevice goes away while we are messaging
					
					if (cachedDevice)
					{
						err = kIOReturnNoDevice;
						USBLog(5, "AppleUSBHubPort[%p]::HandleResetPortHandler - port %d, Sending kIOUSBMessagePortHasBeenReset message (0x%x)", this, _portNum, err);
						cachedDevice->retain();
						cachedDevice->message(kIOUSBMessagePortHasBeenReset, cachedDevice, &err);
						cachedDevice->release();
					}
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
                USBLog(3, "**8** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d, sleeping for %d milliseconds", this, (uint32_t)_portNum, (uint32_t)delay);
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
		if ( _attachMessageDisplayed )
		{
			USBError(1,"[%p] The IOUSBFamily has successfully enumerated the device.", this);
			_attachMessageDisplayed = false;
        }
		
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
		IOUSBDevice *cachedDevice = _portDevice;			// in case _portDevice goes away while we are messaging
		
		if (cachedDevice)
		{
			USBLog(5, "AppleUSBHubPort[%p]::HandleResetPortHandler - port %d, Sending kIOUSBMessagePortHasBeenReset message (0x%x)", this, _portNum, err);
			cachedDevice->retain();
			cachedDevice->message(kIOUSBMessagePortHasBeenReset, cachedDevice, &err);
			cachedDevice->release();
		}
    }
    
    SetPortVector(&AppleUSBHubPort::DefaultResetChangeHandler, kHubPortBeingReset);
    USBLog(5, "AppleUSBHubPort[%p]::HandleResetPortHandler - port %d, err = %x, ALL DONE", this, _portNum, err);
    return err;
}



IOReturn
AppleUSBHubPort::HandleSuspendPortHandler(UInt16 changeFlags, UInt16 statusFlags)
{
	IOReturn status = kIOReturnSuccess;
	
    USBLog(5, "AppleUSBHubPort[%p]::HandleSuspendPortHandler for port(%d) _portPMState (%d) changeFlags:(%p) _resumePending: (%s)", this, (int)_portNum, (int)_portPMState, (void*)changeFlags, _resumePending ? "true" : "false");
    SetPortVector(&AppleUSBHubPort::DefaultSuspendChangeHandler, kHubPortSuspend);

	// Spec calls for 10ms of recovery time after a resume
	IOSleep(10);
	
	if (_resumePending)
	{
		// Clear our pending flag
		_resumePending = false;
		if (_lowerPowerStateOnResume)
		{
			USBLog(5, "AppleUSBHubPort[%p]::HandleSuspendPortHandler - RESUME - clearing _lowerPowerStateOnResume - (hub %p port %d)", this, _hub, _portNum);
			_lowerPowerStateOnResume = false;
			 USBLog(6, "AppleUSBHubPort[%p]::HandleSuspendPortHandler - calling LowerPowerState on hub[%p] port %d after clearing _lowerPowerStateOnResume", this, _hub, _portNum);
			_hub->LowerPowerState();
		}
	}
	
	if (_portPMState == usbHPPMS_drvr_suspended)
	{
		// make sure we are still connected before sending a resume message
		if (statusFlags & kHubPortConnection)
		{
			MessageDeviceClients(kIOUSBMessagePortHasBeenResumed, &status, sizeof(IOReturn) );
		}
	}
	else if (_portPMState == usbHPPMS_pm_suspended)
	{
		// this should be taken care of and cleared before we get to the point where we issue an interrupt read
		USBError(1, "AppleUSBHub[%p]::HandleSuspendPortHandler - port(%d) in portPMState(usbHPPMS_pm_suspended) - should not be here!", this, (int)_portNum);
	}

	_portPMState = usbHPPMS_active;
	WakeSuspendCommand( &_portPMState );

    return status;
}

IOReturn 
AppleUSBHubPort::DefaultOverCrntChangeHandler(UInt16 changeFlags, UInt16 statusFlags)
{
    IOUSBHubDescriptor		hubDescriptor;
    bool					individualPortPower = FALSE;
    UInt16					characteristics;
    IOUSBHubPortStatus		portStatus;
    IOReturn				err;
	AbsoluteTime			currentTime;
	UInt64					elapsedTime;

    err = _hub->GetPortStatus(&portStatus, _portNum);

	USBLog(5, "AppleUSBHubPort[%p]::DefaultOverCrntChangeHandler. Port %d (isRootHub: %d), status = 0x%x, change: 0x%x", this,  _portNum, _hub->_isRootHub, portStatus.statusFlags, portStatus.changeFlags );

	if ( (err == kIOReturnSuccess) && (portStatus.changeFlags != 0x1f) )
    {
		// check to see if either the overcurrent status is on or the port power status is off
        if ( (portStatus.statusFlags & kHubPortOverCurrent) || ( ~(portStatus.statusFlags & kHubPortPower) && !_hub->_isRootHub) )
        {
            USBLog(1, "AppleUSBHubPort[%p]::DefaultOverCrntChangeHandler. OverCurrent condition in Port %d", this,  _portNum );
			USBTrace( kUSBTHubPort,  kTPHubPortDefaultOverCrntChangeHandler, (uintptr_t)this, _portNum, 0, 1 );
            hubDescriptor = _hub->GetCachedHubDescriptor();

            characteristics = USBToHostWord(hubDescriptor.characteristics);

            if ( (characteristics & 0x18) == 0x8 )
                individualPortPower = TRUE;

			// Only display the notice once per port object.  The hardware is supposed to disable the port, so there is no need
			// to keep doing it.  Once they unplug the hub, we will get a new port object and this can trigger again
			//
			clock_get_uptime(&currentTime);
			SUB_ABSOLUTETIME(&currentTime, &_overCurrentNoticeTimeStamp );
			absolutetime_to_nanoseconds(currentTime, &elapsedTime);
			elapsedTime /= 1000000000;			 						// Convert to seconds from nanoseconds

            USBLog(5, "AppleUSBHubPort[%p]::DefaultOverCrntChangeHandler. displayedNoticed: %d, time since last: %qd", this,  _overCurrentNoticeDisplayed, elapsedTime );
			if ( !_overCurrentNoticeDisplayed || (elapsedTime > kDisplayOverCurrentTimeout) )
			{
				DisplayOverCurrentNotice( individualPortPower );
				_overCurrentNoticeDisplayed = true;
				clock_get_uptime(&_overCurrentNoticeTimeStamp);
			}
			else
			{
	            USBLog(5, "AppleUSBHubPort[%p]::DefaultOverCrntChangeHandler. not displaying notice because elapsed time %qd is < kDisplayOverCurrentTimeout seconds", this, elapsedTime );
			}
        }
        else
        {
            // the OverCurrent status for this port has changed to zero.
            //
            USBLog(1, "AppleUSBHubPort[%p]::DefaultOverCrntChangeHandler. No OverCurrent condition. Ignoring. Port %d", this, _portNum );
			USBTrace( kUSBTHubPort,  kTPHubPortDefaultOverCrntChangeHandler, (uintptr_t)this, _portNum, 0, 2 );
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
    IOReturn			err = kIOReturnSuccess;
    IOUSBHubPortStatus	status;

	if (!(changeFlags & kHubPortConnection))
	{
		// this is not a serious error if the port has been disconnected
		USBLog(5, "AppleUSBHubPort[%p]::DefaultEnableChangeHandler for port %d, changeFlags: 0x%04x - this is a serious error (Section 11.24.2.7.2)", this, _portNum, changeFlags);
	}

    if ((err = _hub->GetPortStatus(&status, _portNum)))
    {
        FatalError(err, "getting port status (1)");
        return err;
    }

    if (!(status.statusFlags & kHubPortEnabled) && !(changeFlags & kHubPortConnection))
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

    USBLog(5, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler - handling port %d changes (%x,%x).", this, _portNum, statusFlags, changeFlags);
	
	status.statusFlags = statusFlags;
	status.changeFlags = changeFlags;
	
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
			// If we have a connection, then wait for 100ms.  Otherwise, it's a disconnect
			// 
			if (status.statusFlags & kHubPortConnection)
			{
				USBLog(5, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler port (%d) - waiting 100 ms before asserting reset", this, _portNum);
				_connectionChangedState = 2;
				IOSleep(100);
			}
			else
			{
				USBLog(5, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler port (%d) - This is a disconnect", this, _portNum);
				_connectionChangedState = 2;
				_portPMState = usbHPPMS_active;					// reset the PM state if the device gets disconnected
			}
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
        
        if (_portDevice)
        {	
			if ( _ignoreDisconnectOnWakeup )
			{
				USBLog(5, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler - port %d - we are ignoring this disconnect/reconnect", this, _portNum);
				USBLog(1, "IOUSBFamily:  Ignoring a false disconnect after wake for the device %s at 0x%x\n", _portDevice->getName(), (uint32_t)_hub->_locationID);
				USBTrace( kUSBTHubPort,  kTPHubPortDefaultConnectionChangeHandler, (uintptr_t)this, _portNum, (uint32_t)_hub->_locationID, 0);
				break;
			}
			
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
        USBLog(4, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler port %d status(%04x)/change(%04x) - no error from GetPortStatus", this, _portNum, status.statusFlags, status.changeFlags);
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
			USBTrace(kUSBTEnumeration, kTPEnumerationCallAddDevice, (uintptr_t)this, _portNum, _hub->_locationID, 0);
          _state = hpsDeviceZero;
            _connectionChangedState = 9;
            err = AddDevice();
            _connectionChangedState = 10;
        }
        
    } while(false);

	_ignoreDisconnectOnWakeup = false;
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
	USBLog(6, "AppleUSBHubPort[%p]::PortStatusChangedHandlerEntry - calling LowerPowerState and DecrementOutstandingIO on hub[%p] port %d", me, me->_hub, me->_portNum);
	me->_hub->LowerPowerState();								// as far as this thread is concerned, we can lower the state
	me->_hub->DecrementOutstandingIO();							// this will rearm the interrupt read on the last call
    me->release();
}



void 
AppleUSBHubPort::PortStatusChangedHandler(void)
{
    int			which;
    IOReturn	err = kIOReturnSuccess;
    bool		skipOverGetPortStatus = false;
	
	// If we're already processing a status change, then just indicate so and return
    if (!IOLockTryLock(_runLock))
    {
        USBLog(5, "AppleUSBHubPort[%p]::PortStatusChangedHandler: port %d already in PSCH, setting _retryPortStatus to true", this, _portNum);
        _retryPortStatus = true;
        return;
    }
    
	// Need to wait until the init routine is finished
	if ( !IOLockTryLock(_initLock) )
	{
        USBLog(3, "AppleUSBHubPort[%p]::PortStatusChangedHandler: _initLock for port %d @ 0x%x held! Waiting...", this, (uint32_t)_portNum, (uint32_t)_hub->_locationID);
		
		// Block while we wait for the PortInit routine to finish
		IOLockLock(_initLock);
		
        USBLog(3, "AppleUSBHubPort[%p]::PortStatusChangedHandler: _initLock for port %d released!", this, _portNum);
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
            USBLog(5,"AppleUSBHubPort[%p]::PortStatusChangedHandler - port %d - status(0x%04x)/change(0x%04x) - clearing retryPortStatus", this, _portNum, _portStatus.statusFlags, _portStatus.changeFlags);
			USBTrace(kUSBTEnumeration, kTPEnumerationInitialGetPortStatus, (uintptr_t)this, _portNum, _portStatus.statusFlags, _portStatus.changeFlags);
                        
            _retryPortStatus = false;
        }
        
		// If we have a status of connection and connection change (meaning that the device dropped and came back on and we have an errata to tell us to do so, 
		// then do not remove the device -- just ignore the change.  Only do so if it happens withing 10 seconds of a wake
		if ( _hub->_ignoreDisconnectOnWakeup && (_portStatus.statusFlags & kHubPortConnection) && (_portStatus.changeFlags & kHubPortConnection) )
		{
			_ignoreDisconnectOnWakeup = ShouldApplyDisconnectWorkaround();
            USBLog(5,"AppleUSBHubPort[%p]::PortStatusChangedHandler - port %d - ShouldApplyDisconnectWorkaround returned %d", this, _portNum, _ignoreDisconnectOnWakeup);
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
            
            USBLog(5, "AppleUSBHubPort[%p]::PortStatusChangedHandler - port %d - change %d clearing feature 0x%x.", this, (uint32_t)_portNum, which, (uint32_t)_changeHandler[which].clearFeature);
            _statusChangedState = 4;
            if ((err = _hub->ClearPortFeature(_changeHandler[which].clearFeature, _portNum)))
            {
                USBLog(3, "AppleUSBHubPort[%p]::PortStatusChangedHandler - port %d - error %x clearing feature 0x%x.", this, (uint32_t)_portNum, err, (uint32_t)_changeHandler[which].clearFeature);
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
            USBLog(3, "AppleUSBHubPort[%p]::PortStatusChangedHandler: error 0x%x getting port status", this, err);
			
            FatalError(err, "get status (second in port status change)");
            goto errorExit;
        }

        if ( (_portStatus.statusFlags == 0xffff) && (_portStatus.changeFlags == 0xffff) )
        {
            err = kIOReturnNoDevice;
            goto errorExit;
        }

        _statusChangedState = 7;
        USBLog(5, "AppleUSBHubPort[%p]::PortStatusChangedHandler - port %d - status(0x%04x) - change(0x%04x) - before call to (%d) handler function", this, _portNum, _portStatus.statusFlags, _portStatus.changeFlags, which);
            
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
			// 6067316 - kIOReturnNoDevice is sometimes expected, so don't issue a level 1 log in that case (to keep it out of the system log)
            USBLog((kIOReturnNoDevice == err) ? 5 : 1,"AppleUSBHubPort[%p]::PortStatusChangedHandler - port %d of hub @ 0x%x - error %x from (%d) handler", this, _portNum, (uint32_t)_hub->_locationID, err, which);
            break;
        }
    }
    while (true);

errorExit:
	
	if ( _attachMessageDisplayed )
	{
		if ( err != kIOReturnSuccess )
		{
			USBError(1,"[%p] The IOUSBFamily was not able to enumerate a device.", this);
		}
		_attachMessageDisplayed = false;
	}
	
    if ( _devZero )
    {
        // We should disable the port here as well..
        //
        _bus->ReleaseDeviceZero();
        _devZero = false;
    }
        
    USBLog(5,"AppleUSBHubPort[%p]::PortStatusChangedHandler - port %d - err = %x - done, releasing _runLock", this, _portNum, err);
    IOLockUnlock(_runLock);
    IOLockUnlock(_initLock);
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
    if ( thread_call_enter(_portStatusChangedHandlerThread) == TRUE )
	{
		USBLog(3,"AppleUSBHubPort[%p]::StatusChanged -  _portStatusChangedHandlerThread already queued", this);
		_hub->LowerPowerState();
		_hub->DecrementOutstandingIO();
		release();
	}
    
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
    USBLog(5, "AppleUSBHubPort[%p]::ReleaseDevZeroLock devZero = 0x%x", this, _devZero);

    if (_devZero)
    {
        (void) _hub->ClearPortFeature(kUSBHubPortEnableFeature, _portNum);
        
        USBLog(1, "AppleUSBHubPort[%p]::ReleaseDevZeroLock()", this);
		USBTrace( kUSBTHubPort,  kTPHubPortReleaseDevZeroLock, (uintptr_t)this, _portNum, _devZero, 0);

        _state = hpsNormal;
 
        if ( _bus )
            _bus->ReleaseDeviceZero();
            
        _devZero = false;
        IOSleep(300);
        
        // Should we turn the power off and then back on?
    }
    
    return kIOReturnSuccess;
}

IOReturn
AppleUSBHubPort::DetachDevice()
{
    UInt32		delay = 0;
    IOUSBHubPortStatus	status;
    IOReturn 		err = kIOReturnSuccess;
    
    // The port should be disabled and the devZero lock released before we get here
    //
    USBLog(1, "AppleUSBHubPort[%p]::DetachDevice Port %d of hub @ 0x%x being detached", this, _portNum, (uint32_t)_hub->_locationID);
	USBTrace( kUSBTHubPort,  kTPHubPortDetachDevice, (uintptr_t)this, _portNum, _hub->_locationID, 1);

	// If we haven't displayed the error message, do it this once:
	//
	if ( !_attachMessageDisplayed )
	{
        USBError(1,"[%p] The IOUSBFamily is having trouble enumerating a USB device that has been plugged in.  It will keep retrying.  (Port %d of hub @ location: 0x%x)", this, (uint32_t)_portNum, (uint32_t)_hub->_locationID);
		_attachMessageDisplayed = true;
	}
    
	// Increment our number of attach retries and see if we need to power off the port
    //
    _attachRetry++;
    
    if ( _attachRetry % 4 == 0 )
    {
		
		
        // This device is misbehaving a lot, wait  before attempting to enumerate it again:
        //
        delay = _attachRetry * 100;
        
        USBLog(1, "AppleUSBHubPort[%p]::DetachDevice (Port %d of hub @ 0x%x), attachRetry limit reached. delaying for %d milliseconds", this, (uint32_t)_portNum, (uint32_t)_hub->_locationID, (uint32_t)delay);
		USBTrace( kUSBTHubPort,  kTPHubPortDetachDevice, (uintptr_t)this, _portNum, _hub->_locationID, delay );
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
            USBLog(1, "AppleUSBHubPort[%p]::DetachDevice - port %d of hub @ 0x%x - device has gone away", this, _portNum, (uint32_t)_hub->_locationID);
			USBTrace( kUSBTHubPort,  kTPHubPortDetachDevice, (uintptr_t)this, _portNum, _hub->_locationID, kIOReturnNoDevice );
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
			// We have a connection on this port.  Disable the port
			
			_hub->ClearPortFeature(kUSBHubPortEnableFeature, _portNum);
			
			// If the connection change status is set, clear it so we don't go trying to add the device twice.
			//
			if (status.changeFlags & kHubPortConnection)
			{
				USBLog(5, "AppleUSBHubPort[%p]::DetachDevice - port %d - Clearing connection change", this, _portNum);
				_hub->ClearPortFeature(kUSBHubPortConnectionChangeFeature, _portNum);
			}
			
			_retryPortStatus = true;
            SetPortVector(&AppleUSBHubPort::DefaultResetChangeHandler, kHubPortBeingReset);
			
            RemoveDevice();
            
			USBTrace(kUSBTEnumeration, kTPEnumerationCallAddDevice, (uintptr_t)this, _portNum, _hub->_locationID, 0);
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
    UInt32				delay = 30;
    UInt32				retries = 4;
    IOReturn			err = kIOReturnSuccess;
    IOReturn			portStatusErr = kIOReturnSuccess;
    IOUSBHubPortStatus	status;
    
    do 
    {
		bzero(&_desc, sizeof(_desc));
		err = _bus->GetDeviceZeroDescriptor(&_desc, (kUSBDeviceSpeedHigh == _speed) ? 18 : 8); 	// get the first 8 bytes (4693694: 18 for HS device)
        
        // If the error is kIOReturnOverrun, we still received our 8 bytes, so signal no error. 
        //
        if ( err == kIOReturnOverrun )
        {
			USBLog(1, "AppleUSBHubPort[%p]::GetDevZeroDescriptorWithRetries - port %d - GetDeviceZeroDescriptor returned kIOReturnOverrun.  Checking to for valid descripor", this, _portNum);
			USBTrace( kUSBTHubPort,  kTPHubPortGetDevZeroDescriptorWithRetries, (uintptr_t)this, _portNum, kIOReturnOverrun, 1 );
           
			// We need to check that _desc looks like a valiad one
			if ( (_desc.bDescriptorType == kUSBDeviceDesc) && 
				 (_desc.bLength == 18) &&
				 (_desc.bMaxPacketSize0 >= 8 ) &&
				 (_desc.bMaxPacketSize0 <= 64 )
				)
			{
				// OK, let's assume that the rest of the descriptor is OK
				USBLog(1, "AppleUSBHubPort[%p]::GetDevZeroDescriptorWithRetries - port %d - GetDeviceZeroDescriptor returned kIOReturnOverrun.  Descriptor looks valid", this, _portNum);
				USBTrace( kUSBTHubPort,  kTPHubPortGetDevZeroDescriptorWithRetries, (uintptr_t)this, _portNum, kIOReturnSuccess, 2 );
				err = kIOReturnSuccess;
				break;
			}
        }
        
        if ( err )
        {
			USBLog(1, "AppleUSBHubPort[%p]::GetDevZeroDescriptorWithRetries - port %d - GetDeviceZeroDescriptor returned 0x%x", this, _portNum, err);
			USBTrace( kUSBTHubPort,  kTPHubPortGetDevZeroDescriptorWithRetries, (uintptr_t)this, _portNum, err, 3 );

            // Let's make sure that the device is still here.  Maybe it has gone away and we won't process the notification 'cause we're in PSCH
            // Get the PortStatus to see if the device is still there.
            portStatusErr = _hub->GetPortStatus(&status, _portNum);
            if (portStatusErr != kIOReturnSuccess)
            {
				USBLog(1, "AppleUSBHubPort[%p]::GetDevZeroDescriptorWithRetries - port %d - GetPortStatus returned 0x%x", this, _portNum, err);
				USBTrace( kUSBTHubPort,  kTPHubPortGetDevZeroDescriptorWithRetries, (uintptr_t)this, _portNum, portStatusErr, 4 );
                FatalError(err, "getting port status (4)");
                break;
            }
            
            if ( !(status.statusFlags & kHubPortConnection) )
            {
                // We don't have a connection on this port anymore.
                //
                USBLog(1, "AppleUSBHubPort[%p]::GetDevZeroDescriptorWithRetries - port %d - device has gone away", this, _portNum);
				USBTrace( kUSBTHubPort,  kTPHubPortGetDevZeroDescriptorWithRetries, (uintptr_t)this, _portNum, _state, kIOReturnNoDevice );
                _state = hpsDeadDeviceZero;
                err = kIOReturnNoDevice;
                break;
            }
            
			// If the port is suspended, we should unsuspend it and try again
			// ееее
			if ( status.statusFlags & kHubPortSuspend)
			{
                USBLog(1, "AppleUSBHubPort[%p]::GetDevZeroDescriptorWithRetries - port %d - port is suspended", this, _portNum);
				USBTrace( kUSBTHubPort,  kTPHubPortGetDevZeroDescriptorWithRetries, (uintptr_t)this, _portNum, kHubPortSuspend, 5 );

				portStatusErr = _hub->ClearPortFeature(kUSBHubPortSuspendFeature, _portNum);
				if (kIOReturnSuccess != portStatusErr)
				{
					USBLog(3, "AppleUSBHubPort[%p]::GetDevZeroDescriptorWithRetries Could not ClearPortFeature (%d) (kHubPortSuspend): (0x%x)", this, (uint32_t)_portNum, err);
					break;
				}
			}
			
            if ( retries == 2)
                delay = 3;
            else if ( retries == 1 )
                delay = 30;
                
            USBLog(3, "AppleUSBHubPort[%p]::GetDevZeroDescriptorWithRetries - port %d, err: %x - sleeping for %d milliseconds", this, (uint32_t)_portNum, err, (uint32_t)delay);
            IOSleep( delay );
            retries--;
        }
		if ((_hub->_powerStateChangingTo < kIOUSBHubPowerStateLowPower) && (_hub->_powerStateChangingTo != kIOUSBHubPowerStateStable))
		{
			// I am making this a level 1 because it will generally indicate a problem with a device
			USBLog(1, "AppleUSBHubPort[%p]::GetDevZeroDescriptorWithRetries - aborting due to power change", this);
			USBTrace( kUSBTHubPort,  kTPHubPortGetDevZeroDescriptorWithRetries, (uintptr_t)this, _hub->_powerStateChangingTo, 0, 6 );
		}
    }
    while ( err && (retries > 0) && !((_hub->_powerStateChangingTo < kIOUSBHubPowerStateLowPower) && (_hub->_powerStateChangingTo != kIOUSBHubPowerStateStable)));
    
    return err;
}

bool
AppleUSBHubPort::AcquireDeviceZero()
{
    IOReturn 	err = kIOReturnSuccess;
    bool		devZero = false;
    
    err = _bus->AcquireDeviceZero();
	
	USBLog(7, "AppleUSBHubPort[%p]::AcquireDeviceZero (Port %d of hub @ 0x%x) - _bus->AcquireDeviceZero returned 0x%x", this, (uint32_t)_portNum, (uint32_t)_hub->_locationID, err);
   
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
	USBLog(1, "AppleUSBHubPort[%p]::DisplayOverCurrentNotice - port %d - individual: %d", this, _portNum, individual);
	USBTrace( kUSBTHubPort,  kTPHubPortDisplayOverCurrentNotice, (uintptr_t)this, _portNum, individual, 1 );
	
	if ( _hub == NULL || _hub->_device == NULL )
	{
		USBLog(1, "AppleUSBHubPort[%p]::DisplayOverCurrentNotice - _hub (%p) or _hub->_device is NULL", this, _hub);
		USBTrace( kUSBTHubPort,  kTPHubPortDisplayOverCurrentNotice, (uintptr_t)this, (uintptr_t)_hub, 0, 2 );
		return;
	}
	
    if ( individual )
        _hub->_device->DisplayUserNotification(kUSBIndividualOverCurrentNotificationType);
    else
        _hub->_device->DisplayUserNotification(kUSBGangOverCurrentNotificationType);

    return;
}

bool 
AppleUSBHubPort::IsCaptiveOverride(UInt16 vendorID, UInt16 productID)
{
    CaptiveErrataListEntry	*entryPtr;
    UInt32					i, errata = 0;
    
    for(i = 0, entryPtr = gErrataList; i < ERRATALISTLENGTH; i++, entryPtr++)
    {
        if (vendorID == entryPtr->vendorID &&
            productID >= entryPtr->productIDLo &&
            productID <= entryPtr->productIDHi)
        {
            // we match
            return true;
        }
    }

	return false;
}       

bool 
AppleUSBHubPort::ShouldApplyDisconnectWorkaround()
{
	AbsoluteTime	now;
	UInt64			msElapsed;
	bool			returnValue = false;
	
	clock_get_uptime(&now);
	SUB_ABSOLUTETIME(&now, &(_hub->_wakeupTime));
	absolutetime_to_nanoseconds(now, &msElapsed);
	
	// Convert to millisecs
	msElapsed /= 1000000;
	
	USBLog(6, "AppleUSBHubPort[%p]::ShouldApplyDisconnectWorkaround - port %d - time since wake: %qd ms", this, _portNum, msElapsed);
	
	// Only set the workaround if we are within 20 seconds of a wake event
	if (_portDevice && (msElapsed < (20 * 1000)) )
	{
		OSBoolean * deviceHasMassStorageInterfaceRef = OSDynamicCast( OSBoolean, _portDevice->getProperty("kHasMSCInterface") );
		if ( deviceHasMassStorageInterfaceRef && deviceHasMassStorageInterfaceRef->isTrue() )
		{
			USBLog(5, "AppleUSBHubPort[%p]::ShouldApplyDisconnectWorkaround - port %d - we have a disconnect/reconnect on a mass storage device on a hub that tells us to ignore it", this, _portNum);
			returnValue = true;
		}
		else if ( _hub->_isRootHub )
		{
			USBLog(6, "AppleUSBHubPort[%p]::ShouldApplyDisconnectWorkaround - port %d - we have a disconnect/reconnect on a root hub that tells us to ignore it", this, _portNum);
			returnValue = true;
		}
	}
	
	return returnValue;
}

void 
AppleUSBHubPort::WaitForSuspendCommand( void *event, uint64_t timeout )
{
	IOWorkLoop		*myWL = NULL;
	IOCommandGate	*gate = NULL;
	
	if ( _bus )
	{
		myWL = _bus->getWorkLoop();
	
		if ( myWL )
		{
			gate = _bus->GetCommandGate();
		}

		if ( not myWL or not gate )
		{
			USBLog(1, "AppleUSBHubPort[%p]::WaitForSuspendCommand - nil workloop or nil gate !", this);
			USBTrace( kUSBTHubPort,  kTPHubPortWaitForSuspendCommand, (uintptr_t)this, 0, 0, 1 );
			return;
		}
	}
	
	if ( myWL->onThread() )
	{
		USBLog(6, "AppleUSBHubPort[%p]::WaitForSuspendCommand - called on workloop thread !", this);
		USBTrace( kUSBTHubPort,  kTPHubPortWaitForSuspendCommand, (uintptr_t)this, 0, 0, 2 );
	}
	
	if ( myWL->inGate() )
	{	
		if ( gate )
		{
			USBLog(6,"AppleUSBHubPort[%p]::WaitForSuspendCommand calling commandSleep", this);
			
			uint64_t		currentTime = mach_absolute_time();
			AbsoluteTime	deadline;
			uint64_t		elapsedTime = 0;
			
			absolutetime_to_nanoseconds(*(AbsoluteTime *)&currentTime, &elapsedTime);
			// convert timeout to nanoseconds
			currentTime = elapsedTime + timeout*(1000*1000); 
			nanoseconds_to_absolutetime( currentTime, &deadline);
			
			IOReturn kr = gate->commandSleep( event, deadline, THREAD_UNINT );
			
			switch (kr)
			{
				case THREAD_AWAKENED:
					USBLog(6,"AppleUSBHubPort[%p]::WaitForSuspendCommand commandSleep woke up normally (THREAD_AWAKENED) _hub->_myPowerState(%d)", this, (int)_hub->_myPowerState);
					USBTrace( kUSBTHub, kTPHubPortWaitForSuspendCommand, (uintptr_t)this, 0, (uintptr_t)_hub->_myPowerState, 3 );
					break;
					
				case THREAD_TIMED_OUT:
					USBLog(3,"AppleUSBHubPort[%p]::WaitForSuspendCommand commandSleep timed out (THREAD_TIMED_OUT) _hub->_myPowerState(%d)", this, (int)_hub->_myPowerState);
					USBTrace( kUSBTHub, kTPHubPortWaitForSuspendCommand, (uintptr_t)this, 0, (uintptr_t)_hub->_myPowerState, 4 );
					break;
					
				case THREAD_INTERRUPTED:
					USBLog(3,"AppleUSBHubPort[%p]::WaitForSuspendCommand commandSleep was interrupted (THREAD_INTERRUPTED) _hub->_myPowerState(%d)", this, (int)_hub->_myPowerState);
					USBTrace( kUSBTHub, kTPHubPortWaitForSuspendCommand, (uintptr_t)this, 0, (uintptr_t)_hub->_myPowerState, 5 );
					break;
					
				case THREAD_RESTART:
					USBLog(3,"AppleUSBHubPort[%p]::WaitForSuspendCommand commandSleep was restarted (THREAD_RESTART) _hub->_myPowerState(%d)", this, (int)_hub->_myPowerState);
					USBTrace( kUSBTHub, kTPHubPortWaitForSuspendCommand, (uintptr_t)this, 0, (uintptr_t)_hub->_myPowerState, 6 );
					break;
					
				case kIOReturnNotPermitted:
					USBLog(3,"AppleUSBHubPort[%p]::WaitForSuspendCommand commandSleep woke up (kIOReturnNotPermitted) _hub->_myPowerState(%d)", this, (int)_hub->_myPowerState);
					USBTrace( kUSBTHub, kTPHubPortWaitForSuspendCommand, (uintptr_t)this, 0, (uintptr_t)_hub->_myPowerState, 7 );
					break;
					
				default:
					USBLog(3,"AppleUSBHubPort[%p]::WaitForSuspendCommand commandSleep woke up with status (%p) _hub->_myPowerState(%d)", this, (void*)kr, (int)_hub->_myPowerState);
					USBTrace( kUSBTHub, kTPHubPortWaitForSuspendCommand, (uintptr_t)this, kr, (uintptr_t)_hub->_myPowerState, 8 );
			}
		}
	}
	else
	{
		IOSleep(timeout);
	}
}

void 
AppleUSBHubPort::WakeSuspendCommand( void *event )
{
	IOWorkLoop		*myWL = NULL;
	IOCommandGate	*gate = NULL;
	
	if ( _bus )
	{
		myWL = _bus->getWorkLoop();
		
		if ( myWL )
		{
			gate = _bus->GetCommandGate();
		}
		
		if ( not myWL or not gate )
		{
			USBLog(1, "AppleUSBHub[%p]::WaitForSuspendCommand - nil workloop or nil gate !", this);
			USBTrace( kUSBTHubPort,  kTPHubPortWakeSuspendCommand, (uintptr_t)this, 0, 0, 1 );
			return;
		}
	}
	
	USBLog(3,"AppleUSBHubPort[%p]::WakeSuspendCommand  calling commandWakeUp", this);
	if ( gate )
	{
		gate->commandWakeup( event,  true);
	}
	else
	{
		USBLog(1,"AppleUSBHubPort[%p]::WakeSuspendCommand  cannot call commandGate->wakeup because there is no gate", this);
		USBTrace( kUSBTHubPort,  kTPHubPortWakeSuspendCommand, (uintptr_t)this, 0, 0, 2 );
	}
}