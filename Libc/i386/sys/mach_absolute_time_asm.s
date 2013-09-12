/*
 * Copyright (c) 2003-2007 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 * 
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#include <sys/appleapiopts.h>
#include <machine/cpu_capabilities.h>


/* return mach_absolute_time in %edx:%eax
 *
 * The algorithm we use is:
 *
 *	ns = ((((rdtsc - rnt_tsc_base)<<rnt_shift)*rnt_tsc_scale) / 2**32) + rnt_ns_base;
 *
 * rnt_shift, a constant computed during initialization, is the smallest value for which:
 *
 *  (tscFreq << rnt_shift) > SLOW_TSC_THRESHOLD
 *
 * Where SLOW_TSC_THRESHOLD is about 10e9.  Since most processor's tscFreq is greater
 * than 1GHz, rnt_shift is usually 0.  rnt_tsc_scale is also a 32-bit constant:
 *
 *	rnt_tsc_scale = (10e9 * 2**32) / (tscFreq << rnt_shift);
 */

	.globl _mach_absolute_time
_mach_absolute_time:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%esi
	pushl	%ebx

0:
	movl	_COMM_PAGE_NT_GENERATION,%esi	/* get generation (0 if being changed) */
	testl	%esi,%esi			/* if being updated, loop until stable */
	jz	0b

	lfence
	rdtsc					/* get TSC in %edx:%eax */
	lfence

	subl	_COMM_PAGE_NT_TSC_BASE,%eax
	sbbl	_COMM_PAGE_NT_TSC_BASE+4,%edx
    
    /*
     * Prior to supporting "slow" processors, xnu always set _NT_SHIFT to 32.
     * Now it defaults to 0, unless the processor is slow.  The shifts
     * below implicitly mask the count down to 5 bits, handling either default.
     */
	movl    _COMM_PAGE_NT_SHIFT,%ecx
	shldl   %cl,%eax,%edx			/* shift %edx left, filling in from %eax */
	shll    %cl,%eax			/* finish shifting %edx:%eax left by _COMM_PAGE_NT_SHIFT bits */

	movl	_COMM_PAGE_NT_SCALE,%ecx

	movl	%edx,%ebx
	mull	%ecx
	movl	%ebx,%eax
	movl	%edx,%ebx
	mull	%ecx
	addl	%ebx,%eax
	adcl	$0,%edx

	addl	_COMM_PAGE_NT_NS_BASE,%eax
	adcl	_COMM_PAGE_NT_NS_BASE+4,%edx

	cmpl	_COMM_PAGE_NT_GENERATION,%esi	/* have the parameters changed? */
	jne	0b				/* yes, loop until stable */

	popl	%ebx
	popl	%esi
	popl	%ebp
	ret
