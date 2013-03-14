/*
 * Copyright й 1998-2012 Apple Inc.  All rights reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
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
#include <UserNotification/KUNCUserNotifications.h>
#include <libkern/c++/OSMetaClass.h>
#include <libkern/version.h>

#include <IOKit/IOKitKeys.h>
#include <IOKit/usb/USBSpec.h>
#include <IOKit/usb/USBHub.h>
#include <IOKit/usb/IOUSBHubDevice.h>
#include <IOKit/usb/IOUSBControllerV2.h>
#include <IOKit/usb/IOUSBControllerV3.h>
#include <IOKit/usb/IOUSBLog.h>

#include <AssertMacros.h>

#include "AppleUSBHubPort.h"
#include "USBTracepoints.h"

//================================================================================================
//
//   Local Definitions
//
//================================================================================================
//
#define super OSObject

// Comment out if we don't want to remove the power to the ports of a USB hub when we restart
#define REMOVE_PORTPOWER_ON_STOP 1

enum 
{
	kIOWaitTimeBeforeReEnablingPortPowerAfterOvercurrent = 5*1000,		// Time to wait before enabling power to an overcurrent'd port, in ms 
	kWaitTimeToIgnoreDisconnect = 4000,									// Time to ignore a disconnect from a suspect hub, in ms
	kMaxDevZeroRetries = 4
};

//================================================================================================
//
//   Globals (static member variables)
//
//================================================================================================
static CaptiveErrataListEntry  gErrataList[] = {
    { 0x05ac, 0x8000, 0x8FFF },		// All internal range devices
	{ 0x05ac, 0x020e, 0x021c }, 
	{ 0x05ac, 0x0223, 0x0226 },
	{ 0x05ac, 0x0229, 0x022b },
	{ 0x05ac, 0x022f, 0x022f },
	{ 0x05ac, 0x0230, 0x0238 },		// various internal keyboard/trackpad devices
	{ 0x0a5c, 0x4500, 0x4500 },		// Bluetooth Internal Pseudo-Hub
	{ 0x05ac, 0x0201, 0x0206 }, 
	{ 0x05ac, 0x9210, 0x9217 },		// Displays
	
};

#define ERRATALISTLENGTH (sizeof(gErrataList)/sizeof(CaptiveErrataListEntry))

static portStatusChangeVector defaultPortVectors[kNumChangeHandlers] =
{
    { 0, kHubPortOverCurrent,				kUSBHubPortOverCurrentChangeFeature },
	{ 0, kSSHubPortChangeBHResetMask, 		kUSBHubPortBHResetChangeFeature },
    { 0, kHubPortBeingReset, 				kUSBHubPortResetChangeFeature },
	{ 0, kSSHubPortChangePortLinkStateMask, kUSBHubPortLinkStateChangeFeature },
    { 0, kHubPortSuspend,					kUSBHubPortSuspendChangeFeature },
    { 0, kHubPortEnabled,					kUSBHubPortEnableChangeFeature },
    { 0, kHubPortConnection,				kUSBHubPortConnectionChangeFeature },
	{ 0, kSSHubPortChangePortConfigErrMask,	kUSBHubPortConfigErrorChangeFeature }
};


//================================================================================================
//
//   AppleUSBHubPort Methods
//
//================================================================================================
//
OSDefineMetaClassAndStructors(AppleUSBHubPort, OSObject)


//================================================================================================
//
//   init
//
//================================================================================================
IOReturn 
AppleUSBHubPort::init( AppleUSBHub *parent, int portNum, UInt32 powerAvailable, bool captive, bool hasExternalConnector, bool muxed, char *methodName )
{
    _hub						= parent;
    _bus						= parent->_bus;
    _hubDesc					= &parent->_hubDescriptor;
    _portNum					= portNum;
    _portDevice					= NULL;
    _portPowerAvailable			= powerAvailable;
    _captive					= captive;
	_hasExternalConnector		= hasExternalConnector;
    _state						= hpsNormal;
    _retryPortStatus			= false;
    _statusChangedThreadActive	= false;
    _initThreadActive			= false;
	_addDeviceThreadActive		= false;
	_enablePowerAfterOvercurrentThreadActive = false;
    _inCommandSleep				= false;
    _attachRetry				= 0;
	_attachRetryFailed			= false;
    _devZeroCounter				= 0;
	_attachMessageDisplayed		= false;
    _overCurrentNoticeDisplayed = false;
	_portPMState				= usbHPPMS_uninitialized;
	_resumePending				= false;
	_portResumeRecoveryTime		= kPortResumeRecoveryTime;
	_delayOnStatusChange		= false;

    _muxed                      = muxed;
    _portLinkState              = 0xFFFF;           // Initialize to invalid state

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
	
    _addDeviceThread = thread_call_allocate((thread_call_func_t)AddDeviceEntry, (thread_call_param_t)this);
    
    if (!_addDeviceThread)
    {
		USBLog(2,"AppleUSBHubPort[%p]::init Could not allocate the _addDeviceThread", this);
        thread_call_free(_initThread);
        thread_call_free(_portStatusChangedHandlerThread);
        IOLockFree(_runLock);
        IOLockFree(_initLock);
        IOLockFree(_removeDeviceLock);
        return kIOReturnNoMemory;
    }
	
	
    _enablePowerAfterOvercurrentThread = thread_call_allocate((thread_call_func_t)EnablePowerAfterOvercurrentEntry, (thread_call_param_t)this);
    
    if (!_enablePowerAfterOvercurrentThread)
    {
		USBLog(2,"AppleUSBHubPort[%p]::init Could not allocate the _enablePowerAfterOvercurrentThread", this);
        thread_call_free(_initThread);
        thread_call_free(_portStatusChangedHandlerThread);
        thread_call_free(_addDeviceThread);
        IOLockFree(_runLock);
        IOLockFree(_initLock);
        IOLockFree(_removeDeviceLock);
        return kIOReturnNoMemory;
    }
	
    _gate = IOCommandGate::commandGate(this);
	
    if (!_gate)
    {
		USBError(1, "AppleUSBHubPort[%p]::init - unable to create command gate", this);
        return kIOReturnNoMemory;
    }
    
	_workLoop = _hub->getWorkLoop();
    if ( !_workLoop )
    {
        USBError(1, "AppleUSBHubPort[%p]::init Couldn't get provider's workloop", this);
        OSSafeReleaseNULL(_gate);
        return kIOReturnNoMemory;
    }
	
    // Keep a reference to the workloop
    //
    _workLoop->retain();
		
    if ( _workLoop->addEventSource( _gate ) != kIOReturnSuccess )
    {
        USBError(1, "AppleUSBHubPort[%p]::init Couldn't add gate event source", this);
        OSSafeReleaseNULL(_gate);
        OSSafeReleaseNULL(_workLoop);

        return kIOReturnNoMemory;
    }
    
    InitPortVectors();
    
    return kIOReturnSuccess;
}


//================================================================================================
//
//   start
//
//================================================================================================
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


//================================================================================================
//
//   free
//
//================================================================================================
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


//================================================================================================
//
//   stop
//
//================================================================================================
void 
AppleUSBHubPort::stop(void)
{
	IOReturn				err;
	IOUSBControllerV3		*v3Bus = OSDynamicCast(IOUSBControllerV3, _bus);
	
	retain();
	
    USBLog(3, "AppleUSBHubPort[%p]::stop called, _devZero = (%d).", this, _devZero);
	
	
    if ( _statusChangedThreadActive || _initThreadActive || _addDeviceThreadActive || _enablePowerAfterOvercurrentThread)
    {
        UInt32 retries = 0;
		IOWorkLoop *myWL = NULL;
		IOCommandGate *gate = NULL;
		
		USBTrace( kUSBTHubPort,  kTPHubPortStop, (uintptr_t)this, _portNum, _hub->_locationID, 1 );
		if (_bus)
			myWL = _bus->getWorkLoop();
	    
		if (!myWL)
		{
			USBLog(2, "AppleUSBHubPort[%p]::stop called, no workloop.", this);
			USBTrace( kUSBTHubPort,  kTPHubPortStop, (uintptr_t)this, _portNum, _hub->_locationID, 2 );
		}
		else
		{
			gate = _bus->GetCommandGate();
			if (!gate)
			{
				USBLog(2, "AppleUSBHubPort[%p]::stop - i got the WL but there is no gate.", this);
				USBTrace( kUSBTHubPort,  kTPHubPortStop, (uintptr_t)this, _portNum, _hub->_locationID, 3 );
			}
			if (myWL->onThread())
			{
				USBLog(2, "AppleUSBHubPort[%p]::stop - i am on the main thread. DANGER AHEAD.", this);
				USBTrace( kUSBTHubPort,  kTPHubPortStop, (uintptr_t)this, _portNum, _hub->_locationID, 4 );
			}
		}
		
        while ( retries < 600 && ( _statusChangedThreadActive || _initThreadActive || _addDeviceThreadActive || _enablePowerAfterOvercurrentThreadActive) )
        {
			if (!myWL || !gate || myWL->onThread() || !myWL->inGate())
			{
				USBTrace( kUSBTHubPort,  kTPHubPortStop, _portNum, _hub->_locationID, retries, 5 );
				IOSleep( 100 );
			}
			else
			{
				IOReturn kr = kIOReturnSuccess;
				
				USBLog(2, "AppleUSBHubPort[%p]::stop - trying command sleep (%d/%d/%d/%d).", this, _statusChangedThreadActive, _initThreadActive, _addDeviceThreadActive, _enablePowerAfterOvercurrentThreadActive);
				USBTrace( kUSBTHubPort,  kTPHubPortStop, _portNum, _hub->_locationID, (_statusChangedThreadActive<<3 || _initThreadActive<<2 || _addDeviceThreadActive<< 1 || _enablePowerAfterOvercurrentThreadActive), 13 );
				_inCommandSleep = true;
				if (_statusChangedThreadActive)
				{
					kr = gate->commandSleep(&_statusChangedThreadActive);
					if (kr != THREAD_AWAKENED)
					{
						USBLog(5,"AppleUSBHubPort[%p]::stop  _statusChangedThreadActive commandSleep returned %d", this, kr);
						USBTrace( kUSBTHubPort,  kTPHubPortStop, kr, _portNum, _hub->_locationID, 6 );
					}
				}
				else if (_initThreadActive)
				{
					kr = gate->commandSleep(&_initThreadActive);
					if (kr != THREAD_AWAKENED)
					{
						USBLog(5,"AppleUSBHubPort[%p]::stop  _initThreadActive commandSleep returned %d", this, kr);
						USBTrace( kUSBTHubPort,  kTPHubPortStop, kr, _portNum, _hub->_locationID, 7 );
					}
				}
				else if (_addDeviceThreadActive)
				{
					kr = gate->commandSleep(&_addDeviceThreadActive);
					if (kr != THREAD_AWAKENED)
					{
						USBLog(5,"AppleUSBHubPort[%p]::stop  _addDeviceThreadActive commandSleep returned %d", this, kr);
						USBTrace( kUSBTHubPort,  kTPHubPortStop, kr, _portNum, _hub->_locationID, 8 );
					}
				}
				else if (_enablePowerAfterOvercurrentThreadActive)
				{
					kr = gate->commandSleep(&_enablePowerAfterOvercurrentThreadActive);
					if (kr != THREAD_AWAKENED)
					{
						USBLog(5,"AppleUSBHubPort[%p]::stop  _enablePowerAfterOvercurrentThreadActive commandSleep returned %d", this, kr);
						USBTrace( kUSBTHubPort,  kTPHubPortStop, kr, _portNum, _hub->_locationID, 9 );
					}
				}
				_inCommandSleep = false;
				USBLog(2, "AppleUSBHubPort[%p]::stop - returned from command sleep (%d,%d/%d/%d)!!", this, _statusChangedThreadActive, _initThreadActive, _addDeviceThreadActive, _enablePowerAfterOvercurrentThreadActive);
				USBTrace( kUSBTHubPort,  kTPHubPortStop, _portNum, _hub->_locationID, (_statusChangedThreadActive<<3 || _initThreadActive<<2 || _addDeviceThreadActive<< 1 || _enablePowerAfterOvercurrentThreadActive), 10 );
			}
            retries++;
        }
    }
    if ( _statusChangedThreadActive || _initThreadActive || _addDeviceThreadActive || _enablePowerAfterOvercurrentThreadActive)
    {
		USBLog(2, "AppleUSBHubPort[%p]::stop - not quiesced - just returning", this);
		USBTrace( kUSBTHubPort,  kTPHubPortStop, (uintptr_t)this, _portNum, _hub->_locationID, 11 );
		release();
		return;
    }
    
	// OK, now we are going to be stopped
	_isInactive = true;
	
	if (_devZero)
    {
		USBLog(2, "AppleUSBHubPort[%p]::stop - had devZero, releasing", this);
		USBTrace( kUSBTHubPort,  kTPHubPortStop, (uintptr_t)this, _portNum, _hub->_locationID, 12 );
		_bus->ReleaseDeviceZero();
		_devZero = false;
    }

	USBLog(3, "AppleUSBHubPort[%p]::stop - calling RemoveDevice", this);
	RemoveDevice();
	
	if (v3Bus && (v3Bus->IsControllerAvailable()) && !_hub->isInactive() && !_hub->_portSuspended)
	{
#if REMOVE_PORTPOWER_ON_STOP
#if 0
// for some reason dong this prevents drives from being detected as high speed in EFI or another legacy OS 
// (like windows) see rdar://11095057
        if (_hub->_ssHub && _hub->_isRootHub)     // For SS RootHubs
        {
            // for SS Root hub make soft landing from U0->U3 if there was a device connected
            // rdar://10726839
            //
            err = _hub->GetPortStatus(&_portStatus, _portNum);

            if (err == kIOReturnSuccess) 
            {
                if (GETLINKSTATE(_portStatus.statusFlags) == kSSHubPortLinkStateU0)
                {
                    err = _hub->SetPortLinkState(kSSHubPortLinkStateU3, _portNum);
                    if (err != kIOReturnSuccess)
                    {
                        USBLog(5, "AppleUSBHubPort[%p]::stop - Port %d of Hub at 0x%x  SetPortLinkState(kSSHubPortLinkStateU3)", this, _portNum, (uint32_t)_hub->_locationID);
                    }
                }
            }
        }
#endif        
		// now power off the port
		//
		USBLog(5, "AppleUSBHubPort[%p]::stop - removing port power", this);
		if ( (err = _hub->SetPortPower(_portNum, kHubPortPowerOff)) )
		{
			USBLog(1, "AppleUSBHubPort[%p]::stop - err (%p) from ClearPortFeature(kUSBHubPortPowerFeature)", this, (void*)err);
			USBTrace( kUSBTHubPort,  kTPHubPortStop, (uintptr_t)this, err, kUSBHubPortPowerFeature, 0 );
		}
#endif
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
	
    if (_addDeviceThread)
    {
        thread_call_cancel(_addDeviceThread);
        thread_call_free(_addDeviceThread);
        _addDeviceThread = 0;
    }
	
	if (_enablePowerAfterOvercurrentThread)
    {
        thread_call_cancel(_enablePowerAfterOvercurrentThread);
        thread_call_free(_enablePowerAfterOvercurrentThread);
        _enablePowerAfterOvercurrentThread = 0;
    }
	
    if (_gate)
    {
		if (_workLoop)
			_workLoop->removeEventSource(_gate);
	    
		OSSafeReleaseNULL(_gate);
    }
	
    OSSafeReleaseNULL(_workLoop);
    
	release();
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
    
    USBLog(5, "***** AppleUSBHubPort[%p]::PortInit - port %d on hub at 0x%x beginning INIT (getting _initLock)", this, _portNum, (uint32_t) _hub->_locationID);
    _initThreadActive = true;
    IOLockLock(_initLock);
    
    // turn on Power to the port
    USBLog(5, "***** AppleUSBHubPort[%p]::PortInit - port %d on hub at 0x%x enabling port power", this, _portNum, (uint32_t) _hub->_locationID);
    if ((err = _hub->SetPortPower(_portNum, kHubPortPowerOn)))
    {
        USBLog(3, "***** AppleUSBHubPort[%p]::PortInit - port %d on hub at 0x%x could not (err = %x) enable port power", this, _portNum, (uint32_t)_hub->_locationID, err);
		FatalError(err, "setting port power");
        goto errorExit;
    }
	
    // non captive devices will come in through the status change handler
    if (!_captive && !_hub->_treatAllPortsAsCaptive)
    {
        USBLog(5, "***** AppleUSBHubPort[%p]::PortInit - port %d on hub at 0x%x non-captive device - leaving PortInit", this, _portNum, (uint32_t) _hub->_locationID);
        goto errorExit;
    }
	
    // wait for the power on good time
    USBLog(5, "***** AppleUSBHubPort[%p]::PortInit - port %d on hub at 0x%x waiting %d ms for power on", this, _portNum, (uint32_t)_hub->_locationID, _hubDesc->powerOnToGood * 2);
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
    {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHubPort[%p]::PortInit - IOSleep in gate:%d", this, 1);}}
#endif
    IOSleep(_hubDesc->powerOnToGood * 2);
	
    USBLog(5, "***** AppleUSBHubPort[%p]::PortInit - port %d on hub at 0x%x about to get port status #1", this, _portNum, (uint32_t) _hub->_locationID);
    if ((err = _hub->GetPortStatus(&status, _portNum)))
    {
        USBLog(3, "***** AppleUSBHubPort[%p]::PortInit - port %d on hub at 0x%x could not get (err = %x) port status #1", this, _portNum, (uint32_t)_hub->_locationID, err);
        FatalError(err, "getting port status (2)");
        goto errorExit;
    }
	
    USBLog(5, "***** AppleUSBHubPort[%p]::PortInit - port %d on hub at 0x%x - status(%04x), change(%04x) bits detected", this, _portNum, (uint32_t)_hub->_locationID, status.statusFlags, status.changeFlags);
	
    // we now have port status 
    if (status.changeFlags & kHubPortConnection)
    {
        USBLog(5, "***** AppleUSBHubPort[%p]::PortInit - port %d on hub at 0x%x - clearing connection change feature", this, _portNum, (uint32_t) _hub->_locationID);
        if ((err = _hub->ClearPortFeature(kUSBHubPortConnectionChangeFeature, _portNum)))
        {
            USBLog(3, "***** AppleUSBHubPort[%p]::PortInit - port %d on hub at 0x%x could not (err = %x) clear connection change", this, _portNum, (uint32_t)_hub->_locationID, err);
            FatalError(err, "clearing port connection change");
            goto errorExit;
        }
		
        // We should now be in the disconnected state 
        // Do a port request on current port 
        USBLog(5, "***** AppleUSBHubPort[%p]::PortInit - port %d on hub at 0x%x about to get port status #2", this, _portNum, (uint32_t) _hub->_locationID);
        if ((err = _hub->GetPortStatus(&status, _portNum)))
        {
            USBLog(3, "***** AppleUSBHubPort[%p]::PortInit - port %d on hub at 0x%x could not (err = %x) get port status #2", this, _portNum, (uint32_t)_hub->_locationID, err);
            FatalError(err, "getting port status (3)");
            goto errorExit;
        }
    }
	
    if (status.statusFlags & kHubPortConnection)
    {
        // We have a connection on this port
        USBLog(5, "***** AppleUSBHubPort[%p]::PortInit - port %d on hub at 0x%x device detected calling LaunchAddDeviceThread", this, _portNum, (uint32_t) _hub->_locationID);
		USBTrace(kUSBTEnumeration, kTPEnumerationCallAddDevice, (uintptr_t)this, _portNum, _hub->_locationID, 0);
		LaunchAddDeviceThread(false);
    }
	
errorExit:
		
	USBLog(5, "***** AppleUSBHubPort[%p]::PortInit - port %d on hub at 0x%x - done - releasing _initLock", this, _portNum, (uint32_t) _hub->_locationID);
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



void 
AppleUSBHubPort::AddDeviceEntry(OSObject *target, thread_call_param_t issueRemoveDevice)
{
    AppleUSBHubPort 	*me;
    
    if (!target)
    {
        USBLog(5, "AppleUSBHubPort::AddDeviceEntry - no target!");
        return;
    }
    
    me = OSDynamicCast(AppleUSBHubPort, target);
    
    if (!me)
    {
        USBLog(5, "AppleUSBHubPort::AddDeviceEntry - target is not really me!");
        return;
    }
	
    me->AddDevice(issueRemoveDevice);
	USBLog(6, "AppleUSBHubPort[%p]::AddDeviceEntry - calling LowerPowerState and release on hub[%p] port %d", me, me->_hub, me->_portNum);
	me->_hub->LowerPowerState();
	me->_hub->release();
	me->_addDeviceThreadActive = false;
	if (me->_inCommandSleep)
    {
		IOCommandGate *gate = NULL;
		if (me->_bus)
			gate = me->_bus->GetCommandGate();
		if (gate)
		{
			USBLog(2,"AppleUSBHubPort[%p]::AddDeviceEntry -  calling commandWakeup", me);
			gate->commandWakeup(&me->_addDeviceThreadActive, true);
		}
    }
    me->release();
}



void 
AppleUSBHubPort::AddDevice(bool issueRemoveDevice)
{
    IOReturn			err = kIOReturnSuccess;
    bool				checkingForDeadHub = false;
    IOUSBHubPortStatus	status;
	int					i, j;
	bool				resetActive;

    USBLog(5, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub at 0x%x - start (issueRemoveDevice: %d)", this, _portNum, (uint32_t) _hub->_locationID, issueRemoveDevice);
	USBTrace_Start(kUSBTEnumeration, kTPEnumerationAddDevice, (uintptr_t)this, _portNum, _hub->_locationID, 0);
	
	if (issueRemoveDevice)
	{
		USBLog(5, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub at 0x%x - calling RemoveDevice()", this, _portNum, (uint32_t) _hub->_locationID);
		RemoveDevice();
	}
	if (_hub->isInactive() || !_hub->_device || _hub->_device->isInactive())
	{
		USBLog(5, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub at 0x%x - hub[%p] is inactive or hub device[%p] is missing or inactive - aborting AddDevice", this, _portNum, (uint32_t) _hub->_locationID, _hub, _hub->_device);
		return;
	}
	
	// if we are about to change to off, restart, or sleep, then don't do this
	if ((_hub->_powerStateChangingTo < kIOUSBHubPowerStateLowPower) && (_hub->_powerStateChangingTo != kIOUSBHubPowerStateStable))
	{
		USBLog(5, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub at 0x%x - hub[%p] changing to power state[%d] - aborting AddDevice", this, _portNum, (uint32_t) _hub->_locationID, _hub, (int)_hub->_powerStateChangingTo);
		return;
	}
	
	// 7332546 - check to make sure that there is something still connected before we try to reset it
	err = _hub->GetPortStatus(&status, _portNum);
	if (err)
	{
		USBLog(5, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub at 0x%x - hub[%p] err[%p] getting port status - aborting AddDevice", this, _portNum, (uint32_t) _hub->_locationID, _hub, (void*)err);
		return;
	}

	if (!(status.statusFlags & kHubPortConnection))
	{
		USBLog(5, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub at 0x%x - port no longer connected - aborting AddDevice", this, _portNum, (uint32_t) _hub->_locationID);
		return;
	}
	
    do
    {
         // Indicate that we are dealing with device zero, still
        if ( !_devZero )
        {
            USBLog(5, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub at 0x%x - bus %p - acquiring dev zero lock", this, _portNum, (uint32_t)_hub->_locationID, _bus);
            _devZero = AcquireDeviceZero();
            if (!_devZero)
            {
				USBLog(2, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub at 0x%x - bus %p - unable to get devZero lock", this, _portNum, (uint32_t)_hub->_locationID, _bus);
				FatalError(kIOReturnCannotLock, "acquiring device zero");
                break;
            }
			
			// 7332546 - check to make sure that there is something still connected before we try to reset it
			err = _hub->GetPortStatus(&status, _portNum);
			if (err)
			{
				USBLog(5, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub at 0x%x - err[%p] getting port status - aborting AddDevice", this, _portNum, (uint32_t)_hub->_locationID, (void*)err);
				break;
			}
			
			if (!(status.statusFlags & kHubPortConnection))
			{
				USBLog(5, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub at 0x%x - port no longer connected - aborting AddDevice", this, _portNum, (uint32_t)_hub->_locationID);
				err = kIOReturnNotResponding;
				break;
			}
        }
        else
        {
            USBLog(5, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub at 0x%x - bus %p - already owned devZero lock", this, _portNum, (uint32_t)_hub->_locationID, _bus);
        }

		if(_hub->_ssHub && !_hub->IsHSRootHub())	// For external SS Hubs, don't do a reset
		{
			if ( !_getDeviceDescriptorFailed && GETLINKSTATE(status.statusFlags) == kSSHubPortLinkStateU0 )
			{
				USBLog(5, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub at 0x%x - External Superspeed hub, skipping reset", this, (uint32_t)_portNum, (uint32_t)_hub->_locationID);
				_portStatus = status;
				err = CallAddDeviceResetChangeHandlerDirectly(_portStatus.changeFlags, _portStatus.statusFlags);
				break;
			}
			else
			{
				USBLog(5, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub at 0x%x - External Superspeed hub, but link is not in U0 (it is in %s) or _getDeviceDescriptorFailed (%d), so issuing a reset", this, (uint32_t)_portNum, (uint32_t)_hub->_locationID, _hub->LinkStateName(GETLINKSTATE(status.statusFlags)), _getDeviceDescriptorFailed);
			}
		}
		
        SetPortVector(&AppleUSBHubPort::AddDeviceResetChangeHandler, kHubPortBeingReset);

		// we should not have to do any type of delay between now and a reset completion handler
		// however, we used to do so, and so I am doing it here to be compatible with an old code flow
		// we can't just do it at the end of this function like we used to, because as soon as we set
		// the reset bit, it is possible for the hub driver to get a reset change event, since we have
		// been spun off on a separate thread which does not block the interrupt pipe read
		_delayOnStatusChange = true;		// 7294126 - to be more compatible with the old code, make sure we delay 100 ms before we read the status on a status change

		// For a SuperSpeed Root Hub, we want to issue a USB2 reset during our first attempt at enumerating a device.  If we are having trouble enumerating the device, then
		// we shall issue a Warm Reset, which causes the device to retrain the link.  
		if ( _hub->_ssHub && _getDeviceDescriptorFailed )
		{
			USBLog(5, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub at 0x%x - issuing a Warm Reset", this, (uint32_t)_portNum, (uint32_t)_hub->_locationID);
			err = _hub->SetPortFeature(kUSBHubPortBHPortResetFeature, _portNum);
		}
		else
		{
			USBLog(5, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub at 0x%x - resetting port", this, (uint32_t)_portNum, (uint32_t)_hub->_locationID);
			err = _hub->SetPortFeature(kUSBHubPortResetFeature, _portNum);
		}
        
		USBTrace(kUSBTEnumeration, kTPEnumerationResetPort, (uintptr_t)this, _portNum, _hub->_locationID, err);
        if (err)
        {
			if ( err != kIOUSBDeviceNotHighSpeed)
            {
				USBLog(1, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub at 0x%x - unable (err = %x) to reset port (set feature (resetting port)", this, _portNum, (uint32_t)_hub->_locationID, err);
				USBTrace( kUSBTHubPort,  kTPHubPortAddDevice, _portNum, (uintptr_t)_hub, err, 1 );
			}
			
			if (_portDevice)
			{
				USBLog(1,"AppleUSBHubPort: Removing %s from Port %d of Hub at 0x%x", _portDevice->getName(), _portNum, (uint32_t)_hub->_locationID);
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
		
		// 7332546 - make sure we are still connected
		if (!(status.statusFlags & kHubPortConnection))
			err = kIOReturnInternalError;

		while (j++ < 5 && !resetActive && !err)
		{
			i = 0;
			while ((i++ < 50) && !err)
			{
				err = _hub->GetPortStatus(&status, _portNum);
				// 8049002 - if the _devZero lock has been lost, then we must assume that the reset got active and has been processed
				if (!_devZero || (status.statusFlags & kHubPortBeingReset) || (status.changeFlags & kHubPortBeingReset) || (_state == hpsSetAddress))
				{
					if (!_devZero)
					{
						USBLog(1, "AppleUSBHubPort[%p]::AddDevice - in loop (j:%d i:%d) and devZero is false. bailing", this, (int)j, (int)i);
					}
					resetActive = true;
					break;						// inner loop
				}
				// 7332546 - make sure we are still connected
				if (!(status.statusFlags & kHubPortConnection))
				{
					err = kIOReturnInternalError;
					break;
				}
				USBLog(2, "AppleUSBHubPort[%p]::AddDevice - port %d on hub at 0x%x - port not in reset after %d ms", this, _portNum, (uint32_t)_hub->_locationID, i);
				if (i == 50)
				{
					// 8049002 - if the _devZero lock has been lost, then we must assume that the reset got active and has been processed
					if (!_devZero)
					{
						USBLog(1, "AppleUSBHubPort[%p]::AddDevice - got to second SetPortFeature and devZero is false. bailing", this);
						err = kIOReturnInternalError;
						break;						
					}
					
					USBLog(1, "AppleUSBHubPort[%p]::AddDevice - retrying SetPortReset loop (j:%d i:%d), _state: %d !!", this, (int)j, (int)i, _state);
					err = _hub->SetPortFeature(kUSBHubPortResetFeature, _portNum);
				}
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
                {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHubPort[%p]::AddDevice - IOSleep in gate:%d", this, 1);}}
#endif
				IOSleep(10);
			}
		}

		if ( !resetActive)
		{
			USBLog(1, "AppleUSBHubPort[%p]::AddDevice - port %d on hub at 0x%x - port not in reset after 5 retries", this, _portNum, (uint32_t)_hub->_locationID);
			USBTrace( kUSBTHubPort,  kTPHubPortAddDevice, (uintptr_t)this, _portNum, 0, 3 );
			// 7332546 set the err in this case so that we will release the devZero lock
			err = kIOReturnInternalError;
		}
    } while(false);

    if (err && _devZero)
    {
        USBLog(3, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub at 0x%x - got error (%x) - releasing devZero lock", this, _portNum, (uint32_t)_hub->_locationID, err);
        // Need to disable the port before releasing the lock
        //
        
		if (!checkingForDeadHub)
		{
			if ( !_hub->_ssHub && (err = _hub->ClearPortFeature(kUSBHubPortEnableFeature, _portNum)) )
			{
				// If we get an error at this point, it probably means that the hub went away.  Note the error but continue to 
				// release the devZero lock.
				//
				USBLog(5, "***** AppleUSBHubPort[%p]::AddDevice  ClearPortFeature for Port %d returned 0x%x", this, _portNum, err);
				FatalError(err, "clearing port feature (1)");
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

    USBLog(5, "***** AppleUSBHubPort[%p]::AddDevice - port %d on hub at 0x%x - (err = %x) done - (0x%04x, 0x%04x), returning", this, _portNum, (uint32_t)_hub->_locationID, err, status.statusFlags, status.changeFlags);
	USBTrace_End(kUSBTEnumeration, kTPEnumerationAddDevice, (uintptr_t)this, _portNum, _hub->_locationID, err);
}



IOReturn
AppleUSBHubPort::LaunchAddDeviceThread(bool issueRemoveDevice)
{
    IOReturn        err = kIOReturnNotPermitted;
    
    if (_gate && _workLoop)
	{
		IOCommandGate *	gate = _gate;
		IOWorkLoop *	workLoop = _workLoop;
		
		retain();
        _hub->retain();
        workLoop->retain();
		gate->retain();
        
        USBLog(6, "AppleUSBHubPort[%p]::LaunchAddDeviceThread - calling runAction(LaunchAddDeviceThreadGated) for port %d on hub at 0x%x", this, _portNum, (uint32_t) _hub->_locationID);
        err = gate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &AppleUSBHubPort::LaunchAddDeviceThreadGated), (void *)issueRemoveDevice);
        if ( err != kIOReturnSuccess )
        {
            USBLog(2,"AppleUSBHubPort[%p]::LaunchAddDeviceThread _LaunchAddDeviceThreadGated runAction failed (0x%x)", this, err);
        }
        
        gate->release();
        workLoop->release();
        _hub->release();
        release();
    }
    
    return err;
    
}


IOReturn
AppleUSBHubPort::LaunchAddDeviceThreadGated(bool issueRemoveDevice)
{
	if ( IsInactive() )
	{
		USBLog(6, "AppleUSBHubPort[%p]::LaunchAddDeviceThreadGated - port object is inActive() for port %d on hub at 0x%x", this, _portNum, (uint32_t) _hub->_locationID);
		return kIOReturnNotPermitted;
	}
	
    // If we are already doing an AddDevice() bail
    if ( _addDeviceThreadActive )
    {
		USBLog(6, "AppleUSBHubPort[%p]::LaunchAddDeviceThreadGated - already running _addDeviceThreadActive for port %d on hub at 0x%x", this, _portNum, (uint32_t) _hub->_locationID);
        return kIOReturnSuccess;
    }
    
	retain();
	_hub->retain();
	_hub->RaisePowerState();					// keep the power state up while we do this..
	USBLog(6, "AppleUSBHubPort[%p]::LaunchAddDeviceThreadGated - calling AddDeviceThread for port %d on hub at 0x%x, issueRemoveDevice: %d", this, _portNum, (uint32_t) _hub->_locationID, issueRemoveDevice);
	_addDeviceThreadActive = true;
	if (thread_call_enter1(_addDeviceThread, (thread_call_param_t)issueRemoveDevice) == true)
	{
		USBLog(1, "AppleUSBHubPort[%p]::LaunchAddDeviceThreadGated - cannot call out to AddDevice for port %d on hub at 0x%x", this, _portNum, (uint32_t) _hub->_locationID);
		_hub->LowerPowerState();
		_hub->release();
		release();
		_addDeviceThreadActive = false;
	}

	return kIOReturnSuccess;
}


IOReturn
AppleUSBHubPort::CallAddDeviceResetChangeHandlerDirectly(UInt16 portChange, UInt16 portStatus)
{
	IOReturn 	err = kIOReturnSuccess;
	
	// We need this routine for SS Hubs that do not issue a PORT_RESET (as the hub takes care of that when a device is attahced)
	// yet we still need to do the enumeration that is done in AddDeviceResetChangeHandler().  When called as part of a status change,
	// there are some state that is set up in order to prevent a termination from coming in while we are enumerating (which can lead to the
	// object being released.  This method will set up that protection.
	// Need to wait until the init routine is finished

	// If we're already processing a status change, then wait 500 ms for it to clear
	int 	retries = 10;
	
    while (retries > 0 && !IOLockTryLock(_runLock))
    {
        USBLog(5, "AppleUSBHubPort[%p]::CallAddDeviceResetChangeHandlerDirectly: port %d already had _runlock, setting waiting 50ms (retries %d)", this, _portNum, retries);
        retries--;
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
		{if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHubPort[%p]::CallAddDeviceResetChangeHandlerDirectly - IOSleep in gate: %d", this, 1);}}
#endif
		IOSleep(50);
    }
	
	if (retries == 0)
	{
        USBLog(3, "AppleUSBHubPort[%p]::CallAddDeviceResetChangeHandlerDirectly:  port %d @ 0x%x, was not able to get _runLock.  Possible problems ahead", this, (uint32_t)_portNum, (uint32_t)_hub->_locationID);
	}
	
	// Need to wait until the init routine is finished
	if ( !IOLockTryLock(_initLock) )
	{
        USBLog(3, "AppleUSBHubPort[%p]::CallAddDeviceResetChangeHandlerDirectly: _initLock for port %d @ 0x%x held! Waiting...", this, (uint32_t)_portNum, (uint32_t)_hub->_locationID);
		
		// Block while we wait for the PortStatus routine to finish
		IOLockLock(_initLock);
		
        USBLog(3, "AppleUSBHubPort[%p]::CallAddDeviceResetChangeHandlerDirectly: _initLock for port %d released!", this, _portNum);
	}

	USBLog(5, "AppleUSBHubPort[%p]::CallAddDeviceResetChangeHandlerDirectly: port %d obtained _initLock", this, _portNum);
	
    // Indicate that our thread is running
    //
    _statusChangedState = 0;
    _statusChangedThreadActive = true;

	err = AddDeviceResetChangeHandler(portChange, portStatus);
	
	if ( err != kIOReturnSuccess )
	{
		if ( _attachMessageDisplayed )
		{
			{
				USBError(1,"[%p] The IOUSBFamily was not able to enumerate a device.", this);
			}
			_attachMessageDisplayed = false;
		}
		if ( _devZero )
		{
			// We should disable the port here as well..
			//
			USBLog(5,"AppleUSBHubPort[%p]::CallAddDeviceResetChangeHandlerDirectly - port %d - err = %x - done, releasing Dev Zero lock", this, _portNum, err);
			_bus->ReleaseDeviceZero();
			_devZero = false;
		}
	}
	
    USBLog(5,"AppleUSBHubPort[%p]::CallAddDeviceResetChangeHandlerDirectly - port %d - err = %x - done, releasing _runLock", this, _portNum, err);
    
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
			USBLog(3,"AppleUSBHubPort[%p]::CallAddDeviceResetChangeHandlerDirectly -  calling commandWakeup", this);
			gate->commandWakeup(&_statusChangedThreadActive, true);
		}
    }
	
	return err;
}

void 
AppleUSBHubPort::RemoveDevice(void)
{
    bool					ok;
    const IORegistryPlane 	*usbPlane;
    IOUSBDevice				*cachedPortDevice;
	IOUSBControllerV3		*v3Bus = OSDynamicCast(IOUSBControllerV3, _bus);

	USBTrace_Start( kUSBTHubPort,  kTPHubPortRemoveDevice, (uintptr_t)this, _portNum, _hub->_locationID, (uintptr_t)_portDevice);
	
	// synchronize access to the _portDevice and NULL it out
	IOLockLock(_removeDeviceLock);
	cachedPortDevice = _portDevice;
	_portDevice = NULL;
	bzero(&_desc, sizeof(_desc));
	IOLockUnlock(_removeDeviceLock);

    if (cachedPortDevice)
    {
		USBLog(1, "AppleUSBHubPort[%p]::RemoveDevice start (%s) port %d @ 0x%x", this, cachedPortDevice->getName(), (uint32_t)_portNum, (uint32_t)_hub->_locationID);
		usbPlane = cachedPortDevice->getPlane(kIOUSBPlane);
		
		
		// 8051802 -- check the PM assertions needed because of this device being removed
		if (v3Bus)
		{
			v3Bus->CheckPMAssertions(cachedPortDevice, false);
		}
		
		if ( _usingExtraPortPower )
		{
			// Note:  This assumes that _usingExtraPortPower is only set when we have a USB 2.0 device. Will need to rev if we support a USB 3.0 device requesting
			//        extra power.
			USBLog(4, "AppleUSBHubPort[%p]::RemoveDevice  returning _portPowerAvailable to kUSB100mAAvailable", this);
			USBTrace( kUSBTHubPort,  kTPHubPortRemoveDevice, (uintptr_t)this, _portNum, _hub->_locationID, 1);
			_portPowerAvailable = kUSB100mAAvailable;
			_usingExtraPortPower = false;
		}
		
		if ( _hasExternalConnector && _hub->_ssHub && _hub->_isRootHub)
		{
			USBLog(6, "AppleUSBHubPort[%p]::RemoveDevice - Port %d of SS Root Hub at 0x%x on an external connector, increasing unconnected port count on hub", this,  (uint32_t)_portNum, (uint32_t)_hub->_locationID);
			USBTrace( kUSBTHubPort,  kTPHubPortRemoveDevice, (uintptr_t)this, _portNum, _hub->_locationID, 2);
			_hub->_device->UpdateUnconnectedExternalPorts(1);
		}

		if ( usbPlane )
		{
			USBTrace( kUSBTHubPort,  kTPHubPortRemoveDevice, (uintptr_t)this, _portNum, _hub->_locationID, 3);
			cachedPortDevice->detachAll(usbPlane);
		}

		if (_hub && !_hub->isInactive())
		{
			UInt32		retries = 100;
			USBLog(4, "AppleUSBHubPort[%p]::RemoveDevice - hub still active - terminating device[%p] synchronously", this, cachedPortDevice);
			// before issuing a Synchronous terminate, we need to make sure that the device is not busy
			while (cachedPortDevice->getBusyState() && retries--)
			{
				// wait up to 10 seconds for the device to get un-busy
				USBLog(2, "AppleUSBHubPort[%p]::RemoveDevice - device(%p)[%s] busy - waiting 100ms (retries remaining: %d)", this, cachedPortDevice, cachedPortDevice->getName(), (int)retries);
				USBTrace( kUSBTHubPort,  kTPHubPortRemoveDevice, _portNum, _hub->_locationID, retries, 4);
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
                {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHubPort[%p]::RemoveDevice - IOSleep in gate:%d", this, 1);}}
#endif
				IOSleep(100);
			}
			if (cachedPortDevice->getBusyState())
			{
				USBError(1, "AppleUSBHubPort: Port %d of Hub at 0x%x about to terminate a busy device (%s) after waiting 10 seconds", _portNum, (uint32_t)_hub->_locationID, cachedPortDevice->getName());
				USBTrace( kUSBTHubPort,  kTPHubPortRemoveDevice, (uintptr_t)this, _portNum, _hub->_locationID, 5);
			}
			USBTrace( kUSBTHubPort,  kTPHubPortRemoveDevice, (uintptr_t)this, _portNum, _hub->_locationID, 6);
			cachedPortDevice->terminate(kIOServiceRequired | kIOServiceSynchronous);
		}
		else
		{
			USBLog(4, "AppleUSBHubPort[%p]::RemoveDevice - hub no longer active - terminating device[%p] aynchronously", this, cachedPortDevice);
			USBTrace( kUSBTHubPort,  kTPHubPortRemoveDevice, (uintptr_t)this, _portNum, _hub->_locationID, 7);
			cachedPortDevice->terminate(kIOServiceRequired);
		}

		USBLog(4, "AppleUSBHubPort[%p]::RemoveDevice releasing(%p)", this, cachedPortDevice);
		USBTrace( kUSBTHubPort,  kTPHubPortRemoveDevice, _portNum, _hub->_locationID, (uintptr_t)cachedPortDevice, 8);
		cachedPortDevice->release();

    }
	else
	{
        USBLog(3, "AppleUSBHubPort[%p]::RemoveDevice - looks like someone beat us to it", this);
		USBTrace( kUSBTHubPort,  kTPHubPortRemoveDevice, (uintptr_t)this, _portNum, _hub->_locationID, 9);
	}

    InitPortVectors();

	USBTrace_End( kUSBTHubPort,  kTPHubPortRemoveDevice, (uintptr_t)this, _portNum, _hub->_locationID, (uintptr_t)cachedPortDevice);
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
		// Wait for 10ms (the device should be in the suspended state in 10ms) and if you are in gate, make sure you use command sleep with timeout.
		WaitForSuspendCommand( &_portPMState, 10 );
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
	OSBoolean			*expressCardCantWakeRef;
		
    USBLog(5, "AppleUSBHubPort[%p]::SuspendPort(%s) for port %d fromDevice(%s), _resumePending(%d), isInactive(%s)", this, suspend ? "suspend" : "resume", _portNum, fromDevice ? "true" : "false", _resumePending, _hub->isInactive() ? "true" : "false");

	// If there is a resume pending, then wait until it happens
	while ( _resumePending and resumeRetries > 0 )
	{
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
        {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHubPort[%p]::SuspendPort - IOSleep in gate:%d", this, 1);}}
#endif
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
			
			if( HasExpressCardCantWake() || _detectedExpressCardCantWake )
			{
				USBLog(1, "AppleUSBHubPort[%p]::SuspendPort - (%s) has kUSBExpressCardCantWake disabling port", this, _portDevice->getName() );
				
				IOReturn			err = kIOReturnSuccess;
				if ( !_hub->_ssHub &&  (err = _hub->ClearPortFeature(kUSBHubPortEnableFeature, _portNum)) )
				{
					USBLog(1, "AppleUSBHubPort[%p]::SuspendPort - port %d, unable (err = %x) to disable port", this, _portNum, err);
				}
			}
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

bool
AppleUSBHubPort::HasExpressCardCantWake()
{
	bool		result = false;
	OSBoolean	*expressCardCantWakeRef;
 
	if ( _portDevice && _hub->_hubWithExpressCardPort && (_hub->_expressCardPort == _portNum) )
	{
		// if express card can't wake then disable port
		expressCardCantWakeRef = OSDynamicCast( OSBoolean, _portDevice->getProperty(kUSBExpressCardCantWake) );
		if ( expressCardCantWakeRef && expressCardCantWakeRef->isTrue() )
		{
			result = true;
		}
	}
	
	return result;
}

IOReturn
AppleUSBHubPort::ReEnumeratePort(UInt32 options)
{
	if ( IsInactive() )
	{
		USBLog(5,"AppleUSBHubPort[%p]::ReEnumeratePort -- port %d of hub @ 0x%x:  port is inactive",this, (uint32_t)_portNum, (uint32_t)_hub->_locationID);
		return kIOReturnNotPermitted;
	}
	
	retain();
	
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

	// Remove device can be synchronous, so this could be called much later on.  Check to see if we have been stop()'s by now and if so, just bail.
	if ( IsInactive() )
	{
		USBLog(5,"AppleUSBHubPort[%p]::ReEnumeratePort -- port %d of hub @ 0x%x:  port is now inactive",this, (uint32_t)_portNum, (uint32_t)_hub->_locationID);
		release();
		return kIOReturnNotPermitted;
	}
	
	USBLog(6, "AppleUSBHubPort[%p]::ReEnumeratePort - calling LaunchAddDeviceThread for port %d on hub at 0x%x", this, _portNum, (uint32_t) _hub->_locationID);
	_portPMState = usbHPPMS_active;					// reset the PM state if the device gets disconnected
	LaunchAddDeviceThread();
	
	release();
	
	return kIOReturnSuccess;
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
    
    if (multiTTs)
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
	
    err = _hub->DoDeviceRequestWithRetries(&request);
    USBLog(5,"AppleUSBHubPort[%p]::ClearTT -- port %d, options:%X, wValue:%X, wIndex:%X, IOReturn: 0x%x",this, (uint32_t)_portNum, (uint32_t)opts, wValue, wIndex, err);
    
    if ( (err == kIOReturnSuccess) && (endpointType == 0) )	// Control endpoint.
    {
		wValue ^= (1 << 15);	// Flip direction bit and do it again.
		request.wValue = wValue;
		err = _hub->DoDeviceRequestWithRetries(&request);
		USBLog(5,"AppleUSBHubPort[%p]::ClearTT -- do it again for control transactions, wValue:%X, IOReturn: 0x%x",this, wValue, err);
    }
    
    return err;
}

IOReturn 
AppleUSBHubPort::ResetPort()
{
    IOReturn            err = kIOReturnSuccess;
    IOUSBHubPortStatus	status;
    IOUSBHubPortStatus  portStatus;
	bool				wait100ms = false;
    UInt16              linkState;
	
    USBLog(1, "AppleUSBHubPort[%p]::Resetting device %s:  port %d of Hub at 0x%x", this, _portDevice ? _portDevice->getName() : "Unknown" , _portNum, (uint32_t)_hub->_locationID);
	
	if ( ShouldApplyDisconnectWorkaround() )
	{
		USBLog(5, "AppleUSBHubPort[%p]::ResetPort for port %d, will wait 500 ms", this, _portNum);
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
        {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHubPort[%p]::ResetPort - IOSleep in gate:%d", this, 1);}}
#endif
		IOSleep(500);
		wait100ms = true;
	}
	
	// We need to grab the runlock to make sure that we don't interrupt a pending change process
	// If we're already processing a status change, then just indicate so and return
    if (!IOLockTryLock(_runLock))
    {
        USBLog(1, "AppleUSBHubPort[%p]::ResetPort: port %d is processing a status change, returning kIOReturnBusy", this, _portNum);
        _retryPortStatus = true;
        return kIOReturnBusy;
    }
	
	// We don't want to grab the _runlock.  We only wanted to return an error if it was held
    IOLockUnlock(_runLock);
	
    err = _hub->GetPortStatus(&portStatus, _portNum);
    if (_hub->_ssHub)
    {
        linkState = GETLINKSTATE(portStatus.statusFlags);
        USBLog(5, "AppleUSBHubPort[%p]::ResetPort port (%d) - link state: 0x%x (%s)", this, _portNum, linkState, _hub->LinkStateName(linkState));
        if (linkState == kSSHubPortLinkStateU3)
        {
            USBLog(5, "AppleUSBHubPort[%p]::ResetPort port (%d) - unsuspending", this, _portNum);
			(void) _hub->ClearPortFeature(kUSBHubPortSuspendChangeFeature, _portNum);
        }
    }
    else
    {
        USBLog(5, "AppleUSBHubPort[%p]::ResetPort Port %d of hub @ 0x%x (isRootHub: %d), status = 0x%x, change: 0x%x", this,  _portNum, (uint32_t) _hub->_locationID, _hub->_isRootHub, portStatus.statusFlags, portStatus.changeFlags );
    }
	
	int retries = 4;
	err = kIOReturnBusy;
	
    if (_hub->_ssHub)
	{
		// For a super spped port, let's try to reset 5 times.  First 3 will be with WarmPortReset, last 2 with normal USB 2 reset
		retries = 6;
	}
	
	while ( (retries-- > 0) && ( err != kIOReturnSuccess) )
	{
		do {
			UInt16	feature = kUSBHubPortResetFeature;
			
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
			
			
			// OK, set our handler for a reset to the portReset handler and call the
			// hub to actually reset the port
			//
			SetPortVector(&AppleUSBHubPort::HandleResetPortHandler, kHubPortBeingReset);
			
			if (_hub->_ssHub && (retries > 2))
			{
				// For a super spped port, let's try to reset 5 times.  First 3 will be with WarmPortReset, last 2 with normal USB 2 reset
				feature = kUSBHubPortBHPortResetFeature;
			}

			USBLog(5, "AppleUSBHubPort[%p]::ResetPort Port (%d) calling SetPortFeature(%d)  retries: %d)", this, _portNum, feature, retries);
			err = _hub->SetPortFeature(feature, _portNum);
			if (err != kIOReturnSuccess)
			{
				USBLog(3, "AppleUSBHubPort[%p]::ResetPort Could not SetPortFeature(%d) on port (%d): err = (0x%x)", this, _portNum, feature, err);
				
				// Return our vector to the default handler
				//
				SetPortVector(&AppleUSBHubPort::DefaultResetChangeHandler, kHubPortBeingReset);
				break;
			}
			
			if ( wait100ms )
			{
				USBLog(5, "AppleUSBHubPort[%p]::ResetPort for port %d, waiting for 100ms after reset", this, _portNum);
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
				{if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHubPort[%p]::ResetPort - IOSleep in gate:%d", this, 2);}}
#endif
				IOSleep(100);
			}
		} while (false);
		
		if (err == kIOReturnSuccess)
		{
			USBLog(5, "AppleUSBHubPort[%p]::ResetPort for port %d, waiting for reset handler to fire", this, _portNum);
			_resetPending = true;
			_bus->WaitForReleaseDeviceZero();
			USBLog(5, "AppleUSBHubPort[%p]::ResetPort for port %d, devZero lock was released, reset handler must've occurred (_resetPending = %d)", this, _portNum, _resetPending);
			if (_resetPending)
			{
				err = kIOReturnBusy;
				_resetPending = false;
			}
		}
		else if (_devZero)
		{
			_bus->ReleaseDeviceZero();
			_devZero = false;
		}
		
		// If we have a kIOReturnBusy, check to see if the device is still attached (for USB2) or if the link is in RX.Detect (USB3) and if so
		// then bail
		if (err != kIOReturnSuccess)
		{
			IOReturn kr = _hub->GetPortStatus(&portStatus, _portNum);
			if ( kr == kIOReturnSuccess)
			{
				if (_hub->_ssHub)
				{
					UInt16				linkState = GETLINKSTATE(portStatus.statusFlags);
					
					USBLog(6, "AppleUSBHubPort[%p]::ResetPort - Port %d of Hub at 0x%x status: (0x%04x,0x%04x), LinkState: 0x%x (%s state)", this, _portNum, (uint32_t)_hub->_locationID, portStatus.statusFlags, portStatus.changeFlags, linkState, _hub->LinkStateName(linkState));
					
					// If the link state is either of the following, let's wait for (10 ms) and see if it timeouts to another state
					if ( linkState == kSSHubPortLinkStateRxDetect )
					{
						// We are in RX.Detect, so bail
						USBLog(5, "AppleUSBHubPort[%p]::ResetPort - port %d, port in Rx.Detect, so bail", this, _portNum);
						retries = 0;
					}
				}
				else
				{
					// 2.0 hub, check to see if we have something connected
					if ( !(portStatus.statusFlags & kHubPortConnection) )
					{
						// We are don't have a connection, so bailout
						USBLog(5, "AppleUSBHubPort[%p]::ResetPort - port %d, no device attached, so bail", this, _portNum);
						retries = 0;
					}
				}
			}
			else
			{
				// We couldn't get the PortStatus, so bail out.
				USBLog(5, "AppleUSBHubPort[%p]::ResetPort - port %d, could not get PortStatus: 0x%x", this, _portNum, (uint32_t)err);
				retries = 0;
			}
		}
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
	
	// If we have an error, it means that the reset did not complete.  We need to remove this device
	if ( err != kIOReturnSuccess)
	{
		IOReturn kr = _hub->GetPortStatus(&portStatus, _portNum);
		if ( (kr == kIOReturnSuccess) && (portStatus.statusFlags & kHubPortConnection))
		{
			
			USBLog(3, "AppleUSBHubPort[%p]::ResetPort - port %d, we have a reset error (0x%x), but we still have a connection, call RemoveDevice() and LaunchAddDeviceThread() )", this, _portNum, err);
			LaunchAddDeviceThread(true);
		}
	}
	
    USBLog(5, "AppleUSBHubPort[%p]::ResetPort for port %d, returning 0x%x", this, _portNum, (uint32_t)err);
    return err;
}



void 
AppleUSBHubPort::FatalError(IOReturn err, const char *str)
{
    // Don't USBError if we are a HS Root hub and the error is kIOUSBDeviceTransferredToCompanion.  Prevents us from showing false errors in GM builds in the system.log
    //
    if (_hub->IsHSRootHub())
    {
		if (err != kIOUSBDeviceTransferredToCompanion)
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
        USBLog(1,"AppleUSBHubPort[%p]::FatalError - Removing %s from Port %d of Hub at 0x%x", this, _portDevice->getName(), _portNum, (uint32_t)_hub->_locationID);
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
    
    if (v2Bus != 0)
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
	IOReturn			kr = kIOReturnSuccess;

    v2Bus = OSDynamicCast(IOUSBControllerV2, bus);
    
	USBLog(5, "AppleUSBHubPort[%p]::DoConfigureDeviceZero - maxPacketSize: %d, speed: %d, hub: %d, port: %d", v2Bus, maxPacketSize, speed, hub, port);
	
   	if (v2Bus != 0)
    {
        kr = v2Bus->ConfigureDeviceZero(maxPacketSize, speed, hub, port);
    }
    else
    {
        kr = bus->ConfigureDeviceZero(maxPacketSize, speed);
    }
	
	return kr;
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
	IOUSBControllerV3		*v3Bus = OSDynamicCast(IOUSBControllerV3, _bus);
	UInt16					packetSize = 8;
   
    USBLog(5, "***** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d on hub at 0x%x - start - status(0x%04x) change (0x%04x)", this, _portNum, (uint32_t)_hub->_locationID, (int)statusFlags, (int)changeFlags);
	USBTrace_Start(kUSBTEnumeration, kTPEnumerationAddDeviceResetChangeHandler, (uintptr_t)this, _portNum, _hub->_locationID, err);

	_portLinkErrorCount = 0;		// This Gets set to 0 on a reset
	
    if ( _extraResetDelay )
    {
        USBLog(5, "***** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - delaying 100ms workaround", this);
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
        {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - IOSleep in gate:%d", this, 1);}}
#endif
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
                USBLog(5, "**1** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d on hub at 0x%x - leaving (kHubPortBeingReset)", this, _portNum, (uint32_t) _hub->_locationID);
                // we should never be here, just wait for another status change int
                break;
            }
            
            // If the device attached to this port misbehaved last time we tried to enumerate it, let's
            // relax the timing a little bit and give it more time.
            //
            if (_getDeviceDescriptorFailed)
            {
                delay = 300;
                USBLog(3, "**1** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d on hub at 0x%x - new delay %d", this, (uint32_t)_portNum, (uint32_t)_hub->_locationID, (uint32_t)delay);
            }
                
            // Now wait 10 ms (or 300ms -- see above) after reset
            //
            USBLog(5, "**1** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d on hub at 0x%x - delaying %d ms", this, (uint32_t)_portNum, (uint32_t)_hub->_locationID, (uint32_t)delay);
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
            {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - IOSleep in gate:%d", this, 2);}}
#endif
            IOSleep(delay);
            
            // Mac OS 9 state 2
            // macally iKey doesn't tell us until now what the device speed is.
            if (_portStatus.statusFlags & kHubPortLowSpeed)
            {
                _speed = kUSBDeviceSpeedLow;
				packetSize = 8;
                USBLog(5, "**2** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d on hub at 0x%x - found low speed device", this, _portNum, (uint32_t) _hub->_locationID);
            }
            else if (_portStatus.statusFlags & kHubPortHighSpeed)
			{
				_speed = kUSBDeviceSpeedHigh;
				packetSize = 64;
				USBLog(5, "**2** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d on hub at 0x%x - found high speed device", this, _portNum, (uint32_t) _hub->_locationID);
			}
			else if (_hub->_ssHub && _portStatus.statusFlags & kHubPortSuperSpeed)	// Superspeed hub, so we can check our fake super speed bit
			{
				_speed = kUSBDeviceSpeedSuper;
				packetSize = 9;
				USBLog(5, "**2** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d on hub at 0x%x - found super speed device", this, _portNum, (uint32_t) _hub->_locationID);
			}
			else
			{
				_speed = kUSBDeviceSpeedFull;
				packetSize = 8;
				USBLog(5, "**2** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d on hub at 0x%x - found full speed device", this, _portNum, (uint32_t) _hub->_locationID);
					
			}
			
            USBLog(5, "**2** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d on hub at 0x%x - configuring dev zero", this, _portNum, (uint32_t) _hub->_locationID);

            err = DoConfigureDeviceZero(_bus, packetSize, _speed,  _hub->_device->GetAddress(), _portNum);
            if ( err != kIOReturnSuccess )
            {
				if ( err == kIOUSBDeviceCountExceeded)
				{
					USBLog(1, "AppleUSBHubPort[%p]::DoConfigureDeviceZero  Port %d of hub at 0x%x.  Cannot create USB device (kIOUSBDeviceCountExceeded)", this, _portNum, (uint32_t)_hub->_locationID);
					IOLog("The USB stack is not able to enumerate the device at Port %d of hub at 0x%x because the USB hardware ran out of device slots", _portNum, (uint32_t)_hub->_locationID);
					
					// Send a message to the clients of the hub device so software can listen for this error and realize that we ran out of device slots
					_hub->_device->messageClients(kIOUSBMessageDeviceCountExceeded, &_portNum, sizeof(int) );
				}
				
				if ( err == kIOUSBEndpointCountExceeded )
				{
					// The IOLog for this condition was printed earlier.  
					_hub->_device->messageClients(kIOUSBMessageEndpointCountExceeded, NULL, 0 );
				}

				USBLog(5, "**3** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d on hub at 0x%x - failed to create device zero, detach'ing device", this, _portNum, (uint32_t) _hub->_locationID);
                _getDeviceDescriptorFailed = true;
                _state = hpsDeadDeviceZero;
				
				// OK, disable the port and try to add the device again (only for USB2.0)
				//
				if ( !_hub->_ssHub && (err = _hub->ClearPortFeature(kUSBHubPortEnableFeature, _portNum)) )
				{
					USBLog(3, "**3** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, unable (err = %x) to disable port", this, _portNum, err);
					FatalError(err, "clearing port feature (2)");
				}
				
				_bus->ReleaseDeviceZero();
				_devZero = false;
				_portDevice = NULL;
				return err;
			}
            
            // If our GetDescriptor fails we will clear this flag
            //
            _getDeviceDescriptorFailed = true;
  
            // Now do a device request to find out what it is.  Some fast devices send back packets > 8 bytes to address 0.
            // We will attempt 5 times with a 30ms delay between each (that's  what we do on MacOS 9 )
            //
            bzero(&_desc, sizeof(_desc));
            USBLog(5, "**3** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d on hub at 0x%x - getting dev zero desc", this, _portNum, (uint32_t) _hub->_locationID);
            err = GetDevZeroDescriptorWithRetries();
            
            if ( err != kIOReturnSuccess )
            {
                USBLog(5, "**3** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d on hub at 0x%x - failed to get dev zero desc, detach'ing device", this, _portNum, (uint32_t) _hub->_locationID);
                _getDeviceDescriptorFailed = true;
                _state = hpsDeadDeviceZero;
				
				// OK, disable the port and try to add the device again
				//
				if ( !_hub->_ssHub && (err = _hub->ClearPortFeature(kUSBHubPortEnableFeature, _portNum)) )
				{
					USBLog(3, "**3** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, unable (err = %x) to disable port", this, _portNum, err);
					FatalError(err, "clearing port feature (2)");
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
        
        if (_setAddressFailed > 0)
        {
            // Last time we were here, the following set address failed, so give it some more time
            //
            delay = ((_setAddressFailed) * 30) + (_setAddressFailed * 3);
            USBLog(1, "**4** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, previous SetAddress failed, sleeping for %d milliseconds", this, (uint32_t)_portNum, (uint32_t)delay);
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
            {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - IOSleep in gate:%d", this, 3);}}
#endif
            IOSleep(delay);
        }

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
				USBLog(1,"**5** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - Port %d of Hub at 0x%x,  we have a hub, but this would be the 6th hub in the bus, which is illegal.  Erroring out", this, _portNum, (uint32_t)_hub->_locationID );
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
            USBLog(1,"**5** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - Port %d of Hub at 0x%x, unable to set device %p to address %d - disabling port", this, _portNum, (uint32_t)_hub->_locationID, usbDevice, address );
			USBTrace( kUSBTHubPort,  kTPHubPortAddDeviceResetChangeHandler, _portNum, _hub->_locationID, (uintptr_t)usbDevice, address);
 
            // OK, disable the port and try to add the device again
            //
            if ( !_hub->_ssHub && (err = _hub->ClearPortFeature(kUSBHubPortEnableFeature, _portNum)) )
            {
                USBLog(3, "**5** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, unable (err = %x) to disable port", this, _portNum, err);
                FatalError(err, "clearing port feature (3)");
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
			if ( _addDeviceThreadActive)
			{
				USBLog(7, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d of hub @ 0x%x, _addDeviceThreadActive after SetAddress(), before IOSleep(2) ", this, _portNum,  (uint32_t)_hub->_locationID);
			}
			
            // Section 9.2.6.3 of the spec gives the device 2ms to recover from the SetAddress
            IOSleep( 2 );

            // Release devZero lock
            USBLog(5, "**5** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, Releasing DeviceZero after successful SetAddress to %d", this, _portNum, address);
            _bus->ReleaseDeviceZero();
            _devZero = false;
            _state = hpsNormal;
            
        }
        
        if ( _state == hpsDeadDeviceZero )
        {
            _setAddressFailed++;
            USBLog(3, "**6** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, setaddressfailed = %d, disabling port", this, _portNum, _setAddressFailed);
            
            // Note: we are intentionally not changing the value of err below
            //
            if ( !_hub->_ssHub && (err = _hub->ClearPortFeature(kUSBHubPortEnableFeature, _portNum)) )
            {
                USBLog(3, "**5** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, unable (err = %x) to disable port", this, _portNum, err);
            }
            
            if (_devZero)
            {
                USBLog(3, "**6** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, releasing devZero lock", this, _portNum);
                _bus->ReleaseDeviceZero();
                _devZero = false;
            }
            
            USBLog(3, "**7** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, setting state to hpsSetAddressFailed", this, _portNum);
            _state = hpsSetAddressFailed;
            
        }
        else
        {
            // еее Why don't we add the -1 to setAddressFailed?  Don't add it!  It will break
            // the fix for #2652091 (SetAddress failing).
            //
            delay = (_setAddressFailed * 30) + (_setAddressFailed * 3);
            if ( delay )
            {
                USBLog(3, "**8** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, sleeping for %d milliseconds", this, (uint32_t)_portNum, (uint32_t)delay);
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
                {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - IOSleep in gate:%d", this, 4);}}
#endif
                IOSleep(delay);
            }
        }

        if ( (err != kIOReturnSuccess) && (_state != hpsNormal) )
        {
            // An error setting the address, so go back and try resetting again
            //
            USBLog(3, "**9** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, err = %x, disabling port", this, _portNum, err);
            _retryPortStatus = true;
            
            SetPortVector(&AppleUSBHubPort::DefaultResetChangeHandler, kHubPortBeingReset);

            if ( !_hub->_ssHub && (err = _hub->ClearPortFeature(kUSBHubPortEnableFeature, _portNum)) )
            {
                USBLog(3, "**5** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, unable (err = %x) to disable port", this, _portNum, err);
            }
            
            if (_devZero)
            {
                USBLog(3, "**9** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, releasing devZero lock", this, _portNum);
                _bus->ReleaseDeviceZero();
                _devZero = false;
            }

            USBLog(3, "**9** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, delaying 10 ms and calling AddDevice", this, _portNum);
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
            {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - IOSleep in gate:%d", this, 5);}}
#endif
            IOSleep(10); 
			USBLog(6, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - calling LaunchAddDeviceThread for port %d on hub at 0x%x", this, _portNum, (uint32_t) _hub->_locationID);
			LaunchAddDeviceThread(false);
            return kIOReturnSuccess;
        }

        _state = hpsNormal;
        
		err = DoCreateDevice(_bus, usbDevice, address, _desc.bMaxPacketSize0, _speed, _portPowerAvailable, _hub->_device->GetAddress(), _portNum);
        if ( err == kIOReturnSuccess )
        {
			// We will continue to use usbDevice in the rest of this method, but so the _portDevice so that other methods
			// are not changed (rdar://10585970)
			usbDevice->retain();
			
			_portDevice = usbDevice;
			if ( _printConnectIOLog )
			{
				_printConnectIOLog = false;
				IOLog("The USB device %s (Port %d of Hub at 0x%x) may have caused a wake by being connected\n", _portDevice ? _portDevice->getName() : "UNKNOWN", _portNum, (uint32_t)GetHub()->_locationID);
			}
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
			USBLog(3, "**9** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, usbDevice disappeared, cleaning up", this, _portNum);
			_retryPortStatus = true;

			SetPortVector(&AppleUSBHubPort::DefaultResetChangeHandler, kHubPortBeingReset);
            if ( !_hub->_ssHub && (err = _hub->ClearPortFeature(kUSBHubPortEnableFeature, _portNum)) )
            {
                USBLog(3, "**5** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, unable (err = %x) to disable port", this, _portNum, err);
            }

			if (_devZero)
			{
				USBLog(3, "**9** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, releasing devZero lock", this, _portNum);
				_bus->ReleaseDeviceZero();
				_devZero = false;
			}

			USBLog(3, "**9** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, delaying 10 ms and calling AddDevice", this, _portNum);
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
            {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - IOSleep in gate:%d", this, 6);}}
#endif
			IOSleep(10); // ее╩Nine waits for only 1ms
			USBLog(6, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - calling LaunchAddDeviceThread for port %d on hub at 0x%x", this, _portNum, (uint32_t) _hub->_locationID);
			LaunchAddDeviceThread(false);
			return kIOReturnSuccess;
		}
	
        USBLog(5, "**10** AppleUSBHubPort[%p]::AddDeviceResetChangeHandler -  port %d, at addr: %d, Successful", this, _portNum, address);

        _attachRetry = 0;
		_attachRetryFailed	= false;
		if ( _attachMessageDisplayed )
		{
			USBError(1,"[%p] The IOUSBFamily has successfully enumerated the device.", this);
			_attachMessageDisplayed = false;
        }
       
		// Finally use the data gathered to do some final setup for the device
		
        // link the new device as a child of my hub
        usbPlane = usbDevice->getPlane(kIOUSBPlane);
        if ( usbPlane )
		{
            usbDevice->attachToParent( _hub->_device, usbPlane);
		}
		else
		{
			USBLog(1, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - we could not get the kIOUSBPlane!!  Problems ahead", this);
			USBTrace( kUSBTHubPort,  kTPHubPortAddDeviceResetChangeHandler, (uintptr_t)this, (uintptr_t)usbPlane, 0, 2);
		}
        
        // Create properties in the device nub
        usbDevice->setProperty("PortNum",_portNum,32);
        usbDevice->SetProperties();
		
		// Let the nub know who it's HubPolicyMaker is
		usbDevice->SetHubParent(_hub);
		
        if ( IsCaptive() || IsCaptiveOverride(usbDevice->GetVendorID(), usbDevice->GetProductID()) )
            usbDevice->setProperty("non-removable","yes");

		// are we capable of handing out more power..  Note:  This only works for USB 2.0 devices.  Need to rev for USB 3.0, if we ever support that scheme.
		if (_hub->_hasExtraPowerRequest && !_captive && _portPowerAvailable == kUSB100mAAvailable)
		{
			UInt32			extraPowerAllocated = 0;
			bool			keepExtraPower = false;
			SInt32			loops = 3;
			
			while (loops-- > 0 )
			{
				USBLog(6, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - Checking to see if our device has 400mA extra (%d)", this, (uint32_t)loops);
				
				// See if we have an extra 400mA available to make this a 500mA port
				extraPowerAllocated = usbDevice->RequestExtraPower(kUSBPowerDuringWake, (kUSB500mAAvailable - kUSB100mAAvailable) * 2);
				
				if ( extraPowerAllocated < 400 )
				{
					if ( extraPowerAllocated > 0 )
					{
						USBLog(6, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - returning our extraPower because it is not enough (%d)", this, (uint32_t) extraPowerAllocated);
						usbDevice->ReturnExtraPower(kUSBPowerDuringWake, extraPowerAllocated);
						extraPowerAllocated = 0;
					}
					
					// Since we were not able to get our allocation, ask other devices to see if they can give some of it up.   
					USBLog(6, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - calling for other devices to release extra power", this);
					usbDevice->ReturnExtraPower(kUSBPowerRequestWakeRelease, (kUSB500mAAvailable - kUSB100mAAvailable) * 2);
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
                    {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - IOSleep in gate:%d", this, 7);}}
#endif
					IOSleep(100);
				}
				else 
				{
					USBLog(6, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - got the extra power we requested (%d)", this, (uint32_t) extraPowerAllocated);
					break;
				}
			}
			
			if ( extraPowerAllocated >= 400 )
			{
				if (_desc.bDeviceClass == kUSBHubClass)
				{
					USBLog(5, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d we might use the extra power for hub device - hanging on to it", this, _portNum);
					usbDevice->setProperty("HubWakePowerReserved", extraPowerAllocated, 32);
					keepExtraPower = true;
				}
				else
				{
					int											numConfigs = usbDevice->GetNumConfigurations();
					int											i;
					const IOUSBConfigurationDescriptor *		cd = NULL;
					
					USBLog(6, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d we have extra port power - seeing if we need it with our %d configs", this, _portNum, numConfigs);
					for (i=0; i < numConfigs; i++)
					{
						cd = usbDevice->GetFullConfigurationDescriptor(i);
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
					usbDevice->SetBusPowerAvailable(_portPowerAvailable);
				}
				else
				{
					USBLog(2, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, returning extra power since I don't have a config which needs it", this, _portNum);
					usbDevice->ReturnExtraPower(kUSBPowerDuringWake, extraPowerAllocated);
				}
				
				// The RequestExtraPower does not request any power -- it just causes us to send a message to all devices telling them to try to grab more extra power.  This can be handy if
				// we told all devices to relinquish their extra power earlier and then used only part of it.
				USBLog(6, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - calling for other devices to reallocate extra power", this);
				usbDevice->RequestExtraPower(kUSBPowerRequestWakeReallocate, 0);
			}
		}
		
		// 8051802 - register for PM assertions as needed
		if (v3Bus)
		{
			v3Bus->CheckPMAssertions(usbDevice, true);
		}
		
		if ( _hasExternalConnector && _hub->_ssHub && _hub->_isRootHub)
		{
			USBLog(6, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - Port %d of SS Root Hub at 0x%x on an external connector, decreasing unconnected port count on hub", this,  (uint32_t)_portNum, (uint32_t)_hub->_locationID);
			_hub->_device->UpdateUnconnectedExternalPorts(-1);
		}
	
       // register the NUB
		USBLog(1, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - Port %d of Hub at 0x%x (USB Address: %d), calling registerService for device %s", this,  (uint32_t)_portNum, (uint32_t)_hub->_locationID, (uint32_t)address, usbDevice->getName() );
		USBTrace(kUSBTEnumeration, kTPEnumerationRegisterService, (uintptr_t)this, _portNum, _hub->_locationID, 0);
        usbDevice->registerService();
		
		// Detect New Expresscard and reset the _detectedExpressCardCantWake variable.
		//
		if( (_detectedExpressCardCantWake) && (_cachedBadExpressCardVID != usbDevice->GetVendorID() ||	_cachedBadExpressCardPID != usbDevice->GetProductID()) )
		{
			_detectedExpressCardCantWake  = false;
		}
		
		// Now that we are finished, we can release our usbdevice.  Notice that _portDevice might have been NULL'd in the meantime
		usbDevice->release();

    } while(false);

    if (err)
    {
        if (_devZero)
        {
            USBLog(3, "AppleUSBHubPort[%p]::AddDeviceResetChangeHandler - port %d, err = %x, releasing devZero lock", this, _portNum, err);
			if ( !_hub->_ssHub )
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
#pragma unused (changeFlags, statusFlags)
    IOReturn				err = kIOReturnSuccess;
    UInt32					delay = 10;
	UInt16					packetSize = 8;
   
    USBLog(5, "***** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d on hub at 0x%x - start (_resetPending = %d)", this, _portNum, (uint32_t) _hub->_locationID, _resetPending);
    SetPortVector(&AppleUSBHubPort::DefaultResetChangeHandler, kHubPortBeingReset);
 
	_portLinkErrorCount = 0;		// This Gets set to 0 on a reset

	if ( _extraResetDelay )
    {
        USBLog(5, "***** AppleUSBHubPort[%p]::HandleResetPortHandler - delaying 100ms workaround", this);
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
        {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHubPort[%p]::HandleResetPortHandler - IOSleep in gate:%d", this, 1);}}
#endif
        IOSleep(100);
        _extraResetDelay = false;
    }

    do
    {
        if (_state != hpsDeadDeviceZero)
        {
            if ( !_hub->_ssHub && _portStatus.statusFlags & kHubPortBeingReset)
            {
                USBLog(5, "**1** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d on hub at 0x%x - leaving (kHubPortBeingReset)", this, _portNum, (uint32_t) _hub->_locationID);
                
                // we should never be here, just wait for another status change int
                // OK, set our handler for a reset to the portReset handler
                //
                SetPortVector(&AppleUSBHubPort::HandleResetPortHandler, kHubPortBeingReset);
				_resetPending = false;
                err = kIOReturnSuccess;
				break;
            }
            
			if (changeFlags & kHubPortConnection)
            {
                // We have a connection change after the reset.  That means that the device probably disconnected and reconnected, so it is now unconfigured.  We can then
				// release the devZero lock and indicate that we have no resetPending anymore (which will avoid retries from ResetPort).  We will then handle the device again
				// when we proces the connection change (disconnect).
                //
                USBLog(5, "AppleUSBHubPort[%p]::HandleResetPortHandler - port %d - device appears to have gone away and then come back", this, _portNum);
  				_resetPending = false;
              	err = kIOReturnSuccess;
				_bus->ReleaseDeviceZero();
				_devZero = false;
              	_state = hpsDeadDeviceZero;
				break;
            }
			
			// If the device attached to this port misbehaved last time we tried to enumerate it, let's
            // relax the timing a little bit and give it more time.
            //
            if (_getDeviceDescriptorFailed)
            {
                delay = 300;
                USBLog(3, "**1** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d on hub at 0x%x - new delay %d", this, (uint32_t)_portNum, (uint32_t)_hub->_locationID, (uint32_t)delay);
            }
			
            // Now wait 10 ms (or 300ms -- see above) after reset
            //
            USBLog(5, "**1** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d on hub at 0x%x - delaying %d ms", this, (uint32_t)_portNum, (uint32_t)_hub->_locationID, (uint32_t)delay);
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
            {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHubPort[%p]::HandleResetPortHandler - IOSleep in gate:%d", this, 2);}}
#endif
            IOSleep(delay);
            
            // Mac OS 9 state 2
            // macally iKey doesn't tell us until now what the device speed is.
            if (_portStatus.statusFlags & kHubPortLowSpeed)
            {
                _speed = kUSBDeviceSpeedLow;
				packetSize = 8;
                USBLog(5, "**2** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d on hub at 0x%x - found low speed device", this, (uint32_t)_portNum, (uint32_t) _hub->_locationID);
            }
            else if (_portStatus.statusFlags & kHubPortHighSpeed)
			{
				_speed = kUSBDeviceSpeedHigh;
				packetSize = 64;
				USBLog(5, "**2** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d on hub at 0x%x - found high speed device", this, _portNum, (uint32_t) _hub->_locationID);
			}
			else if (_hub->_ssHub && _portStatus.statusFlags & kHubPortSuperSpeed)	// Superspeed hub, so we can check our fake super speed bit
			{
				_speed = kUSBDeviceSpeedSuper;
				packetSize = 9;
				USBLog(5, "**2** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d on hub at 0x%x - found super speed device", this, _portNum, (uint32_t) _hub->_locationID);
			}
			else
			{
				_speed = kUSBDeviceSpeedFull;
				packetSize = 8;
				USBLog(5, "**2** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d on hub at 0x%x - found full speed device", this, _portNum, (uint32_t) _hub->_locationID);
			}			
			
            USBLog(5, "**2** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d on hub at 0x%x - configuring dev zero", this, (uint32_t)_portNum, (uint32_t) _hub->_locationID);
			// 4693694: configure HS devices with a MPS of 64, which is the spec for those device
            err = DoConfigureDeviceZero(_bus, packetSize, _speed, _hub->_device->GetAddress(), _portNum);
            
            
            // If our GetDescriptor fails we will clear this flag
            //
            _getDeviceDescriptorFailed = true;
			
            // Now do a device request to find out what it is.  Some fast devices send back packets > 8 bytes to address 0.
            // We will attempt 5 times with a 30ms delay between each (that's  what we do on MacOS 9 )
            //
            USBLog(5, "**3** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d on hub at 0x%x - getting dev zero desc", this, (uint32_t)_portNum, (uint32_t) _hub->_locationID);
            err = GetDevZeroDescriptorWithRetries();
            
            if ( err != kIOReturnSuccess )
            {
                USBLog(5, "**3** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d on hub at 0x%x - failed to get dev zero desc, detach'ing device", this, (uint32_t)_portNum, (uint32_t) _hub->_locationID);
                _getDeviceDescriptorFailed = true;
                _state = hpsDeadDeviceZero;
				
				// OK, disable the port and try to add the device again
				//
				if ( !_hub->_ssHub && (err = _hub->ClearPortFeature(kUSBHubPortEnableFeature, _portNum)) )
				{
					USBLog(1, "**3** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d, unable (err = %x) to disable port", this, (uint32_t)_portNum, err);
					USBTrace( kUSBTHubPort,  kTPHubPortHandleResetPortHandler, (uintptr_t)this, _portNum, err, 1);
					FatalError(err, "clearing port feature (4)");
					_bus->ReleaseDeviceZero();
					_devZero = false;
					_portDevice = NULL;
					return err;
				}
				
				_bus->ReleaseDeviceZero();
				_devZero = false;
				_state = hpsSetAddressFailed;
				
				// Do not call DetachDevice now.  Let the ResetPort() code decide when to do that.
				return err;
			}
			
            USBLog(5,"**3** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d, using %d for maxPacketSize", this, _portNum, _desc.bMaxPacketSize0);            
        }
		
        // MacOS 9 STATE 4
        
        if (_setAddressFailed > 0)
        {
            // Last time we were here, the following set address failed, so give it some more time
            //
            delay = ((_setAddressFailed) * 30) + (_setAddressFailed * 3);
            USBLog(3, "**4** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d, previous SetAddress failed, sleeping for %d milliseconds", this, (uint32_t)_portNum, (uint32_t)delay);
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
            {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHubPort[%p]::HandleResetPortHandler - IOSleep in gate:%d", this, 3);}}
#endif
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
                USBLog(1,"**5** AppleUSBHubPort[%p]::HandleResetPortHandler - Port %d of Hub at 0x%x, unable to set device %p to address %d - disabling port", this, _portNum, (uint32_t)_hub->_locationID, _portDevice, _portDevice->GetAddress() );
				USBTrace( kUSBTHubPort,  kTPHubPortHandleResetPortHandler, (uintptr_t)this, _portNum, (uint32_t)_hub->_locationID, _portDevice->GetAddress());
				
                // OK, disable the port and try to add the device again
                //
                if ( !_hub->_ssHub && (err = _hub->ClearPortFeature(kUSBHubPortEnableFeature, _portNum)) )
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
                    FatalError(err, "clearing port feature (5)");
					
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
                
				// Do not call DetachDevice now.  Let the ResetPort() code decide when to do that.
				return err;
                
            }
            else
            {	
                USBLog(5, "AppleUSBHubPort[%p]::HandleResetPortHandler - now check device address", this);

                IOUSBControllerV3* bus3 = OSDynamicCast(IOUSBControllerV3, _bus);
                USBLog(5, "AppleUSBHubPort[%p]::HandleResetPortHandler - now check device address2", this);
       
                if (bus3)
                {
                    USBDeviceAddress latestDeviceAddress = 0;
                    
					IOReturn kr = bus3->GetActualDeviceAddress(_portDevice->GetAddress(), &latestDeviceAddress);
                    
                    if (kr == kIOReturnSuccess)
                    {
						USBLog(5, "AppleUSBHubPort[%p]::HandleResetPortHandler - setting device %d address to address: %d", this, _portDevice->GetAddress(), latestDeviceAddress);
                        _portDevice->SetAddress(latestDeviceAddress);
                    }
                    else
                    {
                        USBLog(5, "AppleUSBHubPort[%p]::HandleResetPortHandler - GetActualDeviceAddress returned: 0x%x", this, (uint32_t)kr);
                    }
                }
                else
                {
                    USBLog(5, "AppleUSBHubPort[%p]::HandleResetPortHandler - Didn't get bus3", this);
                }
                
                // Section 9.2.6.3 of the spec gives the device 2ms to recover from the SetAddress
                IOSleep( 2 );
				
                // Release devZero lock
                USBLog(5, "**5** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d, Releasing DeviceZero after successful SetAddress", this, _portNum);
				_resetPending = false;
               _bus->ReleaseDeviceZero();
                _devZero = false;
                _state = hpsNormal;
            }
        }
        
        // MacOS 9 STATE 6
        
        if ( _state == hpsDeadDeviceZero )
        {
            _setAddressFailed++;
            USBLog(3, "**6** AppleUSBHubPort[%p]::HandleResetPortHandler - port %d, setaddressfailed = %d, disabling port", this, _portNum, _setAddressFailed);
            
            // Note: we are intentionally not changing the value of err below
            //
			if (!_hub->_ssHub)
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
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
                {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHubPort[%p]::HandleResetPortHandler - IOSleep in gate:%d", this, 4);}}
#endif
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
				_resetPending = true;
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
		_attachRetryFailed	= false;
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
            if (!_hub->_ssHub)
				_hub->ClearPortFeature(kUSBHubPortEnableFeature, _portNum);
			_resetPending = true;
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
#pragma unused (changeFlags)
	
	IOReturn	status = kIOReturnSuccess;
	Boolean		fromResume = _resumePending;
	
    USBLog(5, "AppleUSBHubPort[%p]::HandleSuspendPortHandler for port(%d) _portPMState (%d) changeFlags:(%p) _resumePending: (%s)", this, (int)_portNum, (int)_portPMState, (void*)changeFlags, _resumePending ? "true" : "false");
    SetPortVector(&AppleUSBHubPort::DefaultSuspendChangeHandler, kHubPortSuspend);

	// Spec calls for 10ms of recovery time after a resume
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
    {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHubPort[%p]::HandleSuspendPortHandler - IOSleep in gate:%d", this, 1);}}
#endif
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
			USBLog(5, "AppleUSBHubPort[%p]::HandleSuspendPortHandler _suspendChangeAlreadyLogged: %s", this, _suspendChangeAlreadyLogged ? "true" : "false");
			if (!fromResume && !_suspendChangeAlreadyLogged)
			{
				// make a friendly kernel log message, but only if probably associated with a wakeup
				UInt64 wakeTime;
				
				// First have wee seen a wake, _wakeupTime is zeroed at Init.
				absolutetime_to_nanoseconds(_hub->_wakeupTime, &wakeTime);
				if (wakeTime != 0)
				{	
					// We have seen a wake, how long is that since wake
					AbsoluteTime	now;
					UInt64			msElapsed;
					UInt32			msAfterWake = 0;
					
					clock_get_uptime(&now);
					SUB_ABSOLUTETIME(&now, &(_hub->_wakeupTime));
					absolutetime_to_nanoseconds(now, &msElapsed);
					
					// Convert to millisecs
					msElapsed /= 1000000;
#if DEBUG_LEVEL != 0
					msAfterWake = 5000;
#else
					msAfterWake = 1000;
#endif
					if (msElapsed < msAfterWake)	// Within msAfterWake sec of a wake seems like a reasonable value
					{
						IOLog("The USB device %s (Port %d of Hub at 0x%x) may have caused a wake by issuing a remote wakeup (1)\n", _portDevice ? _portDevice->getName() : "Unknown", _portNum, (uint32_t)GetHub()->_locationID);
					}
					USBLog(5, "AppleUSBHubPort[%p]::HandleSuspendPortHandler  Port %d of Hub at 0x%x - device issued remote wakeup, may be wake reason (%d ms since wake)", this, _portNum, (uint32_t)GetHub()->_locationID, (uint32_t)msElapsed);
				}
				else
				{
					USBLog(5, "AppleUSBHubPort[%p]::HandleSuspendPortHandler wakeTime hi:lo %lx:%lx", this, (long unsigned)(wakeTime >> 32), (long unsigned)(wakeTime &0xffffffff));
				}
			}
			USBLog(5, "AppleUSBHubPort[%p]::HandleSuspendPortHandler finish", this);
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

#pragma mark Default Vectors

IOReturn 
AppleUSBHubPort::DefaultOverCrntChangeHandler(UInt16 changeFlags, UInt16 statusFlags)
{
#pragma unused (changeFlags, statusFlags)
    IOUSB3HubDescriptor		hubDescriptor;
    bool					individualPortPower = FALSE;
    UInt16					characteristics;
    IOUSBHubPortStatus		portStatus;
    IOReturn				err;
	AbsoluteTime			currentTime;
	UInt64					elapsedTime;
	bool					displayDialog = true;
	

    err = _hub->GetPortStatus(&portStatus, _portNum);

	USBLog(5, "AppleUSBHubPort[%p]::DefaultOverCrntChangeHandler. Port %d of hub @ 0x%x (isRootHub: %d), status = 0x%x, change: 0x%x", this,  _portNum, (uint32_t) _hub->_locationID, _hub->_isRootHub, portStatus.statusFlags, portStatus.changeFlags );

	// The check for 0x1f is just in case we are getting a bogus change where all bits are ON
	if ( (err == kIOReturnSuccess) && (portStatus.changeFlags != 0x1f) )
    {
		// check to see if either the overcurrent status is on or the port power status is off
        if ( (portStatus.statusFlags & kHubPortOverCurrent) || ( ~(portStatus.statusFlags & kHubPortPower) && !_hub->_isRootHub) )
        {
            USBLog(1, "AppleUSBHubPort[%p]::DefaultOverCrntChangeHandler. OverCurrent condition in Port %d of hub @ 0x%x", this,  _portNum, (uint32_t)_hub->_locationID);
			USBTrace( kUSBTHubPort,  kTPHubPortDefaultOverCrntChangeHandler, (uintptr_t)this, _portNum, (uint32_t) _hub->_locationID, 1 );
            hubDescriptor = _hub->GetCachedHubDescriptor();

            characteristics = USBToHostWord(hubDescriptor.characteristics);

            if ( (characteristics & 0x18) == 0x8 )
                individualPortPower = TRUE;

			// For SuperSpeed Controllers we have 2 root hubs (one for SS, one for HS/FS/LS).  We only want to display the dialog one but
			// both are probably reporting the overcurrent.  We also do need to enable the port for both root hubs, so we can't just
			// bypass reporting the overcurrent.  So, what we do is use a bit of the status flags (that we have already overloaded to report
			// that the port is being debounced), kHubPortDebouncing, to indicate that we should NOT display the dialog.  That bit should only be set
			// in this specific case.
			if ( _hub->_ssHub && _hub->_isRootHub && (portStatus.statusFlags & kHubPortDebouncing) )
			{
				USBLog(5, "AppleUSBHubPort[%p]::DefaultOverCrntChangeHandler. Port %d of hub @ 0x%x will not display dialog", this,  _portNum, (uint32_t)_hub->_locationID);
				displayDialog = false;
			}

			// Only display the notice once per port object.  The hardware is supposed to disable the port, so there is no need
			// to keep doing it.  Once they unplug the hub, we will get a new port object and this can trigger again
			//
			clock_get_uptime(&currentTime);
			SUB_ABSOLUTETIME(&currentTime, &_overCurrentNoticeTimeStamp );
			absolutetime_to_nanoseconds(currentTime, &elapsedTime);
			elapsedTime /= 1000000000;			 						// Convert to seconds from nanoseconds

            USBLog(5, "AppleUSBHubPort[%p]::DefaultOverCrntChangeHandler. displayedNoticed: %d, time since last: %qd", this,  _overCurrentNoticeDisplayed, elapsedTime );
			if ( displayDialog && (!_overCurrentNoticeDisplayed || (elapsedTime > kDisplayOverCurrentTimeout)) )
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
		
		// Check to see if we have to re-enable the port power
		retain();
		_hub->retain();
		_hub->RaisePowerState();					// keep the power state up while we do this..
		USBLog(6, "AppleUSBHubPort[%p]::DefaultOverCrntChangeHandler - calling EnablePowerAfterOvercurrentThread for port %d on hub @ 0x%x", this, _portNum, (uint32_t) _hub->_locationID);
		_enablePowerAfterOvercurrentThreadActive = true;
		if (thread_call_enter(_enablePowerAfterOvercurrentThread) == true)
		{
			USBLog(1, "AppleUSBHubPort[%p]::DefaultOverCrntChangeHandler - cannot call out to EnablePowerAfterOvercurrentThread for port %d on hub at 0x%x", this, _portNum, (uint32_t) _hub->_locationID);
			USBTrace( kUSBTHubPort,  kTPHubPortDefaultOverCrntChangeHandler, (uintptr_t)this, _portNum, (uint32_t) _hub->_locationID, 3 );
			_hub->LowerPowerState();
			_hub->release();
			release();
			_enablePowerAfterOvercurrentThreadActive = false;
		}
		
    }
    
    return err;
}


IOReturn 
AppleUSBHubPort::DefaultResetChangeHandler(UInt16 changeFlags, UInt16 statusFlags)
{
#pragma unused (changeFlags, statusFlags)
    USBLog(5, "AppleUSBHubPort[%p]::DefaultResetChangeHandler for port %d returning kIOReturnSuccess", this, _portNum);
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBHubPort::DefaultSuspendChangeHandler(UInt16 changeFlags, UInt16 statusFlags)
{
#pragma unused (changeFlags, statusFlags)
    USBLog(5, "AppleUSBHubPort[%p]::DefaultSuspendChangeHandler for port %d returning kIOReturnSuccess", this, _portNum);
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBHubPort::DefaultEnableChangeHandler(UInt16 changeFlags, UInt16 statusFlags)
{
#pragma unused (statusFlags)
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
    IOReturn			err = kIOReturnSuccess;
    IOUSBHubPortStatus	status;
    UInt16              linkState;
	
    USBLog(5, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler - handling port %d changes (0x%04x,0x%04x)", this, _portNum, statusFlags, changeFlags);
    
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
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
            {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler - IOSleep in gate:%d", this, 1);}}
#endif
            IOSleep(300);
			if (!(status.statusFlags & kHubPortConnection))
			{
				USBLog(5, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler port (%d) - This is a disconnect", this, _portNum);
				_connectionChangedState = 2;
				_portPMState = usbHPPMS_active;					// reset the PM state if the device gets disconnected
				
				_attachRetry = 0;
				_attachRetryFailed	= false;
			}
        }
        else
        {
			// If we have a connection, then wait for 100ms.  Otherwise, it's a disconnect
			// 
			if (status.statusFlags & kHubPortConnection)
			{
				USBLog(5, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler port (%d) - waiting 100 ms before asserting reset", this, _portNum);
				_connectionChangedState = 2;
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
                {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler - IOSleep in gate:%d", this, 2);}}
#endif
				IOSleep(100);
			}
			else
			{
				// If this is a super speed hub, check for the Link State. 
				if (_hub->_ssHub)
				{
					linkState = GETLINKSTATE(statusFlags);
					USBLog(5, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler port (%d) - no connection, link state: 0x%x (%s)", this, _portNum, linkState, _hub->LinkStateName(linkState));
				}
				{
					USBLog(5, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler port (%d) - This is a disconnect", this, _portNum);
					_connectionChangedState = 2;
					_portPMState = usbHPPMS_active;					// reset the PM state if the device gets disconnected
					
					_attachRetry = 0;
					_attachRetryFailed	= false;
				}
			}
        }
        
        // If we get to here, there was a connection change
        // if we already have a device it must have been disconnected
        // at sometime. We should kill it before servicing a connect event
        
        // If we're still in hpsDeviceZero, it means that we are haven't addressed the device, so we need to release the devZero lock
        //
        if ( _devZero )
        {
			// If we have _resetPending true, it means that we received this disconnect while we were trying to do a reset.  In that case, set _resetPending to false
			// and let the ResetPort() then finish w/out an error.
			if ( _resetPending )
			{
				USBLog(5, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler - port %d - _resetPending was true but the device went away, so let it finish w/out error", this, _portNum);
				_resetPending = false;
			}
			
            USBLog(5, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler - port %d - releasing devZero lock", this, _portNum);
            // _state = hpsNormal;
            _connectionChangedState = 3;
            _bus->ReleaseDeviceZero();
            _devZero = false;
        }
        
        if (_portDevice)
        {	
			_hadResumePendingAndDisconnect = false;
			
			if ( _ignoreDisconnectOnWakeup )
			{
				USBLog(5, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler - port %d - we are ignoring this disconnect/reconnect", this, _portNum);
				USBLog(1, "IOUSBFamily:  Ignoring a false disconnect after wake for the device %s at 0x%x\n", _portDevice->getName(), (uint32_t)_hub->_locationID);
				USBTrace( kUSBTHubPort,  kTPHubPortDefaultConnectionChangeHandler, (uintptr_t)this, _portNum, (uint32_t)_hub->_locationID, 0);
				break;
			}
            _connectionChangedState = 4;
			_portPMState = usbHPPMS_active;						// 7967676 - make sure to reset the _portPMState in this case

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
            FatalError(err, "getting port status (7)");
            break;
        }
		
        if (_hub->_ssHub)
        {
            linkState = GETLINKSTATE(statusFlags);
            USBLog(5, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler port (%d) - after debounce, link state: 0x%x (%s)", this, _portNum, linkState, _hub->LinkStateName(linkState));
        }
        
       _connectionChangedState = 8;
        USBLog(4, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler port %d status(0x%04x)/change(0x%04x) - no error from GetPortStatus", this, _portNum, status.statusFlags, status.changeFlags);
        if (status.changeFlags & kHubPortConnection)
        {
			if (_debounceCount++ < 5)
			{
				_retryPortStatus = true;
				USBLog(5, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler port %d connection bounce - debounceCount (%d)", this, _portNum, (uint32_t)_debounceCount);
				break;
			}
			else
			{
				USBLog(5, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler port %d debounceCount (%d) exceeded. returning kIOReturnInternalError", this, _portNum, (uint32_t)_debounceCount);
				err = kIOReturnInternalError;
				break;
			}

        }
		
        if (status.statusFlags & kHubPortConnection)
        {
            // We have a connection on this port. Attempt to add the device
            //
            USBLog(5, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler - port %d - device detected, calling AddDevice", this, _portNum);
			USBTrace(kUSBTEnumeration, kTPEnumerationCallAddDevice, (uintptr_t)this, _portNum, _hub->_locationID, 0);
			_state = hpsDeviceZero;
            _connectionChangedState = 9;
			USBLog(6, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler - calling LaunchAddDeviceThread for port %d on hub at 0x%x", this, _portNum, (uint32_t) _hub->_locationID);
			LaunchAddDeviceThread(false);
            _connectionChangedState = 10;
        }
        
    } while(false);

	_ignoreDisconnectOnWakeup = false;
    USBLog(5, "AppleUSBHubPort[%p]::DefaultConnectionChangeHandler - port %d done, ending.", this, _portNum);
    return err;
}


IOReturn 
AppleUSBHubPort::DefaultBHResetChangeHandler(UInt16 changeFlags, UInt16 statusFlags)
{
    USBLog(5, "AppleUSBHubPort[%p]::DefaultBHResetChangeHandler - Port %d of Hub at 0x%x changes (0x%04x,0x%04x)", this, _portNum, (uint32_t)_hub->_locationID, statusFlags, changeFlags);

	return kIOReturnSuccess;
}


IOReturn 
AppleUSBHubPort::DefaultPortLinkStateChangeHandler(UInt16 changeFlags, UInt16 statusFlags)
{
	UInt16		linkState = GETLINKSTATE(statusFlags);
	IOReturn	kr = kIOReturnSuccess;
	
    USBLog(5, "AppleUSBHubPort[%p]::DefaultPortLinkStateChangeHandler - handling Port %d of Hub at 0x%x changes (0x%04x,0x%04x), LinkState: 0x%x (%s)", this, _portNum, (uint32_t)_hub->_locationID, statusFlags, changeFlags, linkState, _hub->LinkStateName(linkState));

    // Save the link state since rdar://10403546 indicates that we can't rely on polling 
    _portLinkState = linkState;
    
	kr = HandleLinkState(changeFlags, statusFlags);
	
	return kr;
}


IOReturn 
AppleUSBHubPort::DefaultPortConfigErrChangeHandler(UInt16 changeFlags, UInt16 statusFlags)
{
	UInt16				linkState = GETLINKSTATE(statusFlags);
	IOReturn			kr = kIOReturnSuccess;
	IOUSBHubPortStatus	status;
	UInt16				portLinkErrorCount = 0;
	
	kr = _hub->GetPortErrorCount(_portNum, &portLinkErrorCount);
	if ( kr != kIOReturnSuccess)
	{
		USBLog(5, "AppleUSBHubPort[%p]::DefaultPortConfigErrChangeHandler - handling Port %d of Hub at 0x%x  GetPortErrorCount: %d", this, _portNum, (uint32_t)_hub->_locationID, portLinkErrorCount);
	}
	
	USBLog(5, "AppleUSBHubPort[%p]::DefaultPortConfigErrChangeHandler - handling Port %d of Hub at 0x%x changes (0x%04x,0x%04x), LinkState: 0x%x (%s state), GetPortErrorCount: %d", this, _portNum, (uint32_t)_hub->_locationID, statusFlags, changeFlags, linkState, _hub->LinkStateName(linkState), portLinkErrorCount);
	
	// The port could not be successfully configured.  We will assume that a PLC will not be issued.  The rdar://10473856 indicates that we need to:
	//
	// 1.  If link is in U0, then issue a Port Reset
	// 2.  If link is Inactive, then issue a Warm Port Reset
	// 3.  If Link is Not in U0/Inactive, wait for it to become inactive and then issue a warm reset
	
	// If the link state is not either U0 or Inactive, let's wait for (100 ms) and see if it timeouts to another state
	if ( (linkState != kSSHubPortLinkStateU0) || (linkState != kSSHubPortLinkStateSSInactive) )
	{
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
        {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHubPort[%p]::DefaultPortConfigErrChangeHandler - IOSleep in gate:%d", this, 3);}}
#endif
		IOSleep(100);
		kr = _hub->GetPortStatus(&status, _portNum);
		if (kr != kIOReturnSuccess)
		{
			USBLog(1, "AppleUSBHubPort[%p]::DefaultPortConfigErrChangeHandler - Port %d of Hub at 0x%x  _hub->GetPortStatus() returned 0x%x", this, _portNum, (uint32_t)_hub->_locationID, kr);
			return kr;
		}
		
		changeFlags = status.changeFlags;
		statusFlags = status.statusFlags;
		linkState = GETLINKSTATE(statusFlags);
		USBLog(5, "AppleUSBHubPort[%p]::DefaultPortConfigErrChangeHandler - handling Port %d of Hub at 0x%x after waiting new status (0x%04x,0x%04x), LinkState: 0x%x (%s state)", this, _portNum, (uint32_t)_hub->_locationID, statusFlags, changeFlags, linkState, _hub->LinkStateName(linkState));
	}
	
	// Here, we need to deal with the different link states	
	switch (linkState) 
	{
		case kSSHubPortLinkStateRxDetect:
		case kSSHubPortLinkStateSSDisabled:
			SetPortVector(&AppleUSBHubPort::HandleResetPortHandler, kHubPortBeingReset);
			kr = _hub->SetPortFeature(kUSBHubPortResetFeature, _portNum);
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
        {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHubPort[%p]::DefaultPortConfigErrChangeHandler - IOSleep in gate:%d", this, 4);}}
#endif
			IOSleep(100);
			if (kr != kIOReturnSuccess)
			{
				USBLog(1, "AppleUSBHubPort[%p]::DefaultPortConfigErrChangeHandler - Port %d of Hub at 0x%x  SetPortFeature(kUSBHubPortResetFeature) returned 0x%x", this, _portNum, (uint32_t)_hub->_locationID, kr);
			}
			break;
			
		case kSSHubPortLinkStateSSInactive:
			// In order to get out of these, we need to issue a Warm Reset
			USBLog(1, "AppleUSBHubPort[%p]::DefaultPortConfigErrChangeHandler - Port %d of Hub at 0x%x (0x%04x,0x%04x) In %s, issuing a SetPortFeature(kUSBHubPortBHPortResetFeature)", this, _portNum, (uint32_t)_hub->_locationID, statusFlags, changeFlags, _hub->LinkStateName(linkState));
			
			SetPortVector(&AppleUSBHubPort::DefaultResetChangeHandler, kHubPortBeingReset);
			kr = _hub->SetPortFeature(kUSBHubPortBHPortResetFeature, _portNum);
			if (kr != kIOReturnSuccess)
			{
				USBLog(1, "AppleUSBHubPort[%p]::DefaultPortConfigErrChangeHandler - Port %d of Hub at 0x%x  SetPortFeature(kUSBHubPortBHPortResetFeature) returned 0x%x", this, _portNum, (uint32_t)_hub->_locationID, kr);
			}
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
        {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHubPort[%p]::DefaultPortConfigErrChangeHandler - IOSleep in gate:%d", this, 5);}}
#endif
			IOSleep(100);
			break;
			
		default:
			USBLog(3, "AppleUSBHubPort[%p]::DefaultPortConfigErrChangeHandler - Port %d of Hub at 0x%x  not doing anything with this link state change", this, _portNum, (uint32_t)_hub->_locationID);
			kr = kIOReturnSuccess;
			break;
	}
	
	return kr;
}


# pragma mark Change Handlers
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
	if ( me->_hub != NULL )
	{
		USBLog(6, "AppleUSBHubPort[%p]::PortStatusChangedHandlerEntry - calling LowerPowerState and DecrementOutstandingIO on hub[%p] port %d", me, me->_hub, me->_portNum);
		me->_hub->LowerPowerState();								// as far as this thread is concerned, we can lower the state
		me->_hub->DecrementOutstandingIO();							// this will rearm the interrupt read on the last call
	}
	
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

	if (_delayOnStatusChange)
	{
		USBLog(5, "AppleUSBHubPort[%p]::PortStatusChangedHandler: delaying 100ms before first GetPortStatus after a reset of port %d", this, _portNum);
		// 7294126 - add a 100ms delay immediately after resetting a port and before we try to get status for that port again
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
        {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHubPort[%p]::PortStatusChangedHandler - IOSleep in gate:%d", this, 1);}}
#endif
		IOSleep(100);
		_delayOnStatusChange = false;
	}
	
	
    // Now, loop through each bit in the port status change and see if we need to handle it
    //
    do
    {
        if ( !skipOverGetPortStatus )
        {
			USBLog(5, "AppleUSBHubPort[%p]::PortStatusChangedHandler: calling GetPortStatus for port %d", this, _portNum);
           
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
				FatalError(err, "probably a PC card was ejected");
                err = kIOReturnNoDevice;
                goto errorExit;
            }
            

            _statusChangedState = 2;
			
			if (_hub->_ssHub)
			{
				UInt16	portLinkErrorCount = 0;
				err = _hub->GetPortErrorCount(_portNum, &portLinkErrorCount);
				if ((err ==kIOReturnSuccess) && (portLinkErrorCount > _portLinkErrorCount))
				{
					USBLog(1, "AppleUSBHubPort[%p]::PortStatusChangedHandler - Hub 0x%x port %d: link error count increased by %d, linkState: 0x%x (%s)", 
						   this, (uint32_t)_hub->_locationID, _portNum, (uint32_t)(portLinkErrorCount - _portLinkErrorCount), GETLINKSTATE(_portStatus.statusFlags), _hub->LinkStateName(GETLINKSTATE(_portStatus.statusFlags)));
					_portLinkErrorCount = portLinkErrorCount;
				}
				
          	 	USBLog(5,"AppleUSBHubPort[%p]::PortStatusChangedHandler - Hub 0x%x port %d - Initial status(0x%04x)/change(0x%04x), link state: 0x%x (%s)", 
					   this, (uint32_t)_hub->_locationID, _portNum, _portStatus.statusFlags, _portStatus.changeFlags, GETLINKSTATE(_portStatus.statusFlags), _hub->LinkStateName(GETLINKSTATE(_portStatus.statusFlags)));
			}
			else
			{
           	 	USBLog(5,"AppleUSBHubPort[%p]::PortStatusChangedHandler - Hub 0x%x port %d - Initial status(0x%04x)/change(0x%04x)", this, (uint32_t)_hub->_locationID, _portNum, _portStatus.statusFlags, _portStatus.changeFlags);
            }
 			USBTrace(kUSBTEnumeration, kTPEnumerationInitialGetPortStatus, (uintptr_t)this, _portNum, _hub->_locationID, *(uintptr_t *)&_portStatus);
           
            _retryPortStatus = false;
        }
        
		// If we have a status of connection and connection change (meaning that the device dropped and came back on and we have an errata to tell us to do so, 
		// then do not remove the device -- just ignore the change.  Only do so if it happens withing x seconds of a wake
		if ( _hub->_ignoreDisconnectOnWakeup && (_portStatus.statusFlags & kHubPortConnection) && (_portStatus.changeFlags & kHubPortConnection) )
		{
			_ignoreDisconnectOnWakeup = ShouldApplyDisconnectWorkaround();
            USBLog(_ignoreDisconnectOnWakeup ? 5 : 7,"AppleUSBHubPort[%p]::PortStatusChangedHandler - port %d - ShouldApplyDisconnectWorkaround returned %d", this, _portNum, _ignoreDisconnectOnWakeup);
		}
		
		// Set a flag indicating that we had a disconnect while the _resumePending was true.  If a suspend change is present, we will clear that state variable before we deal with the
		// disconnect, so we need to save that state.
		if (_resumePending && (_portStatus.changeFlags & kHubPortConnection))
		{
			USBLog(5,"AppleUSBHubPort[%p]::PortStatusChangedHandler - Hub 0x%x port %d - Had a connection change while a resume is pending", this, (uint32_t)_hub->_locationID, _portNum);
			_hadResumePendingAndDisconnect = true;
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
            
            USBLog(5, "AppleUSBHubPort[%p]::PortStatusChangedHandler - port %d - change %d clearing feature 0x%x (%s)", this, (uint32_t)_portNum, which, (uint32_t)_changeHandler[which].clearFeature, _hub->FeatureName(_changeHandler[which].clearFeature));
            _statusChangedState = 4;
            if ((err = _hub->ClearPortFeature(_changeHandler[which].clearFeature, _portNum)))
            {
                USBLog(3, "AppleUSBHubPort[%p]::PortStatusChangedHandler - port %d - error %x clearing feature 0x%x (%s)", this, (uint32_t)_portNum, err, (uint32_t)_changeHandler[which].clearFeature, _hub->FeatureName(_changeHandler[which].clearFeature));
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
			FatalError(err, "status and change flags are 0xffff");
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
            USBLog((kIOReturnNoDevice == err) ? 5 : 1,"AppleUSBHubPort[%p]::PortStatusChangedHandler - Port %d of Hub at 0x%x - error %x from (%d) handler", this, _portNum, (uint32_t)_hub->_locationID, err, which);
            break;
        }
    }
    while (true);

// this is not JUST the error exit. it is also the normal exit, in spite of the label name
errorExit:
	
	if ( err != kIOReturnSuccess )
	{
		if ( _attachMessageDisplayed )
		{
			{
				USBError(1,"[%p] The IOUSBFamily was not able to enumerate a device.", this);
			}
			_attachMessageDisplayed = false;
		}
		if ( _devZero )
		{
			// We should disable the port here as well..
			//
			USBLog(5,"AppleUSBHubPort[%p]::PortStatusChangedHandler - port %d - err = %x - done, releasing Dev Zero lock", this, _portNum, err);
			_bus->ReleaseDeviceZero();
			_devZero = false;
		}
	}
	
    USBLog(5,"AppleUSBHubPort[%p]::PortStatusChangedHandler - port %d - err = %x - done, releasing _runLock", this, _portNum, err);
    
    IOLockUnlock(_runLock);
    IOLockUnlock(_initLock);
    _statusChangedThreadActive = false;
	_debounceCount = 0;
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
            case kSSHubPortChangeBHResetMask:
                _changeHandler[vector].handler = &AppleUSBHubPort::DefaultBHResetChangeHandler;
                break;
            case kSSHubPortChangePortLinkStateMask:
                _changeHandler[vector].handler = &AppleUSBHubPort::DefaultPortLinkStateChangeHandler;
                break;
            case kSSHubPortChangePortConfigErrMask:
                _changeHandler[vector].handler = &AppleUSBHubPort::DefaultPortConfigErrChangeHandler;
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
        if (condition == _changeHandler[vector].bit)
        {
            _changeHandler[vector].handler = routine;
        }
    }
}


IOReturn 
AppleUSBHubPort::ReleaseDevZeroLock()
{
    USBLog(5, "AppleUSBHubPort[%p]::ReleaseDevZeroLock Port %d of Hub at 0x%x, devZero = 0x%x, ", this, _portNum, (uint32_t)_hub->_locationID, _devZero);

    if (_devZero)
    {
		IOUSBHubPortStatus	status;
		IOReturn 			kr = kIOReturnSuccess;
		
        // Get the PortStatus to see if the device is still there.
        kr =  _hub->GetPortStatus(&status, _portNum);
		if ( kr != kIOReturnSuccess)
        {
			USBLog(3, "AppleUSBHubPort[%p]::ReleaseDevZeroLock()  Port %d of Hub at 0x%x, GetPortStatus returned 0x%x", this, _portNum, (uint32_t)_hub->_locationID, kr);
        }
		else
		{
			UInt16				linkState = GETLINKSTATE(status.statusFlags);
			USBLog(5, "AppleUSBHubPort[%p]::ReleaseDevZeroLock - Port %d of Hub at 0x%x: status (0x%04x,0x%04x), LinkState: 0x%x (%s state)", this, _portNum, (uint32_t)_hub->_locationID, status.statusFlags, status.changeFlags, linkState, _hub->LinkStateName(linkState));
		}
		
		if (!_hub->_ssHub)
			(void) _hub->ClearPortFeature(kUSBHubPortEnableFeature, _portNum);
        
        USBLog(1, "AppleUSBHubPort[%p]::ReleaseDevZeroLock()", this);
		USBTrace( kUSBTHubPort,  kTPHubPortReleaseDevZeroLock, (uintptr_t)this, _portNum, _devZero, _hub->_locationID);

        _state = hpsNormal;
 
        if ( _bus )
            _bus->ReleaseDeviceZero();
            
        _devZero = false;
        
        
        // Nano says this is probably not needed, it was causing audio glitches.
        // IOSleep(300);
        
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
    USBLog(1, "AppleUSBHubPort[%p]::DetachDevice Port %d of Hub at 0x%x being detached (_attachRetry = %d)", this, (uint32_t)_portNum, (uint32_t)_hub->_locationID, (uint32_t)_attachRetry);
	USBTrace( kUSBTHubPort,  kTPHubPortDetachDevice, (uintptr_t)this, _portNum, _hub->_locationID, 1);

	// If we haven't displayed the error message, do it this once:
	//
	if ( !_attachMessageDisplayed )
	{
        USBError(1,"[%p] The IOUSBFamily is having trouble enumerating a USB device that has been plugged in.  It will keep retrying.  (Port %d of Hub at 0x%x)", this, (uint32_t)_portNum, (uint32_t)_hub->_locationID);
		_attachMessageDisplayed = true;
	}
	
	// Give up after 10 Retries (0-9)
	//
	if ( _attachRetryFailed == true )
	{
		_state = hpsDeadDeviceZero;
		err = kIOReturnNotResponding;
		goto ErrorExit;
	}
	
	if( _attachRetry == 9 )
	{
		_attachRetryFailed = true;
        USBError(1,"[%p] The IOUSBFamily gave up enumerating a USB device after 10 retries.  (Port %d of Hub at 0x%x)", this, (uint32_t)_portNum, (uint32_t)_hub->_locationID);
		_state = hpsDeadDeviceZero;
		err = kIOReturnNotResponding;
		goto ErrorExit;
	}
	
	// Increment our number of attach retries and see if we need to power off the port
	//
	_attachRetry++;
    
    if ( _attachRetry % 4 == 0  )
    {
		const int kWaitForDevicePowerON = 100; // milliseconds
		
        // This device is misbehaving a lot, wait  before attempting to enumerate it again:
        //
        delay = _attachRetry * kWaitForDevicePowerON;
        
        USBLog(1, "AppleUSBHubPort[%p]::DetachDevice (Port %d of Hub at 0x%x), attachRetry limit reached. delaying for %d milliseconds", this, (uint32_t)_portNum, (uint32_t)_hub->_locationID, (uint32_t)delay);
		USBTrace( kUSBTHubPort,  kTPHubPortDetachDevice, (uintptr_t)this, _portNum, _hub->_locationID, delay );
        
		// Try power off and disabling the port
		//
		if ( (err = _hub->SetPortPower(_portNum, kHubPortPowerOff)) )
		{
            FatalError(err, "clearing port power feature");
            goto ErrorExit;
		}
		        
        // Wait for before powering it back on.  Spec says to wait 100ms, we will
        // wait some more.
        //
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
        {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHubPort[%p]::DetachDevice - IOSleep in gate:%d", this, 1);}}
#endif
        IOSleep(delay);
        
		// Try power off and disabling the port
		//
		if ( (err = _hub->SetPortPower(_portNum, kHubPortPowerOn)) )
		{
            FatalError(err, "setting port power feature");
            goto ErrorExit;
		}
		
        // Since this device is misbehaving, wait before returning here
        //
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
        {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHubPort[%p]::DetachDevice - IOSleep in gate:%d", this, 2);}}
#endif
        IOSleep(kWaitForDevicePowerON);
        
        _state = hpsDeadDeviceZero;
        
        err = kIOReturnNotResponding;
    }
    else
    {
        // Get the PortStatus to see if the device is still there.
        if ((err = _hub->GetPortStatus(&status, _portNum)))
        {
            FatalError(err, "getting port status (5)");
            goto ErrorExit;
        }
        
        // If the device is not here, then bail out
        //
        if ( !(status.statusFlags & kHubPortConnection) )
        {
            // We don't have a connection on this port anymore.
            //
            USBLog(1, "AppleUSBHubPort[%p]::DetachDevice - Port %d of Hub at 0x%x - device has gone away", this, _portNum, (uint32_t)_hub->_locationID);
			USBTrace( kUSBTHubPort,  kTPHubPortDetachDevice, (uintptr_t)this, _portNum, _hub->_locationID, kIOReturnNoDevice );
            _state = hpsDeadDeviceZero;
            err = kIOReturnNoDevice;
            goto ErrorExit;
        }

#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
        {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHubPort[%p]::DetachDevice - IOSleep in gate:%d", this, 31);}}
#endif
        IOSleep(300);
        
        // Get the PortStatus to see if the device is still there.
        if ((err = _hub->GetPortStatus(&status, _portNum)))
        {
            FatalError(err, "getting port status (6)");
            goto ErrorExit;
        }
        
        if (status.statusFlags & kHubPortConnection)
        {
			// We have a connection on this port.  Disable the port
			if (!_hub->_ssHub)
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
			
			USBTrace(kUSBTEnumeration, kTPEnumerationCallAddDevice, (uintptr_t)this, _portNum, _hub->_locationID, 0);
			err = kIOReturnSuccess;
			USBLog(6, "AppleUSBHubPort[%p]::DetachDevice - calling LaunchAddDeviceThread for port %d on hub at 0x%x", this, _portNum, (uint32_t) _hub->_locationID);
			LaunchAddDeviceThread(true);
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
    UInt32				retries = kMaxDevZeroRetries;
    IOReturn			err = kIOReturnSuccess;
    IOReturn			portStatusErr = kIOReturnSuccess;
    IOUSBHubPortStatus	status;
    
    do 
    {
		bzero(&_desc, sizeof(_desc));
		err = _bus->GetDeviceZeroDescriptor(&_desc, (kUSBDeviceSpeedHigh == _speed || kUSBDeviceSpeedSuper == _speed) ? 18 : 8); 	// get the first 8 bytes (4693694: 18 for HS device)
        // If the error is kIOReturnOverrun, we still received our 8 bytes, so signal no error.
        //
        if ( err == kIOReturnOverrun )
        {
			USBLog(1, "AppleUSBHubPort[%p]::GetDevZeroDescriptorWithRetries - port %d of hub @ 0x%x - GetDeviceZeroDescriptor returned kIOReturnOverrun.  Checking for valid descriptor", this, _portNum, (uint32_t)_hub->_locationID);
			USBTrace( kUSBTHubPort,  kTPHubPortGetDevZeroDescriptorWithRetries, (uintptr_t)this, _portNum, _hub->_locationID, 1 );
           
			// We need to check that _desc looks like a valiad one
			if ( (_desc.bDescriptorType == kUSBDeviceDesc) && 
				 (_desc.bLength == 18) &&
				 (_desc.bMaxPacketSize0 >= 8 ) &&
				 (_desc.bMaxPacketSize0 <= 64 )
				)
			{
				// OK, let's assume that the rest of the descriptor is OK
				USBLog(1, "AppleUSBHubPort[%p]::GetDevZeroDescriptorWithRetries - port %d of hub @ 0x%x - GetDeviceZeroDescriptor returned kIOReturnOverrun.  Descriptor looks valid", this, _portNum, (uint32_t)_hub->_locationID);
				USBTrace( kUSBTHubPort,  kTPHubPortGetDevZeroDescriptorWithRetries, (uintptr_t)this, _portNum, _hub->_locationID, 2 );
				err = kIOReturnSuccess;
				break;
			}
        }
        
        if ( err )
        {
			USBLog(1, "AppleUSBHubPort[%p]::GetDevZeroDescriptorWithRetries - port %d of hub @ 0x%x - GetDeviceZeroDescriptor returned 0x%x", this, _portNum, (uint32_t)_hub->_locationID, err);
			USBTrace( kUSBTHubPort,  kTPHubPortGetDevZeroDescriptorWithRetries, (uintptr_t)this, _portNum, err, 3 );

            // Let's make sure that the device is still here.  Maybe it has gone away and we won't process the notification 'cause we're in PSCH
            // Get the PortStatus to see if the device is still there.
            portStatusErr = _hub->GetPortStatus(&status, _portNum);
            if (portStatusErr != kIOReturnSuccess)
            {
				USBLog(1, "AppleUSBHubPort[%p]::GetDevZeroDescriptorWithRetries - port %d of hub @ 0x%x - GetPortStatus returned 0x%x", this, _portNum, (uint32_t)_hub->_locationID, err);
				USBTrace( kUSBTHubPort,  kTPHubPortGetDevZeroDescriptorWithRetries, (uintptr_t)this, _portNum, portStatusErr, 4 );
                FatalError(err, "getting port status (4)");
                break;
            }
            
            if ( !(status.statusFlags & kHubPortConnection) )
            {
                // We don't have a connection on this port anymore.
                //
                USBLog(1, "AppleUSBHubPort[%p]::GetDevZeroDescriptorWithRetries - port %d of hub @ 0x%x - device has gone away", this, _portNum, (uint32_t)_hub->_locationID);
				USBTrace( kUSBTHubPort,  kTPHubPortGetDevZeroDescriptorWithRetries, (uintptr_t)this, _portNum, _state, kIOReturnNoDevice );
                _state = hpsDeadDeviceZero;
                err = kIOReturnNoDevice;
                break;
            }
            
			// If the port is suspended, we should unsuspend it and try again
			// ееее
			if ( status.statusFlags & kHubPortSuspend)
			{
                USBLog(1, "AppleUSBHubPort[%p]::GetDevZeroDescriptorWithRetries - port %d of hub @ 0x%x - port is suspended", this, _portNum, (uint32_t)_hub->_locationID);
				USBTrace( kUSBTHubPort,  kTPHubPortGetDevZeroDescriptorWithRetries, (uintptr_t)this, _portNum, _hub->_locationID, 5 );

				portStatusErr = _hub->ClearPortFeature(kUSBHubPortSuspendFeature, _portNum);
				if (kIOReturnSuccess != portStatusErr)
				{
					USBLog(3, "AppleUSBHubPort[%p]::GetDevZeroDescriptorWithRetries Could not ClearPortFeature (%d) (kHubPortSuspend): (0x%x)", this, (uint32_t)_portNum, err);
					break;
				}
			}
			
           if ((err == kIOUSBTransactionTimeout) && (_speed == kUSBDeviceSpeedSuper))
            {
				USBLog(3, "AppleUSBHubPort[%p]::GetDevZeroDescriptorWithRetries  setting tries to 1 because we got a timeout error on a super speed device", this);
                retries = 1;
            }
            else
            {
                // If we got a timeout error, then reset the retry to only 2
                if ((err == kIOUSBTransactionTimeout) && ( retries == kMaxDevZeroRetries))
                {
                    USBLog(3, "AppleUSBHubPort[%p]::GetDevZeroDescriptorWithRetries  setting retries to 2 because we got a timeout error", this);
                    retries = 2;
                }
            }
			
            if ( retries == 2)
                delay = 3;
            else if ( retries == 1 )
                delay = 30;
                
            USBLog(3, "AppleUSBHubPort[%p]::GetDevZeroDescriptorWithRetries - port %d of hub @ 0x%x , err: %x - sleeping for %d milliseconds", this, (uint32_t)_portNum, (uint32_t)_hub->_locationID, err, (uint32_t)delay);
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
            {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHubPort[%p]::GetDevZeroDescriptorWithRetries - IOSleep in gate:%d", this, 1);}}
#endif
            IOSleep( delay );
            retries--;
        }
		else 
		{
			// No error, but let's validate the bMaxPacketSize0
			if ( (_desc.bMaxPacketSize0 < 8) ||  (_desc.bMaxPacketSize0 > 64 ) )
			{
				// The USB descriptor is not right.  Don't even retry
				USBError(1, "The USB Family found a device at  port %d of hub @ 0x%x with a bad USB device descriptor (0x%qx, 0x%qx )", (uint32_t)_portNum, (uint32_t)_hub->_locationID, * (uint64_t *)(&_desc), * (uint64_t *)(&_desc.idVendor));
				USBTrace( kUSBTHubPort,  kTPHubPortGetDevZeroDescriptorWithRetries, (uintptr_t)this, _portNum, _hub->_locationID, 6 );
				retries = 0;
				err = kIOReturnDeviceError;
				break;
			}
			
		}

		if ((_hub->_powerStateChangingTo < kIOUSBHubPowerStateLowPower) && (_hub->_powerStateChangingTo != kIOUSBHubPowerStateStable))
		{
			// I am making this a level 1 because it will generally indicate a problem with a device
			USBLog(1, "AppleUSBHubPort[%p]::GetDevZeroDescriptorWithRetries - aborting due to power change", this);
			USBTrace( kUSBTHubPort,  kTPHubPortGetDevZeroDescriptorWithRetries, (uintptr_t)this, _portNum, _hub->_powerStateChangingTo, 7 );
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
	
	USBLog(6, "AppleUSBHubPort[%p]::AcquireDeviceZero (Port %d of Hub at 0x%x) - _bus->AcquireDeviceZero returned 0x%x", this, (uint32_t)_portNum, (uint32_t)_hub->_locationID, err);
   
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
	UInt64			wakeTime;
	
	// First have we seen a wake, _wakeupTime is zeroed at Init.
	absolutetime_to_nanoseconds(_hub->_wakeupTime, &wakeTime);
	if (wakeTime != 0)
	{	
		clock_get_uptime(&now);
		SUB_ABSOLUTETIME(&now, &(_hub->_wakeupTime));
		absolutetime_to_nanoseconds(now, &msElapsed);
		
		// Convert to millisecs
		msElapsed /= 1000000;
		
		USBLog(7, "AppleUSBHubPort[%p]::ShouldApplyDisconnectWorkaround - port %d of hub @ 0x%x - time since wake: %qd ms", this, _portNum, (uint32_t)_hub->_locationID, msElapsed);
		
		// Only set the workaround if we are within kWaitTimeToIgnoreDisconnect milliseconds of a wake event (and our hub indicates that we should ignore it)
		if (_hub->_ignoreDisconnectOnWakeup && _portDevice && (msElapsed < kWaitTimeToIgnoreDisconnect) )
		{
			OSBoolean * deviceHasMassStorageInterfaceRef = OSDynamicCast( OSBoolean, _portDevice->getProperty("kHasMSCInterface") );
			if ( deviceHasMassStorageInterfaceRef && deviceHasMassStorageInterfaceRef->isTrue() )
			{
				USBLog(5, "AppleUSBHubPort[%p]::ShouldApplyDisconnectWorkaround - port %d of hub @ 0x%x - we have a disconnect/reconnect on a mass storage device on a hub that tells us to ignore it, within %d ms of wake", this, _portNum, (uint32_t)_hub->_locationID, (uint32_t)msElapsed);
				returnValue = true;
			}
			else if ( _hub->_isRootHub )
			{
				// Not quite sure why this check is here. I believe that it might have been for an issue we were going to work around but did not need to do so.
				USBLog(7, "AppleUSBHubPort[%p]::ShouldApplyDisconnectWorkaround - port %d - we have a disconnect/reconnect on a root hub that tells us to ignore it", this, _portNum);
				returnValue = true;
			}
			else 
			{
				// If we have a hub (no IOUSBDevice API to get the bDeviceClass!), then also ignore a disconnect.  This will work if it is indeed a random disconnect and the hub has not
				// dropped off and is still configured.  If it is not, then we will eventually reset it and all the devices downstream will then be disconnected.
				OSNumber * bDeviceClassObj = OSDynamicCast( OSNumber, _portDevice->getProperty(kUSBDeviceClass) );
				if ( bDeviceClassObj && (bDeviceClassObj->unsigned32BitValue() == kUSBHubClass))
				{
					USBLog(5, "AppleUSBHubPort[%p]::ShouldApplyDisconnectWorkaround - port %d of hub @ 0x%x - we have a disconnect/reconnect off a hub on a hub that tells us to ignore it, within %d ms of wake", this, _portNum, (uint32_t)_hub->_locationID, (uint32_t)msElapsed);
					returnValue = true;
				}
			}	
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
					USBTrace( kUSBTHubPort, kTPHubPortWaitForSuspendCommand, (uintptr_t)this, 0, (uintptr_t)_hub->_myPowerState, 3 );
					break;
					
				case THREAD_TIMED_OUT:
					USBLog(3,"AppleUSBHubPort[%p]::WaitForSuspendCommand commandSleep timed out (THREAD_TIMED_OUT) _hub->_myPowerState(%d)", this, (int)_hub->_myPowerState);
					USBTrace( kUSBTHubPort, kTPHubPortWaitForSuspendCommand, (uintptr_t)this, 0, (uintptr_t)_hub->_myPowerState, 4 );
					break;
					
				case THREAD_INTERRUPTED:
					USBLog(3,"AppleUSBHubPort[%p]::WaitForSuspendCommand commandSleep was interrupted (THREAD_INTERRUPTED) _hub->_myPowerState(%d)", this, (int)_hub->_myPowerState);
					USBTrace( kUSBTHubPort, kTPHubPortWaitForSuspendCommand, (uintptr_t)this, 0, (uintptr_t)_hub->_myPowerState, 5 );
					break;
					
				case THREAD_RESTART:
					USBLog(3,"AppleUSBHubPort[%p]::WaitForSuspendCommand commandSleep was restarted (THREAD_RESTART) _hub->_myPowerState(%d)", this, (int)_hub->_myPowerState);
					USBTrace( kUSBTHubPort, kTPHubPortWaitForSuspendCommand, (uintptr_t)this, 0, (uintptr_t)_hub->_myPowerState, 6 );
					break;
					
				case kIOReturnNotPermitted:
					USBLog(3,"AppleUSBHubPort[%p]::WaitForSuspendCommand commandSleep woke up (kIOReturnNotPermitted) _hub->_myPowerState(%d)", this, (int)_hub->_myPowerState);
					USBTrace( kUSBTHubPort, kTPHubPortWaitForSuspendCommand, (uintptr_t)this, 0, (uintptr_t)_hub->_myPowerState, 7 );
					break;
					
				default:
					USBLog(3,"AppleUSBHubPort[%p]::WaitForSuspendCommand commandSleep woke up with status (%p) _hub->_myPowerState(%d)", this, (void*)kr, (int)_hub->_myPowerState);
					USBTrace( kUSBTHubPort, kTPHubPortWaitForSuspendCommand, (uintptr_t)this, kr, (uintptr_t)_hub->_myPowerState, 8 );
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

void 
AppleUSBHubPort::EnablePowerAfterOvercurrentEntry(OSObject *target)
{
    AppleUSBHubPort 	*me;
    
    if (!target)
    {
        USBLog(5, "AppleUSBHubPort::EnablePowerAfterOvercurrentEntry - no target!");
        return;
    }
    
    me = OSDynamicCast(AppleUSBHubPort, target);
    
    if (!me)
    {
        USBLog(5, "AppleUSBHubPort::EnablePowerAfterOvercurrentEntry - target is not really me!");
        return;
    }
	
	if ( me->IsInactive() )
	{
		USBLog(6, "AppleUSBHubPort[%p]::EnablePowerAfterOvercurrentEntry - port object is inActive() for port %d on hub at 0x%x", me, me->_portNum, (uint32_t) me->_hub->_locationID);
		return;
	}

	USBTrace( kUSBTHubPort, kTPHubPortEnablePowerAfterOvercurrent, (uintptr_t)me, me->_hub->_locationID, me->_portNum, 3);
    me->EnablePowerAfterOvercurrent();
	USBLog(6, "AppleUSBHubPort[%p]::EnablePowerAfterOvercurrentEntry - calling LowerPowerState and release on hub[%p] port %d", me, me->_hub, me->_portNum);
	me->_hub->LowerPowerState();
	me->_hub->release();
	me->_enablePowerAfterOvercurrentThreadActive = false;
	if (me->_inCommandSleep)
    {
		IOCommandGate *gate = NULL;
		if (me->_bus)
			gate = me->_bus->GetCommandGate();
		if (gate)
		{
			USBLog(2,"AppleUSBHubPort[%p]::EnablePowerAfterOvercurrentEntry -  calling commandWakeup", me);
			USBTrace( kUSBTHubPort, kTPHubPortEnablePowerAfterOvercurrent, (uintptr_t)me, me->_hub->_locationID, me->_portNum, 4);
			gate->commandWakeup(&me->_enablePowerAfterOvercurrentThreadActive, true);
		}
    }
    me->release();
}

void 
AppleUSBHubPort::EnablePowerAfterOvercurrent()
{
	IOReturn			kr;
	IOUSBHubPortStatus	portStatus;

	if ( IsInactive() )
	{
		USBLog(6, "AppleUSBHubPort[%p]::EnablePowerAfterOvercurrent - port object is inActive() for port %d on hub at 0x%x", this, _portNum, (uint32_t) _hub->_locationID);
		return;
	}

	if (_hub->isInactive() || !_hub->_device || _hub->_device->isInactive())
	{
		USBLog(5, "AppleUSBHubPort[%p]::EnablePowerAfterOvercurrent - port %d on hub at 0x%x - hub[%p] is inactive or hub device[%p] is missing or inactive - aborting EnablePowerAfterOvercurrent", this, _portNum, (uint32_t) _hub->_locationID, _hub, _hub->_device);
		return;
	}
	
	// if we are about to change to off, restart, or sleep, then don't do this
	if ((_hub->_powerStateChangingTo < kIOUSBHubPowerStateLowPower) && (_hub->_powerStateChangingTo != kIOUSBHubPowerStateStable))
	{
		USBLog(5, "AppleUSBHubPort[%p]::EnablePowerAfterOvercurrent - port %d on hub at 0x%x - hub[%p] changing to power state[%d] - aborting EnablePowerAfterOvercurrent", this, _portNum, (uint32_t) _hub->_locationID, _hub, (int)_hub->_powerStateChangingTo);
		return;
	}
	
	// Do a port status to see if the power power is NOT on.  If it is not, then wait kIOWaitTimeBeforeReEnablingPortPowerAfterOvercurrent ms
	kr = _hub->GetPortStatus( &portStatus, _portNum);
	USBTrace( kUSBTHubPort, kTPHubPortEnablePowerAfterOvercurrent, (uint32_t)_hub->_locationID, _portNum, (uintptr_t) (* (uintptr_t *)&portStatus), 0);

	if ( kr == kIOReturnSuccess )
	{
		USBLog(4, "AppleUSBHub[%p]::EnablePowerAfterOvercurrentEntry - port %d of hub at 0x%x status(%p) change(%p)", this, (uint32_t)_portNum, (uint32_t)_hub->_locationID, (void*)portStatus.statusFlags, (void*)portStatus.changeFlags);

		if ((portStatus.statusFlags & kHubPortPower) == 0)
		{
			USBLog(1, "AppleUSBHub[%p]::EnablePowerAfterOvercurrentEntry - port %d of hub at 0x%x is not powered on! portPMState=%d.  Waiting %d ms and then start()'ing port", this, (uint32_t)_portNum, (uint32_t)_hub->_locationID, _portPMState, kIOWaitTimeBeforeReEnablingPortPowerAfterOvercurrent);
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
            {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHubPort[%p]::EnablePowerAfterOvercurrentEntry - IOSleep in gate:%d", this, 1);}}
#endif
			IOSleep(kIOWaitTimeBeforeReEnablingPortPowerAfterOvercurrent);
			
			USBTrace( kUSBTHubPort, kTPHubPortEnablePowerAfterOvercurrent, (uint32_t)_hub->_locationID, _portNum, _portPMState, 1);
			start();
			_portPMState = usbHPPMS_active;
		}
		else 
		{
			USBLog(5, "AppleUSBHub[%p]::EnablePowerAfterOvercurrentEntry - port %d of hub at 0x%x is powered on, nothing to do here! portPMState=%d", this, (uint32_t)_portNum, (uint32_t)_hub->_locationID, _portPMState);
		}
	}
	else
	{
		USBLog(1,"AppleUSBHub[%p]::EnablePowerAfterOvercurrentEntry  GetPortStatus for port %d of hub @ 0x%x returned 0x%x, so port might be dead for good", this, (uint32_t)_portNum, (uint32_t)_hub->_locationID, kr);
		USBTrace( kUSBTHubPort, kTPHubPortEnablePowerAfterOvercurrent, (uint32_t)_hub->_locationID, _portNum, kr, 2);
	}
}	

bool 
AppleUSBHubPort::DetectExpressCardCantWake()
{
	if ( _hub && !_detectedExpressCardCantWake )
	{
		USBLog(2, "AppleUSBHubPort[%p]::DetectExpressCardCantWake found an express card device which will disconnect across sleep  port %d of hub @ 0x%x ", this, (uint32_t)_portNum, (uint32_t)_hub->_locationID );

		_detectedExpressCardCantWake	 = true;
		
		if (_portDevice)
		{
			IOUSBDevice	*cachedPortDevice = _portDevice;

			cachedPortDevice->retain();
			_cachedBadExpressCardVID = cachedPortDevice->GetVendorID();
			_cachedBadExpressCardPID = cachedPortDevice->GetProductID();
			cachedPortDevice->release();
		}
		
		return true;
	}
	return false;
}

