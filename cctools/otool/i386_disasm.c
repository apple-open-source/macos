/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * This file contains the i386 disassembler routine used at NeXT Computer, Inc.
 * to match the the assembler used at NeXT.  It was addapted from a set of
 * source files with the following copyright which is retained below.
 */
/*
  Copyright 1988, 1989 by Intel Corporation, Santa Clara, California.

		All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Intel
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

INTEL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL INTEL BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#include <stdio.h>
#include <string.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/reloc.h>
#include "stuff/bytesex.h"
#include "otool.h"
#include "ofile_print.h"

#define MAX_MNEMONIC	9	/* Maximum number of chars per mnemonic */
#define MAX_RESULT	14	/* Maximum number of char in a register */
				/*  result expression "(%ebx,%ecx,8)" */

#define WBIT(x)	(x & 0x1)		/* to get w bit	*/
#define REGNO(x) (x & 0x7)		/* to get 3 bit register */
#define VBIT(x)	((x)>>1 & 0x1)		/* to get 'v' bit */
#define OPSIZE(data16,wbit) ((wbit) ? ((data16) ? 2:4) : 1 )

#define REG_ONLY 3	/* mode indicates a single register with	*/
			/* no displacement is an operand		*/
#define BYTEOPERAND 0	/* value of the w-bit indicating a byte		*/
			/* operand (1-byte)				*/
#define LONGOPERAND 1	/* value of the w-bit indicating a long		*/
			/* operand (2-bytes or 4-bytes)			*/
#define EBP 5
#define ESP 4

/*
 * This is the structure that is used for storing all the op code information.
 */
struct instable {
    char name[MAX_MNEMONIC];
    const struct instable *indirect;
    unsigned adr_mode;
    int suffix;	/* for instructions which may have a 'w' or 'l' suffix */
};
#define	TERM	0	/* used to indicate that the 'indirect' field of the */
			/* 'instable' terminates - no pointer.	*/
#define	INVALID	{"",TERM,UNKNOWN,0}

static void get_operand(
    const char **symadd,
    const char **symsub,
    unsigned long *value,
    unsigned long *value_size,
    char *result,
    const unsigned long mode,
    const unsigned long r_m,
    const unsigned long wbit,
    const enum bool data16,
    const enum bool addr16,
    const char *sect,
    unsigned long sect_addr,
    unsigned long *length,
    unsigned long *left,
    const unsigned long addr,
    const struct relocation_info *sorted_relocs,
    const unsigned long nsorted_relocs,
    const struct nlist *symbols,
    const unsigned long nsymbols,
    const char *strings,
    const unsigned long strings_size,
    const struct nlist *sorted_symbols,
    const unsigned long nsorted_symbols,
    const enum bool verbose);

static void immediate(
    const char **symadd,
    const char **symsub,
    unsigned long *value,
    unsigned long value_size,
    const char *sect,
    unsigned long sect_addr,
    unsigned long *length,
    unsigned long *left,
    const unsigned long addr,
    const struct relocation_info *sorted_relocs,
    const unsigned long nsorted_relocs,
    const struct nlist *symbols,
    const unsigned long nsymbols,
    const char *strings,
    const unsigned long strings_size,
    const struct nlist *sorted_symbols,
    const unsigned long nsorted_symbols,
    const enum bool verbose);

static void displacement(
    const char **symadd,
    const char **symsub,
    unsigned long *value,
    const unsigned long value_size,
    const char *sect,
    unsigned long sect_addr,
    unsigned long *length,
    unsigned long *left,
    const unsigned long addr,
    const struct relocation_info *sorted_relocs,
    const unsigned long nsorted_relocs,
    const struct nlist *symbols,
    const unsigned long nsymbols,
    const char *strings,
    const unsigned long strings_size,
    const struct nlist *sorted_symbols,
    const unsigned long nsorted_symbols,
    const enum bool verbose);

static void get_symbol(
    const char **symadd,
    const char **symsub,
    unsigned long *offset,
    const unsigned long sect_offset,
    const unsigned long value,
    const struct relocation_info *relocs,
    const unsigned long nrelocs,
    const struct nlist *symbols,
    const unsigned long nsymbols,
    const char *strings,
    const unsigned long strings_size,
    const struct nlist *sorted_symbols,
    const unsigned long nsorted_symbols,
    const enum bool verbose);

static void print_operand(
    const char *seg,
    const char *symadd,
    const char *symsub,
    const unsigned int value,
    const unsigned int value_size,
    const char *result,
    const char *tail);

static unsigned long get_value(
    const unsigned long size,
    const char *sect,
    unsigned long *length,
    unsigned long *left);

static void modrm_byte(
    unsigned long *mode,
    unsigned long *reg,
    unsigned long *r_m,
    unsigned char byte);

#define GET_OPERAND(symadd, symsub, value, value_size, result) \
	get_operand((symadd), (symsub), (value), (value_size), (result), \
		    mode, r_m, wbit, data16, addr16, sect, sect_addr, &length, \
		    &left, addr, sorted_relocs, nsorted_relocs, symbols, \
		    nsymbols, strings, strings_size, sorted_symbols, \
		    nsorted_symbols, verbose)

#define DISPLACEMENT(symadd, symsub, value, value_size) \
	displacement((symadd), (symsub), (value), (value_size), sect, \
		     sect_addr, &length, &left, addr, sorted_relocs, \
		     nsorted_relocs, symbols, nsymbols, strings, strings_size, \
		     sorted_symbols, nsorted_symbols, verbose)

#define IMMEDIATE(symadd, symsub, value, value_size) \
	immediate((symadd), (symsub), (value), (value_size), sect, sect_addr, \
		  &length, &left, addr, sorted_relocs, nsorted_relocs, \
		  symbols, nsymbols, strings, strings_size, sorted_symbols, \
		  nsorted_symbols, verbose)

#define GET_SYMBOL(symadd, symsub, offset, sect_offset, value) \
	get_symbol((symadd), (symsub), (offset), (sect_offset), (value), \
		    sorted_relocs, nsorted_relocs, symbols, nsymbols, strings,\
		    strings_size, sorted_symbols, nsorted_symbols, verbose)

#define GUESS_SYMBOL(value) \
	guess_symbol((value), sorted_symbols, nsorted_symbols, verbose)

/*
 * These are the instruction formats as they appear in the disassembly tables.
 * Here they are given numerical values for use in the actual disassembly of
 * an instruction.
 */
#define UNKNOWN	0
#define MRw	2
#define IMlw	3
#define IMw	4
#define IR	5
#define OA	6
#define AO	7
#define MS	8
#define SM	9
#define Mv	10
#define Mw	11
#define M	12
#define R	13
#define RA	14
#define SEG	15
#define MR	16
#define IA	17
#define MA	18
#define SD	19
#define AD	20
#define SA	21
#define D	22
#define INM	23
#define SO	24
#define BD	25
#define I	26
#define P	27
#define V	28
#define DSHIFT	29 /* for double shift that has an 8-bit immediate */
#define U	30
#define OVERRIDE 31
#define GO_ON	32
#define O	33	/* for call	*/
#define JTAB	34	/* jump table (not used at NeXT) */
#define IMUL	35	/* for 186 iimul instr  */
#define CBW 36 /* so that data16 can be evaluated for cbw and its variants */
#define MvI	37	/* for 186 logicals */
#define ENTER	38	/* for 186 enter instr  */
#define RMw	39	/* for 286 arpl instr */
#define Ib	40	/* for push immediate byte */
#define F	41	/* for 287 instructions */
#define FF	42	/* for 287 instructions */
#define DM	43	/* 16-bit data */
#define AM	44	/* 16-bit addr */
#define LSEG	45	/* for 3-bit seg reg encoding */
#define MIb	46	/* for 386 logicals */
#define SREG	47	/* for 386 special registers */
#define PREFIX	48	/* an instruction prefix like REP, LOCK */
#define INT3	49	/* The int 3 instruction, which has a fake operand */
#define DSHIFTcl 50	/* for double shift that implicitly uses %cl */
#define CWD	51	/* so that data16 can be evaluated for cwd and vars */
#define RET	52	/* single immediate 16-bit operand */
#define MOVZ	53	/* for movs and movz, with different size operands */
#define XINST	54	/* for cmpxchg and xadd */
#define BSWAP	55	/* for bswap */
#define Pi	56
#define Po	57
#define Vi	58
#define Vo	59
#define Mb	60
#define INMl	61

/*
 * In 16-bit addressing mode:
 * Register operands may be indicated by a distinguished field.
 * An '8' bit register is selected if the 'w' bit is equal to 0,
 * and a '16' bit register is selected if the 'w' bit is equal to
 * 1 and also if there is no 'w' bit.
 */
static const char * const REG16[8][2] = {
/* w bit		0		1		*/
/* reg bits */
/* 000	*/		{"%al",		"%ax"},
/* 001  */		{"%cl",		"%cx"},
/* 010  */		{"%dl",		"%dx"},
/* 011	*/		{"%bl",		"%bx"},
/* 100	*/		{"%ah",		"%sp"},
/* 101	*/		{"%ch",		"%bp"},
/* 110	*/		{"%dh",		"%si"},
/* 111	*/		{"%bh",		"%di"}
};

/*
 * In 32-bit addressing mode:
 * Register operands may be indicated by a distinguished field.
 * An '8' bit register is selected if the 'w' bit is equal to 0,
 * and a '32' bit register is selected if the 'w' bit is equal to
 * 1 and also if there is no 'w' bit.
 */
static const char * const REG32[8][2] = {
/* w bit		0		1		*/
/* reg bits */
/* 000	*/		{"%al",		"%eax"},
/* 001  */		{"%cl",		"%ecx"},
/* 010  */		{"%dl",		"%edx"},
/* 011	*/		{"%bl",		"%ebx"},
/* 100	*/		{"%ah",		"%esp"},
/* 101	*/		{"%ch",		"%ebp"},
/* 110	*/		{"%dh",		"%esi"},
/* 111	*/		{"%bh",		"%edi"}
};

/*
 * In 16-bit mode:
 * This initialized array will be indexed by the 'r/m' and 'mod'
 * fields, to determine the size of the displacement in each mode.
 */
static const char dispsize16 [8][4] = {
/* mod		00	01	10	11 */
/* r/m */
/* 000 */	{0,	1,	2,	0},
/* 001 */	{0,	1,	2,	0},
/* 010 */	{0,	1,	2,	0},
/* 011 */	{0,	1,	2,	0},
/* 100 */	{0,	1,	2,	0},
/* 101 */	{0,	1,	2,	0},
/* 110 */	{2,	1,	2,	0},
/* 111 */	{0,	1,	2,	0}
};

/*
 * In 32-bit mode:
 * This initialized array will be indexed by the 'r/m' and 'mod'
 * fields, to determine the size of the displacement in this mode.
 */
static const char dispsize32 [8][4] = {
/* mod		00	01	10	11 */
/* r/m */
/* 000 */	{0,	1,	4,	0},
/* 001 */	{0,	1,	4,	0},
/* 010 */	{0,	1,	4,	0},
/* 011 */	{0,	1,	4,	0},
/* 100 */	{0,	1,	4,	0},
/* 101 */	{4,	1,	4,	0},
/* 110 */	{0,	1,	4,	0},
/* 111 */	{0,	1,	4,	0}
};

/*
 * When data16 has been specified, the following array specifies the registers
 * for the different addressing modes.  Indexed first by mode, then by register
 * number.
 */
static const char * const regname16[4][8] = {
/*reg  000        001        010        011        100    101   110     111 */
/*mod*/
/*00*/{"%bx,%si", "%bx,%di", "%bp,%si", "%bp,%di", "%si", "%di", "",    "%bx"},
/*01*/{"%bx,%si", "%bx,%di", "%bp,%si", "%bp,%di", "%si", "%di", "%bp", "%bx"},
/*10*/{"%bx,%si", "%bx,%di", "%bp,%si", "%bp,%di", "%si", "%di", "%bp", "%bx"},
/*11*/{"%ax",     "%cx",     "%dx",     "%bx",     "%sp", "%bp", "%si", "%di"}
};

