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

#if __OBJC2__

#import "SFAnalytics+Internal.h"
#import "SFAnalyticsDefines.h"
#import "SFAnalyticsActivityTracker+Internal.h"
#import "SFAnalyticsSampler+Internal.h"
#import "SFAnalyticsMultiSampler+Internal.h"
#import "SFAnalyticsSQLiteStore.h"
#import "utilities/debugging.h"
#import <utilities/SecFileLocations.h>
#import <objc/runtime.h>
#import <sys/stat.h>
#import <CoreFoundation/CFPriv.h>

// SFAnalyticsDefines constants
NSString* const SFAnalyticsTableSuccessCount = @"success_count";
NSString* const SFAnalyticsTableHardFailures = @"hard_failures";
NSString* const SFAnalyticsTableSoftFailures = @"soft_failures";
NSString* const SFAnalyticsTableSamples = @"samples";
NSString* const SFAnalyticsTableAllEvents = @"all_events";

NSString* const SFAnalyticsColumnSuccessCount = @"success_count";
NSString* const SFAnalyticsColumnHardFailureCount = @"hard_failure_count";
NSString* const SFAnalyticsColumnSoftFailureCount = @"soft_failure_count";
NSString* const SFAnalyticsColumnSampleValue = @"value";
NSString* const SFAnalyticsColumnSampleName = @"name";

NSString* const SFAnalyticsEventTime = @"eventTime";
NSString* const SFAnalyticsEventType = @"eventType";
NSString* const SFAnalyticsEventClassKey = @"eventClass";

NSString* const SFAnalyticsAttributeErrorUnderlyingChain = @"errorChain";
NSString* const SFAnalyticsAttributeErrorDomain = @"errorDomain";
NSString* const SFAnalyticsAttributeErrorCode = @"errorCode";

NSString* const SFAnalyticsUserDefaultsSuite = @"com.apple.security.analytics";

char* const SFAnalyticsFireSamplersNotification = "com.apple.security.sfanalytics.samplers";

NSString* const SFAnalyticsTopicKeySync = @"KeySyncTopic";
NSString* const SFAnaltyicsTopicTrust = @"TrustTopic";

NSString* const SFAnalyticsTableSchema =    @"CREATE TABLE IF NOT EXISTS hard_failures (\n"
                                                @"id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
                                                @"timestamp REAL,"
                                                @"data BLOB\n"
                                            @");\n"
                                            @"CREATE TRIGGER IF NOT EXISTS maintain_ring_buffer_hard_failures AFTER INSERT ON hard_failures\n"
                                            @"BEGIN\n"
                                                @"DELETE FROM hard_failures WHERE id != NEW.id AND id % 1000 = NEW.id % 1000;\n"
                                            @"END;\n"
                                            @"CREATE TABLE IF NOT EXISTS soft_failures (\n"
                                                @"id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
                                                @"timestamp REAL,"
                                                @"data BLOB\n"
                                            @");\n"
                                            @"CREATE TRIGGER IF NOT EXISTS maintain_ring_buffer_soft_failures AFTER INSERT ON soft_failures\n"
                                            @"BEGIN\n"
                                                @"DELETE FROM soft_failures WHERE id != NEW.id AND id % 1000 = NEW.id % 1000;\n"
                                            @"END;\n"
                                            @"CREATE TABLE IF NOT EXISTS all_events (\n"
                                                @"id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
                                                @"timestamp REAL,"
                                                @"data BLOB\n"
                                            @");\n"
                                            @"CREATE TRIGGER IF NOT EXISTS maintain_ring_buffer_all_events AFTER INSERT ON all_events\n"
                                            @"BEGIN\n"
                                                @"DELETE FROM all_events WHERE id != NEW.id AND id % 10000 = NEW.id % 10000;\n"
                                            @"END;\n"
                                            @"CREATE TABLE IF NOT EXISTS samples (\n"
                                                @"id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
                                                @"timestamp REAL,\n"
                                                @"name STRING,\n"
                                                @"value REAL\n"
                                            @");\n"
                                            @"CREATE TRIGGER IF NOT EXISTS maintain_ring_buffer_samples AFTER INSERT ON samples\n"
                                            @"BEGIN\n"
                                            @"DELETE FROM samples WHERE id != NEW.id AND id % 1000 = NEW.id % 1000;\n"
                                            @"END;\n"
                                            @"CREATE TABLE IF NOT EXISTS success_count (\n"
                                                @"event_type STRING PRIMARY KEY,\n"
                                                @"success_count INTEGER,\n"
                                                @"hard_failure_count INTEGER,\n"
                                                @"soft_failure_count INTEGER\n"
                                            @");\n";

