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
/*
 * assymdefs.c -- list of symbols to #define in assym.h
 */

#import <architecture/ppc/basic_regs.h>
#import <architecture/ppc/fp_regs.h>

#define	__TARGET_ARCHITECTURE__ "ppc"
#import <signal.h>
#import "sigcatch.h"

#import	"genassym.h"

void
assymdefs(void)
{
    comment(MAJOR, "Structure Offsets");
    
    comment(MINOR, "struct sigcontext offsets and sizes");
    PRINT_SIZEOF(struct sigcontext);
    PRINT_OFFSET(struct sigcontext *, sc_regs_saved);
    PRINT_OFFSET(struct sigcontext *, sc_sp);
    PRINT_OFFSET(struct sigcontext *, sc_cia);
    PRINT_OFFSET(struct sigcontext *, sc_a0);
    PRINT_OFFSET(struct sigcontext *, sc_a1);
    PRINT_OFFSET(struct sigcontext *, sc_a2);
    PRINT_OFFSET(struct sigcontext *, sc_ep);
    PRINT_OFFSET(struct sigcontext *, sc_mq);
    PRINT_OFFSET(struct sigcontext *, sc_lr);
    PRINT_OFFSET(struct sigcontext *, sc_cr);
    PRINT_OFFSET(struct sigcontext *, sc_ctr);
    PRINT_OFFSET(struct sigcontext *, sc_xer);
    PRINT_OFFSET(struct sigcontext *, sc_fpscr);
    PRINT_OFFSET(struct sigcontext *, sc_zt);
    PRINT_OFFSET(struct sigcontext *, sc_a3);
    PRINT_OFFSET(struct sigcontext *, sc_a4);
    PRINT_OFFSET(struct sigcontext *, sc_a5);
    PRINT_OFFSET(struct sigcontext *, sc_a6);
    PRINT_OFFSET(struct sigcontext *, sc_a7);
    PRINT_OFFSET(struct sigcontext *, sc_at);
    PRINT_OFFSET(struct sigcontext *, sc_ft0);
    PRINT_OFFSET(struct sigcontext *, sc_fa0);
    PRINT_OFFSET(struct sigcontext *, sc_fa1);
    PRINT_OFFSET(struct sigcontext *, sc_fa2);
    PRINT_OFFSET(struct sigcontext *, sc_fa3);
    PRINT_OFFSET(struct sigcontext *, sc_fa4);
    PRINT_OFFSET(struct sigcontext *, sc_fa5);
    PRINT_OFFSET(struct sigcontext *, sc_fa6);
    PRINT_OFFSET(struct sigcontext *, sc_fa7);
    PRINT_OFFSET(struct sigcontext *, sc_fa8);
    PRINT_OFFSET(struct sigcontext *, sc_fa9);
    PRINT_OFFSET(struct sigcontext *, sc_fa10);
    PRINT_OFFSET(struct sigcontext *, sc_fa11);
    PRINT_OFFSET(struct sigcontext *, sc_fa12);
    PRINT_OFFSET(struct sigcontext *, sc_s17);
    PRINT_OFFSET(struct sigcontext *, sc_s16);
    PRINT_OFFSET(struct sigcontext *, sc_s15);
    PRINT_OFFSET(struct sigcontext *, sc_s14);
    PRINT_OFFSET(struct sigcontext *, sc_s13);
    PRINT_OFFSET(struct sigcontext *, sc_s12);
    PRINT_OFFSET(struct sigcontext *, sc_s11);
    PRINT_OFFSET(struct sigcontext *, sc_s10);
    PRINT_OFFSET(struct sigcontext *, sc_s9);
    PRINT_OFFSET(struct sigcontext *, sc_s8);
    PRINT_OFFSET(struct sigcontext *, sc_s7);
    PRINT_OFFSET(struct sigcontext *, sc_s6);
    PRINT_OFFSET(struct sigcontext *, sc_s5);
    PRINT_OFFSET(struct sigcontext *, sc_s4);
    PRINT_OFFSET(struct sigcontext *, sc_s3);
    PRINT_OFFSET(struct sigcontext *, sc_s2);
    PRINT_OFFSET(struct sigcontext *, sc_s1);
    PRINT_OFFSET(struct sigcontext *, sc_s0);
    PRINT_OFFSET(struct sigcontext *, sc_toc);
    PRINT_OFFSET(struct sigcontext *, sc_fp);
    PRINT_OFFSET(struct sigcontext *, sc_fs17);
    PRINT_OFFSET(struct sigcontext *, sc_fs16);
    PRINT_OFFSET(struct sigcontext *, sc_fs15);
    PRINT_OFFSET(struct sigcontext *, sc_fs14);
    PRINT_OFFSET(struct sigcontext *, sc_fs13);
    PRINT_OFFSET(struct sigcontext *, sc_fs12);
    PRINT_OFFSET(struct sigcontext *, sc_fs11);
    PRINT_OFFSET(struct sigcontext *, sc_fs10);
    PRINT_OFFSET(struct sigcontext *, sc_fs9);
    PRINT_OFFSET(struct sigcontext *, sc_fs8);
    PRINT_OFFSET(struct sigcontext *, sc_fs7);
    PRINT_OFFSET(struct sigcontext *, sc_fs6);
    PRINT_OFFSET(struct sigcontext *, sc_fs5);
    PRINT_OFFSET(struct sigcontext *, sc_fs4);
    PRINT_OFFSET(struct sigcontext *, sc_fs3);
    PRINT_OFFSET(struct sigcontext *, sc_fs2);
    PRINT_OFFSET(struct sigcontext *, sc_fs1);
    PRINT_OFFSET(struct sigcontext *, sc_fs0);
    PRINT_ENUM(REGS_SAVED_NONE);
    PRINT_ENUM(REGS_SAVED_CALLER);
    PRINT_ENUM(REGS_SAVED_ALL);
    
    comment(MINOR, "struct sigstack offsets and sizes");
    PRINT_SIZEOF(struct sigstack);
    PRINT_OFFSET(struct sigstack *, ss_sp);
    PRINT_OFFSET(struct sigstack *, ss_onstack);
    
    comment(MINOR, "sigcatch_t offsets");
    PRINT_SIZEOF(sigcatch_t);
    PRINT_OFFSET(sigcatch_t *, handler);
//    PRINT_OFFSET(sigcatch_t *, flags);
    PRINT_CONSTANT(SV_SAVE_REGS);
}
