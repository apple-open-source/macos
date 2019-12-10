//
//  SecExperimentTests.m
//
//

#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>
#import <CoreFoundation/CFXPCBridge.h>
#import <nw/private.h>
#import <Security/SecProtocolPriv.h>
#import "SecExperimentInternal.h"
#import "SecExperimentPriv.h"

@interface SecExperimentTests : XCTestCase
@end

@implementation SecExperimentTests

- (NSDictionary *)copyRandomConfiguration
{
    const char *testKey = "test_defaults_experiment_key";
    const char *testValue = "test_value";
    NSString *testKeyString = [NSString stringWithUTF8String:testKey];
    NSString *testValueString = [NSString stringWithUTF8String:testValue];
    NSDictionary *testConfigData = @{testKeyString: testValueString};
    NSDictionary *testConfig = @{SecExperimentConfigurationKeyFleetSampleRate : @(100),
                                 SecExperimentConfigurationKeyDeviceSampleRate : @(100),
                                 SecExperimentConfigurationKeyExperimentIdentifier : @"identifier",
                                 SecExperimentConfigurationKeyConfigurationData : testConfigData};
    return testConfig;
}

- (void)testCopyFromDefaults {
    const char *testKey = "test_defaults_experiment_key";
    const char *testValue = "test_value";
    NSString *testKeyString = [NSString stringWithUTF8String:testKey];
    NSString *testValueString = [NSString stringWithUTF8String:testValue];
    NSDictionary *testConfigData = @{testKeyString: testValueString};
    NSDictionary *testConfig = @{SecExperimentConfigurationKeyFleetSampleRate : @(100),
                                 SecExperimentConfigurationKeyDeviceSampleRate : @(100),
                                 SecExperimentConfigurationKeyExperimentIdentifier : @"identifier",
                                 SecExperimentConfigurationKeyConfigurationData : testConfigData};

    SecExperiment *mockExperiment = OCMPartialMock([[SecExperiment alloc] initWithName:kSecExperimentTLSProbe]);
    OCMStub([mockExperiment copyExperimentConfigurationFromUserDefaults]).andReturn(testConfig);
    sec_experiment_t experiment = sec_experiment_create_with_inner_experiment(mockExperiment);

    sec_experiment_set_sampling_disabled(experiment, true);
    xpc_object_t actualConfig = sec_experiment_copy_configuration(experiment);
    XCTAssertNotNil(actualConfig);

    xpc_type_t configType = xpc_get_type(actualConfig);
    XCTAssertTrue(configType == XPC_TYPE_DICTIONARY);

    const char *actualValue = xpc_dictionary_get_string(actualConfig, testKey);
    XCTAssertTrue(actualValue != NULL && strncmp(actualValue, testValue, strlen(testValue)) == 0);
}

- (void)testInitializeWithIdentifier {
    sec_experiment_t experiment = sec_experiment_create(kSecExperimentTLSProbe);
    const char *identifier = sec_experiment_get_identifier(experiment);
    XCTAssertTrue(identifier == NULL);
}

- (void)testExperimentRunAsynchronously_SkipSampling {
    dispatch_queue_t test_queue = dispatch_queue_create("test_queue", NULL);

    __block dispatch_semaphore_t blocker = dispatch_semaphore_create(0);
    __block bool experiment_invoked = false;
    sec_experiment_run_block_t run_block = ^bool(const char *identifier, xpc_object_t config) {
        experiment_invoked = identifier != NULL && config != NULL;
        dispatch_semaphore_signal(blocker);
        return experiment_invoked;
    };

    NSDictionary *testConfig = [self copyRandomConfiguration];

    SecExperiment *mockExperiment = OCMPartialMock([[SecExperiment alloc] initWithName:kSecExperimentTLSProbe]);
    OCMStub([mockExperiment copyExperimentConfigurationFromUserDefaults]).andReturn(testConfig);
    sec_experiment_t experiment = sec_experiment_create_with_inner_experiment(mockExperiment);

    sec_experiment_run_internal(experiment, true, test_queue, run_block, nil, false);
    XCTAssertTrue(dispatch_semaphore_wait(blocker, dispatch_time(DISPATCH_TIME_NOW, (uint64_t)(5 * NSEC_PER_SEC))) == 0L);
    XCTAssertTrue(experiment_invoked);
}

