; Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
;
; @APPLE_LICENSE_HEADER_START@
; 
; Portions Copyright (c) 2003 Apple Computer, Inc.  All Rights
; Reserved.  This file contains Original Code and/or Modifications of
; Original Code as defined in and that are subject to the Apple Public
; Source License Version 1.2 (the "License").  You may not use this file
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

DEBUG	EQU 0
	
%macro DebugCharMacro 1
    push  ax
    mov   al, %1
    call  putc
    pop ax
%endmacro
%macro DebugPauseMacro 0
    push ax
    call getc
    pop ax
%endmacro
	
%if DEBUG
%define DebugChar(x)  DebugCharMacro x
%define DebugPause(x)  DebugPauseMacro
%else
%define DebugChar(x)
%define DebugPause(x)
%endif

kBoot2Sectors        EQU  112           ; sectors to load for boot2
kBoot2Address        EQU  0x0200        ; boot2 load address
kBoot2Segment        EQU  0x2000        ; boot2 load segment
	
kBoot0Stack	     EQU  0xFFF0        ; boot0 stack pointer
	
kReadBuffer          EQU  0x1000        ; disk data buffer address

kVolSectorOffset     EQU  0x47          ; offset in buffer of sector number
                                        ; in volume descriptor
kBootSectorOffset    EQU  0x28          ; offset in boot catalog
                                        ; of sector number to load boot file
kBootCountOffset     EQU  0x26          ; offset in boot catalog 
                                        ; of number of sectors to read

        
        code16
        ORG 0x7c00

        SEGMENT .text

start:
        cli
	jmp 0:start1
	times 8-($-$$) nop	; Put boot information table at offset 8
        
        ; El Torito boot information table, filled in by the
	; mkisofs -boot-info-table option, if used.
bi_pvd:         dd 0			; LBA of primary volume descriptor
bi_file:        dd 0			; LBA of boot file
bi_length:	dd 0			; Length of boot file
bi_csum:	dd 0			; Checksum of boot file
bi_reserved:	times 10 dd 0		; Reserved

start1:
	xor ax, ax			; zero %ax
	mov ss, ax			; setup the
	mov sp, kBoot0Stack		;  stack
	sti
	cld                             ; increment SI after each lodsb call
	mov ds, ax			; setup the
 	mov es, ax			;  data segments
        ;; BIOS boot drive is in DL

        DebugChar('!')
        DebugPause()

%if DEBUG
	mov eax, [kBoot2LoadAddr]
	call print_hex
        call getc
%endif
        
        ;;
        ;; The BIOS likely didn't load the rest of the booter,
        ;; so we have to fetch it ourselves.
        ;;
	xor ax, ax
        mov es, ax
        mov bx, kReadBuffer
        mov al, 1
        mov ecx, 17
        call read_lba
        jc error

        DebugChar('A')
        
        mov ecx, [kReadBuffer + kVolSectorOffset]
%if DEBUG
        mov eax, ecx
        call print_hex
        DebugPause()
%endif
        mov al, 1
        call read_lba
        jc error        
 
        ;; Now we have the boot catalog in the buffer.
        ;; Really we should look at the validation entry, but oh well.

	DebugChar('B')
	
        mov ecx, [kReadBuffer + kBootSectorOffset]
        inc ecx                 ; skip the first sector which is what we are in
%if DEBUG
        mov eax, ecx
        call print_hex
        DebugPause()
%endif
        
        mov ax, kBoot2Segment
        mov es, ax
	
        mov al, kBoot2Sectors / 4
        mov bx, kBoot2Address
        call read_lba
        jc error
        
	DebugChar('C')
%if DEBUG
	mov eax, [es:kBoot2Address]
	call print_hex
        DebugPause()
%endif
	
	xor ax, ax
        mov es, ax
	
	DebugChar('X')
        DebugPause()
        
	;; Jump to newly-loaded booter
        jmp     kBoot2Segment:kBoot2Address

error:
        mov al, 'E'
        call putc
        hlt
        
        ;; 
        ;; Support functions
        ;;
        
;--------------------------------------------------------------------------
; read_lba - Read sectors from CD using LBA addressing.
;
; Arguments:
;   AL = number of 2048-byte sectors to read (valid from 1-127).
;   ES:BX = pointer to where the sectors should be stored.
;   ECX = sector offset in partition 
;   DL = drive number (0x80 + unit number)
;
; Returns:
;   CF = 0  success
;        1 error
;
read_lba:
        pushad                           ; save all registers
        mov     bp, sp                  ; save current SP

        ;
        ; Create the Disk Address Packet structure for the
        ; INT13/F42 (Extended Read Sectors) on the stack.
        ;

        ; push    DWORD 0               ; offset 12, upper 32-bit LBA
        push    ds                      ; For sake of saving memory,
        push    ds                      ; push DS register, which is 0.

        push    ecx

        push    es                      ; offset 6, memory segment

        push    bx                      ; offset 4, memory offset

        xor     ah, ah                  ; offset 3, must be 0
        push    ax                      ; offset 2, number of sectors

        push    WORD 16                 ; offset 0-1, packet size

        ;
        ; INT13 Func 42 - Extended Read Sectors
        ;
        ; Arguments:
        ;   AH    = 0x42
        ;   DL    = drive number (80h + drive unit)
        ;   DS:SI = pointer to Disk Address Packet
        ;
        ; Returns:
        ;   AH    = return status (sucess is 0)
        ;   carry = 0 success
        ;           1 error
        ;
        ; Packet offset 2 indicates the number of sectors read
        ; successfully.
        ;
        mov     si, sp
        mov     ah, 0x42
        int     0x13

        jnc     .exit

        DebugChar('R')                  ; indicate INT13/F42 error

        ;
        ; Issue a disk reset on error.
        ; Should this be changed to Func 0xD to skip the diskette controller
        ; reset?
        ;
        xor     ax, ax                  ; Func 0
        int     0x13                    ; INT 13
        stc                             ; set carry to indicate error

.exit:
        mov     sp, bp                  ; restore SP
        popad
        ret
	
;;
;; Display a single character from AL.
;;
putc:	pushad
	mov bx, 0x1			; attribute for output
	mov ah, 0xe			; BIOS: put_char
	int 0x10			; call BIOS, print char in %al
	popad
	ret

;; 
;; Get a character from the keyboard and return it in AL.
;; 
getc:	mov ah, 0
	int 0x16
	ret
	
%if DEBUG
;--------------------------------------------------------------------------
; Write the 4-byte value to the console in hex.
;
; Arguments:
;   EAX = Value to be displayed in hex.
;
print_hex:
    pushad
    mov     cx, WORD 4
    bswap   eax
.loop
    push    ax
    ror     al, 4
    call    print_nibble            ; display upper nibble
    pop     ax
    call    print_nibble            ; display lower nibble
    ror     eax, 8
    loop    .loop

    mov     al, 10                  ; carriage return
    call    putc
    mov     al, 13
    call    putc
    popad
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

%endif ; DEBUG
        
	

        ;; Pad this file to a size of 2048 bytes (one CD sector).
pad:
        times 2048-($-$$) db 0

        ;; Location of loaded boot2 code.
kBoot2LoadAddr  equ  $

        END
