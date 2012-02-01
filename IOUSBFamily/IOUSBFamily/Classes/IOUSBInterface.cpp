/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1998-2007 Apple Inc.  All Rights Reserved.
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
#include <libkern/version.h>

#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBController.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/IOUSBLog.h>
#include <IOKit/usb/USB.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOMessage.h>

//================================================================================================
//
//   External Definitions
//
//================================================================================================
//
extern KernelDebugLevel	    gKernelDebugLevel;

//================================================================================================
//
//   Local Definitions
//
//================================================================================================
//
#define super IOUSBNub

#define _GATE			_expansionData->_gate
#define _WORKLOOP		_expansionData->_workLoop
#define _NEED_TO_CLOSE	_expansionData->_needToClose
#define _OPEN_CLIENTS	_expansionData->_openClients


/* Convert USBLog to use kprintf debugging */
#define IOUSBINTERFACE_USE_KPRINTF 0

#if IOUSBINTERFACE_USE_KPRINTF
#undef USBLog
#undef USBError
void kprintf(const char *format, ...)
__attribute__((format(printf, 1, 2)));
#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= IOUSBINTERFACE_USE_KPRINTF) { kprintf( FORMAT "\n", ## ARGS ) ; }
#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif

#ifndef kIOMessageDeviceSignaledWakeup
#define kIOMessageDeviceSignaledWakeup  iokit_common_msg(0x350)
#endif

//================================================================================================
//
//   IOUSBInterface Methods
//
//================================================================================================
//
OSDefineMetaClassAndStructors(IOUSBInterface, IOUSBNub)

#pragma mark ееееееее IOService Methods еееееееее

//================================================================================================
//
//   start
//
//================================================================================================
bool 
IOUSBInterface::start(IOService *provider)
{
	OSObject * propertyObj = NULL;
	OSNumber *	locationID = NULL;
	
    IOUSBDevice * device = OSDynamicCast(IOUSBDevice, provider);
	
    if (!device)
		return false;
	
    if ( !super::start(provider))
        return false;
	
	retain();
	
    _device = device;
    _GATE = IOCommandGate::commandGate(this);
	
    if (!_GATE)
    {
        USBError(1, "%s[%p]::start - unable to create command gate", getName(), this);
        goto ErrorExit;
    }
	
    _WORKLOOP = getWorkLoop();
    if (!_WORKLOOP)
    {
        USBError(1, "%s[%p]::start - unable to find my workloop", getName(), this);
        goto ErrorExit;
    }
    _WORKLOOP->retain();
	
    if (_WORKLOOP->addEventSource(_GATE) != kIOReturnSuccess)
    {
        USBError(1, "%s[%p]::start - unable to add gate to work loop", getName(), this);
        goto ErrorExit;
    }
	
    // now that I am attached to a device, I can fill in my property table for matching
    SetProperties();
	
	propertyObj = _device->copyProperty(kUSBDevicePropertyLocationID);
	locationID = OSDynamicCast( OSNumber, propertyObj );
	if ( locationID )
		setProperty(kUSBDevicePropertyLocationID,locationID->unsigned32BitValue(), 32);
	
	if (propertyObj)
		propertyObj->release();
	
	USBLog(6, "IOUSBInterface[%p]::start - opening device[%p]", this, _device);
	_device->open(this);
	
	release();
	
    return true;
	
ErrorExit:
	
	if ( _GATE != NULL )
	{
		_GATE->release();
		_GATE = NULL;
	}
	
    if ( _WORKLOOP != NULL )
    {
        _WORKLOOP->release();
        _WORKLOOP = NULL;
    }
	
	release();
	
    return false;
	
}


//================================================================================================
//
//	open
// 
//	Note that we don't actually do anything related to the open here. we just run the call through our workLoop gate
// the actual open work is handled in handleOpen
//
//================================================================================================
bool 
IOUSBInterface::open( IOService *forClient, IOOptionBits options, void *arg )
{
    bool			res = true;
    IOReturn		error = kIOReturnSuccess;
	
    if (_expansionData && _GATE && _WORKLOOP)
    {
		IOCommandGate *	gate = _GATE;
		IOWorkLoop *	workLoop = _WORKLOOP;
		
		retain();
		workLoop->retain();
		gate->retain();
		
		USBLog(6,"%s[%p]::open calling super::open with gate", getName(), this);
		
		error = gate->runAction(CallSuperOpen, (void *)forClient, (void *)options, (void *)arg, (void *)NULL);
        if ( error != kIOReturnSuccess )
        {
            USBLog(2,"%s[%p]::open super::open failed (0x%x)", getName(), this, error);
            res = false;
        }
		
		gate->release();
		workLoop->release();
		release();
    }
    else
    {
        USBLog(6,"%s[%p]::open calling super::open with NO gate", getName(), this);
        res = super::open(forClient, options, arg);
    }
    
    if (!res)
	{
        USBLog(2,"%s[%p]::open super::open failed (0x%x)", getName(), this, res);
	}
	
    return res;
}


//================================================================================================
//
//	handleOpen
//
//================================================================================================
bool 
IOUSBInterface::handleOpen( IOService *forClient, IOOptionBits options, void *arg )
{
    UInt16		altInterface;
    IOReturn		err = kIOReturnSuccess;
	bool		result = false;
	bool		exclusiveOpen = (options & kUSBOptionBitOpenExclusivelyMask) ? true : false;
	
    USBLog(6, "+%s[%p]::handleOpen (device %s)", getName(), this, _device->getName());
	
	if (forClient->metaCast("IOUSBInterfaceUserClientV2") && !exclusiveOpen)
    {
		if (_OPEN_CLIENTS == NULL)
		{
			_OPEN_CLIENTS = OSSet::withCapacity(1);
		}
		if (_OPEN_CLIENTS)
		{
			_OPEN_CLIENTS->setObject(forClient);

			USBLog(6, "%s[%p]::handleOpen - %s[%p] added to open set", getName(), this, forClient->getName(), forClient);
			result = true;
		}
	}
	else
	{
		USBLog(5, "%s[%p]::handleOpen - [%p] is a %s, exclusiveOpen = %s", getName(), this, forClient, forClient->getName(), exclusiveOpen ? "TRUE":"FALSE");
		result = super::handleOpen(forClient, options, arg);
		USBLog(6, "%s[%p]::handleOpen - super::handleOpen returned 0x%x", getName(), this, result);
		
		if (result)
		{
			if (options & kIOUSBInterfaceOpenAlt)
			{
				altInterface = (UInt16)(uintptr_t)arg;
				
				// Note that SetAlternateInterface will actually create the pipes
				//
				USBLog(5, "%s[%p]::handleOpen calling SetAlternateInterface(%d)", getName(), this, altInterface);
				err =  SetAlternateInterface(forClient, altInterface);
				if ( err != kIOReturnSuccess) 
				{
					USBLog(1, "%s[%p]::handleOpen: SetAlternateInterface failed (0x%x)", getName(), this, err);
				}
			}
			else
			{
				err = CreatePipes();
				if ( err != kIOReturnSuccess) 
				{
					USBError(1, "%s[%p]::handleOpen: CreatePipes failed (0x%x)", getName(), this, err);
				}
			}
		}
		
		if (err != kIOReturnSuccess)
		{
			close(forClient);
			result = false;
		}
	}
	
    return result;
}


// 
// close - the IOService close method
// note that we don't actually do anything related to the close here. we just run the call through our workLoop gate
// the actual close work is handled in handleClose
//
void 
IOUSBInterface::close( IOService *forClient, IOOptionBits options)
{
	IOReturn	err = kIOReturnSuccess;
	bool		exclusiveOpen = (options & kUSBOptionBitOpenExclusivelyMask) ? true : false;
	
	if (forClient->metaCast("IOUSBInterfaceUserClientV2") && !exclusiveOpen)
	{
        USBLog(6,"%s[%p]::close  called from an IOUSBInterfaceUserClientV2 client that was not open exclusively, so no need to abort pipes", getName(), this);
	}
	else
	{
		// If our client is the IOUSBUserClientV2 and we are inactive, then we don't need to Abort() the pipes.  The IOKit termination would have taken care of it via the ClosePipes().
		if (forClient->metaCast("IOUSBInterfaceUserClientV2") && isInactive())
		{
	        USBLog(6,"%s[%p]::close  called from an IOUSBInterfaceUserClientV2 client but we are inactive.  Not aborting pipes", getName(), this);
		}
		else
		{
			if (_expansionData && _GATE && _WORKLOOP)
			{
				IOCommandGate *	gate = _GATE;
				IOWorkLoop *	workLoop = _WORKLOOP;
				
				retain();
				workLoop->retain();
				gate->retain();
				
				USBLog(6,"%s[%p]::close calling _AbortPipes with gate", getName(), this);
				
				err = gate->runAction(_AbortPipes, (void *)NULL, (void *)NULL, (void *)NULL, (void *)NULL);
				if ( err != kIOReturnSuccess )
				{
					USBLog(2,"%s[%p]:message _AbortPipes runAction failed (0x%x)", getName(), this, err);
				}
				
				gate->release();
				workLoop->release();
				release();
			}
		}
	}
	
    if (_expansionData && _GATE && _WORKLOOP)
    {
		IOCommandGate *	gate = _GATE;
		IOWorkLoop *	workLoop = _WORKLOOP;
		
		retain();
		workLoop->retain();
		gate->retain();
		
		USBLog(6,"%s[%p]::close calling super::close with gate", getName(), this);

		err = gate->runAction(CallSuperClose, (void *)forClient, (void *)options, (void *)NULL, (void *)NULL);
		if ( err != kIOReturnSuccess )
		{
			USBLog(2,"%s[%p]:message CallSuperClose() runAction failed (0x%x)", getName(), this, err);
		}
	
		gate->release();
		workLoop->release();
		release();
    }
    else
    {
        USBLog(6,"%s[%p]::close calling super::close with NO gate", getName(), this);
        super::close(forClient, options);
    }
}


