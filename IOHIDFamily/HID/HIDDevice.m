/*!
 * HIDDevice.m
 * HID
 *
 * Copyright © 2022 Apple Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>
#import <IOKit/hid/IOHIDDevice.h>
#import <IOKit/hid/IOHIDLibPrivate.h>
#import <HID/HIDDevice.h>
#import <HID/HIDElement_Internal.h>
#import <HID/NSError+IOReturn.h>
#import <HID/HIDTransaction.h>
#import <os/assumes.h>
#import <stdatomic.h>

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

static void asyncReportCallback(void          * context,
                                IOReturn        status,
                                void          * sender __unused,
                                IOHIDReportType type __unused,
                                uint32_t        reportID,
                                uint8_t       * report,
                                CFIndex         reportLength)
{
    HIDDeviceReportCallback callback = (__bridge HIDDeviceReportCallback)context;

    callback(status, report, reportLength, reportID);

    Block_release(context);
}

- (BOOL)setReport:(const void *)report
     reportLength:(NSInteger)reportLength
   withIdentifier:(NSInteger)reportID
          forType:(HIDReportType)reportType
            error:(out NSError **)outError
{
    return [self setReport:report reportLength:reportLength withIdentifier:reportID forType:reportType error:outError timeout:0 callback:nil];
}

- (BOOL)setReport:(const void *)report
     reportLength:(NSInteger)reportLength
   withIdentifier:(NSInteger)reportID
          forType:(HIDReportType)reportType
            error:(out NSError **)outError
          timeout:(NSInteger)timeout
         callback:(HIDDeviceReportCallback)callback
{
    IOReturn ret;

    if (callback) {
        ret = IOHIDDeviceSetReportWithCallback((__bridge IOHIDDeviceRef)self, (IOHIDReportType)reportType, reportID, (uint8_t *)report, reportLength, timeout, asyncReportCallback, Block_copy((__bridge void *)callback));
    } else {
        ret = IOHIDDeviceSetReport((__bridge IOHIDDeviceRef)self, (IOHIDReportType)reportType, reportID, (uint8_t *)report, reportLength);
    }
    
    if (ret != kIOReturnSuccess && outError) {
        *outError = [NSError errorWithIOReturn:ret];

        if (callback) {
            Block_release((__bridge void *)callback);
        }
    }
    
    return (ret == kIOReturnSuccess);
}

- (BOOL)getReport:(void *)report
     reportLength:(NSInteger *)reportLength
   withIdentifier:(NSInteger)reportID
          forType:(HIDReportType)reportType
            error:(out NSError **)outError
{
    return [self getReport:report reportLength:reportLength withIdentifier:reportID forType:reportType error:outError timeout:0 callback:nil];
}

- (BOOL)getReport:(void *)report
     reportLength:(NSInteger *)reportLength
   withIdentifier:(NSInteger)reportID
          forType:(HIDReportType)reportType
            error:(out NSError **)outError
          timeout:(NSInteger)timeout
         callback:(HIDDeviceReportCallback)callback
{
    CFIndex length = (CFIndex)*reportLength;
    IOReturn ret;

    if (callback) {
        ret = IOHIDDeviceGetReportWithCallback((__bridge IOHIDDeviceRef)self, (IOHIDReportType)reportType, reportID, (uint8_t *)report, &length, timeout, asyncReportCallback, Block_copy((__bridge void *)callback));
    } else {
        ret = IOHIDDeviceGetReport((__bridge IOHIDDeviceRef)self, (IOHIDReportType)reportType, reportID, (uint8_t *)report, &length);
        *reportLength = (NSInteger)length;
    }

    if (ret != kIOReturnSuccess && outError) {
        *outError = [NSError errorWithIOReturn:ret];

        if (callback) {
            Block_release((__bridge void *)callback);
        }
    }

    return (ret == kIOReturnSuccess);
}

- (BOOL)commitElements:(NSArray<HIDElement *> *)elements
             direction:(HIDDeviceCommitDirection)direction
                 error:(out NSError **)outError
{
    return [self commitElements:elements direction:direction error:outError timeout:0 callback:nil];
}

- (BOOL)commitElements:(NSArray<HIDElement *> *)elements
             direction:(HIDDeviceCommitDirection)direction
                 error:(out NSError **)outError
               timeout:(NSInteger)timeout
              callback:(HIDDeviceCommitCallback _Nullable)callback
{
    bool ret;
    HIDTransaction *transaction = nil;

    os_unfair_recursive_lock_lock(&_device.deviceLock);
    if (!_device.transaction) {
        _device.transaction = (void *)CFBridgingRetain([[HIDTransaction alloc] initWithDevice:self]);
    }
    transaction = (__bridge HIDTransaction *)_device.transaction;
    
    if (direction == HIDDeviceCommitDirectionIn) {
        transaction.direction = HIDTransactionDirectionTypeInput;
    } else {
        transaction.direction = HIDTransactionDirectionTypeOutput;
    }

    if (callback) {
        ret = [transaction commitElements:elements error:outError timeout:timeout callback:callback];
    } else {
        ret = [transaction commitElements:elements error:outError];
    }
    os_unfair_recursive_lock_unlock(&_device.deviceLock);

    return ret;
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
    os_assert(atomic_exchange(&_device.elementHandler,
                              (void*)Block_copy((__bridge const void *)handler)) == NULL,
              "Input element handler already set");
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

    os_unfair_recursive_lock_lock(&me->_device.callbackLock);
    NSMutableArray *array = (__bridge NSMutableArray *)me->_device.batchElements;
    
    if (element.type == kIOHIDElementTypeInput_NULL) {
        if (me->_device.elementHandler) {
            ((__bridge HIDDeviceBatchElementHandler)me->_device.elementHandler)(array);
        }
        [array removeAllObjects];
    } else {
        [array addObject:element];
    }
    os_unfair_recursive_lock_unlock(&me->_device.callbackLock);
}

- (void)setBatchInputElementHandler:(HIDDeviceBatchElementHandler)handler
{
    os_assert(atomic_exchange(&_device.elementHandler,
                              (void*)Block_copy((__bridge const void *)handler)) == NULL,
              "Input element handler already set");
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
    os_assert(atomic_exchange(&_device.removalHandler,
                              (void*)Block_copy((__bridge const void *)handler)) == NULL,
              "Removal handler already set");
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
    os_assert(atomic_exchange(&_device.inputReportHandler,
                              (void*)Block_copy((__bridge const void *)handler)) == NULL,
              "Input report handler already set");
    
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

- (BOOL)openWithOptions:(HIDDeviceOptions)options error:(out NSError * _Nullable __autoreleasing *)outError
{
    IOReturn ret = IOHIDDeviceOpen((__bridge IOHIDDeviceRef)self, (IOOptionBits)options);

    if (ret != kIOReturnSuccess && outError) {
        *outError = [NSError errorWithIOReturn:ret];
    }

    return (ret == kIOReturnSuccess);
}

- (void)close
{
    IOHIDDeviceClose((__bridge IOHIDDeviceRef)self, 0);
}

- (void)activate
{
    os_unfair_recursive_lock_lock(&_device.callbackLock);
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

        os_unfair_recursive_lock_unlock(&_device.callbackLock);
        [self setInputElementMatching:newMatch];
    } else {
        os_unfair_recursive_lock_unlock(&_device.callbackLock);
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
