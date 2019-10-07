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
 
#include "IOHIDNXEventRouter.h"
#include "IOHIDEventServiceUserClient.h"
#include "IOHIDEventData.h"
#include <dispatch/private.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/hid/IOHIDServiceKeys.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDLibPrivate.h>
#include "IOHIDDebug.h"


__BEGIN_DECLS
#include <asl.h>
#include <mach/mach.h>
#include <mach/mach_interface.h>
#include <IOKit/IOMessage.h>
#include <mach/mach_time.h>
__END_DECLS


extern "C" void * IOHIDNXEventRouterFactory(CFAllocatorRef allocator __unused, CFUUIDRef typeUUID);

//===========================================================================
// Static Helper Declarations
//===========================================================================
void *IOHIDNXEventRouterFactory(CFAllocatorRef allocator __unused, CFUUIDRef typeID)
{
  if (CFEqual(typeID, kIOHIDServicePlugInTypeID))
        return (void *) IOHIDNXEventRouter::alloc();
  return NULL;
}



//===========================================================================
// CFPlugIn Static Assignments
//===========================================================================
IOCFPlugInInterface IOHIDNXEventRouter::sIOCFPlugInInterfaceV1 =
{
    0,
    &IOHIDIUnknown::genericQueryInterface,
    &IOHIDIUnknown::genericAddRef,
    &IOHIDIUnknown::genericRelease,
    1, 0,	// version/revision
    &IOHIDNXEventRouter::_probe,
    &IOHIDNXEventRouter::_start,
    &IOHIDNXEventRouter::_stop
};

IOHIDServiceInterface2 IOHIDNXEventRouter::sIOHIDServiceInterface2 =
{
    0,
    &IOHIDIUnknown::genericQueryInterface,
    &IOHIDIUnknown::genericAddRef,
    &IOHIDIUnknown::genericRelease,
    &IOHIDNXEventRouter::_open,
    &IOHIDNXEventRouter::_close,
    &IOHIDNXEventRouter::_copyProperty,
    &IOHIDNXEventRouter::_setProperty,
    &IOHIDNXEventRouter::_setEventCallback,
    &IOHIDNXEventRouter::_scheduleWithDispatchQueue,
    &IOHIDNXEventRouter::_unscheduleFromDispatchQueue,
    &IOHIDNXEventRouter::_copyEvent,
    &IOHIDNXEventRouter::_setOutputEvent
};

//===========================================================================
// CONSTRUCTOR / DESTRUCTOR methods
//===========================================================================
//---------------------------------------------------------------------------
// IOHIDNXEventRouter
//---------------------------------------------------------------------------
IOHIDNXEventRouter::IOHIDNXEventRouter() : IOHIDIUnknown(&sIOCFPlugInInterfaceV1)
{
    _hidService.pseudoVTable    = NULL;
    _hidService.obj             = this;
    
    _service                    = MACH_PORT_NULL;
    _isOpen                     = FALSE;

    _asyncPort                  = MACH_PORT_NULL;
    _asyncEventSource           = NULL;
    
    _serviceProperties          = NULL;
    _servicePreferences         = NULL;
    _eventCallback              = NULL;
    _eventTarget                = NULL;
    _eventRefcon                = NULL;
    
    queue_                      = NULL;
}

//---------------------------------------------------------------------------
// ~IOHIDNXEventRouter
//---------------------------------------------------------------------------
IOHIDNXEventRouter::~IOHIDNXEventRouter()
{
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
  
  
    if (queue_) {
      CFRelease(queue_);
      queue_ = NULL;
    }
}

//===========================================================================
// IOCFPlugInInterface methods
//===========================================================================
IOReturn IOHIDNXEventRouter::_probe(void *self, CFDictionaryRef propertyTable, io_service_t service, SInt32 *order)
{
    return getThis(self)->probe(propertyTable, service, order);
}

IOReturn IOHIDNXEventRouter::_start(void *self, CFDictionaryRef propertyTable, io_service_t service)
{
    return getThis(self)->start(propertyTable, service);
}

IOReturn IOHIDNXEventRouter::_stop(void *self)
{
    return getThis(self)->stop();
}

boolean_t IOHIDNXEventRouter::_open(void * self, IOOptionBits options)
{
    return getThis(self)->open(options);
}

void IOHIDNXEventRouter::_close(void * self, IOOptionBits options)
{
    getThis(self)->close(options);
}

CFTypeRef IOHIDNXEventRouter::_copyProperty(void * self, CFStringRef key)
{
    return getThis(self)->copyProperty(key);
}

boolean_t IOHIDNXEventRouter::_setProperty(void * self, CFStringRef key, CFTypeRef property)
{
    return getThis(self)->setProperty(key, property);
}

