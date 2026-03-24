/*
 * Copyright (c) 2025 Apple Inc. All rights reserved.
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

#ifndef __MALLOC_CONFIG_H__
#define __MALLOC_CONFIG_H__

OS_ENUM(malloc_target_platform, uint8_t,
	MALLOC_TARGET_PLATFORM_ANY,
	MALLOC_TARGET_PLATFORM_MAC,
	MALLOC_TARGET_PLATFORM_IOS,
	MALLOC_TARGET_PLATFORM_TV,
	MALLOC_TARGET_PLATFORM_WATCH,
	MALLOC_TARGET_PLATFORM_VISION,
	MALLOC_TARGET_PLATFORM_BRIDGE,
);

OS_ENUM(malloc_target_platform_variant, uint8_t,
	MALLOC_TARGET_PLATFORM_VARIANT_INVALID,
	MALLOC_TARGET_PLATFORM_VARIANT_NORMAL,
	MALLOC_TARGET_PLATFORM_VARIANT_SIMULATOR,
	MALLOC_TARGET_PLATFORM_VARIANT_DRIVERKIT,
);

OS_ENUM(malloc_feature_flag, uint8_t,
	MALLOC_FEATURE_FLAG_INVALID,
	MALLOC_FEATURE_FLAG_TVOS_ENABLEMENT,
	MALLOC_FEATURE_FLAG_COUNT,
);

typedef struct malloc_config_input_s {
	const char **                    mci_envp;
	const char **                    mci_apple_array;
	const char *                     mci_progname;
	bool					         mci_is_pid_1;
	malloc_target_platform_t         mci_target_platform;
	malloc_target_platform_variant_t mci_target_platform_variant;
	os_security_config_t             mci_os_security_config;
	dyld_platform_t                  mci_dyld_active_platform;
	uint64_t                         mci_dyld_program_version_token;
	nano_version_t                   mci_nano_version;
	bool                             mci_allow_internal_security_policy;
	bool                             mci_feature_flags[MALLOC_FEATURE_FLAG_COUNT];
	bool                             mci_process_feature_flags[MALLOC_PROCESS_COUNT];
	bool                             mci_arch_intel;
	bool                             mci_arch_lp64;
	bool                             mci_arch_plain_arm64;
	malloc_zero_policy_t             mci_zero_policy;
} malloc_config_input_s;

typedef malloc_config_input_s *malloc_config_input_t;

typedef struct malloc_config_result_s {
	malloc_process_identity_t   mcr_process_identity;
	bool                        mcr_enable_xzone_malloc;
	bool                        mcr_enable_nano_on_xzone;
	const char *                mcr_xzone_enablement_reason;
} malloc_config_result_s;

typedef malloc_config_result_s *malloc_config_result_t;

MALLOC_NOEXPORT
malloc_config_result_s
malloc_config_from_input(malloc_config_input_t input);

#endif // __MALLOC_CONFIG_H__
