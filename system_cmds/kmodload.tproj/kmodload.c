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
	"$Id: kmodload.c,v 1.7 2001/02/05 19:53:16 lindak Exp $";
#endif /* not lint */

#include <stdlib.h>
#include <err.h>
#include <sys/file.h>
#include <nlist.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <paths.h>

#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/mach_host.h>
#include <mach-o/kld.h>
#include <mach-o/fat.h>

#define KMOD_ERROR_USAGE	1
#define KMOD_ERROR_PERMS	2
#define KMOD_ERROR_LOADING	3		
#define KMOD_ERROR_INTERNAL	4
#define KMOD_ERROR_ALREADY	5

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
#define v_printf	if (verbose) printf

// must not be static; kld library calls
void				kld_error_vprintf(const char *format, va_list ap);

static void			machwarn(int error, const char *message);
static void			macherr(int error, const char *message);

static unsigned long		linkedit_address(unsigned long size,
						 unsigned long headers_size);
static void 			cleanup_kernel_memory();
static void 			link_base(const char *base,
					  const char **dependency_paths,
					  const vm_address_t *dependency_addrs);
static void			clear_globals(void);
static void 			map_module(char *module_path, char **object_addr,
					   long *object_size, kmod_info_t **kinfo);
static struct mach_header	*link_module(const char *filename, 
					     const char *output);
static vm_address_t		patch_module(struct mach_header *mach_header);
static kmod_t			load_module(struct mach_header *mach_header,
					    vm_address_t info);
static void 			set_module_dependencies(kmod_t id);
static void			start_module(kmod_t id);

