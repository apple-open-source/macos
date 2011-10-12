/*
 * Copyright й 1998-2010 Apple Inc.  All rights reserved.
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
#include <libkern/OSByteOrder.h>
#include <IOKit/IOService.h> 
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOKitKeys.h>

#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBLog.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/IOUSBHIDDriver.h>
#include <IOKit/usb/IOUSBHubPolicyMaker.h>
#import "USBTracepoints.h"

/* Convert USBLog to use kprintf debugging */
#ifndef IOUSBHIDDriver_USE_KPRINTF
	#define IOUSBHIDDriver_USE_KPRINTF 0
#endif

#if IOUSBHIDDriver_USE_KPRINTF
#undef USBLog
#undef USBError
void kprintf(const char *format, ...)
__attribute__((format(printf, 1, 2)));
#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= IOUSBHIDDriver_USE_KPRINTF) { kprintf( FORMAT "\n", ## ARGS ) ; }
#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif

//================================================================================================
//
//   Local Definitions
//
//================================================================================================
//
#define super IOHIDDevice

#define _WORKLOOP								_usbHIDExpansionData->_workLoop
#define _HANDLE_REPORT_THREAD					_usbHIDExpansionData->_handleReportThread
#define _ROOT_DOMAIN							_usbHIDExpansionData->_rootDomain
#define _WAKEUP_TIME							_usbHIDExpansionData->_wakeUpTime
#define _COMPLETION_WITH_TIMESTAMP				_usbHIDExpansionData->_completionWithTimeStamp
#define _CHECK_FOR_TIMESTAMP					_usbHIDExpansionData->_checkForTimeStamp
#define _MS_TO_IGNORE_TRANSACTIONS_AFTER_WAKE	_usbHIDExpansionData->_msToIgnoreTransactionsAfterWake
#define _SUSPENDPORT_TIMER						_usbHIDExpansionData->_suspendPortTimer
#define _PORT_SUSPENDED							_usbHIDExpansionData->_portSuspended
#define _SUSPEND_TIMEOUT_IN_MS					_usbHIDExpansionData->_suspendTimeoutInMS
#define _INTERFACE_NUMBER						_usbHIDExpansionData->_interfaceNumber
#define _LOG_HID_REPORTS						_usbHIDExpansionData->_logHIDReports
#define _HID_LOGGING_LEVEL						_usbHIDExpansionData->_hidLoggingLevel
#define	_NEED_TO_CLEARPIPESTALL					_usbHIDExpansionData->_needToClearPipeStall
#define _QUEUED_REPORTS							_usbHIDExpansionData->_queuedReports
#define	_INTERRUPT_TIMESTAMP					_usbHIDExpansionData->_interruptTimeStamp
#define	_POWERSTATECHANGING						_usbHIDExpansionData->_powerStateChanging
#define	_MYPOWERSTATE							_usbHIDExpansionData->_myPowerState
#define	_PENDINGREAD							_usbHIDExpansionData->_pendingRead
#define _DEAD_DEVICE_CHECK_LOCK					_usbHIDExpansionData->_deviceDeadCheckLock
#define _HANDLEREPORTTIMESTAMP					_usbHIDExpansionData->_handleReportTimeStamp

#define ABORTEXPECTED                       _deviceIsDead

#define kMaxQueuedReports					1

// a USB HID device has two power states, off and on
// Note: This defines two states. off and on. In the off state, the upstream is suspended.
static IOPMPowerState ourPowerStates[kUSBHIDNumberPowerStates] = {
	{	// kUSBHIDPowerStateOff - all ports should be off and all devices disconnected
		kIOPMPowerStateVersion1,	// version - version number of this struct
		0,							// capabilityFlags - bits that describe (to interested drivers) the capability of the device in this state
		0,							// outputPowerCharacter - description (to power domain children) of the power provided in this state
		0,							// inputPowerRequirement - description (to power domain parent) of input power required in this state
		0,							// staticPower - average consumption in milliwatts
		0,							// unbudgetedPower - additional consumption from separate power supply (mw)
		0,							// powerToAttain - additional power to attain this state from next lower state (in mw)
		0,							// timeToAttain - time required to enter this state from next lower state (in microseconds)
		0,							// settleUpTime - settle time required after entering this state from next lower state (microseconds)
		0,							// timeToLower - time required to enter next lower state from this one (in microseconds)
		0,							// settleDownTime - settle time required after entering next lower state from this state (microseconds)
		0							// powerDomainBudget - power in mw a domain in this state can deliver to its children
	},
	{	// kUSBHIDPowerStateRestart - all ports should be off and all devices disconnected
		kIOPMPowerStateVersion1,	// version - version number of this struct
		kIOPMRestartCapability,		// capabilityFlags - bits that describe (to interested drivers) the capability of the device in this state
		kIOPMRestart,				// outputPowerCharacter - description (to power domain children) of the power provided in this state
		kIOPMRestart,				// inputPowerRequirement - description (to power domain parent) of input power required in this state
		0,							// staticPower - average consumption in milliwatts
		0,							// unbudgetedPower - additional consumption from separate power supply (mw)
		0,							// powerToAttain - additional power to attain this state from next lower state (in mw)
		0,							// timeToAttain - time required to enter this state from next lower state (in microseconds)
		0,							// settleUpTime - settle time required after entering this state from next lower state (microseconds)
		0,							// timeToLower - time required to enter next lower state from this one (in microseconds)
		0,							// settleDownTime - settle time required after entering next lower state from this state (microseconds)
		0							// powerDomainBudget - power in mw a domain in this state can deliver to its children
	},
	{	// kUSBHIDPowerStateSleep - upstream port should be suspended
		kIOPMPowerStateVersion1,	// version - version number of this struct
		kIOPMSleepCapability,		// capabilityFlags - bits that describe (to interested drivers) the capability of the device in this state
		kIOPMSleep,					// outputPowerCharacter - description (to power domain children) of the power provided in this state
		kIOPMSleep,					// inputPowerRequirement - description (to power domain parent) of input power required in this state
		0,							// staticPower - average consumption in milliwatts
		0,							// unbudgetedPower - additional consumption from separate power supply (mw)
		0,							// powerToAttain - additional power to attain this state from next lower state (in mw)
		0,							// timeToAttain - time required to enter this state from next lower state (in microseconds)
		0,							// settleUpTime - settle time required after entering this state from next lower state (microseconds)
		0,							// timeToLower - time required to enter next lower state from this one (in microseconds)
		0,							// settleDownTime - settle time required after entering next lower state from this state (microseconds)
		0							// powerDomainBudget - power in mw a domain in this state can deliver to its children
	},
	{	// kUSBHIDPowerStateLow Power - all ports should be SUSPENDED and devices still connected
		kIOPMPowerStateVersion1,	// version - version number of this struct
		kIOPMLowPower,				// capabilityFlags - bits that describe (to interested drivers) the capability of the device in this state
		kIOPMLowPower,				// outputPowerCharacter - description (to power domain children) of the power provided in this state
		kIOPMLowPower,				// inputPowerRequirement - description (to power domain parent) of input power required in this state
		0,							// staticPower - average consumption in milliwatts
		0,							// unbudgetedPower - additional consumption from separate power supply (mw)
		0,							// powerToAttain - additional power to attain this state from next lower state (in mw)
		0,							// timeToAttain - time required to enter this state from next lower state (in microseconds)
		0,							// settleUpTime - settle time required after entering this state from next lower state (microseconds)
		0,							// timeToLower - time required to enter next lower state from this one (in microseconds)
		0,							// settleDownTime - settle time required after entering next lower state from this state (microseconds)
		0							// powerDomainBudget - power in mw a domain in this state can deliver to its children
	},
	{	// kUSBHIDPowerStateSleep - i am receiving SOF tokens and my port is on	
		kIOPMPowerStateVersion1,	// version - version number of this struct
		IOPMDeviceUsable,			// capabilityFlags - bits that describe (to interested drivers) the capability of the device in this state
		IOPMPowerOn,				// outputPowerCharacter - description (to power domain children) of the power provided in this state
		IOPMPowerOn,				// inputPowerRequirement - description (to power domain parent) of input power required in this state
		0,							// staticPower - average consumption in milliwatts
		0,							// unbudgetedPower - additional consumption from separate power supply (mw)
		0,							// powerToAttain - additional power to attain this state from next lower state (in mw)
		0,							// timeToAttain - time required to enter this state from next lower state (in microseconds)
		0,							// settleUpTime - settle time required after entering this state from next lower state (microseconds)
		0,							// timeToLower - time required to enter next lower state from this one (in microseconds)
		0,							// settleDownTime - settle time required after entering next lower state from this state (microseconds)
		0							// powerDomainBudget - power in mw a domain in this state can deliver to its children
	}
};


//================================================================================================
//
//   IOUSBHIDDriver Methods
//
//================================================================================================
//
OSDefineMetaClassAndStructors(IOUSBHIDDriver, IOHIDDevice)


#pragma mark ееееееее IOService Methods еееееееее
//================================================================================================
//
//  init
//
//  Do what is necessary to start device before probe is called.
//
//================================================================================================
//
bool 
IOUSBHIDDriver::init(OSDictionary *properties)
{
    if (!super::init(properties))
    {
        return false;
    }

    // Allocate our expansion data
    //
    if (!_usbHIDExpansionData)
    {
        _usbHIDExpansionData = (IOUSBHIDDriverExpansionData *)IOMalloc(sizeof(IOUSBHIDDriverExpansionData));
        if (!_usbHIDExpansionData)
            return false;
        bzero(_usbHIDExpansionData, sizeof(IOUSBHIDDriverExpansionData));
    }
    _retryCount = kHIDDriverRetryCount;
    _maxReportSize = kMaxHIDReportSize;
	_HID_LOGGING_LEVEL = 7;

    return true;
}



void
IOUSBHIDDriver::stop(IOService *  provider)
{
    USBLog(7, "IOUSBHIDDriver(%s)[%p]::stop isInactive = %d", getName(), this, isInactive());
	
	if (_WORKLOOP && _gate)
		_WORKLOOP->removeEventSource(_gate);
	
	if (_WORKLOOP && _SUSPENDPORT_TIMER)
		_WORKLOOP->removeEventSource( _SUSPENDPORT_TIMER );
	
    super::stop(provider);
}



//================================================================================================
//
//  free
//
//================================================================================================
//
void
IOUSBHIDDriver::free()
{
	if (_interruptPipe)
	{
		_interruptPipe->release();
		_interruptPipe = NULL;
	}
	
	if (_interface)
	{
		_interface->release();
		_interface = NULL;
	}
	
	if (_device)
	{
		_device->release();
		_device = NULL;
	}
	
    if (_gate)
    {
        _gate->release();
        _gate = NULL;
    }
	
	if (_buffer)
    {
        _buffer->release();
        _buffer = NULL;
    }
	
    //  This needs to be the LAST thing we do, as it disposes of our "fake" member
    //  variables.
    //
    if (_usbHIDExpansionData)
    {
		if ( _SUSPENDPORT_TIMER )
		{
			_SUSPENDPORT_TIMER->release();
			_SUSPENDPORT_TIMER = NULL;
			_SUSPEND_TIMEOUT_IN_MS= 0;
		}

		if (_WORKLOOP)
		{
			_WORKLOOP->release();
			_WORKLOOP = NULL;
		}
		
		IOFree(_usbHIDExpansionData, sizeof(IOUSBHIDDriverExpansionData));
        _usbHIDExpansionData = NULL;
    }
	
	super::free();
}


