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
#import <string.h>
#import <mach/mach.h>
#import <mach-o/loader.h>
#import <mach-o/nlist.h>
#import <mach-o/reloc.h>
#import <mach-o/dyld_debug.h>
#if !defined(__GONZO_BUNSEN_BEAKER__) && !defined(__HERA__) && defined(__ppc__)
#include <architecture/ppc/processor_facilities.h>
#endif

#import "images.h"
#import "mod_init_funcs.h"
#import "errors.h"
#import "lock.h"
#import "dyld_init.h"
#ifdef __ppc__
#include "fp_save_restore.h"
#endif /* __ppc__ */

/*
 * The calls to the module initialization routines start off delayed so that
 * the initializtion in the runtime start off is done.  When that is done the
 * runtime startoff calls call_module_initializers(TRUE) and then sets
 * delay_mod_init to FALSE. 
 */
static enum bool delay_mod_init = TRUE;

static enum bool call_module_initializers_for_objects(
    enum bool make_delayed_calls,
    enum bool bind_now);

/*
 * call_module_initializers() calls the module initialization routines for
 * modules that are in the registered state and sets them to the linked state
 * when make_delayed_calls is FALSE.  When make_delayed_calls is TRUE this makes
 * the calls to the module initialization routines that have been delayed and
 * sets delay_mod_init to FALSE when done.
 */
void
call_module_initializers(
enum bool make_delayed_calls,
enum bool bind_now)
{
    unsigned long i;
    struct library_images *q;
    unsigned long *init;
#ifdef __ppc__
    double fp_save_area[N_FP_REGS];
    /* we can't use -fvec because "bool" is a keyword when -fvec is used */
    /* vector unsigned long vec_save_area[N_VEC_REGS]; */
    unsigned long vec_save_area[N_VEC_REGS * 4] __attribute__ ((aligned(16)));
    enum bool saved_regs = FALSE;
#if !defined(__GONZO_BUNSEN_BEAKER__) && !defined(__HERA__)
    int facilities_used = -1;
#endif /* !defined(__GONZO_BUNSEN_BEAKER__) && !defined(__HERA__) */
#endif /* __ppc__ */

	/*
	 * First for the object_images modules that are in the registered state
	 * call their module initialization routines.  Because objects can be
	 * unloaded and we have to release the lock to call the functions
	 * we need to repeatedly pass through the data structures for the loaded
	 * objects after each call until we get through cleanly.
	 */
	while(call_module_initializers_for_objects(
		make_delayed_calls, bind_now) == TRUE)
	    ;

	/*
	 * Next for the library modules that are in the registered state call
	 * their module initialization routines.  Note libraries can't be
	 * unloaded.
	 */
	init = NULL;
	q = &library_images;
	do{
	    for(i = 0; i < q->nimages; i++){
		call_module_initializers_for_library(
			q->images + i,
#ifdef __ppc__
			fp_save_area,
			vec_save_area,
			&saved_regs,
#if !defined(__GONZO_BUNSEN_BEAKER__) && !defined(__HERA__)
			&facilities_used,
#endif /* !defined(__GONZO_BUNSEN_BEAKER__) && !defined(__HERA__) */
#endif /* __ppc__ */
			make_delayed_calls,
			bind_now);
	    }
	    q = q->next_images;
	}while(q != NULL);

	if(make_delayed_calls == TRUE)
	    delay_mod_init = FALSE;

#ifdef __ppc__
	if(saved_regs == TRUE){
#if !defined(__GONZO_BUNSEN_BEAKER__) && !defined(__HERA__)
	    if(facilities_used & floatUsed)
#endif
		ppc_fp_restore(fp_save_area);
#if defined(__GONZO_BUNSEN_BEAKER__) || defined(__HERA__)
	    if(processor_has_vec == TRUE)
#else
	    if(_cpu_has_altivec == TRUE && (facilities_used & vectorUsed))
#endif
		ppc_vec_restore(vec_save_area);
	}
#endif /* __ppc__ */
}

/*
 * call_module_initializers_for_library() calls the module initializers for the
 * specified library image.  The caller passes in pointers to the buffers for
 * saving the fp (fp_save_area) and vector (vec_save_area) registers and
 * pointer boolean (saved_regs) to know if the registers have already been
 * saved.  Also on Kodiak and later releases a pointer to facilities_used is
 * passed in to be set determine which register sets to save.  The prameters
 * make_delayed_calls and bind_now are the same as the routine 
 * call_module_initializers() above.
 */