//================================================================================================
//
//	handleClose
//
//================================================================================================
void 
IOUSBInterface::handleClose(IOService *	forClient, IOOptionBits	options )
{	
	bool			exclusiveOpen = (options & kUSBOptionBitOpenExclusivelyMask) ? true : false;

    USBLog(6,"+%s[%p]::handleClose", getName(), this);
	
	if (forClient->metaCast("IOUSBInterfaceUserClientV2") && !exclusiveOpen)
	{
		if (_OPEN_CLIENTS)
		{
			_OPEN_CLIENTS->removeObject(forClient);

			USBLog(6, "%s[%p]::handleClose - %s[%p] removed from open set", getName(), this, forClient->getName(), forClient);
		}
		else
		{
			USBLog(2, "%s[%p]::handleClose - _OPEN_CLIENTS is NULL", getName(), this);
		}
	}
	else
	{
		USBLog(5, "%s[%p]::handleClose - [%p] is a %s, exclusiveOpen = %s", getName(), this, forClient, forClient->getName(), exclusiveOpen ? "TRUE":"FALSE");
		super::handleClose(forClient, options);
		USBLog(6,"%s[%p]::handleClose  after super::handleClose  _OPEN_CLIENTS: %p, ->getCount(): %d", getName(), this, _OPEN_CLIENTS, _OPEN_CLIENTS ? _OPEN_CLIENTS->getCount() : 0);

	}

	// Check to see if we need to close our device now
	if (_expansionData && _NEED_TO_CLOSE)
	{
		USBLog(5,"%s[%p]::handleClose - now closing our provider from deferred close", getName(), this);
		_device->close(this);
		_NEED_TO_CLOSE = false;
	}

	USBLog(6,"-%s[%p]::handleClose", getName(), this);
}



//================================================================================================
//
//	handleIsOpen
//
//	Note that a client that opened us exclusively will not be in our known set of clients who opened us.  Unfortunately handleIsOpen() does
//	not have a variation on whether it was opened with some particular IOOptionBits.  Not sure if this will be a problem, but as long as we
//	know that it is the case, we can work around that limitation
//
//================================================================================================
bool
IOUSBInterface::handleIsOpen(const IOService *forClient) const
{
	bool	result = false;
	
	if (forClient == NULL)
	{
		if (_OPEN_CLIENTS && (_OPEN_CLIENTS->getCount() > 0))
			result = true;
	}
	else if (forClient->metaCast("IOUSBInterfaceUserClientV2"))
	{
		if (_OPEN_CLIENTS)
		{
			if (_OPEN_CLIENTS->containsObject(forClient))
			{
				USBLog(6, "%s[%p]::handleIsOpen - %s[%p] has us open", getName(), this, forClient->getName(), forClient);
				result = true;
			}
			else
			{
				USBLog(2, "%s[%p]::handleIsOpen - %s[%p] is not in _OPEN_CLIENTS", getName(), this, forClient->getName(), forClient);
			}
		}
		else
		{
			USBLog(2, "%s[%p]::handleIsOpen - no _OPEN_CLIENTS", getName(), this);
		}
	}
	
	if (!result)
	{
		result = super::handleIsOpen(forClient);
	}
	
	return result;
}




IOReturn 
IOUSBInterface::message( UInt32 type, IOService * provider,  void * argument )
{
#pragma unused (provider)
	IOReturn	err = kIOReturnSuccess;
	
    switch ( type )
    {
	case kIOUSBMessagePortHasBeenSuspended:
		// Forward the message to our clients
		//
		USBLog(6, "%s[%p]::message - received kIOUSBMessagePortHasBeenSuspended", getName(), this);
		messageClients( kIOUSBMessagePortHasBeenSuspended, argument, sizeof(IOReturn) );
		break;
		
	case kIOUSBMessagePortHasBeenReset:
		if (_expansionData && _GATE && _WORKLOOP)
		{
			IOCommandGate *	gate = _GATE;
			IOWorkLoop *	workLoop = _WORKLOOP;
			
			retain();
			workLoop->retain();
			gate->retain();
			
			USBLog(6,"%s[%p]::message calling _ResetPipes with gate", getName(), this);
			
			err = gate->runAction(_ResetPipes, (void *)NULL, (void *)NULL, (void *)NULL, (void *)NULL);
			if ( err != kIOReturnSuccess )
			{
				USBLog(2,"%s[%p]:message _ResetPipes failed (0x%x)", getName(), this, err);
			}
			else
			{
				// Forward the message to our clients
				//
				USBLog(6, "%s[%p]::message - received kIOUSBMessagePortHasBeenReset", getName(), this);
				messageClients( kIOUSBMessagePortHasBeenReset, argument, sizeof(IOReturn) );
			}
			gate->release();
			workLoop->release();
			release();
		}
		break;
		
	case kIOUSBMessagePortHasBeenResumed:
		// Forward the message to our clients
		//
		USBLog(6, "%s[%p]::message - received kIOUSBMessagePortHasBeenResumed", getName(), this);
		messageClients( kIOUSBMessagePortHasBeenResumed, NULL, 0);
		break;
		
  		case kIOUSBMessageCompositeDriverReconfigured:
		USBLog(5, "%s[%p]::message - received kIOUSBMessageCompositeDriverReconfigured",getName(), this);
		messageClients( kIOUSBMessageCompositeDriverReconfigured, NULL, 0);
		break;

#if VERSION_MAJOR > 10
    case kIOMessageDeviceSignaledWakeup:
		// Forward the message to our clients
		//
        USBLog(6, "%s[%p]::message - received kIOMessageDeviceSignaledWakeup", getName(), this);
        messageClients( kIOMessageDeviceSignaledWakeup, NULL, 0 );
        break;
#endif

	case kIOMessageServiceIsTerminated: 
		break;
		
	case kIOMessageServiceIsSuspended:
	case kIOMessageServiceIsResumed:
	case kIOMessageServiceIsRequestingClose:
	case kIOMessageServiceWasClosed:
	case kIOMessageServiceBusyStateChange:
		err = kIOReturnUnsupported;
	default:
		break;
    }
    
    return err;
}



//
// IOUSBInterface::terminate
//
// Since IOUSBInterface is most often terminated directly by the USB family, willTerminate and didTerminate will not be called most of the time
// therefore we do things which we might otherwise have done in those methods before and after we call super::terminate (5024412)
//
bool	
IOUSBInterface::terminate( IOOptionBits options )
{
	bool	retValue;
	
	USBLog(5, "+%s[%p]::terminate", getName(), this);
	
	if (_device)
	{
		USBLog(5, "%s[%p]::terminate - closing _device", getName(), this);
		if (isOpen())
		{
			USBLog(5, "%s[%p]::terminate - deferring close because someone still has us open", getName(), this);
			_NEED_TO_CLOSE = true;
		}
		else
		{
			_device->close(this);
		}
	}
	else
	{
		USBLog(2, "%s[%p]::didTerminate - NULL _device!!", getName(), this);
	}
	
	if (_expansionData && _OPEN_CLIENTS)
	{
		USBLog(5, "%s[%p]::terminate - _OPEN_CLIENTS has a count of %d", getName(), this, _OPEN_CLIENTS->getCount());
	}
	
	USBLog(6, "%s[%p]::terminate  calling super::terminate", getName(), this);
	retValue = super::terminate(options);
	USBLog(6, "-%s[%p]::terminate", getName(), this);
	
	return retValue;
}



