/*
 * Copyright (c) 2007-2016 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/*
 * @OSF_COPYRIGHT@
 */

#include <pexpert/pexpert.h>
#include <arm/cpuid.h>
#include <arm/cpuid_internal.h>
#include <arm/cpu_data_internal.h>
#include <arm64/proc_reg.h>
#include <kern/lock_rw.h>
#include <vm/vm_page.h>

#include <libkern/section_keywords.h>

/* Temporary types to aid decoding,
 * Everything in Little Endian */

typedef struct {
	uint32_t
	    Ctype1:3, /* 2:0 */
	    Ctype2:3, /* 5:3 */
	    Ctype3:3, /* 8:6 */
	    Ctypes:15, /* 6:23 - Don't Care */
	    LoC:3, /* 26-24 - Level of Coherency */
	    LoU:3, /* 29:27 - Level of Unification */
	    RAZ:2; /* 31:30 - Read-As-Zero */
} arm_cache_clidr_t;

typedef union {
	arm_cache_clidr_t bits;
	uint32_t          value;
} arm_cache_clidr_info_t;


typedef struct {
	uint32_t
	    LineSize:3, /* 2:0 - Number of words in cache line */
	    Assoc:10, /* 12:3 - Associativity of cache */
	    NumSets:15, /* 27:13 - Number of sets in cache */
	    c_type:4; /* 31:28 - Cache type */
} arm_cache_ccsidr_t;


typedef union {
	arm_cache_ccsidr_t bits;
	uint32_t           value;
} arm_cache_ccsidr_info_t;

/* Statics */

static SECURITY_READ_ONLY_LATE(arm_cpu_info_t) cpuid_cpu_info;
static SECURITY_READ_ONLY_LATE(cache_info_t *) cpuid_cache_info_boot_cpu;
static cache_info_t cpuid_cache_info[MAX_CPU_TYPES] = { 0 };
static _Atomic uint8_t cpuid_cache_info_bitmap = 0;

/* Code */

__private_extern__
void
do_cpuid(void)
{
	cpuid_cpu_info.value = machine_read_midr();
#if (__ARM_ARCH__ == 8)

#if defined(HAS_APPLE_PAC)
	cpuid_cpu_info.arm_info.arm_arch = CPU_ARCH_ARMv8E;
#else /* defined(HAS_APPLE_PAC) */
	cpuid_cpu_info.arm_info.arm_arch = CPU_ARCH_ARMv8;
#endif /* defined(HAS_APPLE_PAC) */

#else /* (__ARM_ARCH__ != 8) */
#error Unsupported arch
#endif /* (__ARM_ARCH__ != 8) */
}

arm_cpu_info_t *
cpuid_info(void)
{
	return &cpuid_cpu_info;
}

