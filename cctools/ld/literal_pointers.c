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
#ifdef SHLIB
#include "shlib.h"
#endif SHLIB
/*
 * This file contains the routines that deal with literal pointers sections.
 * A literal pointer must point to something in another literal section.  And
 * since it is a pointer and is being relocated it must have exactly one
 * relocation entry for each pointer.  Also that relocation entry must have an
 * r_length of 2 (long, since each pointer is the size of a long) and must have
 * r_pcrel set to 0 (FALSE, since the pointer is not going to have the pc added
 * to it).  As with all literals, literals in this section must beable to me
 * moved freely with respect to other literals.  This means relocation to this
 * literal must not reach outside the size of the literal.  The size of this
 * this type of section must be a multiple of 4 bytes (size of a pointer) in
 * all input files.
 */
#include <stdlib.h>
#if !(defined(KLD) && defined(__STATIC__))
#include <stdio.h>
#include <mach/mach.h>
#else /* defined(KLD) && defined(__STATIC__) */
#include <mach/kern_return.h>
#endif /* !(defined(KLD) && defined(__STATIC__)) */
#include <stdarg.h>
#include <string.h>
#include "stuff/openstep_mach.h"
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/reloc.h>
#include "stuff/bool.h"
#include "stuff/bytesex.h"

#include "ld.h"
#include "objects.h"
#include "sections.h"
#include "pass2.h"
#include "generic_reloc.h"
#include "pass1.h"
#include "symbols.h"
#include "layout.h"
#include "literal_pointers.h"
#include "cstring_literals.h"
#include "4byte_literals.h"
#include "8byte_literals.h"
#include "dylibs.h"

static unsigned long lookup_literal_pointer(
    struct merged_symbol *merged_symbol,
    struct merged_section *literal_ms,
    unsigned long merged_section_offset,
    unsigned long offset,
    struct literal_pointer_data *data, 
    struct merged_section *ms,
    enum bool *new);
static void count_reloc(
    struct merged_section *ms,
    enum bool new,
    unsigned long r_extern,
    enum bool defined);

/*
 * literal_pointer_merge() merges literal pointers from the specified section
 * in the current object file (cur_obj).  It allocates a fine relocation map and
 * sets the fine_relocs field in the section_map to it (as well as the count).
 */
__private_extern__
void
literal_pointer_merge(
struct literal_pointer_data *data, 
struct merged_section *ms,
struct section *s, 
struct section_map *section_map)
{
    long i;
    unsigned long nliterals, j;
    char *literals;
    struct fine_reloc *fine_relocs;
    struct relocation_info *relocs;
    struct relocation_info *reloc;
    struct scattered_relocation_info *sreloc;
    unsigned long r_address, r_symbolnum, r_pcrel, r_length, r_extern,
		  r_scattered, r_value;
    struct undefined_map *undefined_map;
    struct nlist *nlists;
    char *strings;
    enum bool defined, new;

    struct merged_symbol *merged_symbol, **hash_pointer;
    struct section_map *literal_map;
    struct merged_section *literal_ms;
    struct section *literal_s;
    unsigned long section_value, input_section_offset, merged_section_offset,
		  offset;

    if(s->size == 0){
	section_map->fine_relocs = NULL;
	section_map->nfine_relocs = 0;
	return;
    }
    if(s->size % 4 != 0){
	error_with_cur_obj("literal pointer section (%.16s,%.16s) size is "
			   "not a multiple of 4 bytes",s->segname, s->sectname);
	return;
    }
    nliterals = s->size / 4;
    if(s->nreloc != nliterals){
	error_with_cur_obj("literal pointer section (%.16s,%.16s) does not "
			   "have is exactly one relocation entry for each "
			   "pointer\n", s->segname, s->sectname);
	return;
    }
#ifdef DEBUG
    data->nfiles++;
    data->nliterals += nliterals;
#endif DEBUG
    /*
     * The size is not zero an it has as many relocation entries as literals so
     * this section is being relocated.
     */
    ms->relocated = TRUE;

    /*
     * Set the output_offset to -1 here so that when going through the
     * relocation entries to merge the literals the error of having more than
     * one relocation entry for each literal can be caught.
     */
    fine_relocs = allocate(nliterals * sizeof(struct fine_reloc));
    memset(fine_relocs, '\0', nliterals * sizeof(struct fine_reloc));
    for(i = 0; i < nliterals; i++){
	fine_relocs[i].output_offset = -1;
    }
    section_map->fine_relocs = fine_relocs;
    section_map->nfine_relocs = nliterals;

