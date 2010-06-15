/*
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
 
#include "IOHIDEventServiceClass.h"
#include "IOHIDEventServiceUserClient.h"
#include "IOHIDEventData.h"
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/hid/IOHIDServiceKeys.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/AppleEmbeddedHIDKeys.h>

__BEGIN_DECLS
#include <mach/mach.h>
#include <mach/mach_interface.h>
#include <IOKit/iokitmig.h>
#include <IOKit/IOMessage.h>
#include <mach/mach_time.h>
__END_DECLS

#define QUEUE_LOCK(service)     pthread_mutex_lock(&service->_queueLock)
#define QUEUE_UNLOCK(service)   pthread_mutex_unlock(&service->_queueLock)
#define QUEUE_WAIT(service)     while (service->_queueBusy) { pthread_cond_wait(&service->_queueCondition, &service->_queueLock); }
#define QUEUE_SIGNAL(service)   pthread_cond_signal(&service->_queueCondition)

//===========================================================================
// Static Helper Declarations
//===========================================================================
static IOReturn MergeDictionaries(CFDictionaryRef srcDict, CFMutableDictionaryRef * pDstDict);


//===========================================================================
// CFPlugIn Static Assignments
//===========================================================================
IOCFPlugInInterface IOHIDEventServiceClass::sIOCFPlugInInterfaceV1 =
{
    0,
    &IOHIDIUnknown::genericQueryInterface,
    &IOHIDIUnknown::genericAddRef,
    &IOHIDIUnknown::genericRelease,
    1, 0,	// version/revision
    &IOHIDEventServiceClass::_probe,
    &IOHIDEventServiceClass::_start,
    &IOHIDEventServiceClass::_stop
};

IOHIDServiceInterface2 IOHIDEventServiceClass::sIOHIDServiceInterface2 =
{
    0,
    &IOHIDIUnknown::genericQueryInterface,
    &IOHIDIUnknown::genericAddRef,
    &IOHIDIUnknown::genericRelease,
    &IOHIDEventServiceClass::_open,
    &IOHIDEventServiceClass::_close,
    &IOHIDEventServiceClass::_getProperty,
    &IOHIDEventServiceClass::_setProperty,
    &IOHIDEventServiceClass::_setEventCallback,
    &IOHIDEventServiceClass::_scheduleWithRunLoop,
    &IOHIDEventServiceClass::_unscheduleFromRunLoop,
    &IOHIDEventServiceClass::_copyEvent,
    &IOHIDEventServiceClass::_setElementValue
};

//===========================================================================
// CONSTRUCTOR / DESTRUCTOR methods
//===========================================================================
//---------------------------------------------------------------------------
// IOHIDEventServiceClass
//---------------------------------------------------------------------------
IOHIDEventServiceClass::IOHIDEventServiceClass() : IOHIDIUnknown(&sIOCFPlugInInterfaceV1)
{
    _hidService.pseudoVTable    = (IUnknownVTbl *)  &sIOHIDServiceInterface2;
    _hidService.obj             = this;
    
    _service                    = MACH_PORT_NULL;
    _connect                    = MACH_PORT_NULL;
    _isOpen                     = FALSE;

    _asyncPort                  = MACH_PORT_NULL;
    _asyncEventSource           = NULL;
        
    _serviceProperties          = NULL;
    _dynamicServiceProperties   = NULL;
    _servicePreferences         = NULL;
    _eventCallback              = NULL;
    _eventTarget                = NULL;
    _eventRefcon                = NULL;
    
    _queueMappedMemory          = NULL;
    _queueMappedMemorySize      = 0;
    
    _queueBusy                  = TRUE;

    pthread_mutex_init(&_queueLock, NULL);
    pthread_cond_init(&_queueCondition, NULL);
}

//---------------------------------------------------------------------------
// ~IOHIDEventServiceClass
//---------------------------------------------------------------------------
IOHIDEventServiceClass::~IOHIDEventServiceClass()
{
    QUEUE_LOCK(this);
    
    QUEUE_WAIT(this);

    // finished with the shared memory
    if (_queueMappedMemory)
    {
#if !__LP64__
        vm_address_t        mappedMem = (vm_address_t)_queueMappedMemory;
#else
        mach_vm_address_t   mappedMem = (mach_vm_address_t)_queueMappedMemory;
#endif
        IOConnectUnmapMemory (  _connect, 
                                0, 
                                mach_task_self(), 
                                mappedMem);
        _queueMappedMemory = NULL;
        _queueMappedMemorySize = 0;
    }
    QUEUE_UNLOCK(this);

    if (_connect) {
        IOServiceClose(_connect);
        _connect = MACH_PORT_NULL;
    }
    if (_service) {
        IOObjectRelease(_service);
        _service = MACH_PORT_NULL;
    }
        
    if (_serviceProperties) {
        CFRelease(_serviceProperties);
        _serviceProperties = NULL;
    }

    if (_dynamicServiceProperties) {
        CFRelease(_dynamicServiceProperties);
        _dynamicServiceProperties = NULL;
    }

    if (_servicePreferences) {
        CFRelease(_servicePreferences);
        _servicePreferences = NULL;
    }

    if (_asyncEventSource) {
        CFRelease(_asyncEventSource);
        _asyncEventSource = NULL;
    }
    
    if ( _asyncPort ) {
    //  radr://6727552
    //  mach_port_deallocate(mach_task_self(), _asyncPort);
        mach_port_mod_refs(mach_task_self(), _asyncPort, MACH_PORT_RIGHT_RECEIVE, -1);
        _asyncPort = MACH_PORT_NULL;
    }
        
    pthread_mutex_destroy(&_queueLock);
    pthread_cond_destroy(&_queueCondition);
}

//===========================================================================
// IOCFPlugInInterface methods
//===========================================================================
IOReturn IOHIDEventServiceClass::_probe(void *self, CFDictionaryRef propertyTable, io_service_t service, SInt32 *order)
{
    return getThis(self)->probe(propertyTable, service, order);
}

IOReturn IOHIDEventServiceClass::_start(void *self, CFDictionaryRef propertyTable, io_service_t service)
{
    return getThis(self)->start(propertyTable, service);
}

IOReturn IOHIDEventServiceClass::_stop(void *self)
{
    return getThis(self)->stop();
}

boolean_t IOHIDEventServiceClass::_open(void * self, IOOptionBits options)
{
    return getThis(self)->open(options);
}

void IOHIDEventServiceClass::_close(void * self, IOOptionBits options)
{
    getThis(self)->close(options);
}

CFTypeRef IOHIDEventServiceClass::_getProperty(void * self, CFStringRef key)
{
    return getThis(self)->getProperty(key);
}

boolean_t IOHIDEventServiceClass::_setProperty(void * self, CFStringRef key, CFTypeRef property)
{
    return getThis(self)->setProperty(key, property);
}

IOHIDEventRef IOHIDEventServiceClass::_copyEvent(void *self, IOHIDEventType type, IOHIDEventRef matching, IOOptionBits options)
{
    return getThis(self)->copyEvent(type, matching, options);
}

void IOHIDEventServiceClass::_setElementValue(void *self, uint32_t usagePage, uint32_t usage, uint32_t value)
{
    return getThis(self)->setElementValue(usagePage, usage, value);
}

void IOHIDEventServiceClass::_setEventCallback(void * self, IOHIDServiceEventCallback callback, void * target, void * refcon)
{
    getThis(self)->setEventCallback(callback, target, refcon);
}

void IOHIDEventServiceClass::_scheduleWithRunLoop(void *self, CFRunLoopRef runLoop, CFStringRef runLoopMode)
{
    return getThis(self)->scheduleWithRunLoop(runLoop, runLoopMode);
}

void IOHIDEventServiceClass::_unscheduleFromRunLoop(void *self, CFRunLoopRef runLoop, CFStringRef runLoopMode)
{
    return getThis(self)->unscheduleFromRunLoop(runLoop, runLoopMode);
}

//------------------------------------------------------------------------------
// IOHIDEventServiceClass::_queueEventSourceCallback
//------------------------------------------------------------------------------
void IOHIDEventServiceClass::_queueEventSourceCallback(
                                            CFMachPortRef               cfPort, 
                                            mach_msg_header_t *         msg, 
                                            CFIndex                     size, 
                                            void *                      info)
{
    IOHIDEventServiceClass *eventService = (IOHIDEventServiceClass *)info;
    IOReturn ret = kIOReturnSuccess;

    QUEUE_LOCK(eventService);

    QUEUE_WAIT(eventService);

    eventService->_queueBusy = TRUE;
    
    do {
        if ( !eventService->_queueMappedMemory )
            break;

        // check entry size
        IODataQueueEntry *  nextEntry;
        uint32_t            dataSize;
        CFDataRef           data;

        // if queue empty, then stop
        while ((nextEntry = IODataQueuePeek(eventService->_queueMappedMemory))) {
            data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (const UInt8*)&(nextEntry->data), nextEntry->size, kCFAllocatorNull);

            // if we got an entry
            if (data) {
                IOHIDEventRef event = IOHIDEventCreateWithData(kCFAllocatorDefault, data);
                
                if ( event ) {
                
                    QUEUE_UNLOCK(eventService);
                    eventService->dispatchHIDEvent(event);
                    QUEUE_LOCK(eventService);
                    
                    CFRelease(event);
                }
                CFRelease(data);
            }
            
            // dequeue the item
            dataSize = 0;
            IODataQueueDequeue(eventService->_queueMappedMemory, NULL, &dataSize);
        }
    } while ( 0 );

    eventService->_queueBusy = FALSE;
    
    QUEUE_UNLOCK(eventService);
    
    QUEUE_SIGNAL(eventService);
}


//------------------------------------------------------------------------------
// IOHIDEventServiceClass::dispatchHIDEvent
//------------------------------------------------------------------------------
void IOHIDEventServiceClass::dispatchHIDEvent(IOHIDEventRef event, IOOptionBits options)
{
    if ( !_eventCallback )
        return;
        
    (*_eventCallback)(_eventTarget, _eventRefcon, (void *)&_hidService, event, options);
}



// Public Methods
//---------------------------------------------------------------------------
// IOHIDEventServiceClass::alloc
//---------------------------------------------------------------------------
IOCFPlugInInterface ** IOHIDEventServiceClass::alloc()
{
    IOHIDEventServiceClass * self = new IOHIDEventServiceClass;
    
    return self ? (IOCFPlugInInterface **) &self->iunknown.pseudoVTable : NULL;
}

//---------------------------------------------------------------------------
// IOHIDEventServiceClass::queryInterface
//---------------------------------------------------------------------------
HRESULT IOHIDEventServiceClass::queryInterface(REFIID iid, void **ppv)
{
    CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(NULL, iid);
    HRESULT res = S_OK;

    if (CFEqual(uuid, IUnknownUUID) || CFEqual(uuid, kIOCFPlugInInterfaceID))
    {
        *ppv = &iunknown;
        addRef();
    }
    else if (CFEqual(uuid, kIOHIDServiceInterfaceID) || CFEqual(uuid, kIOHIDServiceInterface2ID))
    {
        *ppv = &_hidService;
        addRef();
    }
    else {
        *ppv = 0;
    }

    if (!*ppv)
        res = E_NOINTERFACE;

    CFRelease(uuid);
    return res;
}

//---------------------------------------------------------------------------
// IOHIDEventServiceClass::probe
//---------------------------------------------------------------------------
IOReturn IOHIDEventServiceClass::probe(CFDictionaryRef propertyTable, io_service_t service, SInt32 * order)
{
    if (!service || !IOObjectConformsTo(service, "IOHIDEventService"))
        return kIOReturnBadArgument;

    return kIOReturnSuccess;
}

#define GET_AND_SET_SERVICE_PROPERTY(reg,regKey,dict,propKey)                                               \
{                                                                                                   \
    CFTypeRef typeRef = IORegistryEntryCreateCFProperty(reg,regKey, kCFAllocatorDefault, kNilOptions);\
    if (typeRef)                                                                                    \
    {                                                                                               \
        CFDictionarySetValue(dict,propKey,typeRef);                                                 \
        CFRelease(typeRef);                                                                         \
    }                                                                                               \
}

#define GET_AND_SET_PROPERTY(prop,regKey,dict,propKey)                                              \
{                                                                                                   \
    CFTypeRef typeRef = CFDictionaryGetValue(prop,regKey);                                          \
    if (typeRef)                                                                                    \
        CFDictionarySetValue(dict,propKey,typeRef);                                                 \
}


//---------------------------------------------------------------------------
// IOHIDEventServiceClass::start
//---------------------------------------------------------------------------
IOReturn IOHIDEventServiceClass::start(CFDictionaryRef propertyTable, io_service_t service)
{
    IOReturn                ret             = kIOReturnError;
    HRESULT                 plugInResult 	= S_OK;
    SInt32                  score           = 0;
    CFMutableDictionaryRef  serviceProps    = NULL;
    
    do {
        _service = service;
        IOObjectRetain(_service);

        _serviceProperties = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        
        if ( !_serviceProperties ) {
            ret = kIOReturnNoMemory;
            break;
        }

        IORegistryEntryCreateCFProperties(service, &serviceProps, kCFAllocatorDefault, 0);

        if ( !serviceProps )
            break;
            
        GET_AND_SET_PROPERTY(serviceProps, CFSTR(kIOHIDTransportKey), _serviceProperties, CFSTR(kIOHIDServiceTransportKey));
        GET_AND_SET_PROPERTY(serviceProps, CFSTR(kIOHIDVendorIDKey), _serviceProperties, CFSTR(kIOHIDServiceVendorIDKey));
        GET_AND_SET_PROPERTY(serviceProps, CFSTR(kIOHIDVendorIDSourceKey), _serviceProperties, CFSTR(kIOHIDServiceVendorIDSourceKey));
        GET_AND_SET_PROPERTY(serviceProps, CFSTR(kIOHIDProductIDKey), _serviceProperties, CFSTR(kIOHIDServiceProductIDKey));
        GET_AND_SET_PROPERTY(serviceProps, CFSTR(kIOHIDVersionNumberKey), _serviceProperties, CFSTR(kIOHIDServiceVersionNumberKey));
        GET_AND_SET_PROPERTY(serviceProps, CFSTR(kIOHIDManufacturerKey), _serviceProperties, CFSTR(kIOHIDServiceManufacturerKey));
        GET_AND_SET_PROPERTY(serviceProps, CFSTR(kIOHIDProductKey), _serviceProperties, CFSTR(kIOHIDServiceProductKey));
        GET_AND_SET_PROPERTY(serviceProps, CFSTR(kIOHIDSerialNumberKey), _serviceProperties, CFSTR(kIOHIDServiceSerialNumberKey));
        GET_AND_SET_PROPERTY(serviceProps, CFSTR(kIOHIDCountryCodeKey), _serviceProperties, CFSTR(kIOHIDServiceCountryCodeKey));
        GET_AND_SET_PROPERTY(serviceProps, CFSTR(kIOHIDLocationIDKey), _serviceProperties, CFSTR(kIOHIDServiceLocationIDKey));
        GET_AND_SET_PROPERTY(serviceProps, CFSTR(kIOHIDPrimaryUsagePageKey), _serviceProperties, CFSTR(kIOHIDServicePrimaryUsagePageKey));
        GET_AND_SET_PROPERTY(serviceProps, CFSTR(kIOHIDPrimaryUsageKey), _serviceProperties, CFSTR(kIOHIDServicePrimaryUsageKey));
        GET_AND_SET_PROPERTY(serviceProps, CFSTR(kIOHIDDeviceUsagePairsKey), _serviceProperties, CFSTR(kIOHIDServiceDeviceUsagePairsKey));
// This should be considered a dymanic property
//        GET_AND_SET_PROPERTY(serviceProps, CFSTR(kIOHIDReportIntervalKey), _serviceProperties, CFSTR(kIOHIDServiceReportIntervalKey));

        CFRelease(serviceProps);
        
        /*
        // Get properties, but do so via IORegistryEntryCreateCFProperty instead
        // of IORegistryEntryCreateCFProperties to avoid pulling in more that we
        // need and increasing footprint
        GET_AND_SET_SERVICE_PROPERTY(service, CFSTR(kIOHIDTransportKey), _serviceProperties, CFSTR(kIOHIDServiceTransportKey));
        GET_AND_SET_SERVICE_PROPERTY(service, CFSTR(kIOHIDVendorIDKey), _serviceProperties, CFSTR(kIOHIDServiceVendorIDKey));
        GET_AND_SET_SERVICE_PROPERTY(service, CFSTR(kIOHIDVendorIDSourceKey), _serviceProperties, CFSTR(kIOHIDServiceVendorIDSourceKey));
        GET_AND_SET_SERVICE_PROPERTY(service, CFSTR(kIOHIDProductIDKey), _serviceProperties, CFSTR(kIOHIDServiceProductIDKey));
        GET_AND_SET_SERVICE_PROPERTY(service, CFSTR(kIOHIDVersionNumberKey), _serviceProperties, CFSTR(kIOHIDServiceVersionNumberKey));
        GET_AND_SET_SERVICE_PROPERTY(service, CFSTR(kIOHIDManufacturerKey), _serviceProperties, CFSTR(kIOHIDServiceManufacturerKey));
        GET_AND_SET_SERVICE_PROPERTY(service, CFSTR(kIOHIDProductKey), _serviceProperties, CFSTR(kIOHIDServiceProductKey));
        GET_AND_SET_SERVICE_PROPERTY(service, CFSTR(kIOHIDSerialNumberKey), _serviceProperties, CFSTR(kIOHIDServiceSerialNumberKey));
        GET_AND_SET_SERVICE_PROPERTY(service, CFSTR(kIOHIDCountryCodeKey), _serviceProperties, CFSTR(kIOHIDServiceCountryCodeKey));
        GET_AND_SET_SERVICE_PROPERTY(service, CFSTR(kIOHIDLocationIDKey), _serviceProperties, CFSTR(kIOHIDServiceLocationIDKey));
        GET_AND_SET_SERVICE_PROPERTY(service, CFSTR(kIOHIDPrimaryUsagePageKey), _serviceProperties, CFSTR(kIOHIDServicePrimaryUsagePageKey));
        GET_AND_SET_SERVICE_PROPERTY(service, CFSTR(kIOHIDPrimaryUsageKey), _serviceProperties, CFSTR(kIOHIDServicePrimaryUsageKey));
// This should be considered a dymanic property
//        GET_AND_SET_SERVICE_PROPERTY(service, CFSTR(kIOHIDReportIntervalKey), _serviceProperties, CFSTR(kIOHIDServiceReportIntervalKey));
        */

        // Establish connection with device
        ret = IOServiceOpen(_service, mach_task_self(), 0, &_connect);
        if (ret != kIOReturnSuccess || !_connect)
            break;
                        
        // allocate the memory
        QUEUE_LOCK(this);

