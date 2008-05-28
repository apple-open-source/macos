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
extern "C" {
#include <kern/thread_call.h>
}
#include <libkern/OSByteOrder.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSData.h>

#include <IOKit/IOKitKeys.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/usb/IOUSBControllerV3.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBLog.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/IOUSBRootHubDevice.h>
#include <IOKit/usb/USB.h>

#include <UserNotification/KUNCUserNotifications.h>


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
#define _DO_PORT_SUSPEND_THREAD			_expansionData->_doPortSuspendThread
#define _PORT_SUSPEND_THREAD_ACTIVE		_expansionData->_portSuspendThreadActive
#define _DO_PORT_REENUMERATE_THREAD		_expansionData->_doPortReEnumerateThread
#define _RESET_IN_PROGRESS				_expansionData->_resetInProgress
#define _PORT_HAS_BEEN_RESET			_expansionData->_portHasBeenReset
#define _GET_CONFIG_LOCK				_expansionData->_getConfigLock
#define _WORKLOOP						_expansionData->_workLoop
#define _NOTIFIERHANDLER_TIMER			_expansionData->_notifierHandlerTimer
#define _NOTIFICATION_TYPE				_expansionData->_notificationType
#define _SUSPEND_IN_PROGRESS			_expansionData->_suspendInProgress
#define _PORT_HAS_BEEN_SUSPENDED_OR_RESUMED		_expansionData->_portHasBeenSuspendedOrResumed
#define _ADD_EXTRA_RESET_TIME			_expansionData->_addExtraResetTime
#define _COMMAND_GATE					_expansionData->_commandGate
#define _SUSPEND_COMMAND				_expansionData->_suspendCommand
#define _OPEN_INTERFACES				_expansionData->_openInterfaces
#define _RESET_COMMAND					_expansionData->_resetCommand
#define _RESET_ERROR					_expansionData->_resetError
#define _SUSPEND_ERROR					_expansionData->_suspendError
#define _DO_MESSAGE_CLIENTS_THREAD		_expansionData->_doMessageClientsThread

#define kNotifyTimerDelay		30000	// in milliseconds = 30 seconds
#define kUserLoginDelay			20000	// in milliseconds = 20 seconds
#define kMaxTimeToWaitForReset	   10000   // in milliseconds = 10 seconds
#define kMaxTimeToWaitForSuspend   20000   // in milliseconds = 20 seconds

typedef struct IOUSBDeviceMessage {
    UInt32			type;
    IOReturn		error;
} IOUSBDeviceMessage;


