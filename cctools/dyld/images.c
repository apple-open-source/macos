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
#import <servers/bootstrap.h>
#import "_dyld_prebind.h"
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

#import "stuff/bool.h"
#import "trace.h"
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

/*
 * This library_image (and the image struct in it), the symbol and module are
 * used for weak libraries and weak symbols.
 */
const struct library_image some_weak_library_image = { { 0 } };
const struct nlist some_weak_symbol = {
    { 0 }, 	   /* n_un.strx */
    N_ABS | N_EXT, /* n_type */
    NO_SECT,       /* n_sect */
    0,             /* n_desc */
    0x0,           /* n_value */
};
/*
 * For some_weak_module to be usable it is a fully linked module, in the
 * FULLYBOUND STATE (0x80), that has had its MODINIT STATE (0x40) set, its
 * MODTERM STATE (0x20) set, and its IMAGE_INIT_DEPEND_STATE (0x10) state set
 */
const module_state some_weak_module = (module_state)(0xf0 | FULLY_LINKED);

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
 * The image_pointers for two-level namespace images are allocated from this
 * pool of image_pointers.  This is is to avoid malloc()'ing space for a
 * reasonable number images_pointers in the most likely cases.
TODO: the size for this needs to be picked.
 */
enum image_pointers_block_size { IMAGE_POINTER_BLOCK_SIZE = 10 };
struct image_pointers_block {
    unsigned long used;		/* the number of items used */
    struct image * image_pointers[IMAGE_POINTER_BLOCK_SIZE];
				/* the image pointers */
};
static struct image_pointers_block image_pointers_block;

static struct image ** allocate_image_pointers(
    unsigned long n);
static void reallocate_image_pointers(
    struct image **image_pointers,
    unsigned long old_size,
    unsigned long new_size);
static void deallocate_image_pointers(
    struct image **image_pointers,
    unsigned long n);

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
    unsigned long *short_name_size,
    enum bool with_underbar_suffix);
static char * get_library_name(
    char *name,
    unsigned long *short_name_size);
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
    ino_t ino,
    struct image **image_pointer,
    enum bool match_filename_by_installname,
    enum bool reference_from_dylib);
/*
 * In the rare case that some library filename is to be matched by install name
 * then some_libraries_match_filename_by_installname is set to TRUE and then
 * load_library_image() will call is_library_loaded_by_matching_installname().
 */
static enum bool some_libraries_match_filename_by_installname = FALSE;
static enum bool is_library_loaded_by_matching_installname(
    char *filename,
    struct dylib_command *dl,
    struct image **image_pointer);
static enum bool validate_library(
    char *dylib_name,
    struct dylib_command *dl,
    struct library_image *li,
    enum bool reference_from_dylib);
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
    struct twolevel_hints_command **hints_cmd,
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
    struct mach_header *mh,
    struct image *image);
static enum bool setup_sub_images(
    struct library_image *library_image);
static enum bool check_time_stamp(
    struct library_image *library_image,
    struct image *sub_image);
static void setup_umbrella_images(
    struct library_image *library_image,
    unsigned long max_libraries);
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
static void notify_prebinding_agent(void);
#ifdef SYSTEM_REGION_BACKED
/*
 * We do not cause the prebinding to be fixed if the program is not using the
 * system shared regions.  This information is returned in the flags of the
 * load_shared_file() call.  Once this information is determined then this
 * is set to true.  If notify_prebinding_agent() is called and this is not
 * yet set to TRUE then it will make a nop call to load_shared_file() to
 * determined if the system shared regions are being used.  If they are not
 * then it will return and do nothing.
 */
static enum bool determined_system_region_backed = FALSE;
#endif /* SYSTEM_REGION_BACKED */

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
    struct twolevel_hints_command *hints_cmd;
    struct object_image *object_image;
    enum bool change_protect_on_reloc, cache_sync_on_reloc,
	      has_coalesced_sections, seg1addr_found;
    char *dylib_name, *p, *passed_dylib_name;
#ifdef __ppc__
    unsigned long images_dyld_stub_binding_helper;

	images_dyld_stub_binding_helper =
	    (unsigned long)(&unlinked_lazy_pointer_handler);
