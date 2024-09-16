/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
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


#ifndef _ARM64_DWARF_UNWIND_H_
#define _ARM64_DWARF_UNWIND_H_

/*
 * This file contains the architecture specific DWARF definitions needed for unwind
 * information added to trap handlers (and hand-written assembly functions that
 *  don't follow the standard calling convention ABI).
 */

/* DWARF Register numbers for ARM64 registers contained in the saved state */
#define DWARF_ARM64_X0 0
#define DWARF_ARM64_X1 1
#define DWARF_ARM64_X2 2
#define DWARF_ARM64_X3 3
#define DWARF_ARM64_X4 4
#define DWARF_ARM64_X5 5
#define DWARF_ARM64_X6 6
#define DWARF_ARM64_X7 7
#define DWARF_ARM64_X8 8
#define DWARF_ARM64_X9 9
#define DWARF_ARM64_X10 10
#define DWARF_ARM64_X11 11
#define DWARF_ARM64_X12 12
#define DWARF_ARM64_X13 13
#define DWARF_ARM64_X14 14
#define DWARF_ARM64_X15 15
#define DWARF_ARM64_X16 16
#define DWARF_ARM64_X17 17
#define DWARF_ARM64_X18 18
#define DWARF_ARM64_X19 19
#define DWARF_ARM64_X20 20
#define DWARF_ARM64_X21 21
#define DWARF_ARM64_X22 22
#define DWARF_ARM64_X23 23
#define DWARF_ARM64_X24 24
#define DWARF_ARM64_X25 25
#define DWARF_ARM64_X26 26
#define DWARF_ARM64_X27 27
#define DWARF_ARM64_X28 28

#define DWARF_ARM64_FP 29
#define DWARF_ARM64_LR 30
#define DWARF_ARM64_SP 31
#define DWARF_ARM64_PC 32
#define DWARF_ARM64_CPSR 33

#define DWARF_ARM64_X0_OFFSET 8
#define DWARF_ARM64_X1_OFFSET 16
#define DWARF_ARM64_X2_OFFSET 24
#define DWARF_ARM64_X3_OFFSET 32
#define DWARF_ARM64_X4_OFFSET 40
#define DWARF_ARM64_X5_OFFSET 48
#define DWARF_ARM64_X6_OFFSET 56
#define DWARF_ARM64_X7_OFFSET 0xc0, 0x00
#define DWARF_ARM64_X8_OFFSET 0xc8, 0x00
#define DWARF_ARM64_X9_OFFSET 0xd0, 0x00
#define DWARF_ARM64_X10_OFFSET 0xd8, 0x00
#define DWARF_ARM64_X11_OFFSET 0xe0, 0x00
#define DWARF_ARM64_X12_OFFSET 0xe8, 0x00
#define DWARF_ARM64_X13_OFFSET 0xf0, 0x00
#define DWARF_ARM64_X14_OFFSET 0xf8, 0x00
#define DWARF_ARM64_X15_OFFSET 0x80, 0x01
#define DWARF_ARM64_X16_OFFSET 0x88, 0x01
#define DWARF_ARM64_X17_OFFSET 0x90, 0x01
#define DWARF_ARM64_X18_OFFSET 0x98, 0x01
#define DWARF_ARM64_X19_OFFSET 0xa0, 0x01
#define DWARF_ARM64_X20_OFFSET 0xa8, 0x01
#define DWARF_ARM64_X21_OFFSET 0xb0, 0x01
#define DWARF_ARM64_X22_OFFSET 0xb8, 0x01
#define DWARF_ARM64_X23_OFFSET 0xc0, 0x01
#define DWARF_ARM64_X24_OFFSET 0xc8, 0x01
#define DWARF_ARM64_X25_OFFSET 0xd0, 0x01
#define DWARF_ARM64_X26_OFFSET 0xd8, 0x01
#define DWARF_ARM64_X27_OFFSET 0xe0, 0x01
#define DWARF_ARM64_X28_OFFSET 0xe8, 0x01

#define DWARF_ARM64_FP_OFFSET 0xf0, 0x01
#define DWARF_ARM64_LR_OFFSET 0xf8, 0x01
#define DWARF_ARM64_SP_OFFSET 0x80, 0x02
#define DWARF_ARM64_PC_OFFSET 0x88, 0x02
#define DWARF_ARM64_CPSR_OFFSET 0x90, 0x02

/* DWARF constants */
#define DW_CFA_register 0x09
#define DW_CFA_expression 0x10
#define DW_CFA_offset_extended_sf 0x11

#define DW_OP_breg21      0x85

#define DW_FORM_LEN_ONE_BYTE_SLEB 2
#define DW_FORM_LEN_TWO_BYTE_SLEB 3


/* The actual unwind directives added to trap handlers to let the debugger know where the register state is stored */

/* Unwind Prologue added to each function to indicate the start of the unwind information. */
#define UNWIND_PROLOGUE \
.cfi_sections .eh_frame %%\
.cfi_startproc          %%\

#define TRAP_UNWIND_PROLOGUE \
UNWIND_PROLOGUE \
.cfi_signal_frame       %%\

