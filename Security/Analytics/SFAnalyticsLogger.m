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

#import "SFAnalyticsLogger.h"
#import "SFSQLite.h"
#import "CKKSViewManager.h"
#import "debugging.h"
#import <objc/runtime.h>

NSString* const SFAnalyticsLoggerTableSuccessCount = @"success_count";
NSString* const SFAnalyticsLoggerColumnEventType = @"event_type";
NSString* const SFAnalyticsLoggerColumnSuccessCount = @"success_count";
NSString* const SFAnalyticsLoggerColumnHardFailureCount = @"hard_failure_count";
NSString* const SFAnalyticsLoggerColumnSoftFailureCount = @"soft_failure_count";

NSString* const SFAnalyticsLoggerTableHardFailures = @"hard_failures";
NSString* const SFAnalyticsLoggerTableSoftFailures = @"soft_failures";
NSString* const SFAnalyticsLoggerTableAllEvents = @"all_events";
NSString* const SFAnalyticsLoggerColumnDate = @"timestamp";
NSString* const SFAnalyticsLoggerColumnData = @"data";

NSString* const SFAnalyticsLoggerUploadDate = @"upload_date";

NSString* const SFAnalyticsLoggerSplunkTopic = @"topic";
NSString* const SFAnalyticsLoggerSplunkEventTime = @"eventTime";
NSString* const SFAnalyticsLoggerSplunkPostTime = @"postTime";
NSString* const SFAnalyticsLoggerSplunkEventType = @"eventType";
NSString* const SFAnalyticsLoggerMetricsBase = @"metricsBase";
NSString* const SFAnalyticsLoggerEventClassKey = @"eventClass";

NSString* const SFAnalyticsUserDefaultsSuite = @"com.apple.security.analytics";

static NSString* const SFAnalyticsLoggerTableSchema = @"CREATE TABLE IF NOT EXISTS hard_failures (\n"
                                                        @"id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
                                                        @"timestamp REAL,"
                                                        @"data BLOB\n"
                                                      @");\n"
                                                      @"CREATE TRIGGER IF NOT EXISTS maintain_ring_buffer_hard_failures AFTER INSERT ON hard_failures\n"
                                                        @"BEGIN\n"
                                                        @"DELETE FROM hard_failures WHERE id != NEW.id AND id % 999 = NEW.id % 999;\n"
                                                      @"END;\n"
                                                      @"CREATE TABLE IF NOT EXISTS soft_failures (\n"
                                                        @"id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
                                                        @"timestamp REAL,"
                                                        @"data BLOB\n"
                                                      @");\n"
                                                      @"CREATE TRIGGER IF NOT EXISTS maintain_ring_buffer_soft_failures AFTER INSERT ON soft_failures\n"
                                                        @"BEGIN\n"
                                                        @"DELETE FROM soft_failures WHERE id != NEW.id AND id % 999 = NEW.id % 999;\n"
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
                                                      @"CREATE TABLE IF NOT EXISTS success_count (\n"
                                                        @"event_type STRING PRIMARY KEY,\n"
                                                        @"success_count INTEGER,\n"
                                                        @"hard_failure_count INTEGER,\n"
                                                        @"soft_failure_count INTEGER\n"
                                                      @");\n";

#define SFANALYTICS_SPLUNK_DEV 0
#define SFANALYTICS_MAX_EVENTS_TO_REPORT 999

#if SFANALYTICS_SPLUNK_DEV
#define SECONDS_BETWEEN_UPLOADS 10
#else
// three days = 60 seconds times 60 minutes * 72 hours
#define SECONDS_BETWEEN_UPLOADS (60 * 60 * 72)
#endif

#define SECONDS_PER_DAY (60 * 60 * 24)

typedef NS_ENUM(NSInteger, SFAnalyticsEventClass) {
    SFAnalyticsEventClassSuccess,
    SFAnalyticsEventClassHardFailure,
    SFAnalyticsEventClassSoftFailure,
    SFAnalyticsEventClassNote
};

@interface SFAnalyticsLoggerSQLiteStore : SFSQLite

@property (readonly, strong) NSArray* failureRecords;
@property (readonly, strong) NSArray* allEvents;
@property (readwrite, strong) NSDate* uploadDate;

+ (instancetype)storeWithPath:(NSString*)path schema:(NSString*)schema;

- (void)incrementSuccessCountForEventType:(NSString*)eventType;
- (void)incrementHardFailureCountForEventType:(NSString*)eventType;
- (void)incrementSoftFailureCountForEventType:(NSString*)eventType;
- (NSInteger)successCountForEventType:(NSString*)eventType;
- (NSInteger)hardFailureCountForEventType:(NSString*)eventType;
- (NSInteger)softFailureCountForEventType:(NSString*)eventType;
- (void)addEventDict:(NSDictionary*)eventDict toTable:(NSString*)table;
- (void)clearAllData;

