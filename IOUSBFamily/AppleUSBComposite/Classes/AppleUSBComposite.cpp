/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

#include <libkern/OSByteOrder.h>
#include <IOKit/IOMessage.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBLog.h>
#include "AppleUSBComposite.h"

#include <IOKit/assert.h>

#define super IOService

OSDefineMetaClassAndStructors(AppleUSBComposite, IOService)

bool 
AppleUSBComposite::start(IOService * provider)
{
    bool 	configured = false;
    
    if( !super::start(provider))
        return (false);

    // get my device
    _device = OSDynamicCast(IOUSBDevice, provider);
    if(!_device)
	return false;

    _expectingClose = false;
    _notifier = NULL;
    
    configured = ConfigureDevice();
    if ( configured )
    {
        // Create the general interest notifier so we get notifications of people attempting to open our device.  We will only
        // close the device when we get a requestingClose message through our message method().  Note that we are registering
        // an interest on the _device, not on ourselves.  We do this because that's who the open/close semantics are about, the IOUSBDevice
        // and not us, the driver.
        // 
        _notifier = _device->registerInterest( gIOGeneralInterest, &AppleUSBComposite::CompositeDriverInterestHandler, this, NULL ); 
    }
    
    USBLog(5, "%s::start returning %d",getName(), configured);
    return configured;
            
}

IOReturn
AppleUSBComposite::CompositeDriverInterestHandler(  void * target, void * refCon, UInt32 messageType, IOService * provider,
                                 void * messageArgument, vm_size_t argSize )
{
    AppleUSBComposite *	me = (AppleUSBComposite *) target;

    if (!me)
    {
        return kIOReturnError;
    }
    
    // IOLog("CompositeDriverInterestHandler: target: %p, refCon: %p: messageType: 0x%lx, provider: %p, arg: %p, size: 0x%x\n", target, refCon, messageType, provider, messageArgument, argSize);
    
    switch ( messageType )
    {
        case kIOMessageServiceIsAttemptingOpen:
            // The messagArgument for this message has the IOOptions passed in to the open() call that caused this message.  If the options are
            // kIOServiceSeize, we now that someone really tried to open it.  In the kernel, we will also get a message() call with a kIIOServiceRequestingClose
            // message, so we deal with closing the driver in that message.  
            //
            USBLog(5, "CompositeDriverInterestHandler received kIOMessageServiceIsAttemptingOpen with argument: %d", (int) messageArgument );
            break;
            
        case kIOMessageServiceWasClosed:
            USBLog(5, "CompositeDriverInterestHandler received kIOMessageServiceWasClosed (expecting close = %d)", me->_expectingClose);
            me->_expectingClose = false;
            break;
            
        case kIOMessageServiceIsTerminated:
        case kIOUSBMessagePortHasBeenReset:
            break;
        
        default:
            USBLog(5, "CompositeDriverInterestHandler message unknown: 0x%lx", messageType);
    }
        
    return kIOReturnSuccess;
    
}