    /*
     * Because at this point it is known that their are exactly as many literals
     * in the section as relocation entries either could be used to merge the
     * the literals themselves.  The loop is driven off the relocation entries
     * for two reasons: first if it looped throught the literals themselfs a
     * costly search would result in trying to find the relocation entry for it
     * and by doing it the way it is done here the r_address (really an offset)
     * can be used directly to get the literal, secondly by looping through the
     * relocation entries checking for the error case of more than one
     * relocation entry refering to the same literal can be caught easily.  So
     * if everything goes without error then their must have been exactly one
     * relocation entry for each literal pointer in this section.  The reason
     * that this loop runs backwards through the relocation entries is to get
     * this implementation of merging literal pointers to match the previous
     * one so a binary compare can be done (the previous implemention went
     * through the literals and the current assembler puts out the relocation
     * entries in reverse order so these two implementions just happen to get
     * the exact same result).
     */
    relocs = (struct relocation_info *)(cur_obj->obj_addr + s->reloff);
    literals = (char *)(cur_obj->obj_addr + s->offset);
    if(cur_obj->swapped)
	swap_relocation_info(relocs, s->nreloc, host_byte_sex);
    merged_symbol = NULL;
    for(i = s->nreloc - 1; i >= 0 ; i--){
	/*
	 * Break out the fields of the relocation entry.
	 */
	if((relocs[i].r_address & R_SCATTERED) != 0){
	    sreloc = (struct scattered_relocation_info *)(relocs + i);
	    reloc = NULL;
	    r_scattered = 1;
	    r_address = sreloc->r_address;
	    r_pcrel = sreloc->r_pcrel;
	    r_length = sreloc->r_length;
	    r_value = sreloc->r_value;
	    r_extern = 0;
	    /* calculate the r_symbolnum (n_sect) from the r_value */
	    r_symbolnum = 0;
	    for(j = 0; j < cur_obj->nsection_maps; j++){
		if(r_value >= cur_obj->section_maps[j].s->addr &&
		   r_value < cur_obj->section_maps[j].s->addr +
			     cur_obj->section_maps[j].s->size){
		    r_symbolnum = j + 1;
		    break;
		}
	    }
	    if(r_symbolnum == 0){
		/*
		 * The edge case where the last address past then end of
		 * of the last section is referenced.
		 */
		for(j = 0; j < cur_obj->nsection_maps; j++){
		    if(r_value == cur_obj->section_maps[j].s->addr +
				  cur_obj->section_maps[j].s->size){
			r_symbolnum = j + 1;
			break;
		    }
		}
		if(r_symbolnum == 0){
		    error_with_cur_obj("r_value (0x%x) field of relocation "
			"entry %lu in section (%.16s,%.16s) out of range",
			(unsigned int)r_value, i, s->segname, s->sectname);
		    continue;
		}
	    }
	}
	else{
	    reloc = relocs + i;
	    sreloc = NULL;
	    r_scattered = 0;
	    r_address = reloc->r_address;
	    r_pcrel = reloc->r_pcrel;
	    r_length = reloc->r_length;
	    r_extern = reloc->r_extern;
	    r_symbolnum = reloc->r_symbolnum;
	    r_value = 0;
	}
	/*
	 * The r_address field is really an offset into the contents of the
	 * section and must reference something inside the section.
	 */
	if(r_address >= s->size){
	    error_with_cur_obj("r_address (0x%x) field of relocation entry "
		"%ld in section (%.16s,%.16s) out of range",
		(unsigned int)r_address, i, s->segname, s->sectname);
	    continue;
	}
	/*
	 * For a literal pointer section all relocation entries must be for one
	 * of the pointers and therefore the offset must be a multiple of 4,
	 * have an r_length field of 2 (long) and a r_pcrel field of 0 (FALSE).
	 */
	if(r_address % 4 != 0){
	    error_with_cur_obj("r_address (0x%x) field of relocation entry "
		"%ld in literal pointer section (%.16s,%.16s) is not a "
		"multiple of 4", (unsigned int)r_address, i, s->segname,
		s->sectname);
	    continue;
	}
	if(r_length != 2){
	    error_with_cur_obj("r_length (0x%x) field of relocation entry "
		"%ld in literal pointer section (%.16s,%.16s) is not 2 (long)",
		(unsigned int)r_length, i, s->segname, s->sectname);
	    continue;
	}
	if(r_pcrel != 0){
	    error_with_cur_obj("r_pcrel (0x%x) field of relocation entry "
		"%ld in literal pointer section (%.16s,%.16s) is not 0 "
		"(FALSE)", (unsigned int)r_pcrel, i, s->segname, s->sectname);
	    continue;
	}
	defined = TRUE;
	/*
	 * If r_extern is set this relocation entry is an external entry
	 * else it is a local entry (or scattered entry).
	 */
	if(r_extern){
	    /*
	     * This is an external relocation entry.  So the value to be
	     * added to the item to be relocated is the value of the symbol.
	     * r_symbolnum is an index into the input file's symbol table
	     * of the symbol being refered to.  The symbol must be an
	     * undefined symbol to be used in an external relocation entry.
	     */
	    if(r_symbolnum >= cur_obj->symtab->nsyms){
		error_with_cur_obj("r_symbolnum (%lu) field of external "
		    "relocation entry %ld in section (%.16s,%.16s) out of "
		    "range", r_symbolnum, i, s->segname, s->sectname);
		continue;
	    }
	    undefined_map = bsearch(&r_symbolnum, cur_obj->undefined_maps,
		cur_obj->nundefineds, sizeof(struct undefined_map),
		(int (*)(const void *, const void *))undef_bsearch);
	    if(undefined_map != NULL){
		merged_symbol = undefined_map->merged_symbol;
	    }
	    else{
		nlists = (struct nlist *)(cur_obj->obj_addr +
					  cur_obj->symtab->symoff);
		strings = cur_obj->obj_addr + cur_obj->symtab->stroff;
		if((nlists[r_symbolnum].n_type & N_EXT) != N_EXT){
		    error_with_cur_obj("r_symbolnum (%lu) field of external "
			"relocation entry %lu in section (%.16s,%.16s) refers "
			"to a non-external symbol", r_symbolnum, i, s->segname,
			s->sectname);
		    continue;
		}
		/*
		 * We must correctly catch the errors of a literal pointer
		 * refering defined global coalesced symbols with external
		 * relocation entries.
		 */
		if((nlists[r_symbolnum].n_type & N_TYPE) == N_SECT &&
		   (cur_obj->section_maps[nlists[r_symbolnum].
		    n_sect-1].s->flags & SECTION_TYPE) == S_COALESCED){
		    hash_pointer = lookup_symbol(strings +
				       nlists[r_symbolnum].n_un.n_strx);
		    if(hash_pointer == NULL){
			fatal("internal error, in literal_pointer_merge() "
			      "failed to lookup coalesced symbol %s",
			      strings + nlists[r_symbolnum].n_un.n_strx);
		    }
		    merged_symbol = *hash_pointer;
		    error_with_cur_obj("exteral symbol (%s) for external "
			"relocation entry %ld in section (%.16s,%.16s) refers "
			"a symbol not defined in a literal section",
			merged_symbol->nlist.n_un.n_name, i, s->segname,
			s->sectname);
		    continue;
		}
		else{
		    if(nlists[r_symbolnum].n_type != (N_EXT | N_UNDF)){
			error_with_cur_obj("r_symbolnum (%lu) field of "
			    "external relocation entry %lu in section "
			    "(%.16s,%.16s) refers to a non-undefined symbol",
			    r_symbolnum, i, s->segname, s->sectname);
			continue;
		    }
		    print_obj_name(cur_obj);
		    fatal("internal error, in literal_pointer_merge() symbol "
			  "index %lu in above file not in undefined map",
			  r_symbolnum);
		}
	    }
	    /*
	     * If this is an indirect symbol resolve indirection (all chains
	     * of indirect symbols have been resolved so that they point at
	     * a symbol that is not an indirect symbol).
	     */
	    if((merged_symbol->nlist.n_type & N_TYPE) == N_INDR)
		merged_symbol = (struct merged_symbol *)
				merged_symbol->nlist.n_value;
	    /*
	     * If the symbol is a common symbol it is an error
	     * because it not a pointer to a literal.
	     */
	    if(merged_symbol->nlist.n_type == (N_EXT | N_UNDF) &&
	       merged_symbol->nlist.n_value != 0){
		error_with_cur_obj("r_symbolnum (%lu) field of external "
		    "relocation entry %ld in section (%.16s,%.16s) refers to "
		    "a common symbol (%s) and is not in a literal section",
		    r_symbolnum, i, s->segname, s->sectname,
		    merged_symbol->nlist.n_un.n_name);
		continue;
	    }
	    /*
	     * If the symbol is an absolute symbol it is treated as an error
	     * because it is not known to be a pointer to a literal.
	     */
	    if((merged_symbol->nlist.n_type & N_TYPE) == N_ABS){
		error_with_cur_obj("r_symbolnum (%lu) field of external "
		    "relocation entry %ld in section (%.16s,%.16s) refers to "
		    "an absolue symbol (%s) and is not known to be in a "
		    "literal section", r_symbolnum, i, s->segname, s->sectname,
		    merged_symbol->nlist.n_un.n_name);
		continue;
	    }
	    /*
	     * For dynamic shared library format files the merged sections that
	     * could have had external relocation entries must be resolved to
	     * private extern symbols.  This is because for MH_DYLIB files all
	     * modules share the merged sections and the entire section gets
	     * relocated when the library is mapped in.  So the above
	     * restriction assures the merged section will get relocated
	     * properly and can be shared amoung library modules.
	     */
	    if(filetype == MH_DYLIB){
		/*
		 * If the symbol is undefined or not a private extern it is an
		 * error for in this section for a MH_DYLIB file.
		 */
		if(merged_symbol->nlist.n_type == (N_EXT | N_UNDF)){
		    if(merged_symbol->error_flagged_for_dylib == 0){
			error_with_cur_obj("illegal undefined reference for "
			    "MH_DYLIB output file to symbol: %s from a literal "
			    "pointer section (section (%.16s,%.16s) relocation "
			    "entry: %lu)", merged_symbol->nlist.n_un.n_name,
			    s->segname, s->sectname, i);
			merged_symbol->error_flagged_for_dylib = 1;
		    }
		}
		else if((merged_symbol->nlist.n_type & N_PEXT) != N_PEXT){
		    if(merged_symbol->error_flagged_for_dylib == 0){
			error_with_cur_obj("illegal external reference for "
			    "MH_DYLIB output file to symbol: %s (not a private "
			    "extern symbol) from a literal pointer section "
			    "(section (%.16s,%.16s) relocation entry: %lu)",
			    merged_symbol->nlist.n_un.n_name, s->segname,
			    s->sectname, i);
			merged_symbol->error_flagged_for_dylib = 1;
		    }
		}
	    }
	    /*
	     * If the symbol is an undefined symbol then the literal is an
	     * offset to be added to the value of the symbol.
	     */
	    if(merged_symbol->nlist.n_type == (N_EXT | N_UNDF)){
		literal_ms = NULL;
		merged_section_offset = 0;
		offset = get_long((long *)(literals + r_address));
		defined = FALSE;
	    }
	    else {
		/*
		 * All other types of symbol of symbol have been handled so this
		 * symbol must be defined in a section.  The section that the
		 * symbol is defined in must be a literal section or else it
		 * is an error.  Since this is an external relocation entry and
		 * the symbol is defined this means it is defined in some other
		 * object than this one.
		 */
		if((merged_symbol->nlist.n_type & N_TYPE) != N_SECT ||
		   (merged_symbol->nlist.n_type & N_EXT)  != N_EXT){
		    fatal("internal error, in merge_literal_pointers() merged "
			  "symbol %s does not have a type of N_EXT|N_SECT",
			  merged_symbol->nlist.n_un.n_name);
		}
		literal_map = &(merged_symbol->definition_object->
				section_maps[merged_symbol->nlist.n_sect - 1]);
		literal_ms = literal_map->output_section;
		if((literal_ms->s.flags & SECTION_TYPE) != S_CSTRING_LITERALS &&
		   (literal_ms->s.flags & SECTION_TYPE) != S_4BYTE_LITERALS &&
		   (literal_ms->s.flags & SECTION_TYPE) != S_8BYTE_LITERALS){
		    error_with_cur_obj("exteral symbol (%s) for external "
			"relocation entry %ld in section (%.16s,%.16s) refers "
			"a symbol not defined in a literal section",
			merged_symbol->nlist.n_un.n_name, i, s->segname,
			s->sectname);
		    continue;
		}
		section_value = merged_symbol->nlist.n_value;
		literal_s = literal_map->s;
		if(section_value < literal_s->addr ||
		   section_value > literal_s->addr + literal_s->size){
		    error_with_cur_obj("exteral symbol's (%s) address not in "
			"the section (%.16s,%.16s) it is defined in", 
			merged_symbol->nlist.n_un.n_name, literal_s->segname,
			literal_s->sectname);
		    continue;
		}
		input_section_offset = section_value - literal_s->addr;
		/*
		 * At this point it is known that the merged section the
		 * literal is defined in is a literal section.  The checking
		 * for an internal error if the section does not have fine_reloc
		 * entry is left to fine_reloc_output_offset();
		 */
		merged_section_offset = fine_reloc_output_offset(literal_map,
							 input_section_offset);
		/*
		 * since this was an external relocation entry the value of the
		 * literal pointer is symbol+offset and the relocation is done
		 * based on only the symbol's value without the offset added.
		 * That's why offset is NOT added to input_section_offset above.
		 * Also if the offset is not zero that is need to be known so
		 * that a scattered relocation entry can be created on output.
		 */
		offset = get_long((long *)(literals + r_address));
		/*
		 * merged_symbol is set to NULL for the call to
		 * lookup_literal_pointer because this symbol is not undefined.
		 */
		merged_symbol = NULL;

		/* mark the section this symbol is in as referenced */
		literal_map->output_section->referenced = TRUE;
	    }
	}
	else{
	    /*
	     * This is a local relocation entry (the value to which the item
	     * to be relocated is refering to is defined in section number
	     * r_symbolnum in this object file).  Check that r_symbolnum is not
	     * R_ABS so it can be used to directly index the section map.
	     * For scattered relocation entries r_value was previously checked
	     * to be in the section refered to by r_symbolnum.
	     */
	    if(r_symbolnum == R_ABS){
		error_with_cur_obj("r_symbolnum (0x%x) field of relocation "
		    "entry %ld in literal pointer section (%.16s,%.16s) is "
		    "R_ABS (not correct for a literal pointer section)",
		    (unsigned int)r_symbolnum, i, s->segname, s->sectname);
		continue;
	    }
	    merged_symbol = NULL;
	    literal_map = &(cur_obj->section_maps[r_symbolnum - 1]);
	    literal_s = literal_map->s;
	    literal_ms  = literal_map->output_section;
	    if(r_scattered == 0){
		offset = 0;
		section_value = get_long((long *)(literals + r_address));
		if(section_value < literal_s->addr ||
		   section_value > literal_s->addr + literal_s->size){
		    error_with_cur_obj("literal pointer (0x%x) in section "
			"(%.16s,%.16s) at address 0x%x does not point into "
			"its (%.16s,%.16s) section as refered to by its "
			"r_symbolnum (0x%x) field in relocation entry %ld as "
			"it should", (unsigned int)section_value, s->segname,
			s->sectname, (unsigned int)(s->addr + r_address),
			literal_s->segname, literal_s->sectname,
			(unsigned int)r_symbolnum, i);
		    continue;
		}
		if((literal_ms->s.flags & SECTION_TYPE) != S_CSTRING_LITERALS &&
		   (literal_ms->s.flags & SECTION_TYPE) != S_4BYTE_LITERALS &&
		   (literal_ms->s.flags & SECTION_TYPE) != S_8BYTE_LITERALS){
		    error_with_cur_obj("r_symbolnum field (0x%x) in relocation "
			"entry %ld in literal pointer section (%.16s,%.16s) "
			"refers to section (%.16s,%.16s) which is not a "
			"literal section", (unsigned int)r_symbolnum, i,
			s->segname, s->sectname, literal_ms->s.segname,
			literal_ms->s.sectname);
		    continue;
		}
	    }
	    else{
		offset = get_long((long *)(literals + r_address)) - r_value;
		section_value = r_value;
		if((literal_ms->s.flags & SECTION_TYPE) != S_CSTRING_LITERALS &&
		   (literal_ms->s.flags & SECTION_TYPE) != S_4BYTE_LITERALS &&
		   (literal_ms->s.flags & SECTION_TYPE) != S_8BYTE_LITERALS){
		    error_with_cur_obj("r_value field (0x%x) in relocation "
			"entry %ld in literal pointer section (%.16s,%.16s) "
			"refers to section (%.16s,%.16s) which is not a "
			"literal section", (unsigned int)r_value, i, s->segname,
			s->sectname, literal_ms->s.segname,
			literal_ms->s.sectname);
		    continue;
		}
	    }
	    input_section_offset = section_value - literal_s->addr;
	    /*
	     * At this point it is known that the merged section the
	     * literal is defined in is a literal section.  The checking
	     * for an internal error if the section does not have fine_reloc
	     * entry is left to fine_reloc_output_offset();
	     */
	    merged_section_offset = fine_reloc_output_offset(literal_map,
						     input_section_offset);
	    /* mark the section this literal is in as referenced */
	    literal_map->output_section->referenced = TRUE;
	}

	/* 
	 * Since all the output_offset field of all the fine reloc entries were
	 * set to -1 before merging the literals and there must be only one
	 * relocation entry for each literal pointer if the relocation entry
	 * for this literal does not have an output_offset of -1 it is an error
	 * because we have seen it before.
	 */ 
	if(fine_relocs[r_address/4].output_offset != -1){
	    error_with_cur_obj("more than one relocation entry for literal "
		"pointer at address 0x%x (r_address 0x%x) in section "
		"(%.16s,%.16s)", (unsigned int)(s->addr + r_address),
		(unsigned int)r_address, s->segname, s->sectname);
	    continue;
	}

	/*
	 * Now at long last the literal pointer can be merged and the fine
	 * relocation entries for it can be built.
	 */
	fine_relocs[r_address/4].input_offset = r_address;
	fine_relocs[r_address/4].output_offset =
		lookup_literal_pointer(merged_symbol, literal_ms,
			       merged_section_offset, offset, data, ms, &new);
	count_reloc(ms, new, r_extern, defined);
    }
}

