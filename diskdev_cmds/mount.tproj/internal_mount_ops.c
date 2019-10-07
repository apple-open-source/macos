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

//  internal_mount_ops.c
//  mount_internal
//
//  Created on 12/11/2018.
//

#include "internal_mount_ops.h"

#ifdef MOUNT_INTERNAL

#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
#include <APFS/APFS.h>
#include <sys/syslimits.h>
#include <sys/param.h>

#define DATA_PARTITION_MOUNT_POINT          "/private/var"
#define DEFAULT_MOUNT_OPTS                  "rw,nosuid,nodev"

// log volume
#define LOG_PARTITION_MOUNT_POINT           DATA_PARTITION_MOUNT_POINT "/logs"

static char log_volume_dev_node[NAME_MAX];
static struct fstab log_volume;

// scratch volume
#define SCRATCH_PARTITION_MOUNT_POINT       DATA_PARTITION_MOUNT_POINT "/internal"

static char scratch_volume_dev_node[NAME_MAX];
static struct fstab scratch_volume;

// dont try an mount an internal volume if the mountpoint doesnt exist
// to avoid a panic during boot
// while there are advantages to this being a hard failure, it could
// brick the device and require an erase install for prod-fused devices
// (can recover on dev-fused devices by booting into single-user mode)
static bool
mount_point_exists(const char *mount_point, uint16_t vol_role)
{
    char mount_path[MAXPATHLEN];
    if (realpath(mount_point, mount_path) == NULL) {
        fprintf(stderr, "WARNING volume role (0x%x) will not be mounted! Cant resolve mount point: %s - %s\n",
             vol_role, mount_point, mount_path);
        return false;
    }

    return true;
}

// lookup a volume by role, and return an array with a single match
static CFMutableArrayRef
get_volume_with_role(const char *container, uint16_t role)
{
    int err = 0;
    CFMutableArrayRef matches = NULL;

    // check if we have a volume with this role
    err = APFSVolumeRoleFind(container, role, &matches);
    if (err) {
        if (err_get_code(err) != ENOATTR) {
            fprintf(stderr, "failed to lookup volume role 0x%x - %s\n",
                    role, strerror(err_get_code(err)));
        }
        return NULL;

    } else if (CFArrayGetCount(matches) > 1) {
        CFRelease(matches);
        fprintf(stderr, "multiple volumes with role 0x%x\n", role);
        return NULL;
    }

    return matches;
}

struct fstab *
get_log_volume(const char *container)
{
    CFStringRef log_volume_dev;
    CFMutableArrayRef matches = NULL;

    matches = get_volume_with_role(container, APFS_VOL_ROLE_INSTALLER);
    // no log volume present, dont try and mount it
    if (!matches) {
        return NULL;
    }

    log_volume_dev = (CFStringRef)CFArrayGetValueAtIndex(matches, 0);
    CFStringGetCString(log_volume_dev, log_volume_dev_node, NAME_MAX, kCFStringEncodingUTF8);
    CFRelease(matches);

    log_volume.fs_spec = log_volume_dev_node;
    log_volume.fs_file = LOG_PARTITION_MOUNT_POINT;
    log_volume.fs_type = FSTAB_RW;
    log_volume.fs_vfstype = EDTVolumeFSType;
    log_volume.fs_mntops = DEFAULT_MOUNT_OPTS;
    log_volume.fs_freq = 0;
    log_volume.fs_passno = 2;

    if (mount_point_exists(LOG_PARTITION_MOUNT_POINT ,APFS_VOL_ROLE_INSTALLER))
        return &log_volume;
    else
        return NULL;
}

struct fstab *
get_scratch_volume(const char *container)
{
    CFStringRef scratch_volume_dev;
    CFMutableArrayRef matches = NULL;

    matches = get_volume_with_role(container, APFS_VOL_ROLE_INTERNAL);
    // no scratch volume present, dont try and mount it
    if (!matches) {
        return NULL;
    }

    scratch_volume_dev = (CFStringRef)CFArrayGetValueAtIndex(matches, 0);
    CFStringGetCString(scratch_volume_dev, scratch_volume_dev_node, NAME_MAX, kCFStringEncodingUTF8);
    CFRelease(matches);

    scratch_volume.fs_spec = scratch_volume_dev_node;
    scratch_volume.fs_file = SCRATCH_PARTITION_MOUNT_POINT;
    scratch_volume.fs_type = FSTAB_RW;
    scratch_volume.fs_vfstype = EDTVolumeFSType;
    scratch_volume.fs_mntops = DEFAULT_MOUNT_OPTS;
    scratch_volume.fs_freq = 0;
    scratch_volume.fs_passno = 2;

    if (mount_point_exists(SCRATCH_PARTITION_MOUNT_POINT ,APFS_VOL_ROLE_INTERNAL))
        return &scratch_volume;
    else
        return NULL;
}
#endif /* (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR) */

#endif /* MOUNT_INTERNAL */
