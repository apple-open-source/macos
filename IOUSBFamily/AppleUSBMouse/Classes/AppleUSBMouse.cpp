/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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

#include <libkern/OSByteOrder.h>

#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOMessage.h>

#include <IOKit/hidsystem/IOHIPointing.h>
#include <IOKit/hidsystem/IOHIDShared.h>
#include <IOKit/hidsystem/IOHIDDescriptorParser.h>

#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/IOUSBLog.h>

#include <IOKit/pwr_mgt/RootDomain.h>

#include "AppleUSBMouse.h"

#define super IOHIPointing
#define DEBUGGING_LEVEL 0

#define kMaxButtons	32	// Is this defined anywhere in the event headers?
#define kMaxValues	32	// This should be plenty big to find the X, Y and wheel values - is there some absolute max?

#define kDefaultFixedResolution (400 << 16)

OSDefineMetaClassAndStructors(AppleUSBMouse, IOHIPointing)

static	bool switchTo800dpi = true;

bool 
AppleUSBMouse::init(OSDictionary * properties)
{
#if (DEBUGGING_LEVEL > 1)
    IOLog("Calling AppleUSBMouse::init.\n");
#endif

  if (!super::init(properties))  return false;

   // Initialize minimal state: fields that have access methods that may
   // be called before AppleUSBMouse::start().
    _numButtons = 1;
    _resolution = kDefaultFixedResolution;
    _preparsedReportDescriptorData = NULL;
    _buttonCollection = -1;
    _xCollection = -1;
    _yCollection = -1;
    _tipPressureCollection = -1;
    _digitizerButtonCollection = -1;
    _scrollWheelCollection = -1;
    _hasInRangeReport = false;
    _tipPressureMin = 255;
    _tipPressureMax = 255;
    _retryCount = kMouseRetryCount;
    _deviceIsDead = false;
    _deviceHasBeenDisconnected = false;
    _outstandingIO = 0;
    _needToClose = false;
    return true;
}