/* Convert USBLog to use kprintf debugging */
#define IOUSBDEVICE_USE_KPRINTF 0

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
    AbsoluteTime	currentTime;
    UInt64			elapsedTime;
    OSObject *		propertyObj = NULL;
	OSBoolean *		boolObj = NULL;
	
    if( !super::start(provider))
        return false;
	
	_controller = OSDynamicCast(IOUSBController, provider);
	
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
    _DO_PORT_RESET_THREAD = thread_call_allocate((thread_call_func_t)ProcessPortResetEntry, (thread_call_param_t)this);
    _DO_PORT_SUSPEND_THREAD = thread_call_allocate((thread_call_func_t)ProcessPortSuspendEntry, (thread_call_param_t)this);
    _DO_PORT_REENUMERATE_THREAD = thread_call_allocate((thread_call_func_t)ProcessPortReEnumerateEntry, (thread_call_param_t)this);
	_DO_MESSAGE_CLIENTS_THREAD = thread_call_allocate((thread_call_func_t)DoMessageClientsEntry, (thread_call_param_t)this);
	
	if ( !_DO_PORT_RESET_THREAD || !_DO_PORT_SUSPEND_THREAD || !_DO_PORT_REENUMERATE_THREAD || !_DO_MESSAGE_CLIENTS_THREAD )
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
	
    _WORKLOOP = getWorkLoop();
    if ( !_WORKLOOP )
    {
        USBError(1, "%s[%p]::start Couldn't get provider's workloop", getName(), this);
        goto ErrorExit;
    }
	
    // Keep a reference to our workloop
    //
    _WORKLOOP->retain();
	
    if ( _WORKLOOP->addEventSource( _NOTIFIERHANDLER_TIMER ) != kIOReturnSuccess )
    {
        USBError(1, "%s[%p]::start Couldn't add timer event source", getName(), this);
        goto ErrorExit;
    }
	
	_COMMAND_GATE = IOCommandGate:: commandGate(this, NULL);
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
	
    return true;
	
	ErrorExit:
	
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
	
    if (_DO_PORT_SUSPEND_THREAD)
    {
        thread_call_cancel(_DO_PORT_SUSPEND_THREAD);
        thread_call_free(_DO_PORT_SUSPEND_THREAD);
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
    IOReturn 			err = kIOReturnSuccess;
    IOUSBRootHubDevice * 	rootHub = NULL;
    OSIterator *		iter;
    IOService *			client;
    UInt32				retries = 100;
    
    switch ( type )
    {
	case kIOUSBMessagePortHasBeenReset:
		
		// First decode the error that the hub driver has sent to us
		_RESET_ERROR = * (IOReturn *) argument;
		
		USBLog(4,"%s[%p] received kIOUSBMessagePortHasBeenReset with error = 0x%x",getName(), this, _RESET_ERROR);
		
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
			}
		}
		
		// Recreate PipeZero object
		_pipeZero = IOUSBPipe::ToEndpoint(&_endpointZero, this, _controller, NULL);
		if (!_pipeZero)
		{
			USBLog(1,"%s[%p]::ProcessPortReset DANGER could not recreate pipe Zero after reset", getName(), this);
			err = kIOReturnNoMemory;
		}
		
		// If there is an error, we don't want to forward the message.  Instead, we will return an error from ResetDevice() using the _RESET_ERROR
		if ( (_RESET_ERROR == kIOReturnSuccess) && (_pipeZero != NULL) )
		{
			USBLog(3, "%s[%p] calling messageClients (kIOUSBMessagePortHasBeenReset (port %ld, err 0x%x))", getName(), this, _PORT_NUMBER, _RESET_ERROR );
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
		// Forward the message to our clients
		//
		USBLog(5,"%s[%p]::message - kIOUSBMessagePortWasNotSuspended or kIOUSBMessagePortHasBeenResumed (%p)", getName(), this, (void*)type);
		
		// Note that we set the following so that the SuspendDevice() loop breaks out on a resume
		// as well.  This vaiable should be called _PORT_HAS_BEEN_SUSPENDED_OR_RESUMEDOrResumed
		//
		_PORT_HAS_BEEN_SUSPENDED_OR_RESUMED = true;
		
		// If we had set our thread to sleep waiting for the suspend to complete, now wake it up
		//
		if ( _SUSPEND_COMMAND )
		{
			USBLog(3,"%s[%p]::message  calling commandWakeUp due to kIOUSBMessagePortHasBeenResumed", getName(),this);
			if (_COMMAND_GATE)
				_COMMAND_GATE->commandWakeup(&_SUSPEND_COMMAND,  true);
			else
			{
				USBLog(1,"%s[%p]::message  cannot call commandGate->wakeup because there is no gate", getName(),this);
			}
		}
		
		messageClients( type, NULL, 0);
		break;
		
	case kIOUSBMessagePortHasBeenSuspended:
		USBLog(5,"%s[%p]: kIOUSBMessagePortHasBeenSuspended with error: 0x%x",getName(),this, * (IOReturn *) argument);
		
		// Get the error from the hub driver
		_SUSPEND_ERROR = * (IOReturn *) argument;
		
		// Finally, indicate that we have finished the suspend
		_PORT_HAS_BEEN_SUSPENDED_OR_RESUMED = true;
		
		// If we had set our thread to sleep waiting for the suspend to complete, now wake it up
		//
		if ( _SUSPEND_COMMAND )
		{
			USBLog(3,"%s[%p]::message  calling commandWakeUp due to kIOUSBMessagePortHasBeenSuspended", getName(),this);
			if (_COMMAND_GATE)
				_COMMAND_GATE->commandWakeup(&_SUSPEND_COMMAND,  true);
			else
			{
				USBLog(1,"%s[%p]::message  cannot call commandGate->wakeup because there is no gate", getName(),this);
			}
		}
		
		// Forward the message to our clients if there was no error
		//
		if ( _SUSPEND_ERROR == kIOReturnSuccess )
		{
			// This should be freed in the thread after messageClients has returned
			IOUSBDeviceMessage *	messageStructPtr = (IOUSBDeviceMessage *) IOMalloc( sizeof(IOUSBDeviceMessage));
			
			messageStructPtr->type = kIOUSBMessagePortHasBeenSuspended;
			messageStructPtr->error = _SUSPEND_ERROR;
			
			USBLog(5,"%s[%p]: kIOUSBMessagePortHasBeenSuspended calling _doClientMessage ",getName(),this);
			retain();
			if ( thread_call_enter1( _DO_MESSAGE_CLIENTS_THREAD, (thread_call_param_t) messageStructPtr ) == TRUE )
			{
				USBLog(3,"%s[%p]: kIOUSBMessagePortHasBeenSuspended _DO_MESSAGE_CLIENTS_THREAD already queued ",getName(),this);
				release();
			}
			
		}
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
    
    if(_pipeZero) 
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
		
		if (_DO_PORT_SUSPEND_THREAD)
		{
			thread_call_cancel(_DO_PORT_SUSPEND_THREAD);
			thread_call_free(_DO_PORT_SUSPEND_THREAD);
			_DO_PORT_SUSPEND_THREAD = NULL;
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
		
        if ( _GET_CONFIG_LOCK )
            IORecursiveLockFree(_GET_CONFIG_LOCK);
		
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
	
    if(!super::init())
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
    
    _GET_CONFIG_LOCK = IORecursiveLockAlloc();
    if (!_GET_CONFIG_LOCK)
    {
		IOFree(_expansionData, sizeof(ExpansionData));
        _expansionData = NULL;
        USBLog(1, "%s[%p]::init - Error allocating getConfigLock", getName(), this);
        return false;
    }
	
    _address = deviceAddress;
    _descriptor.bMaxPacketSize0 = maxPacketSize;
    _busPowerAvailable = powerAvailable;
    _speed = speed;
    _PORT_RESET_THREAD_ACTIVE = false;
    _ALLOW_CONFIGVALUE_OF_ZERO = false;
    _PORT_HAS_BEEN_RESET = false;
    
    USBLog(5,"%s @ %d (%ldmA available, %s speed)", getName(), _address,_busPowerAvailable*2, (_speed == kUSBDeviceSpeedLow) ? "low" : ((_speed == kUSBDeviceSpeedFull) ? "full" : "high") );
    
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
	
    if ( _RESET_IN_PROGRESS )
    {
        USBLog(5, "%s[%p] ResetDevice(%ld) while in progress", getName(), this, _PORT_NUMBER );
		return kIOReturnNotPermitted;
    }
	
    if ( _SUSPEND_IN_PROGRESS )
    {
        USBLog(5, "%s[%p]::ResetDevice(%ld) while SuspendDevice() in progress", getName(), this, _PORT_NUMBER );
        return kIOReturnNotPermitted;
    }

    _RESET_IN_PROGRESS = true;
	
    if ( isInactive() )
    {
        USBLog(1, "%s[%p]::ResetDevice - while terminating!", getName(), this);
        return kIOReturnNoDevice;
    }
	
    retain();
	_PORT_HAS_BEEN_RESET = false;
	_RESET_ERROR = kIOReturnSuccess;
   
    USBLog(5, "+%s[%p] ResetDevice for port %ld", getName(), this, _PORT_NUMBER );
    thread_call_enter( _DO_PORT_RESET_THREAD );
	
	if ( _WORKLOOP->inGate() && _COMMAND_GATE)
	{
		USBLog(5,"%s[%p]::ResetDevice calling commandSleep", getName(), this);
		
		_RESET_COMMAND = true;
		kr = _COMMAND_GATE->commandSleep(&_RESET_COMMAND, THREAD_UNINT);
		_RESET_COMMAND = false;
		USBLog(5,"%s[%p]::ResetDevice  woke up with error 0x%x", getName(), this, kr);
		
	}
	else
	{
		while ( !_PORT_HAS_BEEN_RESET && retries < (kMaxTimeToWaitForReset / 50) )
		{
			IOSleep(50);
			
			if ( isInactive() )
			{
				USBLog(3, "+%s[%p] isInactive() while waiting for reset to finish", getName(), this );
				_RESET_ERROR = kIOReturnNoDevice;
				break;
			}
			
			retries++;
		}
    }
	
	// If we did all our retries, then the reset probably did not complete
	//
	if ( retries == (kMaxTimeToWaitForReset / 50) )
		_RESET_ERROR = kIOUSBTransactionTimeout;
	
    USBLog(5, "-%s[%p] ResetDevice for port %ld, error: 0x%x", getName(), this, _PORT_NUMBER, _RESET_ERROR );
	
    _RESET_IN_PROGRESS = false;
    kr = _RESET_ERROR;
	
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
    UInt8				configIndex = 0;
    IOUSBConfigurationDescriptor	*curConfDesc;
    UInt16				curConfLength;
    UInt8				curConfig;
	
    if (!_configList)
		return NULL;
    
    if (!_currentConfigValue)
    {
		GetConfiguration(&curConfig);
		if (!_currentConfigValue && !_ALLOW_CONFIGVALUE_OF_ZERO)
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
    IOUSBConfigurationDescriptor *  configDescriptor = NULL;
    
    if (!_configList || (index >= _descriptor.bNumConfigurations))
        return NULL;
    
    // it is possible that we could end up in a race condition with multiple drivers trying to get a config descriptor.
    // we are not able to fix that by running this code through the command gate, because then the actual bus command 
    // would not be able to complete with us holding the gate. so we have a device specific lock instead
    
    USBLog(7, "%s[%p]::GetFullConfigurationDescriptor - Index (%x) - about to obtain lock", getName(), this, index);
    IORecursiveLockLock(_GET_CONFIG_LOCK);
    USBLog(7, "%s[%p]::GetFullConfigurationDescriptor - Index (%x) - lock obtained", getName(), this, index);
    
    if(_configList[index] == NULL) 
    {
        int 				len;
        IOUSBConfigurationDescHeader	temp;
        UInt16				idVendor = USBToHostWord(_descriptor.idVendor);
        UInt16				idProduct = USBToHostWord(_descriptor.idProduct);
        
        // 2755742 - workaround for a ill behaved device
        // Also do this for Fujitsu scanner VID = 0x4C5 PID = 0x1040
        if ( ((idVendor == 0x3f0) && (idProduct == 0x1001)) || ((idVendor == 0x4c5) && (idProduct == 0x1040)) )
        {
            USBLog(3, "%s[%p]::GetFullConfigurationDescriptor - assuming config desc length of 39", getName(), this);
            len = 39;
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
        localConfigPointer = IOBufferMemoryDescriptor::withCapacity(len, kIODirectionIn);
        
        if(!localConfigPointer)
        {
            USBLog(1, "%s[%p]::GetFullConfigurationDescriptor - unable to get memory buffer (capacity requested: %d)", getName(), this, len);
            goto Exit;
        }
        
        USBLog(5, "%s[%p]::GetFullConfigurationDescriptor - Index (%x) - getting full %d bytes of config descriptor", getName(), this, index, len);
        err = GetConfigDescriptor(index, localConfigPointer->getBytesNoCopy(), len);
        if (err) 
        {
            USBLog(1, "%s[%p]::GetFullConfigurationDescriptor - Error (%x) getting full %d bytes of config descriptor", getName(), this, err, len);
            
            if ( localConfigPointer )
            {
                localConfigPointer->release();
                localConfigPointer = NULL;
            }
            goto Exit;
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
    configDescriptor = (IOUSBConfigurationDescriptor *)_configList[index]->getBytesNoCopy();
Exit:
        IORecursiveLockUnlock(_GET_CONFIG_LOCK);
    return configDescriptor ;
}



IOReturn 
IOUSBDevice::GetDeviceDescriptor(IOUSBDeviceDescriptor *desc, UInt32 size)
{
    IOUSBDevRequest	request;
    IOReturn		err;
	
    USBLog(5, "%s[%p]::GetDeviceDescriptor (size %ld)", getName(), this, size);
	
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
	
    USBLog(5, "%s[%p]::GetConfigDescriptor (length: %ld)", getName(), this, len);
	
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
    }
	
    return err;
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
    IOReturn							err = kIOReturnSuccess;
    IOUSBDevRequest						request;
    const IOUSBConfigurationDescriptor *confDesc;
    bool								allowConfigValueOfZero = false;
    OSObject *							propertyObj = NULL;
	OSBoolean *							boolObj = NULL;
	OSNumber *							numberObj = NULL;
	
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
		
		USBLog(5, "%s[%p]::SetConfiguration - port %ld, calling Hub to set kUSBDeviceResumeRecoveryTime to %ld", getName(), this, _PORT_NUMBER, portRecoveryOptions.options );
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
		DisplayUserNotification(kUSBNotEnoughPowerNotificationType);
        USBLog(1,"%s[%p]::SetConfiguration  Not enough bus power to configure device",getName(), this);
        IOLog("USB Low Power Notice:  The device \"%s\" cannot be used because there is not enough power to configure it\n",getName());
		
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
        USBLog(1,"%s[%p]: error setting config. err=0x%x", getName(), this, err);
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
    OSString	*wildCard;
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
			USBLog(5,"Finding device driver for %s, matching personality using %s, score: %ld, wildCard = %ld", getName(), identifier->getCStringNoCopy(), *score, wildCardMatches);
		}
		else
		{
			USBLog(6,"Finding device driver for %s, matching user client dictionary, score: %ld", getName(), *score);
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
    if(len == 0)
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
    USBLog(3,"%s[%p]DisplayUserNotificationForDevice notificationType: %ld",getName(), this, _NOTIFICATION_TYPE );
	
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
#endif
	
    switch ( _NOTIFICATION_TYPE )
    {
        case kUSBNotEnoughPowerNotificationType:
            IOLog("USB Notification:  The device \"%s\" @ 0x%lx cannot operate because there is not enough power available\n",getName(), locationID);
            notificationError = KUNCUserNotificationDisplayNotice(
                                                                  0,		// Timeout in seconds
                                                                  0,		// Flags (for later usage)
                                                                  (char *) "",		// iconPath (not supported yet)
                                                                  (char *) "",		// soundPath (not supported yet)
                                                                  (char *) "/System/Library/Extensions/IOUSBFamily.kext",		// localizationPath
                                                                  (char *) "USB Low Power Header",		// the header
                                                                  (char *) "USB Low Power Notice",		// the notice - look in Localizable.strings
                                                                  (char *) "OK");
            break;
			
        case kUSBIndividualOverCurrentNotificationType:
            IOLog("USB Notification:  The device \"%s\" @ 0x%lx has caused an overcurrent condition.  The port it is attached to has been disabled\n",getName(), locationID);
            notificationError = KUNCUserNotificationDisplayNotice(
                                                                  0,		// Timeout in seconds
                                                                  0,		// Flags (for later usage)
                                                                  (char *) "",		// iconPath (not supported yet)
                                                                  (char *) "",		// soundPath (not supported yet)
                                                                  (char *) "/System/Library/Extensions/IOUSBFamily.kext",		// localizationPath
                                                                  (char *) "USB OverCurrent Header",					// the header
                                                                  (char *) "USB Individual OverCurrent Notice",				// the notice - look in Localizable.strings
                                                                  (char *) "OK");
            break;
			
        case kUSBGangOverCurrentNotificationType:
            IOLog("USB Notification:  The device \"%s\" @ 0x%lx has caused an overcurrent condition.  The hub it is attached to has been disabled\n",getName(), locationID);
            notificationError = KUNCUserNotificationDisplayNotice(
                                                                  0,		// Timeout in seconds
                                                                  0,		// Flags (for later usage)
                                                                  (char *) "",		// iconPath (not supported yet)
                                                                  (char *) "",		// soundPath (not supported yet)
                                                                  (char *) "/System/Library/Extensions/IOUSBFamily.kext",		// localizationPath
                                                                  (char *) "USB OverCurrent Header",					// the header
                                                                  (char *) "USB Gang OverCurrent Notice",				// the notice - look in Localizable.strings
                                                                  (char *) "OK");
            break;
    }
	
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


OSMetaClassDefineReservedUsed(IOUSBDevice,  0);
IOReturn 
IOUSBDevice::DeviceRequest(IOUSBDevRequest *request, UInt32 noDataTimeout, UInt32 completionTimeout, IOUSBCompletion *completion)
{
    UInt16	theRequest;
    IOReturn	err;
    UInt16	wValue = request->wValue;
    
    if (isInactive())
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
    
    if (isInactive())
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



//=============================================================================================
//
//  SuspendDevice
//
//
//=============================================================================================
//
OSMetaClassDefineReservedUsed(IOUSBDevice,  2);
IOReturn
IOUSBDevice::SuspendDevice( bool suspend )
{
	IOReturn	kr = kIOReturnSuccess;
    UInt32		retries = 0;
	
    if ( _SUSPEND_IN_PROGRESS )
    {
        USBLog(5, "%s[%p]::SuspendDevice(%ld) while in progress", getName(), this, _PORT_NUMBER );
        return kIOReturnNotPermitted;
    }
	
    if ( _RESET_IN_PROGRESS )
    {
        USBLog(5, "%s[%p]::SuspendDevice(%ld) while in ResetDevice() in progress", getName(), this, _PORT_NUMBER );
        return kIOReturnNotPermitted;
    }
	
    if ( isInactive() )
    {
        USBLog(1, "%s[%p]::SuspendDevice - while inactive!", getName(), this);
        return kIOReturnNoDevice;
    }
	
    _SUSPEND_IN_PROGRESS = true;
    _PORT_HAS_BEEN_SUSPENDED_OR_RESUMED = false;
	_SUSPEND_ERROR = kIOReturnSuccess;
	
	retain();
	
	USBLog(5, "+%s[%p]::SuspendDevice(%s) for port %ld", getName(), this, suspend ? "suspend" : "resume", _PORT_NUMBER );
	if ( thread_call_enter1( _DO_PORT_SUSPEND_THREAD, (thread_call_param_t) suspend ) == TRUE )
	{
		USBLog(3, "%s[%p]::SuspendDevice(%s) for port %ld, _DO_PORT_SUSPEND_THREAD already queued", getName(), this, suspend ? "suspend" : "resume", _PORT_NUMBER );
		release();
	}
	
	if ( _WORKLOOP->inGate() && _COMMAND_GATE)
	{
		USBLog(6,"%s[%p]::SuspendDevice calling commandSleep", getName(), this);

		_SUSPEND_COMMAND = true;
		kr = _COMMAND_GATE->commandSleep(&_SUSPEND_COMMAND, THREAD_UNINT);
		_SUSPEND_COMMAND = false;
		USBLog(6,"%s[%p]::SuspendDevice woke up with error 0x%x", getName(), this, kr);
		
	}
	else
	{
		while ( !(_PORT_HAS_BEEN_SUSPENDED_OR_RESUMED && !_SUSPEND_IN_PROGRESS) && (retries < (kMaxTimeToWaitForSuspend / 5)) )
		{
			IOSleep(5);
			
			if ( isInactive() )
			{
				USBLog(3, "+%s[%p]::SuspendDevice isInactive() while waiting for suspend to finish", getName(), this );
				_SUSPEND_ERROR = kIOReturnNoDevice;
				break;
			}
			
			retries++;
		}
	}
	
	if ( retries == (kMaxTimeToWaitForSuspend / 5) )
	{
		_SUSPEND_ERROR = kIOUSBTransactionTimeout;
	}
	
	USBLog(5, "-%s[%p]::SuspendDevice for port %ld with error 0x%x, _PORT_SUSPEND_THREAD_ACTIVE = %d", getName(), this, _PORT_NUMBER, _SUSPEND_ERROR, _PORT_SUSPEND_THREAD_ACTIVE );

    _SUSPEND_IN_PROGRESS = false;
	
    return _SUSPEND_ERROR;
}



OSMetaClassDefineReservedUsed(IOUSBDevice,  3);
IOReturn
IOUSBDevice::ReEnumerateDevice( UInt32 options )
{
    if (isInactive())
    {
        USBLog(1, "%s[%p]::ReEnumerateDevice - while terminating!", getName(), this);
        return kIOReturnNotResponding;
    }
	
    // If we have the _ADD_EXTRA_RESET_TIME, set bit 31 of the options
    //
    if ( _ADD_EXTRA_RESET_TIME )
    {
        USBLog(1, "%s[%p]::ReEnumerateDevice - setting extra reset time options!", getName(), this);
        options |= kUSBAddExtraResetTimeMask;
    }
    
    // Since we are going to re-enumerate the device, all drivers and interfaces will be
    // terminated, so we don't need to make this device synchronous.  In fact, we want it
    // async, because this device will go away.
    //
    USBLog(3, "+%s[%p] ReEnumerateDevice for port %ld, options 0x%lx", getName(), this, _PORT_NUMBER, options );
    retain();
    if ( thread_call_enter1( _DO_PORT_REENUMERATE_THREAD, (thread_call_param_t) options) == TRUE )
	{
		USBLog(3, "+%s[%p] ReEnumerateDevice for port %ld, _DO_PORT_REENUMERATE_THREAD already queued", getName(), this, _PORT_NUMBER );
		release();
	}
	
    USBLog(3, "-%s[%p] ReEnumerateDevice for port %ld", getName(), this, _PORT_NUMBER );
    
    return kIOReturnSuccess;
}

OSMetaClassDefineReservedUsed(IOUSBDevice,  4);
void
IOUSBDevice::DisplayUserNotification(UInt32 notificationType )
{
    // NOTE:  If we get multiple calls before we actually display the notification (i.e. this gets called at boot)
    //        we will only display the last notification.
    //
    USBLog(3, "%s[%p] DisplayUserNotification type %ld", getName(), this, notificationType );
    _NOTIFICATION_TYPE = notificationType;
    
    DisplayUserNotificationForDeviceEntry(this, NULL );
}

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
	
    if( _pipeZero) 
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
    
    _currentConfigValue = 0;
	
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
    params.portNumber = _PORT_NUMBER;
    params.options = options;
    
    if( _pipeZero) 
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
        USBLog(3, "%s[%p] calling messageClients (kIOUSBMessageHubReEnumeratePort)", getName(), this);
        _USBPLANE_PARENT->retain();
        err = _USBPLANE_PARENT->messageClients(kIOUSBMessageHubReEnumeratePort, &params, sizeof(IOUSBHubPortReEnumerateParam));
        _USBPLANE_PARENT->release();
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
	
    USBLog(6,"+%s[%p]::ProcessPortSuspend, _PORT_SUSPEND_THREAD_ACTIVE = %d, isInactive() = %d",getName(), this, _PORT_RESET_THREAD_ACTIVE, isInactive() ); 
	
	if (!_expansionData)
		return;
	
	// if we are already resetting the port, then just return ( Perhaps we should do this atomically)?
	//
	if ( _PORT_SUSPEND_THREAD_ACTIVE || isInactive() )
		return;
	else
	    _PORT_SUSPEND_THREAD_ACTIVE = true;
    
    // Now, we need to tell our parent to issue a port suspend to our port.  Our parent is an IOUSBDevice
    // that has a hub driver attached to it.  However, we don't know what kind of driver that is, so we
    // just send a message to all the clients of our parent.  The hub driver will be the only one that
    // should do anything with that message.
    //
    if ( _USBPLANE_PARENT )
    {
        USBLog(5, "%s[%p]::ProcessPortSuspend calling messageClients (kIOUSBMessageHubSuspendPort) with value 0x%d", getName(), this, suspend);
		
        _USBPLANE_PARENT->retain();
        if ( suspend )
            err = _USBPLANE_PARENT->messageClients(kIOUSBMessageHubSuspendPort, &_PORT_NUMBER, sizeof(_PORT_NUMBER));
        else
            err = _USBPLANE_PARENT->messageClients(kIOUSBMessageHubResumePort, &_PORT_NUMBER, sizeof(_PORT_NUMBER));
        _USBPLANE_PARENT->release();
    }
	
	_PORT_SUSPEND_THREAD_ACTIVE = false;
	_SUSPEND_IN_PROGRESS = false;
	
    USBLog(6,"-%s[%p]::ProcessPortSuspend",getName(),this); 
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
	
    USBLog(7,"+%s[%p]::DoMessageClients:  type = 0x%lx, error = 0x%x",getName(),this, type, error); 
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
	// this is a deprecated KPI
    return NULL; 
}

OSMetaClassDefineReservedUsed(IOUSBDevice,  5);
IOUSBPipe * 
IOUSBDevice::MakePipe(const IOUSBEndpointDescriptor *ep, IOUSBInterface * interface) 
{
    return IOUSBPipe::ToEndpoint(ep, this, _controller, interface); 
}



