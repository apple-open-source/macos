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

#include <libkern/c++/OSDictionary.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <libkern/c++/OSData.h>

#include <IOKit/usb/USB.h>
#include <IOKit/usb/IOUSBController.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/IOUSBRootHubDevice.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOMessage.h>
#include <IOKit/usb/IOUSBLog.h>
#include <UserNotification/KUNCUserNotifications.h>

#include <kern/thread_call.h>

#define super IOUSBNub

class IOUSBInterfaceIterator : public OSIterator 
{
    OSDeclareDefaultStructors(IOUSBInterfaceIterator)

protected:
    IOUSBFindInterfaceRequest fRequest;
    IOUSBDevice *fDevice;
    IOUSBInterface *fCurrent;

    virtual void free();

public:
    virtual bool init(IOUSBDevice *dev, IOUSBFindInterfaceRequest *reqIn);
    virtual void reset();

    virtual bool isValid();

    virtual OSObject *getNextObject();
};

OSDefineMetaClassAndStructors(IOUSBInterfaceIterator, OSIterator)

bool 
IOUSBInterfaceIterator::init(IOUSBDevice *dev, IOUSBFindInterfaceRequest *reqIn)
{
    if(!OSIterator::init())
	return false;
    fDevice = dev;
    fDevice->retain();
    fRequest = *reqIn;
    fCurrent = NULL;
    return true;
}



void 
IOUSBInterfaceIterator::free()
{
    if(fCurrent)
	fCurrent->release();
        
    fDevice->release();
    OSIterator::free();
}



void 
IOUSBInterfaceIterator::reset()
{
    if(fCurrent)
        fCurrent->release();
        
    fCurrent = NULL;
}



bool 
IOUSBInterfaceIterator::isValid()
{
    return true;
}