#if !__LP64__
        vm_address_t        address = nil;
        vm_size_t           size    = 0;
#else
        mach_vm_address_t   address = nil;
        mach_vm_size_t      size    = 0;
#endif
        ret = IOConnectMapMemory (	_connect, 
                                    0, 
                                    mach_task_self(), 
                                    &address, 
                                    &size, 
                                    kIOMapAnywhere	);
        if (ret != kIOReturnSuccess) 
            return false;
        
        _queueMappedMemory = (IODataQueueMemory *) address;
        _queueMappedMemorySize = size;
        
        if ( !_queueMappedMemory || !_queueMappedMemorySize )
            break;

        _queueBusy = FALSE;
        
        QUEUE_UNLOCK(this);
        
        QUEUE_SIGNAL(this);

        return kIOReturnSuccess;
        
    } while (0);
    
    if ( _service ) {
        IOObjectRelease(_service);
        _service = NULL;
    }
    
    return ret;
}

//---------------------------------------------------------------------------
// IOHIDEventServiceClass::stop
//---------------------------------------------------------------------------
IOReturn IOHIDEventServiceClass::stop()
{
    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------
// IOHIDEventServiceClass::open
//---------------------------------------------------------------------------
boolean_t IOHIDEventServiceClass::open(IOOptionBits options)
{
    uint32_t len = 0;
    uint64_t input = options;
    IOReturn kr;
    bool     ret = true;
    
    QUEUE_LOCK(this);

    if ( !_isOpen ) {
            
        do {
            kr = IOConnectCallScalarMethod(_connect, kIOHIDEventServiceUserClientOpen, &input, 1, 0, &len);; 
            if ( kr != kIOReturnSuccess ) {
                ret = false;
                break;
            }

            _isOpen = true;
            
            // drain the queue just in case
            QUEUE_UNLOCK(this);

            if ( _eventCallback )
                _queueEventSourceCallback(NULL, NULL, 0, this);
                
            QUEUE_LOCK(this);
            
        } while ( 0 );
    }
    
    QUEUE_UNLOCK(this);
                                                    
    return ret;
}

//---------------------------------------------------------------------------
// IOHIDEventServiceClass::close
//---------------------------------------------------------------------------
void IOHIDEventServiceClass::close(IOOptionBits options)
{
    uint32_t len = 0;
    uint64_t input = options;
        
    QUEUE_LOCK(this);

    if ( _isOpen ) {
        (void) IOConnectCallScalarMethod(_connect, kIOHIDEventServiceUserClientClose, &input, 1, 0, &len); 
        
        _isOpen = false;
    }

    QUEUE_UNLOCK(this);
}

//---------------------------------------------------------------------------
// IOHIDEventServiceClass::getProperty
//---------------------------------------------------------------------------
CFTypeRef IOHIDEventServiceClass::getProperty(CFStringRef key)
{
    CFTypeRef value = CFDictionaryGetValue(_serviceProperties, key);
    
    if ( !value ) {
        if ( !_dynamicServiceProperties && 
             !(_dynamicServiceProperties = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)))
            return NULL;

        value = IORegistryEntryCreateCFProperty(_service, key, kCFAllocatorDefault, kNilOptions);
        if (value)
        {
            CFDictionarySetValue(_dynamicServiceProperties,key,value);
            CFRelease(value);
        }        
    }

    return value;
}

