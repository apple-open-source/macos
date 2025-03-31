/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * This document is the property of Apple Inc.
 * It is considered confidential and proprietary.
 *
 * This document may not be reproduced or transmitted in any form,
 * in whole or in part, without the express written permission of
 * Apple Inc.
 */

#import <Foundation/Foundation.h>
#import "IOHIDSensorPowerLoggingFilter.h"
#import <IOKit/hid/IOHIDUsageTables.h>
#import <IOKit/hid/IOHIDServiceKeys.h>
#import <IOKit/hid/IOHIDEventTypes.h>
#include <IOKit/hid/IOHIDLibPrivate.h>
#include <IOKit/hid/IOHIDEventSystemConnection.h>
#include <bsm/libbsm.h>
#import <RunningBoardServices/RBSProcessHandle.h>
#import <PowerLog/PerfPowerServicesTelemetry.h>
#include <AssertMacros.h>

// Subsystem, Category, and Name constants for PerfPowerServices telemetry metrics
#define kSensorUsageMetricSubsystem      "Sensors"
#define kSensorUsageMetricCategory       "SensorUsage"
#define kSensorUsageMetricProcessName    "ProcessName"
#define kSensorUsageMetricReportInterval "ReportInterval"
#define kSensorUsageMetricUsagePage      "UsagePage"
#define kSensorUsageMetricUsage          "Usage"

#define kUnknownClientName               "Unknown Client"

static dispatch_queue_t ppsQueue; /// shared dispatch queue for servicing calls to PPS

static PPSTelemetryIdentifier * streamId = nil; /// PPS telemetry stream identifier

@implementation IOHIDSensorPowerLoggingFilter {
    HIDBlock _cancelHandler;
    bool _activated;
    
    uint64_t _ppsCount;     ///< debug value: number of calls to log PPS telemetry
    uint64_t _lastInterval; ///< debug value: last interval value logged as PPS telemetry

    NSNumber * _usagePage; /// cache the primary usage page property for the matched service
    NSNumber * _usage;     /// cache the primary usage property for the matched service
    
    NSMutableDictionary * _clients; /// mapping from client UUIDs to requested report intervals
}

+ (BOOL)matchService:(HIDEventService *)service
             options:(NSDictionary *)options
               score:(NSInteger *)score
{
    // Plugin uses passive matching only via the info plist. Currently matched usages are:
    // kHIDPage_Sensor:
    //     kHIDUsage_Snsr_Environmental_AtmosphericPressure
    // kHIDPage_AppleVendor:
    //     kHIDUsage_AppleVendor_Accelerometer
    //     kHIDUsage_AppleVendor_Gyro
    //     kHIDUsage_AppleVendor_Compass
    //     kHIDUsage_AppleVendor_Rose
    //     kHIDUsage_AppleVendor_Accelerometer2
    //     kHIDUsage_AppleVendor_IMU
    //     kHIDUsage_AppleVendor_Magnetometer_mT
    // kHIDPage_AppleVendorMotion:
    //     kHIDUsage_AppleVendorMotion_Motion
    //     kHIDUsage_AppleVendorMotion_DeviceMotion6
    //     kHIDUsage_AppleVendorMotion_DeviceMotion
    //     kHIDUsage_AppleVendorMotion_DeviceMotion10
    //     kHIDUsage_AppleVendorMotion_DeviceMotion3
    return true;
}

- (instancetype)initWithService:(HIDEventService *)service
{
    self = [super init];
    if (!self) {
        return self;
    }
    
    _service = service;
    _clients = [NSMutableDictionary new];
    
    _usagePage = [_service propertyForKey:@(kIOHIDPrimaryUsagePageKey)];
    _usage     = [_service propertyForKey:@(kIOHIDPrimaryUsageKey)];
    _ppsCount  = 0;

    static dispatch_once_t dqOnceToken; // once token for initializing the shared dispatch queue used for PPS calls
    dispatch_once(&dqOnceToken, ^{
        ppsQueue = dispatch_queue_create("com.apple.HID.SensorPowerLog", DISPATCH_QUEUE_SERIAL);
    });

    static dispatch_once_t idOnceToken; // once token for initializing the telemetry stream id
    dispatch_once(&idOnceToken, ^{
        dispatch_async(ppsQueue, ^{
            streamId = PPSCreateTelemetryIdentifier(@(kSensorUsageMetricSubsystem), @(kSensorUsageMetricCategory));
        });
    });

    HIDLog("IOHIDSensorPowerLoggingFilter::initWithService: %@", _service);
    return self;
}