static void
usage(void)
{
	if (kmodsyms) {
		fprintf(stderr, "usage: kmodsyms [-v] [-k kernelfile] [-d dependencyfile] -o symbolfile modulefile\n");
		fprintf(stderr, "       kmodsyms [-v]  -k kernelfile  [-d dependencyfile@address] -o symbolfile modulefile@address\n");
	} else {
		fprintf(stderr, "usage: kmodload [-v] [-k kernelfile] [-d dependencyfile] [-o symbolfile] modulefile\n");
	}
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
	char *module_addr = 0;
	long module_size = 0;
	vm_address_t module_faked_address = 0;
	kmod_t module_id = 0;
	kmod_info_t *file_kinfo;

	if ((progname = rindex(argv[0], '/')) == NULL)
		progname = argv[0];
	else
		++progname;

	kmodsyms = !strcmp(progname, "kmodsyms");

	// XXX things to add:
	//  -p data string to send as outofband data on start
	//  -P data file to send as outofband data on start

	while ((c = getopt(argc, argv, "d:o:k:v")) != -1)
		switch (c) {
		case 'd':
			dependencies[dependency_count] = optarg;
			if (kmodsyms) {
				char *address;
				if ((address = rindex(optarg, '@'))) {
					*address++ = 0;
					loaded_addresses[dependency_count] = strtoul(address, NULL, 0);
					link_addrs_set++;
				} else {
					loaded_addresses[dependency_count] = 0;
				}
			}
			if (++dependency_count == MAX_DEPENDANCIES) {
				fprintf(stderr, "%s: internal error, dependency count overflow.\n", progname); 
				exit(KMOD_ERROR_INTERNAL);
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
		if ((address = rindex(module_path, '@'))) {
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

	// map module and then check if it has been loaded
	map_module(module_path, &module_addr, &module_size, &file_kinfo);

	if (!link_addrs_set) {
		kmod_info_t *k;

		// we only need the kernel port if we need to lookup loaded kmods
		r = task_for_pid(mach_task_self(), 0, &kernel_port);
		machwarn(r, "unable to get kernel task port");
		if (r) {
			fprintf(stderr, "%s: You must be running as root to load/check modules in the kernel.\n", progname);
			exit(KMOD_ERROR_PERMS);
		}

		//get loaded modules
		r = kmod_get_info(kernel_port, (void *)&loaded_modules, &loaded_count);  // never freed
		macherr(r, "kmod_get_info() failed");

		// check to see if the module has been loaded
		k = loaded_modules;
		while (k) {
			if (!strcmp(k->name, file_kinfo->name)) {
				if (!kmodsyms) {
					fprintf(stderr, "%s: the module named '%s' is already loaded.\n", progname, k->name); 
					exit(KMOD_ERROR_ALREADY);
				} else {
					module_faked_address = k->address;
				}
				break;
			}
			k = (k->next) ? (k + 1) : 0;
		}

		if (kmodsyms && !module_faked_address) {
			fprintf(stderr, "%s: the module named '%s' has not been loaded.\n", progname, file_kinfo->name); 
			exit(KMOD_ERROR_USAGE);
		}
		
		//XXX it would be nice to be able to verify this is the correct kernel
		//XXX by comparing the kernel version strings (once we have them)
	}

	// link the kernel along with any dependencies
	link_base(kernel, dependencies, loaded_addresses);

	if (kmodsyms) {
	    faked_kernel_load_address = module_faked_address;

	    if (!faked_kernel_load_address) {
		fprintf(stderr, "%s: internal error, fell thru without setting module load address.\n", progname); 
		exit(KMOD_ERROR_INTERNAL);
	    }
	}

	rld_header = link_module(module_path, gdbfile);
	module_info = patch_module(rld_header);

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
	if (error == KERN_SUCCESS) return;
	fprintf(stderr, "%s: %s: %s\n", progname, message, mach_error_string(error));
}

static void
macherr(int error, const char *message)
{
	if (error == KERN_SUCCESS) return;
	fprintf(stderr, "%s: %s: %s\n", progname, message, mach_error_string(error));

	cleanup_kernel_memory();

	exit(KMOD_ERROR_INTERNAL);
}

static void
map_module(char *module_path, char **object_addr, long *object_size, kmod_info_t **kinfo)
{
	int fd;
	struct stat stat_buf;
	struct mach_header *mh;
	char *p;

	struct nlist nl[] = {
		{ "_kmod_info" },
		{ "" },
	};

	if((fd = open(module_path, O_RDONLY)) == -1){
	    fprintf(stderr, "%s: Can't open: %s\n", progname, module_path);
	    exit(KMOD_ERROR_USAGE);
	}
	if (nlist(module_path, nl)) {
		fprintf(stderr, "%s: %s is not a valid kernel module.\n", progname, module_path);
		exit(KMOD_ERROR_USAGE);
	}
	if(fstat(fd, &stat_buf) == -1){
	    fprintf(stderr, "%s: Can't stat file: %s\n", progname, module_path);
	    exit(KMOD_ERROR_PERMS);
	}
	*object_size = stat_buf.st_size;
	if(map_fd(fd, 0, (vm_offset_t *)object_addr, TRUE, *object_size) != KERN_SUCCESS){
	    fprintf(stderr, "%s: Can't map file: %s\n", progname, module_path);
	    exit(KMOD_ERROR_INTERNAL);
	}
	close(fd);

	if (NXSwapBigLongToHost(*((long *)*object_addr)) == FAT_MAGIC) {
		struct host_basic_info hbi;
		struct fat_header *fh;
		struct fat_arch *fat_archs, *fap;
		unsigned i, nfat_arch;

		/* Get our host info */
		i = HOST_BASIC_INFO_COUNT;
		if (host_info(mach_host_self(), HOST_BASIC_INFO, (host_info_t)(&hbi), &i) != KERN_SUCCESS) {
		    fprintf(stderr, "%s: Can't get host's basic info\n", progname);
		    exit(KMOD_ERROR_INTERNAL);
		}

		// get number of architectures
		fh = (struct fat_header *)*object_addr;
		nfat_arch = NXSwapBigLongToHost(fh->nfat_arch);

		// find beginning of fat_arch struct
		fat_archs = (struct fat_arch *)((char *)fh + sizeof(struct fat_header));

		/*
		 * Convert archs to host byte ordering (a constraint of
		 * cpusubtype_getbestarch()
		 */
		for (i = 0; i < nfat_arch; i++) {
			fat_archs[i].cputype =
				NXSwapBigLongToHost(fat_archs[i].cputype);
			fat_archs[i].cpusubtype =
			      NXSwapBigLongToHost(fat_archs[i].cpusubtype);
			fat_archs[i].offset =
				NXSwapBigLongToHost(fat_archs[i].offset);
			fat_archs[i].size =
				NXSwapBigLongToHost(fat_archs[i].size);
			fat_archs[i].align =
				NXSwapBigLongToHost(fat_archs[i].align);
		}

// this code was lifted from Darwin/Libraries/NeXT/libc/gen.subproj/nlist.c
// when cpusubtype_getbestarch exists this code should also be changed.
#define	CPUSUBTYPE_SUPPORT	0

#if	CPUSUBTYPE_SUPPORT
		fap = cpusubtype_getbestarch(hbi.cpu_type, hbi.cpu_subtype,
					     fat_archs, nfat_arch);
#else	CPUSUBTYPE_SUPPORT
#warning	Use the cpusubtype functions!!!
		fap = NULL;
		for (i = 0; i < nfat_arch; i++) {
			if (fat_archs[i].cputype == hbi.cpu_type) {
				fap = &fat_archs[i];
				break;
			}
		}
#endif	CPUSUBTYPE_SUPPORT
		if (!fap) {
		    fprintf(stderr, "%s: could not find the correct architecture in %s.\n", progname, module_path);
		    exit(KMOD_ERROR_USAGE);
		}
		
		*object_addr += fap->offset;
		*object_size = fap->size;
	}

	mh = (struct mach_header *)*object_addr;
	if (*((long *)mh) != MH_MAGIC) {
	    fprintf(stderr, "%s: invalid file format for file: %s\n", progname, module_path);
	    exit(KMOD_ERROR_USAGE);
	}
	p = *object_addr + sizeof(struct mach_header) + mh->sizeofcmds + nl->n_value;
	*kinfo = (kmod_info_t *)p;
}

static void
link_base(const char *base,
	  const char **dependency_paths,
	  const vm_address_t *dependency_addrs)
{
	struct mach_header *rld_header;
	int ok;

	ok = kld_load_basefile(base);
	fflush(stdout);
	if (!ok) {
		fprintf(stderr, "%s: kld_load_basefile(%s) failed.\n", progname, base); 
		exit(KMOD_ERROR_LOADING);
	}

	if (*dependency_paths) {
		char **dependency = dependency_paths;
		const vm_address_t *load_addr = dependency_addrs;

		while (*dependency) {
			char *object_addr;
			long object_size;
			kmod_info_t *file_kinfo;

			map_module(*dependency, &object_addr, &object_size, &file_kinfo);

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
						if (strcmp(k->version, file_kinfo->version)) {
							fprintf(stderr, "%s: loaded kernel module '%s' version differs.\n",
								progname, *dependency);
							fprintf(stderr, "%s: loaded version '%s', file version '%s'.\n",
								progname, k->version, file_kinfo->version);
							exit(KMOD_ERROR_LOADING);
						}
						found_it++;
						break;
					}
					k = (k->next) ? (k + 1) : 0;
				}
				if (!found_it) {
					fprintf(stderr, "%s: kernel module '%s' is not loaded.\n", 
						progname, *dependency);
					exit(KMOD_ERROR_USAGE);
				}	
			
                                tmp = malloc(sizeof(kmod_info_t));
				if (!tmp) {
					fprintf(stderr, "%s: no memory.\n", progname);
					exit(KMOD_ERROR_LOADING);
				}	
                                *tmp = *k;
				tmp->next = module_dependencies;
				module_dependencies = tmp;

				faked_kernel_load_address = k->address;
			}

			rld_header = link_module(*dependency, 0);

			(void)patch_module(rld_header);

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
        v_printf("%s: Returning fake load address of 0x%8x\n",
            progname, kernel_load_address);
        return kernel_load_address;
    }
    if (kmodsyms) {
        fprintf(stderr, "%s: internal error, almost tried to alloc kernel memory.\n", progname); 
        exit(KMOD_ERROR_INTERNAL);
    }

    r = vm_allocate(kernel_port, &kernel_alloc_address, 
        kernel_alloc_size, TRUE);
    macherr(r, "unable to allocate kernel memory");

    v_printf("%s: allocated %ld bytes in kernel space at 0x%8x\n",
        progname, kernel_alloc_size, kernel_alloc_address);

    kernel_load_address = kernel_alloc_address + kernel_hdr_pad;

    v_printf("%s: Returning load address of 0x%x\n",
        progname, kernel_load_address);

    return kernel_load_address;
}

static void
cleanup_kernel_memory()
{
	int r;

	if (faked_kernel_load_address) return;	

	if (kernel_alloc_address || kernel_alloc_size) {	
		v_printf("%s: freeing %ld bytes in kernel space at 0x%x\n",
			 progname, kernel_alloc_size, kernel_alloc_address);
		r = vm_deallocate(kernel_port, kernel_alloc_address, kernel_alloc_size);
                clear_globals();
		kernel_load_address = kernel_load_size = 0;
		machwarn(r, "unable to cleanup kernel memory");
	}
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
	int ok;

	kld_address_func(linkedit_address);

	ok = kld_load(&rld_header, filename, output);
	fflush(stdout);
	if (!ok) {
		fprintf(stderr, "%s: kld_load() failed.\n", progname);
		cleanup_kernel_memory();
		exit(KMOD_ERROR_LOADING);
	}

	return rld_header;
}

vm_address_t
patch_module(struct mach_header *mach_header)
{
	char * symbol = "_kmod_info";
	kmod_info_t *info;
	unsigned long value;
	int ok;

	ok = kld_lookup(symbol, &value);
	fflush(stdout);
	if (!ok) {
		fprintf(stderr, "%s: kld_lookup(%s) failed.\n", progname, symbol);
		cleanup_kernel_memory();
		exit(KMOD_ERROR_LOADING);
	}

	ok = kld_forget_symbol(symbol);
	fflush(stdout);
	if (!ok) {
		fprintf(stderr, "%s: kld_forget_symbol(%s) failed.\n", progname, symbol);
		cleanup_kernel_memory();
		exit(KMOD_ERROR_INTERNAL);
	}

       /* Get the kmod info by translating from the kernel address at value.
        */
	info = (kmod_info_t *)(value - (unsigned long)kernel_load_address + (unsigned long)mach_header);
	v_printf("%s: kmod name: %s\n", progname, info->name);
	v_printf("%s: kmod start @ 0x%x\n", progname, (vm_address_t)info->start);
	v_printf("%s: kmod stop  @ 0x%x\n", progname, (vm_address_t)info->stop);

       /* Record link info in kmod info struct, rounding the hdr_size to fit
        * the adjustment that was made.
        */
	info->address = kernel_alloc_address;
	info->size = kernel_alloc_size;
	info->hdr_size = page_round(kernel_hdr_size);

	if (!info->start) {
		fprintf(stderr, "%s: invalid start address?\n", progname);
		cleanup_kernel_memory();
		exit(KMOD_ERROR_LOADING);
	}
	if (!info->stop) {
		fprintf(stderr, "%s: invalid stop address?\n", progname);
		cleanup_kernel_memory();
		exit(KMOD_ERROR_LOADING);
	}

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

    v_printf("%s: kmod id %d successfully created at 0x%x size %ld.\n", 
        progname, id, kernel_alloc_address, kernel_alloc_size);

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
			exit(KMOD_ERROR_INTERNAL);
		}

		v_printf("%s: kmod id %d reference count was sucessfully incremented.\n", progname, module->id);

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
		exit(KMOD_ERROR_INTERNAL);
	}

	v_printf("%s: kmod id %d successfully started.\n", progname, id);
}

void
kld_error_vprintf(const char *format, va_list ap){
    vfprintf(stderr, format, ap);
    return;
}