bool 
AppleUSBMouse::start(IOService * provider)
{
    IOReturn			err = 0;
    IOWorkLoop			*wl;

    USBLog(3, "%s[%p]::start - beginning - retain count = %d", getName(), this, getRetainCount());
    _interface		= OSDynamicCast(IOUSBInterface, provider);
    if (!_interface)
        return false;
    
    if( !_interface->open(this))
    {
        USBError(1, "%s[%p]::start - unable to open provider. returning false", getName(), this);
        return (false);
    }

    do {
        IOUSBFindEndpointRequest request;

	// remember my device
	_device = _interface->GetDevice();
	if (!_device)
	{
	    USBError(1, "%s[%p]::start - unable to get handle to my device - something must be wrong", getName(), this);
	    break;	// error
	}

        if (!parseHIDDescriptor()) 
        {
            USBError(1, "%s[%p]::start - unable to parse HID descriptor", getName(), this);
            break;
        }

        if ((_device->GetVendorID() == kIOUSBVendorIDAppleComputer) && (_device->GetProductID()  == 0x0306))
        {
            UInt32 	resPrefInt;
            IOFixed	resPref;

            OSNumber * resPrefPtr = (OSNumber *)getProperty("xResolutionPref");
            if (resPrefPtr)
            {
                resPrefInt = resPrefPtr->unsigned32BitValue();
            }
            else
            {
                resPrefInt = kDefaultFixedResolution * 2;
            }

            resPref = (IOFixed) resPrefInt;
            
            if (resPref != _resolution)
            {
		if (switchTo800dpi)
		{
		    IOUSBDevRequest		devReq;
    
		    devReq.bmRequestType = 0x40;
		    devReq.bRequest = 0x01;
		    devReq.wValue = 0x05AC;
		    devReq.wIndex = 0x0452;
		    devReq.wLength = 0x0000;
		    devReq.pData = NULL;
		
		    err = _device->DeviceRequest(&devReq, 5000, 0);
		    
		    if (err) 
		    {
			USBLog(3, "%s[%p]::start - error (%x) setting resolution", getName(), this, err);
			break;		// Don't go on to read.
		    }
		    // with this mouse, we do NOT want to start reading on the interrupt pipe, nor do
		    // we want to call super::start. We just want to wait for the device to get terminated
		    USBLog(3, "%s[%p]::start - waiting for click mouse termination", getName(), this);
    
		    return true;
		}
            }
            else
            {
                // If we are already at the correct resolution for OSX, OK. But what if we are going
                // back to OS 9? On restart, switch back to boot setup. Power Manager will tell us
                // when we are going to restart.
                //
                _notifier = registerPrioritySleepWakeInterest(PowerDownHandler, this, 0);
            }
        }

        _gate = IOCommandGate::commandGate(this);

        if(!_gate)
        {
            USBError(1, "%s[%p]::start - unable to create command gate", getName(), this);
            break;
        }

	wl = getWorkLoop();
	if (!wl)
	{
            USBError(1, "%s[%p]::start - unable to find my workloop", getName(), this);
            break;
	}
	
        if (wl->addEventSource(_gate) != kIOReturnSuccess)
        {
            USBError(1, "%s[%p]::start - unable to add gate to work loop", getName(), this);
            break;
        }

        request.type = kUSBInterrupt;
        request.direction = kUSBIn;
        _interruptPipe = _interface->FindNextPipe(NULL, &request);

        if(!_interruptPipe)
        {
            USBError(1, "%s[%p]::start - unable to get interrupt pipe", getName(), this);
            break;
        }

        // allocate a thread_call structure
        _deviceDeadCheckThread = thread_call_allocate((thread_call_func_t)CheckForDeadDeviceEntry, (thread_call_param_t)this);
        _clearFeatureEndpointHaltThread = thread_call_allocate((thread_call_func_t)ClearFeatureEndpointHaltEntry, (thread_call_param_t)this);
        
        if ( !_deviceDeadCheckThread || !_clearFeatureEndpointHaltThread )
        {
            USBLog(3, "[%s]%p: could not allocate all thread functions", getName(), this);
            break;
        }

        _maxPacketSize = request.maxPacketSize;
        _buffer = IOBufferMemoryDescriptor::withCapacity(_maxPacketSize, kIODirectionIn);
        if ( !_buffer )
        {
            USBError(1, "%s[%p]::start - unable to get create buffer", getName(), this);
            break;
        }

        _completion.target = (void *)this;
        _completion.action = (IOUSBCompletionAction) &AppleUSBMouse::InterruptReadHandlerEntry;
        _completion.parameter = (void *)0;  // not used
        IncrementOutstandingIO();
       
        _buffer->setLength(_maxPacketSize);

        if ((err = _interruptPipe->Read(_buffer, &_completion)))
        {
            USBError(1, "%s[%p]::start - err (%x) queueing interrupt read, retain count %d after release", getName(), this, err, getRetainCount());
            DecrementOutstandingIO();
            break;
        }

        USBError(1, "%s[%p]::start - USB Generic Mouse @ %d (0x%x)", getName(), this, _device->GetAddress(), strtol(_device->getLocation(), (char **)NULL, 16));

	// OK- so this is not totally kosher in the IOKit world. You are supposed to call super::start near the BEGINNING
	// of your own start method. However, the IOHIPointing::start method invokes registerService, which we don't want to
	// do if we get any error up to this point. So we wait and call IOHIPointing::start here.
	if( !super::start(_interface))
	{
	    USBError(1, "%s[%p]::start - unable to start superclass. returning false", getName(), this);
	    break;	// error
	}
	    
        return true;		// Normal successful return.

    } while (false);

    USBLog(3, "%s[%p]::start - err (%x) - aborting -  retain count %d", getName(), this, err, getRetainCount());

    if ( _interruptPipe )
    {
        _interruptPipe->Abort();
        _interruptPipe = NULL;		// NULL this out here since we will not go through the normal termination sequence
    }
    
    if (_interface->isOpen(this))
	_interface->close(this);
	
    stop(_interface);			// this cleans up all the variables. IOKit might do this for us anyway, but I am not sure

    return(false);
}



