/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *  BLIsMountHFS.c
 *  bless
 *
 *  Created by shantonu on Mon Jul 16 2001.
 *  Copyright (c) 2001 __CompanyName__. All rights reserved.
 *
 */

#include <sys/mount.h>
#include <string.h>

#include "bless.h"

int BLIsMountHFS(BLContext context, unsigned char mountpt[], int *isHFS) {
  struct statfs sc;

    int err;

    err = statfs(mountpt, &sc);
    if(err) {
      contextprintf(context, kBLLogLevelError,  "Could not statfs() %s\n", mountpt );
      return 1;
    }
    
    *isHFS = ( !strcmp(sc.f_fstypename, "hfs") ? 1 : 0);
    
    return 0;
}
