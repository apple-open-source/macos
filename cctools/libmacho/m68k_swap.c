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
#import <mach-o/m68k/swap.h>

void
swap_m68k_thread_state_regs(
struct m68k_thread_state_regs *cpu,
enum NXByteOrder target_byte_sex)
{
    uint32_t i;

	for(i = 0; i < 8; i++)
	    cpu->dreg[i] = NXSwapLong(cpu->dreg[i]);
	for(i = 0; i < 8; i++)
	    cpu->areg[i] = NXSwapLong(cpu->areg[i]);
	cpu->pad0 = NXSwapShort(cpu->pad0);
	cpu->sr = NXSwapShort(cpu->sr);
	cpu->pc = NXSwapLong(cpu->pc);
}

void
swap_m68k_thread_state_68882(
struct m68k_thread_state_68882 *fpu,
enum NXByteOrder target_byte_sex)
{
    uint32_t i, tmp;

	for(i = 0; i < 8; i++){
	                   tmp = NXSwapLong(fpu->regs[i].fp[0]);
	    fpu->regs[i].fp[1] = NXSwapLong(fpu->regs[i].fp[1]);
	    fpu->regs[i].fp[0] = NXSwapLong(fpu->regs[i].fp[2]);
	    fpu->regs[i].fp[2] = tmp;
	}
	fpu->cr = NXSwapLong(fpu->cr);
	fpu->sr = NXSwapLong(fpu->sr);
	fpu->iar = NXSwapLong(fpu->iar);
	fpu->state = NXSwapLong(fpu->state);
}

void
swap_m68k_thread_state_user_reg(
struct m68k_thread_state_user_reg *user_reg,
enum NXByteOrder target_byte_sex)
{
	user_reg->user_reg = NXSwapLong(user_reg->user_reg);
}
#endif /* !defined(RLD) */
