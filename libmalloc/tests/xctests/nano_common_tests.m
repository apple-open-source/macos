#import "internal.h"

#if CONFIG_NANOZONE

#import <XCTest/XCTest.h>

// from nano_malloc_common.c
extern nano_version_t
_nano_common_init_pick_mode(const char *envp[], const char *apple[], const char *bootargs, bool space_efficient_enabled);

// Test stubs
bool malloc_space_efficient_enabled;
unsigned int phys_ncpus;

@interface nano_common_tests : XCTestCase {
@private

}
@end

@implementation nano_common_tests

- (nano_version_t)pickModeWithEnv:(const char *)env
							apple:(const char *)apple
						  bootarg:(const char *)bootarg
				  space_efficient:(bool)space_efficient_enabled {
	// Create the null terminated array
	const char *envp[] = { env, NULL};
	const char *applep[] = { apple, NULL};

	return _nano_common_init_pick_mode(envp, applep, bootarg, space_efficient_enabled);
}

- (void)testNanoEnabledIfBootargForcedOn {
	XCTAssertEqual([self pickModeWithEnv:NULL apple:NULL bootarg:"nanov2_mode=forced" space_efficient:false], NANO_V2);
}

- (void)testNanoEnabledIfBootargConditionalAndNotSpaceEfficient {
	XCTAssertEqual([self pickModeWithEnv:NULL apple:NULL bootarg:"nanov2_mode=conditional" space_efficient:false], NANO_V2);
}

- (void)testNanoDisabledIfBootargConditionalAndSpaceEfficient {
	XCTAssertEqual([self pickModeWithEnv:NULL apple:NULL bootarg:"nanov2_mode=conditional" space_efficient:true], NANO_NONE);
}

- (void)testNanoEnvironmentEnableOverridesConditionalSpaceEfficient {
	const char *environments[] = {
		"MallocNanoZone=1",
		"MallocNanoZone=v1",
		"MallocNanoZone=V1",
		"MallocNanoZone=v2",
		"MallocNanoZone=V2",
	};
	for (size_t i = 0; i < countof(environments); i++) {
		XCTAssertEqual([self pickModeWithEnv:environments[i] apple:NULL bootarg:"nanov2_mode=conditional" space_efficient:true], NANO_V2);
	}
}

- (void)testNanoEnvironmentDisableOverridesConditionalSpaceEfficient {
	XCTAssertEqual([self pickModeWithEnv:"MallocNanoZone=0" apple:NULL bootarg:"nanov2_mode=conditional" space_efficient:false], NANO_NONE);
}

- (void)testNanoEnvironmentDisableOverridesApple {
	XCTAssertEqual([self pickModeWithEnv:"MallocNanoZone=0" apple:"MallocNanoZone=1" bootarg:"nanov2_mode=enabled" space_efficient:false], NANO_NONE);
}

- (void)testNanoEnabledIfAppleEnabled {
	// Need to set boot-arg for platforms with the default of conditional
	XCTAssertEqual([self pickModeWithEnv:NULL apple:"MallocNanoZone=1" bootarg:"nanov2_mode=enabled" space_efficient:true], NANO_V2);
}

#if MALLOC_TARGET_IOS || TARGET_OS_DRIVERKIT
// NANOV2_DEFAULT_MODE == NANO_ENABLED

- (void)testDefaultNotSpaceEfficient {
	XCTAssertEqual([self pickModeWithEnv:NULL apple:NULL bootarg:"" space_efficient:false], NANO_NONE);
}

#else // MALLOC_TARGET_IOS || TARGET_OS_DRIVERKIT
// NANOV2_DEFAULT_MODE == NANO_CONDITIONAL

- (void)testDefaultNotSpaceEfficient {
	XCTAssertEqual([self pickModeWithEnv:NULL apple:NULL bootarg:"" space_efficient:false], NANO_V2);
}

#endif // MALLOC_TARGET_IOS || TARGET_OS_DRIVERKIT

@end


#endif // CONFIG_NANOZONE
