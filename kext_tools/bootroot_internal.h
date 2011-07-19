/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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
 * FILE: bootroot_internal.h
 * AUTH: Soren Spies (sspies)
 * DATE: 8 June 2006 (as update_boot.h)
 * DESC: routines for implementing 'kextcache -u' functionality (4252674)
 *       in which bootcaches.plist files get copied to any Apple_Boots
 */

#include <sys/types.h>      // mode_t
#include <CoreFoundation/CoreFoundation.h>

#include "bootroot.h"

// in update_boot.c

int checkUpdateCachesAndBoots(
    CFURLRef volumeURL,
    Boolean force,
    Boolean expectUpToDate,
    Boolean cachesOnly);

// "put" and "take" let routines decide if a lock is needed (e.g. if no kextd)
// only used by volume lockers (kextcache, libBootRoot clients, !kextd)
int takeVolumeForPath(const char *volPath);
int putVolumeForPath(const char *path, int status);
