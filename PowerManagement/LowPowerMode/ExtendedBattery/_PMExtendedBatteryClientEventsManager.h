//
//  _PMExtendedBatteryClientEventsManager.h
//  LowPowerMode-Embedded
//
//  Created by Pablo Pons Bordes on 3/6/25.
//

#import <Foundation/Foundation.h>
#import <LowPowerMode/PMExtendedBatteryTypesAndConstants.h>

#if WATCH_POWER_RANGER

NS_ASSUME_NONNULL_BEGIN
/// Handle incoming events
@protocol _PMExtendedBatteryClientEventsManagerObserver;


/// Manages the handling of incoming XPC events related to extended battery information.
/// A process can only register one handler per event stream. Registering multiple handlers for the same stream results
/// in undefined behavior. This class is a singleton (use `+[_PMExtendedBatteryClientEventsManager sharedInstance]`) to
/// prevent multiple instances from interfering with event handling.
@interface _PMExtendedBatteryClientEventsManager : NSObject

/// Method unavailable, use `+[_PMExtendedBatteryClientEventsManager sharedInstance]` instead
- (instancetype)init NS_UNAVAILABLE;
/// Method unavailable, use `+[_PMExtendedBatteryClientEventsManager sharedInstance]` instead
+ (instancetype)new NS_UNAVAILABLE;


/// Obtain initialized shared instance
+ (instancetype)sharedInstance;

/// Registers an observer to be notified of a specific event.
/// - Parameters:
///   - observer: The object to be notified.
///   - eventName: The name of the event.
- (void)registerObserver:(id<_PMExtendedBatteryClientEventsManagerObserver>)observer forEvent:(NSString *)eventName;

/// Unregisters an observer, preventing further event notifications.
/// - Parameter observer: The object to unregister.
- (void)unregisterObserver:(id<_PMExtendedBatteryClientEventsManagerObserver>)observer;

@end

@protocol _PMExtendedBatteryClientEventsManagerObserver <NSObject>

- (void)extendedBatteryClientEventsManager:(_PMExtendedBatteryClientEventsManager *)manager
                          didReceivedEvent:(NSString *)eventName;

@end
NS_ASSUME_NONNULL_END

#endif // WATCH_POWER_RANGER
