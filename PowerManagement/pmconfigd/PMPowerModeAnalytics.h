//
//  PMPowerModeAnalytics.h
//  PowerManagement
//
//  Created by Saurabh Shah on 4/26/21.
//

#ifndef PMPowerModeAnalytics_h
#define PMPowerModeAnalytics_h

#import <Foundation/Foundation.h>

#define POWERMODE_ANALYTICS_ON_DEVICE (!TARGET_OS_SIMULATOR && !XCTEST)

@interface PMPowerModeAnalytics : NSObject

+ (void)sendAnalyticsEvent:(NSNumber * _Nonnull)newState
          withBatteryLevel:(NSNumber * _Nonnull)level
                fromSource:(NSString * _Nonnull)source
               withCharger:(NSNumber * _Nonnull)pluggedIn
     withDurationInMinutes:(NSNumber * _Nonnull)duration
                 forStream:(NSString * _Nonnull)stream;

+ (void)sendAnalyticsDaily:(NSNumber * _Nonnull)enabled
                 forStream:(NSString * _Nonnull)stream;

@end

#endif /* PMPowerModeAnalytics_h */
