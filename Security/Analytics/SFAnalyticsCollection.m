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
#import <CoreFoundation/CFPriv.h>

#import "SFAnalytics+Internal.h"
#import "SFAnalyticsCollection.h"
#import "Analytics/Protobuf/SECSFARules.h"
#import "Analytics/Protobuf/SECSFAEventRule.h"
#import "Analytics/Protobuf/SECSFAAction.h"
#import "Analytics/Protobuf/SECSFAActionAutomaticBugCapture.h"
#import "Analytics/Protobuf/SECSFAActionTapToRadar.h"
#import "Analytics/Protobuf/SECSFAActionDropEvent.h"
#import "Analytics/Protobuf/SECSFAVersionMatch.h"
#import "Analytics/Protobuf/SECSFAEventFilter.h"
#import "Analytics/Protobuf/SECSFAVersion.h"
#import "utilities/SecABC.h"
#import "utilities/SecTapToRadar.h"

#include <os/feature_private.h>

static NSString* SFCollectionConfig = @"SFCollectionConfig";

static os_log_t
getOSLog(void) {
    static os_log_t sfaLog = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sfaLog = os_log_create("SFA", "log");
    });
    return sfaLog;
}

@interface SFAnalyticsMatchingRule ()
@property (readwrite) SECSFAEventRule *rule;
@property NSDictionary<NSString*,id> *matchingDictionary;
@property NSDate *lastMatch;
@property bool firstMatchArmed;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithSFARule:(SECSFAEventRule *)rule logger:(SFAnalytics*)logger;

- (BOOL)matchAttributes:(NSDictionary *)attributes 
             eventClass:(SFAnalyticsEventClass)eventClass
            processName:(NSString *)processName
                 logger:(SFAnalytics *)logger;

- (BOOL)valueMatch:(id)vKey target:(id)vTarget;
- (BOOL)isSubsetMatch:(NSDictionary *)match target:(NSDictionary *)target;

@end

@implementation SFAnalyticsMatchingRule

- (instancetype)initWithSFARule:(SECSFAEventRule *)rule logger:(SFAnalytics*)logger {
    if ((self = [super init]) == nil) {
        return nil;
    }
    self.eventName = rule.eventType;
    self.rule = rule;
    self.lastMatch = [logger datePropertyForKey:[self lastMatchTimeKey]];
    if ([logger numberPropertyForKey:[self armKey]] != nil) {
        self.firstMatchArmed = YES;
    }

    return self;
}

