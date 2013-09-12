/*
 * Copyright © 1998-2013, Apple Inc.  All rights reserved.
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

#include <IOKit/usb/IOUSBLog.h>
#include <IOKit/IOMessage.h>
#include <IOKit/pwr_mgt/RootDomain.h>

#define FORMOUSETESTING 0

#include "AppleUSBOpticalMouse.h"

#define super IOUSBHIDDriver

#define kDefaultFixedResolution (400 << 16)

/* Convert USBLog to use kprintf debugging */
#define AppleUSBOpticalMouse_USE_KPRINTF 0

#if AppleUSBOpticalMouse_USE_KPRINTF
#undef USBLog
#undef USBError
void kprintf(const char *format, ...)
__attribute__((format(printf, 1, 2)));
#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= AppleUSBOpticalMouse_USE_KPRINTF) { kprintf( FORMAT "\n", ## ARGS ) ; }
#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif
OSDefineMetaClassAndStructors(AppleUSBOpticalMouse, super)

static	bool switchTo800dpi = true;

IOReturn
AppleUSBOpticalMouse::StartFinalProcessing()
{
    OSNumber 		*curResPtr, *resPrefPtr;
    UInt32			curResInt, resPrefInt;
    IOFixed			curRes, resPref;
    IOReturn		err = kIOReturnSuccess;
	
    USBLog(3, "AppleUSBOpticalMouse[%p]::StartFinalProcessing", this);
	_switchBackOnRestart = FALSE;
    
    _switchTo800dpiFlag = ( getProperty("SwitchTo800DPI") == kOSBooleanTrue );
    _switchTo2000fpsFlag = ( getProperty("SwitchTo2000FPS") == kOSBooleanTrue );

    
    if ( _switchTo2000fpsFlag )
    {
        IOUSBDevRequest		devReq;
        
        // Write the 2000 FPS value to the mouse
        //
        devReq.bmRequestType = 0x40;
        devReq.bRequest = 0x01;
        devReq.wValue = 0x05AC;
        devReq.wIndex = 0xd810;
        devReq.wLength = 0x0000;
        devReq.pData = NULL;

        err = _device->DeviceRequest(&devReq, 5000, 0);

        if (err)
		{
            USBLog(3, "AppleUSBOpticalMouse[%p]::StartFinalProcessing - sending 1st part of FPS change received error 0x%x", this, err);
		}
        else
        {
            devReq.bmRequestType = 0x40;
            devReq.bRequest = 0x01;
            devReq.wValue = 0x05AC;
            devReq.wIndex = 0xdc11;
            devReq.wLength = 0x0000;
            devReq.pData = NULL;

            err = _device->DeviceRequest(&devReq, 5000, 0);

            if (err)
			{
                USBLog(3, "AppleUSBOpticalMouse[%p]::StartFinalProcessing - sending 2nd part of FPS change received error 0x%x", this, err);
			}
        }

#if FORMOUSETESTING
        UInt8			hi,lo;
        UInt16			fps;

        // Read back the value:
        //
        devReq.bmRequestType = 0xc0;
        devReq.bRequest = 0x01;
        devReq.wValue = 0x05AC;
        devReq.wIndex = 0x0011;
        devReq.wLength = 1;
        devReq.pData = &hi;

        err = _device->DeviceRequest(&devReq, 5000, 0);
        if (err)
            USBLog(3, "AppleUSBOpticalMouse[%p]::StartFinalProcessing - error reading hi byte: 0x%x", this, err);

        devReq.bmRequestType = 0xc0;
        devReq.bRequest = 0x01;
        devReq.wValue = 0x05AC;
        devReq.wIndex = 0x0010;
        devReq.wLength = 1;
        devReq.pData = &lo;

        err = _device->DeviceRequest(&devReq, 5000, 0);
        if (err)
            USBLog(3, "AppleUSBOpticalMouse[%p]::StartFinalProcessing - read reading lo byte: 0x%x", this, err);

        fps = hi;
        fps = (fps << 8) | lo;

        USBLog(3, "AppleUSBOpticalMouse[%p]::StartFinalProcessing - read : 0x%x", this, fps );

#endif
        
        err = super::StartFinalProcessing();
        if (err)
		{
            USBLog(3, "AppleUSBOpticalMouse[%p]::StartFinalProcessing - super returned error 0x%x", this, err);
		}

    }

    if ( _switchTo800dpiFlag )
    {
        OSObject    *propertyObj;
        
		propertyObj = copyProperty(kIOHIDPointerResolutionKey);
        curResPtr = OSDynamicCast( OSNumber, propertyObj );
        if (curResPtr)
        {
            curResInt = curResPtr->unsigned32BitValue();
            USBLog(3, "AppleUSBOpticalMouse[%p]::StartFinalProcessing - found current resolution property - value 0x%x", this, (uint32_t)curResInt);
        }
        else
        {
            curResInt = kDefaultFixedResolution;
            USBLog(3, "AppleUSBOpticalMouse[%p]::StartFinalProcessing - no current property found - using default 0x%x", this, (uint32_t)curResInt);
        }
		if (propertyObj)
			propertyObj->release();

		propertyObj = copyProperty(("xResolutionPref"));
        resPrefPtr = OSDynamicCast( OSNumber, propertyObj );
        if (resPrefPtr)
            resPrefInt = resPrefPtr->unsigned32BitValue();
        else
        {
            resPrefInt = kDefaultFixedResolution * 2;
            USBLog(3, "AppleUSBOpticalMouse[%p]::StartFinalProcessing - no preference property found - using default 0x%x", this, (uint32_t)resPrefInt);
        }
		if (propertyObj)
			propertyObj->release();
    
        resPref = (IOFixed) resPrefInt;
        curRes = (IOFixed) curResInt;
    
        if (resPref != curRes)
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
                    USBLog(3, "AppleUSBOpticalMouse[%p]::StartFinalProcessing - error (%x) setting resolution", this, err);
				}
                else
				{
					// with this mouse, we do NOT want to start reading on the interrupt pipe, nor do
					// we want to call super::start. We just want to wait for the device to get terminated
                    USBLog(3, "AppleUSBOpticalMouse[%p]::StartFinalProcessing - waiting for click mouse termination", this);
				}
            }
        }
        else
        {
            // If we are already at the correct resolution for OSX, OK. But what if we are going
            // back to OS 9? On restart, switch back to boot setup. Power Manager will tell us
            // when we are going to restart.
            //
			_switchBackOnRestart = TRUE;
            err = super::StartFinalProcessing();
            if (err)
            {
				USBLog(1, "AppleUSBOpticalMouse[%p]::StartFinalProcessing - error (%p) from super::StartFinalProcessing", this, (void*)err);
            }
        }
    }
    
    return err;
}




