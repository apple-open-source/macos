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
#import <stdio.h>
#import <stdlib.h>
#import <sys/file.h>
#import <sys/types.h>
#import <mach/mach.h>
#import "stuff/openstep_mach.h"
#import <mach-o/gmon.h>
#import "stuff/ofile.h"
#import "stuff/errors.h"
#import "stuff/round.h"

static void create(
    char *dylib,
    char *gmon_out);

char *progname = NULL;

/*
 * dylib_pcsampler is invoked as follows:
 *
 *	% dylib_pcsampler dylib gmon.out
 *
 * where dylib is the file name of a dynamic library and gmon.out is the file
 * name of the gprof gmon.out file.
 */
int
main(
int argc,
char *argv[])
{
	progname = argv[0];
	if(argc != 3){
	    fprintf(stderr, "Usage: %s dylib gmon.out\n", progname);
	    exit(EXIT_FAILURE);
	}
	create(argv[1], argv[2]);
	return(EXIT_SUCCESS);
}

/*
 * create() takes the file name of a dynamic library (dylib) and creates a
 * gmon.out file (gmon_out).  It checks to see the dylib file is correct and
 * creates the gmon.out file proportional to the size of the (__TEXT,__text)
 * section of the dynamic library.
 */
static
void
create(
char *dylib,
char *gmon_out)
{
    struct arch_flag host_arch_flag;
    struct ofile ofile;
    unsigned long i, j, size;
    struct load_command *lc;
    struct segment_command *sg;
    struct section *s, *text_section;
    kern_return_t r;
    struct phdr *phdr;
    char *pcsample_buffer;
    int fd;

	/*
	 * Open and map in the dylib file and check it for correctness.
	 */
	if(get_arch_from_host(&host_arch_flag, NULL) == 0)
	    fatal("can't determine the host architecture");
	if(ofile_map(dylib, &host_arch_flag, NULL, &ofile, FALSE) == FALSE)
	    exit(EXIT_FAILURE);
	if(ofile.mh == NULL ||
	   (ofile.mh->filetype != MH_DYLIB &&
	    ofile.mh->filetype != MH_DYLINKER))
	    fatal("file: %s is not a Mach-O dynamic shared library file",
		  dylib);

	/*
	 * Get the text section for dynamic library.
	 */
	text_section = NULL;
	lc = ofile.load_commands;
	for(i = 0; i < ofile.mh->ncmds && text_section == NULL; i++){
	    if(lc->cmd == LC_SEGMENT){
		sg = (struct segment_command *)lc;
		s = (struct section *)
		      ((char *)sg + sizeof(struct segment_command));
		for(j = 0; j < sg->nsects; j++){
		    if(strcmp(s->sectname, SECT_TEXT) == 0 &&
		       strcmp(s->segname, SEG_TEXT) == 0){
			text_section = s;
			break;
		    }
		    s++;
		}
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
	if(text_section == NULL)
	    fatal("file: %s does not have a (" SEG_TEXT "," SECT_TEXT
		  ") section", dylib);

	/*
	 * Create a pcsample buffer for the text section.
	 */
	size = text_section->size / HASHFRACTION + sizeof(struct phdr);
	size = round(size, sizeof(unsigned short));
	r = vm_allocate(mach_task_self(), (vm_address_t *)&pcsample_buffer,
			(vm_size_t)size, TRUE);
	if(r != KERN_SUCCESS)
	    mach_fatal(r, "can't vm_allocate pcsample buffer of size: %lu",
		       size);

	/*
	 * Create and write the pcsample file. See comments in gmon.h for the
	 * values of the profile header (struct phdr).
	 */
	phdr = (struct phdr *)pcsample_buffer;
	phdr->lpc = (char *)(text_section->addr);
	phdr->hpc = (char *)(text_section->addr + text_section->size);
	phdr->ncnt = (int)size;
	if((fd = open(gmon_out, O_WRONLY | O_CREAT | O_TRUNC, 0777)) == -1)
	    system_fatal("can't create gmon.out file: %s", gmon_out);
	if(write(fd, pcsample_buffer, size) != (int)size)
	    system_fatal("can't write gmon.out file: %s", gmon_out);
	if(close(fd) == -1)
	    system_fatal("can't close gmon.out file: %s", gmon_out);
}
