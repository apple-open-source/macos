/*
 * Copyright (c) 2018-2020 Apple Inc. All rights reserved.
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

//  edt_fstab.c
//
//  Created on 12/11/2018.
//

#include <sys/types.h>

#include "edt_fstab.h"

#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)

/* Some APFS specific goop */
#include <APFS/APFS.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <os/bsd.h>
#include <sys/stat.h>
#include <paths.h>

char boot_container[EDTVolumePropertySize] = {};
char data_volume[EDTVolumePropertySize] = {};

static uint32_t edt_os_environment = EDT_OS_ENV_MAIN;

const char *
get_boot_container(uint32_t *os_envp)
{
	kern_return_t err;
	CFMutableDictionaryRef fs_info = NULL;
	CFStringRef container = NULL;
	CFDataRef os_env = NULL;

	// already got the boot container
	if (strnlen(boot_container, sizeof(boot_container)) > 0) {
		*os_envp = edt_os_environment;
		return boot_container;
	}

	fs_info = IORegistryEntryFromPath(kIOMasterPortDefault, kEDTFilesystemEntry);
	if (fs_info == IO_OBJECT_NULL) {
		fprintf(stderr, "failed to get filesystem info\n");
		return NULL;
	}

	// lookup the OS environment being booted, assumes main OS upon failure
	os_env = IORegistryEntryCreateCFProperty(fs_info, kEDTOSEnvironment, kCFAllocatorDefault, 0);
	if (os_env) {
		CFDataGetBytes(os_env, CFRangeMake(0, CFDataGetLength(os_env)), (UInt8*)(&edt_os_environment));
		CFRelease(os_env);
	}
	IOObjectRelease(fs_info);
	*os_envp = edt_os_environment;

	// lookup the boot container
	err = APFSContainerGetBootDevice(&container);
	if (!err) {
		strcpy(boot_container, _PATH_DEV);
		CFStringGetCString(container,
						   boot_container + strlen(_PATH_DEV),
						   EDTVolumePropertySize - strlen(_PATH_DEV),
						   kCFStringEncodingUTF8);
		CFRelease(container);
		return boot_container;
	} else {
		// just a warning if not booting the main OS (rdar://48693021)
		fprintf(stderr, "%sfailed to get boot device - %s\n",
				(edt_os_environment == EDT_OS_ENV_MAIN) ? "" : "warning: ",
				strerror(err_get_code(err)));
		return NULL;
	}
}

const char *
get_data_volume(void)
{
	const char *container = NULL;
	CFMutableArrayRef matches = NULL;
	OSStatus status;

	// already got the data volume
	if (strnlen(data_volume, sizeof(data_volume)) > 0) {
		return data_volume;
	}

	// get the boot container
	if (strlen(boot_container) > 0) {
		container = boot_container;
	} else {
		uint32_t os_env;
		container = get_boot_container(&os_env);
	}
	if (!container) {
		return NULL;
	}

	// lookup the data volume
	status = APFSVolumeRoleFind(container, APFS_VOL_ROLE_DATA, &matches);
	if (status) {
		// just a warning if not booting the main OS
		fprintf(stderr, "%sfailed to lookup data volume - %s\n",
				(edt_os_environment == EDT_OS_ENV_MAIN) ? "" : "warning: ",
				strerror(err_get_code(status)));
		return NULL;
	} else if (CFArrayGetCount(matches) > 1) {
		fprintf(stderr, "found multiple data volumes\n");
		CFRelease(matches);
		return NULL;
	} else {
		CFStringGetCString(CFArrayGetValueAtIndex(matches, 0),
						   data_volume,
						   EDTVolumePropertySize,
						   kCFStringEncodingUTF8);
		CFRelease(matches);
		return data_volume;
	}
}

int
get_boot_manifest_hash(char *boot_manifest_hash, size_t boot_manifest_hash_len)
{
	kern_return_t err = 0;
	io_registry_entry_t chosen;
	CFDataRef bm_hash = NULL;
	size_t bm_hash_size;
	uint8_t bm_hash_buf[EDTVolumePropertyMaxSize] = {};
	const char *hexmap = "0123456789ABCDEF";

	chosen = IORegistryEntryFromPath(kIOMasterPortDefault, kIODeviceTreePlane ":/chosen");
	if (chosen == IO_OBJECT_NULL) {
		fprintf(stderr, "failed to get chosen info\n");
		return ENOENT;
	}

	bm_hash = IORegistryEntryCreateCFProperty(chosen, CFSTR("boot-manifest-hash"), kCFAllocatorDefault, 0);
	if (!bm_hash) {
		fprintf(stderr, "failed to get boot-manifest-hash\n");
		IOObjectRelease(chosen);
		return ENOENT;
	} else {
		bm_hash_size = CFDataGetLength(bm_hash);
		CFDataGetBytes(bm_hash, CFRangeMake(0, bm_hash_size), bm_hash_buf);

		if (boot_manifest_hash_len < (bm_hash_size * 2 + 1)) {
			err = EINVAL;
		} else {
			// hexdump the hash into input buffer
			for (size_t i = 0; i < (2 * bm_hash_size); i++)
				*boot_manifest_hash++ = hexmap[(bm_hash_buf[i / 2] >> ((i % 2) ? 0 : 4)) & 0xf];
			*boot_manifest_hash = '\0';
		}
		CFRelease(bm_hash);
		IOObjectRelease(chosen);
	}

	return err;
}
#endif /* (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR) */
