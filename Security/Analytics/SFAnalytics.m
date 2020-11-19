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
#import "NSDate+SFAnalytics.h"
#import "utilities/debugging.h"
#import <utilities/SecFileLocations.h>
#import <objc/runtime.h>
#import <sys/stat.h>
#import <CoreFoundation/CFPriv.h>
#include <os/transaction_private.h>
#include <os/variant_private.h>

#import <utilities/SecCoreAnalytics.h>

#if TARGET_OS_OSX
#include <sys/sysctl.h>
#include <membership.h>
#else
#import <sys/utsname.h>
#endif

// SFAnalyticsDefines constants
NSString* const SFAnalyticsTableSuccessCount = @"success_count";
NSString* const SFAnalyticsTableHardFailures = @"hard_failures";
NSString* const SFAnalyticsTableSoftFailures = @"soft_failures";
NSString* const SFAnalyticsTableSamples = @"samples";
NSString* const SFAnalyticsTableNotes = @"notes";

NSString* const SFAnalyticsColumnSuccessCount = @"success_count";
NSString* const SFAnalyticsColumnHardFailureCount = @"hard_failure_count";
NSString* const SFAnalyticsColumnSoftFailureCount = @"soft_failure_count";
NSString* const SFAnalyticsColumnSampleValue = @"value";
NSString* const SFAnalyticsColumnSampleName = @"name";

NSString* const SFAnalyticsPostTime = @"postTime";
NSString* const SFAnalyticsEventTime = @"eventTime";
NSString* const SFAnalyticsEventType = @"eventType";
NSString* const SFAnalyticsEventTypeErrorEvent = @"errorEvent";
NSString* const SFAnalyticsEventErrorDestription = @"errorDescription";
NSString* const SFAnalyticsEventClassKey = @"eventClass";

NSString* const SFAnalyticsAttributeErrorUnderlyingChain = @"errorChain";
NSString* const SFAnalyticsAttributeErrorDomain = @"errorDomain";
NSString* const SFAnalyticsAttributeErrorCode = @"errorCode";

NSString* const SFAnalyticsAttributeLastUploadTime = @"lastUploadTime";

NSString* const SFAnalyticsUserDefaultsSuite = @"com.apple.security.analytics";

char* const SFAnalyticsFireSamplersNotification = "com.apple.security.sfanalytics.samplers";

NSString* const SFAnalyticsTopicCloudServices = @"CloudServicesTopic";
NSString* const SFAnalyticsTopicKeySync = @"KeySyncTopic";
NSString* const SFAnalyticsTopicTrust = @"TrustTopic";
NSString* const SFAnalyticsTopicTransparency = @"TransparencyTopic";
NSString* const SFAnalyticsTopicNetworking = @"NetworkingTopic";

NSString* const SFAnalyticsTableSchema =    @"CREATE TABLE IF NOT EXISTS hard_failures (\n"
                                                @"id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
                                                @"timestamp REAL,"
                                                @"data BLOB\n"
                                            @");\n"
                                            @"DROP TRIGGER IF EXISTS maintain_ring_buffer_hard_failures;\n"
                                            @"CREATE TRIGGER IF NOT EXISTS maintain_ring_buffer_hard_failures_v2 AFTER INSERT ON hard_failures\n"
                                            @"BEGIN\n"
                                                @"DELETE FROM hard_failures WHERE id <= NEW.id - 1000;\n"
                                            @"END;\n"
                                            @"CREATE TABLE IF NOT EXISTS soft_failures (\n"
                                                @"id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
                                                @"timestamp REAL,"
                                                @"data BLOB\n"
                                            @");\n"
                                            @"DROP TRIGGER IF EXISTS maintain_ring_buffer_soft_failures;\n"
                                            @"CREATE TRIGGER IF NOT EXISTS maintain_ring_buffer_soft_failures_v2 AFTER INSERT ON soft_failures\n"
                                            @"BEGIN\n"
                                                @"DELETE FROM soft_failures WHERE id <= NEW.id - 1000;\n"
                                            @"END;\n"
                                            @"CREATE TABLE IF NOT EXISTS notes (\n"
                                                @"id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
                                                @"timestamp REAL,"
                                                @"data BLOB\n"
                                            @");\n"
                                            @"DROP TRIGGER IF EXISTS maintain_ring_buffer_notes;\n"
                                            @"CREATE TRIGGER IF NOT EXISTS maintain_ring_buffer_notes_v2 AFTER INSERT ON notes\n"
                                            @"BEGIN\n"
                                                @"DELETE FROM notes WHERE id <= NEW.id - 1000;\n"
                                            @"END;\n"
                                            @"CREATE TABLE IF NOT EXISTS samples (\n"
                                                @"id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
                                                @"timestamp REAL,\n"
                                                @"name STRING,\n"
                                                @"value REAL\n"
                                            @");\n"
                                            @"DROP TRIGGER IF EXISTS maintain_ring_buffer_samples;\n"
                                            @"CREATE TRIGGER IF NOT EXISTS maintain_ring_buffer_samples_v2 AFTER INSERT ON samples\n"
                                            @"BEGIN\n"
                                                @"DELETE FROM samples WHERE id <= NEW.id - 1000;\n"
                                            @"END;\n"
                                            @"CREATE TABLE IF NOT EXISTS success_count (\n"
                                                @"event_type STRING PRIMARY KEY,\n"
                                                @"success_count INTEGER,\n"
                                                @"hard_failure_count INTEGER,\n"
                                                @"soft_failure_count INTEGER\n"
                                            @");\n"
                                            @"DROP TABLE IF EXISTS all_events;\n";

