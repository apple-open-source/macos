/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
/* This file auto-generated from insns.dat by insns.pl - don't edit it */

#include <stdio.h>
#include "nasm.h"
#include "insns.h"

static struct itemplate instrux_AAA[] = {
    {I_AAA, 0, {0,0,0}, "\1\x37", IF_8086},
    {-1}
};

static struct itemplate instrux_AAD[] = {
    {I_AAD, 0, {0,0,0}, "\2\xD5\x0A", IF_8086},
    {I_AAD, 1, {IMMEDIATE,0,0}, "\1\xD5\24", IF_8086},
    {-1}
};

static struct itemplate instrux_AAM[] = {
    {I_AAM, 0, {0,0,0}, "\2\xD4\x0A", IF_8086},
    {I_AAM, 1, {IMMEDIATE,0,0}, "\1\xD4\24", IF_8086},
    {-1}
};

static struct itemplate instrux_AAS[] = {
    {I_AAS, 0, {0,0,0}, "\1\x3F", IF_8086},
    {-1}
};

static struct itemplate instrux_ADC[] = {
    {I_ADC, 2, {MEMORY,REG8,0}, "\300\1\x10\101", IF_8086|IF_SM},
    {I_ADC, 2, {REG8,REG8,0}, "\300\1\x10\101", IF_8086},
    {I_ADC, 2, {MEMORY,REG16,0}, "\320\300\1\x11\101", IF_8086|IF_SM},
    {I_ADC, 2, {REG16,REG16,0}, "\320\300\1\x11\101", IF_8086},
    {I_ADC, 2, {MEMORY,REG32,0}, "\321\300\1\x11\101", IF_386|IF_SM},
    {I_ADC, 2, {REG32,REG32,0}, "\321\300\1\x11\101", IF_386},
    {I_ADC, 2, {REG8,MEMORY,0}, "\301\1\x12\110", IF_8086|IF_SM},
    {I_ADC, 2, {REG8,REG8,0}, "\301\1\x12\110", IF_8086},
    {I_ADC, 2, {REG16,MEMORY,0}, "\320\301\1\x13\110", IF_8086|IF_SM},
    {I_ADC, 2, {REG16,REG16,0}, "\320\301\1\x13\110", IF_8086},
    {I_ADC, 2, {REG32,MEMORY,0}, "\321\301\1\x13\110", IF_386|IF_SM},
    {I_ADC, 2, {REG32,REG32,0}, "\321\301\1\x13\110", IF_386},
    {I_ADC, 2, {REGMEM|BITS16,IMMEDIATE|BITS8,0}, "\320\300\1\x83\202\15", IF_8086},
    {I_ADC, 2, {REGMEM|BITS32,IMMEDIATE|BITS8,0}, "\321\300\1\x83\202\15", IF_386},
    {I_ADC, 2, {REG_AL,IMMEDIATE,0}, "\1\x14\21", IF_8086|IF_SM},
    {I_ADC, 2, {REG_AX,IMMEDIATE,0}, "\320\1\x15\31", IF_8086|IF_SM},
    {I_ADC, 2, {REG_EAX,IMMEDIATE,0}, "\321\1\x15\41", IF_386|IF_SM},
    {I_ADC, 2, {REGMEM|BITS8,IMMEDIATE,0}, "\300\1\x80\202\21", IF_8086|IF_SM},
    {I_ADC, 2, {REGMEM|BITS16,IMMEDIATE,0}, "\320\300\1\x81\202\31", IF_8086|IF_SM},
    {I_ADC, 2, {REGMEM|BITS32,IMMEDIATE,0}, "\321\300\1\x81\202\41", IF_386|IF_SM},
    {I_ADC, 2, {MEMORY,IMMEDIATE|BITS8,0}, "\300\1\x80\202\21", IF_8086|IF_SM},
    {I_ADC, 2, {MEMORY,IMMEDIATE|BITS16,0}, "\320\300\1\x81\202\31", IF_8086|IF_SM},
    {I_ADC, 2, {MEMORY,IMMEDIATE|BITS32,0}, "\321\300\1\x81\202\41", IF_386|IF_SM},
    {-1}
};

static struct itemplate instrux_ADD[] = {
    {I_ADD, 2, {MEMORY,REG8,0}, "\300\17\101", IF_8086|IF_SM},
    {I_ADD, 2, {REG8,REG8,0}, "\300\17\101", IF_8086},
    {I_ADD, 2, {MEMORY,REG16,0}, "\320\300\1\x01\101", IF_8086|IF_SM},
    {I_ADD, 2, {REG16,REG16,0}, "\320\300\1\x01\101", IF_8086},
    {I_ADD, 2, {MEMORY,REG32,0}, "\321\300\1\x01\101", IF_386|IF_SM},
    {I_ADD, 2, {REG32,REG32,0}, "\321\300\1\x01\101", IF_386},
    {I_ADD, 2, {REG8,MEMORY,0}, "\301\1\x02\110", IF_8086|IF_SM},
    {I_ADD, 2, {REG8,REG8,0}, "\301\1\x02\110", IF_8086},
    {I_ADD, 2, {REG16,MEMORY,0}, "\320\301\1\x03\110", IF_8086|IF_SM},
    {I_ADD, 2, {REG16,REG16,0}, "\320\301\1\x03\110", IF_8086},
    {I_ADD, 2, {REG32,MEMORY,0}, "\321\301\1\x03\110", IF_386|IF_SM},
    {I_ADD, 2, {REG32,REG32,0}, "\321\301\1\x03\110", IF_386},
    {I_ADD, 2, {REGMEM|BITS16,IMMEDIATE|BITS8,0}, "\320\300\1\x83\200\15", IF_8086},
    {I_ADD, 2, {REGMEM|BITS32,IMMEDIATE|BITS8,0}, "\321\300\1\x83\200\15", IF_386},
    {I_ADD, 2, {REG_AL,IMMEDIATE,0}, "\1\x04\21", IF_8086|IF_SM},
    {I_ADD, 2, {REG_AX,IMMEDIATE,0}, "\320\1\x05\31", IF_8086|IF_SM},
    {I_ADD, 2, {REG_EAX,IMMEDIATE,0}, "\321\1\x05\41", IF_386|IF_SM},
    {I_ADD, 2, {REGMEM|BITS8,IMMEDIATE,0}, "\300\1\x80\200\21", IF_8086|IF_SM},
    {I_ADD, 2, {REGMEM|BITS16,IMMEDIATE,0}, "\320\300\1\x81\200\31", IF_8086|IF_SM},
    {I_ADD, 2, {REGMEM|BITS32,IMMEDIATE,0}, "\321\300\1\x81\200\41", IF_386|IF_SM},
    {I_ADD, 2, {MEMORY,IMMEDIATE|BITS8,0}, "\300\1\x80\200\21", IF_8086|IF_SM},
    {I_ADD, 2, {MEMORY,IMMEDIATE|BITS16,0}, "\320\300\1\x81\200\31", IF_8086|IF_SM},
    {I_ADD, 2, {MEMORY,IMMEDIATE|BITS32,0}, "\321\300\1\x81\200\41", IF_386|IF_SM},
    {-1}
};

static struct itemplate instrux_AND[] = {
    {I_AND, 2, {MEMORY,REG8,0}, "\300\1\x20\101", IF_8086|IF_SM},
    {I_AND, 2, {REG8,REG8,0}, "\300\1\x20\101", IF_8086},
    {I_AND, 2, {MEMORY,REG16,0}, "\320\300\1\x21\101", IF_8086|IF_SM},
    {I_AND, 2, {REG16,REG16,0}, "\320\300\1\x21\101", IF_8086},
    {I_AND, 2, {MEMORY,REG32,0}, "\321\300\1\x21\101", IF_386|IF_SM},
    {I_AND, 2, {REG32,REG32,0}, "\321\300\1\x21\101", IF_386},
    {I_AND, 2, {REG8,MEMORY,0}, "\301\1\x22\110", IF_8086|IF_SM},
    {I_AND, 2, {REG8,REG8,0}, "\301\1\x22\110", IF_8086},
    {I_AND, 2, {REG16,MEMORY,0}, "\320\301\1\x23\110", IF_8086|IF_SM},
    {I_AND, 2, {REG16,REG16,0}, "\320\301\1\x23\110", IF_8086},
    {I_AND, 2, {REG32,MEMORY,0}, "\321\301\1\x23\110", IF_386|IF_SM},
    {I_AND, 2, {REG32,REG32,0}, "\321\301\1\x23\110", IF_386},
    {I_AND, 2, {REGMEM|BITS16,IMMEDIATE|BITS8,0}, "\320\300\1\x83\204\15", IF_8086},
    {I_AND, 2, {REGMEM|BITS32,IMMEDIATE|BITS8,0}, "\321\300\1\x83\204\15", IF_386},
    {I_AND, 2, {REG_AL,IMMEDIATE,0}, "\1\x24\21", IF_8086|IF_SM},
    {I_AND, 2, {REG_AX,IMMEDIATE,0}, "\320\1\x25\31", IF_8086|IF_SM},
    {I_AND, 2, {REG_EAX,IMMEDIATE,0}, "\321\1\x25\41", IF_386|IF_SM},
    {I_AND, 2, {REGMEM|BITS8,IMMEDIATE,0}, "\300\1\x80\204\21", IF_8086|IF_SM},
    {I_AND, 2, {REGMEM|BITS16,IMMEDIATE,0}, "\320\300\1\x81\204\31", IF_8086|IF_SM},
    {I_AND, 2, {REGMEM|BITS32,IMMEDIATE,0}, "\321\300\1\x81\204\41", IF_386|IF_SM},
    {I_AND, 2, {MEMORY,IMMEDIATE|BITS8,0}, "\300\1\x80\204\21", IF_8086|IF_SM},
    {I_AND, 2, {MEMORY,IMMEDIATE|BITS16,0}, "\320\300\1\x81\204\31", IF_8086|IF_SM},
    {I_AND, 2, {MEMORY,IMMEDIATE|BITS32,0}, "\321\300\1\x81\204\41", IF_386|IF_SM},
    {-1}
};

static struct itemplate instrux_ARPL[] = {
    {I_ARPL, 2, {MEMORY,REG16,0}, "\300\1\x63\101", IF_286|IF_PRIV|IF_SM},
    {I_ARPL, 2, {REG16,REG16,0}, "\300\1\x63\101", IF_286|IF_PRIV},
    {-1}
};

static struct itemplate instrux_BOUND[] = {
    {I_BOUND, 2, {REG16,MEMORY,0}, "\320\301\1\x62\110", IF_186},
    {I_BOUND, 2, {REG32,MEMORY,0}, "\321\301\1\x62\110", IF_386},
    {-1}
};

static struct itemplate instrux_BSF[] = {
    {I_BSF, 2, {REG16,MEMORY,0}, "\320\301\2\x0F\xBC\110", IF_386|IF_SM},
    {I_BSF, 2, {REG16,REG16,0}, "\320\301\2\x0F\xBC\110", IF_386},
    {I_BSF, 2, {REG32,MEMORY,0}, "\321\301\2\x0F\xBC\110", IF_386|IF_SM},
    {I_BSF, 2, {REG32,REG32,0}, "\321\301\2\x0F\xBC\110", IF_386},
    {-1}
};

static struct itemplate instrux_BSR[] = {
    {I_BSR, 2, {REG16,MEMORY,0}, "\320\301\2\x0F\xBD\110", IF_386|IF_SM},
    {I_BSR, 2, {REG16,REG16,0}, "\320\301\2\x0F\xBD\110", IF_386},
    {I_BSR, 2, {REG32,MEMORY,0}, "\321\301\2\x0F\xBD\110", IF_386|IF_SM},
    {I_BSR, 2, {REG32,REG32,0}, "\321\301\2\x0F\xBD\110", IF_386},
    {-1}
};

static struct itemplate instrux_BSWAP[] = {
    {I_BSWAP, 1, {REG32,0,0}, "\321\1\x0F\10\xC8", IF_486},
    {-1}
};

static struct itemplate instrux_BT[] = {
    {I_BT, 2, {MEMORY,REG16,0}, "\320\300\2\x0F\xA3\101", IF_386|IF_SM},
    {I_BT, 2, {REG16,REG16,0}, "\320\300\2\x0F\xA3\101", IF_386},
    {I_BT, 2, {MEMORY,REG32,0}, "\321\300\2\x0F\xA3\101", IF_386|IF_SM},
    {I_BT, 2, {REG32,REG32,0}, "\321\300\2\x0F\xA3\101", IF_386},
    {I_BT, 2, {REGMEM|BITS16,IMMEDIATE,0}, "\320\300\2\x0F\xBA\204\25", IF_386},
    {I_BT, 2, {REGMEM|BITS32,IMMEDIATE,0}, "\321\300\2\x0F\xBA\204\25", IF_386},
    {-1}
};

static struct itemplate instrux_BTC[] = {
    {I_BTC, 2, {MEMORY,REG16,0}, "\320\300\2\x0F\xBB\101", IF_386|IF_SM},
    {I_BTC, 2, {REG16,REG16,0}, "\320\300\2\x0F\xBB\101", IF_386},
    {I_BTC, 2, {MEMORY,REG32,0}, "\321\300\2\x0F\xBB\101", IF_386|IF_SM},
    {I_BTC, 2, {REG32,REG32,0}, "\321\300\2\x0F\xBB\101", IF_386},
    {I_BTC, 2, {REGMEM|BITS16,IMMEDIATE,0}, "\320\300\2\x0F\xBA\207\25", IF_386},
    {I_BTC, 2, {REGMEM|BITS32,IMMEDIATE,0}, "\321\300\2\x0F\xBA\207\25", IF_386},
    {-1}
};

static struct itemplate instrux_BTR[] = {
    {I_BTR, 2, {MEMORY,REG16,0}, "\320\300\2\x0F\xB3\101", IF_386|IF_SM},
    {I_BTR, 2, {REG16,REG16,0}, "\320\300\2\x0F\xB3\101", IF_386},
    {I_BTR, 2, {MEMORY,REG32,0}, "\321\300\2\x0F\xB3\101", IF_386|IF_SM},
    {I_BTR, 2, {REG32,REG32,0}, "\321\300\2\x0F\xB3\101", IF_386},
    {I_BTR, 2, {REGMEM|BITS16,IMMEDIATE,0}, "\320\300\2\x0F\xBA\206\25", IF_386},
    {I_BTR, 2, {REGMEM|BITS32,IMMEDIATE,0}, "\321\300\2\x0F\xBA\206\25", IF_386},
    {-1}
};

static struct itemplate instrux_BTS[] = {
    {I_BTS, 2, {MEMORY,REG16,0}, "\320\300\2\x0F\xAB\101", IF_386|IF_SM},
    {I_BTS, 2, {REG16,REG16,0}, "\320\300\2\x0F\xAB\101", IF_386},
    {I_BTS, 2, {MEMORY,REG32,0}, "\321\300\2\x0F\xAB\101", IF_386|IF_SM},
    {I_BTS, 2, {REG32,REG32,0}, "\321\300\2\x0F\xAB\101", IF_386},
    {I_BTS, 2, {REGMEM|BITS16,IMMEDIATE,0}, "\320\300\2\x0F\xBA\205\25", IF_386},
    {I_BTS, 2, {REGMEM|BITS32,IMMEDIATE,0}, "\321\300\2\x0F\xBA\205\25", IF_386},
    {-1}
};

static struct itemplate instrux_CALL[] = {
    {I_CALL, 1, {IMMEDIATE,0,0}, "\322\1\xE8\64", IF_8086},
    {I_CALL, 1, {IMMEDIATE|FAR,0,0}, "\322\1\x9A\34\37", IF_8086},
    {I_CALL, 2, {IMMEDIATE|COLON,IMMEDIATE,0}, "\322\1\x9A\35\30", IF_8086},
    {I_CALL, 2, {IMMEDIATE|BITS16|COLON,IMMEDIATE,0}, "\320\1\x9A\31\30", IF_8086},
    {I_CALL, 2, {IMMEDIATE|COLON,IMMEDIATE|BITS16,0}, "\320\1\x9A\31\30", IF_8086},
    {I_CALL, 2, {IMMEDIATE|BITS32|COLON,IMMEDIATE,0}, "\321\1\x9A\41\30", IF_386},
    {I_CALL, 2, {IMMEDIATE|COLON,IMMEDIATE|BITS32,0}, "\321\1\x9A\41\30", IF_386},
    {I_CALL, 1, {MEMORY|FAR,0,0}, "\322\300\1\xFF\203", IF_8086},
    {I_CALL, 1, {MEMORY|BITS16|FAR,0,0}, "\320\300\1\xFF\203", IF_8086},
    {I_CALL, 1, {MEMORY|BITS32|FAR,0,0}, "\321\300\1\xFF\203", IF_386},
    {I_CALL, 1, {MEMORY|NEAR,0,0}, "\322\300\1\xFF\202", IF_8086},
    {I_CALL, 1, {MEMORY|BITS16|NEAR,0,0}, "\320\300\1\xFF\202", IF_8086},
    {I_CALL, 1, {MEMORY|BITS32|NEAR,0,0}, "\321\300\1\xFF\202", IF_386},
    {I_CALL, 1, {REG16,0,0}, "\320\300\1\xFF\202", IF_8086},
    {I_CALL, 1, {REG32,0,0}, "\321\300\1\xFF\202", IF_386},
    {I_CALL, 1, {MEMORY,0,0}, "\322\300\1\xFF\202", IF_8086},
    {I_CALL, 1, {MEMORY|BITS16,0,0}, "\320\300\1\xFF\202", IF_8086},
    {I_CALL, 1, {MEMORY|BITS32,0,0}, "\321\300\1\xFF\202", IF_386},
    {-1}
};

static struct itemplate instrux_CBW[] = {
    {I_CBW, 0, {0,0,0}, "\320\1\x98", IF_8086},
    {-1}
};

static struct itemplate instrux_CDQ[] = {
    {I_CDQ, 0, {0,0,0}, "\321\1\x99", IF_386},
    {-1}
};

static struct itemplate instrux_CLC[] = {
    {I_CLC, 0, {0,0,0}, "\1\xF8", IF_8086},
    {-1}
};

static struct itemplate instrux_CLD[] = {
    {I_CLD, 0, {0,0,0}, "\1\xFC", IF_8086},
    {-1}
};

static struct itemplate instrux_CLI[] = {
    {I_CLI, 0, {0,0,0}, "\1\xFA", IF_8086},
    {-1}
};

static struct itemplate instrux_CLTS[] = {
    {I_CLTS, 0, {0,0,0}, "\2\x0F\x06", IF_286|IF_PRIV},
    {-1}
};

static struct itemplate instrux_CMC[] = {
    {I_CMC, 0, {0,0,0}, "\1\xF5", IF_8086},
    {-1}
};

