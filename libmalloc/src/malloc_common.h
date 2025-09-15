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

#if !MALLOC_TARGET_EXCLAVES && !MALLOC_TARGET_EXCLAVES_INTROSPECTOR
#if CONFIG_CHECK_PLATFORM_BINARY
MALLOC_NOEXPORT
extern bool malloc_is_platform_binary;
#endif

MALLOC_NOEXPORT
bool
_malloc_is_platform_binary(void);

MALLOC_NOEXPORT
extern bool malloc_internal_security_policy;

#if !__has_feature(bounds_safety)
MALLOC_NOEXPORT
bool
_malloc_allow_internal_security_policy(const char *envp[]);
#endif

#endif // !MALLOC_TARGET_EXCLAVES && !MALLOC_TARGET_EXCLAVES_INTROSPECTOR

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
#if TARGET_OS_OSX
	MALLOC_PROCESS_QUICKLOOK_UISERVICE,
	MALLOC_PROCESS_QUICKLOOK_MACOS,
#endif // TARGET_OS_OSX

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

#if TARGET_OS_OSX
	MALLOC_PROCESS_SAFARI,
	MALLOC_PROCESS_SAFARI_SUPPORT,
	MALLOC_PROCESS_VTDECODERXPCSERVICE,
#endif // TARGET_OS_OSX

#if TARGET_OS_VISION
	MALLOC_PROCESS_PRESENCED,
	MALLOC_PROCESS_FACETIME,
	MALLOC_PROCESS_MANAGEDASSETSD,
	MALLOC_PROCESS_POLARISD,
	MALLOC_PROCESS_ARKITD,
	MALLOC_PROCESS_BACKBOARDD,
#endif

	MALLOC_PROCESS_REPORTCRASH,
	MALLOC_PROCESS_AUDIOCONVERTERSERVICE,

	MALLOC_PROCESS_HARDENED_HEAP_CONFIG,

	// NOTE: Processes enumerated above this line are considered "security
	// critical", and will get additional features (guard pages, more pointer
	// buckets, etc) if the secure allocator is enabled. Processes below the
	// line have identities, but don't get these additional features
	MALLOC_PROCESS_MAX_SEC_CRITICAL__MARK,
	MALLOC_PROCESS_MAX_SEC_CRITICAL = MALLOC_PROCESS_MAX_SEC_CRITICAL__MARK - 1,

	// Non security critical processes
	MALLOC_PROCESS_AEGIRPOSTER,
	MALLOC_PROCESS_COLLECTIONSPOSTER,

#if TARGET_OS_WATCH
	MALLOC_PROCESS_BACKBOARDD,
	MALLOC_PROCESS_CLOCKFACE,
#endif // TARGET_OS_WATCH

#if TARGET_OS_OSX
	// Processes that need secure allocator
	MALLOC_PROCESS_GROUPSESSIONSERVICE,
	MALLOC_PROCESS_IMTRANSCODERAGENT,
	MALLOC_PROCESS_KEYCHAINSHARINGMESSAGINGD,
	MALLOC_PROCESS_MESSAGES,
	MALLOC_PROCESS_SCREENSHARING,

	// Processes that do not get secure allocator
	MALLOC_PROCESS_VTENCODERXPCSERVICE,
#endif

#if TARGET_OS_VISION
	MALLOC_PROCESS_WAKEBOARDD,
	MALLOC_PROCESS_REALITYCAMERAD,
#endif


	MALLOC_PROCESS_COUNT,
} malloc_process_identity_t;

static MALLOC_INLINE
bool
malloc_process_is_security_critical(malloc_process_identity_t identity)
{
	return identity > MALLOC_PROCESS_NONE &&
			identity <= MALLOC_PROCESS_MAX_SEC_CRITICAL;
}

static MALLOC_INLINE
bool
malloc_process_is_security_critical_max_perf(
		malloc_process_identity_t identity)
{
#if TARGET_OS_OSX
	if (identity == MALLOC_PROCESS_MTLCOMPILERSERVICE) {
		return true;
	}
#elif TARGET_OS_VISION
	if (identity == MALLOC_PROCESS_ARKITD ||
			identity == MALLOC_PROCESS_BACKBOARDD) {
		return true;
	}
#endif

	if (identity == MALLOC_PROCESS_HARDENED_HEAP_CONFIG) {
		return true;
	}

	return false;
}
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

// This function is used to abort the program when freeing an invalid pointer.
// Its goal, as the naming indicates, is to provide a clear indication in the
// call stack that libmalloc is intentionally crashing because the client
// provided a pointer that was deemed invalid.
MALLOC_NOEXPORT MALLOC_NOINLINE
void
___BUG_IN_CLIENT_OF_LIBMALLOC_POINTER_BEING_FREED_WAS_NOT_ALLOCATED(
	int flags,
	void *__unsafe_indexable ptr);

static MALLOC_INLINE
void
malloc_report_pointer_was_not_allocated(int f, void *__unsafe_indexable p) {
	___BUG_IN_CLIENT_OF_LIBMALLOC_POINTER_BEING_FREED_WAS_NOT_ALLOCATED(f, p);
}

#endif // __MALLOC_COMMON_H
