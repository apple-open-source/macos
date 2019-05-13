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
#include <dispatch/private.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/hid/IOHIDServiceKeys.h>
#include <IOKit/hid/IOHIDKeys.h>
#include "IOHIDevicePrivateKeys.h"
#include "IOHIDPrivateKeys.h"

#if TARGET_OS_EMBEDDED // {
#include <IOKit/hid/AppleEmbeddedHIDKeys.h>
#endif // } TARGET_OS_EMBEDDED

__BEGIN_DECLS
#include <asl.h>
#include <mach/mach.h>
#include <mach/mach_interface.h>
#include <IOKit/IOMessage.h>
#include <mach/mach_time.h>
__END_DECLS

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
    &IOHIDEventServiceClass::_copyProperty,
    &IOHIDEventServiceClass::_setProperty,
    &IOHIDEventServiceClass::_setEventCallback,
    &IOHIDEventServiceClass::_scheduleWithDispatchQueue,
    &IOHIDEventServiceClass::_unscheduleFromDispatchQueue,
    &IOHIDEventServiceClass::_copyEvent,
    &IOHIDEventServiceClass::_setOutputEvent
};




static PROPERTY_INFO PropertyInfoTable [] = {
    {
        CFSTR(kIOHIDKeyboardModifierMappingPairsKey),
        (kPropertyInfoCache | kPropertyInfoMutable)
    },
    {
        CFSTR(kIOHIDTransportKey),
        kPropertyInfoCache
    },
    {
        CFSTR(kIOHIDVendorIDKey),
        kPropertyInfoCache
    },
    {
        CFSTR(kIOHIDProductIDKey),
        kPropertyInfoCache
    },
    {
        CFSTR(kIOHIDVersionNumberKey),
        kPropertyInfoCache
    },
    {
        CFSTR(kIOHIDManufacturerKey),
        kPropertyInfoCache
    },
    {
        CFSTR(kIOHIDProductKey),
        kPropertyInfoCache
    },
    {
        CFSTR(kIOHIDSerialNumberKey),
        kPropertyInfoCache
    },
    {
        CFSTR(kIOHIDCountryCodeKey),
        kPropertyInfoCache
    },
    {
        CFSTR(kIOHIDLocationIDKey),
        kPropertyInfoCache
    },
    {
        CFSTR(kIOHIDPrimaryUsagePageKey),
        kPropertyInfoCache
    },
    {
        CFSTR(kIOHIDPrimaryUsageKey),
        kPropertyInfoCache
    },
    {
        CFSTR(kIOHIDDeviceUsagePairsKey),
        kPropertyInfoCache
    },
    {
        CFSTR(kIOHIDScrollAccelerationTypeKey),
        kPropertyInfoProviderOnly
    },
    {
        CFSTR(kIOHIDPointerAccelerationTypeKey),
        kPropertyInfoProviderOnly
    },
    {
        CFSTR(kIOHIDPointerAccelerationTableKey),
        kPropertyInfoProviderOnly
    },
    {
        CFSTR(kIOHIDScrollAccelerationTableKey),
        kPropertyInfoProviderOnly
    },
    {
        CFSTR(kIOHIDScrollAccelerationTableXKey),
        kPropertyInfoProviderOnly
    },
    {
        CFSTR(kIOHIDScrollAccelerationTableYKey),
        kPropertyInfoProviderOnly
    },
    {
        CFSTR(kIOHIDScrollAccelerationTableZKey),
        kPropertyInfoProviderOnly
    },
    {
        CFSTR(kIOHIDScrollAccelerationTableZKey),
        kPropertyInfoProviderOnly
    },
    {
        CFSTR(kIOHIDResetStickyKeyNotification),
        kPropertyNotification
    },
    {
        CFSTR(kIOHIDBuiltInKey),
        kPropertyInfoProviderOnly
    },
    {
        CFSTR(kIOHIDEventServiceQueueSize),
        kPropertyInfoCache
    }
};

//===========================================================================
// CONSTRUCTOR / DESTRUCTOR methods
//===========================================================================
//---------------------------------------------------------------------------
// IOHIDEventServiceClass
//---------------------------------------------------------------------------
IOHIDEventServiceClass::IOHIDEventServiceClass() : IOHIDIUnknown(&sIOCFPlugInInterfaceV1)
{
    _hidService.pseudoVTable    = NULL;
    _hidService.obj             = this;
    
    _service                    = MACH_PORT_NULL;
    _connect                    = MACH_PORT_NULL;
    _isOpen                     = FALSE;

    _asyncPort                  = MACH_PORT_NULL;
    _asyncEventSource           = NULL;
    
    _serviceProperties          = NULL;
    _servicePreferences         = NULL;
    _eventCallback              = NULL;
    _eventTarget                = NULL;
    _eventRefcon                = NULL;
    _queueMappedMemory          = NULL;
    _queueMappedMemorySize      = 0;    
}

