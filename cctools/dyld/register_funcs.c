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
#import <stdio.h>
#import <mach/mach.h>

#import "register_funcs.h"
#import "images.h"
#import "allocate.h"
#import "lock.h"
#import "trace.h"

/*
 * Since there typically only one function registered from the Objective-C
 * runtime or one for the zero overhead C++ execption mechanism for each of
 * these a struct with one function pointer is staticly allocated so normally
 * malloc will never be called.
 */
struct add_image_func {
    void (*func)(struct mach_header *mh, unsigned long vmaddr_slide);
    struct add_image_func *next;
};
static struct add_image_func add_image_func_list = { NULL, NULL };

struct remove_image_func {
    void (*func)(struct mach_header *mh, unsigned long vmaddr_slide);
    struct remove_image_func *next;
};
static struct remove_image_func remove_image_func_list = { NULL, NULL };

struct link_module_func {
    void (*func)(module_state *module);
    struct link_module_func *next;
};
static struct link_module_func link_module_func_list = { NULL, NULL };

struct unlink_module_func {
    void (*func)(module_state *module);
    struct unlink_module_func *next;
};
static struct unlink_module_func unlink_module_func_list = { NULL, NULL };

struct replace_module_func {
    void (*func)(module_state *oldmodule, module_state *newmodule);
    struct replace_module_func *next;
};
static struct replace_module_func replace_module_func_list = { NULL, NULL };

static enum bool call_registered_funcs_for_add_images_in_objects(
    void);
static void call_funcs_for_add_image(
    struct mach_header *mh,
    unsigned long vmaddr_slide);
static enum bool call_registered_funcs_for_linked_modules_in_objects(
    void);
static void call_funcs_for_link_module(
    module_state *module);

/*
 * register_func_for_add_image() adds the specified function pointer to the
 * list of functions to be called when images are added and then calls it for
 * each image currently in the program.
 */
void
register_func_for_add_image(
void (*func)(struct mach_header *mh, unsigned long vmaddr_slide))
{
    struct add_image_func *add_image_func;
    unsigned long i;
    struct object_images *p;
    struct library_images *q;
    enum link_state link_state;

	/*
	 * Add this func to the list of functions to be called when images are
	 * added to this program.
	 */

	/* set lock for dyld data structures */
	set_lock();

	if(add_image_func_list.func == NULL){
	    add_image_func_list.func = func;
	}
	else{
	    add_image_func = allocate(sizeof(struct add_image_func));
	    add_image_func->func = func;
	    add_image_func->next = add_image_func_list.next;
	    add_image_func_list.next = add_image_func;
	}

	/* TODO: fix this to be lock safe when objects get unloaded */
	/*
	 * Call this func for the images currently part of the program.
	 */
	for(p = &object_images ; ; p = p->next_images){
	    for(i = 0; i < p->nimages; i++){
		link_state = GET_LINK_STATE(p->images[i].module);
		if(link_state == UNUSED)
		    continue;
		DYLD_TRACE_CALLOUT_START(DYLD_TRACE_object_func, func);
		release_lock();
		func(p->images[i].image.mh, p->images[i].image.vmaddr_slide);
		set_lock();
		DYLD_TRACE_CALLOUT_END(DYLD_TRACE_object_func, func);
	    }
	    if(p->next_images == NULL)
		break;
	}
	for(q = &library_images ; ; q = q->next_images){
	    for(i = 0; i < q->nimages; i++){
		release_lock();
		DYLD_TRACE_CALLOUT_START(DYLD_TRACE_library_func, func);
		func(q->images[i].image.mh, q->images[i].image.vmaddr_slide);
		DYLD_TRACE_CALLOUT_END(DYLD_TRACE_library_func, func);
		set_lock();
	    }
	    if(q->next_images == NULL)
		break;
	}

	/* release lock for dyld data structures */
	release_lock();
}

/*
 * register_func_for_remove_image() adds the specified function pointer to the
 * list of functions to be called when images are removed.
 */
