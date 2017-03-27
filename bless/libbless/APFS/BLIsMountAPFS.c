/*
 * Copyright (c) 2001-2016 Apple Inc. All Rights Reserved.
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
 *  BLIsMountAPFS.c
 *  bless
 *
 *  Copyright (c) 2001-2016 Apple Inc. All Rights Reserved.
 *
 */

#include <sys/mount.h>
#include <string.h>

#include "bless.h"
#include "bless_private.h"

int BLIsMountAPFS(BLContextPtr context, const char * mountpt, int *isAPFS) {
    struct statfs sc;
    
    int err;
    
    err = statfs(mountpt, &sc);
    if(err) {
        contextprintf(context, kBLLogLevelError,  "Could not statfs() %s\n", mountpt );
        return 1;
    }
    
    *isAPFS = ( !strcmp(sc.f_fstypename, "apfs") ? 1 : 0);
    
    return 0;
}
