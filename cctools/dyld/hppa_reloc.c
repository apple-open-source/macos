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
#ifdef hppa
/*
 * The local and external hppa relocation routines.  The cache sync'ing strategy
 * is to flush each entry as their is a user level cache sync instruction
 * sequence.  But for now the vm_flush_cache() is know to be a hack which
 * flushes the entire cache.
 */
#import <stdlib.h>
#import <limits.h>
#import <mach/mach.h>
#import "stuff/openstep_mach.h"
#import <mach-o/nlist.h>
#import <mach-o/reloc.h>
#import <mach-o/hppa/reloc.h>

#import "stuff/hppa.h"
#import "stuff/vm_flush_cache.h"

#import "images.h"
#import "symbols.h"
#import "errors.h"
#import "reloc.h"

#define U_ABS(l) (((long)(l))<0 ? (unsigned long)(-(l)) : (l))

/*
 * local_relocation() preforms all the local relocations in the specified image.
 * This is only done once at the time the image is loaded into memory and only
 * if the image can't be loaded at the addresses it wants in the file.
 */
void
local_relocation(
struct image *image)
{
    unsigned long i, r_slide, r_address, r_pcrel, r_length, r_value, value;
    unsigned long instruction, immediate;
    enum bool no_sect;
    struct relocation_info *relocs;
    struct scattered_relocation_info *sreloc;
    unsigned long cache_flush_high_addr, cache_flush_low_addr;
    struct relocation_info *pair_reloc;
    struct scattered_relocation_info *spair_reloc;
    enum reloc_type_hppa r_type, pair_r_type;
    unsigned long other_half;
    unsigned long hi21, lo14;
    unsigned long w, w1, w2;

	relocs = (struct relocation_info *)
	    (image->vmaddr_slide +
	     image->linkedit_segment->vmaddr +
	     image->dyst->locreloff -
	     image->linkedit_segment->fileoff);

	other_half = 0;
	cache_flush_high_addr = 0;
	cache_flush_low_addr = ULONG_MAX;
	if(image->mh->flags & MH_SPLIT_SEGS)
	    r_slide = image->segs_read_write_addr + image->vmaddr_slide;
	else
	    r_slide = image->seg1addr + image->vmaddr_slide;
	r_value = 0;