void
call_module_initializers_for_library(
struct library_image *library_image,
#ifdef __ppc__
double *fp_save_area,
unsigned long *vec_save_area,
enum bool *saved_regs,
#if !defined(__GONZO_BUNSEN_BEAKER__) && !defined(__HERA__)
int *facilities_used,
#endif /* !defined(__GONZO_BUNSEN_BEAKER__) && !defined(__HERA__) */
#endif /* __ppc__ */
enum bool make_delayed_calls,
enum bool bind_now)
{
    unsigned long j, k;
    enum link_state link_state;
    struct segment_command *linkedit_segment;
    struct dysymtab_command *dyst;
    struct dylib_module *dylib_modules;
    unsigned long addr, *init;
    void (*func)(void);

	linkedit_segment = library_image->image.linkedit_segment;
	dyst = library_image->image.dyst;
	dylib_modules = (struct dylib_module *)
	    (library_image->image.vmaddr_slide +
	     linkedit_segment->vmaddr +
	     dyst->modtaboff -
	     linkedit_segment->fileoff);
	/*
	 * First set the address of the section with the module
	 * initialization pointers in the image.
	 */
	init = NULL;
	if(library_image->image.init != NULL)
	    init = (unsigned long *)(library_image->image.init->addr +
				     library_image->image.vmaddr_slide);
	for(j = 0; j < dyst->nmodtab; j++){
	    /* skip modules that that not in the registered state */
	    link_state = GET_LINK_STATE(library_image->modules[j]);

	    if(link_state != REGISTERING &&
	       (make_delayed_calls == FALSE ||
		(link_state != LINKED && link_state != FULLY_LINKED)))
		continue;

	    if(link_state == REGISTERING)
		SET_LINK_STATE(library_image->modules[j], INITIALIZING);

	    if(delay_mod_init == FALSE || make_delayed_calls == TRUE){
		if(GET_MODINIT_STATE(library_image->modules[j]) == 0 &&
		   library_image->image.lazy_init == FALSE){
		    SET_MODINIT_STATE(library_image->modules[j]);
		    for(k = 0;
			k < (dylib_modules[j].ninit_nterm & 0xffff);
			k++){
			addr = init[(dylib_modules[j].iinit_iterm &
				     0xffff) + k];
#ifdef __ppc__
#if !defined(__GONZO_BUNSEN_BEAKER__) && !defined(__HERA__)
			if(*facilities_used == -1)
			     *facilities_used =
				processor_facilities_used();
#endif
			if(*saved_regs == FALSE){
#if !defined(__GONZO_BUNSEN_BEAKER__) && !defined(__HERA__)
			    if(*facilities_used & floatUsed)
#endif
				ppc_fp_save(fp_save_area);
#if defined(__GONZO_BUNSEN_BEAKER__) || defined(__HERA__)
			    if(processor_has_vec == TRUE)
#else
			    if(_cpu_has_altivec == TRUE &&
			       (*facilities_used & vectorUsed))
#endif
				ppc_vec_save(vec_save_area);
			    *saved_regs = TRUE;
			}
#endif /* __ppc__ */
			func = (void(*)(void))addr;
			release_lock();
			func();
			set_lock();
		    }
		}
	    }

	    link_state = GET_LINK_STATE(library_image->modules[j]);
	    if(link_state == INITIALIZING){
		if(bind_now == FALSE)
		    SET_LINK_STATE(library_image->modules[j], LINKED);
		else
		    SET_LINK_STATE(library_image->modules[j],
				   FULLY_LINKED);
	    }
	}
}

/*
 * call_module_initializers_for_objects() goes through the list of objects and
 * finds an object that is in the registered state and then calles it's module
 * initilization functions for at most one object image.  If a functions are
 * call TRUE is returned.  If no functions are called (because all object
 * images have had their module initilization functions called) then FALSE is
 * returned.  Since objects can only be loaded and linked programmatically and
 * their module initilization functions are called when loaded, there is no
 * need to save the fp or vec registers.
 */
static
enum bool
call_module_initializers_for_objects(
enum bool make_delayed_calls,
enum bool bind_now)
{
    unsigned long i, j, n;
    enum link_state link_state;
    struct object_images *p;
    unsigned long slide_value, addr;
    void (*func)(void);