OSObject *
IOUSBInterfaceIterator::getNextObject()
{
    IOUSBInterface *next;

    next = fDevice->FindNextInterface(fCurrent, &fRequest);
    if (next)
	next->retain();
    if(fCurrent)
        fCurrent->release();
    fCurrent = next;
    return next;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors(IOUSBDevice, IOUSBNub)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOUSBDevice *
IOUSBDevice::NewDevice()
{
    return new IOUSBDevice;
}


void
IOUSBDevice::SetProperties()
{
    char		location [8];
    const struct IORegistryPlane * 	usbPlane; 
    
    // Note that the "PortNum" property is set by the USB Hub driver
    //
    OSNumber *	port = (OSNumber *) getProperty("PortNum");
    if ( port ) 
    {
        _portNumber = port->unsigned32BitValue();
    }
    
    // Save a reference to our parent in the USB plane
    //
    usbPlane = getPlane(kIOUSBPlane);
    if ( usbPlane )
    {
        _usbPlaneParent = OSDynamicCast( IOUSBDevice, getParentEntry( usbPlane ));
        _usbPlaneParent->retain();
    }
    
    // Set the IOKit location and locationID property for the device.  We need to first find the locationID for
    // this device's hub
    //
    if ( _usbPlaneParent )
    {
        OSNumber *	locationID = (OSNumber *) _usbPlaneParent->getProperty(kUSBDevicePropertyLocationID);    
        if ( locationID )
        {
            UInt32	childLocationID = GetChildLocationID( locationID->unsigned32BitValue(), _portNumber );
            setProperty(kUSBDevicePropertyLocationID, childLocationID,32);
            sprintf(location, "%x", (int) childLocationID);
            setLocation(location);
        }
    }
}



UInt32 
IOUSBDevice::GetChildLocationID(UInt32 parentLocationID, int port)
{
    // LocationID is used to uniquely identify a device or interface and it's
    // suppose to remain constant across reboots as long as the USB topology doesn't
    // change.  It is a 32-bit word.  The top 2 nibbles (bits 31:24) represent the
    // USB Bus Number.  Each nibble after that (e.g. bits 23:20 or 19:16) correspond
    // to the port number of the hub that the device is connected to.
    //
    SInt32	shift;
    UInt32	location = parentLocationID;
    
    // Find the first zero nibble and add the port number we are using
    //
    for ( shift = 20; shift >= 0; shift -= 4)
    {
        if ((location & (0x0f << shift)) == 0)
        {
            location |= (port & 0x0f) << shift;
            break;
        }
    }
    
    return location;
}



bool 
IOUSBDevice::init(USBDeviceAddress deviceAddress, UInt32 powerAvailable, UInt8 speed, UInt8 maxPacketSize)
{

    if(!super::init())
	return false;

    // allocate our expansion data
    if (!_expansionData)
    {
	_expansionData = (ExpansionData *)IOMalloc(sizeof(ExpansionData));
	if (!_expansionData)
	    return false;
	bzero(_expansionData, sizeof(ExpansionData));
    }
    
    _getConfigLock = IORecursiveLockAlloc();
    if (!_getConfigLock)
    {
	IOFree(_expansionData, sizeof(ExpansionData));
        USBError(1, "%s[%p]::init - Error allocating getConfigLock", getName(), this);
        return false;
    }
    
    _address = deviceAddress;
    _descriptor.bMaxPacketSize0 = maxPacketSize;
    _busPowerAvailable = powerAvailable;
    _speed = speed;
    _portResetThreadActive = false;
    _allowConfigValueOfZero = false;
    _portHasBeenReset = false;
    _deviceterminating = false;
    
    USBLog(5,"%s @ %d (%ldmA available, %s speed)", getName(), _address,_busPowerAvailable*2, (_speed == kUSBDeviceSpeedLow) ? "low" : "full");
    
    return true;
}



bool 
IOUSBDevice::finalize(IOOptionBits options)
{
    USBLog(5,"%s[%p]::finalize",getName(), this);
    
    if(_pipeZero) 
    {
        _pipeZero->Abort();
        _pipeZero->release();
        _pipeZero = NULL;
    }
    _currentConfigValue = 0;

    if (_doPortResetThread)
    {
        thread_call_cancel(_doPortResetThread);
        thread_call_free(_doPortResetThread);
    }
    
    if (_doPortSuspendThread)
    {
        thread_call_cancel(_doPortSuspendThread);
        thread_call_free(_doPortSuspendThread);
    }
    
    if ( _usbPlaneParent )
        _usbPlaneParent->release();
        
    return(super::finalize(options));
}



void 
IOUSBDevice::free()
{
    USBLog(5,"%s[%p]::free",getName(), this);
    if(_configList) 
    {
	int 	i;
        for(i=0; i<_descriptor.bNumConfigurations; i++) 
            if(_configList[i])
	    {
		_configList[i]->release();
		_configList[i] = NULL;
	    }
        IODelete(_configList, IOBufferMemoryDescriptor*, _descriptor.bNumConfigurations);
	_configList = NULL;
    }
    _currentConfigValue = 0;

    IORecursiveLockFree(_getConfigLock);
    
    //  This needs to be the LAST thing we do, as it disposes of our "fake" member
    //  variables.
    //
    if (_expansionData)
	IOFree(_expansionData, sizeof(ExpansionData));

    super::free();
}




bool 
IOUSBDevice::start( IOService * provider )
{
    IOReturn 		err;
    char 		name[128];
    UInt32		delay = 30;
    UInt32		retries = 4;
    bool		allowNumConfigsOfZero = false;
    AbsoluteTime	currentTime;
    UInt64		elapsedTime;
   
    if( !super::start(provider))
        return false;

    if (_controller == NULL)
        return false;
        
    // Don't do this until we have a controller
    //
    _endpointZero.bLength = sizeof(_endpointZero);
    _endpointZero.bDescriptorType = kUSBEndpointDesc;
    _endpointZero.bEndpointAddress = 0;
    _endpointZero.bmAttributes = kUSBControl;
    _endpointZero.wMaxPacketSize = HostToUSBWord(_descriptor.bMaxPacketSize0);
    _endpointZero.bInterval = 0;

    _pipeZero = IOUSBPipe::ToEndpoint(&_endpointZero, _speed, _address, _controller);
    
    // See if we have the allowNumConfigsOfZero errata. Some devices have an incorrect device descriptor
    // that specifies a bNumConfigurations value of 0.  We allow those devices to enumerate if we know
    // about them.
    //
    OSBoolean * boolObj = OSDynamicCast( OSBoolean, getProperty(kAllowNumConfigsOfZero) );
    if ( boolObj && boolObj->isTrue() )
        allowNumConfigsOfZero = true;

    // Attempt to read the full device descriptor.  We will attempt 5 times with a 30ms delay between each (that's 
    // what we do on MacOS 9
    //
    do 
    {
        err = GetDeviceDescriptor(&_descriptor, sizeof(_descriptor));
      
        // If the error is kIOReturnOverrun, we still received our 8 bytes, so signal no error. 
        //
        if ( err == kIOReturnOverrun )
        {
            err = kIOReturnSuccess;
        }
        
        if (err )
        {
            USBLog(3, "IOUSBDevice::start, GetDeviceDescriptor -- retrying. err = 0x%x", err);
            if ( retries == 2)
                delay = 3;
            else if ( retries == 1 )
                delay = 30;
            IOSleep( delay );
            retries--;
        }
	else
        {
            USBLog(6,"Device Descriptor Dump");
            USBLog(6,"\tbLength %d",_descriptor.bLength);
            USBLog(6,"\tbDescriptorType %d",_descriptor.bDescriptorType);
            USBLog(6,"\tbcdUSB %d", USBToHostWord(_descriptor.bcdUSB));
            USBLog(6,"\tbDeviceClass %d", _descriptor.bDeviceClass);
            USBLog(6,"\tbDeviceSubClass %d", _descriptor.bDeviceSubClass);
            USBLog(6,"\tbDeviceProtocol %d", _descriptor.bDeviceProtocol);
            USBLog(6,"\tbMaxPacketSize0 %d", _descriptor.bMaxPacketSize0);
            USBLog(6,"\tidVendor %d", USBToHostWord(_descriptor.idVendor));
            USBLog(6,"\tidProduct %d", USBToHostWord(_descriptor.idProduct));
            USBLog(6,"\tbcdDevice %d", USBToHostWord(_descriptor.bcdDevice));
            USBLog(6,"\tiManufacturer %d ", _descriptor.iManufacturer);
            USBLog(6,"\tiProduct %d ", _descriptor.iProduct);
            USBLog(6,"\tiSerialNumber %d", _descriptor.iSerialNumber);
            USBLog(6,"\tbNumConfigurations %d", _descriptor.bNumConfigurations);
        }
    }
    while ( err && retries > 0 );

    if ( err )
    {
        USBLog(3,"%s[%p]::start Couldn't get full device descriptor (0x%x)",getName(),this, err);
        return false;       
    }
    
    if(_descriptor.bNumConfigurations || allowNumConfigsOfZero) 
    {
	_configList = IONew(IOBufferMemoryDescriptor*, _descriptor.bNumConfigurations);
        if(!_configList)
            return false;
        bzero(_configList, sizeof(IOBufferMemoryDescriptor*) * _descriptor.bNumConfigurations);
    }
    else
    {
        // The device specified bNumConfigurations of 0, which is not legal (See Section 9.6.2 of USB 1.1).
        // However, we will not flag this as an error.
        //
        USBLog(3,"%s[%p]::start USB Device specified bNumConfigurations of 0, which is not legal", getName(), this);
    }
    
    if(_descriptor.iProduct) 
    {
        err = GetStringDescriptor(_descriptor.iProduct, name, sizeof(name));
	if(err == kIOReturnSuccess) 
        {
            if ( name[0] != 0 )
                setName(name);
            setProperty("USB Product Name", name);
	}
    }
    else
    {
        switch ( _descriptor.bDeviceClass )
        {
            case (kUSBCompositeClass):		setName("IOUSBCompositeDevice"); break;
            case (kUSBAudioClass):	 	setName("IOUSBAudioDevice"); break;
            case (kUSBCommClass):	 	setName("IOUSBCommunicationsDevice"); break;
            case (kUSBHIDClass):	 	setName("IOIUSBHIDDevice"); break;
            case (kUSBDisplayClass):		setName("IOUSBDisplayDevice"); break;
            case (kUSBPrintingClass):		setName("IOUSBPrintingDevice"); break;
            case (kUSBMassStorageClass): 	setName("IOUSBMassStorageDevice"); break;
            case (kUSBHubClass):	 	setName("IOUSBHubDevice"); break;
            case (kUSBDataClass):	 	setName("IOUSBDataDevice"); break;
            case (kUSBVendorSpecificClass):	setName("IOUSBVendorSpecificDevice"); break;
        }
    }

    if(_descriptor.iManufacturer) 
    {
        err = GetStringDescriptor(_descriptor.iManufacturer, name, sizeof(name));
        if(err == kIOReturnSuccess) 
        {
            setProperty("USB Vendor Name", name);
	}
    }
    if(_descriptor.iSerialNumber) 
    {
        err = GetStringDescriptor(_descriptor.iSerialNumber, name, sizeof(name));
        if(err == kIOReturnSuccess) 
        {
            setProperty("USB Serial Number", name);
        }
    }
    // these properties are used for matching (well, most of them are), and they come from the device descriptor
    setProperty(kUSBDeviceClass, (unsigned long long)_descriptor.bDeviceClass, (sizeof(_descriptor.bDeviceClass) * 8));
    setProperty(kUSBDeviceSubClass, (unsigned long long)_descriptor.bDeviceSubClass, (sizeof(_descriptor.bDeviceSubClass) * 8));
    setProperty(kUSBDeviceProtocol, (unsigned long long)_descriptor.bDeviceProtocol, (sizeof(_descriptor.bDeviceProtocol) * 8));
    setProperty(kUSBDeviceMaxPacketSize, (unsigned long long)_descriptor.bMaxPacketSize0, (sizeof(_descriptor.bMaxPacketSize0) * 8));
    setProperty(kUSBVendorID, (unsigned long long) USBToHostWord(_descriptor.idVendor), (sizeof(_descriptor.idVendor) * 8));
    setProperty(kUSBProductID, (unsigned long long) USBToHostWord(_descriptor.idProduct), (sizeof(_descriptor.idProduct) * 8));
    setProperty(kUSBDeviceReleaseNumber, (unsigned long long) USBToHostWord(_descriptor.bcdDevice), (sizeof(_descriptor.bcdDevice) * 8));
    setProperty(kUSBManufacturerStringIndex, (unsigned long long) _descriptor.iManufacturer, (sizeof(_descriptor.iManufacturer) * 8));
    setProperty(kUSBProductStringIndex, (unsigned long long) _descriptor.iProduct, (sizeof(_descriptor.iProduct) * 8));
    setProperty(kUSBSerialNumberStringIndex, (unsigned long long) _descriptor.iSerialNumber, (sizeof(_descriptor.iSerialNumber) * 8));
    setProperty(kUSBDeviceNumConfigs, (unsigned long long) _descriptor.bNumConfigurations, (sizeof(_descriptor.bNumConfigurations) * 8));

    // these properties are not in the device descriptor, but they are useful
    setProperty(kUSBDevicePropertySpeed, (unsigned long long) _speed, (sizeof(_speed) * 8));
    setProperty(kUSBDevicePropertyBusPowerAvailable, (unsigned long long) _busPowerAvailable, (sizeof(_busPowerAvailable) * 8));
    setProperty(kUSBDevicePropertyAddress, (unsigned long long) _address, (sizeof(_address) * 8));
        
    // Create a "SessionID" property for this device
    clock_get_uptime(&currentTime);
    absolutetime_to_nanoseconds(currentTime, &elapsedTime);
    setProperty("sessionID", elapsedTime, 64);

    // allocate a thread_call structure for reset and suspend
    //
    _doPortResetThread = thread_call_allocate((thread_call_func_t)ProcessPortResetEntry, (thread_call_param_t)this);
    _doPortSuspendThread = thread_call_allocate((thread_call_func_t)ProcessPortSuspendEntry, (thread_call_param_t)this);
    _doPortReEnumerateThread = thread_call_allocate((thread_call_func_t)ProcessPortReEnumerateEntry, (thread_call_param_t)this);
        
    // for now, we have no interfaces, so make sure the list is NULL and zero out other counts    
    _interfaceList = NULL;
    _currentConfigValue	= 0;		// unconfigured device
    _numInterfaces = 0;			// so no active interfaces
        
    return true;
}



bool 
IOUSBDevice::attach(IOService *provider)
{
    if( !super::attach(provider))
        return (false);

    // Set controller if we weren't given one earlier.
    //
    if(_controller == NULL)
		_controller = OSDynamicCast(IOUSBController, provider);
        
    if(_controller == NULL)
		return false;

    return true;
}

// Stop all activity, reset device, restart.  Will not renumerate the device
//
IOReturn 
IOUSBDevice::ResetDevice()
{
    UInt32	retries = 0;

    if ( _resetInProgress )
    {
        USBLog(5, "%s[%p] ResetDevice(%d) while in progress", getName(), this, _portNumber );
       return kIOReturnNotPermitted;
    }

    _resetInProgress = true;
    
    if ( isInactive() )
    {
        USBLog(1, "%s[%p]::ResetDevice - while terminating!", getName(), this);
        return kIOReturnNotResponding;
    }

    _portHasBeenReset = false;
    
    USBLog(3, "+%s[%p] ResetDevice for port %d", getName(), this, _portNumber );
    retain();
    thread_call_enter( _doPortResetThread );

    // Should we do a commandSleep/Wakeup here?
    //
    while ( !_portHasBeenReset && retries < 100 )
    {
        IOSleep(100);

        if ( isInactive() )
        {
            USBLog(3, "+%s[%p] isInactive() while waiting for reset to finish", getName(), this );
            break;
        }
        
        retries++;
    }
    
    USBLog(3, "-%s[%p] ResetDevice for port %d", getName(), this, _portNumber );

    _resetInProgress = false;
    
    return kIOReturnSuccess;
}

/******************************************************
 * Helper Methods
 ******************************************************/

const IOUSBConfigurationDescriptor *
IOUSBDevice::FindConfig(UInt8 configValue, UInt8 *configIndex)
{
    int i;
    const IOUSBConfigurationDescriptor *cd = NULL;

    USBLog(6, "%s[%p]:FindConfig (%d)",getName(), this, configValue);
   
    for(i = 0; i < _descriptor.bNumConfigurations; i++) 
    {
        cd = GetFullConfigurationDescriptor(i);
        if(!cd)
            continue;
        if(cd->bConfigurationValue == configValue)
            break;
    }
    if(cd && configIndex)
	*configIndex = i;

    return cd;
}


static IOUSBDescriptorHeader *
NextDescriptor(const void *desc)
{
    const UInt8 *next = (const UInt8 *)desc;
    UInt8 length = next[0];
    next = &next[length];
    return((IOUSBDescriptorHeader *)next);
}



const IOUSBDescriptorHeader*
IOUSBDevice::FindNextDescriptor(const void *cur, UInt8 descType)
{
    IOUSBDescriptorHeader 		*hdr;
    UInt8				configIndex;
    IOUSBConfigurationDescriptor	*curConfDesc;
    UInt16				curConfLength;
    UInt8				curConfig;
   
    if (!_configList)
	return NULL;
    
    if (!_currentConfigValue)
    {
	GetConfiguration(&curConfig);
	if (!_currentConfigValue && !_allowConfigValueOfZero)
	    return NULL;
    }

    curConfDesc = (IOUSBConfigurationDescriptor	*)FindConfig(_currentConfigValue, &configIndex);
    if (!curConfDesc)
	return NULL;

    if (!_configList[configIndex])		// have seen this happen - causes a panic!
	return NULL;

    curConfLength = _configList[configIndex]->getLength();
    if (!cur)
	hdr = (IOUSBDescriptorHeader*)curConfDesc;
    else
    {
	if ((cur < curConfDesc) || (((int)cur - (int)curConfDesc) >= curConfLength))
	{
	    return NULL;
	}
	hdr = (IOUSBDescriptorHeader *)cur;
    }

    do 
    {
	IOUSBDescriptorHeader 		*lasthdr = hdr;
	hdr = NextDescriptor(hdr);
	if (lasthdr == hdr)
	{
	    return NULL;
	}
	
        if(((int)hdr - (int)curConfDesc) >= curConfLength)
	{
            return NULL;
	}
        if(descType == 0)
	{
            return hdr;			// type 0 is wildcard.
	}
	    
        if(hdr->bDescriptorType == descType)
	{
            return hdr;
	}
    } while(true);
}



IOReturn
IOUSBDevice::FindNextInterfaceDescriptor(const IOUSBConfigurationDescriptor *configDescIn, 
					 const IOUSBInterfaceDescriptor *intfDesc,
                                         const IOUSBFindInterfaceRequest *request,
					 IOUSBInterfaceDescriptor **descOut)
{
    IOUSBConfigurationDescriptor *configDesc = (IOUSBConfigurationDescriptor *)configDescIn;
    IOUSBInterfaceDescriptor *interface, *end;
    
    if (!configDesc && _currentConfigValue)
        configDesc = (IOUSBConfigurationDescriptor*)FindConfig(_currentConfigValue, NULL);
    
    if (!configDesc || (configDesc->bDescriptorType != kUSBConfDesc))
	return kIOReturnBadArgument;
    
    end = (IOUSBInterfaceDescriptor *)(((UInt8*)configDesc) + USBToHostWord(configDesc->wTotalLength));
    
    if (intfDesc != NULL)
    {
	if (((void*)intfDesc < (void*)configDesc) || (intfDesc->bDescriptorType != kUSBInterfaceDesc))
	    return kIOReturnBadArgument;
	interface = (IOUSBInterfaceDescriptor *)NextDescriptor(intfDesc);
    }
    else
	interface = (IOUSBInterfaceDescriptor *)NextDescriptor(configDesc);
    while (interface < end)
    {
	if (interface->bDescriptorType == kUSBInterfaceDesc)
	{
	    if (((request->bInterfaceClass == kIOUSBFindInterfaceDontCare) || (request->bInterfaceClass == interface->bInterfaceClass)) &&
	    ((request->bInterfaceSubClass == kIOUSBFindInterfaceDontCare)  || (request->bInterfaceSubClass == interface->bInterfaceSubClass)) &&
	    ((request->bInterfaceProtocol == kIOUSBFindInterfaceDontCare)  || (request->bInterfaceProtocol == interface->bInterfaceProtocol)) &&
	    ((request->bAlternateSetting == kIOUSBFindInterfaceDontCare)   || (request->bAlternateSetting == interface->bAlternateSetting)))
	    {
		*descOut = interface;
		return kIOReturnSuccess;
	    }
	}
	interface = (IOUSBInterfaceDescriptor *)NextDescriptor(interface);
    }
   return kIOUSBInterfaceNotFound; 
}


// Finding the correct interface
/*
 * findNextInterface
 * This method should really be rewritten to use iterators or
 * be broken out into more functions without a request structure.
 * Or even better, make the interfaces and endpoint objects that
 * have their own methods for this stuff.
 *
 * returns:
 *   next interface matching criteria.  0 if no matches
 */
IOUSBInterface *
IOUSBDevice::FindNextInterface(IOUSBInterface *current, IOUSBFindInterfaceRequest *request)
{
    IOUSBInterfaceDescriptor 		*id = NULL;
    IOUSBInterface 			*intf = NULL;
    
    if (current)
    {
        // get the descriptor for the current interface into id
        
        // make sure it is really an authentic IOUSBInterface
        if (!OSDynamicCast(IOUSBInterface, current))
            return NULL;
	while (true)
	{
	    if (FindNextInterfaceDescriptor(NULL, id, request, &id) != kIOReturnSuccess)
		return NULL;
	    if (GetInterface(id) == current)
		break;
	}
    }
    
    while (!intf)
    {
	// now find either the first interface descriptor which matches, or the first one after current
	if (FindNextInterfaceDescriptor(NULL, id, request, &id) != kIOReturnSuccess)
	    return NULL;
    
	intf =  GetInterface(id);
	// since not all interfaces (e.g. alternate) are instantiated, we only terminate if we find one that is
    }
    return intf;
    
}



OSIterator *
IOUSBDevice::CreateInterfaceIterator(IOUSBFindInterfaceRequest *request)
{
    IOUSBInterfaceIterator *iter = new IOUSBInterfaceIterator;
    if(!iter)
	return NULL;

    if(!iter->init(this, request)) 
    {
	iter->release();
	iter = NULL;
    }
    return iter;
}



const IOUSBConfigurationDescriptor*
IOUSBDevice::GetFullConfigurationDescriptor(UInt8 index)
{
    IOReturn 			err;
    IOBufferMemoryDescriptor * 	localConfigPointer = NULL;
    
    if (!_configList || (index >= _descriptor.bNumConfigurations))
	return NULL;

    // it is possible that we could end up in a race condition with multiple drivers trying to get a config descriptor.
    // we are not able to fix that by running this code through the command gate, because then the actual bus command 
    // would not be able to complete with us holding the gate. so we have a device specific lock instead
    
    USBLog(7, "%s[%p]::GetFullConfigurationDescriptor - Index (%x) - about to obtain lock", getName(), this, index);
    IORecursiveLockLock(_getConfigLock);
    USBLog(7, "%s[%p]::GetFullConfigurationDescriptor - Index (%x) - lock obtained", getName(), this, index);
    
    if(_configList[index] == NULL) 
    {
        int 				len;
        IOUSBConfigurationDescHeader	temp;
	
	// 2755742 - workaround for a ill behaved device
	if ((USBToHostWord(_descriptor.idVendor) == 0x3f0) && (USBToHostWord(_descriptor.idProduct) == 0x1001))
	{
	    USBLog(3, "%s[%p]::GetFullConfigurationDescriptor - assuming config desc length of 39", getName(), this);
	    len = 39;
	}
	else
	{
	    USBLog(5, "%s[%p]::GetFullConfigurationDescriptor - Index (%x) - getting first %d bytes of config descriptor", getName(), this, index, sizeof(temp));
	    err = GetConfigDescriptor(index, &temp, sizeof(temp));
	    if (err) 
	    {
		USBError(1, "%s[%p]::GetFullConfigurationDescriptor - Error (%x) getting first %d bytes of config descriptor", getName(), this, err, sizeof(temp));
                IORecursiveLockUnlock(_getConfigLock);
		return NULL;
	    }
	    len = USBToHostWord(temp.wTotalLength);
	}

        localConfigPointer = IOBufferMemoryDescriptor::withCapacity(len, kIODirectionIn);

        if(!localConfigPointer)
        {
            USBError(1, "%s[%p]::GetFullConfigurationDescriptor - unable to get memory buffer", getName(), this);
            IORecursiveLockUnlock(_getConfigLock);
            return NULL;
        }
        
        USBLog(5, "%s[%p]::GetFullConfigurationDescriptor - Index (%x) - getting full %d bytes of config descriptor", getName(), this, index, len);
        err = GetConfigDescriptor(index, localConfigPointer->getBytesNoCopy(), len);
        if (err) 
	{
            USBError(1, "%s[%p]::GetFullConfigurationDescriptor - Error (%x) getting full %d bytes of config descriptor", getName(), this, err, len);
            
            if ( localConfigPointer )
            {
                localConfigPointer->release();
                localConfigPointer = NULL;
            }
            IORecursiveLockUnlock(_getConfigLock);
            return NULL;
        }
        else
        {
            // If we get to this point and configList[index] is NOT NULL, then it means that another thread already got the config descriptor.
            // In that case, let's just release our descriptor and return the already allocated one.
            //
            if ( _configList[index] != NULL )
                localConfigPointer->release();
            else
                _configList[index] = localConfigPointer;
        }
        
    }
    USBLog(7, "%s[%p]::GetFullConfigurationDescriptor - Index (%x) - about to release lock", getName(), this, index);
    IORecursiveLockUnlock(_getConfigLock);
    return (IOUSBConfigurationDescriptor *)_configList[index]->getBytesNoCopy();
}



IOReturn 
IOUSBDevice::GetDeviceDescriptor(IOUSBDeviceDescriptor *desc, UInt32 size)
{
    IOUSBDevRequest	request;

    USBLog(5,"********** GET DEVICE DESCRIPTOR (%d)**********", (int)size);

    if (!desc)
        return  kIOReturnBadArgument;

    request.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice);
    request.bRequest = kUSBRqGetDescriptor;
    request.wValue = kUSBDeviceDesc << 8;
    request.wIndex = 0;
    request.wLength = size;
    request.pData = desc;

    return DeviceRequest(&request, 5000, 0);
}



