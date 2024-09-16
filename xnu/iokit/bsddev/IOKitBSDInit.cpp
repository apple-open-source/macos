/*
 * Copyright (c) 1998-2021 Apple Inc. All rights reserved.
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
#include <IOKit/IOBSD.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOCatalogue.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IONVRAM.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOUserClient.h>
#include <libkern/c++/OSAllocation.h>

extern "C" {
#include <libkern/amfi/amfi.h>
#include <sys/codesign.h>
#include <sys/code_signing.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <pexpert/pexpert.h>
#include <kern/clock.h>
#if CONFIG_KDP_INTERACTIVE_DEBUGGING
#include <kern/debug.h>
#endif
#include <mach/machine.h>
#include <uuid/uuid.h>
#include <sys/vnode_internal.h>
#include <sys/mount.h>
#include <corecrypto/ccsha2.h>
#include <kdp/sk_core.h>
#include <pexpert/device_tree.h>
#include <kern/startup.h>

// how long to wait for matching root device, secs
#if DEBUG
#define ROOTDEVICETIMEOUT       120
#else
#define ROOTDEVICETIMEOUT       60
#endif

extern dev_t mdevadd(int devid, uint64_t base, unsigned int size, int phys);
extern dev_t mdevlookup(int devid);
extern void mdevremoveall(void);
extern int mdevgetrange(int devid, uint64_t *base, uint64_t *size);
extern void di_root_ramfile(IORegistryEntry * entry);
extern int IODTGetDefault(const char *key, void *infoAddr, unsigned int infoSize);
extern boolean_t cpuid_vmm_present(void);

#define ROUNDUP(a, b) (((a) + ((b) - 1)) & (~((b) - 1)))

#define IOPOLLED_COREFILE       (CONFIG_KDP_INTERACTIVE_DEBUGGING)

#if defined(XNU_TARGET_OS_BRIDGE)
#define kIOCoreDumpPath         "/private/var/internal/kernelcore"
#elif defined(XNU_TARGET_OS_OSX)
#define kIOCoreDumpPath         "/System/Volumes/VM/kernelcore"
#else
#define kIOCoreDumpPath         "/private/var/vm/kernelcore"
#endif

#define kIOCoreDumpPrebootPath      "/private/preboot/kernelcore"

#define SYSTEM_NVRAM_PREFIX     "40A0DDD2-77F8-4392-B4A3-1E7304206516:"

#if CONFIG_KDP_INTERACTIVE_DEBUGGING
/*
 * Touched by IOFindBSDRoot() if a RAMDisk is used for the root device.
 */
extern uint64_t kdp_core_ramdisk_addr;
extern uint64_t kdp_core_ramdisk_size;

/*
 * A callback to indicate that the polled-mode corefile is now available.
 */
extern kern_return_t kdp_core_polled_io_polled_file_available(IOCoreFileAccessCallback access_data, void *access_context, void *recipient_context);

/*
 * A callback to indicate that the polled-mode corefile is no longer available.
 */
extern kern_return_t kdp_core_polled_io_polled_file_unavailable(void);
#endif

#if IOPOLLED_COREFILE
static void IOOpenPolledCoreFile(thread_call_param_t __unused, thread_call_param_t corefilename);
static void IOResolveCoreFilePath();

thread_call_t corefile_open_call = NULL;
SECURITY_READ_ONLY_LATE(const char*) kdp_corefile_path = kIOCoreDumpPath;
#endif

kern_return_t
IOKitBSDInit( void )
{
	IOService::publishResource("IOBSD");

#if IOPOLLED_COREFILE
	corefile_open_call = thread_call_allocate_with_options(IOOpenPolledCoreFile, NULL, THREAD_CALL_PRIORITY_KERNEL, THREAD_CALL_OPTIONS_ONCE);
#endif

	return kIOReturnSuccess;
}

void
IOServicePublishResource( const char * property, boolean_t value )
{
	if (value) {
		IOService::publishResource( property, kOSBooleanTrue );
	} else {
		IOService::getResourceService()->removeProperty( property );
	}
}

boolean_t
IOServiceWaitForMatchingResource( const char * property, uint64_t timeout )
{
	OSDictionary *      dict = NULL;
	IOService *         match = NULL;
	boolean_t           found = false;

	do {
		dict = IOService::resourceMatching( property );
		if (!dict) {
			continue;
		}
		match = IOService::waitForMatchingService( dict, timeout );
		if (match) {
			found = true;
		}
	} while (false);

	if (dict) {
		dict->release();
	}
	if (match) {
		match->release();
	}

	return found;
}

boolean_t
IOCatalogueMatchingDriversPresent( const char * property )
{
	OSDictionary *      dict = NULL;
	OSOrderedSet *      set = NULL;
	SInt32              generationCount = 0;
	boolean_t           found = false;

	do {
		dict = OSDictionary::withCapacity(1);
		if (!dict) {
			continue;
		}
		dict->setObject( property, kOSBooleanTrue );
		set = gIOCatalogue->findDrivers( dict, &generationCount );
		if (set && (set->getCount() > 0)) {
			found = true;
		}
	} while (false);

	if (dict) {
		dict->release();
	}
	if (set) {
		set->release();
	}

	return found;
}

OSDictionary *
IOBSDNameMatching( const char * name )
{
	OSDictionary *      dict;
	const OSSymbol *    str = NULL;

	do {
		dict = IOService::serviceMatching( gIOServiceKey );
		if (!dict) {
			continue;
		}
		str = OSSymbol::withCString( name );
		if (!str) {
			continue;
		}
		dict->setObject( kIOBSDNameKey, (OSObject *) str );
		str->release();

		return dict;
	} while (false);

	if (dict) {
		dict->release();
	}
	if (str) {
		str->release();
	}

	return NULL;
}

OSDictionary *
IOUUIDMatching( void )
{
	OSObject     * obj;
	OSDictionary * result;

	obj = OSUnserialize(
		"{"
		"'IOProviderClass' = 'IOResources';"
		"'IOResourceMatch' = ('IOBSD', 'boot-uuid-media');"
		"}",
		NULL);
	result = OSDynamicCast(OSDictionary, obj);
	assert(result);

	return result;
}

OSDictionary *
IONetworkNamePrefixMatching( const char * prefix )
{
	OSDictionary *       matching;
	OSDictionary *   propDict = NULL;
	const OSSymbol * str      = NULL;
	char networkType[128];

	do {
		matching = IOService::serviceMatching( "IONetworkInterface" );
		if (matching == NULL) {
			continue;
		}

		propDict = OSDictionary::withCapacity(1);
		if (propDict == NULL) {
			continue;
		}

		str = OSSymbol::withCString( prefix );
		if (str == NULL) {
			continue;
		}

		propDict->setObject( "IOInterfaceNamePrefix", (OSObject *) str );
		str->release();
		str = NULL;

		// see if we're contrained to netroot off of specific network type
		if (PE_parse_boot_argn( "network-type", networkType, 128 )) {
			str = OSSymbol::withCString( networkType );
			if (str) {
				propDict->setObject( "IONetworkRootType", str);
				str->release();
				str = NULL;
			}
		}

		if (matching->setObject( gIOPropertyMatchKey,
		    (OSObject *) propDict ) != true) {
			continue;
		}

		propDict->release();
		propDict = NULL;

		return matching;
	} while (false);

	if (matching) {
		matching->release();
	}
	if (propDict) {
		propDict->release();
	}
	if (str) {
		str->release();
	}

	return NULL;
}

