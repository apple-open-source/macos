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
extern "C" {
#include <kern/thread_call.h>
}
#include <libkern/OSByteOrder.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSData.h>
#include <mach/mach_time.h>

#include <IOKit/IOKitKeys.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/usb/IOUSBControllerV3.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBLog.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/IOUSBRootHubDevice.h>
#include <IOKit/usb/IOUSBHubPolicyMaker.h>
#include <IOKit/usb/USB.h>

#include <UserNotification/KUNCUserNotifications.h>
#include "USBTracepoints.h"

//================================================================================================
//
//   Local Definitions
//
//================================================================================================
//
#define super IOUSBNub

#define _PORT_NUMBER					_expansionData->_portNumber
#define _DO_PORT_RESET_THREAD			_expansionData->_doPortResetThread
#define _USBPLANE_PARENT				_expansionData->_usbPlaneParent
#define _PORT_RESET_THREAD_ACTIVE		_expansionData->_portResetThreadActive
#define _ALLOW_CONFIGVALUE_OF_ZERO		_expansionData->_allowConfigValueOfZero
#define _DO_PORT_REENUMERATE_THREAD		_expansionData->_doPortReEnumerateThread
#define _RESET_IN_PROGRESS				_expansionData->_resetInProgress
#define _PORT_HAS_BEEN_RESET			_expansionData->_portHasBeenReset
#define _WORKLOOP						_expansionData->_workLoop
#define _NOTIFIERHANDLER_TIMER			_expansionData->_notifierHandlerTimer
#define _NOTIFICATION_TYPE				_expansionData->_notificationType
#define _SUSPEND_IN_PROGRESS			_expansionData->_suspendInProgress
#define _PORT_HAS_BEEN_SUSPENDED_OR_RESUMED		_expansionData->_portHasBeenSuspendedOrResumed
#define _ADD_EXTRA_RESET_TIME			_expansionData->_addExtraResetTime
#define _COMMAND_GATE					_expansionData->_commandGate
#define _OPEN_INTERFACES				_expansionData->_openInterfaces
#define _RESET_COMMAND					_expansionData->_resetCommand
#define _RESET_ERROR					_expansionData->_resetError
#define _SUSPEND_ERROR					_expansionData->_suspendError
#define _DO_MESSAGE_CLIENTS_THREAD		_expansionData->_doMessageClientsThread
#define _HUBPARENT						_expansionData->_hubPolicyMaker
#define _SLEEPPOWERALLOCATED			_expansionData->_sleepPowerAllocated
#define _WAKEPOWERALLOCATED				_expansionData->_wakePowerAllocated
#define _DEVICEPORTINTO					_expansionData->_devicePortInfo
#define _DEVICEISINTERNAL				_expansionData->_deviceIsInternal
#define _DEVICEISINTERNALISVALID		_expansionData->_deviceIsInternalIsValid
#define _GETCONFIGLOCK					_expansionData->_newGetConfigLock
#define _RESET_AND_REENUMERATE_LOCK		_expansionData->_resetAndReEnumerateLock
#define _LOCATIONID						_expansionData->_locationID


#define kNotifyTimerDelay			30000	// in milliseconds = 30 seconds
#define kUserLoginDelay				20000	// in milliseconds = 20 seconds
#define kMaxTimeToWaitForReset		20000   // in milliseconds = 10 seconds
#define kMaxTimeToWaitForSuspend	20000   // in milliseconds = 20 seconds
#define kGetConfigDeadlineInSecs	30

typedef struct IOUSBDeviceMessage {
    UInt32			type;
    IOReturn		error;
} IOUSBDeviceMessage;


/* Convert USBLog to use kprintf debugging */
#ifndef IOUSBDEVICE_USE_KPRINTF
	#define IOUSBDEVICE_USE_KPRINTF 0
#endif

#if IOUSBDEVICE_USE_KPRINTF
#undef USBLog
#undef USBError
void kprintf(const char *format, ...)
__attribute__((format(printf, 1, 2)));
#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= IOUSBDEVICE_USE_KPRINTF) { kprintf( FORMAT "\n", ## ARGS ) ; }
#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif

//================================================================================================
//
//   External Definitions
//
//================================================================================================
//
extern KernelDebugLevel	    gKernelDebugLevel;

//================================================================================================
//
//   Private IOUSBInterfaceIterator Class Definition
//
//================================================================================================
//
class IOUSBInterfaceIterator : public OSIterator 
{
    OSDeclareDefaultStructors(IOUSBInterfaceIterator)
	
protected:
    IOUSBFindInterfaceRequest 	fRequest;
    IOUSBDevice *		fDevice;
    IOUSBInterface *		fCurrent;
	
    virtual void free();
	
public:
	virtual bool		init(IOUSBDevice *dev, IOUSBFindInterfaceRequest *reqIn);
    virtual void		reset();
    virtual bool		isValid();
    virtual OSObject	*getNextObject();
};

//================================================================================================
//
//   IOUSBInterfaceIterator Methods
//
//================================================================================================
//
OSDefineMetaClassAndStructors(IOUSBInterfaceIterator, OSIterator)

#pragma mark ееееееее IOUSBInterfaceIterator Methods еееееееее
bool 
IOUSBInterfaceIterator::init(IOUSBDevice *dev, IOUSBFindInterfaceRequest *reqIn)
{
    if (!OSIterator::init())
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
    if (fCurrent)
		fCurrent->release();
	
    fDevice->release();
    OSIterator::free();
}



void 
IOUSBInterfaceIterator::reset()
{
    if (fCurrent)
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
    if (fCurrent)
        fCurrent->release();
    fCurrent = next;
    return next;
}

//================================================================================================
//
//   IOUSBDevice Methods
//
//================================================================================================
//
OSDefineMetaClassAndStructors(IOUSBDevice, IOUSBNub)


#pragma mark ееееееее IOService Methods еееееееее
//================================================================================================
//
//   IOKit init
//
//================================================================================================
//
bool
IOUSBDevice::init()
{
	return super::init();
}



