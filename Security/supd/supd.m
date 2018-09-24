/*
 * Copyright (c) 2017-2018 Apple Inc. All Rights Reserved.
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

#import "supd.h"
#import "SFAnalyticsDefines.h"
#import "SFAnalyticsSQLiteStore.h"
#import <Security/SFAnalytics.h>

#include <utilities/SecFileLocations.h>
#import "utilities/debugging.h"
#import <os/variant_private.h>
#import <xpc/xpc.h>
#include <notify.h>
#import "keychain/ckks/CKKSControl.h"
#import <zlib.h>

#import <AuthKit/AKAppleIDAuthenticationContext.h>
#import <AuthKit/AKAppleIDAuthenticationController.h>
#import <AuthKit/AKAppleIDAuthenticationController_Private.h>

#if TARGET_OS_OSX
#include "dirhelper_priv.h"
#import <Accounts/Accounts.h>
#import <Accounts/ACAccountStore_Private.h>
#import <Accounts/ACAccountType_Private.h>
#import <Accounts/ACAccountStore.h>
#import <AOSAccounts/ACAccountStore+iCloudAccount.h>
#import <AOSAccounts/ACAccount+iCloudAccount.h>
#import <AOSAccountsLite/AOSAccountsLite.h>
#import <CrashReporterSupport/CrashReporterSupportPrivate.h>
#else // TARGET_OS_OSX
#import <Accounts/Accounts.h>
#import <AppleAccount/AppleAccount.h>
#import <AppleAccount/ACAccount+AppleAccount.h>
#import <AppleAccount/ACAccountStore+AppleAccount.h>
#if TARGET_OS_EMBEDDED
#import <CrashReporterSupport/CrashReporterSupport.h>
#import <CrashReporterSupport/PreferenceManager.h>
#endif // TARGET_OS_EMBEDDED
#endif // TARGET_OS_OSX

NSString* const SFAnalyticsSplunkTopic = @"topic";
NSString* const SFAnalyticsSplunkPostTime = @"postTime";
NSString* const SFAnalyticsClientId = @"clientId";
NSString* const SFAnalyticsInternal = @"internal";

NSString* const SFAnalyticsMetricsBase = @"metricsBase";
NSString* const SFAnalyticsDeviceID = @"ckdeviceID";

NSString* const SFAnalyticsSecondsCustomerKey = @"SecondsBetweenUploadsCustomer";
NSString* const SFAnalyticsSecondsInternalKey = @"SecondsBetweenUploadsInternal";
NSString* const SFAnalyticsMaxEventsKey = @"NumberOfEvents";
NSString* const SFAnalyticsDevicePercentageCustomerKey = @"DevicePercentageCustomer";
NSString* const SFAnalyticsDevicePercentageInternalKey = @"DevicePercentageInternal";

#define SFANALYTICS_SPLUNK_DEV 0
#define OS_CRASH_TRACER_LOG_BUG_TYPE "226"

#if SFANALYTICS_SPLUNK_DEV
NSUInteger const secondsBetweenUploadsCustomer = 10;
NSUInteger const secondsBetweenUploadsInternal = 10;
#else // SFANALYTICS_SPLUNK_DEV
NSUInteger const secondsBetweenUploadsCustomer = (3 * (60 * 60 * 24));
NSUInteger const secondsBetweenUploadsInternal = (60 * 60 * 24);
#endif // SFANALYTICS_SPLUNK_DEV

@implementation SFAnalyticsReporter
- (BOOL)saveReport:(NSData *)reportData fileName:(NSString *)fileName
{
#if TARGET_OS_OSX
    NSDictionary *optionsDictionary = @{ (__bridge NSString *)kCRProblemReportSubmissionPolicyKey: (__bridge NSString *)kCRSubmissionPolicyAlternate };
#else // !TARGET_OS_OSX
    NSDictionary *optionsDictionary = nil; // The keys above are not defined or required on iOS.
#endif // !TARGET_OS_OSX

    BOOL writtenToLog = NO;
#if !TARGET_OS_SIMULATOR
    secdebug("saveReport", "calling out to `OSAWriteLogForSubmission`");
    writtenToLog = OSAWriteLogForSubmission(@OS_CRASH_TRACER_LOG_BUG_TYPE, fileName,
                                            nil, optionsDictionary, ^(NSFileHandle *fileHandle) {
                                                secnotice("OSAWriteLogForSubmission", "Writing log data to report: %@", fileName);
                                                [fileHandle writeData:reportData];
                                            });
#endif // !TARGET_OS_SIMULATOR
    return writtenToLog;
}
@end

#define DEFAULT_SPLUNK_MAX_EVENTS_TO_REPORT 1000
#define DEFAULT_SPLUNK_DEVICE_PERCENTAGE 100

static supd *_supdInstance = nil;

BOOL runningTests = NO;
BOOL deviceAnalyticsOverride = NO;
BOOL deviceAnalyticsEnabled = NO;
BOOL iCloudAnalyticsOverride = NO;
BOOL iCloudAnalyticsEnabled = NO;

static BOOL
_isDeviceAnalyticsEnabled(void)
{
    // This flag is only set during tests.
    if (deviceAnalyticsOverride) {
        return deviceAnalyticsEnabled;
    }

    static BOOL dataCollectionEnabled = NO;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
#if TARGET_OS_EMBEDDED
        dataCollectionEnabled = DiagnosticLogSubmissionEnabled();
#elif TARGET_OS_OSX
        dataCollectionEnabled = CRIsAutoSubmitEnabled();
#endif
    });
    return dataCollectionEnabled;
}

static NSString *const kAnalyticsiCloudIdMSKey = @"com.apple.idms.config.privacy.icloud.data";

#if TARGET_OS_IPHONE
static NSDictionary *
_getiCloudConfigurationInfoWithError(NSError **outError)
{
    __block NSDictionary *outConfigurationInfo = nil;
    __block NSError *localError = nil;

    ACAccountStore *accountStore = [[ACAccountStore alloc] init];
    ACAccount *primaryAccount = [accountStore aa_primaryAppleAccount];
    if (primaryAccount != nil) {
        NSString *altDSID = [primaryAccount aa_altDSID];
        secnotice("_getiCloudConfigurationInfoWithError", "Fetching configuration info");

        dispatch_semaphore_t sema = dispatch_semaphore_create(0);
        AKAppleIDAuthenticationController *authController = [AKAppleIDAuthenticationController new];
        [authController configurationInfoWithIdentifiers:@[kAnalyticsiCloudIdMSKey]
                                              forAltDSID:altDSID
                                              completion:^(NSDictionary<NSString *, id<NSSecureCoding>> *configurationInfo, NSError *error) {
            if (error) {
                secerror("_getiCloudConfigurationInfoWithError: Error fetching configurationInfo: %@", error);
                localError = error;
            } else if (![configurationInfo isKindOfClass:[NSDictionary class]]) {
                secerror("_getiCloudConfigurationInfoWithError: configurationInfo dict was not a dict, it was a %{public}@", [configurationInfo class]);
                localError = error;
                configurationInfo = nil;
            } else {
                secnotice("_getiCloudConfigurationInfoWithError", "fetched configurationInfo %@", configurationInfo);
                outConfigurationInfo = configurationInfo;
            }
            dispatch_semaphore_signal(sema);
        }];
        dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, (uint64_t)(5 * NSEC_PER_SEC)));
    } else {
        secerror("_getiCloudConfigurationInfoWithError: Failed to fetch primary account info.");
    }

    if (localError && outError) {
        *outError = localError;
    }
    return outConfigurationInfo;
}
#endif // TARGET_OS_IPHONE

#if TARGET_OS_OSX
static NSString *
_iCloudAccount(void)
{
    return CFBridgingRelease(MMLCopyLoggedInAccount());
}

static NSString *
_altDSIDFromAccount(void)
{
    static CFStringRef kMMPropertyAccountAlternateDSID = CFSTR("AccountAlternateDSID");
    NSString *account = _iCloudAccount();
    if (account != nil) {
        return CFBridgingRelease(MMLAccountCopyProperty((__bridge CFStringRef)account, kMMPropertyAccountAlternateDSID));
    }
    secerror("_altDSIDFromAccount: failed to fetch iCloud account");
    return nil;
}
#endif // TARGET_OS_OSX

static BOOL
_isiCloudAnalyticsEnabled()
{
    // This flag is only set during tests.
    if (iCloudAnalyticsOverride) {
        return iCloudAnalyticsEnabled;
    }

    static bool cachedAllowsICloudAnalytics = false;

#if TARGET_OS_OSX
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        /* AOSAccounts is not mastered into the BaseSystem. Check that those classes are linked at runtime and abort if not. */
        if (![AKAppleIDAuthenticationController class]) {
            secnotice("OTATrust", "Weak-linked AOSAccounts framework missing. Are we running in the base system?");
            return;
        }

        NSString *currentAltDSID = _altDSIDFromAccount();
        if (currentAltDSID != nil) {
            AKAppleIDAuthenticationController *authController = [AKAppleIDAuthenticationController new];
            __block bool allowsICloudAnalytics = false;
            dispatch_semaphore_t sem = dispatch_semaphore_create(0);
            secnotice("isiCloudAnalyticsEnabled", "fetching iCloud Analytics value from idms");
            [authController configurationInfoWithIdentifiers:@[kAnalyticsiCloudIdMSKey]
                                                  forAltDSID:currentAltDSID
                                                  completion:^(NSDictionary<NSString *, id> *configurationInfo, NSError *error) {
                                                      if (!error && configurationInfo) {
                                                          NSNumber *value = configurationInfo[kAnalyticsiCloudIdMSKey];
                                                          if (value != nil) {
                                                              secnotice("_isiCloudAnalyticsEnabled", "authController:configurationInfoWithIdentifiers completed with no error and configuration information");
                                                              allowsICloudAnalytics = [value boolValue];
                                                          } else {
                                                              secerror("%s: no iCloud Analytics value found in IDMS", __FUNCTION__);
                                                          }
                                                      } else {
                                                          secerror("%s: Unable to fetch iCloud Analytics value from IDMS.", __FUNCTION__);
                                                      }
                                                      secnotice("_isiCloudAnalyticsEnabled", "authController:configurationInfoWithIdentifiers completed and returning");
                                                      dispatch_semaphore_signal(sem);
                                                  }];
            // Wait 5 seconds before giving up and returning from the block.
            dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, (uint64_t)(5 * NSEC_PER_SEC)));
            cachedAllowsICloudAnalytics = allowsICloudAnalytics;
        } else {
            secerror("_isiCloudAnalyticsEnabled: Failed to fetch alternate DSID");
        }
    });
