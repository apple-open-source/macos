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
; Boot Loader: boot0
;
; A small boot sector program written in x86 assembly whose only
; responsibility is to locate the booter partition, load the
; booter into memory, and jump to the booter's entry point.
; The booter partition can be a primary or a logical partition.
; But the booter partition must reside within the 8GB limit
; imposed by CHS addressing + translation.
; 
; This boot loader can be placed at any of the following places:
; * Master Boot Record (MBR)
; * Boot sector of an extended partition
; * Boot sector of a primary partition
; * Boot sector of a logical partition
;
; In order to coexist with a fdisk partition table (64 bytes), and
; leave room for a two byte signature (0xAA55) in the end, boot0 is
; restricted to 446 bytes (512 - 64 - 2). If boot0 did not have to
; live in the MBR, then we would have 510 bytes to play with.
;
; boot0 is always loaded by the BIOS or another first level booter
; to 0:7C00h.
;
; This code is written for the NASM assembler.
;   nasm boot0.s -o boot0


;--------------------------------------------------------------------------
; Constants.

FLOPPY          EQU  0x00               ; floppy dev number
HDISK           EQU  0x80               ; hard drive dev number
DEBUG           EQU  0                  ; enable debugging output

BOOTSEG         EQU  0x0                ; our sole segment
BOOTSP          EQU  0xFFF0             ; stack pointer
BOOTLOAD        EQU  0x7C00             ; booter load address
BOOTRELOC       EQU  0xE000             ; booter is relocated here
BOOTSIG         EQU  0xAA55             ; booter signature

BOOT2_SIZE      EQU  112                ; load this many blocks for boot2
BOOT2_ADDR      EQU  0x0200             ; where to load boot2
BOOT2_SEG       EQU  0x2000

%IF BOOTDEV = FLOPPY
DRIVE_NUM       EQU  FLOPPY             ; floppy drive
%ELSE
DRIVE_NUM       EQU  HDISK              ; "C" drive
%ENDIF
SECTOR_BYTES    EQU  512                ; sector size in bytes

BUF_MBR         EQU  0x1000             ; memory buffer for MBR
BUF_EXT         EQU  0x1200             ; memory buffer for extended partition

TABLE_MAIN      EQU  BUF_MBR + 0x1be    ; location of main partition table
TABLE_EXT       EQU  BUF_EXT + 0x1be    ; location of ext partition table
ENTRY_SIZE      EQU  16                 ; size of each fdisk partition entry
TYPE_BOOT       EQU  0xab               ; partition type we are looking for
TYPE_EXT        EQU  0x05               ; extended partition type
TYPE_EXT_1      EQU  0x0f               ; Windows extended partition
TYPE_EXT_2      EQU  0x85               ; Linux extended partition
EXT_LEVELS_MAX  EQU  128                ; max extended partition levels


;--------------------------------------------------------------------------
; Start of text segment.

    SEGMENT .text

    ORG     0xE000              ; must match BOOTRELOC

;--------------------------------------------------------------------------
; Loaded at 0:7c00h.
;
start
    ; Set up the stack to grow down from BOOTSEG:BOOTSP.
    ; Interrupts should be off while the stack is being manipulated.
    ;
    cli                         ; interrupts off
    mov     ax, BOOTSEG         ;
    mov     ss, ax              ; ss <- BOOTSEG
    mov     sp, BOOTSP          ; sp <- BOOTSP
    sti                         ; reenable interrupts

    ; Relocate the booter code from DS:SI to ES:DI,
    ; or from 0:7C00h to BOOTSEG:BOOTRELOC.
    ;
    mov     es, ax              ; es <- BOOTSEG
    mov     ds, ax              ; ds <- BOOTSEG

    mov     si, BOOTLOAD        ; si <- BOOTLOAD (source)
    mov     di, BOOTRELOC       ; di <- BOOTRELOC (destination)
    ;
    cld                         ; auto-increment SI and/or DI registers
    mov     cx, 256             ; copy 256 words (512 bytes)
    repnz   movsw               ; repeat string move (word) operation

    ; Code relocated, jump to start_reloc in relocated location.
    ;
    jmp     BOOTSEG:start_reloc

