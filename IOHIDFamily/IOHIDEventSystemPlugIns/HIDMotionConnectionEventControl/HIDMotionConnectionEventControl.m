/*
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 2008 Apple, Inc.  All Rights Reserved.
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
#include <AssertMacros.h>
#import <Foundation/Foundation.h>
#import "HIDMotionConnectionEventControl.h"
#import <IOKit/hid/IOHIDUsageTables.h>
#import <IOKit/hid/IOHIDServiceKeys.h>
#import <IOKit/hid/IOHIDEventTypes.h>
#import <IOKit/hid/IOHIDLibPrivate.h>
#import <Security/SecEntitlements.h>
#import <Security/SecTaskPriv.h>
#include <bsm/libbsm.h>
#import <System/sys/codesign.h>
#import <ManagedAssets/ManagedAssetsInUseArbiter.h>
#import <RunningBoardServices/RBSProcessHandle.h>
#include <IOKit/hid/IOHIDEventSystemPrivate.h>


typedef struct {
    NSInteger usagePage;
    NSInteger usage;
} UsagePair;

static const UsagePair usagePairs[] = {
    { kHIDPage_AppleVendor, kHIDUsage_AppleVendor_Gyro },
    { kHIDPage_AppleVendor, kHIDUsage_AppleVendor_Accelerometer },
    { kHIDPage_AppleVendor, kHIDUsage_AppleVendor_Compass },
    { kHIDPage_AppleVendorMotion, kHIDUsage_AppleVendorMotion_Motion },
    { kHIDPage_AppleVendorMotion, kHIDUsage_AppleVendorMotion_DeviceMotion6 },
    { kHIDPage_AppleVendorMotion, kHIDUsage_AppleVendorMotion_DeviceMotion },
    { kHIDPage_AppleVendorMotion, kHIDUsage_AppleVendorMotion_DeviceMotion10 },
    { kHIDPage_AppleVendorMotion, kHIDUsage_AppleVendorMotion_DeviceMotion3 }
};

static bool isPlatformBinary (audit_token_t token) {
    
    SecTaskRef taskRef = SecTaskCreateWithAuditToken(NULL, token);
    uint32_t csStatus = SecTaskGetCodeSignStatus(taskRef);
    if (taskRef) {
        CFRelease(taskRef);
    } else {
        HIDLogError("SecTaskCreateWithAuditToken failed for %d", audit_token_to_pid(token));
    }
        
    /* check if valid and platform binary, but not platform path */
    return ((csStatus & (CS_VALID | CS_PLATFORM_BINARY | CS_PLATFORM_PATH)) == (CS_VALID | CS_PLATFORM_BINARY));
}

@implementation HIDMotionConnectionEventControl {
    HIDBlock _cancelHandler;
    dispatch_queue_t _queue;
    audit_token_t _token;
    MAAppStatusObserverToken _observer;
    MAAppInUseStatus _status;
    NSUInteger _filteredCount;
    NSUInteger _processedCount;
}

typedef enum  {
    kMatchProcessDefault = 0x1,
    kMatchProcessInclude,
    kMatchProcessExclude
} MatchStatus;

// example to confogure defaults
// login -f mobile defaults write  com.apple.HIDMotionConnectionEventControl includelist -array "com.apple.GazeBenchmark"
//

static  MatchStatus matchProcess (RBSProcessHandle * process)
{
    NSUserDefaults * defaults = [[NSUserDefaults alloc] initWithSuiteName:@"com.apple.HIDMotionConnectionEventControl"];
    NSArray * includelist = [defaults arrayForKey:@"includelist"];
    HIDLogDebug("%s includelist:%@", __PRETTY_FUNCTION__, includelist);
    if (includelist) {
        for (id bundle in includelist) {
            if ([bundle isEqual:process.bundle.identifier]) {
                return kMatchProcessInclude;
            }
        }
    }

    NSArray * excludelist = [defaults arrayForKey:@"excludelist"];
    HIDLogDebug("%s excludelist:%@", __PRETTY_FUNCTION__, excludelist);
    if (excludelist) {
        for (id bundle in excludelist) {
            if ([bundle isEqual:process.bundle.identifier]) {
                return kMatchProcessExclude;
            }
        }
    }

    return kMatchProcessDefault;
}

+ (BOOL)matchConnection:(HIDConnection *)connection
{
    MatchStatus status;
    BOOL ret = NO;
    RBSProcessIdentifier * pid = nil;
    RBSProcessHandle * handle = nil;
    audit_token_t token;
    IOHIDEventSystemConnectionEntitlements * entitlements = IOHIDEventSystemConnectionGetEntitlements((IOHIDEventSystemConnectionRef)connection);

    [connection getAuditToken:&token];

    pid = [RBSProcessIdentifier identifierWithPid:audit_token_to_pid(token)];
    handle = [RBSProcessHandle handleForIdentifier:pid error:nil];
   
    status = matchProcess(handle);
    switch (status) {
        case kMatchProcessInclude:
            ret = YES;
            break;
        case kMatchProcessExclude:
            ret = NO;
            break;
        default:
            break;
    }
    
    require_quiet(status == kMatchProcessDefault, exit);
    
    require (entitlements->entitlements == 0, exit);
    
    require (isPlatformBinary(token) == false, exit);
    
    ret = YES;

exit:
    
    if (ret) {
        HIDLog("%s:%d bundleid:%@ and connection:%@", __PRETTY_FUNCTION__, ret, handle.bundle.identifier, connection.uuid);
    }
    return ret;
}