//=============================================================================================
//
//  start
//
//=============================================================================================
//
bool
IOUSBHIDDriver::start(IOService *provider)
{
    IOReturn					err = kIOReturnSuccess;
    IOWorkLoop	*				workLoop = NULL;
    IOCommandGate *				commandGate = NULL;
    OSNumber *					locationIDProperty = NULL;
    IOUSBFindEndpointRequest	request;
    bool						addEventSourceSuccess = false;
    UInt32						maxInputReportSize = 0;
    OSNumber *					inputReportSize = NULL;
	OSNumber *					numberObj = NULL;
    OSObject *					propertyObj = NULL;
	OSBoolean *					sixAxisProperty = NULL;
    
	USBTrace_Start( kUSBTHID, kTPHIDStart, (uintptr_t)this, 0, 0, 0);
	
    USBLog(6, "IOUSBHIDDriver(%s)[%p]::start", getName(), this);
    IncrementOutstandingIO();			// make sure that once we open we don't close until start is finished

    // Get our locationID as an unsigned 32 bit number
	if ( provider )
	{
		propertyObj = provider->copyProperty(kUSBDevicePropertyLocationID);
		locationIDProperty = OSDynamicCast( OSNumber, propertyObj);
		if ( locationIDProperty )
		{
			_locationID = locationIDProperty->unsigned32BitValue();
		}
		if (propertyObj)
		{
			propertyObj->release();
			propertyObj = NULL;
		}
	}
		
	// this will call handleStart, which is implemented below, and sets up _device and _interface
    if (!super::start(provider))
    {
        USBLog(1, "IOUSBHIDDriver(%s)[%p]::start - super::start returned false!", getName(), this);
		USBTrace( kUSBTHID,  kTPHIDStart, (uintptr_t)this, 0, 0, 1 );
        goto ErrorExit;
    }

    // Attempt to create a command gate for our driver
    //
    commandGate = IOCommandGate::commandGate(this);

    if (!commandGate)
    {
        USBLog(1, "IOUSBHIDDriver(%s)[%p]::start - could not get a command gate", getName(), this);
		USBTrace( kUSBTHID,  kTPHIDStart, (uintptr_t)this, 0, 0, 2 );
        goto ErrorExit;
    }

    workLoop = getWorkLoop();
    if ( !workLoop)
    {
        USBLog(1, "IOUSBHIDDriver(%s)[%p]::start - unable to find my workloop", getName(), this);
        USBTrace( kUSBTHID,  kTPHIDStart, (uintptr_t)this, 0, 0, 3 );
        goto ErrorExit;
    }

    // Hold on to the workloop in cause we're being unplugged at the same time
    //
    workLoop->retain();

    if (workLoop->addEventSource(commandGate) != kIOReturnSuccess)
    {
        USBLog(1, "IOUSBHIDDriver(%s)[%p]::start - unable to add gate to work loop", getName(), this);
        USBTrace( kUSBTHID,  kTPHIDStart, (uintptr_t)this, 0, 0, 4 );
        goto ErrorExit;
    }

    addEventSourceSuccess = true;

    // The HID spec specifies that only input reports should come thru the interrupt pipe.  Thus,
    // set the buffer size to the Max Input Report Size that has been decoded by the HID Mgr.
    //
	propertyObj = copyProperty(kIOHIDMaxInputReportSizeKey);
	inputReportSize = OSDynamicCast( OSNumber, propertyObj);
    if ( inputReportSize )
	{
        maxInputReportSize = inputReportSize->unsigned32BitValue();
	}
	if (propertyObj)
	{
		propertyObj->release();
		propertyObj = NULL;
	}
	
    if (maxInputReportSize == 0)
        maxInputReportSize = _interruptPipe->GetMaxPacketSize();

    if ( maxInputReportSize > 0 )
    {
        _buffer = IOBufferMemoryDescriptor::withCapacity(maxInputReportSize, kIODirectionIn);
        if ( !_buffer )
        {
            USBLog(1, "IOUSBHIDDriver(%s)[%p]::start - unable to get create buffer", getName(), this);
            USBTrace( kUSBTHID,  kTPHIDStart, (uintptr_t)this, 0, 0, 6 );
            goto ErrorExit;
        }
    }
    else
    {
        USBLog(5, "IOUSBHIDDriver(%s)[%p]::start - Device reports maxInputReportSize of 0", getName(), this);
        _buffer = NULL;
    }
    
    // Errata for ALL Saitek devices.  Do a SET_IDLE 0 call
    //
    if ( (_device->GetVendorID()) == 0x06a3 )
        SetIdleMillisecs(0);

    // For Keyboards, set the idle millecs to 24 or to 0 if from Apple
    //
    if ( (_interface->GetInterfaceClass() == kUSBHIDClass) &&
         (_interface->GetInterfaceSubClass() == kUSBHIDBootInterfaceSubClass) &&
         (_interface->GetInterfaceProtocol() == kHIDKeyboardInterfaceProtocol) )
    {
        if (_device->GetVendorID() == kIOUSBVendorIDAppleComputer)
        {
            SetIdleMillisecs(0);
        }
        else
        {
            SetIdleMillisecs(24);
        }
    }
	
	//  Look to see if we have a property to kick the device into operational mode
	propertyObj = provider->copyProperty("SIXAXIS compatibility");
	sixAxisProperty = OSDynamicCast( OSBoolean, propertyObj);
	if ( sixAxisProperty )
	{
		if ( sixAxisProperty->isTrue() )
		{
			IOUSBDevRequest requestPB;
			UInt8 buffer[18];
			
			USBLog(3, "IOUSBHIDDriver(%s)[%p]::start  SIXAXIS compatibility", getName(), this);
			
			requestPB.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBClass, kUSBInterface);
			requestPB.bRequest = kHIDRqGetReport;
			requestPB.wValue = (3 << 8) | 0xf2;
			requestPB.wIndex = _interface->GetInterfaceNumber();
			requestPB.wLength = sizeof(buffer);
			requestPB.pData = &buffer;
			requestPB.wLenDone = 0;
			
			err = _device->DeviceRequest(&requestPB);
			if ( err != kIOReturnSuccess )
			{
				USBLog(3, "IOUSBHIDDriver(%s)[%p]::start  getReport for SIXAXIS compatibility request failed; err = 0x%x)", getName(), this, err);
			}
		}
	}
	
	if (propertyObj)
	{
		propertyObj->release();
		propertyObj = NULL;
	}
	

    // Set the device into Report Protocol if it's a bootInterface subClass interface
    //
    if (_interface->GetInterfaceSubClass() == kUSBHIDBootInterfaceSubClass)
        err = SetProtocol( kHIDReportProtocolValue );
    
    // allocate a thread_call structure to see if our device is "dead" or not. We need to do this on a separate thread
    // to allow it to run without holding up the show
    //
    _deviceDeadCheckThread = thread_call_allocate((thread_call_func_t)CheckForDeadDeviceEntry, (thread_call_param_t)this);
    _clearFeatureEndpointHaltThread = thread_call_allocate((thread_call_func_t)ClearFeatureEndpointHaltEntry, (thread_call_param_t)this);
    _HANDLE_REPORT_THREAD = thread_call_allocate((thread_call_func_t)HandleReportEntry, (thread_call_param_t)this);
    
    if ( !_deviceDeadCheckThread || !_clearFeatureEndpointHaltThread || !_HANDLE_REPORT_THREAD )
    {
        USBLog(1, "[%s]%p: could not allocate all thread functions", getName(), this);
        USBTrace( kUSBTHID,  kTPHIDStart, (uintptr_t)this, 0, 0, 7 );
        goto ErrorExit;
    }
    
    
	_INTERFACE_NUMBER = _interface->GetInterfaceNumber();
	
	// Check to see if we have a logging property
	//
	propertyObj = provider->copyProperty(kUSBHIDReportLoggingLevel);
	numberObj = OSDynamicCast( OSNumber, propertyObj);
    if ( numberObj )
    {
		_HID_LOGGING_LEVEL = numberObj->unsigned32BitValue();
		_LOG_HID_REPORTS = true;
		USBLog(5, "IOUSBHIDDriver[%p](Intfce: %d of device %s @ 0x%x)::start  HID Report Logging at level %d", this, _INTERFACE_NUMBER, _device->getName(), (uint32_t)_locationID, _HID_LOGGING_LEVEL);
    }
	else
	{
		_HID_LOGGING_LEVEL = 7;
		_LOG_HID_REPORTS = false;
	}
	if (propertyObj)
	{
		propertyObj->release();
		propertyObj = NULL;
	}

	// Do the final processing for the "start" method.  This allows subclasses to get called right before we return from
    // the start
    //
    err = StartFinalProcessing();
    if (err != kIOReturnSuccess)
    {
        USBLog(1, "IOUSBHIDDriver(%s)[%p]::start - err (%x) in StartFinalProcessing", getName(), this, err);
        USBTrace( kUSBTHID,  kTPHIDStart, (uintptr_t)this, 0, 0, 8 );
        goto ErrorExit;
    }

    USBLog(1, "[%p] USB HID Interface #%d of device %s @ %d (0x%x)",this, _interface->GetInterfaceNumber(), _device->getName(), _device->GetAddress(), (uint32_t)_locationID );
	USBTrace( kUSBTHID,  kTPHIDStart, _interface->GetInterfaceNumber(), _device->GetAddress(), (uint32_t)_locationID, 0);

    // Now that we have succesfully added our gate to the workloop, set our member variables
    //
    _gate = commandGate;
    _WORKLOOP = workLoop;

	err = InitializeUSBHIDPowerManagement(provider);
	
	if (err)
	{
		USBLog(1, "IOUSBHIDDriver(%s)[%p]::start - err [%p] from InitializeUSBHIDPowerManagement", getName(), this, (void*)err);
		USBTrace( kUSBTHID,  kTPHIDStart, (uintptr_t)this, err, 0, 10);
	}
				
	USBTrace_End( kUSBTHID, kTPHIDStart, (uintptr_t)this, 0, 0, 0);
	
    DecrementOutstandingIO();		// release the hold we put on at the beginning
	
    return true;

ErrorExit:

    USBLog(1, "IOUSBHIDDriver(%s)[%p]::start - @ 0x%x aborting startup", getName(), this, (uint32_t)_locationID);
	USBTrace( kUSBTHID,  kTPHIDStart, (uintptr_t)this, (uint32_t)_locationID, 0, 9 );

    if ( commandGate != NULL )
    {
        if ( addEventSourceSuccess )
            workLoop->removeEventSource(commandGate);

        commandGate->release();
        commandGate = NULL;
    }

    if ( workLoop != NULL )
    {
        workLoop->release();
        workLoop = NULL;
    }

    if (_deviceDeadCheckThread)
        thread_call_free(_deviceDeadCheckThread);

    if (_clearFeatureEndpointHaltThread)
        thread_call_free(_clearFeatureEndpointHaltThread);

    if (_HANDLE_REPORT_THREAD)
        thread_call_free(_HANDLE_REPORT_THREAD);

    if (_interruptPipe)
	{
		_interruptPipe->release();
		_interruptPipe = NULL;
	}
	
    if (_interface)
	{
        _interface->close(this);
		
		// Release the retain done in handleStart();
		_interface->release();
		_interface = NULL;
	}
	
    if (_device)
	{
		// Release the retain done in handleStart();
		_device->release();
		_device = NULL;
	}
	
    DecrementOutstandingIO();		// release the hold we put on at the beginning
    return false;
}


//=============================================================================================
//
//  message
//
//=============================================================================================
//
IOReturn
IOUSBHIDDriver::message( UInt32 type, IOService * provider,  void * argument )
{
    IOReturn	err;

    // Call our superclass to the handle the message first
    //
    err = super::message (type, provider, argument);

    switch ( type )
    {
        case kIOUSBMessagePortHasBeenReset:
            
            USBLog(3, "IOUSBHIDDriver(%s)[%p]: received kIOUSBMessagePortHasBeenReset, checking idle and rearming interrupt read", getName(), this);
            
			if ( _retryCount != 0 )
			{
				USBLog(5, "IOUSBHIDDriver(%s)[%p]: received kIOUSBMessagePortHasBeenReset, we did not initiate this reset, so abort our interrupt pipe", getName(), this);
				ABORTEXPECTED = TRUE;
				if (_interruptPipe)
				{
					_interruptPipe->Abort();
				}
			}
			
            _retryCount = kHIDDriverRetryCount;
            ABORTEXPECTED = FALSE;
            _deviceHasBeenDisconnected = FALSE;
			
			// Redo any initialization that we did at boot time
			
			if ( _interface && _device )
			{
				// For Keyboards, set the idle millecs to 24 or to 0 if from Apple
				//
				if ( (_interface->GetInterfaceClass() == kUSBHIDClass) &&
					 (_interface->GetInterfaceSubClass() == kUSBHIDBootInterfaceSubClass) &&
					 (_interface->GetInterfaceProtocol() == kHIDKeyboardInterfaceProtocol) )
				{
					if (_device->GetVendorID() == kIOUSBVendorIDAppleComputer)
					{
						SetIdleMillisecs(0);
					}
					else
					{
						SetIdleMillisecs(24);
					}
				}
				
				// Set the device into Report Protocol if it's a bootInterface subClass interface
				//
				if (_interface->GetInterfaceSubClass() == kUSBHIDBootInterfaceSubClass)
					err = SetProtocol( kHIDReportProtocolValue );
				
				// Errata for ALL Saitek devices.  Do a SET_IDLE 0 call
				//
				if ( (_device->GetVendorID()) == 0x06a3 )
					SetIdleMillisecs(0);
			}
				
			// Finally, rearm our read
			err = RearmInterruptRead();
			break;
			
        case kIOUSBMessagePortHasBeenSuspended:
            
            USBLog(3, "IOUSBHIDDriver(%s)[%p]: received kIOUSBMessagePortHasBeenSuspended", getName(), this);
 			_PORT_SUSPENDED = true;
			break;
			
        case kIOUSBMessagePortHasBeenResumed:
		case kIOUSBMessagePortWasNotSuspended:
			
            USBLog(3, "IOUSBHIDDriver(%s)[%p]: received message %s (0x%x), rearming interrupt read", getName(), this, type == kIOUSBMessagePortHasBeenResumed ? "kIOUSBMessagePortHasBeenResumed": "kIOUSBMessagePortWasNotSuspended", (uint32_t)type);
            
			_PORT_SUSPENDED = FALSE;
            ABORTEXPECTED = FALSE;
			
            err = RearmInterruptRead();
			
			// Re-enable the timer
			if ( _SUSPENDPORT_TIMER )
			{
				USBLog(5, "IOUSBHIDDriver(%s)[%p]::message  re-enabling the timer", getName(), this);
				
				// Now, set it again
				_SUSPENDPORT_TIMER->setTimeoutMS(_SUSPEND_TIMEOUT_IN_MS);
			}
				
				break;
			
        default:
            break;
    }

    return err;
}


//=============================================================================================
//
//  willTerminate
//
//=============================================================================================
//
bool
IOUSBHIDDriver::willTerminate( IOService * provider, IOOptionBits options )
{
    // this method is intended to be used to stop any pending I/O and to make sure that
    // we have begun getting our callbacks in order. by the time we get here, the
    // isInactive flag is set, so we really are marked as being done. we will do in here
    // what we used to do in the message method (this happens first)
    //
    USBLog(3, "IOUSBHIDDriver(%s)[%p]::willTerminate isInactive (%d) _outstandingIO(%d)", getName(), this, isInactive(), (int)_outstandingIO);
    
	if (_outstandingIO)
	{
		if (_interruptPipe)
		{
			_interruptPipe->Abort();
		}

		// Cancel our suspend Timer if it exists
		if ( _SUSPENDPORT_TIMER )
		{
			_SUSPENDPORT_TIMER->cancelTimeout();
		}
	}
    return super::willTerminate(provider, options);
}



//=============================================================================================
//
//  didTerminate
//
//=============================================================================================
//
bool
IOUSBHIDDriver::didTerminate( IOService * provider, IOOptionBits options, bool * defer )
{
    // this method comes at the end of the termination sequence. Hopefully, all of our outstanding IO is complete
    // in which case we can just close our provider and IOKit will take care of the rest. Otherwise, we need to
    // hold on to the device and IOKit will terminate us when we close it later
    //
    USBLog(3, "IOUSBHIDDriver(%s)[%p]::didTerminate isInactive = %d, outstandingIO = %d", getName(), this, isInactive(), (uint32_t)_outstandingIO);

    PMstop();								// disconnect from the IOPower tree
	
    if (!_outstandingIO)
	{
        _interface->close(this);
	}
    else
        _needToClose = true;

    return super::didTerminate(provider, options, defer);
}



#pragma mark ееееееее Power Manager Methods еееееееее
unsigned long 
IOUSBHIDDriver::maxCapabilityForDomainState ( IOPMPowerFlags domainState )
{
	unsigned long ret = super::maxCapabilityForDomainState(domainState);
	if (isInactive())
	{
		USBLog(2, "IOUSBHIDDriver[%p]::maxCapabilityForDomainState - while inactive - ignoring", this);
		return ret;
	}

	USBLog(5, "IOUSBHIDDriver(%s)[%p]::maxCapabilityForDomainState - domainState[%d] - returning[%d]", getName(), this, (int)domainState, (int)ret);
	return ret;
}