- (NSDictionary*)summaryCounts;

@end

@implementation SFAnalyticsLogger {
    SFAnalyticsLoggerSQLiteStore* _database;
    NSURL* _splunkUploadURL;
    NSString* _splunkTopicName;
    NSURL* _splunkBagURL;
    dispatch_queue_t _queue;
    NSInteger _secondsBetweenUploads;
    NSDictionary* _metricsBase; // data the server provides and wants us to send back
    NSArray* _blacklistedFields;
    NSArray* _blacklistedEvents;
    
    unsigned int _allowInsecureSplunkCert:1;
    unsigned int _disableLogging:1;
    unsigned int _disableUploads:1;
    unsigned int _ignoreServersMessagesTellingUsToGoAway:1;
}

@synthesize splunkUploadURL = _splunkUploadURL;
@synthesize splunkBagURL = _splunkBagURL;
@synthesize splunkTopicName = _splunkTopicName;
@synthesize splunkLoggingQueue = _queue;

+ (instancetype)logger
{
#if TARGET_OS_SIMULATOR
    return nil;
#else
    
    if (self == [SFAnalyticsLogger class]) {
        secerror("attempt to instatiate abstract class SFAnalyticsLogger");
        return nil;
    }

    SFAnalyticsLogger* logger = nil;
    @synchronized(self) {
        logger = objc_getAssociatedObject(self, "SFAnalyticsLoggerInstance");
        if (!logger) {
            logger = [[self alloc] init];
            objc_setAssociatedObject(self, "SFAnalyticsLoggerInstance", logger, OBJC_ASSOCIATION_RETAIN);
        }
    }
    return logger;
#endif
}

+ (NSString*)databasePath
{
    return nil;
}

+ (NSInteger)fuzzyDaysSinceDate:(NSDate*)date
{
    NSTimeInterval timeIntervalSinceDate = [[NSDate date] timeIntervalSinceDate:date];
    if (timeIntervalSinceDate < SECONDS_PER_DAY) {
        return 0;
    }
    else if (timeIntervalSinceDate < (SECONDS_PER_DAY * 7)) {
        return 1;
    }
    else if (timeIntervalSinceDate < (SECONDS_PER_DAY * 30)) {
        return 7;
    }
    else if (timeIntervalSinceDate < (SECONDS_PER_DAY * 365)) {
        return 30;
    }
    else {
        return 365;
    }
}

