/*
 * Copyright (c) 2018 Apple Inc. All rights reserved.
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

/* Private interfaces between libsystem_malloc, libSystem, and MallocStackLogging */

#ifndef _MALLOC_IMPLEMENTATION_H_
#define _MALLOC_IMPLEMENTATION_H_

#include <TargetConditionals.h>

#include <malloc/malloc.h>

#if !TARGET_OS_EXCLAVECORE && !TARGET_OS_EXCLAVEKIT
# include <stack_logging.h>
#endif // !TARGET_OS_EXCLAVECORE && !TARGET_OS_EXCLAVEKIT

#include <stdbool.h>
#include <stddef.h>
#include <ptrauth.h>

#include <malloc/_ptrcheck.h>
__ptrcheck_abi_assume_single()

#if defined(__PTRAUTH_INTRINSICS__) && __PTRAUTH_INTRINSICS__ && \
		__has_builtin(__builtin_ptrauth_string_discriminator)
#define LIBMALLOC_FUNCTION_PTRAUTH(f) \
	__ptrauth(ptrauth_key_function_pointer, 1, \
			__builtin_ptrauth_string_discriminator("libmalloc_functions_" # f) \
	) f
#else
#define LIBMALLOC_FUNCTION_PTRAUTH(f) f
#endif


/*********	Libsystem initializers ************/

struct _malloc_late_init;

struct _malloc_late_init {
	unsigned long version;
	/* The following functions are included in version 1 of this structure */
	void * (*LIBMALLOC_FUNCTION_PTRAUTH(dlopen)) (const char * __null_terminated path, int mode);
	void * (*LIBMALLOC_FUNCTION_PTRAUTH(dlsym)) (void *handle, const char * __null_terminated symbol);
	bool internal_diagnostics;  /* os_variant_has_internal_diagnostics() */
	/* The following are included in version 2 of this structure */
	const struct _malloc_msl_symbols *msl;
};

void __malloc_init(const char * __null_terminated * __null_terminated apple);
#if !TARGET_OS_EXCLAVECORE && !TARGET_OS_EXCLAVEKIT
void __malloc_late_init(const struct _malloc_late_init *);

struct _malloc_msl_lite_hooks_s;

struct _malloc_msl_symbols {
	unsigned long version;

	/* the following are included in version 1 of this structure */

	void (*handle_memory_event) (unsigned long event);
	boolean_t (*stack_logging_locked) (void);
	void (*fork_prepare) (void);
	void (*fork_parent) (void);
	void (*fork_child) (void);

	void (*set_flags_from_environment) (const char **env);
	void (*initialize) (void);
	boolean_t (*turn_on_stack_logging) (stack_logging_mode_type mode);
	void (*turn_off_stack_logging) (void);
	void (*copy_msl_lite_hooks) (struct _malloc_msl_lite_hooks_s *hooksp, size_t size);
};

/*
 * Definitions intended for the malloc stack logging library only.
 * This is SPI that is *not* intended for use elsewhere. It will change
 * and will eventually be removed, without prior warning.
 */
#if defined(MALLOC_ENABLE_MSL_LITE_SPI) && MALLOC_ENABLE_MSL_LITE_SPI

typedef struct szone_s szone_t;

/* Flags which uniquely identify the lite zone's wrapped zone */
#define MALLOC_MSL_LITE_WRAPPED_ZONE_FLAGS (1 << 10)

typedef struct _malloc_msl_lite_hooks_s {
	boolean_t (*has_default_zone0)(void);
	void (*insert_msl_lite_zone)(malloc_zone_t *zone);
	malloc_zone_t *(*get_global_helper_zone)(void);
} _malloc_msl_lite_hooks_t;

#endif // defined(MALLOC_ENABLE_MSL_LITE_SPI) && MALLOC_ENABLE_MSL_LITE_SPI

#endif // !TARGET_OS_EXCLAVECORE && !TARGET_OS_EXCLAVEKIT

#endif