- (void)testExperimentRun_SkipWithoutConfig {
    SecExperiment *mockExperiment = OCMPartialMock([[SecExperiment alloc] initWithName:kSecExperimentTLSProbe]);
    OCMStub([mockExperiment experimentIsAllowedForProcess]).andReturn(YES);
    OCMStub([mockExperiment copyRemoteExperimentAsset]).andReturn(nil);
    OCMStub([mockExperiment copyExperimentConfigurationFromUserDefaults]).andReturn(nil);
    sec_experiment_t experiment = sec_experiment_create_with_inner_experiment(mockExperiment);

    dispatch_queue_t test_queue = dispatch_queue_create("test_queue", NULL);
    __block bool run = false;
    sec_experiment_run_block_t run_block = ^bool(__unused const char *identifier, __unused xpc_object_t config) {
        run = true;
        return true;
    };
    __block BOOL skipped = false;
    sec_experiment_skip_block_t skip_block = ^(__unused const char *identifier) {
        skipped = true;
    };

    bool result = sec_experiment_run_internal(experiment, true, test_queue, run_block, skip_block, true);
    XCTAssertTrue(result);
    XCTAssertTrue(skipped);
    XCTAssertFalse(run);
}

- (void)testExperimentRunSuccess {
    NSDictionary *testConfig = [self copyRandomConfiguration];

    SecExperiment *mockExperiment = OCMPartialMock([[SecExperiment alloc] initWithName:kSecExperimentTLSProbe]);
    OCMStub([mockExperiment isSamplingDisabled]).andReturn(YES);
    OCMStub([mockExperiment experimentIsAllowedForProcess]).andReturn(YES);
    OCMStub([mockExperiment copyRemoteExperimentAsset]).andReturn(testConfig);
    OCMStub([mockExperiment copyExperimentConfigurationFromUserDefaults]).andReturn(nil);
    OCMStub([mockExperiment copyRandomExperimentConfigurationFromAsset:[OCMArg any]]).andReturn(testConfig);
    sec_experiment_t experiment = sec_experiment_create_with_inner_experiment(mockExperiment);

    dispatch_queue_t test_queue = dispatch_queue_create("test_queue", NULL);
    __block bool run = false;
    sec_experiment_run_block_t run_block = ^bool(__unused const char *identifier, __unused xpc_object_t config) {
        run = true;
        return true; // Signal that the experiment run was successful
    };
    __block BOOL skipped = false;
    sec_experiment_skip_block_t skip_block = ^(__unused const char *identifier) {
        skipped = true;
    };

    bool result = sec_experiment_run_internal(experiment, true, test_queue, run_block, skip_block, true);
    XCTAssertTrue(result);
    XCTAssertFalse(skipped);
    XCTAssertTrue(run);
    XCTAssertTrue(sec_experiment_get_run_count(experiment) == 1);
    XCTAssertTrue(sec_experiment_get_successful_run_count(experiment) == 1);
}