- (instancetype)init
{
    if (self = [super init]) {
        _database = [SFAnalyticsLoggerSQLiteStore storeWithPath:self.class.databasePath schema:SFAnalyticsLoggerTableSchema];
        _queue = dispatch_queue_create("com.apple.security.analytics", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
        _secondsBetweenUploads = SECONDS_BETWEEN_UPLOADS;

        NSDictionary* systemDefaultValues = [NSDictionary dictionaryWithContentsOfFile:[[NSBundle bundleWithPath:@"/System/Library/Frameworks/Security.framework"] pathForResource:@"SFAnalyticsLogging" ofType:@"plist"]];
        _splunkTopicName = systemDefaultValues[@"splunk_topic"];
        _splunkUploadURL = [NSURL URLWithString:systemDefaultValues[@"splunk_uploadURL"]];
        _splunkBagURL = [NSURL URLWithString:systemDefaultValues[@"splunk_bagURL"]];
        _allowInsecureSplunkCert = [[systemDefaultValues valueForKey:@"splunk_allowInsecureCertificate"] boolValue];
        NSString* splunkEndpoint = systemDefaultValues[@"splunk_endpointDomain"];

        NSUserDefaults* defaults = [[NSUserDefaults alloc] initWithSuiteName:SFAnalyticsUserDefaultsSuite];
        NSString* userDefaultsSplunkTopic = [defaults stringForKey:@"splunk_topic"];
        if (userDefaultsSplunkTopic) {
            _splunkTopicName = userDefaultsSplunkTopic;
        }

        NSURL* userDefaultsSplunkUploadURL = [NSURL URLWithString:[defaults stringForKey:@"splunk_uploadURL"]];
        if (userDefaultsSplunkUploadURL) {
            _splunkUploadURL = userDefaultsSplunkUploadURL;
        }

        NSURL* userDefaultsSplunkBagURL = [NSURL URLWithString:[defaults stringForKey:@"splunk_bagURL"]];
        if (userDefaultsSplunkUploadURL) {
            _splunkBagURL = userDefaultsSplunkBagURL;
        }

        BOOL userDefaultsAllowInsecureSplunkCert = [defaults boolForKey:@"splunk_allowInsecureCertificate"];
        _allowInsecureSplunkCert |= userDefaultsAllowInsecureSplunkCert;

        NSString* userDefaultsSplunkEndpoint = [defaults stringForKey:@"splunk_endpointDomain"];
        if (userDefaultsSplunkEndpoint) {
            splunkEndpoint = userDefaultsSplunkEndpoint;
        }

#if SFANALYTICS_SPLUNK_DEV
        _ignoreServersMessagesTellingUsToGoAway = YES;

        if (!_splunkUploadURL && splunkEndpoint) {
            NSString* urlString = [NSString stringWithFormat:@"https://%@/report/2/%@", splunkEndpoint, _splunkTopicName];
            _splunkUploadURL = [NSURL URLWithString:urlString];
        }
#else
        (void)splunkEndpoint;
#endif
    }

    return self;
}

- (void)logSuccessForEventNamed:(NSString*)eventName
{
    [self logEventNamed:eventName class:SFAnalyticsEventClassSuccess attributes:nil];
}

- (void)logHardFailureForEventNamed:(NSString*)eventName withAttributes:(NSDictionary*)attributes
{
    [self logEventNamed:eventName class:SFAnalyticsEventClassSoftFailure attributes:attributes];
}

- (void)logSoftFailureForEventNamed:(NSString*)eventName withAttributes:(NSDictionary*)attributes
{
    [self logEventNamed:eventName class:SFAnalyticsEventClassHardFailure attributes:attributes];
}

- (void)noteEventNamed:(NSString*)eventName
{
    [self logEventNamed:eventName class:SFAnalyticsEventClassNote attributes:nil];
}

- (void)logEventNamed:(NSString*)eventName class:(SFAnalyticsEventClass)class attributes:(NSDictionary*)attributes
{
    if (!eventName) {
        secinfo("SFAnalytics", "attempt to log an event with no name");
        return;
    }

    __block NSDate* uploadDate = nil;
    __weak __typeof(self) weakSelf = self;
    dispatch_sync(_queue, ^{
        __strong __typeof(self) strongSelf = weakSelf;
        if (!strongSelf || strongSelf->_disableLogging || [strongSelf->_blacklistedEvents containsObject:eventName]) {
            return;
        }
        
        NSDictionary* eventDict = [self eventDictForEventName:eventName withAttributes:attributes eventClass:class];
        [strongSelf->_database addEventDict:eventDict toTable:SFAnalyticsLoggerTableAllEvents];
        
        if (class == SFAnalyticsEventClassHardFailure) {
            NSDictionary* strippedDict = [self eventDictWithBlacklistedFieldsStrippedFrom:eventDict];
            [strongSelf->_database addEventDict:strippedDict toTable:SFAnalyticsLoggerTableHardFailures];
            [strongSelf->_database incrementHardFailureCountForEventType:eventName];
        }
        else if (class == SFAnalyticsEventClassSoftFailure) {
            NSDictionary* strippedDict = [self eventDictWithBlacklistedFieldsStrippedFrom:eventDict];
            [strongSelf->_database addEventDict:strippedDict toTable:SFAnalyticsLoggerTableSoftFailures];
            [strongSelf->_database incrementSoftFailureCountForEventType:eventName];
        }
        else if (class == SFAnalyticsEventClassSuccess || class == SFAnalyticsEventClassNote) {
            [strongSelf->_database incrementSuccessCountForEventType:eventName];
        }
        
        uploadDate = strongSelf->_database.uploadDate;
    });

    NSDate* nowDate = [NSDate date];
    if (uploadDate) {
        if ([nowDate compare:uploadDate] == NSOrderedDescending) {
            NSError* error = nil;
            BOOL uploadSuccess = [self forceUploadWithError:&error];
            if (uploadSuccess) {
                secinfo("SFAnalytics", "uploaded sync health data");
                [self resetUploadDate:YES];
            }

            if (error) {
                secerror("SFAnalytics: failed to upload json to analytics server with error: %@", error);
            }
        }
    }
    else {
        [self resetUploadDate:NO];
    }
}

- (void)resetUploadDate:(BOOL)clearData
{
    __weak __typeof(self) weakSelf = self;
    dispatch_sync(_queue, ^{
        __strong __typeof(self) strongSelf = weakSelf;
        if (!strongSelf) {
            return;
        }

        if (clearData) {
            [strongSelf->_database clearAllData];
        }
        strongSelf->_database.uploadDate = [NSDate dateWithTimeIntervalSinceNow:strongSelf->_secondsBetweenUploads];
    });
}

- (NSDictionary*)eventDictForEventName:(NSString*)eventName withAttributes:(NSDictionary*)attributes eventClass:(SFAnalyticsEventClass)eventClass
{
    NSMutableDictionary* eventDict = attributes ? attributes.mutableCopy : [NSMutableDictionary dictionary];
    eventDict[SFAnalyticsLoggerSplunkTopic] = _splunkTopicName;
    eventDict[SFAnalyticsLoggerSplunkEventType] = eventName;
    eventDict[SFAnalyticsLoggerSplunkEventTime] = @([[NSDate date] timeIntervalSince1970] * 1000);
    eventDict[SFAnalyticsLoggerEventClassKey] = @(eventClass);
    
    [_metricsBase enumerateKeysAndObjectsUsingBlock:^(NSString* key, id object, BOOL* stop) {
        if (!eventDict[key]) {
            eventDict[key] = object;
        }
    }];
    
    return eventDict;
}

- (NSDictionary*)eventDictWithBlacklistedFieldsStrippedFrom:(NSDictionary*)eventDict
{
    NSMutableDictionary* strippedDict = eventDict.mutableCopy;
    for (NSString* blacklistedField in _blacklistedFields) {
        [strippedDict removeObjectForKey:blacklistedField];
    }
    return strippedDict;
}

- (void)setDateProperty:(NSDate*)date forKey:(NSString*)key
{
    dispatch_sync(_queue, ^{
        [self->_database setDateProperty:date forKey:key];
    });
}

- (NSDate*)datePropertyForKey:(NSString*)key
{
    __block NSDate* result = nil;
    dispatch_sync(_queue, ^{
        result = [self->_database datePropertyForKey:key];
    });
    return result;
}

- (NSDictionary*)extraValuesToUploadToServer
{
    return [NSDictionary dictionary];
}

// this method is kind of evil for the fact that it has side-effects in pulling other things besides the metricsURL from the server, and as such should NOT be memoized.
// TODO redo this, probably to return a dictionary.
- (NSURL*)splunkUploadURL
{
    dispatch_assert_queue(_queue);

    if (_splunkUploadURL) {
        return _splunkUploadURL;
    }

    __weak __typeof(self) weakSelf = self;
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);

    __block NSError* error = nil;
    NSURLSessionConfiguration *defaultConfiguration = [NSURLSessionConfiguration ephemeralSessionConfiguration];
    NSURLSession* storeBagSession = [NSURLSession sessionWithConfiguration:defaultConfiguration
                                                                  delegate:self
                                                             delegateQueue:nil];

    NSURL* requestEndpoint = _splunkBagURL;
    __block NSURL* result = nil;
    NSURLSessionDataTask* storeBagTask = [storeBagSession dataTaskWithURL:requestEndpoint completionHandler:^(NSData * _Nullable data,
                                                                                                              NSURLResponse * _Nullable __unused response,
                                                                                                              NSError * _Nullable responseError) {

        __strong __typeof(self) strongSelf = weakSelf;
        if (!strongSelf) {
            return;
        }

        if (data && !responseError) {
            NSData *responseData = data; // shut up compiler
            NSDictionary* responseDict = [NSJSONSerialization JSONObjectWithData:responseData options:0 error:&error];
            if([responseDict isKindOfClass:NSDictionary.class] && !error) {
                if (!self->_ignoreServersMessagesTellingUsToGoAway) {
                    strongSelf->_disableLogging = [[responseDict valueForKey:@"disabled"] boolValue];
                    if (strongSelf->_disableLogging || [[responseDict valueForKey:@"sendDisabled"] boolValue]) {
                        // then don't upload anything right now
                        secerror("not returning a splunk URL because uploads are disabled");
                        dispatch_semaphore_signal(sem);
                        return;
                    }

                    NSUInteger millisecondsBetweenUploads = [[responseDict valueForKey:@"postFrequency"] unsignedIntegerValue] / 1000;
                    if (millisecondsBetweenUploads > 0) {
                        strongSelf->_secondsBetweenUploads = millisecondsBetweenUploads;
                    }

                    strongSelf->_blacklistedEvents = responseDict[@"blacklistedEvents"];
                    strongSelf->_blacklistedFields = responseDict[@"blacklistedFields"];
                }

                strongSelf->_metricsBase = responseDict[@"metricsBase"];

                NSString* metricsEndpoint = responseDict[@"metricsUrl"];
                if([metricsEndpoint isKindOfClass:NSString.class]) {
                    /* Lives our URL */
                    NSString* endpoint = [metricsEndpoint stringByAppendingFormat:@"/2/%@", strongSelf->_splunkTopicName];
                    secnotice("ckks", "got metrics endpoint: %@", endpoint);
                    NSURL* endpointURL = [NSURL URLWithString:endpoint];
                    if([endpointURL.scheme isEqualToString:@"https"]) {
                        result = endpointURL;
                    }
                }
            }
        }
        else {
            error = responseError;
        }
        if(error) {
            secnotice("ckks", "Unable to fetch splunk endpoint at URL: %@ -- error: %@", requestEndpoint, error.description);
        }
        else if(!result) {
            secnotice("ckks", "Malformed iTunes config payload!");
        }

        dispatch_semaphore_signal(sem);
    }];

    [storeBagTask resume];
    dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);

    return result;
}

