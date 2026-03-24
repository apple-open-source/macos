#import "internal.h"

#import <XCTest/XCTest.h>

@interface malloc_config_tests : XCTestCase {
}

@end

@implementation malloc_config_tests

// Helper method to create a basic config input
- (malloc_config_input_s)createBasicInput {
	malloc_config_input_s input = {};
	input.mci_target_platform = MALLOC_TARGET_PLATFORM_MAC;
	input.mci_target_platform_variant = MALLOC_TARGET_PLATFORM_VARIANT_NORMAL;
	input.mci_arch_lp64 = true;
	input.mci_arch_intel = false;
	input.mci_nano_version = NANO_V2;
	input.mci_zero_policy = MALLOC_ZERO_ON_FREE;
	input.mci_allow_internal_security_policy = false;
	return input;
}

// Helper method to create environment variables array
- (const char **)createEnvArray:(NSDictionary *)env {
	NSMutableArray *envArray = [NSMutableArray array];
	for (NSString *key in env) {
		NSString *envVar = [NSString stringWithFormat:@"%@=%@", key, env[key]];
		[envArray addObject:envVar];
	}

	const char **envp = malloc(sizeof(char *) * ([envArray count] + 1));
	for (NSUInteger i = 0; i < [envArray count]; i++) {
		envp[i] = strdup([envArray[i] UTF8String]);
	}
	envp[[envArray count]] = NULL;
	return envp;
}

- (void)freeEnvArray:(const char **)envp {
	if (!envp) return;
	for (int i = 0; envp[i]; i++) {
		free((void *)envp[i]);
	}
	free(envp);
}

#pragma mark - Process Identity Tests

- (void)testProcessIdentityLaunchdPid1 {
	malloc_config_input_s input = [self createBasicInput];
	input.mci_is_pid_1 = true;
	input.mci_progname = "any_process";

	malloc_config_result_s result = malloc_config_from_input(&input);
	XCTAssertEqual(result.mcr_process_identity, MALLOC_PROCESS_LAUNCHD);
}

- (void)testProcessIdentityLaunchdByName {
	malloc_config_input_s input = [self createBasicInput];
	input.mci_is_pid_1 = false;
	input.mci_progname = "launchd";

	malloc_config_result_s result = malloc_config_from_input(&input);
	XCTAssertEqual(result.mcr_process_identity, MALLOC_PROCESS_LAUNCHD);
}

- (void)testProcessIdentityLaunchdVariants {
	NSArray *launchdNames = @[@"launchd_sim", @"launchd_sim.development", @"launchd_sim.debug",
							  @"launchd.development", @"launchd.debug", @"launchd.testing"];

	for (NSString *name in launchdNames) {
		malloc_config_input_s input = [self createBasicInput];
		input.mci_is_pid_1 = false;
		input.mci_progname = [name UTF8String];

		malloc_config_result_s result = malloc_config_from_input(&input);
		XCTAssertEqual(result.mcr_process_identity, MALLOC_PROCESS_LAUNCHD, @"Failed for %@", name);
	}
}

- (void)testProcessIdentitySystemDaemons {
	NSDictionary *processes = @{
		@"logd": @(MALLOC_PROCESS_LOGD),
		@"notifyd": @(MALLOC_PROCESS_NOTIFYD),
		@"mediaparserd": @(MALLOC_PROCESS_MEDIAPARSERD),
		@"videocodecd": @(MALLOC_PROCESS_VIDEOCODECD),
		@"mediaplaybackd": @(MALLOC_PROCESS_MEDIAPLAYBACKD),
		@"audiomxd": @(MALLOC_PROCESS_AUDIOMXD),
		@"avconferenced": @(MALLOC_PROCESS_AVCONFERENCED),
		@"mediaserverd": @(MALLOC_PROCESS_MEDIASERVERD),
		@"cameracaptured": @(MALLOC_PROCESS_CAMERACAPTURED)
	};

	for (NSString *name in processes) {
		malloc_config_input_s input = [self createBasicInput];
		input.mci_progname = [name UTF8String];

		malloc_config_result_s result = malloc_config_from_input(&input);
		XCTAssertEqual(result.mcr_process_identity, [processes[name] intValue], @"Failed for %@", name);
	}
}

