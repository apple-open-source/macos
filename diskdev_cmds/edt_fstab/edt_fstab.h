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

//  edt_fstab.h
//
//  Created on 12/11/2018.
//

#ifndef edt_fstab_h
#define edt_fstab_h

#include <fstab.h>
#include <TargetConditionals.h>

/*  fstab related functions */
int                 setup_fsent(void);
void                end_fsent(void);
struct fstab        *get_fsent(void);
struct fstab        *get_fsspec(const char *);
struct fstab        *get_fsfile(const char *);

#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
#define MOUNT_INTERNAL_PATH     "/usr/local/bin/mount_internal"
#define RAMDISK_FS_SPEC         "ramdisk"

enum {EDT_OS_ENV_MAIN = 1, EDT_OS_ENV_OTHER};

/*
 * Note - the following functions are independent of the *_fsent functions above.
 * 		Meaning that they will not change the iteration state.
 *		If an fs iteration is in process, the required information is available and will
 *		simply be returned.
 *		If an iteration is not in process, they will lookup and return the required
 *		information, having reset the iteration state.
 *
 *		return values:
 *		get_boot_container, get_data_volume - return the bsd name of the requested
 *		device upon success. Null otherwise.
 *		needs_data_volume - assumes the data volume is required, unless a successful
 *		lookup reveals otherwise.
 */
const char          *get_boot_container(uint32_t *os_env);
const char          *get_data_volume(void);
int                 needs_data_volume(void);
#endif

#endif /* edt_fstab_h */
