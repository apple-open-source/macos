/*
 * Copyright (c) 2022 Apple Inc. All Rights Reserved.
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

#import <os/log.h>

#import "SFAnalytics+Internal.h"
#import "SFAnalyticsCollection.h"
#import "SECSFARules.h"
#import "SECSFARule.h"
#import "SECSFAAction.h"
#import "SECSFAActionAutomaticBugCapture.h"
#import "SECSFAActionTapToRadar.h"
#import "SECSFAActionDropEvent.h"
#import "SecABC.h"
#import "SecTapToRadar.h"

static NSString* SFCollectionConfig = @"SFCollectionConfig";

typedef NSMutableDictionary<NSString*, NSMutableSet<SFAnalyticsMatchingRule*>*> SFAMatchingRules;

@interface SFAnalyticsMatchingRule ()
@property (readwrite) SECSFARule *rule;
@property NSDate *lastMatch;
@property NSDictionary<NSString*,id> *matchingDictionary;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithSFARule:(SECSFARule *)rule logger:(SFAnalytics*)logger;

- (BOOL)matchAttributes:(NSDictionary *)attributes logger:(SFAnalytics *)logger;

- (BOOL)valueMatch:(id)vKey target:(id)vTarget;
- (BOOL)isSubsetMatch:(NSDictionary *)match target:(NSDictionary *)target;

@end

@implementation SFAnalyticsMatchingRule

- (instancetype)initWithSFARule:(SECSFARule *)rule logger:(SFAnalytics*)logger {
    if ((self = [super init]) == nil) {
        return nil;
    }
    self.eventName = rule.eventType;
    self.rule = rule;
    self.lastMatch = [logger datePropertyForKey:[self lastMatchTimeKey]];
    return self;
}


// match one item
- (BOOL)valueMatch:(id)vKey target:(id)vTarget {
    if ([vKey isKindOfClass:[NSString class]]) {
        return [vKey isEqual:vTarget];
    } else if ([vKey isKindOfClass:[NSNumber class]]) {
        return [vKey isEqual:vTarget];
    } else if ([vKey isKindOfClass:[NSDictionary class]]) {
        return [self isSubsetMatch:vKey target:vTarget];
    } else if ([vKey isKindOfClass:[NSArray class]]) {
        NSArray *matchArray = vKey;
        NSArray *targetArray = vTarget;
        for (id vItem in matchArray) {
            BOOL foundMatch = NO;
            for (id vTargetItem in targetArray) {
                if ([self valueMatch:vItem target:vTargetItem]) {
                    foundMatch = YES;
                }
            }
            if (!foundMatch) {
                return NO;
            }
        }
        return YES;
    }
    return NO;
}

// match a dictionary
- (BOOL)isSubsetMatch:(NSDictionary *)match target:(NSDictionary *)target
{
    if (match.count > target.count) {
        return NO;
    }
    for (NSString *key in match) {
        id vTarget = target[key];
        id vKey = match[key];

        if (![self valueMatch:vKey target:vTarget]) {
            return NO;
        }
    }
    return YES;
}

- (NSString *)lastMatchTimeKey {
    return [NSString stringWithFormat:@"SFA-LastMatchRule-%@-", self.rule.eventType];
}

- (BOOL)matchAttributes:(NSDictionary *)attributes
                 logger:(SFAnalytics *)logger
{
    if (self.rule.hasMatch) {
        @synchronized (self) {
            if (self.matchingDictionary == nil) {
                NSError *error = nil;
                NSDictionary *d = [NSPropertyListSerialization propertyListWithData:self.rule.match options:0 format:nil error:&error];
                if (d == nil || error != nil) {
                    os_log_error(OS_LOG_DEFAULT, "SFAnalyticsMatchingRule dictionary wrong");
                    return NO;
                }
                if (![d isKindOfClass:[NSDictionary class]]) {
                    return NO;
                }
                self.matchingDictionary = d;
            }

            /* check if `matchingDictionary' is a subset of `attributes' */
            if (![self isSubsetMatch:self.matchingDictionary target:attributes]) {
                return NO;
            }
        }
    }

    return YES;
}

