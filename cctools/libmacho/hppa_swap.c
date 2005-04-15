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
#import <mach-o/hppa/swap.h>

void
swap_hppa_integer_thread_state(
struct hp_pa_integer_thread_state *regs,
enum NXByteOrder target_byte_order)
{
	regs->ts_gr1 = NXSwapLong(regs->ts_gr1);
	regs->ts_gr2 = NXSwapLong(regs->ts_gr2);
	regs->ts_gr3 = NXSwapLong(regs->ts_gr3);
	regs->ts_gr4 = NXSwapLong(regs->ts_gr4);
	regs->ts_gr5 = NXSwapLong(regs->ts_gr5);
	regs->ts_gr6 = NXSwapLong(regs->ts_gr6);
	regs->ts_gr7 = NXSwapLong(regs->ts_gr7);
	regs->ts_gr8 = NXSwapLong(regs->ts_gr8);
	regs->ts_gr9 = NXSwapLong(regs->ts_gr9);
	regs->ts_gr10 = NXSwapLong(regs->ts_gr10);
	regs->ts_gr11 = NXSwapLong(regs->ts_gr11);
	regs->ts_gr12 = NXSwapLong(regs->ts_gr12);
	regs->ts_gr13 = NXSwapLong(regs->ts_gr13);
	regs->ts_gr14 = NXSwapLong(regs->ts_gr14);
	regs->ts_gr15 = NXSwapLong(regs->ts_gr15);
	regs->ts_gr16 = NXSwapLong(regs->ts_gr16);
	regs->ts_gr17 = NXSwapLong(regs->ts_gr17);
	regs->ts_gr18 = NXSwapLong(regs->ts_gr18);
	regs->ts_gr19 = NXSwapLong(regs->ts_gr19);
	regs->ts_gr20 = NXSwapLong(regs->ts_gr20);
	regs->ts_gr21 = NXSwapLong(regs->ts_gr21);
	regs->ts_gr22 = NXSwapLong(regs->ts_gr22);
	regs->ts_gr23 = NXSwapLong(regs->ts_gr23);
	regs->ts_gr24 = NXSwapLong(regs->ts_gr24);
	regs->ts_gr25 = NXSwapLong(regs->ts_gr25);
	regs->ts_gr26 = NXSwapLong(regs->ts_gr26);
	regs->ts_gr27 = NXSwapLong(regs->ts_gr27);
	regs->ts_gr28 = NXSwapLong(regs->ts_gr28);
	regs->ts_gr29 = NXSwapLong(regs->ts_gr29);
	regs->ts_gr30 = NXSwapLong(regs->ts_gr30);
	regs->ts_gr31 = NXSwapLong(regs->ts_gr31);
	regs->ts_sr0 = NXSwapLong(regs->ts_sr0);
	regs->ts_sr1 = NXSwapLong(regs->ts_sr1);
	regs->ts_sr2 = NXSwapLong(regs->ts_sr2);
	regs->ts_sr3 = NXSwapLong(regs->ts_sr3);
	regs->ts_sar = NXSwapLong(regs->ts_sar);
}

void swap_hppa_frame_thread_state(
struct hp_pa_frame_thread_state *frame,
enum NXByteOrder target_byte_order)
{
	frame->ts_pcsq_front = NXSwapLong(frame->ts_pcsq_front);
	frame->ts_pcsq_back = NXSwapLong(frame->ts_pcsq_back);
	frame->ts_pcoq_front = NXSwapLong(frame->ts_pcoq_front);
	frame->ts_pcoq_back = NXSwapLong(frame->ts_pcoq_back);
	frame->ts_psw = NXSwapLong(frame->ts_psw);
	frame->ts_unaligned_faults = NXSwapLong(frame->ts_unaligned_faults);
	frame->ts_fault_address = NXSwapLong(frame->ts_fault_address);
	frame->ts_step_range_start = NXSwapLong(frame->ts_step_range_start);
	frame->ts_step_range_stop = NXSwapLong(frame->ts_step_range_stop);
}

void swap_hppa_fp_thread_state(
struct hp_pa_fp_thread_state *fp,
enum NXByteOrder target_byte_order)
{
	fp->ts_fp0 = NXSwapDouble(fp->ts_fp0);
	fp->ts_fp1 = NXSwapDouble(fp->ts_fp1);
	fp->ts_fp2 = NXSwapDouble(fp->ts_fp2);
	fp->ts_fp3 = NXSwapDouble(fp->ts_fp3);
	fp->ts_fp4 = NXSwapDouble(fp->ts_fp4);
	fp->ts_fp5 = NXSwapDouble(fp->ts_fp5);
	fp->ts_fp6 = NXSwapDouble(fp->ts_fp6);
	fp->ts_fp7 = NXSwapDouble(fp->ts_fp7);
	fp->ts_fp8 = NXSwapDouble(fp->ts_fp8);
	fp->ts_fp9 = NXSwapDouble(fp->ts_fp9);
	fp->ts_fp10 = NXSwapDouble(fp->ts_fp10);
	fp->ts_fp11 = NXSwapDouble(fp->ts_fp11);
	fp->ts_fp12 = NXSwapDouble(fp->ts_fp12);
	fp->ts_fp13 = NXSwapDouble(fp->ts_fp13);
	fp->ts_fp14 = NXSwapDouble(fp->ts_fp14);
	fp->ts_fp15 = NXSwapDouble(fp->ts_fp15);
	fp->ts_fp16 = NXSwapDouble(fp->ts_fp16);
	fp->ts_fp17 = NXSwapDouble(fp->ts_fp17);
	fp->ts_fp18 = NXSwapDouble(fp->ts_fp18);
	fp->ts_fp19 = NXSwapDouble(fp->ts_fp19);
	fp->ts_fp20 = NXSwapDouble(fp->ts_fp20);
	fp->ts_fp21 = NXSwapDouble(fp->ts_fp21);
	fp->ts_fp22 = NXSwapDouble(fp->ts_fp22);
	fp->ts_fp23 = NXSwapDouble(fp->ts_fp23);
	fp->ts_fp24 = NXSwapDouble(fp->ts_fp24);
	fp->ts_fp25 = NXSwapDouble(fp->ts_fp25);
	fp->ts_fp26 = NXSwapDouble(fp->ts_fp26);
	fp->ts_fp27 = NXSwapDouble(fp->ts_fp27);
	fp->ts_fp28 = NXSwapDouble(fp->ts_fp28);
	fp->ts_fp29 = NXSwapDouble(fp->ts_fp29);
	fp->ts_fp30 = NXSwapDouble(fp->ts_fp30);
	fp->ts_fp31 = NXSwapDouble(fp->ts_fp31);
}
#endif /* !defined(RLD) */