bool 
AppleUSBMouse::parseHIDDescriptor()
{
    bool 			success = true;
    IOReturn			err;
    IOUSBDevRequest		devReq;
    IOUSBHIDDescriptor		hidDescriptor;
    UInt8 *			reportDescriptor = NULL;
    UInt16			size = 0;
    OSStatus			result;
    HIDButtonCapabilities	buttonCaps[kMaxButtons];
    UInt32			numButtonCaps = kMaxButtons;
    HIDValueCapabilities	valueCaps[kMaxValues];
    UInt32			numValueCaps = kMaxValues;

    do {
        devReq.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBInterface);
        devReq.bRequest = kUSBRqGetDescriptor;
        devReq.wValue = (0x20 | kUSBHIDDesc) << 8;
        devReq.wIndex = _interface->GetInterfaceNumber();
        devReq.wLength = sizeof(IOUSBHIDDescriptor);
        devReq.pData = &hidDescriptor;

        err = _device->DeviceRequest(&devReq, 5000, 0);
        if (err) 
        {
            IOLog ("%s: error getting HID Descriptor.  err=0x%x\n", getName(), err);
            success = false;
            break;
        }

        size = (hidDescriptor.hidDescriptorLengthHi * 256) + hidDescriptor.hidDescriptorLengthLo;
        reportDescriptor = (UInt8 *)IOMalloc(size);

        devReq.wValue = ((0x20 | kUSBReportDesc) << 8);
        devReq.wLength = size;
        devReq.pData = reportDescriptor;

        err = _device->DeviceRequest(&devReq, 5000, 0);
        if (err) 
        {
            IOLog ("%s: error getting HID report descriptor.  err=0x%x\n", getName(), err);
            success = false;
            break;
        }
    
        result = HIDOpenReportDescriptor (reportDescriptor, size, &_preparsedReportDescriptorData, 0);
        if (result != noErr) 
        {
            IOLog ("%s: error parsing HID report descriptor.  err=0x%lx (%ld)\n", getName(), result, result);
            success = false;
            break;
        }

        result = HIDGetSpecificButtonCapabilities(kHIDInputReport,
                                          kHIDPage_Button,
                                          0,
                                          0,
                                          buttonCaps,
                                          &numButtonCaps,
                                          _preparsedReportDescriptorData);
        if ((result == noErr) && (numButtonCaps > 0)) 
        {
            _buttonCollection = buttonCaps[0].collection;	// Do we actually need to look at and store all of the button page collections?
            if (buttonCaps[0].isRange) 
            {
                _numButtons = buttonCaps[0].u.range.usageMax - buttonCaps[0].u.range.usageMin + 1;
            }
            
        }

        numButtonCaps = kMaxButtons;
        result = HIDGetSpecificButtonCapabilities(kHIDInputReport,
                                          kHIDPage_Digitizer,
                                          0,
                                          0,
                                          buttonCaps,
                                          &numButtonCaps,
                                          _preparsedReportDescriptorData);
        if ((result == noErr) && (numButtonCaps > 0)) {
            _digitizerButtonCollection = buttonCaps[0].collection;
        }

        numButtonCaps = kMaxButtons;
        result = HIDGetSpecificButtonCapabilities(kHIDInputReport,
                                          kHIDPage_Digitizer,
                                          0,
                                          kHIDUsage_Dig_InRange,
                                          buttonCaps,
                                          &numButtonCaps,
                                          _preparsedReportDescriptorData);
        if (result == noErr) {
            _hasInRangeReport = true;
        }

        result = HIDGetSpecificValueCapabilities(kHIDInputReport,
                                         kHIDPage_GenericDesktop,
                                         0,
                                         kHIDUsage_GD_X,
                                         valueCaps,
                                         &numValueCaps,
                                         _preparsedReportDescriptorData);
        if ((result == noErr) && (numValueCaps > 0))
        {
            _xCollection = valueCaps[0].collection;
            _absoluteCoordinates = valueCaps[0].isAbsolute;
            _bounds.minx = valueCaps[0].logicalMin;
            _bounds.maxx = valueCaps[0].logicalMax;
            
            // Check to see if this has a different resolution. (Only checking x-axis.) 
            // (Can use equation given in section 6.2.2.7 of the Device Class Definition for HID, v 1.1.)
            // _resolution = (logMax -logMin)/((physMax -physMin) * 10 ** exp)

            // If there is no physical min and max in HID descriptor,
            // cababilites calls set equal to logical min and max.
            // Keep default resolution if we don't have distinct physical min and max.
            if (valueCaps[0].physicalMin != valueCaps[0].logicalMin &&
                valueCaps[0].physicalMax != valueCaps[0].logicalMax)
            {
                SInt32 logicalDiff = (valueCaps[0].logicalMax - valueCaps[0].logicalMin);
                SInt32 physicalDiff = (valueCaps[0].physicalMax - valueCaps[0].physicalMin);
                
                // Since IOFixedDivide truncated fractional part and can't use floating point
                // within the kernel, have to convert equation when using negative exponents:
                // _resolution = ((logMax -logMin) * 10 **(-exp))/(physMax -physMin)

                // Even though unitExponent is stored as SInt32, The real values are only
                // a signed nibble that doesn't expand to the full 32 bits.
                SInt32 resExponent = valueCaps[0].unitExponent & 0x0F;
                
                if (resExponent < 8)
                {
                    for (int i = resExponent; i > 0; i--)
                    {
                        physicalDiff *=  10;
                    }
                }
                else
                {
                    for (int i = 0x10 - resExponent; i > 0; i--)
                    {
                        logicalDiff *= 10;
                    }
                }
                _resolution = (logicalDiff / physicalDiff) << 16;

                USBLog (3, "%s[%p]::parseHIDDescriptor: setting resolution to %x", getName(), this, _resolution);
			    
                // Before i added in the AppleUSBMouse::init function, resolution was called to calculate
                // the acceleration curves before we ever got to start, which in turn called parseHIDDescriptor
                // where the real resolution was calculated. In that event, if the resolution changed from the
                // default value, we would have to tell IOHIPointing to recalculate the curves based on the
                // new resolution. Per Adam Wang, we could call IOHIPointing::resetPointer() to do the
                // recalculation. Unfortunately IOHIPointing::resetPointer() is private and cannot be used
                // here. Rather than go back to IOHIPointing for an API change, since the new init function
                // seems to have us calling AppleUSBMouse::start first, we no longer need to deal with this.
                // (I am leaving this code snippet here as a reminder in case something changes and i don't
                // want to loose this arcane bit of knowledge.)
                //
                // if (_resolution != kDefaultFixedResolution)
                // {
                //     resetPointer();
                // }
            }

        } else {
            IOLog ("%s: error getting X axis information from HID report descriptor.  err=0x%lx\n", getName(), result);
            success = false;
            break;
        }

        numValueCaps = kMaxValues;
        result = HIDGetSpecificValueCapabilities(kHIDInputReport,
                                         kHIDPage_GenericDesktop,
                                         0,
                                         kHIDUsage_GD_Y,
                                         valueCaps,
                                         &numValueCaps,
                                         _preparsedReportDescriptorData);
        if ((result == noErr) && (numValueCaps > 0)) {
            _yCollection = valueCaps[0].collection;
            _bounds.miny = valueCaps[0].logicalMin;
            _bounds.maxy = valueCaps[0].logicalMax;
        } else {
            IOLog ("%s: error getting Y axis information from HID report descriptor.  err=0x%lx\n", getName(), result);
            success = false;
            break;
        }

        numValueCaps = kMaxValues;
        result = HIDGetSpecificValueCapabilities(kHIDInputReport,
                                         kHIDPage_Digitizer,
                                         0,
                                         kHIDUsage_Dig_TipPressure,
                                         valueCaps,
                                         &numValueCaps,
                                         _preparsedReportDescriptorData);
        if ((result == noErr) && (numValueCaps > 0)) {
            _tipPressureCollection = valueCaps[0].collection;
            _tipPressureMin = valueCaps[0].logicalMin;
            _tipPressureMax = valueCaps[0].logicalMax;
        }

        numValueCaps = kMaxValues;
        result = HIDGetSpecificValueCapabilities(kHIDInputReport,
                                         kHIDPage_GenericDesktop,
                                         0,
                                         kHIDUsage_GD_Wheel,
                                         valueCaps,
                                         &numValueCaps,
                                         _preparsedReportDescriptorData);
        if ((result == noErr) && (numValueCaps > 0)) {
            _scrollWheelCollection = valueCaps[0].collection;
        }
    } while (false);

    if (reportDescriptor) {
        IOFree(reportDescriptor, size);
    }
    
    return success;
}