;--------------------------------------------------------------------------
; Start execution from the relocated location.
;
start_reloc
    mov     al, '='             ; indicate execution start
    call    putchar

    ; Get disk parameters (CHS) using INT13/F8 call.
    ;
    mov     dl, DRIVE_NUM       ; boot drive is drive C
    mov     ah, 8               ; Read Disk Driver Parameter function
    int     0x13
    and     cl, 0x3f            ; sectors/track
    mov     [max_sectors], cl
    mov     [max_heads], dh
    jc      error

    mov     al, '>'             ; indicate INT13/F8 success
    call    putchar

    ; Since this code may not always reside in the MBR, we will always
    ; start by loading the MBR to BUF_MBR.
    ;
    mov     WORD [chs_cx], 0x0001       ; cyl = 0, sect = 1
    mov     BYTE [chs_dx + 1], 0        ; head = 0
    xor     cx, cx                      ; skip 0 sectors
    mov     ax, 1                       ; read 1 sector
    mov     bx, BUF_MBR                 ; load buffer
    call    load
    jc      error

    mov     di, TABLE_MAIN              ; argument for find_booter
    cmp     WORD [di + 64], BOOTSIG     ; correct signature found?
    jne     error                       ; Oops! no signature!
    mov     bl, TYPE_BOOT               ; look for this partition type
    mov     bh, 0                       ; initial nesting level is 0
    call    find_booter

error
    mov     si, load_error
    call    message
hang_1
    jmp     hang_1

;--------------------------------------------------------------------------
; Locate the booter partition and load the booter.
;
; Arguments:
;   di - pointer to fdisk partition table.
;   bl - partition type to look for.
;
; The following registers are modified:
;   ax, bh
;
find_booter
    push    cx
    push    si

    mov     si, di              ; si <- pointer to partition table
    mov     cx, 4               ; 4 partition entries per table

find_booter_pri
    ;
    ; Hunt for a fdisk partition type that matches the value in bl.
    ;
%IF DEBUG
    mov     al, bh              ; log partition type seen
    call    putspace
    mov     al, [si + 4]
    call    display_byte
%ENDIF

    cmp     BYTE [si + 4], bl   ; Is this the booter partition?
    je      load_booter         ; yes, load the booter

    add     si, ENTRY_SIZE      ; si <- next partition entry
    loop    find_booter_pri     ; loop while cx is not zero

    ; No primary (or perhaps logical) booter partition found in the
    ; current partition table. Restart and look for extended partitions.
    ;
    mov     si, di              ; si <- pointer to partition table
    mov     cx, 4               ; 4 partition entries per table

find_booter_ext
    ;
    ; Look for extended partition entries in the partition table.
    ;
%IF DEBUG
    mov     al, bh              ; log partition type seen
    call    putspace
    mov     al, 'E'
    call    putchar
    mov     al, [si + 4]
    call    display_byte
%ENDIF

    cmp     BYTE [si + 4], TYPE_EXT     ; Is this an extended partition?
    je      find_booter_ext_2           ; yes, load its partition table

    cmp     BYTE [si + 4], TYPE_EXT_1   ; Is this an extended partition?
    je      find_booter_ext_2           ; yes, load its partition table

    cmp     BYTE [si + 4], TYPE_EXT_2   ; Is this an extended partition?
    je      find_booter_ext_2           ; yes, load its partition table

find_booter_ext_1
    ;
    ; si is not pointing to an extended partition entry,
    ; try the next entry in the partition table.
    ;
    add     si, ENTRY_SIZE      ; si <- next partition entry
    loop    find_booter_ext     ; loop while cx is not zero

    jmp     find_booter_end     ; give up

find_booter_ext_2
    cmp     bh, EXT_LEVELS_MAX
    ja      find_booter_end     ; in too deep!

    inc     bh                  ; increment nesting level counter

    ; Prepare the arguments for the load function call to
    ; load the extended partition table into memory.
    ; Note that si points to the extended partition entry.
    ;
    mov     ax, [si]            ; DH/DL
    mov     [chs_dx], ax
    mov     ax, [si + 2]        ; CH/CL
    mov     [chs_cx], ax
    pusha
    xor     cx, cx              ; skip 0 sectors
    mov     ax, 1               ; read 1 sector
    mov     bx, BUF_EXT         ; load to BUF_EXT
    call    load
    popa

    jc      find_booter_ext_3   ; bail out if load failed

    mov     di, TABLE_EXT       ; di <- pointer to new partition table
    cmp     WORD [di + 64], BOOTSIG
    jne     find_booter_ext_3   ; OhOh! no signature!

    call    find_booter         ; recursion...

