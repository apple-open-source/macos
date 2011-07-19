/*
 * Copyright (c) 2007, 2011 Apple Inc. All rights reserved.
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

#include <architecture/ppc/mode_independent_asm.h>
#include <sys/syscall.h>

#define UC_TRAD			1
#define UC_TRAD64		20
#define UC_TRAD64_VEC		25
#define UC_FLAVOR		30
#define UC_FLAVOR_VEC		35
#define UC_FLAVOR64		40
#define UC_FLAVOR64_VEC		45
#define UC_DUAL			50
#define UC_DUAL_VEC		55

/* Structure fields and sizes for ucontext and mcontext.  */
#define UCONTEXT_UC_MCSIZE	MODE_CHOICE (24, 40)
#define UCONTEXT_UC_MCONTEXT	MODE_CHOICE (28, 48)
#define MCONTEXT_SIZE		1032
#define MCONTEXT64_SIZE		1176
#define UC_FLAVOR64_SIZE	600
#define UC_FLAVOR64_VEC_SIZE	MCONTEXT64_SIZE

#define MCONTEXT_ES_EXCEPTION	8
#define MCONTEXT_SS_SRR0	32
#define MCONTEXT_SS_SRR1	36
#define MCONTEXT_SS_R0		40
#define MCONTEXT_SS_CR		168
#define MCONTEXT_SS_XER		172
#define MCONTEXT_SS_LR		176
#define MCONTEXT_SS_CTR		180
#define MCONTEXT_SS_MQ		184
#define MCONTEXT_SS_VRSAVE	188
#define MCONTEXT_FS_FPREGS	192
#define MCONTEXT_FS_FPSCR	448
#define MCONTEXT_VS_SAVE_VR	456
#define MCONTEXT_VS_VSCR	968

#define MCONTEXT64_ES_EXCEPTION	12
#define MCONTEXT64_SS_SRR0	32
#define MCONTEXT64_SS_SRR1	40
#define MCONTEXT64_SS_R0	48
#define MCONTEXT64_SS_CR	304
#define MCONTEXT64_SS_XER	308
#define MCONTEXT64_SS_LR	316
#define MCONTEXT64_SS_CTR	324
#define MCONTEXT64_SS_VRSAVE	332
#define MCONTEXT64_FS_FPREGS	336
#define MCONTEXT64_FS_FPSCR	592
#define MCONTEXT64_VS_SAVE_VR	600
#define MCONTEXT64_VS_VSCR	1112

/* Exception types.  I believe the MCONTEXT_ES_EXCEPTION field is set from
   the address called to handle the exception, for example a
   Program Exception jumps to address 0x00700 and so the field has
   value 7.  */
#define EXCEPTION_DSI 3
#define EXCEPTION_ISI 4
#define EXCEPTION_INTERRUPT 5
#define EXCEPTION_ALIGN 6
#define EXCEPTION_PROGRAM 7
#define EXCEPTION_FPUNAVAIL 8
#define EXCEPTION_DEC 9
#define EXCEPTION_SC 0xC
#define EXCEPTION_TRACE 0xD
#define EXCEPTION_FPASSIST 0xE

/* register allocation:
	r0 : scratch, also used by MI_* macros
	r3 : parameter union __sigaction_u __sigaction_u
	r4 : parameter int sigstyle
	r5 : parameter int sig
	r6 : parameter siginfo_t *sinfo
	r7 : parameter ucontext_t *uctx
	r8 : value of __in_sigtramp
	r9 : &__in_sigtramp
	r12: scratch used by MI_* macros

	r29 : sigstyle
	r28 : uctx
	r27 : uctx->uc_mcontext
	
*/

MI_ENTRY_POINT(__sigtramp)
	/* Save away sigstyle and uctx.  This code doesn't need to
	   restore the callee-saved registers, since sigreturn
	   will do it.  */
	mr r28,r7
	mr r29,r4