/*
 * count reloc is used after a call to lookup_literal_pointer() to count the
 * relocation entry for the literal if it will be in the output file.
 */
static void
count_reloc(
struct merged_section *ms,
enum bool new,
unsigned long r_extern,
enum bool defined)
{
	/*
	 * If saving relocation entries count it as one of the output
	 * relocation entries.
	 */
	if(output_for_dyld && new == TRUE){
	    /*
	     * The number of relocation entries in the output file is based
	     * on one of three different cases:
	     *  The output file is a dynamic shared library file
	     *  The output file has a dynamic linker load command
	     *  The output does not have a dynamic linker load command
	     */
	    if(filetype == MH_DYLIB){
		/*
		 * For dynamic shared library files there are no external
		 * relocation entries that will be left as external as
		 * checked above.  Only non-sectdiff local relocation
		 * entries are kept.  Modules of dylibs are not linked
		 * together and can only be slid keeping all sections
		 * relative to each other the same.
		 */
		ms->nlocrel++;
	    }
	    else if(has_dynamic_linker_command){
		/*
		 * For an file with a dynamic linker load command only
		 * external relocation entries for undefined symbols are
		 * kept.  This output file is a fixed address and can't be
		 * moved.
		 */
		if(r_extern)
		    if(defined == FALSE)
			ms->nextrel++;
	    }
	    else{
		/*
		 * For an file without a dynamic linker load command
		 * external relocation entries for undefined symbols are
		 * kept and locals that are non-sectdiff are kept.  This
		 * file can only be slid keeping all sections relative to
		 * each other the same.
		 */
		if(r_extern){
		    if(defined == FALSE)
			ms->nextrel++;
		    else
			ms->nlocrel++;
		}
		else
		    ms->nlocrel++;
	    }
	}
	else if(save_reloc && new == TRUE){
	    ms->s.nreloc++;
	    nreloc++;
	}
}