NSUInteger const SFAnalyticsMaxEventsToReport = 1000;

NSString* const SFAnalyticsErrorDomain = @"com.apple.security.sfanalytics";

// Local constants
NSString* const SFAnalyticsEventBuild = @"build";
NSString* const SFAnalyticsEventProduct = @"product";
NSString* const SFAnalyticsEventModelID = @"modelid";
NSString* const SFAnalyticsEventInternal = @"internal";
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

+ (NSString *)defaultProtectedAnalyticsDatabasePath:(NSString *)basename uuid:(NSUUID * __nullable)userUuid
{
    // Create the top-level directory with full access
    NSMutableString *directory = [NSMutableString stringWithString:@"sfanalytics"];
    WithPathInProtectedDirectory((__bridge  CFStringRef)directory, ^(const char *path) {
        mode_t permissions = 0777;
        int ret = mkpath_np(path, permissions);
        if (!(ret == 0 || ret ==  EEXIST)) {
            secerror("could not create path: %s (%s)", path, strerror(ret));
        }
        chmod(path, permissions);
    });

    // create per-user directory
    if (userUuid) {
        [directory appendString:@"/"];
        [directory appendString:[userUuid UUIDString]];
        WithPathInProtectedDirectory((__bridge  CFStringRef)directory, ^(const char *path) {
#if TARGET_OS_IPHONE
            mode_t permissions = 0775;
#else
            mode_t permissions = 0700;
            if (geteuid() == 0) {
                // Root user directory needs to be read/write for group so that user supd can upload root data
                permissions = 0775;
            }
#endif // TARGET_OS_IPHONE
            int ret = mkpath_np(path, permissions);
            if (!(ret == 0 || ret ==  EEXIST)) {
                secerror("could not create path: %s (%s)", path, strerror(ret));
            }
            chmod(path, permissions);
        });
    }
    NSString *path = [NSString stringWithFormat:@"%@/%@.db", directory, basename];
    return [(__bridge_transfer NSURL*)SecCopyURLForFileInProtectedDirectory((__bridge CFStringRef)path) path];
}

