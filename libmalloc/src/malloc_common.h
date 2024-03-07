/*
 * Copyright (c) 2018-2023 Apple Inc. All rights reserved.
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

#ifndef __MALLOC_COMMON_H
#define __MALLOC_COMMON_H

#include <TargetConditionals.h>
#include <errno.h>
#include <stddef.h>
#include <sys/cdefs.h>

#include "base.h"

#include <malloc/_ptrcheck.h>
__ptrcheck_abi_assume_single()

MALLOC_NOEXPORT
const char * __null_terminated
malloc_common_strstr(const char * __null_terminated src, const char * __counted_by(target_len) target, size_t target_len);

MALLOC_NOEXPORT
long
malloc_common_convert_to_long(const char * __null_terminated ptr, const char * __null_terminated *end_ptr);

MALLOC_NOEXPORT
const char * __null_terminated
malloc_common_value_for_key(const char * __null_terminated src, const char * __null_terminated key);

MALLOC_NOEXPORT
const char * __null_terminated
malloc_common_value_for_key_copy(const char * __null_terminated src, const char * __null_terminated key,
		 char * __counted_by(maxlen) bufp, size_t maxlen);

__options_decl(malloc_zone_options_t, unsigned, {
	MZ_NONE  = 0x0,
	MZ_POSIX = 0x1,
	MZ_C11   = 0x2,
});

static MALLOC_INLINE void
malloc_set_errno_fast(malloc_zone_options_t mzo, int err)
{
	if (mzo & MZ_POSIX) {
#if MALLOC_TARGET_EXCLAVES || TARGET_OS_SIMULATOR
		errno = err;
#else
		(*_pthread_errno_address_direct()) = err;
#endif // MALLOC_TARGET_EXCLAVES || TARGET_OS_SIMULATOR
	}
}

MALLOC_NOEXPORT
void
malloc_slowpath_update(void);

MALLOC_NOEXPORT
void
find_zone_and_free(void * __unsafe_indexable ptr, bool known_non_default);

typedef enum {
	MALLOC_ZERO_ON_FREE = 0, // this is the 0 case so most checks can be 0-tests
	MALLOC_ZERO_NONE = 1,
	MALLOC_ZERO_ON_ALLOC = 2,
} malloc_zero_policy_t;

MALLOC_NOEXPORT
extern malloc_zero_policy_t malloc_zero_policy;

MALLOC_NOEXPORT
unsigned
malloc_zone_batch_malloc_fallback(malloc_zone_t *zone, size_t size,
		void * __unsafe_indexable * __counted_by(num_requested) results, unsigned num_requested);

MALLOC_NOEXPORT
size_t
malloc_zone_pressure_relief_fallback(malloc_zone_t *zone, size_t goal);

MALLOC_NOEXPORT
void
malloc_zone_batch_free_fallback(malloc_zone_t *zone, void * __unsafe_indexable * __counted_by(num) to_be_freed,
		unsigned num);

#if CONFIG_MALLOC_PROCESS_IDENTITY

typedef enum {
	MALLOC_PROCESS_NONE,

	// Darwin
	MALLOC_PROCESS_LAUNCHD,
	MALLOC_PROCESS_LOGD,
	MALLOC_PROCESS_NOTIFYD,

	// Media
	MALLOC_PROCESS_MEDIAPARSERD,
	MALLOC_PROCESS_VIDEOCODECD,
	MALLOC_PROCESS_MEDIAPLAYBACKD,
	MALLOC_PROCESS_AUDIOMXD,
	MALLOC_PROCESS_AVCONFERENCED,
	MALLOC_PROCESS_MEDIASERVERD,
	MALLOC_PROCESS_CAMERACAPTURED,

	// Messages
	MALLOC_PROCESS_BLASTDOOR_MESSAGES,
	MALLOC_PROCESS_BLASTDOOR_IDS,
	MALLOC_PROCESS_IMDPERSISTENCEAGENT,
	MALLOC_PROCESS_IMAGENT,

	// quicklook
	MALLOC_PROCESS_QUICKLOOK_THUMBNAIL_SECURE,
	MALLOC_PROCESS_QUICKLOOK_PREVIEW,
	MALLOC_PROCESS_QUICKLOOK_THUMBNAIL,

	MALLOC_PROCESS_TELNETD,
	MALLOC_PROCESS_SSHD,
	MALLOC_PROCESS_SSHD_KEYGEN_WRAPPER,
	MALLOC_PROCESS_BASH,
	MALLOC_PROCESS_DASH,
	MALLOC_PROCESS_SH,
	MALLOC_PROCESS_ZSH,
	MALLOC_PROCESS_PYTHON3,
	MALLOC_PROCESS_PERL,
	MALLOC_PROCESS_SU,
	MALLOC_PROCESS_TIME,
	MALLOC_PROCESS_FIND,
	MALLOC_PROCESS_XARGS,

	// Browser
	MALLOC_PROCESS_BROWSER,
	MALLOC_PROCESS_MTLCOMPILERSERVICE,

	// Other
	MALLOC_PROCESS_CALLSERVICESD,
	MALLOC_PROCESS_MAILD,
	MALLOC_PROCESS_MDNSRESPONDER,
	MALLOC_PROCESS_ASVASSETVIEWER,
	MALLOC_PROCESS_IDENTITYSERVICESD,
	MALLOC_PROCESS_WIFID,
	MALLOC_PROCESS_FMFD,
	MALLOC_PROCESS_SEARCHPARTYD,
	MALLOC_PROCESS_VMD,
	MALLOC_PROCESS_COMMCENTER,
	MALLOC_PROCESS_WIFIP2PD,
	MALLOC_PROCESS_WIFIANALYTICSD,
	MALLOC_PROCESS_AEGIRPOSTER,
	MALLOC_PROCESS_COLLECTIONSPOSTER,

	MALLOC_PROCESS_COUNT,
} malloc_process_identity_t;

#endif // CONFIG_MALLOC_PROCESS_IDENTITY

typedef enum : unsigned {
	MALLOC_ZONE_TYPE_UNKNOWN,
	MALLOC_ZONE_TYPE_XZONE,
	MALLOC_ZONE_TYPE_PGM,
	MALLOC_ZONE_TYPE_SANITIZER,
} malloc_zone_type_t;

MALLOC_NOEXPORT
kern_return_t
get_zone_type(task_t task,
		memory_reader_t reader,
		vm_address_t zone_address,
		unsigned *zone_type) __result_use_check;

MALLOC_NOEXPORT
malloc_zone_t *
get_wrapped_zone(malloc_zone_t *zone);

static MALLOC_INLINE
const char *
get_wrapper_zone_label(malloc_zone_t *wrapper_zone)
{
	// malloc_introspection_t::zone_type availability
	MALLOC_ASSERT(wrapper_zone->version >= 14);

	unsigned zone_type = wrapper_zone->introspect->zone_type;
	switch (zone_type) {
	case MALLOC_ZONE_TYPE_PGM:
		return "PGM";
	case MALLOC_ZONE_TYPE_SANITIZER:
		return "Sanitizer";
	}
	__builtin_unreachable();
}

struct wrapper_zone_layout_s {
	malloc_zone_t malloc_zone;
	malloc_zone_t *wrapped_zone;
};

#define WRAPPED_ZONE_OFFSET offsetof(struct wrapper_zone_layout_s, wrapped_zone)
#define ASSERT_WRAPPER_ZONE(zone_t) \
	MALLOC_STATIC_ASSERT(offsetof(zone_t, malloc_zone) == 0, \
			#zone_t " instances must be usable as regular malloc zones"); \
	MALLOC_STATIC_ASSERT(offsetof(zone_t, wrapped_zone) == WRAPPED_ZONE_OFFSET, \
			"malloc_get_wrapped_zone() dependency");

#endif // __MALLOC_COMMON_H