void 
IOUSBInterface::stop( IOService * provider )
{
	
    USBLog(5,"+%s[%p]::stop (provider = %p)", getName(), this, provider);
	
    ClosePipes();
	
	if (_expansionData && _WORKLOOP && _GATE)
	{
		_WORKLOOP->removeEventSource(_GATE);
		
		if (_GATE)
		{
			_GATE->release();
			_GATE = NULL;
		}
		
		if (_WORKLOOP)
		{
			_WORKLOOP->release();
			_WORKLOOP = NULL;
		}
		
	}
		
	if (_OPEN_CLIENTS)
	{
		if (_OPEN_CLIENTS->getCount())
		{
			USBLog(4, "IOUSBInterface[%p]::free - called with %d _OPEN_CLIENTS", this, _OPEN_CLIENTS->getCount());
		}
		else
		{
			_OPEN_CLIENTS->release();
			_OPEN_CLIENTS = NULL;
		}
	}
	
	super::stop(provider);
	
    USBLog(5,"-%s[%p]::stop (provider = %p)", getName(), this, provider);
}



bool 
IOUSBInterface::finalize(IOOptionBits options)
{
    bool ret;
	
    USBLog(5, "+%s[%p]::finalize (options = 0x%qx)", getName(), this, (uint64_t) options);
	
    ret = super::finalize(options);
	
    USBLog(5, "-%s[%p]::finalize (options = 0x%qx)", getName(), this, (uint64_t) options);
	
    return ret;
}



void
IOUSBInterface::free()
{
    USBLog(6,"+IOUSBInterface[%p]::free", this);

    //  This needs to be the LAST thing we do, as it disposes of our "fake" member
    //  variables.
    //
    if (_expansionData)
    {
        IOFree(_expansionData, sizeof(ExpansionData));
        _expansionData = NULL;
    }

	USBLog(6, "-IOUSBInterface[%p]::free", this);

    super::free();
}



#pragma mark ееееееее IOUSBInterface static methods еееееееее

IOUSBInterface*
IOUSBInterface::withDescriptors(const IOUSBConfigurationDescriptor *cfdesc,
								const IOUSBInterfaceDescriptor *ifdesc)
{
    IOUSBInterface *me = new IOUSBInterface;
	
    if (!me)
        return NULL;
	
    if (!me->init(cfdesc, ifdesc)) 
    {
        me->release();
        return NULL;
    }
	
    return me;
}



IOReturn
IOUSBInterface::CallSuperOpen(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
#pragma unused (param4)
    IOUSBInterface *	me = OSDynamicCast(IOUSBInterface, target);
    bool		result;
    IOReturn		ret = kIOReturnSuccess;
    IOService *		forClient = (IOService *) param1;
    IOOptionBits 	options = (uintptr_t) param2;
    void *		arg = param3;
	
    if (!me)
    {
        USBLog(1, "IOUSBInterface::CallSuperOpen - invalid target");
        return kIOReturnBadArgument;
    }
	
    result = me->super::open(forClient, options, arg);
	
    if (! result )
        ret = kIOReturnNoResources;
	
    return ret;
}


IOReturn
IOUSBInterface::CallSuperClose(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
#pragma unused (param3, param4)

    IOUSBInterface *	me = OSDynamicCast(IOUSBInterface, target);
    IOService *		forClient = (IOService *) param1;
    IOOptionBits 	options = (uintptr_t) param2;
    
    if (!me)
    {
        USBLog(1, "IOUSBInterface::CallSuperClose - invalid target");
        return kIOReturnBadArgument;
    }
    
    me->super::close(forClient, options);
	
    return kIOReturnSuccess;
}



UInt8 
IOUSBInterface::hex2char( UInt8 digit )
{
	return ( (digit & 0x000f ) > 9 ? (digit & 0x000f) + 0x37 : (digit & 0x000f) + 0x30);
}



#pragma mark ееееееее IOUSBInterface class methods еееееееее
bool 
IOUSBInterface::init(const IOUSBConfigurationDescriptor *cfdesc,
                     const IOUSBInterfaceDescriptor *ifdesc)
{
    if (!ifdesc || !cfdesc)
        return false;

	// this is the call to the IOService init() method, which we are NOT overriding at this time
    if (!super::init())					// create my property table
        return false;

    // allocate our expansion data
    if (!_expansionData)
    {
        _expansionData = (ExpansionData *)IOMalloc(sizeof(ExpansionData));
        if (!_expansionData)
            return false;
        bzero(_expansionData, sizeof(ExpansionData));
    }

    _configDesc = cfdesc;
    _interfaceDesc = ifdesc;
    
    _bInterfaceNumber = _interfaceDesc->bInterfaceNumber;
    _bAlternateSetting = _interfaceDesc->bAlternateSetting;
    _bNumEndpoints = _interfaceDesc->bNumEndpoints;
    _bInterfaceClass = _interfaceDesc->bInterfaceClass;
    _bInterfaceSubClass = _interfaceDesc->bInterfaceSubClass;
    _bInterfaceProtocol = _interfaceDesc->bInterfaceProtocol;
    _iInterface = _interfaceDesc->iInterface;

    return true;
}



void
IOUSBInterface::SetProperties(void)
{
    // these properties come from the interface descriptor
    setProperty(kUSBInterfaceNumber, (unsigned long long)_bInterfaceNumber, (sizeof(_bInterfaceNumber) * 8));
    setProperty(kUSBAlternateSetting, (unsigned long long)_bAlternateSetting, (sizeof(_bAlternateSetting) * 8));
    setProperty(kUSBNumEndpoints, (unsigned long long)_bNumEndpoints, (sizeof(_bNumEndpoints) * 8));
    setProperty(kUSBInterfaceClass, (unsigned long long)_bInterfaceClass, (sizeof(_bInterfaceClass) * 8));
    setProperty(kUSBInterfaceSubClass, (unsigned long long)_bInterfaceSubClass, (sizeof(_bInterfaceSubClass) * 8));
    setProperty(kUSBInterfaceProtocol, (unsigned long long)_bInterfaceProtocol, (sizeof(_bInterfaceProtocol) * 8));
    setProperty(kUSBInterfaceStringIndex, (unsigned long long)_iInterface, (sizeof(_iInterface) * 8));
    
    // these properties come from the device descriptor, but can be used for interface matching
    setProperty(kUSBVendorID, (unsigned long long)_device->GetVendorID(), (sizeof(UInt16) * 8));
    setProperty(kUSBProductID, (unsigned long long)_device->GetProductID(), (sizeof(UInt16) * 8));
    setProperty(kUSBDeviceReleaseNumber, (unsigned long long)_device->GetDeviceRelease(), (sizeof(UInt16) * 8));
    
    // this property is in the config descriptor, but is useful to have
    setProperty(kUSBConfigurationValue, (unsigned long long)_configDesc->bConfigurationValue, (sizeof(_configDesc->bConfigurationValue) * 8));
    
    OSNumber 	*offset = OSNumber::withNumber(_bInterfaceNumber, 8);
    if (offset) 
    {
        UInt16	interfaceNumber;
        char	location[32];
        
        interfaceNumber = offset->unsigned16BitValue();
        offset->release();
        snprintf( location, sizeof(location), "%x", interfaceNumber );
        setLocation(location);
    }
	
    if (_iInterface) 
    {
        IOReturn err;
        char name[256];
        
        err = _device->GetStringDescriptor(_iInterface, name, sizeof(name));
        if (err == kIOReturnSuccess)
        {
            if ( name[0] != 0 )
			{
                setName(name);
				setProperty("USB Interface Name", name);
			}
        }
    }
    
	
	if ( _bInterfaceClass == kUSBMassStorageInterfaceClass )
	{
		// Mass Storage Class interface. Let's see if we can  generate a GUID
		//
		OSObject * propertyObj = _device->copyProperty(kUSBSerialNumberString);
		OSString* serialNumberRef = OSDynamicCast(OSString, propertyObj);
		if ( serialNumberRef )
		{
			UInt32		length = serialNumberRef->getLength();
			
			if ( length > 11 )
			{
				UInt16		vendorID = _device->GetVendorID();
				UInt16		productID = _device->GetProductID();
				UInt32			j;
				char		guid[255];
				const char *		serial;
				bool		valid = true;
				
				// Validate the serial # according to the USB Mass Storage spec
				//
				serial = serialNumberRef->getCStringNoCopy();
				for ( j = 0; j < length; j++ )
				{
					// Char can be between 0x30-0x39 OR 0x41 - 0x46
					//
					if ( (serial[j] < 0x30) || (serial[j] > 0x46) )
					{
						USBLog(6, "%s[%p]::SetProperties  Serial #%s character %d (0x%x) out of range #1", getName(), this, serial, (uint32_t)j, (uint32_t)serial[j]);
						valid = false;
						break;
					}
					if ( (serial[j] > 0x39) && (serial[j] < 0x41) )
					{
						USBLog(6, "%s[%p]::SetProperties  Serial #%s character %d (0x%x) out of range #2", getName(), this, serial, (uint32_t)j, (uint32_t)serial[j]);
						valid = false;
						break;
					}
				}
				
				if ( valid )
				{
					//  Add the "USB:" prefix to the GUID
					guid[0] = 0x55; guid[1] = 0x53; guid[2] = 0x42; guid[3] = 0x3a;
					
					for ( j = 0; j < 4;j++)
						guid[j+4] = hex2char(vendorID>>(12-4*j));
					
					for ( j = 0; j < 4;j++)
						guid[j+8] = hex2char(productID>>(12-4*j));
					
					for ( j = 0; j < 12;j++)
						guid[j+12] = serial[length-12+j];
					
					guid[24] = '\0';
					USBLog(5,"IOUSBInterface: uid %s", guid);
					_device->setProperty("uid", guid);
				}
				IOLog("USBMSC Identifier (non-unique): %s 0x%x 0x%x 0x%x\n", serial, (uint32_t)_device->GetVendorID(), (uint32_t)_device->GetProductID(), (uint32_t)_device->GetDeviceRelease() );
			}
			else
			{
				USBLog(4, "%s[%p]::SetProperties  Mass Storage device but serial # is only %d characters", getName(), this, (uint32_t)length);
			}
		}			
		else 
		{
			IOLog("USBMSC Identifier (non-unique): 0x%x 0x%x 0x%x\n", (uint32_t)_device->GetVendorID(), (uint32_t)_device->GetProductID(), (uint32_t)_device->GetDeviceRelease() );
		}
		
		if (propertyObj)
			propertyObj->release();
	}
}



