/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
/* Copyright (c) 1992, 1997 NeXT Software, Inc.  All rights reserved.
 *
 *	File:	libc/gen/ppc/isinf.c
 *	Author:	Derek B Clegg, NeXT Software, Inc.
 *
 * HISTORY
*  24-Jan-1997 Umesh Vaishampayan (umeshv@NeXT.com)
*	Ported to PPC.
 *  11-Nov-92  Derek B Clegg (dclegg@next.com)
 *	Created.
 *
 * int isinf(double value);
 *
 * Returns 1 if `value' is equal to positive IEEE infinity, -1 if `value'
 * is equal to negative IEEE infinity, 0 otherwise.
 *
 * An IEEE infinity is a double value with the maximum biased exponent value
 * (2047) and a zero fraction value.
 */
#import "fp.h"

int
isinf(double value)
{
    union dbl d;

    d.value = value;
    if (d.u[0] == 0x7FF00000 && d.u[1] == 0)
	return 1;
    if (d.u[0] == 0xFFF00000 && d.u[1] == 0)
	return -1;
    return 0;
}
