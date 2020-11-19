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
#import "sec_action.h"
#import "keychain/ckks/CKKS.h"
#import <CoreFoundation/CFPreferences.h>   // For clarity. Also included in debugging.h

@interface RateLimiter()
@property (readwrite, nonatomic) NSDictionary *config;
@property (nonatomic) NSArray<NSMutableDictionary<NSString *, NSDate *> *> *groups;
@property (nonatomic) NSDate *lastJudgment;
@property (nonatomic) NSDate *overloadUntil;
@property (nonatomic) NSString *assetType;
@end

@implementation RateLimiter

- (instancetype)initWithConfig:(NSDictionary *)config {
    if ((self = [super init])) {
        _config = config;
        _assetType = nil;
        [self reset];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if (!coder) {
        return nil;
    }
    if ((self = [super init])) {
        _groups = [coder decodeObjectOfClasses:[NSSet setWithObjects: [NSArray class],
                                                                      [NSMutableDictionary class],
                                                                      [NSString class],
                                                                      [NSDate class],
                                                                      nil]
                                        forKey:@"RLgroups"];
        _overloadUntil = [coder decodeObjectOfClass:[NSDate class] forKey:@"RLoverLoadedUntil"];
        _lastJudgment = [coder decodeObjectOfClass:[NSDate class] forKey:@"RLlastJudgment"];
        _assetType = [coder decodeObjectOfClass:[NSString class] forKey:@"RLassetType"];
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
    }
    return self;
}

- (RateLimiterBadness)judge:(id _Nonnull)obj at:(NSDate * _Nonnull)time limitTime:(NSDate * _Nullable __autoreleasing * _Nonnull)limitTime {
    
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
                ckksnotice_global("ratelimit", "Rate limiting disabled, returning automatic all-clear");
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
            ckkserror_global("ratelimiter", "RateLimiter[%@]: Got nil instead of property named %@", self.config[@"general"][@"name"], groupConfig[@"property"]);
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
        self.overloadUntil = [time dateByAddingTimeInterval:[self.config[@"general"][@"overloadDuration"] unsignedIntValue]];
        ckkserror_global("ratelimiter", "RateLimiter[%@] state size %lu exceeds max %lu, overloaded until %@",
                 self.config[@"general"][@"name"],
                 (unsigned long)[self stateSize],
                 [self.config[@"general"][@"maxStateSize"] unsignedLongValue],
                 self.overloadUntil);
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
}

+ (BOOL)supportsSecureCoding {
    return YES;
}

@end
