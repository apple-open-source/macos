/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*-
 * Copyright (c) 1997 Doug Rabson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Original code from:
 *	"kldload.c,v 1.5 1998/07/06 06:58:32 charnier Exp"
 */
#ifndef lint
static const char rcsid[] =
	"$Id: kmodload.c,v 1.14 2002/04/15 20:28:30 lindak Exp $";
#endif /* not lint */

#include <stdlib.h>
#include <err.h>
#include <sys/file.h>
#include <nlist.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <paths.h>

#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/mach_host.h>
#include <mach-o/kld.h>
#include <mach-o/fat.h>

#include <CoreFoundation/CoreFoundation.h>

#include "kld_patch.h"
#include "vers_rsrc.h"

#define KMOD_ERROR_USAGE	1
#define KMOD_ERROR_PERMS	2
#define KMOD_ERROR_LOADING	3		
#define KMOD_ERROR_INTERNAL	4
#define KMOD_ERROR_ALREADY	5

#define kKMOD_INFO_SYMBOLNAME "_kmod_info"
#define kKmodsymsName "kmodsyms"

static mach_port_t kernel_port;
static mach_port_t kernel_priv_port;

static kmod_info_t *module_dependencies = 0;
static vm_address_t kernel_alloc_address = 0;
static unsigned long kernel_alloc_size = 0;
static vm_address_t kernel_load_address = 0;
static unsigned long kernel_load_size = 0;
static unsigned long kernel_hdr_size = 0;
static unsigned long kernel_hdr_pad = 0;
static unsigned long faked_kernel_load_address = 0;

static kmod_info_t *loaded_modules = 0;
static int loaded_count = 0;

static char *progname = "program name?";
static int kmodsyms = 0;
static int link_addrs_set = 0;
static int verbose = 0;

static char *debugdumpfile = NULL;

// must not be static; kld library calls
extern void			kld_error_vprintf(const char *format, va_list ap);
static void			e_printf(const char *fmt, ...);
static void			v_printf(const char *fmt, ...);

static void			machwarn(int error, const char *message);
static void			macherr(int error, const char *message);

static unsigned long	linkedit_address(unsigned long size,
						 unsigned long headers_size);
static void			abort_load(int exitcode, const char *fmt, ...);
static void			map_and_patch(const char *base,
								  const char **library_paths,
								  const char *module);
static void			link_base(const char *base,
					  const char **dependency_paths,
					  const vm_address_t *dependency_addrs);
static void			clear_globals(void);
static kmod_info_t *map_module(const char *filename);
static struct mach_header *link_module(const char *filename, 
					     const char *output);
static vm_address_t		update_kmod_info(struct mach_header *mach_header);
static kmod_t			load_module(struct mach_header *mach_header,
					    vm_address_t info);
static void 			set_module_dependencies(kmod_t id);
static void			start_module(kmod_t id);

static void
usage(void)
{
	if (kmodsyms) {
		fprintf(stderr, "usage: %s [-v] [-k kernelfile] [-d dependencyfile] -o symbolfile modulefile\n", progname);
		fprintf(stderr, "       %s [-v]  -k kernelfile  [-d dependencyfile@address] -o symbolfile modulefile@address\n",
			progname);
	} else {
		fprintf(stderr, "usage: %s [-v] [-k kernelfile] [-d dependencyfile] [-o symbolfile] modulefile\n", progname);
	}
	fflush(stderr);
	exit(KMOD_ERROR_USAGE);
}

