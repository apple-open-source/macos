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
#import <stdio.h>
#import <stdlib.h>
#import <string.h>
#import <limits.h>
#import <mach/mach.h>
#import "stuff/openstep_mach.h"
#import <mach-o/loader.h>
#import <mach-o/nlist.h>

#import "inline_strcmp.h"
#import "stuff/bool.h"
#import "images.h"
#import "symbols.h"
#import "allocate.h"
#import "errors.h"
#import "reloc.h"
#import "register_funcs.h"
#import "mod_init_funcs.h"
#import "lock.h"
#import "dyld_init.h"
#import "trace.h"

/*
 * The head of the undefined list, the list of symbols being linked and a free
 * list to hold symbol list structures not currently being used.
 * These are circular lists so they can be searched from start to end and so
 * new items can be put on the end.  These three structures never have their
 * symbol, module or image filled in but they only serve as the heads and tails
 * of their lists.
 */
struct symbol_list undefined_list = {
    NULL, NULL, NULL, FALSE, FALSE, FALSE, &undefined_list, &undefined_list
};
static struct symbol_list being_linked_list = {
    NULL, NULL, NULL, FALSE, FALSE, FALSE, &being_linked_list,&being_linked_list
};
static struct symbol_list free_list = {
    NULL, NULL, NULL, FALSE, FALSE, FALSE, &free_list, &free_list
};
/*
 * The structures for the symbol list are allocated in blocks and placed on the
 * free list and then handed out.  When they ar no longer needed they are placed
 * on the free list to use again.  This is data structure is the largest one in
 * dyld.  As such great care is taken to dirty as little as possible.
 * 
 * Using /System/Library/CoreServices/Desktop.app/Contents/MacOS/Desktop from
 * MacOS X Public Beta (Kodiak1G7) it uses 43 symbol_blocks to get launched as
 * it does not launch prebound as QuickTime is no prebound.
 */
enum nsymbol_lists { NSYMBOL_LISTS = 100 };
struct symbol_block {
    enum bool initialized;
    struct symbol_list symbols[NSYMBOL_LISTS];
    struct symbol_block *next;
};
/*
 * DO NOT CHANGE the value of NSYMBOL_BLOCKS without changing where it is
 * allocated in section_order.s
 */
enum nsymbol_blocks { NSYMBOL_BLOCKS = 45 };
/*
 * The data is allocated in it's own section so it can go at the end of the
 * data segment.  So that the blocks never touched are never dirtied.
 */
extern struct symbol_block symbol_blocks[NSYMBOL_BLOCKS];
static unsigned long symbol_blocks_used = 0;

static void initialize_symbol_block(
    struct symbol_block *symbol_block);
static struct symbol_list *new_symbol_list(
    void);
static void add_to_undefined_list(
    char *name,
    struct nlist *symbol,
    struct image *image,
    enum bool bind_fully,
    enum bool flat_reference);
static void add_to_being_linked_list(
    char *name,
    struct nlist *symbol,
    struct image *image,
    enum bool flat_reference);
static enum bool are_symbols_coalesced(
    struct image *image1,
    struct nlist *symbol1,
    struct image *image2,
    struct nlist *symbol2);
static enum bool is_symbol_coalesced_and_weak(
    struct image *image,
    struct nlist *symbol);
static enum bool is_section_coalesced(
    struct image *image,
    unsigned int nsect);
static void add_reference(
    char *symbol_name,
    struct nlist *symbol,
    struct image *image,
    enum bool bind_now,
    enum bool bind_fully);
static struct image *get_indr_image(
    char *symbol_name,
    struct image *image);
static enum bool lookup_symbol_in_library_image(
    char *symbol_name,
    unsigned long itoc,
    struct image *image,
    module_state *library_modules,
    struct library_image *library_image,
    struct nlist **defined_symbol,
    module_state **defined_module,
    struct image **defined_image,
    struct library_image **defined_library_image,
    struct indr_loop_list *indr_loop);

static char *nlist_bsearch_strings;
static struct nlist *toc_bsearch_symbols;
static char *toc_bsearch_strings;
#include "inline_bsearch.h"

static struct nlist *nlist_linear_search(
    char *symbol_name,
    struct nlist *symbols,
    unsigned long nsymbols,
    char *strings);

static char * look_for_flat_reference(
    char *symbol_name,
    struct nlist **referenced_symbol,
    module_state **referenced_module,
    struct image **referenced_image,
    struct library_image **referenced_library_image,
    enum bool ignore_lazy_references);

static void relocate_symbol_pointers(
    unsigned long symbol_index,
    unsigned long *indirect_symtab,
    struct image *image,
    unsigned long value,
    enum bool only_lazy_pointers);

static enum bool check_libraries_for_definition_and_refernce(
    char *symbol_name,
    struct library_image *library_image);

/*
 * get_primary_image() gets the primary image for a two-level symbol reference
 * from the specified image.  If the the image is NULL or the symbol is NULL or
 * the image passed is not two-level or force_flat_namespace is TRUE then NULL
 * is returned.  Otherwise the pointer to the primary image for the reference
 * is returned.
 */
/* TODO make this "static inline" moving it to symbols.h after debugging it */
struct image *
get_primary_image(
struct image *image,
struct nlist *symbol)
{
    struct image *primary_image;

	if(image != NULL && symbol != NULL && force_flat_namespace == FALSE &&
	   (image->mh->flags & MH_TWOLEVEL) == MH_TWOLEVEL){
	    if(GET_LIBRARY_ORDINAL(symbol->n_desc) == EXECUTABLE_ORDINAL)
		primary_image = &object_images.images[0].image;
	    else if(GET_LIBRARY_ORDINAL(symbol->n_desc) ==
		    DYNAMIC_LOOKUP_ORDINAL &&
		    image->ndependent_images != DYNAMIC_LOOKUP_ORDINAL)
		primary_image = NULL;
	    else if(GET_LIBRARY_ORDINAL(symbol->n_desc) ==
		    SELF_LIBRARY_ORDINAL){
		/*
		 * For two-level libraries that reference symbols
		 * defined in the same library then the LIBRARY_ORDINAL
		 * will be SELF_LIBRARY_ORDINAL as the symbol is the
		 * defined symbol.
		 */
		primary_image = image;
	    }
	    else{
		/* could make this check
		if(GET_LIBRARY_ORDINAL(symbol->n_desc) - 1 >
		   image->ndependent_images)
		    <<< bad LIBRARY_ORDINAL in object file >>>
		*/
		primary_image = image->dependent_images[
			    GET_LIBRARY_ORDINAL(symbol->n_desc) - 1];
	    }
	}
	else{
	    primary_image = NULL;
	}
	return(primary_image);
}

/*
 * get_hint() gets the twolevel_hint for a two-level symbol reference from the
 * specified image for the specified symbol.  If the the image is NULL or the
 * symbol is NULL or the hints can't be used then NULL is returned.  Otherwise
 * the pointer to the twolevel_hint for the symbol in the image is returned.
 */
/* TODO make this "static inline" moving it to symbols.h after debugging it */
struct twolevel_hint *
get_hint(
struct image *image,
struct nlist *symbol)
{
    struct nlist *symbols;
    struct twolevel_hint *hints, *hint;
    unsigned long symbol_index;

	/*
	 * Note that the bit field image_can_use_hints will never be set if
 	 * force_flat_namespace is TRUE, the image is not two-level, does not
	 * have any hints or the hints can't be used because of possible
	 * new duplicate symbols in the umbrella.
	 */
	if(image != NULL &&
	   symbol != NULL &&
	   image->image_can_use_hints == TRUE &&
	   ((symbol->n_type & N_TYPE) == N_UNDF ||
	    (symbol->n_type & N_TYPE) == N_PBUD) ){
	    symbols = (struct nlist *)
		(image->vmaddr_slide +
		 image->linkedit_segment->vmaddr +
		 image->st->symoff -
		 image->linkedit_segment->fileoff);
	    hints = (struct twolevel_hint *)
		(image->vmaddr_slide +
		 image->linkedit_segment->vmaddr +
		 image->hints_cmd->offset -
		 image->linkedit_segment->fileoff);
	    symbol_index = symbol - symbols;
	    hint = hints + (symbol_index - image->dyst->iundefsym);
	}
	else{
	    hint = NULL;
	}
	return(hint);
}

/*
 * get_weak() is passed a symbol pointer and it the pointer is not NULL and the
 * weak bit of the symbol is set then TRUE is returned indicating the symbol is
 * weak.  Else FALSE is returned.
 */
/* TODO make this "static inline" moving it to symbols.h after debugging it */
enum bool
get_weak(
struct nlist *symbol)
{
	if(symbol != NULL && (symbol->n_desc & N_WEAK_REF) == N_WEAK_REF)
	    return(TRUE);
	return(FALSE);
}

/*
 * setup_initial_undefined_list() builds the initial list of non-lazy symbol
 * references based on the executable's symbols.
 */
void
setup_initial_undefined_list(
enum bool all_symbols)
{
    unsigned long i;
    char *symbol_name;
    struct nlist *symbols;
    struct segment_command *linkedit_segment;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    struct image *image;
    enum bool bind_fully;
    enum bool flat_reference;

	/*
	 * The executable is the first object on the object_image list.
	 */
	linkedit_segment = object_images.images[0].image.linkedit_segment;
	st = object_images.images[0].image.st;
	dyst = object_images.images[0].image.dyst;
	if(linkedit_segment == NULL || st == NULL || dyst == NULL)
	    return;
	/* the vmaddr_slide of an executable is always 0, no need to add it */
	symbols = (struct nlist *)
	    (linkedit_segment->vmaddr +
	     st->symoff -
	     linkedit_segment->fileoff);
	image = &object_images.images[0].image;
	if(force_flat_namespace == FALSE)
	    flat_reference = (object_images.images[0].image.mh->flags &
			      MH_TWOLEVEL) != MH_TWOLEVEL;
	else
	    flat_reference = TRUE;

	bind_fully = FALSE;
#ifdef FULLY_BIND_MOD_INIT_ROUTINES
	/*
	 * If this image has any module init or term routines force the image
	 * to be fully bound.
	 */
	if(object_images.images[0].image.init != NULL ||
	   object_images.images[0].image.term != NULL)
	    bind_fully = TRUE;
#endif /* FULLY_BIND_MOD_INIT_ROUTINES */

	for(i = dyst->iundefsym; i < dyst->iundefsym + dyst->nundefsym; i++){
	    if(executable_bind_at_load == TRUE ||
	       all_symbols == TRUE ||
	       (symbols[i].n_desc & REFERENCE_TYPE) ==
	        REFERENCE_FLAG_UNDEFINED_NON_LAZY){
		symbol_name = (char *)
		    (image->linkedit_segment->vmaddr +
		     image->st->stroff -
		     image->linkedit_segment->fileoff) +
		    symbols[i].n_un.n_strx;
		add_to_undefined_list(symbol_name, symbols + i, image,
		    bind_fully, flat_reference);
	    }
	}
}

/*
 * setup_prebound_coalesed_symbols() builds the list of coalesced symbols in
 * the prebound program.  Then gets the symbol pointers and external relocation
 * entries relocated to the coalesced symbols being used.
 */
void
setup_prebound_coalesed_symbols(
void)
{
    unsigned long i, j, k, isym;
    char *symbol_name;
    struct segment_command *linkedit_segment;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    struct nlist *symbols, *symbol;
    char *strings;
    struct image *image;
    struct library_images *q;
    struct dylib_module *dylib_modules, *dylib_module;
    struct dylib_reference *dylib_references;
    struct symbol_list *being_linked;
    enum link_state link_state;
    enum bool found;
    struct relocation_info *relocs;
    struct dylib_table_of_contents *tocs, *toc;
    enum bool flat_reference;
    int number_of_global_coalesced_symbols;
    int total_number_of_global_coalesced_symbols;

    total_number_of_global_coalesced_symbols = 0;

	/*
	 * The executable is the first object on the object_image list.  So
	 * pick up any global coalesced symbols from it.
	 */
	image = &object_images.images[0].image;
	if(image->has_coalesced_sections == TRUE){
	    number_of_global_coalesced_symbols = 0;
	    linkedit_segment = image->linkedit_segment;
	    st = image->st;
	    dyst = image->dyst;
	    if(force_flat_namespace == FALSE)
		flat_reference = (object_images.images[0].image.mh->flags &
				  MH_TWOLEVEL) != MH_TWOLEVEL;
	    else
		flat_reference = TRUE;
	    if(linkedit_segment != NULL && st != NULL && dyst != NULL &&
	       flat_reference == TRUE){
		/*
		 * The vmaddr_slide of an executable is always 0, no need to add
		 * it when figuring out these pointers.
		 */
		symbols = (struct nlist *)
		    (linkedit_segment->vmaddr +
		     st->symoff -
		     linkedit_segment->fileoff);
		strings = (char *)
		    (linkedit_segment->vmaddr +
		     st->stroff -
		     linkedit_segment->fileoff);

		for(i = dyst->iextdefsym;
		    i < dyst->iextdefsym + dyst->nextdefsym;
		    i++){
		    symbol = symbols + i;
		    if((symbol->n_type & N_TYPE) == N_SECT &&
		       is_section_coalesced(image, symbol->n_sect - 1) == TRUE){
			symbol_name = strings + symbol->n_un.n_strx;
			number_of_global_coalesced_symbols += 1;
			add_to_being_linked_list(symbol_name, symbol, image,
						 flat_reference);
		    }
		}
	    }
	    total_number_of_global_coalesced_symbols +=
		number_of_global_coalesced_symbols;
	}

	/*
	 * Next add any global coalesced symbols from library modules that are
	 * fully linked in that are not already on the list.
	 */
	q = &library_images;
	do{
	    for(i = 0; i < q->nimages; i++){
		number_of_global_coalesced_symbols = 0;
		image = &(q->images[i].image);
		if(image->has_coalesced_sections == FALSE)
		    continue;
		if(force_flat_namespace == FALSE)
		    flat_reference = (image->mh->flags &
				      MH_TWOLEVEL) != MH_TWOLEVEL;
		else
		    flat_reference = TRUE;
		if(flat_reference == FALSE)
		    continue;
		linkedit_segment = image->linkedit_segment;
		st = image->st;
		dyst = image->dyst;
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
		dylib_references = (struct dylib_reference *)
		    (image->vmaddr_slide +
		     linkedit_segment->vmaddr +
		     dyst->extrefsymoff -
		     linkedit_segment->fileoff);
		for(j = 0; j < image->dyst->nmodtab; j++){
		    link_state = GET_LINK_STATE(q->images[i].modules[j]);
		    if(link_state != FULLY_LINKED)
			continue;
			dylib_module = dylib_modules + j;
		    for(k = dylib_module->irefsym;
			k < dylib_module->irefsym + dylib_module->nrefsym;
			k++){
			symbol = symbols + dylib_references[k].isym;
			if(dylib_references[k].flags ==
				REFERENCE_FLAG_DEFINED &&
			   (symbol->n_type & N_TYPE) ==
				N_SECT &&
			   is_section_coalesced(image, symbol->n_sect - 1) ==
				TRUE){

			    symbol_name = strings + symbol->n_un.n_strx;
			    number_of_global_coalesced_symbols += 1;

			    /* check being_linked_list of symbols */
			    found = FALSE;
			    for(being_linked = being_linked_list.next;
				being_linked != &being_linked_list;
				being_linked = being_linked->next){
				if(being_linked->image != image &&
				   strcmp(being_linked->name, symbol_name) ==0){
				    found = TRUE;
				    break;
				}
			    }
			    if(found == FALSE){
				add_to_being_linked_list(symbol_name, symbol,
							 image, flat_reference);
			    }
			}
		    }
		}
		total_number_of_global_coalesced_symbols +=
		    number_of_global_coalesced_symbols;
	    }
	    q = q->next_images;
	}while(q != NULL);

	/*
	 * If there were no global coalesed symbols in two-level namespace
	 * images then nothing should need to be done.
	 */
	if(total_number_of_global_coalesced_symbols == 0)
	    return;

	/*
	 * Get the symbol pointers relocated to the coalesced symbols being
	 * used.  For the prebound case the only object on the object_images
	 * list is just the executable which is the first one.
	 */
	image = &object_images.images[0].image;
	relocate_symbol_pointers_in_object_image(image);

	q = &library_images;
	do{
	    for(i = 0; i < q->nimages; i++){
		image = &(q->images[i].image);
		relocate_symbol_pointers_in_library_image(image);
	    }
	    q = q->next_images;
	}while(q != NULL);

	/*
	 * For each coalesced symbol do each relocation entry that refers to it
	 * in each image.
	 */
	for(being_linked = being_linked_list.next;
	    being_linked != &being_linked_list;
	    being_linked = being_linked->next){

	    /*
	     * First do any relocation for this coalesced symbol in the
	     * executable if the coalesced symbol being used in not from the
	     * executable's image.
	     */
	    image = &object_images.images[0].image;
	    if(being_linked->image != image &&
	       (force_flat_namespace == TRUE ||
		(image->mh->flags & MH_TWOLEVEL) != MH_TWOLEVEL)){
		linkedit_segment = image->linkedit_segment;
		st = image->st;
		dyst = image->dyst;
		if(linkedit_segment != NULL && st != NULL && dyst != NULL){
		    symbols = (struct nlist *)
			(linkedit_segment->vmaddr +
			 st->symoff -
			 linkedit_segment->fileoff);
		    nlist_bsearch_strings = (char *)
			(linkedit_segment->vmaddr +
			 st->stroff -
			 linkedit_segment->fileoff);
		    symbol = inline_bsearch_nlist(being_linked->name,
			   symbols + dyst->iextdefsym,
			   dyst->nextdefsym);
		    if(symbol != NULL){
			if(image->mh->flags & MH_BINDATLOAD)
			    symbol = nlist_linear_search(being_linked->name,
				symbols + dyst->iundefsym, dyst->nundefsym,
				nlist_bsearch_strings);
			else
			    symbol = inline_bsearch_nlist(being_linked->name,
				symbols + dyst->iundefsym, dyst->nundefsym);
		    }
		    if(symbol != NULL){
			isym = symbol - symbols;
			relocs = (struct relocation_info *)
			    (linkedit_segment->vmaddr +
			     dyst->extreloff -
			     linkedit_segment->fileoff);
			if(image->change_protect_on_reloc)
			    make_image_writable(image, "object");
			for(i = 0; i < dyst->nextrel; i++){
			    if(isym == relocs[i].r_symbolnum){
				undo_external_relocation(
				    TRUE, /* undo_prebinding */
				    image,
				    relocs + i,
				    1,
				    symbols,
				    nlist_bsearch_strings,
				    NULL, /* library_name */
				    image->name);
				(void)external_relocation(
				    image,
				    relocs + i,
				    1,
				    symbols,
				    nlist_bsearch_strings,
				    NULL, /* library_name */
				    image->name);
			    }
			}
			if(image->change_protect_on_reloc)
			    restore_image_vm_protections(image, "object");
		    }
		}
	    }

	    /*
	     * Now do any relocation for this coalesced symbol in the each
	     * library image if the coalesced symbol being used in not from the
	     * library's image.
	     */
	    q = &library_images;
	    do{
		for(i = 0; i < q->nimages; i++){
		    image = &(q->images[i].image);
		    if(being_linked->image != image &&
		       (force_flat_namespace == TRUE ||
			(image->mh->flags & MH_TWOLEVEL) != MH_TWOLEVEL)){
			linkedit_segment = image->linkedit_segment;
			st = image->st;
			dyst = image->dyst;
			tocs = (struct dylib_table_of_contents *)
			    (image->vmaddr_slide +
			     linkedit_segment->vmaddr +
			     dyst->tocoff -
			     linkedit_segment->fileoff);
			toc_bsearch_symbols = (struct nlist *)
			    (image->vmaddr_slide +
			     linkedit_segment->vmaddr +
			     st->symoff -
			     linkedit_segment->fileoff);
			toc_bsearch_strings = (char *)
			    (image->vmaddr_slide +
			     linkedit_segment->vmaddr +
			     st->stroff -
			     linkedit_segment->fileoff);
			toc = inline_bsearch_toc(being_linked->name, tocs,
						 dyst->ntoc);
			if(toc != NULL){
			    /* TODO: could have N_INDR in the defined symbols */
			    symbol = toc_bsearch_symbols + toc->symbol_index;
			    isym = toc->symbol_index;
			}
			else{
			    nlist_bsearch_strings = toc_bsearch_strings;
			    symbol = inline_bsearch_nlist(being_linked->name,
					 toc_bsearch_symbols + dyst->iundefsym,
					 dyst->nundefsym);
			    if(symbol == NULL)
				continue;
			    isym = symbol - toc_bsearch_symbols;
			}
			strings = toc_bsearch_strings;
			symbols = toc_bsearch_symbols;
			dylib_modules = (struct dylib_module *)
			    (image->vmaddr_slide +
			     linkedit_segment->vmaddr +
			     dyst->modtaboff -
			     linkedit_segment->fileoff);
			relocs = (struct relocation_info *)
			    (image->vmaddr_slide +
			     linkedit_segment->vmaddr +
			     dyst->extreloff -
			     linkedit_segment->fileoff);
			if(image->change_protect_on_reloc)
			    make_image_writable(image, "object");
			for(j = 0; j < image->dyst->nmodtab; j++){
			    link_state =
				GET_LINK_STATE(q->images[i].modules[j]);
			    if(link_state != FULLY_LINKED)
				continue;
			    dylib_module = dylib_modules + j;
			    for(k = dylib_modules[j].iextrel; 
				k < dylib_modules[j].nextrel;
				k++){
				if(isym == relocs[k].r_symbolnum){
				    undo_external_relocation(
					TRUE, /* undo_prebinding */
					image,
					relocs + k,
					1,
					symbols,
					strings,
					image->name,
					strings + dylib_modules[j].module_name);
				    (void)external_relocation(
					image,
					relocs + k,
					1,
					symbols,
					strings,
					image->name,
					strings + dylib_modules[j].module_name);
				}
			    }
			}
			if(image->change_protect_on_reloc)
			    restore_image_vm_protections(image, "object");
		    }
		}
		q = q->next_images;
	    }while(q != NULL);
	}

	/* clean up the being linked list */
	clear_being_linked_list(FALSE);
}