//---------------------------------------------------------------------------
// ~IOHIDEventServiceClass
//---------------------------------------------------------------------------
IOHIDEventServiceClass::~IOHIDEventServiceClass()
{
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

    if (_servicePreferences) {
        CFRelease(_servicePreferences);
        _servicePreferences = NULL;
    }
    
    if ( _asyncPort ) {
        mach_port_mod_refs(mach_task_self(), _asyncPort, MACH_PORT_RIGHT_RECEIVE, -1);
        _asyncPort = MACH_PORT_NULL;
    }
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

CFTypeRef IOHIDEventServiceClass::_copyProperty(void * self, CFStringRef key)
{
    return getThis(self)->copyProperty(key);
}

boolean_t IOHIDEventServiceClass::_setProperty(void * self, CFStringRef key, CFTypeRef property)
{
    return getThis(self)->setProperty(key, property);
}

IOHIDEventRef IOHIDEventServiceClass::_copyEvent(void *self, IOHIDEventType type, IOHIDEventRef matching, IOOptionBits options)
{
    return getThis(self)->copyEvent(type, matching, options);
}

IOReturn IOHIDEventServiceClass::_setOutputEvent(void *self, IOHIDEventRef event)
{
    return getThis(self)->setOutputEvent(event);
}

void IOHIDEventServiceClass::_setEventCallback(void * self, IOHIDServiceEventCallback callback, void * target, void * refcon)
{
    getThis(self)->setEventCallback(callback, target, refcon);
}

void IOHIDEventServiceClass::_scheduleWithDispatchQueue(void *self, dispatch_queue_t queue)
{
    return getThis(self)->scheduleWithDispatchQueue(queue);
}

void IOHIDEventServiceClass::_unscheduleFromDispatchQueue(void *self, dispatch_queue_t queue)
{
    return getThis(self)->unscheduleFromDispatchQueue(queue);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEventServiceClass::_queueEventSourceCallback
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void IOHIDEventServiceClass::_queueEventSourceCallback(void * info)
{
    IOHIDEventServiceClass * self = (IOHIDEventServiceClass*)info;
    
    mach_msg_size_t size = sizeof(mach_msg_header_t) + MAX_TRAILER_SIZE;
    mach_msg_header_t *msg = (mach_msg_header_t *)CFAllocatorAllocate(kCFAllocatorDefault, size, 0);
    msg->msgh_size = size;
    for (;;) {
        msg->msgh_bits = 0;
        msg->msgh_local_port = self->_asyncPort;
        msg->msgh_remote_port = MACH_PORT_NULL;
        msg->msgh_id = 0;
        kern_return_t ret = mach_msg(msg, MACH_RCV_MSG|MACH_RCV_LARGE|MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0)|MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AV), 0, msg->msgh_size, self->_asyncPort, 0, MACH_PORT_NULL);
        if (MACH_MSG_SUCCESS == ret) break;
        if (MACH_RCV_TOO_LARGE != ret) goto user_exit;
        uint32_t newSize = round_msg(msg->msgh_size + MAX_TRAILER_SIZE);
        msg = (mach_msg_header_t*)CFAllocatorReallocate(kCFAllocatorDefault, msg, newSize, 0);
        msg->msgh_size = newSize;
    }
    
    self->dequeueHIDEvents();
    
user_exit:
    CFAllocatorDeallocate(kCFAllocatorSystemDefault, msg);
}

