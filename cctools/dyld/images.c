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
#import <libc.h>
#ifndef __OPENSTEP__
extern int add_profil(char *, int, int, int);
#endif
#import <stdlib.h>
#import <stdio.h>
#import <string.h>
#import <errno.h>
#import <limits.h>
#import <sys/file.h>
#import <sys/types.h>
#import <sys/stat.h>
#import <mach/mach.h>
#import "stuff/openstep_mach.h"
#import <mach-o/fat.h>
#import <mach-o/loader.h>
#import <mach-o/dyld_debug.h>
#import <mach-o/dyld_gdb.h>
#ifdef hppa
#import <mach-o/hppa/reloc.h>
#endif
#ifdef sparc
#import <mach-o/sparc/reloc.h>
#endif
#ifdef __ppc__
#import <mach-o/ppc/reloc.h>
#endif
#import <sys/mman.h>
#ifdef __OPENSTEP__
/* This should be in mman.h */
extern int mmap(char *, int, int, int, int, int);
/* This should be in stdlib.h */
extern char *realpath(const char *pathname, char resolvedname[MAXPATHLEN]); 
#import <mach-o/gmon.h>
#else
#import <sys/gmon.h>
#endif
/* This is in gmon.c but should be in gmon.h */
#define SCALE_1_TO_1 0x10000L
#if !defined(__GONZO_BUNSEN_BEAKER__) && !defined(__HERA__) && defined(__ppc__)
#include <architecture/ppc/processor_facilities.h>
#endif
#ifdef __MACH30__
#include <pthread.h>
#endif /* __MACH30__ */

#import "stuff/bool.h"
#import "stuff/best_arch.h"
#import "stuff/bytesex.h"
#import "stuff/round.h"

#ifndef __MACH30__
#import "../profileServer/profileServer.h"
#endif /* __MACH30__ */

#import "inline_strcmp.h"
#import "images.h"
#import "reloc.h"
#import "symbols.h"
#import "errors.h"
#import "allocate.h"
#import "dyld_init.h"
#import "entry_point.h"
#import "debug.h"
#import "register_funcs.h"
#import "lock.h"
#import "mod_init_funcs.h"

struct object_images object_images;
struct library_images library_images;

/* The name of the executable, argv[0], for error messages */
char *executables_name = NULL;

/*
 * loading_executables_libraries is set when loading libraries for the
 * executable.
 */
static enum bool loading_executables_libraries = TRUE;

/*
 * To support DYLD_ABORT_MULTIPLE_INITS this gets set before an image init
 * routine gets called and cleared after it has returned.
 */
static enum bool init_routine_being_called = FALSE;

/*
 * The value of $(HOME), the executable's path and the names of object file
 * images are stored in this string block (or just malloc()'ed they don't fit).
 * We need to have a copy of the name for error messages and can't rely on
 * using the name the user gave us when the object file image first came to be
 * as he may be reusing that storage.  This is all done to avoid malloc()'ing
 * space in the most likely cases (for $(HOME), the executable's path, and the
 * executable's name (argv[0]), and a reasonable number of bundles).  This is
 * done by the routine save_string() and some code in load_executable_image().
 */
enum string_block_size { STRING_BLOCK_SIZE = (MAXPATHLEN * 2) + 2 };
struct string_block {
    unsigned long used;		/* the number of bytes used */
    char strings[STRING_BLOCK_SIZE]; /* the strings */
};
static struct string_block string_block;

/*
 * The module_state's for libraries are allocated from this pool of
 * module_states.  This is is to avoid malloc()'ing space for a reasonable
 * number libraries in the most likely cases.
 *
 * Using /System/Library/CoreServices/Desktop.app/Contents/MacOS/Desktop from
 * MacOS X Public Beta (Kodiak1G7)
 *
 * Library		Number of modules
 * NSL				4
 * Print			26
 * Carbon			3
 * CoreFoundation		23
 * DesktopServices		18
 * PowerPlant			33
 * QuickTime			7
 * System			914
 * DesktopServicesPriv		28
 * URLMount			5
 * DiskManager			12
 * CoreServices			3
 * HIToolbox			4
 * Help				14
 * PrintCore			24
 * ImageCapture			13
 * SpeechRecognition		16
 * SecurityHI			16
 * FindByContent		14
 * URLAccess			16
 * ApplicationServices		3
 * OpenScripting		4
 * HTMLRendering		16
 * NavigationServices		16
 * CarbonSound			6
 * CommonPanels			4
 * IBCarbonRuntime		15
 * AE				18
 * AppleShareClient		13
 * DiskArbitration		5
 * IOKit			26
 * MediaKit			13
 * CarbonCore			17
 * NSLCore			13
 * OSServices			4
 * OT				7
 * SecurityCore			17
 * CoreGraphics			9
 * HIServices			7
 * ATS				22
 * QD				16
 * libClientPrintingLib.A.dylib	12
 * libJobTicket.A.dylib		17
 * libUtilities.A.dylib		28
 * ColorSync			5
 * SLDictionary			26
 * SpeechSynthesis		13
 * cdsa				26
 * cdsa_utilities		55
 * cdsa_client			26
 * SearchKit			13
 * CSScripting			4
 * LangAnalysis			16
 * CoreAudio			24
 * AppleShareClientCore		57
 * DesktopDB			4
 * AppleTalk			36
 * QuickTimeComponents.qtx	17
 *
 * TOTAL number of modules	1823
 * TOTAL number of libraries	58
 */
enum module_state_block_size { MODULE_STATE_BLOCK_SIZE = 2000 };
struct module_state_block {
    unsigned long used;		/* the number of items used */
    module_state module_states[MODULE_STATE_BLOCK_SIZE]; /* the module_states */
};
static struct module_state_block module_state_block;

static module_state *allocate_module_states(
    unsigned long nmodules);
static void deallocate_module_states(
    module_state *modules,
    unsigned long nmodules);

/*
 * The function pointer passed to _dyld_moninit() to do profiling of dyld loaded
 * code.  If this function pointer is not NULL at the time of a map_image()
 * called it is called indirectly to set up the profiling buffers.
 */
void (*dyld_monaddition)(char *lowpc, char *highpc) = NULL;

static struct object_image *new_object_image(
    void);
static struct library_image *new_library_image(
    unsigned long nmodules);
static char *get_framework_name(
    char *name,
    enum bool with_underbar_suffix);
static char *look_back_for_slash(
    char *name,
    char *p);
static char *search_for_name_in_path(
    char *name,
    char *path,
    char *suffix);
static char *construct_at_executable_path_library(
    char *dylib_name);
static enum bool map_library_image(
    struct dylib_command *dl,
    char *dylib_name,
    int fd,
    char *file_addr,
    unsigned long file_size,
    unsigned long library_offset,
    unsigned long library_size,
    dev_t dev,
    ino_t ino);
static enum bool validate_library(
    char *dylib_name,
    struct dylib_command *dl,
    struct library_image *li);
static enum bool is_library_loaded_by_name(
    char *dylib_name,
    struct dylib_command *dl);
static enum bool is_library_loaded_by_stat(
    char *dylib_name,
    struct dylib_command *dl,
    struct stat *stat_buf);
static enum bool set_prebound_state(
    struct prebound_dylib_command *pbdylib);

static enum bool check_image(
    char *name,
    char *image_type,
    unsigned long image_size,
    struct mach_header *mh,
    struct segment_command **linkedit_segment,
    struct segment_command **mach_header_segment,
    struct dysymtab_command **dyst,
    struct symtab_command **st,
    struct dylib_command **dlid,
    struct routines_command **rc,
    unsigned long *low_addr,
    unsigned long *high_addr);

static enum bool check_linkedit_info(
    char *name,
    char *image_type,
    struct segment_command *linkedit_segment,
    struct symtab_command *st,
    struct dysymtab_command *dyst,
    struct routines_command *rc);

static enum bool map_image(
    char *name,
    char *image_type,
    unsigned long image_size,
    int fd,
    char *file_addr,
    unsigned long file_size,
    unsigned long library_offset,
    unsigned long library_size,
    unsigned long low_addr,
    unsigned long high_addr,
    struct mach_header **mh,
    struct segment_command **linkedit_segment,
    struct dysymtab_command **dyst,
    struct symtab_command **st,
    struct dylib_command **dlid,
    struct routines_command **rc,
    enum bool *change_protect_on_reloc,
    enum bool *cache_sync_on_reloc,
    enum bool *has_coalesced_sections,
    struct section **init,
    struct section **term,
    unsigned long *seg1addr,
    unsigned long *segs_read_write_addr,
    unsigned long *slide_value,
    unsigned long *images_dyld_stub_binding_helper);
static void set_segment_protections(
    char *name,
    char *image_type,
    struct mach_header *mh,
    unsigned long slide_value);
static enum bool load_images_libraries(
    struct mach_header *mh);
static void unload_shared_file(
    struct library_image *library_image);
static void undo_prebinding_for_library(
    struct library_image *library_image);
static void failed_use_prebound_libraries(
    void);
static void reset_module_states(
    void);
static void call_dependent_init_routines(
    struct library_image *library_image,
    struct image *image,
    module_state *module,
    enum bool use_header_dependencies);
#ifdef __MACH30__
static void setup_for_lazy_init_routines(
    void);
/*
 * These are the values returned by task_get_exception_ports() which we will
 * use to forward exceptions that my exception handler will not be handling.
 */
static exception_mask_t old_exception_masks[1];
static exception_port_t old_exception_ports[1];
static exception_behavior_t old_behaviors[1];
static thread_state_flavor_t old_flavors[1];

/*
 * This could come from <mach/exc_server.h> created with:
 *
 * mig -sheader exc_server.h exc.defs
 *
 * in the libc build so internal_catch_exc_subsystem.maxsize can be used.
 */
struct internal_catch_exc_subsystem {
        struct subsystem *      subsystem;      /* Reserved for system use */
        mach_msg_id_t   start;  /* Min routine number */
        mach_msg_id_t   end;    /* Max routine number + 1 */
        unsigned int    maxsize;        /* Max msg size */
        vm_address_t    base_addr;      /* Base ddress */
        struct routine_descriptor       /*Array of routine descriptors */
                routine[3];
        struct routine_arg_descriptor   /*Array of arg descriptors */
                arg_descriptor[16];
};
extern struct internal_catch_exc_subsystem internal_catch_exc_subsystem;
/*
 * This is the maximum size of the exception message.
 */
#define MY_MSG_SIZE internal_catch_exc_subsystem.maxsize

/* These are not declared in any header file */
extern boolean_t exc_server(
    mach_msg_header_t * in_msg,
    mach_msg_header_t * out_msg);
extern kern_return_t exception_raise(
    exception_port_t exception_port,
    thread_port_t thread,
    task_port_t task, 
    exception_type_t exception,
    exception_data_t code,
    mach_msg_type_number_t code_count);
extern kern_return_t exception_raise_state(
    exception_port_t exception_port,
    exception_type_t exception,
    exception_data_t code,
    mach_msg_type_number_t code_count,
    thread_state_flavor_t *flavor,
    thread_state_t in_state,
    mach_msg_type_number_t in_state_count,
    thread_state_t *out_state,
    mach_msg_type_number_t *out_state_count);
extern kern_return_t exception_raise_state_identity(
    exception_port_t exception_port,
    thread_port_t thread,
    task_port_t task, 
    exception_type_t exception,
    exception_data_t code,
    mach_msg_type_number_t code_count,
    thread_state_flavor_t *flavor,
    thread_state_t in_state,
    mach_msg_type_number_t in_state_count,
    thread_state_t *out_state,
    mach_msg_type_number_t *out_state_count);

static void exception_server_loop(
    mach_port_t my_exception_port);
static enum bool call_lazy_init_routine_for_address(
    unsigned long address);
#endif __MACH30__
#ifdef DEBUG_LAZY_INIT_EXCEPTIONS
static const char * exception_name(
    exception_type_t exception);
#endif /* DEBUG_LAZY_INIT_EXCEPTIONS */

/*
 * The address of these symbols are written in to the (__DATA,__dyld) section
 * at the following offsets:
 *	at offset 0	stub_binding_helper_interface
 *	at offset 4	_dyld_func_lookup
 *	at offset 8	start_debug_thread
 * The 'C' types (if any) for these symbols are ignored here and all are
 * declared as longs so the assignment of their address in to the section will
 * not require a cast.  stub_binding_helper_interface is really a label in the
 * assembly code interface for the stub binding.  It does not have a meaningful 
 * 'C' type.  _dyld_func_lookup is the routine in dyld_libfuncs.c.
 * start_debug_thread is the routine in debug.c.
 *
 * For ppc the image's stub_binding_binding_helper is read from:
 *	at offset 20	the image's stub_binding_binding_helper address
 * and saved into to the image structure.
 */
extern long stub_binding_helper_interface;
extern long _dyld_func_lookup;

/*
 * load_executable_image() loads up the executable into the dynamic linker's
 * data structures (the kernel has loaded the segments into memory).  Then it
 * calls load_images_libraries() for all dynamic shared libraries the executable
 * uses.
 */
