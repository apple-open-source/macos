/*
 * Copyright (c) 1999-2025 Apple Inc. All rights reserved.
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
#include <stdckdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <mach-o/arch.h>
#include <mach-o/fat.h>
#include <mach-o/utils.h>

#include "file.h"

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
#define FAT_GPU_CIGAM 0xbebafecb

static const struct ArchInfo {
	const char	*name;
	cpu_type_t	 cputype;
	cpu_subtype_t	 cpusubtype;
} GPUArchInfoTable[] = {
	/* Apple GPUs */
	{"applegpu_gx2",		CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_GX2 },
	{"applegpu_g4p",		CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G4P },
	{"applegpu_g4g",		CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G4G },
	{"applegpu_g5p",		CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G5P },
	{"applegpu_g9p",		CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G9P },
	{"applegpu_g9g",		CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G9G },
	{"applegpu_g10p",		CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G10P },
	{"applegpu_g11p",		CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G11P },
	{"applegpu_g11m",		CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G11M },
	{"applegpu_g11g",		CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G11G },
	{"applegpu_g11g_8fstp",		CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G11G_8FSTP },
	{"applegpu_g12p",		CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G12P },
	{"applegpu_g13p",		CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G13P },
	{"applegpu_g13g",		CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G13G },
	{"applegpu_g13s",		CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G13S },
	{"applegpu_g13c",		CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G13C },
	{"applegpu_g13d",		CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G13D },
	{"applegpu_g14p",		CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G14P },
	{"applegpu_g14g",		CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G14G },
	{"applegpu_g14s",		CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G14S },
	{"applegpu_g14d",		CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G14D },
	{"applegpu_g15p",		CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G15P },
	{"applegpu_g15g",		CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G15G },
	{"applegpu_g15s",		CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G15S },
	{"applegpu_g16p",		CPU_TYPE_APPLEGPU,	CPU_SUBTYPE_APPLEGPU_G16P },
	/* AMD GPUs */
	{"amdgpu_gfx600",		CPU_TYPE_AMDGPU,	CPU_SUBTYPE_AMD_GFX600 },
	{"amdgpu_gfx600_nwh",		CPU_TYPE_AMDGPU,	CPU_SUBTYPE_AMD_GFX600_NWH },
	{"amdgpu_gfx701",		CPU_TYPE_AMDGPU,	CPU_SUBTYPE_AMD_GFX701 },
	{"amdgpu_gfx704",		CPU_TYPE_AMDGPU,	CPU_SUBTYPE_AMD_GFX704 },
	{"amdgpu_gfx803",		CPU_TYPE_AMDGPU,	CPU_SUBTYPE_AMD_GFX803 },
	{"amdgpu_gfx802",		CPU_TYPE_AMDGPU,	CPU_SUBTYPE_AMD_GFX802 },
	{"amdgpu_gfx900",		CPU_TYPE_AMDGPU,	CPU_SUBTYPE_AMD_GFX900 },
	{"amdgpu_gfx904",		CPU_TYPE_AMDGPU,	CPU_SUBTYPE_AMD_GFX904 },
	{"amdgpu_gfx906",		CPU_TYPE_AMDGPU,	CPU_SUBTYPE_AMD_GFX906 },
	{"amdgpu_gfx1010_nsgc",		CPU_TYPE_AMDGPU,	CPU_SUBTYPE_AMD_GFX1010_NSGC },
	{"amdgpu_gfx1010",		CPU_TYPE_AMDGPU,	CPU_SUBTYPE_AMD_GFX1010 },
	{"amdgpu_gfx1011",		CPU_TYPE_AMDGPU,	CPU_SUBTYPE_AMD_GFX1011 },
	{"amdgpu_gfx1012",		CPU_TYPE_AMDGPU,	CPU_SUBTYPE_AMD_GFX1012 },
	{"amdgpu_gfx1030",		CPU_TYPE_AMDGPU,	CPU_SUBTYPE_AMD_GFX1030 },
	{"amdgpu_gfx1032",		CPU_TYPE_AMDGPU,	CPU_SUBTYPE_AMD_GFX1032 },
	/* Intel GPUs */
	{"intelgpu_skl_gt2r6",		CPU_TYPE_INTELGPU,	CPU_SUBTYPE_INTEL_SKL_GT2R6 },
	{"intelgpu_skl_gt2r7",		CPU_TYPE_INTELGPU,	CPU_SUBTYPE_INTEL_SKL_GT2R7 },
	{"intelgpu_skl_gt3r10",		CPU_TYPE_INTELGPU,	CPU_SUBTYPE_INTEL_SKL_GT3R10 },
	{"intelgpu_kbl_gt2r0",		CPU_TYPE_INTELGPU,	CPU_SUBTYPE_INTEL_KBL_GT2R0 },
	{"intelgpu_kbl_gt2r2",		CPU_TYPE_INTELGPU,	CPU_SUBTYPE_INTEL_KBL_GT2R2 },
	{"intelgpu_kbl_gt2r4",		CPU_TYPE_INTELGPU,	CPU_SUBTYPE_INTEL_KBL_GT2R4 },
	{"intelgpu_kbl_gt3r1",		CPU_TYPE_INTELGPU,	CPU_SUBTYPE_INTEL_KBL_GT3R1 },
	{"intelgpu_kbl_gt3r6",		CPU_TYPE_INTELGPU,	CPU_SUBTYPE_INTEL_KBL_GT3R6 },
	{"intelgpu_icl_1x6x8r7",	CPU_TYPE_INTELGPU,	CPU_SUBTYPE_INTEL_ICL_1X6X8R7 },
	{"intelgpu_icl_1x8x8r7",	CPU_TYPE_INTELGPU,	CPU_SUBTYPE_INTEL_ICL_1X8X8R7 },
	/* AIR */
	{"air64_v16",			CPU_TYPE_AIR64,		CPU_SUBTYPE_AIR_V16 },
	{"air64_v18",			CPU_TYPE_AIR64,		CPU_SUBTYPE_AIR_V18 },
	{"air64_v111",			CPU_TYPE_AIR64,		CPU_SUBTYPE_AIR_V111 },
	{"air64_v20",			CPU_TYPE_AIR64,		CPU_SUBTYPE_AIR_V20 },
	{"air64_v21",			CPU_TYPE_AIR64,		CPU_SUBTYPE_AIR_V21 },
	{"air64_v22",			CPU_TYPE_AIR64,		CPU_SUBTYPE_AIR_V22 },
	{"air64_v23",			CPU_TYPE_AIR64,		CPU_SUBTYPE_AIR_V23 },
	{"air64_v24",			CPU_TYPE_AIR64,		CPU_SUBTYPE_AIR_V24 },
	{"air64_v25",			CPU_TYPE_AIR64,		CPU_SUBTYPE_AIR_V25 },
	{"air64_v26",			CPU_TYPE_AIR64,		CPU_SUBTYPE_AIR_V26 },
	{ 0 }
};