/*
 * When data16 has not been specified, fields, to determine the addressing mode,
 * and will also provide strings for printing.
 */
static const char * const regname32[4][8] = {
/*reg   000     001     010     011     100     101     110     111 */
/*mod*/
/*00 */{"%eax", "%ecx", "%edx", "%ebx", "%esp", "",     "%esi", "%edi"},
/*01 */{"%eax", "%ecx", "%edx", "%ebx", "%esp", "%ebp", "%esi", "%edi"},
/*10 */{"%eax", "%ecx", "%edx", "%ebx", "%esp", "%ebp", "%esi", "%edi"},
/*11 */{"%eax", "%ecx", "%edx", "%ebx", "%esp", "%ebp", "%esi", "%edi"}
};

/*
 * If r/m==100 then the following byte (the s-i-b byte) must be decoded
 */
static const char * const scale_factor[4] = {
    "1",
    "2",
    "4",
    "8"
};

static const char * const indexname[8] = {
    ",%eax",
    ",%ecx",
    ",%edx",
    ",%ebx",
    "",
    ",%ebp",
    ",%esi",
    ",%edi"
};

/*
 * Segment registers are selected by a two or three bit field.
 */
static const char * const SEGREG[6] = {
/* 000 */	"%es",
/* 001 */	"%cs",
/* 010 */	"%ss",
/* 011 */	"%ds",
/* 100 */	"%fs",
/* 101 */	"%gs",
};

/*
 * Special Registers
 */
static const char * const DEBUGREG[8] = {
    "%db0", "%db1", "%db2", "%db3", "%db4", "%db5", "%db6", "%db7"
};

static const char * const CONTROLREG[8] = {
    "%cr0", "%cr1", "%cr2", "%cr3", "%cr4?", "%cr5?", "%cr6?", "%cr7?"
};

static const char * const TESTREG[8] = {
    "%tr0?", "%tr1?", "%tr2?", "%tr3", "%tr4", "%tr5", "%tr6", "%tr7"
};

/*
 * Decode table for 0x0F00 opcodes
 */
static const struct instable op0F00[8] = {
/*  [0]  */	{"sldt",TERM,M,1},	{"str",TERM,M,1},
		{"lldt",TERM,M,1},	{"ltr",TERM,M,1},
/*  [4]  */	{"verr",TERM,M,1},	{"verw",TERM,M,1},
		INVALID,		INVALID,
};


/*
 * Decode table for 0x0F01 opcodes
 */
static const struct instable op0F01[8] = {
/*  [0]  */	{"sgdt",TERM,M,1},	{"sidt",TERM,M,1},
		{"lgdt",TERM,M,1},	{"lidt",TERM,M,1},
/*  [4]  */	{"smsw",TERM,M,1},	INVALID,
		{"lmsw",TERM,M,1},	{"invlpg",TERM,M,1},
};

/*
 * Decode table for 0x0FBA opcodes
 */
static const struct instable op0FBA[8] = {
/*  [0]  */	INVALID,		INVALID,
		INVALID,		INVALID,
/*  [4]  */	{"bt",TERM,MIb,1},	{"bts",TERM,MIb,1},
		{"btr",TERM,MIb,1},	{"btc",TERM,MIb,1},
};

/*
 * Decode table for 0x0F opcodes
 */
static const struct instable op0F[16][16] = {
/*  [00]  */ {  {"",op0F00,TERM,0},	{"",op0F01,TERM,0},
		{"lar",TERM,MR,0},	{"lsl",TERM,MR,0},
/*  [04]  */	INVALID,		INVALID,
		{"clts",TERM,GO_ON,0},	INVALID,
/*  [08]  */	{"invd",TERM,GO_ON,0},	{"wbinvd",TERM,GO_ON,0},
		INVALID,		INVALID,
/*  [0C]  */	INVALID,		INVALID,
		INVALID,		INVALID },
/*  [10]  */ {  INVALID,		INVALID,
		INVALID,		INVALID,
/*  [14]  */	INVALID,		INVALID,
		INVALID,		INVALID,
/*  [18]  */	INVALID,		INVALID,
		INVALID,		INVALID,
/*  [1C]  */	INVALID,		INVALID,
		INVALID,		INVALID },
/*  [20]  */ {  {"mov",TERM,SREG,1},	{"mov",TERM,SREG,1},
		{"mov",TERM,SREG,1},	{"mov",TERM,SREG,1},
/*  [24]  */	{"mov",TERM,SREG,1},	INVALID,
		{"mov",TERM,SREG,1},	INVALID,
/*  [28]  */	INVALID,		INVALID,
		INVALID,		INVALID,
/*  [2C]  */	INVALID,		INVALID,
		INVALID,		INVALID },
/*  [30]  */ {  {"wrmsr",TERM,GO_ON,0},	{"rdtsc",TERM,GO_ON,0},
		{"rdmsr",TERM,GO_ON,0},	{"rdpmc",TERM,GO_ON,0},
/*  [34]  */	INVALID,		INVALID,
		INVALID,		INVALID,
/*  [38]  */	INVALID,		INVALID,
		INVALID,		INVALID,
/*  [3C]  */	INVALID,		INVALID,
		INVALID,		INVALID },
/*  [40]  */ {  {"cmovo",TERM,MRw,1},	{"cmovno",TERM,MRw,1},
		{"cmovb",TERM,MRw,1},	{"cmovae",TERM,MRw,1},
/*  [44]  */	{"cmove",TERM,MRw,1},	{"cmovne",TERM,MRw,1},
		{"cmovbe",TERM,MRw,1},	{"cmova",TERM,MRw,1},
/*  [48]  */	{"cmovs",TERM,MRw,1},	{"cmovns",TERM,MRw,1},
		{"cmovp",TERM,MRw,1},	{"cmovnp",TERM,MRw,1},
/*  [4C]  */	{"cmovl",TERM,MRw,1},	{"cmovge",TERM,MRw,1},
		{"cmovle",TERM,MRw,1},	{"cmovg",TERM,MRw,1} },
/*  [50]  */ {  INVALID,		INVALID,
		INVALID,		INVALID,
/*  [54]  */	INVALID,		INVALID,
		INVALID,		INVALID,
/*  [58]  */	INVALID,		INVALID,
		INVALID,		INVALID,
/*  [5C]  */	INVALID,		INVALID,
		INVALID,		INVALID },
/*  [60]  */ {  INVALID,		INVALID,
		INVALID,		INVALID,
/*  [64]  */	INVALID,		INVALID,
		INVALID,		INVALID,
/*  [68]  */	INVALID,		INVALID,
		INVALID,		INVALID,
/*  [6C]  */	INVALID,		INVALID,
		INVALID,		INVALID },
/*  [70]  */ {  INVALID,		INVALID,
		INVALID,		INVALID,
/*  [74]  */	INVALID,		INVALID,
		INVALID,		INVALID,
/*  [78]  */	INVALID,		INVALID,
		INVALID,		INVALID,
/*  [7C]  */	INVALID,		INVALID,
		INVALID,		INVALID },
/*  [80]  */ {  {"jo",TERM,D,1},	{"jno",TERM,D,1},
		{"jb",TERM,D,1},	{"jae",TERM,D,1},
/*  [84]  */	{"je",TERM,D,1},	{"jne",TERM,D,1},
		{"jbe",TERM,D,1},	{"ja",TERM,D,1},
/*  [88]  */	{"js",TERM,D,1},	{"jns",TERM,D,1},
		{"jp",TERM,D,1},	{"jnp",TERM,D,1},
/*  [8C]  */	{"jl",TERM,D,1},	{"jge",TERM,D,1},
		{"jle",TERM,D,1},	{"jg",TERM,D,1} },
/*  [90]  */ {  {"seto",TERM,Mb,1},	{"setno",TERM,Mb,1},
		{"setb",TERM,Mb,1},	{"setae",TERM,Mb,1},
/*  [94]  */	{"sete",TERM,Mb,1},	{"setne",TERM,Mb,1},
		{"setbe",TERM,Mb,1},	{"seta",TERM,Mb,1},
/*  [98]  */	{"sets",TERM,Mb,1},	{"setns",TERM,Mb,1},
		{"setp",TERM,Mb,1},	{"setnp",TERM,Mb,1},
/*  [9C]  */	{"setl",TERM,Mb,1},	{"setge",TERM,Mb,1},
		{"setle",TERM,Mb,1},	{"setg",TERM,Mb,1} },
/*  [A0]  */ {  {"push",TERM,LSEG,1},	{"pop",TERM,LSEG,1},
		{"cpuid",TERM,GO_ON,0},	{"bt",TERM,RMw,1},
/*  [A4]  */	{"shld",TERM,DSHIFT,1},	{"shld",TERM,DSHIFTcl,1},
		INVALID,		INVALID,
/*  [A8]  */	{"push",TERM,LSEG,1},	{"pop",TERM,LSEG,1},
		{"rsm",TERM,GO_ON,0},	{"bts",TERM,RMw,1},
/*  [AC]  */	{"shrd",TERM,DSHIFT,1},	{"shrd",TERM,DSHIFTcl,1},
		INVALID,		{"imul",TERM,MRw,1} },
/*  [B0]  */ {  {"cmpxchgb",TERM,XINST,0},{"cmpxchg",TERM,XINST,1},
		{"lss",TERM,MR,0},	{"btr",TERM,RMw,1},
/*  [B4]  */	{"lfs",TERM,MR,0},	{"lgs",TERM,MR,0},
		{"movzb",TERM,MOVZ,1},	{"movzwl",TERM,MOVZ,0},
/*  [B8]  */	INVALID,		INVALID,
		{"",op0FBA,TERM,0},	{"btc",TERM,RMw,1},
/*  [BC]  */	{"bsf",TERM,MRw,1},	{"bsr",TERM,MRw,1},
		{"movsb",TERM,MOVZ,1},	{"movswl",TERM,MOVZ,0} },
/*  [C0]  */ {  {"xaddb",TERM,XINST,0},	{"xadd",TERM,XINST,1},
		INVALID,		INVALID,
/*  [C4]  */	INVALID,		INVALID,
		INVALID,		{"cmpxchg8b",TERM,M,1},
/*  [C8]  */	{"bswap",TERM,BSWAP,0},	{"bswap",TERM,BSWAP,0},
		{"bswap",TERM,BSWAP,0},	{"bswap",TERM,BSWAP,0},
/*  [CC]  */	{"bswap",TERM,BSWAP,0},	{"bswap",TERM,BSWAP,0},
		{"bswap",TERM,BSWAP,0},	{"bswap",TERM,BSWAP,0} },
/*  [D0]  */ {  INVALID,		INVALID,
		INVALID,		INVALID,
/*  [D4]  */	INVALID,		INVALID,
		INVALID,		INVALID,
/*  [D8]  */	INVALID,		INVALID,
		INVALID,		INVALID,
/*  [DC]  */	INVALID,		INVALID,
		INVALID,		INVALID },
/*  [E0]  */ {  INVALID,		INVALID,
		INVALID,		INVALID,
/*  [E4]  */	INVALID,		INVALID,
		INVALID,		INVALID,
/*  [E8]  */	INVALID,		INVALID,
		INVALID,		INVALID,
/*  [EC]  */	INVALID,		INVALID,
		INVALID,		INVALID },
/*  [F0]  */ {  INVALID,		INVALID,
		INVALID,		INVALID,
/*  [F4]  */	INVALID,		INVALID,
		INVALID,		INVALID,
/*  [F8]  */	INVALID,		INVALID,
		INVALID,		INVALID,
/*  [FC]  */	INVALID,		INVALID,
		INVALID,		{"ud2",TERM,GO_ON,0} },
};

