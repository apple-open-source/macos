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
#ifndef _ASSYM_H_
#define _ASSYM_H_
#define PCB_FLOAT_STATE 208
#define PCB_FS_F0 208
#define PCB_FS_F1 216
#define PCB_FS_F2 224
#define PCB_FS_F3 232
#define PCB_FS_F4 240
#define PCB_FS_F5 248
#define PCB_FS_F6 256
#define PCB_FS_F7 264
#define PCB_FS_F8 272
#define PCB_FS_F9 280
#define PCB_FS_F10 288
#define PCB_FS_F11 296
#define PCB_FS_F12 304
#define PCB_FS_F13 312
#define PCB_FS_F14 320
#define PCB_FS_F15 328
#define PCB_FS_F16 336
#define PCB_FS_F17 344
#define PCB_FS_F18 352
#define PCB_FS_F19 360
#define PCB_FS_F20 368
#define PCB_FS_F21 376
#define PCB_FS_F22 384
#define PCB_FS_F23 392
#define PCB_FS_F24 400
#define PCB_FS_F25 408
#define PCB_FS_F26 416
#define PCB_FS_F27 424
#define PCB_FS_F28 432
#define PCB_FS_F29 440
#define PCB_FS_F30 448
#define PCB_FS_F31 456
#define PCB_FS_FPSCR 464
#define PCB_SAVED_STATE 0
#define PCB_KSP 472
#define PCB_SR0 476
#define PCB_SIZE 480
#define SS_R0 8
#define SS_R1 12
#define SS_R2 16
#define SS_R3 20
#define SS_R4 24
#define SS_R5 28
#define SS_R6 32
#define SS_R7 36
#define SS_R8 40
#define SS_R9 44
#define SS_R10 48
#define SS_R11 52
#define SS_R12 56
#define SS_R13 60
#define SS_R14 64
#define SS_R15 68
#define SS_R16 72
#define SS_R17 76
#define SS_R18 80
#define SS_R19 84
#define SS_R20 88
#define SS_R21 92
#define SS_R22 96
#define SS_R23 100
#define SS_R24 104
#define SS_R25 108
#define SS_R26 112
#define SS_R27 116
#define SS_R28 120
#define SS_R29 124
#define SS_R30 128
#define SS_R31 132
#define SS_CR 136
#define SS_XER 140
#define SS_LR 144
#define SS_CTR 148
#define SS_SRR0 0
#define SS_SRR1 4
#define SS_MQ 152
#define SS_SR_COPYIN 160
#define SS_SIZE 176
#define PP_SAVE_CR 0
#define PP_SAVE_SRR0 4
#define PP_SAVE_SRR1 8
#define PP_SAVE_DAR 12
#define PP_SAVE_DSISR 16
#define PP_SAVE_SPRG0 20
#define PP_SAVE_SPRG1 24
#define PP_SAVE_SPRG2 28
#define PP_SAVE_SPRG3 32
#define PP_SAVE_EXCEPTION_TYPE 36
#define PP_CPU_DATA 52
#define PP_PHYS_EXCEPTION_HANDLERS 40
#define PP_VIRT_PER_PROC 44
#define PP_ACTIVE_STACKS 56
#define PP_NEED_AST 60
#define PP_FPU_PCB 64
#define KS_PCB 16276
#define KS_R1 16280
#define KS_R2 16284
#define KS_R13 16288
#define KS_LR 16364
#define KS_CR 16368
#define KS_SIZE 96
#define KSTK_SIZE 16372
#define THREAD_PCB 36
#define THREAD_KERNEL_STACK 40
#define THREAD_SWAPFUNC 48
#define THREAD_RECOVER 116
#define THREAD_TASK 12
#define TASK_VMMAP 8
#define TASK_MACH_EXC_PORT 96
#define VMMAP_PMAP 32
#define PMAP_SPACE 4
#define MACH_TRAP_OFFSET_POW2 4
#define MACH_TRAP_ARGC 0
#define MACH_TRAP_FUNCTION 4
#define HOST_SELF 0
#define CPU_ACTIVE_THREAD 0
#define FM_SIZE 56
#define ARG_SIZE 16
#define LA_SIZE 24
#define FM_BACKPTR 0
#define FM_LR_SAVE 8
#define FM_TOC_SAVE 20
#define RPA_SIZE 32
#define SPA_SIZE 16
#define FM_ARG0 56
#define FM_REDZONE 0

#define	SIZEOF_SIGCATCH               4
#define	SIGCATCH_HANDLER              0x00000000

#endif /* _ASSYM_H_ */