bool
AppleUSBOpticalMouse::willTerminate( IOService * provider, IOOptionBits options )
{
    // this method is intended to be used to stop any pending I/O and to make sure that 
    // we have begun getting our callbacks in order. by the time we get here, the 
    // isInactive flag is set, so we really are marked as being done. we will do in here
    // what we used to do in the message method (this happens first)
    USBLog(3, "AppleUSBOpticalMouse[%p]::willTerminate isInactive = %d", this, isInactive());
    
    return super::willTerminate(provider, options);
}


IOReturn
AppleUSBOpticalMouse::setPowerState ( unsigned long powerStateOrdinal, IOService* whatDevice )
{
	USBLog(5, "AppleUSBOpticalMouse[%p]::setPowerState- powerStateOrdinal[%d] _switchBackOnRestart[%s]", this, (int)powerStateOrdinal, _switchBackOnRestart ? "true" : "false");
	if ((powerStateOrdinal == kUSBHIDPowerStateRestart) && _switchBackOnRestart)
	{
		IOUSBDevRequest		devReq;
		IOReturn			err;
		
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
        
		USBLog(5, "AppleUSBOpticalMouse[%p]::setPowerState - issuing command to switch back", this);
		err = _device->DeviceRequest(&devReq, 5000, 0);
		if (err)
		{
			USBLog(1, "AppleUSBOpticalMouse[%p]::setPowerState - err (%p) on DeviceRequest", this, (void*)err);
		}
		else
		{
			USBLog(7, "AppleUSBOpticalMouse[%p]::setPowerState - command done with no err", this);
		}
	}
	return super::setPowerState(powerStateOrdinal, whatDevice);
}

