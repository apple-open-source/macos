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
#import <stdlib.h>
#import <limits.h>
#import <string.h>
#import <mach/mach.h>
#import "stuff/openstep_mach.h"
#import <mach-o/loader.h>
#import <mach-o/nlist.h>
#import <mach-o/reloc.h>
#ifdef hppa
#import <mach-o/hppa/reloc.h>
#endif
#ifdef sparc
#import <mach-o/sparc/reloc.h>
#endif
#import <mach-o/dyld_debug.h>

#import "stuff/vm_flush_cache.h"

#import "images.h"
#import "symbols.h"
#import "errors.h"
#import "reloc.h"
#import "debug.h"
#import "register_funcs.h"

/*
 * relocate_modules_being_linked() preforms the external relocation for the
 * modules being linked and sets the values of the symbol pointers for the
 * symbols being linked.  When trying to launch with prebound libraries then
 * launching_with_prebound_libraries is TRUE.  Then only the executable gets
 * relocated and all library modules in the BEING_LINKED state get set to the
 * FULLY_LINKED state and the others get set to PREBOUND_UNLINKED state.
 */
void
relocate_modules_being_linked(
enum bool launching_with_prebound_libraries)
{
    unsigned long i, j;
    enum link_state link_state;
    struct object_images *p;
    struct library_images *q;
    struct segment_command *linkedit_segment;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    struct dylib_module *dylib_modules;
    struct relocation_info *relocs;
    struct nlist *symbols;
    char *strings;
    struct dyld_event event;

	memset(&event, '\0', sizeof(struct dyld_event));
	event.type = DYLD_MODULE_BOUND;
	/*
	 * First relocate object_images modules that are being linked.
	 */
	p = &object_images;
	do{
	    for(i = 0; i < p->nimages; i++){
		link_state = GET_LINK_STATE(p->images[i].module);
		if(link_state == UNUSED)
		    continue;
		relocate_symbol_pointers_in_object_image(&(p->images[i].image));

		/* skip modules that that not in the being linked state */
		if(link_state != BEING_LINKED)
		    continue;

		linkedit_segment = p->images[i].image.linkedit_segment;
		st = p->images[i].image.st;
		dyst = p->images[i].image.dyst;
		/*
		 * Object images could be loaded that do not have the proper
		 * link edit information.
		 */
		if(linkedit_segment == NULL || st == NULL || dyst == NULL)
		    continue;
		relocs = (struct relocation_info *)
		    (p->images[i].image.vmaddr_slide +
		     linkedit_segment->vmaddr +
		     dyst->extreloff -
		     linkedit_segment->fileoff);
		symbols = (struct nlist *)
		    (p->images[i].image.vmaddr_slide +
		     linkedit_segment->vmaddr +
		     st->symoff -
		     linkedit_segment->fileoff);
		strings = (char *)
		    (p->images[i].image.vmaddr_slide +
		     linkedit_segment->vmaddr +
		     st->stroff -
		     linkedit_segment->fileoff);
		/*
		 * If the image has relocations in read-only segments and the
		 * protection needs to change change it.
		 */
		if(p->images[i].image.change_protect_on_reloc){
		    make_image_writable(&(p->images[i].image), "object");
		}

		link_state = 
		    external_relocation(
			&(p->images[i].image),
			relocs,
			dyst->nextrel,
			symbols,
			strings,
			NULL, /* library_name */
			p->images[i].image.name);
		SET_LINK_STATE(p->images[i].module, link_state);

		if(launching_with_prebound_libraries == TRUE)
		    SET_LINK_STATE(p->images[i].module, FULLY_LINKED);

		/* send the event message that this module was bound */
		event.arg[0].header = p->images[i].image.mh;
		event.arg[0].vmaddr_slide = p->images[i].image.vmaddr_slide;
		event.arg[0].module_index = 0;
		send_event(&event);

		/*
		 * If the image has relocations in read-only segments and the
		 * protection was changed change it back.
		 */
		if(p->images[i].image.change_protect_on_reloc){
		    restore_image_vm_protections(&(p->images[i].image),
						 "object");
		}
	    }
	    p = p->next_images;
	}while(p != NULL);

