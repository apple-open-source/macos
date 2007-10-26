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
#include <mach-o/hppa/reloc.h>
#include "stuff/symbol.h"
#include "stuff/bytesex.h"
#include "stuff/hppa.h"
#include "otool.h"
#include "ofile_print.h"
#include "../as/hppa-opcode.h"

static char *add_conds[] = {"",",tr",",=",",<>",",<",",>=",",<=",",>",
                            ",nuv",",uv",",znv",",vnz",",sv",",nsv",",od",",ev"};
static char *comp_conds[] = {"",",tr",",=",",<>",",<",",>=",",<=",",>",
                            ",<<",",>>=",",<<=",",>>",",sv",",nsv",",od",",ev"};
static char *shift_conds[] = {"",",=",",<",",od",",tr",",<>",",>=",",ev"};

static char *unit_conds[] = {"",",tr",",??",",??",",sbz",",nbz",",shz",",nhz",
                            ",sdc",",ndc",",??",",??",",sbc",",nbc",",shc",",nhc"};
static char *short_cmplts[] = {"",",ma","",",mb"};
static char *stbys_a_cmplts[] = {",b",",e"};
static char *stbys_m_cmplts[] = {"",",m"};
static char *index_cmplts[] = {"",",m",",s",",sm"};

static char *fp_fmts[] = {",sgl",",dbl",",??",",quad"};
static char *fp_conds[] = {",false?",",false",",?",",!<=>",",=",",=T",",?=",",!<>",
                            ",!?>=",",<",",?<",",!>=",",!?>",",<=",",?<=",",!>",
			    ",!?<=",",>",",?>",",!<=",",!?<",",>=",",?>=",",!<",
			    ",!?=",",<>",",!=",",!=T",",!?",",<=>",",true?",",true"};

/* bug #41317 .... umeshv@NeXT.com Wed Jul 27 11:38:58 PDT 1994
* These parse ",cc" and encode "cc" in 2 bits at 20, where "cc" encoding is
* as given in Tables 5-8, 5-9. Refer to 'PA-RISC 1.1 Architecture and
* Instruction Set Reference Manual, Second Edition' for the tables.
*/
static char *store_cache_hint[] = {"", ",bc", "??", "??"};
static char *load_cache_hint[] = {"", ",co", "??", "??"};

/* This macro gets bit fields using HP's numbering (MSB = 0) */

#define GET_FIELD(X, FROM, TO) \
  (((X) >> (31 - (TO))) & ((1 << ((TO) - (FROM) + 1)) - 1))

	    
static void print_immediate(
    unsigned long value, 
    unsigned long pc,
    struct relocation_info *relocs,
    unsigned long nrelocs,
    struct nlist *symbols,
    unsigned long nsymbols,
    struct symbol *sorted_symbols,
    unsigned long nsorted_symbols,
    char *strings,
    unsigned long strings_size,
    enum bool verbose);

static unsigned long get_reloc(
    unsigned long pc,
    struct relocation_info *relocs,
    unsigned long nrelocs);


/*
 * Handles the wierd floating-point register specifications for the fmpyadd &
 * fmpsub instructions.  See page 6.22 of the Instruction Set Reference Manual
 * for an explanation of the voodoo below.
 */
static
void
print_multiple_fpreg(
int reg5,
int fmt)
{
	if(fmt){
	    printf("%%fr%d",reg5|16);
	    if(reg5 & 16)
		printf("r");
	}
	else
	    printf("%%fr%d",reg5);
}

unsigned long
hppa_disassemble(
char *sect,
unsigned long left,
unsigned long addr,
unsigned long sect_addr,
enum byte_sex object_byte_sex,
struct relocation_info *relocs,
unsigned long nrelocs,
struct nlist *symbols,
unsigned long nsymbols,
struct symbol *sorted_symbols,
unsigned long nsorted_symbols,
char *strings,
unsigned long strings_size,
enum bool verbose)
{
    enum byte_sex host_byte_sex;
    enum bool swapped;
    unsigned long opcode;
    unsigned int i, num, do_completers;
    char *format, c;
    int jbsr;
    unsigned long sect_offset;
    
	jbsr = 0;
	sect_offset = addr - sect_addr;
	host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != object_byte_sex;

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
	
