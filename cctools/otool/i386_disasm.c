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
    const enum bool sse2,
    const enum bool mmx,
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
		    mode, r_m, wbit, data16, addr16, sse2, mmx, sect, \
		    sect_addr, &length, &left, addr, sorted_relocs, \
		    nsorted_relocs, symbols, nsymbols, strings, strings_size, \
		    sorted_symbols, nsorted_symbols, verbose)

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
#define SSE2	62	/* SSE2 instruction with possible 3rd opcode byte */
#define SSE2i	63	/* SSE2 instruction with 8 bit immediate */
#define SSE2i1	64	/* SSE2 with one operand and 8 bit immediate */
#define SSE2tm	65	/* SSE2 with dest to memory */
#define SSE2tfm	66	/* SSE2 with dest to memory or memory to dest */
#define PFCH	67	/* prefetch instructions */
#define SFEN	68	/* sfence & clflush */
#define Mnol	69	/* no 'l' suffix, fildl & fistpl */

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
static const char * const SEGREG[8] = {
/* 000 */	"%es",
/* 001 */	"%cs",
/* 010 */	"%ss",
/* 011 */	"%ds",
/* 100 */	"%fs",
/* 101 */	"%gs",
/* 110 */	"%?6",
/* 111 */	"%?7",
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
 * Decode table for 0x0FAE opcodes
 */
static const struct instable op0FAE[8] = {
/*  [0]  */	{"fxsave",TERM,M,1},	{"fxrstor",TERM,M,1},
		{"ldmxcsr",TERM,M,1},	{"stmxcsr",TERM,M,1},
/*  [4]  */	INVALID,		{"lfence",TERM,GO_ON,0},
		{"mfence",TERM,GO_ON,0},{"clflush",TERM,SFEN,1},
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
/*  [10]  */ {  {"mov",TERM,SSE2,0},	{"mov",TERM,SSE2tm,0},
		{"mov",TERM,SSE2,0},	{"movl",TERM,SSE2tm,0},
/*  [14]  */	{"unpckl",TERM,SSE2,0},	{"unpckh",TERM,SSE2,0},
		{"mov",TERM,SSE2,0},	{"movh",TERM,SSE2tm,0},
/*  [18]  */	{"prefetch",TERM,PFCH,1},INVALID,
		INVALID,		INVALID,
/*  [1C]  */	INVALID,		INVALID,
		INVALID,		INVALID },
/*  [20]  */ {  {"mov",TERM,SREG,1},	{"mov",TERM,SREG,1},
		{"mov",TERM,SREG,1},	{"mov",TERM,SREG,1},
/*  [24]  */	{"mov",TERM,SREG,1},	INVALID,
		{"mov",TERM,SREG,1},	INVALID,
/*  [28]  */	{"mova",TERM,SSE2,0},	{"mova",TERM,SSE2tm,0},
		{"cvt",TERM,SSE2,0},	{"movnt",TERM,SSE2tm,0},
/*  [2C]  */	{"cvt",TERM,SSE2,0},	{"cvt",TERM,SSE2,0} ,
		{"ucomi",TERM,SSE2,0},	{"comi",TERM,SSE2,0} },
/*  [30]  */ {  {"wrmsr",TERM,GO_ON,0},	{"rdtsc",TERM,GO_ON,0},
		{"rdmsr",TERM,GO_ON,0},	{"rdpmc",TERM,GO_ON,0},
/*  [34]  */	{"sysenter",TERM,GO_ON,0},{"sysexit",TERM,GO_ON,0},
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
/*  [50]  */ {  {"movmsk",TERM,SSE2,0},	{"sqrt",TERM,SSE2,0},
		{"rsqrt",TERM,SSE2,0},	{"rcp",TERM,SSE2,0},
/*  [54]  */	{"and",TERM,SSE2,0},	{"andn",TERM,SSE2,0},
		{"or",TERM,SSE2,0},	{"xor",TERM,SSE2,0},
/*  [58]  */	{"add",TERM,SSE2,0},	{"mul",TERM,SSE2,0},
		{"cvt",TERM,SSE2,0},	{"cvt",TERM,SSE2,0},
/*  [5C]  */	{"sub",TERM,SSE2,0},	{"min",TERM,SSE2,0},
		{"div",TERM,SSE2,0},	{"max",TERM,SSE2,0} },
/*  [60]  */ {  {"punpcklbw",TERM,SSE2,0},{"punpcklwd",TERM,SSE2,0},
		{"punpckldq",TERM,SSE2,0},{"packsswb",TERM,SSE2,0},
/*  [64]  */	{"pcmpgtb",TERM,SSE2,0},{"pcmpgtw",TERM,SSE2,0},
		{"pcmpgtd",TERM,SSE2,0},{"packuswb",TERM,SSE2,0},
/*  [68]  */	{"punpckhbw",TERM,SSE2,0},{"punpckhwd",TERM,SSE2,0},
		{"punpckhdq",TERM,SSE2,0},{"packssdw",TERM,SSE2,0},
/*  [6C]  */	{"punpckl",TERM,SSE2,0},{"punpckh",TERM,SSE2,0},
		{"movd",TERM,SSE2,0},	{"mov",TERM,SSE2,0} },
/*  [70]  */ {  {"pshu",TERM,SSE2i,0},	{"ps",TERM,SSE2i1,0},
		{"ps",TERM,SSE2i1,0},	{"ps",TERM,SSE2i1,0},
/*  [74]  */	{"pcmpeqb",TERM,SSE2,0},{"pcmpeqw",TERM,SSE2,0},
		{"pcmpeqd",TERM,SSE2,0},{"emms",TERM,GO_ON,0},
/*  [78]  */	INVALID,		INVALID,
		INVALID,		INVALID,
/*  [7C]  */	INVALID,		INVALID,
		{"mov",TERM,SSE2tfm,0},	{"mov",TERM,SSE2tm,0} },
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
		{"",op0FAE,TERM,0},	{"imul",TERM,MRw,1} },
/*  [B0]  */ {  {"cmpxchgb",TERM,XINST,0},{"cmpxchg",TERM,XINST,1},
		{"lss",TERM,MR,0},	{"btr",TERM,RMw,1},
/*  [B4]  */	{"lfs",TERM,MR,0},	{"lgs",TERM,MR,0},
		{"movzb",TERM,MOVZ,1},	{"movzwl",TERM,MOVZ,0},
/*  [B8]  */	INVALID,		INVALID,
		{"",op0FBA,TERM,0},	{"btc",TERM,RMw,1},
/*  [BC]  */	{"bsf",TERM,MRw,1},	{"bsr",TERM,MRw,1},
		{"movsb",TERM,MOVZ,1},	{"movswl",TERM,MOVZ,0} },
/*  [C0]  */ {  {"xaddb",TERM,XINST,0},	{"xadd",TERM,XINST,1},
		{"cmp",TERM,SSE2i,0},	{"movnti",TERM,RMw,0},
/*  [C4]  */	{"pinsrw",TERM,SSE2i,0},{"pextrw",TERM,SSE2i,0},
		{"shuf",TERM,SSE2i,0},	{"cmpxchg8b",TERM,M,1},
/*  [C8]  */	{"bswap",TERM,BSWAP,0},	{"bswap",TERM,BSWAP,0},
		{"bswap",TERM,BSWAP,0},	{"bswap",TERM,BSWAP,0},
/*  [CC]  */	{"bswap",TERM,BSWAP,0},	{"bswap",TERM,BSWAP,0},
		{"bswap",TERM,BSWAP,0},	{"bswap",TERM,BSWAP,0} },
/*  [D0]  */ {  INVALID,		{"psrlw",TERM,SSE2,0},
		{"psrld",TERM,SSE2,0},	{"psrlq",TERM,SSE2,0},
/*  [D4]  */	{"paddq",TERM,SSE2,0},	{"pmullw",TERM,SSE2,0},
		{"mov",TERM,SSE2tm,0},	{"pmovmskb",TERM,SSE2,0},
/*  [D8]  */	{"psubusb",TERM,SSE2,0},{"psubusw",TERM,SSE2,0},
		{"pminub",TERM,SSE2,0},	{"pand",TERM,SSE2,0},
/*  [DC]  */	{"paddusb",TERM,SSE2,0},{"paddusw",TERM,SSE2,0},
		{"pmaxub",TERM,SSE2,0},	{"pandn",TERM,SSE2,0} },
/*  [E0]  */ {  {"pavgb",TERM,SSE2,0},	{"psraw",TERM,SSE2,0},
		{"psrad",TERM,SSE2,0},	{"pavgw",TERM,SSE2,0},
/*  [E4]  */	{"pmulhuw",TERM,SSE2,0},{"pmulhw",TERM,SSE2,0},
		{"cvt",TERM,SSE2,0},	{"movn",TERM,SSE2tm,0},
/*  [E8]  */	{"psubsb",TERM,SSE2,0},	{"psubsw",TERM,SSE2,0},
		{"pminsw",TERM,SSE2,0},	{"por",TERM,SSE2,0},
/*  [EC]  */	{"paddsb",TERM,SSE2,0},	{"paddsw",TERM,SSE2,0},
		{"pmaxsw",TERM,SSE2,0},	{"pxor",TERM,SSE2,0} },
/*  [F0]  */ {  INVALID,		{"psllw",TERM,SSE2,0},
		{"pslld",TERM,SSE2,0},	{"psllq",TERM,SSE2,0},
/*  [F4]  */	{"pmuludq",TERM,SSE2,0},{"pmaddwd",TERM,SSE2,0},
		{"psadbw",TERM,SSE2,0},	{"maskmov",TERM,SSE2,0},
/*  [F8]  */	{"psubb",TERM,SSE2,0},	{"psubw",TERM,SSE2,0},
		{"psubd",TERM,SSE2,0},	{"psubq",TERM,SSE2,0},
/*  [FC]  */	{"paddb",TERM,SSE2,0},	{"paddw",TERM,SSE2,0},
		{"paddd",TERM,SSE2,0},	{"ud2",TERM,GO_ON,0} },
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
/*  [3,0]  */ { {"fildl",TERM,Mnol,1},	INVALID,
		{"fistl",TERM,M,1},	{"fistpl",TERM,Mnol,1},
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
    unsigned opcode1, opcode2, opcode3, opcode4, opcode5, prefix_byte;
    const struct instable *dp, *prefix_dp;
    unsigned long wbit, vbit;
    enum bool got_modrm_byte;
    unsigned long mode, reg, r_m;
    const char *reg_name;
    enum bool data16;		/* 16- or 32-bit data */
    enum bool addr16;		/* 16- or 32-bit addressing */
    enum bool sse2;		/* sse2 instruction using xmmreg's */
    enum bool mmx;		/* mmx instruction using mmreg's */

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
	sse2 = FALSE;
	mmx = FALSE;
	reg_name = NULL;
	wbit = 0;

	length = 0;
	byte = 0;
	opcode4 = 0; /* to remove a compiler warning only */
	opcode5 = 0; /* to remove a compiler warning only */

	/*
	 * As long as there is a prefix, the default segment register,
	 * addressing-mode, or data-mode in the instruction will be overridden.
	 * This may be more general than the chip actually is.
	 */
	prefix_dp = NULL;
	prefix_byte = 0;
	for(;;){
	    byte = get_value(sizeof(char), sect, &length, &left);
	    opcode1 = byte >> 4 & 0xf;
	    opcode2 = byte & 0xf;

	    dp = &distable[opcode1][opcode2];

	    if(dp->adr_mode == PREFIX){
		if(prefix_dp != NULL)
		    printf(dp->name);
		prefix_dp = dp;
		prefix_byte = byte;
	    }
	    else if(dp->adr_mode == AM){
		addr16 = !addr16;
		prefix_byte = byte;
	    }
	    else if(dp->adr_mode == DM){
		data16 = !data16;
		prefix_byte = byte;
	    }
	    else if(dp->adr_mode == OVERRIDE){
		seg = dp->name;
		prefix_byte = byte;
	    }
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
	    /*
	     * SSE and SSE2 instructions have 3 bytes of opcode and the
	     * "third opcode byte" is before the other two (where the prefix
	     * byte would be).  This is why the prefix byte is saved above and
	     * the printing of the last prefix is delayed.
	     */
	    if(dp->adr_mode == SSE2 ||
	       dp->adr_mode == SSE2i ||
	       dp->adr_mode == SSE2i1 ||
	       dp->adr_mode == SSE2tm ||
	       dp->adr_mode == SSE2tfm){
		prefix_dp = NULL;
	    }
	    else{
		/*
		 * Since the opcode is not an SSE or SSE2 instruction that uses
		 * the prefix byte as the "third opcode byte" print the
		 * delayed last prefix if any.
		 */
		if(prefix_dp != NULL)
		    printf(prefix_dp->name);
	    }
	}
	else{
	    /*
	     * The "pause" Spin Loop Hint instruction is a "repz" prefix
	     * followed by a nop (0x90).
	     */
	    if(prefix_dp != NULL && prefix_byte == 0xf3 &&
	       opcode1 == 0x9 && opcode2 == 0x0){
		printf("pause\n");
		return(length);
	    }
	    /*
	     * Since the opcode is not an SSE or SSE2 instruction print the
	     * delayed last prefix if any.
	     */
	    if(prefix_dp != NULL)
		printf(prefix_dp->name);
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
		else{
		    if(dp->adr_mode == Mnol)
			sprintf(mnemonic, "%s", dp->name);
		    else
			sprintf(mnemonic, "%sl", dp->name);
		}
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

	/* SSE2 instructions with further prefix decoding dest to memory or
	   memory to dest depending on the opcode */
	case SSE2tfm:
	    data16 = FALSE;
	    if(got_modrm_byte == FALSE){
		got_modrm_byte = TRUE;
		byte = get_value(sizeof(char), sect, &length, &left);
		modrm_byte(&mode, &reg, &r_m, byte);
	    }
	    switch(opcode4 << 4 | opcode5){
	    case 0x7e: /* movq & movd */
		if(prefix_byte == 0x66){
		    /* movd from xmm to r/m32 */
		    printf("%sd\t%%xmm%lu,", mnemonic, reg);
		    wbit = LONGOPERAND;
		    GET_OPERAND(&symadd0, &symsub0, &value0, &value0_size,
				result0);
		    print_operand(seg, symadd0, symsub0, value0, value0_size,
				  result0, "\n");
		}
		else if(prefix_byte == 0xf0){
		    /* movq from mm to mm/m64 */
		    printf("%sd\t%%mm%lu,", mnemonic, reg);
		    mmx = TRUE;
		    GET_OPERAND(&symadd1, &symsub1, &value1, &value1_size,
				result1);
		    print_operand(seg, symadd1, symsub1, value1, value1_size,
				  result1, "\n");
		}
		else if(prefix_byte == 0xf3){
		    /* movq from xmm2/mem64 to xmm1 */
		    printf("%sq\t", mnemonic);
		    sse2 = TRUE;
		    GET_OPERAND(&symadd0, &symsub0, &value0, &value0_size,
				result0);
		    print_operand(seg, symadd0, symsub0, value0, value0_size,
				  result0, ",");
		    printf("%%xmm%lu\n", reg);
		}
		else{ /* no prefix_byte */
		    /* movd from mm to r/m32 */
		    printf("%sd\t%%mm%lu,", mnemonic, reg);
		    wbit = LONGOPERAND;
		    GET_OPERAND(&symadd1, &symsub1, &value1, &value1_size,
				result1);
		    print_operand(seg, symadd1, symsub1, value1, value1_size,
				  result1, "\n");
		}
	    }
	    return(length);

	/* SSE2 instructions with further prefix decoding dest to memory */
	case SSE2tm:
	    data16 = FALSE;
	    if(got_modrm_byte == FALSE){
		got_modrm_byte = TRUE;
		byte = get_value(sizeof(char), sect, &length, &left);
		modrm_byte(&mode, &reg, &r_m, byte);
	    }
	    sprintf(result0, "%%xmm%lu", reg);
	    switch(opcode4 << 4 | opcode5){
	    case 0x11: /* movupd &         movups */
		       /*          movsd &        movss */
		sse2 = TRUE;
		if(prefix_byte == 0x66)
		    printf("%supd\t", mnemonic);
		else if(prefix_byte == 0xf2)
		    printf("%ssd\t", mnemonic);
		else if(prefix_byte == 0xf3)
		    printf("%sss\t", mnemonic);
		else /* no prefix_byte */
		    printf("%sups\t", mnemonic);
		break;
	    case 0x13: /*  movlpd &          movlps */
	    case 0x17: /*  movhpd &          movhps */
	    case 0x29: /*  movapd &  movasd */
	    case 0x2b: /* movntpd & movntsd */
		sse2 = TRUE;
		if(prefix_byte == 0x66)
		    printf("%spd\t", mnemonic);
		else if(prefix_byte == 0xf2)
		    printf("%ssd\t", mnemonic);
		else if(prefix_byte == 0xf3)
		    printf("%sss\t", mnemonic);
		else /* no prefix_byte */
		    printf("%sps\t", mnemonic);
		break;
	    case 0xd6: /* movq */
		if(prefix_byte == 0x66){
		    sse2 = TRUE;
		    printf("%sq\t", mnemonic);
		}
		break;
	    case 0x7f: /* movdqa, movdqu, movq */
		sse2 = TRUE;
		if(prefix_byte == 0x66)
		    printf("%sdqa\t", mnemonic);
		else if(prefix_byte == 0xf3)
		    printf("%sdqu\t", mnemonic);
		else{
		    sprintf(result0, "%%mm%lu", reg);
		    printf("%sq\t", mnemonic);
		    mmx = TRUE;
		}
		break;
	    case 0xe7: /* movntdq & movntq */
		if(prefix_byte == 0x66){
		    printf("%stdq\t", mnemonic);
		}
		else{ /* no prefix_byte */
		    sprintf(result0, "%%mm%lu", reg);
		    printf("%stq\t", mnemonic);
		    mmx = TRUE;
		}
		break;
	    }
	    printf("%s,", result0);
	    GET_OPERAND(&symadd1, &symsub1, &value1, &value1_size, result1);
	    print_operand(seg, symadd1, symsub1, value1, value1_size,
			  result1, "\n");
	    return(length);

	/* SSE2 instructions with further prefix decoding */
	case SSE2:
	    data16 = FALSE;
	    if(got_modrm_byte == FALSE){
		got_modrm_byte = TRUE;
		byte = get_value(sizeof(char), sect, &length, &left);
		modrm_byte(&mode, &reg, &r_m, byte);
	    }
	    sprintf(result1, "%%xmm%lu", reg);
	    switch(opcode4 << 4 | opcode5){
	    case 0x14: /* unpcklpd &                 unpcklps */
	    case 0x15: /* unpckhpd &                 unpckhps */
	    case 0x28: /*   movapd & movasd */
	    case 0x51: /*   sqrtpd,  sqrtsd, sqrtss &  sqrtps */
	    case 0x52: /*                   rsqrtss & rsqrtps */
	    case 0x53: /*                     rcpss &   rcpps */
	    case 0x54: /*    andpd &  andsd */
	    case 0x55: /*   andnpd & andnsd */
	    case 0x56: /*     orpd &                    orps */
	    case 0x57: /*    xorpd &                   xorps */
	    case 0x58: /*    addpd &  addsd */
	    case 0x59: /*    mulpd,   mulsd,  mulss &   mulps */
	    case 0x5c: /*    subpd,   subsd,  subss &   subps */
	    case 0x5d: /*    minpd,   minsd,  minss &   minps */
	    case 0x5e: /*    divpd,   divsd,  divss &   divps */
	    case 0x5f: /*    maxpd,   maxsd,  maxss &   maxps */
		sse2 = TRUE;
		if(prefix_byte == 0x66)
		    printf("%spd\t", mnemonic);
		else if(prefix_byte == 0xf2)
		    printf("%ssd\t", mnemonic);
		else if(prefix_byte == 0xf3)
		    printf("%sss\t", mnemonic);
		else /* no prefix_byte */
		    printf("%sps\t", mnemonic);
		break;
	    case 0x12: /*   movlpd, movlps & movhlps */
		sse2 = TRUE;
		if(prefix_byte == 0x66)
		    printf("%slpd\t", mnemonic);
		else if(prefix_byte == 0xf2)
		    printf("%slsd\t", mnemonic);
		else if(prefix_byte == 0xf3)
		    printf("%slss\t", mnemonic);
		else{ /* no prefix_byte */
		    if(mode == REG_ONLY)
			printf("%shlps\t", mnemonic);
		    else
			printf("%slps\t", mnemonic);
		}
		break;
	    case 0x16: /*   movhpd, movhps & movlhps */
		sse2 = TRUE;
		if(prefix_byte == 0x66)
		    printf("%shpd\t", mnemonic);
		else if(prefix_byte == 0xf2)
		    printf("%shsd\t", mnemonic);
		else if(prefix_byte == 0xf3)
		    printf("%shss\t", mnemonic);
		else{ /* no prefix_byte */
		    if(mode == REG_ONLY)
			printf("%slhps\t", mnemonic);
		    else
			printf("%shps\t", mnemonic);
		}
		break;
	    case 0x50: /* movmskpd &                 movmskps */
		sse2 = TRUE;
		reg_name = REG32[reg][1];
		strcpy(result1, reg_name);
		if(prefix_byte == 0x66)
		    printf("%spd\t", mnemonic);
		else /* no prefix_byte */
		    printf("%sps\t", mnemonic);
		break;
	    case 0x10: /*   movupd &                  movups */
		       /*             movsd & movss */
		sse2 = TRUE;
		if(prefix_byte == 0x66)
		    printf("%supd\t", mnemonic);
		else if(prefix_byte == 0xf2)
		    printf("%ssd\t", mnemonic);
		else if(prefix_byte == 0xf3)
		    printf("%sss\t", mnemonic);
		else /* no prefix_byte */
		    printf("%sups\t", mnemonic);
		break;
	    case 0x2a: /* cvtpi2pd, cvtsi2sd, cvtsi2ss & cvtpi2ps */
		if(prefix_byte == 0x66){
		    mmx = TRUE;
		    printf("%spi2pd\t", mnemonic);
		}
		else if(prefix_byte == 0xf2){
		    wbit = LONGOPERAND;
		    printf("%ssi2sd\t", mnemonic);
		}
		else if(prefix_byte == 0xf3){
		    wbit = LONGOPERAND;
		    printf("%ssi2ss\t", mnemonic);
		}
		else{ /* no prefix_byte */
		    mmx = TRUE;
		    printf("%spi2ps\t", mnemonic);
		}
		break;
	    case 0x2c: /* cvttpd2pi, cvttsd2si, cvttss2si & cvttps2pi */
		if(prefix_byte == 0x66){
		    sse2 = TRUE;
		    printf("%stpd2pi\t", mnemonic);
		    sprintf(result1, "%%mm%lu", reg);
		}
		else if(prefix_byte == 0xf2){
		    sse2 = TRUE;
		    printf("%stsd2si\t", mnemonic);
		    reg_name = REG32[reg][1];
		    strcpy(result1, reg_name);
		}
		else if(prefix_byte == 0xf3){
		    sse2 = TRUE;
		    printf("%stss2si\t", mnemonic);
		    reg_name = REG32[reg][1];
		    strcpy(result1, reg_name);
		}
		else{ /* no prefix_byte */
		    sse2 = TRUE;
		    printf("%stps2pi\t", mnemonic);
		    sprintf(result1, "%%mm%lu", reg);
		}
		break;
	    case 0x2d: /* cvtpd2pi, cvtsd2si, cvtss2si & cvtps2pi */
		if(prefix_byte == 0x66){
		    sse2 = TRUE;
		    printf("%spd2pi\t", mnemonic);
		    sprintf(result1, "%%mm%lu", reg);
		}
		else if(prefix_byte == 0xf2){
		    sse2 = TRUE;
		    printf("%ssd2si\t", mnemonic);
		    reg_name = REG32[reg][1];
		    strcpy(result1, reg_name);
		}
		else if(prefix_byte == 0xf3){
		    sse2 = TRUE;
		    printf("%sss2si\t", mnemonic);
		    reg_name = REG32[reg][1];
		    strcpy(result1, reg_name);
		}
		else{ /* no prefix_byte */
		    sse2 = TRUE;
		    printf("%sps2pi\t", mnemonic);
		    sprintf(result1, "%%mm%lu", reg);
		}
		break;
	    case 0x2e: /* ucomisd & ucomiss */
	    case 0x2f: /*  comisd &  comiss */
		sse2 = TRUE;
		if(prefix_byte == 0x66)
		    printf("%ssd\t", mnemonic);
		else /* no prefix_byte */
		    printf("%sss\t", mnemonic);
		break;
	    case 0xe0: /* pavgb */
	    case 0xe3: /* pavgw */
		if(prefix_byte == 0x66){
		    sse2 = TRUE;
		    printf("%s\t", mnemonic);
		}
		else{ /* no prefix_byte */
		    sprintf(result1, "%%mm%lu", reg);
		    printf("%s\t", mnemonic);
		    mmx = TRUE;
		}
		break;
	    case 0xe6: /* cvttpd2dq, cvtdq2pd & cvtpd2dq */
		sse2 = TRUE;
		if(prefix_byte == 0x66)
		    printf("%stpd2dq\t", mnemonic);
		if(prefix_byte == 0xf3)
		    printf("%sdq2pd\t", mnemonic);
		else if(prefix_byte == 0xf2)
		    printf("%spd2dq\t", mnemonic);
		break;
	    case 0x5a: /* cvtpd2ps, cvtsd2ss, cvtss2sd & cvtps2pd */
		sse2 = TRUE;
		if(prefix_byte == 0x66)
		    printf("%spd2ps\t", mnemonic);
		else if(prefix_byte == 0xf2)
		    printf("%ssd2ss\t", mnemonic);
		else if(prefix_byte == 0xf3)
		    printf("%sss2sd\t", mnemonic);
		else /* no prefix_byte */
		    printf("%sps2pd\t", mnemonic);
		break;
	    case 0x5b: /* cvtdq2ps, cvttps2dq & cvtps2dq */
		sse2 = TRUE;
		if(prefix_byte == 0x66)
		    printf("%sps2dq\t", mnemonic);
		else if(prefix_byte == 0xf3)
		    printf("%stps2dq\t", mnemonic);
		else /* no prefix_byte */
		    printf("%sdq2ps\t", mnemonic);
		break;
	    case 0x60: /* punpcklbw */
	    case 0x61: /* punpcklwd */
	    case 0x62: /* punpckldq */
	    case 0x63: /* packsswb */
	    case 0x64: /* pcmpgtb */
	    case 0x65: /* pcmpgtw */
	    case 0x66: /* pcmpgtd */
	    case 0x67: /* packuswb */
	    case 0x68: /* punpckhbw */
	    case 0x69: /* punpckhwd */
	    case 0x6a: /* punpckhdq */
	    case 0x6b: /* packssdw */
	    case 0x74: /* pcmpeqb */
	    case 0x75: /* pcmpeqw */
	    case 0x76: /* pcmpeqd */
	    case 0xd1: /* psrlw */
	    case 0xd2: /* psrld */
	    case 0xd3: /* psrlq */
	    case 0xd4: /* paddq */
	    case 0xd5: /* pmullw */
	    case 0xd8: /* psubusb */
	    case 0xd9: /* psubusw */
	    case 0xdb: /* pand */
	    case 0xdc: /* paddusb */
	    case 0xdd: /* paddusw */
	    case 0xdf: /* pandn */
	    case 0xe1: /* psraw */
	    case 0xe2: /* psrad */
	    case 0xe5: /* pmulhw */
	    case 0xe8: /* psubsb */
	    case 0xe9: /* psubsw */
	    case 0xeb: /* por */
	    case 0xec: /* paddsb */
	    case 0xed: /* paddsw */
	    case 0xef: /* pxor */
	    case 0xf1: /* psllw */
	    case 0xf2: /* pslld */
	    case 0xf3: /* psllq */
	    case 0xf5: /* pmaddwd */
	    case 0xf8: /* psubb */
	    case 0xf9: /* psubw */
	    case 0xfa: /* psubd */
	    case 0xfb: /* psubq */
	    case 0xfc: /* paddb */
	    case 0xfd: /* paddw */
	    case 0xfe: /* paddd */
		if(prefix_byte == 0x66){
		    printf("%s\t", mnemonic);
		    sse2 = TRUE;
		}
		else{ /* no prefix_byte */
		    sprintf(result1, "%%mm%lu", reg);
		    printf("%s\t", mnemonic);
		    mmx = TRUE;
		}
		break;
	    case 0x6c: /* punpcklqdq */
	    case 0x6d: /* punpckhqdq */
		sse2 = TRUE;
		if(prefix_byte == 0x66)
		    printf("%sqdq\t", mnemonic);
		break;
	    case 0x6f: /* movdqa, movdqu & movq */
		if(prefix_byte == 0x66){
		    sse2 = TRUE;
		    printf("%sdqa\t", mnemonic);
		}
		else if(prefix_byte == 0xf3){
		    sse2 = TRUE;
		    printf("%sdqu\t", mnemonic);
		}
		else{ /* no prefix_byte */
		    sprintf(result1, "%%mm%lu", reg);
		    printf("%sq\t", mnemonic);
		    mmx = TRUE;
		}
		break;
	    case 0xd6: /* movdq2q & movq2dq */
		if(prefix_byte == 0xf2){
		    sprintf(result1, "%%mm%lu", reg);
		    printf("%sdq2q\t", mnemonic);
		    sse2 = TRUE;
		}
		else if(prefix_byte == 0xf3){
		    printf("%sq2dq\t", mnemonic);
		    mmx = TRUE;
		}
		break;
	    case 0x6e: /* movd */
		if(prefix_byte == 0x66){
		    printf("%s\t", mnemonic);
		    wbit = LONGOPERAND;
		}
		else{ /* no prefix_byte */
		    sprintf(result1, "%%mm%lu", reg);
		    printf("%s\t", mnemonic);
		    wbit = LONGOPERAND;
		}
		break;
	    case 0xd7: /* pmovmskb */
		if(prefix_byte == 0x66){
		    reg_name = REG32[reg][1];
		    printf("%s\t%%xmm%lu,%s\n", mnemonic, r_m, reg_name);
		    return(length);
		}
		else{ /* no prefix_byte */
		    reg_name = REG32[reg][1];
		    printf("%s\t%%mm%lu,%s\n", mnemonic, r_m, reg_name);
		    return(length);
		}
		break;
	    case 0xda: /* pminub */
	    case 0xde: /* pmaxub */
	    case 0xe4: /* pmulhuw */
	    case 0xea: /* pminsw */
	    case 0xee: /* pmaxsw */
	    case 0xf4: /* pmuludq */
	    case 0xf6: /* psadbw */
		if(prefix_byte == 0x66){
		    sse2 = TRUE;
		    printf("%s\t", mnemonic);
		}
		else{ /* no prefix_byte */
		    sprintf(result1, "%%mm%lu", reg);
		    printf("%s\t", mnemonic);
		    mmx = TRUE;
		}
		break;
	    case 0xf7: /* maskmovdqu & maskmovq */
		sse2 = TRUE;
		if(prefix_byte == 0x66)
		    printf("%sdqu\t", mnemonic);
		else{ /* no prefix_byte */
		    printf("%sq\t%%mm%lu,%%mm%lu\n", mnemonic, r_m, reg);
		    return(length);
		}
		break;
	    }
	    GET_OPERAND(&symadd0, &symsub0, &value0, &value0_size, result0);
	    print_operand(seg, symadd0, symsub0, value0, value0_size,
			  result0, ",");
	    printf("%s\n", result1);
	    return(length);

	/* SSE2 instructions with 8 bit immediate with further prefix decoding*/
	case SSE2i:
	    data16 = FALSE;
	    if(got_modrm_byte == FALSE){
		got_modrm_byte = TRUE;
		byte = get_value(sizeof(char), sect, &length, &left);
		modrm_byte(&mode, &reg, &r_m, byte);
	    }
	    /* pshufw */
	    if((opcode4 << 4 | opcode5) == 0x70 && prefix_byte == 0)
		mmx = TRUE;
	    /* pinsrw */
	    else if((opcode4 << 4 | opcode5) == 0xc4)
		wbit = LONGOPERAND;
	    else
		sse2 = TRUE;
	    GET_OPERAND(&symadd0, &symsub0, &value0, &value0_size, result0);
	    byte = get_value(sizeof(char), sect, &length, &left);

	    switch(opcode4 << 4 | opcode5){
	    case 0x70: /* pshufd, pshuflw, pshufhw & pshufw */
		if(prefix_byte == 0x66)
		    printf("%sfd\t$0x%x,", mnemonic, byte);
		else if(prefix_byte == 0xf2)
		    printf("%sflw\t$0x%x,", mnemonic, byte);
		else if(prefix_byte == 0xf3)
		    printf("%sfhw\t$0x%x,", mnemonic, byte);
		else{ /* no prefix_byte */
		    printf("%sfw\t$0x%x,", mnemonic, byte);
		    print_operand(seg, symadd0, symsub0, value0, value0_size,
				  result0, ",");
		    printf("%%mm%lu\n", reg);
		    return(length);
		}
		break;
	    case 0xc4: /* pinsrw */
		if(prefix_byte == 0x66){
		    printf("%s\t$0x%x,", mnemonic, byte);
		}
		else{ /* no prefix_byte */
		    printf("%s\t$0x%x,", mnemonic, byte);
		    print_operand(seg, symadd0, symsub0, value0, value0_size,
				  result0, ",");
		    printf("%%mm%lu\n", reg);
		    return(length);
		}
		break;
	    case 0xc5: /* pextrw */
		if(prefix_byte == 0x66){
		    reg_name = REG32[reg][1];
		    printf("%s\t$0x%x,%%xmm%lu,%s\n", mnemonic, byte, r_m,
			   reg_name);
		    return(length);
		}
		else{ /* no prefix_byte */
		    reg_name = REG32[reg][1];
		    printf("%s\t$0x%x,%%mm%lu,%s\n", mnemonic, byte, r_m,
			   reg_name);
		    return(length);
		}
		break;
	    default:
		if(prefix_byte == 0x66)
		    printf("%spd\t$0x%x,", mnemonic, byte);
		else if(prefix_byte == 0xf2)
		    printf("%ssd\t$0x%x,", mnemonic, byte);
		else if(prefix_byte == 0xf3)
		    printf("%sss\t$0x%x,", mnemonic, byte);
		else /* no prefix_byte */
		    printf("%sps\t$0x%x,", mnemonic, byte);
		break;
	    }
	    print_operand(seg, symadd0, symsub0, value0, value0_size,
			  result0, ",");
	    printf("%%xmm%lu\n", reg);
	    return(length);

	/* SSE2 instructions with 8 bit immediate and only 1 reg */
	case SSE2i1:
	    if(got_modrm_byte == FALSE){
		got_modrm_byte = TRUE;
		byte = get_value(sizeof(char), sect, &length, &left);
		modrm_byte(&mode, &reg, &r_m, byte);
	    }
	    byte = get_value(sizeof(char), sect, &length, &left);
	    switch(opcode4 << 4 | opcode5){
	    case 0x71: /* psrlw, psllw, psraw & psrld */
		if(prefix_byte == 0x66){
		    if(reg == 0x2)
			printf("%srlw\t$0x%x,", mnemonic, byte);
		    else if(reg == 0x4)
			printf("%sraw\t$0x%x,", mnemonic, byte);
		    else if(reg == 0x6)
			printf("%sllw\t$0x%x,", mnemonic, byte);
		}
		else{ /* no prefix_byte */
		    if(reg == 0x2)
			printf("%srlw\t$0x%x,", mnemonic, byte);
		    else if(reg == 0x4)
			printf("%sraw\t$0x%x,", mnemonic, byte);
		    else if(reg == 0x6)
			printf("%sllw\t$0x%x,", mnemonic, byte);
		    printf("%%mm%lu\n", r_m);
		    return(length);
		}
		break;
	    case 0x72: /* psrld, pslld & psrad */
		if(prefix_byte == 0x66){
		    if(reg == 0x2)
			printf("%srld\t$0x%x,", mnemonic, byte);
		    else if(reg == 0x4)
			printf("%srad\t$0x%x,", mnemonic, byte);
		    else if(reg == 0x6)
			printf("%slld\t$0x%x,", mnemonic, byte);
		}
		else{ /* no prefix_byte */
		    if(reg == 0x2)
			printf("%srld\t$0x%x,", mnemonic, byte);
		    else if(reg == 0x4)
			printf("%srad\t$0x%x,", mnemonic, byte);
		    else if(reg == 0x6)
			printf("%slld\t$0x%x,", mnemonic, byte);
		    printf("%%mm%lu\n", r_m);
		    return(length);
		}
		break;
	    case 0x73: /* pslldq & psrldq, psrlq & psllq */
		if(prefix_byte == 0x66){
		    if(reg == 0x7)
			printf("%slldq\t$0x%x,", mnemonic, byte);
		    else if(reg == 0x3)
			printf("%srldq\t$0x%x,", mnemonic, byte);
		    else if(reg == 0x2)
			printf("%srlq\t$0x%x,", mnemonic, byte);
		    else if(reg == 0x6)
			printf("%sllq\t$0x%x,", mnemonic, byte);
		}
		else{ /* no prefix_byte */
		    if(reg == 0x2)
			printf("%srlq\t$0x%x,", mnemonic, byte);
		    else if(reg == 0x6)
			printf("%sllq\t$0x%x,", mnemonic, byte);
		    printf("%%mm%lu\n", r_m);
		    return(length);
		}
		break;
	    }
	    printf("%%xmm%lu\n", r_m);
	    return(length);

	/* prefetch instructions */
	case PFCH:
	    if(got_modrm_byte == FALSE){
		got_modrm_byte = TRUE;
		byte = get_value(sizeof(char), sect, &length, &left);
		modrm_byte(&mode, &reg, &r_m, byte);
	    }
	    switch(reg){
	    case 0:
		printf("%snta", dp->name);
		break;
	    case 1:
		printf("%st0", dp->name);
		break;
	    case 2:
		printf("%st1", dp->name);
		break;
	    case 3:
		printf("%st2", dp->name);
		break;
	    }
	    if(data16 == TRUE)
		printf("w\t");
	    else
		printf("l\t");
	    GET_OPERAND(&symadd0, &symsub0, &value0, &value0_size, result0);
	    print_operand(seg, symadd0, symsub0, value0, value0_size,
			  result0, "\n");
	    return(length);

	/* sfence & clflush */
	case SFEN:
	    if(mode == REG_ONLY && r_m == 0){
		printf("sfence\n");
		return(length);
	    }
	    printf("%s\t", mnemonic);
	    reg = opcode3;
	    GET_OPERAND(&symadd0, &symsub0, &value0, &value0_size, result0);
	    print_operand(seg, symadd0, symsub0, value0, value0_size,
			  result0, "\n");
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

	/* single memory or register operand but don't use 'l' suffix */
	case Mnol:
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
const enum bool sse2,
const enum bool mmx,

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
		if(sse2 == TRUE)
		    sprintf(result, "%%xmm%lu", r_m);
		else if(mmx == TRUE)
		    sprintf(result, "%%mm%lu", r_m);
		else if(data16 == TRUE)
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
    unsigned long n_strx;
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
		if((unsigned long)relocs[i].r_address == sect_offset){
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