static bool
IORegisterNetworkInterface( IOService * netif )
{
	// A network interface is typically named and registered
	// with BSD after receiving a request from a user space
	// "namer". However, for cases when the system needs to
	// root from the network, this registration task must be
	// done inside the kernel and completed before the root
	// device is handed to BSD.

	IOService *    stack;
	OSNumber *     zero    = NULL;
	OSString *     path    = NULL;
	OSDictionary * dict    = NULL;
	OSDataAllocation<char> pathBuf;
	int            len;
	enum { kMaxPathLen = 512 };

	do {
		stack = IOService::waitForService(
			IOService::serviceMatching("IONetworkStack"));
		if (stack == NULL) {
			break;
		}

		dict = OSDictionary::withCapacity(3);
		if (dict == NULL) {
			break;
		}

		zero = OSNumber::withNumber((UInt64) 0, 32);
		if (zero == NULL) {
			break;
		}

		pathBuf = OSDataAllocation<char>( kMaxPathLen, OSAllocateMemory );
		if (!pathBuf) {
			break;
		}

		len = kMaxPathLen;
		if (netif->getPath( pathBuf.data(), &len, gIOServicePlane )
		    == false) {
			break;
		}

		path = OSString::withCStringNoCopy(pathBuf.data());
		if (path == NULL) {
			break;
		}

		dict->setObject( "IOInterfaceUnit", zero );
		dict->setObject( kIOPathMatchKey, path );

		stack->setProperties( dict );
	}while (false);

	if (zero) {
		zero->release();
	}
	if (path) {
		path->release();
	}
	if (dict) {
		dict->release();
	}

	return netif->getProperty( kIOBSDNameKey ) != NULL;
}

OSDictionary *
IOOFPathMatching( const char * path, char * buf, int maxLen )
{
	OSDictionary *      matching = NULL;
	OSString *          str;
	char *              comp;
	int                 len;

	do {
		len = ((int) strlen( kIODeviceTreePlane ":" ));
		maxLen -= len;
		if (maxLen <= 0) {
			continue;
		}

		strlcpy( buf, kIODeviceTreePlane ":", len + 1 );
		comp = buf + len;

		len = ((int) strnlen( path, INT_MAX ));
		maxLen -= len;
		if (maxLen <= 0) {
			continue;
		}
		strlcpy( comp, path, len + 1 );

		matching = OSDictionary::withCapacity( 1 );
		if (!matching) {
			continue;
		}

		str = OSString::withCString( buf );
		if (!str) {
			continue;
		}
		matching->setObject( kIOPathMatchKey, str );
		str->release();

		return matching;
	} while (false);

	if (matching) {
		matching->release();
	}

	return NULL;
}

static int didRam = 0;
enum { kMaxPathBuf = 512, kMaxBootVar = 128 };

bool
IOGetBootUUID(char *uuid)
{
	IORegistryEntry *entry;
	OSData *uuid_data = NULL;
	bool result = false;

	if ((entry = IORegistryEntry::fromPath("/chosen", gIODTPlane))) {
		uuid_data = (OSData *)entry->getProperty("boot-uuid");
		if (uuid_data) {
			unsigned int length = uuid_data->getLength();
			if (length <= sizeof(uuid_string_t)) {
				/* ensure caller's buffer is fully initialized: */
				bzero(uuid, sizeof(uuid_string_t));
				/* copy the content of uuid_data->getBytesNoCopy() into uuid */
				memcpy(uuid, uuid_data->getBytesNoCopy(), length);
				/* guarantee nul-termination: */
				uuid[sizeof(uuid_string_t) - 1] = '\0';
				result = true;
			} else {
				uuid = NULL;
			}
		}
		OSSafeReleaseNULL(entry);
	}
	return result;
}

bool
IOGetApfsPrebootUUID(char *uuid)
{
	IORegistryEntry *entry;
	OSData *uuid_data = NULL;
	bool result = false;

	if ((entry = IORegistryEntry::fromPath("/chosen", gIODTPlane))) {
		uuid_data = (OSData *)entry->getProperty("apfs-preboot-uuid");

		if (uuid_data) {
			unsigned int length = uuid_data->getLength();
			if (length <= sizeof(uuid_string_t)) {
				/* ensure caller's buffer is fully initialized: */
				bzero(uuid, sizeof(uuid_string_t));
				/* copy the content of uuid_data->getBytesNoCopy() into uuid */
				memcpy(uuid, uuid_data->getBytesNoCopy(), length);
				/* guarantee nul-termination: */
				uuid[sizeof(uuid_string_t) - 1] = '\0';
				result = true;
			} else {
				uuid = NULL;
			}
		}
		OSSafeReleaseNULL(entry);
	}
	return result;
}

bool
IOGetAssociatedApfsVolgroupUUID(char *uuid)
{
	IORegistryEntry *entry;
	OSData *uuid_data = NULL;
	bool result = false;

	if ((entry = IORegistryEntry::fromPath("/chosen", gIODTPlane))) {
		uuid_data = (OSData *)entry->getProperty("associated-volume-group");

		if (uuid_data) {
			unsigned int length = uuid_data->getLength();

			if (length <= sizeof(uuid_string_t)) {
				/* ensure caller's buffer is fully initialized: */
				bzero(uuid, sizeof(uuid_string_t));
				/* copy the content of uuid_data->getBytesNoCopy() into uuid */
				memcpy(uuid, uuid_data->getBytesNoCopy(), length);
				/* guarantee nul-termination: */
				uuid[sizeof(uuid_string_t) - 1] = '\0';
				result = true;
			} else {
				uuid = NULL;
			}
		}
		OSSafeReleaseNULL(entry);
	}
	return result;
}

bool
IOGetBootObjectsPath(char *path_prefix)
{
	IORegistryEntry *entry;
	OSData *path_prefix_data = NULL;
	bool result = false;

	if ((entry = IORegistryEntry::fromPath("/chosen", gIODTPlane))) {
		path_prefix_data = (OSData *)entry->getProperty("boot-objects-path");

		if (path_prefix_data) {
			unsigned int length = path_prefix_data->getLength();

			if (length <= MAXPATHLEN) {
				/* ensure caller's buffer is fully initialized: */
				bzero(path_prefix, MAXPATHLEN);
				/* copy the content of path_prefix_data->getBytesNoCopy() into path_prefix */
				memcpy(path_prefix, path_prefix_data->getBytesNoCopy(), length);
				/* guarantee nul-termination: */
				path_prefix[MAXPATHLEN - 1] = '\0';
				result = true;
			} else {
				path_prefix = NULL;
			}
		}
		OSSafeReleaseNULL(entry);
	}
	return result;
}