bool 
IOUSBDevice::start( IOService * provider )
{
    IOReturn 		err;
    char			name[256];
    UInt32			delay = 30;
    UInt32			retries = 4;
    bool			allowNumConfigsOfZero = false;
    UInt64			elapsedTime;
	uint64_t		currentTime;
    OSObject *		propertyObj = NULL;
	OSBoolean *		boolObj = NULL;
	
    if ( !super::start(provider))
        return false;
	
	_controller = OSDynamicCast(IOUSBController, provider);
	
    if (_controller == NULL)
        return false;
	
	_WORKLOOP = getWorkLoop();
	if ( !_WORKLOOP )
    {
        USBError(1, "%s[%p]::start Couldn't get provider's workloop", getName(), this);
        goto ErrorExit;
    }
	
    // Keep a reference to our workloop
    //
    _WORKLOOP->retain();
	
	_COMMAND_GATE = IOCommandGate::commandGate(this, NULL);
	if (!_COMMAND_GATE)
	{
		USBError(1,"%s[%p]::start - unable to create command gate", getName(), this);
        goto ErrorExit;
	}
	
	if (_WORKLOOP->addEventSource(_COMMAND_GATE) != kIOReturnSuccess)
	{
		USBError(1,"%s[%p]::start - unable to add command gate", getName(), this);
        goto ErrorExit;
	}

	// Don't do this until we have a controller
    //
    _endpointZero.bLength = sizeof(_endpointZero);
    _endpointZero.bDescriptorType = kUSBEndpointDesc;
    _endpointZero.bEndpointAddress = 0;
    _endpointZero.bmAttributes = kUSBControl;
    _endpointZero.wMaxPacketSize = HostToUSBWord(_descriptor.bMaxPacketSize0);
    _endpointZero.bInterval = 0;
	
    _pipeZero = IOUSBPipe::ToEndpoint(&_endpointZero, this, _controller, NULL);
	
    // See if we have the allowNumConfigsOfZero errata. Some devices have an incorrect device descriptor
    // that specifies a bNumConfigurations value of 0.  We allow those devices to enumerate if we know
    // about them.
    //
	propertyObj = copyProperty(kAllowNumConfigsOfZero);
    boolObj = OSDynamicCast( OSBoolean, propertyObj );
    if ( boolObj && boolObj->isTrue() )
        allowNumConfigsOfZero = true;
	
	if (propertyObj)
		propertyObj->release();
	
    // Attempt to read the full device descriptor.  We will attempt 5 times with a 30ms delay between each (that's
    // what we do on MacOS 9
    //
    do
    {
        bzero(&_descriptor,sizeof(_descriptor));
		
        err = GetDeviceDescriptor(&_descriptor, sizeof(_descriptor));
		
        // If the error is kIOReturnOverrun, we still received our 8 bytes, so signal no error.
        //
        if ( err == kIOReturnOverrun )
        {
            if ( _descriptor.bDescriptorType == kUSBDeviceDesc )
            {
                err = kIOReturnSuccess;
            }
            else
            {
                USBLog(3,"%s[%p]::start GetDeviceDescriptor returned overrun and is not a device desc (%d н kUSBDeviceDesc)", getName(), this, _descriptor.bDescriptorType );
            }
        }
		
        if ( (err != kIOReturnSuccess) || (_descriptor.bcdUSB == 0) )
        {
            // The check for bcdUSB is a workaround for some devices that send the data with an incorrect
            // toggle and so we miss the first 16 bytes.  Since we have bzero'd the structure above, we
            // can check for bcdUSB being zero to know whether we had this occurrence or not.  We do the
            // workaround because this is a critical section -- if we don't get it right, then the device
            // will not correctly enumerate.
            //
            USBLog(3, "IOUSBDevice::start, GetDeviceDescriptor -- retrying. err = 0x%x", err);
			
            if ( err == kIOReturnSuccess)
                err = kIOUSBPipeStalled;
            
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
            USBLog(6,"\tbcdUSB %d (0x%04x)", USBToHostWord(_descriptor.bcdUSB), USBToHostWord(_descriptor.bcdUSB));
            USBLog(6,"\tbDeviceClass %d", _descriptor.bDeviceClass);
            USBLog(6,"\tbDeviceSubClass %d", _descriptor.bDeviceSubClass);
            USBLog(6,"\tbDeviceProtocol %d", _descriptor.bDeviceProtocol);
            USBLog(6,"\tbMaxPacketSize0 %d", _descriptor.bMaxPacketSize0);
            USBLog(6,"\tidVendor %d (0x%04x)", USBToHostWord(_descriptor.idVendor), USBToHostWord(_descriptor.idVendor));
            USBLog(6,"\tidProduct %d (0x%04x)", USBToHostWord(_descriptor.idProduct),USBToHostWord(_descriptor.idProduct));
            USBLog(6,"\tbcdDevice %d (0x%04x)", USBToHostWord(_descriptor.bcdDevice),USBToHostWord(_descriptor.bcdDevice));
            USBLog(6,"\tiManufacturer %d ", _descriptor.iManufacturer);
            USBLog(6,"\tiProduct %d ", _descriptor.iProduct);
            USBLog(6,"\tiSerialNumber %d", _descriptor.iSerialNumber);
            USBLog(6,"\tbNumConfigurations %d", _descriptor.bNumConfigurations);
        }
    }
    while ( err && retries > 0 );
	
    if ( err )
    {
        USBLog(3,"IOUSBDevice(%s)[%p]::start Couldn't get full device descriptor (0x%x)",getName(),this, err);
		goto ErrorExit;
    }
	
    if (_descriptor.bNumConfigurations || allowNumConfigsOfZero)
    {
        _configList = IONew(IOBufferMemoryDescriptor*, _descriptor.bNumConfigurations);
        if (!_configList)
			goto ErrorExit;
        bzero(_configList, sizeof(IOBufferMemoryDescriptor*) * _descriptor.bNumConfigurations);
    }
    else
    {
        // The device specified bNumConfigurations of 0, which is not legal (See Section 9.6.2 of USB 1.1).
        // However, we will not flag this as an error.
        //
        USBLog(3,"%s[%p]::start USB Device specified bNumConfigurations of 0, which is not legal", getName(), this);
    }
	
	if  ((_descriptor.idVendor == 0x13FE) &&  ((_descriptor.idProduct == 0x1E00) ||(_descriptor.idProduct == 0x1F00)) )
	{
		//  Workaround for a faulty device that needs to be enumerated (rdar://5877895&6282251)
		setName("USB HD");
		setProperty(kUSBProductString, "USB HD");
	}
	else 
	{	
		if (_descriptor.iProduct)
		{
			err = GetStringDescriptor(_descriptor.iProduct, name, sizeof(name));
			if (err == kIOReturnSuccess)
			{
				if ( name[0] != 0 )
				{
					setName(name);
					setProperty(kUSBProductString, name);
				}
			}
		}
		else
		{
			switch ( _descriptor.bDeviceClass )
			{
				case (kUSBCompositeClass):				setName("IOUSBCompositeDevice");			break;
				case (kUSBAudioClass):					setName("IOUSBAudioDevice");				break;
				case (kUSBCommClass):					setName("IOUSBCommunicationsDevice");		break;
				case (kUSBHubClass):					setName("IOUSBHubDevice");					break;
				case (kUSBDataClass):					setName("IOUSBDataDevice");					break;
				case (kUSBDiagnosticClass):				setName("IOUSBDiagnosticDevice");			break;
				case (kUSBWirelessControllerClass):		setName("IOUSBWirelessControllerDevice");	break;
				case (kUSBMiscellaneousClass):			setName("IOUSBMiscellaneousDevice");		break;
				case (kUSBApplicationSpecificClass):	setName("IOUSBApplicationSpecific");		break;
				case (kUSBVendorSpecificClass):			setName("IOUSBVendorSpecificDevice");		break;
			}
		}
		
		if (_descriptor.iManufacturer)
		{
			err = GetStringDescriptor(_descriptor.iManufacturer, name, sizeof(name));
			if (err == kIOReturnSuccess)
			{
				setProperty(kUSBVendorString, name);
			}
		}
		if (_descriptor.iSerialNumber)
		{
			err = GetStringDescriptor(_descriptor.iSerialNumber, name, sizeof(name));
			if (err == kIOReturnSuccess)
			{
				setProperty(kUSBSerialNumberString, name);
			}
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
	currentTime = mach_absolute_time();
    absolutetime_to_nanoseconds(*(AbsoluteTime *)&currentTime, &elapsedTime);
    setProperty("sessionID", elapsedTime, 64);
	
    // allocate a thread_call structure for reset and suspend
    //
    _DO_PORT_RESET_THREAD = thread_call_allocate((thread_call_func_t)ProcessPortResetEntry, (thread_call_param_t)this);
    _DO_PORT_REENUMERATE_THREAD = thread_call_allocate((thread_call_func_t)ProcessPortReEnumerateEntry, (thread_call_param_t)this);
	_DO_MESSAGE_CLIENTS_THREAD = thread_call_allocate((thread_call_func_t)DoMessageClientsEntry, (thread_call_param_t)this);
	
	if ( !_DO_PORT_RESET_THREAD || !_DO_PORT_REENUMERATE_THREAD || !_DO_MESSAGE_CLIENTS_THREAD )
	{
		USBError(1, "%s[%p] could not allocate all thread functions.  Aborting start", getName(), this);
		goto ErrorExit;
	}
	
    // for now, we have no interfaces, so make sure the list is NULL and zero out other counts
    _interfaceList = NULL;
    _currentConfigValue	= 0;		// unconfigured device
    _numInterfaces = 0;			// so no active interfaces
	
    _NOTIFIERHANDLER_TIMER = IOTimerEventSource::timerEventSource(this, (IOTimerEventSource::Action) DisplayUserNotificationForDeviceEntry);
	
    if ( _NOTIFIERHANDLER_TIMER == NULL )
    {
        USBError(1, "%s[%p]::start Couldn't allocate timer event source", getName(), this);
        goto ErrorExit;
    }
		
    if ( _WORKLOOP->addEventSource( _NOTIFIERHANDLER_TIMER ) != kIOReturnSuccess )
    {
        USBError(1, "%s[%p]::start Couldn't add timer event source", getName(), this);
        goto ErrorExit;
    }
	
    return true;
	
ErrorExit:
	
    if (_pipeZero) 
    {
        _pipeZero->Abort();
		_pipeZero->ClosePipe();
        _pipeZero->release();
        _pipeZero = NULL;
    }
	
	if ( _NOTIFIERHANDLER_TIMER )
	{
		if ( _WORKLOOP )
			_WORKLOOP->removeEventSource(_NOTIFIERHANDLER_TIMER);
		_NOTIFIERHANDLER_TIMER->release();
		_NOTIFIERHANDLER_TIMER = NULL;
	}
	
	if ( _COMMAND_GATE )
	{
        if ( _WORKLOOP )
			_WORKLOOP->removeEventSource(_COMMAND_GATE);
		_COMMAND_GATE->release();
		_COMMAND_GATE = NULL;
	}
	
    if ( _WORKLOOP != NULL )
    {
        _WORKLOOP->release();
        _WORKLOOP = NULL;
    }
	
    if (_DO_PORT_RESET_THREAD)
    {
        thread_call_cancel(_DO_PORT_RESET_THREAD);
        thread_call_free(_DO_PORT_RESET_THREAD);
    }
	
    if (_DO_PORT_REENUMERATE_THREAD)
    {
        thread_call_cancel(_DO_PORT_REENUMERATE_THREAD);
        thread_call_free(_DO_PORT_REENUMERATE_THREAD);
    }
	
    if (_DO_MESSAGE_CLIENTS_THREAD)
    {
        thread_call_cancel(_DO_MESSAGE_CLIENTS_THREAD);
        thread_call_free(_DO_MESSAGE_CLIENTS_THREAD);
    }
	
    return false;
}



bool
IOUSBDevice::handleIsOpen(const IOService *forClient) const
{
	bool	result = false;
	
	if (forClient == NULL)
	{
		if (_OPEN_INTERFACES && (_OPEN_INTERFACES->getCount() > 0))
			result = true;
	}
	else if (OSDynamicCast(IOUSBInterface, forClient))
	{
		if (_OPEN_INTERFACES)
		{
			if (_OPEN_INTERFACES->containsObject(forClient))
			{
				USBLog(6, "%s[%p]::handleIsOpen - IOUSBInterface[%p] has us open", getName(), this, forClient);
				result = true;
			}
			else
			{
				USBLog(2, "%s[%p]::handleIsOpen - IOUSBInterface[%p] is not in _OPEN_INTERFACES", getName(), this, forClient);
			}
		}
		else
		{
			USBLog(2, "%s[%p]::handleIsOpen - no _OPEN_INTERFACES", getName(), this);
		}
	}
	
	if (!result)
		result = super::handleIsOpen(forClient);
	
	return result;
}



bool	
IOUSBDevice::handleOpen(IOService *forClient, IOOptionBits options, void *arg)
{
	bool result = false;
	
	if (OSDynamicCast(IOUSBInterface, forClient))
	{
		if (_OPEN_INTERFACES == NULL)
		{
			_OPEN_INTERFACES = OSSet::withCapacity(1);
		}
		if (_OPEN_INTERFACES)
		{
			_OPEN_INTERFACES->setObject(forClient);
			USBLog(6, "%s[%p]::handleOpen - IOUSBInterface[%p] added to open set", getName(), this, forClient);
			result = true;
		}
	}
	else
	{
		USBLog(5, "%s[%p]::handleOpen - [%p] is not an IOUSBInterface", getName(), this, forClient);
		result = super::handleOpen(forClient, options, arg);
		USBLog(6, "%s[%p]::handleOpen - super::handleOpen returned 0x%x", getName(), this, result);
	}
	
	return result;
}



void
IOUSBDevice::handleClose(IOService *forClient, IOOptionBits options)
{
	if (OSDynamicCast(IOUSBInterface, forClient))
	{
		if (_OPEN_INTERFACES)
		{
			_OPEN_INTERFACES->removeObject(forClient);
			USBLog(6, "%s[%p]::handleClose - IOUSBInterface[%p] removed from open set", getName(), this, forClient);
		}
		else
		{
			USBLog(2, "%s[%p]::handleClose - _OPEN_INTERFACES is NULL", getName(), this);
		}
	}
	else
	{
		USBLog(5, "%s[%p]::handleClose - [%p] is not an IOUSBInterface", getName(), this, forClient);
		super::handleClose(forClient, options);
	}
}


//
// IOUSBDevice::terminate
//
// Since IOUSBDevice is most often terminated directly by the USB family, willTerminate and didTerminate will not be called most of the time
// therefore we do things which we might otherwise have done in those methods before and after we call super::terminate (5024412)
//
bool	
IOUSBDevice::terminate(IOOptionBits options)
{	
	bool					retValue;
	IOUSBControllerV3		*v3Bus = OSDynamicCast(IOUSBControllerV3, _controller);
	
	USBLog(5, "IOUSBDevice(%s)[%p]::+terminate", getName(), this);
	
	if ( _NOTIFIERHANDLER_TIMER)
		_NOTIFIERHANDLER_TIMER->cancelTimeout();
	
	// if we are on an IOUSBControllerV3, make sure we enable any endpoints..
	if (v3Bus)
	{
		USBLog(2, "IOUSBDevice(%s)[%p]::terminate - making sure all endpoints are enabled", getName(), this);
		v3Bus->EnableAddressEndpoints(_address, true);
	}
	
	USBLog(5, "IOUSBDevice(%s)[%p]::terminate - calling super::terminate", getName(), this);
	retValue = super::terminate(options);
	if (_OPEN_INTERFACES)
	{
		USBLog(5, "IOUSBDevice(%s)[%p]::terminate - _openInterfaces has a count of %d", getName(), this, _OPEN_INTERFACES->getCount());
	}
	USBLog(5, "IOUSBDevice(%s)[%p]::-terminate", getName(), this);
	
	return retValue;
}



bool
IOUSBDevice::requestTerminate(IOService * provider, IOOptionBits options)
{
	USBLog(5, "IOUSBDevice(%s)[%p]::requestTerminate", getName(), this);
	return super::requestTerminate(provider, options);
}



IOReturn 
IOUSBDevice::message( UInt32 type, IOService * provider,  void * argument )
{
#pragma unused (provider)
    IOReturn 			err = kIOReturnSuccess;
    IOUSBRootHubDevice * 	rootHub = NULL;
    OSIterator *		iter;
    IOService *			client;
    
    switch ( type )
    {
	case kIOUSBMessagePortHasBeenReset:
		
		// First decode the error that the hub driver has sent to us
		_RESET_ERROR = * (IOReturn *) argument;
		
		USBLog(4,"%s[%p] received kIOUSBMessagePortHasBeenReset with error = 0x%x",getName(), this, _RESET_ERROR);
		
		// We have reset our device, so the configuration in the device is 0
		_currentConfigValue = 0;
			
		// If we had set our thread to sleep waiting for the reset to complete, now wake it up
		//
		if ( _RESET_COMMAND )
		{
			USBLog(3,"%s[%p]::message  calling commandWakeUp due to kIOUSBMessagePortHasBeenReset", getName(),this);
			if (_COMMAND_GATE)
			{
				_COMMAND_GATE->commandWakeup(&_RESET_COMMAND,  true);
			}
			else
			{
				USBLog(1,"%s[%p]::message  cannot call commandGate->wakeup because there is no gate", getName(),this);
				USBTrace( kUSBTDevice,  kTPDeviceMessage, (uintptr_t)this, kIOUSBMessagePortHasBeenReset, 0, 1 );
			}
		}
		
		// Recreate PipeZero object
		_pipeZero = IOUSBPipe::ToEndpoint(&_endpointZero, this, _controller, NULL);
		if (!_pipeZero)
		{
			USBLog(1,"%s[%p]::message DANGER could not recreate pipe Zero after reset", getName(), this);
			USBTrace( kUSBTDevice,  kTPDeviceMessage, (uintptr_t)this, kIOUSBMessagePortHasBeenReset, kIOReturnNoMemory, 2 );
			err = kIOReturnNoMemory;
		}
		
		// If there is an error, we don't want to forward the message.  Instead, we will return an error from ResetDevice() using the _RESET_ERROR
		if ( (_RESET_ERROR == kIOReturnSuccess) && (_pipeZero != NULL) )
		{
			USBLog(3, "%s[%p] calling messageClients (kIOUSBMessagePortHasBeenReset (port %d, err 0x%x))", getName(), this, (uint32_t)_PORT_NUMBER, _RESET_ERROR );
			(void) messageClients(kIOUSBMessagePortHasBeenReset, &_RESET_ERROR, sizeof(IOReturn));
		}
		
		// Finally, indicate that we have finished the reset
		_PORT_HAS_BEEN_RESET = true;
		
		break;
		
	case kIOUSBMessageHubIsDeviceConnected:
		
		// We need to send a message to all our clients (we are looking for the Hub driver) asking
		// them if the device for the given port is connected.  The hub driver will return the kIOReturnNoDevice
		// as an error.  However, any client that does not implement the message method will return
		// kIOReturnUnsupported, so we have to treat that as not an error
		//
		if ( _expansionData && _USBPLANE_PARENT )
		{
			_USBPLANE_PARENT->retain();
			
			USBLog(5, "%s at %d: Hub device name is %s at %d", getName(), _address, _USBPLANE_PARENT->getName(), _USBPLANE_PARENT->GetAddress());
			rootHub = OSDynamicCast(IOUSBRootHubDevice, _USBPLANE_PARENT);
			if ( !rootHub )
			{
				// Check to see if our parent is still connected. A kIOReturnSuccess means that
				// our parent is still connected to its parent.
				//
				err = _USBPLANE_PARENT->message(kIOUSBMessageHubIsDeviceConnected, NULL, 0);
			}
			
			if ( err == kIOReturnSuccess )
			{
				iter = _USBPLANE_PARENT->getClientIterator();
				while( (client = (IOService *) iter->getNextObject())) 
				{
					client->retain();
					IOSleep(1);
					
					err = client->message(kIOUSBMessageHubIsDeviceConnected, this, &_PORT_NUMBER);
					
					client->release();
					
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
			_USBPLANE_PARENT->release();
		}
		else
		{
			err = kIOReturnNoDevice;
		}
		
		break;
		
	case kIOUSBMessagePortWasNotSuspended:
	case kIOUSBMessagePortHasBeenResumed:
		USBLog(5,"%s[%p]::message - kIOUSBMessagePortWasNotSuspended or kIOUSBMessagePortHasBeenResumed (%p)", getName(), this, (void*)type);
		break;
		
	case kIOUSBMessagePortHasBeenSuspended:
		USBLog(5,"%s[%p]: kIOUSBMessagePortHasBeenSuspended with error: 0x%x",getName(),this, * (IOReturn *) argument);
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
    
	USBLog(6,"%s[%p]::message  received 0x%x",getName(), this, (unsigned int)type);

    return err;
}



void 
IOUSBDevice::stop( IOService * provider )
{
	IOReturn	kr = kIOReturnSuccess;
	
    USBLog(5, "%s[%p]::stop isInactive = %d", getName(), this, isInactive());
	
	if ( _SLEEPPOWERALLOCATED != 0 )
	{
		USBLog(6, "+%s[%p]::stop  we still had %d sleep power allocated", getName(), this, (uint32_t) _SLEEPPOWERALLOCATED);
		if ( _HUBPARENT )
			kr = _HUBPARENT->ReturnExtraPower(_PORT_NUMBER, kUSBPowerDuringSleep, _SLEEPPOWERALLOCATED);
		
		if ( kr != kIOReturnSuccess )
		{			
			USBLog(6, "+%s[%p]::stop  ReturnExtraPower(kUSBPowerDuringSleep) returned 0x%x", getName(), this, kr);
		}
	}
	
	if ( _WAKEPOWERALLOCATED != 0 )
	{
		USBLog(6, "+%s[%p]::stop  we still had %d wake power allocated", getName(), this, (uint32_t) _WAKEPOWERALLOCATED);
		if ( _HUBPARENT )
			kr = _HUBPARENT->ReturnExtraPower(_PORT_NUMBER, kUSBPowerDuringWake, _WAKEPOWERALLOCATED);
		
		if ( kr != kIOReturnSuccess )
		{			
			USBLog(6, "+%s[%p]::stop  ReturnExtraPower(kUSBPowerDuringSleep) returned 0x%x", getName(), this, kr);
		}
	}

	if ( _WORKLOOP && _NOTIFIERHANDLER_TIMER)
		_WORKLOOP->removeEventSource(_NOTIFIERHANDLER_TIMER);
	
	if ( _WORKLOOP )
		_WORKLOOP->removeEventSource(_COMMAND_GATE);

	super::stop(provider);
	
}



bool 
IOUSBDevice::finalize(IOOptionBits options)
{
    USBLog(5,"%s[%p]::finalize",getName(), this);
    
    if (_pipeZero) 
    {
        _pipeZero->Abort();
		_pipeZero->ClosePipe();
        _pipeZero->release();
        _pipeZero = NULL;
    }
    _currentConfigValue = 0;
	
    return(super::finalize(options));
}



void 
IOUSBDevice::free()
{
    if (_configList) 
    {
		int 	i;
        for(i=0; i<_descriptor.bNumConfigurations; i++) 
            if (_configList[i])
		{
			_configList[i]->release();
			_configList[i] = NULL;
		}
		IODelete(_configList, IOBufferMemoryDescriptor*, _descriptor.bNumConfigurations);
		_configList = NULL;
    }
	
	if (_interfaceList && _numInterfaces)
    {
		IODelete(_interfaceList, IOUSBInterface*, _numInterfaces);
		_interfaceList = NULL;
		_numInterfaces = 0;
    }
	
    _currentConfigValue = 0;
	
    //  This needs to be the LAST thing we do, as it disposes of our "fake" member
    //  variables.
    //
    if (_expansionData)
    {
		if (_DO_PORT_RESET_THREAD)
		{
			thread_call_cancel(_DO_PORT_RESET_THREAD);
			thread_call_free(_DO_PORT_RESET_THREAD);
			_DO_PORT_RESET_THREAD = NULL;
		}
		
		
		if (_DO_PORT_REENUMERATE_THREAD)
		{
			thread_call_cancel(_DO_PORT_REENUMERATE_THREAD);
			thread_call_free(_DO_PORT_REENUMERATE_THREAD);
			_DO_PORT_REENUMERATE_THREAD = NULL;
		}
		
		if (_DO_MESSAGE_CLIENTS_THREAD)
		{
			thread_call_cancel(_DO_MESSAGE_CLIENTS_THREAD);
			thread_call_free(_DO_MESSAGE_CLIENTS_THREAD);
			_DO_MESSAGE_CLIENTS_THREAD = NULL;
		}
		
		if (_OPEN_INTERFACES)
		{
			if (_OPEN_INTERFACES->getCount())
			{
				USBLog(2, "IOUSBDevice[%p]::free - stopping with some open interfaces", this);
			}
			else
			{
				_OPEN_INTERFACES->release();
				_OPEN_INTERFACES = NULL;
			}
		}
		
		if (_NOTIFIERHANDLER_TIMER)
		{
			_NOTIFIERHANDLER_TIMER->release();
			_NOTIFIERHANDLER_TIMER = NULL;
		}
		
		if ( _COMMAND_GATE )
		{
			_COMMAND_GATE->release();
			_COMMAND_GATE = NULL;
		}
		
		if (_WORKLOOP)
        {
            _WORKLOOP->release();
            _WORKLOOP = NULL;
        }
		
		if ( _USBPLANE_PARENT )
		{
			_USBPLANE_PARENT->release();
			_USBPLANE_PARENT = NULL;
		}
		
		if ( _HUBPARENT )
		{
			_HUBPARENT->release();
			_HUBPARENT = NULL;
		}
		
        IOFree(_expansionData, sizeof(ExpansionData));
        _expansionData = NULL;
    }

    super::free();
}



#pragma mark ееееееее IOUSBDevice Methods еееееееее
//================================================================================================
//
//   NewDevice - constructor
//
//================================================================================================
//
IOUSBDevice *
IOUSBDevice::NewDevice()
{
    return new IOUSBDevice;
}


//================================================================================================
//
//   init - not the IOKit version
//
//================================================================================================
//
bool 
IOUSBDevice::init(USBDeviceAddress deviceAddress, UInt32 powerAvailable, UInt8 speed, UInt8 maxPacketSize)
{
	
    if (!super::init())
    {
        USBLog(3,"%s[%p]::init super->init failed", getName(), this);
		return false;
    }
	
    // allocate our expansion data
    if (!_expansionData)
    {
		_expansionData = (ExpansionData *)IOMalloc(sizeof(ExpansionData));
		if (!_expansionData)
			return false;
		bzero(_expansionData, sizeof(ExpansionData));
    }
    
    _address = deviceAddress;
    _descriptor.bMaxPacketSize0 = maxPacketSize;
    _busPowerAvailable = powerAvailable;
    _speed = speed;
    _PORT_RESET_THREAD_ACTIVE = false;
    _ALLOW_CONFIGVALUE_OF_ZERO = false;
    _PORT_HAS_BEEN_RESET = false;
    
    USBLog(5,"%s @ %d (%dmA available, %s speed)", getName(), _address,(uint32_t)_busPowerAvailable*2, (_speed == kUSBDeviceSpeedLow) ? "low" : ((_speed == kUSBDeviceSpeedFull) ? "full" : "high") );
    
    return true;
}



//================================================================================================
//
//   SetProperties
//
//================================================================================================
//
void
IOUSBDevice::SetProperties()
{
    char			location [32];
    const struct	IORegistryPlane * 	usbPlane; 
    OSObject *		propertyObj = NULL;
	OSNumber *		port = NULL;
    OSBoolean *		boolObj = NULL;
	
    // Note that the "PortNum" property is set by the USB Hub driver
    //
	propertyObj = copyProperty("PortNum");
    port = OSDynamicCast( OSNumber, propertyObj );
    if ( port ) 
    {
        _PORT_NUMBER = port->unsigned32BitValue();
    }
	if (propertyObj)
	{
		propertyObj->release();
		propertyObj = NULL;
	}
    
    // Save a reference to our parent in the USB plane
    //
    usbPlane = getPlane(kIOUSBPlane);
    if ( usbPlane )
    {
        _USBPLANE_PARENT = OSDynamicCast( IOUSBDevice, getParentEntry( usbPlane ));
        _USBPLANE_PARENT->retain();
    }
    
    // Set the IOKit location and locationID property for the device.  We need to first find the locationID for
    // this device's hub
    //
    if ( _USBPLANE_PARENT )
    {
		OSNumber *		locationID = NULL;

		propertyObj = _USBPLANE_PARENT->copyProperty(kUSBDevicePropertyLocationID);
        locationID = OSDynamicCast( OSNumber, propertyObj );
        if ( locationID )
        {
            UInt32	childLocationID = GetChildLocationID( locationID->unsigned32BitValue(), _PORT_NUMBER );
            setProperty(kUSBDevicePropertyLocationID, childLocationID, 32);
			_LOCATIONID = childLocationID;
            snprintf(location, sizeof(location), "%x", (unsigned int) childLocationID);
            setLocation(location);
        }
		if (propertyObj)
		{
			propertyObj->release();
			propertyObj = NULL;
		}
    }
	
	// If our _controller has a "Need contiguous memory for isoch" property, copy it to our nub
	propertyObj = _controller->copyProperty(kUSBControllerNeedsContiguousMemoryForIsoch);
    boolObj = OSDynamicCast( OSBoolean, propertyObj );
    if ( boolObj && boolObj->isTrue() )
		setProperty(kUSBControllerNeedsContiguousMemoryForIsoch, kOSBooleanTrue);

	if (propertyObj)
	{
		propertyObj->release();
		propertyObj = NULL;
	}
}


//================================================================================================
//
//   GetChildLocationID
//
//================================================================================================
//
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

// Stop all activity, reset device, restart.  Will not renumerate the device
//
IOReturn 
IOUSBDevice::ResetDevice()
{
    UInt32		retries = 0;
	IOReturn	kr = kIOReturnSuccess;
	bool		gotLock;
	
    if ( isInactive() )
    {
        USBLog(1, "%s[%p]::ResetDevice - while terminating!", getName(), this);
		USBTrace( kUSBTDevice,  kTPDeviceResetDevice, (uintptr_t)this, _PORT_NUMBER, 0, 1);
        return kIOReturnNoDevice;
    }
	
	gotLock = OSCompareAndSwap(0, 1, &_RESET_AND_REENUMERATE_LOCK);
	if ( !gotLock )
	{
        USBLog(5, "%s[%p]::ResetDevice( port %d) while ResetDevice or ReEnumerateDevice in progress. Returning kIOReturnNotPermitted.", getName(), this, (uint32_t)_PORT_NUMBER );
		USBTrace( kUSBTDevice,  kTPDeviceResetDevice, (uintptr_t)this, _PORT_NUMBER, 0, 2);
		return kIOReturnNotPermitted;
	}
	
    retain();
	_PORT_HAS_BEEN_RESET = false;
	_RESET_ERROR = kIOReturnSuccess;
   
    USBLog(5, "+%s[%p]::ResetDevice for port %d", getName(), this, (uint32_t)_PORT_NUMBER );
	USBTrace( kUSBTDevice,  kTPDeviceResetDevice, (uintptr_t)this, _PORT_NUMBER, 0, 7);
    if ( thread_call_enter( _DO_PORT_RESET_THREAD )  == TRUE )
	{
		USBLog(3, "%s[%p]::ResetDevice for port %d, _DO_PORT_RESET_THREAD already queued", getName(), this, (uint32_t)_PORT_NUMBER );
		USBTrace( kUSBTDevice,  kTPDeviceResetDevice, (uintptr_t)this, _PORT_NUMBER, 0, 3);
		release();
		kr = kIOReturnBusy;
	}
	else 
	{
		if ( _WORKLOOP->inGate() && _COMMAND_GATE)
		{
			USBLog(5,"%s[%p]::ResetDevice calling commandSleep", getName(), this);
			
			_RESET_COMMAND = true;
			kr = _COMMAND_GATE->commandSleep(&_RESET_COMMAND);
			_RESET_COMMAND = false;
			if (kr != THREAD_AWAKENED)
			{
				USBLog(5,"%s[%p]::ResetDevice  commandSleep returned %d", getName(), this, kr);
				USBTrace( kUSBTDevice,  kTPDeviceResetDevice, (uintptr_t)this, _PORT_NUMBER, kr, 4);
			}
			USBLog(5,"%s[%p]::ResetDevice  woke up with error 0x%x", getName(), this, kr);
			
		}
		else
		{
			while ( !_PORT_HAS_BEEN_RESET && retries < (kMaxTimeToWaitForReset / 50) )
			{
				IOSleep(50);
				
				if ( isInactive() )
				{
					USBLog(3, "+%s[%p]::ResetDevice isInactive() while waiting for reset to finish", getName(), this );
					USBTrace( kUSBTDevice,  kTPDeviceResetDevice, (uintptr_t)this, _PORT_NUMBER, 0, 5);
					_RESET_ERROR = kIOReturnNoDevice;
					break;
				}
				
				retries++;
			}
		}
		
		// If we did all our retries, then the reset probably did not complete
		//
		if ( retries == (kMaxTimeToWaitForReset / 50) )
		{
			USBLog(5, "+%s[%p]::ResetDevice timed out while waiting for reset to finish", getName(), this );
			USBTrace( kUSBTDevice,  kTPDeviceResetDevice, (uintptr_t)this, _PORT_NUMBER, 0, 6);
			_RESET_ERROR = kIOUSBTransactionTimeout;
		}
		
		USBLog(5, "-%s[%p]::ResetDevice for port %d, error: 0x%x", getName(), this, (uint32_t)_PORT_NUMBER, _RESET_ERROR );
		
		kr = _RESET_ERROR;
	}
	
	gotLock = OSCompareAndSwap(1, 0, &_RESET_AND_REENUMERATE_LOCK);
	if ( !gotLock )
	{
        USBLog(1, "%s[%p]::ResetDevice( port %d) our resetLock was not set.  Unexpected", getName(), this, (uint32_t)_PORT_NUMBER );
	}
	
	USBTrace( kUSBTDevice,  kTPDeviceResetDevice, (uintptr_t)this, _PORT_NUMBER, kr, 8);
	
	release();
	
	return kr;
}

/******************************************************
* Helper Methods
******************************************************/

const IOUSBConfigurationDescriptor *
IOUSBDevice::FindConfig(UInt8 configValue, UInt8 *configIndex)
{
    int i;
    const IOUSBConfigurationDescriptor *cd = NULL;
	
    USBLog(7, "%s[%p]:FindConfig (%d)",getName(), this, configValue);
	
    for(i = 0; i < _descriptor.bNumConfigurations; i++) 
    {
		const IOUSBConfigurationDescriptor * localConfigDesc = NULL;
		
        localConfigDesc = GetFullConfigurationDescriptor(i);
        if (localConfigDesc == NULL)
            continue;
		
        if (localConfigDesc->bConfigurationValue == configValue)
		{
			// We have the config we were looking for, so assign to our return value
			cd = localConfigDesc;
            break;
		}
    }
	
	// If we have a value config, update the index
    if (cd && configIndex)
	{
		*configIndex = i;
	}
	
    USBLog(7, "%s[%p]:FindConfig (%d) returning %p",getName(), this, configValue, cd);
	USBTrace( kUSBTDevice, kTPDeviceConfigLock, (uintptr_t)this, (uintptr_t)configValue, (uintptr_t)cd, 12 );
	
    return cd;
}


static IOUSBDescriptorHeader *
NextDescriptor(const void *desc)
{
    const UInt8 *	next = (const UInt8 *)desc;
    UInt8			length = next[0];
	
	if ( length == 0 )
	{
		USBLog(1, "IOUSBDevice::NextDescriptor (%p), configLength was 0!, bad configuration descriptor", desc);
		return NULL;
	}
	
    next = &next[length];
	
    return((IOUSBDescriptorHeader *)next);
}



const IOUSBDescriptorHeader*
IOUSBDevice::FindNextDescriptor(const void *cur, UInt8 descType)
{
    IOUSBDescriptorHeader 		*hdr;
    UInt8				configIndex = 0;
    IOUSBConfigurationDescriptor	*curConfDesc;
    UInt16				curConfLength;
    UInt8				curConfig = 0;
	
    if (!_configList)
	{
		USBLog(5, "%s[%p]:FindNextDescriptor  _configList is NULL", getName(), this);
		return NULL;
	}
    
    if ( (_currentConfigValue == 0) && !_ALLOW_CONFIGVALUE_OF_ZERO )
    {
		IOReturn ret;
		ret = GetConfiguration(&curConfig);
		if ( ret == kIOReturnSuccess )
		{
			USBLog(5, "%s[%p]:DeviceRequest FindNextDescriptor _currentConfigValue to %d", getName(), this, curConfig);
			_currentConfigValue = curConfig;
		}
		
		if ( (_currentConfigValue == 0) && !_ALLOW_CONFIGVALUE_OF_ZERO )
		{
			USBLog(5, "%s[%p]:FindNextDescriptor  _currentConfigValue is 0", getName(), this);
			return NULL;
		}
    }
	
    curConfDesc = (IOUSBConfigurationDescriptor	*)FindConfig(_currentConfigValue, &configIndex);
    if (!curConfDesc)
	{
		USBLog(5, "%s[%p]:FindNextDescriptor  FindConfig(%d) returned NULL", getName(), this, _currentConfigValue);
		return NULL;
	}
	
    if (!_configList[configIndex])
	{
		USBLog(5, "%s[%p]:FindNextDescriptor  _configList[%d]", getName(), this, configIndex);
		return NULL;
	}
	
    curConfLength = _configList[configIndex]->getLength();
    if (!cur)
		hdr = (IOUSBDescriptorHeader*)curConfDesc;
    else
    {
		if ((cur < curConfDesc) || (((uintptr_t)cur - (uintptr_t)curConfDesc) >= curConfLength))
		{
			return NULL;
		}
		hdr = (IOUSBDescriptorHeader *)cur;
    }
	
    do 
    {
		IOUSBDescriptorHeader 		*lasthdr = hdr;
		hdr = NextDescriptor(hdr);
		
		if (hdr == NULL)
		{
			return NULL;
		}
		
		if (lasthdr == hdr)
		{
			return NULL;
		}
		
        if (((uintptr_t)hdr - (uintptr_t)curConfDesc) >= curConfLength)
		{
            return NULL;
		}
        if (descType == 0)
		{
            return hdr;			// type 0 is wildcard.
		}
	    
        if (hdr->bDescriptorType == descType)
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
	
    while (interface && (interface < end))
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
		{
			return NULL;
		}
		
		intf =  GetInterface(id);
		// since not all interfaces (e.g. alternate) are instantiated, we only terminate if we find one that is
    }
    return intf;
    
}



OSIterator *
IOUSBDevice::CreateInterfaceIterator(IOUSBFindInterfaceRequest *request)
{
    IOUSBInterfaceIterator *iter = new IOUSBInterfaceIterator;
    if (!iter)
		return NULL;
	
    if (!iter->init(this, request)) 
    {
		iter->release();
		iter = NULL;
    }
    return iter;
}



const IOUSBConfigurationDescriptor*
IOUSBDevice::GetFullConfigurationDescriptor(UInt8 index)
{
    IOReturn						err;
    IOBufferMemoryDescriptor *		localConfigIOMD = NULL;
    IOUSBConfigurationDescriptor *  configDescriptor = NULL;
	uint32_t						overrideMaxPower = 0;
	OSNumber *						osNumberRef = NULL;
	
   
    if (!_configList || (index >= _descriptor.bNumConfigurations))
        return NULL;
    
	if ( _WORKLOOP->onThread() )
	{
		USBLog(1, "%s[%p]::GetFullConfigurationDescriptor(index %d) - called on workloop thread, returning NULL", getName(), this, (uint32_t)index);
		USBTrace( kUSBTDevice,  kTPDeviceGetFullConfigurationDescriptor, (uintptr_t)this, index, 0, 5 );
        return NULL;
	}
	
	// if we already have a cached copy, then just use that
    if (_configList[index] != NULL)
	{
		configDescriptor = (IOUSBConfigurationDescriptor *)_configList[index]->getBytesNoCopy();
		USBTrace( kUSBTDevice,  kTPDeviceGetFullConfigurationDescriptor, (uintptr_t)this, index, (uintptr_t)configDescriptor, 6 );
		USBLog(7, "%s[%p]::GetFullConfigurationDescriptor - Index (%x) - returning cached confDescriptor %p", getName(), this, index, configDescriptor);
		return configDescriptor ;
	}

	// it is possible that we could end up in a race condition with multiple drivers trying to get a config descriptor.
    // we are not able to fix that by running this code through the command gate, because then the actual bus command 
    // would not be able to complete with us holding the gate. so we have a device specific lock instead
    
    err = TakeGetConfigLock();
	if (err)
	{
		USBLog(2, "%s[%p]::GetFullConfigurationDescriptor(index %d) - TakeGetConfigLock returned 0x%x", getName(), this, index, (uint32_t)err);
		USBTrace( kUSBTDevice,  kTPDeviceGetFullConfigurationDescriptor, (uintptr_t)this, index, err, 7 );
		return NULL;
	}
    
    if (_configList[index] == NULL) 
    {
        int								len;
        IOUSBConfigurationDescHeader	temp;
        UInt16							idVendor = USBToHostWord(_descriptor.idVendor);
        UInt16							idProduct = USBToHostWord(_descriptor.idProduct);
		OSObject *						propertyObj = NULL;

		if ( index == 0 )
		{
			propertyObj = copyProperty("OverrideConfig0MaxPower");
			osNumberRef = OSDynamicCast( OSNumber, propertyObj );
			if ( osNumberRef )
			{
				overrideMaxPower = osNumberRef->unsigned32BitValue();
				USBLog(6, "%s[%p]::GetFullConfigurationDescriptor - overriding MaxPower to %d for config 0 of VID 0x%x, PID 0x%x", getName(), this, overrideMaxPower, idVendor, idProduct);
			}
			if (propertyObj)
			{
				propertyObj->release();
				propertyObj = NULL;
			}
		}
		
		// Look to see if we have a property to override the config descriptor for this device
		propertyObj = copyProperty(kConfigurationDescriptorOverride);
		if (propertyObj )
		{
			OSArray *			configOverride	= OSDynamicCast(OSArray, propertyObj);

			if (configOverride)
			{
				USBLog(6, "%s[%p]::GetFullConfigurationDescriptor - found ConfigurationDescriptorOverride with capacity of %d", getName(), this, configOverride->getCount());
				
				// See if we can get the object at our configIndex
				
				OSData	* theConfigData = OSDynamicCast(OSData, configOverride->getObject(index));
				if (theConfigData)
				{
					// Read the static descriptor into an IOBMD
					localConfigIOMD = IOBufferMemoryDescriptor::withBytes(theConfigData->getBytesNoCopy(), theConfigData->getLength(), kIODirectionIn, false);
					
					if (!localConfigIOMD)
					{
						USBLog(1, "%s[%p]::GetFullConfigurationDescriptor - unable to get memory buffer for override (capacity requested: %d)", getName(), this, theConfigData->getLength());
						USBTrace( kUSBTDevice,  kTPDeviceGetFullConfigurationDescriptor, (uintptr_t)this, theConfigData->getLength(), 0, 9 );
					}
					else
					{
						USBLog(1, "%s[%p]::GetFullConfigurationDescriptor - we were able to allocate an IOBMD with %d bytes for our override of Config %d", getName(), this, theConfigData->getLength(), index );
						USBTrace( kUSBTDevice,  kTPDeviceGetFullConfigurationDescriptor, (uintptr_t)this, theConfigData->getLength(), 0, 10 );
					
						_configList[index] = localConfigIOMD;
					}
				}
			}
			
			propertyObj->release();
		} 
		
		if (_configList[index] == NULL) 
		{
			// 2755742 - workaround for a ill behaved device
			// Also do this for Fujitsu scanner VID = 0x4C5 PID = 0x1040
			if ( ((idVendor == 0x3f0) && (idProduct == 0x1001)) || ((idVendor == 0x4c5) && (idProduct == 0x1040)) )
			{
				USBLog(3, "%s[%p]::GetFullConfigurationDescriptor - assuming config desc length of 39", getName(), this);
				len = 39;
			}
			else if  ((idVendor == 0x13FE) && (idProduct == 0x1E00))
			{
				// Another ill-behaved device, maybe:  VID = 0x13FE, PID = 0x1E00, len = 0x0020
				USBLog(1, "%s[%p]::GetFullConfigurationDescriptor - assuming config desc length of 0x0020", getName(), this);
				USBTrace( kUSBTDevice,  kTPDeviceGetFullConfigurationDescriptor, (uintptr_t)this, 0x13FE, 0x1E00, 1 );
				len = 0x0020;
			}
			else
			{
				// Get the head for the configuration descriptor
				//
				temp.bLength = 0;
				temp.bDescriptorType = 0;
				temp.wTotalLength = 0;
				
				USBLog(5, "%s[%p]::GetFullConfigurationDescriptor - Index (%x) - getting first %ld bytes of config descriptor", getName(), this, index, sizeof(temp));
				err = GetConfigDescriptor(index, &temp, sizeof(temp));
				
				// If we get an error, try getting the first 9 bytes of the config descriptor.  Note that the structure IOUSBConfigurationDescriptor is 10 bytes long
				// because of padding (and we can't change it), so hardcode the value to 9 bytes.
				//
				if ( err != kIOReturnSuccess)
				{
					IOUSBConfigurationDescriptor	confDesc;
					
					bzero( &confDesc, 9);
					
					USBLog(5, "%s[%p]::GetFullConfigurationDescriptor - Index (%x) - Got error (0x%x), trying first %d bytes of config descriptor", getName(), this, index, err, 9);
					err = GetConfigDescriptor(index, &confDesc, 9);
					if ( (kIOReturnSuccess != err) && ( kIOReturnOverrun != err ) )
					{
						USBLog(1, "%s[%p]::GetFullConfigurationDescriptor - Error (%x) getting first %d bytes of config descriptor", getName(), this, err, 9);
						USBTrace( kUSBTDevice,  kTPDeviceGetFullConfigurationDescriptor, (uintptr_t)this, err, 9, 2 );
						goto Exit;
					}
					
					USBError(1, "USB Device %s is violating Section 9.3.5 of the USB Specification -- Error in GetConfigDescriptor( wLength = 4)", getName());
					if ( kIOReturnOverrun == err )
					{
						// If we get more data than we requested, then verify that the config descriptor header makes sense
						//
						if ( !((confDesc.bLength == 9) && (confDesc.bDescriptorType == kUSBConfDesc) && (confDesc.wTotalLength != 0)) )
						{
							USBLog(1, "%s[%p]::GetFullConfigurationDescriptor - Overrun error and data returned is not correct (%d, %d, %d)", getName(), this, temp.bLength, temp.bDescriptorType, USBToHostWord(temp.wTotalLength));
							USBTrace( kUSBTDevice,  kTPDeviceGetFullConfigurationDescriptor, temp.bLength, temp.bDescriptorType, USBToHostWord(temp.wTotalLength), kIOReturnOverrun );
							goto Exit;
						}
						USBError(1, "USB Device %s is violating Section 9.3.5 of the USB Specification -- Error in GetConfigDescriptor( wLength = 9)", getName());
					}
					// Save our length for our next request
					//
					len = USBToHostWord(confDesc.wTotalLength);
				}
				else
				{
					// Save our length for our next request
					//
					len = USBToHostWord(temp.wTotalLength);
				}
			}
			
			// Allocate a buffer to read in the whole descriptor
			//
			localConfigIOMD = IOBufferMemoryDescriptor::withCapacity(len, kIODirectionIn);
			
			if (!localConfigIOMD)
			{
				USBLog(1, "%s[%p]::GetFullConfigurationDescriptor - unable to get memory buffer (capacity requested: %d)", getName(), this, len);
				USBTrace( kUSBTDevice,  kTPDeviceGetFullConfigurationDescriptor, (uintptr_t)this, len, 0, 3 );
				goto Exit;
			}
			
			USBLog(5, "%s[%p]::GetFullConfigurationDescriptor - Index (%x) - getting full %d bytes of config descriptor", getName(), this, index, len);
			err = GetConfigDescriptor(index, localConfigIOMD->getBytesNoCopy(), len);
			if (err) 
			{
				USBLog(1, "%s[%p]::GetFullConfigurationDescriptor - Error (%x) getting full %d bytes of config descriptor", getName(), this, err, len);
				USBTrace( kUSBTDevice,  kTPDeviceGetFullConfigurationDescriptor, (uintptr_t)this, err, len, 4 );
				
				if ( localConfigIOMD )
				{
					localConfigIOMD->release();
					localConfigIOMD = NULL;
				}
				goto Exit;
			}
			else
			{
				// If we get to this point and configList[index] is NOT NULL, then it means that another thread already got the config descriptor.
				// In that case, let's just release our descriptor and return the already allocated one.
				//
				if ( _configList[index] != NULL )
					localConfigIOMD->release();
				else
				{
					// See if we need to override MaxPower
					if ( index == 0 && overrideMaxPower > 0)
					{
						IOUSBConfigurationDescriptor *	myConfigDesc = (IOUSBConfigurationDescriptor *)localConfigIOMD->getBytesNoCopy();
						
						myConfigDesc->MaxPower = overrideMaxPower;
						localConfigIOMD->writeBytes(0, myConfigDesc, len);
					}
					
					_configList[index] = localConfigIOMD;
				}
			}
			
		}
	}
    configDescriptor = (IOUSBConfigurationDescriptor *)_configList[index]->getBytesNoCopy();

Exit:
	ReleaseGetConfigLock();
	
	USBLog(6, "%s[%p]::GetFullConfigurationDescriptor - Index (%x) - returning confDescriptor %p", getName(), this, index, configDescriptor);
	USBTrace( kUSBTDevice,  kTPDeviceGetFullConfigurationDescriptor, (uintptr_t)this, index, (uintptr_t)configDescriptor, 8 );
    return configDescriptor ;
}



IOReturn 
IOUSBDevice::GetDeviceDescriptor(IOUSBDeviceDescriptor *desc, UInt32 size)
{
    IOUSBDevRequest	request;
    IOReturn		err;
	
    USBLog(5, "%s[%p]::GetDeviceDescriptor (size %d)", getName(), this, (uint32_t)size);
	
    if (!desc)
        return  kIOReturnBadArgument;
	
    request.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice);
    request.bRequest = kUSBRqGetDescriptor;
    request.wValue = kUSBDeviceDesc << 8;
    request.wIndex = 0;
    request.wLength = size;
    request.pData = desc;
	
    err = DeviceRequest(&request, 5000, 0);
    
    if (err)
    {
        USBLog(1,"%s[%p]: Error (0x%x) getting device device descriptor", getName(), this, err);
		USBTrace( kUSBTDevice,  kTPDeviceGetDeviceDescriptor, (uintptr_t)this, err, request.bmRequestType, 0 );
    }
	
    return err;
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
	
    USBLog(5, "%s[%p]::GetConfigDescriptor (length: %d)", getName(), this, (uint32_t)len);
	
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
        USBLog(1,"%s[%p]: Error (0x%x) getting device config descriptor", getName(), this, err);
		USBTrace( kUSBTDevice,  kTPDeviceGetConfigDescriptor, (uintptr_t)this, err, request.bmRequestType, 0 );
    }
	
    return err;
}

void
IOUSBDevice::TerminateInterfaces()
{
    int i;
    
    USBLog(5,"%s[%p]::TerminateInterfaces interfaceList %p, numInterfaces: %d",getName(),this, _interfaceList, _numInterfaces);
    
    if (_interfaceList && _numInterfaces)
    {
		// Go through the list of interfaces and release them
        //
		for (i=0; i < _numInterfaces; i++)
		{
			IOUSBInterface *intf = _interfaceList[i];
			if (intf)
			{
				_interfaceList[i] = NULL;
				
				// this call should do everything, including detaching the interface from us
                //
				USBLog(5,"%s[%p]::TerminateInterfaces terminating interface %p",getName(),this, intf);
				intf->terminate(kIOServiceSynchronous);
			}
		}
		// free the interface list memory
        //
		IODelete(_interfaceList, IOUSBInterface*, _numInterfaces);
		_interfaceList = NULL;
		_numInterfaces = 0;
    }
    USBLog(5,"-%s[%p]::TerminateInterfaces",getName(),this);
    
}

IOReturn 
IOUSBDevice::SetConfiguration(IOService *forClient, UInt8 configNumber, bool startMatchingInterfaces)
{
    IOReturn							err = kIOReturnSuccess;
    IOUSBDevRequest						request;
    const IOUSBConfigurationDescriptor *confDesc;
    bool								allowConfigValueOfZero = false;
    OSObject *							propertyObj = NULL;
	OSBoolean *							boolObj = NULL;
	OSNumber *							numberObj = NULL;
	OSBoolean *							lowPowerNotificationDisplayed;
	bool								lowPowerDisplayed = false;
	
    // See if we have the _ALLOW_CONFIGVALUE_OF_ZERO errata
    //
	propertyObj = copyProperty(kAllowConfigValueOfZero);
	boolObj = OSDynamicCast( OSBoolean, propertyObj);
    if ( boolObj && boolObj->isTrue() )
        _ALLOW_CONFIGVALUE_OF_ZERO = true;
	if (propertyObj)
	{
		propertyObj->release();
		propertyObj = NULL;
	}
	
	propertyObj = copyProperty(("kNeedsExtraResetTime"));
	boolObj = OSDynamicCast( OSBoolean, propertyObj);
    if ( boolObj && boolObj->isTrue() )
    {
        USBLog(3,"%s[%p]::SetConfiguration  kNeedsExtraResetTime is true",getName(), this);
        _ADD_EXTRA_RESET_TIME = true;
    }
	if (propertyObj)
	{
		propertyObj->release();
		propertyObj = NULL;
	}

	// See if there is kUSBDeviceResumeRecoveryTime property and if so
	// Set our _hubResumeRecoveryTime, overriding with a property-based errata
	propertyObj = copyProperty((kUSBDeviceResumeRecoveryTime));
	numberObj = OSDynamicCast( OSNumber, propertyObj);
	if ( numberObj )
	{
		IOUSBHubPortReEnumerateParam	portRecoveryOptions;
		
		portRecoveryOptions.portNumber = _PORT_NUMBER;
		portRecoveryOptions.options = numberObj->unsigned32BitValue();
		if (portRecoveryOptions.options < 10) portRecoveryOptions.options  = 10;
		
		USBLog(5, "%s[%p]::SetConfiguration - port %d, calling Hub to set kUSBDeviceResumeRecoveryTime to %d", getName(), this, (uint32_t)_PORT_NUMBER, (uint32_t)portRecoveryOptions.options );
        _USBPLANE_PARENT->retain();
        err = _USBPLANE_PARENT->messageClients(kIOUSBMessageHubSetPortRecoveryTime, &portRecoveryOptions, sizeof(IOUSBHubPortReEnumerateParam));
        _USBPLANE_PARENT->release();
	}
	
	if (propertyObj)
		propertyObj->release();
	
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
		setProperty("Failed Requested Power", confDesc->MaxPower, 32);

		lowPowerNotificationDisplayed = OSDynamicCast( OSBoolean, getProperty("Low Power Displayed") );
		if ( !lowPowerNotificationDisplayed or (lowPowerNotificationDisplayed && lowPowerNotificationDisplayed->isFalse()) )
		{
			lowPowerDisplayed = true;
			DisplayUserNotification(kUSBNotEnoughPowerNotificationType);
			setProperty("Low Power Displayed", lowPowerDisplayed);
		}

        USBLog(1,"%s[%p]::SetConfiguration  Not enough bus power to configure device want %d, available: %d",getName(), this, (uint32_t)confDesc->MaxPower, (uint32_t)_busPowerAvailable);
		USBTrace( kUSBTDevice,  kTPDeviceSetConfiguration, (uintptr_t)this, (uint32_t)confDesc->MaxPower, (uint32_t)_busPowerAvailable, 1);
		
        IOLog("USB Low Power Notice:  The device \"%s\" cannot be used because there is not enough power to configure it\n",getName());
		
        return kIOUSBNotEnoughPowerErr;
    }
	
	if ( confDesc )
		setProperty("Requested Power", confDesc->MaxPower, 32);
	
	setProperty("Low Power Displayed", lowPowerDisplayed);

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
        USBLog(1,"%s[%p]: error setting config. err=0x%x", getName(), this, err);
		USBTrace( kUSBTDevice,  kTPDeviceSetConfiguration, (uintptr_t)this, err, request.bmRequestType, request.wValue );
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
    if (configNumber || _ALLOW_CONFIGVALUE_OF_ZERO)
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
            
            USBLog(5,"%s[%p]::SetConfiguration  Found InterfaceDescription[%d] = %p",getName(), this, i, intfDesc);
            
            // Check to see whether this interface has the appropriate alternate setting.  If not, then
            // keep getting new ones until we exhaust the list or we match one with the correct setting.
            //
            while ( gotAlternateSetting && (intfDesc != NULL) && (intfDesc->bAlternateSetting != altSetting) )
            {
                intfDesc = (IOUSBInterfaceDescriptor *)FindNextDescriptor(intfDesc, kUSBInterfaceDesc);
            }
			
			if (intfDesc)
            {
				IOUSBInterface *	theInterface = NULL;

                if ( !gotAlternateSetting )
                {
                    // For the first interface, we don't know the alternate setting, so here is where we set it.
                    //
                    altSetting = intfDesc->bAlternateSetting;
                    gotAlternateSetting = true;
                }
                
				theInterface = IOUSBInterface::withDescriptors(confDesc, intfDesc);
                if (theInterface)
                {
					USBLog(6,"%s[%p]::SetConfiguration  retaining the interface[%d] = %p",getName(), this, i, theInterface );
					// retain this interface until we are done with SetConfiguration()
					theInterface->retain();
					
					// Now that we have it retained, put it into our interfaceList
					_interfaceList[i] = theInterface;
					
                    USBLog(5,"%s[%p]::SetConfiguration  Attaching an interface[%d] = %p",getName(), this, i, _interfaceList[i] );
                    
                    if ( _interfaceList[i]->attach(this) )
                    {
                        _interfaceList[i]->release();
                        if (!_interfaceList[i]->start(this))
                        {
                            USBLog(3,"%s[%p]::SetConfiguration  Could not start IOUSBInterface[%d] = %p",getName(), this, i, _interfaceList[i] );
                            _interfaceList[i]->detach(this);
  							_interfaceList[i]->release();
                          _interfaceList[i] = NULL;
                        }
					}
                    else
                    {
                        USBLog(3,"%s[%p]::SetConfiguration  Attaching an interface[%d] = %p failed",getName(), this, i, _interfaceList[i] );
 						_interfaceList[i]->release();
						_interfaceList[i] = NULL;
                       return kIOReturnNoResources;
                    }
					
                }
                else
                {
                    USBLog(3,"%s[%p]::SetConfiguration  Could not init InterfaceDescription[%d] = %p",getName(), this, i, intfDesc );
                    return kIOReturnNoMemory;
                }
				
            }
			else
            {
				USBLog(3,"%s[%p]: SetConfiguration(%d): could not find interface in InterfaceDescription[%d] = %p", getName(), this, configNumber, i, intfDesc);
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
                    
                    USBLog(5,"%s[%p]::SetConfiguration  matching to interface[%d] = %p",getName(), this, i, theInterface);
                    
                    // need to do an extra retain in case we get terminated while loading a driver
                    theInterface->retain();
					theInterface->registerService(kIOServiceSynchronous);
                    theInterface->release();
                }
			}
            release();
		}
		
		// We need to release the interface that we retain()'d when we created it
		for (i=0; i<_numInterfaces; i++)
		{
			if (_interfaceList[i])
			{
				USBLog(6,"%s[%p]::SetConfiguration  releasing interface[%d] = %p",getName(), this, i, _interfaceList[i]);
				_interfaceList[i]->release();
			}
		}
    }
    else
    {
        USBLog(5,"%s[%p]::SetConfiguration  Not matching to interfaces: configNumber (%d) is zero and no errata to allow it (%d)",getName(), this, configNumber, _ALLOW_CONFIGVALUE_OF_ZERO);
    }
    
    USBLog(5,"%s[%p]::SetConfiguration  returning success",getName(), this);
    
    return kIOReturnSuccess;
}