- (void)testProcessIdentityHardenedRuntimeBrowser {
	malloc_config_input_s input = [self createBasicInput];
	input.mci_progname = "test_process";

	const char *apple_array[] = {"HardenedRuntime=15", NULL};
	input.mci_apple_array = apple_array;

	malloc_config_result_s result = malloc_config_from_input(&input);
	XCTAssertEqual(result.mcr_process_identity, MALLOC_PROCESS_BROWSER);
}

- (void)assertProcessIdentity:(malloc_process_identity_t)identity
		forProgname:(const char *)name
		onPlatform:(malloc_target_platform_t)tp
		withSecurityConfig:(os_security_config_t)security_config {
	malloc_config_input_s input = [self createBasicInput];
	input.mci_progname = name;
	input.mci_target_platform = tp;
	input.mci_os_security_config = security_config;

	malloc_config_result_s result = malloc_config_from_input(&input);
	XCTAssertEqual(result.mcr_process_identity, identity);
}

- (void)testProcessIdentityAlterateBrowserUIHosts {
	// Web.app: only on iOS with guard objects
	[self assertProcessIdentity:MALLOC_PROCESS_NONE
			forProgname:"Web"
			onPlatform:MALLOC_TARGET_PLATFORM_MAC
			withSecurityConfig:OS_SECURITY_CONFIG_NONE];

	[self assertProcessIdentity:MALLOC_PROCESS_NONE
			forProgname:"Web"
			onPlatform:MALLOC_TARGET_PLATFORM_MAC
			withSecurityConfig:OS_SECURITY_CONFIG_GUARD_OBJECTS];

	[self assertProcessIdentity:MALLOC_PROCESS_NONE
			forProgname:"Web"
			onPlatform:MALLOC_TARGET_PLATFORM_IOS
			withSecurityConfig:OS_SECURITY_CONFIG_NONE];

	[self assertProcessIdentity:MALLOC_PROCESS_BROWSER
			forProgname:"Web"
			onPlatform:MALLOC_TARGET_PLATFORM_IOS
			withSecurityConfig:OS_SECURITY_CONFIG_GUARD_OBJECTS];

	// WebSheet: only iOS or visionOS with guard objects
	[self assertProcessIdentity:MALLOC_PROCESS_NONE
			forProgname:"WebSheet"
			onPlatform:MALLOC_TARGET_PLATFORM_MAC
			withSecurityConfig:OS_SECURITY_CONFIG_NONE];

	[self assertProcessIdentity:MALLOC_PROCESS_NONE
			forProgname:"WebSheet"
			onPlatform:MALLOC_TARGET_PLATFORM_IOS
			withSecurityConfig:OS_SECURITY_CONFIG_NONE];

	[self assertProcessIdentity:MALLOC_PROCESS_BROWSER
			forProgname:"WebSheet"
			onPlatform:MALLOC_TARGET_PLATFORM_IOS
			withSecurityConfig:OS_SECURITY_CONFIG_GUARD_OBJECTS];

	[self assertProcessIdentity:MALLOC_PROCESS_NONE
			forProgname:"WebSheet"
			onPlatform:MALLOC_TARGET_PLATFORM_VISION
			withSecurityConfig:OS_SECURITY_CONFIG_NONE];

	[self assertProcessIdentity:MALLOC_PROCESS_BROWSER
			forProgname:"WebSheet"
			onPlatform:MALLOC_TARGET_PLATFORM_VISION
			withSecurityConfig:OS_SECURITY_CONFIG_GUARD_OBJECTS];

	// Web App.app: only on macOS with guard objects
	[self assertProcessIdentity:MALLOC_PROCESS_NONE
			forProgname:"Web App"
			onPlatform:MALLOC_TARGET_PLATFORM_MAC
			withSecurityConfig:OS_SECURITY_CONFIG_NONE];

	[self assertProcessIdentity:MALLOC_PROCESS_BROWSER
			forProgname:"Web App"
			onPlatform:MALLOC_TARGET_PLATFORM_MAC
			withSecurityConfig:OS_SECURITY_CONFIG_GUARD_OBJECTS];

	[self assertProcessIdentity:MALLOC_PROCESS_NONE
			forProgname:"Web App"
			onPlatform:MALLOC_TARGET_PLATFORM_IOS
			withSecurityConfig:OS_SECURITY_CONFIG_NONE];

	[self assertProcessIdentity:MALLOC_PROCESS_NONE
			forProgname:"Web App"
			onPlatform:MALLOC_TARGET_PLATFORM_IOS
			withSecurityConfig:OS_SECURITY_CONFIG_GUARD_OBJECTS];
}