int
cpuid_get_cpufamily(void)
{
	int cpufamily = 0;

	switch (cpuid_info()->arm_info.arm_implementor) {
	case CPU_VID_ARM:
		switch (cpuid_info()->arm_info.arm_part) {
		case CPU_PART_CORTEXA9:
			cpufamily = CPUFAMILY_ARM_14;
			break;
		case CPU_PART_CORTEXA8:
			cpufamily = CPUFAMILY_ARM_13;
			break;
		case CPU_PART_CORTEXA7:
			cpufamily = CPUFAMILY_ARM_15;
			break;
		case CPU_PART_1136JFS:
		case CPU_PART_1176JZFS:
			cpufamily = CPUFAMILY_ARM_11;
			break;
		case CPU_PART_926EJS:
		case CPU_PART_920T:
			cpufamily = CPUFAMILY_ARM_9;
			break;
		default:
			cpufamily = CPUFAMILY_UNKNOWN;
			break;
		}
		break;

	case CPU_VID_INTEL:
		cpufamily = CPUFAMILY_ARM_XSCALE;
		break;

	case CPU_VID_APPLE:
		switch (cpuid_info()->arm_info.arm_part) {
		case CPU_PART_TYPHOON:
		case CPU_PART_TYPHOON_CAPRI:
			cpufamily = CPUFAMILY_ARM_TYPHOON;
			break;
		case CPU_PART_TWISTER:
		case CPU_PART_TWISTER_ELBA_MALTA:
			cpufamily = CPUFAMILY_ARM_TWISTER;
			break;
		case CPU_PART_HURRICANE:
		case CPU_PART_HURRICANE_MYST:
			cpufamily = CPUFAMILY_ARM_HURRICANE;
			break;
		case CPU_PART_MONSOON:
		case CPU_PART_MISTRAL:
			cpufamily = CPUFAMILY_ARM_MONSOON_MISTRAL;
			break;
		case CPU_PART_VORTEX:
		case CPU_PART_TEMPEST:
		case CPU_PART_TEMPEST_M9:
		case CPU_PART_VORTEX_ARUBA:
		case CPU_PART_TEMPEST_ARUBA:
			cpufamily = CPUFAMILY_ARM_VORTEX_TEMPEST;
			break;
		case CPU_PART_LIGHTNING:
		case CPU_PART_THUNDER:
		case CPU_PART_THUNDER_M10:
			cpufamily = CPUFAMILY_ARM_LIGHTNING_THUNDER;
			break;
		case CPU_PART_FIRESTORM_JADE_CHOP:
		case CPU_PART_FIRESTORM_JADE_DIE:
		case CPU_PART_ICESTORM_JADE_CHOP:
		case CPU_PART_ICESTORM_JADE_DIE:
		case CPU_PART_FIRESTORM:
		case CPU_PART_ICESTORM:
		case CPU_PART_FIRESTORM_TONGA:
		case CPU_PART_ICESTORM_TONGA:
			cpufamily = CPUFAMILY_ARM_FIRESTORM_ICESTORM;
			break;
		case CPU_PART_BLIZZARD_STATEN:
		case CPU_PART_AVALANCHE_STATEN:
		case CPU_PART_BLIZZARD_RHODES_CHOP:
		case CPU_PART_AVALANCHE_RHODES_CHOP:
		case CPU_PART_BLIZZARD_RHODES_DIE:
		case CPU_PART_AVALANCHE_RHODES_DIE:
		case CPU_PART_BLIZZARD:
		case CPU_PART_AVALANCHE:
			cpufamily = CPUFAMILY_ARM_BLIZZARD_AVALANCHE;
			break;
		case CPU_PART_EVEREST:
		case CPU_PART_SAWTOOTH:
		case CPU_PART_SAWTOOTH_M11:
			cpufamily = CPUFAMILY_ARM_EVEREST_SAWTOOTH;
			break;
		case CPU_PART_ECORE_IBIZA:
		case CPU_PART_PCORE_IBIZA:
			cpufamily = CPUFAMILY_ARM_IBIZA;
			break;
		case CPU_PART_ECORE_PALMA:
		case CPU_PART_PCORE_PALMA:
			cpufamily = CPUFAMILY_ARM_PALMA;
			break;
		case CPU_PART_ECORE_COLL:
		case CPU_PART_PCORE_COLL:
			cpufamily = CPUFAMILY_ARM_COLL;
			break;
		case CPU_PART_ECORE_LOBOS:
		case CPU_PART_PCORE_LOBOS:
			cpufamily = CPUFAMILY_ARM_LOBOS;
			break;
		case CPU_PART_ECORE_DONAN:
		case CPU_PART_PCORE_DONAN:
			cpufamily = CPUFAMILY_ARM_DONAN;
			break;
		default:
			cpufamily = CPUFAMILY_UNKNOWN;
			break;
		}
		break;

	default:
		cpufamily = CPUFAMILY_UNKNOWN;
		break;
	}

	return cpufamily;
}