/*
 * literal_pointer_order() enters literal pointers from the order_file from the
 * merged section structure.  Since this is called before any call to
 * literal_pointer_merge and it enters the literals in the order of the file it
 * causes the section to be ordered.
 */
__private_extern__
void
literal_pointer_order(
struct literal_pointer_data *data, 
struct merged_section *ms)
{
#ifndef RLD
    unsigned long i, j, line_number, line_length, max_line_length, char_pos,
	 merged_section_offset;
    char *buffer, segname[17], sectname[17];
    struct merged_section *literal_ms;
    struct literal8 literal8;
    struct literal4 literal4;
    kern_return_t r;
    enum bool new;
#ifdef __MWERKS__
    struct literal_pointer_data *dummy1;
    struct merged_section *dummy2;
        dummy1 = data;
        dummy2 = ms;
#endif

	/*
	 * Parse the load order file by changing '\n' to '\0'.  Also check for
	 * '\0 in the file and flag them as errors.  Also determine the maximum
	 * line length of the file for the needed buffer to allocate for
	 * character translation.
	 */
	line_number = 1;
	line_length = 1;
	max_line_length = 1;
	for(i = 0; i < ms->order_size; i++){
	    if(ms->order_addr[i] == '\0'){
		fatal("format error in -sectorder file: %s line %lu character "
		      "possition %lu for section (%.16s,%.16s) (illegal null "
		      "character \'\\0\' found)", ms->order_filename,
		      line_number, line_length, ms->s.segname, ms->s.sectname);
	    }
	    if(ms->order_addr[i] == '\n'){
		ms->order_addr[i] = '\0';
		if(line_length > max_line_length)
		    max_line_length = line_length;
		line_length = 1;
	    }
	    else
		line_length++;
	}

