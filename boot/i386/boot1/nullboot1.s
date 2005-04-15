; Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
;
; @APPLE_LICENSE_HEADER_START@
; 
; Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
; Reserved.  This file contains Original Code and/or Modifications of
; Original Code as defined in and that are subject to the Apple Public
; Source License Version 1.1 (the "License").  You may not use this file
; except in compliance with the License.  Please obtain a copy of the
; License at http://www.apple.com/publicsource and read it before using
; this file.
; 
; The Original Code and all software distributed under the License are
; distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
; EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
; INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
; License for the specific language governing rights and limitations
; under the License.
; 
; @APPLE_LICENSE_HEADER_END@
;
; nullboot.s - boot1 written for nasm assembler, since gas only
; generates 32 bit code and this must run in real mode.
; To compile as floppy boot1f.not:
;	nasm -dBOOTDEV=FLOPPY nullboot1.s -o nullboot1

;***********************************************************************
;	This is the code for the NeXT boot1 bootsector.
;***********************************************************************

	SEGMENT .text

	SDEBUG EQU 0

BOOTSEG		EQU	00h
BOOTOFF		EQU	1000h
BUFSZ		EQU	2000h	; 8K disk transfer buffer

; This code is a replacement for boot1.  It is loaded at 0x0:0x7c00

start:
	mov	ax,BOOTSEG
	cli			; interrupts off
	mov	ss,ax		; set up stack seg
	mov	sp,0fff0h
	sti			; reenable interrupts

	xor	ax,ax
	mov	es,ax
	mov	ds,ax
	mov	si,7C00h
	cld			; so pointers will get updated
	mov	di,0E000h	; relocate boot program to 0xE000
	mov	cx,100h		; copy 256x2 bytes
	repnz	movsw		; move it
	jmp	0000:0E000h + (a1 - start)	; jump to a1 in relocated place

a1:
	mov	ax,0E00h
	mov	ds,ax
	mov	ax,BOOTSEG
	mov	es,ax

	mov	si, not_boot
	call	message		; display intro message

halt:
	mov	ah, 00h
	int	16h
	jmp	short halt		; get key and loop forever

message:				; write the error message in ds:esi
					; to console
	push	es
	mov	ax,ds
	mov	es,ax

	mov	bx, 1			; bh=0, bl=1 (blue)
	cld

nextb:
	lodsb				; load a byte into al
	cmp	al, 0
	je	done
	mov	ah, 0eh			; bios int 10, function 0xe
	int	10h			; bios display a byte in tty mode
	jmp	short nextb
done:	pop	es
	ret

putchr:
	push	bx
	mov	bx, 1			; bh=0, bl=1 (blue)
	mov	ah, 0eh			; bios int 10, function 0xe
	int	10h			; bios display a byte in tty mode
	pop	bx
	ret


not_boot:
	db	10,13
	db     'The disk in the floppy disk drive isn',39,'t a startup disk:'
	db	10,13
	db	'It doesn',39,'t contain '	; 39 = a ' char
 	db	'the system files required to start up the computer.'
	db	10,13
	db	'Please eject this disk and restart the computer'
	db	' with a floppy disk,'
	db	10,13
	db	'hard disk, or CD-ROM that is a startup disk.'
	db	10,13
	db	0

; the last 2 bytes in the sector contain the signature
d1:
	a2 equ 510 - (d1 - start)
  times a2 db 0				; fill the rest with zeros
	dw 0AA55h
	ENDS
	END