int
cpuid_get_cpusubfamily(void)
{
	int cpusubfamily = CPUSUBFAMILY_UNKNOWN;

	if (cpuid_info()->arm_info.arm_implementor != CPU_VID_APPLE) {
		return cpusubfamily;
	}

	switch (cpuid_info()->arm_info.arm_part) {
	case CPU_PART_TYPHOON:
	case CPU_PART_TWISTER:
	case CPU_PART_HURRICANE:
	case CPU_PART_MONSOON:
	case CPU_PART_MISTRAL:
	case CPU_PART_VORTEX:
	case CPU_PART_TEMPEST:
	case CPU_PART_LIGHTNING:
	case CPU_PART_THUNDER:
	case CPU_PART_FIRESTORM:
	case CPU_PART_ICESTORM:
	case CPU_PART_BLIZZARD:
	case CPU_PART_AVALANCHE:
	case CPU_PART_SAWTOOTH:
	case CPU_PART_EVEREST:
		cpusubfamily = CPUSUBFAMILY_ARM_HP;
		break;
	case CPU_PART_TYPHOON_CAPRI:
	case CPU_PART_TWISTER_ELBA_MALTA:
	case CPU_PART_HURRICANE_MYST:
	case CPU_PART_VORTEX_ARUBA:
	case CPU_PART_TEMPEST_ARUBA:
	case CPU_PART_FIRESTORM_TONGA:
	case CPU_PART_ICESTORM_TONGA:
	case CPU_PART_BLIZZARD_STATEN:
	case CPU_PART_AVALANCHE_STATEN:
		cpusubfamily = CPUSUBFAMILY_ARM_HG;
		break;
	case CPU_PART_TEMPEST_M9:
	case CPU_PART_THUNDER_M10:
	case CPU_PART_SAWTOOTH_M11:
		cpusubfamily = CPUSUBFAMILY_ARM_M;
		break;
	case CPU_PART_ECORE_IBIZA:
	case CPU_PART_PCORE_IBIZA:
		cpusubfamily = CPUSUBFAMILY_ARM_HG;
		break;
	case CPU_PART_ECORE_COLL:
	case CPU_PART_PCORE_COLL:
		cpusubfamily = CPUSUBFAMILY_ARM_HP;
		break;
	case CPU_PART_ECORE_PALMA:
	case CPU_PART_PCORE_PALMA:
		cpusubfamily = CPUSUBFAMILY_ARM_HC_HD;
		break;
	case CPU_PART_ECORE_LOBOS:
	case CPU_PART_PCORE_LOBOS:
		cpusubfamily = CPUSUBFAMILY_ARM_HS;
		break;
	case CPU_PART_FIRESTORM_JADE_CHOP:
	case CPU_PART_ICESTORM_JADE_CHOP:
		cpusubfamily = CPUSUBFAMILY_ARM_HS;
		break;
	case CPU_PART_FIRESTORM_JADE_DIE:
	case CPU_PART_ICESTORM_JADE_DIE:
		cpusubfamily = CPUSUBFAMILY_ARM_HC_HD;
		break;
	case CPU_PART_BLIZZARD_RHODES_CHOP:
	case CPU_PART_AVALANCHE_RHODES_CHOP:
		cpusubfamily = CPUSUBFAMILY_ARM_HS;
		break;
	case CPU_PART_BLIZZARD_RHODES_DIE:
	case CPU_PART_AVALANCHE_RHODES_DIE:
		cpusubfamily = CPUSUBFAMILY_ARM_HC_HD;
		break;
	case CPU_PART_ECORE_DONAN:
	case CPU_PART_PCORE_DONAN:
		cpusubfamily = CPUSUBFAMILY_ARM_HG;
		break;
	default:
		cpusubfamily = CPUSUBFAMILY_UNKNOWN;
		break;
	}

	return cpusubfamily;
}

void
do_debugid(void)
{
	machine_do_debugid();
}

arm_debug_info_t *
arm_debug_info(void)
{
	return machine_arm_debug_info();
}

void
do_mvfpid(void)
{
	return machine_do_mvfpid();
}

arm_mvfp_info_t
*
arm_mvfp_info(void)
{
	return machine_arm_mvfp_info();
}