bool
AppleUSBComposite::ConfigureDevice()
{
    IOReturn	err = kIOReturnSuccess;
    UInt8	prefConfigValue;
    OSNumber *	prefConfig = NULL;
    
    do {
        USBLog(3,"%s[%p]: USB Generic Composite @ %d", getName(), this, _device->GetAddress());

        // Find if we have a Preferred Configuration
        //
        prefConfig = (OSNumber *) getProperty("Preferred Configuration");
        if ( prefConfig )
        {
            prefConfigValue = prefConfig->unsigned32BitValue();
            USBLog(3, "%s[%p](%s) found a preferred configuration (%d)", getName(), this, _device->getName(), prefConfigValue );
            // Now, figure out if we have enough power for this config
            //
        }

        // No preferred configuration so, find the first config/interface
        //
        if (_device->GetNumConfigurations() < 1)
        {
            USBError(1, "%s[%p](%s) Could not get any configurations", getName(), this, _device->getName() );
            err = kIOUSBConfigNotFound;
            continue;
        }

        // set the configuration to the first config
        //
        const IOUSBConfigurationDescriptor *cd = _device->GetFullConfigurationDescriptor(0);
        if (!cd)
        {
            USBLog(1, "%s[%p](%s) GetFullConfigDescriptor(0) returned NULL, retrying", getName(), this, _device->getName() );
            IOSleep( 300 );
            cd = _device->GetFullConfigurationDescriptor(0);
            if ( !cd )
            {
                USBError(1, "%s[%p](%s) GetFullConfigDescriptor(0) returned NULL", getName(), this, _device->getName() );
                break;
            }
        }
            
	
	if (!_device->open(this))
	{
            USBError(1, "%s[%p](%s) Could not open device", getName(), this, _device->getName() );
	    break;
	}
	err = _device->SetConfiguration(this, (prefConfig ? prefConfigValue : cd->bConfigurationValue), true);
	if (err)
	{
            USBError(1, "%s[%p](%s) SetConfiguration (%d) returned 0x%x", getName(), this, _device->getName(), (prefConfig ? prefConfigValue : cd->bConfigurationValue), err );
            
            // If we used a "Preferred Configuration" then attempt to set the configuration to the default one:
            //
            if ( prefConfig )
            {
                err = _device->SetConfiguration(this, cd->bConfigurationValue, true);
                USBError(1, "%s[%p](%s) SetConfiguration (%d) returned 0x%x", getName(), this, _device->getName(), cd->bConfigurationValue, err );
            }
            
            if ( err )
            {
                _device->close(this);
                break;
            }
	}
        
        // Set the remote wakeup feature if it's supported
        //
        if (cd->bmAttributes & kUSBAtrRemoteWakeup)
        {
            USBLog(3,"%s[%p] Setting kUSBFeatureDeviceRemoteWakeup for device: %s", getName(), this, _device->getName());
            _device->SetFeature(kUSBFeatureDeviceRemoteWakeup);
        }
        
	// Let's close the device
        _expectingClose = true;
        _device->close(this);
        
        return true;

    } while (false);

    USBLog(3, "%s[%p] aborting startup (0x%x)", getName(), this, err );
    
    return false;

}

IOReturn
AppleUSBComposite::ReConfigureDevice()
{
    // IOUSBDevRequest	request;
    IOReturn 		err = kIOReturnSuccess;
    IOUSBDevRequest	request;
    const 		IOUSBConfigurationDescriptor *	cd;
       
    // Clear out the structure for the request
    //
    bzero( &request, sizeof(IOUSBDevRequest));

    // if we're not opened, then we need to open the device before we
    // can reconfigure it.  If that fails, then we need to seize the device and hope that
    // whoever has it will close it
    //
    if ( _device && !_device->isOpen(this) )
    {
         // Let's attempt to open the device
        //
        if ( !_device->open(this) )
        {
            // OK, since we can't open it, we give up.  Note that we will not attempt to
            // seize it -- that's too much.  If somebody has it open, then we shouldn't continue
            // with the reset.  Such is the case with Classic:  they open the device and do a DeviceReset
            // but they don't expect us to actually configure the device.
            //
            USBLog(3, "%s[%p]::ReConfigureDevice.  Can't open it, giving up",getName(), this);
            err = kIOReturnExclusiveAccess;
            goto ErrorExit;
        }
    }
    
    // We have the device open, so now reconfigure it
    //
   
    // Find the first config/interface
    if (_device->GetNumConfigurations() < 1)
    {
        USBLog(3, "%s[%p]::ReConfigureDevice.  no configurations",getName(), this);
        err = kIOUSBConfigNotFound;
        goto ErrorExit;
    }

    // set the configuration to the first config
    cd = _device->GetFullConfigurationDescriptor(0);
    if (!cd)
    {
        USBLog(1, "%s[%p](%s) GetFullConfigDescriptor(0) returned NULL, retrying", getName(), this, _device->getName() );
        IOSleep( 300 );
        cd = _device->GetFullConfigurationDescriptor(0);
        if ( !cd )
        {
            USBError(1, "%s[%p](%s) GetFullConfigDescriptor(0) returned NULL", getName(), this, _device->getName() );
            err = kIOUSBConfigNotFound;
            goto ErrorExit;
        }
    }
    
    // Send the SET_CONFIG request on the bus
    //
    request.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBStandard, kUSBDevice);
    request.bRequest = kUSBRqSetConfig;
    request.wValue = cd->bConfigurationValue;
    request.wIndex = 0;
    request.wLength = 0;
    request.pData = 0;
    err = _device->DeviceRequest(&request, 5000, 0);
    
    // Set the remote wakeup feature if it's supported
    //
    if (cd->bmAttributes & kUSBAtrRemoteWakeup)
    {
        USBLog(3,"%s[%p]::ReConfigureDevice Setting kUSBFeatureDeviceRemoteWakeup for device: %s", getName(), this, _device->getName());
        _device->SetFeature(kUSBFeatureDeviceRemoteWakeup);
    }

    if (err)
    {
        USBLog(3, "%s[%p]::ReConfigureDevice.  SET_CONFIG returned 0x%x",getName(), this, err);
    }
    