void 
AppleUSBMouse::stop(IOService * provider)
{
    
    if (_buffer) 
    {
	_buffer->release();
        _buffer = NULL;
    }
    if (_preparsedReportDescriptorData) 
    {
        HIDCloseReportDescriptor(_preparsedReportDescriptorData);
    }
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
    
    if (_gate)
    {
	IOWorkLoop	*wl = getWorkLoop();
	if (wl)
	    wl->removeEventSource(_gate);
	_gate->release();
	_gate = NULL;
    }
    
    // last i heard, super::stop didn't really do anything, but call it for completeness
    super::stop(provider);
}


void 
AppleUSBMouse::MoveMouse(UInt8 *	mouseData,
                           UInt32	ret_bufsize)
{
    OSStatus	status;
    HIDUsage	usageList[kMaxButtons];
    UInt32	usageListSize = kMaxButtons;
    UInt32	buttonState = 0;
    SInt32	usageValue;
    SInt32	pressure = MAXPRESSURE;
    int		dx = 0, dy = 0, scrollWheelDelta = 0;
    AbsoluteTime now;
    bool	inRange = !_hasInRangeReport;

    if (_buttonCollection != -1) {
        status = HIDGetButtonsOnPage (kHIDInputReport,
                                      kHIDPage_Button,
                                      _buttonCollection,
                                      usageList,
                                      &usageListSize,
                                      _preparsedReportDescriptorData,
                                      mouseData,
                                      ret_bufsize);
        if (status == noErr) {
            UInt32 usageNum;
            for (usageNum = 0; usageNum < usageListSize; usageNum++) {
                if (usageList[usageNum] <= kMaxButtons) {
                    buttonState |= (1 << (usageList[usageNum] - 1));
                }
            }
        }

    }

    if (_tipPressureCollection != -1) {
        status = HIDGetUsageValue (kHIDInputReport,
                                   kHIDPage_Digitizer,
                                   _tipPressureCollection,
                                   kHIDUsage_Dig_TipPressure,
                                   &usageValue,
                                   _preparsedReportDescriptorData,
                                   mouseData,
                                   ret_bufsize);
        if (status == noErr) {
            pressure = usageValue;
        }
    }

    if (_digitizerButtonCollection != -1) {
        usageListSize = kMaxButtons;
        status = HIDGetButtonsOnPage (kHIDInputReport,
                                      kHIDPage_Digitizer,
                                      _digitizerButtonCollection,
                                      usageList,
                                      &usageListSize,
                                      _preparsedReportDescriptorData,
                                      mouseData,
                                      ret_bufsize);
        if (status == noErr) {
            UInt32 usageNum;
            for (usageNum = 0; usageNum < usageListSize; usageNum++) {
                switch (usageList[usageNum]) {
                    case kHIDUsage_Dig_BarrelSwitch:
                        buttonState |= 2;	// Set the right (secondary) button for the barrel switch
                        break;
                    case kHIDUsage_Dig_TipSwitch:
                        buttonState |= 1;	// Set the left (primary) button for the tip switch
                        break;
                    case kHIDUsage_Dig_InRange:
                        inRange = 1;
                        break;
                    default:
                        break;
                }
            }
        }
    }

    if (_scrollWheelCollection != -1) {
        status = HIDGetUsageValue (kHIDInputReport,
                                   kHIDPage_GenericDesktop,
                                   _scrollWheelCollection,
                                   kHIDUsage_GD_Wheel,
                                   &usageValue,
                                   _preparsedReportDescriptorData,
                                   mouseData,
                                   ret_bufsize);
        if (status == noErr) {
            scrollWheelDelta = usageValue;
        }
    }

    status = HIDGetUsageValue (kHIDInputReport,
                               kHIDPage_GenericDesktop,
                               _xCollection,
                               kHIDUsage_GD_X,
                               &usageValue,
                               _preparsedReportDescriptorData,
                               mouseData,
                               ret_bufsize);
    if (status == noErr) {
        dx = usageValue;
    }

    status = HIDGetUsageValue (kHIDInputReport,
                               kHIDPage_GenericDesktop,
                               _yCollection,
                               kHIDUsage_GD_Y,
                               &usageValue,
                               _preparsedReportDescriptorData,
                               mouseData,
                               ret_bufsize);
    if (status == noErr) {
        dy = usageValue;
    }

    clock_get_uptime(&now);

    if (_absoluteCoordinates) {
        Point newLoc;

        newLoc.x = dx;
        newLoc.y = dy;

        dispatchAbsolutePointerEvent(&newLoc, &_bounds, buttonState, inRange, pressure, _tipPressureMin, _tipPressureMax, 90, now);
    } else {
        dispatchRelativePointerEvent(dx, dy, buttonState, now);
    }

    if (scrollWheelDelta != 0) {
        dispatchScrollWheelEvent(scrollWheelDelta, 0, 0, now);
    }
}