//---------------------------------------------------------------------------
// IOHIDEventServiceClass::createFixedProperties
//---------------------------------------------------------------------------
CFDictionaryRef IOHIDEventServiceClass::createFixedProperties(CFDictionaryRef floatProperties)
{
    CFMutableDictionaryRef  newProperties;
    CFIndex                 count, index;
    
    count = CFDictionaryGetCount(floatProperties);
    if ( !count )
        return NULL;
           
    newProperties = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if ( !newProperties )
        return NULL;

    CFTypeRef   values[count];
    CFTypeRef   keys[count];

    CFDictionaryGetKeysAndValues(floatProperties, keys, values);
    
    for ( index=0; index<count; index++) {
        CFTypeRef   value       = values[index];
        CFTypeRef   newValue    = NULL;
        
        if ( (CFNumberGetTypeID() == CFGetTypeID(value)) && CFNumberIsFloatType((CFNumberRef)value) ) {
            double      floatValue  = 0.0;
            IOFixed     fixedValue  = 0;
            
            CFNumberGetValue((CFNumberRef)value, kCFNumberDoubleType, &floatValue);
            
            fixedValue = floatValue * 65535;
            
            value = newValue = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &fixedValue);            
        } 

        CFDictionarySetValue(newProperties, keys[index], value);
        
        if ( newValue )
            CFRelease(newValue);
    }
    
    return newProperties;
}