void
register_func_for_remove_image(
void (*func)(struct mach_header *mh, unsigned long vmaddr_slide))
{
    struct remove_image_func *remove_image_func;

	/*
	 * Add this func to the list of functions to be called when images are
	 * removed from this program.
	 */

	/* set lock for dyld data structures */
	set_lock();

	if(remove_image_func_list.func == NULL){
	    remove_image_func_list.func = func;
	}
	else{
	    remove_image_func = allocate(sizeof(struct remove_image_func));
	    remove_image_func->func = func;
	    remove_image_func->next = remove_image_func_list.next;
	    remove_image_func_list.next = remove_image_func;
	}

	/* release lock for dyld data structures */
	release_lock();
}

/*
 * call_registered_funcs_for_add_images() calls the functions registered to
 * be called when images get added for the images that have not had these
 * called.
 */
void
call_registered_funcs_for_add_images(
void)
{
    unsigned long i;
    struct library_images *q;

	/*
	 * First for the object_images that have not had the functions
	 * registered to be called call them.  Because objects can be unloaded
	 * and we have to release the lock to call the registered functions
	 * we need to repeatedly pass through the data structures for the loaded
	 * objects after each call until we get through cleanly.
	 */
	while(call_registered_funcs_for_add_images_in_objects() == TRUE)
	    ;

	/*
	 * Next for the library modules that have not had the functions
	 * registered to be called call them.  Note libraries can't be unloaded.
	 */
	q = &library_images;
	do{
	    for(i = 0; i < q->nimages; i++){
		if(q->images[i].image.registered == FALSE){
		    q->images[i].image.registered = TRUE;
		    call_funcs_for_add_image(q->images[i].image.mh,
					     q->images[i].image.vmaddr_slide);
		}
	    }
	    q = q->next_images;
	}while(q != NULL);
}

/*
 * call_registered_funcs_for_add_images_in_objects() goes through the list of
 * objects and finds an object that has not had the functions registered to
 * be called for add images and call the functions for at most one object
 * image.  If a functions are call TRUE is returned.  If no functions are called
 * (because all object images have had the registered functions called) then
 * FALSE is returned.
 */
static
enum bool
call_registered_funcs_for_add_images_in_objects(
void)
{
    unsigned long i;
    struct object_images *p;
    enum link_state link_state;

	p = &object_images;
	do{
	    for(i = 0; i < p->nimages; i++){
		link_state = GET_LINK_STATE(p->images[i].module);
		if(link_state == UNUSED)
		    continue;
		if(p->images[i].image.registered == FALSE){
		    p->images[i].image.registered = TRUE;
		    call_funcs_for_add_image(p->images[i].image.mh,
					     p->images[i].image.vmaddr_slide);
		    return(TRUE);
		}
	    }
	    p = p->next_images;
	}while(p != NULL);
	return(FALSE);
}

/*
 * call_funcs_for_add_image() calls each of the functions that was registered
 * to be called when images are added for the new image now being added to the
 * program.
 */
static
void
call_funcs_for_add_image(
struct mach_header *mh,
unsigned long vmaddr_slide)
{
    struct add_image_func *add_image_func;

	for(add_image_func = &add_image_func_list;
	    add_image_func != NULL && add_image_func->func != NULL;
	    add_image_func = add_image_func->next){

	    /*
	     * Since functions can only be added and not removed the 
	     * add_image_func_list will be valid when we get the lock back and
	     * at worse may contain more functions than we started the loop.
	     * If this happens a function will get called twice on the same
	     * image.
	     */
	    release_lock();
            DYLD_TRACE_CALLOUT_START(DYLD_TRACE_add_image_func,
		add_image_func->func);
	    add_image_func->func(mh, vmaddr_slide);
            DYLD_TRACE_CALLOUT_END(DYLD_TRACE_add_image_func,
		add_image_func->func);
	    set_lock();
	}
}

/*
 * call_funcs_for_remove_image() calls each of the functions that was registered
 * to be called when images are removed for the image now being removed from the
 * program.
 */
