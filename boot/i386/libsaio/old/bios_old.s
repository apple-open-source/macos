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
 * Mach Operating System
 * Copyright (c) 1990 Carnegie-Mellon University
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

//			INTEL CORPORATION PROPRIETARY INFORMATION
//
//	This software is supplied under the terms of a license  agreement or 
//	nondisclosure agreement with Intel Corporation and may not be copied 
//	nor disclosed except in accordance with the terms of that agreement.
//
//	Copyright 1988 Intel Corporation
// Copyright 1988, 1989 by Intel Corporation
//

	.file	"bios.s"

#include <machdep/i386/asm.h>
#include "memory.h"

	.text

#if 0
// biosread(dev, cyl, head, sec, num)
//	Read num sectors from disk into the internal buffer "intbuf" which
//	is the first 4K bytes of the boot loader.
// BIOS call "INT 0x13 Function 0x2" to read sectors from disk into memory
//	Call with	%ah = 0x2
//			%al = number of sectors
//			%ch = cylinder
//			%cl = sector
//			%dh = head
//			%dl = drive (0x80 for hard disk, 0x0 for floppy disk)
//			%es:%bx = segment:offset of buffer
//	Return:		
//			%al = 0x0 on success; err code on failure

ENTRY(_biosread)
	push	%ebp
	mov	%esp, %ebp

	push	%ebx
	push	%ecx
	push	%edx
	push	%esi
	push 	%edi
	push	%ds
	push	%es
//	push	%fs
//	push	%gs

	movb	0x10(%ebp), %dh		// head

	movw	0x0c(%ebp), %cx
	xchgb	%ch, %cl		// cylinder; %cl=the high 2 bits of cyl
	rorb	$2, %cl
	movb	0x14(%ebp), %al
	orb	%al, %cl
	incb	%cl			// sector; sec starts from 1, not 0

	movb	0x8(%ebp), %dl		// device
	movb	0x18(%ebp),%bl		// number of sectors
	movb	$2,%bh			// bios read function

	call	EXT(_prot_to_real)	// enter real mode, set %es to BOOTSEG

	data16
	mov	$5,%edi			// retry up to 5 times

retry_disc:
		mov	%ebx,%eax	// get function and amount
//		xor	%ebx, %ebx	// offset = 0
		data16
		mov	$(ptov(BIOS_ADDR)), %ebx

		push %eax		// save amount
		push %ecx
		push %edx
		push %edi
		int	$0x13
		pop %edi
		pop %edx
		pop %ecx
		pop %ebx		// pop amount into bx (safe place)

//		test $0, %ah
		data16
		jnb	read_succes

		// woops, bios failed to read sector
		push %eax		// save error
		xor %eax,%eax
		int	$0x13		// reset disk
		pop %eax		// restore error code
		dec %edi
		data16
		jne retry_disc

read_succes:
	mov	%eax, %ebx		// save return value

	data16
	call	EXT(_real_to_prot) // back to protected mode

	xor	%ax, %ax
	movb	%bh, %al		// return value in %ax

//	pop	%gs
//	pop	%fs
	pop	%es
	pop	%ds
	pop	%edi
	pop	%esi
	pop	%edx
	pop	%ecx
	pop	%ebx

	pop	%ebp

	ret

// putc(ch)
// BIOS call "INT 10H Function 0Eh" to write character to console
//	Call with	%ah = 0x0e
//			%al = character
//			%bh = page
//			%bl = foreground color ( graphics modes)


ENTRY(_putc)
	push	%ebp
	mov	%esp, %ebp

	push	%ebx
	push	%ecx
	push	%edx
	push	%esi
	push 	%edi
	push	%ds
	push	%es

	movb	0x8(%ebp), %cl

	call	EXT(_prot_to_real)

//	data16
//	mov	$0x13, %ebx	// colors in 2 bit palette, grey and white
//	mov	$0x1, %ebx	// %bh=0, %bl=1 (blue)
//
//	movb	$1,%bh		// background gray
	movb	$0,%bh		// background gray
	movb	$3,%bl		// foreground white in 2 bit palette

	movb	$0xe, %ah
	movb	%cl, %al

	int	$0x10		// display a byte

	data16
	call	EXT(_real_to_prot)

	pop	%es
	pop	%ds
	pop	%edi
	pop	%esi
	pop	%edx
	pop	%ecx
	pop	%ebx

	pop	%ebp
	ret