IOReturn
IOUSBHIDDriver::powerStateWillChangeTo ( IOPMPowerFlags capabilities, unsigned long stateNumber, IOService* whatDevice)
{
	USBTrace_Start( kUSBTHID, kTPHIDpowerStateWillChangeTo, (uintptr_t)this, capabilities, stateNumber, (uintptr_t)whatDevice );
	
	if (isInactive())
	{
		USBLog(1, "IOUSBHIDDriver[%p]::powerStateWillChangeTo - while inactive - ignoring", this);
		USBTrace( kUSBTHID,  kTPHIDpowerStateWillChangeTo, (uintptr_t)this, IOPMAckImplied, 0, 1 );
		return IOPMAckImplied;
	}

	USBLog(5, "IOUSBHIDDriver(%s)[%p]::powerStateWillChangeTo - capabilities[%p] stateNumber[%d] whatDevice[%p]", getName(), this, (void*)capabilities, (int)stateNumber, whatDevice);

	if (whatDevice != this)
	{
		USBLog(5, "IOUSBHIDDriver(%s)[%p]::powerStateWillChangeTo - whatDevice[%p] is not me - ignoring", getName(), this, whatDevice);
		return IOPMAckImplied;
	}

	if (stateNumber != _MYPOWERSTATE)
		_POWERSTATECHANGING = true;

	USBLog(5, "IOUSBHIDDriver(%s)[%p]::powerStateWillChangeTo state (%d) - returning (%p)", getName(), this, (int)stateNumber, (void*)IOPMAckImplied);
	
	USBTrace_End( kUSBTHID, kTPHIDpowerStateWillChangeTo, (uintptr_t)this, stateNumber, IOPMAckImplied, 0);
	
	return IOPMAckImplied;
}



IOReturn
IOUSBHIDDriver::setPowerState ( unsigned long powerStateOrdinal, IOService* whatDevice )
{
	USBTrace_Start( kUSBTHID, kTPHIDsetPowerState, (uintptr_t)this, powerStateOrdinal, (uintptr_t)whatDevice, 0);
	
	if ( isInactive() )
	{
		USBLog(3,"IOUSBHIDDriver[%p]::setPowerState - while inactive - ignoring", this);
		return kIOPMAckImplied;
	}
	
	// at the moment, we don't do anything in the setPowerState. However, I want to log it for the time being..
	USBLog(5, "IOUSBHIDDriver(%s)[%p]::setPowerState - powerStateOrdinal[%d] whatDevice[%p] _device[%p](%s)", getName(), this, (int)powerStateOrdinal, whatDevice, _device, _device->getName());

	if ( whatDevice != this )
	{
		USBLog(1,"IOUSBHIDDriver[%p]::setPowerState - whatDevice != this", this);
		USBTrace( kUSBTHID,  kTPHIDsetPowerState, (uintptr_t)this, 0, 0, 1 );
		return kIOPMAckImplied;
	}
	
	if (powerStateOrdinal > kUSBHIDPowerStateOn)
	{
		USBLog(1,"IOUSBHIDDriver[%p]::setPowerState - bad ordinal(%d)", this, (int)powerStateOrdinal);
		USBTrace( kUSBTHID,  kTPHIDsetPowerState, (uintptr_t)this, powerStateOrdinal, 0, 2 );
		return kIOPMNoSuchState;
	}
	
	if (_MYPOWERSTATE == powerStateOrdinal)
	{
		USBLog(5,"IOUSBHIDDriver[%p]::setPowerState - already in correct power state (%d) - no op", this, (int)_MYPOWERSTATE);
		return kIOPMAckImplied;
	}		

	// if we are switching to the suspend state, we will cancel our timer and abort the interrupt pipe
	if (powerStateOrdinal < kUSBHIDPowerStateOn)
	{
		
		// now kill my interrupt pipe, since my upstream hub will suspend me
		USBLog(5, "IOUSBHIDDriver(%s)[%p]::setPowerState state(%d) -  aborting pipe (%p)", getName(), this, (int)powerStateOrdinal, _interruptPipe);
		if ( _interruptPipe )
		{
			ABORTEXPECTED = true;
			_interruptPipe->Abort();
		}
		
		USBLog(5, "IOUSBHIDDriver(%s)[%p]::setPowerState state (%d)- cancelling timer (%p)", getName(), this, (int)powerStateOrdinal, _SUSPENDPORT_TIMER);
		if ( _SUSPENDPORT_TIMER )
		{
			_SUSPENDPORT_TIMER->cancelTimeout();
		}
	}

	switch (powerStateOrdinal)
	{
		case kUSBHIDPowerStateOn:
		case kUSBHIDPowerStateLowPower:
		case kUSBHIDPowerStateSleep:
		case kUSBHIDPowerStateOff:
		case kUSBHIDPowerStateRestart:
			break;
		
		default:
			USBLog(1, "IOUSBHIDDriver(%s)[%p]::setPowerState - unknown ordinal (%d)", getName(), this, (int)powerStateOrdinal);
			USBTrace( kUSBTHID,  kTPHIDsetPowerState, (uintptr_t)this, powerStateOrdinal, 0, 3 );
	}
	
	USBTrace_End( kUSBTHID, kTPHIDsetPowerState, (uintptr_t)this, 0, 0, 0);
	
	return IOPMAckImplied;
}



IOReturn
IOUSBHIDDriver::powerStateDidChangeTo ( IOPMPowerFlags capabilities, unsigned long stateNumber, IOService* whatDevice)
{
	IOReturn		err;

	USBTrace_Start( kUSBTHID, kTPHIDpowerStateDidChangeTo, (uintptr_t)this, capabilities, stateNumber, (uintptr_t)whatDevice );
	
	if ( isInactive() )
	{
		USBLog(3,"IOUSBHIDDriver[%p]::powerStateDidChangeTo - while inactive - ignoring", this);
		return kIOPMAckImplied;
	}
	
	if (whatDevice != this)
	{
		USBLog(5, "IOUSBHIDDriver(%s)[%p]::powerStateDidChangeTo - whatDevice[%p] is not me - ignoring", getName(), this, whatDevice);
		return IOPMAckImplied;
	}

	_MYPOWERSTATE = stateNumber;

	// if we are unsuspending, it is time to issue the interrupt read and start the idle timer
	if (stateNumber == kUSBHIDPowerStateOn)
	{
		if (!_QUEUED_REPORTS)
		{
			USBLog(5, "IOUSBHIDDriver(%s)[%p]::powerStateDidChangeTo - _device (%s) going into RUN mode - issuing read and starting timer", getName(), this, _device->getName());
			ABORTEXPECTED = false;
			err = RearmInterruptRead();
			if (err)
			{
				USBLog(err != kIOReturnNoBandwidth ? 1 : 3, "IOUSBHIDDriver(%s)[%p]::powerStateDidChangeTo - err (%p) returned from RearmInterruptRead", getName(), this, (void*)err);
				USBTrace( kUSBTHID,  kTPHIDpowerStateDidChangeTo, (uintptr_t)this, err, 0, 0);
			}
			if ( _SUSPENDPORT_TIMER )
			{
				_SUSPENDPORT_TIMER->setTimeoutMS(_SUSPEND_TIMEOUT_IN_MS);
			}
		}
		else
		{
			USBLog(2, "IOUSBHIDDriver(%s)[%p]::powerStateDidChangeTo(ON) - not ReArming because _QUEUED_REPORTS was >0", getName(), this);			
		}
	}
	
	USBTrace_End( kUSBTHID, kTPHIDpowerStateDidChangeTo, (uintptr_t)this, 0, 0, 0);

	return IOPMAckImplied;
}



void
IOUSBHIDDriver::powerChangeDone ( unsigned long fromState)
{
	USBTrace_Start( kUSBTHID, kTPHIDpowerChangeDone, (uintptr_t)this, fromState, 0, 0);
	
	if (isInactive())
	{
		USBLog(1, "IOUSBHIDDriver[%p]::powerChangeDone - from state (%d)  - ignoring", this, (int)fromState);
		USBTrace( kUSBTHID,  kTPHIDpowerChangeDone, (uintptr_t)this, fromState, 0, 0);
		return;
	}

	USBTrace_End( kUSBTHID, kTPHIDpowerChangeDone, (uintptr_t)this, 0, 0, 0);
	USBLog((fromState == _MYPOWERSTATE) ? 7 : 5, "IOUSBHIDDriver[%p]::powerChangeDone from state (%d) to state (%d) _device name(%s)", this, (int)fromState, (int)_MYPOWERSTATE, _device->getName());
	_POWERSTATECHANGING = false;
	super::powerChangeDone(fromState);
}


#pragma mark ееееееее IOHIDDevice Methods еееееееее
//================================================================================================
//
//  handleStart
//
//  Note: handleStart is not an IOKit thing, but is a IOHIDDevice thing. It is called from 
//  IOHIDDevice::start after some initialization by that method, but before it calls registerService
//  this method needs to open the provider, and make sure to have enough state (basically _interface
//  and _device) to be able to get information from the device. we do NOT need to start the interrupt read
//  yet, however
//
//================================================================================================
//
bool 
IOUSBHIDDriver::handleStart(IOService * provider)
{
	IOUSBFindEndpointRequest	request;

	USBLog(6, "IOUSBHIDDriver(%s)[%p]::handleStart", getName(), this);
	USBTrace_Start( kUSBTHID, kTPHIDhandleStart, (uintptr_t)this, (uintptr_t)provider, 0, 0);

    if ( !super::handleStart(provider))
    {
        return false;
    }

    // Open our provider so that nobody else an gain access to it
    //
    if ( !provider->open(this))
    {
        USBLog(1, "IOUSBHIDDriver(%s)[%p]::handleStart - unable to open provider. returning false", getName(), this);
		USBTrace( kUSBTHID,  kTPHIDhandleStart, (uintptr_t)this, 0, 0, 1 );
        return (false);
    }

    _interface = OSDynamicCast(IOUSBInterface, provider);
    if (!_interface)
    {
        USBLog(1, "IOUSBHIDDriver(%s)[%p]::handleStart - Our provider is not an IOUSBInterface!!", getName(), this);
		USBTrace( kUSBTHID,  kTPHIDhandleStart, (uintptr_t)this, 0, 0, 2 );
        return false;
    }
	
    _device = _interface->GetDevice();
    if (!_device)
    {
        USBLog(1, "IOUSBHIDDriver(%s)[%p]::handleStart - Cannot get our provider's USB device.  This is bad.", getName(), this);
		USBTrace( kUSBTHID,  kTPHIDhandleStart, (uintptr_t)this, 0, 0, 3 );
        return false;
    }

	// Retain the _interface and _device objects and release them in free()
	_interface->retain();
	_device->retain();
	
    // Now, find our interrupt out pipe and interrupt in pipes
    //
    request.type = kUSBInterrupt;
    request.direction = kUSBOut;
    _interruptOutPipe = _interface->FindNextPipe(NULL, &request);
	
    request.type = kUSBInterrupt;
    request.direction = kUSBIn;
    _interruptPipe = _interface->FindNextPipe(NULL, &request);
	
    if (!_interruptPipe)
    {
        USBLog(1, "IOUSBHIDDriver(%s)[%p]::start - unable to get interrupt pipe", getName(), this);
        USBTrace( kUSBTHID,  kTPHIDStart, (uintptr_t)this, 0, 0, 5 );
        goto ErrorExit;
    }
	
	_interruptPipe->retain();
	
	USBTrace_End( kUSBTHID, kTPHIDhandleStart, (uintptr_t)this, (uintptr_t)_interface, (uintptr_t)_device, 0);
    return true;
	
ErrorExit:
	
	return false;
}


//================================================================================================
//
//  handleStop
//
//  Note: handleStop is not an IOKit thing, but is a IOHIDDevice thing.
//
//================================================================================================
//
void
IOUSBHIDDriver::handleStop(IOService * provider)
{
    USBLog(7, "IOUSBHIDDriver(%s)[%p]::handleStop", getName(), this);

    if (_deviceDeadCheckThread)
    {
        thread_call_cancel(_deviceDeadCheckThread);
        thread_call_free(_deviceDeadCheckThread);
    }

    if (_clearFeatureEndpointHaltThread)
    {
        thread_call_cancel(_clearFeatureEndpointHaltThread);
        thread_call_free(_clearFeatureEndpointHaltThread);
    }
    if (_HANDLE_REPORT_THREAD)
    {
        thread_call_cancel(_HANDLE_REPORT_THREAD);
        thread_call_free(_HANDLE_REPORT_THREAD);
    }
    
	super::handleStop(provider);
}


//================================================================================================
//
//  getReport
//
//================================================================================================
//
IOReturn 
IOUSBHIDDriver::getReport(	IOMemoryDescriptor * report,
                                IOHIDReportType      reportType,
                                IOOptionBits         options )
{
    UInt8		reportID;
    IOReturn		ret;
    UInt8		usbReportType;
    IOUSBDevRequestDesc requestPB;

    // The following should really be an errata bit.  We will need to add that later.  For now
    // hardcode the check.  Some Logitech devices do not respond well to a GET_REPORT, so we need
    // to return unsupported for them.
    //
    if ( _device->GetVendorID() == 0x046d )
    {
        UInt16	prodID = _device->GetProductID();

        if ( (prodID == 0xc202) || (prodID == 0xc207) || (prodID == 0xc208) ||
             (prodID == 0xc209) || (prodID == 0xc20a) || (prodID == 0xc212) || 
             (prodID == 0xc285) || (prodID == 0xc293) || (prodID == 0xc294) ||
             (prodID == 0xc295) || (prodID == 0xc283) )
        {
            return kIOReturnUnsupported;
        }
    }
    
    IncrementOutstandingIO();

    // Get the reportID from the lower 8 bits of options
    //
    reportID = (UInt8) ( options & 0x000000ff);

    // And now save the report type
    //
    usbReportType = HIDMGR2USBREPORTTYPE(reportType);

    //--- Fill out device request form
    //
    requestPB.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBClass, kUSBInterface);
    requestPB.bRequest = kHIDRqGetReport;
    requestPB.wValue = (usbReportType << 8) | reportID;
    requestPB.wIndex = _interface->GetInterfaceNumber();
    requestPB.wLength = report->getLength();
    requestPB.pData = report;
    requestPB.wLenDone = 0;
    
    ret = _device->DeviceRequest(&requestPB);
    if ( ret != kIOReturnSuccess )
	{
            USBLog(3, "IOUSBHIDDriver(%s)[%p]::getReport request failed; err = 0x%x)", getName(), this, ret);
	}
           
	// 9196402:  If we get an IOBMD passed in, set the length to be the # of bytes that were transferred
	IOBufferMemoryDescriptor * buffer = OSDynamicCast(IOBufferMemoryDescriptor, report);
	if (buffer)
	{
		USBLog(5, "IOUSBHIDDriver(%s)[%p]::getReport we have an IOBufferMemoryDescriptor, so set the length to wLenDone of %d", getName(), this, (int)requestPB.wLenDone);
		buffer->setLength((vm_size_t)requestPB.wLenDone);
	}
	
    DecrementOutstandingIO();

	if ( _LOG_HID_REPORTS )
	{
		USBLog(_HID_LOGGING_LEVEL, "IOUSBHIDDriver[%p](Intfce: %d of device %s @ 0x%x)::getReport(%d, type = %s) returned success:", this, _INTERFACE_NUMBER, _device->getName(), (uint32_t)_locationID, reportID, 
			   usbReportType == 1 ? "input" : (usbReportType == 2 ? "output" : (usbReportType == 3 ? "feature" : "unknown")) );
		LogMemReport(_HID_LOGGING_LEVEL, report, report->getLength());
	}

	return ret;
}


