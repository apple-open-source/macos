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
#import <stdlib.h>
#import <mach/mach.h>
#import "stuff/openstep_mach.h"
#import "mach-o/loader.h"
#import "mach-o/dyld_gdb.h"
#import "stuff/bool.h"

#import "inline_strcmp.h"
#import "images.h"
#import "symbols.h"
#import "errors.h"
#import "reloc.h"
#import "lock.h"
#import "register_funcs.h"
#import "mod_init_funcs.h"
#import "dyld_init.h"
#import "debug.h"
#import "allocate.h"
#import "trace.h"

static unsigned long _dyld_image_count(
    void);

static struct mach_header * _dyld_get_image_header(
    unsigned long image_index);

static unsigned long _dyld_get_image_vmaddr_slide(
    unsigned long image_index);

static char * _dyld_get_image_name(
    unsigned long image_index);

static void _dyld_lookup_and_bind(
    char *symbol_name,
    unsigned long *address,
    module_state **module);

static void _dyld_lookup_and_bind_with_hint(
    char *symbol_name,
    char *library_name_hint,
    unsigned long *address,
    module_state **module);

static void _dyld_lookup_and_bind_objc(
    char *symbol_name,
    unsigned long *address,
    module_state **module);

static void _dyld_lookup_and_bind_fully(
    char *symbol_name,
    unsigned long *address,
    module_state **module);

static void _dyld_install_handlers(
    void (*undefined_handler)(
	const char *symbol_name),
    module_state * (*multiple_handler)(
	struct nlist *symbol,
	module_state *old_module,
	module_state *new_module),
    void (*linkEdit_handler)(
	enum link_edit_error_class error_class,
	int error_number,
	const char *file_name,
	const char *error_string));

static void _dyld_link_edit_error(
    enum link_edit_error_class *error_class,
    int *error_number,
    const char **file_name,
    const char **error_string);

static module_state * _dyld_link_module(
    char *object_addr,
    unsigned long object_size,
    char *moduleName,
    unsigned long options);
/* TODO: figure out how to share these with <mach-o/dyld.h> */
#define LINK_OPTION_BINDNOW 0x1
#define LINK_OPTION_PRIVATE 0x2
#define LINK_OPTION_RETURN_ON_ERROR 0x4

static enum bool _dyld_unlink_module(
    module_state *module,
    unsigned long options);
#define UNLINK_OPTION_KEEP_MEMORY_MAPPED	0x1
#define UNLINK_OPTION_RESET_LAZY_REFERENCES	0x2

static void _dyld_register_func_for_add_image(
    void (*func)(struct mach_header *mh, unsigned long vmaddr_slide));

static void _dyld_register_func_for_remove_image(
    void (*func)(struct mach_header *mh, unsigned long vmaddr_slide));

static void _dyld_register_func_for_link_module(
    void (*func)(module_state *module));

static void _dyld_register_func_for_unlink_module(
    void (*func)(module_state *module));

static void _dyld_register_func_for_replace_module(
    void (*func)(module_state *oldmodule, module_state *newmodule));

static void _dyld_get_objc_module_sect_for_module(
    module_state *module,
    unsigned long *objc_module,
    unsigned long *size);

static void _dyld_bind_objc_module(
    unsigned long objc_module);

static enum bool _dyld_bind_fully_image_containing_address(
    unsigned long address);

static enum bool _dyld_image_containing_address(
    unsigned long address);

static void _dyld_moninit(
    void (*monaddition)(char *lowpc, char *highpc));

static void _dyld_fork_prepare(
    void);

static void _dyld_fork_parent(
    void);

static void _dyld_fork_child(
    void);

static void _dyld_fork_child_final(
    void);

static void _dyld_fork_mach_init(
    void);

static void _dyld_make_delayed_module_initializer_calls(
    void);

static char * _dyld_NSNameOfSymbol(
    struct nlist *symbol);

static unsigned long _dyld_NSAddressOfSymbol(
    struct nlist *symbol);

static module_state * _dyld_NSModuleForSymbol(
    struct nlist *symbol);

static struct nlist * _dyld_NSLookupAndBindSymbol(
    char *symbol_name);

static struct nlist * _dyld_NSLookupAndBindSymbolWithHint(
    char *symbol_name,
    char *libraryNameHint);

static struct nlist * _dyld_NSLookupSymbolInModule(
    module_state *module,
    char *symbol_name);

static struct nlist * _dyld_NSLookupSymbolInImage(
    struct mach_header *mh,
    char *symbol_name,
    unsigned long options);
/* TODO: figure out how to share these with <mach-o/dyld.h> */
#define LOOKUP_OPTION_BIND            0x0
#define LOOKUP_OPTION_BIND_NOW        0x1
#define LOOKUP_OPTION_BIND_FULLY      0x2
#define LOOKUP_OPTION_RETURN_ON_ERROR 0x4

static enum bool _dyld_NSMakePrivateModulePublic(
    module_state *module);

static enum bool _dyld_NSIsSymbolNameDefined(
    char *symbol_name);

static enum bool _dyld_NSIsSymbolNameDefinedWithHint(
    char *symbol_name,
    char *libraryNameHint);

static enum bool _dyld_NSIsSymbolNameDefinedInImage(
    struct mach_header *mh,
    char *symbol_name);

static char * _dyld_NSNameOfModule(
    module_state *module);

static char * _dyld_NSLibraryNameForModule(
    module_state *module);

static enum bool _dyld_NSAddLibrary(
    char *dylib_name);

static enum bool _dyld_NSAddLibraryWithSearching(
    char *dylib_name);

static struct mach_header * _dyld_NSAddImage(
    char *dylib_name,
    unsigned long options);
/* TODO: figure out how to share these with <mach-o/dyld.h> */
#define ADDIMAGE_OPTION_NONE                  		0x0
#define ADDIMAGE_OPTION_RETURN_ON_ERROR       		0x1
#define ADDIMAGE_OPTION_WITH_SEARCHING        		0x2
#define ADDIMAGE_OPTION_RETURN_ONLY_IF_LOADED 		0x4
#define ADDIMAGE_OPTION_MATCH_FILENAME_BY_INSTALLNAME	0x8

static int _dyld_NSGetExecutablePath(
    char *buf,
    unsigned long *bufsize);

static enum bool _dyld_launched_prebound(
    void);

struct dyld_func {
    char *funcname;
    void (*address)(void);
};
static
struct dyld_func dyld_funcs[] = {
    {"__dyld_image_count",	(void (*)(void))_dyld_image_count },
    {"__dyld_get_image_header",	(void (*)(void))_dyld_get_image_header },
    {"__dyld_get_image_vmaddr_slide",
				(void (*)(void))_dyld_get_image_vmaddr_slide },
    {"__dyld_get_image_name",	(void (*)(void))_dyld_get_image_name },
    {"__dyld_lookup_and_bind",	(void (*)(void))_dyld_lookup_and_bind },
    {"__dyld_lookup_and_bind_with_hint",
			    (void (*)(void))_dyld_lookup_and_bind_with_hint },
    {"__dyld_lookup_and_bind_objc",
				(void (*)(void))_dyld_lookup_and_bind_objc },
    {"__dyld_lookup_and_bind_fully",
				(void (*)(void))_dyld_lookup_and_bind_fully },
    {"__dyld_install_handlers",	(void (*)(void))_dyld_install_handlers },
    {"__dyld_link_edit_error",	(void (*)(void))_dyld_link_edit_error },
    {"__dyld_link_module",	(void (*)(void))_dyld_link_module },
    {"__dyld_unlink_module",	(void (*)(void))_dyld_unlink_module },
    {"__dyld_register_func_for_add_image",
	(void (*)(void))_dyld_register_func_for_add_image },
    {"__dyld_register_func_for_remove_image",
	(void (*)(void))_dyld_register_func_for_remove_image },
    {"__dyld_register_func_for_link_module",
	(void (*)(void))_dyld_register_func_for_link_module },
    {"__dyld_register_func_for_unlink_module",
	(void (*)(void))_dyld_register_func_for_unlink_module },
    {"__dyld_register_func_for_replace_module",
	(void (*)(void))_dyld_register_func_for_replace_module },
    {"__dyld_get_objc_module_sect_for_module",
	(void (*)(void))_dyld_get_objc_module_sect_for_module },
    {"__dyld_bind_objc_module",	(void (*)(void))_dyld_bind_objc_module },
    {"__dyld_bind_fully_image_containing_address",
	(void (*)(void))_dyld_bind_fully_image_containing_address },
    {"__dyld_image_containing_address",
	(void (*)(void))_dyld_image_containing_address },
    {"__dyld_moninit",		(void (*)(void))_dyld_moninit },
    {"__dyld_fork_prepare",	(void (*)(void))_dyld_fork_prepare },
    {"__dyld_fork_parent",	(void (*)(void))_dyld_fork_parent },
    {"__dyld_fork_child",	(void (*)(void))_dyld_fork_child },
    {"__dyld_fork_child_final",	(void (*)(void))_dyld_fork_child_final },
    {"__dyld_fork_mach_init",	(void (*)(void))_dyld_fork_mach_init },
    {"__dyld_make_delayed_module_initializer_calls",
	(void (*)(void))_dyld_make_delayed_module_initializer_calls },
    {"__dyld_NSNameOfSymbol",	(void (*)(void))_dyld_NSNameOfSymbol },
    {"__dyld_NSAddressOfSymbol",(void (*)(void))_dyld_NSAddressOfSymbol },
    {"__dyld_NSModuleForSymbol",(void (*)(void))_dyld_NSModuleForSymbol },
    {"__dyld_NSLookupAndBindSymbol",
	(void (*)(void))_dyld_NSLookupAndBindSymbol},
    {"__dyld_NSLookupAndBindSymbolWithHint",
	(void (*)(void))_dyld_NSLookupAndBindSymbolWithHint},
    {"__dyld_NSLookupSymbolInModule",
	(void (*)(void))_dyld_NSLookupSymbolInModule},
    {"__dyld_NSLookupSymbolInImage",
	(void (*)(void))_dyld_NSLookupSymbolInImage},
    {"__dyld_NSMakePrivateModulePublic",
	(void (*)(void))_dyld_NSMakePrivateModulePublic},
    {"__dyld_NSIsSymbolNameDefined",
	(void (*)(void))_dyld_NSIsSymbolNameDefined},
    {"__dyld_NSIsSymbolNameDefinedWithHint",
	(void (*)(void))_dyld_NSIsSymbolNameDefinedWithHint},
    {"__dyld_NSIsSymbolNameDefinedInImage",
	(void (*)(void))_dyld_NSIsSymbolNameDefinedInImage},
    {"__dyld_NSNameOfModule",	(void (*)(void))_dyld_NSNameOfModule },
    {"__dyld_NSLibraryNameForModule",
			(void (*)(void))_dyld_NSLibraryNameForModule },
    {"__dyld_NSAddLibrary",	(void (*)(void))_dyld_NSAddLibrary },
    {"__dyld_NSAddLibraryWithSearching",
	(void (*)(void))_dyld_NSAddLibraryWithSearching },
    {"__dyld_NSAddImage", (void (*)(void))_dyld_NSAddImage },
    {"__dyld__NSGetExecutablePath", (void (*)(void))_dyld_NSGetExecutablePath },
    {"__dyld_launched_prebound",(void (*)(void))_dyld_launched_prebound },
    {"__dyld_call_module_initializers_for_dylib",
	(void (*)(void))_dyld_call_module_initializers_for_dylib },
    {"__dyld_mod_term_funcs", (void (*)(void))_dyld_mod_term_funcs },
    {NULL, 0}
};

