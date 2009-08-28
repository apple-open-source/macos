/*
 * Copyright (c) 1999-2009 Apple Inc. All rights reserved.
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

#ifdef BUILTIN_MACHO
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <unistd.h>

#include <mach-o/fat.h>
#include <mach-o/arch.h>
#include <mach-o/swap.h>

#include "file.h"

/* Silence a compiler warning. */
FILE_RCSID("")

static void
print_arch_name_for_file(struct magic_set *ms, cpu_type_t cputype,
	cpu_subtype_t cpusubtype)
{
	const NXArchInfo *ArchInfoTable, *ai;

	ArchInfoTable = NXGetAllArchInfos();

	for (ai = ArchInfoTable; ai->name != NULL; ai++) {
		if(ai->cputype == cputype && ai->cpusubtype == (cpu_subtype_t)(cpusubtype & ~CPU_SUBTYPE_MASK)) {
			file_printf(ms, " (for architecture %s)", ai->name);
			return;
		}
	}

	file_printf(ms, " (for architecture cputype (%d) cpusubtype (%d))",
		cputype, cpusubtype);
}

protected int
file_trymacho(struct magic_set *ms, int fd, const unsigned char *buf,
	size_t nbytes, const char *inname)
{
	struct stat stat_buf;
	unsigned long size;
	struct fat_header fat_header;
	struct fat_arch *fat_archs;
	uint32_t arch_size, i;
	ssize_t tbytes;
	unsigned char *tmpbuf;

	if (fstat(fd, &stat_buf) == -1) {
		return -1;
	}

	size = stat_buf.st_size;

	if (nbytes < sizeof(struct fat_header)) {
		return -1;
	}

	memcpy(&fat_header, buf, sizeof(struct fat_header));
#ifdef __LITTLE_ENDIAN__
	swap_fat_header(&fat_header, NX_LittleEndian);
#endif /* __LITTLE_ENDIAN__ */

	/* Check magic number, plus little hack for Mach-O vs. Java. */
	if(!(fat_header.magic == FAT_MAGIC && fat_header.nfat_arch < 20)) {
		return -1;
	}

	arch_size = fat_header.nfat_arch * sizeof(struct fat_arch);

	if (nbytes < sizeof(struct fat_header) + arch_size) {
		return -1;
	}

	if ((fat_archs = (struct fat_arch *)malloc(arch_size)) == NULL) {
		return -1;
	}

	memcpy((void *)fat_archs, buf + sizeof(struct fat_header), arch_size);
#ifdef __LITTLE_ENDIAN__
	swap_fat_arch(fat_archs, fat_header.nfat_arch, NX_LittleEndian);
#endif /* __LITTLE_ENDIAN__ */

	for(i = 0; i < fat_header.nfat_arch; i++) {
		file_printf(ms, "\n%s", inname);
		print_arch_name_for_file(ms,
			fat_archs[i].cputype, fat_archs[i].cpusubtype);
		file_printf(ms, ":\t");

		if (fat_archs[i].offset + fat_archs[i].size > size) {
			free(fat_archs);
			return -1;
		}

		if (lseek(fd, fat_archs[i].offset, SEEK_SET) == -1) {
			free(fat_archs);
			return -1;
		}

		tmpbuf = malloc(HOWMANY + 1);
		memset(tmpbuf, 0, sizeof(tmpbuf));
		if ((tbytes = read(fd, tmpbuf, HOWMANY)) == -1) {
			free(fat_archs);
			free(tmpbuf);
			return -1;
		}

		file_buffer(ms, -1, inname, tmpbuf, (size_t)tbytes);
		free(tmpbuf);
	}

	free(fat_archs);
	return 0;
}
#endif /* BUILTIN_MACHO */
