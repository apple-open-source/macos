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
#import <mach-o/loader.h>
#ifdef EGREGIOUS_HACK_FOR_SAMPLER
#import <dyld/bool.h>
#else
#import "stuff/bool.h"
#endif
#import <sys/types.h>

enum link_state {
    UNLINKED,		/* the starting point for UNLINKED modules */
    BEING_LINKED,	/* moduled selected to be link into the program */
    RELOCATED,		/* moduled relocated dyld can now use the module */
    REGISTERING,	/* functions registered for modules being link called */
    INITIALIZING,	/* module initializers being called */
    LINKED,		/* module initializers called user can now use module */
    FULLY_LINKED,	/* module fully linked (all lazy symbols resolved) */

    PREBOUND_UNLINKED,	/* the module is prebound image but unlinked after */
			/*  the program was launch. */

    BEING_UNLINKED,	/* not yet used.  TODO unlinking */
    REPLACED,		/* not yet used.  TODO replacing */

    UNUSED		/* a module handle that is now unused */
};

struct image {
    char *name;			/* Image name for reporting errors. */
    unsigned long vmaddr_slide; /* The amount the vmaddresses are slid in the */
				/*  image from the staticly link addresses. */
    struct mach_header *mh;	/* The mach header of the image. */
    unsigned long valid;	/* TRUE if this is struct is valid */
    unsigned long vmaddr_size;  /* The size of the vm this image uses */
    unsigned long seg1addr;	/* The address of the first segment */
    unsigned long		/* The address of the first read-write segment*/
	segs_read_write_addr;	/*  used only for MH_SPLIT_SEGS images. */
    struct symtab_command *st;	/* The symbol table command for the image. */
    struct dysymtab_command	/* The dynamic symbol table command for the */
	*dyst;			/*  image. */
    struct segment_command	/* The link edit segment command for the */
	*linkedit_segment;	/*  image. */
    struct routines_command *rc;/* The routines command for the image */
    struct section *init;	/* The mod init section */
    struct section *term;	/* The mod term section */
#ifdef __ppc__
    unsigned long 		/* the image's dyld_stub_binding_helper */
	dyld_stub_binding_helper; /* address */
#endif
    unsigned long
      prebound:1,		/* Link states set from prebound state */
      change_protect_on_reloc:1,/* The image has relocations in read-only */
				/*  segments and protection needs to change. */
      cache_sync_on_reloc:1,	/* The image has relocations for instructions */
				/*  and the i cache needs to sync with d cache*/
      registered:1,		/* The functions registered for add images */
				/*  have been called */
      private:1,		/* global symbols are not used for linking */
      init_bound:1,		/* the image init routine has been bound */
      init_called:1,		/* the image init routine has been called */
      lazy_init:1,		/* the image init routine to be called lazy */
      has_coalesced_sections:1, /* the image has coalesced sections */
      unused:23;
};

/*
 * This is really an enum link_state.  Originally there was a module structure
 * that had an enum link_state field.  Because the minimum structure aligment 
 * is more than one-byte aligned this wasted space.  Since this is one of the
 * few allocated and written data structures of dyld it is important it is as
 * small as reasonable.  It needs to be addressable so using less than a byte
 * is not acceptable.
 */
typedef char module_state;

/*
 * To keep track of which modules are being fully bound the 0x80 bit of the
 * module state is used.  Fully bound is where all of the dependent references
 * are bound into the program.  Where fully linked in here means that the single
 * module has all it's lazy as well as it's not lazy symbols linked.  If an
 * image has an image initialization routine then the module containing that
 * routine is full bound before it is called.
 */
#define GET_LINK_STATE(m) ((enum link_state)((m) & 0xf))
#define SET_LINK_STATE(m,l) m = (((m) & 0xf0) | ((l) & 0xf))
#define GET_FULLYBOUND_STATE(m) ((m) & 0x80)
#define SET_FULLYBOUND_STATE(m) m = ((m) | 0x80)
#define CLEAR_FULLYBOUND_STATE(m) m = ((m) & 0x7f)

/*
 * To allow image initialization routines (shared library init routines) to
 * force modules in there image to have there module initialization routines
 * run (C++ initializers, via a call to __initializeCplusplus) the 0x40 bit of
 * the module state is used to keep track of if the module initialization
 * routine as been run.  The module initialization routines are normally run
 * after the the image initialization routines so this bit is needed to make
 * sure a module initialization routine is not run twice.
 */
#define GET_MODINIT_STATE(m) ((m) & 0x40)
#define SET_MODINIT_STATE(m) m = ((m) | 0x40)

/*
 * To support module termination routines (to be used for C++ destructors) the
 * 0x20 bit of the module state is used to keep track of if the module
 * termination routine has been run.
 */
#define GET_MODTERM_STATE(m) ((m) & 0x20)
#define SET_MODTERM_STATE(m) m = ((m) | 0x20)

/*
 * To support calling image initialization routines (shared library init
 * routines) in their dependency order each module that defines a shared library
 * init routine and its dependents needs to be checked.  As each module is
 * checked it is marked so that is only checked once.
 */
#define GET_IMAGE_INIT_DEPEND_STATE(m) ((m) & 0x10)
#define SET_IMAGE_INIT_DEPEND_STATE(m) m = ((m) | 0x10)

struct object_image {
    struct image image;
    module_state module;
    enum bool module_state_saved;
    module_state saved_module_state;
};

struct library_image {
    struct image image;
    unsigned long nmodules;
    module_state *modules;
    struct dylib_command *dlid;
    dev_t dev;
    ino_t ino;
    enum bool dependent_libraries_loaded;
    enum bool remove_on_error;
    enum bool module_states_saved;
    module_state *saved_module_states;
    unsigned long library_offset;
};

/*
 * Using /System/Library/CoreServices/Desktop.app/Contents/MacOS/Desktop from
 * MacOS X Public Beta (Kodiak1G7)
 * TOTAL number of bundles	4
 * TOTAL number of libraries	58
 */
enum nobject_images { NOBJECT_IMAGES = 5 };
struct object_images {
    struct object_image images[NOBJECT_IMAGES];
    unsigned long nimages;
    struct object_images *next_images;
};
extern struct object_images object_images;

enum nlibrary_images { NLIBRARY_IMAGES = 60 };
struct library_images {
    struct library_image images[NLIBRARY_IMAGES];
    unsigned long nimages;
    struct library_images *next_images;
};
extern struct library_images library_images;

extern void (*dyld_monaddition)(char *lowpc, char *highpc);

extern void load_executable_image(
    char *name,
    struct mach_header *mh_execute,
    unsigned long *entry_point);

extern enum bool load_dependent_libraries(
    void);

extern enum bool load_library_image(
    struct dylib_command *dl,
    char *dylib_name,
    enum bool force_searching);

extern void unload_remove_on_error_libraries(
    void);

extern void clear_remove_on_error_libraries(
    void);

extern struct object_image *map_bundle_image(
    char *name,
    char *object_addr,
    unsigned long object_size);

extern void unload_bundle_image(
    struct object_image *object_image,
    enum bool keepMemoryMapped,
    enum bool reset_lazy_references);

extern void shared_pcsample_buffer(
    char *name,
    struct section *s,
    unsigned long slide_value);

extern enum bool set_images_to_prebound(
    void);

extern void undo_prebound_images(
    void);

extern void try_to_use_prebound_libraries(
    void);

extern void call_image_init_routines(
    enum bool make_delayed_calls);

extern char *executables_name;

extern char *save_string(
    char *name);

extern void create_executables_path(
    char *exec_path);

extern struct object_image *find_object_image(
    struct image *image);