bool
IOGetBootManifestHash(char *hash_data, size_t *hash_data_size)
{
	IORegistryEntry *entry = NULL;
	OSData *manifest_hash_data = NULL;
	bool result = false;

	if ((entry = IORegistryEntry::fromPath("/chosen", gIODTPlane))) {
		manifest_hash_data = (OSData *)entry->getProperty("boot-manifest-hash");
		if (manifest_hash_data) {
			unsigned int length = manifest_hash_data->getLength();
			/* hashed with SHA2-384 or SHA1, the boot manifest hash should be 48 Bytes or less */
			if ((length <= CCSHA384_OUTPUT_SIZE) && (*hash_data_size >= CCSHA384_OUTPUT_SIZE)) {
				/* ensure caller's buffer is fully initialized: */
				bzero(hash_data, CCSHA384_OUTPUT_SIZE);
				/* copy the content of manifest_hash_data->getBytesNoCopy() into hash_data */
				memcpy(hash_data, manifest_hash_data->getBytesNoCopy(), length);
				*hash_data_size = length;
				result = true;
			} else {
				hash_data = NULL;
				*hash_data_size = 0;
			}
		}
		OSSafeReleaseNULL(entry);
	}

	return result;
}

/*
 * Set NVRAM to boot into the right flavor of Recovery,
 * optionally passing a UUID of a volume that failed to boot.
 * If `reboot` is true, reboot immediately.
 *
 * Returns true if `mode` was understood, false otherwise.
 * (Does not return if `reboot` is true.)
 */
boolean_t
IOSetRecoveryBoot(bsd_bootfail_mode_t mode, uuid_t volume_uuid, boolean_t reboot)
{
	IODTNVRAM *nvram = NULL;
	const OSSymbol *boot_command_sym = NULL;
	OSString *boot_command_recover = NULL;

	if (mode == BSD_BOOTFAIL_SEAL_BROKEN) {
		const char *boot_mode = "ssv-seal-broken";
		uuid_string_t volume_uuid_str;

		// Set `recovery-broken-seal-uuid = <volume_uuid>`.
		if (volume_uuid) {
			uuid_unparse_upper(volume_uuid, volume_uuid_str);

			if (!PEWriteNVRAMProperty(SYSTEM_NVRAM_PREFIX "recovery-broken-seal-uuid",
			    volume_uuid_str, sizeof(uuid_string_t))) {
				IOLog("Failed to write recovery-broken-seal-uuid to NVRAM.\n");
			}
		}

		// Set `recovery-boot-mode = ssv-seal-broken`.
		if (!PEWriteNVRAMProperty(SYSTEM_NVRAM_PREFIX "recovery-boot-mode", boot_mode,
		    (const unsigned int) strlen(boot_mode))) {
			IOLog("Failed to write recovery-boot-mode to NVRAM.\n");
		}
	} else if (mode == BSD_BOOTFAIL_MEDIA_MISSING) {
		const char *boot_picker_reason = "missing-boot-media";

		// Set `boot-picker-bringup-reason = missing-boot-media`.
		if (!PEWriteNVRAMProperty(SYSTEM_NVRAM_PREFIX "boot-picker-bringup-reason",
		    boot_picker_reason, (const unsigned int) strlen(boot_picker_reason))) {
			IOLog("Failed to write boot-picker-bringup-reason to NVRAM.\n");
		}

		// Set `boot-command = recover-system`.

		// Construct an OSSymbol and an OSString to be the (key, value) pair
		// we write to NVRAM. Unfortunately, since our value must be an OSString
		// instead of an OSData, we cannot use PEWriteNVRAMProperty() here.
		boot_command_sym = OSSymbol::withCStringNoCopy(SYSTEM_NVRAM_PREFIX "boot-command");
		boot_command_recover = OSString::withCStringNoCopy("recover-system");
		if (boot_command_sym == NULL || boot_command_recover == NULL) {
			IOLog("Failed to create boot-command strings.\n");
			goto do_reboot;
		}

		// Wait for NVRAM to be readable...
		nvram = OSDynamicCast(IODTNVRAM, IOService::waitForService(
			    IOService::serviceMatching("IODTNVRAM")));
		if (nvram == NULL) {
			IOLog("Failed to acquire IODTNVRAM object.\n");
			goto do_reboot;
		}

		// Wait for NVRAM to be writable...
		if (!IOServiceWaitForMatchingResource("IONVRAM", UINT64_MAX)) {
			IOLog("Failed to wait for IONVRAM service.\n");
			// attempt the work anyway...
		}

		// Write the new boot-command to NVRAM, and sync if successful.
		if (!nvram->setProperty(boot_command_sym, boot_command_recover)) {
			IOLog("Failed to save new boot-command to NVRAM.\n");
		} else {
			nvram->sync();
		}
	} else {
		IOLog("Unknown mode: %d\n", mode);
		return false;
	}

	// Clean up and reboot!
do_reboot:
	if (boot_command_recover != NULL) {
		boot_command_recover->release();
	}

	if (boot_command_sym != NULL) {
		boot_command_sym->release();
	}

	if (reboot) {
		IOLog("\nAbout to reboot into Recovery!\n");
		(void)PEHaltRestart(kPEPanicRestartCPUNoCallouts);
	}

	return true;
}

