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

/* Known values for GPU cpu_type_t */
#define CPU_TYPE_APPLEGPU	((cpu_type_t)	(19 | CPU_ARCH_ABI64))
#define CPU_TYPE_AMDGPU		((cpu_type_t)	(20 | CPU_ARCH_ABI64))
#define CPU_TYPE_INTELGPU	((cpu_type_t)	(21 | CPU_ARCH_ABI64))
#define CPU_TYPE_AIR64		((cpu_type_t)	(23 | CPU_ARCH_ABI64))

/* Known values for Apple cpu_subtype_t */
#define CPU_SUBTYPE_APPLEGPU_GX2	((cpu_subtype_t) 32)
#define CPU_SUBTYPE_APPLEGPU_G4P	((cpu_subtype_t) 17)
#define CPU_SUBTYPE_APPLEGPU_G4G	((cpu_subtype_t) 49)
#define CPU_SUBTYPE_APPLEGPU_G5P	((cpu_subtype_t) 65)
#define CPU_SUBTYPE_APPLEGPU_G9P	((cpu_subtype_t) 81)
#define CPU_SUBTYPE_APPLEGPU_G9G	((cpu_subtype_t) 97)
#define CPU_SUBTYPE_APPLEGPU_G10P	((cpu_subtype_t) 34)
#define CPU_SUBTYPE_APPLEGPU_G11P	((cpu_subtype_t) 114)
#define CPU_SUBTYPE_APPLEGPU_G11M	((cpu_subtype_t) 82)
#define CPU_SUBTYPE_APPLEGPU_G11G	((cpu_subtype_t) 130)
#define CPU_SUBTYPE_APPLEGPU_G11G_8FSTP	((cpu_subtype_t) 1602)
#define CPU_SUBTYPE_APPLEGPU_G12P	((cpu_subtype_t) 210)
#define CPU_SUBTYPE_APPLEGPU_G13P	((cpu_subtype_t) 290)
#define CPU_SUBTYPE_APPLEGPU_G13G	((cpu_subtype_t) 322)
#define CPU_SUBTYPE_APPLEGPU_G13S	((cpu_subtype_t) 530)
#define CPU_SUBTYPE_APPLEGPU_G13C	((cpu_subtype_t) 562)
#define CPU_SUBTYPE_APPLEGPU_G13D	((cpu_subtype_t) 594)
#define CPU_SUBTYPE_APPLEGPU_G14P	((cpu_subtype_t) 370)
#define CPU_SUBTYPE_APPLEGPU_G14G	((cpu_subtype_t) 402)
#define CPU_SUBTYPE_APPLEGPU_G14S	((cpu_subtype_t) 434)
#define CPU_SUBTYPE_APPLEGPU_G14D	((cpu_subtype_t) 498)
#define CPU_SUBTYPE_APPLEGPU_G15P	((cpu_subtype_t) 610)
#define CPU_SUBTYPE_APPLEGPU_G15G	((cpu_subtype_t) 275)
#define CPU_SUBTYPE_APPLEGPU_G15S	((cpu_subtype_t) 419)
#define CPU_SUBTYPE_APPLEGPU_G16P	((cpu_subtype_t) 435)

/* Known values for AMD cpu_subtype_t */
#define CPU_SUBTYPE_AMD_GFX600		((cpu_subtype_t) 4000)
#define CPU_SUBTYPE_AMD_GFX600_NWH	((cpu_subtype_t) 4001)
#define CPU_SUBTYPE_AMD_GFX701		((cpu_subtype_t) 4002)
#define CPU_SUBTYPE_AMD_GFX704		((cpu_subtype_t) 4003)
#define CPU_SUBTYPE_AMD_GFX803		((cpu_subtype_t) 4004)
#define CPU_SUBTYPE_AMD_GFX802		((cpu_subtype_t) 4005)
#define CPU_SUBTYPE_AMD_GFX900		((cpu_subtype_t) 5000)
#define CPU_SUBTYPE_AMD_GFX904		((cpu_subtype_t) 5001)
#define CPU_SUBTYPE_AMD_GFX906		((cpu_subtype_t) 5002)
#define CPU_SUBTYPE_AMD_GFX1010_NSGC	((cpu_subtype_t) 6000)
#define CPU_SUBTYPE_AMD_GFX1010		((cpu_subtype_t) 6001)
#define CPU_SUBTYPE_AMD_GFX1011		((cpu_subtype_t) 6002)
#define CPU_SUBTYPE_AMD_GFX1012		((cpu_subtype_t) 6003)
#define CPU_SUBTYPE_AMD_GFX1030		((cpu_subtype_t) 6004)
#define CPU_SUBTYPE_AMD_GFX1032		((cpu_subtype_t) 6005)