/*
 * initialize_symbol_block() puts the specified symbol_block on free_list.
 */
static
void
initialize_symbol_block(
struct symbol_block *symbol_block)
{
    unsigned long i;
    struct symbol_list *undefineds;

	undefineds = symbol_block->symbols;

	free_list.next = &undefineds[0];
	undefineds[0].prev = &free_list;
	undefineds[0].next = &undefineds[1];
	undefineds[0].symbol = NULL;
	undefineds[0].image = NULL;
	for(i = 1 ; i < NSYMBOL_LISTS - 1 ; i++){
	    undefineds[i].prev  = &undefineds[i-1];
	    undefineds[i].next  = &undefineds[i+1];
	    undefineds[i].symbol = NULL;
	    undefineds[i].image = NULL;
	}
	free_list.prev = &undefineds[i];
	undefineds[i].prev = &undefineds[i-1];
	undefineds[i].next = &free_list;
	undefineds[i].symbol = NULL;
	undefineds[i].image = NULL;

	symbol_block->initialized = TRUE;
}

/*
 * new_symbol_list() takes a symbol list item off the free list and returns it.
 * If there are no more items on the free list it allocates a new block of them
 * links them on to the free list and returns one.
 */
static
struct symbol_list *
new_symbol_list(
void)
{
    struct symbol_block *p, *q;
    struct symbol_list *new;

	if(free_list.next == &free_list){
	    if(symbol_blocks[0].initialized == FALSE){
		symbol_blocks_used++;
		q = symbol_blocks;
	    }
	    else{
		for(p = symbol_blocks; p->next != NULL; p = p->next)
		    ;
		if(symbol_blocks_used < NSYMBOL_BLOCKS){
		    q = symbol_blocks + symbol_blocks_used;
		    symbol_blocks_used++;
		}
		else{
		    q = allocate(sizeof(struct symbol_block));
		}
		p->next = q;
		q->next = NULL;
	    }
	    initialize_symbol_block(q);
	}
	/* take the first one off the free list */
	new = free_list.next;
	new->next->prev = &free_list;
	free_list.next = new->next;

	return(new);
}

/*
 * add_to_undefined_list() adds an item to the list of undefined symbols.
 */
static
void
add_to_undefined_list(
char *name,
struct nlist *symbol,
struct image *image,
enum bool bind_fully,
enum bool flat_reference)
{
    struct symbol_list *undefined;
    struct symbol_list *new;

	for(undefined = undefined_list.next;
	    undefined != &undefined_list;
	    undefined = undefined->next){
	    if(undefined->name == name){
		if(bind_fully == TRUE)
		    undefined->bind_fully = bind_fully;
		if(flat_reference == TRUE)
		    undefined->flat_reference = flat_reference;
		return;
	    }
	}

	/* get a new symbol list entry */
	new = new_symbol_list();

	/* fill in the pointers for the undefined symbol */
	new->name = name;
	new->symbol = symbol;
	new->image = image;
	new->remove_on_error = return_on_error;
	new->bind_fully = bind_fully;
	new->flat_reference = flat_reference;

	/* put this at the end of the undefined list */
	new->prev = undefined_list.prev;
	new->next = &undefined_list;
	undefined_list.prev->next = new;
	undefined_list.prev = new;
}

/*
 * add_to_being_linked_list() adds an item to the list of being linked symbols.
 */
static
void
add_to_being_linked_list(
char *name,
struct nlist *symbol,
struct image *image,
enum bool flat_reference)
{
    struct symbol_list *new;

	/* get a new symbol list entry */
	new = new_symbol_list();

	/* fill in the pointers for the being_linked symbol */
	new->name = name;
	new->symbol = symbol;
	new->image = image;
	new->remove_on_error = return_on_error;
	new->bind_fully = FALSE;
	new->flat_reference = flat_reference;

	/* put this at the end of the being_linked list */
	new->prev = being_linked_list.prev;
	new->next = &being_linked_list;
	being_linked_list.prev->next = new;
	being_linked_list.prev = new;
}

/*
 * clear_being_linked_list() clear off the items on the list of being linked
 * symbols and returns then to the free list.  If only_remove_on_error is TRUE
 * then only items marked remove_on_error are cleared from the list.
 */
void
clear_being_linked_list(
enum bool only_remove_on_error)
{
    struct symbol_list *old, *old_next;

	for(old = being_linked_list.next;
	    old != &being_linked_list;
	    /* no increment expression */){
	    if(only_remove_on_error == FALSE ||
	       old->remove_on_error == TRUE){

		/* take off the list */
		old->next->prev = &being_linked_list;
		old_next = old->next;

		/* put this at the end of the free_list list */
		old->prev = free_list.prev;
		old->next = &free_list;
		free_list.prev->next = old;
		free_list.prev = old;

		/* clear the pointers */
		old->name = NULL;
		old->symbol = NULL;
		old->image = NULL;
		old->remove_on_error = FALSE;
		old->bind_fully = FALSE;
		old->flat_reference = FALSE;
		old = old_next;
	    }
	    /*
	     * NSLinkModule() could be called with the RETURN_ON_ERROR option
	     * from an undefined symbol handler, in that case a symbol that was
	     * on the undefined list (not marked as remove on error) could get
	     * moved to the being linked list.  So if we are clearing symbols
	     * from the being link list in this case any symbol not marked as
	     * remove on error gets put back on the undefined list.
	     */
	    else if(only_remove_on_error == TRUE &&
	            old->remove_on_error == FALSE){
		/* take off the list */
		old->next->prev = &being_linked_list;
		old_next = old->next;

		/* put this at the end of the undefined_list list */
		old->prev = undefined_list.prev;
		old->next = &undefined_list;
		undefined_list.prev->next = old;
		undefined_list.prev = old;
	    }
	}
	if(only_remove_on_error == TRUE &&
	   being_linked_list.prev != &being_linked_list)
	    return;

	being_linked_list.name = NULL;
	being_linked_list.symbol = NULL;
	being_linked_list.image = NULL;
	being_linked_list.prev = &being_linked_list;
	being_linked_list.next = &being_linked_list;
}

/*
 * clear_undefined_list() clear off the items on the list of undefined
 * symbols and returns then to the free list.  If only_remove_on_error is TRUE
 * then only items marked remove_on_error are cleared from the list.
 */
void
clear_undefined_list(
enum bool only_remove_on_error)
{
    struct symbol_list *old, *old_next;

	for(old = undefined_list.next;
	    old != &undefined_list;
	    /* no increment expression */){

	    if(only_remove_on_error == FALSE ||
	       old->remove_on_error == TRUE){
		/* take off the list */
		old->next->prev = &undefined_list;
		old_next = old->next;

		/* put this at the end of the free_list list */
		old->prev = free_list.prev;
		old->next = &free_list;
		free_list.prev->next = old;
		free_list.prev = old;

		/* clear the pointers */
		old->name = NULL;
		old->symbol = NULL;
		old->image = NULL;
		old->remove_on_error = FALSE;
		old->bind_fully = FALSE;
		old->flat_reference = FALSE;
		old = old_next;
	    }
	}
	if(only_remove_on_error == TRUE && 
	   undefined_list.prev != &undefined_list)
	    return;

	undefined_list.name = NULL;
	undefined_list.symbol = NULL;
	undefined_list.image = NULL;
	undefined_list.prev = &undefined_list;
	undefined_list.next = &undefined_list;
}

/*
 * resolve undefined symbols by searching symbol table and linking library
 * modules as needed.  bind_now is TRUE only when fully binding an image,
 * the normal case when lazy binding is to occur it is FALSE.
 *
 * When trying to launch with prebound libraries,
 * launching_with_prebound_libraries is TRUE, then this is passed to
 * link_library_module() so multiply defined symbols do not cause an error but
 * causes link_library_module() to return FALSE which in turn causes this
 * routine to return FALSE otherwise it returns TRUE.
 *
 * When return_on_error is set, we save the current states of library modules
 * for any library for which we are going to link in a module from.  Then if
 * link_library_module() ever returns false we restore the states of the library
 * modules and return FALSE from this routine.  Else if everything is successful
 * we return TRUE.
 */
enum bool
resolve_undefineds(
enum bool bind_now,
enum bool launching_with_prebound_libraries)
{
    struct symbol_list *undefined, *next_undefined, *defined;
    struct nlist *symbol;
    module_state *module;
    struct image *image;
    enum link_state link_state;
    struct library_image *library_image;
    unsigned long module_index;
    enum bool r, bind_fully;
    struct object_image *object_image;

	for(undefined = undefined_list.next;
	    undefined != &undefined_list;
	    /* no increment expression */){
	    bind_fully = undefined->bind_fully;
	    lookup_symbol(undefined->name,
		get_primary_image(undefined->image, undefined->symbol),
		get_hint(undefined->image, undefined->symbol),
		get_weak(undefined->symbol),
		&symbol, &module, &image, &library_image, NO_INDR_LOOP);
	    if(symbol != NULL){
		/*
		 * The symbol was found so remove it from the undefined_list
		 * and add it to the being_linked_list so that any symbol
		 * pointers for this symbol can be set to this symbol's value.
		 */
		/* take this off the undefined list */
		next_undefined = undefined->next;
		undefined->prev->next = undefined->next;
		undefined->next->prev = undefined->prev;
		defined = undefined;
		undefined = next_undefined;

		/* fill in the pointers for the defined symbol */
		/*
		 * If the looked up symbol returned is not the "weak symbol"
		 * then change the name field for the defined symbol_list to
		 * point the string in the image with the defined symbol.
		 * Otherwise leave it pointing into the referenced image.
		 */
		if(symbol != &some_weak_symbol){
		    defined->name = (char *)
			(image->vmaddr_slide +
			 image->linkedit_segment->vmaddr +
			 image->st->stroff -
			 image->linkedit_segment->fileoff) +
			symbol->n_un.n_strx;
		}
		defined->symbol = symbol;
		defined->image = image;

		/* put this at the end of the being_linked list */
		defined->prev = being_linked_list.prev;
		defined->next = &being_linked_list;
		being_linked_list.prev->next = defined;
		being_linked_list.prev = defined;
	
		/*
		 * If we are doing return on error save the state of the modules
		 * in this library or object if not already saved.  If the
		 * library is marked remove_on_error then there is no need to
		 * save its state.
		 */
		if(return_on_error == TRUE){
		    if(library_image != NULL){
			if(library_image->remove_on_error == FALSE &&
			   library_image->module_states_saved == FALSE){
			    if(library_image->saved_module_states == NULL){
				library_image->saved_module_states =
				    allocate(library_image->nmodules *
					     sizeof(module_state));
			    }
			    memcpy(library_image->saved_module_states,
				   library_image->modules,
				   library_image->nmodules *
					sizeof(module_state));
			    library_image->module_states_saved = TRUE;
			}
		    }
		    else{
			object_image = find_object_image(image);
			if(object_image->module_state_saved == FALSE){
			    object_image->saved_module_state =
				object_image->module;
			    object_image->module_state_saved = TRUE;
			}
		    }
		}

		/*
		 * If the module that defineds this symbol is not linked or
		 * being linked then link the module.
		 */
		link_state = GET_LINK_STATE(*module);
		/* TODO: figure out what to do with REPLACED modules */
		if(link_state == PREBOUND_UNLINKED){
		    undo_prebinding_for_library_module(
			module,
			image,
			library_image);
		    module_index = module - library_image->modules;
		    SET_LINK_STATE(*module, UNLINKED);
		    /*
		     * If we are doing return_on_error and this library may need
		     * its state restore if we hit a later error set the state
		     * to be restore to UNLINKED as we do not need to back out
		     * going from PREBOUND_UNLINKED to UNLINKED.
		     */
		    if(return_on_error == TRUE &&
		       library_image->saved_module_states != NULL)
			SET_LINK_STATE(library_image->saved_module_states
				       [module_index], UNLINKED);

		    /* link this library module */
		    r = link_library_module(library_image, image, module,
			    bind_now, bind_fully,
			    launching_with_prebound_libraries);
		    if(return_on_error == TRUE && r == FALSE)
			goto back_out_changes;
		}
		else if(link_state == UNLINKED){
		    /* link this library module */
		    r = link_library_module(library_image, image, module,
			    bind_now, bind_fully,
			    launching_with_prebound_libraries);
		    if(launching_with_prebound_libraries == TRUE && r == FALSE)
			return(FALSE);
		    if(return_on_error == TRUE && r == FALSE)
			goto back_out_changes;
		}
		else if(bind_now == TRUE &&
			library_image != NULL &&
			link_state == LINKED){
		    /* cause this library module to be fully linked */
		    r = link_library_module(library_image, image, module,
			    bind_now, bind_fully,
			    launching_with_prebound_libraries);
		    if(return_on_error == TRUE && r == FALSE)
			goto back_out_changes;
		}
		else if(bind_fully == TRUE &&
			GET_FULLYBOUND_STATE(*module) == 0){
		    if(library_image != NULL){
			/* cause this library module to be fully bound */
			r = link_library_module(library_image, image, module,
				TRUE, bind_fully,
				launching_with_prebound_libraries);
			if(return_on_error == TRUE && r == FALSE)
			    goto back_out_changes;
		    }
		    else{
			/* cause this object module to be fully bound */
			object_image = find_object_image(image);
			r = link_object_module(object_image, TRUE, TRUE);
			if(return_on_error == TRUE && r == FALSE)
			    goto back_out_changes;
		    }
		}
		if(undefined == &undefined_list &&
		   undefined->next != &undefined_list)
		    undefined = undefined->next;
	    }
	    else{
		undefined = undefined->next;
	    }
	}
	/* return to the caller indicating we succeeded */
	return(TRUE);

back_out_changes:
	/*
	 * Back out state changes to the modules that were changed.
	 */
	clear_state_changes_to_the_modules();

	/*
	 * Clear out symbols added to the being linked list and undefined list
	 * after return_on_error was set.
	 */
	clear_being_linked_list(TRUE);
	clear_undefined_list(TRUE);

	/* return to the caller indicating we failed */
	return(FALSE);
}

/*
 * clear_module_states_saved() is called before linking when return_on_error 
 * is set to TRUE while linking.  This is so that resolve_undefineds() will
 * know that the state of a module it is going to change has not yet been saved
 * so it will save it before it changes it so it can be backed out if needed.
 */
void
clear_module_states_saved(
void)
{
    unsigned long i;
    struct object_images *p;
    struct library_images *q;

	for(p = &object_images ; p != NULL; p = p->next_images){
	    for(i = 0; i < p->nimages; i++){
		p->images[i].module_state_saved = FALSE;
	    }
	}
	for(q = &library_images; q != NULL; q = q->next_images){
	    for(i = 0; i < q->nimages; i++){
		q->images[i].module_states_saved = FALSE;
	    }
	}
}

/*
 * clear_state_changes_to_the_modules() back's out state changes to the
 * modules resolve_undefineds() made when return_on_error was set.
 */
void
clear_state_changes_to_the_modules(
void)
{
    unsigned long i, j;
    struct object_images *q;
    struct library_images *p;

	for(q = &object_images; q != NULL; q = q->next_images){
	    for(i = 0; i < q->nimages; i++){
		if(q->images[i].module_state_saved == TRUE){
		    SET_LINK_STATE(q->images[i].module,
				   q->images[i].saved_module_state);
		    if(GET_FULLYBOUND_STATE(
		       q->images[i].saved_module_state) == 0)
			CLEAR_FULLYBOUND_STATE(q->images[i].module);
		    q->images[i].module_state_saved = FALSE;
		}
	    }
	}

	for(p = &library_images; p != NULL; p = p->next_images){
	    for(i = 0; i < p->nimages; i++){
		if(p->images[i].remove_on_error == FALSE &&
		   p->images[i].module_states_saved == TRUE){
		    for(j = 0; j < p->images[i].nmodules ; j++){
			SET_LINK_STATE(p->images[i].modules[j],
				       p->images[i].saved_module_states[j]);
			if(GET_FULLYBOUND_STATE(
			   p->images[i].saved_module_states[j]) == 0)
			    CLEAR_FULLYBOUND_STATE(p->images[i].modules[j]);
		    }
		    p->images[i].module_states_saved = FALSE;
		}
	    }
	}
}

/*
 * are_symbols_coalesced() is passed two pointers to images and symbols in those
 * images.  If both symbols are coalesced symbols then TRUE is returned else
 * FALSE is returned.
 */
static
enum bool
are_symbols_coalesced(
struct image *image1,
struct nlist *symbol1,
struct image *image2,
struct nlist *symbol2)
{
	if((symbol1->n_type & N_TYPE) != N_SECT ||
	   (symbol2->n_type & N_TYPE) != N_SECT)
	    return(FALSE);

	if(is_section_coalesced(image1, symbol1->n_sect - 1) == FALSE)
	    return(FALSE);
	if(is_section_coalesced(image2, symbol2->n_sect - 1) == FALSE)
	    return(FALSE);
	return(TRUE);
}

/*
 * is_symbol_coalesced() is passed a pointer to an image and a symbol in that
 * image.  If the symbol is a coalesced symbol then TRUE is returned else FALSE
 * is returned.
 */
enum bool
is_symbol_coalesced(
struct image *image,
struct nlist *symbol)
{
	if((symbol->n_type & N_TYPE) != N_SECT)
	    return(FALSE);
	return(is_section_coalesced(image, symbol->n_sect - 1));
}

/*
 * is_symbol_coalesced_and_weak() is passed a pointer to an image and a symbol
 * in that image.  If the symbol is a coalesced weak symbol then TRUE is
 * returned else FALSE is returned.
 */
static
enum bool
is_symbol_coalesced_and_weak(
struct image *image,
struct nlist *symbol)
{
	if((symbol->n_type & N_TYPE) != N_SECT)
	    return(FALSE);
	if(is_section_coalesced(image, symbol->n_sect - 1) == FALSE)
	    return(FALSE);
	if((symbol->n_desc & N_WEAK_DEF) != N_WEAK_DEF)
	    return(FALSE);
	return(TRUE);
}

/*
 * is_section_coalesced() is passed a pointer to an image and a section number
 * in that image.  If the section is a coalesced section then TRUE is returned
 * else FALSE is returned.
 */