#if defined(__DYNAMIC__)
	/* ++__in_sigtramp; */
	MI_GET_ADDRESS (r9, ___in_sigtramp)
	lwz r8,0(r9)
	addi r8,r8,1
	stw r8,0(r9)
#endif
	/* Having this here shortens the unwind tables significantly.  */
	lg r27,UCONTEXT_UC_MCONTEXT(r7)

	/* Call the signal handler.
	   Some variants are not supposed to get the last two parameters,
	   but the test to prevent this is more expensive than just passing
	   them.  */
	mtctr r3
	mr r3,r5
	mr r4,r6
	mr r5,r7
Lcall_start:
	bctrl
Lcall_end:

	/* Call __finish_sigtramp in sigtramp.c to complete processing
	   for ppc, or just return to the kernel using sigtramp for ppc64.  */
	mr r3,r28
	mr r4,r29
	b MODE_CHOICE (___finish_sigtramp, ___sigreturn)
	/* Does not return.  */

/* DWARF unwind table #defines.  */
#define DW_CFA_advance_loc_4 0x44
#define DW_CFA_def_cfa 0x0c
#define DW_CFA_def_cfa_expression 0x0F
#define DW_CFA_expression 0x10
#define DW_CFA_val_expression 0x16
#define DW_CFA_offset(column) 0x80+(column)

/* DWARF expression #defines.  */
#define DW_OP_deref 0x06
#define DW_OP_const1u 0x08
#define DW_OP_dup 0x12
#define DW_OP_drop 0x13
#define DW_OP_over 0x14
#define DW_OP_pick 0x15
#define DW_OP_swap 0x16
#define DW_OP_rot 0x17
#define DW_OP_abs 0x19
#define DW_OP_and 0x1a
#define DW_OP_div 0x1b
#define DW_OP_minus 0x1c
#define DW_OP_mod 0x1d
#define DW_OP_mul 0x1e
#define DW_OP_neg 0x1f
#define DW_OP_not 0x20
#define DW_OP_or 0x21
#define DW_OP_plus 0x22
#define DW_OP_plus_uconst 0x23
#define DW_OP_shl 0x24
#define DW_OP_shr 0x25
#define DW_OP_shra 0x26
#define DW_OP_xor 0x27
#define DW_OP_skip 0x2f
#define DW_OP_bra 0x28
#define DW_OP_eq 0x29
#define DW_OP_ge 0x2A
#define DW_OP_gt 0x2B
#define DW_OP_le 0x2C
#define DW_OP_lt 0x2D
#define DW_OP_ne 0x2E
#define DW_OP_lit(n) 0x30+(n)
#define DW_OP_breg(n) 0x70+(n)
#define DW_OP_deref_size 0x94

/* The location expressions we'll use.  */

#ifdef __ppc__
/* The ppc versions test register 29 for UC_TRAD64, UC_TRAD64_VEC,
   UC_FLAVOR64, UC_FLAVOR64_VEC, and then use the appropriate offset
   off r27 (either the offset for a mcontext or a mcontext64).  

   The expression computed has been somewhat optimised to reduce the size
   of the unwind entries, and is of the form

   (r27 + offs
    + ((r29/10)==UC_TRAD64/10 || (r29/10)==UC_FLAVOR64/10)*(offs64-offs))
*/

/* For when REGNO < 128 and OFFS < 64.  */
#define loc_expr_for_reg_sml(regno, offs, offs64)			\
	.byte DW_CFA_expression, regno, 17 /* block length */,		\
	 DW_OP_breg(27), offs,						\
	  DW_OP_breg(29), 0, DW_OP_lit(10), DW_OP_div,			\
	   DW_OP_dup, DW_OP_lit(UC_TRAD64/10), DW_OP_eq,		\
	    DW_OP_swap, DW_OP_lit(UC_FLAVOR64/10), DW_OP_eq, DW_OP_or,	\
	   DW_OP_const1u, offs64-(offs), DW_OP_mul, DW_OP_plus