int
main(int argc, char** argv)
{
	int c, r, i;
	char * kernel = _PATH_UNIX;
	int kernel_set = 0;
	char * gdbfile = 0;
#define MAX_DEPENDANCIES	128
	char * dependencies[MAX_DEPENDANCIES];
	vm_address_t loaded_addresses[MAX_DEPENDANCIES];
	int dependency_count = 0;
	struct mach_header *rld_header;

	char * module_path = "";
	vm_address_t module_info = 0;
	vm_address_t module_faked_address = 0;
	kmod_t module_id = 0;
	kmod_info_t *file_kinfo;

	if ((progname = strrchr(argv[0], '/')) == NULL)
		progname = argv[0];
	else
		++progname;

	kmodsyms = !strcmp(progname, kKmodsymsName);

    fprintf(stderr, "%s is deprecated; use kextload(8) instead\n", progname);
    sleep(5);

	// XXX things to add:
	//  -p data string to send as outofband data on start
	//  -P data file to send as outofband data on start

	while ((c = getopt(argc, argv, "D:d:o:k:v")) != -1)
		switch (c) {
		case 'd':
			dependencies[dependency_count] = optarg;
			if (kmodsyms) {
				char *address;
				if ((address = strrchr(optarg, '@'))) {
					*address++ = 0;
					loaded_addresses[dependency_count] = strtoul(address, NULL, 0);
					link_addrs_set++;
				} else {
					loaded_addresses[dependency_count] = 0;
				}
			}
			if (++dependency_count == MAX_DEPENDANCIES) {
				abort_load(KMOD_ERROR_INTERNAL, 
					"internal error, dependency count overflow."); 
			}
			break;
		case 'o':
			gdbfile = optarg;
			break;
		case 'k':
			kernel_set++;
			kernel = optarg;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'D':
			debugdumpfile = optarg;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	dependencies[dependency_count] = 0;
	loaded_addresses[dependency_count] = 0;

	if (argc != 1) usage();

	module_path = argv[0];

	if (kmodsyms) {
		char *address;

		if (!gdbfile) usage();

		// check for @address
		if ((address = strrchr(module_path, '@'))) {
			*address++ = 0;
			module_faked_address = strtoul(address, NULL, 0);
			link_addrs_set++;
		} else {
			module_faked_address = 0;
		}

		// if any arg uses @address then they all must be and the kernel must be set
		if (link_addrs_set) {
			if (!kernel_set) usage();
			if (!module_faked_address) usage();
			for (i=0; i < dependency_count; i++) {
				if (!loaded_addresses[i]) usage();
			}
		}
	}

	// map the module if possible, map_module will fail if there is a problem
	file_kinfo = map_module(module_path);

	if (!link_addrs_set) {
		kmod_info_t *k;

		// we only need the kernel port if we need to lookup loaded kmods
		r = task_for_pid(mach_task_self(), 0, &kernel_port);
		machwarn(r, "unable to get kernel task port");
		if (KERN_SUCCESS != r) {
			abort_load(KMOD_ERROR_PERMS,
				"You must be running as root to load modules in the kernel.");
		}

		//get loaded modules
		r = kmod_get_info(kernel_port, (void *)&loaded_modules, &loaded_count);  // never freed
		macherr(r, "kmod_get_info() failed");

		// check to see if the module has been loaded
		k = loaded_modules;
		while (k) {
			if (!strcmp(k->name, file_kinfo->name)) {
				if (!kmodsyms) {
					abort_load(KMOD_ERROR_ALREADY,
						"the module named '%s' is already loaded.", k->name); 
				} else {
					module_faked_address = k->address;
				}
				break;
			}
			k = (k->next) ? (k + 1) : 0;
		}

		if (kmodsyms && !module_faked_address) {
			abort_load(KMOD_ERROR_USAGE,
				"the module named '%s' has not been loaded.", file_kinfo->name);
		}
		
		//XXX it would be nice to be able to verify this is the correct kernel
		//XXX by comparing the kernel version strings (once we have them)
	}

	map_and_patch(kernel, dependencies, module_path);

	// Tell the kld linker where to get its load address from
	kld_address_func(linkedit_address);

	// link the kernel along with any dependencies
	link_base(kernel, dependencies, loaded_addresses);

	if (kmodsyms) {
	    faked_kernel_load_address = module_faked_address;

	    if (!faked_kernel_load_address) {
			abort_load(KMOD_ERROR_INTERNAL,
			  "internal error, fell thru without setting module load address.");
	    }
	}

	rld_header = link_module(module_path, gdbfile);
	module_info = update_kmod_info(rld_header);

	if (debugdumpfile)
        kld_file_debug_dump(module_path, debugdumpfile);

	if (kmodsyms) return 0;

	// we need the priv port to load modules into the kernel
	kernel_priv_port = mach_host_self();  /* if we are privileged */

	module_id = load_module(rld_header, module_info);
	set_module_dependencies(module_id);
	start_module(module_id);

	return 0;
}

static void
machwarn(int error, const char *message)
{
	if (KERN_SUCCESS != error)
		e_printf("%s: %s", message, mach_error_string(error));
}

static void
macherr(int error, const char *message)
{
	if (KERN_SUCCESS != error)
		abort_load(KMOD_ERROR_INTERNAL,
			"%s: %s", message, mach_error_string(error));
}

static kmod_info_t *map_module(const char *filename)
{
	kmod_info_t *file_kinfo;

	if (!kld_file_map(filename))
		exit(KMOD_ERROR_LOADING);

	file_kinfo = kld_file_lookupsymbol(filename, kKMOD_INFO_SYMBOLNAME);
	if (!file_kinfo) {
		abort_load(KMOD_ERROR_USAGE,
			"%s is not a valid kernel module.", filename);
	}
	
	return file_kinfo;
}

static void
map_and_patch(const char *base, const char **library_paths, const char *module)
{
	if (!kld_file_map(base))
		exit(KMOD_ERROR_INTERNAL);
	if (!kld_file_merge_OSObjects(base))
		abort_load(KMOD_ERROR_LOADING, NULL);

	if (*library_paths) {
		char **library;
		for (library = library_paths; *library; library++) {
			map_module(*library);
			if (!kld_file_patch_OSObjects(*library))
				abort_load(KMOD_ERROR_LOADING, NULL);
		}
	}

	// Patch the vtables of the object module we are about to load
	// The module has already been mapped in the main() routine as part
	// of validation
	if (!kld_file_patch_OSObjects(module))
		abort_load(KMOD_ERROR_LOADING, NULL);

	// During the patch up process the mapped images were modified
	// to avoid having to allocate more data than necessary.
	// Now we have to give the patcher a chance to clean up after itself.
	if (!kld_file_prepare_for_link())
		abort_load(KMOD_ERROR_LOADING, NULL);
}

static void
link_base(const char *base,
	  const char **dependency_paths,
	  const vm_address_t *dependency_addrs)
{
	struct mach_header *rld_header;
	char *base_addr;
	long base_size;
	int ok;

	// Get the address and size of the base, usually the kernel
	base_addr = kld_file_getaddr(base, &base_size);
	if (!base_addr)
		exit(KMOD_ERROR_INTERNAL);	// Error reported by kld library.

	ok = kld_load_basefile_from_memory(base, base_addr, base_size);
	fflush(stdout);
	if (!ok)
		abort_load(KMOD_ERROR_LOADING, "kld_load_basefile(%s) failed.", base); 

	if (*dependency_paths) {
		char **dependency = dependency_paths;
		const vm_address_t *load_addr = dependency_addrs;

		while (*dependency) {
			kmod_info_t *file_kinfo;

			// Find the kmod_info structure in the image.
			file_kinfo =
				kld_file_lookupsymbol(*dependency, kKMOD_INFO_SYMBOLNAME);
			if (!file_kinfo) {
				abort_load(KMOD_ERROR_USAGE, 
					"%s is not a valid kernel module.", *dependency);
			}

			// find the address that this dependency is loaded at
			if (kmodsyms && *load_addr) {
				faked_kernel_load_address = *load_addr;
			} else {
				kmod_info_t *k;
				kmod_info_t *tmp;
				int found_it = 0;

				// match up file version of kmod_info with kernel version
				k = loaded_modules;
				while (k) {
					if (!strcmp(k->name, file_kinfo->name)) {
                        UInt32 kernel_kmod_version;
                        UInt32 file_kmod_version;

                        if (!VERS_parse_string(k->version, &kernel_kmod_version)) {
							e_printf("can't parse version string \"%s\" in kernel kmod",
                                k->version);
							abort_load(KMOD_ERROR_LOADING, 
								"can't parse kernel kmod version string \"%s\"", k->version);
                        }

                        if (!VERS_parse_string(file_kinfo->version, & file_kmod_version)) {
							e_printf("can't parse version string \"%s\" in kmod file %s",
                                file_kinfo->version, *dependency);
							abort_load(KMOD_ERROR_LOADING,
                                "can't parse version string \"%s\" in kmod file %s",
                                file_kinfo->version, *dependency);
                        }

						if (kernel_kmod_version != file_kmod_version) {
							e_printf("loaded kernel module '%s' version differs.", *dependency);
							abort_load(KMOD_ERROR_LOADING, 
								"loaded version '%s', file version '%s'.",
								k->version, file_kinfo->version);
						}
						found_it++;
						break;
					}
					k = (k->next) ? (k + 1) : 0;
				}
				if (!found_it) {
					abort_load(KMOD_ERROR_USAGE, 
						"kernel module '%s' is not loaded.", *dependency);
				}	
			
				tmp = malloc(sizeof(kmod_info_t));
				if (!tmp)
					abort_load(KMOD_ERROR_LOADING, "no memory.");

				*tmp = *k;
				tmp->next = module_dependencies;
				module_dependencies = tmp;

				faked_kernel_load_address = k->address;
			}

			rld_header = link_module(*dependency, 0);

			(void) update_kmod_info(rld_header);

			dependency++; load_addr++;
		}
		/* make sure we clear these so clean up does the right thing. */
		clear_globals();
	}
}

#if !defined(page_round)
#define page_trunc(p) ((int)(p)&~(vm_page_size-1))
#define page_round(p) page_trunc((int)(p)+vm_page_size-1)
#endif

static unsigned long
linkedit_address(unsigned long size, unsigned long headers_size)
{
    int r;
    unsigned long round_segments_size;
    unsigned long round_headers_size;
    unsigned long round_size;

    kernel_load_size = size;  // The actual size allocated by kld_load...

    round_headers_size = page_round(headers_size);
    round_segments_size = page_round(size - headers_size);
    round_size =  round_headers_size + round_segments_size;

    kernel_alloc_size = round_size;
    kernel_hdr_size = headers_size;  // will need to be rounded to page *after* link.
    kernel_hdr_pad = round_headers_size - headers_size;

    if (faked_kernel_load_address) {
        kernel_load_address = faked_kernel_load_address + kernel_hdr_pad;
        v_printf("Returning fake load address of 0x%8x", kernel_load_address);
        return kernel_load_address;
    }
    if (kmodsyms) {
		abort_load(KMOD_ERROR_INTERNAL, 
			"internal error, almost tried to alloc kernel memory."); 
    }

    r = vm_allocate(kernel_port, &kernel_alloc_address, 
        kernel_alloc_size, TRUE);
    macherr(r, "unable to allocate kernel memory");

    v_printf("allocated %ld bytes in kernel space at 0x%8x",
        kernel_alloc_size, kernel_alloc_address);

    kernel_load_address = kernel_alloc_address + kernel_hdr_pad;

    v_printf("Returning load address of 0x%x", kernel_load_address);

    return kernel_load_address;
}

static void
clear_globals(void)
{
    faked_kernel_load_address = 0;
    kernel_alloc_address = 0;
    kernel_alloc_size = 0;
    kernel_load_address = 0;
    kernel_load_size = 0;
    kernel_hdr_size = 0;
    kernel_hdr_pad = 0;
    return;
}

static struct mach_header *
link_module(const char *filename, const char *output)
{
	struct mach_header *rld_header;
	char *object_addr;
	long object_size;
	int ok;

	// Get the address of the thined MachO image.
	object_addr = kld_file_getaddr(filename, &object_size);
	if (!object_addr)
		abort_load(KMOD_ERROR_LOADING, NULL);

	ok = kld_load_from_memory(&rld_header, filename,
								object_addr, object_size, output);
	fflush(stdout);
	if (!ok)
		abort_load(KMOD_ERROR_LOADING, "kld_load() failed.");

	return rld_header;
}

// Update the kmod_info_t structure in the image to be laoded
// Side effect of removing the kKMOD_INFO_SYMBOLNAME from the 
// loaded symbol name space, otherwise we would have a duplicate
// defined symbol failure
vm_address_t
update_kmod_info(struct mach_header *mach_header)
{
	char * symbol = kKMOD_INFO_SYMBOLNAME;
	kmod_info_t *info;
	unsigned long value;
	int ok;

	ok = kld_lookup(symbol, &value); fflush(stdout);
	if (!ok)
		abort_load(KMOD_ERROR_LOADING, "kld_lookup(%s) failed.", symbol);

	ok = kld_forget_symbol(symbol); fflush(stdout);
	if (!ok)
		abort_load(KMOD_ERROR_LOADING, "kld_forget_symbol(%s) failed.", symbol);

       /* Get the kmod info by translating from the kernel address at value.
        */
	info = (kmod_info_t *)(value - (unsigned long)kernel_load_address + (unsigned long)mach_header);
	v_printf("kmod name: %s", info->name);
	v_printf("kmod start @ 0x%x", (vm_address_t)info->start);
	v_printf("kmod stop  @ 0x%x", (vm_address_t)info->stop);

       /* Record link info in kmod info struct, rounding the hdr_size to fit
        * the adjustment that was made.
        */
	info->address = kernel_alloc_address;
	info->size = kernel_alloc_size;
	info->hdr_size = page_round(kernel_hdr_size);

	if (!info->start)
		abort_load(KMOD_ERROR_LOADING, "invalid start address?");
	else if (!info->stop)
		abort_load(KMOD_ERROR_LOADING, "invalid stop address?");

	return (vm_address_t)value;
}

static kmod_t 
load_module(struct mach_header *mach_header, vm_address_t info)
{
    int r;
    kmod_t id;
    vm_address_t vm_buffer = 0;

    r = vm_allocate(mach_task_self(), &vm_buffer,
        kernel_alloc_size, TRUE);
    macherr(r, "unable to vm_allocate() copy buffer");

   /* Copy the linked segment data into the page-aligned buffer.
    * Do not round the header size here.
    */
    bzero((void *)vm_buffer, kernel_alloc_size);
    memcpy((void *)vm_buffer, mach_header, kernel_hdr_size);
    memcpy((void *)vm_buffer + page_round(kernel_hdr_size),
        (void *)((unsigned long)mach_header + kernel_hdr_size),
        kernel_load_size - kernel_hdr_size);

    // copy linked header into kernel address space
    r = vm_write(kernel_port, kernel_alloc_address,
        vm_buffer, kernel_alloc_size);
    macherr(r, "unable to write module into kernel memory");

    // let the kernel know about it
    r = kmod_create(kernel_priv_port, info, &id);
    macherr(r, "unable to register module with kernel");

    v_printf("kmod id %d successfully created at 0x%x size %ld.\n", 
        id, kernel_alloc_address, kernel_alloc_size);

    // FIXME: make sure this happens even on failure

    vm_deallocate(mach_task_self(), vm_buffer, kernel_alloc_size);
    return id;
}

static void
set_module_dependencies(kmod_t id)
{
	int r;
	void * args = 0;
	int argsCount= 0;
	kmod_info_t *module = module_dependencies;

	while (module) {

		r = kmod_control(kernel_priv_port, KMOD_PACK_IDS(id, module->id), KMOD_CNTL_RETAIN, &args, &argsCount);
		machwarn(r, "kmod_control(retain) failed");
		if (r) {
			clear_globals();
			r = kmod_destroy(kernel_priv_port, id);
			macherr(r, "kmod_destroy failed");
		}

		v_printf("kmod id %d reference count was sucessfully incremented.", module->id);

		module = module->next;
	}
}

static void
start_module(kmod_t id)
{
	int r;
	void * args = 0;
	int argsCount= 0;

	r = kmod_control(kernel_priv_port, id, KMOD_CNTL_START, &args, &argsCount);
	machwarn(r, "kmod_control(start) failed");
	if (r) {
		clear_globals();
		kmod_destroy(kernel_priv_port, id);
		macherr(r, "kmod_destroy failed");
	}

	v_printf("kmod id %d successfully started.", id);
}

static void e_printf(const char *fmt, ...)
{
	va_list ap;
	char msg[1024];

	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	fprintf(stderr, "%s: %s\n", progname, msg);
}

static void v_printf(const char *fmt, ...)
{
	va_list ap;
	char msg[1024];

	if (!verbose) return;

	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	printf("%s: %s\n", progname, msg);
}

static void abort_load(int exitcode, const char *fmt, ...)
{
	if (fmt) {
        va_list ap;
        char msg[1024];
    
        va_start(ap, fmt);
        vsnprintf(msg, sizeof(msg), fmt, ap);
        va_end(ap);
    
        fprintf(stderr, "%s: %s\n", progname, msg);
	}

	if (!faked_kernel_load_address
	&& (kernel_alloc_address || kernel_alloc_size)) {	
		int r;

		v_printf("freeing %ld bytes in kernel space at 0x%x",
					kernel_alloc_size, kernel_alloc_address);
		r = vm_deallocate(kernel_port, kernel_alloc_address, kernel_alloc_size);
		machwarn(r, "unable to cleanup kernel memory");
	}

	exit(exitcode);
}

__private_extern__ void
kld_error_vprintf(const char *fmt, va_list ap)
{
    vfprintf(stderr, fmt, ap);
    fflush(stderr);
}