- (NSString*)description {
    return [NSString stringWithFormat:@"<SFAnalyticsMatchingRule: %@ match: %@ %@>",
            self.eventName, [self cachedMatchDictionary], self.lastMatch];
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

- (NSDictionary * _Nullable)cachedMatchDictionary {
    @synchronized(self) {
        if (self.matchingDictionary == nil) {
            NSError *error = nil;
            NSDictionary *d = [NSPropertyListSerialization propertyListWithData:self.rule.match options:0 format:nil error:&error];
            if (d == nil || error != nil) {
                os_log_error(getOSLog(), "SFAnalyticsMatchingRule match dictionary wrong: %@", error);
                return nil;
            }
            if (![d isKindOfClass:[NSDictionary class]]) {
                os_log_error(getOSLog(), "SFAnalyticsMatchingRule match not dictionary");
                return nil;
            }
            self.matchingDictionary = d;
        }
    }
    return self.matchingDictionary;
}

+ (NSString *)armKeyForEventName:(NSString *)eventName {
    return [NSString stringWithFormat:@"SFAColl-%@-armed", eventName];
}

- (NSString *)armKey {
    return [[self class] armKeyForEventName:self.eventName];
}

- (BOOL)matchAttributes:(NSDictionary *)attributes
             eventClass:(SFAnalyticsEventClass)eventClass
            processName:(NSString *)processName
                 logger:(SFAnalytics *)logger
{
    if (self.rule.processName) {
        if ([processName isEqual:self.rule.processName] == NO) {
            return NO;
        }
    }

    if (self.rule.matchOnFirstFailure) {
        if (eventClass == SFAnalyticsEventClassSuccess && !self.firstMatchArmed) {
            [logger setNumberProperty:@YES forKey:[self armKey]];
            self.firstMatchArmed = YES;
        }
    }
    
    if (self.rule.hasMatch) {
        NSDictionary *match = [self cachedMatchDictionary];
        if (match == nil) {
            return NO;
        }

        /* check if `matchingDictionary' is a subset of `attributes' */
        if (![self isSubsetMatch:match target:attributes]) {
            return NO;
        }
    }

    // we assume we are the only writer of the state, so we can avoid accessing
    // disk for `armKey', so we keep it cached in memory and assume it uptodate.
    if (self.rule.matchOnFirstFailure) {
        if (eventClass == SFAnalyticsEventClassHardFailure || eventClass == SFAnalyticsEventClassSoftFailure) {
            NSString *armKey = [self armKey];

            if (self.firstMatchArmed == NO) {
                return NO;
            }
            [logger setNumberProperty:nil forKey:armKey];
            self.firstMatchArmed = NO;
        }
    }
    switch (self.rule.eventClass) {
        case SECSFAEventClass_All:
            break;
        case SECSFAEventClass_Errors:
            if (eventClass != SFAnalyticsEventClassHardFailure && eventClass != SFAnalyticsEventClassSoftFailure) {
                return NO;
            }
            break;
        case SECSFAEventClass_Success:
            if (eventClass != SFAnalyticsEventClassSuccess) {
                return NO;
            }
            break;
        case SECSFAEventClass_HardFailure:
            if (eventClass != SFAnalyticsEventClassHardFailure) {
                return NO;
            }
            break;
        case SECSFAEventClass_SoftFailure:
            if (eventClass != SFAnalyticsEventClassSoftFailure) {
                return NO;
            }
            break;
        case SECSFAEventClass_Note:
            if (eventClass != SFAnalyticsEventClassNote) {
                return NO;
            }
            break;
        case SECSFAEventClass_Rockwell:
            if (eventClass != SFAnalyticsEventClassRockwell) {
                return NO;
            }
            break;
        default:
            return NO;
    }

    return YES;
}


- (SFAnalyticsMetricsHookActions)doAction:(id<SFAnalyticsCollectionAction>)actions
                               attributes:(NSDictionary* _Nullable)attributes
                                   logger:(SFAnalytics *)logger
{
    SECSFAAction *action = self.rule.action;
    if (action == nil) {
        return 0;
    }

    if (action.hasTtr) {
        if ([actions shouldRatelimit:logger rule:self]) {
            os_log_info(getOSLog(), "SFACollection ratelimit ttr: %@", self.rule.eventType);
            return 0;
        }
        os_log(getOSLog(), "SFACollection action trigger ttr: %@: %@", self.rule.eventType, self.cachedMatchDictionary);

        SECSFAActionTapToRadar *ttr = action.ttr;
        [actions tapToRadar:ttr.alert
                description:ttr.description
                      radar:action.radarnumber
              componentName:ttr.componentName
           componentVersion:ttr.componentVersion
                componentID:ttr.componentID
                 attributes:attributes];
        return 0;
    } else if (action.hasAbc) {
        if ([actions shouldRatelimit:logger rule:self]) {
            os_log_info(getOSLog(), "SFACollection ratelimit abc: %@", self.rule.eventType);
            return 0;
        }
        os_log(getOSLog(), "SFACollection action trigger abc: %@ %@", self.rule.eventType, self.cachedMatchDictionary);

        SECSFAActionAutomaticBugCapture *abc = action.abc;
        if (abc.domain == nil && abc.type == nil){
            return 0;
        }
        [actions autoBugCaptureWithType:abc.type subType:abc.subtype domain:abc.domain];
        return 0;
    } else if (action.hasDrop) {
        os_log(getOSLog(), "SFACollection action trigger drop: %@", self.rule.eventType);
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
        os_log(getOSLog(), "SFACollection unknown action: %@", self.rule.eventType);
    }
    return 0;
}

@end

@implementation SecSFAParsedCollection
@end


@interface SFAnalyticsCollection ()
@property SFAMatchingRules *matchingRules;
@property NSMutableDictionary<NSString*,NSNumber*>* allowedEvents;
@property (readwrite) BOOL excludedVersion;

@property void(^tearDownMetricsHook)(void);
@property id<SFAnalyticsCollectionAction> actions;
@property dispatch_queue_t queue;
@property SECSFAVersion *selfVersion;
@end

@interface DefaultCollectionActions: NSObject <SFAnalyticsCollectionAction>
@end

@implementation DefaultCollectionActions

- (BOOL)shouldRatelimit:(SFAnalytics *)logger rule:(SFAnalyticsMatchingRule *)rule {
    if (rule.lastMatch) {
        int64_t repeatAfterSeconds = rule.rule.repeatAfterSeconds;
        if (repeatAfterSeconds == 0) {
            repeatAfterSeconds = 3600 * 24; //if not set, provided sesable default: 24h
        }
        NSDate *allowedMatch = [NSDate dateWithTimeIntervalSinceNow:-1 * repeatAfterSeconds];
        if ([allowedMatch compare:rule.lastMatch] != NSOrderedDescending) {
            return YES;
        }
    }

    rule.lastMatch = [NSDate date];
    [logger setDateProperty:rule.lastMatch forKey:[rule lastMatchTimeKey]];

    return NO;
}

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
        attributes:(NSDictionary * _Nullable)attributes
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
    if (attributes && [NSJSONSerialization isValidJSONObject:attributes]) {
        NSData *json = [NSJSONSerialization dataWithJSONObject:attributes
                                                       options:(NSJSONWritingSortedKeys|NSJSONWritingPrettyPrinted)
                                                         error:nil];
        if (json) {
            ttr.reason = [[NSString alloc] initWithData:json encoding:NSUTF8StringEncoding];
        }
    }
    [ttr trigger];
}