static enum bool saved_dyld_mem_protect;

/*
 * _dyld_func_lookup() is passed a name of a dyld function and returns the
 * address of the function indirectly.  It returns TRUE if the function is found
 * and FALSE otherwise.
 */
enum bool
_dyld_func_lookup(
char *dyld_func_name,
void (**address)(void))
/* unsigned long *address) */
{
    unsigned long i;

	/* *address = 0; */
	*address = (void (*)(void))0;
	for(i = 0; dyld_funcs[i].funcname != NULL; i++){
	    if(strcmp(dyld_funcs[i].funcname, dyld_func_name) == 0){
		*address = dyld_funcs[i].address;
		return(TRUE);
	    }
	}
	return(FALSE);
}

/*
 * _dyld_image_count() returns the current number of images mapped in by the
 * dynamic link editor.
 */
static
unsigned long
_dyld_image_count(
void)
{
    unsigned nimages, i;
    struct object_images *p;
    struct library_images *q;
    enum link_state link_state;

	/* set lock for dyld data structures */
	set_lock();

	nimages = 0;
	for(p = &object_images; p != NULL; p = p->next_images){
	    for(i = 0; i < p->nimages; i++){
		link_state = GET_LINK_STATE(p->images[i].module);
		if(link_state == UNUSED)
		    continue;
		nimages++;
	    }
	}
	for(q = &library_images; q != NULL; q = q->next_images){
	    nimages += q->nimages;
	}

	/* release lock for dyld data structures */
	release_lock();

	return(nimages);
}

/*
 * _dyld_get_image_header() returns the mach header of the image indexed by
 * image_index.  If image_index is out of range NULL is returned.
 */
static
struct mach_header *
_dyld_get_image_header(
unsigned long image_index)
{
    unsigned nimages, i;
    struct object_images *p;
    struct library_images *q;
    enum link_state link_state;

	/* set lock for dyld data structures */
	set_lock();
	nimages = 0;
	for(p = &object_images; p != NULL; p = p->next_images){
	    for(i = 0; i < p->nimages; i++){
		link_state = GET_LINK_STATE(p->images[i].module);
		if(link_state == UNUSED)
		    continue;
		if(image_index == nimages){
		    /* release lock for dyld data structures */
		    release_lock();
		    return(p->images[i].image.mh);
		}
		nimages++;
	    }
	}
	for(q = &library_images; q != NULL; q = q->next_images){
	    if(image_index >= nimages && image_index < nimages + q->nimages){
		/* release lock for dyld data structures */
		release_lock();
		return(q->images[image_index - nimages].image.mh);
	    }
	    nimages += q->nimages;
	}
	/* release lock for dyld data structures */
	release_lock();
	return(NULL);
}

/*
 * _dyld_get_image_vmaddr_slide() returns the virtural memory address slide
 * amount of the image indexed by image_index.  If image_index is out of range
 * 0 is returned.
 */
static
unsigned long
_dyld_get_image_vmaddr_slide(
unsigned long image_index)
{
    unsigned nimages, i;
    struct object_images *p;
    struct library_images *q;
    enum link_state link_state;

	/* set lock for dyld data structures */
	set_lock();
	nimages = 0;
	for(p = &object_images; p != NULL; p = p->next_images){
	    for(i = 0; i < p->nimages; i++){
		link_state = GET_LINK_STATE(p->images[i].module);
		if(link_state == UNUSED)
		    continue;
		if(image_index == nimages){
		    /* release lock for dyld data structures */
		    release_lock();
		    return(p->images[i].image.vmaddr_slide);
		}
		nimages++;
	    }
	}
	for(q = &library_images; q != NULL; q = q->next_images){
	    if(image_index >= nimages && image_index < nimages + q->nimages){
		/* release lock for dyld data structures */
		release_lock();
		return(q->images[image_index - nimages].image.vmaddr_slide);
	    }
	    nimages += q->nimages;
	}
	/* release lock for dyld data structures */
	release_lock();
	return(0);
}

/*
 * _dyld_get_image_name() returns the name of the image indexed by image_index.
 * If image_index is out of range NULL is returned.
 */
static
char *
_dyld_get_image_name(
unsigned long image_index)
{
    unsigned nimages, i;
    struct object_images *p;
    struct library_images *q;
    enum link_state link_state;

	/* set lock for dyld data structures */
	set_lock();
	nimages = 0;
	for(p = &object_images; p != NULL; p = p->next_images){
	    for(i = 0; i < p->nimages; i++){
		link_state = GET_LINK_STATE(p->images[i].module);
		if(link_state == UNUSED)
		    continue;
		if(image_index == nimages){
		    /* release lock for dyld data structures */
		    release_lock();
		    return(p->images[i].image.name);
		}
		nimages++;
	    }
	}
	for(q = &library_images; q != NULL; q = q->next_images){
	    if(image_index >= nimages && image_index < nimages + q->nimages){
		/* release lock for dyld data structures */
		release_lock();
		return(q->images[image_index - nimages].image.name);
	    }
	    nimages += q->nimages;
	}
	/* release lock for dyld data structures */
	release_lock();
	return(NULL);
}

/*
 * _dyld_lookup_and_bind() looks up the symbol name and binds it into the 
 * program.  It indirectly returns the address and and a pointer to the
 * module that defined the symbol if the pointers are not NULL.
 */
static
void
_dyld_lookup_and_bind(
char *symbol_name,
unsigned long *address,
module_state **module)
{
	/*
	 * The locking is done inside bind_symbol_by_name() because it can cause
	 * the user's undefined symbol handler to be called and the lock must
	 * be released before it is called.
	 */
	bind_symbol_by_name(symbol_name, address, module, NULL, TRUE);
}

/*
 * _dyld_lookup_and_bind_with_hint() is like _dyld_lookup_and_bind() except
 * it takes a library_name_hint.  A lookup is first attempted using the
 * library_name_hint and if that fails then a bind_symbol_by_name() is done.
 * The library_name_hint and strstr(3) is used to match up against the library
 * if the symbol is already bound in from that library then the hint worked.
 */
static
void
_dyld_lookup_and_bind_with_hint(
char *symbol_name,
char *library_name_hint,
unsigned long *address,
module_state **module)
{
    struct nlist *defined_symbol;
    module_state *defined_module;
    struct image *defined_image;
    struct library_image *defined_library_image;
    unsigned long value;
    enum link_state link_state;
        
	/* set lock for dyld data structures */
	set_lock();
	DYLD_TRACE_LIBFUNC_NAMED_START(DYLD_TRACE_lookup_and_bind_with_hint,
	    symbol_name);

	lookup_symbol_in_hinted_library(symbol_name, library_name_hint,
	    &defined_symbol, &defined_module, &defined_image,
	    &defined_library_image);
	if(defined_symbol != NULL){
	    link_state = GET_LINK_STATE(*defined_module);
	    if(link_state == LINKED || link_state == FULLY_LINKED){
		value = defined_symbol->n_value;
		if((defined_symbol->n_type & N_TYPE) != N_ABS)
		    value += defined_image->vmaddr_slide;
		if(address != NULL)
		    *address = value;
		if(module != NULL)
		    *module = defined_module;
                DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_lookup_and_bind_with_hint);
		/* release lock for dyld data structures */
		release_lock();

		return;
	    }
	}

	/* release lock for dyld data structures */
	release_lock();

	/*
	 * The locking is done inside bind_symbol_by_name() because it can cause
	 * the user's undefined symbol handler to be called and the lock must
	 * be released before it is called.
	 */
	bind_symbol_by_name(symbol_name, address, module, NULL, TRUE);
	
        DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_lookup_and_bind_with_hint);
}

/*
 * _dyld_lookup_and_bind_objc() is the same as _dyld_lookup_and_bind() but
 * does not update the symbol pointers if the symbol is in a bound module.
 * The reason for this is that an objc symbol like ".objc_class_name_Object"
 * is never used by a symbol pointer.  Since this is done alot by the objc
 * runtime and updating symbol pointers is not cheep it should not be done.
 */
static
void
_dyld_lookup_and_bind_objc(
char *symbol_name,
unsigned long *address,
module_state **module)
{
	/*
	 * The locking is done inside bind_symbol_by_name() because it can cause
	 * the user's undefined symbol handler to be called and the lock must
	 * be released before it is called.
	 */
	bind_symbol_by_name(symbol_name, address, module, NULL, FALSE);
}

/*
 * _dyld_lookup_and_bind_fully() looks up the symbol name and binds the module
 * that defineds it fully into the program.  It indirectly returns the address
 * and and a pointer to the module that defined the symbol if the pointers are
 * not NULL.
 */
static
void
_dyld_lookup_and_bind_fully(
char *symbol_name,
unsigned long *address,
module_state **module)
{
    struct nlist *defined_symbol;
    module_state *defined_module;
    struct image *defined_image;
    struct library_image *defined_library_image;
    enum link_state link_state;
    struct object_image *object_image;
    unsigned long i;
    struct object_images *p;

        if(dyld_trace)
            set_lock();
	DYLD_TRACE_LIBFUNC_NAMED_START(DYLD_TRACE_lookup_and_bind_fully,
	    symbol_name);
        if(dyld_trace)
            release_lock();
	
	/*
	 * The locking is done inside bind_symbol_by_name() because it can cause
	 * the user's undefined symbol handler to be called and the lock must
	 * be released before it is called.
	 */
	bind_symbol_by_name(symbol_name, address, module, NULL, TRUE);

	/* set lock for dyld data structures */
	set_lock();

	lookup_symbol(symbol_name, NULL, NULL, FALSE, &defined_symbol,
		      &defined_module, &defined_image, &defined_library_image,
		      NULL);
	link_state = GET_LINK_STATE(*defined_module);
	if(link_state == FULLY_LINKED){
	    /* release lock for dyld data structures */
	    release_lock();
	    return;
	}
	/*
	 * It should never happen that the symbol was not found but since we
	 * do release the lock and take it out again in here it might be
	 * possible.  If the user unloads that module before here do nothing.
	 */
	if(defined_symbol != NULL){
	    if(defined_library_image == NULL){
		object_image = NULL;
		for(p = &object_images;
		    p != NULL && object_image == NULL;
		    p = p->next_images){
		    for(i = 0; i < p->nimages && object_image == NULL; i++){
			link_state = GET_LINK_STATE(p->images[i].module);
			if(link_state == UNUSED)
			    continue;
			if(defined_image == &(p->images[i].image))
			    object_image = p->images + i;
		    }
		}
		if(object_image == NULL){
                    DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_lookup_and_bind_fully);
		    /* release lock for dyld data structures */
		    release_lock();
		    return;
		}
		link_object_module(object_image, TRUE, FALSE);
	    }
	    else
		link_library_module(defined_library_image, defined_image,
		    defined_module, TRUE, FALSE, FALSE);
	    /* the lock gets released in link_in_need_modules */
	    link_in_need_modules(TRUE, TRUE);
	}
	DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_lookup_and_bind_fully);
}

