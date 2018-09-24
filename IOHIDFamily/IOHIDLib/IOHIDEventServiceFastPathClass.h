/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 2017 Apple Computer, Inc.  All Rights Reserved.
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


#ifndef IOHIDEventServiceFastPathClass_h
#define IOHIDEventServiceFastPathClass_h

#include <IOKit/hid/IOHIDServicePlugIn.h>
#include <IOKit/IODataQueueClient.h>
#include "IOHIDIUnknown.h"

class IOHIDEventServiceFastPathClass : public IOHIDIUnknown
{
private:
    // Disable copy constructors
    IOHIDEventServiceFastPathClass(IOHIDEventServiceFastPathClass &src);
    void operator =(IOHIDEventServiceFastPathClass &src);
    
protected:
    IOHIDEventServiceFastPathClass();
    virtual ~IOHIDEventServiceFastPathClass();
    
    static IOCFPlugInInterface           sIOCFPlugInInterfaceV1;
    static IOHIDServiceFastPathInterface sIOHIDServiceFastPathInterface;
    
    struct InterfaceMap                 _hidService;
    io_service_t                        _service;
    io_connect_t                        _connect;
    io_service_t                        _client;
    void *                              _sharedMemory;
    vm_size_t                           _sharedMemorySize;

    static inline IOHIDEventServiceFastPathClass *getThis(void *self) { return (IOHIDEventServiceFastPathClass *)((InterfaceMap *) self)->obj; };
    
    // IOCFPlugInInterface methods
    static IOReturn _probe(void *self, CFDictionaryRef propertyTable, io_service_t service, SInt32 *order);
    static IOReturn _start(void *self, CFDictionaryRef propertyTable, io_service_t service);
    static IOReturn _stop(void *self);
    
    // IOHIDServiceInterfaceFastPath methods
    static boolean_t        _open(void *self, IOOptionBits options, CFDictionaryRef property);
    static void             _close(void *self, IOOptionBits options);
    static CFTypeRef        _copyProperty(void *self, CFStringRef key);
    static boolean_t        _setProperty(void *self, CFStringRef key, CFTypeRef property);
    static IOHIDEventRef    _copyEvent(void *self, CFTypeRef copySpec, IOOptionBits options);

public:
    // IOCFPlugin stuff
    static IOCFPlugInInterface **alloc();
    
    HRESULT         queryInterface(REFIID iid, void **ppv);
    IOReturn        probe(CFDictionaryRef propertyTable, io_service_t service, SInt32 * order);
    IOReturn        start(CFDictionaryRef propertyTable, io_service_t service);
    IOReturn        stop();
    
    boolean_t       open(IOOptionBits options, CFDictionaryRef property);
    void            close(IOOptionBits options);
    CFTypeRef       copyProperty(CFStringRef key);
    boolean_t       setProperty(CFStringRef key, CFTypeRef property);
    IOHIDEventRef   copyEvent(CFTypeRef copySpec, IOOptionBits options);
};


#endif /* IOHIDEventServiceFastPathClass_h */
