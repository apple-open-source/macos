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
 * This file contains the routines that deal with indirect sections (both
 * lazy and non-lazy symbol pointer sections as well as symbol stub sections). 
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
#include <mach-o/stab.h>
#include <mach-o/reloc.h>
#include "stuff/arch.h"
#include "stuff/reloc.h"

#include "ld.h"
#include "objects.h"
#include "sections.h"
#include "pass2.h"
#include "generic_reloc.h"
#include "pass1.h"
#include "symbols.h"
#include "layout.h"
#include "coalesced_sections.h"
#include "dylibs.h"

/*
 * coalesced_section_merge() merges items from a coalesced section from the
 * specified section in the current object file (cur_obj).  It allocates a fine
 * relocation map and sets the fine_relocs field in the section_map to it (as
 * well as the count).
 *
 * After all the items for this section in this object file have been merged
 * two more things are done.  First the number of relocation entries that will
 * be in the output file is adjusted (incremented) based on which items are used
 * from this object's section.  Second the number of local symbol table entries
 * and the size of the string table is adjusted (decremented) based on the which
 * symbols are in the items from this object's section that will be in the
 * resulting object file.
 */
__private_extern__
void
coalesced_section_merge(
void *data,
struct merged_section *ms,
struct section *s, 
struct section_map *section_map)
{
    unsigned long i, j, nsect, count;
    struct nlist *object_symbols;
    char *object_strings;
    enum bool start_section;
    struct load_order *load_orders;
    struct fine_reloc *fine_relocs;
    struct merged_symbol *merged_symbol, **hash_pointer;
    struct relocation_info *relocs, reloc;
    struct scattered_relocation_info *sreloc, *spair_reloc;
    unsigned long r_address, r_pcrel, r_length, r_type, pair_r_type, r_extern,
		  r_symbolnum, r_scattered, pair, r_value;
    struct undefined_map *undefined_map;
#ifndef RLD
    enum bool pic;
#endif
    enum bool defined, force_extern_reloc;

	pair_r_type = 0;
	/*
	 * If the size of the section is zero there is nothing to merge.
	 */
	if(s->size == 0)
	    return;

	merged_symbol = NULL;

	/*
	 * Figure out the nsect in the object file we are about to merge.
	 * This is so we can count the symbols that match this nsect which
	 * are defined in this section.
	 */
	nsect = 0;
	for(i = 0; i < cur_obj->nsection_maps; i++){
	    if(&(cur_obj->section_maps[i]) == section_map){
		nsect = i + 1;
		break;
	    }
	}
	if(nsect == 0){
	    fatal("internal error: coalesced_section_merge() called "
		"with bad section_map for section (%.16s,%.16s)",
		s->segname, s->sectname);
	}

	/*
	 * Count the number of symbols in this section in this object file.
	 * Check to see if there is a symbol at the beginning of the section.
	 * and that all symbols defined in this section are global.
	 */
	count = 0;
	start_section = FALSE;
	object_symbols = NULL;
	object_strings = NULL;
	if(cur_obj->symtab != NULL){
	    object_symbols = (struct nlist *)(cur_obj->obj_addr +
					      cur_obj->symtab->symoff);
	    object_strings = (char *)(cur_obj->obj_addr +
				      cur_obj->symtab->stroff);
	    for(i = 0; i < cur_obj->symtab->nsyms; i++){
		if((object_symbols[i].n_type & N_TYPE) == N_SECT &&
		    object_symbols[i].n_sect == nsect &&
		   (object_symbols[i].n_type & N_STAB) == 0){
		    count++;
/*
 * Allow private extern and local symbols.
 */
#ifdef notdef
		    if((object_symbols[i].n_type & (N_EXT|N_PEXT)) ==
		       (N_EXT|N_PEXT)){
			error_with_cur_obj("malformed object (symbol: %s in "
			    "S_COALESCE section (%.16s,%.16s) can't be a "
			    "private extern symbol)",
			    object_strings + object_symbols[i].n_un.n_strx,
			    s->segname, s->sectname);
			return;
		    }
		    if((object_symbols[i].n_type & N_EXT) != N_EXT){
			error_with_cur_obj("malformed object (symbol: %s in "
			    "S_COALESCE section (%.16s,%.16s) not an external "
			    "symbol)",
			    object_strings + object_symbols[i].n_un.n_strx,
			    s->segname, s->sectname);
			return;
		    }
#endif
		    if(object_symbols[i].n_value == s->addr)
			start_section = TRUE;
		}
	    }
	}
	if(start_section == FALSE){
	    error_with_cur_obj("malformed object (section (%.16s,%.16s) no "
		"symbol at start of coalesced section)",s->segname,s->sectname);
	    return;
	}

