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

//  edt_fstab.h
//
//  Created on 12/11/2018.
//

#ifndef edt_fstab_h
#define edt_fstab_h

#include <TargetConditionals.h>

#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
#define RAMDISK_FS_SPEC         "ramdisk"

/*
 *		get_boot_container, get_data_volume - return the bsd name of the requested
 *		device upon success. Null otherwise.
 */
const char          *get_boot_container(uint32_t *os_env);
const char          *get_data_volume(void);

int                 get_boot_manifest_hash(char *boot_manifest_hash, size_t boot_manifest_hash_len);
#endif

#endif /* edt_fstab_h */
