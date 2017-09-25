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

#import "CKKSLogger.h"
#import "debugging.h"
#import "CKKS.h"
#import "CKKSViewManager.h"
#import "keychain/ckks/CKKSKeychainView.h"
#include <utilities/SecFileLocations.h>
#import <Security/SFSQLite.h>
#import <os/log.h>

NSString* const CKKSLoggerTableSuccessCount = @"success_count";
NSString* const CKKSLoggerColumnEventType = @"event_type";
NSString* const CKKSLoggerColumnSuccessCount = @"success_count";
NSString* const CKKSLoggerColumnFailureCount = @"failure_count";

NSString* const CKKSLoggerTableFailures = @"failures";
NSString* const CKKSLoggerColumnData = @"data";

NSString* const CKKSLoggerUploadDate = @"upload_date";
NSString* const CKKSLoggerLastClassASync = @"last_class_a_sync";
NSString* const CKKSLoggerLastClassCSync = @"last_class_c_sync";

NSString* const CKKSLoggerDaysSinceLastSyncClassA = @"lastSyncClassA";
NSString* const CKKSLoggerDaysSinceLastSyncClassC = @"lastSyncClassC";

NSString* const CKKSLoggerSplunkTopic = @"topic";
NSString* const CKKSLoggerSplunkEventTime = @"eventTime";
NSString* const CKKSLoggerSplunkPostTime = @"postTime";
NSString* const CKKSLoggerSplunkEvents = @"events";
NSString* const CKKSLoggerSplunkEventType = @"eventType";
NSString* const CKKSLoggerMetricsBase = @"metricsBase";

NSString* const CKKSLoggerValueSuccess = @"success";

#define CKKS_SPLUNK_DEV 0

#if CKKS_SPLUNK_DEV
#define SECONDS_BETWEEN_UPLOADS 10
#else
// three days = 60 seconds times 60 minutes * 72 hours
#define SECONDS_BETWEEN_UPLOADS (60 * 60 * 72)
#endif

NSString* const CKKSLoggingTableSchema = @"CREATE TABLE IF NOT EXISTS failures (\n"
                                            @"id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
                                            @"data BLOB\n"
                                         @");\n"
                                         @"CREATE TRIGGER IF NOT EXISTS maintain_ring_buffer AFTER INSERT ON failures\n"
                                            @"BEGIN\n"
                                            @"DELETE FROM failures WHERE id != NEW.id AND id % 999 = NEW.id % 999;\n"
                                         @"END;\n"
                                         @"CREATE TABLE IF NOT EXISTS success_count (\n"
                                            @"event_type STRING PRIMARY KEY,\n"
                                            @"success_count INTEGER,\n"
                                            @"failure_count INTEGER\n"
                                         @");\n";

static NSString* CKKSLoggingTablePath()
{
    return [(__bridge_transfer NSURL*)SecCopyURLForFileInKeychainDirectory((__bridge CFStringRef)@"ckks_analytics_v1.db") path];
}

@interface CKKSLoggerSQLiteStore : SFSQLite

+ (instancetype)sharedStore;

@property (readonly, strong) NSArray* failureRecords;
@property (readwrite, strong) NSDate* uploadDate;

- (void)incrementSuccessCountForEventType:(NSString*)eventType;
- (void)incrementFailureCountForEventType:(NSString*)eventType;
- (NSInteger)successCountForEventType:(NSString*)eventType;
- (NSInteger)failureCountForEventType:(NSString*)eventType;
- (void)addFailureRecord:(NSDictionary*)valueDict;
- (void)clearAllData;

- (NSDictionary*)summaryCounts;

@end

@implementation CKKSLogger {
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
#endif
    static CKKSLogger* __sharedLogger;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        __sharedLogger = [[CKKSLogger alloc] init];
    });

    return __sharedLogger;
}

