/*
 * Copyright (c) 1998-2006 Apple Computer, Inc. All rights reserved.
 * Copyright (c) 2007-2021 Apple Inc. All rights reserved.
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

#define IOKIT_ENABLE_SHARED_PTR

#include <AssertMacros.h>
#include <IOKit/IOLib.h>
#include <IOKit/IONVRAM.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOKitKeysPrivate.h>
#include <IOKit/IOBSD.h>
#include <kern/debug.h>
#include <os/system_event_log.h>
#include <pexpert/boot.h>
#include <sys/csr.h>

#define super IOService

OSDefineMetaClassAndStructors(IODTNVRAM, IOService);

class IONVRAMCHRPHandler;
class IONVRAMV3Handler;

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

#define MAX_VAR_NAME_SIZE     63

#define kNVRAMBankSizeKey    "nvram-bank-size"
#define kNVRAMBankCountKey   "nvram-bank-count"
#define kNVRAMCurrentBankKey "nvram-current-bank"

#define kCurrentGenerationCountKey "Generation"
#define kCurrentNVRAMVersionKey    "Version"

#define kNVRAMCommonUsedKey    "CommonUsed"
#define kNVRAMSystemUsedKey    "SystemUsed"

#define kIONVRAMPrivilege       kIOClientPrivilegeAdministrator

#define MIN_SYNC_NOW_INTERVAL 15*60 /* Minimum 15 Minutes interval mandated */