IOHIDEventRef IOHIDNXEventRouter::_copyEvent(void *self, IOHIDEventType type, IOHIDEventRef matching, IOOptionBits options)
{
    return getThis(self)->copyEvent(type, matching, options);
}

IOReturn IOHIDNXEventRouter::_setOutputEvent(void *self, IOHIDEventRef event)
{
    return getThis(self)->setOutputEvent(event);
}

void IOHIDNXEventRouter::_setEventCallback(void * self, IOHIDServiceEventCallback callback, void * target, void * refcon)
{
    getThis(self)->setEventCallback(callback, target, refcon);
}

void IOHIDNXEventRouter::_scheduleWithDispatchQueue(void *self, dispatch_queue_t queue)
{
    return getThis(self)->scheduleWithDispatchQueue(queue);
}

void IOHIDNXEventRouter::_unscheduleFromDispatchQueue(void *self, dispatch_queue_t queue)
{
    return getThis(self)->unscheduleFromDispatchQueue(queue);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDNXEventRouter::_queueEventSourceCallback
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void IOHIDNXEventRouter::_queueEventSourceCallback(void * info)
{
    IOHIDNXEventRouter * self = (IOHIDNXEventRouter*)info;
    
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
    
    //self->dequeueHIDEvents();
    IOHIDNXEventRouter::_queueCallback(info);
user_exit:
    CFAllocatorDeallocate(kCFAllocatorSystemDefault, msg);
}


//------------------------------------------------------------------------------
// IOHIDNXEventRouter::dispatchHIDEvent
//------------------------------------------------------------------------------
void IOHIDNXEventRouter::dispatchHIDEvent(IOHIDEventRef event, IOOptionBits options)
{
    if ( !_eventCallback )
        return;
        
    (*_eventCallback)(_eventTarget, _eventRefcon, (void *)&_hidService, event, options);
}



// Public Methods
//---------------------------------------------------------------------------
// IOHIDNXEventRouter::alloc
//---------------------------------------------------------------------------
IOCFPlugInInterface ** IOHIDNXEventRouter::alloc()
{
 
    IOHIDNXEventRouter * self = new IOHIDNXEventRouter;

    //HIDLogDebug("+{%p}", self);
   
    return self ? (IOCFPlugInInterface **) &self->iunknown.pseudoVTable : NULL;
}

//---------------------------------------------------------------------------
// IOHIDNXEventRouter::queryInterface
//---------------------------------------------------------------------------
HRESULT IOHIDNXEventRouter::queryInterface(REFIID iid, void **ppv)
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
// IOHIDNXEventRouter::probe
//---------------------------------------------------------------------------
IOReturn IOHIDNXEventRouter::probe(CFDictionaryRef propertyTable __unused, io_service_t service, SInt32 * order __unused)
{
    if (!service || !IOObjectConformsTo(service, "IOHIDSystem"))
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
// IOHIDNXEventRouter::start
//---------------------------------------------------------------------------
IOReturn IOHIDNXEventRouter::start(CFDictionaryRef propertyTable __unused, io_service_t service)
{
    IOReturn                ret             = kIOReturnError;
    CFMutableDictionaryRef  serviceProps    = NULL;
  
    do {
    
        ret = IOObjectRetain(service);
        if (ret != kIOReturnSuccess) {
            HIDLogError("IOHIDNXEventRouter failed to retain service object err : 0x%x",ret);
            break;
        }
        _service = service;
        
        _serviceProperties = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        
        if ( !_serviceProperties ) {
            ret = kIOReturnNoMemory;
            break;
        }

        IORegistryEntryCreateCFProperties(service, &serviceProps, kCFAllocatorDefault, 0);

        if ( !serviceProps )
            break;
            
        GET_AND_SET_PROPERTY(serviceProps, CFSTR(kIOHIDServiceGlobalModifiersUsageKey), _serviceProperties, CFSTR(kIOHIDServiceGlobalModifiersUsageKey));
        GET_AND_SET_PROPERTY(serviceProps, CFSTR(kIOHIDPrimaryUsagePageKey), _serviceProperties, CFSTR(kIOHIDServicePrimaryUsagePageKey));
        GET_AND_SET_PROPERTY(serviceProps, CFSTR(kIOHIDPrimaryUsageKey), _serviceProperties, CFSTR(kIOHIDServicePrimaryUsageKey));
        GET_AND_SET_PROPERTY(serviceProps, CFSTR(kIOHIDDeviceUsagePairsKey), _serviceProperties, CFSTR(kIOHIDServiceDeviceUsagePairsKey));

        CFRelease(serviceProps);
        
        queue_ = IOHIDEventQueueCreate (kCFAllocatorDefault, kIOHIDEventQueueTypeKernel, 1024 * 16, 0);
      
        if (queue_ == NULL) {
          ret = kIOReturnNoMemory;
          break;
        }
      
        return  kIOReturnSuccess;
        
    } while (0);
    
    if ( _service ) {
        IOObjectRelease(_service);
        _service = NULL;
    }
    return ret;
}

//---------------------------------------------------------------------------
// IOHIDNXEventRouter::stop
//---------------------------------------------------------------------------
IOReturn IOHIDNXEventRouter::stop()
{
    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------
// IOHIDNXEventRouter::open
//---------------------------------------------------------------------------
boolean_t IOHIDNXEventRouter::open(IOOptionBits options __unused)
{
 
    bool     ret = true;
    
    if ( !_isOpen ) {
        IOHIDEventQueueStart(queue_);
        _isOpen = true;
    }

    return ret;
}

//---------------------------------------------------------------------------
// IOHIDNXEventRouter::close
//---------------------------------------------------------------------------
void IOHIDNXEventRouter::close(IOOptionBits options __unused)
{
  
    if ( _isOpen ) {
        IOHIDEventQueueStop(queue_);
        _isOpen = false;
    }
}

//---------------------------------------------------------------------------
// IOHIDNXEventRouter::copyProperty
//---------------------------------------------------------------------------
CFTypeRef IOHIDNXEventRouter::copyProperty(CFStringRef key)
{
    CFTypeRef value = CFDictionaryGetValue(_serviceProperties, key);
    
    if ( value ) {
        CFRetain(value);
    } else {
        value = IORegistryEntrySearchCFProperty(_service, kIOServicePlane, key, kCFAllocatorDefault, kIORegistryIterateRecursively| kIORegistryIterateParents);
    }

    return value;
}


//---------------------------------------------------------------------------
// IOHIDNXEventRouter::setProperty
//---------------------------------------------------------------------------
boolean_t IOHIDNXEventRouter::setProperty(CFStringRef key, CFTypeRef property)
{
    boolean_t       retVal = true;
  
    CFDictionarySetValue(_serviceProperties, key, property);
  
    if (CFEqual(key, CFSTR(kIOHIDKeyboardGlobalModifiersKey))) {
        retVal = (IORegistryEntrySetCFProperty(_service, key, property) == kIOReturnSuccess);
    }
    
    return retVal;
}

//---------------------------------------------------------------------------
// IOHIDNXEventRouter::copyEvent
//---------------------------------------------------------------------------
IOHIDEventRef IOHIDNXEventRouter::copyEvent(IOHIDEventType eventType __unused, IOHIDEventRef matching __unused, IOOptionBits options __unused)
{
    IOHIDEventRef       event           = NULL;
    return event;
}

//---------------------------------------------------------------------------
// IOHIDNXEventRouter::setOutputEvent
//---------------------------------------------------------------------------
IOReturn IOHIDNXEventRouter::setOutputEvent(IOHIDEventRef event __unused)
{
    IOReturn result = kIOReturnUnsupported;
    return result;
}


//---------------------------------------------------------------------------
// IOHIDNXEventRouter::setEventCallback
//---------------------------------------------------------------------------
void IOHIDNXEventRouter::setEventCallback(IOHIDServiceEventCallback callback, void * target, void * refcon)
{
    _eventCallback  = callback;
    _eventTarget    = target;
    _eventRefcon    = refcon;
}

//---------------------------------------------------------------------------
// IOHIDNXEventRouter::scheduleWithDispatchQueue
//---------------------------------------------------------------------------
void IOHIDNXEventRouter::scheduleWithDispatchQueue(dispatch_queue_t dispatchQueue)
{
    if ( !_asyncPort ) {
        _asyncPort = IODataQueueAllocateNotificationPort();
        if (!_asyncPort) {
            return;
        }
        IOHIDEventQueueSetNotificationPort (queue_, _asyncPort);
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
        _queueCallback(this);
    });    
}

//---------------------------------------------------------------------------
// IOHIDNXEventRouter::unscheduleFromDispatchQueue
//---------------------------------------------------------------------------
void IOHIDNXEventRouter::unscheduleFromDispatchQueue(dispatch_queue_t queue __unused)
{
    if ( _asyncEventSource ) {
        dispatch_source_cancel(_asyncEventSource);
        dispatch_release(_asyncEventSource);
        _asyncEventSource = NULL;
    }
}

//---------------------------------------------------------------------------
// IOHIDNXEventRouter::_queueCallback
//---------------------------------------------------------------------------
void IOHIDNXEventRouter::_queueCallback(void * info)
{
    IOHIDEventRef               event;
    IOHIDNXEventRouter *self = (IOHIDNXEventRouter*)info;
    if ( !info )
        return;
    while ( (event = IOHIDEventQueueDequeueCopy(self->queue_)) ) {
        self->dispatchHIDEvent(event);
        CFRelease(event);
    }
}
