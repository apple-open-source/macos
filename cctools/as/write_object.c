#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <libc.h>
#include <mach/mach.h>
#include "stuff/openstep_mach.h"
#include <mach-o/loader.h>
#include <mach-o/reloc.h>
#ifdef I860
#include <mach-o/i860/reloc.h>
#endif
#ifdef M88K
#include <mach-o/m88k/reloc.h>
#endif
#ifdef PPC
#include <mach-o/ppc/reloc.h>
#endif
#ifdef HPPA
#include <mach-o/hppa/reloc.h>
#include "stuff/hppa.h"
#endif
#ifdef SPARC
#include <mach-o/sparc/reloc.h>
#endif
#include "stuff/round.h"
#include "stuff/bytesex.h"
#include "stuff/errors.h"
#include "as.h"
#include "struc-symbol.h"
#include "symbols.h"
#include "frags.h"
#include "fixes.h"
#include "md.h"
#include "sections.h"
#include "messages.h"
#include "xmalloc.h"
#ifdef I860
#define RELOC_SECTDIFF	I860_RELOC_SECTDIFF
#define RELOC_PAIR	I860_RELOC_PAIR
#endif
#ifdef M88K
#define RELOC_SECTDIFF	M88K_RELOC_SECTDIFF
#define RELOC_PAIR	M88K_RELOC_PAIR
#endif
#ifdef PPC
#define RELOC_SECTDIFF	PPC_RELOC_SECTDIFF
#define RELOC_PAIR	PPC_RELOC_PAIR
#endif
#ifdef HPPA
#define RELOC_SECTDIFF	HPPA_RELOC_SECTDIFF
#define RELOC_PAIR	HPPA_RELOC_PAIR
#endif
#ifdef SPARC
#define RELOC_SECTDIFF	SPARC_RELOC_SECTDIFF
#define RELOC_PAIR	SPARC_RELOC_PAIR
#endif
#if defined(M68K) || defined(I386)
#define RELOC_SECTDIFF	GENERIC_RELOC_SECTDIFF
#define RELOC_PAIR	GENERIC_RELOC_PAIR
#endif

/*
 * These variables are set by layout_symbols() to organize the symbol table and
 * string table in order the dynamic linker expects.  They are then used in
 * write_object() to put out the symbols and strings in that order.
 * The order of the symbol table is:
 *	local symbols
 *	defined external symbols (sorted by name)
 *	undefined external symbols (sorted by name)
 * The order of the string table is:
 *	strings for external symbols
 *	strings for local symbols
 */
/* index to and number of local symbols */
static unsigned long ilocalsym = 0;
static unsigned long nlocalsym = 0;
/* index to, number of and array of sorted externally defined symbols */
static unsigned long iextdefsym = 0;
static unsigned long nextdefsym = 0;
static symbolS **extdefsyms = NULL;
/* index to, number of and array of sorted undefined symbols */
static unsigned long iundefsym = 0;
static unsigned long nundefsym = 0;
static symbolS **undefsyms = NULL;

static unsigned long layout_indirect_symbols(
     void);
static void layout_symbols(
    long *symbol_number,
    long *string_byte_count);
static int qsort_compare(
    const symbolS **sym1,
    const symbolS **sym2);
static unsigned long nrelocs_for_fix(
    struct fix *fixP);
static unsigned long fix_to_relocation_entries(
    struct fix *fixP,
    unsigned long sect_addr,
    struct relocation_info *riP);
#ifdef I860
static void
    I860_tweeks(void);
#endif

/*
 * write_object() writes a Mach-O object file from the built up data structures.
 */