@end

@implementation SFAnalyticsCollection

- (instancetype)init {
    NSDictionary *version = CFBridgingRelease(_CFCopySystemVersionDictionary());
    NSString *build = version[(__bridge NSString *)_kCFSystemVersionBuildVersionKey];
    NSString *product = version[(__bridge NSString *)_kCFSystemVersionProductNameKey];
    if (![build isKindOfClass:[NSString class]] || ![product isKindOfClass:[NSString class]]) {
        return nil;
    }
    self.processName = [NSProcessInfo.processInfo processName];
    return [self initWithActionInterface:[[DefaultCollectionActions alloc] init]
                                 product:product
                                   build:build];
}

- (instancetype)initWithActionInterface:(id<SFAnalyticsCollectionAction>)actions
                                product:(NSString *)product
                                  build:(NSString *)build
{
    SECSFAVersion *selfVersion = [[self class] parseVersion:build platform:product];
    if (selfVersion == nil) {
        return nil;
    }
    if ((self = [super init]) == nil) {
        return nil;
    }
    self.queue = dispatch_queue_create("SFAnalyticsCollection", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
    self.actions = actions;
    self.selfVersion = selfVersion;
    return self;
}

+ (SECSFAVersion *_Nullable)parseVersion:(NSString *)build platform:(NSString *)platform {
    SECSFAVersion *version = [[SECSFAVersion alloc] init];
    if ([platform isEqual:@"macOS"] || [platform isEqual:@"Mac OS X"]) {
        version.productName = SECSFAProductName_macOS;
    } else if ([platform isEqual:@"iPhone OS"]) {
        version.productName = SECSFAProductName_iphoneOS;
    } else if ([platform isEqual:@"Apple TVOS"]) {
        version.productName = SECSFAProductName_tvOS;
    } else if ([platform isEqual:@"visionOS"] || [platform isEqual:@"xrOS"]) {
        version.productName = SECSFAProductName_visionOS;
    } else if ([platform isEqual:@"Watch OS"]) {
        version.productName = SECSFAProductName_watchOS;
    } else {
        return nil;
    }

    NSError *error = NULL;
    NSRegularExpression *regex = [NSRegularExpression regularExpressionWithPattern:@"^(\\d+)([A-Z])(\\d+)" options:0 error:&error];
    NSArray *matches = [regex matchesInString:build options:0 range:NSMakeRange(0, [build length])];
    if (!matches || matches.count != 1) {
        return nil;
    }

    NSTextCheckingResult *matchResult = [matches objectAtIndex:0];

    if (matchResult.numberOfRanges != 4) {
        return nil;
    }

    version.major = [[build substringWithRange:[matchResult rangeAtIndex:1]] intValue];
    version.minor = [[build substringWithRange:[matchResult rangeAtIndex:2]] characterAtIndex:0] - 'A' + 1;
    version.build = [[build substringWithRange:[matchResult rangeAtIndex:3]] intValue];

    return version;
}

// compare if its same platform, and version is same or newer
+ (BOOL)isVersionSameOrNewer:(SECSFAVersion *)v1 than:(SECSFAVersion *)v2 {
    if (v1.productName != v2.productName) {
        return NO;
    }
    if (v1.major > v2.major) {
        return YES;
    }
    if (v1.major < v2.major) {
        return NO;
    }
    if (v1.minor > v2.minor) {
        return YES;
    }
    if (v1.minor < v2.minor) {
        return NO;
    }
    if (v1.build < v2.build) {
        return NO;
    }
    return YES;
}

- (void)dealloc {
    [self onQueue_stopMetricCollection];
}

// is the self version newer then match, allowed if there is no matches
- (BOOL)allowedVersionsWithSelf:(SECSFAVersionMatch*)match {
    if (match.versionsCount == 0) {
        return YES;
    }
    for (SECSFAVersion *v in match.versions) {
        if ([[self class] isVersionSameOrNewer:self.selfVersion than:v]) {
            return YES;
        }
    }
    return NO;
}


- (SecSFAParsedCollection* _Nullable)parseCollection:(NSData *)data logger:(SFAnalytics *)logger {

    NSError *error;

    NSData *decompressed = [data decompressedDataUsingAlgorithm:NSDataCompressionAlgorithmLZFSE error:&error];
    if (decompressed == nil) {
        return nil;
    }

    SECSFARules *rules = [[SECSFARules alloc] initWithData:decompressed];
    
    SecSFAParsedCollection *parsed = [[SecSFAParsedCollection alloc] init];

    if (rules.allowedBuilds.versionsCount > 0) {
        parsed.excludedVersion = ![self allowedVersionsWithSelf:rules.allowedBuilds];
        if (parsed.excludedVersion) {
            return parsed;
        }
    }
    
    NSNumber *zero = @0;

    if (rules.eventFilters.count > 0) {
        parsed.allowedEvents = [NSMutableDictionary dictionary];
        for (SECSFAEventFilter* rule in rules.eventFilters) {
            NSNumber *dropRate = nil;
            if (rule.dropRate == 0) {
                dropRate = zero;
            } else if (rule.dropRate > 0 && rule.dropRate <= 100) {
                dropRate = @(rule.dropRate);
            }
            parsed.allowedEvents[rule.event] = dropRate;
        }
    }
    
    if (rules.eventRules.count > 0) {
        parsed.matchingRules = [NSMutableDictionary dictionary];
        
        for (SECSFAEventRule* rule in rules.eventRules) {

            // Check if this rule apply to this version
            if (rule.versions != nil && ![self allowedVersionsWithSelf:rule.versions]) {
                continue;
            }
            NSMutableSet<SFAnalyticsMatchingRule *>* r = parsed.matchingRules[rule.eventType];
            if (r == NULL) {
                r = [NSMutableSet set];
                parsed.matchingRules[rule.eventType] = r;
            }
            SFAnalyticsMatchingRule *mr = [[SFAnalyticsMatchingRule alloc] initWithSFARule:rule logger:logger];
            if (mr) {
                [r addObject:mr];
            }
            
            // allow filtered events
            parsed.allowedEvents[rule.eventType] = zero;
        }
    }
    
    return parsed;
}

- (void)setupMetricsHook:(SFAnalytics *)logger {
    dispatch_async(self.queue, ^{
        SFAnalyticsMetricsHook metricsHook = NULL;

        // Dont setup metrics hook if it's already done
        if (self.tearDownMetricsHook != nil) {
            return;
        }

        __weak typeof(logger) weakLogger = logger;
        __weak typeof(self) weakSelf = self;
        
        metricsHook = ^SFAnalyticsMetricsHookActions(NSString * _Nonnull eventName, SFAnalyticsEventClass eventClass, NSDictionary * _Nonnull attributes, SFAnalyticsTimestampBucket timestampBucket)
        {
            __strong typeof(logger) strongLogger = weakLogger;
            __strong typeof(self) strongSelf = weakSelf;
            if (strongLogger == nil || strongSelf == nil) {
                return 0;
            }
            if (os_feature_enabled(Security, AllowAllMetrics)) {
                return 0;
            }
            
            // if this version is excluded, stop sending events
            if (strongSelf.excludedVersion) {
                return SFAnalyticsMetricsHookExcludeEvent;
            }

            // if there is an allow list, apply it
            if (strongSelf.allowedEvents) {
                NSNumber *dropRate = strongSelf.allowedEvents[eventName];
                if (dropRate == nil) {
                    return SFAnalyticsMetricsHookExcludeEvent;
                } else if ([dropRate integerValue] > 0) {
                    if ([dropRate integerValue] > arc4random_uniform(100)) {
                        return SFAnalyticsMetricsHookExcludeEvent;
                    }
                }
            }
            
            return [strongSelf match:eventName
                          eventClass:eventClass
                          attributes:attributes
                              bucket:timestampBucket
                              logger:strongLogger];
        };

        if (self.excludedVersion) {
            [logger AddMultiSamplerForName:@"SFACollection" withTimeInterval:SFAnalyticsSamplerIntervalOncePerReport block:^NSDictionary<NSString *,NSNumber *> *{
                return @{
                    @"SFAExclude": @YES,
                };
            }];
        };

        self.tearDownMetricsHook = ^{
            [weakLogger removeMetricsHook:metricsHook];
        };
        if (metricsHook) {
            [logger addMetricsHook:metricsHook];
        }
    });
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
    if (data == nil) {
        os_log(getOSLog(), "No rules, not setting up collection");
        return;
    }
    SecSFAParsedCollection * newRules = [self parseCollection:data logger:logger];
    dispatch_sync(self.queue, ^{
        self.matchingRules = newRules.matchingRules;
        self.excludedVersion = newRules.excludedVersion;
        self.allowedEvents = newRules.allowedEvents;
    });
    
    os_log(getOSLog(), "Loading matching rules: %@", newRules);
    [self setupMetricsHook:logger];
}

- (void)storeCollection:(NSData * _Nullable)data logger:(SFAnalytics * _Nullable)logger
{
    __block BOOL rulesChanged;
    SecSFAParsedCollection* newRules = [self parseCollection:data logger:logger];

    dispatch_sync(self.queue, ^{
        rulesChanged = (newRules.matchingRules != self.matchingRules) || (newRules.allowedEvents != self.allowedEvents);
        self.matchingRules = newRules.matchingRules;
        self.excludedVersion = newRules.excludedVersion;
        self.allowedEvents = newRules.allowedEvents;
    });
    if (logger && rulesChanged) {
        os_log(getOSLog(), "Setting up new rules");
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
    os_log_debug(getOSLog(), "matching rules %@", eventName);

    dispatch_sync(self.queue, ^{
        NSSet<SFAnalyticsMatchingRule*>* rules = self.matchingRules[eventName];
        if (rules == nil || rules.count == 0) {
            os_log_debug(getOSLog(), "no rules %@", eventName);
            return;
        }
        for (SFAnalyticsMatchingRule* rule in rules) {
            if ([rule matchAttributes:attributes 
                           eventClass:eventClass
                          processName:self.processName
                               logger:logger]) {
                actions |= [rule doAction:self.actions attributes:attributes logger:logger];
            }
        }
    });
    return actions;
}

- (void)drainSetupQueue {
    dispatch_sync(self.queue, ^{});
}


@end