kern_return_t
IOFindBSDRoot( char * rootName, unsigned int rootNameSize,
    dev_t * root, u_int32_t * oflags )
{
	mach_timespec_t     t;
	IOService *         service;
	IORegistryEntry *   regEntry;
	OSDictionary *      matching = NULL;
	OSString *          iostr;
	OSNumber *          off;
	OSData *            data = NULL;

	UInt32              flags = 0;
	int                 mnr, mjr;
	const char *        mediaProperty = NULL;
	char *              rdBootVar;
	OSDataAllocation<char> str;
	const char *        look = NULL;
	int                 len;
	int                 wdt = 0;
	bool                debugInfoPrintedOnce = false;
	bool                needNetworkKexts = false;
	const char *        uuidStr = NULL;

	static int          mountAttempts = 0;

	int xchar, dchar;

	// stall here for anyone matching on the IOBSD resource to finish (filesystems)
	matching = IOService::serviceMatching(gIOResourcesKey);
	assert(matching);
	matching->setObject(gIOResourceMatchedKey, gIOBSDKey);

	if ((service = IOService::waitForMatchingService(matching, 30ULL * kSecondScale))) {
		OSSafeReleaseNULL(service);
	} else {
		IOLog("!BSD\n");
	}
	matching->release();
	matching = NULL;

	if (mountAttempts++) {
		IOLog("mount(%d) failed\n", mountAttempts);
		IOSleep( 5 * 1000 );
	}

	str = OSDataAllocation<char>( kMaxPathBuf + kMaxBootVar, OSAllocateMemory );
	if (!str) {
		return kIOReturnNoMemory;
	}
	rdBootVar = str.data() + kMaxPathBuf;

	if (!PE_parse_boot_argn("rd", rdBootVar, kMaxBootVar )
	    && !PE_parse_boot_argn("rootdev", rdBootVar, kMaxBootVar )) {
		rdBootVar[0] = 0;
	}

	if ((regEntry = IORegistryEntry::fromPath( "/chosen", gIODTPlane ))) {
		do {
			di_root_ramfile(regEntry);
			OSObject* unserializedContainer = NULL;
			data = OSDynamicCast(OSData, regEntry->getProperty( "root-matching" ));
			if (data) {
				unserializedContainer = OSUnserializeXML((char *)data->getBytesNoCopy());
				matching = OSDynamicCast(OSDictionary, unserializedContainer);
				if (matching) {
					continue;
				}
			}
			OSSafeReleaseNULL(unserializedContainer);

			data = (OSData *) regEntry->getProperty( "boot-uuid" );
			if (data) {
				uuidStr = (const char*)data->getBytesNoCopy();
				OSString *uuidString = OSString::withCString( uuidStr );

				// match the boot-args boot-uuid processing below
				if (uuidString) {
					IOLog("rooting via boot-uuid from /chosen: %s\n", uuidStr);
					IOService::publishResource( "boot-uuid", uuidString );
					uuidString->release();
					matching = IOUUIDMatching();
					mediaProperty = "boot-uuid-media";
					continue;
				} else {
					uuidStr = NULL;
				}
			}
		} while (false);
		OSSafeReleaseNULL(regEntry);
	}

//
//	See if we have a RAMDisk property in /chosen/memory-map.  If so, make it into a device.
//	It will become /dev/mdx, where x is 0-f.
//

	if (!didRam) {                                                                                           /* Have we already build this ram disk? */
		didRam = 1;                                                                                             /* Remember we did this */
		if ((regEntry = IORegistryEntry::fromPath( "/chosen/memory-map", gIODTPlane ))) {        /* Find the map node */
			data = (OSData *)regEntry->getProperty("RAMDisk");      /* Find the ram disk, if there */
			if (data) {                                                                                      /* We found one */
				uintptr_t *ramdParms;
				/* BEGIN IGNORE CODESTYLE */
				__typed_allocators_ignore_push
				ramdParms = (uintptr_t *)data->getBytesNoCopy();        /* Point to the ram disk base and size */
				__typed_allocators_ignore_pop
				/* END IGNORE CODESTYLE */
#if __LP64__
#define MAX_PHYS_RAM    (((uint64_t)UINT_MAX) << 12)
				if (ramdParms[1] > MAX_PHYS_RAM) {
					panic("ramdisk params");
				}
#endif /* __LP64__ */
				(void)mdevadd(-1, ml_static_ptovirt(ramdParms[0]) >> 12, (unsigned int) (ramdParms[1] >> 12), 0);        /* Initialize it and pass back the device number */
			}
			regEntry->release();                                                            /* Toss the entry */
		}
	}

//
//	Now check if we are trying to root on a memory device
//

	if ((rdBootVar[0] == 'm') && (rdBootVar[1] == 'd') && (rdBootVar[3] == 0)) {
		dchar = xchar = rdBootVar[2];                                                   /* Get the actual device */
		if ((xchar >= '0') && (xchar <= '9')) {
			xchar = xchar - '0';                                    /* If digit, convert */
		} else {
			xchar = xchar & ~' ';                                                           /* Fold to upper case */
			if ((xchar >= 'A') && (xchar <= 'F')) {                          /* Is this a valid digit? */
				xchar = (xchar & 0xF) + 9;                                              /* Convert the hex digit */
				dchar = dchar | ' ';                                                    /* Fold to lower case */
			} else {
				xchar = -1;                                                                     /* Show bogus */
			}
		}
		if (xchar >= 0) {                                                                                /* Do we have a valid memory device name? */
			OSSafeReleaseNULL(matching);
			*root = mdevlookup(xchar);                                                      /* Find the device number */
			if (*root >= 0) {                                                                        /* Did we find one? */
				rootName[0] = 'm';                                                              /* Build root name */
				rootName[1] = 'd';                                                              /* Build root name */
				rootName[2] = (char) dchar;                                                     /* Build root name */
				rootName[3] = 0;                                                                /* Build root name */
				IOLog("BSD root: %s, major %d, minor %d\n", rootName, major(*root), minor(*root));
				*oflags = 0;                                                                    /* Show that this is not network */

#if CONFIG_KDP_INTERACTIVE_DEBUGGING
				/* retrieve final ramdisk range and initialize KDP variables */
				if (mdevgetrange(xchar, &kdp_core_ramdisk_addr, &kdp_core_ramdisk_size) != 0) {
					IOLog("Unable to retrieve range for root memory device %d\n", xchar);
					kdp_core_ramdisk_addr = 0;
					kdp_core_ramdisk_size = 0;
				}
#endif

				goto iofrootx;                                                                  /* Join common exit... */
			}
			panic("IOFindBSDRoot: specified root memory device, %s, has not been configured", rdBootVar); /* Not there */
		}
	}

	if ((!matching) && rdBootVar[0]) {
		// by BSD name
		look = rdBootVar;
		if (look[0] == '*') {
			look++;
		}

		if (strncmp( look, "en", strlen( "en" )) == 0) {
			matching = IONetworkNamePrefixMatching( "en" );
			needNetworkKexts = true;
		} else if (strncmp( look, "uuid", strlen( "uuid" )) == 0) {
			OSDataAllocation<char> uuid( kMaxBootVar, OSAllocateMemory );

			if (uuid) {
				OSString *uuidString;

				if (!PE_parse_boot_argn( "boot-uuid", uuid.data(), kMaxBootVar )) {
					panic( "rd=uuid but no boot-uuid=<value> specified" );
				}
				uuidString = OSString::withCString(uuid.data());
				if (uuidString) {
					IOService::publishResource( "boot-uuid", uuidString );
					uuidString->release();
					IOLog("\nWaiting for boot volume with UUID %s\n", uuid.data());
					matching = IOUUIDMatching();
					mediaProperty = "boot-uuid-media";
				}
			}
		} else {
			matching = IOBSDNameMatching( look );
		}
	}

	if (!matching) {
		OSString * astring;
		// Match any HFS media

		matching = IOService::serviceMatching( "IOMedia" );
		assert(matching);
		astring = OSString::withCStringNoCopy("Apple_HFS");
		if (astring) {
			matching->setObject("Content", astring);
			astring->release();
		}
	}

	if (gIOKitDebug & kIOWaitQuietBeforeRoot) {
		IOLog( "Waiting for matching to complete\n" );
		IOService::getPlatform()->waitQuiet();
	}

	if (matching) {
		OSSerialize * s = OSSerialize::withCapacity( 5 );

		if (matching->serialize( s )) {
			IOLog( "Waiting on %s\n", s->text());
		}
		s->release();
	}

	char namep[8];
	if (needNetworkKexts
	    || PE_parse_boot_argn("-s", namep, sizeof(namep))) {
		IOService::startDeferredMatches();
	}

	PE_parse_boot_argn("wdt", &wdt, sizeof(wdt));
	do {
		t.tv_sec = ROOTDEVICETIMEOUT;
		t.tv_nsec = 0;
		matching->retain();
		service = IOService::waitForService( matching, &t );
		if ((-1 != wdt) && (!service || (mountAttempts == 10))) {
#if !XNU_TARGET_OS_OSX || !defined(__arm64__)
			PE_display_icon( 0, "noroot");
			IOLog( "Still waiting for root device\n" );
#endif

			if (!debugInfoPrintedOnce) {
				debugInfoPrintedOnce = true;
				if (gIOKitDebug & kIOLogDTree) {
					IOLog("\nDT plane:\n");
					IOPrintPlane( gIODTPlane );
				}
				if (gIOKitDebug & kIOLogServiceTree) {
					IOLog("\nService plane:\n");
					IOPrintPlane( gIOServicePlane );
				}
				if (gIOKitDebug & kIOLogMemory) {
					IOPrintMemory();
				}
			}

#if XNU_TARGET_OS_OSX && defined(__arm64__)
			// The disk isn't found - have the user pick from System Recovery.
			(void)IOSetRecoveryBoot(BSD_BOOTFAIL_MEDIA_MISSING, NULL, true);
#elif XNU_TARGET_OS_IOS || XNU_TARGET_OS_XR
			panic("Failed to mount root device");
#endif
		}
	} while (!service);

	OSSafeReleaseNULL(matching);

	if (service && mediaProperty) {
		service = (IOService *)service->getProperty(mediaProperty);
	}

	mjr = 0;
	mnr = 0;

	// If the IOService we matched to is a subclass of IONetworkInterface,
	// then make sure it has been registered with BSD and has a BSD name
	// assigned.

	if (service
	    && service->metaCast( "IONetworkInterface" )
	    && !IORegisterNetworkInterface( service )) {
		service = NULL;
	}

	if (service) {
		len = kMaxPathBuf;
		service->getPath( str.data(), &len, gIOServicePlane );
		IOLog("Got boot device = %s\n", str.data());

		iostr = (OSString *) service->getProperty( kIOBSDNameKey );
		if (iostr) {
			strlcpy( rootName, iostr->getCStringNoCopy(), rootNameSize );
		}
		off = (OSNumber *) service->getProperty( kIOBSDMajorKey );
		if (off) {
			mjr = off->unsigned32BitValue();
		}
		off = (OSNumber *) service->getProperty( kIOBSDMinorKey );
		if (off) {
			mnr = off->unsigned32BitValue();
		}

		if (service->metaCast( "IONetworkInterface" )) {
			flags |= 1;
		}
	} else {
		IOLog( "Wait for root failed\n" );
		strlcpy( rootName, "en0", rootNameSize );
		flags |= 1;
	}

	IOLog( "BSD root: %s", rootName );
	if (mjr) {
		IOLog(", major %d, minor %d\n", mjr, mnr );
	} else {
		IOLog("\n");
	}

	*root = makedev( mjr, mnr );
	*oflags = flags;

iofrootx:

	IOService::setRootMedia(service);

	if ((gIOKitDebug & (kIOLogDTree | kIOLogServiceTree | kIOLogMemory)) && !debugInfoPrintedOnce) {
		IOService::getPlatform()->waitQuiet();
		if (gIOKitDebug & kIOLogDTree) {
			IOLog("\nDT plane:\n");
			IOPrintPlane( gIODTPlane );
		}
		if (gIOKitDebug & kIOLogServiceTree) {
			IOLog("\nService plane:\n");
			IOPrintPlane( gIOServicePlane );
		}
		if (gIOKitDebug & kIOLogMemory) {
			IOPrintMemory();
		}
	}

	return kIOReturnSuccess;
}