- (void)testExperimentRunFailure {
    NSDictionary *testConfig = [self copyRandomConfiguration];

    SecExperiment *mockExperiment = OCMPartialMock([[SecExperiment alloc] initWithName:kSecExperimentTLSProbe]);
    OCMStub([mockExperiment experimentIsAllowedForProcess]).andReturn(YES);
    OCMStub([mockExperiment isSamplingDisabled]).andReturn(YES);
    OCMStub([mockExperiment copyRemoteExperimentAsset]).andReturn(testConfig);
    OCMStub([mockExperiment copyExperimentConfigurationFromUserDefaults]).andReturn(nil);
    OCMStub([mockExperiment copyRandomExperimentConfigurationFromAsset:[OCMArg any]]).andReturn(testConfig);
    sec_experiment_t experiment = sec_experiment_create_with_inner_experiment(mockExperiment);

    dispatch_queue_t test_queue = dispatch_queue_create("test_queue", NULL);
    __block bool run = false;
    sec_experiment_run_block_t run_block = ^bool(__unused const char *identifier, __unused xpc_object_t config) {
        run = true;
        return false; // Signal that the experiment run failed
    };
    __block BOOL skipped = false;
    sec_experiment_skip_block_t skip_block = ^(__unused const char *identifier) {
        skipped = true;
    };

    bool result = sec_experiment_run_internal(experiment, true, test_queue, run_block, skip_block, true);
    XCTAssertTrue(result);
    XCTAssertFalse(skipped);
    XCTAssertTrue(run);
    XCTAssertTrue(sec_experiment_get_run_count(experiment) == 1);
    XCTAssertTrue(sec_experiment_get_successful_run_count(experiment) == 0);
}

- (void)testExperimentRun_SamplingEnabled_NotInFleet {
    size_t fleetNumber = 10;
    NSDictionary *testConfig = @{SecExperimentConfigurationKeyFleetSampleRate: @(fleetNumber), SecExperimentConfigurationKeyDeviceSampleRate : @(2)};

    SecExperimentConfig *mockConfig = OCMPartialMock([[SecExperimentConfig alloc] initWithConfiguration:testConfig]);
    OCMStub([mockConfig hostHash]).andReturn(fleetNumber + 1); // Ensure that fleetNumber < hostHash

    SecExperiment *mockExperiment = OCMPartialMock([[SecExperiment alloc] initWithName:kSecExperimentTLSProbe]);
    OCMStub([mockExperiment experimentIsAllowedForProcess]).andReturn(YES);
    OCMStub([mockExperiment isSamplingDisabled]).andReturn(NO);
    OCMStub([mockExperiment copyExperimentConfiguration]).andReturn(mockConfig);

    sec_experiment_t experiment = sec_experiment_create_with_inner_experiment(mockExperiment);

    dispatch_queue_t test_queue = dispatch_queue_create("test_queue", NULL);
    __block bool run = false;
    sec_experiment_run_block_t run_block = ^bool(__unused const char *identifier, __unused xpc_object_t config) {
        run = true;
        return false; // Signal that the experiment run failed
    };
    __block BOOL skipped = false;
    sec_experiment_skip_block_t skip_block = ^(__unused const char *identifier) {
        skipped = true;
    };

    bool result = sec_experiment_run_internal(experiment, true, test_queue, run_block, skip_block, true);
    XCTAssertTrue(result);
    XCTAssertTrue(skipped);
    XCTAssertFalse(run);
    XCTAssertTrue(sec_experiment_get_run_count(experiment) == 0);
    XCTAssertTrue(sec_experiment_get_successful_run_count(experiment) == 0);
}

