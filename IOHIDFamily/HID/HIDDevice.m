//
//  HIDDevice.m
//  HID
//
//  Created by dekom on 10/9/17.
//

#import <Foundation/Foundation.h>
#import <IOKit/hid/IOHIDDevice.h>
#import "HIDDevicePrivate.h"
#import "HIDElementPrivate.h"
#import "NSError+IOReturn.h"
#import "HIDTransaction.h"
#import <os/assumes.h>

@implementation HIDDevice (HIDFramework)

- (instancetype)initWithService:(io_service_t)service
{
    return (__bridge_transfer HIDDevice *)IOHIDDeviceCreate(kCFAllocatorDefault,
                                                            service);
}

- (id)propertyForKey:(NSString *)key
{
    return (__bridge id)IOHIDDeviceGetProperty((__bridge IOHIDDeviceRef)self,
                                               (__bridge CFStringRef)key);
}

- (BOOL)setProperty:(id)value forKey:(NSString *)key
{
    return IOHIDDeviceSetProperty((__bridge IOHIDDeviceRef)self,
                                  (__bridge CFStringRef)key,
                                  (__bridge CFTypeRef)value);
}

- (BOOL)conformsToUsagePage:(NSInteger)usagePage usage:(NSInteger)usage
{
    return IOHIDDeviceConformsTo((__bridge IOHIDDeviceRef)self,
                                 (uint32_t)usagePage,
                                 (uint32_t)usage);
}

- (NSArray *)elementsMatching:(NSDictionary *)matching
{
    return (NSArray *)CFBridgingRelease(IOHIDDeviceCopyMatchingElements(
                                            (__bridge IOHIDDeviceRef)self,
                                            (__bridge CFDictionaryRef)matching,
                                            0));
}

- (BOOL)setReport:(const void *)report
     reportLength:(NSInteger)reportLength
   withIdentifier:(NSInteger)reportID
          forType:(HIDReportType)reportType
            error:(out NSError **)outError
{
    IOReturn ret = IOHIDDeviceSetReport((__bridge IOHIDDeviceRef)self,
                                        (IOHIDReportType)reportType,
                                        reportID,
                                        (uint8_t *)report,
                                        reportLength);
    
    if (ret != kIOReturnSuccess && outError) {
        *outError = [NSError errorWithIOReturn:ret];
    }
    
    return (ret == kIOReturnSuccess);
}

- (BOOL)getReport:(void *)report
     reportLength:(NSInteger *)reportLength
   withIdentifier:(NSInteger)reportID
          forType:(HIDReportType)reportType
            error:(out NSError **)outError
{
    CFIndex length = (CFIndex)*reportLength;
    
    IOReturn ret = IOHIDDeviceGetReport((__bridge IOHIDDeviceRef)self,
                                        (IOHIDReportType)reportType,
                                        reportID,
                                        (uint8_t *)report,
                                        &length);
    
    if (ret != kIOReturnSuccess && outError) {
        *outError = [NSError errorWithIOReturn:ret];
    }
    
    *reportLength = (NSInteger)length;
    
    return (ret == kIOReturnSuccess);
}

- (BOOL)commitElements:(NSArray<HIDElement *> *)elements
             direction:(HIDDeviceCommitDirection)direction
                 error:(out NSError **)outError
{
    HIDTransaction *transaction = nil;
    
    if (!_device.transaction) {
        _device.transaction = (void *)CFBridgingRetain([[HIDTransaction alloc]
                                                        initWithDevice:self]);
    }
    
    transaction = (__bridge HIDTransaction *)_device.transaction;
    
    if (direction == HIDDeviceCommitDirectionIn) {
        transaction.direction = HIDTransactionDirectionTypeInput;
    } else {
        transaction.direction = HIDTransactionDirectionTypeOutput;
    }
    
    return [transaction commitElements:elements error:outError];
}