ENTRY(cputc)
	push	%ebp
	mov	%esp, %ebp

	push	%ebx
	push	%ecx
	push	%edx
	push	%esi
	push 	%edi
	push	%ds
	push	%es

	movb	0x8(%ebp), %cl
	movb	0x12(%ebp), %bl
	
	call	EXT(_prot_to_real)

	movb	$0,%bh		// page 0

	movb	%cl, %al
	movb	$0x9, %ah
	movb	$1, %cx
	
	int	$0x10		// display a byte

	data16
	call	EXT(_real_to_prot)

	pop	%es
	pop	%ds
	pop	%edi
	pop	%esi
	pop	%edx
	pop	%ecx
	pop	%ebx

	pop	%ebp
	ret

// bgetc()
// BIOS call "INT 16H Function 00H" to read character from keyboard
//	Call with	%ah = 0x0
//	Return:		%ah = keyboard scan code
//			%al = ASCII character

ENTRY(bgetc)
	push	%ebp
	mov	%esp, %ebp

	push	%ebx
	push	%ecx
	push	%edx
	push	%esi
	push 	%edi
	push	%ds
	push	%es

	call	EXT(_prot_to_real)

	movb	$0, %ah
	
	int	$0x16

	mov	%eax, %ebx	// _real_to_prot uses %eax

	data16
	call	EXT(_real_to_prot)

	xor	%eax, %eax
	mov	%bx, %ax

	pop	%es
	pop	%ds
	pop	%edi
	pop	%esi
	pop	%edx
	pop	%ecx
	pop	%ebx

	pop	%ebp
	ret

// readKeyboardStatus()
//       if there is a character pending, return it; otherwise return 0
// BIOS call "INT 16H Function 01H" to check whether a character is pending
//	Call with	%ah = 0x1
//	Return:
//		If key waiting to be input:
//			%ah = keyboard scan code
//			%al = ASCII character
//			Zero flag = clear
//		else
//			Zero flag = set

ENTRY(_readKeyboardStatus)
	push	%ebp
	mov	%esp, %ebp

	push	%ebx
	push	%ecx
	push	%edx
	push	%esi
	push 	%edi
	push	%ds
	push	%es

	call	EXT(_prot_to_real)		// enter real mode

	xor	%ebx, %ebx
	movb	$0x1, %ah
	int	$0x16

	data16
	jz	nochar

	movw	%ax, %bx

nochar:
	data16
	call	EXT(_real_to_prot)

	xor	%eax, %eax
	movw	%bx, %ax

	pop	%es
	pop	%ds
	pop	%edi
	pop	%esi
	pop	%edx
	pop	%ecx
	pop	%ebx

	pop	%ebp
	ret


// time in 18ths of a second
ENTRY(_time18)
	push	%ebp
	mov	%esp, %ebp

	push	%ebx
	push	%ecx
	push	%edx
	push	%esi
	push 	%edi
	push	%ds
	push	%es

	call	EXT(_prot_to_real)		// enter real mode

	xor	%bx, %bx
	xor	%eax, %eax
	int	$0x1a

	mov	%edx,%ebx
	shl	$16,%cx				// shifts ecx
	or	%cx,%bx				// %ebx has 32 bit time

	data16
	call	EXT(_real_to_prot)

	mov	%ebx,%eax

	pop	%es
	pop	%ds
	pop	%edi
	pop	%esi
	pop	%edx
	pop	%ecx
	pop	%ebx

	pop	%ebp
	ret

//
// get_diskinfo():  return a word that represents the
//	max number of sectors and  heads and drives for this device
//

ENTRY(_get_diskinfo)
	push	%ebp
	mov	%esp, %ebp

	push	%ebx
	push	%ecx
	push	%edx
	push	%esi
	push 	%edi
	push	%ds
	push	%es

	movb	0x8(%ebp), %dl		// diskinfo(drive #)
	call	EXT(_prot_to_real)	// enter real mode

	movb	$0x8, %ah		// ask for disk info
	int	$0x13

	data16
	call	EXT(_real_to_prot)	// back to protected mode

