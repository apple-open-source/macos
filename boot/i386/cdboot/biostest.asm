; Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
;
; @APPLE_LICENSE_HEADER_START@
; 
; Portions Copyright (c) 2002-2003 Apple Computer, Inc.  All Rights
; Reserved.  This file contains Original Code and/or Modifications of
; Original Code as defined in and that are subject to the Apple Public
; Source License Version 2.0 (the "License").  You may not use this file
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

kStackAddr	EQU  0xFFF0
	
		code16
		ORG 0x7c00

		SEGMENT .text
	
boot		equ $

_start:		cli
		jmp 0:start
		times 8-($-$$) nop	; Ensure table is at offset 8

		; El Torito boot information table, filled in by the
		; mkisofs -boot-info-table option
bi_pvd:		dd 0			; LBA of primary volume descriptor
bi_file:	dd 0			; LBA of boot file
bi_length:	dd 0			; Length of boot file
bi_csum:	dd 0			; Checksum of boot file
bi_reserved:	times 10 dd 0		; Reserved
	


start:
	xor ax, ax
	mov ss, ax
	mov sp, kStackAddr
	sti
	cld
	mov ds, ax
	mov es, ax
	mov [biosdrive], dl
	
	mov si, header_str
	call print_string

	mov si, drive_str	; print drive number
	call print_string
	mov al, dl
	call print_hex
	mov si, crnl_str
	call print_string
	
test1:				; Test 1: El Torito status call
	mov si, test1_str
	call print_string
	
	mov ax, 0x4b01
	mov al, 0x01
	;; dl already contains drive number
	mov esi, spec
	int 0x13

	jnc test1a		; CF clear?

test1_fail:
	mov si, test1_fail1_str
	call print_string
	jmp short test1_end
	
test1a:
	cmp [spec.drive], dl	; drive in spec packet matches?
	jne test1a_fail
	mov si, pass_str
	call print_string
	jmp short test1_end
	
test1a_fail:	
	mov si, test1_fail2_str
	call print_string

test1_end:	
	mov si, crnl_str
	call print_string
	

test2:				; Check for EBIOS support
	mov si, test2_str
	call print_string

	mov cx, 0
	mov ah, 0x41
	mov bx, 0x55aa
	mov dl, [biosdrive]
	int 0x13

	jnc test2a		; CF clear?

test2_fail:
	mov si, test2_fail1_str
	call print_string
	jmp short test2_end

test2a:
	cmp bx, 0xaa55
	je test2b

test2a_fail:
	mov si, test2_fail2_str
	call print_string
	jmp short test2_end

test2b:
	mov si, test2_vers_str	; print EDD version
	call print_string
	push cx
	mov al, ah
	call print_hex
	mov si, test2_flag_str	; print EDD flags
	call print_string
	mov al, cl
	call print_hex
	mov al, ' '
	call putc
	pop cx
	and cl, 0x01		; EDD drive access?
	jnz test2_pass

test2b_fail:
	mov si, test2_fail3_str
	call print_string
	jmp short test2_end

test2_pass
	mov si, pass_str
	call print_string
	
test2_end:	
	mov si, crnl_str
	call print_string

test3:				; Disk geometry
	mov si, test3_str
	call print_string	

	mov ah, 0x48
	mov dl, [biosdrive]
	mov si, disk_params
	int 0x13

	jnc test3a

test3_fail:
	mov si, test3_fail1_str
	call print_string
	jmp test3_end

test3a:
	cmp word [disk_params.nbps], 0x0800
	je test3_pass

test3a_fail:
	mov al, [disk_params.nbps+1]
	call print_hex
	mov al, [disk_params.nbps]
	call print_hex
	mov al, ' '
	call putc
	mov si, test3_fail2_str
	call print_string
	jmp test3_end
	
test3_pass:
	mov si, pass_str
	call print_string
	
test3_end:
	mov si, crnl_str
	call print_string
	
	
finished:	
	mov si, finished_str
	call print_string
	
halt:	hlt
	jmp short halt

	

	;; ----- Helper functions -----

	;; putc(ch)
	;; character in AL
	;; clobbers AX, BX
putc:	mov bx, 0x000f
	mov ah, 0x0e
	int 0x10
	ret

	;; getc()
	;; clobbers AH
	;; returns character in AL
getc:	mov ah, 0
	int 0x16
	ret

;--------------------------------------------------------------------------
; Write the byte value to the console in hex.
;
; Arguments:
;   AL = Value to be displayed in hex.
;
print_hex:
    push    ax
    ror     al, 4
    call    print_nibble            ; display upper nibble
    pop     ax
    call    print_nibble            ; display lower nibble
    ret

print_nibble:
    and     al, 0x0f
    add     al, '0'
    cmp     al, '9'
    jna     .print_ascii
    add     al, 'A' - '9' - 1
.print_ascii:
    call    putc
    ret

;--------------------------------------------------------------------------
; Write a string to the console.
;
; Arguments:
;   DS:SI   pointer to a NULL terminated string.
;
; Clobber list:
;   none
;
print_string:
    pushad
    mov     bx, 1                   ; BH=0, BL=1 (blue)
.loop
    lodsb                           ; load a byte from DS:SI into AL
    cmp     al, 0                   ; Is it a NULL?
    je      .exit                   ; yes, all done
    mov     ah, 0xE                 ; INT10 Func 0xE
    int     0x10                    ; display byte in tty mode
    jmp     short .loop
.exit
    popad
    ret


	SEGMENT .data
	
	;; Strings

header_str:	db "BIOS test v0.3", 10,13, 0
finished_str:	db "Tests completed.", 10,13,0
drive_str:	db "Boot drive: ", 0
pass_str:	db "pass", 0
fail_str:	db "FAIL", 0
crnl_str:	db 10,13, 0
test1_str:	db "Test 1: El Torito status: ", 0
test1_fail1_str:	db "FAIL (CF=1)", 0
test1_fail2_str:	db "FAIL (spec drive)", 0
test2_str:	db "Test 2: EBIOS check: ", 0
test2_vers_str:	db "vers. ", 0
test2_flag_str:	db " flags ", 0
test2_fail1_str:	db "FAIL (CF=1)", 0
test2_fail2_str:	db "FAIL (BX != AA55)", 0
test2_fail3_str:	db "FAIL (CL & 1 == 0)", 0
test3_str:	db "Test 3: Geometry check: ", 0
test3_fail1_str:	db "FAIL (CF=1)", 0
test3_fail2_str:	db "FAIL (nbps != 0x200)", 0
	
	;; Other variables
	
biosdrive:	db 0

	;; El Torito spec packet
spec:	
.size:		db 0x13
.type:		db 0
.drive:		db 0
.index:		db 0
.lba:		dd 0
.spec:		dw 0
.userseg:	dw 0
.loadseg:	dw 0
.seccnt:	dw 0
.cylcnt:	db 0
.ccl:		db 0
.hdcnt:		db 0

	;; Disk parameter packet
disk_params:	
.size:		dw 66
.flags:		dw 0
.cyls:		dd 0
.heads:		dd 0
.spt:		dd 0
.sectors:	dd 0,0
.nbps:		dw 0
.dpte_offset:	dw 0
.dpte_segment:	dw 0
.key:		dw 0
.ignored:	times 34 db 0

	