/* For when REGNO < 128 and OFFS >= 64.  */
#define loc_expr_for_reg(regno, offs, offs64)				\
	.byte DW_CFA_expression, regno, 18 /* block length */,		\
	 DW_OP_breg(27), (offs & 0x7F) | 0x80, (offs >> 7),		\
	  DW_OP_breg(29), 0, DW_OP_lit(10), DW_OP_div,			\
	   DW_OP_dup, DW_OP_lit(UC_TRAD64/10), DW_OP_eq,		\
	    DW_OP_swap, DW_OP_lit(UC_FLAVOR64/10), DW_OP_eq, DW_OP_or,	\
	   DW_OP_const1u, offs64-(offs), DW_OP_mul, DW_OP_plus

#else

/* The kernel always gives a ppc64 process a mcontext64, so just use
   that offset.  */
#define loc_expr_for_reg(regno, offs, offs64)			\
	.byte DW_CFA_expression, regno, 3 /* block length */,	\
	 DW_OP_breg(27), (offs64 & 0x7F) | 0x80, (offs64 >> 7)

#define loc_expr_for_reg_sml(regno, offs, offs64)	\
  loc_expr_for_reg(regno, offs, offs64)

#endif /* __ppc__ */

#define loc_expr_varying(regno, offs, offs64)			\
  loc_expr_for_reg (regno, offs, (offs64+MODE_CHOICE(4,0)))

/* For REGNO < 22 */
#define loc_expr_gpr_sml(regno)						\
 loc_expr_for_reg_sml (regno, MCONTEXT_SS_R0+(4*regno),			\
 		       MCONTEXT64_SS_R0+(8*regno)+MODE_CHOICE (4,0))

/* For REGNO >= 22 */
#define loc_expr_gpr(regno)				\
 loc_expr_varying (regno, MCONTEXT_SS_R0+(4*regno),	\
 		   MCONTEXT64_SS_R0+(8*regno))

#define loc_expr_fpr(regno)					\
 loc_expr_for_reg (regno+32, MCONTEXT_FS_FPREGS+(8*regno),	\
		   MCONTEXT64_FS_FPREGS+(8*regno))
#define loc_expr_vr(regno)					\
 loc_expr_for_reg (regno+77, MCONTEXT_VS_SAVE_VR+(16*regno),	\
		   MCONTEXT64_VS_SAVE_VR+(16*regno))

	/* Unwind tables.  */
	.section __TEXT,__eh_frame,coalesced,no_toc+strip_static_syms+live_support
EH_frame1:
	.set L$set$0,LECIE1-LSCIE1
	.long L$set$0	; Length of Common Information Entry
LSCIE1:
	.long	0	; CIE Identifier Tag
	.byte	0x3	; CIE Version
	.ascii "zRS\0"	; CIE Augmentation
	;;  Both these alignment values are unused.
	.byte	0x1	; uleb128 0x1; CIE Code Alignment Factor
	.byte	0x7c	; sleb128 -4; CIE Data Alignment Factor
	/* The choice of column for the return address is somewhat tricky.
	   Fortunately, the actual choice is private to this file, and
	   the space it's reserved from is the GCC register space, not the
	   DWARF2 numbering.  So any free element of the right size is an OK
	   choice.  Thus: */
	.byte	67	; CIE RA Column
	.byte	0x1	; uleb128 0x1; Augmentation size
	.byte	0x10	; FDE Encoding (pcrel)
	.byte	0xc	; DW_CFA_def_cfa
	.byte	0x1	; uleb128 0x1
	.byte	0x0	; uleb128 0x0
	.align LOG2_GPR_BYTES
LECIE1:
	.globl _sigtramp.eh
_sigtramp.eh:
LSFDE1:
	.set L$set$1,LEFDE1-LASFDE1
	.long L$set$1	; FDE Length
