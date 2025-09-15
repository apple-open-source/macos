//
//  PMExtendedBattery.h
//  LowPowerMode-Embedded
//
//  Created by Pablo Pons Bordes on 2/26/25.
//

#import <LowPowerMode/PMExtendedBatteryTypesAndConstants.h>

#if WATCH_POWER_RANGER

NS_ASSUME_NONNULL_BEGIN

#pragma mark - Sources

extern NSString *const kPMEBSourceCarousel;
extern NSString *const kPMEBSourceSettings;
extern NSString *const kPMEBSourcePMTool;

@protocol PMExtendedBatteryDelegate;

#pragma mark - Interface

/// Instance of this class allows to manage and obtain information associated with Extended Battery the process
/// that create instance of this class must have `com.apple.powerd.extendedbattery` entitlement.
/// This entitlement is required to allow the connection with the remote service and will give read access.
@interface PMExtendedBattery : NSObject

/// Initializes an Extended Battery object.
/// - Parameters:
///   - delegate: The delegate to notify of updates.
///   - delegateQueue: The serial queue to call the delegate on. If `nil`, an internal serial queue will be created.
- (instancetype)initWithDelegate:(id<PMExtendedBatteryDelegate> _Nullable)delegate queue:(dispatch_queue_t _Nullable)delegateQueue NS_DESIGNATED_INITIALIZER;

#pragma mark Feature Available

/// Indicates if the Extended Battery feature is available on this device.
/// Once the value is different from `Unknown`, it will not change until the device reboots.
@property (nonatomic, readonly) PMExtendedBatteryFeatureAvailable featureAvailable;

/// Indicates if the Extended Battery feature is available on this device.
/// Once the value is different from `Unknown`, it will not change until the device reboots.
- (void)featureAvailableWithCompletion:(PMGetExtendedBatteryAvailableCompletionHandler)handler;

/// Sets the extended battery feature available value. If the given value is different than
/// `PMExtendedBatteryFeatureAvailableTrue`, the state value will be set to `PMExtendedBatteryStateOff`.
/// This method requires the `com.apple.powerd.extendedbattery.setInfo` entitlement.
/// - Parameters:
///   - featureAvailable: The new feature availability value.  The state will be updated as follows:
///     - `PMExtendedBatteryFeatureAvailableUnknown`: Updates the state to `off`, overwriting any previous value.
///     - `PMExtendedBatteryFeatureAvailableFalse`: Updates the state to `off`, overwriting any previous value.
///     - `PMExtendedBatteryFeatureAvailableTrue`: Preserves any existing state value; otherwise, sets it to `on`.
///   - source: The source of the change (e.g., `kPMEBSourceSettings`).
///   - handler: The completion handler to be called after the state is set.
- (void)setFeatureAvailable:(PMExtendedBatteryFeatureAvailable)featureAvailable
                 fromSource:(NSString *)source
             withCompletion:(PMSetExtendedBatteryAvailableCompletionHandler)handler;

/// Sets the extended battery feature available value. If the given value is different than
/// `PMExtendedBatteryFeatureAvailableTrue`, the state will be set to`PMExtendedBatteryStateOff`.
/// This method requires the `com.apple.powerd.extendedbattery.setInfo` entitlement.
/// - Parameters:
///   - available: The new availability state.
///   - source: The source of the change (e.g., `kPMEBSourceSettings`).
- (void)setFeatureAvailable:(PMExtendedBatteryFeatureAvailable)available fromSource:(NSString *)source;

#pragma mark State

/// Provides the current extended battery state. If extended battery is not available, this will always be `PMExtendedBatteryStateOff`.
@property (nonatomic, readonly) PMExtendedBatteryState state;

/// Provides the current extended battery state. If extended battery is not available, this will always be `PMExtendedBatteryStateOff`.
- (void)stateWithCompletion:(PMGetExtendedBatteryStateCompletionHandler)handler;

/// Sets the extended battery state. If extended battery is not available, this method will not take effect.
/// this method requires `com.apple.powerd.extendedbattery.setState` or `com.apple.powerd.extendedbattery.setInfo`
/// entitlements.
/// - Parameters:
///   - state: The new state to set.
///   - source: The source of the change (e.g., `kPMEBSourceSettings`).
///   - handler: The completion handler to be called after the state is set.
- (void)setState:(PMExtendedBatteryState)state
      fromSource:(NSString *)source
  withCompletion:(PMSetExtendedBatteryStateCompletionHandler)handler;

/// Sets the extended battery state. If extended battery is not available, this method will not take effect.
/// this method requires `com.apple.powerd.extendedbattery.setState` or `com.apple.powerd.extendedbattery.setInfo`
/// entitlements.
/// - Parameters:
///   - state: The new state to set.
///   - source: The source of the change (e.g., `kPMEBSourceSettings`).
- (void)setState:(PMExtendedBatteryState)state fromSource:(NSString *)source;

/// The delegate object to notify about updates.
@property (nullable, nonatomic, weak) id<PMExtendedBatteryDelegate> delegate;

@end

#pragma mark - Protocols

@protocol PMExtendedBatteryDelegate <NSObject>

@optional

/// Tells the delegate that the extended battery support has changed.
/// - Parameters:
///   - extendedBattery: The `PMExtendedBattery` instance.
///   - available: The new availability state.
- (void)extendedBattery:(PMExtendedBattery *)extendedBattery didChangeAvailable:(PMExtendedBatteryFeatureAvailable)available;

/// Tells the delegate that the extended battery state has changed.
/// - Parameters:
///   - extendedBattery: The `PMExtendedBattery` instance.
///   - state: The new battery state.
- (void)extendedBattery:(PMExtendedBattery *)extendedBattery  didChangeState:(PMExtendedBatteryState)state;

@end

NS_ASSUME_NONNULL_END

#endif // WATCH_POWER_RANGER