NSUInteger const SFAnalyticsMaxEventsToReport = 1000;

NSString* const SFAnalyticsErrorDomain = @"com.apple.security.sfanalytics";

// Local constants
NSString* const SFAnalyticsEventBuild = @"build";
NSString* const SFAnalyticsEventProduct = @"product";
const NSTimeInterval SFAnalyticsSamplerIntervalOncePerReport = -1.0;

@interface SFAnalytics ()
@property (nonatomic) SFAnalyticsSQLiteStore* database;
@property (nonatomic) dispatch_queue_t queue;
@end

@implementation SFAnalytics {
    SFAnalyticsSQLiteStore* _database;
    dispatch_queue_t _queue;
    NSMutableDictionary<NSString*, SFAnalyticsSampler*>* _samplers;
    NSMutableDictionary<NSString*, SFAnalyticsMultiSampler*>* _multisamplers;
    unsigned int _disableLogging:1;
}

+ (instancetype)logger
{
#if TARGET_OS_SIMULATOR
    return nil;
#else
    
    if (self == [SFAnalytics class]) {
        secerror("attempt to instatiate abstract class SFAnalytics");
        return nil;
    }

    SFAnalytics* logger = nil;
    @synchronized(self) {
        logger = objc_getAssociatedObject(self, "SFAnalyticsInstance");
        if (!logger) {
            logger = [[self alloc] init];
            objc_setAssociatedObject(self, "SFAnalyticsInstance", logger, OBJC_ASSOCIATION_RETAIN);
        }
    }

    [logger database];  // For unit testing so there's always a database. DB shouldn't be nilled in production though
    return logger;
#endif
}

+ (NSString*)databasePath
{
    return nil;
}

+ (NSString *)defaultAnalyticsDatabasePath:(NSString *)basename
{
    WithPathInKeychainDirectory(CFSTR("Analytics"), ^(const char *path) {
#if TARGET_OS_IPHONE
        mode_t permissions = 0775;
#else
        mode_t permissions = 0700;
#endif // TARGET_OS_IPHONE
        int ret = mkpath_np(path, permissions);
        if (!(ret == 0 || ret ==  EEXIST)) {
            secerror("could not create path: %s (%s)", path, strerror(ret));
        }
        chmod(path, permissions);
    });
    NSString *path = [NSString stringWithFormat:@"Analytics/%@.db", basename];
    return [(__bridge_transfer NSURL*)SecCopyURLForFileInKeychainDirectory((__bridge CFStringRef)path) path];
}

+ (NSInteger)fuzzyDaysSinceDate:(NSDate*)date
{
    // Sentinel: it didn't happen at all
    if (!date) {
        return -1;
    }
    
    // Sentinel: it happened but we don't know when because the date doesn't make sense
    // Magic number represents January 1, 2017.
    if ([date compare:[NSDate dateWithTimeIntervalSince1970:1483228800]] == NSOrderedAscending) {
        return 1000;
    }

    NSInteger secondsPerDay = 60 * 60 * 24;
    
    NSTimeInterval timeIntervalSinceDate = [[NSDate date] timeIntervalSinceDate:date];
    if (timeIntervalSinceDate < secondsPerDay) {
        return 0;
    }
    else if (timeIntervalSinceDate < (secondsPerDay * 7)) {
        return 1;
    }
    else if (timeIntervalSinceDate < (secondsPerDay * 30)) {
        return 7;
    }
    else if (timeIntervalSinceDate < (secondsPerDay * 365)) {
        return 30;
    }
    else {
        return 365;
    }
}

// Instantiate lazily so unit tests can have clean databases each
- (SFAnalyticsSQLiteStore*)database
{
    if (!_database) {
        _database = [SFAnalyticsSQLiteStore storeWithPath:self.class.databasePath schema:SFAnalyticsTableSchema];
        if (!_database) {
            seccritical("Did not get a database! (Client %@)", NSStringFromClass([self class]));
        }
    }
    return _database;
}