void
write_object(
char *out_file_name)
{
    /* The structures for Mach-O relocatables */
    struct mach_header		header;
    struct segment_command	reloc_segment;
    struct symtab_command	symbol_table;
    struct dysymtab_command	dynamic_symbol_table;
    unsigned long		section_type, *indirect_symbols;
    isymbolS			*isymbolP;
    unsigned long		i, j, nsects, nsyms, strsize, nindirectsyms;

    /* locals to fill in section struct fields */
    unsigned long offset, zero;

    /* The GAS data structures */
    struct frchain *frchainP, *p;
    struct symbol *symbolP;
    struct frag *fragP;
    struct fix *fixP;

    unsigned long output_size;
    char *output_addr;
    kern_return_t r;

    enum byte_sex host_byte_sex;
    unsigned long reloff, nrelocs;
    long count;
    char *fill_literal;
    long fill_size;
    long num_bytes;
    char *symbol_name;
    int fd;

#ifdef I860
	I860_tweeks();
#endif
	i = 0; /* to shut up a compiler "may be used uninitialized" warning */

	/*
	 * The first group of things to do is to set all the fields in the
	 * header structures which includes offsets and determining the final
	 * sizes of things.
	 */

	/* 
	 * Fill in the addr and size fields of each section structure and count
	 * the number of sections.
	 */
	nsects = 0;
	for(frchainP = frchain_root; frchainP; frchainP = frchainP->frch_next){
	    frchainP->frch_section.addr = frchainP->frch_root->fr_address;
	    frchainP->frch_section.size = frchainP->frch_last->fr_address -
		   			  frchainP->frch_root->fr_address;
	    nsects++;
	}

	/*
	 * Setup the indirect symbol tables by looking up or creating symbol
	 * from the indirect symbol names and recording the symbol pointers.
	 */
	nindirectsyms = layout_indirect_symbols();

	/*
	 * Setup the symbol table to include only those symbols that will be in
	 * the object file, assign the string table offsets into the symbols
	 * and size the string table.
	 */
	nsyms = 0;
	strsize = 0;
	layout_symbols((long *)&nsyms, (long *)&strsize);

	/* fill in the Mach-O header */
	header.magic = MH_MAGIC;
	header.cputype = md_cputype;
	if(archflag_cpusubtype != -1)
	    header.cpusubtype = archflag_cpusubtype;
	else
	    header.cpusubtype = md_cpusubtype;

	header.filetype = MH_OBJECT;
	header.ncmds = 0;
	header.sizeofcmds = 0;
	if(nsects != 0){
	    header.ncmds += 1;
	    header.sizeofcmds += sizeof(struct segment_command) +
				 nsects * sizeof(struct section);
	}
	if(nsyms != 0){
	    header.ncmds += 1;
	    header.sizeofcmds += sizeof(struct symtab_command);
	    if(flagseen['k']){
		header.ncmds += 1;
		header.sizeofcmds += sizeof(struct dysymtab_command);
	    }
	}
	else
	    strsize = 0;
	header.flags = 0;

	/* fill in the segment command */
	memset(&reloc_segment, '\0', sizeof(struct segment_command));
	reloc_segment.cmd = LC_SEGMENT;
	reloc_segment.cmdsize = sizeof(struct segment_command) +
				nsects * sizeof(struct section);
	/* leave reloc_segment.segname full of zeros */
	reloc_segment.vmaddr = 0;
	reloc_segment.vmsize = 0;
	reloc_segment.filesize = 0;
	offset = header.sizeofcmds + sizeof(struct mach_header);
	reloc_segment.fileoff = offset;
	reloc_segment.maxprot = VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE;
	reloc_segment.initprot= VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE;
	reloc_segment.nsects = nsects;
	reloc_segment.flags = 0;
	/*
	 * Set the offsets to the contents of the sections (for non-zerofill
	 * sections) and set the filesize and vmsize of the segment.  This is
	 * complicated by the fact that all the zerofill sections have addresses
	 * after the non-zerofill sections and that the alignment of sections
	 * produces gaps that are not in any section.  For the vmsize we rely on
	 * the fact the the sections start at address 0 so it is just the last
	 * zerofill section or the last not-zerofill section.
	 */
	for(frchainP = frchain_root; frchainP; frchainP = frchainP->frch_next){
	    if((frchainP->frch_section.flags & SECTION_TYPE) == S_ZEROFILL)
		continue;
	    for(p = frchainP->frch_next; p != NULL; p = p->frch_next)
		if((p->frch_section.flags & SECTION_TYPE) != S_ZEROFILL)
		    break;
	    if(p != NULL)
		i = p->frch_section.addr - frchainP->frch_section.addr;
	    else
		i = frchainP->frch_section.size;
	    reloc_segment.filesize += i;
	    frchainP->frch_section.offset = offset;
	    offset += i;
	    reloc_segment.vmsize = frchainP->frch_section.addr +
				   frchainP->frch_section.size;
	}
	for(frchainP = frchain_root; frchainP; frchainP = frchainP->frch_next){
	    if((frchainP->frch_section.flags & SECTION_TYPE) != S_ZEROFILL)
		continue;
	    reloc_segment.vmsize = frchainP->frch_section.addr +
				   frchainP->frch_section.size;
	}
	offset = round(offset, sizeof(long));

	/*
	 * Count the number of relocation entries for each section.
	 */
	for(frchainP = frchain_root; frchainP; frchainP = frchainP->frch_next){
	    frchainP->frch_section.nreloc = 0;
	    for(fixP = frchainP->frch_fix_root; fixP; fixP = fixP->fx_next){
		frchainP->frch_section.nreloc += nrelocs_for_fix(fixP);
	    }
	}

	/*
	 * Fill in the offset to the relocation entries of the sections.
	 */
	offset = round(offset, sizeof(long));
	reloff = offset;
	nrelocs = 0;
	for(frchainP = frchain_root; frchainP; frchainP = frchainP->frch_next){
	    if(frchainP->frch_section.nreloc == 0)
		frchainP->frch_section.reloff = 0;
	    else
		frchainP->frch_section.reloff = offset;
	    offset += frchainP->frch_section.nreloc *
		      sizeof(struct relocation_info);
	    nrelocs += frchainP->frch_section.nreloc;
	}

	if(flagseen['k']){
	    /* fill in the fields of the dysymtab_command */
	    dynamic_symbol_table.cmd = LC_DYSYMTAB;
	    dynamic_symbol_table.cmdsize = sizeof(struct dysymtab_command);

	    dynamic_symbol_table.ilocalsym = ilocalsym;
	    dynamic_symbol_table.nlocalsym = nlocalsym;
	    dynamic_symbol_table.iextdefsym = iextdefsym;
	    dynamic_symbol_table.nextdefsym = nextdefsym;
	    dynamic_symbol_table.iundefsym = iundefsym;
	    dynamic_symbol_table.nundefsym = nundefsym;

	    if(nindirectsyms == 0){
		dynamic_symbol_table.nindirectsyms = 0;
		dynamic_symbol_table.indirectsymoff = 0;
	    }
	    else{
		dynamic_symbol_table.nindirectsyms = nindirectsyms;
		dynamic_symbol_table.indirectsymoff = offset;
		offset += nindirectsyms * sizeof(unsigned long);
	    }

	    dynamic_symbol_table.tocoff = 0;
	    dynamic_symbol_table.ntoc = 0;
	    dynamic_symbol_table.modtaboff = 0;
	    dynamic_symbol_table.nmodtab = 0;
	    dynamic_symbol_table.extrefsymoff = 0;
	    dynamic_symbol_table.nextrefsyms = 0;
	    dynamic_symbol_table.extreloff = 0;
	    dynamic_symbol_table.nextrel = 0;
	    dynamic_symbol_table.locreloff = 0;
	    dynamic_symbol_table.nlocrel = 0;
	}

	/* fill in the fields of the symtab_command (except the string table) */
	symbol_table.cmd = LC_SYMTAB;
	symbol_table.cmdsize = sizeof(struct symtab_command);
	if(nsyms == 0)
	    symbol_table.symoff = 0;
	else
	    symbol_table.symoff = offset;
	symbol_table.nsyms = nsyms;
	offset += symbol_table.nsyms * sizeof(struct nlist);

	/* fill in the string table fields of the symtab_command */
	if(strsize == 0)
	    symbol_table.stroff = 0;
	else
	    symbol_table.stroff = offset;
	symbol_table.strsize = round(strsize, sizeof(unsigned long));
	offset += round(strsize, sizeof(unsigned long));

	/*
	 * The second group of things to do is now with the size of everything
	 * known the object file and the offsets set in the various structures
	 * the contents of the object file can be created.
	 */

	/*
	 * Create the buffer to copy the parts of the output file into.
	 */
	output_size = offset;
	if((r = vm_allocate(mach_task_self(), (vm_address_t *)&output_addr,
			    output_size, TRUE)) != KERN_SUCCESS)
	    as_fatal("can't vm_allocate() buffer for output file of size %lu",
		     output_size);

	/* put the headers in the output file's buffer */
	host_byte_sex = get_host_byte_sex();
	offset = 0;

	/* put the mach_header in the buffer */
	memcpy(output_addr + offset, &header, sizeof(struct mach_header));
	if(host_byte_sex != md_target_byte_sex)
	    swap_mach_header((struct mach_header *)(output_addr + offset),
			     md_target_byte_sex);
	offset += sizeof(struct mach_header);

	/* put the segment_command in the buffer */
	if(nsects != 0){
	    memcpy(output_addr + offset, &reloc_segment,
		   sizeof(struct segment_command));
	    if(host_byte_sex != md_target_byte_sex)
		swap_segment_command((struct segment_command *)
				     (output_addr + offset),
				     md_target_byte_sex);
	    offset += sizeof(struct segment_command);
	}

	/* put the segment_command's section structures in the buffer */
	for(frchainP = frchain_root; frchainP; frchainP = frchainP->frch_next){
	    memcpy(output_addr + offset, &(frchainP->frch_section),
		   sizeof(struct section));
	    if(host_byte_sex != md_target_byte_sex)
		swap_section((struct section *)(output_addr + offset), 1,
				     md_target_byte_sex);
	    offset += sizeof(struct section);
	}

	/* put the symbol_command in the buffer */
	if(nsyms != 0){
	    memcpy(output_addr + offset, &symbol_table,
		   sizeof(struct symtab_command));
	    if(host_byte_sex != md_target_byte_sex)
		swap_symtab_command((struct symtab_command *)
				     (output_addr + offset),
				     md_target_byte_sex);
	    offset += sizeof(struct symtab_command);
	}

	if(flagseen['k']){
	    /* put the dysymbol_command in the buffer */
	    if(nsyms != 0){
		memcpy(output_addr + offset, &dynamic_symbol_table,
		       sizeof(struct dysymtab_command));
		if(host_byte_sex != md_target_byte_sex)
		    swap_dysymtab_command((struct dysymtab_command *)
					  (output_addr + offset),
					  md_target_byte_sex);
		offset += sizeof(struct dysymtab_command);
	    }
	}

	/* put the section contents (frags) in the buffer */
	for(frchainP = frchain_root; frchainP; frchainP = frchainP->frch_next){
	    offset = frchainP->frch_section.offset;
	    for(fragP = frchainP->frch_root; fragP; fragP = fragP->fr_next){
		know(fragP->fr_type == rs_fill);
		/* put the fixed part of the frag in the buffer */
		memcpy(output_addr + offset, fragP->fr_literal, fragP->fr_fix);
		offset += fragP->fr_fix;

		/* put the variable repeated part of the frag in the buffer */
		fill_literal = fragP->fr_literal + fragP->fr_fix;
		fill_size = fragP->fr_var;
		know(fragP->fr_offset >= 0);
		if(fill_size != 0)
		    num_bytes = fragP->fr_offset % fill_size;
		else
		    num_bytes = 0;
		if (num_bytes){
		    long zero = 0;
		    memcpy(output_addr + offset, &zero, num_bytes);
		    offset += num_bytes;
		}
		for(count = fragP->fr_offset-num_bytes;
		    count > 0;
		    count -= fill_size){
		    memcpy(output_addr + offset, fill_literal, fill_size);
		    offset += fill_size;
		}
	    }
	}


	/* put the symbols in the output file's buffer */
	offset = symbol_table.symoff;
	for(symbolP = symbol_rootP; symbolP; symbolP = symbolP->sy_next){
	    if((symbolP->sy_type & N_EXT) == 0){
		symbol_name = symbolP->sy_nlist.n_un.n_name;
		symbolP->sy_nlist.n_un.n_strx = symbolP->sy_name_offset;
		if(symbolP->expression != 0) {
		    expressionS *exp;

		    exp = (expressionS *)symbolP->expression;
		    symbolP->sy_nlist.n_value +=
			exp->X_add_symbol->sy_value -
			exp->X_subtract_symbol->sy_value;
		}
		memcpy(output_addr + offset, (char *)(&symbolP->sy_nlist),
		       sizeof(struct nlist));
		symbolP->sy_nlist.n_un.n_name = symbol_name;
		offset += sizeof(struct nlist);
	    }
	}
	for(i = 0; i < nextdefsym; i++){
	    symbol_name = extdefsyms[i]->sy_nlist.n_un.n_name;
	    extdefsyms[i]->sy_nlist.n_un.n_strx = extdefsyms[i]->sy_name_offset;
	    memcpy(output_addr + offset, (char *)(&extdefsyms[i]->sy_nlist),
	           sizeof(struct nlist));
	    extdefsyms[i]->sy_nlist.n_un.n_name = symbol_name;
	    offset += sizeof(struct nlist);
	}
	for(j = 0; j < nundefsym; j++){
	    symbol_name = undefsyms[j]->sy_nlist.n_un.n_name;
	    undefsyms[j]->sy_nlist.n_un.n_strx = undefsyms[j]->sy_name_offset;
	    memcpy(output_addr + offset, (char *)(&undefsyms[j]->sy_nlist),
	           sizeof(struct nlist));
	    undefsyms[j]->sy_nlist.n_un.n_name = symbol_name;
	    offset += sizeof(struct nlist);
	}
	if(host_byte_sex != md_target_byte_sex)
	    swap_nlist((struct nlist *)(output_addr + symbol_table.symoff),
		       symbol_table.nsyms, md_target_byte_sex);

	/*
	 * Put the relocation entries for each section in the buffer.
	 */
	for(frchainP = frchain_root; frchainP; frchainP = frchainP->frch_next){
	    offset = frchainP->frch_section.reloff;
	    for(fixP = frchainP->frch_fix_root; fixP; fixP = fixP->fx_next){
		offset += fix_to_relocation_entries(
					fixP,
					frchainP->frch_section.addr,
					(struct relocation_info *)(output_addr +
								   offset));
	    }
	}
	if(host_byte_sex != md_target_byte_sex)
	    swap_relocation_info((struct relocation_info *)
		(output_addr + reloff), nrelocs, md_target_byte_sex);

	if(flagseen['k']){
	    /* put the indirect symbol table in the buffer */
	    offset = dynamic_symbol_table.indirectsymoff;
	    for(frchainP = frchain_root;
		frchainP != NULL;
		frchainP = frchainP->frch_next){
		section_type = frchainP->frch_section.flags & SECTION_TYPE;
		if(section_type == S_NON_LAZY_SYMBOL_POINTERS ||
		   section_type == S_LAZY_SYMBOL_POINTERS ||
		   section_type == S_SYMBOL_STUBS){
		    /*
		     * For each indirect symbol put out the symbol number.
		     */
		    for(isymbolP = frchainP->frch_isym_root;
			isymbolP != NULL;
			isymbolP = isymbolP->isy_next){

			memcpy(output_addr + offset,
			       (char *)(&isymbolP->isy_symbol->sy_number),
			       sizeof(unsigned long));
			offset += sizeof(unsigned long);
		    }
		}
	    }
	    if(host_byte_sex != md_target_byte_sex){
		indirect_symbols = (unsigned long *)(output_addr +
				    dynamic_symbol_table.indirectsymoff);
		swap_indirect_symbols(indirect_symbols, nindirectsyms, 
				      md_target_byte_sex);
	    }
	}

	/* put the strings in the output file's buffer */
	offset = symbol_table.stroff;
	if(symbol_table.strsize != 0){
	    zero = 0;
	    memcpy(output_addr + offset, (char *)&zero, sizeof(char));
	    offset += sizeof(char);
	}
	for(symbolP = symbol_rootP; symbolP; symbolP = symbolP->sy_next){
	    /* Ordinary case: not .stabd. */
	    if(symbolP->sy_name != NULL){
		if((symbolP->sy_type & N_EXT) != 0){
		    memcpy(output_addr + offset, symbolP->sy_name,
			   strlen(symbolP->sy_name) + 1);
		    offset += strlen(symbolP->sy_name) + 1;
		}
	    }
	}
	for(symbolP = symbol_rootP; symbolP; symbolP = symbolP->sy_next){
	    /* Ordinary case: not .stabd. */
	    if(symbolP->sy_name != NULL){
		if((symbolP->sy_type & N_EXT) == 0){
		    memcpy(output_addr + offset, symbolP->sy_name,
			   strlen(symbolP->sy_name) + 1);
		    offset += strlen(symbolP->sy_name) + 1;
		}
	    }
	}
	/*
         * Create the output file.  The unlink() is done to handle the problem
         * when the out_file_name is not writable but the directory allows the
         * file to be removed (since the file may not be there the return code
         * of the unlink() is ignored).
         */
	if(bad_error != 0)
	    return;
	(void)unlink(out_file_name);
	if((fd = open(out_file_name, O_WRONLY | O_CREAT | O_TRUNC, 0666)) == -1)
	    as_fatal("can't create output file: %s", out_file_name);
	if(write(fd, output_addr, output_size) != output_size)
	    as_fatal("can't write output file");
	if(close(fd) == -1)
	    as_fatal("can't close output file");
}