/* Known values for Intel cpu_subtype_t */
#define CPU_SUBTYPE_INTEL_SKL_GT2R6	((cpu_subtype_t) 590342)
#define CPU_SUBTYPE_INTEL_SKL_GT2R7	((cpu_subtype_t) 590343)
#define CPU_SUBTYPE_INTEL_SKL_GT3R10	((cpu_subtype_t) 590602)
#define CPU_SUBTYPE_INTEL_KBL_GT2R0	((cpu_subtype_t) 9765376)
#define CPU_SUBTYPE_INTEL_KBL_GT2R2	((cpu_subtype_t) 9765378)
#define CPU_SUBTYPE_INTEL_KBL_GT2R4	((cpu_subtype_t) 9765380)
#define CPU_SUBTYPE_INTEL_KBL_GT3R1	((cpu_subtype_t) 9765633)
#define CPU_SUBTYPE_INTEL_KBL_GT3R6	((cpu_subtype_t) 9765638)
#define CPU_SUBTYPE_INTEL_ICL_1X6X8R7	((cpu_subtype_t) 1115655)
#define CPU_SUBTYPE_INTEL_ICL_1X8X8R7	((cpu_subtype_t) 1116167)

/* Known values for AIR cpu_subtype_t */
#define CPU_SUBTYPE_AIR_V16	((cpu_subtype_t) 1)
#define CPU_SUBTYPE_AIR_V18	((cpu_subtype_t) 2)
#define CPU_SUBTYPE_AIR_V111	((cpu_subtype_t) 3)
#define CPU_SUBTYPE_AIR_V20	((cpu_subtype_t) 4)
#define CPU_SUBTYPE_AIR_V21	((cpu_subtype_t) 5)
#define CPU_SUBTYPE_AIR_V22	((cpu_subtype_t) 6)
#define CPU_SUBTYPE_AIR_V23	((cpu_subtype_t) 7)
#define CPU_SUBTYPE_AIR_V24	((cpu_subtype_t) 8)
#define CPU_SUBTYPE_AIR_V25	((cpu_subtype_t) 9)
#define CPU_SUBTYPE_AIR_V26	((cpu_subtype_t) 10)

/* Magic number for fat GPU files */
#define FAT_GPU_MAGIC 0xcbfebabe