//---------------------------------------------------------------------------
// IOHIDEventServiceClass::setProperty
//---------------------------------------------------------------------------
boolean_t IOHIDEventServiceClass::setProperty(CFStringRef key, CFTypeRef property)
{
    CFDictionaryRef floatProperties = NULL;
    boolean_t       retVal;
    
    // RY: Convert these floating point properties to IOFixed. Limiting to accel shake but can get apply to others as well
    if ( CFEqual(CFSTR(kIOHIDAccelerometerShakeKey), key) && (CFDictionaryGetTypeID() == CFGetTypeID(property)) ) {
        property = floatProperties = createFixedProperties((CFDictionaryRef)property);
    }
        
    retVal = (IORegistryEntrySetCFProperty(_service, key, property) == kIOReturnSuccess);
    
    if ( floatProperties )
        CFRelease(floatProperties);
        
    return retVal;

/*    
    if (kIOReturnSuccess != IORegistryEntrySetCFProperty(_service, key, property))
        return;

    if ( !_dynamicServiceProperties || 
         !(_dynamicServiceProperties = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)))
        return;

    CFDictionarySetValue(_dynamicServiceProperties, key, property);
*/
}

//---------------------------------------------------------------------------
// IOHIDEventServiceClass::copyEvent
//---------------------------------------------------------------------------
IOHIDEventRef IOHIDEventServiceClass::copyEvent(IOHIDEventType eventType, IOHIDEventRef matching, IOOptionBits options)
{
    const UInt8 *       inputData       = NULL;
    size_t              inputDataSize   = 0;
    UInt8 *             outputData      = NULL;
    size_t              outputDataSize  = 0;
    size_t              eventDataSize   = 0;
    CFDataRef           fieldData       = NULL;
    CFMutableDataRef    eventData       = NULL;
    IOHIDEventRef       event           = NULL;
    IOReturn            ret             = kIOReturnSuccess;
    
    if ( matching ) {
        fieldData = IOHIDEventCreateData(kCFAllocatorDefault, matching);
        
        if ( fieldData ) {
            inputData       = CFDataGetBytePtr(fieldData);
            inputDataSize   = CFDataGetLength(fieldData);
        }
    }
    
    do { 
        // Grab the actual event from the user client
        uint64_t input[2];
        
        input[0] = eventType;
        input[1] = options;
        
        IOHIDEventGetQueueElementSize(eventType, outputDataSize);
        if ( !outputDataSize )
            break;
        
        eventData = CFDataCreateMutable(kCFAllocatorDefault, outputDataSize);
        if ( !eventData )
            break;
        
        outputData = CFDataGetMutableBytePtr(eventData);
        
        if ( !outputData )
            break;
            
        CFDataSetLength(eventData, outputDataSize);
        bzero(outputData, outputDataSize);
        
        ret = IOConnectCallMethod(_connect, kIOHIDEventServiceUserClientCopyEvent, input, 2, inputData, inputDataSize, NULL, NULL, outputData, &outputDataSize); 
        if ( ret != kIOReturnSuccess || !outputDataSize)
            break;
            
        CFDataSetLength(eventData, outputDataSize);
        
        event = IOHIDEventCreateWithData(kCFAllocatorDefault, eventData);        
        
    } while ( 0 );
    
    if ( fieldData )
        CFRelease(fieldData);

    if ( eventData )
        CFRelease(eventData);
    
    return event;
}