//	form a longword representing all this gunk
	mov	%ecx, %eax
	shrb	$6, %al
	andb	$0x03, %al
	xchgb	%ah, %al		// ax has max cyl
	shl	$16, %eax
	
	movb	%dh, %ah		// # heads
	andb	$0x3f, %cl		// mask of cylinder gunk
	movb	%cl, %al		// # sectors

	pop	%es
	pop	%ds
	pop	%edi
	pop	%esi
	pop	%edx
	pop	%ecx
	pop	%ebx

	pop	%ebp
	ret
#endif
#if 0
//
// getEisaInfo(<slot>):  return an int that represents the
//	vendor id for the specified slot (0 < slot < 64)
//	returns 0 for any error
//	returns bytes in the order [0 1 2 3] in %eax;

ENTRY(getEisaInfo)
	push	%ebp
	mov	%esp, %ebp

	push	%ebx
	push	%ecx
	push	%edx
	push	%esi
	push 	%edi
	push	%ds
	push	%es

	movb	0x8(%ebp), %cl		// slot #
	call	EXT(_prot_to_real)	// enter real mode

	movb	$0,%al
	movb	$0xd8, %ah		// eisa slot info
	int	$0x15

	data16
	call	EXT(_real_to_prot)	// back to protected mode

	movb	%ah,%bl
	xor	%eax,%eax

	test	$0,%bl
	jne	eisaerr

//	form a longword representing all this gunk
	mov	%esi, %eax
	shl	$16, %eax
	mov	%di,%ax
	bswap	%eax

eisaerr:	
	pop	%es
	pop	%ds
	pop	%edi
	pop	%esi
	pop	%edx
	pop	%ecx
	pop	%ebx

	pop	%ebp
	ret
#endif

#if 0
//
// memsize(i) :  return the memory size in KB. i == 0 for conventional memory,
//		i == 1 for extended memory
//	BIOS call "INT 12H" to get conventional memory size
//	BIOS call "INT 15H, AH=88H" to get extended memory size
//		Both have the return value in AX.
//

ENTRY(_memsize)
	push	%ebp
	mov	%esp, %ebp

	push	%ebx
	push	%ecx
	push	%edx
	push	%esi
	push 	%edi
	push	%ds
	push	%es

	mov	8(%ebp), %ebx
	call	EXT(_prot_to_real)		// enter real mode
	cmpb	$0x1, %bl
	data16
	je	xext
	int	$0x12
	data16
	jmp	xdone
xext:
	movb	$0x88, %ah
	int	$0x15
xdone:
	mov	%eax, %ebx
	data16
	call	EXT(_real_to_prot)
	xor	%eax, %eax
	mov	%ebx, %eax

	pop	%es
	pop	%ds
	pop	%edi
	pop	%esi
	pop	%edx
	pop	%ecx
	pop	%ebx

	pop	%ebp
	ret


// video_mode(mode)
// BIOS call "INT 10H Function 0h" to set vga graphics mode


ENTRY(_video_mode)
	push	%ebp
	mov	%esp, %ebp

	push	%ebx
	push	%ecx
	push	%edx
	push	%esi
	push 	%edi
	push	%ds
	push	%es

	movb	0x8(%ebp), %cl

	call	EXT(_prot_to_real)

	movb	$0, %ah
	movb	%cl, %al

	int	$0x10		// display a byte

	data16
	call	EXT(_real_to_prot)

	pop	%es
	pop	%ds
	pop	%edi
	pop	%esi
	pop	%edx
	pop	%ecx
	pop	%ebx

	pop	%ebp
	ret



// setCursorPosition(x,y)
// BIOS call "INT 10H Function 2" to set cursor position


ENTRY(_setCursorPosition)
	push	%ebp
	mov	%esp, %ebp

	push	%ebx
	push	%ecx
	push	%edx
	push	%esi
	push 	%edi
	push	%ds
	push	%es

	movb	0x8(%ebp), %cl
	movb	0xc(%ebp), %ch

	call	EXT(_prot_to_real)

	movb	$2, %ah		// setcursor function
	movb	$0, %bh		// page num 0 for graphics
	movb	%cl, %dl	// column, x
	movb	%ch, %dh	// row, y

	int	$0x10		// set cursor

	data16
	call	EXT(_real_to_prot)

	pop	%es
	pop	%ds
	pop	%edi
	pop	%esi
	pop	%edx
	pop	%ecx
	pop	%ebx

	pop	%ebp
	ret
#endif