	for(i = 0; i < image->dyst->nlocrel; i++){
	    if((relocs[i].r_address & R_SCATTERED) != 0){
		sreloc = (struct scattered_relocation_info *)(relocs + i);
		r_address = sreloc->r_address;
		r_pcrel = sreloc->r_pcrel;
		r_length = sreloc->r_length;
		r_value = sreloc->r_value;
		r_type = (enum reloc_type_hppa)sreloc->r_type;
		no_sect = FALSE;
	    }
	    else{
		r_address = relocs[i].r_address;
		r_pcrel = relocs[i].r_pcrel;
		r_length = relocs[i].r_length;
		r_type = (enum reloc_type_hppa)relocs[i].r_type;
		no_sect = relocs[i].r_symbolnum == NO_SECT;
	    }
	    /*
	     * If this relocation type has a pair break out it's fields.
	     */
	    pair_r_type = 0;
	    if(r_type == HPPA_RELOC_HI21 ||
	       r_type == HPPA_RELOC_LO14 ||
	       r_type == HPPA_RELOC_BR17){
		pair_reloc = relocs + i + 1;
		if((pair_reloc->r_address & R_SCATTERED) != 0){
		    spair_reloc = (struct scattered_relocation_info *)
				  pair_reloc;
		    pair_r_type = spair_reloc->r_type;
		    other_half  = spair_reloc->r_address;
		}
		else{
		    pair_r_type = pair_reloc->r_type;
		    other_half  = pair_reloc->r_address;
		}
		i++;
	    }

	    /*
	     * If this relocation entry pc relative, which means the value of
	     * the pc will get added to it when it is executed, the item being
	     * relocated has the value of the pc subtracted from it.  So to
	     * relocate this, the amount the image has been slid has to be
	     * subtracted from it also.
	     */
	    value = 0;
	    if(r_pcrel)
		value -= image->vmaddr_slide;
	    /*
	     * Since this is a local relocation entry and all sections are
	     * moving by the same amount everything gets moved except those
	     * things that are defined that are not in a section.  We are
	     * counting on not seeing any section difference relocation entries
	     * and pcrel section based (which would work but be nops).
	     */
	    if(no_sect == FALSE)
		value += image->vmaddr_slide;
	    
	    if(r_type == HPPA_RELOC_VANILLA){
		switch(r_length){
		case 0: /* byte */
		    value += *((char *)(r_address + r_slide));
		    if( (value & 0xffffff00) &&
		       ((value & 0xffffff80) != 0xffffff80)){
			error("relocation overflow (local relocation in %s "
			    "relocation entry %lu does not fit in 1 byte)",
			    image->name, i);
			link_edit_error(DYLD_OTHER_ERROR, DYLD_RELOCATION,
					image->name);
		    }
		    *((char *)(r_address + r_slide)) = value;
		    break;
		case 1: /* word (2 byte) */
		    value += *((short *)(r_address + r_slide));
		    if( (value & 0xffff0000) &&
		       ((value & 0xffff8000) != 0xffff8000)){
			error("relocation overflow (local relocation in %s "
			    "relocation entry %lu does not fit in 2 bytes)",
			    image->name, i);
			link_edit_error(DYLD_OTHER_ERROR, DYLD_RELOCATION,
					image->name);
		    }
		    *((short *)(r_address + r_slide)) = value;
		    break;
		case 2: /* long (4 byte) */
		    value += *((long *)(r_address + r_slide));
		    *((long *)(r_address + r_slide)) = value;
		    break;
		}
	    }
	    /*
	     * Do hppa specific relocation based on the r_type.
	     */
	    else{
		switch(r_type){
		case HPPA_RELOC_PB_LA_PTR:
		    value = r_value + image->vmaddr_slide;
		    *((long *)(r_address + r_slide)) = value;
		    break;
		case HPPA_RELOC_HI21:
		    instruction = *((long *)(r_address + r_slide));
		    immediate = sign_ext(other_half, 14) + 
		               (assemble_21(instruction & 0x1fffff) << 11);
		    calc_hppa_HILO(value + immediate, 0, &hi21, &lo14);
		    instruction = (instruction & 0xffe00000) |
				  dis_assemble_21(hi21 >> 11);
		    *((long *)(r_address + r_slide)) = instruction;
		    break;
		case HPPA_RELOC_LO14:
		    instruction = *((long *)(r_address + r_slide));
		    immediate = low_sign_ext(instruction & 0x3fff, 14) +
		    	        (other_half << 11);
		    calc_hppa_HILO(value + immediate, 0, &hi21, &lo14);
		    lo14 = low_sign_unext(lo14, 14);
		    instruction = (instruction & 0xffffc000) |
				  (lo14 & 0x3fff);
		    *((long *)(r_address + r_slide)) = instruction;
		    break;
		case HPPA_RELOC_BR17:
		    instruction = *((long *)(r_address + r_slide));
		    immediate = assemble_17((instruction & 0x1f0000) >> 16,
			                    (instruction & 0x1ffc) >> 2,
				             instruction & 1);
		    immediate = (sign_ext(immediate, 17) << 2) +
				(other_half << 11);
		    calc_hppa_HILO(value + immediate, 0, &hi21, &lo14);
		    lo14 >>= 2;
		    dis_assemble_17(lo14, &w1, &w2, &w);
		    instruction = (instruction & 0xffe0e002) |
				  (w1 << 16) | (w2 << 2) | w;
		    *((long *)(r_address + r_slide)) = instruction;
		    break;
		case HPPA_RELOC_BL17:
		    instruction = *((long *)(r_address + r_slide));
		    immediate = assemble_17((instruction & 0x1f0000) >> 16,
			                    (instruction & 0x1ffc) >> 2,
				             instruction & 1);
		    if((immediate & 0x10000) != 0)
			immediate |= 0xfffe0000;
		    immediate <<= 2;
		    immediate += value;
		    if(U_ABS(immediate) > 0x3ffff){
			error("relocation overflow (local relocation in %s "
			    "relocation entry %lu displacement too large)",
			    image->name, i);
			link_edit_error(DYLD_OTHER_ERROR, DYLD_RELOCATION,
					image->name);
		    }
		    immediate >>= 2;
		    dis_assemble_17(immediate, &w1, &w2, &w);
		    instruction = (instruction & 0xffe0e002) |
				  (w1 << 16) | (w2 << 2) | w;
		    *((long *)(r_address + r_slide)) = instruction;
		    break;
		default:
		    break;
		}
	    }

	    if(image->cache_sync_on_reloc){
		if(r_address + r_slide < cache_flush_low_addr)
		    cache_flush_low_addr = r_address + r_slide;
		if(r_address + r_slide + (1 << r_length) >
		   cache_flush_high_addr)
		    cache_flush_high_addr = r_address + r_slide +
					    (1 << r_length);
	    }
	}

