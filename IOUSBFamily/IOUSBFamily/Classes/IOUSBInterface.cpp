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


//================================================================================================
//
//   Headers
//
//================================================================================================
//
#include <libkern/OSByteOrder.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSData.h>

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

#define _gate			_expansionData->_gate
#define _workLoop		_expansionData->_workLoop
#define _needToClose	_expansionData->_needToClose


//================================================================================================
//
//   IOUSBInterface Methods
//
//================================================================================================
//
OSDefineMetaClassAndStructors(IOUSBInterface, IOUSBNub)



bool 
IOUSBInterface::init(const IOUSBConfigurationDescriptor *cfdesc,
                     const IOUSBInterfaceDescriptor *ifdesc)
{
    if(!ifdesc || !cfdesc)
        return false;

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

    return (true);
}



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
    if(offset) 
    {
        UInt16	interfaceNumber;
        char	location[32];
        
        interfaceNumber = offset->unsigned16BitValue();
        offset->release();
        sprintf( location, "%x", interfaceNumber );
        setLocation(location);
    }
	
    if(_iInterface) 
    {
        IOReturn err;
        char name[128];
        
        err = _device->GetStringDescriptor(_iInterface, name, sizeof(name));
        if(err == kIOReturnSuccess)
            setName(name);
    }
    
	
	if ( _bInterfaceClass == 8 )
	{
		// Mass Storage Class device. Let's see if we can put generate a GUID
		//
		OSObject * propertyObj = _device->copyProperty("USB Serial Number");
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
						USBLog(4, "%s[%p]::SetProperties  Serial #%s character %ld (0x%x) out of range #1", getName(), this, serial, j, serial[j]);
						valid = false;
						break;
					}
					if ( (serial[j] > 0x39) && (serial[j] < 0x41) )
					{
						USBLog(4, "%s[%p]::SetProperties  Serial #%s character %ld (0x%x) out of range #2", getName(), this, serial, j, serial[j]);
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
			}
			else
				USBLog(4, "%s[%p]::SetProperties  Mass Storage device but serial # is only %ld characters", getName(), this, length);
		}
		if (propertyObj)
			propertyObj->release();
	}
}



bool 
IOUSBInterface::start(IOService *provider)
{
    if ( !super::start(provider))
        return false;

    _gate = IOCommandGate::commandGate(this);

    if(!_gate)
    {
        USBError(1, "%s[%p]::start - unable to create command gate", getName(), this);
        goto ErrorExit;
    }

    _workLoop = getWorkLoop();
    if (!_workLoop)
    {
        USBError(1, "%s[%p]::start - unable to find my workloop", getName(), this);
        goto ErrorExit;
    }
    _workLoop->retain();

    if (_workLoop->addEventSource(_gate) != kIOReturnSuccess)
    {
        USBError(1, "%s[%p]::start - unable to add gate to work loop", getName(), this);
        goto ErrorExit;
    }

    // now that I am attached to a device, I can fill in my property table for matching
    SetProperties();

    if ( _device )
    {
		OSObject * propertyObj = _device->copyProperty(kUSBDevicePropertyLocationID);
        OSNumber *	locationID = OSDynamicCast( OSNumber, propertyObj );
        if ( locationID )
            setProperty(kUSBDevicePropertyLocationID,locationID->unsigned32BitValue(), 32);
		
		if (propertyObj)
			propertyObj->release();
		
		if (_device != provider)
		{
			USBError(1, "IOUSBInterface::start - _device[%p] != provider[%p]", _device, provider);
		}
		USBLog(6, "IOUSBInterface[%p]::start - opening device[%p]", this, _device);
		_device->open(this);
    }

    return true;

ErrorExit:

        if ( _gate != NULL )
        {
            _gate->release();
            _gate = NULL;
        }

    if ( _workLoop != NULL )
    {
        _workLoop->release();
        _workLoop = NULL;
    }

    return false;

}



bool 
IOUSBInterface::attach(IOService *provider)
{
    IOUSBDevice * device = OSDynamicCast(IOUSBDevice, provider);
    
    if(!device)
		return false;
        
    if( !super::attach(provider))
        return false;

    _device = device;

    return true;
}


bool 
IOUSBInterface::finalize(IOOptionBits options)
{
    bool ret;

    USBLog(7, "+%s[%p]::finalize (options = 0x%lx)", getName(), this, (UInt32) options);

    ret = super::finalize(options);

    USBLog(7, "-%s[%p]::finalize (options = 0x%lx)", getName(), this, (UInt32) options);

    return ret;
}

