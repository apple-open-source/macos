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

#ifndef IOHIDUPSClass_h
#define IOHIDUPSClass_h

#import "IOHIDIUnknown2.h"
#import <IOKit/ps/IOUPSPlugIn.h>
#import <IOKit/hid/IOHIDDevicePlugIn.h>

@interface IOHIDUPSClass : IOHIDPlugin {
    IOUPSPlugInInterface_v140               *_ups;
    IOHIDDeviceTimeStampedDeviceInterface   **_device;
    IOHIDDeviceQueueInterface               **_queue;
    IOHIDDeviceTransactionInterface         **_transaction;
    
    NSMutableDictionary                     *_properties;
    NSMutableSet                            *_capabilities;
    NSMutableDictionary                     *_upsEvent;
    NSMutableDictionary                     *_upsUpdatedEvent;
    NSMutableDictionary                     *_debugInformation;
    
    struct {
        NSMutableArray                      *input;
        NSMutableArray                      *output;
        NSMutableArray                      *feature;
    } _elements;
    
    NSMutableArray                          *_commandElements;
    NSMutableArray                          *_eventElements;
    
    IOUPSEventCallbackFunction              _eventCallback;
    void *                                  _eventTarget;
    void *                                  _eventRefcon;
    
    NSTimer                                 *_timer;
    CFRunLoopSourceRef                      _runLoopSource;
}

@end

#endif /* IOHIDUPSClass_h */
