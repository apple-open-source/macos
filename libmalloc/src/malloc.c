/*
 * Copyright (c) 1999, 2000, 2003, 2005, 2008, 2012 Apple Inc. All rights reserved.
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

/*
 * Needed for definition of MALLOC_MSL_LITE_WRAPPED_ZONE_FLAGS
 */
#ifndef MALLOC_ENABLE_MSL_LITE_SPI
#define MALLOC_ENABLE_MSL_LITE_SPI 1
#endif // MALLOC_ENABLE_MSL_LITE_SPI

#include "internal.h"

#if MALLOC_TARGET_IOS
// malloc_report(ASL_LEVEL_INFO...) on iOS doesn't show up in the Xcode Console log of the device,
// but ASL_LEVEL_NOTICE does.  So raising the log level is helpful.
#undef ASL_LEVEL_INFO
#define ASL_LEVEL_INFO ASL_LEVEL_NOTICE
#endif // MALLOC_TARGET_IOS

#define USE_SLEEP_RATHER_THAN_ABORT 0

static _malloc_lock_s _malloc_lock = _MALLOC_LOCK_INIT;
#define MALLOC_LOCK() _malloc_lock_lock(&_malloc_lock)
#define MALLOC_TRY_LOCK() _malloc_lock_trylock(&_malloc_lock)
#define MALLOC_UNLOCK() _malloc_lock_unlock(&_malloc_lock)
#define MALLOC_REINIT_LOCK() _malloc_lock_init(&_malloc_lock)

/* The following variables are exported for the benefit of performance tools
 *
 * It should always be safe to first read malloc_num_zones, then read
 * malloc_zones without taking the lock, if only iteration is required and
 * provided that when malloc_destroy_zone is called all prior operations on that
 * zone are complete and no further calls referencing that zone can be made.
 */
int32_t malloc_num_zones = 0;
int32_t malloc_num_zones_allocated = 0;
malloc_zone_t **malloc_zones = (malloc_zone_t **)0xdeaddeaddeaddead;

// TODO: getter and setter rather than direct export so that this can be baked
// into the cached fastpath checks
malloc_logger_t *malloc_logger = NULL;

static uint32_t initial_num_zones;
static malloc_zone_t *initial_scalable_zone;
#if CONFIG_NANOZONE
static malloc_zone_t *initial_nano_zone;
#endif // CONFIG_NANOZONE
malloc_zone_t *initial_xzone_zone;
static malloc_zone_t *default_purgeable_zone;
static bool has_injected_zone0;
static bool malloc_xzone_enabled = MALLOC_XZONE_ENABLED_DEFAULT;

typedef enum {
	MALLOC_XZONE_OVERRIDE_DEFAULT,
	MALLOC_XZONE_OVERRIDE_DISABLED,
	MALLOC_XZONE_OVERRIDE_ENABLED,
} malloc_xzone_override_t;

static malloc_xzone_override_t malloc_xzone_enabled_override;
static malloc_xzone_override_t malloc_xzone_nano_override;
static malloc_xzone_override_t malloc_nano_on_xzone_override;

#if CONFIG_CHECK_PLATFORM_BINARY
static bool malloc_xzone_allow_non_platform;
#endif // CONFIG_CHECK_PLATFORM_BINARY

unsigned malloc_debug_flags = 0;
bool malloc_tracing_enabled = false;
bool malloc_space_efficient_enabled = false;
bool malloc_medium_space_efficient_enabled = true;

bool malloc_sanitizer_enabled = false;

bool malloc_interposition_compat = false;


#if CONFIG_MALLOC_PROCESS_IDENTITY
malloc_process_identity_t malloc_process_identity = MALLOC_PROCESS_NONE;
#endif // CONFIG_MALLOC_PROCESS_IDENTITY

unsigned malloc_check_start = 0; // 0 means don't check
unsigned malloc_check_counter = 0;
unsigned malloc_check_each = 1000;

static int malloc_check_sleep = 100; // default 100 second sleep
static int malloc_check_abort = 0;   // default is to sleep, not abort
static bool malloc_simple_stack_logging = false;
#define MALLOC_SIMPLE_STACK_LOGGING_FLAGS \
		(ASL_LEVEL_NOTICE | MALLOC_REPORT_NOPREFIX | MALLOC_REPORT_BACKTRACE | MALLOC_REPORT_NOWRITE)

bool malloc_slowpath = false;

static struct _malloc_msl_symbols msl = {};

static struct malloc_sanitizer_poison *sanitizer = NULL;

#if CONFIG_SANITIZER && TARGET_OS_OSX
// Fallback pointers for memory poisoning functions from upstream ASan runtime
static struct {
	void (*memory_poison)(uintptr_t ptr, size_t sz);
	void (*memory_unpoison)(uintptr_t ptr, size_t sz);
} sanitizer_fallback_ptrs;

// Fallback structure for heap allocation functions from our ASan runtime
static struct malloc_sanitizer_poison sanitizer_fallback = {0};

static void malloc_sanitizer_fallback_allocate_poison(uintptr_t ptr, size_t leftrz_sz, size_t alloc_sz, size_t rightrz_sz);
static void malloc_sanitizer_fallback_deallocate_poison(uintptr_t ptr, size_t sz);
#endif
/* These masks are exported for libdispatch to register with (see "internal.h") */
const unsigned long malloc_memorypressure_mask_default_4libdispatch = MALLOC_MEMORYPRESSURE_MASK_DEFAULT;
const unsigned long malloc_memorypressure_mask_msl_4libdispatch = MALLOC_MEMORYPRESSURE_MASK_MSL;

MALLOC_NOEXPORT malloc_zone_t* lite_zone = NULL;

/*
 * Counters that coordinate zone destruction (in malloc_zone_unregister) with
 * find_registered_zone (here abbreviated as FRZ).
 */
static int32_t volatile counterAlice = 0, counterBob = 0;
static int32_t volatile * volatile pFRZCounterLive = &counterAlice;
static int32_t volatile * volatile pFRZCounterDrain = &counterBob;

unsigned int _os_cpu_number_override = -1;

static inline malloc_zone_t *inline_malloc_default_zone(void) __attribute__((always_inline));

#define MALLOC_LOG_TYPE_ALLOCATE stack_logging_type_alloc
#define MALLOC_LOG_TYPE_DEALLOCATE stack_logging_type_dealloc
#define MALLOC_LOG_TYPE_HAS_ZONE stack_logging_flag_zone
#define MALLOC_LOG_TYPE_CLEARED stack_logging_flag_cleared

#define DEFAULT_MALLOC_ZONE_STRING "DefaultMallocZone"
#define DEFAULT_SCALABLE_ZONE_STRING "DefaultScalableMallocZone"
#define DEFAULT_PUREGEABLE_ZONE_STRING "DefaultPurgeableMallocZone"
#define MALLOC_HELPER_ZONE_STRING "MallocHelperZone"

MALLOC_NOEXPORT
unsigned int phys_ncpus;

MALLOC_NOEXPORT
unsigned int logical_ncpus;

#if CONFIG_MAGAZINE_PER_CLUSTER
MALLOC_NOEXPORT
unsigned int ncpuclusters;
#endif // CONFIG_MAGAZINE_PER_CLUSTER

MALLOC_NOEXPORT
unsigned int hyper_shift;

MALLOC_NOEXPORT
size_t malloc_absolute_max_size;

// Boot argument for max magazine control
static const char max_magazines_boot_arg[] = "malloc_max_magazines";

static const char large_expanded_cache_threshold_boot_arg[] = "malloc_large_expanded_cache_threshold";


MALLOC_NOEXPORT
unsigned malloc_zero_on_free_sample_period = 0; // 0 means don't sample

static const char zero_on_free_sample_period_boot_arg[] = "malloc_zero_on_free_sample_period";

MALLOC_NOEXPORT
malloc_zero_policy_t malloc_zero_policy = MALLOC_ZERO_POLICY_DEFAULT;

static const char zero_on_free_enabled_boot_arg[] = "malloc_zero_on_free_enabled";


#if CONFIG_MEDIUM_ALLOCATOR
static const char medium_enabled_boot_arg[] = "malloc_medium_zone";
static const char max_medium_magazines_boot_arg[] = "malloc_max_medium_magazines";
static const char medium_activation_threshold_boot_arg[] = "malloc_medium_activation_threshold";
static const char medium_space_efficient_boot_arg[] = "malloc_medium_space_efficient";
static const char medium_madvise_dram_scale_divisor_boot_arg[] = "malloc_medium_madvise_dram_scale_divisor";
#endif // CONFIG_MEDIUM_ALLOCATOR

static
bool malloc_report_config = false;

/*********	Utilities	************/
static bool _malloc_entropy_initialized;

#if !TARGET_OS_DRIVERKIT
#include <dlfcn.h>

typedef void * (*dlopen_t) (const char * __path, int __mode);
typedef void * (*dlsym_t) (void * __handle, const char * __symbol);

static dlopen_t LIBMALLOC_FUNCTION_PTRAUTH(_dlopen) = NULL;
static dlsym_t LIBMALLOC_FUNCTION_PTRAUTH(_dlsym) = NULL;
#else
#define _dlopen(...) NULL
#define _dlsym(...) NULL
#endif // TARGET_OS_DRIVERKIT

void
malloc_slowpath_update(void)
{
	bool slowpath = has_injected_zone0 ||
			malloc_num_zones == 0 ||
			malloc_check_start ||
			lite_zone ||
			malloc_tracing_enabled ||
			malloc_simple_stack_logging ||
			(malloc_debug_flags & MALLOC_DO_SCRIBBLE) != 0 ||
			malloc_interposition_compat;

	if (malloc_slowpath != slowpath) {
		malloc_slowpath = slowpath;
	}
}

void __malloc_init(const char *apple[]);
static void _malloc_initialize(const char *apple[], const char *bootargs);

static int
__entropy_from_kernel(const char *str)
{
	unsigned long long val;
	char tmp[20], *p;
	int idx = 0;

	/* Skip over key to the first value */
	str = strchr(str, '=');
	if (str == NULL) {
		return 0;
	}
	str++;

	while (str && idx < sizeof(malloc_entropy) / sizeof(malloc_entropy[0])) {
		strlcpy(tmp, str, 20);
		p = strchr(tmp, ',');
		if (p) {
			*p = '\0';
		}
		val = strtoull_l(tmp, NULL, 0, NULL);
		malloc_entropy[idx] = (uint64_t)val;
		idx++;
		if ((str = strchr(str, ',')) != NULL) {
			str++;
		}
	}
	return idx;
}

#if TARGET_OS_OSX && defined(__x86_64__)
static uint64_t
__is_translated(void)
{
	return (*(uint64_t*)_COMM_PAGE_CPU_CAPABILITIES64) & kIsTranslated;
}
#endif /* TARGET_OS_OSX */

#define LIBMALLOC_EXPERIMENT_FACTORS_KEY "MallocExperiment="
#define LIBMALLOC_EXPERIMENT_DISABLE_MEDIUM (1ULL)
#define LIBMALLOC_DEFERRED_RECLAIM_ENABLE "MallocDeferredReclaim=1"
static void
__malloc_init_experiments(const char *str)
{
	uint64_t experiment_factors = 0;
	str = strchr(str, '=');
	if (str) {
		experiment_factors = strtoull_l(str + 1, NULL, 16, NULL);
	}
	switch (experiment_factors) {
	case LIBMALLOC_EXPERIMENT_DISABLE_MEDIUM:
		magazine_medium_enabled = false;
		break;
	}
}

static void
__malloc_init_from_bootargs(const char *bootargs)
{
	// The maximum number of magazines can be set either via a
	// boot argument or from the environment. Get the boot argument value
	// here and store it. We can't bounds check it until we have phys_ncpus,
	// which happens later in _malloc_initialize(), along with handling
	// of the environment value setting.
	char value_buf[256];
	const char *flag = malloc_common_value_for_key_copy(bootargs,
			max_magazines_boot_arg, value_buf, sizeof(value_buf));
	if (flag) {
		const char *endp;
		long value = malloc_common_convert_to_long(flag, &endp);
		if (!*endp && value >= 0) {
			max_magazines = (unsigned int)value;
		} else {
			malloc_report(ASL_LEVEL_ERR,
						   "malloc_max_magazines must be positive - ignored.\n");
		}
	}

	flag = malloc_common_value_for_key_copy(bootargs,
			large_expanded_cache_threshold_boot_arg, value_buf, sizeof(value_buf));
	if (flag) {
		const char *endp;
		long value = malloc_common_convert_to_long(flag, &endp);
		if (!*endp && value >= 0) {
			magazine_large_expanded_cache_threshold = (unsigned int)value;
		} else {
			malloc_report(ASL_LEVEL_ERR,
					"malloc_large_expanded_cache_threshold must be positive - ignored.\n");
		}
	}

	flag = malloc_common_value_for_key_copy(bootargs,
			zero_on_free_enabled_boot_arg, value_buf, sizeof(value_buf));
	if (flag) {
		const char *endp;
		long value = malloc_common_convert_to_long(flag, &endp);
		if (!*endp && (value == 0 || value == 1)) {
			malloc_zero_policy = value ? MALLOC_ZERO_ON_FREE : MALLOC_ZERO_NONE;
		} else {
			malloc_report(ASL_LEVEL_ERR,
					"malloc_zero_on_free_enabled must be 0 or 1 - ignored.\n");
		}
	}

	flag = malloc_common_value_for_key_copy(bootargs,
			zero_on_free_sample_period_boot_arg, value_buf, sizeof(value_buf));
	if (flag) {
		const char *endp;
		long value = malloc_common_convert_to_long(flag, &endp);
		if (!*endp && value >= 0) {
			malloc_zero_on_free_sample_period = (unsigned int)value;
		} else {
			malloc_report(ASL_LEVEL_ERR,
					"malloc_zero_on_free_sample_period must be positive - ignored.\n");
		}
	}


#if CONFIG_MEDIUM_ALLOCATOR
#if TARGET_OS_OSX
#if defined(__x86_64__)
	if (__is_translated()) {
		magazine_medium_active_threshold = 0;
	}
#elif defined(__arm64__)
	magazine_medium_active_threshold = 0;
#endif
#endif /* TARGET_OS_OSX */

	flag = malloc_common_value_for_key_copy(bootargs, medium_enabled_boot_arg,
			value_buf, sizeof(value_buf));
	if (flag) {
		const char *endp;
		long value = malloc_common_convert_to_long(flag, &endp);
		if (!*endp) {
			magazine_medium_enabled = (value != 0);
		}
	}

	flag = malloc_common_value_for_key_copy(bootargs,
			medium_activation_threshold_boot_arg, value_buf, sizeof(value_buf));
	if (flag) {
		const char *endp;
		long value = malloc_common_convert_to_long(flag, &endp);
		if (!*endp && value >= 0) {
			magazine_medium_active_threshold = (uint64_t)value;
		} else {
			malloc_report(ASL_LEVEL_ERR,
					"malloc_medium_activation_threshold must be positive - ignored.\n");
		}
	}

	flag = malloc_common_value_for_key_copy(bootargs,
			max_medium_magazines_boot_arg, value_buf, sizeof(value_buf));
	if (flag) {
		const char *endp;
		long value = malloc_common_convert_to_long(flag, &endp);
		if (!*endp && value >= 0) {
			max_medium_magazines = (int)value;
		} else {
			malloc_report(ASL_LEVEL_ERR,
					"malloc_max_medium_magazines must be positive - ignored.\n");
		}
	}

	flag = malloc_common_value_for_key_copy(bootargs,
			medium_space_efficient_boot_arg, value_buf, sizeof(value_buf));
	if (flag) {
		const char *endp;
		long value = malloc_common_convert_to_long(flag, &endp);
		if (!*endp) {
			malloc_medium_space_efficient_enabled = (value != 0);
		}
	}

	flag = malloc_common_value_for_key_copy(bootargs, medium_madvise_dram_scale_divisor_boot_arg, value_buf, sizeof(value_buf));
	if (flag) {
		const char *endp;
		long value = malloc_common_convert_to_long(flag, &endp);
		if (!*endp && value >= 0) {
			magazine_medium_madvise_dram_scale_divisor = (uint64_t)value;
		} else {
			malloc_report(ASL_LEVEL_ERR,
					"malloc_medium_madvise_dram_scale_divisor must be positive - ignored.\n");
		}
	}
#endif // CONFIG_MEDIUM_ALLOCATOR
}

#if CONFIG_MALLOC_PROCESS_IDENTITY

