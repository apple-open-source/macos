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
#import <string.h>
#import <limits.h>
#import <mach/mach_traps.h>
#import <mach-o/loader.h>
#import <mach-o/ldsyms.h>

#import "stuff/bool.h"
#import "stuff/vm_flush_cache.h"

#import "inline_strcmp.h"
#import "images.h"
#import "reloc.h"
#import "symbols.h"

/*
 * These are used by send_past_events() in debug.c to send a DYLD_IMAGE_ADDED
 * event for the dynamic linker itself if it is a separate image.  Know if it
 * is a separate image dyld_image is declared as common symbol there (with a
 * value of 0 or FALSE) and as a data symbol in here as TRUE and will be
 * over ridden from here if the dynamic linker is linked as a separate image.
 */
enum bool dyld_image = TRUE;
struct mach_header *dyld_image_header =
	(struct mach_header *)(&_mh_dylinker_header);
unsigned long dyld_image_vmaddr_slide = 0;
char *dyld_image_name = NULL;

#ifdef __ppc__
extern void ppc_cache_flush(
    unsigned long byte_address,
    unsigned long size);
#endif

/*
 * _dyld_reloc() is called from the __dyld_start entry point startoff code when
 * the kernel has slid the dynamic linker from it's staticly link edited
 * address.  So _dyld_reloc() relocates the dynamic linker code by calling
 * local_relocation on itself by faking up an image that represents itself.
 * This code is being run before any local relocation is done so it must be
 * written knowing this has not been done.
 */
void
_dyld_reloc(
unsigned long vmaddr_slide)
{
    struct image image;
    struct mach_header *mh;
    unsigned long i, seg1addr, highaddr;
    struct load_command *lc, *load_commands;
    struct segment_command *sg, *linkedit_segment;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    struct dylinker_command *dyld;

	/*
	 * Note the addtion of vmaddr_slide is needed because the value of
	 * dyld_image_header which is initialized to the address of
	 * _mh_dylinker_header is the value the static link editor linked
	 * the dylinker image at and we are about to but have not done the
	 * local relocation (which is what this routine does).
	 */
#ifdef __DYNAMIC__
	mh = (struct mach_header *)((char *)dyld_image_header + vmaddr_slide);
#else
	mh = (struct mach_header *)(((char *)(&_mh_dylinker_header)) +
				    vmaddr_slide);
#endif
	load_commands = (struct load_command *)
			((char *)mh + sizeof(struct mach_header));
	seg1addr = ULONG_MAX;
	highaddr = 0;
	linkedit_segment = NULL;
	st = NULL;
	dyst = NULL;
	dyld = NULL;
	lc = load_commands;
	for(i = 0; i < mh->ncmds; i++){
	    switch(lc->cmd){
	    case LC_SEGMENT:
		sg = (struct segment_command *)lc;
#ifdef __DYNAMIC__
		if(strcmp(sg->segname, SEG_LINKEDIT) == 0){
#else
		if(strcmp(sg->segname, SEG_LINKEDIT + vmaddr_slide) == 0){
#endif
		    if(linkedit_segment == NULL)
			linkedit_segment = sg;
		}
		/* pickup the address of the first segment */
		if(sg->vmaddr < seg1addr)
		    seg1addr = sg->vmaddr;
		/* pickup the highest address of any segment */
		if(sg->vmaddr + sg->vmsize > highaddr)
		    highaddr = sg->vmaddr + sg->vmsize;
		break;
	    case LC_SYMTAB:
		if(st == NULL)
		    st = (struct symtab_command *)lc;
		break;
	    case LC_DYSYMTAB:
		if(dyst == NULL)
		    dyst = (struct dysymtab_command *)lc;
		break;
	    case LC_ID_DYLINKER:
		if(dyld == NULL)
		    dyld = (struct dylinker_command *)lc;
		break;
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
	image.name = (char *)dyld + dyld->name.offset;
	image.vmaddr_slide = vmaddr_slide;
	image.seg1addr = seg1addr;
	image.mh = mh;
	image.st = st;
	image.dyst = dyst;
	image.linkedit_segment = linkedit_segment;
	image.change_protect_on_reloc = FALSE;
	image.cache_sync_on_reloc = FALSE;

	local_relocation(&image);
#ifdef __DYNAMIC__
	relocate_symbol_pointers_for_defined_externs(&image);
#endif

#ifndef __DYNAMIC__
	/*
	 * It may be possible for dyld to slide itself with this routine even
	 * when compiled -static for some architectures depending on the
	 * generated code.  If so it may also need the instruction cache
	 * flushed but doubtful as we are just getting started and shouldn't
	 * have any code in the cache we are going to execute again.
	 */
#if defined(__m68k__) || defined(__i386__) || defined(__hppa__)
        /*
         * At this point mach_init() has not yet been called and we need to call
	 * vm_cache_flush() which takes our task port.  To do this we undefine
	 * the macro mach_task_self() defined in mach_init.h so that we can make
	 * the  real kernel call as the first argument.
         */
#undef mach_task_self()
	vm_flush_cache(mach_task_self(), seg1addr+vmaddr_slide,
		       highaddr-seg1addr);
#endif /* defined(__m68k__) || defined(__i386__) || defined(__hppa__) */
#ifdef __ppc__
	ppc_cache_flush(seg1addr + vmaddr_slide, highaddr - seg1addr);
#endif /* __ppc__ */
#endif /* __DYNAMIC__ */

	/*
	 * Record these values so they can be used to send a DYLD_IMAGE_ADDED
 	 * by send_past_events() in debug.c.  These statements require
	 * relocation so they must be done last.
	 */
	dyld_image_header = mh;
	dyld_image_vmaddr_slide = vmaddr_slide;
	dyld_image_name = image.name;
}
