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

#if OCTAGON
#import "CKKSRateLimiter.h"
#import <utilities/debugging.h>
#import <TargetConditionals.h>

typedef NS_ENUM(int, BucketType) {
    All,
    Group,
    UUID,
};

@interface CKKSRateLimiter()
@property (readwrite) NSDictionary<NSString *, NSNumber *> *config;
@property NSMutableDictionary<NSString *, NSDate *> *buckets;
@property NSDate *overloadUntil;
@end

@implementation CKKSRateLimiter

- (instancetype)init {
    return [self initWithCoder:nil];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    self = [super init];
    if (self) {
        if (coder) {
            NSDictionary *encoded;
            encoded = [coder decodeObjectOfClasses:[NSSet setWithObjects:[NSDictionary class],
                                                    [NSString class],
                                                    [NSDate class],
                                                    nil]
                                            forKey:@"buckets"];

            // Strongly enforce types for the dictionary
            if (![encoded isKindOfClass:[NSDictionary class]]) {
                return nil;
            }
            for (id key in encoded) {
                if (![key isKindOfClass:[NSString class]]) {
                    return nil;
                }
                if (![encoded[key] isKindOfClass:[NSDate class]]) {
                    return nil;
                }
            }
            _buckets = [encoded mutableCopy];
        } else {
            _buckets = [NSMutableDictionary new];
        }


        _overloadUntil = nil;
        // this should be done from a downloadable plist, rdar://problem/29945628
        _config = [NSDictionary dictionaryWithObjectsAndKeys:
                   @30  , @"rateAll",
                   @120 , @"rateGroup",
                   @600 , @"rateUUID",
                   @20  , @"capacityAll",
                   @10  , @"capacityGroup",
                   @3   , @"capacityUUID",
                   @250 , @"trimSize",
                   @3600, @"trimTime",
                   @1800, @"overloadDuration", nil];
    }
    return self;
}

- (BOOL)isEqual: (id) object {
    if(![object isKindOfClass:[CKKSRateLimiter class]]) {
        return NO;
    }

    CKKSRateLimiter* obj = (CKKSRateLimiter*) object;

    return ([self.config isEqual: obj.config] &&
            [self.buckets isEqual: obj.buckets] &&
            ((self.overloadUntil == nil && obj.overloadUntil == nil) || ([self.overloadUntil isEqual: obj.overloadUntil]))) ? YES : NO;
}

- (int)rate:(enum BucketType)type {
    switch (type) {
        case All:
            return [self.config[@"rateAll"] intValue];
        case Group:
            return [self.config[@"rateGroup"] intValue];
        case UUID:
            return [self.config[@"rateUUID"] intValue];
    }
}

- (int)capacity:(enum BucketType)type {
    switch (type) {
        case All:
            return [self.config[@"capacityAll"] intValue];
        case Group:
            return [self.config[@"capacityGroup"] intValue];
        case UUID:
            return [self.config[@"capacityUUID"] intValue];
    }
}

- (NSDate *)consumeTokenFromBucket:(NSString *)name
                              type:(enum BucketType)type
                                at:(NSDate *)time {
    NSDate *threshold = [time dateByAddingTimeInterval:-([self capacity:type] * [self rate:type])];
    NSDate *bucket = self.buckets[name];

    if (!bucket || [bucket timeIntervalSinceDate:threshold] < 0) {
        bucket = threshold;
    }

    // Implicitly track the number of tokens in the bucket.
    // "Would the token I need have been generated in the past or in the future?"
    bucket = [bucket dateByAddingTimeInterval:[self rate:type]];
    self.buckets[name] = bucket;
    return ([bucket timeIntervalSinceDate:time] <= 0) ? nil : [bucket copy];
}

