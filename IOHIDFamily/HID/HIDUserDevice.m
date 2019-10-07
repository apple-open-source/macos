/*
*
* @APPLE_LICENSE_HEADER_START@
*
* Copyright (c) 2019 Apple Computer, Inc.  All Rights Reserved.
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

#import "HIDUserDevice.h"
#import <IOKit/hid/IOHIDUserDevice.h>
#import "NSError+IOReturn.h"
#import <os/assumes.h>

NSString * const kHIDUserDevicePropertyCreateInactiveKey = @"HIDUserDeviceCreateInactive";

@implementation HIDUserDevice {
    IOHIDUserDeviceRef              _device;
    HIDUserDeviceGetReportHandler   _getReportHandler;
    HIDUserDeviceSetReportHandler   _setReportHandler;
    io_service_t                    _service;
}

- (instancetype)initWithProperties:(NSDictionary *)properties
{
    IOOptionBits options = 0;
    
    self = [super init];
    
    if (!self) {
        return self;
    }
    
    if (properties[kHIDUserDevicePropertyCreateInactiveKey] && [properties[kHIDUserDevicePropertyCreateInactiveKey] isEqual:@YES]) {
        options |= kIOHIDUserDeviceCreateOptionStartWhenScheduled;
    }
    
    _device = IOHIDUserDeviceCreateWithOptions(kCFAllocatorDefault,
                                               (__bridge CFDictionaryRef)properties,
                                               options
                                               );
    if (!_device) {
        return nil;
    }
    
    _service = IOHIDUserDeviceCopyService(_device);
    
    return self;
}

- (void)dealloc
{
    if (_service) {
        IOObjectRelease(_service);
    }
    if (_device) {
        CFRelease(_device);
    }
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@", _device];
}

- (id)propertyForKey:(NSString *)key
{
    return (id)CFBridgingRelease(IOHIDUserDeviceCopyProperty(
                                                    _device,
                                                    (__bridge CFStringRef)key));
}

- (BOOL)setProperty:(id)value forKey:(NSString *)key
{
    return IOHIDUserDeviceSetProperty(_device,
                                      (__bridge CFStringRef)key,
                                      (__bridge CFTypeRef)value);
}

- (void)setCancelHandler:(HIDBlock)handler
{
    IOHIDUserDeviceSetCancelHandler(_device, handler);
}

- (void)setDispatchQueue:(dispatch_queue_t)queue
{
    IOHIDUserDeviceSetDispatchQueue(_device, queue);
}

- (void)activate
{
    IOHIDUserDeviceActivate(_device);
}

- (void)cancel
{
    IOHIDUserDeviceCancel(_device);
}

static IOReturn getReportCallback(void *refcon, IOHIDReportType type,
                                  uint32_t reportID, uint8_t *report,
                                  CFIndex *reportLength)
{
    NSInteger length = (NSInteger)*reportLength;
    IOReturn ret;
    HIDUserDevice *me = (__bridge HIDUserDevice *)refcon;
    
    ret = (me->_getReportHandler)((HIDReportType)type,
                                  reportID,
                                  (void *)report,
                                  &length);
    
    *reportLength = (CFIndex)MIN(*reportLength, length);
    
    return ret;
}

- (void)setGetReportHandler:(HIDUserDeviceGetReportHandler)handler
{
    os_assert(!_getReportHandler, "Get report handler already set");
    _getReportHandler = handler;
    
    IOHIDUserDeviceRegisterGetReportWithReturnLengthCallback(
                                                        _device,
                                                        getReportCallback,
                                                        (__bridge void *)self);
}

static IOReturn setReportCallback(void *refcon, IOHIDReportType type,
                                  uint32_t reportID, uint8_t *report,
                                  CFIndex reportLength)
{
    HIDUserDevice *me = (__bridge HIDUserDevice *)refcon;
    
    return (me->_setReportHandler)((HIDReportType)type,
                                   reportID,
                                   (const void *)report,
                                   (NSInteger)reportLength);
}

- (void)setSetReportHandler:(HIDUserDeviceSetReportHandler)handler
{
    os_assert(!_setReportHandler, "Set report handler already set");
    _setReportHandler = handler;
    IOHIDUserDeviceRegisterSetReportCallback(_device,
                                             setReportCallback,
                                             (__bridge void *)self);
}

- (BOOL)handleReport:(NSData *)report
               error:(out NSError **)outError
{
    IOReturn ret = IOHIDUserDeviceHandleReport(_device,
                                               (uint8_t *)[report bytes],
                                               [report length]);
    
    if (ret != kIOReturnSuccess && outError) {
        *outError = [NSError errorWithIOReturn:ret];
    }
    
    return (ret == kIOReturnSuccess);
}

- (BOOL)handleReport:(NSData *)report
       withTimestamp:(uint64_t)timestamp
               error:(out NSError **)outError
{
    IOReturn ret = IOHIDUserDeviceHandleReportWithTimeStamp(_device,
                                                            timestamp,
                                                            (uint8_t *)[report bytes],
                                                            [report length]);

    if (ret != kIOReturnSuccess && outError) {
        *outError = [NSError errorWithIOReturn:ret];
    }

    return (ret == kIOReturnSuccess);
}

- (io_service_t)service
{
    return _service;
}

@end