ErrorExit:

    USBLog(3, "%s[%p]::ReConfigureDevice returned 0x%x",getName(),this, err);
    return err;
}


IOReturn 
AppleUSBComposite::message( UInt32 type, IOService * provider,  void * argument )
{
    IOReturn 	err = kIOReturnSuccess;
    
    err = super::message (type, provider, argument);
    
    switch ( type )
    {
        case kIOUSBMessagePortHasBeenReset:
            // Should we do something here if we get an error?
            //
            USBLog(5, "%s[%p]::message - received kIOUSBMessagePortHasBeenReset",getName(), this);
            err = ReConfigureDevice();
            break;
            
        case kIOMessageServiceIsTerminated:
            USBLog(5, "%s[%p]::message - received kIOMessageServiceIsTerminated",getName(), this);
            break;
            
        case kIOMessageServiceIsRequestingClose:
            // Someone really wants us to close, so let's close our device:
            if ( _device && _device->isOpen(this) )
            {
                USBLog(3, "%s[%p]::message - Received kIOMessageServiceIsRequestingClose - closing device",getName(), this);
                _expectingClose = true;
                _device->close(this);
            }
            else
            {
                err = kIOReturnNotOpen;
            }
            break;
            
        default:
            break;
    }
    
    return err;
}

bool
AppleUSBComposite::willTerminate( IOService * provider, IOOptionBits options )
{
    USBLog(5, "%s[%p]::willTerminate isInactive = %d", getName(), this, isInactive());

    // Clean up our notifier.  That will release it
    //
    if ( _notifier )
        _notifier->remove();
    
    return super::willTerminate(provider, options);
}


bool
AppleUSBComposite::didTerminate( IOService * provider, IOOptionBits options, bool * defer )
{
    USBLog(5, "%s[%p]::didTerminate isInactive = %d", getName(), this, isInactive());
    // if we are still hanging on to the device, go ahead and close it
    if (_device->isOpen(this))
	_device->close(this);
	
    return super::didTerminate(provider, options, defer);
}

#if 0
void 
AppleUSBComposite::stop(IOService * provider)
{
    USBLog(3, "%s[%p]::stop isInactive = %d", getName(), this, isInactive());
    return(super::stop(provider));
}

bool 
AppleUSBComposite::finalize(IOOptionBits options)
{
    USBLog(3, "%s[%p]::finalize isInactive = %d", getName(), this, isInactive());
    return(super::finalize(options));
}


bool 	
AppleUSBComposite::requestTerminate( IOService * provider, IOOptionBits options )
{
    USBLog(3, "%s[%p]::requestTerminate isInactive = %d", getName(), this, isInactive());
    return super::requestTerminate(provider, options);
}


bool
AppleUSBComposite::terminate( IOOptionBits options = 0 )
{
    USBLog(3, "%s[%p]::terminate isInactive = %d", getName(), this, isInactive());
    return super::terminate(options);
}


void
AppleUSBComposite::free( void )
{
    USBLog(3, "%s[%p]::free isInactive = %d", getName(), this, isInactive());
    super::free();
}


bool
AppleUSBComposite::terminateClient( IOService * client, IOOptionBits options )
{
    USBLog(3, "%s[%p]::terminateClient isInactive = %d", getName(), this, isInactive());
    return super::terminateClient(client, options);
}
#endif