static const NXArchInfo GPUArchInfoTable[] =
{
	/* Apple GPUs */
	{"applegpu_gx2",	CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_GX2,	NX_LittleEndian,
	 "APPLEGPU_GX2"},
	{"applegpu_g4p",	CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G4P,	NX_LittleEndian,
	 "APPLEGPU_G4P"},
	{"applegpu_g4g",	CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G4G,	NX_LittleEndian,
	 "APPLEGPU_G4G"},
	{"applegpu_g5p",	CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G5P,	NX_LittleEndian,
	 "APPLEGPU_G5P"},
	{"applegpu_g9p",	CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G9P,	NX_LittleEndian,
	 "APPLEGPU_G9P"},
	{"applegpu_g9g",	CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G9G,	NX_LittleEndian,
	 "APPLEGPU_G9G"},
	{"applegpu_g10p",	CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G10P,	NX_LittleEndian,
	 "APPLEGPU_G10P"},
	{"applegpu_g11p",	CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G11P,	NX_LittleEndian,
	 "APPLEGPU_G11P"},
	{"applegpu_g11m",	CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G11M,	NX_LittleEndian,
	 "APPLEGPU_G11M"},
	{"applegpu_g11g",	CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G11G,	NX_LittleEndian,
	 "APPLEGPU_G11G"},
	{"applegpu_g11g_8fstp",	CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G11G_8FSTP,	NX_LittleEndian,
	 "APPLEGPU_G11G_8FSTP"},
	{"applegpu_g12p",	CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G12P,	NX_LittleEndian,
	 "APPLEGPU_G12P"},
	{"applegpu_g13p",	CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G13P,	NX_LittleEndian,
	 "APPLEGPU_G13P"},
	{"applegpu_g13g",	CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G13G,	NX_LittleEndian,
	 "APPLEGPU_G13G"},
	{"applegpu_g13s",	CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G13S,	NX_LittleEndian,
	 "APPLEGPU_G13S"},
	{"applegpu_g13c",	CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G13C,	NX_LittleEndian,
	 "APPLEGPU_G13C"},
	{"applegpu_g13d",	CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G13D,	NX_LittleEndian,
	 "APPLEGPU_G13D"},
	{"applegpu_g14p",	CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G14P,	NX_LittleEndian,
	 "APPLEGPU_G14P"},
	{"applegpu_g14g",	CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G14G,	NX_LittleEndian,
	 "APPLEGPU_G14G"},
	{"applegpu_g14s",	CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G14S,	NX_LittleEndian,
	 "APPLEGPU_G14S"},
	{"applegpu_g14d",	CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G14D,	NX_LittleEndian,
	 "APPLEGPU_G14D"},
	{"applegpu_g15p",	CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G15P,	NX_LittleEndian,
	 "APPLEGPU_G15P"},
	{"applegpu_g15g",	CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G15G,	NX_LittleEndian,
	 "APPLEGPU_G15G"},
	{"applegpu_g15s",	CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G15S,	NX_LittleEndian,
	 "APPLEGPU_G15S"},
	{"applegpu_g16p",	CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G16P,	NX_LittleEndian,
	 "APPLEGPU_G16P"},
	/* AMD GPUs */
	{"amdgpu_gfx600",	CPU_TYPE_AMDGPU,	CPU_SUBTYPE_AMD_GFX600,	NX_LittleEndian,
	 "AMDGPU_GFX600"},
	{"amdgpu_gfx600_nwh",	CPU_TYPE_AMDGPU,	CPU_SUBTYPE_AMD_GFX600_NWH,	NX_LittleEndian,
	 "AMDGPU_GFX600_NWH"},
	{"amdgpu_gfx701",	CPU_TYPE_AMDGPU,	CPU_SUBTYPE_AMD_GFX701,	NX_LittleEndian,
	 "AMDGPU_GFX701"},
	{"amdgpu_gfx704",	CPU_TYPE_AMDGPU,	CPU_SUBTYPE_AMD_GFX704,	NX_LittleEndian,
	 "AMDGPU_GFX704"},
	{"amdgpu_gfx803",	CPU_TYPE_AMDGPU,	CPU_SUBTYPE_AMD_GFX803,	NX_LittleEndian,
	 "AMDGPU_GFX803"},
	{"amdgpu_gfx802",	CPU_TYPE_AMDGPU,	CPU_SUBTYPE_AMD_GFX802,	NX_LittleEndian,
	 "AMDGPU_GFX802"},
	{"amdgpu_gfx900",	CPU_TYPE_AMDGPU,	CPU_SUBTYPE_AMD_GFX900,	NX_LittleEndian,
	 "AMDGPU_GFX900"},
	{"amdgpu_gfx904",	CPU_TYPE_AMDGPU,	CPU_SUBTYPE_AMD_GFX904,	NX_LittleEndian,
	 "AMDGPU_GFX904"},
	{"amdgpu_gfx906",	CPU_TYPE_AMDGPU,	CPU_SUBTYPE_AMD_GFX906,	NX_LittleEndian,
	 "AMDGPU_GFX906"},
	{"amdgpu_gfx1010_nsgc",		CPU_TYPE_AMDGPU,	CPU_SUBTYPE_AMD_GFX1010_NSGC,	NX_LittleEndian,
	 "AMDGPU_GFX1010_NSGC"},
	{"amdgpu_gfx1010",	CPU_TYPE_AMDGPU,	CPU_SUBTYPE_AMD_GFX1010,	NX_LittleEndian,
	 "AMDGPU_GFX1010"},
	{"amdgpu_gfx1011",	CPU_TYPE_AMDGPU,	CPU_SUBTYPE_AMD_GFX1011,	NX_LittleEndian,
	 "AMDGPU_GFX1011"},
	{"amdgpu_gfx1012",	CPU_TYPE_AMDGPU,	CPU_SUBTYPE_AMD_GFX1012,	NX_LittleEndian,
	 "AMDGPU_GFX1012"},
	{"amdgpu_gfx1030",	CPU_TYPE_AMDGPU,	CPU_SUBTYPE_AMD_GFX1030,	NX_LittleEndian,
	 "AMDGPU_GFX1030"},
	{"amdgpu_gfx1032",	CPU_TYPE_AMDGPU,	CPU_SUBTYPE_AMD_GFX1032,	NX_LittleEndian,
	 "AMDGPU_GFX1032"},
	/* Intel GPUs */
	{"intelgpu_skl_gt2r6",	CPU_TYPE_INTELGPU,	CPU_SUBTYPE_INTEL_SKL_GT2R6,	NX_LittleEndian,
	 "INTELGPU_SKL_GT2R6"},
	{"intelgpu_skl_gt2r7",	CPU_TYPE_INTELGPU,	CPU_SUBTYPE_INTEL_SKL_GT2R7,	NX_LittleEndian,
	 "INTELGPU_SKL_GT2R7"},
	{"intelgpu_skl_gt3r10",	CPU_TYPE_INTELGPU,	CPU_SUBTYPE_INTEL_SKL_GT3R10,	NX_LittleEndian,
	 "INTELGPU_SKL_GT3R10"},
	{"intelgpu_kbl_gt2r0",	CPU_TYPE_INTELGPU,	CPU_SUBTYPE_INTEL_KBL_GT2R0,	NX_LittleEndian,
	 "INTELGPU_KBL_GT2R0"},
	{"intelgpu_kbl_gt2r2",	CPU_TYPE_INTELGPU,	CPU_SUBTYPE_INTEL_KBL_GT2R2,	NX_LittleEndian,
	 "INTELGPU_KBL_GT2R2"},
	{"intelgpu_kbl_gt2r4",	CPU_TYPE_INTELGPU,	CPU_SUBTYPE_INTEL_KBL_GT2R4,	NX_LittleEndian,
	 "INTELGPU_KBL_GT2R4"},
	{"intelgpu_kbl_gt3r1",	CPU_TYPE_INTELGPU,	CPU_SUBTYPE_INTEL_KBL_GT3R1,	NX_LittleEndian,
	 "INTELGPU_KBL_GT3R1"},
	{"intelgpu_kbl_gt3r6",	CPU_TYPE_INTELGPU,	CPU_SUBTYPE_INTEL_KBL_GT3R6,	NX_LittleEndian,
	 "INTELGPU_KBL_GT3R6"},
	{"intelgpu_icl_1x6x8r7",	CPU_TYPE_INTELGPU,	CPU_SUBTYPE_INTEL_ICL_1X6X8R7,	NX_LittleEndian,
	 "INTELGPU_ICL_1X6X8R7"},
	{"intelgpu_icl_1x8x8r7",	CPU_TYPE_INTELGPU,	CPU_SUBTYPE_INTEL_ICL_1X8X8R7,	NX_LittleEndian,
	 "INTELGPU_ICL_1X8X8R7"},
	/* AIR */
	{"air64_v16",	CPU_TYPE_AIR64,	CPU_SUBTYPE_AIR_V16,	NX_LittleEndian,
	 "AIR64_V16"},
	{"air64_v18",	CPU_TYPE_AIR64,	CPU_SUBTYPE_AIR_V18,	NX_LittleEndian,
	 "AIR64_V18"},
	{"air64_v111",	CPU_TYPE_AIR64,	CPU_SUBTYPE_AIR_V111,	NX_LittleEndian,
	 "AIR64_V111"},
	{"air64_v20",	CPU_TYPE_AIR64,	CPU_SUBTYPE_AIR_V20,	NX_LittleEndian,
	 "AIR64_V20"},
	{"air64_v21",	CPU_TYPE_AIR64,	CPU_SUBTYPE_AIR_V21,	NX_LittleEndian,
	 "AIR64_V21"},
	{"air64_v22",	CPU_TYPE_AIR64,	CPU_SUBTYPE_AIR_V22,	NX_LittleEndian,
	 "AIR64_V22"},
	{"air64_v23",	CPU_TYPE_AIR64,	CPU_SUBTYPE_AIR_V23,	NX_LittleEndian,
	 "AIR64_V23"},
	{"air64_v24",	CPU_TYPE_AIR64,	CPU_SUBTYPE_AIR_V24,	NX_LittleEndian,
	 "AIR64_V24"},
	{"air64_v25",	CPU_TYPE_AIR64,	CPU_SUBTYPE_AIR_V25,	NX_LittleEndian,
	 "AIR64_V25"},
	{"air64_v26",	CPU_TYPE_AIR64,	CPU_SUBTYPE_AIR_V26,	NX_LittleEndian,
	 "AIR64_V26"},
	{NULL,	0,	0,	0,
	 NULL}
};