void
IOSetImageBoot(void)
{
	// this will unhide all IOMedia, without waiting for kernelmanagement to start
	IOService::setRootMedia(NULL);
}

bool
IORamDiskBSDRoot(void)
{
	char rdBootVar[kMaxBootVar];
	if (PE_parse_boot_argn("rd", rdBootVar, kMaxBootVar )
	    || PE_parse_boot_argn("rootdev", rdBootVar, kMaxBootVar )) {
		if ((rdBootVar[0] == 'm') && (rdBootVar[1] == 'd') && (rdBootVar[3] == 0)) {
			return true;
		}
	}
	return false;
}

void
IOSecureBSDRoot(const char * rootName)
{
#if CONFIG_SECURE_BSD_ROOT
	IOReturn         result;
	IOPlatformExpert *pe;
	OSDictionary     *matching;
	const OSSymbol   *functionName = OSSymbol::withCStringNoCopy("SecureRootName");

	matching = IOService::serviceMatching("IOPlatformExpert");
	assert(matching);
	pe = (IOPlatformExpert *) IOService::waitForMatchingService(matching, 30ULL * kSecondScale);
	matching->release();
	assert(pe);
	// Returns kIOReturnNotPrivileged is the root device is not secure.
	// Returns kIOReturnUnsupported if "SecureRootName" is not implemented.
	result = pe->callPlatformFunction(functionName, false, (void *)rootName, (void *)NULL, (void *)NULL, (void *)NULL);
	functionName->release();
	OSSafeReleaseNULL(pe);

	if (result == kIOReturnNotPrivileged) {
		mdevremoveall();
	}

#endif  // CONFIG_SECURE_BSD_ROOT
}

void *
IOBSDRegistryEntryForDeviceTree(char * path)
{
	return IORegistryEntry::fromPath(path, gIODTPlane);
}

void
IOBSDRegistryEntryRelease(void * entry)
{
	IORegistryEntry * regEntry = (IORegistryEntry *)entry;

	if (regEntry) {
		regEntry->release();
	}
	return;
}

const void *
IOBSDRegistryEntryGetData(void * entry, char * property_name,
    int * packet_length)
{
	OSData *            data;
	IORegistryEntry *   regEntry = (IORegistryEntry *)entry;

	data = (OSData *) regEntry->getProperty(property_name);
	if (data) {
		*packet_length = data->getLength();
		return data->getBytesNoCopy();
	}
	return NULL;
}

kern_return_t
IOBSDGetPlatformUUID( uuid_t uuid, mach_timespec_t timeout )
{
	IOService * resources;
	OSString *  string;

	resources = IOService::waitForService( IOService::resourceMatching( kIOPlatformUUIDKey ), (timeout.tv_sec || timeout.tv_nsec) ? &timeout : NULL );
	if (resources == NULL) {
		return KERN_OPERATION_TIMED_OUT;
	}

	string = (OSString *) IOService::getPlatform()->getProvider()->getProperty( kIOPlatformUUIDKey );
	if (string == NULL) {
		return KERN_NOT_SUPPORTED;
	}

	uuid_parse( string->getCStringNoCopy(), uuid );

	return KERN_SUCCESS;
}
} /* extern "C" */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <sys/conf.h>
#include <sys/lock.h>
#include <sys/vnode.h>
#include <sys/vnode_if.h>
#include <sys/vnode_internal.h>
#include <sys/fcntl.h>
#include <sys/fsctl.h>
#include <sys/mount.h>
#include <IOKit/IOPolledInterface.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