- (instancetype)initWithConnection:(HIDConnection *)connection
{
    self = [super init];
    if (!self) {
        return self;
    }
    
    self.connection = connection;
    
    [connection getAuditToken:&_token];
    
    return self;
}

- (void)dealloc
{
}

- (NSString *)description
{
    return [[NSString alloc] initWithFormat: @"<HIDMotionConnectionEventControl connection:%@>", self.connection.uuid];
}

- (id)propertyForKey:(NSString *)key
{
    id result = nil;
    
    if ([key isEqualToString:@(kIOHIDConnectionPluginDebugKey)]) {
        // debug dictionary that gets captured by hidutil
        NSMutableDictionary *debug = [NSMutableDictionary new];
        
        debug[@"Class"] = @"HIDMotionConnectionEventControl";
        debug[@"PID"] =  @(audit_token_to_pid(_token));
        debug[@"InUseStatus"] =  @(_status);
        debug[@"FilteredCount"] =  @(_filteredCount);
        debug[@"ProcessedCount"] =  @(_processedCount);
        result = debug;
    }
    
    return result;
}

- (BOOL)setProperty:(id)value forKey:(NSString *)key
{

    bool result = false;
    
    return result;
}

- (HIDEvent *)filterEvent:(HIDEvent *)event
{
    HIDEvent * result = event;
    id sender = (__bridge_transfer id)_IOHIDEventCopyAttachment((__bridge IOHIDEventRef)event, kIOHIDEventAttachmentSender, 0);
    HIDEventService * service = [sender isKindOfClass:[HIDEventService class]] ?  (HIDEventService *) sender : nil;
 

    require (kMAAppInUseStatusImmersive != _status, exit);
    
    // Filter only built-in services.
    require_quiet([event integerValueForField:kIOHIDEventFieldIsBuiltIn], exit);

    // Filter if it's a motion event.
    switch (event.type) {
        case kIOHIDEventTypeGyro:
        case kIOHIDEventTypeAccelerometer:
        case kIOHIDEventTypeCompass:
            result = nil;
        default:
            break;
    }
    
    require_quiet (result && service, exit);

    // Filter if the event came from a motion service.
    for ( NSUInteger i = 0; i < sizeof(usagePairs)/sizeof(usagePairs[0]); i++) {
        if ([service conformsToUsagePage:usagePairs[i].usagePage usage:usagePairs[i].usage]) {
            result = nil;
        }
    }

exit:
    ++_processedCount;

    if (!result) {
        ++_filteredCount;
        HIDLogDebug("%@:HIDMotionConnectionEventControl filter event:%@ sender:%@", self.connection.uuid, event, service);
    }
    return result;
}

- (void)setCancelHandler:(HIDBlock)handler
{
    _cancelHandler = handler;
}

- (void)activate
{
    NSError * err = nil;
    
    [MAInUseArbiter queryInUseStatusWithAuditToken:_token appStatus:&_status error:&err];
 
    HIDLog("%@:HIDMotionConnectionEventControl queryInUseStatusWithAuditToken: status:%@(%lu) error:%@", self.connection.uuid, [HIDMotionConnectionEventControl statusToStr:_status], (unsigned long)_status, err);

    __weak HIDMotionConnectionEventControl * weakSelf = self;
    _observer = [MAInUseArbiter addInUseObserverWithAuditToken:_token
                                                 dispatchQueue:_queue
                                                         block:^(NSArray<MAAppInUseStatusResult*> *_Nullable updateResults, NSError *_Nullable error) {
        __strong HIDMotionConnectionEventControl * strongSelf = weakSelf;
        if (strongSelf) {
            HIDLog("%@:HIDMotionConnectionEventControl queryInUseStatusWithAuditToken: status::%@(%lu) error:%@", self.connection.uuid, [HIDMotionConnectionEventControl statusToStr:updateResults[0].status], (unsigned long)updateResults[0].status, error);
            strongSelf->_status = updateResults[0].status;
        }
    }];
}

- (void)cancel
{
    dispatch_async(_queue, ^{
        if (self->_observer) {
            [MAInUseArbiter removeInUseObserver:self->_observer];
        }
        self->_cancelHandler();
        self->_cancelHandler = nil;
    });
}

- (void)setDispatchQueue:(dispatch_queue_t)queue
{
    _queue = queue;
}


+ (NSString *)statusToStr:(NSInteger) status
{
    switch (status) {
        case kMAAppInUseStatusBackground:
            return @"Background";
        case kMAAppInUseStatusForegroundInactive:
            return @"ForegroundInactive";
        case kMAAppInUseStatusForegroundActive:
            return @"ForegroundActive";
        case kMAAppInUseStatusImmersive:
            return @"Immersive";
    }
    return @"Unknown";
}
@end