	/*
	 * Allocate a load order map for the symbols in this section.
	 * We are not ordering the section but simpily needing to figure out
	 * the sizes and offsets of each symbol and a load order map is now
	 * this is normally done.
	 */
	load_orders = allocate(sizeof(struct load_order) * count);
	memset(load_orders, '\0', sizeof(struct load_order) * count);

	/*
	 * Fill in symbol names and values the load order map for this section
	 * in this object file.
	 */
	j = 0;
	for(i = 0; i < cur_obj->symtab->nsyms; i++){
	    if((object_symbols[i].n_type & N_TYPE) == N_SECT &&
	        object_symbols[i].n_sect == nsect &&
	       (object_symbols[i].n_type & N_STAB) == 0){
		load_orders[j].name = object_strings +
			      object_symbols[i].n_un.n_strx;
		load_orders[j].value =
			      object_symbols[i].n_value;
		/*
		 * We fill in the 'order' field with a boolean test of if the
		 * symbol is external or not.  This is a bit of a hack.  See
		 * below where this is used.
		 */
		load_orders[j].order =
			      (object_symbols[i].n_type & N_EXT) == N_EXT;
		j++;
	    }
	}

#ifdef DEBUG
	if(debug & (1 << 14))
	    print_load_order(load_orders, count, ms,
			     cur_obj, "names and values");
#endif DEBUG

	/*
	 * Sort the load order map by symbol value so the
	 * size and input offset fields can be set.
	 */
	qsort(load_orders,
	      count,
	      sizeof(struct load_order),
	      (int (*)(const void *, const void *))qsort_load_order_values);
	/*
	 * Set the input offset and size fields.
	 */
	for(i = 0; i < count - 1; i++){
	    load_orders[i].input_offset =
			   load_orders[i].value -
			   s->addr;
	    load_orders[i].input_size =
			   load_orders[i + 1].value -
			   load_orders[i].value;
	}
	load_orders[i].input_offset =
			   load_orders[i].value -
			   s->addr;
	load_orders[i].input_size =
			   s->addr + s->size -
			   load_orders[i].value;
#ifdef DEBUG
	if(debug & (1 << 15))
	    print_load_order(load_orders, count, ms,
			     cur_obj, "sizes and offsets");
#endif DEBUG