/*
 * GetConfigDescriptor:
 *	In: pointer to buffer for config descriptor, length to get
 * Assumes: desc is has enough space to put it all
 */
IOReturn
IOUSBDevice::GetConfigDescriptor(UInt8 configIndex, void *desc, UInt32 len)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevRequest	request;

    USBLog(5, "********** GET CONFIG DESCRIPTOR (%d)**********", (int)len);

    /*
     * with config descriptors, the device will send back all descriptors,
     * if the request is big enough.
     */

    request.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice);
    request.bRequest = kUSBRqGetDescriptor;
    request.wValue = (kUSBConfDesc << 8) + configIndex;
    request.wIndex = 0;
    request.wLength = len;
    request.pData = desc;

    err = DeviceRequest(&request, 5000, 0);

    if (err)
    {
        USBError(1,"%s[%p]: error getting device config descriptor. err=0x%x", getName(), this, err);
    }

    return(err);
}

void
IOUSBDevice::TerminateInterfaces()
{
    int i;
    
    USBLog(5,"%s[%p]: removing all interfaces and interface drivers",getName(),this);
    
    if (_interfaceList && _numInterfaces)
    {
	// Go through the list of interfaces and release them
        //
	for (i=0; i < _numInterfaces; i++)
	{
	    IOUSBInterface *intf = _interfaceList[i];
	    if (intf)
	    {
		// this call should do everything, including detaching the interface from us
                //
		intf->terminate(kIOServiceSynchronous);
	    }
	}
	// free the interface list memory
        //
	IODelete(_interfaceList, IOUSBInterface*, _numInterfaces);
	_interfaceList = NULL;
	_numInterfaces = 0;
    }
    
}