static
void
_dyld_install_handlers(
void (*undefined_handler)(
    const char *symbol_name),
module_state * (*multiple_handler)(
    struct nlist *symbol,
    module_state *old_module,
    module_state *new_module),
void (*linkEdit_handler)(
    enum link_edit_error_class error_class,
    int error_number,
    const char *file_name,
    const char *error_string))
{
	/* set lock for dyld data structures */
	set_lock();

	/* set the pointers to the handlers */
	user_undefined_handler = undefined_handler;
	user_multiple_handler = multiple_handler;
	user_linkEdit_handler = linkEdit_handler;

	/* release lock for dyld data structures */
	release_lock();

}

static void _dyld_link_edit_error(
enum link_edit_error_class *error_class,
int *error_number,
const char **file_name,
const char **ptr_error_string)
{
	/* set lock for dyld data structures */
	set_lock();

	*error_class = NSLinkEditError_errorClass;
	*error_number = NSLinkEditError_errorNumber;
	*file_name = NSLinkEditError_fileName;
	*ptr_error_string = error_string;

	/* release lock for dyld data structures */
	release_lock();
}

static module_state *
_dyld_link_module(
char *object_addr,
unsigned long object_size,
char *moduleName,
unsigned long options)
{
    struct object_image *object_image;
    enum bool bind_now;

	/* set lock for dyld data structures */
	set_lock();
	DYLD_TRACE_LIBFUNC_NAMED_START(DYLD_TRACE_link_module, moduleName);

	/*
	 * If the LINK_OPTION_RETURN_ON_ERROR is specified set the global
	 * return_on_error to TRUE.  This will effect all of our callers and
	 * allow all changes to be backed out if an error occurs.  This gets
	 * set back to FALSE in release_lock().
	 */
	if(options & LINK_OPTION_RETURN_ON_ERROR){
	    clear_module_states_saved();
	    return_on_error = TRUE;
	}
	else
	    return_on_error = FALSE;

	object_image = map_bundle_image(moduleName, object_addr, object_size);
	if(object_image == NULL){
            DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_link_module);
	    /* release lock for dyld data structures */
	    release_lock();
	    return(NULL);
	}

	if((object_image->image.mh->flags & MH_BINDATLOAD) != 0 ||
	    (options & LINK_OPTION_BINDNOW) == LINK_OPTION_BINDNOW)
	    bind_now = TRUE;
	else
	    bind_now = FALSE;

	if((options & LINK_OPTION_PRIVATE) == LINK_OPTION_PRIVATE)
	    object_image->image.private = TRUE;
	else
	    object_image->image.private = FALSE;

	/*
	 * Load the dependent libraries.
	 */
	if(load_dependent_libraries() == FALSE &&
	   return_on_error == TRUE){
	    /*
	     * Since at this point no symbols have been bound we can just fake
	     * this bundle as being private and call unload_bundle_image() to
	     * do all the unloading.
	     */
	    object_image->image.private = TRUE;
	    unload_bundle_image(object_image, FALSE, FALSE);
            DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_link_module);
	    /* release lock for dyld data structures */
	    release_lock();
	    return(NULL);
	}

	/*
	 * If we are not forcing flatname space and this object image is a
	 * two-level namespace image and has hints then set the
	 * image_can_use_hints bit for this object image.
	 */
	if(force_flat_namespace == FALSE){
	    if((object_image->image.mh->flags & MH_TWOLEVEL) == MH_TWOLEVEL &&
	       (object_image->image.hints_cmd != NULL))
		object_image->image.image_can_use_hints = TRUE;
	     else
		object_image->image.image_can_use_hints = FALSE;
	}

	/*
	 * Link in the object modules symbols, checking for multiply defined
	 * symbols if not private and add the undefined symbols to the undefined
	 * list.  If return_on_error is set link_object_module returning FALSE
	 * it indicates that there were multiply defined symbols (and then no
	 * undefined symbols were then added to the undefined list, but there
	 * could have been coalesced symbols added to the being linked list) so
	 * the clean up here is to clear the being linked list of symbols that
	 * were added when return_on_error was set and unmap the object_image
	 * and return NULL.
	 */
	if(link_object_module(object_image, bind_now, FALSE) == FALSE &&
	   return_on_error == TRUE){
	    clear_being_linked_list(TRUE);
	    /*
	     * Since at this point no symbols have been bound we can just fake
	     * this bundle as being private and call unload_bundle_image() to
	     * do all the unloading.
	     */
	    object_image->image.private = TRUE;
	    unload_bundle_image(object_image, FALSE, FALSE);
    	    DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_link_module);
	    /* release lock for dyld data structures */
	    release_lock();
	    return(NULL);
	}

	/*
	 * Now link in the needed modules for this object module.  The above
	 * call to link_object_module() has placed the needed undefined symbols
	 * on the undefined list and coalesced symbols on the being linked list.
	 * link_in_need_modules() will load the dependent libraries, resolve any
	 * needed undefined symbols and relocate any newly linked in modules.
	 * If return_on_error is set we need backout the undefineds added to
	 * the undefined list and coalesced symbols on the being linked list
	 * created by link_object_module(), unmap the object_image and return
	 * NULL.
	 *
	 * NOTE: the lock gets released with link_in_need_modules( ,TRUE)
	 */
	if(return_on_error == TRUE){
	    if(link_in_need_modules(FALSE, FALSE) == FALSE){
		/*
		 * Back out the undefined symbols and coalesced symbols added by
		 * link_object_module() after return_on_error was set.  Note
		 * NSLinkModule with LINK_OPTION_RETURN_ON_ERROR could have
		 * been called from an undefined error handler so we can not
		 * just clear the undefined list.
		 */
		clear_being_linked_list(TRUE);
		clear_undefined_list(TRUE);

		/*
		 * Since at this point any references to symbols defined in the
		 * bundle have been backed out so we can just fake this bundle
		 * as being private and call unload_bundle_image() to do the
		 * rest of the unloading.
		 */
		object_image->image.private = TRUE;
		unload_bundle_image(object_image, FALSE, FALSE);
                DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_link_module);
		/* release lock for dyld data structures */
		release_lock();
		return(NULL);
	    }
	    else{
		/* release lock for dyld data structures */
		release_lock();
	    }
	}
	else{
	    (void)link_in_need_modules(FALSE, TRUE);
	}

	/*
	 * Call the routine that gdb might have a break point on to let it
	 * know it is time to re-read the internal dyld structures as defined
	 * by <mach-o/dyld_gdb.h>
	 */
	gdb_dyld_state_changed();

	DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_link_module);
	return(&object_image->module);
}

static
enum bool
_dyld_unlink_module(
module_state *module,
unsigned long options)
{
    unsigned long i;
    struct object_images *p;
    enum bool retval;
    enum link_state link_state;
    enum bool keepMemoryMapped, reset_lazy_references;

	/* set lock for dyld data structures */
	set_lock();
	DYLD_TRACE_LIBFUNC_START(DYLD_TRACE_unlink_module);

	if(options & UNLINK_OPTION_KEEP_MEMORY_MAPPED)
    	    keepMemoryMapped = TRUE;
	else
    	    keepMemoryMapped = FALSE;

	if(options & UNLINK_OPTION_RESET_LAZY_REFERENCES)
    	    reset_lazy_references = TRUE;
	else
    	    reset_lazy_references = FALSE;
/*
printf("In _dyld_unlink_module(module = 0x%x, options = %d)\n",
module, options);
*/
	retval = FALSE;

	/*
	 * This version of dyld only supports NSUnlinkModule() for things
	 * that were NSLinkModule().  So ignore the executable image that is
	 * the first object file image (a hack but this version of unlinking
	 * is a hack).  Then look through the object images trying to find this
	 * module.  Then if found call to get it unlinked.
	 */
	if(module == &(object_images.images[0].module))
	    goto done;

	for(p = &object_images; p != NULL; p = p->next_images){
	    for(i = 0; i < p->nimages; i++){
/*
printf("p->images[%lu].module = 0x%x name = %s\n", i, p->images[i].module, p->images[i].image.name);
*/
		if(module == &(p->images[i].module)){
		    /*
		     * If the module handle is marked UNUSED it was already
		     * unlinked and another unlink is being attempted on it.
		     */
		    link_state = GET_LINK_STATE(p->images[i].module);
		    if(link_state == UNUSED)
			goto done;

		    call_module_terminator_for_object(&(p->images[i]));

		    call_funcs_for_remove_image(p->images[i].image.mh,
					    p->images[i].image.vmaddr_slide);

		    unload_bundle_image(&(p->images[i]),
					keepMemoryMapped,
				        reset_lazy_references);

		    retval = TRUE;
		    goto done;
		}
	    }
	}
done:
	/*
	 * Call the routine that gdb might have a break point on to let it
	 * know it is time to re-read the internal dyld structures as defined
	 * by <mach-o/dyld_gdb.h>
	 */
	gdb_dyld_state_changed();

	DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_unlink_module);
	/* release lock for dyld data structures */
	release_lock();

	return(retval);
}

/*
 * _dyld_register_func_for_add_image registers the specified function to be
 * called when a new image is added (a bundle or a dynamic shared library) to
 * the program.  When this function is first registered it is called for once
 * for each image that is currently part of the program.
 */
static
void
_dyld_register_func_for_add_image(
void (*func)(struct mach_header *mh, unsigned long vmaddr_slide))
{
	register_func_for_add_image(func);
}

/*
 * _dyld_register_func_for_remove_image registers the specified function to be
 * called when an image is removed (a bundle or a dynamic shared library) from
 * the program.
 */
static
void
_dyld_register_func_for_remove_image(
void (*func)(struct mach_header *mh, unsigned long vmaddr_slide))
{
	register_func_for_remove_image(func);
}

/*
 * _dyld_register_func_for_link_module registers the specified function to be
 * called when a module is bound into the program.  When this function is first
 * registered it is called for once for each module that is currently bound into
 * the program.
 */
static
void
_dyld_register_func_for_link_module(
void (*func)(module_state *module))
{
	register_func_for_link_module(func);
}

/*
 * _dyld_register_func_for_unlink_module registers the specified function to be
 * called when a module is unbound from the program.
 */
static
void
_dyld_register_func_for_unlink_module(
void (*func)(module_state *module))
{
}

/*
 * _dyld_register_func_for_replace_module registers the specified function to be
 * called when a module is to be replace with another module in the program.
 */
static
void
_dyld_register_func_for_replace_module(
void (*func)(module_state *oldmodule, module_state *newmodule))
{
}

/*
 * _dyld_get_objc_module_sect_for_module is passed a module and sets a 
 * pointer to the (__OBJC,__module) section and its size for the specified
 * module.
 */
