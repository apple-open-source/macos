//
//  SecExperiment.m
//  Security
//

#include <xpc/xpc.h>
#include <os/log.h>
#include <Security/SecTrustPriv.h>
#include <uuid/uuid.h>

#define OS_OBJECT_HAVE_OBJC_SUPPORT 1

#define SEC_EXP_NULL_BAD_INPUT ((void *_Nonnull)NULL)
#define SEC_EXP_NULL_OUT_OF_MEMORY SEC_EXP_NULL_BAD_INPUT

#define SEC_EXP_NIL_BAD_INPUT ((void *_Nonnull)nil)
#define SEC_EXP_NIL_OUT_OF_MEMORY SEC_EXP_NIL_BAD_INPUT

#define SEC_EXP_CONCRETE_CLASS_NAME(external_type) SecExpConcrete_##external_type
#define SEC_EXP_CONCRETE_PREFIX_STR "SecExpConcrete_"

#define SEC_EXP_OBJECT_DECL_INTERNAL_OBJC(external_type)                                                    \
@class SEC_EXP_CONCRETE_CLASS_NAME(external_type);                                                        \
typedef SEC_EXP_CONCRETE_CLASS_NAME(external_type) *external_type##_t

#define SEC_EXP_OBJECT_IMPL_INTERNAL_OBJC_WITH_PROTOCOL_AND_VISBILITY(external_type, _protocol, visibility, ...)    \
@protocol OS_OBJECT_CLASS(external_type) <_protocol>                                                        \
@end                                                                                                        \
visibility                                                                                                    \
@interface SEC_EXP_CONCRETE_CLASS_NAME(external_type) : NSObject<OS_OBJECT_CLASS(external_type)>                \
_Pragma("clang diagnostic push")                                                                    \
_Pragma("clang diagnostic ignored \"-Wobjc-interface-ivars\"")                                        \
__VA_ARGS__                                                                                        \
_Pragma("clang diagnostic pop")                                                                        \
@end                                                                                                    \
typedef int _useless_typedef_oio_##external_type

#define SEC_EXP_OBJECT_IMPL_INTERNAL_OBJC_WITH_PROTOCOL(external_type, _protocol, ...)                        \
SEC_EXP_OBJECT_IMPL_INTERNAL_OBJC_WITH_PROTOCOL_AND_VISBILITY(external_type, _protocol, ,__VA_ARGS__)