- (BOOL)forceUploadWithError:(NSError**)error
{
    __block BOOL result = NO;
    NSData* json = [self getLoggingJSONWithError:error];
    dispatch_sync(_queue, ^{
        if (json && [self _onQueuePostJSON:json error:error]) {
            secinfo("ckks", "uploading sync health data: %@", json);

            [self->_database clearAllData];
            self->_database.uploadDate = [NSDate dateWithTimeIntervalSinceNow:self->_secondsBetweenUploads];
            result = YES;
        }
        else {
            result = NO;
        }
    });

    return result;
}

- (BOOL)_onQueuePostJSON:(NSData*)json error:(NSError**)error
{
    dispatch_assert_queue(_queue);

    /*
     * Create the NSURLSession
     *  We use the ephemeral session config because we don't need cookies or cache
     */
    NSURLSessionConfiguration *defaultConfiguration = [NSURLSessionConfiguration ephemeralSessionConfiguration];
    NSURLSession* postSession = [NSURLSession sessionWithConfiguration:defaultConfiguration
                                                              delegate:self
                                                         delegateQueue:nil];

    /*
     * Create the request
     */
    NSURL* postEndpoint = self.splunkUploadURL;
    if (!postEndpoint) {
        secerror("failed to get a splunk upload endpoint - not uploading");
        return NO;
    }

    NSMutableURLRequest* postRequest = [[NSMutableURLRequest alloc] init];
    postRequest.URL = postEndpoint;
    postRequest.HTTPMethod = @"POST";
    postRequest.HTTPBody = json;

    /*
     * Create the upload task.
     */
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    __block BOOL uploadSuccess = NO;
    NSURLSessionDataTask* uploadTask = [postSession dataTaskWithRequest:postRequest
                                                      completionHandler:^(NSData * _Nullable __unused data, NSURLResponse * _Nullable response, NSError * _Nullable requestError) {
                                                          if(requestError) {
                                                              secerror("Error in uploading the events to splunk: %@", requestError);
                                                          }
                                                          else if (![response isKindOfClass:NSHTTPURLResponse.class]){
                                                              Class class = response.class;
                                                              secerror("Received the wrong kind of response: %@", NSStringFromClass(class));
                                                          }
                                                          else {
                                                              NSHTTPURLResponse* httpResponse = (NSHTTPURLResponse*)response;
                                                              if(httpResponse.statusCode >= 200 && httpResponse.statusCode < 300) {
                                                                  /* Success */
                                                                  uploadSuccess = YES;
                                                                  secnotice("ckks", "Splunk upload success");
                                                              }
                                                              else {
                                                                  secnotice("ckks", "Splunk upload unexpected status to URL: %@ -- status: %d", postEndpoint, (int)(httpResponse.statusCode));
                                                              }
                                                          }
                                                          dispatch_semaphore_signal(sem);
                                                      }];

    secnotice("ckks", "Splunk upload start");
    [uploadTask resume];
    dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
    return uploadSuccess;
}