	/*
	 * First deal with the contents of section for each symbol and determine
	 * based on the symbol if the contents will be used from this object or
	 * used from a previously merged object.  This information is encoded
	 * into the fine_reloc structures for each item.
	 */
	fine_relocs = allocate(count * sizeof(struct fine_reloc));
	memset(fine_relocs, '\0', count * sizeof(struct fine_reloc));
	for(i = 0; i < count; i++){
	    fine_relocs[i].input_offset = load_orders[i].input_offset;
	    /*
	     * We previously filled in the 'order' field with a boolean test of
	     * if the symbol is external or not.  This is a bit of a hack.
	     * See above where this is done.
	     */
	    if(load_orders[i].order){
		merged_symbol = *(lookup_symbol(load_orders[i].name));
		if(merged_symbol == NULL)
		    fatal("internal error, coalesced_section_merge() failed in "
			  "looking up external symbol: %s",load_orders[i].name);
		/*
		 * If the merged symbol comes from this object then this
		 * object's contents will be used.  If not it won't.
		 */
		if(merged_symbol->definition_object == cur_obj){
		    fine_relocs[i].use_contents = TRUE;
		    fine_relocs[i].indirect_defined = FALSE;
		    /* align size before using it to assign output offset */
		    ms->s.size = round(ms->s.size, 1 << ms->s.align);
		    fine_relocs[i].output_offset = ms->s.size;
		    ms->s.size += load_orders[i].input_size;
		}
		else{
		    fine_relocs[i].use_contents = FALSE;
		    fine_relocs[i].indirect_defined = FALSE;
		    fine_relocs[i].output_offset = (unsigned long)merged_symbol;
		}
		/*
		 * We want to set local_symbol to TRUE if this symbol is a
		 * private extern symbol so that section difference relocation
		 * entries to it are not flagged as illegal references.
		 */
		if((merged_symbol->nlist.n_type & N_PEXT) == N_PEXT)
		    fine_relocs[i].local_symbol = TRUE;
		else
		    fine_relocs[i].local_symbol = FALSE;
	    }
	    else{
		/*
		 * This is a local or private_extern symbol so keep its
		 * contents.
		 */
		fine_relocs[i].use_contents = TRUE;
		fine_relocs[i].local_symbol = TRUE;
		fine_relocs[i].indirect_defined = FALSE;
		/* align size before using it to assign output offset */
		ms->s.size = round(ms->s.size, 1 << ms->s.align);
		fine_relocs[i].output_offset = ms->s.size;
		ms->s.size += load_orders[i].input_size;
	    }
	}
	section_map->fine_relocs = fine_relocs;
	section_map->nfine_relocs = count;

#ifdef COALESCE_DEBUG
	/*
	 * For debugging print out the fine relocs we created for this section.
	 */
	printf("Fine relocs created for coalesced section (%.16s,%.16s) in ",
	       s->segname, s->sectname);
	print_obj_name(cur_obj);
	printf("\n");
	for(i = 0; i < count; i++){
	    printf("fine_relocs[%lu] load_orders[%lu].name = %s\n",
		   i, i, load_orders[i].name);
	    printf("\tuse_contents = %s\n", fine_relocs[i].use_contents == TRUE
		   ? "TRUE" : "FALSE");
	    printf("\tlocal_symbol = %s\n", fine_relocs[i].local_symbol == TRUE
		   ? "TRUE" : "FALSE");
	    printf("\tinput_offset = %d\n", fine_relocs[i].input_offset);
	    if(fine_relocs[i].use_contents == TRUE)
		printf("\toutput_offset = %ld\n", fine_relocs[i].output_offset);
	    else{
		merged_symbol = (struct merged_symbol *)
				fine_relocs[i].output_offset;
		printf("\t%s from ", merged_symbol->nlist.n_un.n_name);
		print_obj_name(merged_symbol->definition_object);
		printf("\n");
	    }
	}
#endif

	/*
	 * Second deal with the relocation entries for the section in this
	 * object file.  Now that it has been determined for which items the 
	 * contents will be used from this object file.
	 */