	if(image->cache_sync_on_reloc &&
	   cache_flush_high_addr > cache_flush_low_addr)
	    vm_flush_cache(mach_task_self(), cache_flush_low_addr,
			   cache_flush_high_addr - cache_flush_low_addr);
}

/*
 * external_relocation() preforms the external relocations in the specified 
 * image.
 */
enum link_state
external_relocation(
struct image *image,
struct relocation_info *relocs,
unsigned long nrelocs,
struct nlist *symbols,
char *strings,
char *library_name,
char *module_name)
{
    unsigned long i, value, r_slide;
    enum link_state link_state;
    char *symbol_name;
    struct nlist *defined_symbol;
    module_state *defined_module;
    struct image *defined_image;
    struct library_image *defined_library_image;
    unsigned long cache_flush_high_addr, cache_flush_low_addr;
    unsigned long instruction, immediate;
    unsigned long other_half;
    unsigned long hi21, lo14;
    unsigned long w, w1, w2;

	link_state = RELOCATED;
	cache_flush_high_addr = 0;
	cache_flush_low_addr = ULONG_MAX;
	if(image->mh->flags & MH_SPLIT_SEGS)
	    r_slide = image->segs_read_write_addr + image->vmaddr_slide;
	else
	    r_slide = image->seg1addr + image->vmaddr_slide;
	for(i = 0; i < nrelocs; i++){
	    /*
	     * look up the symbol being referenced by this relocation entry to
	     * get the defined symbol's value to be used.  If this is a private
	     * image and the symbol is coalesced then use the coalesced symbol
	     * from this image.
	     *
	     * nmedit(1) run on a dynamic library does not change the relocation
	     * entries of symbols it turns into private_externs into local
	     * relocation entries as ld(1) does.  So what we end up with is an
	     * external relocation entry with a symbol that is not external but
	     * was a private_extern.  So in this case also use the symbol in the
	     * image.
	     */
	    symbol_name = strings + symbols[relocs[i].r_symbolnum].n_un.n_strx;
	    if((image->private == TRUE &&
	        image->has_coalesced_sections == TRUE &&
	        is_symbol_coalesced(image,
				    symbols + relocs[i].r_symbolnum) == TRUE) ||
		((symbols[relocs[i].r_symbolnum].n_type & N_EXT) != N_EXT &&
		 (symbols[relocs[i].r_symbolnum].n_type & N_PEXT) == N_PEXT) ){
		defined_symbol = symbols + relocs[i].r_symbolnum;
		defined_module = NULL; /* not needed here */
		defined_image = image;
		defined_library_image = NULL; /* not needed here */
		
	    }
	    else{
		lookup_symbol(symbol_name,
			      get_primary_image(image, symbols +
						       relocs[i].r_symbolnum),
			      get_hint(image, symbols + relocs[i].r_symbolnum),
			      get_weak(symbols + relocs[i].r_symbolnum),
			      &defined_symbol, &defined_module,
			      &defined_image, &defined_library_image, NULL);
	    }

	    /* 
	     * If this symbol is not defined undo the relocation that has been
	     * done for this module upto this point and leave the link state
	     * being linked.  This is done so that if the symbol is later
	     * defined things will not get relocate more than once.
	     */
	    if(defined_symbol == NULL){
		undo_external_relocation(
		    FALSE, /* undo_prebinding */
		    image,
		    relocs,
		    i, /* nrelocs */
		    symbols,
		    strings,
		    library_name,
		    module_name);
		link_state = BEING_LINKED;
		goto done;
	    }

	    value = defined_symbol->n_value;
	    if((defined_symbol->n_type & N_TYPE) != N_ABS)
		value += defined_image->vmaddr_slide;
	    /*
	     * If this relocation entry pc relative, which means the value of
	     * the pc will get added to it when it is executed, the item being
	     * relocated has the value of the pc subtracted from it.  So to
	     * relocate this, the amount the image has been slid has to be
	     * subtracted from it also.
	     */
	    if(relocs[i].r_pcrel)
		value -= image->vmaddr_slide;

	    if(relocs[i].r_type == HPPA_RELOC_VANILLA){
		switch(relocs[i].r_length){
		case 0: /* byte */
		    value += *((char *)(relocs[i].r_address + r_slide));
		    if( (value & 0xffffff00) &&
		       ((value & 0xffffff80) != 0xffffff80)){
			set_error_string("dyld: relocation overflow (external "
			    "relocation for symbol %s in ", symbol_name);
			if(library_name != NULL)
			    add_error_string("%s(%s) ", library_name,
				module_name);
			else
			    add_error_string("%s ", module_name);
			add_error_string("relocation entry %lu does not fit in "
			    "1 byte", i);
			link_edit_error(DYLD_OTHER_ERROR, DYLD_RELOCATION,
					image->name);
		    }
		    *((char *)(relocs[i].r_address + r_slide)) = value;
		    break;
		case 1: /* word (2 byte) */
		    value += *((short *)(relocs[i].r_address + r_slide));
		    if( (value & 0xffff0000) &&
		       ((value & 0xffff8000) != 0xffff8000)){
			set_error_string("dyld: relocation overflow (external "
			    "relocation for symbol %s in ", symbol_name);
			if(library_name != NULL)
			    add_error_string("%s(%s) ", library_name,
				module_name);
			else
			    add_error_string("%s ", module_name);
			add_error_string("relocation entry %lu does not fit in "
			    "2 bytes", i);
			link_edit_error(DYLD_OTHER_ERROR, DYLD_RELOCATION,
					image->name);
		    }
		    *((short *)(relocs[i].r_address + r_slide)) = value;
		    break;
		case 2: /* long (4 byte) */
		    value += *((long *)(relocs[i].r_address + r_slide));
		    *((long *)(relocs[i].r_address + r_slide)) = value;
		    break;
		}
	    }
	    /*
	     * Do hppa specific relocation based on the r_type.
	     */
	    else{
		instruction = *((long *)(relocs[i].r_address + r_slide));
		switch(relocs[i].r_type){
		case HPPA_RELOC_HI21:
		    other_half  = relocs[i + 1].r_address;
		    immediate = sign_ext(other_half, 14) + 
		               (assemble_21(instruction & 0x1fffff) << 11);
		    calc_hppa_HILO(value + immediate, 0, &hi21, &lo14);
		    instruction = (instruction & 0xffe00000) |
				  dis_assemble_21(hi21 >> 11);
		    break;
		case HPPA_RELOC_LO14:
		    other_half  = relocs[i + 1].r_address;
		    immediate = low_sign_ext(instruction & 0x3fff, 14) +
		    	        (other_half << 11);
		    calc_hppa_HILO(value + immediate, 0, &hi21, &lo14);
		    lo14 = low_sign_unext(lo14, 14);
		    instruction = (instruction & 0xffffc000) |
				  (lo14 & 0x3fff);
		    break;
		case HPPA_RELOC_BR17:
		    other_half  = relocs[i + 1].r_address;
		    immediate = assemble_17((instruction & 0x1f0000) >> 16,
			                    (instruction & 0x1ffc) >> 2,
				             instruction & 1);
		    immediate = (sign_ext(immediate, 17) << 2) +
				(other_half << 11);
		    calc_hppa_HILO(value + immediate, 0, &hi21, &lo14);
		    lo14 >>= 2;
		    dis_assemble_17(lo14, &w1, &w2, &w);
		    instruction = (instruction & 0xffe0e002) |
				  (w1 << 16) | (w2 << 2) | w;
		    break;
		case HPPA_RELOC_BL17:
		    immediate = assemble_17((instruction & 0x1f0000) >> 16,
			                    (instruction & 0x1ffc) >> 2,
				             instruction & 1);
		    if((immediate & 0x10000) != 0)
			immediate |= 0xfffe0000;
		    immediate <<= 2;
		    immediate += value;
		    if(U_ABS(immediate) > 0x3ffff){
			error("relocation overflow (local relocation in %s "
			    "relocation entry %lu displacement too large)",
			    image->name, i);
			link_edit_error(DYLD_OTHER_ERROR, DYLD_RELOCATION,
					image->name);
		    }
		    immediate >>= 2;
		    dis_assemble_17(immediate, &w1, &w2, &w);
		    instruction = (instruction & 0xffe0e002) |
				  (w1 << 16) | (w2 << 2) | w;
		    break;
		default:
		    break;
		}
		*((long *)(relocs[i].r_address + r_slide)) = instruction;
	    }
	    if(image->cache_sync_on_reloc){
		if(relocs[i].r_address + r_slide < cache_flush_low_addr)
		    cache_flush_low_addr = relocs[i].r_address + r_slide;
		if(relocs[i].r_address + r_slide + (1 << relocs[i].r_length) >
		   cache_flush_high_addr)
		    cache_flush_high_addr = relocs[i].r_address + r_slide +
					    (1 << relocs[i].r_length);
	    }
	    /*
	     * If the relocation entry had a pair step over it.
	     */
	    if(relocs[i].r_type == HPPA_RELOC_HI21 ||
	       relocs[i].r_type == HPPA_RELOC_LO14 ||
	       relocs[i].r_type == HPPA_RELOC_BR17)
		i++;
	}
done:
	if(image->cache_sync_on_reloc &&
	   cache_flush_high_addr > cache_flush_low_addr)
	    vm_flush_cache(mach_task_self(), cache_flush_low_addr,
			   cache_flush_high_addr - cache_flush_low_addr);

	return(link_state);
}