#define SEC_EXP_OBJECT_IMPL_INTERNAL_OBJC(external_type, ...)                                                \
SEC_EXP_OBJECT_IMPL_INTERNAL_OBJC_WITH_PROTOCOL(external_type, NSObject, ##__VA_ARGS__)

#define SEC_EXP_OBJECT_IMPL_INTERNAL_OBJC_WITH_VISIBILITY(external_type, visibility, ...)                    \
SEC_EXP_OBJECT_IMPL_INTERNAL_OBJC_WITH_PROTOCOL_AND_VISBILITY(external_type, NSObject, visibility, ##__VA_ARGS__)

SEC_EXP_OBJECT_DECL_INTERNAL_OBJC(sec_experiment);

#define SEC_EXP_OBJECT_IMPL 1
#import "SecExperimentPriv.h"
#import "SecExperimentInternal.h"
#import "SecCFRelease.h"
#import <Foundation/Foundation.h>
#import <CoreFoundation/CFXPCBridge.h>
#import <System/sys/codesign.h>
#import <sys/errno.h>

#define SEC_EXPERIMENT_SAMPLING_RATE 100.0
#define HASH_INITIAL_VALUE 0
#define HASH_MULTIPLIER 31

const char *kSecExperimentDefaultsDomain = "com.apple.security.experiment";
const char *kSecExperimentDefaultsDisableSampling = "disableSampling";
const char *kSecExperimentTLSProbe = "TLSProbeExperiment";

const NSString *SecExperimentConfigurationKeyFleetSampleRate = @"FleetSampleRate";
const NSString *SecExperimentConfigurationKeyDeviceSampleRate = @"DeviceSampleRate";
const NSString *SecExperimentConfigurationKeyExperimentIdentifier = @"ExpName";
const NSString *SecExperimentConfigurationKeyConfigurationData = @"ConfigData";

static os_log_t
sec_experiment_copy_log_handle(void)
{
    static dispatch_once_t onceToken = 0;
    static os_log_t experiment_log = nil;
    dispatch_once(&onceToken, ^{
        experiment_log = os_log_create("com.apple.security", "experiment");
    });
    return experiment_log;
}

#define sec_experiment_log_info(fmt, ...) \
    do { \
        os_log_t _log_handle = sec_experiment_copy_log_handle(); \
        if (_log_handle) { \
            os_log_info(_log_handle, fmt, ##__VA_ARGS__); \
        } \
    } while (0);

#define sec_experiment_log_debug(fmt, ...) \
    do { \
        os_log_t _log_handle = sec_experiment_copy_log_handle(); \
        if (_log_handle) { \
            os_log_debug(_log_handle, fmt, ##__VA_ARGS__); \
        } \
    } while (0);

#define sec_experiment_log_error(fmt, ...) \
    do { \
        os_log_t _log_handle = sec_experiment_copy_log_handle(); \
        if (_log_handle) { \
            os_log_error(_log_handle, fmt, ##__VA_ARGS__); \
        } \
    } while (0);

// Computes hash of input and returns a value between 1-100
static uint32_t
sec_experiment_hash_multiplicative(const uint8_t *key, size_t len)
{
    if (!key) {
        return 0;
    }

    uint32_t hash = HASH_INITIAL_VALUE;
    for (uint32_t i = 0; i < len; ++i) {
        hash = HASH_MULTIPLIER * hash + key[i];
    }

    return hash % 101; // value between 0-100
}

static uint32_t
sec_experiment_host_hash(void)
{
    static uuid_string_t hostuuid = {};
    static uint32_t hash = 0;
    static dispatch_once_t onceToken = 0;
    dispatch_once(&onceToken, ^{
        struct timespec timeout = {0, 0};
        uuid_t uuid = {};
        if (gethostuuid(uuid, &timeout) == 0) {
            uuid_unparse(uuid, hostuuid);
            hash = sec_experiment_hash_multiplicative((const uint8_t *)hostuuid, strlen(hostuuid));
        } else {
            onceToken = 0;
        }
    });
    return hash;
}

SEC_EXP_OBJECT_IMPL_INTERNAL_OBJC(sec_experiment,
{
@public
    SecExperiment *innerExperiment;
    size_t numRuns;
    size_t successRuns;
});

@implementation SEC_EXP_CONCRETE_CLASS_NAME(sec_experiment)

- (instancetype)initWithName:(const char *)name
{
    if (name == NULL) {
        return SEC_EXP_NIL_BAD_INPUT;
    }

    if ((self = [super init])) {
        self->innerExperiment = [[SecExperiment alloc] initWithName:name];
    } else {
        return SEC_EXP_NIL_OUT_OF_MEMORY;
    }
    return self;
}

- (instancetype)initWithInnerExperiment:(SecExperiment *)experiment
{
    if (experiment == NULL) {
        return SEC_EXP_NIL_BAD_INPUT;
    }

    if ((self = [super init])) {
        self->innerExperiment = experiment;
    } else {
        return SEC_EXP_NIL_OUT_OF_MEMORY;
    }
    return self;
}

- (const char *)name
{
    return [innerExperiment.name UTF8String];
}

- (const char *)identifier
{
    return [innerExperiment.identifier UTF8String];
}

- (BOOL)experimentIsAllowedForProcess
{
    return [innerExperiment experimentIsAllowedForProcess];
}

- (BOOL)isSamplingDisabledWithDefault:(BOOL)defaultValue
{
    return [innerExperiment isSamplingDisabledWithDefault:defaultValue];
}

- (BOOL)isSamplingDisabled
{
    return [innerExperiment isSamplingDisabled];
}

- (SecExperimentConfig *)copyExperimentConfiguration
{
    return [innerExperiment copyExperimentConfiguration];
}

@end

@interface SecExperiment()
@property NSString *name;
@property (nonatomic) BOOL samplingDisabled;
@property SecExperimentConfig *cachedConfig;
@end

@implementation SecExperiment

- (instancetype)initWithName:(const char *)name
{
    if (name == NULL) {
        return SEC_EXP_NIL_BAD_INPUT;
    }

    if ((self = [super init])) {
        self.name = [NSString stringWithUTF8String:name];
    } else {
        return SEC_EXP_NIL_OUT_OF_MEMORY;
    }
    return self;
}

- (BOOL)experimentIsAllowedForProcess
{
    __block NSArray<NSString *> *whitelistedProcesses = @[
        @"nsurlsessiond",
        @"com.apple.WebKit.Networking",
        @"experimentTool",
        @"network_test",
    ];

    static BOOL isAllowed = NO;
    static dispatch_once_t onceToken = 0;
    dispatch_once(&onceToken, ^{
        uint32_t flags = 0;
        int ret = csops(getpid(), CS_OPS_STATUS, &flags, sizeof(flags));
        if (ret) {
            // Fail closed if we're not able to determine the type of binary.
            return;
        }

        if (!(flags & CS_PLATFORM_BINARY)) {
            // Allow SecExperiment on all non-platform binaries, e.g., third party apps.
            isAllowed = YES;
            return;
        }

        // Otherwise, this is a platform binary. Check against the set of whitelisted processes.
        NSString *process = [NSString stringWithFormat:@"%s", getprogname()];
        [whitelistedProcesses enumerateObjectsUsingBlock:^(NSString * _Nonnull whitelistedProcess, NSUInteger idx, BOOL * _Nonnull stop) {
            if ([whitelistedProcess isEqualToString:process]) {
                isAllowed = YES;
                *stop = YES; // Stop searching the whitelist
            }
        }];
    });

    return isAllowed;
}

- (BOOL)isSamplingDisabledWithDefault:(BOOL)defaultValue
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if (defaults != nil) {
        NSMutableDictionary *experimentDefaults = [[defaults persistentDomainForName:[NSString stringWithUTF8String:kSecExperimentDefaultsDomain]] mutableCopy];
        if (experimentDefaults != nil) {
            NSString *key = [NSString stringWithUTF8String:kSecExperimentDefaultsDisableSampling];
            if (experimentDefaults[key] != nil) {
                return [experimentDefaults[key] boolValue];
            }
        }
    }

    return defaultValue;
}

- (BOOL)isSamplingDisabled
{
    return [self isSamplingDisabledWithDefault:self.samplingDisabled];
}

- (NSDictionary *)copyExperimentConfigurationFromUserDefaults
{
    NSDictionary *result = nil;

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if (defaults != nil) {
        NSMutableDictionary *experimentDefaults = [[defaults persistentDomainForName:[NSString stringWithUTF8String:kSecExperimentDefaultsDomain]] mutableCopy];
        if (experimentDefaults != nil) {
            NSString *key = self.name;
            if (experimentDefaults[key] != nil) {
                result = experimentDefaults[key];
            }
        }
    }

    return result;
}

- (NSDictionary *)copyRemoteExperimentAsset
{
    CFErrorRef error = NULL;
    NSDictionary *config = NULL;
    NSDictionary *asset = CFBridgingRelease(SecTrustOTASecExperimentCopyAsset(&error));
    if (asset) {
        config = [asset valueForKey:self.name];
    }
    CFReleaseNull(error);
    return config;
}

- (NSDictionary *)copyRandomExperimentConfigurationFromAsset:(NSDictionary *)asset
{
    NSArray *array = [asset valueForKey:@"ConfigArray"];
    if (array == nil) {
        return nil;
    }
    return [array objectAtIndex:(arc4random() % [array count])];
}

- (SecExperimentConfig *)copyExperimentConfiguration
{
    if (self.cachedConfig) {
        // If we've fetched an experiment config already, use it for the duration of this object's lifetime.
        return self.cachedConfig;
    }

    NSDictionary *defaultsDictionary = [self copyExperimentConfigurationFromUserDefaults];
    if (defaultsDictionary != nil) {
        self.cachedConfig = [[SecExperimentConfig alloc] initWithConfiguration:defaultsDictionary];
        return self.cachedConfig;
    }

    NSDictionary *remoteAsset = [self copyRemoteExperimentAsset];
    if (remoteAsset != nil) {
        NSDictionary *randomConfig = [self copyRandomExperimentConfigurationFromAsset:remoteAsset];
        self.cachedConfig = [[SecExperimentConfig alloc] initWithConfiguration:randomConfig];
        return self.cachedConfig;
    }

    return nil;
}

- (NSString *)identifier
{
    if (self.cachedConfig != nil) {
        return [self.cachedConfig identifier];
    } else {
        return nil;
    }
}

@end

@interface SecExperimentConfig()
@property NSString *identifier;
@property NSDictionary *config;
@property uint32_t fleetSampleRate;
@property uint32_t deviceSampleRate;
@property NSDictionary *configurationData;
@end

@implementation SecExperimentConfig

- (instancetype)initWithConfiguration:(NSDictionary *)configuration
{
    if (configuration == nil) {
        return SEC_EXP_NIL_BAD_INPUT;
    }

    if ((self = [super init])) {
        // Parse out experiment information from the configuration dictionary
        self.config = configuration;
        self.identifier = [configuration objectForKey:SecExperimentConfigurationKeyExperimentIdentifier];

        NSNumber *deviceSampleRate = [configuration objectForKey:SecExperimentConfigurationKeyDeviceSampleRate];
        if (deviceSampleRate != nil) {
            self.deviceSampleRate = [deviceSampleRate unsignedIntValue];
        }

        NSNumber *fleetSampleRate = [configuration objectForKey:SecExperimentConfigurationKeyFleetSampleRate];
        if (fleetSampleRate != nil) {
            self.fleetSampleRate = [fleetSampleRate unsignedIntValue];
        }

        self.configurationData = [configuration objectForKey:SecExperimentConfigurationKeyConfigurationData];
    } else {
        return SEC_EXP_NIL_OUT_OF_MEMORY;
    }

    return self;
}

- (uint32_t)hostHash
{
    return sec_experiment_host_hash();
}

- (BOOL)shouldRunWithSamplingRate:(NSNumber *)sampleRate
{
    if (!sampleRate) {
        return NO;
    }

    uint32_t sample = arc4random();
    return ((float)sample < ((float)UINT32_MAX / [sampleRate unsignedIntegerValue]));
}

- (BOOL)isSampled
{
    uint32_t hostIdHash = [self hostHash];
    if ((hostIdHash == 0) || (self.fleetSampleRate < hostIdHash)) {
        return NO;
    }

    return [self shouldRunWithSamplingRate:@(self.deviceSampleRate)];
}

@end

sec_experiment_t
sec_experiment_create(const char *name)
{
    return [[SEC_EXP_CONCRETE_CLASS_NAME(sec_experiment) alloc] initWithName:name];
}

sec_experiment_t
sec_experiment_create_with_inner_experiment(SecExperiment *experiment)
{
    return [[SEC_EXP_CONCRETE_CLASS_NAME(sec_experiment) alloc] initWithInnerExperiment:experiment];
}

void
sec_experiment_set_sampling_disabled(sec_experiment_t experiment, bool sampling_disabled)
{
    experiment->innerExperiment.samplingDisabled = sampling_disabled;
}

const char *
sec_experiment_get_identifier(sec_experiment_t experiment)
{
    return [experiment identifier];
}

xpc_object_t
sec_experiment_copy_configuration(sec_experiment_t experiment)
{
    if (experiment == nil) {
        return nil;
    }

    // Check first for defaults configured
    SecExperimentConfig *experimentConfiguration = [experiment copyExperimentConfiguration];
    if (experimentConfiguration != nil) {
        NSDictionary *configurationData = [experimentConfiguration configurationData];
        if (![experiment isSamplingDisabled]) {
            if ([experimentConfiguration isSampled]) {
                return _CFXPCCreateXPCObjectFromCFObject((__bridge CFDictionaryRef)configurationData);
            } else {
                sec_experiment_log_info("Configuration '%{public}s' for experiment '%{public}s' not sampled to run",
                                        [experiment name], [[experimentConfiguration identifier] UTF8String]);
                return nil;
            }
        } else {
            return _CFXPCCreateXPCObjectFromCFObject((__bridge CFDictionaryRef)configurationData);
        }
    }

    return nil;
}

bool
sec_experiment_run_internal(sec_experiment_t experiment, bool sampling_disabled, dispatch_queue_t queue, sec_experiment_run_block_t run_block, sec_experiment_skip_block_t skip_block, bool synchronous)
{
    if (experiment == NULL || run_block == nil) {
        return false;
    }

    if (![experiment experimentIsAllowedForProcess]) {
        sec_experiment_log_info("Not running experiments for disallowed process");
        return false;
    }

     dispatch_block_t experiment_block = ^{
         bool experiment_sampling_disabled = [experiment isSamplingDisabledWithDefault:sampling_disabled];
         sec_experiment_set_sampling_disabled(experiment, [experiment isSamplingDisabledWithDefault:sampling_disabled]);
         xpc_object_t config = sec_experiment_copy_configuration(experiment);
         const char *identifier = sec_experiment_get_identifier(experiment);
         if (config != nil) {
             experiment->numRuns++;
             if (run_block(identifier, config)) {
                 experiment->successRuns++;
                 sec_experiment_log_info("Configuration '%s' for experiment '%s' succeeded", identifier, [experiment name]);
             } else {
                 sec_experiment_log_info("Configuration '%s' for experiment '%s' failed", identifier, [experiment name]);
             }
         } else {
             sec_experiment_log_info("Configuration '%s' for experiment '%s' not configured to run with sampling %s", identifier,
                                     [experiment name], experiment_sampling_disabled ? "disabled" : "enabled");
             if (skip_block) {
                 skip_block(sec_experiment_get_identifier(experiment));
             }
         }
    };

    if (synchronous || !queue) {
        sec_experiment_log_info("Starting experiment '%s' synchronously with sampling %s", [experiment name], sampling_disabled ? "disabled" : "enabled");
        experiment_block();
    } else {
        sec_experiment_log_info("Starting experiment '%s' asynchronously with sampling %s", [experiment name], sampling_disabled ? "disabled" : "enabled");
        dispatch_async(queue, experiment_block);
    }

    return true;
}

bool
sec_experiment_run(const char *experiment_name, sec_experiment_run_block_t run_block, sec_experiment_skip_block_t skip_block)
{
    // Sampling is always enabled for SecExperiment callers. Appliations may override this by setting the
    // `disableSampling` key in the `com.apple.security.experiment` defaults domain.
    sec_experiment_t experiment = sec_experiment_create(experiment_name);
    if (experiment) {
        return sec_experiment_run_internal(experiment, false, NULL, run_block, skip_block, true);
    } else {
        sec_experiment_log_info("Experiment '%s' not found", experiment_name);
        return false;
    }
}

bool
sec_experiment_run_async(const char *experiment_name, dispatch_queue_t queue, sec_experiment_run_block_t run_block, sec_experiment_skip_block_t skip_block)
{
    sec_experiment_t experiment = sec_experiment_create(experiment_name);
    if (experiment) {
        return sec_experiment_run_internal(experiment, false, queue, run_block, skip_block, false);
    } else {
        sec_experiment_log_info("Experiment '%s' not found", experiment_name);
        return false;
    }
}

bool
sec_experiment_run_with_sampling_disabled(const char *experiment_name, sec_experiment_run_block_t run_block, sec_experiment_skip_block_t skip_block, bool sampling_disabled)
{
    sec_experiment_t experiment = sec_experiment_create(experiment_name);
    if (experiment) {
        return sec_experiment_run_internal(experiment, sampling_disabled, NULL, run_block, skip_block, true);
    } else {
        sec_experiment_log_info("Experiment '%s' not found", experiment_name);
        return false;
    }
}

bool
sec_experiment_run_async_with_sampling_disabled(const char *experiment_name, dispatch_queue_t queue, sec_experiment_run_block_t run_block, sec_experiment_skip_block_t skip_block, bool sampling_disabled)
{
    sec_experiment_t experiment = sec_experiment_create(experiment_name);
    if (experiment) {
        return sec_experiment_run_internal(experiment, sampling_disabled, queue, run_block, skip_block, false);
    } else {
        sec_experiment_log_info("Experiment '%s' not found", experiment_name);
        return false;
    }
}

size_t
sec_experiment_get_run_count(sec_experiment_t experiment)
{
    return experiment->numRuns;
}

size_t
sec_experiment_get_successful_run_count(sec_experiment_t experiment)
{
    return experiment->successRuns;
}