- (void)testProcessIdentityHardenedRuntimeNonBrowser {
	malloc_config_input_s input = [self createBasicInput];
	input.mci_progname = "test_process";

	const char *apple_array[] = {"HardenedRuntime=16", NULL};
	input.mci_apple_array = apple_array;

	malloc_config_result_s result = malloc_config_from_input(&input);
	XCTAssertEqual(result.mcr_process_identity, MALLOC_PROCESS_NONE);
}

- (void)testProcessIdentityVisionPlatformSpecific {
	NSDictionary *processes = @{
		@"presenced": @(MALLOC_PROCESS_VISION_PRESENCED),
		@"FaceTime": @(MALLOC_PROCESS_VISION_FACETIME),
		@"managedassetsd": @(MALLOC_PROCESS_VISION_MANAGEDASSETSD),
		@"polarisd": @(MALLOC_PROCESS_VISION_POLARISD),
		@"arkitd": @(MALLOC_PROCESS_VISION_ARKITD),
		@"backboardd": @(MALLOC_PROCESS_VISION_BACKBOARDD),
		@"realitycamerad": @(MALLOC_PROCESS_VISION_REALITYCAMERAD)
	};

	for (NSString *name in processes) {
		malloc_config_input_s input = [self createBasicInput];
		input.mci_target_platform = MALLOC_TARGET_PLATFORM_VISION;
		input.mci_progname = [name UTF8String];

		malloc_config_result_s result = malloc_config_from_input(&input);
		XCTAssertEqual(result.mcr_process_identity, [processes[name] intValue], @"Failed for %@", name);
	}
}

- (void)testProcessIdentityVisionPlatformSpecificWrongPlatform {
	malloc_config_input_s input = [self createBasicInput];
	input.mci_target_platform = MALLOC_TARGET_PLATFORM_IOS;
	input.mci_progname = "presenced";

	malloc_config_result_s result = malloc_config_from_input(&input);
	XCTAssertEqual(result.mcr_process_identity, MALLOC_PROCESS_NONE);
}

- (void)testProcessIdentityWatchPlatformSpecific {
	NSDictionary *processes = @{
		@"backboardd": @(MALLOC_PROCESS_WATCH_BACKBOARDD),
		@"ClockFace": @(MALLOC_PROCESS_WATCH_CLOCKFACE)
	};

	for (NSString *name in processes) {
		malloc_config_input_s input = [self createBasicInput];
		input.mci_target_platform = MALLOC_TARGET_PLATFORM_WATCH;
		input.mci_progname = [name UTF8String];

		malloc_config_result_s result = malloc_config_from_input(&input);
		XCTAssertEqual(result.mcr_process_identity, [processes[name] intValue], @"Failed for %@", name);
	}
}

- (void)testProcessIdentityHardenedHeapConfig {
	malloc_config_input_s input = [self createBasicInput];
	input.mci_progname = "unknown_process";
	input.mci_os_security_config = OS_SECURITY_CONFIG_HARDENED_HEAP;

	malloc_config_result_s result = malloc_config_from_input(&input);
	XCTAssertEqual(result.mcr_process_identity, MALLOC_PROCESS_HARDENED_HEAP_CONFIG);
}

