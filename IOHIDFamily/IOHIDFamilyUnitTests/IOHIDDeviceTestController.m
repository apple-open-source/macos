//
//  IOHIDTestController+IOHIDDeviceTestController.m
//  IOHIDFamily
//
//  Created by YG on 10/24/16.
//
//

#import "IOHIDDeviceTestController.h"
#include "IOHIDUnitTestUtility.h"

static void HIDManagerDeviceAddedCallback ( void * _Nullable context, IOReturn  result, void * _Nullable  sender, IOHIDDeviceRef device);
static void HIDManagerDeviceRemovedCallback ( void * _Nullable context, IOReturn  result, void * _Nullable  sender, IOHIDDeviceRef device);
static void HIDDeviceValueCallback (void * _Nullable  context, IOReturn result, void * _Nullable  sender, IOHIDValueRef value);
static void HIDDeviceReportCallback (void * _Nullable        context,
                                     IOReturn                result,
                                     void * _Nullable        sender,
                                     IOHIDReportType         type,
                                     uint32_t                reportID,
                                     uint8_t *               report,
                                     CFIndex                 reportLength);



@implementation IOHIDDeviceTestController {
    CFRunLoopRef            runloop;
    id                      deviceUniqueID;
    dispatch_semaphore_t    sema;
    uint8_t                 _report[0xffff];
}

-(nullable instancetype) initWithDeviceUniqueID: (nonnull id) deviceID :(nonnull CFRunLoopRef) runLoop {
  
    NSDictionary *matching = @{@kIOHIDPhysicalDeviceUniqueIDKey : deviceID};
  
    return [self initWithMatching: matching :runLoop];
}