IOReturn 
IOUSBDevice::SetFeature(UInt8 feature)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevRequest	request;
	
    USBLog(5, "%s[%p]::SetFeature (%d)", getName(), this, feature);
	
    request.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBStandard, kUSBDevice);
    request.bRequest = kUSBRqSetFeature;
    request.wValue = feature;
    request.wIndex = 0;
    request.wLength = 0;
    request.pData = 0;
	
    err = DeviceRequest(&request, 5000, 0);
	
    if (err)
    {
        USBLog(1, "%s[%p]: error setting feature (%d). err=0x%x", getName(), this, feature, err);
		USBTrace( kUSBTDevice,  kTPDeviceSetFeature, (uintptr_t)this, err, request.bmRequestType, request.wValue );
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
    if (!cd)
        return kIOUSBConfigNotFound;
	
    toCopy = USBToHostWord(cd->wTotalLength);
    if (len < toCopy)
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
    char	logString[256]="";
    UInt32      wildCardMatches = 0;
    UInt32      vendorIsWildCard = 0;
    UInt32      productIsWildCard = 0;
    UInt32      deviceReleaseIsWildCard = 0;
    UInt32      classIsWildCard = 0;
    UInt32      subClassIsWildCard = 0;
    UInt32      protocolIsWildCard = 0;
    bool	usedMaskForProductID = false;
    
    if ( table == NULL )
    {
        return false;
    }
	
    bool	vendorPropertyExists = table->getObject(kUSBVendorID) ? true : false ;
    bool	productPropertyExists = table->getObject(kUSBProductID) ? true : false ;
    bool	deviceReleasePropertyExists = table->getObject(kUSBDeviceReleaseNumber) ? true : false ;
    bool	deviceClassPropertyExists = table->getObject(kUSBDeviceClass) ? true : false ;
    bool	deviceSubClassPropertyExists = table->getObject(kUSBDeviceSubClass) ? true : false ;
    bool	deviceProtocolPropertyExists= table->getObject(kUSBDeviceProtocol) ? true : false ;
    bool        productIDMaskExists = table->getObject(kUSBProductIDMask) ? true : false;
	
    // USBComparePropery() will return false if the property does NOT exist, or if it exists and it doesn't match
    //
    bool	vendorPropertyMatches = USBCompareProperty(table, kUSBVendorID);
    bool	productPropertyMatches = USBCompareProperty(table, kUSBProductID);
    bool	deviceReleasePropertyMatches = USBCompareProperty(table, kUSBDeviceReleaseNumber);
    bool	deviceClassPropertyMatches = USBCompareProperty(table, kUSBDeviceClass);
    bool	deviceSubClassPropertyMatches = USBCompareProperty(table, kUSBDeviceSubClass);
    bool	deviceProtocolPropertyMatches= USBCompareProperty(table, kUSBDeviceProtocol);
	
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
			productIsWildCard++; 
    }
	
    if ( !deviceReleasePropertyMatches )              
    { 
		deviceReleasePropertyMatches = IsWildCardMatch(table, kUSBDeviceReleaseNumber); 
		if ( deviceReleasePropertyMatches ) 
			deviceReleaseIsWildCard++; 
    }
	
    if ( !deviceClassPropertyMatches )              
    { 
		deviceClassPropertyMatches = IsWildCardMatch(table, kUSBDeviceClass); 
		if ( deviceClassPropertyMatches ) 
			classIsWildCard++; 
    }
	
    if ( !deviceSubClassPropertyMatches )            
    { 
		deviceSubClassPropertyMatches = IsWildCardMatch(table, kUSBDeviceSubClass); 
		if ( deviceSubClassPropertyMatches ) 
			subClassIsWildCard++; 
    }
	
    if ( !deviceProtocolPropertyMatches )          
    { 
		deviceProtocolPropertyMatches = IsWildCardMatch(table, kUSBDeviceProtocol); 
		if ( deviceProtocolPropertyMatches ) 
			protocolIsWildCard++; 
    }
	
    // If the productID didn't match, then see if there is a kProductIDMask property.  If there is, mask this device's prodID and the dictionary's prodID with it and see
    // if they are equal and if so, then we say we productID matches.
    //
    if ( productIDMaskExists && !productPropertyMatches )
    {
		productPropertyMatches = USBComparePropertyWithMask( table, kUSBProductID, kUSBProductIDMask);
		usedMaskForProductID = productPropertyMatches;
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
    if ( propertyScore >= 10000 ) 
        propertyScore = 9000;
    
    // Do the Device Matching algorithm
    //
	OSObject *deviceClassProp = copyProperty(kUSBDeviceClass);
    OSNumber *deviceClass = OSDynamicCast(OSNumber, deviceClassProp);
	
    if ( vendorPropertyMatches && productPropertyMatches && deviceReleasePropertyMatches &&
		 (!deviceClassPropertyExists || deviceClassPropertyMatches) && 
		 (!deviceSubClassPropertyExists || deviceSubClassPropertyMatches) && 
		 (!deviceProtocolPropertyExists || deviceProtocolPropertyMatches) )
    {
        *score = 100000;
        wildCardMatches = vendorIsWildCard + productIsWildCard + deviceReleaseIsWildCard;
    }
    else if ( vendorPropertyMatches && productPropertyMatches  && 
			  (!deviceReleasePropertyExists || deviceReleasePropertyMatches) && 
			  (!deviceClassPropertyExists || deviceClassPropertyMatches) && 
			  (!deviceSubClassPropertyExists || deviceSubClassPropertyMatches) && 
			  (!deviceProtocolPropertyExists || deviceProtocolPropertyMatches) )
    {
        *score = 90000;
        wildCardMatches = vendorIsWildCard + productIsWildCard;
    }
    else if ( deviceClass && deviceClass->unsigned32BitValue() == kUSBVendorSpecificClass )
    {
        if (  vendorPropertyMatches && deviceSubClassPropertyMatches && deviceProtocolPropertyMatches &&
			  (!deviceClassPropertyExists || deviceClassPropertyMatches) && 
			  !deviceReleasePropertyExists &&  !productPropertyExists )
        {
            *score = 80000;
            wildCardMatches = vendorIsWildCard + subClassIsWildCard + protocolIsWildCard;
        }
        else if ( vendorPropertyMatches && deviceSubClassPropertyMatches &&
				  (!deviceClassPropertyExists || deviceClassPropertyMatches) && 
				  !deviceProtocolPropertyExists && !deviceReleasePropertyExists && 
				  !productPropertyExists )
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
    else if (  deviceClassPropertyMatches && deviceSubClassPropertyMatches && deviceProtocolPropertyMatches &&
			   !deviceReleasePropertyExists &&  !vendorPropertyExists && !productPropertyExists )
    {
        *score = 60000;
        wildCardMatches = classIsWildCard + subClassIsWildCard + protocolIsWildCard;
    }
    else if (  deviceClassPropertyMatches && deviceSubClassPropertyMatches &&
			   !deviceProtocolPropertyExists && !deviceReleasePropertyExists &&  !vendorPropertyExists && 
			   !productPropertyExists )
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
		OSString *	identifier = OSDynamicCast(OSString, table->getObject("CFBundleIdentifier"));
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
		OSNumber *	dictMask = (OSNumber *) table->getObject(kUSBProductIDMask);
		bool		match = false;
		
		if (identifier)
		{
			USBLog(5,"Finding device driver for %s, matching personality using %s, score: %d, wildCard = %d", getName(), identifier->getCStringNoCopy(), (uint32_t)*score, (uint32_t)wildCardMatches);
		}
		else
		{
			USBLog(6,"Finding device driver for %s, matching user client dictionary, score: %d", getName(), (uint32_t)*score);
		}
		
		if ( vendor && product && release && deviceClass && deviceSubClass && protocol )
		{
			char tempString[256]="";
			
			snprintf(logString, sizeof(logString), "\tMatched: ");
			if ( vendorPropertyMatches ) { match = true; snprintf(tempString, sizeof(tempString), "idVendor (%d) ", vendor->unsigned32BitValue()); strlcat(logString, tempString, sizeof(logString)); }
			if ( productPropertyMatches ) { match = true; snprintf(tempString, sizeof(tempString), "idProduct (%d) ", product->unsigned32BitValue());  strlcat(logString, tempString, sizeof(logString));}
			if ( usedMaskForProductID && dictMask && dictProduct ) { snprintf(tempString, sizeof(tempString), "to (%d) with mask (0x%x), ", dictProduct->unsigned32BitValue(), dictMask->unsigned32BitValue()); strlcat(logString, tempString, sizeof(logString));}
			if ( deviceReleasePropertyMatches ) { match = true; snprintf(tempString, sizeof(tempString), "bcdDevice (%d) ", release->unsigned32BitValue());  strlcat(logString, tempString, sizeof(logString));}
			if ( deviceClassPropertyMatches ) { match = true; snprintf(tempString, sizeof(tempString), "bDeviceClass (%d) ", deviceClass->unsigned32BitValue());  strlcat(logString, tempString, sizeof(logString));}
			if ( deviceSubClassPropertyMatches ) { match = true; snprintf(tempString, sizeof(tempString), "bDeviceSubClass (%d) ", deviceSubClass->unsigned32BitValue());  strlcat(logString, tempString, sizeof(logString));}
			if ( deviceProtocolPropertyMatches ) { match = true; snprintf(tempString, sizeof(tempString), "bDeviceProtocol (%d) ", protocol->unsigned32BitValue());  strlcat(logString, tempString, sizeof(logString));}
			if ( !match ) strlcat(logString, "no properties", sizeof(logString));
			
			USBLog(6,"%s", logString);
			
			snprintf(logString, sizeof(logString), "\tDidn't Match: ");
			
			match = false;
			if ( vendorPropertyExists && !vendorPropertyMatches && dictVendor ) 
			{ 
				match = true; 
				snprintf(tempString, sizeof(tempString), "idVendor (%d,%d) ", dictVendor->unsigned32BitValue(), vendor->unsigned32BitValue());
				strlcat(logString, tempString, sizeof(logString));
			}
			if ( productPropertyExists && !productPropertyMatches && dictProduct) 
			{ 
				match = true; 
				snprintf(tempString, sizeof(tempString), "idProduct (%d,%d) ", dictProduct->unsigned32BitValue(), product->unsigned32BitValue());
				strlcat(logString, tempString, sizeof(logString)); 
			}
			if ( deviceReleasePropertyExists && !deviceReleasePropertyMatches && dictRelease) 
			{ 
				match = true; 
				snprintf(tempString, sizeof(tempString), "bcdDevice (%d,%d) ", dictRelease->unsigned32BitValue(), release->unsigned32BitValue());
				strlcat(logString, tempString, sizeof(logString)); 
			}
			if ( deviceClassPropertyExists && !deviceClassPropertyMatches && dictDeviceClass) 
			{ 
				match = true; 
				snprintf(tempString, sizeof(tempString), "bDeviceClass (%d,%d) ", dictDeviceClass->unsigned32BitValue(), deviceClass->unsigned32BitValue());
				strlcat(logString, tempString, sizeof(logString));
			}
			if ( deviceSubClassPropertyExists && !deviceSubClassPropertyMatches && dictDeviceSubClass) 
			{ 
				match = true; 
				snprintf(tempString, sizeof(tempString), "bDeviceSubClass (%d,%d) ", dictDeviceSubClass->unsigned32BitValue(), deviceSubClass->unsigned32BitValue());
				strlcat(logString, tempString, sizeof(logString)); 
			}
			if ( deviceProtocolPropertyExists && !deviceProtocolPropertyMatches && dictProtocol) 
			{ 
				match = true; 
				snprintf(tempString, sizeof(tempString), "bDeviceProtocol (%d,%d) ", dictProtocol->unsigned32BitValue(), protocol->unsigned32BitValue());
				strlcat(logString, tempString, sizeof(logString)); 
			}
			if ( !match ) strlcat(logString, "nothing", sizeof(logString));
			
			USBLog(6, "%s", logString);
		}
    }
    if (deviceClassProp)
		deviceClassProp->release();
		
    return returnValue;
}