#else // TARGET_OS_OSX
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSError *error = nil;
        NSDictionary *accountConfiguration = _getiCloudConfigurationInfoWithError(&error);
        if (error == nil && accountConfiguration != nil) {
            id iCloudAnalyticsOptIn = accountConfiguration[kAnalyticsiCloudIdMSKey];
            if (iCloudAnalyticsOptIn != nil) {
                BOOL iCloudAnalyticsOptInHasCorrectType = ([iCloudAnalyticsOptIn isKindOfClass:[NSNumber class]] || [iCloudAnalyticsOptIn isKindOfClass:[NSString class]]);
                if (iCloudAnalyticsOptInHasCorrectType) {
                    NSNumber *iCloudAnalyticsOptInNumber = @([iCloudAnalyticsOptIn integerValue]);
                    cachedAllowsICloudAnalytics = ![iCloudAnalyticsOptInNumber isEqualToNumber:[NSNumber numberWithInteger:0]];
                }
            }
        } else if (error != nil) {
            secerror("_isiCloudAnalyticsEnabled: %@", error);
        }
    });
#endif // TARGET_OS_OSX

    return cachedAllowsICloudAnalytics;
}

/*  NSData GZip category based on GeoKit's implementation */
@interface NSData (GZip)
- (NSData *)supd_gzipDeflate;
@end

#define GZIP_OFFSET 16
#define GZIP_STRIDE_LEN 16384

@implementation NSData (Gzip)
- (NSData *)supd_gzipDeflate
{
    if ([self length] == 0) {
        return self;
    }

    z_stream strm;
    memset(&strm, 0, sizeof(strm));
    strm.next_in=(uint8_t *)[self bytes];
    strm.avail_in = (unsigned int)[self length];


    if (Z_OK != deflateInit2(&strm, Z_BEST_COMPRESSION, Z_DEFLATED,
                             MAX_WBITS + GZIP_OFFSET, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY)) {
        return nil;
    }

    NSMutableData *compressed = [NSMutableData dataWithLength:GZIP_STRIDE_LEN];

    do {
        if (strm.total_out >= [compressed length]) {
            [compressed increaseLengthBy: 16384];
        }

        strm.next_out = [compressed mutableBytes] + strm.total_out;
        strm.avail_out = (int)[compressed length] - (int)strm.total_out;

        deflate(&strm, Z_FINISH);

    } while (strm.avail_out == 0);

    deflateEnd(&strm);

    [compressed setLength: strm.total_out];
    if (strm.avail_in == 0) {
        return [NSData dataWithData:compressed];
    } else {
        return nil;
    }
}
@end

@implementation SFAnalyticsClient {
    NSString* _path;
    NSString* _name;
    BOOL _requireDeviceAnalytics;
    BOOL _requireiCloudAnalytics;
}

@synthesize storePath = _path;
@synthesize name = _name;

- (instancetype)initWithStorePath:(NSString*)path name:(NSString*)name
                  deviceAnalytics:(BOOL)deviceAnalytics iCloudAnalytics:(BOOL)iCloudAnalytics {
    if (self = [super init]) {
        _path = path;
        _name = name;
        _requireDeviceAnalytics = deviceAnalytics;
        _requireiCloudAnalytics = iCloudAnalytics;
    }
    return self;
}

@end

@interface SFAnalyticsTopic ()
@property NSURL* _splunkUploadURL;

@property BOOL allowInsecureSplunkCert;
@property BOOL ignoreServersMessagesTellingUsToGoAway;
@property BOOL disableUploads;
@property BOOL disableClientId;

@property NSUInteger secondsBetweenUploads;
@property NSUInteger maxEventsToReport;
@property float devicePercentage; // for sampling reporting devices

@property NSDictionary* metricsBase; // data the server provides and wants us to send back
@property NSArray* blacklistedFields;
@property NSArray* blacklistedEvents;
@end

@implementation SFAnalyticsTopic