- (void)testExperimentRun_SamplingEnabled_InFleetNotInSample {
    size_t fleetNumber = 10;
    NSDictionary *testConfig = @{SecExperimentConfigurationKeyFleetSampleRate: @(fleetNumber), SecExperimentConfigurationKeyDeviceSampleRate : @(2)};

    SecExperimentConfig *mockConfig = OCMPartialMock([[SecExperimentConfig alloc] initWithConfiguration:testConfig]);
    OCMStub([mockConfig hostHash]).andReturn(fleetNumber - 1); // Ensure that fleetNumber > hostHash
    OCMStub([mockConfig shouldRunWithSamplingRate:[OCMArg any]]).andReturn(NO); // Determine that we're not in the fleet

    SecExperiment *mockExperiment = OCMPartialMock([[SecExperiment alloc] initWithName:kSecExperimentTLSProbe]);
    OCMStub([mockExperiment experimentIsAllowedForProcess]).andReturn(YES);
    OCMStub([mockExperiment isSamplingDisabled]).andReturn(NO);
    OCMStub([mockExperiment copyExperimentConfiguration]).andReturn(mockConfig);

    sec_experiment_t experiment = sec_experiment_create_with_inner_experiment(mockExperiment);

    dispatch_queue_t test_queue = dispatch_queue_create("test_queue", NULL);
    __block bool run = false;
    sec_experiment_run_block_t run_block = ^bool(__unused const char *identifier, __unused xpc_object_t config) {
        run = true;
        return false; // Signal that the experiment run failed
    };
    __block BOOL skipped = false;
    sec_experiment_skip_block_t skip_block = ^(__unused const char *identifier) {
        skipped = true;
    };

    bool result = sec_experiment_run_internal(experiment, true, test_queue, run_block, skip_block, true);
    XCTAssertTrue(result);
    XCTAssertTrue(skipped);
    XCTAssertFalse(run);
    XCTAssertTrue(sec_experiment_get_run_count(experiment) == 0);
    XCTAssertTrue(sec_experiment_get_successful_run_count(experiment) == 0);
}

- (void)testExperimentRun_DisallowedProcess {
    SecExperiment *mockExperiment = OCMPartialMock([[SecExperiment alloc] initWithName:kSecExperimentTLSProbe]);
    OCMStub([mockExperiment experimentIsAllowedForProcess]).andReturn(NO);
    sec_experiment_t experiment = sec_experiment_create_with_inner_experiment(mockExperiment);

    dispatch_queue_t test_queue = dispatch_queue_create("test_queue", NULL);
    sec_experiment_run_block_t run_block = ^bool(__unused const char *identifier, __unused xpc_object_t config) {
        return true;
    };

    bool result = sec_experiment_run_internal(experiment, true, test_queue, run_block, nil, false);
    XCTAssertFalse(result);
}

- (void)testExperimentRunSynchronously_SkipSampling {
    dispatch_queue_t test_queue = dispatch_queue_create("test_queue", NULL);

    __block bool experiment_invoked = false;
    sec_experiment_run_block_t run_block = ^bool(const char *identifier, xpc_object_t config) {
        experiment_invoked = identifier != NULL && config != NULL;
        return experiment_invoked;
    };

    NSDictionary *testConfig = [self copyRandomConfiguration];

    SecExperiment *mockExperiment = OCMPartialMock([[SecExperiment alloc] initWithName:kSecExperimentTLSProbe]);
    OCMStub([mockExperiment copyExperimentConfigurationFromUserDefaults]).andReturn(testConfig);
    sec_experiment_t experiment = sec_experiment_create_with_inner_experiment(mockExperiment);

    sec_experiment_run_internal(experiment, true, test_queue, run_block, nil, true);
    XCTAssertTrue(experiment_invoked);
}

