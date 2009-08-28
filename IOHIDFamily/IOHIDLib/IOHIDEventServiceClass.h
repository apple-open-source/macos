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
#ifndef _IOKIT_IOHIDEVENTSERVICECLASS_H
#define _IOKIT_IOHIDEVENTSERVICECLASS_H

#include <pthread.h>
#include <IOKit/hid/IOHIDServicePlugIn.h>
#include <IOKit/IODataQueueClient.h>
#include "IOHIDIUnknown.h"

class IOHIDEventServiceClass : public IOHIDIUnknown
{
private:
    // Disable copy constructors
    IOHIDEventServiceClass(IOHIDEventServiceClass &src);
    void operator =(IOHIDEventServiceClass &src);

protected:
    IOHIDEventServiceClass();
    virtual ~IOHIDEventServiceClass();

    static IOCFPlugInInterface          sIOCFPlugInInterfaceV1;
    static IOHIDServiceInterface2       sIOHIDServiceInterface2;

    struct InterfaceMap                 _hidService;
    io_service_t                        _service;
    io_connect_t                        _connect;
    bool                                _isOpen;
    
    mach_port_t                         _asyncPort;

    CFRunLoopSourceRef                  _asyncEventSource;

    CFMutableDictionaryRef              _serviceProperties;
    CFMutableDictionaryRef              _dynamicServiceProperties;
    CFMutableDictionaryRef              _servicePreferences;
        
    IOHIDServiceEventCallback           _eventCallback;
    void *                              _eventTarget;
    void *                              _eventRefcon;

    IODataQueueMemory *                 _queueMappedMemory;
    vm_size_t                           _queueMappedMemorySize;
    
    bool                                _queueBusy;
    pthread_mutex_t                     _queueLock;
    pthread_cond_t                      _queueCondition;
    

    static inline IOHIDEventServiceClass *getThis(void *self) { return (IOHIDEventServiceClass *)((InterfaceMap *) self)->obj; };

    // IOCFPlugInInterface methods
    static IOReturn _probe(void *self, CFDictionaryRef propertyTable, io_service_t service, SInt32 *order);
    static IOReturn _start(void *self, CFDictionaryRef propertyTable, io_service_t service);
    static IOReturn _stop(void *self);

    // IOHIDServiceInterface2 methods
    static boolean_t        _open(void *self, IOOptionBits options);
    static void             _close(void *self, IOOptionBits options);
    static CFTypeRef        _getProperty(void *self, CFStringRef key);
    static boolean_t        _setProperty(void *self, CFStringRef key, CFTypeRef property);
    static IOHIDEventRef    _copyEvent(void *self, IOHIDEventType type, IOHIDEventRef matching, IOOptionBits options);
    static void             _setEventCallback(void *self, IOHIDServiceEventCallback callback, void * target, void * refcon);
    static void             _scheduleWithRunLoop(void *self, CFRunLoopRef runLoop, CFStringRef runLoopMode);
    static void             _unscheduleFromRunLoop(void *self, CFRunLoopRef runLoop, CFStringRef runLoopMode);
    
    // Support methods
    static void             _queueEventSourceCallback(
                                            CFMachPortRef               cfPort, 
                                            mach_msg_header_t *         msg, 
                                            CFIndex                     size, 
                                            void *                      info);
    void                    dispatchHIDEvent(IOHIDEventRef event, IOOptionBits options=0);

    CFDictionaryRef         createFixedProperties(CFDictionaryRef floatProperties);
public:
    // IOCFPlugin stuff
    static IOCFPlugInInterface **alloc();

    virtual HRESULT         queryInterface(REFIID iid, void **ppv);
    virtual IOReturn        probe(CFDictionaryRef propertyTable, io_service_t service, SInt32 * order);
    virtual IOReturn        start(CFDictionaryRef propertyTable, io_service_t service);
    virtual IOReturn        stop();
    
    virtual boolean_t       open(IOOptionBits options);
    virtual void            close(IOOptionBits options);
    virtual CFTypeRef       getProperty(CFStringRef key);
    virtual boolean_t       setProperty(CFStringRef key, CFTypeRef property);
    virtual IOHIDEventRef   copyEvent(IOHIDEventType type, IOHIDEventRef matching, IOOptionBits options);
    virtual void            setEventCallback(IOHIDServiceEventCallback callback, void * target, void * refcon);
    virtual void            scheduleWithRunLoop(CFRunLoopRef runLoop, CFStringRef runLoopMode);
    virtual void            unscheduleFromRunLoop(CFRunLoopRef runLoop, CFStringRef runLoopMode);
};

#endif /* !_IOKIT_IOHIDEVENTSERVICECLASS_H */