/*
 * Decode table for 0x80 opcodes
 */
static const struct instable op80[8] = {
/*  [0]  */	{"addb",TERM,IMlw,0},	{"orb",TERM,IMw,0},
		{"adcb",TERM,IMlw,0},	{"sbbb",TERM,IMlw,0},
/*  [4]  */	{"andb",TERM,IMw,0},	{"subb",TERM,IMlw,0},
		{"xorb",TERM,IMw,0},	{"cmpb",TERM,IMlw,0},
};

/*
 * Decode table for 0x81 opcodes.
 */
static const struct instable op81[8] = {
/*  [0]  */	{"add",TERM,IMlw,1},	{"or",TERM,IMw,1},
		{"adc",TERM,IMlw,1},	{"sbb",TERM,IMlw,1},
/*  [4]  */	{"and",TERM,IMw,1},	{"sub",TERM,IMlw,1},
		{"xor",TERM,IMw,1},	{"cmp",TERM,IMlw,1},
};

/*
 * Decode table for 0x82 opcodes.
 */
static const struct instable op82[8] = {
/*  [0]  */	{"addb",TERM,IMlw,0},	INVALID,
		{"adcb",TERM,IMlw,0},	{"sbbb",TERM,IMlw,0},
/*  [4]  */	INVALID,		{"subb",TERM,IMlw,0},
		INVALID,		{"cmpb",TERM,IMlw,0},
};

/*
 * Decode table for 0x83 opcodes.
 */
static const struct instable op83[8] = {
/*  [0]  */	{"add",TERM,IMlw,1},	{"or",TERM,IMlw,1},
		{"adc",TERM,IMlw,1},	{"sbb",TERM,IMlw,1},
/*  [4]  */	{"and",TERM,IMlw,1},	{"sub",TERM,IMlw,1},
		{"xor",TERM,IMlw,1},	{"cmp",TERM,IMlw,1},
};

/*
 * Decode table for 0xC0 opcodes.
 */
static const struct instable opC0[8] = {
/*  [0]  */	{"rolb",TERM,MvI,0},	{"rorb",TERM,MvI,0},
		{"rclb",TERM,MvI,0},	{"rcrb",TERM,MvI,0},
/*  [4]  */	{"shlb",TERM,MvI,0},	{"shrb",TERM,MvI,0},
		INVALID,		{"sarb",TERM,MvI,0},
};

/*
 * Decode table for 0xD0 opcodes.
 */
static const struct instable opD0[8] = {
/*  [0]  */	{"rolb",TERM,Mv,0},	{"rorb",TERM,Mv,0},
		{"rclb",TERM,Mv,0},	{"rcrb",TERM,Mv,0},
/*  [4]  */	{"shlb",TERM,Mv,0},	{"shrb",TERM,Mv,0},
		INVALID,		{"sarb",TERM,Mv,0},
};

/*
 * Decode table for 0xC1 opcodes.
 * 186 instruction set
 */
static const struct instable opC1[8] = {
/*  [0]  */	{"rol",TERM,MvI,1},	{"ror",TERM,MvI,1},
		{"rcl",TERM,MvI,1},	{"rcr",TERM,MvI,1},
/*  [4]  */	{"shl",TERM,MvI,1},	{"shr",TERM,MvI,1},
		INVALID,		{"sar",TERM,MvI,1},
};

/*
 * Decode table for 0xD1 opcodes.
 */
static const struct instable opD1[8] = {
/*  [0]  */	{"rol",TERM,Mv,1},	{"ror",TERM,Mv,1},
		{"rcl",TERM,Mv,1},	{"rcr",TERM,Mv,1},
/*  [4]  */	{"shl",TERM,Mv,1},	{"shr",TERM,Mv,1},
		INVALID,		{"sar",TERM,Mv,1},
};

/*
 * Decode table for 0xD2 opcodes.
 */
static const struct instable opD2[8] = {
/*  [0]  */	{"rolb",TERM,Mv,0},	{"rorb",TERM,Mv,0},
		{"rclb",TERM,Mv,0},	{"rcrb",TERM,Mv,0},
/*  [4]  */	{"shlb",TERM,Mv,0},	{"shrb",TERM,Mv,0},
		INVALID,		{"sarb",TERM,Mv,0},
};

/*
 * Decode table for 0xD3 opcodes.
 */
static const struct instable opD3[8] = {
/*  [0]  */	{"rol",TERM,Mv,1},	{"ror",TERM,Mv,1},
		{"rcl",TERM,Mv,1},	{"rcr",TERM,Mv,1},
/*  [4]  */	{"shl",TERM,Mv,1},	{"shr",TERM,Mv,1},
		INVALID,		{"sar",TERM,Mv,1},
};

/*
 * Decode table for 0xF6 opcodes.
 */
static const struct instable opF6[8] = {
/*  [0]  */	{"testb",TERM,IMw,0},	INVALID,
		{"notb",TERM,Mw,0},	{"negb",TERM,Mw,0},
/*  [4]  */	{"mulb",TERM,MA,0},	{"imulb",TERM,MA,0},
		{"divb",TERM,MA,0},	{"idivb",TERM,MA,0},
};

/*
 * Decode table for 0xF7 opcodes.
 */
static const struct instable opF7[8] = {
/*  [0]  */	{"test",TERM,IMw,1},	INVALID,
		{"not",TERM,Mw,1},	{"neg",TERM,Mw,1},
/*  [4]  */	{"mul",TERM,MA,1},	{"imul",TERM,MA,1},
		{"div",TERM,MA,1},	{"idiv",TERM,MA,1},
};

/*
 * Decode table for 0xFE opcodes.
 */
static const struct instable opFE[8] = {
/*  [0]  */	{"incb",TERM,Mw,0},	{"decb",TERM,Mw,0},
		INVALID,		INVALID,
/*  [4]  */	INVALID,		INVALID,
		INVALID,		INVALID,
};

/*
 * Decode table for 0xFF opcodes.
 */
static const struct instable opFF[8] = {
/*  [0]  */	{"inc",TERM,Mw,1},	{"dec",TERM,Mw,1},
		{"call",TERM,INM,1},	{"lcall",TERM,INMl,1},
/*  [4]  */	{"jmp",TERM,INM,1},	{"ljmp",TERM,INMl,1},
		{"push",TERM,M,1},	INVALID,
};

/* for 287 instructions, which are a mess to decode */
static const struct instable opFP1n2[8][8] = {
/* bit pattern:	1101 1xxx MODxx xR/M */
/*  [0,0]  */ { {"fadds",TERM,M,1},	{"fmuls",TERM,M,1},
		{"fcoms",TERM,M,1},	{"fcomps",TERM,M,1},
/*  [0,4]  */	{"fsubs",TERM,M,1},	{"fsubrs",TERM,M,1},
		{"fdivs",TERM,M,1},	{"fdivrs",TERM,M,1} },
/*  [1,0]  */ { {"flds",TERM,M,1},	INVALID,
		{"fsts",TERM,M,1},	{"fstps",TERM,M,1},
/*  [1,4]  */	{"fldenv",TERM,M,1},	{"fldcw",TERM,M,1},
		{"fnstenv",TERM,M,1},	{"fnstcw",TERM,M,1} },
/*  [2,0]  */ { {"fiaddl",TERM,M,1},	{"fimull",TERM,M,1},
		{"ficoml",TERM,M,1},	{"ficompl",TERM,M,1},
/*  [2,4]  */	{"fisubl",TERM,M,1},	{"fisubrl",TERM,M,1},
		{"fidivl",TERM,M,1},	{"fidivrl",TERM,M,1} },
/*  [3,0]  */ { {"fildl",TERM,M,1},	INVALID,
		{"fistl",TERM,M,1},	{"fistpl",TERM,M,1},
/*  [3,4]  */	INVALID,		{"fldt",TERM,M,1},
		INVALID,		{"fstpt",TERM,M,1} },
/*  [4,0]  */ { {"faddl",TERM,M,1},	{"fmull",TERM,M,1},
		{"fcoml",TERM,M,1},	{"fcompl",TERM,M,1},
/*  [4,1]  */	{"fsubl",TERM,M,1},	{"fsubrl",TERM,M,1},
		{"fdivl",TERM,M,1},	{"fdivrl",TERM,M,1} },
/*  [5,0]  */ { {"fldl",TERM,M,1},	INVALID,
		{"fstl",TERM,M,1},	{"fstpl",TERM,M,1},
/*  [5,4]  */	{"frstor",TERM,M,1},	INVALID,
		{"fnsave",TERM,M,1},	{"fnstsw",TERM,M,1} },
/*  [6,0]  */ { {"fiadds",TERM,M,1},	{"fimuls",TERM,M,1},
		{"ficoms",TERM,M,1},	{"ficomps",TERM,M,1},
/*  [6,4]  */	{"fisubs",TERM,M,1},	{"fisubrs",TERM,M,1},
		{"fidivs",TERM,M,1},	{"fidivrs",TERM,M,1} },
/*  [7,0]  */ { {"filds",TERM,M,1},	INVALID,
		{"fists",TERM,M,1},	{"fistps",TERM,M,1},
/*  [7,4]  */	{"fbld",TERM,M,1},	{"fildq",TERM,M,1},
		{"fbstp",TERM,M,1},	{"fistpq",TERM,M,1} },
};

static const struct instable opFP3[8][8] = {
/* bit  pattern:	1101 1xxx 11xx xREG */
/*  [0,0]  */ { {"fadd",TERM,FF,0},	{"fmul",TERM,FF,0},
		{"fcom",TERM,F,0},	{"fcomp",TERM,F,0},
/*  [0,4]  */	{"fsub",TERM,FF,0},	{"fsubr",TERM,FF,0},
		{"fdiv",TERM,FF,0},	{"fdivr",TERM,FF,0} },
/*  [1,0]  */ { {"fld",TERM,F,0},	{"fxch",TERM,F,0},
		{"fnop",TERM,GO_ON,0},	{"fstp",TERM,F,0},
/*  [1,4]  */	INVALID,		INVALID,
		INVALID,		INVALID },
/*  [2,0]  */ { {"fcmovb",TERM,FF,0},	{"fcmove",TERM,FF,0},
		{"fcmovbe",TERM,FF,0},	{"fcmovu",TERM,FF,0},
/*  [2,4]  */	INVALID,		{"fucompp",TERM,GO_ON,0},
		INVALID,		INVALID },
/*  [3,0]  */ { {"fcmovnb",TERM,FF,0},	{"fcmovne",TERM,FF,0},
		{"fcmovnbe",TERM,FF,0},	{"fcmovnu",TERM,FF,0},
/*  [3,4]  */	INVALID,		{"fucomi",TERM,FF,0},
		{"fcomi",TERM,FF,0},	INVALID },
/*  [4,0]  */ { {"fadd",TERM,FF,0},	{"fmul",TERM,FF,0},
		{"fcom",TERM,F,0},	{"fcomp",TERM,F,0},
/*  [4,4]  */	{"fsub",TERM,FF,0},	{"fsubr",TERM,FF,0},
		{"fdiv",TERM,FF,0},	{"fdivr",TERM,FF,0} },
/*  [5,0]  */ { {"ffree",TERM,F,0},	{"fxch",TERM,F,0},
		{"fst",TERM,F,0},	{"fstp",TERM,F,0},
/*  [5,4]  */	{"fucom",TERM,F,0},	{"fucomp",TERM,F,0},
		INVALID,		INVALID },
/*  [6,0]  */ { {"faddp",TERM,FF,0},	{"fmulp",TERM,FF,0},
		{"fcomp",TERM,F,0},	{"fcompp",TERM,GO_ON,0},
/*  [6,4]  */	{"fsubp",TERM,FF,0},	{"fsubrp",TERM,FF,0},
		{"fdivp",TERM,FF,0},	{"fdivrp",TERM,FF,0} },
/*  [7,0]  */ { {"ffree",TERM,F,0},	{"fxch",TERM,F,0},
		{"fstp",TERM,F,0},	{"fstp",TERM,F,0},
/*  [7,4]  */	{"fnstsw",TERM,M,1},	{"fucomip",TERM,FF,0},
		{"fcomip",TERM,FF,0},	INVALID },
};