- (void)setupClientsForTopic:(NSString *)topicName
{
    NSMutableArray<SFAnalyticsClient*>* clients = [NSMutableArray<SFAnalyticsClient*> new];
    if ([topicName isEqualToString:SFAnalyticsTopicKeySync]) {
        [clients addObject:[[SFAnalyticsClient alloc] initWithStorePath:[self.class databasePathForCKKS]
                                                                   name:@"ckks" deviceAnalytics:NO iCloudAnalytics:YES]];
        [clients addObject:[[SFAnalyticsClient alloc] initWithStorePath:[self.class databasePathForSOS]
                                                                   name:@"sos" deviceAnalytics:NO iCloudAnalytics:YES]];
        [clients addObject:[[SFAnalyticsClient alloc] initWithStorePath:[self.class databasePathForPCS]
                                                                   name:@"pcs" deviceAnalytics:NO iCloudAnalytics:YES]];
        [clients addObject:[[SFAnalyticsClient alloc] initWithStorePath:[self.class databasePathForSignIn]
                                                                   name:@"signins" deviceAnalytics:NO iCloudAnalytics:YES]];
        [clients addObject:[[SFAnalyticsClient alloc] initWithStorePath:[self.class databasePathForLocal]
                                                                   name:@"local" deviceAnalytics:YES iCloudAnalytics:NO]];
    } else if ([topicName isEqualToString:SFAnaltyicsTopicTrust]) {
#if TARGET_OS_OSX
        _set_user_dir_suffix("com.apple.trustd"); // supd needs to read trustd's cache dir for these
#endif
        [clients addObject:[[SFAnalyticsClient alloc] initWithStorePath:[self.class databasePathForTrust]
                                                                   name:@"trust" deviceAnalytics:YES iCloudAnalytics:NO]];
        [clients addObject:[[SFAnalyticsClient alloc] initWithStorePath:[self.class databasePathForTrustdHealth]
                                                                   name:@"trustdHealth" deviceAnalytics:YES iCloudAnalytics:NO]];
        [clients addObject:[[SFAnalyticsClient alloc] initWithStorePath:[self.class databasePathForTLS]
                                                                   name:@"tls" deviceAnalytics:YES iCloudAnalytics:NO]];

#if TARGET_OS_OSX
        _set_user_dir_suffix(NULL); // set back to the default cache dir
#endif
    }

    _topicClients = clients;
}

- (instancetype)initWithDictionary:(NSDictionary *)dictionary name:(NSString *)topicName samplingRates:(NSDictionary *)rates {
    if (self = [super init]) {
        _internalTopicName = topicName;
        [self setupClientsForTopic:topicName];
        _splunkTopicName = dictionary[@"splunk_topic"];
        __splunkUploadURL = [NSURL URLWithString:dictionary[@"splunk_uploadURL"]];
        _splunkBagURL = [NSURL URLWithString:dictionary[@"splunk_bagURL"]];
        _allowInsecureSplunkCert = [[dictionary valueForKey:@"splunk_allowInsecureCertificate"] boolValue];
        NSString* splunkEndpoint = dictionary[@"splunk_endpointDomain"];
        if (dictionary[@"disableClientId"]) {
            _disableClientId = YES;
        }

        NSUserDefaults* defaults = [[NSUserDefaults alloc] initWithSuiteName:SFAnalyticsUserDefaultsSuite];
        NSString* userDefaultsSplunkTopic = [defaults stringForKey:@"splunk_topic"];
        if (userDefaultsSplunkTopic) {
            _splunkTopicName = userDefaultsSplunkTopic;
        }

        NSURL* userDefaultsSplunkUploadURL = [NSURL URLWithString:[defaults stringForKey:@"splunk_uploadURL"]];
        if (userDefaultsSplunkUploadURL) {
            __splunkUploadURL = userDefaultsSplunkUploadURL;
        }

        NSURL* userDefaultsSplunkBagURL = [NSURL URLWithString:[defaults stringForKey:@"splunk_bagURL"]];
        if (userDefaultsSplunkBagURL) {
            _splunkBagURL = userDefaultsSplunkBagURL;
        }

        BOOL userDefaultsAllowInsecureSplunkCert = [defaults boolForKey:@"splunk_allowInsecureCertificate"];
        _allowInsecureSplunkCert |= userDefaultsAllowInsecureSplunkCert;

        NSString* userDefaultsSplunkEndpoint = [defaults stringForKey:@"splunk_endpointDomain"];
        if (userDefaultsSplunkEndpoint) {
            splunkEndpoint = userDefaultsSplunkEndpoint;
        }

#if SFANALYTICS_SPLUNK_DEV
        _secondsBetweenUploads = secondsBetweenUploadsInternal;
        _maxEventsToReport = SFAnalyticsMaxEventsToReport;
        _devicePercentage = DEFAULT_SPLUNK_DEVICE_PERCENTAGE;
#else
        bool internal = os_variant_has_internal_diagnostics("com.apple.security");
        if (rates) {
            NSNumber *secondsNum = internal ? rates[SFAnalyticsSecondsInternalKey] : rates[SFAnalyticsSecondsCustomerKey];
            _secondsBetweenUploads = [secondsNum integerValue];
            _maxEventsToReport = [rates[SFAnalyticsMaxEventsKey] unsignedIntegerValue];
            NSNumber *percentageNum = internal ? rates[SFAnalyticsDevicePercentageInternalKey] : rates[SFAnalyticsDevicePercentageCustomerKey];
            _devicePercentage = [percentageNum floatValue];
        } else {
            _secondsBetweenUploads = internal ? secondsBetweenUploadsInternal : secondsBetweenUploadsCustomer;
            _maxEventsToReport = SFAnalyticsMaxEventsToReport;
            _devicePercentage = DEFAULT_SPLUNK_DEVICE_PERCENTAGE;
        }
#endif
        secnotice("supd", "created %@ with %lu seconds between uploads, %lu max events, %f percent of uploads",
                  _internalTopicName, (unsigned long)_secondsBetweenUploads, (unsigned long)_maxEventsToReport, _devicePercentage);

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

- (BOOL)isSampledUpload
{
    uint32_t sample = arc4random();
    if ((double)_devicePercentage < ((double)1 / UINT32_MAX) * 100) {
        /* Requested percentage is smaller than we can sample. just do 1 out of UINT32_MAX */
        if (sample == 0) {
            return YES;
        }
    } else {
        if ((double)sample <= (double)UINT32_MAX * ((double)_devicePercentage / 100)) {
            return YES;
        }
    }
    return NO;
}

- (BOOL)postJSON:(NSData*)json toEndpoint:(NSURL*)endpoint error:(NSError**)error
{
    if (!endpoint) {
        if (error) {
            NSString *description = [NSString stringWithFormat:@"No endpoint for %@", _internalTopicName];
            *error = [NSError errorWithDomain:@"SupdUploadErrorDomain"
                                         code:-10
                                     userInfo:@{NSLocalizedDescriptionKey : description}];
        }
        return false;
    }
    /*
     * Create the NSURLSession
     *  We use the ephemeral session config because we don't need cookies or cache
     */
    NSURLSessionConfiguration *configuration = [NSURLSessionConfiguration ephemeralSessionConfiguration];

    configuration.HTTPAdditionalHeaders = @{ @"User-Agent" : [NSString stringWithFormat:@"securityd/%s", SECURITY_BUILD_VERSION]};

    NSURLSession* postSession = [NSURLSession sessionWithConfiguration:configuration
                                                              delegate:self
                                                         delegateQueue:nil];

    NSMutableURLRequest* postRequest = [[NSMutableURLRequest alloc] init];
    postRequest.URL = endpoint;
    postRequest.HTTPMethod = @"POST";
    postRequest.HTTPBody = [json supd_gzipDeflate];
    [postRequest setValue:@"gzip" forHTTPHeaderField:@"Content-Encoding"];

    /*
     * Create the upload task.
     */
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    __block BOOL uploadSuccess = NO;
    NSURLSessionDataTask* uploadTask = [postSession dataTaskWithRequest:postRequest
                                                      completionHandler:^(NSData * _Nullable __unused data, NSURLResponse * _Nullable response, NSError * _Nullable requestError) {
        if (requestError) {
            secerror("Error in uploading the events to splunk for %@: %@", self->_internalTopicName, requestError);
        } else if (![response isKindOfClass:NSHTTPURLResponse.class]){
            Class class = response.class;
            secerror("Received the wrong kind of response for %@: %@", self->_internalTopicName, NSStringFromClass(class));
        } else {
            NSHTTPURLResponse* httpResponse = (NSHTTPURLResponse*)response;
            if(httpResponse.statusCode >= 200 && httpResponse.statusCode < 300) {
                /* Success */
                uploadSuccess = YES;
                secnotice("upload", "Splunk upload success for %@", self->_internalTopicName);
            } else {
                secnotice("upload", "Splunk upload for %@ unexpected status to URL: %@ -- status: %d",
                          self->_internalTopicName, endpoint, (int)(httpResponse.statusCode));
            }
        }
        dispatch_semaphore_signal(sem);
    }];
    secnotice("upload", "Splunk upload start for %@", self->_internalTopicName);
    [uploadTask resume];
    dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, (uint64_t)(5 * 60 * NSEC_PER_SEC)));
    return uploadSuccess;
}

