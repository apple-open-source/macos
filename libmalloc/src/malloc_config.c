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

#include "internal.h"

static malloc_process_identity_t
_malloc_check_process_identity(malloc_config_input_t input)
{
	static const struct {
		const char *name;
		malloc_process_identity_t identity;
		malloc_target_platform_t target_platform;
	} name_identity_mapping[] = {
		{ "launchd_sim",             MALLOC_PROCESS_LAUNCHD, },
		{ "launchd_sim.development", MALLOC_PROCESS_LAUNCHD, },
		{ "launchd_sim.debug",       MALLOC_PROCESS_LAUNCHD, },
		{ "launchd",                 MALLOC_PROCESS_LAUNCHD, },
		{ "launchd.development",     MALLOC_PROCESS_LAUNCHD, },
		{ "launchd.debug",           MALLOC_PROCESS_LAUNCHD, },
		{ "launchd.testing",         MALLOC_PROCESS_LAUNCHD, },

		{ "logd",                MALLOC_PROCESS_LOGD, },
		{ "notifyd",             MALLOC_PROCESS_NOTIFYD, },

		{ "mediaparserd",        MALLOC_PROCESS_MEDIAPARSERD, },
		{ "videocodecd",         MALLOC_PROCESS_VIDEOCODECD, },
		{ "mediaplaybackd",      MALLOC_PROCESS_MEDIAPLAYBACKD, },
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
		{ "ThumbnailExtension",       MALLOC_PROCESS_QUICKLOOK_THUMBNAIL, },

		{ "QuickLookUIService",       MALLOC_PROCESS_MAC_QUICKLOOK_UISERVICE,  MALLOC_TARGET_PLATFORM_MAC, },
		{ "ThumbnailExtension_macOS", MALLOC_PROCESS_MAC_QUICKLOOK_THUMBNAIL,  MALLOC_TARGET_PLATFORM_MAC, },
		{ "QuickLookSatellite",       MALLOC_PROCESS_MAC_QUICKLOOK,            MALLOC_TARGET_PLATFORM_MAC },
		{ "quicklookd",               MALLOC_PROCESS_MAC_QUICKLOOK,            MALLOC_TARGET_PLATFORM_MAC },
		{ "com.apple.quicklook.ThumbnailsAgent", MALLOC_PROCESS_MAC_QUICKLOOK, MALLOC_TARGET_PLATFORM_MAC },
		{ "ExternalQuickLookSatellite-arm64",    MALLOC_PROCESS_MAC_QUICKLOOK, MALLOC_TARGET_PLATFORM_MAC },
		{ "ExternalQuickLookSatellite-x86_64",   MALLOC_PROCESS_MAC_QUICKLOOK, MALLOC_TARGET_PLATFORM_MAC },

		{ "MobileSafari",                                          MALLOC_PROCESS_BROWSER, },
		{ "SafariViewService",                                     MALLOC_PROCESS_BROWSER, },
		{ "com.apple.WebKit.Networking",                           MALLOC_PROCESS_BROWSER, },
		{ "com.apple.WebKit.Networking.Development",               MALLOC_PROCESS_BROWSER, },
		{ "com.apple.WebKit.GPU",                                  MALLOC_PROCESS_BROWSER, },
		{ "com.apple.WebKit.GPU.Development",                      MALLOC_PROCESS_BROWSER, },
		{ "com.apple.WebKit.WebContent",                           MALLOC_PROCESS_BROWSER, },
		{ "com.apple.WebKit.WebContent.Development",               MALLOC_PROCESS_BROWSER, },
		{ "com.apple.WebKit.WebContent.CaptivePortal",             MALLOC_PROCESS_BROWSER, },
		{ "com.apple.WebKit.WebContent.CaptivePortal.Development", MALLOC_PROCESS_BROWSER, },
		{ "com.apple.WebKit.WebContent.EnhancedSecurity",             MALLOC_PROCESS_BROWSER, },
		{ "com.apple.WebKit.WebContent.EnhancedSecurity.Development", MALLOC_PROCESS_BROWSER, },
		{ "MTLCompilerService",          MALLOC_PROCESS_MTLCOMPILERSERVICE },

		{ "Safari",                                      MALLOC_PROCESS_MAC_SAFARI,         MALLOC_TARGET_PLATFORM_MAC, },
		{ "com.apple.Safari.CredentialExtractionHelper", MALLOC_PROCESS_MAC_SAFARI_SUPPORT, MALLOC_TARGET_PLATFORM_MAC, },
		{ "com.apple.Safari.History",                    MALLOC_PROCESS_MAC_SAFARI_SUPPORT, MALLOC_TARGET_PLATFORM_MAC, },
		{ "com.apple.Safari.SandboxBroker",              MALLOC_PROCESS_MAC_SAFARI_SUPPORT, MALLOC_TARGET_PLATFORM_MAC, },
		{ "com.apple.Safari.SearchHelper",               MALLOC_PROCESS_MAC_SAFARI_SUPPORT, MALLOC_TARGET_PLATFORM_MAC, },
		{ "com.apple.SafariFoundation.CredentialProviderExtensionHelper",
				MALLOC_PROCESS_MAC_SAFARI_SUPPORT, MALLOC_TARGET_PLATFORM_MAC, },
		{ "com.apple.SafariPlatformSupport.Helper",      MALLOC_PROCESS_MAC_SAFARI_SUPPORT, MALLOC_TARGET_PLATFORM_MAC, },
		{ "com.apple.SafariServices.ExtensionHelper",    MALLOC_PROCESS_MAC_SAFARI_SUPPORT, MALLOC_TARGET_PLATFORM_MAC, },
		{ "com.apple.SafariServices",                    MALLOC_PROCESS_MAC_SAFARI_SUPPORT, MALLOC_TARGET_PLATFORM_MAC, },
		{ "VTDecoderXPCService",
				MALLOC_PROCESS_MAC_VTDECODERXPCSERVICE, MALLOC_TARGET_PLATFORM_MAC, },

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

		{ "presenced",           MALLOC_PROCESS_VISION_PRESENCED,      MALLOC_TARGET_PLATFORM_VISION, },
		{ "FaceTime",            MALLOC_PROCESS_VISION_FACETIME,       MALLOC_TARGET_PLATFORM_VISION, },
		{ "managedassetsd",      MALLOC_PROCESS_VISION_MANAGEDASSETSD, MALLOC_TARGET_PLATFORM_VISION, },
		{ "polarisd",            MALLOC_PROCESS_VISION_POLARISD,       MALLOC_TARGET_PLATFORM_VISION, },
		{ "arkitd",              MALLOC_PROCESS_VISION_ARKITD,         MALLOC_TARGET_PLATFORM_VISION, },
		{ "backboardd",          MALLOC_PROCESS_VISION_BACKBOARDD,     MALLOC_TARGET_PLATFORM_VISION, },
		{ "wakeboardd",          MALLOC_PROCESS_VISION_WAKEBOARDD,     MALLOC_TARGET_PLATFORM_VISION, },
		{ "realitycamerad",      MALLOC_PROCESS_VISION_REALITYCAMERAD, MALLOC_TARGET_PLATFORM_VISION, },

		{ "AegirPoster",         MALLOC_PROCESS_AEGIRPOSTER, },
		{ "CollectionsPoster",   MALLOC_PROCESS_COLLECTIONSPOSTER, },

		{ "backboardd",          MALLOC_PROCESS_WATCH_BACKBOARDD,      MALLOC_TARGET_PLATFORM_WATCH, },
		{ "ClockFace",           MALLOC_PROCESS_WATCH_CLOCKFACE,       MALLOC_TARGET_PLATFORM_WATCH, },

		{ "GroupSessionService",       MALLOC_PROCESS_MAC_GROUPSESSIONSERVICE,       MALLOC_TARGET_PLATFORM_MAC, },
		{ "IMTranscoderAgent",         MALLOC_PROCESS_MAC_IMTRANSCODERAGENT,         MALLOC_TARGET_PLATFORM_MAC, },
		{ "Messages",                  MALLOC_PROCESS_MAC_MESSAGES,                  MALLOC_TARGET_PLATFORM_MAC, },
		{ "Screen Sharing",            MALLOC_PROCESS_MAC_SCREENSHARING,             MALLOC_TARGET_PLATFORM_MAC, },
		{ "keychainsharingmessagingd", MALLOC_PROCESS_MAC_KEYCHAINSHARINGMESSAGINGD, MALLOC_TARGET_PLATFORM_MAC, },

		{ "VTEncoderXPCService",       MALLOC_PROCESS_MAC_VTENCODERXPCSERVICE,       MALLOC_TARGET_PLATFORM_MAC, },

		{ "ReportCrash", MALLOC_PROCESS_REPORTCRASH, },
		{ "AudioConverterService", MALLOC_PROCESS_AUDIOCONVERTERSERVICE, },

	};

	if (input->mci_is_pid_1) {
		return MALLOC_PROCESS_LAUNCHD;
	}

	const char *flag = _simple_getenv(input->mci_apple_array, "HardenedRuntime");
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
				return MALLOC_PROCESS_BROWSER;
			}
		}
	}

	const char *name = input->mci_progname;

	if (input->mci_os_security_config & OS_SECURITY_CONFIG_GUARD_OBJECTS) {
		// The names of these alternate Safari UI processes are extremely
		// generic, so we only check for them in processes that have guard
		// objects enabled on the platforms where we expect them, to avoid
		// collisions with unrelated processes with the same name
		malloc_target_platform_t tp = input->mci_target_platform;
		if (tp == MALLOC_TARGET_PLATFORM_IOS && !strcmp(name, "Web")) {
			return MALLOC_PROCESS_BROWSER;
		}

		if ((tp == MALLOC_TARGET_PLATFORM_IOS ||
				tp == MALLOC_TARGET_PLATFORM_VISION) &&
				!strcmp(name, "WebSheet")) {
			return MALLOC_PROCESS_BROWSER;
		}

		if (tp == MALLOC_TARGET_PLATFORM_MAC && !strcmp(name, "Web App")) {
			return MALLOC_PROCESS_BROWSER;
		}
	}

	for (size_t i = 0; i < countof(name_identity_mapping); i++) {
		malloc_target_platform_t identity_target_platform =
				name_identity_mapping[i].target_platform;
		if (!strcmp(name, name_identity_mapping[i].name) &&
				(identity_target_platform == MALLOC_TARGET_PLATFORM_ANY ||
				 identity_target_platform == input->mci_target_platform)) {
			return name_identity_mapping[i].identity;
		}
	}

	if (input->mci_os_security_config & OS_SECURITY_CONFIG_HARDENED_HEAP) {
		return MALLOC_PROCESS_HARDENED_HEAP_CONFIG;
	}

	return MALLOC_PROCESS_NONE;
}

