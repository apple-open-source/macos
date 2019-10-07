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

//  internal_mount_ops.h
//  mount_internal
//
//  Created on 12/11/2018.
//

#ifndef internal_mount_ops_h
#define internal_mount_ops_h

#ifdef MOUNT_INTERNAL
#include <TargetConditionals.h>

#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
#include <fstab.h>
/*
 * This is currently used to auto-mount volumes used on internal
 * builds only that do not have EDT entries.
 * For now we assume a certain mountpoint for these volumes. If a volume
 * with one of these roles is present, but the assumed mountpoint is
 * non existent, mount will fail and cause the device to panic at boot.
 * This is intentional. We assume that if you set this role, you want the
 * volume mounted. Implications of this:
 *  1) PurpleRestore Erase Install - if a role is set, the appropriate mountpoint
 *      must be created as well
 *  2) PurpleRestore Update Install / OTA - if a role is set, it will be preserved
 *      and the appropriate mountpoint must be preserved as well.
 */
struct fstab *get_log_volume(const char *container);
struct fstab *get_scratch_volume(const char *container);
#endif

#endif /* MOUNT_INTERNAL */

#endif /* internal_mount_ops_h */