/*
 * layout_indirect_symbols() setups the indirect symbol tables by looking up or
 * creating symbol from the indirect symbol names and recording the symbol
 * pointers.  It returns the total count of indirect symbol table entries.
 */
static
unsigned long
layout_indirect_symbols(void)
{
    struct frchain *frchainP;
    unsigned long section_type, total, count, stride;
    isymbolS *isymbolP;
    symbolS *symbolP;

	/*
	 * Mark symbols that only appear in a lazy section with 
	 * REFERENCE_FLAG_UNDEFINED_LAZY.  To do this we first make sure a
	 * symbol exists for all non-lazy symbols.  Then we make a pass looking
	 * up the lazy symbols and if not there we make the symbol and mark it
	 * with REFERENCE_FLAG_UNDEFINED_LAZY.
	 */
	for(frchainP = frchain_root; frchainP; frchainP = frchainP->frch_next){
	    section_type = frchainP->frch_section.flags & SECTION_TYPE;
	    if(section_type == S_NON_LAZY_SYMBOL_POINTERS){
		for(isymbolP = frchainP->frch_isym_root;
		    isymbolP != NULL;
		    isymbolP = isymbolP->isy_next){
/*
(void)symbol_find_or_make(isymbolP->isy_name);
*/
		    symbolP = symbol_find(isymbolP->isy_name);
		    if(symbolP == NULL){
			symbolP = symbol_new(isymbolP->isy_name, N_UNDF, 0, 0,
					     0, &zero_address_frag);
			symbol_table_insert(symbolP);
		    }
		}
	    }
	}
	for(frchainP = frchain_root; frchainP; frchainP = frchainP->frch_next){
	    section_type = frchainP->frch_section.flags & SECTION_TYPE;
	    if(section_type == S_LAZY_SYMBOL_POINTERS ||
	       section_type == S_SYMBOL_STUBS){
		for(isymbolP = frchainP->frch_isym_root;
		    isymbolP != NULL;
		    isymbolP = isymbolP->isy_next){

		    symbolP = symbol_find(isymbolP->isy_name);
		    if(symbolP == NULL){
			symbolP = symbol_find_or_make(isymbolP->isy_name);
			symbolP->sy_desc |= REFERENCE_FLAG_UNDEFINED_LAZY;
		    }
		}
	    }
	}

	total = 0;
	for(frchainP = frchain_root; frchainP; frchainP = frchainP->frch_next){
	    section_type = frchainP->frch_section.flags & SECTION_TYPE;
	    if(section_type == S_LAZY_SYMBOL_POINTERS ||
	       section_type == S_NON_LAZY_SYMBOL_POINTERS ||
	       section_type == S_SYMBOL_STUBS){
		count = 0;
		for(isymbolP = frchainP->frch_isym_root;
		    isymbolP != NULL;
		    isymbolP = isymbolP->isy_next){

/*
symbolP = symbol_find_or_make(isymbolP->isy_name);
*/
		    symbolP = symbol_find(isymbolP->isy_name);
		    if(symbolP == NULL){
			symbolP = symbol_new(isymbolP->isy_name, N_UNDF, 0, 0,
					     0, &zero_address_frag);
			symbol_table_insert(symbolP);
		    }
		    isymbolP->isy_symbol = symbolP;
		    count++;
		}
		/*
		 * Check for missing indirect symbols.
		 */
		if(section_type == S_SYMBOL_STUBS)
		    stride = frchainP->frch_section.reserved2;
		else
		    stride = sizeof(unsigned long);
		if(frchainP->frch_section.size / stride != count)
		    as_warn("missing indirect symbols for section (%s,%s)",
			    frchainP->frch_section.segname,
			    frchainP->frch_section.sectname);
		/*
		 * Set the index into the indirect symbol table for this
		 * section into the reserved1 field.
		 */
		frchainP->frch_section.reserved1 = total;
		total += count;
	    }
	}
	return(total);
}

