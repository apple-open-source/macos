/*
 * kextaudit.c - code for forwarding kext load information to bridgeOS
 *               over SMC to allow for certain system policy decisions.
 *
 * Copyright (c) 2018 Apple Computer, Inc. All rights reserved.
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

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/kext/OSKext.h>
#include <IOKit/kext/OSKextPrivate.h>

#include "security.h"
#include "kext_tools_util.h"

/* KextAuditLoadCallback
 *
 * Summary:
 *
 * This function should be called whenever a kext is about to be loaded by
 * IOKitUser, after every check has been made on the kext to make sure that
 * it's authentic, has its dependencies resolved, and is more or less ready
 * to go. We want a callback instead of calling this before OSKextLoad()
 * to make sure that we don't accidentally audit kexts that aren't even
 * getting loaded.
 *
 * Arguments: The kext that's about to be loaded.
 *
 * Returns: Success condition.
 *
 * NB: ***If this function returns false, the kext load will fail!***
 *
 */
Boolean KextAuditLoadCallback(OSKextRef theKext)
{
        (void)theKext;
        return true;
}
