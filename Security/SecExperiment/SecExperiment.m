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

#define SEC_EXPERIMENT_SAMPLING_RATE 100.0
#define HASH_INITIAL_VALUE 0
#define HASH_MULTIPLIER 31

const char *kSecExperimentDefaultsDomain = "com.apple.security.experiment";
const char *kSecExperimentDefaultsDisableSampling = "disableSampling";
const char *kSecExperimentTLSMobileAssetConfig = "TLSConfig";

const NSString *SecExperimentMAPrefix = @"com.apple.MobileAsset.";

SEC_EXP_OBJECT_IMPL_INTERNAL_OBJC(sec_experiment,
{
    const char *identifier;
    bool sampling_disabled;
});

@implementation SEC_EXP_CONCRETE_CLASS_NAME(sec_experiment)

- (instancetype)initWithBundle:(const char *)bundle
{
    if (bundle == NULL) {
        return SEC_EXP_NIL_BAD_INPUT;
    }
    
    self = [super init];
    if (self == nil) {
        return SEC_EXP_NIL_OUT_OF_MEMORY;
    } else {
        self->identifier = bundle;
    }
    return self;
}

// Computes hash of input and returns a value between 1-100
static uint32_t
_hash_multiplicative(const char *key, size_t len)
{
    if (!key) {
        return 0;
    }
    uint32_t hash = HASH_INITIAL_VALUE;
    for (uint32_t i = 0; i < len; ++i) {
        hash = HASH_MULTIPLIER * hash + key[i];
    }
    return hash % 101; // value between 1-100
}

// Computes hash of device UUID
static uint32_t
_get_host_id_hash(void)
{
    static uuid_string_t hostuuid = {};
    static uint32_t hash = 0;
    static dispatch_once_t onceToken = 0;
    dispatch_once(&onceToken, ^{
        struct timespec timeout = {0, 0};
        uuid_t uuid = {};
        if (gethostuuid(uuid, &timeout) == 0) {
            uuid_unparse(uuid, hostuuid);
            hash = _hash_multiplicative(hostuuid, strlen(hostuuid));
        } else {
            onceToken = 0;
        }
    });
    return hash;
}

static bool
sec_experiment_is_sampling_disabled_with_default(bool default_value)
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
    
    return default_value;
}

sec_experiment_t
sec_experiment_create(const char *bundle)
{
    return [[SEC_EXP_CONCRETE_CLASS_NAME(sec_experiment) alloc] initWithBundle:bundle];
}

static xpc_object_t
_copy_builtin_experiment_asset(sec_experiment_t experiment)
{
    if (strncmp(experiment->identifier, kSecExperimentTLSMobileAssetConfig, strlen(kSecExperimentTLSMobileAssetConfig)) != 0) {
        return nil;
    }

    static NSDictionary *defaultTLSConfig = NULL;
    static dispatch_once_t onceToken = 0;
    dispatch_once(&onceToken, ^{
        NSDictionary *validate = @{
                                   @"tcp" : @{},
                                   @"tls" : @{@"max_version": @0x0303,
                                              @"false_start_enabled" : @false
                                              }
                                   };
        NSDictionary *transform = @{
                                    @"tcp" : @{},
                                    @"tls" : @{@"max_version": @0x0304,
                                               @"false_start_enabled" : @true
                                               }
                                    };
        defaultTLSConfig = @{
                             @"validate" : validate,
                             @"transform" : transform
                             };
    });

    return _CFXPCCreateXPCObjectFromCFObject((__bridge CFDictionaryRef)defaultTLSConfig);
}

// Default check to compute sampling in lieu of MobileAsset download
static bool
_device_is_in_experiment(sec_experiment_t experiment)
{
    if (experiment->sampling_disabled) {
        return YES;
    }

    uint32_t sample = arc4random();
    return (float)sample < ((float)UINT32_MAX / SEC_EXPERIMENT_SAMPLING_RATE);
}

static NSDictionary *
_copy_experiment_asset(sec_experiment_t experiment)
{
    CFErrorRef error = NULL;
    NSDictionary *config = NULL;
    NSDictionary *asset = CFBridgingRelease(SecTrustOTASecExperimentCopyAsset(&error));
    if (asset) {
        config = [asset valueForKey:[NSString stringWithUTF8String:experiment->identifier]];
    }
    return config;
}