	p = &object_images;
	do{
	    for(i = 0; i < p->nimages; i++){
		/* skip modules that that not in the registered state */
		link_state = GET_LINK_STATE(p->images[i].module);

		if(link_state != REGISTERING &&
		   (make_delayed_calls == FALSE ||
		    (link_state != LINKED && link_state != FULLY_LINKED)))
		    continue;

		if(link_state == REGISTERING){
		    SET_LINK_STATE(p->images[i].module, INITIALIZING);
		    if(p->images[i].image.init == NULL){
			if(bind_now == FALSE)
			    SET_LINK_STATE(p->images[i].module, LINKED);
			else
			    SET_LINK_STATE(p->images[i].module, FULLY_LINKED);
			continue;
		    }
		}

		if(p->images[i].image.init == NULL)
		    continue;

		if(delay_mod_init == FALSE || make_delayed_calls == TRUE){
		    slide_value = p->images[i].image.vmaddr_slide;
		    n = p->images[i].image.init->size / sizeof(unsigned long);
		    for(j = 0; j < n; j++){
			addr = *((long *)
			     (p->images[i].image.init->addr + slide_value) + j);
			func = (void(*)(void))addr;
			release_lock();
			func();
			set_lock();
		    }
		}
		link_state = GET_LINK_STATE(p->images[i].module);
		if(link_state == INITIALIZING){
		    if(bind_now == FALSE)
			SET_LINK_STATE(p->images[i].module, LINKED);
		    else
			SET_LINK_STATE(p->images[i].module, FULLY_LINKED);
		}

		if(make_delayed_calls == FALSE)
		    return(TRUE);
	    }
	    p = p->next_images;
	}while(p != NULL);
	return(FALSE);
}

/*
 * call_module_terminator_for_object() is part of the sort term work to make
 * module termination functions work for NSUnLinkModule() ONLY.
 *
 * It is passed a pointer to an object image and calls the module termination
 * functions if it has any.
 */
void
call_module_terminator_for_object(
struct object_image *object_image)
{
    unsigned long i, n;
    unsigned long slide_value, addr;
    void (*func)(void);

	if(object_image->image.term == NULL)
	    return;

	slide_value = object_image->image.vmaddr_slide;
	n = object_image->image.term->size / sizeof(unsigned long);
	for(i = 0; i < n; i++){
	    addr = *((long *)
		(object_image->image.term->addr + slide_value) + i);
	    func = (void(*)(void))addr;
	    release_lock();
	    func();
	    set_lock();
	}
}

/*
 * _dyld_call_module_initializers_for_dylib() is the dyld side of
 * __initialize_Cplusplus() which is in dylib1.o .  Since this is called
 * directly by there is no need to save any fp or vec registers.
 */
void
_dyld_call_module_initializers_for_dylib(
struct mach_header *mh_dylib_header)
{
    unsigned long i, j, k;
    enum link_state link_state;
    struct library_images *q;
    struct segment_command *linkedit_segment;
    struct dysymtab_command *dyst;
    struct dylib_module *dylib_modules;
    unsigned long addr, *init;
    void (*func)(void);

	/* set lock for dyld data structures */
	set_lock();

	/*
	 * Look through the library modules for the specified library's mach
	 * header.  If found then call the module init routines for that
	 * library.
	 */
	init = NULL;
	q = &library_images;
	do{
	    for(i = 0; i < q->nimages; i++){
		if(q->images[i].image.mh != mh_dylib_header)
		    continue;

		linkedit_segment = q->images[i].image.linkedit_segment;
		dyst = q->images[i].image.dyst;
		dylib_modules = (struct dylib_module *)
		    (q->images[i].image.vmaddr_slide +
		     linkedit_segment->vmaddr +
		     dyst->modtaboff -
		     linkedit_segment->fileoff);
		/*
		 * First set the address of the section with the module
		 * initialization pointers in the image.
		 */
		if(q->images[i].image.init != NULL)
		    init = (unsigned long *)(q->images[i].image.init->addr +
					     q->images[i].image.vmaddr_slide);
		for(j = 0; j < dyst->nmodtab; j++){
		    /*
		     * This routine is called from a image init routine to force
		     * the module init routines to be called.  So the modules
		     * that need the module init routines run will be in the
		     * REGISTERING state.  So skip modules that that not in the
		     * REGISTERING state.
		     */
		    link_state = GET_LINK_STATE(q->images[i].modules[j]);
		    if(link_state != REGISTERING)
			continue;

		    if(GET_MODINIT_STATE(q->images[i].modules[j]) != 0)
			continue;
		    SET_MODINIT_STATE(q->images[i].modules[j]);

		    for(k = 0; k < (dylib_modules[j].ninit_nterm & 0xffff);k++){
			addr = init[(dylib_modules[j].iinit_iterm & 0xffff)+ k];
			func = (void(*)(void))addr;
			release_lock();
			func();
			set_lock();
		    }
		}
		/*
		 * Since we found the mach header and ran the module init
		 * routines we are done and can return.
		 */
		/* release lock for dyld data structures */
		release_lock();
		return;
	    }
	    q = q->next_images;
	}while(q != NULL);