find_booter_ext_3
    dec     bh                  ; decrement nesting level counter

    ; If we got here, then we know there isn't a booter
    ; partition linked from this partition entry.

    test    bh, bh              ; if we are at level 0, then
    jz      find_booter_ext_1   ; look for next extended partition entry

find_booter_end
    pop     si
    pop     cx
    ret

;--------------------------------------------------------------------------
; Yeah! Found the booter partition. The first sector in this partition
; is reserved for the boot sector code (us). So load the booter
; starting from the second sector in the partition, then jump to the
; start of the booter.
;
load_booter
    mov     ax, [si]            ; DH/DL
    mov     [chs_dx], ax
    mov     ax, [si + 2]        ; CH/CL
    mov     [chs_cx], ax

    mov     cx, 1               ; skip the initial boot sector
    mov     ax, BOOT2_SIZE      ; read BOOT2_SIZE sectors
    mov     bx, BOOT2_SEG
    mov     es, bx
    mov     bx, BOOT2_ADDR      ; where to place boot2 code
    call    load                ; load it...

    xor     edx, edx
    mov     dl, DRIVE_NUM       ; argument for boot2

    jmp     BOOT2_SEG:BOOT2_ADDR  ; there is no going back now!

;--------------------------------------------------------------------------
; Load sectors from disk using INT13/F2 call. The sectors are loaded
; one sector at a time to avoid any BIOS bugs, and eliminate
; complexities with crossing track boundaries, and other gotchas.
;
; Arguments:
;   cx - number of sectors to skip
;   ax - number of sectors to read
;   bx - pointer to the memory buffer (must not cross a segment boundary)
;   [chs_cx][chs_dx] - CHS starting position
;
; Returns:
;   CF = 0  success
;   CF = 1  error
;
; The caller must save any registers it needs.
;
load
    jcxz    load_sectors
    call    next_sector         ; [chs_cx][chs_dx] <- next sector
    loop    load

load_sectors
    mov     cx, ax              ; cx <- number of sectors to read

load_loop
    call    read_sector         ; load a single sector
    jc      load_exit           ; abort if carry flag is set
    add     bx, SECTOR_BYTES    ; increment buffer pointer
    call    next_sector         ; [chs_cx][chs_dx] <- next sector
    loop    load_loop
    clc                         ; successful exit
load_exit
    ret

;--------------------------------------------------------------------------
; Read a single sector from the hard disk.
;
; Arguments:
;   [chs_cx][chs_dx] - CHS starting position
;   es:bx - pointer to the sector memory buffer
;           (must not cross a segment boundary)
;
; Returns:
;   CF = 0  success
;   CF = 1  error
;
; Caller's cx register is preserved.
;
read_sector
    push    cx
    mov     cx, 5               ; try 5 times to read the sector

read_sector_1
    mov     bp, cx              ; save cx

    mov     cx, [chs_cx]
    mov     dx, [chs_dx]
    mov     dl, DRIVE_NUM       ; drive number
    mov     ax, 0x0201          ; Func 2, read 1 sector
    int     0x13                ; read sector
    jnc     read_sector_ok      ; CF = 0 indicates success

    mov     al, '*'             ; sector read error indicator
    call    putchar

    xor     ax, ax              ; Reset the drive and retry the read
    int     0x13

    mov     cx, bp
    loop    read_sector_1       ; retry while cx is not zero

    stc                         ; set carry flag to indicate error
    pop     cx
    ret

read_sector_ok
    mov     al, '.'             ; successful sector read indicator
    call    putchar
    clc                         ; success, clear carry flag
    pop     cx
    ret

;--------------------------------------------------------------------------
; Given the current CHS position stored in [chs_cx][chs_dx], update
; it so that the value in [chs_cx][chs_dx] points to the following
; sector.
;
; Arguments:
;   [chs_cx][chs_dx] - CHS position
;
;   [max_sectors] and [max_heads] must be valid.
;
; Caller's ax and bx registers are preserved.
;
next_sector
    push    ax
    push    bx

    ; Extract the CHS values from the packed register values in memory.
    ;
    mov     al, [chs_cx]
    and     al, 0x3f            ; al <- sector number (1-63)

    mov     bx, [chs_cx]
    rol     bl, 2
    ror     bx, 8
    and     bx, 0x03ff          ; bx <- cylinder number

    mov     ah, [chs_dx + 1]    ; ah <- head number

    inc     al                  ; Increment CHS by one sector.
    cmp     al, [max_sectors]
    jbe     next_sector_done

    inc     ah
    cmp     ah, [max_heads]
    jbe     next_sector_new_head

    inc     bx

