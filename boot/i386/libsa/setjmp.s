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

#include <architecture/i386/asm_help.h>

#define	O_EDI	0
#define	O_ESI	4
#define	O_EBX	8
#define	O_EBP	12
#define	O_ESP	16
#define O_EIP	20

LEAF(_setjmp, 0)
X_LEAF(_set_label, _setjmp)
	movl	4(%esp), %edx       // address of save area
	movl	%edi, O_EDI(%edx)
	movl	%esi, O_ESI(%edx)
	movl	%ebx, O_EBX(%edx)
	movl	%ebp, O_EBP(%edx)
	movl	%esp, O_ESP(%edx)
	movl	(%esp), %ecx        // %eip (return address)
	movl	%ecx, O_EIP(%edx)
	subl	%eax, %eax          // retval <- 0
	ret

LEAF(_longjmp, 0)
X_LEAF(_jump_label, _longjmp)
	movl	4(%esp), %edx       // address of save area
	movl	O_EDI(%edx), %edi
	movl	O_ESI(%edx), %esi
	movl	O_EBX(%edx), %ebx
	movl	O_EBP(%edx), %ebp
	movl	O_ESP(%edx), %esp
	movl	O_EIP(%edx), %eax   // %eip (return address)
	movl	%eax, 0(%esp)
	popl	%eax                // ret addr != 0
	jmp	    *%eax               // indirect
