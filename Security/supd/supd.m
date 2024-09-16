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
#import <Foundation/NSXPCConnection_Private.h>

#import "SFAnalyticsDefines.h"
#import "SFAnalyticsSQLiteStore.h"
#import <Security/SFAnalytics.h>
#import <OSAnalytics/OSAnalytics.h>

#include <utilities/SecFileLocations.h>
#import "utilities/debugging.h"
#import <os/lock.h>
#import <os/lock_private.h>
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
#include <membership.h>
#endif

#if TARGET_OS_OSX
#import <CrashReporterSupport/CrashReporterSupportPrivate.h>
#elif !TARGET_OS_SIMULATOR
#import <CrashReporterSupport/CrashReporterSupport.h>
#endif

#import <Accounts/Accounts.h>
#import <Accounts/ACAccountStore_Private.h>
#import <Accounts/ACAccountType_Private.h>
#import <Accounts/ACAccountStore.h>
#import <AppleAccount/AppleAccount.h>
#import <AppleAccount/ACAccount+AppleAccount.h>
#import <AppleAccount/ACAccountStore+AppleAccount.h>

#import <OSAnalytics/OSAnalytics.h>

#import "utilities/simulatecrash_assert.h"
#import "trust/trustd/trustdFileHelper/trustdFileHelper.h"

NSString* const SFAnalyticsSplunkTopic = @"topic";
NSString* const SFAnalyticsClientId = @"clientId";
NSString* const SFAnalyticsInternal = @"internal";

NSString* const SFAnalyticsMetricsBase = @"metricsBase";
NSString* const SFAnalyticsDeviceID = @"ckdeviceID";
NSString* const SFAnalyticsAccountID = @"sfaAccountID";
NSString* const SFAnalyticsAltDSID = @"altDSID";
NSString* const SFAnalyticsIsAppleUser = @"isAppleUser";

NSString* const SFAnalyticsEventCorrelationID = @"eventLinkID";

NSString* const SFAnalyticsSecondsCustomerKey = @"SecondsBetweenUploadsCustomer";
NSString* const SFAnalyticsSecondsInternalKey = @"SecondsBetweenUploadsInternal";
NSString* const SFAnalyticsSecondsSeedKey = @"SecondsBetweenUploadsSeed";
NSString* const SFAnalyticsMaxEventsKey = @"NumberOfEvents";
NSString* const SFAnalyticsDevicePercentageCustomerKey = @"DevicePercentageCustomer";
NSString* const SFAnalyticsDevicePercentageInternalKey = @"DevicePercentageInternal";
NSString* const SFAnalyticsDevicePercentageSeedKey = @"DevicePercentageSeed";

NSString* const SupdErrorDomain = @"com.apple.security.supd";

#define SFANALYTICS_SPLUNK_DEV 0
#define OS_CRASH_TRACER_LOG_BUG_TYPE "226"

#if SFANALYTICS_SPLUNK_DEV
NSUInteger const secondsBetweenUploadsCustomer = 10;
NSUInteger const secondsBetweenUploadsInternal = 10;
NSUInteger const secondsBetweenUploadsSeed = 10;
#else // SFANALYTICS_SPLUNK_DEV
NSUInteger const secondsBetweenUploadsCustomer = (3 * (60 * 60 * 24));
NSUInteger const secondsBetweenUploadsInternal = (60 * 60 * 24);
NSUInteger const secondsBetweenUploadsSeed = (60 * 60 * 24);
#endif // SFANALYTICS_SPLUNK_DEV

@implementation SFAnalyticsReporter
- (BOOL)saveReport:(NSData *)reportData fileName:(NSString *)fileName
{
    BOOL writtenToLog = NO;
#if !TARGET_OS_SIMULATOR
#if TARGET_OS_OSX
    NSDictionary *optionsDictionary = @{ (__bridge NSString *)kCRProblemReportSubmissionPolicyKey: (__bridge NSString *)kCRSubmissionPolicyAlternate };
#else // !TARGET_OS_OSX
    NSDictionary *optionsDictionary = nil; // The keys above are not defined or required on iOS.
#endif // !TARGET_OS_OSX


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
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
        dataCollectionEnabled = DiagnosticLogSubmissionEnabled();
#elif TARGET_OS_OSX
        dataCollectionEnabled = CRIsAutoSubmitEnabled();
#endif
    });
    return dataCollectionEnabled;
}

static NSString *
accountAltDSID(void)
{
    ACAccountStore *accountStore = [ACAccountStore defaultStore];
    ACAccount *primaryAccount = [accountStore aa_primaryAppleAccount];
    if (primaryAccount == nil) {
        return nil;
    }
    return [primaryAccount aa_altDSID];
}

static NSString *const kAnalyticsiCloudIdMSKey = @"com.apple.idms.config.privacy.icloud.data";

