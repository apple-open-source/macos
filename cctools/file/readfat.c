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
#ifdef BUILTIN_FAT
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>	/* for MAXPATHLEN */
#include <sys/file.h>
#include <unistd.h>     /* for read() */

#include <mach-o/fat.h>
#include <mach-o/arch.h>
#include <mach-o/swap.h>

#include "file.h"

static void print_arch_name_for_file(
    cpu_type_t cputype,
    cpu_subtype_t cpusubtype);

void
tryfat(
const char *inname,
int fd,
char *buf,
int nbytes)
{
    struct fat_header fat_header;
    struct fat_arch *fat_archs;
    unsigned long i, arch_size, tbytes;
    char *arch_buf;
    unsigned char tmpbuf[HOWMANY+1];	/* one extra for terminating '\0' */
    

	if(nbytes < sizeof(struct fat_header)){
	    return;
	}
	memcpy(&fat_header, buf, sizeof(struct fat_header));
#ifdef __LITTLE_ENDIAN__
	swap_fat_header(&fat_header, NX_LittleEndian);
#endif /* __LITTLE_ENDIAN__ */
	arch_size = fat_header.nfat_arch * sizeof(struct fat_arch);
	if(arch_size + sizeof(struct fat_header) > nbytes){
	    return;
	}
	arch_buf = malloc(nbytes);
	if(arch_buf == NULL)
	    return;
	memcpy(arch_buf, buf + sizeof(struct fat_header), arch_size);
	fat_archs = (struct fat_arch *)(arch_buf);
#ifdef __LITTLE_ENDIAN__
	swap_fat_arch(fat_archs, fat_header.nfat_arch, NX_LittleEndian);
#endif /* __LITTLE_ENDIAN__ */
	for(i = 0; i < fat_header.nfat_arch; i++){
	    printf("\n%s", inname);
	    print_arch_name_for_file(fat_archs[i].cputype,
				     fat_archs[i].cpusubtype);
	    printf(":\t");
	    lseek(fd, fat_archs[i].offset, L_SET);
	    /*
	     * try looking at the first HOWMANY bytes
	     */
	    if ((tbytes = read(fd, (char *)tmpbuf, HOWMANY)) == -1) {
		error("read failed (%s).\n", strerror(errno));
		/*NOTREACHED*/
	    }
	    tryit(tmpbuf, tbytes, 0);
	}
}

static
void
print_arch_name_for_file(
cpu_type_t cputype,
cpu_subtype_t cpusubtype)
{
    const NXArchInfo *ArchInfoTable, *ai;

	ArchInfoTable = NXGetAllArchInfos();
	for(ai = ArchInfoTable; ai->name != NULL; ai++){
	    if(ai->cputype == cputype &&
	       ai->cpusubtype == cpusubtype){
		printf(" (for architecture %s)", ai->name);
		return;
	    }
	}
	printf(" (for architecture cputype (%d) cpusubtype (%d))",
	       cputype, cpusubtype);
}
#endif /* BUILTIN_FAT */
