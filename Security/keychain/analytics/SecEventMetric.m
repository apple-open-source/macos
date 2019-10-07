/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */


#import "keychain/analytics/SecEventMetric.h"
#import "keychain/analytics/SecC2DeviceInfo.h"
#import "keychain/analytics/C2Metric/SECC2MPMetric.h"
#import "keychain/analytics/C2Metric/SECC2MPGenericEvent.h"
#import "keychain/analytics/C2Metric/SECC2MPGenericEventMetric.h"
#import "keychain/analytics/C2Metric/SECC2MPGenericEventMetricValue.h"
#import "keychain/analytics/C2Metric/SECC2MPError.h"

#import <os/log.h>

@interface SecEventMetric ()
@property NSString *eventName;
@property NSMutableDictionary *attributes;
@end

@implementation SecEventMetric

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)initWithEventName:(NSString *)name
{
    if ((self = [super init]) == NULL) {
        return self;
    }
    self.eventName = name;
    self.attributes = [NSMutableDictionary dictionary];
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    self = [super init];
    if (self) {
        NSMutableSet *attributeClasses = [[[self class] supportedAttributeClasses] mutableCopy];
        [attributeClasses addObject:[NSDictionary class]];

        _eventName = [coder decodeObjectOfClass:[NSString class] forKey:@"eventName"];
        _attributes = [coder decodeObjectOfClasses:attributeClasses forKey:@"attributes"];

        if (!_eventName || !_attributes) {
            return NULL;
        }
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:self.eventName forKey:@"eventName"];
    [coder encodeObject:self.attributes forKey:@"attributes"];
}

+ (NSSet *)supportedAttributeClasses {
    static NSSet *supported = NULL;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSArray *clss = @[
            [NSString class],
            [NSNumber class],
            [NSDate class],
            [NSError class],
        ];
        supported = [NSSet setWithArray:clss];
    });
    return supported;
}

- (void)setObject:(nullable id)object forKeyedSubscript:(NSString *)key {
    bool found = false;
    if (key == NULL) {
        return;
    }
    if (object) {
        for (Class cls in [[self class] supportedAttributeClasses]) {
            if ([object isKindOfClass:cls]) {
                found = true;
                break;
            }
        }
        if (!found) {
            os_log(OS_LOG_DEFAULT, "genericMetric  %{public}@ with unhandled metric type: %{public}@", key, NSStringFromClass([object class]));
            return;
        }
    }
    @synchronized(self) {
        self.attributes[key] = object;
    }
}
- (void)setMetricValue:(nullable id)metric forKey:(NSString *)key {
    self[key] = metric;
}

- (uint64_t)convertTimeIntervalToServerTime:(NSTimeInterval)timeInterval
{
    return (uint64_t)((timeInterval + NSTimeIntervalSince1970) * 1000.);
}

- (SECC2MPGenericEvent *)genericEvent {
    SECC2MPGenericEvent* genericEvent = [[SECC2MPGenericEvent alloc] init];

    genericEvent.name = self.eventName;
    genericEvent.type = SECC2MPGenericEvent_Type_cloudkit_client;

    genericEvent.timestampStart = 0;
    genericEvent.timestampEnd = 0;
    [self.attributes enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key, id  _Nonnull obj, BOOL * _Nonnull stop) {
        SECC2MPGenericEventMetric* metric = [[SECC2MPGenericEventMetric alloc] init];
        metric.key = key;

        metric.value = [[SECC2MPGenericEventMetricValue alloc] init];
        if ([obj isKindOfClass:[NSError class]]) {
            metric.value.errorValue = [self generateError:obj];
        } else if ([obj isKindOfClass:[NSDate class]]) {
            metric.value.dateValue = [self convertTimeIntervalToServerTime:[obj timeIntervalSinceReferenceDate]];
        } else if ([obj isKindOfClass:[NSNumber class]]) {
            metric.value.doubleValue = [obj doubleValue];
        } else if ([obj isKindOfClass:[NSString class]]) {
            metric.value.stringValue = obj;
        } else {
            // should never happen since setObject:forKeyedSubscript: validates the input and rejects invalid input
            return;
        }
        if (metric.value) {
            [genericEvent addMetric:metric];
        }
    }];
    return genericEvent;
}

- (SECC2MPError*) generateError:(NSError*)error
{
    SECC2MPError* generatedError = [[SECC2MPError alloc] init];
    generatedError.errorDomain = error.domain;
    generatedError.errorCode = error.code;
    if ([SecC2DeviceInfo isAppleInternal]) {
        generatedError.errorDescription = error.userInfo[NSLocalizedDescriptionKey];
    }
    NSError* underlyingError = error.userInfo[NSUnderlyingErrorKey];
    if (underlyingError) {
        generatedError.underlyingError = [self generateError:underlyingError];
    }
    return generatedError;
}

@end