/* Unwind Epilogue added to each function to indicate the end of the unwind information */

#define UNWIND_EPILOGUE .cfi_endproc

/*  Unwind directives for trap handlers let the debugger know where the register state is stored.
 *
 *   The saved state is stored in the `struct arm_kernel_saved_state` pointed to by x21.
 *
 *   A note on setting the CFA relative to $fp:
 *   We have hand-written assembly that alters $sp all over the place, which interferes with InstructionEmulation-based unwinding.
 *   So lldb falls back to the architecture-default unwind plan - which sets the CFA relative to $sp.
 *   Although the $sp tampering means it can't be provided - we don't need at all for the unwind plan here.
 *   By setting the CFA relative to $fp, which can be provided, lldb won't stop unwinding */
#define TRAP_UNWIND_DIRECTIVES \
.cfi_def_cfa w29, 0     %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_X0, DW_FORM_LEN_ONE_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_X0_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_X1, DW_FORM_LEN_ONE_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_X1_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_X2, DW_FORM_LEN_ONE_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_X2_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_X3, DW_FORM_LEN_ONE_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_X3_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_X4, DW_FORM_LEN_ONE_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_X4_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_X5, DW_FORM_LEN_ONE_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_X5_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_X6, DW_FORM_LEN_ONE_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_X6_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_X7, DW_FORM_LEN_TWO_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_X7_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_X8, DW_FORM_LEN_TWO_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_X8_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_X9, DW_FORM_LEN_TWO_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_X9_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_X10, DW_FORM_LEN_TWO_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_X10_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_X11, DW_FORM_LEN_TWO_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_X11_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_X12, DW_FORM_LEN_TWO_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_X12_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_X13, DW_FORM_LEN_TWO_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_X13_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_X14, DW_FORM_LEN_TWO_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_X14_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_X15, DW_FORM_LEN_TWO_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_X15_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_X16, DW_FORM_LEN_TWO_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_X16_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_X17, DW_FORM_LEN_TWO_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_X17_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_X18, DW_FORM_LEN_TWO_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_X18_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_X19, DW_FORM_LEN_TWO_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_X19_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_X20, DW_FORM_LEN_TWO_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_X20_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_X21, DW_FORM_LEN_TWO_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_X21_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_X22, DW_FORM_LEN_TWO_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_X22_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_X23, DW_FORM_LEN_TWO_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_X23_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_X24, DW_FORM_LEN_TWO_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_X24_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_X25, DW_FORM_LEN_TWO_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_X25_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_X26, DW_FORM_LEN_TWO_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_X26_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_X27, DW_FORM_LEN_TWO_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_X27_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_X28, DW_FORM_LEN_TWO_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_X28_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_FP, DW_FORM_LEN_TWO_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_FP_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_LR, DW_FORM_LEN_TWO_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_LR_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_SP, DW_FORM_LEN_TWO_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_SP_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_PC, DW_FORM_LEN_TWO_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_PC_OFFSET %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_CPSR, DW_FORM_LEN_TWO_BYTE_SLEB, DW_OP_breg21, DWARF_ARM64_CPSR_OFFSET %%\

#if CONFIG_SPTM
/**
 * These offsets are SLEB128 encodings of the offsets of the FP, LR, SP, and PC
 * members of the SPTM dispatch_panic_state_t structure defined in sptm/dispatch/dispatch.h
 *
 * If the offsets of these members are changed in SPTM data structures, they should be
 * changed here to match accordingly.
 */
#define SPTM_UNWIND_DIRECTIVES \
.cfi_escape DW_CFA_expression, DWARF_ARM64_FP, DW_FORM_LEN_ONE_BYTE_SLEB, DW_OP_breg21, 0 %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_LR, DW_FORM_LEN_ONE_BYTE_SLEB, DW_OP_breg21, 8 %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_SP, DW_FORM_LEN_ONE_BYTE_SLEB, DW_OP_breg21, 16 %%\
.cfi_escape DW_CFA_expression, DWARF_ARM64_PC, DW_FORM_LEN_ONE_BYTE_SLEB, DW_OP_breg21, 24 %%\

#endif /* CONFIG_SPTM */

/*  Special unwinding instructions for `return_to_kernel`.
 *
 *   We set the CFA, $fp and $lr the same as would be for the architecture default
 *   plan.
 *   We have to tell the unwinder that x21 is at x21, because it isn't aware
 *   we use it for the saved state.
 *   (i.e. it isn't used as a general purpose register / isn't volatile)
 */
#define RETURN_TO_KERNEL_UNWIND \
.cfi_def_cfa w29, 16     %%\
.cfi_escape DW_CFA_offset_extended_sf, DWARF_ARM64_FP, 2 %%\
.cfi_escape DW_CFA_offset_extended_sf, DWARF_ARM64_LR, 1 %%\
.cfi_escape DW_CFA_register, DWARF_ARM64_X21, DWARF_ARM64_X21 %%\

#endif /* _ARM64_DWARF_UNWIND_H_ */