// Lowlevel requests for non-standard device requests
IOReturn 
IOUSBDevice::DeviceRequest(IOUSBDevRequest *request, IOUSBCompletion *completion)
{
    UInt16	theRequest;
    IOReturn	err;
    UInt16	wValue = request->wValue;
    
    if (isInactive())
    {
        USBLog(1, "%s[%p]::DeviceRequest - while terminating!", getName(), this);
		USBTrace( kUSBTDevice,  kTPDeviceDeviceRequest, (uintptr_t)this, kIOReturnNotResponding, request->wValue, 1 );
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
    
    if (isInactive())
    {
        USBLog(1, "%s[%p]::DeviceRequest - while terminating!",getName(),this);
		USBTrace( kUSBTDevice, kTPDeviceDeviceRequest, (uintptr_t)this, kIOReturnNotResponding, request->wValue, 2 );
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
 				USBLog(3, "%s[%p]:DeviceRequest kSetConfiguration to %d", getName(), this, wValue);
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
	
    USBLog(5, "%s[%p]::GetConfiguration", getName(), this);
	
    request.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice);
    request.bRequest = kUSBRqGetConfig;
    request.wValue = 0;
    request.wIndex = 0;
    request.wLength = sizeof(*configNumber);
    request.pData = configNumber;
	
    err = DeviceRequest(&request, 5000, 0);
	
    if (err)
    {
        USBLog(1,"%s[%p]: error getting config. err=0x%x", getName(), this, err);
		USBTrace( kUSBTDevice, kTPDeviceGetConfiguration, (uintptr_t)this, err, request.bmRequestType, 0);
    }
	
    return(err);
}



IOReturn 
IOUSBDevice::GetDeviceStatus(USBStatus *status)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevRequest	request;
	
    USBLog(5, "%s[%p]::GetDeviceStatus", getName(), this);
	
    request.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice);
    request.bRequest = kUSBRqGetStatus;
    request.wValue = 0;
    request.wIndex = 0;
    request.wLength = sizeof(*status);
    request.pData = status;
	
    err = DeviceRequest(&request, 5000, 0);
	
    if (err)
	{
        USBLog(1,"%s[%p]: error getting device status. err=0x%x", getName(), this, err);
		USBTrace( kUSBTDevice, kTPDeviceGetDeviceStatus, (uintptr_t)this, err, request.bmRequestType, 0);
	}
	
    return(err);
}