static void
_malloc_check_process_identity(const char *apple[])
{
	static const struct {
		const char *name;
		malloc_process_identity_t identity;
	} name_identity_mapping[] = {
#if TARGET_OS_SIMULATOR
		{ "launchd_sim",             MALLOC_PROCESS_LAUNCHD, },
		{ "launchd_sim.development", MALLOC_PROCESS_LAUNCHD, },
		{ "launchd_sim.debug",       MALLOC_PROCESS_LAUNCHD, },
#else // TARGET_OS_SIMULATOR
		{ "launchd",             MALLOC_PROCESS_LAUNCHD, },
		{ "launchd.development", MALLOC_PROCESS_LAUNCHD, },
		{ "launchd.debug",       MALLOC_PROCESS_LAUNCHD, },
		{ "launchd.testing",     MALLOC_PROCESS_LAUNCHD, },
#endif // TARGET_OS_SIMULATOR
		{ "logd",                MALLOC_PROCESS_LOGD, },
		{ "notifyd",             MALLOC_PROCESS_NOTIFYD, },

		{ "mediaparserd",        MALLOC_PROCESS_MEDIAPARSERD, },
		{ "videocodecd",         MALLOC_PROCESS_VIDEOCODECD, },
		{ "mediaplaybackd",      MALLOC_PROCESS_MEDIAPLAYBACKD },
		{ "audiomxd",            MALLOC_PROCESS_AUDIOMXD, },
		{ "avconferenced",       MALLOC_PROCESS_AVCONFERENCED, },
		{ "mediaserverd",        MALLOC_PROCESS_MEDIASERVERD, },
		{ "cameracaptured",      MALLOC_PROCESS_CAMERACAPTURED, },

		{ "MessagesBlastDoorService", MALLOC_PROCESS_BLASTDOOR_MESSAGES, },
		{ "MessagesAirlockService",   MALLOC_PROCESS_BLASTDOOR_MESSAGES, },
		{ "IDSBlastDoorService",      MALLOC_PROCESS_BLASTDOOR_IDS, },
		{ "IMDPersistenceAgent",      MALLOC_PROCESS_IMDPERSISTENCEAGENT },
		{ "imagent",                  MALLOC_PROCESS_IMAGENT, },

		{ "ThumbnailExtensionSecure", MALLOC_PROCESS_QUICKLOOK_THUMBNAIL_SECURE, },
		{ "com.apple.quicklook.extension.previewUI", MALLOC_PROCESS_QUICKLOOK_PREVIEW, },
		{ "QuickLookUIExtension",     MALLOC_PROCESS_QUICKLOOK_PREVIEW, },
		{ "ThumbnailExtension",       MALLOC_PROCESS_QUICKLOOK_THUMBNAIL },
#if TARGET_OS_OSX
		// Load-bearing: not already MallocSpaceEfficient
		{ "QuickLookUIService",       MALLOC_PROCESS_QUICKLOOK_UISERVICE },
		// Already MallocSpaceEfficient, but valuable to identify as
		// security-relevant for other special treatment
		{ "ThumbnailExtension_macOS", MALLOC_PROCESS_QUICKLOOK_THUMBNAIL },
		{ "QuickLookSatellite",       MALLOC_PROCESS_QUICKLOOK_MACOS },
		{ "quicklookd",               MALLOC_PROCESS_QUICKLOOK_MACOS },
		{ "com.apple.quicklook.ThumbnailsAgent", MALLOC_PROCESS_QUICKLOOK_MACOS },
		{ "ExternalQuickLookSatellite-arm64",    MALLOC_PROCESS_QUICKLOOK_MACOS },
		{ "ExternalQuickLookSatellite-x86_64",   MALLOC_PROCESS_QUICKLOOK_MACOS },
#endif // TARGET_OS_OSX

		{ "MobileSafari",                MALLOC_PROCESS_BROWSER, },
		{ "com.apple.WebKit.Networking", MALLOC_PROCESS_BROWSER, },
		{ "com.apple.WebKit.GPU",        MALLOC_PROCESS_BROWSER, },
		{ "com.apple.WebKit.WebContent", MALLOC_PROCESS_BROWSER, },
		{ "com.apple.WebKit.WebContent.CaptivePortal", MALLOC_PROCESS_BROWSER, },
		{ "MTLCompilerService",          MALLOC_PROCESS_MTLCOMPILERSERVICE },

#if TARGET_OS_OSX
		{ "Safari",                                      MALLOC_PROCESS_SAFARI, },
		{ "com.apple.Safari.CredentialExtractionHelper", MALLOC_PROCESS_SAFARI_SUPPORT, },
		{ "com.apple.Safari.History",                    MALLOC_PROCESS_SAFARI_SUPPORT, },
		{ "com.apple.Safari.SandboxBroker",              MALLOC_PROCESS_SAFARI_SUPPORT, },
		{ "com.apple.Safari.SearchHelper",               MALLOC_PROCESS_SAFARI_SUPPORT, },
		{ "com.apple.SafariFoundation.CredentialProviderExtensionHelper", MALLOC_PROCESS_SAFARI_SUPPORT, },
		{ "com.apple.SafariPlatformSupport.Helper",      MALLOC_PROCESS_SAFARI_SUPPORT, },
		{ "com.apple.SafariServices.ExtensionHelper",    MALLOC_PROCESS_SAFARI_SUPPORT, },
		{ "com.apple.SafariServices",                    MALLOC_PROCESS_SAFARI_SUPPORT, },
		{ "VTDecoderXPCService",                         MALLOC_PROCESS_VTDECODERXPCSERVICE, },
#endif // TARGET_OS_OSX

		{ "callservicesd",       MALLOC_PROCESS_CALLSERVICESD, },
		{ "maild",               MALLOC_PROCESS_MAILD, },
		{ "mDNSResponder",       MALLOC_PROCESS_MDNSRESPONDER, },
		{ "ASVAssetViewer",      MALLOC_PROCESS_ASVASSETVIEWER },
		{ "identityservicesd",   MALLOC_PROCESS_IDENTITYSERVICESD, },
		{ "wifid",               MALLOC_PROCESS_WIFID, },
		{ "fmfd",                MALLOC_PROCESS_FMFD, },
		{ "searchpartyd",        MALLOC_PROCESS_SEARCHPARTYD, },
		{ "vmd",                 MALLOC_PROCESS_VMD, },
		{ "CommCenter",          MALLOC_PROCESS_COMMCENTER, },
		{ "wifip2pd",            MALLOC_PROCESS_WIFIP2PD, },
		{ "wifianalyticsd",      MALLOC_PROCESS_WIFIANALYTICSD, },

		{ "mds_stores",          MALLOC_PROCESS_MDS_STORES },

		{ "AegirPoster",         MALLOC_PROCESS_AEGIRPOSTER, },
		{ "CollectionsPoster",   MALLOC_PROCESS_COLLECTIONSPOSTER, },
	};

	if (getpid() == 1) {
		malloc_process_identity = MALLOC_PROCESS_LAUNCHD;
		return;
	}

	const char *flag = _simple_getenv(apple, "HardenedRuntime");
	if (flag) {
		unsigned long long value = strtoull(flag, NULL, 0);
		if (value) {
			// reproduced from xnu
			enum {
				MallocBrowserHostEntitlementMask       = 0x01,
				MallocBrowserGPUEntitlementMask        = 0x02,
				MallocBrowserNetworkEntitlementMask    = 0x04,
				MallocBrowserWebContentEntitlementMask = 0x08,
			};

			long enablement_mask = MallocBrowserHostEntitlementMask |
					MallocBrowserGPUEntitlementMask |
					MallocBrowserNetworkEntitlementMask |
					MallocBrowserWebContentEntitlementMask;
			if (value & enablement_mask) {
				malloc_process_identity = MALLOC_PROCESS_BROWSER;
				return;
			}
		}
	}

	const char *name = getprogname();
	for (size_t i = 0; i < countof(name_identity_mapping); i++) {
		if (!strcmp(name, name_identity_mapping[i].name)) {
			malloc_process_identity = name_identity_mapping[i].identity;
			break;
		}
	}
}

#if !CONFIG_FEATUREFLAGS_SIMPLE
#error "must have feature flags"
#endif

static bool
_malloc_check_secure_allocator_process_enablement(void)
{
	// launchd is special because the feature flag check can't work for it
	if (malloc_process_identity == MALLOC_PROCESS_LAUNCHD) {
		return MALLOC_SECURE_ALLOCATOR_LAUNCHD_ENABLED_DEFAULT;
	}

#define ENABLEMENT_CASE(id, enable_status) \
		case MALLOC_PROCESS_##id: \
			return (enable_status);
#define ENABLEMENT_CASE_FF(id, name, darwin_default, simulator_default) \
		case MALLOC_PROCESS_##id: \
			return malloc_secure_feature_enabled(SecureAllocator_process_##name, \
					(darwin_default), (simulator_default))


	switch (malloc_process_identity) {
	ENABLEMENT_CASE(LOGD, true);
	ENABLEMENT_CASE(NOTIFYD, true);

	ENABLEMENT_CASE(MEDIAPARSERD, true);
	ENABLEMENT_CASE(VIDEOCODECD, true);
	ENABLEMENT_CASE(MEDIAPLAYBACKD, true);
	ENABLEMENT_CASE(AVCONFERENCED, true);
	ENABLEMENT_CASE(MEDIASERVERD, true);
	ENABLEMENT_CASE(AUDIOMXD, true);
	ENABLEMENT_CASE(CAMERACAPTURED, true);

	ENABLEMENT_CASE(BLASTDOOR_MESSAGES, true);
	ENABLEMENT_CASE(BLASTDOOR_IDS, true);
	ENABLEMENT_CASE(IMDPERSISTENCEAGENT, true);
	ENABLEMENT_CASE(IMAGENT, true);

#if TARGET_OS_OSX
	ENABLEMENT_CASE(QUICKLOOK_MACOS, true);
	ENABLEMENT_CASE(QUICKLOOK_UISERVICE, true);
#endif
	ENABLEMENT_CASE(QUICKLOOK_THUMBNAIL_SECURE, true);
	ENABLEMENT_CASE(QUICKLOOK_PREVIEW, true);
	ENABLEMENT_CASE(QUICKLOOK_THUMBNAIL, true);

#if TARGET_OS_OSX
	ENABLEMENT_CASE_FF(MTLCOMPILERSERVICE, MTLCompilerService, false, false);
#else
	ENABLEMENT_CASE(MTLCOMPILERSERVICE, true);
#endif

	ENABLEMENT_CASE(CALLSERVICESD, true);
	ENABLEMENT_CASE(MAILD, true);
	ENABLEMENT_CASE(MDNSRESPONDER, true);
	ENABLEMENT_CASE(ASVASSETVIEWER, true);
	ENABLEMENT_CASE(IDENTITYSERVICESD, true);
	ENABLEMENT_CASE(WIFID, true);
	ENABLEMENT_CASE(FMFD, true);
	ENABLEMENT_CASE(SEARCHPARTYD, true);
	ENABLEMENT_CASE(VMD, true);
	ENABLEMENT_CASE(WIFIP2PD, true);
	ENABLEMENT_CASE(WIFIANALYTICSD, true);

	ENABLEMENT_CASE(COMMCENTER, true);

#if TARGET_OS_SIMULATOR
	ENABLEMENT_CASE(BROWSER, false);
#else
	ENABLEMENT_CASE(BROWSER, true);
#endif // TARGET_OS_SIMULATOR

#if TARGET_OS_OSX
	ENABLEMENT_CASE(SAFARI, true);
	ENABLEMENT_CASE(SAFARI_SUPPORT, true);
#endif

	ENABLEMENT_CASE_FF(AEGIRPOSTER, aegirposter, false, false);
	ENABLEMENT_CASE_FF(COLLECTIONSPOSTER, CollectionsPoster, false, false);

#if TARGET_OS_OSX
	ENABLEMENT_CASE(VTDECODERXPCSERVICE, true);
#endif

	ENABLEMENT_CASE(MDS_STORES, true);

	default:
		return false;
	}
}

#endif // CONFIG_MALLOC_PROCESS_IDENTITY

static void
_malloc_init_featureflags(void)
{
#if CONFIG_FEATUREFLAGS_SIMPLE
	bool zero_on_free_feature_enabled = os_feature_enabled_simple(libmalloc,
			ZeroOnFree, MALLOC_ZERO_POLICY_DEFAULT == MALLOC_ZERO_ON_FREE);
	bool policy_is_zero_on_free = (malloc_zero_policy == MALLOC_ZERO_ON_FREE);
	if (zero_on_free_feature_enabled != policy_is_zero_on_free) {
		malloc_zero_policy = zero_on_free_feature_enabled ?
				MALLOC_ZERO_ON_FREE : MALLOC_ZERO_NONE;
	}

	bool secure_allocator = false;
#if CONFIG_MALLOC_PROCESS_IDENTITY
	if (malloc_process_identity != MALLOC_PROCESS_NONE) {
		secure_allocator = _malloc_check_secure_allocator_process_enablement();
	} else
#endif // CONFIG_MALLOC_PROCESS_IDENTITY
	{
#if MALLOC_TARGET_IOS_ONLY
		secure_allocator = malloc_secure_feature_enabled(
				SecureAllocator_SystemWide, true, true);
#else
		secure_allocator = malloc_secure_feature_enabled(
				SecureAllocator_SystemWide, false, false);
#endif	// MALLOC_TARGET_IOS_ONLY
	}

#if TARGET_OS_OSX && !TARGET_CPU_ARM64
	if (!os_feature_enabled_simple(libmalloc, SecureAllocator_Intel, false)) {
		secure_allocator = false;
	}
#endif // TARGET_OS_OSX && !TARGET_CPU_ARM64

#if TARGET_OS_OSX
	bool allow_non_platform = os_feature_enabled_simple(libmalloc,
			SecureAllocator_NonPlatform, false);
	if (malloc_xzone_allow_non_platform != allow_non_platform) {
		malloc_xzone_allow_non_platform = allow_non_platform;
	}
#endif // TARGET_OS_OSX

	if (secure_allocator != malloc_xzone_enabled) {
		malloc_xzone_enabled = secure_allocator;
	}
#endif // CONFIG_FEATUREFLAGS_SIMPLE
}

extern malloc_zone_t *force_asan_init_if_present(void)
		asm("_malloc_default_zone");

void
__malloc_init(const char *apple[])
{
	// We could try to be clever and cater for arbitrary length bootarg
	// strings, but it's probably not worth it, especially as we would need
	// to temporarily allocate at least a page of memory to read the bootargs
	// into.
	char bootargs[1024] = { '\0' };
	bool allow_bootargs = true;
#if CONFIG_FEATUREFLAGS_SIMPLE
	// os_feature_enabled_simple() doesn't work in launchd, but launchd should
	// be able to read boot-args
	if (getpid() != 1) {
		allow_bootargs &= os_feature_enabled_simple(libmalloc, EnableBootArgs, false);
	}
#endif
#if defined(_COMM_PAGE_DEV_FIRM)
	allow_bootargs &= !!*((uint32_t *)_COMM_PAGE_DEV_FIRM);
#endif // _COMM_PAGE_DEV_FIRM

	size_t len = sizeof(bootargs) - 1;
	if (allow_bootargs &&
			!sysctlbyname("kern.bootargs", bootargs, &len, NULL, 0) &&
			len > 0) {
		bootargs[len + 1] = '\0';
	}

	// Cache the calculation of this "constant", which unfortunately depends on
	// runtime values of vm_kernel_page_size and vm_page_size
	malloc_absolute_max_size = _MALLOC_ABSOLUTE_MAX_SIZE;

#if CONFIG_CHECK_PLATFORM_BINARY
	bool is_platform_binary = _malloc_is_platform_binary();
	if (malloc_is_platform_binary != is_platform_binary) {
		malloc_is_platform_binary = is_platform_binary;
	}
#endif

#if CONFIG_CHECK_SECURITY_POLICY
	bool allow_internal_security = _malloc_allow_internal_security_policy();
	if (allow_internal_security != malloc_internal_security_policy) {
		malloc_internal_security_policy = allow_internal_security;
	}
#endif

#if CONFIG_MALLOC_PROCESS_IDENTITY
	_malloc_check_process_identity(apple);
#endif

	_malloc_init_featureflags();

	const char **p;
	const char *malloc_experiments = NULL;
	for (p = apple; p && *p; p++) {
		if (strstr(*p, "malloc_entropy") == *p) {
			int count = __entropy_from_kernel(*p);
			bzero((void *)*p, strlen(*p));

			if (sizeof(malloc_entropy) / sizeof(malloc_entropy[0]) == count) {
				_malloc_entropy_initialized = true;
			}
		}
		if (strstr(*p, LIBMALLOC_EXPERIMENT_FACTORS_KEY) == *p) {
			malloc_experiments = *p;
		}
#if CONFIG_DEFERRED_RECLAIM
		if (strstr(*p, LIBMALLOC_DEFERRED_RECLAIM_ENABLE) == *p) {
			// Turn on the large cache which will place
			// its free entries in the deferred reclaim buffer
			large_cache_enabled = 1;
		}
#endif /* CONFIG_DEFERRED_RECLAIM */
	}
	if (!_malloc_entropy_initialized) {
		getentropy((void*)malloc_entropy, sizeof(malloc_entropy));
		_malloc_entropy_initialized = true;
	}

	if (malloc_experiments) {
		__malloc_init_experiments(malloc_experiments);
	}
	__malloc_init_from_bootargs(bootargs);
	mvm_aslr_init();

	/*
	 * This really is a renamed call to malloc_default_zone() which is
	 * interposable and interposed by asan, so that we trigger the lazy
	 * initialization of asan _BEFORE_ _malloc_initialize().
	 *
	 * If we do it after, then _malloc_initialize() then ASAN will replace
	 * the system allocator too late and bad things happen.
	 */
	force_asan_init_if_present();

	_malloc_initialize(apple, bootargs);
}

static void register_pgm_zone(bool internal_diagnostics);
static void stack_logging_early_finished(const struct _malloc_late_init *funcs);

// WARNING: The passed _malloc_late_init is a stack variable in
// libSystem_initializer().  We must not hold on to it.
//
// WARNING 2: By the time this is called, malloc() is already used
// a bunch of times (from libobjc, libxpc, ...).
void
__malloc_late_init(const struct _malloc_late_init *mli)
{
	register_pgm_zone(mli->internal_diagnostics);
	stack_logging_early_finished(mli);
	initial_num_zones = malloc_num_zones;
	
#if CONFIG_SANITIZER
	if (malloc_sanitizer_enabled) {
		sanitizer_reset_environment();

#if TARGET_OS_OSX
		// Fetch the memory poisoning functions from the upstream ASan runtime
		sanitizer_fallback_ptrs.memory_poison = _dlsym(RTLD_MAIN_ONLY, "__asan_poison_memory_region");
		sanitizer_fallback_ptrs.memory_unpoison = _dlsym(RTLD_MAIN_ONLY, "__asan_unpoison_memory_region");

		// Prefer the heap allocation functions from our ASan runtime, if available
		void *heap_alloc = _dlsym(RTLD_MAIN_ONLY, "__asan_poison_heap_memory_alloc"),
			 *heap_dealloc = _dlsym(RTLD_MAIN_ONLY, "__asan_poison_heap_memory_free"),
			 *heap_internal_poison = _dlsym(RTLD_MAIN_ONLY, "__asan_poison_heap_memory_internal");
		if (heap_alloc && heap_dealloc && heap_internal_poison) {
			sanitizer_fallback.heap_allocate_poison = heap_alloc;
			sanitizer_fallback.heap_deallocate_poison = heap_dealloc;
			sanitizer_fallback.heap_internal_poison = heap_internal_poison;
		} else {
			sanitizer_fallback.heap_allocate_poison = &malloc_sanitizer_fallback_allocate_poison;
			sanitizer_fallback.heap_deallocate_poison = &malloc_sanitizer_fallback_deallocate_poison;
			sanitizer_fallback.heap_internal_poison = &malloc_sanitizer_fallback_deallocate_poison;
		}
#endif
	}
#endif
}

MALLOC_ALWAYS_INLINE
static inline malloc_zone_t *
runtime_default_zone(void) {
	return (lite_zone) ? lite_zone : inline_malloc_default_zone();
}

static size_t
default_zone_size(malloc_zone_t *zone, const void *ptr)
{
	zone = runtime_default_zone();
	
	return zone->size(zone, ptr);
}

static void *
default_zone_malloc(malloc_zone_t *zone, size_t size)
{
	zone = runtime_default_zone();
	
	return zone->malloc(zone, size);
}

