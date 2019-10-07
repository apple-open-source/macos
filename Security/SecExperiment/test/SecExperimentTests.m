//
//  SecExperimentTests.m
//
//

#import <XCTest/XCTest.h>
#import "SecExperimentInternal.h"
#import "SecExperimentPriv.h"

@interface SecExperimentTests : XCTestCase
@end

@implementation SecExperimentTests

- (void)testCStyleGetTlsConfig {
    sec_experiment_t experiment = sec_experiment_create(kSecExperimentTLSMobileAssetConfig);
    sec_experiment_set_sampling_disabled(experiment, true);
    XCTAssert(experiment, @"sec_experiment_create");

    xpc_object_t tlsconfig = nil;
    tlsconfig = sec_experiment_copy_configuration(experiment);
    XCTAssertNotNil(tlsconfig);

}

- (void)testCStyleGetTlsConfig_SkipSampling {
    sec_experiment_t experiment = sec_experiment_create(kSecExperimentTLSMobileAssetConfig);
    XCTAssert(experiment, @"sec_experiment_create");

    sec_experiment_set_sampling_disabled(experiment, true);
    xpc_object_t tlsconfig = sec_experiment_copy_configuration(experiment);
    XCTAssertNotNil(tlsconfig);
}

- (void)testExperimentRun_SkipSamping {
    dispatch_queue_t test_queue = dispatch_queue_create("test_queue", NULL);
    
    __block dispatch_semaphore_t blocker = dispatch_semaphore_create(0);
    __block bool experiment_invoked = false;
    sec_experiment_run_block_t run_block = ^bool(const char *identifier, xpc_object_t config) {
        experiment_invoked = identifier != NULL && config != NULL;
        dispatch_semaphore_signal(blocker);
        return experiment_invoked;
    };
    
    sec_experiment_run_internal(kSecExperimentTLSMobileAssetConfig, true, test_queue, run_block);
    XCTAssertTrue(dispatch_semaphore_wait(blocker, dispatch_time(DISPATCH_TIME_NOW, (uint64_t)(5 * NSEC_PER_SEC))) == 0L);
    XCTAssertTrue(experiment_invoked);
}

- (void)testDefaultsConfigCopy {
    const char *test_key = "test_defaults_experiment_key";
    const char *test_value = "test_value";

    sec_experiment_t experiment = sec_experiment_create(test_key);
    XCTAssert(experiment, @"sec_experiment_create");

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    NSString *test_key_string = [NSString stringWithUTF8String:test_key];
    NSString *test_value_string = [NSString stringWithUTF8String:test_value];
    NSDictionary *testConfig = @{test_key_string: test_value_string};
    [defaults setPersistentDomain:testConfig forName:[NSString stringWithUTF8String:kSecExperimentDefaultsDomain]];

    sec_experiment_set_sampling_disabled(experiment, true);
    xpc_object_t tlsconfig = sec_experiment_copy_configuration(experiment);
    XCTAssertNotNil(tlsconfig);
    XCTAssertTrue(xpc_get_type(tlsconfig) == XPC_TYPE_STRING);
    if (xpc_get_type(tlsconfig) == XPC_TYPE_STRING) {
        XCTAssertTrue(strcmp(xpc_string_get_string_ptr(tlsconfig), test_value) == 0);
    }

    // Clear the persistent domain
    [defaults removePersistentDomainForName:[NSString stringWithUTF8String:kSecExperimentDefaultsDomain]];
}

@end