//=============================================================================================
//
//  setReport
//
//=============================================================================================
//
IOReturn 
IOUSBHIDDriver::setReport( IOMemoryDescriptor * 	report,
                            IOHIDReportType      	reportType,
                            IOOptionBits         	options)
{
    UInt8		reportID;
    IOReturn		ret;
    UInt8		usbReportType;
    IOUSBDevRequestDesc requestPB;
    
    IncrementOutstandingIO();
    
    // Get the reportID from the lower 8 bits of options
    //
    reportID = (UInt8) ( options & 0x000000ff);

    // And now save the report type
    //
    usbReportType = HIDMGR2USBREPORTTYPE(reportType);
    
    // If we have an interrupt out pipe, try to use it for output type of reports.
    if ( kHIDOutputReport == usbReportType && _interruptOutPipe )
    {
		if ( _LOG_HID_REPORTS )
		{
            USBLog(_HID_LOGGING_LEVEL, "IOUSBHIDDriver[%p](Intfce: %d of device %s @ 0x%x)::setReport sending out interrupt out pipe buffer (%p,%d):", this, _INTERFACE_NUMBER, _device->getName(), (uint32_t)_locationID, report, (uint32_t)report->getLength() );
            LogMemReport(_HID_LOGGING_LEVEL, report, report->getLength());
        }

		ret = _interruptOutPipe->Write(report);
        if (ret == kIOReturnSuccess)
        {       
            DecrementOutstandingIO();
            return ret;
        }
        else
        {
            USBLog(3, "IOUSBHIDDriver(%s)[%p]::setReport _interruptOutPipe->Write failed; err = 0x%x)", getName(), this, ret);
        }
    }
        
    // If we did not succeed using the interrupt out pipe, we may still be able to use the control pipe.
    // We'll let the family check whether it's a disjoint descriptor or not (but right now it doesn't do it)
    //
	if ( _LOG_HID_REPORTS )
	{
        USBLog(_HID_LOGGING_LEVEL, "IOUSBHIDDriver[%p](Intfce: %d of device %s @ 0x%x)::SetReport sending out control pipe:", this, _INTERFACE_NUMBER, _device->getName(), (uint32_t)_locationID);
        LogMemReport(_HID_LOGGING_LEVEL,  report, report->getLength());
	}
	
    //--- Fill out device request form
    requestPB.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface);
    requestPB.bRequest = kHIDRqSetReport;
    requestPB.wValue = (usbReportType << 8) | reportID;
    requestPB.wIndex = _interface->GetInterfaceNumber();
    requestPB.wLength = report->getLength();
    requestPB.pData = report;
    requestPB.wLenDone = 0;
    
    ret = _device->DeviceRequest(&requestPB);
    if (ret != kIOReturnSuccess)
	{
            USBLog(3, "IOUSBHIDDriver(%s)[%p]::setReport request failed; err = 0x%x)", getName(), this, ret);
	}
        
    DecrementOutstandingIO();
    USBLog(_HID_LOGGING_LEVEL, "IOUSBHIDDriver[%p](Intfce: %d of device %s @ 0x%x)::setReport returning 0x%x", this, _INTERFACE_NUMBER, _device->getName(), (uint32_t)_locationID, (uint32_t)ret);
    
	return ret;
}


//=============================================================================================
//
//  newLocationIDNumber
//
//=============================================================================================
//
OSNumber *
IOUSBHIDDriver::newLocationIDNumber() const
{
    OSNumber *	newLocationID = NULL;
	OSNumber *	locationID = NULL;
    OSObject *	propertyObj = NULL;

    if (_interface != NULL)
    {
		propertyObj = _interface->copyProperty(kUSBDevicePropertyLocationID);
		locationID = OSDynamicCast( OSNumber, propertyObj);
        if ( locationID )
		{
            // I should be able to just duplicate locationID, but no OSObject::clone() or such.
            newLocationID = OSNumber::withNumber(locationID->unsigned32BitValue(), 32);
		}
		if (propertyObj)
			propertyObj->release();
    }

    return newLocationID;
}


//=============================================================================================
//
//  newManufacturerString
//
//=============================================================================================
//
OSString *
IOUSBHIDDriver::newManufacturerString() const
{
    char 	manufacturerString[256];
    UInt32 	strSize;
    UInt8 	index;
    IOReturn	err;

    manufacturerString[0] = 0;

    index = _device->GetManufacturerStringIndex();
    strSize = sizeof(manufacturerString);

    err = GetIndexedString(index, (UInt8 *)manufacturerString, &strSize);

    if ( err == kIOReturnSuccess )
        return OSString::withCString(manufacturerString);
    else
        return NULL;
}


//=============================================================================================
//
//  newProductIDNumber
//
//=============================================================================================
//
OSNumber *
IOUSBHIDDriver::newProductIDNumber() const
{
    UInt16 productID = 0;

    if (_device != NULL)
        productID = _device->GetProductID();

    return OSNumber::withNumber(productID, 16);
}


//=============================================================================================
//
//  newProductString
//
//=============================================================================================
//
OSString *
IOUSBHIDDriver::newProductString() const
{
    char 	productString[256];
    UInt32 	strSize;
    UInt8 	index;
    IOReturn	err;

    productString[0] = 0;

    index = _device->GetProductStringIndex();
    strSize = sizeof(productString);

    err = GetIndexedString(index, (UInt8 *)productString, &strSize);

    if ( err == kIOReturnSuccess )
        return OSString::withCString(productString);
    else
        return NULL;
}


//=============================================================================================
//
//  newReportDescriptor
//
//=============================================================================================
//
IOReturn
IOUSBHIDDriver::newReportDescriptor(IOMemoryDescriptor ** desc) const
{
    IOBufferMemoryDescriptor * 	bufferDesc = NULL;
    IOReturn ret = 		kIOReturnNoMemory;
    IOUSBHIDDriver * 		me = (IOUSBHIDDriver *) this;

    // Get the proper HID report descriptor size.
    //
    UInt32 inOutSize = 0;
    ret = me->GetHIDDescriptor(kUSBReportDesc, 0, NULL, &inOutSize);

    if ( ret == kIOReturnSuccess && inOutSize != 0)
    {
        bufferDesc = IOBufferMemoryDescriptor::withCapacity(inOutSize, kIODirectionOutIn);
    }

    if (bufferDesc)
    {
        ret = me->GetHIDDescriptor(kUSBReportDesc, 0, (UInt8 *)bufferDesc->getBytesNoCopy(), &inOutSize);

        if ( ret != kIOReturnSuccess )
        {
            bufferDesc->release();
            bufferDesc = NULL;
        }
    }

    *desc = bufferDesc;

    return ret;
}


//=============================================================================================
//
//  newSerialNumberString
//
//=============================================================================================
//
OSString * 
IOUSBHIDDriver::newSerialNumberString() const
{
    char 	serialNumberString[256];
    UInt32 	strSize;
    UInt8 	index;
    IOReturn	err;
    
    serialNumberString[0] = 0;

    index = _device->GetSerialNumberStringIndex();
    strSize = sizeof(serialNumberString);
    
    err = GetIndexedString(index, (UInt8 *)serialNumberString, &strSize);
    
    if ( err == kIOReturnSuccess )
        return OSString::withCString(serialNumberString);
    else
        return NULL;
}


//=============================================================================================
//
//  newTransportString
//
//=============================================================================================
//
OSString *
IOUSBHIDDriver::newTransportString() const
{
    return OSString::withCString("USB");
}

//=============================================================================================
//
//  newVendorIDNumber
//
//=============================================================================================
//
OSNumber *
IOUSBHIDDriver::newVendorIDNumber() const
{
    UInt16 vendorID = 0;

    if (_device != NULL)
        vendorID = _device->GetVendorID();

	USBLog(6, "IOUSBHIDDriver(%s)[%p]::newVendorIDNumber -returning %d", getName(), this, (uint32_t)vendorID);
    return OSNumber::withNumber(vendorID, 16);
}


//=============================================================================================
//
//  newVersionNumber
//
//=============================================================================================
//
OSNumber *
IOUSBHIDDriver::newVersionNumber() const
{
    UInt16 releaseNum = 0;

    if (_device != NULL)
        releaseNum = _device->GetDeviceRelease();

	USBLog(6, "IOUSBHIDDriver(%s)[%p]::newVersionNumber -returning %d", getName(), this, (uint32_t)releaseNum);
	return OSNumber::withNumber(releaseNum, 16);
}


//=============================================================================================
//
//  newCountryCodeNumber
//
//=============================================================================================
//
OSNumber *
IOUSBHIDDriver::newCountryCodeNumber() const
{
    IOUSBHIDDescriptor 		*theHIDDesc;
    
    if (!_interface)
    {
        USBLog(2, "IOUSBHIDDriver(%s)[%p]::newCountryCodeNumber - no _interface", getName(), this);
        return NULL;
    }
    
    // From the interface descriptor, get the HID descriptor.
    theHIDDesc = (IOUSBHIDDescriptor *)_interface->FindNextAssociatedDescriptor(NULL, kUSBHIDDesc);
    
    if (theHIDDesc == NULL)
    {
        USBLog(2, "IOUSBHIDDriver(%s)[%p]::newCountryCodeNumber - FindNextAssociatedDescriptor(NULL, kUSBHIDDesc) failed", getName(), this);
        return NULL;
    }
    
	USBLog(6, "IOUSBHIDDriver(%s)[%p]::newCountryCodeNumber - returning 0x%d", getName(), this, (uint32_t)theHIDDesc->hidCountryCode);
    return OSNumber::withNumber((unsigned long long)theHIDDesc->hidCountryCode, 8);
}


//=============================================================================================
//
//  newReportIntervalNumber
//
//=============================================================================================
//
OSNumber *
IOUSBHIDDriver::newReportIntervalNumber() const
{
	UInt8	interval = 8;
	
	if ( _interruptPipe )
	{
		UInt8	pollingRate;
		
		// The problem with the GetInterval() is that this is the descriptors polling rate.  The USB controller is free to change the polling interval to whatever they
		// like as long as it's less than the descriptor's rate. 
		pollingRate = _interruptPipe->GetInterval();
		
		// We assume that a controller will actually only poll at powers of 2 and that it will not poll any slower than 32 ms.
		
		if (pollingRate >= 32)		{ interval = 32; }
		else if (pollingRate >= 16) { interval = 16; }
		else if (pollingRate >= 8)	{ interval = 8; }
		else if (pollingRate >= 4)	{ interval = 4; }
		else if (pollingRate >= 2)	{ interval = 2; }
		else interval = 1;
		
		USBLog(5, "IOUSBHIDDriver(%s)[%p]::newReportIntervalNumber - _interval is %d", getName(), this, (uint32_t)interval);
	}
	else
	{
        USBLog(5, "IOUSBHIDDriver(%s)[%p]::newReportIntervalNumber - no _interruptPipe, returning 8", getName(), this);
	}

	
	return OSNumber::withNumber((unsigned long long)(interval*1000), 32);
}


#pragma mark ееееееее Static Methods еееееееее
//=============================================================================================
//
//  InterruptReadHandlerEntry is called to process any data coming in through our interrupt pipe
//
//=============================================================================================
//
void 
IOUSBHIDDriver::InterruptReadHandlerEntry(OSObject *target, void *param, IOReturn status, UInt32 bufferSizeRemaining)
{
#pragma unused (param)
    IOUSBHIDDriver *	me = OSDynamicCast(IOUSBHIDDriver, target);
    uint64_t			timeStamp;
    
    if (!me)
        return;
    
	// If we don't have a timestamp, we need to fake one up
	timeStamp = mach_absolute_time();

    me->InterruptReadHandler(status, bufferSizeRemaining, *(AbsoluteTime *)&timeStamp);
    me->DecrementOutstandingIO();
}

void 
IOUSBHIDDriver::InterruptReadHandlerWithTimeStampEntry(OSObject *target, void *param, IOReturn status, UInt32 bufferSizeRemaining, AbsoluteTime timeStamp)
{
#pragma unused (param, timeStamp)
   IOUSBHIDDriver *	me = OSDynamicCast(IOUSBHIDDriver, target);
    
    if (!me)
        return;
    
    me->InterruptReadHandler(status, bufferSizeRemaining, timeStamp);
    me->DecrementOutstandingIO();
}