/*
 * layout_symbols() removes temporary symbols (symbols that are of the form L1
 * and 1:) if the -L flag is not seen so the symbol table has only the symbols
 * it will have in the output file.  Then each remaining symbol is given a
 * symbol number and a string offset for the symbol name which also sizes the
 * string table.
 * The order of the symbol table is:
 *	local symbols
 *	defined external symbols (sorted by name)
 *	undefined external symbols (sorted by name)
 * The order of the string table is:
 *	strings for external symbols
 *	strings for local symbols
 */
static
void
layout_symbols(
long *symbol_number,
long *string_byte_count)
{
    unsigned long i, j;
    symbolS *symbolP;
    symbolS **symbolPP;
    char *name;

	*symbol_number = 0;
	*string_byte_count = sizeof(char);

	/*
	 * First pass through the symbols remove temporary symbols that are not
	 * going to be in the output file.  Also number the local symbols and
	 * assign string offset to external symbols.
	 */
	symbolPP = &symbol_rootP;
	while((symbolP = *symbolPP)){
	    name = symbolP->sy_name;
	    /*
	     * Deal with temporary symbols.  Temporary symbols start with 'L'
	     * but are not stabs.  It is an error if they are undefined.  They
	     * are removed if the -L flag is not seen else they are kept.
	     */
	    if(name != NULL &&
	       (symbolP->sy_nlist.n_type & N_STAB) == 0 &&
	       name[0] == 'L'){

	        if((symbolP->sy_nlist.n_type & N_TYPE) == N_UNDF){
		    if(name[1] != '\0' && name[2] == '\001'){
			as_bad("Undefined local symbol %c (%cf or %cb)",
				name[1], name[1], name[1]);
		    }
		    else{
			as_bad("Undefined local symbol %s", name);
		    }
		    /* don't keep this symbol */
		    *symbolPP = symbolP->sy_next;
		}
	        else if(flagseen['L'] || (symbolP->sy_type & N_EXT) != 0){
		    if((symbolP->sy_type & N_EXT) == 0){
			nlocalsym++;
			symbolP->sy_number = *symbol_number;
			*symbol_number = *symbol_number + 1;
		    }
		    else{
			nextdefsym++;
			symbolP->sy_name_offset = *string_byte_count;
			*string_byte_count += strlen(symbolP->sy_name) + 1;
		    }
		    symbolPP = &(symbolP->sy_next);
		}
		else{
		    /* don't keep this symbol */
		    *symbolPP = symbolP->sy_next;
		}
	    }
	    /*
	     * All non-temporary symbols will be the symbol table in the output
	     * file.
	     */
	    else{
		/* Any undefined symbols become N_EXT. */
		if(symbolP->sy_type == N_UNDF)
		    symbolP->sy_type |= N_EXT;

		if((symbolP->sy_type & N_EXT) == 0){
		    symbolP->sy_number = *symbol_number;
		    *symbol_number = *symbol_number + 1;
		    nlocalsym++;
		}
		else{
		    if((symbolP->sy_type & N_TYPE) != N_UNDF)
			nextdefsym++;
		    else
			nundefsym++;

		    if(name != NULL){
			/* the ordinary case (symbol has a name) */
			symbolP->sy_name_offset = *string_byte_count;
			*string_byte_count += strlen(symbolP->sy_name) + 1;
		    }
		    else{
			/* the .stabd case (symbol has no name) */
			symbolP->sy_name_offset = 0;
		    }
		}
		symbolPP = &(symbolP->sy_next);
	    }
	}

	/*
	 * Check to see that any symbol that is marked as a weak_definition
	 * is a global symbol defined in a coalesced section.
	 */
	for(symbolP = symbol_rootP; symbolP; symbolP = symbolP->sy_next){
	    if((symbolP->sy_nlist.n_type & N_STAB) == 0 &&
	       (symbolP->sy_desc & N_WEAK_DEF) == N_WEAK_DEF){
		if((symbolP->sy_type & N_EXT) == 0){
		    as_bad("Non-global symbol: %s can't be a weak_definition",
			   symbolP->sy_name);
		}
		else if((symbolP->sy_type & N_TYPE) == N_UNDF){
		    as_bad("Undefined symbol: %s can't be a weak_definition",
			   symbolP->sy_name);
		}
		else if((symbolP->sy_type & N_TYPE) != N_SECT ||
			is_section_coalesced(symbolP->sy_other) == FALSE){
		  as_bad("symbol: %s can't be a weak_definition (currently "
		         "only supported in section of type coalesced)",
			 symbolP->sy_name);
		}
	    }
	}

	/* Set the indexes for symbol groups into the symbol table */
	ilocalsym = 0;
	iextdefsym = nlocalsym;
	iundefsym = nlocalsym + nextdefsym;

	/* allocate arrays for sorting externals by name */
	extdefsyms = xmalloc(nextdefsym * sizeof(symbolS *));
	undefsyms = xmalloc(nundefsym * sizeof(symbolS *));

	i = 0;
	j = 0;
	for(symbolP = symbol_rootP; symbolP; symbolP = symbolP->sy_next){
	    if((symbolP->sy_type & N_EXT) == 0){
		if(symbolP->sy_name != NULL){
		    /* the ordinary case (symbol has a name) */
		    symbolP->sy_name_offset = *string_byte_count;
		    *string_byte_count += strlen(symbolP->sy_name) + 1;
		}
		else{
		    /* the .stabd case (symbol has no name) */
		    symbolP->sy_name_offset = 0;
		}
	    }
	    else{
		if((symbolP->sy_type & N_TYPE) != N_UNDF)
		    extdefsyms[i++] = symbolP;
		else
		    undefsyms[j++] = symbolP;
	    }
	}
	qsort(extdefsyms, nextdefsym, sizeof(symbolS *),
	      (int (*)(const void *, const void *))qsort_compare);
	qsort(undefsyms, nundefsym, sizeof(symbolS *),
	      (int (*)(const void *, const void *))qsort_compare);
	for(i = 0; i < nextdefsym; i++){
	    extdefsyms[i]->sy_number = *symbol_number;
	    *symbol_number = *symbol_number + 1;
	}
	for(j = 0; j < nundefsym; j++){
	    undefsyms[j]->sy_number = *symbol_number;
	    *symbol_number = *symbol_number + 1;
	}
}

