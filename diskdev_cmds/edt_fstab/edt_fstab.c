/*
 * Copyright (c) 2018-2019 Apple Inc. All rights reserved.
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

#ifdef MOUNT_INTERNAL
#include "internal_mount_ops.h"

/* boot-arg conditioning RW system mount - internal only */
#define APFS_EDT_MOUNT_RW       "apfs_edt_rw_mount"

enum {EDT_MOUNT_RO, EDT_MOUNT_RW};
#endif

/* fstab info specifying the OS environment being booted */
#ifndef kEDTOSEnvironment
#define kEDTOSEnvironment       CFSTR("os_env_type")
#endif

static io_registry_entry_t edt_fsent_iter = 0;
static uint32_t current_entry = 0;
static uint32_t fsent_count = 0;
static struct fstab *edt_fsent = NULL;
char boot_container[EDTVolumePropertySize] = {0};
char data_volume[EDTVolumePropertySize] = {0};
static int data_volume_required = 0;

static int64_t edt_mount_mode = 0;
static uint32_t edt_os_environment = 0;

// can be called multiple times
static void
edt_endfsent(void)
{
    if (edt_fsent_iter != IO_OBJECT_NULL) {
        IOObjectRelease(edt_fsent_iter);
        edt_fsent_iter = IO_OBJECT_NULL;
    }

    if (edt_fsent) {
        for (int i = 0; i < fsent_count; i++) {
            if (edt_fsent[i].fs_spec)
                free(edt_fsent[i].fs_spec);
            if (edt_fsent[i].fs_file)
                free(edt_fsent[i].fs_file);
            if (edt_fsent[i].fs_mntops)
                free(edt_fsent[i].fs_mntops);
            if (edt_fsent[i].fs_type)
                free(edt_fsent[i].fs_type);
        }
        free(edt_fsent);
        edt_fsent = NULL;
    }
}

// returns 0 upon success
static int
get_property(const void *key, char *prop, CFMutableDictionaryRef dict)
{
    *prop = 0;
    const CFDataRef value = CFDictionaryGetValue(dict, key);
    if (!value) {
        CFRelease(dict);
        edt_endfsent();
        fprintf(stderr, "failed to get filesystem entry property %s\n",
                CFStringGetCStringPtr(key, kCFStringEncodingUTF8));
        return 1;
    }
    CFDataGetBytes(value, CFRangeMake(0,CFDataGetLength(value)), (UInt8*)prop);
    return 0;
}

