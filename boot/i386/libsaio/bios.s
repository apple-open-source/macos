/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
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
 * Copyright 1993 NeXT Computer, Inc.
 * All rights reserved.
 *
 * Harness for calling real-mode BIOS functions.
 */

#include <architecture/i386/asm_help.h>
#include "memory.h"

#define data32  .byte 0x66
#define addr32  .byte 0x67

#define O_INT   0
#define O_EAX   4
#define O_EBX   8
#define O_ECX   12
#define O_EDX   16
#define O_EDI   20
#define O_ESI   24
#define O_EBP   28
#define O_CS    32
#define O_DS    34
#define O_ES    36
#define O_FLG   38

.data
    .lcomm save_eax,  4,2
    .lcomm save_edx,  4,2
    .lcomm save_es,   2,1
    .lcomm save_flag, 2,1
    .lcomm new_eax,   4,2
    .lcomm new_edx,   4,2
    .lcomm new_es,    2,1

.text

/*============================================================================
 * Call real-mode BIOS INT functions.
 *
 */
LABEL(_bios)
    enter   $0, $0
    pushal

    movl    8(%ebp), %edx       // address of save area
    movb    O_INT(%edx), %al    // save int number
    movb    %al, do_int+1

    movl    O_EBX(%edx), %ebx
    movl    O_ECX(%edx), %ecx
    movl    O_EDI(%edx), %edi
    movl    O_ESI(%edx), %esi
    movl    O_EBP(%edx), %ebp
    movl    %edx, save_edx
    movl    O_EAX(%edx), %eax
    movl    %eax, new_eax
    movl    O_EDX(%edx), %eax
    movl    %eax, new_edx
    movw    O_ES(%edx),  %ax
    movl    %ax, new_es

    call    __prot_to_real

    data32
    addr32
    mov     OFFSET16(new_eax), %eax
    data32
    addr32
    mov     OFFSET16(new_edx), %edx
    data32
    addr32
    mov     OFFSET16(new_es), %es

do_int:
    int     $0x00
    pushf
    data32
    addr32
    movl    %eax, OFFSET16(save_eax)
    popl    %eax                         // actually pop %ax
    addr32
    movl    %eax, OFFSET16(save_flag)  // actually movw
    mov     %es, %ax
    addr32
    movl    %eax, OFFSET16(save_es)    // actually movw
    data32
    call    __real_to_prot

    movl    %edx, new_edx       // save new edx before clobbering
    movl    save_edx, %edx
    movl    new_edx, %eax       // now move it into buffer
    movl    %eax, O_EDX(%edx)
    movl    save_eax, %eax
    movl    %eax, O_EAX(%edx)
    movw    save_es, %ax
    movw    %ax, O_ES(%edx)
    movw    save_flag, %ax
    movw    %ax, O_FLG(%edx)
    movl    %ebx, O_EBX(%edx)
    movl    %ecx, O_ECX(%edx)
    movl    %edi, O_EDI(%edx)
    movl    %esi, O_ESI(%edx)
    movl    %ebp, O_EBP(%edx)

    popal
    leave

    ret

/*============================================================================
 * Determines the total system memory size using various BIOS Int 15 calls.
 *
 */
LABEL(_get_memsize)
    enter   $0, $0              # create frame pointer (32 bit operand/stack)
    pushal                      # save all registers

    movl    8(%ebp), %ebx       # push input structure pointer to stack
    pushl   %ebx

    call    __prot_to_real      # switch to real mode

    ##################################################################
    # In real mode.
    # Do not forget the opcode overrides, since the assembler
    # does not know we have made a transition to 16-bit operation.
    ##################################################################

    data32
    movl    $0xE801, %eax       # Get memory size
    clc
    int     $0x15
    data32
    jnc     getmsz_e801

    data32
    movl    $0xDA88, %eax       # Get memory size
    clc
    int     $0x15
    data32
    jnc     getmsz_da88

    movb    $0x8A, %ah          # Get memory size
    clc
    int     $0x15
    data32
    jnc     getmsz_8a

    movb    $0x88, %ah          # Get memory size
    clc
    int     $0x15
    data32
    jnc     getmsz_88

    xorl    %edx, %edx          # Error, cannot get memory size
    xorl    %eax, %eax

getmsz_done:
    data32
    addr32
    pushl   %eax                # Push EAX to 32-bit stack

    data32
    call    __real_to_prot      # Back to protected mode. EAX is modified.

    ##################################################################
    # Back to protected mode.
    ##################################################################

    popl    %eax                # Pop EAX from stack
    popl    %ebx                # Pop pointer to register structure

    # Copy the result to the input structure pointed to by a pointer
    # which is on top of the stack. Write register EAX and EDX to the
    # structure.

    movl    %eax, O_EAX(%ebx)
    movl    %edx, O_EDX(%ebx)

    popal                       # restore all registers
    leave                       # undo enter operator
    ret

getmsz_88:
    orl     %eax, %eax
    data32
    jz      getmsz_64m
    xorl    %edx, %edx

getmsz_8a:
    data32
    movl    $1024, %ebx         # Add in 1M
    addl    %ebx, %eax
    adcl    $0, %edx
    data32
    jmp     getmsz_done

getmsz_64m:
    data32
    movl    $1, %edx
    xorl    %eax, %eax
    data32
    jmp     getmsz_done

getmsz_da88:
    xor     %dh, %dh
    movb    %cl, %dl
    movl    %ebx, %eax
    data32
    jmp     getmsz_8a

getmsz_e801:
    xorl    %edx, %edx
    orl     %ebx, %ebx
    data32
    jz      getmsz_88

    data32
    movl    $64, %eax
    mul     %ebx

    data32
    movl    $16384, %ebx
    addl    %ebx, %eax
    adcl    $0, %edx

    data32
    jmp     getmsz_done
