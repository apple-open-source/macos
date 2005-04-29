/*
 * Copyright (c) 1999-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 2.0 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright 1993 NeXT, Inc.
 * All rights reserved.
 */

#include "libsa.h"

struct putc_info {
    char * str;
    char * last_str;
};

static void
sputc(int c, struct putc_info * pi)
{
    if (pi->last_str)
        if (pi->str == pi->last_str) {
            *(pi->str) = '\0';
            return;
        }
    *(pi->str)++ = c;
}

/*VARARGS1*/
int sprintf(char * str, const char * fmt, ...)
{
    va_list ap;
    struct putc_info pi;

    va_start(ap, fmt);
    pi.str = str;
    pi.last_str = 0;
    prf(fmt, ap, sputc, &pi);
    *pi.str = '\0';
    va_end(ap);
    return 0;
}

/*VARARGS1*/
int slvprintf(char * str, int len, const char * fmt, va_list ap)
{
    struct putc_info pi;
    pi.str = str;
    pi.last_str = str + len - 1;
    prf(fmt, ap, sputc, &pi);
    *pi.str = '\0';
    return (pi.str - str);
}