void 
IOUSBInterface::ClosePipes(void)
{
    IOReturn		err = kIOReturnSuccess;

    USBLog(6,"+%s[%p]::ClosePipes", getName(), this);

    if (_expansionData && _GATE && _WORKLOOP)
	{
		IOCommandGate *	gate = _GATE;
		IOWorkLoop *	workLoop = _WORKLOOP;
		
		retain();
		workLoop->retain();
		gate->retain();
		
		USBLog(6,"%s[%p]::ClosePipes calling _ClosePipes with gate", getName(), this);
		
		err = gate->runAction(_ClosePipes, (void *)NULL, (void *)NULL, (void *)NULL, (void *)NULL);
		if ( err != kIOReturnSuccess )
		{
			USBLog(2,"%s[%p]:ClosePipes _ClosePipes runAction failed (0x%x)", getName(), this, err);
		}
		
		gate->release();
		workLoop->release();
		release();
	}

    USBLog(7,"-%s[%p]::ClosePipes", getName(), this);
}

IOReturn
IOUSBInterface::_ResetPipes(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
#pragma unused (param1, param2, param3, param4)
	
    IOUSBInterface*	me	= OSDynamicCast(IOUSBInterface, target);
    IOReturn		ret = kIOReturnSuccess;
	
    if (!me)
    {
        USBLog(1, "IOUSBInterface::_ResetPipes - invalid target");
        return kIOReturnBadArgument;
    }
	
    ret = me->ResetPipes();
	
    return ret;
}

IOReturn
IOUSBInterface::ResetPipes(void)
{
	IOUSBPipe*	pipe;
    IOReturn	ret = kIOReturnSuccess;

	USBLog(6,"+%s[%p]::ResetPipes", getName(), this);

	for ( unsigned int i = 0; i < kUSBMaxPipes; i++ )
	{
		pipe = OSDynamicCast(IOUSBPipe,_pipeList[i]);
		if ( pipe ) 
		{
			ret = pipe->Reset(); 
		}
		
		if(ret != kIOReturnSuccess) break;
	}
	
	USBLog(7,"-%s[%p]::ResetPipes", getName(), this);
	
	return ret;
}

IOReturn
IOUSBInterface::_AbortPipes(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
#pragma unused (param1, param2, param3, param4)
	
    IOUSBInterface*	me	= OSDynamicCast(IOUSBInterface, target);
    IOReturn		ret = kIOReturnSuccess;
	
    if (!me)
    {
        USBLog(1, "IOUSBInterface::_AbortPipes - invalid target");
        return kIOReturnBadArgument;
    }
	
    ret = me->AbortPipesGated();
	
    return ret;
}

IOReturn
IOUSBInterface::AbortPipesGated(void)
{
	IOUSBPipe*	pipe;
    IOReturn	ret = kIOReturnSuccess;
	
	USBLog(6,"+%s[%p]::AbortPipesGated", getName(), this);
	
	for ( unsigned int i = 0; i < kUSBMaxPipes; i++ )
	{
		pipe = OSDynamicCast(IOUSBPipe,_pipeList[i]);
		if ( pipe ) 
		{
			ret = pipe->Abort(); 
		}
		
		if(ret != kIOReturnSuccess) break;
	}
	
	USBLog(7,"-%s[%p]::AbortPipesGated", getName(), this);
	
	return ret;
}

IOReturn
IOUSBInterface::_ClosePipes(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
#pragma unused (param1, param2, param3, param4)
	
    IOUSBInterface*	me	= OSDynamicCast(IOUSBInterface, target);
    IOReturn		ret = kIOReturnSuccess;
	
    if (!me)
    {
        USBLog(1, "IOUSBInterface::_ClosePipes - invalid target");
        return kIOReturnBadArgument;
    }
	
    ret = me->ClosePipesGated();
	
    return ret;
}

IOReturn
IOUSBInterface::ClosePipesGated(void)
{
	IOUSBPipe*	pipe;
    IOReturn	ret = kIOReturnSuccess;
	
	USBLog(6,"+%s[%p]::ClosePipesGated", getName(), this);
	
    for( unsigned int i=0; i < kUSBMaxPipes; i++) 
    {
        if ( (pipe = OSDynamicCast(IOUSBPipe,_pipeList[i]))) 
        {
            pipe->Abort(); 
            pipe->ClosePipe();
            pipe->release();
            _pipeList[i] = NULL;
        }
    }
	
	USBLog(7,"-%s[%p]::ClosePipesGated", getName(), this);
	
	return ret;
}

IOReturn
IOUSBInterface::CreatePipes(void)
{
    unsigned int					i = 0;
    const IOUSBDescriptorHeader		*pos = NULL;						// start with the interface descriptor
    const IOUSBEndpointDescriptor	*ep = NULL;
    bool							makePipeFailed = false;
    IOReturn						res = kIOReturnSuccess;
    
    while ((pos = FindNextAssociatedDescriptor(pos, kUSBEndpointDesc))) 
    {
        // Don't open twice!
        if (_pipeList[i] == NULL)
		{
			ep = (const IOUSBEndpointDescriptor *)pos;
			// 4266888 - check to make sure that the ep number is non-zero so we don't screw up the control ep
			if ((ep->bEndpointAddress & kUSBPipeIDMask) != 0)
				_pipeList[i] = _device->MakePipe(ep, this);
			else
			{
				USBError(1, "USB Device (%s) interface (%d) has an invalid endpoint descriptor with zero endpoint number", _device->getName(), _bInterfaceNumber);
			}
		}
        
        if (_pipeList[i] == NULL) 
        {
            makePipeFailed = true;
        }
            
		// bounds checking
        if( ++i >= kUSBMaxPipes )
		{
			break;
		}
    }
    
   // Relax the checkin on whether the # of pipes created was the same as the bNumEndpoints
    // specified in the interface descriptor.  Instead of failing to create the pipes, we will
    // now write out a message to the log
    //
    if ( (i != _bNumEndpoints) && !makePipeFailed )
    {
        if (i < _bNumEndpoints)
		{
            USBLog(3, "%s[%p]: NOTE: Interface descriptor defines more endpoints (bNumEndpoints = %d, descriptors = %d) than endpoint descriptors", getName(), this, _bNumEndpoints, i);
		}
        else
		{
            USBLog(3, "%s[%p]: NOTE: Interface descriptor defines less endpoints (bNumEndpoints = %d, descriptors = %d) than endpoint descriptors", getName(), this, _bNumEndpoints, i);
		}
    }

    if ( makePipeFailed )
    {
        USBLog(3,"%s[%p]: Failed to create all pipes.", getName(), this);

        // We don't set an error here so that we allow those pipes that were successfully created to coexist with some that were not
        // This is true for some devices that incorrectly specify an interrupt pipe with a polling interval of 0ms along with their other pipes
    }
    
    return res;
}


const IOUSBInterfaceDescriptor *
IOUSBInterface::FindNextAltInterface(const IOUSBInterfaceDescriptor *current,
                                    IOUSBFindInterfaceRequest *request)
{
    IOUSBInterfaceDescriptor 	*id = (IOUSBInterfaceDescriptor *)current;
    
    while (_device->FindNextInterfaceDescriptor(NULL, id, request, &id) == kIOReturnSuccess)
    {
        if (id == NULL)
            return NULL;
            
        if (id->bInterfaceNumber != _bInterfaceNumber)
            continue;
        return id;
    }
    return NULL;
}



IOUSBPipe*
IOUSBInterface::FindNextPipe(IOUSBPipe *current, 
							 IOUSBFindEndpointRequest *request)
{
	IOUSBPipe *							pipe;
	
	//	For backwards compatibility, release the retained pipe before returning to the caller.
	pipe = FindNextPipe(current, request, true);
	if (pipe)
		pipe->release();
	
	return pipe;
}