LASFDE1:
	.long	LASFDE1-EH_frame1	; FDE CIE offset
	.g_long	Lcall_start-.	; FDE initial location
	.set L$set$2,Lcall_end-Lcall_start
	.g_long	L$set$2	; FDE address range
	.byte	0x0	; uleb128 0x0; Augmentation size

	/* Now for the expressions, which all compute
	   uctx->uc_mcontext->register
	   for each register.
	   uctx->uc_mcontext is already in r27, so
	   the tricky part is that this might be a 64-bit context,
	   in which case the offset would be different.

	   In the case of a dual context, only the low half of a
	   GPR is restored.
	   
	   Restore even the registers that are not call-saved because they
	   might be being used in the prologue to save other registers,
	   for instance GPR0 is sometimes used to save LR.    */

	loc_expr_gpr_sml (0)
	loc_expr_gpr_sml (1)
	loc_expr_gpr_sml (2)
	loc_expr_gpr_sml (3)
	loc_expr_gpr_sml (4)
	loc_expr_gpr_sml (5)
	loc_expr_gpr (6)
	loc_expr_gpr (7)
	loc_expr_gpr (8)
	loc_expr_gpr (9)
	loc_expr_gpr (10)
	loc_expr_gpr (11)
	loc_expr_gpr (12)
	loc_expr_gpr (13)
	loc_expr_gpr (14)
	loc_expr_gpr (15)
	loc_expr_gpr (16)
	loc_expr_gpr (17)
	loc_expr_gpr (18)
	loc_expr_gpr (19)
	loc_expr_gpr (20)
	loc_expr_gpr (21)
	loc_expr_gpr (22)
	loc_expr_gpr (23)
	loc_expr_gpr (24)
	loc_expr_gpr (25)
	loc_expr_gpr (26)
	loc_expr_gpr (27)
	loc_expr_gpr (28)
	loc_expr_gpr (29)
	loc_expr_gpr (30)
	loc_expr_gpr (31)

	loc_expr_for_reg (64, MCONTEXT_SS_CR, MCONTEXT64_SS_CR)
	loc_expr_varying (76, MCONTEXT_SS_XER, MCONTEXT64_SS_XER)
	loc_expr_varying (65, MCONTEXT_SS_LR, MCONTEXT64_SS_LR)
	loc_expr_varying (66, MCONTEXT_SS_CTR, MCONTEXT64_SS_CTR)
	loc_expr_for_reg (109, MCONTEXT_SS_VRSAVE, MCONTEXT64_SS_VRSAVE)

	loc_expr_fpr (0)
	loc_expr_fpr (1)
	loc_expr_fpr (2)
	loc_expr_fpr (3)
	loc_expr_fpr (4)
	loc_expr_fpr (5)
	loc_expr_fpr (6)
	loc_expr_fpr (7)
	loc_expr_fpr (8)
	loc_expr_fpr (9)
	loc_expr_fpr (10)
	loc_expr_fpr (11)
	loc_expr_fpr (12)
	loc_expr_fpr (13)
	loc_expr_fpr (14)
	loc_expr_fpr (15)
	loc_expr_fpr (16)
	loc_expr_fpr (17)
	loc_expr_fpr (18)
	loc_expr_fpr (19)
	loc_expr_fpr (20)
	loc_expr_fpr (21)
	loc_expr_fpr (22)
	loc_expr_fpr (23)
	loc_expr_fpr (24)
	loc_expr_fpr (25)
	loc_expr_fpr (26)
	loc_expr_fpr (27)
	loc_expr_fpr (28)
	loc_expr_fpr (29)
	loc_expr_fpr (30)
	loc_expr_fpr (31)

	loc_expr_for_reg (112, MCONTEXT_FS_FPSCR, MCONTEXT64_FS_FPSCR)

	loc_expr_vr (0)
	loc_expr_vr (1)
	loc_expr_vr (2)
	loc_expr_vr (3)
	loc_expr_vr (4)
	loc_expr_vr (5)
	loc_expr_vr (6)
	loc_expr_vr (7)
	loc_expr_vr (8)
	loc_expr_vr (9)
	loc_expr_vr (10)
	loc_expr_vr (11)
	loc_expr_vr (12)
	loc_expr_vr (13)
	loc_expr_vr (14)
	loc_expr_vr (15)
	loc_expr_vr (16)
	loc_expr_vr (17)
	loc_expr_vr (18)
	loc_expr_vr (19)
	loc_expr_vr (20)
	loc_expr_vr (21)
	loc_expr_vr (22)
	loc_expr_vr (23)
	loc_expr_vr (24)
	loc_expr_vr (25)
	loc_expr_vr (26)
	loc_expr_vr (27)
	loc_expr_vr (28)
	loc_expr_vr (29)
	loc_expr_vr (30)
	loc_expr_vr (31)

	loc_expr_for_reg (110, MCONTEXT_VS_VSCR, MCONTEXT64_VS_VSCR)

     	/* The return address is even more complicated, because it needs
	   to be the actual address to which to return, and so
	   depends on the signal thrown, because some signals have SRR0
	   as the address of the faulting instruction, and others
	   have it as the next address to execute.

	   Although MCONTEXT_SS_SRR0 is the same as MCONTEXT64_SS_SRR0,
	   that doesn't really simplify things much, since if
	   the context is a 64-bit context for a 32-bit process,
	   we'll need to add 4 to get to the low word.  */
     
	/* The exception types that point to the faulting instruction are:
	   EXCEPTION_DSI, EXCEPTION_ALIGN, EXCEPTION_FPUNAVAIL,
	   and 
	   EXCEPTION_PROGRAM when SRR1[47] is clear.
	   The others point to the next instruction to execute.

	   EXCEPTION_ISI is a special case.  There are these possibilies:
	   - program calls a subroutine which is NULL, in which case
	     SRR0 holds NULL and LR-4 is the faulting instruction.
	   - program executes a computed goto to NULL, in which case
	     there is no way to know the faulting instruction.
	   - program runs off end of its text, in which case
	     SRR0-4 is the faulting instruction
	   - program executes a wild branch.
	   I think this code most needs to handle the first case, as
	   the other cases are rare or can't be handled.  */

	.byte DW_CFA_val_expression, 67
	.set L$set$3,Lpc_end-Lpc_start
	.byte L$set$3