void
call_funcs_for_remove_image(
struct mach_header *mh,
unsigned long vmaddr_slide)
{
    struct remove_image_func *remove_image_func;

	for(remove_image_func = &remove_image_func_list;
	    remove_image_func != NULL && remove_image_func->func != NULL;
	    remove_image_func = remove_image_func->next){

            DYLD_TRACE_CALLOUT_START(DYLD_TRACE_remove_image_func,
		remove_image_func->func);
	    /*
	     * Since functions can only be added and not removed the 
	     * remove_image_func_list will be valid when we get the lock back
	     * and at worse may contain more functions than we started the loop.
	     * If this happens a function will get called twice on the same
	     * image.
	     */
	    release_lock();
	    remove_image_func->func(mh, vmaddr_slide);
	    set_lock();
	    DYLD_TRACE_CALLOUT_END(DYLD_TRACE_remove_image_func,
		remove_image_func->func);
	}
}

/*
 * register_func_for_link_module() adds the specified function pointer to the
 * list of functions to be called when modules are bound and then calls it for
 * each module currently bound into the program.
 */
void
register_func_for_link_module(
void (*func)(module_state *module))
{
    struct link_module_func *link_module_func;
    unsigned long i, j;
    struct object_images *p;
    struct library_images *q;
    enum link_state link_state;

	/*
	 * Add this func to the list of functions to be called when modules are
	 * bound into this program.
	 */

	/* set lock for dyld data structures */
	set_lock();

	if(link_module_func_list.func == NULL){
	    link_module_func_list.func = func;
	}
	else{
	    link_module_func = allocate(sizeof(struct link_module_func));
	    link_module_func->func = func;
	    link_module_func->next = link_module_func_list.next;
	    link_module_func_list.next = link_module_func;
	}

	/* TODO: fix this to be lock safe when objects get unloaded */
	/*
	 * Call this func for the modules currently bound in the program.
	 */
	for(p = &object_images ; ; p = p->next_images){
	    for(i = 0; i < p->nimages; i++){
		/* skip modules that that are not linked */
		link_state = GET_LINK_STATE(p->images[i].module);
		if(link_state != LINKED && link_state != FULLY_LINKED)
		    continue;

		DYLD_TRACE_CALLOUT_START(DYLD_TRACE_link_object_module_func,
		    func);
		release_lock();
		func(&(p->images[i].module));
		set_lock();
		DYLD_TRACE_CALLOUT_END(DYLD_TRACE_link_object_module_func,
		    func);
	    }
	    if(p->next_images == NULL)
		break;
	}
	for(q = &library_images ; ; q = q->next_images){
	    for(i = 0; i < q->nimages; i++){
		for(j = 0; j < q->images[i].image.dyst->nmodtab; j++){
		    link_state = GET_LINK_STATE(q->images[i].modules[j]);
		    /* skip modules that that not being linked */
		    if(link_state != LINKED && link_state != FULLY_LINKED)
			continue;

                    DYLD_TRACE_CALLOUT_START(
			DYLD_TRACE_link_library_module_func, func);
		    release_lock();
		    func(q->images[i].modules + j);
		    set_lock();
		    DYLD_TRACE_CALLOUT_END(
			DYLD_TRACE_link_library_module_func, func);
		}
	    }
	    if(q->next_images == NULL)
		break;
	}
	/* release lock for dyld data structures */
	release_lock();
}

/*
 * call_registered_funcs_for_linked_modules() calls the functions registered to
 * be called when modules get linked for modules that are in the relocatated
 * state and sets them to the register state.
 */
void
call_registered_funcs_for_linked_modules(
void)
{
    unsigned long i, j;
    enum link_state link_state;
    struct library_images *q;
    struct segment_command *linkedit_segment;
    struct dysymtab_command *dyst;
    struct dylib_module *dylib_modules;

	/*
	 * First for the object_images modules that have been relocated call
	 * the functions registered to be called when modules get linked.
	 * Because objects can be unloaded and we have to release the lock to
	 * call the registered functions we need to repeatedly pass through the 
	 * data structures for the loaded objects after each call until we get
	 * through cleanly.
	 */
	while(call_registered_funcs_for_linked_modules_in_objects() == TRUE)
	    ;

	/*
	 * Next for the library modules that have been relocated call the
	 * functions registered to be called when modules get linked.
	 */
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

		for(j = 0; j < dyst->nmodtab; j++){

		    /* skip modules that that not in the relocated state */
		    link_state = GET_LINK_STATE(q->images[i].modules[j]);
		    /* skip modules that that not being linked */
		    if(link_state != RELOCATED)
			continue;
		    SET_LINK_STATE(q->images[i].modules[j], REGISTERING);

		    if(link_module_func_list.func == NULL)
			continue;

		    call_funcs_for_link_module(q->images[i].modules + j);
		}
	    }
	    q = q->next_images;
	}while(q != NULL);
}