static void *
default_zone_calloc(malloc_zone_t *zone, size_t num_items, size_t size)
{
	zone = runtime_default_zone();
	
	return zone->calloc(zone, num_items, size);
}

static void *
default_zone_valloc(malloc_zone_t *zone, size_t size)
{
	zone = runtime_default_zone();
	
	return zone->valloc(zone, size);
}

static void
default_zone_free(malloc_zone_t *zone, void *ptr)
{
	zone = runtime_default_zone();
	
	return zone->free(zone, ptr);
}

static void *
default_zone_realloc(malloc_zone_t *zone, void *ptr, size_t new_size)
{
	zone = runtime_default_zone();
	
	return zone->realloc(zone, ptr, new_size);
}

static void
default_zone_destroy(malloc_zone_t *zone)
{
	zone = runtime_default_zone();
	
	return zone->destroy(zone);
}

static unsigned
default_zone_batch_malloc(malloc_zone_t *zone, size_t size, void **results, unsigned count)
{
	zone = runtime_default_zone();
	
	return zone->batch_malloc(zone, size, results, count);
}

static void
default_zone_batch_free(malloc_zone_t *zone, void **to_be_freed, unsigned count)
{
	zone = runtime_default_zone();
	
	return zone->batch_free(zone, to_be_freed, count);
}

static void *
default_zone_memalign(malloc_zone_t *zone, size_t alignment, size_t size)
{
	zone = runtime_default_zone();
	
	return zone->memalign(zone, alignment, size);
}

static void
default_zone_free_definite_size(malloc_zone_t *zone, void *ptr, size_t size)
{
	zone = runtime_default_zone();
	
	return zone->free_definite_size(zone, ptr, size);
}

static size_t
default_zone_pressure_relief(malloc_zone_t *zone, size_t goal)
{
	zone = runtime_default_zone();

	return zone->pressure_relief(zone, goal);
}

static boolean_t
default_zone_malloc_claimed_address(malloc_zone_t *zone, void *ptr)
{
	zone = runtime_default_zone();

	return malloc_zone_claimed_address(zone, ptr);
}

static kern_return_t
default_zone_ptr_in_use_enumerator(task_t task,
								   void *context,
								   unsigned type_mask,
								   vm_address_t zone_address,
								   memory_reader_t reader,
								   vm_range_recorder_t recorder)
{
	malloc_zone_t *zone = runtime_default_zone();
	
	return zone->introspect->enumerator(task, context, type_mask, (vm_address_t) zone, reader, recorder);
}

static size_t
default_zone_good_size(malloc_zone_t *zone, size_t size)
{
	zone = runtime_default_zone();
	
	return zone->introspect->good_size(zone, size);
}

static boolean_t
default_zone_check(malloc_zone_t *zone)
{
	zone = runtime_default_zone();
	
	return zone->introspect->check(zone);
}

static void
default_zone_print(malloc_zone_t *zone, boolean_t verbose)
{
	zone = runtime_default_zone();

	return (void)zone->introspect->print(zone, verbose);
}

static void
default_zone_log(malloc_zone_t *zone, void *log_address)
{
	zone = runtime_default_zone();
	
	return zone->introspect->log(zone, log_address);
}

static void
default_zone_force_lock(malloc_zone_t *zone)
{
	zone = runtime_default_zone();
	
	return zone->introspect->force_lock(zone);
}

static void
default_zone_force_unlock(malloc_zone_t *zone)
{
	zone = runtime_default_zone();
	
	return zone->introspect->force_unlock(zone);
}

static void
default_zone_statistics(malloc_zone_t *zone, malloc_statistics_t *stats)
{
	zone = runtime_default_zone();
	
	return zone->introspect->statistics(zone, stats);
}

static boolean_t
default_zone_locked(malloc_zone_t *zone)
{
	zone = runtime_default_zone();
	
	return zone->introspect->zone_locked(zone);
}

static void
default_zone_reinit_lock(malloc_zone_t *zone)
{
	zone = runtime_default_zone();
	
	return zone->introspect->reinit_lock(zone);
}

static struct malloc_introspection_t default_zone_introspect = {
	default_zone_ptr_in_use_enumerator,
	default_zone_good_size,
	default_zone_check,
	default_zone_print,
	default_zone_log,
	default_zone_force_lock,
	default_zone_force_unlock,
	default_zone_statistics,
	default_zone_locked,
	NULL,
	NULL,
	NULL,
	NULL,
	default_zone_reinit_lock
};

typedef struct {
	malloc_zone_t malloc_zone;
	uint8_t pad[PAGE_MAX_SIZE - sizeof(malloc_zone_t)];
} virtual_default_zone_t;

static virtual_default_zone_t virtual_default_zone
__attribute__((section("__DATA,__v_zone")))
__attribute__((aligned(PAGE_MAX_SIZE))) = {
	NULL,
	NULL,
	default_zone_size,
	default_zone_malloc,
	default_zone_calloc,
	default_zone_valloc,
	default_zone_free,
	default_zone_realloc,
	default_zone_destroy,
	DEFAULT_MALLOC_ZONE_STRING,
	default_zone_batch_malloc,
	default_zone_batch_free,
	&default_zone_introspect,
	10,
	default_zone_memalign,
	default_zone_free_definite_size,
	default_zone_pressure_relief,
	default_zone_malloc_claimed_address,
};

MALLOC_NOEXPORT malloc_zone_t *default_zone = &virtual_default_zone.malloc_zone;

MALLOC_NOEXPORT
/*static*/ boolean_t
has_default_zone0(void)
{
	return !has_injected_zone0;
}

static inline malloc_zone_t *_find_registered_zone(const void *, size_t *, bool) __attribute__((always_inline));
static inline malloc_zone_t *
_find_registered_zone(const void *ptr, size_t *returned_size, bool known_non_default)
{
	// Returns a zone which contains ptr, else NULL

	if (0 == malloc_num_zones) {
		if (returned_size) {
			*returned_size = 0;
		}
		return NULL;
	}

	// first look in the lite zone
	if (lite_zone) {
		malloc_zone_t *zone = lite_zone;
		size_t size = zone->size(zone, ptr);
		if (size) { // Claimed by this zone?
			if (returned_size) {
				*returned_size = size;
			}
			// Return the virtual default zone instead of the lite zone - see <rdar://problem/24994311>
			return default_zone;
		}
	}

	malloc_zone_t *zone;
	size_t size;

	// We assume that the initial zones will never be unregistered concurrently while this code is running so we can have
	// a fast path without synchronization.  Callers who really do unregister these (to install their own default zone) need
	// to ensure they establish their zone setup during initialization and before entering a multi-threaded environment.
	for (uint32_t i = known_non_default ? 1 : 0; i < initial_num_zones; i++) {
		zone = malloc_zones[i];
		size = zone->size(zone, ptr);

		if (size) { // Claimed by this zone?
			if (returned_size) {
				*returned_size = size;
			}

			// Asan and others replace the zone at position 0 with their own zone.
			// In that case just return that zone as they need this information.
			// Otherwise return the virtual default zone, not the actual zone in position 0.
			if (i == 0 && has_default_zone0()) {
				return default_zone;
			}

			return zone;
		}
	}

	int32_t volatile *pFRZCounter = pFRZCounterLive;   // Capture pointer to the counter of the moment
	OSAtomicIncrement32Barrier(pFRZCounter); // Advance this counter -- our thread is in FRZ

	int32_t limit = *(int32_t volatile *)&malloc_num_zones;

	// From this point on, FRZ is accessing the malloc_zones[] array without locking
	// in order to avoid contention on common operations (such as non-default-zone free()).
	// In order to ensure that this is actually safe to do, register/unregister take care
	// to:
	//
	//   1. Register ensures that newly inserted pointers in malloc_zones[] are visible
	//      when malloc_num_zones is incremented. At the moment, we're relying on that store
	//      ordering to work without taking additional steps here to ensure load memory
	//      ordering.
	//
	//   2. Unregister waits for all readers in FRZ to complete their iteration before it
	//      returns from the unregister call (during which, even unregistered zone pointers
	//      are still valid). It also ensures that all the pointers in the zones array are
	//      valid until it returns, so that a stale value in limit is not dangerous.

	for (uint32_t i = initial_num_zones; i < limit; i++) {
		zone = malloc_zones[i];
		size = zone->size(zone, ptr);
		if (size) { // Claimed by this zone?
			goto out;
		}
	}
	// Unclaimed by any zone.
	zone = NULL;
	size = 0;
out:
	if (returned_size) {
		*returned_size = size;
	}
	OSAtomicDecrement32Barrier(pFRZCounter); // our thread is leaving FRZ
	return zone;
}

malloc_zone_t *
find_registered_zone(const void *ptr, size_t *returned_size,
		bool known_non_default)
{
	return _find_registered_zone(ptr, returned_size, known_non_default);
}

void
malloc_error_break(void)
{
	// Provides a non-inlined place for various malloc error procedures to call
	// that will be called after an error message appears.  It does not make
	// sense for developers to call this function, so it is marked
	// hidden to prevent it from becoming API.
	MAGMALLOC_MALLOCERRORBREAK(); // DTrace USDT probe
}

int
malloc_gdb_po_unsafe(void)
{
	// In order to implement "po" other data formatters in gdb, the debugger
	// calls functions that call malloc.  The debugger will  only run one thread
	// of the program in this case, so if another thread is holding a zone lock,
	// gdb may deadlock in this case.
	//
	// Iterate over the zones in malloc_zones, and call "trylock" on the zone
	// lock.  If trylock succeeds, unlock it, otherwise return "locked".  Returns
	// 0 == safe, 1 == locked/unsafe.

	if (msl.stack_logging_locked && msl.stack_logging_locked()) {
		return 1;
	}

	malloc_zone_t **zones = malloc_zones;
	unsigned i, e = malloc_num_zones;

	for (i = 0; i != e; ++i) {
		malloc_zone_t *zone = zones[i];

		// Version must be >= 5 to look at the new introspection field.
		if (zone->version < 5) {
			continue;
		}

		if (zone->introspect->zone_locked && zone->introspect->zone_locked(zone)) {
			return 1;
		}
	}
	return 0;
}

/*********	Creation and destruction	************/

static void set_flags_from_environment(void);

MALLOC_NOEXPORT void
malloc_zone_register_while_locked(malloc_zone_t *zone, bool make_default)
{
	size_t protect_size;
	unsigned i;

	/* scan the list of zones, to see if this zone is already registered.  If
	 * so, print an error message and return. */
	for (i = 0; i != malloc_num_zones; ++i) {
		if (zone == malloc_zones[i]) {
			malloc_report(ASL_LEVEL_ERR, "Attempted to register zone more than once: %p\n", zone);
			return;
		}
	}

	if (malloc_num_zones == malloc_num_zones_allocated) {
		size_t malloc_zones_size = malloc_num_zones * sizeof(malloc_zone_t *);
		mach_vm_size_t alloc_size = round_page(malloc_zones_size + vm_page_size);
		mach_vm_address_t vm_addr;
		int alloc_flags = VM_FLAGS_ANYWHERE | VM_MAKE_TAG(VM_MEMORY_MALLOC);

		vm_addr = vm_page_size;
		kern_return_t kr = mach_vm_allocate(mach_task_self(), &vm_addr, alloc_size, alloc_flags);
		if (kr) {
			malloc_report(ASL_LEVEL_ERR, "malloc_zone_register allocation failed: %d\n", kr);
			return;
		}

		malloc_zone_t **new_zones = (malloc_zone_t **)vm_addr;
		/* If there were previously allocated malloc zones, we need to copy them
		 * out of the previous array and into the new zones array */
		if (malloc_zones) {
			memcpy(new_zones, malloc_zones, malloc_zones_size);
			vm_addr = (mach_vm_address_t)malloc_zones;
			mach_vm_size_t dealloc_size = round_page(malloc_zones_size);
			mach_vm_deallocate(mach_task_self(), vm_addr, dealloc_size);
		}

		/* Update the malloc_zones pointer, which we leak if it was previously
		 * allocated, and the number of zones allocated */
		protect_size = (size_t)alloc_size;
		malloc_zones = new_zones;
		malloc_num_zones_allocated = (int32_t)(alloc_size / sizeof(malloc_zone_t *));
	} else {
		/* If we don't need to reallocate zones, we need to briefly change the
		 * page protection the malloc zones to allow writes */
		protect_size = malloc_num_zones_allocated * sizeof(malloc_zone_t *);
		mprotect(malloc_zones, protect_size, PROT_READ | PROT_WRITE);
	}

	if (make_default) {
		memmove(&malloc_zones[1], &malloc_zones[0], malloc_num_zones * sizeof(malloc_zone_t *));
		malloc_zones[0] = zone;
	} else {
		malloc_zones[malloc_num_zones] = zone;
	}

	/* <rdar://problem/12871662> This store-increment needs to be visible in the correct
	 * order to any threads in find_registered_zone, such that if the incremented value
	 * in malloc_num_zones is visible then the pointer write before it must also be visible.
	 *
	 * While we could be slightly more efficent here with atomic ops the cleanest way to
	 * ensure the proper store-release operation is performed is to use OSAtomic*Barrier
	 * to update malloc_num_zones.
	 */
	OSAtomicIncrement32Barrier(&malloc_num_zones);

	/* Finally, now that the zone is registered, disallow write access to the
	 * malloc_zones array */
	mprotect(malloc_zones, protect_size, PROT_READ);

	// ASan and others interpose on mprotect() to ensure their zone is always at position 0.
	if (make_default && malloc_zones[0] != zone) {
		has_injected_zone0 = true;
		malloc_slowpath_update();
	}

	//malloc_report(ASL_LEVEL_INFO, "Registered malloc_zone %p in malloc_zones %p [%u zones, %u bytes]\n", zone, malloc_zones,
	// malloc_num_zones, protect_size);
}

// This used to be called lazyily because it is using
// dyld_process_is_restricted() before dyld_init() has run.
//
// However this function is safe to use, we keep this function separate
// if we ever need to have a 2-stage init in the future.
static void
_malloc_initialize(const char *apple[], const char *bootargs)
{
	phys_ncpus = *(uint8_t *)(uintptr_t)_COMM_PAGE_PHYSICAL_CPUS;
	logical_ncpus = *(uint8_t *)(uintptr_t)_COMM_PAGE_LOGICAL_CPUS;
#if CONFIG_MAGAZINE_PER_CLUSTER
	{
		ncpuclusters = *(uint8_t *)(uintptr_t)_COMM_PAGE_CPU_CLUSTERS;
	}
#endif // CONFIG_MAGAZINE_PER_CLUSTER

	if (0 != (logical_ncpus % phys_ncpus)) {
		MALLOC_REPORT_FATAL_ERROR(logical_ncpus % phys_ncpus,
				"logical_ncpus %% phys_ncpus != 0\n");
	}

	switch (logical_ncpus / phys_ncpus) {
	case 1:
		hyper_shift = 0;
		break;
	case 2:
		hyper_shift = 1;
		break;
	case 4:
		hyper_shift = 2;
		break;
	default:
		MALLOC_REPORT_FATAL_ERROR(logical_ncpus / phys_ncpus, "logical_ncpus / phys_ncpus not 1, 2, or 4");
	}

	// max_magazines may already be set from a boot argument. Make sure that it
	// is bounded by the number of CPUs.
	if (max_magazines) {
		max_magazines = MIN(max_magazines, logical_ncpus);
	} else {
		max_magazines = logical_ncpus;
	}

	// similiarly, cap medium magazines at logical_ncpus but don't cap it by
	// the max magazines if it has been set explicitly
	if (max_medium_magazines) {
		max_medium_magazines = MIN(max_medium_magazines, logical_ncpus);
	} else {
		max_medium_magazines = max_magazines;
	}

	// The "single-cluster" concept doesn't apply to macOS: there is no
	// non-Intel real hardware without multiple AMP clusters, so this will only
	// happen under virtualization, and for that case we'd prefer the
	// straight-to-pcpu behaviour rather than falling back to the old allocator.
#if CONFIG_MAGAZINE_PER_CLUSTER && !(TARGET_OS_OSX || MALLOC_TARGET_DK_OSX)
	if (ncpuclusters == 1) {
#if CONFIG_FEATUREFLAGS_SIMPLE
		// Not with the other feature flag checks because ncpuclusters needs to
		// be initialized first
		bool secure_allocator_single_cluster = os_feature_enabled_simple(
				libmalloc, SecureAllocator_SingleCluster, false);
#else // CONFIG_FEATUREFLAGS_SIMPLE
		bool secure_allocator_single_cluster = false;
#endif // CONFIG_FEATUREFLAGS_SIMPLE
		if (malloc_xzone_enabled && !secure_allocator_single_cluster) {
			malloc_xzone_enabled = false;
		}
	}
#endif // CONFIG_MAGAZINE_PER_CLUSTER && !(TARGET_OS_OSX || MALLOC_TARGET_DK_OSX)

	_malloc_detect_interposition();


	set_flags_from_environment();


#if CONFIG_SANITIZER
	malloc_sanitizer_enabled = sanitizer_should_enable();
#endif
	
#if CONFIG_NANOZONE
	// TODO: envp should be passed down from Libsystem
	const char **envp = (const char **)*_NSGetEnviron();
	
	// Disable nano when:
	// - sanitizer is enabled, to avoid speculative out-of-bounds
	//   use-after-free reads that nano/nanov2 performs, OR
	// - MallocScribble is enabled, which nanov2 does not implement
	// - MallocCheckZeroOnFreeCorruption sampling is enabled, which nanov2 does
	//   not implement
	if (!malloc_sanitizer_enabled &&
			!(malloc_debug_flags & MALLOC_DO_SCRIBBLE) &&
			!malloc_zero_on_free_sample_period) {
		nano_common_init(envp, apple, bootargs);
	}
#endif // CONFIG_NANOZONE

	bool nano_on_xzone = false;


	if (!initial_xzone_zone || nano_on_xzone) {
		if (!initial_xzone_zone) {
			initial_scalable_zone = create_scalable_zone(0, malloc_debug_flags);
			malloc_set_zone_name(initial_scalable_zone, DEFAULT_MALLOC_ZONE_STRING);
			malloc_zone_register_while_locked(initial_scalable_zone, /*make_default=*/true);
		}

#if CONFIG_NANOZONE
		nano_common_configure();

		malloc_zone_t *helper_zone = initial_xzone_zone ?: initial_scalable_zone;

		if (_malloc_engaged_nano == NANO_V2) {
			if (malloc_report_config) {
				bool nano_on_xzone_enabled = (helper_zone == initial_xzone_zone);
				malloc_report(ASL_LEVEL_INFO, "NanoV2 Config:\n"
						"\tNano On Xzone: %d\n",
						nano_on_xzone_enabled);
			}
			initial_nano_zone = nanov2_create_zone(helper_zone, malloc_debug_flags);
		}

		if (initial_nano_zone) {
			malloc_set_zone_name(initial_nano_zone, DEFAULT_MALLOC_ZONE_STRING);
			malloc_set_zone_name(helper_zone, MALLOC_HELPER_ZONE_STRING);
			malloc_zone_register_while_locked(initial_nano_zone, /*make_default=*/true);
		}
#endif
	}

#if CONFIG_SANITIZER
	if (malloc_sanitizer_enabled) {
		malloc_zone_t *wrapped_zone = malloc_zones[0];
		malloc_zone_t *sanitizer_zone = sanitizer_create_zone(wrapped_zone);
		malloc_zone_register_while_locked(sanitizer_zone, /*make_default=*/true);
	}
#endif

	// Initialize slowpath check after we've registered zones so that
	// malloc_num_zones check is meaningful
	malloc_slowpath_update();

	initial_num_zones = malloc_num_zones;

#if CONFIG_DEFERRED_RECLAIM
	kern_return_t vmdr_kr = KERN_SUCCESS;
	if (large_cache_enabled) {
		if (initial_xzone_zone) {
			// xzone_malloc will own the deferred_reclaim buffer
			large_cache_enabled = false;
		} else {
			vmdr_kr = mvm_deferred_reclaim_init();
			if (vmdr_kr != KERN_SUCCESS) {
				large_cache_enabled = false;
				malloc_report(ASL_LEVEL_ERR, "Unable to set up reclaim buffer (%d) - disabling large cache\n", vmdr_kr);
			}
		}
	}
#endif /* CONFIG_DEFERRED_RECLAIM */

	if (malloc_report_config && initial_scalable_zone) {
		bool scribble = !!(malloc_debug_flags & MALLOC_DO_SCRIBBLE);
		malloc_report(ASL_LEVEL_INFO, "Magazine Config:\n"
				"\tMax Magazines: %d\n"
				"\tMedium Enabled: %d\n"
				"\tAggressive Madvise: %d\n"
#if CONFIG_DEFERRED_RECLAIM
				"\tLarge Cache: %d%s\n"
#endif // CONFIG_DEFERRED_RECLAIM
				"\tScribble: %d\n",
				max_magazines, magazine_medium_enabled,
				aggressive_madvise_enabled,
#if CONFIG_DEFERRED_RECLAIM
				vmdr_kr ?: large_cache_enabled, vmdr_kr ? " (ERROR)" : "",
#endif // CONFIG_DEFERRED_RECLAIM
				scribble);
	}

#if CONFIG_MEDIUM_ALLOCATOR
	uint64_t memsize = platform_hw_memsize();
	if (memsize >= 8 * magazine_medium_madvise_dram_scale_divisor) {
		magazine_medium_madvise_window_scale_factor = 8;
	} else if (memsize >= 4 * magazine_medium_madvise_dram_scale_divisor) {
		magazine_medium_madvise_window_scale_factor = 4;
	} else if (memsize >= 2 * magazine_medium_madvise_dram_scale_divisor) {
		magazine_medium_madvise_window_scale_factor = 2;
	} else {
		magazine_medium_madvise_window_scale_factor = 1;
	}
#endif /* CONFIG_MEDIUM_ALLOCATOR */

	// malloc_report(ASL_LEVEL_INFO, "%d registered zones\n", malloc_num_zones);
	// malloc_report(ASL_LEVEL_INFO, "malloc_zones is at %p; malloc_num_zones is at %p\n", (unsigned)&malloc_zones,
	// (unsigned)&malloc_num_zones);
}