	/* release lock for dyld data structures */
	release_lock();
}

/*
 * _dyld_mod_term_funcs() is the function that gets registered with atexit() in
 * crt1.o to cause module termination functions to be called.  Since this is
 * called directly by there is no need to save any fp or vec registers.
 */
void
_dyld_mod_term_funcs(
void)
{
    unsigned long i, j, k;
    enum bool clean_pass;
    enum link_state link_state;
    struct object_images *p;
    unsigned long slide_value, addr, *term;
    void (*func)(void);
    struct library_images *q;
    struct segment_command *linkedit_segment;
    struct dysymtab_command *dyst;
    struct dylib_module *dylib_modules;

	/* set lock for dyld data structures */
	set_lock();
top:
	clean_pass = TRUE;

	p = &object_images;
	do{
	    for(i = 0; i < p->nimages; i++){
		/*
		 * Skip modules that that not in the link or fully linked
		 * state.
		 */
		link_state = GET_LINK_STATE(p->images[i].module);
		if(link_state != LINKED && link_state != FULLY_LINKED)
		    continue;

		/*
		 * If this module's termination routines have already been
		 * called then slip it.  Otherwise mark it and call the
		 * routines.
		 */
		if(GET_MODTERM_STATE(p->images[i].module) != 0)
		    continue;
		SET_MODTERM_STATE(p->images[i].module);

		if(p->images[i].image.term == NULL)
		    continue;

		slide_value = p->images[i].image.vmaddr_slide;
		k = p->images[i].image.term->size / sizeof(unsigned long);
		for(j = 0; j < k; j++){
		    addr = *((long *)
			 (p->images[i].image.term->addr + slide_value) + j);
		    func = (void(*)(void))addr;
		    release_lock();
		    func();
		    set_lock();
		}
		clean_pass = FALSE;
	    }
	    p = p->next_images;
	}while(p != NULL);

	term = NULL;
	q = &library_images;
	do{
	    for(i = 0; i < q->nimages; i++){
		linkedit_segment = q->images[i].image.linkedit_segment;
		dyst = q->images[i].image.dyst;
		dylib_modules = (struct dylib_module *)
		    (q->images[i].image.vmaddr_slide +
		     linkedit_segment->vmaddr +
		     dyst->modtaboff -
		     linkedit_segment->fileoff);
		/*
		 * First set the address of the section with the module
		 * termination pointers in the image.
		 */
		if(q->images[i].image.term != NULL)
		    term = (unsigned long *)(q->images[i].image.term->addr +
					     q->images[i].image.vmaddr_slide);
		for(j = 0; j < dyst->nmodtab; j++){
		    /*
		     * Skip modules that that not in the link or fully linked
		     * state.
		     */
		    link_state = GET_LINK_STATE(q->images[i].modules[j]);
		    if(link_state != LINKED && link_state != FULLY_LINKED)
			continue;

		    if(GET_MODTERM_STATE(q->images[i].modules[j]) != 0)
			continue;
		    SET_MODTERM_STATE(q->images[i].modules[j]);

		    for(k = 0;
			k < ((dylib_modules[j].ninit_nterm >> 16) & 0xffff);
			k++){
			addr = term[((dylib_modules[j].iinit_iterm >> 16)
				     & 0xffff) + k];
			func = (void(*)(void))addr;
			release_lock();
			func();
			set_lock();
			clean_pass = FALSE;
		    }
		}
	    }
	    q = q->next_images;
	}while(q != NULL);

	/*
	 * Since modules maybe linked in when termination routines run we have
	 * make sure we have done a clean pass through them to know all of them
	 * have been called.
	 */
	if(clean_pass == FALSE)
	    goto top;

	/* release lock for dyld data structures */
	release_lock();
}
