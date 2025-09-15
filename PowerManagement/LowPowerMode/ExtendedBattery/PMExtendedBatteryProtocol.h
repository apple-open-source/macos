//
//  PMExtendedBatteryProtocol.h
//  LowPowerMode-Embedded
//
//  Created by Pablo Pons Bordes on 2/26/25.
//

#import <Foundation/Foundation.h>
#import <LowPowerMode/PMExtendedBatteryTypesAndConstants.h>

#if WATCH_POWER_RANGER

NS_ASSUME_NONNULL_BEGIN

typedef void (^PMExtendedBatteryCompletionHandler)(PMExtendedBatteryFeatureAvailable available,
                                                   PMExtendedBatteryState state,
                                                   NSError *_Nullable error);

@protocol PMExtendedBatteryServiceProtocol <NSObject>

- (void)extendedBatteryInfoWithCompletion:(PMExtendedBatteryCompletionHandler)handler;

- (void)setState:(PMExtendedBatteryState)state
      fromSource:(NSString *)source
  withCompletion:(PMExtendedBatteryCompletionHandler)handler;

- (void)setFeatureAvailable:(PMExtendedBatteryFeatureAvailable)available
                 fromSource:(NSString *)source
             withCompletion:(PMExtendedBatteryCompletionHandler)handler;

@end

@protocol PMExtendedBatteryClientProtocol <NSObject>

- (void)remoteServiceDidUpdateFeatureAvailable:(PMExtendedBatteryFeatureAvailable)available
                                         state:(PMExtendedBatteryState)state;

@end

NS_ASSUME_NONNULL_END

#endif // WATCH_POWER_RANGER