void
load_executable_image(
char *name,
struct mach_header *mh,
unsigned long *entry_point)
{
    unsigned long i, j, seg1addr;
    struct load_command *lc, *load_commands;
    struct segment_command *sg, *linkedit_segment;
    struct section *s, *init, *term;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    struct thread_command *thread_command;
    struct object_image *object_image;
    enum bool change_protect_on_reloc, cache_sync_on_reloc,
	      has_coalesced_sections, seg1addr_found;
    char *dylib_name, *p;
#ifdef __ppc__
    unsigned long images_dyld_stub_binding_helper;

	images_dyld_stub_binding_helper =
	    (unsigned long)(&unlinked_lazy_pointer_handler);
#endif

	/* set for error reporting in here */
	executables_name = name;

	/*
	 * Pick up the linkedit segment and dynamic symbol command from the
	 * executable.  We don't check for the error of having more than one of
	 * these but just pick up the first one if any.  Checks to see if the
	 * load commands for an executable are valid are not done as we assume
	 * they are good since the kernel used them and we are running.
	 */
	linkedit_segment = NULL;
	st = NULL;
	dyst = NULL;
	init = NULL;
	term = NULL;
	seg1addr_found = FALSE;
	seg1addr = 0;
	change_protect_on_reloc = FALSE;
	cache_sync_on_reloc = FALSE;
	has_coalesced_sections = FALSE;
	load_commands = (struct load_command *)
			((char *)mh + sizeof(struct mach_header));
	lc = load_commands;
	for(i = 0; i < mh->ncmds; i++){
	    switch(lc->cmd){
	    case LC_SEGMENT:
		sg = (struct segment_command *)lc;
		if(strcmp(sg->segname, SEG_LINKEDIT) == 0){
		    if(linkedit_segment == NULL)
			linkedit_segment = sg;
		}
		/*
		 * Pickup the address of the first segment.  Note this may not
		 * be the lowest address, but it is the address relocation
		 * entries are based off of.
		 */
		if(seg1addr_found == FALSE){
		    seg1addr = sg->vmaddr;
		    seg1addr_found = TRUE;
		}
		/*
		 * Stuff the address of the stub_binding_helper_interface into
		 * the first 4 bytes of the (__DATA,__dyld) section if there is
		 * one.  And stuff the address of _dyld_func_lookup in the
		 * second 4 bytes of the (__DATA,__dyld) section.  And stuff the
		 * address of start_debug_thread in the third 4 bytes of the
		 * (__DATA,__dyld) section.
		 */
		s = (struct section *)
		    ((char *)sg + sizeof(struct segment_command));
		for(j = 0; j < sg->nsects; j++){
		    if(strcmp(s->segname, "__DATA") == 0 &&
		       strcmp(s->sectname, "__dyld") == 0){
			if(s->size >= sizeof(unsigned long)){
			    *((long *)s->addr) =
				(long)&stub_binding_helper_interface;
			}
			if(s->size >= 2 * sizeof(unsigned long)){
			    *((long *)(s->addr + 4)) =
				(long)&_dyld_func_lookup;
			}
			if(s->size >= 3 * sizeof(unsigned long)){
			    *((long *)(s->addr + 8)) =
				(long)&start_debug_thread;
			}
#ifdef __ppc__
			if(s->size >= 5 * sizeof(unsigned long)){
			    images_dyld_stub_binding_helper = 
				*((long *)(s->addr + 20));
			}
#endif
		    }
		    s++;
		}
		/*
		 * If this segment is not to have write protection then check to
		 * see if any of the sections have external relocations and if
		 * so mark the image as needing to change protections when doing
		 * relocation in it.
		 */
		if((sg->initprot & VM_PROT_WRITE) == 0){
		    s = (struct section *)
			((char *)sg + sizeof(struct segment_command));
		    for(j = 0; j < sg->nsects; j++){
			if((s->flags & S_ATTR_EXT_RELOC)){
			    if((sg->maxprot & VM_PROT_READ) == 0 ||
			       (sg->maxprot & VM_PROT_WRITE) == 0){
	    			error("malformed executable: %s (segment %.16s "
				    "has relocation entries but the max vm "
				    "protection does not allow reading and "
				    "writing)", name, sg->segname);
	    			link_edit_error(DYLD_FILE_FORMAT, EBADMACHO,
				    name);
			    }
			    change_protect_on_reloc = TRUE;
			    break;
			}
			s++;
		    }
		}
		/*
		 * If the image has relocations for instructions then the i
		 * cache needs to sync with d cache on relocation.  A good guess
		 * is made based on the section attributes and section name.
		 */
		s = (struct section *)
		    ((char *)sg + sizeof(struct segment_command));
		for(j = 0; j < sg->nsects; j++){
		    if(((strcmp(s->segname, "__TEXT") == 0 &&
		         strcmp(s->sectname, "__text") == 0) ||
			 (s->flags & S_ATTR_SOME_INSTRUCTIONS)) &&
			 (s->flags & S_ATTR_EXT_RELOC)){
			cache_sync_on_reloc = TRUE;
			break;
		    }
		    s++;
		}
		/*
		 * If the image has a module init section pick it up.
		 */
		if(init == NULL){
		    s = (struct section *)
			((char *)sg + sizeof(struct segment_command));
		    for(j = 0; j < sg->nsects; j++){
			if((s->flags & SECTION_TYPE) ==
			   S_MOD_INIT_FUNC_POINTERS){
			    init = s;
			    break;
			}
			s++;
		    }
		}
		/*
		 * If the image has a module term section pick it up.
		 */
		if(term == NULL){
		    s = (struct section *)
			((char *)sg + sizeof(struct segment_command));
		    for(j = 0; j < sg->nsects; j++){
			if((s->flags & SECTION_TYPE) ==
			   S_MOD_TERM_FUNC_POINTERS){
			    term = s;
			    break;
			}
			s++;
		    }
		}
		/*
		 * If the image has any coalesced sections note that
		 */
		if(has_coalesced_sections == FALSE){
		    s = (struct section *)
			((char *)sg + sizeof(struct segment_command));
		    for(j = 0; j < sg->nsects; j++){
			if((s->flags & SECTION_TYPE) == S_COALESCED){
			    has_coalesced_sections = TRUE;
			    break;
			}
			s++;
		    }
		}
		break;
	    case LC_SYMTAB:
		if(st == NULL)
		    st = (struct symtab_command *)lc;
		break;
	    case LC_DYSYMTAB:
		if(dyst == NULL)
		    dyst = (struct dysymtab_command *)lc;
		break;
	    case LC_UNIXTHREAD:
		thread_command = (struct thread_command *)lc;
		*entry_point = get_entry_point(thread_command);
		break;
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
	/*
	 * We allow anything that make some sense and issue warnings that most
	 * likely are errors for the executable (in libraries these are hard
	 * errors).
	 */
	if(st == NULL && dyst != NULL){
	    error("malformed executable: %s (dynamic symbol table command "
		"but no standard symbol table command)", name);
	    link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
	    dyst = NULL;
	}
	else if(linkedit_segment == NULL){
	    if(dyst != NULL){
		error("malformed executable: %s (dynamic symbol table command"
		    "but no " SEG_LINKEDIT "segment)", name);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		error("malformed executable: %s (symbol table command but no "
		    SEG_LINKEDIT "segment)", name);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
	    }
	    else{
		error("possible malformed executable: %s (no dynamic symbol "
		    "table command)", name);
		link_edit_error(DYLD_WARNING, EBADMACHO, name);
		error("possible malformed executable: %s (no " SEG_LINKEDIT
		    " segment)", name);
		link_edit_error(DYLD_WARNING, EBADMACHO, name);
	    }
	    /* without a linkedit segment we can't get to the symbol table */
	    st = NULL;
	    dyst = NULL;
	}
	else{
	    if(dyst == NULL){
		error("possible malformed executable: %s (no dynamic symbol "
		    "table command)", name);
		link_edit_error(DYLD_WARNING, EBADMACHO, name);
	    }
	    else{
		check_linkedit_info(name, "executable", linkedit_segment, st,
				    dyst, NULL);
	    }
	}

	/*
	 * Set up the executable as the first object image loaded.
	 */
	object_image = new_object_image();
	object_image->image.name = save_string(name);
	executables_name = object_image->image.name;
	object_image->image.vmaddr_slide = 0;
	/* note the executable is not always contiguious in memory and should
	   not be deallocated using vmaddr_size */
	object_image->image.vmaddr_size = 0;
	object_image->image.seg1addr = seg1addr;
	object_image->image.mh = mh;
	object_image->image.st = st;
	object_image->image.dyst = dyst;
	object_image->image.linkedit_segment = linkedit_segment;
	object_image->image.change_protect_on_reloc = change_protect_on_reloc;
	object_image->image.cache_sync_on_reloc = cache_sync_on_reloc;
	object_image->image.has_coalesced_sections = has_coalesced_sections;
	object_image->image.init = init;
	object_image->image.term = term;
#ifdef __ppc__
	object_image->image.dyld_stub_binding_helper =
			    images_dyld_stub_binding_helper;
#endif
	object_image->image.valid = TRUE;
	SET_LINK_STATE(object_image->module, BEING_LINKED);

	/*
	 * If DYLD_INSERT_LIBRARIES is set insert the libraries listed.
	 */
	if(dyld_insert_libraries != NULL){
	    if(dyld_print_libraries == TRUE)
		print("loading libraries for DYLD_INSERT_LIBRARIES=%s\n",
			dyld_insert_libraries);
	    dylib_name = dyld_insert_libraries;
	    for(;;){
		p = strchr(dylib_name, ':');
		if(p != NULL)
		    *p = '\0';
		/*
		 * This feature disables prebinding.  We know that launched is
		 * FALSE at this point.
		 */
		if(dyld_prebind_debug != 0 && prebinding == TRUE)
		    print("dyld: %s: prebinding disabled due to "
			   "DYLD_INSERT_LIBRARIES being set\n",
			   executables_name);
		prebinding = FALSE;
		(void)load_library_image(NULL, dylib_name, FALSE);
		if(p == NULL){
		    break;
		}
		else{
		    *p = ':';
		    dylib_name = p + 1;
		}
	    }
	}

	/*
	 * Now load the library images this executable uses.
	 */
	if(dyld_print_libraries == TRUE)
	    print("loading libraries for image: %s\n",
		   object_image->image.name);
	loading_executables_libraries = TRUE;
	(void)load_images_libraries(mh);
	loading_executables_libraries = FALSE;

	/*
	 * Load the dependent libraries.
	 */
	load_dependent_libraries();

	/*
	 * Call the routine that gdb might have a break point on to let it
	 * know it is time to re-read the internal dyld structures as defined
	 * by <mach-o/dyld_gdb.h>
	 */
	gdb_dyld_state_changed();
}

/*
 * Look for the library in the DYLD_FRAMEWORK_PATH path list.
 */
static
char *
try_framework_search(
char *dylib_name,
char *path,
char *suffix)
{
    char *name, *new_dylib_name;

	if(path == NULL){
	    return(NULL);
	}
	new_dylib_name = NULL;
	if((name = get_framework_name(dylib_name, FALSE)) != NULL){
	    new_dylib_name = search_for_name_in_path(name, path, suffix);
	}
	if(new_dylib_name == NULL){
	    if((name = get_framework_name(dylib_name, TRUE)) != NULL){
		new_dylib_name = search_for_name_in_path(name, path, suffix);
	    }
	}
	return new_dylib_name;
}

/*
 * try_library_search() looks for the library in the specified path list with
 * the specified suffix (which maybe NULL).  If it finds a library it returns
 * the path name else it returns NULL.
 */
static
char *
try_library_search(
char *dylib_name,
char *path,
char *suffix)
{
    char *name, *new_dylib_name;

	if(path == NULL){
	    return(NULL);
	}
	new_dylib_name = NULL;
	name = strrchr(dylib_name, '/');
	if(name != NULL && name[1] != '\0')
	    name++;
	else
	    name = dylib_name;
	new_dylib_name = search_for_name_in_path(name, path, suffix);
	return new_dylib_name;
}

/*
 * try_image_suffix_search() takes a name of a dynamic library and a possible
 * suffix and looks for the library with the given suffix.  The standard
 * makefiles build dynamic libraries (not Frameworks) with names like this:
 *    libTestLib.A.dylib
 *    libTestLib.dylib
 *    libTestLib.A_profile.dylib
 *    libTestLib_profile.dylib
 * So if the names ends in .dylib then the suffix is tacked on just before the
 * the .dylib.  For everything else (Frameworks and bundles) the suffix is just
 * appended to the end which matches the standard makefiles.  Note if there was
 * a suffix in the dynamic library passed to us it is not "changed" to the
 * suffix passed in, just an attempt to tack it on is maded.
 */
static
char *
try_image_suffix_search(
char *dylib_name,
char *suffix)
{
    char *new_dylib_name, *ext;
    struct stat stat_buf;
    long nmlen;

	if(suffix == NULL){
	    return(NULL);
	}
	nmlen = strlen(dylib_name);
	new_dylib_name = allocate(nmlen + strlen(suffix) + 1);
	ext = strrchr(dylib_name, '.');
	if(nmlen > 6 && ext != NULL && strcmp(ext, ".dylib") == 0){
	    strcpy(new_dylib_name, dylib_name);
	    new_dylib_name[nmlen - 6] = '\0';
	    strcat(new_dylib_name, suffix);
	    strcat(new_dylib_name, ".dylib");
	}
	else{
	    strcpy(new_dylib_name, dylib_name);
	    strcat(new_dylib_name, suffix);
	}
	if(stat(new_dylib_name, &stat_buf) == 0){
	    return(new_dylib_name);
	}
	free(new_dylib_name);
	return(NULL);
}

/*
 * Look for needed '@executable_path/' substitution and do it, first
 * with suffix, then without.
 */
static
char *
try_executable_search(
char *dylib_name,
char *suffix)
{
    char *constructed_name, *new_dylib_name;
    struct stat stat_buf;

	constructed_name = NULL;
	if(strncmp(dylib_name, "@executable_path/",
		    sizeof("@executable_path/")-1) == 0){
	    if(executables_path != NULL){
		constructed_name =
			construct_at_executable_path_library(dylib_name);
		new_dylib_name = try_image_suffix_search(constructed_name, suffix);
		if (new_dylib_name) {
		    free(constructed_name);
		    return(new_dylib_name);
		}
		if(stat(constructed_name, &stat_buf) == 0){
		    return(constructed_name);
		}
		free(constructed_name);
	    }
	    else{
		/*
		 * We do not have the executables_path so we can't
		 * construct the library name. We'll try the fall back
		 * paths but most likely we'll fail.
		 */
		error("can not get executable's path, so can't construct "
			"path for library: %s", dylib_name);
		link_edit_error(DYLD_WARNING, ENOENT, dylib_name);
	    }
	}

	return(NULL);
}

/*
 * load_library_image() causes the specified dynamic shared library to be loaded
 * into memory and added to the dynamic link editor data structures to use it.
 * Specifily this routine takes a pointer to a dylib_command for a library and
 * finds and opens the file corresponding to it.  It deals with the file being
 * fat and then calls map_library_image() to have the library's segments mapped
 * into memory. 
 */
enum bool
load_library_image(
struct dylib_command *dl, /* allow NULL for NSAddLibrary() to use this */
char *dylib_name,
enum bool force_searching)
{
    char *new_dylib_name, *constructed_name;
    int fd, errnum, save_errno;
    struct stat stat_buf;
    unsigned long file_size;
    char *file_addr;
    kern_return_t r;
    struct fat_header *fat_header;
    struct fat_arch *fat_archs, *best_fat_arch;
    struct mach_header *mh;

	new_dylib_name = NULL;
	constructed_name = NULL;
	/*
	 * If the dylib_command is not NULL then this is not a result of a call
	 * to NSAddLibrary() so searching may take place.  Otherwise, just open
	 * the name passed in.
	 */
	if(dl != NULL || force_searching == TRUE){
	    if(dl != NULL)
		dylib_name = (char *)dl + dl->dylib.name.offset;
	    /*
	     * If the dyld_framework_path is set and this dylib_name is a
	     * framework name, use the first file that exists in the framework
	     * path if any.  If there is none go on to search the
	     * dyld_library_path if any.
	     */
	    if(dyld_framework_path != NULL)
		new_dylib_name = try_framework_search(dylib_name,
						      dyld_framework_path,
						      dyld_image_suffix);
	    /*
	     * If the dyld_library_path is set then use the first file that
	     * exists in the path.  If none use the original name. 
	     * The string dyld_library_path points to is "path1:path2:path3" and
	     * comes from the enviroment variable DYLD_LIBRARY_PATH.
	     */
	    if(new_dylib_name == NULL && dyld_library_path != NULL)
		new_dylib_name = try_library_search(dylib_name,
						    dyld_library_path,
						    dyld_image_suffix);
	    /*
	     * If we haven't done any searching and found a library and the
	     * dylib_name starts with "@executable_path/" then construct the
	     * library name.
	     */
	    if(new_dylib_name == NULL &&
	       strncmp(dylib_name, "@executable_path/",
		       sizeof("@executable_path/")-1) == 0){
		constructed_name = try_executable_search(dylib_name,
							 dyld_image_suffix);
		if(constructed_name != NULL)
		    dylib_name = constructed_name;
	    }
	    /*
	     * Finally, if no other searching has been done or successful, if
	     * dyld_image_suffix is set try the name with the suffix.
	     */
	    if(new_dylib_name == NULL &&
	       constructed_name == NULL &&
	       dyld_image_suffix != NULL)
		new_dylib_name = try_image_suffix_search(dylib_name,
							 dyld_image_suffix);

	    /*
	     * If we found a new name use it in place of the original.
	     */
	    if(new_dylib_name != NULL)
		dylib_name = new_dylib_name;
	}
	/*
	 * If the library is already loaded just return.
	 */
	if(is_library_loaded_by_name(dylib_name, dl) == TRUE){
	    if(new_dylib_name != NULL)
		free(new_dylib_name);
	    if(constructed_name != NULL)
		free(constructed_name);
	    /* if dl is NULL free() the NSAddLibrary() allocated dylib_name */
	    if(dl == NULL)
		free(dylib_name);
	    return(TRUE);
	}

	/*
	 * Open the file descriptor.
	 */
	fd = open(dylib_name, O_RDONLY, 0);

	/*
	 * If the open failed and the dylib_command is not NULL (so searching
	 * may take place) and we have not previously found a name then try
	 * searching the fall back paths (including the default fall back
	 * framework path).
	 */
	if(fd == -1 &&
	    (dl != NULL || force_searching == TRUE) &&
	    new_dylib_name == NULL &&
	    constructed_name == NULL){
	    save_errno = errno;
	    /*
	     * First try the the dyld_fallback_framework_path if that has
	     * been set.
	     */
	    new_dylib_name = try_framework_search(dylib_name,
						  dyld_fallback_framework_path,
						  dyld_image_suffix);
	    /*
	     * If a new name is still not found try
	     * dyld_fallback_library_path if that was set.
	     */
	    if(new_dylib_name == NULL){
		new_dylib_name = try_library_search(dylib_name,
						    dyld_fallback_library_path,
						    dyld_image_suffix);
	    }
	    /*
	     * If a new name is still not found use the default fallback
	     * framework and library paths creating them if they have not
	     * yet been created.
	     */
	    if(new_dylib_name == NULL){
		if(default_fallback_framework_path == NULL){
		    default_fallback_framework_path = allocate(strlen(home) +
				sizeof(DEFAULT_FALLBACK_FRAMEWORK_PATH));
		    strcpy(default_fallback_framework_path, home);
		    strcat(default_fallback_framework_path,
				DEFAULT_FALLBACK_FRAMEWORK_PATH);
		}
		new_dylib_name = try_framework_search(
					dylib_name,
					default_fallback_framework_path,
					dyld_image_suffix);
	    }
	    if(new_dylib_name == NULL){
		if(default_fallback_library_path == NULL){
		    default_fallback_library_path = allocate(strlen(home) +
				sizeof(DEFAULT_FALLBACK_LIBRARY_PATH));
		    strcpy(default_fallback_library_path, home);
		    strcat(default_fallback_library_path,
				DEFAULT_FALLBACK_LIBRARY_PATH);
		}
		new_dylib_name = try_library_search(
					dylib_name,
					default_fallback_library_path,
					dyld_image_suffix);
	    }
	    /*
	     * If the search through the fall back paths found a new path
	     * then open it.  If no name was ever found put back the errno
	     * from the original open that failed.
	     */
	    if(new_dylib_name != NULL) {
		dylib_name = new_dylib_name;
		fd = open(dylib_name, O_RDONLY, 0);
	    }
	    else {
		errno = save_errno;
	    }
	}
	/*
	 * The file name that will be used for this library has been opened.
	 * If that failed report it and return.  For fixed address shared
	 * libraries the kernel maps the library and if it is not present you
	 * get an EBADEXEC, for access permissions you get EACCES ...  Here we
	 * just return whatever the errno from the open call happens to be.
	 * So this won't match what the kernel might have done (but we also
	 * don't allow execute only permissions as we require read permission).
	 */
	if(fd == -1){
	    errnum = errno;
	    /*
	     * If we constructed a library name that had "@executable_path/"
	     * in it use the constructed name to report the error.
	     */
	    if(constructed_name != NULL)
		dylib_name = constructed_name;
	    system_error(errnum, "can't open library: %s ", dylib_name);
	    link_edit_error(DYLD_FILE_ACCESS, errnum, dylib_name);
	    if(new_dylib_name != NULL)
		free(new_dylib_name);
	    if(constructed_name != NULL)
		free(constructed_name);
	    return(FALSE);
	}

	if(dyld_print_libraries == TRUE)
	    print("loading library: %s\n", dylib_name);

	/*
	 * Fill the stat buffer.
	 */
	if(fstat(fd, &stat_buf) == -1){
	    errnum = errno;
	    system_error(errnum, "can't stat library: %s", dylib_name);
	    link_edit_error(DYLD_FILE_ACCESS, errnum, dylib_name);
	    goto load_library_image_cleanup2;
	}
	/*
	 * If the library is already loaded just return.
	 */
	if(is_library_loaded_by_stat(dylib_name, dl, &stat_buf) == TRUE){
	    close(fd);
	    if(new_dylib_name != NULL)
		free(new_dylib_name);
	    if(constructed_name != NULL)
		free(constructed_name);
	    /* if dl is NULL free() the NSAddLibrary() allocated dylib_name */
	    if(dl == NULL)
		free(dylib_name);
	    return(TRUE);
	}
	/*
	 * Now that the dylib_name has been determined and opened get it into
	 * memory by mapping it.
	 */
	file_size = stat_buf.st_size;
	/*
	 * For some reason mapping files with zero size fails so it has to
	 * be handled specially.
	 */
	if(file_size == 0){
	    error("truncated or malformed library: %s (file is empty)",
		dylib_name);
	    link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, dylib_name);
	    goto load_library_image_cleanup2;
	}
	/*
	 * Since directories can be opened but not mapped check to see this
	 * is a plain file.  Which will give a less confusing error message.
	 */
	if((stat_buf.st_mode & S_IFMT) != S_IFREG){
	    error("file is not a regular file: %s (can't possibly be a "
		  "library)", dylib_name);
	    link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, dylib_name);
	    goto load_library_image_cleanup2;
	}
	if((r = map_fd((int)fd, (vm_offset_t)0, (vm_offset_t *)&file_addr,
	    (boolean_t)TRUE, (vm_size_t)file_size)) != KERN_SUCCESS){
	    mach_error(r, "can't map library: %s", dylib_name);
	    link_edit_error(DYLD_MACH_RESOURCE_RECOVERABLE, r, dylib_name);
	    goto load_library_image_cleanup2;
	}

	/*
	 * Determine what type of file it is fat or thin and if it is even an
	 * object file.
	 */
	if(sizeof(struct fat_header) > file_size){
	    error("truncated or malformed library: %s (file too small to be "
		  "a library)", dylib_name);
	    link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, dylib_name);
	    goto load_library_image_cleanup1;
	}
	fat_header = (struct fat_header *)file_addr;
#ifdef __BIG_ENDIAN__
	if(fat_header->magic == FAT_MAGIC)
#endif
#ifdef __LITTLE_ENDIAN__
	if(fat_header->magic == SWAP_LONG(FAT_MAGIC))
#endif
	{
#ifdef __LITTLE_ENDIAN__
	    swap_fat_header(fat_header, LITTLE_ENDIAN_BYTE_SEX);
#endif
	    if(sizeof(struct fat_header) + fat_header->nfat_arch *
	       sizeof(struct fat_arch) > file_size){
		error("truncated or malformed library: %s (fat file's fat_arch "
		      "structs extend past the end of the file)", dylib_name);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, dylib_name);
		goto load_library_image_cleanup1;
	    }
	    fat_archs = (struct fat_arch *)(file_addr +
					    sizeof(struct fat_header));
#ifdef __LITTLE_ENDIAN__
	    swap_fat_arch(fat_archs, fat_header->nfat_arch,
			  LITTLE_ENDIAN_BYTE_SEX);
#endif
	    best_fat_arch = cpusubtype_findbestarch(host_basic_info.cpu_type,
						    host_basic_info.cpu_subtype,
						    fat_archs,
						    fat_header->nfat_arch);
	    if(best_fat_arch == NULL){
		error("bad CPU type in library: %s", dylib_name);
		link_edit_error(DYLD_FILE_FORMAT, EBADARCH, dylib_name);
		goto load_library_image_cleanup1;
	    }
	    if(sizeof(struct mach_header) > best_fat_arch->size){
		error("truncated or malformed library: %s (file too small to "
		      "be a library)", dylib_name);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, dylib_name);
		goto load_library_image_cleanup1;
	    }
	    mh = (struct mach_header *)(file_addr + best_fat_arch->offset);
	    if(mh->magic != MH_MAGIC){
		error("malformed library: %s (not a Mach-O file, bad magic "
		    "number)", dylib_name);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, dylib_name);
		goto load_library_image_cleanup1;
	    }
	    /*
	     * This file has the right magic number so try to map it in. 
	     * map_library_image() will close the file descriptor and
	     * deallocate the mapped in memory.
	     */
	    return(map_library_image(dl, dylib_name, fd, file_addr, file_size,
			             best_fat_arch->offset, best_fat_arch->size,
			             stat_buf.st_dev, stat_buf.st_ino));
	}
	else{
	    if(sizeof(struct mach_header) > file_size){
		error("truncated or malformed library: %s (file too small to "
		      "be a library)", dylib_name);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, dylib_name);
		goto load_library_image_cleanup1;
	    }
	    mh = (struct mach_header *)file_addr;
	    if(mh->magic == SWAP_LONG(MH_MAGIC)){
		error("bad CPU type in library: %s", dylib_name);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, dylib_name);
		goto load_library_image_cleanup1;
	    }
	    if(mh->magic != MH_MAGIC){
		error("malformed library: %s (not a Mach-O file, bad magic "
		    "number)", dylib_name);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, dylib_name);
		goto load_library_image_cleanup1;
	    }
	    /*
	     * This file has the right magic number so try to map it in. 
	     * map_library_image() will close the file descriptor and
	     * deallocate the mapped in memory.
	     */
	    return(map_library_image(dl, dylib_name, fd, file_addr, file_size,
				     0, file_size, stat_buf.st_dev,
				     stat_buf.st_ino));
	}

load_library_image_cleanup1:
	if((r = vm_deallocate(mach_task_self(), (vm_address_t)file_addr,
	    (vm_size_t)file_size)) != KERN_SUCCESS){
	    mach_error(r, "can't vm_deallocate map_fd memory for library: %s",
		dylib_name);
	    link_edit_error(DYLD_MACH_RESOURCE, r, dylib_name);
	}

load_library_image_cleanup2:
	if(close(fd) == -1){
	    errnum = errno;
	    system_error(errnum, "can't close file descriptor for library: %s ",
		 dylib_name);
	    link_edit_error(DYLD_UNIX_RESOURCE, errnum, dylib_name);
	}
	if(new_dylib_name != NULL)
	    free(new_dylib_name);
	if(constructed_name != NULL)
	    free(constructed_name);
	return(FALSE);
}

/*
 * get_framework_name() is passed a name of a dynamic library and returns a
 * pointer to the start of the framework name if one exist or NULL none exists.
 * A framework name can take one of the following two forms:
 *	Foo.framework/Versions/A/Foo
 *	Foo.framework/Foo
 * Where A and Foo can be any string.  If with_underbar_suffix is TRUE then
 * try to parse off a trailing suffix starting with an underbar.
 */
static
char *
get_framework_name(
char *name,
enum bool with_underbar_suffix)
{
    char *foo, *a, *b, *c, *d, *suffix;
    unsigned long l, s;

	/* pull off the last component and make foo point to it */
	a = strrchr(name, '/');
	if(a == NULL)
	    return(NULL);
	if(a == name)
	    return(NULL);
	foo = a + 1;
	l = strlen(foo);
	
	/* look for suffix if requested starting with a '_' */
	if(with_underbar_suffix){
	    suffix = strrchr(foo, '_');
	    if(suffix != NULL){
		s = strlen(suffix);
		if(suffix == foo || s < 2)
		    suffix = NULL;
		else
		    l -= s;
	    }
	}

	/* first look for the form Foo.framework/Foo */
	b = look_back_for_slash(name, a);
	if(b == NULL){
	    if(strncmp(name, foo, l) == 0 &&
	       strncmp(name + l, ".framework/", sizeof(".framework/")-1 ) == 0)
		return(name);
	    else
		return(NULL);
	}
	else{
	    if(strncmp(b+1, foo, l) == 0 &&
	       strncmp(b+1 + l, ".framework/", sizeof(".framework/")-1 ) == 0)
		return(b+1);
	}

	/* next look for the form Foo.framework/Versions/A/Foo */
	if(b == name)
	    return(NULL);
	c = look_back_for_slash(name, b);
	if(c == NULL ||
	   c == name ||
	   strncmp(c+1, "Versions/", sizeof("Versions/")-1) != 0)
	    return(NULL);
	d = look_back_for_slash(name, c);
	if(d == NULL){
	    if(strncmp(name, foo, l) == 0 &&
	       strncmp(name + l, ".framework/", sizeof(".framework/")-1 ) == 0)
		return(name);
	    else
		return(NULL);
	}
	else{
	    if(strncmp(d+1, foo, l) == 0 &&
	       strncmp(d+1 + l, ".framework/", sizeof(".framework/")-1 ) == 0)
		return(d+1);
	    else
		return(NULL);
	}
}

/*
 * look_back_for_slash() is passed a string name and an end point in name to
 * start looking for '/' before the end point.  It returns a pointer to the
 * '/' back from the end point or NULL if there is none.
 */
static
char *
look_back_for_slash(
char *name,
char *p)
{
	for(p = p - 1; p >= name; p--){
	    if(*p == '/')
		return(p);
	}
	return(NULL);
}

/*
 * search_for_name_in_path() is used in searching for name in the
 * DYLD_LIBRARY_PATH or DYLD_FRAMEWORK_PATH.  It is passed a name, path,
 * and suffix and returns the name of the first combination that exist
 * or NULL if none exists.  The dylib is looked for first with the
 * suffix, then without.
 */
