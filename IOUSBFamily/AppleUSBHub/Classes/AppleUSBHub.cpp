/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright й 1998-2012 Apple Inc.  All rights reserved.
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
#include <libkern/OSDebug.h>
#include <libkern/OSAtomic.h>
#include <libkern/version.h>

#include <IOKit/IOKitKeys.h>
#include <IOKit/IOMessage.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/IOPlatformExpert.h>

#include <IOKit/usb/IOUSBLog.h>

#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBRootHubDevice.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/IOUSBControllerV2.h>
#include <IOKit/usb/IOUSBControllerV3.h>

#include "AppleUSBHub.h"
#include "AppleUSBHubPort.h"
#include "USBTracepoints.h"

//================================================================================================
//
//   Local Definitions
//
//================================================================================================
//
#define super IOUSBHubPolicyMaker
#define self this

// Comment out the following to test whether a hub will recover from errors and reset itself (needs a Q30 keyboard)
// #define TEST_HUB_RECOVERY 1

// Uncomment the following if we only want the root hubs to suspend on sleep -- and not individual ports of every hub
// #define USE_GLOBAL_SUSPEND	1


#define  WATCHDOGSECONDS	6

enum
{
	kWatchdogTimerPeriod	=	1000 * WATCHDOGSECONDS,			// Issue our watchdog timer every WATCHDOGSECONDS sec
	kDevZeroTimeoutCount	=   30 / WATCHDOGSECONDS,			// We will look at dev zero locks every # of these
	kHubDriverRetryCount	=	3,
	kRootHubPollingInterval	=	32,
	kInitialDelayTime		=	1500							// wait at least 1.5 seconds before allowing us to go to low power
};

// from the EHCI driver
enum
{
    kEHCITestMode_Off		= 0,
    kEHCITestMode_J_State	= 1,
    kEHCITestMode_K_State 	= 2,
    kEHCITestMode_SE0_NAK	= 3,
    kEHCITestMode_Packet	= 4,
    kEHCITestMode_ForceEnable	= 5,
    kEHCITestMode_Start		= 10,
    kEHCITestMode_End		= 11,
#ifdef SUPPORTS_SS_USB
#endif
};

#define errataListLength (sizeof(errataList)/sizeof(ErrataListEntry))


//================================================================================================
//
//   Globals (static member variables)
//
//================================================================================================
static ErrataListEntry	errataList[] = {

/* For the Cherry 4 port KB, From Cherry:
We use the bcd_releasenumber-highbyte for hardware- and the lowbyte for
firmwarestatus. We have 2 different for the hardware 03(eprom) and
06(masked microcontroller). Firmwarestatus is 05 today.
So high byte can be 03 or 06 ----  low byte can be 01, 02, 03, 04, 05

Currently we are working on a new mask with the new descriptors. The
firmwarestatus will be higher than 05.
*/
      {0x046a, 0x0003, 0x0301, 0x0305, kErrataCaptiveOKBit}, // Cherry 4 port KB
      {0x046a, 0x0003, 0x0601, 0x0605, kErrataCaptiveOKBit}  // Cherry 4 port KB
};


//================================================================================================
//
//   AppleUSBHub Methods
//
//================================================================================================
//
OSDefineMetaClassAndStructors(AppleUSBHub, IOUSBHubPolicyMaker)


#pragma mark ееееееее IOService Methods ееееееее
bool 
AppleUSBHub::init( OSDictionary * propTable )
{
    if ( !super::init(propTable))
        return (false);

	// Just make sure some variables are initialized to our expected values
    _numCaptive = 0;
    _startupDelay = 0;
    _timerSource = NULL;
    _gate = NULL;
    _portSuspended = false;
    _hubHasBeenDisconnected = false;
    _hubIsDead = false;
    _workThread = NULL;
    _resetPortZeroThread = NULL;
    _hubDeadCheckThread =  NULL;
    _busPowerGood = false;
    _powerForCaptive = 0;
    _outstandingIO = 0;
    _outstandingResumes = 0;
	_raisedPowerStateCount = 0;
    _needToClose = false;
	_abortExpected = false;
	_needInterruptRead = false;
	_devZeroLockedTimeoutCounter = kDevZeroTimeoutCount;
	_retryCount = kHubDriverRetryCount;
	_checkPortsThreadActive	= false;
    _overCurrentNoticeDisplayed = false;
	SUB_ABSOLUTETIME(&_wakeupTime, &_wakeupTime);	// (BT) Easiest way of zeroing this I could think of
	clock_get_uptime(&_overCurrentNoticeTimeStamp);

	return(true);
}



bool 
AppleUSBHub::start(IOService * provider)
{
	bool ret;
	
    _inStartMethod = true;
	_raisedPowerStateCount = 1;								// start at 1 (which we will lower later) but don't call RaiseOutstandingIO since we don't want a power manager call yet

	USBLog(6, "AppleUSBHub[%p]::start - calling IncrementOutstandingIO", this);
    IncrementOutstandingIO();								// make sure we don't close until start is done
	
	
	USBLog(7, "AppleUSBHub[%p]::start - This is handled by the superclass", this);
	ret = super::start(provider);
	if (ret)
	{
#ifdef SUPPORTS_SS_USB
		if (_ssHub)
		{
			USBLog(1, "AppleUSBHub[%p]::start -  USB3 Generic Hub @ %d (0x%x)", this, _address, (uint32_t)_locationID);
		}
		else
#endif
		{
			USBLog(1, "AppleUSBHub[%p]::start -  USB Generic Hub @ %d (0x%x)", this, _address, (uint32_t)_locationID);
		}
		USBLog(6, "AppleUSBHub[%p]::start - device name %s - done - calling LowerPowerState", this, _device->getName());
		LowerPowerState();									// this is to balance the setting of the state to 1 at the beginning
	}
	else
	{
		USBLog(1, "AppleUSBHub[%p]::start -  super::start failed", this);
	}
	
	USBLog(6, "AppleUSBHub[%p]::start - device name %s - done ret (%s) - calling DecrementOutstandingIO", this, _device ? _device->getName() : "No Device", ret ? "TRUE" : "FALSE");
	DecrementOutstandingIO();
	_inStartMethod = false;
	return ret;
}