//------------------------------------------------------------------------------
// IOHIDEventServiceClass::dequeueHIDEvents
//------------------------------------------------------------------------------
void IOHIDEventServiceClass::dequeueHIDEvents(boolean_t suppress)
{
    do {
        if ( !_queueMappedMemory )
            break;

        // check entry size
        IODataQueueEntry *  nextEntry;
        uint32_t            dataSize;

        // if queue empty, then stop
        while ((nextEntry = IODataQueuePeek(_queueMappedMemory))) {
            if ( !suppress ) {
                IOHIDEventRef event = IOHIDEventCreateWithBytes(kCFAllocatorDefault, (const UInt8*)&(nextEntry->data), nextEntry->size);

                if ( event ) {
                    dispatchHIDEvent(event);
                    CFRelease(event);
                }
            }
            
            // dequeue the item
            dataSize = 0;
            IODataQueueDequeue(_queueMappedMemory, NULL, &dataSize);
        }
    } while ( 0 );
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
    else if (CFEqual(uuid, kIOHIDServiceInterface2ID))
    {
        _hidService.pseudoVTable    = (IUnknownVTbl *)  &sIOHIDServiceInterface2;
        _hidService.obj             = this;
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
IOReturn IOHIDEventServiceClass::probe(CFDictionaryRef propertyTable __unused, io_service_t service, SInt32 * order __unused)
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
IOReturn IOHIDEventServiceClass::start(CFDictionaryRef propertyTable __unused, io_service_t service)
{
    IOReturn                ret             = kIOReturnError;
    
    do {
        _service = service;
        IOObjectRetain(_service);
      
        _serviceProperties = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if ( !_serviceProperties ) {
            ret = kIOReturnNoMemory;
            break;
        }
//
//        IORegistryEntryCreateCFProperties(service, &serviceProps, kCFAllocatorDefault, 0);
//
//        if ( !serviceProps )
//            break;
//            
//        GET_AND_SET_PROPERTY(serviceProps, CFSTR(kIOHIDTransportKey), _serviceProperties, CFSTR(kIOHIDServiceTransportKey));
//        GET_AND_SET_PROPERTY(serviceProps, CFSTR(kIOHIDVendorIDKey), _serviceProperties, CFSTR(kIOHIDServiceVendorIDKey));
//        GET_AND_SET_PROPERTY(serviceProps, CFSTR(kIOHIDVendorIDSourceKey), _serviceProperties, CFSTR(kIOHIDServiceVendorIDSourceKey));
//        GET_AND_SET_PROPERTY(serviceProps, CFSTR(kIOHIDProductIDKey), _serviceProperties, CFSTR(kIOHIDServiceProductIDKey));
//        GET_AND_SET_PROPERTY(serviceProps, CFSTR(kIOHIDVersionNumberKey), _serviceProperties, CFSTR(kIOHIDServiceVersionNumberKey));
//        GET_AND_SET_PROPERTY(serviceProps, CFSTR(kIOHIDManufacturerKey), _serviceProperties, CFSTR(kIOHIDServiceManufacturerKey));
//        GET_AND_SET_PROPERTY(serviceProps, CFSTR(kIOHIDProductKey), _serviceProperties, CFSTR(kIOHIDServiceProductKey));
//        GET_AND_SET_PROPERTY(serviceProps, CFSTR(kIOHIDSerialNumberKey), _serviceProperties, CFSTR(kIOHIDServiceSerialNumberKey));
//        GET_AND_SET_PROPERTY(serviceProps, CFSTR(kIOHIDCountryCodeKey), _serviceProperties, CFSTR(kIOHIDServiceCountryCodeKey));
//        GET_AND_SET_PROPERTY(serviceProps, CFSTR(kIOHIDLocationIDKey), _serviceProperties, CFSTR(kIOHIDServiceLocationIDKey));
//        GET_AND_SET_PROPERTY(serviceProps, CFSTR(kIOHIDPrimaryUsagePageKey), _serviceProperties, CFSTR(kIOHIDServicePrimaryUsagePageKey));
//        GET_AND_SET_PROPERTY(serviceProps, CFSTR(kIOHIDPrimaryUsageKey), _serviceProperties, CFSTR(kIOHIDServicePrimaryUsageKey));
//        GET_AND_SET_PROPERTY(serviceProps, CFSTR(kIOHIDDeviceUsagePairsKey), _serviceProperties, CFSTR(kIOHIDServiceDeviceUsagePairsKey));
//// This should be considered a dymanic property
////        GET_AND_SET_PROPERTY(serviceProps, CFSTR(kIOHIDReportIntervalKey), _serviceProperties, CFSTR(kIOHIDServiceReportIntervalKey));
//
//        CFRelease(serviceProps);
        
        // Establish connection with device
        ret = IOServiceOpen(_service, mach_task_self(), kIOHIDEventServiceUserClientType, &_connect);
        if (ret != kIOReturnSuccess || !_connect)
            break;

        boolean_t createQueue = true;
        CFNumberRef queueSize = (CFNumberRef)IORegistryEntryCreateCFProperty (_service, CFSTR(kIOHIDEventServiceQueueSize), kCFAllocatorDefault, 0);
        if (queueSize) {
            uint32_t value = 0;
            CFNumberGetValue (queueSize, kCFNumberSInt32Type, &value);
            if (value == 0) {
               createQueue =  false;
            }
            CFDictionarySetValue(_serviceProperties, CFSTR(kIOHIDEventServiceQueueSize), queueSize);
            CFRelease(queueSize);
        }
        
        if (createQueue) {
            // allocate the memory
#if !__LP64__
            vm_address_t        address = static_cast<vm_address_t>(0);
            vm_size_t           size    = 0;
#else
            mach_vm_address_t   address = static_cast<mach_vm_address_t>(0);
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
            
            if (!_queueMappedMemory || !_queueMappedMemorySize ) {
                break;
            }
        }
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
    
    if ( !_isOpen ) {
            
        do {
            kr = IOConnectCallScalarMethod(_connect, kIOHIDEventServiceUserClientOpen, &input, 1, 0, &len);; 
            if ( kr != kIOReturnSuccess ) {
                ret = false;
                break;
            }

            _isOpen = true;
            
        } while ( 0 );
    }
                                                        
    return ret;
}

//---------------------------------------------------------------------------
// IOHIDEventServiceClass::close
//---------------------------------------------------------------------------
void IOHIDEventServiceClass::close(IOOptionBits options)
{
    uint32_t len = 0;
    uint64_t input = options;
        
    if ( _isOpen ) {
        (void) IOConnectCallScalarMethod(_connect, kIOHIDEventServiceUserClientClose, &input, 1, 0, &len); 
        
        // drain the queue just in case
        if ( _eventCallback )
            dequeueHIDEvents(true);
		
        _isOpen = false;
    }
}

//---------------------------------------------------------------------------
// IOHIDEventServiceClass::copyProperty
//---------------------------------------------------------------------------
CFTypeRef IOHIDEventServiceClass::copyProperty(CFStringRef key)
{
  
    //collect debug info
    if (CFEqual(key, CFSTR(kIOHIDServicePluginDebugKey))) {
        CFStringRef class_key = CFSTR ("Class");
        CFStringRef class_value = CFSTR ("IOHIDEventServiceClass");
        CFDictionaryRef debug = CFDictionaryCreate(kCFAllocatorDefault, (const void**)&class_key, (const void**)&class_value, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        return debug;
    }
    CFTypeRef value = CFDictionaryGetValue(_serviceProperties, key);
    if ( value ) {
        CFRetain(value);
    } else {
        CFDictionaryRef sericePropertyCache = (CFDictionaryRef)IORegistryEntryCreateCFProperty (_service, CFSTR(kIOHIDEventServicePropertiesKey), kCFAllocatorDefault, 0);
        if (sericePropertyCache) {
            if (CFGetTypeID(sericePropertyCache) == CFDictionaryGetTypeID()) {
                value = CFDictionaryGetValue(sericePropertyCache, key);
                if (value) {
                  CFRetain(value);
                }
            }
            CFRelease(sericePropertyCache);
        }
        
        PROPERTY_INFO* propertyInfo = getPropertyInfo(key);
        
        if (!value) {
            if  (propertyInfo && propertyInfo->flags & kPropertyInfoProviderOnly) {
                
                value = (CFDictionaryRef)IORegistryEntryCreateCFProperty (_service, key, kCFAllocatorDefault, 0);
            
            } else {
                
                value = IORegistryEntrySearchCFProperty(_service, kIOServicePlane, key, kCFAllocatorDefault, kIORegistryIterateRecursively| kIORegistryIterateParents);
            
            }
        }
        if (value) {
            //
            // Push back to cache if applicable for property
            //
            if (propertyInfo && propertyInfo->flags & kPropertyInfoCache) {
                // It's unsafe to cache the key that was passed in. Cache our copy.
                // <rdar://problem/42592555>
                CFDictionarySetValue(_serviceProperties, propertyInfo->key, value);
            }
            
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
  
    PROPERTY_INFO *propertyInfo = getPropertyInfo(key);
    if (propertyInfo && (propertyInfo->flags & kPropertyNotification)) {
        return true;
    }
    if ( propertyInfo && (propertyInfo->flags & kPropertyInfoCache)) {
        // It's unsafe to cache the key that was passed in. Cache our copy.
        // <rdar://problem/42592555>
        CFDictionarySetValue(_serviceProperties, propertyInfo->key, property);
    }
#if TARGET_OS_EMBEDDED // {
    // RY: Convert these floating point properties to IOFixed. Limiting to accel shake but can get apply to others as well
    if ( CFEqual(CFSTR(kIOHIDAccelerometerShakeKey), key) && (CFDictionaryGetTypeID() == CFGetTypeID(property)) ) {
        property = floatProperties = createFixedProperties((CFDictionaryRef)property);
    }
#endif // } TARGET_OS_EMBEDDED
        
    retVal = (IORegistryEntrySetCFProperty(_service, key, property) == kIOReturnSuccess);
    
    if ( floatProperties )
        CFRelease(floatProperties);
        
    return retVal;
}

//---------------------------------------------------------------------------
// IOHIDEventServiceClass::getPropertyInfo
//---------------------------------------------------------------------------
PROPERTY_INFO* IOHIDEventServiceClass::getPropertyInfo(CFStringRef key) {
  for (size_t index = 0; index < sizeof(PropertyInfoTable)/sizeof(PropertyInfoTable[0]); index++) {
    if (CFEqual (PropertyInfoTable[index].key, key)) {
      return &PropertyInfoTable[index];
    }
  }
  return NULL;
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
        
        if ( matching &&  IOHIDEventGetType(matching) == kIOHIDEventTypeVendorDefined) {
            outputDataSize += IOHIDEventGetIntegerValue(matching, kIOHIDEventFieldVendorDefinedDataLength);
        }
        
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
// IOHIDEventServiceClass::setOutputEvent
//---------------------------------------------------------------------------
IOReturn IOHIDEventServiceClass::setOutputEvent(IOHIDEventRef event)
{
    IOReturn result = kIOReturnUnsupported;
    CFIndex  LEDUsage = IOHIDEventGetIntegerValue(event, kIOHIDEventFieldLEDNumber);
    if (IOHIDEventGetType(event) == kIOHIDEventTypeLED ) {
        uint64_t input[3] = {kHIDPage_LEDs, (uint64_t)LEDUsage, (uint64_t)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldLEDState)};
        result = IOConnectCallMethod(_connect, kIOHIDEventServiceUserClientSetElementValue, input, 3, NULL, 0, NULL, NULL, NULL, NULL);
        if (result == kIOReturnUnsupported && LEDUsage >= kHIDUsage_LED_Player1 && LEDUsage <= kHIDUsage_LED_Player8) {
            input[1] = 0xff00 | IOHIDEventGetIntegerValue(event, kIOHIDEventFieldLEDNumber) - kHIDUsage_LED_Player1;
            result = IOConnectCallMethod(_connect, kIOHIDEventServiceUserClientSetElementValue, input, 3, NULL, 0, NULL, NULL, NULL, NULL);
        }
    }
    return result;
}


//---------------------------------------------------------------------------
// IOHIDEventServiceClass::setEventCallback
//---------------------------------------------------------------------------
void IOHIDEventServiceClass::setEventCallback(IOHIDServiceEventCallback callback, void * target, void * refcon)
{
    _eventCallback  = callback;
    _eventTarget    = target;
    _eventRefcon    = refcon;
}

//---------------------------------------------------------------------------
// IOHIDEventServiceClass::scheduleWithDispatchQueue
//---------------------------------------------------------------------------
void IOHIDEventServiceClass::scheduleWithDispatchQueue(dispatch_queue_t dispatchQueue)
{
    if ( !_asyncPort ) {
        _asyncPort = IODataQueueAllocateNotificationPort();
        if (!_asyncPort)
            return;
            
        IOReturn ret = IOConnectSetNotificationPort(_connect, 0, _asyncPort, NULL);
        if ( kIOReturnSuccess != ret )
            return;
    }
    
    if ( !_asyncEventSource ) {
        _asyncEventSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, _asyncPort, 0, dispatchQueue);
        
        if ( !_asyncEventSource ) {
            return;
        }
        dispatch_set_context(_asyncEventSource, this);
        dispatch_source_set_event_handler_f(_asyncEventSource, _queueEventSourceCallback);
    }
    dispatch_resume(_asyncEventSource);
    
    dispatch_async(dispatchQueue, ^{
        dequeueHIDEvents();
    });    
}

//---------------------------------------------------------------------------
// IOHIDEventServiceClass::unscheduleFromDispatchQueue
//---------------------------------------------------------------------------
void IOHIDEventServiceClass::unscheduleFromDispatchQueue(dispatch_queue_t queue __unused)
{
    if ( _asyncEventSource ) {
        dispatch_source_cancel (_asyncEventSource);
        dispatch_release(_asyncEventSource);
    }
}