- (void)removeState
{
    [_samplers removeAllObjects];
    [_multisamplers removeAllObjects];

    __weak __typeof(self) weakSelf = self;
    dispatch_sync(_queue, ^{
        __strong __typeof(self) strongSelf = weakSelf;
        if (strongSelf) {
            [strongSelf.database close];
            strongSelf->_database = nil;
        }
    });
}

- (void)setDateProperty:(NSDate*)date forKey:(NSString*)key
{
    __weak __typeof(self) weakSelf = self;
    dispatch_sync(_queue, ^{
        __strong __typeof(self) strongSelf = weakSelf;
        if (strongSelf) {
            [strongSelf.database setDateProperty:date forKey:key];
        }
    });
}

- (NSDate*)datePropertyForKey:(NSString*)key
{
    __block NSDate* result = nil;
    __weak __typeof(self) weakSelf = self;
    dispatch_sync(_queue, ^{
        __strong __typeof(self) strongSelf = weakSelf;
        if (strongSelf) {
            result = [strongSelf.database datePropertyForKey:key];
        }
    });
    return result;
}

+ (void)addOSVersionToEvent:(NSMutableDictionary*)eventDict {
    static dispatch_once_t onceToken;
    static NSString *build = NULL;
    static NSString *product = NULL;
    dispatch_once(&onceToken, ^{
        NSDictionary *version = CFBridgingRelease(_CFCopySystemVersionDictionary());
        if (version == NULL)
            return;
        build = version[(__bridge NSString *)_kCFSystemVersionBuildVersionKey];
        product = version[(__bridge NSString *)_kCFSystemVersionProductNameKey];
    });
    if (build) {
        eventDict[SFAnalyticsEventBuild] = build;
    }
    if (product) {
        eventDict[SFAnalyticsEventProduct] = product;
    }
}