+ (NSString *)defaultProtectedAnalyticsDatabasePath:(NSString *)basename
{
#if TARGET_OS_OSX
    uid_t euid = geteuid();
    uuid_t currentUserUuid;
    int ret = mbr_uid_to_uuid(euid, currentUserUuid);
    if (ret != 0) {
        secerror("failed to get UUID for user(%d) - %d", euid, ret);
        return [SFAnalytics defaultProtectedAnalyticsDatabasePath:basename uuid:nil];
    }
    NSUUID *userUuid = [[NSUUID alloc] initWithUUIDBytes:currentUserUuid];
    return [SFAnalytics defaultProtectedAnalyticsDatabasePath:basename uuid:userUuid];
#else
    return [SFAnalytics defaultProtectedAnalyticsDatabasePath:basename uuid:nil];
#endif // TARGET_OS_IPHONE
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


- (void)incrementIntegerPropertyForKey:(NSString*)key
{
    __weak __typeof(self) weakSelf = self;
    dispatch_sync(_queue, ^{
        __strong __typeof(self) strongSelf = weakSelf;
        if (strongSelf == nil) {
            return;
        }
        NSInteger integer = [[strongSelf.database propertyForKey:key] integerValue];
        [strongSelf.database setProperty:[NSString stringWithFormat:@"%ld", (long)integer + 1] forKey:key];
    });
}

- (void)setNumberProperty:(NSNumber* _Nullable)number forKey:(NSString*)key
{
    __weak __typeof(self) weakSelf = self;
    dispatch_sync(_queue, ^{
        __strong __typeof(self) strongSelf = weakSelf;
        if (strongSelf) {
            [strongSelf.database setProperty:[number stringValue] forKey:key];
        }
    });
}

- (NSNumber* _Nullable)numberPropertyForKey:(NSString*)key
{
    __block NSNumber* result = nil;
    __weak __typeof(self) weakSelf = self;
    dispatch_sync(_queue, ^{
        __strong __typeof(self) strongSelf = weakSelf;
        if (strongSelf) {
            NSString *property = [strongSelf.database propertyForKey:key];
            if (property) {
                result = [NSNumber numberWithInteger:[property integerValue]];
            }
        }
    });
    return result;
}

+ (NSString*)hwModelID
{
    static NSString *hwModel = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
#if TARGET_OS_SIMULATOR
        // Asking for a real value in the simulator gives the results for the underlying mac. Not particularly useful.
        hwModel = [NSString stringWithFormat:@"%s", getenv("SIMULATOR_MODEL_IDENTIFIER")];
#elif TARGET_OS_OSX
        size_t size;
        sysctlbyname("hw.model", NULL, &size, NULL, 0);
        char *sysctlString = malloc(size);
        sysctlbyname("hw.model", sysctlString, &size, NULL, 0);
        hwModel = [[NSString alloc] initWithUTF8String:sysctlString];
        free(sysctlString);
#else
        struct utsname systemInfo;
        uname(&systemInfo);

        hwModel = [NSString stringWithCString:systemInfo.machine
                                     encoding:NSUTF8StringEncoding];
#endif
    });
    return hwModel;
}