// see HFSIOC_VOLUME_STATUS in APFS/HFS
#define HFS_IOCTL_VOLUME_STATUS _IOR('h', 24, u_int32_t)

LCK_GRP_DECLARE(gIOPolledCoreFileGrp, "polled_corefile");
LCK_MTX_DECLARE(gIOPolledCoreFileMtx, &gIOPolledCoreFileGrp);

IOPolledFileIOVars * gIOPolledCoreFileVars;
kern_return_t gIOPolledCoreFileOpenRet = kIOReturnNotReady;
IOPolledCoreFileMode_t gIOPolledCoreFileMode = kIOPolledCoreFileModeNotInitialized;

#if IOPOLLED_COREFILE

#define ONE_MB                  1024ULL * 1024ULL

#if defined(XNU_TARGET_OS_BRIDGE)
// On bridgeOS allocate a 150MB corefile and leave 150MB free
#define kIOCoreDumpSize         150ULL * ONE_MB
#define kIOCoreDumpFreeSize     150ULL * ONE_MB

#elif defined(XNU_TARGET_OS_OSX)

// on macOS devices allocate a corefile sized at 1GB / 32GB of DRAM,
// fallback to a 1GB corefile and leave at least 1GB free
#define kIOCoreDumpMinSize              1024ULL * ONE_MB
#define kIOCoreDumpIncrementalSize      1024ULL * ONE_MB

#define kIOCoreDumpFreeSize     1024ULL * ONE_MB

// on older macOS devices we allocate a 1MB file at boot
// to store a panic time stackshot
#define kIOStackshotFileSize    ONE_MB

#elif defined(XNU_TARGET_OS_XR)

// XR OS requries larger corefile storage because XNU core can take
// up to ~500MB.

#define kIOCoreDumpMinSize      350ULL * ONE_MB
#define kIOCoreDumpLargeSize    750ULL * ONE_MB

#define kIOCoreDumpFreeSize     350ULL * ONE_MB

#else /* defined(XNU_TARGET_OS_BRIDGE) */

// On embedded devices with >3GB DRAM we allocate a 500MB corefile
// otherwise allocate a 350MB corefile. Leave 350 MB free
#define kIOCoreDumpMinSize      350ULL * ONE_MB
#define kIOCoreDumpLargeSize    500ULL * ONE_MB

#define kIOCoreDumpFreeSize     350ULL * ONE_MB

#endif /* defined(XNU_TARGET_OS_BRIDGE) */

static IOPolledCoreFileMode_t
GetCoreFileMode()
{
	if (on_device_corefile_enabled()) {
		return kIOPolledCoreFileModeCoredump;
	} else if (panic_stackshot_to_disk_enabled()) {
		return kIOPolledCoreFileModeStackshot;
	} else {
		return kIOPolledCoreFileModeDisabled;
	}
}

static void
IOResolveCoreFilePath()
{
	DTEntry node;
	const char *value = NULL;
	unsigned int size = 0;

	if (kSuccess != SecureDTLookupEntry(NULL, "/product", &node)) {
		return;
	}
	if (kSuccess != SecureDTGetProperty(node, "kernel-core-dump-location", (void const **) &value, &size)) {
		return;
	}
	if (size == 0) {
		return;
	}

	// The kdp_corefile_path is allowed to be one of 2 options to working locations.
	// This value is set on EARLY_BOOT since we need to know it before any volumes are mounted. The mount
	// event triggers IOOpenPolledCoreFile() which opens the file. Once we commit to using the path from EDT
	// we can't back out since a different path may reside in a different volume.
	// In case the path from EDT can't be opened, there will not be a kernel core-dump
	if (strlcmp(value, "preboot", size) == 0) {
		kdp_corefile_path = kIOCoreDumpPrebootPath;
	} else if (strlcmp(value, "default", size) != 0) {
		IOLog("corefile path selection in device-tree is not one of the allowed values: %s, Using default %s\n", value, kdp_corefile_path);
		return;
	}

	IOLog("corefile path selection in device-tree was set to: %s (value: %s)\n", kdp_corefile_path, value);
}
STARTUP(EARLY_BOOT, STARTUP_RANK_MIDDLE, IOResolveCoreFilePath);

static void
IOCoreFileGetSize(uint64_t *ideal_size, uint64_t *fallback_size, uint64_t *free_space_to_leave, IOPolledCoreFileMode_t mode)
{
	unsigned int requested_corefile_size = 0;

	*ideal_size = *fallback_size = *free_space_to_leave = 0;

	// If a custom size was requested, override the ideal and requested sizes
	if (PE_parse_boot_argn("corefile_size_mb", &requested_corefile_size,
	    sizeof(requested_corefile_size))) {
		IOLog("Boot-args specify %d MB kernel corefile\n", requested_corefile_size);

		*ideal_size = *fallback_size = (requested_corefile_size * ONE_MB);
		return;
	}

	unsigned int status_flags = 0;
	int error = VNOP_IOCTL(rootvnode, HFS_IOCTL_VOLUME_STATUS, (caddr_t)&status_flags, 0,
	    vfs_context_kernel());
	if (!error) {
		if (status_flags & (VQ_VERYLOWDISK | VQ_LOWDISK | VQ_NEARLOWDISK)) {
			IOLog("Volume is low on space. Not allocating kernel corefile.\n");
			return;
		}
	} else {
		IOLog("Couldn't retrieve volume status. Error %d\n", error);
	}

#if defined(XNU_TARGET_OS_BRIDGE)
#pragma unused(mode)
	*ideal_size = *fallback_size = kIOCoreDumpSize;
	*free_space_to_leave = kIOCoreDumpFreeSize;
#elif !defined(XNU_TARGET_OS_OSX) /* defined(XNU_TARGET_OS_BRIDGE) */
#pragma unused(mode)
	*ideal_size = *fallback_size = kIOCoreDumpMinSize;

	if (max_mem > (3 * 1024ULL * ONE_MB)) {
		*ideal_size = kIOCoreDumpLargeSize;
	}

	*free_space_to_leave = kIOCoreDumpFreeSize;
#else /* defined(XNU_TARGET_OS_BRIDGE) */
	if (mode == kIOPolledCoreFileModeCoredump) {
		*ideal_size = *fallback_size = kIOCoreDumpMinSize;
		if (kIOCoreDumpIncrementalSize != 0 && max_mem > (32 * 1024ULL * ONE_MB)) {
			*ideal_size = ((ROUNDUP(max_mem, (32 * 1024ULL * ONE_MB)) / (32 * 1024ULL * ONE_MB)) * kIOCoreDumpIncrementalSize);
		}
		*free_space_to_leave = kIOCoreDumpFreeSize;
	} else if (mode == kIOPolledCoreFileModeStackshot) {
		*ideal_size = *fallback_size = *free_space_to_leave = kIOStackshotFileSize;
	}
#endif /* defined(XNU_TARGET_OS_BRIDGE) */

#if EXCLAVES_COREDUMP
	*ideal_size += sk_core_size();
#endif /* EXCLAVES_COREDUMP */

	return;
}