- (id)propertyForKey:(NSString *)key client:(HIDConnection *)client
{
    id result = nil;
    
    if ([key isEqualToString:@(kIOHIDServiceFilterDebugKey)]) {
        // debug dictionary that gets captured by hidutil
        NSMutableDictionary * debug = [NSMutableDictionary new];
        debug[@"Class"]         = @"IOHIDSensorPowerLoggingFilter";
        debug[@"cancelHandler"] = _cancelHandler ? @YES : @NO;
        result = debug;
    }
    else if ([key isEqualToString:@"TestHIDSensorPowerLoggingFilterKey"]) {
        // property queried by unit test
        NSMutableDictionary * debug = [NSMutableDictionary new];
        debug[@"clients"]      = _clients;
        debug[@"logCount"]     = [NSNumber numberWithUnsignedLongLong:_ppsCount];
        debug[@"lastInterval"] = [NSNumber numberWithUnsignedLongLong:_lastInterval];
        debug[@"PPSInitDone"]  = streamId ? @YES : @NO;
        result = debug;
    }
    
    return result;
}

- (BOOL)setProperty:(id)value
             forKey:(NSString *)key
             client:(HIDConnection *)client
{
    bool result = false;

    if ([key isEqualToString:@kIOHIDReportIntervalKey]) {
        
        NSString * procName = nil;
        NSNumber * interval = nil;
        
        require_quiet(value && [value isKindOfClass:[NSNumber class]], exit);
        
        interval = (NSNumber *)value;
        procName = client ? [IOHIDSensorPowerLoggingFilter getClientProcessName:client] : @(kUnknownClientName);
        
        HIDLogDebug("IOHIDSensorPowerLoggingFilter: set report interval:%@ client:%@", interval, client);
        
        // update client->interval mapping
        if (client.uuid) {

            NSMutableDictionary* clientInfo = _clients[client.uuid];

            if (clientInfo) {
                NSNumber* oldInterval = clientInfo[@(kIOHIDReportIntervalKey)];
                if ([oldInterval isEqualToNumber:interval]) {
                    // skip without logging telemetry
                    return true;
                }
                clientInfo[@(kIOHIDReportIntervalKey)] = interval;
            }
            else {
                clientInfo = [[NSMutableDictionary alloc] initWithDictionary:@{
                    @(kIOHIDReportIntervalKey) : interval,
                    @"ProcessName"             : procName
                }];
            }

            [_clients setObject:clientInfo forKey:client.uuid];
        }

        // send telemetry to PerfPowerServices
        [self sendPPSTelemetry:interval
                 withUsagePage:_usagePage
                      andUsage:_usage
                     forClient:procName];
        
        result = true;
    }

exit:
    return result;
}


- (HIDEvent *)filterEvent:(HIDEvent *)event
{
    return event;
}

- (HIDEvent *)filterEventMatching:(NSDictionary *)matching
                            event:(HIDEvent *)event
                        forClient:(HIDConnection *)client
{
    return event;
}

- (void)setCancelHandler:(HIDBlock)handler
{
    _cancelHandler = handler;
}

- (void)activate
{
    _activated = true;
}

- (void)cancel
{
    self->_cancelHandler();
    self->_cancelHandler = nil;
}


- (void)clientNotification:(HIDConnection *)client added:(BOOL)added
{
    NSDictionary * clientInfo  = nil;
    NSNumber     * oldInterval = nil;
    NSString     * procName    = nil;
    
    // ignore unidentifiable clients
    require_quiet(!added && client && client.uuid, exit);
    
    // ignore clients who never set a report interval
    clientInfo = [_clients objectForKey:client.uuid];
    require_quiet(clientInfo, exit);
    
    HIDLogDebug("IOHIDSensorPowerLoggingFilter: client removed:%@", client);
    [_clients removeObjectForKey:client.uuid];

    oldInterval = clientInfo[@(kIOHIDReportIntervalKey)];
    if (![oldInterval isEqualToNumber:@(0)]) {
        procName = clientInfo[@"ProcessName"];
        [self sendPPSTelemetry:@(0)
                 withUsagePage:_usagePage
                      andUsage:_usage
                     forClient:procName];
    }

exit:
    return;
}

- (void)sendPPSTelemetry:(NSNumber * _Nonnull)interval
           withUsagePage:(NSNumber * _Nonnull)usagePage
                andUsage:(NSNumber * _Nonnull)usage
               forClient:(NSString * _Nonnull)name
{
    if (!streamId) {
        return;
    }

    NSDictionary * telemetry = @{
        @(kSensorUsageMetricProcessName)    : name,
        @(kSensorUsageMetricReportInterval) : interval,
        @(kSensorUsageMetricUsagePage)      : usagePage,
        @(kSensorUsageMetricUsage)          : usage
    };
    HIDLogDebug("IOHIDSensorPowerLoggingFilter: sending telemetry: %@", telemetry);

    ++_ppsCount; // for use in tests
    _lastInterval = interval.unsignedLongLongValue; // for use in tests
    dispatch_async(ppsQueue, ^{
        PPSSendTelemetry(streamId, telemetry);
    });

    return;
}

+ (NSString *)getClientProcessName:(HIDConnection *)client
{
    return (__bridge NSString *)IOHIDEventSystemConnectionGetProcName((__bridge IOHIDEventSystemConnectionRef)client);
}

@end