/*
 * call_registered_funcs_for_linked_modules_in_objects() goes through the list
 * of objects and finds an object that is in the relocated state and then calls
 * the functions registered to be called for linked modules for at most one
 * object image.  If a functions are called TRUE is returned.  If no functions
 * are called (because there are no object images left in the relocated state)
 * then FALSE is returned.
 */
static
enum bool
call_registered_funcs_for_linked_modules_in_objects(
void)
{
    unsigned long i;
    enum link_state link_state;
    struct object_images *p;

	/*
	 * First for the object_images modules that have been relocated call
	 * the functions registered to be called when modules get linked.
	 */
	p = &object_images;
	do{
	    for(i = 0; i < p->nimages; i++){
		/* skip modules that that not in the relocated state */
		link_state = GET_LINK_STATE(p->images[i].module);
		if(link_state != RELOCATED)
		    continue;
		SET_LINK_STATE(p->images[i].module, REGISTERING);

		if(link_module_func_list.func == NULL)
		    continue;

		call_funcs_for_link_module(&(p->images[i].module));
		return(TRUE);
	    }
	    p = p->next_images;
	}while(p != NULL);
	return(FALSE);
}

/*
 * call_funcs_for_link_module() calls each of the functions that were register
 * to be called when modules are bound for the new module now being bound into
 * the program.
 */
static
void
call_funcs_for_link_module(
module_state *module)
{
    struct link_module_func *link_module_func;

	for(link_module_func = &link_module_func_list;
	    link_module_func != NULL && link_module_func->func != NULL;
	    link_module_func = link_module_func->next){

            DYLD_TRACE_CALLOUT_START(DYLD_TRACE_link_module_func,
		link_module_func->func);
	    /*
	     * Since functions can only be added and not removed the 
	     * link_module_func_list will be valid when we get the lock back and
	     * at worse may contain more functions than we started the loop.
	     * If this happens a function will get called twice on the same
	     * module.
	     */
	    release_lock();
	    link_module_func->func(module);
	    set_lock();
	    DYLD_TRACE_CALLOUT_END(DYLD_TRACE_link_module_func,
		link_module_func->func);
	}
}

void
register_func_for_unlink_module(
void (*func)(module_state *module))
{
    struct unlink_module_func *unlink_module_func;

	/* set lock for dyld data structures */
	set_lock();

	/*
	 * Add this func to the list of functions to be called when modules are
	 * unlinked from this program.
	 */
	if(unlink_module_func_list.func == NULL){
	    unlink_module_func_list.func = func;
	}
	else{
	    unlink_module_func = allocate(sizeof(struct unlink_module_func));
	    unlink_module_func->func = func;
	    unlink_module_func->next = unlink_module_func_list.next;
	    unlink_module_func_list.next = unlink_module_func;
	}
	/* TODO: call these unlink_module functions at some point */

	/* release lock for dyld data structures */
	release_lock();
}

void
register_func_for_replace_module(
void (*func)(module_state *oldmodule, module_state *newmodule))
{
    struct replace_module_func *replace_module_func;

	/* set lock for dyld data structures */
	set_lock();

	/*
	 * Add this func to the list of functions to be called when modules are
	 * replaced in this program.
	 */
	if(replace_module_func_list.func == NULL){
	    replace_module_func_list.func = func;
	}
	else{
	    replace_module_func = allocate(sizeof(struct replace_module_func));
	    replace_module_func->func = func;
	    replace_module_func->next = replace_module_func_list.next;
	    replace_module_func_list.next = replace_module_func;
	}

	/* TODO: call these replace_module functions at some point */

	/* release lock for dyld data structures */
	release_lock();
}
