/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *	Copyright (c) 1998, Apple Computer Inc. All rights reserved.
 *
 *	File: _setjmp.h
 *
 *	Defines for register offsets in the save area.
 *
 */

/* NOTE: jmp_bufs are only 4-byte aligned.  This means we
 * need to pad before the VR and FPR save areas, so that they
 * can be naturally aligned in the buffer.  In case a jmp_buf
 * is bcopy'd to a different alignment between the setjmp
 * and longjmp, we need to save the jmp_buf address in the
 * jmp_buf at setjmp time, so we can realign before reloading.
 *
 * ALSO NOTE: the typedef for jmpbufs in <ppc/setjmp.h> is
 * 192 ints (0x300 bytes) long, due to incorrectly assuming
 * that we need to save all 32 VRs in a jmpbuf.  This is
 * fortuitous, because when it came time to add additional
 * fields and expand GPRs for 64-bit mode, there was plenty
 * of unused space!
 */
 
 /* 32-bit-mode layout */
 
#if defined(__ppc__)
 
#define JMP_r1	0x00
#define JMP_r2	0x04
#define JMP_r13	0x08
#define JMP_r14	0x0c
#define JMP_r15	0x10
#define JMP_r16	0x14
#define JMP_r17	0x18
#define JMP_r18	0x1c
#define JMP_r19	0x20
#define JMP_r20	0x24
#define JMP_r21	0x28
#define JMP_r22	0x2c
#define JMP_r23	0x30
#define JMP_r24	0x34
#define JMP_r25	0x38
#define JMP_r26	0x3c
#define JMP_r27	0x40
#define JMP_r28	0x44
#define JMP_r29	0x48
#define JMP_r30	0x4c
#define JMP_r31	0x50
#define JMP_lr  0x54
#define JMP_cr  0x58
#define JMP_SIGFLAG 0x5c
#define JMP_sig	0x60    /* reserve 8 bytes for sigmask */
#define JMP_fpscr  0x68 /* reserve 8 bytes for FPSCR too */
#define JMP_vrsave 0x70
#define JMP_addr_at_setjmp 0x74
/* 12 bytes padding here */
#define JMP_vr_base_addr 0x84
/* save room for 12 VRs (v20-v31), or 0xC0 bytes */
#define JMP_fp_base_addr 0x144
/* save room for 18 FPRs (f14-f31), or 0x90 bytes */
#define	JMP_ss_flags	 0x1d4
#define JMP_buf_end 0x1d8

 
 /* 64-bit-mode layout */

#elif defined(__ppc64__)

#define JMP_r1	0x00
#define JMP_r2	0x08
#define JMP_r13	0x10
#define JMP_r14	0x18
#define JMP_r15	0x20
#define JMP_r16	0x28
#define JMP_r17	0x30
#define JMP_r18	0x38
#define JMP_r19	0x40
#define JMP_r20	0x48
#define JMP_r21	0x50
#define JMP_r22	0x58
#define JMP_r23	0x60
#define JMP_r24	0x68
#define JMP_r25	0x70
#define JMP_r26	0x78
#define JMP_r27	0x80
#define JMP_r28	0x88
#define JMP_r29	0x90
#define JMP_r30	0x98
#define JMP_r31	0xa0
#define JMP_lr  0xa8
#define JMP_cr  0xb0
#define JMP_sig	0xb8
#define JMP_SIGFLAG 0xc0
#define JMP_fpscr  0xc8
#define JMP_vrsave 0xd0
#define JMP_addr_at_setjmp 0xd8
/* 12 bytes padding here */
#define JMP_vr_base_addr 0x0ec
/* save room for 12 VRs (v20-v31), or 0xC0 bytes */
#define JMP_fp_base_addr 0x1ac
/* save room for 18 FPRs (f14-f31), or 0x90 bytes */
#define	JMP_ss_flags	 0x23c
#define JMP_buf_end 0x240

#else
#error architecture not supported
#endif