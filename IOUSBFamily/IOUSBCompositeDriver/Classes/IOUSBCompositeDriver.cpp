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


//================================================================================================
//
//   Headers
//
//================================================================================================
//
#include <IOKit/usb/IOUSBCompositeDriver.h>
#include "USBTracepoints.h"

//================================================================================================
//
//   Declare the AppleUSBComposite class so that bundles that instantiated it before the name
//   change will still work.  We keep the same CFBundleIdentifier
//
//================================================================================================
//
class AppleUSBComposite : public IOUSBCompositeDriver
{
    OSDeclareDefaultStructors(AppleUSBComposite)
};

OSDefineMetaClassAndStructors(AppleUSBComposite, IOUSBCompositeDriver)


//================================================================================================
//
//   Local Definitions
//
//================================================================================================
//
#define super IOService

/* Convert USBLog to use kprintf debugging */
#ifndef IOUSBCOMPOSITEDRIVER_USE_KPRINTF
#define IOUSBCOMPOSITEDRIVER_USE_KPRINTF 0
#endif

#if IOUSBCOMPOSITEDRIVER_USE_KPRINTF
#undef USBLog
#undef USBError
void kprintf(const char *format, ...)
__attribute__((format(printf, 1, 2)));
#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= IOUSBCOMPOSITEDRIVER_USE_KPRINTF) { kprintf( FORMAT "\n", ## ARGS ) ; }
#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif

//================================================================================================
//
//   IOUSBCompositeDriver Methods
//
//================================================================================================
//
OSDefineMetaClassAndStructors(IOUSBCompositeDriver, IOService)



#pragma mark ееееееее IOService Methods еееееееее
//=============================================================================================
//
//  start
//
//=============================================================================================
//
bool 
IOUSBCompositeDriver::start(IOService * provider)
{
    bool 	configured = false;
    
    if ( !super::start(provider))
        return (false);
    
    // Save a copy of our USB Device
    //
    fDevice = OSDynamicCast(IOUSBDevice, provider);
    if (!fDevice)
	return false;
    
    fExpectingClose = false;
    fNotifier = NULL;
    
    configured = ConfigureDevice();
    if ( configured )
    {
        // Create the general interest notifier so we get notifications of people attempting to open our device.  We will only
        // close the device when we get a requestingClose message through our message method().  Note that we are registering
        // an interest on the fDevice, not on ourselves.  We do this because that's who the open/close semantics are about, the IOUSBDevice
        // and not us, the driver.
        // 
        fNotifier = fDevice->registerInterest( gIOGeneralInterest, &IOUSBCompositeDriver::CompositeDriverInterestHandler, this, NULL ); 

        USBLog(3,"%s[%p]::start USB Generic Composite @ %d", getName(), this, fDevice->GetAddress());
    }
    
    USBLog(5, "%s[%p]::start returning %d", getName(), this, configured);
    
    return configured;
}


//=============================================================================================
//
//  init
//
//=============================================================================================
//
bool 
IOUSBCompositeDriver::init(OSDictionary * propTable)
{
    if (!super::init(propTable))  return false;
	
    // allocate our expansion data
    if (!fIOUSBCompositeExpansionData)
    {
		fIOUSBCompositeExpansionData = (IOUSBCompositeDriverExpansionData *)IOMalloc(sizeof(fIOUSBCompositeExpansionData));
		if (!fIOUSBCompositeExpansionData)
			return false;
		bzero(fIOUSBCompositeExpansionData, sizeof(fIOUSBCompositeExpansionData));
    }
	
	return true;
}


//=============================================================================================
//
//  free
//
//=============================================================================================
//
void
IOUSBCompositeDriver::free()
{
    USBLog(6,"+IOUSBCompositeDriver[%p]::free", this);
	
    //  This needs to be the LAST thing we do, as it disposes of our "fake" member
    //  variables.
    //
    if (fIOUSBCompositeExpansionData)
    {
        IOFree(fIOUSBCompositeExpansionData, sizeof(fIOUSBCompositeExpansionData));
        fIOUSBCompositeExpansionData = NULL;
    }
	
	USBLog(6, "-IOUSBCompositeDriver[%p]::free", this);
	
    super::free();
}