IOReturn 
IOUSBDevice::GetStringDescriptor(UInt8 index, char *utf8Buffer, int utf8BufferSize, UInt16 lang)
{
    IOReturn 		err;
    UInt8 		desc[256]; // Max possible descriptor length
    IOUSBDevRequest	request;
    int			i, len;
	
    // The buffer needs to be > 5 (One UTF8 character could be 4 bytes long, plus length byte)
    //
    if ( utf8BufferSize < 6 )
        return kIOReturnBadArgument;
	
    // Clear our buffer
    //
    bzero(utf8Buffer, utf8BufferSize);
    
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
        USBLog(5,"%s[%p]::GetStringDescriptor reading string length returned error (0x%x) - retrying with max length",getName(), this, err );
		
        // Let's try again full length.  Here's why:  On USB 2.0 controllers, we will not get an overrun error.  We just get a "babble" error
        // and no valid data.  So, if we ask for the max size, we will either get it, or we'll get an underrun.  It looks like we get it w/out an underrun
        //
        request.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice);
        request.bRequest = kUSBRqGetDescriptor;
        request.wValue = (kUSBStringDesc << 8) | index;
        request.wIndex = lang;
        request.wLength = 256;
        bzero(desc, 256);
        request.pData = &desc;
		
        err = DeviceRequest(&request, 5000, 0);
        if ( (err != kIOReturnSuccess) && (err != kIOReturnUnderrun) )
        {
            USBLog(3,"%s[%p]::GetStringDescriptor reading string length (256) returned error (0x%x)",getName(), this, err);
            return err;
        }
    }
	
    request.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice);
    request.bRequest = kUSBRqGetDescriptor;
    request.wValue = (kUSBStringDesc << 8) | index;
    request.wIndex = lang;
    len = desc[0];
    
    // If the length is 0 (empty string), just set the buffer to be 0.
    //
    if (len == 0)
    {
        USBLog(5, "%s[%p]::GetStringDescriptor (%d)  Length was zero", getName(), this, index);
        return kIOReturnSuccess;
    }
    
    // Make sure that desc[1] == kUSBStringDesc 
    //
    if ( desc[1] != kUSBStringDesc )
    {
        USBLog(3,"%s[%p]::GetStringDescriptor descriptor is not a string (%d н kUSBStringDesc)", getName(), this, desc[1] );
        return kIOReturnDeviceError;
    }
	
    if ( (desc[0] & 1) != 0)
    {
        // Odd length for the string descriptor!  That is odd.  Truncate it to an even #
        //
        USBLog(3,"%s[%p]::GetStringDescriptor descriptor length (%d) is odd, which is illegal", getName(), this, desc[0]);
        desc[0] &= 0xfe;
    }
    
    request.wLength = len;
    bzero(desc, len);
    request.pData = &desc;
	
    err = DeviceRequest(&request, 5000, 0);
	
    if (err != kIOReturnSuccess)
    {
        USBLog(3,"%s[%p]::GetStringDescriptor reading entire string returned error (0x%x)",getName(), this, err);
        return err;
    }
	
    // Make sure that desc[1] == kUSBStringDesc
    //
    if ( desc[1] != kUSBStringDesc )
    {
        USBLog(3,"%s[%p]::GetStringDescriptor descriptor is not a string (%d н kUSBStringDesc)", getName(), this, desc[1] );
        return kIOReturnDeviceError;
    }
	
    if ( (desc[0] & 1) != 0)
    {
        // Odd length for the string descriptor!  That is odd.  Truncate it to an even #
        //
        USBLog(3,"%s[%p]::GetStringDescriptor(2) descriptor length (%d) is odd, which is illegal", getName(), this, desc[0]);
        desc[0] &= 0xfe;
    }
	
    USBLog(5, "%s[%p]::GetStringDescriptor Got string descriptor %d, length %d, got %d", getName(), this,
           index, desc[0], request.wLength);
	
    // The string descriptor is in "Unicode".  We need to convert it to UTF-8.
    //
    SInt32              length = 0;
    UInt32              byteCounter = 0;
    SInt32              stringLength = desc[0] - 2;  // makes it neater
    UInt8 		utf8Bytes[4];
    char *		temp = utf8Buffer;
    UInt16		*uniCodeBytes = (UInt16	*)desc + 1;	// Just the Unicode words (i.e., no size byte or descriptor type byte)
    
    // Endian swap the Unicode bytes
    //
    SwapUniWords (&uniCodeBytes, stringLength);
    
    // Now pass each Unicode word (2 bytes) to the UTF-8 conversion routine
    //
    for (i = 0 ; i < stringLength / 2 ; i++)
    {
        // Convert the word
        //
        byteCounter = SimpleUnicodeToUTF8 (uniCodeBytes[i], utf8Bytes);	
        if (byteCounter == 0) 
            break;								// At the end
        
        //  Check to see if we have room in our buffer (leave room for NULL at the end)
        //
        length += byteCounter;			 
        if ( length > (utf8BufferSize - 1) )
            break;
        
        // Place the resulting byte(s) into our buffer and increment the buffer position
        //
        bcopy (utf8Bytes, temp, byteCounter);					// place resulting byte[s] in new buffer
        temp += byteCounter;							// inc buffer
    }
    
    return kIOReturnSuccess;
}



