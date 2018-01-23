/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#import "RateLimiter.h"
#import <utilities/debugging.h>
#import "sec_action.h"
#import <CoreFoundation/CFPreferences.h>   // For clarity. Also included in debugging.h

#if !TARGET_OS_BRIDGE
#import <WirelessDiagnostics/WirelessDiagnostics.h>
#import "keychain/analytics/awd/AWDMetricIds_Keychain.h"
#import "keychain/analytics/awd/AWDKeychainCKKSRateLimiterOverload.h"
#import "keychain/analytics/awd/AWDKeychainCKKSRateLimiterTopWriters.h"
#import "keychain/analytics/awd/AWDKeychainCKKSRateLimiterAggregatedScores.h"
#endif

@interface RateLimiter()
@property (readwrite, nonatomic) NSDictionary *config;
@property (nonatomic) NSArray<NSMutableDictionary<NSString *, NSDate *> *> *groups;
@property (nonatomic) NSDate *lastJudgment;
@property (nonatomic) NSDate *overloadUntil;
@property (nonatomic) NSString *assetType;
#if !TARGET_OS_BRIDGE
@property (nonatomic) NSMutableArray<NSNumber *> *badnessData;
@property (nonatomic) AWDServerConnection *awdConnection;
#endif
@end

@implementation RateLimiter

- (instancetype)initWithConfig:(NSDictionary *)config {
    self = [super init];
    if (self) {
        _config = config;
        _assetType = nil;
        [self reset];
        [self setUpAwdMetrics];
    }
    return self;
}

- (instancetype)initWithPlistFromURL:(NSURL *)url {
    self = [super init];
    if (self) {
        _config = [NSDictionary dictionaryWithContentsOfURL:url];
        if (!_config) {
            secerror("RateLimiter[?]: could not read config from %@", url);
            return nil;
        }
        _assetType = nil;
        [self reset];
        [self setUpAwdMetrics];
    }
    return self;
}

// TODO implement MobileAsset loading
- (instancetype)initWithAssetType:(NSString *)type {
    return nil;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if (!coder) {
        return nil;
    }
    self = [super init];
    if (self) {
        _groups = [coder decodeObjectOfClasses:[NSSet setWithObjects: [NSArray class],
                                                                      [NSMutableDictionary class],
                                                                      [NSString class],
                                                                      [NSDate class],
                                                                      nil]
                                        forKey:@"RLgroups"];
        _overloadUntil = [coder decodeObjectOfClass:[NSDate class] forKey:@"RLoverLoadedUntil"];
        _lastJudgment = [coder decodeObjectOfClass:[NSDate class] forKey:@"RLlastJudgment"];
        _assetType = [coder decodeObjectOfClass:[NSString class] forKey:@"RLassetType"];
#if !TARGET_OS_BRIDGE
        _badnessData = [coder decodeObjectOfClasses:[NSSet setWithObjects: [NSArray class],
                                                                           [NSNumber class],
                                                                           nil]
                                             forKey:@"RLbadnessData"];
#endif
        if (!_assetType) {
            // This list of types might be wrong. Be careful.
            _config = [coder decodeObjectOfClasses:[NSSet setWithObjects: [NSMutableArray class],
                                                                          [NSDictionary class],
                                                                          [NSString class],
                                                                          [NSNumber class],
                                                                          [NSDate class],
                                                                          nil]
                                            forKey:@"RLconfig"];
        }
        [self setUpAwdMetrics];
    }
    return self;
}

