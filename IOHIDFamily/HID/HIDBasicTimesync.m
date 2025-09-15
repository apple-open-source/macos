/*!
 * HIDBasicTimeSync.m
 * HID
 *
 * Copyright © 2025 Apple Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>
#import <HID/HIDBasicTimeSync.h>
#import <HID/NSError+IOReturn.h>
#import <os/assumes.h>
#import <AssertMacros.h>
#import <HID/HIDDevice.h>
#import <HID/HIDServiceClient.h>
#import <IOKit/hid/IOHIDLibPrivate.h>
#import <IOKit/hid/IOHIDPrivateKeys.h>

#if TIMESYNC_BASIC_AVAILABLE

#import <TimeSync/TimeSync.h>
#import <SoftLinking/WeakLinking.h>

WEAK_IMPORT_OBJC_CLASS(TSClockManager);
WEAK_IMPORT_OBJC_CLASS(TSUserFilteredClock);

@implementation HIDBasicTimeSync
{
    TSClockIdentifier _clockID;
    TSClockManager *_tsMgr;
    TSUserFilteredClock *_tsClock;
    BOOL _active;
}

- (instancetype)init
{
    self = [super initInternal];

    _tsMgr = [TSClockManager sharedClockManager];

    return self;
}

- (uint64_t)syncedTimeFromData:(NSData *)timeData error:(out NSError * _Nullable * _Nullable)outError
{
    uint64_t inTimestamp = 0;
    uint64_t outSyncedAbs = 0;
    uint64_t syncDeltaAbs = 0;
    uint64_t syncDeltaNs = 0;
    mach_timebase_info_data_t timeBase;
    uint64_t localAbs = mach_absolute_time();
    IOReturn status = kIOReturnSuccess;

    os_assert(self.state == HIDTimeSyncStateActivate);

    require_action(_active, bail, status = kIOReturnOffline);
    require_action(timeData.length == sizeof(inTimestamp), bail, status = kIOReturnBadArgument);
    require_action_quiet(_tsClock && _tsClock.lockState == TSClockLocked, bail, {
        os_log_error(_IOHIDLog(), "TimeSync: not locked, clockID: 0x%llx state: %d",
            (unsigned long long)_tsClock.clockIdentifier, (int)_tsClock.lockState);
        status = kIOReturnNotReady;
    });

    mach_timebase_info(&timeBase);

    inTimestamp = *(uint64_t *)timeData.bytes;
    outSyncedAbs = [_tsClock convertFromDomainToMachAbsoluteTime:inTimestamp];

    syncDeltaAbs = (localAbs > outSyncedAbs) ? (localAbs - outSyncedAbs) : (outSyncedAbs - localAbs);
    syncDeltaNs = (uint64_t)((syncDeltaAbs * timeBase.numer) / (timeBase.denom));
    os_log_debug(_IOHIDLog(), "W2 btclk(ns):%llu local abs:%llu Synced ts:%llu remote->local latency(ns):%s%llu", inTimestamp, localAbs, outSyncedAbs, (localAbs > outSyncedAbs) ? "+" : "-", syncDeltaNs);

bail:
    if (kIOReturnSuccess != status && outError) {
        *outError = [NSError errorWithIOReturn:status];
    }

    return outSyncedAbs;
}


- (NSData * _Nullable)dataFromSyncedTime:(uint64_t)syncedTime error:(out NSError * _Nullable * _Nullable)outError
{
    NSData * outTimestamp = nil;
    IOReturn status = kIOReturnSuccess;

    os_assert(self.state == HIDTimeSyncStateActivate);

    require_action(_active, bail, status = kIOReturnOffline);

    // Not yet supported.
    status = kIOReturnUnsupported;

bail:
    if (kIOReturnSuccess != status && outError) {
        *outError = [NSError errorWithIOReturn:status];
    }

    return outTimestamp;
}

- (void)handleActivate
{
    BOOL ok = [self setProviderProperty:@(YES) forKey:@kIOHIDTimeSyncEnabledKey];
    if (!ok) {
        os_log_error(_IOHIDLog(), "handleActivate enabling TS failed");
    }
}

- (void)handleCancel
{
    BOOL ok = [self setProviderProperty:@(NO) forKey:@kIOHIDTimeSyncEnabledKey];
    if (!ok) {
        os_log_error(_IOHIDLog(), "handleCancel disabling TS failed");
    }
}

- (void)handlePropertyUpdate:(NSDictionary *)properties
{
    TSClockIdentifier newClockID = 0;
    id active = properties[@kIOHIDTimeSyncActiveKey];

    id clockID = properties[@kIOHIDTimeSyncTSClockIDKey];
    if ([clockID isKindOfClass:[NSNumber class]]) {
        newClockID = [(NSNumber *)clockID unsignedLongLongValue];
    }

    if (!_active && [active isEqual:@(TRUE)]) {
        _clockID = newClockID;
        _tsClock = (TSUserFilteredClock *)[_tsMgr clockWithClockIdentifier:_clockID];
        if (!_tsClock) {
            os_log_error(_IOHIDLog(), "Couldn't create TSUserFilteredClock!");
            return;
        }

        _active = YES;

        self.eventHandler(HIDTimeSyncEventActive, HIDTimeSyncPrecisionUnknown);
    } else if (_active && ![active isEqual:@(TRUE)]) {
        _active = NO;
        self.eventHandler(HIDTimeSyncEventInactive, HIDTimeSyncPrecisionUnknown);

        _tsClock = nil;
    }
}

@end

#endif
