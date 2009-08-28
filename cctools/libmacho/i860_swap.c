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
#import <mach-o/i860/swap.h>

void
swap_i860_thread_state_regs(
struct i860_thread_state_regs *cpu,
enum NXByteOrder target_byte_sex)
{
    int i;

	for(i = 0; i < 31; i++)
	    cpu->ireg[i] = NXSwapLong(cpu->ireg[i]);
	for(i = 0; i < 30; i++)
	    cpu->freg[i] = NXSwapLong(cpu->freg[i]);
	cpu->psr = NXSwapLong(cpu->psr);
	cpu->epsr = NXSwapLong(cpu->epsr);
	cpu->db = NXSwapLong(cpu->db);
	cpu->pc = NXSwapLong(cpu->pc);
	cpu->_padding_ = NXSwapLong(cpu->_padding_);
	cpu->Mres3 = NXSwapDouble(cpu->Mres3);
	cpu->Ares3 = NXSwapDouble(cpu->Ares3);
	cpu->Mres2 = NXSwapDouble(cpu->Mres2);
	cpu->Ares2 = NXSwapDouble(cpu->Ares2);
	cpu->Mres1 = NXSwapDouble(cpu->Mres1);
	cpu->Ares1 = NXSwapDouble(cpu->Ares1);
	cpu->Ires1 = NXSwapDouble(cpu->Ires1);
	cpu->Lres3m = NXSwapDouble(cpu->Lres3m);
	cpu->Lres2m = NXSwapDouble(cpu->Lres2m);
	cpu->Lres1m = NXSwapDouble(cpu->Lres1m);
	cpu->KR = NXSwapDouble(cpu->KR);
	cpu->KI = NXSwapDouble(cpu->KI);
	cpu->T = NXSwapDouble(cpu->T);
	cpu->Fsr3 = NXSwapLong(cpu->Fsr3);
	cpu->Fsr2 = NXSwapLong(cpu->Fsr2);
	cpu->Fsr1 = NXSwapLong(cpu->Fsr1);
	cpu->Mergelo32 = NXSwapLong(cpu->Mergelo32);
	cpu->Mergehi32 = NXSwapLong(cpu->Mergehi32);
}
#endif /* !defined(RLD) */
