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
#include <stdio.h>
#include <stdlib.h>
#include "mach/mach.h"
#include "stuff/ofile.h"
#include "stuff/errors.h"
#include "stuff/allocate.h"

char *progname = NULL;

void
main(
int argc,
char *argv[])
{
    unsigned long i;
    struct ofile ofile;
    struct arch_flag *arch_flag;
    char *file_name, *object_name;

	progname = argv[0];
        arch_flag = NULL;
        file_name = NULL;
        object_name = NULL;
	for(i = 1; i < argc; i++){
	    if(argv[i][0] == '-'){
		arch_flag = allocate(sizeof(struct arch_flag));
		if(get_arch_from_flag(argv[i] + 1, arch_flag) == 0)
		    fatal("unknown arch flag %s", argv[i]);
	    }
	    else if(file_name == NULL)
		file_name = argv[i];
	    else
		object_name = argv[i];
	}
	if(ofile_map(file_name, arch_flag, object_name, &ofile, TRUE) == TRUE)
	    ofile_print(&ofile);
	exit(0);
}

void
ofile_print(
struct ofile *ofile)
{
	printf("file_name = %s\n", ofile->file_name);
	printf("file_addr = 0x%x\n", (unsigned int)ofile->file_addr);
	printf("file_size = 0x%x\n", (unsigned int)ofile->file_size);
	printf("file_type = 0x%x\n", (unsigned int)ofile->file_type);
	printf("fat_header = 0x%x\n", (unsigned int)ofile->fat_header);
	printf("fat_archs = 0x%x\n", (unsigned int)ofile->fat_archs);
	printf("narch = 0x%x\n", (unsigned int)ofile->narch);
	printf("arch_type = 0x%x\n", (unsigned int)ofile->arch_type);
	printf("arch_flag.name = %s\n", ofile->arch_flag.name);
	printf("arch_flag.cputype = 0x%x\n",
		(unsigned int)ofile->arch_flag.cputype);
	printf("arch_flag.cpusubtype = 0x%x\n",
		(unsigned int)ofile->arch_flag.cpusubtype);
	printf("member_offset = 0x%x\n", (unsigned int)ofile->member_offset);
	printf("member_addr = 0x%x\n", (unsigned int)ofile->member_addr);
	printf("member_size = 0x%x\n", (unsigned int)ofile->member_size);
	printf("member_ar_hdr = 0x%x\n", (unsigned int)ofile->member_ar_hdr);
	printf("member_type = 0x%x\n", (unsigned int)ofile->member_type);
	printf("archive_cputype = 0x%x\n",
		(unsigned int)ofile->archive_cputype);
	printf("archive_cpusubtype = 0x%x\n",
		(unsigned int)ofile->archive_cpusubtype);
	printf("object_addr = 0x%x\n", (unsigned int)ofile->object_addr);
	printf("object_size = 0x%x\n", (unsigned int)ofile->object_size);
	printf("object_byte_sex = 0x%x\n",
		(unsigned int)ofile->object_byte_sex);
	printf("mh = 0x%x\n", (unsigned int)ofile->mh);
	printf("load_commands = 0x%x\n", (unsigned int)ofile->load_commands);
}