- (NSInteger)judge:(id _Nonnull)obj at:(NSDate * _Nonnull)time limitTime:(NSDate * _Nullable __autoreleasing * _Nonnull)limitTime {
    
    //sudo defaults write /Library/Preferences/com.apple.security DisableKeychainRateLimiting -bool YES
    NSNumber *disabled = CFBridgingRelease(CFPreferencesCopyValue(CFSTR("DisableKeychainRateLimiting"),
                                                                  CFSTR("com.apple.security"),
                                                                  kCFPreferencesAnyUser, kCFPreferencesAnyHost));
    if ([disabled isKindOfClass:[NSNumber class]] && [disabled boolValue] == YES) {
        static dispatch_once_t token;
        static sec_action_t action;
        dispatch_once(&token, ^{
            action = sec_action_create("ratelimiterdisabledlogevent", 60);
            sec_action_set_handler(action, ^{
                secnotice("ratelimit", "Rate limiting disabled, returning automatic all-clear");
          });
        });
        sec_action_perform(action);

        *limitTime = nil;
        return RateLimiterBadnessClear;
    }
    
    RateLimiterBadness badness = RateLimiterBadnessClear;

    if (self.overloadUntil) {
        if ([time timeIntervalSinceDate:self.overloadUntil] >= 0) {
            [self trim:time];
        }
        if (self.overloadUntil) {
            *limitTime = self.overloadUntil;
            badness = RateLimiterBadnessOverloaded;
        }
    }

    if (badness == RateLimiterBadnessClear &&
        ((self.lastJudgment && [time timeIntervalSinceDate:self.lastJudgment] > [self.config[@"general"][@"maxItemAge"] intValue]) ||
        [self stateSize] > [self.config[@"general"][@"maxStateSize"] unsignedIntegerValue])) {
        [self trim:time];
        if (self.overloadUntil) {
            *limitTime = self.overloadUntil;
            badness = RateLimiterBadnessOverloaded;
        }
    }

    if (badness != RateLimiterBadnessClear) {
#if !TARGET_OS_BRIDGE
        self.badnessData[badness] = @(self.badnessData[badness].intValue + 1);
#endif
        return badness;
    }
    
    NSDate *resultTime = [NSDate distantPast];
    for (unsigned long idx = 0; idx < self.groups.count; ++idx) {
        NSDictionary *groupConfig = self.config[@"groups"][idx];
        NSString *name;
        if (idx == 0) {
            name = groupConfig[@"property"];    // global bucket, does not correspond to object property
        } else {
            name = [self getPropertyValue:groupConfig[@"property"] object:obj];
        }
        // Pretend this property doesn't exist. Should be returning an error instead but currently it's only used with
        // approved properties 'accessGroup' and 'uuid' and if the item doesn't have either it's sad times anyway.
        // <rdar://problem/33434425> Improve rate limiter error handling
        if (!name) {
            secerror("RateLimiter[%@]: Got nil instead of property named %@", self.config[@"general"][@"name"], groupConfig[@"property"]);
            continue;
        }
        NSDate *singleTokenTime = [self consumeTokenFromBucket:self.groups[idx]
                                                        config:groupConfig
                                                          name:name
                                                            at:time];
        if (singleTokenTime) {
            resultTime = [resultTime laterDate:singleTokenTime];
            badness = MAX([groupConfig[@"badness"] intValue], badness);
        }
    }

#if !TARGET_OS_BRIDGE
    self.badnessData[badness] = @(self.badnessData[badness].intValue + 1);
#endif
    *limitTime = badness == RateLimiterBadnessClear ? nil : resultTime;
    self.lastJudgment = time;
    return badness;
}

- (NSDate *)consumeTokenFromBucket:(NSMutableDictionary *)group
                            config:(NSDictionary *)config
                              name:(NSString *)name
                                at:(NSDate *)time {
    NSDate *threshold = [time dateByAddingTimeInterval:-([config[@"capacity"] intValue] * [config[@"rate"] intValue])];
    NSDate *bucket = group[name];

    if (!bucket || [bucket timeIntervalSinceDate:threshold] < 0) {
        bucket = threshold;
    }

    // Implicitly track the number of tokens in the bucket.
    // "Would the token I need have been generated in the past or in the future?"
    bucket = [bucket dateByAddingTimeInterval:[config[@"rate"] intValue]];
    group[name] = bucket;
    return ([bucket timeIntervalSinceDate:time] <= 0) ? nil : bucket;
}

- (BOOL)isEqual:(id)object {
    if (![object isKindOfClass:[RateLimiter class]]) {
        return NO;
    }
    RateLimiter *other = (RateLimiter *)object;
    return ([self.config isEqual:other.config] &&
            [self.groups isEqual:other.groups] &&
            [self.lastJudgment isEqual:other.lastJudgment] &&
            ((self.overloadUntil == nil && other.overloadUntil == nil) || [self.overloadUntil isEqual:other.overloadUntil]) &&
            ((self.assetType == nil && other.assetType == nil) || [self.assetType isEqualToString:other.assetType]));
}

- (void)reset {
    NSMutableArray *newgroups = [NSMutableArray new];
    for (unsigned long idx = 0; idx < [self.config[@"groups"] count]; ++idx) {
        [newgroups addObject:[NSMutableDictionary new]];
    }
    self.groups = newgroups;
    self.lastJudgment = [NSDate distantPast];   // will cause extraneous trim on first judgment but on empty groups
    self.overloadUntil = nil;
#if !TARGET_OS_BRIDGE
    // Corresponds to the number of RateLimiterBadness enum values
    self.badnessData = [[NSMutableArray alloc] initWithObjects:@0, @0, @0, @0, @0, nil];
#endif
}