static xpc_object_t
_copy_defaults_experiment_asset(sec_experiment_t experiment)
{
    xpc_object_t result = nil;

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if (defaults != nil) {
        NSMutableDictionary *experimentDefaults = [[defaults persistentDomainForName:[NSString stringWithUTF8String:kSecExperimentDefaultsDomain]] mutableCopy];
        if (experimentDefaults != nil) {
            NSString *key = [NSString stringWithUTF8String:experiment->identifier];
            if (experimentDefaults[key] != nil) {
                result = _CFXPCCreateXPCObjectFromCFObject((__bridge CFDictionaryRef)experimentDefaults[key]);
            }
        }
    }

    return result;
}

void
sec_experiment_set_sampling_disabled(sec_experiment_t experiment, bool sampling_disabled)
{
    experiment->sampling_disabled = sampling_disabled;
}

xpc_object_t
sec_experiment_copy_configuration(sec_experiment_t experiment)
{
    if (experiment == NULL) {
        return NULL;
    }
    /* Check first for defaults configured */
    if (!sec_experiment_is_sampling_disabled_with_default(experiment->sampling_disabled)) {
        xpc_object_t defaultAsset = _copy_defaults_experiment_asset(experiment);
        if (defaultAsset != nil) {
            return defaultAsset;
        }
    }
    /* Copy assets downloaded from MA */
    NSDictionary *asset = _copy_experiment_asset(experiment);
    if (asset != NULL) {
        /* Get random config from array of experiments */
        NSArray *array = [asset valueForKey:@"ConfigArray"];
        if (array == NULL) {
            return NULL;
        }
        NSDictionary *randomConfig = [array objectAtIndex:(arc4random() % [array count])];

        /* Only if sampling is enabled for the experiment */
        if (!experiment->sampling_disabled) {
            /* Check FleetSampleRate if device should be in experiment */
            uint32_t fleetSample = [[randomConfig objectForKey:@"FleetSampleRate"] intValue];

            /* fleetSample is a percentage value configured to determine
               percentage of devices in an experiment */
            uint32_t hostIdHash = _get_host_id_hash();
            if ((hostIdHash == 0) || (fleetSample < hostIdHash)) {
                return nil;
            }
            /* Check device sampling rate if device should run experiment */
            uint32_t samplingRate = [[randomConfig objectForKey:@"DeviceSampleRate"] intValue];
            /* Only run experiment 1 out of the samplingRate value */
            uint32_t sample = arc4random();
            if ((float)sample > ((float)UINT32_MAX / samplingRate)) {
                return nil;
            }
        }
        return _CFXPCCreateXPCObjectFromCFObject((__bridge CFDictionaryRef)randomConfig);
    }

    /* If asset download is not successful, fallback to built-in */
    if (_device_is_in_experiment(experiment)) {
        return _copy_builtin_experiment_asset(experiment);
    }
    return nil;
}

const char *
sec_experiment_get_identifier(sec_experiment_t experiment)
{
    return experiment->identifier;
}

bool
sec_experiment_run_internal(const char *experiment_name, bool sampling_disabled, dispatch_queue_t queue, sec_experiment_run_block_t run_block)
{
    if (experiment_name == NULL || queue == nil || run_block == nil) {
        return false;
    }
    
    dispatch_async(queue, ^{
        sec_experiment_t experiment = sec_experiment_create(experiment_name);
        if (experiment != nil) {
            sec_experiment_set_sampling_disabled(experiment, sec_experiment_is_sampling_disabled_with_default(sampling_disabled));
            xpc_object_t config = sec_experiment_copy_configuration(experiment);
            if (config != nil) {
                const char *identifier = sec_experiment_get_identifier(experiment);
                if (run_block(identifier, config)) {
                    os_log_info(OS_LOG_DEFAULT, "Configuration '%s' for experiment '%s' succeeded", identifier, experiment_name);
                } else {
                    os_log_info(OS_LOG_DEFAULT, "Configuration '%s' for experiment '%s' failed", identifier, experiment_name);
                }
            } else {
                os_log_debug(OS_LOG_DEFAULT, "Experiment '%s' not sampled to run", experiment_name);
            }
        } else {
            os_log_debug(OS_LOG_DEFAULT, "Experiment '%s' not found", experiment_name);
        }
    });
    
    return true;
}

bool
sec_experiment_run(const char *experiment_name, dispatch_queue_t queue, sec_experiment_run_block_t run_block)
{
    // Sampling is always enabled for SecExperiment callers. Appliations may override this by setting the
    // `disableSampling` key in the `com.apple.security.experiment` defaults domain.
    return sec_experiment_run_internal(experiment_name, false, queue, run_block);
}

@end