- (BOOL)shouldRatelimit:(SFAnalytics *)logger {
    if (self.lastMatch) {
        int64_t repeatAfterSeconds = self.rule.repeatAfterSeconds;
        if (repeatAfterSeconds == 0) {
            repeatAfterSeconds = 3600 * 24; //if not set, provided sesable default: 24h
        }
        NSDate *allowedMatch = [NSDate dateWithTimeIntervalSinceNow:-1 * repeatAfterSeconds];
        if ([allowedMatch compare:self.lastMatch] != NSOrderedDescending) {
            return YES;
        }
    }

    self.lastMatch = [NSDate date];
    [logger setDateProperty:self.lastMatch forKey:[self lastMatchTimeKey]];

    return NO;
}


- (SFAnalyticsMetricsHookActions)doAction:(id<SFAnalyticsCollectionAction>)actions logger:(SFAnalytics *)logger  {

    SECSFAAction *action = self.rule.action;
    if (action == nil) {
        return 0;
    }

    if (action.hasTtr) {
        os_log(OS_LOG_DEFAULT, "SFACollection action trigger ttr: %@", self.rule.eventType);
        if ([self shouldRatelimit:logger]) {
            return 0;
        }

        SECSFAActionTapToRadar *ttr = action.ttr;
        [actions tapToRadar:ttr.alert
         description:ttr.description
                      radar:action.radarnumber
              componentName:ttr.componentName
           componentVersion:ttr.componentVersion
                componentID:ttr.componentID];
        return 0;
    } else if (action.hasAbc) {
        os_log(OS_LOG_DEFAULT, "SFACollection action trigger abc: %@", self.rule.eventType);
        if ([self shouldRatelimit:logger]) {
            return 0;
        }

        SECSFAActionAutomaticBugCapture *abc = action.abc;
        if (abc.domain == nil && abc.type == nil){
            return 0;
        }
        [actions autoBugCaptureWithType:abc.type subType:abc.subtype domain:abc.domain];
        return 0;
    } else if (action.hasDrop) {
        os_log(OS_LOG_DEFAULT, "SFACollection action trigger drop: %@", self.rule.eventType);
        SFAnalyticsMetricsHookActions dropActions = 0;
        SECSFAActionDropEvent *drop = action.drop;
        if (drop.excludeEvent) {
            dropActions |= SFAnalyticsMetricsHookExcludeEvent;
        }
        if (drop.excludeCount) {
            dropActions |= SFAnalyticsMetricsHookExcludeCount;
        }
        return dropActions;
    } else {
        os_log(OS_LOG_DEFAULT, "SFACollection unknown action: %@", self.rule.eventType);
    }
    return 0;
}

@end

@interface SFAnalyticsCollection ()
@property SFAMatchingRules *matchingRules;
@property void(^tearDownMetricsHook)(void);
@property id<SFAnalyticsCollectionAction> actions;
@property dispatch_queue_t queue;

@end

@interface DefaultCollectionActions: NSObject <SFAnalyticsCollectionAction>
@end

@implementation DefaultCollectionActions

- (void)autoBugCaptureWithType:(NSString *)type subType:(NSString *)subType domain:(NSString *)domain {
    [SecABC triggerAutoBugCaptureWithType:type
                                  subType:subType
                           subtypeContext:nil
                                   domain:domain
                                   events:nil
                                  payload:nil
                          detectedProcess:nil];
}

- (void)tapToRadar:(NSString*)alert
       description:(NSString*)description
             radar:(NSString*)radar
     componentName:(NSString*)componentName
  componentVersion:(NSString*)componentVersion
       componentID:(NSString*)componentID
{
    /**TODO: *submit a new TTR on next unlock though xpc_activities, possible with help of supd */

    SecTapToRadar *ttr = [[SecTapToRadar alloc] initTapToRadar:alert
                                                   description:description
                                                         radar:radar];
    if (componentName && componentVersion && componentID) {
        ttr.componentName = componentName;
        ttr.componentVersion = componentVersion;
        ttr.componentID = componentID;
    }

    [ttr trigger];
}

@end



@implementation SFAnalyticsCollection

- (instancetype)init {
    return [self initWithActionInterface:[[DefaultCollectionActions alloc] init]];
}