static const struct instable opFP4[4][8] = {
/* bit pattern:	1101 1001 111x xxxx */
/*  [0,0]  */ { {"fchs",TERM,GO_ON,0},	{"fabs",TERM,GO_ON,0},
		INVALID,		INVALID,
/*  [0,4]  */	{"ftst",TERM,GO_ON,0},	{"fxam",TERM,GO_ON,0},
		INVALID,		INVALID },
/*  [1,0]  */ { {"fld1",TERM,GO_ON,0},	{"fldl2t",TERM,GO_ON,0},
		{"fldl2e",TERM,GO_ON,0},{"fldpi",TERM,GO_ON,0},
/*  [1,4]  */	{"fldlg2",TERM,GO_ON,0},{"fldln2",TERM,GO_ON,0},
		{"fldz",TERM,GO_ON,0},	INVALID },
/*  [2,0]  */ { {"f2xm1",TERM,GO_ON,0},	{"fyl2x",TERM,GO_ON,0},
		{"fptan",TERM,GO_ON,0},	{"fpatan",TERM,GO_ON,0},
/*  [2,4]  */	{"fxtract",TERM,GO_ON,0},{"fprem1",TERM,GO_ON,0},
		{"fdecstp",TERM,GO_ON,0},{"fincstp",TERM,GO_ON,0} },
/*  [3,0]  */ { {"fprem",TERM,GO_ON,0},	{"fyl2xp1",TERM,GO_ON,0},
		{"fsqrt",TERM,GO_ON,0},	{"fsincos",TERM,GO_ON,0},
/*  [3,4]  */	{"frndint",TERM,GO_ON,0},{"fscale",TERM,GO_ON,0},
		{"fsin",TERM,GO_ON,0},	{"fcos",TERM,GO_ON,0} },
};

static const struct instable opFP5[8] = {
/* bit pattern:	1101 1011 1110 0xxx */
/*  [0]  */	INVALID,		INVALID,
		{"fnclex",TERM,GO_ON,0},{"fninit",TERM,GO_ON,0},
/*  [4]  */	{"fsetpm",TERM,GO_ON,0},INVALID,
		INVALID,		INVALID,
};

/*
 * Main decode table for the op codes.  The first two nibbles
 * will be used as an index into the table.  If there is a
 * a need to further decode an instruction, the array to be
 * referenced is indicated with the other two entries being
 * empty.
 */
static const struct instable distable[16][16] = {
/* [0,0] */  {  {"addb",TERM,RMw,0},	{"add",TERM,RMw,1},
		{"addb",TERM,MRw,0},	{"add",TERM,MRw,1},
/* [0,4] */	{"addb",TERM,IA,0},	{"add",TERM,IA,1},
		{"push",TERM,SEG,1},	{"pop",TERM,SEG,1},
/* [0,8] */	{"orb",TERM,RMw,0},	{"or",TERM,RMw,1},
		{"orb",TERM,MRw,0},	{"or",TERM,MRw,1},
/* [0,C] */	{"orb",TERM,IA,0},	{"or",TERM,IA,1},
		{"push",TERM,SEG,1},
		{"",(const struct instable *)op0F,TERM,0} },
/* [1,0] */  {  {"adcb",TERM,RMw,0},	{"adc",TERM,RMw,1},
		{"adcb",TERM,MRw,0},	{"adc",TERM,MRw,1},
/* [1,4] */	{"adcb",TERM,IA,0},	{"adc",TERM,IA,1},
		{"push",TERM,SEG,1},	{"pop",TERM,SEG,1},
/* [1,8] */	{"sbbb",TERM,RMw,0},	{"sbb",TERM,RMw,1},
		{"sbbb",TERM,MRw,0},	{"sbb",TERM,MRw,1},
/* [1,C] */	{"sbbb",TERM,IA,0},	{"sbb",TERM,IA,1},
		{"push",TERM,SEG,1},	{"pop",TERM,SEG,1} },
/* [2,0] */  {  {"andb",TERM,RMw,0},	{"and",TERM,RMw,1},
		{"andb",TERM,MRw,0},	{"and",TERM,MRw,1},
/* [2,4] */	{"andb",TERM,IA,0},	{"and",TERM,IA,1},
		{"%es:",TERM,OVERRIDE,0}, {"daa",TERM,GO_ON,0},
/* [2,8] */	{"subb",TERM,RMw,0},	{"sub",TERM,RMw,1},
		{"subb",TERM,MRw,0},	{"sub",TERM,MRw,1},
/* [2,C] */	{"subb",TERM,IA,0},	{"sub",TERM,IA,1},
		{"%cs:",TERM,OVERRIDE,0}, {"das",TERM,GO_ON,0} },
/* [3,0] */  {  {"xorb",TERM,RMw,0},	{"xor",TERM,RMw,1},
		{"xorb",TERM,MRw,0},	{"xor",TERM,MRw,1},
/* [3,4] */	{"xorb",TERM,IA,0},	{"xor",TERM,IA,1},
		{"%ss:",TERM,OVERRIDE,0}, {"aaa",TERM,GO_ON,0},
/* [3,8] */	{"cmpb",TERM,RMw,0},	{"cmp",TERM,RMw,1},
		{"cmpb",TERM,MRw,0},	{"cmp",TERM,MRw,1},
/* [3,C] */	{"cmpb",TERM,IA,0},	{"cmp",TERM,IA,1},
		{"%ds:",TERM,OVERRIDE,0}, {"aas",TERM,GO_ON,0} },
/* [4,0] */  {  {"inc",TERM,R,1},	{"inc",TERM,R,1},
		{"inc",TERM,R,1},	{"inc",TERM,R,1},
/* [4,4] */	{"inc",TERM,R,1},	{"inc",TERM,R,1},
		{"inc",TERM,R,1},	{"inc",TERM,R,1},
/* [4,8] */	{"dec",TERM,R,1},	{"dec",TERM,R,1},
		{"dec",TERM,R,1},	{"dec",TERM,R,1},
/* [4,C] */	{"dec",TERM,R,1},	{"dec",TERM,R,1},
		{"dec",TERM,R,1},	{"dec",TERM,R,1} },
/* [5,0] */  {  {"push",TERM,R,1},	{"push",TERM,R,1},
		{"push",TERM,R,1},	{"push",TERM,R,1},
/* [5,4] */	{"push",TERM,R,1},	{"push",TERM,R,1},
		{"push",TERM,R,1},	{"push",TERM,R,1},
/* [5,8] */	{"pop",TERM,R,1},	{"pop",TERM,R,1},
		{"pop",TERM,R,1},	{"pop",TERM,R,1},
/* [5,C] */	{"pop",TERM,R,1},	{"pop",TERM,R,1},
		{"pop",TERM,R,1},	{"pop",TERM,R,1} },
/* [6,0] */  {  {"pusha",TERM,GO_ON,1},	{"popa",TERM,GO_ON,1},
		{"bound",TERM,MR,1},	{"arpl",TERM,RMw,0},
/* [6,4] */	{"%fs:",TERM,OVERRIDE,0}, {"%gs:",TERM,OVERRIDE,0},
		{"data16",TERM,DM,0},	{"addr16",TERM,AM,0},
/* [6,8] */	{"push",TERM,I,1},	{"imul",TERM,IMUL,1},
		{"push",TERM,Ib,1},	{"imul",TERM,IMUL,1},
/* [6,C] */	{"insb",TERM,GO_ON,0},	{"ins",TERM,GO_ON,1},
		{"outsb",TERM,GO_ON,0},	{"outs",TERM,GO_ON,1} },
/* [7,0] */  {  {"jo",TERM,BD,0},	{"jno",TERM,BD,0},
		{"jb",TERM,BD,0},	{"jae",TERM,BD,0},
/* [7,4] */	{"je",TERM,BD,0},	{"jne",TERM,BD,0},
		{"jbe",TERM,BD,0},	{"ja",TERM,BD,0},
/* [7,8] */	{"js",TERM,BD,0},	{"jns",TERM,BD,0},
		{"jp",TERM,BD,0},	{"jnp",TERM,BD,0},
/* [7,C] */	{"jl",TERM,BD,0},	{"jge",TERM,BD,0},
		{"jle",TERM,BD,0},	{"jg",TERM,BD,0} },
/* [8,0] */  {  {"",op80,TERM,0},	{"",op81,TERM,0},
		{"",op82,TERM,0},	{"",op83,TERM,0},
/* [8,4] */	{"testb",TERM,MRw,0},	{"test",TERM,MRw,1},
		{"xchgb",TERM,MRw,0},	{"xchg",TERM,MRw,1},
/* [8,8] */	{"movb",TERM,RMw,0},	{"mov",TERM,RMw,1},
		{"movb",TERM,MRw,0},	{"mov",TERM,MRw,1},
/* [8,C] */	{"mov",TERM,SM,1},	{"lea",TERM,MR,1},
		{"mov",TERM,MS,1},	{"pop",TERM,M,1} },
/* [9,0] */  {  {"nop",TERM,GO_ON,0},	{"xchg",TERM,RA,1},
		{"xchg",TERM,RA,1},	{"xchg",TERM,RA,1},
/* [9,4] */	{"xchg",TERM,RA,1},	{"xchg",TERM,RA,1},
		{"xchg",TERM,RA,1},	{"xchg",TERM,RA,1},
/* [9,8] */	{"",TERM,CBW,0},	{"",TERM,CWD,0},
		{"lcall",TERM,SO,0},	{"wait/",TERM,PREFIX,0},
/* [9,C] */	{"pushf",TERM,GO_ON,1},	{"popf",TERM,GO_ON,1},
		{"sahf",TERM,GO_ON,0},	{"lahf",TERM,GO_ON,0} },
/* [A,0] */  {  {"movb",TERM,OA,0},	{"mov",TERM,OA,1},
		{"movb",TERM,AO,0},	{"mov",TERM,AO,1},
/* [A,4] */	{"movsb",TERM,SD,0},	{"movs",TERM,SD,1},
		{"cmpsb",TERM,SD,0},	{"cmps",TERM,SD,1},
/* [A,8] */	{"testb",TERM,IA,0},	{"test",TERM,IA,1},
		{"stosb",TERM,AD,0},	{"stos",TERM,AD,1},
/* [A,C] */	{"lodsb",TERM,SA,0},	{"lods",TERM,SA,1},
		{"scasb",TERM,AD,0},	{"scas",TERM,AD,1} },
/* [B,0] */  {  {"movb",TERM,IR,0},	{"movb",TERM,IR,0},
		{"movb",TERM,IR,0},	{"movb",TERM,IR,0},
/* [B,4] */	{"movb",TERM,IR,0},	{"movb",TERM,IR,0},
		{"movb",TERM,IR,0},	{"movb",TERM,IR,0},
/* [B,8] */	{"mov",TERM,IR,1},	{"mov",TERM,IR,1},
		{"mov",TERM,IR,1},	{"mov",TERM,IR,1},
/* [B,C] */	{"mov",TERM,IR,1},	{"mov",TERM,IR,1},
		{"mov",TERM,IR,1},	{"mov",TERM,IR,1} },
/* [C,0] */  {  {"",opC0,TERM,0},	{"",opC1,TERM,0},
		{"ret",TERM,RET,0},	{"ret",TERM,GO_ON,0},
/* [C,4] */	{"les",TERM,MR,0},	{"lds",TERM,MR,0},
		{"movb",TERM,IMw,0},	{"mov",TERM,IMw,1},
/* [C,8] */	{"enter",TERM,ENTER,0},	{"leave",TERM,GO_ON,0},
		{"lret",TERM,RET,0},	{"lret",TERM,GO_ON,0},
/* [C,C] */	{"int",TERM,INT3,0},	{"int",TERM,Ib,0},
		{"into",TERM,GO_ON,0},	{"iret",TERM,GO_ON,0} },
/* [D,0] */  {  {"",opD0,TERM,0},	{"",opD1,TERM,0},
		{"",opD2,TERM,0},	{"",opD3,TERM,0},
/* [D,4] */	{"aam",TERM,U,0},	{"aad",TERM,U,0},
		{"falc",TERM,GO_ON,0},	{"xlat",TERM,GO_ON,0},
/* 287 instructions.  Note that although the indirect field		*/
/* indicates opFP1n2 for further decoding, this is not necessarily	*/
/* the case since the opFP arrays are not partitioned according to key1	*/
/* and key2.  opFP1n2 is given only to indicate that we haven't		*/
/* finished decoding the instruction.					*/
/* [D,8] */	{"",(const struct instable *)opFP1n2,TERM,0},
		{"",(const struct instable *)opFP1n2,TERM,0},
		{"",(const struct instable *)opFP1n2,TERM,0},
		{"",(const struct instable *)opFP1n2,TERM,0},
/* [D,C] */	{"",(const struct instable *)opFP1n2,TERM,0},
		{"",(const struct instable *)opFP1n2,TERM,0},
		{"",(const struct instable *)opFP1n2,TERM,0},
		{"",(const struct instable *)opFP1n2,TERM,0} },
/* [E,0] */  {  {"loopnz",TERM,BD,0},	{"loopz",TERM,BD,0},
		{"loop",TERM,BD,0},	{"jcxz",TERM,BD,0},
/* [E,4] */	{"inb",TERM,Pi,0},	{"in",TERM,Pi,1},
		{"outb",TERM,Po,0},	{"out",TERM,Po,1},
/* [E,8] */	{"call",TERM,D,1},	{"jmp",TERM,D,1},
		{"ljmp",TERM,SO,0},	{"jmp",TERM,BD,0},
/* [E,C] */	{"inb",TERM,Vi,0},	{"in",TERM,Vi,1},
		{"outb",TERM,Vo,0},	{"out",TERM,Vo,1} },
/* [F,0] */  {  {"lock/",TERM,PREFIX,0}, INVALID,
		{"repnz/",TERM,PREFIX,0}, {"repz/",TERM,PREFIX,0},
/* [F,4] */	{"hlt",TERM,GO_ON,0},	{"cmc",TERM,GO_ON,0},
		{"",opF6,TERM,0},	{"",opF7,TERM,0},
/* [F,8] */	{"clc",TERM,GO_ON,0},	{"stc",TERM,GO_ON,0},
		{"cli",TERM,GO_ON,0},	{"sti",TERM,GO_ON,0},
/* [F,C] */	{"cld",TERM,GO_ON,0},	{"std",TERM,GO_ON,0},
		{"",opFE,TERM,0},	{"",opFF,TERM,0} },
};