UInt32 
AppleUSBMouse::interfaceID( void )
{
    return( NX_EVS_DEVICE_INTERFACE_OTHER );
}



UInt32 
AppleUSBMouse::deviceType( void )
{
    return( 0 );
}



IOFixed 
AppleUSBMouse::resolution()
{
    USBLog(3, "%s[%p]::resolution: returning %x", getName(), this, _resolution);

    return _resolution;
}



IOItemCount
AppleUSBMouse::buttonCount()
{
    return _numButtons;
}



IOReturn 
AppleUSBMouse::message( UInt32 type, IOService * provider,  void * argument = 0 )
{
    IOReturn		err = kIOReturnSuccess;
    
    switch ( type )
    {
        case kIOMessageServiceIsTerminated:
            USBLog(3, "%s[%p]::message - kIOMessageServiceIsTerminated - ignoring now", getName(), this);
	    break;

        case kIOUSBMessagePortHasBeenReset:
	    USBLog(3, "%s[%p]::message - kIOUSBMessagePortHasBeenReset", getName(), this);
            _retryCount = kMouseRetryCount;
            _deviceIsDead = FALSE;
            _deviceHasBeenDisconnected = FALSE;
            
            IncrementOutstandingIO();
	    if (_interruptPipe && !isInactive())
	    {
		if ((err = _interruptPipe->Read(_buffer, &_completion)))
		{
		    DecrementOutstandingIO();
		    USBLog(3, "%s[%p]::message - err (%x) in interrupt read", getName(), this, err);
		}
	    }
	    else
	    {
		USBError(1, "%s[%p]::message - no _interruptPipe(%x) or already inactive(%d)", getName(), this, _interruptPipe, isInactive());
		DecrementOutstandingIO();	// we decided not to queue anything
	    }
	    
            break;
  
        default:
            err = kIOReturnUnsupported;
            break;
    }
    
    return err;
}

bool 
AppleUSBMouse::finalize(IOOptionBits options)
{
    return(super::finalize(options));
}

//=============================================================================================
//
//  InterruptReadHandlerEntry is called to process any data coming in through our interrupt pipe
//
//=============================================================================================
//
void 
AppleUSBMouse::InterruptReadHandlerEntry(OSObject *target, void *param, IOReturn status, UInt32 bufferSizeRemaining)
{
    AppleUSBMouse *	me = OSDynamicCast(AppleUSBMouse, target);

    if (!me)
        return;
    
    me->InterruptReadHandler(status, bufferSizeRemaining);
    me->DecrementOutstandingIO();
}