- (NSString*)stringForEventClass:(SFAnalyticsEventClass)eventClass
{
    if (eventClass == SFAnalyticsEventClassNote) {
        return @"EventNote";
    }
    else if (eventClass == SFAnalyticsEventClassSuccess) {
        return @"EventSuccess";
    }
    else if (eventClass == SFAnalyticsEventClassHardFailure) {
        return @"EventHardFailure";
    }
    else if (eventClass == SFAnalyticsEventClassSoftFailure) {
        return @"EventSoftFailure";
    }
    else {
        return @"EventUnknown";
    }
}

- (NSString*)sysdiagnoseStringForEventRecord:(NSDictionary*)eventRecord
{
    NSMutableDictionary* mutableEventRecord = eventRecord.mutableCopy;
    [mutableEventRecord removeObjectForKey:SFAnalyticsLoggerSplunkTopic];

    NSDate* eventDate = [NSDate dateWithTimeIntervalSince1970:[[eventRecord valueForKey:SFAnalyticsLoggerSplunkEventTime] doubleValue] / 1000];
    [mutableEventRecord removeObjectForKey:SFAnalyticsLoggerSplunkEventTime];

    NSString* eventName = eventRecord[SFAnalyticsLoggerSplunkEventType];
    [mutableEventRecord removeObjectForKey:SFAnalyticsLoggerSplunkEventType];

    SFAnalyticsEventClass eventClass = [[eventRecord valueForKey:SFAnalyticsLoggerEventClassKey] integerValue];
    NSString* eventClassString = [self stringForEventClass:eventClass];
    [mutableEventRecord removeObjectForKey:SFAnalyticsLoggerEventClassKey];

    NSMutableString* additionalAttributesString = [NSMutableString string];
    if (mutableEventRecord.count > 0) {
        [additionalAttributesString appendString:@" - Attributes: {" ];
        __block BOOL firstAttribute = YES;
        [mutableEventRecord enumerateKeysAndObjectsUsingBlock:^(NSString* key, id object, BOOL* stop) {
            NSString* openingString = firstAttribute ? @"" : @", ";
            [additionalAttributesString appendString:[NSString stringWithFormat:@"%@%@ : %@", openingString, key, object]];
            firstAttribute = NO;
        }];
        [additionalAttributesString appendString:@" }"];
    }

    return [NSString stringWithFormat:@"%@ %@: %@%@", eventDate, eventClassString, eventName, additionalAttributesString];
}