	/* special-case for jbsr */
	if ( ((opcode & 0xfc00e000) == 0xe8000000) && 
	     (get_reloc(sect_offset, relocs, nrelocs) == HPPA_RELOC_JBSR) ) {
	     jbsr = 1;
	     i = NUMOPCODES-1;  /* jbsr is the last entry in the table */
	     }
        else {
	  /* search through the opcode table */
	  for(i=0; i<NUMOPCODES; ++i)
	  if ((opcode & pa_opcodes[i].mask) == pa_opcodes[i].match)
	    break;
          }
	
	if (i<NUMOPCODES) {
	  printf("%s",pa_opcodes[i].name);
	  format = pa_opcodes[i].args;
	  do_completers = 1;
	  while((c = *(format++))) {
	    if (do_completers && (!strchr ("cCY<?!-+&U>~nZFGM,lL3a", c))) {
	      printf("\t");
	      do_completers=0;
	      }
	    switch (c) {
	      case 'x' : printf("%%r%lu",GET_FIELD(opcode, 11, 15)); break;
	      case 'b' : printf("%%r%lu",GET_FIELD(opcode, 6, 10)); break;
	      case 't' : printf("%%r%lu",GET_FIELD(opcode, 27, 31)); break;
	      case 'B' : if ((num = GET_FIELD(opcode, 16, 17)))
	                   printf(" %u,",num);
			 printf("%%r%lu",GET_FIELD(opcode, 6, 10));
			 break;
	      case 'n' : if (opcode & 2) printf(",n");
			 break;
	      case '5'   : num = low_sign_ext(GET_FIELD(opcode,11,15),5);
	      		   printf("0x%x",num);
			   break;
	      case 'Z' : if (GET_FIELD(opcode, 26, 26)) printf(",m");
	                 break;
	      case 'S' : printf("%%sr%lu",assemble_3(GET_FIELD(opcode,16,18)));
	                 break;
	      case '?' :
	      case '<' : printf("%s",comp_conds[GET_FIELD(opcode,16,18)<<1]);
			 break;
	      case '-' : printf("%s",comp_conds[GET_FIELD(opcode,16,19)]);
			 break;
	      case '~' :
	      case '>' : printf("%s",shift_conds[GET_FIELD(opcode,16,18)]);
			 break;
	      case 'U' : printf("%s",unit_conds[GET_FIELD(opcode,16,19)]);
			 break;
	      case 'C'  :printf("%s",short_cmplts[GET_FIELD(opcode,26,26) |
	                                         (GET_FIELD(opcode,18,18)<<1)]);
			 break;
	      case 'c'  :printf("%s",index_cmplts[GET_FIELD(opcode,26,26) |
	                                         (GET_FIELD(opcode,18,18)<<1)]);
			 break;
	      case '!' : printf("%s",add_conds[GET_FIELD(opcode,16,18)<<1]);
			 break;
	      case '+' : 
	      case '&' : printf("%s",add_conds[GET_FIELD(opcode,16,19)]);
			 break;
	      case 'j'  :
	      		 num = low_sign_ext(GET_FIELD(opcode,18,31),14);
			 print_immediate(num, sect_offset, relocs, nrelocs,
			    symbols, nsymbols, sorted_symbols, nsorted_symbols,
			    strings, strings_size, verbose);
			 break;
	      case 'w'   : num = sign_ext(assemble_12(GET_FIELD(opcode,19,29), 
	                                              GET_FIELD(opcode,31,31)),12);
			   num = addr+(num*4)+8;
			   print_immediate(num, sect_offset, relocs, nrelocs,
			    symbols, nsymbols, sorted_symbols, nsorted_symbols,
			    strings, strings_size, verbose);
			   break;
	      case 'z'   : 
	      case 'W'   : num = sign_ext(assemble_17(GET_FIELD(opcode,11,15),
	      					      GET_FIELD(opcode,19,29),
						      GET_FIELD(opcode,31,31)),17);
			   num*=4;
			   if (c=='W') num += addr+8;
			   if (jbsr) printf("0x%x",num);
			     else 
			   print_immediate(num, sect_offset, relocs, nrelocs,
			    symbols, nsymbols, sorted_symbols, nsorted_symbols,
			    strings, strings_size, verbose);
			   break;
	      case 'k'   : num = assemble_21(GET_FIELD(opcode,11,31))<<11;
			   print_immediate(num, sect_offset, relocs, nrelocs,
			    symbols, nsymbols, sorted_symbols, nsorted_symbols,
			    strings, strings_size, verbose);
			   break;
	      case 'i'   : num = low_sign_ext(GET_FIELD(opcode,18,31),11);
	      		   printf("0x%x",num);
			   break;
	      case 'T'   : num = 32 - GET_FIELD(opcode,27,31);
	      		   printf("%d",num);
			   break;
	      case 'p'   : num = 31 - GET_FIELD(opcode, 22, 26);
	      		   printf("%d",num);
			   break;
	      case 'P'   : printf("%ld",GET_FIELD(opcode, 22, 26));
			   break;
	      case 'Q'   : printf("%ld",GET_FIELD(opcode, 6, 10));
			   break;
	      case 'R'   : printf("%ld",GET_FIELD(opcode, 11, 15));
			   break;
	      case 'r'   : printf("%ld",GET_FIELD(opcode, 27, 31));
			   break;
	      case 'M'   : printf("%s",fp_conds[GET_FIELD(opcode, 27, 31)]);
	                   break;
	      case 'u'   : printf("%ld",GET_FIELD(opcode,23,25));
	                   break;
	      case 'F'   : if (GET_FIELD(opcode,0,5)==0x0C)
	                     num = GET_FIELD(opcode, 19, 20);
			   else num = GET_FIELD(opcode,20,20);
			   printf("%s",fp_fmts[num]);
			   break;
	      case 'G'   : printf("%s",fp_fmts[GET_FIELD(opcode, 17, 18)]);
			   break;
	      case 'H'   : printf("%s",fp_fmts[1-GET_FIELD(opcode, 26, 26)]);
			   break;
	      case 'v'   : printf("%%fr%ld",GET_FIELD(opcode,27,31));
	                   if (GET_FIELD(opcode,25,25)) printf("r");
	                   break;
	      case 'E'   : printf("%%fr%ld",GET_FIELD(opcode,6,10));
	                   if (GET_FIELD(opcode,24,24)) printf("r");
	                   break;
	      case 'X'   : printf("%%fr%ld",GET_FIELD(opcode,11,15));
	      		   if ((GET_FIELD(opcode,0,5)==0x0E) &&
			       (GET_FIELD(opcode,19,19))!=0) printf("r");
			   break;
	      case 'V'   : num=low_sign_ext(GET_FIELD(opcode,27,31),5);
	      		   printf("0x%x",num);
			   break;
	      case 'f'   : printf("%ld",GET_FIELD(opcode, 23, 25));
	      		   break;
	      case '4'   : print_multiple_fpreg(GET_FIELD(opcode,6,10),
					       GET_FIELD(opcode,26,26));
			   break;
	      case '6'   : print_multiple_fpreg(GET_FIELD(opcode,11,15),
					       GET_FIELD(opcode,26,26));
			   break;
	      case '7'   : print_multiple_fpreg(GET_FIELD(opcode,16,20),
					       GET_FIELD(opcode,26,26));
			   break;
	      case '9'   : print_multiple_fpreg(GET_FIELD(opcode,21,25),
					       GET_FIELD(opcode,26,26));
			   break;
	      case '8'   : print_multiple_fpreg(GET_FIELD(opcode,27,31),
					       GET_FIELD(opcode,26,26));
			   break;
	      case 'o'   : printf("%ld",GET_FIELD(opcode,6,20));
	                   break;
	      case 'O'   : num = (GET_FIELD(opcode, 6,20) << 5) |
			          GET_FIELD(opcode, 27, 31);
	                   printf("%d",num);
			   break;
	      case 'Y'   : printf("%s%s%s",stbys_a_cmplts[GET_FIELD(opcode,18,18)],
	                                  stbys_m_cmplts[GET_FIELD(opcode,26,26)],
                                    store_cache_hint[GET_FIELD(opcode,20,21)]);
			   break;
	      case 'D'   : printf("%ld",GET_FIELD(opcode,6,31));
	      		   break;
	      case 'A'   : printf("%ld",GET_FIELD(opcode,6,18));
	      		   break;
	      case '2'   : printf("%ld",(GET_FIELD(opcode,6,22) << 5) |
			                GET_FIELD(opcode,27,31));
	      		   break;
	      case '1'   : printf("%ld",(GET_FIELD(opcode,11,20) << 5) |
			                GET_FIELD(opcode,27,31));
	      		   break;
	      case '0'   : printf("%ld",(GET_FIELD(opcode,16,20) << 5) |
			                GET_FIELD(opcode,27,31));
	      		   break;
	      case '@'   : print_immediate(sect_addr, sect_offset, relocs, nrelocs,
			    symbols, nsymbols, sorted_symbols, nsorted_symbols,
			    strings, strings_size, verbose);
			   break;
		  case 'l'   :printf("%s%s",short_cmplts[GET_FIELD(opcode,26,26) |
	                                         (GET_FIELD(opcode,18,18)<<1)],
                                    store_cache_hint[GET_FIELD(opcode,20,21)]);
		  		break;
		  case 'L'   :printf("%s%s",index_cmplts[GET_FIELD(opcode,26,26) |
	                                         (GET_FIELD(opcode,18,18)<<1)],
                                    load_cache_hint[GET_FIELD(opcode,20,21)]);
		  		break;
		  case '3'   :printf("%s%s",index_cmplts[GET_FIELD(opcode,26,26) |
	                                         (GET_FIELD(opcode,18,18)<<1)],
                                    store_cache_hint[GET_FIELD(opcode,20,21)]);
		  		break;
		  case 'a'   :printf("%s%s",short_cmplts[GET_FIELD(opcode,26,26) |
	                                         (GET_FIELD(opcode,18,18)<<1)],
                                    load_cache_hint[GET_FIELD(opcode,20,21)]);

		  		break;
	      default : printf("%c",c);
	      }
	    }
	  printf("\n");
	  }
	else {
	  printf(".long 0x%08x\n", (unsigned int)opcode);
	  }
       return 4;
}



