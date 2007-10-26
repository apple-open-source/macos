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
#include <stdio.h>
#include <string.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/reloc.h>
#include <mach-o/ppc/reloc.h>
#include "stuff/bytesex.h"
#include "stuff/symbol.h"
#include "otool.h"
#include "ofile_print.h"
#include "ppc_disasm.h"

#define	RT(x)		(((x) >> 21) & 0x1f)
#define	RA(x)		(((x) >> 16) & 0x1f)
#define	RB(x)		(((x) >> 11) & 0x1f)

#define	VT(x)		(((x) >> 21) & 0x1f)
#define	VA(x)		(((x) >> 16) & 0x1f)
#define	VB(x)		(((x) >> 11) & 0x1f)
#define	VC(x)		(((x) >> 6) & 0x1f)
#define	TAG(x)		(((x) >> 21) & 0x3)
#define VX_Rc(x)	(rc[((x >> 10) & 0x1)])

#define	RS(x)		(((x) >> 21) & 0x1f)
#define	TO(x)		(((x) >> 21) & 0x1f)
#define	L(x)		(((x) >> 21) & 0x1)
#define	NB(x)		(((x) >> 11) & 0x1f)
#define	SH(x)		(((x) >> 11) & 0x1f)
#define	MB(x)		(((x) >> 6) & 0x1f)
#define	ME(x)		(((x) >> 1) & 0x1f)

#define	BF(x)		(((x) >> 23) & 0x7)
#define	BFA(x)		(((x) >> 18) & 0x7)
#define	U(x)		(((x) >> 12) & 0xf)
#define	BT(x)		(((x) >> 21) & 0x1f)

#define	SR(x)		(((x) >> 16) & 0xf)

#define	FRT(x)		(((x) >> 21) & 0x1f)
#define	FRA(x)		(((x) >> 16) & 0x1f)
#define	FRB(x)		(((x) >> 11) & 0x1f)
#define	FRC(x)		(((x) >> 6) & 0x1f)
#define	FLM(x)		(((x) >> 17) & 0xff)

#define OE_Rc(x)	(oe_rc[((x >> 9) & 0x2) | ((x) & 0x1)])
#define Rc(x)		(rc[(x) & 0x1])
#define LK(x)		(lk[(x) & 0x1])
#define AA(x)		(aa[(x >> 1) & 0x1])

#define	BH(x)		(((x) >> 11) & 0x3)
#define	BC(x)		(((x) >> 16) & 0x3)
#define	CR_FIELD(x)	(((x) >> 18) & 0x7)
#define	Y_BIT(x)	(((x) >> 21) & 0x1)
#define	BC_TRUE(x)	(bc_true[BC(x)])
#define	BC_FALSE(x)	(bc_false[BC(x)])

#define	TH(x)		(((x) >> 21) & 0xf)

static const char *oe_rc[] = { "", ".", "o", "o." };
static const char *rc[] = { "", "." };
static const char *lk[] = { "", "l" };
static const char *aa[] = { "", "a" };
static const char *bc_true[]  = { "lt", "gt", "eq", "un" };
static const char *bc_false[] = { "ge", "le", "ne", "nu" };

static void xo_form(
    const char *name,
    unsigned long opcode,
    unsigned long nregs);

static void x_form(
    const char *name,
    unsigned long opcode,
    unsigned long nregs);

static void sx_form(
    const char *name,
    unsigned long opcode,
    unsigned long nregs);

static void fx_form(
    const char *name,
    unsigned long opcode,
    unsigned long nregs);

static void xl_form(
    const char *name,
    unsigned long opcode,
    unsigned long nregs);

static void a_form(
    const char *name,
    unsigned long opcode,
    unsigned long nregs);

static unsigned long bc(
    const char *name,
    unsigned long opcode,
    unsigned long sect_offset,
    struct relocation_info *relocs,
    unsigned long nrelocs);

static void trap(
    const char *name,
    unsigned long opcode);

static void print_special_register_name(
    unsigned opcode);

static void print_immediate(
    unsigned long value, 
    unsigned long sect_offset,
    struct relocation_info *sorted_relocs,
    unsigned long nsorted_relocs,
    struct nlist *symbols,
    struct nlist_64 *symbols64,
    unsigned long nsymbols,
    struct symbol *sorted_symbols,
    unsigned long nsorted_symbols,
    char *strings,
    unsigned long strings_size,
    enum bool verbose);

static unsigned long get_reloc_r_type(
    unsigned long pc,
    struct relocation_info *relocs,
    unsigned long nrelocs);

static unsigned long get_reloc_r_length(
    unsigned long sect_offset,
    struct relocation_info *relocs,
    unsigned long nrelocs);

unsigned long
ppc_disassemble(
char *sect,
unsigned long left,
unsigned long addr,
unsigned long sect_addr,
enum byte_sex object_byte_sex,
struct relocation_info *relocs,
unsigned long nrelocs,
struct nlist *symbols,
struct nlist_64 *symbols64,
unsigned long nsymbols,
struct symbol *sorted_symbols,
unsigned long nsorted_symbols,
char *strings,
unsigned long strings_size,
uint32_t *indirect_symbols,
unsigned long nindirect_symbols,
struct load_command *load_commands,
uint32_t ncmds,
uint32_t sizeofcmds,
enum bool verbose)
{
    enum byte_sex host_byte_sex;
    enum bool swapped, jbsr;
    unsigned long opcode, base, disp;
    long simm;
    unsigned long sect_offset;
    const char *indirect_symbol_name;

	host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != object_byte_sex;
	sect_offset = addr - sect_addr;

	if(left < sizeof(unsigned long)){
	   if(left != 0){
		memcpy(&opcode, sect, left);
		if(swapped)
		    opcode = SWAP_LONG(opcode);
		printf(".long\t0x%08x\n", (unsigned int)opcode);
	   }
	   printf("(end of section)\n");
	   return(left);
	}

	memcpy(&opcode, sect, sizeof(unsigned long));
	if(swapped)
	    opcode = SWAP_LONG(opcode);

