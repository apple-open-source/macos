/*
 * Copyright (c) 2001-2007 Apple Inc. All Rights Reserved.
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
 *  output.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Wed Feb 20 2002.
 *  Copyright (c) 2001-2007 Apple Inc. All Rights Reserved.
 *
 *  $Id: output.c,v 1.18 2005/02/03 00:42:22 ssen Exp $
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <sys/types.h>
#include "enums.h"
#include "structs.h"
#include "bless.h"
#include "protos.h"

int blesslog(void *context, int loglevel, const char *string) {
    int ret = 0;
    int willprint = 0;
    FILE *out = NULL;

    struct blesscon *con = (struct blesscon *)context;

    switch(loglevel) {
    case kBLLogLevelNormal:
        if(con->quiet) {
            willprint = 0;
        } else {
            willprint = 1;
        }
        out = stdout;
        break;
    case kBLLogLevelVerbose:
        if(con->quiet) {
            willprint = 0;
        } else if(con->verbose) {
            willprint = 1;
        } else {
            willprint = 0;
        }
        out = stderr;
        break;
    case kBLLogLevelError:
        willprint = 1;
        out = stderr;
        break;
    }

    if(willprint) {
        ret = fputs(string, out);
    }
    return ret;
}