static bool
enable_pgm(unsigned flags)
{
	bool other_debug_tool = has_injected_zone0 || malloc_sanitizer_enabled;
	// To avoid allocations in the lite helper zone that don't have msl data at
	// the end of the allocation, don't enable PGM on the lite helper zone
	bool zone_is_msl = flags & MALLOC_MSL_LITE_WRAPPED_ZONE_FLAGS;
	return !other_debug_tool && !zone_is_msl && pgm_should_enable();
}

static void
register_pgm_zone(bool internal_diagnostics)
{
	pgm_init_config(internal_diagnostics);
	if (enable_pgm(0)) {
		malloc_zone_t *wrapped_zone = malloc_zones[0];
		malloc_zone_t *pgm_zone = pgm_create_zone(wrapped_zone);
		malloc_zone_register_while_locked(pgm_zone, /*make_default=*/true);
	}
}

static inline malloc_zone_t *
inline_malloc_default_zone(void)
{
	// malloc_report(ASL_LEVEL_INFO, "In inline_malloc_default_zone with %d %d\n", malloc_num_zones, malloc_has_debug_zone);
	return malloc_zones[0];
}

malloc_zone_t *
malloc_default_zone(void)
{
	return default_zone;
}

static void *
legacy_zeroing_large_malloc(malloc_zone_t *zone, size_t size)
{
	if (size > LEGACY_ZEROING_THRESHOLD) {
		// Leopard and earlier returned a ZFOD range, so clear to zero always,
		// ham-handedly touching in each page
		return default_zone_calloc(zone, 1, size);
	} else {
		return default_zone_malloc(zone, size);
	}
}

static void *
legacy_zeroing_large_valloc(malloc_zone_t *zone, size_t size)
{
	void *p = default_zone_valloc(zone, size);

	// Leopard and earlier returned a ZFOD range, so ...
	memset(p, 0, size); // Clear to zero always, ham-handedly touching in each page
	return p;
}

void
zeroify_scalable_zone(malloc_zone_t *zone)
{
	// <rdar://problem/27190324> this checkfix should replace the default zone's
	// allocation routines with the zeroing versions. Instead of getting in hot 
	// water with the wrong zone, ensure that we're mutating the zone we expect.
	// 
	// Additionally, the default_zone is no longer PROT_READ, so the two mprotect
	// calls that were here are no longer needed.
	if (zone == default_zone) {
		zone->malloc = (void *)legacy_zeroing_large_malloc;
		zone->valloc = (void *)legacy_zeroing_large_valloc;
	}
}

/*
 * Returns the version of the Nano allocator that's in use, or 0 if not.
 */
int
malloc_engaged_nano(void)
{
#if CONFIG_NANOZONE
	return (initial_nano_zone || initial_xzone_zone) ? _malloc_engaged_nano : 0;
#else
	return 0;
#endif
}

int
malloc_engaged_secure_allocator(void)
{
	return !!initial_xzone_zone;
}

