//
//  PMExtendedBatteryTypesAndConstants.m
//  PowerManagement
//
//  Created by Pablo Pons Bordes on 2/27/25.
//

#import <os/feature_private.h>

#import "PMExtendedBatteryTypesAndConstants.h"

#if WATCH_POWER_RANGER

NSString *const PMExtendedBatteryMachServiceName = @"com.apple.powerd.extendedbattery";

NSString *const PMExtendedBatteryErrorDomain = @"com.apple.powerd.extendedBattery";

os_log_t pm_extendedBattery_log(void) {
    static os_log_t __logger = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        __logger = os_log_create("com.apple.powerd", "ExtendedBattery");
    });
    return __logger;
}

BOOL PMExtendedBatteryFeatureFlagEnabled(void) {
    return (os_feature_enabled(ExtendedBattery, classic_testing_extended_battery) ||
            os_feature_enabled(ExtendedBattery, tinker_extended_battery));
}

NSError *PMExtendedBatteryError(PMExtendedBatteryErrorCode code,
                                NSString *debugDescription,
                                NSError *_Nullable underlyingError) {
    NSMutableDictionary *userInfo = [NSMutableDictionary new];
    userInfo[NSDebugDescriptionErrorKey] = debugDescription;
    if (underlyingError != nil) userInfo[NSUnderlyingErrorKey] = underlyingError;
    return [NSError errorWithDomain: PMExtendedBatteryErrorDomain
                               code: code
                           userInfo: userInfo];
}

PMExtendedBatteryState PMExtendedBatteryStateFromNSInteger(NSInteger integer) {
    PMExtendedBatteryState castInteger = (PMExtendedBatteryState)integer;
    switch (castInteger) {
        case PMExtendedBatteryStateOn:
        case PMExtendedBatteryStateOff:
            return castInteger;
    }
    
    os_log_error(pm_extendedBattery_log(), "Invalid integer (%ld) for PMExtendedBatteryState.", (long)integer);
    return PMExtendedBatteryStateOff;
}

NSString *NSStringFromPMExtendedBatteryState(PMExtendedBatteryState state) {
    switch (state) {
        case PMExtendedBatteryStateOff: return @"PMExtendedBatteryStateOff";
        case PMExtendedBatteryStateOn: return @"PMExtendedBatteryStateOn";
    }

    os_log_error(pm_extendedBattery_log(), "Invalid state value (%ld) to convert into string.", (long)state);
    return [NSString stringWithFormat:@"PMExtendedBatteryStateUnexpected(%ld)", (long)state];
    
}

PMExtendedBatteryFeatureAvailable PMExtendedBatteryFeatureAvailableFromNSInteger(NSInteger integer) {
    PMExtendedBatteryFeatureAvailable castSupport = (PMExtendedBatteryFeatureAvailable)integer;
    switch (castSupport) {
        case PMExtendedBatteryFeatureAvailableFalse:
        case PMExtendedBatteryFeatureAvailableTrue:
        case PMExtendedBatteryFeatureAvailableUnknown:
            return castSupport;
    }
    
    os_log_error(pm_extendedBattery_log(), "Invalid integer (%ld) for PMExtendedBatteryFeatureAvailable.", (long)integer);
    return PMExtendedBatteryFeatureAvailableUnknown;
}

NSString *NSStringFromPMExtendedBatteryFeatureAvailable(PMExtendedBatteryFeatureAvailable available) {
    switch (available) {
        case PMExtendedBatteryFeatureAvailableFalse: return @"PMExtendedBatteryFeatureAvailableFalse";
        case PMExtendedBatteryFeatureAvailableTrue: return @"PMExtendedBatteryFeatureAvailableTrue";
        case PMExtendedBatteryFeatureAvailableUnknown: return @"PMExtendedBatteryFeatureAvailableUnknown";
    }

    os_log_error(pm_extendedBattery_log(), "Invalid feature available value (%ld) to convert into string.", (long)available);
    return [NSString stringWithFormat:@"PMExtendedBatteryFeatureAvailableUnexpected(%ld)", (long)available];
}

#endif // WATCH_POWER_RANGER