void 
IOUSBHIDDriver::InterruptReadHandler(IOReturn status, UInt32 bufferSizeRemaining, AbsoluteTime timeStamp)
{
    bool			queueAnother = false;					// make the default to not queue another - since the callout threads usually do
    IOReturn		err = kIOReturnSuccess;
    
	_PENDINGREAD = false;
	// Save our timestamp, since we are going to callout to the HID manager
	_INTERRUPT_TIMESTAMP = timeStamp;
	
    // Calculate the # of milliseconds since we woke up.  If this is <= the amount specified in _msToIgnoreTransactionsAfterWake, then
    // we will ignore the transaction.
	//    UInt64			timeElapsed;
	//    uint64_t	timeStop;
    // timeStop = mach_absolute_time();
    //SUB_ABSOLUTETIME(&timeStop, &timeStamp);
    //absolutetime_to_nanoseconds(timeStop, &timeElapsed);
	//
    // USBLog(5,"IOUSBHIDDriver(%s)[%p]::InterruptReadHandler:  microsecs since filter interrupt:  %qd", getName(), this, timeElapsed / 1000);
    // USBLog(5,"IOUSBHIDDriver(%s)[%p]::InterruptReadHandler:  time.hi 0x%x time.lo 0x%x", getName(), this, timeStamp.hi, timeStamp.lo);
    // kprintf("IOUSBHIDDriver(%s)[%p]::InterruptReadHandler:  time.hi 0x%x time.lo 0x%x\n", getName(), this, timeStamp.hi, timeStamp.lo);

	USBLog(7, "IOUSBHIDDriver(%s)[%p]::InterruptReadHandler  bufferSizeRemaining: %d, error 0x%x", getName(), this, (uint32_t)bufferSizeRemaining, status);
		
	if ( status == kIOReturnSuccess )
	{
		vm_size_t	capacity = (_buffer ? _buffer->getCapacity() : 0);
		
		USBTrace( kUSBTHID,  kTPHIDInterruptRead, (uintptr_t)this, capacity - bufferSizeRemaining, capacity, 0);
	}
	else
	{
		USBTrace( kUSBTHID,  kTPHIDInterruptRead, (uintptr_t)this, (uintptr_t)status, _deviceHasBeenDisconnected, 1);
	}
	
	// Initialize so that we don't queue a clearFeatureEndpointHalt
	_NEED_TO_CLEARPIPESTALL = false;
	
    switch (status)
    {
        case kIOReturnOverrun:
            USBLog(3, "IOUSBHIDDriver(%s)[%p]::InterruptReadHandler kIOReturnOverrun error", getName(), this);
            // This is an interesting error, as we have the data that we wanted and more...  We will use this
            // data but first we need to clear the stall and reset the data toggle on the device. We then just 
            // fall through to the kIOReturnSuccess case.
            // 01-18-02 JRH If we are inactive, then ignore this
			if (!isInactive() && _interruptPipe)
            {
                //
                // First, clear the halted bit in the controller
                //
                _interruptPipe->ClearStall();
				
				// We will need to clear the stall
				_NEED_TO_CLEARPIPESTALL = true;
            }
            // Fall through to process the data.
            
        case kIOReturnSuccess:
            // Reset the retry count, since we had a successful read
            //
            _retryCount = kHIDDriverRetryCount;
			_deviceHasBeenDisconnected = FALSE;
			
			// If we got a "short" transfer, adjust the length of the buffer so we don't send stale data to the HID family.
			// We will reset the length before rearming the read
			if ( bufferSizeRemaining != 0 )
			{
				_buffer->setLength(_buffer->getCapacity()-bufferSizeRemaining);
			}

            // Handle the data.  We do this on a callout thread so that we don't block all
            // of USB I/O if the HID system is blocked.  We only do that if we have less than than kMaxQueuedReports pending
            //
			if ( _QUEUED_REPORTS < kMaxQueuedReports)
			{
				// Do not call handle report if we have no new data in our buffer
				if ( bufferSizeRemaining != _buffer->getCapacity() )
				{
					// If thread_call_enter() returns TRUE, then a call is already
					// pending, and we need to drop our outstandingIO count.
					IncrementOutstandingIO();
					_QUEUED_REPORTS++;				// do this while we are still inside the gate
					_HANDLEREPORTTIMESTAMP = mach_absolute_time();

					if ( thread_call_enter1(_HANDLE_REPORT_THREAD, (thread_call_param_t) &_INTERRUPT_TIMESTAMP) == TRUE)
					{
						USBLog(3, "IOUSBHIDDriver(%s)[%p]::InterruptReadHandler  _HANDLE_REPORT_THREAD was already queued!", getName(), this);
						USBTrace( kUSBTHID,  kTPHIDInterruptRead, (uintptr_t)this, 0, 0, 3);
						DecrementOutstandingIO();
						_QUEUED_REPORTS--;
					}
				}
				else
				{
					queueAnother = true;
					USBTrace( kUSBTHID,  kTPHIDInterruptRead, (uintptr_t)this, 0, 0, 4);
				}
			}
			else
			{
				USBLog(2, "IOUSBHIDDriver(%s)[%p]::InterruptReadHandler  Not calling handleReport thread because we already have a report queued", getName(), this);
				USBTrace( kUSBTHID,  kTPHIDInterruptRead, (uintptr_t)this, 0, 0, 5);
			}
            break;
			
        case kIOReturnNotResponding:
		case kIOUSBHighSpeedSplitError:
            USBLog(3, "IOUSBHIDDriver(%s)[%p]::InterruptReadHandler 0x%x (%s)", getName(), this, status, USBStringFromReturn(status));
            // If our device has been disconnected or we're already processing a
            // terminate message, just go ahead and close the device (i.e. don't
            // queue another read.  Otherwise, go check to see if the device is
            // around or not. 
            //
			if ( IsPortSuspended() && _interruptPipe)
			{
				// If the port is suspended, then we can expect this.  Just ignore the error.
	            USBLog(4, "IOUSBHIDDriver(%s)[%p]::InterruptReadHandler kIOReturnNotResponding error but port is suspended", getName(), this);
				USBTrace( kUSBTHID,  kTPHIDInterruptRead, (uintptr_t)this, 0, 0, 6);
				
				// If the port is suspended, we don't need to re-arm the interrupt, but we still need to clear the stall.  
				_interruptPipe->ClearStall();				
			}
			else
			{	
				if ( !_deviceHasBeenDisconnected && !isInactive() && _interruptPipe)
				{
					USBLog(3, "IOUSBHIDDriver(%s)[%p]::InterruptReadHandler Checking to see if HID device is still connected", getName(), this);
					USBTrace( kUSBTHID,  kTPHIDInterruptRead, (uintptr_t)this, 0, 0, 7);
	
					// If thread_call_enter() returns TRUE, then a call is already
					// pending, and we need to drop our outstandingIO count.
					IncrementOutstandingIO();
					if ( thread_call_enter(_deviceDeadCheckThread) == TRUE )
					{
						USBLog(3, "IOUSBHIDDriver(%s)[%p]::InterruptReadHandler  _deviceDeadCheckThread was already queued!", getName(), this);
						DecrementOutstandingIO();
						USBTrace( kUSBTHID,  kTPHIDInterruptRead, (uintptr_t)this, 0, 0, 8);
					}
					
					// Before requeueing, we need to clear the stall
					//
					_interruptPipe->ClearStall();
					queueAnother = true;						// if the device is really dead, this request will get aborted
				}
			}
			break;
            
		case kIOReturnAborted:
			// This generally means that we are done, because we were unplugged, but not always
            //
            if (!isInactive() && !ABORTEXPECTED && _interruptPipe)
            {
                USBLog(3, "IOUSBHIDDriver(%s)[%p]::InterruptReadHandler error kIOReturnAborted. Try again.", getName(), this);
				USBTrace( kUSBTHID,  kTPHIDInterruptRead, (uintptr_t)this, 0, 0, 9);
				queueAnother = true;
            }
			else if ( ABORTEXPECTED )
			{
                USBLog(5, "IOUSBHIDDriver(%s)[%p]::InterruptReadHandler error kIOReturnAborted. Expected.  Not rearming interrupt", getName(), this);
				USBTrace( kUSBTHID,  kTPHIDInterruptRead, (uintptr_t)this, 0, 0, 10);
			}
			
			USBTrace( kUSBTHID,  kTPHIDInterruptReadError, (uintptr_t)this, ABORTEXPECTED, queueAnother, status);
			
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
            
            USBLog(3, "IOUSBHIDDriver(%s)[%p]::InterruptReadHandler error (0x%x) reading interrupt pipe", getName(), this, status);
            // 01-18-02 JRH If we are inactive, then ignore this
            if (!isInactive() && _interruptPipe)
            {
				USBTrace( kUSBTHID,  kTPHIDInterruptRead, (uintptr_t)this, 0, 0, 11);
                // First, clear the halted bit in the controller
                //
                _interruptPipe->ClearStall();
				
                // And call the device to reset the endpoint as well
                //
  
				// If thread_call_enter() returns TRUE, then a call is already
				// pending, and we need to drop our outstandingIO count.
				IncrementOutstandingIO();
                if ( thread_call_enter(_clearFeatureEndpointHaltThread) == TRUE )					// this will rearm the request when it is done
				{
					USBLog(3, "IOUSBHIDDriver(%s)[%p]::InterruptReadHandler  _clearFeatureEndpointHaltThread was already queued!", getName(), this);
					DecrementOutstandingIO();
					USBTrace( kUSBTHID,  kTPHIDInterruptRead, (uintptr_t)this, 0, 0, 12);
				}
            }
			break;
			
        default:
            // We should handle other errors more intelligently, but
            // for now just return and assume the error is recoverable.
            USBLog(3, "IOUSBHIDDriver(%s)[%p]::InterruptReadHandler Unknown error (0x%x) reading interrupt pipe", getName(), this, status);
			USBTrace( kUSBTHID,  kTPHIDInterruptRead, (uintptr_t)this, status, 0, 13);

			if ( !isInactive() && _interruptPipe )
                _interruptPipe->ClearStall();
			queueAnother = true;													// no callout to go to - rearm it now
			
			break;
    }

    if ( queueAnother )
    {
        // Queue up another one before we leave.
        //
		USBLog(7, "IOUSBHIDDriver(%s)[%p]::InterruptReadHandler - queueing another", getName(), this);
        (void) RearmInterruptRead();
    }
}


//=============================================================================================
//
//  CheckForDeadDevice
//
//  Is called when we get a kIODeviceNotResponding error in our interrupt pipe.
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
IOUSBHIDDriver::CheckForDeadDeviceEntry(OSObject *target)
{
    IOUSBHIDDriver *	me = OSDynamicCast(IOUSBHIDDriver, target);
    
    if (!me)
        return;
        
    me->CheckForDeadDevice();
    me->DecrementOutstandingIO();
}

void 
IOUSBHIDDriver::CheckForDeadDevice()
{
    IOReturn			err = kIOReturnSuccess;
	bool				gotLock;

	if (isInactive())
	{
		USBLog(4, "IOUSBHIDDriver(%s)[%p]::CheckForDeadDevice - called while inActive - ignoring", getName(), this);
		USBTrace( kUSBTHID,  kTPHIDCheckForDeadDevice, (uintptr_t)this, 0, 0, 12);
		return;
	}

 	gotLock = OSCompareAndSwap(0, 1, &_DEAD_DEVICE_CHECK_LOCK);
	if ( !gotLock )
	{
        USBLog(5, "IOUSBHIDDriver(%s)[%p]::CheckForDeadDevice  already active, returning", getName(), this );
		USBTrace( kUSBTHID,  kTPHIDCheckForDeadDevice, (uintptr_t)this, _retryCount, _deviceHasBeenDisconnected, 1);
		return;
	}

	USBTrace( kUSBTHID,  kTPHIDCheckForDeadDevice, (uintptr_t)this, _retryCount, _deviceHasBeenDisconnected, 2);
	
    // Are we still connected?
    //
    if ( _interface && _device )
    {
		UInt32		info = 0;
		
		_device->retain();
		err = _device->GetDeviceInformation(&info);
		_device->release();
		
		USBLog(6, "IOUSBHIDDriver(%s)[%p]::CheckForDeadDevice  GetDeviceInformation returned error 0x%x, info: 0x%x, retryCount: %d", getName(), this, err, (uint32_t)info, (uint32_t)_retryCount);

		// if we get an error from GetDeviceInformation, do NOT treat it like the device has been disconnected.  Just lower our retry count AND if we have reached 0, then assume it has been disconnected
		if ( err != kIOReturnSuccess )
		{
			USBTrace( kUSBTHID,  kTPHIDCheckForDeadDevice, (uintptr_t)this, err, _retryCount, 3);
			if (_retryCount != 0)
				_retryCount--;
			
			if (_retryCount == 0 )
            {
	            _deviceHasBeenDisconnected = TRUE;
				
				USBLog(5, "IOUSBHIDDriver(%s)[%p]: CheckForDeadDevice:  GetDeviceInformation and our retryCount is 0, so assume device has been unplugged", getName(), this);
				USBTrace( kUSBTHID,  kTPHIDCheckForDeadDevice, (uintptr_t)this, 0, 0, 4);
			}
			else 
			{
				USBLog(5, "IOUSBHIDDriver(%s)[%p]: CheckForDeadDevice:  GetDeviceInformation returned an error, but our retryCount is not 0", getName(), this);
				USBTrace( kUSBTHID,  kTPHIDCheckForDeadDevice, (uintptr_t)this, _retryCount, 0, 13);
			}
		}
		else
		{
			USBTrace( kUSBTHID,  kTPHIDCheckForDeadDevice, (uintptr_t)this, info, _retryCount, 5);
			// GetDeviceInformation did not error out, so see if our device is still attached (connected )
			if (info & kUSBInformationDeviceIsConnectedMask)
			{
				USBTrace( kUSBTHID,  kTPHIDCheckForDeadDevice, (uintptr_t)this, _retryCount, 0, 6);
				// Looks like the device is still plugged in.  Have we reached our retry count limit?
				//
				if (_retryCount != 0)
					_retryCount--;
				
				if (_retryCount == 0 )
				{
					ABORTEXPECTED = TRUE;
					USBLog(1, "IOUSBHIDDriver(%s)[%p]: Detected an kIONotResponding error but still connected.  Resetting port", getName(), this);
					
					if (_interruptPipe)
					{
						_interruptPipe->Abort();
					}
					
					USBTrace( kUSBTHID,  kTPHIDCheckForDeadDevice, (uintptr_t)this, 0, 0, 7);
					// OK, let 'er rip.  Let's do the reset thing
					//
					_device->ResetDevice();
				}
				else 
				{
					USBLog(5, "IOUSBHIDDriver(%s)[%p]: Still connected but retry count (%d) not reached", getName(), this, (uint32_t)_retryCount);
					USBTrace( kUSBTHID,  kTPHIDCheckForDeadDevice, (uintptr_t)this, _retryCount, 0, 11);
				}
			}
			else
			{
				// Device is not connected -- our device has gone away.
				//
				_deviceHasBeenDisconnected = TRUE;
				
				USBLog(5, "IOUSBHIDDriver(%s)[%p]: CheckForDeadDevice: device %s has been unplugged", getName(), this, _device->getName());
				USBTrace( kUSBTHID,  kTPHIDCheckForDeadDevice, (uintptr_t)this, (uintptr_t)_device, 0, 8);
			}
		}
    }
	else 
	{
		USBTrace( kUSBTHID,  kTPHIDCheckForDeadDevice, (uintptr_t)this, 0, 0, 9);
	}

	// Release our lock
	_DEAD_DEVICE_CHECK_LOCK = 0;
}