- (void)trim:(NSDate *)time {
    int threshold = [self.config[@"general"][@"maxItemAge"] intValue];
    for (NSMutableDictionary *group in self.groups) {
        NSSet *toRemove = [group keysOfEntriesPassingTest:^BOOL(NSString *key, NSDate *obj, BOOL *stop) {
            return [time timeIntervalSinceDate:obj] > threshold;
        }];
        [group removeObjectsForKeys:[toRemove allObjects]];
    }

    if ([self stateSize] > [self.config[@"general"][@"maxStateSize"] unsignedIntegerValue]) {
        // Trimming did not reduce size (enough), we need to take measures
        self.overloadUntil = [time dateByAddingTimeInterval:[self.config[@"general"][@"overloadDuration"] intValue]];
        secerror("RateLimiter[%@] state size %lu exceeds max %lu, overloaded until %@",
                 self.config[@"general"][@"name"],
                 (unsigned long)[self stateSize],
                 [self.config[@"general"][@"maxStateSize"] unsignedLongValue],
                 self.overloadUntil);
#if !TARGET_OS_BRIDGE
        AWDKeychainCKKSRateLimiterOverload *metric = [AWDKeychainCKKSRateLimiterOverload new];
        metric.ratelimitertype = self.config[@"general"][@"name"];
        AWDPostMetric(AWDComponentId_Keychain, metric);
#endif
    } else {
        self.overloadUntil = nil;
    }
}

- (NSUInteger)stateSize {
    NSUInteger size = 0;
    for (NSMutableDictionary *group in self.groups) {
        size += [group count];
    }
    return size;
}

- (NSString *)diagnostics {
    return [NSString stringWithFormat:@"RateLimiter[%@]\nconfig:%@\ngroups:%@\noverloaded:%@\nlastJudgment:%@",
            self.config[@"general"][@"name"],
            self.config,
            self.groups,
            self.overloadUntil,
            self.lastJudgment];
}

//This could probably be improved, rdar://problem/33416163
- (NSString *)getPropertyValue:(NSString *)selectorString object:(id)obj {
    if ([selectorString isEqualToString:@"accessGroup"] ||
        [selectorString isEqualToString:@"uuid"]) {
        
        SEL selector = NSSelectorFromString(selectorString);
        IMP imp = [obj methodForSelector:selector];
        NSString *(*func)(id, SEL) = (void *)imp;
        return func(obj, selector);
    } else {
        seccritical("RateLimter[%@]: \"%@\" is not an approved selector string", self.config[@"general"][@"name"], selectorString);
        return nil;
    }
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_groups forKey:@"RLgroups"];
    [coder encodeObject:_overloadUntil forKey:@"RLoverloadedUntil"];
    [coder encodeObject:_lastJudgment forKey:@"RLlastJudgment"];
    [coder encodeObject:_assetType forKey:@"RLassetType"];
    if (!_assetType) {
        [coder encodeObject:_config forKey:@"RLconfig"];
    }
#if !TARGET_OS_BRIDGE
    [coder encodeObject:_badnessData forKey:@"RLbadnessData"];
#endif
}

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (NSArray *)topOffenders:(int)num {
    NSInteger idx = [self.config[@"general"][@"topOffendersPropertyIndex"] integerValue];
    NSDate *now = [NSDate date];
    NSSet *contenderkeys = [self.groups[idx] keysOfEntriesPassingTest:^BOOL(NSString *key, NSDate *obj, BOOL *stop) {
        return [now timeIntervalSinceDate:obj] > 0 ? YES : NO;
    }];
    if ([contenderkeys count] == 0) {
        return [NSArray new];
    }
    NSDictionary *contenders = [NSDictionary dictionaryWithObjects:[self.groups[idx] objectsForKeys:[contenderkeys allObjects]
                                                                                     notFoundMarker:[NSDate date]]
                                                           forKeys:[contenderkeys allObjects]];
    return [[[contenders keysSortedByValueUsingSelector:@selector(compare:)] reverseObjectEnumerator] allObjects];
}

- (void)setUpAwdMetrics {
#if !TARGET_OS_BRIDGE
    self.awdConnection = [[AWDServerConnection alloc] initWithComponentId:AWDComponentId_Keychain];

    [self.awdConnection registerQueriableMetric:AWDMetricId_Keychain_CKKSRateLimiterTopWriters callback:^(UInt32 metricId) {
        AWDKeychainCKKSRateLimiterTopWriters *metric = [AWDKeychainCKKSRateLimiterTopWriters new];
        NSArray *offenders = [self topOffenders:3];
        for (NSString *offender in offenders) {
            [metric addWriter:offender];
        }
        metric.ratelimitertype = self.config[@"general"][@"name"];
        AWDPostMetric(metricId, metric);
    }];

    [self.awdConnection registerQueriableMetric:AWDMetricId_Keychain_CKKSRateLimiterAggregatedScores callback:^(UInt32 metricId) {
        AWDKeychainCKKSRateLimiterAggregatedScores *metric = [AWDKeychainCKKSRateLimiterAggregatedScores new];
        for (NSNumber *num in self.badnessData) {
            [metric addData:[num unsignedIntValue]];
        }
        metric.ratelimitertype = self.config[@"general"][@"name"];
        AWDPostMetric(metricId, metric);
        // Corresponds to the number of RateLimiterBadness enum values
        self.badnessData = [[NSMutableArray alloc] initWithObjects:@0, @0, @0, @0, @0, nil];
    }];
#endif
}

@end