- (void)testProcessIdentityNameOverridesHardenedHeapConfig {
	malloc_config_input_s input = [self createBasicInput];
	input.mci_progname = "launchd";
	input.mci_os_security_config = OS_SECURITY_CONFIG_HARDENED_HEAP;

	malloc_config_result_s result = malloc_config_from_input(&input);
	XCTAssertEqual(result.mcr_process_identity, MALLOC_PROCESS_LAUNCHD);
}

- (void)testProcessIdentityUnknownProcess {
	malloc_config_input_s input = [self createBasicInput];
	input.mci_progname = "unknown_process";

	malloc_config_result_s result = malloc_config_from_input(&input);
	XCTAssertEqual(result.mcr_process_identity, MALLOC_PROCESS_NONE);
}

#pragma mark - Xzone Malloc Enablement Tests

- (void)testXzoneDisabledNot64Bit {
	malloc_config_input_s input = [self createBasicInput];
	input.mci_arch_lp64 = false;
	input.mci_progname = "test_process";

	malloc_config_result_s result = malloc_config_from_input(&input);
	XCTAssertFalse(result.mcr_enable_xzone_malloc);
	XCTAssertTrue(strcmp(result.mcr_xzone_enablement_reason, "not LP64") == 0);
}

- (void)testXzoneEnvironmentOverride {
	malloc_config_input_s input = [self createBasicInput];
	input.mci_progname = "test_process";
	input.mci_allow_internal_security_policy = true;

	NSDictionary *env = @{@"MallocSecureAllocator": @"1"};
	input.mci_envp = [self createEnvArray:env];

	malloc_config_result_s result = malloc_config_from_input(&input);
	XCTAssertTrue(result.mcr_enable_xzone_malloc);
	XCTAssertTrue(strcmp(result.mcr_xzone_enablement_reason, "MallocSecureAllocator in environment") == 0);

	[self freeEnvArray:input.mci_envp];
}

- (void)testXzoneEnvironmentOverrideDisabled {
	malloc_config_input_s input = [self createBasicInput];
	input.mci_progname = "test_process";
	input.mci_allow_internal_security_policy = true;

	NSDictionary *env = @{@"MallocSecureAllocator": @"0"};
	input.mci_envp = [self createEnvArray:env];

	malloc_config_result_s result = malloc_config_from_input(&input);
	XCTAssertFalse(result.mcr_enable_xzone_malloc);
	XCTAssertTrue(strcmp(result.mcr_xzone_enablement_reason, "MallocSecureAllocator in environment") == 0);

	[self freeEnvArray:input.mci_envp];
}

- (void)testXzoneEnvironmentOverrideIgnoredWithoutInternalSecurity {
	malloc_config_input_s input = [self createBasicInput];
	input.mci_progname = "test_process";
	input.mci_allow_internal_security_policy = false;

	NSDictionary *env = @{@"MallocSecureAllocator": @"1"};
	input.mci_envp = [self createEnvArray:env];

	malloc_config_result_s result = malloc_config_from_input(&input);
	XCTAssertTrue(result.mcr_enable_xzone_malloc);
	XCTAssertTrue(strcmp(result.mcr_xzone_enablement_reason, "default") == 0);

	[self freeEnvArray:input.mci_envp];
}

- (void)testXzoneEnabledTvOS {
	malloc_config_input_s input = [self createBasicInput];
	input.mci_target_platform = MALLOC_TARGET_PLATFORM_TV;
	input.mci_progname = "test_process";

	malloc_config_result_s result = malloc_config_from_input(&input);
	XCTAssertTrue(result.mcr_enable_xzone_malloc);
	XCTAssertTrue(strcmp(result.mcr_xzone_enablement_reason, "default") == 0);
}

- (void)testXzoneEnabledTvOSFeatureFlag {
	malloc_config_input_s input = [self createBasicInput];
	input.mci_target_platform = MALLOC_TARGET_PLATFORM_TV;
	input.mci_progname = "test_process";
	input.mci_allow_internal_security_policy = true;
	input.mci_feature_flags[MALLOC_FEATURE_FLAG_TVOS_ENABLEMENT] = true;

	malloc_config_result_s result = malloc_config_from_input(&input);
	XCTAssertTrue(result.mcr_enable_xzone_malloc);
	XCTAssertTrue(strcmp(result.mcr_xzone_enablement_reason, "default") == 0);
}