OSMetaClassDefineReservedUsed(IOUSBInterface,  1);

IOUSBPipe*
IOUSBInterface::FindNextPipe(IOUSBPipe *current,
							 IOUSBFindEndpointRequest *request,
							 bool withRetain)
{
	IOUSBPipe *	thePipeObj = NULL;
	IOReturn	err = kIOReturnSuccess;
	
    if (_expansionData && _GATE && _WORKLOOP)
    {
		IOCommandGate *	gate = _GATE;
		IOWorkLoop *	workLoop = _WORKLOOP;
		
		retain();
		workLoop->retain();
		gate->retain();
		
		USBLog(7,"%s[%p]::FindNextPipe  calling _FindNextPipe (&returnPipe %p) with gate", getName(), this, &thePipeObj);
		
		err = gate->runAction(_FindNextPipe, (void *)current, (void *)request, (void*)withRetain, (void*)&thePipeObj);
		if ( err != kIOReturnSuccess )
		{
			USBLog(2,"%s[%p]:FindNextPipe  _FindNextPipe runAction failed (0x%x)", getName(), this, err);
		}
 		
		gate->release();
		workLoop->release();
		release();
   }
	
	USBLog(7,"%s[%p]::FindNextPipe  returning pipe %p", getName(), this, thePipeObj);
	return thePipeObj;
}


IOReturn
IOUSBInterface::_FindNextPipe(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
#pragma unused (param4)
	
    IOUSBInterface*				me	= OSDynamicCast(IOUSBInterface, target);
    IOUSBPipe *					current = (IOUSBPipe *) param1;
    IOUSBFindEndpointRequest * 	request = (IOUSBFindEndpointRequest *) param2;
    bool						retain = (bool) param3;
    IOUSBPipe **				thePipeObj = (IOUSBPipe **) param4;
	
    if (!me)
    {
        USBLog(1, "IOUSBInterface::_FindNextPipe - invalid target");
        return kIOReturnBadArgument;
    }
	
    *thePipeObj = me->FindNextPipeGated(current, request, retain);
	USBLog(7,"%s[%p]::_FindNextPipe  FindNextPipeGated pipe %p", me->getName(), me, *thePipeObj);

    return kIOReturnSuccess;
}

IOUSBPipe*
IOUSBInterface::FindNextPipeGated(IOUSBPipe *current,
							 IOUSBFindEndpointRequest *request,
							 bool withRetain)
{
	const IOUSBController::Endpoint *	endpoint;
	IOUSBPipe *							pipe = NULL;
	int									numEndpoints;
	int									i;
	
	numEndpoints = _bNumEndpoints;
	
	if (request == 0)
		return NULL;
	
	if (current != 0)
	{
		for(i=0; (i < numEndpoints) && (i < kUSBMaxPipes) ; i++)
		{
			if (_pipeList[i] == current)
			{
				i++; // Skip the one we just did
				break;
			}
		}
	}
	else
	{
		i = 0;	// Start at beginning.
	}
	
	for ( ; (i < numEndpoints) && (i < kUSBMaxPipes) ; i++) 
	{
		pipe = OSDynamicCast(IOUSBPipe,_pipeList[i]);
		USBLog(6,"%s[%p]::FindNextPipeGated created pipe %p", getName(), this, pipe);
		if (!pipe)
			continue;
		
		endpoint = pipe->GetEndpoint();
		
		// check the request parameters
		if (request->type != kUSBAnyType && request->type != endpoint->transferType)
		{
			pipe = NULL;								// this is not it
		}
		else if (request->direction != kUSBAnyDirn && request->direction != endpoint->direction)
		{
			pipe = NULL;								// this is not it
		}
		
		if (pipe == NULL)
			continue;
		
		request->type = endpoint->transferType;
		request->direction = endpoint->direction;
		request->maxPacketSize = endpoint->maxPacketSize;
		request->interval = endpoint->interval;
		break;
	}
	
	if (pipe && withRetain)
		pipe->retain();				// caller will release
	
	return pipe;
}

const IOUSBDescriptorHeader *
IOUSBInterface::FindNextAssociatedDescriptor(const void *current, UInt8 type)
{
    const IOUSBDescriptorHeader *next;

    if (current == NULL)
        current = _interfaceDesc;

    next = (const IOUSBDescriptorHeader *)current;

    while (true) 
    {
        next = _device->FindNextDescriptor(next, kUSBAnyDesc);

        // The Interface Descriptor ends when we find the end of the Config Desc, an Interface Association Descriptor
        // or an InterfaceDescriptor WITH a different interface number (this will allow us to look for the alternate settings
        // of Interface descriptors
        //
        if (!next || next->bDescriptorType == kUSBInterfaceAssociationDesc || ( (next->bDescriptorType == kUSBInterfaceDesc) && ( type != kUSBInterfaceDesc) ) )
        {
            return NULL;
        }

        if ( (next->bDescriptorType == kUSBInterfaceDesc) && ( ( (IOUSBInterfaceDescriptor *)next)->bInterfaceNumber != _bInterfaceNumber) )
        {
            return NULL;
        }

        if (next->bDescriptorType == type || type == kUSBAnyDesc)
            break;
    }
    return next;
}


// zzz need to go behind a gate??
IOReturn
IOUSBInterface::SetAlternateInterface(IOService *forClient, UInt16 alternateSetting)
{
    const IOUSBDescriptorHeader 	*next;
    const IOUSBInterfaceDescriptor 	*ifdesc = NULL;
    const IOUSBInterfaceDescriptor 	*prevDesc = NULL;
    IOUSBDevRequest			request;
    IOReturn 				res;
	UInt32					altSettingCount = 0;

    USBLog(6,"+%s[%p]::SetAlternateInterface for interface %d to %d",getName(), this, _bInterfaceNumber, alternateSetting);
    
    // If we're not opened by our client, we can't set the configuration
    //
    if (!isOpen(forClient))
    {
		res = kIOReturnExclusiveAccess;
        goto ErrorExit;
    }
    
    next = (const IOUSBDescriptorHeader *)_configDesc;

    USBLog(6,"%s[%p]::SetAlternateInterface starting @ %p",getName(), this, next);
	
    while( (next = _device->FindNextDescriptor(next, kUSBInterfaceDesc))) 
    {
		altSettingCount++;
		
        ifdesc = (const IOUSBInterfaceDescriptor *)next;
		USBLog(6,"%s[%p]::SetAlternateInterface found InterfaceDesc @ %p, bInterfaceNumber = %d, bAlternateSetting = %d",getName(), this, ifdesc, ifdesc->bInterfaceNumber, ifdesc->bAlternateSetting);

        if ((ifdesc->bInterfaceNumber == _bInterfaceNumber))
		{
			prevDesc = ifdesc;
			if ( (ifdesc->bAlternateSetting == alternateSetting))
				break;
		}
		
		// Reset ifdesc so that if the while loop exits, we return an error of kIOUSBInterfaceNotFound
		//
		ifdesc = NULL;
		
    }

    if ( ifdesc == NULL)
    {
		// Workaround for rdar://4007300
		// If we have a printing class/subclass AND the desired setting is 1 AND the printer only had ONE altSetting and it's value is 0 then 
		// use that setting so that we behave like Panther
		//
		if ( ( _bInterfaceClass == 7) && ( _bInterfaceSubClass = 1) && (alternateSetting == 1) && (altSettingCount == 1) && (prevDesc != NULL ) && (prevDesc->bAlternateSetting == 0) )
		{
			ifdesc = prevDesc;
		}
		else
		{
			res =  kIOUSBInterfaceNotFound;
			goto ErrorExit;
		}
    }

    // we have a valid alternate interface, so we need to first make all of the existing pipes invalid
    ClosePipes();
    
    // now adjust our state variables
    _interfaceDesc = ifdesc;

	_bInterfaceNumber = _interfaceDesc->bInterfaceNumber;		// this should NOT have changed
    _bAlternateSetting = _interfaceDesc->bAlternateSetting;		// this SHOULD have changed
    _bNumEndpoints = _interfaceDesc->bNumEndpoints;
    _bInterfaceClass = _interfaceDesc->bInterfaceClass;
    _bInterfaceSubClass = _interfaceDesc->bInterfaceSubClass;
    _bInterfaceProtocol = _interfaceDesc->bInterfaceProtocol;
    _iInterface = _interfaceDesc->iInterface;
 	USBLog(5,"%s[%p]::SetAlternateInterface bInterfaceNumber = %d, bAlternateSetting = %d, bNumEndpoints = %d, class = %d, subClass = %d, protocol = %d",getName(), this, _bInterfaceNumber, _bAlternateSetting,
		  _bNumEndpoints, _bInterfaceClass,  _bInterfaceSubClass, _bInterfaceProtocol);
   
    // now issue the actual bus command
    request.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBStandard, kUSBInterface);
    request.bRequest = kUSBRqSetInterface;
    request.wValue = _bAlternateSetting;
    request.wIndex = _bInterfaceNumber;
    request.wLength = 0;
    request.pData = NULL;

	USBLog(6,"%s[%p]::SetAlternateInterface Sending SETINTERFACE, bInterfaceNumber = %d, bAlternateSetting = %d",getName(), this, _bInterfaceNumber, _bAlternateSetting);
    res = _device->DeviceRequest(&request);

    if (res != kIOReturnSuccess) 
    {
        goto ErrorExit;
    }
    
    // my property list may have changed, so update it
    //
    SetProperties();
	
    res = CreatePipes();
    