- (instancetype)init
{
    if (self = [super init]) {
        _queue = dispatch_queue_create("com.apple.security.ckks.logging", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
        _secondsBetweenUploads = SECONDS_BETWEEN_UPLOADS;
        
        NSDictionary* systemDefaultValues = [NSDictionary dictionaryWithContentsOfFile:[[NSBundle bundleWithPath:@"/System/Library/Frameworks/Security.framework"] pathForResource:@"CKKSLogging" ofType:@"plist"]];
        _splunkTopicName = systemDefaultValues[@"splunk_topic"];
        _splunkUploadURL = [NSURL URLWithString:systemDefaultValues[@"splunk_uploadURL"]];
        _splunkBagURL = [NSURL URLWithString:systemDefaultValues[@"splunk_bagURL"]];
        _allowInsecureSplunkCert = [[systemDefaultValues valueForKey:@"splunk_allowInsecureCertificate"] boolValue];
        NSString* splunkEndpoint = systemDefaultValues[@"splunk_endpointDomain"];

        NSUserDefaults* defaults = [[NSUserDefaults alloc] initWithSuiteName:SecCKKSUserDefaultsSuite];
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

#if CKKS_SPLUNK_DEV
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

- (void)setLastSuccessfulClassASyncDate:(NSDate*)lastSuccessfulClassASyncDate
{
    dispatch_sync(_queue, ^{
        [[CKKSLoggerSQLiteStore sharedStore] setDateProperty:lastSuccessfulClassASyncDate forKey:CKKSLoggerLastClassASync];
    });
}

- (NSDate*)lastSuccessfulClassASyncDate
{
    __block NSDate* result = nil;
    dispatch_sync(_queue, ^{
        result = [self _onQueueLastSuccessfulClassASyncDate];
    });

    return result;
}

- (NSDate*)_onQueueLastSuccessfulClassASyncDate
{
    dispatch_assert_queue(_queue);
    return [[CKKSLoggerSQLiteStore sharedStore] datePropertyForKey:CKKSLoggerLastClassASync] ?: [NSDate distantPast];
}

- (void)setLastSuccessfulClassCSyncDate:(NSDate*)lastSuccessfulClassCSyncDate
{
    dispatch_sync(_queue, ^{
        [[CKKSLoggerSQLiteStore sharedStore] setDateProperty:lastSuccessfulClassCSyncDate forKey:CKKSLoggerLastClassCSync];
    });
}

- (NSDate*)lastSuccessfulClassCSyncDate
{
    __block NSDate* result = nil;
    dispatch_sync(_queue, ^{
        result = [self _onQueueLastSuccessfulClassCSyncDate];
    });

    return result;
}

- (NSDate*)_onQueueLastSuccessfulClassCSyncDate
{
    dispatch_assert_queue(_queue);
    return [[CKKSLoggerSQLiteStore sharedStore] datePropertyForKey:CKKSLoggerLastClassCSync] ?: [NSDate distantPast];
}

- (void)logSuccessForEventNamed:(NSString*)eventName
{
    [self logEventNamed:eventName value:nil isSuccess:YES];
}

- (void)logFailureForEventNamed:(NSString*)eventName withAttributes:(NSDictionary*)attributes
{
    [self logEventNamed:eventName value:attributes isSuccess:NO];
}

- (void)logEventNamed:(NSString*)eventName value:(NSDictionary*)valueDict isSuccess:(BOOL)isSuccess
{
    __weak __typeof(self) weakSelf = self;
    dispatch_async(_queue, ^{

        __strong __typeof(self) strongSelf = weakSelf;
        if (!strongSelf) {
            return;
        }

        if (strongSelf->_disableLogging || [strongSelf->_blacklistedEvents containsObject:eventName]) {
            return;
        }

        CKKSLoggerSQLiteStore* store = [CKKSLoggerSQLiteStore sharedStore];
        if (isSuccess) {
            [store incrementSuccessCountForEventType:eventName];
        }
        else {
            [store incrementFailureCountForEventType:eventName];
            NSMutableDictionary* eventDict = valueDict.mutableCopy;
            eventDict[CKKSLoggerSplunkTopic] = strongSelf->_splunkTopicName;
            eventDict[CKKSLoggerSplunkEventType] = eventName;
            eventDict[CKKSLoggerSplunkEventTime] = @([[NSDate date] timeIntervalSince1970] * 1000);
            eventDict[CKKSLoggerMetricsBase] = strongSelf->_metricsBase ?: [NSDictionary dictionary];

            for (NSString* blacklistedField in strongSelf->_blacklistedFields) {
                [eventDict removeObjectForKey:blacklistedField];
            }

            [store addFailureRecord:eventDict];
        }

        NSDate* uploadDate = store.uploadDate;
        NSDate* nowDate = [NSDate date];
        if (uploadDate) {
            if ([nowDate compare:uploadDate] == NSOrderedDescending) {
                [self _onQueueUploadDataWithError:nil];
            }
        }
        else {
            store.uploadDate = [nowDate dateByAddingTimeInterval:strongSelf->_secondsBetweenUploads];
        }
    });
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
    dispatch_sync(_queue, ^{
        result = [self _onQueueUploadDataWithError:error];
    });
    return result;
}

- (BOOL)_onQueueUploadDataWithError:(NSError**)error
{
    dispatch_assert_queue(_queue);
    
    NSData* json = [self _onQueueGetLoggingJSONWithError:error];
    if (json && [self _onQueuePostJSON:json error:error]) {
        secinfo("ckks", "uploading sync health data: %@", json);
        
        CKKSLoggerSQLiteStore* store = [CKKSLoggerSQLiteStore sharedStore];
        [store clearAllData];
        store.uploadDate = [NSDate dateWithTimeIntervalSinceNow:_secondsBetweenUploads];
        return YES;
    }
    else {
        return NO;
    }
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

#define SECOND_PER_DAY (60 * 60 * 24)

- (NSInteger)fuzzyDaysSinceDate:(NSDate*)date
{
    NSTimeInterval timeIntervalSinceDate = [[NSDate date] timeIntervalSinceDate:date];
    if (timeIntervalSinceDate < SECOND_PER_DAY) {
        return 0;
    }
    else if (timeIntervalSinceDate < (SECOND_PER_DAY * 7)) {
        return 1;
    }
    else if (timeIntervalSinceDate < (SECOND_PER_DAY * 30)) {
        return 7;
    }
    else if (timeIntervalSinceDate < (SECOND_PER_DAY * 365)) {
        return 30;
    }
    else {
        return 365;
    }
}

- (NSData*)getLoggingJSONWithError:(NSError**)error
{
    __block NSData* json = nil;
    dispatch_sync(_queue, ^{
        json = [self _onQueueGetLoggingJSONWithError:error];
    });
    
    return json;
}

- (NSData*)_onQueueGetLoggingJSONWithError:(NSError**)error
{
    dispatch_assert_queue(_queue);
    
    CKKSLoggerSQLiteStore* store = [CKKSLoggerSQLiteStore sharedStore];
    NSArray* failureRecords = [store failureRecords];

    NSDictionary* successCounts = [store summaryCounts];
    NSInteger totalSuccessCount = 0;
    NSInteger totalFailureCount = 0;
    for (NSDictionary* perEventTypeSuccessCounts in successCounts.objectEnumerator) {
        totalSuccessCount += [perEventTypeSuccessCounts[CKKSLoggerColumnSuccessCount] integerValue];
        totalFailureCount += [perEventTypeSuccessCounts[CKKSLoggerColumnFailureCount] integerValue];
    }

    NSDate* now = [NSDate date];

    NSMutableDictionary* healthSummaryEvent = [[NSMutableDictionary alloc] init];
    healthSummaryEvent[CKKSLoggerSplunkTopic] = _splunkTopicName ?: [NSNull null];
    healthSummaryEvent[CKKSLoggerSplunkEventTime] = @([now timeIntervalSince1970] * 1000);
    healthSummaryEvent[CKKSLoggerSplunkEventType] = @"manifestHealthSummary";
    healthSummaryEvent[CKKSLoggerColumnSuccessCount] = @(totalSuccessCount);
    healthSummaryEvent[CKKSLoggerColumnFailureCount] = @(totalFailureCount);
    healthSummaryEvent[CKKSLoggerMetricsBase] = _metricsBase ?: [NSDictionary dictionary];

    for (NSString* viewName in [CKKSViewManager viewList]) {
        CKKSKeychainView* view = [CKKSViewManager findOrCreateView:viewName];
        [healthSummaryEvent setValue:@([self fuzzyDaysSinceDate:[self _onQueueLastSuccessfulClassASyncDate]]) forKey:[NSString stringWithFormat:@"%@-%@", view.zoneName, CKKSLoggerDaysSinceLastSyncClassA]];
        [healthSummaryEvent setValue:@([self fuzzyDaysSinceDate:[self _onQueueLastSuccessfulClassCSyncDate]]) forKey:[NSString stringWithFormat:@"%@-%@", view.zoneName, CKKSLoggerDaysSinceLastSyncClassC]];
    }

    NSMutableArray* splunkRecords = failureRecords.mutableCopy;
    [splunkRecords addObject:healthSummaryEvent];

    NSDictionary* jsonDict = @{CKKSLoggerSplunkPostTime : @([now timeIntervalSince1970] * 1000), @"events" : splunkRecords};

    return [NSJSONSerialization dataWithJSONObject:jsonDict options:NSJSONWritingPrettyPrinted error:error];
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

        OSStatus status = SecTrustEvaluate(serverTrust, &result);
        if (status == errSecSuccess && (result == kSecTrustResultProceed || result == kSecTrustResultUnspecified)) {
            /*
             * All is well, accept the credentials
             */

            cred = [NSURLCredential credentialForTrust:serverTrust];
            completionHandler(NSURLSessionAuthChallengeUseCredential, cred);
        } else if (_allowInsecureSplunkCert) {
            secnotice("ckks", "Force Accepting Splunk Credential");

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

@end

@implementation CKKSLoggerSQLiteStore

+ (instancetype)sharedStore
{
    static CKKSLoggerSQLiteStore* store = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        store = [[self alloc] initWithPath:CKKSLoggingTablePath() schema:CKKSLoggingTableSchema];
        [store open];
    });

    return store;
}

- (void)dealloc
{
    [self close];
}

- (NSInteger)successCountForEventType:(NSString*)eventType
{
    return [[[[self select:@[CKKSLoggerColumnSuccessCount] from:CKKSLoggerTableSuccessCount where:@"event_type = ?" bindings:@[eventType]] firstObject] valueForKey:CKKSLoggerColumnSuccessCount] integerValue];
}

- (void)incrementSuccessCountForEventType:(NSString*)eventType
{
    @try {
        NSInteger successCount = [self successCountForEventType:eventType];
        NSInteger failureCount = [self failureCountForEventType:eventType];
        [self insertOrReplaceInto:CKKSLoggerTableSuccessCount values:@{CKKSLoggerColumnEventType : eventType, CKKSLoggerColumnSuccessCount : @(successCount + 1), CKKSLoggerColumnFailureCount : @(failureCount)}];
    } @catch (id ue) {
        secerror("incrementSuccessCountForEventType exception: %@", ue);
    }
}

- (NSInteger)failureCountForEventType:(NSString*)eventType
{
    return [[[[self select:@[CKKSLoggerColumnFailureCount] from:CKKSLoggerTableSuccessCount where:@"event_type = ?" bindings:@[eventType]] firstObject] valueForKey:CKKSLoggerColumnFailureCount] integerValue];
}

- (void)incrementFailureCountForEventType:(NSString*)eventType
{
    @try {
        NSInteger successCount = [self successCountForEventType:eventType];
        NSInteger failureCount = [self failureCountForEventType:eventType];
        [self insertOrReplaceInto:CKKSLoggerTableSuccessCount values:@{CKKSLoggerColumnEventType : eventType, CKKSLoggerColumnSuccessCount : @(successCount), CKKSLoggerColumnFailureCount : @(failureCount + 1)}];
    } @catch (id ue) {
        secerror("incrementFailureCountForEventType exception: %@", ue);
    }
}

- (NSDictionary*)summaryCounts
{
    NSMutableDictionary* successCountsDict = [NSMutableDictionary dictionary];
    NSArray* rows = [self selectAllFrom:CKKSLoggerTableSuccessCount where:nil bindings:nil];
    for (NSDictionary* rowDict in rows) {
        successCountsDict[rowDict[CKKSLoggerColumnEventType]] = @{CKKSLoggerColumnSuccessCount : rowDict[CKKSLoggerColumnSuccessCount], CKKSLoggerColumnFailureCount : rowDict[CKKSLoggerColumnFailureCount]};
    }

    return successCountsDict;
}

- (NSArray*)failureRecords
{
    NSArray* recordBlobs = [self select:@[CKKSLoggerColumnData] from:CKKSLoggerTableFailures];

    NSMutableArray* failureRecords = [[NSMutableArray alloc] init];
    for (NSDictionary* row in recordBlobs) {
        NSDictionary* deserializedRecord = [NSPropertyListSerialization propertyListWithData:row[CKKSLoggerColumnData] options:0 format:nil error:nil];
        [failureRecords addObject:deserializedRecord];
    }

    return failureRecords;
}

- (void)addFailureRecord:(NSDictionary*)valueDict
{
    @try {
        NSError* error = nil;
        NSData* serializedRecord = [NSPropertyListSerialization dataWithPropertyList:valueDict format:NSPropertyListBinaryFormat_v1_0 options:0 error:&error];
        if(!error && serializedRecord) {
            [self insertOrReplaceInto:CKKSLoggerTableFailures values:@{CKKSLoggerColumnData : serializedRecord}];
        }
        if(error && !serializedRecord) {
            secerror("Couldn't serialize failure record: %@", error);
        }
    } @catch (id ue) {
        secerror("addFailureRecord exception: %@", ue);
    }
}

- (NSDate*)uploadDate
{
    return [self datePropertyForKey:CKKSLoggerUploadDate];
}

- (void)setUploadDate:(NSDate*)uploadDate
{
    [self setDateProperty:uploadDate forKey:CKKSLoggerUploadDate];
}

- (void)clearAllData
{
    [self deleteFrom:CKKSLoggerTableSuccessCount where:@"event_type like ?" bindings:@[@"%"]];
    [self deleteFrom:CKKSLoggerTableFailures where:@"id >= 0" bindings:nil];
}

@end

#endif // OCTAGON
