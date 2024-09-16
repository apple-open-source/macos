/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#pragma once

#if CONFIG_EXCLAVES

#include <sys/cdefs.h>
#include <stdbool.h>

#include <kern/assert.h>
#include <kern/debug.h>

#include <mach/exclaves.h>

#if DEVELOPMENT || DEBUG
extern unsigned int exclaves_debug;
#else
#define exclaves_debug 0
#endif /* DEVELOPMENT || DEBUG */

/* Flag values in exclaves_debug boot-arg/sysctl */
__options_closed_decl(exclaves_debug_flags, unsigned int, {
	exclaves_debug_show_errors = 0x1,
	exclaves_debug_show_progress = 0x2,
	exclaves_debug_show_scheduler_request_response = 0x4,
	exclaves_debug_show_storage_upcalls = 0x8,
	exclaves_debug_show_iokit_upcalls = 0x10,
	exclaves_debug_show_notification_upcalls = 0x20,
	exclaves_debug_show_test_output = 0x40,
	exclaves_debug_show_lifecycle_upcalls = 0x80,
});

#define EXCLAVES_ENABLE_SHOW_ERRORS                     (DEVELOPMENT || DEBUG)
#define EXCLAVES_ENABLE_SHOW_PROGRESS                   (DEVELOPMENT || DEBUG)
#define EXCLAVES_ENABLE_SHOW_SCHEDULER_REQUEST_RESPONSE (DEVELOPMENT || DEBUG)
#define EXCLAVES_ENABLE_SHOW_STORAGE_UPCALLS            (DEVELOPMENT || DEBUG)
#define EXCLAVES_ENABLE_SHOW_IOKIT_UPCALLS              (DEVELOPMENT || DEBUG)
#define EXCLAVES_ENABLE_SHOW_NOTIFICATION_UPCALLS       (DEVELOPMENT || DEBUG)
#define EXCLAVES_ENABLE_SHOW_TEST_OUTPUT                (DEVELOPMENT || DEBUG)
#define EXCLAVES_ENABLE_SHOW_LIFECYCLE_UPCALLS          (DEVELOPMENT || DEBUG)

#if EXCLAVES_ENABLE_SHOW_ERRORS || EXCLAVES_ENABLE_SHOW_TEST_OUTPUT
#define exclaves_debug_show_errors_flag (exclaves_debug_show_errors|exclaves_debug_show_test_output)
#else
#define exclaves_debug_show_errors_flag 0
#endif
#if EXCLAVES_ENABLE_SHOW_PROGRESS
#define exclaves_debug_show_progress_flag exclaves_debug_show_progress
#else
#define exclaves_debug_show_progress_flag 0
#endif
#if EXCLAVES_ENABLE_SHOW_SCHEDULER_REQUEST_RESPONSE
#define exclaves_debug_show_scheduler_request_response_flag \
    exclaves_debug_show_scheduler_request_response
#else
#define exclaves_debug_show_scheduler_request_response_flag 0
#endif
#if EXCLAVES_ENABLE_SHOW_STORAGE_UPCALLS
#define exclaves_debug_show_storage_upcalls_flag \
    exclaves_debug_show_storage_upcalls
#else
#define exclaves_debug_show_storage_upcalls_flag 0
#endif
#if EXCLAVES_ENABLE_SHOW_IOKIT_UPCALLS
#define exclaves_debug_show_iokit_upcalls_flag exclaves_debug_show_iokit_upcalls
#else
#define exclaves_debug_show_iokit_upcalls_flag 0
#endif
#if EXCLAVES_ENABLE_SHOW_NOTIFICATION_UPCALLS
#define exclaves_debug_show_notification_upcalls_flag exclaves_debug_show_notification_upcalls
#else
#define exclaves_debug_show_notification_upcalls_flag 0
#endif
#if EXCLAVES_ENABLE_SHOW_TEST_OUTPUT
#define exclaves_debug_show_test_output_flag exclaves_debug_show_test_output
#else
#define exclaves_debug_show_test_output_flag 0
#endif
#if EXCLAVES_ENABLE_SHOW_LIFECYCLE_UPCALLS
#define exclaves_debug_show_lifecycle_upcalls_flag exclaves_debug_show_lifecycle_upcalls
#else
#define exclaves_debug_show_lifecycle_upcalls_flag 0
#endif

#define exclaves_debug_enabled(flag) \
    ((bool)(exclaves_debug & exclaves_debug_##flag##_flag))
#define exclaves_debug_printf(flag, format, ...) ({ \
	if (exclaves_debug_enabled(flag)) { \
	        printf(format, ##__VA_ARGS__); \
	}})


#pragma mark exclaves relaxed requirement management

#if DEVELOPMENT || DEVELOPMENT
extern exclaves_requirement_t exclaves_relaxed_requirements;
#else
extern const exclaves_requirement_t exclaves_relaxed_requirements;
#endif /* DEVELOPMENT || DEBUG */

/*
 * Return true if the specified exclaves requirement has been relaxed, false
 * otherwise.
 */
static inline bool
exclaves_requirement_is_relaxed(exclaves_requirement_t requirement)
{
	assert3u(requirement & (requirement - 1), ==, 0);

	/*
	 * The medium-term plan is that the boot-arg controlling entitlements
	 * goes away entirely and is replaced with EXCLAVES_R_ENTITLEMENTS.
	 * Until that happens, for historical reasons, if the entitlement
	 * boot-arg has disabled EXCLAVES_PRIV_CONCLAVE_HOST, then relax
	 * EXCLAVES_R_CONCLAVE and EXCLAVES_R_CONCLAVE_RESOURCES here too.
	 */
	extern unsigned int exclaves_entitlement_flags;
	exclaves_requirement_t current = exclaves_relaxed_requirements;
	if ((exclaves_entitlement_flags & EXCLAVES_PRIV_CONCLAVE_HOST) == 0) {
		current |= EXCLAVES_R_CONCLAVE | EXCLAVES_R_CONCLAVE_RESOURCES;
	}


	return (requirement & current) != 0;
}

/*
 * Called when a requirement has not been met. Produces a log message and
 * continues if the requirement is relaxed, otherwise panics.
 */
#define exclaves_requirement_assert(requirement, fmt, ...) { \
	if (exclaves_requirement_is_relaxed(requirement)) {                   \
	        exclaves_debug_printf(show_errors,                            \
	            "exclaves: requirement was relaxed, ignoring error: "     \
	             fmt "\n", ##__VA_ARGS__);                                \
	} else {                                                              \
	        panic("exclaves: requirement failed: " fmt,                   \
	            ##__VA_ARGS__);                                           \
	}                                                                     \
};

#endif /* CONFIG_EXCLAVES */