Lpc_start:
#ifdef __ppc__
	/* On ppc, compute whether or not a 64-bit exception frame is in
	   use.  */
	.byte  DW_OP_breg(29), 0, DW_OP_lit(10), DW_OP_div
	.byte   DW_OP_dup, DW_OP_lit(UC_TRAD64/10), DW_OP_eq
	.byte    DW_OP_swap, DW_OP_lit(UC_FLAVOR64/10), DW_OP_eq, DW_OP_or

	/* Find the value of SRR0.  */
	.byte   DW_OP_dup
	.byte	 DW_OP_lit(MCONTEXT64_SS_SRR0+4-MCONTEXT_SS_SRR0), DW_OP_mul
	.byte    DW_OP_breg(27), MCONTEXT_SS_SRR0
	.byte     DW_OP_plus, DW_OP_deref
	/* Determine the exception type.  */
	.byte    DW_OP_swap, DW_OP_dup
	.byte     DW_OP_lit(MCONTEXT64_ES_EXCEPTION-MCONTEXT_ES_EXCEPTION)
	.byte	   DW_OP_mul
	.byte     DW_OP_breg(27), MCONTEXT_ES_EXCEPTION
	.byte      DW_OP_plus, DW_OP_deref
	/* Find the value of SRR1.  */
	.byte	  DW_OP_swap, DW_OP_dup
	.byte      DW_OP_lit(MCONTEXT64_SS_SRR1+4-MCONTEXT_SS_SRR1), DW_OP_mul
	.byte      DW_OP_breg(27), MCONTEXT_SS_SRR1
	.byte       DW_OP_plus, DW_OP_deref
	/* Find the value of LR.  */
	.byte	   DW_OP_swap
	.byte      DW_OP_const1u, MCONTEXT64_SS_LR+4-MCONTEXT_SS_LR, DW_OP_mul
	.byte      DW_OP_breg(27), MCONTEXT_SS_LR, MCONTEXT_SS_LR >> 7
	.byte       DW_OP_plus, DW_OP_deref
