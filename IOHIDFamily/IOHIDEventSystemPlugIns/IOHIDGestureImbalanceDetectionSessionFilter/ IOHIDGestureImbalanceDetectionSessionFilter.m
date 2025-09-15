//
//  IOHIDGestureImbalanceDetectionSessionFilter.m
//
//  Created by Austin Erck on 7/9/24.
//

#import <Foundation/Foundation.h>
#import <HID/HIDEventService.h>
#import <HID/HIDEvent.h>
#include <IOKit/hid/IOHIDEventTypes.h>
#include <IOKit/hid/IOHIDEventSystemKeys.h>
#include <os/variant_private.h>
#include <stdlib.h>

#import "IOHIDGestureImbalanceDetectionSessionFilter.h"
#import "GestureStats.h"

#define kMaxTrackedDevices 16
#define kAppleBluetoothVendorID 76
#define kAppleUSBVendorID 1452

extern os_log_t logHandle(void)
{
    static os_log_t __logObj = NULL;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        __logObj = os_log_create("com.apple.iohid.internal", "GestureImbalanceDetection");
    });
    return __logObj;
}

@interface IOHIDGestureImbalanceDetectionSessionFilter ()

@property(nonnull) NSMutableDictionary<NSNumber*, GestureStats*>* deviceGestureStats;
@property(nonatomic) NSUInteger randomDropRate;

@end

@implementation IOHIDGestureImbalanceDetectionSessionFilter

- (nullable instancetype)initWithSession:(nonnull HIDSession*)session
{
    self = [super init];

    if (self)
    {
        _deviceGestureStats = [NSMutableDictionary new];
        _randomDropRate = 0;
    }

    return self;
}

- (NSString *)description
{
    return @"IOHIDGestureImbalanceDetectionSessionFilter";
}

- (void)activate
{
    // Intentionally empty.
}

- (nullable id)propertyForKey:(nonnull NSString*)key
{
    id value = nil;

    if ([key isEqualToString:@(kIOHIDSessionFilterDebugKey)])
    {
        NSMutableDictionary* debug = [NSMutableDictionary new];
        debug[@"Class"] = @"IOHIDGestureImbalanceDetectionSessionFilter";
        [self.deviceGestureStats enumerateKeysAndObjectsUsingBlock:^(NSNumber* key, GestureStats* stats, BOOL* stop) {
            debug[[NSString stringWithFormat:@"%@", key]] = stats.debug;
        }];

        value = debug;
    }

    return value;
}

- (BOOL)setProperty:(nullable id)value forKey:(nonnull NSString*)key
{
    return false;
}

/// Handles callbacks when a service is added/removed. Used to track connected Multitouch devices
- (void)serviceNotification:(HIDEventService*)service added:(BOOL)added
{
    if (![self isMultitouchPointingDevice:service])
    {
        return;
    }

    if (added)
    {
        // Add new device
        self.deviceGestureStats[@(service.serviceID)] = [[GestureStats alloc] initWithServiceID:@(service.serviceID)];
    }
    else
    {
        // Mark device as removed
        self.deviceGestureStats[@(service.serviceID)].removedAt = NSDate.now;
    }

    [self removeOldDevices];
}

/// Filter that allows all events to pass, but logs gesture phase changes
- (nullable HIDEvent*)filterEvent:(nonnull HIDEvent*)event forService:(nonnull HIDEventService*)service
{
    // We expect that randomDropRate will always be 0 on non-internal builds
    // This special casing prevents calling arc4random_uniform if not required
    [self.deviceGestureStats[@(service.serviceID)] handleHIDEvent:event];
    // Never filter any events
    return event;
}

- (void)dealloc
{
    //Intentionally empty.
}

- (void)setDispatchQueue:(dispatch_queue_t)queue
{
    //Intentionally empty.
}

/// Determines if the device is a post-2015 Multitouch device
/// - Parameter service: Service that is being checked
/// - Returns: Bool representing if the service is for a Multitouch device
- (BOOL)isMultitouchPointingDevice:(HIDEventService*)service
{
    //Bypass for virtual service created for testing purposes
    NSString* testPointerServiceProp = [service propertyForKey:@"TestHIDVirtualPointerService"];
    if ([testPointerServiceProp boolValue])
    {
        return true;
    }
    
    // Ignore non-AMD classes
    NSString* ioClass = [service propertyForKey:@(kIOClassKey)];
    if (![ioClass isEqualToString:@"AppleMultitouchDevice"])
    {
        return false;
    }

    // We want to capture MIDs(USB/BT), Pre-MTP Macs(USB vid & builtIn), MTP Macs(builtIn)
    bool builtIn = [[service propertyForKey:@(kIOHIDBuiltInKey)] boolValue];
    NSNumber* vendorID = [service propertyForKey:@(kIOHIDVendorIDKey)];
    return builtIn || [@[ @(kAppleBluetoothVendorID), @(kAppleUSBVendorID) ] containsObject:vendorID];
}

/// Performs cleanup of the `deviceGestureStats` dictionary and its children
/// The goal of this filter is to capture gesture imbalances in the field. If a user hits an issue, they may be inclined to power cycle a device.
/// Therefore we should only delete data once we have reached the sane maximum.
- (void)removeOldDevices
{
    NSInteger numberOfDevicesToRemove = self.deviceGestureStats.count - kMaxTrackedDevices;
    if (numberOfDevicesToRemove < 1)
    {
        return;
    }

    // Get a list of devices that can be removed sorted oldest to newest
    NSArray<GestureStats*>* removedDevices = [self.deviceGestureStats.allValues filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(GestureStats* stats, NSDictionary* bindings) {
                                                                                    return stats.removedAt != nil; // Filter only for removed devices
                                                                                }]];
    NSSortDescriptor* removedDateDescriptor = [[NSSortDescriptor alloc] initWithKey:@"removedAt" ascending:YES];
    NSArray<GestureStats*>* sortedDevicesToRemove = [removedDevices sortedArrayUsingDescriptors:@[ removedDateDescriptor ]].copy;
    os_log_error(logHandle(),"number of devices: %lu", (unsigned long)sortedDevicesToRemove.count);
    for (int index = 0; index < MIN(numberOfDevicesToRemove, sortedDevicesToRemove.count); index++)
    {
        GestureStats* stats = removedDevices[index];
        self.deviceGestureStats[stats.serviceID] = nil;
    }
}

@end