+ (void)addOSVersionToEvent:(NSMutableDictionary*)eventDict {
    static dispatch_once_t onceToken;
    static NSString *build = NULL;
    static NSString *product = NULL;
    static NSString *modelID = nil;
    static BOOL internal = NO;
    dispatch_once(&onceToken, ^{
        NSDictionary *version = CFBridgingRelease(_CFCopySystemVersionDictionary());
        if (version == NULL)
            return;
        build = version[(__bridge NSString *)_kCFSystemVersionBuildVersionKey];
        product = version[(__bridge NSString *)_kCFSystemVersionProductNameKey];
        internal = os_variant_has_internal_diagnostics("com.apple.security");

        modelID = [self hwModelID];
    });
    if (build) {
        eventDict[SFAnalyticsEventBuild] = build;
    }
    if (product) {
        eventDict[SFAnalyticsEventProduct] = product;
    }
    if (modelID) {
        eventDict[SFAnalyticsEventModelID] = modelID;
    }
    if (internal) {
        eventDict[SFAnalyticsEventInternal] = @YES;
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

- (NSDictionary *)coreAnalyticsKeyFilter:(NSDictionary<NSString *, id> *)info
{
    NSMutableDictionary *filtered = [NSMutableDictionary dictionary];
    [info enumerateKeysAndObjectsUsingBlock:^(NSString *_Nonnull key, id  _Nonnull obj, BOOL * _Nonnull stop) {
        filtered[[key stringByReplacingOccurrencesOfString:@"-" withString:@"_"]] = obj;
    }];
    return filtered;
}

// Daily CoreAnalytics metrics
// Call this once per say if you want to have the once per day sampler collect their data and submit it

- (void)dailyCoreAnalyticsMetrics:(NSString *)eventName
{
    NSMutableDictionary<NSString*, NSNumber*> *dailyMetrics = [NSMutableDictionary dictionary];
    __block NSDictionary<NSString*, SFAnalyticsMultiSampler*>* multisamplers;
    __block NSDictionary<NSString*, SFAnalyticsSampler*>* samplers;

    dispatch_sync(_queue, ^{
        multisamplers = [self->_multisamplers copy];
        samplers = [self->_samplers copy];
    });

    [multisamplers enumerateKeysAndObjectsUsingBlock:^(NSString * _Nonnull key, SFAnalyticsMultiSampler * _Nonnull obj, BOOL * _Nonnull stop) {
        if (obj.oncePerReport == FALSE) {
            return;
        }
        NSDictionary<NSString*, NSNumber*> *samples = [obj sampleNow];
        if (samples == nil) {
            return;
        }
        [dailyMetrics addEntriesFromDictionary:samples];
    }];

    [samplers enumerateKeysAndObjectsUsingBlock:^(NSString * _Nonnull key, SFAnalyticsSampler * _Nonnull obj, BOOL * _Nonnull stop) {
        if (obj.oncePerReport == FALSE) {
            return;
        }
        dailyMetrics[key] = [obj sampleNow];
    }];

    [SecCoreAnalytics sendEvent:eventName event:[self coreAnalyticsKeyFilter:dailyMetrics]];
}

// MARK: Event logging

- (void)logSuccessForEventNamed:(NSString*)eventName timestampBucket:(SFAnalyticsTimestampBucket)timestampBucket
{
    [self logEventNamed:eventName class:SFAnalyticsEventClassSuccess attributes:nil timestampBucket:timestampBucket];
}

- (void)logSuccessForEventNamed:(NSString*)eventName
{
    [self logSuccessForEventNamed:eventName timestampBucket:SFAnalyticsTimestampBucketSecond];
}

- (void)logHardFailureForEventNamed:(NSString*)eventName withAttributes:(NSDictionary*)attributes timestampBucket:(SFAnalyticsTimestampBucket)timestampBucket
{
    [self logEventNamed:eventName class:SFAnalyticsEventClassHardFailure attributes:attributes timestampBucket:timestampBucket];
}

- (void)logHardFailureForEventNamed:(NSString*)eventName withAttributes:(NSDictionary*)attributes
{
    [self logHardFailureForEventNamed:eventName withAttributes:attributes timestampBucket:SFAnalyticsTimestampBucketSecond];
}

- (void)logSoftFailureForEventNamed:(NSString*)eventName withAttributes:(NSDictionary*)attributes timestampBucket:(SFAnalyticsTimestampBucket)timestampBucket
{
    [self logEventNamed:eventName class:SFAnalyticsEventClassSoftFailure attributes:attributes timestampBucket:timestampBucket];
}

- (void)logSoftFailureForEventNamed:(NSString*)eventName withAttributes:(NSDictionary*)attributes
{
    [self logSoftFailureForEventNamed:eventName withAttributes:attributes timestampBucket:SFAnalyticsTimestampBucketSecond];
}

- (void)logResultForEvent:(NSString*)eventName hardFailure:(bool)hardFailure result:(NSError*)eventResultError timestampBucket:(SFAnalyticsTimestampBucket)timestampBucket
{
    [self logResultForEvent:eventName hardFailure:hardFailure result:eventResultError withAttributes:nil timestampBucket:SFAnalyticsTimestampBucketSecond];
}

- (void)logResultForEvent:(NSString*)eventName hardFailure:(bool)hardFailure result:(NSError*)eventResultError
{
    [self logResultForEvent:eventName hardFailure:hardFailure result:eventResultError timestampBucket:SFAnalyticsTimestampBucketSecond];
}

- (void)logResultForEvent:(NSString*)eventName hardFailure:(bool)hardFailure result:(NSError*)eventResultError withAttributes:(NSDictionary*)attributes timestampBucket:(SFAnalyticsTimestampBucket)timestampBucket
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

- (void)logResultForEvent:(NSString*)eventName hardFailure:(bool)hardFailure result:(NSError*)eventResultError withAttributes:(NSDictionary*)attributes
{
    [self logResultForEvent:eventName hardFailure:hardFailure result:eventResultError withAttributes:attributes timestampBucket:SFAnalyticsTimestampBucketSecond];
}

- (void)noteEventNamed:(NSString*)eventName timestampBucket:(SFAnalyticsTimestampBucket)timestampBucket
{
    [self logEventNamed:eventName class:SFAnalyticsEventClassNote attributes:nil timestampBucket:timestampBucket];
}

- (void)noteEventNamed:(NSString*)eventName
{
    [self noteEventNamed:eventName timestampBucket:SFAnalyticsTimestampBucketSecond];
}

- (void)logEventNamed:(NSString*)eventName class:(SFAnalyticsEventClass)class attributes:(NSDictionary*)attributes timestampBucket:(SFAnalyticsTimestampBucket)timestampBucket
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

        [strongSelf.database begin];

        NSDictionary* eventDict = [self eventDictForEventName:eventName withAttributes:attributes eventClass:class timestampBucket:timestampBucket];

        if (class == SFAnalyticsEventClassHardFailure) {
            [strongSelf.database addEventDict:eventDict toTable:SFAnalyticsTableHardFailures timestampBucket:timestampBucket];
            [strongSelf.database incrementHardFailureCountForEventType:eventName];
        }
        else if (class == SFAnalyticsEventClassSoftFailure) {
            [strongSelf.database addEventDict:eventDict toTable:SFAnalyticsTableSoftFailures timestampBucket:timestampBucket];
            [strongSelf.database incrementSoftFailureCountForEventType:eventName];
        }
        else if (class == SFAnalyticsEventClassNote) {
            [strongSelf.database addEventDict:eventDict toTable:SFAnalyticsTableNotes timestampBucket:timestampBucket];
            [strongSelf.database incrementSuccessCountForEventType:eventName];
        }
        else if (class == SFAnalyticsEventClassSuccess) {
            [strongSelf.database incrementSuccessCountForEventType:eventName];
        }

        [strongSelf.database end];
    });
}