	/*
	 * Allocate the buffer to translate the order file lines' escape
	 * characters into real characters.
	 */
	buffer = allocate(max_line_length + 1);

	/*
	 * Process each line in the order file by translating all escape
	 * characters and then entering the cstring using lookup_cstring().
	 */
	line_number = 1;
	for(i = 0; i < ms->order_size; i++){
	    
	    char_pos = 1;
	    /* copy segment name into segname */
	    j = 0;
	    while(i < ms->order_size &&
		  ms->order_addr[i] != ':' &&
		  ms->order_addr[i] != '\0'){
		if(j <= 16)
		    segname[j++] = ms->order_addr[i++];
		else
		    i++;
		char_pos++;
	    }
	    if(i >= ms->order_size || ms->order_addr[i] == '\0'){
		error("format error in -sectorder file: %s line %lu for section"
		      " (%.16s,%.16s) (missing ':' after segment name)",
		      ms->order_filename, line_number, ms->s.segname,
		      ms->s.sectname);
		continue;
	    }
	    segname[j] = '\0';
	    i++;
	    char_pos++;

	    /* copy section name into sectname */
	    j = 0;
	    while(i < ms->order_size &&
		  ms->order_addr[i] != ':' &&
		  ms->order_addr[i] != '\0'){
		if(j <= 16)
		    sectname[j++] = ms->order_addr[i++];
		else
		    i++;
		char_pos++;
	    }
	    if(i >= ms->order_size || ms->order_addr[i] == '\0'){
		error("format error in -sectorder file: %s line %lu for section"
		      " (%.16s,%.16s) (missing ':' after section name)",
		      ms->order_filename, line_number, ms->s.segname,
		      ms->s.sectname);
		continue;
	    }
	    sectname[j] = '\0';
	    i++;
	    char_pos++;
	
	    literal_ms = lookup_merged_section(segname, sectname);
	    if(literal_ms == NULL){
		error("error in -sectorder file: %s line %lu for section "
		      "(%.16s,%.16s) (specified section (%s,%s) is not "
		      "loaded objects)", ms->order_filename, line_number,
		      ms->s.segname, ms->s.sectname, segname, sectname);
	    }
	    else{
		switch(literal_ms->s.flags & SECTION_TYPE){
		case S_CSTRING_LITERALS:
		    get_cstring_from_sectorder(ms, &i, buffer, line_number,
					       char_pos);
		    merged_section_offset = lookup_cstring(buffer,
					  literal_ms->literal_data, literal_ms);
		    (void)lookup_literal_pointer(NULL, literal_ms,
				    merged_section_offset, 0, data, ms, &new);
		    count_reloc(ms, new, 0, FALSE);
		    break;
		case S_4BYTE_LITERALS:
		    if(get_hex_from_sectorder(ms, &i, &(literal4.long0),
					      line_number) == TRUE){
			merged_section_offset = lookup_literal4(literal4,
					  literal_ms->literal_data, literal_ms);
		        (void)lookup_literal_pointer(NULL, literal_ms,
				    merged_section_offset, 0, data, ms, &new);
			count_reloc(ms, new, 0, FALSE);
		    }
		    else{
			error("error in -sectorder file: %s line %lu for "
			      "section (%.16s,%.16s) (missing hex number for "
			      "specified 4 byte literal section (%s,%s))",
			      ms->order_filename, line_number,
			      ms->s.segname, ms->s.sectname, segname, sectname);
		    }
		    break;
		case S_8BYTE_LITERALS:
		    if(get_hex_from_sectorder(ms, &i, &(literal8.long0),
					      line_number) == TRUE){
			if(get_hex_from_sectorder(ms, &i, &(literal8.long1),
						  line_number) == TRUE){
			    merged_section_offset = lookup_literal8(literal8,
						  literal_ms->literal_data,
						  literal_ms);
			    (void)lookup_literal_pointer(NULL, literal_ms,
				    merged_section_offset, 0, data, ms, &new);
			    count_reloc(ms, new, 0, FALSE);
			}
			else{
			    error("error in -sectorder file: %s line %lu for "
				  "section (%.16s,%.16s) (missing second hex "
				  "number for specified 8 byte literal section "
				  "(%s,%s))", ms->order_filename, line_number,
				  ms->s.segname, ms->s.sectname, segname,
				  sectname);
			}
		    }
		    else{
			error("error in -sectorder file: %s line %lu for "
			      "section (%.16s,%.16s) (missing first hex number "
			      "for specified 8 byte literal section (%s,%s))",
			      ms->order_filename, line_number,
			      ms->s.segname, ms->s.sectname, segname, sectname);
		    }
		    break;
		default:
		    error("error in -sectorder file: %s line %lu for section "
			  "(%.16s,%.16s) (specified section (%s,%s) is not a "
			  "literal section)", ms->order_filename, line_number,
			  ms->s.segname, ms->s.sectname, segname, sectname);
		    break;
		}
	    }

	    /* skip any trailing characters on the line */
	    while(i < ms->order_size && ms->order_addr[i] != '\0')
		i++;

	    line_number++;
	}

