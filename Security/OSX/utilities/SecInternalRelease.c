/*
 * Copyright (c) 2015-2016 Apple Inc. All Rights Reserved.
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

#include <dispatch/dispatch.h>
#include <AssertMacros.h>
#include <strings.h>
#include <os/variant_private.h>
#include <sys/sysctl.h>

#include "debugging.h"
#include "SecInternalReleasePriv.h"

bool SecAreQARootCertificatesEnabled(void) {
    static bool sQACertsEnabled = false;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        int value = 0;
        size_t size = sizeof(value);
        int ret = sysctlbyname("security.mac.amfi.qa_root_certs_allowed", &value, &size, NULL, 0);
        if (ret == 0) {
            sQACertsEnabled = (value == 1);
        } else {
            secerror("Unable to check QA certificate status: %d", ret);
        }
    });
    return sQACertsEnabled;
}

bool SecIsInternalRelease(void) {
    static bool isInternal = false;


    return isInternal;
}