static const char *
gpu_arch_name_for_cpu_type(cpu_type_t cputype, cpu_subtype_t cpusubtype)
{
	const struct ArchInfo *ai;

	for (ai = GPUArchInfoTable; ai->name != NULL; ai++) {
		if (ai->cputype == cputype && ai->cpusubtype == cpusubtype) {
			return ai->name;
		}
	}
	return NULL;
}

static void
print_arch_name_for_cpu_type(struct magic_set *ms, cpu_type_t cputype,
    cpu_subtype_t cpusubtype)
{
	const char *arch_name = macho_arch_name_for_cpu_type(cputype, cpusubtype);

	if (arch_name == NULL) {
		/* could be a GPU, which libmacho does not recognize */
		arch_name = gpu_arch_name_for_cpu_type(cputype, cpusubtype);
	}

	if (arch_name != NULL) {
		file_printf(ms, " (for architecture %s)", arch_name);
		return;
	}

	file_printf(ms, " (for architecture cputype (%d) cpusubtype (%d))",
	    cputype, cpusubtype);
}

static void
swap32(void *buf, size_t len)
{
	for (uint32_t *u32 = buf; len >= sizeof(*u32); u32++, len -= sizeof(*u32)) {
		*u32 = __builtin_bswap32(*u32);
	}
}

protected int
file_trymacho(struct magic_set *ms, const struct buffer *b, const char *inname)
{
	struct fat_header fhdr;
	struct fat_arch ahdr;
	struct mach_header mhdr;
	off_t offset;
	bool swap = false;

	if (b->flen < sizeof(fhdr) + sizeof(ahdr) + sizeof(mhdr)) {
		/* too small for even a single empty slice */
		return -1;
	}
	memcpy(&fhdr, b->fbuf, sizeof(fhdr));
	switch (fhdr.magic) {
	case FAT_GPU_MAGIC:
	case FAT_MAGIC:
		break;
	case FAT_GPU_CIGAM:
	case FAT_CIGAM:
		swap32(&fhdr, sizeof(fhdr));
		swap = true;
		break;
	default:
		return -1;
	}

	for (unsigned int i = 0; i < fhdr.nfat_arch; i++) {
		if (ckd_mul(&offset, sizeof(ahdr), i) ||
		    ckd_add(&offset, offset, sizeof(fhdr))) {
			return -1;
		}
		if (offset <= b->flen - sizeof(ahdr)) {
			memcpy(&ahdr, b->fbuf + offset, sizeof(ahdr));
		} else if (lseek(b->fd, offset, SEEK_SET) != offset ||
		    read(b->fd, &ahdr, sizeof(ahdr)) != sizeof(ahdr)) {
			return -1;
		}
		if (swap) {
			swap32(&ahdr, sizeof(ahdr));
		}
		if (ahdr.size < sizeof(mhdr)) {
			return -1;
		}
		if (ahdr.offset <= b->flen - sizeof(mhdr)) {
			memcpy(&mhdr, b->fbuf + ahdr.offset, sizeof(mhdr));
		} else if (lseek(b->fd, ahdr.offset, SEEK_SET) != ahdr.offset ||
		    read(b->fd, &mhdr, sizeof(mhdr)) != sizeof(mhdr)) {
			return -1;
		}
		file_printf(ms, "\n%s", inname);
		print_arch_name_for_cpu_type(ms, ahdr.cputype,
		    ahdr.cpusubtype & ~CPU_SUBTYPE_MASK);
		file_printf(ms, ":\t");
		file_buffer(ms, -1, NULL, inname, &mhdr, sizeof(mhdr));
	}
	return 0;
}
#endif /* BUILTIN_MACHO */