static NSDictionary *
_getiCloudConfigurationInfoWithError(NSError **outError)
{
    __block NSDictionary *outConfigurationInfo = nil;
    __block NSError *localError = nil;

    NSString *altDSID = accountAltDSID();
    if (altDSID != nil) {
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

static BOOL
_isiCloudAnalyticsEnabled(void)
{
    // This flag is only set during tests.
    if (iCloudAnalyticsOverride) {
        return iCloudAnalyticsEnabled;
    }

    static bool cachedAllowsICloudAnalytics = false;

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

    return cachedAllowsICloudAnalytics;
}

/// Returns one of the values in the dictionary, or `nil` if the dictionary is
/// empty. Like `-[NSSet anyObject]`, but for dictionaries.
static inline id _Nullable
_anyValueFromDictionary(NSDictionary *dictionary)
{
    __block id value;
    [dictionary enumerateKeysAndObjectsUsingBlock:^(id __unused anyKey, id anyValue, BOOL *stop) {
        // Just grab the first value we see...
        value = anyValue;
        *stop = YES;
    }];
    return value;
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

@interface SFAnalyticsClient ()

- (instancetype)initWithStore:(SFAnalyticsSQLiteStore *)store
                        queue:(dispatch_queue_t)queue
                         name:(NSString*)name
       requireDeviceAnalytics:(BOOL)requireDeviceAnalytics
       requireiCloudAnalytics:(BOOL)requireiCloudAnalytics NS_DESIGNATED_INITIALIZER;

@property (readonly, nonatomic, nullable) SFAnalyticsSQLiteStore *store;
@property (readonly, nonatomic) dispatch_queue_t queue;

@end

@implementation SFAnalyticsClient {
    NSString* _name;
    BOOL _requireDeviceAnalytics;
    BOOL _requireiCloudAnalytics;
}

static os_unfair_lock sharedClientsLock = OS_UNFAIR_LOCK_INIT;
static NSMutableDictionary<NSString *, NSMutableDictionary<NSString *, SFAnalyticsClient *> *> *namedSharedClientsByStorePath;

@synthesize name = _name;

+ (void)clearSFAnalyticsClientGlobalCache {
    os_unfair_lock_lock_scoped_guard(lock, &sharedClientsLock);
    namedSharedClientsByStorePath = nil;
}

- (NSString *)storePath
{
    return _store.path;
}

+ (SFAnalyticsClient *)getSharedClientNamed:(NSString *)name
                      orCreateWithStorePath:(NSString *)storePath
                     requireDeviceAnalytics:(BOOL)requireDeviceAnalytics
                     requireiCloudAnalytics:(BOOL)requireiCloudAnalytics
{
    // The shared clients dictionary is:
    //   (1) Global and protected by an unfair lock.
    //   (2) Double-keyed by the store path and name. A path can have multiple
    //       clients with different names, but all clients for that path use the
    //       same underlying store.
    os_unfair_lock_lock_scoped_guard(lock, &sharedClientsLock);

    if (!namedSharedClientsByStorePath) {
        namedSharedClientsByStorePath = [NSMutableDictionary dictionary];
    }

    NSString *standardizedStorePath = storePath.stringByStandardizingPath;

    NSMutableDictionary<NSString *, SFAnalyticsClient *> *sharedClientsByName = namedSharedClientsByStorePath[standardizedStorePath];
    if (!sharedClientsByName) {
        // No client for this path yet, let's make one.
        SFAnalyticsClient *sharedClient = [[SFAnalyticsClient alloc] initWithStorePath:standardizedStorePath
                                                                                  name:name
                                                                requireDeviceAnalytics:requireDeviceAnalytics
                                                                requireiCloudAnalytics:requireiCloudAnalytics];
        if (sharedClient.storePath != nil && name != nil) {
            namedSharedClientsByStorePath[sharedClient.storePath] = [NSMutableDictionary dictionaryWithObject:sharedClient forKey:name];
        } else {
            if (sharedClient.storePath == nil) {
                secerror("SFAnalyticsClient: sharedClient.storePath is unexpectedly nil! Not adding to namedSharedClientsByStorePath");
            }
            if (name == nil) {
                secerror("SFAnalyticsClient: name is unexpectedly nil! Not adding to namedSharedClientsByStorePath");
            }
        }
        if (sharedClient == nil) {
            secerror("SFAnalyticsClient: sharedClient is unexpectedly nil!");
        }
        return sharedClient;
    }

    // If we already have clients for this store path, make sure we have one
    // with the same name.
    SFAnalyticsClient *existingSharedClient = sharedClientsByName[name];
    if (existingSharedClient) {
        return existingSharedClient;
    }

    // If not, we might be reading different analytics from the same store
    // (e.g., trust and root trust; networking and root networking). Create a
    // new client, but with the same underlying store and queue as an existing
    // one. This avoids opening multiple connections to the SQLite database, and
    // ensures that the single connection is never shared between threads. It
    // doesn't matter which existing shared client we use; all clients for the
    // same path will have the same store.
    existingSharedClient = _anyValueFromDictionary(sharedClientsByName);
    NSAssert(existingSharedClient != NULL, @"Expected at least one named existing client");
    SFAnalyticsClient *newSharedClient = [[SFAnalyticsClient alloc] initFromExistingClient:existingSharedClient
                                                                                      name:name
                                                                    requireDeviceAnalytics:requireDeviceAnalytics
                                                                    requireiCloudAnalytics:requireiCloudAnalytics];
    sharedClientsByName[name] = newSharedClient;
    return newSharedClient;
}

- (instancetype)initWithStorePath:(NSString*)path
                             name:(NSString*)name
           requireDeviceAnalytics:(BOOL)requireDeviceAnalytics
           requireiCloudAnalytics:(BOOL)requireiCloudAnalytics
{
    SFAnalyticsSQLiteStore *store = [[SFAnalyticsSQLiteStore alloc] initWithPath:path schema:SFAnalyticsTableSchema];

    NSString* queueName = [NSString stringWithFormat: @"SFAnalyticsClient queue-%@", name];
    dispatch_queue_t queue = dispatch_queue_create([queueName UTF8String], DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);

    return [self initWithStore:store
                         queue:queue
                          name:name
        requireDeviceAnalytics:requireDeviceAnalytics
        requireiCloudAnalytics:requireiCloudAnalytics];
}

- (instancetype)initFromExistingClient:(SFAnalyticsClient *)client
                                  name:(NSString *)name
                requireDeviceAnalytics:(BOOL)requireDeviceAnalytics
                requireiCloudAnalytics:(BOOL)requireiCloudAnalytics
{
    NSString* queueName = [NSString stringWithFormat: @"SFAnalyticsClient queue-%@", name];
    dispatch_queue_t queue = dispatch_queue_create_with_target([queueName UTF8String], DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL, client.queue);

    return [self initWithStore:client.store
                         queue:queue
                          name:name
        requireDeviceAnalytics:requireDeviceAnalytics
        requireiCloudAnalytics:requireiCloudAnalytics];
}

- (instancetype)initWithStore:(SFAnalyticsSQLiteStore *)store
                        queue:(dispatch_queue_t)queue
                         name:(NSString *)name
       requireDeviceAnalytics:(BOOL)requireDeviceAnalytics
       requireiCloudAnalytics:(BOOL)requireiCloudAnalytics
{
    if (self = [super init]) {
        _store = store;
        _queue = queue;
        _name = name;
        _requireDeviceAnalytics = requireDeviceAnalytics;
        _requireiCloudAnalytics = requireiCloudAnalytics;
    }
    return self;
}

- (void)withStore:(void (^ NS_NOESCAPE)(SFAnalyticsSQLiteStore *store))block
{
    dispatch_sync(self.queue, ^{
        NSError *error;
        BOOL didStoreOpen = [self.store openWithError:&error];
        if (!didStoreOpen && !(error && error.code == SQLITE_AUTH)) {
            // If opening the store fails, we'll log an error, but still call
            // the block. Attempting to use the store will likely fail, but we
            // don't want to leave the caller hanging.
            secerror("SFAnalytics: could not open db at init, will try again later. Error: %@", error);
        }
        block(self.store);
        if (didStoreOpen) {
            [self.store close];
        }
    });
}
@end

@interface SFAnalyticsTopic ()
@property NSURL* _splunkUploadURL;

@property BOOL allowInsecureSplunkCert;
@property BOOL ignoreServersMessagesTellingUsToGoAway;
@property BOOL disableUploads;
@property BOOL disableClientId;
@property BOOL terseMetrics;

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
        [clients addObject:[SFAnalyticsClient getSharedClientNamed:@"ckks"
                                             orCreateWithStorePath:[self.class databasePathForCKKS]
                                            requireDeviceAnalytics:NO
                                            requireiCloudAnalytics:YES]];
        [clients addObject:[SFAnalyticsClient getSharedClientNamed:@"sos"
                                             orCreateWithStorePath:[self.class databasePathForSOS]
                                            requireDeviceAnalytics:NO
                                            requireiCloudAnalytics:YES]];
        [clients addObject:[SFAnalyticsClient getSharedClientNamed:@"pcs"
                                             orCreateWithStorePath:[self.class databasePathForPCS]
                                            requireDeviceAnalytics:NO
                                            requireiCloudAnalytics:YES]];
        [clients addObject:[SFAnalyticsClient getSharedClientNamed:@"local"
                                             orCreateWithStorePath:[self.class databasePathForLocal]
                                            requireDeviceAnalytics:YES
                                            requireiCloudAnalytics:NO]];
    } else if ([topicName isEqualToString:SFAnalyticsTopicCloudServices]) {
        [clients addObject:[SFAnalyticsClient getSharedClientNamed:@"CloudServices"
                                             orCreateWithStorePath:[self.class databasePathForCloudServices]
                                            requireDeviceAnalytics:YES
                                            requireiCloudAnalytics:NO]];
    } else if ([topicName isEqualToString:SFAnalyticsTopicTrust]) {
        [clients addObject:[SFAnalyticsClient getSharedClientNamed:@"trust"
                                             orCreateWithStorePath:[self.class databasePathForTrust]
                                            requireDeviceAnalytics:YES
                                            requireiCloudAnalytics:NO]];
#if TARGET_OS_OSX
        [clients addObject:[SFAnalyticsClient getSharedClientNamed:@"rootTrust"
                                             orCreateWithStorePath:[self.class databasePathForRootTrust]
                                            requireDeviceAnalytics:YES
                                            requireiCloudAnalytics:NO]];
#endif
    } else if ([topicName isEqualToString:SFAnalyticsTopicNetworking]) {
        [clients addObject:[SFAnalyticsClient getSharedClientNamed:@"networking"
                                             orCreateWithStorePath:[self.class databasePathForNetworking]
                                            requireDeviceAnalytics:YES
                                            requireiCloudAnalytics:NO]];
#if TARGET_OS_OSX
        [clients addObject:[SFAnalyticsClient getSharedClientNamed:@"rootNetworking"
                                             orCreateWithStorePath:[self.class databasePathForRootNetworking]
                                            requireDeviceAnalytics:YES
                                            requireiCloudAnalytics:NO]];
#endif
    } else if ([topicName isEqualToString:SFAnalyticsTopicTransparency]) {
        self.terseMetrics = YES;
        [clients addObject:[SFAnalyticsClient getSharedClientNamed:@"transparency"
                                             orCreateWithStorePath:[self.class databasePathForTransparency]
                                            requireDeviceAnalytics:NO
                                            requireiCloudAnalytics:YES]];
    } else if ([topicName isEqualToString:SFAnalyticsTopicSWTransparency]) {
        self.terseMetrics = YES;
        [clients addObject:[SFAnalyticsClient getSharedClientNamed:@"swtransparency"
                                             orCreateWithStorePath:[self.class databasePathForSWTransparency]
                                            requireDeviceAnalytics:NO
                                            requireiCloudAnalytics:YES]];
    }


    _topicClients = clients;
}

- (instancetype)initWithDictionary:(NSDictionary *)dictionary name:(NSString *)topicName samplingRates:(NSDictionary *)rates {
    if (self = [super init]) {
        _terseMetrics = NO;
        _internalTopicName = topicName;
        [self setupClientsForTopic:topicName];
        _splunkTopicName = dictionary[@"splunk_topic"];
        __splunkUploadURL = [NSURL URLWithString:dictionary[@"splunk_uploadURL"]];
        _splunkBagURL = [NSURL URLWithString:dictionary[@"splunk_bagURL"]];
        _allowInsecureSplunkCert = [[dictionary valueForKey:@"splunk_allowInsecureCertificate"] boolValue];
        _uploadSizeLimit = [[dictionary valueForKey:@"uploadSizeLimit"] unsignedIntegerValue];

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

        NSInteger userDefaultsUploadSizeLimit = [defaults integerForKey:@"uploadSizeLimit"];
        if (userDefaultsUploadSizeLimit > 0) {
            _uploadSizeLimit = userDefaultsUploadSizeLimit;
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
#if RC_SEED_BUILD
            NSNumber *secondsNum = internal ? rates[SFAnalyticsSecondsInternalKey] : rates[SFAnalyticsSecondsSeedKey];
            NSNumber *percentageNum = internal ? rates[SFAnalyticsDevicePercentageInternalKey] : rates[SFAnalyticsDevicePercentageSeedKey];
#else
            NSNumber *secondsNum = internal ? rates[SFAnalyticsSecondsInternalKey] : rates[SFAnalyticsSecondsCustomerKey];
            NSNumber *percentageNum = internal ? rates[SFAnalyticsDevicePercentageInternalKey] : rates[SFAnalyticsDevicePercentageCustomerKey];
#endif
            _secondsBetweenUploads = [secondsNum integerValue];
            _maxEventsToReport = [rates[SFAnalyticsMaxEventsKey] unsignedIntegerValue];
            _devicePercentage = [percentageNum floatValue];
        } else {
#if RC_SEED_BUILD
            _secondsBetweenUploads = internal ? secondsBetweenUploadsInternal : secondsBetweenUploadsSeed;
#else
            _secondsBetweenUploads = internal ? secondsBetweenUploadsInternal : secondsBetweenUploadsCustomer;
#endif
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

- (NSURLSession*) getSession
{
    /*
     * Create the NSURLSession
     *  We use the ephemeral session config because we don't need cookies or cache
     */
    NSURLSessionConfiguration *configuration = [NSURLSessionConfiguration ephemeralSessionConfiguration];

    configuration.HTTPAdditionalHeaders = @{ @"User-Agent" : [NSString stringWithFormat:@"securityd/%s", SECURITY_BUILD_VERSION]};

    NSURLSession* postSession = [NSURLSession sessionWithConfiguration:configuration
                                                              delegate:self
                                                         delegateQueue:nil];

    return postSession;
}

- (BOOL)postJSON:(NSData*)json toEndpoint:(NSURL*)endpoint postSession:(NSURLSession*)postSession error:(NSError**)error
{
    if (!endpoint) {
        if (error) {
            NSString *description = [NSString stringWithFormat:@"No endpoint for %@", _internalTopicName];
            *error = [NSError errorWithDomain:@"SupdUploadErrorDomain"
                                         code:-10
                                     userInfo:@{NSLocalizedDescriptionKey : description}];
        }
        return NO;
    }
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

- (BOOL)prepareEventForUpload:(NSMutableDictionary*)event
                   linkedUUID:(NSUUID *)linkedUUID {
    if ([self eventIsBlacklisted:event]) {
        return NO;
    }

    [self removeBlacklistedFieldsFromEvent:event];
    [self addRequiredFieldsToEvent:event];
    if (_disableClientId) {
        event[SFAnalyticsClientId] = @0;
    }
    event[SFAnalyticsSplunkTopic] = self->_splunkTopicName ?: [NSNull null];
    if (linkedUUID) {
        event[SFAnalyticsEventCorrelationID] = [linkedUUID UUIDString];
    }
    return YES;
}

- (void)addFailures:(NSMutableArray<NSArray*>*)failures toUploadRecords:(NSMutableArray*)records threshold:(NSUInteger)threshold linkedUUID:(NSUUID *)linkedUUID
{
    // The first 0 through 'threshold' items are getting uploaded in any case (which might be 0 for lower priority data)

    for (NSArray* client in failures) {
        [client enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
            NSMutableDictionary* event = (NSMutableDictionary*)obj;
            if (idx >= threshold) {
                *stop = YES;
                return;
            }
            @autoreleasepool {
                if ([self prepareEventForUpload:event linkedUUID:linkedUUID]) {
                    if ([NSJSONSerialization isValidJSONObject:event]) {
                        [records addObject:event];
                    } else {
                        secerror("supd: Replacing event with errorEvent because invalid JSON: %@", event);
                        NSString* originalType = event[SFAnalyticsEventType];
                        NSDictionary* errorEvent = @{ SFAnalyticsEventType : SFAnalyticsEventTypeErrorEvent,
                                                      SFAnalyticsEventErrorDestription : [NSString stringWithFormat:@"JSON:%@", originalType]};
                        [records addObject:errorEvent];
                    }
                }
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
                    if ([self prepareEventForUpload:obj linkedUUID:linkedUUID]) {
                        [records addObject:obj];
                    }
                }];
            }
        }
    }
}

- (NSDictionary*)sampleStatisticsForSamples:(NSArray*)samples withName:(NSString*)name
{
    NSMutableDictionary* statistics = [NSMutableDictionary dictionary];
    NSUInteger count = samples.count;
    NSArray* sortedSamples = [samples sortedArrayUsingSelector:@selector(compare:)];
    NSArray* samplesAsExpressionArray = @[[NSExpression expressionForConstantValue:sortedSamples]];

    if (count == 1) {
        statistics[name] = samples[0];
    } else {
        statistics[[NSString stringWithFormat:@"%@-avg", name]] = [[NSExpression expressionForFunction:@"average:" arguments:samplesAsExpressionArray] expressionValueWithObject:nil context:nil];

        if (!self.terseMetrics) {
            // NSExpression takes population standard deviation. Our data is a sample of whatever we sampled over time,
            // but the difference between the two is fairly minor (divide by N before taking sqrt versus divide by N-1).
            statistics[[NSString stringWithFormat:@"%@-dev", name]] = [[NSExpression expressionForFunction:@"stddev:" arguments:samplesAsExpressionArray] expressionValueWithObject:nil context:nil];

            statistics[[NSString stringWithFormat:@"%@-min", name]] = [[NSExpression expressionForFunction:@"min:" arguments:samplesAsExpressionArray] expressionValueWithObject:nil context:nil];
            statistics[[NSString stringWithFormat:@"%@-max", name]] = [[NSExpression expressionForFunction:@"max:" arguments:samplesAsExpressionArray] expressionValueWithObject:nil context:nil];
            statistics[[NSString stringWithFormat:@"%@-med", name]] = [[NSExpression expressionForFunction:@"median:" arguments:samplesAsExpressionArray] expressionValueWithObject:nil context:nil];
        }
    }

    if (count > 3 && !self.terseMetrics) {
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

- (NSMutableDictionary*)healthSummaryWithName:(SFAnalyticsClient*)client
                                        store:(SFAnalyticsSQLiteStore*)store
                                         uuid:(NSUUID *)uuid
                                    timestamp:(NSNumber*)timestamp
                               lastUploadTime:(NSNumber*)lastUploadTime
{
    dispatch_assert_queue(client.queue);
    NSString *name = client.name;
    __block NSMutableDictionary* summary = [NSMutableDictionary dictionary];

    // Add some events of our own before pulling in data
    summary[SFAnalyticsEventType] = [NSString stringWithFormat:@"%@HealthSummary", name];
    if ([self eventIsBlacklisted:summary]) {
        return nil;
    }
    summary[SFAnalyticsEventTime] = timestamp;    // Splunk wants milliseconds
    [SFAnalytics addOSVersionToEvent:summary];
    if (lastUploadTime != nil) {
        summary[SFAnalyticsAttributeLastUploadTime] = lastUploadTime;
    }

    // Process counters
    NSDictionary* successCounts = store.summaryCounts;
    __block NSInteger totalSuccessCount = 0;
    __block NSInteger totalHardFailureCount = 0;
    __block NSInteger totalSoftFailureCount = 0;
    if (self.terseMetrics) {
        summary[@"T"] = @1;
    }
    [successCounts enumerateKeysAndObjectsUsingBlock:^(NSString* _Nonnull eventType, NSDictionary* _Nonnull counts, BOOL* _Nonnull stop) {
        NSInteger success = [counts[SFAnalyticsColumnSuccessCount] integerValue];
        NSInteger hardfail = [counts[SFAnalyticsColumnHardFailureCount] integerValue];
        NSInteger softfail = [counts[SFAnalyticsColumnSoftFailureCount] integerValue];
        if (self.terseMetrics) {
            if ((hardfail == 0 && softfail == 0) || success != 0) {
                summary[[NSString stringWithFormat:@"%@-s", eventType]] = @(success);
            }
            if (hardfail) {
                summary[[NSString stringWithFormat:@"%@-h", eventType]] = @(hardfail);
            }
            if (softfail) {
                summary[[NSString stringWithFormat:@"%@-f", eventType]] = @(softfail);
            }
        } else {
            summary[[NSString stringWithFormat:@"%@-success", eventType]] = @(success);
            summary[[NSString stringWithFormat:@"%@-hardfail", eventType]] = @(hardfail);
            summary[[NSString stringWithFormat:@"%@-softfail", eventType]] = @(softfail);
        }
        totalSuccessCount += success;
        totalHardFailureCount += hardfail;
        totalSoftFailureCount += softfail;
    }];

    summary[SFAnalyticsColumnSuccessCount] = @(totalSuccessCount);
    summary[SFAnalyticsColumnHardFailureCount] = @(totalHardFailureCount);
    summary[SFAnalyticsColumnSoftFailureCount] = @(totalSoftFailureCount);
    if (os_variant_has_internal_diagnostics("com.apple.security")) {
        summary[SFAnalyticsInternal] = @YES;
    }

    NSString *storeAccountIdentifier = [store metricsAccountID];
    if (storeAccountIdentifier) {
        summary[SFAnalyticsAccountID] = storeAccountIdentifier;
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
        NSDictionary* event = [self sampleStatisticsForSamples:obj withName:key];
        [summary addEntriesFromDictionary:event];
    }];

    // Should always return yes because we already checked for event blacklisting specifically (unless summary itself is blacklisted)
    if (![self prepareEventForUpload:summary linkedUUID:uuid]) {
        secwarning("supd: health summary for %@ blacklisted", name);
        return nil;
    }

    // Seems unlikely because we only insert strings, samplers only take NSNumbers and frankly, sampleStatisticsForSamples probably would have crashed
    if (![NSJSONSerialization isValidJSONObject:summary]) {
        secerror("json: health summary for client %@ is invalid JSON: %@", name, summary);
        return [@{ SFAnalyticsEventType : SFAnalyticsEventTypeErrorEvent,
                   SFAnalyticsEventErrorDestription : [NSString stringWithFormat:@"JSON:%@HealthSummary", name]} mutableCopy];
    }

    return summary;
}

- (void)updateUploadDateForClients:(NSArray<SFAnalyticsClient*>*)clients date:(NSDate *)date clearData:(BOOL)clearData
{
    for (SFAnalyticsClient* client in clients) {
        [client withStore:^(SFAnalyticsSQLiteStore *store) {
            secnotice("postprocess", "Setting upload date (%@) for client: %@", date, client.name);
            store.uploadDate = date;
            if (clearData) {
                secnotice("postprocess", "Clearing collected data for client: %@", client.name);
                [store clearAllData];
            }
        }];
    }
}

- (size_t)serializedEventSize:(NSObject *)event
                        error:(NSError**)error
{
    NSError *tmpError = nil;
    do {
        @autoreleasepool {
            if (![NSJSONSerialization isValidJSONObject:event]) {
                secnotice("serializedEventSize", "invalid JSON object");
                    tmpError = [NSError errorWithDomain:SupdErrorDomain code:SupdInvalidJSONError
                                             userInfo:@{NSLocalizedDescriptionKey : [NSString localizedStringWithFormat:@"Event is not valid JSON: %@", event]}];
                break;
            }
            
            NSData *json = [NSJSONSerialization dataWithJSONObject:event
                                                           options:0
                                                             error:&tmpError];
            if (json) {
                return [json length];
            } else {
                secnotice("serializedEventSize", "failed to serialize event: %@", tmpError);
                break;
             }
        }
    } while (0);
    if (error != nil) {
        *error = tmpError;
    }
    return 0;
}

- (NSArray<NSArray *> *)chunkFailureSet:(size_t)sizeCapacity
                                 events:(NSArray<NSDictionary *> *)events
                                  error:(NSError **)error
{
    const size_t postBodyLimit = 1000; // 1000 events in a single upload
    size_t currentSize = 0;
    size_t currentEventCount = 0;

    NSMutableArray<NSArray<NSDictionary*>*>*eventChunks = [NSMutableArray array];
    NSMutableArray<NSDictionary *> *currentEventChunk = [NSMutableArray array];

    for (NSDictionary *event in events) {
        NSError *localError = nil;
        size_t eventSize = [self serializedEventSize:event error:&localError];
        if (localError != nil) {
            if (error) {
                *error = localError;
            }
            secemergency("Unable to serialize event JSON: %@", [localError localizedDescription]);
            return nil;
        }

        BOOL countLessThanLimit = currentEventCount < postBodyLimit;
        BOOL sizeLessThanCapacity = (currentSize + eventSize) <= sizeCapacity;
        if (!countLessThanLimit || !sizeLessThanCapacity) {
            if (currentEventChunk.count > 0) {
                [eventChunks addObject:currentEventChunk];
                currentEventChunk = [NSMutableArray array];
            }
            currentEventCount = 0;
            currentSize = 0;
        }

        [currentEventChunk addObject:event];
        currentEventCount++;
        currentSize += eventSize;
    }

    if ([currentEventChunk count] > 0) {
        [eventChunks addObject:currentEventChunk];
    }

    return eventChunks;
}

- (NSDictionary *)createEventDictionary:(NSArray<NSDictionary *> *)events
                              timestamp:(NSNumber *)timestamp
                                  error:(NSError **)error
{
    NSError *tmpError = nil;
    @autoreleasepool {
        NSDictionary *eventDictionary = @{
            SFAnalyticsPostTime : timestamp,
            @"events" : events,
        };
        if (![NSJSONSerialization isValidJSONObject:eventDictionary]) {
            secemergency("json: final dictionary invalid JSON.");
            tmpError = [NSError errorWithDomain:SupdErrorDomain code:SupdInvalidJSONError
                                       userInfo:@{NSLocalizedDescriptionKey : [NSString localizedStringWithFormat:@"Final dictionary for upload is invalid JSON: %@", eventDictionary]}];
        } else {
            return eventDictionary;
        }
    }
    if (error != nil) {
        *error = tmpError;
    }
    return nil;
}


- (NSArray<NSDictionary *> *)createChunkedLoggingJSON:(NSArray<NSDictionary *> *)healthSummaries
                                             failures:(NSArray<NSDictionary *> *)failures
                                                error:(NSError **)error
{
    NSMutableArray<NSDictionary *> *jsonResults = [NSMutableArray array];
    NSNumber *now = @([[NSDate date] timeIntervalSince1970] * 1000);

    NSArray<NSArray *>* chunkedEvents;

    chunkedEvents = [self chunkFailureSet:self.uploadSizeLimit events:healthSummaries error:error];
    for (NSArray<NSDictionary *> *healthChunk in chunkedEvents) {
        NSDictionary *events = [self createEventDictionary:healthChunk timestamp:now error:error];
        if (events) {
            [jsonResults addObject:events];
        }
    }
    
    chunkedEvents = [self chunkFailureSet:self.uploadSizeLimit events:failures error:error];
    for (NSArray<NSDictionary *> *failureChunk in chunkedEvents) {
        NSDictionary *events = [self createEventDictionary:failureChunk timestamp:now error:error];
        if (events) {
            [jsonResults addObject:events];
        }
    }
    return jsonResults;
}

-(NSString *)dataAnalyticsSetting:(NSString *)key {
#if TARGET_OS_OSX
#define PREFS_USER kCFPreferencesAnyUser
#else
#define PREFS_USER CFSTR("mobile")
#endif
    NSString *setting = CFBridgingRelease(CFPreferencesCopyValue((__bridge CFStringRef)key,
                                                                 CFSTR("com.apple.da"),
                                                                 PREFS_USER,
                                                                 kCFPreferencesAnyHost));
    if (![setting isKindOfClass:[NSString class]]) {
        return nil;
    }
    return setting;
}


- (NSDictionary *)carryStatus {
    NSDictionary *carryStatus;

    if (os_variant_has_internal_diagnostics("com.apple.security")) {
        NSMutableDictionary *carry = [NSMutableDictionary dictionary];

        NSString *deviceGroup = [OSASystemConfiguration automatedDeviceGroup];
        if (deviceGroup == nil) {
            // legacy location that lots of other automation scripting is still using
            deviceGroup = [self dataAnalyticsSetting:@"AutomatedDeviceGroup"];
        }
        if (deviceGroup) {
            carry[@"automatedDeviceGroup"] = deviceGroup;
        }

        NSString *experimentGroup = [self dataAnalyticsSetting:@"ExperimentGroup"];
        if (deviceGroup == nil && ([experimentGroup isEqual:@"walkabout"] || [experimentGroup isEqual:@"carry"])) {
            carry[@"carry"] = @YES;
        }

        if (carry.count != 0) {
            carryStatus = carry;
        }
    }

    secnotice("getLoggingJSON", "carrystatus is %@", carryStatus);

    return carryStatus;
}

- (BOOL)ckDeviceAccountApprovedTopic:(NSString *)topic {
    if (os_variant_has_internal_diagnostics("com.apple.security") == false) {
        return NO;
    }
    static dispatch_once_t onceToken;
    static NSSet<NSString*>* topics;
    dispatch_once(&onceToken, ^{
        topics = [NSSet setWithArray:@[
            SFAnalyticsTopicKeySync,
            SFAnalyticsTopicCloudServices,
            SFAnalyticsTopicTransparency,
            SFAnalyticsTopicSWTransparency,
        ]];
    });
    return [topics containsObject:topic];
}

- (BOOL)copyEvents:(NSMutableArray<NSDictionary *> *)healthSummaries
          failures:(NSMutableArray<NSDictionary *> *)failures
         forUpload:(BOOL)upload
participatingClients:(NSMutableArray<SFAnalyticsClient*>*)clients
             force:(BOOL)force
        linkedUUID:(NSUUID *)linkedUUID
             error:(NSError**)error
{
    NSMutableArray<SFAnalyticsClient*> *localClients = [NSMutableArray array];
    NSMutableArray<NSArray*> *rockwells = [NSMutableArray array];
    NSMutableArray<NSArray*> *hardFailures = [NSMutableArray array];
    NSMutableArray<NSArray*> *softFailures = [NSMutableArray array];
    NSString *ckdeviceID = nil;
    NSString *accountID = nil;
    NSString *appleUser = nil;
    NSDictionary *carryStatus = nil;

    if ([self ckDeviceAccountApprovedTopic:_internalTopicName]) {
        ckdeviceID = [self askSecurityForCKDeviceID];
        accountID = accountAltDSID();
        appleUser = [self appleUser];
        carryStatus = [self carryStatus];
        secnotice("getLoggingJSON", "including deviceID for internal user");
    } else {
        secnotice("getLoggingJSON", "no deviceID for internal user");
    }
    
    NSNumber* timestamp = @([[NSDate date] timeIntervalSince1970] * 1000);
    for (SFAnalyticsClient* client in self->_topicClients) {
        [client withStore:^(SFAnalyticsSQLiteStore *store) {
            NSNumber* lastUploadTime = nil;
            if (store.uploadDate){
                lastUploadTime = @([store.uploadDate timeIntervalSince1970] * 1000);
            }
            if (!force && [client requireDeviceAnalytics] && !_isDeviceAnalyticsEnabled()) {
                // Client required device analytics, yet the user did not opt in.
                secnotice("getLoggingJSON", "Client '%@' requires device analytics yet user did not opt in.", [client name]);
                return;
            }
            if (!force && [client requireiCloudAnalytics] && !_isiCloudAnalyticsEnabled()) {
                // Client required iCloud analytics, yet the user did not opt in.
                secnotice("getLoggingJSON", "Client '%@' requires iCloud analytics yet user did not opt in.", [client name]);
                return;
            }

            if (upload) {
                NSDate* uploadDate = store.uploadDate;
                if (!force && uploadDate && [[NSDate date] timeIntervalSinceDate:uploadDate] < _secondsBetweenUploads) {
                    secnotice("json", "ignoring client '%@' for %@ because last upload too recent: %@",
                              client.name, _internalTopicName, uploadDate);
                    return;
                }

                if (force) {
                    secnotice("json", "client '%@' for topic '%@' force-included", client.name, _internalTopicName);
                } else {
                    secnotice("json", "including client '%@' for topic '%@' for upload", client.name, _internalTopicName);
                }
                [localClients addObject:client];
            }

            NSMutableDictionary* healthSummary = [self healthSummaryWithName:client store:store uuid:linkedUUID timestamp:timestamp lastUploadTime:lastUploadTime];
            if (healthSummary) {
                if (ckdeviceID) {
                    healthSummary[SFAnalyticsDeviceID] = ckdeviceID;
                }
                if (accountID) {
                    healthSummary[SFAnalyticsAltDSID] = accountID;
                }
                if (carryStatus) {
                    [healthSummary addEntriesFromDictionary:carryStatus];
                }
                if (appleUser) {
                    healthSummary[SFAnalyticsIsAppleUser] = appleUser != nil ? @YES : @NO;
                }
                [healthSummaries addObject:healthSummary];
            }

            [rockwells addObject:store.rockwells];
            [hardFailures addObject:store.hardFailures];
            [softFailures addObject:store.softFailures];
        }];
    }

    if (upload && [localClients count] == 0) {
        if (error) {
            NSString *description = [NSString stringWithFormat:@"Upload too recent for all clients for %@", _internalTopicName];
            *error = [NSError errorWithDomain:@"SupdUploadErrorDomain"
                                         code:-10
                                     userInfo:@{NSLocalizedDescriptionKey : description}];
        }
        return NO;
    }

    [clients addObjectsFromArray:localClients];

    NSMutableArray<NSDictionary *>* localFailures = [NSMutableArray array];

    [self addFailures:rockwells toUploadRecords:localFailures threshold:_maxEventsToReport/10 linkedUUID:linkedUUID];
    [self addFailures:hardFailures toUploadRecords:localFailures threshold:_maxEventsToReport/10 linkedUUID:linkedUUID];
    [self addFailures:softFailures toUploadRecords:localFailures threshold:0 linkedUUID:linkedUUID];

    [failures addObjectsFromArray:localFailures];

    return YES;
}

- (NSArray<NSDictionary *> *)createChunkedLoggingJSON:(bool)pretty
                                            forUpload:(BOOL)upload
                                 participatingClients:(NSMutableArray<SFAnalyticsClient*>*)clients
                                                force:(BOOL)force                                       // supdctl uploads ignore privacy settings and recency
                                                error:(NSError**)error
{
    NSUUID *linkedUUID = [NSUUID UUID];
    NSError *localError = nil;
    NSMutableArray<NSDictionary *>* failures = [NSMutableArray array];
    NSMutableArray<NSDictionary *>* healthSummaries = [NSMutableArray array];

    BOOL copied = [self copyEvents:healthSummaries
                          failures:failures
                         forUpload:upload
              participatingClients:clients
                             force:force
                        linkedUUID:linkedUUID
                             error:&localError];
    if (!copied || localError) {
        if (error) {
            *error = localError;
        }
        return nil;
    }

    // Trim failures to the max count, disregard the health summaries
    if (failures.count > _maxEventsToReport) {
        NSRange range = NSMakeRange(0, _maxEventsToReport);
        failures = [[failures subarrayWithRange:range] mutableCopy];
    }

    return [self createChunkedLoggingJSON:healthSummaries failures:failures error:error];
}

- (NSDictionary *)createLoggingJSON:(bool)pretty
                          forUpload:(BOOL)upload
               participatingClients:(NSMutableArray<SFAnalyticsClient*>*)clients
                              force:(BOOL)force                                       // supdctl uploads ignore privacy settings and recency
                              error:(NSError**)error
{
    NSError *localError = nil;
    NSMutableArray<NSDictionary *>* failures = [NSMutableArray array];
    NSMutableArray<NSDictionary *>* healthSummaries = [NSMutableArray array];

    BOOL copied = [self copyEvents:healthSummaries
                          failures:failures
                         forUpload:upload
              participatingClients:clients
                             force:force
                        linkedUUID:nil
                             error:&localError];
    if (!copied || localError) {
        if (error) {
            *error = localError;
        }
        return nil;
    }

    // Trim failures to the max count, based on health summary count
    if ([failures count] > _maxEventsToReport) {
        NSRange range = NSMakeRange(0, _maxEventsToReport);
        failures = [[failures subarrayWithRange:range] mutableCopy];
    }

    NSMutableArray<NSDictionary *>* events = [NSMutableArray array];
    [events addObjectsFromArray:healthSummaries];
    [events addObjectsFromArray:failures];

    return [self createEventDictionary:events timestamp:@([[NSDate date] timeIntervalSince1970] * 1000) error:error];
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

- (NSString * _Nullable)appleUser {
    ACAccountStore *store = [[ACAccountStore alloc] init];
    ACAccountType *accountType = [store accountTypeWithAccountTypeIdentifier:ACAccountTypeIdentifierIMAP];
    NSArray *accounts = [store accountsWithAccountType:accountType];

    for (ACAccount *curAccount in accounts) {
        id addresses = curAccount[ACEmailAliasKeyEmailAddresses];
        NSArray *emails = nil;
        if ([addresses isKindOfClass:[NSDictionary class]]) {
            emails = [addresses allKeys];
        } else if ([addresses isKindOfClass:[NSArray class]]) {
            emails = addresses;
        } else if ([addresses isKindOfClass:[NSString class]]) {
            emails = [addresses componentsSeparatedByString:@","];
        }
        for (NSString *email in emails) {
            if ([email hasSuffix:@"@apple.com"]) {
                return email;
            }
        }
    }
    return nil;
}

// this method is kind of evil for the fact that it has side-effects in pulling other things besides the metricsURL from the server, and as such should NOT be memoized.
// TODO redo this, probably to return a dictionary.
- (NSURL*)splunkUploadURL:(BOOL)force urlSession:(NSURLSession*)urlSession
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
    NSURL* requestEndpoint = _splunkBagURL;
    __block NSURL* result = nil;
    NSURLSessionDataTask* storeBagTask = [urlSession dataTaskWithURL:requestEndpoint completionHandler:^(NSData * _Nullable data,
                                                                                                         NSURLResponse * _Nullable __unused response,
                                                                                                         NSError * _Nullable responseError) {

        __strong __typeof(self) strongSelf = weakSelf;
        if (!strongSelf) {
            return;
        }

        if (data && !responseError) {
            NSData *responseData = data; // shut up compiler
            NSDictionary* responseDict;
            @autoreleasepool {
                responseDict = [NSJSONSerialization JSONObjectWithData:responseData options:0 error:&error];
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
        }
        else {
            error = responseError;
        }
        if (error) {
            secnotice("upload", "Unable to fetch splunk endpoint at URL for %@: %@ -- error: %@",
                      self->_internalTopicName, requestEndpoint, error);
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

    if ([challenge previousFailureCount] > 0) {
        // Previous failures occurred, bail
        completionHandler(NSURLSessionAuthChallengeCancelAuthenticationChallenge, nil);

    } else if ([challenge.protectionSpace.authenticationMethod isEqualToString:NSURLAuthenticationMethodServerTrust]) {
        /*
         * Evaluate trust for the certificate
         */

        SecTrustRef serverTrust = challenge.protectionSpace.serverTrust;
        // Coverity gets upset if we don't check status even though result is all we need.
        bool trustResult = SecTrustEvaluateWithError(serverTrust, NULL);
        if (_allowInsecureSplunkCert || trustResult) {
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
#if TARGET_OS_OSX
    return [SFAnalytics defaultProtectedAnalyticsDatabasePath:@"ckks_analytics"];
#else
    return [(__bridge_transfer NSURL*)SecCopyURLForFileInKeychainDirectory((__bridge CFStringRef)@"Analytics/ckks_analytics.db") path];
#endif
}

+ (NSString*)databasePathForSOS
{
#if TARGET_OS_OSX
    return [SFAnalytics defaultProtectedAnalyticsDatabasePath:@"sos_analytics"];
#else
    return [(__bridge_transfer NSURL*)SecCopyURLForFileInKeychainDirectory((__bridge CFStringRef)@"Analytics/sos_analytics.db") path];
#endif
}

+ (NSString*)AppSupportPath
{
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
    return @"/var/mobile/Library/Application Support";
#else
    NSArray<NSString *>*paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, true);
    if ([paths count] < 1) {
        return nil;
    }
    return [NSString stringWithString: paths[0]];
#endif /* TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR  */
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
#if TARGET_OS_OSX
    return [SFAnalytics defaultProtectedAnalyticsDatabasePath:@"localkeychain"];
#else
    return [(__bridge_transfer NSURL*)SecCopyURLForFileInKeychainDirectory((__bridge CFStringRef)@"Analytics/localkeychain.db") path];
#endif
}

+ (NSString*)databasePathForTrust
{
#if TARGET_OS_OSX
    return [SFAnalytics defaultProtectedAnalyticsDatabasePath:@"trust_analytics"];
#else
    return [(__bridge_transfer NSURL*)SecCopyURLForFileInKeychainDirectory(CFSTR("Analytics/trust_analytics.db")) path];
#endif
}

#if TARGET_OS_OSX
#define TRUSTD_ROLE_ACCOUNT 282

+ (NSUUID *)trustdUUID
{
    uuid_t rootUuid;
    int ret = mbr_uid_to_uuid(TRUSTD_ROLE_ACCOUNT, rootUuid);
    if (ret != 0) {
        return nil;
    }
    return [[NSUUID alloc] initWithUUIDBytes:rootUuid];
}
#endif

#if TARGET_OS_OSX
+ (NSString*)databasePathForRootTrust
{
    return [SFAnalytics defaultProtectedAnalyticsDatabasePath:@"trust_analytics" uuid:[SFAnalyticsTopic trustdUUID]];
}
#endif

+ (NSString*)databasePathForNetworking
{
#if TARGET_OS_OSX
    return [SFAnalytics defaultProtectedAnalyticsDatabasePath:@"networking_analytics"];
#else
    return [(__bridge_transfer NSURL*)SecCopyURLForFileInKeychainDirectory(CFSTR("Analytics/networking_analytics.db")) path];
#endif
}

#if TARGET_OS_OSX
+ (NSString*)databasePathForRootNetworking
{
    return [SFAnalytics defaultProtectedAnalyticsDatabasePath:@"networking_analytics" uuid:[SFAnalyticsTopic trustdUUID]];
}
#endif

+ (NSString*)databasePathForCloudServices
{
#if TARGET_OS_OSX
    return [SFAnalytics defaultProtectedAnalyticsDatabasePath:@"CloudServicesAnalytics"];
#else
    return [(__bridge_transfer NSURL*)SecCopyURLForFileInKeychainDirectory(CFSTR("Analytics/CloudServicesAnalytics.db")) path];
#endif
}

+ (NSString*)databasePathForTransparency
{
#if TARGET_OS_OSX
    return [SFAnalytics defaultProtectedAnalyticsDatabasePath:@"TransparencyAnalytics"];
#else
    return [(__bridge_transfer NSURL*)SecCopyURLForFileInKeychainDirectory((__bridge CFStringRef)@"Analytics/TransparencyAnalytics.db") path];
#endif
}

+ (NSString*)databasePathForSWTransparency
{
#if TARGET_OS_OSX
    return [SFAnalytics defaultProtectedAnalyticsDatabasePath:@"SWTransparencyAnalytics"];
#else
    return [(__bridge_transfer NSURL*)SecCopyURLForFileInKeychainDirectory((__bridge CFStringRef)@"Analytics/SWTransparencyAnalytics.db") path];
#endif
}

@end

@interface supd ()
@property NSDictionary *topicsSamplingRates;
@property NSXPCConnection *connection;
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
    NSBundle *trustStoreBundle = [NSBundle bundleWithPath:SystemTrustStorePath];

    NSURL *protectedDirectory = CFBridgingRelease(SecCopyURLForFileInProtectedDirectory(CFSTR("trustd/")));
    NSURL *directory = [protectedDirectory URLByAppendingPathComponent:@"SupplementalsAssets/" isDirectory:YES];

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

- (instancetype)initWithConnection:(NSXPCConnection *)connection reporter:(SFAnalyticsReporter *)reporter
{
    if ((self = [super init])) {
        _connection = connection;
        _reporter = reporter;
        [self setupSamplingRates];
        [self setupTopics];

        static dispatch_once_t onceToken;
        dispatch_once(&onceToken, ^{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            xpc_activity_register("com.apple.securityuploadd.triggerupload", XPC_ACTIVITY_CHECK_IN, ^(xpc_activity_t activity) {
                xpc_activity_state_t activityState = xpc_activity_get_state(activity);
                secnotice("supd", "hit xpc activity trigger, state: %ld", activityState);
                if (activityState == XPC_ACTIVITY_STATE_RUN) {
                    // Run our regularly scheduled scan
                    [self performRegularlyScheduledUpload];
                }
            });
#pragma clang diagnostic pop
        });
    }
    return self;
}

- (instancetype)initWithConnection:(NSXPCConnection *)connection {
    SFAnalyticsReporter *reporter = [[SFAnalyticsReporter alloc] init];
    return [self initWithConnection:connection reporter:reporter];
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

+ (NSData *)serializeLoggingEvent:(NSDictionary *)event
                                        error:(NSError **)error
{
    if (!event) {
        if (error){
            *error = [NSError errorWithDomain:SupdErrorDomain code:SupdMissingParamError userInfo:nil];
        }
        return nil;
    }

    NSError *serializationError = nil;
    NSData* serializedEvent;
    @autoreleasepool {
        serializedEvent = [NSJSONSerialization dataWithJSONObject:event
                                                          options:0
                                                            error:&serializationError];
    }
    if (serializedEvent && !serializationError) {
        return serializedEvent;
    } else if (error) {
        *error = serializationError;
        return nil;
    }

    return nil;
}

- (BOOL)uploadAnalyticsWithError:(NSError**)error force:(BOOL)force {
    [self sendNotificationForOncePerReportSamplers];
    
    BOOL result = NO;
    NSError* localError = nil;
    NSURLSession* postSession = nil;
    for (SFAnalyticsTopic *topic in _analyticsTopics) {
        @autoreleasepool { // The logging JSONs get quite large. Ensure they're deallocated between topics.
            if (postSession == nil) {
                postSession = [topic getSession];
            }

            __block NSURL* endpoint = [topic splunkUploadURL:force urlSession:postSession];   // has side effects!

            if (!endpoint) {
                secnotice("upload", "Skipping upload for %@ because no endpoint", [topic internalTopicName]);
                continue;
            }

            if ([topic disableUploads]) {
                secnotice("upload", "Aborting upload task for %@ because uploads are disabled", [topic internalTopicName]);
                continue;
            }

            NSMutableArray<SFAnalyticsClient*>* clients = [NSMutableArray array];
            NSArray<NSDictionary *> *jsonEvents = [topic createChunkedLoggingJSON:false
                                                                        forUpload:YES
                                                             participatingClients:clients
                                                                            force:force
                                                                            error:&localError];
            if (!jsonEvents || localError) {
                if ([[localError domain] isEqualToString:SupdErrorDomain] && [localError code] == SupdInvalidJSONError) {
                    // Pretend this was a success because at least we'll get rid of bad data.
                    // If someone keeps logging bad data and we only catch it here then
                    // this causes sustained data loss for the entire topic.
                    [topic updateUploadDateForClients:clients date:[NSDate date] clearData:YES];
                }
                secerror("upload: failed to create chunked log events for logging topic %@: %@", [topic internalTopicName], localError);
                continue;
            }


            if ([topic isSampledUpload]) {
                bool failed = false;
                for (NSDictionary* event in jsonEvents) {
                    // make sure we don't hold on to each NSURL related in each autorelease pool
                    @autoreleasepool {
                        NSData *serializedEvent = [supd serializeLoggingEvent:event error:&localError];
                        if (!serializedEvent || localError) {
                            if ([[localError domain] isEqualToString:SupdErrorDomain] && [localError code] == SupdInvalidJSONError) {
                                // Pretend this was a success because at least we'll get rid of bad data.
                                // If someone keeps logging bad data and we only catch it here then
                                // this causes sustained data loss for the entire topic.
                                failed = true;
                                [topic updateUploadDateForClients:clients date:[NSDate date] clearData:YES];
                            }
                            secerror("upload: failed to serialized chunked log events for logging topic %@: %@", [topic internalTopicName], localError);
                            break;
                        }
                        
                        if (![self->_reporter saveReport:serializedEvent fileName:[topic internalTopicName]]) {
                            secerror("upload: failed to write analytics data to log");
                        }
                        if ([topic postJSON:serializedEvent toEndpoint:endpoint postSession:postSession error:&localError]) {
                            secnotice("upload", "Successfully posted JSON for %@", [topic internalTopicName]);
                            result = YES;
                        } else {
                            secerror("upload: Failed to post JSON for %@: %@", [topic internalTopicName], localError);
                        }
                    }
                }
                if (failed == false) {
                    [topic updateUploadDateForClients:clients date:[NSDate date] clearData:YES];
                }
            } else {
                /* If we didn't sample this report, update date to prevent trying to upload again sooner
                 * than we should. Clear data so that per-day calculations remain consistent. */
                secnotice("upload", "skipping unsampled upload for %@ and clearing data", [topic internalTopicName]);
                [topic updateUploadDateForClients:clients date:[NSDate date] clearData:YES];
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
            [client withStore:^(SFAnalyticsSQLiteStore *store) {
                [sysdiagnose appendString:[NSString stringWithFormat:@"Client: %@\n", client.name]];
                NSArray* allEvents = store.allEvents;
                for (NSDictionary* eventRecord in allEvents) {
                    [sysdiagnose appendFormat:@"%@\n", [self sysdiagnoseStringForEventRecord:eventRecord]];
                }
                if (allEvents.count == 0) {
                    [sysdiagnose appendString:@"No data to report for this client\n"];
                }
            }];
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
    else if (eventClass == SFAnalyticsEventClassRockwell) {
        return @"EventRockwell";
    }
    else {
        return @"EventUnknown";
    }
}

// MARK: XPC Procotol Handlers
- (BOOL)checkSupdEntitlement {
    NSNumber *supdEntitlement = [self.connection valueForEntitlement:@"com.apple.private.securityuploadd"];
    if (![supdEntitlement isKindOfClass:[NSNumber class]] || ![supdEntitlement boolValue]) {
        return NO;
    }
    return YES;
}

- (void)getSysdiagnoseDumpWithReply:(void (^)(NSString*))reply {
    if ([self checkSupdEntitlement]) {
        reply([self getSysdiagnoseDump]);
    } else {
        reply(@"client not entitled");
    }
}

- (void)createLoggingJSON:(bool)pretty topic:(NSString *)topicName reply:(void (^)(NSData *, NSError*))reply {
    if (![self checkSupdEntitlement]) {
        NSError *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecMissingEntitlement userInfo:nil];
        reply(nil, error);
        return;
    }

    secnotice("rpcCreateLoggingJSON", "Building a JSON blob resembling the one we would have uploaded");
    NSError* error = nil;
    [self sendNotificationForOncePerReportSamplers];
    NSDictionary *eventDictionary = nil;
    for (SFAnalyticsTopic* topic in self->_analyticsTopics) {
        if ([topic.internalTopicName isEqualToString:topicName]) {
            eventDictionary = [topic createLoggingJSON:pretty forUpload:NO participatingClients:nil force:!runningTests error:&error];
        }
    }

    NSData *data = nil;
    if (!eventDictionary) {
        secerror("Unable to obtain JSON: %@", error);
    } else {
        @autoreleasepool {
            data = [NSJSONSerialization dataWithJSONObject:eventDictionary
                                                   options:(pretty ? NSJSONWritingPrettyPrinted : 0)
                                                     error:&error];
        }
    }

    reply(data, error);
}

- (void)createChunkedLoggingJSON:(bool)pretty topic:(NSString *)topicName reply:(void (^)(NSData *, NSError*))reply
{
    if (![self checkSupdEntitlement]) {
        NSError *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecMissingEntitlement userInfo:nil];
        reply(nil, error);
        return;
    }

    secnotice("rpcCreateChunkedLoggingJSON", "Building an array of JSON blobs resembling the one we would have uploaded");
    NSError* error = nil;
    [self sendNotificationForOncePerReportSamplers];
    NSArray<NSDictionary *> *events = nil;
    for (SFAnalyticsTopic* topic in self->_analyticsTopics) {
        if ([topic.internalTopicName isEqualToString:topicName]) {
            events = [topic createChunkedLoggingJSON:pretty forUpload:NO participatingClients:nil force:!runningTests error:&error];
        }
    }

    NSData *data = nil;

    @autoreleasepool {
        if (!events) {
            secerror("Unable to obtain JSON: %@", error);
        } else {
            data = [NSJSONSerialization dataWithJSONObject:events
                                                   options:(pretty ? NSJSONWritingPrettyPrinted : 0)
                                                     error:&error];
        }
    }

    reply(data, error);
}

- (void)forceUploadWithReply:(void (^)(BOOL, NSError*))reply {
    if (![self checkSupdEntitlement]) {
        NSError *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecMissingEntitlement userInfo:nil];
        reply(NO, error);
        return;
    }

    secnotice("upload", "Performing upload in response to rpc message");
    NSError* error = nil;
    BOOL result = [self uploadAnalyticsWithError:&error force:YES];
    secnotice("upload", "Result of manually triggered upload: %@, error: %@", result ? @"success" : @"failure", error);
    reply(result, error);
}

- (void)setUploadDateWith:(NSDate *)date reply:(void (^)(BOOL, NSError*))reply
{
    if (![self checkSupdEntitlement]) {
        NSError *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecMissingEntitlement userInfo:nil];
        reply(NO, error);
        return;
    }

    for (SFAnalyticsTopic* topic in _analyticsTopics) {
        [topic updateUploadDateForClients:topic.topicClients date:date clearData:NO];
    }
    reply(YES, nil);
}

- (void)clientStatus:(void (^)(NSDictionary<NSString *, id> *, NSError *))reply
{
    if (![self checkSupdEntitlement]) {
        NSError *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecMissingEntitlement userInfo:nil];
        reply(nil, error);
        return;
    }

    NSMutableDictionary *info = [NSMutableDictionary dictionary];
    for (SFAnalyticsTopic* topic in _analyticsTopics) {
        for (SFAnalyticsClient *client in topic.topicClients) {
            [client withStore:^(SFAnalyticsSQLiteStore *store) {
                NSMutableDictionary *clientInfo = [NSMutableDictionary dictionary];
                clientInfo[@"uploadDate"] = store.uploadDate;
                info[client.name] = clientInfo;
            }];
        }
    }

    reply(info, nil);
}

- (void)fixFiles:(void (^)(BOOL, NSError*))reply
{
    if (![[self.connection valueForEntitlement:@"com.apple.private.trustd.FileHelp"] boolValue]) {
        reply(NO, [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecMissingEntitlement userInfo:nil]);
        return;
    }
#if TARGET_OS_IPHONE
    TrustdFileHelper *helper = [[TrustdFileHelper alloc] init];
    [helper fixFiles:reply];
#else
    reply(NO, [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil]);
#endif
}

@end
