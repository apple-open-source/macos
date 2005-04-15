/*
 * Copyright (c) 1999-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999-2002 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.2 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* 
 * Mach Operating System
 * Copyright (c) 1990 Carnegie-Mellon University
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * $Log: asm.s,v $
 * Revision 1.2  2003/04/08 20:28:27  curtisg
 * Merged PR-3073653, PR-3172003.
 *
 * Revision 1.1.2.1  2003/04/05 00:48:42  curtisg
 * Changes for 3073653.
 *
 * Revision 1.4  2002/10/02 00:06:18  curtisg
 * Integrating PR-3032510.
 *
 * Revision 1.3.6.1  2002/08/30 21:16:29  curtisg
 * KERNBOOTSTRUCT is going away in favor of KernelBootArgs_t in <pexpert/i386/boot.h>.
 *
 * Revision 1.3  2002/07/09 14:06:21  jliu
 * Merging changes from PR-2954224 branch in boot/i386.
 *
 * Revision 1.2.30.1  2002/07/05 16:24:51  jliu
 * Merged UFS/HFS/HFS+ filesystem support from BootX.
 * Moved boot2 load address due to increased size. boot0/boot1 also changed.
 * Updated boot graphics and CLUT.
 * Added support to chain load foreign booters.
 * Fixed param passing bug in network loader.
 * Misc cleanup in libsaio.
 *
 * Revision 1.2  2000/05/23 23:01:11  lindak
 * Merged PR-2309530 into Kodiak (liu i386 booter: does not support label-less
 * ufs partitions)
 *
 * Revision 1.1.1.2.4.1  2000/05/13 17:07:39  jliu
 * New boot0 (boot1 has been deprecated). Booter must now reside in its own partition, no disk label required.
 *
 * Revision 1.1.1.2  1999/08/04 21:16:57  wsanchez
 * Impoort of boot-66
 *
 * Revision 1.3  1999/08/04 21:12:12  wsanchez
 * Update APSL
 *
 * Revision 1.2  1999/03/25 05:48:30  wsanchez
 * Add APL.
 * Remove unused gzip code.
 * Remove unused Adobe fonts.
 *
 * Revision 1.1.1.1.66.2  1999/03/16 16:08:54  wsanchez
 * Substitute License
 *
 * Revision 1.1.1.1.66.1  1999/03/16 07:33:21  wsanchez
 * Add APL
 *
 * Revision 1.1.1.1  1997/12/05 21:57:57  wsanchez
 * Import of boot-25 (~mwatson)
 *
 * Revision 2.1.1.2  90//03//22  17:59:50  rvb
 *  Added _sp() => where is the stack at. [kupfer]
 * 
 * Revision 2.1.1.1  90//02//09  17:25:04  rvb
 *  Add Intel copyright
 *  [90//02//09            rvb]
 * 
 */


//          INTEL CORPORATION PROPRIETARY INFORMATION
//
//  This software is supplied under the terms of a license  agreement or 
//  nondisclosure agreement with Intel Corporation and may not be copied 
//  nor disclosed except in accordance with the terms of that agreement.
//
//  Copyright 1988 Intel Corporation
//  Copyright 1988, 1989 by Intel Corporation
//

#include <architecture/i386/asm_help.h>
#include "memory.h"

#define data32  .byte 0x66
#define addr32  .byte 0x67

    .file "asm.s"

CR0_PE_ON  = 0x1
CR0_PE_OFF = 0x7ffffffe

STACK32_BASE = ADDR32(STACK_SEG, 0)
STACK16_SEG  = STACK_SEG
CODE32_BASE  = ADDR32(BOOT1U_SEG, 0)
CODE16_SEG   = BOOT1U_SEG

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Pointer to 6-bytes in memory that contains the base address and the limit
// (size of GDT table in bytes) of the GDT. The LGDT is the only instruction
// that directly loads a linear address (not a segment relative address) and
// a limit in protected mode.

.globl _Gdtr
    .data
    .align 2, 0x90
_Gdtr:
    .word GDTLIMIT
    .long vtop(_Gdt)

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Data area for __switch_stack.
//
save_sp: .long  STACK_OFS
save_ss: .long  STACK_SEG

    .text

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// real_to_prot()
//
// Transfer from real mode to protected mode.
// Preserves all registers except EAX.
//
LABEL(__real_to_prot)

    // Interrupts are disabled in protected mode.

    cli

    // Load the Global Descriptor Table Register (GDTR).

    addr32
    data32
    lgdt    OFFSET1U16(_Gdtr)

    // Enter protected mode by setting the PE bit in CR0.

    mov     %cr0, %eax
    data32
    or      $CR0_PE_ON, %eax
    mov     %eax, %cr0

    // Make intrasegment jump to flush the processor pipeline and
    // reload CS register.

    data32
    ljmp    $0x08, $xprot