static const NXArchInfo *GetAllGPUArchInfos(void)
{
	return GPUArchInfoTable;
}

static int
print_known_arch_name_for_file(struct magic_set *ms, cpu_type_t cputype,
	cpu_subtype_t cpusubtype, const NXArchInfo *ArchInfoTable)
{
	const NXArchInfo *ai;

	for (ai = ArchInfoTable; ai->name != NULL; ai++) {
		if(ai->cputype == cputype && ai->cpusubtype == (cpu_subtype_t)(cpusubtype & ~CPU_SUBTYPE_MASK)) {
			file_printf(ms, " (for architecture %s)", ai->name);
			return 0;
		}
	}

	return -1;
}

static void
print_arch_name_for_file(struct magic_set *ms, cpu_type_t cputype,
	cpu_subtype_t cpusubtype)
{
	if (print_known_arch_name_for_file(ms, cputype, cpusubtype,
		NXGetAllArchInfos()) == 0)
		return;

	if (print_known_arch_name_for_file(ms, cputype, cpusubtype,
		GetAllGPUArchInfos()) == 0)
		return;

	file_printf(ms, " (for architecture cputype (%d) cpusubtype (%d))",
		cputype, cpusubtype);
}

protected int
file_trymacho(struct magic_set *ms, const struct buffer *b, const char *inname)
{
	int fd = b->fd;
	const unsigned char *buf = b->fbuf;
	size_t nbytes = b->flen;
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
	if(!((fat_header.magic == FAT_MAGIC && fat_header.nfat_arch < 20) ||
	      fat_header.magic == FAT_GPU_MAGIC && fat_header.nfat_arch < 20)) {
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

		tmpbuf = calloc(1,ms->bytes_max + 1);
		if ((tbytes = read(fd, tmpbuf, ms->bytes_max)) == -1) {
			free(fat_archs);
			free(tmpbuf);
			return -1;
		}

		file_buffer(ms, -1, NULL, inname, tmpbuf, (size_t)tbytes);
		free(tmpbuf);
	}

	free(fat_archs);
	return 0;
}
#endif /* BUILTIN_MACHO */