// returns 0 upon success
static errno_t
edt_setfsent(bool container_only)
{
    kern_return_t err;
    io_registry_entry_t fsent, fs;
    CFMutableDictionaryRef fs_info = NULL;
    CFStringRef container = NULL;

#ifdef MOUNT_INTERNAL
    (void) os_parse_boot_arg_int(APFS_EDT_MOUNT_RW, &edt_mount_mode);
#endif

    // reset
    if (edt_fsent) {
        current_entry = 0;
        return 0;
    }

    fsent = IORegistryEntryFromPath(kIOMasterPortDefault, kEDTFilesystemEntry);
    if (fsent == IO_OBJECT_NULL) {
        fprintf(stderr, "failed to get filesystem info\n");
        return ENOENT;
    }

    CFDataRef os_env = IORegistryEntryCreateCFProperty(fsent, kEDTOSEnvironment, kCFAllocatorDefault, 0);
    if (!os_env) {
        edt_os_environment = EDT_OS_ENV_MAIN;
    } else {
        CFDataGetBytes(os_env, CFRangeMake(0, CFDataGetLength(os_env)), (UInt8*)(&edt_os_environment));
        CFRelease(os_env);
    }

    CFDataRef max_entries = IORegistryEntryCreateCFProperty(fsent, kEDTMaxFSEnries, kCFAllocatorDefault, 0);
    if (!max_entries) {
        IOObjectRelease(fsent);
        fprintf(stderr, "failed to get filesystem entry count\n");
        return ENOENT;
    }

    CFDataGetBytes(max_entries, CFRangeMake(0,CFDataGetLength(max_entries)), (UInt8*)(&fsent_count));
    CFRelease(max_entries);

    err = IORegistryEntryCreateIterator(fsent, kIODeviceTreePlane, 0, &edt_fsent_iter);
    IOObjectRelease(fsent);
    if (err) {
        fprintf(stderr, "failed to create filesystem entry iterator - %s\n",
                strerror(err_get_code(err)));
        return err_get_code(err);
    }

    edt_fsent = calloc(fsent_count, sizeof(struct fstab));
    if (edt_fsent == NULL) {
        edt_endfsent();
        fprintf(stderr, "failed to allocate memory for filesystem entries\n");
        return ENOMEM;
    }

    // ignore this error here, bail out if we actually need the container device
    err = APFSContainerGetBootDevice(&container);
    if (!err) {
        strcpy(boot_container, _PATH_DEV);
        CFStringGetCString(container,
                           boot_container + strlen(_PATH_DEV),
                           EDTVolumePropertySize,
                           kCFStringEncodingUTF8);
        CFRelease(container);
    } else {
        bool main_os_boot = (edt_os_environment == EDT_OS_ENV_MAIN);
        fprintf(stderr, "%sfailed to get boot device - %s\n",
                main_os_boot ? "" : "warning: ", strerror(err_get_code(err)));
        if (main_os_boot) {
            CFRelease(fs_info);
            return err_get_code(err);
        } else {
            // just a warning here (rdar://48693021)
            err = 0;
        }
    }

    // we only need the boot device, dont need to get the fs entries from EDT
    if (container_only) {
        return err;
    }

    // get all fs entries from EDT and insert them into edt_fsent array indexed by mount order
    // we wont necessarily have max(mount_order) entries, so we may have unused entries.
    // edt_getfsent knows to skip these entries
    while ((fs = IOIteratorNext(edt_fsent_iter)) != IO_OBJECT_NULL) {
        uint32_t mount_order;
        uint32_t fs_role = APFS_VOL_ROLE_NONE;
        CFMutableArrayRef matches = NULL;
        struct fstab *current = NULL;
        bool ephemeral = false;

        err = IORegistryEntryCreateCFProperties(fs, &fs_info, kCFAllocatorDefault, 0);
        IOObjectRelease(fs);
        if (err) {
            edt_endfsent();
            fprintf(stderr, "failed to create filesystem entry properties - %s\n",
                    strerror(err_get_code(err)));
            return err_get_code(err);
        }

        if (get_property(kEDTVolumeMountOrder, (char*)&mount_order, fs_info)) {
            return ENOENT;
        }
        current = &edt_fsent[mount_order];

        if ((current->fs_spec = calloc(EDTVolumePropertySize, sizeof(char))) == NULL ||
            (current->fs_type = calloc(EDTVolumePropertySize, sizeof(char))) == NULL ||
            (current->fs_file = calloc(EDTVolumePropertyMaxSize, sizeof(char))) == NULL ||
            (current->fs_mntops = calloc(EDTVolumePropertyMaxSize, sizeof(char))) == NULL) {
            CFRelease(fs_info);
            edt_endfsent();
            fprintf(stderr, "failed to allocate memory for filesystem entry\n");
            return ENOMEM;
        }

        if (CFDictionaryGetValue(fs_info, kEDTVolumeEphemeral) != NULL) {
            ephemeral = true ;
        }

        current->fs_vfstype = EDTVolumeFSType;
        if (get_property(kEDTVolumeMountPoint, current->fs_file, fs_info) ||
            get_property(kEDTVolumeMountType, current->fs_type, fs_info) ||
            get_property(kEDTVolumeMountOpts, current->fs_mntops, fs_info) ||
            get_property(kEDTVolumePassno, (char*)&current->fs_passno, fs_info)) {
            return ENOENT;
        }

        if (ephemeral) {
            // ephemeral fs does not have vol.fs_role - use a synthetic dev name
            strlcpy(current->fs_spec, RAMDISK_FS_SPEC, sizeof(RAMDISK_FS_SPEC));
            err = 0;
        } else {

            if (get_property(kEDTVolumeRole, (char*)&fs_role, fs_info)) {
                return ENOENT;
            }

            err = APFSVolumeRoleFind(boot_container, fs_role, &matches);
            if (err) {
                fprintf(stderr, "failed to lookup volume role 0x%x - %s\n",
                        fs_role, strerror(err_get_code(err)));
                err = err_get_code(err);
            } else if (CFArrayGetCount(matches) > 1) {
                fprintf(stderr, "multiple volumes with role 0x%x\n",
                        fs_role);
                err = E2BIG;
            }

            if (!err) {
                CFStringGetCString(CFArrayGetValueAtIndex(matches, 0),
                                   current->fs_spec,
                                   EDTVolumePropertySize,
                                   kCFStringEncodingUTF8);
                if (fs_role == APFS_VOL_ROLE_DATA) {
                    strlcpy(data_volume, current->fs_spec, sizeof(data_volume));
                    data_volume_required = 1;
                }
            } else {
                // ignore missing Data volume as it might be missing due to obliteration,
                // but mark it as required since the EDT says it should be present
                if (fs_role == APFS_VOL_ROLE_DATA) {
                    free(current->fs_spec);
                    current->fs_spec = NULL;
                    err = 0;
                    data_volume_required = 1;
                }
            }

            if (matches) {
                CFRelease(matches);
            }

            if (err) {
                CFRelease(fs_info);
                edt_endfsent();
                return err;
            }
        }



#ifdef MOUNT_INTERNAL
        char vol_name[EDTVolumePropertySize];
        if (get_property(kEDTVolumeName, vol_name, fs_info)) {
            return ENOENT;
        }

        if ((!strcmp(vol_name, kAPFSVolumeRoleSystem) && (edt_mount_mode == EDT_MOUNT_RW))) {
            current->fs_type[1] = 'w';
        }
#endif

        if (strlen(current->fs_mntops) > 0) {
            strcat(current->fs_mntops, ",");
        }
        strcat(current->fs_mntops, current->fs_type);

        CFRelease(fs_info);
    }

    return 0;
}

