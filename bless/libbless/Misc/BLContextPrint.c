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
 *  BLContextPrint.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Fri Apr 25 2002.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLContextPrint.c,v 1.1 2002/04/27 17:55:17 ssen Exp $
 *
 *  $Log: BLContextPrint.c,v $
 *  Revision 1.1  2002/04/27 17:55:17  ssen
 *  Rewrite output logic to format the string before sending of to logger
 *
 */

 #include <stdlib.h>
 #include <stdarg.h>
 #include <stdio.h>
 
 #include "bless.h"
 
 
 
int contextprintf(BLContext context, int loglevel, char const *fmt, ...) {
    int ret;
    char *out;
    va_list ap;

    if(context && context->logstring) {

        va_start(ap, fmt);
        ret = vasprintf(&out, fmt, ap);  
        va_end(ap);
    
        if((ret == -1) || (out == NULL)) {
            return context->logstring(context->logrefcon, loglevel, "Memory error, log entry not available");
        }

        ret = context->logstring(context->logrefcon, loglevel, out);
        free(out);
        return ret;
    }

    return 0;
}