static
void
_dyld_get_objc_module_sect_for_module(
module_state *module,
unsigned long *objc_module,
unsigned long *size)
{
    unsigned long i, j, k;
    struct object_images *p;
    struct library_images *q;
    struct load_command *lc;
    struct segment_command *sg;
    struct section *s;
    struct dylib_module *dylib_modules;
    enum link_state link_state;

	/* set lock for dyld data structures */
	set_lock();

	*objc_module = 0;
	*size = 0;
	/*
	 * Look through the object images and then the library images trying
	 * to find this module.  Then if found then set the starting address
	 * and size of the (__OBJC,__module_info) section.
	 */
	for(p = &object_images; p != NULL; p = p->next_images){
	    for(i = 0; i < p->nimages; i++){
		link_state = GET_LINK_STATE(p->images[i].module);
		if(link_state == UNUSED)
		    continue;
		if(module == &(p->images[i].module)){
		    lc = (struct load_command *)((char *)p->images[i].image.mh +
			    sizeof(struct mach_header));
		    for(j = 0; j < p->images[i].image.mh->ncmds; j++){
			switch(lc->cmd){
			case LC_SEGMENT:
			    sg = (struct segment_command *)lc;
			    s = (struct section *)
				((char *)sg + sizeof(struct segment_command));
			    for(k = 0; k < sg->nsects; k++){
				if(strcmp(s->segname, SEG_OBJC) == 0 &&
				   strcmp(s->sectname, SECT_OBJC_MODULES) == 0){
				    if(s->size != 0){
					*objc_module = s->addr +
					    p->images[i].image.vmaddr_slide;
					*size = s->size;
				    }
				    /* release lock for dyld data structures */
				    release_lock();
				    return;
				}
				s++;
			    }
			}
			lc = (struct load_command *)((char *)lc + lc->cmdsize);
		    }
		    /* release lock for dyld data structures */
		    release_lock();
		    return;
		}
	    }
	}
	for(q = &library_images; q != NULL; q = q->next_images){
	    for(i = 0; i < q->nimages; i++){
		if(module >= q->images[i].modules &&
		   module < q->images[i].modules +
			    q->images[i].image.dyst->nmodtab){
		    j = module - q->images[i].modules;
		    dylib_modules = (struct dylib_module *)
			(q->images[i].image.vmaddr_slide +
			 q->images[i].image.linkedit_segment->vmaddr +
			 q->images[i].image.dyst->modtaboff -
			 q->images[i].image.linkedit_segment->fileoff);
		    if(dylib_modules[j].objc_module_info_size != 0){
			*objc_module = dylib_modules[j].objc_module_info_addr +
				       q->images[i].image.vmaddr_slide;
			*size = dylib_modules[j].objc_module_info_size;
		    }
		    /* release lock for dyld data structures */
		    release_lock();
		    return;
		}
	    }
	}
	/* release lock for dyld data structures */
	release_lock();
}

/*
 * _dyld_bind_objc_module() is passed a pointer to something in an (__OBJC,
 * __module) section and causes the module that is associated with that address
 * to be bound.
 */
static
void
_dyld_bind_objc_module(
unsigned long objc_module)
{
    unsigned long i, j;
    struct library_images *q;
    struct dylib_module *dylib_modules;
    enum link_state link_state;

	/* set lock for dyld data structures */
	set_lock();
	DYLD_TRACE_LIBFUNC_START(DYLD_TRACE_bind_objc_module);
        
	/*
	 * Look through the library images trying to find this module.  Then if 
	 * found see if it is linked and if not force it to be linked.
	 */
	for(q = &library_images; q != NULL; q = q->next_images){
	    for(i = 0; i < q->nimages; i++){
		dylib_modules = (struct dylib_module *)
		    (q->images[i].image.vmaddr_slide +
		     q->images[i].image.linkedit_segment->vmaddr +
		     q->images[i].image.dyst->modtaboff -
		     q->images[i].image.linkedit_segment->fileoff);
		for(j = 0; j < q->images[i].image.dyst->nmodtab; j++){
		    if(objc_module >= dylib_modules[j].objc_module_info_addr +
				      q->images[i].image.vmaddr_slide &&
		       objc_module < dylib_modules[j].objc_module_info_addr +
		    		     dylib_modules[j].objc_module_info_size +
				     q->images[i].image.vmaddr_slide)
			break;
		}
		if(j == q->images[i].image.dyst->nmodtab)
		    continue;

		link_state = GET_LINK_STATE(q->images[i].modules[j]);

		/*
		 * Library modules in a linked state simply return as nothing 
		 * needs to be done.  Otherwise if the module is in any other
		 * state except unlinked (it was removed or replaced) it is an
		 * error as it can't be linked again (also return).
		 */
		if(link_state == LINKED || link_state == FULLY_LINKED){
		    release_lock();
		    return;
		}
		if(link_state != UNLINKED &&
		   link_state != PREBOUND_UNLINKED){
                    DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_bind_objc_module);
		    release_lock();
		    return;
		}

		/*
		 * If this module is in the prebound unlinked state first
		 * undo the prebinding and then set it to the unlinked
		 * state.
		 */
		if(link_state == PREBOUND_UNLINKED){
		    undo_prebinding_for_library_module(
			q->images[i].modules + j,
			&(q->images[i].image),
			q->images + i);
		    SET_LINK_STATE(q->images[i].modules[j], UNLINKED);
		}

		/*
		 * This module is not linked.  So force to be linked,
		 * resolve undefineds, relocate modules that got
		 * linked in and check and report undefined symbols.
		 */
		link_library_module(q->images + i,
				    &(q->images[i].image),
				    q->images[i].modules + j,
				    FALSE, FALSE, FALSE);
		resolve_undefineds(FALSE, FALSE);
		relocate_modules_being_linked(FALSE);
		check_and_report_undefineds();
		call_registered_funcs_for_add_images();
		call_registered_funcs_for_linked_modules();
		call_image_init_routines(FALSE);
		call_module_initializers(FALSE, FALSE);
                
		DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_bind_objc_module);
		/* release lock for dyld data structures */
		release_lock();
		return;
	    }
	}
        DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_bind_objc_module);
	/* release lock for dyld data structures */
	release_lock();
}

/*
 * _dyld_bind_fully_image_containing_address() is passed an address in an image.
 * and then fully binds that image so all symbols that image references directly
 * and indirectly are bound.  This is used when installing a signal handler or
 * dyld undefined handler so that everything the handler needs is bound and it
 * not try to bind anything when handling the signal or undefined symbol.
 * If the address is found in an image and it was successfully fully bound TRUE
 * is returned else FALSE is returned.
 */
static
enum bool
_dyld_bind_fully_image_containing_address(
unsigned long address)
{
    unsigned long i, j;
    struct object_images *p;
    struct library_images *q;
    struct load_command *lc;
    struct segment_command *sg;
    enum link_state link_state;
	

	/* set lock for dyld data structures */
	set_lock();
	DYLD_TRACE_LIBFUNC_START(
	    DYLD_TRACE_bind_fully_image_containing_address);
	/*
	 * Look through the object images and then the library images trying
	 * to find this address.  Then if found then cause the image to be
	 * fully bound.
	 */
	for(p = &object_images; p != NULL; p = p->next_images){
	    for(i = 0; i < p->nimages; i++){
		link_state = GET_LINK_STATE(p->images[i].module);
		if(link_state == UNUSED)
		    continue;
		lc = (struct load_command *)((char *)p->images[i].image.mh +
			sizeof(struct mach_header));
		for(j = 0; j < p->images[i].image.mh->ncmds; j++){
		    switch(lc->cmd){
		    case LC_SEGMENT:
			sg = (struct segment_command *)lc;
			if(address >= sg->vmaddr +
				      p->images[i].image.vmaddr_slide &&
			   address < sg->vmaddr + sg->vmsize +
				     p->images[i].image.vmaddr_slide){
			    link_state = GET_LINK_STATE(p->images[i].module);
			    if(link_state == FULLY_LINKED){
				/* release lock for dyld data structures */
				release_lock();
				DYLD_TRACE_LIBFUNC_END(
				DYLD_TRACE_bind_fully_image_containing_address);
				return(TRUE);
			    }
			    link_object_module(&(p->images[i]), TRUE, FALSE);
			    link_in_need_modules(TRUE, TRUE);
			    /* the lock gets released in link_in_need_modules */
			    DYLD_TRACE_LIBFUNC_END(
				DYLD_TRACE_bind_fully_image_containing_address);
			    return(TRUE);
			}
		    }
		    lc = (struct load_command *)((char *)lc + lc->cmdsize);
		}
	    }
	}
	for(q = &library_images; q != NULL; q = q->next_images){
	    for(i = 0; i < q->nimages; i++){
		lc = (struct load_command *)((char *)q->images[i].image.mh +
			sizeof(struct mach_header));
		for(j = 0; j < q->images[i].image.mh->ncmds; j++){
		    switch(lc->cmd){
		    case LC_SEGMENT:
			sg = (struct segment_command *)lc;
			if(address >= sg->vmaddr +
				      q->images[i].image.vmaddr_slide &&
			   address < sg->vmaddr + sg->vmsize +
				     q->images[i].image.vmaddr_slide){
			    for(j=0; j < q->images[i].image.dyst->nmodtab; j++){
				/* TODO: skip replaced modules */
				link_state =
				    GET_LINK_STATE(q->images[i].modules[j]);
				if(link_state == PREBOUND_UNLINKED){
				    undo_prebinding_for_library_module(
					q->images[i].modules + j,
					&(q->images[i].image),
					q->images + i);
				    link_state = UNLINKED;
				    SET_LINK_STATE(q->images[i].modules[j],
						   link_state);
				}
				if(link_state != FULLY_LINKED)
				    link_library_module(q->images + i,
						    &(q->images[i].image),
						    q->images[i].modules + j,
						    TRUE, FALSE, FALSE);
			    }
			    link_in_need_modules(TRUE, TRUE);
			    /* the lock gets released in link_in_need_modules */
			    DYLD_TRACE_LIBFUNC_END(
				DYLD_TRACE_bind_fully_image_containing_address);
			    return(TRUE);
			}
		    }
		    lc = (struct load_command *)((char *)lc + lc->cmdsize);
		}
	    }
	}
        
        DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_bind_fully_image_containing_address);
	/* release lock for dyld data structures */
	release_lock();
	return(FALSE);
}

/*
 * _dyld_image_containing_address() is passed an address that might be in image
 * managed by dyld.  If the address is found in an image TRUE is returned else
 * FALSE is returned.
 */