static
enum bool
is_section_coalesced(
struct image *image,
unsigned int nsect)
{
    unsigned long i, j;
    struct load_command *lc, *load_commands;
    struct segment_command *sg;
    struct section *s;

	load_commands = (struct load_command *)
			((char *)image->mh + sizeof(struct mach_header));
	lc = load_commands;
	j = 0;
	for(i = 0; i < image->mh->ncmds; i++){
	    switch(lc->cmd){
	    case LC_SEGMENT:
		sg = (struct segment_command *)lc;
		if(nsect - j < sg->nsects){
		    s = (struct section *)
			((char *)sg + sizeof(struct segment_command));
		    if((s[nsect - j].flags & SECTION_TYPE) == S_COALESCED)
			return(TRUE);
		    else
			return(FALSE);
		}
		j += sg->nsects;
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
	return(FALSE);
}

/*
 * link_library_module() links in the specified library module. It checks the
 * module for symbols that are already defined and reports multiply defined
 * errors.  Then it adds it's non-lazy undefined symbols to the undefined
 * list. bind_now is TRUE only when fully binding a library module, the normal
 * case when lazy binding is to occur it is FALSE.
 * 
 * When trying to launch with prebound libraries,
 * launching_with_prebound_libraries is TRUE, then multiply defined symbols do
 * not cause an error but causes this routine to return FALSE otherwise it
 * returns TRUE.
 * 
 * When return_on_error is set, if we get a multiply defined symbol we report it
 * and return without adding any symbols to the undefined list and return FALSE.
 * Else we return TRUE indicating success to our caller.
 */
enum bool
link_library_module(
struct library_image *library_image,
struct image *image,
module_state *module,
enum bool bind_now,
enum bool bind_fully,
enum bool launching_with_prebound_libraries)
{
    unsigned long i, j;
    char *symbol_name;

    struct segment_command *linkedit_segment;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    struct nlist *symbols;
    char *strings;
    struct dylib_module *dylib_modules, *dylib_module;
    unsigned long module_index;
    enum link_state link_state;

    struct nlist *prev_symbol;
    module_state *prev_module;
    struct image *prev_image;
    struct library_image *prev_library_image;
    enum link_state prev_link_state;
    struct segment_command *prev_linkedit_segment;
    struct symtab_command *prev_st;
    struct dysymtab_command *prev_dyst;
    struct nlist *prev_symbols;
    char *prev_strings, *prev_library_name, *prev_module_name;
    struct dylib_module *prev_dylib_modules, *prev_dylib_module;
    unsigned long prev_module_index;

    struct dylib_reference *dylib_references;
    enum link_state ref_link_state;
    enum bool found;
    struct symbol_list *being_linked;
    enum bool private_refs;
    enum bool flat_reference;

	linkedit_segment = image->linkedit_segment;
	st = image->st;
	dyst = image->dyst;
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
	dylib_module = dylib_modules + module_index;

#ifdef FULLY_BIND_MOD_INIT_ROUTINES
	/*
	 * If this module has any module init or term routines force the module
	 * to be fully bound.
	 */
	if(bind_fully == FALSE && dylib_module->ninit_nterm != 0){
	    bind_fully = TRUE;
	    bind_now = TRUE;
	}
#endif /* FULLY_BIND_MOD_INIT_ROUTINES */

	if(force_flat_namespace == FALSE)
	    flat_reference = (image->mh->flags & MH_TWOLEVEL) != MH_TWOLEVEL;
	else
	    flat_reference = TRUE;

	/*
	 * If this module is in the LINKED state or BEING_LINKED state then we
	 * were called to fully link this module.  Since it was already linked
	 * we can skip checking its defined symbols for being multiply defined
	 * as that was done when it was first linked.
	 */
	link_state = GET_LINK_STATE(*module);
	if(link_state == LINKED || link_state == BEING_LINKED)
	    goto add_undefineds;

	/*
	 * If we are not forcing the flat namespace and this is a two-level
	 * namespace image its defined symbols can't cause any multiply defined 
	 * so we can skip checking for them and go on to adding undefined
	 * symbols.
	 */
	if(force_flat_namespace == FALSE &&
	   (image->mh->flags & MH_TWOLEVEL) == MH_TWOLEVEL){
	    /*
	     * If this image has global coalesced symbols they must be added to
	     * the being linked list even though it is defined in this module
	     * because all references to coalesced symbols are done indirectly
	     * and the indirect symbol pointers must get filled in.
	     */
	    if(image->has_coalesced_sections == TRUE){
		for(i = dylib_module->iextdefsym;
		    i < dylib_module->iextdefsym + dylib_module->nextdefsym;
		    i++){

		    if(is_symbol_coalesced(image, symbols + i) == TRUE){
			/* check being_linked_list of symbols */
			found = FALSE;
			for(being_linked = being_linked_list.next;
			    being_linked != &being_linked_list;
			    being_linked = being_linked->next){
			    if(being_linked->symbol == prev_symbol){
				found = TRUE;
				break;
			    }
			}
			if(found == FALSE){
			    symbol_name = strings + symbols[i].n_un.n_strx;
			    add_to_being_linked_list(symbol_name,
						     symbols + i, image,
						     flat_reference);
			}
		    }
		}
	    }
	    goto add_undefineds;
	}

	/*
	 * For each defined symbol check to see if it is not defined in a module
	 * that is already linked (or being linked).
	 */
	for(i = dylib_module->iextdefsym;
	    i < dylib_module->iextdefsym + dylib_module->nextdefsym;
	    i++){
	    symbol_name = strings + symbols[i].n_un.n_strx;
	    lookup_symbol(symbol_name, NULL, NULL, FALSE, &prev_symbol,
			  &prev_module, &prev_image, &prev_library_image,
			  NO_INDR_LOOP);
	    /*
	     * The symbol maybe found in this module which is not an error.
	     * or the symbol may be in a two-level namespace image which is
	     * not an error.
	     */
	    if(prev_symbol != NULL && module != prev_module && 
	       prev_image != NULL &&
	       (force_flat_namespace == TRUE ||
		(prev_image->mh->flags & MH_TWOLEVEL) != MH_TWOLEVEL)){
		prev_link_state = GET_LINK_STATE(*prev_module);

		if(prev_link_state == BEING_LINKED ||
		   prev_link_state == RELOCATED ||
		   prev_link_state == REGISTERING ||
		   prev_link_state == INITIALIZING ||
		   prev_link_state == LINKED  ||
		   prev_link_state == FULLY_LINKED){

		    prev_linkedit_segment = prev_image->linkedit_segment;
		    prev_st = prev_image->st;
		    prev_dyst = prev_image->dyst;
		    prev_symbols = (struct nlist *)
			(prev_image->vmaddr_slide +
			 prev_linkedit_segment->vmaddr +
			 prev_st->symoff -
			 prev_linkedit_segment->fileoff);
		    prev_strings = (char *)
			(prev_image->vmaddr_slide +
			 prev_linkedit_segment->vmaddr +
			 prev_st->stroff -
			 prev_linkedit_segment->fileoff);
		    prev_dylib_modules = (struct dylib_module *)
			(prev_image->vmaddr_slide +
			 prev_linkedit_segment->vmaddr +
			 prev_dyst->modtaboff -
			 prev_linkedit_segment->fileoff);
		    if(prev_library_image != NULL){
			prev_module_index = prev_module -
					    prev_library_image->modules;
			prev_dylib_module = prev_dylib_modules +
					    prev_module_index;
			prev_library_name = prev_image->name;
			prev_module_name = prev_strings +
					   prev_dylib_module->module_name;
		    }
		    else{
			prev_library_name = NULL;
			prev_module_name = prev_image->name;
		    }
		    if((image->has_coalesced_sections == TRUE &&
		        (are_symbols_coalesced(image, symbols + i, 
					prev_image,prev_symbol) == TRUE ||
		        is_symbol_coalesced_and_weak(image, symbols + i))) ||
			(prev_image->has_coalesced_sections == TRUE &&
			 is_symbol_coalesced_and_weak(prev_image,
					prev_symbol) == TRUE) ){
			/* check being_linked_list of symbols */
			found = FALSE;
			for(being_linked = being_linked_list.next;
			    being_linked != &being_linked_list;
			    being_linked = being_linked->next){
			    if(being_linked->symbol == prev_symbol){
				found = TRUE;
				break;
			    }
			}
			/*
			 * If one the previous symbol is a weak coalesced symbol
			 * then use this one.
			 */
			if(prev_image->has_coalesced_sections == TRUE &&
			   is_symbol_coalesced_and_weak(prev_image,
				prev_symbol) == TRUE){
			    /* remove prev_symbol from being_linked,
			       only if the flat_reference matches? */
			    if(found == TRUE){
				/* take this off the being_linked list */
				being_linked->prev->next = being_linked->next;
				being_linked->next->prev = being_linked->prev;

				/* put this at the end of the free_list list */
				being_linked->prev = free_list.prev;
				being_linked->next = &free_list;

				/* clear the pointers */
				being_linked->name = NULL;
				being_linked->symbol = NULL;
				being_linked->image = NULL;
				being_linked->remove_on_error = FALSE;
				being_linked->bind_fully = FALSE;
				being_linked->flat_reference = FALSE;
			    }
			    /* add (image,symbols[i]) to being linked list */
			    /* check being_linked_list of symbols */
			    found = FALSE;
			    for(being_linked = being_linked_list.next;
				being_linked != &being_linked_list;
				being_linked = being_linked->next){
				if(being_linked->symbol == symbols + i){
				    found = TRUE;
				    break;
				}
			    }
			    if(found == FALSE)
				add_to_being_linked_list(symbol_name,
				    symbols + i, image, flat_reference);
			}
			else{
			    if(found == FALSE)
				add_to_being_linked_list(symbol_name,
				    prev_symbol, prev_image, flat_reference);
			}
			continue;
		    }

		    if(launching_with_prebound_libraries == TRUE){
			if(dyld_prebind_debug != 0 && prebinding == TRUE)
			    print("dyld: %s: trying to use prebound libraries "
				   "failed due to multiply defined symbols\n",
				   executables_name);
			prebinding = FALSE;
			return(FALSE);
		    }

		    multiply_defined_error(symbol_name,
					   image,
					   symbols + i,
					   module,
					   image->name,
					   strings + dylib_module->module_name,
					   prev_image,
					   prev_symbol,
					   prev_module,
					   prev_library_name,
					   prev_module_name);
		    /*
		     * We had a multiply defined error so don't link this
		     * module in and let the caller know we failed.
		     */
		    if(return_on_error == TRUE)
			return(FALSE);
		}
		else{
		    /*
		     * If this is a coalesced symbol it must be added to the
		     * being linked list even though it is defined in this
		     * module because all references to coalesced symbols are
		     * done indirectly and the indirect symbol pointers must
		     * get filled in.
		     */
		    if(image->has_coalesced_sections == TRUE &&
		       is_symbol_coalesced(image, symbols + i) == TRUE){
			add_to_being_linked_list(symbol_name, symbols+i, image,
						 flat_reference);
		    }
		}
	    }
	    else{
		/*
		 * If this is a coalesced symbol it must be added to the being
		 * linked list even though it is defined in this module because
		 * all references to coalesced symbols are done indirectly and
		 * the indirect symbol pointers must get filled in.
		 */
		if(image->has_coalesced_sections == TRUE &&
		   is_symbol_coalesced(image, symbols + i) == TRUE){
		    add_to_being_linked_list(symbol_name, symbols + i, image,
					     flat_reference);
		}
	    }
	}

add_undefineds:
	private_refs = FALSE;
	/*
	 * For each reference to a non-lazy undefined add the reference.
	 */
	dylib_references = (struct dylib_reference *)
	    (image->vmaddr_slide +
	     linkedit_segment->vmaddr +
	     dyst->extrefsymoff -
	     linkedit_segment->fileoff);
	for(i = dylib_module->irefsym;
	    i < dylib_module->irefsym + dylib_module->nrefsym;
	    i++){
	    if(dylib_references[i].flags == REFERENCE_FLAG_UNDEFINED_NON_LAZY ||
	       ((executable_bind_at_load || bind_now || bind_fully) && 
	        dylib_references[i].flags == REFERENCE_FLAG_UNDEFINED_LAZY)){
		symbol_name = strings +
			      symbols[dylib_references[i].isym].n_un.n_strx;
		add_reference(symbol_name, symbols + dylib_references[i].isym,
			      image, bind_now, bind_fully);
	    }
	    else if(dylib_references[i].flags ==
				   REFERENCE_FLAG_PRIVATE_UNDEFINED_NON_LAZY ||
		   ((executable_bind_at_load || bind_now || bind_fully) && 
		    dylib_references[i].flags ==
				   REFERENCE_FLAG_PRIVATE_UNDEFINED_LAZY)){
		private_refs = TRUE;
	    }
	}

	/*
	 * Finally change this module's linked state.  If we are not binding
	 * everything in this module now mark this as BEING_LINKED.  If we are
	 * binding everything in this module now and if the module is not
	 * LINKED, again set it to BEING_LINKED, otherwise set it to
	 * FULLY_LINKED.
	 */
	link_state = GET_LINK_STATE(*module);
	if(bind_now == FALSE){
	    if(link_state != LINKED && link_state != FULLY_LINKED)
		SET_LINK_STATE(*module, BEING_LINKED);
	}
	else{
	    if(link_state != LINKED && link_state != FULLY_LINKED)
		SET_LINK_STATE(*module, BEING_LINKED);
	    else
		SET_LINK_STATE(*module, FULLY_LINKED);
	}

	/*
	 * If we are binding this module and all of its dependents fully then
	 * set the fully bound state on this module.
	 */
	if(bind_fully == TRUE)
	    SET_FULLYBOUND_STATE(*module);

	/*
	 * If this module makes references to private symbols in this library
	 * make sure they are linked in to the level needed.
	 */
	if(private_refs == TRUE){
	    for(i = dylib_module->irefsym;
		i < dylib_module->irefsym + dylib_module->nrefsym;
		i++){
		if(dylib_references[i].flags ==
				   REFERENCE_FLAG_PRIVATE_UNDEFINED_NON_LAZY ||
		       ((executable_bind_at_load || bind_now || bind_fully) && 
			dylib_references[i].flags ==
				   REFERENCE_FLAG_PRIVATE_UNDEFINED_LAZY)){
		    for(j = 0; j < dyst->nmodtab; j++){
			if(dylib_references[i].isym >=
			       dylib_modules[j].ilocalsym &&
			   dylib_references[i].isym <
			       dylib_modules[j].ilocalsym +
				   dylib_modules[j].nlocalsym)
			    break;
		    }
		    if(j < dyst->nmodtab){
			ref_link_state =
			    GET_LINK_STATE(library_image->modules[j]);
			if(ref_link_state == PREBOUND_UNLINKED){
			    undo_prebinding_for_library_module(
				library_image->modules + j,
				image,
				library_image);
			    ref_link_state = UNLINKED;
			    SET_LINK_STATE(library_image->modules[j],
					   ref_link_state);
			    /*
			     * If we are doing return_on_error and this library
			     * may need its state restore if we hit a later
			     * error set the state to be restore to UNLINKED as
			     * we do not need to back out going from
			     * PREBOUND_UNLINKED to UNLINKED.
			     */
			    if(return_on_error == TRUE &&
			       library_image->saved_module_states != NULL)
				SET_LINK_STATE(library_image->
					saved_module_states[j], UNLINKED);
			}
			if(ref_link_state == UNLINKED ||
			   (bind_fully == TRUE && 
			    GET_FULLYBOUND_STATE(
				library_image->modules[j]) == 0)){
			    if(link_library_module(
				library_image,
				&(library_image->image),
				library_image->modules + j,
				bind_now,
				bind_fully,
				launching_with_prebound_libraries) == FALSE){
				return(FALSE);
			    }
			}
		    }
		}
	    }
	}

	/*
	 * Since a module from this library was just linked in, we have to make
	 * sure the module for the initialization routine for this library is
	 * fully bound.  This is so it can be called later and that when it is
	 * called it does not lazy bind anything so it and enforce an order with
	 * other library initialization routines if needed.
	 */
	if(library_image->image.rc != NULL){
	    module = library_image->modules +
		     library_image->image.rc->init_module;
	    link_state = GET_LINK_STATE(*module);
	    if(link_state == PREBOUND_UNLINKED){
		undo_prebinding_for_library_module(
		    module,
		    image,
		    library_image);
		SET_LINK_STATE(*module, UNLINKED);
		/*
		 * If we are doing return_on_error and this library may
		 * need it state restore if we hit a later error set
		 * the state to be restore to UNLINKED as we do not need
		 * to back out going from PREBOUND_UNLINKED to UNLINKED.
		 */
		if(return_on_error == TRUE &&
		   library_image->saved_module_states != NULL)
		    SET_LINK_STATE(library_image->saved_module_states[ 
			library_image->image.rc->init_module], UNLINKED);
	    }
	    if(library_image->image.init_bound == FALSE){
	        library_image->image.init_bound = TRUE;
		return(link_library_module(
			    library_image,
			    &(library_image->image),
			    module,
			    TRUE, /* bind_now */
			    TRUE, /* bind_fully */
			    launching_with_prebound_libraries));
	    }
	}

	return(TRUE);
}

/*
 * add_reference() makes sure the reference to the specified symbol is resolved.
 * It is is passed the name of a symbol, and pointers to the symbol table entry
 * and the image the reference is coming from and the bind_now and bind_fully
 * flags.
 */
static
void
add_reference(
char *symbol_name,
struct nlist *symbol,
struct image *image,
enum bool bind_now,
enum bool bind_fully)
{
    struct nlist *ref_symbol;
    module_state *ref_module;
    struct image *ref_image;
    struct library_image *ref_library_image;
    enum link_state ref_link_state;
    enum bool found;
    struct symbol_list *being_linked;
    enum bool flat_reference;

	if(force_flat_namespace == FALSE)
	    flat_reference = (image->mh->flags & MH_TWOLEVEL) != MH_TWOLEVEL;
	else
	    flat_reference = TRUE;
	/*
	 * Lookup the symbol to see if it is defined in an already linked (or
	 * being linked) module.
	 *     If it is then check the list of symbols being linked if the
	 *     symbol needs to be fully_linked and the module is not fully
	 *     put the symbol on the undefined list otherwise put it on the
	 *     being linked list if it is not already there.
	 *
	 *     If it is not then add it to the undefined list.
	 */
	lookup_symbol(symbol_name, get_primary_image(image, symbol),
		      get_hint(image, symbol), get_weak(symbol), &ref_symbol,
		      &ref_module, &ref_image, &ref_library_image,NO_INDR_LOOP);
	if(ref_symbol != NULL){
	    ref_link_state = GET_LINK_STATE(*ref_module);

	    if(ref_link_state == BEING_LINKED ||
	       ref_link_state == RELOCATED ||
	       ref_link_state == REGISTERING ||
	       ref_link_state == INITIALIZING ||
	       ref_link_state == LINKED ||
	       ref_link_state == FULLY_LINKED){

		/* check being_linked_list of symbols */
		found = FALSE;
		for(being_linked = being_linked_list.next;
		    being_linked != &being_linked_list;
		    being_linked = being_linked->next){
		    if(being_linked->symbol == ref_symbol){
			found = TRUE;
			break;
		    }
		}
		if(found == FALSE){
		    if((bind_fully == TRUE &&
		        GET_FULLYBOUND_STATE(*ref_module) == 0) ||
		       (bind_now == TRUE &&
			ref_library_image != NULL &&
			ref_link_state == LINKED) ){
			add_to_undefined_list(symbol_name, ref_symbol,
					 ref_image, bind_fully, flat_reference);
		    }
		    else{
			add_to_being_linked_list(symbol_name, ref_symbol,
						 ref_image, flat_reference);
		    }
		}
		else{
		    if((bind_fully == TRUE &&
		        GET_FULLYBOUND_STATE(*ref_module) == 0) ||
		       (bind_now == TRUE &&
			ref_library_image != NULL &&
			ref_link_state == LINKED) ){

			/* take this off the being_linked list */
			being_linked->prev->next = being_linked->next;
			being_linked->next->prev = being_linked->prev;

			/* put this at the end of the free_list list */
			being_linked->prev = free_list.prev;
			being_linked->next = &free_list;

			/* clear the pointers */
			being_linked->name = NULL;
			being_linked->symbol = NULL;
			being_linked->image = NULL;
			being_linked->remove_on_error = FALSE;
			being_linked->bind_fully = FALSE;
			/* don't loose if this was a flat_reference */
			if(being_linked->flat_reference == TRUE)
			    flat_reference = TRUE;
			being_linked->flat_reference = FALSE;

			add_to_undefined_list(symbol_name, ref_symbol,
					 ref_image, bind_fully, flat_reference);
		    }
		    else if(flat_reference == TRUE &&
			    being_linked->flat_reference == FALSE){
			being_linked->flat_reference = TRUE;
		    }
		}
	    }
	    else{
		add_to_undefined_list(symbol_name, ref_symbol, ref_image,
				      bind_fully, flat_reference);
	    }
	}
	else{
	    add_to_undefined_list(symbol_name, symbol, image, bind_fully,
				  flat_reference);
	}
}

/*
 * link_object_module() links in the specified object module. It checks the
 * module for symbols that are already defined and reports multiply defined
 * errors.  Then it adds it's non-lazy undefined symbols to the undefined
 * list.  And if bind_now is true it also adds lazy undefined symbols to the
 * undefined symbols to the undefined list.  If the module is private then
 * the multiply symbols are not checked for.  If return_on_error is set and
 * there is a multiply defined symbol then FALSE is returned else TRUE is
 * returned.
 */
enum bool
link_object_module(
struct object_image *object_image,
enum bool bind_now,
enum bool bind_fully)
{
    unsigned long i;
    char *symbol_name;

    struct segment_command *linkedit_segment;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    unsigned long *indirect_symtab;
    struct nlist *symbols;
    char *strings;
    enum link_state link_state, saved_link_state;

    struct nlist *prev_symbol;
    module_state *prev_module;
    struct image *prev_image;
    struct library_image *prev_library_image;
    enum link_state prev_link_state;
    struct segment_command *prev_linkedit_segment;
    struct symtab_command *prev_st;
    struct dysymtab_command *prev_dyst;
    struct nlist *prev_symbols;
    char *prev_strings, *prev_library_name, *prev_module_name;
    struct dylib_module *prev_dylib_modules, *prev_dylib_module;
    unsigned long prev_module_index;

    enum bool found;
    struct symbol_list *being_linked;
    enum bool flat_reference;

	linkedit_segment = object_image->image.linkedit_segment;
	st = object_image->image.st;
	dyst = object_image->image.dyst;
	symbols = (struct nlist *)
	    (object_image->image.vmaddr_slide +
	     linkedit_segment->vmaddr +
	     st->symoff -
	     linkedit_segment->fileoff);
	strings = (char *)
	    (object_image->image.vmaddr_slide +
	     linkedit_segment->vmaddr +
	     st->stroff -
	     linkedit_segment->fileoff);
	if(force_flat_namespace == FALSE)
	    flat_reference = (object_image->image.mh->flags &
			      MH_TWOLEVEL) != MH_TWOLEVEL;
	else
	    flat_reference = TRUE;

#ifdef FULLY_BIND_MOD_INIT_ROUTINES
	/*
	 * If this image has any module init or term routines force the image
	 * to be fully bound.
	 */
	if(bind_fully == FALSE && 
	   (object_image->image.init != NULL ||
	    object_image->image.init != NULL)){
	    bind_fully = TRUE;
	    bind_now = TRUE;
	}
#endif /* FULLY_BIND_MOD_INIT_ROUTINES */

	/*
	 * If this module is in the LINKED state or BEING_LINKED state skip
	 * checking for multiply defined symbols and move on to added undefined
	 * symbols.  If this module is in the LINKED state or BEING_LINKED
	 * state then we were called to fully link this module.  Since it was
	 * already linked we can skip checking for its defined symbols for
	 * being multiply defined as that was done when it was first linked.
	 */
	link_state = GET_LINK_STATE(object_image->module);
	if(link_state == LINKED || link_state == BEING_LINKED)
	    goto add_undefineds;

	/*
	 * If this object file image is private or a two-level name space
	 * image we can skip checking for its defined symbols for being
	 * multiply defined.  But the module may still have coaleseced
	 * symbols that need to be delt with.
	 */
	if(object_image->image.private == TRUE ||
	  (force_flat_namespace == TRUE ||
	   (object_image->image.mh->flags & MH_TWOLEVEL) == MH_TWOLEVEL)){
	    if(object_image->image.has_coalesced_sections == FALSE)
		goto add_undefineds;
	    /*
	     * The image is private but could have defined coalesced symbols
	     * for which there are symbol pointers or external relocation
	     * entries. These need to be filled in with the defined coalesced
	     * symbols in this module not the global coaleseced symbol in use.
	     */
	    indirect_symtab = (unsigned long *)
		(object_image->image.vmaddr_slide +
		 linkedit_segment->vmaddr +
		 dyst->indirectsymoff -
		 linkedit_segment->fileoff);
	    for(i = 0; i < dyst->nindirectsyms; i++){
		if(indirect_symtab[i] != INDIRECT_SYMBOL_LOCAL &&
		   indirect_symtab[i] != INDIRECT_SYMBOL_ABS &&
		   is_symbol_coalesced(&(object_image->image),
				       symbols + indirect_symtab[i]) == TRUE){
		    relocate_symbol_pointers(
			indirect_symtab[i],
			indirect_symtab,
			&(object_image->image),
			symbols[indirect_symtab[i]].n_value +
			    object_image->image.vmaddr_slide,
			FALSE);
		}
	    }
	    goto add_undefineds;
	}

	/*
	 * If we are not forcing the flat namespace and this is a two-level
	 * namespace image its defined symbols can't cause any multiply defined
	 * errors so skip checking them and move on to adding this module's
	 * undefined symbols.
	 */
	if(force_flat_namespace == FALSE &&
	   (object_image->image.mh->flags & MH_TWOLEVEL) == MH_TWOLEVEL)
	    goto add_undefineds;

	/*
	 * For flat namespace each defined symbol check to see if it is not
	 * defined in a module that is already linked (or being linked).  To do 
	 * this we set the link state of this object image's module temporarily
	 * to UNUSED so the lookup_symbol() routine will not find the symbols
	 * from this object image.
	 */
	saved_link_state = GET_LINK_STATE(object_image->module);
	SET_LINK_STATE(object_image->module, UNUSED);
	for(i = dyst->iextdefsym;
	    i < dyst->iextdefsym + dyst->nextdefsym;
	    i++){
	    symbol_name = strings + symbols[i].n_un.n_strx;
	    lookup_symbol(symbol_name, NULL, NULL, FALSE,
			  &prev_symbol, &prev_module, &prev_image,
			  &prev_library_image, NO_INDR_LOOP);
	    /*
	     * The symbol maybe found in this module which is not an error.
	     * or the symbol may be in a two-level namespace image which is
	     * not an error.
	     */
	    if(prev_symbol != NULL && &object_image->module != prev_module && 
	       prev_image != NULL &&
	       (force_flat_namespace == FALSE &&
		(prev_image->mh->flags & MH_TWOLEVEL) != MH_TWOLEVEL)){
		prev_link_state = GET_LINK_STATE(*prev_module);

		if(prev_link_state == BEING_LINKED ||
		   prev_link_state == RELOCATED ||
		   prev_link_state == REGISTERING ||
		   prev_link_state == INITIALIZING ||
		   prev_link_state == LINKED ||
		   prev_link_state == FULLY_LINKED){

		    prev_linkedit_segment = prev_image->linkedit_segment;
		    prev_st = prev_image->st;
		    prev_dyst = prev_image->dyst;
		    prev_symbols = (struct nlist *)
			(prev_image->vmaddr_slide +
			 prev_linkedit_segment->vmaddr +
			 prev_st->symoff -
			 prev_linkedit_segment->fileoff);
		    prev_strings = (char *)
			(prev_image->vmaddr_slide +
			 prev_linkedit_segment->vmaddr +
			 prev_st->stroff -
			 prev_linkedit_segment->fileoff);
		    prev_dylib_modules = (struct dylib_module *)
			(prev_image->vmaddr_slide +
			 prev_linkedit_segment->vmaddr +
			 prev_dyst->modtaboff -
			 prev_linkedit_segment->fileoff);
		    if(prev_library_image != NULL){
			prev_module_index = prev_module -
					    prev_library_image->modules;
			prev_dylib_module = prev_dylib_modules +
					    prev_module_index;
			prev_library_name = prev_image->name;
			prev_module_name = prev_strings +
					   prev_dylib_module->module_name;
		    }
		    else{
			prev_library_name = NULL;
			prev_module_name = prev_image->name;
		    }
		    if((object_image->image.has_coalesced_sections == TRUE &&
		        (are_symbols_coalesced(&(object_image->image),
				symbols + i, prev_image, prev_symbol) == TRUE ||
		        is_symbol_coalesced_and_weak(&(object_image->image),
				symbols + i))) ||
			(prev_image->has_coalesced_sections == TRUE &&
			 is_symbol_coalesced_and_weak(prev_image,
					prev_symbol) == TRUE) ){
			/* check being_linked_list of symbols */
			found = FALSE;
			for(being_linked = being_linked_list.next;
			    being_linked != &being_linked_list;
			    being_linked = being_linked->next){
			    if(being_linked->symbol == prev_symbol){
				found = TRUE;
				break;
			    }
			}
			/*
			 * If one the previous symbol is a weak coalesced symbol
			 * then use this one.
			 */
			if(prev_image->has_coalesced_sections == TRUE &&
			   is_symbol_coalesced_and_weak(prev_image,
				prev_symbol) == TRUE){
			    /* remove prev_symbol from being_linked,
			       only if the flat_reference matches? */
			    if(found == TRUE){
				/* take this off the being_linked list */
				being_linked->prev->next = being_linked->next;
				being_linked->next->prev = being_linked->prev;

				/* put this at the end of the free_list list */
				being_linked->prev = free_list.prev;
				being_linked->next = &free_list;

				/* clear the pointers */
				being_linked->name = NULL;
				being_linked->symbol = NULL;
				being_linked->image = NULL;
				being_linked->remove_on_error = FALSE;
				being_linked->bind_fully = FALSE;
				being_linked->flat_reference = FALSE;
			    }
			    /* add (image,symbols[i]) to being linked list */
			    /* check being_linked_list of symbols */
			    found = FALSE;
			    for(being_linked = being_linked_list.next;
				being_linked != &being_linked_list;
				being_linked = being_linked->next){
				if(being_linked->symbol == symbols + i){
				    found = TRUE;
				    break;
				}
			    }
			    if(found == FALSE)
				add_to_being_linked_list(symbol_name,
				    symbols + i, &(object_image->image),
				    flat_reference);
			}
			else{
			    discard_symbol(&(object_image->image), symbols + i);
			    if(found == FALSE)
				add_to_being_linked_list(symbol_name,
				    prev_symbol, prev_image, flat_reference);
			}
			continue;
		    }
		    multiply_defined_error(symbol_name,
					   &(object_image->image),
					   symbols + i,
					   &object_image->module,
					   NULL,
					   object_image->image.name,
					   prev_image,
					   prev_symbol,
					   prev_module,
					   prev_library_name,
					   prev_module_name);
		    /*
		     * Return indicating failure if return_on_error is set for
		     * the first multiply defined error.  Reset the object
		     * image's state back to the saved link state.
		     */
		    if(return_on_error == TRUE){
			SET_LINK_STATE(object_image->module, saved_link_state);
			return(FALSE);
		    }
		}
		else{
		    /*
		     * If this is a coalesced symbol it must be added to the
		     * being linked list even though it is defined in this
		     * module because all references to coalesced symbols are
		     * done indirectly and the indirect symbol pointers must 
		     * get filled in.
		     */
		    if(object_image->image.has_coalesced_sections == TRUE &&
		       is_symbol_coalesced(&(object_image->image),
					   symbols + i) == TRUE){
			add_to_being_linked_list(symbol_name, symbols + i,
						 &(object_image->image),
						 flat_reference);
		    }
		}
	    }
	    else{
		/*
		 * If this is a coalesced symbol it must be added to the
		 * being linked list even though it is defined in this
		 * module because all references to coalesced symbols are
		 * done indirectly and the indirect symbol pointers must 
		 * get filled in.
		 */
		if(object_image->image.has_coalesced_sections == TRUE &&
		   is_symbol_coalesced(&(object_image->image),
				       symbols + i) == TRUE){
		    add_to_being_linked_list(symbol_name, symbols + i,
					     &(object_image->image),
					     flat_reference);
		}
	    }
	}
	/*
	 * Now that the object image's defined symbols have been checked for
	 * multiply defined symbols, reset object image's state back to
	 * the saved link state.
	 */
	SET_LINK_STATE(object_image->module, saved_link_state);

add_undefineds:
	/*
	 * Add a reference for each non-lazy undefined or all undefineds if
	 * everything is to be bound.
	 */
	for(i = dyst->iundefsym; i < dyst->iundefsym + dyst->nundefsym; i++){
	    if(bind_now == TRUE ||
	       bind_fully == TRUE ||
	       executable_bind_at_load == TRUE ||
	       (symbols[i].n_desc & REFERENCE_TYPE) ==
	        REFERENCE_FLAG_UNDEFINED_NON_LAZY){

		symbol_name = strings + symbols[i].n_un.n_strx;
		add_reference(symbol_name, symbols + i, &(object_image->image),
			      bind_now, bind_fully);
	    }
	}

	/*
	 * Finally change this module's linked state.  If we are not fully
	 * binding marked this as being linked.  If we are fully binding and
	 * if the module is not linked again set it to being linked otherwise
	 * set it to fully linked.
	 */
	link_state = GET_LINK_STATE(object_image->module);
	if(bind_now == FALSE){
	    if(link_state != LINKED && link_state != FULLY_LINKED)
		SET_LINK_STATE(object_image->module, BEING_LINKED);
	}
	else{
	    if(link_state != LINKED && link_state != FULLY_LINKED)
		SET_LINK_STATE(object_image->module, BEING_LINKED);
	    else
		SET_LINK_STATE(object_image->module, FULLY_LINKED);
	}

	/*
	 * If we are binding this module and all of its dependents fully then
	 * set the fully bound state on this module.
	 */
	if(bind_fully == TRUE)
	    SET_FULLYBOUND_STATE(object_image->module);

	return(TRUE);
}

/*
 * unlink_object_module() create undefined symbols if any of the defined symbols
 * in the object_image are being referenced.  Also reset any lazy pointers for
 * defined symbols in the object_image if reset_lazy_references is TRUE.
 */
void
unlink_object_module(
struct object_image *object_image,
enum bool reset_lazy_references)
{
    unsigned long i;
    struct segment_command *linkedit_segment;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    struct nlist *symbols, *symbol;
    char *strings, *symbol_name;

    char *reference_name;
    struct nlist *referenced_symbol;
    module_state *referenced_module;
    struct image *referenced_image;
    struct library_image *referenced_library_image;

	linkedit_segment = object_image->image.linkedit_segment;
	st = object_image->image.st;
	dyst = object_image->image.dyst;
	/*
	 * Object images could be loaded that do not have the proper
	 * link edit information.
	 */
	if(linkedit_segment == NULL || st == NULL || dyst == NULL)
	    return;

	symbols = (struct nlist *)
	    (object_image->image.vmaddr_slide +
	     linkedit_segment->vmaddr +
	     st->symoff -
	     linkedit_segment->fileoff);
	strings = (char *)
	    (object_image->image.vmaddr_slide +
	     linkedit_segment->vmaddr +
	     st->stroff -
	     linkedit_segment->fileoff);

	/*
	 * Walk through the list of defined symbols in this module and see
	 * if anything is referencing a defined symbol.  If so add this to
	 * the undefined symbol list.
	 */
	for(i = 0; i < dyst->nextdefsym; i++){
	    symbol = symbols + dyst->iextdefsym + i;
	    if(symbol->n_un.n_strx == 0)
		continue;
	    symbol_name = strings + symbol->n_un.n_strx;

	    reference_name = look_for_flat_reference(symbol_name,
				&referenced_symbol,
    				&referenced_module, &referenced_image,
				&referenced_library_image,
				reset_lazy_references);

	    if(reference_name != NULL){
		add_to_undefined_list(reference_name, referenced_symbol,
				      referenced_image, FALSE, TRUE);
	    }

	    /*
	     * If the reset_lazy_references option is TRUE then reset the
	     * lazy pointers in all images for this symbol.
	     */
	    if(reset_lazy_references == TRUE){
		change_symbol_pointers_in_flat_images(symbol_name,
				(unsigned long)&unlinked_lazy_pointer_handler,
				TRUE /* only_lazy_pointers */);
	    }
	}
}

/*
 * lookup_symbol() looks up the symbol_name and sets pointers to the defintion
 * symbol, module and image.  If the symbol is not found the pointers are set
 * to NULL.  The last argument, indr_loop, should be NULL on initial calls to
 * lookup_symbol().
 *
 * For two-level namespace lookups the primary_image is not NULL and the symbol
 * is only looked up in that image and its sub-images.
 */
void
lookup_symbol(
char *symbol_name,
struct image *primary_image,    /* the primary_image for two-level name space */
struct twolevel_hint *hint,	/* the two-level hint if not NULL */
enum bool weak_symbol,		/* the symbol is allowed to be missing, weak */
struct nlist **defined_symbol,	/* the defined symbol */
module_state **defined_module,	/* the module the symbol is in */
struct image **defined_image,	/* the image the module is in */
struct library_image **defined_library_image,
struct indr_loop_list *indr_loop)
{
    unsigned long i, itoc;
    struct object_images *p;
    struct library_images *q;
    struct library_image *outer_library_image;
    struct object_image *outer_object_image;
    
        DYLD_TRACE_SYMBOLS_NAMED_START(DYLD_TRACE_lookup_symbol, symbol_name);
	/*
	 * This is done first so that if executable_bind_at_load is set it can
	 * set these to the first definition found and keep looking for the
	 * first definition that is in a linked module, and this will provide a 
	 * way to check (*defined_symbol != NULL) to see if a first definition
	 * not yet linked was already found.
	 */ 
	*defined_symbol = NULL;
	*defined_module = NULL;
	*defined_image = NULL;
	*defined_library_image = NULL;

	/*
	 * If primary_image is non-NULL this is a two-level name space lookup.
	 * So look this symbol up in the primary_image and its sub-images
	 * only.
	 */
	if(primary_image != NULL){
	    /*
	     * For weak libraries that are missing the primary image is a
	     * pointer to the image in some_weak_library_image. Every symbol
	     * referenced in this library should be a weak_symbol.
	     */
	    if(primary_image ==
	       (struct image *)&(some_weak_library_image.image)){
		weak_symbol = TRUE;
		goto weak_library_symbol;
	    }
	    if(primary_image->mh->filetype != MH_DYLIB){
		outer_object_image = (struct object_image *)
				     primary_image->outer_image;
		if(lookup_symbol_in_object_image(symbol_name, primary_image,
			&(outer_object_image->module), defined_symbol,
			defined_module, defined_image, defined_library_image,
			indr_loop) == TRUE){
                    DYLD_TRACE_SYMBOLS_END(DYLD_TRACE_lookup_symbol);
		    return;
		}
	    }
	    else{
		/*
		 * First set the index into the table of contents to ULONG_MAX
		 * indicating we don't have a hint.  Then see if we have a hint
		 * that can be used for which of the sub-images to look in
		 * first.
		 */
		itoc = ULONG_MAX;
		if(hint != NULL && primary_image->subs_can_use_hints == TRUE){
		    /*
		     * If the hint is for a sub image (isub_image != 0) then
		     * look in that sub-image otherwise the hint is for the
		     * primary_image itself.
		     */
		    if(hint->isub_image != 0){
			/*
			 * If the hint for the sub image is not greater than the
			 * number of sub images we have a valid hint and we
			 * first search that sub image for this symbol.
			 */
			if((unsigned long)(hint->isub_image - 1) <
			   primary_image->nsub_images){
			    if(primary_image->sub_images[hint->isub_image - 1]->
			       mh->filetype == MH_DYLIB){
				outer_library_image = (struct library_image *)
				    primary_image->sub_images[hint->isub_image -
							       1]->outer_image;
				if(lookup_symbol_in_library_image(symbol_name,
					hint->itoc,
					primary_image->sub_images[
					    hint->isub_image - 1],
					outer_library_image->modules,
					outer_library_image, defined_symbol,
					defined_module, defined_image,
					defined_library_image, indr_loop) ==
									  TRUE){
	
				    if(dyld_hints_debug > 1)
					print("hint for symbol: %s into "
					      "sub-image: %s of image: %s "
					      "worked\n", symbol_name,
					      primary_image->sub_images[
						hint->isub_image - 1]->name,
					      primary_image->name);
                                    DYLD_TRACE_SYMBOLS_END(
					DYLD_TRACE_lookup_symbol);
				    return;
			        }
			    }
			}
			if(dyld_hints_debug > 1)
			    print("hint for symbol: %s into image: %s failed\n",
				  symbol_name, primary_image->name);
		    }
		    else{
			/*
			 * isub_image is 0 which means the hint is for the
			 * primary_image.  So just pick up the hint for the
			 * index into the table of contents and fall into
			 * starting the search with the primary_image.
			 */
			itoc = hint->itoc;
		    }
		}
		outer_library_image = (struct library_image *)
				      primary_image->outer_image;
		if(lookup_symbol_in_library_image(symbol_name, itoc,
			primary_image, outer_library_image->modules,
			outer_library_image, defined_symbol, defined_module,
			defined_image, defined_library_image,indr_loop) ==TRUE){
		    if(dyld_hints_debug > 1 && itoc != ULONG_MAX)
			print("hint for symbol: %s into image: %s worked\n",
			      symbol_name, primary_image->name);
                    DYLD_TRACE_SYMBOLS_END(DYLD_TRACE_lookup_symbol);
		    return;
		}
		if(dyld_hints_debug > 1 && itoc != ULONG_MAX)
		    print("hint for symbol: %s into image: %s failed\n",
			  symbol_name, primary_image->name);
	    }
	    for(i = 0; i < primary_image->nsub_images; i++){
		if(primary_image->sub_images[i]->mh->filetype != MH_DYLIB){
		    outer_object_image = (struct object_image *)
			primary_image->sub_images[i]->outer_image;
		    if(lookup_symbol_in_object_image(symbol_name, 
			    primary_image->sub_images[i],
			    &(outer_object_image->module), defined_symbol,
			    defined_module, defined_image,
			    defined_library_image, indr_loop) == TRUE){
			DYLD_TRACE_SYMBOLS_END(DYLD_TRACE_lookup_symbol);
			return;
		    }
		}
		else{
		    outer_library_image = (struct library_image *)
			primary_image->sub_images[i]->outer_image;
		    if(lookup_symbol_in_library_image(symbol_name, ULONG_MAX,
			    primary_image->sub_images[i],
			    outer_library_image->modules, outer_library_image,
			    defined_symbol, defined_module, defined_image,
			    defined_library_image, indr_loop) == TRUE){
			DYLD_TRACE_SYMBOLS_END(DYLD_TRACE_lookup_symbol);
			return;
		    }
		}
	    }
	    /*
	     * If we get here the symbol was either not found in the
	     * primary_image and its sub-images so it is undefined for a
	     * two-level name space lookup.  Or we were doing a bind at load
	     * operation and the defined_* return values were set to the first
	     * unbound definition.  Or the symbol could have been a weak symbol
	     * that is missing and if so return the constant values for a
	     * weak symbol lookup.
	     */
weak_library_symbol:
	    if(defined_symbol != NULL && weak_symbol == TRUE){
    		*defined_symbol = (struct nlist *)&some_weak_symbol;
    		*defined_module = (module_state *)&some_weak_module;;
		if(primary_image ==
		   (struct image *)&(some_weak_library_image.image)){
		    *defined_image =
			(struct image *)&(some_weak_library_image.image);
		    *defined_library_image =
			(struct library_image *)&some_weak_library_image;
		}
		else{
		    *defined_image = primary_image;
		    if(primary_image->mh->filetype == MH_DYLIB)
			*defined_library_image = (struct library_image *)
			      primary_image->outer_image;
		    else
			*defined_library_image = NULL;
		}
	    }
            DYLD_TRACE_SYMBOLS_END(DYLD_TRACE_lookup_symbol);
	    return;
	}

	/*
	 * For flat namespace lookups first look in object_images for a
	 * definition of symbol_name.
	 */
	p = &object_images;
	do{
	    for(i = 0; i < p->nimages; i++){
		/* If the image is private skip it */
		if(p->images[i].image.private == TRUE)
		    continue;
		/* TODO: skip modules that are replaced */
		if(lookup_symbol_in_object_image(symbol_name,
			&(p->images[i].image), &(p->images[i].module),
			defined_symbol, defined_module, defined_image,
			defined_library_image, indr_loop) == TRUE){
                    DYLD_TRACE_SYMBOLS_END(DYLD_TRACE_lookup_symbol);
		    return;
		}
	    }
	    p = p->next_images;
	}while(p != NULL);

	/*
	 * Next for flat namespace lookups look in the library_images for a
	 * definition of symbol_name.
	 */
	q = &library_images;
	do{
	    for(i = 0; i < q->nimages; i++){
		if(lookup_symbol_in_library_image(symbol_name, ULONG_MAX,
			&(q->images[i].image), q->images[i].modules,
			&(q->images[i]), defined_symbol, defined_module,
			defined_image, defined_library_image, indr_loop) ==
		    TRUE){
                    DYLD_TRACE_SYMBOLS_END(DYLD_TRACE_lookup_symbol);
		    return;
		}
	    }
	    q = q->next_images;
	}while(q != NULL);

	/*
	 * If we aren't doing a executable_bind_at_load and previously found
	 * a symbol definition check to see if this is a weak_symbol that is
	 * missing.  If so return the constant values for a weak symbol lookup.
	 */
	if(defined_symbol != NULL && weak_symbol == TRUE){
	    *defined_symbol = (struct nlist *)&some_weak_symbol;
	    *defined_module = (module_state *)&some_weak_module;;
	    *defined_image = (struct image *)&(some_weak_library_image.image);
	    *defined_library_image =
		(struct library_image *)&some_weak_library_image;
	}
	DYLD_TRACE_SYMBOLS_END(DYLD_TRACE_lookup_symbol);
}

/*
 * get_indr_image() is passed the the indirect name of an N_INDR symbol and the
 * image it came from.  It returns the image to look this indirect name up in.
 * For flat images it returns NULL.  For two-level images it finds the
 * corresponding undefined symbol for the indirect name and returns the primary
 * image that undefined symbol is bound to.
 */
static
struct image *
get_indr_image(
char *symbol_name,
struct image *image)
{
    struct segment_command *linkedit_segment;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    struct nlist *symbols, *symbol;
    char *strings;
    struct dylib_table_of_contents *tocs, *toc;
    unsigned long symbol_index;
    struct image *indr_image;

	/*
	 * If this is a flat image then the indr image is NULL.
	 */
	if(force_flat_namespace == TRUE ||
	   (image->mh->flags & MH_TWOLEVEL) != MH_TWOLEVEL)
	    return(NULL);

	linkedit_segment = image->linkedit_segment;
	st = image->st;
	dyst = image->dyst;
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

	if(image->mh->filetype != MH_DYLIB){
	    nlist_bsearch_strings = strings;
	    symbol = inline_bsearch_nlist(symbol_name,
				symbols + dyst->iundefsym, dyst->nundefsym);
	    /* if this fails we really have a malformed symbol table */
	    if(symbol == NULL)
		return(NULL);
	    indr_image = get_primary_image(image, symbol);
	}
	else{
	    /*
	     * We need the "undefined symbol" in this image for this
	     * symbol_name so we can get the primary image for its lookup.
	     * Since this image is a library the "undefined symbol" maybe
	     * defined in this library but in a different module so first
	     * look in the defined symbols then in the undefined symbols.
	     */
	    tocs = (struct dylib_table_of_contents *)
		(image->vmaddr_slide +
		 linkedit_segment->vmaddr +
		 dyst->tocoff -
		 linkedit_segment->fileoff);
	    toc_bsearch_symbols = symbols;
	    toc_bsearch_strings = strings;
	    toc = inline_bsearch_toc(symbol_name, tocs, dyst->ntoc);
	    if(toc != NULL){
		symbol_index = toc->symbol_index;
	    }
	    else{
		nlist_bsearch_strings = strings;
		symbol = inline_bsearch_nlist(symbol_name,
			   symbols + dyst->iundefsym, dyst->nundefsym);
		/* if this fails we really have a malformed symbol table */
		if(symbol == NULL)
		    return(NULL);
		symbol_index = symbol - symbols;
	    }
	    indr_image = get_primary_image(image, symbols + symbol_index);
	}
	return(indr_image);
}

/*
 * lookup_symbol_in_object_image() is a sub-routine for lookup_symbol().  It
 * looks up the symbol_name in the specified object image and object_module.
 * Its return results depends on the value of executable_bind_at_load.
 *
 * If executable_bind_at_load is FALSE and a definition is found it sets
 * pointers to the defintion symbol, module, image and library_image and returns
 * TRUE.
 * 
 * If executable_bind_at_load is TRUE the caller is expected to set the value
 * of *defined_symbol to NULL before any lookup_symbol sub-routine calls.
 * If executable_bind_at_load is TRUE and definition is found and this object
 * module is bound then it sets the pointers to the defined_* prameters and
 * returns TRUE.  If the definition is found but this object is not bound
 * and *defined_symbol is still NULL then sets the pointers to the defined_*
 * prameters and returns FALSE.  Thus repeated calls will find a bound defintion
 * or the first unbound definition.
 *
 * If no definition is found FALSE is returned and the pointers to the defined_*
 * prameters are unchanged.
 *
 * The last argument, indr_loop, should be NULL on initial calls.
 */
enum bool
lookup_symbol_in_object_image(
char *symbol_name,
struct image *object_image,     /* the object_image to look in */
module_state *object_module,	/* the object_module for the object_image */
struct nlist **defined_symbol,	/* the defined symbol */
module_state **defined_module,	/* the module the symbol is in */
struct image **defined_image,	/* the image the module is in */
struct library_image **defined_library_image,
struct indr_loop_list *indr_loop)
{
    enum link_state link_state;
    struct segment_command *linkedit_segment;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    struct nlist *symbols, *symbol;
    struct indr_loop_list new_indr_loop, *loop, *l;
    char *strings, *module_name;
    struct dylib_module *dylib_modules, *dylib_module;
    unsigned long module_index;
    struct image *indr_image;

	link_state = GET_LINK_STATE(*object_module);
	/* If this object file image is currently unused don't search it */
	if(link_state == UNUSED)
	    return(FALSE);

	linkedit_segment = object_image->linkedit_segment;
	st = object_image->st;
	dyst = object_image->dyst;
	/*
	 * Object images could be loaded that do not have the proper
	 * link edit information.
	 */
	if(linkedit_segment == NULL || st == NULL || dyst == NULL)
	    return(FALSE);
	symbols = (struct nlist *)
	    (object_image->vmaddr_slide +
	     linkedit_segment->vmaddr +
	     st->symoff -
	     linkedit_segment->fileoff);
	nlist_bsearch_strings = (char *)
	    (object_image->vmaddr_slide +
	     linkedit_segment->vmaddr +
	     st->stroff -
	     linkedit_segment->fileoff);
	symbol = inline_bsearch_nlist(symbol_name,
				      symbols + dyst->iextdefsym,
				      dyst->nextdefsym);
	if(symbol != NULL &&
	   (symbol->n_desc & N_DESC_DISCARDED) == 0){
	    if((symbol->n_type & N_TYPE) == N_INDR &&
		indr_loop != NO_INDR_LOOP){
		/*
		 * The symbol found is an N_INDR.  First check to see that
		 * we don't have a loop of symbols, if so report the loop.
		 * Then and then lookup the indirected symbol name.
		 */
		for(loop = indr_loop; loop != NULL; loop = loop->next){
		    if(loop->defined_symbol == symbol &&
		       loop->defined_module == object_module &&
		       loop->defined_image == object_image &&
		       loop->defined_library_image == NULL){
			error("indirect symbol loop:");
			for(l = indr_loop; l != NULL; l = l->next){
			    linkedit_segment =
				l->defined_image->linkedit_segment;
			    st = l->defined_image->st;
			    strings = (char *)
				(l->defined_image->vmaddr_slide +
				 linkedit_segment->vmaddr +
				 st->stroff -
				 linkedit_segment->fileoff);
			    if(l->defined_library_image != NULL){
				dyst = l->defined_image->dyst;
				dylib_modules =
				    (struct dylib_module *)
				    (l->defined_image->vmaddr_slide +
				    linkedit_segment->vmaddr +
				    dyst->modtaboff -
				    linkedit_segment->fileoff);
				module_index = l->defined_module -
				    l->defined_library_image->modules;
				dylib_module = dylib_modules +
					       module_index;
				module_name = strings +
				    dylib_module->module_name;
				add_error_string("%s(%s) definition of "
				    "%s as indirect for %s\n", 
				    l->defined_image->name, module_name,
				    strings +
					l->defined_symbol->n_un.n_strx,
				    strings +
					l->defined_symbol->n_value);
			    }
			    else{
				add_error_string("%s definition of "
				    "%s as indirect for %s\n", 
				    l->defined_image->name,
				    strings +
					l->defined_symbol->n_un.n_strx,
				    strings +
					l->defined_symbol->n_value);
			    }
			    if(l == loop)
				break;
			}
			link_edit_error(DYLD_OTHER_ERROR,
					DYLD_INDR_LOOP, NULL);
			return(FALSE);
		    }
		}
		new_indr_loop.defined_symbol = symbol;
		new_indr_loop.defined_module = object_module;
		new_indr_loop.defined_image = object_image;
		new_indr_loop.defined_library_image = NULL;
		new_indr_loop.next = indr_loop;
		symbol_name = nlist_bsearch_strings + symbol->n_value;
		indr_image = get_indr_image(symbol_name,
					new_indr_loop.defined_image);
		lookup_symbol(symbol_name, indr_image, NULL, FALSE,
			      defined_symbol, defined_module, defined_image,
			      defined_library_image, &new_indr_loop);
		return(defined_symbol != NULL);
	    }
	    else{
		/*
		 * This symbol is defined in the image.  So set the define_*
		 * return values.
		 *
		 *  If we are doing bind at load we want to return the first
		 * defined symbol that is linked or if none then the first
		 * symbol found.  So if defined_symbol == NULL then this it the 
		 * first symbol found and save its info in the defined_* return
		 * values.  So that if no linked symbol is found then that
		 * symbol is used.  If we do find a linked symbol overwrite the
		 * defined_* return values and return TRUE to indicate the
		 * symbol to be used is found.
		 */
		if(executable_bind_at_load != TRUE ||
		   *defined_symbol == NULL){
		    *defined_symbol = symbol;
		    *defined_module = object_module;
		    *defined_image = object_image;
		    *defined_library_image = NULL;
		}
		if(executable_bind_at_load == TRUE){
		    if(link_state == BEING_LINKED ||
		       link_state == RELOCATED ||
		       link_state == REGISTERING ||
		       link_state == INITIALIZING ||
		       link_state == LINKED ||
		       link_state == FULLY_LINKED){
			*defined_symbol = symbol;
			*defined_module = object_module;
			*defined_image = object_image;
			*defined_library_image = NULL;
			return(TRUE);
		    }
		    else
			/*
			 * For bind at load the defined_* return values are set 
			 * and may be used if no other linked symbol is found.
			 */
			return(FALSE);
		}
		return(TRUE);
	    }
	}
	return(FALSE);
}

/*
 * lookup_symbol_in_library_image() is a sub-routine for lookup_symbol().  It
 * looks up the symbol_name in the specified library image and library_modules.
 * Its return results depends on the value of executable_bind_at_load.
 *
 * If executable_bind_at_load is FALSE and a definition is found it sets
 * pointers to the defintion symbol, module, image and library_image and returns
 * TRUE.
 * 
 * If executable_bind_at_load is TRUE the caller is expected to set the value
 * of *defined_symbol to NULL before any lookup_symbol sub-routine calls.
 * If executable_bind_at_load is TRUE and definition is found and this library
 * module is bound then it sets the pointers to the defined_* prameters and
 * returns TRUE.  If the definition is found but this library module is not
 * bound and *defined_symbol is still NULL then sets the pointers to the
 * defined_* prameters and returns FALSE.  Thus repeated calls will find a
 * bound defintion or the first unbound definition.
 *
 * If no definition is found FALSE is returned and the pointers to the defined_*
 * prameters are unchanged.
 *
 * The last argument, indr_loop, should be NULL on initial calls.
 */
static
enum bool
lookup_symbol_in_library_image(
char *symbol_name,
unsigned long itoc,		/* hint of index into the table of contents */
struct image *image,    	/* the library image to look in */
module_state *library_modules,	/* the library_modules for the image */
struct library_image *library_image,  /* the library_image for the image */
struct nlist **defined_symbol,	/* the defined symbol */
module_state **defined_module,	/* the module the symbol is in */
struct image **defined_image,	/* the image the module is in */
struct library_image **defined_library_image,
struct indr_loop_list *indr_loop)
{
    struct segment_command *linkedit_segment;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    struct nlist *symbol;
    struct dylib_table_of_contents *tocs, *toc;
    struct indr_loop_list new_indr_loop, *loop, *l;
    char *strings, *module_name;
    struct dylib_module *dylib_modules, *dylib_module;
    unsigned long module_index;
    enum link_state link_state;
    struct image *indr_image;

	linkedit_segment = image->linkedit_segment;
	st = image->st;
	dyst = image->dyst;

	tocs = (struct dylib_table_of_contents *)
	    (image->vmaddr_slide +
	     linkedit_segment->vmaddr +
	     dyst->tocoff -
	     linkedit_segment->fileoff);
	toc_bsearch_symbols = (struct nlist *)
	    (image->vmaddr_slide +
	     linkedit_segment->vmaddr +
	     st->symoff -
	     linkedit_segment->fileoff);
	toc_bsearch_strings = (char *)
	    (image->vmaddr_slide +
	     linkedit_segment->vmaddr +
	     st->stroff -
	     linkedit_segment->fileoff);
	toc = inline_bsearch_toc_with_index(symbol_name, tocs, dyst->ntoc,itoc);
	/* TODO: skip modules that that are replaced */
	if(toc != NULL &&
	   (toc_bsearch_symbols[toc->symbol_index].n_desc &
	    N_DESC_DISCARDED) == 0){
	    symbol = toc_bsearch_symbols + toc->symbol_index;
	    if((symbol->n_type & N_TYPE) == N_INDR &&
		indr_loop != NO_INDR_LOOP){
		/*
		 * The symbol found is an N_INDR.  First check to see that
		 * we don't have a loop of symbols, if so report the loop.
		 * Then and then lookup the indirected symbol name.
		 */
		for(loop = indr_loop; loop != NULL; loop = loop->next){
		    if(loop->defined_symbol ==
			toc_bsearch_symbols + toc->symbol_index &&
		       loop->defined_module ==
			library_modules + toc->module_index &&
		       loop->defined_image == image &&
		       loop->defined_library_image == library_image){
			error("indirect symbol loop:");
			for(l = indr_loop; l != NULL; l = l->next){
			    linkedit_segment =
				l->defined_image->linkedit_segment;
			    st = l->defined_image->st;
			    strings = (char *)
				(l->defined_image->vmaddr_slide +
				 linkedit_segment->vmaddr +
				 st->stroff -
				 linkedit_segment->fileoff);
			    if(l->defined_library_image != NULL){
				dyst = l->defined_image->dyst;
				dylib_modules =
				    (struct dylib_module *)
				    (l->defined_image->vmaddr_slide +
				    linkedit_segment->vmaddr +
				    dyst->modtaboff -
				    linkedit_segment->fileoff);
				module_index = l->defined_module -
				    l->defined_library_image->modules;
				dylib_module = dylib_modules +
					       module_index;
				module_name = strings +
				    dylib_module->module_name;
				add_error_string("%s(%s) definition of "
				    "%s as indirect for %s\n", 
				    l->defined_image->name, module_name,
				    strings +
					l->defined_symbol->n_un.n_strx,
				    strings +
					l->defined_symbol->n_value);
			    }
			    else{
				add_error_string("%s definition of "
				    "%s as indirect for %s\n", 
				    l->defined_image->name,
				    strings +
					l->defined_symbol->n_un.n_strx,
				    strings +
					l->defined_symbol->n_value);
			    }
			    if(l == loop)
				break;
			}
			link_edit_error(DYLD_OTHER_ERROR,
					DYLD_INDR_LOOP, NULL);
			return(FALSE);
		    }
		}
		new_indr_loop.defined_symbol =
		    toc_bsearch_symbols + toc->symbol_index;
		new_indr_loop.defined_module =
		    library_modules + toc->module_index;
		new_indr_loop.defined_image = image;
		new_indr_loop.defined_library_image = library_image;
		new_indr_loop.next = indr_loop;
		symbol_name = toc_bsearch_strings + symbol->n_value;
		indr_image = get_indr_image(symbol_name,
					    new_indr_loop.defined_image);
		lookup_symbol(symbol_name, indr_image, NULL, FALSE,
			      defined_symbol, defined_module, defined_image,
			      defined_library_image, &new_indr_loop);
		return(defined_symbol != NULL);
	    }
	    else{
		/*
		 * This symbol is defined in the image.  So set the define_*
		 * return values.
		 *
		 *  If we are doing bind at load we want to return the first
		 * defined symbol that is linked or if none then the first
		 * symbol found.  So if defined_symbol == NULL then this it the 
		 * first symbol found and save its info in the defined_* return
		 * values.  So that if no linked symbol is found then that
		 * symbol is used.  If we do find a linked symbol overwrite the
		 * defined_* return values and return TRUE to indicate the
		 * symbol to be used is found.
		 */
		if(executable_bind_at_load != TRUE ||
		   *defined_symbol == NULL){
		    *defined_symbol =
			toc_bsearch_symbols + toc->symbol_index;
		    *defined_module =
			library_modules + toc->module_index;
		    *defined_image = image;
		    *defined_library_image = library_image;
		}
		if(executable_bind_at_load == TRUE){
		    link_state = GET_LINK_STATE(*(library_modules +
						toc->module_index));
		    if(link_state == BEING_LINKED ||
		       link_state == RELOCATED ||
		       link_state == REGISTERING ||
		       link_state == INITIALIZING ||
		       link_state == LINKED ||
		       link_state == FULLY_LINKED){
			*defined_symbol =
			    toc_bsearch_symbols + toc->symbol_index;
			*defined_module =
			    library_modules + toc->module_index;
			*defined_image = image;
			*defined_library_image = library_image;
			return(TRUE);
		    }
		    else
			/*
			 * For bind at load the defined_* return values are set 
			 * and may be used if no other linked symbol is found.
			 */
			return(FALSE);
		}
		return(TRUE);
	    }
	}
	return(FALSE);
}

/*
 * lookup_symbol_in_hinted_library() for a library that has the strstr(3) of
 * the string hinted_library it it.  If found it looks up the symbol_name in
 * that library.  And if that module is bound then it sets pointers to the
 * defintion symbol, module and image.  If the library is not found or the
 * symbol is not found or not in a bound module the pointers are set to NULL.
 * Note: this is used for API's that are flat lookups.
 */
void
lookup_symbol_in_hinted_library(
char *symbol_name,
char *hinted_library,
struct nlist **defined_symbol,	/* the defined symbol */
module_state **defined_module,	/* the module the symbol is in */
struct image **defined_image,	/* the image the module is in */
struct library_image **defined_library_image)
{
    unsigned long i;
    struct library_images *q;
    struct segment_command *linkedit_segment;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    struct nlist *symbol;
    struct dylib_table_of_contents *tocs, *toc;
    enum link_state link_state;

	*defined_symbol = NULL;
	*defined_module = NULL;
	*defined_image = NULL;
	*defined_library_image = NULL;
	/*
	 * If the hinted library is NULL return and force a full lookup.
	 */
	if(hinted_library == NULL)
	    return;

	q = &library_images;
	do{
	    for(i = 0; i < q->nimages; i++){
		if(strstr(q->images[i].image.name, hinted_library) == NULL)
		    continue;
		linkedit_segment = q->images[i].image.linkedit_segment;
		st = q->images[i].image.st;
		dyst = q->images[i].image.dyst;

		tocs = (struct dylib_table_of_contents *)
		    (q->images[i].image.vmaddr_slide +
		     linkedit_segment->vmaddr +
		     dyst->tocoff -
		     linkedit_segment->fileoff);
		toc_bsearch_symbols = (struct nlist *)
		    (q->images[i].image.vmaddr_slide +
		     linkedit_segment->vmaddr +
		     st->symoff -
		     linkedit_segment->fileoff);
		toc_bsearch_strings = (char *)
		    (q->images[i].image.vmaddr_slide +
		     linkedit_segment->vmaddr +
		     st->stroff -
		     linkedit_segment->fileoff);
		toc = inline_bsearch_toc(symbol_name, tocs, dyst->ntoc);
		/* TODO: skip modules that that are replaced */
		if(toc != NULL &&
		   (toc_bsearch_symbols[toc->symbol_index].n_desc &
		    N_DESC_DISCARDED) == 0){
		    symbol = toc_bsearch_symbols + toc->symbol_index;
		    /*
		     * the hint failed it we find an N_INDR symbol as further
		     * lookups are needed.
		     */
		    if((symbol->n_type & N_TYPE) == N_INDR)
			return;

		    link_state = GET_LINK_STATE(*(q->images[i].modules +
						toc->module_index));
		    if(link_state == BEING_LINKED ||
		       link_state == RELOCATED ||
		       link_state == REGISTERING ||
		       link_state == INITIALIZING ||
		       link_state == LINKED ||
		       link_state == FULLY_LINKED){
			*defined_symbol =
			    toc_bsearch_symbols + toc->symbol_index;
			*defined_module =
			    q->images[i].modules + toc->module_index;
			*defined_image = &(q->images[i].image);
			*defined_library_image = &(q->images[i]);
		    }
		}
		/*
		 * We found the hinted library so return if the symbol was
		 * found or not.
		 */
		return;
	    }
	    q = q->next_images;
	}while(q != NULL);
}

/*
 * look_for_flat_reference() looks up in flat namespace images the symbol_name
 * and sets pointers to the reference symbol, module and image.  If no reference
 * is found the pointers are set to NULL.  If it a reference is found then the
 * symbol_name in the referencing image is returned else NULL.  If
 * ignore_lazy_references is TRUE then it returns a non-lazy reference or NULL
 * if there are only lazy references.
 */
static
char *
look_for_flat_reference(
char *symbol_name,
struct nlist **referenced_symbol,	/* the referenced symbol */
module_state **referenced_module,	/* the module the symbol is in */
struct image **referenced_image,	/* the image the module is in */
struct library_image **referenced_library_image,
enum bool ignore_lazy_references)
{
    unsigned long i, j, k, isym;
    struct object_images *p;
    struct library_images *q;
    struct segment_command *linkedit_segment;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    struct nlist *symbols, *symbol;
    struct dylib_table_of_contents *tocs, *toc;
    struct dylib_module *dylib_modules;
    struct dylib_reference *dylib_references;
    char *reference_name;
    enum link_state link_state;

	/*
	 * First look in object_images for a reference of symbol_name.
	 */
	p = &object_images;
	do{
	    for(i = 0; i < p->nimages; i++){
		/* If this object file image is currently unused skip it */
		link_state = GET_LINK_STATE(p->images[i].module);
		if(link_state == UNUSED)
		    continue;
		/* If this is a two-level image skip it */
		if(force_flat_namespace == FALSE &&
		   (p->images[i].image.mh->flags & MH_TWOLEVEL) == MH_TWOLEVEL)
		    continue;
		/* TODO: skip modules that that are replaced */
		linkedit_segment = p->images[i].image.linkedit_segment;
		st = p->images[i].image.st;
		dyst = p->images[i].image.dyst;
		/*
		 * Object images could be loaded that do not have the proper
		 * link edit information.
		 */
		if(linkedit_segment == NULL || st == NULL || dyst == NULL)
		    continue;
		symbols = (struct nlist *)
		    (p->images[i].image.vmaddr_slide +
		     linkedit_segment->vmaddr +
		     st->symoff -
		     linkedit_segment->fileoff);
		nlist_bsearch_strings = (char *)
		    (p->images[i].image.vmaddr_slide +
		     linkedit_segment->vmaddr +
		     st->stroff -
		     linkedit_segment->fileoff);
		if(p->images[i].image.mh->flags & MH_BINDATLOAD)
		    symbol = nlist_linear_search(symbol_name,
			symbols + dyst->iundefsym, dyst->nundefsym,
			nlist_bsearch_strings);
		else
		    symbol = inline_bsearch_nlist(symbol_name,
						  symbols + dyst->iundefsym,
						  dyst->nundefsym);
		if(symbol != NULL){
		    if(ignore_lazy_references == TRUE &&
		       (symbol->n_desc & REFERENCE_TYPE) ==
			REFERENCE_FLAG_UNDEFINED_LAZY)
			continue;
		    *referenced_symbol = symbol;
		    *referenced_module = &(p->images[i].module);
		    *referenced_image = &(p->images[i].image);
		    *referenced_library_image = NULL;
		    reference_name = nlist_bsearch_strings +
				     symbol->n_un.n_strx;
		    return(reference_name);
		}
	    }
	    p = p->next_images;
	}while(p != NULL);

	/*
	 * Next look in the library_images for a reference of symbol_name.
	 */
	q = &library_images;
	do{
	    for(i = 0; i < q->nimages; i++){
		/* If this is a two-level image skip it */
		if(force_flat_namespace == FALSE &&
		   (q->images[i].image.mh->flags & MH_TWOLEVEL) == MH_TWOLEVEL)
		    continue;
		linkedit_segment = q->images[i].image.linkedit_segment;
		st = q->images[i].image.st;
		dyst = q->images[i].image.dyst;

		/*
		 * First find the symbol either in the table of contents
		 * (for defined symbols) or in the undefined symbols.
		 */
		tocs = (struct dylib_table_of_contents *)
		    (q->images[i].image.vmaddr_slide +
		     linkedit_segment->vmaddr +
		     dyst->tocoff -
		     linkedit_segment->fileoff);
		toc_bsearch_symbols = (struct nlist *)
		    (q->images[i].image.vmaddr_slide +
		     linkedit_segment->vmaddr +
		     st->symoff -
		     linkedit_segment->fileoff);
		toc_bsearch_strings = (char *)
		    (q->images[i].image.vmaddr_slide +
		     linkedit_segment->vmaddr +
		     st->stroff -
		     linkedit_segment->fileoff);
		toc = inline_bsearch_toc(symbol_name, tocs, dyst->ntoc);
		if(toc != NULL){
		    /* TODO: could have N_INDR in the defined symbols */
		    symbol = toc_bsearch_symbols + toc->symbol_index;
		    isym = toc->symbol_index;
		}
		else{
		    nlist_bsearch_strings = toc_bsearch_strings;
		    symbol = inline_bsearch_nlist(symbol_name,
				     toc_bsearch_symbols + dyst->iundefsym,
				     dyst->nundefsym);
		    if(symbol == NULL)
			continue;
		    isym = symbol - toc_bsearch_symbols;
		}
		reference_name = nlist_bsearch_strings + symbol->n_un.n_strx;
		dylib_modules = (struct dylib_module *)
		    (q->images[i].image.vmaddr_slide +
		     linkedit_segment->vmaddr +
		     dyst->modtaboff -
		     linkedit_segment->fileoff);
		dylib_references = (struct dylib_reference *)
		    (q->images[i].image.vmaddr_slide +
		     linkedit_segment->vmaddr +
		     dyst->extrefsymoff -
		     linkedit_segment->fileoff);
		/*
		 * Walk the module table and look at modules that are linked in.
		 */
		for(j = 0; j < dyst->nmodtab; j++){
		    link_state = GET_LINK_STATE(q->images[i].modules[j]);
		    if(link_state == BEING_LINKED ||
		       link_state == RELOCATED ||
		       link_state == REGISTERING ||
		       link_state == INITIALIZING ||
		       link_state == LINKED  ||
		       link_state == FULLY_LINKED){
			/*
			 * The module is linked in look at the reference table
			 * for this module to see if we are making a reference
			 * from the module to this symbol.
			 */
			for(k = dylib_modules[j].irefsym;
			    k < dylib_modules[j].irefsym +
				dylib_modules[j].nrefsym;
			    k++){
			    if(dylib_references[k].isym == isym){
				if(dylib_references[k].flags ==
				   REFERENCE_FLAG_UNDEFINED_NON_LAZY ||
				   (dylib_references[k].flags ==
				    REFERENCE_FLAG_UNDEFINED_LAZY &&
				    ignore_lazy_references == FALSE)){
					/* a reference is found return it */
					*referenced_symbol = symbol;
					*referenced_module =
					    q->images[i].modules + j;
					*referenced_image =
					    &(q->images[i].image);
					*referenced_library_image =
					    &(q->images[i]);
					return(reference_name);
				}
			    }
			}
		    }
		}
	    }
	    q = q->next_images;
	}while(q != NULL);

	*referenced_symbol = NULL;
	*referenced_module = NULL;
	*referenced_image = NULL;
	*referenced_library_image = NULL;
	return(NULL);
}

/*
 * nlist_linear_search() is used to search the symbol table of an object image
 * that is marked with MH_BINDATLOAD whos undefined symbols are NOT sorted by
 * symbol name.  If it finds the symbol with the specified symbol_name it
 * returns a pointer to the symbol else it returns NULL.
 */
static
struct nlist *
nlist_linear_search(
char *symbol_name,
struct nlist *symbols,
unsigned long nsymbols,
char *strings)
{
    unsigned long i;

	for(i = 0; i < nsymbols; i++){
	    if(strcmp(symbol_name, strings + symbols[i].n_un.n_strx) == 0)
		return(symbols + i);
	}
	return(NULL);
}

/*
 * validate_NSSymbol() is used by the functions that implement dyld library
 * functions that take NSSymbols as arguments.  It returns true if the symbol
 * is valid and fills in the other parameters for that symbol.  If not it
 * returns FALSE and sets the other parameters to NULL.
 */
enum bool
validate_NSSymbol(
struct nlist *symbol,		/* the symbol to validate */
module_state **defined_module,	/* the module the symbol is in */
struct image **defined_image,	/* the image the module is in */
struct library_image **defined_library_image)
{
    unsigned long i, j, isym;
    struct object_images *p;
    struct library_images *q;
    struct segment_command *linkedit_segment;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    struct nlist *symbols;
    struct dylib_module *dylib_modules;
    enum link_state link_state;

	*defined_module = NULL;
	*defined_image = NULL;
	*defined_library_image = NULL;

	/*
	 * First look in object_images for the symbol.
	 */
	p = &object_images;
	do{
	    for(i = 0; i < p->nimages; i++){
		/* If this object file image is currently unused skip it */
		link_state = GET_LINK_STATE(p->images[i].module);
		if(link_state == UNUSED)
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
		symbols = (struct nlist *)
		    (p->images[i].image.vmaddr_slide +
		     linkedit_segment->vmaddr +
		     st->symoff -
		     linkedit_segment->fileoff);
		if(symbol >= symbols + dyst->iextdefsym &&
		   symbol < symbols + (dyst->iextdefsym + dyst->nextdefsym)){
		    link_state = GET_LINK_STATE(p->images[i].module);
		    if(link_state != INITIALIZING &&
		       link_state != LINKED &&
		       link_state != FULLY_LINKED)
			return(FALSE);
		    if((symbol->n_desc & N_DESC_DISCARDED) == N_DESC_DISCARDED)
			return(FALSE);

		    *defined_module = &(p->images[i].module);
		    *defined_image = &(p->images[i].image);
		    *defined_library_image = NULL;
		    return(TRUE);
		}
	    }
	    p = p->next_images;
	}while(p != NULL);

	/*
	 * Next look in the library_images for a definition of symbol_name.
	 */
	q = &library_images;
	do{
	    for(i = 0; i < q->nimages; i++){
		linkedit_segment = q->images[i].image.linkedit_segment;
		st = q->images[i].image.st;
		dyst = q->images[i].image.dyst;
		symbols = (struct nlist *)
		    (q->images[i].image.vmaddr_slide +
		     linkedit_segment->vmaddr +
		     st->symoff -
		     linkedit_segment->fileoff);
		if(symbol >= symbols + dyst->iextdefsym &&
		   symbol < symbols + (dyst->iextdefsym + dyst->nextdefsym)){
		    if((symbol->n_desc & N_DESC_DISCARDED) == N_DESC_DISCARDED)
			return(FALSE);
		    /*
		     * We found this symbol in this library image.  So now
		     * determine the module the symbol is in and if it is
		     * linked.
		     */
		    isym = symbol - symbols;
		    dylib_modules = (struct dylib_module *)
			(q->images[i].image.vmaddr_slide +
			 linkedit_segment->vmaddr +
			 dyst->modtaboff -
			 linkedit_segment->fileoff);
		    for(j = 0; j < dyst->nmodtab; j++){
			if(isym >= dylib_modules[j].iextdefsym &&
			   isym < dylib_modules[j].iextdefsym +
				  dylib_modules[j].nextdefsym)
			    break;
		    }
		    if(j >= dyst->nmodtab)
			return(FALSE);
		    link_state = GET_LINK_STATE(q->images[i].modules[j]);
		    if(link_state != INITIALIZING &&
		       link_state != LINKED &&
		       link_state != FULLY_LINKED)
			return(FALSE);

		    *defined_module = q->images[i].modules + j;
		    *defined_image = &(q->images[i].image);
		    *defined_library_image = q->images + i;
		    return(TRUE);
		}
	    }
	    q = q->next_images;
	}while(q != NULL);

	return(FALSE);
}

/*
 * validate_NSModule() is used by the functions that implement dyld library
 * functions that take NSmodules as arguments.  It returns true if the module
 * is valid and fills in the other parameters for that module.  If not it
 * returns FALSE and sets the other parameters to NULL.
 */
enum bool
validate_NSModule(
module_state *module,		/* the module to validate */
struct image **defined_image,	/* the image the module is in */
struct library_image **defined_library_image)
{
    unsigned long i;
    struct object_images *p;
    struct library_images *q;
    enum link_state link_state;

	*defined_image = NULL;
	*defined_library_image = NULL;

	/*
	 * First look in object_images for the module.
	 */
	p = &object_images;
	do{
	    for(i = 0; i < p->nimages; i++){
		/* If this object file image is currently unused skip it */
		link_state = GET_LINK_STATE(p->images[i].module);
		if(link_state == UNUSED)
		    continue;
		if(&(p->images[i].module) == module){
		    *defined_image = &(p->images[i].image);
		    *defined_library_image = NULL;
		    return(TRUE);
		}
	    }
	    p = p->next_images;
	}while(p != NULL);

	/*
	 * Next look in the library_images for the module.
	 */
	q = &library_images;
	do{
	    for(i = 0; i < q->nimages; i++){
		if(module >= q->images[i].modules &&
		   module < q->images[i].modules + q->images[i].nmodules){
		    *defined_image = &(q->images[i].image);
		    *defined_library_image = q->images + i;
		    return(TRUE);
		}
	    }
	    q = q->next_images;
	}while(q != NULL);

	return(FALSE);
}

/*
 * relocate_symbol_pointers_in_object_image() looks up each symbol that is on
 * the being_linked list and if the object references the symbol cause any
 * symbol pointers in the image to be set to the value of the defined symbol.
 */
void
relocate_symbol_pointers_in_object_image(
struct image *image)
{
    struct segment_command *linkedit_segment;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    struct nlist *symbols, *symbol;
    unsigned long *indirect_symtab;

    struct symbol_list *symbol_list;
    char *symbol_name, *strings;
    unsigned long value, symbol_index;

    struct nlist *defined_symbol;
    module_state *defined_module;
    struct image *defined_image;
    struct library_image *defined_library_image;

    struct image *primary_image;
    unsigned long i;
    enum bool match;

	linkedit_segment = image->linkedit_segment;
	st = image->st;
	dyst = image->dyst;
	/*
	 * Object images could be loaded that do not have the proper
	 * link edit information.
	 */
	if(linkedit_segment == NULL || st == NULL || dyst == NULL)
	    return;

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
	indirect_symtab = (unsigned long *)
	    (image->vmaddr_slide +
	     linkedit_segment->vmaddr +
	     dyst->indirectsymoff -
	     linkedit_segment->fileoff);
	nlist_bsearch_strings = strings;
	/*
	 * For each symbol that is in the being linked list see if it is in the
	 * symbol table of the object (defined or undefined).
	 */
	for(symbol_list = being_linked_list.next;
	    symbol_list != &being_linked_list;
	    symbol_list = symbol_list->next){

	    symbol_name = symbol_list->name;
	    symbol = inline_bsearch_nlist(symbol_name,
			symbols + dyst->iextdefsym, dyst->nextdefsym);
	    if(symbol == NULL){
		if(image->mh->flags & MH_BINDATLOAD)
		    symbol = nlist_linear_search(symbol_name,
			symbols + dyst->iundefsym, dyst->nundefsym,
			nlist_bsearch_strings);
		else
		    symbol = inline_bsearch_nlist(symbol_name,
				    symbols + dyst->iundefsym, dyst->nundefsym);
	    }
	    if(symbol == NULL)
		continue;
	    symbol_index = symbol - symbols;

	    /*
	     * The symbol on the being linked list has been found in this image.
	     * If we are doing two-level namespace linking and this is a
	     * two-level namespace image then we only relocate the symbol
	     * pointers in this image to this being linked symbol if the
	     * primary image of the being linked matches that of the symbol in
	     * this image.
	     */
	    if(force_flat_namespace == FALSE &&
	       (image->mh->flags & MH_TWOLEVEL) == MH_TWOLEVEL){
		if(GET_LIBRARY_ORDINAL(symbol->n_desc) == EXECUTABLE_ORDINAL)
		    primary_image = &object_images.images[0].image;
		else if(GET_LIBRARY_ORDINAL(symbol->n_desc) ==
			DYNAMIC_LOOKUP_ORDINAL &&
			image->ndependent_images != DYNAMIC_LOOKUP_ORDINAL)
		    primary_image = symbol_list->image;
		else if(GET_LIBRARY_ORDINAL(symbol->n_desc) ==
			SELF_LIBRARY_ORDINAL)
		    primary_image = image;
		else{
		    /* could make this check
		    if(GET_LIBRARY_ORDINAL(symbol->n_desc) - 1 >
		       image->ndependent_images)
			<<< bad LIBRARY_ORDINAL in object file >>>
		    */
		    primary_image = image->dependent_images[
			    GET_LIBRARY_ORDINAL(symbol->n_desc) - 1];
		}
		/* match this primary_image with symbol_list->image */
		if(primary_image != symbol_list->image){
		    match = FALSE;
		    for(i = 0;
			i < symbol_list->image->numbrella_images;
			i++){
			if(primary_image ==
			   symbol_list->image->umbrella_images[i]){
			    match = TRUE;
			    break;
			}
		    }
		    if(match != TRUE)
			continue; /* do not relocate symbol pointers in this
				     image to this being linked symbol */
		}
	    }
	    else{
		if(symbol_list->flat_reference == FALSE)
		    continue; /* go to next being linked symbol */
	    }

	    if(symbol_list->symbol != NULL &&
	       (symbol_list->symbol->n_type & N_TYPE) == N_INDR){
		/* TODO: could this lookup fail? */
		lookup_symbol(symbol_name,
			      get_primary_image(symbol_list->image,
						symbol_list->symbol),
			      get_hint(symbol_list->image, symbol_list->symbol),
			      get_weak(symbol_list->symbol),
			      &defined_symbol, &defined_module, &defined_image,
			      &defined_library_image, NULL);
		nlist_bsearch_strings = strings;

		value = defined_symbol->n_value;
		if((defined_symbol->n_type & N_TYPE) != N_ABS)
		    value += defined_image->vmaddr_slide;
#ifdef DYLD_PROFILING
		if(strcmp(symbol_name, "__exit") == 0)
		    value = (unsigned long)profiling_exit +
			    dyld_image_vmaddr_slide;
#endif
		relocate_symbol_pointers(symbol_index, indirect_symtab,
					 image, value, FALSE);
	    }
	    else{
		value = symbol_list->symbol->n_value;
		if((symbol_list->symbol->n_type & N_TYPE) != N_ABS)
		    value += symbol_list->image->vmaddr_slide;
#ifdef DYLD_PROFILING
		if(strcmp(symbol_name, "__exit") == 0)
		    value = (unsigned long)profiling_exit +
			    dyld_image_vmaddr_slide;
#endif
		relocate_symbol_pointers(symbol_index, indirect_symtab,
					 image, value, FALSE);
	    }
	}
}

/*
 * relocate_symbol_pointers_in_library_image() looks up each symbol that is on
 * the being_linked list and if the library references the symbol cause any
 * symbol pointers in the image to be set to the value of the defined symbol.
 */
void
relocate_symbol_pointers_in_library_image(
struct image *image)
{
    struct segment_command *linkedit_segment;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    struct dylib_table_of_contents *tocs, *toc;
    unsigned long *indirect_symtab;
    struct nlist *symbols, *symbol;
    char *strings;

    struct symbol_list *symbol_list;
    char *symbol_name;
    unsigned long value, symbol_index;

    struct nlist *defined_symbol;
    module_state *defined_module;
    struct image *defined_image;
    struct library_image *defined_library_image;

    struct image *primary_image;
    unsigned long i;
    enum bool match;

	linkedit_segment = image->linkedit_segment;
	st = image->st;
	dyst = image->dyst;

	tocs = (struct dylib_table_of_contents *)
	    (image->vmaddr_slide +
	     linkedit_segment->vmaddr +
	     dyst->tocoff -
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
	indirect_symtab = (unsigned long *)
	    (image->vmaddr_slide +
	     linkedit_segment->vmaddr +
	     dyst->indirectsymoff -
	     linkedit_segment->fileoff);
	toc_bsearch_symbols = symbols;
	toc_bsearch_strings = strings;
	nlist_bsearch_strings = strings;
	/*
	 * For each symbol that is in the being linked list see if it is in the
	 * symbol table of the library (defined or undefined).
	 */
	for(symbol_list = being_linked_list.next;
	    symbol_list != &being_linked_list;
	    symbol_list = symbol_list->next){

	    symbol_name = symbol_list->name;
	    toc = inline_bsearch_toc(symbol_name, tocs, dyst->ntoc);
	    if(toc != NULL){
		symbol_index = toc->symbol_index;
		symbol = symbols + symbol_index;
	    }
	    else{
		symbol = inline_bsearch_nlist(symbol_name,
			       symbols + dyst->iundefsym, dyst->nundefsym);
		if(symbol == NULL)
		    continue;
		symbol_index = symbol - symbols;
	    }

	    /*
	     * The symbol on the being linked list has been found in this image.
	     * If we are doing two-level namespace linking and this is a
	     * two-level namespace image then we only relocate the symbol
	     * pointers in this image to this being linked symbol if the
	     * primary image of the being linked matches that of the symbol in
	     * this image.
	     */
	    if(force_flat_namespace == FALSE &&
	       (image->mh->flags & MH_TWOLEVEL) == MH_TWOLEVEL){
		if(GET_LIBRARY_ORDINAL(symbol->n_desc) == EXECUTABLE_ORDINAL)
		    primary_image = &object_images.images[0].image;
		else if(GET_LIBRARY_ORDINAL(symbol->n_desc) ==
			DYNAMIC_LOOKUP_ORDINAL &&
			image->ndependent_images != DYNAMIC_LOOKUP_ORDINAL)
		    primary_image = symbol_list->image;
		else if(GET_LIBRARY_ORDINAL(symbol->n_desc) ==
			SELF_LIBRARY_ORDINAL)
		    primary_image = image;
		else{
		    /* could make this check
		    if(GET_LIBRARY_ORDINAL(symbol->n_desc) - 1 >
		       image->ndependent_images)
			<<< bad LIBRARY_ORDINAL in object file >>>
		    */
		    primary_image = image->dependent_images[
			    GET_LIBRARY_ORDINAL(symbol->n_desc) - 1];
		}
		/* match this primary_image with symbol_list->image */
		if(primary_image != symbol_list->image){
		    match = FALSE;
		    for(i = 0;
			i < symbol_list->image->numbrella_images;
			i++){
			if(primary_image ==
			   symbol_list->image->umbrella_images[i]){
			    match = TRUE;
			    break;
			}
		    }
		    if(match != TRUE)
			continue; /* do not relocate symbol pointers in this
				     image to this being linked symbol */
		}
	    }
	    else{
		if(symbol_list->flat_reference == FALSE)
		    continue; /* go to next being linked symbol */
	    }

	    if(symbol_list->symbol != NULL &&
	       (symbol_list->symbol->n_type & N_TYPE) == N_INDR){
		/* TODO: could this lookup fail? */
		lookup_symbol(symbol_name,
			      get_primary_image(symbol_list->image,
						symbol_list->symbol),
			      get_hint(symbol_list->image, symbol_list->symbol),
			      get_weak(symbol_list->symbol),
			      &defined_symbol, &defined_module, &defined_image,
			      &defined_library_image, NULL);
		toc_bsearch_symbols = symbols;
		toc_bsearch_strings = strings;
		nlist_bsearch_strings = strings;

		value = defined_symbol->n_value;
		if((defined_symbol->n_type & N_TYPE) != N_ABS)
		    value += defined_image->vmaddr_slide;
#ifdef DYLD_PROFILING
		if(strcmp(symbol_name, "__exit") == 0)
		    value = (unsigned long)profiling_exit +
			    dyld_image_vmaddr_slide;
#endif
		relocate_symbol_pointers(symbol_index, indirect_symtab,
					 image, value, FALSE);
	    }
	    else{
		value = symbol_list->symbol->n_value;
		if((symbol_list->symbol->n_type & N_TYPE) != N_ABS)
		    value += symbol_list->image->vmaddr_slide;
#ifdef DYLD_PROFILING
		if(strcmp(symbol_name, "__exit") == 0)
		    value = (unsigned long)profiling_exit +
			    dyld_image_vmaddr_slide;
#endif
		relocate_symbol_pointers(symbol_index, indirect_symtab,
					 image, value, FALSE);
	    }
	}
}

/*
 * change_symbol_pointers_in_flat_images() changes the symbol pointers in all
 * the flat images to the symbol_name to the new specified value.  This not
 * used in normal dynamic linking but used in response to things like multiply
 * defined symbol error handling and replacing of modules.  If
 * only_lazy_pointers is TRUE then only lazy symbol pointers are changed.
 */
void
change_symbol_pointers_in_flat_images(
char *symbol_name,
unsigned long value,
enum bool only_lazy_pointers)
{
    unsigned long i;
    struct object_images *p;
    struct library_images *q;

    struct segment_command *linkedit_segment;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    struct nlist *symbols, *symbol;
    unsigned long *indirect_symtab;
    unsigned long symbol_index;
    struct dylib_table_of_contents *tocs, *toc;
    char *strings;
    enum link_state link_state;

	/*
	 * First change the object_images that have been linked.
	 */
	p = &object_images;
	do{
	    for(i = 0; i < p->nimages; i++){
		/* If this object file image is currently unused skip it */
		link_state = GET_LINK_STATE(p->images[i].module);
		if(link_state == UNUSED)
		    continue;
		/* If this is a two-level image skip it */
		if(force_flat_namespace == FALSE &&
		   (p->images[i].image.mh->flags & MH_TWOLEVEL) == MH_TWOLEVEL)
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
		symbols = (struct nlist *)
		    (p->images[i].image.vmaddr_slide +
		     linkedit_segment->vmaddr +
		     st->symoff -
		     linkedit_segment->fileoff);
		nlist_bsearch_strings = (char *)
		    (p->images[i].image.vmaddr_slide +
		     linkedit_segment->vmaddr +
		     st->stroff -
		     linkedit_segment->fileoff);
		indirect_symtab = (unsigned long *)
		    (p->images[i].image.vmaddr_slide +
		     linkedit_segment->vmaddr +
		     dyst->indirectsymoff -
		     linkedit_segment->fileoff);

		symbol = inline_bsearch_nlist(symbol_name,
			   symbols + dyst->iextdefsym, dyst->nextdefsym);
		if(symbol == NULL){
		    if(p->images[i].image.mh->flags & MH_BINDATLOAD)
			symbol = nlist_linear_search(symbol_name,
			    symbols + dyst->iundefsym, dyst->nundefsym,
			    nlist_bsearch_strings);
		    else
			symbol = inline_bsearch_nlist(symbol_name,
				symbols + dyst->iundefsym, dyst->nundefsym);
		}
		if(symbol == NULL)
		    continue;
		symbol_index = symbol - symbols;
#ifdef DYLD_PROFILING
		if(strcmp(symbol_name, "__exit") == 0)
		    value = (unsigned long)profiling_exit +
			    dyld_image_vmaddr_slide;
#endif
		relocate_symbol_pointers(symbol_index, indirect_symtab,
					 &(p->images[i].image), value,
					 only_lazy_pointers);
	    }
	    p = p->next_images;
	}while(p != NULL);

	/*
	 * Next change the library_images.
	 */
	q = &library_images;
	do{
	    for(i = 0; i < q->nimages; i++){
		/* If this is a two-level image skip it */
		if(force_flat_namespace == FALSE &&
		   (q->images[i].image.mh->flags & MH_TWOLEVEL) == MH_TWOLEVEL)
		    continue;
		linkedit_segment = q->images[i].image.linkedit_segment;
		st = q->images[i].image.st;
		dyst = q->images[i].image.dyst;

		tocs = (struct dylib_table_of_contents *)
		    (q->images[i].image.vmaddr_slide +
		     linkedit_segment->vmaddr +
		     dyst->tocoff -
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
		indirect_symtab = (unsigned long *)
		    (q->images[i].image.vmaddr_slide +
		     linkedit_segment->vmaddr +
		     dyst->indirectsymoff -
		     linkedit_segment->fileoff);
		toc_bsearch_symbols = symbols;
		toc_bsearch_strings = strings;
		nlist_bsearch_strings = strings;

		toc = inline_bsearch_toc(symbol_name, tocs, dyst->ntoc);
		if(toc != NULL){
		    symbol_index = toc->symbol_index;
		}
		else{
		    symbol = inline_bsearch_nlist(symbol_name,
				symbols + dyst->iundefsym, dyst->nundefsym);
		    if(symbol == NULL)
			continue;
		    symbol_index = symbol - symbols;
		}
#ifdef DYLD_PROFILING
		if(strcmp(symbol_name, "__exit") == 0)
		    value = (unsigned long)profiling_exit +
			    dyld_image_vmaddr_slide;
#endif
		relocate_symbol_pointers(symbol_index, indirect_symtab,
					 &(q->images[i].image), value,
					 only_lazy_pointers);
	    }
	    q = q->next_images;
	}while(q != NULL);
}

/*
 * relocate_symbol_pointers() sets the value of the defined symbol for
 * symbol_name into the symbol pointers of the image which have the specified
 * symbol_index.
 */
static
void
relocate_symbol_pointers(
unsigned long symbol_index,
unsigned long *indirect_symtab,
struct image *image,
unsigned long value,
enum bool only_lazy_pointers)
{
    unsigned long i, j, k, section_type, size, vmaddr_slide, reserved1, addr;
    struct load_command *lc;
    struct segment_command *sg;
    struct section *s;

#ifdef __ppc__
	if(only_lazy_pointers == TRUE)
	    value = image->dyld_stub_binding_helper;
#endif
	/*
	 * A symbol being linked is in this image at the symbol_index. Walk
	 * the headers looking for indirect sections.  Then for each indirect
	 * section scan the indirect table entries for this symbol_index and
	 * if found set the value of the defined symbol into the pointer.
	 */
	lc = (struct load_command *)((char *)image->mh +
				     sizeof(struct mach_header));
        vmaddr_slide = image->vmaddr_slide;
	for(i = 0; i < image->mh->ncmds; i++){
	    switch(lc->cmd){
	    case LC_SEGMENT:
		sg = (struct segment_command *)lc;
		s = (struct section *)
		    ((char *)sg + sizeof(struct segment_command));
		for(j = 0 ; j < sg->nsects ; j++){
		    size = s->size;
		    reserved1 = s->reserved1;
		    addr = s->addr;
		    section_type = s->flags & SECTION_TYPE;
		    if((section_type == S_NON_LAZY_SYMBOL_POINTERS &&
			only_lazy_pointers == FALSE) ||
		       section_type == S_LAZY_SYMBOL_POINTERS){
			for(k = 0; k < size / sizeof(unsigned long); k++){
			    if(indirect_symtab[reserved1 + k] == symbol_index){
				*((long *)(vmaddr_slide + addr +
					   (k * sizeof(long)))) = value;
			    }
			}
		    }
		    s++;
		}
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
}

/*
 * relocate_symbol_pointers_for_defined_externs() adjust the value of the
 * the non-lazy symbol pointers of the image which are for symbols that are
 * defined externals.
 */
void
relocate_symbol_pointers_for_defined_externs(
struct image *image)
{
    struct segment_command *linkedit_segment;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    unsigned long *indirect_symtab;
    struct nlist *symbols;

    unsigned long index;
    unsigned long i, j, k, section_type;
    struct load_command *lc;
    struct segment_command *sg;
    struct section *s;

	linkedit_segment = image->linkedit_segment;
	st = image->st;
	dyst = image->dyst;

	symbols = (struct nlist *)
	    (image->vmaddr_slide +
	     linkedit_segment->vmaddr +
	     st->symoff -
	     linkedit_segment->fileoff);
	indirect_symtab = (unsigned long *)
	    (image->vmaddr_slide +
	     linkedit_segment->vmaddr +
	     dyst->indirectsymoff -
	     linkedit_segment->fileoff);

	/*
	 * Loop through the S_NON_LAZY_SYMBOL_POINTERS sections and adjust the
	 * value of all pointers that are for defined externals by the
	 * vmaddr_slide.
	 */
	lc = (struct load_command *)((char *)image->mh +
				     sizeof(struct mach_header));
	for(i = 0; i < image->mh->ncmds; i++){
	    switch(lc->cmd){
	    case LC_SEGMENT:
		sg = (struct segment_command *)lc;
		s = (struct section *)
		    ((char *)sg + sizeof(struct segment_command));
		for(j = 0 ; j < sg->nsects ; j++){
		    section_type = s->flags & SECTION_TYPE;
		    if(section_type == S_NON_LAZY_SYMBOL_POINTERS){
			for(k = 0; k < s->size / sizeof(unsigned long); k++){
			    index = indirect_symtab[s->reserved1 + k];
			    if(index == INDIRECT_SYMBOL_ABS)
				continue;
			    if(index == INDIRECT_SYMBOL_LOCAL ||
			       ((symbols[index].n_type & N_TYPE) != N_UNDF &&
			        (symbols[index].n_type & N_TYPE) != N_ABS)){
				*((long *)(image->vmaddr_slide + s->addr +
				    (k * sizeof(long)))) += image->vmaddr_slide;
			    }
			}
		    }
		    s++;
		}
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
}

/*
 * bind_lazy_symbol_reference() is transfered to to bind a lazy reference the
 * first time it is called.  The symbol being referenced is the indirect symbol
 * for the lazy_symbol_pointer_address specified in the image with the specified
 * mach header.
 */
unsigned long
bind_lazy_symbol_reference(
struct mach_header *mh,
unsigned long lazy_symbol_pointer_address)
{
    struct object_images *p;
    struct library_images *q;
    struct image *image;
    enum bool flat_reference;

    unsigned long i, j, section_type, isym;
    struct load_command *lc;
    struct segment_command *sg;
    struct section *s;
    struct nlist *symbols, *symbol;
    unsigned long *indirect_symtab;

    char *symbol_name;
    struct nlist *defined_symbol;
    module_state *defined_module;
    struct image *defined_image;
    struct library_image *defined_library_image;
    enum link_state link_state;
    unsigned long value;

    struct library_image *library_image;
    struct dylib_module *dylib_modules;

	/* set lock for dyld data structures */
	set_lock();

	DYLD_TRACE_SYMBOLS_ADDRESSED_START(
	    DYLD_TRACE_bind_lazy_symbol_reference, lazy_symbol_pointer_address);
        
	/*
	 * Figure out which image this mach header is for.
	 */
	image = NULL;
	/*
	 * First look in object_images for this mach_header.
	 */
	p = &object_images;
	do{
	    for(i = 0; image == NULL && i < p->nimages; i++){
		if(p->images[i].image.mh == mh)
		    image = &(p->images[i].image);
	    }
	    p = p->next_images;
	}while(image == NULL && p != NULL);
	library_image = NULL;
	if(image == NULL){
	    /*
	     * Next look in the library_images for this mach_header.
	     */
	    q = &library_images;
	    do{
		for(i = 0; image == NULL && i < q->nimages; i++){
		    if(q->images[i].image.mh == mh){
			image = &(q->images[i].image);
			library_image = q->images + i;
		    }
		}
		q = q->next_images;
	    }while(image == NULL && q != NULL);
	}
	if(image == NULL){
	    error("bad mach header passed to stub_binding_helper");
	    link_edit_error(DYLD_OTHER_ERROR, DYLD_LAZY_BIND, NULL);
            DYLD_TRACE_SYMBOLS_END(DYLD_TRACE_bind_lazy_symbol_reference);
	    halt();
	    exit(DYLD_EXIT_FAILURE_BASE + DYLD_OTHER_ERROR);
	}

	/*
	 * Now determine which symbol is being refered to.
	 */
	symbols = (struct nlist *)
	    (image->vmaddr_slide +
	     image->linkedit_segment->vmaddr +
	     image->st->symoff -
	     image->linkedit_segment->fileoff);
	indirect_symtab = (unsigned long *)
	    (image->vmaddr_slide +
	     image->linkedit_segment->vmaddr +
	     image->dyst->indirectsymoff -
	     image->linkedit_segment->fileoff);
	symbol = NULL;
	isym = 0;
	lc = (struct load_command *)((char *)image->mh +
				     sizeof(struct mach_header));
	for(i = 0; symbol == NULL && i < image->mh->ncmds; i++){
	    switch(lc->cmd){
	    case LC_SEGMENT:
		sg = (struct segment_command *)lc;
		s = (struct section *)
		    ((char *)sg + sizeof(struct segment_command));
		for(j = 0; symbol == NULL && j < sg->nsects; j++){
		    section_type = s->flags & SECTION_TYPE;
		    if(section_type == S_LAZY_SYMBOL_POINTERS){
			if(lazy_symbol_pointer_address >=
			       s->addr + image->vmaddr_slide &&
			   lazy_symbol_pointer_address <
			       s->addr + image->vmaddr_slide + s->size){
			    isym = indirect_symtab[s->reserved1 +
				    (lazy_symbol_pointer_address - (s->addr +
				     image->vmaddr_slide)) /
				    sizeof(unsigned long)];
			    symbol = symbols + isym;
			}
		    }
		    s++;
		}
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
	if(symbol == NULL){
	    error("bad address of lazy symbol pointer passed to "
		  "stub_binding_helper");
	    link_edit_error(DYLD_OTHER_ERROR, DYLD_LAZY_BIND, NULL);
            DYLD_TRACE_SYMBOLS_END(DYLD_TRACE_bind_lazy_symbol_reference);
	    halt();
	    exit(DYLD_EXIT_FAILURE_BASE + DYLD_OTHER_ERROR);
	}

	/*
	 * If this symbol pointer is for a private extern symbol that is no
	 * longer external then use it's value.
	 */
	if((symbol->n_type & N_PEXT) == N_PEXT &&
	   (symbol->n_type & N_EXT) != N_EXT){
	    /*
	     * If this is from a library make sure the module that defines this
	     * symbol is linked in.
	     */
	    if(library_image != NULL){
		dylib_modules = (struct dylib_module *)
		    (image->vmaddr_slide +
		     image->linkedit_segment->vmaddr +
		     image->dyst->modtaboff -
		     image->linkedit_segment->fileoff);
		for(j = 0; j < image->dyst->nmodtab; j++){
		    if(isym >= dylib_modules[j].ilocalsym &&
		       isym < dylib_modules[j].ilocalsym +
			      dylib_modules[j].nlocalsym)
			break;
		}
		if(j < image->dyst->nmodtab){
		    link_state = GET_LINK_STATE(library_image->modules[j]);
		    /*
		     * If this module is unlinked cause it to be linked in.
		     * As of the Mac OS X Public Beta release we now allow
		     * modules in a library that defineds private externs to
		     * also have external definitions so we must call
		     * link_library_module() and can no longer short cut the
		     * linking process by just adding the external references
		     * and setting the link state directly.
		     */
		    if(link_state == PREBOUND_UNLINKED){
			undo_prebinding_for_library_module(
			    library_image->modules + j,
			    image,
			    library_image);
			link_state = UNLINKED;
			SET_LINK_STATE(library_image->modules[j], link_state);
		    }
		    if(link_state == UNLINKED){
			link_library_module(library_image, image,
				library_image->modules + j,
				executable_bind_at_load, /* bind_now */
				FALSE, /* bind_fully */
				FALSE /* launching_with_prebound_libraries */);
			link_in_need_modules(FALSE, FALSE);
		    }
		    value = symbol->n_value;
		    if((symbol->n_type & N_TYPE) != N_ABS)
			value += image->vmaddr_slide;
		    *((long *)lazy_symbol_pointer_address) = value;
                    
                    DYLD_TRACE_SYMBOLS_END(
			DYLD_TRACE_bind_lazy_symbol_reference);
                    
		    /* release lock for dyld data structures */
		    release_lock();
		    return(value);
		}
	    }
	    else {
		value = symbol->n_value;
		if((symbol->n_type & N_TYPE) != N_ABS)
		    value += image->vmaddr_slide;
		*((long *)lazy_symbol_pointer_address) = value;
                
                DYLD_TRACE_SYMBOLS_END(DYLD_TRACE_bind_lazy_symbol_reference);
                                
		/* release lock for dyld data structures */
		release_lock();
		return(value);
	    }
	}

	/*
	 * Now that we have the image and the symbol lookup this symbol and see
	 * if it is in a linked module.  If so just set the value of the symbol
	 * into the lazy symbol pointer and return that value.
	 */
	symbol_name = (char *)
	    (image->vmaddr_slide +
	     image->linkedit_segment->vmaddr +
	     image->st->stroff -
	     image->linkedit_segment->fileoff) +
	    symbol->n_un.n_strx;
	lookup_symbol(symbol_name, get_primary_image(image, symbol),
		      get_hint(image, symbol), get_weak(symbol),
		      &defined_symbol, &defined_module, &defined_image,
		      &defined_library_image, NULL);
	if(defined_symbol != NULL){
	    link_state = GET_LINK_STATE(*defined_module);
	    if(link_state == LINKED || link_state == FULLY_LINKED){
		value = defined_symbol->n_value;
		if((defined_symbol->n_type & N_TYPE) != N_ABS)
		    value += defined_image->vmaddr_slide;
#ifdef DYLD_PROFILING
		if(strcmp(symbol_name, "__exit") == 0)
		    value = (unsigned long)profiling_exit +
			    dyld_image_vmaddr_slide;
#endif
		*((long *)lazy_symbol_pointer_address) = value;
		DYLD_TRACE_SYMBOLS_END(DYLD_TRACE_bind_lazy_symbol_reference);
		/* release lock for dyld data structures */
		release_lock();
		return(value);
	    }
	}

	if(dyld_prebind_debug > 1)
	    print("dyld: %s: linking in non-prebound lazy symbol: %s\n",
		   executables_name, symbol_name);

	/*
	 * This symbol is not defined in a linked module.  So put it on the
	 * undefined list and link in the needed modules.
	 */
	if(force_flat_namespace == FALSE)
	    flat_reference = (image->mh->flags & MH_TWOLEVEL) != MH_TWOLEVEL;
	else
	    flat_reference = TRUE;
	add_to_undefined_list(symbol_name, symbol, image, FALSE,flat_reference);
	link_in_need_modules(FALSE, FALSE);

	/*
	 * Now that all the needed module are linked in there can't be any
	 * undefineds left.  Therefore the lookup_symbol can't fail.
	 */
	lookup_symbol(symbol_name, get_primary_image(image, symbol),
		      get_hint(image, symbol), get_weak(symbol),
		      &defined_symbol, &defined_module, &defined_image,
		      &defined_library_image, NULL);
	value = defined_symbol->n_value;
	if((defined_symbol->n_type & N_TYPE) != N_ABS)
	    value += defined_image->vmaddr_slide;
#ifdef DYLD_PROFILING
	if(strcmp(symbol_name, "__exit") == 0)
	    value = (unsigned long)profiling_exit + dyld_image_vmaddr_slide;
#endif
	DYLD_TRACE_SYMBOLS_END(DYLD_TRACE_bind_lazy_symbol_reference);
	/* release lock for dyld data structures */
	release_lock();
	return(value);
}

void
bind_symbol_by_name(
char *symbol_name,
unsigned long *address,
module_state **module,
struct nlist **symbol,
enum bool change_symbol_pointers)
{
    struct nlist *defined_symbol;
    module_state *defined_module;
    struct image *defined_image;
    struct library_image *defined_library_image;
    enum link_state link_state;
    unsigned long value;

	/* set lock for dyld data structures */
	set_lock();

	DYLD_TRACE_SYMBOLS_ADDRESSED_START(DYLD_TRACE_bind_symbol_by_name,
					   address);

	lookup_symbol(symbol_name, NULL, NULL, FALSE, &defined_symbol,
		      &defined_module, &defined_image, &defined_library_image,
		      NULL);
	if(defined_symbol != NULL){
	    link_state = GET_LINK_STATE(*defined_module);
	    if(link_state == LINKED || link_state == FULLY_LINKED){
		value = defined_symbol->n_value;
		if((defined_symbol->n_type & N_TYPE) != N_ABS)
		    value += defined_image->vmaddr_slide;
		if(change_symbol_pointers == TRUE)
		    change_symbol_pointers_in_flat_images(symbol_name,
							  value, FALSE);
		if(address != NULL)
		    *address = value;
		if(module != NULL)
		    *module = defined_module;
		if(symbol != NULL)
		    *symbol = defined_symbol;
                    
		DYLD_TRACE_SYMBOLS_END(DYLD_TRACE_bind_symbol_by_name);
		/* release lock for dyld data structures */
		release_lock();
		return;
	    }
	}

	if(dyld_prebind_debug > 1)
	    print("dyld: %s: linking in bind by name symbol: %s\n",
		   executables_name, symbol_name);

	/*
	 * This symbol is not defined in a linked module.  So put it on the
	 * undefined list and link in the needed modules.
	 */
	add_to_undefined_list(symbol_name, NULL, NULL, FALSE, TRUE);
	link_in_need_modules(FALSE, FALSE);

	/*
	 * Now that all the needed modules are linked in there can't be any
	 * undefineds left.  Therefore the lookup_symbol can't fail.
	 */
	lookup_symbol(symbol_name, NULL, NULL, FALSE, &defined_symbol,
		      &defined_module, &defined_image, &defined_library_image,
		      NULL);
	value = defined_symbol->n_value;
	if((defined_symbol->n_type & N_TYPE) != N_ABS)
	    value += defined_image->vmaddr_slide;
	if(address != NULL)
	    *address = value;
	if(module != NULL)
	    *module = defined_module;
	if(symbol != NULL)
	    *symbol = defined_symbol;
	DYLD_TRACE_SYMBOLS_END(DYLD_TRACE_bind_symbol_by_name);
	/* release lock for dyld data structures */
	release_lock();
}

/*
 * link_in_need_modules() causes any needed modules to be linked into the
 * program.  This causes resolution of undefined symbols, relocation modules
 * that got linked in, checking for and reporting of undefined symbols and
 * module initialization routines to be called.  bind_now is TRUE only when
 * fully binding an image, the normal case when lazy binding is to occur it
 * is FALSE.
 *
 * Before being called the lock for the dyld data structures must be set and
 * either:
 * 	1) symbols placed on the undefined list or
 *	2) a module's state is set to BEING_LINKED and it's non-lazy symbols
 * 	   have been placed on the undefined list (if bind_now then it's lazy
 *	   symbols have also been placed on the undefined list).
 *
 * The return value is only used when return_on_error is TRUE.  In this case
 * a return value of TRUE indicates success and a return value of FALSE
 * indicates failure and that all change have been backed out.
 */
enum bool
link_in_need_modules(
enum bool bind_now,
enum bool release_lock_flag)
{
    enum bool tried_to_use_prebinding_post_launch;

	DYLD_TRACE_SYMBOLS_START(DYLD_TRACE_link_in_need_modules);

	/*
	 * Resolve all non-lazy symbol references this program currently
	 * has so it can be continued.
	 */
	if(resolve_undefineds(bind_now, FALSE) == FALSE &&
	   return_on_error == TRUE){
	    /*
	     * Unload any dependent libraries previously loaded by
	     * load_dependent_libraries() with return_on_error set since we had
	     * errors resolving undefined symbols.
	     */
	    unload_remove_on_error_libraries();
            DYLD_TRACE_SYMBOLS_END(DYLD_TRACE_link_in_need_modules);
	    return(FALSE);
	}

	/*
 	 * If return_on_error is set check for undefineds now before relocation
	 * is done for modules being linked.  If there are undefineds:
	 * 	back out state changes to added library modules
	 * 	back out symbols added to being linked list
	 * 	back out symbols added to undefined list
	 * 	back out dependent libraries
	 */
	if(return_on_error == TRUE &&
	   check_and_report_undefineds() == FALSE){
	    clear_state_changes_to_the_modules();
	    clear_being_linked_list(TRUE);
	    clear_undefined_list(TRUE);
	    unload_remove_on_error_libraries();
            DYLD_TRACE_SYMBOLS_END(DYLD_TRACE_link_in_need_modules);
	    return(FALSE);
	}

	/*
	 * If when loading libraries and its dependent libraries we tried to
	 * use the prebinding then we need to cache this in a local variable.
	 * Then we need to setup the libraries that can have their prebinding
	 * used and undo those that could not.  Later we then need to cause the
	 * module init routines to be run in those prebound libraries. 
	 */
	tried_to_use_prebinding_post_launch = 
	    trying_to_use_prebinding_post_launch;
	if(tried_to_use_prebinding_post_launch == TRUE){
	    find_twolevel_prebound_lib_subtrees();
	    undo_prebound_images(TRUE);
	}

	/*
	 * Now do all the relocation of modules being linked to that resolved
	 * undefined symbols.
	 */
	relocate_modules_being_linked(FALSE);
	    /*
	     * TODO: for return on error: if relocation fails:
	     * back out state changes to added library modules
	     * back out symbols added to being linked list
	     * back out symbols added to undefined list
	     * back out dependent libraries
	     */

	/*
	 * Now check and report any non-lazy symbols that are still undefined.
	 */
	check_and_report_undefineds();

	/*
	 * If return_on_error is set clear remove_on_error for libraries now
	 * that no more errors can occur.  Then clear return_on_error.  This is
	 * done now because we will now call user code that can come back to
	 * dyld.
	 */
	if(return_on_error == TRUE){
	    clear_remove_on_error_libraries();
	    return_on_error = FALSE;
	}

	/*
	 * Now call the functions that were registered to be called when an
	 * image gets added.
	 */
	call_registered_funcs_for_add_images();

	/*
	 * Now call the functions that were registered to be called when a
	 * module gets linked.
	 */
	call_registered_funcs_for_linked_modules();

	/*
	 * Now call the image initialization routines for the images that have
	 * modules newly being used in them.
	 */
	call_image_init_routines(FALSE);

	/*
	 * First cause the the module init routines to be run for in those
	 * prebound libraries if we used the prebinding in some of them.
	 */
	if(tried_to_use_prebinding_post_launch == TRUE){
	    /*
	     * Now call module initialization routines for modules that have
	     * been linked in from the post launch prebound libraries being used
	     * prebound.
	     */
	    call_module_initializers(FALSE, FALSE, TRUE);
	    /*
	     * Now clear the trying_to_use_prebinding_post_launch variable and
	     * any libraries that had the trying_to_use_prebinding_post_launch
	     * field set.
	     */
	    clear_trying_to_use_prebinding_post_launch();
	}
	/*
	 * Now call module initialization routines for modules that have been
	 * linked in.
	 */
	call_module_initializers(FALSE, bind_now, FALSE);

	if(release_lock_flag){
	    /* release lock for dyld data structures */
	    release_lock();
	}

	DYLD_TRACE_SYMBOLS_END(DYLD_TRACE_link_in_need_modules);
	return(TRUE);
}

/*
 * check_executable_for_overrides() is called when trying to launch using just
 * prebound libraries and not having a prebound executable.  This checks to see
 * that the executable does not define a symbol that is defined and referenced
 * by a library it uses.  If there are no such symbols the executable can be
 * launch using just prebound libraries and TRUE is returned.  Else FALSE is
 * returned.
 */
enum bool
check_executable_for_overrides(
void)
{
    unsigned long i;
    char *symbol_name;
    struct nlist *symbols;
    struct segment_command *linkedit_segment;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    struct image *image;

	/*
	 * The executable is the first object on the object_image list.
	 */
	linkedit_segment = object_images.images[0].image.linkedit_segment;
	st = object_images.images[0].image.st;
	dyst = object_images.images[0].image.dyst;
	if(linkedit_segment == NULL || st == NULL || dyst == NULL)
	    return(TRUE);
	/* the vmaddr_slide of an executable is always 0, no need to add it */
	symbols = (struct nlist *)
	    (linkedit_segment->vmaddr +
	     st->symoff -
	     linkedit_segment->fileoff);
	image = &object_images.images[0].image;

	for(i = dyst->iextdefsym; i < dyst->iextdefsym + dyst->nextdefsym; i++){
	    symbol_name = (char *)
		(image->linkedit_segment->vmaddr +
		 image->st->stroff -
		 image->linkedit_segment->fileoff) +
		symbols[i].n_un.n_strx;
	    if(check_libraries_for_definition_and_refernce(symbol_name, NULL) ==
	       TRUE)
		return(FALSE);
	}
	return(TRUE);
}

/*
 * check_libraries_for_overrides() is called when trying to launch using just
 * prebound libraries and not having a prebound executable.  This checks to see
 * that the libraries do not define a symbols that are defined and referenced
 * by a library it uses.  If there are no such symbols the executable can be
 * launch using just prebound libraries and TRUE is returned.  Else FALSE is
 * returned.
 */
enum bool
check_libraries_for_overrides(
void)
{
    unsigned long i, j;
    struct library_images *q;
    struct library_image *library_image;
    struct segment_command *linkedit_segment;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    struct dylib_table_of_contents *tocs;
    struct nlist *symbols;
    char *strings, *symbol_name;

	/*
	 * Look in the library_images for a definition of symbol_name.
	 */
	q = &library_images;
	do{
	    for(i = 0; i < q->nimages; i++){
		library_image = &(q->images[i]);
		linkedit_segment = library_image->image.linkedit_segment;
		st = library_image->image.st;
		dyst = library_image->image.dyst;

		tocs = (struct dylib_table_of_contents *)
		    (library_image->image.vmaddr_slide +
		     linkedit_segment->vmaddr +
		     dyst->tocoff -
		     linkedit_segment->fileoff);
		symbols = (struct nlist *)
		    (library_image->image.vmaddr_slide +
		     linkedit_segment->vmaddr +
		     st->symoff -
		     linkedit_segment->fileoff);
		strings = (char *)
		    (library_image->image.vmaddr_slide +
		     linkedit_segment->vmaddr +
		     st->stroff -
		     linkedit_segment->fileoff);
		for(j = 0; j < dyst->ntoc; j++){
		    symbol_name = strings +
				  symbols[tocs[j].symbol_index].n_un.n_strx;
		    if(check_libraries_for_definition_and_refernce(symbol_name,
							library_image) == TRUE)
			return(FALSE);
		}
	    }
	    q = q->next_images;
	}while(q != NULL);
	return(TRUE);
}

/*
 * check_libraries_for_definition_and_refernce() is used to check that the
 * symbol name is defined and referenced in the same library for all the
 * libraries being used.  If there is such a library TRUE is returned else
 * FALSE is returned.  If we are checking symbols from a specific library
 * then library_image is set to that and we ignore that library image.
 */
static
enum bool
check_libraries_for_definition_and_refernce(
char *symbol_name,
struct library_image *library_image)
{
    unsigned long i, j;
    struct library_images *q;
    struct segment_command *linkedit_segment;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    struct dylib_table_of_contents *tocs, *toc;
    struct dylib_reference *dylib_references;

	/*
	 * Look in the library_images for a definition of symbol_name.
	 */
	q = &library_images;
	do{
	    for(i = 0; i < q->nimages; i++){
		if(library_image == &(q->images[i]))
		    continue;
		linkedit_segment = q->images[i].image.linkedit_segment;
		st = q->images[i].image.st;
		dyst = q->images[i].image.dyst;

		tocs = (struct dylib_table_of_contents *)
		    (q->images[i].image.vmaddr_slide +
		     linkedit_segment->vmaddr +
		     dyst->tocoff -
		     linkedit_segment->fileoff);
		toc_bsearch_symbols = (struct nlist *)
		    (q->images[i].image.vmaddr_slide +
		     linkedit_segment->vmaddr +
		     st->symoff -
		     linkedit_segment->fileoff);
		toc_bsearch_strings = (char *)
		    (q->images[i].image.vmaddr_slide +
		     linkedit_segment->vmaddr +
		     st->stroff -
		     linkedit_segment->fileoff);
		toc = inline_bsearch_toc(symbol_name, tocs, dyst->ntoc);
		if(toc != NULL){
		    /*
		     * This library has a definition of this symbol see if it
		     * also has a reference.
		     */
		    dylib_references = (struct dylib_reference *)
			(q->images[i].image.vmaddr_slide +
			 linkedit_segment->vmaddr +
			 dyst->extrefsymoff -
			 linkedit_segment->fileoff);
		    for(j = 0; j < dyst->nextrefsyms; j++){
			if(dylib_references[j].isym == toc->symbol_index &&
			   dylib_references[j].flags != REFERENCE_FLAG_DEFINED){
			    if(dyld_prebind_debug != 0 && prebinding == TRUE)
				print("dyld: %s: trying to use prebound "
				    "libraries failed due to overridden "
				    "symbols\n", executables_name);
			    prebinding = FALSE;
			    return(TRUE);
			}
		    }
		}
	    }
	    q = q->next_images;
	}while(q != NULL);
	return(FALSE);
}

/*
 * discard_symbol() takes a pointer to an image and a pointer to a symbol in
 * that image and marks the symbol discarded.  This is a very expensive thing
 * to do and is only used in error or very rare cases.  It dirties the link
 * edit segment to avoid having to have bits for each symbol in each image.
 */
void
discard_symbol(
struct image *image,
struct nlist *symbol)
{
    kern_return_t r;

	/* make sure the linkedit segment for image is writable */
	if((r = vm_protect(mach_task_self(),
	    image->linkedit_segment->vmaddr +
	    image->vmaddr_slide,
	    (vm_size_t)image->linkedit_segment->vmsize, FALSE,
	    VM_PROT_WRITE | VM_PROT_READ)) != KERN_SUCCESS){
	    mach_error(r, "can't set vm_protection on segment: %.16s "
		"for: %s", image->linkedit_segment->segname,
		image->name);
	    link_edit_error(DYLD_MACH_RESOURCE, r, image->name);
	}

	/* mark the symbol as discarded in the n_desc */
	symbol->n_desc |= N_DESC_DISCARDED;

	if((r = vm_protect(mach_task_self(),
	    image->linkedit_segment->vmaddr +
	    image->vmaddr_slide,
	    (vm_size_t)image->linkedit_segment->vmsize, FALSE,
	    image->linkedit_segment->initprot)) != KERN_SUCCESS){
	    mach_error(r, "can't set vm_protection on segment: %.16s "
		"for: %s", image->linkedit_segment->segname,
		image->name);
	    link_edit_error(DYLD_MACH_RESOURCE, r, image->name);
	}
}