/*
 * i386_disassemble()
 */
unsigned long
i386_disassemble(
char *sect,
unsigned long left,
unsigned long addr,
unsigned long sect_addr,
enum byte_sex object_byte_sex,
struct relocation_info *sorted_relocs,
unsigned long nsorted_relocs,
struct nlist *symbols,
unsigned long nsymbols,
struct nlist *sorted_symbols,
unsigned long nsorted_symbols,
char *strings,
unsigned long strings_size,
unsigned long *indirect_symbols,
unsigned long nindirect_symbols,
struct mach_header *mh,
struct load_command *load_commands,
enum bool verbose)
{
    char mnemonic[MAX_MNEMONIC+2]; /* one extra for suffix */
    const char *seg;
    const char *symbol0, *symbol1;
    const char *symadd0, *symsub0, *symadd1, *symsub1;
    unsigned long value0, value1;
    unsigned long value0_size, value1_size;
    char result0[MAX_RESULT], result1[MAX_RESULT];
    const char *indirect_symbol_name;

    unsigned long length;
    unsigned char byte;
    /* nibbles (4 bits) of the opcode */
    unsigned opcode1, opcode2, opcode3, opcode4, opcode5;
    const struct instable *dp;
    unsigned long wbit, vbit;
    enum bool got_modrm_byte;
    unsigned long mode, reg, r_m;
    const char *reg_name;
    enum bool data16;		/* 16- or 32-bit data */
    enum bool addr16;		/* 16- or 32-bit addressing */

	if(left == 0){
	   printf("(end of section)\n");
	   return(0);
	}

	memset(mnemonic, '\0', sizeof(mnemonic));
	seg = "";
	symbol0 = NULL;
	symbol1 = NULL;
	value0 = 0;
	value1 = 0;
	value0_size = 0;
	value1_size = 0;
	memset(result0, '\0', sizeof(result0));
	memset(result1, '\0', sizeof(result1));
	data16 = FALSE;
	addr16 = FALSE;
	reg_name = NULL;

	length = 0;
	byte = 0;
	opcode4 = 0; /* to remove a compiler warning only */
	opcode5 = 0; /* to remove a compiler warning only */

	/*
	 * As long as there is a prefix, the default segment register,
	 * addressing-mode, or data-mode in the instruction will be overridden.
	 * This may be more general than the chip actually is.
	 */
	for(;;){
	    byte = get_value(sizeof(char), sect, &length, &left);
	    opcode1 = byte >> 4 & 0xf;
	    opcode2 = byte & 0xf;

	    dp = &distable[opcode1][opcode2];

	    if(dp->adr_mode == PREFIX)
		printf(dp->name);
	    else if(dp->adr_mode == AM)
		addr16 = !addr16;
	    else if(dp->adr_mode == DM)
		data16 = !data16;
	    else if(dp->adr_mode == OVERRIDE)
		seg = dp->name;
	    else
		break;
	}

	/*
	 * Some 386 instructions have 2 bytes of opcode before the mod_r/m
	 * byte so we need to perform a table indirection.
	 */
	if(dp->indirect == (const struct instable *)op0F){
	    byte = get_value(sizeof(char), sect, &length, &left);
	    opcode4 = byte >> 4 & 0xf;
	    opcode5 = byte & 0xf;
	    dp = &op0F[opcode4][opcode5];
	}

	got_modrm_byte = FALSE;
	if(dp->indirect != TERM){
	    /*
	     * This must have been an opcode for which several instructions
	     * exist.  The opcode3 field further decodes the instruction.
	     */
	    got_modrm_byte = TRUE;
	    byte = get_value(sizeof(char), sect, &length, &left);
	    modrm_byte(&mode, (unsigned long *)&opcode3, &r_m, byte);
	    /*
	     * decode 287 instructions (D8-DF) from opcodeN
	     */
	    if(opcode1 == 0xD && opcode2 >= 0x8){
		/* instruction form 5 */
		if(opcode2 == 0xB && mode == 0x3 && opcode3 == 4)
		    dp = &opFP5[r_m];
		else if(opcode2 == 0xB && mode == 0x3 && opcode3 > 6){
		    printf(".byte 0x%01x%01x, 0x%01x%01x 0x%02x #bad opcode\n",
			   (unsigned int)opcode1, (unsigned int)opcode2,
			   (unsigned int)opcode4, (unsigned int)opcode5,
			   (unsigned int)byte);
		    return(length);
		}
		/* instruction form 4 */
		else if(opcode2 == 0x9 && mode == 0x3 && opcode3 >= 4)
		    dp = &opFP4[opcode3-4][r_m];
		/* instruction form 3 */
		else if(mode == 0x3)
		    dp = &opFP3[opcode2-8][opcode3];
		else /* instruction form 1 and 2 */
		    dp = &opFP1n2[opcode2-8][opcode3];
	    }
	    else
		dp = dp->indirect + opcode3;
		/* now dp points the proper subdecode table entry */
	}

	if(dp->indirect != TERM){
	    printf(".byte 0x%02x #bad opcode\n", (unsigned int)byte);
	    return(length);
	}

	/* setup the mnemonic with a possible suffix */
	if(dp->adr_mode != CBW && dp->adr_mode != CWD){
	    if(dp->suffix){
		if(data16 == TRUE)
		    sprintf(mnemonic, "%sw", dp->name);
		else
		    sprintf(mnemonic, "%sl", dp->name);
	    }
	    else{
		sprintf(mnemonic, "%s", dp->name);
	    }
	}

