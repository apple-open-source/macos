; bootnot.asm - boot1 written for turbo assembler, since gas only
; generates 32 bit code and this must run in real mode.
; To compile as floppy boot1f.not:
;	tasm /m3 /dBOOTDEV=FLOPPY boot1 ,boot1f
;	tlink boot1f
;	exe2bin boot1f
;	ren boot1f.bin boot1f.not

;***********************************************************************
;	This is the code for the NeXT boot1 bootsector.
;***********************************************************************

	P486			;enable i386 instructions
	IDEAL
	SEGMENT CSEG
	ASSUME CS:CSEG,DS:CSEG

	SDEBUG = 0

;BOOTSEG		=	100h	; boot will be loaded at 4k
;BOOTOFF		=	0000h
BOOTSEG		=	00h
BOOTOFF		=	1000h
BUFSZ		=	2000h	; 8K disk transfer buffer

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
	off1	=  0E000h + (a1 - start)
	jmp	FAR 0000:off1	; jump to a1 in relocated place

a1:
	mov	ax,0E00h
	mov	ds,ax
	mov	ax,BOOTSEG
	mov	es,ax

	mov	si, OFFSET not_boot
	call	message		; display intro message

halt:
	mov	ah, 00h
	int	16h
	jmp	halt		; get key and loop forever

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
	jmp	nextb
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
	db     'The disk in the floppy disk drive isn''t a startup disk:'
	db	10,13
	db	'It doesn''t contain '
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
	a2 = 510 - (d1 - start)
	DB a2 dup (0)
	DW 0AA55h
	ENDS
	END