//=============================================================================================
//
//  message
//
//=============================================================================================
//
IOReturn 
IOUSBCompositeDriver::message( UInt32 type, IOService * provider,  void * argument )
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
            
		case kIOUSBMessageCompositeDriverReconfigured:
            USBLog(5, "%s[%p]::message - received kIOUSBMessageCompositeDriverReconfigured",getName(), this);
            break;
			
        case kIOMessageServiceIsRequestingClose:
            // Someone really wants us to close, so let's close our device:
            if ( fDevice && fDevice->isOpen(this) )
            {
                USBLog(3, "%s[%p]::message - Received kIOMessageServiceIsRequestingClose - closing device",getName(), this);
                fExpectingClose = true;
                fDevice->close(this);
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


//=============================================================================================
//
//  willTerminate
//
//=============================================================================================
//
bool
IOUSBCompositeDriver::willTerminate( IOService * provider, IOOptionBits options )
{
    USBLog(6, "%s[%p]::willTerminate isInactive = %d", getName(), this, isInactive());
    
    // Clean up our notifier.  That will release it
    //
    if ( fNotifier )
        fNotifier->remove();
    
    return super::willTerminate(provider, options);
}


//=============================================================================================
//
//  didTerminate
//
//=============================================================================================
//
bool
IOUSBCompositeDriver::didTerminate( IOService * provider, IOOptionBits options, bool * defer )
{
    USBLog(6, "%s[%p]::didTerminate isInactive = %d", getName(), this, isInactive());
    // if we are still hanging on to the device, go ahead and close it
    if (fDevice->isOpen(this))
	fDevice->close(this);
    
    return super::didTerminate(provider, options, defer);
}


#pragma mark ееееееее IOUSBCompositeDriver Methods еееееееее
//=============================================================================================
//
//  ConfigureDevice
//
//=============================================================================================
//
bool
IOUSBCompositeDriver::ConfigureDevice()
{
    IOReturn                                err = kIOReturnSuccess;
    UInt8                                   prefConfigValue = 0;
    OSNumber *                              prefConfig = NULL;
    const IOUSBConfigurationDescriptor *    cd = NULL;
    const IOUSBConfigurationDescriptor *    cdTemp = NULL;
    UInt8                                   i;
    SInt16                                  maxPower = -1;
    UInt8                                   numberOfConfigs = 0;
	OSBoolean *								suspendPropertyRef;
	OSBoolean *								expressCardCantWakeRef;
	OSBoolean *								lowPowerNotificationDisplayed;
	bool									issueRemoteWakeup = false;
    
   // Find if we have a Preferred Configuration
    //
    prefConfig = (OSNumber *) getProperty(kUSBPreferredConfiguration);
    if ( prefConfig )
    {
        prefConfigValue = prefConfig->unsigned32BitValue();
        USBLog(3, "%s[%p](%s) found a preferred configuration (%d)", getName(), this, fDevice->getName(), prefConfigValue );
    }
	else
	{
		// Try the IOUSBBDevice
		prefConfig = (OSNumber *) fDevice->getProperty(kUSBPreferredConfiguration);
		if ( prefConfig )
		{
			prefConfigValue = prefConfig->unsigned32BitValue();
			USBLog(3, "%s[%p](%s) found a preferred configuration (%d)", getName(), this, fDevice->getName(), prefConfigValue );
		}
	}
    
    // No preferred configuration so, find the first config/interface
    //
    numberOfConfigs = fDevice->GetNumConfigurations();
    if ( numberOfConfigs < 1)
    {
        USBError(1, "%s[%p](%s) Could not get any configurations", getName(), this, fDevice->getName() );
        err = kIOUSBConfigNotFound;
        goto ErrorExit;
    }
    
    if (numberOfConfigs > 1)
    {
        // We have more than 1 configuration.  Select the one with the highest power that is available in the port.  This presumes
        // that such a configuration is more desirable.
        //
        for (i = 0; i < numberOfConfigs; i++) 
        {
            cdTemp = fDevice->GetFullConfigurationDescriptor(i);
            if (!cdTemp)
            {
				USBLog(1, "%s[%p](%s)::ConfigureDevice GetFullConfigDescriptor(%d) returned NULL, retrying", getName(), this, fDevice->getName(), i );
				USBTrace( kUSBTCompositeDriver, kTPCompositeDriverConfigureDevice , (uintptr_t)this, i, 0, 1 );
				IOSleep( 300 );
				cdTemp = fDevice->GetFullConfigurationDescriptor(i);
				if ( !cdTemp )
				{
					USBError(1, "%s[%p](%s)::ConfigureDevice GetFullConfigDescriptor(%d) returned NULL", getName(), this, fDevice->getName(), i );
					err = kIOUSBConfigNotFound;
					goto ErrorExit;
				}
            }
            
            // Get the MaxPower for this configuration.  If we have enough power for it AND it's greater than our previous power
            // then use this config
            if ( (fDevice->GetBusPowerAvailable() >= cdTemp->MaxPower) && ( ((SInt16)cdTemp->MaxPower) > maxPower) )
            {
                USBLog(5,"%s[%p](%s) ConfigureDevice Config %d with MaxPower %d", getName(), this, fDevice->getName(), i, cdTemp->MaxPower );
                cd = cdTemp;
                maxPower = (SInt16) cdTemp->MaxPower;
            }
            else
            {
                USBLog(5,"%s[%p](%s) ConfigureDevice Config %d with MaxPower %d cannot be used (available: %d, previous %d)", getName(), this, fDevice->getName(), i, cdTemp->MaxPower, (uint32_t)fDevice->GetBusPowerAvailable(), maxPower );
				fDevice->setProperty("Failed Requested Power", cdTemp->MaxPower, 32);
          }
        }
        
		if ( !cd )
        {
			USBError(1,"USB Low Power Notice:  The device \"%s\" cannot be used because there is not enough power to configure it",fDevice->getName());
            USBLog(3, "%s[%p](%s) ConfigureDevice failed to find configuration by power", getName(), this, fDevice->getName() );
            err = kIOUSBNotEnoughPowerErr;
			lowPowerNotificationDisplayed = OSDynamicCast( OSBoolean, fDevice->getProperty("Low Power Displayed") );
			if ( !lowPowerNotificationDisplayed or (lowPowerNotificationDisplayed && lowPowerNotificationDisplayed->isFalse()) )
			{
				bool	display = true;
				fDevice->DisplayUserNotification(kUSBNotEnoughPowerNotificationType);
				fDevice->setProperty("Low Power Displayed", display);
			}
            goto ErrorExit;
			
		}
    }
    else
    {
        // set the configuration to the first config
        //
        cd = fDevice->GetFullConfigurationDescriptor(0);
        if (!cd)
        {
            USBLog(1, "%s[%p](%s) GetFullConfigDescriptor(0) returned NULL, retrying", getName(), this, fDevice->getName() );
			USBTrace( kUSBTCompositeDriver, kTPCompositeDriverConfigureDevice , (uintptr_t)this, 0, 0, 2 );
            IOSleep( 300 );
            cd = fDevice->GetFullConfigurationDescriptor(0);
            if ( !cd )
            {
                USBError(1, "%s[%p](%s) GetFullConfigDescriptor(0) returned NULL", getName(), this, fDevice->getName() );
                err = kIOUSBConfigNotFound;
                goto ErrorExit;
            }
        }
    }
    
    // Open our device so that we can configure it
    //
    if ( !fDevice->open(this) )
    {
        USBError(1, "%s[%p] Could not open device (%s)", getName(), this, fDevice->getName() );
        err = kIOReturnExclusiveAccess;
        goto ErrorExit;
    }
    
    // Save our configuration value
    //
    fConfigValue = prefConfig ? prefConfigValue : cd->bConfigurationValue;
    
    // Get the remote wakeup feature if it's supported (there is a bug here where if we have a prefConfig, we are not looking for
	// the atributes of the pref config, but instead we look at the default's config attributes)
    //
    fConfigbmAttributes = cd->bmAttributes;
    if (fConfigbmAttributes & kUSBAtrRemoteWakeup)
    {
		fIOUSBCompositeExpansionData->fIssueRemoteWakeup = true;
	}
	
    // Now, configure it
    //
    err = SetConfiguration(fConfigValue, true);
    if (err)
    {
        USBError(1, "%s[%p](%s) SetConfiguration (%d) returned 0x%x", getName(), this, fDevice->getName(), (prefConfig ? prefConfigValue : cd->bConfigurationValue), err );
        
        // If we tried a "Preferred Configuration" then attempt to set the configuration to the default one:
        //
        if ( prefConfig )
        {
            fConfigValue = cd->bConfigurationValue;
            err = SetConfiguration(fConfigValue, true);
            USBError(1, "%s[%p](%s) SetConfiguration (%d) returned 0x%x", getName(), this, fDevice->getName(), cd->bConfigurationValue, err );
        }
        
        if ( err )
        {
            fDevice->close(this);
            goto ErrorExit;
        }
    }
    
	if (!fIOUSBCompositeExpansionData->fRemoteWakeupIssued)
	{
		// Set the remote wakeup feature if it's supported
		//
		fConfigbmAttributes = cd->bmAttributes;
		if (fConfigbmAttributes & kUSBAtrRemoteWakeup)
		{
			USBLog(3,"%s[%p] Setting kUSBFeatureDeviceRemoteWakeup for device: %s", getName(), this, fDevice->getName());
			err = fDevice->SetFeature(kUSBFeatureDeviceRemoteWakeup);
			if ( err != kIOReturnSuccess )
			{
				// Wait and retry
				IOSleep(300);
				err = fDevice->SetFeature(kUSBFeatureDeviceRemoteWakeup);
			}
		}
    }
	
	// See if this is an express card device which would disconnect on sleep (thus waking everytime)
	//
	expressCardCantWakeRef = OSDynamicCast( OSBoolean, fDevice->getProperty(kUSBExpressCardCantWake) );
	if ( expressCardCantWakeRef && expressCardCantWakeRef->isTrue() )
	{
		USBLog(3, "%s[%p](%s) found an express card device which will disconnect across sleep", getName(), this, fDevice->getName() );
		fDevice->GetBus()->retain();
		fDevice->GetBus()->message(kIOUSBMessageExpressCardCantWake, this, fDevice);
		fDevice->GetBus()->release();
	}
	
	// If we have a property that tells us that we should suspend the port, do it now
	//
	suspendPropertyRef = OSDynamicCast( OSBoolean, fDevice->getProperty(kUSBSuspendPort) );
	if ( suspendPropertyRef && suspendPropertyRef->isTrue() )
	{
		USBLog(3, "%s[%p](%s) Need to suspend the port", getName(), this, fDevice->getName() );
		err = fDevice->SuspendDevice(true);
		if ( err != kIOReturnSuccess )
		{
			USBLog(3, "%s[%p](%s) SuspendDevice returned 0x%x", getName(), this, fDevice->getName(), err );
		}
	}
	
    // Let's close the device since we're done configuring it
    //
    fExpectingClose = true;
    fDevice->close(this);
    
    return true;
    
ErrorExit:
        
        USBLog(3, "%s[%p]::start aborting startup (0x%x)", getName(), this, err );
    
    return false;
    
}


//=============================================================================================
//
//  ReConfigureDevice
//
//=============================================================================================
//
IOReturn
IOUSBCompositeDriver::ReConfigureDevice()
{
    const IOUSBConfigurationDescriptor *    cd = NULL;
    const IOUSBConfigurationDescriptor *    cdTemp = NULL;
    IOReturn                                err = kIOReturnSuccess;
    IOUSBDevRequest                         request;
    UInt8                                   numberOfConfigs = 0;
    UInt32                                  i;
	OSBoolean *								suspendPropertyRef;
	OSBoolean *								expressCardCantWakeRef;
    
    // Clear out the structure for the request
    //
    bzero( &request, sizeof(IOUSBDevRequest));
    
    // if we're not opened, then we need to open the device before we
    // can reconfigure it.  If that fails, then we need to seize the device and hope that
    // whoever has it will close it
    //
    if ( fDevice && !fDevice->isOpen(this) )
    {
        // Let's attempt to open the device
        //
        if ( !fDevice->open(this) )
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
    // Send the SET_CONFIG request on the bus, using the fConfigValue we used in ConfigureDevice
    //
    request.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBStandard, kUSBDevice);
    request.bRequest = kUSBRqSetConfig;
    request.wValue = fConfigValue;
    request.wIndex = 0;
    request.wLength = 0;
    request.pData = 0;
    err = fDevice->DeviceRequest(&request, 5000, 0);
    
    if (err)
    {
        USBLog(3, "%s[%p]::ReConfigureDevice.  SET_CONFIG returned 0x%x",getName(), this, err);
		fDevice->close(this);
		
		goto ErrorExit;
    }

    // Set the remote wakeup feature if it's supported
    //
    if (fConfigbmAttributes & kUSBAtrRemoteWakeup)
    {
        USBLog(3,"%s[%p]::ReConfigureDevice Setting kUSBFeatureDeviceRemoteWakeup for device: %s", getName(), this, fDevice->getName());
        err = fDevice->SetFeature(kUSBFeatureDeviceRemoteWakeup);
        if ( err != kIOReturnSuccess )
        {
            // Wait and retry
            IOSleep(300);
            err = fDevice->SetFeature(kUSBFeatureDeviceRemoteWakeup);
        }
    }
    
	// See if this is an express card device which would disconnect on sleep (thus waking everytime)
	//
	expressCardCantWakeRef = OSDynamicCast( OSBoolean, fDevice->getProperty(kUSBExpressCardCantWake) );
	if ( expressCardCantWakeRef && expressCardCantWakeRef->isTrue() )
	{
		USBLog(3, "%s[%p](%s) found an express card device which will disconnect across sleep", getName(), this, fDevice->getName() );
		fDevice->GetBus()->retain();
		fDevice->GetBus()->message(kIOUSBMessageExpressCardCantWake, this, fDevice);
		fDevice->GetBus()->release();
	}
	
	// If we have a property that tells us that we should suspend the port, do it now
	//
	suspendPropertyRef = OSDynamicCast( OSBoolean, fDevice->getProperty(kUSBSuspendPort) );
	if ( suspendPropertyRef && suspendPropertyRef->isTrue() )
	{
		USBLog(3, "%s[%p](%s) Need to suspend the port", getName(), this, fDevice->getName() );
		err = fDevice->SuspendDevice(true);
		if ( err != kIOReturnSuccess )
		{
			USBLog(3, "%s[%p](%s) SuspendDevice returned 0x%x", getName(), this, fDevice->getName(), err );
		}
	}
	
	// Make sure we close our provider so that others can open it.  Do this before calling them with the kIOUSBMessageCompositeDriverReconfigured
	// message in case they need to open the device for some reason.
	fDevice->close(this);
    
	// If we are succesful, ask our provider, the IOUSBDevice, to message its clients.  Make sure to retain it during the call in case the device
	// goes away.
	fDevice->retain();
	(void) fDevice->messageClients(kIOUSBMessageCompositeDriverReconfigured, NULL, 0);
	fDevice->release();

ErrorExit:
        
    USBLog(6, "%s[%p]::ReConfigureDevice returned 0x%x",getName(),this, err);
    return err;
}


//=============================================================================================
//
//  SetConfiguration
//
//=============================================================================================
//
IOReturn
IOUSBCompositeDriver::SetConfiguration(UInt8 configValue, bool startInterfaceMatching)
{
	// Call the device object to do the configuration
	//
	if ( fIOUSBCompositeExpansionData && fIOUSBCompositeExpansionData->fIssueRemoteWakeup )
	{
		fIOUSBCompositeExpansionData->fRemoteWakeupIssued = true;
		return fDevice->SetConfiguration(this, configValue, startInterfaceMatching, true);
	}
	else
	{
		fIOUSBCompositeExpansionData->fRemoteWakeupIssued = false;
		return fDevice->SetConfiguration(this, configValue, startInterfaceMatching);
	}
}



#pragma mark ееееееее Static Methods еееееееее
//=============================================================================================
//
//  CompositeDriverInterestHandler
//
//=============================================================================================
//
IOReturn
IOUSBCompositeDriver::CompositeDriverInterestHandler(  void * target, void * refCon, UInt32 messageType, IOService * provider,
                                                       void * messageArgument, vm_size_t argSize )
{
    IOUSBCompositeDriver *	me = (IOUSBCompositeDriver *) target;
    
    if (!me)
    {
        return kIOReturnError;
    }
    
    switch ( messageType )
    {
        case kIOMessageServiceIsAttemptingOpen:
            // The messagArgument for this message has the IOOptions passed in to the open() call that caused this message.  If the options are
            // kIOServiceSeize, we now that someone really tried to open it.  In the kernel, we will also get a message() call with a kIIOServiceRequestingClose
            // message, so we deal with closing the driver in that message.  
            //
            USBLog(5, "CompositeDriverInterestHandler received kIOMessageServiceIsAttemptingOpen with argument: %p", messageArgument );
            break;
            
        case kIOMessageServiceWasClosed:
            USBLog(5, "CompositeDriverInterestHandler received kIOMessageServiceWasClosed (expecting close = %d)", me->fExpectingClose);
            me->fExpectingClose = false;
            break;
            
        case kIOMessageServiceIsTerminated:
        case kIOUSBMessagePortHasBeenReset:
            break;
            
        default:
            USBLog(5, "CompositeDriverInterestHandler message unknown: 0x%x", (uint32_t)messageType);
    }
    
    return kIOReturnSuccess;
    
}

#pragma mark ееееееее Padding Methods еееееееее
OSMetaClassDefineReservedUnused(IOUSBCompositeDriver,  0);
OSMetaClassDefineReservedUnused(IOUSBCompositeDriver,  1);
OSMetaClassDefineReservedUnused(IOUSBCompositeDriver,  2);
OSMetaClassDefineReservedUnused(IOUSBCompositeDriver,  3);
OSMetaClassDefineReservedUnused(IOUSBCompositeDriver,  4);
OSMetaClassDefineReservedUnused(IOUSBCompositeDriver,  5);
OSMetaClassDefineReservedUnused(IOUSBCompositeDriver,  6);
OSMetaClassDefineReservedUnused(IOUSBCompositeDriver,  7);
OSMetaClassDefineReservedUnused(IOUSBCompositeDriver,  8);
OSMetaClassDefineReservedUnused(IOUSBCompositeDriver,  9);
OSMetaClassDefineReservedUnused(IOUSBCompositeDriver, 10);
OSMetaClassDefineReservedUnused(IOUSBCompositeDriver, 11);
OSMetaClassDefineReservedUnused(IOUSBCompositeDriver, 12);
OSMetaClassDefineReservedUnused(IOUSBCompositeDriver, 13);
OSMetaClassDefineReservedUnused(IOUSBCompositeDriver, 14);
OSMetaClassDefineReservedUnused(IOUSBCompositeDriver, 15);
OSMetaClassDefineReservedUnused(IOUSBCompositeDriver, 16);
OSMetaClassDefineReservedUnused(IOUSBCompositeDriver, 17);
OSMetaClassDefineReservedUnused(IOUSBCompositeDriver, 18); 
OSMetaClassDefineReservedUnused(IOUSBCompositeDriver, 19);


