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
/*
 * assymdefs.c -- list of symbols to #define in assym.h
 */
#import	<bsd/ppc/setjmp.h>
#define	__TARGET_ARCHITECTURE__ "ppc"
#import <signal.h>
#import <assert.h>
#import <bsd/stddef.h>
#import	"genassym.h"

void
assymdefs(void)
{
    /* This is required for `setjmp' to work. */
    assert(offsetof(struct _jmp_buf, csr[18])
	   == offsetof(struct _jmp_buf, fp));

    comment(MAJOR, "Structure Offsets");
    comment(MINOR, "jmpbuf_t offsets and constants");

    PRINT_OFFSET(struct _jmp_buf *, magic);
    PRINT_OFFSET(struct _jmp_buf *, sp);
    PRINT_OFFSET(struct _jmp_buf *, csr[0]);
    PRINT_OFFSET(struct _jmp_buf *, csr[17]);
    PRINT_OFFSET(struct _jmp_buf *, fp);
    PRINT_OFFSET(struct _jmp_buf *, toc);
    PRINT_OFFSET(struct _jmp_buf *, cr);
    PRINT_OFFSET(struct _jmp_buf *, lr);
    PRINT_OFFSET(struct _jmp_buf *, fpr[0]);
    PRINT_OFFSET(struct _jmp_buf *, fpr[1]);
    PRINT_OFFSET(struct _jmp_buf *, fpr[2]);
    PRINT_OFFSET(struct _jmp_buf *, fpr[3]);
    PRINT_OFFSET(struct _jmp_buf *, fpr[4]);
    PRINT_OFFSET(struct _jmp_buf *, fpr[5]);
    PRINT_OFFSET(struct _jmp_buf *, fpr[6]);
    PRINT_OFFSET(struct _jmp_buf *, fpr[7]);
    PRINT_OFFSET(struct _jmp_buf *, fpr[8]);
    PRINT_OFFSET(struct _jmp_buf *, fpr[9]);
    PRINT_OFFSET(struct _jmp_buf *, fpr[10]);
    PRINT_OFFSET(struct _jmp_buf *, fpr[11]);
    PRINT_OFFSET(struct _jmp_buf *, fpr[12]);
    PRINT_OFFSET(struct _jmp_buf *, fpr[13]);
    PRINT_OFFSET(struct _jmp_buf *, fpr[14]);
    PRINT_OFFSET(struct _jmp_buf *, fpr[15]);
    PRINT_OFFSET(struct _jmp_buf *, fpr[16]);
    PRINT_OFFSET(struct _jmp_buf *, fpr[17]);
    PRINT_OFFSET(struct _jmp_buf *, fpscr);
    PRINT_OFFSET(struct _jmp_buf *, sig_onstack);
    PRINT_OFFSET(struct _jmp_buf *, sig_mask);
    PRINT_CONSTANT(_JMP_BUF_MAGICNUM);
    
    comment(MINOR, "sigcontext offsets, sizes, and constants");
    PRINT_SIZEOF(struct sigcontext);
    PRINT_OFFSET(struct sigcontext *, sc_onstack);
    PRINT_OFFSET(struct sigcontext *, sc_mask);
    PRINT_OFFSET(struct sigcontext *, sc_sp);
    PRINT_OFFSET(struct sigcontext *, sc_cia);
    PRINT_OFFSET(struct sigcontext *, sc_regs_saved);
    PRINT_OFFSET(struct sigcontext *, sc_a0);
    PRINT_ENUM(REGS_SAVED_NONE);
    
    comment(MINOR, "struct sigstack offsets and sizes");
    PRINT_SIZEOF(struct sigstack);
    PRINT_OFFSET(struct sigstack *, ss_sp);
    PRINT_OFFSET(struct sigstack *, ss_onstack);
}