IOReturn 
IOUSBDevice::SetConfiguration(IOService *forClient, UInt8 configNumber, bool startMatchingInterfaces)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevRequest	request;
    const IOUSBConfigurationDescriptor *confDesc;
    bool		allowConfigValueOfZero = false;
 
    // See if we have the _allowConfigValueOfZero errata
    //
    OSBoolean * boolObj = OSDynamicCast( OSBoolean, getProperty(kAllowConfigValueOfZero) );
    if ( boolObj && boolObj->isTrue() )
        _allowConfigValueOfZero = true;
        
    // If we're not opened by our client, we can't set the configuration
    //
    if (!isOpen(forClient))
    {
	USBLog(3,"%s[%p]::SetConfiguration  Error: Client does not have device open",getName(), this);
	return kIOReturnExclusiveAccess;
    }
    
    confDesc = FindConfig(configNumber);

    if ( (configNumber || allowConfigValueOfZero) && !confDesc)
    {
	USBLog(3,"%s[%p]::SetConfiguration  Error: Could not find configuration (%d)",getName(), this, configNumber);
	return kIOUSBConfigNotFound;
    }
    
    // Do we need to check if the device is self_powered?
    if (confDesc && (confDesc->MaxPower > _busPowerAvailable))
    {
	USBLog(3,"%s[%p]::SetConfiguration  Not enough bus power to configure device",getName(), this);
	DisplayNotEnoughPowerNotice();
	return kIOUSBNotEnoughPowerErr;
    }

    //  Go ahead and remove all the interfaces that are attached to this
    //  device.
    //
    TerminateInterfaces();
    
    USBLog(5,"%s[%p]::SetConfiguration to %d",getName(), this, configNumber);

    request.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBStandard, kUSBDevice);
    request.bRequest = kUSBRqSetConfig;
    request.wValue = configNumber;
    request.wIndex = 0;
    request.wLength = 0;
    request.pData = 0;
    err = DeviceRequest(&request, 5000, 0);

    if (err)
    {
        USBError(1,"%s[%p]: error setting config. err=0x%x", getName(), this, err);
	return err;
    }

    // Update our global now that we've set the configuration successfully
    //
    _currentConfigValue = configNumber;

    // If the device is now configured (non zero) then instantiate interfaces and begin interface
    // matching if appropriate.  Here's how we deal with alternate settings:  We will look for the first
    // interface description and then instantiate all other interfaces that match the alternate setting
    // of the first interface.  Then, when we look for all the interfaces that are defined in the config
    // descriptor, we need to make sure that we find the ones with the appropriate alternate setting.
    //
    if (configNumber || _allowConfigValueOfZero)
    {
	int 		i;
	Boolean 	gotAlternateSetting = false;
        UInt8		altSetting = 0;
        
	if (!confDesc || !confDesc->bNumInterfaces)
        {
            USBLog(5,"%s[%p]::SetConfiguration  No confDesc (%p) or no bNumInterfaces",getName(), this, confDesc);
	    return kIOReturnNoResources;
        }
                
	_interfaceList = IONew(IOUSBInterface*, confDesc->bNumInterfaces);
	if (!_interfaceList)
        {
            USBLog(3,"%s[%p]::SetConfiguration  Could not create IOUSBInterface list",getName(), this );
	    return kIOReturnNoResources;
        }
        
	_numInterfaces = confDesc->bNumInterfaces;
	
	const IOUSBInterfaceDescriptor *intfDesc = NULL;
        
	for (i=0; i<_numInterfaces; i++)
	{
	    _interfaceList[i] = NULL;
	    intfDesc = (IOUSBInterfaceDescriptor *)FindNextDescriptor(intfDesc, kUSBInterfaceDesc);
            
            USBLog(5,"%s[%p]::SetConfiguration  Found an interface (%p) ",getName(), this, intfDesc );
            
            // Check to see whether this interface has the appropriate alternate setting.  If not, then
            // keep getting new ones until we exhaust the list or we match one with the correct setting.
            //
            while ( gotAlternateSetting && (intfDesc != NULL) && (intfDesc->bAlternateSetting != altSetting) )
            {
                intfDesc = (IOUSBInterfaceDescriptor *)FindNextDescriptor(intfDesc, kUSBInterfaceDesc);
            }

	    if (intfDesc)
            {
                if ( !gotAlternateSetting )
                {
                    // For the first interface, we don't know the alternate setting, so here is where we set it.
                    //
                    altSetting = intfDesc->bAlternateSetting;
                    gotAlternateSetting = true;
                }
                
		_interfaceList[i] = IOUSBInterface::withDescriptors(confDesc, intfDesc);
                if (_interfaceList[i])
                {
                    USBLog(5,"%s[%p]::SetConfiguration  Attaching an interface (%p) ",getName(), this, _interfaceList[i] );
                    
                    if ( _interfaceList[i]->attach(this) )
                    {
                        _interfaceList[i]->release();
                        if (!_interfaceList[i]->start(this))
                        {
                            USBLog(3,"%s[%p]::SetConfiguration  Could not start IOUSBInterface (%p)",getName(), this, _interfaceList[i] );
                            _interfaceList[i]->detach(this);
                            _interfaceList[i] = NULL;
                        }
                     }
                    else
                    {
                        USBLog(3,"%s[%p]::SetConfiguration  Attaching an interface (%p) failed",getName(), this, _interfaceList[i] );
                        return kIOReturnNoResources;
                    }

                }
                else
                {
                    USBLog(3,"%s[%p]::SetConfiguration  Could not init IOUSBInterface",getName(), this );
                    return kIOReturnNoMemory;
                }

            }
	    else
            {
		USBLog(3,"%s[%p]: SetConfiguration(%d): could not find interface (%d)", getName(), this, configNumber, i);
            }
	}
	if (startMatchingInterfaces)
	{
            retain();	// retain ourselves because registerService could block
	    for (i=0; i<_numInterfaces; i++)
	    {
		if (_interfaceList[i])
                {
                    IOUSBInterface *theInterface = _interfaceList[i];
                    
                    USBLog(5,"%s[%p]::SetConfiguration  matching to interfaces (%d) ",getName(), this, i );
                    
                    // need to do an extra retain in case we get terminated while loading a driver
                    theInterface->retain();
		    theInterface->registerService(kIOServiceSynchronous);
                    theInterface->release();
                }
	    }
            release();
	}
    }
    else
    {
        USBLog(5,"%s[%p]::SetConfiguration  Not matching to interfaces: configNumber (%d) is zero and no errata to allow it (%d)",getName(), this, configNumber, _allowConfigValueOfZero);
    }
    
    USBLog(5,"%s[%p]::SetConfiguration  returning success",getName(), this);
    
    return kIOReturnSuccess;
}