static struct itemplate instrux_CMP[] = {
    {I_CMP, 2, {MEMORY,REG8,0}, "\300\1\x38\101", IF_8086|IF_SM},
    {I_CMP, 2, {REG8,REG8,0}, "\300\1\x38\101", IF_8086},
    {I_CMP, 2, {MEMORY,REG16,0}, "\320\300\1\x39\101", IF_8086|IF_SM},
    {I_CMP, 2, {REG16,REG16,0}, "\320\300\1\x39\101", IF_8086},
    {I_CMP, 2, {MEMORY,REG32,0}, "\321\300\1\x39\101", IF_386|IF_SM},
    {I_CMP, 2, {REG32,REG32,0}, "\321\300\1\x39\101", IF_386},
    {I_CMP, 2, {REG8,MEMORY,0}, "\301\1\x3A\110", IF_8086|IF_SM},
    {I_CMP, 2, {REG8,REG8,0}, "\301\1\x3A\110", IF_8086},
    {I_CMP, 2, {REG16,MEMORY,0}, "\320\301\1\x3B\110", IF_8086|IF_SM},
    {I_CMP, 2, {REG16,REG16,0}, "\320\301\1\x3B\110", IF_8086},
    {I_CMP, 2, {REG32,MEMORY,0}, "\321\301\1\x3B\110", IF_386|IF_SM},
    {I_CMP, 2, {REG32,REG32,0}, "\321\301\1\x3B\110", IF_386},
    {I_CMP, 2, {REGMEM|BITS16,IMMEDIATE|BITS8,0}, "\320\300\1\x83\207\15", IF_8086},
    {I_CMP, 2, {REGMEM|BITS32,IMMEDIATE|BITS8,0}, "\321\300\1\x83\207\15", IF_386},
    {I_CMP, 2, {REG_AL,IMMEDIATE,0}, "\1\x3C\21", IF_8086|IF_SM},
    {I_CMP, 2, {REG_AX,IMMEDIATE,0}, "\320\1\x3D\31", IF_8086|IF_SM},
    {I_CMP, 2, {REG_EAX,IMMEDIATE,0}, "\321\1\x3D\41", IF_386|IF_SM},
    {I_CMP, 2, {REGMEM|BITS8,IMMEDIATE,0}, "\300\1\x80\207\21", IF_8086|IF_SM},
    {I_CMP, 2, {REGMEM|BITS16,IMMEDIATE,0}, "\320\300\1\x81\207\31", IF_8086|IF_SM},
    {I_CMP, 2, {REGMEM|BITS32,IMMEDIATE,0}, "\321\300\1\x81\207\41", IF_386|IF_SM},
    {I_CMP, 2, {MEMORY,IMMEDIATE|BITS8,0}, "\300\1\x80\207\21", IF_8086|IF_SM},
    {I_CMP, 2, {MEMORY,IMMEDIATE|BITS16,0}, "\320\300\1\x81\207\31", IF_8086|IF_SM},
    {I_CMP, 2, {MEMORY,IMMEDIATE|BITS32,0}, "\321\300\1\x81\207\41", IF_386|IF_SM},
    {-1}
};

static struct itemplate instrux_CMPSB[] = {
    {I_CMPSB, 0, {0,0,0}, "\1\xA6", IF_8086},
    {-1}
};

static struct itemplate instrux_CMPSD[] = {
    {I_CMPSD, 0, {0,0,0}, "\321\1\xA7", IF_386},
    {-1}
};

static struct itemplate instrux_CMPSW[] = {
    {I_CMPSW, 0, {0,0,0}, "\320\1\xA7", IF_8086},
    {-1}
};

static struct itemplate instrux_CMPXCHG[] = {
    {I_CMPXCHG, 2, {MEMORY,REG8,0}, "\300\2\x0F\xB0\101", IF_PENT|IF_SM},
    {I_CMPXCHG, 2, {REG8,REG8,0}, "\300\2\x0F\xB0\101", IF_PENT},
    {I_CMPXCHG, 2, {MEMORY,REG16,0}, "\320\300\2\x0F\xB1\101", IF_PENT|IF_SM},
    {I_CMPXCHG, 2, {REG16,REG16,0}, "\320\300\2\x0F\xB1\101", IF_PENT},
    {I_CMPXCHG, 2, {MEMORY,REG32,0}, "\321\300\2\x0F\xB1\101", IF_PENT|IF_SM},
    {I_CMPXCHG, 2, {REG32,REG32,0}, "\321\300\2\x0F\xB1\101", IF_PENT},
    {-1}
};

static struct itemplate instrux_CMPXCHG486[] = {
    {I_CMPXCHG486, 2, {MEMORY,REG8,0}, "\300\2\x0F\xA6\101", IF_486|IF_SM|IF_UNDOC},
    {I_CMPXCHG486, 2, {REG8,REG8,0}, "\300\2\x0F\xA6\101", IF_486|IF_UNDOC},
    {I_CMPXCHG486, 2, {MEMORY,REG16,0}, "\320\300\2\x0F\xA7\101", IF_486|IF_SM|IF_UNDOC},
    {I_CMPXCHG486, 2, {REG16,REG16,0}, "\320\300\2\x0F\xA7\101", IF_486|IF_UNDOC},
    {I_CMPXCHG486, 2, {MEMORY,REG32,0}, "\321\300\2\x0F\xA7\101", IF_486|IF_SM|IF_UNDOC},
    {I_CMPXCHG486, 2, {REG32,REG32,0}, "\321\300\2\x0F\xA7\101", IF_486|IF_UNDOC},
    {-1}
};

static struct itemplate instrux_CMPXCHG8B[] = {
    {I_CMPXCHG8B, 1, {MEMORY,0,0}, "\300\2\x0F\xC7\201", IF_PENT},
    {-1}
};

static struct itemplate instrux_CPUID[] = {
    {I_CPUID, 0, {0,0,0}, "\2\x0F\xA2", IF_PENT},
    {-1}
};

static struct itemplate instrux_CWD[] = {
    {I_CWD, 0, {0,0,0}, "\320\1\x99", IF_8086},
    {-1}
};

static struct itemplate instrux_CWDE[] = {
    {I_CWDE, 0, {0,0,0}, "\321\1\x98", IF_386},
    {-1}
};

static struct itemplate instrux_DAA[] = {
    {I_DAA, 0, {0,0,0}, "\1\x27", IF_8086},
    {-1}
};

static struct itemplate instrux_DAS[] = {
    {I_DAS, 0, {0,0,0}, "\1\x2F", IF_8086},
    {-1}
};

static struct itemplate instrux_DB[] = {
    {-1}
};

static struct itemplate instrux_DD[] = {
    {-1}
};

static struct itemplate instrux_DEC[] = {
    {I_DEC, 1, {REG16,0,0}, "\320\10\x48", IF_8086},
    {I_DEC, 1, {REG32,0,0}, "\321\10\x48", IF_386},
    {I_DEC, 1, {REGMEM|BITS8,0,0}, "\300\1\xFE\201", IF_8086},
    {I_DEC, 1, {REGMEM|BITS16,0,0}, "\320\300\1\xFF\201", IF_8086},
    {I_DEC, 1, {REGMEM|BITS32,0,0}, "\321\300\1\xFF\201", IF_386},
    {-1}
};

static struct itemplate instrux_DIV[] = {
    {I_DIV, 1, {REGMEM|BITS8,0,0}, "\300\1\xF6\206", IF_8086},
    {I_DIV, 1, {REGMEM|BITS16,0,0}, "\320\300\1\xF7\206", IF_8086},
    {I_DIV, 1, {REGMEM|BITS32,0,0}, "\321\300\1\xF7\206", IF_386},
    {-1}
};

static struct itemplate instrux_DQ[] = {
    {-1}
};

static struct itemplate instrux_DT[] = {
    {-1}
};

static struct itemplate instrux_DW[] = {
    {-1}
};

