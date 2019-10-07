//
//  HIDAnalyticsEventField.h
//  HIDAnalytics
//
//  Created by AB on 11/26/18.
//  Copyright © 2018 apple. All rights reserved.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@protocol HIDAnalyticsEventFieldProtocol <NSObject>

@property id __nullable value;

@optional

-(void) setIntegerValue:(uint64_t) value;

@end


@interface HIDAnalyticsEventField : NSObject <HIDAnalyticsEventFieldProtocol>

@property(readonly) NSString *fieldName;

-(nullable instancetype) initWithName:(NSString*) name;

@end


NS_ASSUME_NONNULL_END