	/* deallocate the buffer */
	free(buffer);

	/*
	 * Deallocate the memory for the load order file now that it is
	 * nolonger needed (since the memory has been written on it is
	 * allways deallocated so it won't get written to the swap file
	 * unnecessarily).
	 */
	if((r = vm_deallocate(mach_task_self(), (vm_address_t)
	    ms->order_addr, ms->order_size)) != KERN_SUCCESS)
	    mach_fatal(r, "can't vm_deallocate() memory for -sectorder "
		       "file: %s for section (%.16s,%.16s)",
		       ms->order_filename, ms->s.segname,
		       ms->s.sectname);
	ms->order_addr = NULL;
#else /* RLD */
#ifdef __MWERKS__
    struct literal_pointer_data *dummy1;
    struct merged_section *dummy2;
        dummy1 = data;
        dummy2 = ms;
#endif
#endif /* RLD */
}
/*
 * lookup_literal_pointer() is passed a quad that defined a literal pointer
 * (merged_symbol, literal_ms, merged_section_offset, offset).  If merged_symbol
 * is not NULL then this pointer is the undefined merged_symbol plus the offset
 * else the pointer is into the merged literal section litersal_ms with an
 * offset into that section of merged_section_offset plus offset.  In either
 * case the literal pointer must match exactly (that means merged_section_offset
 * can't be added to offset and the sum be used to determine a match).
 */
static
unsigned long
lookup_literal_pointer(
struct merged_symbol *merged_symbol,
struct merged_section *literal_ms,
unsigned long merged_section_offset,
unsigned long offset,
struct literal_pointer_data *data, 
struct merged_section *ms,
enum bool *new)
{
    unsigned long hashval, output_offset;
    struct literal_pointer_block **p, *literal_pointer_block;
    struct literal_pointer *literal_pointer;
    struct literal_pointer_bucket *bp;

	*new = FALSE;
	if(data->hashtable == NULL){
	    data->hashtable = allocate(sizeof(struct literal_pointer_bucket *) *
						      LITERAL_POINTER_HASHSIZE);
	    memset(data->hashtable, '\0',
		   sizeof(struct literal_pointer_bucket *) *
						      LITERAL_POINTER_HASHSIZE);
	}
#if defined(DEBUG) && defined(PROBE_COUNT)
	    data->nprobes++;
#endif
	hashval = ((long)merged_symbol + (long)literal_ms +
		   merged_section_offset + offset) % LITERAL_POINTER_HASHSIZE;
	for(bp = data->hashtable[hashval]; bp; bp = bp->next){
#if defined(DEBUG) && defined(PROBE_COUNT)
	    data->nprobes++;
#endif
	    if(bp->literal_pointer->merged_symbol == merged_symbol &&
	       bp->literal_pointer->literal_ms == literal_ms &&
	       bp->literal_pointer->merged_section_offset ==
						        merged_section_offset &&
	       bp->literal_pointer->offset == offset)
		return(bp->output_offset);
	}