static
char *
search_for_name_in_path(
char *name,
char *path,
char *suffix)
{
    char *dylib_name, *new_dylib_name, *p;
    struct stat stat_buf;

	dylib_name = allocate(strlen(name) + strlen(path) + 2);
	for(;;){
	    p = strchr(path, ':');
	    if(p != NULL)
		*p = '\0';
	    if(*path == '\0')
		goto next_path;
	    strcpy(dylib_name, path);
	    strcat(dylib_name, "/");
	    strcat(dylib_name, name);
	    new_dylib_name = try_image_suffix_search(dylib_name, suffix);
	    if(new_dylib_name != NULL){
		free(dylib_name);
		if(p != NULL)
		    *p = ':';
		return(new_dylib_name);
	    }
	    if(stat(dylib_name, &stat_buf) == 0){
		if(p != NULL)
		    *p = ':';
		return(dylib_name);
	    }
	    if(p == NULL){
		free(dylib_name);
		return(NULL);
	    }
	    else{
next_path:
		*p = ':';
		path = p + 1;
	    }
	}
	/* can't get here */
	return(NULL);
}

/*
 * construct_at_executable_path_library() takes a dylib_name that starts with
 * "@executable_path/" and constructs a name replacing that with the
 * executable's path and then returning the constructed name.
 */
static
char *
construct_at_executable_path_library(
char *dylib_name)
{
    unsigned long m, n;
    char *p;
    static char *resolvedname = NULL;
    static unsigned long resolvedname_len = 0;

	/*
	 * To handle a symbolic link to the executable we need to call
	 * realpath() so we can get the directory the executable is really in.
	 */
	if(resolvedname == NULL){
	    resolvedname = allocate(MAXPATHLEN + 1);
	    p = realpath(executables_path, resolvedname);
	    if(p == NULL){
		system_error(errno, "can't get realpath of executable: %s ",
			     executables_path);
		link_edit_error(DYLD_FILE_ACCESS, errno, resolvedname);

		free(resolvedname);
		resolvedname = NULL;

		/*
		 * We can't get a resolved path so just use the executable's
		 * path and hope it works.  Note that executables_path is an
		 * absolute path so strrchr(, '/') will not return NULL.
		 */
		p = strrchr(executables_path, '/');
		m = (p - executables_path) + 1;
		n = strlen(dylib_name + (sizeof("@executable_path/") - 1));
		p = allocate(m + n + 1);
		strncpy(p, executables_path, m);
		strcpy(p + m,
		       dylib_name + (sizeof("@executable_path/") - 1));
		return(p);
	    }
	}
	if(resolvedname_len == 0){
	    /*
	     * Note that resolvedname is an absolute path so strrchr(, '/')
	     * will not return NULL.
	     */
	    p = strrchr(resolvedname, '/');
	    resolvedname_len = (p - resolvedname) + 1;
	}

	n = strlen(dylib_name + (sizeof("@executable_path/") - 1));
	p = allocate(resolvedname_len + n + 1);
	strncpy(p, resolvedname, resolvedname_len);
	strcpy(p + resolvedname_len,
	       dylib_name + (sizeof("@executable_path/") - 1));
	return(p);
}

/*
 * map_library_image() maps segments of the specified library into memory and
 * adds the library to the list of library images.
 */
static
enum bool
map_library_image(
struct dylib_command *dl, /* allow NULL for NSAddLibrary() to use this */
char *dylib_name,
int fd,
char *file_addr,
unsigned long file_size,
unsigned long library_offset,
unsigned long library_size,
dev_t dev,
ino_t ino)
{
    struct mach_header *mh;
    int errnum;
    kern_return_t r;
    unsigned long low_addr, high_addr, slide_value, seg1addr,
		  segs_read_write_addr;
    struct segment_command *linkedit_segment, *mach_header_segment;
    struct dysymtab_command *dyst;
    struct symtab_command *st;
    struct dylib_command *dlid;
    struct routines_command *rc;
    enum bool change_protect_on_reloc, cache_sync_on_reloc,
	      has_coalesced_sections;
    struct section *init, *term;
    struct library_image *library_image;
    struct dyld_event event;
    char *name;
    unsigned long images_dyld_stub_binding_helper;

#ifdef __ppc__
	images_dyld_stub_binding_helper =
	    (unsigned long)(&unlinked_lazy_pointer_handler);
#endif
	/*
	 * On entry the only checks that have been done are the file_addr and
	 * file_size have only been checked so that file_size >= sizeof(mach
	 * _header) and the magic number MH_MAGIC is correct.  The caller has
	 * checked that the library_offset to the library_size is contained in
	 * the file_size. file_addr is guaranteed to be on a page boundary as		 * allocated by mach.  All file format errors reported here will be
	 * DYLD_FILE_FORMAT and EBADMACHO.
	 */
	mh = (struct mach_header *)(file_addr + library_offset);
	if(check_image(dylib_name, "library", library_size, mh,
	    &linkedit_segment, &mach_header_segment, &dyst, &st, &dlid, &rc,
	    &low_addr, &high_addr) == FALSE)
	    goto map_library_image_cleanup;

	/*
	 * Do the library specific checks on the mach header and load commands.
	 */
	if(mh->filetype != MH_DYLIB){
	    error("malformed library: %s (not a Mach-O library file, bad "
		"filetype value)", dylib_name);
	    link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, dylib_name);
	    goto map_library_image_cleanup;
	}
	if(dlid == NULL){
	    error("malformed library: %s (Mach-O library file, does not have "
		"an LC_ID_DYLIB command)", dylib_name);
	    link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, dylib_name);
	    goto map_library_image_cleanup;
	}
	/*
	 * If the compatibility version of the user of the library is
	 * greater than the library then it is an error and the
	 * library can't be used.
	 */
	if(dl != NULL &&
	   dl->dylib.compatibility_version >
	   dlid->dylib.compatibility_version){
	    error("version mismatch for library: %s (compatibility "
		"version of user: %lu.%lu.%lu greater than library's version: "
		"%lu.%lu.%lu)", dylib_name,
		dl->dylib.compatibility_version >> 16,
		(dl->dylib.compatibility_version >> 8) & 0xff,
		dl->dylib.compatibility_version & 0xff,
		dlid->dylib.compatibility_version >> 16,
		(dlid->dylib.compatibility_version >> 8) & 0xff,
		dlid->dylib.compatibility_version & 0xff);
	    link_edit_error(DYLD_FILE_FORMAT, ESHLIBVERS, dylib_name);
	    goto map_library_image_cleanup;
	}

	/*
	 * Now that the library checks out map the library in.
	 */
	if(map_image(dylib_name, "library", library_size, fd, file_addr, 
	    file_size, library_offset, library_size, low_addr, high_addr, &mh,
	    &linkedit_segment, &dyst, &st, &dlid, &rc, &change_protect_on_reloc,
	    &cache_sync_on_reloc, &has_coalesced_sections, &init, &term,
	    &seg1addr, &segs_read_write_addr, &slide_value,
	    &images_dyld_stub_binding_helper) == FALSE){

	    return(FALSE);
	}

	/*
	 * To avoid allocating space for the dylib_name we use the name
	 * from the library's id command when it is the same as the
	 * dylib_name passed in.  As the space for the name in the library's
	 * id command will never go away as libraries are not unloaded but
	 * dylib_name may point at a load library command in an object that
	 * might be unloaded.
	 */
	name = (char *)dlid + dlid->dylib.name.offset;
	if(strcmp(dylib_name, name) == 0)
	    dylib_name = name;

	/*
	 * This library is now successfully mapped in add it to the list of
	 * libraries.
	 */
	library_image = new_library_image(dyst->nmodtab);
	library_image->image.name = dylib_name;
	library_image->image.vmaddr_slide = slide_value;
	library_image->image.vmaddr_size = high_addr - low_addr;
	library_image->image.seg1addr = seg1addr;
	library_image->image.segs_read_write_addr = segs_read_write_addr;
	library_image->image.mh = mh;
	library_image->image.st = st;
	library_image->image.dyst = dyst;
	library_image->image.rc = rc;
	library_image->image.linkedit_segment = linkedit_segment;
	library_image->image.change_protect_on_reloc = change_protect_on_reloc;
	library_image->image.cache_sync_on_reloc = cache_sync_on_reloc;
	library_image->image.has_coalesced_sections = has_coalesced_sections;
	library_image->image.init = init;
	library_image->image.term = term;
#ifdef __ppc__
	library_image->image.dyld_stub_binding_helper =
			     images_dyld_stub_binding_helper;
#endif
	library_image->dlid = dlid; 
	library_image->dev = dev;
	library_image->ino = ino;
	library_image->remove_on_error = return_on_error;
	library_image->library_offset = library_offset;
	library_image->image.valid = TRUE;

	/*
	 * Do local relocation if this library was slid.  This also disables 
	 * prebinding and undoes the prebinding
	 */
	if(slide_value != 0){
	    if(dyld_prebind_debug != 0 &&
	       prebinding == TRUE &&
	       launched == FALSE)
		print("dyld: %s: prebinding disabled because library: %s got "
		       "slid\n", executables_name, dylib_name);
	    if(launched == FALSE)
		prebinding = FALSE;
	    local_relocation(&(library_image->image));
	    relocate_symbol_pointers_for_defined_externs(
		&(library_image->image));
	}

	/*
	 * If this library is not prebound then disable prebinding.
	 */
	if((mh->flags & MH_PREBOUND) != MH_PREBOUND){
	    if(dyld_prebind_debug != 0 &&
	       prebinding == TRUE &&
	       launched == FALSE)
		print("dyld: %s: prebinding disabled because library: %s not "
		       "prebound\n", executables_name, dylib_name);
	    if(launched == FALSE)
		prebinding = FALSE;
	}
	else{
	    /*
	     * The library is prebound.  If we have already launched the
	     * program we can't use the prebinding and it must be undone.
	     */
	    if(launched == TRUE)
		undo_prebinding_for_library(library_image);
	}

	/*
	 * Check to see if the time stamps match of the user of this library an
	 * in the id of this library.
	 */
	if(dl != NULL &&
	   dl->dylib.timestamp != dlid->dylib.timestamp){
	    if(prebinding == TRUE && launched == FALSE){
		/*
		 * The timestamps don't match so if we are not loading
		 * libraries from the executable then prebinding is always 
		 * disabled.  If we are loading libraries from the executable
		 * and the executable was prebound then also disable prebinding.
		 * This allows trying to use just prebound libraries.
		 */
		if(loading_executables_libraries == FALSE ||
		   executable_prebound == TRUE){
		    if(dyld_prebind_debug != 0)
			print("dyld: %s: prebinding disabled because time "
			       "stamp of library: %s did not match\n",
			       executables_name, dylib_name);
		    prebinding = FALSE;
		}
	    }
	}

	/*
	 * Set the segment protections on the library now that relocation is
	 * done.
	 */
	set_segment_protections(dylib_name, "library", mh, slide_value);

	/* send the event message that this image was added */
	memset(&event, '\0', sizeof(struct dyld_event));
	event.type = DYLD_IMAGE_ADDED;
	event.arg[0].header = mh;
	event.arg[0].vmaddr_slide = slide_value;
	event.arg[0].module_index = 0;
	send_event(&event);

	return(TRUE);

map_library_image_cleanup:
	/*
	 * The above label is used in error conditions before map_image() is
	 * called.  map_image() does this cleanup.
	 */
	if((r = vm_deallocate(mach_task_self(), (vm_address_t)file_addr,
	    (vm_size_t)file_size)) != KERN_SUCCESS){
	    mach_error(r, "can't vm_deallocate map_fd memory for library: %s",
		dylib_name);
	    link_edit_error(DYLD_MACH_RESOURCE, r, dylib_name);
	}
	if(close(fd) == -1){
	    errnum = errno;
	    system_error(errnum, "can't close file descriptor for library: %s ",
		 dylib_name);
	    link_edit_error(DYLD_UNIX_RESOURCE, errnum, dylib_name);
	}
	return(FALSE);
}

/*
 * map_bundle_image() maps segments of the specified bundle into memory and
 * adds the bundle to the list of object images.  This is used to implement the
 * NSloadModule() api.
 */
struct object_image *
map_bundle_image(
char *name,
char *object_addr,
unsigned long object_size)
{
    struct mach_header *mh;
    unsigned long low_addr, high_addr, slide_value, seg1addr,
		  segs_read_write_addr;
    struct segment_command *linkedit_segment, *mach_header_segment;
    struct dysymtab_command *dyst;
    struct symtab_command *st;
    enum bool change_protect_on_reloc, cache_sync_on_reloc,
	      has_coalesced_sections;
    struct section *init, *term;
    struct object_image *object_image;
    struct dyld_event event;
    unsigned long images_dyld_stub_binding_helper;
#ifdef __ppc__
	images_dyld_stub_binding_helper =
	    (unsigned long)(&unlinked_lazy_pointer_handler);
#endif

	/*
	 * This routine only deals with MH_BUNDLE files that are on page
	 * boundaries.  The library code for NSloadModule() insures this for
	 * it's call to here and deals with things that are not this type in
	 * the library code.
	 */
	if(((int)object_addr % vm_page_size) != 0){
	    error("malformed object file image: %s (address of image not on a "
		"page boundary)", name);
	    link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
	    return(NULL);
	}
	if(sizeof(struct mach_header) > object_size){
	    error("truncated or malformed object file image: %s (too small to "
		"be an object file image)", name);
	    link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
	    return(NULL);
	}
	mh = (struct mach_header *)object_addr;
	if(mh->magic != MH_MAGIC){
	    error("malformed object file image: %s (not a Mach-O image, bad "
		"magic number)", name);
	    link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
	    return(NULL);
	}
	/*
	 * Now that it looks like it could be an object file image check it.
	 */
	if(check_image(name, "object file image", object_size, mh,
	    &linkedit_segment, &mach_header_segment, &dyst, &st, NULL, NULL,
	    &low_addr, &high_addr) == FALSE)
	    return(NULL);
	/*
	 * Do the bundle specific check on the mach header.
	 */
	if(mh->filetype != MH_BUNDLE){
	    error("malformed object file image: %s (not a Mach-O bundle file, "
		"bad filetype value)", name);
	    link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
	    return(NULL);
	}

	/*
	 * Now that the object file image checks out map it in.
	 */
	if(map_image(name, "object file image", object_size, -1, NULL, 
	    0, 0, 0, low_addr, high_addr, &mh, &linkedit_segment, &dyst, &st,
	    NULL, NULL, &change_protect_on_reloc, &cache_sync_on_reloc,
	    &has_coalesced_sections, &init, &term, &seg1addr,
	    &segs_read_write_addr, &slide_value,
	    &images_dyld_stub_binding_helper) == FALSE){

	    return(NULL);
	}

	/*
	 * This object file image is now successfully mapped in add it to the
	 * list of object image loaded.
	 */
	object_image = new_object_image();
	object_image->image.name = save_string(name);
	object_image->image.vmaddr_slide = slide_value;
	object_image->image.vmaddr_size = high_addr - low_addr;
	object_image->image.seg1addr = seg1addr;
	object_image->image.segs_read_write_addr = segs_read_write_addr;
	object_image->image.mh = mh;
	object_image->image.st = st;
	object_image->image.dyst = dyst;
	object_image->image.linkedit_segment = linkedit_segment;
	object_image->image.change_protect_on_reloc = change_protect_on_reloc;
	object_image->image.cache_sync_on_reloc = cache_sync_on_reloc;
	object_image->image.has_coalesced_sections = has_coalesced_sections;
	object_image->image.init = init;
	object_image->image.term = term;
#ifdef __ppc__
	object_image->image.dyld_stub_binding_helper =
			    images_dyld_stub_binding_helper;
#endif
	object_image->image.valid = TRUE;
	SET_LINK_STATE(object_image->module, UNLINKED);

	/*
	 * Do local relocation if this object file image was slid.
	 */
	if(slide_value != 0){
	    local_relocation(&(object_image->image));
	    relocate_symbol_pointers_for_defined_externs(
		&(object_image->image));
	}

	/*
	 * Set the segment protections on the object file image now that
	 * relocation is done.
	 */
	set_segment_protections(name, "object file image", mh, slide_value);

	/* send the event message that this image was added */
	memset(&event, '\0', sizeof(struct dyld_event));
	event.type = DYLD_IMAGE_ADDED;
	event.arg[0].header = mh;
	event.arg[0].vmaddr_slide = slide_value;
	event.arg[0].module_index = 0;
	send_event(&event);

	/*
	 * Now load each of the libraries this object file image loads.
	 */
	if(dyld_print_libraries == TRUE)
	    print("loading libraries for image: %s\n",
		   object_image->image.name);
	if(load_images_libraries(mh) == FALSE &&
	   return_on_error == TRUE){
	    /*
	     * If we are doing return on error and the libraries for this
	     * bundle can't be loaded load_images_libraries() will unload
	     * any libraries for this image and we just need to unload the
	     * bundle image.  Since at this point no symbols have been bound
	     * we can just fake this bundle as being private and call
	     * unload_bundle_image() to do all the unloading.
	     */
	    object_image->image.private = TRUE;
	    unload_bundle_image(object_image, FALSE, FALSE);
	    return(NULL);
	}

	/*
	 * Return the module.
	 */
	return(object_image);
}

/*
 * unload_bundle_image() is the hack that unlinks a module loaded with
 * NSUnlinkModule().
 */
void
unload_bundle_image(
struct object_image *object_image,
enum bool keepMemoryMapped,
enum bool reset_lazy_references)
{
    kern_return_t r;
    struct dyld_event event;

	object_image->image.valid = FALSE;

	/* send the event message that this module was removed */
	memset(&event, '\0', sizeof(struct dyld_event));
	event.type = DYLD_MODULE_REMOVED;
	event.arg[0].header = object_image->image.mh;
	event.arg[0].vmaddr_slide = object_image->image.vmaddr_slide;
	event.arg[0].module_index = 0;
	send_event(&event);
	/*
	 * If the memory for this module is going to be deallocated send the
	 * event message that this image was removed.
	 */
	if(keepMemoryMapped == FALSE){
	    memset(&event, '\0', sizeof(struct dyld_event));
	    event.type = DYLD_IMAGE_REMOVED;
	    event.arg[0].header = object_image->image.mh;
	    event.arg[0].vmaddr_slide = object_image->image.vmaddr_slide;
	    event.arg[0].module_index = 0;
	    send_event(&event);
	}

	/*
	 * If this is not a private image remove the defined symbols in the
	 * module and create undefined symbols if any of them are currently
	 * referenced.  Then check and report any undefined symbols that may
	 * have been created.
	 */
	if(object_image->image.private == FALSE){
	    unlink_object_module(object_image, reset_lazy_references);
	    check_and_report_undefineds();
	}

	/*
	 * Deallocate the memory for this image if keepMemoryMapped is FALSE.
	 */
	if(keepMemoryMapped == FALSE){
/*
printf("keepMemoryMapped == FALSE doing vm_deallocate() mh = 0x%x size = 0x%x\n",
object_image->image.mh, object_image->image.vmaddr_size);
*/
	    if((r = vm_deallocate(mach_task_self(),
			(vm_address_t)object_image->image.mh,
			(vm_size_t)object_image->image.vmaddr_size)) !=
			KERN_SUCCESS){
		mach_error(r, "can't vm_deallocate memory for module: %s",
			   object_image->image.name);
		link_edit_error(DYLD_MACH_RESOURCE, r,object_image->image.name);
	    }
	}

	/*
	 * Mark this object file image structure unused and clean it up.
	 * TODO: reclaim the storage for the name:
	unsave_string(object_image->image.name);
	 */
	memset(object_image, '\0', sizeof(struct object_image));
	SET_LINK_STATE(object_image->module, UNUSED);

	return;
}


/*
 * map_image() maps an images' Mach-O segments into memory.  If sucessfull it
 * returns TRUE if not it returns false.  In either case if the image is from
 * a file the file descriptor is closed and the map_fd() memory is deallocated.
 */