- (BOOL)eventIsBlacklisted:(NSMutableDictionary*)event {
    return _blacklistedEvents ? [_blacklistedEvents containsObject:event[SFAnalyticsEventType]] : NO;
}

- (void)removeBlacklistedFieldsFromEvent:(NSMutableDictionary*)event {
    for (NSString* badField in self->_blacklistedFields) {
        [event removeObjectForKey:badField];
    }
}

- (void)addRequiredFieldsToEvent:(NSMutableDictionary*)event {
    [_metricsBase enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key, id  _Nonnull obj, BOOL * _Nonnull stop) {
        if (!event[key]) {
            event[key] = obj;
        }
    }];
}

- (BOOL)prepareEventForUpload:(NSMutableDictionary*)event {
    if ([self eventIsBlacklisted:event]) {
        return NO;
    }

    [self removeBlacklistedFieldsFromEvent:event];
    [self addRequiredFieldsToEvent:event];
    if (_disableClientId) {
        event[SFAnalyticsClientId] = @(0);
    }
    event[SFAnalyticsSplunkTopic] = self->_splunkTopicName ?: [NSNull null];
    return YES;
}

- (void)addFailures:(NSMutableArray<NSArray*>*)failures toUploadRecords:(NSMutableArray*)records threshold:(NSUInteger)threshold
{
    // The first 0 through 'threshold' items are getting uploaded in any case (which might be 0 for lower priority data)

    for (NSArray* client in failures) {
        [client enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
            if (idx >= threshold) {
                *stop = YES;
                return;
            }
            if ([self prepareEventForUpload:obj]) {
                [records addObject:obj];
            }
        }];
    }

    // Are there more items than we shoved into the upload records?
    NSInteger excessItems = 0;
    for (NSArray* client in failures) {
        NSInteger localExcess = client.count - threshold;
        excessItems += localExcess > 0 ? localExcess : 0;
    }

    // Then, if we have space and items left, apply a scaling factor to distribute events across clients to fill upload buffer
    if (records.count < _maxEventsToReport && excessItems > 0) {
        double scale = (_maxEventsToReport - records.count) / (double)excessItems;
        if (scale > 1) {
            scale = 1;
        }

        for (NSArray* client in failures) {
            if (client.count > threshold) {
                NSRange range = NSMakeRange(threshold, (client.count - threshold) * scale);
                NSArray* sub = [client subarrayWithRange:range];
                [sub enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
                    if ([self prepareEventForUpload:obj]) {
                        [records addObject:obj];
                    }
                }];
            }
        }
    }
}

- (NSMutableDictionary*)sampleStatisticsForSamples:(NSArray*)samples withName:(NSString*)name
{
    NSMutableDictionary* statistics = [NSMutableDictionary dictionary];
    NSUInteger count = samples.count;
    NSArray* sortedSamples = [samples sortedArrayUsingSelector:@selector(compare:)];
    NSArray* samplesAsExpressionArray = @[[NSExpression expressionForConstantValue:sortedSamples]];

    if (count == 1) {
        statistics[name] = samples[0];
    } else {
        // NSExpression takes population standard deviation. Our data is a sample of whatever we sampled over time,
        // but the difference between the two is fairly minor (divide by N before taking sqrt versus divide by N-1).
        statistics[[NSString stringWithFormat:@"%@-dev", name]] = [[NSExpression expressionForFunction:@"stddev:" arguments:samplesAsExpressionArray] expressionValueWithObject:nil context:nil];

        statistics[[NSString stringWithFormat:@"%@-min", name]] = [[NSExpression expressionForFunction:@"min:" arguments:samplesAsExpressionArray] expressionValueWithObject:nil context:nil];
        statistics[[NSString stringWithFormat:@"%@-max", name]] = [[NSExpression expressionForFunction:@"max:" arguments:samplesAsExpressionArray] expressionValueWithObject:nil context:nil];
        statistics[[NSString stringWithFormat:@"%@-avg", name]] = [[NSExpression expressionForFunction:@"average:" arguments:samplesAsExpressionArray] expressionValueWithObject:nil context:nil];
        statistics[[NSString stringWithFormat:@"%@-med", name]] = [[NSExpression expressionForFunction:@"median:" arguments:samplesAsExpressionArray] expressionValueWithObject:nil context:nil];
    }

    if (count > 3) {
        NSString* q1 = [NSString stringWithFormat:@"%@-1q", name];
        NSString* q3 = [NSString stringWithFormat:@"%@-3q", name];
        // From Wikipedia, which is never wrong
        if (count % 2 == 0) {
            // The lower quartile value is the median of the lower half of the data. The upper quartile value is the median of the upper half of the data.
            statistics[q1] = [[NSExpression expressionForFunction:@"median:" arguments:@[[NSExpression expressionForConstantValue:[sortedSamples subarrayWithRange:NSMakeRange(0, count / 2)]]]] expressionValueWithObject:nil context:nil];
            statistics[q3] = [[NSExpression expressionForFunction:@"median:" arguments:@[[NSExpression expressionForConstantValue:[sortedSamples subarrayWithRange:NSMakeRange((count / 2), count / 2)]]]] expressionValueWithObject:nil context:nil];
        } else if (count % 4 == 1) {
            // If there are (4n+1) data points, then the lower quartile is 25% of the nth data value plus 75% of the (n+1)th data value;
            // the upper quartile is 75% of the (3n+1)th data point plus 25% of the (3n+2)th data point.
            // (offset n by -1 since we count from 0)
            NSUInteger n = count / 4;
            statistics[q1] = @(([sortedSamples[n - 1] doubleValue] + [sortedSamples[n] doubleValue] * 3.0) / 4.0);
            statistics[q3] = @(([sortedSamples[(3 * n)] doubleValue] * 3.0 + [sortedSamples[(3 * n) + 1] doubleValue]) / 4.0);
        } else if (count % 4 == 3){
            // If there are (4n+3) data points, then the lower quartile is 75% of the (n+1)th data value plus 25% of the (n+2)th data value;
            // the upper quartile is 25% of the (3n+2)th data point plus 75% of the (3n+3)th data point.
            // (offset n by -1 since we count from 0)
            NSUInteger n = count / 4;
            statistics[q1] = @(([sortedSamples[n] doubleValue] * 3.0 + [sortedSamples[n + 1] doubleValue]) / 4.0);
            statistics[q3] = @(([sortedSamples[(3 * n) + 1] doubleValue] + [sortedSamples[(3 * n) + 2] doubleValue] * 3.0) / 4.0);
        }
    }

    return statistics;
}