static void
set_flags_from_environment(void)
{
	const char *flag;
	const char **env = (const char **)*_NSGetEnviron();
	const char **p;
	const char *c;

#if defined(__LP64__)
	malloc_debug_flags = MALLOC_ABORT_ON_CORRUPTION; // Set always on 64-bit processes
#else
	int libSystemVersion = NSVersionOfLinkTimeLibrary("System");
	if ((-1 != libSystemVersion) && ((libSystemVersion >> 16) < 126) /* Lion or greater */) {
		malloc_debug_flags = 0;
	} else {
		malloc_debug_flags = MALLOC_ABORT_ON_CORRUPTION;
	}
#endif

#if TARGET_OS_OSX
	// rdar://99288027
	if (!dyld_program_sdk_at_least(dyld_platform_version_macOS_13_0)) {
		if (malloc_zero_policy == MALLOC_ZERO_ON_FREE) {
			malloc_zero_policy = MALLOC_ZERO_ON_ALLOC;
		}
	}
#else // TARGET_OS_OSX
#endif // TARGET_OS_OSX

	/*
	 * Given that all environment variables start with "Malloc" we optimize by scanning quickly
	 * first the environment, therefore avoiding repeated calls to getenv().
	 * If we are setu/gid these flags are ignored to prevent a malicious invoker from changing
	 * our behaviour.
	 */
	for (p = env; (c = *p) != NULL; ++p) {
#if RDAR_48993662
		if (!strncmp(c, "Malloc", 6) || !strncmp(c, "_Malloc", 6)) {
#else // RDAR_48993662
		if (!strncmp(c, "Malloc", 6)) {
#endif // RDAR_48993662
			if (issetugid()) {
				return;
			}
			break;
		}
	}

	/*
	 * Deny certain flags for entitled processes rdar://problem/13521742
	 * MallocLogFile & MallocCorruptionAbort
	 * as these provide the ability to turn *off* aborting in error cases.
	 */
	bool restricted = dyld_process_is_restricted();
	malloc_print_configure(restricted);

	if (c == NULL) {
		return;
	}

	flag = getenv("MallocGuardEdges");
	if (flag) {
		if (!strcmp(flag, "all")) {
			// "MallocGuardEdges=all" adds guard page(s) for every region.
			// Do not do this on 32-bit platforms because there is insufficient
			// address space. These pages are always protected.
#if MALLOC_TARGET_64BIT
			malloc_debug_flags |= MALLOC_GUARD_ALL | MALLOC_ADD_GUARD_PAGE_FLAGS;
			malloc_debug_flags &= ~(MALLOC_DONT_PROTECT_PRELUDE|MALLOC_DONT_PROTECT_POSTLUDE);
			malloc_report(ASL_LEVEL_INFO, "adding guard pages to all regions\n");
#endif // MALLOC_TARGET_64BIT
		} else {
			malloc_debug_flags |= MALLOC_ADD_GUARD_PAGE_FLAGS;
			malloc_debug_flags &= ~MALLOC_GUARD_ALL;
			malloc_report(ASL_LEVEL_INFO, "adding guard pages for large allocator blocks\n");
			if (getenv("MallocDoNotProtectPrelude")) {
				malloc_debug_flags |= MALLOC_DONT_PROTECT_PRELUDE;
				malloc_report(ASL_LEVEL_INFO, "... but not protecting prelude guard page\n");
			}
			if (getenv("MallocDoNotProtectPostlude")) {
				malloc_debug_flags |= MALLOC_DONT_PROTECT_POSTLUDE;
				malloc_report(ASL_LEVEL_INFO, "... but not protecting postlude guard page\n");
			}
		}
	}

	if (getenv("MallocScribble")) {
		malloc_debug_flags |= MALLOC_DO_SCRIBBLE;
		malloc_report(ASL_LEVEL_INFO, "enabling scribbling to detect mods to free blocks\n");
	}
	if (getenv("MallocErrorAbort")) {
		malloc_debug_flags |= MALLOC_ABORT_ON_ERROR;
		malloc_report(ASL_LEVEL_INFO, "enabling abort() on bad malloc or free\n");
	}
	if (getenv("MallocTracing")) {
		malloc_tracing_enabled = true;
	}
	if (getenv("MallocSimpleStackLogging")) {
		malloc_simple_stack_logging = true;
	}
	if (getenv("MallocReportConfig")) {
		malloc_report_config = true;
	}

#if defined(__LP64__)
/* initialization above forces MALLOC_ABORT_ON_CORRUPTION of 64-bit processes */
#else
	flag = getenv("MallocCorruptionAbort");
	if (!restricted && flag && (flag[0] == '0')) { // Set from an environment variable in 32-bit processes
		malloc_debug_flags &= ~MALLOC_ABORT_ON_CORRUPTION;
	} else if (flag) {
		malloc_debug_flags |= MALLOC_ABORT_ON_CORRUPTION;
	}
#endif
	flag = getenv("MallocCheckHeapStart");
	if (flag) {
		malloc_check_start = (unsigned)strtoul(flag, NULL, 0);
		if (malloc_check_start == 0) {
			malloc_check_start = 1;
		}
		if (malloc_check_start == -1) {
			malloc_check_start = 1;
		}
		flag = getenv("MallocCheckHeapEach");
		if (flag) {
			malloc_check_each = (unsigned)strtoul(flag, NULL, 0);
			if (malloc_check_each == 0) {
				malloc_check_each = 1;
			}
			if (malloc_check_each == -1) {
				malloc_check_each = 1;
			}
		}
		malloc_report(ASL_LEVEL_INFO, "checks heap after operation #%d and each %d operations\n", malloc_check_start, malloc_check_each);
		flag = getenv("MallocCheckHeapAbort");
		if (flag) {
			malloc_check_abort = (unsigned)strtol(flag, NULL, 0);
		}
		if (malloc_check_abort) {
			malloc_report(ASL_LEVEL_INFO, "will abort on heap corruption\n");
		} else {
			flag = getenv("MallocCheckHeapSleep");
			if (flag) {
				malloc_check_sleep = (unsigned)strtol(flag, NULL, 0);
			}
			if (malloc_check_sleep > 0) {
				malloc_report(ASL_LEVEL_INFO, "will sleep for %d seconds on heap corruption\n", malloc_check_sleep);
			} else if (malloc_check_sleep < 0) {
				malloc_report(ASL_LEVEL_INFO, "will sleep once for %d seconds on heap corruption\n", -malloc_check_sleep);
			} else {
				malloc_report(ASL_LEVEL_INFO, "no sleep on heap corruption\n");
			}
		}
	}

	flag = getenv("MallocMaxMagazines");
#if RDAR_48993662
	if (!flag) {
		flag = getenv("_MallocMaxMagazines");
	}
#endif // RDAR_48993662
	if (flag) {

		int value = (int)strtol(flag, NULL, 0);
		if (value == 0) {
			malloc_report(ASL_LEVEL_INFO, "Maximum magazines defaulted to %d\n", max_magazines);
#if CONFIG_MAGAZINE_PER_CLUSTER
		} else if (value == UINT16_MAX) {
			{
				max_magazines = ncpuclusters;
				malloc_report(ASL_LEVEL_INFO,
						"Maximum magazines limited to ncpuclusters (%d)\n",
						max_magazines);
			}
#endif // CONFIG_MAGAZINE_PER_CLUSTER
		} else if (value < 0) {
			malloc_report(ASL_LEVEL_ERR, "Maximum magazines must be positive - ignored.\n");
		} else if (value > logical_ncpus) {
			max_magazines = logical_ncpus;
			malloc_report(ASL_LEVEL_INFO, "Maximum magazines limited to number of logical CPUs (%d)\n", max_magazines);
		} else {
			max_magazines = value;
			malloc_report(ASL_LEVEL_INFO, "Maximum magazines set to %d\n", max_magazines);
		}
	}

	flag = getenv("MallocLargeExpandedCacheThreshold");
	if (flag) {
		uint64_t value = (uint64_t)strtoull(flag, NULL, 0);
		if (value == 0) {
			malloc_report(ASL_LEVEL_INFO, "Large expanded cache threshold defaulted to %lly\n", magazine_large_expanded_cache_threshold);
		} else if (value < 0) {
			malloc_report(ASL_LEVEL_ERR, "MallocLargeExpandedCacheThreshold must be positive - ignored.\n");
		} else {
			magazine_large_expanded_cache_threshold = value;
			malloc_report(ASL_LEVEL_INFO, "Large expanded cache threshold set to %lly\n", magazine_large_expanded_cache_threshold);
		}
	}

	flag = getenv("MallocLargeDisableASLR");
	if (flag) {
		uint64_t value = (uint64_t)strtoull(flag, NULL, 0);
		if (value == 0) {
			malloc_report(ASL_LEVEL_INFO, "Enabling ASLR slide on large allocations\n");
			malloc_debug_flags &= ~DISABLE_LARGE_ASLR;
		} else if (value != 0) {
			malloc_report(ASL_LEVEL_INFO, "Disabling ASLR slide on large allocations\n");
			malloc_debug_flags |= DISABLE_LARGE_ASLR;
		}
	}

#if CONFIG_AGGRESSIVE_MADVISE || CONFIG_LARGE_CACHE
	// convenience flag to configure policies usually associated with memory-constrained platforms (iOS)
	// that trade some amount of time efficiency for space efficiency
	flag = getenv("MallocSpaceEfficient");
	if (flag) {
		const char *endp;
		long value = malloc_common_convert_to_long(flag, &endp);
		if (!*endp && endp != flag && (value == 0 || value == 1)) {
#if CONFIG_AGGRESSIVE_MADVISE
			aggressive_madvise_enabled = (value == 1);
#endif // CONFIG_AGGRESSIVE_MADVISE
#if CONFIG_LARGE_CACHE
			// Disable the large cache in space efficient mode
			if (value != 0){
				large_cache_enabled = 0;
			}
#endif // CONFIG_LARGE_CACHE
			malloc_space_efficient_enabled = (value == 1);
			// consider disabling medium magazine if aggressive madvise is not sufficient
		} else {
			malloc_report(ASL_LEVEL_ERR, "MallocSpaceEfficient must be 0 or 1.\n");
		}
	}
#endif // CONFIG_AGGRESSIVE_MADVISE || CONFIG_LARGE_CACHE

#if CONFIG_MEDIUM_ALLOCATOR
	flag = getenv("MallocMediumZone");
	if (flag) {
		int value = (unsigned)strtol(flag, NULL, 0);
		if (value == 0) {
			magazine_medium_enabled = false;
		} else if (value == 1) {
			magazine_medium_enabled = true;
		}
	}

	flag = getenv("MallocMediumActivationThreshold");
	if (flag) {
		uint64_t value = (uint64_t)strtoull(flag, NULL, 0);
		if (value == 0) {
			malloc_report(ASL_LEVEL_INFO, "Medium activation threshold defaulted to %lly\n", magazine_medium_active_threshold);
		} else if (value < 0) {
			malloc_report(ASL_LEVEL_ERR, "MallocMediumActivationThreshold must be positive - ignored.\n");
		} else {
			magazine_medium_active_threshold = value;
			malloc_report(ASL_LEVEL_INFO, "Medium activation threshold set to %lly\n", magazine_medium_active_threshold);
		}
	}

	flag = getenv("MallocMediumSpaceEfficient");
	if (flag) {
		uint64_t value = (uint64_t)strtoull(flag, NULL, 0);
		if (value == 0) {
			malloc_medium_space_efficient_enabled = false;
		} else if (value == 1) {
			malloc_medium_space_efficient_enabled = true;
		}
	}

	if (malloc_medium_space_efficient_enabled && malloc_space_efficient_enabled) {
		// Bring down MallocMaxMediumMagazines to only a single magazine in
		// space-efficent processes but do this before the envvar so that it
		// can still be overridden at the command line.
		max_medium_magazines = 1;
	}

	flag = getenv("MallocMaxMediumMagazines");
#if RDAR_48993662
	if (!flag) {
		flag = getenv("_MallocMaxMediumMagazines");
	}
#endif // RDAR_48993662
	if (flag) {
		int value = (unsigned)strtol(flag, NULL, 0);
		if (value == 0) {
			malloc_report(ASL_LEVEL_INFO, "Maximum medium magazines defaulted to %d\n", max_magazines);
		} else if (value < 0) {
			malloc_report(ASL_LEVEL_ERR, "Maximum medium magazines must be positive - ignored.\n");
		} else if (value > logical_ncpus) {
			max_medium_magazines = logical_ncpus;
			malloc_report(ASL_LEVEL_INFO, "Maximum medium magazines limited to number of logical CPUs (%d)\n", max_medium_magazines);
		} else {
			max_medium_magazines = value;
			malloc_report(ASL_LEVEL_INFO, "Maximum medium magazines set to %d\n", max_medium_magazines);
		}
	}
#endif // CONFIG_MEDIUM_ALLOCATOR

#if CONFIG_AGGRESSIVE_MADVISE
	flag = getenv("MallocAggressiveMadvise");
	if (flag) {
		const char *endp;
		long value = malloc_common_convert_to_long(flag, &endp);
		if (!*endp && endp != flag && (value == 0 || value == 1)) {
			aggressive_madvise_enabled = (value == 1);
		} else {
			malloc_report(ASL_LEVEL_ERR, "MallocAggressiveMadvise must be 0 or 1.\n");
		}
	}
#endif // CONFIG_AGGRESSIVE_MADVISE

#if CONFIG_LARGE_CACHE
	flag = getenv("MallocLargeCache");
	if (flag) {
		const char *endp;
		long value = malloc_common_convert_to_long(flag, &endp);
		if (!*endp && endp != flag && (value == 0 || value == 1)) {
			large_cache_enabled = (value == 1);
		} else {
			malloc_report(ASL_LEVEL_ERR, "MallocLargeCache must be 0 or 1.\n");
		}
	}
#endif // CONFIG_LARGE_CACHE

#if CONFIG_RECIRC_DEPOT
	flag = getenv("MallocRecircRetainedRegions");
	if (flag) {
		int value = (int)strtol(flag, NULL, 0);
		if (value > 0) {
			recirc_retained_regions = value;
		} else {
			malloc_report(ASL_LEVEL_ERR, "MallocRecircRetainedRegions must be positive - ignored.\n");
		}
	}
#endif // CONFIG_RECIRC_DEPOT

	flag = getenv("MallocZeroOnFree");
	if (flag) {
		const char *endp;
		long value = malloc_common_convert_to_long(flag, &endp);
		if (!*endp && endp != flag && (value == 0 || value == 1)) {
			malloc_zero_policy = value ? MALLOC_ZERO_ON_FREE : MALLOC_ZERO_NONE;
		} else {
			malloc_report(ASL_LEVEL_ERR, "MallocZeroOnFree must be 0 or 1.\n");
		}
	}

	flag = getenv("MallocZeroOnAlloc");
	if (flag) {
		const char *endp;
		long value = malloc_common_convert_to_long(flag, &endp);
		if (!*endp && endp != flag && (value == 0 || value == 1)) {
			malloc_zero_policy = value ? MALLOC_ZERO_ON_ALLOC : MALLOC_ZERO_NONE;
		} else {
			malloc_report(ASL_LEVEL_ERR, "MallocZeroOnAlloc must be 0 or 1.\n");
		}
	}

	flag = getenv("MallocCheckZeroOnFreeCorruption");
	if (flag) {
		int value = (int)strtol(flag, NULL, 0);
		if (value > 0) {
			malloc_zero_on_free_sample_period = value;
		} else {
			malloc_report(ASL_LEVEL_ERR,
					"malloc_zero_on_free_sample_period must be positive - ignored.\n");
		}
	}

	flag = getenv("MallocSecureAllocator");
	if (flag && malloc_internal_security_policy) {
		const char *endp;
		long value = malloc_common_convert_to_long(flag, &endp);
		if (!*endp && endp != flag && (value == 0 || value == 1)) {
			malloc_xzone_enabled = value;
			malloc_xzone_enabled_override = value ?
					MALLOC_XZONE_OVERRIDE_ENABLED :
					MALLOC_XZONE_OVERRIDE_DISABLED;
		} else {
			malloc_report(ASL_LEVEL_ERR, "MallocSecureAllocator must be 0 or 1.\n");
		}
	}

	flag = getenv("MallocSecureAllocatorNano");
	if (flag) {
		const char *endp;
		long value = malloc_common_convert_to_long(flag, &endp);
		if (!*endp && endp != flag && (value == 0 || value == 1)) {
			malloc_xzone_nano_override = value ? MALLOC_XZONE_OVERRIDE_ENABLED :
					MALLOC_XZONE_OVERRIDE_DISABLED;
		} else {
			malloc_report(ASL_LEVEL_ERR, "MallocSecureAllocatorNano must be 0 or 1.\n");
		}
	}

	flag = getenv("MallocNanoOnXzone");
	if (flag) {
		const char *endp;
		long value = malloc_common_convert_to_long(flag, &endp);
		if (!*endp && endp != flag && (value == 0 || value == 1)) {
			malloc_nano_on_xzone_override = value ? MALLOC_XZONE_OVERRIDE_ENABLED :
					MALLOC_XZONE_OVERRIDE_DISABLED;
		} else {
			malloc_report(ASL_LEVEL_ERR, "MallocNanoOnXzone must be 0 or 1.\n");
		}
	}



	if (getenv("MallocHelp")) {
		malloc_report(ASL_LEVEL_INFO,
				"environment variables that can be set for debug:\n"
				"- MallocLogFile <f> to create/append messages to file <f> instead of stderr\n"
				"- MallocGuardEdges to add 2 guard pages for each large block\n"
				"- MallocDoNotProtectPrelude to disable protection (when previous flag set)\n"
				"- MallocDoNotProtectPostlude to disable protection (when previous flag set)\n"
				"- MallocStackLogging to record all stacks.  Tools like leaks can then be applied\n"
				"- MallocStackLoggingNoCompact to record all stacks.  Needed for malloc_history\n"
				"- MallocStackLoggingDirectory to set location of stack logs, which can grow large; default is /tmp\n"
				"- MallocScribble to detect writing on free blocks and missing initializers:\n"
				"  0x55 is written upon free and 0xaa is written on allocation\n"
				"- MallocCheckHeapStart <n> to start checking the heap after <n> operations\n"
				"- MallocCheckHeapEach <s> to repeat the checking of the heap after <s> operations\n"
				"- MallocCheckHeapSleep <t> to sleep <t> seconds on heap corruption\n"
				"- MallocCheckHeapAbort <b> to abort on heap corruption if <b> is non-zero\n"
				"- MallocCorruptionAbort to abort on malloc errors, but not on out of memory for 32-bit processes\n"
				"  MallocCorruptionAbort is always set on 64-bit processes\n"
				"- MallocErrorAbort to abort on any malloc error, including out of memory\n"\
				"- MallocTracing to emit kdebug trace points on malloc entry points\n"\
				"- MallocZeroOnFree to enable or disable zero-on-free behavior (for debugging only)\n"\
				"- MallocCheckZeroOnFreeCorruption to enable zero-on-free corruption detection\n"\
				"- MallocHelp - this help!\n");
	}
}


malloc_zone_t *
malloc_create_zone(vm_size_t start_size, unsigned flags)
{
	malloc_zone_t *zone = NULL;

	/* start_size doesn't actually appear to be used, but we test anyway. */
	if (start_size > malloc_absolute_max_size) {
		return NULL;
	}


	if (!zone) {
		zone = create_scalable_zone(start_size, flags | malloc_debug_flags);
	}

	if (enable_pgm(flags)) {
		malloc_zone_t *pgm_zone = pgm_create_zone(zone);
		MALLOC_LOCK();
		malloc_zone_register_while_locked(pgm_zone, false);
		malloc_zone_register_while_locked(zone, false);
		MALLOC_UNLOCK();
		return pgm_zone;
	}

	malloc_zone_register(zone);
	return zone;
}

/*
 * For use by CheckFix: establish a new default zone whose behavior is, apart from
 * the use of death-row and per-CPU magazines, that of Leopard.
 */
void
malloc_create_legacy_default_zone(void)
{
	malloc_zone_t *zone;

	zone = create_legacy_scalable_zone(0, malloc_debug_flags);

	MALLOC_LOCK();

	//
	// Establish the legacy scalable zone just created as the default zone.
	//
	malloc_zone_t *hold = malloc_zones[0];
	if (hold->zone_name && strcmp(hold->zone_name, DEFAULT_MALLOC_ZONE_STRING) == 0) {
		malloc_set_zone_name(hold, NULL);
	}
	malloc_set_zone_name(zone, DEFAULT_MALLOC_ZONE_STRING);

	malloc_zone_register_while_locked(zone, /*make_default=*/true);
	MALLOC_UNLOCK();
}

void
malloc_destroy_zone(malloc_zone_t *zone)
{
	malloc_set_zone_name(zone, NULL); // Deallocate zone name wherever it may reside PR_7701095
	malloc_zone_unregister(zone);
	zone->destroy(zone);
}

static vm_address_t *frames = NULL;
static unsigned num_frames;

MALLOC_NOINLINE
void
malloc_zone_check_fail(const char *msg, const char *fmt, ...)
{
	_SIMPLE_STRING b = _simple_salloc();
	if (b) {
		_simple_sprintf(b, "*** MallocCheckHeap: FAILED check at operation #%d\n", malloc_check_counter - 1);
	} else {
		malloc_report(MALLOC_REPORT_NOLOG, "*** MallocCheckHeap: FAILED check at operation #%d\n", malloc_check_counter - 1);
	}
	if (frames) {
		unsigned index = 1;
		if (b) {
			_simple_sappend(b, "Stack for last operation where the malloc check succeeded: ");
			while (index < num_frames)
				_simple_sprintf(b, "%p ", (void*)frames[index++]);
			malloc_report(MALLOC_REPORT_NOLOG, "%s\n(Use 'atos' for a symbolic stack)\n", _simple_string(b));
		} else {
			/*
			 * Should only get here if vm_allocate() can't get a single page of
			 * memory, implying _simple_asl_log() would also fail.  So we just
			 * print to the file descriptor.
			 */
			malloc_report(MALLOC_REPORT_NOLOG, "Stack for last operation where the malloc check succeeded: ");
			while (index < num_frames) {
				malloc_report(MALLOC_REPORT_NOLOG, "%p ", (void *)frames[index++]);
			}
			malloc_report(MALLOC_REPORT_NOLOG, "\n(Use 'atos' for a symbolic stack)\n");
		}
	}
	if (malloc_check_each > 1) {
		unsigned recomm_each = (malloc_check_each > 10) ? malloc_check_each / 10 : 1;
		unsigned recomm_start =
				(malloc_check_counter > malloc_check_each + 1) ? malloc_check_counter - 1 - malloc_check_each : 1;
		malloc_report(MALLOC_REPORT_NOLOG,
				"*** Recommend using 'setenv MallocCheckHeapStart %d; setenv MallocCheckHeapEach %d' to narrow down failure\n",
				recomm_start, recomm_each);
	}

	if (b) {
		_simple_sfree(b);
	}

	// Use malloc_vreport() to:
	// 	* report the error
	// 	* call malloc_error_break() for a breakpoint
	// 	* sleep or stop for debug
	// 	* set the crash message and crash if malloc_check_abort is set.
	unsigned sleep_time = 0;
	uint32_t report_flags = ASL_LEVEL_ERR | MALLOC_REPORT_DEBUG | MALLOC_REPORT_NOLOG;
	if (malloc_check_abort) {
		report_flags |= MALLOC_REPORT_CRASH;
	} else {
		if (malloc_check_sleep > 0) {
			malloc_report(ASL_LEVEL_NOTICE, "*** Will sleep for %d seconds to leave time to attach\n", malloc_check_sleep);
			sleep_time = malloc_check_sleep;
		} else if (malloc_check_sleep < 0) {
			malloc_report(ASL_LEVEL_NOTICE, "*** Will sleep once for %d seconds to leave time to attach\n", -malloc_check_sleep);
			sleep_time = -malloc_check_sleep;
			malloc_check_sleep = 0;
		}
	}
	va_list ap;
	va_start(ap, fmt);
	malloc_vreport(report_flags, sleep_time, msg, NULL, fmt, ap);
	va_end(ap);
}

/*********	Block creation and manipulation	************/

__attribute__((cold, noinline))
static void
internal_check(void)
{
	if (malloc_check_counter++ < malloc_check_start) {
		return;
	}
	if (malloc_zone_check(NULL)) {
		if (!frames) {
			vm_allocate(mach_task_self(), (void *)&frames, vm_page_size, 1);
		}
		thread_stack_pcs(frames, (unsigned)(vm_page_size / sizeof(vm_address_t) - 1), &num_frames);
	}
	malloc_check_start += malloc_check_each;
}

MALLOC_NOINLINE
static void *
_malloc_zone_malloc_instrumented_or_legacy(malloc_zone_t *zone, size_t size,
		malloc_zone_options_t mzo)
{
	uint64_t type_id = malloc_get_tsd_type_id();
#if MALLOC_TARGET_64BIT
	bool clear_type = false;
	if (!type_id) {
		malloc_type_descriptor_t fallback =
				malloc_callsite_fallback_type_descriptor();
		malloc_set_tsd_type_descriptor(fallback);
		type_id = fallback.type_id;
		clear_type = true;
	}
#endif // MALLOC_TARGET_64BIT
	MALLOC_TRACE(TRACE_malloc | DBG_FUNC_START, (uintptr_t)zone, size, type_id,
			0);

	void *ptr = NULL;

	if (malloc_check_start) {
		internal_check();
	}
	if (size > malloc_absolute_max_size) {
		goto out;
	}

	ptr = zone->malloc(zone, size);

	if (os_unlikely(malloc_logger)) {
		malloc_logger(MALLOC_LOG_TYPE_ALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE,
				(uintptr_t)zone, (uintptr_t)size, 0, (uintptr_t)ptr, 0);
	}

	if (os_unlikely(malloc_simple_stack_logging)) {
		malloc_report(MALLOC_SIMPLE_STACK_LOGGING_FLAGS, "malloc (%p/%llu): ",
				ptr, (unsigned long long)size);
	}

	MALLOC_TRACE(TRACE_malloc | DBG_FUNC_END, (uintptr_t)zone, size,
			(uintptr_t)ptr, type_id);
out:
#if MALLOC_TARGET_64BIT
	if (clear_type) {
		malloc_set_tsd_type_descriptor(MALLOC_TYPE_DESCRIPTOR_NONE);
	}
#endif // MALLOC_TARGET_64BIT
	if (os_unlikely(ptr == NULL)) {
		malloc_set_errno_fast(mzo, ENOMEM);
	}
	return ptr;
}

void *
_malloc_zone_malloc(malloc_zone_t *zone, size_t size, malloc_zone_options_t mzo)
{
	if (zone == default_zone && !lite_zone) {
		// Eagerly resolve the virtual default zone to make the zone version
		// check accurate
		zone = malloc_zones[0];
	}

	if (os_unlikely(malloc_slowpath || malloc_logger || zone->version < 13)) {
		return _malloc_zone_malloc_instrumented_or_legacy(zone, size, mzo);
	}

	if (os_unlikely(size > malloc_absolute_max_size)) {
		malloc_set_errno_fast(mzo, ENOMEM);
		return NULL;
	}

	if (zone->version >= 16) {
		return zone->malloc_type_malloc(zone, size,
				malloc_callsite_fallback_type_id());
	}

	// zone versions >= 13 set errno on failure so we can tail-call
	return zone->malloc(zone, size);
}

MALLOC_NOINLINE
void *
malloc_zone_malloc(malloc_zone_t *zone, size_t size)
{
	return _malloc_zone_malloc(zone, size, MZ_NONE);
}

MALLOC_NOINLINE
static void *
_malloc_zone_calloc_instrumented_or_legacy(malloc_zone_t *zone,
		size_t num_items, size_t size, malloc_zone_options_t mzo)
{
	uint64_t type_id = malloc_get_tsd_type_id();
#if MALLOC_TARGET_64BIT
	bool clear_type = false;
	if (!type_id) {
		malloc_type_descriptor_t fallback =
				malloc_callsite_fallback_type_descriptor();
		malloc_set_tsd_type_descriptor(fallback);
		type_id = fallback.type_id;
		clear_type = true;
	}
#endif // MALLOC_TARGET_64BIT
	MALLOC_TRACE(TRACE_calloc | DBG_FUNC_START, (uintptr_t)zone, num_items,
			size, type_id);

	void *ptr;
	if (malloc_check_start) {
		internal_check();
	}

	ptr = zone->calloc(zone, num_items, size);

	if (os_unlikely(malloc_logger)) {
		uint32_t logger_type = MALLOC_LOG_TYPE_ALLOCATE |
				MALLOC_LOG_TYPE_HAS_ZONE | MALLOC_LOG_TYPE_CLEARED;
		malloc_logger(logger_type, (uintptr_t)zone,
				(uintptr_t)(num_items * size), 0, (uintptr_t)ptr, 0);
	}

	if (os_unlikely(malloc_simple_stack_logging)) {
		malloc_report(MALLOC_SIMPLE_STACK_LOGGING_FLAGS,
				"calloc (%p/%llu*%llu): ", ptr, (unsigned long long)num_items,
				(unsigned long long)size);
	}

	MALLOC_TRACE(TRACE_calloc | DBG_FUNC_END, (uintptr_t)zone, num_items, size,
			(uintptr_t)ptr);
#if MALLOC_TARGET_64BIT
	if (clear_type) {
		malloc_set_tsd_type_descriptor(MALLOC_TYPE_DESCRIPTOR_NONE);
	}
#endif // MALLOC_TARGET_64BIT
	if (os_unlikely(ptr == NULL)) {
		malloc_set_errno_fast(mzo, ENOMEM);
	}
	return ptr;
}

MALLOC_NOINLINE
void *
_malloc_zone_calloc(malloc_zone_t *zone, size_t num_items, size_t size,
		malloc_zone_options_t mzo)
{
	if (zone == default_zone && !lite_zone) {
		// Eagerly resolve the virtual default zone to make the zone version
		// check accurate
		zone = malloc_zones[0];
	}

	if (os_unlikely(malloc_slowpath || malloc_logger || zone->version < 13)) {
		return _malloc_zone_calloc_instrumented_or_legacy(zone, num_items, size, mzo);
	}

	if (zone->version >= 16) {
		return zone->malloc_type_calloc(zone, num_items, size,
				malloc_callsite_fallback_type_id());
	}

	// zone versions >= 13 set errno on failure so we can tail-call
	return zone->calloc(zone, num_items, size);
}

MALLOC_NOINLINE
void *
malloc_zone_calloc(malloc_zone_t *zone, size_t num_items, size_t size)
{
	return _malloc_zone_calloc(zone, num_items, size, MZ_NONE);
}

MALLOC_NOINLINE
void *
_malloc_zone_valloc(malloc_zone_t *zone, size_t size, malloc_zone_options_t mzo)
{
	uint64_t type_id = malloc_get_tsd_type_id();
#if MALLOC_TARGET_64BIT
	bool clear_type = false;
	if (!type_id) {
		malloc_type_descriptor_t fallback =
				malloc_callsite_fallback_type_descriptor();
		malloc_set_tsd_type_descriptor(fallback);
		type_id = fallback.type_id;
		clear_type = true;
	}
#endif // MALLOC_TARGET_64BIT
	MALLOC_TRACE(TRACE_valloc | DBG_FUNC_START, (uintptr_t)zone, size, type_id,
			0);

	void *ptr = NULL;
	if (malloc_check_start) {
		internal_check();
	}
	if (size > malloc_absolute_max_size) {
		goto out;
	}

	ptr = zone->valloc(zone, size);

	if (os_unlikely(malloc_logger)) {
		malloc_logger(MALLOC_LOG_TYPE_ALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE,
				(uintptr_t)zone, (uintptr_t)size, 0, (uintptr_t)ptr, 0);
	}

	if (os_unlikely(malloc_simple_stack_logging)) {
		malloc_report(MALLOC_SIMPLE_STACK_LOGGING_FLAGS, "valloc (%p/%llu): ",
				ptr, (unsigned long long)size);
	}

	MALLOC_TRACE(TRACE_valloc | DBG_FUNC_END, (uintptr_t)zone, size,
			(uintptr_t)ptr, type_id);
out:
#if MALLOC_TARGET_64BIT
	if (clear_type) {
		malloc_set_tsd_type_descriptor(MALLOC_TYPE_DESCRIPTOR_NONE);
	}
#endif // MALLOC_TARGET_64BIT

	if (os_unlikely(ptr == NULL)) {
		malloc_set_errno_fast(mzo, ENOMEM);
	}
	return ptr;
}

MALLOC_NOINLINE
void *
malloc_zone_valloc(malloc_zone_t *zone, size_t size)
{
	return _malloc_zone_valloc(zone, size, MZ_NONE);
}

// We have this function so code within libmalloc can call it without going
// through the (potentially interposed) dyld symbol stub
void *
_malloc_zone_realloc(malloc_zone_t *zone, void *ptr, size_t size,
		malloc_type_descriptor_t type_desc)
{
	uint64_t type_id = malloc_get_tsd_type_id();
#if MALLOC_TARGET_64BIT
	bool clear_type = false;
	if (!type_id) {
		// A type descriptor in the TSD takes precendence over one passed as a
		// parameter - the one in the TSD will be a real one set by e.g.
		// _malloc_type_realloc_outlined(), whereas the parameter will be a
		// callsite-derived fallback
		malloc_set_tsd_type_descriptor(type_desc);
		type_id = type_desc.type_id;
		clear_type = true;
	}
#endif // MALLOC_TARGET_64BIT
	MALLOC_TRACE(TRACE_realloc | DBG_FUNC_START, (uintptr_t)zone,
			(uintptr_t)ptr, size, type_id);

	void *new_ptr = NULL;
	if (malloc_check_start) {
		internal_check();
	}
	if (size > malloc_absolute_max_size) {
		goto out;
	}

	new_ptr = zone->realloc(zone, ptr, size);
	
	if (os_unlikely(malloc_logger)) {
		uint32_t logger_type = MALLOC_LOG_TYPE_ALLOCATE |
			MALLOC_LOG_TYPE_DEALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE;
		malloc_logger(logger_type, (uintptr_t)zone, (uintptr_t)ptr,
				(uintptr_t)size, (uintptr_t)new_ptr, 0);
	}

	if (os_unlikely(malloc_simple_stack_logging)) {
		malloc_report(MALLOC_SIMPLE_STACK_LOGGING_FLAGS,
				"realloc (%p->%p/%llu): ", ptr, new_ptr,
				(unsigned long long)size);
	}

	MALLOC_TRACE(TRACE_realloc | DBG_FUNC_END, (uintptr_t)zone, (uintptr_t)ptr,
			size, (uintptr_t)new_ptr);
out:
#if MALLOC_TARGET_64BIT
	if (clear_type) {
		malloc_set_tsd_type_descriptor(MALLOC_TYPE_DESCRIPTOR_NONE);
	}
#endif // MALLOC_TARGET_64BIT
	return new_ptr;
}

MALLOC_NOINLINE
void *
malloc_zone_realloc(malloc_zone_t *zone, void *ptr, size_t size)
{
	return _malloc_zone_realloc(zone, ptr, size,
			malloc_callsite_fallback_type_descriptor());
}

MALLOC_NOINLINE
void
malloc_zone_free(malloc_zone_t *zone, void *ptr)
{
	MALLOC_TRACE(TRACE_free, (uintptr_t)zone, (uintptr_t)ptr, (ptr) ? *(uintptr_t*)ptr : 0, 0);

	if (os_unlikely(malloc_logger)) {
		malloc_logger(MALLOC_LOG_TYPE_DEALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE, (uintptr_t)zone, (uintptr_t)ptr, 0, 0, 0);
	}
	if (os_unlikely(malloc_simple_stack_logging)) {
		malloc_report(MALLOC_SIMPLE_STACK_LOGGING_FLAGS, "malloc_zone_free (%p): ", ptr);
	}
	if (malloc_check_start) {
		internal_check();
	}

	zone->free(zone, ptr);
}

static void
malloc_zone_free_definite_size(malloc_zone_t *zone, void *ptr, size_t size)
{
	MALLOC_TRACE(TRACE_free, (uintptr_t)zone, (uintptr_t)ptr, size, (ptr && size) ? *(uintptr_t*)ptr : 0);

	if (os_unlikely(malloc_logger)) {
		malloc_logger(MALLOC_LOG_TYPE_DEALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE, (uintptr_t)zone, (uintptr_t)ptr, 0, 0, 0);
	}
	if (os_unlikely(malloc_simple_stack_logging)) {
		malloc_report(MALLOC_SIMPLE_STACK_LOGGING_FLAGS, "free (%p/%llu): ",
				ptr, (unsigned long long)size);
	}
	if (malloc_check_start) {
		internal_check();
	}

	zone->free_definite_size(zone, ptr, size);
}

malloc_zone_t *
malloc_zone_from_ptr(const void *ptr)
{
	if (!ptr) {
		return NULL;
	} else {
		return _find_registered_zone(ptr, NULL, false);
	}
}

MALLOC_NOINLINE
void *
_malloc_zone_memalign(malloc_zone_t *zone, size_t alignment, size_t size,
		malloc_zone_options_t mzo, malloc_type_descriptor_t type_desc)
{
	uint64_t type_id = malloc_get_tsd_type_id();
#if MALLOC_TARGET_64BIT
	bool clear_type = false;
	if (!type_id) {
		// A type descriptor in the TSD takes precendence over one passed as a
		// parameter - the one in the TSD will be a real one set by e.g.
		// _malloc_type_aligned_alloc_outlined(), whereas the parameter will
		// usually be a callsite-derived fallback
		malloc_set_tsd_type_descriptor(type_desc);
		type_id = type_desc.type_id;
		clear_type = true;
	}
#endif // MALLOC_TARGET_64BIT
	MALLOC_TRACE(TRACE_memalign | DBG_FUNC_START, (uintptr_t)zone, alignment,
			size, type_id);

	void *ptr = NULL;
	int err = ENOMEM;
	// Version must be >= 5 to look at the new memalign field.
	if (zone->version < 5) {
		goto out;
	}
	if (malloc_check_start) {
		internal_check();
	}
	if (size > malloc_absolute_max_size) {
		goto out;
	}
	// excludes 0 == alignment
	// relies on sizeof(void *) being a power of two.
	if (alignment < sizeof(void *) ||
			0 != (alignment & (alignment - 1))) {
		err = EINVAL;
		goto out;
	}
	// C11 aligned_alloc requires size to be a multiple of alignment, but
	// posix_memalign does not
	if ((mzo & MZ_C11) && (size & (alignment - 1)) != 0) {
		err = EINVAL;
		goto out;
	}

	if (!(zone->memalign)) {
		goto out;
	}
	ptr = zone->memalign(zone, alignment, size);

	if (os_unlikely(malloc_logger)) {
		malloc_logger(MALLOC_LOG_TYPE_ALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE,
				(uintptr_t)zone, (uintptr_t)size, 0, (uintptr_t)ptr, 0);
	}
	if (os_unlikely(malloc_simple_stack_logging)) {
		malloc_report(MALLOC_SIMPLE_STACK_LOGGING_FLAGS,
				"memalign (%p/%llu,%llu): ", ptr, (unsigned long long)alignment,
				(unsigned long long)size);
	}

	MALLOC_TRACE(TRACE_memalign | DBG_FUNC_END, (uintptr_t)zone, alignment,
			size, (uintptr_t)ptr);

out:
#if MALLOC_TARGET_64BIT
	if (clear_type) {
		malloc_set_tsd_type_descriptor(MALLOC_TYPE_DESCRIPTOR_NONE);
	}
#endif // MALLOC_TARGET_64BIT

	if (os_unlikely(ptr == NULL)) {
		if (mzo & MZ_POSIX) {
			malloc_set_errno_fast(mzo, err);
		}
	}
	return ptr;
}

MALLOC_NOINLINE
void *
malloc_zone_memalign(malloc_zone_t *zone, size_t alignment, size_t size)
{
	return _malloc_zone_memalign(zone, alignment, size, MZ_NONE,
			malloc_callsite_fallback_type_descriptor());
}

boolean_t
malloc_zone_claimed_address(malloc_zone_t *zone, void *ptr)
{
	if (!ptr) {
		// NULL is not a member of any zone.
		return false;
	}

	if (malloc_check_start) {
		internal_check();
	}

	if (zone->version < 10 || !zone->claimed_address) {
		// For zones that have not implemented claimed_address, we always have
		// to return true to avoid a false negative.
		return true;
	}

	return zone->claimed_address(zone, ptr);
}

/*********	Functions for zone implementors	************/

void
malloc_zone_register(malloc_zone_t *zone)
{
	MALLOC_LOCK();
	malloc_zone_register_while_locked(zone, false);
	MALLOC_UNLOCK();
}

void
malloc_zone_unregister(malloc_zone_t *z)
{
	unsigned index;

	if (malloc_num_zones == 0) {
		return;
	}

	MALLOC_LOCK();
	for (index = 0; index < malloc_num_zones; ++index) {
		if (z != malloc_zones[index]) {
			continue;
		}

		// Modify the page to be allow write access, so that we can update the
		// malloc_zones array.
		size_t protect_size = malloc_num_zones_allocated * sizeof(malloc_zone_t *);
		mprotect(malloc_zones, protect_size, PROT_READ | PROT_WRITE);

		// If we found a match, replace it with the entry at the end of the list, shrink the list,
		// and leave the end of the list intact to avoid racing with find_registered_zone().

		malloc_zones[index] = malloc_zones[malloc_num_zones - 1];
		--malloc_num_zones;

		mprotect(malloc_zones, protect_size, PROT_READ);

		// MAX(num_zones, 1) retains the fast path in find_registered_zone() for zone 0 even
		// if it is a custom zone, e.g., ASan and user zones.
		initial_num_zones = MAX(MIN(malloc_num_zones, initial_num_zones), 1);

		// Exchange the roles of the FRZ counters. The counter that has captured the number of threads presently
		// executing *inside* find_registered_zone is swapped with the counter drained to zero last time through.
		// The former is then allowed to drain to zero while this thread yields.
		int32_t volatile *p = pFRZCounterLive;
		pFRZCounterLive = pFRZCounterDrain;
		pFRZCounterDrain = p;
		OSMemoryBarrier(); // Full memory barrier

		while (0 != *pFRZCounterDrain) {
			yield();
		}

		MALLOC_UNLOCK();

		return;
	}
	MALLOC_UNLOCK();
	malloc_report(ASL_LEVEL_ERR, "*** malloc_zone_unregister() failed for %p\n", z);
}

void
malloc_set_zone_name(malloc_zone_t *z, const char *name)
{
	// TODO: save and restore permissions generally
	bool mprotect_zone = true;
	if (mprotect_zone) {
		mprotect(z, sizeof(malloc_zone_t), PROT_READ | PROT_WRITE);
	}
	if (z->zone_name) {
		malloc_zone_t *old_zone = _find_registered_zone(z->zone_name, NULL, false);
		if (old_zone) {
			malloc_zone_free(old_zone, (char *)z->zone_name);
		}
		z->zone_name = NULL;
	}
	if (name) {
		size_t buflen = strlen(name) + 1;
		if (_dyld_is_memory_immutable(name, buflen)) {
			z->zone_name = name;
		} else {
			char *name_copy = _malloc_zone_malloc(z, buflen, MZ_NONE);
			if (name_copy) {
				strcpy(name_copy, name);
				z->zone_name = name_copy;
			}
		}

		malloc_zone_t *wrapped_zone = get_wrapped_zone(z);
		if (wrapped_zone) {
			// <name>-<wrapper_label>-<suffix>\0  // Wrapped zone name format
			//       ^               ^        ^   // 2 dashes and \0 -> +3
			const char *wrapper_label = get_wrapper_zone_label(z);
			const char *suffix = "Wrapped";
			size_t buflen = strlen(name) + strlen(wrapper_label) + strlen(suffix) + 3;
			char *wz_name = _malloc_zone_malloc(wrapped_zone, buflen, MZ_NONE);
			if (wz_name) {
				// snprintf() may allocate (not safe to use from libmalloc) and
				// _simple_sprintf()/_simple_salloc() call vm_allocate() which is
				// undesirable for such a simple API as malloc_set_zone_name()
				strcpy(wz_name, name);
				strcat(wz_name, "-");
				strcat(wz_name, wrapper_label);
				strcat(wz_name, "-");
				strcat(wz_name, suffix);
				malloc_set_zone_name(wrapped_zone, wz_name);
				malloc_zone_free(wrapped_zone, wz_name);
			}
		}
	}
	if (mprotect_zone) {
		mprotect(z, sizeof(malloc_zone_t), PROT_READ);
	}
}

const char *
malloc_get_zone_name(malloc_zone_t *zone)
{
	return zone->zone_name;
}

void
find_zone_and_free(void *ptr, bool known_non_default)
{
	malloc_zone_t *zone;
	size_t size;
	if (!ptr) {
		return;
	}

	zone = _find_registered_zone(ptr, &size, known_non_default);
	if (!zone) {
		int flags = MALLOC_REPORT_DEBUG | MALLOC_REPORT_NOLOG;
		if ((malloc_debug_flags & (MALLOC_ABORT_ON_CORRUPTION | MALLOC_ABORT_ON_ERROR))) {
			flags = MALLOC_REPORT_CRASH | MALLOC_REPORT_NOLOG;
		}
		malloc_report(flags,
				"*** error for object %p: pointer being freed was not allocated\n", ptr);
	} else if (zone->version >= 6 && zone->free_definite_size) {
		malloc_zone_free_definite_size(zone, ptr, size);
	} else {
		malloc_zone_free(zone, ptr);
	}
}


/*********	Generic ANSI callouts	************/

MALLOC_NOINLINE
void *
malloc(size_t size)
{
	return _malloc_zone_malloc(default_zone, size, MZ_POSIX);
}

MALLOC_NOINLINE
void *
aligned_alloc(size_t alignment, size_t size)
{
	return _malloc_zone_memalign(default_zone, alignment, size,
	    MZ_POSIX | MZ_C11, malloc_callsite_fallback_type_descriptor());
}

MALLOC_NOINLINE
void *
calloc(size_t num_items, size_t size)
{
	return _malloc_zone_calloc(default_zone, num_items, size, MZ_POSIX);
}

// We have this function so code within libmalloc can call it without going
// through the (potentially interposed) dyld symbol stub
void
_free(void *ptr)
{
	if (!ptr) {
		return;
	}

	malloc_zone_t *zone0 = malloc_zones[0];
	if (os_unlikely(malloc_slowpath ||
				malloc_logger ||
				zone0->version < 13)) {
		find_zone_and_free(ptr, false);
		return;
	}

	if (zone0->try_free_default) {
		zone0->try_free_default(zone0, ptr);
	} else {
		find_zone_and_free(ptr, false);
	}
}

MALLOC_NOINLINE
void
free(void *ptr)
{
	return _free(ptr);
}

// We have this function so code within libmalloc can call it without going
// through the (potentially interposed) dyld symbol stub
void *
_realloc(void *in_ptr, size_t new_size)
{
	void *retval = NULL;
	malloc_zone_t *zone;

	// SUSv3: "If size is 0 and ptr is not a null pointer, the object
	// pointed to is freed. If the space cannot be allocated, the object
	// shall remain unchanged."  Also "If size is 0, either a null pointer
	// or a unique pointer that can be successfully passed to free() shall
	// be returned."  We choose to allocate a minimum size object by calling
	// malloc_zone_malloc with zero size, which matches "If ptr is a null
	// pointer, realloc() shall be equivalent to malloc() for the specified
	// size."  So we only free the original memory if the allocation succeeds.
	//
	// When in_ptr is NULL, we want to ensure that the fallback type descriptor
	// is good.  We can ensure that by tail-calling from here, so that the
	// callsite information is accurate.
	//
	// We don't really care about the quality of the type descriptor
	// for the new_size == 0 allocation, so it's fine for it to always be based
	// on the callsite in this function.
	//
	// Note: the fact that we allocate from the default zone in the
	// new_size == 0 case regardless of the zone in_ptr belongs to is arguably a
	// bug.
	if (!in_ptr) {
		return _malloc_zone_malloc(default_zone, new_size, MZ_POSIX);
	} else if (new_size == 0) {
		retval = _malloc_zone_malloc(default_zone, new_size, MZ_NONE);
	} else {
		zone = _find_registered_zone(in_ptr, NULL, false);
		if (!zone) {
			int flags = MALLOC_REPORT_DEBUG | MALLOC_REPORT_NOLOG;
			const int abort_flags =
					(MALLOC_ABORT_ON_CORRUPTION | MALLOC_ABORT_ON_ERROR);
			if (malloc_debug_flags & abort_flags) {
				flags = MALLOC_REPORT_CRASH | MALLOC_REPORT_NOLOG;
			}
			malloc_report(flags, "*** error for object %p: "
					"pointer being realloc'd was not allocated\n", in_ptr);
		} else {
			retval = _malloc_zone_realloc(zone, in_ptr, new_size,
					malloc_callsite_fallback_type_descriptor());
		}
	}

	if (retval == NULL) {
		malloc_set_errno_fast(MZ_POSIX, ENOMEM);
	} else if (new_size == 0) {
		_free(in_ptr);
	}
	return retval;
}

MALLOC_NOINLINE
void *
realloc(void *in_ptr, size_t new_size)
{
	return _realloc(in_ptr, new_size);
}

MALLOC_NOINLINE
void *
reallocf(void *in_ptr, size_t new_size)
{
	void *ptr = realloc(in_ptr, new_size);

	if (!ptr && in_ptr && new_size != 0) {
		free(in_ptr);
	}

	return ptr;
}

MALLOC_NOINLINE
void *
valloc(size_t size)
{
	return _malloc_zone_valloc(default_zone, size, MZ_POSIX);
}

extern void
vfree(void *ptr)
{
	_free(ptr);
}

size_t
malloc_size(const void *ptr)
{
	size_t size = 0;

	if (!ptr) {
		return size;
	}

	(void)_find_registered_zone(ptr, &size, false);
	return size;
}

size_t
malloc_good_size(size_t size)
{
	malloc_zone_t *zone = default_zone;
	return zone->introspect->good_size(zone, size);
}

/*
 * The posix_memalign() function shall allocate size bytes aligned on a boundary specified by alignment,
 * and shall return a pointer to the allocated memory in memptr.
 * The value of alignment shall be a multiple of sizeof( void *), that is also a power of two.
 * Upon successful completion, the value pointed to by memptr shall be a multiple of alignment.
 *
 * Upon successful completion, posix_memalign() shall return zero; otherwise,
 * an error number shall be returned to indicate the error.
 *
 * The posix_memalign() function shall fail if:
 * EINVAL
 *	The value of the alignment parameter is not a power of two multiple of sizeof( void *).
 * ENOMEM
 *	There is insufficient memory available with the requested alignment.
 */

// We have this function so code within libmalloc can call it without going
// through the (potentially interposed) dyld symbol stub
int
_posix_memalign(void **memptr, size_t alignment, size_t size)
{
	void *retval;

	/* POSIX is silent on NULL == memptr !?! */

	retval = _malloc_zone_memalign(default_zone, alignment, size, MZ_NONE,
			malloc_callsite_fallback_type_descriptor());
	if (retval == NULL) {
		// To avoid testing the alignment constraints redundantly, we'll rely on the
		// test made in malloc_zone_memalign to vet each request. Only if that test fails
		// and returns NULL, do we arrive here to detect the bogus alignment and give the
		// required EINVAL return.
		if (alignment < sizeof(void *) ||			  // excludes 0 == alignment
				0 != (alignment & (alignment - 1))) { // relies on sizeof(void *) being a power of two.
			return EINVAL;
		}
		return ENOMEM;
	} else {
		*memptr = retval; // Set iff allocation succeeded
		return 0;
	}
}

MALLOC_NOINLINE
int
posix_memalign(void **memptr, size_t alignment, size_t size)
{
	return _posix_memalign(memptr, alignment, size);
}

boolean_t
malloc_claimed_address(void *ptr)
{
	// We need to check with each registered zone whether it claims "ptr".
	// Use logic similar to that in find_registered_zone().
	if (malloc_num_zones == 0) {
		return false;
	}

	// Start with the lite zone, if it's in use.
	if (lite_zone && malloc_zone_claimed_address(lite_zone, ptr)) {
		return true;
	}

	// Next, try the initial zones.
	for (uint32_t i = 0; i < initial_num_zones; i++) {
		if (malloc_zone_claimed_address(malloc_zones[i], ptr)) {
			return true;
		}
	}

	// Try all the other zones. Increment the FRZ barrier so that we can
	// walk the zones array without a lock (see find_registered_zone() for
	// the details).
	int32_t volatile *pFRZCounter = pFRZCounterLive;
	OSAtomicIncrement32Barrier(pFRZCounter);

	int32_t limit = *(int32_t volatile *)&malloc_num_zones;
	boolean_t result = false;
	for (uint32_t i = initial_num_zones; i < limit; i++) {
		malloc_zone_t *zone = malloc_zones[i];
		if (malloc_zone_claimed_address(zone, ptr)) {
			result = true;
			break;
		}
	}

	OSAtomicDecrement32Barrier(pFRZCounter);
	return result;
}

void *
reallocarray(void * in_ptr, size_t nmemb, size_t size){
	size_t alloc_size;
	if (os_mul_overflow(nmemb, size, &alloc_size)){
		malloc_set_errno_fast(MZ_POSIX, ENOMEM);
		return NULL;
	}
	return realloc(in_ptr, alloc_size);
}

void *
reallocarrayf(void * in_ptr, size_t nmemb, size_t size){
	size_t alloc_size;
	if (os_mul_overflow(nmemb, size, &alloc_size)){
		malloc_set_errno_fast(MZ_POSIX, ENOMEM);
		return NULL;
	}
	return reallocf(in_ptr, alloc_size);
}

void *
_malloc_zone_malloc_with_options_np_outlined(malloc_zone_t *zone, size_t align,
		size_t size, malloc_options_np_t options)
{
	void *ptr = NULL;

	if (zone == NULL || zone == default_zone) {
		zone = runtime_default_zone();
	}

#if MALLOC_TARGET_64BIT
	uint64_t type_id = malloc_get_tsd_type_id();
	bool clear_type = false;
	if (!type_id) {
		malloc_type_descriptor_t fallback =
				malloc_callsite_fallback_type_descriptor();
		malloc_set_tsd_type_descriptor(fallback);
		type_id = fallback.type_id;
		clear_type = true;
	}
#endif // MALLOC_TARGET_64BIT

	if (malloc_interposition_compat || (zone->version < 15) ||
			!zone->malloc_with_options) {
		// There's no reasonable way to have the fallback callsite type
		// descriptor work here.  That's okay, as it's uncommon and SPI, so its
		// callers should be built with TMO.
		if (align) {
			ptr = malloc_zone_memalign(zone, align, size);
			if (ptr && (options & MALLOC_NP_OPTION_CLEAR)) {
				memset(ptr, 0, size);
			}
		} else if (options & MALLOC_NP_OPTION_CLEAR) {
			ptr = malloc_zone_calloc(zone, 1, size);
		} else {
			ptr = malloc_zone_malloc(zone, size);
		}
	} else {
		MALLOC_TRACE(TRACE_malloc_options | DBG_FUNC_START, (uintptr_t)zone,
				align, size, 0);
		ptr = zone->malloc_with_options(zone, align, size, options);

		if (os_unlikely(malloc_logger)) {
			uint32_t flags = MALLOC_LOG_TYPE_ALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE;
			if (options & MALLOC_NP_OPTION_CLEAR) {
				flags |= MALLOC_LOG_TYPE_CLEARED;
			}
			malloc_logger(flags, (uintptr_t)zone, (uintptr_t)size,
					0, (uintptr_t)ptr, 0);
		}
		if (os_unlikely(malloc_simple_stack_logging)) {
			malloc_report(MALLOC_SIMPLE_STACK_LOGGING_FLAGS,
					"malloc_with_options (%p/%llu,%llu): ", ptr,
					(unsigned long long)align, (unsigned long long)size);
		}
		MALLOC_TRACE(TRACE_malloc_options | DBG_FUNC_END,
				(uintptr_t)zone, align, size, (uintptr_t)ptr);
	}

#if MALLOC_TARGET_64BIT
	if (clear_type) {
		malloc_set_tsd_type_descriptor(MALLOC_TYPE_DESCRIPTOR_NONE);
	}
#endif

	return ptr;
}

void *
malloc_zone_malloc_with_options_np(malloc_zone_t *zone, size_t align,
		size_t size, malloc_options_np_t options)
{
	if (os_unlikely((align != 0) && (!powerof2(align) ||
			((size & (align-1)) != 0)))) { // equivalent to (size % align != 0)
		return NULL;
	}

	if (os_unlikely(malloc_logger || malloc_slowpath)) {
		return _malloc_zone_malloc_with_options_np_outlined(zone, align, size,
				options);
	}

	if (zone == NULL || zone == default_zone) {
		zone = malloc_zones[0];
	}

	if (zone->version >= 16 && zone->malloc_type_malloc_with_options) {
		return zone->malloc_type_malloc_with_options(zone, align, size,
				options, malloc_callsite_fallback_type_id());
	}

	return _malloc_zone_malloc_with_options_np_outlined(zone, align, size,
			options);
}

/*********	Purgeable zone	************/

static void
_malloc_create_purgeable_zone(void * __unused ctx)
{

	//
	// PR_7288598: Must pass a *scalable* zone (szone) as the helper for create_purgeable_zone().
	// Take care that the zone so obtained is not subject to interposing.
	//
	if (!initial_scalable_zone) {
		// If the process didn't initially create a scalable zone because xzone
		// malloc is enabled, we should create one now.
		//
		// TODO: xzone-backed purgeable zone
		initial_scalable_zone = create_scalable_zone(0, malloc_debug_flags);
		malloc_set_zone_name(initial_scalable_zone,
				DEFAULT_SCALABLE_ZONE_STRING);
		malloc_zone_register(initial_scalable_zone);
	}

	default_purgeable_zone = create_purgeable_zone(0, initial_scalable_zone,
			malloc_debug_flags);
	malloc_zone_register(default_purgeable_zone);
	malloc_set_zone_name(default_purgeable_zone,
			DEFAULT_PUREGEABLE_ZONE_STRING);
}

malloc_zone_t *
malloc_default_purgeable_zone(void)
{
	static os_once_t pred;
	os_once(&pred, NULL, _malloc_create_purgeable_zone);
	return default_purgeable_zone;
}

static malloc_zone_t *
find_registered_purgeable_zone(void *ptr)
{
	if (!ptr) {
		return NULL;
	}

	/*
	 * Look for a zone which contains ptr.  If that zone does not have the purgeable malloc flag
	 * set, or the allocation is too small, do nothing.  Otherwise, set the allocation volatile.
	 * FIXME: for performance reasons, we should probably keep a separate list of purgeable zones
	 * and only search those.
	 */
	size_t size = 0;
	malloc_zone_t *zone = _find_registered_zone(ptr, &size, false);

	/* FIXME: would really like a zone->introspect->flags->purgeable check, but haven't determined
	 * binary compatibility impact of changing the introspect struct yet. */
	if (!zone) {
		return NULL;
	}

	/* Check to make sure pointer is page aligned and size is multiple of page size */
	if ((size < vm_page_size) || ((size % vm_page_size) != 0)) {
		return NULL;
	}

	return zone;
}

void
malloc_make_purgeable(void *ptr)
{
	malloc_zone_t *zone = find_registered_purgeable_zone(ptr);
	if (!zone) {
		return;
	}

	int state = VM_PURGABLE_VOLATILE;
	vm_purgable_control(mach_task_self(), (vm_address_t)ptr, VM_PURGABLE_SET_STATE, &state);
	return;
}

/* Returns true if ptr is valid.  Ignore the return value from vm_purgeable_control and only report
 * state. */
int
malloc_make_nonpurgeable(void *ptr)
{
	malloc_zone_t *zone = find_registered_purgeable_zone(ptr);
	if (!zone) {
		return 0;
	}

	int state = VM_PURGABLE_NONVOLATILE;
	vm_purgable_control(mach_task_self(), (vm_address_t)ptr, VM_PURGABLE_SET_STATE, &state);

	if (state == VM_PURGABLE_EMPTY) {
		return EFAULT;
	}

	return 0;
}

/*********	Memory events	************/

void
malloc_enter_process_memory_limit_warn_mode(void)
{
	// <rdar://problem/25063714>
}

// Note that malloc_memory_event_handler is not thread-safe, and we are relying on the callers of this for synchronization
void
malloc_memory_event_handler(unsigned long event)
{
#if CONFIG_MADVISE_PRESSURE_RELIEF || (CONFIG_LARGE_CACHE && !CONFIG_DEFERRED_RECLAIM)
	if (event & MALLOC_MEMORYSTATUS_MASK_PRESSURE_RELIEF) {
		malloc_zone_pressure_relief(0, 0);
	}
#endif /* CONFIG_MADVISE_PRESSURE_RELIEF || (CONFIG_LARGE_CACHE && !CONFIG_DEFERRED_RECLAIM) */

	if ((event & NOTE_MEMORYSTATUS_MSL_STATUS) != 0 && (event & ~NOTE_MEMORYSTATUS_MSL_STATUS) == 0) {
		malloc_register_stack_logger();
	}

#if ENABLE_MEMORY_RESOURCE_EXCEPTION_HANDLING
	if (event & MALLOC_MEMORYSTATUS_MASK_RESOURCE_EXCEPTION_HANDLING) {
		malloc_register_stack_logger();
	}
#endif /* ENABLE_MEMORY_RESOURCE_EXCEPTION_HANDLING */

	if (msl.handle_memory_event) {
		// Let MSL see the event.
		msl.handle_memory_event(event);
	}
}

size_t
malloc_zone_pressure_relief(malloc_zone_t *zone, size_t goal)
{
	if (!zone) {
		unsigned index = 0;
		size_t total = 0;

		// Take lock to defend against malloc_destroy_zone()
		MALLOC_LOCK();
		while (index < malloc_num_zones) {
			zone = malloc_zones[index++];
			if (zone->version < 8) {
				continue;
			}
			if (NULL == zone->pressure_relief) {
				continue;
			}
			if (0 == goal) { /* Greedy */
				total += zone->pressure_relief(zone, 0);
			} else if (goal > total) {
				total += zone->pressure_relief(zone, goal - total);
			} else { /* total >= goal */
				break;
			}
		}
		MALLOC_UNLOCK();
		return total;
	} else {
		// Assumes zone is not destroyed for the duration of this call
		if (zone->version < 8) {
			return 0;
		}
		if (NULL == zone->pressure_relief) {
			return 0;
		}
		return zone->pressure_relief(zone, goal);
	}
}

/*********	Batch methods	************/

unsigned
malloc_zone_batch_malloc(malloc_zone_t *zone, size_t size, void **results, unsigned num_requested)
{
	if (!zone->batch_malloc) {
		return 0;
	}
	if (malloc_check_start) {
		internal_check();
	}
	unsigned batched = zone->batch_malloc(zone, size, results, num_requested);
	
	if (os_unlikely(malloc_logger)) {
		unsigned index = 0;
		while (index < batched) {
			malloc_logger(MALLOC_LOG_TYPE_ALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE, (uintptr_t)zone, (uintptr_t)size, 0,
					(uintptr_t)results[index], 0);
			index++;
		}
	}
	return batched;
}

void
malloc_zone_batch_free(malloc_zone_t *zone, void **to_be_freed, unsigned num)
{
	if (malloc_check_start) {
		internal_check();
	}
	if (os_unlikely(malloc_logger)) {
		unsigned index = 0;
		while (index < num) {
			malloc_logger(
					MALLOC_LOG_TYPE_DEALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE, (uintptr_t)zone, (uintptr_t)to_be_freed[index], 0, 0, 0);
			index++;
		}
	}
	
	if (zone->batch_free) {
		zone->batch_free(zone, to_be_freed, num);
	} else {
		void (*free_fun)(malloc_zone_t *, void *) = zone->free;
		
		while (num--) {
			void *ptr = *to_be_freed++;
			free_fun(zone, ptr);
		}
	}
}

/*********  Functions for sanitization ************/

#if TARGET_OS_OSX

// Shadow memory helpers
#define SHADOW_MEMORY_BASE (0x0000200000000000ull)
#define SHADOW_GRANULARITY_BITS 3u
#define SHADOW_GRANULARITY (1u << SHADOW_GRANULARITY_BITS)
#define PTR_TO_SHADOW(p) (void *)((((uintptr_t)ptr) >> SHADOW_GRANULARITY_BITS) + SHADOW_MEMORY_BASE)
#define SIZE_TO_SHADOW_SIZE(p) ((p) >> SHADOW_GRANULARITY_BITS)
#define SHADOW_POISON_HEAP_LEFT_RZ 0xfa
#define SHADOW_POISON_HEAP_RIGHT_RZ 0xfb
#define SHADOW_POISON_HEAP_FREED 0xfd

static void
malloc_sanitizer_fallback_allocate_poison(uintptr_t ptr, size_t leftrz_sz, size_t alloc_sz, size_t rightrz_sz)
{
	uintptr_t alloc_ptr, rightrz_ptr;
	MALLOC_ASSERT(!os_add_overflow(ptr, leftrz_sz, &alloc_ptr) &&
				  !os_add_overflow(alloc_ptr, alloc_sz, &rightrz_ptr));

	// Tag the ASan shadow as ASAN_POISON_USER/ASAN_VALID/ASAN_POISON_USER
	if (sanitizer_fallback_ptrs.memory_poison && sanitizer_fallback_ptrs.memory_unpoison) {
		(*sanitizer_fallback_ptrs.memory_poison)(ptr, leftrz_sz);
		(*sanitizer_fallback_ptrs.memory_unpoison)(alloc_ptr, alloc_sz);
		(*sanitizer_fallback_ptrs.memory_poison)(rightrz_ptr, rightrz_sz);
	} else {
		// This code should only be executed by Rosetta on macOS
		MALLOC_ASSERT(!(leftrz_sz % SHADOW_GRANULARITY) &&
					  !((alloc_sz + rightrz_sz) % SHADOW_GRANULARITY));

		const uint8_t partial = alloc_sz & (SHADOW_GRANULARITY - 1);
		// Partial poison is not supported, leave it unpoisoned
		const size_t left = SIZE_TO_SHADOW_SIZE(leftrz_sz),
					right = SIZE_TO_SHADOW_SIZE(rightrz_sz),
					alloc = SIZE_TO_SHADOW_SIZE(alloc_sz) + !!partial;
		uint8_t *shadow = PTR_TO_SHADOW(ptr);
		memset(shadow, SHADOW_POISON_HEAP_LEFT_RZ, left);
		shadow += left;
		bzero(shadow + left, alloc);
		shadow += alloc;
		memset(shadow, SHADOW_POISON_HEAP_RIGHT_RZ, right);
	}
}

static void
malloc_sanitizer_fallback_deallocate_poison(uintptr_t ptr, size_t sz)
{
	// Tag the ASan shadow as ASAN_POISON_USER
	if (sanitizer_fallback_ptrs.memory_poison) {
		(*sanitizer_fallback_ptrs.memory_poison)(ptr, sz);
	} else {
		memset(PTR_TO_SHADOW(ptr), SHADOW_POISON_HEAP_FREED, SIZE_TO_SHADOW_SIZE(sz));
	}
}

#endif // TARGET_OS_OSX

bool malloc_sanitizer_is_enabled(void)
{
	return malloc_sanitizer_enabled;
}

const struct malloc_sanitizer_poison *
malloc_sanitizer_get_functions(void)
{
#if TARGET_OS_OSX
	return sanitizer ? sanitizer : &sanitizer_fallback;
#else
	return sanitizer;
#endif
}

void
malloc_sanitizer_set_functions(struct malloc_sanitizer_poison *s)
{
	sanitizer = s;
}

/*********	Functions for performance tools	************/

kern_return_t
malloc_get_all_zones(task_t task, memory_reader_t reader, vm_address_t **addresses, unsigned *count)
{
	// Note that the 2 following addresses are not correct if the address of the target is different from your own.  This notably
	// occurs if the address of System.framework is slid (e.g. different than at B & I )
	vm_address_t remote_malloc_zones = (vm_address_t)&malloc_zones;
	vm_address_t remote_malloc_num_zones = (vm_address_t)&malloc_num_zones;
	kern_return_t err;
	vm_address_t zones_address;
	vm_address_t *zones_address_ref;
	unsigned num_zones;
	unsigned *num_zones_ref;
	reader = reader_or_in_memory_fallback(reader, task);

	// printf("Read malloc_zones at address %p should be %p\n", &malloc_zones, malloc_zones);
	err = reader(task, remote_malloc_zones, sizeof(void *), (void **)&zones_address_ref);
	// printf("Read malloc_zones[%p]=%p\n", remote_malloc_zones, *zones_address_ref);
	if (err) {
		malloc_report(ASL_LEVEL_ERR, "*** malloc_get_all_zones: error reading zones_address at %p\n", (void *)remote_malloc_zones);
		return err;
	}
	zones_address = *zones_address_ref;
	// printf("Reading num_zones at address %p\n", remote_malloc_num_zones);
	err = reader(task, remote_malloc_num_zones, sizeof(unsigned), (void **)&num_zones_ref);
	if (err) {
		malloc_report(ASL_LEVEL_ERR, "*** malloc_get_all_zones: error reading num_zones at %p\n", (void *)remote_malloc_num_zones);
		return err;
	}
	num_zones = *num_zones_ref;
	// printf("Read malloc_num_zones[%p]=%d\n", remote_malloc_num_zones, num_zones);
	*count = num_zones;
	// printf("malloc_get_all_zones succesfully found %d zones\n", num_zones);
	err = reader(task, zones_address, sizeof(malloc_zone_t *) * num_zones, (void **)addresses);
	if (err) {
		malloc_report(ASL_LEVEL_ERR, "*** malloc_get_all_zones: error reading zones at %p\n", &zones_address);
		return err;
	}
	// printf("malloc_get_all_zones succesfully read %d zones\n", num_zones);
	return err;
}

/*********	Debug helpers	************/

void
malloc_zone_print_ptr_info(void *ptr)
{
	malloc_zone_t *zone;
	if (!ptr) {
		return;
	}
	zone = malloc_zone_from_ptr(ptr);
	if (zone) {
		printf("ptr %p in registered zone %p\n", ptr, zone);
	} else {
		printf("ptr %p not in heap\n", ptr);
	}
}

boolean_t
malloc_zone_check(malloc_zone_t *zone)
{
	boolean_t ok = 1;
	if (!zone) {
		unsigned index = 0;
		while (index < malloc_num_zones) {
			zone = malloc_zones[index++];
			if (!zone->introspect->check(zone)) {
				ok = 0;
			}
		}
	} else {
		ok = zone->introspect->check(zone);
	}
	return ok;
}

void
malloc_zone_print(malloc_zone_t *zone, boolean_t verbose)
{
	if (!zone) {
		unsigned index = 0;
		while (index < malloc_num_zones) {
			zone = malloc_zones[index++];
			zone->introspect->print(zone, verbose);
		}
	} else {
		zone->introspect->print(zone, verbose);
	}
}

void
malloc_zone_statistics(malloc_zone_t *zone, malloc_statistics_t *stats)
{
	if (!zone) {
		memset(stats, 0, sizeof(*stats));
		unsigned index = 0;
		while (index < malloc_num_zones) {
			zone = malloc_zones[index++];
			malloc_statistics_t this_stats;
			zone->introspect->statistics(zone, &this_stats);
			stats->blocks_in_use += this_stats.blocks_in_use;
			stats->size_in_use += this_stats.size_in_use;
			stats->max_size_in_use += this_stats.max_size_in_use;
			stats->size_allocated += this_stats.size_allocated;
		}
	} else {
		zone->introspect->statistics(zone, stats);
	}
}

void
malloc_zone_log(malloc_zone_t *zone, void *address)
{
	if (!zone) {
		unsigned index = 0;
		while (index < malloc_num_zones) {
			zone = malloc_zones[index++];
			zone->introspect->log(zone, address);
		}
	} else {
		zone->introspect->log(zone, address);
	}
}

/*********	Misc other entry points	************/

void
mag_set_thread_index(unsigned int index)
{
	_os_cpu_number_override = index;
#if CONFIG_NANOZONE
	nano_common_cpu_number_override_set();
#endif // CONFIG_NANOZONE
}

static void
DefaultMallocError(int x)
{
#if USE_SLEEP_RATHER_THAN_ABORT
	malloc_report(ASL_LEVEL_ERR, "*** error %d\n", x);
	sleep(3600);
#else
	_SIMPLE_STRING b = _simple_salloc();
	if (b) {
		_simple_sprintf(b, "*** error %d", x);
		malloc_report(MALLOC_REPORT_NOLOG, "%s\n", _simple_string(b));
		_os_set_crash_log_message_dynamic(_simple_string(b));
	} else {
		malloc_report(MALLOC_REPORT_NOLOG, "*** error %d\n", x);
		_os_set_crash_log_message("*** DefaultMallocError called");
	}
	abort();
#endif
}

void (*malloc_error(void (*func)(int)))(int)
{
	return DefaultMallocError;
}

static void
_malloc_lock_all(void (*callout)(void))
{
	unsigned index = 0;
	MALLOC_LOCK();
#if CONFIG_EARLY_MALLOC
	mfm_lock();
#endif
	while (index < malloc_num_zones) {
		malloc_zone_t *zone = malloc_zones[index++];
		zone->introspect->force_lock(zone);
	}
#if CONFIG_XZONE_MALLOC
	// All of the xzone malloc zones share some global state, and that global
	// state needs to be locked after all the zone specific state has been
	// locked, to prevent deadlocks
	if (initial_xzone_zone) {
		xzm_force_lock_global_state(initial_xzone_zone);
	}
#endif // CONFIG_XZONE_MALLOC
	if (callout) {
		callout();
	}
}

static void
_malloc_unlock_all(void (*callout)(void))
{
#if CONFIG_XZONE_MALLOC
	if (initial_xzone_zone) {
		xzm_force_unlock_global_state(initial_xzone_zone);
	}
#endif // CONFIG_XZONE_MALLOC
	unsigned index = 0;
	if (callout) {
		callout();
	}
	while (index < malloc_num_zones) {
		malloc_zone_t *zone = malloc_zones[index++];
		zone->introspect->force_unlock(zone);
	}
#if CONFIG_EARLY_MALLOC
	mfm_unlock();
#endif
	MALLOC_UNLOCK();
}

static void
_malloc_reinit_lock_all(void (*callout)(void))
{
#if CONFIG_XZONE_MALLOC
	if (initial_xzone_zone) {
		xzm_force_reinit_lock_global_state(initial_xzone_zone);
	}
#endif // CONFIG_XZONE_MALLOC
	unsigned index = 0;
	if (callout) {
		callout();
	}
	while (index < malloc_num_zones) {
		malloc_zone_t *zone = malloc_zones[index++];
		if (zone->version < 9) { // Version must be >= 9 to look at reinit_lock
			zone->introspect->force_unlock(zone);
		} else {
			zone->introspect->reinit_lock(zone);
		}
	}
#if CONFIG_EARLY_MALLOC
	mfm_reinit_lock();
#endif
	MALLOC_REINIT_LOCK();
}


// Called prior to fork() to guarantee that malloc is not in any critical
// sections during the fork(); prevent any locks from being held by non-
// surviving threads after the fork.
void
_malloc_fork_prepare(void)
{
	return _malloc_lock_all(msl.fork_prepare);
}

// Called in the parent process after fork() to resume normal operation.
void
_malloc_fork_parent(void)
{
	return _malloc_unlock_all(msl.fork_parent);
}

// Called in the child process after fork() to resume normal operation.
void
_malloc_fork_child(void)
{
#if CONFIG_NANOZONE
	if (_malloc_entropy_initialized) {
		if (initial_nano_zone) {
			nanov2_forked_zone((nanozonev2_t *)initial_nano_zone);
		}
	}
#endif
	return _malloc_reinit_lock_all(msl.fork_child);
}

/*
 * A Glibc-like mstats() interface.
 *
 * Note that this interface really isn't very good, as it doesn't understand
 * that we may have multiple allocators running at once.  We just massage
 * the result from malloc_zone_statistics in any case.
 */
struct mstats
mstats(void)
{
	malloc_statistics_t s;
	struct mstats m;

	malloc_zone_statistics(NULL, &s);
	m.bytes_total = s.size_allocated;
	m.chunks_used = s.blocks_in_use;
	m.bytes_used = s.size_in_use;
	m.chunks_free = 0;
	m.bytes_free = m.bytes_total - m.bytes_used; /* isn't this somewhat obvious? */

	return (m);
}

boolean_t
malloc_zone_enable_discharge_checking(malloc_zone_t *zone)
{
	if (zone->version < 7) { // Version must be >= 7 to look at the new discharge checking fields.
		return FALSE;
	}
	if (NULL == zone->introspect->enable_discharge_checking) {
		return FALSE;
	}
	return zone->introspect->enable_discharge_checking(zone);
}

void
malloc_zone_disable_discharge_checking(malloc_zone_t *zone)
{
	if (zone->version < 7) { // Version must be >= 7 to look at the new discharge checking fields.
		return;
	}
	if (NULL == zone->introspect->disable_discharge_checking) {
		return;
	}
	zone->introspect->disable_discharge_checking(zone);
}

void
malloc_zone_discharge(malloc_zone_t *zone, void *memory)
{
	if (NULL == zone) {
		zone = malloc_zone_from_ptr(memory);
	}
	if (NULL == zone) {
		return;
	}
	if (zone->version < 7) { // Version must be >= 7 to look at the new discharge checking fields.
		return;
	}
	if (NULL == zone->introspect->discharge) {
		return;
	}
	zone->introspect->discharge(zone, memory);
}

void
malloc_zone_enumerate_discharged_pointers(malloc_zone_t *zone, void (^report_discharged)(void *memory, void *info))
{
	if (!zone) {
		unsigned index = 0;
		while (index < malloc_num_zones) {
			zone = malloc_zones[index++];
			if (zone->version < 7) {
				continue;
			}
			if (NULL == zone->introspect->enumerate_discharged_pointers) {
				continue;
			}
			zone->introspect->enumerate_discharged_pointers(zone, report_discharged);
		}
	} else {
		if (zone->version < 7) {
			return;
		}
		if (NULL == zone->introspect->enumerate_discharged_pointers) {
			return;
		}
		zone->introspect->enumerate_discharged_pointers(zone, report_discharged);
	}
}

void
malloc_zero_on_free_disable(void)
{
	malloc_zero_policy = MALLOC_ZERO_NONE;
}

bool
malloc_variant_is_debug_4test(void)
{
#ifdef DEBUG
	return true;
#else
	return false;
#endif
}

/*****************	OBSOLETE ENTRY POINTS	********************/

#ifndef PHASE_OUT_OLD_MALLOC
#define PHASE_OUT_OLD_MALLOC 0
#endif

#if PHASE_OUT_OLD_MALLOC
#error PHASE OUT THE FOLLOWING FUNCTIONS
#endif

void
set_malloc_singlethreaded(boolean_t single)
{
	static boolean_t warned = 0;
	if (!warned) {
#if PHASE_OUT_OLD_MALLOC
		malloc_report(ASL_LEVEL_ERR, "*** OBSOLETE: set_malloc_singlethreaded(%d)\n", single);
#endif
		warned = 1;
	}
}

void
malloc_singlethreaded(void)
{
	static boolean_t warned = 0;
	if (!warned) {
		malloc_report(ASL_LEVEL_ERR, "*** OBSOLETE: malloc_singlethreaded()\n");
		warned = 1;
	}
}

int
malloc_debug(int level)
{
	malloc_report(ASL_LEVEL_ERR, "*** OBSOLETE: malloc_debug()\n");
	return 0;
}

#pragma mark -
#pragma mark Malloc Thread Options

typedef union {
	malloc_thread_options_t options;
	void *storage;
} th_opts_t;

MALLOC_STATIC_ASSERT(sizeof(th_opts_t) == sizeof(void *), "Options fit into pointer bits");

malloc_thread_options_t
malloc_get_thread_options(void)
{
	th_opts_t x = {.storage = _pthread_getspecific_direct(__TSD_MALLOC_THREAD_OPTIONS)};
	return x.options;
}

void
malloc_set_thread_options(malloc_thread_options_t opts)
{
	// Canonicalize options
	if (opts.DisableExpensiveDebuggingOptions) {
		opts.DisableProbabilisticGuardMalloc = true;
		opts.DisableMallocStackLogging = true;
	}

	pgm_thread_set_disabled(opts.DisableProbabilisticGuardMalloc);

	th_opts_t x = {.options = opts};
	_pthread_setspecific_direct(__TSD_MALLOC_THREAD_OPTIONS, x.storage);
}

#pragma mark -
#pragma mark Malloc Stack Logging

static bool _malloc_register_stack_logger(bool at_startup);

/* this is called from libsystem during initialization. */
static void
stack_logging_early_finished(const struct _malloc_late_init *funcs)
{
#if !TARGET_OS_DRIVERKIT
	_dlopen = funcs->dlopen;
	_dlsym = funcs->dlsym;
#endif
	if (funcs->version >= 2 && funcs->msl) {
		msl = *funcs->msl;
	}
	const char **env = (const char**) *_NSGetEnviron();
	for (const char **e = env; *e; e++) {
		if (0==strncmp(*e, "MallocStackLogging", 18)) {
			_malloc_register_stack_logger(true);
			if (msl.set_flags_from_environment) {
				// rdar://125495815 - re-fetch env, as it may have moved
				msl.set_flags_from_environment((const char **)*_NSGetEnviron());
			}
			break;
		}
	}
	if (msl.initialize) {
		msl.initialize();
	}
}


static os_once_t _register_msl_dylib_pred;

static void
register_msl_dylib(void *dylib)
{
#if TARGET_OS_DRIVERKIT
	set_msl_lite_hooks(msl.copy_msl_lite_hooks);
#else
	if (!dylib) {
		return;
	}
	msl.handle_memory_event = _dlsym(dylib, "msl_handle_memory_event");
	msl.stack_logging_locked = _dlsym(dylib, "msl_stack_logging_locked");
	msl.fork_prepare = _dlsym(dylib, "msl_fork_prepare");
	msl.fork_child = _dlsym(dylib, "msl_fork_child");
	msl.fork_parent = _dlsym(dylib, "msl_fork_parent");
	msl.turn_on_stack_logging = _dlsym(dylib, "msl_turn_on_stack_logging");
	msl.turn_off_stack_logging = _dlsym(dylib, "msl_turn_off_stack_logging");
	msl.set_flags_from_environment = _dlsym(dylib, "msl_set_flags_from_environment");
	msl.initialize = _dlsym(dylib, "msl_initialize");

	void (*msl_copy_msl_lite_hooks) (struct _malloc_msl_lite_hooks_s *hooksp, size_t size);
	msl_copy_msl_lite_hooks = _dlsym(dylib, "msl_copy_msl_lite_hooks");
	if (msl_copy_msl_lite_hooks) {
		set_msl_lite_hooks(msl_copy_msl_lite_hooks);
	}
#endif
}

MALLOC_EXPORT
boolean_t
malloc_register_stack_logger(void)
{
    return _malloc_register_stack_logger(false);
}

static bool
_malloc_register_stack_logger(bool at_startup)
{
	void *dylib = NULL;
	if (malloc_sanitizer_enabled && !at_startup) {
		return false;
	}
#if !TARGET_OS_DRIVERKIT
	if (msl.handle_memory_event != NULL) {
		return true;
	}
	if (_dlopen == NULL) {
		return false;
	}
	dylib = _dlopen("/System/Library/PrivateFrameworks/MallocStackLogging.framework/MallocStackLogging", RTLD_GLOBAL);
	if (dylib == NULL) {
		return false;	
	}
#endif
	os_once(&_register_msl_dylib_pred, dylib, register_msl_dylib);
	if (!msl.handle_memory_event) {
		malloc_report(ASL_LEVEL_WARNING, "failed to load MallocStackLogging.framework\n");
		return false;
	}
	return true;
}

/* Symbolication.framework looks up this symbol by name inside libsystem_malloc.dylib. */
uint64_t __mach_stack_logging_shared_memory_address = 0;


#pragma mark -
#pragma mark Malloc Stack Logging - Legacy stubs

/*
 * legacy API for MallocStackLogging.
 *
 * TODO, deprecate this, move clients off it and delete it.   Clients should move
 * to MallocStackLogging.framework for these APIs.
 */

MALLOC_EXPORT
boolean_t
turn_on_stack_logging(stack_logging_mode_type mode)
{
	malloc_register_stack_logger();
	if (!msl.turn_on_stack_logging) {
		return false;
	}
	return msl.turn_on_stack_logging(mode);
}

MALLOC_EXPORT
void turn_off_stack_logging(void)
{
	malloc_register_stack_logger();
	if (msl.turn_off_stack_logging) {
		msl.turn_off_stack_logging();
	}
}

/* WeChat references this, only god knows why.  This symbol does nothing. */
int stack_logging_enable_logging = 0;

/* vim: set noet:ts=4:sw=4:cindent: */