next_sector_new_cyl
    xor     ah, ah              ; head number starts at 0

next_sector_new_head
    mov     al, 1               ; sector number starts at 1

next_sector_done
    ; Reassemble the CHS values back into the packed representation
    ; in memory.
    ;
    mov     [chs_cx + 1], bl    ; lower 8-bits of the 10-bit cylinder
    ror     bh, 2
    or      bh, al
    mov     [chs_cx], bh        ; cylinder & sector number
    mov     [chs_dx + 1], ah    ; head number

    pop     bx
    pop     ax
    ret

;--------------------------------------------------------------------------
; Write a string to the console.
;
; Arguments:
;   ds:si   pointer to a NULL terminated string.
;
; The following registers are modified:
;   ax, bx, si
;
message
    mov     bx, 1               ; bh=0, bl=1 (blue)
    cld                         ; increment SI after each lodsb call
message_loop
    lodsb                       ; load a byte from DS:SI into al
    cmp     al, 0               ; Is it a NULL?
    je      message_done        ; yes, all done
    mov     ah, 0xE             ; bios INT10 Func 0xE
    int     0x10                ; bios display a byte in tty mode
    jmp     short message_loop
message_done
    ret

;--------------------------------------------------------------------------
; Write a ASCII character to the console.
;
; Arguments:
;   al   contains the ASCII character printed.
;
putchar
    push    bx
    mov     bx, 1               ; bh=0, bl=1 (blue)
    mov     ah, 0x0e            ; bios int 10, function 0xe
    int     0x10                ; bios display a byte in tty mode
    pop     bx
    ret

%IF DEBUG
;==========================================================================
; DEBUG FUNCTION START
;
; If DEBUG is set to 1, this booter will become too large for the MBR,
; but it will still be less than 510 bytes, which is fine for a partition's
; boot sector.
;==========================================================================

;--------------------------------------------------------------------------
; Write a variable number of spaces to the console.
;
; Arguments:
;   al   number to spaces to display
;
putspace
    push    cx
    xor     cx, cx
    mov     cl, al              ; use cx as the loop counter
    mov     al, ' '             ; character to print
putspace_loop
    jcxz    putspace_done
    call    putchar
    loop    putspace_loop
putspace_done
    pop     cx
    ret

;--------------------------------------------------------------------------
; Write the hex byte value to the console.
;
; Arguments:
;   al   contains the byte to be displayed. e.g. if al is 0x3f, then 3F
;        will be displayed on screen.
;
display_byte
    push    ax
    ror     al, 4
    call    display_nibble      ; display upper nibble
    pop     ax
    call    display_nibble      ; display lower nibble
    ;
    mov     ax, 10              ; display carriage return
    call    putchar
    mov     ax, 13
    call    putchar
    ret

display_nibble
    and     al, 0x0f
    add     al, '0'
    cmp     al, '9'
    jna     display_nibble_1
    add     al, 'A' - '9' - 1
display_nibble_1
    call    putchar
    ret

;==========================================================================
; DEBUG FUNCTION END
;==========================================================================
%ENDIF

; Disk parameters gathered through INT13/F8 call.
;
max_sectors     db   0                  ; number of sectors per track
max_heads       db   0                  ; number of heads

; Parameters to our load function.
;
chs_cx          dw   0x0001                  ; cx register for INT13/F2 call
chs_dx          dw   0x0000                  ; dx register for INT13/F2 call

;--------------------------------------------------------------------------
; NULL terminated strings.
;
load_error   db  10, 13, 'Load Error', 0

;--------------------------------------------------------------------------
; Pad the rest of the 512 byte sized booter with zeroes. The last
; two bytes is the mandatory boot sector signature.
;
; If the booter code becomes too large, then nasm will complain
; that the 'times' argument is negative.

pad_boot
    times 446-($-$$) db 0

%IF BOOTDEV = FLOPPY
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
%ENDIF

pad_table_and_sig
    times 510-($-$$) db 0
    dw BOOTSIG

    END