	/*
	 * Next relocate the library_images.
	 */
	q = &library_images;
	do{
	    for(i = 0; i < q->nimages; i++){
		linkedit_segment = q->images[i].image.linkedit_segment;
		st = q->images[i].image.st;
		dyst = q->images[i].image.dyst;
		relocs = (struct relocation_info *)
		    (q->images[i].image.vmaddr_slide +
		     linkedit_segment->vmaddr +
		     dyst->extreloff -
		     linkedit_segment->fileoff);
		symbols = (struct nlist *)
		    (q->images[i].image.vmaddr_slide +
		     linkedit_segment->vmaddr +
		     st->symoff -
		     linkedit_segment->fileoff);
		strings = (char *)
		    (q->images[i].image.vmaddr_slide +
		     linkedit_segment->vmaddr +
		     st->stroff -
		     linkedit_segment->fileoff);
		dylib_modules = (struct dylib_module *)
		    (q->images[i].image.vmaddr_slide +
		     linkedit_segment->vmaddr +
		     dyst->modtaboff -
		     linkedit_segment->fileoff);
		/*
		 * If the image has relocations in read-only segments and the
		 * protection needs to change change it.
		 */
		if(q->images[i].image.change_protect_on_reloc)
		    make_image_writable(&(q->images[i].image), "library");
		for(j = 0; j < dyst->nmodtab; j++){

		    /* skip modules that that not in the being linked state */
		    link_state = GET_LINK_STATE(q->images[i].modules[j]);
		    if(link_state != BEING_LINKED){
			if(launching_with_prebound_libraries == TRUE)
			    SET_LINK_STATE(q->images[i].modules[j],
					   PREBOUND_UNLINKED);
			continue;
		    }

		    if(launching_with_prebound_libraries == FALSE){
			link_state = 
			    external_relocation(
				&(q->images[i].image),
				relocs + dylib_modules[j].iextrel,
				dylib_modules[j].nextrel,
				symbols,
				strings,
				q->images[i].image.name,
				strings + dylib_modules[j].module_name);
			SET_LINK_STATE(q->images[i].modules[j], link_state);
		    }
		    else
			SET_LINK_STATE(q->images[i].modules[j], FULLY_LINKED);

		    /* send the event message that this module was bound */
		    event.arg[0].header = q->images[i].image.mh;
		    event.arg[0].vmaddr_slide = q->images[i].image.vmaddr_slide;
		    event.arg[0].module_index = j;
		    send_event(&event);
		}
		/*
		 * If the image has relocations in read-only segments and the
		 * protection was changed change it back.
		 */
		if(q->images[i].image.change_protect_on_reloc)
		    restore_image_vm_protections(&(q->images[i].image),
						 "library");

		if(launching_with_prebound_libraries == FALSE)
		    relocate_symbol_pointers_in_library_image(
			&(q->images[i].image));
	    }
	    q = q->next_images;
	}while(q != NULL);

	clear_being_linked_list(FALSE);
}

/*
 * undo_prebound_lazy_pointers() undoes the prebinding for lazy symbol pointers.
 * This is also done by local_relocation() which is called if the image can't
 * be loaded at the addresses it wants in the file.  This is called when the
 * prebinding can't be used and the image was loaded at the address it wants.
 */
void
undo_prebound_lazy_pointers(
struct image *image,
unsigned long PB_LA_PTR_r_type)
{
    unsigned long i, r_slide, r_address, r_type, r_value, value;
    struct relocation_info *relocs;
    struct scattered_relocation_info *sreloc;
    unsigned long cache_flush_high_addr, cache_flush_low_addr;

	relocs = (struct relocation_info *)
	    (image->vmaddr_slide +
	     image->linkedit_segment->vmaddr +
	     image->dyst->locreloff -
	     image->linkedit_segment->fileoff);

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
		r_type = sreloc->r_type;
		r_value = sreloc->r_value;

		/*
		 * If the relocation entry is for a prebound lazy pointer
		 * (r_type is the passed in value PB_LA_PTR_r_type) then undo
		 * the prebinding by using the value of the lazy pointer saved
		 * in r_value plus the slide amount.
		 */
		if(r_type == PB_LA_PTR_r_type){
		    value = r_value + image->vmaddr_slide;
		    *((long *)(r_address + r_slide)) = value;

		    if(image->cache_sync_on_reloc){
			if(r_address + r_slide < cache_flush_low_addr)
			    cache_flush_low_addr = r_address + r_slide;
			if(r_address + r_slide + (1 << 2) >
			    cache_flush_high_addr)
			    cache_flush_high_addr =
				r_address + r_slide + (1 << 2);
		    }
		}
	    }
#if defined(hppa) || defined(sparc)
	    else{
		r_type = relocs[i].r_type;
	    }
	    /*
	     * If the relocation entry had a pair step over it.
	     */
#ifdef hppa
	    if(r_type == HPPA_RELOC_HI21 ||
	       r_type == HPPA_RELOC_LO14 ||
	       r_type == HPPA_RELOC_BR17)
#endif
#ifdef sparc
	    if(r_type == SPARC_RELOC_HI22 ||
	       r_type == SPARC_RELOC_LO10 )
#endif
		i++;