	/*
	 * This loop loops through the relocation entries and using the
	 * use_contents field (via a call to fine_reloc_offset_in_output())		 * of the fine_relocs just created determines how many relocation
	 * entries will be in the output for this section of this object file.
	 */
	relocs = (struct relocation_info *)(cur_obj->obj_addr + s->reloff);
	for(i = 0; i < s->nreloc; i++){
	    reloc = relocs[i];
	    if(cur_obj->swapped)
		swap_relocation_info(&reloc, 1, host_byte_sex);
	    /*
	     * Break out the fields of the relocation entry we need here.
	     */
	    if((reloc.r_address & R_SCATTERED) != 0){
		sreloc = (struct scattered_relocation_info *)(&reloc);
		r_scattered = 1;
		r_address = sreloc->r_address;
		r_pcrel = sreloc->r_pcrel;
		r_length = sreloc->r_length;
		r_type = sreloc->r_type;
		r_extern = 0;
		r_value = sreloc->r_value;
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
			    (unsigned int)r_value, i, section_map->s->segname,
			    section_map->s->sectname);
			return;
		    }
		}
	    }
	    else{
		r_scattered = 0;
		r_address = reloc.r_address;
		r_pcrel = reloc.r_pcrel;
		r_length = reloc.r_length;
		r_type = reloc.r_type;
		r_extern = reloc.r_extern;
		r_symbolnum = reloc.r_symbolnum;
	    }
	    /*
	     * Make sure that this is not a stray PAIR relocation entry.
	     */
	    if(r_type == reloc_pair_r_type(arch_flag.cputype)){
		error_with_cur_obj("malformed object (stray relocation PAIR "
		    "entry (%lu) in section (%.16s,%.16s))", i, s->segname,
		    s->sectname);
		continue;
	    }
	    /*
	     * The r_address field is really an offset into the contents of the
	     * section and must reference something inside the section.
	     */
	    if(r_address >= s->size){
		error_with_cur_obj("malformed object (r_address (0x%x) field "
		    "of relocation entry %ld in section (%.16s,%.16s) out of "
		    "range)",(unsigned int)r_address, i,s->segname,s->sectname);
		continue;
	    }
	    /*
	     * If this relocation entry is suppose to have a PAIR make sure it
	     * does.
	     */
	    if(reloc_has_pair(arch_flag.cputype, r_type)){
		if(i + 1 < s->nreloc){
		    reloc = relocs[i + 1];
		    if(cur_obj->swapped)
			swap_relocation_info(&reloc, 1, host_byte_sex);
		    if((reloc.r_address & R_SCATTERED) != 0){
			spair_reloc = (struct scattered_relocation_info *)
                                      &reloc;
                        pair_r_type = spair_reloc->r_type;
		    }
		    else{
                        pair_r_type = reloc.r_type;
		    }
		}
		if(i + 1 >= s->nreloc ||
		   pair_r_type != reloc_pair_r_type(arch_flag.cputype)){
		    error_with_cur_obj("malformed object (relocation entry "
			"(%lu) in section (%.16s,%.16s) missing following "
			"associated PAIR entry)", i, s->segname, s->sectname);
                    continue;
                }
	    }

	    /*
	     * Assumed the symbol for this relocation entry is defined (always
	     * true for local relocation entries).  Then reset the variable
	     * "defined" correctly if this is an external relocation entry based
	     * on if the symbol is defined, where it is defined and the output
	     * file type.
	     */
	    defined = TRUE;
	    force_extern_reloc = FALSE;
	    if(output_for_dyld && r_extern){
		/*
		 * This is an external relocation entry.  So the value to be
		 * added to the item to be relocated is the value of the symbol.
		 * r_symbolnum is an index into the input file's symbol table
		 * of the symbol being refered to.  The symbol must be an
		 * undefined symbol to be used in an external relocation entry
		 * or a global coalesced symbol.
		 */
		if(r_symbolnum >= cur_obj->symtab->nsyms){
		    error_with_cur_obj("r_symbolnum (%lu) field of external "
			"relocation entry %lu in section (%.16s,%.16s) out of "
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
		    if((object_symbols[r_symbolnum].n_type & N_EXT) != N_EXT){
			error_with_cur_obj("r_symbolnum (%lu) field of external"
			    " relocation entry %lu in section (%.16s,%.16s) "
			    "refers to a non-external symbol", r_symbolnum, i,
			    section_map->s->segname, section_map->s->sectname);
			return;
		    }
		    /*
		     * We must allow and create references to defined global
		     * coalesced symbols with external relocation entries so
		     * that the dynamic linker can relocate all references to
		     * the same symbol.
		     */
		    if((object_symbols[r_symbolnum].n_type & N_TYPE) ==
			N_SECT &&
		       (cur_obj->section_maps[object_symbols[r_symbolnum].
			n_sect-1].s->flags & SECTION_TYPE) == S_COALESCED){
			hash_pointer = lookup_symbol(object_strings +
				     object_symbols[r_symbolnum].n_un.n_strx);
			if(hash_pointer == NULL){
			    fatal("internal error, in coalesced_section_merge()"
				  " failed to lookup coalesced symbol %s",
				  object_strings +
				  object_symbols[r_symbolnum].n_un.n_strx);
			}
			merged_symbol = *hash_pointer;
			if(((merged_symbol->nlist.n_type & N_PEXT) == N_PEXT &&
			    keep_private_externs == FALSE) ||
			    dynamic == FALSE ||
			   (output_for_dyld && has_dynamic_linker_command))
			    force_extern_reloc = FALSE;
			else
			    force_extern_reloc = TRUE;
		    }
		    else{
			if(object_symbols[r_symbolnum].n_type !=
			   (N_EXT | N_UNDF)){
			    error_with_cur_obj("r_symbolnum (%lu) field of "
				"external relocation entry %lu in section "
				"(%.16s,%.16s) refers to a non-undefined "
				"symbol", r_symbolnum, i,
				section_map->s->segname,
				section_map->s->sectname);
			    return;
			}
			print_obj_name(cur_obj);
			fatal("internal error, in coalesced_section_merge() "
			    "symbol index %lu in above file not in undefined "
			    "map", r_symbolnum);
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
		 * Is the merged symbol for this external relocation entry
		 * defined in the output file.
		 */
		if(merged_symbol->nlist.n_type == (N_EXT | N_UNDF) ||
		   merged_symbol->nlist.n_type == (N_EXT | N_PBUD) ||
		   (merged_symbol->nlist.n_type == (N_EXT | N_INDR) &&
		    merged_symbol->defined_in_dylib == TRUE))
		    defined = FALSE;
		else
		    defined = TRUE;
	    }
	    if(reloc_has_pair(arch_flag.cputype, r_type))
		pair = 1;
	    else
		pair = 0;
#ifndef RLD
	    /*
	     * If saving relocation entries see if this relocation entry is for 
	     * an item that is going to be in the output file and if so count it
	     * as one of the output relocation entries.
	     */
	    if(output_for_dyld &&
	       fine_reloc_offset_in_output(section_map, r_address)){
		/*
		 * Mark this section as being relocated (staticly).
		 */
		ms->relocated = TRUE;
		if(r_extern == 0)
		    pic = (enum bool)
			  (reloc_is_sectdiff(arch_flag.cputype, r_type) ||
			   (r_pcrel == 1 && r_symbolnum != NO_SECT));
		else
		    pic = (enum bool)
			  (r_pcrel == 1 &&
			   (merged_symbol->nlist.n_type & N_TYPE) == N_SECT);
		/*
		 * The number of relocation entries in the output file is based
		 * on one of three different cases:
		 *  The output file is a dynamic shared library file
		 *  The output file has a dynamic linker load command
		 *  The output does not have a dynamic linker load command
		 */
		if(filetype == MH_DYLIB){
		    /*
		     * For dynamic shared library files the modules are kept
		     * separate so external relocation entries on input will be 
		     * external relocation entries on output.  For local
		     * relocation entries only non-position-independent local
		     * relocation entries are kept.  Modules of dylibs are not
		     * linked together and can only be slid keeping all sections
		     * relative to each other the same.
		     */
		    if(r_extern && (merged_symbol->nlist.n_type & N_PEXT) == 0)
			section_map->nextrel += 1 + pair;
		    else if(pic == FALSE)
			section_map->nlocrel += 1 + pair;
		}
		else if(has_dynamic_linker_command){
		    /*
		     * For an file with a dynamic linker load command only
		     * external relocation entries for undefined symbols are
		     * kept.  This output file is a fixed address and can't be
		     * moved.
		     */
		    if(r_extern && defined == FALSE)
			section_map->nextrel += 1 + pair;
		}
		else{
		    /*
		     * For an file without a dynamic linker load command
		     * external relocation entries for undefined symbols are
		     * kept and locals that are non-position-independent are
		     * kept.  This file can only be slid keeping all sections
		     * relative to each other the same.
		     */
		    if(r_extern && (merged_symbol->nlist.n_type & N_PEXT) == 0){
			if(defined == FALSE || force_extern_reloc == TRUE)
			    section_map->nextrel += 1 + pair;
			else if(pic == FALSE)
			    section_map->nlocrel += 1 + pair;
		    }
		    else if(pic == FALSE)
			section_map->nlocrel += 1 + pair;
		}
	    }
	    else if(save_reloc &&
	            fine_reloc_offset_in_output(section_map, r_address)){
		ms->s.nreloc += 1 + pair;
		nreloc += 1 + pair;
	    }
#endif !defined(RLD)
	    i += pair;
	}
	/*
	 * If the the number of relocation entries is not zero mark this section
	 * as being relocated (staticly).
	 */
	if(ms->s.nreloc != 0)
	    ms->relocated = TRUE;

	/*
	 * Third deal with the symbol table entries for local symbols and N_STAB
	 * symbols in this section in this object file.  Now that it has been
	 * determined for which items the contents will be used from this
	 * object file.  
	 */
	discard_local_symbols_for_section(nsect, object_symbols, object_strings,
    					  s, section_map);

	/* Should the load_order be set into the section_map or free()'ed? */
	free(load_orders);
}

__private_extern__
void
coalesced_section_order(
void *data,
struct merged_section *ms)
{
#ifndef RLD
    kern_return_t r;
#ifdef __MWERKS__
    struct coalesced_section_data *dummy1;
    struct merged_section *dummy2;
        dummy1 = data;
        dummy2 = ms;
#endif

	warning("section ordering for coalesced sections not supported ("
		"-sectorder %s %s %s ignored)", ms->s.segname, ms->s.sectname,
		ms->order_filename);
	/*
	 * Deallocate the memory for the load order file now that it is
	 * nolonger needed.
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
    struct coalesced_section_data *dummy1;
    struct merged_section *dummy2;
        dummy1 = data;
        dummy2 = ms;
#endif
#endif /* RLD */
}