static
enum bool
_dyld_image_containing_address(
unsigned long address)
{
    unsigned long i, j;
    struct object_images *p;
    struct library_images *q;
    struct load_command *lc;
    struct segment_command *sg;
    enum bool first_object_image;
    enum link_state link_state;

	/* set lock for dyld data structures */
	set_lock();

	/*
	 * Look through the object images and then the library images trying
	 * to find this address.
	 */
	first_object_image = TRUE;
	for(p = &object_images; p != NULL; p = p->next_images){
	    for(i = 0; i < p->nimages; i++){
		link_state = GET_LINK_STATE(p->images[i].module);
		if(link_state == UNUSED)
		    continue;
		/*
		 * The first object image is the executable and it is not
		 * always contiguious in memory and can't be tested with
		 * vmaddr_size.  Also split images are not contiguious in
		 * memory and can't be tested with vmaddr_size.
		 */
		if((p->images[i].image.mh->flags & MH_SPLIT_SEGS) != 0 ||
		   first_object_image == TRUE){
		    lc = (struct load_command *)((char *)p->images[i].image.mh +
			    sizeof(struct mach_header));
		    for(j = 0; j < p->images[i].image.mh->ncmds; j++){
			switch(lc->cmd){
			case LC_SEGMENT:
			    sg = (struct segment_command *)lc;
			    if(address >= sg->vmaddr +
					  p->images[i].image.vmaddr_slide &&
			       address < sg->vmaddr + sg->vmsize +
					 p->images[i].image.vmaddr_slide){
				/* release lock for dyld data structures */
				release_lock();
				return(TRUE);
			    }
			}
			lc = (struct load_command *)((char *)lc + lc->cmdsize);
		    }
		    first_object_image = FALSE;
		}
		else{
		    if(address >= ((unsigned long)p->images[i].image.mh) &&
		       address < ((unsigned long)p->images[i].image.mh) +
				 p->images[i].image.vmaddr_size){
			/* release lock for dyld data structures */
			release_lock();
			return(TRUE);
		    }
		}
	    }
	}
	for(q = &library_images; q != NULL; q = q->next_images){
	    for(i = 0; i < q->nimages; i++){
		/*
		 * Split images are not contiguious in memory and can't be
		 * tested with vmaddr_size.
		 */
		if((q->images[i].image.mh->flags & MH_SPLIT_SEGS) != 0){
		    lc = (struct load_command *)((char *)q->images[i].image.mh +
			    sizeof(struct mach_header));
		    for(j = 0; j < q->images[i].image.mh->ncmds; j++){
			switch(lc->cmd){
			case LC_SEGMENT:
			    sg = (struct segment_command *)lc;
			    if(address >= sg->vmaddr +
					  q->images[i].image.vmaddr_slide &&
			       address < sg->vmaddr + sg->vmsize +
					 q->images[i].image.vmaddr_slide){
				/* release lock for dyld data structures */
				release_lock();
				return(TRUE);
			    }
			}
			lc = (struct load_command *)((char *)lc + lc->cmdsize);
		    }
		}
		else{
		    if(address >= ((unsigned long)q->images[i].image.mh) &&
		       address < ((unsigned long)q->images[i].image.mh) +
				 q->images[i].image.vmaddr_size){
			/* release lock for dyld data structures */
			release_lock();
			return(TRUE);
		    }
		}
	    }
	}
	/* release lock for dyld data structures */
	release_lock();
	return(FALSE);
}

/*
 * _dyld_moninit() is called from the profiling runtime routine moninit() to
 * cause the dyld loaded code to be profiled.  It is passed a pointer to the the
 * profiling runtime routine monaddtion() to be called after an image had been
 * mapped in.
 */
static
void
_dyld_moninit(
void (*monaddition)(char *lowpc, char *highpc))
{
    unsigned long i, j, k;
    struct object_images *p;
    struct library_images *q;
    struct load_command *lc;
    struct segment_command *sg;
    struct section *s;
    enum link_state link_state;

	/* set lock for dyld data structures */
	set_lock();

	dyld_monaddition = monaddition;
	if(dyld_monaddition == NULL){
	    release_lock();
	    return;
	}

	for(p = &object_images; p != NULL; p = p->next_images){
	    for(i = 0; i < p->nimages; i++){
		link_state = GET_LINK_STATE(p->images[i].module);
		if(link_state == UNUSED)
		    continue;
		/*
		 * Skip the executable which is the first object image, this
		 * will be done by moninit which calls this routine.
		 */
		if(p == &object_images && i == 0)
		    continue;
		lc = (struct load_command *)((char *)p->images[i].image.mh +
			sizeof(struct mach_header));
		for(j = 0; j < p->images[i].image.mh->ncmds; j++){
		    switch(lc->cmd){
		    case LC_SEGMENT:
			sg = (struct segment_command *)lc;
			s = (struct section *)
			    ((char *)sg + sizeof(struct segment_command));
			for(k = 0; k < sg->nsects; k++){
			    /* TODO this should be based on SOME_INSTRUCTIONS */
			    if(strcmp(s->segname, SEG_TEXT) == 0 &&
			       strcmp(s->sectname, SECT_TEXT) == 0){
				if(s->size != 0){
				    release_lock();
				    dyld_monaddition(
					(char *)(s->addr +
					    p->images[i].image.vmaddr_slide),
					(char *)(s->addr +
					    p->images[i].image.vmaddr_slide +
					    s->size));
				    set_lock();
				}
			    }
			    s++;
			}
		    }
		    lc = (struct load_command *)((char *)lc + lc->cmdsize);
		}
	    }
	}
	for(q = &library_images; q != NULL; q = q->next_images){
	    for(i = 0; i < q->nimages; i++){
		lc = (struct load_command *)((char *)q->images[i].image.mh +
			sizeof(struct mach_header));
		for(j = 0; j < q->images[i].image.mh->ncmds; j++){
		    switch(lc->cmd){
		    case LC_SEGMENT:
			sg = (struct segment_command *)lc;
			s = (struct section *)
			    ((char *)sg + sizeof(struct segment_command));
			for(k = 0; k < sg->nsects; k++){
			    /* TODO this should be based on SOME_INSTRUCTIONS */
			    if(strcmp(s->segname, SEG_TEXT) == 0 &&
			       strcmp(s->sectname, SECT_TEXT) == 0){
				if(s->size != 0){
				    release_lock();
				    dyld_monaddition(
					(char *)(s->addr +
					    q->images[i].image.vmaddr_slide),
					(char *)(s->addr +
					    q->images[i].image.vmaddr_slide +
					    s->size));
				    set_lock();
				}
			    }
			    s++;
			}
		    }
		    lc = (struct load_command *)((char *)lc + lc->cmdsize);
		}
	    }
	}
	/* release lock for dyld data structures */
	release_lock();
}

/*
 * _dyld_fork_prepare() is called in the fork(2) system call stub before the
 * system call is made.  This takes out the dyld lock and returns with the lock
 * taken.  This is done so after the fork on the child side the other threads
 * will block so the child side can be sure to call _dyld_fork_child()
 * before any other child thread attempts to use dyld.
 */
static
void
_dyld_fork_prepare(
void)
{
	/* set lock for dyld data structures */
	set_lock();
}

/*
 * _dyld_fork_parent() is called on the parent's side of the fork to release the
 * dyld lock the _dyld_fork_prepare() took out.  There is no api in
 * libdyld to get at this as it can't be dynamicly bound on the parent side
 * of the fork call.  So it uses _dyld_func_lookup("__dyld_fork_parent",
 * &address) and calls through the returned address.
 */
static
void
_dyld_fork_parent(
void)
{
	/* release lock for dyld data structures */
	release_lock();
}

/* 
 * _dyld_fork_child() is the first thing called on the child's side of a fork
 * to let dyld know the task has changed.  This recalls mach_task_self() inside
 * dyld so that mach_task_self_ is reset to the new task id.  There is no api
 * in libdyld to get at this as it can't be dynamicly bound on the child side
 * of the fork call.  So it uses _dyld_func_lookup("__dyld_fork_child",
 * &address) and calls through the returned address.  After this is called in
 * the fork stub a bunch of other things are called that mess with the ports.
 * Which is why _dyld_fork_child_final() is called at the end of the fork
 * stub and why the dyld_mem_protect feature is disabled until then.
 */
static
void
_dyld_fork_child(
void)
{
#ifdef __MACH30__
#undef mach_task_self
    extern mach_port_t mach_task_self(void);

        mach_task_self_ = mach_task_self();
#else
#undef task_self
    extern int task_self(void);

        task_self_ = task_self();
#endif
	cached_thread = MACH_PORT_NULL;
	cached_stack = 0;

	saved_dyld_mem_protect = dyld_mem_protect;
	dyld_mem_protect = FALSE;

	/* clear the global lock in case another thread in the parent got it
	   while we were fork'ing */
	clear_lock(global_lock);

	/* release lock for dyld data structures */
	release_lock();
}

/* 
 * _dyld_fork_child_final() is called on the child's side of fork just before
 * returning to the user.  This is to let dyld know the fork is done.  This
 * recalls task_init() one more time inside dyld so that mach_task_self_ is
 * reset to the new task id.  There is no api in libdyld to get at this as
 * it can't be dynamicly bound on the child side of the fork call.  So it uses
 * _dyld_func_lookup("__dyld_fork_child_final", &address) and calls through the 
 * returned address.
 */
static
void
_dyld_fork_child_final(
void)
{
#ifdef __MACH30__
#undef mach_task_self
    extern mach_port_t mach_task_self(void);

#else
#undef task_self
    extern int task_self(void);
#endif

	/* set lock for dyld data structures */
	set_lock();

#ifdef __MACH30__
        mach_task_self_ = mach_task_self();
#else
        task_self_ = task_self();
#endif

	dyld_mem_protect = saved_dyld_mem_protect;

	/* release lock for dyld data structures */
	release_lock();
}

/* 
 * THIS IS OBSOLETE (should be using the above: _dyld_fork_prepare(),
 * _dyld_fork_parent(), _dyld_fork_child(), _dyld_fork_child_final())
 *
 * _dyld_fork_mach_init() is called on the child's side of fork to let dyld
 * know the task has changed.  This recalls mach_init() inside dyld so that
 * things like mach_task_self_ is reset to the new task id.  There is no api in
 * libdyld to get at this as it can't be dynamicly bound on the child side
 * of the fork call.  So it uses _dyld_func_lookup("__dyld_fork_mach_init",
 * &address) and calls through the returned address.
 */
static
void
_dyld_fork_mach_init(
void)
{
    extern void mach_init(void);

	/* set lock for dyld data structures */
	set_lock();

	mach_init();

	/* release lock for dyld data structures */
	release_lock();
}

/*
 * _dyld_make_delayed_module_initializer_calls calls the module initialization
 * routines for modules that have been linked in but the calls have been delayed
 * waiting for the runtime startoff initialization.  There is no need for api in
 * libdyld to get at as it is only used from the runtime startoff.  So it uses
 * _dyld_func_lookup("__dyld_make_delayed_module_initializer_calls", &address)
 * and calls through the returned address.
 */
static
void
_dyld_make_delayed_module_initializer_calls(
void)
{
	/* set lock for dyld data structures */
	set_lock();
	DYLD_TRACE_LIBFUNC_START(
	    DYLD_TRACE_make_delayed_module_initializer_calls);
        
	call_image_init_routines(TRUE);
	call_module_initializers(TRUE, FALSE);

	DYLD_TRACE_LIBFUNC_END(
	    DYLD_TRACE_make_delayed_module_initializer_calls);
	/* release lock for dyld data structures */
	release_lock();
}

/*
 * _dyld_NSNameOfSymbol() is the dyld side of NSNameOfSymbol().  This is passed
 * an NSSymbol which is implemented as a pointer to an nlist struct in an
 * image.  It then returns the name of the symbol if it is a valid global symbol
 * in a currently linked image.  If not it returns NULL.  This call is allowed
 * to be made from the user's multiply defined error handler.
 */
