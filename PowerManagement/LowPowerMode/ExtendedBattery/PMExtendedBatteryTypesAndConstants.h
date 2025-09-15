//
//  PMExtendedBatteryTypesAndConstants.h
//  LowPowerMode-Embedded
//
//  Created by Pablo Pons Bordes on 2/26/25.
//

#import <Foundation/Foundation.h>
#import <TargetConditionals.h>
#import <os/log.h>

#ifndef WATCH_POWER_RANGER
#define WATCH_POWER_RANGER TARGET_OS_WATCH
#endif // !WATCH_POWER_RANGER

#if WATCH_POWER_RANGER

NS_ASSUME_NONNULL_BEGIN

extern os_log_t pm_extendedBattery_log(void);

extern NSString *const PMExtendedBatteryMachServiceName;

/// XPC Event Stream
#define PMExtendedBatteryXPCEventStream         @"com.apple.powerd.extendedbattery"
#define PMExtendedBatteryXPCEventUpdatedInfo    @"updatedInfo"

extern BOOL PMExtendedBatteryFeatureFlagEnabled(void);

/// Extended Battery feature support
typedef NS_ENUM(NSInteger, PMExtendedBatteryFeatureAvailable) {
    PMExtendedBatteryFeatureAvailableUnknown = -1,   /// Is not possible to determine yet if Extended battery feature is available.
    PMExtendedBatteryFeatureAvailableFalse = 0,      /// Extended battery is disabled.
    PMExtendedBatteryFeatureAvailableTrue = 1        /// Extended battery is available.
};

/// Converts an `NSInteger` to a `PMExtendedBatteryFeatureAvailable` value.
///  Returns `PMExtendedBatteryFeatureAvailableUnknown` if the input integer does not correspond to a valid `PMExtendedBatteryFeatureAvailable` value.
/// - Parameter integer: Integer to convert.
extern PMExtendedBatteryFeatureAvailable PMExtendedBatteryFeatureAvailableFromNSInteger(NSInteger integer);

/// Obtain an string representation of the given `PMExtendedBatteryFeatureAvailable` value.
/// - Parameter support: value to represent as string.
extern NSString *NSStringFromPMExtendedBatteryFeatureAvailable(PMExtendedBatteryFeatureAvailable support);

/// Extended Battery state
typedef NS_ENUM(NSInteger, PMExtendedBatteryState) {
    PMExtendedBatteryStateOff = 0,   /// Extended Battery is not active
    PMExtendedBatteryStateOn = 1     /// Extended Battery is active
};

/// Converts an `NSInteger` to a `PMExtendedBatteryState` value.
/// Returns `PMExtendedBatteryStateOff` if the input integer does not correspond to a valid `PMExtendedBatteryState` value.
/// - Parameter integer: Integer to convert.
extern PMExtendedBatteryState PMExtendedBatteryStateFromNSInteger(NSInteger integer);

/// Obtain an sting representation of the given state `PMExtendedBatteryState` value.
/// - Parameter state: value to represent as string
extern NSString *NSStringFromPMExtendedBatteryState(PMExtendedBatteryState state);

#pragma mark - Error

extern NSString *const PMExtendedBatteryErrorDomain;

typedef NS_ENUM(NSInteger, PMExtendedBatteryErrorCode) {
    PMExtendedBatteryErrorCodeConnection          = -1000,  /// Failed remote process connection
    PMExtendedBatteryErrorCodeMissingEntitlement  = -1001,  /// Missing entitlement
    PMExtendedBatteryErrorCodeInvalidParameter    = -1002,  /// Invalid Parameters
};

/// Create new Error with `PMExtendedBatteryErrorDomain` as domain.
/// - Parameters:
///   - code: error code
///   - debugDescription: String which will be shown when constructing the debugDescription of the NSError, to be used
///                       when debugging or when formatting the error with %@.  This string will never be used in
///                       localizedDescription, so will not be shown to the user.
///   - underlyingError: Underlying error code to add at the `userInfo` property dictionary at the new error.
extern NSError *PMExtendedBatteryError(PMExtendedBatteryErrorCode code,
                                       NSString *debugDescription,
                                       NSError *_Nullable underlyingError);


#pragma mark - Completion Blocks

typedef void (^PMGetExtendedBatteryAvailableCompletionHandler)(PMExtendedBatteryFeatureAvailable available, NSError *_Nullable error);
typedef void (^PMSetExtendedBatteryAvailableCompletionHandler)(PMExtendedBatteryFeatureAvailable available, PMExtendedBatteryState state, NSError *_Nullable error);

typedef void (^PMGetExtendedBatteryStateCompletionHandler)(PMExtendedBatteryState state, NSError *_Nullable error);
typedef void (^PMSetExtendedBatteryStateCompletionHandler)(PMExtendedBatteryState state, NSError *_Nullable error);

NS_ASSUME_NONNULL_END

#endif // WATCH_POWER_RANGER