void
IOUSBDevice::DisplayNotEnoughPowerNotice()
{
    KUNCUserNotificationDisplayFromBundle(
                                          KUNCGetNotificationID(),
                                          (char *) "/System/Library/Extensions/IOUSBFamily.kext",
                                          (char *) "Dialogs",
                                          (char *) "plist",
                                          (char *) "Low Power Dialog",
                                          (char *) getName(),
                                          (KUNCUserNotificationCallBack) NULL,
                                          0);
    return;
}



void
IOUSBDevice::DisplayUserNotificationForDeviceEntry(OSObject *owner, IOTimerEventSource *sender)
{
#pragma unused (sender)
    IOUSBDevice *	me = OSDynamicCast(IOUSBDevice, owner);
	
    if (!me)
        return;
	
    me->retain();
    me->DisplayUserNotificationForDevice();
    me->release();
}



void
IOUSBDevice::DisplayUserNotificationForDevice ()
{
    kern_return_t	notificationError = kIOReturnSuccess;
    OSNumber *		locationIDProperty = NULL;
    UInt32			locationID = 0;
    OSObject *		propertyObj = NULL;
	
    // Get our locationID as an unsigned 32 bit number so we can
	propertyObj = copyProperty(kUSBDevicePropertyLocationID);
    locationIDProperty = OSDynamicCast( OSNumber, propertyObj );
    if ( locationIDProperty )
    {
        locationID = locationIDProperty->unsigned32BitValue();
    }
	if (propertyObj)
		propertyObj->release();
	
    // We will attempt to display the notification.  If we get an error, we will use a timer to fire the notification again
    // at some other point, until we don't get an error.
    //
    USBLog(3,"%s[%p]DisplayUserNotificationForDevice notificationType: %d",getName(), this, (uint32_t)_NOTIFICATION_TYPE );
	
#if 0
    switch ( _NOTIFICATION_TYPE )
    {
        case kUSBNotEnoughPowerNotificationType:
            IOLog("USB Notification:  The device \"%s\" cannot operate because there is not enough power available\n",getName());
            notificationError = KUNCUserNotificationDisplayFromBundle(
                                                                      KUNCGetNotificationID(),
                                                                      "/System/Library/Extensions/IOUSBFamily.kext",
                                                                      "Dialogs",
                                                                      "plist",
                                                                      "Low Power Dialog",
                                                                      (char *) (_descriptor.iProduct ? getName() : "Unnamed Device"),
                                                                      (KUNCUserNotificationCallBack) NULL,
                                                                      0);
            break;
			
        case kUSBIndividualOverCurrentNotificationType:
            IOLog("USB Notification:  The device \"%s\" has caused an overcurrent condition.  The port it is attached to has been disabled\n",getName());
            notificationError = KUNCUserNotificationDisplayFromBundle(
                                                                      KUNCGetNotificationID(),
                                                                      "/System/Library/Extensions/IOUSBFamily.kext",
                                                                      "Dialogs",
                                                                      "plist",
                                                                      "Overcurrent Dialog",
                                                                      (char *) (_descriptor.iProduct ? getName() : "Unnamed Device"),
                                                                      (KUNCUserNotificationCallBack) NULL,
                                                                      0);
            break;
			
        case kUSBGangOverCurrentNotificationType:
            IOLog("USB Notification:  The device \"%s\" has caused an overcurrent condition.  The hub it is attached to has been disabled\n",getName());
            notificationError = KUNCUserNotificationDisplayFromBundle(
                                                                      KUNCGetNotificationID(),
                                                                      "/System/Library/Extensions/IOUSBFamily.kext",
                                                                      "Dialogs",
                                                                      "plist",
                                                                      "Gang Overcurrent Dialog",
                                                                      (char *) (_descriptor.iProduct ? getName() : "Unnamed Device"),
                                                                      0,
                                                                      0);
            break;
    }
#else
	
    switch ( _NOTIFICATION_TYPE )
    {
        case kUSBNotEnoughPowerNotificationType:
            IOLog("USB Notification:  The device \"%s\" @ 0x%x cannot operate because there is not enough power available\n",getName(), (uint32_t) locationID);
            notificationError = KUNCUserNotificationDisplayNotice(
                                                                  0,		// Timeout in seconds
                                                                  0,		// Flags (for later usage)
                                                                  (char *) "",		// iconPath (not supported yet)
                                                                  (char *) "",		// soundPath (not supported yet)
                                                                  (char *) "/System/Library/Extensions/IOUSBFamily.kext",		// localizationPath
                                                                  (char *) "USB Low Power Header",		// the header
                                                                  (char *) "USB Low Power Notice",		// the notice - look in Localizable.strings
                                                                  (char *) "OK");

			if ( _expansionData )
			{
				if ( _USBPLANE_PARENT)
				{
				_USBPLANE_PARENT->retain();
				_USBPLANE_PARENT->messageClients( kIOUSBMessageNotEnoughPower, &locationID, sizeof(locationID));
				_USBPLANE_PARENT->release();
				}
				else
				{
					messageClients( kIOUSBMessageNotEnoughPower, &locationID, sizeof(locationID));
				}
			}
            break;
			
        case kUSBIndividualOverCurrentNotificationType:
            IOLog("USB Notification:  The device \"%s\" @ 0x%x has caused an overcurrent condition.  The port it is attached to has been disabled\n",getName(), (uint32_t) locationID);
            notificationError = KUNCUserNotificationDisplayNotice(
                                                                  0,		// Timeout in seconds
                                                                  0,		// Flags (for later usage)
                                                                  (char *) "",		// iconPath (not supported yet)
                                                                  (char *) "",		// soundPath (not supported yet)
                                                                  (char *) "/System/Library/Extensions/IOUSBFamily.kext",		// localizationPath
                                                                  (char *) "USB OverCurrent Header",					// the header
                                                                  (char *) "USB Individual OverCurrent Notice",				// the notice - look in Localizable.strings
                                                                  (char *) "OK");
			if ( _expansionData )
			{
				if ( _USBPLANE_PARENT)
				{
					_USBPLANE_PARENT->retain();
					_USBPLANE_PARENT->messageClients( kIOUSBMessageOvercurrentCondition, &locationID, sizeof(locationID));
					_USBPLANE_PARENT->release();
				}
				else
				{
					messageClients( kIOUSBMessageOvercurrentCondition, &locationID, sizeof(locationID));
				}
			}
			break;
			
        case kUSBGangOverCurrentNotificationType:
            IOLog("USB Notification:  The device \"%s\" @ 0x%x has caused an overcurrent condition.  The hub it is attached to has been disabled\n",getName(), (uint32_t)locationID);
            notificationError = KUNCUserNotificationDisplayNotice(
                                                                  0,		// Timeout in seconds
                                                                  0,		// Flags (for later usage)
                                                                  (char *) "",		// iconPath (not supported yet)
                                                                  (char *) "",		// soundPath (not supported yet)
                                                                  (char *) "/System/Library/Extensions/IOUSBFamily.kext",		// localizationPath
                                                                  (char *) "USB OverCurrent Header",					// the header
                                                                  (char *) "USB Individual OverCurrent Notice",				// the notice - look in Localizable.strings (Note that it is the same as for the Individual, because Pubs doesn't want to distinguish that)
                                                                  (char *) "OK");
			if ( _expansionData )
			{
				if ( _USBPLANE_PARENT)
				{
					_USBPLANE_PARENT->retain();
					_USBPLANE_PARENT->messageClients( kIOUSBMessageOvercurrentCondition, &locationID, sizeof(locationID));
					_USBPLANE_PARENT->release();
				}
				else
				{
					messageClients( kIOUSBMessageOvercurrentCondition, &locationID, sizeof(locationID));
				}
			}
			break;
    }
#endif
	
    // Check to see if we succeeded
    //
    if ( notificationError != kIOReturnSuccess )
    {
        // Bummer, we couldn't do it.  Set the time so that we try again later
        //
        USBLog(3,"%s[%p]DisplayUserNotificationForDevice returned error 0x%x", getName(), this, notificationError);
		
        _NOTIFIERHANDLER_TIMER->setTimeoutMS (kNotifyTimerDelay);	// No one logged in yet (except maybe root) reset the timer to fire later.
    }
	
    return;
}



void 
IOUSBDevice::SwapUniWords (UInt16  **unicodeString, UInt32 uniSize)
{
    UInt16	*wordPtr;
    
    // Start at the end of the buffer and work our way back, swapping every 2 bytes
    //
    wordPtr = &((*unicodeString)[uniSize/2-1]);  // uniSize is in bytes while the unicodeString is words (2-bytes).
    
    while (wordPtr >= *unicodeString)
    {
        // The Unicode String is in "USB" order (little endian), so swap it
        //
        *wordPtr = USBToHostWord( *wordPtr );
        wordPtr--;
    }
    
}



enum {
    kUTF8ByteMask = 0x3F,
    kUTF8ByteMark = 0x80
};



UInt32 
IOUSBDevice::SimpleUnicodeToUTF8(UInt16 uChar, UInt8 utf8Bytes[4])
{
    UInt32	bytesToWrite;
    UInt8		*curBytePtr;
    UInt32		utf8FirstByteMark;
    
    if (uChar == 0x0000) return 0;	//  Null word - no conversion?
    
    if  (uChar < 0x80)
    {
        bytesToWrite = 1;
        utf8FirstByteMark = 0x00;
    }
    else if (uChar < 0x800)
    {
        bytesToWrite = 2;
        utf8FirstByteMark = 0xC0;
    }
    else
    {
        bytesToWrite = 3;
        utf8FirstByteMark = 0xE0;
    }
    
    curBytePtr = utf8Bytes + bytesToWrite;
    switch (bytesToWrite)
    {
        // Note: we intentionally fall through from one case to the next
        case 3:
            *(--curBytePtr) = (uChar & kUTF8ByteMask) | kUTF8ByteMark;
            uChar >>= 6;
        case 2:
            *(--curBytePtr) = (uChar & kUTF8ByteMask) | kUTF8ByteMark;
            uChar >>= 6;
        case 1:
            *(--curBytePtr) = uChar | utf8FirstByteMark;
            break;
    }
    
    return bytesToWrite;
}