void
do_cacheid(void)
{
	arm_cache_clidr_info_t arm_cache_clidr_info;
	arm_cache_ccsidr_info_t arm_cache_ccsidr_info;

	/*
	 * We only need to parse cache geometry parameters once per cluster type.
	 * Skip this if some other core of the same type has already parsed them.
	 */
	cluster_type_t cluster_type = ml_get_topology_info()->cpus[ml_get_cpu_number_local()].cluster_type;
	uint8_t prev_cpuid_cache_info_bitmap = os_atomic_or_orig(&cpuid_cache_info_bitmap,
	    (uint8_t)(1 << cluster_type), acq_rel);
	if (prev_cpuid_cache_info_bitmap & (1 << cluster_type)) {
		return;
	}

	cache_info_t *cpuid_cache_info_p = &cpuid_cache_info[cluster_type];

	arm_cache_clidr_info.value = machine_read_clidr();

	/*
	 * For compatibility purposes with existing callers, let's cache the boot CPU
	 * cache parameters and return those upon any call to cache_info();
	 */
	if (prev_cpuid_cache_info_bitmap == 0) {
		cpuid_cache_info_boot_cpu = cpuid_cache_info_p;
	}

	/* Select L1 data/unified cache */

	machine_write_csselr(CSSELR_L1, CSSELR_DATA_UNIFIED);
	arm_cache_ccsidr_info.value = machine_read_ccsidr();

	cpuid_cache_info_p->c_unified = (arm_cache_clidr_info.bits.Ctype1 == 0x4) ? 1 : 0;

	switch (arm_cache_ccsidr_info.bits.c_type) {
	case 0x1:
		cpuid_cache_info_p->c_type = CACHE_WRITE_ALLOCATION;
		break;
	case 0x2:
		cpuid_cache_info_p->c_type = CACHE_READ_ALLOCATION;
		break;
	case 0x4:
		cpuid_cache_info_p->c_type = CACHE_WRITE_BACK;
		break;
	case 0x8:
		cpuid_cache_info_p->c_type = CACHE_WRITE_THROUGH;
		break;
	default:
		cpuid_cache_info_p->c_type = CACHE_UNKNOWN;
	}

	cpuid_cache_info_p->c_linesz = 4 * (1 << (arm_cache_ccsidr_info.bits.LineSize + 2));
	cpuid_cache_info_p->c_assoc = (arm_cache_ccsidr_info.bits.Assoc + 1);

	/* I cache size */
	cpuid_cache_info_p->c_isize = (arm_cache_ccsidr_info.bits.NumSets + 1) * cpuid_cache_info_p->c_linesz * cpuid_cache_info_p->c_assoc;

	/* D cache size */
	cpuid_cache_info_p->c_dsize = (arm_cache_ccsidr_info.bits.NumSets + 1) * cpuid_cache_info_p->c_linesz * cpuid_cache_info_p->c_assoc;


	if ((arm_cache_clidr_info.bits.Ctype3 == 0x4) ||
	    (arm_cache_clidr_info.bits.Ctype2 == 0x4) || (arm_cache_clidr_info.bits.Ctype2 == 0x2)) {
		if (arm_cache_clidr_info.bits.Ctype3 == 0x4) {
			/* Select L3 (LLC) if the SoC is new enough to have that.
			 * This will be the second-level cache for the highest-performing ACC. */
			machine_write_csselr(CSSELR_L3, CSSELR_DATA_UNIFIED);
		} else {
			/* Select L2 data cache */
			machine_write_csselr(CSSELR_L2, CSSELR_DATA_UNIFIED);
		}
		arm_cache_ccsidr_info.value = machine_read_ccsidr();

		cpuid_cache_info_p->c_linesz = 4 * (1 << (arm_cache_ccsidr_info.bits.LineSize + 2));
		cpuid_cache_info_p->c_assoc = (arm_cache_ccsidr_info.bits.Assoc + 1);
		cpuid_cache_info_p->c_l2size = (arm_cache_ccsidr_info.bits.NumSets + 1) * cpuid_cache_info_p->c_linesz * cpuid_cache_info_p->c_assoc;
		cpuid_cache_info_p->c_inner_cache_size = cpuid_cache_info_p->c_dsize;
		cpuid_cache_info_p->c_bulksize_op = cpuid_cache_info_p->c_l2size;

		/* capri has a 2MB L2 cache unlike every other SoC up to this
		 * point with a 1MB L2 cache, so to get the same performance
		 * gain from coloring, we have to double the number of colors.
		 * Note that in general (and in fact as it's implemented in
		 * i386/cpuid.c), the number of colors is calculated as the
		 * cache line size * the number of sets divided by the page
		 * size. Also note that for H8 devices and up, the page size
		 * will be 16k instead of 4, which will reduce the number of
		 * colors required. Thus, this is really a temporary solution
		 * for capri specifically that we may want to generalize later:
		 *
		 * TODO: Are there any special considerations for our unusual
		 * cache geometries (3MB)?
		 */
		vm_cache_geometry_colors = ((arm_cache_ccsidr_info.bits.NumSets + 1) * cpuid_cache_info_p->c_linesz) / PAGE_SIZE;
		kprintf(" vm_cache_geometry_colors: %d\n", vm_cache_geometry_colors);
	} else {
		cpuid_cache_info_p->c_l2size = 0;

		cpuid_cache_info_p->c_inner_cache_size = cpuid_cache_info_p->c_dsize;
		cpuid_cache_info_p->c_bulksize_op = cpuid_cache_info_p->c_dsize;
	}

	if (cpuid_cache_info_p->c_unified == 0) {
		machine_write_csselr(CSSELR_L1, CSSELR_INSTR);
		arm_cache_ccsidr_info.value = machine_read_ccsidr();
		uint32_t c_linesz = 4 * (1 << (arm_cache_ccsidr_info.bits.LineSize + 2));
		uint32_t c_assoc = (arm_cache_ccsidr_info.bits.Assoc + 1);
		/* I cache size */
		cpuid_cache_info_p->c_isize = (arm_cache_ccsidr_info.bits.NumSets + 1) * c_linesz * c_assoc;
	}

	if (cpuid_cache_info_p == cpuid_cache_info_boot_cpu) {
		cpuid_cache_info_p->c_valid = true;
	} else {
		os_atomic_store(&cpuid_cache_info_p->c_valid, true, release);
		thread_wakeup((event_t)&cpuid_cache_info_p->c_valid);
	}

	kprintf("%s() - %u bytes %s cache (I:%u D:%u (%s)), %u-way assoc, %u bytes/line\n",
	    __FUNCTION__,
	    cpuid_cache_info_p->c_dsize + cpuid_cache_info_p->c_isize,
	    ((cpuid_cache_info_p->c_type == CACHE_WRITE_BACK) ? "WB" :
	    (cpuid_cache_info_p->c_type == CACHE_WRITE_THROUGH ? "WT" : "Unknown")),
	    cpuid_cache_info_p->c_isize,
	    cpuid_cache_info_p->c_dsize,
	    (cpuid_cache_info_p->c_unified) ? "unified" : "separate",
	    cpuid_cache_info_p->c_assoc,
	    cpuid_cache_info_p->c_linesz);
}

cache_info_t   *
cache_info(void)
{
	return cpuid_cache_info_boot_cpu;
}

cache_info_t   *
cache_info_type(cluster_type_t cluster_type)
{
	assert((cluster_type >= 0) && (cluster_type < MAX_CPU_TYPES));
	cache_info_t *ret = &cpuid_cache_info[cluster_type];

	/*
	 * cpuid_cache_info_boot_cpu is always populated by the time
	 * cache_info_type() is callable.  Other clusters may not have completed
	 * do_cacheid() yet.
	 */
	if (ret == cpuid_cache_info_boot_cpu) {
		return ret;
	}

	while (!os_atomic_load(&ret->c_valid, acquire)) {
		assert_wait((event_t)&ret->c_valid, THREAD_UNINT);
		if (os_atomic_load(&ret->c_valid, relaxed)) {
			clear_wait(current_thread(), THREAD_AWAKENED);
		} else {
			thread_block(THREAD_CONTINUE_NULL);
		}
	}

	return ret;
}