IOReturn 
IOUSBDevice::SetFeature(UInt8 feature)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevRequest	request;

    USBLog(5, "********** SET FEATURE %d **********", feature);

    request.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBStandard, kUSBDevice);
    request.bRequest = kUSBRqSetFeature;
    request.wValue = feature;
    request.wIndex = 0;
    request.wLength = 0;
    request.pData = 0;

    err = DeviceRequest(&request, 5000, 0);

    if (err)
    {
        USBError(1, "%s[%p]: error setting feature. err=0x%x", getName(), this, err);
    }

    return(err);
}


//
// returns a previously instantiated interface which is already attached and which matches 
// the given interface descriptor
//
IOUSBInterface* 
IOUSBDevice::GetInterface(const IOUSBInterfaceDescriptor *intfDesc)
{
    int				i;

    if (!_interfaceList)
        return NULL;
        
    for (i=0; i < _numInterfaces; i++)
    {
        if (!_interfaceList[i])
            continue;
        if ( (_interfaceList[i]->GetInterfaceNumber() == intfDesc->bInterfaceNumber) &&
                (_interfaceList[i]->GetAlternateSetting() == intfDesc->bAlternateSetting) )
            return _interfaceList[i];
    }
    return NULL;
    
}



// Copy data into supplied buffer, up to 'len' bytes.
IOReturn 
IOUSBDevice::GetConfigurationDescriptor(UInt8 configValue, void *data, UInt32 len)
{
    unsigned int toCopy;
    const IOUSBConfigurationDescriptor *cd;
    cd = FindConfig(configValue);
    if(!cd)
        return kIOUSBConfigNotFound;

    toCopy = USBToHostWord(cd->wTotalLength);
    if(len < toCopy)
	toCopy = len;
    bcopy(cd, data, toCopy);
    return kIOReturnSuccess;
}



/**
 ** Matching methods
 **/
bool 
IOUSBDevice::matchPropertyTable(OSDictionary * table, SInt32 *score)
{
    bool 	returnValue = true;
    SInt32	propertyScore = *score;
    OSString	*userClientInitMatchKey;
    char	logString[256]="";
    
    bool	vendorPropertyExists = table->getObject(kUSBVendorID);
    bool	productPropertyExists = table->getObject(kUSBProductID);
    bool	deviceReleasePropertyExists = table->getObject(kUSBDeviceReleaseNumber);
    bool	deviceClassPropertyExists = table->getObject(kUSBDeviceClass);
    bool	deviceSubClassPropertyExists = table->getObject(kUSBDeviceSubClass);
    bool	deviceProtocolPropertyExists= table->getObject(kUSBDeviceProtocol);

    // USBComparePropery() will return false if the property does NOT exist, or if it exists and it doesn't match
    //
    bool	vendorPropertyMatches = USBCompareProperty(table, kUSBVendorID);
    bool	productPropertyMatches = USBCompareProperty(table, kUSBProductID);
    bool	deviceReleasePropertyMatches = USBCompareProperty(table, kUSBDeviceReleaseNumber);
    bool	deviceClassPropertyMatches = USBCompareProperty(table, kUSBDeviceClass);
    bool	deviceSubClassPropertyMatches = USBCompareProperty(table, kUSBDeviceSubClass);
    bool	deviceProtocolPropertyMatches= USBCompareProperty(table, kUSBDeviceProtocol);

    // If the service object wishes to compare some of its properties in its
    // property table against the supplied matching dictionary,
    // it should do so in this method and return truth on success.
    //
    if (!super::matchPropertyTable(table))  
        return false;

    // something of a hack. We need the IOUSBUserClientInit "driver" to match in order
    // for us to be able to find the user mode plugin. However, that driver can't
    // effectively match using the USB Common Class Spec, so we special case it as a
    // short circuit
    userClientInitMatchKey = OSDynamicCast(OSString, table->getObject(kIOMatchCategoryKey));
    if (userClientInitMatchKey && !strcmp(userClientInitMatchKey->getCStringNoCopy(), "IOUSBUserClientInit"))
    {
        *score = 9000;
        return true;
    }
    
    // If the property score is > 10000, then clamp it to 9000.  We will then add this score
    // to the matching criteria score.  This will allow drivers
    // to still override a driver with the same matching criteria.  Since we score the matching
    // criteria in increments of 10,000, clamping it to 9000 guarantees us 1000 to do what we please.
    if ( propertyScore >= 10000 ) 
        propertyScore = 9000;
    
    // Do the Device Matching algorithm
    //
    OSNumber *	deviceClass = (OSNumber *) getProperty(kUSBDeviceClass);

    if ( vendorPropertyMatches && productPropertyMatches && deviceReleasePropertyMatches &&
        (!deviceClassPropertyExists || deviceClassPropertyMatches) && 
        (!deviceSubClassPropertyExists || deviceSubClassPropertyMatches) && 
        (!deviceProtocolPropertyExists || deviceProtocolPropertyMatches) )
    {
        *score = 100000;
    }
    else if ( vendorPropertyMatches && productPropertyMatches  && 
            (!deviceReleasePropertyExists || deviceReleasePropertyMatches) && 
            (!deviceClassPropertyExists || deviceClassPropertyMatches) && 
            (!deviceSubClassPropertyExists || deviceSubClassPropertyMatches) && 
            (!deviceProtocolPropertyExists || deviceProtocolPropertyMatches) )
    {
        *score = 90000;
    }
    else if ( deviceClass && deviceClass->unsigned32BitValue() == kUSBVendorSpecificClass )
    {
        if (  vendorPropertyMatches && deviceSubClassPropertyMatches && deviceProtocolPropertyMatches &&
                (!deviceClassPropertyExists || deviceClassPropertyMatches) && 
                !deviceReleasePropertyExists &&  !productPropertyExists )
        {
            *score = 80000;
        }
        else if ( vendorPropertyMatches && deviceSubClassPropertyMatches &&
                    (!deviceClassPropertyExists || deviceClassPropertyMatches) && 
                    !deviceProtocolPropertyExists && !deviceReleasePropertyExists && 
                    !productPropertyExists )
        {
            *score = 70000;
        }
        else
        {
            *score = 0;
            returnValue = false;
        }
    }
    else if (  deviceClassPropertyMatches && deviceSubClassPropertyMatches && deviceProtocolPropertyMatches &&
                !deviceReleasePropertyExists &&  !vendorPropertyExists && !productPropertyExists )
    {
        *score = 60000;
    }
    else if (  deviceClassPropertyMatches && deviceSubClassPropertyMatches &&
                !deviceProtocolPropertyExists && !deviceReleasePropertyExists &&  !vendorPropertyExists && 
                !productPropertyExists )
    {
        *score = 50000;
    }
    else
    {
        *score = 0;
        returnValue = false;
    }

    // Finally, add in the xml probe score if it's available.
    //
    if ( *score != 0 )
        *score += propertyScore;

    OSString *  identifier = OSDynamicCast(OSString, table->getObject("CFBundleIdentifier"));
    OSNumber *	vendor = (OSNumber *) getProperty(kUSBVendorID);
    OSNumber *	product = (OSNumber *) getProperty(kUSBProductID);
    OSNumber *	release = (OSNumber *) getProperty(kUSBDeviceReleaseNumber);
    OSNumber *	deviceSubClass = (OSNumber *) getProperty(kUSBDeviceSubClass);
    OSNumber *	protocol = (OSNumber *) getProperty(kUSBDeviceProtocol);
    OSNumber *	dictVendor = (OSNumber *) table->getObject(kUSBVendorID);
    OSNumber *	dictProduct = (OSNumber *) table->getObject(kUSBProductID);
    OSNumber *	dictRelease = (OSNumber *) table->getObject(kUSBDeviceReleaseNumber);
    OSNumber *	dictDeviceClass = (OSNumber *) table->getObject(kUSBDeviceClass);
    OSNumber *	dictDeviceSubClass = (OSNumber *) table->getObject(kUSBDeviceSubClass);
    OSNumber *	dictProtocol = (OSNumber *) table->getObject(kUSBDeviceProtocol);
    bool	match = false;

    if (identifier)
        USBLog(5,"Finding device driver for %s, matching personality using %s, score: %ld", getName(), identifier->getCStringNoCopy(), *score);
    else
        USBLog(6,"Finding device driver for %s, matching user client dictionary, score: %ld", getName(), *score);
    
    if ( vendor && product && release && deviceClass && deviceSubClass && protocol )
    {
        char tempString[256]="";
        
        sprintf(logString,"\tMatched: ");
        if ( vendorPropertyMatches ) { match = true; sprintf(tempString,"idVendor (%d) ", vendor->unsigned32BitValue()); strcat(logString, tempString); }
        if ( productPropertyMatches ) { match = true; sprintf(tempString,"idProduct (%d) ", product->unsigned32BitValue());  strcat(logString, tempString);}
        if ( deviceReleasePropertyMatches ) { match = true; sprintf(tempString,"bcdDevice (%d) ", release->unsigned32BitValue());  strcat(logString, tempString);}
        if ( deviceClassPropertyMatches ) { match = true; sprintf(tempString,"bDeviceClass (%d) ", deviceClass->unsigned32BitValue());  strcat(logString, tempString);}
        if ( deviceSubClassPropertyMatches ) { match = true; sprintf(tempString,"bDeviceSubClass (%d) ", deviceSubClass->unsigned32BitValue());  strcat(logString, tempString);}
        if ( deviceProtocolPropertyMatches ) { match = true; sprintf(tempString,"bDeviceProtocol (%d) ", protocol->unsigned32BitValue());  strcat(logString, tempString);}
        if ( !match ) strcat(logString,"no properties");
        
        USBLog(6,logString);
        
        sprintf(logString,"\tDidn't Match: ");
        
        match = false;
        if ( vendorPropertyExists && !vendorPropertyMatches && dictVendor ) 
        { 
            match = true; 
            sprintf(tempString,"idVendor (%d,%d) ", dictVendor->unsigned32BitValue(), vendor->unsigned32BitValue());
            strcat(logString, tempString);
        }
        if ( productPropertyExists && !productPropertyMatches && dictProduct) 
        { 
            match = true; 
            sprintf(tempString,"idProduct (%d,%d) ", dictProduct->unsigned32BitValue(), product->unsigned32BitValue());
            strcat(logString, tempString); 
        }
        if ( deviceReleasePropertyExists && !deviceReleasePropertyMatches && dictRelease) 
        { 
            match = true; 
            sprintf(tempString,"bcdDevice (%d,%d) ", dictRelease->unsigned32BitValue(), release->unsigned32BitValue());
            strcat(logString, tempString); 
        }
        if ( deviceClassPropertyExists && !deviceClassPropertyMatches && dictDeviceClass) 
        { 
            match = true; 
            sprintf(tempString,"bDeviceClass (%d,%d) ", dictDeviceClass->unsigned32BitValue(), deviceClass->unsigned32BitValue());
            strcat(logString, tempString);
        }
        if ( deviceSubClassPropertyExists && !deviceSubClassPropertyMatches && dictDeviceSubClass) 
        { 
            match = true; 
            sprintf(tempString,"bDeviceSubClass (%d,%d) ", dictDeviceSubClass->unsigned32BitValue(), deviceSubClass->unsigned32BitValue());
            strcat(logString, tempString); 
        }
        if ( deviceProtocolPropertyExists && !deviceProtocolPropertyMatches && dictProtocol) 
        { 
            match = true; 
            sprintf(tempString,"bDeviceProtocol (%d,%d) ", dictProtocol->unsigned32BitValue(), protocol->unsigned32BitValue());
            strcat(logString, tempString); 
        }
        if ( !match ) strcat(logString,"nothing");
        
        USBLog(6,logString);
    }
    
    return returnValue;
}