- (NSMutableDictionary*)healthSummaryWithName:(NSString*)name store:(SFAnalyticsSQLiteStore*)store
{
    __block NSMutableDictionary* summary = [NSMutableDictionary new];

    // Add some events of our own before pulling in data
    summary[SFAnalyticsEventType] = [NSString stringWithFormat:@"%@HealthSummary", name];
    if ([self eventIsBlacklisted:summary]) {
        return nil;
    }
    summary[SFAnalyticsEventTime] = @([[NSDate date] timeIntervalSince1970] * 1000);    // Splunk wants milliseconds
    [SFAnalytics addOSVersionToEvent:summary];

    // Process counters
    NSDictionary* successCounts = store.summaryCounts;
    __block NSInteger totalSuccessCount = 0;
    __block NSInteger totalHardFailureCount = 0;
    __block NSInteger totalSoftFailureCount = 0;
    [successCounts enumerateKeysAndObjectsUsingBlock:^(NSString* _Nonnull eventType, NSDictionary* _Nonnull counts, BOOL* _Nonnull stop) {
        summary[[NSString stringWithFormat:@"%@-success", eventType]] = counts[SFAnalyticsColumnSuccessCount];
        summary[[NSString stringWithFormat:@"%@-hardfail", eventType]] = counts[SFAnalyticsColumnHardFailureCount];
        summary[[NSString stringWithFormat:@"%@-softfail", eventType]] = counts[SFAnalyticsColumnSoftFailureCount];
        totalSuccessCount += [counts[SFAnalyticsColumnSuccessCount] integerValue];
        totalHardFailureCount += [counts[SFAnalyticsColumnHardFailureCount] integerValue];
        totalSoftFailureCount += [counts[SFAnalyticsColumnSoftFailureCount] integerValue];
    }];

    summary[SFAnalyticsColumnSuccessCount] = @(totalSuccessCount);
    summary[SFAnalyticsColumnHardFailureCount] = @(totalHardFailureCount);
    summary[SFAnalyticsColumnSoftFailureCount] = @(totalSoftFailureCount);
    if (os_variant_has_internal_diagnostics("com.apple.security")) {
        summary[SFAnalyticsInternal] = @YES;
    }

    // Process samples
    NSMutableDictionary<NSString*,NSMutableArray*>* samplesBySampler = [NSMutableDictionary<NSString*,NSMutableArray*> dictionary];
    for (NSDictionary* sample in [store samples]) {
        if (!samplesBySampler[sample[SFAnalyticsColumnSampleName]]) {
            samplesBySampler[sample[SFAnalyticsColumnSampleName]] = [NSMutableArray array];
        }
        [samplesBySampler[sample[SFAnalyticsColumnSampleName]] addObject:sample[SFAnalyticsColumnSampleValue]];
    }
    [samplesBySampler enumerateKeysAndObjectsUsingBlock:^(NSString * _Nonnull key, NSMutableArray * _Nonnull obj, BOOL * _Nonnull stop) {
        NSMutableDictionary* event = [self sampleStatisticsForSamples:obj withName:key];
        [summary addEntriesFromDictionary:event];
    }];

    // Should always return yes because we already checked for event blacklisting specifically
    if (![self prepareEventForUpload:summary]) {
        return nil;
    }
    return summary;
}

- (void)updateUploadDateForClients:(NSArray<SFAnalyticsClient*>*)clients clearData:(BOOL)clearData
{
    for (SFAnalyticsClient* client in clients) {
        SFAnalyticsSQLiteStore* store = [SFAnalyticsSQLiteStore storeWithPath:client.storePath schema:SFAnalyticsTableSchema];
        secnotice("postprocess", "Setting upload date for client: %@", client.name);
        store.uploadDate = [NSDate date];
        if (clearData) {
            secnotice("postprocess", "Clearing collected data for client: %@", client.name);
            [store clearAllData];
        }
    }
}

- (NSData*)getLoggingJSON:(bool)pretty
                forUpload:(BOOL)upload
     participatingClients:(NSMutableArray<SFAnalyticsClient*>**)clients
                    force:(BOOL)force                                       // supdctl uploads ignore privacy settings and recency
                    error:(NSError**)error
{
    NSMutableArray<SFAnalyticsClient*>* localClients = [NSMutableArray new];
    __block NSMutableArray* uploadRecords = [NSMutableArray arrayWithCapacity:_maxEventsToReport];
    __block NSError *localError;
    __block NSMutableArray<NSArray*>* hardFailures = [NSMutableArray new];
    __block NSMutableArray<NSArray*>* softFailures = [NSMutableArray new];
    NSString* ckdeviceID = nil;
    if ([_internalTopicName isEqualToString:SFAnalyticsTopicKeySync]) {
        ckdeviceID = os_variant_has_internal_diagnostics("com.apple.security") ? [self askSecurityForCKDeviceID] : nil;
    }
    for (SFAnalyticsClient* client in self->_topicClients) {
        if (!force && [client requireDeviceAnalytics] && !_isDeviceAnalyticsEnabled()) {
            // Client required device analytics, yet the user did not opt in. 
            secnotice("getLoggingJSON", "Client '%@' requires device analytics yet user did not opt in.", [client name]);
            continue;
        } 
        if (!force && [client requireiCloudAnalytics] && !_isiCloudAnalyticsEnabled()) {
            // Client required iCloud analytics, yet the user did not opt in. 
            secnotice("getLoggingJSON", "Client '%@' requires iCloud analytics yet user did not opt in.", [client name]);
            continue;
        }

        SFAnalyticsSQLiteStore* store = [SFAnalyticsSQLiteStore storeWithPath:client.storePath schema:SFAnalyticsTableSchema];

        if (upload) {
            NSDate* uploadDate = store.uploadDate;
            if (!force && uploadDate && [[NSDate date] timeIntervalSinceDate:uploadDate] < _secondsBetweenUploads) {
                secnotice("json", "ignoring client '%@' for %@ because last upload too recent: %@",
                          client.name, _internalTopicName, uploadDate);
                continue;
            }

            if (!force && !uploadDate) {
                secnotice("json", "ignoring client '%@' because doesn't have an upload date; giving it a baseline date",
                          client.name);
                [self updateUploadDateForClients:@[client] clearData:NO];
                continue;
            }

            if (force) {
                secnotice("json", "client '%@' for topic '%@' force-included", client.name, _internalTopicName);
            } else {
                secnotice("json", "including client '%@' for topic '%@' for upload", client.name, _internalTopicName);
            }
            [localClients addObject:client];
        }

        NSMutableDictionary* healthSummary = [self healthSummaryWithName:client.name store:store];
        if (healthSummary) {
            if (ckdeviceID) {
                healthSummary[SFAnalyticsDeviceID] = ckdeviceID;
            }
            [uploadRecords addObject:healthSummary];
        }

        [hardFailures addObject:store.hardFailures];
        [softFailures addObject:store.softFailures];
    }

    if (upload && [localClients count] == 0) {
        if (error) {
            NSString *description = [NSString stringWithFormat:@"Upload too recent for all clients for %@", _internalTopicName];
            *error = [NSError errorWithDomain:@"SupdUploadErrorDomain"
                                         code:-10
                                     userInfo:@{NSLocalizedDescriptionKey : description}];
        }
        return nil;
    }

    if (clients) {
        *clients = localClients;
    }

    [self addFailures:hardFailures toUploadRecords:uploadRecords threshold:_maxEventsToReport/10];
    [self addFailures:softFailures toUploadRecords:uploadRecords threshold:0];

    NSDictionary* jsonDict = @{
                               SFAnalyticsSplunkPostTime : @([[NSDate date] timeIntervalSince1970] * 1000),
                               @"events" : uploadRecords
                               };

    NSData *json = [NSJSONSerialization dataWithJSONObject:jsonDict
                                                   options:(pretty ? NSJSONWritingPrettyPrinted : 0)
                                                     error:&localError];
    
    if (error) {
        *error = localError;
    }

    return json;
}