- (void)testDefaultsConfigCopy {
    const char *testKey = "test_defaults_experiment_key";
    const char *testValue = "test_value";
    NSString *testKeyString = [NSString stringWithUTF8String:testKey];
    NSString *testValueString = [NSString stringWithUTF8String:testValue];
    NSDictionary *testConfigData = @{testKeyString: testValueString};
    NSString *experimentName = @"TestExperiment";
    NSDictionary *testConfig = @{experimentName: @{SecExperimentConfigurationKeyFleetSampleRate : @(100),
                                                   SecExperimentConfigurationKeyDeviceSampleRate : @(100),
                                                   SecExperimentConfigurationKeyExperimentIdentifier : @"identifier",
                                                   SecExperimentConfigurationKeyConfigurationData : testConfigData}};

    sec_experiment_t experiment = sec_experiment_create([experimentName UTF8String]);
    XCTAssert(experiment, @"sec_experiment_create");

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults setPersistentDomain:testConfig forName:[NSString stringWithUTF8String:kSecExperimentDefaultsDomain]];

    sec_experiment_set_sampling_disabled(experiment, true);
    xpc_object_t tlsconfig = sec_experiment_copy_configuration(experiment);
    XCTAssertNotNil(tlsconfig);
    if (tlsconfig) {
        XCTAssertTrue(xpc_get_type(tlsconfig) == XPC_TYPE_DICTIONARY);
        if (xpc_get_type(tlsconfig) == XPC_TYPE_DICTIONARY) {
            const char *actualValue = xpc_dictionary_get_string(tlsconfig, testKey);
            XCTAssertTrue(actualValue != NULL);
            if (actualValue != NULL) {
                XCTAssertTrue(strcmp(actualValue, testValue) == 0);
            }
        }
    }

    // Clear the persistent domain
    [defaults removePersistentDomainForName:[NSString stringWithUTF8String:kSecExperimentDefaultsDomain]];
}

- (void)testRunWithIdentifier {
    const char *testKey = "test_defaults_experiment_key";
    const char *testValue = "test_value";
    NSString *testKeyString = [NSString stringWithUTF8String:testKey];
    NSString *testValueString = [NSString stringWithUTF8String:testValue];
    NSDictionary *testConfigData = @{testKeyString: testValueString};
    NSString *experimentName = @"TestExperiment";
    NSString *identifierName = @"ExperimentIdentifier";
    NSDictionary *testConfig = @{experimentName: @{SecExperimentConfigurationKeyFleetSampleRate : @(100),
                                                   SecExperimentConfigurationKeyDeviceSampleRate : @(100),
                                                   SecExperimentConfigurationKeyExperimentIdentifier : identifierName,
                                                   SecExperimentConfigurationKeyConfigurationData : testConfigData}};

    SecExperiment *mockExperiment = OCMPartialMock([[SecExperiment alloc] initWithName:[experimentName UTF8String]]);
    OCMStub([mockExperiment copyExperimentConfigurationFromUserDefaults]).andReturn(testConfig);
    sec_experiment_t experiment = sec_experiment_create_with_inner_experiment(mockExperiment);

    sec_experiment_run_internal(experiment, true, nil, ^bool(const char * _Nonnull identifier, xpc_object_t  _Nonnull experiment_config) {
        XCTAssertTrue(identifier != NULL);
        if (identifier != NULL) {
            XCTAssertTrue(strcmp([identifierName UTF8String], identifier) == 0);
        }
    }, ^(const char * _Nonnull identifier) {
        XCTAssertFalse(true);
    }, true);
}

- (void)testGeneration {
    nw_protocol_options_t tls_options = nw_tls_create_options();
    sec_protocol_options_t sec_options = nw_tls_copy_sec_protocol_options(tls_options);

    sec_protocol_options_set_tls_grease_enabled(sec_options, true);
    xpc_object_t config = sec_protocol_options_create_config(sec_options);

    xpc_dictionary_apply(config, ^bool(const char * _Nonnull key, xpc_object_t  _Nonnull value) {
        if (xpc_get_type(value) == XPC_TYPE_BOOL) {
            NSLog(@"%s -> %d", key, xpc_bool_get_value(value));
        } else if (xpc_get_type(value) == XPC_TYPE_UINT64) {
            NSLog(@"%s -> %zu", key, (size_t)xpc_uint64_get_value(value));
        } else {
            NSLog(@"%s -> %d", key, (int)xpc_get_type(value));
        }

        return true;
    });

    CFDictionaryRef config_dictionary = _CFXPCCreateCFObjectFromXPCObject(config);
    XCTAssertTrue(config_dictionary != NULL);
    NSLog(@"%@", (__bridge NSDictionary *)config_dictionary);
}

@end