ErrorExit:

    if ( res != kIOReturnSuccess )
    {
        USBLog(3,"%s[%p]::SetAlternateInterface (Intfc: %d, AltSet: %d) returning error (0x%x)",getName(), this, _bInterfaceNumber, _bAlternateSetting, res);
    }
    
    USBLog(6,"-%s[%p]::SetAlternateInterface for interface %d to %d",getName(), this, _bInterfaceNumber, alternateSetting);
    return res;
}



/**
 ** Matching methods
 **/
bool 
IOUSBInterface::matchPropertyTable(OSDictionary * table, SInt32 *score)
{
    bool		returnValue = true;
    SInt32		propertyScore = *score;
    char		logString[256]="";
    UInt32      wildCardMatches = 0;
    UInt32      vendorIsWildCard = 0;
    UInt32      productIsWildCard = 0;
    UInt32      interfaceNumberIsWildCard = 0;
    UInt32      configurationValueIsWildCard = 0;
    UInt32      deviceReleaseIsWildCard = 0;
    UInt32      classIsWildCard = 0;
    UInt32      subClassIsWildCard = 0;
    UInt32      protocolIsWildCard = 0;
	bool		usedMaskForProductID = false;
	UInt32		productIDInArray;

    if ( table == NULL )
    {
        return false;
    }
	
    bool	vendorPropertyExists = table->getObject(kUSBVendorID) ? true : false ;
    bool	productPropertyExists = table->getObject(kUSBProductID) ? true : false ;
    bool	interfaceNumberPropertyExists = table->getObject(kUSBInterfaceNumber) ? true : false ;
    bool	configurationValuePropertyExists = table->getObject(kUSBConfigurationValue) ? true : false ;
    bool	deviceReleasePropertyExists = table->getObject(kUSBDeviceReleaseNumber) ? true : false ;
    bool	interfaceClassPropertyExists = table->getObject(kUSBInterfaceClass) ? true : false ;
    bool	interfaceSubClassPropertyExists = table->getObject(kUSBInterfaceSubClass) ? true : false ;
    bool	interfaceProtocolPropertyExists= table->getObject(kUSBInterfaceProtocol) ? true : false ;
    bool    productIDMaskExists = table->getObject(kUSBProductIDMask) ? true : false;
	bool	productIDArrayExists = OSDynamicCast(OSArray, table->getObject(kUSBProductIdsArrayName)) ? true : false;
	
    // USBComparePropery() will return false if the property does NOT exist, or if it exists and it doesn't match
    //
    bool	vendorPropertyMatches = USBCompareProperty(table, kUSBVendorID);
    bool	productPropertyMatches = USBCompareProperty(table, kUSBProductID);
    bool	interfaceNumberPropertyMatches = USBCompareProperty(table, kUSBInterfaceNumber);
    bool	configurationValuePropertyMatches = USBCompareProperty(table, kUSBConfigurationValue);
    bool	deviceReleasePropertyMatches = USBCompareProperty(table, kUSBDeviceReleaseNumber);
    bool	interfaceClassPropertyMatches = USBCompareProperty(table, kUSBInterfaceClass);
    bool	interfaceSubClassPropertyMatches = USBCompareProperty(table, kUSBInterfaceSubClass);
    bool	interfaceProtocolPropertyMatches= USBCompareProperty(table, kUSBInterfaceProtocol);
    
	// If there is an productIDArray, then see if any of the entries matches to our productID
	
	if ( productIDArrayExists )
	{
		productPropertyMatches = USBComparePropertyInArray( table, kUSBProductIdsArrayName, kUSBProductID, &productIDInArray);
        USBLog(7, "%s[%p]::matchPropertyTable  productIDArrayExists was true, matches = %s", getName(), this, productPropertyMatches?"true":"false");
	}

    // Now, let's look and see whether any of the properties were OSString's AND their value was "*".  This indicates that
    // we should match to all the values for that category -- i.e. a wild card match
    //
    if ( !vendorPropertyMatches )               
    { 
		vendorPropertyMatches = IsWildCardMatch(table, kUSBVendorID); 
		if ( vendorPropertyMatches ) 
			vendorIsWildCard++; 
    }
    
    if ( !productPropertyMatches )
    { 
		productPropertyMatches = IsWildCardMatch(table, kUSBProductID);
		if ( productPropertyMatches ) 
		{
			productIsWildCard++; 
		}
    }
    
    if ( !interfaceNumberPropertyMatches )
    { 
		interfaceNumberPropertyMatches = IsWildCardMatch(table, kUSBInterfaceNumber);
		if ( interfaceNumberPropertyMatches ) 
			interfaceNumberIsWildCard++; 
    }
    
    if ( !configurationValuePropertyMatches )
    { 
		configurationValuePropertyMatches = IsWildCardMatch(table, kUSBConfigurationValue);
		if ( configurationValuePropertyMatches ) 
			configurationValueIsWildCard++; 
    }
    
    if ( !deviceReleasePropertyMatches )
    { 
		deviceReleasePropertyMatches = IsWildCardMatch(table, kUSBDeviceReleaseNumber);
		if ( deviceReleasePropertyMatches ) 
			deviceReleaseIsWildCard++;
    }
    
    if ( !interfaceClassPropertyMatches )
    { 
		interfaceClassPropertyMatches = IsWildCardMatch(table, kUSBInterfaceClass);
		if ( interfaceClassPropertyMatches ) 
			classIsWildCard++;
    }
    
    if ( !interfaceSubClassPropertyMatches )
    { 
		interfaceSubClassPropertyMatches = IsWildCardMatch(table, kUSBInterfaceSubClass);
		if ( interfaceSubClassPropertyMatches ) 
			subClassIsWildCard++; 
    }
    
    if ( !interfaceProtocolPropertyMatches )
    { 
		interfaceProtocolPropertyMatches = IsWildCardMatch(table, kUSBInterfaceProtocol);
		if ( interfaceProtocolPropertyMatches ) 
			protocolIsWildCard++; 
    }
    
    // If the productID didn't match, then see if there is a kProductIDMask property.  If there is, mask this device's prodID and the dictionary's prodID with it and see
    // if they are equal and if so, then we say we productID matches.
	//
    if ( !productPropertyMatches && productIDMaskExists )
    {
		if ( !productIDArrayExists )
		{
			productPropertyMatches = USBComparePropertyWithMask( table, kUSBProductID, kUSBProductIDMask);
			usedMaskForProductID = productPropertyMatches;
		}
		else 
		{
			productPropertyMatches = USBComparePropertyInArrayWithMask( table, kUSBProductIdsArrayName, kUSBProductID, kUSBProductIDMask, &productIDInArray);
			usedMaskForProductID = productPropertyMatches;
		}
    }
    
    // If the service object wishes to compare some of its properties in its
    // property table against the supplied matching dictionary,
    // it should do so in this method and return truth on success.
    //
    if (!super::matchPropertyTable(table))  
		return false;
        
    // If the property score is > 10000, then clamp it to 9000.  We will then add this score
    // to the matching criteria score.  This will allow drivers
    // to still override a driver with the same matching criteria.  Since we score the matching
    // criteria in increments of 10,000, clamping it to 9000 guarantees us 1000 to do what we please.
    //
    if ( propertyScore >= 10000 ) 
        propertyScore = 9000;
    
    // Get the class to see if it's vendor-specific later on.
    //
	OSObject *interfaceClassProp = copyProperty(kUSBInterfaceClass);
    OSNumber *interfaceClass = OSDynamicCast(OSNumber, interfaceClassProp);
	
    if ( vendorPropertyMatches && productPropertyMatches && deviceReleasePropertyMatches &&
		 configurationValuePropertyMatches &&  interfaceNumberPropertyMatches &&
		 (!interfaceClassPropertyExists || interfaceClassPropertyMatches) && 
		 (!interfaceSubClassPropertyExists || interfaceSubClassPropertyMatches) && 
		 (!interfaceProtocolPropertyExists || interfaceProtocolPropertyMatches) )
    {
        *score = 100000;
        wildCardMatches = vendorIsWildCard + productIsWildCard + deviceReleaseIsWildCard + configurationValueIsWildCard + interfaceNumberIsWildCard;
    }
    else if ( vendorPropertyMatches && productPropertyMatches && configurationValuePropertyMatches && 
			  interfaceNumberPropertyMatches &&
			  (!deviceReleasePropertyExists || deviceReleasePropertyMatches) && 
			  (!interfaceClassPropertyExists || interfaceClassPropertyMatches) && 
			  (!interfaceSubClassPropertyExists || interfaceSubClassPropertyMatches) && 
			  (!interfaceProtocolPropertyExists || interfaceProtocolPropertyMatches) )
    {
        *score = 90000;
        wildCardMatches = vendorIsWildCard + productIsWildCard + configurationValueIsWildCard + interfaceNumberIsWildCard;
    }
    else if ( interfaceClass && (interfaceClass->unsigned32BitValue() == kUSBVendorSpecificClass ))
    {
        if (  vendorPropertyMatches && interfaceSubClassPropertyMatches && interfaceProtocolPropertyMatches && 
			  (!interfaceClassPropertyExists || interfaceClassPropertyMatches) && 
			  !deviceReleasePropertyExists && !productPropertyExists &&
			  !interfaceNumberPropertyExists && !configurationValuePropertyExists )
        {
            *score = 80000;
            wildCardMatches = vendorIsWildCard + subClassIsWildCard + protocolIsWildCard;
        }
        else if ( vendorPropertyMatches && interfaceSubClassPropertyMatches &&
				  (!interfaceClassPropertyExists || interfaceClassPropertyMatches) && 
				  !interfaceProtocolPropertyExists && !deviceReleasePropertyExists && 
				  !productPropertyExists && !interfaceNumberPropertyExists && !configurationValuePropertyExists )
        {
            *score = 70000;
            wildCardMatches = vendorIsWildCard + subClassIsWildCard;
        }
        else
        {
            *score = 0;
            returnValue = false;
        }
    }
    else if (  interfaceClassPropertyMatches && interfaceSubClassPropertyMatches && interfaceProtocolPropertyMatches &&
			   !deviceReleasePropertyExists &&  !vendorPropertyExists && !productPropertyExists && 
			   !interfaceNumberPropertyExists && !configurationValuePropertyExists )
    {
        *score = 60000;
        wildCardMatches = classIsWildCard + subClassIsWildCard + protocolIsWildCard;
    }
    else if (  interfaceClassPropertyMatches && interfaceSubClassPropertyMatches &&
			   !interfaceProtocolPropertyExists && !deviceReleasePropertyExists &&  !vendorPropertyExists && 
			   !productPropertyExists && !interfaceNumberPropertyExists && !configurationValuePropertyExists )
    {
        *score = 50000;
        wildCardMatches = classIsWildCard + subClassIsWildCard;
    }
    else
    {
        *score = 0;
        returnValue = false;
    }
	
    // Add in the xml probe score if it's available.
    //
    if ( *score != 0 )
        *score += propertyScore;
    
    // Subtract the # of wildcards that matched by 1000
    //
    *score -= wildCardMatches * 1000;
    
    // Subtract 500 if the usedMaskForProductID was set
    //
    if ( usedMaskForProductID )
		*score -= 500;
    
    //  Only execute the debug code if we are logging at higher than level 4
    //
    if ( (*score > 0) && (gKernelDebugLevel > 4) )
    {
		OSString * 	identifier = OSDynamicCast(OSString, table->getObject("CFBundleIdentifier"));
		OSNumber *	vendor = (OSNumber *) getProperty(kUSBVendorID);
		OSNumber *	product = (OSNumber *) getProperty(kUSBProductID);
		OSNumber *	release = (OSNumber *) getProperty(kUSBDeviceReleaseNumber);
		OSNumber *	configuration = (OSNumber *) getProperty(kUSBConfigurationValue);
		OSNumber *	interfaceNumber = (OSNumber *) getProperty(kUSBInterfaceNumber);
		OSNumber *	interfaceSubClass = (OSNumber *) getProperty(kUSBInterfaceSubClass);
		OSNumber *	protocol = (OSNumber *) getProperty(kUSBInterfaceProtocol);
		OSNumber *	dictVendor = (OSNumber *) table->getObject(kUSBVendorID);
		OSNumber *	dictProduct = (OSNumber *) table->getObject(kUSBProductID);
		OSNumber *	dictRelease = (OSNumber *) table->getObject(kUSBDeviceReleaseNumber);
		OSNumber *	dictConfiguration = (OSNumber *) table->getObject(kUSBConfigurationValue);
		OSNumber *	dictInterfaceNumber = (OSNumber *) table->getObject(kUSBInterfaceNumber);
		OSNumber *	dictInterfaceClass = (OSNumber *) table->getObject(kUSBInterfaceClass);
		OSNumber *	dictInterfaceSubClass = (OSNumber *) table->getObject(kUSBInterfaceSubClass);
		OSNumber *	dictProtocol = (OSNumber *) table->getObject(kUSBInterfaceProtocol);
		OSNumber *	dictMask = (OSNumber *) table->getObject(kUSBProductIDMask);
		bool		match;
		
		if (identifier)
		{
			USBLog(5,"Finding driver for interface #%d of %s, matching personality using %s, score: %d, wildCard = %d", _bInterfaceNumber, _device->getName(), identifier->getCStringNoCopy(), (uint32_t)*score, (uint32_t)wildCardMatches);
		}
		else
		{
			USBLog(6,"Finding driver for interface #%d of %s, matching user client dictionary, score: %d", _bInterfaceNumber, _device->getName(), (uint32_t)*score);
		}
		
		if ( vendor && product && release && configuration && interfaceNumber && interfaceClass && interfaceSubClass && protocol )
		{
			char tempString[256]="";
			
			snprintf(logString, sizeof(logString), "\tMatched: ");
			match = false;
			if ( vendorPropertyMatches ) { match = true; snprintf(tempString, sizeof(tempString), "idVendor (%d) ", vendor->unsigned32BitValue()); strlcat(logString, tempString, sizeof(logString)); }
			
			if ( productIDArrayExists && productPropertyMatches ) { match = true; snprintf(tempString, sizeof(tempString), "idProduct in array (%d) ", (uint32_t)productIDInArray);  strlcat(logString, tempString, sizeof(logString));}
			else if ( productPropertyMatches ) { match = true; snprintf(tempString, sizeof(tempString), "idProduct (%d) ", product->unsigned32BitValue());  strlcat(logString, tempString, sizeof(logString));}
			
			if ( usedMaskForProductID && dictMask )
				if (dictProduct) { snprintf(tempString, sizeof(tempString), "to (%d) with mask (0x%x), ", dictProduct->unsigned32BitValue(), dictMask->unsigned32BitValue()); strlcat(logString, tempString, sizeof(logString));}
				else if ( productIDArrayExists && productPropertyMatches ) { snprintf(tempString, sizeof(tempString), "to (%d) with mask (0x%x), ", (uint32_t)productIDInArray, dictMask->unsigned32BitValue()); strlcat(logString, tempString, sizeof(logString));}

			if ( deviceReleasePropertyMatches ) { match = true; snprintf(tempString, sizeof(tempString), "bcdDevice (%d) ", release->unsigned32BitValue()); strlcat(logString, tempString, sizeof(logString)); }
			if ( configurationValuePropertyMatches ) { match = true; snprintf(tempString, sizeof(tempString), "bConfigurationValue (%d) ", configuration->unsigned32BitValue());strlcat(logString, tempString, sizeof(logString));  }
			if ( interfaceNumberPropertyMatches ) { match = true; snprintf(tempString, sizeof(tempString), "bInterfaceNumber (%d) ", interfaceNumber->unsigned32BitValue());strlcat(logString, tempString, sizeof(logString));  }
			if ( interfaceClassPropertyMatches ) { match = true; snprintf(tempString, sizeof(tempString), "bInterfaceClass (%d) ", interfaceClass->unsigned32BitValue()); strlcat(logString, tempString, sizeof(logString)); }
			if ( interfaceSubClassPropertyMatches ) { match = true; snprintf(tempString, sizeof(tempString), "bInterfaceSubClass (%d) ", interfaceSubClass->unsigned32BitValue());strlcat(logString, tempString, sizeof(logString));  }
			if ( interfaceProtocolPropertyMatches ) { match = true; snprintf(tempString, sizeof(tempString), "bInterfaceProtocol (%d) ", protocol->unsigned32BitValue());strlcat(logString, tempString, sizeof(logString));  }
			if ( !match ) strlcat(logString, "no properties", sizeof(logString));
			
			USBLog(6, "%s", logString);
			
			snprintf(logString, sizeof(logString), "\tDidn't Match: ");
			
			match = false;
			if ( !vendorPropertyMatches && dictVendor ) 
			{ 
				match = true; 
				snprintf(tempString, sizeof(tempString), "idVendor (%d,%d) ", dictVendor->unsigned32BitValue(), vendor->unsigned32BitValue()); 
				strlcat(logString, tempString, sizeof(logString)); 
			}
			if ( !productPropertyMatches && dictProduct) 
			{ 
				match = true; 
				snprintf(tempString, sizeof(tempString), "idProduct (%d,%d) ", dictProduct->unsigned32BitValue(), product->unsigned32BitValue()); 
				strlcat(logString, tempString, sizeof(logString)); 
			}
			if ( !deviceReleasePropertyMatches && dictRelease) 
			{ 
				match = true; 
				snprintf(tempString, sizeof(tempString), "bcdDevice (%d,%d) ", dictRelease->unsigned32BitValue(), release->unsigned32BitValue()); 
				strlcat(logString, tempString, sizeof(logString)); 
			}
			if ( !configurationValuePropertyMatches && dictConfiguration) 
			{ 
				match = true; 
				snprintf(tempString, sizeof(tempString), "bConfigurationValue (%d,%d) ", dictConfiguration->unsigned32BitValue(), configuration->unsigned32BitValue()); 
				strlcat(logString, tempString, sizeof(logString)); 
			}
			if ( !interfaceNumberPropertyMatches && dictInterfaceNumber) 
			{ 
				match = true; 
				snprintf(tempString, sizeof(tempString), "bInterfaceNumber (%d,%d) ", dictInterfaceNumber->unsigned32BitValue(), interfaceNumber->unsigned32BitValue()); 
				strlcat(logString, tempString, sizeof(logString)); 
			}
			if ( !interfaceClassPropertyMatches && dictInterfaceClass) 
			{ 
				match = true; 
				snprintf(tempString, sizeof(tempString), "bInterfaceClass (%d,%d) ", dictInterfaceClass->unsigned32BitValue(), interfaceClass->unsigned32BitValue()); 
				strlcat(logString, tempString, sizeof(logString)); 
			}
			if ( !interfaceSubClassPropertyMatches && dictInterfaceSubClass) 
			{ 
				match = true; 
				snprintf(tempString, sizeof(tempString), "bInterfaceSubClass (%d,%d) ", dictInterfaceSubClass->unsigned32BitValue(), interfaceSubClass->unsigned32BitValue()); 
				strlcat(logString, tempString, sizeof(logString)); 
			}
			if ( !interfaceProtocolPropertyMatches && dictProtocol) 
			{ 
				match = true; 
				snprintf(tempString, sizeof(tempString), "bDeviceProtocol (%d,%d) ", dictProtocol->unsigned32BitValue(), protocol->unsigned32BitValue()); 
				strlcat(logString, tempString, sizeof(logString)); 
			}
			if ( !match ) strlcat(logString, "nothing", sizeof(logString));
			
			USBLog(6, "%s", logString);
		}
    }
	
	if (interfaceClassProp)
		interfaceClassProp->release();
	
	return returnValue;
}