	/*
	 * Each instruction has a particular instruction syntax format
	 * stored in the disassembly tables.  The assignment of formats
	 * to instructions was made by the author.  Individual formats
	 * are explained as they are encountered in the following
	 * switch construct.
	 */
	switch(dp -> adr_mode){

	case BSWAP:
	    reg_name = REG32[(opcode5 & 0x7)][1];
	    printf("%s\t%s\n", mnemonic, reg_name);
	    return(length);

	case XINST:
	    wbit = WBIT(opcode5);
	    if(got_modrm_byte == FALSE){
		got_modrm_byte = TRUE;
		byte = get_value(sizeof(char), sect, &length, &left);
		modrm_byte(&mode, &reg, &r_m, byte);
	    }
	    GET_OPERAND(&symadd0, &symsub0, &value0, &value0_size, result0);
	    if(data16 == TRUE)
		reg_name = REG16[reg][wbit];
	    else
		reg_name = REG32[reg][wbit];
	    printf("%s\t%s,", mnemonic, reg_name);
	    print_operand(seg, symadd0, symsub0, value0, value0_size, result0,
			  "\n");
	    return(length);

	/* movsbl movsbw (0x0FBE) or movswl (0x0FBF) */
	/* movzbl movzbw (0x0FB6) or mobzwl (0x0FB7) */
	/* wbit lives in 2nd byte, note that operands are different sized */
	case MOVZ:
	    /* Get second operand first so data16 can be destroyed */
	    if(got_modrm_byte == FALSE){
		got_modrm_byte = TRUE;
		byte = get_value(sizeof(char), sect, &length, &left);
		modrm_byte(&mode, &reg, &r_m, byte);
	    }
	    if(data16 == TRUE)
		reg_name = REG16[reg][LONGOPERAND];
	    else
		reg_name = REG32[reg][LONGOPERAND];
	    wbit = WBIT(opcode5);
	    data16 = 1;
	    GET_OPERAND(&symadd0, &symsub0, &value0, &value0_size, result0);
	    printf("%s\t", mnemonic);
	    print_operand(seg, symadd0, symsub0, value0, value0_size, result0,
			  ",");
	    printf("%s\n", reg_name);
	    return(length);

	/* imul instruction, with either 8-bit or longer immediate */
	case IMUL:
	    if(got_modrm_byte == FALSE){
		got_modrm_byte = TRUE;
		byte = get_value(sizeof(char), sect, &length, &left);
		modrm_byte(&mode, &reg, &r_m, byte);
	    }
	    wbit = LONGOPERAND;
	    GET_OPERAND(&symadd1, &symsub1, &value1, &value1_size, result1);
	    /* opcode 0x6B for byte, sign-extended displacement,
		0x69 for word(s) */
	    value0_size = OPSIZE(data16, opcode2 == 0x9);
	    IMMEDIATE(&symadd0, &symsub0, &value0, value0_size);
	    if(data16 == TRUE)
		reg_name = REG16[reg][wbit];
	    else
		reg_name = REG32[reg][wbit];
	    printf("%s\t$", mnemonic);
	    print_operand("", symadd0, symsub0, value0, value0_size, "", ",");
	    print_operand(seg, symadd1, symsub1, value1, value1_size, result1,
			  ",");
	    printf("%s\n", reg_name);
	    return(length);

	/* memory or register operand to register, with 'w' bit	*/
	case MRw:
	    wbit = WBIT(opcode2);
	    if(got_modrm_byte == FALSE){
		got_modrm_byte = TRUE;
		byte = get_value(sizeof(char), sect, &length, &left);
		modrm_byte(&mode, &reg, &r_m, byte);
	    }
	    GET_OPERAND(&symadd0, &symsub0, &value0, &value0_size, result0);
	    if(data16 == TRUE)
		reg_name = REG16[reg][wbit];
	    else
		reg_name = REG32[reg][wbit];
	    printf("%s\t", mnemonic);
	    print_operand(seg, symadd0, symsub0, value0, value0_size, result0,
			  ",");
	    printf("%s\n", reg_name);
	    return(length);

	/* register to memory or register operand, with 'w' bit	*/
	/* arpl happens to fit here also because it is odd */
	case RMw:
	    wbit = WBIT(opcode2);
	    if(got_modrm_byte == FALSE){
		got_modrm_byte = TRUE;
		byte = get_value(sizeof(char), sect, &length, &left);
		modrm_byte(&mode, &reg, &r_m, byte);
	    }
	    GET_OPERAND(&symadd0, &symsub0, &value0, &value0_size, result0);
	    if(data16 == TRUE)
		reg_name = REG16[reg][wbit];
	    else
		reg_name = REG32[reg][wbit];
	    printf("%s\t%s,", mnemonic, reg_name);
	    print_operand(seg, symadd0, symsub0, value0, value0_size, result0,
			  "\n");
	    return(length);

	/* Double shift. Has immediate operand specifying the shift. */
	case DSHIFT:
	    if(got_modrm_byte == FALSE){
		got_modrm_byte = TRUE;
		byte = get_value(sizeof(char), sect, &length, &left);
		modrm_byte(&mode, &reg, &r_m, byte);
	    }
	    wbit = LONGOPERAND;
	    GET_OPERAND(&symadd1, &symsub1, &value1, &value1_size, result1);
	    value0_size = sizeof(char);
	    IMMEDIATE(&symadd0, &symsub0, &value0, value0_size);
	    if(data16 == TRUE)
		reg_name = REG16[reg][wbit];
	    else
		reg_name = REG32[reg][wbit];
	    printf("%s\t$", mnemonic);
	    print_operand("", symadd0, symsub0, value0, value0_size, "", ",");
	    printf("%s,", reg_name);
	    print_operand(seg, symadd1, symsub1, value1, value1_size, result1,
			  "\n");
	    return(length);

	/* Double shift. With no immediate operand, specifies using %cl. */
	case DSHIFTcl:
	    if(got_modrm_byte == FALSE){
		got_modrm_byte = TRUE;
		byte = get_value(sizeof(char), sect, &length, &left);
		modrm_byte(&mode, &reg, &r_m, byte);
	    }
	    wbit = LONGOPERAND;
	    GET_OPERAND(&symadd0, &symsub0, &value0, &value0_size, result0);
	    if(data16 == TRUE)
		reg_name = REG16[reg][wbit];
	    else
		reg_name = REG32[reg][wbit];
	    printf("%s\t%%cl,%s,", mnemonic, reg_name);
	    print_operand(seg, symadd0, symsub0, value0, value0_size, result0,
			  "\n");
	    return(length);

	/* immediate to memory or register operand */
	case IMlw:
	    wbit = WBIT(opcode2);
	    GET_OPERAND(&symadd1, &symsub1, &value1, &value1_size, result1);
	    /* A long immediate is expected for opcode 0x81, not 0x80 & 0x83 */
	    value0_size = OPSIZE(data16, opcode2 == 1);
	    IMMEDIATE(&symadd0, &symsub0, &value0, value0_size);
	    printf("%s\t$", mnemonic);
	    print_operand("", symadd0, symsub0, value0, value0_size, "", ",");
	    print_operand(seg, symadd1, symsub1, value1, value1_size, result1,
			  "\n");
	    return(length);

	/* immediate to memory or register operand with the 'w' bit present */
	case IMw:
	    if(got_modrm_byte == FALSE){
		got_modrm_byte = TRUE;
		byte = get_value(sizeof(char), sect, &length, &left);
		modrm_byte(&mode, &reg, &r_m, byte);
	    }
	    wbit = WBIT(opcode2);
	    GET_OPERAND(&symadd1, &symsub1, &value1, &value1_size, result1);
	    value0_size = OPSIZE(data16, wbit);
	    IMMEDIATE(&symadd0, &symsub0, &value0, value0_size);
	    printf("%s\t$", mnemonic);
	    print_operand("", symadd0, symsub0, value0, value0_size, "", ",");
	    print_operand(seg, symadd1, symsub1, value1, value1_size, result1,
			  "\n");
	    return(length);

	/* immediate to register with register in low 3 bits of op code */
	case IR:
	    wbit = (opcode2 >> 3) & 0x1; /* w-bit here (with regs) is bit 3 */
	    reg = REGNO(opcode2);
	    value0_size = OPSIZE(data16, wbit);
	    IMMEDIATE(&symadd0, &symsub0, &value0, value0_size);
	    if(data16 == TRUE)
		reg_name = REG16[reg][wbit];
	    else
		reg_name = REG32[reg][wbit];
	    printf("%s\t$", mnemonic);
	    print_operand("", symadd0, symsub0, value0, value0_size, "", ",");
	    printf("%s\n", reg_name);
	    return(length);

	/* memory operand to accumulator */
	case OA:
	    value0_size = OPSIZE(data16, LONGOPERAND);
	    IMMEDIATE(&symadd0, &symsub0, &value0, value0_size);
	    printf("%s\t", mnemonic);
	    print_operand(seg, symadd0, symsub0, value0, value0_size, "", ",");
	    wbit = WBIT(opcode2);
	    reg_name = (data16 ? REG16 : REG32)[0][wbit];
	    printf("%s\n", reg_name);
	    return(length);

	/* accumulator to memory operand */
	case AO:
	    value0_size = OPSIZE(data16, LONGOPERAND);
	    IMMEDIATE(&symadd0, &symsub0, &value0, value0_size);
	    wbit = WBIT(opcode2);
	    reg_name = (data16 ? REG16 : REG32)[0][wbit];
	    printf("%s\t%s,", mnemonic, reg_name);
	    print_operand(seg, symadd0, symsub0, value0, value0_size, "", "\n");
	    return(length);

	/* memory or register operand to segment register */
	case MS:
	    if(got_modrm_byte == FALSE){
		got_modrm_byte = TRUE;
		byte = get_value(sizeof(char), sect, &length, &left);
		modrm_byte(&mode, &reg, &r_m, byte);
	    }
	    wbit = LONGOPERAND;
	    GET_OPERAND(&symadd0, &symsub0, &value0, &value0_size, result0);
	    printf("%s\t", mnemonic);
	    print_operand(seg, symadd0, symsub0, value0, value0_size, result0,
			  ",");
	    printf("%s\n", SEGREG[reg]);
	    return(length);

	/* segment register to memory or register operand	*/
	case SM:
	    if(got_modrm_byte == FALSE){
		got_modrm_byte = TRUE;
		byte = get_value(sizeof(char), sect, &length, &left);
		modrm_byte(&mode, &reg, &r_m, byte);
	    }
	    wbit = LONGOPERAND;
	    GET_OPERAND(&symadd0, &symsub0, &value0, &value0_size, result0);
	    printf("%s\t%s,", mnemonic, SEGREG[reg]);
	    print_operand(seg, symadd0, symsub0, value0, value0_size, result0,
			  "\n");
	    return(length);

	/* rotate or shift instrutions, which may shift by 1 or */
	/* consult the cl register, depending on the 'v' bit	*/
	case Mv:
	    vbit = VBIT(opcode2);
	    wbit = WBIT(opcode2);
	    GET_OPERAND(&symadd0, &symsub0, &value0, &value0_size, result0);
	    /* When vbit is set, register is an operand, otherwise just $0x1 */
	    reg_name = vbit ? "%cl," : "" ;
	    printf("%s\t%s", mnemonic, reg_name);
	    print_operand(seg, symadd0, symsub0, value0, value0_size, result0,
			  "\n");
	    return(length);

	/* immediate rotate or shift instrutions, which may or */
	/* may not consult the cl register, depending on the 'v' bit */
	case MvI:
	    vbit = VBIT(opcode2);
	    wbit = WBIT(opcode2);
	    GET_OPERAND(&symadd0, &symsub0, &value0, &value0_size, result0);
	    value1_size = sizeof(char);
	    IMMEDIATE(&symadd1, &symsub1, &value1, value1_size);
	    /* When vbit is set, register is an operand, otherwise just $0x1 */
	    reg_name = vbit ? "%cl," : "" ;
	    printf("%s\t$", mnemonic);
	    print_operand("", symadd1, symsub1, value1, value1_size, "", ",");
	    printf("%s", reg_name);
	    print_operand(seg, symadd0, symsub0, value0, value0_size, result0,
			  "\n");
	    return(length);

	case MIb:
	    wbit = LONGOPERAND;
	    GET_OPERAND(&symadd0, &symsub0, &value0, &value0_size, result0);
	    value1_size = sizeof(char);
	    IMMEDIATE(&symadd1, &symsub1, &value1, value1_size);
	    printf("%s\t$", mnemonic);
	    print_operand("", symadd1, symsub1, value1, value1_size, "", ",");
	    print_operand(seg, symadd0, symsub0, value0, value0_size, result0,
			  "\n");
	    return(length);

	/* single memory or register operand with 'w' bit present */
	case Mw:
	    wbit = WBIT(opcode2);
	    GET_OPERAND(&symadd0, &symsub0, &value0, &value0_size, result0);
	    printf("%s\t", mnemonic);
	    print_operand(seg, symadd0, symsub0, value0, value0_size, result0,
			  "\n");
	    return(length);

	/* single memory or register operand */
	case M:
	    if(got_modrm_byte == FALSE){
		got_modrm_byte = TRUE;
		byte = get_value(sizeof(char), sect, &length, &left);
		modrm_byte(&mode, &reg, &r_m, byte);
	    }
	    wbit = LONGOPERAND;
	    GET_OPERAND(&symadd0, &symsub0, &value0, &value0_size, result0);
	    printf("%s\t", mnemonic);
	    print_operand(seg, symadd0, symsub0, value0, value0_size, result0,
			  "\n");
	    return(length);

	/* single memory or register operand */
	case Mb:
	    if(got_modrm_byte == FALSE){
		got_modrm_byte = TRUE;
		byte = get_value(sizeof(char), sect, &length, &left);
		modrm_byte(&mode, &reg, &r_m, byte);
	    }
	    wbit = BYTEOPERAND;
	    GET_OPERAND(&symadd0, &symsub0, &value0, &value0_size, result0);
	    printf("%s\t", mnemonic);
	    print_operand(seg, symadd0, symsub0, value0, value0_size, result0,
			  "\n");
	    return(length);

	case SREG: /* special register */
	    byte = get_value(sizeof(char), sect, &length, &left);
	    modrm_byte(&mode, &reg, &r_m, byte);
	    vbit = 0;
	    switch(opcode5){
	    case 2:
		vbit = 1;
		/* fall thru */
	    case 0: 
		reg_name = CONTROLREG[reg];
		break;
	    case 3:
		vbit = 1;
		/* fall thru */
	    case 1:
		reg_name = DEBUGREG[reg];
		break;
	    case 6:
		vbit = 1;
		/* fall thru */
	    case 4:
		reg_name = TESTREG[reg];
		break;
	    }
	    if(vbit)
		printf("%s\t%s,%s\n", mnemonic, REG32[r_m][1], reg_name);
	    else
		printf("%s\t%s,%s\n", mnemonic, reg_name, REG32[r_m][1]);
	    return(length);

	/* single register operand with register in the low 3	*/
	/* bits of op code					*/
	case R:
	    reg = REGNO(opcode2);
	    if(data16 == TRUE)
		reg_name = REG16[reg][LONGOPERAND];
	    else
		reg_name = REG32[reg][LONGOPERAND];
	    printf("%s\t%s\n", mnemonic, reg_name);
	    return(length);

	/* register to accumulator with register in the low 3	*/
	/* bits of op code, xchg instructions                   */
	case RA:
	    reg = REGNO(opcode2);
	    if(data16 == TRUE){
		reg_name = REG16[reg][LONGOPERAND];
		printf("%s\t%s,%%ax\n", mnemonic, reg_name);
	    }
	    else{
		reg_name = REG32[reg][LONGOPERAND];
		printf("%s\t%s,%%eax\n", mnemonic, reg_name);
	    }
	    return(length);

	/* single segment register operand, with reg in bits 3-4 of op code */
	case SEG:
	    reg = byte >> 3 & 0x3; /* segment register */
	    printf("%s\t%s\n", mnemonic, SEGREG[reg]);
	    return(length);

	/* single segment register operand, with register in	*/
	/* bits 3-5 of op code					*/
	case LSEG:
	    reg = byte >> 3 & 0x7; /* long seg reg from opcode */
	    printf("%s\t%s\n", mnemonic, SEGREG[reg]);
	    return(length);

	/* memory or register operand to register */
	case MR:
	    if(got_modrm_byte == FALSE){
		got_modrm_byte = TRUE;
		byte = get_value(sizeof(char), sect, &length, &left);
		modrm_byte(&mode, &reg, &r_m, byte);
	    }
	    wbit = LONGOPERAND;
	    GET_OPERAND(&symadd0, &symsub0, &value0, &value0_size, result0);
	    if(data16 == TRUE)
		reg_name = REG16[reg][wbit];
	    else
		reg_name = REG32[reg][wbit];
	    printf("%s\t", mnemonic);
	    print_operand(seg, symadd0, symsub0, value0, value0_size, result0,
			  ",");
	    printf("%s\n", reg_name);
	    return(length);

	/* immediate operand to accumulator */
	case IA:
	    value0_size = OPSIZE(data16, WBIT(opcode2));
	    switch(value0_size) {
		case 1: reg_name = "%al"; break;
		case 2: reg_name = "%ax"; break;
		case 4: reg_name = "%eax"; break;
	    }
	    IMMEDIATE(&symadd0, &symsub0, &value0, value0_size);
	    printf("%s\t$", mnemonic);
	    print_operand("", symadd0, symsub0, value0, value0_size, "", ",");
	    printf("%s\n", reg_name);
	    return(length);

	/* memory or register operand to accumulator */
	case MA:
	    wbit = WBIT(opcode2);
	    GET_OPERAND(&symadd0, &symsub0, &value0, &value0_size, result0);
	    printf("%s\t", mnemonic);
	    print_operand(seg, symadd0, symsub0, value0, value0_size, result0,
			  "\n");
	    return(length);

	/* si register to di register */
	case SD:
	    if(addr16 == TRUE)
		printf("%s\t%s(%%si),(%%di)\n", mnemonic, seg);
	    else
		printf("%s\t%s(%%esi),(%%edi)\n", mnemonic, seg);
	    return(length);

	/* accumulator to di register */
	case AD:
	    wbit = WBIT(opcode2);
	    reg_name = (data16 ? REG16 : REG32)[0][wbit];
	    if(addr16 == TRUE)
		printf("%s\t%s,%s(%%di)\n", mnemonic, reg_name, seg);
	    else
		printf("%s\t%s,%s(%%edi)\n", mnemonic, reg_name, seg);
	    return(length);

	/* si register to accumulator */
	case SA:
	    wbit = WBIT(opcode2);
	    reg_name = (data16 ? REG16 : REG32)[0][wbit];
	    if(addr16 == TRUE)
		printf("%s\t%s(%%si),%s\n", mnemonic, seg, reg_name);
	    else
		printf("%s\t%s(%%esi),%s\n", mnemonic, seg, reg_name);
	    return(length);

	/* single operand, a 16/32 bit displacement */
	case D:
	    value0_size = OPSIZE(data16, LONGOPERAND);
	    DISPLACEMENT(&symadd0, &symsub0, &value0, value0_size);
	    printf("%s\t", mnemonic);
	    print_operand(seg, symadd0, symsub0, value0, value0_size, "", "");
	    if(verbose){
		indirect_symbol_name = guess_indirect_symbol(value0,
		    mh, load_commands, object_byte_sex, indirect_symbols,
		    nindirect_symbols, symbols, nsymbols, strings,strings_size);
		if(indirect_symbol_name != NULL)
		    printf("\t; symbol stub for: %s", indirect_symbol_name);
	    }
	    printf("\n");
	    return(length);

	/* indirect to memory or register operand */
	case INM:
	    wbit = LONGOPERAND;
	    GET_OPERAND(&symadd0, &symsub0, &value0, &value0_size, result0);
	    if((mode == 0 && (r_m == 5 || r_m == 4)) || mode == 1 || mode == 2)
		printf("%s\t*", mnemonic);
	    else
		printf("%s\t", mnemonic);
	    print_operand(seg, symadd0, symsub0, value0, value0_size, result0,
			  "\n");
	    return(length);

	/* indirect to memory or register operand (for lcall and ljmp) */
	case INMl:
	    wbit = LONGOPERAND;
	    GET_OPERAND(&symadd0, &symsub0, &value0, &value0_size, result0);
	    printf("%s\t", mnemonic);
	    print_operand(seg, symadd0, symsub0, value0, value0_size, result0,
			  "\n");
	    return(length);

	/*
	 * For long jumps and long calls -- a new code segment
	 * register and an offset in IP -- stored in object
	 * code in reverse order
	 */
	case SO:
	    value1_size = OPSIZE(data16, LONGOPERAND);
	    IMMEDIATE(&symadd1, &symsub1, &value1, value1_size);
	    value0_size = sizeof(short);
	    IMMEDIATE(&symadd0, &symsub0, &value0, value0_size);
	    printf("%s\t$", mnemonic);
	    print_operand("", symadd0, symsub0, value0, value0_size, "", ",$");
	    print_operand(seg, symadd1, symsub1, value1, value1_size, "", "\n");
	    return(length);

	/* jmp/call. single operand, 8 bit displacement */
	case BD:
	    value0_size = sizeof(char);
	    DISPLACEMENT(&symadd0, &symsub0, &value0, value0_size);
	    printf("%s\t", mnemonic);
	    print_operand(seg, symadd0, symsub0, value0, sizeof(long), "",
			  "\n");
	    return(length);

	/* single 32/16 bit immediate operand */
	case I:
	    value0_size = OPSIZE(data16, LONGOPERAND);
	    IMMEDIATE(&symadd0, &symsub0, &value0, value0_size);
	    printf("%s\t$", mnemonic);
	    print_operand("", symadd0, symsub0, value0, value0_size, "", "\n");
	    return(length);

	/* single 8 bit immediate operand */
	case Ib:
	    value0_size = sizeof(char);
	    IMMEDIATE(&symadd0, &symsub0, &value0, value0_size);
	    printf("%s\t$", mnemonic);
	    print_operand("", symadd0, symsub0, value0, value0_size, "", "\n");
	    return(length);

	case ENTER:
	    value0_size = sizeof(short);
	    IMMEDIATE(&symadd0, &symsub0, &value0, value0_size);
	    value1_size = sizeof(char);
	    IMMEDIATE(&symadd1, &symsub1, &value1, value1_size);
	    printf("%s\t$", mnemonic);
	    print_operand("", symadd0, symsub0, value0, value0_size, "", ",$");
	    print_operand("", symadd1, symsub1, value1, value1_size, "", "\n");
	    return(length);

	/* 16-bit immediate operand */
	case RET:
	    value0_size = sizeof(short);
	    IMMEDIATE(&symadd0, &symsub0, &value0, value0_size);
	    printf("%s\t$", mnemonic);
	    print_operand("", symadd0, symsub0, value0, value0_size, "", "\n");
	    return(length);

	/* single 8 bit port operand */
	case P:
	    value0_size = sizeof(char);
	    IMMEDIATE(&symadd0, &symsub0, &value0, value0_size);
	    printf("%s\t$", mnemonic);
	    print_operand(seg, symadd0, symsub0, value0, value0_size, "", "\n");
	    return(length);

	/* single 8 bit (input) port operand				*/
	case Pi:
	    value0_size = sizeof(char);
	    IMMEDIATE(&symadd0, &symsub0, &value0, value0_size);
	    printf("%s\t$", mnemonic);
	    print_operand(seg, symadd0, symsub0, value0, value0_size, "",
			  ",%eax\n");
	    return(length);

	/* single 8 bit (output) port operand				*/
	case Po:
	    value0_size = sizeof(char);
	    IMMEDIATE(&symadd0, &symsub0, &value0, value0_size);
	    printf("%s\t%%eax,$", mnemonic);
	    print_operand(seg, symadd0, symsub0, value0, value0_size, "", "\n");
	    return(length);

	/* single operand, dx register (variable port instruction) */
	case V:
	    printf("%s\t%s(%%dx)\n", mnemonic, seg);
	    return(length);

	/* single operand, dx register (variable (input) port instruction) */
	case Vi:
	    printf("%s\t%s%%dx,%%eax\n", mnemonic, seg);
	    return(length);

	/* single operand, dx register (variable (output) port instruction)*/
	case Vo:
	    printf("%s\t%s%%eax,%%dx\n", mnemonic, seg);
	    return(length);

	/* The int instruction, which has two forms: int 3 (breakpoint) or  */
	/* int n, where n is indicated in the subsequent byte (format Ib).  */
	/* The int 3 instruction (opcode 0xCC), where, although the 3 looks */
	/* like an operand, it is implied by the opcode. It must be converted */
	/* to the correct base and output. */
	case INT3:
	    printf("%s\t$0x3\n", mnemonic);
	    return(length);

	/* just an opcode and an unused byte that must be discarded */
	case U:
	    byte = get_value(sizeof(char), sect, &length, &left);
	    printf("%s\n", mnemonic);
	    return(length);

	case CBW:
	    if(data16 == TRUE)
		printf("cbtw\n");
	    else
		printf("cwtl\n");
	    return(length);

	case CWD:
	    if(data16 == TRUE)
		printf("cwtd\n");
	    else
		printf("cltd\n");
	    return(length);

	/* no disassembly, the mnemonic was all there was so go on */
	case GO_ON:
	    printf("%s\n", mnemonic);
	    return(length);

	/* float reg */
	case F:
	    printf("%s\t%%st(%1.1lu)\n", mnemonic, r_m);
	    return(length);

	/* float reg to float reg, with ret bit present */
	case FF:
	    /* return result bit for 287 instructions */
	    if(((opcode2 >> 2) & 0x1) == 0x1 && opcode2 != 0xf)
		printf("%s\t%%st,%%st(%1.1lu)\n", mnemonic, r_m);
	    else
		printf("%s\t%%st(%1.1lu),%%st\n", mnemonic, r_m);
	    return(length);

	/* an invalid op code */
	case AM:
	case DM:
	case OVERRIDE:
	case PREFIX:
	case UNKNOWN:
	default:
	    printf(".byte 0x%02x #bad opcode\n", (unsigned int)byte);
	    return(length);
	} /* end switch */
}