#if defined(DEBUG) || defined(DEVELOPMENT)
#define DEBUG_IFERROR(err, fmt, args...)                                     \
({                                                                           \
	if ((err != kIOReturnSuccess) || gNVRAMLogging)                          \
	IOLog("%s:%s:%u - " fmt, __FILE_NAME__, __FUNCTION__, __LINE__, ##args); \
})

#define DEBUG_INFO(fmt, args...)                                             \
({                                                                           \
	if (gNVRAMLogging)                                                       \
	IOLog("%s:%s:%u - " fmt, __FILE_NAME__, __FUNCTION__, __LINE__, ##args); \
})

#define DEBUG_ALWAYS(fmt, args...)                                           \
({                                                                           \
	IOLog("%s:%s:%u - " fmt, __FILE_NAME__, __FUNCTION__, __LINE__, ##args); \
})
#else
#define DEBUG_IFERROR(err, fmt, args...) (void)NULL
#define DEBUG_INFO(fmt, args...) (void)NULL
#define DEBUG_ALWAYS(fmt, args...) (void)NULL
#endif

#define DEBUG_ERROR DEBUG_ALWAYS

#define SAFE_TO_LOCK() (preemption_enabled() && !panic_active())

#define CONTROLLERLOCK()                     \
({                                           \
	if (SAFE_TO_LOCK())                  \
	        IOLockLock(_controllerLock); \
})

#define CONTROLLERUNLOCK()                     \
({                                             \
	if (SAFE_TO_LOCK())                    \
	        IOLockUnlock(_controllerLock); \
})

#define NVRAMREADLOCK()                       \
({                                            \
	if (SAFE_TO_LOCK())                   \
	        IORWLockRead(_variableLock);  \
})

#define NVRAMWRITELOCK()                      \
({                                            \
	if (SAFE_TO_LOCK())                   \
	        IORWLockWrite(_variableLock); \
})

#define NVRAMUNLOCK()                          \
({                                             \
	if (SAFE_TO_LOCK())                    \
	        IORWLockUnlock(_variableLock); \
})

#define NVRAMLOCKASSERTHELD()                                       \
({                                                                  \
	if (SAFE_TO_LOCK())                                         \
	        IORWLockAssert(_variableLock, kIORWLockAssertHeld); \
})

#define NVRAMLOCKASSERTEXCLUSIVE()                                   \
({                                                                   \
	if (SAFE_TO_LOCK())                                          \
	        IORWLockAssert(_variableLock, kIORWLockAssertWrite); \
})

// MOD = Write, Delete
// RST = Reset, Obliterate
// RD  = Read
// DEL = Delete
#define ENT_MOD_RST     ((1 << kIONVRAMOperationWrite)      | (1 << kIONVRAMOperationDelete)  | (1 << kIONVRAMOperationObliterate) | (1 << kIONVRAMOperationReset))
#define ENT_MOD_RD      ((1 << kIONVRAMOperationRead)       | (1 << kIONVRAMOperationWrite)   | (1 << kIONVRAMOperationDelete))
#define ENT_MOD         ((1 << kIONVRAMOperationWrite)      | (1 << kIONVRAMOperationDelete))
#define ENT_RST         ((1 << kIONVRAMOperationObliterate) | (1 << kIONVRAMOperationReset))
#define ENT_RD          ((1 << kIONVRAMOperationRead))
#define ENT_DEL         ((1 << kIONVRAMOperationDelete))

enum NVRAMVersion {
	kNVRAMVersionUnknown,
	kNVRAMVersion1,       // Legacy, banks, 0x800 common partition size
	kNVRAMVersion2,       // V1 but with (0x2000 - sizeof(struct apple_nvram_header) - sizeof(struct chrp_nvram_header)) common region
	kNVRAMVersion3,       // New EFI based format
	kNVRAMVersionMax
};

// Guid for Apple System Boot variables
// 40A0DDD2-77F8-4392-B4A3-1E7304206516
UUID_DEFINE(gAppleSystemVariableGuid, 0x40, 0xA0, 0xDD, 0xD2, 0x77, 0xF8, 0x43, 0x92, 0xB4, 0xA3, 0x1E, 0x73, 0x04, 0x20, 0x65, 0x16);

// Apple NVRAM Variable namespace (APPLE_VENDOR_OS_VARIABLE_GUID)
// 7C436110-AB2A-4BBB-A880-FE41995C9F82
UUID_DEFINE(gAppleNVRAMGuid, 0x7C, 0x43, 0x61, 0x10, 0xAB, 0x2A, 0x4B, 0xBB, 0xA8, 0x80, 0xFE, 0x41, 0x99, 0x5C, 0x9F, 0x82);

// Wifi NVRAM namespace
// 36C28AB5-6566-4C50-9EBD-CBB920F83843
UUID_DEFINE(gAppleWifiGuid, 0x36, 0xC2, 0x8A, 0xB5, 0x65, 0x66, 0x4C, 0x50, 0x9E, 0xBD, 0xCB, 0xB9, 0x20, 0xF8, 0x38, 0x43);

// Prefix for kernel-only variables
#define KERNEL_ONLY_VAR_NAME_PREFIX "krn."

static TUNABLE(bool, gNVRAMLogging, "nvram-log", false);
static bool gInternalBuild = false;

// IONVRAMSystemVariableListInternal:
// Used for internal builds only
// "force-lock-bits" used by fwam over ssh nvram to device so they are unable to use entitlements
// "stress-rack" used in SEG stress-rack restore prdocs
#define IONVRAMSystemVariableListInternal IONVRAMSystemVariableList, \
	                                      "force-lock-bits", \
	                                      "stress-rack"

// allowlist variables from macboot that need to be set/get from system region if present
static const char * const gNVRAMSystemList[] = { IONVRAMSystemVariableList, nullptr };
static const char * const gNVRAMSystemListInternal[] = { IONVRAMSystemVariableListInternal, nullptr };

typedef struct {
	const char *name;
	IONVRAMVariableType type;
} VariableTypeEntry;

static const
VariableTypeEntry gVariableTypes[] = {
	{"auto-boot?", kOFVariableTypeBoolean},
	{"boot-args", kOFVariableTypeString},
	{"boot-command", kOFVariableTypeString},
	{"boot-device", kOFVariableTypeString},
	{"boot-file", kOFVariableTypeString},
	{"boot-screen", kOFVariableTypeString},
	{"boot-script", kOFVariableTypeString},
	{"console-screen", kOFVariableTypeString},
	{"default-client-ip", kOFVariableTypeString},
	{"default-gateway-ip", kOFVariableTypeString},
	{"default-mac-address?", kOFVariableTypeBoolean},
	{"default-router-ip", kOFVariableTypeString},
	{"default-server-ip", kOFVariableTypeString},
	{"default-subnet-mask", kOFVariableTypeString},
	{"diag-device", kOFVariableTypeString},
	{"diag-file", kOFVariableTypeString},
	{"diag-switch?", kOFVariableTypeBoolean},
	{"fcode-debug?", kOFVariableTypeBoolean},
	{"input-device", kOFVariableTypeString},
	{"input-device-1", kOFVariableTypeString},
	{"little-endian?", kOFVariableTypeBoolean},
	{"ldm", kOFVariableTypeBoolean},
	{"load-base", kOFVariableTypeNumber},
	{"mouse-device", kOFVariableTypeString},
	{"nvramrc", kOFVariableTypeString},
	{"oem-banner", kOFVariableTypeString},
	{"oem-banner?", kOFVariableTypeBoolean},
	{"oem-logo", kOFVariableTypeString},
	{"oem-logo?", kOFVariableTypeBoolean},
	{"output-device", kOFVariableTypeString},
	{"output-device-1", kOFVariableTypeString},
	{"pci-probe-list", kOFVariableTypeNumber},
	{"pci-probe-mask", kOFVariableTypeNumber},
	{"real-base", kOFVariableTypeNumber},
	{"real-mode?", kOFVariableTypeBoolean},
	{"real-size", kOFVariableTypeNumber},
	{"screen-#columns", kOFVariableTypeNumber},
	{"screen-#rows", kOFVariableTypeNumber},
	{"security-mode", kOFVariableTypeString},
	{"selftest-#megs", kOFVariableTypeNumber},
	{"use-generic?", kOFVariableTypeBoolean},
	{"use-nvramrc?", kOFVariableTypeBoolean},
	{"virt-base", kOFVariableTypeNumber},
	{"virt-size", kOFVariableTypeNumber},
	// Variables used for testing
	{"test-bool", kOFVariableTypeBoolean},
	{"test-num", kOFVariableTypeNumber},
	{"test-str", kOFVariableTypeString},
	{"test-data", kOFVariableTypeData},
#if !defined(__x86_64__)
	{"acc-cm-override-charger-count", kOFVariableTypeNumber},
	{"acc-cm-override-count", kOFVariableTypeNumber},
	{"acc-mb-ld-lifetime", kOFVariableTypeNumber},
	{"com.apple.System.boot-nonce", kOFVariableTypeString},
	{"darkboot", kOFVariableTypeBoolean},
	{"enter-tdm-mode", kOFVariableTypeBoolean},
#endif /* !defined(__x86_64__) */
	{nullptr, kOFVariableTypeData} // Default type to return
};

union VariablePermission {
	struct {
		uint64_t UserWrite            :1;
		uint64_t RootRequired         :1;
		uint64_t KernelOnly           :1;
		uint64_t ResetNVRAMOnlyDelete :1;
		uint64_t NeverAllowedToDelete :1;
		uint64_t SystemReadHidden     :1;
		uint64_t FullAccess           :1;
		uint64_t InternalOnly         :1;
		uint64_t Reserved:57;
	} Bits;
	uint64_t Uint64;
};

typedef struct {
	const char *name;
	VariablePermission p;
} VariablePermissionEntry;

static const
VariablePermissionEntry gVariablePermissions[] = {
	{"aapl,pci", .p.Bits.RootRequired = 1},
	{"battery-health", .p.Bits.RootRequired = 1,
	 .p.Bits.NeverAllowedToDelete = 1},
	{"boot-image", .p.Bits.UserWrite = 1},
	{"com.apple.System.fp-state", .p.Bits.KernelOnly = 1},
	{"fm-account-masked", .p.Bits.RootRequired = 1,
	 .p.Bits.NeverAllowedToDelete = 1},
	{"fm-activation-locked", .p.Bits.RootRequired = 1,
	 .p.Bits.NeverAllowedToDelete = 1},
	{"fm-spkeys", .p.Bits.RootRequired = 1,
	 .p.Bits.NeverAllowedToDelete = 1},
	{"fm-spstatus", .p.Bits.RootRequired = 1,
	 .p.Bits.NeverAllowedToDelete = 1},
	{"policy-nonce-digests", .p.Bits.ResetNVRAMOnlyDelete = 1}, // Deleting this via user triggered obliterate leave J273a unable to boot
	{"recoveryos-passcode-blob", .p.Bits.SystemReadHidden = 1},
	{"security-password", .p.Bits.RootRequired = 1},
	{"system-passcode-lock-blob", .p.Bits.SystemReadHidden = 1},

#if !defined(__x86_64__)
	{"acc-cm-override-charger-count", .p.Bits.KernelOnly = 1},
	{"acc-cm-override-count", .p.Bits.KernelOnly = 1},
	{"acc-mb-ld-lifetime", .p.Bits.KernelOnly = 1},
	{"backlight-level", .p.Bits.UserWrite = 1},
	{"backlight-nits", .p.Bits.UserWrite = 1},
	{"ldm", .p.Bits.KernelOnly = 1},
	{"com.apple.System.boot-nonce", .p.Bits.KernelOnly = 1},
	{"com.apple.System.sep.art", .p.Bits.KernelOnly = 1},
	{"darkboot", .p.Bits.UserWrite = 1},
	{"nonce-seeds", .p.Bits.KernelOnly = 1},
#endif /* !defined(__x86_64__) */
	// Variables used for testing permissions
	{"testSysReadHidden", .p.Bits.SystemReadHidden = 1},
	{"testKernelOnly", .p.Bits.KernelOnly = 1},
	{"testResetOnlyDel", .p.Bits.ResetNVRAMOnlyDelete = 1},
	{"testNeverDel", .p.Bits.NeverAllowedToDelete = 1},
	{"testUserWrite", .p.Bits.UserWrite = 1},
	{"testRootReq", .p.Bits.RootRequired = 1},
	{"reclaim-int", .p.Bits.InternalOnly = 1},
	{nullptr, {.Bits.FullAccess = 1}} // Default access
};

typedef struct {
	const uint8_t checkOp;
	const uuid_t  *varGuid;
	const char    *varName;
	const char    *varEntitlement;
} VariableEntitlementEntry;

// variable-guid pair entries that require entitlement check to do specified nvram operations
static const
VariableEntitlementEntry gVariableEntitlements[] = {
	{ENT_MOD_RST, &gAppleNVRAMGuid, "ownership-warning", "com.apple.private.iokit.ddl-write"},
	{ENT_MOD, &gAppleSystemVariableGuid, "BluetoothInfo", "com.apple.private.iokit.nvram-bluetooth"},
	{ENT_MOD, &gAppleSystemVariableGuid, "BluetoothUHEDevices", "com.apple.private.iokit.nvram-bluetooth"},
	{ENT_MOD, &gAppleNVRAMGuid, "bluetoothExternalDongleFailed", "com.apple.private.iokit.nvram-bluetooth"},
	{ENT_MOD, &gAppleNVRAMGuid, "bluetoothInternalControllerInfo", "com.apple.private.iokit.nvram-bluetooth"},
	{ENT_RD, &gAppleSystemVariableGuid, "current-network", "com.apple.private.security.nvram.wifi-psks"},
	{ENT_RD, &gAppleWifiGuid, "current-network", "com.apple.private.security.nvram.wifi-psks"},
	{ENT_RD, &gAppleSystemVariableGuid, "preferred-networks", "com.apple.private.security.nvram.wifi-psks"},
	{ENT_RD, &gAppleWifiGuid, "preferred-networks", "com.apple.private.security.nvram.wifi-psks"},
	{ENT_RD, &gAppleSystemVariableGuid, "preferred-count", "com.apple.private.security.nvram.wifi-psks"},
	{ENT_RD, &gAppleWifiGuid, "preferred-count", "com.apple.private.security.nvram.wifi-psks"},
	// Variables used for testing entitlement
	{ENT_MOD_RST, &gAppleNVRAMGuid, "testEntModRst", "com.apple.private.iokit.testEntModRst"},
	{ENT_MOD_RST, &gAppleSystemVariableGuid, "testEntModRstSys", "com.apple.private.iokit.testEntModRst"},
	{ENT_RST, &gAppleNVRAMGuid, "testEntRst", "com.apple.private.iokit.testEntRst"},
	{ENT_RST, &gAppleSystemVariableGuid, "testEntRstSys", "com.apple.private.iokit.testEntRst"},
	{ENT_RD, &gAppleNVRAMGuid, "testEntRd", "com.apple.private.iokit.testEntRd"},
	{ENT_RD, &gAppleSystemVariableGuid, "testEntRdSys", "com.apple.private.iokit.testEntRd"},
	{ENT_DEL, &gAppleNVRAMGuid, "testEntDel", "com.apple.private.iokit.testEntDel"},
	{ENT_DEL, &gAppleSystemVariableGuid, "testEntDelSys", "com.apple.private.iokit.testEntDel"},
	{0, &UUID_NULL, nullptr, nullptr}
};

static NVRAMPartitionType
getPartitionTypeForGUID(const uuid_t guid)
{
	if (uuid_compare(guid, gAppleSystemVariableGuid) == 0) {
		return kIONVRAMPartitionSystem;
	} else {
		return kIONVRAMPartitionCommon;
	}
}

static IONVRAMVariableType
getVariableType(const char *propName)
{
	const VariableTypeEntry *entry;

	entry = gVariableTypes;
	while (entry->name != nullptr) {
		if (strcmp(entry->name, propName) == 0) {
			break;
		}
		entry++;
	}

	return entry->type;
}

static IONVRAMVariableType
getVariableType(const OSSymbol *propSymbol)
{
	return getVariableType(propSymbol->getCStringNoCopy());
}

static VariablePermission
getVariablePermission(const char *propName)
{
	const VariablePermissionEntry *entry;

	entry = gVariablePermissions;
	while (entry->name != nullptr) {
		if (strcmp(entry->name, propName) == 0) {
			break;
		}
		entry++;
	}

	return entry->p;
}

static bool
variableInAllowList(const char *varName)
{
	unsigned int i = 0;
	const char * const *list = gInternalBuild ? gNVRAMSystemListInternal : gNVRAMSystemList;

	while (list[i] != nullptr) {
		if (strcmp(varName, list[i]) == 0) {
			return true;
		}
		i++;
	}

	return false;
}

static bool
verifyWriteSizeLimit(const uuid_t varGuid, const char *variableName, size_t propDataSize)
{
	if (variableInAllowList(variableName)) {
		return propDataSize <= BOOT_LINE_LENGTH;
	}

	return true;
}

#if defined(DEBUG) || defined(DEVELOPMENT)
static const char *
getNVRAMOpString(IONVRAMOperation op)
{
	switch (op) {
	case kIONVRAMOperationRead:
		return "Read";
	case kIONVRAMOperationWrite:
		return "Write";
	case kIONVRAMOperationDelete:
		return "Delete";
	case kIONVRAMOperationObliterate:
		return "Obliterate";
	case kIONVRAMOperationReset:
		return "Reset";
	case kIONVRAMOperationInit:
		return "Init";
	default:
		return "Unknown";
	}
}
#endif

/*
 * Parse a variable name of the form "GUID:name".
 * If the name cannot be parsed, substitute the Apple global variable GUID.
 * Returns TRUE if a GUID was found in the name, FALSE otherwise.
 * The guidResult and nameResult arguments may be nullptr if you just want
 * to check the format of the string.
 */
static bool
parseVariableName(const char *key, uuid_t *guidResult, const char **nameResult)
{
	uuid_string_t temp    = {0};
	size_t        keyLen  = strlen(key);
	bool          ok      = false;
	const char    *name   = key;
	uuid_t        guid;

	if (keyLen > sizeof(temp)) {
		// check for at least UUID + ":" + more
		memcpy(temp, key, sizeof(temp) - 1);

		if ((uuid_parse(temp, guid) == 0) &&
		    (key[sizeof(temp) - 1] == ':')) {
			name = key + sizeof(temp);
			ok     = true;
		}
	}

	if (guidResult) {
		ok ? uuid_copy(*guidResult, guid) : uuid_copy(*guidResult, gAppleNVRAMGuid);
	}
	if (nameResult) {
		*nameResult = name;
	}

	return ok;
}

static bool
parseVariableName(const OSSymbol *key, uuid_t *guidResult, const char **nameResult)
{
	return parseVariableName(key->getCStringNoCopy(), guidResult, nameResult);
}

/**
 * @brief Translates(if needed) varGuid and stores it in destGuid
 *
 * @param varGuid       guid to translate
 * @param variableName  variable name attached to the guid
 * @param destGuid      translated guid is saved here
 * @param systemActive  boolean to indicate if it has system partition size > 0
 */
static void
translateGUID(const uuid_t varGuid, const char *variableName, uuid_t destGuid, bool systemActive)
{
	if (varGuid == nullptr || variableName == nullptr || destGuid == nullptr) {
		DEBUG_ERROR("nullptr passed as an argument\n");
		return;
	}

	bool systemGuid = uuid_compare(varGuid, gAppleSystemVariableGuid) == 0;

	if (systemActive) {
		if (variableInAllowList(variableName)) {
			DEBUG_INFO("Using system GUID due to allow list\n");
			uuid_copy(destGuid, gAppleSystemVariableGuid);
		} else if (systemGuid) {
			DEBUG_INFO("System GUID used\n");
			uuid_copy(destGuid, gAppleSystemVariableGuid);
		} else {
			DEBUG_INFO("Use given guid\n");
			uuid_copy(destGuid, varGuid);
		}
	} else if (systemGuid) {
		DEBUG_INFO("Overriding to Apple guid\n");
		uuid_copy(destGuid, gAppleNVRAMGuid);
	} else {
		DEBUG_INFO("Use given guid\n");
		uuid_copy(destGuid, varGuid);
	}
}

/**
 * @brief Checks if the variable-guid(translated) pair is present in gVariableEntitlements and if so,
 *        does it have the required entitlement for the NVRAM operation passed in
 *
 * @param varGuid       guid for the variable to be checked, this gets translated by translateGUID
 * @param varName       variable name
 * @param op            NVRAM operation
 * @param systemActive  used to pass into translateGUID to get the correct guid to check against
 * @param veChecked     if variable entitlement is checked, this is set to true
 * @return true         if variable wasn't present in gVariableEntitlements,
 *                      entitlement check wasn't required for operation passed in,
 *                      or if entitlement check returned true
 * @return false        if varName/varGuid/veChecked was NULL or if entitlement check returned false
 */
static bool
verifyVarEntitlement(const uuid_t varGuid, const char *varName, IONVRAMOperation op, bool systemActive, bool *veChecked)
{
	if (varGuid == nullptr || varName == nullptr || veChecked == nullptr) {
		DEBUG_ERROR("nullptr passed as an argument\n");
		return false;
	}

	uuid_t translatedGuid;
	const VariableEntitlementEntry *entry;
	*veChecked = false;

	translateGUID(varGuid, varName, translatedGuid, systemActive);

	entry = gVariableEntitlements;
	while ((entry != nullptr) && (entry->varName != nullptr)) {
		if ((strcmp(entry->varName, varName) == 0) && (uuid_compare(translatedGuid, *(entry->varGuid)) == 0)) {
			// check if task entitlement check is required for this operation
			if (entry->checkOp & (1 << op)) {
				*veChecked = true;
				DEBUG_INFO("Checking entitlement %s for %s for operation %s\n", entry->varEntitlement, varName, getNVRAMOpString(op));
				return IOCurrentTaskHasEntitlement(entry->varEntitlement);
			}
			break;
		}
		entry++;
	}

	return true;
}

static bool
kernelOnlyVar(const uuid_t varGuid, const char *varName)
{
	if (strncmp(varName, KERNEL_ONLY_VAR_NAME_PREFIX, sizeof(KERNEL_ONLY_VAR_NAME_PREFIX) - 1) == 0) {
		return true;
	}

	return false;
}

static bool
verifyPermission(IONVRAMOperation op, const uuid_t varGuid, const char *varName, const bool systemActive)
{
	VariablePermission perm;
	bool kernel, varEntitled, writeEntitled = false, readEntitled = false, allowList, systemGuid = false, systemEntitled = false, systemInternalEntitled = false, systemAllow, systemReadHiddenAllow = false;
	bool admin = false;
	bool ok = false;

	if (verifyVarEntitlement(varGuid, varName, op, systemActive, &varEntitled) == false) {
		goto exit;
	}

	perm = getVariablePermission(varName);

	kernel = current_task() == kernel_task;

	if (perm.Bits.KernelOnly || kernelOnlyVar(varGuid, varName)) {
		DEBUG_INFO("KernelOnly access for %s, kernel=%d\n", varName, kernel);
		ok = kernel;
		goto exit;
	}

	if (perm.Bits.InternalOnly && !gInternalBuild) {
		DEBUG_INFO("InternalOnly access for %s, gInternalBuild=%d\n", varName, gInternalBuild);
		goto exit;
	}

	allowList              = variableInAllowList(varName);
	systemGuid             = uuid_compare(varGuid, gAppleSystemVariableGuid) == 0;
	admin                  = IOUserClient::clientHasPrivilege(current_task(), kIONVRAMPrivilege) == kIOReturnSuccess;
	writeEntitled          = IOCurrentTaskHasEntitlement(kIONVRAMWriteAccessKey);
	readEntitled           = IOCurrentTaskHasEntitlement(kIONVRAMReadAccessKey);
	systemEntitled         = IOCurrentTaskHasEntitlement(kIONVRAMSystemAllowKey);
	systemInternalEntitled = IOCurrentTaskHasEntitlement(kIONVRAMSystemInternalAllowKey);
	systemReadHiddenAllow  = IOCurrentTaskHasEntitlement(kIONVRAMSystemHiddenAllowKey);

	systemAllow = systemEntitled || (systemInternalEntitled && gInternalBuild) || kernel;

	switch (op) {
	case kIONVRAMOperationRead:
		if (systemGuid && perm.Bits.SystemReadHidden) {
			ok = systemReadHiddenAllow;
		} else if (kernel || admin || readEntitled || perm.Bits.FullAccess || varEntitled) {
			ok = true;
		}
		break;

	case kIONVRAMOperationWrite:
		if (kernel || perm.Bits.UserWrite || admin || writeEntitled || varEntitled) {
			if (systemGuid) {
				if (allowList) {
					if (!systemAllow) {
						DEBUG_ERROR("Allowed write to system region when NOT entitled for %s\n", varName);
					}
				} else if (varEntitled) {
					DEBUG_INFO("Allowed write to system region using variable specific entitlement for %s\n", varName);
				} else if (!systemAllow) {
					DEBUG_ERROR("Not entitled for system region writes for %s\n", varName);
					break;
				}
			}
			ok = true;
		}
		break;

	case kIONVRAMOperationDelete:
	case kIONVRAMOperationObliterate:
	case kIONVRAMOperationReset:
		if (perm.Bits.NeverAllowedToDelete) {
			DEBUG_INFO("Never allowed to delete %s\n", varName);
			break;
		} else if ((op == kIONVRAMOperationObliterate) && perm.Bits.ResetNVRAMOnlyDelete) {
			DEBUG_INFO("Not allowed to obliterate %s\n", varName);
			break;
		} else if ((op == kIONVRAMOperationDelete) && perm.Bits.ResetNVRAMOnlyDelete) {
			DEBUG_INFO("Only allowed to delete %s via NVRAM reset\n", varName);
			break;
		}

		if (kernel || perm.Bits.UserWrite || admin || writeEntitled || varEntitled) {
			if (systemGuid) {
				if (allowList) {
					if (!systemAllow) {
						DEBUG_ERROR("Allowed delete to system region when NOT entitled for %s\n", varName);
					}
				} else if (varEntitled) {
					DEBUG_INFO("Allowed delete to system region using variable specific entitlement for %s\n", varName);
				} else if (!systemAllow) {
					DEBUG_ERROR("Not entitled for system region deletes for %s\n", varName);
					break;
				}
			}
			ok = true;
		}
		break;

	case kIONVRAMOperationInit:
		break;
	}

exit:
	DEBUG_INFO("Permission for %s of %s %s: I=%d kern=%d, adm=%d, wE=%d, rE=%d, sG=%d, sEd=%d, sIEd=%d, sRHA=%d, UW=%d, vE=%d\n", getNVRAMOpString(op), varName, ok ? "granted" : "denied",
	    gInternalBuild, kernel, admin, writeEntitled, readEntitled, systemGuid, systemEntitled, systemInternalEntitled, systemReadHiddenAllow, perm.Bits.UserWrite, varEntitled);

	return ok;
}

static bool
verifyPermission(IONVRAMOperation op, const OSSymbol *canonicalKey, const bool systemActive)
{
	const char *varName;
	uuid_t varGuid;

	parseVariableName(canonicalKey->getCStringNoCopy(), &varGuid, &varName);

	return verifyPermission(op, varGuid, varName, systemActive);
}

static bool
skipKey(const OSSymbol *aKey)
{
	return aKey->isEqualTo(kIORegistryEntryAllowableSetPropertiesKey) ||
	       aKey->isEqualTo(kIORegistryEntryDefaultLockingSetPropertiesKey) ||
	       aKey->isEqualTo(kIOClassNameOverrideKey) ||
	       aKey->isEqualTo(kIOBSDNameKey) ||
	       aKey->isEqualTo(kIOBSDNamesKey) ||
	       aKey->isEqualTo(kIOBSDMajorKey) ||
	       aKey->isEqualTo(kIOBSDMinorKey) ||
	       aKey->isEqualTo(kIOBSDUnitKey) ||
	       aKey->isEqualTo(kIOUserServicePropertiesKey) ||
	       aKey->isEqualTo(kIOExclaveAssignedKey) ||
	       aKey->isEqualTo(kIOMatchCategoryKey) ||
	       aKey->isEqualTo(kIOBusyInterest);
}

static OSSharedPtr<const OSSymbol>
keyWithGuidAndCString(const uuid_t guid, const char * cstring)
{
	size_t                      length;
	OSSharedPtr<const OSSymbol> symbolObj;
	char                        *canonicalString;

	length = sizeof(uuid_string_t) - 1 + sizeof(':') + strlen(cstring) + 1;

	canonicalString = (char *) IOMallocData(length);
	if (canonicalString == nullptr) {
		return NULL;
	}

	uuid_unparse(guid, *((uuid_string_t*)canonicalString));
	canonicalString[sizeof(uuid_string_t) - 1] = ':';

	strlcpy(&canonicalString[sizeof(uuid_string_t)], cstring, length - sizeof(uuid_string_t));

	symbolObj = OSSymbol::withCString(canonicalString);
	IOFreeData(canonicalString, length);

	return symbolObj;
}

static void
dumpDict(const OSDictionary *dict)
{
	const OSSymbol                    *key;
	OSSharedPtr<OSCollectionIterator> iter;
	unsigned int                      count = 0;

	iter = OSCollectionIterator::withCollection(dict);
	if (iter == nullptr) {
		DEBUG_ERROR("failed to create iterator\n");
		goto exit;
	}

	DEBUG_INFO("Dumping dict...\n");
	while ((key = OSDynamicCast(OSSymbol, iter->getNextObject()))) {
		count++;
		DEBUG_INFO("%u: %s\n", count, key->getCStringNoCopy());
	}

exit:
	return;
}

// ************************** IODTNVRAMPlatformNotifier ****************************
// private IOService based class for passing notifications to IODTNVRAM

class IODTNVRAMPlatformNotifier : public IOService
{
	OSDeclareDefaultStructors(IODTNVRAMPlatformNotifier)
private:
	IODTNVRAM *_provider;

public:
	bool start(IOService * provider) APPLE_KEXT_OVERRIDE;

	virtual IOReturn callPlatformFunction( const OSSymbol * functionName,
	    bool waitForFunction,
	    void *param1, void *param2,
	    void *param3, void *param4 ) APPLE_KEXT_OVERRIDE;
};

OSDefineMetaClassAndStructors(IODTNVRAMPlatformNotifier, IOService)

bool
IODTNVRAMPlatformNotifier::start(IOService * provider)
{
	OSSharedPtr<OSSerializer> serializer;
	OSSharedPtr<OSNumber> value = OSNumber::withNumber(1000, 32);

	_provider = OSDynamicCast(IODTNVRAM, provider);
	require(_provider != nullptr, error);

	setProperty(gIOPlatformWakeActionKey, value.get());

	require(super::start(provider), error);

	registerService();

	return true;

error:
	stop(provider);

	return false;
}

#include <IOKit/IOHibernatePrivate.h>
#include <IOKit/pwr_mgt/RootDomain.h>
static const OSSharedPtr<const OSSymbol> gIOHibernateStateKey = OSSymbol::withCString(kIOHibernateStateKey);

static uint32_t
hibernateState(void)
{
	OSSharedPtr<OSData> data = OSDynamicPtrCast<OSData>(IOService::getPMRootDomain()->copyProperty(gIOHibernateStateKey.get()->getCStringNoCopy()));
	uint32_t hibernateState = 0;
	if ((data != NULL) && (data->getLength() == sizeof(hibernateState))) {
		memcpy(&hibernateState, data->getBytesNoCopy(), sizeof(hibernateState));
	}
	return hibernateState;
}

IOReturn
IODTNVRAMPlatformNotifier::callPlatformFunction( const OSSymbol * functionName,
    bool waitForFunction,
    void *param1, void *param2,
    void *param3, void *param4 )
{
	if ((functionName == gIOPlatformWakeActionKey) &&
	    (hibernateState() == kIOHibernateStateWakingFromHibernate)) {
		DEBUG_INFO("waking from hibernate\n");
		_provider->reload();
		return kIOReturnSuccess;
	}

	return super::callPlatformFunction(functionName, waitForFunction, param1, param2, param3, param4);
}


// ************************** IODTNVRAMDiags ****************************
// private IOService based class for passing notifications to IODTNVRAM
#define kIODTNVRAMDiagsStatsKey   "Stats"
#define kIODTNVRAMDiagsInitKey    "Init"
#define kIODTNVRAMDiagsReadKey    "Read"
#define kIODTNVRAMDiagsWriteKey   "Write"
#define kIODTNVRAMDiagsDeleteKey  "Delete"
#define kIODTNVRAMDiagsNameKey    "Name"
#define kIODTNVRAMDiagsSizeKey    "Size"
#define kIODTNVRAMDiagsPresentKey "Present"

// private IOService based class for publishing diagnostic info for IODTNVRAM
class IODTNVRAMDiags : public IOService
{
	OSDeclareDefaultStructors(IODTNVRAMDiags)
private:
	IODTNVRAM                 *_provider;
	IORWLock                  *_variableLock;
	OSSharedPtr<OSDictionary> _stats;

	bool serializeStats(void *, OSSerialize * serializer);

public:
	bool start(IOService * provider) APPLE_KEXT_OVERRIDE;
	void logVariable(NVRAMPartitionType region, IONVRAMOperation op, const char *name, void *data);
};

OSDefineMetaClassAndStructors(IODTNVRAMDiags, IOService)

bool
IODTNVRAMDiags::start(IOService * provider)
{
	OSSharedPtr<OSSerializer> serializer;

	_provider = OSDynamicCast(IODTNVRAM, provider);
	require(_provider != nullptr, error);

	require(super::start(provider), error);

	_variableLock = IORWLockAlloc();
	require(_variableLock != nullptr, error);

	_stats = OSDictionary::withCapacity(1);
	require(_stats != nullptr, error);

	serializer = OSSerializer::forTarget(this, OSMemberFunctionCast(OSSerializerCallback, this, &IODTNVRAMDiags::serializeStats));
	require(serializer != nullptr, error);

	setProperty(kIODTNVRAMDiagsStatsKey, serializer.get());

	registerService();

	return true;

error:
	stop(provider);

	return false;
}

void
IODTNVRAMDiags::logVariable(NVRAMPartitionType region, IONVRAMOperation op, const char *name, void *data)
{
	// "Stats"        : OSDictionary
	// - "XX:varName" : OSDictionary, XX is the region value prefix to distinguish which dictionary the variable is in
	//   - "Init"     : OSBoolean True/present if variable present at initialization
	//   - "Read"     : OSNumber count
	//   - "Write"    : OSNumber count
	//   - "Delete"   : OSNumber count
	//   - "Size"     : OSNumber size, latest size from either init or write
	//   - "Present"  : OSBoolean True/False if variable is present or not
	char                      *entryKey;
	size_t                    entryKeySize;
	OSSharedPtr<OSDictionary> existingEntry;
	OSSharedPtr<OSNumber>     currentCount;
	OSSharedPtr<OSNumber>     varSize;
	const char                *opCountKey = nullptr;

	entryKeySize = strlen("XX:") + strlen(name) +  1;
	entryKey = IONewData(char, entryKeySize);
	require(entryKey, exit);

	snprintf(entryKey, entryKeySize, "%02X:%s", region, name);

	NVRAMWRITELOCK();
	existingEntry.reset(OSDynamicCast(OSDictionary, _stats->getObject(entryKey)), OSRetain);

	if (existingEntry == nullptr) {
		existingEntry = OSDictionary::withCapacity(4);
	}

	switch (op) {
	case kIONVRAMOperationRead:
		opCountKey = kIODTNVRAMDiagsReadKey;
		if (existingEntry->getObject(kIODTNVRAMDiagsPresentKey) == nullptr) {
			existingEntry->setObject(kIODTNVRAMDiagsPresentKey, kOSBooleanFalse);
		}
		break;
	case kIONVRAMOperationWrite:
		opCountKey = kIODTNVRAMDiagsWriteKey;
		varSize = OSNumber::withNumber((size_t)data, 64);
		existingEntry->setObject(kIODTNVRAMDiagsSizeKey, varSize);
		existingEntry->setObject(kIODTNVRAMDiagsPresentKey, kOSBooleanTrue);
		break;
	case kIONVRAMOperationDelete:
	case kIONVRAMOperationObliterate:
	case kIONVRAMOperationReset:
		opCountKey = kIODTNVRAMDiagsDeleteKey;
		existingEntry->setObject(kIODTNVRAMDiagsPresentKey, kOSBooleanFalse);
		break;
	case kIONVRAMOperationInit:
		varSize = OSNumber::withNumber((size_t)data, 64);
		existingEntry->setObject(kIODTNVRAMDiagsInitKey, varSize);
		existingEntry->setObject(kIODTNVRAMDiagsSizeKey, varSize);
		existingEntry->setObject(kIODTNVRAMDiagsPresentKey, kOSBooleanTrue);
		break;
	default:
		goto unlock;
	}

	if (opCountKey) {
		currentCount.reset(OSDynamicCast(OSNumber, existingEntry->getObject(opCountKey)), OSRetain);

		if (currentCount == nullptr) {
			currentCount = OSNumber::withNumber(1, 64);
		} else {
			currentCount->addValue(1);
		}

		existingEntry->setObject(opCountKey, currentCount);
	}

	_stats->setObject(entryKey, existingEntry);

unlock:
	NVRAMUNLOCK();

exit:
	IODeleteData(entryKey, char, entryKeySize);

	return;
}

bool
IODTNVRAMDiags::serializeStats(void *, OSSerialize * serializer)
{
	bool ok;

	NVRAMREADLOCK();
	ok = _stats->serialize(serializer);
	NVRAMUNLOCK();

	return ok;
}

// ************************** IODTNVRAMVariables ****************************

// private IOService based class for publishing distinct dictionary properties on
// for easy ioreg access since the serializeProperties call is overloaded and is used
// as variable access
class IODTNVRAMVariables : public IOService
{
	OSDeclareDefaultStructors(IODTNVRAMVariables)
private:
	IODTNVRAM        *_provider;
	uuid_t           _guid;
	bool             _systemActive;

public:
	bool                    init(const uuid_t guid, const bool systemActive);
	virtual bool            start(IOService * provider) APPLE_KEXT_OVERRIDE;

	virtual bool            serializeProperties(OSSerialize *s) const APPLE_KEXT_OVERRIDE;
	virtual OSPtr<OSObject> copyProperty(const OSSymbol *aKey) const APPLE_KEXT_OVERRIDE;
	virtual OSObject        *getProperty(const OSSymbol *aKey) const APPLE_KEXT_OVERRIDE;
	virtual bool            setProperty(const OSSymbol *aKey, OSObject *anObject) APPLE_KEXT_OVERRIDE;
	virtual IOReturn        setProperties(OSObject *properties) APPLE_KEXT_OVERRIDE;
	virtual void            removeProperty(const OSSymbol *aKey) APPLE_KEXT_OVERRIDE;
};

OSDefineMetaClassAndStructors(IODTNVRAMVariables, IOService)

bool
IODTNVRAMVariables::init(const uuid_t guid, const bool systemActive)
{
	require(super::init(), fail);

	uuid_copy(_guid, guid);
	_systemActive = systemActive;

	return true;

fail:
	return false;
}

bool
IODTNVRAMVariables::start(IOService * provider)
{
	_provider = OSDynamicCast(IODTNVRAM, provider);
	if (_provider == nullptr) {
		goto error;
	}

	if (!super::start(provider)) {
		goto error;
	}

	registerService();

	return true;

error:
	stop(provider);

	return false;
}

bool
IODTNVRAMVariables::serializeProperties(OSSerialize *s) const
{
	const OSSymbol                    *key;
	OSSharedPtr<OSDictionary>         dict;
	OSSharedPtr<OSCollectionIterator> iter;
	OSSharedPtr<OSDictionary>         localVariables = _provider->_varDict;
	bool                              ok = false;

	dict = OSDictionary::withCapacity(localVariables->getCount());
	if (dict == nullptr) {
		DEBUG_ERROR("No dictionary\n");
		goto exit;
	}

	iter = OSCollectionIterator::withCollection(localVariables.get());
	if (iter == nullptr) {
		DEBUG_ERROR("failed to create iterator\n");
		goto exit;
	}

	while ((key = OSDynamicCast(OSSymbol, iter->getNextObject()))) {
		if (verifyPermission(kIONVRAMOperationRead, key, _systemActive)) {
			uuid_t guid;
			const char *name;

			parseVariableName(key, &guid, &name);

			if (uuid_compare(_guid, guid) == 0) {
				OSSharedPtr<const OSSymbol> sym = OSSymbol::withCString(name);
				dict->setObject(sym.get(), localVariables->getObject(key));
			}
		}
	}

	ok = dict->serialize(s);

exit:
	DEBUG_INFO("ok=%d\n", ok);
	return ok;
}

OSPtr<OSObject>
IODTNVRAMVariables::copyProperty(const OSSymbol *aKey) const
{
	if (_provider && !skipKey(aKey)) {
		DEBUG_INFO("aKey=%s\n", aKey->getCStringNoCopy());

		return _provider->copyPropertyWithGUIDAndName(_guid, aKey->getCStringNoCopy());
	} else {
		return nullptr;
	}
}

OSObject *
IODTNVRAMVariables::getProperty(const OSSymbol *aKey) const
{
	OSSharedPtr<OSObject> theObject = copyProperty(aKey);

	return theObject.get();
}

bool
IODTNVRAMVariables::setProperty(const OSSymbol *aKey, OSObject *anObject)
{
	if (_provider) {
		return _provider->setPropertyWithGUIDAndName(_guid, aKey->getCStringNoCopy(), anObject) == kIOReturnSuccess;
	} else {
		return false;
	}
}

IOReturn
IODTNVRAMVariables::setProperties(OSObject *properties)
{
	IOReturn                          ret = kIOReturnSuccess;
	OSObject                          *object;
	const OSSymbol                    *key;
	OSDictionary                      *dict;
	OSSharedPtr<OSCollectionIterator> iter;

	dict = OSDynamicCast(OSDictionary, properties);
	if (dict == nullptr) {
		DEBUG_ERROR("Not a dictionary\n");
		return kIOReturnBadArgument;
	}

	iter = OSCollectionIterator::withCollection(dict);
	if (iter == nullptr) {
		DEBUG_ERROR("Couldn't create iterator\n");
		return kIOReturnBadArgument;
	}

	while (ret == kIOReturnSuccess) {
		key = OSDynamicCast(OSSymbol, iter->getNextObject());
		if (key == nullptr) {
			break;
		}

		object = dict->getObject(key);
		if (object == nullptr) {
			continue;
		}

		ret = _provider->setPropertyWithGUIDAndName(_guid, key->getCStringNoCopy(), object);
	}

	DEBUG_INFO("ret=%#08x\n", ret);

	return ret;
}

void
IODTNVRAMVariables::removeProperty(const OSSymbol *aKey)
{
	_provider->removePropertyWithGUIDAndName(_guid, aKey->getCStringNoCopy());
}

// ************************** Format Handlers ***************************
class IODTNVRAMFormatHandler
{
protected:
	uint32_t _bankSize;
	uint32_t _bankCount;
	uint32_t _currentBank;

public:
	virtual
	~IODTNVRAMFormatHandler();
	virtual bool     getNVRAMProperties(void);
	virtual IOReturn unserializeVariables(void) = 0;
	virtual IOReturn setVariable(const uuid_t varGuid, const char *variableName, OSObject *object) = 0;
	virtual bool     setController(IONVRAMController *_nvramController) = 0;
	virtual IOReturn sync(void) = 0;
	virtual IOReturn flush(const uuid_t guid, IONVRAMOperation op) = 0;
	virtual void     reload(void) = 0;
	virtual uint32_t getGeneration(void) const = 0;
	virtual uint32_t getVersion(void) const = 0;
	virtual uint32_t getSystemUsed(void) const = 0;
	virtual uint32_t getCommonUsed(void) const = 0;
	virtual bool     getSystemPartitionActive(void) const = 0;
};

IODTNVRAMFormatHandler::~IODTNVRAMFormatHandler()
{
}

bool
IODTNVRAMFormatHandler::getNVRAMProperties()
{
	bool                         ok    = false;
	OSSharedPtr<IORegistryEntry> entry;
	OSSharedPtr<OSObject>        prop;
	OSData *                     data;

	entry = IORegistryEntry::fromPath("/chosen", gIODTPlane);
	require_action(entry, exit, DEBUG_ERROR("Unable to find chosen node\n"));

	prop = entry->copyProperty(kNVRAMBankSizeKey);
	require_action(prop, exit, DEBUG_ERROR("Unable to find %s property\n", kNVRAMBankSizeKey));

	data = OSDynamicCast(OSData, prop.get());
	require(data, exit);

	_bankSize = *((uint32_t *)data->getBytesNoCopy());

	prop = entry->copyProperty(kNVRAMBankCountKey);
	require_action(prop, exit, DEBUG_ERROR("Unable to find %s property\n", kNVRAMBankCountKey));

	data = OSDynamicCast(OSData, prop.get());
	require(data, exit);

	_bankCount = *((uint32_t *)data->getBytesNoCopy());

	prop = entry->copyProperty(kNVRAMCurrentBankKey);
	require_action(prop, exit, DEBUG_ERROR("Unable to find %s property\n", kNVRAMCurrentBankKey));

	data = OSDynamicCast(OSData, prop.get());
	require(data, exit);

	_currentBank = *((uint32_t *)data->getBytesNoCopy());

	ok = true;

	DEBUG_ALWAYS("_bankSize=%#X, _bankCount=%#X, _currentBank=%#X\n", _bankSize, _bankCount, _currentBank);

exit:
	return ok;
}

#include "IONVRAMCHRPHandler.cpp"

#include "IONVRAMV3Handler.cpp"

// **************************** IODTNVRAM *********************************

bool
IODTNVRAM::init(IORegistryEntry *old, const IORegistryPlane *plane)
{
	OSSharedPtr<OSDictionary> dict;

	DEBUG_INFO("...\n");

	require(super::init(old, plane), fail);

#if CONFIG_CSR && XNU_TARGET_OS_OSX
	gInternalBuild = (csr_check(CSR_ALLOW_APPLE_INTERNAL) == 0);
#elif defined(DEBUG) || defined(DEVELOPMENT)
	gInternalBuild = true;
#endif
	DEBUG_INFO("gInternalBuild = %d\n", gInternalBuild);

	_variableLock = IORWLockAlloc();
	require(_variableLock != nullptr, fail);

	_controllerLock = IOLockAlloc();
	require(_controllerLock != nullptr, fail);

	// Clear the IORegistryEntry property table
	dict =  OSDictionary::withCapacity(1);
	require(dict != nullptr, fail);

	setPropertyTable(dict.get());
	dict.reset();

	return true;

fail:
	return false;
}

bool
IODTNVRAM::start(IOService *provider)
{
	OSSharedPtr<OSNumber> version;

	DEBUG_INFO("...\n");

	require(super::start(provider), fail);

	// Check if our overridden init function was called
	// If not, skip any additional initialization being done here.
	// This is not an error we just need to successfully exit this function to allow
	// AppleEFIRuntime to proceed and take over operation
	require_action(_controllerLock != nullptr, no_common, DEBUG_INFO("x86 init\n"));

	_diags = new IODTNVRAMDiags;
	if (!_diags || !_diags->init()) {
		DEBUG_ERROR("Unable to create/init the diags service\n");
		OSSafeReleaseNULL(_diags);
		goto fail;
	}

	if (!_diags->attach(this)) {
		DEBUG_ERROR("Unable to attach the diags service!\n");
		OSSafeReleaseNULL(_diags);
		goto fail;
	}

	if (!_diags->start(this)) {
		DEBUG_ERROR("Unable to start the diags service!\n");
		_diags->detach(this);
		OSSafeReleaseNULL(_diags);
		goto fail;
	}

	_notifier = new IODTNVRAMPlatformNotifier;
	if (!_notifier || !_notifier->init()) {
		DEBUG_ERROR("Unable to create/init the notifier service\n");
		OSSafeReleaseNULL(_notifier);
		goto fail;
	}

	if (!_notifier->attach(this)) {
		DEBUG_ERROR("Unable to attach the notifier service!\n");
		OSSafeReleaseNULL(_notifier);
		goto fail;
	}

	if (!_notifier->start(this)) {
		DEBUG_ERROR("Unable to start the notifier service!\n");
		_notifier->detach(this);
		OSSafeReleaseNULL(_notifier);
		goto fail;
	}

	// This will load the proxied variable data which will call back into
	// IODTNVRAM for the variable sets which will also update the system/common services
	initImageFormat();

	version = OSNumber::withNumber(_format->getVersion(), 32);
	_diags->setProperty(kCurrentNVRAMVersionKey, version.get());

	if (_format->getSystemUsed()) {
		_systemService = new IODTNVRAMVariables;

		if (!_systemService || !_systemService->init(gAppleSystemVariableGuid, _format->getSystemPartitionActive())) {
			DEBUG_ERROR("Unable to start the system service!\n");
			OSSafeReleaseNULL(_systemService);
			goto no_system;
		}

		_systemService->setName("options-system");

		if (!_systemService->attach(this)) {
			DEBUG_ERROR("Unable to attach the system service!\n");
			OSSafeReleaseNULL(_systemService);
			goto no_system;
		}

		if (!_systemService->start(this)) {
			DEBUG_ERROR("Unable to start the system service!\n");
			_systemService->detach(this);
			OSSafeReleaseNULL(_systemService);
			goto no_system;
		}
	}

no_system:
	_commonService = new IODTNVRAMVariables;

	if (!_commonService || !_commonService->init(gAppleNVRAMGuid, _format->getSystemPartitionActive())) {
		DEBUG_ERROR("Unable to start the common service!\n");
		OSSafeReleaseNULL(_commonService);
		goto no_common;
	}

	_commonService->setName("options-common");

	if (!_commonService->attach(this)) {
		DEBUG_ERROR("Unable to attach the common service!\n");
		OSSafeReleaseNULL(_commonService);
		goto no_common;
	}

	if (!_commonService->start(this)) {
		DEBUG_ERROR("Unable to start the common service!\n");
		_commonService->detach(this);
		OSSafeReleaseNULL(_commonService);
		goto no_common;
	}

no_common:
	return true;

fail:
	stop(provider);
	return false;
}

void
IODTNVRAM::initImageFormat(void)
{
	OSSharedPtr<IORegistryEntry> entry;
	OSSharedPtr<OSObject>        prop;
	const char                   *proxyDataKey = "nvram-proxy-data";
	const char                   *bankSizeKey = "nvram-bank-size";
	OSData                       *data = nullptr;
	uint32_t                     size = 0;
	const uint8_t                *image = nullptr;

	entry = IORegistryEntry::fromPath("/chosen", gIODTPlane);

	require(entry != nullptr, skip);

	prop = entry->copyProperty(bankSizeKey);
	require(prop != nullptr, skip);

	data = OSDynamicCast(OSData, prop.get());
	require(data != nullptr, skip);

	size = *((uint32_t*)data->getBytesNoCopy());
	require_action(size != 0, skip, panic("NVRAM size is 0 bytes, possibly due to bad config with iBoot + xnu mismatch"));
	DEBUG_ALWAYS("NVRAM size is %u bytes\n", size);

	prop = entry->copyProperty(proxyDataKey);
	require(prop != nullptr, skip);

	data = OSDynamicCast(OSData, prop.get());
	require_action(data != nullptr, skip, DEBUG_ERROR("No proxy data!\n"));

	image = (const uint8_t *)data->getBytesNoCopy();

skip:
	if (IONVRAMV3Handler::isValidImage(image, size)) {
		_format = IONVRAMV3Handler::init(this, image, size, _varDict);
		require_action(_format, skip, panic("IONVRAMV3Handler creation failed\n"));
	} else {
		_format = IONVRAMCHRPHandler::init(this, image, size, _varDict);
		require_action(_format, skip, panic("IONVRAMCHRPHandler creation failed\n"));
	}

	_format->unserializeVariables();

	dumpDict(_varDict.get());

#if defined(RELEASE)
	if (entry != nullptr) {
		entry->removeProperty(proxyDataKey);
	}
#endif

	_lastDeviceSync = 0;
	_freshInterval = true;
}

void
IODTNVRAM::registerNVRAMController(IONVRAMController *controller)
{
	DEBUG_INFO("setting controller\n");

	NVRAMWRITELOCK();
	CONTROLLERLOCK();

	_format->setController(controller);

	CONTROLLERUNLOCK();
	NVRAMUNLOCK();

	return;
}

bool
IODTNVRAM::safeToSync(void)
{
	AbsoluteTime delta;
	UInt64       delta_ns;
	SInt32       delta_secs;

	// delta interval went by
	clock_get_uptime(&delta);

	// Figure it in seconds.
	absolutetime_to_nanoseconds(delta, &delta_ns);
	delta_secs = (SInt32)(delta_ns / NSEC_PER_SEC);

	if ((delta_secs > (_lastDeviceSync + MIN_SYNC_NOW_INTERVAL)) || _freshInterval) {
		_lastDeviceSync = delta_secs;
		_freshInterval = false;
		return true;
	}

	return false;
}

IOReturn
IODTNVRAM::syncInternal(bool rateLimit)
{
	IOReturn ret = kIOReturnSuccess;

	DEBUG_INFO("rateLimit=%d\n", rateLimit);

	if (!SAFE_TO_LOCK()) {
		DEBUG_INFO("cannot lock\n");
		goto exit;
	}

	// Rate limit requests to sync. Drivers that need this rate limiting will
	// shadow the data and only write to flash when they get a sync call
	if (rateLimit) {
		if (safeToSync() == false) {
			DEBUG_INFO("safeToSync()=false\n");
			goto exit;
		}
	}

	DEBUG_INFO("Calling sync()\n");
	record_system_event(SYSTEM_EVENT_TYPE_INFO, SYSTEM_EVENT_SUBSYSTEM_NVRAM, "sync", "triggered");

	NVRAMREADLOCK();
	CONTROLLERLOCK();

	ret = _format->sync();

	CONTROLLERUNLOCK();
	NVRAMUNLOCK();

	record_system_event(SYSTEM_EVENT_TYPE_INFO, SYSTEM_EVENT_SUBSYSTEM_NVRAM, "sync", "completed with ret=%08x", ret);

	if (_diags) {
		OSSharedPtr<OSNumber> generation = OSNumber::withNumber(_format->getGeneration(), 32);
		_diags->setProperty(kCurrentGenerationCountKey, generation.get());
	}

exit:
	return ret;
}

IOReturn
IODTNVRAM::sync(void)
{
	return syncInternal(false);
}

void
IODTNVRAM::reload(void)
{
	_format->reload();
}

OSPtr<OSDictionary>
IODTNVRAM::dictionaryWithProperties(void) const
{
	const OSSymbol                    *canonicalKey;
	OSSharedPtr<OSDictionary>         localVarDict, returnDict;
	OSSharedPtr<OSCollectionIterator> iter;
	unsigned int                      totalCapacity = 0;
	uuid_t                            varGuid;
	const char *                      varName;

	NVRAMREADLOCK();
	if (_varDict) {
		localVarDict = OSDictionary::withDictionary(_varDict.get());
	}
	NVRAMUNLOCK();

	if (localVarDict != nullptr) {
		totalCapacity =  localVarDict->getCapacity();
	}

	returnDict = OSDictionary::withCapacity(totalCapacity);

	if (returnDict == nullptr) {
		DEBUG_ERROR("No dictionary\n");
		goto exit;
	}

	// Copy system entries first if present then copy unique other entries
	iter = OSCollectionIterator::withCollection(localVarDict.get());
	if (iter == nullptr) {
		DEBUG_ERROR("failed to create iterator\n");
		goto exit;
	}

	while ((canonicalKey = OSDynamicCast(OSSymbol, iter->getNextObject()))) {
		parseVariableName(canonicalKey, &varGuid, &varName);

		if ((uuid_compare(varGuid, gAppleSystemVariableGuid) == 0) &&
		    verifyPermission(kIONVRAMOperationRead, varGuid, varName, _format->getSystemPartitionActive())) {
			OSSharedPtr<const OSSymbol> returnKey = OSSymbol::withCString(varName);
			returnDict->setObject(returnKey.get(), localVarDict->getObject(canonicalKey));
		}
	}

	iter.reset();

	iter = OSCollectionIterator::withCollection(localVarDict.get());
	if (iter == nullptr) {
		DEBUG_ERROR("failed to create iterator\n");
		goto exit;
	}

	while ((canonicalKey = OSDynamicCast(OSSymbol, iter->getNextObject()))) {
		parseVariableName(canonicalKey, &varGuid, &varName);

		if (uuid_compare(varGuid, gAppleNVRAMGuid) == 0) {
			if (returnDict->getObject(varName) != nullptr) {
				// Skip non uniques
				continue;
			}

			if (verifyPermission(kIONVRAMOperationRead, varGuid, varName, _format->getSystemPartitionActive())) {
				OSSharedPtr<const OSSymbol> returnKey = OSSymbol::withCString(varName);
				returnDict->setObject(returnKey.get(), localVarDict->getObject(canonicalKey));
			}
		}
	}
exit:
	return returnDict;
}

bool
IODTNVRAM::serializeProperties(OSSerialize *s) const
{
	bool ok = false;
	OSSharedPtr<OSDictionary> dict;

	dict = dictionaryWithProperties();
	if (dict) {
		ok = dict->serialize(s);
	}

	DEBUG_INFO("ok=%d\n", ok);
	return ok;
}

IOReturn
IODTNVRAM::flushGUID(const uuid_t guid, IONVRAMOperation op)
{
	IOReturn ret = kIOReturnSuccess;

	if (_format->getSystemPartitionActive() && (uuid_compare(guid, gAppleSystemVariableGuid) == 0)) {
		ret = _format->flush(guid, op);

		DEBUG_INFO("system variables flushed, ret=%08x\n", ret);
	} else if (uuid_compare(guid, gAppleNVRAMGuid) == 0) {
		ret = _format->flush(guid, op);

		DEBUG_INFO("common variables flushed, ret=%08x\n", ret);
	}

	return ret;
}

bool
IODTNVRAM::handleSpecialVariables(const char *name, const uuid_t guid, const OSObject *obj, IOReturn *error)
{
	IOReturn ret = kIOReturnSuccess;
	bool special = false;

	NVRAMLOCKASSERTEXCLUSIVE();

	// ResetNVRam flushes both regions in one call
	// Obliterate can flush either separately
	if (strcmp(name, "ObliterateNVRam") == 0) {
		special = true;
		ret = flushGUID(guid, kIONVRAMOperationObliterate);
	} else if (strcmp(name, "ResetNVRam") == 0) {
		special = true;
		ret = flushGUID(gAppleSystemVariableGuid, kIONVRAMOperationReset);

		if (ret != kIOReturnSuccess) {
			goto exit;
		}

		ret = flushGUID(gAppleNVRAMGuid, kIONVRAMOperationReset);
	}

exit:
	if (error) {
		*error = ret;
	}

	return special;
}

OSSharedPtr<OSObject>
IODTNVRAM::copyPropertyWithGUIDAndName(const uuid_t guid, const char *name) const
{
	OSSharedPtr<const OSSymbol> canonicalKey;
	OSSharedPtr<OSObject>       theObject;
	uuid_t                      newGuid;

	if (_varDict == nullptr) {
		DEBUG_INFO("No dictionary\n");
		goto exit;
	}

	if (!verifyPermission(kIONVRAMOperationRead, guid, name, _format->getSystemPartitionActive())) {
		DEBUG_INFO("Not privileged\n");
		goto exit;
	}

	translateGUID(guid, name, newGuid, _format->getSystemPartitionActive());

	canonicalKey = keyWithGuidAndCString(newGuid, name);

	NVRAMREADLOCK();
	theObject.reset(_varDict->getObject(canonicalKey.get()), OSRetain);
	NVRAMUNLOCK();

	if (_diags) {
		_diags->logVariable(getPartitionTypeForGUID(newGuid), kIONVRAMOperationRead, name, NULL);
	}

	if (theObject != nullptr) {
		DEBUG_INFO("%s has object\n", canonicalKey.get()->getCStringNoCopy());
	} else {
		DEBUG_INFO("%s no entry\n", canonicalKey.get()->getCStringNoCopy());
	}

exit:
	return theObject;
}

OSSharedPtr<OSObject>
IODTNVRAM::copyProperty(const OSSymbol *aKey) const
{
	const char            *variableName;
	uuid_t                varGuid;

	if (skipKey(aKey)) {
		return nullptr;
	}
	DEBUG_INFO("aKey=%s\n", aKey->getCStringNoCopy());

	parseVariableName(aKey->getCStringNoCopy(), &varGuid, &variableName);

	return copyPropertyWithGUIDAndName(varGuid, variableName);
}

OSSharedPtr<OSObject>
IODTNVRAM::copyProperty(const char *aKey) const
{
	OSSharedPtr<const OSSymbol> keySymbol;
	OSSharedPtr<OSObject>       theObject;

	keySymbol = OSSymbol::withCString(aKey);
	if (keySymbol != nullptr) {
		theObject = copyProperty(keySymbol.get());
	}

	return theObject;
}

OSObject *
IODTNVRAM::getProperty(const OSSymbol *aKey) const
{
	// The shared pointer gets released at the end of the function,
	// and returns a view into theObject.
	OSSharedPtr<OSObject> theObject = copyProperty(aKey);

	return theObject.get();
}

OSObject *
IODTNVRAM::getProperty(const char *aKey) const
{
	// The shared pointer gets released at the end of the function,
	// and returns a view into theObject.
	OSSharedPtr<OSObject> theObject = copyProperty(aKey);

	return theObject.get();
}

IOReturn
IODTNVRAM::setPropertyWithGUIDAndName(const uuid_t guid, const char *name, OSObject *anObject)
{
	IOReturn              ret = kIOReturnSuccess;
	bool                  remove = false;
	OSString              *tmpString = nullptr;
	OSSharedPtr<OSObject> propObject;
	OSSharedPtr<OSObject> sharedObject(anObject, OSRetain);
	bool                  deletePropertyKey, syncNowPropertyKey, forceSyncNowPropertyKey, deletePropertyKeyWRet;
	bool                  ok;
	size_t                propDataSize = 0;
	uuid_t                newGuid;

	deletePropertyKey = strncmp(name, kIONVRAMDeletePropertyKey, sizeof(kIONVRAMDeletePropertyKey)) == 0;
	deletePropertyKeyWRet = strncmp(name, kIONVRAMDeletePropertyKeyWRet, sizeof(kIONVRAMDeletePropertyKeyWRet)) == 0;
	syncNowPropertyKey = strncmp(name, kIONVRAMSyncNowPropertyKey, sizeof(kIONVRAMSyncNowPropertyKey)) == 0;
	forceSyncNowPropertyKey = strncmp(name, kIONVRAMForceSyncNowPropertyKey, sizeof(kIONVRAMForceSyncNowPropertyKey)) == 0;

	if (deletePropertyKey || deletePropertyKeyWRet) {
		tmpString = OSDynamicCast(OSString, anObject);
		if (tmpString != nullptr) {
			const char *variableName;
			uuid_t     valueVarGuid;
			bool       guidProvided;
			IOReturn   removeRet;

			guidProvided = parseVariableName(tmpString->getCStringNoCopy(), &valueVarGuid, &variableName);

			// nvram tool will provide a "nvram -d var" or "nvram -d guid:var" as
			// kIONVRAMDeletePropertyKey=var or kIONVRAMDeletePropertyKey=guid:var
			// that will come into this function as (gAppleNVRAMGuid, varname, nullptr)
			// if we provide the "-z" flag to the nvram tool this function will come in as
			// (gAppleSystemVariableGuid, varname, nullptr). We are reparsing the value string,
			// if there is a GUID provided with the value then use that GUID otherwise use the
			// guid that was provided via the node selection or default.
			if (guidProvided == false) {
				DEBUG_INFO("Removing with API provided GUID\n");
				removeRet = removePropertyWithGUIDAndName(guid, variableName);
			} else {
				DEBUG_INFO("Removing with value provided GUID\n");
				removeRet = removePropertyWithGUIDAndName(valueVarGuid, variableName);
			}
			if (deletePropertyKeyWRet) {
				ret = removeRet;
			}
			DEBUG_INFO("%s found, removeRet=%#08x\n", deletePropertyKeyWRet ? "deletePropertyKeyWRet" : "deletePropertyKey", removeRet);
		} else {
			DEBUG_INFO("%s value needs to be an OSString\n", deletePropertyKeyWRet ? "deletePropertyKeyWRet" : "deletePropertyKey");
			ret = kIOReturnError;
		}
		goto exit;
	} else if (syncNowPropertyKey || forceSyncNowPropertyKey) {
		tmpString = OSDynamicCast(OSString, anObject);
		DEBUG_INFO("NVRAM sync key %s found\n", name);
		if (tmpString != nullptr) {
			// We still want to throttle NVRAM commit rate for SyncNow. ForceSyncNow is provided as a really big hammer.
			ret = syncInternal(syncNowPropertyKey);
		} else {
			DEBUG_INFO("%s value needs to be an OSString\n", name);
			ret = kIOReturnError;
		}
		goto exit;
	}

	if (!verifyPermission(kIONVRAMOperationWrite, guid, name, _format->getSystemPartitionActive())) {
		DEBUG_INFO("Not privileged\n");
		ret = kIOReturnNotPrivileged;
		goto exit;
	}

	// Make sure the object is of the correct type.
	switch (getVariableType(name)) {
	case kOFVariableTypeBoolean:
		propObject = OSDynamicPtrCast<OSBoolean>(sharedObject);
		if (propObject) {
			record_system_event(SYSTEM_EVENT_TYPE_INFO, SYSTEM_EVENT_SUBSYSTEM_NVRAM, "write", "%s as bool to %d", name, ((OSBoolean *)propObject.get())->getValue());
		}
		break;

	case kOFVariableTypeNumber:
		propObject = OSDynamicPtrCast<OSNumber>(sharedObject);
		if (propObject) {
			record_system_event(SYSTEM_EVENT_TYPE_INFO, SYSTEM_EVENT_SUBSYSTEM_NVRAM, "write", "%s as number to %#llx", name, ((OSNumber *)propObject.get())->unsigned64BitValue());
		}
		break;

	case kOFVariableTypeString:
		propObject = OSDynamicPtrCast<OSString>(sharedObject);
		if (propObject != nullptr) {
			record_system_event(SYSTEM_EVENT_TYPE_INFO, SYSTEM_EVENT_SUBSYSTEM_NVRAM, "write", "%s as string to %s", name, ((OSString *)propObject.get())->getCStringNoCopy());

			propDataSize = (OSDynamicPtrCast<OSString>(propObject))->getLength();

			if ((strncmp(name, kIONVRAMBootArgsKey, sizeof(kIONVRAMBootArgsKey)) == 0) && (propDataSize >= BOOT_LINE_LENGTH)) {
				DEBUG_ERROR("boot-args size too large for BOOT_LINE_LENGTH, propDataSize=%zu\n", propDataSize);
				ret = kIOReturnNoSpace;
				goto exit;
			}
		}
		break;

	case kOFVariableTypeData:
		propObject = OSDynamicPtrCast<OSData>(sharedObject);
		if (propObject == nullptr) {
			tmpString = OSDynamicCast(OSString, sharedObject.get());
			if (tmpString != nullptr) {
				propObject = OSData::withBytes(tmpString->getCStringNoCopy(),
				    tmpString->getLength());
			}
		}

		if (propObject != nullptr) {
			propDataSize = (OSDynamicPtrCast<OSData>(propObject))->getLength();
			record_system_event(SYSTEM_EVENT_TYPE_INFO, SYSTEM_EVENT_SUBSYSTEM_NVRAM, "write", "%s as data with size %#x", name, ((OSData *)propObject.get())->getLength());
		}

#if defined(XNU_TARGET_OS_OSX)
		if ((propObject != nullptr) && ((OSDynamicPtrCast<OSData>(propObject))->getLength() == 0)) {
			remove = true;
		}
#endif /* defined(XNU_TARGET_OS_OSX) */
		break;
	default:
		break;
	}

	if (propObject == nullptr) {
		DEBUG_ERROR("No property object\n");
		ret = kIOReturnBadArgument;
		goto exit;
	}

	if (!verifyWriteSizeLimit(guid, name, propDataSize)) {
		DEBUG_ERROR("Property data size of %zu too long for %s\n", propDataSize, name);
		ret = kIOReturnNoSpace;
		goto exit;
	}

	NVRAMWRITELOCK();
	ok = handleSpecialVariables(name, guid, propObject.get(), &ret);
	NVRAMUNLOCK();

	if (ok) {
		goto exit;
	}

	if (remove == false) {
		DEBUG_INFO("Adding object\n");

		translateGUID(guid, name, newGuid, _format->getSystemPartitionActive());

		NVRAMWRITELOCK();

		ret = _format->setVariable(newGuid, name, propObject.get());

		NVRAMUNLOCK();
	} else {
		DEBUG_INFO("Removing object\n");
		ret = removePropertyWithGUIDAndName(guid, name);
	}

	if (tmpString) {
		propObject.reset();
	}

exit:
	DEBUG_INFO("ret=%#08x\n", ret);

	return ret;
}

IOReturn
IODTNVRAM::setPropertyInternal(const OSSymbol *aKey, OSObject *anObject)
{
	const char *variableName;
	uuid_t     varGuid;

	DEBUG_INFO("aKey=%s\n", aKey->getCStringNoCopy());

	parseVariableName(aKey->getCStringNoCopy(), &varGuid, &variableName);

	return setPropertyWithGUIDAndName(varGuid, variableName, anObject);
}

bool
IODTNVRAM::setProperty(const OSSymbol *aKey, OSObject *anObject)
{
	return setPropertyInternal(aKey, anObject) == kIOReturnSuccess;
}

void
IODTNVRAM::removeProperty(const OSSymbol *aKey)
{
	IOReturn ret;

	ret = removePropertyInternal(aKey);

	if (ret != kIOReturnSuccess) {
		DEBUG_INFO("removePropertyInternal failed, ret=%#08x\n", ret);
	}
}

IOReturn
IODTNVRAM::removePropertyWithGUIDAndName(const uuid_t guid, const char *name)
{
	IOReturn ret;
	uuid_t   newGuid;

	DEBUG_INFO("name=%s\n", name);

	if (_varDict == nullptr) {
		DEBUG_INFO("No dictionary\n");
		ret = kIOReturnNotFound;
		goto exit;
	}

	if (!verifyPermission(kIONVRAMOperationDelete, guid, name, _format->getSystemPartitionActive())) {
		DEBUG_INFO("Not privileged\n");
		ret = kIOReturnNotPrivileged;
		goto exit;
	}

	translateGUID(guid, name, newGuid, _format->getSystemPartitionActive());

	NVRAMWRITELOCK();

	ret = _format->setVariable(newGuid, name, nullptr);

	if (ret != kIOReturnSuccess) {
		DEBUG_INFO("%s not found\n", name);
		ret = kIOReturnNotFound;
	}

	NVRAMUNLOCK();

	record_system_event(SYSTEM_EVENT_TYPE_INFO, SYSTEM_EVENT_SUBSYSTEM_NVRAM, "delete", "%s", name);

exit:
	return ret;
}

IOReturn
IODTNVRAM::removePropertyInternal(const OSSymbol *aKey)
{
	IOReturn   ret;
	const char *variableName;
	uuid_t     varGuid;

	DEBUG_INFO("aKey=%s\n", aKey->getCStringNoCopy());

	parseVariableName(aKey->getCStringNoCopy(), &varGuid, &variableName);

	ret = removePropertyWithGUIDAndName(varGuid, variableName);

	return ret;
}

IOReturn
IODTNVRAM::setProperties(OSObject *properties)
{
	IOReturn                          ret = kIOReturnSuccess;
	OSObject                          *object;
	const OSSymbol                    *key;
	OSDictionary                      *dict;
	OSSharedPtr<OSCollectionIterator> iter;

	dict = OSDynamicCast(OSDictionary, properties);
	if (dict == nullptr) {
		DEBUG_ERROR("Not a dictionary\n");
		return kIOReturnBadArgument;
	}

	iter = OSCollectionIterator::withCollection(dict);
	if (iter == nullptr) {
		DEBUG_ERROR("Couldn't create iterator\n");
		return kIOReturnBadArgument;
	}

	while (ret == kIOReturnSuccess) {
		key = OSDynamicCast(OSSymbol, iter->getNextObject());
		if (key == nullptr) {
			break;
		}

		object = dict->getObject(key);
		if (object == nullptr) {
			continue;
		}

		ret = setPropertyInternal(key, object);
	}

	DEBUG_INFO("ret=%#08x\n", ret);

	return ret;
}

// ********************** Deprecated ********************

IOReturn
IODTNVRAM::readXPRAM(IOByteCount offset, uint8_t *buffer,
    IOByteCount length)
{
	return kIOReturnUnsupported;
}

IOReturn
IODTNVRAM::writeXPRAM(IOByteCount offset, uint8_t *buffer,
    IOByteCount length)
{
	return kIOReturnUnsupported;
}

IOReturn
IODTNVRAM::readNVRAMProperty(IORegistryEntry *entry,
    const OSSymbol **name,
    OSData **value)
{
	return kIOReturnUnsupported;
}

IOReturn
IODTNVRAM::writeNVRAMProperty(IORegistryEntry *entry,
    const OSSymbol *name,
    OSData *value)
{
	return kIOReturnUnsupported;
}

OSDictionary *
IODTNVRAM::getNVRAMPartitions(void)
{
	return NULL;
}

IOReturn
IODTNVRAM::readNVRAMPartition(const OSSymbol *partitionID,
    IOByteCount offset, uint8_t *buffer,
    IOByteCount length)
{
	return kIOReturnUnsupported;
}

IOReturn
IODTNVRAM::writeNVRAMPartition(const OSSymbol *partitionID,
    IOByteCount offset, uint8_t *buffer,
    IOByteCount length)
{
	return kIOReturnUnsupported;
}

IOByteCount
IODTNVRAM::savePanicInfo(uint8_t *buffer, IOByteCount length)
{
	return 0;
}