/*
 * Function for qsort to sort symbol structs by their name
 */
static
int
qsort_compare(
const symbolS **sym1,
const symbolS **sym2)
{
        return(strcmp((*sym1)->sy_name, (*sym2)->sy_name));
}

/*
 * nrelocs_for_fix() returns the number of relocation entries needed for the
 * specified fix structure.
 */
static
unsigned long
nrelocs_for_fix(
struct fix *fixP)
{
	/*
	 * If fx_addsy is NULL then this fix needs no relocation entry.
	 */
	if(fixP->fx_addsy == NULL)
	    return(0);

	/*
	 * If this fix has a subtract symbol it is a SECTDIFF relocation which
	 * takes two relocation entries.
	 */
	if(fixP->fx_subsy != NULL)
	    return(2);

	/*
	 * For RISC machines whenever we have a relocation item using the half
	 * of an address a second a relocation item describing the other
	 * half of the address is used.
	 */
#ifdef I860
	if(fixP->fx_r_type == I860_RELOC_HIGH ||
	   fixP->fx_r_type == I860_RELOC_HIGHADJ)
	    return(2);
#endif
#ifdef M88K
	if(fixP->fx_r_type == M88K_RELOC_HI16 ||
	   fixP->fx_r_type == M88K_RELOC_LO16)
	    return(2);
#endif
#ifdef PPC
	if(fixP->fx_r_type == PPC_RELOC_HI16 ||
	   fixP->fx_r_type == PPC_RELOC_LO16 ||
	   fixP->fx_r_type == PPC_RELOC_HA16 ||
	   fixP->fx_r_type == PPC_RELOC_LO14 ||
	   fixP->fx_r_type == PPC_RELOC_JBSR)
	    return(2);
#endif
#ifdef HPPA
	if(fixP->fx_r_type == HPPA_RELOC_HI21 ||
	   fixP->fx_r_type == HPPA_RELOC_LO14 ||
	   fixP->fx_r_type == HPPA_RELOC_BR17 ||
	   fixP->fx_r_type == HPPA_RELOC_JBSR)
	    return(2);
#endif
#ifdef SPARC
	if(fixP->fx_r_type == SPARC_RELOC_HI22 ||
	   fixP->fx_r_type == SPARC_RELOC_LO10)
	    return(2);
#endif
	return(1);
}