static IOReturn
IOAccessCoreFileData(void *context, boolean_t write, uint64_t offset, int length, void *buffer)
{
	errno_t vnode_error = 0;
	vfs_context_t vfs_context;
	vnode_t vnode_ptr = (vnode_t) context;

	vfs_context = vfs_context_kernel();
	vnode_error = vn_rdwr(write ? UIO_WRITE : UIO_READ, vnode_ptr, (caddr_t)buffer, length, offset,
	    UIO_SYSSPACE, IO_SWAP_DISPATCH | IO_SYNC | IO_NOCACHE | IO_UNIT, vfs_context_ucred(vfs_context), NULL, vfs_context_proc(vfs_context));

	if (vnode_error) {
		IOLog("Failed to %s the corefile. Error %d\n", write ? "write to" : "read from", vnode_error);
		return kIOReturnError;
	}

	return kIOReturnSuccess;
}

static void
IOOpenPolledCoreFile(thread_call_param_t __unused, thread_call_param_t corefilename)
{
	assert(corefilename != NULL);

	IOReturn err;
	char *filename = (char *) corefilename;
	uint64_t corefile_size_bytes = 0, corefile_fallback_size_bytes = 0, free_space_to_leave_bytes = 0;
	IOPolledCoreFileMode_t mode_to_init = GetCoreFileMode();

	if (gIOPolledCoreFileVars) {
		return;
	}
	if (!IOPolledInterface::gMetaClass.getInstanceCount()) {
		return;
	}

	if (gIOPolledCoreFileMode == kIOPolledCoreFileModeUnlinked) {
		return;
	}

	if (mode_to_init == kIOPolledCoreFileModeDisabled) {
		gIOPolledCoreFileMode = kIOPolledCoreFileModeDisabled;
		return;
	}

	// We'll overwrite this once we open the file, we update this to mark that we have made
	// it past initialization
	gIOPolledCoreFileMode = kIOPolledCoreFileModeClosed;

	IOCoreFileGetSize(&corefile_size_bytes, &corefile_fallback_size_bytes, &free_space_to_leave_bytes, mode_to_init);

	if (corefile_size_bytes == 0 && corefile_fallback_size_bytes == 0) {
		gIOPolledCoreFileMode = kIOPolledCoreFileModeUnlinked;
		return;
	}

	do {
		// This file reference remains open long-term in case we need to write a core-dump
		err = IOPolledFileOpen(filename, kIOPolledFileCreate, corefile_size_bytes, free_space_to_leave_bytes,
		    NULL, 0, &gIOPolledCoreFileVars, NULL, NULL, NULL);
		if (kIOReturnSuccess == err) {
			break;
		} else if (kIOReturnNoSpace == err) {
			IOLog("Failed to open corefile of size %llu MB (low disk space)\n",
			    (corefile_size_bytes / (1024ULL * 1024ULL)));
			if (corefile_size_bytes == corefile_fallback_size_bytes) {
				gIOPolledCoreFileOpenRet = err;
				return;
			}
		} else {
			IOLog("Failed to open corefile of size %llu MB (returned error 0x%x)\n",
			    (corefile_size_bytes / (1024ULL * 1024ULL)), err);
			gIOPolledCoreFileOpenRet = err;
			return;
		}

		err = IOPolledFileOpen(filename, kIOPolledFileCreate, corefile_fallback_size_bytes, free_space_to_leave_bytes,
		    NULL, 0, &gIOPolledCoreFileVars, NULL, NULL, NULL);
		if (kIOReturnSuccess != err) {
			IOLog("Failed to open corefile of size %llu MB (returned error 0x%x)\n",
			    (corefile_fallback_size_bytes / (1024ULL * 1024ULL)), err);
			gIOPolledCoreFileOpenRet = err;
			return;
		}
	} while (false);

	gIOPolledCoreFileOpenRet = IOPolledFilePollersSetup(gIOPolledCoreFileVars, kIOPolledPreflightCoreDumpState);
	if (kIOReturnSuccess != gIOPolledCoreFileOpenRet) {
		IOPolledFileClose(&gIOPolledCoreFileVars, 0, NULL, 0, 0, 0, false);
		IOLog("IOPolledFilePollersSetup for corefile failed with error: 0x%x\n", err);
	} else {
		IOLog("Opened corefile of size %llu MB\n", (corefile_size_bytes / (1024ULL * 1024ULL)));
		gIOPolledCoreFileMode = mode_to_init;
	}

	// Provide the "polled file available" callback with a temporary way to read from the file
	(void) IOProvideCoreFileAccess(kdp_core_polled_io_polled_file_available, NULL);

	return;
}

kern_return_t
IOProvideCoreFileAccess(IOCoreFileAccessRecipient recipient, void *recipient_context)
{
	kern_return_t error = kIOReturnSuccess;
	errno_t vnode_error = 0;
	vfs_context_t vfs_context;
	vnode_t vnode_ptr;

	if (!recipient) {
		return kIOReturnBadArgument;
	}

	if (kIOReturnSuccess != gIOPolledCoreFileOpenRet) {
		return kIOReturnNotReady;
	}

	// Open the kernel corefile
	vfs_context = vfs_context_kernel();
	vnode_error = vnode_open(kdp_corefile_path, (FREAD | FWRITE | O_NOFOLLOW), 0600, 0, &vnode_ptr, vfs_context);
	if (vnode_error) {
		IOLog("Failed to open the corefile. Error %d\n", vnode_error);
		return kIOReturnError;
	}

	// Call the recipient function
	error = recipient(IOAccessCoreFileData, (void *)vnode_ptr, recipient_context);

	// Close the kernel corefile
	vnode_close(vnode_ptr, FREAD | FWRITE, vfs_context);

	return error;
}

static void
IOClosePolledCoreFile(void)
{
	// Notify kdp core that the corefile is no longer available
	(void) kdp_core_polled_io_polled_file_unavailable();

	gIOPolledCoreFileOpenRet = kIOReturnNotOpen;
	gIOPolledCoreFileMode = kIOPolledCoreFileModeClosed;
	IOPolledFilePollersClose(gIOPolledCoreFileVars, kIOPolledPostflightCoreDumpState);
	IOPolledFileClose(&gIOPolledCoreFileVars, 0, NULL, 0, 0, 0, false);
}

static void
IOUnlinkPolledCoreFile(void)
{
	// Notify kdp core that the corefile is no longer available
	(void) kdp_core_polled_io_polled_file_unavailable();

	gIOPolledCoreFileOpenRet = kIOReturnNotOpen;
	gIOPolledCoreFileMode = kIOPolledCoreFileModeUnlinked;
	IOPolledFilePollersClose(gIOPolledCoreFileVars, kIOPolledPostflightCoreDumpState);
	IOPolledFileClose(&gIOPolledCoreFileVars, 0, NULL, 0, 0, 0, true);
}

