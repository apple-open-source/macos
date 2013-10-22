/*
 * Copyright (c) 2011-12 Apple Inc. All Rights Reserved.
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
 * Copyright (c) 2005 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "ossl-config.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct strpool {
    char *str;
    size_t len;
};

#ifndef ROKEN_LIB_FUNCTION
#define	ROKEN_LIB_FUNCTION
#endif
#ifndef	ROKEN_LIB_CALL
#define	ROKEN_LIB_CALL
#endif

/*
 *
 */

ROKEN_LIB_FUNCTION void ROKEN_LIB_CALL
strpoolfree(struct strpool *p)
{
    if (p->str) {
	free(p->str);
	p->str = NULL;
    }
    free(p);
}

/*
 *
 */

ROKEN_LIB_FUNCTION struct strpool * ROKEN_LIB_CALL
strpoolprintf(struct strpool *p, const char *fmt, ...)
{
    va_list ap;
    char *str, *str2;
    int len;

    if (p == NULL) {
	p = malloc(sizeof(*p));
	if (p == NULL)
	    return NULL;
	p->str = NULL;
	p->len = 0;
    }
    va_start(ap, fmt);
    len = vasprintf(&str, fmt, ap);
    va_end(ap);
    if (str == NULL) {
	strpoolfree(p);
	return NULL;
    }
    str2 = realloc(p->str, len + p->len + 1);
    if (str2 == NULL) {
	strpoolfree(p);
	return NULL;
    }
    p->str = str2;
    memcpy(p->str + p->len, str, len + 1);
    p->len += len;
    free(str);
    return p;
}

/*
 *
 */

ROKEN_LIB_FUNCTION char * ROKEN_LIB_CALL
strpoolcollect(struct strpool *p)
{
    char *str;
    if (p == NULL)
	return strdup("");
    str = p->str;
    p->str = NULL;
    free(p);
    return str;
}