void 
AppleUSBMouse::InterruptReadHandler(IOReturn status, UInt32 bufferSizeRemaining)
{
    bool		queueAnother = true;
    bool		timeToGoAway = false;
    IOReturn		err = kIOReturnSuccess;

    switch (status)
    {
        case kIOReturnOverrun:
            USBLog(3, "%s[%p]::InterruptReadHandler kIOReturnOverrun error", getName(), this);
            // This is an interesting error, as we have the data that we wanted and more...  We will use this
            // data but first we need to clear the stall and reset the data toggle on the device.  We will not
            // requeue another read because our _clearFeatureEndpointHaltThread will requeue it.  We then just 
            // fall through to the kIOReturnSuccess case.
	    // 01-18-02 if we are inactive, then we won't queue any more, so was can ignore this
            
	    if (!isInactive())
	    {
		//
		// First, clear the halted bit in the controller
		//
		_interruptPipe->ClearStall();
		
		// And call the device to reset the endpoint as well
		//
		IncrementOutstandingIO();
		thread_call_enter(_clearFeatureEndpointHaltThread);
	    }
            queueAnother = false;
            timeToGoAway = false;
            
            // Fall through to process the data.
            
        case kIOReturnSuccess:
            // Reset the retry count, since we had a successful read
            //
            _retryCount = kMouseRetryCount;

            // Handle the data
            //
            MoveMouse((UInt8 *) _buffer->getBytesNoCopy(), (UInt32)  _maxPacketSize - bufferSizeRemaining);
	    if (isInactive())
		queueAnother = false;
            break;

        case kIOReturnNotResponding:
            USBLog(3, "%s[%p]::InterruptReadHandler kIOReturnNotResponding error", getName(), this);
            // If our device has been disconnected or we're already processing a
            // terminate message, just go ahead and close the device (i.e. don't
            // queue another read.  Otherwise, go check to see if the device is
            // around or not. 
            //
            if ( _deviceHasBeenDisconnected || isInactive() )
            {
                  queueAnother = false;
                  timeToGoAway = true;
            }
            else
            {
                USBLog(3, "%s[%p]::InterruptReadHandler Checking to see if mouse is still connected", getName(), this);
                IncrementOutstandingIO();
                thread_call_enter(_deviceDeadCheckThread);
                
                // Before requeueing, we need to clear the stall
                //
                _interruptPipe->ClearStall();
            }
                
            break;
            
	case kIOReturnAborted:
	    // This generally means that we are done, because we were unplugged, but not always
            //
            if (isInactive() || _deviceIsDead )
	    {
                USBLog(3, "%s[%p]::InterruptReadHandler error kIOReturnAborted (expected)", getName(), this);
		queueAnother = false;
                timeToGoAway = true;
	    }
	    else
            {
                USBLog(3, "%s[%p]::InterruptReadHandler error kIOReturnAborted. Try again.", getName(), this);
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
            // to clear the stall at the controller and at the device.  We will not requeue the read
            // until after we clear the ENDPOINT_HALT feature.  We need to do a callout thread because
            // we are executing inside the gate here and we cannot issue a synchronous request.
            USBLog(3, "%s[%p]::InterruptReadHandler OHCI error (0x%x) reading interrupt pipe", getName(), this, status);
            
	    // 01-18-02 JRH If we are inactive, then we can ignore this
	    if (!isInactive())
	    {
		// First, clear the halted bit in the controller
		//
		_interruptPipe->ClearStall();
		
		// And call the device to reset the endpoint as well
		//
		IncrementOutstandingIO();
		thread_call_enter(_clearFeatureEndpointHaltThread);
            }
            // We don't want to requeue the read here, AND we don't want to indicate that we are done
            //
            queueAnother = false;
            break;
            
        default:
            // We should handle other errors more intelligently, but
            // for now just return and assume the error is recoverable.
            USBLog(3, "%s[%p]::InterruptReadHandler error (0x%x) reading interrupt pipe", getName(), this, status);
	    if (isInactive())
		queueAnother = false;
            break;
    }

    if ( queueAnother )
    {
        // Queue up another one before we leave.
        //
	IncrementOutstandingIO();
        err = _interruptPipe->Read(_buffer, &_completion);
        if ( err != kIOReturnSuccess)
        {
            // This is bad.  We probably shouldn't continue on from here.
            USBError(1, "%s[%p]::InterruptReadHandler immediate error 0x%x queueing read\n", getName(), this, err);
            DecrementOutstandingIO();
            timeToGoAway = true;
        }
    }

    if ( timeToGoAway )
    {
	AbsoluteTime 	now;
	
        // It's time to go away
	clock_get_uptime(&now);
	dispatchRelativePointerEvent(0, 0, 0, now);
    }
}

//=============================================================================================
//
//  CheckForDeadDevice is called when we get a kIODeviceNotResponding error in our interrupt pipe.
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
AppleUSBMouse::CheckForDeadDeviceEntry(OSObject *target)
{
    AppleUSBMouse *	me = OSDynamicCast(AppleUSBMouse, target);
    
    if (!me)
        return;
        
    me->CheckForDeadDevice();
    me->DecrementOutstandingIO();
}

void 
AppleUSBMouse::CheckForDeadDevice()
{
    IOReturn			err = kIOReturnSuccess;
    IOUSBDevice *		device;

    _deviceDeadThreadActive = TRUE;
    // Are we still connected?
    //
    if ( _interface )
    {
        device = _interface->GetDevice();
        if ( device )
        {
            err = device->message(kIOUSBMessageHubIsDeviceConnected, NULL, 0);
        
            if ( kIOReturnSuccess == err )
            {
                // Looks like the device is still plugged in.  Have we reached our retry count limit?
                //
                if ( --_retryCount == 0 )
                {
                    _deviceIsDead = TRUE;
                    USBLog(3, "%s[%p]: Detected an kIONotResponding error but still connected.  Resetting port", getName(), this);
                    
                    if (_interruptPipe)
                        _interruptPipe->Abort();  // This will end up closing the interface as well.

                    // OK, let 'er rip.  Let's do the reset thing
                    //
                     device->ResetDevice();
                        
                }
            }
            else
            {
                // Device is not connected -- our device has gone away.  The message kIOServiceIsTerminated
                // will take care of shutting everything down.  
                //
                _deviceHasBeenDisconnected = TRUE;
                USBLog(5, "%s[%p]: CheckForDeadDevice: device has been unplugged", getName(), this);
            }
        }
    }
    _deviceDeadThreadActive = FALSE;
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
AppleUSBMouse::ClearFeatureEndpointHaltEntry(OSObject *target)
{
    AppleUSBMouse *	me = OSDynamicCast(AppleUSBMouse, target);
    
    if (!me)
        return;
        
    me->ClearFeatureEndpointHalt();
    me->DecrementOutstandingIO();
}

void 
AppleUSBMouse::ClearFeatureEndpointHalt( )
{
    IOReturn			status;
    IOUSBDevRequest		request;
    
    // Clear out the structure for the request
    //
    bzero( &request, sizeof(IOUSBDevRequest));

    // Build the USB command to clear the ENDPOINT_HALT feature for our interrupt endpoint
    //
    request.bmRequestType 	= USBmakebmRequestType(kUSBNone, kUSBStandard, kUSBEndpoint);
    request.bRequest 		= kUSBRqClearFeature;
    request.wValue		= kUSBFeatureEndpointStall;
    request.wIndex		= _interruptPipe->GetEndpointNumber() | 0x80 ; // bit 7 sets the direction of the endpoint to IN
    request.wLength		= 0;
    request.pData 		= NULL;

    // Send the command over the control endpoint
    //
    status = _device->DeviceRequest(&request, 5000, 0);

    if ( status )
    {
        USBLog(3, "%s[%p]::ClearFeatureEndpointHalt -  DeviceRequest returned: 0x%x", getName(), this, status);
    }
    
    // Now that we've sent the ENDPOINT_HALT clear feature, we need to requeue the interrupt read.  Note
    // that we are doing this even if we get an error from the DeviceRequest.
    //
    IncrementOutstandingIO();
    status = _interruptPipe->Read(_buffer, &_completion);
    if ( status != kIOReturnSuccess)
    {
        // This is bad.  We probably shouldn't continue on from here.
        USBLog(3, "%s[%p]::ClearFeatureEndpointHalt -  immediate error %d queueing read", getName(), this, status);
        DecrementOutstandingIO();
        // _interface->close(this); we should let didTerminate do this
    }
}


bool
AppleUSBMouse::willTerminate( IOService * provider, IOOptionBits options )
{
    // this method is intended to be used to stop any pending I/O and to make sure that 
    // we have begun getting our callbacks in order. by the time we get here, the 
    // isInactive flag is set, so we really are marked as being done. we will do in here
    // what we used to do in the message method (this happens first)
    USBLog(3, "%s[%p]::willTerminate isInactive = %d", getName(), this, isInactive());
    if (_interruptPipe)
	_interruptPipe->Abort();

    // Clean up our notifier.  That will release it
    //
    if ( _notifier )
        _notifier->remove();
    
    return super::willTerminate(provider, options);
}


bool
AppleUSBMouse::didTerminate( IOService * provider, IOOptionBits options, bool * defer )
{
    // this method comes at the end of the termination sequence. Hopefully, all of our outstanding IO is complete
    // in which case we can just close our provider and IOKit will take care of the rest. Otherwise, we need to 
    // hold on to the device and IOKit will terminate us when we close it later
    USBLog(3, "%s[%p]::didTerminate isInactive = %d, outstandingIO = %d", getName(), this, isInactive(), _outstandingIO);
    if (!_outstandingIO)
	_interface->close(this);
    else
	_needToClose = true;
    return super::didTerminate(provider, options, defer);
}


#if 0
bool 	
AppleUSBMouse::requestTerminate( IOService * provider, IOOptionBits options )
{
    USBLog(3, "%s[%p]::requestTerminate isInactive = %d", getName(), this, isInactive());
    return super::requestTerminate(provider, options);
}


bool
AppleUSBMouse::terminate( IOOptionBits options = 0 )
{
    USBLog(3, "%s[%p]::terminate isInactive = %d", getName(), this, isInactive());
    return super::terminate(options);
}


void
AppleUSBMouse::free( void )
{
    USBLog(3, "%s[%p]::free isInactive = %d", getName(), this, isInactive());
    super::free();
}


bool
AppleUSBMouse::terminateClient( IOService * client, IOOptionBits options )
{
    USBLog(3, "%s[%p]::terminateClient isInactive = %d", getName(), this, isInactive());
    return super::terminateClient(client, options);
}
#endif


void
AppleUSBMouse::DecrementOutstandingIO(void)
{
    if (!_gate)
    {
	if (!--_outstandingIO && _needToClose)
	{
	    USBLog(3, "%s[%p]::DecrementOutstandingIO isInactive = %d, outstandingIO = %d - closing device", getName(), this, isInactive(), _outstandingIO);
	    _interface->close(this);
	}
	return;
    }
    _gate->runAction(ChangeOutstandingIO, (void*)-1);
}


void
AppleUSBMouse::IncrementOutstandingIO(void)
{
    if (!_gate)
    {
	_outstandingIO++;
	return;
    }
    _gate->runAction(ChangeOutstandingIO, (void*)1);
}


IOReturn
AppleUSBMouse::ChangeOutstandingIO(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
    AppleUSBMouse *me = OSDynamicCast(AppleUSBMouse, target);
    UInt32	direction = (UInt32)param1;
    
    if (!me)
    {
	USBLog(1, "AppleUSBMouse::ChangeOutstandingIO - invalid target");
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
		USBLog(3, "%s[%p]::ChangeOutstandingIO isInactive = %d, outstandingIO = %d - closing device", me->getName(), me, me->isInactive(), me->_outstandingIO);
		me->_interface->close(me);
	    }
	    break;
	    
	default:
	    USBLog(1, "%s[%p]::ChangeOutstandingIO - invalid direction", me->getName(), me);
    }
    return kIOReturnSuccess;
}

//=============================================================================================
//
//  PowerDownHandler
//	When OSX starts up the Click mouse, it switches into 800 dpi, 16 bit report mode for better
//	response. When we restart back into OS 9 AND the Click mouse is plugged into the root hub,
//	it will not be powered down and will remain in the 800 dpi mode. This makes the cursor move
//	too rapidly in 9. The fix is to switch it back to 400 dpi and 8 bit report mode. We only have
//	to do this when we are going to restart since that is the only time we can get to OS 9 without
//	powering down. Since we only registerPrioritySleepWakeInterest() when the Click mouse is being
//	switched to 800 dpi mode, anytime we get here, we know we may need that switchback command.
//
//=============================================================================================
//
IOReturn 
AppleUSBMouse::PowerDownHandler(void *target, void *refCon, UInt32 messageType, IOService *service,
                                void *messageArgument, vm_size_t argSize )
{
    IOUSBDevRequest 	devReq;
    IOReturn		err = kIOReturnUnsupported;
    AppleUSBMouse *	me = OSDynamicCast(AppleUSBMouse, (OSObject *)target);

    if (!me)
        return err;

    switch (messageType)
    {
        case kIOMessageSystemWillRestart:
            // Tell the driver (using a static variable that will survive across termination)
            // that we don't want to switch to 800 dpi on the next driver start
            //
            switchTo800dpi = false;

            // Send switch back command.
            devReq.bmRequestType = 0x40;
            devReq.bRequest = 0x01;
            devReq.wValue = 0x05AC;
            devReq.wIndex = 0x0052;		// switch = 0452; switchback = 0052
            devReq.wLength = 0x0000;
            devReq.pData = NULL;
        
            err = (me)->_device->DeviceRequest(&devReq, 5000, 0);

            break;
            
        default:
            // We don't care about any other message that comes in here.
            break;
    }
    
    // Allow shutdown to go on no matter what Click mouse is doing.
    return err;
}