- (void)testXzoneDisabledTvOSFeatureFlag {
	malloc_config_input_s input = [self createBasicInput];
	input.mci_target_platform = MALLOC_TARGET_PLATFORM_TV;
	input.mci_progname = "test_process";
	input.mci_allow_internal_security_policy = true;
	input.mci_feature_flags[MALLOC_FEATURE_FLAG_TVOS_ENABLEMENT] = false;

	malloc_config_result_s result = malloc_config_from_input(&input);
	XCTAssertFalse(result.mcr_enable_xzone_malloc);
	XCTAssertTrue(strcmp(result.mcr_xzone_enablement_reason, "tvOS feature flag") == 0);
}

- (void)testXzoneDisabledTvOSARM64 {
	malloc_config_input_s input = [self createBasicInput];
	input.mci_target_platform = MALLOC_TARGET_PLATFORM_TV;
	input.mci_progname = "test_process";
	input.mci_arch_plain_arm64 = true;

	malloc_config_result_s result = malloc_config_from_input(&input);
	XCTAssertFalse(result.mcr_enable_xzone_malloc);
	XCTAssertTrue(strcmp(result.mcr_xzone_enablement_reason, "arm64 tvOS") == 0);
}

- (void)testXzoneDisabledBridgeOS {
	malloc_config_input_s input = [self createBasicInput];
	input.mci_target_platform = MALLOC_TARGET_PLATFORM_BRIDGE;
	input.mci_progname = "test_process";

	malloc_config_result_s result = malloc_config_from_input(&input);
	XCTAssertFalse(result.mcr_enable_xzone_malloc);
	XCTAssertTrue(strcmp(result.mcr_xzone_enablement_reason, "bridgeOS") == 0);
}

- (void)testXzoneDisabledIntelMac {
	malloc_config_input_s input = [self createBasicInput];
	input.mci_target_platform = MALLOC_TARGET_PLATFORM_MAC;
	input.mci_arch_intel = true;
	input.mci_progname = "test_process";

	malloc_config_result_s result = malloc_config_from_input(&input);
	XCTAssertFalse(result.mcr_enable_xzone_malloc);
	XCTAssertTrue(strcmp(result.mcr_xzone_enablement_reason, "Intel") == 0);
}

- (void)testXzoneDisabledKnownProcess {
	NSArray *disabledProcesses = @[@"AegirPoster", @"CollectionsPoster"];

	for (NSString *name in disabledProcesses) {
		malloc_config_input_s input = [self createBasicInput];
		input.mci_progname = [name UTF8String];

		malloc_config_result_s result = malloc_config_from_input(&input);
		XCTAssertFalse(result.mcr_enable_xzone_malloc, @"Failed for %@", name);
		XCTAssertTrue(strcmp(result.mcr_xzone_enablement_reason, "disabled known process") == 0, @"Failed for %@", name);
	}
}

- (void)testXzoneEnabledDefault {
	malloc_config_input_s input = [self createBasicInput];
	input.mci_progname = "test_process";

	malloc_config_result_s result = malloc_config_from_input(&input);
	XCTAssertTrue(result.mcr_enable_xzone_malloc);
	XCTAssertTrue(strcmp(result.mcr_xzone_enablement_reason, "default") == 0);
}

- (void)testXzoneEnabledIOS {
	malloc_config_input_s input = [self createBasicInput];
	input.mci_target_platform = MALLOC_TARGET_PLATFORM_IOS;
	input.mci_progname = "test_process";

	malloc_config_result_s result = malloc_config_from_input(&input);
	XCTAssertTrue(result.mcr_enable_xzone_malloc);
	XCTAssertTrue(strcmp(result.mcr_xzone_enablement_reason, "default") == 0);
}

- (void)testXzoneEnabledWatch {
	malloc_config_input_s input = [self createBasicInput];
	input.mci_target_platform = MALLOC_TARGET_PLATFORM_WATCH;
	input.mci_progname = "test_process";

	malloc_config_result_s result = malloc_config_from_input(&input);
	XCTAssertTrue(result.mcr_enable_xzone_malloc);
	XCTAssertTrue(strcmp(result.mcr_xzone_enablement_reason, "default") == 0);
}