- (instancetype)init
{
    if (self = [super init]) {
        _queue = dispatch_queue_create("SFAnalytics data access queue", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
        _samplers = [NSMutableDictionary<NSString*, SFAnalyticsSampler*> new];
        _multisamplers = [NSMutableDictionary<NSString*, SFAnalyticsMultiSampler*> new];
        [self database];    // for side effect of instantiating DB object. Used for testing.
    }

    return self;
}

// MARK: Event logging

- (void)logSuccessForEventNamed:(NSString*)eventName
{
    [self logEventNamed:eventName class:SFAnalyticsEventClassSuccess attributes:nil];
}

- (void)logHardFailureForEventNamed:(NSString*)eventName withAttributes:(NSDictionary*)attributes
{
    [self logEventNamed:eventName class:SFAnalyticsEventClassHardFailure attributes:attributes];
}

- (void)logSoftFailureForEventNamed:(NSString*)eventName withAttributes:(NSDictionary*)attributes
{
    [self logEventNamed:eventName class:SFAnalyticsEventClassSoftFailure attributes:attributes];
}

- (void)logResultForEvent:(NSString*)eventName hardFailure:(bool)hardFailure result:(NSError*)eventResultError
{
    [self logResultForEvent:eventName hardFailure:hardFailure result:eventResultError withAttributes:nil];
}

- (void)logResultForEvent:(NSString*)eventName hardFailure:(bool)hardFailure result:(NSError*)eventResultError withAttributes:(NSDictionary*)attributes
{
    if(!eventResultError) {
        [self logSuccessForEventNamed:eventName];
    } else {
        // Make an Attributes dictionary
        NSMutableDictionary* eventAttributes = nil;
        if (attributes) {
            eventAttributes = [attributes mutableCopy];
        } else {
            eventAttributes = [NSMutableDictionary dictionary];
        }

        /* if we have underlying errors, capture the chain below the top-most error */
        NSError *underlyingError = eventResultError.userInfo[NSUnderlyingErrorKey];
        if ([underlyingError isKindOfClass:[NSError class]]) {
            NSMutableString *chain = [NSMutableString string];
            int count = 0;
            do {
                [chain appendFormat:@"%@-%ld:", underlyingError.domain, (long)underlyingError.code];
                underlyingError = underlyingError.userInfo[NSUnderlyingErrorKey];
            } while (count++ < 5 && [underlyingError isKindOfClass:[NSError class]]);

            eventAttributes[SFAnalyticsAttributeErrorUnderlyingChain] = chain;
        }

        eventAttributes[SFAnalyticsAttributeErrorDomain] = eventResultError.domain;
        eventAttributes[SFAnalyticsAttributeErrorCode] = @(eventResultError.code);

        if(hardFailure) {
            [self logHardFailureForEventNamed:eventName withAttributes:eventAttributes];
        } else {
            [self logSoftFailureForEventNamed:eventName withAttributes:eventAttributes];
        }
    }
}

- (void)noteEventNamed:(NSString*)eventName
{
    [self logEventNamed:eventName class:SFAnalyticsEventClassNote attributes:nil];
}

- (void)logEventNamed:(NSString*)eventName class:(SFAnalyticsEventClass)class attributes:(NSDictionary*)attributes
{
    if (!eventName) {
        secerror("SFAnalytics: attempt to log an event with no name");
        return;
    }

    __weak __typeof(self) weakSelf = self;
    dispatch_sync(_queue, ^{
        __strong __typeof(self) strongSelf = weakSelf;
        if (!strongSelf || strongSelf->_disableLogging) {
            return;
        }

        NSDictionary* eventDict = [self eventDictForEventName:eventName withAttributes:attributes eventClass:class];
        [strongSelf.database addEventDict:eventDict toTable:SFAnalyticsTableAllEvents];
        
        if (class == SFAnalyticsEventClassHardFailure) {
            [strongSelf.database addEventDict:eventDict toTable:SFAnalyticsTableHardFailures];
            [strongSelf.database incrementHardFailureCountForEventType:eventName];
        }
        else if (class == SFAnalyticsEventClassSoftFailure) {
            [strongSelf.database addEventDict:eventDict toTable:SFAnalyticsTableSoftFailures];
            [strongSelf.database incrementSoftFailureCountForEventType:eventName];
        }
        else if (class == SFAnalyticsEventClassSuccess || class == SFAnalyticsEventClassNote) {
            [strongSelf.database incrementSuccessCountForEventType:eventName];
        }
    });
}

- (NSDictionary*)eventDictForEventName:(NSString*)eventName withAttributes:(NSDictionary*)attributes eventClass:(SFAnalyticsEventClass)eventClass
{
    NSMutableDictionary* eventDict = attributes ? attributes.mutableCopy : [NSMutableDictionary dictionary];
    eventDict[SFAnalyticsEventType] = eventName;
    // our backend wants timestamps in milliseconds
    eventDict[SFAnalyticsEventTime] = @([[NSDate date] timeIntervalSince1970] * 1000);
    eventDict[SFAnalyticsEventClassKey] = @(eventClass);
    [SFAnalytics addOSVersionToEvent:eventDict];

    return eventDict;
}

// MARK: Sampling

- (SFAnalyticsSampler*)addMetricSamplerForName:(NSString *)samplerName withTimeInterval:(NSTimeInterval)timeInterval block:(NSNumber *(^)(void))block
{
    if (!samplerName) {
        secerror("SFAnalytics: cannot add sampler without name");
        return nil;
    }
    if (timeInterval < 1.0f && timeInterval != SFAnalyticsSamplerIntervalOncePerReport) {
        secerror("SFAnalytics: cannot add sampler with interval %f", timeInterval);
        return nil;
    }
    if (!block) {
        secerror("SFAnalytics: cannot add sampler without block");
        return nil;
    }

    __block SFAnalyticsSampler* sampler = nil;

    __weak __typeof(self) weakSelf = self;
    dispatch_sync(_queue, ^{
        __strong __typeof(self) strongSelf = weakSelf;
        if (strongSelf->_samplers[samplerName]) {
            secerror("SFAnalytics: sampler \"%@\" already exists", samplerName);
        } else {
            sampler = [[SFAnalyticsSampler alloc] initWithName:samplerName interval:timeInterval block:block clientClass:[self class]];
            strongSelf->_samplers[samplerName] = sampler;   // If sampler did not init because of bad data this 'removes' it from the dict, so a noop
        }
    });

    return sampler;
}

- (SFAnalyticsMultiSampler*)AddMultiSamplerForName:(NSString *)samplerName withTimeInterval:(NSTimeInterval)timeInterval block:(NSDictionary<NSString *,NSNumber *> *(^)(void))block
{
    if (!samplerName) {
        secerror("SFAnalytics: cannot add sampler without name");
        return nil;
    }
    if (timeInterval < 1.0f && timeInterval != SFAnalyticsSamplerIntervalOncePerReport) {
        secerror("SFAnalytics: cannot add sampler with interval %f", timeInterval);
        return nil;
    }
    if (!block) {
        secerror("SFAnalytics: cannot add sampler without block");
        return nil;
    }

    __block SFAnalyticsMultiSampler* sampler = nil;
    __weak __typeof(self) weakSelf = self;
    dispatch_sync(_queue, ^{
        __strong __typeof(self) strongSelf = weakSelf;
        if (strongSelf->_multisamplers[samplerName]) {
            secerror("SFAnalytics: multisampler \"%@\" already exists", samplerName);
        } else {
            sampler = [[SFAnalyticsMultiSampler alloc] initWithName:samplerName interval:timeInterval block:block clientClass:[self class]];
            strongSelf->_multisamplers[samplerName] = sampler;
        }

    });

    return sampler;
}

- (SFAnalyticsSampler*)existingMetricSamplerForName:(NSString *)samplerName
{
    __block SFAnalyticsSampler* sampler = nil;

    __weak __typeof(self) weakSelf = self;
    dispatch_sync(_queue, ^{
        __strong __typeof(self) strongSelf = weakSelf;
        if (strongSelf) {
            sampler = strongSelf->_samplers[samplerName];
        }
    });
    return sampler;
}

- (SFAnalyticsMultiSampler*)existingMultiSamplerForName:(NSString *)samplerName
{
    __block SFAnalyticsMultiSampler* sampler = nil;

    __weak __typeof(self) weakSelf = self;
    dispatch_sync(_queue, ^{
        __strong __typeof(self) strongSelf = weakSelf;
        if (strongSelf) {
            sampler = strongSelf->_multisamplers[samplerName];
        }
    });
    return sampler;
}

- (void)removeMetricSamplerForName:(NSString *)samplerName
{
    if (!samplerName) {
        secerror("Attempt to remove sampler without specifying samplerName");
        return;
    }

    __weak __typeof(self) weakSelf = self;
    dispatch_async(_queue, ^{
        __strong __typeof(self) strongSelf = weakSelf;
        if (strongSelf) {
            [strongSelf->_samplers[samplerName] pauseSampling];    // when dealloced it would also stop, but we're not sure when that is so let's stop it right away
            [strongSelf->_samplers removeObjectForKey:samplerName];
        }
    });
}

- (void)removeMultiSamplerForName:(NSString *)samplerName
{
    if (!samplerName) {
        secerror("Attempt to remove multisampler without specifying samplerName");
        return;
    }

    __weak __typeof(self) weakSelf = self;
    dispatch_async(_queue, ^{
        __strong __typeof(self) strongSelf = weakSelf;
        if (strongSelf) {
            [strongSelf->_multisamplers[samplerName] pauseSampling];    // when dealloced it would also stop, but we're not sure when that is so let's stop it right away
            [strongSelf->_multisamplers removeObjectForKey:samplerName];
        }
    });
}

- (SFAnalyticsActivityTracker*)logSystemMetricsForActivityNamed:(NSString *)eventName withAction:(void (^)(void))action
{
    if (![eventName isKindOfClass:[NSString class]]) {
        secerror("Cannot log system metrics without name");
        return nil;
    }
    SFAnalyticsActivityTracker* tracker = [[SFAnalyticsActivityTracker alloc] initWithName:eventName clientClass:[self class]];
    if (action) {
        [tracker performAction:action];
    }
    return tracker;
}

- (void)logMetric:(NSNumber *)metric withName:(NSString *)metricName
{
    [self logMetric:metric withName:metricName oncePerReport:NO];
}

- (void)logMetric:(NSNumber*)metric withName:(NSString*)metricName oncePerReport:(BOOL)once
{
    if (![metric isKindOfClass:[NSNumber class]] || ![metricName isKindOfClass:[NSString class]]) {
        secerror("SFAnalytics: Need a valid result and name to log result");
        return;
    }

    __weak __typeof(self) weakSelf = self;
    dispatch_async(_queue, ^{
        __strong __typeof(self) strongSelf = weakSelf;
        if (strongSelf && !strongSelf->_disableLogging) {
            if (once) {
                [strongSelf.database removeAllSamplesForName:metricName];
            }
            [strongSelf.database addSample:metric forName:metricName];
        }
    });
}

@end

#endif // __OBJC2__