/*
 * undo_external_relocation() undoes the external relocations in the specified 
 * image.  This is done based on if undo_prebinding is TRUE, to undo the
 * external of a prebound image or, if undo_prebinding is FALSE, to undo part
 * of the external relocation that external_relocation() did before running into
 * a non-existant symbol.
 */
void
undo_external_relocation(
enum bool undo_prebinding,
struct image *image,
struct relocation_info *relocs,
unsigned long nrelocs,
struct nlist *symbols,
char *strings,
char *library_name,
char *module_name)
{
    unsigned long i, value, r_slide, contents;
    char *symbol_name;
    struct nlist *defined_symbol;
    module_state *defined_module;
    struct image *defined_image;
    struct library_image *defined_library_image;
    unsigned long cache_flush_high_addr, cache_flush_low_addr;
    unsigned long instruction, immediate;
    unsigned long other_half;
    unsigned long hi21, lo14;
    unsigned long w, w1, w2;
    enum bool relocs_writable;
    kern_return_t r;

	cache_flush_high_addr = 0;
	cache_flush_low_addr = ULONG_MAX;
	if(image->mh->flags & MH_SPLIT_SEGS)
	    r_slide = image->segs_read_write_addr + image->vmaddr_slide;
	else
	    r_slide = image->seg1addr + image->vmaddr_slide;
	relocs_writable = FALSE;
	for(i = 0; i < nrelocs; i++){
	    /*
	     * look up the symbol being referenced by this relocation entry to
	     * get the defined symbol's value to be used.  If we are undoing
	     * the relocation done by prebinding use the value from the image
	     * as it was staticly linked otherwise use the value as it is being
	     * used by the dynamic linker.
	     */
	    symbol_name = strings + symbols[relocs[i].r_symbolnum].n_un.n_strx;
	    if(undo_prebinding == TRUE){
		value = symbols[relocs[i].r_symbolnum].n_value;
	    }
	    else{
		/*
	         * Get the defined symbol's value to be used.  If this is a
		 * private image and the symbol is coalesced then use the
		 * coalesced symbol from this image.  If not look it up noting
		 * that the lookup_symbol() can't fail as it just suceeded in
		 * the routine external_relocation() that called us.
		 */
		if(image->private == TRUE &&
		   image->has_coalesced_sections == TRUE &&
		   is_symbol_coalesced(image,
				       symbols+relocs[i].r_symbolnum) == TRUE){
		    defined_symbol = symbols + relocs[i].r_symbolnum;
		    defined_module = NULL; /* not needed here */
		    defined_image = image;
		    defined_library_image = NULL; /* not needed here */
		    
		}
		else{
		    lookup_symbol(symbol_name,
				  get_primary_image(image, symbols +
						       relocs[i].r_symbolnum),
				  get_hint(image, symbols +
						       relocs[i].r_symbolnum),
				  get_weak(symbols + relocs[i].r_symbolnum),
				  &defined_symbol, &defined_module,
				  &defined_image, &defined_library_image, NULL);
		}

		/*
		 * Get the value of the symbol.  If the image has been slid and
		 * this is not an absolute symbol the symbol's value must
		 * include the amount the image it is defined is was slid.
		 */
		value = defined_symbol->n_value;
		if((defined_symbol->n_type & N_TYPE) != N_ABS)
		    value += defined_image->vmaddr_slide;

		/*
		 * If this relocation entry is pc relative, which means the
		 * value of the pc will get added to it when it is executed,
		 * the item being relocated has the value of the pc subtracted
		 * from it.  So to relocate this, the amount the image has been
		 * slid has to be subtracted from it also.
		 */
		if(relocs[i].r_pcrel)
		    value -= image->vmaddr_slide;
	    }

	    if(relocs[i].r_type == HPPA_RELOC_VANILLA){
		switch(relocs[i].r_length){
		case 0: /* byte */
		    contents = *((char *)(relocs[i].r_address + r_slide));
		    contents -= value;
		    if( (contents & 0xffffff00) &&
		       ((contents & 0xffffff80) != 0xffffff80)){
			set_error_string("dyld: relocation overflow (external "
			    "relocation for symbol %s in ", symbol_name);
			if(library_name != NULL)
			    add_error_string("%s(%s) ", library_name,
				module_name);
			else
			    add_error_string("%s ", module_name);
			add_error_string("relocation entry %lu does not fit in "
			    "1 byte", i);
			link_edit_error(DYLD_OTHER_ERROR, DYLD_RELOCATION,
					image->name);
		    }
		    *((char *)(relocs[i].r_address + r_slide)) = contents;
		    break;
		case 1: /* word (2 byte) */
		    contents = *((short *)(relocs[i].r_address + r_slide));
		    contents -= value;
		    if( (contents & 0xffff0000) &&
		       ((contents & 0xffff8000) != 0xffff8000)){
			set_error_string("dyld: relocation overflow (external "
			    "relocation for symbol %s in ", symbol_name);
			if(library_name != NULL)
			    add_error_string("%s(%s) ", library_name,
				module_name);
			else
			    add_error_string("%s ", module_name);
			add_error_string("relocation entry %lu does not fit in "
			    "2 bytes", i);
			link_edit_error(DYLD_OTHER_ERROR, DYLD_RELOCATION,
					image->name);
		    }
		    *((short *)(relocs[i].r_address + r_slide)) = contents;
		    break;
		case 2: /* long (4 byte) */
		    contents = *((long *)(relocs[i].r_address + r_slide));
		    contents -= value;
		    *((long *)(relocs[i].r_address + r_slide)) = contents;
		    break;
		}
	    }
	    /*
	     * Do hppa specific relocation based on the r_type.
	     */
	    else{
		if(relocs_writable == FALSE){
		    /* make sure the linkedit segment for is writable */
		    if((r = vm_protect(mach_task_self(),
			image->linkedit_segment->vmaddr + image->vmaddr_slide,
			(vm_size_t)image->linkedit_segment->vmsize, FALSE,
			VM_PROT_WRITE | VM_PROT_READ)) != KERN_SUCCESS){
			mach_error(r, "can't set vm_protection on segment: "
			    "%.16s for: %s", image->linkedit_segment->segname,
			    image->name);
			link_edit_error(DYLD_MACH_RESOURCE, r, image->name);
		    }
		    relocs_writable = TRUE;
		}
		instruction = *((long *)(relocs[i].r_address + r_slide));
		switch(relocs[i].r_type){
		case HPPA_RELOC_HI21:
		    other_half  = relocs[i + 1].r_address;
		    immediate = sign_ext(other_half, 14) + 
		               (assemble_21(instruction & 0x1fffff) << 11);
		    calc_hppa_HILO(-value + immediate, 0, &hi21, &lo14);
		    instruction = (instruction & 0xffe00000) |
				  dis_assemble_21(hi21 >> 11);
		    relocs[i + 1].r_address = lo14 & 0x3fff;
		    break;
		case HPPA_RELOC_LO14:
		    other_half  = relocs[i + 1].r_address;
		    immediate = low_sign_ext(instruction & 0x3fff, 14) +
		    	        (other_half << 11);
		    calc_hppa_HILO(-value + immediate, 0, &hi21, &lo14);
		    lo14 = low_sign_unext(lo14, 14);
		    instruction = (instruction & 0xffffc000) |
				  (lo14 & 0x3fff);
		    relocs[i + 1].r_address = hi21 >> 11;
		    break;
		case HPPA_RELOC_BR17:
		    other_half  = relocs[i + 1].r_address;
		    immediate = assemble_17((instruction & 0x1f0000) >> 16,
			                    (instruction & 0x1ffc) >> 2,
				             instruction & 1);
		    immediate = (sign_ext(immediate, 17) << 2) +
				(other_half << 11);
		    calc_hppa_HILO(-value + immediate, 0, &hi21, &lo14);
		    lo14 >>= 2;
		    dis_assemble_17(lo14, &w1, &w2, &w);
		    instruction = (instruction & 0xffe0e002) |
				  (w1 << 16) | (w2 << 2) | w;
		    relocs[i + 1].r_address = hi21 >> 11;
		    break;
		case HPPA_RELOC_BL17:
		    immediate = assemble_17((instruction & 0x1f0000) >> 16,
			                    (instruction & 0x1ffc) >> 2,
				             instruction & 1);
		    if((immediate & 0x10000) != 0)
			immediate |= 0xfffe0000;
		    immediate <<= 2;
		    immediate -= value;
		    if(U_ABS(immediate) > 0x3ffff){
			error("relocation overflow (local relocation in %s "
			    "relocation entry %lu displacement too large)",
			    image->name, i);
			link_edit_error(DYLD_OTHER_ERROR, DYLD_RELOCATION,
					image->name);
		    }
		    immediate >>= 2;
		    dis_assemble_17(immediate, &w1, &w2, &w);
		    instruction = (instruction & 0xffe0e002) |
				  (w1 << 16) | (w2 << 2) | w;
		    break;
		default:
		    break;
		}
		*((long *)(relocs[i].r_address + r_slide)) = instruction;
	    }
	    if(image->cache_sync_on_reloc){
		if(relocs[i].r_address + r_slide < cache_flush_low_addr)
		    cache_flush_low_addr = relocs[i].r_address + r_slide;
		if(relocs[i].r_address + r_slide + (1 << relocs[i].r_length) >
		   cache_flush_high_addr)
		    cache_flush_high_addr = relocs[i].r_address + r_slide +
					    (1 << relocs[i].r_length);
	    }
	    /*
	     * If the relocation entry had a pair step over it.
	     */
	    if(relocs[i].r_type == HPPA_RELOC_HI21 ||
	       relocs[i].r_type == HPPA_RELOC_LO14 ||
	       relocs[i].r_type == HPPA_RELOC_BR17)
		i++;
	}

	if(relocs_writable == TRUE){
	    if((r = vm_protect(mach_task_self(),
		image->linkedit_segment->vmaddr + image->vmaddr_slide,
		(vm_size_t)image->linkedit_segment->vmsize,
		 FALSE, image->linkedit_segment->initprot)) != KERN_SUCCESS){
		mach_error(r, "can't set vm_protection on segment: %.16s "
		    "for: %s", image->linkedit_segment->segname, image->name);
		link_edit_error(DYLD_MACH_RESOURCE, r, image->name);
	    }
	}

	if(image->cache_sync_on_reloc &&
	   cache_flush_high_addr > cache_flush_low_addr)
	    vm_flush_cache(mach_task_self(), cache_flush_low_addr,
			   cache_flush_high_addr - cache_flush_low_addr);
}
#endif /* hppa */