- (void)setInputElementMatching:(id)matching
{
    os_assert([matching isKindOfClass:[NSDictionary class]] ||
              [matching isKindOfClass:[NSArray class]],
              "Unknown matching criteria: %@", matching);
    
    if ([matching isKindOfClass:[NSDictionary class]]) {
        CFDictionaryRef matchDict = (__bridge CFDictionaryRef)matching;
        
        if (((NSDictionary *)matching).count) {
            IOHIDDeviceSetInputValueMatching((__bridge IOHIDDeviceRef)self,
                                             matchDict);
        } else {
            IOHIDDeviceSetInputValueMatching((__bridge IOHIDDeviceRef)self,
                                             nil);
        }
    } else if ([matching isKindOfClass:[NSArray class]]) {
        CFArrayRef matchArray = (__bridge CFArrayRef)matching;
        
        if (((NSArray *)matching).count) {
            IOHIDDeviceSetInputValueMatchingMultiple(
                                                (__bridge IOHIDDeviceRef)self,
                                                matchArray);
        } else {
            IOHIDDeviceSetInputValueMatchingMultiple(
                                                (__bridge IOHIDDeviceRef)self,
                                                nil);
        }
    }
}

static void inputValueCallback(void *context, IOReturn result __unused,
                               void *sender __unused, IOHIDValueRef value)
{
    HIDDevice *me = (__bridge HIDDevice *)context;
    HIDElement *element = (__bridge HIDElement *)IOHIDValueGetElement(value);
    element.valueRef = value;
    
    if (me->_device.elementHandler) {
        ((__bridge HIDDeviceElementHandler)me->_device.elementHandler)(element);
    }
}

- (void)setInputElementHandler:(HIDDeviceElementHandler)handler
{
    os_assert(!_device.elementHandler, "Input element handler already set");
    _device.elementHandler = (void *)Block_copy((__bridge const void *)handler);
    IOHIDDeviceRegisterInputValueCallback((__bridge IOHIDDeviceRef)self,
                                          inputValueCallback,
                                          (__bridge void *)self);
}

static void batchInputValueCallback(void *context, IOReturn result __unused,
                               void *sender __unused, IOHIDValueRef value)
{
    HIDDevice *me = (__bridge HIDDevice *)context;
    HIDElement *element = (__bridge HIDElement *)IOHIDValueGetElement(value);
    element.valueRef = value;
    NSMutableArray *array = (__bridge NSMutableArray *)me->_device.batchElements;
    
    if (element.type == kIOHIDElementTypeInput_NULL) {
        if (me->_device.elementHandler) {
            ((__bridge HIDDeviceBatchElementHandler)me->_device.elementHandler)(array);
        }
        [array removeAllObjects];
    } else {
        [array addObject:element];
    }
}

- (void)setBatchInputElementHandler:(HIDDeviceBatchElementHandler)handler
{
    os_assert(!_device.elementHandler, "Input element handler already set");
    _device.elementHandler = (void *)Block_copy((__bridge const void *)handler);
    _device.batchElements = CFArrayCreateMutable(kCFAllocatorDefault,
                                                 0,
                                                 &kCFTypeArrayCallBacks);
    
    IOHIDDeviceRegisterInputValueCallback((__bridge IOHIDDeviceRef)self,
                                          batchInputValueCallback,
                                          (__bridge void *)self);
}

static void removalCallback(void *context, IOReturn result __unused,
                            void *sender __unused)
{
    HIDDevice *me = (__bridge HIDDevice *)context;
    
    if (me->_device.removalHandler) {
        ((__bridge HIDBlock)me->_device.removalHandler)();
        Block_release(me->_device.removalHandler);
        me->_device.removalHandler = nil;
    }
}

- (void)setRemovalHandler:(HIDBlock)handler
{
    os_assert(!_device.removalHandler, "Removal handler already set");
    _device.removalHandler = (void *)Block_copy((__bridge const void *)handler);
    IOHIDDeviceRegisterRemovalCallback((__bridge IOHIDDeviceRef)self,
                                       removalCallback,
                                       (__bridge void *)self);
}