#endif /* IOPOLLED_COREFILE */

extern "C" void
IOBSDMountChange(struct mount * mp, uint32_t op)
{
#if IOPOLLED_COREFILE
	uint64_t flags;
	char path[128];
	int pathLen;
	vnode_t vn;
	int result;

	lck_mtx_lock(&gIOPolledCoreFileMtx);

	switch (op) {
	case kIOMountChangeMount:
	case kIOMountChangeDidResize:

		if (gIOPolledCoreFileVars) {
			break;
		}
		flags = vfs_flags(mp);
		if (MNT_RDONLY & flags) {
			break;
		}
		if (!(MNT_LOCAL & flags)) {
			break;
		}

		vn = vfs_vnodecovered(mp);
		if (!vn) {
			break;
		}
		pathLen = sizeof(path);
		result = vn_getpath(vn, &path[0], &pathLen);
		vnode_put(vn);
		if (0 != result) {
			break;
		}
		if (!pathLen) {
			break;
		}
#if defined(XNU_TARGET_OS_BRIDGE)
		// on bridgeOS systems we put the core in /private/var/internal. We don't
		// want to match with /private/var because /private/var/internal is often mounted
		// over /private/var
		if ((pathLen - 1) < (int) strlen("/private/var/internal")) {
			break;
		}
#endif
		// Does this mount point include the kernel core-file?
		if (0 != strncmp(path, kdp_corefile_path, pathLen - 1)) {
			break;
		}

		thread_call_enter1(corefile_open_call, (void *) kdp_corefile_path);
		break;

	case kIOMountChangeUnmount:
	case kIOMountChangeWillResize:
		if (gIOPolledCoreFileVars && (mp == kern_file_mount(gIOPolledCoreFileVars->fileRef))) {
			thread_call_cancel_wait(corefile_open_call);
			IOClosePolledCoreFile();
		}
		break;
	}

	lck_mtx_unlock(&gIOPolledCoreFileMtx);
#endif /* IOPOLLED_COREFILE */
}

extern "C" void
IOBSDLowSpaceUnlinkKernelCore(void)
{
#if IOPOLLED_COREFILE
	lck_mtx_lock(&gIOPolledCoreFileMtx);
	if (gIOPolledCoreFileVars) {
		thread_call_cancel_wait(corefile_open_call);
		IOUnlinkPolledCoreFile();
	}
	lck_mtx_unlock(&gIOPolledCoreFileMtx);
#endif
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static char*
copyOSStringAsCString(OSString *string)
{
	size_t string_length = 0;
	char *c_string = NULL;

	if (string == NULL) {
		return NULL;
	}
	string_length = string->getLength() + 1;

	/* Allocate kernel data memory for the string */
	c_string = (char*)kalloc_data(string_length, (zalloc_flags_t)(Z_ZERO | Z_WAITOK | Z_NOFAIL));
	assert(c_string != NULL);

	/* Copy in the string */
	strlcpy(c_string, string->getCStringNoCopy(), string_length);

	return c_string;
}

extern "C" OS_ALWAYS_INLINE boolean_t
IOCurrentTaskHasStringEntitlement(const char *entitlement, const char *value)
{
	return IOTaskHasStringEntitlement(NULL, entitlement, value);
}

extern "C" boolean_t
IOTaskHasStringEntitlement(task_t task, const char *entitlement, const char *value)
{
	if (task == NULL) {
		task = current_task();
	}

	/* Validate input arguments */
	if (task == kernel_task || entitlement == NULL || value == NULL) {
		return false;
	}
	proc_t proc = (proc_t)get_bsdtask_info(task);

	kern_return_t ret = amfi->OSEntitlements.queryEntitlementStringWithProc(
		proc,
		entitlement,
		value);

	if (ret == KERN_SUCCESS) {
		return true;
	}

	return false;
}

extern "C" OS_ALWAYS_INLINE boolean_t
IOCurrentTaskHasEntitlement(const char *entitlement)
{
	return IOTaskHasEntitlement(NULL, entitlement);
}

extern "C" boolean_t
IOTaskHasEntitlement(task_t task, const char *entitlement)
{
	if (task == NULL) {
		task = current_task();
	}

	/* Validate input arguments */
	if (task == kernel_task || entitlement == NULL) {
		return false;
	}
	proc_t proc = (proc_t)get_bsdtask_info(task);

	kern_return_t ret = amfi->OSEntitlements.queryEntitlementBooleanWithProc(
		proc,
		entitlement);

	if (ret == KERN_SUCCESS) {
		return true;
	}

	return false;
}

extern "C" OS_ALWAYS_INLINE char*
IOCurrentTaskGetEntitlement(const char *entitlement)
{
	return IOTaskGetEntitlement(NULL, entitlement);
}

extern "C" char*
IOTaskGetEntitlement(task_t task, const char *entitlement)
{
	void *entitlement_object = NULL;
	char *return_value = NULL;

	if (task == NULL) {
		task = current_task();
	}

	/* Validate input arguments */
	if (task == kernel_task || entitlement == NULL) {
		return NULL;
	}
	proc_t proc = (proc_t)get_bsdtask_info(task);

	kern_return_t ret = amfi->OSEntitlements.copyEntitlementAsOSObjectWithProc(
		proc,
		entitlement,
		&entitlement_object);

	if (ret != KERN_SUCCESS) {
		return NULL;
	}
	assert(entitlement_object != NULL);

	OSObject *os_object = (OSObject*)entitlement_object;
	OSString *os_string = OSDynamicCast(OSString, os_object);

	/* Get a C string version of the OSString */
	return_value = copyOSStringAsCString(os_string);

	/* Free the OSObject which was given to us */
	OSSafeReleaseNULL(os_object);

	return return_value;
}

extern "C" boolean_t
IOVnodeHasEntitlement(vnode_t vnode, int64_t off, const char *entitlement)
{
	OSObject * obj;
	off_t offset = (off_t)off;

	obj = IOUserClient::copyClientEntitlementVnode(vnode, offset, entitlement);
	if (!obj) {
		return false;
	}
	obj->release();
	return obj != kOSBooleanFalse;
}

extern "C" char *
IOVnodeGetEntitlement(vnode_t vnode, int64_t off, const char *entitlement)
{
	OSObject *obj = NULL;
	OSString *str = NULL;
	size_t len;
	char *value = NULL;
	off_t offset = (off_t)off;

	obj = IOUserClient::copyClientEntitlementVnode(vnode, offset, entitlement);
	if (obj != NULL) {
		str = OSDynamicCast(OSString, obj);
		if (str != NULL) {
			len = str->getLength() + 1;
			value = (char *)kalloc_data(len, Z_WAITOK);
			strlcpy(value, str->getCStringNoCopy(), len);
		}
		obj->release();
	}
	return value;
}