- (void)testXzoneEnabledVision {
	malloc_config_input_s input = [self createBasicInput];
	input.mci_target_platform = MALLOC_TARGET_PLATFORM_VISION;
	input.mci_progname = "test_process";

	malloc_config_result_s result = malloc_config_from_input(&input);
	XCTAssertTrue(result.mcr_enable_xzone_malloc);
	XCTAssertTrue(strcmp(result.mcr_xzone_enablement_reason, "default") == 0);
}

#pragma mark - Nano on Xzone Tests

- (void)testNanoOnXzoneDisabledWhenXzoneDisabled {
	malloc_config_input_s input = [self createBasicInput];
	input.mci_arch_lp64 = false;
	input.mci_progname = "test_process";

	malloc_config_result_s result = malloc_config_from_input(&input);
	XCTAssertFalse(result.mcr_enable_xzone_malloc);
	XCTAssertFalse(result.mcr_enable_nano_on_xzone);
}

- (void)testNanoOnXzoneDisabledWithNanoNone {
	malloc_config_input_s input = [self createBasicInput];
	input.mci_nano_version = NANO_NONE;
	input.mci_progname = "test_process";

	malloc_config_result_s result = malloc_config_from_input(&input);
	XCTAssertTrue(result.mcr_enable_xzone_malloc);
	XCTAssertFalse(result.mcr_enable_nano_on_xzone);
}

- (void)testNanoOnXzoneEnvironmentOverride {
	malloc_config_input_s input = [self createBasicInput];
	input.mci_progname = "test_process";
	input.mci_allow_internal_security_policy = true;

	NSDictionary *env = @{@"MallocNanoOnXzone": @"1"};
	input.mci_envp = [self createEnvArray:env];

	malloc_config_result_s result = malloc_config_from_input(&input);
	XCTAssertTrue(result.mcr_enable_xzone_malloc);
	XCTAssertTrue(result.mcr_enable_nano_on_xzone);

	[self freeEnvArray:input.mci_envp];
}

- (void)testNanoOnXzoneEnvironmentOverrideDisabled {
	malloc_config_input_s input = [self createBasicInput];
	input.mci_progname = "test_process";
	input.mci_allow_internal_security_policy = true;

	NSDictionary *env = @{@"MallocNanoOnXzone": @"0"};
	input.mci_envp = [self createEnvArray:env];

	malloc_config_result_s result = malloc_config_from_input(&input);
	XCTAssertTrue(result.mcr_enable_xzone_malloc);
	XCTAssertFalse(result.mcr_enable_nano_on_xzone);

	[self freeEnvArray:input.mci_envp];
}

- (void)testNanoOnXzoneDefaultDisabled {
	malloc_config_input_s input = [self createBasicInput];
	input.mci_progname = "test_process";

	malloc_config_result_s result = malloc_config_from_input(&input);
	XCTAssertTrue(result.mcr_enable_xzone_malloc);
	XCTAssertFalse(result.mcr_enable_nano_on_xzone);
}


#pragma mark - Environment Variable Tests

- (void)testInvalidEnvironmentBoolValues {
	malloc_config_input_s input = [self createBasicInput];
	input.mci_progname = "test_process";
	input.mci_allow_internal_security_policy = true;

	NSArray *invalidValues = @[@"yes", @"true", @"2", @"invalid", @""];

	for (NSString *value in invalidValues) {
		NSDictionary *env = @{@"MallocSecureAllocator": value};
		input.mci_envp = [self createEnvArray:env];

		malloc_config_result_s result = malloc_config_from_input(&input);
		XCTAssertTrue(result.mcr_enable_xzone_malloc, @"Should use default when env var is invalid: %@", value);
		XCTAssertTrue(strcmp(result.mcr_xzone_enablement_reason, "default") == 0, @"Should use default when env var is invalid: %@", value);

		[self freeEnvArray:input.mci_envp];
	}
}

@end
