/*
 * Copyright (c) 2001-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 *  Copyright (c) 2001-2003 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLContextPrint.c,v 1.8 2003/07/22 15:58:34 ssen Exp $
 *
 *  $Log: BLContextPrint.c,v $
 *  Revision 1.8  2003/07/22 15:58:34  ssen
 *  APSL 2.0
 *
 *  Revision 1.7  2003/04/19 00:11:12  ssen
 *  Update to APSL 1.2
 *
 *  Revision 1.6  2003/04/16 23:57:33  ssen
 *  Update Copyrights
 *
 *  Revision 1.5  2003/03/22 06:31:01  ssen
 *  add version field
 *
 *  Revision 1.4  2002/07/27 02:58:25  ssen
 *  use vsnprintf
 *
 *  Revision 1.3  2002/07/27 02:38:27  ssen
 *  don't use vasprintf on Puma
 *
 *  Revision 1.2  2002/06/11 00:50:48  ssen
 *  All function prototypes need to use BLContextPtr. This is really
 *  a minor change in all of the files.
 *
 *  Revision 1.1  2002/04/27 17:55:17  ssen
 *  Rewrite output logic to format the string before sending of to logger
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
#if OSX_TARGET < 1020
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
