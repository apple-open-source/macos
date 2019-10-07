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

#ifndef IOHIDQueueClass_h
#define IOHIDQueueClass_h

#import "IOHIDIUnknown2.h"
#import <IOKit/hid/IOHIDDevicePlugIn.h>
#import <IOKit/IODataQueueShared.h>
#import "IOHIDDeviceClass.h"

@interface IOHIDQueueClass : IOHIDIUnknown2 {
    IOHIDDeviceQueueInterface   *_queue;
    __weak IOHIDDeviceClass     *_device;
    
    mach_port_t                 _port;
    CFMachPortRef               _machPort;
    CFRunLoopSourceRef          _runLoopSource;
    
    IODataQueueMemory           *_queueMemory;
    vm_size_t                   _queueMemorySize;
    bool                        _queueSizeChanged;
    uint32_t                    _lastTail;
    
    uint32_t                    _depth;
    uint64_t                    _queueToken;
    
    IOHIDCallback               _valueAvailableCallback;
    void                        *_valueAvailableContext;

    CFTypeRef                   _usageAnalytics;
}

- (nullable instancetype)initWithDevice:(IOHIDDeviceClass * _Nonnull)device;
- (nullable instancetype)initWithDevice:(IOHIDDeviceClass * _Nonnull)device
                                   port:(mach_port_t)port
                                 source:(CFRunLoopSourceRef _Nullable)source;

- (IOReturn)addElement:(IOHIDElementRef _Nonnull)element;
- (IOReturn)setValueAvailableCallback:(IOHIDCallback _Nonnull)callback
                              context:(void * _Nullable)context;
- (IOReturn)start;
- (IOReturn)stop;
- (IOReturn)copyNextValue:(IOHIDValueRef _Nullable * _Nullable)pValue;

- (void)queueCallback:(CFMachPortRef _Nonnull)port
                  msg:(mach_msg_header_t * _Nonnull)msg
                 size:(CFIndex)size
                 info:(void * _Nullable)info;

@end;

// We will have to support this until kIOHIDDeviceInterfaceID is deprecated
// (see 35698866)
@interface IOHIDObsoleteQueueClass : IOHIDQueueClass {
    IOHIDQueueInterface     *_interface;
    
    IOHIDCallbackFunction   _eventCallback;
    void                    *_eventCallbackTarget;
    void                    *_eventCallbackRefcon;
}

@end;

#endif /* IOHIDQueueClass_h */