struct fstab*
edt_getfsent(void)
{
    if (current_entry >= fsent_count) {
#ifdef MOUNT_INTERNAL
        struct fstab *fs;

        if (current_entry == fsent_count) {
            current_entry++;
            fs = get_log_volume(boot_container);
            if (fs) {
                return fs;
            }
        }

#if TARGET_OS_BRIDGE
        /*
         * The scratch volume is currently only used on the bridge.
         * An attempt to mount it on other embedded platforms will fail
         * with EPERM, so ignore this role on all platforms except OS_BRIDGE
         */
        if (current_entry == (fsent_count + 1)) {
            current_entry++;
            fs = get_scratch_volume(boot_container);
            if (fs) {
                return fs;
            }
        }
#endif /* TARGET_OS_BRIDGE */
#endif /* MOUNT_INTERNAL */
        return NULL;
    }

    // skip non-existent entries
    while (edt_fsent[current_entry].fs_spec == NULL) {
        current_entry++;
        if (current_entry == fsent_count) {
            return edt_getfsent();
        }
    }

    current_entry++;
    return &edt_fsent[current_entry - 1];
}

static struct fstab*
edt_getfs(const char *fsid, bool fsspec)
{
    struct fstab *fs = NULL;
    char *cmp_str;

    atexit(edt_endfsent);
    edt_setfsent(false);

    while ((fs = get_fsent()) != NULL) {
        if (fsspec) {
            cmp_str = fs->fs_spec;
        } else {
            cmp_str = fs->fs_file;
        }

        if (!strncmp(fsid, cmp_str, EDTVolumePropertyMaxSize))
            break;
    }

    return fs;
}

static struct fstab*
edt_getfsspec(const char *fsspec)
{
    return edt_getfs(fsspec, true /* fs_spec */);
}

static struct fstab*
edt_getfsfile(const char *fsspec)
{
    return edt_getfs(fsspec, false /* fs_file */);
}

const char *
get_boot_container(uint32_t *os_env)
{
    const char *container = NULL;

    // iteration in process - just return the boot container
    if (edt_fsent) {
        container = boot_container;
    } else if ((edt_setfsent(true /* container_only */) == 0)) {
        edt_endfsent();
        container = boot_container;
    }

    if (container) {
        *os_env = edt_os_environment;
    }

    return container;
}

const char *
get_data_volume(void)
{
    const char *data_vol = NULL;

    // iteration in process - just return the data volume
    if (edt_fsent) {
        data_vol = data_volume;
    } else if ((edt_setfsent(false) == 0)) {
        edt_endfsent();
        data_vol = data_volume;
    }
    
    if (strnlen(data_volume, sizeof(data_volume)) == 0) {
        data_vol = NULL;
    }

    return data_vol;
}

int
needs_data_volume(void)
{
    int data_vol_required = 1;

    // iteration in process - just return the data volume requirement
    if (edt_fsent) {
        data_vol_required = data_volume_required;
    } else if ((edt_setfsent(false) == 0)) {
        edt_endfsent();
        data_vol_required = data_volume_required;
    }

    return data_vol_required;
}
#endif /* (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR) */

// to match setfsent - returns 0 upon failure
int
setup_fsent(void)
{
#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
    return ((errno = edt_setfsent(false)) ? 0 : 1);
#else
    return setfsent();
#endif
}

void
end_fsent(void)
{
#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
    edt_endfsent();
#else
    endfsent();
#endif
}

struct fstab*
get_fsent(void)
{
#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
    return edt_getfsent();
#else
    return getfsent();
#endif
}

struct fstab*
get_fsspec(const char *fsspec)
{
#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
    return edt_getfsspec(fsspec);
#else
    return getfsspec(fsspec);
#endif
}

struct fstab*
get_fsfile(const char *fsfile)
{
#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
    return edt_getfsfile(fsfile);
#else
    return getfsfile(fsfile);
#endif
}