-(nullable instancetype) initWithMatching: (nonnull NSDictionary *) matching :(nonnull CFRunLoopRef) runLoop {

    self = [super init];
    if (!self) {
        return self;
    }

    
    self->_deviceManager = IOHIDManagerCreate (kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (!self->_deviceManager) {
        return nil;
    }
  
    self->runloop = runLoop;
    
    self.values = [[NSMutableArray alloc] init];
    self.reports = [[NSMutableArray alloc] init];

    self->sema = dispatch_semaphore_create(0);

    deviceUniqueID = matching[@kIOHIDPhysicalDeviceUniqueIDKey];

    IOHIDManagerSetDeviceMatching(self.deviceManager, (CFDictionaryRef)matching);
    IOHIDManagerRegisterDeviceMatchingCallback(self.deviceManager, HIDManagerDeviceAddedCallback, (__bridge void *)(self));
    IOHIDManagerRegisterDeviceRemovalCallback(self.deviceManager, HIDManagerDeviceRemovedCallback, (__bridge void *)(self));
    if (CFRunLoopGetCurrent() == self->runloop) {
        IOHIDManagerScheduleWithRunLoop(self.deviceManager, (CFRunLoopRef)self->runloop , kCFRunLoopDefaultMode);
        IOHIDManagerOpen(self.deviceManager, kIOHIDOptionsTypeNone);
        CFRunLoopRunInMode (kCFRunLoopDefaultMode, 2.0, false);
    } else {
        CFRunLoopPerformBlock(self->runloop, kCFRunLoopDefaultMode, ^{
            IOHIDManagerScheduleWithRunLoop(self.deviceManager, (CFRunLoopRef)self->runloop , kCFRunLoopDefaultMode);
            IOHIDManagerOpen(self.deviceManager, kIOHIDOptionsTypeNone);
        });
        CFRunLoopWakeUp(self->runloop);
    }
    
    if (dispatch_semaphore_wait (self->sema, dispatch_time(DISPATCH_TIME_NOW, kDeviceMatchingTimeout * NSEC_PER_SEC))) {
        return nil;
    }
  
    return self;
}

-(void)dealloc {
    
    [self invalidate];
    
    if (self.device) {
        CFRelease (self.device);
    }
    if (self.deviceManager) {
        CFRelease (self.deviceManager);
    }
}

-(void)ManagerDeviceAdded   : (IOHIDDeviceRef) device {
    CFTypeRef value = IOHIDDeviceGetProperty (device, CFSTR (kIOHIDPhysicalDeviceUniqueIDKey));
    if (value && CFEqual(value, (__bridge CFTypeRef)(self->deviceUniqueID))) {
        self->_device = (IOHIDDeviceRef) CFRetain(device);
        TestLog("ManagerDeviceAdded: %@\n", self.device);
        IOHIDDeviceRegisterInputValueCallback (self.device, HIDDeviceValueCallback, (__bridge void * _Nullable)(self));
        IOHIDDeviceRegisterInputReportCallback(self.device, _report, sizeof(_report), HIDDeviceReportCallback, (__bridge void * _Nullable)(self));
        IOHIDDeviceOpen(self.device, 0);
        dispatch_semaphore_signal (self->sema);
    }
}

-(void)ManagerDeviceRemoved: (IOHIDDeviceRef) device {
    if (self.device  == device) {
        TestLog("ManagerDeviceRemoved: %@\n", self.device);
        self.device = nil;
    }
}

-(void)DeviceValueCallback: (IOHIDValueRef) value {
    NSMutableDictionary *valueDict = [[NSMutableDictionary alloc] initWithCapacity:2];
    valueDict[@"timestamp"] = @(IOHIDValueGetTimeStamp (value));
    CFIndex  lenght = IOHIDValueGetLength (value);
    const uint8_t  *bytes = IOHIDValueGetBytePtr (value);
    if (lenght && bytes) {
        valueDict[@"data"] = [[NSData alloc] initWithBytes:bytes length:lenght];
    }
    IOHIDElementRef element = IOHIDValueGetElement(value);
    valueDict [@"element"]    = (__bridge id _Nullable)(element);
    @synchronized (self) {
        [self.values addObject:valueDict];
    }
    TestLog("DeviceValueCallback: %@\n", valueDict);
}

-(void)DeviceReportCallback: (IOHIDReportType) type :(uint32_t)reportID :(uint8_t *)report :(CFIndex)reportLength {
    NSMutableDictionary *reportDict = [[NSMutableDictionary alloc] initWithCapacity:2];
    reportDict[@"type"] = @(type);
    reportDict[@"reportID"] = @(reportID);
    reportDict[@"data"] = [NSData dataWithBytes:report length:reportLength];
    @synchronized (self) {
        [self.reports addObject:reportDict];
    }
    TestLog("DeviceReportCallback: %@\n", reportDict);
}



-(void)invalidate {
    if (self->runloop) {

        if (CFRunLoopGetCurrent() == runloop) {
            IOHIDManagerUnscheduleFromRunLoop (self.deviceManager, self->runloop, kCFRunLoopDefaultMode);
            IOHIDManagerClose(self.deviceManager, 0);
        } else {
            dispatch_semaphore_t completionSema = dispatch_semaphore_create(0);
            CFRunLoopPerformBlock(self->runloop, kCFRunLoopDefaultMode, ^{
                IOHIDManagerUnscheduleFromRunLoop(self.deviceManager, (CFRunLoopRef)self->runloop , kCFRunLoopDefaultMode);
                IOHIDManagerClose(self.deviceManager, kIOHIDOptionsTypeNone);
                dispatch_semaphore_signal(completionSema);
            });
            CFRunLoopWakeUp(self->runloop);
            dispatch_semaphore_wait (completionSema, DISPATCH_TIME_FOREVER);
        }
        self->runloop = NULL;
    }
}

@end

void HIDManagerDeviceAddedCallback ( void * _Nullable context, IOReturn  result, void * _Nullable  sender __unused, IOHIDDeviceRef device) {
    IOHIDDeviceTestController *self = (__bridge IOHIDDeviceTestController *)context;
    if (result) {
        TestLog("HIDManagerDeviceAddedCallback: result:0x%x\n", result);
    }
    [self ManagerDeviceAdded : device];
}

void HIDManagerDeviceRemovedCallback ( void * _Nullable context, IOReturn  result, void * _Nullable  sender __unused, IOHIDDeviceRef device) {
    IOHIDDeviceTestController *self = (__bridge IOHIDDeviceTestController *)context;
    if (result) {
        TestLog("HIDManagerDeviceRemovedCallback: result:0x%x\n", result);
    }
    [self ManagerDeviceRemoved : device];
}

void HIDDeviceValueCallback (void * _Nullable  context, IOReturn result, void * _Nullable  sender __unused, IOHIDValueRef value) {
    IOHIDDeviceTestController *self = (__bridge IOHIDDeviceTestController *)context;
    if (result) {
        TestLog("HIDDeviceValueCallback: result:0x%x\n", result);
    }
    [self DeviceValueCallback : value];
}

void HIDDeviceReportCallback (void * _Nullable        context,
                              IOReturn                result,
                              void * _Nullable        sender __unused,
                              IOHIDReportType         type,
                              uint32_t                reportID,
                              uint8_t *               report,
                              CFIndex                 reportLength) {

    IOHIDDeviceTestController *self = (__bridge IOHIDDeviceTestController *)context;
    if (result) {
        TestLog("HIDDeviceReportCallback: result:0x%x\n", result);
        return;
    }
    [self DeviceReportCallback :type :reportID :report :reportLength];
}