- (instancetype)initWithActionInterface:(id<SFAnalyticsCollectionAction>)actions
{
    if ((self = [super init]) == nil) {
        return nil;
    }
    self.queue = dispatch_queue_create("SFAnalyticsCollection", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
    self.actions = actions;
    return self;
}

- (void)dealloc {
    [self onQueue_stopMetricCollection];
}

- (SFAMatchingRules* _Nullable)parseCollection:(NSData *)data logger:(SFAnalytics *)logger {

    NSError *error;

    NSData *decompressed = [data decompressedDataUsingAlgorithm:NSDataCompressionAlgorithmLZFSE error:&error];
    if (decompressed == nil) {
        return nil;
    }

    SECSFARules *rules = [[SECSFARules alloc] initWithData:decompressed];
    SFAMatchingRules* parsed = [NSMutableDictionary dictionary];

    for (SECSFARule* rule in rules.rules) {
        NSMutableSet<SFAnalyticsMatchingRule *>* r = parsed[rule.eventType];
        if (r == NULL) {
            r = [NSMutableSet set];
            parsed[rule.eventType] = r;
        }
        SFAnalyticsMatchingRule *mr = [[SFAnalyticsMatchingRule alloc] initWithSFARule:rule logger:logger];
        if (mr) {
            [r addObject:mr];
        }
    }
    return parsed;
}

- (void)setupMetricsHook:(SFAnalytics *)logger {
    __block SFAnalyticsMetricsHook metricsHook = NULL;
    dispatch_async(self.queue, ^{
        // Dont setup metrics hook if it's already done
        if (self.tearDownMetricsHook != nil) {
            return;
        }
        __weak typeof(logger) weakLogger = logger;
        __weak typeof(self) weakSelf = self;

        metricsHook = ^SFAnalyticsMetricsHookActions(NSString * _Nonnull eventName, SFAnalyticsEventClass eventClass, NSDictionary * _Nonnull attributes, SFAnalyticsTimestampBucket timestampBucket) {
            __strong typeof(logger) strongLogger = weakLogger;
            if (strongLogger == nil) {
                return 0;
            }
            return [weakSelf match:eventName eventClass:eventClass attributes:attributes bucket:timestampBucket logger:strongLogger];
        };

        self.tearDownMetricsHook = ^{
            [weakLogger removeMetricsHook:metricsHook];
        };
    });
    if (metricsHook) {
        [logger addMetricsHook:metricsHook];
    }
}

- (void)onQueue_stopMetricCollection {
    __block void(^teardown)(void) = nil;
    teardown = self.tearDownMetricsHook;
    self.tearDownMetricsHook = NULL;

    if (teardown != nil) {
        dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
            teardown();
        });
    }
}

- (void)stopMetricCollection {
    dispatch_sync(self.queue, ^{
        [self onQueue_stopMetricCollection];
    });
}

- (void)loadCollection:(SFAnalytics *)logger
{
    NSData *data = [logger dataPropertyForKey:SFCollectionConfig];
    SFAMatchingRules * newRules = [self parseCollection:data logger:logger];
    dispatch_sync(self.queue, ^{
        self.matchingRules = newRules;
    });
    [self setupMetricsHook:logger];
}

- (void)storeCollection:(NSData * _Nullable)data logger:(SFAnalytics * _Nullable)logger
{
    __block BOOL rulesChanged;
    __typeof(self.matchingRules) newRules = [self parseCollection:data logger:logger];

    dispatch_sync(self.queue, ^{
        rulesChanged = (newRules != self.matchingRules);
        self.matchingRules = newRules;
    });
    if (logger && rulesChanged) {
        [logger setDataProperty:data forKey:SFCollectionConfig];
        [self setupMetricsHook:logger];
    }
}

- (SFAnalyticsMetricsHookActions)match:(NSString*)eventName
                            eventClass:(SFAnalyticsEventClass)eventClass
                            attributes:(NSDictionary*)attributes
                                bucket:(SFAnalyticsTimestampBucket)timestampBucket
                                logger:(SFAnalytics *)logger
{
    __block SFAnalyticsMetricsHookActions actions = SFAnalyticsMetricsHookNoAction;
    dispatch_sync(self.queue, ^{
        NSSet<SFAnalyticsMatchingRule*>* rules = self.matchingRules[eventName];
        if (rules == nil || rules.count == 0) {
            return;
        }
        for (SFAnalyticsMatchingRule* rule in rules) {
            if ([rule matchAttributes:attributes logger:logger]) {
                actions |= [rule doAction:self.actions logger:logger];
            }
        }
    });
    return actions;
}

@end
