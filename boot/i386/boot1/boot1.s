; Copyright (c) 1999-2002 Apple Computer, Inc. All rights reserved.
;
; @APPLE_LICENSE_HEADER_START@
; 
; Portions Copyright (c) 1999-2002 Apple Computer, Inc.  All Rights
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
;
; Partition Boot Loader: boot1h
;
; This program is designed to reside in sector 0 of an HFS+ partition.
; The HFS+ partition can be a primary or a logical partition.
; It expects that the MBR has left the drive number in DL
; and a pointer to the partition entry in SI.
; 
; This version requires a BIOS with EBIOS (LBA) support.
;
; This code is written for the NASM assembler.
;   nasm boot0.s -o boot0


;
; Set to 1 to enable obscure debug messages.
;
DEBUG                EQU  0

;
; Set to 1 to support loading the booter (boot2) from a
; logical partition.
;
EXT_PART_SUPPORT     EQU  0

;
; Various constants.
;
kBoot0Segment        EQU  0x0000
kBoot0Stack          EQU  0xFFF0        ; boot0 stack pointer
kBoot0LoadAddr       EQU  0x7C00        ; boot0 load address
kBoot0RelocAddr      EQU  0xE000        ; boot0 relocated address

ebios_lba            EQU  0xEF00	; storage for variables

kMBRBuffer           EQU  0x1000        ; MBR buffer address
kExtBuffer           EQU  0x1200        ; EXT boot block buffer address

kPartTableOffset     EQU  0x1be
kMBRPartTable        EQU  kMBRBuffer + kPartTableOffset
kExtPartTable        EQU  kExtBuffer + kPartTableOffset

kBoot2Sectors        EQU  112           ; sectors to load for boot2
kBoot2Address        EQU  0x0000        ; boot2 load address
kBoot2Segment        EQU  0x2000        ; boot2 load segment

kSectorBytes         EQU  512           ; sector size in bytes
kBootSignature       EQU  0xAA55        ; boot sector signature

kPartCount           EQU  4             ; number of paritions per table
kPartTypeBoot        EQU  0xab          ; boot2 partition type
kPartTypeHFS         EQU  0xaf
kPartTypeExtDOS      EQU  0x05          ; DOS extended partition type
kPartTypeExtWin      EQU  0x0f          ; Windows extended partition type
kPartTypeExtLinux    EQU  0x85          ; Linux extended partition type
	
kPartActive	     EQU  0x80

;;
;; HFS constants
;;
kHFSSig              EQU  0x4442	; HFS volume signature
kAlBlStOffset        EQU  0x1c
kEmbedStartOffset    EQU  0x7e
kAlBlkSizOffset      EQU  0x14

;;
;; HFS+ constants
;;
kHFSPlusSig          EQU  0x2B48	; HFS+ volume signature
kBlockSizeOffset     EQU  0x28
kExtentOffset        EQU  0x1c0
	
kHFSBuffer           EQU  0x1400        ; HFS volume header address

kHFSSigAddr          EQU  kHFSBuffer
kHFSAlBlSt           EQU  kHFSBuffer + kAlBlStOffset
kHFSEmbedStart       EQU  kHFSBuffer + kEmbedStartOffset
kHFSAlBlkSiz         EQU  kHFSBuffer + kAlBlkSizOffset

kHFSPlusSigAddr      EQU  kHFSBuffer
kHFSPlusBlockSize    EQU  kHFSBuffer + kBlockSizeOffset
kHFSPlusExtent       EQU  kHFSBuffer + kExtentOffset


%ifdef FLOPPY
kDriveNumber         EQU  0x00
%else
kDriveNumber         EQU  0x80
%endif