static
enum bool
map_image(
    /* input */
char *name,
char *image_type,
unsigned long image_size,
int fd,
char *file_addr,
unsigned long file_size,
unsigned long library_offset,
unsigned long library_size,
unsigned long low_addr,
unsigned long high_addr,
    /* in/out */
struct mach_header **mh,
    /* output */
struct segment_command **linkedit_segment,
struct dysymtab_command **dyst,
struct symtab_command **st,
struct dylib_command **dlid,
struct routines_command **rc,
enum bool *change_protect_on_reloc,
enum bool *cache_sync_on_reloc,
enum bool *has_coalesced_sections,
struct section **init,
struct section **term,
unsigned long *seg1addr,
unsigned long *segs_read_write_addr,
unsigned long *slide_value,
unsigned long *images_dyld_stub_binding_helper)
{
    vm_address_t address, image_addr;
    vm_size_t size;
#ifdef __MACH30__
    vm_region_info_data_t info;
    mach_msg_type_number_t infoCnt;
#else
    vm_prot_t protection, max_protection;
    vm_inherit_t inheritance;
    boolean_t shared;
    vm_offset_t offset;
#endif
    mach_port_t object_name;
    enum bool slide_it, in_the_way;

    int errnum;
    kern_return_t r;
    unsigned long i, j;
    struct load_command *lc, *load_commands;
    struct segment_command *sg;
    struct section *s;
    unsigned long mach_header_segment_vmaddr;

#ifdef SHARED_LIBRARY_SERVER_SUPPORTED
    unsigned long nsegs;
    int ret;
#define ARRAY_ENTRIES 5
    struct sf_mapping sf_mapping_array[ARRAY_ENTRIES], *sf_mapping_pointer, *m;
    vm_address_t base_address;
    int flags;
    static enum bool first_load_shared_file = TRUE;
#endif /* SHARED_LIBRARY_SERVER_SUPPORTED */

	mach_header_segment_vmaddr = 0;

#ifdef SHARED_LIBRARY_SERVER_SUPPORTED
	/*
	 * For MH_SPLIT_SEGS images they are mapped using the load_shared_file()
	 * call not mupliple map_fd() calls.
	 */
	if((*mh)->flags & MH_SPLIT_SEGS){
	    /* first count the number of segments and get the base address */
	    nsegs = 0;
	    *seg1addr = ULONG_MAX;
	    *segs_read_write_addr = ULONG_MAX;
	    load_commands = (struct load_command *)((char *)*mh +
						    sizeof(struct mach_header));
	    lc = load_commands;
	    for(i = 0; i < (*mh)->ncmds; i++){
		switch(lc->cmd){
		case LC_SEGMENT:
		    sg = (struct segment_command *)lc;
		    nsegs++;
		    /* pickup the address of the first segment */
		    if(sg->vmaddr < *seg1addr)
			*seg1addr = sg->vmaddr;
		    /* if this segment has a zero-fill area account for it */
		    if((sg->initprot & VM_PROT_WRITE) == VM_PROT_WRITE &&
		       sg->vmsize > sg->filesize)
			nsegs++;
		    /*
		     * Pickup the address of the first read-write segment for
		     * MH_SPLIT_SEGS images.
		     */
		    if((sg->initprot & VM_PROT_WRITE) == VM_PROT_WRITE &&
		       sg->vmaddr < *segs_read_write_addr)
			*segs_read_write_addr = sg->vmaddr;
		    if(sg->fileoff == 0)
			mach_header_segment_vmaddr = sg->vmaddr;
		}
		lc = (struct load_command *)((char *)lc + lc->cmdsize);
	    }
	    /* set up to use or allocate a set of sf_mapping structs */
	    if(nsegs <= ARRAY_ENTRIES){
		sf_mapping_pointer = NULL;
		m = sf_mapping_array;
	    }
	    else{
		sf_mapping_pointer = allocate(sizeof(struct sf_mapping) *
					      nsegs);
		m = sf_mapping_pointer;
	    }
	    /*
	     * Fill in the sf_mapping structs for each of the segments from the
	     * file.
	     */
	    j = 0;
	    lc = load_commands;
	    for(i = 0; i < (*mh)->ncmds; i++){
		switch(lc->cmd){
		case LC_SEGMENT:
		    sg = (struct segment_command *)lc;
		    m[j].cksum = 0;
		    m[j].size = sg->filesize;
		    m[j].file_offset = sg->fileoff + library_offset;
		    m[j].mapping_offset = sg->vmaddr - *seg1addr;
		    if((sg->initprot & VM_PROT_WRITE) == VM_PROT_WRITE)
			m[j].protection = sg->initprot | VM_PROT_COW;
		    else
			m[j].protection = sg->initprot;
		    j++;
		    /* if this segment has a zero-fill area create a mapping */
		    if((sg->initprot & VM_PROT_WRITE) == VM_PROT_WRITE &&
		       sg->vmsize > sg->filesize){
			m[j].size = sg->vmsize - sg->filesize;
			m[j].file_offset = 0;
			m[j].mapping_offset = (sg->vmaddr + sg->filesize) -
					      *seg1addr;
			m[j].protection = sg->initprot | VM_PROT_COW |
					  VM_PROT_ZF;
			j++;
		    }
		}
		lc = (struct load_command *)((char *)lc + lc->cmdsize);
	    }
	    /*
	     * Now call load_shared_file() to map in all the segments.
	     */
	    base_address = *seg1addr;
	    flags = 0;
#ifdef NEW_LOCAL_SHARED_REGIONS
	    if(first_load_shared_file == TRUE &&
	       dyld_new_local_shared_regions == TRUE)
		flags |= NEW_LOCAL_SHARED_REGIONS;
#endif /* NEW_LOCAL_SHARED_REGIONS */
	    ret = load_shared_file(name, (caddr_t)file_addr, file_size,
		(caddr_t *)&base_address, nsegs, m, &flags);
	    if(ret == -1){
		flags |= ALTERNATE_LOAD_SITE;
		ret = load_shared_file(name, (caddr_t)file_addr, file_size,
		    (caddr_t *)&base_address, nsegs, m, &flags);
	    }
	    first_load_shared_file = FALSE;
	    if(ret == -1){
		errnum = errno;
		system_error(errnum, "load_shared_file() failed for %s ", name);
		link_edit_error(DYLD_UNIX_RESOURCE, errnum, name);
		if(sf_mapping_pointer != NULL)
		    free(sf_mapping_pointer);
		goto map_image_cleanup1;
	    }
	    /* determine the slide value from the returned base_address */
	    *slide_value = base_address - *seg1addr;

	    if(sf_mapping_pointer != NULL)
		free(sf_mapping_pointer);

	    goto cleanup_and_reset_pointers;
	}
#else /* !defined(SHARED_LIBRARY_SERVER_SUPPORTED) */
	if((*mh)->flags & MH_SPLIT_SEGS){
	    error("unsupported Mach-O format file: %s (MH_SPILT_SEGS format "
		  "not supported in this version of dyld)", name);
	    link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
	    return(FALSE);
	}
#endif /* SHARED_LIBRARY_SERVER_SUPPORTED */

	/*
	 * We want this image at low_addr to high_addr so see if those
	 * vmaddresses are available to map in the image or will it have to be
	 * slid to an available address.  If the memory we have is from a
	 * mapped file and is the only thing in the way then we'll move it.
	 */
	slide_it = FALSE;
	in_the_way = FALSE;
	address = low_addr;
#ifdef __MACH30__
	infoCnt = VM_REGION_BASIC_INFO_COUNT;
	r = vm_region(mach_task_self(), &address, &size, VM_REGION_BASIC_INFO, 
		      info, &infoCnt, &object_name);
#else
	r = vm_region(mach_task_self(), &address, &size, &protection,
		      &max_protection, &inheritance, &shared, &object_name,
		      &offset);
#endif
	/*
	 * If the return value is KERN_SUCCESS we found a vm_region at covers
	 * the low_addr or is above the low_addr.
	 */
	if(r == KERN_SUCCESS){
	    /*
	     * If the address of the region found is less than the high address
	     * needed for the library this region is where the library wants to
	     * be.
	     */ 
	    if(address < high_addr){
		/*
		 * If we have a file mapped see if the region is the memory
		 * we have the file mapped.
		 */
		if(fd != -1 && address == (vm_offset_t)file_addr){
		    /*
		     * This region is for the memory we have the file mapped so
		     * look for the next region after this one to see if it
		     * covers part of the wanted vmaddresses for the image.
		     */
		    in_the_way = TRUE;
		    address = (vm_offset_t)(file_addr + file_size);
#ifdef __MACH30__
		    infoCnt = VM_REGION_BASIC_INFO_COUNT;
		    r = vm_region(mach_task_self(), &address, &size,
				  VM_REGION_BASIC_INFO, info, &infoCnt,
				  &object_name);
#else
		    r = vm_region(mach_task_self(), &address, &size, &protection,
				  &max_protection, &inheritance, &shared,
				  &object_name, &offset);
#endif
		    /*
		     * If we find a region and its address is less than the
		     * high address wanted for the image the image will
		     * have to be slid.
		     */
		    if(r == KERN_SUCCESS && address < high_addr)
			slide_it = TRUE;
		}
		/*
		 * There is some memory other than the memory we have the file
		 * mapped at the address we want for the image so the image
		 * will have to be slid.
		 */
		else{
		    slide_it = TRUE;
		}
	    }
	}

	/*
	 * Now that we know if we will have to slide the image or not, allocate
	 * the memory that will be used for the image's segments.
	 */
	if(slide_it == FALSE){
	    /*
	     * We don't have to slide the image but the map_fd memory for the
	     * file may be in the way and have to be moved so we can allocate
	     * the library where we want it.
	     */
	    *slide_value = 0;
	    if(in_the_way == TRUE){
		if((r = vm_deallocate(mach_task_self(), (vm_address_t)file_addr,
		    (vm_size_t)file_size)) != KERN_SUCCESS){
		    mach_error(r, "can't vm_deallocate map_fd memory for "
			"%s: %s", image_type, name);
		    link_edit_error(DYLD_MACH_RESOURCE, r, name);
		    goto map_image_cleanup2;
		}
	    }
	    if((r = vm_allocate(mach_task_self(), (vm_address_t *)&low_addr,
				high_addr - low_addr, FALSE)) != KERN_SUCCESS){
		slide_it = TRUE;
	    }
	    if(in_the_way == TRUE){
		if((r = map_fd((int)fd, 0, (vm_offset_t *)&file_addr,
		    (boolean_t)TRUE, (vm_size_t)file_size)) != KERN_SUCCESS){
		    mach_error(r, "can't map %s: %s", image_type, name);
		    link_edit_error(DYLD_MACH_RESOURCE, r, name);
		    if(slide_it == FALSE){
			if((r = vm_deallocate(mach_task_self(), (vm_address_t)
			    low_addr, (vm_size_t)(high_addr - low_addr))) !=
			    KERN_SUCCESS){
			    mach_error(r, "can't vm_deallocate memory to load "
				"in %s: %s", image_type, name);
			    link_edit_error(DYLD_MACH_RESOURCE, r, name);
			}
		    }
		    goto map_image_cleanup2;
		}
	    }
	}
	if(slide_it == TRUE){
	    if((r = vm_allocate(mach_task_self(), &address,high_addr - low_addr,
				TRUE)) != KERN_SUCCESS){
		mach_error(r, "can't vm_allocate memory to load in %s: %s",
		    image_type, name);
		link_edit_error(DYLD_MACH_RESOURCE_RECOVERABLE, r, name);
		goto map_image_cleanup1;
	    }
	    *slide_value = address - low_addr;
	}

	/*
	 * Now that we have the memory allocated for the segments of the image
	 * map or vm_copy the parts of the segments from the file or memory
	 * into the memory for the segments.
	 */
	load_commands = (struct load_command *)((char *)*mh +
						sizeof(struct mach_header));
	lc = load_commands;
	for(i = 0; i < (*mh)->ncmds; i++){
	    switch(lc->cmd){
	    case LC_SEGMENT:
		sg = (struct segment_command *)lc;
		address = sg->vmaddr + *slide_value;
		if(fd != -1){
		    if((r = map_fd((int)fd, (vm_offset_t)sg->fileoff +
			library_offset, &address, FALSE,
			(vm_size_t)sg->filesize)) != KERN_SUCCESS){
			mach_error(r, "can't map segment: %.16s for %s: %s",
			    sg->segname, image_type, name);
			link_edit_error(DYLD_MACH_RESOURCE_RECOVERABLE, r,name);
			goto map_image_cleanup0;
		    }
		}
		else{
		    image_addr = (vm_address_t)*mh;
		    if((r = vm_copy(mach_task_self(), image_addr + sg->fileoff,
			round(sg->filesize, vm_page_size), address)) !=
			KERN_SUCCESS){
			mach_error(r, "can't vm_copy segment: %.16s for %s: %s",
			    sg->segname, image_type, name);
			link_edit_error(DYLD_MACH_RESOURCE_RECOVERABLE, r,name);
			goto map_image_cleanup0;
		    }
		}
		if(sg->fileoff == 0)
		    mach_header_segment_vmaddr = sg->vmaddr;
		break;
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}

#ifdef SHARED_LIBRARY_SERVER_SUPPORTED
cleanup_and_reset_pointers:
#endif
	/*
	 * Cleanup the map_fd memory for the image file and close the file
	 * descriptor if we have one.
	 */
	if(fd != -1){
	    if((r = vm_deallocate(mach_task_self(), (vm_address_t)file_addr,
		(vm_size_t)file_size)) != KERN_SUCCESS){
		mach_error(r, "can't vm_deallocate map_fd memory for %s: %s",
		    image_type, name);
		link_edit_error(DYLD_MACH_RESOURCE, r, name);
	    }
	    if(close(fd) == -1){
		errnum = errno;
		system_error(errnum, "can't close file descriptor for %s: %s ",
		     image_type, name);
		link_edit_error(DYLD_UNIX_RESOURCE, errnum, name);
	    }
	}

	/*
	 * Reset the pointers to the mach_header, linkedit_segment, symbol
	 * table command and  dynamic symbol table command to the memory that
	 * mapped in the segments.
	 *
	 * Also determine the first segment address, if relocation entries are
	 * in read-only segments and if there are relocation entries for
	 * instructions.
	 */
	*mh = (struct mach_header *)((char *)mach_header_segment_vmaddr +
				     *slide_value);
	load_commands = (struct load_command *)((char *)*mh +
						sizeof(struct mach_header));
	lc = load_commands;
	*st = NULL;
	*dyst = NULL;
	*linkedit_segment = NULL;
	if(dlid != NULL)
	    *dlid = NULL;
	if(rc != NULL)
	    *rc = NULL;
	*seg1addr = ULONG_MAX;
	*segs_read_write_addr = ULONG_MAX;
	*change_protect_on_reloc = FALSE;
	*cache_sync_on_reloc = FALSE;
	*has_coalesced_sections = FALSE;
	*init = NULL;
	*term = NULL;
	for(i = 0; i < (*mh)->ncmds; i++){
	    switch(lc->cmd){
	    case LC_SEGMENT:
		sg = (struct segment_command *)lc;
/*
printf("name = %s segname = %s sg = 0x%x\n", name, sg->segname, sg);
*/
		if(strcmp(sg->segname, SEG_LINKEDIT) == 0){
		    if(*linkedit_segment == NULL)
			*linkedit_segment = sg;
		}
		/* pickup the address of the first segment */
		if(sg->vmaddr < *seg1addr)
		    *seg1addr = sg->vmaddr;
		/*
		 * Pickup the address of the first read-write segment for
		 * MH_SPLIT_SEGS images.
		 */
		if(((*mh)->flags & MH_SPLIT_SEGS) == MH_SPLIT_SEGS &&
		   (sg->initprot & VM_PROT_WRITE) == VM_PROT_WRITE &&
		   sg->vmaddr < *segs_read_write_addr)
		    *segs_read_write_addr = sg->vmaddr;
		/*
		 * Stuff the address of the stub_binding_helper_interface into
		 * the first 4 bytes of the (__DATA,__dyld) section if there is
		 * one.  And stuff the address of _dyld_func_lookup in the
		 * second 4 bytes of the (__DATA,__dyld) section.  And stuff the
		 * address of start_debug_thread in the third 4 bytes of the
		 * (__DATA,__dyld) section.
		 */
		s = (struct section *)
		    ((char *)sg + sizeof(struct segment_command));
		for(j = 0; j < sg->nsects; j++){
		    if(strcmp(s->segname, "__DATA") == 0 &&
		       strcmp(s->sectname, "__dyld") == 0){
			if(s->size >= sizeof(unsigned long)){
			    *((long *)(s->addr + *slide_value)) =
				(long)&stub_binding_helper_interface;
			}
			if(s->size >= 2 * sizeof(unsigned long)){
			    *((long *)(s->addr + *slide_value + 4)) =
				(long)&_dyld_func_lookup;
			}
			if(s->size >= 3 * sizeof(unsigned long)){
			    *((long *)(s->addr + *slide_value + 8)) =
				(long)&start_debug_thread;
			}
#ifdef __ppc__
			if(s->size >= 5 * sizeof(unsigned long)){
			    *images_dyld_stub_binding_helper = 
				*((long *)(s->addr + *slide_value + 20)) +
				*slide_value;
			}
#endif
		    }
		    /*
		     * If we are doing profiling call monaddition() for the
		     * sections that have instructions.
		     */
		    if(dyld_monaddition != NULL){
			/* TODO this should be based on SOME_INSTRUCTIONS */
			if(strcmp(s->segname, SEG_TEXT) == 0 &&
			   strcmp(s->sectname, SECT_TEXT) == 0){
			    if(s->size != 0){
				release_lock();
				dyld_monaddition(
				    (char *)(s->addr + *slide_value),
				    (char *)(s->addr + *slide_value + s->size));
				set_lock();
			    }
			}
		    }
#ifndef __MACH30__
		    else{
			if(profile_server == TRUE &&
			   strcmp(s->segname, SEG_TEXT) == 0 &&
			   strcmp(s->sectname, SECT_TEXT) == 0)
			    shared_pcsample_buffer(name, s, *slide_value);
		    }
#endif /* __MACH30__ */
		    s++;
		}
		/*
		 * If this segment is not to have write protection then check to
		 * see if any of the sections have external relocations and if
		 * so mark the image as needing to change protections when doing
		 * relocation in it.
		 */
		if((sg->initprot & VM_PROT_WRITE) == 0){
		    s = (struct section *)
			((char *)sg + sizeof(struct segment_command));
		    for(j = 0; j < sg->nsects; j++){
			if((s->flags & S_ATTR_EXT_RELOC)){
			    *change_protect_on_reloc = TRUE;
			    break;
			}
			s++;
		    }
		}
		/*
		 * If the image has relocations for instructions then the
		 * instruction cache needs to be synchronized with the date
		 * cache on relocation.  A good guess is made based on the
		 * section attributes and section name.
		 */
		s = (struct section *)
		    ((char *)sg + sizeof(struct segment_command));
		for(j = 0; j < sg->nsects; j++){
		    if(((strcmp(s->segname, "__TEXT") == 0 &&
		         strcmp(s->sectname, "__text") == 0) ||
			 (s->flags & S_ATTR_SOME_INSTRUCTIONS)) &&
			 (s->flags & S_ATTR_EXT_RELOC)){
			*cache_sync_on_reloc = TRUE;
			break;
		    }
		    s++;
		}
		/*
		 * If the image has a module init section pick it up.
		 */
		if(*init == NULL){
		    s = (struct section *)
			((char *)sg + sizeof(struct segment_command));
		    for(j = 0; j < sg->nsects; j++){
			if((s->flags & SECTION_TYPE) ==
			   S_MOD_INIT_FUNC_POINTERS){
			    *init = s;
			    break;
			}
			s++;
		    }
		}
		/*
		 * If the image has a module term section pick it up.
		 */
		if(*term == NULL){
		    s = (struct section *)
			((char *)sg + sizeof(struct segment_command));
		    for(j = 0; j < sg->nsects; j++){
			if((s->flags & SECTION_TYPE) ==
			   S_MOD_TERM_FUNC_POINTERS){
			    *term = s;
			    break;
			}
			s++;
		    }
		}
		/*
		 * If the image has any coalesced sections note that
		 */
		if(*has_coalesced_sections == FALSE){
		    s = (struct section *)
			((char *)sg + sizeof(struct segment_command));
		    for(j = 0; j < sg->nsects; j++){
			if((s->flags & SECTION_TYPE) == S_COALESCED){
			    *has_coalesced_sections = TRUE;
			    break;
			}
			s++;
		    }
		}
		break;
	    case LC_SYMTAB:
		if(*st == NULL)
		    *st = (struct symtab_command *)lc;
		break;
	    case LC_DYSYMTAB:
		if(*dyst == NULL)
		    *dyst = (struct dysymtab_command *)lc;
		break;
	    case LC_ID_DYLIB:
		if(dlid != NULL && *dlid == NULL)
		    *dlid = (struct dylib_command *)lc;
		break;
	    case LC_ROUTINES:
		if(rc != NULL && *rc == NULL)
		    *rc = (struct routines_command *)lc;
		break;
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
/*
if(*linkedit_segment != NULL)
printf("name = %s *linkedit_segment = 0x%x\n", name, *linkedit_segment);
*/
	return(TRUE);

map_image_cleanup0:
	if((r = vm_deallocate(mach_task_self(), (vm_address_t)low_addr,
	    (vm_size_t)(high_addr - low_addr))) != KERN_SUCCESS){
	    mach_error(r, "can't vm_deallocate memory to load in "
		"%s: %s", image_type, name);
	    link_edit_error(DYLD_MACH_RESOURCE, r, name);
	}
map_image_cleanup1:
	if(fd != -1){
	    if((r = vm_deallocate(mach_task_self(), (vm_address_t)file_addr,
		(vm_size_t)file_size)) != KERN_SUCCESS){
		mach_error(r, "can't vm_deallocate map_fd memory for %s: %s",
		    image_type, name);
		link_edit_error(DYLD_MACH_RESOURCE, r, name);
	    }
	}
map_image_cleanup2:
	if(fd != -1){
	    if(close(fd) == -1){
		errnum = errno;
		system_error(errnum, "can't close file descriptor for %s: %s ",
		     image_type, name);
		link_edit_error(DYLD_UNIX_RESOURCE, errnum, name);
	    }
	}
	return(FALSE);
}

#ifndef __MACH30__
/*
 * shared_pcsample_buffer() is called with a name of a dynamic library, a
 * section pointer and the slide_value of the library.  If their is a shared
 * pcsample buffer for this library then the buffer file is mapped in shared
 * and a profil(2) call is made.
 */
void
shared_pcsample_buffer(
char *name,
struct section *s,
unsigned long slide_value)
{
    struct stat stat_buf;
    unsigned long size, expected_size;
    kern_return_t r;
    char *buf, *rbuf;
    int fd;
    char gmon_out[MAXPATHLEN];
    static enum bool first_time = TRUE;
#ifdef __OPENSTEP__
    struct phdr profile_header;
#else
    struct gmonhdr profile_header;
#endif

	/*
	 * Contact the server and see if for the shared library "name" we have
	 * a pcsample buffer file.
	 */
	if(dyld_sample_debug == 2)
	    print("calling buffer_for_dylib for: %s\n", name);
	if(buffer_for_dylib(name, gmon_out) == FALSE){
	    if(dyld_sample_debug == 2)
		print("buffer_for_dylib for: %s returned FALSE\n", name);
	    return;
	}
	if(dyld_sample_debug == 2)
	    print("buffer_for_dylib for: %s returned: %s\n", name, gmon_out);
	fd = open(gmon_out, O_RDWR, 0);
	if(fd == -1){
	    if(dyld_sample_debug)
		print("can't open: %s for: %s\n", gmon_out, name);
	    return;
	}
	if(fstat(fd, &stat_buf) == -1){
	    if(dyld_sample_debug)
		print("can't stat: %s for: %s\n", gmon_out, name);
	    (void)close(fd);
	    return;
	}
	size = stat_buf.st_size;
	/*
	 * The size of the pcsample buffer file should be exactly the right
	 * size for SCALE_1_TO_1 mapping.
	 */
	expected_size = round(s->size / 1, sizeof(unsigned short)) +
		        sizeof(profile_header);
	if(size != expected_size){
	    if(dyld_sample_debug)
		print("size of: %s for: %s is %ld, expected %ld\n", gmon_out,
		       name, size, expected_size);
	    (void)close(fd);
	    return;
	}
#ifndef MWATSON
	r = vm_allocate(mach_task_self(), (vm_address_t *)&buf, size, TRUE);
	if(r != KERN_SUCCESS){
	    if(dyld_sample_debug)
		print("can't vm_allocate buffer to map: %s for: %s\n",
		       gmon_out, name);
	    (void)close(fd);
	    return;
	}
	rbuf = (char *)mmap(buf, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
#else
	rbuf = (char *)mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
#endif
#ifndef __OPENSTEP__
	if(rbuf == NULL)
#else
	if((int)rbuf == -1)
#endif
	{
	    if(dyld_sample_debug)
		print("can't mmap: %s for: %s\n", gmon_out, name);
	    goto cleanup;
	}
	(void)close(fd);

	if(first_time == TRUE){
	    if(profil(rbuf + sizeof(profile_header),
		      size - sizeof(profile_header),
		      (int)s->addr + slide_value,
		     SCALE_1_TO_1) == -1){
		if(dyld_sample_debug)
		    print("profil failed: %s for: %s\n", gmon_out, name);
		goto cleanup;
	    }
	    first_time = FALSE;
	}
	else{
	    if(add_profil(rbuf + sizeof(profile_header),
			  size - sizeof(profile_header),
			 (int)s->addr + slide_value,
			 SCALE_1_TO_1) == -1){
		if(dyld_sample_debug)
		    print("profil failed: %s for: %s\n", gmon_out, name);
		goto cleanup;
	    }
	}
	if(dyld_sample_debug == 2)
	    print("successfully set up: %s for: %s\n", gmon_out, name);
	return;

cleanup:
	(void)close(fd);
#ifndef MWATSON
	r = vm_deallocate(mach_task_self(), (vm_address_t)buf, (vm_size_t)size);
	if(r != KERN_SUCCESS){
	    mach_error(r, "can't vm_deallocate shared pcsample buffer "
		" memory for %s", name);
	    link_edit_error(DYLD_MACH_RESOURCE, r, name);
	}
#endif
	return;
}
#endif /* __MACH30__ */

static
void
set_segment_protections(
char *name,
char *image_type,
struct mach_header *mh,
unsigned long slide_value)
{
    unsigned long i;
    struct load_command *lc, *load_commands;
    struct segment_command *sg;
    vm_address_t address;
    kern_return_t r;

	/*
	 * For images with split segments the load_shared_file() call sets this
	 * up.  If a vm_protect() is done on these segments it will fail so
	 * we must not do it here.
	 */
	if(mh->flags & MH_SPLIT_SEGS)
	    return;

	/*
	 * Set the initial protection of the segments.  The maximum protection
	 * is not set in case there is relocation to be done later.
	 */
	load_commands = (struct load_command *)((char *)mh +
						sizeof(struct mach_header));
	lc = load_commands;
	for(i = 0; i < mh->ncmds; i++){
	    switch(lc->cmd){
	    case LC_SEGMENT:
		sg = (struct segment_command *)lc;
		address = sg->vmaddr + slide_value;
		if((r = vm_protect(mach_task_self(), address,
				   (vm_size_t)sg->vmsize,
				   FALSE, sg->initprot)) != KERN_SUCCESS){
		    mach_error(r, "can't vm_protect segment: %.16s for %s:"
			" %s", sg->segname, image_type, name);
		    link_edit_error(DYLD_MACH_RESOURCE, r, name);
		}
		break;
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
}

/*
 * load_dependent_libraries() loads the dependent libraries for libraries that
 * have not had their dependent libraries loaded.  This is done this way to get
 * the proper order of libraries.  The proper order is to have all the libraries
 * in the executable before any of the dependent libraries.  This allows the
 * executable to over ride a dependent library of a library it uses.
 *
 * The return value is only used when return_on_error is TRUE.  In this case 
 * a return value of TRUE indicates success and a return value of FALSE
 * indicates failure and that all change have been backed out.
 */
enum bool
load_dependent_libraries(
void)
{
    unsigned long i;
    struct library_images *q;

	q = &library_images;
	do{
	    for(i = 0; i < q->nimages; i++){
		if(q->images[i].dependent_libraries_loaded == FALSE){
		    if(dyld_print_libraries == TRUE)
			print("loading libraries for image: %s\n",
			       q->images[i].image.name);
		    if(load_images_libraries(q->images[i].image.mh) == FALSE &&
		       return_on_error == TRUE)
			return(FALSE);
		    q->images[i].dependent_libraries_loaded = TRUE;
		}
	    }
	    q = q->next_images;
	}while(q != NULL);

	return(TRUE);
}

/*
 * load_images_libraries() loads the library image's for the specified mach
 * header.  It does not in turn load any libraries these libraries depend on.
 * If we are doing return_on_error then any libraries loaded are then unloaded
 * and FALSE is returned.  Else TRUE is returned.
 */
static
enum bool
load_images_libraries(
struct mach_header *mh)
{
    unsigned long i;
    struct load_command *lc, *load_commands;
    struct dylib_command *dl_load;

	/*
	 * Load each of the libraries this image uses.
	 */
	load_commands = (struct load_command *)((char *)mh +
						sizeof(struct mach_header));
	lc = load_commands;
	for(i = 0; i < mh->ncmds; i++){
	    switch(lc->cmd){
	    case LC_LOAD_DYLIB:
		dl_load = (struct dylib_command *)lc;
		if(load_library_image(dl_load, NULL, FALSE) == FALSE &&
		   return_on_error == TRUE){
		    unload_remove_on_error_libraries();
		    return(FALSE);
		}
		break;
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
	return(TRUE);
}

/*
 * unload_remove_on_error_libraries() unloads any libraries that were loaded
 * while return_on_error was set.
 */
void
unload_remove_on_error_libraries(
void)
{
    struct library_images *p, *q, *temp;
    struct library_image *library_image;
    unsigned long i, j;
    kern_return_t r;
    enum bool split_lib;
    struct dyld_event event;

	for(p = &library_images; p != NULL; p = p->next_images){
	    for(i = 0; i < p->nimages; i++){
		/*
		 * Find the first library image that is marked as
		 * remove_on_error and then remove it and all library images
		 * after it.
		 */
		if(p->images[i].remove_on_error == TRUE){
		    /*
		     * For this library image and all later library images in
		     * this block remove them.  Then clear out the image
		     * structure as it will be reused.
		     */
		    for(j = i; j < p->nimages; j++){
			library_image = p->images + j;
			library_image->image.valid = FALSE;
			/*
			 * Send the event message that the images is being
			 * removed.
			 */
			memset(&event, '\0', sizeof(struct dyld_event));
			event.type = DYLD_IMAGE_REMOVED;
			event.arg[0].header = library_image->image.mh;
			event.arg[0].vmaddr_slide =
				library_image->image.vmaddr_slide;
			event.arg[0].module_index = 0;
			send_event(&event);

			split_lib = (library_image->image.mh->flags &
				     MH_SPLIT_SEGS) == MH_SPLIT_SEGS;
			if(split_lib == TRUE){
			    unload_shared_file(library_image);
			}
			else if((r = vm_deallocate(mach_task_self(),
				(vm_address_t)library_image->image.mh,
				(vm_size_t)library_image->image.vmaddr_size)) !=
				KERN_SUCCESS){
			    mach_error(r, "can't vm_deallocate memory for "
				       "library: %s",library_image->image.name);
			    link_edit_error(DYLD_MACH_RESOURCE, r,
					    library_image->image.name);
			}
			deallocate_module_states(library_image->modules,
						 library_image->nmodules);
			/* zero out the image as it will be reused */
			memset(library_image,'0',sizeof(struct library_image));
		    }
		    /* reset the number of images in this block */
		    p->nimages = i;

		    /*
		     * Remove all library images in any next blocks.
		     */
		    q = p->next_images;
		    /* clear the pointer to the next blocks */
		    p->next_images = NULL;
		    while(q != NULL){
			for(j = 0; j < q->nimages; j++){
			    library_image = q->images + j;
			    library_image->image.valid = FALSE;
			    /*
			     * Send the event message that the images is being
			     * removed.
			     */
			    memset(&event, '\0', sizeof(struct dyld_event));
			    event.type = DYLD_IMAGE_REMOVED;
			    event.arg[0].header = library_image->image.mh;
			    event.arg[0].vmaddr_slide =
				    library_image->image.vmaddr_slide;
			    event.arg[0].module_index = 0;
			    send_event(&event);

			    split_lib = (library_image->image.mh->flags &
					 MH_SPLIT_SEGS) == MH_SPLIT_SEGS;
			    if(split_lib == TRUE){
				unload_shared_file(library_image);
			    }
			    else if((r = vm_deallocate(mach_task_self(),
				(vm_address_t)library_image->image.mh,
				(vm_size_t)library_image->image.vmaddr_size)) !=
				KERN_SUCCESS){
				mach_error(r, "can't vm_deallocate memory for "
				      "library: %s", library_image->image.name);
				link_edit_error(DYLD_MACH_RESOURCE, r,
						library_image->image.name);
			    }
			    deallocate_module_states(library_image->modules,
						     library_image->nmodules);
			}
			temp = q->next_images;
			free(q);
			q = temp;
		    }
		    return;
		}
	    }
	}
}

/*
 * unload_shared_file() is called to get rid of a split shared library.
 */
static
void
unload_shared_file(
struct library_image *library_image)
{
#ifdef SHARED_LIBRARY_SERVER_SUPPORTED
    unsigned long i, j;
    struct load_command *lc, *load_commands;
    struct segment_command *sg;
    unsigned long nsegs;
    int ret;
#define ARRAY_ENTRIES 5
    struct sf_mapping sf_mapping_array[ARRAY_ENTRIES], *sf_mapping_pointer, *m;
    vm_address_t base_address;

	/*
	 * For MH_SPLIT_SEGS images they were mapped using the
	 * load_shared_file() call not mupliple map_fd() calls so we can't
	 * use vm_deallocte().  We would like to have an unload_shared_file()
	 * call but all the kernel gives us is reset_shared_file().
	 */
	/* count the number of segments */
	nsegs = 0;
	load_commands = (struct load_command *)
			    ((char *)(library_image->image.mh) +
			    sizeof(struct mach_header));
	lc = load_commands;
	for(i = 0; i < library_image->image.mh->ncmds; i++){
	    switch(lc->cmd){
	    case LC_SEGMENT:
		sg = (struct segment_command *)lc;
		nsegs++;
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
	/* set up to use or allocate a set of sf_mapping structs */
	if(nsegs <= ARRAY_ENTRIES){
	    sf_mapping_pointer = NULL;
	    m = sf_mapping_array;
	}
	else{
	    sf_mapping_pointer = allocate(sizeof(struct sf_mapping) *
					  nsegs);
	    m = sf_mapping_pointer;
	}
	/*
	 * Fill in the sf_mapping structs for each of the segments from the
	 * file.
	 */
	j = 0;
	lc = load_commands;
	for(i = 0; i < library_image->image.mh->ncmds; i++){
	    switch(lc->cmd){
	    case LC_SEGMENT:
		sg = (struct segment_command *)lc;
		m[j].cksum = 0;
		m[j].size = sg->filesize;
		m[j].file_offset = sg->fileoff + library_image->library_offset;
		m[j].mapping_offset = sg->vmaddr - 
			library_image->image.seg1addr;
		if((sg->initprot & VM_PROT_WRITE) == VM_PROT_WRITE)
		    m[j].protection = sg->initprot | VM_PROT_COW;
		else
		    m[j].protection = sg->initprot;
		j++;
		/* if this segment has a zero-fill area create a mapping */
		if((sg->initprot & VM_PROT_WRITE) == VM_PROT_WRITE &&
		   sg->vmsize > sg->filesize){
		    m[j].size = sg->vmsize - sg->filesize;
		    m[j].file_offset = 0;
		    m[j].mapping_offset = (sg->vmaddr + sg->filesize) -
			library_image->image.seg1addr;
		    m[j].protection = sg->initprot | VM_PROT_COW |
				      VM_PROT_ZF;
		    j++;
		}
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
	/*
	 * Now call reset_shared_file() to reset all the segments.
	 */
	base_address = library_image->image.seg1addr +
		       library_image->image.vmaddr_slide;
	ret = reset_shared_file((caddr_t *)&base_address, nsegs, m);
	if(ret == -1){
	    system_error(errno, "reset_shared_file() failed for %s ",
		library_image->image.name);
	    link_edit_error(DYLD_UNIX_RESOURCE, errno,
		library_image->image.name);
	}
	if(sf_mapping_pointer != NULL)
	    free(sf_mapping_pointer);
#endif /* SHARED_LIBRARY_SERVER_SUPPORTED */
}

/*
 * clear_remove_on_error_libraries() is called after a successful
 * _dyld_link_module() call with the LINK_OPTION_RETURN_ON_ERROR option.
 */
void
clear_remove_on_error_libraries(
void)
{
    unsigned long i;
    struct library_images *p;

	for(p = &library_images; p != NULL; p = p->next_images){
	    for(i = 0; i < p->nimages; i++){
		p->images[i].remove_on_error = FALSE;
	    }
	}
}

/*
 * check_linkedit_info() checks the mach_header and load_commands of an image.
 * The image is assumed to be mapped into memory at the mach_header pointer for
 * a sizeof image_size.  The strings name and image_type are used for error
 * messages.  TRUE is returned if everything is ok else FALSE is returned after 
 * link_edit_error() is called.  A bunch of things from the Mach-O image are
 * returned.
 */
static
enum bool
check_image(
    /* inputs */
char *name,
char *image_type,
unsigned long image_size,
struct mach_header *mh,
    /* outputs */
struct segment_command **linkedit_segment,
struct segment_command **mach_header_segment,
struct dysymtab_command **dyst,
struct symtab_command **st,
struct dylib_command **dlid,
struct routines_command **rc,
unsigned long *low_addr,
unsigned long *high_addr)
{
    unsigned long i, j;
    struct load_command *lc, *load_commands;
    struct segment_command *sg;
    struct dylib_command *dl_load;
    char *load_dylib_name;

	*linkedit_segment = NULL;
	*mach_header_segment = NULL;
	*st = NULL;
	*dyst = NULL;
	if(rc != NULL)
	    *rc = NULL;
	if(dlid != NULL)
	    *dlid = NULL;
	*low_addr = ULONG_MAX;
	*high_addr = 0;

	/*
	 * Do the needed remaining checks on the mach header.  The caller needs
	 * to check the filetype and if it is a library to check the dlid passed
	 * back for compatibility.
	 */
	if(mh->cputype != host_basic_info.cpu_type){
	    error("bad CPU type in %s: %s", image_type, name);
	    link_edit_error(DYLD_FILE_FORMAT, EBADARCH, name);
	    return(FALSE);
	}
	if(cpusubtype_combine(host_basic_info.cpu_type,
			  host_basic_info.cpu_subtype, mh->cpusubtype) == -1){
	    error("bad CPU subtype in %s: %s", image_type, name);
	    link_edit_error(DYLD_FILE_FORMAT, EBADARCH, name);
	    return(FALSE);
	}
	if(mh->sizeofcmds + sizeof(struct mach_header) > image_size){
	    error("truncated or malformed %s: %s (load commands extend "
		"past the end of the image)", image_type, name);
	    link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
	    return(FALSE);
	}

	/*
	 * Cycle through the load commands doing the minimum checks for the
	 * things we need.  Checks for duplicate things are not done and just
	 * the first one found is used and checked.
	 *
	 * Pick up the linkedit segment, the segment mapping the mach header,
	 * the symbol table command, the dynamic symbol command and the dynamic
	 * library identification command from the image.  We don't check for
	 * the error of having more than one of these but just pick up the
	 * first one if any. 
	 *
	 * Determined the lowest and highest address needed to cover the segment
	 * commands.  These images are suppose to be contiguious but that is not
	 * checked here.  If it isn't it might fail to load because the spread
	 * covers more address space than we can allocate.
	 */
	load_commands = (struct load_command *)((char *)mh +
						sizeof(struct mach_header));
	lc = load_commands;
	for(i = 0; i < mh->ncmds; i++){
	    if(lc->cmdsize % sizeof(long) != 0){
		error("truncated or malformed %s: %s (load command %lu "
		    "size not a multiple of sizeof(long))",image_type, name, i);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		return(FALSE);
	    }
	    if(lc->cmdsize == 0){
		error("truncated or malformed %s: %s (load command %lu "
		    "size is equal to zero)", image_type, name, i);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		return(FALSE);
	    }
	    if((char *)lc + lc->cmdsize >
	       (char *)load_commands + mh->sizeofcmds){
		error("truncated or malformed %s: %s (load command %lu "
		    "extends past end of all load commands)", image_type,
		    name, i);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		return(FALSE);
	    }
	    switch(lc->cmd){
	    case LC_SEGMENT:
		sg = (struct segment_command *)lc;
		if(sg->fileoff > image_size){
		    error("truncated or malformed %s: %s (load command "
			"%lu fileoff extends past end of the library)",
			image_type, name, i);
		    link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		    return(FALSE);
		}
		if(sg->fileoff + sg->filesize > image_size){
		    error("truncated or malformed %s: %s (load command "
			"%lu fileoff plus filesize extends past end of the "
			"library)", image_type, name, i);
		    link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		    return(FALSE);
		}
		if(sg->cmdsize != sizeof(struct segment_command) +
				     sg->nsects * sizeof(struct section)){
		    error("malformed %s: %s (cmdsize field of load "
			"command %lu is inconsistant for a segment command "
			"with the number of sections it has)", image_type,
			name, i);
		    link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		    return(FALSE);
		}
		if(strcmp(sg->segname, SEG_LINKEDIT) == 0){
		    if(*linkedit_segment == NULL)
			*linkedit_segment = sg;
		}
		if(sg->fileoff == 0)
		    *mach_header_segment = sg;
		if(sg->vmaddr < *low_addr)
		    *low_addr = sg->vmaddr;
		if(sg->vmaddr + sg->vmsize > *high_addr)
		    *high_addr = sg->vmaddr + sg->vmsize;
		break;

	    case LC_DYSYMTAB:
		if(*dyst == NULL)
		    *dyst = (struct dysymtab_command *)lc;
		break;

	    case LC_SYMTAB:
		if(*st == NULL)
		    *st = (struct symtab_command *)lc;
		break;

	    case LC_ROUTINES:
		if(*rc == NULL)
		    *rc = (struct routines_command *)lc;
		break;

	    case LC_ID_DYLIB:
		if(dlid != NULL && *dlid == NULL){
		    *dlid = (struct dylib_command *)lc;
		    if((*dlid)->cmdsize < sizeof(struct dylib_command)){
			error("truncated or malformed %s: %s (cmdsize of "
			    "load command %lu incorrect for LC_ID_DYLIB)",
			    image_type, name, i);
			link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
			return(FALSE);
		    }
		}
		break;

	    case LC_LOAD_DYLIB:
		dl_load = (struct dylib_command *)lc;
		if(dl_load->cmdsize < sizeof(struct dylib_command)){
		    error("truncated or malformed %s: %s (cmdsize of load "
			"command %lu incorrect for LC_LOAD_DYLIB)", image_type,
			name, i);
		    link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		    return(FALSE);
		}
		if(dl_load->dylib.name.offset >= dl_load->cmdsize){
		    error("truncated or malformed %s: %s (name.offset of "
			"load command %lu extends past the end of the load "
			"command)", image_type, name, i); 
		    link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		    return(FALSE);
		}
		load_dylib_name = (char *)dl_load + dl_load->dylib.name.offset;
		for(j = 0;
		    j < dl_load->cmdsize - dl_load->dylib.name.offset;
		    j++){
		    if(load_dylib_name[j] == '\0')
			break;
		}
		if(j >= dl_load->cmdsize - dl_load->dylib.name.offset){
		    error("truncated or malformed %s: %s (library name of "
			"load command %lu not null terminated)", image_type,
			name, i);
		    link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		    return(FALSE);
		}
		break;
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
	if(*mach_header_segment == NULL){
	    error("malformed %s: %s (no segment command maps the mach_header)",
		image_type, name);
	    link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
	    return(FALSE);
	}
	if(*st == NULL){
	    error("malformed %s: %s (no symbol table command)", image_type,
		name);
	    link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
	    return(FALSE);
	}
	if(*dyst == NULL){
	    error("malformed %s: %s (no dynamic symbol table command)",
		image_type, name);
	    link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
	    return(FALSE);
	}
	if(*linkedit_segment == NULL){
	    error("malformed %s: %s (no " SEG_LINKEDIT "segment)",
		image_type, name);
	    link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
	    return(FALSE);
	}
	if(check_linkedit_info(name, image_type, *linkedit_segment, *st,
			       *dyst, rc == NULL ? NULL : *rc) == FALSE)
	    return(FALSE);

	return(TRUE);
}

/*
 * check_linkedit_info() check to see if the offsets and spans of the linkedit
 * information for the symbol table command and dynamic symbol table command
 * are contained in the link edit segment.  TRUE is returned if everything is
 * ok else FALSE is returned after link_edit_error() is called.  These are the
 * only checks done on the linkedit information.  The individual entries will
 * be assumed to be correct.
 */
static
enum bool
check_linkedit_info(
char *name,
char *image_type,
struct segment_command *linkedit_segment,
struct symtab_command *st,
struct dysymtab_command *dyst,
struct routines_command *rc)
{
	if(st->nsyms != 0){
	    if(st->symoff < linkedit_segment->fileoff ||
	       st->symoff > linkedit_segment->fileoff +
			    linkedit_segment->filesize){
		error("malformed %s: %s (offset to symbol table not in "
		    SEG_LINKEDIT " segment)", image_type, name);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		return(FALSE);
	    }
	    if(st->symoff + st->nsyms * sizeof(struct nlist) <
	       linkedit_segment->fileoff ||
	       st->symoff + st->nsyms * sizeof(struct nlist) >
	       linkedit_segment->fileoff + linkedit_segment->filesize){
		error("malformed %s: %s (symbol table not contained in "
		    SEG_LINKEDIT " segment)", image_type, name);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		return(FALSE);
	    }
	}
	if(st->strsize != 0){
	    if(st->stroff < linkedit_segment->fileoff ||
	       st->stroff > linkedit_segment->fileoff +
			    linkedit_segment->filesize){
		error("malformed %s: %s (offset to string table not in "
		    SEG_LINKEDIT " segment)", image_type, name);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		return(FALSE);
	    }
	    if(st->stroff + st->strsize < linkedit_segment->fileoff ||
	       st->stroff + st->strsize >
	       linkedit_segment->fileoff + linkedit_segment->filesize){
		error("malformed %s: %s (string table not contained in "
		    SEG_LINKEDIT " segment)", image_type, name);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		return(FALSE);
	    }
	}
	if(dyst->nlocalsym != 0){
	    if(dyst->ilocalsym > st->nsyms){
		error("malformed %s: %s (ilocalsym in LC_DYSYMTAB load command "
		    "extends past the end of the symbol table", image_type,
		    name);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		return(FALSE);
	    }
	    if(dyst->ilocalsym + dyst->nlocalsym > st->nsyms){
		error("malformed %s: %s (ilocalsym plus nlocalsym in "
		    "LC_DYSYMTAB load command extends past the end of the "
		    "symbol table", image_type, name);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		return(FALSE);
	    }
	}
	if(dyst->nextdefsym != 0){
	    if(dyst->iextdefsym > st->nsyms){
		error("malformed %s: %s (iextdefsym in LC_DYSYMTAB load "
		    "command extends past the end of the symbol table",
		    image_type, name);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		return(FALSE);
	    }
	    if(dyst->iextdefsym + dyst->nextdefsym > st->nsyms){
		error("malformed %s: %s (iextdefsym plus nextdefsym in "
		    "LC_DYSYMTAB load command extends past the end of the "
		    "symbol table", image_type, name);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		return(FALSE);
	    }
	}
	if(dyst->nundefsym != 0){
	    if(dyst->iundefsym > st->nsyms){
		error("malformed %s: %s (iundefsym in LC_DYSYMTAB load command "
		    "extends past the end of the symbol table", image_type,
		    name);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		return(FALSE);
	    }
	    if(dyst->iundefsym + dyst->nundefsym > st->nsyms){
		error("malformed %s: %s (iundefsym plus nundefsym in "
		    "LC_DYSYMTAB load command extends past the end of the "
		    "symbol table", image_type, name);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		return(FALSE);
	    }
	}
	if(dyst->ntoc != 0){
	    if(dyst->tocoff < linkedit_segment->fileoff ||
	       dyst->tocoff > linkedit_segment->fileoff +
			      linkedit_segment->filesize){
		error("malformed %s: %s (offset to table of contents not in "
		    SEG_LINKEDIT " segment)", image_type, name);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		return(FALSE);
	    }
	    if(dyst->tocoff +
	       dyst->ntoc * sizeof(struct dylib_table_of_contents) <
	       linkedit_segment->fileoff ||
	       dyst->tocoff +
	       dyst->ntoc * sizeof(struct dylib_table_of_contents) >
	       linkedit_segment->fileoff + linkedit_segment->filesize){
		error("malformed %s: %s (table of contents not contained in "
		    SEG_LINKEDIT " segment)", image_type, name);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		return(FALSE);
	    }
	}
	if(dyst->nmodtab != 0){
	    if(dyst->modtaboff < linkedit_segment->fileoff ||
	       dyst->modtaboff > linkedit_segment->fileoff +
			         linkedit_segment->filesize){
		error("malformed %s: %s (offset to module table not in "
		    SEG_LINKEDIT " segment)", image_type, name);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		return(FALSE);
	    }
	    if(dyst->modtaboff + dyst->nmodtab * sizeof(struct dylib_module) <
	       linkedit_segment->fileoff ||
	       dyst->modtaboff + dyst->nmodtab * sizeof(struct dylib_module) >
	       linkedit_segment->fileoff + linkedit_segment->filesize){
		error("malformed %s: %s (module table not contained in "
		    SEG_LINKEDIT " segment)", image_type, name);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		return(FALSE);
	    }
	    if(rc != NULL){
		if(rc->init_module > dyst->nmodtab){
		    error("malformed %s: %s (module table index for "
			"initialization routine not contained in module table)",
			image_type, name);
		    link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		    return(FALSE);
		}
	    }
	}
	if(dyst->nextrefsyms != 0){
	    if(dyst->extrefsymoff < linkedit_segment->fileoff ||
	       dyst->extrefsymoff > linkedit_segment->fileoff +
			            linkedit_segment->filesize){
		error("malformed %s: %s (offset to referenced symbol table not "
		    "in " SEG_LINKEDIT " segment)", image_type, name);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		return(FALSE);
	    }
	    if(dyst->extrefsymoff +
	       dyst->nextrefsyms * sizeof(struct dylib_reference) <
	       linkedit_segment->fileoff ||
	       dyst->extrefsymoff +
	       dyst->nextrefsyms * sizeof(struct dylib_reference) >
	       linkedit_segment->fileoff + linkedit_segment->filesize){
		error("malformed %s: %s (referenced table not contained in "
		    SEG_LINKEDIT " segment)", image_type, name);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		return(FALSE);
	    }
	}
	if(dyst->nindirectsyms != 0){
	    if(dyst->indirectsymoff < linkedit_segment->fileoff ||
	       dyst->indirectsymoff > linkedit_segment->fileoff +
			              linkedit_segment->filesize){
		error("malformed %s: %s (offset to indirect symbol table not "
		    "in " SEG_LINKEDIT " segment)", image_type, name);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		return(FALSE);
	    }
	    if(dyst->indirectsymoff +
	       dyst->nindirectsyms * sizeof(unsigned long) <
	       linkedit_segment->fileoff ||
	       dyst->indirectsymoff +
	       dyst->nindirectsyms * sizeof(unsigned long) >
	       linkedit_segment->fileoff + linkedit_segment->filesize){
		error("malformed %s: %s (indirect symbol table not contained "
		    "in " SEG_LINKEDIT " segment)", image_type, name);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		return(FALSE);
	    }
	}
	if(dyst->nextrel != 0){
	    if(dyst->extreloff < linkedit_segment->fileoff ||
	       dyst->extreloff > linkedit_segment->fileoff +
			         linkedit_segment->filesize){
		error("malformed %s: %s (offset to external relocation entries "
		    "not in " SEG_LINKEDIT " segment)", image_type, name);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		return(FALSE);
	    }
	    if(dyst->extreloff +
	       dyst->nextrel * sizeof(struct relocation_info) <
	       linkedit_segment->fileoff ||
	       dyst->extreloff +
	       dyst->nextrel * sizeof(struct relocation_info) >
	       linkedit_segment->fileoff + linkedit_segment->filesize){
		error("malformed %s: %s (external relocation entries not "
		    "contained in " SEG_LINKEDIT " segment)", image_type, name);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		return(FALSE);
	    }
	}
	if(dyst->nlocrel != 0){
	    if(dyst->locreloff < linkedit_segment->fileoff ||
	       dyst->locreloff > linkedit_segment->fileoff +
			         linkedit_segment->filesize){
		error("malformed %s: %s (offset to local relocation entries "
		    "not in " SEG_LINKEDIT " segment)", image_type, name);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		return(FALSE);
	    }
	    if(dyst->locreloff +
	       dyst->nlocrel * sizeof(struct relocation_info) <
	       linkedit_segment->fileoff ||
	       dyst->locreloff +
	       dyst->nlocrel * sizeof(struct relocation_info) >
	       linkedit_segment->fileoff + linkedit_segment->filesize){
		error("malformed %s: %s (local relocation entries not "
		    "contained in " SEG_LINKEDIT " segment)", image_type, name);
		link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		return(FALSE);
	    }
	}
	return(TRUE);
}

/*
 * save_string() is passed the name of an object file image (or some other
 * string) and returns a pointer to a copy of the name that has been saved
 * away.  The name is saved in either the string table or in some malloc()'ed
 * area.
 */
char *
save_string(
char *string)
{
    unsigned long len;
    char *p;

	len = strlen(string) + 1;
	if(len <= STRING_BLOCK_SIZE - string_block.used){
	    p = string_block.strings + string_block.used;
	    strcpy(p, string);
	    string_block.used += len;
	}
	else{
	    p = allocate(len);
	    strcpy(p, string);
	}
	return(p);
}

#ifdef __OPENSTEP__
extern char *realpath(const char *pathname, char resolvedname[MAXPATHLEN]); 

char *
getcwd(
char *buf,
size_t size)
{
	if(size == 0)
	    return(getwd(allocate(MAXPATHLEN + 1)));
	if(size >= MAXPATHLEN)
	    return(getwd(buf));
	return(NULL);
}
#endif /* __OPENSTEP__ */

/*
 * create_executables_path() is passed the exec_path (the first argument to
 * exec, or argv[0]) and constructs the executable's path and sets the pointer
 * executables_path to the constructed path.  This routine tries to avoid doing
 * a malloc and uses the string block as the memory to construct the path if
 * at all possible.
 */
void
create_executables_path(
char *exec_path)
{
    char *p, *cwd;
    unsigned long n, max, cwd_len, executables_pathlen;

	/* n is the size of the exec_path not including the trailing '\0' */
	n = strlen(exec_path);

	/*
	 * try to used the string block instead of malloc'ing memory.
	 */
	max = STRING_BLOCK_SIZE - string_block.used;
	executables_path = string_block.strings + string_block.used;

	/*
	 * If the exec path name starts with '/' we don't need to prepend the
	 * current working directory.
	 */
	if(exec_path[0] == '/'){
	    executables_pathlen = n + 1;
	    if(executables_pathlen <= max)
		string_block.used += executables_pathlen;
	    else
		executables_path = allocate(executables_pathlen);
	    p = executables_path;
	}
	else{
	    /*
	     * The exec path name does not start with a '/' so prepend the
	     * current working directory, trying to get it into the string
	     * block to avoid a malloc'ing.
	     */
	    if(getcwd(executables_path, max) != NULL){
		cwd_len = strlen(executables_path);
		executables_pathlen = cwd_len + 1 + n + 1;
		if(executables_pathlen <= max){
		    string_block.used += executables_pathlen;
		    p = executables_path + cwd_len;
		}
		else{
		    p = allocate(executables_pathlen);
		    strncpy(p, executables_path, cwd_len);
		    p = p + cwd_len;
		}
	    }
	    else if((cwd = getcwd(NULL, 0)) != NULL){
		cwd_len = strlen(cwd);
		executables_pathlen = cwd_len + 1 + n + 1;
		executables_path = allocate(executables_pathlen);
		p = executables_path;
		strncpy(p, cwd, cwd_len);
		p = p + cwd_len;
		free(cwd);
	    }
	    else{
		/*
		 * This is an error in that we can't get current working
		 * directory, but if the program does not used any libraries
		 * relative to the executable's path then it does not cause
		 * a problem.  So executables_path is tested for NULL when it
		 * is needed.
		 */
		executables_path = NULL;
		if(dyld_executable_path_debug == TRUE)
		    printf("executables_path = NULL (getcwd() failed "
			   "errno = %d)\n", errno);
		return;
	    }
	    *p = '/';
	    p++;
	}
	/* add the exec_path */
	strcpy(p, exec_path);
}

/*
 * new_object_image() allocates an object_image structure on the list of object
 * images and returns a pointer to it.
 */
static
struct object_image *
new_object_image(
void)
{
    struct object_images *p;
    unsigned long i;
    enum link_state link_state;

	for(p = &object_images ; ; p = p->next_images){
	    for(i = 0; i < p->nimages; i++){
		/* If this object file image is currently unused reuse it */
		link_state = GET_LINK_STATE(p->images[i].module);
		if(link_state == UNUSED)
		    return(p->images + i);
	    }
	    if(p->nimages != NOBJECT_IMAGES){
		return(p->images + p->nimages++);
	    }
	    if(p->next_images == NULL)
		break;
	}
	p->next_images = allocate(sizeof(struct object_images));
	memset(p->next_images, '\0',  sizeof(struct object_images));
	return(p->next_images->images + p->next_images->nimages++);
}

/*
 * Is passed a image pointer and returns the object_image pointer that owns
 * that image or NULL.
 */
struct object_image *
find_object_image(
struct image *image)
{
    struct object_images *p;
    unsigned long i;
    enum link_state link_state;

	for(p = &object_images ; ; p = p->next_images){
	    for(i = 0; i < p->nimages; i++){
		link_state = GET_LINK_STATE(p->images[i].module);
		if(link_state == UNUSED)
		    continue;
		if(image == &(p->images[i].image))
		    return(p->images + i);
	    }
	    if(p->next_images == NULL)
		break;
	}
	return(NULL);
}

/*
 * new_library_image() allocates a library_image structure on the list of
 * library images and returns a pointer to it.  It also allocates nmodule
 * structures for the library_image and fills in the pointer to the module
 * structure in the library_image and the count of modules.
 */
static
struct library_image *
new_library_image(
unsigned long nmodules)
{
    struct library_images *p;
    struct library_image *library_image;

	for(p = &library_images ; ; p = p->next_images){
	    if(p->nimages != NLIBRARY_IMAGES){
		library_image = p->images + p->nimages++;
		library_image->nmodules = nmodules;
		library_image->modules = allocate_module_states(nmodules);
		memset(library_image->modules, '\0',
		       sizeof(module_state) * nmodules);
		return(library_image);
	    }
	    if(p->next_images == NULL)
		break;
	}
	p->next_images = allocate(sizeof(struct library_images));
	memset(p->next_images, '\0',  sizeof(struct library_images));
	library_image = p->next_images->images + p->next_images->nimages++;
	library_image->nmodules = nmodules;
	library_image->modules = allocate_module_states(nmodules);
	memset(library_image->modules, '\0', sizeof(module_state) * nmodules);
	return(library_image);
}

/*
 * allocate_module_states() is passed the number of modules in a library and
 * returns a pointer to that many module_states.  The module_states either come
 * from the block of module states in the module_state_block or are malloc()'ed.
 */
static
module_state *
allocate_module_states(
unsigned long nmodules)
{
    module_state *p;

	if(nmodules <= MODULE_STATE_BLOCK_SIZE - module_state_block.used &&
	   return_on_error == FALSE){
	    p = module_state_block.module_states + module_state_block.used;
	    module_state_block.used += nmodules;
	}
	else{
	    p = allocate(sizeof(module_state) * nmodules);
	}
	return(p);
}

/*
 * deallocate_module_states() is passed a pointer to an array of module states
 * allocated by the above allocate_module_states() routine to deallocate in the
 * case of an error when return_on_error is set.  These are alwasys allocated
 * with allocate() so we can free them.
 */
static
void
deallocate_module_states(
module_state *modules,
unsigned long nmodules)
{
	if(modules < module_state_block.module_states &&
	   modules + nmodules >= module_state_block.module_states +
				 MODULE_STATE_BLOCK_SIZE){
	    free(modules);
	}
}

/*
 * validate_library() reports an error and returns FALSE if the loaded
 * library's compatibility_version is less than the one the load command
 * requires, or the timestaps do not match. Otherwise, returns TRUE.
 */
static
enum bool
validate_library(
char *dylib_name,
struct dylib_command *dl,
struct library_image *li)
{
	if(dl != NULL &&
	   dl->dylib.compatibility_version >
	   li->dlid->dylib.compatibility_version){
	    error("version mismatch for library: %s (compatibility "
		"version of user: %lu.%lu.%lu greater than "
		"library's version: %lu.%lu.%lu)", dylib_name,
		dl->dylib.compatibility_version >> 16,
		(dl->dylib.compatibility_version >> 8) & 0xff,
		dl->dylib.compatibility_version & 0xff,
		li->dlid->dylib.compatibility_version >> 16,
		(li->dlid->dylib.compatibility_version >> 8) & 0xff,
		li->dlid->dylib.compatibility_version & 0xff);
	    link_edit_error(DYLD_FILE_FORMAT, ESHLIBVERS, dylib_name);
	    return(FALSE);
	}
	/*
	 * If this library's time stamps do not match then disable
	 * prebinding.
	 */
	if(dl == NULL || 
	   dl->dylib.timestamp != li->dlid->dylib.timestamp){
	    if(dyld_prebind_debug != 0 &&
	       prebinding == TRUE &&
	       launched == FALSE)
		print("dyld: %s: prebinding disabled because time stamp of "
		      "library: %s did not match\n", executables_name,
		      dylib_name);
	    if(launched == FALSE)
		prebinding = FALSE;
	}
	return(TRUE);
}

/*
 * is_library_loaded_by_name() returns TRUE if the library is already loaded.
 * Also it validates the compatibility version and timestamp of the library
 * against the load command.
 */
static
enum bool
is_library_loaded_by_name(
char *dylib_name,
struct dylib_command *dl)
{
    unsigned long i;
    struct library_images *p;

	for(p = &library_images; p != NULL; p = p->next_images){
	    for(i = 0; i < p->nimages; i++){
		if(strcmp(dylib_name, p->images[i].image.name) == 0){
		    return(validate_library(dylib_name, dl, &(p->images[i])));
		}
	    }
	}
	return(FALSE);
}

/*
 * is_library_loaded_by_stat() returns TRUE if the library is already loaded. 
 * Also it validates the compatibility version and timestamp of the library
 * against the load command.
 */
static
enum bool
is_library_loaded_by_stat(
char *dylib_name,
struct dylib_command *dl,
struct stat *stat_buf)
{
    unsigned long i;
    struct library_images *p;

	/*
	 * This may be the same library but as a different name in the file
	 * system.  So compare the device and inode pair for a match.
	 */
	for(p = &library_images; p != NULL; p = p->next_images){
	    for(i = 0; i < p->nimages; i++){
		if(stat_buf->st_dev == p->images[i].dev &&
		   stat_buf->st_ino == p->images[i].ino){
		    return(validate_library(dylib_name, dl, &(p->images[i])));
		}
	    }
	}
	return(FALSE);
}

/*
 * set_images_to_prebound() is called to set the modules of the images to their
 * prebound state.  If successfull it returns TRUE else FALSE.
 */
enum bool
set_images_to_prebound(
void)
{
    unsigned long i, j;
    struct load_command *lc;
    struct prebound_dylib_command *pbdylib;
    struct library_images *q;
#ifdef __MACH30__
    enum bool lazy_inits;

	lazy_inits = FALSE;
#endif __MACH30__
	/*
	 * Walk through the executable's load commands for LC_PREBOUND_DYLIB
	 * commands setting the module's state for the specified library.
	 */
	lc = (struct load_command *)((char *)object_images.images[0].image.mh +
				    sizeof(struct mach_header));
	for(i = 0; i < object_images.images[0].image.mh->ncmds; i++){
	    switch(lc->cmd){
	    case LC_PREBOUND_DYLIB:
		pbdylib = (struct prebound_dylib_command *)lc;
		if(set_prebound_state(pbdylib) == FALSE){
		    reset_module_states();
		    return(FALSE);
		}
		break;
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}

	/*
	 * Check to see that all the libraries got their state set to the
	 * prebound state.
	 */
	q = &library_images;
	do{
	    for(i = 0; i < q->nimages; i++){
#ifdef __MACH30__
		if(q->images[i].image.mh->flags & MH_LAZY_INIT)
		    lazy_inits = TRUE;
#endif __MACH30__
		if(q->images[i].image.prebound != TRUE){
		    if(dyld_prebind_debug != 0)
			print("dyld: %s: prebinding disabled because no "
			       "LC_PREBOUND_DYLIB for library: %s\n",
				executables_name,
				(char *)q->images[i].dlid +
				q->images[i].dlid->dylib.name.offset);
		    prebinding = FALSE;
		    reset_module_states();
		    return(FALSE);
		}
	    }
	    q = q->next_images;
	}while(q != NULL);

	/*
	 * Every thing checks so set the executable to fully linked and all the
	 * library modules that did not get set to fully linked to prebound
	 * unlinked.  Then return TRUE.
	 */
	q = &library_images;
	do{
	    for(i = 0; i < q->nimages; i++){
		for(j = 0; j < q->images[i].nmodules; j++)
		    if(q->images[i].modules[j] != FULLY_LINKED)
			SET_LINK_STATE(q->images[i].modules[j],
				       PREBOUND_UNLINKED);
	    }
	    q = q->next_images;
	}while(q != NULL);

	SET_LINK_STATE(object_images.images[0].module, FULLY_LINKED);

	setup_prebound_coalesed_symbols();

#ifdef __MACH30__
	if(lazy_inits == TRUE)
	    setup_for_lazy_init_routines();
#endif __MACH30__

	call_registered_funcs_for_add_images();
	return(TRUE);
}

/*
 * set_prebound_state() takes a prebound_dylib_command and sets the modules of
 * the specified library to the fully linked state for the linked modules.
 * If the prebound_dylib_command refers to a library that is not loaded or it
 * has the wrong number of modules or it is not the first one referencing the
 * library FALSE is returned.  Otherwise it is successfull and TRUE is returned.
 */
static
enum bool
set_prebound_state(
struct prebound_dylib_command *pbdylib)
{
    unsigned long i, j;
    char *name, *linked_modules, *install_name;
    struct library_images *q;

	name = (char *)pbdylib + pbdylib->name.offset;
	linked_modules = (char *)pbdylib + pbdylib->linked_modules.offset;
	q = &library_images;
	do{
	    for(i = 0; i < q->nimages; i++){
		install_name = (char *)q->images[i].dlid +
				q->images[i].dlid->dylib.name.offset;
		if(strcmp(install_name, name) == 0){
		    if(q->images[i].image.prebound == TRUE ||
		       q->images[i].image.dyst->nmodtab != pbdylib->nmodules){
			if(dyld_prebind_debug != 0)
			    print("dyld: %s: prebinding disabled because "
				   "nmodules in LC_PREBOUND_DYLIB for library: "
				   "%s does not match\n",executables_name,name);
			prebinding = FALSE;
			return(FALSE);
		    }
		    for(j = 0; j < q->images[i].nmodules; j++){
			if((linked_modules[j/8] >> (j%8)) & 1)
			    SET_LINK_STATE(q->images[i].modules[j],
					   FULLY_LINKED);
		    }
		    q->images[i].image.prebound = TRUE;
		    return(TRUE);
		}
	    }
	    q = q->next_images;
	}while(q != NULL);
	if(dyld_prebind_debug != 0)
	    print("dyld: %s: prebinding disabled because LC_PREBOUND_DYLIB "
		   "found for library: %s but it was not loaded\n",
		   executables_name, name);
	prebinding = FALSE;
	return(FALSE);
}

/*
 * undo_prebound_images() is called when the prebound state of the images can't
 * be used.  This undoes the prebound state.  Note that if the library image
 * was slid then it's local relocation has already been done.
 */
void
undo_prebound_images(
void)
{
    unsigned long i;
    struct object_images *p;
    struct library_images *q;
    struct segment_command *linkedit_segment;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    struct relocation_info *relocs;
    struct nlist *symbols;
    char *strings;
    enum link_state link_state;

	/*
	 * First undo the prebinding for the object images.
	 */
	p = &object_images;
	do{
	    for(i = 0; i < p->nimages; i++){
		link_state = GET_LINK_STATE(p->images[i].module);
		if(link_state == UNUSED)
		    continue;
		/* if this image was not prebound skip it */
		if((p->images[i].image.mh->flags & MH_PREBOUND) != MH_PREBOUND)
		    continue;

		/*
		 * If the image has relocations in read-only segments and the
		 * protection needs to change change it.
		 */
		if(p->images[i].image.change_protect_on_reloc){
		    make_image_writable(&(p->images[i].image), "object");
		}

		/* undo the prebinding of the lazy symbols pointers */
		undo_prebound_lazy_pointers(
		    &(p->images[i].image),
#if defined(m68k) || defined(__i386__)
		    GENERIC_RELOC_PB_LA_PTR);
#endif
#ifdef hppa
		    HPPA_RELOC_PB_LA_PTR);
#endif
#ifdef sparc
		    SPARC_RELOC_PB_LA_PTR);
#endif
#ifdef __ppc__
		    PPC_RELOC_PB_LA_PTR);
#endif
		linkedit_segment = p->images[i].image.linkedit_segment;
		st = p->images[i].image.st;
		dyst = p->images[i].image.dyst;
		/*
		 * Object images could be loaded that do not have the proper
		 * link edit information.
		 */
		if(linkedit_segment != NULL && st != NULL && dyst != NULL){
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
		    /* undo the prebinding of the external relocation */
		    undo_external_relocation(
			TRUE, /* undo_prebinding */
			&(p->images[i].image),
			relocs,
			dyst->nextrel,
			symbols,
			strings,
			NULL, /* library_name */
			p->images[i].image.name);
		}

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
	 * Second undo the prebinding for the library images.
	 */
	q = &library_images;
	do{
	    for(i = 0; i < q->nimages; i++){
		/* if this image was not prebound skip it */
		if((q->images[i].image.mh->flags & MH_PREBOUND) != MH_PREBOUND)
		    continue;
		undo_prebinding_for_library(q->images + i);
	    }
	    q = q->next_images;
	}while(q != NULL);
}

/*
 * undo_prebinding_for_library() undoes the prebinding for the specified
 * library.  We make sure the lazy pointers are reset and then just set the
 * module state to PREBOUND_UNLINKED.
 */
static
void
undo_prebinding_for_library(
struct library_image *library_image)
{
    unsigned long j;

	/*
	 * Undo the prebinding of the lazy symbols pointers if the
	 * library as not been slid.  If it has been slid then this
	 * would have been done in local_relocation().
	 */
	if(library_image->image.vmaddr_slide == 0)
	    undo_prebound_lazy_pointers(
		&(library_image->image),
#if defined(m68k) || defined(__i386__)
		GENERIC_RELOC_PB_LA_PTR);
#endif
#ifdef hppa
		HPPA_RELOC_PB_LA_PTR);
#endif
#ifdef sparc
		SPARC_RELOC_PB_LA_PTR);
#endif
#ifdef __ppc__
		PPC_RELOC_PB_LA_PTR);
#endif

	for(j = 0; j < library_image->nmodules; j++)
	    SET_LINK_STATE(library_image->modules[j], PREBOUND_UNLINKED);

}

/*
 * try_to_use_prebound_libraries() is called when the libraries are setup for
 * prebinding but the executable is not.  If if is successfull prebinding is
 * left set to TRUE if not prebinding gets set to FALSE.
 */
void
try_to_use_prebound_libraries(
void)
{
	/*
	 * For now without two-level namespaves it is very expensive to check
	 * all libraries against all other libraries just to see if we can
	 * launch using prebound libraries.  So for now if there is more than
	 * one library we'll not use prebound libraries.
	 */
	if(library_images.nimages > 1){
	    prebinding = FALSE;
	    return;
	}

	/*
	 * Check to see this executable and libraries do not define any symbols
	 * defined and referenced in the libraries used.
	 */
	if(check_executable_for_overrides() == FALSE)
	    return;
	if(check_libraries_for_overrides() == FALSE)
	    return;

	/*
	 * Now put all undefined symbols from the executable on the undefined
	 * list.
	 */
	setup_initial_undefined_list(TRUE);

	/*
	 * Now resolve all symbol references this program has to see it there
	 * will be any undefined symbols and to mark which libraries modules
	 * will be linked.
	 */
	if(resolve_undefineds(TRUE, TRUE) == FALSE){
	    /* a multiply defined error occured */
	    failed_use_prebound_libraries();
	    return;
	}

	/*
	 * If there are undefineds then this try failed.
	 */
	if(undefined_list.next != &undefined_list){
	    if(dyld_prebind_debug != 0 && prebinding == TRUE)
		print("dyld: %s: trying to use prebound libraries failed due "
		       "to undefined symbols\n", executables_name);
	    prebinding = FALSE;
	    failed_use_prebound_libraries();
	    return;
	}

	/*
	 * Now do the relocation of just the executable module and mark the
	 * library modules that were being linked as FULLY_LINKED and the other
	 * library modules as PREBOUND_UNLINKED.
	 */
	relocate_modules_being_linked(TRUE);
	SET_LINK_STATE(object_images.images[0].module, FULLY_LINKED);

	setup_prebound_coalesed_symbols();

	/*
	 * This just causes all images to be marked as the register funcs (which
	 * there are none at launch time) are called.
	 */
	call_registered_funcs_for_add_images();
}

/*
 * failed_use_prebound_libraries() is called when the try to use prebound
 * libraries failed and things need to be cleaned up.  This clears up the
 * undefined and being linked lists.  Resets all the module_states to unlinked.
 */
static
void
failed_use_prebound_libraries(
void)
{
	/* clear undefined list */
	clear_undefined_list(FALSE);

	/* clear being linked list */
	clear_being_linked_list(FALSE);
    
	/* reset all the module_states to unlinked */
	reset_module_states();
}

/*
 * reset_module_states() is used when prebinding fails and all the module states
 * need to be set back to UNLINKED.
 */
static
void
reset_module_states(
void)
{
    unsigned long i, j;
    struct library_images *q;

	SET_LINK_STATE(object_images.images[0].module, BEING_LINKED);
	CLEAR_FULLYBOUND_STATE(object_images.images[0].module);

	q = &library_images;
	do{
	    for(i = 0; i < q->nimages; i++){
		q->images[i].image.init_bound = FALSE;
		for(j = 0; j < q->images[i].nmodules; j++){
		    SET_LINK_STATE(q->images[i].modules[j], UNLINKED);
		    CLEAR_FULLYBOUND_STATE(q->images[i].modules[j]);
		}
	    }
	    q = q->next_images;
	}while(q != NULL);
}

#ifdef __ppc__
#include "fp_save_restore.h"
#endif /* __ppc__ */
/*
 * call_image_init_routines() calls the image initialization routines for the
 * images that have modules newly being used in them.
 */
void
call_image_init_routines(
enum bool make_delayed_calls)
{
    unsigned long i, addr;
    struct library_images *q;
    module_state *module;
    void (*init_routine)(void);
    enum link_state link_state;
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
     * The calls to the image initialization routines start off delayed so that
     * the initialization in the runtime start off is done.  When that is done
     * the runtime startoff calls call_image_init_routines(TRUE) and then sets
     * delay_init_routines to FALSE. 
     */
    static enum bool delay_init_routines = TRUE;

	if(delay_init_routines == TRUE && make_delayed_calls == FALSE)
	    return;
	if(make_delayed_calls == TRUE)
	    delay_init_routines = FALSE;

	/*
	 * For the libraries which have initialization routines and the module
	 * that contains the initialization routine has been bound, call the
	 * ones that have not been called.
	 */
	q = &library_images;
	do{
	    for(i = 0; i < q->nimages; i++){
		if(q->images[i].image.rc != NULL &&
		   q->images[i].image.init_called == FALSE){
		    module = q->images[i].modules +
			     q->images[i].image.rc->init_module;
		    link_state = GET_LINK_STATE(*module);
		    if(link_state != UNLINKED &&
		       link_state != PREBOUND_UNLINKED){
#ifdef __ppc__
#if !defined(__GONZO_BUNSEN_BEAKER__) && !defined(__HERA__)
			if(facilities_used == -1)
			     facilities_used = processor_facilities_used();
#endif
			if(saved_regs == FALSE){
#if !defined(__GONZO_BUNSEN_BEAKER__) && !defined(__HERA__)
			    if(facilities_used & floatUsed)
#endif
				ppc_fp_save(fp_save_area);
#if defined(__GONZO_BUNSEN_BEAKER__) || defined(__HERA__)
			    if(processor_has_vec == TRUE)
#else
			    if(_cpu_has_altivec == TRUE &&
			       (facilities_used & vectorUsed))
#endif
				ppc_vec_save(vec_save_area);
			    saved_regs = TRUE;
			}
#endif /* __ppc__ */
			q->images[i].image.init_called = TRUE;

			call_dependent_init_routines(
			    q->images + i,
			    &(q->images[i].image),
			    module,
			    make_delayed_calls == TRUE && prebinding == TRUE);

			/* do not actually init routines marked lazy here */
			if(q->images[i].image.lazy_init == FALSE){
			    addr = q->images[i].image.rc->init_address +
				   q->images[i].image.vmaddr_slide;
			    init_routine = (void(*)(void))addr;

			    if(init_routine_being_called == FALSE)
				init_routine_being_called = TRUE;
			    else if(dyld_abort_multiple_inits == TRUE)
				abort();

			    release_lock();
			    init_routine();
			    set_lock();
			    init_routine_being_called = FALSE;
			}
		    }
		}
	    }
	    q = q->next_images;
	}while(q != NULL);

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
 * call_dependent_init_routines() is passed a library (or NULL), image and
 * module for the module that an image init routine depends on.
 * If use_header_dependencies is FALSE the references of that module are looked
 * up and if any them have library init routines that have not been called they 
 * are called.  Before the init routine is called this routine is first called
 * recursively.  If use_header_dependencies is TRUE then this is a case of
 * the first time init routines are called in a prebound launch.  If so then
 * instead of using the symbol references to figure out dependencies they are
 * determined from the headers of the libraries.
 */
static
void
call_dependent_init_routines(
struct library_image *library_image,
struct image *image,
module_state *module,
enum bool use_header_dependencies)
{
    unsigned long i;
    struct segment_command *linkedit_segment;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    struct nlist *symbols;
    char *strings;
    struct dylib_module *dylib_modules, *dylib_module;
    unsigned long module_index;
    struct dylib_reference *dylib_references;
    char *symbol_name, *module_name;

    struct nlist *defined_symbol;
    module_state *defined_module;
    struct image *defined_image;
    struct library_image *defined_library_image;
    unsigned long addr;
    void (*init_routine)(void);

    unsigned long j;
    struct load_command *lc;
    struct dylib_command *dl;
    struct library_images *q;
    char *dependent_name;
    struct library_image *dependent_library_image;
    struct image *dependent_image;
    module_state *dependent_module;
    enum link_state link_state;

	/*
	 * Since this module is a dependent of an image library init routine
	 * it was fully linked when bound.
	 *
	if(GET_INIT_STATE(*module) == 0)
	    << this is an internal error >>
	 */

	/*
	 * Mark this module as having its init routine dependencies checked
	 * so that it is only checked once.  Note that when 
	 * use_header_dependencies is TRUE module will be NULL and that library
	 * images (not modules) may be checked more than once.
	 */
	if(use_header_dependencies == FALSE || module != NULL) 
	    SET_IMAGE_INIT_DEPEND_STATE(*module);

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

	/*
	 * Now go through the references of this module and check to see all
	 * of them have had their dependencies checked.
	 */
	if(library_image == NULL){
/*
printf("call_dependent_init_routines() for %s\n", image->name);
*/
	    for(i = dyst->iundefsym;
		i < dyst->iundefsym + dyst->nundefsym;
		i++){
		symbol_name = strings + symbols[i].n_un.n_strx;
		lookup_symbol(symbol_name, &defined_symbol, &defined_module,
			      &defined_image, &defined_library_image,
			      NO_INDR_LOOP);

		/*
		 * Since this module should be fully linked we should not find
		 * any references of this module undefined.
		 */
		if(defined_symbol == NULL){
/*
printf("undefined symbol %s in %s\n", symbol_name, image->name);
*/
		    continue;
		}

		/*
		 * If this module has not had its dependent init checked then
		 * check and call them as needed.
		 */
		if(GET_IMAGE_INIT_DEPEND_STATE(*module) != 0){
		    /*
		     * Make sure all its dependent init routines are called.
		     */
		    call_dependent_init_routines(
			defined_library_image,
			defined_image,
			defined_module,
			use_header_dependencies);
		}
	    }
	}
	else{
	    dylib_modules = (struct dylib_module *)
		(image->vmaddr_slide +
		 linkedit_segment->vmaddr +
		 dyst->modtaboff -
		 linkedit_segment->fileoff);
	    if(module != NULL){
		module_index = module - library_image->modules;
		dylib_module = dylib_modules + module_index;
		module_name = strings + dylib_module->module_name;
	    }
	    else{
		dylib_module = NULL;
		module_name = "NULL";
	    }
/*
printf("call_dependent_init_routines() for %s(%s)\n", image->name, module_name);
*/
	    /*
	     * use_header_dependencies will be TRUE only in the case we are
	     * the now making the delayed calls and we are prebound.  In this	
	     * case we can walk the headers for the dependencies and avoid
	     * touching the symbol tables and all the lookups.
	     */
	    if(use_header_dependencies == TRUE){
		lc = (struct load_command *)((char *)image->mh +
		     sizeof(struct mach_header));
		for(i = 0; i < image->mh->ncmds; i++){
		    if(lc->cmd == LC_LOAD_DYLIB){
			dl = (struct dylib_command *)lc;
			dependent_name = (char *)dl + dl->dylib.name.offset;
			dependent_image = NULL;
			dependent_library_image = NULL;
			q = &library_images;
			do{
			    for(j = 0;
				dependent_image == NULL && j < q->nimages;
				j++){
				if(strcmp(q->images[j].image.name,
					  dependent_name) == 0){
				    dependent_image = &(q->images[j].image);
				    dependent_library_image = q->images + j;
				}
			    }
			    q = q->next_images;
			}while(dependent_image == NULL && q != NULL);
			/*
			 * Even if this image does not an init routine one of
			 * its dependent libraries may have one so they need
			 * to be called first).
			 */
			if(dependent_image != NULL){
			    /*
			     * Make sure all its dependent init routines
			     * are called.
			     */
			    call_dependent_init_routines(
				dependent_library_image,
				dependent_image,
				NULL,
				use_header_dependencies);
			}
			/*
			 * If this dependent image has an init routine that has
			 * not yet been called and it is in a module that was
			 * bound in then cause its dependencies to be called
			 * and then call the init routine.
			 */
			if(dependent_image != NULL &&
			   dependent_image->rc != NULL &&
			   dependent_library_image->image.init_called == FALSE){
			    dependent_module =
				     dependent_library_image->modules +
				     dependent_image->rc->init_module;
			    link_state = GET_LINK_STATE(*dependent_module);
			    if(link_state != UNLINKED &&
			       link_state != PREBOUND_UNLINKED){
				/*
				 * Mark this image as having its init routine
				 * called.
				 */
				dependent_library_image->image.init_called =
				    TRUE;

				/*
				 * Do not actually call init routines marked
				 * lazy here.
				 */
				if(dependent_library_image->image.lazy_init ==
				   FALSE){
				    /* now actually call the init routine */
				    addr = dependent_library_image->
					       image.rc->init_address +
					   dependent_library_image->
					       image.vmaddr_slide;
				    init_routine = (void(*)(void))addr;

				    if(init_routine_being_called == FALSE)
					init_routine_being_called = TRUE;
				    else if(dyld_abort_multiple_inits == TRUE)
					abort();

/*
printf("call_dependent_init_routines(use_header_dependencies == TRUE) for "
"%s(%s)\n", dependent_image->name, module_name);
*/

				    release_lock();
				    init_routine();
				    set_lock();
				    init_routine_being_called = FALSE;
				}
			    }
			}
		    }
		    lc = (struct load_command *)((char *)lc + lc->cmdsize);
		}
		return;
	    }
	    /*
	     * This is the code used normally that walks the symbol dependencies
	     * when which is used when use_header_dependencies==FALSE
	     * (non-prebound and non-make_delayed_calls==TRUE case).
	     */
	    dylib_references = (struct dylib_reference *)
		(image->vmaddr_slide +
		 linkedit_segment->vmaddr +
		 dyst->extrefsymoff -
		 linkedit_segment->fileoff);

	    for(i = dylib_module->irefsym;
		i < dylib_module->irefsym + dylib_module->nrefsym;
		i++){

		symbol_name = strings +
			      symbols[dylib_references[i].isym].n_un.n_strx;
		if(dylib_references[i].flags !=
			REFERENCE_FLAG_UNDEFINED_NON_LAZY &&
		   dylib_references[i].flags !=
			REFERENCE_FLAG_UNDEFINED_LAZY)
		    continue;
		lookup_symbol(symbol_name, &defined_symbol, &defined_module,
			      &defined_image, &defined_library_image,
			      NO_INDR_LOOP);
/*
printf("reference to %s ", symbol_name);
if(defined_library_image != NULL)
    printf("defined in library %s module #%d\n", defined_image->name,
	defined_module - defined_library_image->modules);
else
    printf("defined in %s\n", defined_image->name);
*/

		/*
		 * Since this module should be fully linked we should not find
		 * any references of this module undefined.
		 */
		if(defined_symbol == NULL){
/*
printf("undefined symbol %s in %s(%s)\n", symbol_name, image->name,module_name);
*/
		    continue;
		}

		/*
		 * If this module has not had its dependent init checked then
		 * check and call them as needed.
		 */
		if(GET_IMAGE_INIT_DEPEND_STATE(*defined_module) == 0){
		    /*
		     * Make sure all its dependent init routines are called.
		     */
		    call_dependent_init_routines(
			defined_library_image,
			defined_image,
			defined_module,
			use_header_dependencies);
		}

		/*
		 * Now if this symbol was actually defined in a library and it
		 * has an init routine that was not called yet call it.
		 */ 
		if(defined_library_image != NULL &&
		   defined_library_image->image.rc != NULL &&
		   defined_library_image->image.init_called == FALSE){

		    /* mark this image as having its init routine called */
		    defined_library_image->image.init_called = TRUE;

		    /* do not actually init routines marked lazy here */
		    if(defined_library_image->image.lazy_init == FALSE){
			/* now actually call the init routine */
			addr = defined_library_image->image.rc->init_address +
			       defined_library_image->image.vmaddr_slide;
			init_routine = (void(*)(void))addr;

/*
printf("call_dependent_init_routines(use_header_dependencies == FALSE) for "
"%s(%s)\n", defined_library_image->image.name, module_name);
*/

			if(init_routine_being_called == FALSE)
			    init_routine_being_called = TRUE;
			else if(dyld_abort_multiple_inits == TRUE)
			    abort();

			release_lock();
			init_routine();
			set_lock();
			init_routine_being_called = FALSE;
		    }
		}
	    }
	}
}

#ifdef __MACH30__
/*
 * setup_for_lazy_init_routines() turns off the vm protection for the writeable
 * segments of libraries that are marked with MH_LAZY_INIT.  It is called
 * when we are launching prebound and there are libraries marked with
 * MH_LAZY_INIT.
 */
static
void
setup_for_lazy_init_routines(
void)
{
    struct library_images *q;
    unsigned long i, j;
    struct load_command *lc, *load_commands;
    struct segment_command *sg;
    vm_address_t address;
    kern_return_t r;
    enum bool set_up_handler;
    mach_port_t my_exception_port;
    mach_msg_type_number_t old_exception_count;
    thread_state_flavor_t my_flavor;
    int result;
    pthread_t pthread;

	set_up_handler = FALSE;
	q = &library_images;
	do{
	    for(i = 0; i < q->nimages; i++){
		if((q->images[i].image.mh->flags & MH_LAZY_INIT) != 0 &&
		    q->images[i].image.rc != NULL){
		    /*
		     * Set the initial protection of the segments which are
		     * writeable to no protection so that they will cause a
		     * memory fault when accessed.
		     */
		    load_commands = (struct load_command *)
				((char *)q->images[i].image.mh +
				 sizeof(struct mach_header));
		    lc = load_commands;
		    for(j = 0; j < q->images[i].image.mh->ncmds; j++){
			switch(lc->cmd){
			case LC_SEGMENT:
			    sg = (struct segment_command *)lc;
			    address = sg->vmaddr +
				      q->images[i].image.vmaddr_slide;
			    if((sg->initprot & VM_PROT_WRITE) != 0 &&
			       address != (vm_address_t)q->images[i].image.mh &&
			       sg != q->images[i].image.linkedit_segment){
				if((r = vm_protect(mach_task_self(), address,
					   (vm_size_t)sg->vmsize,
					   FALSE, VM_PROT_NONE)) !=
					   KERN_SUCCESS){
				    mach_error(r, "can't vm_protect segment: "
					"%.16s for library: %s", sg->segname,
					q->images[i].image.name);
				    link_edit_error(DYLD_MACH_RESOURCE, r,
					q->images[i].image.name);
				}
				q->images[i].image.lazy_init = TRUE;
				set_up_handler = TRUE;
			    }
			    break;
			}
			lc = (struct load_command *)((char *)lc + lc->cmdsize);
		    }
		}
	    }
	    q = q->next_images;
	}while(q != NULL);

	/*
	 * If we turned off the protections on anything then we need to set up
	 * a handler to deal with the memory exceptions if not were done.
	 */
	if(set_up_handler == FALSE)
	    return;

	/*
	 * Allocate a port to receive exception messages on.
	 */
	r = mach_port_allocate(
		mach_task_self(),
		MACH_PORT_RIGHT_RECEIVE,
		&my_exception_port);
	if(r != KERN_SUCCESS){
	    mach_error(r, "can't allocate exception port, mach_port_allocate() "
		       "failed");
	    link_edit_error(DYLD_MACH_RESOURCE, r, NULL);
	}
	/*
	 * We need to insert send rights on this port or
	 * task_set_exception_ports() will fail with "(ipc/send) invalid port
	 * right".
	 */
	r = mach_port_insert_right(
		mach_task_self(),
		my_exception_port,
		my_exception_port,
		MACH_MSG_TYPE_MAKE_SEND);
	if(r != KERN_SUCCESS){
	    mach_error(r, "can't insert send rights on exception port, "
		       "mach_port_insert_right() failed");
	    link_edit_error(DYLD_MACH_RESOURCE, r, NULL);
	}

	/*
	 * Before we set the new exception port we would like to know the
	 * thread flavor the old exception port is expecting so that we also
	 * get that flavor.
	 *
	 * We would rather use the atomic task_swap_exception_ports() but
	 * since we don't know what if any the old exception port wants for its
	 * thread flavor we have to do a pair of task_get_exception_ports() and
	 * task_set_exception_ports() calls.
	 */
	old_exception_count = 1;
	r = task_get_exception_ports(
		mach_task_self(),	/* task */
		EXC_MASK_BAD_ACCESS,	/* exception_types */
		old_exception_masks,
		&old_exception_count,
		old_exception_ports,
		old_behaviors,
		old_flavors);
	if(r != KERN_SUCCESS){
	    mach_error(r, "can't get old exception port, "
		       "task_get_exception_ports() failed");
	    link_edit_error(DYLD_MACH_RESOURCE, r, NULL);
	}
	if(old_exception_count != 1){
	    error("unexpected value returned from task_get_exception_ports() "
		  "old_exception_count (%d) value not 1 for a single "
		  "exception_type (EXC_MASK_BAD_ACCESS)", old_exception_count);
	    link_edit_error(DYLD_OTHER_ERROR, DYLD_LAZY_INIT, NULL);
	}
	/*
	 * We have to be careful to set the flavor for our handler to a valid
	 * thread flavor since we want the EXCEPTION_STATE_IDENTITY behavior.
	 * If it were set to THREAD_STATE_NONE the kernel will panic when the
	 * exception occurs.  THREAD_STATE_NONE is what gets returned as the
	 * old_flavor for the EXCEPTION_DEFAULT behavior.
	 */
	if(old_behaviors[0] == EXCEPTION_DEFAULT){
#ifdef __ppc__
	    my_flavor = PPC_THREAD_STATE;
#endif
#ifdef __i386__
	    my_flavor = i386_THREAD_STATE;
#endif
#ifdef m68k
	    my_flavor = M68K_THREAD_STATE_REGS;
#endif
#ifdef hppa
	    my_flavor = HPPA_FRAME_THREAD_STATE;
#endif
#ifdef sparc
	    my_flavor = SPARC_THREAD_STATE_REGS;
#endif
	}
	else{
	    my_flavor = old_flavors[0];
	}
	/*
	 * Set the exception port to the port we just allocated and using
	 * the thread flavor of the old exception port.
	 */
	r = task_set_exception_ports(
		mach_task_self(),	 /* task */
		EXC_MASK_BAD_ACCESS,	 /* exception_types */
		my_exception_port,	 /* exception_port */
		EXCEPTION_STATE_IDENTITY,/* behavior */
		my_flavor);		 /* flavor */
	if(r != KERN_SUCCESS){
	    mach_error(r, "can't set exception port, "
		       "task_set_exception_ports() failed");
	    link_edit_error(DYLD_MACH_RESOURCE, r, NULL);
	}

	/*
	 * Create a pthread to listen on the exception port.
	 * We do not need to set the attribute of PTHREAD_CREATE_JOINABLE as
	 * this is the default with a NULL attribute.
	 */
	result = pthread_create(
			&pthread,		/* thread */
			NULL,			/* attributes */
			(void *(*)(void *))
			exception_server_loop,	/* start_routine */
			(void *)
			my_exception_port);	/* arg */
	if(result != 0){
	    system_error(result, "can't create a pthread for exception server "
			 "loop");
	    link_edit_error(DYLD_UNIX_RESOURCE, result, NULL);
	}
}

/*
 * exception_server_loop() is where the pthread created to handle the exception
 * starts executing.
 */
static
void
exception_server_loop(
mach_port_t my_exception_port)
{
    unsigned char msg_buf[MY_MSG_SIZE], reply_buf[MY_MSG_SIZE];
    mach_msg_header_t *msg, *reply;
    kern_return_t r;
    boolean_t eret;

    msg = (mach_msg_header_t *) msg_buf;
    reply = (mach_msg_header_t *) reply_buf;

	/*
	 * This is the exception server loop which receives messages on the
	 * exception port, calls the library routine exc_server(), and sends
	 * a reply to the message.
	 */
	for(;;){
	    r = mach_msg(msg, 			/* msg */
			 MACH_RCV_MSG, 		/* option */
			 0,			/* send size */
			 MY_MSG_SIZE,		/* receive_limit */
			 my_exception_port,	/* receive_name */
			 0,			/* timeout */
			 MACH_PORT_NULL);	/* notify */
	    if(r != KERN_SUCCESS){
		mach_error(r, "can't receive exception message in "
			   "exception_server_loop(), mach_msg() failed");
		link_edit_error(DYLD_MACH_RESOURCE, r, NULL);
	    }

	    /*
	     * call the exc_server() routine in the library to break out the
	     * message which will call
	     * internal_catch_exception_raise_state_identity()
	     * defined below.  In reading the mig generated code on MacOS X
	     * this can only happen if exc_server() can call the needed function
	     * for the behavior which would be a programing error.
	     */
	    eret = exc_server(msg, reply);
	    if(eret == FALSE){
		error("exc_server() returned FALSE which is an internal error "
		      "in the way the library and user code got linked up");
		link_edit_error(DYLD_OTHER_ERROR, DYLD_LAZY_INIT, NULL);
	    }
  
	    r = mach_msg(reply,
			 (MACH_SEND_MSG | MACH_MSG_OPTION_NONE),
			 reply->msgh_size,
			 0,
			 MACH_PORT_NULL,
			 MACH_MSG_TIMEOUT_NONE,
			 MACH_PORT_NULL);
	    if(r != KERN_SUCCESS){
		mach_error(r, "can't send reply exception message in "
			   "exception_server_loop(), mach_msg() failed");
		link_edit_error(DYLD_MACH_RESOURCE, r, NULL);
	    }
	}
}

/*
 * These are needed to avoid pulling things in from libc.a that use
 * _dyld_lookup_and_bind.  This is all because of this odd interface where these
 * functions must be defined if exc_server() is used.
 */
void internal_catch_exception_raise(void){}
void internal_catch_exception_raise_state(void){}

/*
 * internal_catch_exception_raise_state_identity() must be global as it is
 * called by the library routine exc_server().  This routine calls
 * call_lazy_init_routine_for_address() to do the real work to fix up the
 * exception by changing the protection on the pages we expect to see
 * exceptions and returns TRUE, if it returns FALSE we forward the exception.
 */
kern_return_t
internal_catch_exception_raise_state_identity(
exception_port_t exception_port,
thread_port_t thread,
task_port_t task, 
exception_type_t exception,
exception_data_t code,
mach_msg_type_number_t code_count,
thread_state_flavor_t *flavor,
thread_state_t in_state,
mach_msg_type_number_t in_state_count,
thread_state_t *out_state,		/* wrong type in documentation */
mach_msg_type_number_t *out_state_count)/* wrong type in documentation */
{
    mach_msg_type_number_t i;
    boolean_t expected_exception, found_segment;
    unsigned long exception_address;
    vm_prot_t protection;
    kern_return_t r;

    expected_exception = FALSE;
    exception_address = 0;
    found_segment = FALSE;
    protection = VM_PROT_NONE;

#ifdef DEBUG_LAZY_INIT_EXCEPTIONS
	/*
	 * Print out the exception info for this exception.
	 */
	printf("internal_catch_exception_raise_state_identity called\n");
	printf("        exception_port: 0x%x\n", exception_port);
	printf("                thread: 0x%x\n", thread);
	printf("                  task: 0x%x\n", task);
	printf("             exception: 0x%x (%s)\n", exception,
	        exception_name(exception));
	printf( "            code_count: 0x%x\n", code_count);
#endif /* DEBUG_LAZY_INIT_EXCEPTIONS */

	for(i = 0 ; i < code_count; i++){
#ifdef DEBUG_LAZY_INIT_EXCEPTIONS
	    printf("               code[%d]: 0x%x", i, code[i]);
#endif /* DEBUG_LAZY_INIT_EXCEPTIONS */
	    /*
	     * I'm guessing the first word of code is what they call
	     * the "code" and for EXC_BAD_ACCESS this is a kern_return_t.
	     */
	    if(exception == EXC_BAD_ACCESS && i == 0){
		switch(code[i]){
		case KERN_PROTECTION_FAILURE:
		    expected_exception = TRUE;
#ifdef DEBUG_LAZY_INIT_EXCEPTIONS
		    printf(" (code, KERN_PROTECTION_FAILURE)\n");
		    break;
		case KERN_INVALID_ADDRESS:
		    printf(" (code, KERN_INVALID_ADDRESS)\n");
		    break;
		default:
		    printf(" (code, unknown)\n");
#endif /* DEBUG_LAZY_INIT_EXCEPTIONS */
		    break;
		}
	    }
	    /*
	     * I'm guessing the second word of code is what they call
	     * the "subcode" and and for EXC_BAD_ACCESS this is the bad memory
	     * address.
	     */
	    else if(exception == EXC_BAD_ACCESS && i == 1){
		exception_address = code[i];
#ifdef DEBUG_LAZY_INIT_EXCEPTIONS
		printf(" (subcode, the bad memory address)\n");
	    }
	    else{
		printf("\n");
#endif /* DEBUG_LAZY_INIT_EXCEPTIONS */
	    }
	}
#ifdef DEBUG_LAZY_INIT_EXCEPTIONS
	printf("                flavor: %d ", *flavor);
	switch(*flavor){
#ifdef __ppc__
	case PPC_THREAD_STATE:
	    printf("PPC_THREAD_STATE\n");
	    printf("        in_state_count: %d ", in_state_count);
	    if(in_state_count == PPC_THREAD_STATE_COUNT)
		printf("PPC_THREAD_STATE_COUNT\n");
	    else
		printf("(not PPC_THREAD_STATE_COUNT)\n");
	    /* print_ppc_thread_state(in_state, *flavor); */
	    break;
	case PPC_FLOAT_STATE:
	    printf("PPC_FLOAT_STATE\n");
	    printf("        in_state_count: %d ", in_state_count);
	    if(in_state_count == PPC_FLOAT_STATE_COUNT)
		printf("PPC_FLOAT_STATE_COUNT\n");
	    else
		printf("(not PPC_FLOAT_STATE_COUNT)\n");
	    /* print_ppc_thread_state(in_state, *flavor); */
	    break;
	case PPC_EXCEPTION_STATE:
	    printf("PPC_EXCEPTION_STATE\n");
	    printf("        in_state_count: %d ", in_state_count);
	    if(in_state_count == PPC_EXCEPTION_STATE_COUNT)
		printf("PPC_EXCEPTION_STATE_COUNT\n");
	    else
		printf("(not PPC_EXCEPTION_STATE_COUNT)\n");
	    /* print_ppc_thread_state(in_state, *flavor); */
	    break;
#endif /* __ppc__ */
#ifdef __i386__
	case i386_THREAD_STATE:
	    printf("i386_THREAD_STATE\n");
	    printf("        in_state_count: %d\n", in_state_count);
	    break;
#endif /* __i386__ */
	default:
	    printf("(unknown state)\n");
	    printf("        in_state_count: %d\n", in_state_count);
	    break;
	}
#endif /* DEBUG_LAZY_INIT_EXCEPTIONS */

	/*
	 * If we handle the exception we want the thread to just continue from
	 * where it was so we need to copy the in_state to the out_state.  And
	 * if we forward the exception the behavior may not include the state
	 * but since we did we still have to copy the state.  The exception
	 * handler we are forwarding can still change this state if the want.
	 */
	memcpy(out_state, in_state,
	       sizeof(mach_msg_type_number_t) * in_state_count);
	*out_state_count = in_state_count;

	/*
	 * Here's where we take the memory address of the exception and
	 * check to see if it is in one of our segments and then change the
	 * protections on that page.  If we can deal with it we return 
	 * KERN_SUCCESS else we forward the message on to the old exception
	 * port.
	 */
	if(expected_exception == TRUE){
	    if(call_lazy_init_routine_for_address(exception_address) == TRUE)
		return(KERN_SUCCESS);
	}
	
	/*
	 * This exception is not for a area of memory we have the protection
	 * turned off so forward it to the old exception port using the old
	 * behavior.
	 */
	switch(old_behaviors[0]){
	case EXCEPTION_DEFAULT:
#ifdef DEBUG_LAZY_INIT_EXCEPTIONS
	    printf("forwarding the exception with exception_raise()\n");
#endif /* DEBUG_LAZY_INIT_EXCEPTIONS */
	    r = exception_raise(
		old_exception_ports[0],
		thread,
		task,
		exception,
		code,
		code_count);
	    if(r != KERN_SUCCESS){
		mach_error(r, "exception_raise() failed in forwarding "
			   "exception");
		link_edit_error(DYLD_MACH_RESOURCE, r, NULL);
	    }
	    break;

	case EXCEPTION_STATE:
#ifdef DEBUG_LAZY_INIT_EXCEPTIONS
	    printf("forwarding the exception with exception_raise_state()\n");
#endif /* DEBUG_LAZY_INIT_EXCEPTIONS */
	    r = exception_raise_state(
		old_exception_ports[0],
		exception,
		code,
		code_count,
		flavor,
		in_state,
		in_state_count,
		out_state,
		out_state_count);
	    if(r != KERN_SUCCESS){
		mach_error(r, "exception_raise_state() failed in forwarding "
			   "exception");
		link_edit_error(DYLD_MACH_RESOURCE, r, NULL);
	    }
	    break;

	case EXCEPTION_STATE_IDENTITY:
#ifdef DEBUG_LAZY_INIT_EXCEPTIONS
	    printf("forwarding the exception with "
		   "exception_raise_state_identity()\n");
#endif /* DEBUG_LAZY_INIT_EXCEPTIONS */
	    r = exception_raise_state_identity(
		old_exception_ports[0],
		thread,
		task,
		exception,
		code,
		code_count,
		flavor,
		in_state,
		in_state_count,
		out_state,
		out_state_count);
	    if(r != KERN_SUCCESS){
		mach_error(r, "exception_raise_state_identity() failed in "
			   "forwarding exception");
		link_edit_error(DYLD_MACH_RESOURCE, r, NULL);
	    }
	    break;

	default:
	    error("unknown old_behavior (%d) of old exception port (don't know "
		  "how to forward the exception)\n", old_behaviors[0]);
	    link_edit_error(DYLD_OTHER_ERROR, DYLD_LAZY_INIT, NULL);
	}
#ifdef DEBUG_LAZY_INIT_EXCEPTIONS
	printf("returning with KERN_SUCCESS from my handler for forwarded "
	       "exception\n");
#endif /* DEBUG_LAZY_INIT_EXCEPTIONS */
	return(KERN_SUCCESS);
}

#ifdef DEBUG_LAZY_INIT_EXCEPTIONS
/*
 * exception_name() returns a string name for the exception type passed to
 * it.
 */
static
const char *
exception_name(
exception_type_t exception)
{
	switch(exception){
	case EXC_BAD_ACCESS:
	    return("EXC_BAD_ACCESS");
	case EXC_BAD_INSTRUCTION:
	    return("EXC_BAD_INSTRUCTION");
	case EXC_ARITHMETIC:
	    return("EXC_ARITHMETIC");
	case EXC_EMULATION:
	    return("EXC_EMULATION");
	case EXC_SOFTWARE:
	    return("EXC_SOFTWARE");
	case EXC_BREAKPOINT: 
	     return("EXC_BREAKPOINT");
	case EXC_SYSCALL:
	     return("EXC_SYSCALL");
	case EXC_MACH_SYSCALL:
	     return("EXC_MACH_SYSCALL");
	case EXC_RPC_ALERT:
	     return("EXC_RPC_ALERT");
	default:
	    return("Unknown");
	}
}
#endif /* DEBUG_LAZY_INIT_EXCEPTIONS */

/*
 * call_lazy_init_routine_for_address() is the handler for image init routines
 * to be called when they fault on a address who we have turned off the vm
 * protection for.  If the address pass to us is for such an image then
 * all the threads in the task are suppended but this one, the vm protection is
 * restored, the init routine is called, then the threads in the task are
 * resumed.  This routine returns TRUE if the address is for one of our images
 * that has a lazy init routine else it returns FALSE.
 */
static
enum bool
call_lazy_init_routine_for_address(
unsigned long address)
{
    struct library_images *q;
    struct library_image *library_image;
    unsigned long i, j;
    struct load_command *lc, *load_commands;
    struct segment_command *sg;
    kern_return_t r;
    unsigned long addr;
    void (*init_routine)(void);
    mach_port_t my_thread, *threads;
    unsigned int thread_count;
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

	/* get the dyld lock */
	set_lock();

	/*
	 * First see if this address if for a library we have turned off the
	 * the protection for.
	 */
	library_image = NULL;
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
				library_image = q->images + i;
				goto down;
			    }
			}
			lc = (struct load_command *)((char *)lc + lc->cmdsize);
		    }
		}
		else{
		    if(address >= ((unsigned long)q->images[i].image.mh) &&
		       address < ((unsigned long)q->images[i].image.mh) +
				 q->images[i].image.vmaddr_size){
			library_image = q->images + i;
			goto down;
		    }
		}
	    }
	}
	/*
	 * This address is not for a library we have turned off the the
	 * protection for so return FALSE so our caller will know to forward
	 * the exception for this address on.
	 */
	if(library_image == NULL){
	    release_lock();
	    return(FALSE);
	}

down:
	/*
	 * Suspend all the threads in the task are but this one.  This can't
	 * be done for sure by the task itself as other threads could get
	 * started by the theads after the task_threads() call or injected
	 * into the task by another task.  So this is the best we can do.
	 */
	my_thread = mach_thread_self();
	r = task_threads(mach_task_self(), &threads, &thread_count);
	if(r != KERN_SUCCESS){
	    mach_error(r, "can't get thread list, task_threads() failed");
	    link_edit_error(DYLD_MACH_RESOURCE, r, NULL);
	}
	for(i = 0; i < thread_count; i++){
	    if(threads[i] != my_thread){
		r = thread_suspend(threads[i]);
		if(r != KERN_SUCCESS){
		    mach_error(r, "can't suppend threads, thread_suspend() "
			       "failed");
		    link_edit_error(DYLD_MACH_RESOURCE, r, NULL);
		}
	    }
	}

	/*
	 * Reset the initial protection of the segments which were writeable to
	 * back to writeable.
	 */
	load_commands = (struct load_command *)
		    ((char *)library_image->image.mh +
		     sizeof(struct mach_header));
	lc = load_commands;
	for(i = 0; i < library_image->image.mh->ncmds; i++){
	    switch(lc->cmd){
	    case LC_SEGMENT:
		sg = (struct segment_command *)lc;
		address = sg->vmaddr +
			  library_image->image.vmaddr_slide;
		if((sg->initprot & VM_PROT_WRITE) != 0 &&
		   address != (vm_address_t)library_image->image.mh &&
		   sg != library_image->image.linkedit_segment){
		    if((r = vm_protect(mach_task_self(), address,
			       (vm_size_t)sg->vmsize,
			       FALSE, sg->initprot)) !=
			       KERN_SUCCESS){
			mach_error(r, "can't vm_protect segment: "
			    "%.16s for library: %s", sg->segname,
			    library_image->image.name);
			link_edit_error(DYLD_MACH_RESOURCE, r,
			    library_image->image.name);
		    }
		}
		break;
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}

	/*
	 * Call the image init routine and clear the "to be called lazy" bit.
	 */
	if(library_image->image.rc != NULL &&
	   library_image->image.lazy_init == TRUE){

	    /* clear the indication this is to be called lazy */
	    library_image->image.lazy_init = FALSE;

	    /* mark this image as having its init routine called */
	    library_image->image.init_called = TRUE;

	    /* now actually call the init routine */
	    addr = library_image->image.rc->init_address +
		   library_image->image.vmaddr_slide;
	    init_routine = (void(*)(void))addr;

	    if(init_routine_being_called == FALSE)
		init_routine_being_called = TRUE;
	    else if(dyld_abort_multiple_inits == TRUE)
		abort();

	    release_lock();
	    init_routine();
	    set_lock();
	    init_routine_being_called = FALSE;

	    /*
	     * Call the module init routines for this library.
	     */
	    call_module_initializers_for_library(
		    library_image,
#ifdef __ppc__
		    fp_save_area,
		    vec_save_area,
		    &saved_regs,
#if !defined(__GONZO_BUNSEN_BEAKER__) && !defined(__HERA__)
		    &facilities_used,
#endif /* !defined(__GONZO_BUNSEN_BEAKER__) && !defined(__HERA__) */
#endif /* __ppc__ */
		    TRUE  /* make_delayed_calls */,
		    FALSE /* bind_now */);
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
	 * Resume all the threads in the task but this one.
	 */
	for(i = 0; i < thread_count; i++){
	    if(threads[i] != my_thread){
		r = thread_resume(threads[i]);
		if(r != KERN_SUCCESS){
		    mach_error(r, "can't resume threads, thread_resume() "
			       "failed");
		    link_edit_error(DYLD_MACH_RESOURCE, r, NULL);
		}
		r = mach_port_deallocate(mach_task_self(), threads[i]);
		if(r != KERN_SUCCESS){
		    mach_error(r, "can't deallocate port right, "
			"mach_port_deallocate() failed");
		    link_edit_error(DYLD_MACH_RESOURCE, r, NULL);
		}
	    }
	}
	r = vm_deallocate(mach_task_self(), (vm_address_t)threads,
			  sizeof(threads[0]) * thread_count);
	if(r != KERN_SUCCESS){
	    mach_error(r, "can't vm_deallocate threads list memory");
	    link_edit_error(DYLD_MACH_RESOURCE, r, NULL);
	}
	r = mach_port_deallocate(mach_task_self(), my_thread);
	if(r != KERN_SUCCESS){
	    mach_error(r, "can't deallocate port right, "
		"mach_port_deallocate() failed");
	    link_edit_error(DYLD_MACH_RESOURCE, r, NULL);
	}

	/*
	 * Release the dyld lock and return TRUE to our caller indicating we
	 * handled the exception and to just let the the thread causing the
	 * exception to continue.
	 */
	release_lock();
	return(TRUE);
}
#endif __MACH30__
