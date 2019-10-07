//
//  HIDAnalyticsEventFieldHistogram.h
//  HIDAnalytics
//
//  Created by AB on 11/26/18.
//  Copyright © 2018 apple. All rights reserved.
//


#import <HIDAnalytics/HIDAnalyticsEventField.h>
#import <HIDAnalytics/HIDAnalytics.h>
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface HIDAnalyticsHistogramEventField : NSObject <HIDAnalyticsEventFieldProtocol>

@property(readonly) NSString *fieldName;

//segments is array of HIDAnalyticsHistogramConfig used for defining variable length buckets
-(nullable instancetype) initWithAttributes:(NSString*) name
                                   segments:(HIDAnalyticsHistogramSegmentConfig*) segments count:(NSInteger) count;

@end

NS_ASSUME_NONNULL_END