static
void
print_immediate(
unsigned long value, 
unsigned long pc,
struct relocation_info *relocs,
unsigned long nrelocs,
struct nlist *symbols,
unsigned long nsymbols,
struct symbol *sorted_symbols,
unsigned long nsorted_symbols,
char *strings,
unsigned long strings_size,
enum bool verbose)
{
    long reloc_found, offset;
    unsigned long i, r_address, r_symbolnum, r_type, r_extern,
		  r_value, r_scattered, pair_r_type, pair_r_value;
    unsigned long other_half;
    const char *name, *add, *sub;
    struct relocation_info *rp, *pairp;
    struct scattered_relocation_info *srp;

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
		if(r_type == HPPA_RELOC_PAIR){
		    fprintf(stderr, "Stray HPPA_RELOC_PAIR relocation entry "
			    "%lu\n", i);
		    continue;
		}
		if(r_address == pc){
		    if(r_type == HPPA_RELOC_HI21 ||
		       r_type == HPPA_RELOC_LO14 ||
		       r_type == HPPA_RELOC_BR17 ||
		       r_type == HPPA_RELOC_JBSR ||
		       r_type == HPPA_RELOC_SECTDIFF ||
		       r_type == HPPA_RELOC_HI21_SECTDIFF ||
		       r_type == HPPA_RELOC_LO14_SECTDIFF){
			if(i+1 < nrelocs){
			    pairp = &rp[1];
			    if(pairp->r_address & R_SCATTERED){
				srp = (struct scattered_relocation_info *)pairp;
				other_half = srp->r_address;
				pair_r_type = srp->r_type;
				pair_r_value = srp->r_value;
			    }
			    else{
				other_half = pairp->r_address;
				pair_r_type = pairp->r_type;
			    }
			    if(pair_r_type != HPPA_RELOC_PAIR){
				fprintf(stderr, "No HPPA_RELOC_PAIR relocation "
					"entry after entry %lu\n", i);
				continue;
			    }
			}
		    }
		    reloc_found = 1;
		    break;
		}
		if(r_type == HPPA_RELOC_HI21 ||
		   r_type == HPPA_RELOC_LO14 ||
		   r_type == HPPA_RELOC_BR17 ||
		   r_type == HPPA_RELOC_JBSR ||
		   r_type == HPPA_RELOC_SECTDIFF ||
		   r_type == HPPA_RELOC_HI21_SECTDIFF ||
		   r_type == HPPA_RELOC_LO14_SECTDIFF){
		    if(i+1 < nrelocs){
			pairp = &rp[1];
			if(pairp->r_address & R_SCATTERED){
			    srp = (struct scattered_relocation_info *)pairp;
			    pair_r_type = srp->r_type;
			}
			else{
			    pair_r_type = pairp->r_type;
			}
			if(pair_r_type == HPPA_RELOC_PAIR)
			    i++;
			else
			    fprintf(stderr, "No HPPA_RELOC_PAIR relocation "
				    "entry after entry %lu\n", i);
		    }
		}
	    }
	}

	if(reloc_found && r_extern == 1){
	    if(symbols[r_symbolnum].n_un.n_strx < 0 ||
	       (unsigned long)symbols[r_symbolnum].n_un.n_strx >= strings_size)
		name = "bad string offset";
	    else
		name = strings + symbols[r_symbolnum].n_un.n_strx;
	    switch(r_type){
	    case HPPA_RELOC_HI21:
		value += sign_ext(other_half,14);
		printf("L`%s", name);
		if (value) 
		  printf("+0x%x",(unsigned int)value);
		break;
	    case HPPA_RELOC_LO14:
		value += other_half<<11;
		printf("R`%s", name);
		if (value) 
		  printf("+0x%x",(unsigned int)value);
		break;
	    case HPPA_RELOC_BR17:
		/* word offset?  Check in the linker what happens here */
		value += other_half<<11;
		printf("R`%s", name);
		if (value) 
		  printf("+0x%x", (unsigned int)value);
		break;
	    case HPPA_RELOC_JBSR:
		printf("%s",name);
		if (other_half!=0)
		  printf("+0x%x",(unsigned int)sign_ext(other_half,24));
		break;
	    case HPPA_RELOC_BL17:
		/* for now, this falls through... */
	    default:
		printf("%s",name);
		if (value) printf("+0x%x",(unsigned int)value);
	    }
	    return;
	}

	offset = 0;
	if(reloc_found){
	    if(r_type == HPPA_RELOC_HI21 ||
	       r_type == HPPA_RELOC_HI21_SECTDIFF)
		value += sign_ext(other_half, 14);
	    else if(r_type == HPPA_RELOC_LO14 ||
		    r_type == HPPA_RELOC_LO14_SECTDIFF)
		value += other_half << 11;
	    else if(r_type == HPPA_RELOC_BR17)
		  /* word offset?  Check in the linker what happens here */
		  value += other_half << 11;
	    else if(r_type == HPPA_RELOC_JBSR)
	          value += sign_ext(other_half, 24);
	    if(r_scattered &&
	       (r_type != HPPA_RELOC_HI21_SECTDIFF &&
	        r_type != HPPA_RELOC_LO14_SECTDIFF)){
		offset = value - r_value;
		value = r_value;
	    }
	}

	if(reloc_found &&
	   (r_type == HPPA_RELOC_HI21_SECTDIFF ||
	    r_type == HPPA_RELOC_LO14_SECTDIFF)){
	    if(r_type == HPPA_RELOC_HI21_SECTDIFF)
		printf("L`");
	    else
		printf("R`");
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
	    return;
	}

	name = guess_symbol(value, sorted_symbols,
			    nsorted_symbols, verbose);
	if(name != NULL){
	    if(reloc_found){
		switch(r_type){
		case HPPA_RELOC_HI21:
		    printf("L`%s", name);
		    if(offset != 0)
			printf("+0x%x", (unsigned int)offset);
		    break;
		case HPPA_RELOC_LO14:
		case HPPA_RELOC_BR17:
		    printf("R`%s", name);
		    if(offset != 0)
			printf("+0x%x", (unsigned int)offset);
		    break;
		default:
		    printf("%s", name);
		    if(offset != 0)
			printf("+0x%x",(unsigned int)offset);
		    break;
		}
	    }
	    else{
		printf("%s", name);
		if(offset != 0)
		    printf("+0x%x", (unsigned int)offset);
	    }
	    return;
	}

	if(reloc_found){
	    if(r_type == HPPA_RELOC_HI21 ||
	       r_type == HPPA_RELOC_HI21_SECTDIFF)
		printf("L`0x%x", (unsigned int)value);
	    else if(r_type == HPPA_RELOC_LO14 ||
		    r_type == HPPA_RELOC_LO14_SECTDIFF ||
		    r_type == HPPA_RELOC_BR17)
		printf("R`0x%x", (unsigned int)value);
	    else
		printf("0x%x", (unsigned int)value);
	}
        else
	    printf("0x%x", (unsigned int)value);
	if(offset != 0)
	    printf("+0x%x", (unsigned int)offset);

	return;
}

/*
 * To handle the jsbr type instruction, we have to search for a reloc
 * of type HPPA_RELOC_JBSR whenever a bl type instruction is encountered.
 * If such a reloc type exists at the correct pc, then we have to print out
 * jbsr instead of bl.  This routine uses the logic from above to loop though
 * the relocs and give the r_type for the particular address.
 */
static
unsigned long
get_reloc(
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
	    if(r_type == HPPA_RELOC_PAIR)
		continue;
	    if(r_address == pc)
		return(r_type);
	}
	return(0xffffffff);
}
