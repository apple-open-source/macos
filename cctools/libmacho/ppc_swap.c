/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
#ifndef RLD

#define __cr cr
#define __ctr ctr
#define __dar dar
#define __dsisr dsisr
#define __exception exception
#define __fpregs fpregs
#define __fpscr fpscr
#define __fpscr_pad fpscr_pad
#define __lr lr
#define __mq mq
#define __pad0 pad0
#define __pad1 pad1
#define __r0 r0
#define __r1 r1
#define __r10 r10
#define __r11 r11
#define __r12 r12
#define __r13 r13
#define __r14 r14
#define __r15 r15
#define __r16 r16
#define __r17 r17
#define __r18 r18
#define __r19 r19
#define __r2 r2
#define __r20 r20
#define __r21 r21
#define __r22 r22
#define __r23 r23
#define __r24 r24
#define __r25 r25
#define __r26 r26
#define __r27 r27
#define __r28 r28
#define __r29 r29
#define __r3 r3
#define __r30 r30
#define __r31 r31
#define __r4 r4
#define __r5 r5
#define __r6 r6
#define __r7 r7
#define __r8 r8
#define __r9 r9
#define __srr0 srr0
#define __srr1 srr1
#define __vrsave vrsave
#define __xer xer

#import <mach-o/ppc/swap.h>
#import <architecture/nrw/reg_help.h>

void
swap_ppc_thread_state_t(
ppc_thread_state_t *cpu,
enum NXByteOrder target_byte_sex)
{
	cpu->srr0 = NXSwapLong(cpu->srr0);
	cpu->srr1 = NXSwapLong(cpu->srr1);
	cpu->r0 = NXSwapLong(cpu->r0);
	cpu->r1 = NXSwapLong(cpu->r1);
	cpu->r2 = NXSwapLong(cpu->r2);
	cpu->r3 = NXSwapLong(cpu->r3);
	cpu->r4 = NXSwapLong(cpu->r4);
	cpu->r5 = NXSwapLong(cpu->r5);
	cpu->r6 = NXSwapLong(cpu->r6);
	cpu->r7 = NXSwapLong(cpu->r7);
	cpu->r8 = NXSwapLong(cpu->r8);
	cpu->r9 = NXSwapLong(cpu->r9);
	cpu->r10 = NXSwapLong(cpu->r10);
	cpu->r11 = NXSwapLong(cpu->r11);
	cpu->r12 = NXSwapLong(cpu->r12);
	cpu->r13 = NXSwapLong(cpu->r13);
	cpu->r14 = NXSwapLong(cpu->r14);
	cpu->r15 = NXSwapLong(cpu->r15);
	cpu->r16 = NXSwapLong(cpu->r16);
	cpu->r17 = NXSwapLong(cpu->r17);
	cpu->r18 = NXSwapLong(cpu->r18);
	cpu->r19 = NXSwapLong(cpu->r19);
	cpu->r20 = NXSwapLong(cpu->r20);
	cpu->r21 = NXSwapLong(cpu->r21);
	cpu->r22 = NXSwapLong(cpu->r22);
	cpu->r23 = NXSwapLong(cpu->r23);
	cpu->r24 = NXSwapLong(cpu->r24);
	cpu->r25 = NXSwapLong(cpu->r25);
	cpu->r26 = NXSwapLong(cpu->r26);
	cpu->r27 = NXSwapLong(cpu->r27);
	cpu->r28 = NXSwapLong(cpu->r28);
	cpu->r29 = NXSwapLong(cpu->r29);
	cpu->r30 = NXSwapLong(cpu->r30);
	cpu->r31 = NXSwapLong(cpu->r31);
	cpu->lr  = NXSwapLong(cpu->lr);
	cpu->cr  = NXSwapLong(cpu->cr);
	cpu->xer = NXSwapLong(cpu->xer);
	cpu->ctr = NXSwapLong(cpu->ctr);
	cpu->mq  = NXSwapLong(cpu->mq);
	cpu->vrsave = NXSwapLong(cpu->vrsave);

}

void
swap_ppc_float_state_t(
ppc_float_state_t *fpu,
enum NXByteOrder target_byte_sex)
{
    uint32_t i;
	
	for(i = 0; i < 32; i++)
	    fpu->fpregs[i] = NXSwapDouble(fpu->fpregs[i]);

	fpu->fpscr_pad = NXSwapLong(fpu->fpscr_pad);
	fpu->fpscr = NXSwapLong(fpu->fpscr);
}

void
swap_ppc_exception_state_t(
ppc_exception_state_t *state,
enum NXByteOrder target_byte_sex)
{
    uint32_t i;
	
	state->dar = NXSwapLong(state->dar);
	state->dsisr = NXSwapLong(state->dsisr);
	state->exception = NXSwapLong(state->exception);
	state->pad0 = NXSwapLong(state->pad0);

	for(i = 0; i < 4; i++)
	    state->pad1[i] = NXSwapLong(state->pad1[i]);
}
#endif /* !defined(RLD) */
