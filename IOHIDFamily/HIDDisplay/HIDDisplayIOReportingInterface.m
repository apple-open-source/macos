//
//  HIDDisplayIOReportingInterface.m
//  HIDDisplay
//
//  Created by AB on 3/27/19.
//

#import "HIDDisplayIOReportingInterface.h"
#import "HIDDisplayPrivate.h"
#import "HIDDisplayInterfacePrivate.h"
#import "HIDDisplayCAPI.h"
#import "HIDDisplayIOReportingInterfacePrivate.h"

#import <AssertMacros.h>
#import <IOKit/hid/AppleHIDUsageTables.h>
#import <IOKit/hid/IOHIDKeys.h>
#import <IOKit/hid/IOHIDElement.h>
#import <IOKit/hid/IOHIDLib.h>
#import <IOKit/hid/IOHIDValue.h>

#define kDeviceCancelTimeout 5
#define kDeviceActiveTimeout 5

static void _inputValueCallback(
                           void * _Nullable        context,
                           IOReturn                result,
                           void * _Nullable  __unused      sender,
                           IOHIDValueRef           value)
{
    
    //redundant check since we are calling
    if (!context || result != kIOReturnSuccess) {
        return;
    }
    
    HIDDisplayIOReportingInterface *_hidInterface = (__bridge HIDDisplayIOReportingInterface*)context;
    
    [_hidInterface handleInputData:value];
    
}

@implementation HIDDisplayIOReportingInterface
{
    NSDictionary<NSNumber*,HIDElement*>* _usageElementMap;
    CFRunLoopRef _runLoop;
    IOReportingInputDataHandler _dataHandler;
    dispatch_block_t _cancelHandler;
    dispatch_queue_t _queue;
}

-(nullable instancetype) initWithContainerID:(NSString*) containerID
{
    self = [super initWithContainerID:containerID];
    
    if (!self) {
        return nil;
    }
    
    if ([self setupIOReporting] == NO) {
        return nil;
    }
    
    return self;
}

-(void) handleInputData:(IOHIDValueRef) value
{
    
    if (!value) {
        return;
    }
    
    IOHIDElementRef element  =  IOHIDValueGetElement(value);
    
    if (!element) {
        return;
    }
    
    uint32_t usage  = IOHIDElementGetUsage(element);
    uint32_t usagePage = IOHIDElementGetUsagePage(element);
    
    
    if (usagePage != kHIDPage_AppleVendorDisplayIOReporting && usage != kHIDUsage_AppleVendorDisplayIOReporting_Input) {
        return;
    }
    
    //expect this to be released under the hood, so not setting freeWhenDone
    NSData *data = [NSData dataWithBytesNoCopy:(void *)IOHIDValueGetBytePtr(value) length:IOHIDValueGetLength(value) freeWhenDone:NO];
    
   
    if (_queue && _dataHandler) {
        dispatch_sync(_queue, ^{
            
            self->_dataHandler((__bridge
                                CFDataRef)data);
        });
    }
    
}

-(BOOL) setupIOReporting
{
    NSDictionary *matching = @{@kIOHIDElementUsagePageKey: @(kHIDPage_AppleVendorDisplayIOReporting)};
    
    NSDictionary<NSNumber*,HIDElement*>* usageElementMap = [self getDeviceElements:matching];
    
    if (!usageElementMap || usageElementMap.count == 0) {
        return NO;
    }
    
    _usageElementMap = usageElementMap;
    
    return YES;
}

-(NSArray*) getHIDDevices
{
    NSDictionary *matching = @{@kIOHIDDeviceUsagePairsKey : @[@{
                                                                  @kIOHIDDeviceUsagePageKey : @(kHIDPage_AppleVendor),
                                                                  @kIOHIDDeviceUsageKey: @(kHIDUsage_AppleVendor_IOReporting)
                                                                  }]};
    return [self getHIDDevicesForMatching:matching];
    
}

-(void) setInputDataHandler:(IOReportingInputDataHandler) handler
{
    _dataHandler = handler;
}

-(void) setDispatchQueue:(dispatch_queue_t) queue
{
    _queue = queue;
}

-(void) setCancelHandler:(dispatch_block_t) handler
{
    _cancelHandler = handler;
}

-(bool) setOutputData:(NSData*) data error:(NSError**) err
{
    
    HIDElement *ioReportingOutputElement = [_usageElementMap objectForKey:@(kHIDUsage_AppleVendorDisplayIOReporting_Output)];
    
    if (!ioReportingOutputElement) {
        
        if (err) {
            *err = [NSError errorWithDomain:NSOSStatusErrorDomain code:kIOReturnUnsupported userInfo:nil];
        }
        return false;
    }
    
    ioReportingOutputElement.dataValue = data;
    
    return [self commit:@[ioReportingOutputElement] error:err];
}

-(void) activate
{
    
    __weak HIDDisplayIOReportingInterface *weakSelf = self;
    
    __block dispatch_semaphore_t activateSemaphore = dispatch_semaphore_create(0);
    
    [NSThread detachNewThreadWithBlock:^{
        
        __strong HIDDisplayIOReportingInterface *strongSelf = weakSelf;
        
        if (!strongSelf) {
            return;
        }

        
        if (strongSelf->_runLoop) {
            return;
        }
        
        strongSelf->_runLoop = CFRunLoopGetCurrent();
        
        NSDictionary *matching = @{@kIOHIDElementUsagePageKey: @(kHIDPage_AppleVendorDisplayIOReporting)};
        
        IOHIDDeviceSetInputValueMatching(strongSelf.device, (__bridge CFDictionaryRef)matching);
        
        IOHIDDeviceRegisterInputValueCallback(strongSelf.device, _inputValueCallback, (__bridge void*)strongSelf);
        
        IOHIDDeviceScheduleWithRunLoop(strongSelf.device, strongSelf->_runLoop, kCFRunLoopDefaultMode);
        
        dispatch_semaphore_signal(activateSemaphore);
        
        CFRunLoopRun();
        
    }];
    
    // reason to use seamphore is what if NSThread is not created, user call cancel, and thread is dispatched after that
    // we may end up in having stale objects
    
    dispatch_semaphore_wait(activateSemaphore, dispatch_time(DISPATCH_TIME_NOW, kDeviceActiveTimeout * NSEC_PER_SEC));
    
}

-(void) cancel
{
    
    // Thread may not be called yet so , we should take care of teardown in dealloc
    // Case thread not scheduled , runloop not assigned,
    // call cancel, after which runloop execute , we may end up not cleaning objects (FIXME)
    if (!_runLoop) {
        return;
    }
    
    __weak HIDDisplayIOReportingInterface *weakSelf = self;
    
    CFRunLoopPerformBlock(_runLoop, kCFRunLoopDefaultMode, ^{
        
        __strong HIDDisplayIOReportingInterface *strongSelf = weakSelf;
        
        if (!strongSelf) {
            return;
        }
        
        IOHIDDeviceUnscheduleFromRunLoop(strongSelf.device, strongSelf->_runLoop, kCFRunLoopDefaultMode);
        CFRunLoopStop(strongSelf->_runLoop);
        
        dispatch_async(strongSelf->_queue, ^{
            
            if (strongSelf->_cancelHandler) {
                strongSelf->_cancelHandler();
            }
            
        });
        
    });
    
    CFRunLoopWakeUp(_runLoop);
    
}

@end
