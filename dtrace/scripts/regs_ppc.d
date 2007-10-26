/*
 * Copyright (c) 2005-2006 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * PowerPC register set. See osfmk/ppc/savearea.h
 */

inline int GPR_R0 = 0;
#pragma D binding "1.0" GPR_R0
inline int GPR_R1 = 1;
#pragma D binding "1.0" GPR_R1
inline int GPR_R2 = 2;
#pragma D binding "1.0" GPR_R2
inline int GPR_R3 = 3;
#pragma D binding "1.0" GPR_R3
inline int GPR_R4 = 4;
#pragma D binding "1.0" GPR_R4
inline int GPR_R5 = 5;
#pragma D binding "1.0" GPR_R5
inline int GPR_R6 = 6;
#pragma D binding "1.0" GPR_R6
inline int GPR_R7 = 7;
#pragma D binding "1.0" GPR_R7
inline int GPR_R8 = 8;
#pragma D binding "1.0" GPR_R8
inline int GPR_R9 = 9;
#pragma D binding "1.0" GPR_R9
inline int GPR_R10 = 10;
#pragma D binding "1.0" GPR_R10
inline int GPR_R11 = 11;
#pragma D binding "1.0" GPR_R11
inline int GPR_R12 = 12;
#pragma D binding "1.0" GPR_R12
inline int GPR_R13 = 13;
#pragma D binding "1.0" GPR_R13
inline int GPR_R14 = 14;
#pragma D binding "1.0" GPR_R14
inline int GPR_R15 = 15;
#pragma D binding "1.0" GPR_R15
inline int GPR_R16 = 16;
#pragma D binding "1.0" GPR_R16
inline int GPR_R17 = 17;
#pragma D binding "1.0" GPR_R17
inline int GPR_R18 = 18;
#pragma D binding "1.0" GPR_R18
inline int GPR_R19 = 19;
#pragma D binding "1.0" GPR_R19
inline int GPR_R20 = 20;
#pragma D binding "1.0" GPR_R20
inline int GPR_R21 = 21;
#pragma D binding "1.0" GPR_R21
inline int GPR_R22 = 22;
#pragma D binding "1.0" GPR_R22
inline int GPR_R23 = 23;
#pragma D binding "1.0" GPR_R23
inline int GPR_R24 = 24;
#pragma D binding "1.0" GPR_R24
inline int GPR_R25 = 25;
#pragma D binding "1.0" GPR_R25
inline int GPR_R26 = 26;
#pragma D binding "1.0" GPR_R26
inline int GPR_R27 = 27;
#pragma D binding "1.0" GPR_R27
inline int GPR_R28 = 28;
#pragma D binding "1.0" GPR_R28
inline int GPR_R29 = 29;
#pragma D binding "1.0" GPR_R29
inline int GPR_R30 = 30;
#pragma D binding "1.0" GPR_R30
inline int GPR_R31 = 31;
#pragma D binding "1.0" GPR_R31
inline int R_SRR0 = 32;
#pragma D binding "1.0" R_SRR0
inline int R_SRR1 = 33;
#pragma D binding "1.0" R_SRR1
inline int R_XER = 34;
#pragma D binding "1.0" R_XER
inline int R_LR = 35;
#pragma D binding "1.0" R_LR
inline int R_CTR = 36;
#pragma D binding "1.0" R_CTR
inline int R_DAR = 37;
#pragma D binding "1.0" R_DAR
inline int R_CR = 38;
#pragma D binding "1.0" R_CR
inline int R_DSISR = 39;
#pragma D binding "1.0" R_DSISR
inline int R_EXCEPTION = 40;
#pragma D binding "1.0" R_EXCEPTION
inline int R_VRSAVE = 41;
#pragma D binding "1.0" R_VRSAVE
inline int R_VSCR_0 = 42;
#pragma D binding "1.0" R_VSCR_0
inline int R_VSCR_1 = 43;
#pragma D binding "1.0" R_VSCR_1
inline int R_VSCR_2 = 44;
#pragma D binding "1.0" R_VSCR_2
inline int R_VSCR_3 = 45;
#pragma D binding "1.0" R_VSCR_3
inline int R_FPSCRPAD = 46;
#pragma D binding "1.0" R_FPSCRPAD
inline int R_FPSCR = 47;
#pragma D binding "1.0" R_FPSCR
inline int RSAVOFF_1D8_0 = 48;
#pragma D binding "1.0" RSAVOFF_1D8_0
inline int RSAVOFF_1D8_1 = 49;
#pragma D binding "1.0" RSAVOFF_1D8_1
inline int RSAVOFF_1E0_0 = 50;
#pragma D binding "1.0" RSAVOFF_1E0_0
inline int RSAVOFF_1E0_1 = 51;
#pragma D binding "1.0" RSAVOFF_1E0_1
inline int RSAVOFF_1E0_2 = 52;
#pragma D binding "1.0" RSAVOFF_1E0_2
inline int RSAVOFF_1E0_3 = 53;
#pragma D binding "1.0" RSAVOFF_1E0_3
inline int RSAVOFF_1E0_4 = 54;
#pragma D binding "1.0" RSAVOFF_1E0_4
inline int RSAVOFF_1E0_5 = 55;
#pragma D binding "1.0" RSAVOFF_1E0_5
inline int RSAVOFF_1E0_6 = 56;
#pragma D binding "1.0" RSAVOFF_1E0_6
inline int RSAVOFF_1E0_7 = 57;
#pragma D binding "1.0" RSAVOFF_1E0_7
inline int R_PMC_0 = 58;
#pragma D binding "1.0" R_PMC_0
inline int R_PMC_1 = 59;
#pragma D binding "1.0" R_PMC_1
inline int R_PMC_2 = 60;
#pragma D binding "1.0" R_PMC_2
inline int R_PMC_3 = 61;
#pragma D binding "1.0" R_PMC_3
inline int R_PMC_4 = 62;
#pragma D binding "1.0" R_PMC_4
inline int R_PMC_5 = 63;
#pragma D binding "1.0" R_PMC_5
inline int R_PMC_6 = 64;
#pragma D binding "1.0" R_PMC_6
inline int R_PMC_7 = 65;
#pragma D binding "1.0" R_PMC_7
inline int R_MMCR0 = 66;
#pragma D binding "1.0" R_MMCR0
inline int R_MMCR1 = 67;
#pragma D binding "1.0" R_MMCR1
inline int R_MMCR2 = 68;
#pragma D binding "1.0" R_MMCR2
