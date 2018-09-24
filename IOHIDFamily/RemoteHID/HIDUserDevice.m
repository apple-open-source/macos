//
//  HIDUserDevice.m
//  HID
//
//  Created by dekom on 10/9/17.
//

#import "HIDUserDevice.h"
#import <IOKit/hid/IOHIDUserDevice.h>

@implementation HIDUserDevice {
    IOHIDUserDeviceRef              _device;
    dispatch_queue_t                _queue;
    HIDUserDeviceGetReportHandler   _getReportHandler;
    HIDUserDeviceSetReportHandler   _setReportHandler;
    io_service_t                    _service;
}

- (nullable instancetype)initWithProperties:(nonnull NSDictionary *)properties
{
    self = [super init];
    
    if (!self) {
        return self;
    }
    
    _device = IOHIDUserDeviceCreate(kCFAllocatorDefault,
                                    (__bridge CFDictionaryRef)properties);
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

- (BOOL)isEqualToHIDUserDevice:(HIDUserDevice *)device {
    if (!device) {
        return NO;
    }
    
    if (self->_device != device->_device) {
        return NO;
    }
    
    return YES;
}

- (BOOL)isEqual:(id)object {
    if (self == object) {
        return YES;
    }
    
    if (![object isKindOfClass:[HIDUserDevice class]]) {
        return NO;
    }
    
    return [self isEqualToHIDUserDevice:(HIDUserDevice *)object];
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@", _device];
}

- (void)setCancelHandler:(nonnull dispatch_block_t __unused) handler
{
    //IOHIDUserDeviceSetCancelHandler(_device, handler);
}

- (void)setDispatchQueue:(nonnull dispatch_queue_t)queue
{
    assert(!_queue);
    _queue = queue;
}

- (void)activate
{
    assert(_queue);
    IOHIDUserDeviceScheduleWithDispatchQueue(_device, _queue);
}

- (void)cancel
{
    assert(_queue);
    IOHIDUserDeviceUnscheduleFromDispatchQueue(_device, _queue);
}

static IOReturn getReportCallback(void *refcon, IOHIDReportType type,
                                  uint32_t reportID, uint8_t *report,
                                  CFIndex *reportLength)
{
    NSUInteger length = (NSUInteger)*reportLength;
    IOReturn ret;
    HIDUserDevice *me = (__bridge HIDUserDevice *)refcon;
    
    ret = (me->_getReportHandler)(type, reportID, report, &length);
    *reportLength = (CFIndex)length;
    
    return ret;
}

- (void)setGetReportHandler:(nonnull HIDUserDeviceGetReportHandler)handler
{
    assert(!_getReportHandler);
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
    return (me->_setReportHandler)(type,
                                   reportID,
                                   report,
                                   (NSUInteger)reportLength);
}

- (void)setSetReportHandler:(nonnull HIDUserDeviceSetReportHandler)handler
{
    assert(!_setReportHandler);
    _setReportHandler = handler;
    IOHIDUserDeviceRegisterSetReportCallback(_device,
                                             setReportCallback,
                                             (__bridge void *)self);
}

- (IOReturn)handleReport:(nonnull uint8_t *)report
            reportLength:(NSInteger)reportLength
{
    return IOHIDUserDeviceHandleReport(_device, report, reportLength);
}

- (io_service_t)service
{
    return _service;
}

@end
