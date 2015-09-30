/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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


//
// These functions should be deprectaed!
// Try to find a better way instead of using them.
//

#ifndef _TOOL_ERRORS_H_
#define _TOOL_ERRORS_H_

#include <stdarg.h>
#include <stdio.h>
#include "SecurityTool/SecurityTool.h"

static inline const char *
sec_errstr(int err)
{
    const char *errString;
    static char buffer[12];
    
    sprintf(buffer, "%d", err);
    errString = buffer;
#if 0
    if (IS_SEC_ERROR(err))
        errString = SECErrorString(err);
    else
        errString = cssmErrorString(err);
#endif
    return errString;
}

static inline void
sec_error(const char *msg, ...)
{
    va_list args;
    
    fprintf(stderr, "%s: ", prog_name);
    
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    
    fprintf(stderr, "\n");
}

static inline void
sec_perror(const char *msg, int err)
{
    sec_error("%s: %s", msg, sec_errstr(err));
}



#endif