- (NSString*)getSysdiagnoseDumpWithError:(NSError**)error
{
    NSMutableString* sysdiagnose = [[NSMutableString alloc] init];

    NSDictionary* extraValues = self.extraValuesToUploadToServer;
    [extraValues enumerateKeysAndObjectsUsingBlock:^(NSString* key, id object, BOOL* stop) {
        [sysdiagnose appendFormat:@"Key: %@, Value: %@\n", key, object];
    }];

    [sysdiagnose appendString:@"\n"];

    dispatch_sync(_queue, ^{
        NSArray* allEvents = self->_database.allEvents;
        for (NSDictionary* eventRecord in allEvents) {
            [sysdiagnose appendFormat:@"%@\n", [self sysdiagnoseStringForEventRecord:eventRecord]];
        }
    });

    return sysdiagnose;
}

- (NSData*)getLoggingJSONWithError:(NSError**)error
{
    __block NSData* json = nil;
    NSDictionary* extraValues = self.extraValuesToUploadToServer;
    dispatch_sync(_queue, ^{
        NSArray* failureRecords = self->_database.failureRecords;

        NSDictionary* successCounts = self->_database.summaryCounts;
        NSInteger totalSuccessCount = 0;
        NSInteger totalHardFailureCount = 0;
        NSInteger totalSoftFailureCount = 0;
        for (NSDictionary* perEventTypeSuccessCounts in successCounts.objectEnumerator) {
            totalSuccessCount += [perEventTypeSuccessCounts[SFAnalyticsLoggerColumnSuccessCount] integerValue];
            totalHardFailureCount += [perEventTypeSuccessCounts[SFAnalyticsLoggerColumnHardFailureCount] integerValue];
            totalSoftFailureCount += [perEventTypeSuccessCounts[SFAnalyticsLoggerColumnSoftFailureCount] integerValue];
        }

        NSDate* now = [NSDate date];

        NSMutableDictionary* healthSummaryEvent = extraValues ? extraValues.mutableCopy : [[NSMutableDictionary alloc] init];
        healthSummaryEvent[SFAnalyticsLoggerSplunkTopic] = self->_splunkTopicName ?: [NSNull null];
        healthSummaryEvent[SFAnalyticsLoggerSplunkEventTime] = @([now timeIntervalSince1970] * 1000);
        healthSummaryEvent[SFAnalyticsLoggerSplunkEventType] = @"ckksHealthSummary";
        healthSummaryEvent[SFAnalyticsLoggerColumnSuccessCount] = @(totalSuccessCount);
        healthSummaryEvent[SFAnalyticsLoggerColumnHardFailureCount] = @(totalHardFailureCount);
        healthSummaryEvent[SFAnalyticsLoggerColumnSoftFailureCount] = @(totalSoftFailureCount);

        NSMutableArray* splunkRecords = failureRecords.mutableCopy;
        [splunkRecords addObject:healthSummaryEvent];

        NSDictionary* jsonDict = @{SFAnalyticsLoggerSplunkPostTime : @([now timeIntervalSince1970] * 1000), @"events" : splunkRecords};

        json = [NSJSONSerialization dataWithJSONObject:jsonDict options:NSJSONWritingPrettyPrinted error:error];
    });

    return json;
}

- (void)URLSession:(NSURLSession *)session didReceiveChallenge:(NSURLAuthenticationChallenge *)challenge
 completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition, NSURLCredential *))completionHandler {
    assert(completionHandler);
    (void)session;
    secnotice("ckks", "Splunk upload challenge");
    NSURLCredential *cred = nil;
    SecTrustResultType result = kSecTrustResultInvalid;

    if ([challenge previousFailureCount] > 0) {
        // Previous failures occurred, bail
        completionHandler(NSURLSessionAuthChallengeCancelAuthenticationChallenge, nil);

    } else if ([challenge.protectionSpace.authenticationMethod isEqualToString:NSURLAuthenticationMethodServerTrust]) {
        /*
         * Evaluate trust for the certificate
         */

        SecTrustRef serverTrust = challenge.protectionSpace.serverTrust;
        SecTrustEvaluate(serverTrust, &result);
        if (_allowInsecureSplunkCert || (result == kSecTrustResultProceed) || (result == kSecTrustResultUnspecified)) {
            /*
             * All is well, accept the credentials
             */
            if(_allowInsecureSplunkCert) {
                secnotice("ckks", "Force Accepting Splunk Credential");
            }
            cred = [NSURLCredential credentialForTrust:serverTrust];
            completionHandler(NSURLSessionAuthChallengeUseCredential, cred);

        } else {
            /*
             * An error occurred in evaluating trust, bail
             */
            completionHandler(NSURLSessionAuthChallengeCancelAuthenticationChallenge, nil);
        }
    } else {
        /*
         * Just perform the default handling
         */
        completionHandler(NSURLSessionAuthChallengePerformDefaultHandling, nil);
    }

}

