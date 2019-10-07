//
//  HIDAnalyticsReporter.h
//  HIDAnalytics
//
//  Created by AB on 11/26/18.
//  Copyright © 2018 apple. All rights reserved.
//


#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class HIDAnalyticsEvent;

@interface HIDAnalyticsReporter : NSObject

//Register event with reporter. Event need to be registered for collecting data.
-(void) registerEvent:(HIDAnalyticsEvent*) event;

//Unregister event with reporter. Stop collecting data for given event
-(void) unregisterEvent:(HIDAnalyticsEvent*) event;

//Start collecting data for all registered events
-(void) start;

//Stop collecting  data for all registered events
-(void) stop;

-(void) dispatchAnalyticsForEvent:(HIDAnalyticsEvent*) event;

@end

NS_ASSUME_NONNULL_END