static
char *
_dyld_NSNameOfSymbol(
struct nlist *symbol)
{
    enum bool lock_set;
    module_state *defined_module;
    struct image *defined_image;
    struct library_image *defined_library_image;
    char *symbol_name;

	/*
	 * Set the lock for dyld data structures or verifiy this is being
	 * called from the user's multiply defined handler which this this
	 * routine is allowed to be called from.
	 */
	lock_set = set_lock_or_in_multiply_defined_handler();
	DYLD_TRACE_LIBFUNC_START(DYLD_TRACE_NSNameOfSymbol);	

	symbol_name = NULL;
	if(validate_NSSymbol(symbol, &defined_module,
		&defined_image, &defined_library_image) == TRUE){
	    symbol_name = (char *)
		(defined_image->vmaddr_slide +
		 defined_image->linkedit_segment->vmaddr +
		 defined_image->st->stroff -
		 defined_image->linkedit_segment->fileoff) +
		symbol->n_un.n_strx;
	}

	DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_NSNameOfSymbol);
	/* release lock for dyld data structures if we set it */
	if(lock_set == TRUE)
	    release_lock();

	return(symbol_name);
}

/*
 * _dyld_NSAddressOfSymbol() is the dyld side of NSAddressOfSymbol().  This is
 * passed an NSSymbol which is implemented as a pointer to an nlist struct in an
 * image.  It then returns the address of the symbol if it is a valid global 
 * symbol in a currently linked image.  If not it returns NULL.
 */
static
unsigned long
_dyld_NSAddressOfSymbol(
struct nlist *symbol)
{
    module_state *defined_module;
    struct image *defined_image;
    struct library_image *defined_library_image;
    unsigned long value;

	/* set lock for dyld data structures */
	set_lock();
	DYLD_TRACE_LIBFUNC_START(DYLD_TRACE_NSAddressOfSymbol);
        
	value = 0;
	if(validate_NSSymbol(symbol, &defined_module,
		&defined_image, &defined_library_image) == TRUE){
	    value = symbol->n_value;
	    if((symbol->n_type & N_TYPE) != N_ABS)
		value += defined_image->vmaddr_slide;
	}

	DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_NSAddressOfSymbol);
	/* release lock for dyld data structures */
	release_lock();

	return(value);
}

/*
 * _dyld_NSModuleForSymbol() is the dyld side of NSModuleForSymbol().  This is
 * passed an NSSymbol which is implemented as a pointer to an nlist struct in an
 * image.  It then returns the NSModule (implemented as a pointer to a module
 * state) for the symbol if it is a valid global symbol in a currently linked
 * image.  If not it returns NULL.
 */
static
module_state *
_dyld_NSModuleForSymbol(
struct nlist *symbol)
{
    module_state *defined_module;
    struct image *defined_image;
    struct library_image *defined_library_image;

	/* set lock for dyld data structures */
	set_lock();
	DYLD_TRACE_LIBFUNC_START(DYLD_TRACE_NSModuleForSymbol);
        
	defined_module = NULL;
	(void)validate_NSSymbol(symbol, &defined_module,
		&defined_image, &defined_library_image);

	DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_NSModuleForSymbol);
	/* release lock for dyld data structures */
	release_lock();

	return(defined_module);
}

/*
 * _dyld_NSLookupAndBindSymbol() is the dyld side of NSLookupAndBindSymbol().
 * This is passed a symbol name and returns an NSSymbol which is implemented as
 * a pointer to an nlist struct in an image.
 */
static
struct nlist *
_dyld_NSLookupAndBindSymbol(
char *symbol_name)
{
    struct nlist *symbol;

	/*
	 * The locking is done inside bind_symbol_by_name() because it can cause
	 * the user's undefined symbol handler to be called and the lock must
	 * be released before it is called.
	 */
	bind_symbol_by_name(symbol_name, NULL, NULL, &symbol, TRUE);
	return(symbol);
}

/*
 * _dyld_NSLookupAndBindSymbolWithHint() is the dyld side of
 * NSLookupAndBindSymbolWithHint().  It is just like
 * _dyld_NSLookupAndBindSymbol() except it takes a libraryNameHint.  A lookup
 * is first attempted using the libraryNameHint and if that fails then a
 * bind_symbol_by_name() is done.  The libraryNameHint and strstr(3) is used to 
 * match up against the library if the symbol is already bound in from that
 * library then the hint worked.
 */
static
struct nlist *
_dyld_NSLookupAndBindSymbolWithHint(
char *symbol_name,
char *libraryNameHint)
{
    struct nlist *defined_symbol;
    module_state *defined_module;
    struct image *defined_image;
    struct library_image *defined_library_image;
    enum link_state link_state;
    
	/* set lock for dyld data structures */
	set_lock();
	DYLD_TRACE_LIBFUNC_NAMED_START(DYLD_TRACE_NSLookupAndBindSymbolWithHint,
	    symbol_name);

	lookup_symbol_in_hinted_library(symbol_name, libraryNameHint,
	    &defined_symbol, &defined_module, &defined_image,
	    &defined_library_image);
	if(defined_symbol != NULL){
	    link_state = GET_LINK_STATE(*defined_module);
	    if(link_state == LINKED || link_state == FULLY_LINKED){
                DYLD_TRACE_LIBFUNC_END(
		    DYLD_TRACE_NSLookupAndBindSymbolWithHint);
		/* release lock for dyld data structures */
		release_lock();
		return(defined_symbol);
	    }
	}

	/* release lock for dyld data structures */
	release_lock();

	/*
	 * The locking is done inside bind_symbol_by_name() because it can cause
	 * the user's undefined symbol handler to be called and the lock must
	 * be released before it is called.
	 */
	bind_symbol_by_name(symbol_name, NULL, NULL, &defined_symbol, TRUE);
        
	DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_NSLookupAndBindSymbolWithHint);
	return(defined_symbol);
}

/*
 * _dyld_NSLookupSymbolInModule() is the dyld side of NSLookupSymbolInModule().
 * This is passed a module and a symbol name and returns an NSSymbol which is
 * implemented as a pointer to an nlist struct in an image.
 */
static
struct nlist *
_dyld_NSLookupSymbolInModule(
module_state *module,
char *symbol_name)
{
    unsigned long i;
    struct object_images *p;
    enum link_state link_state;

    struct nlist *defined_symbol;
    module_state *defined_module;
    struct image *defined_image;
    struct library_image *defined_library_image;

	/* set lock for dyld data structures */
	set_lock();
	DYLD_TRACE_LIBFUNC_NAMED_START(DYLD_TRACE_NSLookupSymbolInModule, 
	    symbol_name);
        
	defined_symbol = NULL;
/*
printf("In __dyld_NSLookupAndBindSymbol(module = 0x%x, symbol_name = %s)\n",
module, symbol_name);
*/

	/*
	 * This version of dyld only supports NSLookupSymbolInModule() for
	 * things that were NSLinkModule().  So ignore the executable image
	 * that is the first object file image (a hack but this version of 
	 * is a hack).  Then look through the object images trying to find this
	 * module.  Then if found search the image for the defined name.
	 */
	if(module == &(object_images.images[0].module))
	    goto done;

	for(p = &object_images; p != NULL; p = p->next_images){
	    for(i = 0; i < p->nimages; i++){
/*
printf("p->images[%lu].module = 0x%x name = %s\n", i, p->images[i].module, p->images[i].image.name);
*/
		if(module == &(p->images[i].module)){
		    /*
		     * If this object file image is currently unused the caller
		     * is using an unlinked module handle.
		     */
		    link_state = GET_LINK_STATE(p->images[i].module);
		    if(link_state == UNUSED)
			goto done;

		    (void)lookup_symbol_in_object_image(symbol_name,
			&(p->images[i].image), &(p->images[i].module),
			&defined_symbol, &defined_module, &defined_image,
			&defined_library_image, NULL);
		    goto done;
		}
	    }
	}
done:
	DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_NSLookupSymbolInModule);
	/* release lock for dyld data structures */
	release_lock();

	return(defined_symbol);
}

/*
 * _dyld_NSLookupSymbolInImage() is the dyld side of NSLookupSymbolInImage().
 * It is passed a pointer to a mach header of a dynamic library and a
 * symbol_name and a set of options.  It looks up and binds the symbol_name
 * from the dynamic library for the specified options and returns a pointer to
 * the symbol.
 */
