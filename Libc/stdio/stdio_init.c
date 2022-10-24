/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
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

#include <TargetConditionals.h>
#include <mach-o/dyld.h>
#include <mach-o/dyld_priv.h>
#include "crt_externs.h" /* _NSGetMachExecuteHeader() */

#include "stdio_init.h"

#ifndef PR_96211868_CHECK
#define PR_96211868_CHECK TARGET_OS_OSX
#endif

__attribute__ ((visibility ("hidden")))
bool __ftell_conformance_fix = true;

#if PR_96211868_CHECK
static bool
__chk_ftell_skip_conformance(const struct mach_header *mh) {
  return (dyld_get_active_platform() == PLATFORM_MACOS &&
      !dyld_sdk_at_least(mh, dyld_platform_version_macOS_13_0));
}
#endif

/* Initializer for libc stdio */
__attribute__ ((visibility ("hidden")))
void __stdio_init(void) {
#if PR_96211868_CHECK
    const struct mach_header *hdr = (struct mach_header *)_NSGetMachExecuteHeader();

    if (__chk_ftell_skip_conformance(hdr)) {
        __ftell_conformance_fix = false;
    }
#endif /* PR_96211868_CHECK */
}