- (int)judge:(CKKSOutgoingQueueEntry * _Nonnull const)entry
          at:(NSDate * _Nonnull)time
   limitTime:(NSDate * _Nonnull __autoreleasing * _Nonnull) limitTime
{
    if (self.overloadUntil) {
        if ([time timeIntervalSinceDate:self.overloadUntil] >= 0) {
            [self trim:time];
        }
        if (self.overloadUntil) {
            *limitTime = [self.overloadUntil copy];
            return 5;
        }
    }

    NSDate *all = self.buckets[@"All"];
    if ((all && [time timeIntervalSinceDate:all] > [self.config[@"trimTime"] intValue]) ||
        self.buckets.count >= [self.config[@"trimSize"] unsignedIntValue]) {
        [self trim:time];
        if (self.overloadUntil) {
            *limitTime = self.overloadUntil;
            return 5;
        }
    }

    int badness = 0;
    NSDate *sendTime = [self consumeTokenFromBucket:@"All" type:All at:time];
    if (sendTime) {
        badness = 1;
    }
    NSDate *backoff = [self consumeTokenFromBucket:[NSString stringWithFormat:@"G:%@", entry.accessgroup] type:Group at:time];
    if (backoff) {
        sendTime = sendTime == nil ? backoff : [sendTime laterDate:backoff];
        badness = ([backoff timeIntervalSinceDate:
                    [time dateByAddingTimeInterval:([self rate:Group] * 2)]] < 0) ? 2 : 3;
    }
    backoff = [self consumeTokenFromBucket:[NSString stringWithFormat:@"U:%@", entry.uuid] type:UUID at:time];
    if (backoff) {
        sendTime = sendTime == nil ? backoff : [sendTime laterDate:backoff];
        badness = 4;
    }

    *limitTime = sendTime;
    return badness;
}

- (NSUInteger)stateSize {
    return self.buckets.count;
}

- (void)reset {
    self.buckets = [NSMutableDictionary new];
    self.overloadUntil = nil;
}

- (void)trim:(NSDate *)time {
    int threshold = [self.config[@"trimTime"] intValue];
    NSSet *toRemove = [self.buckets keysOfEntriesPassingTest:^BOOL(NSString *key, NSDate *obj, BOOL *stop) {
        return [time timeIntervalSinceDate:obj] > threshold;
    }];
    
    // Nothing to remove means everybody keeps being noisy. Tell them to go away.
    if ([toRemove count] == 0) {
        self.overloadUntil = [self.buckets[@"All"] dateByAddingTimeInterval:[self.config[@"overloadDuration"] unsignedIntValue]];
        seccritical("RateLimiter overloaded until %@", self.overloadUntil);
    } else {
        self.overloadUntil = nil;
        [self.buckets removeObjectsForKeys:[toRemove allObjects]];
    }
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:self.buckets forKey:@"buckets"];
}

- (NSString *)diagnostics {
    NSMutableString *diag = [NSMutableString stringWithFormat:@"RateLimiter config: %@\n", [self.config description]];

    if (self.overloadUntil != nil) {
        [diag appendFormat:@"Overloaded until %@, %lu total buckets\n", self.overloadUntil, (unsigned long)[self.buckets count]];
    } else {
        [diag appendFormat:@"Not overloaded, %lu total buckets\n", (unsigned long)[self.buckets count]];
    }

    NSArray *offenders = [self topOffendingAccessGroups:10];
    if (offenders) {
        [diag appendFormat:@"%lu congested buckets. Top offenders: \n%@ range %@ to %@\n",
         (unsigned long)[offenders count], offenders, self.buckets[offenders[0]], self.buckets[offenders[[offenders count] - 1]]];
    } else {
        [diag appendString:@"No buckets congested"];
    }

    return diag;
}

- (NSArray *)topOffendingAccessGroups:(NSUInteger)num {
    NSDate *now = [NSDate date];
    NSSet *congestedKeys = [self.buckets keysOfEntriesPassingTest:^BOOL(NSString *key, NSDate *obj, BOOL *stop) {
        if (![key hasPrefix:@"G:"]) {
            return NO;
        }
        return [now timeIntervalSinceDate:obj] <= 0 ? NO : YES;
    }];

    if ([congestedKeys count] > 0) {
        // Marker must be type NSDate but can be anything since we know all objects will be in the dictionary
        NSDictionary *congested = [NSDictionary dictionaryWithObjects:[self.buckets objectsForKeys:[congestedKeys allObjects]
                                                                                    notFoundMarker:[NSDate date]]
                                                              forKeys:[congestedKeys allObjects]];
        NSArray *sortedKeys = [[[congested keysSortedByValueUsingSelector:@selector(compare:)] reverseObjectEnumerator] allObjects];
        if ([sortedKeys count] > num) {
            return [sortedKeys subarrayWithRange:NSMakeRange(0, num)];
        } else {
            return sortedKeys;
        }
    } else {
        return nil;
    }
}

+ (BOOL)supportsSecureCoding {
    return YES;
}

@end

#endif // OCTAGON