bool
AppleUSBHub::ConfigureHubDriver(void)
{
	// this is called as part of AppleUSBHub::start
    IOReturn				err = 0;
    OSDictionary			*providerDict;
    OSNumber				*errataProperty;
    OSNumber				*locationIDProperty;
	OSBoolean				*boolProperty = NULL;
	
   // Create the timeout event source
    //
    _timerSource = IOTimerEventSource::timerEventSource(this, (IOTimerEventSource::Action) TimeoutOccurred);
	
    if ( _timerSource == NULL )
    {
        USBError(1, "AppleUSBHub[%p]::ConfigureHubDriver Couldn't allocate timer event source", this);
        goto ErrorExit;
    }
	
    
    _gate = IOCommandGate::commandGate(this);
	
    if (!_gate)
    {
		USBError(1, "AppleUSBHub[%p]::ConfigureHubDriver - unable to create command gate", this);
        goto ErrorExit;
    }
	
	_workLoop = getWorkLoop();
    if ( !_workLoop )
    {
        USBError(1, "AppleUSBHub::ConfigureHubDriver Couldn't get provider's workloop");
        goto ErrorExit;
    }
	
    // Keep a reference to our workloop
    //
    _workLoop->retain();
	
    if ( _workLoop->addEventSource( _timerSource ) != kIOReturnSuccess )
    {
        USBError(1, "AppleUSBHub::ConfigureHubDriver Couldn't add timer event source");
        goto ErrorExit;
    }
	
    if ( _workLoop->addEventSource( _gate ) != kIOReturnSuccess )
    {
        USBError(1, "AppleUSBHub::ConfigureHubDriver Couldn't add gate event source");
        goto ErrorExit;
    }
    
    _address	= _device->GetAddress();
	
    if (!_device->open(this))
    {
        USBError(1, "AppleUSBHub::ConfigureHubDriver unable to open provider");
        goto ErrorExit;
    }
	
    // Check to see if we have an errata for the startup delay and if so,
    // then sleep for that amount of time.
    //
    errataProperty = (OSNumber *)_device->getProperty("kStartupDelay");
    if ( errataProperty )
    {
        _startupDelay = errataProperty->unsigned32BitValue();
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
        {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHub[%p]::ConfigureHubDriver - IOSleep in gate:%d", this, 1);}}
#endif
        IOSleep( _startupDelay );
    }
	
    boolProperty = (OSBoolean *)_device->getProperty("kIgnoreDisconnectOnWakeup");
    if ( boolProperty )
    {
        _ignoreDisconnectOnWakeup = boolProperty->isTrue();
		USBLog(5, "AppleUSBHub[%p]::ConfigureHubDriver - found kIgnoreDisconnectOnWakeup (%d)", this, _ignoreDisconnectOnWakeup);
    }
    
    boolProperty = (OSBoolean *)_device->getProperty("kRetryBogusPortStatus");
    if ( boolProperty )
    {
        _retryBogusPortStatus = boolProperty->isTrue();
		USBLog(5, "AppleUSBHub[%p]::ConfigureHubDriver - found kRetryBogusPortStatus (%d)", this, _retryBogusPortStatus);
    }

	boolProperty = (OSBoolean *)_device->getProperty("kAssumePortsAreCaptive");
    if ( boolProperty )
    {
        _treatAllPortsAsCaptive = boolProperty->isTrue();
		USBLog(5, "AppleUSBHub[%p]::ConfigureHubDriver - found kAssumePortsAreCaptive (%d)", this, _treatAllPortsAsCaptive);
    }

    locationIDProperty = (OSNumber *)_device->getProperty("ExtraPowerRequest");
    if ( locationIDProperty )
    {
        _hasExtraPowerRequest = true;
		USBLog(5, "AppleUSBHub[%p]::ConfigureHubDriver - found ExtraPowerRequest", this);
    }
    
	locationIDProperty = (OSNumber *) _device->getProperty(kUSBDevicePropertyLocationID);
	if ( locationIDProperty )
	{
		_locationID = locationIDProperty->unsigned32BitValue();
	}

    // Go ahead and configure the hub
    //
    err = ConfigureHub();
    
    if ( err == kIOReturnSuccess )
    {		
		registerService();					// for the benefit of a user client
		        
        // allocate a thread_call structure
        _workThread = thread_call_allocate((thread_call_func_t)ProcessStatusChangedEntry, (thread_call_param_t)this);
        _resetPortZeroThread = thread_call_allocate((thread_call_func_t)ResetPortZeroEntry, (thread_call_param_t)this);
        _hubDeadCheckThread = thread_call_allocate((thread_call_func_t)CheckForDeadHubEntry, (thread_call_param_t)this);
        _clearFeatureEndpointHaltThread = thread_call_allocate((thread_call_func_t)ClearFeatureEndpointHaltEntry, (thread_call_param_t)this);
        _checkForActivePortsThread = thread_call_allocate((thread_call_func_t)CheckForActivePortsEntry, (thread_call_param_t)this);
        _waitForPortResumesThread = thread_call_allocate((thread_call_func_t)WaitForPortResumesEntry, (thread_call_param_t)this);
        _ensureUsabilityThread = thread_call_allocate((thread_call_func_t)EnsureUsabilityEntry, (thread_call_param_t)this);
        _initialDelayThread = thread_call_allocate((thread_call_func_t)InitialDelayEntry, (thread_call_param_t)this);
        _hubResetPortThread = thread_call_allocate((thread_call_func_t)HubResetPortAfterPowerChangeDoneEntry, (thread_call_param_t)this);
        
        if ( !_workThread || !_resetPortZeroThread || !_hubDeadCheckThread || !_clearFeatureEndpointHaltThread || !_checkForActivePortsThread || !_waitForPortResumesThread || !_ensureUsabilityThread || !_initialDelayThread || !_hubResetPortThread)
        {
            USBError(1, "AppleUSBHub[%p]::ConfigureHubDriver  could not allocate all thread functions.  Aborting ConfigureHubDriver", this);
            goto ErrorExit;
        }
		
		// Get the other errata that are not personality based
		//
		_errataBits = GetHubErrataBits();
		setProperty("Errata", _errataBits, 32);
		
		
		return true;
    }
    
ErrorExit:
		
	// We need to stop the port objects...
		
	USBError(1,"AppleUSBHub[%p]::ConfigureHubDriver  Aborting startup for hub @ 0x%x,  error 0x%x (%s)", this, (uint32_t)_locationID, err, USBStringFromReturn(err));
	StopPorts();
	
	stop(_device);
	
	if ( _timerSource )
	{
		if ( _workLoop )
			_workLoop->removeEventSource(_timerSource);
		
		_timerSource->release();
		_timerSource = NULL;
	}
	
    if (_gate)
    {
		if (_workLoop)
			_workLoop->removeEventSource(_gate);
	    
		_gate->release();
		_gate = NULL;
    }
	
    if ( _workLoop )
    {
        _workLoop->release();
        _workLoop = NULL;
    }

	if ( _device && _device->isOpen(this) )
		_device->close(this);
	
	return false;
}



void 
AppleUSBHub::stop(IOService * provider)
{

    if (_buffer) 
    {
        _buffer->release();
		_buffer = NULL;
    }
	
	if (_hsHub)
	{
		if (_bus)
			_bus->RemoveHSHub(_address);
	}
	
    if (_hubInterface) 
    {
		int	retries = 500;
		
		// Wait for up to 5 seconds if we have a thread outstanding 
		while (_checkPortsThreadActive && (--retries > 0))
		{
			USBLog(1, "AppleUSBHub[%p]::stop  _checkPortsThreadActive is active, sleeping 10ms (%d)", this, retries);
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
            {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHub[%p]::stop - IOSleep in gate:%d", this, 1);}}
#endif
			IOSleep(10);
		}
		
		if (_interruptPipe)
		{
			_interruptPipe->release();
			_interruptPipe = NULL;
		}
		
		_hubInterface->close(this);
        _hubInterface->release();
        _hubInterface = NULL;
    }
    
    if (_workThread)
    {
        thread_call_cancel(_workThread);
        thread_call_free(_workThread);
		_workThread = 0;
    }
    
    if (_resetPortZeroThread)
    {
        thread_call_cancel(_resetPortZeroThread);
        thread_call_free(_resetPortZeroThread);
		_resetPortZeroThread = 0;
    }

    if (_hubDeadCheckThread)
    {
        thread_call_cancel(_hubDeadCheckThread);
        thread_call_free(_hubDeadCheckThread);
		_hubDeadCheckThread = 0;
    }

    if (_clearFeatureEndpointHaltThread)
    {
        thread_call_cancel(_clearFeatureEndpointHaltThread);
        thread_call_free(_clearFeatureEndpointHaltThread);
		_clearFeatureEndpointHaltThread = 0;
    }
	
    if (_checkForActivePortsThread)
    {
        thread_call_cancel(_checkForActivePortsThread);
        thread_call_free(_checkForActivePortsThread);
		_checkForActivePortsThread = 0;
    }
	
    if (_waitForPortResumesThread)
    {
        thread_call_cancel(_waitForPortResumesThread);
        thread_call_free(_waitForPortResumesThread);
		_waitForPortResumesThread = 0;
    }
	
    if (_ensureUsabilityThread)
    {
        thread_call_cancel(_ensureUsabilityThread);
        thread_call_free(_ensureUsabilityThread);
		_ensureUsabilityThread = 0;
    }
	
    if (_initialDelayThread)
    {
        thread_call_cancel(_initialDelayThread);
        thread_call_free(_initialDelayThread);
		_initialDelayThread = 0;
    }

    if (_hubResetPortThread)
    {
        thread_call_cancel(_hubResetPortThread);
        thread_call_free(_hubResetPortThread);
		_hubResetPortThread = 0;
    }
	
	if (_workLoop)
		_workLoop->removeEventSource(_gate);

	if ( _workLoop )
		_workLoop->removeEventSource(_timerSource);
	
    USBLog(6, "AppleUSBHub[%p]::stop - calling PMstop", this);
	PMstop();
	
    USBLog(6, "AppleUSBHub[%p]::stop - calling super::stop - retainCount(%d)", this, getRetainCount());

    super::stop(provider);
}



IOReturn 
AppleUSBHub::message( UInt32 type, IOService * provider,  void * argument )
{
#pragma unused (provider)
    IOReturn						err = kIOReturnSuccess;
    IOUSBHubPortStatus				status;
    IOUSBHubPortReEnumerateParam *	params ;
    IOUSBHubPortClearTTParam *      ttParams;
    
	USBLog(6, "AppleUSBHub[%p]::message(%p)(%s) - isInactive(%s) _myPowerState(%d) _powerStateChangingTo(%d)", this, (void*)type, HubMessageToString(type), isInactive() ? "true" : "false", (int)_myPowerState, (int)_powerStateChangingTo);
	
    switch ( type )
    {
		case kIOUSBMessageHubIsDeviceConnected:
		
			// If we are in the process of terminating, or if we have determined that the hub is dead, then
			// just return as if the device was unplugged.  The hub itself is going away or is already resetting,
			// so it any device that was connected will not be anymore.
			//
			if ( isInactive() || _hubIsDead )
			{
				USBLog(3,"AppleUSBHub[%p] : got kIOUSBMessageHubIsDeviceConnected while isInactive() or _hubIsDead", this);
				err = kIOReturnNoDevice;
				break;
			}
			
			// Get the status for the port.  Note that the argument passed into this method is the port number.  If we get an
			// error from the call, then that means that our hub is not 100% so the device is probably not connected.  Otherwise
			// check the kHubPortConnection bit to see whether there is a device connected to the port or not
			//
			USBLog(6, "AppleUSBHub[%p]::message(kIOUSBMessageHubIsDeviceConnected) - calling IncrementOutstandingIO", this);
			IncrementOutstandingIO();
			err = GetPortStatus(&status, * (UInt32 *) argument );
			if ( err != kIOReturnSuccess )
			{
				err = kIOReturnNoDevice;
			}
			else
			{
				USBLog(5,"AppleUSBHub[%p]::kIOUSBMessageHubIsDeviceConnected - port %d - status(%8x)/change(%8x), _ignoreDisconnectOnWakeup = %d", this, * (uint32_t *) argument, status.statusFlags, status.changeFlags, _ignoreDisconnectOnWakeup);
				if ( (status.statusFlags & kHubPortConnection) && (_ignoreDisconnectOnWakeup || !(status.changeFlags & kHubPortConnection)) )
				{
					if ( _ignoreDisconnectOnWakeup )
					{
						USBLog(6, "AppleUSBHub[%p]::message(kIOUSBMessageHubIsDeviceConnected) - _ignoreDisconnectOnWakeup is true", this);
					}
					err = kIOReturnSuccess;
				}
				else
					err = kIOReturnNoDevice;
			}
			USBLog(6, "AppleUSBHub[%p]::message(kIOUSBMessageHubIsDeviceConnected) - calling DecrementOutstandingIO", this);
			DecrementOutstandingIO();
			break;
		
		case kIOUSBMessageHubSuspendPort:
		case kIOUSBMessageHubResumePort:
			USBLog(1, "AppleUSBHub[%p]::message - sending obsolete message(%s)", this, HubMessageToString(type));
			break;
			
		case kIOUSBMessageHubResetPort:
			USBLog(1, "AppleUSBHub[%p]::message(%s) - this message is obsolete.  Please use the IOUSBDevice API", this, HubMessageToString(type));
			EnsureUsability();
			err = DoPortAction( type, * (UInt32 *) argument, 0 );
			break;
			
		case kIOUSBMessageHubReEnumeratePort:
			USBLog(1, "AppleUSBHub[%p]::message(%s) - this message is obsolete.  Please use the IOUSBDevice API", this, HubMessageToString(type));
			params = (IOUSBHubPortReEnumerateParam *) argument;
			EnsureUsability();
			err = DoPortAction( type, params->portNumber, params->options );
			break;
			
			// this should only go through DoPortAction if we are a HS hub. Otherwise, short circuit it
		case kIOUSBMessageHubPortClearTT:
			if ( _hsHub )
			{
				ttParams = (IOUSBHubPortClearTTParam *) argument;
				err = DoPortAction( type, ttParams->portNumber, ttParams->options );
			}
			else
			{
				err = kIOReturnUnsupported;
			}
			break;
		
		// this does not need to go through DoPortAction, because it just sets an iVar in the port structure
		case kIOUSBMessageHubSetPortRecoveryTime:
			{
				// this uses the same params as ReEnumeratePort
				params = (IOUSBHubPortReEnumerateParam *) argument;
				AppleUSBHubPort *port = _ports ? _ports[params->portNumber-1] : NULL;
				if (port)
				{
					USBLog(5, "AppleUSBHub[%p]::message(kIOUSBMessageHubSetPortRecoveryTime) - port %d, setting _portResumeRecoveryTime to %d", this, (uint32_t)params->portNumber, (uint32_t)params->options);
					port->_portResumeRecoveryTime = params->options;
				}
			}
			break;
			
		case kIOMessageServiceIsTerminated: 	
			USBLog(3,"AppleUSBHub[%p] : Received kIOMessageServiceIsTerminated - ignoring", this);
			break;
			
			
		case kIOUSBMessagePortHasBeenReset:
			// We don't pay attention to this message, since ResetDevice is now more synchronous.
			break;
			
		case kIOUSBMessagePortHasBeenResumed:
		case kIOUSBMessagePortWasNotSuspended:
			_portSuspended = false;
			_abortExpected = false;
			USBLog(5, "AppleUSBHub[%p]: received kIOUSBMessagePortHasBeenResumed or kIOUSBMessagePortWasNotSuspended (%p) - _myPowerState(%d) - ensuring usability", this, (void*)type, (int)_myPowerState);
			_needInterruptRead = true;
			EnsureUsability();
			break;

        // Handle message from UHCI. See AppleUSBUHCI::RestoreControllerStateFromSleep().
        case kIOUSBMessageRootHubWakeEvent:
			{
                AppleUSBHubPort *   port = NULL;
                UInt32              portIndex = (UInt32) (uintptr_t)argument;
				UInt32				portNum = portIndex+1;

                if (portIndex < _hubDescriptor.numPorts)
                    port = _ports ? _ports[portIndex] : NULL;

                if (port && !port->_suspendChangeAlreadyLogged)
                {
					port->_suspendChangeAlreadyLogged = true;
					if (port->_portDevice)
					{
						IOLog("The USB device %s (Port %d of Hub at 0x%x) may have caused a wake by issuing a remote wakeup (2)\n", (port->_portDevice)->getName(), (int)portNum, (uint32_t)_locationID);
					}
					else 
					{
						IOLog("An unknown USB device (Port %d of Hub at 0x%x) may have caused a wake by issuing a remote wakeup\n", (int)portNum, (uint32_t)_locationID);
					}

                }
            }
            break;

        case kIOUSBMessageHubPortDeviceDisconnected:
            {
                char *muxMethod = (char*)argument;
                USBLog(5, "AppleUSBHub[%p]: received kIOUSBMessageHubPortDeviceDisconnected hub @ 0x%x (%s)", this, (uint32_t)_locationID, _device ? "NULL" : _device->getName());               
                _device->GetBus()->retain();
                _device->GetBus()->message(type, this, muxMethod);
                _device->GetBus()->release();
            }
            break;
            
		default:
			break;
		
    }
    
    return err;
}



bool 
AppleUSBHub::finalize(IOOptionBits options)
{
    return(super::finalize(options));
}



bool 	
AppleUSBHub::requestTerminate( IOService * provider, IOOptionBits options )
{
	int			retries = 10;
	
    USBLog(3, "AppleUSBHub[%p]::requestTerminate - _myPowerState(%d) _powerStateChangingTo(%d)", this, (int)_myPowerState, (int)_powerStateChangingTo);
	// let's be ON to terminate
	if ((_myPowerState != kIOUSBHubPowerStateOn) || (_powerStateChangingTo != kIOUSBHubPowerStateStable))
		changePowerStateTo(kIOUSBHubPowerStateOn);

	if (_myPowerState > kIOUSBHubPowerStateOff)
	{
		while ((_myPowerState != kIOUSBHubPowerStateOn) && (_powerStateChangingTo != kIOUSBHubPowerStateStable) && (retries-- > 0))
		{
			USBLog(3, "AppleUSBHub[%p]::requestTerminate - still waiting - _myPowerState(%d) _powerStateChangingTo(%d) retries left(%d)", this, (int)_myPowerState, (int)_powerStateChangingTo, retries);
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
            {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHub[%p]::requestTerminate - IOSleep in gate:%d", this, 1);}}
#endif
			IOSleep(100);
		}
	}
	
    return super::requestTerminate(provider, options);
}



bool
AppleUSBHub::willTerminate( IOService * provider, IOOptionBits options )
{
    IOReturn				err;
    int						portIndex, portNum;
    AppleUSBHubPort *		port;
    
    USBLog(3, "AppleUSBHub[%p]::willTerminate isInactive = %d", this, isInactive());
	
    if ( _interruptPipe )
    {
		err = _interruptPipe->Abort();
        if ( err != kIOReturnSuccess )
        {
            USBLog(1, "AppleUSBHub[%p]::willTerminate interruptPipe->Abort returned 0x%x", this, err);
			USBTrace( kUSBTHub,  kTPHubWillTerminate, (uintptr_t)this, (uintptr_t)provider, err, 1 );
        }
    }
	
    // JRH 09/19/2003 rdar://problem/3290312
    // make sure that none of our ports has the dev zero lock held. if they do, it is safe to go ahead
    // and release it since the hub is now gone (we are terminating)
    if ( _ports)
    {
        for (portIndex = 0; portIndex < _hubDescriptor.numPorts; portIndex++)
        {
			portNum = portIndex + 1;
 			port = _ports ? _ports[portIndex] : NULL;
            if (port)
            {
                if (port->_devZero)
                {
                    USBLog(1, "AppleUSBHub[%p]::willTerminate - port %d had the dev zero lock", this, portNum);
					USBTrace( kUSBTHub,  kTPHubWillTerminate, (uintptr_t)this, portNum, portIndex, port->_devZero );
                }
                port->ReleaseDevZeroLock();
				if (port->_portPMState == usbHPPMS_pm_suspended)
				{
 					IOUSBControllerV3		*v3Bus = NULL;

					USBLog(5, "AppleUSBHub[%p]::willTerminate - port %d was suspended by the power manager - re-enabling the endpoints", this, portNum);

					if (_device)
						v3Bus = OSDynamicCast(IOUSBControllerV3, _bus);
					
					if (v3Bus && port->_portDevice)
					{
						USBLog(5, "AppleUSBHub[%p]::willTerminate - Enabling endpoints for device at address (%d)", this, (int)port->_portDevice->GetAddress());
						err = v3Bus->EnableAddressEndpoints(port->_portDevice->GetAddress(), true);
						if (err)
						{
							USBLog(5, "AppleUSBHub[%p]::willTerminate - EnableAddressEndpoints returned (%p)", this, (void*)err);
						}
					}
					port->_portPMState = usbHPPMS_active;
				}
            }
        }
    }
	
    // We are going to be terminated, so clean up! Make sure we don't get any more status change interrupts.
    // Note that if we are terminated before we set up our interrupt pipe, then we better not call it!
    //
    if (_timerSource) 
    {
		_timerSource->cancelTimeout();
    }
    
    USBLog(3, "AppleUSBHub[%p]::willTerminate - calling super::willTerminate", this);
    return super::willTerminate(provider, options);
}



bool
AppleUSBHub::didTerminate( IOService * provider, IOOptionBits options, bool * defer )
{
    USBLog(3, "AppleUSBHub[%p]::didTerminate isInactive[%s] _outstandingIO[%d]", this, isInactive() ? "true" : "false", (int)_outstandingIO);
    
    if (!_outstandingIO && (_powerStateChangingTo == kIOUSBHubPowerStateStable) && !_doPortActionLock)
    {
		USBLog(5, "AppleUSBHub[%p]::didTerminate - closing _device(%p)", this, _device);
		// Stop/close all ports, deallocate our ports
		//
		StopPorts();
		_device->close(this);
    }
    else
    {
		USBLog(5, "AppleUSBHub[%p]::didTerminate - _device(%p) - setting needToClose - _outstandingIO(%d) _powerStateChangingTo(%d)", this, _device, (int)_outstandingIO, (int)_powerStateChangingTo);
		_needToClose = true;
    }
    USBLog(6, "AppleUSBHub[%p]::didTerminate isInactive = %d - retainCount(%d) - calling superclass", this, isInactive(), getRetainCount());
    return super::didTerminate(provider, options, defer);
}



bool
AppleUSBHub::terminate( IOOptionBits options )
{
    USBLog(6, "AppleUSBHub[%p]::terminate isInactive = %d - retainCount(%d)", this, isInactive(), getRetainCount());
    return super::terminate(options);
}


void
AppleUSBHub::free( void )
{
	USBLog(6, "AppleUSBHub[%p]::free", this);
	if (_timerSource)
    {
        _timerSource->release();
        _timerSource = NULL;
    }
	
    if (_gate)
    {
        _gate->release();
        _gate = NULL;
    }
    
	if (_workLoop)
	{
		_workLoop->release();
		_workLoop = NULL;
	}
	
    super::free();
}



bool
AppleUSBHub::terminateClient( IOService * client, IOOptionBits options )
{
    USBLog(3, "AppleUSBHub[%p]::terminateClient isInactive = %d", this, isInactive());
    return super::terminateClient(client, options);
}



IOReturn
AppleUSBHub::powerStateWillChangeTo ( IOPMPowerFlags capabilities, unsigned long stateNumber, IOService* whatDevice)
{
	IOReturn	ret;
	
	USBLog(5, "AppleUSBHub[%p]::powerStateWillChangeTo - from State(%d) to state(%d) _needInterruptRead (%s) _outstandingIO(%d) isInactive(%s)", this, (int)_myPowerState, (int)stateNumber, _needInterruptRead ? "true" : "false", (int)_outstandingIO, isInactive() ? "true" : "false");
	ret = super::powerStateWillChangeTo( capabilities, stateNumber, whatDevice);
	
	// if we are going to off, restart, or sleep, we need to wait for any status change threads to complete (up to 30 seconds)
	if (_powerStateChangingTo < kIOUSBHubPowerStateOn)
	{
		int		retries = 300;
		
		_abandonCheckPorts = true;											// make sure that we aren't trying to check our ports at this point

		// kill my interrupt pipe, since my upstream hub will suspend me
		if ( _interruptPipe )
		{
			USBLog(5, "AppleUSBHub[%p]::powerStateWillChangeTo - aborting pipe", this);
			_abortExpected = true;
			_needInterruptRead = false;
			_interruptPipe->Abort();
			USBLog(5, "AppleUSBHub[%p]::powerStateWillChangeTo - done aborting pipe", this);
		}

		while (retries-- && (IsPortInitThreadActiveForAnyPort() || IsStatusChangedThreadActiveForAnyPort() || _checkPortsThreadActive))
		{
			USBLog(retries ? 5 : 1, "AppleUSBHub[%p]::powerStateWillChangeTo - an init thread or status changed thread or checkPorts thread is still active for some port - waiting 100ms (retries=%d)", this, retries);
			USBTrace( kUSBTHub,  kTPHubPowerStateWillChangeTo, (uintptr_t)this, retries, _powerStateChangingTo, kIOUSBHubPowerStateLowPower );
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
            {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHub[%p]::powerStateWillChangeTo - IOSleep in gate:%d", this, 1);}}
#endif
			IOSleep(100);
		}
	}
	
	USBLog(5, "AppleUSBHub[%p]::powerStateWillChangeTo - DONE from State(%d) to state(%d) _needInterruptRead (%s) _outstandingIO(%d) isInactive(%s) ret(%p)", this, (int)_myPowerState, (int)stateNumber, _needInterruptRead ? "true" : "false", (int)_outstandingIO, isInactive() ? "true" : "false", (void*)ret);
	return ret;
}



IOReturn		
AppleUSBHub::setPowerState ( unsigned long powerStateOrdinal, IOService* whatDevice )
{
	// 5654850 - this used to be done in powerStateWillChangeTo. however, that was too early, so we moved it to HubPowerChange, which is the call
	// which gets made from the superclass when a setPowerState comes in.
	// however, the superclass implementation will short-circuit that call if we are being terminated, so now we do it just before
	// the superclass::setPowerState. this makes it work much better when a hub in low power mode gets disconnected
	if (_myPowerState != powerStateOrdinal)
	{
		USBLog(6, "AppleUSBHub[%p]::setPowerState - calling IncrementOutstandingIO", this);
		IncrementOutstandingIO(true);				// this will be decremented in powerStateDidChangeTo
	}
	
	return super::setPowerState(powerStateOrdinal, whatDevice);
}



IOReturn		
AppleUSBHub::powerStateDidChangeTo ( IOPMPowerFlags capabilities, unsigned long stateNumber, IOService* whatDevice)
{	
	unsigned long	oldState = _myPowerState;
	IOReturn		ret;
	
	USBLog(5, "AppleUSBHub[%p]::+powerStateDidChangeTo - from State(%d) to state(%d) _needInterruptRead (%s)", this, (int)oldState, (int)stateNumber, _needInterruptRead ? "true" : "false");
	ret =  super::powerStateDidChangeTo( capabilities, stateNumber, whatDevice);

	// this needs to be done here instead of in powerChangeDone, because powerChangeDone won't get called until our children are all up, and 
	// one of our children might need an interrupt read to occur, which won't occur until this Decrement occurs
	if (oldState != stateNumber)
	{
		USBLog(6, "AppleUSBHub[%p]::powerStateDidChangeTo - calling DecrementOutstandingIO", this);
		DecrementOutstandingIO(true);				// this is from the increment in setPowerState
	}
		
	USBLog(5, "AppleUSBHub[%p]::-powerStateDidChangeTo - from State(%d) to state(%d) _needInterruptRead (%s) ret[%p]", this, (int)oldState, (int)stateNumber, _needInterruptRead ? "true" : "false", (void*)ret);
	return ret;
}



void
AppleUSBHub::powerChangeDone ( unsigned long fromState)
{
    super::powerChangeDone(fromState);
	USBLog((fromState == _myPowerState) ? 7 : 5, "AppleUSBHub[%p]::powerChangeDone - device(%s) from State(%d) to state(%d) _needInterruptRead (%s)", this, _device->getName(), (int)fromState, (int)_myPowerState, _needInterruptRead ? "true" : "false");
	
    
	if (isInactive())
	{
        if (_outstandingIOFromPowerChange)
        {
            USBLog(2, "AppleUSBHub[%p]::powerChangeDone - have an outstanding IO from a power change - calling DecrementOutstandingIO", this);
            DecrementOutstandingIO(true);
        }
        else if (_needToClose)
        {
            // tickle the outstanding io - just in case
            IncrementOutstandingIO();
            DecrementOutstandingIO();
        }
	}
	
	if ((_myPowerState == kIOUSBHubPowerStateOn) and _waitingForPowerOn)
	{
		USBLog(5,"AppleUSBHub[%p]::powerChangeDone - calling WakeOnPowerOn", this);
		WakeOnPowerOn();
	}
	
	// it is possible that we "deferred" a check ports call while we were in the middle of a power change
	// since we have already called our superclass, _myPowerState is now stable, as we should go ahead 
	// and see if we need to check the ports again (as long as we are ON now)
	if (!isInactive() && !_hubIsDead && !_checkPortsThreadActive && (_myPowerState == kIOUSBHubPowerStateOn))
	{
		_abandonCheckPorts = false;
		_checkPortsThreadActive = true;
		retain();											// in case we get terminated while the thread is still waiting to be scheduled
		USBLog(5,"AppleUSBHub[%p]::powerChangeDone - spawning _checkForActivePortsThread", this);
		if ( thread_call_enter(_checkForActivePortsThread) == TRUE )
		{
			USBLog(1,"AppleUSBHub[%p]::powerChangeDone - _checkForActivePortsThread already queued", this);
			release();
		}
	}

	// Check flag and call reset and then turn on again and do override
	if ( _needToCallResetDevice and (_myPowerState == kIOUSBHubPowerStateOff) and (not isInactive()) )
	{
		_needToCallResetDevice = false;
	
		USBLog(5, "AppleUSBHub[%p]::powerChangeDone - calling HubResetPortAfterPowerChangeDone thread", this);
		USBTrace( kUSBTHub,  kTPHubPowerChangeDone, (uintptr_t)this, fromState, _myPowerState, kIOUSBHubPowerStateOff );
		
		retain();
		if (thread_call_enter(_hubResetPortThread) == TRUE)
		{
			USBLog(1, "AppleUSBHub[%p]::powerChangeDone - _hubResetPortThread already queued - UNEXPECTED!", this);
			release();
		}

		
	}
}



#pragma mark ееееееее Configuration ееееееее
IOReturn
AppleUSBHub::ConfigureHub()
{
    IOReturn							err = kIOReturnSuccess;
    IOUSBFindInterfaceRequest			req;
    const IOUSBConfigurationDescriptor	*cd;
	OSBoolean							*expressCardCantWakeRef;
	int									tryCount = 4;				// initial try + 3 retries
	UInt32								hsFlags = 0;
	UInt8								hubTTThinkTime;

	USBLog(3,"AppleUSBHub[%p]::ConfigureHub", this);
    // Reset some of our variables that so that when we reconfigure due to a reset
    // we don't reuse old values
    //
    _busPowerGood = false;
    _powerForCaptive = 0;
    _numCaptive = 0;
	_retryCount = kHubDriverRetryCount;
	
    // Find the first config/interface
    if (_device->GetNumConfigurations() < 1)
    {
        USBError(1,"AppleUSBHub[%p]::ConfigureHub(hub @ 0x%x) No hub configurations", this, (uint32_t)_locationID);
        err = kIOReturnNoResources;		// Need better error
        goto ErrorExit;
    }

	// Let's try to configure the hub.  Frst we need to get the config descriptor and then set the configuration.  If we
	// have trouble doing, we will retry and even reset the hub.  We have discovered some hubs that are finicky.
	do
	{
		// If this is our first time around, we don't wait.
		if ( tryCount < 4 )
        {
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
            {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHub[%p]::ConfigureHub - IOSleep in gate:%d", this, 1);}}
#endif
			IOSleep(100);
		}
		// Try to get the config descriptor
		cd = _device->GetFullConfigurationDescriptor(0);
		if (!cd)
		{
			USBError(1,"AppleUSBHub[%p]::ConfigureHub(hub @ 0x%x) No config descriptor, retrying", this, (uint32_t)_locationID);

#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
            {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHub[%p]::ConfigureHub - IOSleep in gate:%d", this, 2);}}
#endif
			IOSleep( 300 );
			cd = _device->GetFullConfigurationDescriptor(0);
			if ( !cd )
			{
				// OK, we did not get the config descriptor in our 2 tries.  Let's reset the hub and try again.  Give up after trying 5 times, as it probably 
				// indicates that the hub is muy dead
				
				if ( _hubGetConfigResetRetries++ < 6 )
				{
					USBError(1,"AppleUSBHub[%p]::ConfigureHub(hub @ 0x%x) No config descriptor, after retry, resetting the port (%d)", this, (uint32_t)_locationID, (uint32_t)_hubGetConfigResetRetries);
					retain();
					ResetMyPort();
					release();
					
					// Indicate that we were succesful so the hub driver loads.  However, the reset will come and reconfigure us
					err = kIOReturnSuccess;
				}
				else
				{
					USBError(1,"AppleUSBHub[%p]::ConfigureHub(hub @ 0x%x) No config descriptor, after retry, and resetting 5 times", this, (uint32_t)_locationID);
					err = kIOUSBConfigNotFound;
				}
				
				break;
			}
		}

		// Set the configuration to the first value in the descriptor
		err = _device->SetConfiguration(this, cd->bConfigurationValue, false);
		
		// We will retry if we get a pipestall.  Any other error or success, we just bail otut of the loop
		if ( err != kIOUSBPipeStalled)
		{
			USBLog( err == kIOReturnSuccess ? 7 : 1,"AppleUSBHub[%p]::ConfigureHub(hub @ 0x%x) SetConfiguration returned 0x%x", this, (uint32_t)_locationID, err);
			break;
		}
	}
	while ( --tryCount > 0 && (err == kIOUSBPipeStalled) );
	        
	// If we did not get a config descriptor or we failed the SetConfig, bail
	if ( !cd || err != kIOReturnSuccess )
	{
	 	goto ErrorExit;
	}
	
#ifdef SUPPORTS_SS_USB
	if ( !_isRootHub && (_device->GetBus()->GetControllerSpeed() == kUSBDeviceSpeedSuper))
	{
		// 9/23/11 (rdar://10175323):  We have seen SS Hubs that disconnect on a resume, so for now, don't allow them to suspend
		// 01/12/12 (rdar: //10385717: We want to allow doze and disable the PLL when nothing is attached, so don't do this for the 2 XHCI root hubs)
		USBLog(3,"AppleUSBHub[%p]::ConfigureHub(hub @ 0x%x)  we have a hub attached to a SS Controller (SS/HS), dont allow low power", this, (uint32_t)_locationID);
		_dontAllowLowPower = true;
	}
	
	if(_device->GetProtocol() == 3)	// Superspeed hub
	{
		_ssHub = true;
		
		if (!_isRootHub)
		{
			UInt32	realLocationID = _locationID & 0xff7FFFFF;
			UInt8   depth = calculateDepth(realLocationID) - 1;
			USBLog(3,"AppleUSBHub[%p]::ConfigureHub(hub @ 0x%x)  we have a SuperSpeed Hub with hub depth of %d", this, (uint32_t)_locationID, depth);
			err = SetHubDepth(depth);							// Hub depth is zero based on first external hub
			if ( err != kIOReturnSuccess )
			{
				USBLog(3,"AppleUSBHub[%p]::ConfigureHub(hub @ 0x%x)  SetHubDepth returned 0x%x, bailing!", this, (uint32_t)_locationID, err);
				goto ErrorExit;
			}
		}
	}
#endif
	
    // Set the remote wakeup feature if it's supported. 
#ifdef SUPPORTS_SS_USB
    // Technically this should only be done for non-SS hubs, but it looks like some of them do, so leave this for now.
#endif
	
    if (cd->bmAttributes & kUSBAtrRemoteWakeup)
    {
        USBLog(3,"AppleUSBHub[%p]::ConfigureHub(hub @ 0x%x)  Setting kUSBFeatureDeviceRemoteWakeup for Hub device (%p)", this, (uint32_t)_locationID, _device);
        err = _device->SetFeature(kUSBFeatureDeviceRemoteWakeup);
        if ( err)
		{
            USBError(1,"AppleUSBHub[%p]::ConfigureHub(hub @ 0x%x)  SetFeature(kUSBFeatureDeviceRemoteWakeup) failed. Error 0x%x", this, (uint32_t)_locationID, err);
		}
    }

	// See if this is an express card device which would disconnect on sleep (thus waking everytime)
	//
	_hubWithExpressCardPort = HasExpressCardPort();
	expressCardCantWakeRef = OSDynamicCast( OSBoolean, _device->getProperty(kUSBExpressCardCantWake) );
	if ( expressCardCantWakeRef && expressCardCantWakeRef->isTrue() )
	{
		USBLog(3, "%s[%p](%s) found an express card device which will disconnect across sleep (hub @ 0x%x)", getName(), this, _device->getName(), (uint32_t)_locationID );
		_device->GetBus()->retain();
		_device->GetBus()->message(kIOUSBMessageExpressCardCantWake, this, _device);
		_device->GetBus()->release();
	}
	
	// Find the interface for our hub -- there's only one
    //
    req.bInterfaceClass = kUSBHubClass;
    req.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
    req.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
    req.bAlternateSetting = kIOUSBFindInterfaceDontCare;
    _hubInterface = _device->FindNextInterface(NULL, &req);
	if (_hubInterface  == 0)
    {
        USBError(1,"AppleUSBHub[%p]::ConfigureHub(hub @ 0x%x)  no interface found, trying again", this, (uint32_t)_locationID);
		
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
        {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHub[%p]::ConfigureHub - IOSleep in gate:%d", this, 3);}}
#endif
		IOSleep(100);
		req.bInterfaceClass = kUSBHubClass;
		req.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
		req.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
		req.bAlternateSetting = kIOUSBFindInterfaceDontCare;
		_hubInterface = _device->FindNextInterface(NULL, &req);
		if (_hubInterface  == 0)
		{
			USBError(1,"AppleUSBHub[%p]::ConfigureHub(hub @ 0x%x)  no interface found", this, (uint32_t)_locationID);
			err = kIOUSBInterfaceNotFound;
			goto ErrorExit;
		}
    }
    
    _hubInterface->retain();
    
    
    _busPowered = (cd->bmAttributes & kUSBAtrBusPowered) ? TRUE : FALSE;	//FIXME
    _selfPowered = (cd->bmAttributes & kUSBAtrSelfPowered) ? TRUE : FALSE;

    if ( !(_busPowered || _selfPowered) )
    {
        USBError(1,"AppleUSBHub[%p]::ConfigureHub(hub @ 0x%x)  illegal device config - no power", this, (uint32_t)_locationID);
        err = kIOReturnNoPower;		// Need better error code here.
        goto ErrorExit;
    }

    // Get the hub descriptor
    if ( (err = GetHubDescriptor(&_hubDescriptor)) )
    {
        USBError(1,"AppleUSBHub[%p]::ConfigureHub(hub @ 0x%x)  could not get hub descriptor (0x%x)", this, (uint32_t)_locationID, err);
        goto ErrorExit;
    }
    
    if (_hubDescriptor.numPorts < 1)
    {
        USBLog(1,"AppleUSBHub[%p]::ConfigureHub(hub @ 0x%x)  there are no ports on this hub", this, (uint32_t)_locationID);
		USBTrace( kUSBTHub,  kTPHubConfigureHub, (uintptr_t)this, _hubDescriptor.numPorts, 0, 1 );
    }
	
    if (_hubDescriptor.numPorts > 64)
    {
        USBLog(3,"AppleUSBHub[%p]::ConfigureHub(hub @ 0x%x)  hub reports %d ports, but we support up to 64, setting to 64", this, (uint32_t)_locationID, _hubDescriptor.numPorts);
		_hubDescriptor.numPorts = 64;
    }
	
    if (_hubDescriptor.numPorts > 15)
    {
        USBLog(3,"AppleUSBHub[%p]::ConfigureHub(hub @ 0x%x)  there are an awful lot of ports (%d) on this hub", this, (uint32_t)_locationID, _hubDescriptor.numPorts);
    }
    _readBytes = (_hubDescriptor.numPorts / 8) + 1;

    hsFlags |= (_hubDescriptor.numPorts << kUSBHSHubFlagsNumPortsShift);
	
	hubTTThinkTime = ((_hubDescriptor.characteristics & kHubTTThinkTimeMask) >> kHubTTThinkTimeShift);
	
	hsFlags |= (hubTTThinkTime << kUSBHSHubFlagsTTThinkTimeShift);
	hsFlags |= kUSBHSHubFlagsMoreInfoMask;										// these fields are now valid
	
    // Setup for reading status pipe
    _buffer = IOBufferMemoryDescriptor::withCapacity(_readBytes, kIODirectionIn);

    if (!_hubInterface->open(this))
    {
        USBError(1," AppleUSBHub[%p]::ConfigureHub(hub @ 0x%x)  could not open hub interface", this, (uint32_t)_locationID);
        err = kIOReturnNotOpen;
        goto ErrorExit;
    }
    
    // after opening the interface, but before we get the pipe, we need to see if this is a 2.0
    // capabale hub, and if so, we need to set the multiTT status if possible
    _multiTTs = false;
    _hsHub = false;
    
    if (_device->GetbcdUSB() >= 0x200)
    {
		if (_bus)
		{
			switch (_device->GetProtocol())
			{
				case 0:
					USBLog(5, "AppleUSBHub[%p]::ConfigureHub(hub @ 0x%x) - found FS/LS only hub", this, (uint32_t)_locationID);
					break;
					
				case 1:
					USBLog(5, "AppleUSBHub[%p]::ConfigureHub(hub @ 0x%x) - found single TT hub", this, (uint32_t)_locationID);
					_bus->AddHSHub(_address, hsFlags);
					_hsHub = true;
					break;
					
				case 2:
					USBLog(5, "AppleUSBHub[%p]::ConfigureHub(hub @ 0x%x)  - found multi TT hub", this, (uint32_t)_locationID);
					_hsHub = true;

					if ((err = _hubInterface->SetAlternateInterface(this, 1))) 		// pick the multi-TT setting
					{
						USBError(1, "AppleUSBHub[%p]::ConfigureHub(hub @ 0x%x) - err (%x) setting alt interface", this, (uint32_t)_locationID, err);
						_bus->AddHSHub(_address, hsFlags);
					}
					else
						_bus->AddHSHub(_address, hsFlags | kUSBHSHubFlagsMultiTTMask);
					
					_multiTTs = true;
					break;
					
#ifdef SUPPORTS_SS_USB
				case 3:
						USBLog(5, "AppleUSBHub[%p]::ConfigureHub(hub @ 0x%x) - found SuperSpeed  hub", this, (uint32_t)_locationID);
						break;
#endif					
				default:
					USBError(1, "AppleUSBHub[%p]::ConfigureHub(hub @ 0x%x) - unknown protocol (%d)", this, (uint32_t)_locationID, _device->GetProtocol());
					break;
			}
		}
		else
		{
			USBLog(5, "AppleUSBHub[%p]::ConfigureHub(hub @ 0x%x) - not on a V2 controller", this, (uint32_t)_locationID);
		}
    }
    

    IOUSBFindEndpointRequest request;
    request.type = kUSBInterrupt;
    request.direction = kUSBIn;
    _interruptPipe = _hubInterface->FindNextPipe(NULL, &request);

    if (!_interruptPipe)
    {
        USBError(1,"AppleUSBHub[%p]::ConfigureHub(hub @ 0x%x)  could not find interrupt pipe", this, (uint32_t)_locationID);
        err = kIOUSBNotEnoughPipesErr;		// Need better error code here.
        goto ErrorExit;
    } 
    
	_interruptPipe->retain();
	
	// Some USB3 hubs do support the USB3 wakeup.  Some don't.  For now, just enabled it for VIA hubs.
#ifdef SUPPORTS_SS_USB
	if (_ssHub && _ignoreDisconnectOnWakeup)
	{
		USBLog(4,"AppleUSBHub[%p]::ConfigureHub(hub @ 0x%x)  SetPortRemoteWakeMask to 0x3", this, (uint32_t)_locationID);
		// In order to enable remote wakeup on Super Speed hubs, we need to set the wakeup mask to 0x3 (bit 0 is connection wakeup, bit 1 is disconnect wakeup
		// bit 2 is overcurrent wakeup).  We don't set the overcurrent because some hubs issue phantom wakes
		for (int currentPort = 1; currentPort <= _hubDescriptor.numPorts; currentPort++)
		{
			err = SetPortRemoteWakeMask(0x03, currentPort);
			if (err != kIOReturnSuccess)
			{
				USBLog(1,"AppleUSBHub[%p]::ConfigureHub(hub @ 0x%x)  SetPortRemoteWakeMask port %d returned 0x%x", this, (uint32_t)_locationID, currentPort, (uint32_t) err);
			}
		}
		
	}
#endif
	
    // prepare the ports
    UnpackPortFlags();
    CountCaptivePorts();
    err = CheckPortPowerRequirements();
    if ( err != kIOReturnSuccess )
    {
        USBError(1,"AppleUSBHub[%p]::ConfigureHub(hub @ 0x%x)  CheckPortPowerRequirements failed with 0x%x", this,(uint32_t)_locationID, err);
        goto ErrorExit;
    }

    err = AllocatePortMemory();
    if ( err != kIOReturnSuccess )
    {
        USBError(1,"AppleUSBHub[%p]::ConfigureHub(hub @ 0x%x)  AllocatePortMemory failed with 0x%x", this, err, (uint32_t)_locationID);
        goto ErrorExit;
    }
    
    if (_hsHub)
    {
		// with a HS hub, we will put a property in our own object specifying the number
		// of TTs in the hub. 
		if (!_multiTTs)
			setProperty("High Speed", (unsigned long long)1, 8);		// 8 bits
		else
			setProperty("High Speed", (unsigned long long)_hubDescriptor.numPorts, 8);	// 8 bits
    }
    
	// Set the UserClient for all hubs, hs and full speed
	setProperty("IOUserClientClass", "AppleUSBHSHubUserClient");

	_hubIsDead = FALSE;

ErrorExit:

    return err;
}


/**********************************************************************
 **
 ** HUB FUNCTIONS
 **
 **********************************************************************/
void 
AppleUSBHub::UnpackUSB2PortFlags(void)
{
#ifdef SUPPORTS_SS_USB
    int i;
	IOUSBHubDescriptor *desc;

    int numFlags = (_hubDescriptor.numPorts / 8) + 1;

	// USB2 Hub descriptor has to be unpacked into the USB3 descriptor
	// which is how the descriptor is stored.
	
	desc = (IOUSBHubDescriptor *)&_hubDescriptor;
	for(i = kNumPortBytes-1; i >= 0; i--)
	{
		_hubDescriptor.pwrCtlPortFlags[i] = desc->pwrCtlPortFlags[i];
	}
	for(i = kNumPortBytes-1; i >= 0; i--)
	{
		_hubDescriptor.removablePortFlags[i] = desc->removablePortFlags[i];
	}
	_hubDescriptor.hubHdrDecLat = 0;
	_hubDescriptor.hubDelay = 0;
		
	// Then unpack the port flags as before
    for(i = 0; i < numFlags; i++)
    {
        _hubDescriptor.pwrCtlPortFlags[i] = _hubDescriptor.removablePortFlags[numFlags+i];
        _hubDescriptor.removablePortFlags[numFlags+i] = 0;
    }
#else
	int i;
	
    int numFlags = (_hubDescriptor.numPorts / 8) + 1;
    for(i = 0; i < numFlags; i++)
    {
        _hubDescriptor.pwrCtlPortFlags[i] = _hubDescriptor.removablePortFlags[numFlags+i];
        _hubDescriptor.removablePortFlags[numFlags+i] = 0;
    }
#endif
}


#ifdef SUPPORTS_SS_USB
void 
AppleUSBHub::UnpackUSB3PortFlags(void)
{
    int i;
	
    int numFlags = (_hubDescriptor.numPorts / 8) + 1;
    for(i = 0; i < numFlags; i++)
    {
		// No one should be looking at these, they were deprecated in the 2.0 spec
        _hubDescriptor.pwrCtlPortFlags[i] = 0xff;
    }
}
#endif


void 
AppleUSBHub::UnpackPortFlags(void)
{
#ifdef SUPPORTS_SS_USB
	if(_hubDescriptor.hubType == kUSB3HUBDesc)
	{
		UnpackUSB3PortFlags();
	}
	else
#endif
	{
		UnpackUSB2PortFlags();
	}
}



void 
AppleUSBHub::CountCaptivePorts(void)
{
    int 		portMask = 2;
    int 		portByte = 0;
    int			currentPort;


    for (currentPort = 1; currentPort <= _hubDescriptor.numPorts; currentPort++)
    {
        /* determine if the port is a captive port */
        if ((_hubDescriptor.removablePortFlags[portByte] & portMask) != 0)
            _numCaptive++;		// save this for power calculations

        portMask <<= 1;
        if (portMask > 0x80)
        {
            portMask = 1;
            portByte++;
        }
    }
}



/*
                ExtPower   ExtPower
                Good       off

Bus  Self

0     0     	Illegal config
1     0     	Always 100mA per port
0     1     	500mA     0 (dead)
1     1     	500       100
*/

IOReturn 
AppleUSBHub::CheckPortPowerRequirements(void)
{
    IOReturn	err = kIOReturnSuccess;
    /* Note hub current in units of 1mA, everything else in units of 2mA */
    UInt32	hubPower = _hubDescriptor.hubCurrent/2;
    UInt32	busPower = _device->GetBusPowerAvailable();
    UInt32	powerAvailForPorts = 0;
    UInt32	powerNeededForPorts = 0;
#ifdef SUPPORTS_SS_USB
	UInt32  standardPowerForPort = _ssHub ? kUSB3MaxPowerPerPort: kUSB2MaxPowerPerPort;
	UInt32  minumumPowerForPort = _ssHub ? kUSB150mAAvailable : kUSB100mAAvailable;
#else
	UInt32  standardPowerForPort = kUSB2MaxPowerPerPort;
	UInt32  minumumPowerForPort = kUSB100mAAvailable;
#endif
    bool	startExternal;
	OSNumber *	hubWakePowerReserved;
	
	USBLog(3, "AppleUSBHub[%p]::CheckPortPowerRequirements  Hub @ 0x%x with %d ports, device is %s(%p), speed is: %d, standardPowerForPort: %d", this, (uint32_t)_locationID, (uint32_t)_hubDescriptor.numPorts, _device->getName(), _device, _device->GetSpeed(), (uint32_t)standardPowerForPort);
	_device->setProperty("Ports", _hubDescriptor.numPorts, 32);

    do
    {
        if (hubPower > busPower)
        {
            // Don't put up an alert here.  The Adaptec 2.0 Hub claims that it needs 250mA. We will catch this later
            //
            USBLog(3, "AppleUSBHub[%p]::CheckPortPowerRequirements  Hub @ 0x%x claims to need more power (%d > %d) than available", this, (uint32_t)_locationID, (uint32_t)hubPower, (uint32_t)busPower);
            _busPowerGood = false;
            _powerForCaptive = 0;
        }
        else
        {
            powerAvailForPorts = busPower - hubPower;

#ifdef SUPPORTS_SS_USB
			// we minimally need make available 100mA (USB2) or 150mA (USB3) per non-captive port
#endif
            powerNeededForPorts = (_hubDescriptor.numPorts - _numCaptive) * minumumPowerForPort;
            _busPowerGood = (powerAvailForPorts >= powerNeededForPorts);

			USBLog(6, "AppleUSBHub[%p]::CheckPortPowerRequirements  Hub @ 0x%x  powerAvailForPorts: %d, powerNeededForPorts: %d, _numCaptive: %d, _busPowerGood: %s", this, (uint32_t)_locationID, (uint32_t)powerAvailForPorts, (uint32_t)powerNeededForPorts, (uint32_t)_numCaptive, _busPowerGood ? "TRUE" : "FALSE");
			
          	 if (_numCaptive > 0)
            {
                if (_busPowerGood)
                    _powerForCaptive =  (powerAvailForPorts - powerNeededForPorts) / _numCaptive;
                else
                    _powerForCaptive = powerAvailForPorts / _numCaptive;
            }

            if ( (_errataBits & kErrataCaptiveOKBit) != 0)
                _powerForCaptive = minumumPowerForPort;
 
			USBLog(3, "AppleUSBHub[%p]::CheckPortPowerRequirements  Hub @ 0x%x _busPowerGood = %s, _numCaptive: %d, _powerForCaptive: %d", this, (uint32_t)_locationID, _busPowerGood ? "TRUE" : "FALSE", (uint32_t)_numCaptive, (uint32_t) _powerForCaptive);
		}
        
        _selfPowerGood = false;
        
        if (_selfPowered)
        {
            // Check the status of the power source
            //
            USBStatus	status = 0;
            IOReturn	localErr;

            _powerForCaptive = minumumPowerForPort;

            localErr = GetDeviceStatus(&status);
            if ( localErr != kIOReturnSuccess )
            {
                USBLog(3,"AppleUSBHub[%p]::CheckPowerPowerRequirements   Hub @ 0x%x GetDeviceStatus returned 0x%x", this, (uint32_t)_locationID, (uint32_t)localErr);
                err = localErr;
                break;
            }
            
			// Check to make sure that the device reports that it is self powered
            status = USBToHostWord(status);
            _selfPowerGood = ((status & 1) != 0);
			if ( !_selfPowerGood )
			{
                USBLog(1,"AppleUSBHub[%p]::CheckPowerPowerRequirements   Hub @ 0x%x attached - reported to be self-powered, but the device reports it is NOT self powered", this, (uint32_t)_locationID);
			}
        }

        if (_selfPowered && _busPowered)
        {
            /* Dual power hub */
            
            if (_selfPowerGood)
            {
                USBLog(3,"AppleUSBHub[%p]::CheckPowerPowerRequirements - Hub @ 0x%x attached - Self/Bus powered, power supply good", this, (uint32_t)_locationID);
				if (!_isRootHub && !_dontAllowSleepPower)
				{
					OSObject *	anObj = _device->copyProperty(kAppleStandardPortCurrentInSleep);
					if ( anObj == NULL )
					{
						USBLog(5,"AppleUSBHub[%p]::CheckPowerPowerRequirements(hub @ 0x%x) -  setting kAppleStandardPortCurrentInSleep to %d", this, (uint32_t)_locationID, (uint32_t) standardPowerForPort);
						
#ifdef SUPPORTS_SS_USB
						// self powered hubs can provide extra power in sleep - standard spec power of 500/900 ma per port
#endif
						_device->setProperty(kAppleStandardPortCurrentInSleep, standardPowerForPort, 32);
					}
					else
					{
						anObj->release();
						USBLog(3,"AppleUSBHub[%p]::CheckPowerPowerRequirements(hub @ 0x%x) -  device already had a kAppleStandardPortCurrentInSleep property", this, (uint32_t)_locationID);
					}
				}
			}
            else
            {
                USBLog(3,"AppleUSBHub[%p]::CheckPowerPowerRequirements   Hub @0x%x attached - Self/Bus powered, no external power", this, (uint32_t)_locationID);
            }
        }
        else
        {
            /* Single power hub */
            if (_selfPowered)
            {
                if (_selfPowerGood)
                {
                    USBLog(3,"AppleUSBHub[%p]::CheckPowerPowerRequirements -  Hub @ 0x%x attached - Self powered, power supply good", this, (uint32_t)_locationID);
					
					if (!_isRootHub && !_dontAllowSleepPower)
					{
						OSObject *	anObj = _device->copyProperty(kAppleStandardPortCurrentInSleep);
						if ( anObj == NULL )
						{
							USBLog(5,"AppleUSBHub[%p]::CheckPowerPowerRequirements(hub @ 0x%x) -  setting kAppleStandardPortCurrentInSleep to %d", this, (uint32_t)_locationID, (uint32_t) standardPowerForPort);
							// self powered hubs can provide extra power in sleep - 500 ma per port
							_device->setProperty(kAppleStandardPortCurrentInSleep, standardPowerForPort, 32);
						}
						else
						{
							anObj->release();
							USBLog(3,"AppleUSBHub[%p]::CheckPowerPowerRequirements(hub @ 0x%x) -  device already had a kAppleStandardPortCurrentInSleep property", this, (uint32_t)_locationID);
						}
					}
				}
                else
                {
                    USBLog(3,"AppleUSBHub[%p]::CheckPowerPowerRequirements Hub @ 0x%x attached - Self powered, no external power", this, (uint32_t)_locationID);
                }
            }
            else
            {
                USBLog(3,"AppleUSBHub[%p]::CheckPowerPowerRequirements  Hub @ 0x%x attached - Bus powered", this, (uint32_t)_locationID);
            }
			
        }

		if (_isRootHub && (_device->GetHubCharacteristics() & kIOUSBHubDeviceCanSleep))
		{
			// Only do it if the property is not there
			OSObject *	anObj = _device->copyProperty(kAppleStandardPortCurrentInSleep);
			if ( anObj == NULL )
			{
				USBLog(5,"AppleUSBHub[%p]::CheckPowerPowerRequirements(hub @ 0x%x) -  setting kAppleStandardPortCurrentInSleep for root hub to %d", this, (uint32_t)_locationID, (uint32_t) standardPowerForPort);
				_device->setProperty(kAppleStandardPortCurrentInSleep, standardPowerForPort, 32);
			}
			else
			{
				anObj->release();
			}
		}

        startExternal = (_busPowerGood || _selfPowerGood);
        if ( !startExternal )
        {	/* not plugged in or bus powered on a bus powered hub */
            err = kIOReturnNoPower;
            _device->DisplayUserNotification(kUSBNotEnoughPowerNotificationType);
			
            IOLog("USB Low Power Notice:  The hub \"%s\" at 0x%x cannot be used because there is not enough power for all its ports (%d, %d  %d, %d)\n", _device->getName(), (uint32_t)_locationID, _busPowerGood, _selfPowerGood, (uint32_t)powerAvailForPorts, (uint32_t)powerNeededForPorts);
            USBLog(1,"AppleUSBHub[%p]: insufficient power to turn on ports for hub @ 0x%x  (%d, %d  %d, %d)", this, (uint32_t)_locationID, _busPowerGood, _selfPowerGood, (uint32_t)powerAvailForPorts, (uint32_t)powerNeededForPorts);
			USBTrace( kUSBTHub, kTPHubCheckPowerRequirements, (uintptr_t)this, kIOReturnNoPower, 0, 0 );
			
            if (!_busPowered)
            {
                /* may be able to turn on compound devices */
                break;	/* Now what ?? */
            }
        }
		
		hubWakePowerReserved = OSDynamicCast(OSNumber, _device->getProperty("HubWakePowerReserved"));
		if ( hubWakePowerReserved && _selfPowerGood )
		{
			UInt32	wakePowerReserved = hubWakePowerReserved->unsigned32BitValue();
			
			USBLog(3,"AppleUSBHub[%p]::CheckPowerPowerRequirements(hub @ 0x%x) -  returning %d power because we actually have a self-powered hub", this, (uint32_t)wakePowerReserved, (uint32_t)_locationID);
			_device->IOUSBDevice::ReturnExtraPower(kUSBPowerDuringWake, wakePowerReserved);
			wakePowerReserved = 0;
			_device->setProperty("HubWakePowerReserved", wakePowerReserved, 32);
		}
		
		
    } while (false);

    return err;
}



IOReturn 
AppleUSBHub::AllocatePortMemory(void)
{
    AppleUSBHubPort			*port;
    UInt32					power;
    UInt32					portMask = 2;
    UInt32					portByte = 0;
    UInt32					currentPort;
    bool					captive;
	bool					hasExternalConnector = false;
    AppleUSBHubPort 	 	**cachedPorts;

#ifdef SUPPORTS_SS_USB
    bool                    muxed = false;
    bool                    controllerMuxed = false;
    char                    methodName[kIOUSBMuxMethodNameLength];
#endif
    
#ifdef SUPPORTS_SS_USB
	UInt32  				minumumPowerForPort = _ssHub ? kUSB150mAAvailable : kUSB100mAAvailable;
	UInt32  				maximumPowerForPort = _ssHub ? kUSB900mAAvailable : kUSB500mAAvailable;
#else
	UInt32  				minumumPowerForPort = kUSB100mAAvailable;
	UInt32  				maximumPowerForPort = kUSB500mAAvailable;
#endif

    cachedPorts = (AppleUSBHubPort **) IOMalloc(sizeof(AppleUSBHubPort *) * _hubDescriptor.numPorts);

    if (!cachedPorts)
        return kIOReturnNoMemory;

#ifdef SUPPORTS_SS_USB
    // We want to get mux methods for EHCI Roothubs/Built-in hubs DS of EHCI only
    ControllerHasMuxedPorts(&controllerMuxed);
#endif
    
    for (currentPort = 1; currentPort <= _hubDescriptor.numPorts; currentPort++)
    {
#ifdef SUPPORTS_SS_USB
        if(controllerMuxed) 
        {
            GetACPIPortMuxProperties(currentPort, &muxed, methodName);
        }
#endif
        
        /* determine if the port is a captive port */
        if ((_hubDescriptor.removablePortFlags[portByte] & portMask) != 0)
        {
			USBLog(5, "AppleUSBHub[%p]::AllocatePortMemory - locationID(0x%x) port (%d) marked as captive", this, (int)_locationID, (int)currentPort);
            power = _selfPowerGood ? maximumPowerForPort : _powerForCaptive;
            captive = true;
			hasExternalConnector = false;
        }
        else
        {
			USBLog(5, "AppleUSBHub[%p]::AllocatePortMemory - locationID(0x%x) port (%d) not marked as captive - calling GetACPIPortCaptiveProperties", this, (int)_locationID, (int)currentPort);
            power = _selfPowerGood ? maximumPowerForPort : minumumPowerForPort;
			GetACPIPortCaptiveProperties(currentPort, &captive, &hasExternalConnector);
        }

		
        port = new AppleUSBHubPort;

#ifdef SUPPORTS_SS_USB
		USBLog(5, "AppleUSBHub[%p]::AllocatePortMemory - locationID(0x%x) Initializing port %d(%p) with power: %d, captive: %s, hasExternalConnector: %s muxed: %s %s", this, (int)_locationID, (int)currentPort, port, (uint32_t)power, captive ? "YES" : "NO", hasExternalConnector ? "YES" : "NO"
               , muxed ? "YES" : "NO", muxed ? methodName : "");
#else
		USBLog(5, "AppleUSBHub[%p]::AllocatePortMemory - locationID(0x%x) Initializing port %d(%p) with power: %d, captive: %s, hasExternalConnector: %s", this, (int)_locationID, (int)currentPort, port, (uint32_t)power, captive ? "YES" : "NO", hasExternalConnector ? "YES" : "NO");
#endif            

#ifdef SUPPORTS_SS_USB
		if (port->init(self, currentPort, power, captive, hasExternalConnector, muxed, methodName) != kIOReturnSuccess)
#else
		if (port->init(self, currentPort, power, captive, hasExternalConnector) != kIOReturnSuccess)
#endif            
        {
            port->release();
            cachedPorts[currentPort-1] = NULL;
        }
        else
		{
            cachedPorts[currentPort-1] = port;
			
			// If we have an external connector to the enclosure, then update the hub device with the info
			if (hasExternalConnector )
			{
				_device->UpdateUnconnectedExternalPorts(1);
			}
		}
		
        portMask <<= 1;
        if (portMask > 0x80)
        {
            portMask = 1;
            portByte++;
        }
    }
    
    _ports = cachedPorts;
    return kIOReturnSuccess;
}



#pragma mark ееее Power Management ееее
IOReturn		
AppleUSBHub::HubPowerChange(unsigned long powerStateOrdinal)
{
	IOReturn		err = kIOReturnSuccess;
	UInt32			retries;
	IOReturn		ret = kIOPMAckImplied;
	
	USBLog(5, "AppleUSBHub[%p]::HubPowerChange - powerStateOrdinal(%d) _needInterruptRead(%s) isInactive(%s)", this, (int)powerStateOrdinal, _needInterruptRead ? "true" : "false", isInactive() ? "true" : "false");
	
	if ( (_myPowerState == kIOUSBHubPowerStateSleep) && ( powerStateOrdinal > kIOUSBHubPowerStateSleep) )
	{
		USBLog(6, "AppleUSBHub[%p]::HubPowerChange - We are waking up _myPowerState(%d), powerStateOrdinal(%d)", this, (uint32_t)_myPowerState, (uint32_t)powerStateOrdinal);
		
		// Set the "wakeup time"
		clock_get_uptime(&_wakeupTime);
	}
	
	switch (powerStateOrdinal)
	{
		case kIOUSBHubPowerStateOn:
	
			if (_portSuspended && _device)
			{
				USBLog(5, "AppleUSBHub[%p]::HubPowerChange - hub (0x%x) going to ON - my port is suspended, waking up onThread(%s)", this, (uint32_t)_locationID, _workLoop->onThread() ? "true" : "false");
				_device->SuspendDevice(false);
				retries = 20;
				do {
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
                    {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHub[%p]::HubPowerChange - IOSleep in gate:%d", this, 1);}}
#endif
					IOSleep(20);
				} while (_portSuspended && (retries-- > 0));
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
                {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHub[%p]::HubPowerChange - IOSleep in gate:%d", this, 2);}}
#endif
				IOSleep(_hubResumeRecoveryTime);			// 10 more ms for the recovery time before I start talking to my device
				if (retries == 0)
				{
					USBError(1, "AppleUSBHub[%p]::HubPowerChange - hub (0x%x) going to ON - my port is suspended, could not wake it up!  onThread(%s)", this, (uint32_t)_locationID, _workLoop->onThread() ? "true" : "false");
				}
			}
			if (!_ports)
			{
				err = AllocatePortMemory();
				if (err)
				{
					USBError(1,"AppleUSBHub[%p]::HubPowerChange - AllocatePortMemory failed with %p", this, (void*)err);
				}
			}
			// power ON (or unsuspend) the ports
			if (_myPowerState < kIOUSBHubPowerStateLowPower)
			{
				if (_device && (_myPowerState < kIOUSBHubPowerStateSleep) && IsHubDeviceInternal() && _needsInoculation)
				{
					// 11309709: we need to do a SUSPEND/RESUME sequence on this hub to "inoculate" it from a potential remote wakeup issue
					// we tested various amounts of time for the SUSPEND, and settled on 30ms which is 10 times the amount of time that the device
					// needs to know that it is SUSPENDed. This value could probably be as little as 3-4 ms, although it will be followed by a 20ms
					// RESUME and then a 10ms hub recovery time, so the total with a 30ms SUSPEND is about 60ms.
					// Note that this happens during the power change from OFF -> ON, so the downstream ports are disabled at this time, and the 
					// downstream port drivers will be loaded after all of this occurs (See StartPorts() immediately after this block of code)
					IOReturn	err;
					err = _device->SuspendDevice(true);									
					if (err == kIOReturnSuccess)
					{
						IOSleep(30);
						err = _device->SuspendDevice(false);
						if (err == kIOReturnSuccess)
						{

#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
							{if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHub[%p]::HubPowerChange - IOSleep in gate:%d", this, 2);}}
							USBLog(1, "AppleUSBHub[%p]::HubPowerChange - inoculated hub @ 0x%x", this, (int)_locationID);
							setProperty("inoculated", (unsigned long long)++_inoculatedCount, 32);
#endif
							IOSleep(_hubResumeRecoveryTime);			// 10 more ms for the recovery time before I start talking to my device
						}
						else
						{
							USBError(1, "AppleUSBHub[%p]::HubPowerChange - Inoculation RESUME failed with 0x%x", this, err);
						}
					}
					else
					{
						USBError(1, "AppleUSBHub[%p]::HubPowerChange - Inoculation SUSPEND failed with 0x%x", this, err);
					}
				}
				err = StartPorts();
				if ( err != kIOReturnSuccess )
				{
					USBError(1,"AppleUSBHub[%p]::HubPowerChange - StartPorts failed with 0x%x", this, err);
					break;
				}
			}
			// Start the timeout Timer
			if (_timerSource)
			{
				_timerSource->setTimeoutMS(kWatchdogTimerPeriod); 
			}	
			
			if ( !_isRootHub && _interruptPipe)
				_interruptPipe->ClearPipeStall(true);

			_abortExpected = false;
			_needInterruptRead = true;
			break;
			
		case kIOUSBHubPowerStateLowPower:

			if ( _dontAllowLowPower )
			{
				USBLog(1, "AppleUSBHub[%p]::HubPowerChange - hub going to doze - but this hub does NOT allow it", this);
				USBTrace( kUSBTHub, kTPHubHubPowerChange, (uintptr_t)this, _portSuspended, _myPowerState, 1 );
				break;
			}
			if (_myPowerState > kIOUSBHubPowerStateLowPower)				// don't do anything if we are coming from a lower state
			{
				if (_portSuspended)
				{
					USBLog(1, "AppleUSBHub[%p]::HubPowerChange ((hub @ 0x%x) - hub going to doze - my port is suspended! UNEXPECTED", this, (uint32_t)_locationID);
					USBTrace( kUSBTHub, kTPHubHubPowerChange, (uintptr_t)this, _locationID, 0, 2 );
					break;
				}
				if (!HubAreAllPortsDisconnectedOrSuspended())
				{
					UInt32					portIndex, portNum;
					IOUSBHubPortStatus		portStatus;
					bool					realError = false;

					for (portIndex = 0; portIndex < _hubDescriptor.numPorts; portIndex++)
					{
						IOReturn				kr;
						
						portNum = portIndex + 1;
						kr = GetPortStatus(&portStatus, portNum);
						if ( kr == kIOReturnSuccess )
						{
							if ( (portStatus.changeFlags == 0) && (portStatus.statusFlags & kHubPortConnection) && (portStatus.statusFlags & kHubPortEnabled) && !(portStatus.statusFlags & kHubPortSuspend))
							{
								USBTrace( kUSBTHub, kTPHubHubPowerChange, _locationID, (uint32_t)portNum, portStatus.statusFlags, portStatus.changeFlags);
								if ( !_isRootHub )
								{
									USBLog(1, "AppleUSBHub[%p](0x%x)::HubPowerChange - port %d (status: 0x%x change: 0x%x) connected, enabled and not suspended - UNEXPECTED", this, (uint32_t)_locationID, (uint32_t)portNum, portStatus.statusFlags, portStatus.changeFlags);
									realError = true;
								}
								else 
								{
									// If it's a root hub, don't set realError to true so that we end up calling EnsureUsability()
									USBError(1, "AppleUSBHub[%p](0x%x)::HubPowerChange - RootHub port %d (status: 0x%x change: 0x%x) connected, enabled and not suspended - UNEXPECTED", this, (uint32_t)_locationID, (uint32_t)portNum, portStatus.statusFlags, portStatus.changeFlags);
								}

								break;
							}
						}
					}
					if (!realError)
					{
						USBLog(2, "AppleUSBHub[%p]::HubPowerChange (hub @ 0x%x) - hub going to doze - some port has a recent status change - we will pick it up later.  Calling EnsureUsability()", this, (uint32_t)_locationID);
						EnsureUsability();
					}
				}
				else
				{
					// Proceed to suspend the port
					// We need to suspend our port.  If we have I/O pending, set a flag that tells the interrupt handler
					// routine that we don't need to rearm the read.
					//
					// Now, call in to suspend the port
					if ( _device && !_isRootHub)
					{
						USBLog(5, "AppleUSBHub[%p]::HubPowerChange - calling SuspendDevice", this);
						err = _device->SuspendDevice(true);
						USBLog(5, "AppleUSBHub[%p]::HubPowerChange - done with SuspendDevice", this);
						if ( err == kIOReturnSuccess )
						{
							_portSuspended = true;
						}
						else
						{
							USBLog(1, "AppleUSBHub[%p]::HubPowerChange SuspendDevice returned %p", this, (void*)err );
							USBTrace( kUSBTHub, kTPHubHubPowerChange, (uintptr_t)this, err, 0, 3 );
						}
					}
					else
					{
						USBLog(5, "AppleUSBHub[%p]::HubPowerChange _isRootHub or _device was NULL", this );
					}
				}
			}
			_needInterruptRead = false;
			break;
			
		case kIOUSBHubPowerStateSleep:
			if (_hubIsDead)
			{
				USBLog(1, "AppleUSBHub[%p]::HubPowerChange - hub is dead, so just acknowledge the HubPowerChange", this);
				USBTrace( kUSBTHub, kTPHubHubPowerChange, (uintptr_t)this, kIOUSBHubPowerStateSleep, 0, 4);
				break;
			}
		
			if (!(_device->GetHubCharacteristics() & kIOUSBHubDeviceCanSleep))
			{
				USBLog(1, "AppleUSBHub[%p]::HubPowerChange - hub doesn't support sleep, don't do anything on Suspend", this);
				USBTrace( kUSBTHub, kTPHubHubPowerChange, (uintptr_t)this, _device->GetHubCharacteristics(), kIOUSBHubDeviceCanSleep, 5);
				break;
			}
			// first kill the timer so we don't notice that the ports are suspended
			if (_timerSource) 
			{
				_timerSource->cancelTimeout();
			}
			
#ifdef USE_GLOBAL_SUSPEND
			// only do this stuff if I am not already suspended
			if (!_portSuspended)
			{
				if (_isRootHub)
				{
					USBError(1,"AppleUSBHub[%p]::HubPowerChange - Suspending root hub ports (device: %s)", this, _device->getName());
				// I need to suspend each of my downstream ports
					err = SuspendPorts();
					if ( err != kIOReturnSuccess )
					{
						USBError(1,"AppleUSBHub[%p]::HubPowerChange - SuspendPorts failed with 0x%x", this, err);
						break;
					}
				}
			}
#else
			// only do this stuff if I am not already suspended
			if (!_portSuspended)
			{
				// I need to suspend each of my downstream ports
				err = SuspendPorts();
				if ( err != kIOReturnSuccess )
				{
					USBError(1,"AppleUSBHub[%p]::HubPowerChange - SuspendPorts failed with 0x%x", this, err);
					break;
				}
								
				// Proceed to suspend the upstream port
				// We need to suspend our port.  If we have I/O pending, set a flag that tells the interrupt handler
				// routine that we don't need to rearm the read.
				//
				// Now, call in to suspend the port
				if ( _device && !_isRootHub)
				{
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
                    {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHub[%p]::HubPowerChange - IOSleep in gate:%d", this, 1);}}
#endif
					IOSleep(25);					// 7332243: make sure to wait > 20 ms after suspending all downstream ports before suspending the upstream
					USBLog(5, "AppleUSBHub[%p]::HubPowerChange - calling SuspendDevice", this);
					err = _device->SuspendDevice(true);
					USBLog(5, "AppleUSBHub[%p]::HubPowerChange - done with SuspendDevice", this);
					if ( err == kIOReturnSuccess )
					{
						_portSuspended = true;
					}
					else
					{
						USBLog(1, "AppleUSBHub[%p]::HubPowerChange SuspendDevice returned %p", this, (void*)err );
						USBTrace( kUSBTHub, kTPHubHubPowerChange, (uintptr_t)this, err, kIOUSBHubPowerStateSleep, 6);
					}
				}
				else
				{
					USBLog(5, "AppleUSBHub[%p]::HubPowerChange _isRootHub or _device was NULL - not suspending", this );
				}
			}
#endif
			else
			{
				USBLog(5, "AppleUSBHub[%p]::HubPowerChange - device already suspended - no need to do it again", this );
			}
			_needInterruptRead = false;
			break;
			
		case kIOUSBHubPowerStateOff:
		case kIOUSBHubPowerStateRestart:
			// Stop/close all ports, deallocate our ports
			//			
			StopPorts();
			
			_needInterruptRead = false;
			break;
		
		default:
			USBError(1, "AppleUSBHub[%p]::HubPowerChange - unknown ordinal (%d)", this, (int)powerStateOrdinal);
	}
	USBLog(5, "AppleUSBHub[%p]::HubPowerChange - done _needInterruptRead(%s) _outstandingResumes(%d)", this, _needInterruptRead ? "true" : "false", (int)_outstandingResumes);
	if (_outstandingResumes)
	{
		USBLog(5, "AppleUSBHub[%p]::HubPowerChange - with outstanding resumes - spawning thread and returning kIOPMWillAckLater", this);
		_needToAckSetPowerState = true;
		IncrementOutstandingIO();
		retain();
		if (thread_call_enter(_waitForPortResumesThread) == TRUE)
		{
			USBLog(1,"AppleUSBHub[%p]::HubPowerChange - _waitForPortResumesThread already queued", this);
			USBTrace( kUSBTHub, kTPHubHubPowerChange, (uintptr_t)this, 0, kIOPMWillAckLater, 7);
			DecrementOutstandingIO();
			release();
		}
		ret = kIOPMWillAckLater;
	}
	return ret;
}



bool	
AppleUSBHub::HubAreAllPortsDisconnectedOrSuspended()
{
    UInt32					portIndex, portNum;
    IOUSBHubPortStatus		portStatus;
	IOReturn				kr;
    AppleUSBHubPort			*port;
	bool					returnValue = true;
    
	USBLog(7, "AppleUSBHub[%p]::HubAreAllPortsDisconnectedOrSuspended (0x%x) - _myPowerState(%d), _hubDescriptor.numPorts = %d", this, (uint32_t)_locationID, (int)_myPowerState, (uint32_t)_hubDescriptor.numPorts);
	
	if (!_ports)
	{
		USBLog(3, "AppleUSBHub[%p]::HubAreAllPortsDisconnectedOrSuspended - no _ports - returning false", this);
		return false;
	}
	
	
	if (_hubHasBeenDisconnected || isInactive() || _hubIsDead || (_myPowerState < kIOUSBHubPowerStateLowPower))
	{
		USBLog(3, "AppleUSBHub[%p]::HubAreAllPortsDisconnectedOrSuspended - hub has been disconnected or is inActive or dead or sleeping - returning false", this);
		return false;
	}
	
	for (portIndex = 0; portIndex < _hubDescriptor.numPorts; portIndex++)
	{
		portNum = portIndex + 1;
		if (!_ports)
		{
			// I will make this a USBLog(3 after some testing
			USBLog(1, "AppleUSBHub[%p]::HubAreAllPortsDisconnectedOrSuspended - the _ports went away - bailing", this);
			USBTrace( kUSBTHub, kTPHubAreAllPortsDisconnectedOrSuspended, (uintptr_t)this, portNum, _hubDescriptor.numPorts, 1);
			returnValue = false;
			break;
		}
		port = _ports ? _ports[portIndex] : NULL;
		if (!port)
		{
			USBLog(3, "AppleUSBHub[%p]::HubAreAllPortsDisconnectedOrSuspended - no port at index (%d) - returning false", this, (int)portIndex);
			returnValue = false;
			break;
		}
		if ((port->_initThreadActive) || (port->_statusChangedThreadActive) || (port->_addDeviceThreadActive))
		{
			USBLog(3, "AppleUSBHub[%p]::HubAreAllPortsDisconnectedOrSuspended - port %d still initing, status changing, or adding a device (%d/%d/%d)", this, (uint32_t)portNum, port->_initThreadActive, port->_statusChangedThreadActive, port->_addDeviceThreadActive);
			returnValue = false;
			break;
		}
		kr = GetPortStatus( &portStatus, portNum);
		if ( kr == kIOReturnSuccess )
		{
			USBLog(7, "AppleUSBHub[%p]::HubAreAllPortsDisconnectedOrSuspended - _ports(%p) port(%d) status(%p) change(%p) portDevice(%p)", this, _ports, (uint32_t)portNum, (void*)portStatus.statusFlags, (void*)portStatus.changeFlags, port->_portDevice);
			// do we need to check and re-enable port power? - not quite sure..
			if ((portStatus.statusFlags & kHubPortPower) == 0)
			{
				USBLog(2, "AppleUSBHub[%p](0x%x)::HubAreAllPortsDisconnectedOrSuspended - port %d is not powered on! portPMState=%d  Thread should be enabling it (%d)", this, (uint32_t)_locationID, (uint32_t)portNum, port->_portPMState, port->_enablePowerAfterOvercurrentThreadActive);
				USBTrace( kUSBTHub, kTPHubAreAllPortsDisconnectedOrSuspended, _locationID, portNum, port->_portPMState, 2);
			}
			if ( ((portStatus.statusFlags & kHubPortConnection) && !(portStatus.statusFlags & kHubPortSuspend)) || (portStatus.changeFlags))
			{
				USBLog(7, "AppleUSBHub[%p](0x%x)::HubAreAllPortsDisconnectedOrSuspended - port %d enabled and not suspended or there is a change", this, (uint32_t)_locationID, (uint32_t)portNum);
				returnValue = false;
				break;
			}
			if (portStatus.statusFlags & kHubPortOverCurrent)
			{
				USBLog(5, "AppleUSBHub[%p](0x%x)::HubAreAllPortsDisconnectedOrSuspended - port %d as an active overcurrent condition. returning FALSE", this, (uint32_t)_locationID, (uint32_t)portNum);
				returnValue = false;
				break;
			}
            
#ifdef SUPPORTS_SS_USB
            if ( (portStatus.statusFlags & kHubPortDebouncing) && _isRootHub && _device && (_device->GetBus()->GetControllerSpeed() == kUSBDeviceSpeedSuper))
			{
				USBLog(5, "AppleUSBHub[%p](0x%x)::HubAreAllPortsDisconnectedOrSuspended - port %d is a XHCI Root Hub with a pending connection, returning FALSE", this, (uint32_t)_locationID, (uint32_t)portNum);
				returnValue = false;
				break;
			}
#endif
		}
		else
		{
			USBLog(1,"AppleUSBHub[%p](0x%x)::HubAreAllPortsDisconnectedOrSuspended  GetPortStatus for port %d returned 0x%x", this, (uint32_t)_locationID, (uint32_t)portNum, kr);
			USBTrace( kUSBTHub, kTPHubAreAllPortsDisconnectedOrSuspended, _locationID, portNum, kr, 3);
			returnValue = false;
			break;
		}
	}		
	
Exit:

	USBLog(7, "AppleUSBHub[%p](0x%x)::HubAreAllPortsDisconnectedOrSuspended - returning (%s)", this, (uint32_t)_locationID, returnValue ? "true" : "false");
	return returnValue;
}


bool	
AppleUSBHub::IsPortInitThreadActiveForAnyPort()
{
    UInt32					currentPort;
    IOUSBHubPortStatus		portStatus;
	IOReturn				kr;
    AppleUSBHubPort			*port;
	bool					returnValue = false;
    
	if (!_ports)
	{
		USBLog(3, "AppleUSBHub[%p]::IsPortInitThreadActiveForAnyPort - no _ports - returning false", this);
		return false;
	}
	
	for (currentPort = 1; currentPort <= _hubDescriptor.numPorts; currentPort++)
	{
        port = _ports ? _ports[currentPort-1] : NULL;
		if (!port)
		{
			USBLog(3, "AppleUSBHub[%p]::IsPortInitThreadActiveForAnyPort - no port object[index: %d]", this, (int)currentPort-1);
			continue;
		}
		
		if ( port->_initThreadActive)
		{
			USBLog(4, "AppleUSBHub[%p]::IsPortInitThreadActiveForAnyPort - port(%d) had the  initThreadActive(%d)", this, (int)currentPort, port->_initThreadActive);
			returnValue = true;
			break;
		}
	}		
	
	USBLog(6, "AppleUSBHub[%p](0x%x)::IsPortInitThreadActiveForAnyPort - %s", this, (uint32_t)_locationID, returnValue ? "true" : "false");

	return returnValue;
}



bool	
AppleUSBHub::IsStatusChangedThreadActiveForAnyPort()
{
    UInt32					currentPort;
    IOUSBHubPortStatus		portStatus;
	IOReturn				kr;
    AppleUSBHubPort			*port;
	bool					returnValue = false;
    
	if (!_ports)
	{
		USBLog(3, "AppleUSBHub[%p]::IsStatusChangedThreadActiveForAnyPort - no _ports - returning false", this);
		return false;
	}
	
	for (currentPort = 1; currentPort <= _hubDescriptor.numPorts; currentPort++)
	{
        port = _ports ? _ports[currentPort-1] : NULL;
		if (!port)
		{
			USBLog(3, "AppleUSBHub[%p]::IsStatusChangedThreadActiveForAnyPort - no port object[index: %d]", this, (int)currentPort-1);
			continue;
		}
		
		if ( port->_statusChangedThreadActive)
		{
			USBLog(4, "AppleUSBHub[%p]::IsStatusChangedThreadActiveForAnyPort - port(%d) had the _statusChangedThreadActive(%d)", this, (int)currentPort, port->_statusChangedThreadActive);
			returnValue = true;
			break;
		}
	}		
	
	USBLog(6, "AppleUSBHub[%p](0x%x)::IsStatusChangedThreadActiveForAnyPort - %s", this, (uint32_t)_locationID, returnValue ? "true" : "false");

	return returnValue;
}

#if 0
//================================================================================================
//
//	IsPortSuspended()
//
#ifdef SUPPORTS_SS_USB
//	For SuperSpeed Root Hub controllers we cannot rely on polling for the Port Link State.  Instead
//  we use a state variable in the port object that is set when we process a port link state change.
//  This is for rdar://10403546
#endif
//
//================================================================================================
//
bool
AppleUSBHub::IsPortSuspended(UInt32 portNumber, UInt16 statusFlags)
{   
    bool                    suspended = false;
#ifdef SUPPORTS_SS_USB
    AppleUSBHubPort			*port = NULL;
    int						portIndex = portNumber -1;
    
    if ( _isRootHub && _device && _device->GetBus()->GetControllerSpeed() == kUSBDeviceSpeedSuper)     // For SS RootHubs
    {
  		port = _ports ? _ports[portIndex] : NULL;

        if (port != NULL )
        {
            if ( port->_portLinkState == 0xFFFF )
            {
                
                USBLog(3, "AppleUSBHub[%p]::IsPortSuspended - Port %d of Hub at 0x%x  port->_portLinkState is not initialized, returning false", this, (uint32_t)portNumber, (uint32_t)_locationID);
                suspended = false;
            }
            else
            {
                suspended = (port->_portLinkState == kSSHubPortLinkStateU3) ? true : false;
            }
        }
    }
    else
#endif
   {
        suspended = (statusFlags & kHubPortSuspend) ? true : false;
    }
    
    USBLog(6, "AppleUSBHub[%p]::IsPortSuspended - Port %d of Hub at 0x%x  returning %s", this, (uint32_t)portNumber, (uint32_t)_locationID, suspended ? "True" : "False");
    
    return suspended;
}
#endif

#pragma mark ееееееее Ports ееееееее
IOReturn 
AppleUSBHub::StartPorts(void)
{
    AppleUSBHubPort			*port;
    int						portIndex, portNum;
	IOReturn				err;
	bool					resumedOneAlready = false;
	bool					workToDo = true;				// don't stop until we have done all we can
	bool					needsPowerSequencingDelay = false;
	bool					needsInitialDelay = true;
	

    USBLog(5, "AppleUSBHub[%p]::StartPorts - _bus[%p] starting (%d) ports of hub @ 0x%x, isInactive(%s)", this, _bus, _hubDescriptor.numPorts, (uint32_t)_locationID, isInactive() ? "true" : "false");

	if (!_ports)
	{
		USBError(1, "AppleUSBHub[%p]::StartPorts - no memory for ports!!", this);
		return kIOReturnNoMemory;
	}
	
	if (isInactive())
	{
		USBLog(5, "AppleUSBHub[%p]::StartPorts - we are inactive. Nothing to do.", this);
		return kIOReturnSuccess;
	}
	
	// Radar 5713215
	// if this hub needs a little extra delay sequencing, check for that here
	OSBoolean * boolObj = OSDynamicCast( OSBoolean, _device->getProperty("kHubPowerSequencingDelay") );
	needsPowerSequencingDelay = ( boolObj && boolObj->isTrue() );
		
	
    for (portIndex = 0; portIndex < _hubDescriptor.numPorts; portIndex++)
    {
		portNum = portIndex+1;
		
 		port = _ports ? _ports[portIndex] : NULL;
		if (port)
		{
			port->retain();
			if (port->_portPMState == usbHPPMS_uninitialized)
			{
				if (needsInitialDelay)
				{
					USBLog(5, "AppleUSBHub[%p]::StartPorts _bus[%p] -at least one unititialized port, calling RaisePowerState and spawning initialDelayThread", this, _bus);
					needsInitialDelay = false;
					RaisePowerState();
					retain();
					if (thread_call_enter(_initialDelayThread) == TRUE)
					{
						USBLog(1, "AppleUSBHub[%p]::StartPorts - InitialDelayThread already queued - UNEXPECTED!", this);
						LowerPowerState();
						release();
					}
				}
				// Radar 5713215
				// if this hub needs a little extra delay sequencing, then let's just delay before starting each port
				if (needsPowerSequencingDelay)
				{
					USBLog(5, "AppleUSBHub[%p]::StartPorts - _bus[%p] port (%d) power sequence delay for %d ms", this, _bus, portNum, _hubDescriptor.powerOnToGood *2);
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
                    {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHub[%p]::StartPorts - IOSleep in gate:%d", this, 1);}}
#endif
					IOSleep(_hubDescriptor.powerOnToGood*2);    // let's use the POTGT to tell us how long to delay
				}

				USBLog(5, "AppleUSBHub[%p]::StartPorts - _bus[%p] port (%d) uninitialized - calling port->start", this, _bus, portNum);
				port->start();
				port->_portPMState = usbHPPMS_active;
				USBLog(5, "AppleUSBHub[%p]::StartPorts - _bus[%p] port[%p] now in state[%d]", this, _bus, port, port->_portPMState);
			}
			else 
			{
				IOUSBHubPortStatus		portStatus;
				bool					enableEndpoints = false;				// usually this is done when the resume completes
				
				// first let's get the state to see if the resume has already changed (which probably means that it is disconnected)
				USBLog(5, "AppleUSBHub[%p]::StartPorts - port[%p] number (%d) - calling GetPortStatus", this, port, portNum);
				portStatus.statusFlags = 0;
				portStatus.changeFlags = 0;
				err = GetPortStatus(&portStatus, portNum);
				USBLog(5, "AppleUSBHub[%p]::StartPorts - port[%p] number (%d) -  GetPortStatus returned 0x%x, status: %p, change: %p", this, port, portNum, err, (void*)portStatus.statusFlags, (void*)portStatus.changeFlags);
				if (err)
				{
					// this happens because we cannot talk to the hub to get the status for the port - usually we (the hub) are no longer on the bus
					// we need to go ahead and re-enable the endpoints before we finish and go to the ON state
					USBLog((err == kIOReturnNoDevice) ? 5 : 1, "AppleUSBHub[%p]::StartPorts - err (%p) from GetPortStatus for port (%d) isInactive(%s) - reenabling endpoints anyway", this, (void*)err, portNum, isInactive() ? "true" : "false");
					enableEndpoints = true;
				}
				else if ( (portStatus.changeFlags & kHubPortConnection) && (portStatus.statusFlags & kHubPortConnection) )
				{
					
					if ( portNum == _expressCardPort && _hubWithExpressCardPort )
					{
						port->DetectExpressCardCantWake();

						USBLog(2, "AppleUSBHub[%p]::StartPorts - port[%p] number (%d) indicating kIOUSBMessageExpressCardCantWake to controller", this, port, portNum);
						
						_device->GetBus()->retain();
						_device->GetBus()->message(kIOUSBMessageExpressCardCantWake, this, _device);
						_device->GetBus()->release();
					}
				}
				else if ((portStatus.changeFlags & kHubPortSuspend) || (portStatus.changeFlags & kHubPortConnection))
				{
					if ((portStatus.changeFlags & kHubPortSuspend) &&  !(portStatus.changeFlags & kHubPortConnection))
					{
						if (!port->_suspendChangeAlreadyLogged)
						{
							port->_suspendChangeAlreadyLogged = true;

							if (port->_portDevice)
							{
								// this port is probably the cause of the wake. It already has its suspend change set
								IOLog("The USB device %s (Port %d of Hub at 0x%x) may have caused a wake by issuing a remote wakeup (3)\n", (port->_portDevice)->getName(), portNum, (uint32_t)_locationID);
								USBLog(5, "AppleUSBHub[%p]::StartPorts - The USB device %s (port[%p] %d of hub at 0x%x), probably just caused a wake (remote wakeup)", this, (port->_portDevice)->getName(), port, portNum, (uint32_t)_locationID );
							}
							else 
							{
								IOLog("An unknown USB device (Port %d of Hub at 0x%x) may have caused a wake by issuing a remote wakeup\n", portNum, (uint32_t)_locationID);
							}

						}
					}					
					else if (portStatus.changeFlags & kHubPortConnection)
					{
						if (port->_portDevice)
						{
							if ( portStatus.statusFlags & kHubPortPower )
							{
								if ( portNum == _expressCardPort && _hubWithExpressCardPort )
								{
									port->DetectExpressCardCantWake();
								
									USBLog(2, "AppleUSBHub[%p]::StartPorts - port[%p] number (%d) indicating kIOUSBMessageExpressCardCantWake to controller", this, port, portNum);
									
									_device->GetBus()->retain();
									_device->GetBus()->message(kIOUSBMessageExpressCardCantWake, this, _device);
									_device->GetBus()->release();
								}
							}
						
							IOLog("The USB device %s (Port %d of Hub at 0x%x) may have caused a wake by being disconnected\n", (port->_portDevice)->getName(), portNum, (uint32_t)_locationID);
							USBLog(5, "AppleUSBHub[%p]::StartPorts - The USB device %s (port[%p] %d of hub at 0x%x), just caused a wake (disconnect)", this, (port->_portDevice)->getName(), port, portNum, (uint32_t)_locationID );
						}
						else
						{
							IOLog("An Unknown USB Device (Port %d of Hub at 0x%x), may have caused a wake by being connected\n", portNum, (uint32_t)_locationID);
							USBLog(5, "AppleUSBHub[%p]::StartPorts - port[%p] %d of hub at 0x%x, just caused a wake (new connection)", this, port, portNum, (uint32_t)_locationID );
						}

					}					
				}
				else if ( (port->_portDevice == NULL) && (portStatus.statusFlags & kHubPortConnection) && !(portStatus.changeFlags & kHubPortConnection) )
				{
					USBLog(1, "AppleUSBHub[%p]::StartPorts - port[%p] %d of hub at 0x%x, might have a missed a connection change (0x%04x, 0x%04x)", this, port, portNum, (uint32_t)_locationID, portStatus.statusFlags, portStatus.changeFlags );
				}
				
				if (port->_portPMState == usbHPPMS_pm_suspended)
				{
					if (!err)
					{
						if ((portStatus.changeFlags & kHubPortSuspend) || (portStatus.changeFlags & kHubPortConnection))
						{
							
							// this is the case where the actual device has been unplugged, but our hub is still on the bus
							USBLog(5, "AppleUSBHub[%p]::StartPorts - port[%p] %d of hub at 0x%x (%s) has the kHubPortSuspendChange bit or kHubPortConnectionChange already set (probably unplugged) status[%p] change[%p] - won't have to wait! YAY!", this, port, portNum, (uint32_t)_locationID, (port->_portDevice)->getName(), (void*)portStatus.statusFlags, (void*)portStatus.changeFlags);
							
							// i need to clear the suspend change feature because it cannot get processed again without wreaking havoc
							if (portStatus.changeFlags & kHubPortSuspend)
                            {
								ClearPortFeature(kUSBHubPortSuspendChangeFeature, portNum);
                            }
#ifdef SUPPORTS_SS_USB
                            else
                            {
                                if ( _ssHub && (GETLINKSTATE(portStatus.statusFlags) == kSSHubPortLinkStateU3) )
                                {
                                    // If we are in a super speed hub and the link state is still in U3, then clear the link state
                                    USBLog(5, "AppleUSBHub[%p]::StartPorts - port[%p] %d of hub at 0x%x (%s) has the link state in U3 and has a connection change, unsuspend", this, port, portNum, (uint32_t)_locationID, (port->_portDevice)->getName());
                                   ClearPortFeature(kUSBHubPortSuspendFeature, portNum);
                                }
                            }
#endif
							enableEndpoints = true;
							
						}
						else if (!(portStatus.statusFlags & kHubPortSuspend))
						{
							// this can happen on OHCI controllers if the device issues a remote wakeup on the way to forced sleep and the HC completes the resume itself
							USBLog(3, "AppleUSBHub[%p]::StartPorts - port %d marked as suspended, but the suspend status bit is not set status[%p] change[%p] _portDevice name[%s]", this, (int)portNum, (void*)portStatus.statusFlags, (void*)portStatus.changeFlags, port->_portDevice ? port->_portDevice->getName() : "no device");

							enableEndpoints = true;		// this will cause the the port to be marked active as well
							
							if ( (portStatus.statusFlags & kHubPortConnection) &&  !(portStatus.statusFlags & kHubPortEnabled) && port->HasExpressCardCantWake() )
							{	
								USBLog(5, "AppleUSBHub[%p]::StartPorts - found an express card in port %d that is still disabled, needs power cycling", this, portNum);
								
								// Try power off and disabling the port
								//
								if ( (err = SetPortPower(portNum, kHubPortPowerOff)) )
								{
									USBLog(5, "AppleUSBHub[%p]::StartPorts - got err %x clearing port power feature portNum %d", this, err, portNum);
								}
								
								// Wait for before powering it back on.  Spec says to wait 100ms, we will
								// wait some more.
								//
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
                                {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHub[%p]::StartPorts - IOSleep in gate:%d", this, 2);}}
#endif
								IOSleep(100);
								
								if ( (err = SetPortPower(portNum, kHubPortPowerOn)) )
								{
									USBLog(5, "AppleUSBHub[%p]::StartPorts - got err %x setting port power feature portNum %d", this, err, portNum);
								}
							}
						}
						else
						{
							USBLog(5, "AppleUSBHub[%p]::StartPorts - port[%p] number (%d) suspended by PM statusFlags[%p] - calling SuspendPort(false)", this, port, portNum, (void*)portStatus.statusFlags);
							if (!resumedOneAlready)
							{
								err = port->SuspendPort(false, false);
								resumedOneAlready = true;
							}
							else
								err = kIOReturnSuccess;							// we pretend that we resumed - it will be re-handled in WaitForPortResumes
							
							// the rest of what is needed to wake this port will be done in the HandleSuspendPortChange
							if (err == kIOReturnNotResponding)
							{
								USBLog(5, "AppleUSBHub[%p]::StartPorts - got err kIOReturnNotResponding from SuspendPort- we must be dead! aborting StartPorts", this);
								break;
							}
							if (err != kIOReturnSuccess)
							{
								USBLog(5, "AppleUSBHub[%p]::StartPorts - got err %p from SuspendPort- perhaps we need to deal with this!", this, (void*)err);
							}
							else
							{
								// these will be decremented when the resume notification comes in in WaitForPortResumes
								// we need these even for ports which we didn't actually resume yet. The will get resumed in WaitForPortResumes (one at a time)
								USBLog(6, "AppleUSBHub[%p]::StartPorts - calling IncrementOutstandingIO and IncrementOutstandingResumes", this);
								IncrementOutstandingIO();
								IncrementOutstandingResumes();
							}
						}
					}
					
					if (enableEndpoints)
					{
						IOUSBControllerV3		*v3Bus = NULL;
						IOReturn				err;
						
						// wait at least 10 ms for port recovery before re-enabling the endpoints on the list
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
                        {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHub[%p]::StartPorts - IOSleep in gate:%d", this, 3);}}
#endif
						IOSleep(port->_portResumeRecoveryTime);
						if (_device)
							v3Bus = OSDynamicCast(IOUSBControllerV3, _device->GetBus());
						
						if (v3Bus && port->_portDevice)
						{
							USBLog(5, "AppleUSBHub[%p]::StartPorts - Enabling endpoints for device at address (%d)", this, (int)port->_portDevice->GetAddress());
							err = v3Bus->EnableAddressEndpoints(port->_portDevice->GetAddress(), true);
							if (err)
							{
								USBLog(5, "AppleUSBHub[%p]::StartPorts - EnableAddressEndpoints returned (%p)", this, (void*)err);
							}
						}
						port->_portPMState = usbHPPMS_active;
					}
				}
				else
				{
#ifdef SUPPORTS_SS_USB
					if ( _ssHub && (GETLINKSTATE(portStatus.statusFlags) == kSSHubPortLinkStateU3) )
					{
						// If we are in a super speed hub and the link state is still in U3, then clear the link state
						USBLog(5, "AppleUSBHub[%p]::StartPorts - port[%p] %d of hub at 0x%x has the link state in U3 (and no device) and has a connection change, unsuspend", this, port, portNum, (uint32_t)_locationID);
						ClearPortFeature(kUSBHubPortSuspendFeature, portNum);
					}
					else
#endif
					{
					USBLog(5, "AppleUSBHub[%p]::StartPorts - port[%p] number (%d) in PMState (%d) _portDevice[%p] - nothing to do", this, port, portNum, port->_portPMState, port->_portDevice);
				}
			}
			}
			port->release();
		}
		else
		{
			USBError(1, "AppleUSBHub[%p]::StartPorts - port (%d) unexpectedly missing", this, portNum);
		}

    }
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBHub::SuspendPorts(void)
{
    AppleUSBHubPort			*port;
    int						currentPort;
	IOReturn				err;
	IOUSBControllerV3		*v3Bus;

    USBLog(5, "AppleUSBHub[%p]::SuspendPorts - (%d) ports", this, _hubDescriptor.numPorts);

    for (currentPort = 1; currentPort <= _hubDescriptor.numPorts; currentPort++)
    {
        port = _ports ? _ports[currentPort-1] : NULL;
		if (port)
		{
			port->retain();
			port->_suspendChangeAlreadyLogged = false;
			if ((port->_portPMState == usbHPPMS_active) && (port->_portDevice))
			{
				USBLog(5, "AppleUSBHub[%p]::SuspendPorts - suspending port[%p] number (%d) - need to disable device (%p) named(%s)", this, port, currentPort, port->_portDevice, port->_portDevice ? port->_portDevice->getName() : "NULL");
				v3Bus = OSDynamicCast(IOUSBControllerV3, _bus);
				if (v3Bus)
				{
					err = v3Bus->EnableAddressEndpoints(port->_portDevice->GetAddress(), false);
					if (err)
					{
						USBLog(2, "AppleUSBHub[%p]::SuspendPorts - EnableAddressEndpoints returned (%p)", this, (void*)err);
					}
				}
				err = port->SuspendPort(true, false);
				if (err)
				{
					USBLog(1, "AppleUSBHub[%p]::SuspendPorts - err [%p] suspending port (%d)", this, (void*)err, currentPort);
					USBTrace( kUSBTHub, kTPHubSuspendPorts, (uintptr_t)this, err, currentPort, 1);
				}
				else
				{
					port->_portPMState = usbHPPMS_pm_suspended;
					USBLog(5, "AppleUSBHub[%p]::SuspendPorts - port[%p] now in state[%d]", this, port, port->_portPMState);
				}
			}
			else
			{
				USBLog(5, "AppleUSBHub[%p]::SuspendPorts - port (%d) in _portPMState (%d) _portDevice(%p) - not calling port->SuspendPort", this, currentPort, port->_portPMState, port->_portDevice);
			}
			port->release();
		}
		else
		{
			USBError(1, "AppleUSBHub[%p]::SuspendPorts - port (%d) unexpectedly missing", this, currentPort);
		}
    }
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBHub::StopPorts(void)
{
    AppleUSBHubPort *		port;
    AppleUSBHubPort **	 	cachedPorts;
    int						currentPort;

    USBLog(5, "AppleUSBHub[%p]::+StopPorts - (%d) ports total", this, _hubDescriptor.numPorts);

	// make sure that we don't receive any more status changes, since the ports will now be gone (5388864)
	if ( _interruptPipe )
	{
		USBLog(5, "AppleUSBHub[%p]::StopPorts - aborting pipe", this);
		_abortExpected = true;
		_needInterruptRead = false;
		_interruptPipe->Abort();
		USBLog(5, "AppleUSBHub[%p]::StopPorts - done aborting pipe", this);
	}

    if ( _ports)
    {
        cachedPorts = _ports;
        _ports = NULL;

        for (currentPort = 0; currentPort < _hubDescriptor.numPorts; currentPort++)
        {
            port = cachedPorts[currentPort];
            if (port)
            {
                cachedPorts[currentPort] = NULL;
				USBLog(5, "AppleUSBHub[%p]::StopPorts - calling stop on port (%p)", this, port);
                port->stop();
				USBLog(5, "AppleUSBHub[%p]::StopPorts - calling release on port (%p)", this, port);
                port->release();
            }
        }
        IOFree(cachedPorts, sizeof(AppleUSBHubPort *) * _hubDescriptor.numPorts);
    }

    return kIOReturnSuccess;
}


bool 
AppleUSBHub::HubStatusChanged(void)
{
 	AbsoluteTime	currentTime;
	UInt64			elapsedTime;
   IOReturn			err = kIOReturnSuccess;

	USBLog(6,"AppleUSBHub[%p]::HubStatusChanged - calling IncrementOutstandingIO", this);
	IncrementOutstandingIO();
    do
    {
        if ((err = GetHubStatus(&_hubStatus)))
        {
            FatalError(err, "get status (first in hub status change)");
            break;
        }
        _hubStatus.statusFlags = USBToHostWord(_hubStatus.statusFlags);
        _hubStatus.changeFlags = USBToHostWord(_hubStatus.changeFlags);

        USBLog(3,"AppleUSBHub[%p]::HubStatusChanged - _isRootHub(%s) hub status = %x/%x", this, _isRootHub ? "true" : "false", _hubStatus.statusFlags, _hubStatus.changeFlags);

        if (_hubStatus.changeFlags & kHubLocalPowerStatusChange)
        {
            USBLog(3, "AppleUSBHub[%p]::HubStatusChanged  Hub Local Power Status Change detected", this);
            if ((err = ClearHubFeature(kUSBHubLocalPowerChangeFeature)))
            {
                FatalError(err, "clear hub power status feature");
                break;
            }
            if ((err = GetHubStatus(&_hubStatus)))
            {
                FatalError(err, "get status (second in hub status change)");
                break;
            }
            
			_hubStatus.statusFlags = USBToHostWord(_hubStatus.statusFlags);
            _hubStatus.changeFlags = USBToHostWord(_hubStatus.changeFlags);
			
			USBLog(3,"AppleUSBHub[%p]::HubStatusChanged  hub status after clearing LocalPowerChange = %x/%x", this, _hubStatus.statusFlags, _hubStatus.changeFlags);
			// Need to check whether we successfully cleared the change
        }

        if (_hubStatus.changeFlags & kHubOverCurrentIndicatorChange)
        {
            USBLog(3, "AppleUSBHub[%p]::HubStatusChanged  Hub OverCurrent detected", this);

			// Only display the notice once per hub.  The hardware is supposed to disable the port, so there is no need
			// to keep doing it.  Once they unplug the hub, we will get a new port object and this can trigger again
			//
			clock_get_uptime(&currentTime);
			SUB_ABSOLUTETIME(&currentTime, &_overCurrentNoticeTimeStamp );
			absolutetime_to_nanoseconds(currentTime, &elapsedTime);
			elapsedTime /= 1000000000;			 						// Convert to seconds from nanoseconds
			
            USBLog(5, "AppleUSBHub[%p]::HubStatusChanged. displayedNoticed: %d, time since last: %qd", this,  _overCurrentNoticeDisplayed, elapsedTime );
			if ( !_overCurrentNoticeDisplayed || (elapsedTime > kDisplayOverCurrentTimeout) )
			{
				// According to 11.24.2.6 of the USB spec, only hubs with ganged overcurrent detection
				// will enable this bit.   So, let's tell the user what is happening.
				_device->DisplayUserNotification(kUSBGangOverCurrentNotificationType);

				_overCurrentNoticeDisplayed = true;
				clock_get_uptime(&_overCurrentNoticeTimeStamp);
			}
			else
			{
	            USBLog(5, "AppleUSBHub[%p]::HubStatusChanged  not displaying overcurrent notice because elapsed time %qd is < kDisplayOverCurrentTimeout seconds", this, elapsedTime );
			}

			
            if ((err = ClearHubFeature(kUSBHubOverCurrentChangeFeature)))
            {
                FatalError(err, "clear hub over-current feature");
                break;
            }
			
			// wait 3 seconds for the overcurrent to disappear
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
            {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHub[%p]::HubStatusChanged - IOSleep in gate:%d", this, 1);}}
#endif
			IOSleep(3000);		
			
            if ((err = GetHubStatus(&_hubStatus)))
            {
                FatalError(err, "get status (second in hub status change)");
                break;
            }
			
            _hubStatus.statusFlags = USBToHostWord(_hubStatus.statusFlags);
            _hubStatus.changeFlags = USBToHostWord(_hubStatus.changeFlags);
			USBLog(3,"AppleUSBHub[%p]::HubStatusChanged  hub status after clearing HubOvercurrent = %x/%x", this, _hubStatus.statusFlags, _hubStatus.changeFlags);
        }

        // See if we have the kResetOnPowerStatusChange errata. This means that upon getting a hub status change, we should do
        // a device reset
        //
        OSBoolean * boolObj = OSDynamicCast( OSBoolean, _device->getProperty("kResetOnPowerStatusChange") );
        if ( boolObj && boolObj->isTrue() )
        {
            // Set an error so that we cause our port to be reset. The overcurrent and power status changes might disable the ports downstream
            // so we need a reset to recover. Do this ONLY if the change flag indicated that the status was a change to ON.
            //
			if ( _hubStatus.statusFlags & ( kUSBHubLocalPowerChangeFeature || kHubOverCurrentIndicatorChange ) )
			{
				USBLog(3,"AppleUSBHub[%p]::HubStatusChanged -  change to ON (0x%x)", this, _hubStatus.statusFlags);
				err = kIOReturnBusy;
			}
        }

    } while(false);

    if ( err != kIOReturnSuccess )
	{
        // If we get an error, then we better reset our hub
        //
		USBLog(1, "AppleUSBHub[%p]::HubStatusChanged - err (%p) - reseting my port", this, (void*)err);
		USBTrace( kUSBTHub, kTPHubSuspendPorts, (uintptr_t)this, err, 0, 2);
        retain();
        ResetMyPort();
        release();
        
    }

	USBLog(6,"AppleUSBHub[%p]::HubStatusChanged calling DecrementOutstandingIO", this);
	DecrementOutstandingIO();
	USBLog(6,"AppleUSBHub[%p]::HubStatusChanged returning %s", this, err == kIOReturnSuccess ? "true" : "false");
	return (err == kIOReturnSuccess);
}

UInt32 
AppleUSBHub::GetHubErrataBits()
{
	UInt16		vendID, deviceID, revisionID;
	ErrataListEntry	*entryPtr;
	UInt32		i, errata = 0;
	
	USBLog(6,"AppleUSBHub[%p]::GetHubErrataBits  Hub at location 0x%x", this, (uint32_t)_locationID);
	
	// get this chips vendID, deviceID, revisionID
	vendID = _device->GetVendorID();
	deviceID = _device->GetProductID();
	revisionID = _device->GetDeviceRelease();
	
	for(i=0, entryPtr = errataList; i < errataListLength; i++, entryPtr++)
	{
		if (vendID == entryPtr->vendID
			&& deviceID == entryPtr->deviceID
			&& revisionID >= entryPtr->revisionLo
			&& revisionID <= entryPtr->revisionHi)
		{
			errata |= entryPtr->errata;  // we match, add this errata to our list
		}
	}
	
	
	UInt32	bits = 0;
	if ( GetInternalHubErrataBits(&bits) )
	{
		USBLog(6,"AppleUSBHub[%p]::GetHubErrataBits 	GetInternalHubErrata returned 0x%x", this, (uint32_t)bits);
		errata |= bits;
	}

	return errata;
}



void 
AppleUSBHub::FatalError(IOReturn err, const char *str)
{
    USBError(1, "AppleUSBHub[%p]::FatalError 0x%x: %s", this, err, str);
}



#ifdef SUPPORTS_SS_USB
IOReturn 
AppleUSBHub::GetHubDescriptor(IOUSB3HubDescriptor *desc)
{
    IOReturn	err = kIOReturnSuccess;
    IOUSBDevRequest	request;

    if (!desc) 
		return (kIOReturnBadArgument);

    request.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBClass, kUSBDevice);
    request.bRequest = kUSBRqGetDescriptor;

	if(_ssHub)	
	{
		request.wValue = (kUSB3HubDescriptorType << 8) + 0;	// Descriptor type goes in high byte, index in low
	}
	else
	{
		request.wValue = (kUSBHubDescriptorType << 8) + 0;	// Descriptor type goes in high byte, index in low
	}

    request.wIndex = 0;
    request.wLength = sizeof(IOUSBHubDescriptor);
    request.pData = desc;

    err = DoDeviceRequestWithRetries(&request, false);

    if (err)
    {
         // Is this a bogus hub?  Some hubs require 0 for the descriptor type
         // to get their device descriptor.  This is a bug, but it's actually
         // spec'd out in the USB 1.1 docs.

        USBLog(5,"AppleUSBHub[%p]: GetHubDescriptor w/ type = %X returned error: 0x%x", this, kUSBHubDescriptorType, err);
        request.wValue = 0;
        request.wLength = sizeof(IOUSBHubDescriptor);
        err = DoDeviceRequestWithRetries(&request, false);
    }

    if (err)
    {
        USBLog(3, "AppleUSBHub [%p] GetHubDescriptor error = 0x%x", this, err);
    }

    return err;
}
#else
IOReturn 
AppleUSBHub::GetHubDescriptor(IOUSBHubDescriptor *desc)
{
    IOReturn	err = kIOReturnSuccess;
    IOUSBDevRequest	request;

    if (!desc) return (kIOReturnBadArgument);

    request.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBClass, kUSBDevice);
    request.bRequest = kUSBRqGetDescriptor;
    request.wValue = (kUSBHubDescriptorType << 8) + 0;	// Descriptor type goes in high byte, index in low
    request.wIndex = 0;
    request.wLength = sizeof(IOUSBHubDescriptor);
    request.pData = desc;

    err = DoDeviceRequestWithRetries(&request, false);

    if (err)
    {
         // Is this a bogus hub?  Some hubs require 0 for the descriptor type
         // to get their device descriptor.  This is a bug, but it's actually
         // spec'd out in the USB 1.1 docs.

        USBLog(5,"AppleUSBHub[%p]: GetHubDescriptor w/ type = %X returned error: 0x%x", this, kUSBHubDescriptorType, err);
        request.wValue = 0;
        request.wLength = sizeof(IOUSBHubDescriptor);
        err = DoDeviceRequestWithRetries(&request, false);
    }

    if (err)
    {
        USBLog(3, "AppleUSBHub [%p] GetHubDescriptor error = 0x%x", this, err);
    }

    return err;
}
#endif


IOReturn 
AppleUSBHub::GetHubStatus(IOUSBHubStatus *status)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevRequest	request;

    request.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBClass, kUSBDevice);
    request.bRequest = kUSBRqGetStatus;
    request.wValue = 0;
    request.wIndex = 0;
    request.wLength = sizeof(IOUSBHubStatus);
    request.pData = status;

    err = DoDeviceRequestWithRetries(&request);

    if (err)
    {
        USBLog(3, "AppleUSBHub [%p] GetHubStatus error = 0x%x", this, err);
    }        

    return(err);
}



IOReturn 
AppleUSBHub::GetPortState(UInt8 *state, UInt16 port)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevRequest	request;

    request.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBClass, kUSBOther);
    request.bRequest = kUSBRqGetState;
    request.wValue = 0;
    request.wIndex = port;
    request.wLength = sizeof(*state);
    request.pData = state;

	USBLog(7, "AppleUSBHub[%p]::GetPortState - issuing DeviceRequest", this);
    err = DoDeviceRequestWithRetries(&request);

    if (err)
    {
        USBLog(3, "AppleUSBHub [%p] GetPortState error = 0x%x", this, err);
    }        

    return(err);
}



IOReturn 
AppleUSBHub::ClearHubFeature(UInt16 feature)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevRequest	request;

    request.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBDevice);
    request.bRequest = kUSBRqClearFeature;
    request.wValue = feature;
    request.wIndex = 0;
    request.wLength = 0;
    request.pData = NULL;

	USBLog(7, "AppleUSBHub[%p]::ClearHubFeature - issuing DeviceRequest", this);
    err = DoDeviceRequestWithRetries(&request);

    if (err)
    {
        USBLog(3, "AppleUSBHub [%p] ClearHubFeature error = 0x%x", this, err);
    }        

    return(err);
}


#ifdef SUPPORTS_SS_USB
//================================================================================================
//
//	GetPortStatus
//
//	This supports 2.0 and 3.0 hubs, but for 3.0 hubs we have to synthesize some of the bits and
//  move things around. This is how the 16 bits are defined
//
//  +-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
//  |  15   |  14   |  13   |  12   |  11   |  10   |   9   |   8   |   7   |   6   |   5   |   4   |   3   |   2   |   1   |   0   |
//  +-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
//	| Unused| Link3 | Super |  Port |  Port |  High |  Low  |  Port | Link2 | Link1 | Link0 |  Port |  Over |  Port |  Port |Connect|
//	|       |  Bit  | Speed |Indicat|  Test | Speed | Speed | Power |  Bit  |  Bit  |  Bit  | Reset |Current|Suspend|Enabled|       |
//	|       |(USB 3)|Device |(USB 2)|(USB 2)| Speed |Device |       |(USB 3)|(USB 3)|(USB 3)|       |       |(USB 2)|       |       |
//	|       |       |(synth)|       |       |(USB 2)|(USB 2)|       |       |       |       |       |       |       |       |       |
//  +-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
//
//  Note:  Bit 13 is a synthesized bit indicating that the port has a super speed device present.  It is not in either spec.  The Link bits
//         represent the USB 3 Port Link State bits, but the high bit is moved to bit 14 from bit 8, as the 2 specs have different bits
//         for port power
//================================================================================================
//
#endif
IOReturn 
AppleUSBHub::GetPortStatus(IOUSBHubPortStatus *status, UInt16 port)
{
    IOReturn			err = kIOReturnSuccess;
    IOUSBDevRequest		request;
	int					i = 0;

    request.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBClass, kUSBOther);
    request.bRequest = kUSBRqGetStatus;
    request.wValue = 0;
    request.wIndex = port;
    request.wLength = sizeof(IOUSBHubPortStatus);
    request.pData = status;
	request.wLenDone = 0;

	USBLog(7, "AppleUSBHub[%p]::GetPortStatus - issuing DoDeviceRequestWithRetries speed:%d _isRootHub:(%s)", this, _device->GetSpeed(), _isRootHub ? "true" : "false");
    err = DoDeviceRequestWithRetries(&request, true);
	
	while ((i++ < 30) && !err && (request.wLenDone != sizeof(IOUSBHubPortStatus)))
	{
		USBLog(2, "AppleUSBHub[%p]::GetPortStatus - request came back with only %d bytes - retrying", this, (int)request.wLenDone);
		err = DoDeviceRequestWithRetries(&request, true);
	}

	if ( _retryBogusPortStatus && (USBToHostWord(status->changeFlags) & 0xFFE0) )
	{
		USBLog(2, "AppleUSBHub[%p]::GetPortStatus - _retryBogusPortStatus set, change: 0x%x, retrying", this, USBToHostWord(status->changeFlags));
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
        {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHub[%p]::GetPortStatus - IOSleep in gate:%d", this, 1);}}
#endif
		IOSleep(50);
		err = DoDeviceRequestWithRetries(&request);
	}
	
	if (!err && (request.wLenDone != sizeof(IOUSBHubPortStatus)))
	{
		USBLog(2, "AppleUSBHub[%p]::GetPortStatus - request never returned bytes in %d tries - returning kIOReturnUnderrun", this, i);
		err = kIOReturnUnderrun;
	}

    if (err)
    {
        USBLog(3, "AppleUSBHub[%p]::GetPortStatus, error (%x) returned from DoDeviceRequestWithRetries", this, err);
    }

    if ( err == kIOReturnSuccess)
    {
		// Get things the right way round.
		status->statusFlags = USBToHostWord(status->statusFlags);
		status->changeFlags = USBToHostWord(status->changeFlags);
		
		USBLog( 7, "AppleUSBHub[%p]::GetPortStatus for port %d, status: 0x%8x, change: 0x%8x - returning kIOReturnSuccess", this, port, status->statusFlags, status->changeFlags);

#ifdef SUPPORTS_SS_USB
		if (_ssHub)
		{
			// We need to munge the port status of a SuperSpeed hub (including root hubs)
			// 
			// 1.  We will move bit 8 (part of the LinkState) to bit 14
			// 2.  We will move bit 9 (PortPower) down to bit 8.  
			
			UInt16	linkState = (status->statusFlags & kSSHubPortStatusLinkStateMask) >> kSSHubPortStatusLinkStateShift;
			UInt16  linkStateBit8 = linkState >> 3;
			UInt16	portPower = (status->statusFlags & kSSHubPortStatusPowerMask) >> kSSHubPortStatusPowerBit;
			UInt16	portSpeed = (status->statusFlags & kSSHubPortStatusSpeedMask) >> kSSHubPortStatusSpeedShift;
		
			UInt16	newStatus = (status->statusFlags & 0x00FF);		// First 8 bits are not changed.
			
			// Set the USB2 PortPower bit
			if (portPower) 
				newStatus |= kHubPortPower;
			
			// Set our fake SS bit
			if (portSpeed == kSSHubPortSpeed5Gbps)
				newStatus |= kHubPortSuperSpeed;
			
			// Set the fake Link State Bit 3
			newStatus |= (linkStateBit8 << 14);
            
            // If the link state is in U3, set the suspend status
            if ( linkState == kSSHubPortLinkStateU3)
				newStatus |= kHubPortSuspend;
			
			// If the port is debouncing, fake our bit
			if (status->statusFlags & kHubPortDebouncing)
				newStatus |= kHubPortDebouncing;
			
			USBLog(6, "AppleUSBHub[%p]::GetPortStatus for SuperSpeed port %d  USB3 status was: 0x%04x, now: 0x%04x, change: 0x%04x", this, port, status->statusFlags, newStatus, status->changeFlags);
			status->statusFlags = newStatus;
		}
#endif
    }
	
    return(err);
}



IOReturn 
AppleUSBHub::SetPortFeature(UInt16 feature, UInt16 port)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevRequest	request;

    USBLog(5, "AppleUSBHub[%p]::SetPortFeature port/feature (0x%04x) - setting", this, (port << 16) | feature);

#ifdef SUPPORTS_SS_USB
	if (_ssHub && !_isRootHub && (feature == kUSBHubPortSuspendFeature))
	{
		USBLog(5, "AppleUSBHub[%p]::SetPortFeature  suspending a port(%d) on a SS Hub (0x%x), so calling SetPortLinkState(kSSHubPortLinkStateU3)", this, port, (uint32_t)_locationID);
		return SetPortLinkState(kSSHubPortLinkStateU3, port);
	}
#endif
    request.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBOther);
    request.bRequest = kUSBRqSetFeature;
    request.wValue = feature;
    request.wIndex = port;
    request.wLength = 0;
    request.pData = NULL;

    err = DoDeviceRequestWithRetries(&request);
    USBTrace( kUSBTHub, kTPHubSetPortFeature, (uintptr_t)this, feature, port, err);

    if (err && (err != kIOUSBDeviceTransferredToCompanion) && (err != kIOReturnUnsupported))
    {
        USBLog(1, "AppleUSBHub[%p]::SetPortFeature (%d) to port %d of hub @ 0x%x got error 0x%x (%s) from DoDeviceRequestWithRetries", this, feature, port, (uint32_t)_locationID, err, USBStringFromReturn(err));
    }

    return(err);
}


#ifdef SUPPORTS_SS_USB
//================================================================================================
//
//   GetPortErrorCount
//
//================================================================================================
//
IOReturn 
AppleUSBHub::GetPortErrorCount(UInt16 port, UInt16 * portErrorCount)
{
	IOUSBDevRequest		request;
	IOReturn			err;
	
	
	request.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBClass, kUSBOther);
	request.bRequest = kUSBHubRqGetPortErrorCount;
	request.wValue = 0;
	request.wIndex = port;
	request.wLength = sizeof(UInt16);
	request.pData = portErrorCount;
	request.wLenDone = 0;
	
    err = DoDeviceRequestWithRetries(&request);
	if ( err != kIOReturnSuccess)
	{
        USBLog(3, "AppleUSBHub[%p]::GetPortErrorCount port %d of hub @ 0x%x got error 0x%x (%s) from DoDeviceRequestWithRetries", this, port, (uint32_t)_locationID, err, USBStringFromReturn(err));
	}
	
	return err;
	
}

//================================================================================================
//
//   SetPortLinkState
//
//================================================================================================
//
IOReturn 
AppleUSBHub::SetPortLinkState(UInt16 linkState, UInt16 port)
{
	IOReturn		kr = kIOReturnUnsupported;
	
    USBLog(5, "AppleUSBHub[%p]::SetPortLinkState Port %d of Hub @ 0x%x, state %d (%s)", this, port, (uint32_t)_locationID, linkState, LinkStateName(linkState));
	
	kr = SetPortFeature(kUSBHubPortLinkStateFeature, (linkState << 8) + port);
	
	if ( kr != kIOReturnSuccess)
	{
		USBLog(5, "AppleUSBHub[%p]::SetPortLinkState Port %d of Hub @ 0x%x, returned 0x%x", this, port, (uint32_t)_locationID, kr);
	}
	
	return kr;
}


//================================================================================================
//
//   SetPortU1Timeout
//
//================================================================================================
//
IOReturn 
AppleUSBHub::SetPortU1Timeout(UInt16 timeout, UInt16 port)
{
	IOReturn		kr = kIOReturnUnsupported;
	
    USBLog(5, "AppleUSBHub[%p]::SetPortU1Timeout  Port %d of Hub @ 0x%x, timeout: %d ╡secs", this, port, (uint32_t)_locationID, timeout);
	
	kr = SetPortFeature(kUSBHubPortU1TimeoutFeature, (timeout << 8) + port);
	
	if ( kr != kIOReturnSuccess)
	{
		USBLog(5, "AppleUSBHub[%p]::SetPortU1Timeout  Port %d of Hub @ 0x%x, returned 0x%x", this, port, (uint32_t)_locationID, kr);
	}
	
	return kr;
}


//================================================================================================
//
//   SetPortU2Timeout
//
//================================================================================================
//
IOReturn 
AppleUSBHub::SetPortU2Timeout(UInt16 timeout, UInt16 port)
{
	IOReturn		kr = kIOReturnUnsupported;
	
    USBLog(5, "AppleUSBHub[%p]::SetPortU2Timeout  Port %d of Hub @ 0x%x, timeout: %d (%d ╡secs)", this, port, (uint32_t)_locationID, timeout, timeout * 256);
	
	kr = SetPortFeature(kUSBHubPortU2TimeoutFeature, (timeout << 8) + port);
	
	if ( kr != kIOReturnSuccess)
	{
		USBLog(5, "AppleUSBHub[%p]::SetPortU2Timeout  Port %d of Hub @ 0x%x, returned 0x%x", this, port, (uint32_t)_locationID, kr);
	}
	
	return kr;
}


//================================================================================================
//
//   SetPortRemoteWakeMask
//
//================================================================================================
//
IOReturn 
AppleUSBHub::SetPortRemoteWakeMask(UInt16 mask, UInt16 port)
{
	IOReturn		kr = kIOReturnUnsupported;
	
    USBLog(5, "AppleUSBHub[%p]::SetPortRemoteWakeMask Port %d of Hub @ 0x%x, mask %d", this, port, (uint32_t)_locationID, mask);
	
	kr = SetPortFeature(kUSBHubPortRemoteWakeMaskFeature, (mask << 8) + port);
	
	if ( kr != kIOReturnSuccess)
	{
		USBLog(5, "AppleUSBHub[%p]::SetPortRemoteWakeMask Port %d of Hub @ 0x%x, returned 0x%x", this, port, (uint32_t)_locationID, kr);
	}
	
	return kr;
}


//================================================================================================
//
//   calculateDepth
//
//		Calculate the depth of the Hub in the USB bus based on the location ID
//		This function borrowed from IOUSBController
//
//================================================================================================
//
UInt16 AppleUSBHub::calculateDepth(UInt32 locationID)
{
	UInt16 depth = 0;
	SInt32	shift;
	
	for ( shift = 20; shift >= 0; shift -= 4)
	{
		if ((locationID & (0x0f << shift)) == 0)
		{
			break;
		}
		depth++;
	}
	return depth;
}


IOReturn 
AppleUSBHub::SetHubDepth(UInt16 depth)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevRequest	request;
	
    USBLog(5, "AppleUSBHub[%p]::SetHubDepth depth: %d", this, depth);
	
    request.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBDevice);
    request.bRequest = kUSBHubRqSetHubDepth;
    request.wValue = depth;
    request.wIndex = 0;
    request.wLength = 0;
    request.pData = NULL;
	
    err = DoDeviceRequestWithRetries(&request);
	
    if (err)
    {
        USBLog(1, "AppleUSBHub[%p]::SetHubDepth (%d) got error (%x) from DoDeviceRequestWithRetries", this, depth, err);
    }
	
    return(err);
}
#endif


IOReturn 
AppleUSBHub::ClearPortFeature(UInt16 feature, UInt16 port)
{
    IOReturn			err = kIOReturnSuccess;
    IOUSBDevRequest		request;
	
    USBLog(5, "AppleUSBHub[%p]::ClearPortFeature port/feature (%x) - clearing", this, (port << 16) | feature);

#ifdef SUPPORTS_SS_USB
    // We want to set the PortLinkState to U0 if we have a change or a status because the "change" is synthesized and when
    // it's reported, the link is probably in U3 still.
	if (_ssHub && !_isRootHub && ((feature == kUSBHubPortSuspendFeature) || feature == kUSBHubPortSuspendChangeFeature))
	{
		USBLog(5, "AppleUSBHub[%p]::ClearPortFeature  Resuming a port(%d) on a SS Hub (0x%x), so calling SetPortLinkState(kSSHubPortLinkStateU0)", this, port, (uint32_t)_locationID);
		return SetPortLinkState(kSSHubPortLinkStateU0, port);
	}
#endif
    request.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBOther);
    request.bRequest = kUSBRqClearFeature;
    request.wValue = feature;
    request.wIndex = port;
    request.wLength = 0;
    request.pData = NULL;

    err = DoDeviceRequestWithRetries(&request);

    if (err)
    {
        USBLog(1, "AppleUSBHub[%p]::ClearPortFeature got error (%x) to DoDeviceRequestWithRetries", this, err);
		USBTrace( kUSBTHub, kTPHubClearPortFeature, (uintptr_t)this, feature, port, err);
    }

    return err;
}

IOReturn		
AppleUSBHub::GetDeviceStatus(USBStatus *status)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevRequest	request;
	
    USBLog(7, "AppleUSBHub[%p]::GetDeviceStatus", this);
	
    request.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice);
    request.bRequest = kUSBRqGetStatus;
    request.wValue = 0;
    request.wIndex = 0;
    request.wLength = sizeof(*status);
    request.pData = status;
	
    err = DoDeviceRequestWithRetries(&request, true);
	
    if (err)
	{
        USBLog(1,"AppleUSBHub[%p]::ClearPortFeature got error (%x) to DoDeviceRequestWithRetries",this, err);
	}
	
    return(err);
}


IOReturn 
AppleUSBHub::DoPortAction(UInt32 type, UInt32 portNumber, UInt32 options  )
{
    AppleUSBHubPort 		*port;
    IOReturn				err = kIOReturnSuccess;
	bool					tryWakingUp = true;
	UInt32					portIndex = portNumber - 1;
	
	
	if (isInactive() || _hubIsDead)
	{
		USBLog(3, "AppleUSBHub[%p]::DoPortAction - isInactive(%s) _hubIsDead(%s) - returning kIOReturnNotResponding", this, isInactive() ? "true" : "false", _hubIsDead ? "true" : "false");
		err =  kIOReturnNotResponding;
	}
	else
	{
		if (_myPowerState < kIOUSBHubPowerStateOn)
		{
			if (_powerStateChangingTo != kIOUSBHubPowerStateStable)
			{
				// we are in the process of doing a power state change
				// that is, we have had a call to powerStateWillChangeTo and haven't yet gotten to powerStateDone
				// unless the new state is ON, then we will not be able to get to that state
				if ((unsigned long)_powerStateChangingTo != kIOUSBHubPowerStateOn)
				{
					USBLog(5,"AppleUSBHub[%p]::DoPortAction - we are already in the process of changing to state (%d) - unable to wake up - returning not responding", this, (int)_powerStateChangingTo);
					tryWakingUp = false;
				}
			}
			else if (_myPowerState < kIOUSBHubPowerStateLowPower)
			{
				// we are not actively changing state, but we are apparently already in a state which we cannot wake up from this way
				USBLog(5,"AppleUSBHub[%p]::DoPortAction - not while we are asleep - _myPowerState(%d)  - returning not responding", this, (int)_myPowerState);
				tryWakingUp = false;
			}
			if (!tryWakingUp)
			{
				USBLog(2, "AppleUSBHub[%p]::DoPortAction - _myPowerState(%d) _powerStateChangingTo(%d) - returning kIOReturnNotResponding", this, (int)_myPowerState, (int)_powerStateChangingTo);
				err = kIOReturnNotResponding;
				port = _ports ? _ports[portIndex] : NULL;
				if (port && port->_portDevice)
				{
					USBLog(5,"AppleUSBHub[%p]::DoPortAction - _portDevice name is %s", this, port->_portDevice->getName());
				}
			}
		}
	}

	if (err == kIOReturnSuccess)
	{
		USBLog(6,"AppleUSBHub[%p]::DoPortAction - calling RaisePowerState", this);
		RaisePowerState();
		
		err = WaitForPowerOn(2000);		// wait up to 2 seconds for the power to go on
		
		if (err != kIOReturnSuccess)
		{
			USBLog(3,"AppleUSBHub[%p]::DoPortAction - could not get power state up - returning kIOReturnNotResponding", this);
			err =  kIOReturnNotResponding;
		}
		else if ( _portSuspended && _device )
		{
			// If we are suspended, we need to first wake up
			USBLog(5,"AppleUSBHub[%p]::DoPortAction(%s) for port (%d), unsuspending port", this, HubMessageToString(type), (uint32_t)portNumber);
			err = _device->SuspendDevice(false);
			if ( err == kIOReturnSuccess )
			{
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
                {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHub[%p]::DoPortAction - IOSleep in gate:%d", this, 1);}}
#endif
				IOSleep(_hubResumeRecoveryTime);						// give ourselves time to recover
			}
		}	
		if ( err != kIOReturnSuccess )
		{
			USBLog(3,"AppleUSBHub[%p]::DoPortAction - err(%p) calling LowerPowerState", this, (void*)err);
			LowerPowerState();
		}
	}

    if ((_ports == NULL ) && (err == kIOReturnSuccess))
	{
		USBLog(1,"AppleUSBHub[%p]::DoPortAction  _ports is NULL! - calling LowerPowerState and returning kIOReturnNoDevice", this);
		USBTrace( kUSBTHub, kTPHubDoPortAction, (uintptr_t)this, kIOReturnNoDevice, 0, 0);
		LowerPowerState();								// we raised this above because there had been no error
		err = kIOReturnNoDevice;
	}
	
	if (err != kIOReturnSuccess)
	{
		USBLog(5, "AppleUSBHub[%p]::DoPortAction - unable to find success in the setup - err(%p)", this, (void*)err);
		if ((type == kIOUSBMessageHubResumePort) || (type == kIOUSBMessageHubSuspendPort))
		{
			IOUSBDevice *cachedDevice;					// in case _portDevice goes away while we are messaging
			USBLog(2, "AppleUSBHub[%p]::DoPortAction - err(%p) message was %s - need to send a message", this, (void*)err, HubMessageToString(type));
			port = _ports ? _ports[portIndex] : NULL;
			cachedDevice = port ? port->_portDevice : NULL;
			if (cachedDevice)
			{
				cachedDevice->retain();
				cachedDevice->message(kIOUSBMessagePortWasNotSuspended, cachedDevice, NULL);
				cachedDevice->release();
			}
			else
			{
				USBLog(2, "AppleUSBHub[%p]::DoPortAction - err(%p) - message was %s - no portDevice to send message to", this, (void*)err, HubMessageToString(type));
			}
		}
		return err;
	}
			
	USBLog(5,"AppleUSBHub[%p]::DoPortAction(%s) for port (%d), options (0x%x) _myPowerState(%d), getting _doPortActionLock", this, HubMessageToString(type), (uint32_t)portNumber, (uint32_t)options, (uint32_t)_myPowerState);
	err = TakeDoPortActionLock();
	if (err != kIOReturnSuccess)
	{
		USBLog(1, "AppleUSBHub[%p]::DoPortAction - unable to take the DoPortAction lock (err %p)", this, (void*)err);
		return err;
	}
	
	
	
	// no longer do this as it affects the interrupt read pipe for all ports
	// USBLog(5,"AppleUSBHub[%p]::DoPortAction - got _doPortActionLock, calling IncrementOutstandingIO", this);
	// IncrementOutstandingIO();

	port = _ports ? _ports[portIndex] : NULL;
    if (port)
    {
		// Keep a reference while we work with the port
		port->retain();		
		USBLog(5,"AppleUSBHub[%p]::DoPortAction(%s) for port (%d) of hub at 0x%x, options (0x%x) _portPMState (%d)", this, HubMessageToString(type), (uint32_t)portNumber, (uint32_t)_locationID, (uint32_t)options, port->_portPMState);
		
        switch ( type )
        {
            case kIOUSBMessageHubSuspendPort:

				IOUSBHubPortStatus		portStatus;
				IOReturn				localErr;
				
				// this completes essentially synchronously
				err = port->SuspendPort( true, true );
				localErr = GetPortStatus(&portStatus, portNumber);
				if ( localErr != kIOReturnSuccess )
				{
					USBLog(5, "AppleUSBHub[%p]::DoPortAction - GetPortStatus port[%p] returned 0x%x", this, port, localErr);
				}
				else 
				{
					USBLog(5, "AppleUSBHub[%p]::DoPortAction  port %d of hub at 0x%x after suspend - got statusFlags(%p) changeFlags(%p)", this, (uint32_t)portNumber, (uint32_t)_locationID, (void*)portStatus.statusFlags, (void*)portStatus.changeFlags);
					
					if ( !(portStatus.statusFlags & kHubPortSuspend) && (portStatus.statusFlags & kHubPortConnection) )
					{
						// We are not suspended! Somethings's amiss, but let our clients know that we did not suspend by sending a kIOUSBMessagePortHasBeenResumed message
						USBLog(1, "AppleUSBHub[%p]::DoPortAction  port %d of hub at 0x%x:  after suspend - we are not suspended, sending a kIOUSBMessagePortHasBeenResumed", this, (uint32_t)portNumber, (uint32_t)_locationID);
						port->_portPMState = usbHPPMS_active;
						port->MessageDeviceClients(kIOUSBMessagePortHasBeenResumed, &err, sizeof(IOReturn) );
					}
				}

				USBLog(5, "AppleUSBHub[%p]::DoPortAction - port[%p] now in state[%d]", this, port, port->_portPMState);
				break;

           case kIOUSBMessageHubResumePort:
				EnsureUsability();
                err = port->SuspendPort( false, true );
				USBLog(5, "AppleUSBHub[%p]::DoPortAction - port[%p] port %d of hub at 0x%x now in state[%d]", this, port, (uint32_t)portNumber, (uint32_t)_locationID, port->_portPMState);
				if (!err && (_myPowerState == kIOUSBHubPowerStateLowPower))
				{
					// this only happens with root hubs which are the only hubs we can communicate with while they are in low power mode
					// however, since there is no outstanding interrupt read, we need to check the port status ourselves
					if (!_isRootHub)
					{
						USBError(1, "AppleUSBHub[%p]::DoPortAction - resuming port %d of hub at 0x%x, but we are not a root hub - shouldn't be here", this, (uint32_t)portNumber, (uint32_t)_locationID);
					}
					else
					{
						int						retries = 5000;
						IOUSBHubPortStatus		portStatus;
						// since we won't have an interrupt read to complete the resume, we need to wait for it ourselves - it takes at least 20 ms
						// note - this is something that happens outside of the gate most of the time
						USBLog(5, "AppleUSBHub[%p]::DoPortAction - sleeping 20ms for the resume - onThread(%s)", this, _workLoop->onThread() ? "true" : "false");
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
                        {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHub[%p]::DoPortAction - IOSleep in gate:%d", this, 2);}}
#endif
						IOSleep(20);
						err = GetPortStatus(&portStatus, portNumber);
						while (!err && retries-- > 0)
						{
							USBLog(5, "AppleUSBHub[%p]::DoPortAction - got statusFlags(%p) changeFlags(%p)", this, (void*)portStatus.statusFlags, (void*)portStatus.changeFlags);
							if (portStatus.changeFlags & kHubPortSuspend)
							{
								ClearPortFeature(kUSBHubPortSuspendChangeFeature, portNumber);
								USBLog(3, "AppleUSBHub[%p]::DoPortAction - port[%p], which is number[%d] has the suspend change bit set- good - statusFlags[%p] and changeFlags[%p]", this, port, (int)portNumber, (void*)portStatus.statusFlags, (void*)portStatus.changeFlags);
								port->_portPMState = usbHPPMS_active;
								port->_resumePending = false;
								if ( port->_portDevice)
								{
									IOUSBDevice *cachedDevice = port->_portDevice;			// in case _portDevice goes away while we are messaging
									
									if (cachedDevice)
									{
										cachedDevice->retain();
										cachedDevice->message(kIOUSBMessagePortHasBeenResumed, cachedDevice, NULL);
										cachedDevice->release();
									}
								}
								break;
							}
							USBLog(3, "AppleUSBHub[%p]::DoPortAction - port still not resumed - sleeping 1ms before trying again - retries left (%d)", this, retries);
							IOSleep(1);
							err = GetPortStatus(&portStatus, portNumber);
						}
					}
				}
               break;

            case kIOUSBMessageHubReEnumeratePort:
                err = port->ReEnumeratePort(options);
                break;
				
            case kIOUSBMessageHubResetPort:
                err = port->ResetPort();
                break;
				
            case kIOUSBMessageHubPortClearTT:
                // ClearTT is only supported on HS Hubs
                //
                if ( _hsHub )
                    err = port->ClearTT(_multiTTs, options);
                else
                    err = kIOReturnUnsupported;
                break;
			
			case kIOUSBMessageHubSetPortRecoveryTime:
                break;
				
		}
		
		// and now, release our reference
		port->release();
    }

    // since we stopped doing the Increment above, we need to not do this either
	// USBLog(5,"AppleUSBHub[%p]::DoPortAction - calling DecrementOutstandingIO", this);
	// DecrementOutstandingIO();

	// since holding this lock can cause us to delay a close call, we will do an Increment/Decrement to cause a check here
	// we will Increment before we unlock the lock and Decrement afterwards
	IncrementOutstandingIO();
	ReleaseDoPortActionLock();
	DecrementOutstandingIO();
	
	USBLog(6,"AppleUSBHub[%p]::DoPortAction - calling LowerPowerState", this);
	LowerPowerState();

	// since a port action could have caused all the ports to now be suspended, spawn a thread to see
	// we do this because the outstanding io might be 1, because we have an outstanding read which will 
	// not complete in this case - JRH - 20071207 - only do this if we processed a suspend port
	if (!isInactive() && !_hubIsDead && !_checkPortsThreadActive && (type == kIOUSBMessageHubSuspendPort))
	{
		_abandonCheckPorts = false;
		_checkPortsThreadActive = true;
		retain();											// in case we get terminated while the thread is still waiting to be scheduled
		USBLog(7,"AppleUSBHub[%p]::DoPortAction - spawning _checkForActivePortsThread", this);
		if ( thread_call_enter(_checkForActivePortsThread) == TRUE )
		{
			USBLog(1,"AppleUSBHub[%p]::DoPortAction - _checkForActivePortsThread already queued", this);
			USBTrace( kUSBTHub, kTPHubDoPortAction, (uintptr_t)this, kIOUSBMessageHubSuspendPort, 0, 0);
			release();
		}
	}

	USBLog(5,"AppleUSBHub[%p]::DoPortAction(%s) for port (%d), returning 0x%x", this, HubMessageToString(type), (uint32_t)portNumber, err);
	
    return err;
}

void 
AppleUSBHub::InterruptReadHandlerEntry(OSObject *target, void *param, IOReturn status, UInt32 bufferSizeRemaining)
{
#pragma unused (param)
    AppleUSBHub *	me = OSDynamicCast(AppleUSBHub, target);

    if (!me)
        return;
    
	USBLog(7, "AppleUSBHub[%p]::InterruptReadHandlerEntry", me);
    me->InterruptReadHandler(status, bufferSizeRemaining);
	USBLog(6, "AppleUSBHub[%p]::InterruptReadHandlerEntry - calling DecrementOutstandingIO", me);
    me->DecrementOutstandingIO();
}



void 
AppleUSBHub::InterruptReadHandler(IOReturn status, UInt32 bufferSizeRemaining)
{
    bool			queueAnother = TRUE;
    IOReturn		err = kIOReturnSuccess;
	
	(void) OSCompareAndSwap(1, 0, &_interruptReadPending);

	USBTrace( kUSBTHub, kTPHubInterruptReadHandler, (uintptr_t)this, status, bufferSizeRemaining, 2);
	
#ifdef TEST_HUB_RECOVERY
	// The following code will simulate the Hub error recovery.  Just enable this code and attach a Q30 keyboard to
	// a laptop with nothing else attached.  Boot up, attach the kbd with 1 device attached to it and then remove the device.
	static			int count = 0;

	if ( !_isRootHub )
		count++;
	
	USBLog(3, "AppleUSBHub[%p]::InterruptReadHandler count: %d", this, count);

	if ( !_isRootHub and count >= 22 and count <26 )
		status = kIOReturnNotResponding;
#endif
	
	setProperty("Interrupt Pending", kOSBooleanFalse);

	switch (status)
	{
		case kIOReturnOverrun:
			USBLog(3, "AppleUSBHub[%p]::InterruptReadHandler kIOReturnOverrun error", this);
			// This is an interesting error, as we have the data that we wanted and more...  We will use this
			// data but first we need to clear the stall and reset the data toggle on the device.  We then just 
			// fall through to the kIOReturnSuccess case.
			
			if (!isInactive())
			{
				//
				// First, clear the halted bit in the controller
				//
				if ( _interruptPipe )
				{
					_interruptPipe->ClearStall();
					
					// And call the device to reset the endpoint as well
					//
					IncrementOutstandingIO();
					retain();
					if ( thread_call_enter(_clearFeatureEndpointHaltThread) == TRUE )
					{
						USBLog(3, "AppleUSBHub[%p]::InterruptReadHandler _clearFeatureEndpointHaltThread already queued - calling DecrementOutstandingIO", this);
						DecrementOutstandingIO();
						release();
					}
				}
			}
				
				// Fall through to process the data.
				
			case kIOReturnSuccess:
				
				_retryCount = kHubDriverRetryCount;
				
				if (!_ports)
				{
					USBLog(1, "AppleUSBHub[%p]::InterruptReadHandler - avoiding NULL _ports - unlike before!!", this);
					USBTrace( kUSBTHub, kTPHubInterruptReadHandler, (uintptr_t)this, kHubDriverRetryCount, 0, 1);
				}
			
				// Handle the data
				//
				if ( !_hubIsDead  && (_ports != NULL))
				{
					USBLog(6, "AppleUSBHub[%p]::InterruptReadHandler - calling IncrementOutstandingIO", this);
					IncrementOutstandingIO();
					USBLog(6, "AppleUSBHub[%p]::InterruptReadHandler - calling EnsureUsability", this);
					EnsureUsability();										// JRH: 06-05-2008 - rdar://5946536 make sure we are usable when we get a status change the DecrementOutstandingIO will undo this
					retain();
					if ( thread_call_enter(_workThread) == TRUE )
					{
						USBLog(3, "AppleUSBHub[%p]::InterruptReadHandler _workThread already queued - calling DecrementOutstandingIO", this);
						DecrementOutstandingIO();
						release();
					}
				}
				
				// Note that the workThread will requeue the interrupt, so we don't
				// need to do it again
				//
				queueAnother = FALSE;
				
				break;
				
			case kIOReturnNotResponding:
				// If our device has been disconnected or we're already processing a
				// terminate message, just go ahead and close the device (i.e. don't
				// queue another read.  Otherwise, go check to see if the device is
				// around or not. 
				//
				USBLog(3, "AppleUSBHub[%p]::InterruptReadHandler error kIOReturnNotResponding", this);
				
				if ( _hubHasBeenDisconnected || isInactive() )
				{
					queueAnother = false;
				}
				else
				{
					USBLog(3, "AppleUSBHub[%p]::InterruptReadHandler Checking to see if hub is still connected", this);
					
					CallCheckForDeadHub();
					
					// Note that since we don't do retries on the Hub, if we get a kIOReturnNotResponding error
					// we will either determine that the hub is disconnected or we will reset the hub.  In either
					// case, we will not need to requeue the interrupt, so we don't need to clear the stall.
					
					queueAnother = false;
					
				}
				
				break;
				
			case kIOReturnAborted:
				// This generally means that we are done, because we were unplugged, but not always
				//
				if (isInactive() || _hubIsDead || _abortExpected )
				{
					USBLog(3, "AppleUSBHub[%p]::InterruptReadHandler error kIOReturnAborted (expected)", this);
					queueAnother = false;
				}
				else
				{
					USBLog(3, "AppleUSBHub[%p]::InterruptReadHandler error kIOReturnAborted. Try again.", this);
				}
				break;
				
			case kIOReturnUnderrun:
			case kIOUSBPipeStalled:
			case kIOUSBLinkErr:
			case kIOUSBNotSent2Err:
			case kIOUSBNotSent1Err:
			case kIOUSBBufferUnderrunErr:
			case kIOUSBBufferOverrunErr:
			case kIOUSBWrongPIDErr:
			case kIOUSBPIDCheckErr:
			case kIOUSBDataToggleErr:
			case kIOUSBBitstufErr:
			case kIOUSBCRCErr:
				// These errors will halt the endpoint, so before we requeue the interrupt read, we have
				// to clear the stall at the controller and at the device.
				//
				USBLog(3, "AppleUSBHub[%p]::InterruptReadHandler error (0x%x) reading interrupt pipe", this, status);
				
				// 01-28-02 JRH If we are inactive, then we can ignore this
				//
				if (!isInactive())
				{
					// First, clear the halted bit in the controller
					//
					if ( _interruptPipe )
					{
						_interruptPipe->ClearStall();
						
						// And call the device to reset the endpoint as well
						//
						_needInterruptRead = true;
						retain();
						IncrementOutstandingIO();
						if ( thread_call_enter(_clearFeatureEndpointHaltThread) == TRUE )
						{
							USBLog(3, "AppleUSBHub[%p]::InterruptReadHandler _clearFeatureEndpointHaltThread already queued - calling DecrementOutstandingIO", this);
							DecrementOutstandingIO();
							release();
						}
					}
				}
					
					queueAnother = false;
				break;
				
			case kIOUSBHighSpeedSplitError:
				// If our device has been disconnected or we're already processing a
				// terminate message, just go ahead and close the device (i.e. don't
				// queue another read.  Otherwise, go check to see if the device is
				// around or not. 
				//
				USBLog(3, "AppleUSBHub[%p]::InterruptReadHandler error kIOUSBHighSpeedSplitError", this);
				
				if ( _hubHasBeenDisconnected || isInactive() )
				{
					queueAnother = false;
				}
				else
				{
					USBLog(3, "AppleUSBHub[%p]::InterruptReadHandler Checking to see if hub is still connected", this);
					
					CallCheckForDeadHub();
					
					// Note that since we don't do retries on the Hub, if we get a kIOReturnNotResponding error
					// we will either determine that the hub is disconnected or we will reset the hub.  In either
					// case, we will not need to requeue the interrupt, so we don't need to clear the stall.
					
					queueAnother = false;
					
				}
				
				break;
				
			default:
				USBLog(3,"AppleUSBHub[%p]::InterruptReadHandler error 0x%x reading interrupt pipe", this, status);
				if (isInactive() || ((_powerStateChangingTo != kIOUSBHubPowerStateStable) && (_powerStateChangingTo < kIOUSBHubPowerStateLowPower)) )
					queueAnother = false;
				else
				{
					// Clear the halted bit in the controller
					//
					if ( _interruptPipe )
						_interruptPipe->ClearStall();
				}
					break;
	}
    if ( queueAnother )
    {
        // RearmInterruptRead will close the device if it fails, so we don't need to do it here
        //
        _needInterruptRead = true;
    }
}



void
AppleUSBHub::ResetPortZeroEntry(OSObject *target)
{
    AppleUSBHub *	me = OSDynamicCast(AppleUSBHub, target);
    
    if (!me)
        return;
        
    me->ResetPortZero();
    me->DecrementOutstandingIO();
	me->release();
}



void
AppleUSBHub::ResetPortZero()
{
    AppleUSBHubPort 	*port;
    UInt32		currentPort;

	if (isInactive())
	{
		USBLog(4, "AppleUSBHub[%p]::ResetPortZero - called while inActive - ignoring", this);
		return;
	}

    // Find out which port we have to reset
    //
    if ( _ports) 
        for (currentPort = 1; currentPort <= _hubDescriptor.numPorts; currentPort++)
        {
            port = _ports ? _ports[currentPort-1] : NULL;
            if (port) 
            {
                Boolean	locked;
				port->retain();
                locked = port->GetDevZeroLock();
                if ( locked )
                {
                    // If the timeout flag for this port is already set, AND the timestamp is the same as when we
                    // first detected the lock, then is time to release the devZero lock
                    if ( ((_timeoutFlag & (1 << (currentPort-1))) != 0) && (_portTimeStamp[currentPort-1] == port->GetPortTimeStamp()) )
                    {
                        USBLog(1, "AppleUSBHub[%p]::ResetPortZero: - port %d of hub @ 0x%x- Releasing devZero lock", this, (uint32_t)currentPort, (uint32_t)_locationID);
						USBTrace( kUSBTHub, kTPHubResetPortZero, (uintptr_t)this, (uint32_t)currentPort, 0, 0);
                        _timeoutFlag &= ~( 1<<(currentPort-1));
                        port->ReleaseDevZeroLock();
                    }
                }
				port->release();
            }
        }
}

//
// ProcessStatusChanged
// This method will run on one of the shared kernel threads. It is called when an Async read
// on the interrupt pipe returns some data. It needs to issue another read when it is done
// processing, and then just return
//
void 
AppleUSBHub::ProcessStatusChangedEntry(OSObject *target)
{
    AppleUSBHub *	me = OSDynamicCast(AppleUSBHub, target);
    
    if (!me)
        return;
        
    me->ProcessStatusChanged();
	USBLog(6, "AppleUSBHub[%p]::ProcessStatusChangedEntry - calling DecrementOutstandingIO", me);
    me->DecrementOutstandingIO();
	me->release();
}



void 
AppleUSBHub::ProcessStatusChanged()
{
    const UInt8	*		statusChangedBitmapPtr = 0;
    int					portMask;
    int					portByte;
    int					portIndex, portNum;
    AppleUSBHubPort 	*port;
    bool				portSuccess = false;
    bool				hubStatusSuccess = true;

    if (isInactive() || !_buffer || !_ports)
        return;

    portMask = 2;
    portByte = 0;
    statusChangedBitmapPtr = (const UInt8*)_buffer->getBytesNoCopy();
    if (statusChangedBitmapPtr == NULL)
    {
        USBError(1, "AppleUSBHub[%p]::ProcessStatusChanged: No interrupt pipe buffer!", this);
    }
    else
    {
        if ( statusChangedBitmapPtr[0] == 0xff)
        {
            USBLog(5,"AppleUSBHub[%p]::ProcessStatusChanged found (FF) in statusChangedBitmap", this);
        }
        else
        {
			// We need to indicate that we want a read after all is said and done
			_needInterruptRead = true;
			
           if ((statusChangedBitmapPtr[0] & 1) != 0)
            {
				hubStatusSuccess = HubStatusChanged();
            }

            if ( hubStatusSuccess )
            {
				// check these again to make sure that we didn't go away while in HubStatusChanged
				if (isInactive() || !_buffer || !_ports)
				{
					USBLog(1,"AppleUSBHub[%p]::ProcessStatusChanged - in inner loop - we seem to have gone away. bailing", this);
					USBTrace( kUSBTHub, kTPHubProcessStateChanged, (uintptr_t)this, hubStatusSuccess, 0, 1 );
					return;
				}

                USBLog(6,"AppleUSBHub[%p](0x%x)::ProcessStatusChanged - calling IncrementOutstandingIO", this, (uint32_t)_locationID);
				IncrementOutstandingIO();				// once for the master count of this loop
                USBLog(5,"AppleUSBHub[%p]::ProcessStatusChanged found (0x%8.8x) in statusChangedBitmap", this, statusChangedBitmapPtr[0]);
				USBTrace(kUSBTEnumeration, kTPEnumerationProcessStatusChanged, (uintptr_t)this, (uintptr_t)statusChangedBitmapPtr[0], _locationID, 0);
				
                for (portIndex = 0; portIndex < _hubDescriptor.numPorts; portIndex++)
                {
					portNum = portIndex + 1;
                    if ((statusChangedBitmapPtr[portByte] & portMask) != 0)
                    {
						port = _ports ? _ports[portIndex] : NULL;
						if ( port )
						{
							IOReturn  kr;
							
							// rdar://6558060. make sure the system is not dozing. this is a light weight call
							USBLog(6,"AppleUSBHub[%p]::ProcessStatusChanged. Calling wakeFromDoze", this);
							getPMRootDomain()->wakeFromDoze();
							
							USBLog(6,"AppleUSBHub[%p]::ProcessStatusChanged Port %d of Hub at 0x%x, calling IncrementOutstandingIO and port(%p)->StatusChanged", this, portNum, (uint32_t)_locationID, port);
							// note that the StatusChanged call below will happen on another thread
							// which means that we will end up Rearming the interrupt read before it is actually done
							IncrementOutstandingIO();					// once for each port which changes
							RaisePowerState();							// same for this...	
							kr = WaitForPowerOn(2000);					// wait up to 2 seconds for the power to change.
							// if we are not in the On state, or we are in the process of swithing to LowPower, the we need to get to the On state..
							if (kr)
							{
								USBLog(3,"AppleUSBHub[%p]::ProcessStatusChanged - did not get power on - _myPowerState(%d) _powerStateChangingTo(%d)", this, (int)_myPowerState, (int)_powerStateChangingTo);
							}
							portSuccess = port->StatusChanged();
							if (! portSuccess )
							{
								USBLog(1,"AppleUSBHub[%p]::ProcessStatusChanged port->StatusChanged() returned false", this);
								USBTrace( kUSBTHub, kTPHubProcessStateChanged, (uintptr_t)this, portNum, (int)_myPowerState, (int)_powerStateChangingTo );
							}
						}
                    }

                    portMask <<= 1;
                    if (portMask > 0x80)
                    {
                        portMask = 1;
                        portByte++;
                    }
                }
                USBLog(6,"AppleUSBHub[%p]::ProcessStatusChanged - calling DecrementOutstandingIO", this);
                DecrementOutstandingIO();					// this is the master - will rearm the read if necessary
            }
			else
			{
                USBLog(3,"AppleUSBHub[%p]::ProcessStatusChanged - hubStatusSuccess(FALSE)", this);
				USBTrace( kUSBTHub, kTPHubProcessStateChanged, (uintptr_t)this, hubStatusSuccess, 0, 2 );
			}
        }
    }
}



IOReturn
AppleUSBHub::RearmInterruptRead()
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBCompletion	comp;
	bool			gotLock = false;
	
	gotLock = OSCompareAndSwap(0, 1, &_interruptReadPending);
	if ( !gotLock )
	{
		USBLog(6,"AppleUSBHub[%p]::RearmInterruptRead but read was already pending", this);
		err = kIOReturnNotPermitted;
		goto Exit;
	}

	USBLog(6,"AppleUSBHub[%p]::RearmInterruptRead", this);
	
	// only read if we are in ON state
	if (_myPowerState < kIOUSBHubPowerStateOn)
	{
		USBLog(1,"AppleUSBHub[%p]::RearmInterruptRead - unexpected power state (%d)", this, (int)_myPowerState);
		USBTrace( kUSBTHub, kTPHubRearmInterruptRead, (uintptr_t) this, (int)_myPowerState, kIOUSBHubPowerStateOn, 1 );
		_needInterruptRead = true;
		goto Exit;
	}	
	
	USBLog(6,"AppleUSBHub[%p]::RearmInterruptRead - calling IncrementOutstandingIO", this);
    IncrementOutstandingIO();			// make sure we don't get closed before the callback

	if (!_ports)
	{
		USBLog(1, "AppleUSBHub[%p]::RearmInterruptRead - avoiding NULL _ports - unlike before!!", this);
		USBTrace( kUSBTHub, kTPHubRearmInterruptRead, (uintptr_t) this, 0, 0, 2);
	}
	
    if ( isInactive() || (_buffer == NULL) || ( _interruptPipe == NULL ) || (_ports == NULL))
	{
		USBLog(6,"AppleUSBHub[%p]::RearmInterruptRead - inactive or other params are NULL.  Calling DecrementOutstandingIO", this);
		DecrementOutstandingIO();
        goto Exit;
	}
	
    comp.target = this;
    comp.action = (IOUSBCompletionAction) InterruptReadHandlerEntry;
    comp.parameter = NULL;
    _buffer->setLength(_readBytes);

    if ((err = _interruptPipe->Read(_buffer, &comp)))
    {
        USBLog(1,"AppleUSBHub[%p]::RearmInterruptRead error %x reading interrupt pipe - calling DecrementOutstandingIO", this, err);
		USBTrace( kUSBTHub, kTPHubRearmInterruptRead, (uintptr_t)this, err, _interruptReadPending, 3 );
		
        DecrementOutstandingIO();
    }

Exit:
	USBLog(6,"AppleUSBHub[%p]::RearmInterruptRead (0x%x)", this, err);
	if ( err )
	{
		(void) OSCompareAndSwap(1, 0, &_interruptReadPending);
	}
	else 
	{
		if (!isInactive())
			setProperty("Interrupt Pending", kOSBooleanTrue);
	}

	
    return err;
}


#ifdef SUPPORTS_SS_USB
void 
AppleUSBHub::PrintHubDescriptor(IOUSB3HubDescriptor *desc)
{
    int i = 0;
    const char *characteristics[] =
        { "ppsw", "nosw", "comp", "ppoc", "nooc", 0 };


    if (desc->length == 0) return;

    IOLog("hub descriptor: (%d bytes)\n", desc->length);
    IOLog("\thubType = %d\n", desc->hubType);
    IOLog("\tnumPorts = %d\n", desc->numPorts);
    IOLog("\tcharacteristics = %x ( ",
                                USBToHostWord(desc->characteristics));
    do
    {
        if (USBToHostWord(desc->characteristics) & (1 << i))
            IOLog("%s ", characteristics[i]);
    } while (characteristics[++i]);
    IOLog(")\n");
    IOLog("\tpowerOnToGood = %d ms\n", desc->powerOnToGood * 2);
    IOLog("\thubCurrent = %d\n", desc->hubCurrent);
    IOLog("\tremovablePortFlags = %p %p\n", &desc->removablePortFlags[1], &desc->removablePortFlags[0]);
    IOLog("\tpwrCtlPortFlags    = %p %p\n", &desc->pwrCtlPortFlags[1], &desc->removablePortFlags[0]);
}
#else
void 
AppleUSBHub::PrintHubDescriptor(IOUSBHubDescriptor *desc)
{
    int i = 0;
    const char *characteristics[] =
        { "ppsw", "nosw", "comp", "ppoc", "nooc", 0 };


    if (desc->length == 0) return;

    IOLog("hub descriptor: (%d bytes)\n", desc->length);
    IOLog("\thubType = %d\n", desc->hubType);
    IOLog("\tnumPorts = %d\n", desc->numPorts);
    IOLog("\tcharacteristics = %x ( ",
                                USBToHostWord(desc->characteristics));
    do
    {
        if (USBToHostWord(desc->characteristics) & (1 << i))
            IOLog("%s ", characteristics[i]);
    } while (characteristics[++i]);
    IOLog(")\n");
    IOLog("\tpowerOnToGood = %d ms\n", desc->powerOnToGood * 2);
    IOLog("\thubCurrent = %d\n", desc->hubCurrent);
    IOLog("\tremovablePortFlags = %p %p\n", &desc->removablePortFlags[1], &desc->removablePortFlags[0]);
    IOLog("\tpwrCtlPortFlags    = %p %p\n", &desc->pwrCtlPortFlags[1], &desc->removablePortFlags[0]);
}
#endif

IOReturn 
AppleUSBHub::DoDeviceRequest(IOUSBDevRequest *request)
{
    IOReturn err;
    
#if 0
	if (_myPowerState < kIOUSBHubPowerStateLowPower)
	{
		char*		bt[8];
		
		OSBacktrace((void**)bt, 8);
		
		USBLog(4, "AppleUSBHub[%p]::DoDeviceRequest - while _myPowerState(%d) _powerStateChangingTo(%d), bt:[%p][%p][%p][%p][%p][%p][%p]", this, (int)_myPowerState, (int)_powerStateChangingTo, bt[1], bt[2], bt[3], bt[4], bt[5], bt[6], bt[7]);
	}
#endif

    // Paranoia:  if we don't have a device ('cause it was stop'ped), then don't send
    // the request.  If our hub is being terminated (inActive), then do do it either.
    //
    if ( isInactive() || !_device || _device->isInactive() || !_device->isOpen(this))
	{
		USBLog(1, "AppleUSBHub[%p]::DoDeviceRequest - _device(%p) isInactive(%s) isOpen(%s)  - returning kIOReturnNoDevice", this, _device, (_device && !_device->isInactive()) ? "false" : "true", (_device && _device->isOpen(this)) ? "true" : "false");
		USBTrace( kUSBTHub, kTPHubDoDeviceRequest, (uintptr_t)this, kIOReturnNoDevice, 0, 0 );
        err = kIOReturnNoDevice;
	}
	else
	{
		USBLog(_powerStateChangingTo != -1 ? 5 : 7, "AppleUSBHub[%p]::DoDeviceRequest - _device[%p](%s) _myPowerState[%d] _powerStateChangingTo[%d] _portSuspended[%s]", this, _device, _device->getName(), (int)_myPowerState, (int)_powerStateChangingTo, _portSuspended ? "true" : "false");
		// If we are suspended, we need to first wake up
		if ( _portSuspended )
		{
			USBLog(5,"AppleUSBHub[%p]::DoDeviceRequest, unsuspending port", this);
			err = _device->SuspendDevice(false);
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
            {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHub[%p]::DoDeviceRequest - IOSleep in gate:%d", this, 1);}}
#endif
			IOSleep(_hubResumeRecoveryTime);		// give ourselves time to recover if we are suspended
			if ( err != kIOReturnSuccess )
				return err;
		}		

        err = _device->DeviceRequest(request, 5000, 0);
	}
        
	USBLog(err != kIOReturnSuccess ? 5 : 7, "AppleUSBHub[%p]::DoDeviceRequest - returning err (0x%x)", this, (uint32_t)err);
	return err;
}


IOReturn
AppleUSBHub::DoDeviceRequestWithRetries(IOUSBDevRequest *request, bool retrySTALLs)
{
    IOReturn err = kIOReturnNoDevice;
	int tryCount = 3;		// Will try 3 times, initial try, and 2 retries
	int abortedCount = 100;
	int sleep = 3;
		
	while (tryCount-- > 0 && !isInactive())
	{
		err = DoDeviceRequest(request);
		
		if (err == kIOReturnAborted)
		{
			// Don't fail because of aborted status, unless its happening rediculously often.
			if(abortedCount-- < 1)
			{
				USBLog(3, "AppleUSBHub[%p]::DoDeviceRequestWithRetries - retried aborted (%p) 100 times, giving up", this, (void*)err);
				break;
			}
			USBLog(5, "AppleUSBHub[%p]::DoDeviceRequestWithRetries - retrying abort(%p), sleeping: %d", this, (void*)err, sleep);
			tryCount++;	// This doesn't count against retries
			continue;
		}
		
		if (err == kIOReturnSuccess)
		{
			break;
		}
		
		if (_hubIsDead)
		{
			USBLog(5, "AppleUSBHub[%p]::DoDeviceRequestWithRetries - _hubISDead is true", this);
			err = kIOReturnNoDevice;
			break;
		}
		
		if ( (err == kIOReturnNoDevice) || (err == kIOUSBTransactionTimeout) || ( err == kIOUSBDeviceTransferredToCompanion) || ( err == kIOReturnUnsupported) || ( err == kIOUSBEndpointNotFound) )
		{
			break;
		}
		
		if (!retrySTALLs && (err == kIOUSBPipeStalled) )
		{
			break;
		}
		
		if (tryCount < 2)
		{
			sleep = 30;
		}
		// Do we want to distinguish transaction errors from other errors?
		// Transaction errors are:
		// kIOUSBLinkErr < err < kIOUSBCRCErr or kIOReturnUnderrun, kIOReturnOverrun, kIOReturnNotResponding
		// kIOUSBPipeStalled is handled differently
		
		USBLog(5, "AppleUSBHub[%p]::DoDeviceRequestWithRetries - retrying err(%p), try count: %d, sleeping: %d", this, (void*)err, tryCount, sleep);

		// Give things a chance to settle down
		if ( _workLoop->inGate() )
		{
			IOReturn  kr;
			long nothing;
			AbsoluteTime	deadline;
			
			clock_interval_to_deadline(sleep, kMillisecondScale, &deadline);
			
			kr = _gate->commandSleep(&nothing, deadline, THREAD_ABORTSAFE);
			
			if ( !_workLoop->inGate() )
			{
				USBLog(1, "AppleUSBHub[%p]::DoDeviceRequestWithRetries - No longer have the gate after sleeping. kr == %p", this, (void*)kr );
			}
			else if(kr != THREAD_TIMED_OUT)
			{
				USBLog(5, "AppleUSBHub[%p]::DoDeviceRequestWithRetries - unexpected return from commandSleep: %p", this, (void*)kr);
			}
		}
		else 
		{
			IOSleep(sleep);			
		}

	}
	
    if ( err == kIOUSBTransactionTimeout )
	{
        // If we get an kIOUSBTransactionTimeout, then we better reset our hub
        //
		USBLog(1, "AppleUSBHub[%p]::DoDeviceRequestWithRetries - err kIOUSBTransactionTimeout - reseting my port", this);
        retain();
        ResetMyPort();
        release();
        
    }

	USBLog(err != kIOReturnSuccess ? 5 : 7, "AppleUSBHub[%p]::DoDeviceRequestWithRetries - returning err (0x%x)", this, (uint32_t)err);
	return err;
	
}



void 
AppleUSBHub::TimeoutOccurred(OSObject *owner, IOTimerEventSource *sender)
{
#pragma unused (sender)
    AppleUSBHub			*me;
    AppleUSBHubPort 	*port;
    UInt32				currentPort;
	IOReturn			kr = kIOReturnSuccess;
	bool				checkPorts = false;
	
    me = OSDynamicCast(AppleUSBHub, owner);
    if (!me)
        return;

	if (me->isInactive())
	{
		USBLog(4, "AppleUSBHub[%p]::TimeoutOccurred - called while inActive - ignoring", me);
		return;
	}
	
	me->retain();
	
	// Only check for ports every kDevZeroTimeoutCount counts
	if ( --me->_devZeroLockedTimeoutCounter == 0 )
	{
		checkPorts = true;
		me->_devZeroLockedTimeoutCounter = kDevZeroTimeoutCount;
	}

    if ( checkPorts && me->_ports) 
	{
        for (currentPort = 1; currentPort <= me->_hubDescriptor.numPorts; currentPort++)
        {
			port = me->_ports ? me->_ports[currentPort-1] : NULL;
            if (port) 
            {
                Boolean	locked;
				
				port->retain();
				
                locked = port->GetDevZeroLock();
                if ( locked )
                {
                    // If the timeout flag for this port is already set, AND the timestamp is the same as when we
                    // first detected the lock, then is time to release the devZero lock
                    if ( ((me->_timeoutFlag & (1 << (currentPort-1))) != 0) && (me->_portTimeStamp[currentPort-1] == port->GetPortTimeStamp()) )
                    {
                        // Need to call through a separate thread because we will end up making synchronous calls
                        // to the USB bus.  That thread will clear the timeoutFlag for this port.
                        USBLog(3,"AppleUSBHub[%p]::TimeoutOccurred error - calling IncrementOutstandingIO", me);
                        me->IncrementOutstandingIO();
						me->retain();
                        if ( thread_call_enter(me->_resetPortZeroThread) == TRUE )
						{
							USBLog(3, "AppleUSBHub[%p]::TimeoutOccurred _resetPortZeroThread already queued - calling DecrementOutstandingIO", me);
							me->DecrementOutstandingIO();
							me->release();
						}
					}
                    else
                    {
                        // Set the timeout flag for this port
                        me->_timeoutFlag |= (1<<(currentPort-1));
                        
                        // Set the timestamp for the port
                        me->_portTimeStamp[currentPort-1] = port->GetPortTimeStamp();
                    }
                }
                else
                {
                    // Port is not locked, make sure that we clear the timeoutFlag for this port and reset the timestamp
                    me->_timeoutFlag &= ~( 1<<(currentPort-1));
                    me->_portTimeStamp[currentPort-1] = 0;
                }
				
				port->release();
            }
        }
	}
	
    /*
     * Restart the watchdog timer
     */
    if (me->_timerSource && !me->isInactive() )
    {
        // me->retain();
        me->_timerSource->setTimeoutMS(kWatchdogTimerPeriod);
    }
    me->release();

}/* end timeoutOccurred */


//=============================================================================================
//
//  CallCheckForDeadHub
//  This is called by the hub driver if the interrupt pipe comes back with a NotResponding error
//  or by the hub port driver if somethine has gone wrong trying to talk to the hub (e.g. a port reset
//  fails) It increments the IO count and spins off a new thread to actually do the checking
//
//=============================================================================================
//
void
AppleUSBHub::CallCheckForDeadHub(void)
{
	if (isInactive())
	{
		USBLog(4, "AppleUSBHub[%p]::CallCheckForDeadHub - called while inActive - ignoring", this);
		return;
	}
	
    IncrementOutstandingIO();
	retain();
    if ( thread_call_enter(_hubDeadCheckThread) == TRUE )
	{
		USBLog(3, "AppleUSBHub[%p]::CallCheckForDeadHub _hubDeadCheckThread already queued - calling DecrementOutstandingIO", this);
		DecrementOutstandingIO();
		release();
	}
}



//=============================================================================================
//
//  CheckForDeadHub is called when we get a kIODeviceNotResponding error in our interrupt pipe.
//  This can mean that (1) the device was unplugged, or (2) we lost contact
//  with our hub.  In case (1), we just need to close the driver and go.  In
//  case (2), we need to ask if we are still attached.  If we are, then we update 
//  our retry count.  Once our retry count (3 from the 9 sources) are exhausted, then we
//  issue a DeviceReset to our provider, with the understanding that we will go
//  away (as an interface).
//
//=============================================================================================
//
void 
AppleUSBHub::CheckForDeadHubEntry(OSObject *target)
{
    AppleUSBHub *	me = OSDynamicCast(AppleUSBHub, target);
    
    if (!me)
        return;
     
    me->CheckForDeadHub();
	USBLog(6, "AppleUSBHub[%p]::CheckForDeadHubEntry - calling DecrementOutstandingIO", me);
    me->DecrementOutstandingIO();
	me->release();
}



void 
AppleUSBHub::CheckForDeadHub()
{
    IOReturn			err = kIOReturnSuccess;
	bool				gotLock;
    
	if (isInactive())
	{
		USBLog(4, "AppleUSBHub[%p]::CheckForDeadHub - called while inActive - ignoring", this);
		USBTrace( kUSBTHub,  kTPHubCheckForDeadDevice, (uintptr_t)this, 0, 0, 0);
		return;
	}

	if ( _hubIsDead )
	{
		USBLog(4, "AppleUSBHub[%p]::CheckForDeadHub - hubIsDead is true, returning", this);
		USBTrace( kUSBTHub,  kTPHubCheckForDeadDevice, (uintptr_t)this, 0, 0, 1);
		return;
	}
	
 	gotLock = OSCompareAndSwap(0, 1, &_hubDeadCheckLock);
	if ( !gotLock )
	{
        USBLog(5, "AppleUSBHub[%p]::CheckForDeadHub  already active, returning", this );
		USBTrace( kUSBTHub,  kTPHubCheckForDeadDevice, (uintptr_t)this, _retryCount, _hubHasBeenDisconnected, 10);
		return;
	}

	USBTrace( kUSBTHub,  kTPHubCheckForDeadDevice, (uintptr_t)this, _retryCount, _hubHasBeenDisconnected, 2);

	// Are we still connected?
    //
    if ( _device && _hubInterface && _interruptPipe)
    {
		UInt32		info = 0;
		
		_device->retain();
		err = _device->GetDeviceInformation(&info);
		_device->release();
	
		USBLog(6, "AppleUSBHub(%s)[%p]::CheckForDeadHub  GetDeviceInformation returned error 0x%x, info: 0x%x, retryCount: %d", getName(), this, err, (uint32_t)info, (uint32_t)_retryCount);
		
		// if we get an error from GetDeviceInformation, do NOT treat it like the device has been disconnected.  Just lower our retry count AND if we have reached 0, then assume it has been disconnected
		if ( err != kIOReturnSuccess )
		{
			USBTrace( kUSBTHub,  kTPHubCheckForDeadDevice, (uintptr_t)this, err, _retryCount, 3);
			if (_retryCount != 0)
				_retryCount--;
			
			if (_retryCount == 0 )
            {
	            _hubHasBeenDisconnected = TRUE;
				
				USBLog(5, "AppleUSBHub(%s)[%p]::CheckForDeadHub:  GetDeviceInformation returned 0x%x and our retryCount is 0, so assume device has been unplugged", getName(), this, err);
				USBTrace( kUSBTHub,  kTPHubCheckForDeadDevice, (uintptr_t)this, 0, 0, 4);
			}
			else 
			{
				USBLog(5, "AppleUSBHub(%s)[%p]::CheckForDeadHub:  GetDeviceInformation returned 0x%x and our retryCount is not 0", getName(), this, err);
				USBTrace( kUSBTHub,  kTPHubCheckForDeadDevice, (uintptr_t)this, 0, 0, 5);
				_needInterruptRead = true;
				if ( _interruptPipe )
					_interruptPipe->ClearPipeStall(true);
			}
		}
		else 
		{
			USBTrace( kUSBTHub,  kTPHubCheckForDeadDevice, (uintptr_t)this, info, _retryCount, 6);
			
			if ((info & kUSBInformationDeviceIsConnectedMask) && !isInactive())
			{
				if (info & kUSBInformationDeviceIsEnabledMask)
				{
					USBTrace( kUSBTHub,  kTPHubCheckForDeadDevice, (uintptr_t)this, _retryCount, 0, 13);

					// If we are connected, we want to rearm the interrupt read when appropriate, so say so
					_needInterruptRead = true;
					
					// Looks like the device is still plugged in.  Have we reached our retry count limit?
					//
					if (_retryCount != 0)
						_retryCount--;
					
					if (_retryCount == 0 )
					{
						USBLog(1, "AppleUSBHub[%p]::CheckForDeadHub - Still connected and retry count reached, calling ResetMyPort()", this);
						USBTrace( kUSBTHub,  kTPHubCheckForDeadDevice, (uintptr_t)this, 0, 0, 12);

						retain();
						ResetMyPort();
						release();
					}
					else
					{
						USBLog(5, "AppleUSBHub[%p]::CheckForDeadHub - Still connected but retry count (%d) not reached, clearing stall and retrying", this, (uint32_t)_retryCount);
						USBTrace( kUSBTHub,  kTPHubCheckForDeadDevice, (uintptr_t)this, 0, 0, 7);
						
						// First, clear the halted bit in the controller and the device and re-arm the interrupt
						//
						if ( _interruptPipe )
							_interruptPipe->ClearPipeStall(true);
					}
				}
				else
				{
					// if the connected bit is set but the enabled bit is NOT set, then either we will be getting 
					// a termination soon (because of a connection change) or we won't, because the port got 
					// disabled without registering a connection change. We wait 300ms to see if we are getting terminated
					// and if not, we go ahead and reset the port instead
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
                    {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHub[%p]::CheckForDeadHub - IOSleep in gate:%d", this, 1);}}
#endif
					IOSleep(300);
					if (isInactive())
					{
						_hubHasBeenDisconnected = TRUE;
						USBLog(5, "AppleUSBHub[%p]::CheckForDeadHub - Getting a termination soon.. Just bailing", this);
						USBTrace( kUSBTHub,  kTPHubCheckForDeadDevice, (uintptr_t)this, (uintptr_t)_device, 0, 8);
					}
					else
					{
						USBLog(1, "AppleUSBHub[%p]::CheckForDeadHub - Still connected but disabled, calling ResetMyPort()", this);
						USBTrace( kUSBTHub,  kTPHubCheckForDeadDevice, (uintptr_t)this, 0, 0, 14);
						
						retain();
						ResetMyPort();
						release();
					}

				}

			}
			else
			{
				// Device is not connected -- our device has gone away.  The message kIOServiceIsTerminated
				// will take care of shutting everything down.  
				//
				_hubHasBeenDisconnected = TRUE;
				USBLog(5, "AppleUSBHub[%p]::CheckForDeadHub - device has been unplugged", this);
				USBTrace( kUSBTHub,  kTPHubCheckForDeadDevice, (uintptr_t)this, (uintptr_t)_device, 0, 8);
			}
		}
    }
    else
    {
        USBLog(3,"AppleUSBHub[%p]::CheckForDeadHub -- no hubInterface (%p), device(%p), or pipe(%p)", this, _hubInterface, _device, _interruptPipe);
		USBTrace( kUSBTHub,  kTPHubCheckForDeadDevice, (uintptr_t)this, (uintptr_t)_hubInterface, (uintptr_t)_device, 9);
    }

	// Release our lock
	_hubDeadCheckLock = 0;
}



//=============================================================================================
//
//  ClearFeatureEndpointHaltEntry is called when we get an error from our interrupt read
//  (except for kIOReturnNotResponding  which will check for a dead device).  In these cases
//  we need to clear the halted bit in the controller AND we need to reset the data toggle on the
//  device.
//
//=============================================================================================
//
void
AppleUSBHub::ClearFeatureEndpointHaltEntry(OSObject *target)
{
    AppleUSBHub *	me = OSDynamicCast(AppleUSBHub, target);

    if (!me)
        return;

    me->ClearFeatureEndpointHalt();
	USBLog(6, "AppleUSBHub[%p]::ClearFeatureEndpointHaltEntry - calling DecrementOutstandingIO", me);
    me->DecrementOutstandingIO();
	me->release();
}



void
AppleUSBHub::ClearFeatureEndpointHalt( )
{
    IOReturn			status = kIOReturnSuccess;
    IOUSBDevRequest		request;
    UInt32			retries = 2;
    
	if (isInactive())
	{
		USBLog(4, "AppleUSBHub[%p]::ClearFeatureEndpointHalt - called while inActive - ignoring", this);
		return;
	}
	
    // Clear out the structure for the request
    //
    bzero( &request, sizeof(IOUSBDevRequest));
    
    while ( (retries > 0) && (_interruptPipe) )
    {
        retries--;
        
        // Build the USB command to clear the ENDPOINT_HALT feature for our interrupt endpoint
        //
        request.bmRequestType 	= USBmakebmRequestType(kUSBNone, kUSBStandard, kUSBEndpoint);
        request.bRequest        = kUSBRqClearFeature;
        request.wValue		= kUSBFeatureEndpointStall;
        request.wIndex		= _interruptPipe->GetEndpointNumber() | 0x80 ; // bit 7 sets the direction of the endpoint to IN
        request.wLength		= 0;
        request.pData 		= NULL;
        
        // Send the command over the control endpoint
        //
        status = _device->DeviceRequest(&request, 5000, 0);
        
        if ( status != kIOReturnSuccess )
        {
            USBLog(3, "AppleUSBHub[%p]::ClearFeatureEndpointHalt -  DeviceRequest returned: 0x%x, retries = %d", this, status, (uint32_t)retries);
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
            {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHub[%p]::ClearFeatureEndpointHalt - IOSleep in gate:%d", this, 1);}}
#endif
            IOSleep(100);
        }
        else
            break;
    }
    
}



//=============================================================================================
//
//  CheckForActivePortsEntry is called when we think that we are done with the hub state machine
//  at least for a while. If all ports are either disconnected or suspended, then we will call
//	changePowerStateToPriv to lower the state. Note that the ports themselves have a separate
//	power state by virtue of changePowerStateTo
//=============================================================================================
//
void
AppleUSBHub::CheckForActivePortsEntry(OSObject *target)
{
    AppleUSBHub *	me = OSDynamicCast(AppleUSBHub, target);
	
	if (!me)
        return;
	
 	USBLog(6, "AppleUSBHub[%p]::CheckForActivePortsEntry hub at 0x%x (_parentHubHasNoSuspendDownstreamErrataBit = %d)", me, (uint32_t)me->_locationID, me->_parentHubHasNoSuspendDownstreamErrataBit);
   	me->CheckForActivePorts();
	USBLog(6, "AppleUSBHub[%p]::CheckForActivePortsEntry - setting _checkPortsThreadActive to false ", me);
	me->_checkPortsThreadActive = false;
	me->release();
}



void
AppleUSBHub::CheckForActivePorts( )
{
	int		msToDelay = kUSBRootHubPollingRate;							// wait 32 ms (the root hub timer period) before possible changing the state
	
	if (isInactive())
	{
		USBLog(4, "AppleUSBHub[%p]::CheckForActivePorts - called while inActive - ignoring", this);
		return;
	}

	
	// If this hub doesn't allow low power mode, then just return
	if ( _dontAllowLowPower )
	{
		USBLog(4, "AppleUSBHub[%p]::CheckForActivePorts - this hub does not allow low power, so abandoning", this);
		return;
	}

	if (_powerStateChangingTo != kIOUSBHubPowerStateStable)
	{
		USBLog(4, "AppleUSBHub[%p]::CheckForActivePorts - in the middle of a power change, so abandoning", this);
		return;
	}
	
	while (msToDelay--)
	{
		if (_abandonCheckPorts)
		{
			USBLog(4, "AppleUSBHub[%p]::CheckForActivePorts - abandoning ship before checking ports!!", this);
			_abandonCheckPorts = false;
			return;
		}
		IOSleep(1);					// delay one ms each time around
	}
	if (_abandonCheckPorts)
	{
		USBLog(4, "AppleUSBHub[%p]::CheckForActivePorts - abandoning ship before checking ports!!", this);
		_abandonCheckPorts = false;
		return;
	}
	USBLog(7, "AppleUSBHub[%p]::CheckForActivePorts - checking for disconnected ports", this);
	if (HubAreAllPortsDisconnectedOrSuspended())
	{
		if (_abandonCheckPorts)
		{
			USBLog(4, "AppleUSBHub[%p]::CheckForActivePorts - abandoning ship!!", this);
			_abandonCheckPorts = false;
			return;
		}
		USBLog(4, "AppleUSBHub[%p]::CheckForActivePorts - lowering power state", this);
		_dozeEnabled = true;
		changePowerStateToPriv(kIOUSBHubPowerStateLowPower);
		// note that the state could go back up with a call to EnsureUsability
	}
}



//=============================================================================================
//
//  WaitForPortResumesEntry is called when we are waking up from sleep, and we have resume calls
//	outstanding on ports which had been suspended by the power manager. since we are not yet
//  in the ON state, we don't have an interrupt read pending, so we can't let the port status
//	change handler deal with it. Instead we poll the individual port status change bits
//	ourselves until all ports are in the active state..
//=============================================================================================
//
void
AppleUSBHub::WaitForPortResumesEntry(OSObject *target)
{
    AppleUSBHub *	me = OSDynamicCast(AppleUSBHub, target);
	
    if (!me)
        return;
	
    me->WaitForPortResumes();
	me->DecrementOutstandingIO();
	me->release();
}



void
AppleUSBHub::WaitForPortResumes( )
{
	int						portIndex;
	int						portNum;
	bool					needToWait = true;
    AppleUSBHubPort *		port;
	IOUSBHubPortStatus		portStatus;
	IOReturn				kr;
	int						innerRetries = 30;						// wait at most 30 milliseconds (50% margin)
	int						outerRetries = 100;						// total wait of 3 seconds
	bool					dealWithPort[_hubDescriptor.numPorts];
	bool					recoveryNeeded = true;

	// We need to run this code even if we are inactive, as there are some Outstanding IO's that need to be reaped. 
	
	if (!_ports)
		return;
	
	for (portIndex = 0; portIndex < _hubDescriptor.numPorts; portIndex++)
		dealWithPort[portIndex] = false;
	
	USBLog(3, "AppleUSBHub[%p]::WaitForPortResumes - checking for ports which need to be resumed", this);
	while (needToWait && (outerRetries-- > 0))
	{
		innerRetries = 30;
		while (needToWait && (innerRetries-- > 0))
		{
			needToWait = false;							// try to get out of here...
			for (portIndex = 0; portIndex < _hubDescriptor.numPorts; portIndex++)
			{
				portNum = portIndex+1;
				port = _ports ? _ports[portIndex] : NULL;
				if (port)
				{
					if (port->_portPMState == usbHPPMS_pm_suspended)
					{
						USBLog(3, "AppleUSBHub[%p]::WaitForPortResumes - port[%p] which is port number (%d) still waiting", this, port, portNum);
						portStatus.statusFlags = 0;
						portStatus.changeFlags = 0;
						kr = GetPortStatus(&portStatus, portNum);
						if (kr)
						{
							USBLog(1, "AppleUSBHub[%p]::WaitForPortResumes - err (%p) from GetPortStatus for port (%d) - setting port to active", this, (void*)kr, portNum);
							USBTrace( kUSBTHub, kTPHubWaitForPortResumes, (uintptr_t)this, kr, portNum, 1 );
							dealWithPort[portIndex] = true;
							port->_portPMState = usbHPPMS_active;
							port->_resumePending = false;
						}
                        
#ifdef SUPPORTS_SS_USB
						// For super speed hubs, according to rdar://10403546, we cannot rely on the link state polling when transitioning from U3 to U0.  We have to rely on a PLC (Port Link State Change).  By the time that the PLC is
						// generated, the PLS will be valid.  So, the following code for SS Hubs will look to see if we have a PLC.  So, if is set, then get the port status again
						if ( (portStatus.changeFlags & kHubPortSuspend) || (_ssHub && (portStatus.changeFlags & kSSHubPortChangePortLinkStateMask)) )
#else
						if (portStatus.changeFlags & kHubPortSuspend)
#endif
							{
								int						nextIndex;
								AppleUSBHubPort *		nextPort = NULL;
								IOReturn                kr = kIOReturnSuccess;
								
								if (portStatus.changeFlags & kHubPortSuspend)
								{
									kr = ClearPortFeature(kUSBHubPortSuspendChangeFeature, portNum);
									if ( kr != kIOReturnSuccess)
									{
										USBLog(3, "AppleUSBHub[%p]::WaitForPortResumes - ClearPortFeature(kUSBHubPortSuspendChangeFeature, %d) returned 0x%x", this, portNum, (uint32_t)kr);
									}
								}
								
#ifdef SUPPORTS_SS_USB
								if (_ssHub && (portStatus.changeFlags & kSSHubPortChangePortLinkStateMask))
								{
									// Since we have a PLC, get the port status again to get the PLS
									kr = GetPortStatus(&portStatus, portNum);
									if (kr)
									{
										USBLog(1, "AppleUSBHub[%p]::WaitForPortResumes #2 - err (%p) from GetPortStatus for port (%d) - setting port to active", this, (void*)kr, portNum);
										USBTrace( kUSBTHub, kTPHubWaitForPortResumes, (uintptr_t)this, kr, portNum, 1 );
										dealWithPort[portIndex] = true;
										port->_portPMState = usbHPPMS_active;
										port->_resumePending = false;
									}
									else
									{
										// If the link state is not U0, then log this:
										if ( GETLINKSTATE(portStatus.statusFlags) != kSSHubPortLinkStateU0)
										{
											USBLog(1, "AppleUSBHub[%p]::WaitForPortResumes - port[%p], which is number[%d] in not in kSSHubPortLinkStateU0 as it should be.  status(0x%04x)/change(0x%04x), link state: 0x%x (%s)", this, port, portNum, portStatus.statusFlags, portStatus.changeFlags, GETLINKSTATE(portStatus.statusFlags), LinkStateName(GETLINKSTATE(portStatus.statusFlags)));
										}
										else
										{
											USBLog(6, "AppleUSBHub[%p]::WaitForPortResumes - port[%p], which is number[%d] had status(0x%04x)/change(0x%04x), link state: 0x%x (%s), clearing PLC", this, port, portNum, portStatus.statusFlags, portStatus.changeFlags, GETLINKSTATE(portStatus.statusFlags), LinkStateName(GETLINKSTATE(portStatus.statusFlags)));
											kr = ClearPortFeature(kUSBHubPortLinkStateChangeFeature, portNum);
											if ( kr != kIOReturnSuccess)
											{
												USBLog(3, "AppleUSBHub[%p]::WaitForPortResumes - ClearPortFeature(kUSBHubPortLinkStateChangeFeature, %d) returned 0x%x", this, portNum, (uint32_t)kr);
											}
										}
									}
								}
#endif
								
								dealWithPort[portIndex] = true;
								USBLog(3, "AppleUSBHub[%p]::WaitForPortResumes - port[%p], which is number[%d] has the suspend change bit set- good - statusFlags[%p] and changeFlags[%p]", this, port, portNum, (void*)portStatus.statusFlags, (void*)portStatus.changeFlags);
								port->_portPMState = usbHPPMS_active;
								if (port->_resumePending)
								{
									port->_resumePending = false;
									if (port->_lowerPowerStateOnResume)
									{
										port->_lowerPowerStateOnResume = false;
										USBLog(5, "AppleUSBHub[%p]::WaitForPortResumes - calling LowerPowerState after clearing _lowerPowerStateOnResume for port %p", this, port);
										LowerPowerState();
									}
								}
								for (nextIndex = portIndex+1; nextIndex < _hubDescriptor.numPorts; nextIndex++)
								{
									nextPort = _ports ? _ports[nextIndex] : NULL;
									if (nextPort && (nextPort->_portPMState == usbHPPMS_pm_suspended))
									{
										// reissue the command which was issued earlier and apparently didn't stick
										USBLog(3, "AppleUSBHub[%p]::WaitForPortResumes - since port %d is now done, issuing the resume for port %d", this, portNum, nextIndex+1);
										kr = nextPort->SuspendPort(false, false);					
										if (kr)
										{
											USBLog(1, "AppleUSBHub[%p]::WaitForPortResumes - err (%p) from SuspendPort for port (%d)", this, (void*)kr, nextIndex+1);
											USBTrace( kUSBTHub, kTPHubWaitForPortResumes, (uintptr_t)this, kr, nextIndex+1, 3 );
											dealWithPort[nextIndex] = true;
											nextPort->_portPMState = usbHPPMS_active;		// give up on this port i guess
											nextPort->_resumePending = false;
										}
										else
										{
											needToWait = true;					// to send us back up
											innerRetries = 30;					// and start us all over with this new port
											break;								// break out of the for loop so we only do this once
										}
									}
								}
							}
							else
							{
								USBLog(3, "AppleUSBHub[%p]::WaitForPortResumes - port[%p], which is number(%d) has statusFlags[%p] and changeFlags[%p]", this, port, portNum, (void*)portStatus.statusFlags, (void*)portStatus.changeFlags);
								needToWait = true;
							}
					}
				}
			}
			// this is still in the inner loop - wait 1 ms between retries
			if (needToWait)
				IOSleep(1);
			
		}
		if (needToWait)
		{
			needToWait = false;
			// this is out of the inner loop. if needToWait is till set, then it appears that some port is not getting cleared for some reason
			for (portIndex = 0; portIndex < _hubDescriptor.numPorts; portIndex++)
			{
				portNum = portIndex+1;
				port = _ports[portIndex];
				if (port)
				{
					if (port->_portPMState == usbHPPMS_pm_suspended)
					{
						USBLog(3, "AppleUSBHub[%p]::WaitForPortResumes - port[%p] which is port number (%d) still waiting", this, port, portNum);
						portStatus.statusFlags = 0;
						portStatus.changeFlags = 0;
						kr = GetPortStatus(&portStatus, portNum);
						if (kr)
						{
							USBLog(1, "AppleUSBHub[%p]::WaitForPortResumes - err (%p) from GetPortStatus for port (%d) - setting port to active", this, (void*)kr, portNum);
							USBTrace( kUSBTHub, kTPHubWaitForPortResumes, (uintptr_t)this, kr, portNum, 2 );
							dealWithPort[portIndex] = true;
							port->_portPMState = usbHPPMS_active;
							port->_resumePending = false;
						}
						else
						{
							if (portStatus.changeFlags & kHubPortSuspend)
							{
								USBLog(3, "AppleUSBHub[%p]::WaitForPortResumes - port[%p] which is port number (%d) has the suspend change set in the OUTER loop", this, port, portNum);
								needToWait = true;					// to send us back up
							}
							else if (portStatus.statusFlags & kHubPortSuspend)
							{
								// reissue the command which was issued earlier and apparently didn't stick
								kr = port->SuspendPort(false, false);					
								if (kr)
								{
									USBLog(1, "AppleUSBHub[%p]::WaitForPortResumes - err (%p) from SuspendPort for port (%d)", this, (void*)kr, portNum);
									USBTrace( kUSBTHub, kTPHubWaitForPortResumes, (uintptr_t)this, kr, portNum, 3 );
									dealWithPort[portIndex] = true;
									port->_portPMState = usbHPPMS_active;		// give up on this port i guess
									port->_resumePending = false;
								}
								else
								{
									needToWait = true;					// to send us back up
								}
							}
							else if (port->_resumePending)
							{
								USBLog(3, "AppleUSBHub[%p]::WaitForPortResumes - port number %d still has a pending resume in OUTER loop - going around again", this, portNum);
								needToWait = true;
							}
							else
							{
								USBLog(1, "AppleUSBHub[%p]::WaitForPortResumes - not sure what is going on with port number %d in OUTER loop - terminating", this, portNum);
								USBTrace( kUSBTHub, kTPHubWaitForPortResumes, (uintptr_t)this, kr, portNum, 4 );
							}
						}
					}
				}
			}
		}
	}
	
	// now deal with any ports which were marked
	for (portIndex = 0; portIndex < _hubDescriptor.numPorts; portIndex++)
	{
		portNum = portIndex + 1;
		port = _ports[portIndex];
		if (port && dealWithPort[portIndex])
		{
			IOUSBControllerV3		*v3Bus = NULL;
			IOReturn				err;
			
			if (recoveryNeeded)
			{
				// according to the USB 2.0 spec, when resuming, we need to allow 10 ms recover time before we talk to devices
				// on the bus. so don't enable the endpoints until that recovery is over. this will apply to all ports which
				// we have recently enabled
				recoveryNeeded = false;
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
                {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHub[%p]::WaitForPortResumes - IOSleep in gate:%d", this, 1);}}
#endif
				IOSleep(port->_portResumeRecoveryTime);
			}
			if (_device)
				v3Bus = OSDynamicCast(IOUSBControllerV3, _bus);
			
			if (v3Bus && port->_portDevice)
			{
				USBLog(5, "AppleUSBHub[%p]::WaitForPortResumes - Enabling endpoints for device at address (%d)", this, (int)port->_portDevice->GetAddress());
				err = v3Bus->EnableAddressEndpoints(port->_portDevice->GetAddress(), true);
				if (err)
				{
					USBLog(2, "AppleUSBHub[%p]::WaitForPortResumes - EnableAddressEndpoints returned (%p)", this, (void*)err);
				}
			}
			// these are to counteract the Increments in StartPorts
			// note that this needs to be done AFTER we re-enable the Address endpoints
			USBLog(6, "AppleUSBHub[%p]::WaitForPortResumes - calling DecrementOutstandingIO", this);
			DecrementOutstandingResumes();
			DecrementOutstandingIO();
		}
	}
}



void 
AppleUSBHub::ResetMyPort()
{
	IOReturn				err;
    int						currentPort;
    AppleUSBHubPort *		port;
	
	USBLog(1, "AppleUSBHub[%p]::ResetMyPort - marking hub @ 0x%x as dead", this, (uint32_t)_locationID);	
	_hubIsDead = TRUE;
	
    if ( _interruptPipe )
    {
		err = _interruptPipe->Abort();
        if ( err != kIOReturnSuccess )
        {
            USBLog(1, "AppleUSBHub[%p]::ResetMyPort interruptPipe->Abort returned %p", this, (void*)err);
			USBTrace( kUSBTHub, kTPHubResetMyPort, (uintptr_t)this, err, 0, 1 );
        }
    }
	
    if ( _ports)
    {
        for (currentPort = 1; currentPort <= _hubDescriptor.numPorts; currentPort++)
        {
            port = _ports ? _ports[currentPort-1] : NULL;
            if (port)
            {
                if (port->_devZero)
                {
                    USBLog(1, "AppleUSBHub[%p]::StopPorts - port %d had the dev zero lock", this, currentPort);
					USBTrace( kUSBTHub, kTPHubResetMyPort, (uintptr_t)this, currentPort, _hubDescriptor.numPorts, 2 );
                }
                port->ReleaseDevZeroLock();
            }
        }
    }
	
    // If our timerSource is going, cancel it, as we don't need to
    // timeout our ports anymore
    //
    if (_timerSource) 
    {
        _timerSource->cancelTimeout();
    }

 	// We need to call resetDevice once any power changes are done, so we set up a flag here and check it in powerChangeDone and
	// do the reset there.
	USBLog(3, "AppleUSBHub[%p]::ResetMyPort  calling changePowerStateToPriv(kIOUSBHubPowerStateOff) %d", this, kIOUSBHubPowerStateOff);
	_needToCallResetDevice = true;
	changePowerStateToPriv(kIOUSBHubPowerStateOn);
	powerOverrideOnPriv();
	changePowerStateToPriv(kIOUSBHubPowerStateOff);

}



bool
AppleUSBHub::IsHSRootHub()
{
#ifdef SUPPORTS_SS_USB
    if ( (_hsHub || _ssHub) && _isRootHub)
#else
    if (_hsHub && _isRootHub)
#endif
		return true;
    else
		return false;
}


IOReturn
AppleUSBHub::GetPortInformation(UInt32 portNum, UInt32 *info)
{
    AppleUSBHubPort	*	port;
	UInt32				portIndex = portNum -1;
	IOUSBHubPortStatus	portStatus;
	UInt32				information = 0;
	IOReturn			kr = kIOReturnSuccess;
	bool				shouldApplyWorkaround = false;
	
	USBLog(5, "AppleUSBHub[%p]::GetPortInformation  for port %d of hub @ 0x%x", this, (uint32_t)portNum, (uint32_t)_locationID);
	
	// If we are the root hub, set the bit
	if ( _isRootHub )
		information |= ( 1 << kUSBInformationDeviceIsAttachedToRootHubBit);
	
	// Check to see if we are captive
    if ( _ports)
    {
		port = _ports ? _ports[portIndex] : NULL;
		if (port)
		{
			if ( port->IsCaptive() )
				information |= (1 << kUSBInformationDeviceIsCaptiveBit);
			
			if ( port->HasExternalConnector() )
				information |= (1 << kUSBInformationDeviceIsAttachedToEnclosure);
			
			shouldApplyWorkaround = port->ShouldApplyDisconnectWorkaround();
		}
		
		// Do a GetPortStatus to get the rest of the information
		portStatus.statusFlags = 0;
		portStatus.changeFlags = 0;
		kr = GetPortStatus(&portStatus, portNum);
		if (kr != kIOReturnSuccess)
		{
			USBLog(3, "AppleUSBHub[%p]::GetPortInformation -  error 0x%x getting port status for port %d", this, kr, (uint32_t)portNum);
		}
		else
		{
			USBLog(6, "AppleUSBHub[%p]::GetPortInformation - got statusFlags(%p) changeFlags(%p), _ignoreDisconnectOnWakeup = %d", this, (void*)portStatus.statusFlags, (void*)portStatus.changeFlags, _ignoreDisconnectOnWakeup);
			
			if ( (portStatus.statusFlags & kHubPortConnection) && !(portStatus.changeFlags & kHubPortConnection) )
				information |= ( 1 << kUSBInformationDeviceIsConnectedBit);
			
			if (portStatus.statusFlags & kHubPortEnabled) 
				information |= ( 1 << kUSBInformationDeviceIsEnabledBit);
			
			if (portStatus.statusFlags & kHubPortSuspend) 
				information |= ( 1 << kUSBInformationDeviceIsSuspendedBit);
			
			if (portStatus.statusFlags & kHubPortOverCurrent) 
				information |= ( 1 << kUSBInformationDeviceOvercurrentBit);
			
			if (portStatus.statusFlags & kHubPortBeingReset) 
				information |= ( 1 << kUSBInformationDeviceIsInResetBit);
			
			if (portStatus.statusFlags & kHubPortTestMode) 
				information |= ( 1 << kUSBInformationDevicePortIsInTestModeBit);
			
			// If we  have a defective hub/device combo, lie:
			if ( _ignoreDisconnectOnWakeup && shouldApplyWorkaround )
			{
#ifdef SUPPORTS_SS_USB
				if ( _ssHub )
				{
					// This is a workaround.  Sometimes on a SS hub, right after wake, we see the link
					// training again.  The MSC driver will see if there is someting connected and if there 
					// is not, it will just give up and assume it will get a termination soon.  By always
					// returning a connection we force it to try harder before giving up.
					information |= ( kUSBInformationDeviceIsConnectedMask | kUSBInformationDeviceIsEnabledMask);
				}
				else
#endif
				{
					if ( portStatus.statusFlags & kHubPortConnection )
					{
						information |= ( kUSBInformationDeviceIsConnectedMask | kUSBInformationDeviceIsEnabledMask);
					}
				}
			}
		}
    }
	
	*info = information;
	return kr;
}

IOReturn
AppleUSBHub::ResetPort(UInt32 portNum)
{
    IOReturn    ret;
    
	USBLog(5, "AppleUSBHub[%p]::ResetPort  for port %d of hub @ 0x%x", this, (uint32_t)portNum, (uint32_t)_locationID);

    // make sure that we don't close while we are doing this
    retain();
    
	EnsureUsability();
	
	ret =  DoPortAction( kIOUSBMessageHubResetPort , portNum, 0 );
    
    release();
    
    return ret;
}

IOReturn
AppleUSBHub::SuspendPort(UInt32 portNum, bool suspend )
{
    IOReturn    ret;
    
	USBLog(5, "AppleUSBHub[%p]::SuspendPort  %s for port %d of hub @ 0x%x", this, suspend ? "SUSPEND" : "RESUME", (uint32_t)portNum, (uint32_t)_locationID);

    // make sure that we don't close while we are doing this
    retain();
    
	EnsureUsability();
	
	ret = DoPortAction( suspend ? kIOUSBMessageHubSuspendPort : kIOUSBMessageHubResumePort , portNum, 0 );
    
    release();
    
    return ret;
}

IOReturn
AppleUSBHub::ReEnumeratePort(UInt32 portNum, UInt32 options)
{
    IOReturn    ret;
    
	USBLog(5, "AppleUSBHub[%p]::ReEnumeratePort  for port %d of hub @ 0x%x, options %d", this, (uint32_t)portNum, (uint32_t)_locationID, (uint32_t) options);
	
    // make sure that we don't close while we are doing this
    retain();
    
	EnsureUsability();
	
	ret = DoPortAction( kIOUSBMessageHubReEnumeratePort , portNum, options );
    
    release();
    
    return ret;
}

#pragma mark ееееееее Bookkeeping ееееееее
void
AppleUSBHub::DecrementOutstandingIO(bool fromPowerChange)
{
	UInt32			outstandingIO;
	IOReturn		err;
	static			int		gSerial = 0;
	int				localSerial;
	
	localSerial = gSerial++;
    if (!_gate)
    {
		USBLog(6, "AppleUSBHub[%p]::DecrementOutstandingIO(%d) isInactive(%d), outstandingIO(%d), _needInterruptRead(%d) - no gate", this, localSerial, isInactive(), (uint32_t)_outstandingIO, _needInterruptRead);
		outstandingIO = --_outstandingIO;
        if (fromPowerChange)
            _outstandingIOFromPowerChange = false;
    }
	else
	{
		err = _gate->runAction(ChangeOutstandingIO, (void*)-1, &outstandingIO, (void*)fromPowerChange);
		USBLog(6, "AppleUSBHub[%p]::DecrementOutstandingIO(%d) isInactive(%s), _interruptReadPending(%s), gated call returned err (%p) count (%d), _needInterruptRead(%d)", this, localSerial, isInactive() ? "true" : "false", _interruptReadPending ? "true" : "false", (void*)err, (int)outstandingIO, _needInterruptRead);
	}
	USBTrace( kUSBTOutstandingIO, kTPHubDecrement , (uintptr_t)this, localSerial, outstandingIO, _needInterruptRead);

	if ( IsPortInitThreadActiveForAnyPort() )
	{
		// We want to rearm the read if any port has the devZero lock, as they are probably waiting for a port status
		if ((_myPowerState == kIOUSBHubPowerStateOn) && _needInterruptRead)
		{
			// even though i may lower my power state below, it may not actually get lowered until our children are all at a lower state
			// so go ahead and schedule another interrupt read
			USBLog(5, "AppleUSBHub[%p]::DecrementOutstandingIO(%d), rearming read because devZero lock was held by a port", this, localSerial);
			_needInterruptRead = false;
			RearmInterruptRead();
		}
	}
	else if (_needToClose)
	{
		if (!outstandingIO && !_doPortActionLock && ((_myPowerState == kIOUSBHubPowerStateOn) || ((_myPowerState == kIOUSBHubPowerStateOff) && _okToCloseWhileOff)))
		{
			_needToClose = false;						// so that we don't do this twice..
			USBLog(3, "AppleUSBHub[%p]::DecrementOutstandingIO(%d) isInactive(%s) outstandingIO(%d) _powerStateChangingTo(%d) - closing device", this, localSerial, isInactive() ? "true" : "false", (int)outstandingIO, (int)_powerStateChangingTo);
			// Stop/close all ports, deallocate our ports
			//
			StopPorts();
			_device->close(this);
		}
		else
		{
			USBLog(3, "AppleUSBHub[%p]::DecrementOutstandingIO(%d) _needToClose(true) but waiting for outstandingIO OR _myPowerState == kIOUSBHubPowerStateOn(%d) OR for the _doPortActionLock", this, localSerial, (int)_myPowerState);
		}
	}
	else if ((outstandingIO == 0) || ((outstandingIO == 1) && _interruptReadPending))
	{
		if ( (_myPowerState == kIOUSBHubPowerStateOn) && !_hubIsDead && !isInactive())
		{
			// even though i may lower my power state below, it may not actually get lowered until our children are all at a lower state
			// so go ahead and schedule another interrupt read
			if (_needInterruptRead)
			{
				_needInterruptRead = false;
				USBLog(3, "AppleUSBHub[%p]::DecrementOutstandingIO(%d), outstandingIO(%d), _interruptReadPending(%s) - rearming read", this, localSerial, (uint32_t)outstandingIO, _interruptReadPending ? "true" : "false");
				RearmInterruptRead();
			}
			if (!_checkPortsThreadActive && (_powerStateChangingTo == kIOUSBHubPowerStateStable))
			{
				_abandonCheckPorts = false;
				_checkPortsThreadActive = true;
				retain();
				USBLog(5,"AppleUSBHub[%p]::DecrementOutstandingIO(%d) - spawning _checkForActivePortsThread", this, localSerial);
				if ( thread_call_enter(_checkForActivePortsThread) == TRUE )
				{
					USBLog(1,"AppleUSBHub[%p]::DecrementOutstandingIO -  _checkForActivePortsThread already queued", this);
					USBTrace( kUSBTHub, kTPHubDecrementOutstandingIO, (uintptr_t)this, 0, 0, 0 );
					release();
				}
			}
		}
	}
	USBLog(6, "AppleUSBHub[%p]::DecrementOutstandingIO(%d)", this, (uint32_t)outstandingIO);
}



void
AppleUSBHub::IncrementOutstandingIO(bool fromPowerChange)
{
    if (!_gate)
    {
		_outstandingIO++;
        if (fromPowerChange)
            _outstandingIOFromPowerChange = true;
        
		USBLog(6, "AppleUSBHub[%p]::IncrementOutstandingIO isInactive = %d, outstandingIO = %d - no gate", this, isInactive(), (uint32_t)_outstandingIO);
		USBTrace( kUSBTOutstandingIO, kTPHubIncrement , (uintptr_t)this, _outstandingIO, 0, 0);
		return;
    }
    _gate->runAction(ChangeOutstandingIO, (void*)1, NULL, (void*)fromPowerChange);
	USBTrace( kUSBTOutstandingIO, kTPHubIncrement , (uintptr_t)this, _outstandingIO, 0, 0);
}



IOReturn
AppleUSBHub::ChangeOutstandingIO(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
#pragma unused (param3, param4)
    AppleUSBHub		*me = OSDynamicCast(AppleUSBHub, target);
    UInt32			direction = (uintptr_t)param1;
	UInt32			*retCount = (UInt32*)param2;
    bool            fromPowerChange = (bool)param3;
    
	IOReturn		ret = kIOReturnSuccess;
    
    if (!me)
    {
		USBLog(1, "AppleUSBHub::ChangeOutstandingIO - invalid target");
		USBTrace( kUSBTHub, kTPHubChangeOutstandingIO, (uintptr_t)me, direction, *retCount, kIOReturnBadArgument );
		return kIOReturnBadArgument;
    }
    switch (direction)
    {
		case 1:
			me->_outstandingIO++;
            if (fromPowerChange)
                me->_outstandingIOFromPowerChange = true;
			USBLog(6, "AppleUSBHub[%p]::ChangeOutstandingIO(+) now (%d) fromPowerChange(%s)", me, (int)me->_outstandingIO, me->_outstandingIOFromPowerChange ? "true" : "false");
			break;
			
		case -1:
			--me->_outstandingIO;
            if (fromPowerChange)
                me->_outstandingIOFromPowerChange = false;
			USBLog(6, "AppleUSBHub[%p]::ChangeOutstandingIO(-) now (%d) fromPowerChange(%s)", me, (int)me->_outstandingIO, me->_outstandingIOFromPowerChange ? "true" : "false");
			break;
			
		default:
			USBLog(1, "AppleUSBHub[%p]::ChangeOutstandingIO - invalid direction", me);
			USBTrace( kUSBTHub, kTPHubChangeOutstandingIO, (uintptr_t)me, direction, kIOReturnBadArgument, 0 );
			ret = kIOReturnBadArgument;
    }
	
	if (retCount)
		*retCount = me->_outstandingIO;
		
    return ret;
}



void
AppleUSBHub::LowerPowerState(void)
{
	UInt32			raisedPowerStateCount;
	IOReturn		err;
	static			int		gSerial = 0;
	int				localSerial;
	
	localSerial = gSerial++;
    if (!_gate)
    {
		USBLog(6, "AppleUSBHub[%p]::LowerPowerState(%d) isInactive = %d, _raisedPowerStateCount = %d - no gate", this, localSerial, isInactive(), (uint32_t)_raisedPowerStateCount);
		raisedPowerStateCount = --_raisedPowerStateCount;
    }
	else
	{
		err = _gate->runAction(ChangeRaisedPowerState, (void*)-1, &raisedPowerStateCount);
		USBLog(6, "AppleUSBHub[%p]::LowerPowerState(%d) isInactive(%s), gated call returned err (%p) count (%d)", this, localSerial, isInactive() ? "true" : "false", (void*)err, (uint32_t)raisedPowerStateCount);
	}

	if (!raisedPowerStateCount)
	{
		changePowerStateTo(kIOUSBHubPowerStateLowPower);
	}	
	USBLog(6, "AppleUSBHub[%p]::LowerPowerState(%d)", this, localSerial);
}



void
AppleUSBHub::RaisePowerState(void)
{
	IOReturn		err;
	UInt32			raisedPowerStateCount;

    if (!_gate)
    {
		USBLog(6, "AppleUSBHub[%p]::RaisePowerState isInactive = %d, outstandingIO = %d - no gate", this, isInactive(), (uint32_t)_outstandingIO);
		raisedPowerStateCount = _raisedPowerStateCount++;
    }
	else
	{
		err = _gate->runAction(ChangeRaisedPowerState, (void*)1, &raisedPowerStateCount);
		USBLog(6, "AppleUSBHub[%p]::RaisePowerState isInactive(%s), gated call returned err (%p) count (%d)", this, isInactive() ? "true" : "false", (void*)err, (uint32_t)raisedPowerStateCount);
	}
	if (raisedPowerStateCount == 1)				// only need to do this once
	{
		changePowerStateTo(kIOUSBHubPowerStateOn);
	}
}



IOReturn
AppleUSBHub::ChangeRaisedPowerState(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
#pragma unused (param3, param4)
    AppleUSBHub		*me = OSDynamicCast(AppleUSBHub, target);
    UInt32			direction = (uintptr_t)param1;
	UInt32			*retCount = (UInt32*)param2;
	IOReturn		ret = kIOReturnSuccess;
    
    if (!me)
    {
		USBLog(1, "AppleUSBHub::ChangeRaisedPowerState - invalid target");
		USBTrace( kUSBTHub, kTPHubChangeRaisedPowerState, (uintptr_t)me, direction, *retCount, kIOReturnBadArgument );
		return kIOReturnBadArgument;
    }
    switch (direction)
    {
		case 1:
			me->_raisedPowerStateCount++;
			USBLog(3, "AppleUSBHub[%p]::ChangeRaisedPowerState(+) now (%d)", me, (int)me->_raisedPowerStateCount);
			break;
			
		case -1:
			--me->_raisedPowerStateCount;
			USBLog(3, "AppleUSBHub[%p]::ChangeRaisedPowerState(-) now (%d)", me, (int)me->_raisedPowerStateCount);
			break;
			
		default:
			USBLog(1, "AppleUSBHub[%p]::ChangeRaisedPowerState - invalid direction", me);
			USBTrace( kUSBTHub, kTPHubChangeRaisedPowerState, (uintptr_t)me, direction, kIOReturnBadArgument, 0 );
			ret = kIOReturnBadArgument;
    }
	
	if (retCount)
		*retCount = me->_raisedPowerStateCount;
		
    return ret;
}



void
AppleUSBHub::DecrementOutstandingResumes(void)
{
	UInt32			outstandingResumes;
	IOReturn		err;
	static			int		gSerial = 0;
	int				localSerial;
	
	localSerial = gSerial++;
    if (!_gate)
    {
		USBLog(5, "AppleUSBHub[%p]::+DecrementOutstandingResumes(%d) isInactive = %d, outstandingIO = %d, _needInterruptRead: %d - no gate", this, localSerial, isInactive(), (uint32_t)_outstandingIO, _needInterruptRead);
		outstandingResumes = --_outstandingResumes;
    }
	else
	{
		err = _gate->runAction(ChangeOutstandingResumes, (void*)-1, &outstandingResumes);
		USBLog(5, "AppleUSBHub[%p]::DecrementOutstandingResumes(%d) isInactive(%s), gated call returned err (%p) count (%d)", this, localSerial, isInactive() ? "true" : "false", (void*)err, (int)outstandingResumes);
	}
	
	if (!outstandingResumes)
	{
		USBLog(5, "AppleUSBHub[%p]::DecrementOutstandingResumes(%d) - resumes down to zero", this, localSerial);
		if (_needToAckSetPowerState)
		{
			USBLog(5, "AppleUSBHub[%p]::DecrementOutstandingResumes(%d) - calling acknowledgeSetPowerState", this, localSerial);
			_needToAckSetPowerState = false;
			acknowledgeSetPowerState();
		}
	}
	
	USBLog(5, "AppleUSBHub[%p]::-DecrementOutstandingResumes(%d)", this, localSerial);
}



void
AppleUSBHub::IncrementOutstandingResumes(void)
{
    if (!_gate)
    {
		USBLog(5, "AppleUSBHub[%p]::IncrementOutstandingResumes isInactive = %d, outstandingIO = %d - no gate", this, isInactive(), (uint32_t)_outstandingResumes);
		_outstandingResumes++;
		return;
    }
    _gate->runAction(ChangeOutstandingResumes, (void*)1);
}



IOReturn
AppleUSBHub::ChangeOutstandingResumes(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
#pragma unused (param3, param4)
    AppleUSBHub		*me = OSDynamicCast(AppleUSBHub, target);
    UInt32			direction = (uintptr_t)param1;
	UInt32			*retCount = (UInt32*)param2;
	IOReturn		ret = kIOReturnSuccess;
    
    if (!me)
    {
		USBLog(1, "AppleUSBHub::ChangeOutstandingResumes - invalid target");
		USBTrace( kUSBTHub, kTPHubChangeOutstandingResumes, (uintptr_t)me, direction, *retCount, kIOReturnBadArgument );
		return kIOReturnBadArgument;
    }
    switch (direction)
    {
		case 1:
			me->_outstandingResumes++;
			USBLog(3, "AppleUSBHub[%p]::ChangeOutstandingResumes(+) now (%d)", me, (int)me->_outstandingResumes);
			break;
			
		case -1:
			--me->_outstandingResumes;
			USBLog(3, "AppleUSBHub[%p]::ChangeOutstandingResumes(-) now (%d)", me, (int)me->_outstandingResumes);
			break;
			
		default:
			USBLog(1, "AppleUSBHub[%p]::ChangeOutstandingResumes - invalid direction", me);
			USBTrace( kUSBTHub, kTPHubChangeOutstandingResumes, (uintptr_t)me, direction, kIOReturnBadArgument, 0 );
			ret = kIOReturnBadArgument;
    }
	
	if (retCount)
		*retCount = me->_outstandingResumes;
	
    return ret;
}



IOReturn			
AppleUSBHub::TakeDoPortActionLock(void)
{
	if (!_workLoop || !_gate)
	{
		USBLog(1, "AppleUSBHub[%p]::TakeDoPortActionLock - no WorkLoop or no gate!", this);
		return kIOReturnNotPermitted;
	}
	if (_workLoop->onThread())
	{
		USBLog(1, "AppleUSBHub[%p]::TakeDoPortActionLock - called onThread -- not allowed!", this);
		return kIOReturnNotPermitted;
	}
	USBLog(6, "AppleUSBHub[%p]::TakeDoPortActionLock - calling through to ChangeDoPortActionLock", this);
	return _gate->runAction(ChangeDoPortActionLock, (void*)true);
}



IOReturn			
AppleUSBHub::ReleaseDoPortActionLock(void)
{
	if (!_workLoop || !_gate)
	{
		USBLog(1, "AppleUSBHub[%p]::TakeDoPortActionLock - no WorkLoop or no gate!", this);
		return kIOReturnNotPermitted;
	}
	USBLog(6, "AppleUSBHub[%p]::ReleaseDoPortActionLock - calling through to ChangeDoPortActionLock", this);
	return _gate->runAction(ChangeDoPortActionLock, (void*)false);
}


#define DO_PORT_ACTION_DEADLINE_IN_SECONDS	30
IOReturn		
AppleUSBHub::ChangeDoPortActionLock(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
#pragma unused (param2, param3, param4)
    AppleUSBHub		*me = OSDynamicCast(AppleUSBHub, target);
    bool			takeLock = (bool)param1;
	IOReturn		retVal = kIOReturnSuccess;
	
	if (takeLock)
	{
		if (!me->_doPortActionLock)
		{
			USBLog(5, "AppleUSBHub[%p]::ChangeDoPortActionLock - _doPortActionLock available for use - no commandSleep needed", me);
			USBTrace( kUSBTHub, kTPHubDoPortActionLock, (uintptr_t)me, 0, 0, 0 );
		}
		
		while (me->_doPortActionLock and (retVal == kIOReturnSuccess))
		{
			AbsoluteTime	deadline;
			IOReturn		kr;

			clock_interval_to_deadline(DO_PORT_ACTION_DEADLINE_IN_SECONDS, kSecondScale, &deadline);
			USBLog(5, "AppleUSBHub[%p]::ChangeDoPortActionLock - _doPortActionLock held by someone else - calling commandSleep to wait for lock", me);
			USBTrace( kUSBTHub, kTPHubDoPortActionLock, (uintptr_t)me, 0, 0, 1 );
			kr = me->_gate->commandSleep(&me->_doPortActionLock, deadline, THREAD_ABORTSAFE);
			switch (kr)
			{
				case THREAD_AWAKENED:
					USBLog(6,"AppleUSBHub[%p]::ChangeDoPortActionLock commandSleep woke up normally (THREAD_AWAKENED) _doPortActionLock(%s)", me, me->_doPortActionLock ? "true" : "false");
					USBTrace( kUSBTHub, kTPHubDoPortActionLock, (uintptr_t)me, (uintptr_t)me->_doPortActionLock, 0, 2 );
					break;

				case THREAD_TIMED_OUT:
					USBLog(3,"AppleUSBHub[%p]::ChangeDoPortActionLock commandSleep timeout out (THREAD_TIMED_OUT) _doPortActionLock(%s)", me, me->_doPortActionLock ? "true" : "false");
					USBTrace( kUSBTHub, kTPHubDoPortActionLock, (uintptr_t)me, (uintptr_t)me->_doPortActionLock, 0, 7 );
					retVal = kIOReturnNotPermitted;
					break;

				case THREAD_INTERRUPTED:
					USBLog(3,"AppleUSBHub[%p]::ChangeDoPortActionLock commandSleep interrupted (THREAD_INTERRUPTED) _doPortActionLock(%s)", me, me->_doPortActionLock ? "true" : "false");
					USBTrace( kUSBTHub, kTPHubDoPortActionLock, (uintptr_t)me, (uintptr_t)me->_doPortActionLock, 0, 3 );
					retVal = kIOReturnNotPermitted;
					break;

				case THREAD_RESTART:
					USBLog(3,"AppleUSBHub[%p]::ChangeDoPortActionLock commandSleep restarted (THREAD_RESTART) _doPortActionLock(%s)", me, me->_doPortActionLock ? "true" : "false");
					USBTrace( kUSBTHub, kTPHubDoPortActionLock, (uintptr_t)me, (uintptr_t)me->_doPortActionLock, 0, 4 );
					retVal = kIOReturnNotPermitted;
					break;

				case kIOReturnNotPermitted:
					USBLog(3,"AppleUSBHub[%p]::ChangeDoPortActionLock woke up with status (kIOReturnNotPermitted) - we do not hold the WL!", me);
					USBTrace( kUSBTHub, kTPHubDoPortActionLock, (uintptr_t)me, (uintptr_t)me->_doPortActionLock, 0, 5 );
					retVal = kr;
					break;
					
				default:
					USBLog(3,"AppleUSBHub[%p]::ChangeDoPortActionLock woke up with unknown status %p",  me, (void*)kr);
					USBTrace( kUSBTHub, kTPHubDoPortActionLock, (uintptr_t)me, (uintptr_t)me->_doPortActionLock, 0, 6 );
					retVal = kIOReturnNotPermitted;
			}
		}
		if (retVal == kIOReturnSuccess)
		{
			USBLog(6, "AppleUSBHub[%p]::ChangeDoPortActionLock - setting _doPortActionLock to true", me);
			me->_doPortActionLock = true;
		}
	}
	else
	{
		USBLog(5, "AppleUSBHub[%p]::ChangeDoPortActionLock - setting _doPortActionLock to false and calling commandWakeup", me);
		USBTrace( kUSBTHub, kTPHubDoPortActionLock, (uintptr_t)me, 0, 0, 8 );
		me->_doPortActionLock = false;
		me->_gate->commandWakeup(&me->_doPortActionLock, true);
	}
	return retVal;
}



#pragma mark Utility Functions

void
AppleUSBHub::EnsureUsabilityEntry(OSObject *target)
{
    AppleUSBHub *	me = OSDynamicCast(AppleUSBHub, target);
	
    if (!me)
        return;
	
	USBLog(7, "AppleUSBHub[%p]::EnsureUsabilityEntry - calling in", me);
	me->EnsureUsability();
	me->DecrementOutstandingIO();
	USBLog(7, "AppleUSBHub[%p]::EnsureUsabilityEntry - done", me);
	me->release();
}



//================================================================================================
//   EnsureUsability
//		Override the IOUSBHubPolicymaker implementation in order to make sure that we 
//		are not in the CheckForActivePorts thread when we get to the superclass method
//================================================================================================

IOReturn		
AppleUSBHub::EnsureUsability(void)
{
	UInt32			retries = 200;
	
	if (isInactive())
	{
		USBLog(4, "AppleUSBHub[%p]::EnsureUsability - called while inActive - ignoring", this);
		return kIOReturnSuccess;
	}
	
	if (_workLoop->inGate())
	{
		if (_ensureUsabilityThread)
		{
			USBLog(7, "AppleUSBHub[%p]::EnsureUsability - called inGate - spawning thread instead, _inStartMethod (%s)", this, _inStartMethod ? "true" : "false");
			IncrementOutstandingIO();
			retain();
			if (thread_call_enter(_ensureUsabilityThread) == true)
			{
				// we must already be queued so just decrement
				DecrementOutstandingIO();
				release();
			}
		}
		else
		{
			USBLog(2, "AppleUSBHub[%p]::EnsureUsability - called inGate - no thread call available, ignoring, _inStartMethod (%s)", this, _inStartMethod ? "true" : "false");
		}
		return kIOReturnSuccess;
	}
	
	USBLog(7, "AppleUSBHub[%p]::EnsureUsability - setting _abandonCheckPorts, inGate(%s)", this, _workLoop->inGate() ? "true" : "false");

	_abandonCheckPorts = true;
	// wait up to 400 ms for us to complete the CheckForActivePorts
	while (_checkPortsThreadActive && retries--)
	{
		// make sure that the CheckForActivePorts thread is not running. we have already told it to abandon ship
		// but it may or may not have noticed that before lowering the power state. at any rate, once it is no longer
		// active, then we can raise the power state and all should be well
		USBLog(5, "AppleUSBHub[%p]::EnsureUsability - _checkPortsThreadActive, sleeping 2 ms (retries %d)", this, (int)retries);
		IOSleep(2);
	}
	if (_checkPortsThreadActive)
	{
		USBLog(1, "AppleUSBHub[%p]::EnsureUsability - _checkPortsThreadActive after delay!", this);
		USBTrace( kUSBTHub, kTPHubEnsureUsability, (uintptr_t)this, 0, 0, 0 );
	}
	USBLog(7, "AppleUSBHub[%p]::EnsureUsability - clearing _abandonCheckPorts, inGate(%s)", this, _workLoop->inGate() ? "true" : "false");
	_abandonCheckPorts = false;
	return super::EnsureUsability();
}



// timeout is in ms
IOReturn				
AppleUSBHub::WaitForPowerOn( uint64_t timeout )
{
	if ((_myPowerState == kIOUSBHubPowerStateOn) and (_powerStateChangingTo != kIOUSBHubPowerStateLowPower))
	{
		USBLog(6,"AppleUSBHub[%p]::WaitForPowerOn was successful, _myPowerState[%d], returning kIOReturnSuccess", this, (int)_myPowerState);
		return kIOReturnSuccess;
	}

	if ( not _workLoop or not _gate )
	{
		USBLog(1, "AppleUSBHub[%p]::WaitForPowerOn - nil workloop or nil gate !", this);
		USBTrace( kUSBTHub, kTPHubWaitForPowerOn, (uintptr_t)this, 0, 0, 1 );
		return kIOReturnInternalError;
	}

	if ( _workLoop->onThread() )
	{
		USBLog(1, "AppleUSBHub[%p]::WaitForPowerOn - called on workloop thread (this should be OK)", this);
		USBTrace( kUSBTHub, kTPHubWaitForPowerOn, (uintptr_t)this, 0, 0, 2 );
	}
	
	if ( _workLoop->inGate() )
	{	
		USBLog(5,"AppleUSBHub[%p]::WaitForPowerOn called inGate, _waitingForPowerOn(%s)", this, _waitingForPowerOn ? "true" : "false");
		
		if (_waitingForPowerOn)
		{
			USBLog(1,"AppleUSBHub[%p]::WaitForPowerOn called inGate, but we are already waiting, returning kIOReturnInternalErr", this);
			USBTrace( kUSBTHub, kTPHubWaitForPowerOn, (uintptr_t)this, 0, 0, 3 );
			return kIOReturnInternalError;
		}
		
		AbsoluteTime	deadline;
		IOReturn		kr;
		
		clock_interval_to_deadline(timeout, kMillisecondScale, &deadline);
		_waitingForPowerOn = true;
		kr = _gate->commandSleep(&_waitingForPowerOn, deadline, THREAD_ABORTSAFE);
		_waitingForPowerOn = false;
		switch (kr)
		{
			case THREAD_AWAKENED:
				USBLog(6,"AppleUSBHub[%p]::WaitForPowerOn commandSleep woke up normally (THREAD_AWAKENED) _myPowerState(%d)", this, (int)_myPowerState);
				USBTrace( kUSBTHub, kTPHubWaitForPowerOn, (uintptr_t)this, 0, (uintptr_t)_myPowerState, 12 );
				break;
				
			case THREAD_TIMED_OUT:
				USBLog(3,"AppleUSBHub[%p]::WaitForPowerOn commandSleep timed out (THREAD_TIMED_OUT) _myPowerState(%d)", this, (int)_myPowerState);
				USBTrace( kUSBTHub, kTPHubWaitForPowerOn, (uintptr_t)this, 0, (uintptr_t)_myPowerState, 4 );
				break;
				
			case THREAD_INTERRUPTED:
				USBLog(3,"AppleUSBHub[%p]::WaitForPowerOn commandSleep was interrupted (THREAD_INTERRUPTED) _myPowerState(%d)", this, (int)_myPowerState);
				USBTrace( kUSBTHub, kTPHubWaitForPowerOn, (uintptr_t)this, 0, (uintptr_t)_myPowerState, 5 );
				break;
				
			case THREAD_RESTART:
				USBLog(3,"AppleUSBHub[%p]::WaitForPowerOn commandSleep was restarted (THREAD_RESTART) _myPowerState(%d)", this, (int)_myPowerState);
				USBTrace( kUSBTHub, kTPHubWaitForPowerOn, (uintptr_t)this, 0, (uintptr_t)_myPowerState, 6 );
				break;
				
			case kIOReturnNotPermitted:
				USBLog(3,"AppleUSBHub[%p]::WaitForPowerOn commandSleep woke up (kIOReturnNotPermitted) _myPowerState(%d)", this, (int)_myPowerState);
				USBTrace( kUSBTHub, kTPHubWaitForPowerOn, (uintptr_t)this, 0, (uintptr_t)_myPowerState, 7 );
			break;
				
			default:
				USBLog(3,"AppleUSBHub[%p]::WaitForPowerOn commandSleep woke up with status (%p) _myPowerState(%d)", this, (void*)kr, (int)_myPowerState);
				USBTrace( kUSBTHub, kTPHubWaitForPowerOn, (uintptr_t)this, kr, (uintptr_t)_myPowerState, 8 );
		}
	}
	else
	{
		int		retries = timeout / 10;
		while (((_myPowerState < kIOUSBHubPowerStateOn) or (_powerStateChangingTo == kIOUSBHubPowerStateLowPower)) && (retries-- > 0))
		{
			USBLog(6, "AppleUSBHub[%p]::WaitForPowerOn - not in gate, sleeping 10 ms _myPowerState[%d] (retries = %d)", this, (int)_myPowerState, (int)retries);
			USBTrace( kUSBTHub, kTPHubWaitForPowerOn, (uintptr_t)this, (uintptr_t)_myPowerState, retries, 9 );
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
            {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHub[%p]::WaitForPowerOn - IOSleep in gate:%d", this, 1);}}
#endif
			IOSleep(10);
		}
	}
	if ((_myPowerState == kIOUSBHubPowerStateOn) and (_powerStateChangingTo != kIOUSBHubPowerStateLowPower))
	{
		USBLog(6,"AppleUSBHub[%p]::WaitForPowerOn was successful, _myPowerState[%d], returning kIOReturnSuccess", this, (int)_myPowerState);
		USBTrace( kUSBTHub, kTPHubWaitForPowerOn, (uintptr_t)this, (uintptr_t)_myPowerState, 0, 10 );
		return kIOReturnSuccess;
	}
	else 
	{
		USBLog(1,"AppleUSBHub[%p]::WaitForPowerOn was not successful, _myPowerState[%d], _powerStateChangingTo[%d] - returning kIOReturnInternalErr", this, (int)_myPowerState, (int)_powerStateChangingTo);
		USBTrace( kUSBTHub, kTPHubWaitForPowerOn, (uintptr_t)this, (uintptr_t)_myPowerState, (uintptr_t)_powerStateChangingTo, 11 );
		return kIOReturnInternalError;
	}
}



void
AppleUSBHub::WakeOnPowerOn( )
{
	if ( _workLoop and _gate and _waitingForPowerOn)
	{
		USBLog(2, "AppleUSBHub[%p]::WakeOnPowerOn - waking up commandSleep", this);
		_gate->commandWakeup(&_waitingForPowerOn, true);
	}
	else
	{
		USBLog(2, "AppleUSBHub[%p]::WakeOnPowerOn - nothing to do", this);
	}
}



void
AppleUSBHub::InitialDelayEntry(OSObject *target)
{
    AppleUSBHub *	me = OSDynamicCast(AppleUSBHub, target);
	
    if (!me)
        return;
	
	USBLog(7, "AppleUSBHub[%p]::InitialDelayEntry - calling in", me);
	me->InitialDelay();
	USBLog(7, "AppleUSBHub[%p]::InitialDelayEntry - done", me);
	me->release();
}



void
AppleUSBHub::InitialDelay(void)
{
	if (!isInactive())
	{
		USBLog(5, "AppleUSBHub[%p]::InitialDelay - sleeping for %d milliseconds", this, (int)kInitialDelayTime);
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
        {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHub[%p]::InitialDelay - IOSleep in gate:%d", this, 1);}}
#endif
		IOSleep(kInitialDelayTime);						// delay some time before going to low power
	}
	
	LowerPowerState();
	USBLog(5, "AppleUSBHub[%p]::InitialDelay - done", this);
}

void
AppleUSBHub::HubResetPortAfterPowerChangeDoneEntry(OSObject *target)
{
    AppleUSBHub *	me = OSDynamicCast(AppleUSBHub, target);
	
    if (!me)
        return;
	
	USBLog(7, "AppleUSBHub[%p]::HubResetPortAfterPowerChangeDoneEntry - calling in", me);
	me->HubResetPortAfterPowerChangeDone();
	USBLog(7, "AppleUSBHub[%p]::HubResetPortAfterPowerChangeDoneEntry - done", me);
	me->release();
}

void
AppleUSBHub::HubResetPortAfterPowerChangeDone()
{
	if (isInactive())
	{
		USBLog(4, "AppleUSBHub[%p]::HubResetPortAfterPowerChangeDone - called while inActive - ignoring", this);
		return;
	}
	
	USBLog(6, "AppleUSBHub[%p]::HubResetPortAfterPowerChangeDone - calling IncrementOutstandingIO", this);
	IncrementOutstandingIO();		// make sure we don't close until start is done
	
	// Ask our device to reset our port
	//
	USBLog(6, "AppleUSBHub[%p]::HubResetPortAfterPowerChangeDone  calling ResetDevice()", this);
	
	IOReturn err = _device->ResetDevice();
	if ( err != kIOReturnSuccess || isInactive())
	{
		// If we get an error from ResetDevice(), probably means that the device is AWOL, so just forget about it.
		USBLog(3, "AppleUSBHub[%p]::HubResetPortAfterPowerChangeDone - ResetDevice returned 0x%x, or inActive(%d), calling DecrementOutstandingIO() and bailing out", this, err, isInactive());
		USBTrace( kUSBTHub,  kTPHubPowerChangeDone, (uintptr_t)this, err, 0,  1 );
		_okToCloseWhileOff = true;
		DecrementOutstandingIO();
		return;
	}
	
	if (!_inStartMethod)
	{
		_inStartMethod = true;
		USBLog(6, "AppleUSBHub[%p]::HubResetPortAfterPowerChangeDone -- reconfiguring hub (_checkPortsThreadActive = %d)", this, _checkPortsThreadActive);
		
		// Abort any transactions (should not have any pending at this point)
		//
		if ( _interruptPipe )
		{
			_interruptPipe->Abort();
			_interruptPipe->release();
			_interruptPipe = NULL;
		}
		
		// When we reconfigure the hub, we'll recreate the hubInterface, so let's tear it down.
		//
		if (_hubInterface) 
		{
			USBLog(6, "AppleUSBHub[%p]::HubResetPortAfterPowerChangeDone -- checking _checkPortsThreadActive = %d", this, _checkPortsThreadActive);
			int	retries = 500;
			
			// Wait for up to 5 seconds if we have a thread outstanding 
			while (_checkPortsThreadActive && (--retries > 0))
			{
				USBLog(1, "AppleUSBHub[%p]::HubResetPortAfterPowerChangeDone  _checkPortsThreadActive is active, sleeping 10ms (%d)", this, retries);
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
                {if(_bus->getWorkLoop()->inGate()){USBLog(1, "AppleUSBHub[%p]::HubResetPortAfterPowerChangeDone - IOSleep in gate:%d", this, 1);}}
#endif
				IOSleep(10);
			}
			USBLog(6, "AppleUSBHub[%p]::HubResetPortAfterPowerChangeDone -- about to close interface ( _checkPortsThreadActive = %d)", this, _checkPortsThreadActive);
			_hubInterface->close(this);
			USBLog(6, "AppleUSBHub[%p]::HubResetPortAfterPowerChangeDone -- about to release interface ( _checkPortsThreadActive = %d)", this, _checkPortsThreadActive);
			_hubInterface->release();
			_hubInterface = NULL;
		}
		
		// release our existing buffers, if any
		if (_buffer) 
		{
			_buffer->release();
			_buffer = NULL;
		}
		
		// Reconfigure our Hub. 
		err = ConfigureHub();
		if ( err ) 
		{
			USBLog(3, "AppleUSBHub[%p]::HubResetPortAfterPowerChangeDone  Reconfiguring hub returned: 0x%x", this, err);
		}
		else
		{
#ifdef SUPPORTS_SS_USB
			if (_ssHub)
			{
				USBLog(1, "[%p] (Reset) USB3 Generic Hub @ %d (0x%x)", this, _address, (uint32_t)_locationID);
			}
			else
#endif
			{
				USBLog(1, "[%p] (Reset) USB Generic Hub @ %d (0x%x)", this, _address, (uint32_t)_locationID);
			}
			USBTrace( kUSBTHub,  kTPHubMessage, (uintptr_t)this, _address, (uint32_t)_locationID, 0 );
		}
		// Set the "wakeup time"
		clock_get_uptime(&_wakeupTime);
		
		_inStartMethod = false;
	}
	
	// Now, make sure that our hub goes back to the on state
	USBLog(6, "AppleUSBHub[%p]::HubResetPortAfterPowerChangeDone - calling changePowerStateToPriv(kIOUSBHubPowerStateOn)", this);
	USBTrace( kUSBTHub,  kTPHubPowerChangeDone, (uintptr_t)this, 0, 0, kIOUSBHubPowerStateOn );
	
	changePowerStateToPriv(kIOUSBHubPowerStateOn);
	powerOverrideOffPriv();
	
	USBLog(6, "AppleUSBHub[%p]::HubResetPortAfterPowerChangeDone - calling DecrementOutstandingIO", this);
	DecrementOutstandingIO();
	
}	


//================================================================================================
//
//	bool		IsHubDeviceInternal
//
//	Checks to see if the hub hardware that this instance of AppleUSBHub is driving is internal to the hardware
//	This will of course be true for the root hub (see IOUSBRootHubDevice::GetDeviceInformation)
//	This will also be true if the hub is connected to a captive port of the root hub (based on that port's _captive iVar)
//
//================================================================================================
bool
AppleUSBHub::IsHubDeviceInternal()
{
	UInt32			info = 0;
	IOReturn		status = kIOReturnSuccess;
	
	status = _device->GetDeviceInformation(&info);
	
	if (status != kIOReturnSuccess)
	{
		USBLog(3, "AppleUSBHub[%p]::IsHubDeviceInternal - Hub @ 0x%x GetDeviceInformation returned 0x%x, exiting", this, (uint32_t)_locationID, status );
		return false;
	}
	
	// If not built-in return false
	if( not (info & (1 << kUSBInformationDeviceIsInternalBit)) )
	{
		USBLog(3, "AppleUSBHub[%p]::IsHubDeviceInternal - Hub @ 0x%x is not internal, exiting", this, (uint32_t)_locationID );
		return false;
	}
	
	return true;
}

#ifdef SUPPORTS_SS_USB

void				
AppleUSBHub::ControllerHasMuxedPorts(bool *isMuxed)
{
	bool muxed = false;
    
	USBLog(6, "AppleUSBHub[%p]::ControllerHasMuxedPorts - Hub @ 0x%x is %s", this, (uint32_t)_locationID, _device->getName() );
	
	if( IsHubDeviceInternal() )
	{
		// If we get here, this means that the hub is an internal hub.  It could be a root hub or an internal hub
		
		// parent will be the actual Host Controller Driver (HCD) for my hub device (AppleUSBEHCI, AppleUSBOHCI, etc).  What about an internal hub (e.g. SMSC hub in K23F)?
		IORegistryEntry *parent = _device->getParentEntry(gIOServicePlane);
		
		if (parent)
		{
			// grandparent will be the IOPCIDevice for the Host Controller (HC) hardware
			IORegistryEntry *grandParent = parent->getParentEntry(gIOServicePlane);
			
			if (grandParent)
			{
                muxed = _device->GetBus()->IsControllerMuxed(grandParent, _locationID); 
			}
		}	
	}
	
	USBLog(6, "AppleUSBHub[%p]::ControllerHasMuxedPorts - Hub @ 0x%x is %s", this, (uint32_t)_locationID, (muxed == true)? "muxed" : "not muxed");
	
    *isMuxed = muxed;
}

void
AppleUSBHub::GetACPIPortMuxProperties(UInt32 portnum, bool *isMuxed, char *muxName)
{
	bool muxed = false;
    
	USBLog(6, "AppleUSBHub[%p]::GetACPIPortMuxProperties - Hub @ 0x%x is %s", this, (uint32_t)_locationID, _device->getName() );
	
	if( IsHubDeviceInternal() )
	{
		// If we get here, this means that the hub is an internal hub.  It could be a root hub or an internal hub
		
		// parent will be the actual Host Controller Driver (HCD) for my hub device (AppleUSBEHCI, AppleUSBOHCI, etc).  What about an internal hub (e.g. SMSC hub in K23F)?
		IORegistryEntry *parent = _device->getParentEntry(gIOServicePlane);
		
		if (parent)
		{
			// grandparent will be the IOPCIDevice for the Host Controller (HC) hardware
			IORegistryEntry *grandParent = parent->getParentEntry(gIOServicePlane);
			
			if (grandParent)
			{
                muxed = _device->GetBus()->IsPortMuxed(grandParent, portnum, _locationID, muxName); 
			}
		}	
	}
	
	USBLog(6, "AppleUSBHub[%p]::GetACPIPortMuxProperties - Hub @ 0x%x port %d is %s method %s", this, (uint32_t)_locationID, (unsigned int)portnum, (muxed == true)? "muxed" : "not muxed", (muxed == true)? muxName:"");
	
    *isMuxed = muxed;
}	
#endif	



bool
AppleUSBHub::GetInternalHubErrataBits(UInt32 *errataBits)
{
	bool returnValue = false;
	bool available   = false;
    bool internalHub = IsHubDeviceInternal();
    
	USBLog(6, "AppleUSBHub[%p]::GetInternalHubErrataBits - Hub @ 0x%x is %s %s", this, (uint32_t)_locationID, (internalHub == true)? "internal" : "external", _device->getName());
	
	if (internalHub)
	{
		// If we get here, this means that the hub is an internal hub.  It could be a root hub or an internal hub
		
		// parent will be the actual Host Controller Driver (HCD) for my hub device (AppleUSBEHCI, AppleUSBOHCI, etc).  What about an internal hub (e.g. SMSC hub in K23F)?
		IORegistryEntry *parent = _device->getParentEntry(gIOServicePlane);
		
		if (parent)
		{
			// grandparent will be the IOPCIDevice for the Host Controller (HC) hardware
			IORegistryEntry *grandParent = parent->getParentEntry(gIOServicePlane);
			
			if (grandParent)
			{
				for ( int portNumber = 1; portNumber <= _hubDescriptor.numPorts; portNumber++)
				{
 					UInt32		internalPortErrata = 0;

               		available = _device->GetBus()->GetInternalHubErrataBits(grandParent, portNumber, _locationID, &internalPortErrata); 
					if (available)
					{
						returnValue = true;
						*errataBits |= internalPortErrata;
					}
				}
			}
		}
	}
	
	USBLog(6, "AppleUSBHub[%p]::GetInternalHubErrataBits - Hub @ 0x%x %s errataBits 0x%x", this, (uint32_t)_locationID, (returnValue == true)? "found" : "not found", (uint32_t)*errataBits);
	
    return returnValue;;
}	

//================================================================================================
//
//	void	GetACPIPortCaptiveProperties
//
//	Checks to see if the device on the given port of this hub (driven by AppleUSBHub) is internal to the hardware and whether it has
//  an external connector
//	It starts by calling IsHubDeviceInternal to make sure that the hub itself is internal, and then calls the controller to 
//	check further
//
//================================================================================================
void
AppleUSBHub::GetACPIPortCaptiveProperties(UInt32 portnum, bool *isInternal, bool *hasExternalConnector)
{
	bool internal = false;
	bool hasConnector = false;
    
	USBLog(6, "AppleUSBHub[%p]::GetACPIPortCaptiveProperties - Hub @ 0x%x is %s", this, (uint32_t)_locationID, _device->getName() );
	
	if( IsHubDeviceInternal() )
	{
		// If we get here, this means that the hub is an internal hub.  It could be a root hub or an internal hub
		
		// parent will be the actual Host Controller Driver (HCD) for my hub device (AppleUSBEHCI, AppleUSBOHCI, etc).  What about an internal hub (e.g. SMSC hub in K23F)?
		IORegistryEntry *parent = _device->getParentEntry(gIOServicePlane);
		
		if (parent)
		{
			// grandparent will be the IOPCIDevice for the Host Controller (HC) hardware
			IORegistryEntry *grandParent = parent->getParentEntry(gIOServicePlane);
			
			if (grandParent)
			{
				// note that _device->GetBus() is the same object as parent - just an FYI
				internal = _device->GetBus()->IsPortInternal(grandParent, portnum, _locationID);
			}
		}	
		
		// Now, if the port is NOT internal, but the HUB is internal, then the port has an external connector
		hasConnector = !internal;
	}
	
	USBLog(6, "AppleUSBHub[%p]::GetACPIPortCaptiveProperties - Hub @ 0x%x has %s device in port %d, hasConnector: %d", this, (uint32_t)_locationID, (internal == true)? "internal" : "external", (unsigned int)portnum, hasConnector );
	
	*isInternal = internal;
	*hasExternalConnector =hasConnector; 
}	


//================================================================================================
//
//	bool	HasExpressCardPort
//
//	Checks to see if this hub has an express card connector on ANY of the ports
//
//================================================================================================
bool
AppleUSBHub::HasExpressCardPort()
{
	USBLog(6, "AppleUSBHub[%p]::HasExpressCardPort - Hub @ 0x%x is %s", this, (uint32_t)_locationID, _device->getName() );

	if( IsHubDeviceInternal() )
	{
		IORegistryEntry *parent = _device->getParentEntry(gIOServicePlane);
		
		if (parent)
		{
			IORegistryEntry *grandParent = parent->getParentEntry(gIOServicePlane);
			
			if (grandParent)
			{
				_expressCardPort = _device->GetBus()->ExpressCardPort(grandParent);
			}
		}	
	}
	
	USBLog(6, "AppleUSBHub[%p]::HasExpressCardPort - Hub @ 0x%x _expressCardPort %s", this, (uint32_t)_locationID, (_expressCardPort > 0)? "found" : "not found" );

	return (_expressCardPort > 0);
}	

#ifdef SUPPORTS_SS_USB

//================================================================================================
//
//	IOReturn	DeviceDisconnected
//
//	Device Disconnected on a muxed port so inform its root hub
//
//================================================================================================
IOReturn 
AppleUSBHub::DeviceDisconnected(UInt16 port, char *muxName)
{
    IOUSBControllerV2 	*controller;
	IOReturn			err = kIOReturnSuccess;

    USBLog(3, "AppleUSBHub[%p]::DeviceDisconnected - Hub @ 0x%x port %d", this, (uint32_t)_locationID, port);  

	EnsureUsability();
	
    controller = OSDynamicCast(IOUSBControllerV2, _bus);
    
    if ( !controller )
    {
        return kIOReturnBadArgument;
    }

    if ( IsHubDeviceInternal() || _isRootHub )
    {            
        if( controller->_rootHubDevice && controller->_rootHubDevice->GetPolicyMaker() )
        {
			USBLog(3, "AppleUSBHub[%p]::DeviceDisconnected - Hub @ 0x%x port %d mux method %s sending kIOUSBMessageHubPortDeviceDisconnected", this, (uint32_t)_locationID, port, muxName);  
            err = controller->_rootHubDevice->GetPolicyMaker()->message(kIOUSBMessageHubPortDeviceDisconnected, this, (void *)(muxName));
            
			USBTrace( kUSBTHub, kTPHubPortDeviceDisconnected, (uintptr_t)this, (uint32_t)_locationID, (uintptr_t)controller->_rootHubDevice, port );
        }
    }

	return err;
}

#endif

#pragma mark Utility Functions
//================================================================================================
//   HubMessageToString
//================================================================================================
const char *	
AppleUSBHub::HubMessageToString(UInt32 message)
{
	switch (message)
	{
		case kIOUSBMessageHubResetPort:									// 0xe00004001
			return "kIOUSBMessageHubResetPort";
		case kIOUSBMessageHubSuspendPort:								// 0xe00004002
			return "kIOUSBMessageHubSuspendPort";
		case kIOUSBMessageHubResumePort:								// 0xe00004003
			return "kIOUSBMessageHubResumePort";
		case kIOUSBMessageHubIsDeviceConnected:							// 0xe00004004
			return "kIOUSBMessageHubIsDeviceConnected";
		case kIOUSBMessageHubIsPortEnabled:								// 0xe00004005
			return "kIOUSBMessageHubIsPortEnabled";
		case kIOUSBMessageHubReEnumeratePort:							// 0xe00004006
			return "kIOUSBMessageHubReEnumeratePort";
		case kIOUSBMessagePortHasBeenReset:								// 0xe0000400a
			return "kIOUSBMessagePortHasBeenReset";
		case kIOUSBMessagePortHasBeenResumed:							// 0xe0000400b
			return "kIOUSBMessagePortHasBeenResumed";			
		case kIOUSBMessageHubPortClearTT:								// 0xe0000400c
			return "kIOUSBMessageHubPortClearTT";
		case kIOUSBMessagePortHasBeenSuspended:							// 0xe0000400d
			return "kIOUSBMessagePortHasBeenSuspended";
		case kIOUSBMessageFromThirdParty:								// 0xe0000400e
			return "kIOUSBMessageFromThirdParty";
		case kIOUSBMessagePortWasNotSuspended:							// 0xe0000400f
			return "kIOUSBMessagePortWasNotSuspended";
		case kIOUSBMessageExpressCardCantWake:							// 0xe00004010
			return "kIOUSBMessageExpressCardCantWake";
		case kIOUSBMessageCompositeDriverReconfigured:					// 0xe00004011
			return "kIOUSBMessageCompositeDriverReconfigured";
		case kIOUSBMessageHubSetPortRecoveryTime:						// 0xe00004012
			return "kIOUSBMessageHubSetPortRecoveryTime";
		case kIOUSBMessageOvercurrentCondition:							// 0xe00004013
			return "kIOUSBMessageOvercurrentCondition";
		case kIOUSBMessageNotEnoughPower:								// 0xe00004014
			return "kIOUSBMessageNotEnoughPower";
		case kIOUSBMessageController:									// 0xe00004015
			return "kIOUSBMessageController";
		case kIOUSBMessageRootHubWakeEvent:								// 0xe00004016
			return "kIOUSBMessageRootHubWakeEvent";
		case kIOUSBMessageReleaseExtraCurrent:							// 0xe00004017
			return "kIOUSBMessageReleaseExtraCurrent";
		case kIOUSBMessageReallocateExtraCurrent:						// 0xe00004018
			return "kIOUSBMessageReallocateExtraCurrent";
		case kIOUSBMessageHubPortDeviceDisconnected:
            return "kIOUSBMessageHubPortDeviceDisconnected";
            
		case kIOMessageServiceIsTerminated: 	
			return "kIOMessageServiceIsTerminated";
		case kIOMessageServiceIsRequestingClose:
			return "kIOMessageServiceIsRequestingClose";
		case kIOMessageServiceIsAttemptingOpen:
			return "kIOMessageServiceIsAttemptingOpen";
		case kIOMessageServiceIsSuspended:
			return "kIOMessageServiceIsSuspended";
		case kIOMessageServiceIsResumed:
			return "kIOMessageServiceIsResumed";
		case kIOMessageServiceWasClosed:
			return "kIOMessageServiceWasClosed";
		case kIOMessageDeviceSignaledWakeup:
			return "kIOMessageDeviceSignaledWakeup";
			
		default:
			return "UNKNOWN";
	}
}



//================================================================================================
//   FeatureName
//================================================================================================
const char *	
AppleUSBHub::FeatureName(UInt32 feature)
{
	switch (feature)
	{
		case kUSBHubPortConnectionFeature:									
			return "kUSBHubPortConnectionFeature";
		case kUSBHubPortEnableFeature:									
			return "kUSBHubPortEnableFeature";
		case kUSBHubPortSuspendFeature:									
			return "kUSBHubPortSuspendFeature";
		case kUSBHubPortOverCurrentFeature:									
			return "kUSBHubPortOverCurrentFeature";
		case kUSBHubPortResetFeature:									
			return "kUSBHubPortResetFeature";
		case kUSBHubPortPowerFeature:									
			return "kUSBHubPortPowerFeature";
		case kUSBHubPortLowSpeedFeature:									
			return "kUSBHubPortLowSpeedFeature";
		case kUSBHubPortConnectionChangeFeature:									
			return "kUSBHubPortConnectionChangeFeature";
		case kUSBHubPortEnableChangeFeature:									
			return "kUSBHubPortEnableChangeFeature";
		case kUSBHubPortSuspendChangeFeature:									
			return "kUSBHubPortSuspendChangeFeature";
		case kUSBHubPortOverCurrentChangeFeature:									
			return "kUSBHubPortOverCurrentChangeFeature";
		case kUSBHubPortResetChangeFeature:									
			return "kUSBHubPortResetChangeFeature";
		case kUSBHubPortTestFeature:									
			return "kUSBHubPortTestFeature";
		case kUSBHubPortIndicatorFeature:									
			return "kUSBHubPortIndicatorFeature";
#ifdef SUPPORTS_SS_USB
		case kUSBHubPortLinkStateFeature:									
			return "kUSBHubPortLinkStateFeature";
		case kUSBHubPortU1TimeoutFeature:									
			return "kUSBHubPortU1TimeoutFeature";
		case kUSBHubPortU2TimeoutFeature:									
			return "kUSBHubPortU2TimeoutFeature";
		case kUSBHubPortLinkStateChangeFeature:									
			return "kUSBHubPortLinkStateChangeFeature";
		case kUSBHubPortConfigErrorChangeFeature:									
			return "kUSBHubPortConfigErrorChangeFeature";
		case kUSBHubPortRemoteWakeMaskFeature:									
			return "kUSBHubPortRemoteWakeMaskFeature";
		case kUSBHubPortBHPortResetFeature:									
			return "kUSBHubPortBHPortResetFeature";
		case kUSBHubPortBHResetChangeFeature:									
			return "kUSBHubPortBHResetChangeFeature";
		case kUSBHubPortForceLinkPMAcceptFeature:									
			return "kUSBHubPortForceLinkPMAcceptFeature";
#endif			
		default:
			return "Unknown Feature";
	}
}

#ifdef SUPPORTS_SS_USB
//================================================================================================
//   LinkStateName
//================================================================================================
const char *	
AppleUSBHub::LinkStateName(UInt32 state)
{
	switch (state)
	{
		case 0:									
			return "U0";
		case 1:									
			return "U1";
		case 2:									
			return "U2";
		case 3:									
			return "U3";
		case 4:									
			return "SS.Disabled)";
		case 5:									
			return "Rx.Detect)";
		case 6:									
			return "SS.Inactive";
		case 7:									
			return "Polling)";
		case 8:									
			return "Recovery";
		case 9:									
			return "Hot Reset";
		case 0xA:									
			return "Compliance Mode";
		case 0xB:									
			return "Loopback";
		case 0xC:									
		case 0xD:									
		case 0xE:									
		case 0xF:									
			return "Reserved State";
			
		default:
			return "Unknown State";
	}
}

#endif