- (BOOL)ignoreServerDisablingMessages
{
    return _ignoreServersMessagesTellingUsToGoAway;
}

- (void)setIgnoreServerDisablingMessages:(BOOL)ignoreServer
{
    _ignoreServersMessagesTellingUsToGoAway = ignoreServer ? YES : NO;
}

- (BOOL)allowsInsecureSplunkCert
{
    return _allowInsecureSplunkCert;
}

- (void)setAllowsInsecureSplunkCert:(BOOL)allowsInsecureSplunkCert
{
    _allowInsecureSplunkCert = allowsInsecureSplunkCert ? YES : NO;
}

@end

@implementation SFAnalyticsLoggerSQLiteStore

+ (instancetype)storeWithPath:(NSString*)path schema:(NSString*)schema
{
    SFAnalyticsLoggerSQLiteStore* store = nil;
    @synchronized([SFAnalyticsLoggerSQLiteStore class]) {
        static NSMutableDictionary* loggingStores = nil;
        static dispatch_once_t onceToken;
        dispatch_once(&onceToken, ^{
            loggingStores = [[NSMutableDictionary alloc] init];
        });

        NSString* standardizedPath = path.stringByStandardizingPath;
        store = loggingStores[standardizedPath];
        if (!store) {
            store = [[self alloc] initWithPath:standardizedPath schema:schema];
            loggingStores[standardizedPath] = store;
        }

        [store open];
    }

    return store;
}

- (void)dealloc
{
    [self close];
}

- (NSInteger)successCountForEventType:(NSString*)eventType
{
    return [[[[self select:@[SFAnalyticsLoggerColumnSuccessCount] from:SFAnalyticsLoggerTableSuccessCount where:@"event_type = ?" bindings:@[eventType]] firstObject] valueForKey:SFAnalyticsLoggerColumnSuccessCount] integerValue];
}

- (void)incrementSuccessCountForEventType:(NSString*)eventType
{
    NSInteger successCount = [self successCountForEventType:eventType];
    NSInteger hardFailureCount = [self hardFailureCountForEventType:eventType];
    NSInteger softFailureCount = [self softFailureCountForEventType:eventType];
    [self insertOrReplaceInto:SFAnalyticsLoggerTableSuccessCount values:@{SFAnalyticsLoggerColumnEventType : eventType, SFAnalyticsLoggerColumnSuccessCount : @(successCount + 1), SFAnalyticsLoggerColumnHardFailureCount : @(hardFailureCount), SFAnalyticsLoggerColumnSoftFailureCount : @(softFailureCount)}];
}

- (NSInteger)hardFailureCountForEventType:(NSString*)eventType
{
    return [[[[self select:@[SFAnalyticsLoggerColumnHardFailureCount] from:SFAnalyticsLoggerTableSuccessCount where:@"event_type = ?" bindings:@[eventType]] firstObject] valueForKey:SFAnalyticsLoggerColumnHardFailureCount] integerValue];
}

- (NSInteger)softFailureCountForEventType:(NSString*)eventType
{
    return [[[[self select:@[SFAnalyticsLoggerColumnSoftFailureCount] from:SFAnalyticsLoggerTableSuccessCount where:@"event_type = ?" bindings:@[eventType]] firstObject] valueForKey:SFAnalyticsLoggerColumnSoftFailureCount] integerValue];
}

- (void)incrementHardFailureCountForEventType:(NSString*)eventType
{
    NSInteger successCount = [self successCountForEventType:eventType];
    NSInteger hardFailureCount = [self hardFailureCountForEventType:eventType];
    NSInteger softFailureCount = [self softFailureCountForEventType:eventType];
    [self insertOrReplaceInto:SFAnalyticsLoggerTableSuccessCount values:@{SFAnalyticsLoggerColumnEventType : eventType, SFAnalyticsLoggerColumnSuccessCount : @(successCount), SFAnalyticsLoggerColumnHardFailureCount : @(hardFailureCount + 1), SFAnalyticsLoggerColumnSoftFailureCount : @(softFailureCount)}];
}