void
IOUSBInterface::free()
{
    IOReturn ret = kIOReturnSuccess;

    //  This needs to be the LAST thing we do, as it disposes of our "fake" member
    //  variables.
    //
    if (_expansionData)
    {
        if (_gate)
        {
            if (_workLoop)
            {
                ret = _workLoop->removeEventSource(_gate);
                _workLoop->release();
                _workLoop = NULL;
            }

            _gate->release();
            _gate = NULL;
        }

        IOFree(_expansionData, sizeof(ExpansionData));
        _expansionData = NULL;
    }
    
    super::free();
}

IOReturn
IOUSBInterface::CallSuperOpen(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
    IOUSBInterface *	me = OSDynamicCast(IOUSBInterface, target);
    bool		result;
    IOReturn		ret = kIOReturnSuccess;
    IOService *		forClient = (IOService *) param1;
    IOOptionBits 	options = (IOOptionBits ) param2;
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
    IOUSBInterface *	me = OSDynamicCast(IOUSBInterface, target);
    bool		result;
    IOReturn		ret = kIOReturnSuccess;
    IOService *		forClient = (IOService *) param1;
    IOOptionBits 	options = (IOOptionBits ) param2;
    
    if (!me)
    {
        USBLog(1, "IOUSBInterface::CallSuperClose - invalid target");
        return kIOReturnBadArgument;
    }
    
    me->super::close(forClient, options);
 
    return kIOReturnSuccess;
}


void 
IOUSBInterface::ClosePipes(void)
{
    IOUSBPipe * pipe;

    USBLog(7,"+%s[%p]::ClosePipes", getName(), this);

    for( unsigned int i=0; i < kUSBMaxPipes; i++) 
    {
        if( (pipe = _pipeList[i])) 
        {
            pipe->Abort(); 
            pipe->ClosePipe();
            pipe->release();
            _pipeList[i] = NULL;
        }
    }

    USBLog(7,"-%s[%p]::ClosePipes", getName(), this);
}


IOReturn
IOUSBInterface::CreatePipes(void)
{
    unsigned int 		i = 0;
    const IOUSBDescriptorHeader *pos = NULL;			// start with the interface descriptor
    bool			makePipeFailed = false;
    IOReturn			res = kIOReturnSuccess;
    
    while ((pos = FindNextAssociatedDescriptor(pos, kUSBEndpointDesc))) 
    {
        // Don't open twice!
        if(_pipeList[i] == NULL) 
            _pipeList[i] = _device->MakePipe((const IOUSBEndpointDescriptor *)pos, this);
        
        if(_pipeList[i] == NULL) 
        {
            makePipeFailed = true;
        }
            
        i++;
    }
    
   // Relax the checkin on whether the # of pipes created was the same as the bNumEndpoints
    // specified in the interface descriptor.  Instead of failing to create the pipes, we will
    // now write out a message to the log
    //
    if ( (i != _bNumEndpoints) && !makePipeFailed )
    {
        if (i < _bNumEndpoints)
            USBLog(3, "%s[%p]: NOTE: Interface descriptor defines more endpoints (bNumEndpoints = %d, descriptors = %d) than endpoint descriptors", getName(), this, _bNumEndpoints, i);
        else
            USBLog(3, "%s[%p]: NOTE: Interface descriptor defines less endpoints (bNumEndpoints = %d, descriptors = %d) than endpoint descriptors", getName(), this, _bNumEndpoints, i);
    }

    if ( makePipeFailed )
    {
        USBLog(3,"%s[%p]: Failed to create all pipes.", getName(), this);

        // We don't set an error here so that we allow those pipes that were successfully created to coexist with some that were not
        // This is true for some devices that incorrectly specify an interrupt pipe with a polling interval of 0ms along with their other pipes
    }
    
    return res;
}


// Open all the pipes.
bool 
IOUSBInterface::open( IOService *forClient, IOOptionBits options, void *arg )
{
    bool			res = true;
    IOReturn		error = kIOReturnSuccess;
    bool			useGate = false;
    OSObject *		propertyObj = NULL;
	OSBoolean *		boolObj = NULL;
	
    // Check to see if we need to open the driver while holding the gate.  The USB Device Nub should
    // have the kCallInterfaceOpenWithGate property set.
    //
	if ( _device )
	{
		propertyObj = _device->copyProperty(kCallInterfaceOpenWithGate);
		boolObj = OSDynamicCast( OSBoolean, propertyObj);
		if ( boolObj && boolObj->isTrue() )
		{
			useGate = true;
		}
		if (propertyObj)
			propertyObj->release();
	}
    
    if ( _gate && useGate )
    {
        USBLog(6,"%s[%p]::open calling super::open with gate", getName(), this);
        error = _gate->runAction( CallSuperOpen, (void *)forClient, (void *)options, (void *)arg );
        if ( error != kIOReturnSuccess )
        {
            USBLog(1,"%s[%p]::open super::open failed (0x%x)", getName(), this, error);
            res = false;
        }
    }
    else
    {
        USBLog(6,"%s[%p]::open calling super::open with NO gate", getName(), this);
        res = super::open(forClient, options, arg);
    }
    
    if (!res)
        USBLog(1,"%s[%p]::open super::open failed (0x%x)", getName(), this, res);

    return res;
}