xprot:
    // we are in USE32 mode now
    // set up the protected mode segment registers : DS, SS, ES, FS, GS

    mov     $0x10, %eax
    movw    %ax, %ds
    movw    %ax, %ss
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs

    // Convert STACK_SEG:SP to 32-bit linear stack pointer.

    movzwl  %sp, %eax
    addl    $STACK32_BASE, %eax
    movl    %eax, %esp

    // Convert STACK_SEG:BP to 32-bit linear base pointer.

    movzwl  %bp, %eax
    addl    $STACK32_BASE, %eax
    movl    %eax, %ebp

    // Modify the caller's return address on the stack from
    // segment offset to linear address.

    popl    %eax
    addl    $CODE32_BASE, %eax
    pushl   %eax

    ret

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// prot_to_real()
//
// Transfer from protected mode to real mode.
// Preserves all registers except EAX.
// 
LABEL(__prot_to_real)

    // Set up segment registers appropriate for real mode.

    movw    $0x30, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs
    movw    %ax, %ss

    ljmp    $0x18, $x16       // change to USE16 mode

x16:
    mov     %cr0, %eax        // clear the PE bit of CR0
    data32
    and     $CR0_PE_OFF, %eax
    mov     %eax, %cr0

    // make intersegment jmp to flush the processor pipeline
    // and reload CS register

    data32
    ljmp    $CODE16_SEG, $xreal - CODE32_BASE

xreal:
    // we are in real mode now
    // set up the real mode segment registers : DS, DS, ES, FS, GS

    movw    %cs, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs

    // load stack segment register SS.

    data32
    movl    $STACK16_SEG, %eax
    movw    %ax, %ss

    // clear top 16-bits of ESP and EBP.

    data32
    movzwl  %sp, %esp
    data32
    movzwl  %bp, %ebp

    // Modify caller's return address on the stack
    // from linear address to segment offset.

    data32
    popl    %eax
    data32
    movzwl  %ax, %eax
    data32
    pushl   %eax

    // Reenable maskable interrupts.

    sti

    data32
    ret

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// halt()
//
LABEL(_halt)
    hlt
    jmp     _halt

#if 0
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// startprog(phyaddr, arg)
// Start the program on protected mode where phyaddr is the entry point.
// Passes arg to the program in %eax.
//
LABEL(_startprog)
    push    %ebp
    mov     %esp, %ebp

    mov     0xc(%ebp), %eax  // argument to program
    mov     0x8(%ebp), %ecx  // entry offset 
    mov     $0x28, %ebx      // segment
    push    %ebx
    push    %ecx

    // set up %ds and %es

    mov     $0x20, %ebx
    movw    %bx, %ds
    movw    %bx, %es

    lret
#endif

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Returns the current stack pointer.
//
LABEL(__sp)
    mov %esp, %eax
    ret

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Returns the current frame pointer.
//
LABEL(__bp)
    mov %ebp, %eax
    ret

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// switch_stack()
//
// Switches stack pointer between SS:SP and memory save_ss:save_sp.
// Call this function from real mode only!!!
//
// AX, DI, and SI are clobbered.
//
LABEL(__switch_stack)
    popl    %eax                # save return address
    popl    %edi                # discard upper 16-bit

    data32
    addr32
    movl    OFFSET1U16(save_ss), %esi   # new SS to SI

    data32
    addr32
    movl    OFFSET1U16(save_sp), %edi   # new SP to DI

    addr32
    mov     %ss, OFFSET1U16(save_ss)    # save current SS to memory

    data32
    addr32
    movl    %esp, OFFSET1U16(save_sp)   # save current SP to memory

    cli
    mov     %si, %ss            # switch stack
    mov     %di, %sp
    sti

    pushl   %eax                # push IP of caller onto the new stack

    xorl    %eax, %eax
    xorl    %esi, %esi
    xorl    %edi, %edi

    ret

#if 0
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// loader()
//
// Issue a request to the network loader.
//
LABEL(_loader)
    enter   $0, $0
    pushal

    #
    # Pass a far pointer to the command structure
    # to the INT call through DX:CX.
    #
    # The command code is in BX.
    #

    movw     8(%ebp), %bx       #  8[EBP] = command code
    movw    12(%ebp), %cx       # 12[EBP] = command structure offset
    movw    14(%ebp), %dx       # 14[EBP] = command structure segment

    call    __prot_to_real      # Revert to real mode

    ###### Real Mode Begin ######

    data32
    call    __switch_stack      # Switch to NBP stack

    int     $0x2b               # Call NBP

    data32
    call    __switch_stack      # Restore stack

    data32
    call    __real_to_prot      # Back to protected mode

    ###### Real Mode End ######

    popal
    leave
    ret
#endif

#if 0
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// pcpy(src, dst, cnt)
//  where src is a virtual address and dst is a physical address
//
LABEL(_pcpy)
    push    %ebp
    mov     %esp, %ebp
    push    %es
    push    %esi
    push    %edi
    push    %ecx

    cld

    // set %es to point at the flat segment

    mov     $0x20, %eax
    movw    %ax , %es

    mov     0x8(%ebp), %esi     // source
    mov     0xc(%ebp), %edi     // destination
    mov     0x10(%ebp), %ecx    // count

    rep
    movsb

    pop     %ecx
    pop     %edi
    pop     %esi
    pop     %es
    pop     %ebp

    ret
#endif