/*
 * get_operand() is used to return the symbolic operand for an operand that is
 * encoded with a mod r/m byte.
 */
static
void
get_operand(
const char **symadd,
const char **symsub,
unsigned long *value,
unsigned long *value_size,
char *result,

const unsigned long mode,
const unsigned long r_m,
const unsigned long wbit,
const enum bool data16,
const enum bool addr16,

const char *sect,
unsigned long sect_addr,
unsigned long *length,
unsigned long *left,

const unsigned long addr,
const struct relocation_info *sorted_relocs,
const unsigned long nsorted_relocs,
const struct nlist *symbols,
const unsigned long nsymbols,
const char *strings,
const unsigned long strings_size,

const struct nlist *sorted_symbols,
const unsigned long nsorted_symbols,
const enum bool verbose)
{
    enum bool s_i_b;		/* flag presence of scale-index-byte */
    unsigned char byte;		/* the scale-index-byte */
    unsigned long ss;		/* scale-factor from scale-index-byte */
    unsigned long index; 	/* index register number from scale-index-byte*/
    unsigned long base;  	/* base register number from scale-index-byte */
    unsigned long sect_offset;
    unsigned long offset;

	*symadd = NULL;
	*symsub = NULL;
	*value = 0;
	*result = '\0';

	/* check for the presence of the s-i-b byte */
	if(r_m == ESP && mode != REG_ONLY && addr16 == FALSE){
	    s_i_b = TRUE;
	    byte = get_value(sizeof(char), sect, length, left);
	    modrm_byte(&ss, &index, &base, byte);
	}
	else
	    s_i_b = FALSE;

	if(addr16)
	    *value_size = dispsize16[r_m][mode];
	else
	    *value_size = dispsize32[r_m][mode];

	if(s_i_b == TRUE && mode == 0 && base == EBP)
	    *value_size = sizeof(long);

	if(*value_size != 0){
	    sect_offset = addr + *length - sect_addr;
	    *value = get_value(*value_size, sect, length, left);
	    GET_SYMBOL(symadd, symsub, &offset, sect_offset, *value);
	    if(*symadd != NULL){
		*value = offset;
	    }
	    else{
		*symadd = GUESS_SYMBOL(*value);
		if(*symadd != NULL)
		    *value = 0;
	    }
	}

	if(s_i_b == TRUE){
	    sprintf(result, "(%s%s,%s)", regname32[mode][base],
		    indexname[index], scale_factor[ss]);
	}
	else{ /* no s-i-b */
	    if(mode == REG_ONLY){
		if(data16 == TRUE)
		    strcpy(result, REG16[r_m][wbit]);
		else
		    strcpy(result, REG32[r_m][wbit]);
	    }
	    else{ /* Modes 00, 01, or 10 */
		if(r_m == EBP && mode == 0){ /* displacement only */
		    *result = '\0';
		}
		else {
		    /* Modes 00, 01, or 10, not displacement only, no s-i-b */
		    if(addr16 == TRUE)
			sprintf(result, "(%s)", regname16[mode][r_m]);
		    else
			sprintf(result, "(%s)", regname32[mode][r_m]);
		}
	    }
	}
}