	bp = allocate(sizeof(struct literal_pointer_bucket));
	output_offset = 0;
	for(p = &(data->literal_pointer_blocks);
	    *p ;
	    p = &(literal_pointer_block->next)){

	    literal_pointer_block = *p;
	    if(literal_pointer_block->used != LITERAL_POINTER_BLOCK_SIZE){
		literal_pointer = literal_pointer_block->literal_pointers +
				  literal_pointer_block->used;
		literal_pointer->merged_symbol = merged_symbol;
		literal_pointer->literal_ms = literal_ms;
	        literal_pointer->merged_section_offset = merged_section_offset;
	        literal_pointer->offset = offset;

		bp->literal_pointer = literal_pointer;
		bp->output_offset = output_offset +
				    literal_pointer_block->used * 4;
		bp->next = data->hashtable[hashval];
		data->hashtable[hashval] = bp;

		literal_pointer_block->used++;
		ms->s.size += 4;
		*new = TRUE;
		return(bp->output_offset);
	    }
	    output_offset += literal_pointer_block->used * 4;
	}
	*p = allocate(sizeof(struct literal_pointer_block));
	literal_pointer_block = *p;
	literal_pointer = literal_pointer_block->literal_pointers;
	literal_pointer->merged_symbol = merged_symbol;
	literal_pointer->literal_ms = literal_ms;
	literal_pointer->merged_section_offset = merged_section_offset;
	literal_pointer->offset = offset;
	literal_pointer_block->used = 1;
	literal_pointer_block->next = NULL;

	bp->literal_pointer = literal_pointer;
	bp->output_offset = output_offset;
	bp->next = data->hashtable[hashval];
	data->hashtable[hashval] = bp;

	ms->s.size += 4;
	*new = TRUE;
	return(bp->output_offset);
}

/*
 * literal_pointer_output puts the literal pointers into the output file.
 * It also puts the relocation entries for the literal pointers in the output
 * file if relocation entries are being saved.
 */
__private_extern__
void
literal_pointer_output(
struct literal_pointer_data *data, 
struct merged_section *ms)
{
    unsigned long i;
    long *output_pointer;
    struct literal_pointer_block **p, *literal_pointer_block;
    struct literal_pointer *literal_pointers;

#ifndef RLD
    struct relocation_info *reloc, *extreloc, *r;
    struct scattered_relocation_info *sreloc;
    unsigned long r_address;
#endif !defined(RLD)

	/*
	 * Put the literal pointers into the output file.
	 */
	output_pointer = (long *)(output_addr + ms->s.offset);
	for(p = &(data->literal_pointer_blocks);
	    *p ;
	    p = &(literal_pointer_block->next)){

	    literal_pointer_block = *p;
	    literal_pointers = literal_pointer_block->literal_pointers;
	    for(i = 0; i < literal_pointer_block->used; i++){
		if(literal_pointers[i].merged_symbol != NULL){
		    *output_pointer = literal_pointers[i].offset;
		}
		else{
		    *output_pointer = 
			     literal_pointers[i].literal_ms->s.addr +
			     literal_pointers[i].merged_section_offset +
			     literal_pointers[i].offset;
		}
		if(host_byte_sex != target_byte_sex)
		    *output_pointer = SWAP_LONG(*output_pointer);
		output_pointer++;
	    }
	}
#ifndef RLD
	output_flush(ms->s.offset,
		(char *)output_pointer - (char *)(output_addr + ms->s.offset));