static void inputReportCallback(void *context,
                                IOReturn result __unused,
                                void *sender,
                                IOHIDReportType type,
                                uint32_t reportID,
                                uint8_t *report,
                                CFIndex reportLength,
                                uint64_t timeStamp)
{
    HIDDevice *me = (__bridge HIDDevice *)context;
    NSData *data = [[NSData alloc] initWithBytesNoCopy:report
                                                length:reportLength
                                          freeWhenDone:NO];
    
    if (me->_device.inputReportHandler) {
        ((__bridge HIDReportHandler)me->_device.inputReportHandler)(
                                            (__bridge HIDDevice *)sender,
                                            timeStamp,
                                            (HIDReportType)type,
                                            reportID,
                                            data);
    }
}

- (void)setInputReportHandler:(HIDReportHandler)handler
{
    NSUInteger bufferSize = 1;
    
    os_assert(!_device.inputReportHandler, "Input report handler already set");
    _device.inputReportHandler = (void *)Block_copy(
                                                (__bridge const void *)handler);
    
    NSNumber *reportSize = [self propertyForKey:@kIOHIDMaxInputReportSizeKey];
    if (reportSize != nil) {
        bufferSize = reportSize.unsignedIntegerValue;
    }
    
    if (!_device.reportBuffer) {
        _device.reportBuffer = CFDataCreateMutable(kCFAllocatorDefault,
                                                   bufferSize);
        CFDataSetLength(_device.reportBuffer, bufferSize);
    }
    
    IOHIDDeviceRegisterInputReportWithTimeStampCallback(
                    (__bridge IOHIDDeviceRef)self,
                    (uint8_t *)CFDataGetMutableBytePtr(_device.reportBuffer),
                    CFDataGetLength(_device.reportBuffer),
                    inputReportCallback,
                    (__bridge void *)self);
}

- (void)setCancelHandler:(HIDBlock)handler
{
    IOHIDDeviceSetCancelHandler((__bridge IOHIDDeviceRef)self, handler);
}

- (void)setDispatchQueue:(dispatch_queue_t)queue
{
    IOHIDDeviceSetDispatchQueue((__bridge IOHIDDeviceRef)self, queue);
}

- (void)open
{
    IOHIDDeviceOpen((__bridge IOHIDDeviceRef)self, 0);
}

- (BOOL)openSeize: (out NSError **)outError
{
    IOReturn status = IOHIDDeviceOpen((__bridge IOHIDDeviceRef)self,
                                      kIOHIDOptionsTypeSeizeDevice);
    if (status && outError) {
        *outError = [NSError errorWithIOReturn:status];
    }
    return (status == kIOReturnSuccess);
}

- (void)close
{
    IOHIDDeviceClose((__bridge IOHIDDeviceRef)self, 0);
}

- (void)activate
{
    if (_device.batchElements) {
        NSArray *oldMatch = (__bridge NSArray *)_device.inputMatchingMultiple;
        NSMutableArray *newMatch = nil;
        
        if (oldMatch) {
            newMatch = [NSMutableArray arrayWithArray:oldMatch];
        } else {
            newMatch = [NSMutableArray arrayWithObjects:
                        @{@kIOHIDElementTypeKey: @(kIOHIDElementTypeInput_Misc)},
                        @{@kIOHIDElementTypeKey: @(kIOHIDElementTypeInput_Button)},
                        @{@kIOHIDElementTypeKey: @(kIOHIDElementTypeInput_Axis)},
                        @{@kIOHIDElementTypeKey: @(kIOHIDElementTypeInput_ScanCodes)},
                        nil];
        }
        
        [newMatch addObject:@{@kIOHIDElementTypeKey:
                                  @(kIOHIDElementTypeInput_NULL)}];
        
        [self setInputElementMatching:newMatch];
    }
    
    IOHIDDeviceActivate((__bridge IOHIDDeviceRef)self);
}

- (void)cancel
{
    IOHIDDeviceCancel((__bridge IOHIDDeviceRef)self);
}

- (io_service_t)service
{
    return IOHIDDeviceGetService((__bridge IOHIDDeviceRef)self);
}

@end