OS_ENUM(malloc_env_result, uint8_t,
	MALLOC_ENV_RESULT_NOT_PRESENT,
	MALLOC_ENV_RESULT_PRESENT,
);

static malloc_env_result_t
__malloc_config_check_env_bool(malloc_config_input_t input, const char *name,
		bool *value_out)
{
	if (!input->mci_envp) {
		return MALLOC_ENV_RESULT_NOT_PRESENT;
	}

	const char *flag = _simple_getenv(input->mci_envp, name);
	if (flag) {
		const char *endp;
		long value = malloc_common_convert_to_long(flag, &endp);
		if (!*endp && endp != flag && (value == 0 || value == 1)) {
			*value_out = (bool)value;
			return MALLOC_ENV_RESULT_PRESENT;
		} else {
			malloc_report(ASL_LEVEL_ERR, "%s must be 0 or 1.\n", name);
		}
	}

	return MALLOC_ENV_RESULT_NOT_PRESENT;
}

static malloc_env_result_t
_malloc_config_check_development_only_env_bool(malloc_config_input_t input,
		const char *name, bool *value_out)
{
	if (!input->mci_allow_internal_security_policy) {
		return MALLOC_ENV_RESULT_NOT_PRESENT;
	}

	return __malloc_config_check_env_bool(input, name, value_out);
}