//=============================================================================================
//
//  ClearFeatureEndpointHaltEntry is called when we get an OHCI error from our interrupt read
//  (except for kIOReturnNotResponding  which will check for a dead device).  In these cases
//  we need to clear the halted bit in the controller AND we need to reset the data toggle on the
//  device.
//
//=============================================================================================
//
void
IOUSBHIDDriver::ClearFeatureEndpointHaltEntry(OSObject *target)
{
    IOUSBHIDDriver *	me = OSDynamicCast(IOUSBHIDDriver, target);

    if (!me)
        return;

    me->ClearFeatureEndpointHalt();
    me->DecrementOutstandingIO();
}



void
IOUSBHIDDriver::ClearFeatureEndpointHalt( )
{
    IOReturn			status;
    IOUSBDevRequest		request;
    UInt32			retries = 2;
    
    if (!_interruptPipe)
	{
		USBLog(1, "IOUSBHIDDriver(%s)[%p]::ClearFeatureEndpointHalt -  no interrupt pipe - bailing", getName(), this);
		USBTrace( kUSBTHID,  kTPHIDClearFeatureEndpointHalt, (uintptr_t)this, 0, 0, 0);
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
        request.bRequest 		= kUSBRqClearFeature;
        request.wValue			= 0;	// Zero is ENDPOINT_HALT
        request.wIndex			= _interruptPipe->GetEndpointNumber() | 0x80 ; // bit 7 sets the direction of the endpoint to IN
        request.wLength			= 0;
        request.pData			= NULL;
        
        // Send the command over the control endpoint
        //
        status = _device->DeviceRequest(&request, 5000, 0);
        
        if ( status != kIOReturnSuccess )
        {
            USBLog(3, "IOUSBHIDDriver(%s)[%p]::ClearFeatureEndpointHalt -  DeviceRequest returned: 0x%x, retries = %d", getName(), this, status, (uint32_t)retries);
            IOSleep(100);
        }
        else
            break;
    }
    
    // Now that we've sent the ENDPOINT_HALT clear feature, we need to requeue the interrupt read.  Note
    // that we are doing this even if we get an error from the DeviceRequest.
    //
	USBLog(5, "IOUSBHIDDriver(%s)[%p]::ClearFeatureEndpointHalt -  rearming interrupt read", getName(), this);
    (void) RearmInterruptRead();
}

//=============================================================================================
//
//  HandleReportEntry 
//
//  Calls the HID System to handle the report we got.  Note that we are relying on the fact
//  that the _buffer data will not be overwritten.  We can assume this because we are not 
//  rearming the Read until after we are done with handleReport
//=============================================================================================
//
void
IOUSBHIDDriver::HandleReportEntry(OSObject *target, thread_call_param_t timeStamp)
{
    IOUSBHIDDriver *	me = OSDynamicCast(IOUSBHIDDriver, target);
	uint64_t			currentTime = mach_absolute_time();
	uint64_t			timeElapsed;
	
    if (!me)
        return;
    
	SUB_ABSOLUTETIME(&currentTime, &me->_HANDLEREPORTTIMESTAMP);
	absolutetime_to_nanoseconds(*(AbsoluteTime *)&currentTime, &timeElapsed);
	
	USBTrace( kUSBTHID,  kTPHIDInterruptRead, (uintptr_t)me, timeElapsed/1000, 0, 14);
    me->HandleReport(  * (AbsoluteTime *)timeStamp );
    me->DecrementOutstandingIO();
}

void
IOUSBHIDDriver::HandleReport(AbsoluteTime timeStamp)
{
#pragma unused (timeStamp)
    IOReturn			status;
    IOUSBDevRequest		request;
    
	if ( _buffer->getLength() > 0 )
	{
		if ( _LOG_HID_REPORTS )
		{
			USBLog(_HID_LOGGING_LEVEL, "IOUSBHIDDriver(%s)[%p](Intfce: %d of device %s @ 0x%x) Interrupt IN report came in (%d of %d):", getName(), this, _INTERFACE_NUMBER, _device->getName(), (uint32_t)_locationID, (uint32_t)_buffer->getLength(), (uint32_t)_buffer->getCapacity() );
			LogMemReport(_HID_LOGGING_LEVEL, _buffer, _buffer->getLength() );
		}

		//status = handleReportWithTime(_INTERRUPT_TIMESTAMP, _buffer);
		status = handleReport(_buffer);
		if ( status != kIOReturnSuccess)
		{
			UInt32	bytesToLog = _buffer->getLength() > 16 ? 16 : _buffer->getLength();
			
			USBLog(1, "IOUSBHIDDriver(%s)[%p]::HandleReport handleReportWithTime() returned 0x%x (%s), report data (%d of %d bytes):", getName(), this, status, USBStringFromReturn(status), (uint32_t)bytesToLog, (uint32_t)_buffer->getLength());
			LogMemReport(1, _buffer, bytesToLog );
			USBTrace( kUSBTHID,  kTPHIDHandleReport, (uintptr_t)this, status, 0, 0);
		}
	}

	_QUEUED_REPORTS--;
	
	// Reset our timer, if applicable
	if ( _SUSPENDPORT_TIMER )
	{
		USBLog(5, "IOUSBHIDDriver(%s)[%p]::HandleReport cancelling the timeout", getName(), this);
		// First, cancel the present one
		_SUSPENDPORT_TIMER->cancelTimeout();
		
		// Now, set it again
		_SUSPENDPORT_TIMER->setTimeoutMS(_SUSPEND_TIMEOUT_IN_MS);
	}
	
    if ( !isInactive() )
    {
        // If we need to clear the stall, then do it (It will queue the interrupt read.  Otherwise, just rearm the interrupt read 
        //
		if ( _NEED_TO_CLEARPIPESTALL  )
		{
			// If thread_call_enter() returns TRUE, then a call is already
			// pending, and we need to drop our outstandingIO count.
			IncrementOutstandingIO();
			if ( thread_call_enter(_clearFeatureEndpointHaltThread) == TRUE )					// this will rearm the request when it is done
			{
				USBLog(3, "IOUSBHIDDriver(%s)[%p]::HandleReport  _clearFeatureEndpointHaltThread was already queued!", getName(), this);
				DecrementOutstandingIO();
			}
		}
		else
		{
			USBLog(7, "IOUSBHIDDriver(%s)[%p]::HandleReport  calling RearmInterruptRead()", getName(), this);
			(void) RearmInterruptRead();
		}
    }
}

void
IOUSBHIDDriver::SuspendPortTimer(OSObject *target, IOTimerEventSource *source)
{
    IOUSBHIDDriver *	me = OSDynamicCast(IOUSBHIDDriver, target);
    IOReturn			status;
	
    if (!me || !source || me->isInactive())
    {
        return;
    }
	
	USBLog(5, "IOUSBHIDDriver(%s)[%p]::SuspendPortTimer  calling AbortAndSuspend()", me->getName(), me);
	// If this timer gets called, we suspend the port.  Then, when we get resumed, we will re-enable it
	(void) me->AbortAndSuspend( true );
}


#pragma mark ееееееее HID Driver Methods еееееееее
//=============================================================================================
//
//  getMaxReportSize
//
//  Looks at both the input and feature report sizes and returns the maximum
//
//=============================================================================================
//
UInt32
IOUSBHIDDriver::getMaxReportSize()
{
    UInt32		maxInputReportSize = 0;
    UInt32		maxFeatureReportSize = 0;
	OSNumber *	inputReportSize = NULL;
	OSNumber *	featureReportSize = NULL;
    OSObject *	propertyObj = NULL;

	propertyObj = copyProperty(kIOHIDMaxInputReportSizeKey);
	inputReportSize = OSDynamicCast( OSNumber, propertyObj);
    if ( inputReportSize )
        maxInputReportSize = inputReportSize->unsigned32BitValue();
	
	if (propertyObj)
	{
		propertyObj->release();
		propertyObj = NULL;
	}

	propertyObj = copyProperty(kIOHIDMaxFeatureReportSizeKey);
	featureReportSize = OSDynamicCast( OSNumber, propertyObj);
    if ( featureReportSize )
        maxFeatureReportSize = featureReportSize->unsigned32BitValue();
	
	if (propertyObj)
		propertyObj->release();

    return ( (maxInputReportSize > maxFeatureReportSize) ? maxInputReportSize : maxFeatureReportSize);

}


//=============================================================================================
//
//  GetHIDDescriptor
//
// HIDGetHIDDescriptor is used to get a specific HID descriptor from a HID device
// (such as a report descriptor).
//
//=============================================================================================
//
IOReturn
IOUSBHIDDriver::GetHIDDescriptor(UInt8 inDescriptorType, UInt8 inDescriptorIndex, UInt8 *vOutBuf, UInt32 *vOutSize)
{
    IOUSBDevRequest 		requestPB;
    IOUSBHIDDescriptor 		*theHIDDesc;
    IOUSBHIDReportDesc 		*hidTypeSizePtr;	// For checking owned descriptors.
    UInt8 			*descPtr;
    UInt32 			providedBufferSize;
    UInt16 			descSize;
    UInt8 			descType;
    UInt8 			typeIndex;
    UInt8 			numberOwnedDesc;
    IOReturn 		err = kIOReturnSuccess;
    Boolean			foundIt;

    if (!vOutSize)
        return  kIOReturnBadArgument;

    if (!_interface)
    {
        USBLog(2, "IOUSBHIDDriver(%s)[%p]::GetHIDDescriptor - no _interface", getName(), this);
        return kIOReturnNotFound;
    }

    // From the interface descriptor, get the HID descriptor.
    theHIDDesc = (IOUSBHIDDescriptor *)_interface->FindNextAssociatedDescriptor(NULL, kUSBHIDDesc);

    if (theHIDDesc == NULL)
    {
        USBLog(2, "IOUSBHIDDriver(%s)[%p]::GetHIDDescriptor - FindNextAssociatedDescriptor(NULL, kUSBHIDDesc) failed", getName(), this);
        return kIOReturnNotFound;
    }

    // Remember the provided buffer size
    providedBufferSize = *vOutSize;
    // Are we looking for just the main HID descriptor?
    if (inDescriptorType == kUSBHIDDesc || (inDescriptorType == 0 && inDescriptorIndex == 0))
    {
        descSize = theHIDDesc->descLen;
        descPtr = (UInt8 *)theHIDDesc;

        // No matter what, set the return size to the actual size of the data.
        *vOutSize = descSize;

        // If the provided size is 0, they are just asking for the size, so don't return an error.
        if (providedBufferSize == 0)
            err = kIOReturnSuccess;
        // Otherwise, if the buffer too small, return buffer too small error.
        else if (descSize > providedBufferSize)
            err = kIOReturnNoSpace;
        // Otherwise, if the buffer nil, return that error.
        else if (vOutBuf == NULL)
            err = kIOReturnBadArgument;
        // Otherwise, looks good, so copy the deiscriptor.
        else
        {
            //IOLog("  Copying HIDDesc w/ vOutBuf = 0x%x, descPtr = 0x%x, and descSize = 0x%x.\n", vOutBuf, descPtr, descSize);
            memcpy(vOutBuf, descPtr, descSize);
        }
    }
    else
    {	// Looking for a particular type of descriptor.
      // The HID descriptor tells how many endpoint and report descriptors it contains.
        numberOwnedDesc = ((IOUSBHIDDescriptor *)theHIDDesc)->hidNumDescriptors;
        hidTypeSizePtr = (IOUSBHIDReportDesc *)&((IOUSBHIDDescriptor *)theHIDDesc)->hidDescriptorType;
        //IOLog("     %d owned descriptors start at %08x\n", numberOwnedDesc, (unsigned int)hidTypeSizePtr);

        typeIndex = 0;
        foundIt = false;
        err = kIOReturnNotFound;
        for (UInt8 i = 0; i < numberOwnedDesc; i++)
        {
            descType = hidTypeSizePtr->hidDescriptorType;

            // Are we indexing for a specific type?
            if (inDescriptorType != 0)
            {
                if (inDescriptorType == descType)
                {
                    if (inDescriptorIndex == typeIndex)
                    {
                        foundIt = true;
                    }
                    else
                    {
                        typeIndex++;
                    }
                }
            }
            // Otherwise indexing across descriptors in general.
            // (If looking for any type, index must be 1 based or we'll get HID descriptor.)
            else if (inDescriptorIndex == i + 1)
            {
                //IOLog("  said we found it because inDescriptorIndex = 0x%x.\n", inDescriptorIndex);
                typeIndex = i;
                foundIt = true;
            }

            if (foundIt)
            {
                err = kIOReturnSuccess;		// Maybe
                                         //IOLog("     Found the requested owned descriptor, %d.\n", i);
                descSize = (hidTypeSizePtr->hidDescriptorLengthHi << 8) + hidTypeSizePtr->hidDescriptorLengthLo;

                // Did we just want the size or the whole descriptor?
                // No matter what, set the return size to the actual size of the data.
                *vOutSize = descSize;	// OSX: Won't get back if we return an error!

                // If the provided size is 0, they are just asking for the size, so don't return an error.
                if (providedBufferSize == 0)
                    err = kIOReturnSuccess;
                // Otherwise, if the buffer too small, return buffer too small error.
                else if (descSize > providedBufferSize)
                    err = kIOReturnNoSpace;
                // Otherwise, if the buffer nil, return that error.
                else if (vOutBuf == NULL)
                    err = kIOReturnBadArgument;
                // Otherwise, looks good, so copy the descriptor.
                else
                {
                    if (!_device)
                    {
                        USBLog(2, "IOUSBHIDDriver(%s)[%p]::GetHIDDescriptor - no _device", getName(), this);
                        return kIOReturnNotFound;
                    }

                    requestPB.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBInterface);
                    requestPB.bRequest = kUSBRqGetDescriptor;
                    requestPB.wValue = (inDescriptorType << 8) + typeIndex;		// type and index
                    requestPB.wIndex = _interface->GetInterfaceNumber();
                    requestPB.wLength = descSize;
                    requestPB.pData = vOutBuf;						// So we don't have to do any allocation here.
					requestPB.wLenDone = 0;

					err = _device->DeviceRequest(&requestPB, 5000, 0);
                    if (err != kIOReturnSuccess)
                    {
                        USBLog(3, "IOUSBHIDDriver(%s)[%p]::GetHIDDescriptor Final request failed; err = 0x%x", getName(), this, err);
                        return err;
                    }
 					else 
					{
                        USBLog(7, "IOUSBHIDDriver(%s)[%p]::GetHIDDescriptor Final request descSize: %d, wLenDone: %d", getName(), this, descSize, (uint32_t)requestPB.wLenDone);
					}
					
					if ( requestPB.wLenDone != descSize )
					{
                        USBLog(1, "IOUSBHIDDriver(%s)[%p]::GetHIDDescriptor Final request asked for %d, but got only %d", getName(), this, descSize, (uint32_t)requestPB.wLenDone);
						
						// Wait to retry
						IOSleep(100);
						requestPB.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBInterface);
						requestPB.bRequest = kUSBRqGetDescriptor;
						requestPB.wValue = (inDescriptorType << 8) + typeIndex;		// type and index
						requestPB.wIndex = _interface->GetInterfaceNumber();
						requestPB.wLength = descSize;
						requestPB.pData = vOutBuf;						// So we don't have to do any allocation here.
						requestPB.wLenDone = 0;
						
						err = _device->DeviceRequest(&requestPB, 5000, 0);
						if (err != kIOReturnSuccess)
						{
							USBLog(1, "IOUSBHIDDriver(%s)[%p]::GetHIDDescriptor Final request#2 failed; err = 0x%x", getName(), this, err);
							return err;
						}
						if ( requestPB.wLenDone != descSize )
						{
							USBLog(3, "IOUSBHIDDriver(%s)[%p]::GetHIDDescriptor Final request#2 asked for %d, but got only %d, bailing", getName(), this, descSize, (uint32_t)requestPB.wLenDone);
						}
						else 
						{
							USBLog(7, "IOUSBHIDDriver(%s)[%p]::GetHIDDescriptor Final request#2  got the full one after waiting 100ms", getName(), this);
						}
					}               }
                break;	// out of for i loop.
            }
            // Make sure we add 3 bytes not 4 regardless of struct alignment.
            hidTypeSizePtr = (IOUSBHIDReportDesc *)(((UInt8 *)hidTypeSizePtr) + 3);
        }
    }
    return err;
}