#endif /* defined(hppa) || defined(sparc) */

	}

	if(image->cache_sync_on_reloc &&
	   cache_flush_high_addr > cache_flush_low_addr)
	    vm_flush_cache(mach_task_self(), cache_flush_low_addr,
			   cache_flush_high_addr - cache_flush_low_addr);
}

/*
 * undo_prebinding_for_library_module() is called when a library module is in
 * the PREBOUND_UNLINKED state to get it into the UNLINKED.  The library module
 * is specified with the module, image and library_image parameters.
 */
void
undo_prebinding_for_library_module(
module_state *module,
struct image *image,
struct library_image *library_image)
{
    struct segment_command *linkedit_segment;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    struct relocation_info *relocs;
    struct nlist *symbols;
    char *strings;
    struct dylib_module *dylib_modules;
    unsigned long module_index;

	linkedit_segment = image->linkedit_segment;
	st = image->st;
	dyst = image->dyst;
	relocs = (struct relocation_info *)
	    (image->vmaddr_slide +
	     linkedit_segment->vmaddr +
	     dyst->extreloff -
	     linkedit_segment->fileoff);
	symbols = (struct nlist *)
	    (image->vmaddr_slide +
	     linkedit_segment->vmaddr +
	     st->symoff -
	     linkedit_segment->fileoff);
	strings = (char *)
	    (image->vmaddr_slide +
	     linkedit_segment->vmaddr +
	     st->stroff -
	     linkedit_segment->fileoff);
	dylib_modules = (struct dylib_module *)
	    (image->vmaddr_slide +
	     linkedit_segment->vmaddr +
	     dyst->modtaboff -
	     linkedit_segment->fileoff);
	module_index = module - library_image->modules;

	if(image->change_protect_on_reloc)
	    make_image_writable(image, "library");

	undo_external_relocation(
	    TRUE, /* undo_prebinding */
	    image,
	    relocs + dylib_modules[module_index].iextrel,
	    dylib_modules[module_index].nextrel,
	    symbols,
	    strings,
	    image->name,
	    strings + dylib_modules[module_index].module_name);

	if(image->change_protect_on_reloc)
	    restore_image_vm_protections(image, "library");
}

/*
 * make_image_writable() changes the protections of the segments of the image
 * to its maxprot to make it writeable.  This is only called when the image has
 * relocation entries in read-only segments (bad code gen).  The image_type is
 * a string like "library" or "object" and used only for error messages.
 */  
void
make_image_writable(
struct image *image,
char *image_type)
{
    unsigned long i;
    struct load_command *lc;
    struct segment_command *sg;
    kern_return_t r;

    /*
     * If the image has relocations in read-only segments and
     * the protection needs to change change it.
     */
	lc = (struct load_command *)((char *)image->mh +
				     sizeof(struct mach_header));
	for(i = 0; i < image->mh->ncmds; i++){
	    switch(lc->cmd){
	    case LC_SEGMENT:
		sg = (struct segment_command *)lc;
		if((r = vm_protect(mach_task_self(),sg->vmaddr +
				   image->vmaddr_slide, (vm_size_t)sg->vmsize,
		    		   FALSE, sg->maxprot)) != KERN_SUCCESS){
		    mach_error(r, "can't set vm_protection on segment: %.16s "
			       "for %s: %s", sg->segname, image_type,
			       image->name);
		    link_edit_error(DYLD_MACH_RESOURCE, r, image->name);
		}
		break;
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
}

/*
 * restore_image_vm_protections() undoes what make_image_writable() and changes
 * the protections of the segments of the image back to its initprot.  This
 * is only called when the image has relocation entries in read-only segments
 * (bad code gen). The image_type is a string like "library" or "object" and
 * used only for error messages.
 */  
void
restore_image_vm_protections(
struct image *image,
char *image_type)
{
    unsigned long i;
    struct load_command *lc;
    struct segment_command *sg;
    kern_return_t r;

    /*
     * If the image has relocations in read-only segments and
     * the protection was changed change it back.
     */
	lc = (struct load_command *)((char *)image->mh +
				     sizeof(struct mach_header));
	for(i = 0; i < image->mh->ncmds; i++){
	    switch(lc->cmd){
	    case LC_SEGMENT:
		sg = (struct segment_command *)lc;
		if((r = vm_protect(mach_task_self(),sg->vmaddr +
				   image->vmaddr_slide, (vm_size_t)sg->vmsize,
				   FALSE, sg->initprot)) != KERN_SUCCESS){
		    mach_error(r, "can't set vm_protection on segment: %.16s "
			       "for %s: %s", sg->segname, image_type, 
			       image->name);
		    link_edit_error(DYLD_MACH_RESOURCE, r, image->name);
		}
		break;
	    }
	    lc = (struct load_command *)
		 ((char *)lc + lc->cmdsize);
	}
}