IOUSBPipe * 
IOUSBInterface::GetPipeObj(UInt8 index) 
{ 
	IOUSBPipe *	thePipeObj = NULL;
	IOReturn	err = kIOReturnSuccess;
	
    if (_expansionData && _GATE && _WORKLOOP)
    {
		IOCommandGate *	gate = _GATE;
		IOWorkLoop *	workLoop = _WORKLOOP;
		
		retain();
		workLoop->retain();
		gate->retain();

		err = gate->runAction(_GetPipeObj, (void *)index, (void *)&thePipeObj, (void *)NULL, (void *)NULL);
		if ( err != kIOReturnSuccess )
		{
			USBLog(2,"%s[%p]:GetPipeObj _GetPipeObj runAction failed (0x%x)", getName(), this, err);
		}
		
		gate->release();
		workLoop->release();
		release();
	}
	
	return thePipeObj;
}


IOReturn
IOUSBInterface::_GetPipeObj(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
#pragma unused (param3, param4)
	
    IOUSBInterface*				me	= OSDynamicCast(IOUSBInterface, target);
    UInt8						index = (uintptr_t) param1;
    IOUSBPipe **				thePipeObj = (IOUSBPipe **) param2;
	
    if (!me)
    {
        USBLog(1, "IOUSBInterface::_GetPipeObj - invalid target");
        return kIOReturnBadArgument;
    }
	
    *thePipeObj = me->GetPipeObjGated(index);
	
    return kIOReturnSuccess;
}