#endif

	DYLD_TRACE_IMAGES_NAMED_START(DYLD_TRACE_load_executable_image, name);
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
	hints_cmd = NULL;
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
	    case LC_TWOLEVEL_HINTS:
		if(hints_cmd == NULL)
		    hints_cmd = (struct twolevel_hints_command *)lc;
		break;
	    /* this has the LC_REQ_DYLD bit in the command */
	    case LC_LOAD_WEAK_DYLIB:
		break;
	    default:
		if((lc->cmd & LC_REQ_DYLD) == LC_REQ_DYLD){
		    error("can't launch executable: %s (unknown load command "
			  "%ld (0x%x) required for execution)", name, i,
			  (unsigned int)(lc->cmd));
		    link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
		}
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
	object_image->image.physical_name = object_image->image.name;
	executables_name = object_image->image.name;
	object_image->image.vmaddr_slide = 0;
	/* note the executable is not always contiguious in memory and should
	   not be deallocated using vmaddr_size */
	object_image->image.vmaddr_size = 0;
	object_image->image.seg1addr = seg1addr;
	object_image->image.mh = mh;
	object_image->image.st = st;
	object_image->image.dyst = dyst;
	object_image->image.hints_cmd = hints_cmd;
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
	object_image->image.outer_image = object_image;
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
		all_twolevel_modules_prebound = FALSE;
		passed_dylib_name = allocate(strlen(dylib_name) + 1);
		strcpy(passed_dylib_name, dylib_name);
		(void)load_library_image(NULL, passed_dylib_name, FALSE, FALSE,
					 NULL, NULL, FALSE);
		if(p == NULL){
		    break;
		}
		else{
		    /*
		     * Do not put colons put back in the string as we are using
		     * a pointer to it as the dylib_name.  Note that
		     * dyld_insert_libraries is a copy of the environment string
		     * as made in pickup_enviroment_strings().
		     */
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
	(void)load_images_libraries(mh, &(object_image->image));
	loading_executables_libraries = FALSE;

	/*
	 * Load the dependent libraries.
	 */
	load_dependent_libraries();

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
	if(dyld_hints_debug != 0)
	    printf("image_can_use_hints = %s for image: %s\n",
		   object_image->image.image_can_use_hints == TRUE ?
		   "TRUE" : "FALSE", object_image->image.name);

	/*
	 * Call the routine that gdb might have a break point on to let it
	 * know it is time to re-read the internal dyld structures as defined
	 * by <mach-o/dyld_gdb.h>
	 */
	gdb_dyld_state_changed();
	
	DYLD_TRACE_IMAGES_END(DYLD_TRACE_load_executable_image);

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
    unsigned long size;

	if(path == NULL){
	    return(NULL);
	}
	new_dylib_name = NULL;
	if((name = get_framework_name(dylib_name, &size, FALSE)) != NULL){
	    new_dylib_name = search_for_name_in_path(name, path, suffix);
	}
	if(new_dylib_name == NULL){
	    if((name = get_framework_name(dylib_name, &size, TRUE)) != NULL){
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
    int r;

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
	r = stat(new_dylib_name, &stat_buf);
	if(r == 0 && (stat_buf.st_mode & S_IFMT) == S_IFREG){
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
    int r;

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
		r = stat(constructed_name, &stat_buf);
		if(r == 0 && (stat_buf.st_mode & S_IFMT) == S_IFREG){
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
 * Specifically this routine takes a pointer to a dylib_command for a library
 * and finds and opens the file corresponding to it (or if that is NULL then
 * the specified passed_dylib_name).  It deals with the file being fat and then
 * calls map_library_image() to have the library's segments mapped into memory.
 * For two-level images that depend on this library, image_pointer is the
 * address of a pointer to an image struct to be set (if not NULL) after this
 * image is loaded.  If passed_dylib_name is not NULL it is always free()'ed
 * before returning.
 */
enum bool
load_library_image(
struct dylib_command *dl, /* allow NULL for NSAddLibrary() to use this */
char *passed_dylib_name,
enum bool force_searching,
enum bool match_filename_by_installname,
struct image **image_pointer,
enum bool *already_loaded,
enum bool reference_from_dylib)
{
    char *dylib_name, *new_dylib_name, *constructed_name;
    int fd, errnum, save_errno;
    struct stat stat_buf;
    unsigned long file_size;
    char *file_addr;
    kern_return_t r;
    struct fat_header *fat_header;
    struct fat_arch *fat_archs, *best_fat_arch;
    struct mach_header *mh;
	enum bool return_value;

	new_dylib_name = NULL;
	constructed_name = NULL;
	if(dl != NULL)
	    dylib_name = (char *)dl + dl->dylib.name.offset;
	else
	    dylib_name = passed_dylib_name;
        if(already_loaded != NULL)
	    *already_loaded = FALSE;
        DYLD_TRACE_IMAGES_NAMED_START(DYLD_TRACE_load_library_image,dylib_name);

	/*
	 * If there has previously been a library loaded with
	 * match_filename_by_installname set and this is a library coming from
	 * an image see if the filename for this library matches an install name
	 * for any library marked with match_filename_by_installname.
	 */
	if(some_libraries_match_filename_by_installname == TRUE && dl != NULL){
	    if(is_library_loaded_by_matching_installname(dylib_name, dl,
		image_pointer) == TRUE){
		DYLD_TRACE_IMAGES_END(DYLD_TRACE_load_library_image);
		if(already_loaded != NULL)
		    *already_loaded = TRUE;
		return(TRUE);
	    }
	}
        
	/*
	 * If the dylib_command is not NULL then this is not a result of a call
	 * to NSAddLibrary() so searching may take place.  Otherwise, just open
	 * the name passed in.
	 */
	if(dl != NULL || force_searching == TRUE){
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
	if(is_library_loaded_by_name(dylib_name, dl, image_pointer,
				     reference_from_dylib) == TRUE){
            DYLD_TRACE_IMAGES_END(DYLD_TRACE_load_library_image);
	    if(passed_dylib_name != NULL)
		free(passed_dylib_name);
	    if(new_dylib_name != NULL)
		free(new_dylib_name);
	    if(constructed_name != NULL)
		free(constructed_name);
	    if(already_loaded != NULL)
		*already_loaded = TRUE;
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
	    if(new_dylib_name != NULL){
		dylib_name = new_dylib_name;
		fd = open(dylib_name, O_RDONLY, 0);
	    }
	    else{
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
	    if(dl != NULL && dl->cmd == LC_LOAD_WEAK_DYLIB){
		if(image_pointer != NULL)
		    *image_pointer =
			(struct image *)&(some_weak_library_image.image);
	    }
	    else{
		system_error(errnum, "can't open library: %s ", dylib_name);
		link_edit_error(DYLD_FILE_ACCESS, errnum, dylib_name);
	    }
            DYLD_TRACE_IMAGES_END(DYLD_TRACE_load_library_image);
	    if(passed_dylib_name != NULL)
		free(passed_dylib_name);
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
	if(is_library_loaded_by_stat(dylib_name, dl, &stat_buf, image_pointer,
				     reference_from_dylib) == TRUE){
	    close(fd);
            DYLD_TRACE_IMAGES_END(DYLD_TRACE_load_library_image);
	    if(passed_dylib_name != NULL)
		free(passed_dylib_name);
	    if(new_dylib_name != NULL)
		free(new_dylib_name);
	    if(constructed_name != NULL)
		free(constructed_name);
	    if(already_loaded != NULL)
		*already_loaded = TRUE;
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
	    return_value = map_library_image(dl, dylib_name, fd, file_addr,
				     file_size, best_fat_arch->offset,
				     best_fat_arch->size, stat_buf.st_dev,
				     stat_buf.st_ino, image_pointer,
				     match_filename_by_installname,
				     reference_from_dylib);
	    if(return_value == FALSE)
		goto load_library_image_cleanup3;
            DYLD_TRACE_IMAGES_END(DYLD_TRACE_load_library_image);
            return(return_value);

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
	    return_value = map_library_image(dl, dylib_name, fd, file_addr,
				     file_size, 0, file_size, stat_buf.st_dev,
				     stat_buf.st_ino, image_pointer,
				     match_filename_by_installname,
				     reference_from_dylib);
	    if(return_value == FALSE)
		goto load_library_image_cleanup3;
	    DYLD_TRACE_IMAGES_END(DYLD_TRACE_load_library_image);
	    return(return_value);

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
load_library_image_cleanup3:
        DYLD_TRACE_IMAGES_END(DYLD_TRACE_load_library_image);
	if(passed_dylib_name != NULL)
	    free(passed_dylib_name);
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
 * try to parse off a trailing suffix starting with an underbar.  The size of
 * the short name (that is "Foo" for this example) is returned indirectly
 * through short_name_size.
 */
static
char *
get_framework_name(
char *name,
unsigned long *short_name_size,
enum bool with_underbar_suffix)
{
    char *foo, *a, *b, *c, *d, *suffix;
    unsigned long l, s;

	*short_name_size = 0;
	/* pull off the last component and make foo point to it */
	a = strrchr(name, '/');
	if(a == NULL)
	    return(NULL);
	if(a == name)
	    return(NULL);
	foo = a + 1;
	l = strlen(foo);
	*short_name_size = l;
	
	/* look for suffix if requested starting with a '_' */
	if(with_underbar_suffix){
	    suffix = strrchr(foo, '_');
	    if(suffix != NULL){
		s = strlen(suffix);
		if(suffix == foo || s < 2)
		    suffix = NULL;
		else{
		    l -= s;
		    *short_name_size = l;
		}
	    }
	}

	/* first look for the form Foo.framework/Foo */
	b = look_back_for_slash(name, a);
	if(b == NULL){
	    if(strncmp(name, foo, l) == 0 &&
	       strncmp(name + l, ".framework/", sizeof(".framework/")-1 ) == 0)
		return(name);
	    else{
		*short_name_size = 0;
		return(NULL);
	    }
	}
	else{
	    if(strncmp(b+1, foo, l) == 0 &&
	       strncmp(b+1 + l, ".framework/", sizeof(".framework/")-1 ) == 0)
		return(b+1);
	}

	/* next look for the form Foo.framework/Versions/A/Foo */
	if(b == name){
	    *short_name_size = 0;
	    return(NULL);
	}
	c = look_back_for_slash(name, b);
	if(c == NULL ||
	   c == name ||
	   strncmp(c+1, "Versions/", sizeof("Versions/")-1) != 0){
	    *short_name_size = 0;
	    return(NULL);
	}
	d = look_back_for_slash(name, c);
	if(d == NULL){
	    if(strncmp(name, foo, l) == 0 &&
	       strncmp(name + l, ".framework/", sizeof(".framework/")-1 ) == 0)
		return(name);
	    else{
		*short_name_size = 0;
		return(NULL);
	    }
	}
	else{
	    if(strncmp(d+1, foo, l) == 0 &&
	       strncmp(d+1 + l, ".framework/", sizeof(".framework/")-1 ) == 0)
		return(d+1);
	    else{
		*short_name_size = 0;
		return(NULL);
	    }
	}
}

/*
 * get_library_name() is passed a name of a dynamic library and returns a
 * pointer to the start of the library name if one exist or NULL none exists.
 * The name of the dynamic library is recognized as a library name if it has
 * one of the two following forms (with any preceding directory name):
 *	libFoo.A.dylib
 *	libFoo.dylib
 * The library may have a suffix trailing the name Foo of the form:
 *	libFoo_profile.A.dylib
 *	libFoo_profile.dylib
 * The pointer returned in the above case is to "libFoo" and the size of
 * the short name (that is "libFoo" for this example) is returned indirectly
 * through short_name_size.
 */
static
char *
get_library_name(
char *name,
unsigned long *short_name_size)
{
    char *a, *b, *c;
    enum bool saw_version_letter;

	*short_name_size = 0;
	/* pull off the suffix after the "." and make a point to it */
	a = strrchr(name, '.');
	if(a == NULL)
	    return(NULL);
	if(a == name)
	    return(NULL);
	if(strcmp(a, ".dylib") != 0)
	    return(NULL);

	/* first pull off the version letter for the form Foo.A.dylib */
	if(a - name >= 3 && a[-2] == '.'){
	    a = a - 2;
	    saw_version_letter = TRUE;
	}
	else{
	    saw_version_letter = FALSE;
	}

	b = look_back_for_slash(name, a);
	if(b == name)
	    return(NULL);
	if(b == NULL){
	    /* ignore any suffix after an underbar
	       like Foo_profile.A.dylib */
	    c = strchr(name, '_');
	    if(c != NULL && c != name){
		*short_name_size = c - name;
	    }
	    else
		*short_name_size = a - name;
	    /* there are incorrect library names of the form:
	       libSystem.B_debug.dylib so check for these */
	    if(saw_version_letter == FALSE &&
	       *short_name_size >= 3 && name[*short_name_size - 2] == '.')
		    *short_name_size -= 2;
	    return(name);
	}
	else{
	    /* ignore any suffix after an underbar
	       like Foo_profile.A.dylib */
	    c = strchr(b+1, '_');
	    if(c != NULL && c != b+1){
		*short_name_size = c - (b+1);
	    }
	    else
		*short_name_size = a - (b+1);
	    /* there are incorrect library names of the form:
	       libSystem.B_debug.dylib so check for these */
	    if(saw_version_letter == FALSE &&
	       *short_name_size >= 3 && b[*short_name_size - 1] == '.')
		    *short_name_size -= 2;
	    return(b+1);
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
    int r;

	dylib_name = allocate(strlen(name) + strlen(path) + 2);
	for(;;){
	    p = strchr(path, ':');
	    if(p != NULL)
		*p = '\0';
	    else{
		if(*path == '\0')
		    return(NULL);
	    }
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
	    r = stat(dylib_name, &stat_buf);
	    if(r == 0 && (stat_buf.st_mode & S_IFMT) == S_IFREG){
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
 * adds the library to the list of library images.  For two-level images that
 * depend on this library, image_pointer is the address of a pointer to an
 * image struct to be set (if not NULL) after if this image is loaded.
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
ino_t ino,
struct image **image_pointer,
enum bool match_filename_by_installname,
enum bool reference_from_dylib)
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
    struct twolevel_hints_command *hints_cmd;
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
	DYLD_TRACE_IMAGES_NAMED_START(DYLD_TRACE_map_library_image, dylib_name);
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
	    &linkedit_segment, &dyst, &st, &dlid, &rc, &hints_cmd,
	    &change_protect_on_reloc, &cache_sync_on_reloc,
	    &has_coalesced_sections, &init, &term, &seg1addr,
	    &segs_read_write_addr, &slide_value,
	    &images_dyld_stub_binding_helper) == FALSE){

            DYLD_TRACE_IMAGES_END(DYLD_TRACE_map_library_image);
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
	library_image->image.physical_name = library_image->image.name;
	library_image->image.vmaddr_slide = slide_value;
	library_image->image.vmaddr_size = high_addr - low_addr;
	library_image->image.seg1addr = seg1addr;
	library_image->image.segs_read_write_addr = segs_read_write_addr;
	library_image->image.mh = mh;
	library_image->image.st = st;
	library_image->image.dyst = dyst;
	library_image->image.rc = rc;
	library_image->image.hints_cmd = hints_cmd;
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
	library_image->image.sub_images_setup = FALSE;
	library_image->image.umbrella_images_setup = FALSE;
	if(image_pointer != NULL)
	    *image_pointer = &(library_image->image);
	library_image->image.match_filename_by_installname =
			     match_filename_by_installname;
	if(match_filename_by_installname)
	    some_libraries_match_filename_by_installname = TRUE;
	/*
	 * Since we can't know if the name may have an underbar in it first
	 * try to get the name without a suffix then if that fails try it
	 * with a suffix.
	 */
	library_image->image.umbrella_name = get_framework_name(
		(char *)dlid + dlid->dylib.name.offset,
		&(library_image->image.name_size),
		FALSE);
	if(library_image->image.umbrella_name == NULL)
	    library_image->image.umbrella_name = get_framework_name(
		    (char *)dlid + dlid->dylib.name.offset,
		    &(library_image->image.name_size),
		    TRUE);
	if(library_image->image.umbrella_name == NULL)
	    library_image->image.library_name = get_library_name(
		    (char *)dlid + dlid->dylib.name.offset,
		    &(library_image->image.name_size));
	library_image->image.outer_image = library_image;
	library_image->image.valid = TRUE;

	/*
	 * Do local relocation if this library was slid.  This also disables 
	 * prebinding and undoes the prebinding
	 */
	if(slide_value != 0){
	    if(prebinding == TRUE && launched == FALSE){
		if(dyld_prebind_debug != 0)
		    print("dyld: %s: prebinding disabled because library: %s "
			  "got slid\n", executables_name, dylib_name);
		if((mh->flags & MH_PREBOUND) == MH_PREBOUND)
		    notify_prebinding_agent();
	    }
	    if(launched == FALSE)
		prebinding = FALSE;
	    all_twolevel_modules_prebound = FALSE;
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
	    all_twolevel_modules_prebound = FALSE;
	}
	else{
	    /*
	     * The library is prebound.  If we are forcing flat name space and
	     * this is a two-level namespace library then disable prebinding.
	     */
	    if(force_flat_namespace == TRUE &&
	       (mh->flags & MH_TWOLEVEL) == MH_TWOLEVEL){
		if(dyld_prebind_debug != 0 &&
		   prebinding == TRUE &&
		   launched == FALSE)
		    print("dyld: %s: prebinding disabled because library: %s "
			  "is two-level namespace and flat namespace is being "
			  "used\n", executables_name, dylib_name);
		if(launched == FALSE)
		    prebinding = FALSE;
		all_twolevel_modules_prebound = FALSE;
	    }
	    /*
	     * The library is prebound.  If we have already launched the
	     * program we can't use the prebinding and it must be undone.
	     */
	    if(launched == TRUE){
		/*
		 * This library is prebound but we have already launched. We can
		 * still try to use the prebinding if all the previously loaded
		 * libraries were two-level and all the modules were prebound if
		 * this library is also two-level.
		 */
		if(all_twolevel_modules_prebound == TRUE &&
	           (mh->flags & MH_TWOLEVEL) == MH_TWOLEVEL){
		    trying_to_use_prebinding_post_launch = TRUE;
		    library_image->image.trying_to_use_prebinding_post_launch =
			TRUE;
		}
		else
		    undo_prebinding_for_library(library_image);
	    }
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
		    if((mh->flags & MH_PREBOUND) == MH_PREBOUND)
			notify_prebinding_agent();
		    prebinding = FALSE;
		}
	    }
	    if(reference_from_dylib == TRUE)
		all_twolevel_modules_prebound = FALSE;
	}

	/*
	 * If this library is not a two-level namespace library the that too
	 * causes all_twolevel_modules_prebound to be set to FALSE.
	 */
	if((mh->flags & MH_TWOLEVEL) != MH_TWOLEVEL)
	    all_twolevel_modules_prebound = FALSE;

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

	DYLD_TRACE_IMAGES_END(DYLD_TRACE_map_library_image);
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
	DYLD_TRACE_IMAGES_END(DYLD_TRACE_map_library_image);
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
char *physical_name,
char *object_addr,
unsigned long object_size)
{
    struct mach_header *mh;
    unsigned long low_addr, high_addr, slide_value, seg1addr,
		  segs_read_write_addr;
    struct segment_command *linkedit_segment, *mach_header_segment;
    struct dysymtab_command *dyst;
    struct symtab_command *st;
    struct twolevel_hints_command *hints_cmd;
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
	DYLD_TRACE_IMAGES_NAMED_START(DYLD_TRACE_map_bundle_image, name);
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
            DYLD_TRACE_IMAGES_END(DYLD_TRACE_map_bundle_image);
	    return(NULL);
	}
	if(sizeof(struct mach_header) > object_size){
	    error("truncated or malformed object file image: %s (too small to "
		"be an object file image)", name);
	    link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
            DYLD_TRACE_IMAGES_END(DYLD_TRACE_map_bundle_image);
	    return(NULL);
	}
	mh = (struct mach_header *)object_addr;
	if(mh->magic != MH_MAGIC){
	    error("malformed object file image: %s (not a Mach-O image, bad "
		"magic number)", name);
	    link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
            DYLD_TRACE_IMAGES_END(DYLD_TRACE_map_bundle_image);
	    return(NULL);
	}
	/*
	 * Now that it looks like it could be an object file image check it.
	 */
	if(check_image(name, "object file image", object_size, mh,
	    &linkedit_segment, &mach_header_segment, &dyst, &st, NULL, NULL,
	    &low_addr, &high_addr) == FALSE){
            DYLD_TRACE_IMAGES_END(DYLD_TRACE_map_bundle_image);
	    return(NULL);
	}
	/*
	 * Do the bundle specific check on the mach header.
	 */
	if(mh->filetype != MH_BUNDLE){
	    error("malformed object file image: %s (not a Mach-O bundle file, "
		"bad filetype value)", name);
	    link_edit_error(DYLD_FILE_FORMAT, EBADMACHO, name);
            DYLD_TRACE_IMAGES_END(DYLD_TRACE_map_bundle_image);
	    return(NULL);
	}

	/*
	 * Now that the object file image checks out map it in.
	 */
	if(map_image(name, "object file image", object_size, -1, NULL, 
	    0, 0, 0, low_addr, high_addr, &mh, &linkedit_segment, &dyst, &st,
	    NULL, NULL, &hints_cmd, &change_protect_on_reloc,
	    &cache_sync_on_reloc, &has_coalesced_sections, &init, &term,
	    &seg1addr, &segs_read_write_addr, &slide_value,
	    &images_dyld_stub_binding_helper) == FALSE){

            DYLD_TRACE_IMAGES_END(DYLD_TRACE_map_bundle_image);
	    return(NULL);
	}

	/*
	 * This object file image is now successfully mapped in add it to the
	 * list of object image loaded.
	 */
	object_image = new_object_image();
	object_image->image.name = save_string(name);
	if(physical_name != name)
	    object_image->image.physical_name = save_string(physical_name);
	else
	    object_image->image.physical_name = object_image->image.name;
	object_image->image.vmaddr_slide = slide_value;
	object_image->image.vmaddr_size = high_addr - low_addr;
	object_image->image.seg1addr = seg1addr;
	object_image->image.segs_read_write_addr = segs_read_write_addr;
	object_image->image.mh = mh;
	object_image->image.st = st;
	object_image->image.dyst = dyst;
	object_image->image.hints_cmd = hints_cmd;
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
	object_image->image.outer_image = object_image;
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
	if(load_images_libraries(mh, &(object_image->image)) == FALSE &&
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
            DYLD_TRACE_IMAGES_END(DYLD_TRACE_map_bundle_image);
	    return(NULL);
	}

	/*
	 * Return the module.
	 */
	DYLD_TRACE_IMAGES_END(DYLD_TRACE_map_bundle_image);
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
	    check_and_report_undefineds(FALSE);
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
	 */
	if(object_image->image.name != object_image->image.physical_name)
	    unsave_string(object_image->image.physical_name);
	unsave_string(object_image->image.name);
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
struct twolevel_hints_command **hints_cmd,
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

	DYLD_TRACE_IMAGES_NAMED_START(DYLD_TRACE_map_image, name);

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
		    if(sg->fileoff == 0 && sg->filesize != 0 &&
		       mach_header_segment_vmaddr == 0)
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
#ifdef SYSTEM_REGION_BACKED
	    if(determined_system_region_backed == FALSE &&
	       (flags & SYSTEM_REGION_BACKED) != SYSTEM_REGION_BACKED){
		dyld_no_fix_prebinding = TRUE;
		if(dyld_prebind_debug != 0)
		    print("dyld: in map_image() determined the "
			  "system shared regions are NOT used\n");
	    }
	    else{
		if(determined_system_region_backed == FALSE &&
		   dyld_prebind_debug != 0)
		    print("dyld: in map_image() determined the "
			  "system shared regions ARE used\n");
	    }
	    determined_system_region_backed = TRUE;
#endif /* SYSTEM_REGION_BACKED */
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
            DYLD_TRACE_IMAGES_END(DYLD_TRACE_map_image);
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
		if(sg->fileoff == 0 && sg->filesize != 0 &&
		   mach_header_segment_vmaddr == 0)
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
	if(hints_cmd != NULL)
	    *hints_cmd = NULL;
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
	    case LC_TWOLEVEL_HINTS:
		if(hints_cmd != NULL && *hints_cmd == NULL)
		    *hints_cmd = (struct twolevel_hints_command *)lc;
		break;
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
/*
if(*linkedit_segment != NULL)
printf("name = %s *linkedit_segment = 0x%x\n", name, *linkedit_segment);
*/
	DYLD_TRACE_IMAGES_END(DYLD_TRACE_map_image);
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
	DYLD_TRACE_IMAGES_END(DYLD_TRACE_map_image);
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
 * With this defined it adds some checks to the code that trys to use subtrees
 * of two-level namespace prebound libraries.  It should not be on for
 * production builds.
 */
#define SANITY_CHECK_TWOLEVEL_PREBOUND

/*
 * all_dependents_twolevel_prebound() determines whether lib and all
 * dependents can be used as two-level and prebound libraries.
 */
static
enum bool
all_dependents_twolevel_prebound(
struct library_image *const lib)
{
    const struct dylib_command *dl;
    const struct load_command *lc;
    struct library_image *dep_lib;
    struct image *dep_image;
    const char *dep_name;
    unsigned long i, j;

#ifdef SANITY_CHECK_TWOLEVEL_PREBOUND
	if(lib->image.subtrees_twolevel_prebound_setup == TRUE){
	    /* should never get here */
	    print("dyld: internal error calc_twolevel_prebound_dependency() "
		  "called for: %s who's two-level prebound state is already "
		  "setup", lib->image.name);

	    return(lib->image.subtrees_twolevel_prebound);
	}
#endif

	/*
	 * Avoid wacky circular dependencies by setting the
	 * "setup" bit to TRUE and the twolevel-prebound state
	 * to FALSE to indicate it "can't be used twolevel-prebound".
	 */
	lib->image.subtrees_twolevel_prebound_setup = TRUE;
	lib->image.subtrees_twolevel_prebound = FALSE;

	if(dyld_prebind_debug > 2)
	    print("dyld: checking: %s and dependents\n", lib->image.name);

	/*
	 * Run through dependents of lib, marking them as appropriate.
	 */
	lc = (struct load_command *)
	     ((char *)lib->image.mh + sizeof(struct mach_header));
	j = 0;
	for(i = 0; i < lib->image.mh->ncmds; i++){
	    if(lc->cmd == LC_LOAD_DYLIB || lc->cmd == LC_LOAD_WEAK_DYLIB){
		dl = (struct dylib_command *)lc;
		dep_name = (char *)dl + dl->dylib.name.offset;
		dep_image = lib->image.dependent_images[j++];
		dep_lib = (struct library_image *)(dep_image->outer_image);
		/*
		 * If dep_lib has a different date from that recorded in the
		 * dylib load command, bail out.
		 */
		if(dl->dylib.timestamp != dep_lib->dlid->dylib.timestamp){
		    if(dyld_prebind_debug > 2)
			print("dyld: %s timestamp differs from the "
			      "load_cmd's\n", lib->image.name);
		    return(FALSE);
		}
		else if(dep_image->subtrees_twolevel_prebound_setup){
		    /*
		     * If we already know that this subtree can't be used
		     * twolevel-prebound, bail out.
		     */
		    if(dep_image->subtrees_twolevel_prebound == FALSE)
			return(FALSE);
		}
		else{
		    /*
		     * Check all of dep_lib's dependent libs.
		     */
		    dep_image->subtrees_twolevel_prebound =
			all_dependents_twolevel_prebound(dep_lib);

		    /*
		     * If dep_lib cannot be twolevel-prebound, neither can we.
		     */
		    if(dep_image->subtrees_twolevel_prebound == FALSE)
			return(FALSE);
		}
#ifdef SANITY_CHECK_TWOLEVEL_PREBOUND
		/*
		 * Ensure that DEP_LIB's subtree is good.
		 */
		if(dep_image->subtrees_twolevel_prebound == FALSE)
		    print("dyld: internal error: lib %s twolevel_prebound state"
			  " is FALSE!", dep_image->name);
#endif
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}

	/*
	 * Reaching here means that this subtree (lib and all its dependents)
	 * can be used twolevel-prebound.
	 */
	lib->image.subtrees_twolevel_prebound = TRUE;

	if(dyld_prebind_debug > 2)
	    print("  --> %s and its dependents are OK\n", lib->image.name);

	return(TRUE);
}

/*
 * find_twolevel_prebound_lib_subtrees() walks the dependency graph of all the
 * libraries looking for subtrees which are two-level prebound, not slid and
 * all the dependent timestamps match.
 */
void
find_twolevel_prebound_lib_subtrees(
void)
{
    struct library_images *q;
    struct library_image *l;
    unsigned long i, total, used;

	if(dyld_prebind_debug > 2)
	    print("dyld: In find_twolevel_prebound_lib_subtrees()\n");

	/*
	 * Do trivial weeding out of any library that is not two-level,
	 * prebound and at the correct address.
	 */
	q = &library_images;
	do{
	    for(i = 0; i < q->nimages; i++){
		l = &q->images[i];
		if((l->image.mh->flags & (MH_TWOLEVEL | MH_PREBOUND)) !=
					 (MH_TWOLEVEL | MH_PREBOUND) ||
		    l->image.vmaddr_slide != 0){

		    l->image.subtrees_twolevel_prebound = FALSE;
		    l->image.subtrees_twolevel_prebound_setup = TRUE;

		    if(dyld_prebind_debug > 2){
			print("dyld: **** '%s' ", l->image.name);
			if(l->image.vmaddr_slide != 0)
			    print("slid ****\n");
			else
			    print("not twolevel-prebound ****\n");
		    }
		}
	    }
	    q = q->next_images;
	}while(q != NULL);

	/*
	 * See which libs have the time stamps of all the dependents that match
	 * so we can use them twolevel-prebound.
	 */
	q = &library_images;
	do{
	    for(i = 0; i < q->nimages; i++){
		l = &q->images[i];
		if(l->image.subtrees_twolevel_prebound_setup == FALSE){
		    /*
		     * See if all dependents are twolevel-prebound OK.
		     * Note that all_dependents_twolevel_prebound() sets
		     * the "subtrees_twolevel_prebound_setup" field (as
		     * well as "subtrees_twolevel_prebound", which means
		     * we don't need to set it here, but it looks nicer :-)
		     */
		    l->image.subtrees_twolevel_prebound =
				all_dependents_twolevel_prebound(l);
		}
	    }
	    q = q->next_images;
	}while(q != NULL);

	/*
	 * Show the names of those libs we CAN use prebound, if debugging.
	 * While we're there, check to make sure we've visited every lib.
	 */
	if(dyld_prebind_debug != 0){
	    total = 0;
	    used = 0;
	    q = &library_images;
	    do{
		for(i = 0; i < q->nimages; ++i){
		    l = &q->images[i];

#ifdef SANITY_CHECK_TWOLEVEL_PREBOUND
		    if(l->image.subtrees_twolevel_prebound_setup == FALSE){
			print("dyld: internal error two-level prebound state "
			      "of lib: %s not setup\n", l->image.name);
		    }
#endif
		    total++;
		    if(l->image.subtrees_twolevel_prebound == TRUE){
			used++;
			if(dyld_prebind_debug > 1)
			    print("dyld: using: %s as two-level prebound\n",
				    q->images[i].image.name);
		    }
		    else{
			if(dyld_prebind_debug > 1)
			    print("dyld: NOT using: %s as two-level prebound\n",
				    q->images[i].image.name);
		    }
		}
		q = q->next_images;
	    }while(q != NULL);
	    print("dyld: %lu two-level prebound libraries used out of %ld\n",
		  used, total);
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
 *
 * Once all the needed libraries are loaded then the sub_images and
 * umbrella_images lists are set up for those two-level namespace libraries
 * that need them set up.
 */
enum bool
load_dependent_libraries(
void)
{
    unsigned long i, max_libraries;
    struct library_images *q;
    enum bool some_images_setup;
		
	DYLD_TRACE_IMAGES_START(DYLD_TRACE_load_dependent_libraries);

	q = &library_images;
	do{
	    for(i = 0; i < q->nimages; i++){
		if(q->images[i].dependent_libraries_loaded == FALSE){
		    if(dyld_print_libraries == TRUE)
			print("loading libraries for image: %s\n",
			       q->images[i].image.name);
		    if(load_images_libraries(q->images[i].image.mh,
					     &(q->images[i].image)) == FALSE &&
		       return_on_error == TRUE){
			DYLD_TRACE_IMAGES_END(
			    DYLD_TRACE_load_dependent_libraries);
			return(FALSE);
		    }
		    q->images[i].dependent_libraries_loaded = TRUE;
		}
	    }
	    q = q->next_images;
	}while(q != NULL);

	/*
	 * To support the "primary" library concept each image that has
	 * sub-frameworks and sub-umbrellas has a sub_images list created for
	 * it for other libraries to search in for symbol names.
	 *
	 * To be able to bind a symbol pointers for symbols in a sub_image of
	 * an umbrella each sub_image has a list of umbrella images that it is
	 * a part of.
	 *
	 * These lists are set up after all the dependent libraries are loaded
	 * in the loops above.
	 */
	if(force_flat_namespace == FALSE){
	    /*
	     * Now with all the libraries loaded and the dependent_images set up
	     * set up the sub_images for any library that does not have this set
	     * up yet.  Since sub_images include sub_umbrellas any image that
	     * has sub_umbrellas must have the sub_umbrella images set up first.
	     * To do this setup_sub_images() will return FALSE for an image that
	     * needed one of its sub_umbrellas set up and we will loop here
	     * until we get a clean pass with no more images needing setup.
	     */
	    do{
		some_images_setup = FALSE;
		q = &library_images;
		do{
		    for(i = 0; i < q->nimages; i++){
			if(q->images[i].image.sub_images_setup == FALSE){
			    some_images_setup |=
				    setup_sub_images(&(q->images[i]));
			}
		    }
		    q = q->next_images;
		}while(q != NULL);
	    }while(some_images_setup == TRUE);
	    /*
	     * TODO: will adding a loop here checking for images not set up
	     * catch circular sub_umbrella loops?  Or is it even possible to
	     * build such a thing?
	     */

	    /*
	     * Now with all the sub_images set up set up the umbrella_images for
	     * any library that does not have this set up yet.
	     */
	    max_libraries = 0;
	    q = &library_images;
	    do{
		max_libraries += q->nimages;
		q = q->next_images;
	    }while(q != NULL);

	    q = &library_images;
	    do{
		for(i = 0; i < q->nimages; i++){
		    if(q->images[i].image.umbrella_images_setup == FALSE){
			setup_umbrella_images(&(q->images[i]), max_libraries);
		    }
		}
		q = q->next_images;
	    }while(q != NULL);

	    /*
	     * If DYLD_TWO_LEVEL_DEBUG is set print out the lists.
	     */
	    if(dyld_two_level_debug == TRUE){
		static enum bool list_printed = FALSE;

		if(list_printed == FALSE){
		    printf("Library order\n");
		    q = &library_images;
		    do{
			for(i = 0; i < q->nimages; i++){
			    if(q->images[i].image.umbrella_name != NULL)
			       printf("\t%.*s\n",
				  (int)q->images[i].image.name_size,
				  q->images[i].image.umbrella_name);
			    else if(q->images[i].image.library_name != NULL)
			       printf("\t%.*s\n",
				  (int)q->images[i].image.name_size,
				  q->images[i].image.library_name);
			    else
			       printf("\t%s\n", q->images[i].image.name);
			}
			q = q->next_images;
		    }while(q != NULL);
		    list_printed = TRUE;
		}

		q = &library_images;
		do{
		    for(i = 0; i < q->nimages; i++){
			unsigned long j;
			struct image **sp;

			if(q->images[i].image.two_level_debug_printed == FALSE){
			    printf("library: %s",
				   q->images[i].image.name);
			    if(q->images[i].image.umbrella_name != NULL)
				printf(" umbrella_name = %.*s\n",
				   (int)(q->images[i].image.name_size),
				   q->images[i].image.umbrella_name);
			    else if(q->images[i].image.library_name != NULL)
				printf(" library_name = %.*s\n",
				   (int)(q->images[i].image.name_size),
				   q->images[i].image.library_name);
			    else
				printf(" umbrella & library name = NULL\n");

			    printf("    ndependent_images = %lu\n",
				   q->images[i].image.ndependent_images);
			    sp = q->images[i].image.dependent_images;
			    for(j = 0;
				j < q->images[i].image.ndependent_images;
				j++){
				if(sp[j]->umbrella_name != NULL)
				   printf("\t[%lu] %.*s\n", j,
					  (int)sp[j]->name_size,
					  sp[j]->umbrella_name);
				else if(sp[j]->library_name != NULL)
				   printf("\t[%lu] %.*s\n", j,
					  (int)sp[j]->name_size,
					  sp[j]->library_name);
				else
				   printf("\t[%lu] %s\n", j, sp[j]->name);
			    }

			    printf("    nsub_images = %lu\n",
				   q->images[i].image.nsub_images);
			    sp = q->images[i].image.sub_images;
			    for(j = 0; j < q->images[i].image.nsub_images; j++){
				if(sp[j]->umbrella_name != NULL)
				   printf("\t[%lu] %.*s\n", j,
					  (int)sp[j]->name_size,
					  sp[j]->umbrella_name);
				else if(sp[j]->library_name != NULL)
				   printf("\t[%lu] %.*s\n", j,
					  (int)sp[j]->name_size,
					  sp[j]->library_name);
				else
				   printf("\t[%lu] %s\n", j, sp[j]->name);
			    }

			    printf("    numbrella_images = %lu\n",
				   q->images[i].image.numbrella_images);
			    sp = q->images[i].image.umbrella_images;
			    for(j = 0;
				j < q->images[i].image.numbrella_images;
				j++){
				if(sp[j]->umbrella_name != NULL)
				   printf("\t[%lu] %.*s\n", j,
					  (int)sp[j]->name_size,
					  sp[j]->umbrella_name);
				else if(sp[j]->library_name != NULL)
				   printf("\t[%lu] %.*s\n", j,
					  (int)sp[j]->name_size,
					  sp[j]->library_name);
				else
				   printf("\t[%lu] %s\n", j, sp[j]->name);
			    }
			}
			q->images[i].image.two_level_debug_printed = TRUE;
		    }
		    q = q->next_images;
		}while(q != NULL);
	    }
	}

	DYLD_TRACE_IMAGES_END(DYLD_TRACE_load_dependent_libraries);
	return(TRUE);
}

/*
 * load_images_libraries() loads the library image's for the specified mach
 * header from the specified image.  It does not in turn load any libraries
 * these libraries depend on.  If force_flat_namespace is FALSE then the array
 * of pointers to the dependent images is allocated and set into the image
 * structure.  If we are doing return_on_error then any libraries loaded are
 * then unloaded and FALSE is returned if errors occur.  Else TRUE is returned.
 */
static
enum bool
load_images_libraries(
struct mach_header *mh,
struct image *image)
{
    unsigned long i, ndependent_images;
    struct load_command *lc, *load_commands;
    struct dylib_command *dl_load;
    struct image **dependent_images, **image_pointer;

	load_commands = (struct load_command *)((char *)mh +
						sizeof(struct mach_header));
	/*
	 * If force_flat_namespace is false count the number of dependent
	 * images and allocate the image pointers for them.
	 */
	ndependent_images = 0;
	dependent_images = NULL;
	if(force_flat_namespace == FALSE){
	    lc = load_commands;
	    for(i = 0; i < mh->ncmds; i++){
		switch(lc->cmd){
		case LC_LOAD_DYLIB:
		case LC_LOAD_WEAK_DYLIB:
		    ndependent_images++;
		    break;
		}
		lc = (struct load_command *)((char *)lc + lc->cmdsize);
	    }
	    dependent_images = allocate_image_pointers(ndependent_images);
	    image->dependent_images = dependent_images;
	    image->ndependent_images = ndependent_images;
	}

	/*
	 * Load each of the libraries this image uses.
	 */
	ndependent_images = 0;
	lc = load_commands;
	for(i = 0; i < mh->ncmds; i++){
	    switch(lc->cmd){
	    case LC_LOAD_DYLIB:
	    case LC_LOAD_WEAK_DYLIB:
		dl_load = (struct dylib_command *)lc;
		if(dependent_images != NULL)
		    image_pointer = &(dependent_images[ndependent_images++]);
		else
		    image_pointer = NULL;
		if(load_library_image(dl_load, NULL, FALSE, FALSE,
				      image_pointer, NULL,
				      mh->filetype == MH_DYLIB) == FALSE &&
		   dl_load->cmd != LC_LOAD_WEAK_DYLIB &&
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
 * setup_sub_images() is called to set up the sub images that make up the
 * specified "primary" library_image.  If not all of its sub_umbrella's or 
 * sub_librarys are set up then it will return FALSE and not set up the sub
 * images.  The caller will loop through all the libraries until all libraries
 * are setup.  This routine will return TRUE when it set up the sub_images and
 * will also set the sub_images_setup field to TRUE in the specified library.
 */
static
enum bool
setup_sub_images(
struct library_image *library_image)
{
    unsigned long i, j, k, l, n, max_libraries;
    struct mach_header *mh;
    struct load_command *lc, *load_commands;
    struct sub_umbrella_command *usub;
    struct sub_library_command *lsub;
    struct sub_framework_command *sub;
    struct image **deps;
    char *sub_umbrella_name, *sub_library_name, *sub_framework_name;
    enum bool found, ignore_time_stamps, subs_can_use_hints, time_stamps_match;

	max_libraries = 0;
	deps = library_image->image.dependent_images;

	/*
	 * For the two-level namespace hints to be usable for images that
	 * have this image as a sub image we must know that there can't be the
	 * possiblity of new multiple definitions of the same symbol in the
	 * umbrella.  If any of this library's sub-umbrellas or sub-libraries
	 * were marked as with subs_can_use_hints == FALSE then hints for images
	 * that have this image as a sub image also can't be used.  So assume
	 * that we can use the hints and set subs_can_use_hints to FALSE if any
	 * of the sub-umbrellas or sub-libraries hints can't be used.
	 */
	subs_can_use_hints = TRUE;

	/*
	 * First see if this library has any sub-umbrellas or sub-libraries and
	 * that they have had their sub-images set up.  If not return FALSE and
	 * wait for this to be set up.  If so add the count of sub-images to
	 * max_libraries value which will be used for allocating the array for
	 * the sub-images of this library.
	 */
	mh = library_image->image.mh;
	load_commands = (struct load_command *)((char *)mh +
						sizeof(struct mach_header));
	lc = load_commands;
	for(i = 0; i < mh->ncmds; i++){
	    switch(lc->cmd){
	    case LC_SUB_UMBRELLA:
		usub = (struct sub_umbrella_command *)lc;
		sub_umbrella_name = (char *)usub + usub->sub_umbrella.offset;
		for(j = 0; j < library_image->image.ndependent_images; j++){
		    if(deps[j]->umbrella_name != NULL &&
		       strncmp(sub_umbrella_name, deps[j]->umbrella_name,
			       deps[j]->name_size) == 0 &&
		       sub_umbrella_name[deps[j]->name_size] == '\0'){
			/*
			 * TODO: can't this logic (here and in our caller) hang
		         * if there is a circular loop?  And is that even
			 * possible to create?  See comments in our caller.
			 */
			if(deps[j]->sub_images_setup == FALSE)
			    return(FALSE);
			max_libraries += 1 + deps[j]->nsub_images;
			if(deps[j]->subs_can_use_hints == FALSE)
			    subs_can_use_hints = FALSE;
		    }
		}
		break;
	    case LC_SUB_LIBRARY:
		lsub = (struct sub_library_command *)lc;
		sub_library_name = (char *)lsub + lsub->sub_library.offset;
		for(j = 0; j < library_image->image.ndependent_images; j++){
		    if(deps[j]->library_name != NULL &&
		       strncmp(sub_library_name, deps[j]->library_name,
			       deps[j]->name_size) == 0 &&
		       sub_library_name[deps[j]->name_size] == '\0'){
			/*
			 * TODO: can't this logic (here and in our caller) hang
		         * if there is a circular loop?  And is that even
			 * possible to create?  See comments in our caller.
			 */
			if(deps[j]->sub_images_setup == FALSE)
			    return(FALSE);
			max_libraries += 1 + deps[j]->nsub_images;
			if(deps[j]->subs_can_use_hints == FALSE)
			    subs_can_use_hints = FALSE;
		    }
		}
		break;
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}

	/*
	 * Allocate the sub-images array of pointers to images that make up
	 * this "primary" library.  Allocate enough to handle the max and
	 * then this allocation will be reallocated with the actual needed
	 * size.
	 */
	max_libraries += library_image->image.ndependent_images;
	library_image->image.sub_images =
	    allocate_image_pointers(max_libraries);
	n = 0;

	/*
	 * For the two-level namespace hints to be usable we must know that
	 * there can't be the possiblity of new multiple definitions of the
	 * same symbol in the umbrella.  We can know this two ways.  First if
	 * this umbrella is built with the MH_NOMULTIDEFS flag and the builder
	 * of the umbrella assures this for us.  Second all the time stamps of
	 * all the sub-images must match (note they don't have to be prebound
	 * or at the correct address).  We must avoid using the hints if there
	 * is a possiblity of new multiple definitions of the same symbol in
	 * the umbrella as using the hint and not using the hint may get a
	 * different answer.
	 *
	 * Checking of the time stamps of the sub images is only done if we
	 * don't know we can already use the hints.  We keep checking the time
	 * stamps while time_stamps_match is TRUE only if ignore_time_stamps is
	 * FALSE
	 */
	if((library_image->image.mh->flags & MH_NOMULTIDEFS) == MH_NOMULTIDEFS){
	    ignore_time_stamps = TRUE;
	    time_stamps_match = TRUE;
	}
	else{
	    ignore_time_stamps = FALSE;
	    if(subs_can_use_hints == TRUE)
		time_stamps_match = TRUE;
	    else
		time_stamps_match = FALSE;
	}

	/*
	 * First add the dependent images which are sub-frameworks of this
	 * image to the sub images list.
	 */
	if(library_image->image.umbrella_name != NULL){
	    for(i = 0; i < library_image->image.ndependent_images; i++){
		if(deps[i] == &(some_weak_library_image.image))
		    continue;
		mh = deps[i]->mh;
		load_commands = (struct load_command *)((char *)(mh) +
						    sizeof(struct mach_header));
		lc = load_commands;
		for(j = 0; j < mh->ncmds; j++){
		    if(lc->cmd == LC_SUB_FRAMEWORK){
			sub = (struct sub_framework_command *)lc;
			sub_framework_name = (char *)sub + sub->umbrella.offset;
			if(library_image->image.umbrella_name != NULL &&
			   strncmp(sub_framework_name,
			       library_image->image.umbrella_name,
			       library_image->image.name_size) == 0 &&
			   sub_framework_name[
			       library_image->image.name_size] =='\0'){
			    library_image->image.sub_images[n++] = deps[i];
			    /*
			     * Mark this dependent as not having its
			     * umbrella images set up to force them to be
			     * set up again in case this is a new umbrella
			     * library for this dependent.
			     */
			    deps[i]->umbrella_images_setup = FALSE;

			    /*
			     * If it is not known we can use the hints and the
			     * timestamps still match we must check this sub
			     * image's timestamp.
			     */
			    if(ignore_time_stamps == FALSE &&
			       time_stamps_match == TRUE)
				time_stamps_match = check_time_stamp(
				    library_image, deps[i]);
			    goto next_dep;
			}
		    }
		    lc = (struct load_command *)((char *)lc + lc->cmdsize);
		}
next_dep:	;
	    }
	}

	/*
	 * Second add the sub-umbrella's and sub-library's sub-images to the
	 * sub images list.
	 */
	mh = library_image->image.mh;
	load_commands = (struct load_command *)((char *)mh +
						sizeof(struct mach_header));
	lc = load_commands;
	for(i = 0; i < mh->ncmds; i++){
	    switch(lc->cmd){
	    case LC_SUB_UMBRELLA:
		usub = (struct sub_umbrella_command *)lc;
		sub_umbrella_name = (char *)usub + usub->sub_umbrella.offset;
		for(j = 0; j < library_image->image.ndependent_images; j++){
		    if(deps[j]->umbrella_name != NULL &&
		       strncmp(sub_umbrella_name, deps[j]->umbrella_name,
			       deps[j]->name_size) == 0 &&
		       sub_umbrella_name[deps[j]->name_size] == '\0'){

			/* make sure this image is not already on the list */
			found = FALSE;
			for(l = 0; l < n; l++){
			    if(library_image->image.sub_images[l] == deps[j]){
				found = TRUE;
				break;
			    }
			}
			if(found == FALSE){
			    library_image->image.sub_images[n++] = deps[j];
			    /*
			     * Mark this dependent as not having its
			     * umbrella images set up to force them to be
			     * set up again in case this is a new umbrella
			     * library for this dependent.
			     */
			    deps[j]->umbrella_images_setup = FALSE;
			    /*
			     * If it is not known we can use the hints and the
			     * timestamps still match we must check this sub
			     * image's timestamp.
			     */
			    if(ignore_time_stamps == FALSE &&
			       time_stamps_match == TRUE)
				time_stamps_match = check_time_stamp(
				    library_image, deps[j]);
			}

			for(k = 0; k < deps[j]->nsub_images; k++){
			    /* make sure this image is not already on the list*/
			    found = FALSE;
			    for(l = 0; l < n; l++){
				if(library_image->image.sub_images[l] ==
				   deps[j]->sub_images[k]){
				    found = TRUE;
				    break;
				}
			    }
			    if(found == FALSE){
				library_image->image.sub_images[n++] = 
				    deps[j]->sub_images[k];
				/*
				 * Mark this dependent as not having its
				 * umbrella images set up to force them to be
				 * set up again in case this is a new umbrella
				 * library for this dependent.
				 */
				deps[j]->sub_images[k]->umbrella_images_setup =
				     FALSE;
				/*
				 * If it is not known we can use the hints and
				 * the timestamps still match we must check
				 * this sub image's timestamp.
				 */
				if(ignore_time_stamps == FALSE &&
				   time_stamps_match == TRUE)
				    time_stamps_match = check_time_stamp(
					library_image, deps[j]->sub_images[k]);
			    }
			}
		    }
		}
		break;
	    case LC_SUB_LIBRARY:
		lsub = (struct sub_library_command *)lc;
		sub_library_name = (char *)lsub + lsub->sub_library.offset;
		for(j = 0; j < library_image->image.ndependent_images; j++){
		    if(deps[j]->library_name != NULL &&
		       strncmp(sub_library_name, deps[j]->library_name,
			       deps[j]->name_size) == 0 &&
		       sub_library_name[deps[j]->name_size] == '\0'){

			/* make sure this image is not already on the list */
			found = FALSE;
			for(l = 0; l < n; l++){
			    if(library_image->image.sub_images[l] == deps[j]){
				found = TRUE;
				break;
			    }
			}
			if(found == FALSE){
			    library_image->image.sub_images[n++] = deps[j];
			    /*
			     * Mark this dependent as not having its
			     * umbrella images set up to force them to be
			     * set up again in case this is a new umbrella
			     * library for this dependent.
			     */
			    deps[j]->umbrella_images_setup = FALSE;
			    /*
			     * If it is not known we can use the hints and the
			     * timestamps still match we must check this sub
			     * image's timestamp.
			     */
			    if(ignore_time_stamps == FALSE &&
			       time_stamps_match == TRUE)
				time_stamps_match = check_time_stamp(
				    library_image, deps[j]);
			}

			for(k = 0; k < deps[j]->nsub_images; k++){
			    /* make sure this image is not already on the list*/
			    found = FALSE;
			    for(l = 0; l < n; l++){
				if(library_image->image.sub_images[l] ==
				   deps[j]->sub_images[k]){
				    found = TRUE;
				    break;
				}
			    }
			    if(found == FALSE){
				library_image->image.sub_images[n++] = 
				    deps[j]->sub_images[k];
				/*
				 * Mark this dependent as not having its
				 * umbrella images set up to force them to be
				 * set up again in case this is a new umbrella
				 * library for this dependent.
				 */
				deps[j]->sub_images[k]->umbrella_images_setup =
				     FALSE;
				/*
				 * If it is not known we can use the hints and
				 * the timestamps still match we must check
				 * this sub image's timestamp.
				 */
				if(ignore_time_stamps == FALSE &&
				   time_stamps_match == TRUE)
				    time_stamps_match = check_time_stamp(
					library_image, deps[j]->sub_images[k]);
			    }
			}
		    }
		}
		break;
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
	/*
	 * Now reallocate the sub-images of this library to the actual size
	 * needed for it.  Note this just gives back the pointers we don't
	 * use when allocated from the block of preallocated pointers.
	 */
	reallocate_image_pointers(library_image->image.sub_images,
				  max_libraries, n);
	library_image->image.nsub_images = n;

	library_image->image.sub_images_setup = TRUE;

	/*
	 * If all this image's sub-umbrellas and sub-libraries have hints
	 * that can be used then the hints can be used in this image to aid in
	 * the lookup of symbols in this image if it is two-level and if it
	 * has hints.
	 */
	if(subs_can_use_hints == TRUE){
	    if((library_image->image.mh->flags & MH_TWOLEVEL) == MH_TWOLEVEL &&
	       (library_image->image.hints_cmd != NULL))
		library_image->image.image_can_use_hints = TRUE;
	     else
		library_image->image.image_can_use_hints = FALSE;
	    /*
	     * If this image is a sub-image of another umbrella image then for
	     * hints in the umbrella to be used this image's time stamps for its
	     * sub images much match.  Or MH_NOMULTIDEFS was set in the
	     * mach header and we already assumed the time_stamps_match.
	     */
	    if(time_stamps_match == TRUE)
		library_image->image.subs_can_use_hints = TRUE;
	    else
		library_image->image.subs_can_use_hints = FALSE;
	}
	else{
	    library_image->image.image_can_use_hints = FALSE;
	    library_image->image.subs_can_use_hints = FALSE;
	}
	if(dyld_hints_debug != 0)
	    printf("image_can_use_hints = %s subs_can_use_hints = %s for "
		   "image: %s\n",
		   library_image->image.image_can_use_hints == TRUE ?
		   "TRUE" : "FALSE",
		   library_image->image.subs_can_use_hints == TRUE ?
		   "TRUE" : "FALSE",
		   library_image->image.name);

	return(TRUE);
}

/*
 * check_time_stamp() checks the timestamp for LC_LOAD_DYLIB or
 * LC_LOAD_WEAK_DYLIB command in the specified library_image against the time
 * stamp of the LC_ID_DYLIB command for the sub_image.  If they are the same it 
 * returns TRUE else it returns FALSE.
 */
static
enum bool
check_time_stamp(
struct library_image *library_image,
struct image *sub_image)
{
    unsigned long i;
    struct load_command *lc;
    struct dylib_command *dl_load, *dlid;
    char *dep_name, *name;

	dlid = ((struct library_image *)sub_image->outer_image)->dlid;
	dep_name = (char *)dlid + dlid->dylib.name.offset;

	lc = (struct load_command *)((char *)(library_image->image.mh) +
				    sizeof(struct mach_header));
	for(i = 0; i < library_image->image.mh->ncmds; i++){
	    switch(lc->cmd){
	    case LC_LOAD_DYLIB:
	    case LC_LOAD_WEAK_DYLIB:
		dl_load = (struct dylib_command *)lc;
		name = (char *)dl_load + dl_load->dylib.name.offset;
		if(strcmp(name, dep_name) == 0){
		    if(dlid->dylib.timestamp == dl_load->dylib.timestamp)
			return(TRUE);
		    else
			return(FALSE);
		}
		break;
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
	/* should never get here */
	return(FALSE);
}

/*
 * setup_umbrella_images() sets up the umbrella_images list for the specified
 * library.  If any library has this library listed as one of its sub_images
 * then that library is added to the list of umbrella_images.  The prameter
 * max_libraries is the total number of libraries and is used to at first to
 * allocate the number of image pointers for the umbrella_images.  After the
 * true size that is needed is known then umbrella_images is "reallocated" to
 * the actual size needed.
 */
static
void
setup_umbrella_images(
struct library_image *library_image,
unsigned long max_libraries)
{
    unsigned long i, j;
    struct library_images *q;
    enum bool found;

	library_image->image.umbrella_images =
	    allocate_image_pointers(max_libraries);
	library_image->image.numbrella_images = 0;

	q = &library_images;
	do{
	    for(i = 0; i < q->nimages; i++){
		/*
		 * Check to see if this library is on the list of
		 * umbrella_images already.  If so nothing more to do with it.
		 */
		found = FALSE;
		for(j = 0; j < library_image->image.numbrella_images; j++){
		    if(library_image->image.umbrella_images[j] ==
		       &(q->images[i].image) ){
			found = TRUE;
			break;
		    }
		}
		if(found == TRUE)
		    continue;

		/*
		 * This library is not on the list of umbrella_images so see if
		 * the library we are setting up the list for is listed as one
		 * of this library's sub-images.  If so put it on the
		 * umbrella_images list.
		 */
		for(j = 0; j < q->images[i].image.nsub_images; j++){
		    if(q->images[i].image.sub_images[j] ==
		       &(library_image->image) ){
			library_image->image.umbrella_images[
			    library_image->image.numbrella_images++] =
				&(q->images[i].image);
			break;
		    }
		}
	    }
	    q = q->next_images;
	}while(q != NULL);

	/*
	 * Now reallocate the sub-umbrellas of this library to the actual size
	 * needed for it.  Note this just gives back the pointers we don't
	 * use when allocated from the block of preallocated pointers.
	 */
	reallocate_image_pointers(library_image->image.umbrella_images,
				  max_libraries,
				  library_image->image.numbrella_images);
	library_image->image.umbrella_images_setup = TRUE;
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
			deallocate_image_pointers(
			    library_image->image.dependent_images,
			    library_image->image.ndependent_images);
			deallocate_image_pointers(
			    library_image->image.sub_images,
			    library_image->image.nsub_images);
			deallocate_image_pointers(
			    library_image->image.umbrella_images,
			    library_image->image.numbrella_images);
			/* zero out the image as it will be reused */
			memset(library_image,'\0',sizeof(struct library_image));
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
			    deallocate_image_pointers(
				library_image->image.dependent_images,
				library_image->image.ndependent_images);
			    deallocate_image_pointers(
				library_image->image.sub_images,
				library_image->image.nsub_images);
			    deallocate_image_pointers(
				library_image->image.umbrella_images,
				library_image->image.numbrella_images);
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
 * clear_trying_to_use_prebinding_post_launch() is called after a successful
 * load of libraries when trying_to_use_prebinding_post_launch got set and
 * some of the libraries may have had their prebinding used.
 */
void
clear_trying_to_use_prebinding_post_launch(
void)
{
    unsigned long i;
    struct library_images *p;

	for(p = &library_images; p != NULL; p = p->next_images){
	    for(i = 0; i < p->nimages; i++){
		p->images[i].image.trying_to_use_prebinding_post_launch = FALSE;
	    }
	}
	trying_to_use_prebinding_post_launch = FALSE;
}

/*
 * check_image() checks the mach_header and load_commands of an image.
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
	if(cpusubtype_execute(host_basic_info.cpu_type,
		      host_basic_info.cpu_subtype, mh->cpusubtype) == FALSE){
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
	    case LC_LOAD_WEAK_DYLIB:
		dl_load = (struct dylib_command *)lc;
		if(dl_load->cmdsize < sizeof(struct dylib_command)){
		    error("truncated or malformed %s: %s (cmdsize of load "
			"command %lu incorrect for %s)", image_type,
			name, i, lc->cmd == LC_LOAD_DYLIB ? "LC_LOAD_DYLIB" : 
			"LC_LOAD_WEAK_DYLIB");
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

	    default:
		if((lc->cmd & LC_REQ_DYLD) == LC_REQ_DYLD){
		    error("can't use %s: %s (unknown load command %ld (0x%x) "
			  "required for execution)", image_type, name, i,
			  (unsigned int)(lc->cmd));
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

/*
 * unsave_string() is passed a string previously returned by save_string()
 * above.  If the string is not in the string block it it free()ed.
 */
void
unsave_string(
char *string)
{
	if(string < string_block.strings ||
	   string > string_block.strings + STRING_BLOCK_SIZE)
	    free(string);
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
    unsigned long n, max, cwd_len;

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
 * case of an error when return_on_error is set.  These are always allocated
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
 * allocate_image_pointers() is passed the number of image pointers to allocate.
 * The allocation/reallocation of image pointers is based on setting up one of
 * these arrays at a time. Using a maximum value for n then reallocateing it to
 * the actual smaller value.
 */
static
struct image **
allocate_image_pointers(
unsigned long n)
{
    struct image **p;

	if(n <= IMAGE_POINTER_BLOCK_SIZE - image_pointers_block.used &&
	   return_on_error == FALSE){
	    p = image_pointers_block.image_pointers + image_pointers_block.used;
	    image_pointers_block.used += n;
	}
	else{
	    p = allocate(sizeof(struct image *) * n);
	}
	return(p);
}

/*
 * reallocate_image_pointers() is used ONLY to allocate a SMALLER number of
 * image pointers.  That is old_size must always be greater or the same as 
 * new_size.
 */
static
void
reallocate_image_pointers(
struct image **image_pointers,
unsigned long old_size,
unsigned long new_size)
{
	if(image_pointers < image_pointers_block.image_pointers &&
	   image_pointers + old_size >= image_pointers_block.image_pointers +
				 IMAGE_POINTER_BLOCK_SIZE){
	    return;
	}
	else{
	    if(old_size >= new_size &&
	       image_pointers + old_size ==
	       image_pointers_block.image_pointers + image_pointers_block.used){
		memset(image_pointers + new_size, '\0',
		       sizeof(struct image *) * (old_size - new_size));
		image_pointers_block.used -= (old_size - new_size);
	    }
	}
}

/*
 * deallocate_image_pointers() deallocates the image pointers allocated with
 * allocate_image_pointers().  If this was the last allocated set of pointers
 * in the preallocated block the pointers are reclaimed. If the pointer are
 * outside the preallocated block they are free()'ed else they are leaked.
 */
static
void
deallocate_image_pointers(
struct image **image_pointers,
unsigned long n)
{
	if(image_pointers == NULL || n == 0)
	    return;

	if(image_pointers < image_pointers_block.image_pointers &&
	   image_pointers + n >= image_pointers_block.image_pointers +
				 IMAGE_POINTER_BLOCK_SIZE){
	    free(image_pointers);
	}
	else{
	    if(image_pointers + n ==
	       image_pointers_block.image_pointers + image_pointers_block.used){
		memset(image_pointers, '\0', sizeof(struct image *) * n);
		image_pointers_block.used -= n;
	    }
	}
}

/*
 * validate_library() reports an error and returns FALSE if the loaded
 * library's compatibility_version is less than the one the load command
 * requires, or the timestaps do not match. Otherwise, returns TRUE.
 * This routine will also set all_twolevel_modules_prebound to FALSE if
 * dl is not NULL and the time stamps don't match and reference_from_dylib
 * is TRUE.
 */
static
enum bool
validate_library(
char *dylib_name,
struct dylib_command *dl,
struct library_image *li,
enum bool reference_from_dylib)
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
	    if(prebinding == TRUE && launched == FALSE){
		if(dyld_prebind_debug != 0)
		    print("dyld: %s: prebinding disabled because time stamp of "
			  "library: %s did not match\n", executables_name,
			  dylib_name);
		if((li->image.mh->flags & MH_PREBOUND) == MH_PREBOUND)
		    notify_prebinding_agent();
	    }
	    if(launched == FALSE)
		prebinding = FALSE;
	    if(dl != NULL && reference_from_dylib == TRUE)
		all_twolevel_modules_prebound = FALSE;
	}
	return(TRUE);
}

/*
 * is_library_loaded_by_name() returns TRUE if the library is already loaded.
 * Also it validates the compatibility version and timestamp of the library
 * against the load command.  For two-level images that depend on this library,
 * image_pointer is the address of a pointer to an image struct to be set (if
 * not NULL) after if this image is loaded.  reference_from_dylib is TRUE when
 * this library is referenced by another dylib and FALSE otherwise.
 */
enum bool
is_library_loaded_by_name(
char *dylib_name,
struct dylib_command *dl,
struct image **image_pointer,
enum bool reference_from_dylib)
{
    unsigned long i;
    struct library_images *p;

	/*
	 * If this library name is not a full path we can't match by name if the
	 * program was already launched as it could be the same name as a
	 * library already loaded but be a different library if the current
	 * working directory changed.  So return FALSE and let it be opened and 
	 * checked to be the same if the stat(2) info is the same.
	 */
	if(launched == FALSE && *dylib_name != '/')
	    return(FALSE);
	for(p = &library_images; p != NULL; p = p->next_images){
	    for(i = 0; i < p->nimages; i++){
		if(strcmp(dylib_name, p->images[i].image.name) == 0){
		    if(validate_library(dylib_name, dl, &(p->images[i]),
		 			reference_from_dylib) == TRUE){
			if(image_pointer != NULL)
			    *image_pointer = &(p->images[i].image);
			return(TRUE);
		    }
		}
	    }
	}
	return(FALSE);
}

/*
 * is_library_loaded_by_stat() returns TRUE if the library is already loaded. 
 * Also it validates the compatibility version and timestamp of the library
 * against the load command.  For two-level images that depend on this library,
 * image_pointer is the address of a pointer to an image struct to be set (if
 * not NULL) after if this image is loaded.  reference_from_dylib is TRUE when
 * this library is referenced by another dylib and FALSE otherwise.
 */
enum bool
is_library_loaded_by_stat(
char *dylib_name,
struct dylib_command *dl,
struct stat *stat_buf,
struct image **image_pointer,
enum bool reference_from_dylib)
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
		    if(validate_library(dylib_name, dl, &(p->images[i]),
					reference_from_dylib) == TRUE){
			if(image_pointer != NULL)
			    *image_pointer = &(p->images[i].image);
			return(TRUE);
		    }
		}
	    }
	}
	return(FALSE);
}

/*
 * is_library_loaded_by_matching_installname() returns TRUE if there is a
 * library already loaded that is to match the filename to install names and
 * this filename matches that library's installname. For two-level images that
 * depend on this library, image_pointer is the address of a pointer to an image
 * struct to be set (if not NULL) after if this image is loaded.
 */
static
enum bool
is_library_loaded_by_matching_installname(
char *filename,
struct dylib_command *dl,
struct image **image_pointer)
{
    unsigned long i;
    struct library_images *p;
    char *installname;

	for(p = &library_images; p != NULL; p = p->next_images){
	    for(i = 0; i < p->nimages; i++){
		if(p->images[i].image.match_filename_by_installname == TRUE){
		    installname = (char *)(p->images[i].dlid) +
				  p->images[i].dlid->dylib.name.offset;
		    if(strcmp(filename, installname) == 0){
			if(image_pointer != NULL)
			    *image_pointer = &(p->images[i].image);
			return(TRUE);
		    }
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
		if(q->images[i].image.prebound != TRUE){
		    if(dyld_prebind_debug != 0)
			print("dyld: %s: prebinding disabled because no "
			       "LC_PREBOUND_DYLIB for library: %s\n",
				executables_name,
				(char *)q->images[i].dlid +
				q->images[i].dlid->dylib.name.offset);
		    notify_prebinding_agent();
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

	call_registered_funcs_for_add_images();
	return(TRUE);
}

/*
 * set_all_twolevel_modules_prebound() is called after the program is launched
 * to set the all_twolevel_modules_prebound variable to TRUE if all the
 * libraries are being used as two-level, prebound and all modules in them are
 * bound.
 */
void
set_all_twolevel_modules_prebound(
void)
{
    unsigned long i;
    struct library_images *q;

	/*
	 * If we are forcing flat namespace return leaving
	 * all_twolevel_modules_prebound set to its initial value of FALSE.
	 */
	if(force_flat_namespace == TRUE){
	    if(dyld_prebind_debug > 1)
		printf("dyld: all_twolevel_modules_prebound is FALSE\n");
	    return;
	}
	/*
	 * Check to see that all the libraries are two-level, prebound and have
	 * all of their modules bound.
	 */
	q = &library_images;
	do{
	    for(i = 0; i < q->nimages; i++){
		if((q->images[i].image.mh->flags & MH_TWOLEVEL) !=
		    MH_TWOLEVEL ||
		   (q->images[i].image.mh->flags & MH_PREBOUND) !=
		    MH_PREBOUND ||
		   q->images[i].image.all_modules_linked != TRUE){
		    if(dyld_prebind_debug > 1)
			printf("dyld: all_twolevel_modules_prebound is "
			       "FALSE\n");
		    return;
		}
	    }
	    q = q->next_images;
	}while(q != NULL);
	if(dyld_prebind_debug > 1)
	    printf("dyld: all_twolevel_modules_prebound is TRUE\n");
	all_twolevel_modules_prebound = TRUE;
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
    enum bool some_modules_linked, all_modules_linked;

	name = (char *)pbdylib + pbdylib->name.offset;
	linked_modules = (char *)pbdylib + pbdylib->linked_modules.offset;
	q = &library_images;
	do{
	    for(i = 0; i < q->nimages; i++){
		install_name = (char *)q->images[i].dlid +
				q->images[i].dlid->dylib.name.offset;
		if(strcmp(install_name, name) == 0){
		    some_modules_linked = FALSE;
		    if(q->images[i].image.prebound == TRUE ||
		       q->images[i].image.dyst->nmodtab != pbdylib->nmodules){
			if(dyld_prebind_debug != 0)
			    print("dyld: %s: prebinding disabled because "
				   "nmodules in LC_PREBOUND_DYLIB for library: "
				   "%s does not match\n",executables_name,name);
			notify_prebinding_agent();
			prebinding = FALSE;
			return(FALSE);
		    }
		    all_modules_linked = TRUE;
		    for(j = 0; j < q->images[i].nmodules; j++){
			if((linked_modules[j/8] >> (j%8)) & 1){
			    SET_LINK_STATE(q->images[i].modules[j],
					   FULLY_LINKED);
			    some_modules_linked = TRUE;
			}
			else{
			    all_modules_linked = FALSE;
			}
		    }
		    q->images[i].image.all_modules_linked = all_modules_linked;
		    q->images[i].image.prebound = TRUE;
		    if(some_modules_linked == FALSE){
			/* undo the prebinding of the lazy symbols pointers */
			undo_prebound_lazy_pointers(
			    &(q->images[i].image),
#if defined(m68k) || defined(__i386__)
			    GENERIC_RELOC_PB_LA_PTR,
#endif
#ifdef hppa
			    HPPA_RELOC_PB_LA_PTR,
#endif
#ifdef sparc
			    SPARC_RELOC_PB_LA_PTR,
#endif
#ifdef __ppc__
			    PPC_RELOC_PB_LA_PTR,
#endif
			    TRUE, /* all_lazy_pointers */
			    0);
			q->images[i].image.undone_prebound_lazy_pointers = TRUE;
		    }
		    return(TRUE);
		}
	    }
	    q = q->next_images;
	}while(q != NULL);
	if(dyld_prebind_debug != 0)
	    print("dyld: %s: prebinding disabled because LC_PREBOUND_DYLIB "
		   "found for library: %s but it was not loaded\n",
		   executables_name, name);
	notify_prebinding_agent();
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
enum bool post_launch_libraries_only)
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

	if(post_launch_libraries_only == TRUE)
	    goto do_library_images;

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
		    GENERIC_RELOC_PB_LA_PTR,
#endif
#ifdef hppa
		    HPPA_RELOC_PB_LA_PTR,
#endif
#ifdef sparc
		    SPARC_RELOC_PB_LA_PTR,
#endif
#ifdef __ppc__
		    PPC_RELOC_PB_LA_PTR,
#endif
		    TRUE, /* all_lazy_pointers */
		    0);
		p->images[i].image.undone_prebound_lazy_pointers = TRUE;
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

do_library_images:
	/*
	 * Second undo the prebinding for the library images.
	 */
	q = &library_images;
	do{
	    for(i = 0; i < q->nimages; i++){
		/* if this image was not prebound skip it */
		if((q->images[i].image.mh->flags & MH_PREBOUND) != MH_PREBOUND)
		    continue;

		if(post_launch_libraries_only == TRUE &&
		   q->images[i].image.trying_to_use_prebinding_post_launch ==
									FALSE)
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
 *
 * For the optimization of using two-level namespace prebound libraries if
 * this is such a library then its lazy pointer are left set and the module
 * state is set to FULLY_LINKED and the FULLYBOUND_STATE is set.
 */
static
void
undo_prebinding_for_library(
struct library_image *library_image)
{
    unsigned long j;

	/*
	 * If this library and all dependents are not two-level namespace
	 * and prebound, then undo the lazy symbols pointers and set the
	 * module states to PREBOUND_UNLINKED.
	 *
	 * If this library and all dependents are two-level namespace and
	 * prebound just set the module states to FULLY_LINKED and leave the
	 * lazy symbols set.
	 */
	if(library_image->image.subtrees_twolevel_prebound == FALSE){
	    /*
	     * Undo the prebinding of the lazy symbols pointers if the
	     * library has not been slid.  If it has been slid then this
	     * would have been done in local_relocation().
	     */
	    if(library_image->image.vmaddr_slide == 0){
		undo_prebound_lazy_pointers(
		    &(library_image->image),
#if defined(m68k) || defined(__i386__)
		    GENERIC_RELOC_PB_LA_PTR,
#endif
#ifdef hppa
		    HPPA_RELOC_PB_LA_PTR,
#endif
#ifdef sparc
		    SPARC_RELOC_PB_LA_PTR,
#endif
#ifdef __ppc__
		    PPC_RELOC_PB_LA_PTR,
#endif
		    TRUE, /* all_lazy_pointers */
		    0);
		library_image->image.undone_prebound_lazy_pointers = TRUE;
	    }
	    for(j = 0; j < library_image->nmodules; j++)
		SET_LINK_STATE(library_image->modules[j], PREBOUND_UNLINKED);
	}
	else{
	    if(dyld_prebind_debug > 2){
		print("dyld: In undo_prebinding_for_library() name = %s "
		      "setup = %s twolevel_prebound = %s\n",
		      library_image->image.name,
		      library_image->image.subtrees_twolevel_prebound_setup ==
		      TRUE ? "TRUE" : "FALSE",
		      library_image->image.subtrees_twolevel_prebound ==
		      TRUE ? "TRUE" : "FALSE");
	    }
	    for(j = 0; j < library_image->nmodules; j++){
		SET_LINK_STATE(library_image->modules[j], FULLY_LINKED);
		SET_FULLYBOUND_STATE(library_image->modules[j]);
	    }
	    library_image->image.all_modules_linked = TRUE;
	}
}

/*
 * try_to_use_prebound_libraries() is called when the libraries are setup for
 * prebinding but the executable is not.  If if is successfull prebinding is
 * left set to TRUE if not prebinding gets set to FALSE.  This is checking to
 * see if flat namespace semantics can be used.
 */
void
try_to_use_prebound_libraries(
void)
{
	/*
	 * Without two-level namespaces it is very expensive to check all
	 * libraries against all other libraries just to see if we can
	 * launch using prebound libraries if we are using flat namespace
	 * semantics.  So for now if there is more than one library just
	 * return and don't try to use the prebound libraries.
	 * 
	 * Also return if we are not forcing flat namespace and there is only
	 * one library and it is two-level namespace so that
	 * find_twolevel_prebound_lib_subtrees() can be called to use
	 * the two-level namespace prebound library which is quicker than
	 * checking for overriddes which does not need to happen with a
	 * two-level namespace library.
	 */
	if(library_images.nimages > 1 ||
	   (force_flat_namespace == FALSE &&
	    library_images.nimages == 1 &&
	    (library_images.images[0].image.mh->flags & MH_TWOLEVEL) ==
	     MH_TWOLEVEL) ){
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

			addr = q->images[i].image.rc->init_address +
			       q->images[i].image.vmaddr_slide;
			init_routine = (void(*)(void))addr;

			if(init_routine_being_called == FALSE)
			    init_routine_being_called = TRUE;
			else if(dyld_abort_multiple_inits == TRUE)
			    abort();

/*
printf("Calling init routine 0x%x in %s\n", init_routine, q->images[i].image.name);
*/
			DYLD_TRACE_CALLOUT_START(
			    DYLD_TRACE_image_init_routine, init_routine);
			release_lock();
			init_routine();
			set_lock();
			DYLD_TRACE_CALLOUT_END(
			    DYLD_TRACE_image_init_routine, init_routine);
			init_routine_being_called = FALSE;
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
		lookup_symbol(symbol_name, get_primary_image(image, symbols +i),
			      get_hint(image, symbols + i),
			      get_weak(symbols + i),
			      &defined_symbol, &defined_module,
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
printf("call_dependent_init_routines() for %s(%s)\n", image->name,
module_name == NULL ? "NULL" : module_name);
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
		    if(lc->cmd == LC_LOAD_DYLIB ||
		       lc->cmd == LC_LOAD_WEAK_DYLIB){
			dl = (struct dylib_command *)lc;
			dependent_name = (char *)dl + dl->dylib.name.offset;
			dependent_image = NULL;
			dependent_library_image = NULL;
			q = &library_images;
			do{
			    for(j = 0;
				dependent_image == NULL && j < q->nimages;
				j++){
				if(strcmp((char *)q->images[j].dlid +
					  q->images[j].dlid->dylib.name.offset,
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
			 * to be called first).  We use the init_called for
			 * libraries that do not have init routines when
			 * use_header_dependencies == TRUE so that we don't
			 * have to visit more than once.
			 */
			if(dependent_image != NULL &&
			   dependent_library_image->image.init_called == FALSE){
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

/*
printf("Calling init routine 0x%x in %s\n", init_routine, dependent_library_image->image.name);
*/
				DYLD_TRACE_CALLOUT_START(
				    DYLD_TRACE_dependent_init_routine,
				    init_routine);
				release_lock();
				init_routine();
				set_lock();
				DYLD_TRACE_CALLOUT_END(
				    DYLD_TRACE_dependent_init_routine,
				    init_routine);
				init_routine_being_called = FALSE;
			    }
			}
			/*
			 * As noted above the corresponding if for this else
			 * we mark this library as the init routine was called
			 * even it it does not have an init routine so we
			 * don't visit it again.
			 */
			else if(dependent_library_image != NULL){
			    dependent_library_image->image.init_called = TRUE;
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
		lookup_symbol(symbol_name, get_primary_image(image, symbols +
						    dylib_references[i].isym),
			      get_hint(image, symbols+dylib_references[i].isym),
			      get_weak(symbols + dylib_references[i].isym),
			      &defined_symbol, &defined_module,
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

/*
printf("Calling init routine 0x%x in %s\n", init_routine, defined_library_image->image.name);
*/
		    DYLD_TRACE_CALLOUT_START(
			DYLD_TRACE_dependent_init_routine,
			init_routine);
		    release_lock();
		    init_routine();
		    set_lock();
		    DYLD_TRACE_CALLOUT_END(
			DYLD_TRACE_dependent_init_routine,
			init_routine);
		    init_routine_being_called = FALSE;
		}
	    }
	}
}

/*
 * notify_prebinding_agent() notifies the prebinding agent that the executable
 * could not be launched using its prebinding info for some reason.
 */ 
static
void
notify_prebinding_agent(
void)
{
    struct statfs sb;
    int ret;
    struct stat fileStat, rootStat;

    struct timeval currentTime;

    char *serviceName;
    port_t serverPort;
    port_t bootstrapPort;

	DYLD_TRACE_IMAGES_NAMED_START(DYLD_TRACE_notify_prebinding_agent,
				      executables_path);

	/*
	 * If the DYLD_NO_FIX_PREBINDING environment variable was picked up
	 * or if fix_prebinding was determined not to be done then the variable
	 * dyld_no_fix_prebinding would have been set to TRUE.  If so then don't
	 * notify the prebinding agent.
	 */
	if(dyld_no_fix_prebinding == TRUE)
	    goto leave_notify_prebinding_agent;

#ifdef SYSTEM_REGION_BACKED
	/*
 	 * If we have not determined if the system shared regions are being used
	 * then make a nop call to load_shared_file() to determined this.  If
	 * they are not used return and do nothing.
	 */
	if(determined_system_region_backed == FALSE){
	    int flags;
	    vm_address_t base_address;

	    determined_system_region_backed = TRUE;
	    flags = QUERY_IS_SYSTEM_REGION;
	    (void)load_shared_file(NULL, (caddr_t)NULL, 0,
		(caddr_t *)&base_address, 0, NULL, &flags);
	    if((flags & SYSTEM_REGION_BACKED) != SYSTEM_REGION_BACKED){
		if(dyld_prebind_debug != 0)
		    print("dyld: in notify_prebinding_agent() determined the "
			  "system shared regions are NOT used\n");
		goto leave_notify_prebinding_agent;
	    }
	    else{
		if(dyld_prebind_debug != 0)
		    print("dyld: in notify_prebinding_agent() determined the "
			  "system shared regions ARE used\n");
	    }
	}
#endif /* SYSTEM_REGION_BACKED */


	/*
	 * Check the write access of the root file system.  If it is mounted
	 * read-only then we're probably still starting up the OS and we won't
	 * be able to write files so just return.
	 */
	ret = statfs("/", &sb);
	if(ret == -1 || (sb.f_flags & MNT_RDONLY) != 0)
	    goto leave_notify_prebinding_agent;

	/*
	 * Paths longer than PATH_MAX won't trigger prebinding, just so
	 * we don't need to do anything clever with longer paths and
	 * mach messages. 
	 *
	 * I think that this is not needed as the name is sent in out of line
	 * data with a length.
	 */
	if(executables_pathlen >= PATH_MAX)
	    goto leave_notify_prebinding_agent;

	/*
	 * Until fix_prebinding is build with the ld(1) flag -nofixprebinding
	 * we check the name of the executable.  This code will be removed at
	 * some point.
	 *
	 * If the name's too short to be fix_prebinding, or if the compare
	 * doesn't work, then do the re-prebinding.  We need to do the test
	 * here because notifying fix_prebinding of a problem with
	 * fix_prebinding can put us in an endless loop.  fix_prebinding can't
	 * do the check because it would only be able to check after the
	 * trouble has been started.
	 */
	if((executables_pathlen >= sizeof("fix_prebinding")) &&
	   (strcmp(executables_path +
		   ((executables_pathlen - 1) - (sizeof("fix_prebinding") -1)),
	          "fix_prebinding")) == 0){
	    goto leave_notify_prebinding_agent;
	}

	/*
	 * The next bit of code should not be in dyld and it is making policy
	 * on what will have its prebinding fixed.  Also this has to stat()
	 * calls that are very expensive in the program launch path.
	 *
	 * Determine if the executable is not on the root volume.
	 * If we ever start re-prebinding remote files (or files on other
	 * local volumes) this code would have to change.
	 */
 
	/*
	 * Get the device number for the root directory. If we get an error
	 * just return and don't fix the prebinding.
	 */
	if(stat("/", &rootStat) != 0)
	    goto leave_notify_prebinding_agent;

	/*
	 * Get the device number for the executable.  Again if get an error
	 * just return and don't fix the prebinding.
	 */
	if(stat(executables_path, &fileStat) != 0)
	    goto leave_notify_prebinding_agent;

	/*
	 * If the device numbers are not the same it is not on the root volume
	 * so just return and don't fix the prebinding.
	 */
	if(fileStat.st_dev != rootStat.st_dev){
	    if(dyld_prebind_debug != 0)
	    print("dyld: %s is not on root volume, not re-prebinding.\n",
		  executables_path);
	    goto leave_notify_prebinding_agent;
	}

	/*
	 * Collect all the information to send to the prebinding server that
	 * it needs to decide whether the file's prebinding will be fixed.
	 * If some part of this information can't be gathered then we just
	 * return and don't fix the prebinding.
	 */

	/*
	 * Get the current time.  This is used this to decide whether the
	 * problems noted in a queued up prebinding warning might have already
	 * been fixed by the time fix_prebinding gets to work on the file.  The
	 * value is will be compared against the modification dates of all
	 * files that need changing; if any were changed after we noted the
	 * prebinding problem, we assume the problem may have been fixed, and
	 * fix_prebinding will choose not to fix the binary.
	 */
	if(gettimeofday(&currentTime, NULL) != 0)
	    goto leave_notify_prebinding_agent;

	/*
	 * Find the "PrebindServer" service that we'll send the message to.
	 * This code should be the same when we're communicating with a service
	 * started by mach_init.
	 */
	ret = task_get_bootstrap_port(mach_task_self(), &bootstrapPort);
	if(ret != KERN_SUCCESS){
	    if(dyld_prebind_debug != 0)
		print("dyld: task_get_bootstrap_port() failed with: %d\n", ret);
	    goto leave_notify_prebinding_agent;
	}

	serviceName = "PrebindService";
	serverPort = PORT_NULL;
	ret = bootstrap_look_up(bootstrapPort,serviceName,&serverPort);
	if(ret != KERN_SUCCESS){
	    if(dyld_prebind_debug != 0)
		print("dyld: bootstrap_look_up() for: %s failed with: %d\n",
		      serviceName, ret);
	    goto leave_notify_prebinding_agent;
	}


	/*
	 * Call the mig stub to actually get the message sent. The mig-generated
	 * code will actually send the message to fix_prebinding.
	 */
	(void)warnBadlyPrebound(serverPort,
				currentTime,
				rootStat.st_dev, rootStat.st_ino,
				executables_path, strlen(executables_path) + 1,
				0);
  
leave_notify_prebinding_agent:

	DYLD_TRACE_IMAGES_END(DYLD_TRACE_notify_prebinding_agent);
}

/*
 * We have our own localtime() to avoid needing the notify API which is used
 * by the code in libc.a for localtime() but is in libnotify.
 */
struct tm *
localtime(
const time_t *t)
{
	return((struct tm *)0);
}
