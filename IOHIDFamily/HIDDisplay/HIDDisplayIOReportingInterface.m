//
//  HIDDisplayIOReportingInterface.m
//  HIDDisplay
//
//  Created by AB on 4/22/19.
//

#import "HIDDisplayIOReportingInterface.h"
#import "HIDDisplayPrivate.h"
#import "HIDDisplayInterfacePrivate.h"
#import "HIDDisplayCAPI.h"

#import <AssertMacros.h>
#import <IOKit/hid/AppleHIDUsageTables.h>
#import <IOKit/hid/IOHIDKeys.h>
#import <HID/HIDDevice.h>
#import <HID/HIDElement.h>


@implementation HIDDisplayIOReportingInterface
{
    NSDictionary<NSNumber*,HIDElement*>* _usageElementMap;
    IOReportingInputDataHandler _dataHandler;
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

-(nullable instancetype) initWithService:(io_service_t __unused) service
{
    return nil;
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
    [self.device setCancelHandler:(HIDBlock)handler];
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
    
    NSDictionary *matching = @{@kIOHIDElementUsagePageKey: @(kHIDPage_AppleVendorDisplayIOReporting)};
    
    
    [self.device setInputElementMatching:matching];
    
    [self.device setInputElementHandler:^(HIDElement *element) {
        
        __strong HIDDisplayIOReportingInterface *strongSelf = weakSelf;
        
        if (!strongSelf) {
            return;
        }
        
        
        NSInteger usage  = element.usage;
        NSInteger usagePage = element.usagePage;
        
        if (usagePage != kHIDPage_AppleVendorDisplayIOReporting && usage != kHIDUsage_AppleVendorDisplayIOReporting_Input) {
            return;
        }
    
        if (strongSelf->_dataHandler) {
            strongSelf->_dataHandler((__bridge CFDataRef)element.dataValue);
        }
        
    }];
    
    
    [self.device setDispatchQueue:_queue];
    
    [self.device activate];
    
}

-(void) cancel
{
    [self.device cancel];
}

@end
