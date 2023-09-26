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
#ifndef __MALLOC_COMMON_H
#define __MALLOC_COMMON_H

MALLOC_NOEXPORT
const char *
malloc_common_strstr(const char *src, const char *target, size_t target_len);

MALLOC_NOEXPORT
long
malloc_common_convert_to_long(const char *ptr, const char **end_ptr);

MALLOC_NOEXPORT
const char *
malloc_common_value_for_key(const char *src, const char *key);

MALLOC_NOEXPORT
const char *
malloc_common_value_for_key_copy(const char *src, const char *key,
		 char *bufp, size_t maxlen);

__options_decl(malloc_zone_options_t, unsigned, {
	MZ_NONE  = 0x0,
	MZ_POSIX = 0x1,
	MZ_C11   = 0x2,
});

static MALLOC_INLINE void
malloc_set_errno_fast(malloc_zone_options_t mzo, int err)
{
	if (mzo & MZ_POSIX) {
#if TARGET_OS_SIMULATOR
		errno = err;
#else
		(*_pthread_errno_address_direct()) = err;
#endif
	}
}

MALLOC_NOEXPORT
void
malloc_slowpath_update(void);

MALLOC_NOEXPORT
void
find_zone_and_free(void *ptr, bool known_non_default);

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
		void **results, unsigned num_requested);

MALLOC_NOEXPORT
size_t
malloc_zone_pressure_relief_fallback(malloc_zone_t *zone, size_t goal);

MALLOC_NOEXPORT
void
malloc_zone_batch_free_fallback(malloc_zone_t *zone, void **to_be_freed,
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

	// Messages
	MALLOC_PROCESS_BLASTDOOR_MESSAGES,
	MALLOC_PROCESS_BLASTDOOR_IDS,
	MALLOC_PROCESS_IMDPERSISTENCEAGENT,
	MALLOC_PROCESS_IMAGENT,

	// quicklook
	MALLOC_PROCESS_QUICKLOOK_THUMBNAIL_SECURE,
	MALLOC_PROCESS_QUICKLOOK_PREVIEW,
	MALLOC_PROCESS_QUICKLOOK_THUMBNAIL,

	// Safari/WebKit
	MALLOC_PROCESS_WEBKIT_NETWORKING,
	MALLOC_PROCESS_WEBKIT_GPU,
	MALLOC_PROCESS_MTLCOMPILERSERVICE,
	MALLOC_PROCESS_WEBKIT_WEBCONTENT,

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

	MALLOC_PROCESS_COUNT,
} malloc_process_identity_t;

#endif // CONFIG_MALLOC_PROCESS_IDENTITY

typedef enum : unsigned {
	MALLOC_ZONE_TYPE_UNKNOWN,
	MALLOC_ZONE_TYPE_XZONE,
	MALLOC_ZONE_TYPE_PGM,
} malloc_zone_type_t;

#endif // __MALLOC_COMMON_H