static struct nlist *
_dyld_NSLookupSymbolInImage(
struct mach_header *mh,
char *symbol_name,
unsigned long options)
{
    unsigned long i;
    enum bool image_found, return_value, bind_now, bind_fully, module_index;
    struct object_images *p;
    struct library_images *q;
    enum link_state link_state;
    struct image *defined_image, *lookup_image;
    struct nlist *defined_symbol;
    module_state *defined_module;
    struct library_image *defined_library_image;
    struct object_image *object_image;
    char *image_name;

	/* set lock for dyld data structures */
	set_lock();
	DYLD_TRACE_LIBFUNC_NAMED_START(DYLD_TRACE_NSLookupSymbolInImage,
	    symbol_name);

	defined_symbol = NULL;
	image_name = NULL;
	/*
	 * First find the dynamic library for this mach header and lookup the
	 * the symbol_name in it if the mach header is a loaded dynamic library.
	 */
	i = 0;
	image_found = FALSE;
	for(q = &library_images; q != NULL; q = q->next_images){
	    for(i = 0; i < q->nimages; i++){
		if(q->images[i].image.mh == mh){
		    image_name = q->images[i].image.name;
		    if(force_flat_namespace == FALSE)
			lookup_image = &(q->images[i].image);
		    else
			lookup_image = NULL;
		    /*
		     * look this symbol up in this image.
		     */
		    lookup_symbol(symbol_name, lookup_image, NULL, FALSE,
				  &defined_symbol, &defined_module,
				  &defined_image, &defined_library_image, NULL);
		    image_found = TRUE;
		    goto down;
		}
	    }
	}
	for(p = &object_images; p != NULL; p = p->next_images){
	    for(i = 0; i < p->nimages; i++){
		if(p->images[i].image.mh == mh){
		    image_name = p->images[i].image.name;
		    if(force_flat_namespace == FALSE ||
		       p->images[i].image.private == TRUE)
			lookup_image = &(p->images[i].image);
		    else
			lookup_image = NULL;
		    /*
		     * look this symbol up in this image.
		     */
		    lookup_symbol(symbol_name, lookup_image, NULL, FALSE,
			          &defined_symbol, &defined_module,
				  &defined_image, &defined_library_image, NULL);
		    image_found = TRUE;
		    goto down;
		}
	    }
	}
down:
	/*
	 * If the mach header was invalid and return on error is set then set
	 * the error info before returning.
	 */
	if(image_found == FALSE){
	    if(options & LOOKUP_OPTION_RETURN_ON_ERROR){
		error("NSLookupSymbolInImage() invalid struct mach_header * "
		      "argument: 0x%x", (unsigned int)mh);
		return_on_error = TRUE;
		link_edit_error(DYLD_OTHER_ERROR, DYLD_INVALID_ARGS, NULL);
	    }
            DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_NSLookupSymbolInImage);
	    /*
	     * Release lock for dyld data structures.
	     * Note release_lock() alwasys clears return_on_error.
	     */
	    release_lock();
	    return(NULL);
	}
	/*
	 * If the symbol was not found in the library for the specified mach
	 * header and return on error is set then set the error info before
	 * returning.
	 */
	if(defined_image == NULL){
	    if(options & LOOKUP_OPTION_RETURN_ON_ERROR){
		error("NSLookupSymbolInImage() dynamic library: %s does not "
		      "define symbol: %s", image_name, symbol_name);
		return_on_error = TRUE;
		link_edit_error(DYLD_OTHER_ERROR, DYLD_INVALID_ARGS,
				image_name);
	    }
            DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_NSLookupSymbolInImage);
	    /*
	     * Release lock for dyld data structures.
	     * Note release_lock() always clears return_on_error.
	     */
	    release_lock();
	    return(NULL);
	}
	/*
	 * If the module that defines this symbol is linked enough to satisfy
	 * the options specified then there is nothing needed to do so just
	 * return the pointer to the symbol.
	 */
	link_state = GET_LINK_STATE(*defined_module);
	if((link_state == FULLY_LINKED &&
	     (options & LOOKUP_OPTION_BIND_FULLY) == 0) ||
	   (link_state == LINKED &&
	    (options & LOOKUP_OPTION_BIND_FULLY) == 0 &&
	    (options & LOOKUP_OPTION_BIND_NOW) == 0) ){
            DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_NSLookupSymbolInImage);
            /* release lock for dyld data structures */
            release_lock();
		
	    return(defined_symbol);
	}

	/*
	 * If the module that defines this symbol is not linked enough to
	 * satisfy the options specified so link the library module based on
	 * the options.
	 */
	if(options & LOOKUP_OPTION_RETURN_ON_ERROR)
	    return_on_error = TRUE;
	else
	    return_on_error = FALSE;
	if(options & LOOKUP_OPTION_BIND_NOW)
	    bind_now = TRUE;
	else
	    bind_now = FALSE;
	if(options & LOOKUP_OPTION_BIND_FULLY){
	    bind_fully = TRUE;
	    bind_now = TRUE;
	}
	else
	    bind_fully = FALSE;
	/*
	 * If return on error is TRUE then we may have to back out the module
	 * state changes to this library and any other library we cause modules
	 * to be linked in from if there later is an error.
	 * 
	 * So first call clear_module_states_saved to clear the
	 * module_states_saved fields of everything so that resolve_undefineds()
	 * will know to save the state of the modules it changes.
	 *
	 * Then save the module states of this library before linking in the
	 * needed module.
	 */
	if(return_on_error == TRUE){
	    clear_module_states_saved();
	    if(defined_library_image->saved_module_states == NULL){
		defined_library_image->saved_module_states =
		    allocate(defined_library_image->nmodules *
			     sizeof(module_state));
	    }
	    memcpy(defined_library_image->saved_module_states,
		   defined_library_image->modules,
		   defined_library_image->nmodules *
			sizeof(module_state));
	    defined_library_image->module_states_saved = TRUE;
	    if(defined_image->mh->filetype != MH_DYLIB){
		object_image = (struct object_image *)
				defined_image->outer_image;
		object_image->saved_module_state = object_image->module;
		object_image->module_state_saved = TRUE;
	    }
	}
	/*
	 * Link in the module that defines this symbol if needed.
	 */
	if(defined_image->mh->filetype == MH_DYLIB){
	    if(link_state == PREBOUND_UNLINKED){
		undo_prebinding_for_library_module(
		    defined_module,
		    defined_image,
		    defined_library_image);
		module_index = defined_module - defined_library_image->modules;
		SET_LINK_STATE(*defined_module, UNLINKED);
		link_state = UNLINKED;
		/*
		 * If we are doing return_on_error and this library may need
		 * its state restore if we hit a later error set the state
		 * to be restore to UNLINKED as we do not need to back out
		 * going from PREBOUND_UNLINKED to UNLINKED.
		 */
		if(return_on_error == TRUE &&
		   defined_library_image->saved_module_states != NULL)
		    SET_LINK_STATE(defined_library_image->saved_module_states
				   [module_index], UNLINKED);
	    }
	    if(link_state == UNLINKED ||
	       (bind_now == TRUE && link_state == LINKED) ||
	       (bind_fully == TRUE &&
		GET_FULLYBOUND_STATE(*defined_module) == 0) ){
		return_value = link_library_module(defined_library_image,
				    defined_image, defined_module,
				    bind_now, bind_fully, FALSE);
	    }
	    else{
		return_value = TRUE;
	    }
	}
	else{
	    if(bind_fully == TRUE && GET_FULLYBOUND_STATE(*defined_module)==0){
		object_image = (struct object_image *)
				defined_image->outer_image;
		return_value = link_object_module(object_image, bind_now,
						  bind_fully);
	    }
	    else{
		return_value = TRUE;
	    }
	}

	/*
	 * If we are doing return on error and the library module failed to be
	 * linked then back out the changes and return NULL.
	 */
	if(return_on_error == TRUE && return_value == FALSE)
	    goto back_out_changes;

	/*
	 * Now link in any needed modules to the level of binding specified in
	 * the options.
	 */
	return_value = link_in_need_modules(bind_fully, FALSE);

	/*
	 * Again if we are doing return on error and any needed library modules
	 * failed to be linked then back out the changes and return NULL.
	 */
	if(return_on_error == TRUE && return_value == FALSE)
	    goto back_out_changes;

	DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_NSLookupSymbolInImage);
	/* release lock for dyld data structures */
	release_lock();

	return(defined_symbol);

back_out_changes:
	/*
	 * Back out state changes to the modules that were changed.
	 */
	clear_state_changes_to_the_modules();
	/*
	 * Clear out symbols added to the being linked list and undefined
	 * list after return_on_error was set.
	 */
	clear_being_linked_list(TRUE);
	clear_undefined_list(TRUE);

	DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_NSLookupSymbolInImage);
	/* release lock for dyld data structures */
	release_lock();

	/* return NULL to our caller to indicate failure */
	return(NULL);
}

/*
 * _dyld_NSMakePrivateModulePublic() is the dyld side of the hack
 * NSMakePrivateModulePublic() needed for the dlopen() to turn it's
 * RTLD_LOCAL handles into RTLD_GLOBAL.  It just simply turns off the private
 * flag on the image for this module.  If the module was found and it was
 * private then everything worked and TRUE is returned else FALSE is returned.
 */
static
enum bool
_dyld_NSMakePrivateModulePublic(
module_state *module)
{
    unsigned long i;
    struct object_images *p;
    enum link_state link_state;
    enum bool retval;

	/* set lock for dyld data structures */
	set_lock();

	retval = FALSE;

	/*
	 * This only works for things that were NSLinkModule().  So ignore the
	 * executable image that is the first object file image (a hack but
	 * is a hack).  Then look through the object images trying to find this
	 * module.  Then if found see if private is TRUE and if so set it to
	 * FALSE.
	 */
	if(module == &(object_images.images[0].module))
	    goto done;

	for(p = &object_images; p != NULL; p = p->next_images){
	    for(i = 0; i < p->nimages; i++){
		if(module == &(p->images[i].module)){
		    /*
		     * If this object file image is currently unused the caller
		     * is using an unlinked module handle.
		     */
		    link_state = GET_LINK_STATE(p->images[i].module);
		    if(link_state == UNUSED)
			goto done;

		    if(p->images[i].image.private == FALSE)
			goto done;

		    p->images[i].image.private = FALSE;
		    retval = TRUE;
		    goto done;
		}
	    }
	}
done:
	/* release lock for dyld data structures */
	release_lock();

	return(retval);
}

/*
 * _dyld_NSIsSymbolNameDefined() is the dyld side of NSIsSymbolNameDefined().
 * This is passed a symbol name and returns TRUE if the symbol is defined and
 * FALSE in not.
 */
static
enum bool
_dyld_NSIsSymbolNameDefined(
char *symbol_name)
{
    enum bool defined;
    struct nlist *defined_symbol;
    module_state *defined_module;
    struct image *defined_image;
    struct library_image *defined_library_image;

	/* set lock for dyld data structures */
	set_lock();
	DYLD_TRACE_LIBFUNC_NAMED_START(DYLD_TRACE_NSIsSymbolNameDefined,
	    symbol_name);

	defined = FALSE;
	lookup_symbol(symbol_name, NULL, NULL, FALSE, &defined_symbol,
		      &defined_module, &defined_image, &defined_library_image,
		      NULL);
	if(defined_symbol != NULL)
	    defined = TRUE;

	DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_NSIsSymbolNameDefined);
        /* release lock for dyld data structures */
	release_lock();

	return(defined);
}

/*
 * _dyld_NSIsSymbolNameDefinedWithHint() is the dyld side of
 * NSIsSymbolNameDefinedWithHint(). It is just like
 * _dyld_NSIsSymbolNameDefined() except it takes a libraryNameHint.  A lookup
 * is first attempted using the libraryNameHint and if that fails then a
 * lookup_symbol() is done.  The libraryNameHint and strstr(3) is used to 
 * match up against the library if the symbol is already bound in from that
 * library then the hint worked.
 */
static
enum bool
_dyld_NSIsSymbolNameDefinedWithHint(
char *symbol_name,
char *libraryNameHint)
{
    enum bool defined;
    struct nlist *defined_symbol;
    module_state *defined_module;
    struct image *defined_image;
    struct library_image *defined_library_image;

	/* set lock for dyld data structures */
	set_lock();
	DYLD_TRACE_LIBFUNC_NAMED_START(DYLD_TRACE_NSIsSymbolNameDefinedWithHint,
	    symbol_name);
        
	defined = FALSE;

	lookup_symbol_in_hinted_library(symbol_name, libraryNameHint,
	    &defined_symbol, &defined_module, &defined_image,
	    &defined_library_image);
	if(defined_symbol != NULL){
	    defined = TRUE;
	}
	else{
	    lookup_symbol(symbol_name, NULL, NULL, FALSE, &defined_symbol,
			  &defined_module, &defined_image,
			  &defined_library_image, NULL);
	    if(defined_symbol != NULL)
		defined = TRUE;
	}

	DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_NSIsSymbolNameDefinedWithHint);
	/* release lock for dyld data structures */
	release_lock();

	return(defined);
}

/*
 * _dyld_NSIsSymbolNameDefinedInImage() is the dyld side of
 * NSIsSymbolNameDefinedInImage().  It looks up the specified symbol_name in the
 * dynamic library for the specified mach_header.  If it is defined there it
 * returns TRUE else it returns FALSE.
 */