- (void)logEventNamed:(NSString*)eventName class:(SFAnalyticsEventClass)class attributes:(NSDictionary*)attributes
{
    [self logEventNamed:eventName class:class attributes:attributes timestampBucket:SFAnalyticsTimestampBucketSecond];
}

- (NSDictionary*) eventDictForEventName:(NSString*)eventName withAttributes:(NSDictionary*)attributes eventClass:(SFAnalyticsEventClass)eventClass timestampBucket:(NSTimeInterval)timestampBucket
{
    NSMutableDictionary* eventDict = attributes ? attributes.mutableCopy : [NSMutableDictionary dictionary];
    eventDict[SFAnalyticsEventType] = eventName;

    NSTimeInterval timestamp = [[NSDate date] timeIntervalSince1970WithBucket:timestampBucket];

    // our backend wants timestamps in milliseconds
    eventDict[SFAnalyticsEventTime] = @(timestamp * 1000);
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
        os_transaction_t transaction = os_transaction_create("com.apple.security.sfanalytics.samplerGC");
        __strong __typeof(self) strongSelf = weakSelf;
        if (strongSelf) {
            [strongSelf->_samplers[samplerName] pauseSampling];    // when dealloced it would also stop, but we're not sure when that is so let's stop it right away
            [strongSelf->_samplers removeObjectForKey:samplerName];
        }
        (void)transaction;
        transaction = nil;
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
        os_transaction_t transaction = os_transaction_create("com.apple.security.sfanalytics.samplerGC");
        __strong __typeof(self) strongSelf = weakSelf;
        if (strongSelf) {
            [strongSelf->_multisamplers[samplerName] pauseSampling];    // when dealloced it would also stop, but we're not sure when that is so let's stop it right away
            [strongSelf->_multisamplers removeObjectForKey:samplerName];
        }
        (void)transaction;
        transaction = nil;
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

- (SFAnalyticsActivityTracker*)startLogSystemMetricsForActivityNamed:(NSString *)eventName
{
    if (![eventName isKindOfClass:[NSString class]]) {
        secerror("Cannot log system metrics without name");
        return nil;
    }
    SFAnalyticsActivityTracker* tracker = [[SFAnalyticsActivityTracker alloc] initWithName:eventName clientClass:[self class]];
    [tracker start];
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
        os_transaction_t transaction = os_transaction_create("com.apple.security.sfanalytics.samplerGC");
        __strong __typeof(self) strongSelf = weakSelf;
        if (strongSelf && !strongSelf->_disableLogging) {
            if (once) {
                [strongSelf.database removeAllSamplesForName:metricName];
            }
            [strongSelf.database addSample:metric forName:metricName];
        }
        (void)transaction;
        transaction = nil;
    });
}

@end

#endif // __OBJC2__