bool 
IOUSBInterface::handleOpen( IOService *forClient, IOOptionBits options, void *arg )
{
    UInt16		altInterface;
    IOReturn		err = kIOReturnSuccess;
        

    USBLog(6, "+%s[%p]::handleOpen (device %s)", getName(), this, _device->getName());
    if(!super::handleOpen(forClient, options, arg))
    {
        USBLog(1,"%s[%p]::handleOpen failing because super::handleOpen failed (someone already has it open)", getName(), this);
		return false;
    }

    if (options & kIOUSBInterfaceOpenAlt)
    {
        altInterface = (UInt16)(UInt32)arg;
        
        // Note that SetAlternateInterface will actually create the pipes
        //
        USBLog(5, "%s[%p]::handleOpen calling SetAlternateInterface(%d)", getName(), this, altInterface);
        err =  SetAlternateInterface(forClient, altInterface);
        if ( err != kIOReturnSuccess) 
            USBError(1, "%s[%p]::handleOpen: SetAlternateInterface failed (0x%x)", getName(), this, err);
        
    }
    else
    {
        err = CreatePipes();
        if ( err != kIOReturnSuccess) 
            USBError(1, "%s[%p]::handleOpen: CreatePipes failed (0x%x)", getName(), this, err);
    }
    
    if (err != kIOReturnSuccess)
    {
        close(forClient);
        return false;
    }

    return true;
}


void 
IOUSBInterface::close( IOService *forClient, IOOptionBits options)
{
    IOReturn		error = kIOReturnSuccess;
    bool			useGate = false;
    OSObject *		propertyObj = NULL;
	OSBoolean *		boolObj = NULL;
    
    // Check to see if we need to open the driver while holding the gate.  The USB Device Nub should
    // have the kCallInterfaceOpenWithGate property set.
    //
	if ( _device )
	{
		propertyObj = _device->copyProperty(kCallInterfaceOpenWithGate);
		boolObj = OSDynamicCast( OSBoolean, propertyObj);
		if ( boolObj && boolObj->isTrue() )
		{
			useGate = true;
		}
		if (propertyObj)
			propertyObj->release();
	}

    if ( _gate && useGate )
    {
        USBLog(6,"%s[%p]::close calling super::close with gate", getName(), this);
        (void) _gate->runAction( CallSuperClose, (void *)forClient, (void *)options);
    }
    else
    {
        USBLog(6,"%s[%p]::close calling super::close with NO gate", getName(), this);
        super::close(forClient, options);
    }
}



void 
IOUSBInterface::handleClose(IOService *	forClient, IOOptionBits	options )
{
    IOUSBPipe 		*pipe;
    unsigned int	i;
    // Don't tear down the pipes when we get closed.  Do it in finalize. (Radar #2607982)

    // however, we do want to go ahead and abort any transactions left on those pipes.
    //
    USBLog(7,"+IOUSBInterface[%p]::handleClose", this);

    for( i=0; i < kUSBMaxPipes; i++) 
        if( (pipe = _pipeList[i])) 
            pipe->Abort(); 
    
    super::handleClose(forClient, options);

	if (_needToClose)
	{
		USBLog(3,"IOUSBInterface[%p]::handleClose - now closing our provider from deferred close", this);
		_device->close(this);
		_needToClose = false;
	}
    USBLog(7,"-IOUSBInterface[%p]::handleClose", this);
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
    const IOUSBController::Endpoint *	endpoint;
    IOUSBPipe *				pipe;
    int					numEndpoints;
    int					i;

    numEndpoints = _bNumEndpoints;

    if (request == 0)
        return NULL;

    if(current != 0) 
    {
        for(i=0;i < numEndpoints; i++) 
        {
            if(_pipeList[i] == current) 
            {
		i++; // Skip the one we just did
                break;
            }
	}
    }
    else
        i = 0;	// Start at beginning.

    for ( ;i < numEndpoints; i++) {
        pipe = _pipeList[i];
        if(!pipe)
            continue;
        endpoint = pipe->GetEndpoint();

        // check the request parameters
        if (request->type != kUSBAnyType &&
            request->type != endpoint->transferType)
            pipe = 0;		// this is not it
        else if (request->direction != kUSBAnyDirn &&
            request->direction != endpoint->direction)
            pipe = 0;		// this is not it

        if (pipe == 0)
            continue;

        request->type = endpoint->transferType;
        request->direction = endpoint->direction;
        request->maxPacketSize = endpoint->maxPacketSize;
        request->interval = endpoint->interval;
        return(pipe);
    }

    return(0);
}