IOUSBPipe * 
IOUSBInterface::GetPipeObjGated(UInt8 index) 
{ 
	IOUSBPipe *	thePipeObj = NULL;
	
    thePipeObj = (index < kUSBMaxPipes) ? _pipeList[index] : NULL ; 
	
	return thePipeObj;
}



UInt8  
IOUSBInterface::GetConfigValue()
{ 
    return _configDesc->bConfigurationValue; 
}



IOUSBDevice * 
IOUSBInterface::GetDevice() 
{ 
    return _device; 
}



UInt8  
IOUSBInterface::GetInterfaceNumber() 
{ 
    return _bInterfaceNumber; 
}



UInt8  
IOUSBInterface::GetAlternateSetting() 
{ 
    return _bAlternateSetting; 
}



UInt8  
IOUSBInterface::GetNumEndpoints() 
{ 
    return _bNumEndpoints; 
}



UInt8  
IOUSBInterface::GetInterfaceClass() 
{ 
    return _bInterfaceClass; 
}



UInt8  
IOUSBInterface::GetInterfaceSubClass() 
{ 
    return _bInterfaceSubClass; 
}



UInt8  
IOUSBInterface::GetInterfaceProtocol() 
{ 
    return _bInterfaceProtocol; 
}



UInt8  
IOUSBInterface::GetInterfaceStringIndex() 
{ 
    return _iInterface; 
}



IOReturn  
IOUSBInterface::DeviceRequest(IOUSBDevRequest *request, IOUSBCompletion *completion)
{ 
    return _device->DeviceRequest(request, completion); 
}



IOReturn  
IOUSBInterface::DeviceRequest(IOUSBDevRequestDesc *request, IOUSBCompletion *completion)
{ 
    return _device->DeviceRequest(request, completion); 
}



OSMetaClassDefineReservedUsed(IOUSBInterface,  0);
IOReturn
IOUSBInterface::GetEndpointProperties(UInt8 alternateSetting, UInt8 endpointNumber, UInt8 direction, UInt8 *transferType, UInt16 *maxPacketSize, UInt8 *interval)
{
    const IOUSBDescriptorHeader 	*next, *next2;
    const IOUSBInterfaceDescriptor 	*ifdesc = NULL;
    const IOUSBEndpointDescriptor 	*endp = NULL;
    UInt8				endpointAddress = endpointNumber | ((direction == kUSBIn) ? 0x80 : 0x00);

    next = (const IOUSBDescriptorHeader *)_configDesc;

    while( (next = _device->FindNextDescriptor(next, kUSBInterfaceDesc))) 
    {
        ifdesc = (const IOUSBInterfaceDescriptor *)next;
        if ((ifdesc->bInterfaceNumber == _bInterfaceNumber) && (ifdesc->bAlternateSetting == alternateSetting))
	{
	    next2 = next;
	    while ( (next2 = FindNextAssociatedDescriptor(next2, kUSBEndpointDesc)))
	    {
		endp = (const IOUSBEndpointDescriptor*)next2;
		if (endp->bEndpointAddress == endpointAddress)
		{
		    *transferType = endp->bmAttributes & 0x03;
		    *maxPacketSize = mungeMaxPacketSize(USBToHostWord(endp->wMaxPacketSize));
		    *interval = endp->bInterval;
		    USBLog(6, "%s[%p]::GetEndpointProperties - tt=%d, mps=%d, int=%d", getName(), this, *transferType, *maxPacketSize, *interval);
		    return kIOReturnSuccess;
		}
	    }
	}
    }
    return kIOUSBEndpointNotFound;
}

OSMetaClassDefineReservedUnused(IOUSBInterface,  2);
OSMetaClassDefineReservedUnused(IOUSBInterface,  3);
OSMetaClassDefineReservedUnused(IOUSBInterface,  4);
OSMetaClassDefineReservedUnused(IOUSBInterface,  5);
OSMetaClassDefineReservedUnused(IOUSBInterface,  6);
OSMetaClassDefineReservedUnused(IOUSBInterface,  7);
OSMetaClassDefineReservedUnused(IOUSBInterface,  8);
OSMetaClassDefineReservedUnused(IOUSBInterface,  9);
OSMetaClassDefineReservedUnused(IOUSBInterface,  10);
OSMetaClassDefineReservedUnused(IOUSBInterface,  11);
OSMetaClassDefineReservedUnused(IOUSBInterface,  12);
OSMetaClassDefineReservedUnused(IOUSBInterface,  13);
OSMetaClassDefineReservedUnused(IOUSBInterface,  14);
OSMetaClassDefineReservedUnused(IOUSBInterface,  15);
OSMetaClassDefineReservedUnused(IOUSBInterface,  16);
OSMetaClassDefineReservedUnused(IOUSBInterface,  17);
OSMetaClassDefineReservedUnused(IOUSBInterface,  18);
OSMetaClassDefineReservedUnused(IOUSBInterface,  19);