#else
	/* Find the value of SRR0.  */
	.byte  DW_OP_breg(27), MCONTEXT64_SS_SRR0, DW_OP_deref
	/* Determine the exception type.  */
	.byte   DW_OP_breg(27), MCONTEXT64_ES_EXCEPTION, DW_OP_deref_size, 4
	/* Find the value of SRR1.  */
	.byte    DW_OP_breg(27), MCONTEXT64_SS_SRR1, DW_OP_deref
	/* Find the value of LR.  */
	.byte     DW_OP_breg(27), MCONTEXT64_SS_LR & 0x7f | 0x80
	.byte	    MCONTEXT64_SS_LR >> 7
	.byte	   DW_OP_deref
#endif
	/* At this point, the stack contains LR, SRR1, the exception type,
	   SRR0, and the base CFA address (which this doesn't use).  */

	/* If the exception type is EXCEPTION_ISI, the result is LR.  */
	.byte	   DW_OP_pick, 2
	.byte	    DW_OP_lit(EXCEPTION_ISI), DW_OP_eq
	.byte	    DW_OP_bra	; 'bra' is a conditional branch.
	.set L$set$5,Lpc_end-0f
	.short	     L$set$5
0:	
	.byte	   DW_OP_drop

	/* Otherwise, start by determining if SRR1[47] is clear...  */
	.byte     DW_OP_not, DW_OP_lit(16), DW_OP_shr, DW_OP_lit(1), DW_OP_and
	/* ...and the exception type is EXCEPTION_PROGRAM.  */
	.byte     DW_OP_over, DW_OP_lit(EXCEPTION_PROGRAM), DW_OP_eq, DW_OP_and
	/* Check if any of the other exception cases are present.  */
	.byte     DW_OP_over, DW_OP_lit(EXCEPTION_DSI), DW_OP_eq, DW_OP_or
	.byte     DW_OP_over, DW_OP_lit(EXCEPTION_ALIGN), DW_OP_eq, DW_OP_or
	.byte     DW_OP_swap, DW_OP_lit(EXCEPTION_FPUNAVAIL)
	.byte      DW_OP_eq, DW_OP_or
	/* If the exception points to the faulting instruction, add
	   4 to point past the faulting instruction.  */
	.byte    DW_OP_lit(4), DW_OP_mul, DW_OP_plus
Lpc_end:	

	/* The CFA will have been saved as the value of R1.  */
	.byte DW_CFA_def_cfa_expression
	.set L$set$4,Lcfa_end-Lcfa_start
	.byte L$set$4
Lcfa_start:	
#ifdef __ppc__
	.byte DW_OP_breg(27), MCONTEXT_SS_R0+4
	.byte  DW_OP_breg(29), 0, DW_OP_lit(10), DW_OP_div
	.byte   DW_OP_dup, DW_OP_lit(UC_TRAD64/10), DW_OP_eq
	.byte    DW_OP_swap, DW_OP_lit(UC_FLAVOR64/10), DW_OP_eq, DW_OP_or
	.byte  DW_OP_lit(MCONTEXT64_SS_R0+12-MCONTEXT_SS_R0-4)
	.byte   DW_OP_mul, DW_OP_plus
	.byte DW_OP_deref
#else
	.byte DW_OP_breg(27), MCONTEXT64_SS_R0+8, DW_OP_deref
#endif
Lcfa_end:

	.align LOG2_GPR_BYTES
LEFDE1:
		
	.subsections_via_symbols