// These functions will generally overlap, but differ when there are processes
// that have recently been enabled that we still want to keep the feature flag
// for
static bool
_malloc_process_identity_supports_feature_flag(
		malloc_process_identity_t identity)
{
	switch (identity) {
	case MALLOC_PROCESS_AEGIRPOSTER:
	case MALLOC_PROCESS_COLLECTIONSPOSTER:
	case MALLOC_PROCESS_MAC_VTENCODERXPCSERVICE:
	case MALLOC_PROCESS_WATCH_BACKBOARDD:
	case MALLOC_PROCESS_WATCH_CLOCKFACE:
		return true;
	default:
		return false;
	}
}

static bool
_malloc_process_identity_disables_xzone_malloc(
		malloc_process_identity_t identity)
{
	switch (identity) {
	case MALLOC_PROCESS_AEGIRPOSTER:
	case MALLOC_PROCESS_COLLECTIONSPOSTER:
	case MALLOC_PROCESS_MAC_VTENCODERXPCSERVICE:
	case MALLOC_PROCESS_WATCH_BACKBOARDD:
	case MALLOC_PROCESS_WATCH_CLOCKFACE:
		return true;
	default:
		return false;
	}
}


static bool
_malloc_should_enable_xzone(malloc_config_input_t input,
		malloc_process_identity_t identity, const char **reason_out)
{
	malloc_target_platform_t tp = input->mci_target_platform;

	// These platforms mechanically can't support xzone malloc, so it is never
	// enabled
	if (!input->mci_arch_lp64) {
		*reason_out = "not LP64";
		return false;
	}

	// If a platform can support xzone malloc, the environment variable
	// overrides everything else
	bool secure_allocator_env_bool = false;
	malloc_env_result_t er = _malloc_config_check_development_only_env_bool(
			input, "MallocSecureAllocator", &secure_allocator_env_bool);
	if (er == MALLOC_ENV_RESULT_PRESENT) {
		*reason_out = "MallocSecureAllocator in environment";
		return secure_allocator_env_bool;
	}

#if CONFIG_MTE
	// If MTE is enabled for the process, that wins
	if ((input->mci_os_security_config & OS_SECURITY_CONFIG_MTE)) {
		*reason_out = "MTE";
		return true;
	}
#endif

	// Does the platform not enable xzone malloc by default system-wide?  If
	// not, that's all
	if (tp == MALLOC_TARGET_PLATFORM_BRIDGE) {
		*reason_out = "bridgeOS";
		return false;
	}

	// Does the platform have a system-wide feature flag that we need to check?
	if (input->mci_allow_internal_security_policy &&
			tp == MALLOC_TARGET_PLATFORM_TV &&
			!input->mci_feature_flags[MALLOC_FEATURE_FLAG_TVOS_ENABLEMENT]) {
		*reason_out = "tvOS feature flag";
		return false;
	}

	// xzone malloc is not enabled on pre-arm64e tvOS devices
	if (tp == MALLOC_TARGET_PLATFORM_TV && input->mci_arch_plain_arm64) {
		*reason_out = "arm64 tvOS";
		return false;
	}

	// xzone malloc is never enabled on Intel (including Rosetta and the
	// embedded simulators)
	if (input->mci_arch_intel) {
		*reason_out = "Intel";
		return false;
	}


	// When internal security is allowed, the processes that support feature
	// flags can have their enablement controlled that way
	if (input->mci_allow_internal_security_policy &&
			_malloc_process_identity_supports_feature_flag(identity)) {
		*reason_out = "process feature flag";
		return input->mci_process_feature_flags[identity];
	}

	// In the normal case without internal security, a few processes explicitly
	// have xzone malloc disabled
	if (_malloc_process_identity_disables_xzone_malloc(identity)) {
		*reason_out = "disabled known process";
		return false;
	}

	// And that's it: in all other cases, we enable xzone malloc
	*reason_out = "default";
	return true;
}


static bool
_malloc_should_enable_nano_on_xzone(malloc_config_input_t input)
{
	bool nano_on_xzone_env_bool = false;
	malloc_env_result_t er = _malloc_config_check_development_only_env_bool(
			input, "MallocNanoOnXzone", &nano_on_xzone_env_bool);
	if (er == MALLOC_ENV_RESULT_PRESENT) {
		return nano_on_xzone_env_bool;
	}


	return false;
}

malloc_config_result_s
malloc_config_from_input(malloc_config_input_t input)
{
	malloc_process_identity_t identity = _malloc_check_process_identity(input);
	const char *xzone_reason = NULL;
	bool enable_xzone_malloc = _malloc_should_enable_xzone(input, identity,
			&xzone_reason);

	bool nano_on_xzone = false;
	if (enable_xzone_malloc && input->mci_nano_version == NANO_V2) {
		nano_on_xzone = _malloc_should_enable_nano_on_xzone(input);
	}

	return (malloc_config_result_s){
		.mcr_process_identity = identity,
		.mcr_enable_xzone_malloc = enable_xzone_malloc,
		.mcr_enable_nano_on_xzone = nano_on_xzone,
		.mcr_xzone_enablement_reason = xzone_reason,
	};
}