static struct itemplate instrux_EMMS[] = {
    {I_EMMS, 0, {0,0,0}, "\2\x0F\x77", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_ENTER[] = {
    {I_ENTER, 2, {IMMEDIATE,IMMEDIATE,0}, "\1\xC8\30\25", IF_186},
    {-1}
};

static struct itemplate instrux_EQU[] = {
    {I_EQU, 1, {IMMEDIATE,0,0}, "\0", IF_8086},
    {I_EQU, 2, {IMMEDIATE|COLON,IMMEDIATE,0}, "\0", IF_8086},
    {-1}
};

static struct itemplate instrux_F2XM1[] = {
    {I_F2XM1, 0, {0,0,0}, "\2\xD9\xF0", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FABS[] = {
    {I_FABS, 0, {0,0,0}, "\2\xD9\xE1", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FADD[] = {
    {I_FADD, 1, {MEMORY|BITS32,0,0}, "\300\1\xD8\200", IF_8086|IF_FPU},
    {I_FADD, 1, {MEMORY|BITS64,0,0}, "\300\1\xDC\200", IF_8086|IF_FPU},
    {I_FADD, 1, {FPUREG|TO,0,0}, "\1\xDC\10\xC0", IF_8086|IF_FPU},
    {I_FADD, 1, {FPUREG,0,0}, "\1\xD8\10\xC0", IF_8086|IF_FPU},
    {I_FADD, 2, {FPUREG,FPU0,0}, "\1\xDC\10\xC0", IF_8086|IF_FPU},
    {I_FADD, 2, {FPU0,FPUREG,0}, "\1\xD8\11\xC0", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FADDP[] = {
    {I_FADDP, 1, {FPUREG,0,0}, "\1\xDE\10\xC0", IF_8086|IF_FPU},
    {I_FADDP, 2, {FPUREG,FPU0,0}, "\1\xDE\10\xC0", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FBLD[] = {
    {I_FBLD, 1, {MEMORY|BITS80,0,0}, "\300\1\xDF\204", IF_8086|IF_FPU},
    {I_FBLD, 1, {MEMORY,0,0}, "\300\1\xDF\204", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FBSTP[] = {
    {I_FBSTP, 1, {MEMORY|BITS80,0,0}, "\300\1\xDF\206", IF_8086|IF_FPU},
    {I_FBSTP, 1, {MEMORY,0,0}, "\300\1\xDF\206", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FCHS[] = {
    {I_FCHS, 0, {0,0,0}, "\2\xD9\xE0", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FCLEX[] = {
    {I_FCLEX, 0, {0,0,0}, "\3\x9B\xDB\xE2", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FCMOVB[] = {
    {I_FCMOVB, 1, {FPUREG,0,0}, "\1\xDA\10\xC0", IF_P6|IF_FPU},
    {I_FCMOVB, 2, {FPU0,FPUREG,0}, "\1\xDA\11\xC0", IF_P6|IF_FPU},
    {-1}
};

static struct itemplate instrux_FCMOVBE[] = {
    {I_FCMOVBE, 1, {FPUREG,0,0}, "\1\xDA\10\xD0", IF_P6|IF_FPU},
    {I_FCMOVBE, 2, {FPU0,FPUREG,0}, "\1\xDA\11\xD0", IF_P6|IF_FPU},
    {-1}
};

static struct itemplate instrux_FCMOVE[] = {
    {I_FCMOVE, 1, {FPUREG,0,0}, "\1\xDA\10\xC8", IF_P6|IF_FPU},
    {I_FCMOVE, 2, {FPU0,FPUREG,0}, "\1\xDA\11\xC8", IF_P6|IF_FPU},
    {-1}
};

static struct itemplate instrux_FCMOVNB[] = {
    {I_FCMOVNB, 1, {FPUREG,0,0}, "\1\xDB\10\xC0", IF_P6|IF_FPU},
    {I_FCMOVNB, 2, {FPU0,FPUREG,0}, "\1\xDB\11\xC0", IF_P6|IF_FPU},
    {-1}
};

static struct itemplate instrux_FCMOVNBE[] = {
    {I_FCMOVNBE, 1, {FPUREG,0,0}, "\1\xDB\10\xD0", IF_P6|IF_FPU},
    {I_FCMOVNBE, 2, {FPU0,FPUREG,0}, "\1\xDB\11\xD0", IF_P6|IF_FPU},
    {-1}
};

static struct itemplate instrux_FCMOVNE[] = {
    {I_FCMOVNE, 1, {FPUREG,0,0}, "\1\xDB\10\xC8", IF_P6|IF_FPU},
    {I_FCMOVNE, 2, {FPU0,FPUREG,0}, "\1\xDB\11\xC8", IF_P6|IF_FPU},
    {-1}
};

static struct itemplate instrux_FCMOVNU[] = {
    {I_FCMOVNU, 1, {FPUREG,0,0}, "\1\xDB\10\xD8", IF_P6|IF_FPU},
    {I_FCMOVNU, 2, {FPU0,FPUREG,0}, "\1\xDB\11\xD8", IF_P6|IF_FPU},
    {-1}
};

static struct itemplate instrux_FCMOVU[] = {
    {I_FCMOVU, 1, {FPUREG,0,0}, "\1\xDA\10\xD8", IF_P6|IF_FPU},
    {I_FCMOVU, 2, {FPU0,FPUREG,0}, "\1\xDA\11\xD8", IF_P6|IF_FPU},
    {-1}
};

static struct itemplate instrux_FCOM[] = {
    {I_FCOM, 1, {MEMORY|BITS32,0,0}, "\300\1\xD8\202", IF_8086|IF_FPU},
    {I_FCOM, 1, {MEMORY|BITS64,0,0}, "\300\1\xDC\202", IF_8086|IF_FPU},
    {I_FCOM, 1, {FPUREG,0,0}, "\1\xD8\10\xD0", IF_8086|IF_FPU},
    {I_FCOM, 2, {FPU0,FPUREG,0}, "\1\xD8\11\xD0", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FCOMI[] = {
    {I_FCOMI, 1, {FPUREG,0,0}, "\1\xDB\10\xF0", IF_P6|IF_FPU},
    {I_FCOMI, 2, {FPU0,FPUREG,0}, "\1\xDB\11\xF0", IF_P6|IF_FPU},
    {-1}
};

static struct itemplate instrux_FCOMIP[] = {
    {I_FCOMIP, 1, {FPUREG,0,0}, "\1\xDF\10\xF0", IF_P6|IF_FPU},
    {I_FCOMIP, 2, {FPU0,FPUREG,0}, "\1\xDF\11\xF0", IF_P6|IF_FPU},
    {-1}
};

static struct itemplate instrux_FCOMP[] = {
    {I_FCOMP, 1, {MEMORY|BITS32,0,0}, "\300\1\xD8\203", IF_8086|IF_FPU},
    {I_FCOMP, 1, {MEMORY|BITS64,0,0}, "\300\1\xDC\203", IF_8086|IF_FPU},
    {I_FCOMP, 1, {FPUREG,0,0}, "\1\xD8\10\xD8", IF_8086|IF_FPU},
    {I_FCOMP, 2, {FPU0,FPUREG,0}, "\1\xD8\11\xD8", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FCOMPP[] = {
    {I_FCOMPP, 0, {0,0,0}, "\2\xDE\xD9", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FCOS[] = {
    {I_FCOS, 0, {0,0,0}, "\2\xD9\xFF", IF_386|IF_FPU},
    {-1}
};

static struct itemplate instrux_FDECSTP[] = {
    {I_FDECSTP, 0, {0,0,0}, "\2\xD9\xF6", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FDISI[] = {
    {I_FDISI, 0, {0,0,0}, "\3\x9B\xDB\xE1", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FDIV[] = {
    {I_FDIV, 1, {MEMORY|BITS32,0,0}, "\300\1\xD8\206", IF_8086|IF_FPU},
    {I_FDIV, 1, {MEMORY|BITS64,0,0}, "\300\1\xDC\206", IF_8086|IF_FPU},
    {I_FDIV, 1, {FPUREG|TO,0,0}, "\1\xDC\10\xF8", IF_8086|IF_FPU},
    {I_FDIV, 2, {FPUREG,FPU0,0}, "\1\xDC\10\xF8", IF_8086|IF_FPU},
    {I_FDIV, 1, {FPUREG,0,0}, "\1\xD8\10\xF0", IF_8086|IF_FPU},
    {I_FDIV, 2, {FPU0,FPUREG,0}, "\1\xD8\11\xF0", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FDIVP[] = {
    {I_FDIVP, 2, {FPUREG,FPU0,0}, "\1\xDE\10\xF8", IF_8086|IF_FPU},
    {I_FDIVP, 1, {FPUREG,0,0}, "\1\xDE\10\xF8", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FDIVR[] = {
    {I_FDIVR, 1, {MEMORY|BITS32,0,0}, "\300\1\xD8\207", IF_8086|IF_FPU},
    {I_FDIVR, 1, {MEMORY|BITS64,0,0}, "\300\1\xDC\207", IF_8086|IF_FPU},
    {I_FDIVR, 1, {FPUREG|TO,0,0}, "\1\xDC\10\xF0", IF_8086|IF_FPU},
    {I_FDIVR, 2, {FPUREG,FPU0,0}, "\1\xDC\10\xF0", IF_8086|IF_FPU},
    {I_FDIVR, 1, {FPUREG,0,0}, "\1\xD8\10\xF8", IF_8086|IF_FPU},
    {I_FDIVR, 2, {FPU0,FPUREG,0}, "\1\xD8\11\xF8", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FDIVRP[] = {
    {I_FDIVRP, 1, {FPUREG,0,0}, "\1\xDE\10\xF0", IF_8086|IF_FPU},
    {I_FDIVRP, 2, {FPUREG,FPU0,0}, "\1\xDE\10\xF0", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FENI[] = {
    {I_FENI, 0, {0,0,0}, "\3\x9B\xDB\xE0", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FFREE[] = {
    {I_FFREE, 1, {FPUREG,0,0}, "\1\xDD\10\xC0", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FIADD[] = {
    {I_FIADD, 1, {MEMORY|BITS32,0,0}, "\300\1\xDA\200", IF_8086|IF_FPU},
    {I_FIADD, 1, {MEMORY|BITS16,0,0}, "\300\1\xDE\200", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FICOM[] = {
    {I_FICOM, 1, {MEMORY|BITS32,0,0}, "\300\1\xDA\202", IF_8086|IF_FPU},
    {I_FICOM, 1, {MEMORY|BITS16,0,0}, "\300\1\xDE\202", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FICOMP[] = {
    {I_FICOMP, 1, {MEMORY|BITS32,0,0}, "\300\1\xDA\203", IF_8086|IF_FPU},
    {I_FICOMP, 1, {MEMORY|BITS16,0,0}, "\300\1\xDE\203", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FIDIV[] = {
    {I_FIDIV, 1, {MEMORY|BITS32,0,0}, "\300\1\xDA\206", IF_8086|IF_FPU},
    {I_FIDIV, 1, {MEMORY|BITS16,0,0}, "\300\1\xDE\206", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FIDIVR[] = {
    {I_FIDIVR, 1, {MEMORY|BITS32,0,0}, "\300\1\xDA\207", IF_8086|IF_FPU},
    {I_FIDIVR, 1, {MEMORY|BITS16,0,0}, "\300\1\xDE\207", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FILD[] = {
    {I_FILD, 1, {MEMORY|BITS32,0,0}, "\300\1\xDB\200", IF_8086|IF_FPU},
    {I_FILD, 1, {MEMORY|BITS16,0,0}, "\300\1\xDF\200", IF_8086|IF_FPU},
    {I_FILD, 1, {MEMORY|BITS64,0,0}, "\300\1\xDF\205", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FIMUL[] = {
    {I_FIMUL, 1, {MEMORY|BITS32,0,0}, "\300\1\xDA\201", IF_8086|IF_FPU},
    {I_FIMUL, 1, {MEMORY|BITS16,0,0}, "\300\1\xDE\201", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FINCSTP[] = {
    {I_FINCSTP, 0, {0,0,0}, "\2\xD9\xF7", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FINIT[] = {
    {I_FINIT, 0, {0,0,0}, "\3\x9B\xDB\xE3", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FIST[] = {
    {I_FIST, 1, {MEMORY|BITS32,0,0}, "\300\1\xDB\202", IF_8086|IF_FPU},
    {I_FIST, 1, {MEMORY|BITS16,0,0}, "\300\1\xDF\202", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FISTP[] = {
    {I_FISTP, 1, {MEMORY|BITS32,0,0}, "\300\1\xDB\203", IF_8086|IF_FPU},
    {I_FISTP, 1, {MEMORY|BITS16,0,0}, "\300\1\xDF\203", IF_8086|IF_FPU},
    {I_FISTP, 1, {MEMORY|BITS64,0,0}, "\300\1\xDF\207", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FISUB[] = {
    {I_FISUB, 1, {MEMORY|BITS32,0,0}, "\300\1\xDA\204", IF_8086|IF_FPU},
    {I_FISUB, 1, {MEMORY|BITS16,0,0}, "\300\1\xDE\204", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FISUBR[] = {
    {I_FISUBR, 1, {MEMORY|BITS32,0,0}, "\300\1\xDA\205", IF_8086|IF_FPU},
    {I_FISUBR, 1, {MEMORY|BITS16,0,0}, "\300\1\xDE\205", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FLD[] = {
    {I_FLD, 1, {MEMORY|BITS32,0,0}, "\300\1\xD9\200", IF_8086|IF_FPU},
    {I_FLD, 1, {MEMORY|BITS64,0,0}, "\300\1\xDD\200", IF_8086|IF_FPU},
    {I_FLD, 1, {MEMORY|BITS80,0,0}, "\300\1\xDB\205", IF_8086|IF_FPU},
    {I_FLD, 1, {FPUREG,0,0}, "\1\xD9\10\xC0", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FLD1[] = {
    {I_FLD1, 0, {0,0,0}, "\2\xD9\xE8", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FLDCW[] = {
    {I_FLDCW, 1, {MEMORY,0,0}, "\300\1\xD9\205", IF_8086|IF_FPU|IF_SW},
    {-1}
};

static struct itemplate instrux_FLDENV[] = {
    {I_FLDENV, 1, {MEMORY,0,0}, "\300\1\xD9\204", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FLDL2E[] = {
    {I_FLDL2E, 0, {0,0,0}, "\2\xD9\xEA", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FLDL2T[] = {
    {I_FLDL2T, 0, {0,0,0}, "\2\xD9\xE9", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FLDLG2[] = {
    {I_FLDLG2, 0, {0,0,0}, "\2\xD9\xEC", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FLDLN2[] = {
    {I_FLDLN2, 0, {0,0,0}, "\2\xD9\xED", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FLDPI[] = {
    {I_FLDPI, 0, {0,0,0}, "\2\xD9\xEB", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FLDZ[] = {
    {I_FLDZ, 0, {0,0,0}, "\2\xD9\xEE", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FMUL[] = {
    {I_FMUL, 1, {MEMORY|BITS32,0,0}, "\300\1\xD8\201", IF_8086|IF_FPU},
    {I_FMUL, 1, {MEMORY|BITS64,0,0}, "\300\1\xDC\201", IF_8086|IF_FPU},
    {I_FMUL, 1, {FPUREG|TO,0,0}, "\1\xDC\10\xC8", IF_8086|IF_FPU},
    {I_FMUL, 2, {FPUREG,FPU0,0}, "\1\xDC\10\xC8", IF_8086|IF_FPU},
    {I_FMUL, 1, {FPUREG,0,0}, "\1\xD8\10\xC8", IF_8086|IF_FPU},
    {I_FMUL, 2, {FPU0,FPUREG,0}, "\1\xD8\11\xC8", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FMULP[] = {
    {I_FMULP, 1, {FPUREG,0,0}, "\1\xDE\10\xC8", IF_8086|IF_FPU},
    {I_FMULP, 2, {FPUREG,FPU0,0}, "\1\xDE\10\xC8", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FNCLEX[] = {
    {I_FNCLEX, 0, {0,0,0}, "\2\xDB\xE2", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FNDISI[] = {
    {I_FNDISI, 0, {0,0,0}, "\2\xDB\xE1", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FNENI[] = {
    {I_FNENI, 0, {0,0,0}, "\2\xDB\xE0", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FNINIT[] = {
    {I_FNINIT, 0, {0,0,0}, "\2\xDB\xE3", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FNOP[] = {
    {I_FNOP, 0, {0,0,0}, "\2\xD9\xD0", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FNSAVE[] = {
    {I_FNSAVE, 1, {MEMORY,0,0}, "\300\1\xDD\206", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FNSTCW[] = {
    {I_FNSTCW, 1, {MEMORY,0,0}, "\300\1\xD9\207", IF_8086|IF_FPU|IF_SW},
    {-1}
};

static struct itemplate instrux_FNSTENV[] = {
    {I_FNSTENV, 1, {MEMORY,0,0}, "\300\1\xD9\206", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FNSTSW[] = {
    {I_FNSTSW, 1, {MEMORY,0,0}, "\300\1\xDD\207", IF_8086|IF_FPU|IF_SW},
    {I_FNSTSW, 1, {REG_AX,0,0}, "\2\xDF\xE0", IF_286|IF_FPU},
    {-1}
};

static struct itemplate instrux_FPATAN[] = {
    {I_FPATAN, 0, {0,0,0}, "\2\xD9\xF3", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FPREM[] = {
    {I_FPREM, 0, {0,0,0}, "\2\xD9\xF8", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FPREM1[] = {
    {I_FPREM1, 0, {0,0,0}, "\2\xD9\xF5", IF_386|IF_FPU},
    {-1}
};

static struct itemplate instrux_FPTAN[] = {
    {I_FPTAN, 0, {0,0,0}, "\2\xD9\xF2", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FRNDINT[] = {
    {I_FRNDINT, 0, {0,0,0}, "\2\xD9\xFC", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FRSTOR[] = {
    {I_FRSTOR, 1, {MEMORY,0,0}, "\300\1\xDD\204", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FSAVE[] = {
    {I_FSAVE, 1, {MEMORY,0,0}, "\300\2\x9B\xDD\206", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FSCALE[] = {
    {I_FSCALE, 0, {0,0,0}, "\2\xD9\xFD", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FSETPM[] = {
    {I_FSETPM, 0, {0,0,0}, "\2\xDB\xE4", IF_286|IF_FPU},
    {-1}
};

static struct itemplate instrux_FSIN[] = {
    {I_FSIN, 0, {0,0,0}, "\2\xD9\xFE", IF_386|IF_FPU},
    {-1}
};

static struct itemplate instrux_FSINCOS[] = {
    {I_FSINCOS, 0, {0,0,0}, "\2\xD9\xFB", IF_386|IF_FPU},
    {-1}
};

static struct itemplate instrux_FSQRT[] = {
    {I_FSQRT, 0, {0,0,0}, "\2\xD9\xFA", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FST[] = {
    {I_FST, 1, {MEMORY|BITS32,0,0}, "\300\1\xD9\202", IF_8086|IF_FPU},
    {I_FST, 1, {MEMORY|BITS64,0,0}, "\300\1\xDD\202", IF_8086|IF_FPU},
    {I_FST, 1, {FPUREG,0,0}, "\1\xDD\10\xD0", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FSTCW[] = {
    {I_FSTCW, 1, {MEMORY,0,0}, "\300\2\x9B\xD9\207", IF_8086|IF_FPU|IF_SW},
    {-1}
};

static struct itemplate instrux_FSTENV[] = {
    {I_FSTENV, 1, {MEMORY,0,0}, "\300\2\x9B\xD9\206", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FSTP[] = {
    {I_FSTP, 1, {MEMORY|BITS32,0,0}, "\300\1\xD9\203", IF_8086|IF_FPU},
    {I_FSTP, 1, {MEMORY|BITS64,0,0}, "\300\1\xDD\203", IF_8086|IF_FPU},
    {I_FSTP, 1, {MEMORY|BITS80,0,0}, "\300\1\xDB\207", IF_8086|IF_FPU},
    {I_FSTP, 1, {FPUREG,0,0}, "\1\xDD\10\xD8", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FSTSW[] = {
    {I_FSTSW, 1, {MEMORY,0,0}, "\300\2\x9B\xDD\207", IF_8086|IF_FPU|IF_SW},
    {I_FSTSW, 1, {REG_AX,0,0}, "\3\x9B\xDF\xE0", IF_286|IF_FPU},
    {-1}
};

static struct itemplate instrux_FSUB[] = {
    {I_FSUB, 1, {MEMORY|BITS32,0,0}, "\300\1\xD8\204", IF_8086|IF_FPU},
    {I_FSUB, 1, {MEMORY|BITS64,0,0}, "\300\1\xDC\204", IF_8086|IF_FPU},
    {I_FSUB, 1, {FPUREG|TO,0,0}, "\1\xDC\10\xE8", IF_8086|IF_FPU},
    {I_FSUB, 2, {FPUREG,FPU0,0}, "\1\xDC\10\xE8", IF_8086|IF_FPU},
    {I_FSUB, 1, {FPUREG,0,0}, "\1\xD8\10\xE0", IF_8086|IF_FPU},
    {I_FSUB, 2, {FPU0,FPUREG,0}, "\1\xD8\11\xE0", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FSUBP[] = {
    {I_FSUBP, 1, {FPUREG,0,0}, "\1\xDE\10\xE8", IF_8086|IF_FPU},
    {I_FSUBP, 2, {FPUREG,FPU0,0}, "\1\xDE\10\xE8", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FSUBR[] = {
    {I_FSUBR, 1, {MEMORY|BITS32,0,0}, "\300\1\xD8\205", IF_8086|IF_FPU},
    {I_FSUBR, 1, {MEMORY|BITS64,0,0}, "\300\1\xDC\205", IF_8086|IF_FPU},
    {I_FSUBR, 1, {FPUREG|TO,0,0}, "\1\xDC\10\xE0", IF_8086|IF_FPU},
    {I_FSUBR, 2, {FPUREG,FPU0,0}, "\1\xDC\10\xE0", IF_8086|IF_FPU},
    {I_FSUBR, 1, {FPUREG,0,0}, "\1\xD8\10\xE8", IF_8086|IF_FPU},
    {I_FSUBR, 2, {FPU0,FPUREG,0}, "\1\xD8\11\xE8", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FSUBRP[] = {
    {I_FSUBRP, 1, {FPUREG,0,0}, "\1\xDE\10\xE0", IF_8086|IF_FPU},
    {I_FSUBRP, 2, {FPUREG,FPU0,0}, "\1\xDE\10\xE0", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FTST[] = {
    {I_FTST, 0, {0,0,0}, "\2\xD9\xE4", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FUCOM[] = {
    {I_FUCOM, 1, {FPUREG,0,0}, "\1\xDD\10\xE0", IF_386|IF_FPU},
    {I_FUCOM, 2, {FPU0,FPUREG,0}, "\1\xDD\11\xE0", IF_386|IF_FPU},
    {-1}
};

static struct itemplate instrux_FUCOMI[] = {
    {I_FUCOMI, 1, {FPUREG,0,0}, "\1\xDB\10\xE8", IF_P6|IF_FPU},
    {I_FUCOMI, 2, {FPU0,FPUREG,0}, "\1\xDB\11\xE8", IF_P6|IF_FPU},
    {-1}
};

static struct itemplate instrux_FUCOMIP[] = {
    {I_FUCOMIP, 1, {FPUREG,0,0}, "\1\xDF\10\xE8", IF_P6|IF_FPU},
    {I_FUCOMIP, 2, {FPU0,FPUREG,0}, "\1\xDF\11\xE8", IF_P6|IF_FPU},
    {-1}
};

static struct itemplate instrux_FUCOMP[] = {
    {I_FUCOMP, 1, {FPUREG,0,0}, "\1\xDD\10\xE8", IF_386|IF_FPU},
    {I_FUCOMP, 2, {FPU0,FPUREG,0}, "\1\xDD\11\xE8", IF_386|IF_FPU},
    {-1}
};

static struct itemplate instrux_FUCOMPP[] = {
    {I_FUCOMPP, 0, {0,0,0}, "\2\xDA\xE9", IF_386|IF_FPU},
    {-1}
};

static struct itemplate instrux_FXAM[] = {
    {I_FXAM, 0, {0,0,0}, "\2\xD9\xE5", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FXCH[] = {
    {I_FXCH, 0, {0,0,0}, "\2\xD9\xC9", IF_8086|IF_FPU},
    {I_FXCH, 1, {FPUREG,0,0}, "\1\xD9\10\xC8", IF_8086|IF_FPU},
    {I_FXCH, 2, {FPUREG,FPU0,0}, "\1\xD9\10\xC8", IF_8086|IF_FPU},
    {I_FXCH, 2, {FPU0,FPUREG,0}, "\1\xD9\11\xC8", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FXTRACT[] = {
    {I_FXTRACT, 0, {0,0,0}, "\2\xD9\xF4", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FYL2X[] = {
    {I_FYL2X, 0, {0,0,0}, "\2\xD9\xF1", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_FYL2XP1[] = {
    {I_FYL2XP1, 0, {0,0,0}, "\2\xD9\xF9", IF_8086|IF_FPU},
    {-1}
};

static struct itemplate instrux_HLT[] = {
    {I_HLT, 0, {0,0,0}, "\1\xF4", IF_8086},
    {-1}
};

static struct itemplate instrux_IBTS[] = {
    {I_IBTS, 2, {MEMORY,REG16,0}, "\320\300\2\x0F\xA7\101", IF_386|IF_SW|IF_UNDOC},
    {I_IBTS, 2, {REG16,REG16,0}, "\320\300\2\x0F\xA7\101", IF_386|IF_UNDOC},
    {I_IBTS, 2, {MEMORY,REG32,0}, "\321\300\2\x0F\xA7\101", IF_386|IF_SD|IF_UNDOC},
    {I_IBTS, 2, {REG32,REG32,0}, "\321\300\2\x0F\xA7\101", IF_386|IF_UNDOC},
    {-1}
};

static struct itemplate instrux_ICEBP[] = {
    {I_ICEBP, 0, {0,0,0}, "\1\xF1", IF_P6},
    {-1}
};

static struct itemplate instrux_IDIV[] = {
    {I_IDIV, 1, {REGMEM|BITS8,0,0}, "\300\1\xF6\207", IF_8086},
    {I_IDIV, 1, {REGMEM|BITS16,0,0}, "\320\300\1\xF7\207", IF_8086},
    {I_IDIV, 1, {REGMEM|BITS32,0,0}, "\321\300\1\xF7\207", IF_386},
    {-1}
};

static struct itemplate instrux_IMUL[] = {
    {I_IMUL, 1, {REGMEM|BITS8,0,0}, "\300\1\xF6\205", IF_8086},
    {I_IMUL, 1, {REGMEM|BITS16,0,0}, "\320\300\1\xF7\205", IF_8086},
    {I_IMUL, 1, {REGMEM|BITS32,0,0}, "\321\300\1\xF7\205", IF_386},
    {I_IMUL, 2, {REG16,MEMORY,0}, "\320\301\2\x0F\xAF\110", IF_386|IF_SM},
    {I_IMUL, 2, {REG16,REG16,0}, "\320\301\2\x0F\xAF\110", IF_386},
    {I_IMUL, 2, {REG32,MEMORY,0}, "\321\301\2\x0F\xAF\110", IF_386|IF_SM},
    {I_IMUL, 2, {REG32,REG32,0}, "\321\301\2\x0F\xAF\110", IF_386},
    {I_IMUL, 3, {REG16,MEMORY,IMMEDIATE|BITS8}, "\320\301\1\x6B\110\16", IF_286|IF_SM},
    {I_IMUL, 3, {REG16,REG16,IMMEDIATE|BITS8}, "\320\301\1\x6B\110\16", IF_286},
    {I_IMUL, 3, {REG16,MEMORY,IMMEDIATE}, "\320\301\1\x69\110\32", IF_286|IF_SM},
    {I_IMUL, 3, {REG16,REG16,IMMEDIATE}, "\320\301\1\x69\110\32", IF_286|IF_SM},
    {I_IMUL, 3, {REG32,MEMORY,IMMEDIATE|BITS8}, "\321\301\1\x6B\110\16", IF_386|IF_SM},
    {I_IMUL, 3, {REG32,REG32,IMMEDIATE|BITS8}, "\321\301\1\x6B\110\16", IF_386},
    {I_IMUL, 3, {REG32,MEMORY,IMMEDIATE}, "\321\301\1\x69\110\42", IF_386|IF_SM},
    {I_IMUL, 3, {REG32,REG32,IMMEDIATE}, "\321\301\1\x69\110\42", IF_386|IF_SM},
    {I_IMUL, 2, {REG16,IMMEDIATE|BITS8,0}, "\320\1\x6B\100\15", IF_286},
    {I_IMUL, 2, {REG16,IMMEDIATE,0}, "\320\1\x69\100\31", IF_286|IF_SM},
    {I_IMUL, 2, {REG32,IMMEDIATE|BITS8,0}, "\321\1\x6B\100\15", IF_386},
    {I_IMUL, 2, {REG32,IMMEDIATE,0}, "\321\1\x69\100\41", IF_386|IF_SM},
    {-1}
};

static struct itemplate instrux_IN[] = {
    {I_IN, 2, {REG_AL,IMMEDIATE,0}, "\1\xE4\25", IF_8086},
    {I_IN, 2, {REG_AX,IMMEDIATE,0}, "\320\1\xE5\25", IF_8086},
    {I_IN, 2, {REG_EAX,IMMEDIATE,0}, "\321\1\xE5\25", IF_386},
    {I_IN, 2, {REG_AL,REG_DX,0}, "\1\xEC", IF_8086},
    {I_IN, 2, {REG_AX,REG_DX,0}, "\320\1\xED", IF_8086},
    {I_IN, 2, {REG_EAX,REG_DX,0}, "\321\1\xED", IF_386},
    {-1}
};

static struct itemplate instrux_INC[] = {
    {I_INC, 1, {REG16,0,0}, "\320\10\x40", IF_8086},
    {I_INC, 1, {REG32,0,0}, "\321\10\x40", IF_386},
    {I_INC, 1, {REGMEM|BITS8,0,0}, "\300\1\xFE\200", IF_8086},
    {I_INC, 1, {REGMEM|BITS16,0,0}, "\320\300\1\xFF\200", IF_8086},
    {I_INC, 1, {REGMEM|BITS32,0,0}, "\321\300\1\xFF\200", IF_386},
    {-1}
};

static struct itemplate instrux_INCBIN[] = {
    {-1}
};

static struct itemplate instrux_INSB[] = {
    {I_INSB, 0, {0,0,0}, "\1\x6C", IF_186},
    {-1}
};

static struct itemplate instrux_INSD[] = {
    {I_INSD, 0, {0,0,0}, "\321\1\x6D", IF_386},
    {-1}
};

static struct itemplate instrux_INSW[] = {
    {I_INSW, 0, {0,0,0}, "\320\1\x6D", IF_186},
    {-1}
};

static struct itemplate instrux_INT[] = {
    {I_INT, 1, {IMMEDIATE,0,0}, "\1\xCD\24", IF_8086},
    {-1}
};

static struct itemplate instrux_INT01[] = {
    {I_INT01, 0, {0,0,0}, "\1\xF1", IF_P6},
    {-1}
};

static struct itemplate instrux_INT1[] = {
    {I_INT1, 0, {0,0,0}, "\1\xF1", IF_P6},
    {-1}
};

static struct itemplate instrux_INT3[] = {
    {I_INT3, 0, {0,0,0}, "\1\xCC", IF_8086},
    {-1}
};

static struct itemplate instrux_INTO[] = {
    {I_INTO, 0, {0,0,0}, "\1\xCE", IF_8086},
    {-1}
};

static struct itemplate instrux_INVD[] = {
    {I_INVD, 0, {0,0,0}, "\2\x0F\x08", IF_486},
    {-1}
};

static struct itemplate instrux_INVLPG[] = {
    {I_INVLPG, 1, {MEMORY,0,0}, "\300\2\x0F\x01\207", IF_486},
    {-1}
};

static struct itemplate instrux_IRET[] = {
    {I_IRET, 0, {0,0,0}, "\322\1\xCF", IF_8086},
    {-1}
};

static struct itemplate instrux_IRETD[] = {
    {I_IRETD, 0, {0,0,0}, "\321\1\xCF", IF_386},
    {-1}
};

static struct itemplate instrux_IRETW[] = {
    {I_IRETW, 0, {0,0,0}, "\320\1\xCF", IF_8086},
    {-1}
};

static struct itemplate instrux_JCXZ[] = {
    {I_JCXZ, 1, {IMMEDIATE,0,0}, "\320\1\xE3\50", IF_8086},
    {-1}
};

static struct itemplate instrux_JECXZ[] = {
    {I_JECXZ, 1, {IMMEDIATE,0,0}, "\321\1\xE3\50", IF_386},
    {-1}
};

static struct itemplate instrux_JMP[] = {
    {I_JMP, 1, {IMMEDIATE|SHORT,0,0}, "\1\xEB\50", IF_8086},
    {I_JMP, 1, {IMMEDIATE,0,0}, "\322\1\xE9\64", IF_8086},
    {I_JMP, 1, {IMMEDIATE|FAR,0,0}, "\322\1\xEA\34\37", IF_8086},
    {I_JMP, 2, {IMMEDIATE|COLON,IMMEDIATE,0}, "\322\1\xEA\35\30", IF_8086},
    {I_JMP, 2, {IMMEDIATE|BITS16|COLON,IMMEDIATE,0}, "\320\1\xEA\31\30", IF_8086},
    {I_JMP, 2, {IMMEDIATE|COLON,IMMEDIATE|BITS16,0}, "\320\1\xEA\31\30", IF_8086},
    {I_JMP, 2, {IMMEDIATE|BITS32|COLON,IMMEDIATE,0}, "\321\1\xEA\41\30", IF_386},
    {I_JMP, 2, {IMMEDIATE|COLON,IMMEDIATE|BITS32,0}, "\321\1\xEA\41\30", IF_386},
    {I_JMP, 1, {MEMORY|FAR,0,0}, "\322\300\1\xFF\205", IF_8086},
    {I_JMP, 1, {MEMORY|BITS16|FAR,0,0}, "\320\300\1\xFF\205", IF_8086},
    {I_JMP, 1, {MEMORY|BITS32|FAR,0,0}, "\321\300\1\xFF\205", IF_386},
    {I_JMP, 1, {MEMORY|NEAR,0,0}, "\322\300\1\xFF\204", IF_8086},
    {I_JMP, 1, {MEMORY|BITS16|NEAR,0,0}, "\320\300\1\xFF\204", IF_8086},
    {I_JMP, 1, {MEMORY|BITS32|NEAR,0,0}, "\321\300\1\xFF\204", IF_386},
    {I_JMP, 1, {REG16,0,0}, "\320\300\1\xFF\204", IF_8086},
    {I_JMP, 1, {REG32,0,0}, "\321\300\1\xFF\204", IF_386},
    {I_JMP, 1, {MEMORY,0,0}, "\322\300\1\xFF\204", IF_8086},
    {I_JMP, 1, {MEMORY|BITS16,0,0}, "\320\300\1\xFF\204", IF_8086},
    {I_JMP, 1, {MEMORY|BITS32,0,0}, "\321\300\1\xFF\204", IF_386},
    {-1}
};

static struct itemplate instrux_LAHF[] = {
    {I_LAHF, 0, {0,0,0}, "\1\x9F", IF_8086},
    {-1}
};

static struct itemplate instrux_LAR[] = {
    {I_LAR, 2, {REG16,MEMORY,0}, "\320\301\2\x0F\x02\110", IF_286|IF_PRIV|IF_SM},
    {I_LAR, 2, {REG16,REG16,0}, "\320\301\2\x0F\x02\110", IF_286|IF_PRIV},
    {I_LAR, 2, {REG32,MEMORY,0}, "\321\301\2\x0F\x02\110", IF_286|IF_PRIV|IF_SM},
    {I_LAR, 2, {REG32,REG32,0}, "\321\301\2\x0F\x02\110", IF_286|IF_PRIV},
    {-1}
};

static struct itemplate instrux_LDS[] = {
    {I_LDS, 2, {REG16,MEMORY,0}, "\320\301\1\xC5\110", IF_8086},
    {I_LDS, 2, {REG32,MEMORY,0}, "\321\301\1\xC5\110", IF_8086},
    {-1}
};

static struct itemplate instrux_LEA[] = {
    {I_LEA, 2, {REG16,MEMORY,0}, "\320\301\1\x8D\110", IF_8086},
    {I_LEA, 2, {REG32,MEMORY,0}, "\321\301\1\x8D\110", IF_8086},
    {-1}
};

static struct itemplate instrux_LEAVE[] = {
    {I_LEAVE, 0, {0,0,0}, "\1\xC9", IF_186},
    {-1}
};

static struct itemplate instrux_LES[] = {
    {I_LES, 2, {REG16,MEMORY,0}, "\320\301\1\xC4\110", IF_8086},
    {I_LES, 2, {REG32,MEMORY,0}, "\321\301\1\xC4\110", IF_8086},
    {-1}
};

static struct itemplate instrux_LFS[] = {
    {I_LFS, 2, {REG16,MEMORY,0}, "\320\301\2\x0F\xB4\110", IF_386},
    {I_LFS, 2, {REG32,MEMORY,0}, "\321\301\2\x0F\xB4\110", IF_386},
    {-1}
};

static struct itemplate instrux_LGDT[] = {
    {I_LGDT, 1, {MEMORY,0,0}, "\300\2\x0F\x01\202", IF_286|IF_PRIV},
    {-1}
};

static struct itemplate instrux_LGS[] = {
    {I_LGS, 2, {REG16,MEMORY,0}, "\320\301\2\x0F\xB5\110", IF_386},
    {I_LGS, 2, {REG32,MEMORY,0}, "\321\301\2\x0F\xB5\110", IF_386},
    {-1}
};

static struct itemplate instrux_LIDT[] = {
    {I_LIDT, 1, {MEMORY,0,0}, "\300\2\x0F\x01\203", IF_286|IF_PRIV},
    {-1}
};

static struct itemplate instrux_LLDT[] = {
    {I_LLDT, 1, {MEMORY,0,0}, "\300\1\x0F\17\202", IF_286|IF_PRIV},
    {I_LLDT, 1, {MEMORY|BITS16,0,0}, "\300\1\x0F\17\202", IF_286|IF_PRIV},
    {I_LLDT, 1, {REG16,0,0}, "\300\1\x0F\17\202", IF_286|IF_PRIV},
    {-1}
};

static struct itemplate instrux_LMSW[] = {
    {I_LMSW, 1, {MEMORY,0,0}, "\300\2\x0F\x01\206", IF_286|IF_PRIV},
    {I_LMSW, 1, {MEMORY|BITS16,0,0}, "\300\2\x0F\x01\206", IF_286|IF_PRIV},
    {I_LMSW, 1, {REG16,0,0}, "\300\2\x0F\x01\206", IF_286|IF_PRIV},
    {-1}
};

static struct itemplate instrux_LOADALL[] = {
    {I_LOADALL, 0, {0,0,0}, "\2\x0F\x07", IF_386|IF_UNDOC},
    {-1}
};

static struct itemplate instrux_LOADALL286[] = {
    {I_LOADALL286, 0, {0,0,0}, "\2\x0F\x05", IF_286|IF_UNDOC},
    {-1}
};

static struct itemplate instrux_LODSB[] = {
    {I_LODSB, 0, {0,0,0}, "\1\xAC", IF_8086},
    {-1}
};

static struct itemplate instrux_LODSD[] = {
    {I_LODSD, 0, {0,0,0}, "\321\1\xAD", IF_386},
    {-1}
};

static struct itemplate instrux_LODSW[] = {
    {I_LODSW, 0, {0,0,0}, "\320\1\xAD", IF_8086},
    {-1}
};

static struct itemplate instrux_LOOP[] = {
    {I_LOOP, 1, {IMMEDIATE,0,0}, "\312\1\xE2\50", IF_8086},
    {I_LOOP, 2, {IMMEDIATE,REG_CX,0}, "\310\1\xE2\50", IF_8086},
    {I_LOOP, 2, {IMMEDIATE,REG_ECX,0}, "\311\1\xE2\50", IF_386},
    {-1}
};

static struct itemplate instrux_LOOPE[] = {
    {I_LOOPE, 1, {IMMEDIATE,0,0}, "\312\1\xE1\50", IF_8086},
    {I_LOOPE, 2, {IMMEDIATE,REG_CX,0}, "\310\1\xE1\50", IF_8086},
    {I_LOOPE, 2, {IMMEDIATE,REG_ECX,0}, "\311\1\xE1\50", IF_386},
    {-1}
};

static struct itemplate instrux_LOOPNE[] = {
    {I_LOOPNE, 1, {IMMEDIATE,0,0}, "\312\1\xE0\50", IF_8086},
    {I_LOOPNE, 2, {IMMEDIATE,REG_CX,0}, "\310\1\xE0\50", IF_8086},
    {I_LOOPNE, 2, {IMMEDIATE,REG_ECX,0}, "\311\1\xE0\50", IF_386},
    {-1}
};

static struct itemplate instrux_LOOPNZ[] = {
    {I_LOOPNZ, 1, {IMMEDIATE,0,0}, "\312\1\xE0\50", IF_8086},
    {I_LOOPNZ, 2, {IMMEDIATE,REG_CX,0}, "\310\1\xE0\50", IF_8086},
    {I_LOOPNZ, 2, {IMMEDIATE,REG_ECX,0}, "\311\1\xE0\50", IF_386},
    {-1}
};

static struct itemplate instrux_LOOPZ[] = {
    {I_LOOPZ, 1, {IMMEDIATE,0,0}, "\312\1\xE1\50", IF_8086},
    {I_LOOPZ, 2, {IMMEDIATE,REG_CX,0}, "\310\1\xE1\50", IF_8086},
    {I_LOOPZ, 2, {IMMEDIATE,REG_ECX,0}, "\311\1\xE1\50", IF_386},
    {-1}
};

static struct itemplate instrux_LSL[] = {
    {I_LSL, 2, {REG16,MEMORY,0}, "\320\301\2\x0F\x03\110", IF_286|IF_PRIV|IF_SM},
    {I_LSL, 2, {REG16,REG16,0}, "\320\301\2\x0F\x03\110", IF_286|IF_PRIV},
    {I_LSL, 2, {REG32,MEMORY,0}, "\321\301\2\x0F\x03\110", IF_286|IF_PRIV|IF_SM},
    {I_LSL, 2, {REG32,REG32,0}, "\321\301\2\x0F\x03\110", IF_286|IF_PRIV},
    {-1}
};

static struct itemplate instrux_LSS[] = {
    {I_LSS, 2, {REG16,MEMORY,0}, "\320\301\2\x0F\xB2\110", IF_386},
    {I_LSS, 2, {REG32,MEMORY,0}, "\321\301\2\x0F\xB2\110", IF_386},
    {-1}
};

static struct itemplate instrux_LTR[] = {
    {I_LTR, 1, {MEMORY,0,0}, "\300\1\x0F\17\203", IF_286|IF_PRIV},
    {I_LTR, 1, {MEMORY|BITS16,0,0}, "\300\1\x0F\17\203", IF_286|IF_PRIV},
    {I_LTR, 1, {REG16,0,0}, "\300\1\x0F\17\203", IF_286|IF_PRIV},
    {-1}
};

static struct itemplate instrux_MOV[] = {
    {I_MOV, 2, {MEMORY,REG_CS,0}, "\320\300\1\x8C\201", IF_8086|IF_SM},
    {I_MOV, 2, {MEMORY,REG_DESS,0}, "\320\300\1\x8C\101", IF_8086|IF_SM},
    {I_MOV, 2, {MEMORY,REG_FSGS,0}, "\320\300\1\x8C\101", IF_386|IF_SM},
    {I_MOV, 2, {REG16,REG_CS,0}, "\320\300\1\x8C\201", IF_8086},
    {I_MOV, 2, {REG16,REG_DESS,0}, "\320\300\1\x8C\101", IF_8086},
    {I_MOV, 2, {REG16,REG_FSGS,0}, "\320\300\1\x8C\101", IF_386},
    {I_MOV, 2, {REGMEM|BITS32,REG_CS,0}, "\321\300\1\x8C\201", IF_8086},
    {I_MOV, 2, {REGMEM|BITS32,REG_DESS,0}, "\321\300\1\x8C\101", IF_8086},
    {I_MOV, 2, {REGMEM|BITS32,REG_FSGS,0}, "\321\300\1\x8C\101", IF_386},
    {I_MOV, 2, {REG_DESS,MEMORY,0}, "\320\301\1\x8E\110", IF_8086|IF_SM},
    {I_MOV, 2, {REG_FSGS,MEMORY,0}, "\320\301\1\x8E\110", IF_386|IF_SM},
    {I_MOV, 2, {REG_DESS,REG16,0}, "\320\301\1\x8E\110", IF_8086},
    {I_MOV, 2, {REG_FSGS,REG16,0}, "\320\301\1\x8E\110", IF_386},
    {I_MOV, 2, {REG_DESS,REGMEM|BITS32,0}, "\321\301\1\x8E\110", IF_8086},
    {I_MOV, 2, {REG_FSGS,REGMEM|BITS32,0}, "\321\301\1\x8E\110", IF_386},
    {I_MOV, 2, {REG_AL,MEM_OFFS,0}, "\301\1\xA0\35", IF_8086|IF_SM},
    {I_MOV, 2, {REG_AX,MEM_OFFS,0}, "\301\320\1\xA1\35", IF_8086|IF_SM},
    {I_MOV, 2, {REG_EAX,MEM_OFFS,0}, "\301\321\1\xA1\35", IF_386|IF_SM},
    {I_MOV, 2, {MEM_OFFS,REG_AL,0}, "\300\1\xA2\34", IF_8086|IF_SM},
    {I_MOV, 2, {MEM_OFFS,REG_AX,0}, "\300\320\1\xA3\34", IF_8086|IF_SM},
    {I_MOV, 2, {MEM_OFFS,REG_EAX,0}, "\300\321\1\xA3\34", IF_386|IF_SM},
    {I_MOV, 2, {REG32,REG_CR4,0}, "\2\x0F\x20\204", IF_PENT},
    {I_MOV, 2, {REG32,REG_CREG,0}, "\2\x0F\x20\101", IF_386},
    {I_MOV, 2, {REG32,REG_DREG,0}, "\2\x0F\x21\101", IF_386},
    {I_MOV, 2, {REG32,REG_TREG,0}, "\2\x0F\x24\101", IF_386},
    {I_MOV, 2, {REG_CR4,REG32,0}, "\2\x0F\x22\214", IF_PENT},
    {I_MOV, 2, {REG_CREG,REG32,0}, "\2\x0F\x22\110", IF_386},
    {I_MOV, 2, {REG_DREG,REG32,0}, "\2\x0F\x23\110", IF_386},
    {I_MOV, 2, {REG_TREG,REG32,0}, "\2\x0F\x26\110", IF_386},
    {I_MOV, 2, {MEMORY,REG8,0}, "\300\1\x88\101", IF_8086|IF_SM},
    {I_MOV, 2, {REG8,REG8,0}, "\300\1\x88\101", IF_8086},
    {I_MOV, 2, {MEMORY,REG16,0}, "\320\300\1\x89\101", IF_8086|IF_SM},
    {I_MOV, 2, {REG16,REG16,0}, "\320\300\1\x89\101", IF_8086},
    {I_MOV, 2, {MEMORY,REG32,0}, "\321\300\1\x89\101", IF_386|IF_SM},
    {I_MOV, 2, {REG32,REG32,0}, "\321\300\1\x89\101", IF_386},
    {I_MOV, 2, {REG8,MEMORY,0}, "\301\1\x8A\110", IF_8086|IF_SM},
    {I_MOV, 2, {REG8,REG8,0}, "\301\1\x8A\110", IF_8086},
    {I_MOV, 2, {REG16,MEMORY,0}, "\320\301\1\x8B\110", IF_8086|IF_SM},
    {I_MOV, 2, {REG16,REG16,0}, "\320\301\1\x8B\110", IF_8086},
    {I_MOV, 2, {REG32,MEMORY,0}, "\321\301\1\x8B\110", IF_386|IF_SM},
    {I_MOV, 2, {REG32,REG32,0}, "\321\301\1\x8B\110", IF_386},
    {I_MOV, 2, {REG8,IMMEDIATE,0}, "\10\xB0\21", IF_8086|IF_SM},
    {I_MOV, 2, {REG16,IMMEDIATE,0}, "\320\10\xB8\31", IF_8086|IF_SM},
    {I_MOV, 2, {REG32,IMMEDIATE,0}, "\321\10\xB8\41", IF_386|IF_SM},
    {I_MOV, 2, {REGMEM|BITS8,IMMEDIATE,0}, "\300\1\xC6\200\21", IF_8086|IF_SM},
    {I_MOV, 2, {REGMEM|BITS16,IMMEDIATE,0}, "\320\300\1\xC7\200\31", IF_8086|IF_SM},
    {I_MOV, 2, {REGMEM|BITS32,IMMEDIATE,0}, "\321\300\1\xC7\200\41", IF_386|IF_SM},
    {I_MOV, 2, {MEMORY,IMMEDIATE|BITS8,0}, "\300\1\xC6\200\21", IF_8086|IF_SM},
    {I_MOV, 2, {MEMORY,IMMEDIATE|BITS16,0}, "\320\300\1\xC7\200\31", IF_8086|IF_SM},
    {I_MOV, 2, {MEMORY,IMMEDIATE|BITS32,0}, "\321\300\1\xC7\200\41", IF_386|IF_SM},
    {-1}
};

static struct itemplate instrux_MOVD[] = {
    {I_MOVD, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\x6E\110", IF_PENT|IF_MMX|IF_SD},
    {I_MOVD, 2, {MMXREG,REG32,0}, "\2\x0F\x6E\110", IF_PENT|IF_MMX},
    {I_MOVD, 2, {MEMORY,MMXREG,0}, "\300\2\x0F\x7E\101", IF_PENT|IF_MMX|IF_SD},
    {I_MOVD, 2, {REG32,MMXREG,0}, "\2\x0F\x7E\101", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_MOVQ[] = {
    {I_MOVQ, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\x6F\110", IF_PENT|IF_MMX|IF_SM},
    {I_MOVQ, 2, {MMXREG,MMXREG,0}, "\2\x0F\x6F\110", IF_PENT|IF_MMX},
    {I_MOVQ, 2, {MEMORY,MMXREG,0}, "\300\2\x0F\x7F\101", IF_PENT|IF_MMX|IF_SM},
    {I_MOVQ, 2, {MMXREG,MMXREG,0}, "\2\x0F\x7F\101", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_MOVSB[] = {
    {I_MOVSB, 0, {0,0,0}, "\1\xA4", IF_8086},
    {-1}
};

static struct itemplate instrux_MOVSD[] = {
    {I_MOVSD, 0, {0,0,0}, "\321\1\xA5", IF_386},
    {-1}
};

static struct itemplate instrux_MOVSW[] = {
    {I_MOVSW, 0, {0,0,0}, "\320\1\xA5", IF_8086},
    {-1}
};

static struct itemplate instrux_MOVSX[] = {
    {I_MOVSX, 2, {REG16,MEMORY,0}, "\320\301\2\x0F\xBE\110", IF_386|IF_SB},
    {I_MOVSX, 2, {REG16,REG8,0}, "\320\301\2\x0F\xBE\110", IF_386},
    {I_MOVSX, 2, {REG32,REGMEM|BITS8,0}, "\321\301\2\x0F\xBE\110", IF_386},
    {I_MOVSX, 2, {REG32,REGMEM|BITS16,0}, "\321\301\2\x0F\xBF\110", IF_386},
    {-1}
};

static struct itemplate instrux_MOVZX[] = {
    {I_MOVZX, 2, {REG16,MEMORY,0}, "\320\301\2\x0F\xB6\110", IF_386|IF_SB},
    {I_MOVZX, 2, {REG16,REG8,0}, "\320\301\2\x0F\xB6\110", IF_386},
    {I_MOVZX, 2, {REG32,REGMEM|BITS8,0}, "\321\301\2\x0F\xB6\110", IF_386},
    {I_MOVZX, 2, {REG32,REGMEM|BITS16,0}, "\321\301\2\x0F\xB7\110", IF_386},
    {-1}
};

static struct itemplate instrux_MUL[] = {
    {I_MUL, 1, {REGMEM|BITS8,0,0}, "\300\1\xF6\204", IF_8086},
    {I_MUL, 1, {REGMEM|BITS16,0,0}, "\320\300\1\xF7\204", IF_8086},
    {I_MUL, 1, {REGMEM|BITS32,0,0}, "\321\300\1\xF7\204", IF_386},
    {-1}
};

static struct itemplate instrux_NEG[] = {
    {I_NEG, 1, {REGMEM|BITS8,0,0}, "\300\1\xF6\203", IF_8086},
    {I_NEG, 1, {REGMEM|BITS16,0,0}, "\320\300\1\xF7\203", IF_8086},
    {I_NEG, 1, {REGMEM|BITS32,0,0}, "\321\300\1\xF7\203", IF_386},
    {-1}
};

static struct itemplate instrux_NOP[] = {
    {I_NOP, 0, {0,0,0}, "\1\x90", IF_8086},
    {-1}
};

static struct itemplate instrux_NOT[] = {
    {I_NOT, 1, {REGMEM|BITS8,0,0}, "\300\1\xF6\202", IF_8086},
    {I_NOT, 1, {REGMEM|BITS16,0,0}, "\320\300\1\xF7\202", IF_8086},
    {I_NOT, 1, {REGMEM|BITS32,0,0}, "\321\300\1\xF7\202", IF_386},
    {-1}
};

static struct itemplate instrux_OR[] = {
    {I_OR, 2, {MEMORY,REG8,0}, "\300\1\x08\101", IF_8086|IF_SM},
    {I_OR, 2, {REG8,REG8,0}, "\300\1\x08\101", IF_8086},
    {I_OR, 2, {MEMORY,REG16,0}, "\320\300\1\x09\101", IF_8086|IF_SM},
    {I_OR, 2, {REG16,REG16,0}, "\320\300\1\x09\101", IF_8086},
    {I_OR, 2, {MEMORY,REG32,0}, "\321\300\1\x09\101", IF_386|IF_SM},
    {I_OR, 2, {REG32,REG32,0}, "\321\300\1\x09\101", IF_386},
    {I_OR, 2, {REG8,MEMORY,0}, "\301\1\x0A\110", IF_8086|IF_SM},
    {I_OR, 2, {REG8,REG8,0}, "\301\1\x0A\110", IF_8086},
    {I_OR, 2, {REG16,MEMORY,0}, "\320\301\1\x0B\110", IF_8086|IF_SM},
    {I_OR, 2, {REG16,REG16,0}, "\320\301\1\x0B\110", IF_8086},
    {I_OR, 2, {REG32,MEMORY,0}, "\321\301\1\x0B\110", IF_386|IF_SM},
    {I_OR, 2, {REG32,REG32,0}, "\321\301\1\x0B\110", IF_386},
    {I_OR, 2, {REGMEM|BITS16,IMMEDIATE|BITS8,0}, "\320\300\1\x83\201\15", IF_8086},
    {I_OR, 2, {REGMEM|BITS32,IMMEDIATE|BITS8,0}, "\321\300\1\x83\201\15", IF_386},
    {I_OR, 2, {REG_AL,IMMEDIATE,0}, "\1\x0C\21", IF_8086|IF_SM},
    {I_OR, 2, {REG_AX,IMMEDIATE,0}, "\320\1\x0D\31", IF_8086|IF_SM},
    {I_OR, 2, {REG_EAX,IMMEDIATE,0}, "\321\1\x0D\41", IF_386|IF_SM},
    {I_OR, 2, {REGMEM|BITS8,IMMEDIATE,0}, "\300\1\x80\201\21", IF_8086|IF_SM},
    {I_OR, 2, {REGMEM|BITS16,IMMEDIATE,0}, "\320\300\1\x81\201\31", IF_8086|IF_SM},
    {I_OR, 2, {REGMEM|BITS32,IMMEDIATE,0}, "\321\300\1\x81\201\41", IF_386|IF_SM},
    {I_OR, 2, {MEMORY,IMMEDIATE|BITS8,0}, "\300\1\x80\201\21", IF_8086|IF_SM},
    {I_OR, 2, {MEMORY,IMMEDIATE|BITS16,0}, "\320\300\1\x81\201\31", IF_8086|IF_SM},
    {I_OR, 2, {MEMORY,IMMEDIATE|BITS32,0}, "\321\300\1\x81\201\41", IF_386|IF_SM},
    {-1}
};

static struct itemplate instrux_OUT[] = {
    {I_OUT, 2, {IMMEDIATE,REG_AL,0}, "\1\xE6\24", IF_8086},
    {I_OUT, 2, {IMMEDIATE,REG_AX,0}, "\320\1\xE7\24", IF_8086},
    {I_OUT, 2, {IMMEDIATE,REG_EAX,0}, "\321\1\xE7\24", IF_386},
    {I_OUT, 2, {REG_DX,REG_AL,0}, "\1\xEE", IF_8086},
    {I_OUT, 2, {REG_DX,REG_AX,0}, "\320\1\xEF", IF_8086},
    {I_OUT, 2, {REG_DX,REG_EAX,0}, "\321\1\xEF", IF_386},
    {-1}
};

static struct itemplate instrux_OUTSB[] = {
    {I_OUTSB, 0, {0,0,0}, "\1\x6E", IF_186},
    {-1}
};

static struct itemplate instrux_OUTSD[] = {
    {I_OUTSD, 0, {0,0,0}, "\321\1\x6F", IF_386},
    {-1}
};

static struct itemplate instrux_OUTSW[] = {
    {I_OUTSW, 0, {0,0,0}, "\320\1\x6F", IF_186},
    {-1}
};

static struct itemplate instrux_PACKSSDW[] = {
    {I_PACKSSDW, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\x6B\110", IF_PENT|IF_MMX|IF_SM},
    {I_PACKSSDW, 2, {MMXREG,MMXREG,0}, "\2\x0F\x6B\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PACKSSWB[] = {
    {I_PACKSSWB, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\x63\110", IF_PENT|IF_MMX|IF_SM},
    {I_PACKSSWB, 2, {MMXREG,MMXREG,0}, "\2\x0F\x63\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PACKUSWB[] = {
    {I_PACKUSWB, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\x67\110", IF_PENT|IF_MMX|IF_SM},
    {I_PACKUSWB, 2, {MMXREG,MMXREG,0}, "\2\x0F\x67\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PADDB[] = {
    {I_PADDB, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\xFC\110", IF_PENT|IF_MMX|IF_SM},
    {I_PADDB, 2, {MMXREG,MMXREG,0}, "\2\x0F\xFC\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PADDD[] = {
    {I_PADDD, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\xFE\110", IF_PENT|IF_MMX|IF_SM},
    {I_PADDD, 2, {MMXREG,MMXREG,0}, "\2\x0F\xFE\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PADDSB[] = {
    {I_PADDSB, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\xEC\110", IF_PENT|IF_MMX|IF_SM},
    {I_PADDSB, 2, {MMXREG,MMXREG,0}, "\2\x0F\xEC\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PADDSIW[] = {
    {I_PADDSIW, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\x51\110", IF_PENT|IF_MMX|IF_SM|IF_CYRIX},
    {I_PADDSIW, 2, {MMXREG,MMXREG,0}, "\2\x0F\x51\110", IF_PENT|IF_MMX|IF_CYRIX},
    {-1}
};

static struct itemplate instrux_PADDSW[] = {
    {I_PADDSW, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\xED\110", IF_PENT|IF_MMX|IF_SM},
    {I_PADDSW, 2, {MMXREG,MMXREG,0}, "\2\x0F\xED\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PADDUSB[] = {
    {I_PADDUSB, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\xDC\110", IF_PENT|IF_MMX|IF_SM},
    {I_PADDUSB, 2, {MMXREG,MMXREG,0}, "\2\x0F\xDC\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PADDUSW[] = {
    {I_PADDUSW, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\xDD\110", IF_PENT|IF_MMX|IF_SM},
    {I_PADDUSW, 2, {MMXREG,MMXREG,0}, "\2\x0F\xDD\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PADDW[] = {
    {I_PADDW, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\xFD\110", IF_PENT|IF_MMX|IF_SM},
    {I_PADDW, 2, {MMXREG,MMXREG,0}, "\2\x0F\xFD\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PAND[] = {
    {I_PAND, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\xDB\110", IF_PENT|IF_MMX|IF_SM},
    {I_PAND, 2, {MMXREG,MMXREG,0}, "\2\x0F\xDB\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PANDN[] = {
    {I_PANDN, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\xDF\110", IF_PENT|IF_MMX|IF_SM},
    {I_PANDN, 2, {MMXREG,MMXREG,0}, "\2\x0F\xDF\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PAVEB[] = {
    {I_PAVEB, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\x50\110", IF_PENT|IF_MMX|IF_SM|IF_CYRIX},
    {I_PAVEB, 2, {MMXREG,MMXREG,0}, "\2\x0F\x50\110", IF_PENT|IF_MMX|IF_CYRIX},
    {-1}
};

static struct itemplate instrux_PCMPEQB[] = {
    {I_PCMPEQB, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\x74\110", IF_PENT|IF_MMX|IF_SM},
    {I_PCMPEQB, 2, {MMXREG,MMXREG,0}, "\2\x0F\x74\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PCMPEQD[] = {
    {I_PCMPEQD, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\x76\110", IF_PENT|IF_MMX|IF_SM},
    {I_PCMPEQD, 2, {MMXREG,MMXREG,0}, "\2\x0F\x76\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PCMPEQW[] = {
    {I_PCMPEQW, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\x75\110", IF_PENT|IF_MMX|IF_SM},
    {I_PCMPEQW, 2, {MMXREG,MMXREG,0}, "\2\x0F\x75\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PCMPGTB[] = {
    {I_PCMPGTB, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\x64\110", IF_PENT|IF_MMX|IF_SM},
    {I_PCMPGTB, 2, {MMXREG,MMXREG,0}, "\2\x0F\x64\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PCMPGTD[] = {
    {I_PCMPGTD, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\x66\110", IF_PENT|IF_MMX|IF_SM},
    {I_PCMPGTD, 2, {MMXREG,MMXREG,0}, "\2\x0F\x66\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PCMPGTW[] = {
    {I_PCMPGTW, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\x65\110", IF_PENT|IF_MMX|IF_SM},
    {I_PCMPGTW, 2, {MMXREG,MMXREG,0}, "\2\x0F\x65\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PDISTIB[] = {
    {I_PDISTIB, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\x54\110", IF_PENT|IF_MMX|IF_SM|IF_CYRIX},
    {-1}
};

static struct itemplate instrux_PMACHRIW[] = {
    {I_PMACHRIW, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\x5E\110", IF_PENT|IF_MMX|IF_SM|IF_CYRIX},
    {-1}
};

static struct itemplate instrux_PMADDWD[] = {
    {I_PMADDWD, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\xF5\110", IF_PENT|IF_MMX|IF_SM},
    {I_PMADDWD, 2, {MMXREG,MMXREG,0}, "\2\x0F\xF5\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PMAGW[] = {
    {I_PMAGW, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\x52\110", IF_PENT|IF_MMX|IF_SM|IF_CYRIX},
    {I_PMAGW, 2, {MMXREG,MMXREG,0}, "\2\x0F\x52\110", IF_PENT|IF_MMX|IF_CYRIX},
    {-1}
};

static struct itemplate instrux_PMULHRW[] = {
    {I_PMULHRW, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\x59\110", IF_PENT|IF_MMX|IF_SM|IF_CYRIX},
    {I_PMULHRW, 2, {MMXREG,MMXREG,0}, "\2\x0F\x59\110", IF_PENT|IF_MMX|IF_CYRIX},
    {-1}
};

static struct itemplate instrux_PMULHRIW[] = {
    {I_PMULHRIW, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\x5D\110", IF_PENT|IF_MMX|IF_SM|IF_CYRIX},
    {I_PMULHRIW, 2, {MMXREG,MMXREG,0}, "\2\x0F\x5D\110", IF_PENT|IF_MMX|IF_CYRIX},
    {-1}
};

static struct itemplate instrux_PMULHW[] = {
    {I_PMULHW, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\xE5\110", IF_PENT|IF_MMX|IF_SM},
    {I_PMULHW, 2, {MMXREG,MMXREG,0}, "\2\x0F\xE5\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PMULLW[] = {
    {I_PMULLW, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\xD5\110", IF_PENT|IF_MMX|IF_SM},
    {I_PMULLW, 2, {MMXREG,MMXREG,0}, "\2\x0F\xD5\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PMVGEZB[] = {
    {I_PMVGEZB, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\x5C\110", IF_PENT|IF_MMX|IF_SM|IF_CYRIX},
    {-1}
};

static struct itemplate instrux_PMVLZB[] = {
    {I_PMVLZB, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\x5B\110", IF_PENT|IF_MMX|IF_SM|IF_CYRIX},
    {-1}
};

static struct itemplate instrux_PMVNZB[] = {
    {I_PMVNZB, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\x5A\110", IF_PENT|IF_MMX|IF_SM|IF_CYRIX},
    {-1}
};

static struct itemplate instrux_PMVZB[] = {
    {I_PMVZB, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\x58\110", IF_PENT|IF_MMX|IF_SM|IF_CYRIX},
    {-1}
};

static struct itemplate instrux_POP[] = {
    {I_POP, 1, {REG16,0,0}, "\320\10\x58", IF_8086},
    {I_POP, 1, {REG32,0,0}, "\321\10\x58", IF_386},
    {I_POP, 1, {REGMEM|BITS16,0,0}, "\320\300\1\x8F\200", IF_8086},
    {I_POP, 1, {REGMEM|BITS32,0,0}, "\321\300\1\x8F\200", IF_386},
    {I_POP, 1, {REG_CS,0,0}, "\1\x0F", IF_8086|IF_UNDOC},
    {I_POP, 1, {REG_DESS,0,0}, "\4", IF_8086},
    {I_POP, 1, {REG_FSGS,0,0}, "\1\x0F\5", IF_386},
    {-1}
};

static struct itemplate instrux_POPA[] = {
    {I_POPA, 0, {0,0,0}, "\322\1\x61", IF_186},
    {-1}
};

static struct itemplate instrux_POPAD[] = {
    {I_POPAD, 0, {0,0,0}, "\321\1\x61", IF_386},
    {-1}
};

static struct itemplate instrux_POPAW[] = {
    {I_POPAW, 0, {0,0,0}, "\320\1\x61", IF_186},
    {-1}
};

static struct itemplate instrux_POPF[] = {
    {I_POPF, 0, {0,0,0}, "\322\1\x9D", IF_186},
    {-1}
};

static struct itemplate instrux_POPFD[] = {
    {I_POPFD, 0, {0,0,0}, "\321\1\x9D", IF_386},
    {-1}
};

static struct itemplate instrux_POPFW[] = {
    {I_POPFW, 0, {0,0,0}, "\320\1\x9D", IF_186},
    {-1}
};

static struct itemplate instrux_POR[] = {
    {I_POR, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\xEB\110", IF_PENT|IF_MMX|IF_SM},
    {I_POR, 2, {MMXREG,MMXREG,0}, "\2\x0F\xEB\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PSLLD[] = {
    {I_PSLLD, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\xF2\110", IF_PENT|IF_MMX|IF_SM},
    {I_PSLLD, 2, {MMXREG,MMXREG,0}, "\2\x0F\xF2\110", IF_PENT|IF_MMX},
    {I_PSLLD, 2, {MMXREG,IMMEDIATE,0}, "\2\x0F\x72\206\25", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PSLLQ[] = {
    {I_PSLLQ, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\xF3\110", IF_PENT|IF_MMX|IF_SM},
    {I_PSLLQ, 2, {MMXREG,MMXREG,0}, "\2\x0F\xF3\110", IF_PENT|IF_MMX},
    {I_PSLLQ, 2, {MMXREG,IMMEDIATE,0}, "\2\x0F\x73\206\25", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PSLLW[] = {
    {I_PSLLW, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\xF1\110", IF_PENT|IF_MMX|IF_SM},
    {I_PSLLW, 2, {MMXREG,MMXREG,0}, "\2\x0F\xF1\110", IF_PENT|IF_MMX},
    {I_PSLLW, 2, {MMXREG,IMMEDIATE,0}, "\2\x0F\x71\206\25", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PSRAD[] = {
    {I_PSRAD, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\xE2\110", IF_PENT|IF_MMX|IF_SM},
    {I_PSRAD, 2, {MMXREG,MMXREG,0}, "\2\x0F\xE2\110", IF_PENT|IF_MMX},
    {I_PSRAD, 2, {MMXREG,IMMEDIATE,0}, "\2\x0F\x72\204\25", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PSRAW[] = {
    {I_PSRAW, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\xE1\110", IF_PENT|IF_MMX|IF_SM},
    {I_PSRAW, 2, {MMXREG,MMXREG,0}, "\2\x0F\xE1\110", IF_PENT|IF_MMX},
    {I_PSRAW, 2, {MMXREG,IMMEDIATE,0}, "\2\x0F\x71\204\25", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PSRLD[] = {
    {I_PSRLD, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\xD2\110", IF_PENT|IF_MMX|IF_SM},
    {I_PSRLD, 2, {MMXREG,MMXREG,0}, "\2\x0F\xD2\110", IF_PENT|IF_MMX},
    {I_PSRLD, 2, {MMXREG,IMMEDIATE,0}, "\2\x0F\x72\202\25", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PSRLQ[] = {
    {I_PSRLQ, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\xD3\110", IF_PENT|IF_MMX|IF_SM},
    {I_PSRLQ, 2, {MMXREG,MMXREG,0}, "\2\x0F\xD3\110", IF_PENT|IF_MMX},
    {I_PSRLQ, 2, {MMXREG,IMMEDIATE,0}, "\2\x0F\x73\202\25", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PSRLW[] = {
    {I_PSRLW, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\xD1\110", IF_PENT|IF_MMX|IF_SM},
    {I_PSRLW, 2, {MMXREG,MMXREG,0}, "\2\x0F\xD1\110", IF_PENT|IF_MMX},
    {I_PSRLW, 2, {MMXREG,IMMEDIATE,0}, "\2\x0F\x71\202\25", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PSUBB[] = {
    {I_PSUBB, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\xF8\110", IF_PENT|IF_MMX|IF_SM},
    {I_PSUBB, 2, {MMXREG,MMXREG,0}, "\2\x0F\xF8\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PSUBD[] = {
    {I_PSUBD, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\xFA\110", IF_PENT|IF_MMX|IF_SM},
    {I_PSUBD, 2, {MMXREG,MMXREG,0}, "\2\x0F\xFA\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PSUBSB[] = {
    {I_PSUBSB, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\xE8\110", IF_PENT|IF_MMX|IF_SM},
    {I_PSUBSB, 2, {MMXREG,MMXREG,0}, "\2\x0F\xE8\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PSUBSIW[] = {
    {I_PSUBSIW, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\x55\110", IF_PENT|IF_MMX|IF_SM|IF_CYRIX},
    {I_PSUBSIW, 2, {MMXREG,MMXREG,0}, "\2\x0F\x55\110", IF_PENT|IF_MMX|IF_CYRIX},
    {-1}
};

static struct itemplate instrux_PSUBSW[] = {
    {I_PSUBSW, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\xE9\110", IF_PENT|IF_MMX|IF_SM},
    {I_PSUBSW, 2, {MMXREG,MMXREG,0}, "\2\x0F\xE9\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PSUBUSB[] = {
    {I_PSUBUSB, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\xD8\110", IF_PENT|IF_MMX|IF_SM},
    {I_PSUBUSB, 2, {MMXREG,MMXREG,0}, "\2\x0F\xD8\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PSUBUSW[] = {
    {I_PSUBUSW, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\xD9\110", IF_PENT|IF_MMX|IF_SM},
    {I_PSUBUSW, 2, {MMXREG,MMXREG,0}, "\2\x0F\xD9\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PSUBW[] = {
    {I_PSUBW, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\xF9\110", IF_PENT|IF_MMX|IF_SM},
    {I_PSUBW, 2, {MMXREG,MMXREG,0}, "\2\x0F\xF9\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PUNPCKHBW[] = {
    {I_PUNPCKHBW, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\x68\110", IF_PENT|IF_MMX|IF_SM},
    {I_PUNPCKHBW, 2, {MMXREG,MMXREG,0}, "\2\x0F\x68\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PUNPCKHDQ[] = {
    {I_PUNPCKHDQ, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\x6A\110", IF_PENT|IF_MMX|IF_SM},
    {I_PUNPCKHDQ, 2, {MMXREG,MMXREG,0}, "\2\x0F\x6A\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PUNPCKHWD[] = {
    {I_PUNPCKHWD, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\x69\110", IF_PENT|IF_MMX|IF_SM},
    {I_PUNPCKHWD, 2, {MMXREG,MMXREG,0}, "\2\x0F\x69\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PUNPCKLBW[] = {
    {I_PUNPCKLBW, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\x60\110", IF_PENT|IF_MMX|IF_SM},
    {I_PUNPCKLBW, 2, {MMXREG,MMXREG,0}, "\2\x0F\x60\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PUNPCKLDQ[] = {
    {I_PUNPCKLDQ, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\x62\110", IF_PENT|IF_MMX|IF_SM},
    {I_PUNPCKLDQ, 2, {MMXREG,MMXREG,0}, "\2\x0F\x62\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PUNPCKLWD[] = {
    {I_PUNPCKLWD, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\x61\110", IF_PENT|IF_MMX|IF_SM},
    {I_PUNPCKLWD, 2, {MMXREG,MMXREG,0}, "\2\x0F\x61\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_PUSH[] = {
    {I_PUSH, 1, {REG16,0,0}, "\320\10\x50", IF_8086},
    {I_PUSH, 1, {REG32,0,0}, "\321\10\x50", IF_386},
    {I_PUSH, 1, {REGMEM|BITS16,0,0}, "\320\300\1\xFF\206", IF_8086},
    {I_PUSH, 1, {REGMEM|BITS32,0,0}, "\321\300\1\xFF\206", IF_386},
    {I_PUSH, 1, {REG_FSGS,0,0}, "\1\x0F\7", IF_386},
    {I_PUSH, 1, {REG_SREG,0,0}, "\6", IF_8086},
    {I_PUSH, 1, {IMMEDIATE|BITS8,0,0}, "\1\x6A\14", IF_286},
    {I_PUSH, 1, {IMMEDIATE|BITS16,0,0}, "\320\1\x68\30", IF_286},
    {I_PUSH, 1, {IMMEDIATE|BITS32,0,0}, "\321\1\x68\40", IF_386},
    {-1}
};

static struct itemplate instrux_PUSHA[] = {
    {I_PUSHA, 0, {0,0,0}, "\322\1\x60", IF_186},
    {-1}
};

static struct itemplate instrux_PUSHAD[] = {
    {I_PUSHAD, 0, {0,0,0}, "\321\1\x60", IF_386},
    {-1}
};

static struct itemplate instrux_PUSHAW[] = {
    {I_PUSHAW, 0, {0,0,0}, "\320\1\x60", IF_186},
    {-1}
};

static struct itemplate instrux_PUSHF[] = {
    {I_PUSHF, 0, {0,0,0}, "\322\1\x9C", IF_186},
    {-1}
};

static struct itemplate instrux_PUSHFD[] = {
    {I_PUSHFD, 0, {0,0,0}, "\321\1\x9C", IF_386},
    {-1}
};

static struct itemplate instrux_PUSHFW[] = {
    {I_PUSHFW, 0, {0,0,0}, "\320\1\x9C", IF_186},
    {-1}
};

static struct itemplate instrux_PXOR[] = {
    {I_PXOR, 2, {MMXREG,MEMORY,0}, "\301\2\x0F\xEF\110", IF_PENT|IF_MMX|IF_SM},
    {I_PXOR, 2, {MMXREG,MMXREG,0}, "\2\x0F\xEF\110", IF_PENT|IF_MMX},
    {-1}
};

static struct itemplate instrux_RCL[] = {
    {I_RCL, 2, {REGMEM|BITS8,UNITY,0}, "\300\1\xD0\202", IF_8086},
    {I_RCL, 2, {REGMEM|BITS8,REG_CL,0}, "\300\1\xD2\202", IF_8086},
    {I_RCL, 2, {REGMEM|BITS8,IMMEDIATE,0}, "\300\1\xC0\202\25", IF_286|IF_SB},
    {I_RCL, 2, {REGMEM|BITS16,UNITY,0}, "\320\300\1\xD1\202", IF_8086},
    {I_RCL, 2, {REGMEM|BITS16,REG_CL,0}, "\320\300\1\xD3\202", IF_8086},
    {I_RCL, 2, {REGMEM|BITS16,IMMEDIATE,0}, "\320\300\1\xC1\202\25", IF_286|IF_SB},
    {I_RCL, 2, {REGMEM|BITS32,UNITY,0}, "\321\300\1\xD1\202", IF_386},
    {I_RCL, 2, {REGMEM|BITS32,REG_CL,0}, "\321\300\1\xD3\202", IF_386},
    {I_RCL, 2, {REGMEM|BITS32,IMMEDIATE,0}, "\321\300\1\xC1\202\25", IF_386|IF_SB},
    {-1}
};

static struct itemplate instrux_RCR[] = {
    {I_RCR, 2, {REGMEM|BITS8,UNITY,0}, "\300\1\xD0\203", IF_8086},
    {I_RCR, 2, {REGMEM|BITS8,REG_CL,0}, "\300\1\xD2\203", IF_8086},
    {I_RCR, 2, {REGMEM|BITS8,IMMEDIATE,0}, "\300\1\xC0\203\25", IF_286|IF_SB},
    {I_RCR, 2, {REGMEM|BITS16,UNITY,0}, "\320\300\1\xD1\203", IF_8086},
    {I_RCR, 2, {REGMEM|BITS16,REG_CL,0}, "\320\300\1\xD3\203", IF_8086},
    {I_RCR, 2, {REGMEM|BITS16,IMMEDIATE,0}, "\320\300\1\xC1\203\25", IF_286|IF_SB},
    {I_RCR, 2, {REGMEM|BITS32,UNITY,0}, "\321\300\1\xD1\203", IF_386},
    {I_RCR, 2, {REGMEM|BITS32,REG_CL,0}, "\321\300\1\xD3\203", IF_386},
    {I_RCR, 2, {REGMEM|BITS32,IMMEDIATE,0}, "\321\300\1\xC1\203\25", IF_386|IF_SB},
    {-1}
};

static struct itemplate instrux_RDMSR[] = {
    {I_RDMSR, 0, {0,0,0}, "\2\x0F\x32", IF_PENT},
    {-1}
};

static struct itemplate instrux_RDPMC[] = {
    {I_RDPMC, 0, {0,0,0}, "\2\x0F\x33", IF_P6},
    {-1}
};

static struct itemplate instrux_RDTSC[] = {
    {I_RDTSC, 0, {0,0,0}, "\2\x0F\x31", IF_PENT},
    {-1}
};

static struct itemplate instrux_RESB[] = {
    {I_RESB, 1, {IMMEDIATE,0,0}, "\340", IF_8086},
    {-1}
};

static struct itemplate instrux_RESD[] = {
    {-1}
};

static struct itemplate instrux_RESQ[] = {
    {-1}
};

static struct itemplate instrux_REST[] = {
    {-1}
};

static struct itemplate instrux_RESW[] = {
    {-1}
};

static struct itemplate instrux_RET[] = {
    {I_RET, 0, {0,0,0}, "\1\xC3", IF_8086},
    {I_RET, 1, {IMMEDIATE,0,0}, "\1\xC2\30", IF_8086},
    {-1}
};

static struct itemplate instrux_RETF[] = {
    {I_RETF, 0, {0,0,0}, "\1\xCB", IF_8086},
    {I_RETF, 1, {IMMEDIATE,0,0}, "\1\xCA\30", IF_8086},
    {-1}
};

static struct itemplate instrux_RETN[] = {
    {I_RETN, 0, {0,0,0}, "\1\xC3", IF_8086},
    {I_RETN, 1, {IMMEDIATE,0,0}, "\1\xC2\30", IF_8086},
    {-1}
};

static struct itemplate instrux_ROL[] = {
    {I_ROL, 2, {REGMEM|BITS8,UNITY,0}, "\300\1\xD0\200", IF_8086},
    {I_ROL, 2, {REGMEM|BITS8,REG_CL,0}, "\300\1\xD2\200", IF_8086},
    {I_ROL, 2, {REGMEM|BITS8,IMMEDIATE,0}, "\300\1\xC0\200\25", IF_286|IF_SB},
    {I_ROL, 2, {REGMEM|BITS16,UNITY,0}, "\320\300\1\xD1\200", IF_8086},
    {I_ROL, 2, {REGMEM|BITS16,REG_CL,0}, "\320\300\1\xD3\200", IF_8086},
    {I_ROL, 2, {REGMEM|BITS16,IMMEDIATE,0}, "\320\300\1\xC1\200\25", IF_286|IF_SB},
    {I_ROL, 2, {REGMEM|BITS32,UNITY,0}, "\321\300\1\xD1\200", IF_386},
    {I_ROL, 2, {REGMEM|BITS32,REG_CL,0}, "\321\300\1\xD3\200", IF_386},
    {I_ROL, 2, {REGMEM|BITS32,IMMEDIATE,0}, "\321\300\1\xC1\200\25", IF_386|IF_SB},
    {-1}
};

static struct itemplate instrux_ROR[] = {
    {I_ROR, 2, {REGMEM|BITS8,UNITY,0}, "\300\1\xD0\201", IF_8086},
    {I_ROR, 2, {REGMEM|BITS8,REG_CL,0}, "\300\1\xD2\201", IF_8086},
    {I_ROR, 2, {REGMEM|BITS8,IMMEDIATE,0}, "\300\1\xC0\201\25", IF_286|IF_SB},
    {I_ROR, 2, {REGMEM|BITS16,UNITY,0}, "\320\300\1\xD1\201", IF_8086},
    {I_ROR, 2, {REGMEM|BITS16,REG_CL,0}, "\320\300\1\xD3\201", IF_8086},
    {I_ROR, 2, {REGMEM|BITS16,IMMEDIATE,0}, "\320\300\1\xC1\201\25", IF_286|IF_SB},
    {I_ROR, 2, {REGMEM|BITS32,UNITY,0}, "\321\300\1\xD1\201", IF_386},
    {I_ROR, 2, {REGMEM|BITS32,REG_CL,0}, "\321\300\1\xD3\201", IF_386},
    {I_ROR, 2, {REGMEM|BITS32,IMMEDIATE,0}, "\321\300\1\xC1\201\25", IF_386|IF_SB},
    {-1}
};

static struct itemplate instrux_RSM[] = {
    {I_RSM, 0, {0,0,0}, "\2\x0F\xAA", IF_PENT},
    {-1}
};

static struct itemplate instrux_SAHF[] = {
    {I_SAHF, 0, {0,0,0}, "\1\x9E", IF_8086},
    {-1}
};

static struct itemplate instrux_SAL[] = {
    {I_SAL, 2, {REGMEM|BITS8,UNITY,0}, "\300\1\xD0\204", IF_8086},
    {I_SAL, 2, {REGMEM|BITS8,REG_CL,0}, "\300\1\xD2\204", IF_8086},
    {I_SAL, 2, {REGMEM|BITS8,IMMEDIATE,0}, "\300\1\xC0\204\25", IF_286|IF_SB},
    {I_SAL, 2, {REGMEM|BITS16,UNITY,0}, "\320\300\1\xD1\204", IF_8086},
    {I_SAL, 2, {REGMEM|BITS16,REG_CL,0}, "\320\300\1\xD3\204", IF_8086},
    {I_SAL, 2, {REGMEM|BITS16,IMMEDIATE,0}, "\320\300\1\xC1\204\25", IF_286|IF_SB},
    {I_SAL, 2, {REGMEM|BITS32,UNITY,0}, "\321\300\1\xD1\204", IF_386},
    {I_SAL, 2, {REGMEM|BITS32,REG_CL,0}, "\321\300\1\xD3\204", IF_386},
    {I_SAL, 2, {REGMEM|BITS32,IMMEDIATE,0}, "\321\300\1\xC1\204\25", IF_386|IF_SB},
    {-1}
};

static struct itemplate instrux_SALC[] = {
    {I_SALC, 0, {0,0,0}, "\1\xD6", IF_8086|IF_UNDOC},
    {-1}
};

static struct itemplate instrux_SAR[] = {
    {I_SAR, 2, {REGMEM|BITS8,UNITY,0}, "\300\1\xD0\207", IF_8086},
    {I_SAR, 2, {REGMEM|BITS8,REG_CL,0}, "\300\1\xD2\207", IF_8086},
    {I_SAR, 2, {REGMEM|BITS8,IMMEDIATE,0}, "\300\1\xC0\207\25", IF_286|IF_SB},
    {I_SAR, 2, {REGMEM|BITS16,UNITY,0}, "\320\300\1\xD1\207", IF_8086},
    {I_SAR, 2, {REGMEM|BITS16,REG_CL,0}, "\320\300\1\xD3\207", IF_8086},
    {I_SAR, 2, {REGMEM|BITS16,IMMEDIATE,0}, "\320\300\1\xC1\207\25", IF_286|IF_SB},
    {I_SAR, 2, {REGMEM|BITS32,UNITY,0}, "\321\300\1\xD1\207", IF_386},
    {I_SAR, 2, {REGMEM|BITS32,REG_CL,0}, "\321\300\1\xD3\207", IF_386},
    {I_SAR, 2, {REGMEM|BITS32,IMMEDIATE,0}, "\321\300\1\xC1\207\25", IF_386|IF_SB},
    {-1}
};

static struct itemplate instrux_SBB[] = {
    {I_SBB, 2, {MEMORY,REG8,0}, "\300\1\x18\101", IF_8086|IF_SM},
    {I_SBB, 2, {REG8,REG8,0}, "\300\1\x18\101", IF_8086},
    {I_SBB, 2, {MEMORY,REG16,0}, "\320\300\1\x19\101", IF_8086|IF_SM},
    {I_SBB, 2, {REG16,REG16,0}, "\320\300\1\x19\101", IF_8086},
    {I_SBB, 2, {MEMORY,REG32,0}, "\321\300\1\x19\101", IF_386|IF_SM},
    {I_SBB, 2, {REG32,REG32,0}, "\321\300\1\x19\101", IF_386},
    {I_SBB, 2, {REG8,MEMORY,0}, "\301\1\x1A\110", IF_8086|IF_SM},
    {I_SBB, 2, {REG8,REG8,0}, "\301\1\x1A\110", IF_8086},
    {I_SBB, 2, {REG16,MEMORY,0}, "\320\301\1\x1B\110", IF_8086|IF_SM},
    {I_SBB, 2, {REG16,REG16,0}, "\320\301\1\x1B\110", IF_8086},
    {I_SBB, 2, {REG32,MEMORY,0}, "\321\301\1\x1B\110", IF_386|IF_SM},
    {I_SBB, 2, {REG32,REG32,0}, "\321\301\1\x1B\110", IF_386},
    {I_SBB, 2, {REGMEM|BITS16,IMMEDIATE|BITS8,0}, "\320\300\1\x83\203\15", IF_8086},
    {I_SBB, 2, {REGMEM|BITS32,IMMEDIATE|BITS8,0}, "\321\300\1\x83\203\15", IF_8086},
    {I_SBB, 2, {REG_AL,IMMEDIATE,0}, "\1\x1C\21", IF_8086|IF_SM},
    {I_SBB, 2, {REG_AX,IMMEDIATE,0}, "\320\1\x1D\31", IF_8086|IF_SM},
    {I_SBB, 2, {REG_EAX,IMMEDIATE,0}, "\321\1\x1D\41", IF_386|IF_SM},
    {I_SBB, 2, {REGMEM|BITS8,IMMEDIATE,0}, "\300\1\x80\203\21", IF_8086|IF_SM},
    {I_SBB, 2, {REGMEM|BITS16,IMMEDIATE,0}, "\320\300\1\x81\203\31", IF_8086|IF_SM},
    {I_SBB, 2, {REGMEM|BITS32,IMMEDIATE,0}, "\321\300\1\x81\203\41", IF_386|IF_SM},
    {I_SBB, 2, {MEMORY,IMMEDIATE|BITS8,0}, "\300\1\x80\203\21", IF_8086|IF_SM},
    {I_SBB, 2, {MEMORY,IMMEDIATE|BITS16,0}, "\320\300\1\x81\203\31", IF_8086|IF_SM},
    {I_SBB, 2, {MEMORY,IMMEDIATE|BITS32,0}, "\321\300\1\x81\203\41", IF_386|IF_SM},
    {-1}
};

static struct itemplate instrux_SCASB[] = {
    {I_SCASB, 0, {0,0,0}, "\1\xAE", IF_8086},
    {-1}
};

static struct itemplate instrux_SCASD[] = {
    {I_SCASD, 0, {0,0,0}, "\321\1\xAF", IF_386},
    {-1}
};

static struct itemplate instrux_SCASW[] = {
    {I_SCASW, 0, {0,0,0}, "\320\1\xAF", IF_8086},
    {-1}
};

static struct itemplate instrux_SGDT[] = {
    {I_SGDT, 1, {MEMORY,0,0}, "\300\2\x0F\x01\200", IF_286|IF_PRIV},
    {-1}
};

static struct itemplate instrux_SHL[] = {
    {I_SHL, 2, {REGMEM|BITS8,UNITY,0}, "\300\1\xD0\204", IF_8086},
    {I_SHL, 2, {REGMEM|BITS8,REG_CL,0}, "\300\1\xD2\204", IF_8086},
    {I_SHL, 2, {REGMEM|BITS8,IMMEDIATE,0}, "\300\1\xC0\204\25", IF_286|IF_SB},
    {I_SHL, 2, {REGMEM|BITS16,UNITY,0}, "\320\300\1\xD1\204", IF_8086},
    {I_SHL, 2, {REGMEM|BITS16,REG_CL,0}, "\320\300\1\xD3\204", IF_8086},
    {I_SHL, 2, {REGMEM|BITS16,IMMEDIATE,0}, "\320\300\1\xC1\204\25", IF_286|IF_SB},
    {I_SHL, 2, {REGMEM|BITS32,UNITY,0}, "\321\300\1\xD1\204", IF_386},
    {I_SHL, 2, {REGMEM|BITS32,REG_CL,0}, "\321\300\1\xD3\204", IF_386},
    {I_SHL, 2, {REGMEM|BITS32,IMMEDIATE,0}, "\321\300\1\xC1\204\25", IF_386|IF_SB},
    {-1}
};

static struct itemplate instrux_SHLD[] = {
    {I_SHLD, 3, {MEMORY,REG16,IMMEDIATE}, "\300\320\2\x0F\xA4\101\26", IF_386|IF_SM2},
    {I_SHLD, 3, {REG16,REG16,IMMEDIATE}, "\300\320\2\x0F\xA4\101\26", IF_386|IF_SM2},
    {I_SHLD, 3, {MEMORY,REG32,IMMEDIATE}, "\300\321\2\x0F\xA4\101\26", IF_386|IF_SM2},
    {I_SHLD, 3, {REG32,REG32,IMMEDIATE}, "\300\321\2\x0F\xA4\101\26", IF_386|IF_SM2},
    {I_SHLD, 3, {MEMORY,REG16,REG_CL}, "\300\320\2\x0F\xA5\101", IF_386|IF_SM},
    {I_SHLD, 3, {REG16,REG16,REG_CL}, "\300\320\2\x0F\xA5\101", IF_386},
    {I_SHLD, 3, {MEMORY,REG32,REG_CL}, "\300\321\2\x0F\xA5\101", IF_386|IF_SM},
    {I_SHLD, 3, {REG32,REG32,REG_CL}, "\300\321\2\x0F\xA5\101", IF_386},
    {-1}
};

static struct itemplate instrux_SHR[] = {
    {I_SHR, 2, {REGMEM|BITS8,UNITY,0}, "\300\1\xD0\205", IF_8086},
    {I_SHR, 2, {REGMEM|BITS8,REG_CL,0}, "\300\1\xD2\205", IF_8086},
    {I_SHR, 2, {REGMEM|BITS8,IMMEDIATE,0}, "\300\1\xC0\205\25", IF_286|IF_SB},
    {I_SHR, 2, {REGMEM|BITS16,UNITY,0}, "\320\300\1\xD1\205", IF_8086},
    {I_SHR, 2, {REGMEM|BITS16,REG_CL,0}, "\320\300\1\xD3\205", IF_8086},
    {I_SHR, 2, {REGMEM|BITS16,IMMEDIATE,0}, "\320\300\1\xC1\205\25", IF_286|IF_SB},
    {I_SHR, 2, {REGMEM|BITS32,UNITY,0}, "\321\300\1\xD1\205", IF_386},
    {I_SHR, 2, {REGMEM|BITS32,REG_CL,0}, "\321\300\1\xD3\205", IF_386},
    {I_SHR, 2, {REGMEM|BITS32,IMMEDIATE,0}, "\321\300\1\xC1\205\25", IF_386|IF_SB},
    {-1}
};

static struct itemplate instrux_SHRD[] = {
    {I_SHRD, 3, {MEMORY,REG16,IMMEDIATE}, "\300\320\2\x0F\xAC\101\26", IF_386|IF_SM2},
    {I_SHRD, 3, {REG16,REG16,IMMEDIATE}, "\300\320\2\x0F\xAC\101\26", IF_386|IF_SM2},
    {I_SHRD, 3, {MEMORY,REG32,IMMEDIATE}, "\300\321\2\x0F\xAC\101\26", IF_386|IF_SM2},
    {I_SHRD, 3, {REG32,REG32,IMMEDIATE}, "\300\321\2\x0F\xAC\101\26", IF_386|IF_SM2},
    {I_SHRD, 3, {MEMORY,REG16,REG_CL}, "\300\320\2\x0F\xAD\101", IF_386|IF_SM},
    {I_SHRD, 3, {REG16,REG16,REG_CL}, "\300\320\2\x0F\xAD\101", IF_386},
    {I_SHRD, 3, {MEMORY,REG32,REG_CL}, "\300\321\2\x0F\xAD\101", IF_386|IF_SM},
    {I_SHRD, 3, {REG32,REG32,REG_CL}, "\300\321\2\x0F\xAD\101", IF_386},
    {-1}
};

static struct itemplate instrux_SIDT[] = {
    {I_SIDT, 1, {MEMORY,0,0}, "\300\2\x0F\x01\201", IF_286|IF_PRIV},
    {-1}
};

static struct itemplate instrux_SLDT[] = {
    {I_SLDT, 1, {MEMORY,0,0}, "\300\1\x0F\17\200", IF_286|IF_PRIV},
    {I_SLDT, 1, {MEMORY|BITS16,0,0}, "\300\1\x0F\17\200", IF_286|IF_PRIV},
    {I_SLDT, 1, {REG16,0,0}, "\300\1\x0F\17\200", IF_286|IF_PRIV},
    {-1}
};

static struct itemplate instrux_SMI[] = {
    {I_SMI, 0, {0,0,0}, "\1\xF1", IF_386|IF_UNDOC},
    {-1}
};

static struct itemplate instrux_SMSW[] = {
    {I_SMSW, 1, {MEMORY,0,0}, "\300\2\x0F\x01\204", IF_286|IF_PRIV},
    {I_SMSW, 1, {MEMORY|BITS16,0,0}, "\300\2\x0F\x01\204", IF_286|IF_PRIV},
    {I_SMSW, 1, {REG16,0,0}, "\300\2\x0F\x01\204", IF_286|IF_PRIV},
    {-1}
};

static struct itemplate instrux_STC[] = {
    {I_STC, 0, {0,0,0}, "\1\xF9", IF_8086},
    {-1}
};

static struct itemplate instrux_STD[] = {
    {I_STD, 0, {0,0,0}, "\1\xFD", IF_8086},
    {-1}
};

static struct itemplate instrux_STI[] = {
    {I_STI, 0, {0,0,0}, "\1\xFB", IF_8086},
    {-1}
};

static struct itemplate instrux_STOSB[] = {
    {I_STOSB, 0, {0,0,0}, "\1\xAA", IF_8086},
    {-1}
};

static struct itemplate instrux_STOSD[] = {
    {I_STOSD, 0, {0,0,0}, "\321\1\xAB", IF_386},
    {-1}
};

static struct itemplate instrux_STOSW[] = {
    {I_STOSW, 0, {0,0,0}, "\320\1\xAB", IF_8086},
    {-1}
};

static struct itemplate instrux_STR[] = {
    {I_STR, 1, {MEMORY,0,0}, "\300\1\x0F\17\201", IF_286|IF_PRIV},
    {I_STR, 1, {MEMORY|BITS16,0,0}, "\300\1\x0F\17\201", IF_286|IF_PRIV},
    {I_STR, 1, {REG16,0,0}, "\300\1\x0F\17\201", IF_286|IF_PRIV},
    {-1}
};

static struct itemplate instrux_SUB[] = {
    {I_SUB, 2, {MEMORY,REG8,0}, "\300\1\x28\101", IF_8086|IF_SM},
    {I_SUB, 2, {REG8,REG8,0}, "\300\1\x28\101", IF_8086},
    {I_SUB, 2, {MEMORY,REG16,0}, "\320\300\1\x29\101", IF_8086|IF_SM},
    {I_SUB, 2, {REG16,REG16,0}, "\320\300\1\x29\101", IF_8086},
    {I_SUB, 2, {MEMORY,REG32,0}, "\321\300\1\x29\101", IF_386|IF_SM},
    {I_SUB, 2, {REG32,REG32,0}, "\321\300\1\x29\101", IF_386},
    {I_SUB, 2, {REG8,MEMORY,0}, "\301\1\x2A\110", IF_8086|IF_SM},
    {I_SUB, 2, {REG8,REG8,0}, "\301\1\x2A\110", IF_8086},
    {I_SUB, 2, {REG16,MEMORY,0}, "\320\301\1\x2B\110", IF_8086|IF_SM},
    {I_SUB, 2, {REG16,REG16,0}, "\320\301\1\x2B\110", IF_8086},
    {I_SUB, 2, {REG32,MEMORY,0}, "\321\301\1\x2B\110", IF_386|IF_SM},
    {I_SUB, 2, {REG32,REG32,0}, "\321\301\1\x2B\110", IF_386},
    {I_SUB, 2, {REGMEM|BITS16,IMMEDIATE|BITS8,0}, "\320\300\1\x83\205\15", IF_8086},
    {I_SUB, 2, {REGMEM|BITS32,IMMEDIATE|BITS8,0}, "\321\300\1\x83\205\15", IF_386},
    {I_SUB, 2, {REG_AL,IMMEDIATE,0}, "\1\x2C\21", IF_8086|IF_SM},
    {I_SUB, 2, {REG_AX,IMMEDIATE,0}, "\320\1\x2D\31", IF_8086|IF_SM},
    {I_SUB, 2, {REG_EAX,IMMEDIATE,0}, "\321\1\x2D\41", IF_386|IF_SM},
    {I_SUB, 2, {REGMEM|BITS8,IMMEDIATE,0}, "\300\1\x80\205\21", IF_8086|IF_SM},
    {I_SUB, 2, {REGMEM|BITS16,IMMEDIATE,0}, "\320\300\1\x81\205\31", IF_8086|IF_SM},
    {I_SUB, 2, {REGMEM|BITS32,IMMEDIATE,0}, "\321\300\1\x81\205\41", IF_386|IF_SM},
    {I_SUB, 2, {MEMORY,IMMEDIATE|BITS8,0}, "\300\1\x80\205\21", IF_8086|IF_SM},
    {I_SUB, 2, {MEMORY,IMMEDIATE|BITS16,0}, "\320\300\1\x81\205\31", IF_8086|IF_SM},
    {I_SUB, 2, {MEMORY,IMMEDIATE|BITS32,0}, "\321\300\1\x81\205\41", IF_386|IF_SM},
    {-1}
};

static struct itemplate instrux_TEST[] = {
    {I_TEST, 2, {MEMORY,REG8,0}, "\300\1\x84\101", IF_8086|IF_SM},
    {I_TEST, 2, {REG8,REG8,0}, "\300\1\x84\101", IF_8086},
    {I_TEST, 2, {MEMORY,REG16,0}, "\320\300\1\x85\101", IF_8086|IF_SM},
    {I_TEST, 2, {REG16,REG16,0}, "\320\300\1\x85\101", IF_8086},
    {I_TEST, 2, {MEMORY,REG32,0}, "\321\300\1\x85\101", IF_386|IF_SM},
    {I_TEST, 2, {REG32,REG32,0}, "\321\300\1\x85\101", IF_386},
    {I_TEST, 2, {REG_AL,IMMEDIATE,0}, "\1\xA8\21", IF_8086|IF_SM},
    {I_TEST, 2, {REG_AX,IMMEDIATE,0}, "\320\1\xA9\31", IF_8086|IF_SM},
    {I_TEST, 2, {REG_EAX,IMMEDIATE,0}, "\321\1\xA9\41", IF_386|IF_SM},
    {I_TEST, 2, {REGMEM|BITS8,IMMEDIATE,0}, "\300\1\xF6\200\21", IF_8086|IF_SM},
    {I_TEST, 2, {REGMEM|BITS16,IMMEDIATE,0}, "\320\300\1\xF7\200\31", IF_8086|IF_SM},
    {I_TEST, 2, {REGMEM|BITS32,IMMEDIATE,0}, "\321\300\1\xF7\200\41", IF_386|IF_SM},
    {I_TEST, 2, {MEMORY,IMMEDIATE|BITS8,0}, "\300\1\xF6\200\21", IF_8086|IF_SM},
    {I_TEST, 2, {MEMORY,IMMEDIATE|BITS16,0}, "\320\300\1\xF7\200\31", IF_8086|IF_SM},
    {I_TEST, 2, {MEMORY,IMMEDIATE|BITS32,0}, "\321\300\1\xF7\200\41", IF_386|IF_SM},
    {-1}
};

static struct itemplate instrux_UMOV[] = {
    {I_UMOV, 2, {MEMORY,REG8,0}, "\300\2\x0F\x10\101", IF_386|IF_UNDOC|IF_SM},
    {I_UMOV, 2, {REG8,REG8,0}, "\300\2\x0F\x10\101", IF_386|IF_UNDOC},
    {I_UMOV, 2, {MEMORY,REG16,0}, "\320\300\2\x0F\x11\101", IF_386|IF_UNDOC|IF_SM},
    {I_UMOV, 2, {REG16,REG16,0}, "\320\300\2\x0F\x11\101", IF_386|IF_UNDOC},
    {I_UMOV, 2, {MEMORY,REG32,0}, "\321\300\2\x0F\x11\101", IF_386|IF_UNDOC|IF_SM},
    {I_UMOV, 2, {REG32,REG32,0}, "\321\300\2\x0F\x11\101", IF_386|IF_UNDOC},
    {I_UMOV, 2, {REG8,MEMORY,0}, "\301\2\x0F\x12\110", IF_386|IF_UNDOC|IF_SM},
    {I_UMOV, 2, {REG8,REG8,0}, "\301\2\x0F\x12\110", IF_386|IF_UNDOC},
    {I_UMOV, 2, {REG16,MEMORY,0}, "\320\301\2\x0F\x13\110", IF_386|IF_UNDOC|IF_SM},
    {I_UMOV, 2, {REG16,REG16,0}, "\320\301\2\x0F\x13\110", IF_386|IF_UNDOC},
    {I_UMOV, 2, {REG32,MEMORY,0}, "\321\301\2\x0F\x13\110", IF_386|IF_UNDOC|IF_SM},
    {I_UMOV, 2, {REG32,REG32,0}, "\321\301\2\x0F\x13\110", IF_386|IF_UNDOC},
    {-1}
};

static struct itemplate instrux_VERR[] = {
    {I_VERR, 1, {MEMORY,0,0}, "\300\1\x0F\17\204", IF_286|IF_PRIV},
    {I_VERR, 1, {MEMORY|BITS16,0,0}, "\300\1\x0F\17\204", IF_286|IF_PRIV},
    {I_VERR, 1, {REG16,0,0}, "\300\1\x0F\17\204", IF_286|IF_PRIV},
    {-1}
};

static struct itemplate instrux_VERW[] = {
    {I_VERW, 1, {MEMORY,0,0}, "\300\1\x0F\17\205", IF_286|IF_PRIV},
    {I_VERW, 1, {MEMORY|BITS16,0,0}, "\300\1\x0F\17\205", IF_286|IF_PRIV},
    {I_VERW, 1, {REG16,0,0}, "\300\1\x0F\17\205", IF_286|IF_PRIV},
    {-1}
};

static struct itemplate instrux_WAIT[] = {
    {I_WAIT, 0, {0,0,0}, "\1\x9B", IF_8086},
    {-1}
};

static struct itemplate instrux_WBINVD[] = {
    {I_WBINVD, 0, {0,0,0}, "\2\x0F\x09", IF_486},
    {-1}
};

static struct itemplate instrux_WRMSR[] = {
    {I_WRMSR, 0, {0,0,0}, "\2\x0F\x30", IF_PENT},
    {-1}
};

static struct itemplate instrux_XADD[] = {
    {I_XADD, 2, {MEMORY,REG8,0}, "\300\2\x0F\xC0\101", IF_486|IF_SM},
    {I_XADD, 2, {REG8,REG8,0}, "\300\2\x0F\xC0\101", IF_486},
    {I_XADD, 2, {MEMORY,REG16,0}, "\320\300\2\x0F\xC1\101", IF_486|IF_SM},
    {I_XADD, 2, {REG16,REG16,0}, "\320\300\2\x0F\xC1\101", IF_486},
    {I_XADD, 2, {MEMORY,REG32,0}, "\321\300\2\x0F\xC1\101", IF_486|IF_SM},
    {I_XADD, 2, {REG32,REG32,0}, "\321\300\2\x0F\xC1\101", IF_486},
    {-1}
};

static struct itemplate instrux_XBTS[] = {
    {I_XBTS, 2, {REG16,MEMORY,0}, "\320\301\2\x0F\xA6\110", IF_386|IF_SW|IF_UNDOC},
    {I_XBTS, 2, {REG16,REG16,0}, "\320\301\2\x0F\xA6\110", IF_386|IF_UNDOC},
    {I_XBTS, 2, {REG32,MEMORY,0}, "\321\301\2\x0F\xA6\110", IF_386|IF_SD|IF_UNDOC},
    {I_XBTS, 2, {REG32,REG32,0}, "\321\301\2\x0F\xA6\110", IF_386|IF_UNDOC},
    {-1}
};

static struct itemplate instrux_XCHG[] = {
    {I_XCHG, 2, {REG_AX,REG16,0}, "\320\11\x90", IF_8086},
    {I_XCHG, 2, {REG_EAX,REG32,0}, "\321\11\x90", IF_386},
    {I_XCHG, 2, {REG16,REG_AX,0}, "\320\10\x90", IF_8086},
    {I_XCHG, 2, {REG32,REG_EAX,0}, "\321\10\x90", IF_386},
    {I_XCHG, 2, {REG8,MEMORY,0}, "\301\1\x86\110", IF_8086|IF_SM},
    {I_XCHG, 2, {REG8,REG8,0}, "\301\1\x86\110", IF_8086},
    {I_XCHG, 2, {REG16,MEMORY,0}, "\320\301\1\x87\110", IF_8086|IF_SM},
    {I_XCHG, 2, {REG16,REG16,0}, "\320\301\1\x87\110", IF_8086},
    {I_XCHG, 2, {REG32,MEMORY,0}, "\321\301\1\x87\110", IF_386|IF_SM},
    {I_XCHG, 2, {REG32,REG32,0}, "\321\301\1\x87\110", IF_386},
    {I_XCHG, 2, {MEMORY,REG8,0}, "\300\1\x86\101", IF_8086|IF_SM},
    {I_XCHG, 2, {REG8,REG8,0}, "\300\1\x86\101", IF_8086},
    {I_XCHG, 2, {MEMORY,REG16,0}, "\320\300\1\x87\101", IF_8086|IF_SM},
    {I_XCHG, 2, {REG16,REG16,0}, "\320\300\1\x87\101", IF_8086},
    {I_XCHG, 2, {MEMORY,REG32,0}, "\321\300\1\x87\101", IF_386|IF_SM},
    {I_XCHG, 2, {REG32,REG32,0}, "\321\300\1\x87\101", IF_386},
    {-1}
};

static struct itemplate instrux_XLATB[] = {
    {I_XLATB, 0, {0,0,0}, "\1\xD7", IF_8086},
    {-1}
};

static struct itemplate instrux_XOR[] = {
    {I_XOR, 2, {MEMORY,REG8,0}, "\300\1\x30\101", IF_8086|IF_SM},
    {I_XOR, 2, {REG8,REG8,0}, "\300\1\x30\101", IF_8086},
    {I_XOR, 2, {MEMORY,REG16,0}, "\320\300\1\x31\101", IF_8086|IF_SM},
    {I_XOR, 2, {REG16,REG16,0}, "\320\300\1\x31\101", IF_8086},
    {I_XOR, 2, {MEMORY,REG32,0}, "\321\300\1\x31\101", IF_386|IF_SM},
    {I_XOR, 2, {REG32,REG32,0}, "\321\300\1\x31\101", IF_386},
    {I_XOR, 2, {REG8,MEMORY,0}, "\301\1\x32\110", IF_8086|IF_SM},
    {I_XOR, 2, {REG8,REG8,0}, "\301\1\x32\110", IF_8086},
    {I_XOR, 2, {REG16,MEMORY,0}, "\320\301\1\x33\110", IF_8086|IF_SM},
    {I_XOR, 2, {REG16,REG16,0}, "\320\301\1\x33\110", IF_8086},
    {I_XOR, 2, {REG32,MEMORY,0}, "\321\301\1\x33\110", IF_386|IF_SM},
    {I_XOR, 2, {REG32,REG32,0}, "\321\301\1\x33\110", IF_386},
    {I_XOR, 2, {REGMEM|BITS16,IMMEDIATE|BITS8,0}, "\320\300\1\x83\206\15", IF_8086},
    {I_XOR, 2, {REGMEM|BITS32,IMMEDIATE|BITS8,0}, "\321\300\1\x83\206\15", IF_386},
    {I_XOR, 2, {REG_AL,IMMEDIATE,0}, "\1\x34\21", IF_8086|IF_SM},
    {I_XOR, 2, {REG_AX,IMMEDIATE,0}, "\320\1\x35\31", IF_8086|IF_SM},
    {I_XOR, 2, {REG_EAX,IMMEDIATE,0}, "\321\1\x35\41", IF_386|IF_SM},
    {I_XOR, 2, {REGMEM|BITS8,IMMEDIATE,0}, "\300\1\x80\206\21", IF_8086|IF_SM},
    {I_XOR, 2, {REGMEM|BITS16,IMMEDIATE,0}, "\320\300\1\x81\206\31", IF_8086|IF_SM},
    {I_XOR, 2, {REGMEM|BITS32,IMMEDIATE,0}, "\321\300\1\x81\206\41", IF_386|IF_SM},
    {I_XOR, 2, {MEMORY,IMMEDIATE|BITS8,0}, "\300\1\x80\206\21", IF_8086|IF_SM},
    {I_XOR, 2, {MEMORY,IMMEDIATE|BITS16,0}, "\320\300\1\x81\206\31", IF_8086|IF_SM},
    {I_XOR, 2, {MEMORY,IMMEDIATE|BITS32,0}, "\321\300\1\x81\206\41", IF_386|IF_SM},
    {-1}
};

static struct itemplate instrux_CMOVcc[] = {
    {I_CMOVcc, 2, {REG16,MEMORY,0}, "\320\301\1\x0F\330\x40\110", IF_P6|IF_SM},
    {I_CMOVcc, 2, {REG16,REG16,0}, "\320\301\1\x0F\330\x40\110", IF_P6},
    {I_CMOVcc, 2, {REG32,MEMORY,0}, "\321\301\1\x0F\330\x40\110", IF_P6|IF_SM},
    {I_CMOVcc, 2, {REG32,REG32,0}, "\321\301\1\x0F\330\x40\110", IF_P6},
    {-1}
};

static struct itemplate instrux_Jcc[] = {
    {I_Jcc, 1, {IMMEDIATE|NEAR,0,0}, "\322\1\x0F\330\x80\64", IF_386},
    {I_Jcc, 1, {IMMEDIATE,0,0}, "\330\x70\50", IF_8086},
    {I_Jcc, 1, {IMMEDIATE|SHORT,0,0}, "\330\x70\50", IF_8086},
    {-1}
};

static struct itemplate instrux_SETcc[] = {
    {I_SETcc, 1, {MEMORY,0,0}, "\300\1\x0F\330\x90\200", IF_386|IF_SB},
    {I_SETcc, 1, {REG8,0,0}, "\300\1\x0F\330\x90\200", IF_386},
    {-1}
};

struct itemplate *nasm_instructions[] = {
    instrux_AAA,
    instrux_AAD,
    instrux_AAM,
    instrux_AAS,
    instrux_ADC,
    instrux_ADD,
    instrux_AND,
    instrux_ARPL,
    instrux_BOUND,
    instrux_BSF,
    instrux_BSR,
    instrux_BSWAP,
    instrux_BT,
    instrux_BTC,
    instrux_BTR,
    instrux_BTS,
    instrux_CALL,
    instrux_CBW,
    instrux_CDQ,
    instrux_CLC,
    instrux_CLD,
    instrux_CLI,
    instrux_CLTS,
    instrux_CMC,
    instrux_CMP,
    instrux_CMPSB,
    instrux_CMPSD,
    instrux_CMPSW,
    instrux_CMPXCHG,
    instrux_CMPXCHG486,
    instrux_CMPXCHG8B,
    instrux_CPUID,
    instrux_CWD,
    instrux_CWDE,
    instrux_DAA,
    instrux_DAS,
    instrux_DB,
    instrux_DD,
    instrux_DEC,
    instrux_DIV,
    instrux_DQ,
    instrux_DT,
    instrux_DW,
    instrux_EMMS,
    instrux_ENTER,
    instrux_EQU,
    instrux_F2XM1,
    instrux_FABS,
    instrux_FADD,
    instrux_FADDP,
    instrux_FBLD,
    instrux_FBSTP,
    instrux_FCHS,
    instrux_FCLEX,
    instrux_FCMOVB,
    instrux_FCMOVBE,
    instrux_FCMOVE,
    instrux_FCMOVNB,
    instrux_FCMOVNBE,
    instrux_FCMOVNE,
    instrux_FCMOVNU,
    instrux_FCMOVU,
    instrux_FCOM,
    instrux_FCOMI,
    instrux_FCOMIP,
    instrux_FCOMP,
    instrux_FCOMPP,
    instrux_FCOS,
    instrux_FDECSTP,
    instrux_FDISI,
    instrux_FDIV,
    instrux_FDIVP,
    instrux_FDIVR,
    instrux_FDIVRP,
    instrux_FENI,
    instrux_FFREE,
    instrux_FIADD,
    instrux_FICOM,
    instrux_FICOMP,
    instrux_FIDIV,
    instrux_FIDIVR,
    instrux_FILD,
    instrux_FIMUL,
    instrux_FINCSTP,
    instrux_FINIT,
    instrux_FIST,
    instrux_FISTP,
    instrux_FISUB,
    instrux_FISUBR,
    instrux_FLD,
    instrux_FLD1,
    instrux_FLDCW,
    instrux_FLDENV,
    instrux_FLDL2E,
    instrux_FLDL2T,
    instrux_FLDLG2,
    instrux_FLDLN2,
    instrux_FLDPI,
    instrux_FLDZ,
    instrux_FMUL,
    instrux_FMULP,
    instrux_FNCLEX,
    instrux_FNDISI,
    instrux_FNENI,
    instrux_FNINIT,
    instrux_FNOP,
    instrux_FNSAVE,
    instrux_FNSTCW,
    instrux_FNSTENV,
    instrux_FNSTSW,
    instrux_FPATAN,
    instrux_FPREM,
    instrux_FPREM1,
    instrux_FPTAN,
    instrux_FRNDINT,
    instrux_FRSTOR,
    instrux_FSAVE,
    instrux_FSCALE,
    instrux_FSETPM,
    instrux_FSIN,
    instrux_FSINCOS,
    instrux_FSQRT,
    instrux_FST,
    instrux_FSTCW,
    instrux_FSTENV,
    instrux_FSTP,
    instrux_FSTSW,
    instrux_FSUB,
    instrux_FSUBP,
    instrux_FSUBR,
    instrux_FSUBRP,
    instrux_FTST,
    instrux_FUCOM,
    instrux_FUCOMI,
    instrux_FUCOMIP,
    instrux_FUCOMP,
    instrux_FUCOMPP,
    instrux_FXAM,
    instrux_FXCH,
    instrux_FXTRACT,
    instrux_FYL2X,
    instrux_FYL2XP1,
    instrux_HLT,
    instrux_IBTS,
    instrux_ICEBP,
    instrux_IDIV,
    instrux_IMUL,
    instrux_IN,
    instrux_INC,
    instrux_INCBIN,
    instrux_INSB,
    instrux_INSD,
    instrux_INSW,
    instrux_INT,
    instrux_INT01,
    instrux_INT1,
    instrux_INT3,
    instrux_INTO,
    instrux_INVD,
    instrux_INVLPG,
    instrux_IRET,
    instrux_IRETD,
    instrux_IRETW,
    instrux_JCXZ,
    instrux_JECXZ,
    instrux_JMP,
    instrux_LAHF,
    instrux_LAR,
    instrux_LDS,
    instrux_LEA,
    instrux_LEAVE,
    instrux_LES,
    instrux_LFS,
    instrux_LGDT,
    instrux_LGS,
    instrux_LIDT,
    instrux_LLDT,
    instrux_LMSW,
    instrux_LOADALL,
    instrux_LOADALL286,
    instrux_LODSB,
    instrux_LODSD,
    instrux_LODSW,
    instrux_LOOP,
    instrux_LOOPE,
    instrux_LOOPNE,
    instrux_LOOPNZ,
    instrux_LOOPZ,
    instrux_LSL,
    instrux_LSS,
    instrux_LTR,
    instrux_MOV,
    instrux_MOVD,
    instrux_MOVQ,
    instrux_MOVSB,
    instrux_MOVSD,
    instrux_MOVSW,
    instrux_MOVSX,
    instrux_MOVZX,
    instrux_MUL,
    instrux_NEG,
    instrux_NOP,
    instrux_NOT,
    instrux_OR,
    instrux_OUT,
    instrux_OUTSB,
    instrux_OUTSD,
    instrux_OUTSW,
    instrux_PACKSSDW,
    instrux_PACKSSWB,
    instrux_PACKUSWB,
    instrux_PADDB,
    instrux_PADDD,
    instrux_PADDSB,
    instrux_PADDSIW,
    instrux_PADDSW,
    instrux_PADDUSB,
    instrux_PADDUSW,
    instrux_PADDW,
    instrux_PAND,
    instrux_PANDN,
    instrux_PAVEB,
    instrux_PCMPEQB,
    instrux_PCMPEQD,
    instrux_PCMPEQW,
    instrux_PCMPGTB,
    instrux_PCMPGTD,
    instrux_PCMPGTW,
    instrux_PDISTIB,
    instrux_PMACHRIW,
    instrux_PMADDWD,
    instrux_PMAGW,
    instrux_PMULHRW,
    instrux_PMULHRIW,
    instrux_PMULHW,
    instrux_PMULLW,
    instrux_PMVGEZB,
    instrux_PMVLZB,
    instrux_PMVNZB,
    instrux_PMVZB,
    instrux_POP,
    instrux_POPA,
    instrux_POPAD,
    instrux_POPAW,
    instrux_POPF,
    instrux_POPFD,
    instrux_POPFW,
    instrux_POR,
    instrux_PSLLD,
    instrux_PSLLQ,
    instrux_PSLLW,
    instrux_PSRAD,
    instrux_PSRAW,
    instrux_PSRLD,
    instrux_PSRLQ,
    instrux_PSRLW,
    instrux_PSUBB,
    instrux_PSUBD,
    instrux_PSUBSB,
    instrux_PSUBSIW,
    instrux_PSUBSW,
    instrux_PSUBUSB,
    instrux_PSUBUSW,
    instrux_PSUBW,
    instrux_PUNPCKHBW,
    instrux_PUNPCKHDQ,
    instrux_PUNPCKHWD,
    instrux_PUNPCKLBW,
    instrux_PUNPCKLDQ,
    instrux_PUNPCKLWD,
    instrux_PUSH,
    instrux_PUSHA,
    instrux_PUSHAD,
    instrux_PUSHAW,
    instrux_PUSHF,
    instrux_PUSHFD,
    instrux_PUSHFW,
    instrux_PXOR,
    instrux_RCL,
    instrux_RCR,
    instrux_RDMSR,
    instrux_RDPMC,
    instrux_RDTSC,
    instrux_RESB,
    instrux_RESD,
    instrux_RESQ,
    instrux_REST,
    instrux_RESW,
    instrux_RET,
    instrux_RETF,
    instrux_RETN,
    instrux_ROL,
    instrux_ROR,
    instrux_RSM,
    instrux_SAHF,
    instrux_SAL,
    instrux_SALC,
    instrux_SAR,
    instrux_SBB,
    instrux_SCASB,
    instrux_SCASD,
    instrux_SCASW,
    instrux_SGDT,
    instrux_SHL,
    instrux_SHLD,
    instrux_SHR,
    instrux_SHRD,
    instrux_SIDT,
    instrux_SLDT,
    instrux_SMI,
    instrux_SMSW,
    instrux_STC,
    instrux_STD,
    instrux_STI,
    instrux_STOSB,
    instrux_STOSD,
    instrux_STOSW,
    instrux_STR,
    instrux_SUB,
    instrux_TEST,
    instrux_UMOV,
    instrux_VERR,
    instrux_VERW,
    instrux_WAIT,
    instrux_WBINVD,
    instrux_WRMSR,
    instrux_XADD,
    instrux_XBTS,
    instrux_XCHG,
    instrux_XLATB,
    instrux_XOR,
    instrux_CMOVcc,
    instrux_Jcc,
    instrux_SETcc,
};