const IOUSBDescriptorHeader *
IOUSBInterface::FindNextAssociatedDescriptor(const void *current, UInt8 type)
{
    const IOUSBDescriptorHeader *next;

    if(current == NULL)
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
        close(forClient);
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
    bool 	returnValue = true;
    SInt32	propertyScore = *score;
    OSString	*userClientInitMatchKey;
    char	logString[256]="";
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
	productPropertyMatches = USBComparePropertyWithMask( table, kUSBProductID, kUSBProductIDMask);
	usedMaskForProductID = productPropertyMatches;
    }
    
    // If the service object wishes to compare some of its properties in its
    // property table against the supplied matching dictionary,
    // it should do so in this method and return truth on success.
    //
    if (!super::matchPropertyTable(table))  return false;
    
    // something of a hack. We need the IOUSBUserClientInit "driver" to match in order
    // for us to be able to find the user mode plugin. However, that driver can't
    // effectively match using the USB Common Class Spec, so we special case it as a
    // short circuit
    userClientInitMatchKey = OSDynamicCast(OSString, table->getObject(kIOMatchCategoryKey));
    if (userClientInitMatchKey && !strcmp(userClientInitMatchKey->getCStringNoCopy(), "IOUSBUserClientInit"))
    {
        USBLog(6, "%s[%p]: matching IOUSBUserClientInit", getName(), this);
        *score = 9000;
        return true;
    }
    
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
    if ( gKernelDebugLevel > 4 )
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
	    USBLog(5,"Finding driver for interface #%d of %s, matching personality using %s, score: %ld, wildCard = %ld", _bInterfaceNumber, _device->getName(), identifier->getCStringNoCopy(), *score, wildCardMatches);
	else
	    USBLog(6,"Finding driver for interface #%d of %s, matching user client dictionary, score: %ld", _bInterfaceNumber, _device->getName(), *score);
	
	if ( vendor && product && release && configuration && interfaceNumber && interfaceClass && interfaceSubClass && protocol )
	{
	    char tempString[256]="";
	    
	    sprintf(logString,"\tMatched: ");
	    match = false;
	    if ( vendorPropertyMatches ) { match = true; sprintf(tempString,"idVendor (%d) ", vendor->unsigned32BitValue()); strcat(logString, tempString); }
	    if ( productPropertyMatches ) { match = true; sprintf(tempString,"idProduct (%d) ", product->unsigned32BitValue()); strcat(logString, tempString); }
	    if ( usedMaskForProductID && dictMask && dictProduct) { sprintf(tempString,"to (%d) with mask (0x%x), ", dictProduct->unsigned32BitValue(), dictMask->unsigned32BitValue()); strcat(logString, tempString);}
	    if ( deviceReleasePropertyMatches ) { match = true; sprintf(tempString,"bcdDevice (%d) ", release->unsigned32BitValue()); strcat(logString, tempString); }
	    if ( configurationValuePropertyMatches ) { match = true; sprintf(tempString,"bConfigurationValue (%d) ", configuration->unsigned32BitValue());strcat(logString, tempString);  }
	    if ( interfaceNumberPropertyMatches ) { match = true; sprintf(tempString,"bInterfaceNumber (%d) ", interfaceNumber->unsigned32BitValue());strcat(logString, tempString);  }
	    if ( interfaceClassPropertyMatches ) { match = true; sprintf(tempString,"bInterfaceClass (%d) ", interfaceClass->unsigned32BitValue()); strcat(logString, tempString); }
	    if ( interfaceSubClassPropertyMatches ) { match = true; sprintf(tempString,"bInterfaceSubClass (%d) ", interfaceSubClass->unsigned32BitValue());strcat(logString, tempString);  }
	    if ( interfaceProtocolPropertyMatches ) { match = true; sprintf(tempString,"bInterfaceProtocol (%d) ", protocol->unsigned32BitValue());strcat(logString, tempString);  }
	    if ( !match ) strcat(logString, "no properties");
	    
	    USBLog(6,logString);
	    
	    sprintf(logString,"\tDidn't Match: ");
	    
	    match = false;
	    if ( !vendorPropertyMatches && dictVendor ) 
	    { 
		match = true; 
		sprintf(tempString,"idVendor (%d,%d) ", dictVendor->unsigned32BitValue(), vendor->unsigned32BitValue()); 
		strcat(logString, tempString); 
	    }
	    if ( !productPropertyMatches && dictProduct) 
	    { 
		match = true; 
		sprintf(tempString,"idProduct (%d,%d) ", dictProduct->unsigned32BitValue(), product->unsigned32BitValue()); 
		strcat(logString, tempString); 
	    }
	    if ( !deviceReleasePropertyMatches && dictRelease) 
	    { 
		match = true; 
		sprintf(tempString,"bcdDevice (%d,%d) ", dictRelease->unsigned32BitValue(), release->unsigned32BitValue()); 
		strcat(logString, tempString); 
	    }
	    if ( !configurationValuePropertyMatches && dictConfiguration) 
	    { 
		match = true; 
		sprintf(tempString,"bConfigurationValue (%d,%d) ", dictConfiguration->unsigned32BitValue(), configuration->unsigned32BitValue()); 
		strcat(logString, tempString); 
	    }
	    if ( !interfaceNumberPropertyMatches && dictInterfaceNumber) 
	    { 
		match = true; 
		sprintf(tempString,"bInterfaceNumber (%d,%d) ", dictInterfaceNumber->unsigned32BitValue(), interfaceNumber->unsigned32BitValue()); 
		strcat(logString, tempString); 
	    }
	    if ( !interfaceClassPropertyMatches && dictInterfaceClass) 
	    { 
		match = true; 
		sprintf(tempString,"bInterfaceClass (%d,%d) ", dictInterfaceClass->unsigned32BitValue(), interfaceClass->unsigned32BitValue()); 
		strcat(logString, tempString); 
	    }
	    if ( !interfaceSubClassPropertyMatches && dictInterfaceSubClass) 
	    { 
		match = true; 
		sprintf(tempString,"bInterfaceSubClass (%d,%d) ", dictInterfaceSubClass->unsigned32BitValue(), interfaceSubClass->unsigned32BitValue()); 
		strcat(logString, tempString); 
	    }
	    if ( !interfaceProtocolPropertyMatches && dictProtocol) 
	    { 
		match = true; 
		sprintf(tempString,"bDeviceProtocol (%d,%d) ", dictProtocol->unsigned32BitValue(), protocol->unsigned32BitValue()); 
		strcat(logString, tempString); 
	    }
	    if ( !match ) strcat(logString,"nothing");
	    
	    USBLog(6,logString);
	}
    }
	
	if (interfaceClassProp)
		interfaceClassProp->release();
		
	return returnValue;
}