;
; Format of fdisk partition entry.
;
; The symbol 'part_size' is automatically defined as an `EQU'
; giving the size of the structure.
;
           struc part
.bootid:   resb 1      ; bootable or not 
.head:     resb 1      ; starting head, sector, cylinder
.sect:     resb 1      ;
.cyl:      resb 1      ;
.type:     resb 1      ; partition type
.endhead   resb 1      ; ending head, sector, cylinder
.endsect:  resb 1      ;
.endcyl:   resb 1      ;
.lba:      resd 1      ; starting lba
.sectors   resd 1      ; size in sectors
           endstruc

;
; Macros.
;
%macro DebugCharMacro 1
    mov   al, %1
    call  print_char
    call getc
%endmacro

%if DEBUG
%define DebugChar(x)  DebugCharMacro x
%else
%define DebugChar(x)
%endif

	
;--------------------------------------------------------------------------
; Start of text segment.

    SEGMENT .text

    ORG     0xE000                  ; must match kBoot0RelocAddr

;--------------------------------------------------------------------------
; Boot code is loaded at 0:7C00h.
;
start
    ;
    ; Set up the stack to grow down from kBoot0Segment:kBoot0Stack.
    ; Interrupts should be off while the stack is being manipulated.
    ;
    cli                         ; interrupts off
    xor     ax, ax                  ; zero ax
    mov     ss, ax                  ; ss <- 0
    mov     sp, kBoot0Stack         ; sp <- top of stack
    sti                         ; reenable interrupts

    mov     es, ax                  ; es <- 0
    mov     ds, ax                  ; ds <- 0

    DebugChar('H')

%if DEBUG
    mov     eax, [si + part.lba]
    call    print_hex
%endif

    ;
    ; Clear various flags in memory.
    ;
    xor     eax, eax
    mov     [ebios_lba], eax        ; clear EBIOS LBA offset

    cmp     BYTE [si + part.type], kPartTypeHFS
    jne     .part_err
    cmp     BYTE [si + part.bootid], kPartActive
    jne     .part_err

    jmp     find_startup
	
.part_err:
    DebugChar('P')
    jmp     hang

;;; ---------------------------------------
;;; 
;;; find_startup - Find HFS+ startup file in a partition.
;;;
;;; Arguments:
;;;   DL = drive number (0x80 + unit number)
;;;   SI = pointer to the partition entry.
;;;
;;; On success, loads booter and jumps to it.
;;;
find_startup:
    DebugChar(']')		

    mov     al, 1                   ; read 1 sector
    xor     bx, bx
    mov     es, bx                  ; es = 0
    mov     bx, kHFSBuffer          ; load volume header
    mov     ecx, DWORD 2
    call    load
    jnc      .ok                    ; load error

    jmp     startup_err

.ok
    mov     ax, [kHFSSigAddr]
    cmp     ax, kHFSSig
    jne     .hfs_plus

    DebugChar('|')
    mov     ebx, [kHFSAlBlkSiz]
    bswap   ebx
    sar     ebx, 9

    xor     eax, eax
    mov     ax, [kHFSEmbedStart]
    xchg    al, ah		; byte-swap
    push    dx
    mul     ebx 		; result in eax
    pop     dx

    xor     ebx, ebx
    mov     bx, [kHFSAlBlSt]
    xchg    bl, bh		; byte-swap
    add     eax, ebx

    ;; now eax has sector of HFS+ partition
    inc     eax
    inc     eax
    mov     ecx, eax

    mov     al, 1                   ; read 1 sector
    xor     bx, bx
    mov     es, bx                  ; es = 0
    mov     bx, kHFSBuffer          ; load volume header
    call    load
    jc      startup_err             ; load error

.hfs_plus
    DebugChar('}')
    mov     ax, [kHFSPlusSigAddr]
    cmp     ax, kHFSPlusSig
    jne     startup_err

;;; Now the HFS+ volume header is in our buffer.

    DebugChar('*')
    mov     eax, [kHFSPlusBlockSize]
    bswap   eax
    sar     eax, 9

    mov     ebx, [kHFSPlusExtent]
    bswap   ebx
    push    dx
    mul     ebx 		;  result in eax
    pop     dx

    dec     eax
    dec     eax
;     add     [ebios_lba], eax 		;  offset to startup file
;     mov     ecx, [ebios_lba]
    add     ecx, eax

    DebugChar('!')	
	
    mov     al, kBoot2Sectors
    mov     bx, kBoot2Segment
    mov     es, bx
    mov     bx, kBoot2Address + kSectorBytes
    call    load
    jc      startup_err

    DebugChar('Y')
    ;
    ; Jump to boot2. The drive number is already in register DL.
    ;
    jmp     kBoot2Segment:kBoot2Address + kSectorBytes

startup_err:

    DebugChar('X')
	
hang:
    hlt
    jmp     SHORT hang

;--------------------------------------------------------------------------
; load - Load one or more sectors from a partition.
;
; Arguments:
;   AL = number of 512-byte sectors to read.
;   ES:BX = pointer to where the sectors should be stored.
;   ECX = sector offset in partition
;   DL = drive number (0x80 + unit number)
;   SI = pointer to the partition entry.
;
; Returns:
;   CF = 0  success
;        1 error
;
; load:
; ;    push    cx

; .ebios:
; ;    mov     cx, 5                   ; load retry count
; .ebios_loop:
;     call    read_lba                ; use INT13/F42
;     jnc     .exit
; ;     loop    .ebios_loop

; .exit
;     pop     cx
;     ret


;--------------------------------------------------------------------------
; read_lba - Read sectors from a partition using LBA addressing.
;
; Arguments:
;   AL = number of 512-byte sectors to read (valid from 1-127).
;   ES:BX = pointer to where the sectors should be stored.
;   ECX = sector offset in partition 
;   DL = drive number (0x80 + unit number)
;   SI = pointer to the partition entry.
;
; Returns:
;   CF = 0  success
;        1 error
;
read_lba:
load:	
    pushad                           ; save all registers
    mov     bp, sp                  ; save current SP

;
    ; Create the Disk Address Packet structure for the
    ; INT13/F42 (Extended Read Sectors) on the stack.
    ;

    ; push    DWORD 0               ; offset 12, upper 32-bit LBA
    push    ds                      ; For sake of saving memory,
    push    ds                      ; push DS register, which is 0.

    add     ecx, [ebios_lba]        ; offset 8, lower 32-bit LBA
    add     ecx, [si + part.lba]

    push    ecx

    push    es                      ; offset 6, memory segment

    push    bx                      ; offset 4, memory offset

    xor     ah, ah                  ; offset 3, must be 0
    push    ax                      ; offset 2, number of sectors

%if DEBUG
    push ax
    DebugChar('-')		; absolute sector offset
    mov     eax, ecx
    call    print_hex
    DebugChar('=')		; count
    pop ax
    call print_hex
%endif

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
;   mov     dl, kDriveNumber
    mov     si, sp
    mov     ah, 0x42
    int     0x13

    jnc     .exit

%if DEBUG
    call print_hex
%endif
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

%if 0
;-------------------------------------------------------------------------
; Write a string to the console.
;
; Arguments:
;   DS:SI   pointer to a NULL terminated string.
;
; Clobber list:
;   AX, BX, SI
;
print_string
    mov     bx, 1                   ; BH=0, BL=1 (blue)
    cld                         ; increment SI after each lodsb call
.loop
    lodsb                           ; load a byte from DS:SI into AL
    cmp     al, 0               ; Is it a NULL?
    je      .exit                   ; yes, all done
    mov     ah, 0xE                 ; INT10 Func 0xE
    int     0x10                    ; display byte in tty mode
    jmp     short .loop
.exit
    ret
%endif

%if DEBUG

;--------------------------------------------------------------------------
; Write a ASCII character to the console.
;
; Arguments:
;   AL = ASCII character.
;
print_char
    pushad
    mov     bx, 1                   ; BH=0, BL=1 (blue)
    mov     ah, 0x0e                ; bios INT 10, Function 0xE
    int     0x10                    ; display byte in tty mode
    popad
    ret

;--------------------------------------------------------------------------
; Write a variable number of spaces to the console.
;
; Arguments:
;   AL = number to spaces.
;
print_spaces:
    pushad
    xor     cx, cx
    mov     cl, al                  ; use CX as the loop counter
    mov     al, ' '             ; character to print
.loop:
    jcxz    .exit
    call    print_char
    loop    .loop
.exit:
    popad
    ret

;--------------------------------------------------------------------------
; Write the byte value to the console in hex.
;
; Arguments:
;   AL = Value to be displayed in hex.
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
    call    print_char
    mov     al, 13
    call    print_char
    popad
    ret

print_nibble:
    and     al, 0x0f
    add     al, '0'
    cmp     al, '9'
    jna     .print_ascii
    add     al, 'A' - '9' - 1
.print_ascii:
    call    print_char
    ret

getc:
    mov    ah, 0
    int    0x16
    ret

%endif ; DEBUG

;--------------------------------------------------------------------------
; NULL terminated strings.
;
; boot_error_str   db  10, 13, 'Error', 0

;--------------------------------------------------------------------------
; Pad the rest of the 512 byte sized booter with zeroes. The last
; two bytes is the mandatory boot sector signature.
;
; If the booter code becomes too large, then nasm will complain
; that the 'times' argument is negative.

pad_boot
    times 510-($-$$) db 0

%ifdef FLOPPY
;--------------------------------------------------------------------------
; Put fake partition entries for the bootable floppy image
;
part1bootid     db        0x80  ; first partition active
part1head       db        0x00  ; head #
part1sect       db        0x02  ; sector # (low 6 bits)
part1cyl        db        0x00  ; cylinder # (+ high 2 bits of above)
part1systid     db        0xab  ; Apple boot partition
times   3       db        0x00  ; ignore head/cyl/sect #'s
part1relsect    dd  0x00000001  ; start at sector 1
part1numsect    dd  0x00000080  ; 64K for booter
part2bootid     db        0x00  ; not active
times   3       db        0x00  ; ignore head/cyl/sect #'s
part2systid     db        0xa8  ; Apple UFS partition
times   3       db        0x00  ; ignore head/cyl/sect #'s
part2relsect    dd  0x00000082  ; start after booter
; part2numsect  dd  0x00000abe  ; 1.44MB - 65K
part2numsect    dd  0x000015fe  ; 2.88MB - 65K
%endif

pad_table_and_sig
    times 510-($-$$) db 0
    dw    kBootSignature

    END