/*
 * fix_to_relocation_entries() creates the needed relocation entries for the
 * specified fix structure that is from a section who's address starts at
 * sect_addr.  It returns the number of bytes of relocation_info structs it
 * placed at riP.
 */
static
unsigned long
fix_to_relocation_entries(
struct fix *fixP,
unsigned long sect_addr,
struct relocation_info *riP)
{
    struct symbol *symbolP;
    unsigned long count;
    struct scattered_relocation_info sri;
    unsigned long sectdiff;
#ifdef HPPA
    unsigned long left21, right14;
#endif

	/*
	 * If fx_addsy is NULL then this fix needs no relocation entry.
	 */
	if(fixP->fx_addsy == NULL)
	    return(0);

	memset(riP, '\0', sizeof(struct relocation_info));
	symbolP = fixP->fx_addsy;

	switch(fixP->fx_size){
	    case 1:
		riP->r_length = 0;
		break;
	    case 2:
		riP->r_length = 1;
		break;
	    case 4:
		riP->r_length = 2;
		break;
	    default:
		as_fatal("Bad fx_size (0x%x) in fix_to_relocation_info()\n",
			 fixP->fx_size);
	}
	riP->r_pcrel = fixP->fx_pcrel;
	riP->r_address = fixP->fx_frag->fr_address + fixP->fx_where -
			 sect_addr;
	riP->r_type = fixP->fx_r_type;
	/*
	 * For undefined symbols this will be an external relocation entry.
	 * Or if this is an external coalesced symbol.
	 */
	if((symbolP->sy_type & N_TYPE) == N_UNDF ||
	   ((symbolP->sy_type & N_EXT) == N_EXT &&
	    (symbolP->sy_type & N_TYPE) == N_SECT &&
	    is_section_coalesced(symbolP->sy_other) &&
	    fixP->fx_subsy == NULL)){
	    riP->r_extern = 1;
	    riP->r_symbolnum = symbolP->sy_number;
	}
	else{
	    /*
	     * For defined symbols this will be a local relocation entry
	     * (possibly a section difference or a scattered relocation entry).
	     */
	    riP->r_extern = 0;
	    riP->r_symbolnum = symbolP->sy_other; /* nsect */

	    /*
	     * Determine if this is left as a local relocation entry or
	     * changed to a SECTDIFF relocation entry.  If this comes from a fix
	     * that has a subtract symbol it is a SECTDIFF relocation.  Which is
	     * "addsy - subsy + constant" where both symbols are defined in
	     * sections.  To encode all this information two scattered
	     * relocation entries are used.  The first has the add symbol value
	     * and the second has the subtract symbol value.
	     */
	    if(fixP->fx_subsy != NULL){
#ifdef PPC
		if(fixP->fx_r_type == PPC_RELOC_HI16)
		    sectdiff = PPC_RELOC_HI16_SECTDIFF;
		else if(fixP->fx_r_type == PPC_RELOC_LO16)
		    sectdiff = PPC_RELOC_LO16_SECTDIFF;
		else if(fixP->fx_r_type == PPC_RELOC_HA16)
		    sectdiff = PPC_RELOC_HA16_SECTDIFF;
		else
#endif
#ifdef HPPA
		if(fixP->fx_r_type == HPPA_RELOC_HI21)
		    sectdiff = HPPA_RELOC_HI21_SECTDIFF;
		else if(fixP->fx_r_type == HPPA_RELOC_LO14)
		    sectdiff = HPPA_RELOC_LO14_SECTDIFF;
		else
#endif
#ifdef SPARC
		if(fixP->fx_r_type == SPARC_RELOC_HI22)
		    sectdiff = SPARC_RELOC_HI22_SECTDIFF;
		else if(fixP->fx_r_type == SPARC_RELOC_LO10)
		    sectdiff = SPARC_RELOC_LO10_SECTDIFF;
		else
#endif
		{
		    if(fixP->fx_r_type != 0)
			as_fatal("incorrect fx_r_type (%u) for fx_subsy != 0 "
				 "in fix_to_relocation_info()",fixP->fx_r_type);
		    sectdiff = RELOC_SECTDIFF;
		}
		memset(&sri, '\0',sizeof(struct scattered_relocation_info));
		sri.r_scattered = 1;
		sri.r_length    = riP->r_length;
		sri.r_pcrel     = riP->r_pcrel;
		sri.r_address   = riP->r_address;
		sri.r_type      = sectdiff;
		sri.r_value     = symbolP->sy_value;
		*riP = *((struct relocation_info *)&sri);
		riP++;

		sri.r_type      = RELOC_PAIR;
		sri.r_value     = fixP->fx_subsy->sy_value;
		if(sectdiff == RELOC_SECTDIFF)
		    sri.r_address = 0;
#ifdef PPC
		else if(sectdiff == PPC_RELOC_HI16_SECTDIFF ||
		        sectdiff == PPC_RELOC_HA16_SECTDIFF){
		    sri.r_address = (symbolP->sy_value -
				     fixP->fx_subsy->sy_value
				     + fixP->fx_offset) & 0xffff;
		}
		else if(sectdiff == PPC_RELOC_LO16_SECTDIFF){
		    sri.r_address = ((symbolP->sy_value -
				      fixP->fx_subsy->sy_value +
				      fixP->fx_offset) >> 16) & 0xffff;
		}
#endif
#ifdef HPPA
		else if(sectdiff == HPPA_RELOC_HI21_SECTDIFF){
		    calc_hppa_HILO(symbolP->sy_value - fixP->fx_subsy->sy_value,
				   fixP->fx_offset, &left21, &right14);
		    sri.r_address = right14 & 0x3fff;
		}
		else if(sectdiff == HPPA_RELOC_LO14_SECTDIFF){
		    calc_hppa_HILO(symbolP->sy_value - fixP->fx_subsy->sy_value,
				   fixP->fx_offset, &left21, &right14);
		    sri.r_address = left21 >> 11;
		}
#endif
#ifdef SPARC
		else if(sectdiff == SPARC_RELOC_HI22_SECTDIFF){
		    sri.r_address = (symbolP->sy_value -
				     fixP->fx_subsy->sy_value
				     + fixP->fx_offset) & 0x3ff;
		}
		else if(sectdiff == SPARC_RELOC_LO10_SECTDIFF){
		    sri.r_address = ((symbolP->sy_value -
				      fixP->fx_subsy->sy_value +
				      fixP->fx_offset) >> 10) & 0x3fffff;
		}
#endif
		*riP = *((struct relocation_info *)&sri);
		return(2 * sizeof(struct relocation_info));
	    }
	    /*
	     * Determine if this is left as a local relocation entry must be
	     * changed to a scattered relocation entry.  These entries allow
	     * the link editor to scatter the contents of a section and a local
	     * relocation can't be used when an offset is added to the symbol's
	     * value (symbol + offset).  This is because the relocation must be
	     * based on the value of the symbol not the value of the expression.
	     * Thus a scattered relocation entry that encodes the address of the
	     * symbol is used when the offset is non-zero.
	     */
#if !defined(I860)
	    /*
	     * For processors that don't have all references as unique 32 bits
	     * wide references scattered relocation entries are not generated.
	     * This is so that the link editor does not get stuck not being able
	     * to do the relocation if the high half of the reference is shared
	     * by two references to two different symbols.
	     */
	    if(fixP->fx_offset != 0 &&
	       ((symbolP->sy_type & N_TYPE) & ~N_EXT) != N_ABS
#ifdef M68K
	       /*
		* Since the m68k's pc relative branch instructions use the
		* address of the beginning of the displacement (except for
		* byte) the code in m68k.c when generating fixes adds to the
		* offset 2 for word and 4 for long displacements.
		*/
	       && !(fixP->fx_pcrel &&
	            ((fixP->fx_size == 2 && fixP->fx_offset == 2) ||
	             (fixP->fx_size == 4 && fixP->fx_offset == 4)) )
#endif /* M68K */
	       ){

		memset(&sri, '\0',sizeof(struct scattered_relocation_info));
		sri.r_scattered = 1;
		sri.r_length    = riP->r_length;
		sri.r_pcrel     = riP->r_pcrel;
		sri.r_address   = riP->r_address;
		sri.r_type      = riP->r_type;
		sri.r_value     = symbolP->sy_value;
		*riP = *((struct relocation_info *)&sri);
	    }
#endif /* !defined(I860) */
	}
	count = 1;
	riP++;

#if !defined(M68K) && !defined(I386)
	/*
	 * For RISC machines whenever we have a relocation item using the half
	 * of an address we also emit a relocation item describing the other
	 * half of the address so the linker can reconstruct the address to do
	 * the relocation.
	 */
#ifdef I860
	if(fixP->fx_r_type == I860_RELOC_HIGH ||
	   fixP->fx_r_type == I860_RELOC_HIGHADJ)
#endif
#ifdef M88K
	if(fixP->fx_r_type == M88K_RELOC_HI16 ||
	   fixP->fx_r_type == M88K_RELOC_LO16)
#endif
#ifdef PPC
	if(fixP->fx_r_type == PPC_RELOC_HI16 ||
	   fixP->fx_r_type == PPC_RELOC_LO16 ||
	   fixP->fx_r_type == PPC_RELOC_HA16 ||
	   fixP->fx_r_type == PPC_RELOC_LO14 ||
	   fixP->fx_r_type == PPC_RELOC_JBSR)
#endif
#ifdef HPPA
	if(fixP->fx_r_type == HPPA_RELOC_HI21 ||
	   fixP->fx_r_type == HPPA_RELOC_LO14 ||
	   fixP->fx_r_type == HPPA_RELOC_BR17 ||
	   fixP->fx_r_type == HPPA_RELOC_JBSR)
#endif
#ifdef SPARC
	if(fixP->fx_r_type == SPARC_RELOC_HI22 ||
	   fixP->fx_r_type == SPARC_RELOC_LO10)
#endif
	{
	    memset(riP, '\0', sizeof(struct relocation_info));
	    switch(fixP->fx_size){
		case 1:
		    riP->r_length = 0;
		    break;
		case 2:
		    riP->r_length = 1;
		    break;
		case 4:
		    riP->r_length = 2;
		    break;
		default:
		    as_fatal("Bad fx_size (0x%x) in fix_to_relocation_info()\n",
			     fixP->fx_size);
	    }
	    riP->r_pcrel = fixP->fx_pcrel;
	    /*
	     * We set r_extern to 0, so other apps won't try to use r_symbolnum
	     * as a symbol table indice.  We set all the bits of r_symbolnum so 
	     * it is all but guaranteed to be outside the range we use for non-
	     * external types to denote what section the relocation is in.
	     */
	    riP->r_extern = 0;
	    riP->r_symbolnum = 0x00ffffff;
#ifdef I860
	    riP->r_type	= I860_RELOC_PAIR;
	    riP->r_address = 0xffff & fixP->fx_value;
#endif
#ifdef M88K
	    riP->r_type = M88K_RELOC_PAIR;
	    if(fixP->fx_r_type == M88K_RELOC_HI16)
		riP->r_address = 0xffff & fixP->fx_value;
	    else if(fixP->fx_r_type == M88K_RELOC_LO16)
		riP->r_address = 0xffff & (fixP->fx_value >> 16);
#endif
#ifdef PPC
	    riP->r_type = PPC_RELOC_PAIR;
	    if(fixP->fx_r_type == PPC_RELOC_HI16 ||
	       fixP->fx_r_type == PPC_RELOC_HA16)
		riP->r_address = 0xffff & fixP->fx_value;
	    else if(fixP->fx_r_type == PPC_RELOC_LO16 ||
		    fixP->fx_r_type == PPC_RELOC_LO14)
		riP->r_address = 0xffff & (fixP->fx_value >> 16);
	    else if (fixP->fx_r_type == PPC_RELOC_JBSR){
		/*
		 * To allow the "true target address" to use the full 32 bits
		 * we convert this PAIR relocation entry to a scattered
		 * relocation entry if the true target address has the
		 * high bit (R_SCATTERED) set and store the "true target
		 * address" in the r_value field.  Or for an external relocation
		 * entry if the "offset" to the symbol has the high bit set
		 * we also use a scattered relocation entry.
		 */
		if((fixP->fx_value & R_SCATTERED) == 0){
		    riP->r_address = fixP->fx_value;
		}
		else{
		    memset(&sri, '\0',sizeof(struct scattered_relocation_info));
		    sri.r_scattered = 1;
		    sri.r_pcrel     = riP->r_pcrel;
		    sri.r_length    = riP->r_length;
		    sri.r_type      = riP->r_type;
		    sri.r_address   = 0;
		    sri.r_value     = fixP->fx_value;
		    *riP = *((struct relocation_info *)&sri);
		}
	    }
#endif
#ifdef HPPA
	    riP->r_type	 = HPPA_RELOC_PAIR;
	    calc_hppa_HILO(fixP->fx_value - fixP->fx_offset,
			   fixP->fx_offset, &left21, &right14);
	    if (fixP->fx_r_type == HPPA_RELOC_LO14 ||
		fixP->fx_r_type == HPPA_RELOC_BR17)
		riP->r_address = left21 >> 11;
	    else if (fixP->fx_r_type == HPPA_RELOC_HI21)
		riP->r_address = right14 & 0x3fff;
	    else if (fixP->fx_r_type == HPPA_RELOC_JBSR){
		if((symbolP->sy_type & N_TYPE) == N_UNDF)
		    riP->r_address = fixP->fx_value & 0xffffff;
		else
		    riP->r_address = (fixP->fx_value - sect_addr) & 0xffffff;
	    }
#endif
#ifdef SPARC
	    riP->r_type	 = SPARC_RELOC_PAIR;
	    if (fixP->fx_r_type == SPARC_RELOC_HI22)
		riP->r_address = fixP->fx_value & 0x3ff;
	    else if (fixP->fx_r_type == SPARC_RELOC_LO10)
		riP->r_address = (fixP->fx_value >> 10) & 0x3fffff;
#endif
	    count = 2;
	}
#endif /* !defined(M68K) && !defined(I386) */
	return(count * sizeof(struct relocation_info));
}