IOReturn 
IOUSBInterface::message( UInt32 type, IOService * provider,  void * argument )
{
    IOReturn	err = kIOReturnSuccess;
    
    switch ( type )
    {
        case kIOUSBMessagePortHasBeenReset:
            // Forward the message to our clients
            //
            messageClients( kIOUSBMessagePortHasBeenReset, this, NULL);
            break;
  
       case kIOUSBMessagePortHasBeenResumed:
            // Forward the message to our clients
            //
            messageClients( kIOUSBMessagePortHasBeenResumed, this, NULL);
            break;
  
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



bool
IOUSBInterface::didTerminate( IOService * provider, IOOptionBits options, bool * defer )
{
	if (_device && (_device == provider))
	{
		USBLog(6, "IOUSBInterface[%p]::didTerminate - closing _device", this);
		if (isOpen())
		{
			USBLog(3, "IOUSBInterface[%p]::didTerminate - deferring close because someone still has us open", this);
			_needToClose = true;
		}
		else
		{
			_device->close(this);
		}
	}
	else
	{
		USBError(1, "IOUSBInterface[%p]::didTerminate - not closing device[%p] provider[%p]", this, _device, provider);
	}
	return true;
}



void 
IOUSBInterface::stop( IOService * provider )
{

    USBLog(7,"+%s[%p]::stop (provider = %p)", getName(), this, provider);

    ClosePipes();
    super::stop(provider);

    USBLog(7,"-%s[%p]::stop (provider = %p)", getName(), this, provider);

}



IOUSBPipe * 
IOUSBInterface::GetPipeObj(UInt8 index) 
{ 
    return (index < kUSBMaxPipes) ? _pipeList[index] : NULL ; 
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

UInt8 
IOUSBInterface::hex2char( UInt8 digit )
{
	return ( (digit & 0x000f ) > 9 ? (digit & 0x000f) + 0x37 : (digit & 0x000f) + 0x30);
}


OSMetaClassDefineReservedUnused(IOUSBInterface,  1);
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