// Is at least one client eligible for data collection based on user consent? Otherwise callers should NOT reach off-device.
- (BOOL)haveEligibleClients {
    for (SFAnalyticsClient* client in self.topicClients) {
        if ((!client.requireDeviceAnalytics || _isDeviceAnalyticsEnabled()) &&
            (!client.requireiCloudAnalytics || _isiCloudAnalyticsEnabled())) {
            return YES;
        }
    }
    return NO;
}

- (NSString*)askSecurityForCKDeviceID
{
    NSError* error = nil;
    CKKSControl* rpc = [CKKSControl controlObject:&error];
    if(error || !rpc) {
        secerror("unable to obtain CKKS endpoint: %@", error);
        return nil;
    }

    __block NSString* localCKDeviceID;
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    [rpc rpcGetCKDeviceIDWithReply:^(NSString* ckdeviceID) {
        localCKDeviceID = ckdeviceID;
        dispatch_semaphore_signal(sema);
    }];

    if (dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 10)) != 0) {
        secerror("timed out waiting for a response from security");
        return nil;
    }

    return localCKDeviceID;
}

// this method is kind of evil for the fact that it has side-effects in pulling other things besides the metricsURL from the server, and as such should NOT be memoized.
// TODO redo this, probably to return a dictionary.
- (NSURL*)splunkUploadURL:(BOOL)force
{
    if (!force && ![self haveEligibleClients]) {    // force is true IFF called from supdctl. Customers don't have it and internal audiences must call it explicitly.
        secnotice("getURL", "Not going to talk to server for topic %@ because no eligible clients", [self internalTopicName]);
        return nil;
    }

    if (__splunkUploadURL) {
        return __splunkUploadURL;
    }

    secnotice("getURL", "Asking server for endpoint and config data for topic %@", [self internalTopicName]);

    __weak __typeof(self) weakSelf = self;
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);

    __block NSError* error = nil;
    NSURLSessionConfiguration *configuration = [NSURLSessionConfiguration ephemeralSessionConfiguration];
    NSURLSession* storeBagSession = [NSURLSession sessionWithConfiguration:configuration
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
                    self->_disableUploads = [[responseDict valueForKey:@"sendDisabled"] boolValue];
                    if (self->_disableUploads) {
                        // then don't upload anything right now
                        secerror("not returning a splunk URL because uploads are disabled for %@", self->_internalTopicName);
                        dispatch_semaphore_signal(sem);
                        return;
                    }

                    // backend works with milliseconds
                    NSUInteger secondsBetweenUploads = [[responseDict valueForKey:@"postFrequency"] unsignedIntegerValue] / 1000;
                    if (secondsBetweenUploads > 0) {
                        if (os_variant_has_internal_diagnostics("com.apple.security") &&
                            self->_secondsBetweenUploads < secondsBetweenUploads) {
                            secnotice("getURL", "Overriding server-sent post frequency because device is internal (%lu -> %lu)", (unsigned long)secondsBetweenUploads, (unsigned long)self->_secondsBetweenUploads);
                        } else {
                            strongSelf->_secondsBetweenUploads = secondsBetweenUploads;
                        }
                    }

                    strongSelf->_blacklistedEvents = responseDict[@"blacklistedEvents"];
                    strongSelf->_blacklistedFields = responseDict[@"blacklistedFields"];
                }

                strongSelf->_metricsBase = responseDict[@"metricsBase"];

                NSString* metricsEndpoint = responseDict[@"metricsUrl"];
                if([metricsEndpoint isKindOfClass:NSString.class]) {
                    /* Lives our URL */
                    NSString* endpoint = [metricsEndpoint stringByAppendingFormat:@"/2/%@", strongSelf->_splunkTopicName];
                    secnotice("upload", "got metrics endpoint %@ for %@", endpoint, self->_internalTopicName);
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
        if (error) {
            secnotice("upload", "Unable to fetch splunk endpoint at URL for %@: %@ -- error: %@",
                      self->_internalTopicName, requestEndpoint, error.description);
        }
        else if (!result) {
            secnotice("upload", "Malformed iTunes config payload for %@!", self->_internalTopicName);
        }

        dispatch_semaphore_signal(sem);
    }];

    [storeBagTask resume];
    dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, (uint64_t)(60 * NSEC_PER_SEC)));

    return result;
}