// Lowlevel requests for non-standard device requests
IOReturn 
IOUSBDevice::DeviceRequest(IOUSBDevRequest *request, IOUSBCompletion *completion)
{
    UInt16	theRequest;
    IOReturn	err;
    UInt16	wValue = request->wValue;
    
    if (_deviceterminating)
    {
        USBLog(1, "%s[%p]::DeviceRequest - while terminating!", getName(), this);
        return kIOReturnNotResponding;
    }
    theRequest = (request->bRequest << 8) | request->bmRequestType;

    if (theRequest == kSetAddress) 
    {
        USBLog(3, "%s[%p]:DeviceRequest ignoring kSetAddress", getName(), this);
        return kIOReturnNotPermitted;
    }

    if ( _pipeZero )
    {
        err = _pipeZero->ControlRequest(request, completion);
        if ( err == kIOReturnSuccess)
        {
            if ( theRequest == kSetConfiguration)
            {
                _currentConfigValue = wValue;
            }
        }
        return err;
    }
    else
        return kIOUSBUnknownPipeErr;
}



IOReturn 
IOUSBDevice::DeviceRequest(IOUSBDevRequestDesc *request, IOUSBCompletion *completion)
{
    UInt16	theRequest;
    IOReturn	err;
    UInt16	wValue = request->wValue;
    
    if (_deviceterminating)
    {
        USBLog(1, "%s[%p]::DeviceRequest - while terminating!",getName(),this);
        return kIOReturnNotResponding;
    }
    theRequest = (request->bRequest << 8) | request->bmRequestType;

    if (theRequest == kSetAddress) 
    {
        USBLog(3, "%s[%p]:DeviceRequest ignoring kSetAddress", getName(), this);
        return kIOReturnNotPermitted;
    }

    if ( _pipeZero )
    {
        err = _pipeZero->ControlRequest(request, completion);
        if ( err == kIOReturnSuccess)
        {
            if ( theRequest == kSetConfiguration)
            {
                _currentConfigValue = wValue;
            }
        }
        return err;
    }
    else
        return kIOUSBUnknownPipeErr;
}



IOReturn 
IOUSBDevice::GetConfiguration(UInt8 *configNumber)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevRequest	request;
 
    USBLog(5, "************ GET CONFIGURATION *************");

    request.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice);
    request.bRequest = kUSBRqGetConfig;
    request.wValue = 0;
    request.wIndex = 0;
    request.wLength = sizeof(*configNumber);
    request.pData = configNumber;

    err = DeviceRequest(&request, 5000, 0);

    if (err)
    {
        USBError(1,"%s[%p]: error getting config. err=0x%x", getName(), this, err);
    }

    return(err);
}



IOReturn 
IOUSBDevice::GetDeviceStatus(USBStatus *status)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevRequest	request;

    USBLog(5, "*********** GET DEVICE STATUS ***********");

    request.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice);
    request.bRequest = kUSBRqGetStatus;
    request.wValue = 0;
    request.wIndex = 0;
    request.wLength = sizeof(*status);
    request.pData = status;

    err = DeviceRequest(&request, 5000, 0);

    if (err)
        USBError(1,"%s[%p]: error getting device status. err=0x%x", getName(), this, err);

    return(err);
}



IOReturn 
IOUSBDevice::GetStringDescriptor(UInt8 index, char *buf, int maxLen, UInt16 lang)
{
    IOReturn 		err;
    UInt8 		desc[256]; // Max possible descriptor length
    IOUSBDevRequest	request;
    int			i, len;

    // The buffer needs to be > 1
    //
    if ( maxLen < 2 )
        return kIOReturnBadArgument;
        
    // First get actual length (lame devices don't like being asked for too much data)
    //
    request.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice);
    request.bRequest = kUSBRqGetDescriptor;
    request.wValue = (kUSBStringDesc << 8) | index;
    request.wIndex = lang;
    request.wLength = 2;
    bzero(desc, 2);
    request.pData = &desc;

    err = DeviceRequest(&request, 5000, 0);

    if ( (err != kIOReturnSuccess) && (err != kIOReturnOverrun) )
    {
        USBLog(2,"%s[%p]::GetStringDescriptor reading string length returned error (0x%x)",getName(), this, err);
        return err;
    }

    request.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice);
    request.bRequest = kUSBRqGetDescriptor;
    request.wValue = (kUSBStringDesc << 8) | index;
    request.wIndex = lang;
    len = desc[0];
    
    // If the length is 0 (empty string), just set the buffer to be 0.
    //
    if(len == 0)
    {
        buf[0] = 0;
        return kIOReturnSuccess;
    }
    
    // Make sure that desc[1] == kUSBStringDesc and that the length is even
    //
    if ( desc[1] != kUSBStringDesc || ((desc[0] & 1) != 0) )
    {
        USBLog(2,"%s[%p]::GetStringDescriptor descriptor is not a string (%d ­ kUSBStringDesc) or length (%d) is odd", getName(), this, desc[1], desc[0]);
        return kIOReturnDeviceError;
    }

    request.wLength = len;
    bzero(desc, len);
    request.pData = &desc;

    err = DeviceRequest(&request, 5000, 0);

    if (err != kIOReturnSuccess)
    {
        USBLog(2,"%s[%p]::GetStringDescriptor reading entire string returned error (0x%x)",getName(), this, err);
        return err;
    }

    // If the 2nd byte is a string descriptor and the 1st byte is even, then we're OK
    //
    if ( (desc[1] != kUSBStringDesc) || ( (desc[0] & 1) != 0 ) )
    {
        USBLog(2,"%s[%p]::GetStringDescriptor descriptor is not a string (%d ­ kUSBStringDesc) or length (%d) is odd", getName(), this, desc[1], desc[0]);
        return kIOReturnDeviceError;
    }
    
    USBLog(5, "%s[%p]::GetStringDescriptor Got string descriptor %d, length %d, got %d", getName(), this,
             index, desc[0], request.wLength);

    // The following code tries to translate the Unicode string to ascii, but it does a 
    // poor job (in that it does not deal with characters that have the high byte set.
    //
    if ( desc[0] > 1 )
    {
        len = (desc[0]-2)/2;
        if(len > maxLen-1)
            len = maxLen-1;
        for(i=0; i<len; i++)
            buf[i] = desc[2*i+2];
        buf[len] = 0;
    }
    else
        buf[0] = 0;
        
    return kIOReturnSuccess;
}