//---------------------------------------------------------------------------
// IOHIDEventServiceClass::setElementValue
//---------------------------------------------------------------------------
void IOHIDEventServiceClass::setElementValue(uint32_t usagePage, uint32_t usage, uint32_t value)
{
    uint64_t input[3] = {usagePage, usage, value};

    IOConnectCallMethod(_connect, kIOHIDEventServiceUserClientSetElementValue, input, 3, NULL, 0, NULL, NULL, NULL, NULL); 
}


//---------------------------------------------------------------------------
// IOHIDEventServiceClass::setEventCallback
//---------------------------------------------------------------------------
void IOHIDEventServiceClass::setEventCallback(IOHIDServiceEventCallback callback, void * target, void * refcon)
{
    _eventCallback = callback;
    _eventTarget = target;
    _eventRefcon = refcon;
    
    // drain the queue just in case
    if ( _eventCallback )
        _queueEventSourceCallback(NULL, NULL, 0, this);
}

//---------------------------------------------------------------------------
// IOHIDEventServiceClass::scheduleWithRunLoop
//---------------------------------------------------------------------------
void IOHIDEventServiceClass::scheduleWithRunLoop(CFRunLoopRef runLoop, CFStringRef runLoopMode)
{
    if ( !_asyncEventSource ) {
        CFMachPortRef       cfPort;
        CFMachPortContext   context;
        Boolean             shouldFreeInfo;

        if ( !_asyncPort ) {     
            IOReturn ret = IOCreateReceivePort(kOSNotificationMessageID, &_asyncPort);
            if (kIOReturnSuccess != ret || !_asyncPort)
                return;
                
            ret = IOConnectSetNotificationPort(_connect, 0, _asyncPort, NULL);
            if ( kIOReturnSuccess != ret )
                return;
        }

        context.version = 1;
        context.info = this;
        context.retain = NULL;
        context.release = NULL;
        context.copyDescription = NULL;

        cfPort = CFMachPortCreateWithPort(NULL, _asyncPort,
                    (CFMachPortCallBack) IOHIDEventServiceClass::_queueEventSourceCallback,
                    &context, &shouldFreeInfo);
        if (!cfPort)
            return;
        
        _asyncEventSource = CFMachPortCreateRunLoopSource(NULL, cfPort, 0);
        CFRelease(cfPort);
        
        if ( !_asyncEventSource )
            return;
    }    
    CFRunLoopAddSource(runLoop, _asyncEventSource, runLoopMode);
    
    // kick him for good measure
    if ( _queueMappedMemory )
        CFRunLoopSourceSignal(_asyncEventSource);
}

//---------------------------------------------------------------------------
// IOHIDEventServiceClass::unscheduleFromRunLoop
//---------------------------------------------------------------------------
void IOHIDEventServiceClass::unscheduleFromRunLoop(CFRunLoopRef runLoop, CFStringRef runLoopMode)
{
    CFRunLoopRemoveSource(runLoop, _asyncEventSource, runLoopMode);
}

//===========================================================================
// Static Helper Definitions
//===========================================================================
IOReturn MergeDictionaries(CFDictionaryRef srcDict, CFMutableDictionaryRef * pDstDict)
{
    uint32_t        count;
    CFTypeRef *     values;
    CFStringRef *   keys;
    
    if ( !pDstDict || !srcDict || !(count = CFDictionaryGetCount(srcDict)))
        return kIOReturnBadArgument;
        
    if ( !*pDstDict || 
        !(*pDstDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)))
        return kIOReturnNoMemory;
                
    values  = (CFTypeRef *)malloc(sizeof(CFTypeRef) * count);
    keys    = (CFStringRef *)malloc(sizeof(CFStringRef) * count);
    
    for ( uint32_t i=0; i<count; i++) 
        CFDictionarySetValue(*pDstDict, keys[i], values[i]);
    
    return kIOReturnSuccess;
}