- (void)URLSession:(NSURLSession *)session didReceiveChallenge:(NSURLAuthenticationChallenge *)challenge
 completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition, NSURLCredential *))completionHandler {
    assert(completionHandler);
    (void)session;
    secnotice("upload", "Splunk upload challenge for %@", _internalTopicName);
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
        // Coverity gets upset if we don't check status even though result is all we need.
        OSStatus status = SecTrustEvaluate(serverTrust, &result);
        if (_allowInsecureSplunkCert || (status == errSecSuccess && ((result == kSecTrustResultProceed) || (result == kSecTrustResultUnspecified)))) {
            /*
             * All is well, accept the credentials
             */
            if(_allowInsecureSplunkCert) {
                secnotice("upload", "Force Accepting Splunk Credential for %@", _internalTopicName);
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

- (NSDictionary*)eventDictWithBlacklistedFieldsStrippedFrom:(NSDictionary*)eventDict
{
    NSMutableDictionary* strippedDict = eventDict.mutableCopy;
    for (NSString* blacklistedField in _blacklistedFields) {
        [strippedDict removeObjectForKey:blacklistedField];
    }
    return strippedDict;
}

// MARK: Database path retrieval

+ (NSString*)databasePathForCKKS
{
    return [(__bridge_transfer NSURL*)SecCopyURLForFileInKeychainDirectory((__bridge CFStringRef)@"Analytics/ckks_analytics.db") path];
}

+ (NSString*)databasePathForSOS
{
    return [(__bridge_transfer NSURL*)SecCopyURLForFileInKeychainDirectory((__bridge CFStringRef)@"Analytics/sos_analytics.db") path];
}

+ (NSString*)AppSupportPath
{
#if TARGET_OS_IOS
    return @"/var/mobile/Library/Application Support";
#else
    NSArray<NSString *>*paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, true);
    if ([paths count] < 1) {
        return nil;
    }
    return [NSString stringWithString: paths[0]];
#endif /* TARGET_OS_IOS */
}

+ (NSString*)databasePathForPCS
{
    NSString *appSup = [self AppSupportPath];
    if (!appSup) {
        return nil;
    }
    NSString *dbpath = [NSString stringWithFormat:@"%@/com.apple.ProtectedCloudStorage/PCSAnalytics.db", appSup];
    secnotice("supd", "PCS Database path (%@)", dbpath);
    return dbpath;
}

+ (NSString*)databasePathForLocal
{
    return [(__bridge_transfer NSURL*)SecCopyURLForFileInKeychainDirectory((__bridge CFStringRef)@"Analytics/localkeychain.db") path];
}

+ (NSString*)databasePathForTrustdHealth
{
#if TARGET_OS_IPHONE
    return [(__bridge_transfer NSURL*)SecCopyURLForFileInKeychainDirectory(CFSTR("Analytics/trustd_health_analytics.db")) path];
#else
    return [(__bridge_transfer NSURL*)SecCopyURLForFileInUserCacheDirectory(CFSTR("Analytics/trustd_health_analytics.db")) path];
#endif
}

+ (NSString*)databasePathForTrust
{
#if TARGET_OS_IPHONE
    return [(__bridge_transfer NSURL*)SecCopyURLForFileInKeychainDirectory(CFSTR("Analytics/trust_analytics.db")) path];
#else
    return [(__bridge_transfer NSURL*)SecCopyURLForFileInUserCacheDirectory(CFSTR("Analytics/trust_analytics.db")) path];
#endif
}

+ (NSString*)databasePathForTLS
{
#if TARGET_OS_IPHONE
    return [(__bridge_transfer NSURL*)SecCopyURLForFileInKeychainDirectory(CFSTR("Analytics/TLS_analytics.db")) path];
#else
    return [(__bridge_transfer NSURL*)SecCopyURLForFileInUserCacheDirectory(CFSTR("Analytics/TLS_analytics.db")) path];
#endif
}

+ (NSString*)databasePathForSignIn
{
    return [(__bridge_transfer NSURL*)SecCopyURLForFileInKeychainDirectory(CFSTR("Analytics/signin_metrics.db")) path];
}

@end

@interface supd ()
@property NSDictionary *topicsSamplingRates;
@end

@implementation supd
- (void)setupTopics
{
    NSDictionary* systemDefaultValues = [NSDictionary dictionaryWithContentsOfFile:[[NSBundle bundleWithPath:@"/System/Library/Frameworks/Security.framework"] pathForResource:@"SFAnalytics" ofType:@"plist"]];
    NSMutableArray <SFAnalyticsTopic*>* topics = [NSMutableArray array];
    for (NSString *topicKey in systemDefaultValues) {
        NSDictionary *topicSamplingRates = _topicsSamplingRates[topicKey];
        SFAnalyticsTopic *topic = [[SFAnalyticsTopic alloc] initWithDictionary:systemDefaultValues[topicKey] name:topicKey samplingRates:topicSamplingRates];
        [topics addObject:topic];
    }
    _analyticsTopics = [NSArray arrayWithArray:topics];
}

+ (void)instantiate {
    [supd instance];
}

+ (instancetype)instance {
#if TARGET_OS_SIMULATOR
    return nil;
#else
    if (!_supdInstance) {
        _supdInstance = [self new];
    }
    return _supdInstance;
#endif
}

// Use this for testing to get rid of any state
+ (void)removeInstance {
    _supdInstance = nil;
}


static NSString *SystemTrustStorePath = @"/System/Library/Security/Certificates.bundle";
static NSString *AnalyticsSamplingRatesFilename = @"AnalyticsSamplingRates";
static NSString *ContentVersionKey = @"MobileAssetContentVersion";
static NSString *AssetContextFilename = @"OTAPKIContext.plist";

static NSNumber *getSystemVersion(NSBundle *trustStoreBundle) {
    NSDictionary *systemVersionPlist = [NSDictionary dictionaryWithContentsOfURL:[trustStoreBundle URLForResource:@"AssetVersion"
                                                                                                    withExtension:@"plist"]];
    if (!systemVersionPlist || ![systemVersionPlist isKindOfClass:[NSDictionary class]]) {
        return nil;
    }
    NSNumber *systemVersion = systemVersionPlist[ContentVersionKey];
    if (systemVersion == nil || ![systemVersion isKindOfClass:[NSNumber class]]) {
        return nil;
    }
    return systemVersion;
}

static NSNumber *getAssetVersion(NSURL *directory) {
    NSDictionary *assetContextPlist = [NSDictionary dictionaryWithContentsOfURL:[directory URLByAppendingPathComponent:AssetContextFilename]];
    if (!assetContextPlist || ![assetContextPlist isKindOfClass:[NSDictionary class]]) {
        return nil;
    }
    NSNumber *assetVersion = assetContextPlist[ContentVersionKey];
    if (assetVersion == nil || ![assetVersion isKindOfClass:[NSNumber class]]) {
        return nil;
    }
    return assetVersion;
}

static bool ShouldInitializeWithAsset(NSBundle *trustStoreBundle, NSURL *directory) {
    NSNumber *systemVersion = getSystemVersion(trustStoreBundle);
    NSNumber *assetVersion = getAssetVersion(directory);

    if (assetVersion == nil || systemVersion == nil) {
        return false;
    }
    if ([assetVersion compare:systemVersion] == NSOrderedDescending) {
        return true;
    }
    return false;
}

- (void)setupSamplingRates {
#if TARGET_OS_SIMULATOR
    NSBundle *trustStoreBundle = [NSBundle bundleWithPath:[NSString stringWithFormat:@"%s%@", getenv("SIMULATOR_ROOT"), SystemTrustStorePath]];
#else
    NSBundle *trustStoreBundle = [NSBundle bundleWithPath:SystemTrustStorePath];
#endif

#if TARGET_OS_IPHONE
    NSURL *keychainsDirectory = CFBridgingRelease(SecCopyURLForFileInKeychainDirectory(nil));
#else
    NSURL *keychainsDirectory = [NSURL fileURLWithFileSystemRepresentation:"/Library/Keychains/" isDirectory:YES relativeToURL:nil];
#endif
    NSURL *directory = [keychainsDirectory URLByAppendingPathComponent:@"SupplementalsAssets/" isDirectory:YES];

    NSDictionary *analyticsSamplingRates = nil;
    if (ShouldInitializeWithAsset(trustStoreBundle, directory)) {
        /* Try to get the asset version of the sampling rates */
        NSURL *analyticsSamplingRateURL = [directory URLByAppendingPathComponent:[NSString stringWithFormat:@"%@.plist", AnalyticsSamplingRatesFilename]];
        analyticsSamplingRates = [NSDictionary dictionaryWithContentsOfURL:analyticsSamplingRateURL];
        secnotice("supd", "read sampling rates from SupplementalsAssets dir");
        if (!analyticsSamplingRates || ![analyticsSamplingRates isKindOfClass:[NSDictionary class]]) {
            analyticsSamplingRates = nil;
        }
    }
    if (!analyticsSamplingRates) {
        analyticsSamplingRates = [NSDictionary dictionaryWithContentsOfURL: [trustStoreBundle URLForResource:AnalyticsSamplingRatesFilename
                                                                                               withExtension:@"plist"]];
    }
    if (analyticsSamplingRates && [analyticsSamplingRates isKindOfClass:[NSDictionary class]]) {
        _topicsSamplingRates = analyticsSamplingRates[@"Topics"];
        if (!_topicsSamplingRates || ![analyticsSamplingRates isKindOfClass:[NSDictionary class]]) {
            _topicsSamplingRates = nil; // Something has gone terribly wrong, so we'll use the hardcoded defaults in this case
        }
    }
}

- (instancetype)initWithReporter:(SFAnalyticsReporter *)reporter
{
    if (self = [super init]) {
        [self setupSamplingRates];
        [self setupTopics];
        _reporter = reporter;

        xpc_activity_register("com.apple.securityuploadd.triggerupload", XPC_ACTIVITY_CHECK_IN, ^(xpc_activity_t activity) {
            xpc_activity_state_t activityState = xpc_activity_get_state(activity);
            secnotice("supd", "hit xpc activity trigger, state: %ld", activityState);
            if (activityState == XPC_ACTIVITY_STATE_RUN) {
                // Run our regularly scheduled scan
                [self performRegularlyScheduledUpload];
            }
        });
    }

    return self;
}

- (instancetype)init {
    SFAnalyticsReporter *reporter = [[SFAnalyticsReporter alloc] init];
    return [self initWithReporter:reporter];
}

- (void)sendNotificationForOncePerReportSamplers
{
    notify_post(SFAnalyticsFireSamplersNotification);
    [NSThread sleepForTimeInterval:3.0];
}

- (void)performRegularlyScheduledUpload {
    secnotice("upload", "Starting uploads in response to regular trigger");
    NSError *error = nil;
    if ([self uploadAnalyticsWithError:&error force:NO]) {
        secnotice("upload", "Regularly scheduled upload successful");
    } else {
        secerror("upload: Failed to complete regularly scheduled upload: %@", error);
    }
}

- (BOOL)uploadAnalyticsWithError:(NSError**)error force:(BOOL)force {
    [self sendNotificationForOncePerReportSamplers];
    
    BOOL result = NO;
    NSError* localError = nil;
    for (SFAnalyticsTopic *topic in _analyticsTopics) {
        @autoreleasepool { // The logging JSONs get quite large. Ensure they're deallocated between topics.
            __block NSURL* endpoint = [topic splunkUploadURL:force];   // has side effects!

            if (!endpoint) {
                secnotice("upload", "Skipping upload for %@ because no endpoint", [topic internalTopicName]);
                continue;
            }

            if ([topic disableUploads]) {
                secnotice("upload", "Aborting upload task for %@ because uploads are disabled", [topic internalTopicName]);
                continue;
            }

            NSMutableArray<SFAnalyticsClient*>* clients = [NSMutableArray new];
            NSData* json = [topic getLoggingJSON:false forUpload:YES participatingClients:&clients force:force error:&localError];
            if (json) {
                if ([topic isSampledUpload]) {
                    if (![self->_reporter saveReport:json fileName:[topic internalTopicName]]) {
                        secerror("upload: failed to write analytics data to log");
                    }
                    if ([topic postJSON:json toEndpoint:endpoint error:&localError]) {
                        secnotice("upload", "Successfully posted JSON for %@", [topic internalTopicName]);
                        result = YES;
                        [topic updateUploadDateForClients:clients clearData:YES];
                    } else {
                        secerror("upload: Failed to post JSON for %@", [topic internalTopicName]);
                    }
                } else {
                    /* If we didn't sample this report, update date to prevent trying to upload again sooner
                     * than we should. Clear data so that per-day calculations remain consistent. */
                    secnotice("upload", "skipping unsampled upload for %@ and clearing data", [topic internalTopicName]);
                    [topic updateUploadDateForClients:clients clearData:YES];
                }
            } else {
                secerror("upload: failed to get logging JSON");
            }
        }
        if (error && localError) {
            *error = localError;
        }
    }
    return result;
}

- (NSString*)sysdiagnoseStringForEventRecord:(NSDictionary*)eventRecord
{
    NSMutableDictionary* mutableEventRecord = eventRecord.mutableCopy;
    [mutableEventRecord removeObjectForKey:SFAnalyticsSplunkTopic];

    NSDate* eventDate = [NSDate dateWithTimeIntervalSince1970:[[eventRecord valueForKey:SFAnalyticsEventTime] doubleValue] / 1000];
    [mutableEventRecord removeObjectForKey:SFAnalyticsEventTime];

    NSString* eventName = eventRecord[SFAnalyticsEventType];
    [mutableEventRecord removeObjectForKey:SFAnalyticsEventType];

    SFAnalyticsEventClass eventClass = [[eventRecord valueForKey:SFAnalyticsEventClassKey] integerValue];
    NSString* eventClassString = [self stringForEventClass:eventClass];
    [mutableEventRecord removeObjectForKey:SFAnalyticsEventClassKey];

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

- (NSString*)getSysdiagnoseDump
{
    NSMutableString* sysdiagnose = [[NSMutableString alloc] init];

    for (SFAnalyticsTopic* topic in _analyticsTopics) {
        for (SFAnalyticsClient* client in topic.topicClients) {
            [sysdiagnose appendString:[NSString stringWithFormat:@"Client: %@\n", client.name]];
            SFAnalyticsSQLiteStore* store = [SFAnalyticsSQLiteStore storeWithPath:client.storePath schema:SFAnalyticsTableSchema];
            NSArray* allEvents = store.allEvents;
            for (NSDictionary* eventRecord in allEvents) {
                [sysdiagnose appendFormat:@"%@\n", [self sysdiagnoseStringForEventRecord:eventRecord]];
            }
            if (allEvents.count == 0) {
                [sysdiagnose appendString:@"No data to report for this client\n"];
            }
        }
    }
    return sysdiagnose;
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

// MARK: XPC Procotol Handlers

- (void)getSysdiagnoseDumpWithReply:(void (^)(NSString*))reply {
    reply([self getSysdiagnoseDump]);
}

- (void)getLoggingJSON:(bool)pretty topic:(NSString *)topicName reply:(void (^)(NSData*, NSError*))reply {
    secnotice("rpcGetLoggingJSON", "Building a JSON blob resembling the one we would have uploaded");
    NSError* error = nil;
    [self sendNotificationForOncePerReportSamplers];
    NSData* json = nil;
    for (SFAnalyticsTopic* topic in self->_analyticsTopics) {
        if ([topic.internalTopicName isEqualToString:topicName]) {
            json = [topic getLoggingJSON:pretty forUpload:NO participatingClients:nil force:!runningTests error:&error];
        }
    }
    if (!json) {
        secerror("Unable to obtain JSON: %@", error);
    }
    reply(json, error);
}

- (void)forceUploadWithReply:(void (^)(BOOL, NSError*))reply {
    secnotice("upload", "Performing upload in response to rpc message");
    NSError* error = nil;
    BOOL result = [self uploadAnalyticsWithError:&error force:YES];
    secnotice("upload", "Result of manually triggered upload: %@, error: %@", result ? @"success" : @"failure", error);
    reply(result, error);
}

@end
