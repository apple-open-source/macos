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
#ifdef __ppc__
/*
 * The local and external ppc relocation routines.  The cache sync'ing strategy
 * is to flush each entry as there is a user level cache sync instruction
 * sequence.  This is done by calling the assembly routine ppc_cache_flush()
 * which uses the "icbi" instruction.
 */
extern void ppc_cache_flush(
    unsigned long double_word_address,
    unsigned long size);

#import <stdlib.h>
#import <limits.h>
#import <mach/mach.h>
#import "stuff/openstep_mach.h"
#import <mach-o/nlist.h>
#import <mach-o/reloc.h>
#import <mach-o/ppc/reloc.h>

#import "images.h"
#import "symbols.h"
#import "errors.h"
#import "reloc.h"

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
    struct relocation_info *pair_reloc;
    struct scattered_relocation_info *spair_reloc;
    enum reloc_type_ppc r_type, pair_r_type;
    unsigned long other_half;
    unsigned long cache_flush_high_addr, cache_flush_low_addr;

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
		r_type = (enum reloc_type_ppc)sreloc->r_type;
		no_sect = FALSE;
	    }
	    else{
		r_address = relocs[i].r_address;
		r_pcrel = relocs[i].r_pcrel;
		r_length = relocs[i].r_length;
		r_type = (enum reloc_type_ppc)relocs[i].r_type;
		no_sect = relocs[i].r_symbolnum == NO_SECT;
	    }
	    /*
	     * If this relocation type has a pair break out it's fields.
	     */
	    pair_r_type = 0;
	    if(r_type == PPC_RELOC_HI16 || r_type == PPC_RELOC_LO16 ||
	       r_type == PPC_RELOC_HA16 || r_type == PPC_RELOC_LO14){
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
	    
	    if(r_type == PPC_RELOC_VANILLA){
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
	     * Do ppc specific relocation based on the r_type.
	     */
	    else{
		switch(r_type){
		case PPC_RELOC_PB_LA_PTR:
		    value = r_value + image->vmaddr_slide;
		    *((long *)(r_address + r_slide)) = value;
		    break;
		case PPC_RELOC_HI16:
		    instruction = *((long *)(r_address + r_slide));
		    immediate = ((instruction & 0xffff) << 16) | other_half;
		    immediate += value;
		    instruction = (instruction & 0xffff0000) |
				  ((immediate >> 16) & 0xffff);
		    *((long *)(r_address + r_slide)) = instruction;
		    break;
		case PPC_RELOC_LO16:
		    instruction = *((long *)(r_address + r_slide));
		    immediate = (other_half << 16) | (instruction & 0xffff);
		    immediate += value;
		    instruction = (instruction & 0xffff0000) |
				  (immediate & 0xffff);
		    *((long *)(r_address + r_slide)) = instruction;
		    break;
		case PPC_RELOC_HA16:
		    instruction = *((long *)(r_address + r_slide));
		    if((other_half & 0x00008000) != 0)
			immediate = ((instruction & 0xffff) << 16) +
				    (0xffff0000 + other_half);
		    else
			immediate = ((instruction & 0xffff) << 16) +
				    (other_half);
		    immediate += value;
		    if((immediate & 0x00008000) != 0)
			instruction = (instruction & 0xffff0000) |
				  (((immediate + 0x00008000) >> 16) & 0xffff);
		    else
			instruction = (instruction & 0xffff0000) |
				      ((immediate >> 16) & 0xffff);
		    *((long *)(r_address + r_slide)) = instruction;
		    break;
		case PPC_RELOC_LO14:
		    instruction = *((long *)(r_address + r_slide));
		    immediate = (other_half << 16) | (instruction & 0xfffc);
		    immediate += value;
		    if((immediate & 0x3) != 0){
			error("relocation error (local relocation in %s "
			    "relocation entry %lu relocated value not a "
			    "multiple of 4 bytes)", image->name, i);
			link_edit_error(DYLD_OTHER_ERROR, DYLD_RELOCATION,
					image->name);
		    }
		    instruction = (instruction & 0xffff0003) |
				  (immediate & 0xfffc);
		    *((long *)(r_address + r_slide)) = instruction;
		    break;
		case PPC_RELOC_BR14:
		    instruction = *((long *)(r_address + r_slide));
		    immediate = instruction & 0xfffc;
		    if((immediate & 0x8000) != 0)
			immediate |= 0xffff0000;
		    immediate += value;
		    if((immediate & 0x3) != 0){
			error("relocation error (local relocation in %s "
			    "relocation entry %lu relocated value not a "
			    "multiple of 4 bytes)", image->name, i);
			link_edit_error(DYLD_OTHER_ERROR, DYLD_RELOCATION,
					image->name);
		    }
		    if((immediate & 0xfffe0000) != 0xfffe0000 &&
		       (immediate & 0xfffe0000) != 0x00000000){
			error("relocation overflow (local relocation in %s "
			    "relocation entry %lu displacement too large)",
			    image->name, i);
			link_edit_error(DYLD_OTHER_ERROR, DYLD_RELOCATION,
					image->name);
		    }
		    instruction = (instruction & 0xffff0003) |
				  (immediate & 0xfffc);
		    *((long *)(r_address + r_slide)) = instruction;
		    break;
		case PPC_RELOC_BR24:
		    instruction = *((long *)(r_address + r_slide));
		    immediate = instruction & 0x03fffffc;
		    if((immediate & 0x02000000) != 0)
			immediate |= 0xfc000000;
		    immediate += value;
		    if((immediate & 0x3) != 0){
			error("relocation error (local relocation in %s "
			    "relocation entry %lu relocated value not a "
			    "multiple of 4 bytes)", image->name, i);
			link_edit_error(DYLD_OTHER_ERROR, DYLD_RELOCATION,
					image->name);
		    }
		    if((immediate & 0xfe000000) != 0xfe000000 &&
		       (immediate & 0xfe000000) != 0x00000000){
			error("relocation overflow (local relocation in %s "
			    "relocation entry %lu displacement too large)",
			    image->name, i);
			link_edit_error(DYLD_OTHER_ERROR, DYLD_RELOCATION,
					image->name);
		    }
		    instruction = (instruction & 0xfc000003) |
		    		  (immediate & 0x03fffffc);
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
	    ppc_cache_flush(cache_flush_low_addr,
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
    unsigned long instruction, immediate, other_half, br14_disp_sign;
    unsigned long cache_flush_high_addr, cache_flush_low_addr;

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
	     */
	    symbol_name = strings + symbols[relocs[i].r_symbolnum].n_un.n_strx;
	    if(image->private == TRUE &&
	       image->has_coalesced_sections == TRUE &&
	       is_symbol_coalesced(image,
				   symbols + relocs[i].r_symbolnum) == TRUE){
		defined_symbol = symbols + relocs[i].r_symbolnum;
		defined_module = NULL; /* not needed here */
		defined_image = image;
		defined_library_image = NULL; /* not needed here */
		
	    }
	    else{
		lookup_symbol(symbol_name, &defined_symbol, &defined_module,
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

	    if(relocs[i].r_type == PPC_RELOC_VANILLA){
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
	     * Do ppc specific relocation based on the r_type.
	     */
	    else{
		instruction = *((long *)(relocs[i].r_address + r_slide));
		switch(relocs[i].r_type){

		case PPC_RELOC_HI16:
		    other_half  = relocs[i + 1].r_address;
		    immediate = ((instruction & 0xffff) << 16) | other_half;
		    immediate += value;
		    instruction = (instruction & 0xffff0000) |
				  ((immediate >> 16) & 0xffff);
		    break;
		case PPC_RELOC_LO16:
		    other_half  = relocs[i + 1].r_address;
		    immediate = (other_half << 16) | (instruction & 0xffff);
		    immediate += value;
		    instruction = (instruction & 0xffff0000) |
				  (immediate & 0xffff);
		    break;
		case PPC_RELOC_HA16:
		    other_half  = relocs[i + 1].r_address;
		    immediate = ((instruction & 0xffff) << 16) | other_half;
		    immediate += value;
		    if((immediate & 0x00008000) != 0)
			instruction = (instruction & 0xffff0000) |
				  (((immediate + 0x00008000) >> 16) & 0xffff);
		    else
			instruction = (instruction & 0xffff0000) |
				      ((immediate >> 16) & 0xffff);
		    break;
		case PPC_RELOC_LO14:
		    other_half  = relocs[i + 1].r_address;
		    immediate = (other_half << 16) | (instruction & 0xfffc);
		    immediate += value;
		    if((immediate & 0x3) != 0){
			set_error_string("dyld: relocation error (external "
			    "relocation for symbol %s in ", symbol_name);
			if(library_name != NULL)
			    add_error_string("%s(%s) ", library_name,
				module_name);
			else
			    add_error_string("%s ", module_name);
			add_error_string("relocation entry %lu relocated value "
			    "not a multiple of 4 bytes)", i);
			link_edit_error(DYLD_OTHER_ERROR, DYLD_RELOCATION,
					image->name);
		    }
		    instruction = (instruction & 0xffff0003) |
				  (immediate & 0xfffc);
		    break;
		case PPC_RELOC_BR14:
		    br14_disp_sign = (instruction & 0x8000);
		    immediate = instruction & 0xfffc;
		    if((immediate & 0x8000) != 0)
			immediate |= 0xffff0000;
		    immediate += value;
		    if((immediate & 0x3) != 0){
			set_error_string("dyld: relocation error (external "
			    "relocation for symbol %s in ", symbol_name);
			if(library_name != NULL)
			    add_error_string("%s(%s) ", library_name,
				module_name);
			else
			    add_error_string("%s ", module_name);
			add_error_string("relocation entry %lu relocated value "
			    "not a multiple of 4 bytes)", i);
			link_edit_error(DYLD_OTHER_ERROR, DYLD_RELOCATION,
					image->name);
		    }
		    if((immediate & 0xfffe0000) != 0xfffe0000 &&
		       (immediate & 0xfffe0000) != 0x00000000){
			set_error_string("dyld: relocation error (external "
			    "relocation for symbol %s in ", symbol_name);
			if(library_name != NULL)
			    add_error_string("%s(%s) ", library_name,
				module_name);
			else
			    add_error_string("%s ", module_name);
			add_error_string("relocation entry %lu displacement "
			    "too large)", i);
			link_edit_error(DYLD_OTHER_ERROR, DYLD_RELOCATION,
					image->name);
		    }
		    instruction = (instruction & 0xffff0003) |
				  (immediate & 0xfffc);
		    /*
		     * If this is a branch conditional B-form where
		     * the branch condition is not branch always and
		     * the sign of the displacement is different
		     * after relocation then flip the Y-bit to
		     * preserve the sense of the branch prediction. 
		     */
		    if((instruction & 0xfc000000) == 0x40000000 &&
		       (instruction & 0x03e00000) != 0x02800000 &&
		       (instruction & 0x00008000) != br14_disp_sign)
			instruction ^= (1 << 21);
		    break;
		case PPC_RELOC_BR24:
		    immediate = instruction & 0x03fffffc;
		    if((immediate & 0x02000000) != 0)
			immediate |= 0xfc000000;
		    immediate += value;
		    if((immediate & 0x3) != 0){
			set_error_string("dyld: relocation error (external "
			    "relocation for symbol %s in ", symbol_name);
			if(library_name != NULL)
			    add_error_string("%s(%s) ", library_name,
				module_name);
			else
			    add_error_string("%s ", module_name);
			add_error_string("relocation entry %lu relocated value "
			    "not a multiple of 4 bytes)", i);
			link_edit_error(DYLD_OTHER_ERROR, DYLD_RELOCATION,
					image->name);
		    }
		    if((immediate & 0xfe000000) != 0xfe000000 &&
		       (immediate & 0xfe000000) != 0x00000000){
			set_error_string("dyld: relocation error (external "
			    "relocation for symbol %s in ", symbol_name);
			if(library_name != NULL)
			    add_error_string("%s(%s) ", library_name,
				module_name);
			else
			    add_error_string("%s ", module_name);
			add_error_string("relocation entry %lu displacement "
			    "too large)", i);
			link_edit_error(DYLD_OTHER_ERROR, DYLD_RELOCATION,
					image->name);
		    }
		    instruction = (instruction & 0xfc000003) |
		    		  (immediate & 0x03fffffc);
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
	    if(relocs[i].r_type == PPC_RELOC_HI16 ||
	       relocs[i].r_type == PPC_RELOC_LO16 ||
	       relocs[i].r_type == PPC_RELOC_HA16 ||
	       relocs[i].r_type == PPC_RELOC_LO14)
		i++;
	}
done:
	if(image->cache_sync_on_reloc &&
	   cache_flush_high_addr > cache_flush_low_addr)
	    ppc_cache_flush(cache_flush_low_addr,
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
    unsigned long instruction, immediate, other_half, br14_disp_sign;
    enum bool relocs_writable;
    kern_return_t r;
    unsigned long cache_flush_high_addr, cache_flush_low_addr;

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
		    lookup_symbol(symbol_name, &defined_symbol, &defined_module,
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

	    if(relocs[i].r_type == PPC_RELOC_VANILLA){
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
	     * Do ppc specific relocation based on the r_type.
	     */
	    else{
		if(relocs_writable == FALSE){
		    /* make sure the linkedit segment for is writable */
		    if((r = vm_protect(mach_task_self(),
			image->linkedit_segment-> vmaddr + image->vmaddr_slide,
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
		case PPC_RELOC_HI16:
		    other_half  = relocs[i + 1].r_address;
		    immediate = ((instruction & 0xffff) << 16) | other_half;
		    immediate -= value;
		    instruction = (instruction & 0xffff0000) |
				  ((immediate >> 16) & 0xffff);
		    relocs[i + 1].r_address = immediate & 0xffff;
		    break;
		case PPC_RELOC_LO16:
		    other_half  = relocs[i + 1].r_address;
		    immediate = (other_half << 16) | (instruction & 0xffff);
		    immediate -= value;
		    instruction = (instruction & 0xffff0000) |
				  (immediate & 0xffff);
		    relocs[i + 1].r_address = (immediate >> 16) & 0xffff;
		    break;
		case PPC_RELOC_HA16:
		    other_half  = relocs[i + 1].r_address;
		    if((other_half & 0x00008000) != 0)
			immediate = ((instruction & 0xffff) << 16) +
				    (0xffff0000 + other_half);
		    else
			immediate = ((instruction & 0xffff) << 16) +
				    (other_half);
		    immediate -= value;
		    if((immediate & 0x00008000) != 0)
			instruction = (instruction & 0xffff0000) |
				  (((immediate + 0x00008000) >> 16) & 0xffff);
		    else
			instruction = (instruction & 0xffff0000) |
				      ((immediate >> 16) & 0xffff);
		    relocs[i + 1].r_address = immediate & 0xffff;
		    break;
		case PPC_RELOC_LO14:
		    other_half  = relocs[i + 1].r_address;
		    immediate = (other_half << 16) | (instruction & 0xfffc);
		    immediate -= value;
		    if((immediate & 0x3) != 0){
			set_error_string("dyld: relocation error (external "
			    "relocation for symbol %s in ", symbol_name);
			if(library_name != NULL)
			    add_error_string("%s(%s) ", library_name,
				module_name);
			else
			    add_error_string("%s ", module_name);
			add_error_string("relocation entry %lu relocated value "
			    "not a multiple of 4 bytes)", i);
			link_edit_error(DYLD_OTHER_ERROR, DYLD_RELOCATION,
					image->name);
		    }
		    instruction = (instruction & 0xffff0003) |
				  (immediate & 0xfffc);
		    relocs[i + 1].r_address = (immediate >> 16) & 0xffff;
		    break;
		case PPC_RELOC_BR14:
		    br14_disp_sign = (instruction & 0x8000);
		    immediate = instruction & 0xfffc;
		    if((immediate & 0x8000) != 0)
			    immediate |= 0xffff0000;
		    immediate -= value;
		    if((immediate & 0x3) != 0){
			set_error_string("dyld: relocation error (external "
			    "relocation for symbol %s in ", symbol_name);
			if(library_name != NULL)
			    add_error_string("%s(%s) ", library_name,
				module_name);
			else
			    add_error_string("%s ", module_name);
			add_error_string("relocation entry %lu relocated value "
			    "not a multiple of 4 bytes)", i);
			link_edit_error(DYLD_OTHER_ERROR, DYLD_RELOCATION,
					image->name);
		    }
		    if((immediate & 0xfffe0000) != 0xfffe0000 &&
		       (immediate & 0xfffe0000) != 0x00000000){
			set_error_string("dyld: relocation error (external "
			    "relocation for symbol %s in ", symbol_name);
			if(library_name != NULL)
			    add_error_string("%s(%s) ", library_name,
				module_name);
			else
			    add_error_string("%s ", module_name);
			add_error_string("relocation entry %lu displacement "
			    "too large)", i);
			link_edit_error(DYLD_OTHER_ERROR, DYLD_RELOCATION,
					image->name);
		    }
		    instruction = (instruction & 0xffff0003) |
				  (immediate & 0xfffc);
		    /*
		     * If this is a branch conditional B-form where
		     * the branch condition is not branch always and
		     * the sign of the displacement is different
		     * after relocation then flip the Y-bit to
		     * preserve the sense of the branch prediction. 
		     */
		    if((instruction & 0xfc000000) == 0x40000000 &&
		       (instruction & 0x03e00000) != 0x02800000 &&
		       (instruction & 0x00008000) != br14_disp_sign)
			instruction ^= (1 << 21);
		    break;
		case PPC_RELOC_BR24:
		    immediate = instruction & 0x03fffffc;
		    if((immediate & 0x02000000) != 0)
			immediate |= 0xfc000000;
		    immediate -= value;
		    if((immediate & 0x3) != 0){
			set_error_string("dyld: relocation error (external "
			    "relocation for symbol %s in ", symbol_name);
			if(library_name != NULL)
			    add_error_string("%s(%s) ", library_name,
				module_name);
			else
			    add_error_string("%s ", module_name);
			add_error_string("relocation entry %lu relocated value "
			    "not a multiple of 4 bytes)", i);
			link_edit_error(DYLD_OTHER_ERROR, DYLD_RELOCATION,
					image->name);
		    }
		    if((immediate & 0xfe000000) != 0xfe000000 &&
		       (immediate & 0xfe000000) != 0x00000000){
			set_error_string("dyld: relocation error (external "
			    "relocation for symbol %s in ", symbol_name);
			if(library_name != NULL)
			    add_error_string("%s(%s) ", library_name,
				module_name);
			else
			    add_error_string("%s ", module_name);
			add_error_string("relocation entry %lu displacement "
			    "too large)", i);
			link_edit_error(DYLD_OTHER_ERROR, DYLD_RELOCATION,
					image->name);
		    }
		    instruction = (instruction & 0xfc000003) |
		    		  (immediate & 0x03fffffc);
		    break;
		default:
		    break;
		}
		*((long *)(relocs[i].r_address + r_slide)) = instruction;
	    }
	    /*
	     * If the relocation entry had a pair step over it.
	     */
	    if(relocs[i].r_type == PPC_RELOC_HI16 ||
	       relocs[i].r_type == PPC_RELOC_LO16 ||
	       relocs[i].r_type == PPC_RELOC_HA16 ||
	       relocs[i].r_type == PPC_RELOC_LO14)
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
	    ppc_cache_flush(cache_flush_low_addr,
			    cache_flush_high_addr - cache_flush_low_addr);
}
#endif /* __ppc__ */