IOReturn			
IOUSBDevice::TakeGetConfigLock(void)
{
	if (!_WORKLOOP || !_COMMAND_GATE)
	{
		USBLog(1, "%s[%p]::TakeGetConfigLock - no WorkLoop or no gate!", getName(), this);
		USBTrace( kUSBTDevice, kTPDeviceConfigLock, (uintptr_t)this, (uintptr_t)_WORKLOOP, (uintptr_t)_COMMAND_GATE, 1 );
		return kIOReturnNotPermitted;
	}
	if (_WORKLOOP->onThread())
	{
		USBLog(1, "%s[%p]::TakeGetConfigLock - called onThread -- not allowed!", getName(), this);
		USBTrace( kUSBTDevice, kTPDeviceConfigLock, (uintptr_t)this, 0, 0, 2 );
		return kIOReturnNotPermitted;
	}
	USBLog(5, "%s[%p]::TakeGetConfigLock - calling through to ChangeGetConfigLock", getName(), this);
	return _COMMAND_GATE->runAction(ChangeGetConfigLock, (void*)true);
}



IOReturn			
IOUSBDevice::ReleaseGetConfigLock(void)
{
	if (!_WORKLOOP || !_COMMAND_GATE)
	{
		USBLog(1, "%s[%p]::TakeGetConfigLock - no WorkLoop or no gate!", getName(), this);
		USBTrace( kUSBTDevice, kTPDeviceConfigLock, (uintptr_t)this, (uintptr_t)_WORKLOOP, (uintptr_t)_COMMAND_GATE, 3 );
		return kIOReturnNotPermitted;
	}
	USBLog(5, "%s[%p]::ReleaseGetConfigLock - calling through to ChangeGetConfigLock", getName(), this);
	return _COMMAND_GATE->runAction(ChangeGetConfigLock, (void*)false);
}


IOReturn		
IOUSBDevice::ChangeGetConfigLock(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
#pragma unused ( param2, param3, param4)
    IOUSBDevice		*me = OSDynamicCast(IOUSBDevice, target);
    bool			takeLock = (bool)param1;
	IOReturn		retVal = kIOReturnSuccess;
	
	if (takeLock)
	{
		while (me->_GETCONFIGLOCK and (retVal == kIOReturnSuccess))
		{
			AbsoluteTime	deadline;
			IOReturn		kr;
			
			clock_interval_to_deadline(kGetConfigDeadlineInSecs, NSEC_PER_SEC, &deadline);
			USBLog(5, "%s[%p]::ChangeGetConfigLock - _GETCONFIGLOCK held by someone else - calling commandSleep to wait for lock", me->getName(), me);
			USBTrace( kUSBTDevice, kTPDeviceConfigLock, (uintptr_t)me, 0, 0, 4 );
			kr = me->_COMMAND_GATE->commandSleep(&me->_GETCONFIGLOCK, deadline, THREAD_ABORTSAFE);
			switch (kr)
			{
				case THREAD_AWAKENED:
					USBLog(6,"%s[%p]::ChangeGetConfigLock commandSleep woke up normally (THREAD_AWAKENED) _GETCONFIGLOCK(%s)", me->getName(), me, me->_GETCONFIGLOCK ? "true" : "false");
					USBTrace( kUSBTDevice, kTPDeviceConfigLock, (uintptr_t)me, (uintptr_t)me->_GETCONFIGLOCK, 0, 5 );
					break;
					
				case THREAD_TIMED_OUT:
					USBLog(3,"%s[%p]::ChangeGetConfigLock commandSleep timeout out (THREAD_TIMED_OUT) _GETCONFIGLOCK(%s)", me->getName(), me, me->_GETCONFIGLOCK ? "true" : "false");
					USBTrace( kUSBTDevice, kTPDeviceConfigLock, (uintptr_t)me, (uintptr_t)me->_GETCONFIGLOCK, 0, 6 );
					retVal = kIOReturnNotPermitted;
					break;
					
				case THREAD_INTERRUPTED:
					USBLog(3,"%s[%p]::ChangeGetConfigLock commandSleep interrupted (THREAD_INTERRUPTED) _GETCONFIGLOCK(%s)", me->getName(), me, me->_GETCONFIGLOCK ? "true" : "false");
					USBTrace( kUSBTDevice, kTPDeviceConfigLock, (uintptr_t)me, (uintptr_t)me->_GETCONFIGLOCK, 0, 7 );
					retVal = kIOReturnNotPermitted;
					break;
					
				case THREAD_RESTART:
					USBLog(3,"%s[%p]::ChangeGetConfigLock commandSleep restarted (THREAD_RESTART) _GETCONFIGLOCK(%s)", me->getName(), me, me->_GETCONFIGLOCK ? "true" : "false");
					USBTrace( kUSBTDevice, kTPDeviceConfigLock, (uintptr_t)me, (uintptr_t)me->_GETCONFIGLOCK, 0, 8 );
					retVal = kIOReturnNotPermitted;
					break;
					
				case kIOReturnNotPermitted:
					USBLog(3,"%s[%p]::ChangeGetConfigLock woke up with status (kIOReturnNotPermitted) - we do not hold the WL!", me->getName(), me);
					USBTrace( kUSBTDevice, kTPDeviceConfigLock, (uintptr_t)me, 0, 0, 9 );
					retVal = kr;
					break;
					
				default:
					USBLog(3,"%s[%p]::ChangeGetConfigLock woke up with unknown status %p",  me->getName(), me, (void*)kr);
					USBTrace( kUSBTDevice, kTPDeviceConfigLock, (uintptr_t)me, 0, 0, 10 );
					retVal = kIOReturnNotPermitted;
			}
		}
		if (retVal == kIOReturnSuccess)
		{
			USBLog(5, "%s[%p]::ChangeGetConfigLock - setting _GETCONFIGLOCK to true", me->getName(), me);
			USBTrace( kUSBTDevice, kTPDeviceConfigLock, (uintptr_t)me, 0, 0, 13 );
			me->_GETCONFIGLOCK = true;
		}
	}
	else
	{
		USBLog(5, "%s[%p]::ChangeGetConfigLock - setting _GETCONFIGLOCK to false and calling commandWakeup", me->getName(), me);
		USBTrace( kUSBTDevice, kTPDeviceConfigLock, (uintptr_t)me, 0, 0, 11 );
		me->_GETCONFIGLOCK = false;
		me->_COMMAND_GATE->commandWakeup(&me->_GETCONFIGLOCK, true);
	}
	return retVal;
}



IOReturn 
IOUSBDevice::DeviceRequest(IOUSBDevRequest *request, UInt32 noDataTimeout, UInt32 completionTimeout, IOUSBCompletion *completion)
{
    UInt16	theRequest;
    IOReturn	err;
    UInt16	wValue = request->wValue;
    
    if (isInactive())
    {
        USBLog(1, "%s[%p]::DeviceRequest - while terminating!", getName(), this);
		USBTrace( kUSBTDevice, kTPDeviceDeviceRequest, (uintptr_t)this, kIOReturnNotResponding, request->wValue, 3 );
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
				USBLog(6, "%s[%p]:DeviceRequest kSetConfiguration to %d", getName(), this, wValue);
               _currentConfigValue = wValue;
            }
        }
        return err;
    }
    else
        return kIOUSBUnknownPipeErr;
}



IOReturn 
IOUSBDevice::DeviceRequest(IOUSBDevRequestDesc *request, UInt32 noDataTimeout, UInt32 completionTimeout, IOUSBCompletion *completion)
{
    UInt16	theRequest;
    IOReturn	err;
    UInt16	wValue = request->wValue;
    
    if (isInactive())
    {
        USBLog(1, "%s[%p]::DeviceRequest - while terminating!", getName(), this);
		USBTrace( kUSBTDevice, kTPDeviceDeviceRequest, (uintptr_t)this, kIOReturnNotResponding, request->wValue, 4 );
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
				USBLog(6, "%s[%p]:DeviceRequest kSetConfiguration to %d", getName(), this, wValue);
               _currentConfigValue = wValue;
            }
        }
        return err;
    }
    else
        return kIOUSBUnknownPipeErr;
}



//=============================================================================================
//
//  SuspendDevice
//
//
//=============================================================================================
//
IOReturn
IOUSBDevice::SuspendDevice( bool suspend )
{
	IOReturn	status		= kIOReturnSuccess;

	if ( _WORKLOOP->onThread() )
	{
		USBLog(1, "%s[%p]::SuspendDevice(port %d) - called on workloop thread !", getName(), this, (uint32_t)_PORT_NUMBER);
		USBTrace( kUSBTDevice,  kTPDeviceSuspendDevice, (uintptr_t)this, _PORT_NUMBER, 0, 1);
        return kIOUSBSyncRequestOnWLThread;
	}
	
    if ( _RESET_IN_PROGRESS )
    {
        USBLog(3, "%s[%p]::SuspendDevice(port %d) while in ResetDevice or ReEnumerateDevice in progress. Returning kIOReturnNotPermitted.", getName(), this, (uint32_t)_PORT_NUMBER );
		USBTrace( kUSBTDevice,  kTPDeviceSuspendDevice, (uintptr_t)this, _PORT_NUMBER, 0, 2);
        return kIOReturnNotPermitted;
    }
	
    if ( isInactive() )
    {
        USBLog(1, "%s[%p]::SuspendDevice - while inactive!", getName(), this);
		USBTrace( kUSBTDevice,  kTPDeviceSuspendDevice, (uintptr_t)this, _PORT_NUMBER, 0, 3);
        return kIOReturnNoDevice;
    }
	
	USBLog(5, "+%s[%p]::SuspendDevice(%s) for port %d", getName(), this, suspend ? "suspend" : "resume", (uint32_t)_PORT_NUMBER );
	USBTrace( kUSBTDevice,  kTPDeviceSuspendDevice, (uintptr_t)this, _PORT_NUMBER, suspend, 4);

	// zzz Remove after <rdar://problem/6553617> AppleUSBBluetoothHCIController ... is submitted
	if ( suspend )
		status = _USBPLANE_PARENT->messageClients(kIOUSBMessageHubSuspendPort, &_PORT_NUMBER, sizeof(_PORT_NUMBER));

	if ( _HUBPARENT )
	{
		status = _HUBPARENT->SuspendPort( _PORT_NUMBER, suspend );
	}

	UInt32 messageToSend = 0;
	
	if ( suspend )
	{
		messageToSend = ( status == kIOReturnSuccess) ? kIOUSBMessagePortHasBeenSuspended : kIOUSBMessagePortWasNotSuspended ;
	}
	else
	{
		 if ( status != kIOReturnSuccess) 
			 messageToSend = kIOUSBMessagePortWasNotSuspended ;
		
		status = kIOReturnSuccess;
	}
	
	if ( messageToSend )
	{   
		// This should be freed in the thread after messageClients has returned
		IOUSBDeviceMessage *	messageStructPtr = (IOUSBDeviceMessage *) IOMalloc( sizeof(IOUSBDeviceMessage));
		
		messageStructPtr->type	= messageToSend;
		messageStructPtr->error = 0;
		
		USBTrace( kUSBTDevice,  kTPDeviceSuspendDevice, (uintptr_t)this, _PORT_NUMBER, messageToSend, 6);
		retain();
		if ( thread_call_enter1( _DO_MESSAGE_CLIENTS_THREAD, (thread_call_param_t) messageStructPtr ) == TRUE )
		{
			USBLog(3,"%s[%p]: SuspendDevice _DO_MESSAGE_CLIENTS_THREAD already queued ",getName(),this);
			USBTrace( kUSBTDevice,  kTPDeviceSuspendDevice, (uintptr_t)this, _PORT_NUMBER, 0, 7);
			release();
		}
	}
	
	USBLog(5, "-%s[%p]::SuspendDevice for port %d with error 0x%x ", getName(), this, (uint32_t)_PORT_NUMBER, status );
	USBTrace( kUSBTDevice,  kTPDeviceSuspendDevice, (uintptr_t)this, _PORT_NUMBER, status, 5);

    return status;
}



IOReturn
IOUSBDevice::ReEnumerateDevice( UInt32 options )
{
	IOReturn	kr = kIOReturnSuccess;
	bool		gotLock;
	
    if (isInactive())
    {
        USBLog(1, "%s[%p]::ReEnumerateDevice - while terminating!", getName(), this);
		USBTrace( kUSBTDevice, kTPDeviceReEnumerateDevice, (uintptr_t)this, _PORT_NUMBER, 0, 1);
        return kIOReturnNoDevice;
    }
	
	gotLock = OSCompareAndSwap(0, 1, &_RESET_AND_REENUMERATE_LOCK);
	if ( !gotLock )
	{
        USBLog(5, "%s[%p]::ReEnumerateDevice( port %d) while ResetDevice or ReEnumerateDevice in progress.  Returning kIOReturnNotPermitted.", getName(), this, (uint32_t)_PORT_NUMBER );
		USBTrace( kUSBTDevice, kTPDeviceReEnumerateDevice, (uintptr_t)this, _PORT_NUMBER, 0, 2);
		return kIOReturnNotPermitted;
	}
	
    // If we have the _ADD_EXTRA_RESET_TIME, set bit 31 of the options
    //
    if ( _ADD_EXTRA_RESET_TIME )
    {
        USBLog(1, "%s[%p]::ReEnumerateDevice - setting extra reset time options!", getName(), this);
		USBTrace( kUSBTDevice, kTPDeviceReEnumerateDevice, (uintptr_t)this, _PORT_NUMBER, options, 3 );
        options |= kUSBAddExtraResetTimeMask;
    }

    // Since we are going to re-enumerate the device, all drivers and interfaces will be
    // terminated, so we don't need to make this device synchronous.  In fact, we want it
    // async, because this device will go away. We do clear our lock when we finish processing the re-enumeration in the callout thread.
    //
    USBLog(5, "%s[%p]::ReEnumerateDevice for port %d, options 0x%x", getName(), this, (uint32_t)_PORT_NUMBER, (uint32_t)options );
	USBTrace( kUSBTDevice, kTPDeviceReEnumerateDevice, (uintptr_t)this, _PORT_NUMBER, 0, 5 );
    retain();
    if ( thread_call_enter1( _DO_PORT_REENUMERATE_THREAD, (thread_call_param_t) options) == TRUE )
	{
		USBLog(3, "%s[%p]::ReEnumerateDevice for port %d, _DO_PORT_REENUMERATE_THREAD already queued", getName(), this, (uint32_t)_PORT_NUMBER );
		USBTrace( kUSBTDevice, kTPDeviceReEnumerateDevice, (uintptr_t)this, _PORT_NUMBER, 0, 4);
		release();
		kr = kIOReturnBusy;
	}

	USBLog(5, "%s[%p]::ReEnumerateDevice for port %d, returning 0x%x", getName(), this, (uint32_t)_PORT_NUMBER, (uint32_t)kr );
	USBTrace( kUSBTDevice, kTPDeviceReEnumerateDevice, (uintptr_t)this, _PORT_NUMBER, kr, 6 );
	
	return kr;
}

void
IOUSBDevice::DisplayUserNotification(UInt32 notificationType )
{
    // NOTE:  If we get multiple calls before we actually display the notification (i.e. this gets called at boot)
    //        we will only display the last notification.
    //
    USBLog(3, "%s[%p] DisplayUserNotification type %d", getName(), this, (uint32_t)notificationType );
    _NOTIFICATION_TYPE = notificationType;
    
    DisplayUserNotificationForDeviceEntry(this, NULL );
}

void
IOUSBDevice::SetHubParent(IOUSBHubPolicyMaker *hubParent)
{
    USBLog(6, "%s[%p]::SetHubParent  set to %p(%s)", getName(), this, hubParent, hubParent->getName() );
	
	_HUBPARENT = hubParent;
	
	// Retain it iand release when are stop'd
	_HUBPARENT->retain();
}


IOUSBHubPolicyMaker *
IOUSBDevice::GetHubParent()
{
    USBLog(6, "%s[%p]::GetHubParent  returning %p", getName(), this, _HUBPARENT);
	return (IOUSBHubPolicyMaker*) _HUBPARENT;
}