	/*
	 * If saving relocation entries the create the proper relocation entry
	 * for the literal pointer and put it in the file.
	 */
	if(save_reloc || output_for_dyld){
	    if(output_for_dyld){
		extreloc = (struct relocation_info *)(output_addr +
                            output_dysymtab_info.dysymtab_command.extreloff +
                            ms->iextrel * sizeof(struct relocation_info));
		reloc = (struct relocation_info *)(output_addr +
                            output_dysymtab_info.dysymtab_command.locreloff +
                            ms->ilocrel * sizeof(struct relocation_info));
		sreloc = (struct scattered_relocation_info *)reloc;

		/*
		 * For MH_SPLIT_SEGS images the r_address is relative to the
		 * first read-write segment and there are no relocation entries
		 * allowed in the read-only segments.  This is needed because
		 * the r_address field in a scattered relocation entry is 24
		 * bits which means that the normal split of 265meg wouldn't
		 * allow the use of 24 bits from the address of the first
		 * segment which is what is normally used for outputs for dyld.
		 */
		if(segs_read_only_addr_specified == TRUE)
		    r_address = ms->s.addr - segs_read_write_addr;
		else
		    r_address = ms->s.addr - merged_segments->sg.vmaddr;
	    }
	    else{
		extreloc = NULL;
		reloc = (struct relocation_info *)(output_addr + ms->s.reloff);
		sreloc = (struct scattered_relocation_info *)reloc;
		r_address = 0;
	    }

	    for(p = &(data->literal_pointer_blocks);
		*p ;
		p = &(literal_pointer_block->next)){

		literal_pointer_block = *p;
		literal_pointers = literal_pointer_block->literal_pointers;
		for(i = 0; i < literal_pointer_block->used; i++){
		    /*
		     * If the pointer is made up from an undefined merged 
		     * symbol and external relocation entry is created.
		     */
		    if(literal_pointers[i].merged_symbol != NULL){
			if(output_for_dyld)
			    r = extreloc;
			else
			    r = reloc;
			r->r_address = r_address;
			r->r_symbolnum =
				merged_symbol_output_index(
					literal_pointers[i].merged_symbol);
			r->r_pcrel = 0;
			r->r_length = 2;
			r->r_extern = 1;
			r->r_type = 0;
			if(output_for_dyld)
			    extreloc++;
			else
			    reloc++;
		    }
		    /*
		     * If the offset added to the item to be relocated is
		     * zero then local relocation entry is created.
		     */
		    else if(literal_pointers[i].offset == 0){
			reloc->r_address = r_address;
			reloc->r_symbolnum =
				 literal_pointers[i].literal_ms->output_sectnum;
			reloc->r_pcrel = 0;
			reloc->r_length = 2;
			reloc->r_extern = 0;
			reloc->r_type = 0;
			reloc++;
			sreloc++;
		    }
		    /*
		     * The offset added to the item to be relocated is NOT
		     * zero so a scattered relocation entry is created.
		     */
		    else{
			sreloc->r_scattered = 1;
			sreloc->r_pcrel = 0;
			sreloc->r_length = 2;
			sreloc->r_type = 0;
			sreloc->r_address = r_address;
			sreloc->r_value =
				      literal_pointers[i].literal_ms->s.addr +
				      literal_pointers[i].merged_section_offset;
			reloc++;
			sreloc++;
		    }
		    r_address += 4;
		}
	    }
	    if(output_for_dyld){
		if(host_byte_sex != target_byte_sex){
		    swap_relocation_info((struct relocation_info *)
			    (output_addr +
                            output_dysymtab_info.dysymtab_command.extreloff +
                            ms->iextrel * sizeof(struct relocation_info)),
			    ms->nextrel, target_byte_sex);
		    swap_relocation_info((struct relocation_info *)
			    (output_addr +
                            output_dysymtab_info.dysymtab_command.locreloff +
                            ms->ilocrel * sizeof(struct relocation_info)),
			    ms->nlocrel, target_byte_sex);
		}
		output_flush(output_dysymtab_info.dysymtab_command.extreloff +
                             ms->iextrel * sizeof(struct relocation_info),
			     ms->nextrel * sizeof(struct relocation_info));
		output_flush(output_dysymtab_info.dysymtab_command.locreloff +
                             ms->ilocrel * sizeof(struct relocation_info),
			     ms->nlocrel * sizeof(struct relocation_info));
	    }
	    else{
		if(host_byte_sex != target_byte_sex)
		    swap_relocation_info((struct relocation_info *)
			    (output_addr + ms->s.reloff), ms->s.nreloc,
			    target_byte_sex);
		output_flush(ms->s.reloff,
			     ms->s.nreloc * sizeof(struct relocation_info));
	    }
	}
#endif !defined(RLD)

	literal_pointer_free(data);
}

/*
 * literal_pointer_free() free()'s up all space used by the data block except 
 * the data block itself.
 */
__private_extern__
void
literal_pointer_free(
struct literal_pointer_data *data)
{
    unsigned long i;
    struct literal_pointer_bucket *bp, *next_bp;
    struct literal_pointer_block *literal_pointer_block,
				 *next_literal_pointer_block;

	/*
	 * Free all data for this block.
	 */
	if(data->hashtable != NULL){
	    for(i = 0; i < LITERAL_POINTER_HASHSIZE; i++){
		for(bp = data->hashtable[i]; bp; ){
		    next_bp = bp->next;
		    free(bp);
		    bp = next_bp;
		}
	    }
	    free(data->hashtable);
	    data->hashtable = NULL;
	}
	for(literal_pointer_block = data->literal_pointer_blocks;
	    literal_pointer_block;
	    literal_pointer_block = next_literal_pointer_block){

	    next_literal_pointer_block = literal_pointer_block->next;
	    free(literal_pointer_block);
	}
	data->literal_pointer_blocks = NULL;
}

#ifdef DEBUG
/*
 * print_literal_pointer_data() prints a literal_pointer_data.  Used for
 * debugging.
 */
__private_extern__
void
print_literal_pointer_data(
struct literal_pointer_data *data, 
char *indent)
{
    unsigned long i;
    struct literal_pointer_block **p, *literal_pointer_block;
    struct literal_pointer *literal_pointers;

	print("%sliteral pointer data at 0x%x\n", indent, (unsigned int)data);
	if(data == NULL)
	    return;
	for(p = &(data->literal_pointer_blocks);
	    *p ;
	    p = &(literal_pointer_block->next)){

	    literal_pointer_block = *p;
	    literal_pointers = literal_pointer_block->literal_pointers;
	    print("%sused %lu\n", indent, literal_pointer_block->used);
	    for(i = 0; i < literal_pointer_block->used; i++){
		if(literal_pointers[i].merged_symbol != NULL){
		    print("%s    symbol %s offset %lu\n", indent,
			  literal_pointers[i].merged_symbol->nlist.n_un.n_name,
			  literal_pointers[i].offset);
		}
		else{
		    print("%s    section (%.16s,%.16s) section_offset %lu "
			  "offset %lu\n", indent,
			  literal_pointers[i].literal_ms->s.segname,
			  literal_pointers[i].literal_ms->s.sectname,
			  literal_pointers[i].merged_section_offset,
			  literal_pointers[i].offset);
		}
	    }
	}
}

/*
 * literal_pointer_data_stats() prints the literal_pointer_data stats.  Used for
 * tuning.
 */
__private_extern__
void
literal_pointer_data_stats(
struct literal_pointer_data *data,
struct merged_section *ms)
{
	if(data == NULL)
	    return;
	print("literal pointer section (%.16s,%.16s) contains:\n",
	      ms->s.segname, ms->s.sectname);
	print("    %lu merged literal pointers\n", ms->s.size / 4);
	print("    from %lu files and %lu total literal pointers from those "
	      "files\n", data->nfiles, data->nliterals);
	print("    average number of literals per file %g\n",
	      (double)((double)data->nliterals / (double)(data->nfiles)));
	if(data->nprobes != 0){
	    print("    average number of hash probes %g\n",
	      (double)((double)data->nprobes / (double)(data->nliterals)));
	}
}
#endif DEBUG