//=============================================================================================
//
//  GetIndexedString
//
//=============================================================================================
//
IOReturn
IOUSBHIDDriver::GetIndexedString(UInt8 index, UInt8 *vOutBuf, UInt32 *vOutSize, UInt16 lang) const
{
    char 	strBuf[256];
    UInt16 	strLen = sizeof(strBuf) - 1;	// GetStringDescriptor MaxLen = 255
    UInt32 	outSize = *vOutSize;
    IOReturn 	err;

    // Valid string index?
    if (index == 0)
    {
        return kIOReturnBadArgument;
    }

    // Valid language?
    if (lang == 0)
    {
        lang = 0x409;	// Default is US English.
    }

    err = _device->GetStringDescriptor((UInt8)index, strBuf, strLen, (UInt16)lang);
    
    // When string is returned, it has been converted from Unicode and is null terminated!

    if (err != kIOReturnSuccess)
    {
        return err;
    }

    // We return the length of the string plus the null terminator,
    // but don't say a null string is 1 byte long.
    //
    strLen = (strBuf[0] == 0) ? 0 : strlen(strBuf) + 1;

    if (outSize == 0)
    {
        *vOutSize = strLen;
        return kIOReturnSuccess;
    }
    else if (outSize < strLen)
    {
        return kIOReturnMessageTooLarge;
    }

    strlcpy((char *)vOutBuf, strBuf, (size_t)strlen);
    *vOutSize = strLen;
    return kIOReturnSuccess;
}

//=============================================================================================
//
//  newIndexedString
//
//=============================================================================================
//
OSString *
IOUSBHIDDriver::newIndexedString(UInt8 index) const
{
    char string[256];
    UInt32 strSize;
    IOReturn	err = kIOReturnSuccess;

    string[0] = 0;
    strSize = sizeof(string);

    err = GetIndexedString(index, (UInt8 *)string, &strSize );

    if ( err == kIOReturnSuccess )
        return OSString::withCString(string);
    else
        return NULL;
}

//================================================================================================
//
//  StartFinalProcessing
//
//  This method may have a confusing name. This is not talking about Final Processing of the driver (as in
//  the driver is going away or something like that. It is talking about FinalProcessing of the start method.
//  It is called as the very last thing in the start method, and by default it issues a read on the interrupt
//  pipe.
//
//================================================================================================
//
IOReturn
IOUSBHIDDriver::StartFinalProcessing(void)
{
    IOReturn	err = kIOReturnSuccess;

    _COMPLETION_WITH_TIMESTAMP.target = (void *)this;
    _COMPLETION_WITH_TIMESTAMP.action = (IOUSBCompletionActionWithTimeStamp) &IOUSBHIDDriver::InterruptReadHandlerWithTimeStampEntry;
    _COMPLETION_WITH_TIMESTAMP.parameter = (void *)0;
    
    _completion.target = (void *)this;
    _completion.action = (IOUSBCompletionAction) &IOUSBHIDDriver::InterruptReadHandlerEntry;
    _completion.parameter = (void *)0;
    
    return err;
}


//================================================================================================
//
//  SetIdleMillisecs
//
//================================================================================================
//
IOReturn
IOUSBHIDDriver::SetIdleMillisecs(UInt16 msecs)
{
    IOReturn    		err = kIOReturnSuccess;
    IOUSBDevRequest		request;

    request.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface);
    request.bRequest = kHIDRqSetIdle;  //See USBSpec.h
    request.wValue = (msecs/4) << 8;
    request.wIndex = _interface->GetInterfaceNumber();
    request.wLength = 0;
    request.pData = NULL;

    err = _device->DeviceRequest(&request, 5000, 0);
    if (err != kIOReturnSuccess)
    {
        USBLog(3, "IOUSBHIDDriver(%s)[%p]::SetIdleMillisecs returned error 0x%x",getName(), this, err);
    }

    return err;

}

//================================================================================================
//
//  SetIdleMillisecs
//
//================================================================================================
//
IOReturn
IOUSBHIDDriver::SetProtocol(UInt32 protocol)
{
    IOReturn    		err = kIOReturnSuccess;
    IOUSBDevRequest		request;

    request.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface);
    request.bRequest = kHIDRqSetProtocol;  //See USBSpec.h
    request.wValue = protocol;
    request.wIndex = _interface->GetInterfaceNumber();
    request.wLength = 0;
    request.pData = NULL;

    err = _device->DeviceRequest(&request, 5000, 0);
    if (err != kIOReturnSuccess)
    {
        USBLog(3, "IOUSBHIDDriver(%s)[%p]::SetProtocol returned error 0x%x",getName(), this, err);
    }

    return err;

}

//================================================================================================
//
//  SuspendPort
//
//================================================================================================
//
OSMetaClassDefineReservedUsed(IOUSBHIDDriver,  1);
IOReturn
IOUSBHIDDriver::SuspendPort(bool suspendPort, UInt32 timeoutInMS )
{
	IOReturn	status = kIOReturnSuccess;
	
	IncrementOutstandingIO();
	
	// If we are inactive, then just return an error
	//
	if ( isInactive() )
	{
		USBLog(5, "IOUSBHIDDriver(%s)[%p]::SuspendPort (%s), but inactive", getName(), this, suspendPort ? "suspend": "resume");
		DecrementOutstandingIO();
		return kIOReturnNotPermitted;
	}
	
	USBLog(5, "IOUSBHIDDriver(%s)[%p]::SuspendPort (%s), timeout: %d, _outstandingIO = %d", getName(), this, suspendPort ? "suspend": "resume", (uint32_t)timeoutInMS, (uint32_t)_outstandingIO );
	
	// If the timeout is non-zero, that means that we are being told to enable the suspend port after the tiemout period of inactivity, not
	// immediately
	if ( suspendPort )
	{
		do
		{
			if ( timeoutInMS != 0 )
			{
				// If we already have a timer AND the timeout is different, then just update the timer with the new value, otherwise, create a
				// new timer
				if ( _SUSPENDPORT_TIMER )
				{
					if (_SUSPEND_TIMEOUT_IN_MS != timeoutInMS) 
					{
						_SUSPEND_TIMEOUT_IN_MS = timeoutInMS;
						_SUSPENDPORT_TIMER->cancelTimeout();
						_SUSPENDPORT_TIMER->setTimeoutMS(_SUSPEND_TIMEOUT_IN_MS);
					}
					break;
				}
				
				// We didn't have a timer already, so create it
				
				if ( _WORKLOOP )
				{
					_SUSPENDPORT_TIMER = IOTimerEventSource::timerEventSource(this, SuspendPortTimer);
					if (!_SUSPENDPORT_TIMER)
					{
						USBLog(1, "IOUSBHIDDriver(%s)[%p]::SuspendPort - could not create _SUSPENDPORT_TIMER", getName(), this);
						USBTrace( kUSBTHID,  kTPHIDSuspendPort, (uintptr_t)this, kIOReturnNoResources,0, 1 );
						status = kIOReturnNoResources;
						break;
					}
					
					status = _WORKLOOP->addEventSource(_SUSPENDPORT_TIMER);
					if ( status != kIOReturnSuccess)
					{
						USBLog(1, "IOUSBHIDDriver(%s)[%p]::SuspendPort - addEventSource returned 0x%x", getName(), this, status);
						USBTrace( kUSBTHID,  kTPHIDSuspendPort, (uintptr_t)this, status, 0, 2 );
						break;
					}
					
					// Now prime the sucker
					_SUSPEND_TIMEOUT_IN_MS = timeoutInMS;
					_SUSPENDPORT_TIMER->setTimeoutMS(_SUSPEND_TIMEOUT_IN_MS);
				}
				else
				{
					USBLog(1, "IOUSBHIDDriver(%s)[%p]::SuspendPort - no workloop!", getName(), this);
					USBTrace( kUSBTHID,  kTPHIDSuspendPort, (uintptr_t)this, kIOReturnNoResources, 0, 3 );
					status = kIOReturnNoResources;
				}
			}
			else
			{
				// We need to suspend right away
				status = AbortAndSuspend( true );
			}
		} while (false);
	}
	
	if ( !suspendPort and (status == kIOReturnSuccess) )
	{
		// If the timeouts are enabled, then cancel them
		if ( _SUSPENDPORT_TIMER )
		{
				// After this call completes, the action will not be called again.
				_SUSPENDPORT_TIMER->cancelTimeout();
				
				// Remove the event source
				if ( _WORKLOOP )
					_WORKLOOP->removeEventSource( _SUSPENDPORT_TIMER );
				
				_SUSPENDPORT_TIMER->release();
				_SUSPENDPORT_TIMER = NULL;
				_SUSPEND_TIMEOUT_IN_MS = 0;
		}
		
		status = AbortAndSuspend( false );
	}
	
	DecrementOutstandingIO();
	
	USBLog(5, "IOUSBHIDDriver(%s)[%p]::SuspendPort returning 0x%x", getName(), this, status );
	
	return status;
}


//================================================================================================
//
//  AbortAndSuspend
//
//================================================================================================
//
IOReturn
IOUSBHIDDriver::AbortAndSuspend(bool suspendPort )
{
	IOReturn status = kIOReturnSuccess;
	
	if ( suspendPort )
	{
		// We need to suspend our port.  If we have I/O pending, set a flag that tells the interrupt handler
		// routine that we don't need to rearm the read.
		//
		if ( _outstandingIO )
		{
			ABORTEXPECTED = true;
			if ( _interruptPipe )
			{
				
				//  We need to abort the pipes.  As of 10.5, the Abort() will not clear the data toggle at the host. Prior to
				//  that, we needed to do a ClearPipeStall(true)
				//
				status = _interruptPipe->Abort();
				if ( status != kIOReturnSuccess)
				{
					USBLog(4, "IOUSBHIDDriver(%s)[%p]::AbortAndSuspend _interruptPipe->ClearPipeStall returned 0x%x", getName(), this, status );
				}
			}
		}
		else
		{
			USBLog(4, "IOUSBHIDDriver(%s)[%p]::AbortAndSuspend suspending device, but no outstandingIO", getName(), this );
		}
		
		// Now, call in to suspend the port
		status = _device->SuspendDevice(true);
		if ( status == kIOReturnSuccess )
			_PORT_SUSPENDED = true;
		else
		{
			USBLog(4, "IOUSBHIDDriver(%s)[%p]::AbortAndSuspend SuspendDevice returned 0x%x", getName(), this, status );
		}
	}
	else
	{
		// Resuming our port
		//
		ABORTEXPECTED = false;
		
		USBLog(2, "IOUSBHIDDriver(%s)[%p]::AbortAndSuspend - calling SuspendDevice(false)", getName(), this );
		status = _device->SuspendDevice(false);
		
		if ( status != kIOReturnSuccess )
		{
			USBLog(1, "IOUSBHIDDriver(%s)[%p]::AbortAndSuspend resuming the device returned 0x%x", getName(), this, status);
			USBTrace( kUSBTHID,  kTPHIDAbortAndSuspend, (uintptr_t)this, status, 0, 0);
		}
		
		// Start up our reads again
		status = RearmInterruptRead();
	}
	
	return status;
}


OSMetaClassDefineReservedUsed(IOUSBHIDDriver,  2);
bool 
IOUSBHIDDriver::IsPortSuspended()
{
	return _PORT_SUSPENDED;
}


#pragma mark ееееееее Bookkeeping Methods еееееееее
//================================================================================================
//
//   ClaimPendingRead function (static)
//
//================================================================================================
//
IOReturn
IOUSBHIDDriver::ClaimPendingRead(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
    IOUSBHIDDriver			*me = OSDynamicCast(IOUSBHIDDriver, target);
    bool					*retVal = (bool*)param1;
	bool					myPend;
	
    if (!me)
    {
		USBLog(1, "IOUSBHIDDriver::ClaimPendingRead - invalid target");
		USBTrace( kUSBTHID,  kTPHIDClaimPendingRead, (uintptr_t)param2, (uintptr_t)param3, (uintptr_t)param4, 1  );
		return kIOReturnSuccess;
    }

	if (me->_PENDINGREAD)
	{
		myPend = false;
	}
	else
	{
		myPend = true;
		me->_PENDINGREAD = true;
	}

	if (!retVal)
	{
		USBLog(1, "IOUSBHIDDriver::ClaimPendingRead - NULL retVal!!");
		USBTrace( kUSBTHID,  kTPHIDClaimPendingRead, (uintptr_t)param2, (uintptr_t)param3, (uintptr_t)param4, 2  );
	}
	else
	{
		*retVal = myPend;
	}
	return kIOReturnSuccess;
}


