/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 * 386/thread.c
 *
 * Cproc startup for 386 MTHREAD implementation.
 */

#include <cthreads.h>
#include <mach/mach.h>
#include <mach/i386/thread_status.h>
#include "cthread_internals.h"

void
_pthread_set_self(p)
	cproc_t	p;
{
	asm("pushl	%0" : : "m" (p));
	asm("pushl	$0");			// fake the kernel out
	asm("movl	$1, %%eax" : : : "ax");
	asm("lcall	$0x3b, $0");
	asm("addl	$0x8, %%esp" ::);
}

void *
pthread_self()
{
	asm("movl	$0, %eax");
	asm("lcall	$0x3b, $0");
}