IOReturn 
IOUSBDevice::message( UInt32 type, IOService * provider,  void * argument )
{
    IOReturn 			err = kIOReturnSuccess;
    IOUSBRootHubDevice * 	rootHub = NULL;
    OSIterator *		iter;
    IOService *			client;
    UInt32			retries = 100;
    IOReturn			resetErr = kIOReturnSuccess;
    
    switch ( type )
    {
        case kIOUSBMessagePortHasBeenReset:
            resetErr = * (IOReturn *) argument;

            USBLog(4,"%s[%p] received kIOUSBMessagePortHasBeenReset with error = 0x%x",getName(), this, resetErr);
            
            // When we get this message, we need to forward it to our clients
            //
            // Make sure that _pipeZero is not NULL before dispatching this call
            //
            while ( _pipeZero == NULL && retries > 0)
            {
                retries--;
                IOSleep(50);
            }
            
            // We should do something if _pipeZero does not exist
            //
            if ( _pipeZero && (resetErr == kIOReturnSuccess) )
            {
                USBLog(3, "%s[%p] calling messageClients (kIOUSBMessagePortHasBeenReset (%d))", getName(), this, _portNumber );
                (void) messageClients(kIOUSBMessagePortHasBeenReset, &_portNumber, sizeof(_portNumber));
            }
            
            _portHasBeenReset = true;
            
            break;
            
        case kIOUSBMessageHubIsDeviceConnected:
        
            // We need to send a message to all our clients (we are looking for the Hub driver) asking
            // them if the device for the given port is connected.  The hub driver will return the kIOReturnNoDevice
            // as an error.  However, any client that does not implement the message method will return
            // kIOReturnUnsupported, so we have to treat that as not an error
            //
            USBLog(5, "%s at %d: Hub device name is %s at %d", getName(), _address, _usbPlaneParent->getName(), _usbPlaneParent->GetAddress());
            rootHub = OSDynamicCast(IOUSBRootHubDevice, _usbPlaneParent);
            if ( !rootHub )
            {
                // Check to see if our parent is still connected. A kIOReturnSuccess means that
                // our parent is still connected to its parent.
                //
                err = _usbPlaneParent->message(kIOUSBMessageHubIsDeviceConnected, NULL, 0);
            }

           
            if ( err == kIOReturnSuccess )
            {
                iter = _usbPlaneParent->getClientIterator();
                while( (client = (IOService *) iter->getNextObject())) 
                {
                    IOSleep(1);
                    
                    err = client->message(kIOUSBMessageHubIsDeviceConnected, this, &_portNumber);
    
                    // If we get a kIOReturnUnsupported error, treat it as no error
                    //
                    if ( err == kIOReturnUnsupported )
                        err = kIOReturnSuccess;
                    else
                    
                    if ( err != kIOReturnSuccess )
                        break;
                }
                iter->release();
            }
           break;
            
        case kIOUSBMessagePortHasBeenResumed:
            // Forward the message to our clients
            //
            messageClients( kIOUSBMessagePortHasBeenResumed, this, _portNumber);
            break;
            
        case kIOMessageServiceIsTerminated:
            USBLog(5,"%s[%p]: kIOMessageServiceIsTerminated",getName(),this);
            _deviceterminating = true;
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

void 
IOUSBDevice::stop( IOService * provider )
{
    USBLog(5, "%s[%p]::stop isInactive = %d", getName(), this, isInactive());
    super::stop(provider);
}



bool
IOUSBDevice::willTerminate( IOService * provider, IOOptionBits options )
{
    // this method is intended to be used to stop any pending I/O and to make sure that 
    // we have begun getting our callbacks in order. by the time we get here, the 
    // isInactive flag is set, so we really are marked as being done. we will do in here
    // what we used to do in the message method (this happens first)
    USBLog(3, "%s[%p]::willTerminate isInactive = %d", getName(), this, isInactive());

    return super::willTerminate(provider, options);
}


bool
IOUSBDevice::didTerminate( IOService * provider, IOOptionBits options, bool * defer )
{
    // this method comes at the end of the termination sequence. Hopefully, all of our outstanding IO is complete
    // in which case we can just close our provider and IOKit will take care of the rest. Otherwise, we need to 
    // hold on to the device and IOKit will terminate us when we close it later
    USBLog(3, "%s[%p]::didTerminate isInactive = %d", getName(), this, isInactive());

    return super::didTerminate(provider, options, defer);
}



void
IOUSBDevice::DisplayNotEnoughPowerNotice()
{
    KUNCUserNotificationDisplayNotice(
	0,		// Timeout in seconds
	0,		// Flags (for later usage)
	"",		// iconPath (not supported yet)
	"",		// soundPath (not supported yet)
	"/System/Library/Extensions/IOUSBFamily.kext",		// localizationPath
	"Low Power Header",		// the header
	"Low Power Notice",		// the notice
	"OK"); 
    return;
}


OSMetaClassDefineReservedUsed(IOUSBDevice,  0);
IOReturn 
IOUSBDevice::DeviceRequest(IOUSBDevRequest *request, UInt32 noDataTimeout, UInt32 completionTimeout, IOUSBCompletion *completion)
{
    UInt16	theRequest;
    IOReturn	err;
    UInt16	wValue = request->wValue;
    
    if (_deviceterminating)
    {
        USBLog(1, "%s[%p]::DeviceRequest - while terminating!", getName(), this);
        return kIOReturnNotResponding;
    }

    theRequest = (request->bRequest << 8) | request->bmRequestType;

    if (theRequest == kSetAddress) 
    {
        USBLog(3, "%s[%p]:DeviceRequest ignoring kSetAddress", getName(), this);
        return kIOReturnNotPermitted;
    }

    if ( _pipeZero )
    {
        err = _pipeZero->ControlRequest(request, noDataTimeout, completionTimeout, completion);
        if ( err == kIOReturnSuccess)
        {
            if ( theRequest == kSetConfiguration)
            {
                _currentConfigValue = wValue;
            }
        }
        return err;
    }
    else
        return kIOUSBUnknownPipeErr;
}



OSMetaClassDefineReservedUsed(IOUSBDevice,  1);
IOReturn 
IOUSBDevice::DeviceRequest(IOUSBDevRequestDesc *request, UInt32 noDataTimeout, UInt32 completionTimeout, IOUSBCompletion *completion)
{
    UInt16	theRequest;
    IOReturn	err;
    UInt16	wValue = request->wValue;
    
    if (_deviceterminating)
    {
        USBLog(1, "%s[%p]::DeviceRequest - while terminating!", getName(), this);
        return kIOReturnNotResponding;
    }

    theRequest = (request->bRequest << 8) | request->bmRequestType;

    if (theRequest == kSetAddress) 
    {
        USBLog(3, "%s[%p]:DeviceRequest ignoring kSetAddress", getName(), this);
        return kIOReturnNotPermitted;
    }

    if ( _pipeZero )
    {
        err = _pipeZero->ControlRequest(request, noDataTimeout, completionTimeout, completion);
        if ( err == kIOReturnSuccess)
        {
            if ( theRequest == kSetConfiguration)
            {
                _currentConfigValue = wValue;
            }
        }
        return err;
    }
    else
        return kIOUSBUnknownPipeErr;
}



OSMetaClassDefineReservedUsed(IOUSBDevice,  2);
IOReturn
IOUSBDevice::SuspendDevice( bool suspend )
{

    if (_deviceterminating)
    {
        USBLog(1, "%s[%p]::SuspendDevice - while terminating!", getName(),  this);
        return kIOReturnNotResponding;
    }

    retain();
    
    thread_call_enter1( _doPortSuspendThread, (thread_call_param_t) suspend );

    return kIOReturnSuccess;
}

OSMetaClassDefineReservedUsed(IOUSBDevice,  3);
IOReturn
IOUSBDevice::ReEnumerateDevice( UInt32 options )
{
    if (_deviceterminating)
    {
        USBLog(1, "%s[%p]::ReEnumerateDevice - while terminating!", getName(), this);
        return kIOReturnNotResponding;
    }
    
    // Since we are going to re-enumerate the device, all drivers and interfaces will be
    // terminated, so we don't need to make this device synchronous.  In fact, we want it
    // async, because this device will go away.
    //
    USBLog(3, "+%s[%p] ReEnumerateDevice for port %d", getName(), this, _portNumber );
    retain();
    thread_call_enter1( _doPortReEnumerateThread, (thread_call_param_t) options );

    USBLog(3, "-%s[%p] ReEnumerateDevice for port %d", getName(), this, _portNumber );
    
    return kIOReturnSuccess;
}

OSMetaClassDefineReservedUnused(IOUSBDevice,  4);
OSMetaClassDefineReservedUnused(IOUSBDevice,  5);
OSMetaClassDefineReservedUnused(IOUSBDevice,  6);
OSMetaClassDefineReservedUnused(IOUSBDevice,  7);
OSMetaClassDefineReservedUnused(IOUSBDevice,  8);
OSMetaClassDefineReservedUnused(IOUSBDevice,  9);
OSMetaClassDefineReservedUnused(IOUSBDevice,  10);
OSMetaClassDefineReservedUnused(IOUSBDevice,  11);
OSMetaClassDefineReservedUnused(IOUSBDevice,  12);
OSMetaClassDefineReservedUnused(IOUSBDevice,  13);
OSMetaClassDefineReservedUnused(IOUSBDevice,  14);
OSMetaClassDefineReservedUnused(IOUSBDevice,  15);
OSMetaClassDefineReservedUnused(IOUSBDevice,  16);
OSMetaClassDefineReservedUnused(IOUSBDevice,  17);
OSMetaClassDefineReservedUnused(IOUSBDevice,  18);
OSMetaClassDefineReservedUnused(IOUSBDevice,  19);

//=============================================================================================
//
//  ProcessPortReset
//
//  This routine will tear stop all activity on the default pipe, tear down all the interfaces,
//  (causing their drivers to get unloaded), issue a reset port request to the hub driver, and
//  will then re-create the default pipe.  The hub driver is responsible for sending a message
//  to the appropriate device telling it that it has been reset. 
//
//=============================================================================================
//
void 
IOUSBDevice::ProcessPortResetEntry(OSObject *target)
{
    IOUSBDevice *	me = OSDynamicCast(IOUSBDevice, target);
    
    if (!me)
        return;
        
    me->ProcessPortReset();
    me->release();
}



void 
IOUSBDevice::ProcessPortReset()
{
    IOReturn			err = kIOReturnSuccess;
   
    USBLog(5,"+%s[%p]::ProcessPortReset",getName(),this); 
    _portResetThreadActive = true;
    
    // USBLog(3, "+%s[%p]::ProcessPortReset begin", getName(), this);
    if( _pipeZero) 
    {
        _pipeZero->Abort();
        _pipeZero->release();
        _pipeZero = NULL;
    }
   
    // In the past, we "terminated" all the interfaces at this point.  As of 9/17/01, we will not do 
    // that anymore.  10/30/01:  We will do it in the case that we want to reenumerate devices:
    //
    
    // Now, we need to tell our parent to issue a port reset to our port.  Our parent is an IOUSBDevice
    // that has a hub driver attached to it.  However, we don't know what kind of driver that is, so we
    // just send a message to all the clients of our parent.  The hub driver will be the only one that
    // should do anything with that message.
    //
    if ( _usbPlaneParent )
    {
        USBLog(3, "%s[%p] calling messageClients (kIOUSBMessageHubResetPort)", getName(), this);
        err = _usbPlaneParent->messageClients(kIOUSBMessageHubResetPort, &_portNumber, sizeof(_portNumber));
    }
    
   // Recreate pipe 0 object
    _pipeZero = IOUSBPipe::ToEndpoint(&_endpointZero, _speed, _address, _controller);
    if (!_pipeZero)
    {
        USBError(1,"%s[%p]::ProcessPortReset DANGER could not recreate pipe Zero after reset");
        err = kIOReturnNoMemory;
    }

    _currentConfigValue = 0;
    // USBLog(3, "-%s[%p]::ProcessPortReset", getName(), this);
   
     _portResetThreadActive = false;
    USBLog(5,"-%s[%p]::ProcessPortReset",getName(),this); 
}

//=============================================================================================
//
//  ProcessPortReEnumerateEntry
//
//  This routine will stop all activity on the default pipe, tear down all the interfaces,
//  (causing their drivers to get unloaded), issue a reenumerate port request to the hub driver.
//  The Hub driver will terminate the device and then add it again, generating a reset pulse
//  during this new enumeration.
//
//=============================================================================================
//
void 
IOUSBDevice::ProcessPortReEnumerateEntry(OSObject *target,thread_call_param_t options)
{
    IOUSBDevice *	me = OSDynamicCast(IOUSBDevice, target);
    
    if (!me)
        return;
        
    me->ProcessPortReEnumerate( (UInt32) options);
    me->release();
}



void 
IOUSBDevice::ProcessPortReEnumerate(UInt32 options)
{
    IOReturn				err = kIOReturnSuccess;
    IOUSBHubPortReEnumerateParam	params;
    
    USBLog(5,"+%s[%p]::ProcessPortReEnumerate",getName(),this); 

    // Set the parameters for our message call
    //
    params.portNumber = _portNumber;
    params.options = options;
    
    if( _pipeZero) 
    {
        _pipeZero->Abort();
        _pipeZero->release();
        _pipeZero = NULL;
    }
   
    // Remove any interfaces and their drivers
    //
    TerminateInterfaces();
    
    // Now, we need to tell our parent to issue a port reenumerate to our port.  Our parent is an IOUSBDevice
    // that has a hub driver attached to it.  However, we don't know what kind of driver that is, so we
    // just send a message to all the clients of our parent.  The hub driver will be the only one that
    // should do anything with that message.
    //
    if ( _usbPlaneParent )
    {
        USBLog(3, "%s[%p] calling messageClients (kIOUSBMessageHubReEnumeratePort)", getName(), this);
        err = _usbPlaneParent->messageClients(kIOUSBMessageHubReEnumeratePort, &params, sizeof(IOUSBHubPortReEnumerateParam));
    }
    
    USBLog(5,"-%s[%p]::ProcessPortReEnumerate",getName(),this); 
}

//=============================================================================================
//
//  ProcessPortSuspend/Resume
//
//
//=============================================================================================
//
void 
IOUSBDevice::ProcessPortSuspendEntry(OSObject *target, thread_call_param_t suspend)
{
    IOUSBDevice *	me = OSDynamicCast(IOUSBDevice, target);
    
    if (!me)
        return;
        
    me->ProcessPortSuspend( (bool) suspend );
    me->release();
}



void 
IOUSBDevice::ProcessPortSuspend(bool suspend)
{
    IOReturn			err = kIOReturnSuccess;

    _portSuspendThreadActive = true;
    
    // Now, we need to tell our parent to issue a port suspend to our port.  Our parent is an IOUSBDevice
    // that has a hub driver attached to it.  However, we don't know what kind of driver that is, so we
    // just send a message to all the clients of our parent.  The hub driver will be the only one that
    // should do anything with that message.
    //
    if ( _usbPlaneParent )
    {
        if ( suspend )
            err = _usbPlaneParent->messageClients(kIOUSBMessageHubSuspendPort, &_portNumber, sizeof(_portNumber));
        else
            err = _usbPlaneParent->messageClients(kIOUSBMessageHubResumePort, &_portNumber, sizeof(_portNumber));
        
    }
   
     _portSuspendThreadActive = false;
}

// Obsolete, do NOT use
//
void 
IOUSBDevice::SetPort(void *port) 
{ 
    _port = port;
}

USBDeviceAddress 
IOUSBDevice::GetAddress(void) 
{ 
    return _address; 
}

UInt8 
IOUSBDevice::GetSpeed(void) 
{ 
    return _speed; 
}

IOUSBController *
IOUSBDevice::GetBus(void) 
{ 
    return _controller; 
}

UInt32 
IOUSBDevice::GetBusPowerAvailable( void ) 
{ 
    return _busPowerAvailable; 
}

UInt8 
IOUSBDevice::GetMaxPacketSize(void) 
{ 
    return _descriptor.bMaxPacketSize0; 
}
	
UInt16 
IOUSBDevice::GetVendorID(void) 
{ 
    return USBToHostWord(_descriptor.idVendor); 
}

UInt16 
IOUSBDevice::GetProductID(void) 
{ 
    return USBToHostWord(_descriptor.idProduct);
}

UInt16  
IOUSBDevice::GetDeviceRelease(void)
{ 
    return USBToHostWord(_descriptor.bcdDevice); 
}

UInt8  
IOUSBDevice::GetNumConfigurations(void)
{ 
    return _descriptor.bNumConfigurations; 
}

UInt8  
IOUSBDevice::GetManufacturerStringIndex(void ) 
{ 
    return _descriptor.iManufacturer; 
}

UInt8  
IOUSBDevice::GetProductStringIndex(void )
{ 
    return _descriptor.iProduct; 
}

UInt8  
IOUSBDevice::GetSerialNumberStringIndex(void ) 
{ 
    return _descriptor.iSerialNumber; 
}

IOUSBPipe *  
IOUSBDevice::GetPipeZero(void) 
{ 
    return _pipeZero; 
}

IOUSBPipe * 
IOUSBDevice::MakePipe(const IOUSBEndpointDescriptor *ep) 
{
    return IOUSBPipe::ToEndpoint(ep, _speed, _address, _controller); 
}