//================================================================================================
//
//   ChangeOutstandingIO function
//
//================================================================================================
//
IOReturn
IOUSBHIDDriver::ChangeOutstandingIO(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
#pragma unused (param2, param3, param4)
    IOUSBHIDDriver *me = OSDynamicCast(IOUSBHIDDriver, target);
    UInt32	direction = (uintptr_t)param1;
    
    if (!me)
    {
		USBLog(1, "IOUSBHIDDriver::ChangeOutstandingIO - invalid target");
		USBTrace( kUSBTHID,  kTPHIDChangeOutstandingIO, (uintptr_t)me, kIOReturnSuccess, direction, 1 );
		return kIOReturnSuccess;
    }
    switch (direction)
    {
		case 1:
			me->_outstandingIO++;
			break;
			
		case -1:
			if (!--me->_outstandingIO && me->_needToClose)
			{
				USBLog(3, "IOUSBHIDDriver(%s)[%p]::ChangeOutstandingIO isInactive = %d, outstandingIO = %d - closing device", me->getName(), me, me->isInactive(), (uint32_t)me->_outstandingIO);
				me->_interface->close(me);
			}
			break;
			
		default:
			USBLog(1, "IOUSBHIDDriver(%s)[%p]::ChangeOutstandingIO - invalid direction", me->getName(), me);
			USBTrace( kUSBTHID,  kTPHIDChangeOutstandingIO, (uintptr_t)me, me->_outstandingIO, kIOReturnSuccess, direction );
    }
    return kIOReturnSuccess;
}


//================================================================================================
//
//   DecrementOutstandingIO function
//
//================================================================================================
//
void
IOUSBHIDDriver::DecrementOutstandingIO(void)
{
    if (!_gate)
    {
		if (!--_outstandingIO && _needToClose)
		{
			USBLog(3, "IOUSBHIDDriver(%s)[%p]::DecrementOutstandingIO isInactive = %d, outstandingIO = %d - closing device", getName(), this, isInactive(), (uint32_t)_outstandingIO);
			_interface->close(this);
		}
		return;
    }
	//USBTrace( kUSBTOutstandingIO, kTPHIDDecrement , (uintptr_t)this, (int)_outstandingIO );
    _gate->runAction(ChangeOutstandingIO, (void*)-1);
}


//================================================================================================
//
//   IncrementOutstandingIO function
//
//================================================================================================
//
void
IOUSBHIDDriver::IncrementOutstandingIO(void)
{
    if (!_gate)
    {
		_outstandingIO++;
		return;
    }
	//USBTrace( kUSBTOutstandingIO, kTPHIDIncrement , (uintptr_t)this, (int)_outstandingIO );
    _gate->runAction(ChangeOutstandingIO, (void*)1);
}



#pragma mark ееееееее Debug Methods еееееееее
OSMetaClassDefineReservedUsed(IOUSBHIDDriver,  3);
void 
IOUSBHIDDriver::LogMemReport(UInt8 level, IOMemoryDescriptor * reportBuffer, IOByteCount size)
{
    IOByteCount		reportSize;
	IOByteCount		tempSize, offset;
    char			outBuffer[1024];
    char			in[128];
    char			*out;
    char			inChar;
    
    out = (char *)&outBuffer;
    reportSize = size;
	offset = 0;
	while (reportSize)
	{
		if (reportSize > 128) 
			tempSize = 128;
		else
			tempSize = reportSize;
		
		reportBuffer->readBytes(offset, in, tempSize );
		
		for (unsigned int i = 0; i < tempSize; i++)
		{
			inChar = in[i];
			*out++ = GetHexChar(inChar >> 4);
			*out++ = GetHexChar(inChar & 0x0F);
			*out++ = ' ';
		}
		*out = 0;
		USBLog(level, "IOUSBHIDDriver(%s)[%p]  %s", getName(), this, outBuffer);
		
	    out = (char *)&outBuffer;
		offset += tempSize;
		reportSize -= tempSize;
	}
}

char 
IOUSBHIDDriver::GetHexChar(char hexChar)
{
    char hexChars[] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
    return hexChars[0x0F & hexChar];
}


#pragma mark ееееееее Obsolete Methods еееееееее
//================================================================================================
//
//   Obsolete methods
//
//================================================================================================
//
void
IOUSBHIDDriver::processPacket(void *data, UInt32 size)
{
#pragma unused (data, size)
   return;
}

IOReturn
IOUSBHIDDriver::GetReport(UInt8 inReportType, UInt8 inReportID, UInt8 *vInBuf, UInt32 *vInSize)
{
#pragma unused (inReportType, inReportID, vInBuf, vInSize)
   return kIOReturnSuccess;
}

IOReturn
IOUSBHIDDriver::SetReport(UInt8 outReportType, UInt8 outReportID, UInt8 *vOutBuf, UInt32 vOutSize)
{
#pragma unused (outReportType, outReportID, vOutBuf, vOutSize)
    return kIOReturnSuccess;
}

#pragma mark ееееееее Padding Methods еееееееее
OSMetaClassDefineReservedUsed(IOUSBHIDDriver,  0);
IOReturn
IOUSBHIDDriver::RearmInterruptRead()
{
    IOReturn		err = kIOReturnUnsupported;
	SInt32			retries = 0;
	bool			gotPend = false;
    
    // Queue up another one before we leave.
    //
	if (isInactive())
	{
		USBLog(5, "IOUSBHIDDriver(%s)[%p]::RearmInterruptRead - isInactive() - returning kIOReturnNotResponding", getName(), this);
		USBTrace( kUSBTHID,  kTPHIDRearmInterruptRead, (uintptr_t)this, 0, 0, 6 );
		_PENDINGREAD = false;
		return kIOReturnNotResponding;
	}
	
    if (!_gate || _gate->runAction(ClaimPendingRead, (void*)&gotPend))
	{
		USBLog(1, "IOUSBHIDDriver(%s)[%p]::RearmInterruptRead - unable to check for pending (_gate:%p)", getName(), this, _gate);
		USBTrace( kUSBTHID,  kTPHIDRearmInterruptRead, (uintptr_t)this, 0, 0, 3 );
		return err;
	}
	
	if (!gotPend)
	{
		USBLog(2, "IOUSBHIDDriver(%s)[%p]::RearmInterruptRead - already had outstanding read pending - just ignoring", getName(), this);
		return kIOReturnSuccess;
	}

    if ( (_buffer == NULL) || (_interruptPipe == NULL))
	{
		USBLog(1, "IOUSBHIDDriver(%s)[%p]::RearmInterruptRead - no _buffer or _interruptPipe", getName(), this);
		USBTrace( kUSBTHID,  kTPHIDRearmInterruptRead, (uintptr_t)this, 0, 0, 1 );
		_PENDINGREAD = false;
        return err;
	}
    
	// if both actions are NULL, then someone subclassed us and didn't fill them in
	if ((_COMPLETION_WITH_TIMESTAMP.action == NULL) && (_completion.action == NULL))
	{
		USBLog(1, "IOUSBHIDDriver(%s)[%p]::RearmInterruptRead - no action method", getName(), this);
		USBTrace( kUSBTHID,  kTPHIDRearmInterruptRead, (uintptr_t)this, 0, 0, 2 );
		_PENDINGREAD = false;
		return err;
	}
	
	if (isInactive())
	{
		USBLog(5, "IOUSBHIDDriver(%s)[%p]::RearmInterruptRead - isInactive() - returning kIOReturnNotResponding", getName(), this);
		USBTrace( kUSBTHID,  kTPHIDRearmInterruptRead, (uintptr_t)this, 0, 0, 6 );
		_PENDINGREAD = false;
		return kIOReturnNotResponding;
	}
	
	USBLog(7, "IOUSBHIDDriver(%s)[%p]::RearmInterruptRead - _completion.action(%p)", getName(), this, _completion.action);
    IncrementOutstandingIO();

	// Reset the length of the buffer
	_buffer->setLength(_buffer->getCapacity());

	while ( (err != kIOReturnSuccess) && (err != kIOReturnNoBandwidth) && ( retries++ < 30 ) && (_buffer != NULL) && (_interruptPipe != NULL))
	{
		if (isInactive())
		{
			USBLog(5, "IOUSBHIDDriver(%s)[%p]::RearmInterruptRead - isInactive() - returning kIOReturnNotResponding", getName(), this);
			USBTrace( kUSBTHID,  kTPHIDRearmInterruptRead, (uintptr_t)this, 0, 0, 6 );
			err = kIOReturnNotResponding;
			break;
		}
		
		if (_COMPLETION_WITH_TIMESTAMP.action)
		{
			err = _interruptPipe->Read(_buffer, 0, 0, _buffer->getLength(), &_COMPLETION_WITH_TIMESTAMP);
			if ((err == kIOReturnNotResponding) && (_POWERSTATECHANGING || (_MYPOWERSTATE < kUSBHIDPowerStateOn)))
			{
				USBLog(3, "IOUSBHIDDriver(%s)[%p]::RearmInterruptRead - err(kIOReturnNotResponding) while _POWERSTATECHANGING(%s) or (_MYPOWERSTATE < kUSBHIDPowerStateOn)(%d) - no posting read", getName(), this, _POWERSTATECHANGING ? "true" : "false", (int)_MYPOWERSTATE);
				USBTrace( kUSBTHID,  kTPHIDRearmInterruptRead, (uintptr_t)this, _POWERSTATECHANGING, _MYPOWERSTATE, 7 );
				break;			// out of the while loop
			}
		}
		else
		{
			// if there is no _COMPLETION_WITH_TIMESTAMP.action, then a subclass has overriden the initialization and didn't fill it in 
			// in that case, we need to use the other method.
			err = kIOReturnUnsupported;
		}
		
		// If we got an unsupported error, try the read without a timestamp
		//
		if ( (err == kIOReturnUnsupported) && (_interruptPipe != NULL) && (_buffer != NULL))
		{
			err = _interruptPipe->Read(_buffer, 0, 0, _buffer->getLength(), &_completion);
			if ((err == kIOReturnNotResponding) && (_POWERSTATECHANGING || (_MYPOWERSTATE < kUSBHIDPowerStateOn)))
			{
				USBLog(3, "IOUSBHIDDriver(%s)[%p]::RearmInterruptRead - err(kIOReturnNotResponding) while _POWERSTATECHANGING(%s) or (_MYPOWERSTATE < kUSBHIDPowerStateOn)(%d) - no posting read", getName(), this, _POWERSTATECHANGING ? "true" : "false", (int)_MYPOWERSTATE);
				USBTrace( kUSBTHID,  kTPHIDRearmInterruptRead, (uintptr_t)this, _POWERSTATECHANGING, _MYPOWERSTATE, 8 );
				break;			// out of the while loop
			}
		}

		// If we get an error, let's clear the pipe and try again
		if ( (err != kIOReturnSuccess) && (err != kIOReturnNoBandwidth) && (_interruptPipe != NULL))
		{
			USBLog(1, "IOUSBHIDDriver(%s)[%p]::RearmInterruptRead  immediate error 0x%x queueing read, clearing stall and trying again(%d)", getName(), this, err, (uint32_t)retries);
			USBTrace( kUSBTHID,  kTPHIDRearmInterruptRead, (uintptr_t)this, err, (uint32_t)retries, 4 );
			_interruptPipe->ClearPipeStall(false);			
			
			IOSleep(10);				// wait 10 ms before trying again
			
		}
		
    }
	
	if ( err )
	{
		if (isInactive())
		{
			USBLog(5, "IOUSBHIDDriver(%s)[%p]::RearmInterruptRead  returning error 0x%x, not issuing any reads to device", getName(), this, err);
			USBTrace( kUSBTHID,  kTPHIDRearmInterruptRead, (uintptr_t)this, err, 0, 6 );
		}
		else
		{
			if ( err != kIOReturnNoBandwidth )
			{
				USBError(1, "IOUSBHIDDriver(%s)[%p]::RearmInterruptRead  returning error 0x%x, not issuing any reads to device", getName(), this, err);
			}
			else
			{
				USBLog(3, "IOUSBHIDDriver(%s)[%p]::RearmInterruptRead  returning error 0x%x, not issuing any reads to device", getName(), this, err);
			}
		}

		_PENDINGREAD = false;
		DecrementOutstandingIO();
	}
	
	USBTrace( kUSBTHID,  kTPHIDRearmInterruptRead, (uintptr_t)this, err, 0, 5 );
    return err;
}



OSMetaClassDefineReservedUsed(IOUSBHIDDriver,  4)
IOReturn	
IOUSBHIDDriver::InitializeUSBHIDPowerManagement(IOService *provider)
{
	IOReturn		err = kIOReturnSuccess;
	
	PMinit();						// initialize IOService variables for Power Management
	
	provider->joinPMtree(this);
	
	makeUsable();
	
	err = registerPowerDriver(this, ourPowerStates, kUSBHIDNumberPowerStates);

	if (err)
	{
		USBLog(1, "IOUSBHIDDriver(%s)[%p]::InitializeUSBHIDPowerManagement - err [%p] from registerPowerDriver", getName(), this, (void*)err);
		USBTrace( kUSBTHID,  kTPHIDInitializeUSBHIDPowerManagement, (uintptr_t)this, err, 0, 0);
		PMstop();
	}
	return err;
}

OSMetaClassDefineReservedUnused(IOUSBHIDDriver,  5);
OSMetaClassDefineReservedUnused(IOUSBHIDDriver,  6);
OSMetaClassDefineReservedUnused(IOUSBHIDDriver,  7);
OSMetaClassDefineReservedUnused(IOUSBHIDDriver,  8);
OSMetaClassDefineReservedUnused(IOUSBHIDDriver,  9);
OSMetaClassDefineReservedUnused(IOUSBHIDDriver, 10);
OSMetaClassDefineReservedUnused(IOUSBHIDDriver, 11);
OSMetaClassDefineReservedUnused(IOUSBHIDDriver, 12);
OSMetaClassDefineReservedUnused(IOUSBHIDDriver, 13);
OSMetaClassDefineReservedUnused(IOUSBHIDDriver, 14);
OSMetaClassDefineReservedUnused(IOUSBHIDDriver, 15);
OSMetaClassDefineReservedUnused(IOUSBHIDDriver, 16);
OSMetaClassDefineReservedUnused(IOUSBHIDDriver, 17);
OSMetaClassDefineReservedUnused(IOUSBHIDDriver, 18); 
OSMetaClassDefineReservedUnused(IOUSBHIDDriver, 19);

