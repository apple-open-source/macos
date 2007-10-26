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
 *  BLContextPrint.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Fri Apr 25 2002.
 *  Copyright (c) 2001-2007 Apple Inc. All Rights Reserved.
 *
 *  $Id: BLContextPrint.c,v 1.13 2006/02/20 22:49:56 ssen Exp $
 *
 */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
 
#include "bless.h"
#include "bless_private.h"
 
int contextprintf(BLContextPtr context, int loglevel, char const *fmt, ...) {
    int ret;
    char *out;
    va_list ap;
    
    
    if(!context) return 0;

    
    
    if(context->version == 0 && context->logstring) {

        va_start(ap, fmt);
#if NO_VASPRINTF
	out = malloc(1024);
	ret = vsnprintf(out, 1024, fmt, ap);  
#else
	ret = vasprintf(&out, fmt, ap);  
#endif
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