/*
 * immediate() is used to return the symbolic operand for an immediate operand.
 */
static
void
immediate(
const char **symadd,
const char **symsub,
unsigned long *value,
unsigned long value_size,

const char *sect,
unsigned long sect_addr,
unsigned long *length,
unsigned long *left,

const unsigned long addr,
const struct relocation_info *sorted_relocs,
const unsigned long nsorted_relocs,
const struct nlist *symbols,
const unsigned long nsymbols,
const char *strings,
const unsigned long strings_size,

const struct nlist *sorted_symbols,
const unsigned long nsorted_symbols,
const enum bool verbose)
{
    unsigned long sect_offset, offset;

	sect_offset = addr + *length - sect_addr;
	*value = get_value(value_size, sect, length, left);
	GET_SYMBOL(symadd, symsub, &offset, sect_offset, *value);
	if(*symadd == NULL){
	    *symadd = GUESS_SYMBOL(*value);
	    if(*symadd != NULL)
		*value = 0;
	}
	else if(*symsub != NULL){
	    *value = offset;
	}
}

/*
 * displacement() is used to return the symbolic operand for an operand that is
 * encoded as a displacement from the program counter.
 */
static
void
displacement(
const char **symadd,
const char **symsub,
unsigned long *value,
const unsigned long value_size,

const char *sect,
unsigned long sect_addr,
unsigned long *length,
unsigned long *left,

const unsigned long addr,
const struct relocation_info *sorted_relocs,
const unsigned long nsorted_relocs,
const struct nlist *symbols,
const unsigned long nsymbols,
const char *strings,
const unsigned long strings_size,

const struct nlist *sorted_symbols,
const unsigned long nsorted_symbols,
const enum bool verbose)
{
    unsigned long sect_offset, offset;

	sect_offset = addr + *length - sect_addr;
	*value = get_value(value_size, sect, length, left);
	switch(value_size){
	case 1:
	    if((*value) & 0x80)
		*value = *value | 0xffffff00;
	    break;
	case 2:
	    if((*value) & 0x8000)
		*value = *value | 0xffff0000;
	    break;
	}
	*value += addr + *length;
	GET_SYMBOL(symadd, symsub, &offset, sect_offset, *value);
	if(*symadd == NULL){
	    *symadd = GUESS_SYMBOL(*value);
	    if(*symadd != NULL)
		*value = 0;
	}
	else if(*symsub != NULL){
	    *value = offset;
	}
}

/*
 * get_symbol() returns the name of a symbol (or NULL) based on the relocation
 * information at the specified address.
 */
static
void
get_symbol(
const char **symadd,
const char **symsub,
unsigned long *offset,

const unsigned long sect_offset,
const unsigned long value,
const struct relocation_info *relocs,
const unsigned long nrelocs,
const struct nlist *symbols,
const unsigned long nsymbols,
const char *strings,
const unsigned long strings_size,
const struct nlist *sorted_symbols,
const unsigned long nsorted_symbols,
const enum bool verbose)
{
    unsigned long i;
    struct scattered_relocation_info *sreloc, *pair;
    unsigned int r_symbolnum;
    long n_strx;
    const char *name, *add, *sub;

    static char add_buffer[11]; /* max is "0x1234678\0" */
    static char sub_buffer[11];

	*symadd = NULL;
	*symsub = NULL;
	*offset = value;

	if(verbose == FALSE)
	    return;

	for(i = 0; i < nrelocs; i++){
	    if(((relocs[i].r_address) & R_SCATTERED) != 0){
		sreloc = (struct scattered_relocation_info *)(relocs + i);
		if(sreloc->r_type == GENERIC_RELOC_PAIR){
		    fprintf(stderr, "Stray GENERIC_RELOC_PAIR relocation entry "
			    "%lu\n", i);
		    continue;
		}
		if(sreloc->r_type == GENERIC_RELOC_VANILLA){
		    if(sreloc->r_address == sect_offset){
			name = guess_symbol(sreloc->r_value,
					    sorted_symbols,
					    nsorted_symbols,
					    verbose);
			if(name != NULL){
			    *symadd = name;
			    *offset = value - sreloc->r_value;
			    return;
			}
		    }
		    continue;
		}
		if(sreloc->r_type != GENERIC_RELOC_SECTDIFF){
		    fprintf(stderr, "Unknown relocation r_type for entry "
			    "%lu\n", i);
		    continue;
		}
		if(i + 1 < nrelocs){
		    pair = (struct scattered_relocation_info *)(relocs + i + 1);
		    if(pair->r_scattered == 0 ||
		       pair->r_type != GENERIC_RELOC_PAIR){
			fprintf(stderr, "No GENERIC_RELOC_PAIR relocation "
				"entry after entry %lu\n", i);
			continue;
		    }
		}
		else{
		    fprintf(stderr, "No GENERIC_RELOC_PAIR relocation entry "
			    "after entry %lu\n", i);
		    continue;
		}
		i++; /* skip the pair reloc */

		if(sreloc->r_address == sect_offset){
		    add = guess_symbol(sreloc->r_value, sorted_symbols,
				       nsorted_symbols, verbose);
		    sub = guess_symbol(pair->r_value, sorted_symbols,
				       nsorted_symbols, verbose);
		    if(add == NULL){
			sprintf(add_buffer, "0x%x",
				(unsigned int)sreloc->r_value);
			add = add_buffer;
		    }
		    if(sub == NULL){
			sprintf(sub_buffer, "0x%x",
				(unsigned int)pair->r_value);
			sub = sub_buffer;
		    }
		    *symadd = add;
		    *symsub = sub;
		    *offset = value - (sreloc->r_value - pair->r_value);
		    return;
		}
	    }
	    else{
		if(relocs[i].r_address == sect_offset){
		    r_symbolnum = relocs[i].r_symbolnum;
		    if(relocs[i].r_extern){
		        if(r_symbolnum >= nsymbols)
			    return;
			n_strx = symbols[r_symbolnum].n_un.n_strx;
			if(n_strx <= 0 || n_strx >= strings_size)
			    return;
			*symadd = strings + n_strx;
			return;
		    }
		    break;
		}
	    }
	}
}

/*
 * print_operand() prints an operand from it's broken out symbolic
 * representation.
 */
static
void
print_operand(
const char *seg,
const char *symadd,
const char *symsub,
const unsigned int value,
const unsigned int value_size,
const char *result,
const char *tail)
{
	if(symadd != NULL){
	    if(symsub != NULL){
		if(value_size != 0){
		    if(value != 0)
			printf("%s%s-%s+0x%0*x%s%s", seg, symadd, symsub,
			       (int)value_size * 2, value, result, tail);
		    else
			printf("%s%s-%s%s%s",seg, symadd, symsub, result, tail);
		}
		else{
		    printf("%s%s%s%s", seg, symadd, result, tail);
		}
	    }
	    else{
		if(value_size != 0){
		    if(value != 0)
			printf("%s%s+0x%0*x%s%s", seg, symadd,
			       (int)value_size * 2, value, result, tail);
		    else
			printf("%s%s%s%s", seg, symadd, result, tail);
		}
		else{
		    printf("%s%s%s%s", seg, symadd, result, tail);
		}
	    }
	}
	else{
	    if(value_size != 0){
		printf("%s0x%0*x%s%s", seg, (int)value_size *2, value, result,
		       tail);
	    }
	    else{
		printf("%s%s%s", seg, result, tail);
	    }
	}
}

/*
 * get_value() gets a value of size from sect + length and decrease left by the
 * size and increase length by size.  The size of the value can be 1, 2 or 4
 * bytes and the value is in little endian byte order.  The value is always
 * returned as a unsigned long and is not sign extended.
 */
static
unsigned long
get_value(
const unsigned long size,/* size of the value to get as a number of bytes (in)*/
const char *sect,	/* pointer to the raw data of the section (in) */
unsigned long *length,	/* number of bytes taken from the sect (in/out) */
unsigned long *left)	/* number of bytes left in sect after length (in/out) */
{
    unsigned long i, value;
    unsigned char byte;

	if(left == 0)
	    return(0);

	value = 0;
	for(i = 0; i < size; i++) {
	    byte = 0;
	    if(*left > 0){
		byte = sect[*length];
		(*length)++;
		(*left)--;
	    }
	    value |= (unsigned long)byte << (8*i);
	}
	return(value);
}

/*
 * modrm_byte() breaks a byte out in to it's mode, reg and r/m bits.
 */
static
void
modrm_byte(
unsigned long *mode,
unsigned long *reg,
unsigned long *r_m,
unsigned char byte)
{
	*r_m = byte & 0x7; /* r/m field from the byte */
	*reg = byte >> 3 & 0x7; /* register field from the byte */
	*mode = byte >> 6 & 0x3; /* mode field from the byte */
}