	switch(opcode & 0xfc000000){
	case 0x00000000:
	    if((opcode & 0xfc0007ff) == 0x00000200){
		printf("attn\t0x%x\n", (unsigned int)((opcode >> 11) & 0x7fff));
		break;
	    }
	    printf(".long 0x%08x\n", (unsigned int)opcode);
	    break;
	case 0x38000000:
	    if(RA(opcode) == 0)
		printf("li\tr%lu,", RT(opcode));
	    else
		printf("addi\tr%lu,r%lu,", RT(opcode), RA(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols,
			    nsorted_symbols, strings, strings_size, verbose);
	    printf("\n");
	    break;
	case 0x3c000000:
	    if(RA(opcode) == 0)
		printf("lis\tr%lu,", RT(opcode));
	    else
		printf("addis\tr%lu,r%lu,", RT(opcode), RA(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols,
			    nsorted_symbols, strings, strings_size, verbose);
	    printf("\n");
	    break;
	case 0x30000000:
	    printf("addic\tr%lu,r%lu,", RT(opcode), RA(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols,
			    nsorted_symbols, strings, strings_size, verbose);
	    printf("\n");
	    break;
	case 0x34000000:
	    printf("addic.\tr%lu,r%lu,", RT(opcode), RA(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    printf("\n");
	    break;
	case 0x20000000:
	    printf("subfic\tr%lu,r%lu,", RT(opcode), RA(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    printf("\n");
	    break;
	case 0x1c000000:
	    printf("mulli\tr%lu,r%lu,", RT(opcode), RA(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    printf("\n");
	    break;
	case 0x28000000:
	    if(Zflag == TRUE)
		printf("cmpli\tcr%lu,%ld,r%lu,", BF(opcode), L(opcode),
		       RA(opcode));
	    else if(BF(opcode) == 0)
		printf("cmpl%si\tr%lu,", L(opcode) == 0 ? "w" : "d",RA(opcode));
	    else
		printf("cmpl%si\tcr%lu,r%lu,", L(opcode) == 0 ?"w":"d",
		       BF(opcode), RA(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    printf("\n");
	    break;
	case 0x2c000000:
	    if(Zflag == TRUE)
		printf("cmpi\tcr%lu,%ld,r%lu,", BF(opcode), L(opcode),
			RA(opcode));
	    else if(BF(opcode) == 0)
		printf("cmp%si\tr%lu,", L(opcode) == 0 ? "w" : "d",RA(opcode));
	    else
		printf("cmp%si\tcr%lu,r%lu,", L(opcode) == 0 ?"w":"d",
		       BF(opcode), RA(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    printf("\n");
	    break;
	case 0x48000000:
	    if((opcode & 0xfc000003) == 0x48000001 && 
	       get_reloc_r_type(sect_offset,relocs, nrelocs) == PPC_RELOC_JBSR){
		printf("jbsr\t");
		jbsr = TRUE;
	    }
	    else if((opcode & 0xfc000003) == 0x48000000 && 
	       get_reloc_r_type(sect_offset,relocs, nrelocs) == PPC_RELOC_JBSR){
		printf("jmp\t");
		jbsr = TRUE;
	    }
	    else{
		printf("b%s%s\t", LK(opcode), AA(opcode));
		jbsr = FALSE;
	    }
	    if((opcode & 0x00000002) != 0)
		base = 0;
	    else
		base = addr;
	    if((opcode & 0x02000000) == 0)
		disp = opcode & 0x03fffffc;
	    else
		disp = (opcode & 0x03fffffc) | 0xfc000000;
	    if(jbsr == TRUE){
		print_immediate(sect_addr, sect_offset,
		    relocs, nrelocs, symbols, symbols64, nsymbols,
		    sorted_symbols, nsorted_symbols, strings, strings_size,
		    verbose);
		printf(",0x%x\n", (unsigned int)(base + disp));
	    }
	    else{
		print_immediate(base + disp, sect_offset,
			relocs, nrelocs, symbols, symbols64,  nsymbols,
			sorted_symbols, nsorted_symbols, strings, strings_size,
			verbose);
		if(verbose){
		    indirect_symbol_name = guess_indirect_symbol(base + disp,
			ncmds, sizeofcmds, load_commands, object_byte_sex,
			indirect_symbols, nindirect_symbols, symbols, symbols64,
			nsymbols, strings, strings_size);
		    if(indirect_symbol_name != NULL)
			printf("\t; symbol stub for: %s", indirect_symbol_name);
		}
		printf("\n");
	    }
	    break;
	case 0x40000000:
	    if(bc("", opcode, sect_offset, relocs, nrelocs) == 0)
		printf("\t");
	    else
		printf(",");
	    if((opcode & 0x00000002) != 0)
		base = 0;
	    else
		base = addr;
	    if((opcode & 0x00008000) == 0)
		print_immediate(base + (opcode & 0x0000fffc), sect_offset,
		    relocs, nrelocs, symbols, symbols64, nsymbols,
		    sorted_symbols, nsorted_symbols, strings, strings_size,
		    verbose);
	    else
		print_immediate(base +
		    ((opcode & 0x0000fffc) | 0xffff0000), sect_offset,
		    relocs, nrelocs, symbols, symbols64, nsymbols,
		    sorted_symbols, nsorted_symbols, strings, strings_size,
		    verbose);
	    printf("\n");
	    break;
	case 0x88000000:
	    printf("lbz\tr%lu,", RT(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    if(RA(opcode) == 0)
		printf("(0)\n");
	    else
		printf("(r%lu)\n", RA(opcode));
	    break;
	case 0x8c000000:
	    printf("lbzu\tr%lu,", RT(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    if(RA(opcode) == 0)
		printf("(0)\n");
	    else
		printf("(r%lu)\n", RA(opcode));
	    break;
	case 0xa0000000:
	    printf("lhz\tr%lu,", RT(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    if(RA(opcode) == 0)
		printf("(0)\n");
	    else
		printf("(r%lu)\n", RA(opcode));
	    break;
	case 0xa4000000:
	    printf("lhzu\tr%lu,", RT(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    if(RA(opcode) == 0)
		printf("(0)\n");
	    else
		printf("(r%lu)\n", RA(opcode));
	    break;
	case 0xa8000000:
	    printf("lha\tr%lu,", RT(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    if(RA(opcode) == 0)
		printf("(0)\n");
	    else
		printf("(r%lu)\n", RA(opcode));
	    break;
	case 0xac000000:
	    printf("lhau\tr%lu,", RT(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    printf("(r%lu)\n", RA(opcode));
	    break;
	case 0x80000000:
	    printf("lwz\tr%lu,", RT(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    if(RA(opcode) == 0)
		printf("(0)\n");
	    else
		printf("(r%lu)\n", RA(opcode));
	    break;
	case 0x84000000:
	    printf("lwzu\tr%lu,", RT(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    printf("(r%lu)\n", RA(opcode));
	    break;
	case 0x98000000:
	    printf("stb\tr%lu,", RT(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    if(RA(opcode) == 0)
		printf("(0)\n");
	    else
		printf("(r%lu)\n", RA(opcode));
	    break;
	case 0x9c000000:
	    printf("stbu\tr%lu,", RT(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    printf("(r%lu)\n", RA(opcode));
	    break;
	case 0xb0000000:
	    printf("sth\tr%lu,", RT(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    if(RA(opcode) == 0)
		printf("(0)\n");
	    else
		printf("(r%lu)\n", RA(opcode));
	    break;
	case 0xb4000000:
	    printf("sthu\tr%lu,", RT(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    printf("(r%lu)\n", RA(opcode));
	    break;
	case 0x90000000:
	    printf("stw\tr%lu,", RT(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    if(RA(opcode) == 0)
		printf("(0)\n");
	    else
		printf("(r%lu)\n", RA(opcode));
	    break;
	case 0x94000000:
	    printf("stwu\tr%lu,", RT(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    printf("(r%lu)\n", RA(opcode));
	    break;
	case 0xb8000000:
	    printf("lmw\tr%lu,", RT(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    if(RA(opcode) == 0)
		printf("(0)\n");
	    else
		printf("(r%lu)\n", RA(opcode));
	    break;
	case 0xbc000000:
	    printf("stmw\tr%lu,", RT(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    if(RA(opcode) == 0)
		printf("(0)\n");
	    else
		printf("(r%lu)\n", RA(opcode));
	    break;
	case 0x08000000:
	    trap("d", opcode);
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    printf("\n");
	    break;
	case 0x0c000000:
	    trap("w", opcode);
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    printf("\n");
	    break;
	case 0x70000000:
	    printf("andi.\tr%lu,r%lu,", RA(opcode), RS(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    printf("\n");
	    break;
	case 0x74000000:
	    printf("andis.\tr%lu,r%lu,", RA(opcode), RS(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    printf("\n");
	    break;
	case 0x60000000:
	    if(opcode == 0x60000000)
		printf("nop\n");
	    else{
		printf("ori\tr%lu,r%lu,", RA(opcode), RS(opcode));
		print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
				symbols, symbols64, nsymbols, sorted_symbols,
				nsorted_symbols, strings, strings_size,verbose);
		printf("\n");
	    }
	    break;
	case 0x64000000:
	    printf("oris\tr%lu,r%lu,", RA(opcode), RS(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    printf("\n");
	    break;
	case 0x68000000:
	    printf("xori\tr%lu,r%lu,", RA(opcode), RS(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    printf("\n");
	    break;
	case 0x6c000000:
	    printf("xoris\tr%lu,r%lu,", RA(opcode), RS(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    printf("\n");
	    break;
	case 0xc0000000:
	    printf("lfs\tf%lu,", FRT(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    if(RA(opcode) == 0)
		printf("(0)\n");
	    else
		printf("(r%lu)\n", RA(opcode));
	    break;
	case 0xc4000000:
	    printf("lfsu\tf%lu,", FRT(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    printf("(r%lu)\n", RA(opcode));
	    break;
	case 0xc8000000:
	    printf("lfd\tf%lu,", FRT(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    if(RA(opcode) == 0)
		printf("(0)\n");
	    else
		printf("(r%lu)\n", RA(opcode));
	    break;
	case 0xcc000000:
	    printf("lfdu\tf%lu,", FRT(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    printf("(r%lu)\n", RA(opcode));
	    break;
	case 0xd0000000:
	    printf("stfs\tf%lu,", FRT(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    if(RA(opcode) == 0)
		printf("(0)\n");
	    else
		printf("(r%lu)\n", RA(opcode));
	    break;
	case 0xd4000000:
	    printf("stfsu\tf%lu,", FRT(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    printf("(r%lu)\n", RA(opcode));
	    break;
	case 0xd8000000:
	    printf("stfd\tf%lu,", FRT(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    if(RA(opcode) == 0)
		printf("(0)\n");
	    else
		printf("(r%lu)\n", RA(opcode));
	    break;
	case 0xdc000000:
	    printf("stfdu\tf%lu,", FRT(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    printf("(r%lu)\n", RA(opcode));
	    break;
	case 0x24000000:
	    printf("dozi\tr%lu,r%lu,", RT(opcode), RA(opcode));
	    print_immediate(opcode & 0xffff, sect_offset, relocs, nrelocs,
			    symbols, symbols64, nsymbols, sorted_symbols, 
			    nsorted_symbols, strings, strings_size, verbose);
	    break;
	case 0xe8000000:
	    switch(opcode & 0x3){
	    case 0:
		printf("ld\tr%lu,", RT(opcode));
		print_immediate(opcode & 0xfffc, sect_offset, relocs, nrelocs,
				symbols, symbols64, nsymbols, sorted_symbols,
				nsorted_symbols, strings, strings_size,verbose);
		if(RA(opcode) == 0)
		    printf("(0)\n");
		else
		    printf("(r%lu)\n", RA(opcode));
		break;
	    case 1:
		printf("ldu\tr%lu,", RT(opcode));
		print_immediate(opcode & 0xfffc, sect_offset, relocs, nrelocs,
				symbols, symbols64, nsymbols, sorted_symbols,
				nsorted_symbols, strings, strings_size,verbose);
		printf("(r%lu)\n", RA(opcode));
		break;
	    case 2:
		printf("lwa\tr%lu,", RT(opcode));
		print_immediate(opcode & 0xfffc, sect_offset, relocs, nrelocs,
				symbols, symbols64, nsymbols, sorted_symbols,
				nsorted_symbols, strings, strings_size,verbose);
		if(RA(opcode) == 0)
		    printf("(0)\n");
		else
		    printf("(r%lu)\n", RA(opcode));
		break;
	    default:
		printf(".long 0x%08x\n", (unsigned int)opcode);
		break;
	    }
	    break;
	case 0xf8000000:
	    switch(opcode & 0x3){
	    case 0:
		printf("std\tr%lu,", RT(opcode));
		print_immediate(opcode & 0xfffc, sect_offset, relocs, nrelocs,
				symbols, symbols64, nsymbols, sorted_symbols,
				nsorted_symbols, strings, strings_size,verbose);
		if(RA(opcode) == 0)
		    printf("(0)\n");
		else
		    printf("(r%lu)\n", RA(opcode));
		break;
	    case 1:
		printf("stdu\tr%lu,", RT(opcode));
		print_immediate(opcode & 0xfffc, sect_offset, relocs, nrelocs,
				symbols, symbols64, nsymbols, sorted_symbols,
				nsorted_symbols, strings, strings_size,verbose);
		printf("(r%lu)\n", RA(opcode));
		break;
	    default:
		printf(".long 0x%08x\n", (unsigned int)opcode);
		break;
	    }
	    break;
	case 0x7c000000:
	    switch(opcode & 0x000007fe){
	    case 0x00000214:
	    case 0x00000614:
		xo_form("add", opcode, 3);
		break;
	    case 0x00000050:
	    case 0x00000450:
		xo_form("subf", opcode, 3);
		break;
	    case 0x00000014:
	    case 0x00000414:
		xo_form("addc", opcode, 3);
		break;
	    case 0x00000114:
	    case 0x00000514:
		xo_form("adde", opcode, 3);
		break;
	    case 0x00000010:
	    case 0x00000410:
		xo_form("subfc", opcode, 3);
		break;
	    case 0x00000110:
	    case 0x00000510:
		xo_form("subfe", opcode, 3);
		break;
	    case 0x000001d4:
	    case 0x000005d4:
		xo_form("addme", opcode, 2);
		break;
	    case 0x00000194:
	    case 0x00000594:
		xo_form("addze", opcode, 2);
		break;
	    case 0x000001d0:
	    case 0x000005d0:
		xo_form("subfme", opcode, 2);
		break;
	    case 0x00000190:
	    case 0x00000590:
		xo_form("subfze", opcode, 2);
		break;
	    case 0x000000d0:
	    case 0x000004d0:
		xo_form("neg", opcode, 2);
		break;
	    case 0x000001d6:
	    case 0x000005d6:
		xo_form("mullw", opcode, 3);
		break;
	    case 0x000001d2:
	    case 0x000005d2:
		xo_form("mulld", opcode, 3);
		break;
	    case 0x00000092:
	    case 0x00000492:
		xo_form("mulhd", opcode, 3);
		break;
	    case 0x00000096:
	    case 0x00000496:
		xo_form("mulhw", opcode, 3);
		break;
	    case 0x00000012:
	    case 0x00000412:
		xo_form("mulhdu", opcode, 3);
		break;
	    case 0x00000016:
	    case 0x00000416:
		xo_form("mulhwu", opcode, 3);
		break;
	    case 0x000003d2:
	    case 0x000007d2:
		xo_form("divd", opcode, 3);
		break;
	    case 0x000003d6:
	    case 0x000007d6:
		xo_form("divw", opcode, 3);
		break;
	    case 0x00000392:
	    case 0x00000792:
		xo_form("divdu", opcode, 3);
		break;
	    case 0x00000396:
	    case 0x00000796:
		xo_form("divwu", opcode, 3);
		break;

	    case 0x000000ae:
		if(RA(opcode) == 0)
		    printf("lbzx\tr%lu,0,r%lu\n", RT(opcode), RB(opcode));
		else
		    printf("lbzx\tr%lu,r%lu,r%lu\n", RT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x000000ee:
		x_form("lbzux", opcode, 3);
		break;
	    case 0x0000022e:
		if(RA(opcode) == 0)
		    printf("lhzx\tr%lu,0,r%lu\n", RT(opcode), RB(opcode));
		else
		    printf("lhzx\tr%lu,r%lu,r%lu\n", RT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x0000026e:
		x_form("lhzux", opcode, 3);
		break;
	    case 0x000002ae:
		if(RA(opcode) == 0)
		    printf("lhax\tr%lu,0,r%lu\n", RT(opcode), RB(opcode));
		else
		    printf("lhax\tr%lu,r%lu,r%lu\n", RT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x000002ee:
		x_form("lhaux", opcode, 3);
		break;
	    case 0x0000002e:
		if(RA(opcode) == 0)
		    printf("lwzx\tr%lu,0,r%lu\n", RT(opcode), RB(opcode));
		else
		    printf("lwzx\tr%lu,r%lu,r%lu\n", RT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x0000006e:
		x_form("lwzux", opcode, 3);
		break;
	    case 0x000002aa:
		if(RA(opcode) == 0)
		    printf("lwax\tr%lu,0,r%lu\n", RT(opcode), RB(opcode));
		else
		    printf("lwax\tr%lu,r%lu,r%lu\n", RT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x000002ea:
		x_form("lwaux", opcode, 3);
		break;
	    case 0x0000002a:
		if(RA(opcode) == 0)
		    printf("ldx\tr%lu,0,r%lu\n", RT(opcode), RB(opcode));
		else
		    printf("ldx\tr%lu,r%lu,r%lu\n", RT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x0000006a:
		x_form("ldux", opcode, 3);
		break;
	    case 0x000001ae:
		if(RA(opcode) == 0)
		    printf("stbx\tr%lu,0,r%lu\n", RT(opcode), RB(opcode));
		else
		    printf("stbx\tr%lu,r%lu,r%lu\n", RT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x000001ee:
		x_form("stbux", opcode, 3);
		break;
	    case 0x0000032e:
		if(RA(opcode) == 0)
		    printf("sthx\tr%lu,0,r%lu\n", RT(opcode), RB(opcode));
		else
		    printf("sthx\tr%lu,r%lu,r%lu\n", RT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x0000036e:
		x_form("sthux", opcode, 3);
		break;
	    case 0x0000012e:
		if(RA(opcode) == 0)
		    printf("stwx\tr%lu,0,r%lu\n", RT(opcode), RB(opcode));
		else
		    printf("stwx\tr%lu,r%lu,r%lu\n", RT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x0000016e:
		x_form("stwux", opcode, 3);
		break;
	    case 0x0000012a:
		if(RA(opcode) == 0)
		    printf("stdx\tr%lu,0,r%lu\n", RT(opcode), RB(opcode));
		else
		    printf("stdx\tr%lu,r%lu,r%lu\n", RT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x0000016a:
		x_form("stdux", opcode, 3);
		break;
	    case 0x0000062c:
		if(RA(opcode) == 0)
		    printf("lhbrx\tr%lu,0,r%lu\n", RT(opcode), RB(opcode));
		else
		    printf("lhbrx\tr%lu,r%lu,r%lu\n", RT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x0000042c:
		if(RA(opcode) == 0)
		    printf("lwbrx\tr%lu,0,r%lu\n", RT(opcode), RB(opcode));
		else
		    printf("lwbrx\tr%lu,r%lu,r%lu\n", RT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x0000072c:
		if(RA(opcode) == 0)
		    printf("sthbrx\tr%lu,0,r%lu\n", RT(opcode), RB(opcode));
		else
		    printf("sthbrx\tr%lu,r%lu,r%lu\n", RT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x0000052c:
		if(RA(opcode) == 0)
		    printf("stwbrx\tr%lu,0,r%lu\n", RT(opcode), RB(opcode));
		else
		    printf("stwbrx\tr%lu,r%lu,r%lu\n", RT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x0000042a:
		if(RA(opcode) == 0)
		    printf("lswx\tr%lu,0,r%lu\n", RT(opcode), RB(opcode));
		else
		    printf("lswx\tr%lu,r%lu,r%lu\n", RT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x0000052a:
		if(RA(opcode) == 0)
		    printf("stswx\tr%lu,0,r%lu\n", RT(opcode), RB(opcode));
		else
		    printf("stswx\tr%lu,r%lu,r%lu\n", RT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x00000028:
		if(RA(opcode) == 0)
		    printf("lwarx\tr%lu,0,r%lu\n", RT(opcode), RB(opcode));
		else
		    printf("lwarx\tr%lu,r%lu,r%lu\n", RT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x000000a8:
		if(RA(opcode) == 0)
		    printf("ldarx\tr%lu,0,r%lu\n", RT(opcode), RB(opcode));
		else
		    printf("ldarx\tr%lu,r%lu,r%lu\n", RT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x0000022a:
		if(RA(opcode) == 0)
		    printf("lscbx%s\tr%lu,0,r%lu\n", Rc(opcode), RT(opcode),
			   RB(opcode));
		else
		    printf("lscbx%s\tr%lu,r%lu,r%lu\n", Rc(opcode), RT(opcode),
			   RA(opcode), RB(opcode));
		break;
	    case 0x0000012c:
		if(RA(opcode) == 0)
		    printf("stwcx.\tr%lu,0,r%lu\n", RT(opcode), RB(opcode));
		else
		    printf("stwcx.\tr%lu,r%lu,r%lu\n", RT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x000001ac:
		if(RA(opcode) == 0)
		    printf("stdcx.\tr%lu,0,r%lu\n", RT(opcode), RB(opcode));
		else
		    printf("stdcx.\tr%lu,r%lu,r%lu\n", RT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x00000038:
		sx_form("and", opcode, 3);
		break;
	    case 0x00000278:
		sx_form("xor", opcode, 3);
		break;
	    case 0x00000378:
		sx_form("or", opcode, 3);
		break;
	    case 0x000003b8:
		sx_form("nand", opcode, 3);
		break;
	    case 0x000000f8:
		sx_form("nor", opcode, 3);
		break;
	    case 0x00000078:
		sx_form("andc", opcode, 3);
		break;
	    case 0x00000238:
		sx_form("eqv", opcode, 3);
		break;
	    case 0x00000338:
		sx_form("orc", opcode, 3);
		break;
	    case 0x00000774:
		sx_form("extsb", opcode, 2);
		break;
	    case 0x000007b4:
		sx_form("extsw", opcode, 2);
		break;
	    case 0x00000734:
		sx_form("extsh", opcode, 2);
		break;
	    case 0x00000074:
		sx_form("cntlzd", opcode, 2);
		break;
	    case 0x00000034:
		sx_form("cntlzw", opcode, 2);
		break;
	    case 0x00000036:
		sx_form("sld", opcode, 3);
		break;
	    case 0x00000030:
		sx_form("slw", opcode, 3);
		break;
	    case 0x00000436:
		sx_form("srd", opcode, 3);
		break;
	    case 0x00000430:
		sx_form("srw", opcode, 3);
		break;
	    case 0x00000634:
		sx_form("srad", opcode, 3);
		break;
	    case 0x00000630:
		sx_form("sraw", opcode, 3);
		break;
	    case 0x000004aa:
		if(RA(opcode) == 0)
		    printf("lswi\tr%lu,0,%lu\n", RT(opcode), NB(opcode));
		else
		    printf("lswi\tr%lu,r%lu,%lu\n", RT(opcode), RA(opcode),
			   NB(opcode));
		break;
	    case 0x000005aa:
		if(RA(opcode) == 0)
		    printf("stswi\tr%lu,0,%lu\n", RT(opcode), NB(opcode));
		else
		    printf("stswi\tr%lu,r%lu,%lu\n", RT(opcode), RA(opcode),
			   NB(opcode));
		break;
	    case 0x000004ac:
		switch((opcode >> 21) & 0x3){
		case 0:
		    printf("sync\n");
		    break;
		case 1:
		    printf("lwsync\n");
		    break;
		case 2:
		    printf("ptesync\n");
		    break;
		case 3:
		    printf("sync\t3\n");
		    break;
		}
		break;
	    case 0x00000000:
		if(Zflag == TRUE)
		    printf("cmp\tcr%lu,%ld,r%lu,r%lu\n",
			   BF(opcode), L(opcode), RA(opcode), RB(opcode));
		else if(BF(opcode) == 0)
		    printf("cmp%s\tr%lu,r%lu\n", L(opcode) == 0 ? "w" : "d",
			   RA(opcode), RB(opcode));
		else
		    printf("cmp%s\tcr%lu,r%lu,r%lu\n", L(opcode) == 0 ? "w":"d",
			   BF(opcode), RA(opcode), RB(opcode));
		break;
	    case 0x00000040:
		if(Zflag == TRUE)
		    printf("cmpl\tcr%lu,%ld,r%lu,r%lu\n", BF(opcode), L(opcode),
			   RA(opcode), RB(opcode));
		else if(BF(opcode) == 0)
		    printf("cmpl%s\tr%lu,r%lu\n", L(opcode) == 0 ? "w" : "d",
			   RA(opcode), RB(opcode));
		else
		    printf("cmpl%s\tcr%lu,r%lu,r%lu\n", L(opcode) == 0 ?"w":"d",
			   BF(opcode), RA(opcode), RB(opcode));
		break;
	    case 0x00000088:
		trap("d", opcode);
		printf("r%lu\n", RB(opcode));
		break;
	    case 0x00000008:
		if(opcode == 0x7fe00008){
		    printf("trap\n");
		}
		else{
		    trap("w", opcode);
		    printf("r%lu\n", RB(opcode));
		}
		break;
	    case 0x00000670:
		printf("srawi%s\tr%lu,r%lu,%lu\n", Rc(opcode), RA(opcode),
		       RS(opcode), SH(opcode));
		break;
	    case 0x00000674:
	    case 0x00000676:
		printf("sradi%s\tr%lu,r%lu,%lu\n", Rc(opcode), RA(opcode),
		       RS(opcode), ((opcode & 0x2) << 4) | SH(opcode));
		break;
	    case 0x000003a6:
		if((opcode & 0xfc1fffff) == 0x7c1c43a6){
		    printf("mttbl\t");
		    printf("r%lu\n", RS(opcode));
		}
		else if((opcode & 0xfc1fffff) == 0x7c1d43a6){
		    printf("mttbu\t");
		    printf("r%lu\n", RS(opcode));
		}
		else{
		    printf("mtspr\t");
		    print_special_register_name(opcode);
		    printf(",r%lu\n", RS(opcode));
		}
		break;
	    case 0x000002a6:
		printf("mfspr\tr%lu,", RT(opcode));
		print_special_register_name(opcode);
		printf("\n");
		break;
	    case 0x00000120:
		if(opcode & 0x00100000)
		    printf("mtocrf\t0x%02x,r%lu\n",
			   (unsigned int)((opcode >> 12) & 0xff),
			   RS(opcode));
		else
		    printf("mtcrf\t%lu,r%lu\n", (opcode >> 12) & 0xff,
			   RS(opcode));
		break;
	    case 0x00000400:
		printf("mcrxr\tcr%lu\n", BF(opcode));
		break;
	    case 0x00000026:
		if(opcode & 0x00100000)
		    printf("mfocrf\tr%lu,0x%02x\n", RS(opcode),
			   (unsigned int)((opcode >> 12) & 0xff));
		else
		    printf("mfcr\tr%lu\n", RT(opcode));
		break;
	    case 0x0000042e:
		if(RA(opcode) == 0)
		    printf("lfsx\tf%lu,0,r%lu\n", RT(opcode), RB(opcode));
		else
		    printf("lfsx\tf%lu,r%lu,r%lu\n", RT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x0000046e:
		fx_form("lfsux", opcode, 3);
		break;
	    case 0x000004ae:
		if(RA(opcode) == 0)
		    printf("lfdx\tf%lu,0,r%lu\n", RT(opcode), RB(opcode));
		else
		    printf("lfdx\tf%lu,r%lu,r%lu\n", RT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x000004ee:
		fx_form("lfdux", opcode, 3);
		break;
	    case 0x0000052e:
		if(RA(opcode) == 0)
		    printf("stfsx\tf%lu,0,r%lu\n", RT(opcode), RB(opcode));
		else
		    printf("stfsx\tf%lu,r%lu,r%lu\n", RT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x0000056e:
		fx_form("stfsux", opcode, 3);
		break;
	    case 0x000005ae:
		if(RA(opcode) == 0)
		    printf("stfdx\tf%lu,0,r%lu\n", RT(opcode), RB(opcode));
		else
		    printf("stfdx\tf%lu,r%lu,r%lu\n", RT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x000007ae:
		if(RA(opcode) == 0)
		    printf("stfiwx\tf%lu,0,r%lu\n", RT(opcode), RB(opcode));
		else
		    printf("stfiwx\tf%lu,r%lu,r%lu\n", RT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x000005ee:
		fx_form("stfdux", opcode, 3);
		break;
	    case 0x000007ac:
		if(RA(opcode) == 0)
		    printf("icbi\t0,r%lu\n", RB(opcode));
		else
		    printf("icbi\tr%lu,r%lu\n", RA(opcode), RB(opcode));
		break;
	    case 0x0000022c:
		if(RT(opcode) != 0){
		    if(RA(opcode) == 0)
			printf("dcbt\t0,r%lu,0x%x\n", RB(opcode),
			       (unsigned int)TH(opcode));
		    else
			printf("dcbt\tr%lu,r%lu,0x%x\n", RA(opcode),
			       RB(opcode), (unsigned int)TH(opcode));
		}
		else{
		    if(RA(opcode) == 0)
			printf("dcbt\t0,r%lu\n", RB(opcode));
		    else
			printf("dcbt\tr%lu,r%lu\n", RA(opcode), RB(opcode));
		}
		break;
	    case 0x000001ec:
		if(RA(opcode) == 0)
		    printf("dcbtst\t0,r%lu\n", RB(opcode));
		else
		    printf("dcbtst\tr%lu,r%lu\n", RA(opcode), RB(opcode));
		break;
	    case 0x000007ec:
		if((opcode & 0x00200000) == 0x00200000){
		    if(RA(opcode) == 0)
			printf("dcbzl\t0,r%lu\n", RB(opcode));
		    else
			printf("dcbzl\tr%lu,r%lu\n", RA(opcode), RB(opcode));
		}
		else{
		    if(RA(opcode) == 0)
			printf("dcbz\t0,r%lu\n", RB(opcode));
		    else
			printf("dcbz\tr%lu,r%lu\n", RA(opcode), RB(opcode));
		}
		break;
	    case 0x0000006c:
		if(RA(opcode) == 0)
		    printf("dcbst\t0,r%lu\n", RB(opcode));
		else
		    printf("dcbst\tr%lu,r%lu\n", RA(opcode), RB(opcode));
		break;
	    case 0x000000ac:
		if(RA(opcode) == 0)
		    printf("dcbf\t0,r%lu\n", RB(opcode));
		else
		    printf("dcbf\tr%lu,r%lu\n", RA(opcode), RB(opcode));
		break;
	    case 0x0000026c:
		if(RA(opcode) == 0)
		    printf("eciwx\tr%lu,0,r%lu\n", RT(opcode), RB(opcode));
		else
		    printf("eciwx\tr%lu,r%lu,r%lu\n", RT(opcode), RA(opcode),
			    RB(opcode));
		break;
	    case 0x0000036c:
		if(RA(opcode) == 0)
		    printf("ecowx\tr%lu,0,r%lu\n", RT(opcode), RB(opcode));
		else
		    printf("ecowx\tr%lu,r%lu,r%lu\n", RT(opcode), RA(opcode),
			    RB(opcode));
		break;
	    case 0x000006ac:
		printf("eieio\n");
		break;
	    case 0x00000124:
		printf("mtmsr\tr%lu\n", RS(opcode));
		break;
	    case 0x00000164:
		if((opcode & 0x00010000) == 0)
		    printf("mtmsrd\tr%lu\n", RS(opcode));
		else
		    printf("mtmsrd\tr%lu,1\n", RS(opcode));
		break;
	    case 0x000000a6:
		printf("mfmsr\tr%lu\n", RS(opcode));
		break;
	    case 0x000003ac:
		if(RA(opcode) == 0)
		    printf("dcbi\t0,r%lu\n", RB(opcode));
		else
		    printf("dcbi\tr%lu,r%lu\n", RA(opcode), RB(opcode));
		break;
	    case 0x000005ec:
		if(RA(opcode) == 0)
		    printf("dcba\t0,r%lu\n", RB(opcode));
		else
		    printf("dcba\tr%lu,r%lu\n", RA(opcode), RB(opcode));
		break;
	    case 0x000001a4:
		printf("mtsr\tsr%lu,r%lu\n", SR(opcode), RS(opcode));
		break;
	    case 0x000004a6:
		printf("mfsr\tr%lu,sr%lu\n", RT(opcode), SR(opcode));
		break;
	    case 0x000001e4:
		printf("mtsrin\tr%lu,r%lu\n", RS(opcode), RB(opcode));
		break;
	    case 0x00000526:
		printf("mfsrin\tr%lu,r%lu\n", RT(opcode), RB(opcode));
		break;
	    case 0x00000264:
		if((opcode & 0x00200000) == 0)
		    printf("tlbie\tr%lu\n", RB(opcode));
		else
		    printf("tlbie\tr%lu,1\n", RB(opcode));
		break;
	    case 0x00000224:
		printf("tlbiel\tr%lu\n", RB(opcode));
		break;
	    case 0x0000046c:
		printf("tlbsync\n");
		break;
	    case 0x000002e4:
		printf("tlbia\n");
		break;
	    case 0x000002e6:
		switch(opcode & 0x001ff800){
		case 0x000c4000:
		    printf("mftb\tr%lu\n", RT(opcode));
		    break;
		case 0x000d4000:
		    printf("mftbu\tr%lu\n", RT(opcode));
		    break;
		default:
		    printf("mftb\tr%lu,%lu\n", RT(opcode),
			   ((opcode >> 16) & 0x1f) | ((opcode >> 6) & 0x3e0));
		}
		break;
	    case 0x00000210:
	    case 0x00000610:
		xo_form("doz", opcode, 3);
		break;
	    case 0x000002d0:
	    case 0x000006d0:
		xo_form("abs", opcode, 2);
		break;
	    case 0x000003d0:
	    case 0x000007d0:
		xo_form("nabs", opcode, 2);
		break;
	    case 0x000000d6:
	    case 0x000004d6:
		xo_form("mul", opcode, 3);
		break;
	    case 0x00000296:
	    case 0x00000696:
		xo_form("div", opcode, 3);
		break;
	    case 0x000002d6:
	    case 0x000006d6:
		xo_form("divs", opcode, 3);
		break;
	    case 0x00000432:
		sx_form("rrib", opcode, 3);
		break;
	    case 0x0000003a:
		sx_form("maskg", opcode, 3);
		break;
	    case 0x0000043a:
		sx_form("maskir", opcode, 3);
		break;
	    case 0x00000130:
		sx_form("slq", opcode, 3);
		break;
	    case 0x00000530:
		sx_form("srq", opcode, 3);
		break;
	    case 0x00000170:
		printf("sliq%s\tr%lu,r%lu,%lu\n", Rc(opcode), RA(opcode),
		       RS(opcode), SH(opcode));
		break;
	    case 0x00000570:
		printf("sriq%s\tr%lu,r%lu,%lu\n", Rc(opcode), RA(opcode),
		       RS(opcode), SH(opcode));
		break;
	    case 0x000001f0:
		printf("slliq%s\tr%lu,r%lu,%lu\n", Rc(opcode), RA(opcode),
		       RS(opcode), SH(opcode));
		break;
	    case 0x000005f0:
		printf("srliq%s\tr%lu,r%lu,%lu\n", Rc(opcode), RA(opcode),
		       RS(opcode), SH(opcode));
		break;
	    case 0x000001b0:
		sx_form("sllq", opcode, 3);
		break;
	    case 0x000005b0:
		sx_form("srlq", opcode, 3);
		break;
	    case 0x00000132:
		sx_form("sle", opcode, 3);
		break;
	    case 0x00000532:
		sx_form("sre", opcode, 3);
		break;
	    case 0x000001b2:
		sx_form("sleq", opcode, 3);
		break;
	    case 0x000005b2:
		sx_form("sreq", opcode, 3);
		break;
	    case 0x00000770:
		printf("sraiq%s\tr%lu,r%lu,%lu\n", Rc(opcode), RA(opcode),
		       RS(opcode), SH(opcode));
		break;
	    case 0x00000730:
		sx_form("sraq", opcode, 3);
		break;
	    case 0x00000732:
		sx_form("srea", opcode, 3);
		break;
	    case 0x00000426:
		printf("clcs\tr%lu,r%lu\n", RT(opcode), RA(opcode));
		break;
	    case 0x000007a4:
		printf("tlbld\tr%lu\n", RB(opcode));
		break;
	    case 0x000007e4:
		printf("tlbli\tr%lu\n", RB(opcode));
		break;
	    case 0x00000364:
		printf("slbie\tr%lu\n", RB(opcode));
		break;
	    case 0x000003e4:
		printf("slbia\n");
		break;
	    case 0x00000324:
		printf("slbmte\tr%lu,r%lu\n", RS(opcode), RB(opcode));
		break;
	    case 0x000006a6:
		printf("slbmfev\tr%lu,r%lu\n", RS(opcode), RB(opcode));
		break;
	    case 0x00000726:
		printf("slbmfee\tr%lu,r%lu\n", RS(opcode), RB(opcode));
		break;
	    case 0x0000000e:
		if(RA(opcode) == 0)
		    printf("lvebx\tv%lu,0,r%lu\n", VT(opcode), RB(opcode));
		else
		    printf("lvebx\tv%lu,r%lu,r%lu\n", VT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x0000004e:
		if(RA(opcode) == 0)
		    printf("lvehx\tv%lu,0,r%lu\n", VT(opcode), RB(opcode));
		else
		    printf("lvehx\tv%lu,r%lu,r%lu\n", VT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x0000008e:
		if(RA(opcode) == 0)
		    printf("lvewx\tv%lu,0,r%lu\n", VT(opcode), RB(opcode));
		else
		    printf("lvewx\tv%lu,r%lu,r%lu\n", VT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x000000ce:
		if(RA(opcode) == 0)
		    printf("lvx\tv%lu,0,r%lu\n", VT(opcode), RB(opcode));
		else
		    printf("lvx\tv%lu,r%lu,r%lu\n", VT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x000002ce:
		if(RA(opcode) == 0)
		    printf("lvxl\tv%lu,0,r%lu\n", VT(opcode), RB(opcode));
		else
		    printf("lvxl\tv%lu,r%lu,r%lu\n", VT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x0000010e:
		if(RA(opcode) == 0)
		    printf("stvebx\tv%lu,0,r%lu\n", VT(opcode), RB(opcode));
		else
		    printf("stvebx\tv%lu,r%lu,r%lu\n", VT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x0000014e:
		if(RA(opcode) == 0)
		    printf("stvehx\tv%lu,0,r%lu\n", VT(opcode), RB(opcode));
		else
		    printf("stvehx\tv%lu,r%lu,r%lu\n", VT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x0000018e:
		if(RA(opcode) == 0)
		    printf("stvewx\tv%lu,0,r%lu\n", VT(opcode), RB(opcode));
		else
		    printf("stvewx\tv%lu,r%lu,r%lu\n", VT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x000001ce:
		if(RA(opcode) == 0)
		    printf("stvx\tv%lu,0,r%lu\n", VT(opcode), RB(opcode));
		else
		    printf("stvx\tv%lu,r%lu,r%lu\n", VT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x000003ce:
		if(RA(opcode) == 0)
		    printf("stvxl\tv%lu,0,r%lu\n", VT(opcode), RB(opcode));
		else
		    printf("stvxl\tv%lu,r%lu,r%lu\n", VT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x0000000c:
		if(RA(opcode) == 0)
		    printf("lvsl\tv%lu,0,r%lu\n", VT(opcode), RB(opcode));
		else
		    printf("lvsl\tv%lu,r%lu,r%lu\n", VT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x0000004c:
		if(RA(opcode) == 0)
		    printf("lvsr\tv%lu,0,r%lu\n", VT(opcode), RB(opcode));
		else
		    printf("lvsr\tv%lu,r%lu,r%lu\n", VT(opcode), RA(opcode),
			   RB(opcode));
		break;
	    case 0x000002ac:
		if((opcode & (1 << 25)) == 0){
		    printf("dst\tr%lu,r%lu,%lu\n", RA(opcode), RB(opcode),
			   TAG(opcode));
		} else {
		    printf("dstt\tr%lu,r%lu,%lu\n", RA(opcode), RB(opcode),
			   TAG(opcode));
		}
		break;
	    case 0x000002ec:
		if((opcode & (1 << 25)) == 0){
		    printf("dstst\tr%lu,r%lu,%lu\n", RA(opcode), RB(opcode),
			   TAG(opcode));
		} else {
		    printf("dststt\tr%lu,r%lu,%lu\n", RA(opcode),RB(opcode),
			   TAG(opcode));
		}
		break;
	    case 0x0000066c:
		if((opcode & (1 << 25)) == 0)
		    printf("dss\t%lu\n", TAG(opcode));
		else
		    printf("dssall\n");
		break;
	    default:
		printf(".long 0x%08x\n", (unsigned int)opcode);
		break;
	    }
	    break;
	case 0x4c000000:
	    switch(opcode & 0x000007fe){
	    case 0x00000000:
		printf("mcrf\tcr%lu,cr%lu\n", BF(opcode), BFA(opcode));
		break;
	    case 0x00000020:
		(void)bc("lr", opcode, sect_offset, relocs, nrelocs);
		printf("\n");
		break;
	    case 0x00000024:
		printf("rfid\n");
		break;
	    case 0x00000420:
		(void)bc("ctr", opcode, sect_offset, relocs, nrelocs);
		printf("\n");
		break;
	    case 0x00000202:
		xl_form("crand", opcode, 3);
		break;
	    case 0x00000182:
		xl_form("crxor", opcode, 3);
		break;
	    case 0x00000382:
		xl_form("cror", opcode, 3);
		break;
	    case 0x000001c2:
		xl_form("crnand", opcode, 3);
		break;
	    case 0x00000042:
		xl_form("crnor", opcode, 3);
		break;
	    case 0x00000102:
		xl_form("crandc", opcode, 3);
		break;
	    case 0x00000242:
		xl_form("creqv", opcode, 3);
		break;
	    case 0x00000342:
		xl_form("crorc", opcode, 3);
		break;
	    case 0x0000012c:
		printf("isync\n");
		break;
	    case 0x00000064:
		printf("rfi\n");
		break;
	    default:
		printf(".long 0x%08x\n", (unsigned int)opcode);
		break;
	    }
	    break;
	case 0x78000000:
	    switch(opcode & 0x0000001e){
	    case 0x00000000:
	    case 0x00000002:
		printf("rldicl%s\tr%lu,r%lu,%lu,%lu\n", Rc(opcode), RA(opcode),
		       RS(opcode), ((opcode & 0x2) << 4) | SH(opcode),
		       ((opcode & 0x20) | ((opcode >> 6) & 0x1f)) );
		break;
	    case 0x00000004:
	    case 0x00000006:
		printf("rldicr%s\tr%lu,r%lu,%lu,%lu\n", Rc(opcode), RA(opcode),
		       RS(opcode), ((opcode & 0x2) << 4) | SH(opcode),
		       ((opcode & 0x20) | ((opcode >> 6) & 0x1f)) );
		break;
	    case 0x00000008:
	    case 0x0000000a:
		printf("rldic%s\tr%lu,r%lu,%lu,%lu\n", Rc(opcode), RA(opcode),
		       RS(opcode), ((opcode & 0x2) << 4) | SH(opcode),
		       ((opcode & 0x20) | ((opcode >> 6) & 0x1f)) );
		break;
	    case 0x0000000c:
	    case 0x0000000e:
		printf("rldimi%s\tr%lu,r%lu,%lu,%lu\n", Rc(opcode), RA(opcode),
		       RS(opcode), ((opcode & 0x2) << 4) | SH(opcode),
		       ((opcode & 0x20) | ((opcode >> 6) & 0x1f)) );
		break;
	    case 0x00000010:
		printf("rldcl%s\tr%lu,r%lu,r%lu,%lu\n", Rc(opcode), RA(opcode),
		       RS(opcode), RB(opcode),
		       ((opcode & 0x20) | ((opcode >> 6) & 0x1f)) );
		break;
	    case 0x00000012:
		printf("rldcr%s\tr%lu,r%lu,r%lu,%lu\n", Rc(opcode), RA(opcode),
		       RS(opcode), RB(opcode),
		       ((opcode & 0x20) | ((opcode >> 6) & 0x1f)) );
		break;
	    default:
		printf(".long 0x%08x\n", (unsigned int)opcode);
		break;
	    }
	    break;
	case 0x44000000:
	    switch(opcode & 0x000007fe){
	    case 0x00000002:
		printf("sc\n");
		break;
	    default:
		printf(".long 0x%08x\n", (unsigned int)opcode);
		break;
	    }
	    break;
	case 0x54000000:
	    printf("rlwinm%s\tr%lu,r%lu,%lu,%lu,%lu\n", Rc(opcode), RA(opcode),
		   RS(opcode), SH(opcode), MB(opcode), ME(opcode));
	    break;
	case 0x58000000:
	    printf("rlmi%s\tr%lu,r%lu,r%lu,%lu,%lu\n", Rc(opcode), RA(opcode),
		   RS(opcode), RB(opcode), MB(opcode), ME(opcode));
	    break;
	case 0x5c000000:
	    printf("rlwnm%s\tr%lu,r%lu,r%lu,%lu,%lu\n", Rc(opcode), RA(opcode),
		   RS(opcode), RB(opcode), MB(opcode), ME(opcode));
	    break;
	case 0x50000000:
	    printf("rlwimi%s\tr%lu,r%lu,%lu,%lu,%lu\n", Rc(opcode), RA(opcode),
		   RS(opcode), SH(opcode), MB(opcode), ME(opcode));
	    break;
	case 0xfc000000:
	    switch(opcode & 0x000007fe){
	    case 0x00000090:
		fx_form("fmr", opcode, 2);
		break;
	    case 0x00000210:
		fx_form("fabs", opcode, 2);
		break;
	    case 0x00000050:
		fx_form("fneg", opcode, 2);
		break;
	    case 0x00000110:
		fx_form("fnabs", opcode, 2);
		break;
	    case 0x00000018:
		fx_form("frsp", opcode, 2);
		break;
	    case 0x00000034:
		fx_form("frsqrte", opcode, 2);
		break;
	    case 0x0000002c:
		fx_form("fsqrt", opcode, 2);
		break;
    	    case 0x0000065c:
		fx_form("fctid", opcode, 2);
		break;
	    case 0x0000065e:
		fx_form("fctidz", opcode, 2);
		break;
	    case 0x0000001c:
		fx_form("fctiw", opcode, 2);
		break;
	    case 0x0000001e:
		fx_form("fctiwz", opcode, 2);
		break;
	    case 0x0000069c:
		fx_form("fcfid", opcode, 2);
		break;
	    case 0x00000000:
		printf("fcmpu\tcr%lu,f%lu,f%lu\n", BF(opcode), FRA(opcode),
		       FRB(opcode));
		break;
	    case 0x00000040:
		printf("fcmpo\tcr%lu,f%lu,f%lu\n", BF(opcode), FRA(opcode),
		       FRB(opcode));
		break;
	    case 0x0000048e:
		printf("mffs%s\tf%lu\n", Rc(opcode), FRT(opcode));
		break;
	    case 0x00000080:
		printf("mcrfs\tcr%lu,%lu\n", BF(opcode), BFA(opcode));
		break;
	    case 0x0000010c:
		printf("mtfsfi%s\t%lu,%lu\n", Rc(opcode), BF(opcode),
		       U(opcode));
		break;
	    case 0x0000058e:
		printf("mtfsf%s\t%lu,f%lu\n", Rc(opcode), FLM(opcode),
		       FRB(opcode));
		break;
	    case 0x0000008c:
		printf("mtfsb0%s\t%lu\n", Rc(opcode), BT(opcode));
		break;
	    case 0x0000004c:
		printf("mtfsb1%s\t%lu\n", Rc(opcode), BT(opcode));
		break;
	    default:
		switch(opcode & 0x0000003e){
		case 0x0000002a:
		    a_form("fadd", opcode, 3);
		    break;
		case 0x00000028:
		    a_form("fsub", opcode, 3);
		    break;
		case 0x00000032:
		    printf("fmul%s\tf%lu,f%lu,f%lu\n", Rc(opcode), FRT(opcode),
			   FRA(opcode), FRC(opcode));
		    break;
		case 0x0000002e:
		    printf("fsel%s\tf%lu,f%lu,f%lu,f%lu\n", Rc(opcode),
			   FRT(opcode), FRA(opcode), FRC(opcode), FRB(opcode));
		    break;
		case 0x00000024:
		    a_form("fdiv", opcode, 3);
		    break;
		case 0x0000003a:
		    a_form("fmadd", opcode, 4);
		    break;
		case 0x00000038:
		    a_form("fmsub", opcode, 4);
		    break;
		case 0x0000003e:
		    a_form("fnmadd", opcode, 4);
		    break;
		case 0x0000003c:
		    a_form("fnmsub", opcode, 4);
		    break;
		default:
		    printf(".long 0x%08x\n", (unsigned int)opcode);
		    break;
		}
	    }
	    break;
	case 0xec000000:
	    switch(opcode & 0x0000003e){
	    case 0x0000002a:
		a_form("fadds", opcode, 3);
		break;
	    case 0x00000028:
		a_form("fsubs", opcode, 3);
		break;
	    case 0x00000030:
		printf("fres%s\tf%lu,f%lu\n", Rc(opcode), FRT(opcode),
		       FRB(opcode));
		break;
	    case 0x0000002c:
		printf("fsqrts%s\tf%lu,f%lu\n", Rc(opcode), FRT(opcode),
		       FRB(opcode));
		break;
	    case 0x00000032:
		printf("fmuls%s\tf%lu,f%lu,f%lu\n", Rc(opcode), FRT(opcode),
		       FRA(opcode), FRC(opcode));
		break;
	    case 0x00000024:
		a_form("fdivs", opcode, 3);
		break;
	    case 0x0000003a:
		a_form("fmadds", opcode, 4);
		break;
	    case 0x00000038:
		a_form("fmsubs", opcode, 4);
		break;
	    case 0x0000003e:
		a_form("fnmadds", opcode, 4);
		break;
	    case 0x0000003c:
		a_form("fnmsubs", opcode, 4);
		break;
	    default:
		printf(".long 0x%08x\n", (unsigned int)opcode);
		break;
	    }
	    break;
	case 0x10000000:
	    if((opcode & (1 << 5)) == 0){
		if((opcode & 0x31) != 0){
		    printf(".long 0x%08x\n", (unsigned int)opcode);
		    break;
		}
		switch((opcode >> 1) & 0x7){
		case 0:
		    switch(opcode & 0x7c0){
		    case 0x00000000:
			printf("vaddubm\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000040:
			printf("vadduhm\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000080:
			printf("vadduwm\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000180:
			printf("vaddcuw\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000200:
			printf("vaddubs\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000240:
			printf("vadduhs\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000280:
			printf("vadduws\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000300:
			printf("vaddsbs\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000340:
			printf("vaddshs\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000380:
			printf("vaddsws\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000400:
			printf("vsububm\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000440:
			printf("vsubuhm\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000480:
			printf("vsubuwm\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000580:
			printf("vsubcuw\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000600:
			printf("vsububs\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000640:
			printf("vsubuhs\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000680:
			printf("vsubuws\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000700:
			printf("vsubsbs\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000740:
			printf("vsubshs\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000780:
			printf("vsubsws\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    default:
			printf(".long 0x%08x\n", (unsigned int)opcode);
			break;
		    }
		    break;
		case 1:
		    switch(opcode & 0x7c0){
		    case 0x00000000:
			printf("vmaxub\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000040:
			printf("vmaxuh\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000080:
			printf("vmaxuw\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000100:
			printf("vmaxsb\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000140:
			printf("vmaxsh\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000180:
			printf("vmaxsw\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000200:
			printf("vminub\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000240:
			printf("vminuh\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000280:
			printf("vminuw\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000300:
			printf("vminsb\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000340:
			printf("vminsh\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000380:
			printf("vminsw\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000400:
			printf("vavgub\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000440:
			printf("vavguh\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000480:
			printf("vavguw\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000500:
			printf("vavgsb\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000540:
			printf("vavgsh\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000580:
			printf("vavgsw\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    default:
			printf(".long 0x%08x\n", (unsigned int)opcode);
			break;
		    }
		    break;
		case 2:
		    switch(opcode & 0x7c0){
		    case 0x00000000:
			printf("vrlb\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000040:
			printf("vrlh\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000080:
			printf("vrlw\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000100:
			printf("vslb\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000140:
			printf("vslh\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000180:
			printf("vslw\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x000001c0:
			printf("vsl\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000200:
			printf("vsrb\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000240:
			printf("vsrh\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000280:
			printf("vsrw\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x000002c0:
			printf("vsr\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000300:
			printf("vsrab\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000340:
			printf("vsrah\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000380:
			printf("vsraw\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000400:
			printf("vand\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000440:
			printf("vandc\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000480:
			printf("vor\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x000004c0:
			printf("vxor\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000500:
			printf("vnor\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000600:
			printf("mfvscr\tv%lu\n", VT(opcode));
			break;
		    case 0x00000640:
			printf("mtvscr\tv%lu\n", VB(opcode));
			break;
		    default:
			printf(".long 0x%08x\n", (unsigned int)opcode);
			break;
		    }
		    break;
		case 3:
		    switch(opcode & 0x3c0){
		    case 0x00000000:
			printf("vcmpequb%s\tv%lu,v%lu,v%lu\n", VX_Rc(opcode),
			       VT(opcode), VA(opcode), VB(opcode));
			break;
		    case 0x00000040:
			printf("vcmpequh%s\tv%lu,v%lu,v%lu\n", VX_Rc(opcode),
			       VT(opcode), VA(opcode), VB(opcode));
			break;
		    case 0x00000080:
			printf("vcmpequw%s\tv%lu,v%lu,v%lu\n", VX_Rc(opcode),
			       VT(opcode), VA(opcode), VB(opcode));
			break;
		    case 0x000000c0:
			printf("vcmpeqfp%s\tv%lu,v%lu,v%lu\n", VX_Rc(opcode),
			       VT(opcode), VA(opcode), VB(opcode));
			break;
		    case 0x000001c0:
			printf("vcmpgefp%s\tv%lu,v%lu,v%lu\n", VX_Rc(opcode),
			       VT(opcode), VA(opcode), VB(opcode));
			break;
		    case 0x00000200:
			printf("vcmpgtub%s\tv%lu,v%lu,v%lu\n", VX_Rc(opcode),
			       VT(opcode), VA(opcode), VB(opcode));
			break;
		    case 0x00000240:
			printf("vcmpgtuh%s\tv%lu,v%lu,v%lu\n", VX_Rc(opcode),
			       VT(opcode), VA(opcode), VB(opcode));
			break;
		    case 0x00000280:
			printf("vcmpgtuw%s\tv%lu,v%lu,v%lu\n", VX_Rc(opcode),
			       VT(opcode), VA(opcode), VB(opcode));
			break;
		    case 0x000002c0:
			printf("vcmpgtfp%s\tv%lu,v%lu,v%lu\n", VX_Rc(opcode),
			       VT(opcode), VA(opcode), VB(opcode));
			break;
		    case 0x00000300:
			printf("vcmpgtsb%s\tv%lu,v%lu,v%lu\n", VX_Rc(opcode),
			       VT(opcode), VA(opcode), VB(opcode));
			break;
		    case 0x00000340:
			printf("vcmpgtsh%s\tv%lu,v%lu,v%lu\n", VX_Rc(opcode),
			       VT(opcode), VA(opcode), VB(opcode));
			break;
		    case 0x00000380:
			printf("vcmpgtsw%s\tv%lu,v%lu,v%lu\n", VX_Rc(opcode),
			       VT(opcode), VA(opcode), VB(opcode));
			break;
		    case 0x000003c0:
			printf("vcmpbfp%s\tv%lu,v%lu,v%lu\n", VX_Rc(opcode),
			       VT(opcode), VA(opcode), VB(opcode));
			break;
		    default:
			printf(".long 0x%08x\n", (unsigned int)opcode);
			break;
		    }
		    break;
		case 4:
		    switch(opcode & 0x7c0){
		    case 0x00000000:
			printf("vmuloub\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000040:
			printf("vmulouh\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000100:
			printf("vmulosb\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000140:
			printf("vmulosh\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000200:
			printf("vmuleub\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000240:
			printf("vmuleuh\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000300:
			printf("vmulesb\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000340:
			printf("vmulesh\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000600:
			printf("vsum4ubs\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000700:
			printf("vsum4sbs\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000640:
			printf("vsum4shs\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000680:
			printf("vsum2sws\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000780:
			printf("vsumsws\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    default:
			printf(".long 0x%08x\n", (unsigned int)opcode);
			break;
		    }
		    break;
		case 5:
		    switch((opcode >> 6) & 0x1f){
		    case 0:
			printf("vaddfp\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 1:
			printf("vsubfp\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 4:
			printf("vrefp\tv%lu,v%lu\n", VT(opcode), VB(opcode));
			break;
		    case 5:
			printf("vrsqrtefp\tv%lu,v%lu\n",VT(opcode), VB(opcode));
			break;
		    case 6:
			printf("vexptefp\tv%lu,v%lu\n", VT(opcode), VB(opcode));
			break;
		    case 7:
			printf("vlogefp\tv%lu,v%lu\n", VT(opcode), VB(opcode));
			break;
		    case 8:
			printf("vrfin\tv%lu,v%lu\n", VT(opcode), VB(opcode));
			break;
		    case 9:
			printf("vrfiz\tv%lu,v%lu\n", VT(opcode), VB(opcode));
			break;
		    case 10:
			printf("vrfip\tv%lu,v%lu\n", VT(opcode), VB(opcode));
			break;
		    case 11:
			printf("vrfim\tv%lu,v%lu\n", VT(opcode), VB(opcode));
			break;
		    case 12:
			printf("vcfux\tv%lu,v%lu,%lu\n", VT(opcode),
			       VB(opcode), (opcode >> 16) & 0x1f);
			break;
		    case 13:
			printf("vcfsx\tv%lu,v%lu,%lu\n", VT(opcode),
			       VB(opcode), (opcode >> 16) & 0x1f);
			break;
		    case 14:
			printf("vctuxs\tv%lu,v%lu,%lu\n", VT(opcode),
			       VB(opcode), (opcode >> 16) & 0x1f);
			break;
		    case 15:
			printf("vctsxs\tv%lu,v%lu,%lu\n", VT(opcode),
			       VB(opcode), (opcode >> 16) & 0x1f);
			break;
		    case 16:
			printf("vmaxfp\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 17:
			printf("vminfp\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    default:
			printf(".long 0x%08x\n", (unsigned int)opcode);
			break;
		    }
		    break;
		case 6:
		    switch(opcode & 0x7c0){
		    case 0x00000000:
			printf("vmrghb\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000040:
			printf("vmrghh\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000080:
			printf("vmrghw\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000100:
			printf("vmrglb\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000140:
			printf("vmrglh\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000180:
			printf("vmrglw\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000200:
			printf("vspltb\tv%lu,v%lu,%lu\n", VT(opcode),
			       VB(opcode), (opcode >> 16) & 0x1f);
			break;
		    case 0x00000240:
			printf("vsplth\tv%lu,v%lu,%lu\n", VT(opcode),
			       VB(opcode), (opcode >> 16) & 0x1f);
			break;
		    case 0x00000280:
			printf("vspltw\tv%lu,v%lu,%lu\n", VT(opcode),
			       VB(opcode), (opcode >> 16) & 0x1f);
			break;
		    case 0x00000300:
			if((((opcode >> 16) & 0x1f) & 0x10) == 0x10)
			    simm = 0xfffffff0 | ((opcode >> 16) & 0x1f);
			else
			    simm = (opcode >> 16) & 0x1f;
			printf("vspltisb\tv%lu,%ld\n", VT(opcode), simm);
			break;
		    case 0x00000340:
			if((((opcode >> 16) & 0x1f) & 0x10) == 0x10)
			    simm = 0xfffffff0 | ((opcode >> 16) & 0x1f);
			else
			    simm = (opcode >> 16) & 0x1f;
			printf("vspltish\tv%lu,%ld\n", VT(opcode), simm);
			break;
		    case 0x00000380:
			if((((opcode >> 16) & 0x1f) & 0x10) == 0x10)
			    simm = 0xfffffff0 | ((opcode >> 16) & 0x1f);
			else
			    simm = (opcode >> 16) & 0x1f;
			printf("vspltisw\tv%lu,%ld\n", VT(opcode), simm);
			break;
		    case 0x00000400:
			printf("vslo\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 0x00000440:
			printf("vsro\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    default:
			printf(".long 0x%08x\n", (unsigned int)opcode);
			break;
		    }
		    break;
		case 7:
		    switch((opcode >> 6) & 0x1f){
		    case 0:
			printf("vpkuhum\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 1:
			printf("vpkuwum\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 2:
			printf("vpkuhus\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 3:
			printf("vpkuwus\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 4:
			printf("vpkshus\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 5:
			printf("vpkswus\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 6:
			printf("vpkshss\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 7:
			printf("vpkswss\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 8:
			printf("vupkhsb\tv%lu,v%lu\n", VT(opcode),
			       VB(opcode));
			break;
		    case 9:
			printf("vupkhsh\tv%lu,v%lu\n", VT(opcode),
			       VB(opcode));
			break;
		    case 10:
			printf("vupklsb\tv%lu,v%lu\n", VT(opcode),
			       VB(opcode));
			break;
		    case 11:
			printf("vupklsh\tv%lu,v%lu\n", VT(opcode),
			       VB(opcode));
			break;
		    case 12:
			printf("vpkpx\tv%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode));
			break;
		    case 13:
			printf("vupkhpx\tv%lu,v%lu\n", VT(opcode),
			       VB(opcode));
			break;
		    case 15:
			printf("vupklpx\tv%lu,v%lu\n", VT(opcode),
			       VB(opcode));
			break;
		    default:
			printf(".long 0x%08x\n", (unsigned int)opcode);
			break;
		    }
		    break;
		default:
		    printf(".long 0x%08x\n", (unsigned int)opcode);
		    break;
		}
	    }
	    else{
		switch(opcode & 0xf){
		case 0:
		    printf("vmhaddshs\tv%lu,v%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode), VC(opcode));
		    break;
		case 1:
		    printf("vmhraddshs\tv%lu,v%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode), VC(opcode));
		    break;
		case 2:
		    printf("vmladduhm\tv%lu,v%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode), VC(opcode));
		    break;
		case 4:
		    printf("vmsumubm\tv%lu,v%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode), VC(opcode));
		    break;
		case 5:
		    printf("vmsummbm\tv%lu,v%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode), VC(opcode));
		    break;
		case 6:
		    printf("vmsumuhm\tv%lu,v%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode), VC(opcode));
		    break;
		case 7:
		    printf("vmsumuhs\tv%lu,v%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode), VC(opcode));
		    break;
		case 8:
		    printf("vmsumshm\tv%lu,v%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode), VC(opcode));
		    break;
		case 9:
		    printf("vmsumshs\tv%lu,v%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode), VC(opcode));
		    break;
		case 10:
		    printf("vsel\tv%lu,v%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode), VC(opcode));
		    break;
		case 11:
		    printf("vperm\tv%lu,v%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VB(opcode), VC(opcode));
		    break;
		case 12:
		    printf("vsldoi\tv%lu,v%lu,v%lu,%lu\n", VT(opcode),
			       VA(opcode), VB(opcode), (opcode >> 6) & 0xf);
		    break;
		case 14:
		    printf("vmaddfp\tv%lu,v%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VC(opcode), VB(opcode));
		    break;
		case 15:
		    printf("vnmsubfp\tv%lu,v%lu,v%lu,v%lu\n", VT(opcode),
			       VA(opcode), VC(opcode), VB(opcode));
		    break;
		default:
		    printf(".long 0x%08x\n", (unsigned int)opcode);
		    break;
		}
	    }
	    break;
	default:
	    printf(".long 0x%08x\n", (unsigned int)opcode);
	    break;
	}
	return(4);
}

static
void
xo_form(
const char *name,
unsigned long opcode,
unsigned long nregs)
{
	if(nregs == 3)
	    printf("%s%s\tr%lu,r%lu,r%lu\n", name, OE_Rc(opcode), RT(opcode),
		   RA(opcode), RB(opcode));
	else /* nregs == 2 */
	    printf("%s%s\tr%lu,r%lu\n", name, OE_Rc(opcode), RT(opcode),
		   RA(opcode));
}

static
void
x_form(
const char *name,
unsigned long opcode,
unsigned long nregs)
{
	if(nregs == 3)
	    printf("%s%s\tr%lu,r%lu,r%lu\n", name, Rc(opcode), RT(opcode),
		   RA(opcode), RB(opcode));
	else /* nregs == 2 */
	    printf("%s%s\tr%lu,r%lu\n", name, Rc(opcode), RT(opcode),
		   RA(opcode));
}

static
void
sx_form(
const char *name,
unsigned long opcode,
unsigned long nregs)
{
	if(nregs == 3)
	    printf("%s%s\tr%lu,r%lu,r%lu\n", name, Rc(opcode), RA(opcode),
		   RS(opcode), RB(opcode));
	else /* nregs == 2 */
	    printf("%s%s\tr%lu,r%lu\n", name, Rc(opcode), RA(opcode),
		   RS(opcode));
}

static
void
fx_form(
const char *name,
unsigned long opcode,
unsigned long nregs)
{
	if(nregs == 3)
	    printf("%s\tf%lu,r%lu,r%lu\n", name, FRT(opcode),
		   RA(opcode), RB(opcode));
	else /* nregs == 2 */
	    printf("%s%s\tf%lu,f%lu\n", name, Rc(opcode), FRT(opcode),
		   FRB(opcode));
}

static
void
xl_form(
const char *name,
unsigned long opcode,
unsigned long nregs)
{
	if(nregs == 3)
	    printf("%s%s\t%lu,%lu,%lu\n", name, LK(opcode), RT(opcode),
		   RA(opcode), RB(opcode));
	else /* nregs == 2 */
	    printf("%s%s\t%lu,%lu\n", name, LK(opcode), RT(opcode),
		   RA(opcode));
}

static
void
a_form(
const char *name,
unsigned long opcode,
unsigned long nregs)
{
	if(nregs == 3)
	    printf("%s%s\tf%lu,f%lu,f%lu\n", name, Rc(opcode), FRT(opcode),
		   FRA(opcode), FRB(opcode));
	else /* nregs == 4 */
	    printf("%s%s\tf%lu,f%lu,f%lu,f%lu\n", name, Rc(opcode), FRT(opcode),
		   FRA(opcode), FRC(opcode), FRB(opcode));
}

static
unsigned long
bc(
const char *name,
unsigned long opcode,
unsigned long sect_offset,
struct relocation_info *relocs,
unsigned long nrelocs)
{
    char *prediction;
    const char *a;
    unsigned long operands;
    enum bool branch_to_register, predicted;

	operands = 0;
	prediction = "";
	/*
	 * For branch conditional instructions that use the Y-bit that were
	 * predicted the r_length is set to 3 instead of 2.  So to correctly
	 * print the prediction, we have to search for a reloc of and look at
	 * the r_length.  If there is a reloc at this pc, and if the r_length
	 * is 3 it then we know it was a predicted branch and we will always
	 * print the prediction based on the Y-bit, the sign of the displacement
	 * or the opcode (in the case of bclrX and bcctrX instructions).
	 */
	if(get_reloc_r_length(sect_offset, relocs, nrelocs) == 3)
	    predicted = TRUE;
	else
	    predicted = FALSE;
	/* branch conditional (to displacment) */
	if((opcode & 0xfc000000) == 0x40000000){
	    branch_to_register = FALSE;
	    a = aa[(opcode >> 1) & 0x1];
	    if(Y_BIT(opcode) == 0){
		/*
		 * the Y-bit is zero so don't print prediction unless there was
		 * a reloc that said this was a predicted branch.
		 */
		if(predicted == TRUE){
		    if((opcode & 0x00008000) != 0)
			prediction = "+";
		    else
			prediction = "-";
		}
		else{
		    prediction = "";
		}
	    }
	    else{
		if((opcode & 0x00008000) != 0)
		    prediction = "-";
		else
		    prediction = "+";
	    }
	}
	else{
	    /* branch conditional (to link or count register) */
	    branch_to_register = TRUE;
	    a = "";
	    if(Y_BIT(opcode) == 0){
		/* the Y-bit is zero so don't print prediction */
		prediction = "";
		/*
		 * the Y-bit is zero so don't print prediction unless there was
		 * a reloc that said this was a predicted branch.
		 */
		if(predicted == TRUE){
		    prediction = "-";
		}
		else{
		    prediction = "";
		}
	    }
	    else{
		prediction = "+";
	    }
	}
	if(Zflag == TRUE){
	    if(branch_to_register == TRUE){
		printf("bc%s%s%s\t%lu,%lu,%lu", name, LK(opcode), a, RT(opcode),
		       RA(opcode), BH(opcode));
		operands = 3;
	    }
	    else{
		printf("bc%s%s%s\t%lu,%lu", name, LK(opcode), a, RT(opcode),
		       RA(opcode));
		operands = 2;
	    }
	    return(operands);
	}
	
	switch(opcode & 0x03e00000){
	case 0x01c00000:
	    prediction = "--";
	    goto bt;
	case 0x01e00000:
	    prediction = "++";
	    goto bt;
	case 0x01800000:
	case 0x01a00000:
bt:	    /* branch if condition true */
	    printf("b%s%s%s%s%s", BC_TRUE(opcode), name, LK(opcode), a,
		   prediction);
	    if(CR_FIELD(opcode) != 0 ||
	      (branch_to_register == TRUE && BH(opcode) != 0)){
		printf("\tcr%lu", CR_FIELD(opcode));
		operands = 1;
		if(branch_to_register == TRUE && BH(opcode) != 0){
		    printf(",%lu", BH(opcode));
		    operands = 2;
		}
	    }
	    break;
	case 0x00c00000:
	    prediction = "--";
	    goto bf;
	case 0x00e00000:
	    prediction = "++";
	    goto bf;
	case 0x00800000:
	case 0x00a00000:
bf:	    /* branch if condition false */
	    printf("b%s%s%s%s%s", BC_FALSE(opcode), name, LK(opcode), a,
		   prediction);
	    if(CR_FIELD(opcode) != 0 ||
	      (branch_to_register == TRUE && BH(opcode) != 0)){
		printf("\tcr%lu", CR_FIELD(opcode));
		operands = 1;
		if(branch_to_register == TRUE && BH(opcode) != 0){
		    printf(",%lu", BH(opcode));
		    operands = 2;
		}
	    }
	    break;
	case 0x03000000:
	    prediction = "--";
	    goto bdnz;
	case 0x03200000:
	    prediction = "++";
	    goto bdnz;
	case 0x02000000:
	case 0x02200000:
bdnz:	    /* decrement ctr branch if ctr non-zero */
	    if((opcode & 0xfc0007fe) == 0x4c000420 ||
	       (opcode & 0x001f0000) != 0x00000000)
		goto bc_general_default_form;
	    printf("bdnz%s%s%s%s", name, LK(opcode), a, prediction);
	    if(branch_to_register == TRUE && BH(opcode) != 0){
		printf("\t%lu", BH(opcode));
		operands = 1;
	    }
	    break;
	case 0x01000000:
	case 0x01200000:
	    /* decrement ctr branch if ctr non-zero and condition true */
	    if((opcode & 0xfc0007fe) == 0x4c000420)
		goto bc_general_default_form;
	    printf("bdnzt%s%s%s%s\t", name, LK(opcode), a, prediction);
	    if(CR_FIELD(opcode) != 0)
		printf("cr%lu+", CR_FIELD(opcode));
	    printf("%s", BC_TRUE(opcode));
	    operands = 1;
	    if(branch_to_register == TRUE && BH(opcode) != 0){
		printf(",%lu", BH(opcode));
		operands = 2;
	    }
	    break;
	case 0x00000000:
	case 0x00200000:
	    /* decrement ctr branch if ctr non-zero and condition false */
	    if((opcode & 0xfc0007fe) == 0x4c000420)
		goto bc_general_default_form;
	    printf("bdnzf%s%s%s%s\t", name, LK(opcode), a, prediction);
	    if(CR_FIELD(opcode) != 0)
		printf("cr%lu+", CR_FIELD(opcode));
	    printf("%s", BC_TRUE(opcode));
	    operands = 1;
	    if(branch_to_register == TRUE && BH(opcode) != 0){
		printf(",%lu", BH(opcode));
		operands = 2;
	    }
	    break;
	case 0x03400000:
	    prediction = "--";
	    goto bdz;
	case 0x03600000:
	    prediction = "++";
	    goto bdz;
	case 0x02400000:
	case 0x02600000:
bdz:	    /* decrement ctr branch if ctr zero */
	    if((opcode & 0xfc0007fe) == 0x4c000420 ||
	       (opcode & 0x001f0000) != 0x00000000)
		goto bc_general_default_form;
	    printf("bdz%s%s%s%s", name, LK(opcode), a, prediction);
	    if(branch_to_register == TRUE && BH(opcode) != 0){
		printf("\t%lu", BH(opcode));
		operands = 1;
	    }
	    break;
	case 0x01400000:
	case 0x01600000:
	    /* decrement ctr branch if ctr zero and condition true */
	    if((opcode & 0xfc0007fe) == 0x4c000420)
		goto bc_general_default_form;
	    printf("bdzt%s%s%s%s\t", name, LK(opcode), a, prediction);
	    if(CR_FIELD(opcode) != 0)
		printf("cr%lu+", CR_FIELD(opcode));
	    printf("%s", BC_TRUE(opcode));
	    operands = 1;
	    if(branch_to_register == TRUE && BH(opcode) != 0){
		printf(",%lu", BH(opcode));
		operands = 2;
	    }
	    break;
	case 0x00400000:
	case 0x00600000:
	    /* decrement ctr branch if ctr zero and condition false */
	    if((opcode & 0xfc0007fe) == 0x4c000420)
		goto bc_general_default_form;
	    printf("bdzf%s%s%s%s\t", name, LK(opcode), a, prediction);
	    if(CR_FIELD(opcode) != 0)
		printf("cr%lu+", CR_FIELD(opcode));
	    printf("%s", BC_TRUE(opcode));
	    operands = 1;
	    if(branch_to_register == TRUE && BH(opcode) != 0){
		printf(",%lu", BH(opcode));
		operands = 2;
	    }
	    break;
	case 0x02800000:
	    /* branch unconditionally */
	    if(Y_BIT(opcode) != 0 || RA(opcode) != 0)
		goto bc_general_default_form;
	    if(BH(opcode) != 0)
		printf("b%s%s%s\t%lu", name, LK(opcode), a, BH(opcode));
	    else
		printf("b%s%s%s", name, LK(opcode), a);
	    break;
	default:
bc_general_default_form:
	    if(RT(opcode) == 20){ /* branch always */
		if(branch_to_register == TRUE && BH(opcode) != 0){
		    printf("bc%s%s%s\t%lu,%lu,%lu", name, LK(opcode), a,
			   RT(opcode), RA(opcode), BH(opcode));
		    operands = 3;
		}
		else{
		    printf("bc%s%s%s\t%lu,%lu", name, LK(opcode), a,
			   RT(opcode), RA(opcode));
		    operands = 2;
		}
	    }
	    else{
		if(branch_to_register == TRUE && BH(opcode) != 0){
		    printf("bc%s%s%s%s\t%lu,%lu,%lu", name, LK(opcode), a,
			   prediction, RT(opcode), RA(opcode), BH(opcode));
		    operands = 3;
		}
		else{
		    printf("bc%s%s%s%s\t%lu,%lu", name, LK(opcode), a,
			   prediction, RT(opcode), RA(opcode));
		    operands = 2;
		}
	    }
	    break;
	}
	return(operands);
}

static
void
trap(
const char *name,
unsigned long opcode)
{
    char *i;

	if(((opcode & 0xfc000000) == 0x08000000) ||
	   ((opcode & 0xfc000000) == 0x0c000000))
	    i = "i";
	else
	    i = "";

	switch(opcode & 0x03e00000){
	case 0x02000000:
	    printf("t%slt%s\tr%lu,", name, i, RA(opcode));
	    break;
	case 0x02800000:
	    printf("t%sle%s\tr%lu,", name, i, RA(opcode));
	    break;
	case 0x00800000:
	    printf("t%seq%s\tr%lu,", name, i, RA(opcode));
	    break;
	case 0x01800000:
	    printf("t%sge%s\tr%lu,", name, i, RA(opcode));
	    break;
	case 0x01000000:
	    printf("t%sgt%s\tr%lu,", name, i, RA(opcode));
	    break;
	case 0x03000000:
	    printf("t%sne%s\tr%lu,", name, i, RA(opcode));
	    break;
	case 0x00400000:
	    printf("t%sllt%s\tr%lu,", name, i, RA(opcode));
	    break;
	case 0x00c00000:
	    printf("t%slle%s\tr%lu,", name, i, RA(opcode));
	    break;
	case 0x00a00000:
	    printf("t%slge%s\tr%lu,", name, i, RA(opcode));
	    break;
	case 0x00200000:
	    printf("t%slgt%s\tr%lu,", name, i, RA(opcode));
	    break;
	default:
	    printf("t%s%s\t%lu,r%lu,", name, i, TO(opcode), RA(opcode));
	    break;
	}
}

static
void
print_special_register_name(
unsigned opcode)
{
    unsigned long reg;

	reg = ((((opcode >> 11) & 0x1f) << 5) | ((opcode >> 16) & 0x1f));
	switch(reg){
	case 0:
	    printf("mq");
	    break;
	case 1:
	    printf("xer");
	    break;
	case 4:
	    printf("rtcu");
	    break;
	case 5:
	    printf("rtcl");
	    break;
	case 8:
	    printf("lr");
	    break;
	case 9:
	    printf("ctr");
	    break;
	case 18:
	    printf("dsisr");
	    break;
	case 19:
	    printf("dar");
	    break;
	case 22:
	    printf("dec");
	    break;
	case 25:
	    printf("sdr1");
	    break;
	case 26:
	    printf("srr0");
	    break;
	case 27:
	    printf("srr1");
	    break;
	case 256:
	    printf("VRsave");
	    break;
	case 272:
	    printf("sprg0");
	    break;
	case 273:
	    printf("sprg1");
	    break;
	case 274:
	    printf("sprg2");
	    break;
	case 275:
	    printf("sprg3");
	    break;
	case 280:
	    printf("asr");
	    break;
	case 281:
	    printf("rtcd");
	    break;
	case 282:
	    printf("rtci");
	    break;
	case 284:
	    printf("tbl");
	    break;
	case 285:
	    printf("tbu");
	    break;
	case 287:
	    printf("pvr");
	    break;
	case 528:
	    printf("ibat0u");
	    break;
	case 529:
	    printf("ibat0l");
	    break;
	case 530:
	    printf("ibat1u");
	    break;
	case 531:
	    printf("ibat1l");
	    break;
	case 532:
	    printf("ibat2u");
	    break;
	case 533:
	    printf("ibat2l");
	    break;
	case 534:
	    printf("ibat3u");
	    break;
	case 535:
	    printf("ibat3l");
	    break;
	case 536:
	    printf("dbat0u");
	    break;
	case 537:
	    printf("dbat0l");
	    break;
	case 538:
	    printf("dbat1u");
	    break;
	case 539:
	    printf("dbat1l");
	    break;
	case 540:
	    printf("dbat2u");
	    break;
	case 541:
	    printf("dbat2l");
	    break;
	case 542:
	    printf("dbat3u");
	    break;
	case 543:
	    printf("dbat3l");
	    break;
	case 936:
	    printf("ummcr0");
	    break;
	case 937:
	    printf("upmc1");
	    break;
	case 938:
	    printf("upmc2");
	    break;
	case 939:
	    printf("usia");
	    break;
	case 940:
	    printf("ummcr1");
	    break;
	case 941:
	    printf("upmc3");
	    break;
	case 942:
	    printf("upmc4");
	    break;
	case 952:
	    printf("mmcr0");
	    break;
	case 953:
	    printf("pmc1");
	    break;
	case 954:
	    printf("pmc2");
	    break;
	case 955:
	    printf("sia");
	    break;
	case 956:
	    printf("mmcr1");
	    break;
	case 957:
	    printf("pmc3");
	    break;
	case 958:
	    printf("pmc4");
	    break;
	case 959:
	    printf("sda");
	    break;
	case 976:
	    printf("dmiss");
	    break;
	case 977:
	    printf("dcmp");
	    break;
	case 978:
	    printf("hash1");
	    break;
	case 979:
	    printf("hash2");
	    break;
	case 980:
	    printf("imiss");
	    break;
	case 981:
	    printf("icmp");
	    break;
	case 982:
	    printf("rpa");
	    break;
	case 1008:
	    printf("hid0");
	    break;
	case 1009:
	    printf("hid1");
	    break;
	case 1010:
	    printf("hid2");
	    break;
	case 1013:
	    printf("dabr");
	    break;
	case 1017:
	    printf("l2cr");
	    break;
	case 1019:
	    printf("ictc");
	    break;
	case 1020:
	    printf("thrm1");
	    break;
	case 1021:
	    printf("thrm2");
	    break;
	case 1022:
	    printf("thrm3");
	    break;
	case 1023:
	    printf("pir");
	    break;
	default:
	    printf("%lu", reg);
	}
}

static
void
print_immediate(
unsigned long value, 
unsigned long sect_offset,
struct relocation_info *relocs,
unsigned long nrelocs,
struct nlist *symbols,
struct nlist_64 *symbols64,
unsigned long nsymbols,
struct symbol *sorted_symbols,
unsigned long nsorted_symbols,
char *strings,
unsigned long strings_size,
enum bool verbose)
{
    long low, high, mid, reloc_found, offset;
    unsigned long i, r_address, r_symbolnum, r_type, r_extern,
		  r_value, r_scattered, pair_r_type, pair_r_value;
    unsigned long other_half;
    const char *name, *add, *sub;
    struct relocation_info *rp, *pairp;
    struct scattered_relocation_info *srp, *spairp;
    uint32_t n_strx;

	r_symbolnum = 0;
	r_type = 0;
	r_extern = 0;
	r_value = 0;
	r_scattered = 0;
	other_half = 0;
	pair_r_value = 0;

	if(verbose == FALSE){
	    printf("0x%x", (unsigned int)value);
	    return;
	}
	reloc_found = 0;
	if(nrelocs != 0){
	    for(i = 0; i < nrelocs; i++){
		rp = &relocs[i];
		if(rp->r_address & R_SCATTERED){
		    srp = (struct scattered_relocation_info *)rp;
		    r_scattered = 1;
		    r_address = srp->r_address;
		    r_extern = 0;
		    r_type = srp->r_type;
		    r_value = srp->r_value;
		}
		else{
		    r_scattered = 0;
		    r_address = rp->r_address;
		    r_symbolnum = rp->r_symbolnum;
		    r_extern = rp->r_extern;
		    r_type = rp->r_type;
		}
		if(r_type == PPC_RELOC_PAIR){
		    fprintf(stderr, "Stray PPC_RELOC_PAIR relocation entry "
			    "%lu\n", i);
		    continue;
		}
		if(r_address == sect_offset){
		    if(r_type == PPC_RELOC_HI16 ||
		       r_type == PPC_RELOC_LO16 ||
		       r_type == PPC_RELOC_HA16 ||
		       r_type == PPC_RELOC_SECTDIFF ||
		       r_type == PPC_RELOC_LOCAL_SECTDIFF ||
		       r_type == PPC_RELOC_HI16_SECTDIFF ||
		       r_type == PPC_RELOC_LO16_SECTDIFF ||
		       r_type == PPC_RELOC_LO14_SECTDIFF ||
		       r_type == PPC_RELOC_HA16_SECTDIFF ||
		       r_type == PPC_RELOC_LO14 ||
		       r_type == PPC_RELOC_JBSR){
			if(i+1 < nrelocs){
			    pairp = &rp[1];
			    if(pairp->r_address & R_SCATTERED){
				spairp = (struct scattered_relocation_info *)
					 pairp;
			        if(r_type == PPC_RELOC_JBSR)
				    other_half = spairp->r_address;
				else
				    other_half = spairp->r_address & 0xffff;
				pair_r_type = spairp->r_type;
				pair_r_value = spairp->r_value;
			    }
			    else{
			        if(r_type == PPC_RELOC_JBSR)
				    other_half = pairp->r_address;
				else
				    other_half = pairp->r_address & 0xffff;
				pair_r_type = pairp->r_type;
			    }
			    if(pair_r_type != PPC_RELOC_PAIR){
				fprintf(stderr, "No PPC_RELOC_PAIR relocation "
					"entry after entry %lu\n", i);
				continue;
			    }
			}
		    }
		    reloc_found = 1;
		    break;
		}
		if(r_type == PPC_RELOC_HI16 ||
		   r_type == PPC_RELOC_LO16 ||
		   r_type == PPC_RELOC_HA16 ||
		   r_type == PPC_RELOC_SECTDIFF ||
		   r_type == PPC_RELOC_LOCAL_SECTDIFF ||
		   r_type == PPC_RELOC_HI16_SECTDIFF ||
		   r_type == PPC_RELOC_LO16_SECTDIFF ||
		   r_type == PPC_RELOC_LO14_SECTDIFF ||
		   r_type == PPC_RELOC_HA16_SECTDIFF ||
		   r_type == PPC_RELOC_LO14 ||
		   r_type == PPC_RELOC_JBSR){
		    if(i+1 < nrelocs){
			pairp = &rp[1];
			if(pairp->r_address & R_SCATTERED){
			    spairp = (struct scattered_relocation_info *)pairp;
			    pair_r_type = spairp->r_type;
			}
			else{
			    pair_r_type = pairp->r_type;
			}
			if(pair_r_type == PPC_RELOC_PAIR)
			    i++;
			else
			    fprintf(stderr, "No PPC_RELOC_PAIR relocation "
				    "entry after entry %lu\n", i);
		    }
		}
	    }
	}

	if(reloc_found && r_extern == 1){
	    if(symbols != NULL)
		n_strx = symbols[r_symbolnum].n_un.n_strx;
	    else
		n_strx = symbols64[r_symbolnum].n_un.n_strx;
	    if(n_strx >= strings_size)
		name = "bad string offset";
	    else
		name = strings + n_strx;
	    if(value != 0){
		switch(r_type){
		case PPC_RELOC_HI16:
		    value = value << 16 | other_half;
		    printf("hi16(%s+0x%x)", name, (unsigned int)value);
		    break;
		case PPC_RELOC_HA16:
		    value = value << 16 | other_half;
		    printf("ha16(%s+0x%x)", name, (unsigned int)value);
		    break;
		case PPC_RELOC_LO16:
		case PPC_RELOC_LO14:
		    value = other_half << 16 | value;
		    printf("lo16(%s+0x%x)", name, (unsigned int)value);
		    break;
		case PPC_RELOC_JBSR:
		    printf("%s",name);
		    if(other_half != 0)
			printf("+0x%x",(unsigned int)other_half);
		    break;
		default:
		    printf("%s+0x%x", name, (unsigned int)value);
		    break;
		}
	    }
	    else{
		switch(r_type){
		case PPC_RELOC_HI16:
		    value = value << 16 | other_half;
		    if(value == 0)
			printf("hi16(%s)", name);
		    else
			printf("hi16(%s+0x%x)", name, (unsigned int)value);
		    break;
		case PPC_RELOC_HA16:
		    value = value << 16 | other_half;
		    if(value == 0)
			printf("ha16(%s)", name);
		    else
			printf("ha16(%s+0x%x)", name, (unsigned int)value);
		    break;
		case PPC_RELOC_LO16:
		case PPC_RELOC_LO14:
		    value = other_half << 16 | value;
		    if(value == 0)
			printf("lo16(%s)", name);
		    else
			printf("lo16(%s+0x%x)", name, (unsigned int)value);
		    break;
		case PPC_RELOC_JBSR:
		    if(other_half != 0)
			printf("%s+0x%x", name, (unsigned int)other_half);
		    else
			printf("%s", name);
		    break;
		default:
		    if(value == 0)
			printf("%s", name);
		    else
			printf("%s+0x%x", name, (unsigned int)value);
		}
	    }
	    return;
	}

	offset = 0;
	if(reloc_found){
	    if(r_type == PPC_RELOC_HI16 ||
	       r_type == PPC_RELOC_HI16_SECTDIFF)
		value = value << 16 | other_half;
	    else if(r_type == PPC_RELOC_HA16 ||
		    r_type == PPC_RELOC_HA16_SECTDIFF){
		if((other_half & 0x00008000) != 0)
		    value = (value << 16) + (0xffff0000 | other_half);
		else
		    value = (value << 16) + other_half;
	    }
	    else if(r_type == PPC_RELOC_LO16 ||
		    r_type == PPC_RELOC_LO16_SECTDIFF ||
		    r_type == PPC_RELOC_LO14_SECTDIFF ||
		    r_type == PPC_RELOC_LO14)
		value = other_half << 16 | value;
	    else if(r_type == PPC_RELOC_JBSR)
		value = other_half;
	    if(r_scattered &&
               (r_type != PPC_RELOC_HI16_SECTDIFF &&
                r_type != PPC_RELOC_HA16_SECTDIFF &&
                r_type != PPC_RELOC_LO14_SECTDIFF &&
                r_type != PPC_RELOC_LO16_SECTDIFF)){
		offset = value - r_value;
		value = r_value;
	    }
	}

	if(reloc_found &&
	   (r_type == PPC_RELOC_HI16_SECTDIFF ||
	    r_type == PPC_RELOC_HA16_SECTDIFF ||
	    r_type == PPC_RELOC_LO14_SECTDIFF ||
	    r_type == PPC_RELOC_LO16_SECTDIFF)){
	    if(r_type == PPC_RELOC_HI16_SECTDIFF)
		printf("hi16(");
	    else if(r_type == PPC_RELOC_HA16_SECTDIFF)
		printf("ha16(");
	    else
		printf("lo16(");
	    add = guess_symbol(r_value, sorted_symbols,
			       nsorted_symbols, verbose);
	    sub = guess_symbol(pair_r_value, sorted_symbols,
			       nsorted_symbols, verbose);
	    offset = value - (r_value - pair_r_value);
	    if(add != NULL)
		printf("%s", add);
	    else
		printf("0x%x", (unsigned int)r_value);
	    if(sub != NULL)
		printf("-%s", sub);
	    else
		printf("-0x%x", (unsigned int)pair_r_value);
	    if(offset != 0)
		printf("+0x%x", (unsigned int)offset);
	    printf(")");
	    return;
	}

	low = 0;
	high = nsorted_symbols - 1;
	mid = (high - low) / 2;
	while(high >= low){
	    if(sorted_symbols[mid].n_value == value){
		if(reloc_found){
		    switch(r_type){
		    case PPC_RELOC_HI16:
			if(offset == 0)
			    printf("hi16(%s)",
				   sorted_symbols[mid].name);
			else
			    printf("hi16(%s+0x%x)",
				    sorted_symbols[mid].name,
				    (unsigned int)offset);
			break;
		    case PPC_RELOC_HA16:
			if(offset == 0)
			    printf("ha16(%s)",
				   sorted_symbols[mid].name);
			else
			    printf("ha16(%s+0x%x)",
				    sorted_symbols[mid].name,
				    (unsigned int)offset);
			break;
		    case PPC_RELOC_LO16:
		    case PPC_RELOC_LO14:
			if(offset == 0)
			    printf("lo16(%s)",
				   sorted_symbols[mid].name);
			else
			    printf("lo16(%s+0x%x)",
				   sorted_symbols[mid].name,
				   (unsigned int)offset);
			break;
		    default:
			if(offset == 0)
			    printf("%s",sorted_symbols[mid].name);
			else
			    printf("%s+0x%x",
				   sorted_symbols[mid].name,
				   (unsigned int)offset);
			break;
		    }
		}
		else{
		    if(offset == 0)
			printf("%s",sorted_symbols[mid].name);
		    else
			printf("%s+0x%x",
			       sorted_symbols[mid].name,
			       (unsigned int)offset);
		}
		return;
	    }
	    if(sorted_symbols[mid].n_value > value){
		high = mid - 1;
		mid = (high + low) / 2;
	    }
	    else{
		low = mid + 1;
		mid = (high + low) / 2;
	    }
	}
	if(offset == 0){
	    if(reloc_found){
		if(r_type == PPC_RELOC_HI16)
		    printf("hi16(0x%x)", (unsigned int)value);
		else if(r_type == PPC_RELOC_HA16)
		    printf("ha16(0x%x)", (unsigned int)value);
		else if(r_type == PPC_RELOC_LO16 ||
		        r_type == PPC_RELOC_LO14)
		    printf("lo16(0x%x)", (unsigned int)value);
		else
		    printf("0x%x", (unsigned int)value);
	    }
	    else
		printf("0x%x", (unsigned int)value);
	}
	else{
	    if(reloc_found){
		if(r_type == PPC_RELOC_HI16)
		    printf("hi16(0x%x+0x%x)",
			    (unsigned int)value, (unsigned int)offset);
		else if(r_type == PPC_RELOC_HA16)
		    printf("ha16(0x%x+0x%x)",
			    (unsigned int)value, (unsigned int)offset);
		else if(r_type == PPC_RELOC_LO16 ||
		        r_type == PPC_RELOC_LO14)
		    printf("lo16(0x%x+0x%x)",
			    (unsigned int)value, (unsigned int)offset);
		else
		    printf("0x%x+0x%x",
			    (unsigned int)value, (unsigned int)offset);
	    }
	    else
		printf("0x%x+0x%x",
			(unsigned int)value, (unsigned int)offset);
	}
	return;
}

/*
 * To handle the jsbr type instruction, we have to search for a reloc
 * of type PPC_RELOC_JBSR whenever a bl type instruction is encountered.
 * If such a reloc type exists at the correct pc, then we have to print out
 * jbsr instead of bl.  This routine uses the logic from above to loop though
 * the relocs and give the r_type for the particular address.
 */
static
unsigned long
get_reloc_r_type(
unsigned long pc,
struct relocation_info *relocs,
unsigned long nrelocs)
{
    unsigned long i;
    struct relocation_info *rp;
    unsigned long r_type, r_address;
  
	for(i = 0; i < nrelocs; i++){
	    rp = &relocs[i];
	    if(rp->r_address & R_SCATTERED){
		r_type = ((struct scattered_relocation_info *)rp)->r_type;
		r_address = ((struct scattered_relocation_info *)rp)->r_address;
	    }
	    else{
		r_type = rp->r_type;
		r_address = rp->r_address;
	    }
	    if(r_type == PPC_RELOC_PAIR)
		continue;
	    if(r_address == pc)
		return(r_type);
	}
	return(0xffffffff);
}

/*
 * For branch conditional instructions that use the Y-bit that were
 * predicted the r_length is set to 3 instead of 2.  So to correctly
 * print the prediction, we have to search for a reloc of and look at
 * the r_length.  If there is a reloc at the pc of the branch, and if the
 * r_length is 3 it then we know it was a predicted branch and we will always
 * print the prediction based on the Y-bit, the sign of the displacement
 * or the opcode (in the case of bclrX and bcctrX instructions). This routine
 * uses the logic from the above routine to loop though the relocs and give the
 * r_length for the particular address.
 */
static
unsigned long
get_reloc_r_length(
unsigned long sect_offset,
struct relocation_info *relocs,
unsigned long nrelocs)
{
    unsigned long i;
    struct relocation_info *rp;
    unsigned long r_length, r_address, r_type;
  
	for(i = 0; i < nrelocs; i++){
	    rp = &relocs[i];
	    if(rp->r_address & R_SCATTERED){
		r_type = ((struct scattered_relocation_info *)rp)->r_type;
		r_length = ((struct scattered_relocation_info *)rp)->r_length;
		r_address = ((struct scattered_relocation_info *)rp)->r_address;
	    }
	    else{
		r_type = rp->r_type;
		r_length = rp->r_length;
		r_address = rp->r_address;
	    }
	    if(r_type == PPC_RELOC_PAIR)
		continue;
	    if(r_address == sect_offset)
		return(r_length);
	}
	return(0xffffffff);
}