IOReturn
IOUSBDevice::GetDeviceInformation(UInt32 *info)
{
	IOReturn	kr = kIOReturnNoDevice;
	OSObject *	prop = NULL;
	
	USBLog(6, "%s[%p]::GetDeviceInformation  Hub parent: %p", getName(), this, _HUBPARENT);

	if ( _HUBPARENT )
		kr = _HUBPARENT->GetPortInformation(_PORT_NUMBER, info);
	else {
		*info = 0;
		goto ErrorExit;
	}

	
	// Allow for setting the "non-removable" property for cases where the hub doesn't have it right
	prop = getProperty("non-removable");
	if ( prop )
	{
		// We assume that if the property is there, then it is a non-removable
		USBLog(6, "%s[%p]::GetDeviceInformation,  non-removable property exists, setting kUSBInformationDeviceIsCaptiveBit", getName(), this);
		*info |= ( 1 << kUSBInformationDeviceIsCaptiveBit);
	}
	
	// Determine if our device is internal:  We need to get our USB plane parent and do a GetDeviceInformation on it.  If it is captive AND is 
	// a root hub device, then we set the internal bit.
	
	if ( !_DEVICEISINTERNALISVALID )
	{
		_DEVICEISINTERNALISVALID = true;

		// If we are not captive, then we are not internal
		if ( *info & (1 << kUSBInformationDeviceIsCaptiveBit) )
		{
			if ( _USBPLANE_PARENT )
			{
				UInt32		parentInfo = 0;
				IOReturn	err;
				
				_USBPLANE_PARENT->retain();
				USBLog(5, "%s[%p]::GetDeviceInformation  Hub device name is %s at USB address %d", getName(), this, _USBPLANE_PARENT->getName(), _USBPLANE_PARENT->GetAddress());
				
				err = _USBPLANE_PARENT->GetDeviceInformation( &parentInfo);
				if ( err == kIOReturnSuccess )
				{
					if ( parentInfo & ( 1 << kUSBInformationDeviceIsInternalBit ) )
					{
						USBLog(6, "%s[%p]::GetDeviceInformation  USB parent is internal", getName(), this);
						_DEVICEISINTERNAL = true;
					}
				}
				_USBPLANE_PARENT->release();
			}
		}
	}
	
	if ( _DEVICEISINTERNAL )
		*info |= ( 1 << kUSBInformationDeviceIsInternalBit);

ErrorExit:
	USBLog(6, "%s[%p]::GetDeviceInformation, error: 0x%x, info: 0x%x", getName(), this, kr, (uint32_t) *info);
	
	return kr;
}


UInt32
IOUSBDevice::RequestExtraPower(UInt32 type, UInt32 requestedPower)
{
	UInt32	returnValue = 0;
	
	USBLog(6, "%s[%p]::RequestExtraPower type %d, requested %d", getName(), this, (uint32_t) type, (uint32_t) requestedPower);

	if ( _HUBPARENT )
		returnValue = _HUBPARENT->RequestExtraPower(_PORT_NUMBER, type, requestedPower);
	else
	{
		USBLog(6, "%s[%p]::RequestExtraPower  no _HUBPARENT", getName(), this);
	}

	if ( type == kUSBPowerDuringSleep )
	{
		_SLEEPPOWERALLOCATED += (UInt32) returnValue;
		setProperty("PortUsingExtraPowerForSleep", _SLEEPPOWERALLOCATED, 32);
	}
	
	if ( type == kUSBPowerDuringWake )
	{
		_WAKEPOWERALLOCATED += (UInt32) returnValue;
		setProperty("PortUsingExtraPowerForWake", _WAKEPOWERALLOCATED, 32);
	}
	
	USBLog(6, "%s[%p]::RequestExtraPower type %d, returning %d", getName(), this, (uint32_t) type, (uint32_t) returnValue);
	
	return returnValue;
}



IOReturn
IOUSBDevice::ReturnExtraPower(UInt32 type, UInt32 returnedPower)
{
	IOReturn	kr = kIOReturnSuccess;
	
	USBLog(6, "%s[%p]::ReturnExtraPower type %d, returnedPower %d", getName(), this, (uint32_t) type, (uint32_t) returnedPower);
	
	// Make sure that we are not returning more than we had requested
	if ( type == kUSBPowerDuringSleep )
	{
		if ( returnedPower > _SLEEPPOWERALLOCATED )
		{
			USBLog(6, "%s[%p]::ReturnExtraPower type %d, returnedPower %d was more than what we had previously allocated (%d)", getName(), this, (uint32_t) type, (uint32_t) returnedPower,  (uint32_t) _SLEEPPOWERALLOCATED);
			return kIOReturnBadArgument;
		}
	}

	if ( type == kUSBPowerDuringWake )
	{
		if ( returnedPower > _WAKEPOWERALLOCATED )
		{
			USBLog(6, "%s[%p]::ReturnExtraPower type %d, returnedPower %d was more than what we had previously allocated (%d)", getName(), this, (uint32_t) type, (uint32_t) returnedPower,  (uint32_t) _WAKEPOWERALLOCATED);
			return kIOReturnBadArgument;
		}
	}
	
	// Now call out ot actually return the power
	if ( _HUBPARENT )
		kr = _HUBPARENT->ReturnExtraPower(_PORT_NUMBER, type, returnedPower);
	
	if (kr == kIOReturnSuccess )
	{
		// Keep track of what we have allocated
		if ( type == kUSBPowerDuringSleep )
		{
			_SLEEPPOWERALLOCATED -= returnedPower;
			setProperty("PortUsingExtraPowerForWake", _SLEEPPOWERALLOCATED, 32);
		}
		
		if ( type == kUSBPowerDuringWake )
		{
			_WAKEPOWERALLOCATED -= returnedPower;
			setProperty("PortUsingExtraPowerForWake", _WAKEPOWERALLOCATED, 32);
		}
	}
	else
	{
		USBLog(3, "%s[%p]::ReturnExtraPower  _HUBPARENT->ReturnExtraPower returned 0x%x", getName(), this, kr);
	}
	return kr;
}

UInt32
IOUSBDevice::GetExtraPowerAllocated(UInt32 type)
{
	UInt32		returnValue = 0;
	
	// Keep track of what we have allocated
	if ( type == kUSBPowerDuringSleep )
		returnValue =  _SLEEPPOWERALLOCATED;
	else
	if ( type == kUSBPowerDuringWake )
		returnValue =  _WAKEPOWERALLOCATED;
	
	return returnValue;
}

bool
IOUSBDevice::DoLocationOverrideAndModelMatch()
{	
	enum
	{
		kMaxMacModelStringLength		=	14
	};

	bool		returnValue = false;
	bool		overrideMatches = false;
	OSObject *	anObject = NULL;
	OSNumber *	osNumberRef = NULL;
	OSString *	osStringRef = NULL;
	OSArray *	anArrayRef = NULL;
	
	char		macModel [ kMaxMacModelStringLength ];
	unsigned int	index;
	
	USBLog(6, "%s[%p]::DoLocationOverrideAndModelMatch (_locationID: 0x%x)", getName(), this, (uint32_t)_LOCATIONID);

	// First, look for the "OverrideAtLocationID" array that will contain the locationIDs where we
	// should apple the override
	
	anObject = copyProperty(kOverrideIfAtLocationID);
	anArrayRef	= OSDynamicCast(OSArray, anObject);
	if (anArrayRef)
	{
		USBLog(6, "%s[%p]::DoLocationOverrideAndModelMatch - found kOverrideIfAtLocationID array with capacity of %d", getName(), this, anArrayRef->getCount());
		
		for (index = 0; index < anArrayRef->getCount(); index++)
		{
			// See if we can get the object at our configIndex
			
			osNumberRef = OSDynamicCast(OSNumber, anArrayRef->getObject(index));
			if (osNumberRef)
			{
				USBLog(6, "%s[%p]::DoLocationOverrideAndModelMatch - found kOverrideIfAtLocationID[%d] with value: 0x%x", getName(), this, index, osNumberRef->unsigned32BitValue());
				if ( osNumberRef->unsigned32BitValue() == _LOCATIONID )
				{
					USBLog(6, "%s[%p]::DoLocationOverrideAndModelMatch - override locationID did match", getName(), this);
					overrideMatches = true;
					break;
				}
			}
		}
	}
	else
	{
		// We don't have the property "OverrideAtLocationID", so always return a match
		returnValue = true;
	}
	
	if (anObject)
	{
		anObject->release();
		anObject = NULL;
	}
	
	// Now, if the override matches, look for the MacModel property
	if (overrideMatches)
	{
		// What MacModel are we running on?
		getPlatform()->getModelName ( macModel, kMaxMacModelStringLength ); 
		
		USBLog(6, "%s[%p]::DoLocationOverrideAndModelMatch - machine Model is: %s", getName(), this, macModel);
		
		// Look for our MacModel property in the array of supported models
		anObject = copyProperty("MacModel");
		anArrayRef	= OSDynamicCast(OSArray, anObject);
		if (anArrayRef)
		{
			USBLog(6, "%s[%p]::DoLocationOverrideAndModelMatch - found MacModel array with capacity of %d", getName(), this, anArrayRef->getCount());
			
			for (index = 0; index < anArrayRef->getCount(); index++)
			{
				// Look at the models
				osStringRef = OSDynamicCast(OSString, anArrayRef->getObject(index));
				if ( osStringRef )
				{
					// Compate to the MacModel property
					if ( osStringRef->isEqualTo(macModel))
					{
						USBLog(6, "%s[%p]::DoLocationOverrideAndModelMatch - our model property matched, index: %d", getName(), this, index);
						returnValue = true;
						break;
					}
				}
			}
		}
						
		if (anObject)
		{
			anObject->release();
			anObject = NULL;
		}
	}
	
	USBLog(6, "%s[%p]::DoLocationOverrideAndModelMatch returning %s", getName(), this, returnValue ? "TRUE" : "FALSE");
	return returnValue;
}

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
	
    me->retain();
    me->ProcessPortReset();
    me->release();
}



void 
IOUSBDevice::ProcessPortReset()
{
    IOReturn			err = kIOReturnSuccess;
	
    USBLog(6,"+%s[%p]::ProcessPortReset, _PORT_RESET_THREAD_ACTIVE = %d, isInactive() = %d",getName(), this, _PORT_RESET_THREAD_ACTIVE, isInactive() ); 
	
	// if we are already resetting the port, then just return ( Perhaps we should do this atomically)?
	//
	if ( _PORT_RESET_THREAD_ACTIVE || isInactive() )
		return;
	else
	    _PORT_RESET_THREAD_ACTIVE = true;
	
    if ( _pipeZero) 
    {
        _pipeZero->Abort();
		_pipeZero->ClosePipe();
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
    if ( _USBPLANE_PARENT )
    {
        USBLog(5, "%s[%p]::ProcessPortReset calling messageClients (kIOUSBMessageHubResetPort)", getName(), this);
		
        _USBPLANE_PARENT->retain();
        err = _USBPLANE_PARENT->messageClients(kIOUSBMessageHubResetPort, &_PORT_NUMBER, sizeof(_PORT_NUMBER));
        _USBPLANE_PARENT->release();
    }
    
	_PORT_RESET_THREAD_ACTIVE = false;
	
    USBLog(6,"-%s[%p]::ProcessPortReset (_pipeZero %p)",getName(), this, _pipeZero); 
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
	
    me->ProcessPortReEnumerate( (uintptr_t) options);
    me->release();
}



void 
IOUSBDevice::ProcessPortReEnumerate(UInt32 options)
{
    IOReturn				err = kIOReturnSuccess;
    IOUSBHubPortReEnumerateParam	params;
	bool					gotLock;
    
    USBLog(5,"+%s[%p]::ProcessPortReEnumerate",getName(),this); 
	
    // Set the parameters for our message call
    //
    params.portNumber = _PORT_NUMBER;
    params.options = options;
    
    if ( _pipeZero) 
    {
        _pipeZero->Abort();
		_pipeZero->ClosePipe();
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
    if ( _expansionData && _USBPLANE_PARENT )
    {
        USBLog(3, "%s[%p]::ProcessPortReEnumerate calling messageClients (kIOUSBMessageHubReEnumeratePort)", getName(), this);
        _USBPLANE_PARENT->retain();
        err = _USBPLANE_PARENT->messageClients(kIOUSBMessageHubReEnumeratePort, &params, sizeof(IOUSBHubPortReEnumerateParam));
        _USBPLANE_PARENT->release();
    }
    
	
	gotLock = OSCompareAndSwap(1, 0, &_RESET_AND_REENUMERATE_LOCK);
	if ( !gotLock )
	{
        USBLog(1, "%s[%p]::ProcessPortReEnumerate( port %d) our resetLock was not set.  Unexpected", getName(), this, (uint32_t)_PORT_NUMBER );
	}
	
    USBLog(5,"-%s[%p]::ProcessPortReEnumerate",getName(),this); 
}

void 
IOUSBDevice::DoMessageClientsEntry(OSObject *target, thread_call_param_t messageStructPtr)
{
    IOUSBDevice *	me = OSDynamicCast(IOUSBDevice, target);
    
    if (!me)
        return;
	
    me->DoMessageClients(messageStructPtr);
	
	// Free the struct that was allocated in calling this
	IOFree(messageStructPtr, sizeof(IOUSBDeviceMessage));
		   
    me->release();
}



void 
IOUSBDevice::DoMessageClients(void * messageStruct)
{
	UInt32		type = ( (IOUSBDeviceMessage *)messageStruct)->type;
	IOReturn	error = ( (IOUSBDeviceMessage *)messageStruct)->error;
	
    USBLog(7,"+%s[%p]::DoMessageClients:  type = 0x%x, error = 0x%x",getName(),this, (uint32_t)type, error); 
	messageClients( type, &error, sizeof(IOReturn));
	USBLog(7,"-%s[%p]::DoMessageClients",getName(),this); 
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

void
IOUSBDevice::SetBusPowerAvailable(UInt32 newPower)
{
	_busPowerAvailable = newPower;
	setProperty(kUSBDevicePropertyBusPowerAvailable, (unsigned long long)_busPowerAvailable, (sizeof(_busPowerAvailable) * 8));
	USBLog(2, "IOUSBDevice[%p]::SetBusPowerAvailable - power now(%d)", this, (int)_busPowerAvailable);
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

UInt16  
IOUSBDevice::GetbcdUSB(void)
{ 
    return USBToHostWord(_descriptor.bcdUSB); 
}

UInt8  
IOUSBDevice::GetNumConfigurations(void)
{ 
    return _descriptor.bNumConfigurations; 
}

UInt8  
IOUSBDevice::GetProtocol(void)
{ 
    return _descriptor.bDeviceProtocol; 
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
#pragma unused (ep)
	// this is a deprecated KPI
    return NULL; 
}

IOUSBPipe * 
IOUSBDevice::MakePipe(const IOUSBEndpointDescriptor *ep, IOUSBInterface * interface) 
{
    return IOUSBPipe::ToEndpoint(ep, this, _controller, interface); 
}

OSMetaClassDefineReservedUsed(IOUSBDevice,  0);
OSMetaClassDefineReservedUsed(IOUSBDevice,  1);
OSMetaClassDefineReservedUsed(IOUSBDevice,  2);
OSMetaClassDefineReservedUsed(IOUSBDevice,  3);
OSMetaClassDefineReservedUsed(IOUSBDevice,  4);
OSMetaClassDefineReservedUsed(IOUSBDevice,  5);
OSMetaClassDefineReservedUsed(IOUSBDevice,  6);
OSMetaClassDefineReservedUsed(IOUSBDevice,  7);
OSMetaClassDefineReservedUsed(IOUSBDevice,  8);
OSMetaClassDefineReservedUsed(IOUSBDevice,  9);
OSMetaClassDefineReservedUsed(IOUSBDevice,  10);
OSMetaClassDefineReservedUsed(IOUSBDevice,  11);
OSMetaClassDefineReservedUsed(IOUSBDevice,  12);

OSMetaClassDefineReservedUnused(IOUSBDevice,  13);
OSMetaClassDefineReservedUnused(IOUSBDevice,  14);
OSMetaClassDefineReservedUnused(IOUSBDevice,  15);
OSMetaClassDefineReservedUnused(IOUSBDevice,  16);
OSMetaClassDefineReservedUnused(IOUSBDevice,  17);
OSMetaClassDefineReservedUnused(IOUSBDevice,  18);
OSMetaClassDefineReservedUnused(IOUSBDevice,  19);



