/*
 * Copyright (c) 1999-2003 Apple Computer, Inc. All rights reserved.
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
/* Copyright 1998 Apple Computer, Inc. */

#include "SYS.h"

#define	__APPLE_API_PRIVATE
#include <machine/cpu_capabilities.h>
#undef	__APPLE_API_PRIVATE

MI_ENTRY_POINT(___commpage_gettimeofday)
    ba	_COMM_PAGE_GETTIMEOFDAY


/* This syscall is special cased: the timeval is returned in r3/r4.
 * Note also that the "seconds" field of the timeval is a long, so
 * it's size is mode dependent.
 */
MI_ENTRY_POINT(___ppc_gettimeofday)
    mr      r12,r3              // save ptr to timeval
    SYSCALL_NONAME(gettimeofday,0)
	mr.     r12,r12             // was timeval ptr null?
	beq     3f
	stg     r3,0(r12)           // "stw" in 32-bit mode, "std" in 64-bit mode
	stw     r4,GPR_BYTES(r12)
	li      r3,0
3:
	blr