#ifdef I860
/*
 * set_default_section_align() is used to set a default minimum section
 * alignment if the section exist.
 */
static
void
set_default_section_align(
char *segname,
char *sectname,
unsigned long align)
{
    frchainS *frcP;

	for(frcP = frchain_root; frcP != NULL; frcP = frcP->frch_next){
	    if(strncmp(frcP->frch_section.segname, segname,
		       sizeof(frcP->frch_section.segname)) == 0 &&
	       strncmp(frcP->frch_section.sectname, sectname,
		       sizeof(frcP->frch_section.sectname)) == 0){
		if(align > frcP->frch_section.align)
		    frcP->frch_section.align = align;
		return;
	    }
	}
}

/* 
 * clear_section_flags() clears the section types for literals from the section
 * flags field.  This is needed for processors that don't have all references
 * to sections as unique 32 bits wide references.  In this case the literal
 * flags are not set.  This is so that the link editor does not merge them and
 * get stuck not being able to fit the relocated address in the item to be
 * relocated or if the high half of the reference is shared by two references
 * to different symbols (which can also stick the link editor).
 */
static
void
clear_section_flags(void)
{
    frchainS *frcP;

	for(frcP = frchain_root; frcP != NULL; frcP = frcP->frch_next)
	    if(frcP->frch_section.flags != S_ZEROFILL)
		frcP->frch_section.flags = 0;
}

/*
 * I860_tweeks() preforms the tweeks needed by the I860 processor to get minimum
 * section alignments and no merging of literals by the link editor.
 */
static
void
I860_tweeks(void)
{
	set_default_section_align("__TEXT", "__text", 5);
	set_default_section_align("__DATA", "__data", 4);
	set_default_section_align("__DATA", "__bss",  4);

	clear_section_flags();
}
#endif
