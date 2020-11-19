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

#ifndef IOHIDDeviceClass_h
#define IOHIDDeviceClass_h

#import "IOHIDIUnknown2.h"
#import <IOKit/hid/IOHIDElement.h>
#import <IOKit/hid/IOHIDValue.h>
#import <IOKit/hid/IOHIDDevicePlugIn.h>
#import <IOKit/hid/IOHIDLibUserClient.h>

@class IOHIDQueueClass;

enum {
    kHIDSetElementValuePendEvent    = 0x00010000,
    kHIDGetElementValuePendEvent    = kHIDSetElementValuePendEvent,
    kHIDGetElementValueForcePoll    = 0x00020000,
    kHIDGetElementValuePreventPoll  = 0x00040000,
};

enum {
    kHIDCopyMatchingElementsDictionary = 0x1
};

@interface IOHIDDeviceClass : IOHIDPlugin {
    IOHIDDeviceTimeStampedDeviceInterface   *_device;
    io_service_t                            _service;
    io_connect_t                            _connect;
    
    mach_port_t                             _port;
    CFMachPortRef                           _machPort;
    CFRunLoopSourceRef                      _runLoopSource;
    
    BOOL                                    _opened;
    BOOL                                    _tccRequested;
    BOOL                                    _tccGranted;
    
    IOHIDQueueClass                         *_queue;
    NSMutableArray                          *_elements;
    NSMutableArray                          *_sortedElements;
    NSMutableArray                          *_reportElements;
    NSMutableDictionary                     *_properties;
    
    IOHIDReportCallback                     _inputReportCallback;
    IOHIDReportWithTimeStampCallback        _inputReportTimestampCallback;
    void                                    *_inputReportContext;
    uint8_t                                 *_inputReportBuffer;
    CFIndex                                 _inputReportBufferLength;
}

- (void)initQueue;

- (IOReturn)open:(IOOptionBits)options;
- (IOReturn)close:(IOOptionBits)options;

- (IOReturn)copyMatchingElements:(NSDictionary * _Nullable)matching
                        elements:(CFArrayRef _Nonnull * _Nonnull)pElements
                         options:(IOOptionBits)options;

- (IOReturn)setInputReportCallback:(uint8_t * _Nonnull)report
                      reportLength:(CFIndex)reportLength
                          callback:(IOHIDReportCallback _Nonnull)callback
                           context:(void * _Nullable)context
                           options:(IOOptionBits)options;

- (IOReturn)setReport:(IOHIDReportType)reportType
             reportID:(uint32_t)reportID
               report:(const uint8_t * _Nonnull)report
         reportLength:(CFIndex)reportLength
              timeout:(uint32_t)timeout
             callback:(IOHIDReportCallback _Nullable)callback
              context:(void * _Nullable)context
              options:(IOOptionBits)options;

- (IOReturn)getReport:(IOHIDReportType)reportType
             reportID:(uint32_t)reportID
               report:(uint8_t * _Nonnull)report
         reportLength:(CFIndex * _Nonnull)pReportLength
              timeout:(uint32_t)timeout
             callback:(IOHIDReportCallback _Nullable)callback
              context:(void * _Nullable)context
              options:(IOOptionBits)options;

- (IOReturn)getValue:(IOHIDElementRef _Nonnull)element
               value:(IOHIDValueRef _Nonnull * _Nonnull)pValue
             timeout:(uint32_t)timeout
            callback:(IOHIDValueCallback _Nullable)callback
             context:(void * _Nullable)context
             options:(IOOptionBits)options;

- (IOReturn)setValue:(IOHIDElementRef _Nonnull)element
               value:(IOHIDValueRef _Nonnull)value
             timeout:(uint32_t)timeout
            callback:(IOHIDValueCallback _Nullable)callback
             context:(void * _Nullable)context
             options:(IOOptionBits)options;

- (IOHIDElementRef _Nullable)getElement:(uint32_t)cookie;

- (void)releaseOOBReport:(uint64_t)reportAddress;

@property (readonly)            mach_port_t         port;
@property (readonly, nullable)  CFRunLoopSourceRef  runLoopSource;
@property (readonly)            io_connect_t        connect;
@property (readonly)            io_service_t        service;

@end

#endif /* IOHIDDeviceClass_h */