static
enum bool
_dyld_NSIsSymbolNameDefinedInImage(
struct mach_header *mh,
char *symbol_name)
{
    unsigned long i;
    struct library_images *q;
    struct object_images *p;
    struct nlist *defined_symbol;
    module_state *defined_module;
    struct image *defined_image, *lookup_image;
    struct library_image *defined_library_image;

	/* set lock for dyld data structures */
	set_lock();
	DYLD_TRACE_LIBFUNC_NAMED_START(DYLD_TRACE_NSIsSymbolNameDefinedInImage,
	    symbol_name);
        
	defined_symbol = NULL;

	/*
	 * First find the dynamic library for this mach header and lookup the
	 * the symbol_name in it if the mach header is a loaded dynamic library.
	 */
	i = 0;
	for(q = &library_images; q != NULL; q = q->next_images){
	    for(i = 0; i < q->nimages; i++){
		if(q->images[i].image.mh == mh){
		    if(force_flat_namespace == FALSE)
			lookup_image = &(q->images[i].image);
		    else
			lookup_image = NULL;
		    /*
		     * look this symbol up in this image.
		     */
		    lookup_symbol(symbol_name, lookup_image, NULL, FALSE,
			          &defined_symbol, &defined_module,
				  &defined_image, &defined_library_image, NULL);
		    goto down;
		}
	    }
	}
	for(p = &object_images; p != NULL; p = p->next_images){
	    for(i = 0; i < p->nimages; i++){
		if(p->images[i].image.mh == mh){
		    if(force_flat_namespace == FALSE ||
		       p->images[i].image.private == TRUE)
			lookup_image = &(p->images[i].image);
		    else
			lookup_image = NULL;
		    /*
		     * look this symbol up in this image.
		     */
		    lookup_symbol(symbol_name, lookup_image, NULL, FALSE,
			          &defined_symbol, &defined_module,
				  &defined_image, &defined_library_image, NULL);
		    goto down;
		}
	    }
	}
down:
	DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_NSIsSymbolNameDefinedInImage);
	/* release lock for dyld data structures */
	release_lock();
        
	return(defined_image != NULL);
}

/*
 * _dyld_NSNameOfModule() is the dyld side of NSNameOfModule().  This is passed
 * an NSModule which is implemented as a pointer to a module_state.  It then
 * returns the name of the module if it is a valid module_state pointer
 * in a currently linked module.  If not it returns NULL.  This call is allowed
 * to be made from the user's multiply defined error handler.
 */
static
char *
_dyld_NSNameOfModule(
module_state *module)
{
    enum bool lock_set;
    struct image *defined_image;
    struct library_image *defined_library_image;
    struct dylib_module *dylib_modules;
    unsigned long module_index;
    char *strings;
    char *module_name;

	/*
	 * Set the lock for dyld data structures or verifiy this is being
	 * called from the user's multiply defined handler which this this
	 * routine is allowed to be called from.
	 */
	lock_set = set_lock_or_in_multiply_defined_handler();
	DYLD_TRACE_LIBFUNC_START(DYLD_TRACE_NSNameOfModule);
        	
	module_name = NULL;
	if(validate_NSModule(module, &defined_image,
			     &defined_library_image) == TRUE){
	    if(defined_library_image != NULL){
		dylib_modules = (struct dylib_module *)
		    (defined_image->vmaddr_slide +
		     defined_image->linkedit_segment->vmaddr +
		     defined_image->dyst->modtaboff -
		     defined_image->linkedit_segment->fileoff);
		module_index = module - defined_library_image->modules;
		strings = (char *)
		    (defined_image->vmaddr_slide +
		     defined_image->linkedit_segment->vmaddr +
		     defined_image->st->stroff -
		     defined_image->linkedit_segment->fileoff);
		module_name = strings + dylib_modules[module_index].module_name;
	    }
	    else{
		module_name = defined_image->name;
	    }
	}

	DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_NSNameOfModule);
	/* release lock for dyld data structures if we set it */
	if(lock_set == TRUE)
	    release_lock();

	return(module_name);
}

/*
 * _dyld_NSLibraryNameForModule() is the dyld side of NSLibraryNameForModule().
 * This is passed an NSModule which is implemented as a pointer to a
 * module_state.  It then returns the name of the library the module is in if
 * it is a valid module_state pointer in a currently linked module.  If not it
 * valid or the module is not in a library it returns NULL.  This call is
 * allowed to be made from the user's multiply defined error handler.
 */
static
char *
_dyld_NSLibraryNameForModule(
module_state *module)
{
    enum bool lock_set;
    struct image *defined_image;
    struct library_image *defined_library_image;
    char *library_name;

	/*
	 * Set the lock for dyld data structures or verifiy this is being
	 * called from the user's multiply defined handler which this this
	 * routine is allowed to be called from.
	 */
	lock_set = set_lock_or_in_multiply_defined_handler();
	DYLD_TRACE_LIBFUNC_START(DYLD_TRACE_NSLibraryNameForModule);
        	
	library_name = NULL;
	if(validate_NSModule(module, &defined_image,
			     &defined_library_image) == TRUE){
	    if(defined_library_image != NULL){
		library_name = defined_image->name;
	    }
	}

	DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_NSLibraryNameForModule);
	/* release lock for dyld data structures if we set it */
	if(lock_set == TRUE)
	    release_lock();

	return(library_name);
}

/*
 * _dyld_NSAddLibrary is the dyld side of NSAddLibrary().  It is passed a path
 * name of a library to load.
 */
static
enum bool
_dyld_NSAddLibrary(
char *dylib_name)
{
    enum bool return_value;
    char *p;

	/* set lock for dyld data structures */
	set_lock();
	DYLD_TRACE_LIBFUNC_NAMED_START(DYLD_TRACE_NSAddLibrary, dylib_name);
        
	p = allocate(strlen(dylib_name) + 1);
	strcpy(p, dylib_name);
	return_value = load_library_image(NULL, p, FALSE, FALSE, NULL);
	load_dependent_libraries();
	call_registered_funcs_for_add_images();

	/*
	 * Call the routine that gdb might have a break point on to let it
	 * know it is time to re-read the internal dyld structures as defined
	 * by <mach-o/dyld_gdb.h>
	 */
	gdb_dyld_state_changed();

	DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_NSAddLibrary);
	/* release lock for dyld data structures */
	release_lock();

	return(return_value);
}

/*
 * _dyld_NSAddLibraryWithSearching is the dyld side of
 * NSAddLibraryWithSearching().  It is passed a path name of a library to load
 * and allows the searching of the library with the various DYLD_ environment
 * variables.
 */
static
enum bool
_dyld_NSAddLibraryWithSearching(
char *dylib_name)
{
    enum bool return_value;
    char *p;

	/* set lock for dyld data structures */
	set_lock();
	DYLD_TRACE_LIBFUNC_NAMED_START(DYLD_TRACE_NSAddLibraryWithSearching, 
	    dylib_name);

	p = allocate(strlen(dylib_name) + 1);
	strcpy(p, dylib_name);
	return_value = load_library_image(NULL, p, TRUE, FALSE, NULL);
	load_dependent_libraries();
	call_registered_funcs_for_add_images();
        
	DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_NSAddLibraryWithSearching);
	/* release lock for dyld data structures */
	release_lock();

	return(return_value);
}

/*
 * _dyld_NSAddImage is the dyld side of NSAddImage().
 */
static
struct mach_header *
_dyld_NSAddImage(
char *dylib_name,
unsigned long options)
{
    enum bool return_value, force_searching, match_filename_by_installname;
    struct image *image;
    struct stat stat_buf;
    char *p;

	/* set lock for dyld data structures */
	set_lock();
	DYLD_TRACE_LIBFUNC_NAMED_START(DYLD_TRACE_NSAddImage, dylib_name);

	/*
	 * If the RETURN_ONLY_IF_LOADED option is used then check the library
	 * images loaded to see if this name is loaded.  Otherwise stat the
	 * dylib_name and see if the library is loaded by maching the stat
	 * struct info.  If it is not loaded return NULL.
	 */
	if(options & ADDIMAGE_OPTION_RETURN_ONLY_IF_LOADED){
	    return_value = is_library_loaded_by_name(dylib_name, NULL, &image);
	    if(return_value == TRUE){
		/* release lock for dyld data structures */
		release_lock();
		return(image->mh);
	    }
	    if(stat(dylib_name, &stat_buf) == -1){
		DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_NSAddImage);
		/* release lock for dyld data structures */
		release_lock();
		return(NULL);
	    }
	    return_value = is_library_loaded_by_stat(dylib_name, NULL,
						     &stat_buf, &image);
	    if(return_value == TRUE){
		DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_NSAddImage);
		/* release lock for dyld data structures */
		release_lock();
		return(image->mh);
	    }
            DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_NSAddImage);
	    /* release lock for dyld data structures */
	    release_lock();
	    return(NULL);
	}

	p = allocate(strlen(dylib_name) + 1);
	strcpy(p, dylib_name);

	if(options & ADDIMAGE_OPTION_WITH_SEARCHING)
	    force_searching = TRUE;
	else
	    force_searching = FALSE;

	if(options & ADDIMAGE_OPTION_RETURN_ON_ERROR)
	    return_on_error = TRUE;
	else
	    return_on_error = FALSE;
	if(options & ADDIMAGE_OPTION_MATCH_FILENAME_BY_INSTALLNAME)
	    match_filename_by_installname = TRUE;
	else
	    match_filename_by_installname = FALSE;

	return_value = load_library_image(NULL, p, force_searching,
					  match_filename_by_installname,&image);
	if(return_on_error == TRUE && return_value == FALSE){
            DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_NSAddImage);
	    /* release lock for dyld data structures */
	    release_lock();
            
	    return(NULL);
	}

	return_value = load_dependent_libraries();
	if(return_on_error == TRUE && return_value == FALSE){
	    /*
	     * Note that the library loaded with the above load_library_image()
	     * is also automaticly unloaded if load_dependent_libraries()
	     * returns FALSE since return_on_error was set to true before
	     * the call to load_library_image().
	     */
            DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_NSAddImage);
	    /* release lock for dyld data structures */
	    release_lock();
	    return(NULL);
	}

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

	call_registered_funcs_for_add_images();

	/*
	 * Release lock for dyld data structures.
	 * Note this will also set return_on_error to FALSE and clear and
	 * return_on_error bits that are set on libraries we loaded here.
	 */
	release_lock();

	/*
	 * Call the routine that gdb might have a break point on to let it
	 * know it is time to re-read the internal dyld structures as defined
	 * by <mach-o/dyld_gdb.h>
	 */
	gdb_dyld_state_changed();
    
	DYLD_TRACE_LIBFUNC_END(DYLD_TRACE_NSAddImage);
	return(image->mh);
}

/*
 *_dyld_NSGetExecutablePath is the dyld side of _NSGetExecutablePath which
 * copies the path of the executable into the buffer and returns 0 if the path
 * was successfully copied in the provided buffer. If the buffer is not large
 * enough, -1 is returned and the expected buffer size is copied in *bufsize.
 * Note that _NSGetExecutablePath will return "a path" to the executable not a
 * "real path" to the executable. That is the path may be a symbolic link and
 * not the real file. And with deep directories the total bufsize needed could
 * be more than MAXPATHLEN.
 */
static
int
_dyld_NSGetExecutablePath(
char *buf,
unsigned long *bufsize)
{
	/*
	 * This this string is setup in dyld_init() and never changed we don't
	 * lock the dyld data structures for this call.
	 */
	if(*bufsize < strlen(executables_path) + 1){
	    *bufsize = strlen(executables_path) + 1;
	    return(-1);
	}
	strcpy(buf, executables_path);
	return(0);
}

static
enum bool
_dyld_launched_prebound(
void)
{
	return(prebinding);
}