- (void)incrementSoftFailureCountForEventType:(NSString*)eventType
{
    NSInteger successCount = [self successCountForEventType:eventType];
    NSInteger hardFailureCount = [self hardFailureCountForEventType:eventType];
    NSInteger softFailureCount = [self softFailureCountForEventType:eventType];
    [self insertOrReplaceInto:SFAnalyticsLoggerTableSuccessCount values:@{SFAnalyticsLoggerColumnEventType : eventType, SFAnalyticsLoggerColumnSuccessCount : @(successCount), SFAnalyticsLoggerColumnHardFailureCount : @(hardFailureCount), SFAnalyticsLoggerColumnSoftFailureCount : @(softFailureCount + 1)}];
}

- (NSDictionary*)summaryCounts
{
    NSMutableDictionary* successCountsDict = [NSMutableDictionary dictionary];
    NSArray* rows = [self selectAllFrom:SFAnalyticsLoggerTableSuccessCount where:nil bindings:nil];
    for (NSDictionary* rowDict in rows) {
        NSString* eventName = rowDict[SFAnalyticsLoggerColumnEventType];
        if (!eventName) {
            secinfo("SFAnalytics", "ignoring entry in success counts table without an event name");
            continue;
        }

        successCountsDict[eventName] = @{SFAnalyticsLoggerTableSuccessCount : rowDict[SFAnalyticsLoggerColumnSuccessCount], SFAnalyticsLoggerColumnHardFailureCount : rowDict[SFAnalyticsLoggerColumnHardFailureCount], SFAnalyticsLoggerColumnSoftFailureCount : rowDict[SFAnalyticsLoggerColumnSoftFailureCount]};
    }

    return successCountsDict;
}

- (NSArray*)failureRecords
{
    NSArray* recordBlobs = [self select:@[SFAnalyticsLoggerColumnData] from:SFAnalyticsLoggerTableHardFailures];
    if (recordBlobs.count < SFANALYTICS_MAX_EVENTS_TO_REPORT) {
        NSArray* softFailureBlobs = [self select:@[SFAnalyticsLoggerColumnData] from:SFAnalyticsLoggerTableSoftFailures];
        if (softFailureBlobs.count > 0) {
            NSInteger numSoftFailuresToReport = SFANALYTICS_MAX_EVENTS_TO_REPORT - recordBlobs.count;
            recordBlobs = [recordBlobs arrayByAddingObjectsFromArray:[softFailureBlobs subarrayWithRange:NSMakeRange(softFailureBlobs.count - numSoftFailuresToReport, numSoftFailuresToReport)]];
        }
    }

    NSMutableArray* failureRecords = [[NSMutableArray alloc] init];
    for (NSDictionary* row in recordBlobs) {
        NSDictionary* deserializedRecord = [NSPropertyListSerialization propertyListWithData:row[SFAnalyticsLoggerColumnData] options:0 format:nil error:nil];
        [failureRecords addObject:deserializedRecord];
    }

    return failureRecords;
}

- (NSArray*)allEvents
{
    NSArray* recordBlobs = [self select:@[SFAnalyticsLoggerColumnData] from:SFAnalyticsLoggerTableAllEvents];
    NSMutableArray* records = [[NSMutableArray alloc] init];
    for (NSDictionary* row in recordBlobs) {
        NSDictionary* deserializedRecord = [NSPropertyListSerialization propertyListWithData:row[SFAnalyticsLoggerColumnData] options:0 format:nil error:nil];
        [records addObject:deserializedRecord];
    }
    return records;
}

- (void)addEventDict:(NSDictionary*)eventDict toTable:(NSString*)table
{
    NSError* error = nil;
    NSData* serializedRecord = [NSPropertyListSerialization dataWithPropertyList:eventDict format:NSPropertyListBinaryFormat_v1_0 options:0 error:&error];
    if(!error && serializedRecord) {
        [self insertOrReplaceInto:table values:@{SFAnalyticsLoggerColumnDate : [NSDate date], SFAnalyticsLoggerColumnData : serializedRecord}];
    }
    if(error && !serializedRecord) {
        secerror("Couldn't serialize failure record: %@", error);
    }
}

- (NSDate*)uploadDate
{
    return [self datePropertyForKey:SFAnalyticsLoggerUploadDate];
}

- (void)setUploadDate:(NSDate*)uploadDate
{
    [self setDateProperty:uploadDate forKey:SFAnalyticsLoggerUploadDate];
}

- (void)clearAllData
{
    [self deleteFrom:SFAnalyticsLoggerTableSuccessCount where:@"event_type like ?" bindings:@[@"%"]];
    [self deleteFrom:SFAnalyticsLoggerTableHardFailures where:@"id >= 0" bindings:nil];
    [self deleteFrom:SFAnalyticsLoggerTableSoftFailures where:@"id >= 0" bindings:nil];
}

@end

#endif // __OBJC2__
