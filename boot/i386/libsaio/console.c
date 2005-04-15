/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
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
 * Mach Operating System
 * Copyright (c) 1990 Carnegie-Mellon University
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 * 			INTEL CORPORATION PROPRIETARY INFORMATION
 *
 *	This software is supplied under the terms of a license  agreement or 
 *	nondisclosure agreement with Intel Corporation and may not be copied 
 *	nor disclosed except in accordance with the terms of that agreement.
 *
 *	Copyright 1988, 1989 Intel Corporation
 */

/*
 * Copyright 1993 NeXT, Inc.
 * All rights reserved.
 */

#include "libsaio.h"

BOOL gVerboseMode;
BOOL gErrors;

/*
 * write one character to console
 */
void putchar(int c)
{
	if ( c == '\t' )
	{
		for (c = 0; c < 8; c++) putc(' ');
		return;
	}

	if ( c == '\n' )
    {
		putc('\r');
    }

	putc(c);
}

int getc()
{
    int c = bgetc();

    if ((c & 0xff) == 0)
        return c;
    else
        return (c & 0xff);
}

// Read and echo a character from console.  This doesn't echo backspace
// since that screws up higher level handling

int getchar()
{
	register int c = getc();

	if ( c == '\r' ) c = '\n';

	if ( c >= ' ' && c < 0x7f) putchar(c);
	
	return (c);
}

int printf(const char * fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    prf(fmt, ap, putchar, 0);
    va_end(ap);
    return 0;
}

int verbose(const char * fmt, ...)
{
    va_list ap;
    
    if (gVerboseMode)
    {
        va_start(ap, fmt);
        prf(fmt, ap, putchar, 0);
        va_end(ap);
    }
    return(0);
}

int error(const char * fmt, ...)
{
    va_list ap;
    gErrors = YES;
    va_start(ap, fmt);
    prf(fmt, ap, putchar, 0);
    va_end(ap);
    return(0);
}

void stop(const char * msg)
{
    error("\n%s\n", msg);
    halt();
}
