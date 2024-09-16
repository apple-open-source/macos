/*
 * Copyright (c) 2017-2020 Apple Inc. All rights reserved.
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

#include <arm/cpu_data_internal.h>
#include <arm/machine_routines.h>
#include <arm64/monotonic.h>
#include <kern/assert.h>
#include <kern/cpc.h>
#include <kern/debug.h> /* panic */
#include <kern/kpc.h>
#include <kern/monotonic.h>
#include <machine/atomic.h>
#include <machine/limits.h> /* CHAR_BIT */
#include <os/overflow.h>
#include <pexpert/arm64/board_config.h>
#include <pexpert/device_tree.h> /* SecureDTFindEntry */
#include <pexpert/pexpert.h>
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/monotonic.h>

/*
 * Ensure that control registers read back what was written under MACH_ASSERT
 * kernels.
 *
 * A static inline function cannot be used due to passing the register through
 * the builtin -- it requires a constant string as its first argument, since
 * MSRs registers are encoded as an immediate in the instruction.
 */
#if MACH_ASSERT
#define CTRL_REG_SET(reg, val) do { \
	__builtin_arm_wsr64((reg), (val)); \
	uint64_t __check_reg = __builtin_arm_rsr64((reg)); \
	if (__check_reg != (val)) { \
	        panic("value written to %s was not read back (wrote %llx, read %llx)", \
	            #reg, (val), __check_reg); \
	} \
} while (0)
#else /* MACH_ASSERT */
#define CTRL_REG_SET(reg, val) __builtin_arm_wsr64((reg), (val))
#endif /* MACH_ASSERT */

#pragma mark core counters

const bool mt_core_supported = true;

static const ml_topology_info_t *topology_info;

/*
 * PMC[0-1] are the 48/64-bit fixed counters -- PMC0 is cycles and PMC1 is
 * instructions (see arm64/monotonic.h).
 *
 * PMC2+ are currently handled by kpc.
 */
#define PMC_0_7(X, A) X(0, A); X(1, A); X(2, A); X(3, A); X(4, A); X(5, A); \
    X(6, A); X(7, A)

#if CORE_NCTRS > 8
#define PMC_8_9(X, A) X(8, A); X(9, A)
#else // CORE_NCTRS > 8
#define PMC_8_9(X, A)
#endif // CORE_NCTRS > 8

#define PMC_ALL(X, A) PMC_0_7(X, A); PMC_8_9(X, A)

#if CPMU_64BIT_PMCS
#define PMC_WIDTH (63)
#else // UPMU_64BIT_PMCS
#define PMC_WIDTH (47)
#endif // !UPMU_64BIT_PMCS

#define CTR_MAX ((UINT64_C(1) << PMC_WIDTH) - 1)

#define CYCLES 0
#define INSTRS 1

/*
 * PMC0's offset into a core's PIO range.
 *
 * This allows cores to remotely query another core's counters.
 */

#define PIO_PMC0_OFFSET (0x200)

/*
 * The offset of the counter in the configuration registers.  Post-Hurricane
 * devices have additional counters that need a larger shift than the original
 * counters.
 *
 * XXX For now, just support the lower-numbered counters.
 */
#define CTR_POS(CTR) (CTR)

/*
 * PMCR0 is the main control register for the performance monitor.  It
 * controls whether the counters are enabled, how they deliver interrupts, and
 * other features.
 */

#define PMCR0_CTR_EN(CTR) (UINT64_C(1) << CTR_POS(CTR))
#define PMCR0_FIXED_EN (PMCR0_CTR_EN(CYCLES) | PMCR0_CTR_EN(INSTRS))
/* how interrupts are delivered on a PMI */
enum {
	PMCR0_INTGEN_OFF = 0,
	PMCR0_INTGEN_PMI = 1,
	PMCR0_INTGEN_AIC = 2,
	PMCR0_INTGEN_HALT = 3,
	PMCR0_INTGEN_FIQ = 4,
};
#define PMCR0_INTGEN_SET(X) ((uint64_t)(X) << 8)

#if CPMU_AIC_PMI
#define PMCR0_INTGEN_INIT PMCR0_INTGEN_SET(PMCR0_INTGEN_AIC)
#else /* CPMU_AIC_PMI */
#define PMCR0_INTGEN_INIT PMCR0_INTGEN_SET(PMCR0_INTGEN_FIQ)
#endif /* !CPMU_AIC_PMI */

#define PMCR0_PMI_SHIFT (12)
#define PMCR0_CTR_GE8_PMI_SHIFT (44)
#define PMCR0_PMI_EN(CTR) (UINT64_C(1) << (PMCR0_PMI_SHIFT + CTR_POS(CTR)))
/* fixed counters are always counting */
#define PMCR0_PMI_INIT (PMCR0_PMI_EN(CYCLES) | PMCR0_PMI_EN(INSTRS))
/* disable counting on a PMI */
#define PMCR0_DISCNT_EN (UINT64_C(1) << 20)
/* block PMIs until ERET retires */
#define PMCR0_WFRFE_EN (UINT64_C(1) << 22)
/* count global (not just core-local) L2C events */
#define PMCR0_L2CGLOBAL_EN (UINT64_C(1) << 23)
/* user mode access to configuration registers */
#define PMCR0_USEREN_EN (UINT64_C(1) << 30)
#define PMCR0_CTR_GE8_EN_SHIFT (32)

#if HAS_CPMU_PC_CAPTURE
#define PMCR0_PCC_INIT (UINT64_C(0x7) << 24)
#else /* HAS_CPMU_PC_CAPTURE */
#define PMCR0_PCC_INIT (0)
#endif /* !HAS_CPMU_PC_CAPTURE */

#define PMCR0_INIT (PMCR0_INTGEN_INIT | PMCR0_PMI_INIT | PMCR0_PCC_INIT)

/*
 * PMCR1 controls which execution modes count events.
 */
#define PMCR1_EL0A32_EN(CTR) (UINT64_C(1) << (0 + CTR_POS(CTR)))
#define PMCR1_EL0A64_EN(CTR) (UINT64_C(1) << (8 + CTR_POS(CTR)))
#define PMCR1_EL1A64_EN(CTR) (UINT64_C(1) << (16 + CTR_POS(CTR)))
/* PMCR1_EL3A64 is not supported on systems with no monitor */
#if defined(APPLEHURRICANE)
#define PMCR1_EL3A64_EN(CTR) UINT64_C(0)
#else
#define PMCR1_EL3A64_EN(CTR) (UINT64_C(1) << (24 + CTR_POS(CTR)))
#endif
#define PMCR1_ALL_EN(CTR) (PMCR1_EL0A32_EN(CTR) | PMCR1_EL0A64_EN(CTR) | \
	                   PMCR1_EL1A64_EN(CTR) | PMCR1_EL3A64_EN(CTR))

/* fixed counters always count in all modes */
#define PMCR1_INIT (PMCR1_ALL_EN(CYCLES) | PMCR1_ALL_EN(INSTRS))

static inline void
core_init_execution_modes(void)
{
	uint64_t pmcr1;

	pmcr1 = __builtin_arm_rsr64("PMCR1_EL1");
	pmcr1 |= PMCR1_INIT;
	__builtin_arm_wsr64("PMCR1_EL1", pmcr1);
#if CONFIG_EXCLAVES
	__builtin_arm_wsr64("PMCR1_EL12", pmcr1);
#endif
}

#define PMSR_OVF(CTR) (1ULL << (CTR))

static int
core_init(__unused mt_device_t dev)
{
	/* the dev node interface to the core counters is still unsupported */
	return ENOTSUP;
}

struct mt_cpu *
mt_cur_cpu(void)
{
	return &getCpuDatap()->cpu_monotonic;
}

uint64_t
mt_core_snap(unsigned int ctr)
{
	switch (ctr) {
#define PMC_RD(CTR, UNUSED) case (CTR): return __builtin_arm_rsr64(__MSR_STR(PMC ## CTR))
		PMC_ALL(PMC_RD, 0);
#undef PMC_RD
	default:
		panic("monotonic: invalid core counter read: %u", ctr);
		__builtin_unreachable();
	}
}

void
mt_core_set_snap(unsigned int ctr, uint64_t count)
{
	switch (ctr) {
	case 0:
		__builtin_arm_wsr64("PMC0", count);
		break;
	case 1:
		__builtin_arm_wsr64("PMC1", count);
		break;
	default:
		panic("monotonic: invalid core counter %u write %llu", ctr, count);
		__builtin_unreachable();
	}
}

static void
core_set_enabled(void)
{
	uint32_t kpc_mask = kpc_get_running() &
	    (KPC_CLASS_CONFIGURABLE_MASK | KPC_CLASS_POWER_MASK);
	uint64_t pmcr0 = __builtin_arm_rsr64("PMCR0_EL1");
	pmcr0 |= PMCR0_INIT | PMCR0_FIXED_EN;

	if (kpc_mask != 0) {
		uint64_t kpc_ctrs = kpc_get_configurable_pmc_mask(kpc_mask) <<
		        MT_CORE_NFIXED;
#if KPC_ARM64_CONFIGURABLE_COUNT > 6
		uint64_t ctrs_ge8 = kpc_ctrs >> 8;
		pmcr0 |= ctrs_ge8 << PMCR0_CTR_GE8_EN_SHIFT;
		pmcr0 |= ctrs_ge8 << PMCR0_CTR_GE8_PMI_SHIFT;
		kpc_ctrs &= (1ULL << 8) - 1;
#endif /* KPC_ARM64_CONFIGURABLE_COUNT > 6 */
		kpc_ctrs |= kpc_ctrs << PMCR0_PMI_SHIFT;
		pmcr0 |= kpc_ctrs;
	}

	__builtin_arm_wsr64("PMCR0_EL1", pmcr0);
#if MACH_ASSERT
	/*
	 * Only check for the values that were ORed in.
	 */
	uint64_t pmcr0_check = __builtin_arm_rsr64("PMCR0_EL1");
	if ((pmcr0_check & (PMCR0_INIT | PMCR0_FIXED_EN)) != (PMCR0_INIT | PMCR0_FIXED_EN)) {
		panic("monotonic: hardware ignored enable (read %llx, wrote %llx)",
		    pmcr0_check, pmcr0);
	}
#endif /* MACH_ASSERT */
}

static void
core_idle(__unused cpu_data_t *cpu)
{
	assert(cpu != NULL);
	assert(ml_get_interrupts_enabled() == FALSE);

#if DEBUG
	uint64_t pmcr0 = __builtin_arm_rsr64("PMCR0_EL1");
	if ((pmcr0 & PMCR0_FIXED_EN) == 0) {
		panic("monotonic: counters disabled before idling, pmcr0 = 0x%llx", pmcr0);
	}
	uint64_t pmcr1 = __builtin_arm_rsr64("PMCR1_EL1");
	if ((pmcr1 & PMCR1_INIT) == 0) {
		panic("monotonic: counter modes disabled before idling, pmcr1 = 0x%llx", pmcr1);
	}
#endif /* DEBUG */

	/* disable counters before updating */
	__builtin_arm_wsr64("PMCR0_EL1", PMCR0_INIT);

	mt_update_fixed_counts();
}

#pragma mark uncore performance monitor

#if HAS_UNCORE_CTRS

static bool mt_uncore_initted = false;

static bool mt_uncore_suspended_cpd = false;

/*
 * Uncore Performance Monitor
 *
 * Uncore performance monitors provide event-counting for the last-level caches
 * (LLCs).  Each LLC has its own uncore performance monitor, which can only be
 * accessed by cores that use that LLC.  Like the core performance monitoring
 * unit, uncore counters are configured globally.  If there is more than one
 * LLC on the system, PIO reads must be used to satisfy uncore requests (using
 * the `_r` remote variants of the access functions).  Otherwise, local MSRs
 * suffice (using the `_l` local variants of the access functions).
 */

#if UNCORE_PER_CLUSTER
#define MAX_NMONITORS MAX_CPU_CLUSTERS
static uintptr_t cpm_impl[MAX_NMONITORS] = {};
#else
#define MAX_NMONITORS (1)
#endif /* UNCORE_PER_CLUSTER */

#if UNCORE_VERSION >= 2
/*
 * V2 uncore monitors feature a CTI mechanism -- the second bit of UPMSR is
 * used to track if a CTI has been triggered due to an overflow.
 */
#define UPMSR_OVF_POS 2
#else /* UNCORE_VERSION >= 2 */
#define UPMSR_OVF_POS 1
#endif /* UNCORE_VERSION < 2 */
#define UPMSR_OVF(R, CTR) ((R) >> ((CTR) + UPMSR_OVF_POS) & 0x1)
#define UPMSR_OVF_MASK    (((UINT64_C(1) << UNCORE_NCTRS) - 1) << UPMSR_OVF_POS)

#define UPMPCM_CORE(ID) (UINT64_C(1) << (ID))

#if UPMU_64BIT_PMCS
#define UPMC_WIDTH (63)
#else // UPMU_64BIT_PMCS
#define UPMC_WIDTH (47)
#endif // !UPMU_64BIT_PMCS

/*
 * The uncore_pmi_mask is a bitmask of CPUs that receive uncore PMIs.  It's
 * initialized by uncore_init and controllable by the uncore_pmi_mask boot-arg.
 */
static int32_t uncore_pmi_mask = 0;

/*
 * The uncore_active_ctrs is a bitmask of uncore counters that are currently
 * requested.
 */
static uint16_t uncore_active_ctrs = 0;
static_assert(sizeof(uncore_active_ctrs) * CHAR_BIT >= UNCORE_NCTRS,
    "counter mask should fit the full range of counters");

/*
 * mt_uncore_enabled is true when any uncore counters are active.
 */
bool mt_uncore_enabled = false;

/*
 * The uncore_events are the event configurations for each uncore counter -- as
 * a union to make it easy to program the hardware registers.
 */
static struct uncore_config {
	union {
		uint8_t uce_ctrs[UNCORE_NCTRS];
		uint64_t uce_regs[UNCORE_NCTRS / 8];
	} uc_events;
	union {
		uint16_t uccm_masks[UNCORE_NCTRS];
		uint64_t uccm_regs[UNCORE_NCTRS / 4];
	} uc_cpu_masks[MAX_NMONITORS];
} uncore_config;

static struct uncore_monitor {
	/*
	 * The last snapshot of each of the hardware counter values.
	 */
	uint64_t um_snaps[UNCORE_NCTRS];

	/*
	 * The accumulated counts for each counter.
	 */
	uint64_t um_counts[UNCORE_NCTRS];

	/*
	 * Protects accessing the hardware registers and fields in this structure.
	 */
	lck_spin_t um_lock;

	/*
	 * Whether this monitor needs its registers restored after wake.
	 */
	bool um_sleeping;

#if MACH_ASSERT
	/*
	 * Save the last ID that read from this monitor.
	 */
	uint8_t um_last_read_id;

	/*
	 * Save whether this monitor has been read since sleeping.
	 */
	bool um_read_since_sleep;
#endif /* MACH_ASSERT */
} uncore_monitors[MAX_NMONITORS];

/*
 * Each uncore unit has its own monitor, corresponding to the memory hierarchy
 * of the LLCs.
 */
static unsigned int
uncore_nmonitors(void)
{
#if UNCORE_PER_CLUSTER
	return topology_info->num_clusters;
#else /* UNCORE_PER_CLUSTER */
	return 1;
#endif /* !UNCORE_PER_CLUSTER */
}

static unsigned int
uncmon_get_curid(void)
{
#if UNCORE_PER_CLUSTER
	return cpu_cluster_id();
#else /* UNCORE_PER_CLUSTER */
	return 0;
#endif /* !UNCORE_PER_CLUSTER */
}

/*
 * Per-monitor locks are required to prevent races with the PMI handlers, not
 * from other CPUs that are configuring (those are serialized with monotonic's
 * per-device lock).
 */

static int
uncmon_lock(struct uncore_monitor *mon)
{
	int intrs_en = ml_set_interrupts_enabled(FALSE);
	lck_spin_lock(&mon->um_lock);
	return intrs_en;
}

static void
uncmon_unlock(struct uncore_monitor *mon, int intrs_en)
{
	lck_spin_unlock(&mon->um_lock);
	(void)ml_set_interrupts_enabled(intrs_en);
}

static bool
uncmon_is_remote(unsigned int monid)
{
	if (monid >= MAX_NMONITORS) {
		panic("monotonic: %s: invalid monid %u (> %u)", __FUNCTION__, monid, MAX_NMONITORS);
	}
	struct uncore_monitor *mon = &uncore_monitors[monid];
#pragma unused(mon)
	LCK_SPIN_ASSERT(&mon->um_lock, LCK_ASSERT_OWNED);
	return monid == uncmon_get_curid();
}

/*
 * Helper functions for accessing the hardware -- these require the monitor be
 * locked to prevent other CPUs' PMI handlers from making local modifications
 * or updating the counts.
 */

#if UNCORE_VERSION >= 2
#define UPMCR0_INTEN_POS 20
#define UPMCR0_INTGEN_POS 16
#else /* UNCORE_VERSION >= 2 */
#define UPMCR0_INTEN_POS 12
#define UPMCR0_INTGEN_POS 8
#endif /* UNCORE_VERSION < 2 */
enum {
	UPMCR0_INTGEN_OFF = 0,
	/* fast PMIs are only supported on core CPMU */
	UPMCR0_INTGEN_AIC = 2,
	UPMCR0_INTGEN_HALT = 3,
	UPMCR0_INTGEN_FIQ = 4,
};
/* always enable interrupts for all counters */
#define UPMCR0_INTEN (((1ULL << UNCORE_NCTRS) - 1) << UPMCR0_INTEN_POS)
/* route uncore PMIs through the FIQ path */
#define UPMCR0_INIT (UPMCR0_INTEN | (UPMCR0_INTGEN_FIQ << UPMCR0_INTGEN_POS))

/*
 * Turn counting on for counters set in the `enctrmask` and off, otherwise.
 */
static inline void
uncmon_set_counting_locked_l(__unused unsigned int monid, uint64_t enctrmask)
{
	/*
	 * UPMCR0 controls which counters are enabled and how interrupts are generated
	 * for overflows.
	 */
	__builtin_arm_wsr64("UPMCR0_EL1", UPMCR0_INIT | enctrmask);
}

#if UNCORE_PER_CLUSTER

/*
 * Turn counting on for counters set in the `enctrmask` and off, otherwise.
 */
static inline void
uncmon_set_counting_locked_r(unsigned int monid, uint64_t enctrmask)
{
	const uintptr_t upmcr0_offset = 0x4180;
	*(uint64_t *)(cpm_impl[monid] + upmcr0_offset) = UPMCR0_INIT | enctrmask;
}

#endif /* UNCORE_PER_CLUSTER */

/*
 * The uncore performance monitoring counters (UPMCs) are 48/64-bits wide.  The
 * high bit is an overflow bit, triggering a PMI, providing 47/63 usable bits.
 */

#define UPMC_MAX ((UINT64_C(1) << UPMC_WIDTH) - 1)

/*
 * The `__builtin_arm_{r,w}sr` functions require constant strings, since the
 * MSR/MRS instructions encode the registers as immediates.  Otherwise, this
 * would be indexing into an array of strings.
 */

#define UPMC_0_7(X, A) X(0, A); X(1, A); X(2, A); X(3, A); X(4, A); X(5, A); \
	        X(6, A); X(7, A)
#if UNCORE_NCTRS <= 8
#define UPMC_ALL(X, A) UPMC_0_7(X, A)
#else /* UNCORE_NCTRS <= 8 */
#define UPMC_8_15(X, A) X(8, A); X(9, A); X(10, A); X(11, A); X(12, A); \
	        X(13, A); X(14, A); X(15, A)
#define UPMC_ALL(X, A) UPMC_0_7(X, A); UPMC_8_15(X, A)
#endif /* UNCORE_NCTRS > 8 */

__unused
static inline uint64_t
uncmon_read_counter_locked_l(__unused unsigned int monid, unsigned int ctr)
{
	assert(ctr < UNCORE_NCTRS);
	switch (ctr) {
#define UPMC_RD(CTR, UNUSED) case (CTR): return __builtin_arm_rsr64(__MSR_STR(UPMC ## CTR))
		UPMC_ALL(UPMC_RD, 0);
#undef UPMC_RD
	default:
		panic("monotonic: invalid counter read %u", ctr);
		__builtin_unreachable();
	}
}

static inline void
uncmon_write_counter_locked_l(__unused unsigned int monid, unsigned int ctr,
    uint64_t count)
{
	assert(count < UPMC_MAX);
	assert(ctr < UNCORE_NCTRS);
	switch (ctr) {
#define UPMC_WR(CTR, COUNT) case (CTR): \
	        return __builtin_arm_wsr64(__MSR_STR(UPMC ## CTR), (COUNT))
		UPMC_ALL(UPMC_WR, count);
#undef UPMC_WR
	default:
		panic("monotonic: invalid counter write %u", ctr);
	}
}

#if UNCORE_PER_CLUSTER

uintptr_t upmc_offs[UNCORE_NCTRS] = {
	[0] = 0x4100, [1] = 0x4248, [2] = 0x4110, [3] = 0x4250, [4] = 0x4120,
	[5] = 0x4258, [6] = 0x4130, [7] = 0x4260, [8] = 0x4140, [9] = 0x4268,
	[10] = 0x4150, [11] = 0x4270, [12] = 0x4160, [13] = 0x4278,
	[14] = 0x4170, [15] = 0x4280,
};

static inline uint64_t
uncmon_read_counter_locked_r(unsigned int mon_id, unsigned int ctr)
{
	assert(mon_id < uncore_nmonitors());
	assert(ctr < UNCORE_NCTRS);
	return *(uint64_t *)(cpm_impl[mon_id] + upmc_offs[ctr]);
}

static inline void
uncmon_write_counter_locked_r(unsigned int mon_id, unsigned int ctr,
    uint64_t count)
{
	assert(count < UPMC_MAX);
	assert(ctr < UNCORE_NCTRS);
	assert(mon_id < uncore_nmonitors());
	*(uint64_t *)(cpm_impl[mon_id] + upmc_offs[ctr]) = count;
}

#endif /* UNCORE_PER_CLUSTER */

static inline void
uncmon_update_locked(unsigned int monid, unsigned int ctr)
{
	struct uncore_monitor *mon = &uncore_monitors[monid];
	if (!mon->um_sleeping) {
		uint64_t snap = 0;
#if UNCORE_PER_CLUSTER
		snap = uncmon_read_counter_locked_r(monid, ctr);
#else /* UNCORE_PER_CLUSTER */
		snap = uncmon_read_counter_locked_l(monid, ctr);
#endif /* UNCORE_PER_CLUSTER */
		if (snap < mon->um_snaps[ctr]) {
#if MACH_ASSERT
#if UNCORE_PER_CLUSTER
			uint64_t remote_value = uncmon_read_counter_locked_r(monid, ctr);
#endif /* UNCORE_PER_CLUSTER */
			panic("monotonic: UPMC%d on UPMU %d went backwards from "
			    "%llx to %llx, read via %s, last was %s from UPMU %hhd%s"
#if UNCORE_PER_CLUSTER
			    ", re-read remote value is %llx"
#endif /* UNCORE_PER_CLUSTER */
			    , ctr,
			    monid, mon->um_snaps[ctr], snap,
			    uncmon_get_curid() == monid ? "local" : "remote",
			    mon->um_last_read_id == monid ? "local" : "remote",
			    mon->um_last_read_id,
			    mon->um_read_since_sleep ? "" : ", first read since sleep"
#if UNCORE_PER_CLUSTER
			    , remote_value
#endif /* UNCORE_PER_CLUSTER */
			    );
#else /* MACH_ASSERT */
			snap = mon->um_snaps[ctr];
#endif /* !MACH_ASSERT */
		}
		mon->um_counts[ctr] += snap - mon->um_snaps[ctr];
		mon->um_snaps[ctr] = snap;
	}
}

static inline void
uncmon_program_events_locked_l(unsigned int monid)
{
	/*
	 * UPMESR[01] is the event selection register that determines which event a
	 * counter will count.
	 */
	CTRL_REG_SET("UPMESR0_EL1", uncore_config.uc_events.uce_regs[0]);

#if UNCORE_NCTRS > 8
	CTRL_REG_SET("UPMESR1_EL1", uncore_config.uc_events.uce_regs[1]);
#endif /* UNCORE_NCTRS > 8 */

	/*
	 * UPMECM[0123] are the event core masks for each counter -- whether or not
	 * that counter counts events generated by an agent.  These are set to all
	 * ones so the uncore counters count events from all cores.
	 *
	 * The bits are based off the start of the cluster -- e.g. even if a core
	 * has a CPU ID of 4, it might be the first CPU in a cluster.  Shift the
	 * registers right by the ID of the first CPU in the cluster.
	 */
	CTRL_REG_SET("UPMECM0_EL1",
	    uncore_config.uc_cpu_masks[monid].uccm_regs[0]);
	CTRL_REG_SET("UPMECM1_EL1",
	    uncore_config.uc_cpu_masks[monid].uccm_regs[1]);

#if UNCORE_NCTRS > 8
	CTRL_REG_SET("UPMECM2_EL1",
	    uncore_config.uc_cpu_masks[monid].uccm_regs[2]);
	CTRL_REG_SET("UPMECM3_EL1",
	    uncore_config.uc_cpu_masks[monid].uccm_regs[3]);
#endif /* UNCORE_NCTRS > 8 */
}

#if UNCORE_PER_CLUSTER

static inline void
uncmon_program_events_locked_r(unsigned int monid)
{
	const uintptr_t upmesr_offs[2] = {[0] = 0x41b0, [1] = 0x41b8, };

	for (unsigned int i = 0; i < sizeof(upmesr_offs) / sizeof(upmesr_offs[0]);
	    i++) {
		*(uint64_t *)(cpm_impl[monid] + upmesr_offs[i]) =
		    uncore_config.uc_events.uce_regs[i];
	}

	const uintptr_t upmecm_offs[4] = {
		[0] = 0x4190, [1] = 0x4198, [2] = 0x41a0, [3] = 0x41a8,
	};

	for (unsigned int i = 0; i < sizeof(upmecm_offs) / sizeof(upmecm_offs[0]);
	    i++) {
		*(uint64_t *)(cpm_impl[monid] + upmecm_offs[i]) =
		    uncore_config.uc_cpu_masks[monid].uccm_regs[i];
	}
}

#endif /* UNCORE_PER_CLUSTER */

static void
uncmon_clear_int_locked_l(__unused unsigned int monid)
{
	__builtin_arm_wsr64("UPMSR_EL1", 0);
}

#if UNCORE_PER_CLUSTER

static void
uncmon_clear_int_locked_r(unsigned int monid)
{
	const uintptr_t upmsr_off = 0x41c0;
	*(uint64_t *)(cpm_impl[monid] + upmsr_off) = 0;
}

#endif /* UNCORE_PER_CLUSTER */

/*
 * Get the PMI mask for the provided `monid` -- that is, the bitmap of CPUs
 * that should be sent PMIs for a particular monitor.
 */
static uint64_t
uncmon_get_pmi_mask(unsigned int monid)
{
	uint64_t pmi_mask = uncore_pmi_mask;

#if UNCORE_PER_CLUSTER
	pmi_mask &= topology_info->clusters[monid].cpu_mask;
#else /* UNCORE_PER_CLUSTER */
#pragma unused(monid)
#endif /* !UNCORE_PER_CLUSTER */

	return pmi_mask;
}

/*
 * Initialization routines for the uncore counters.
 */

static void
uncmon_init_locked_l(unsigned int monid)
{
	/*
	 * UPMPCM defines the PMI core mask for the UPMCs -- which cores should
	 * receive interrupts on overflow.
	 */
	CTRL_REG_SET("UPMPCM_EL1", uncmon_get_pmi_mask(monid));
	uncmon_set_counting_locked_l(monid,
	    mt_uncore_enabled ? uncore_active_ctrs : 0);
}

#if UNCORE_PER_CLUSTER

static uintptr_t acc_impl[MAX_NMONITORS] = {};

static void
uncmon_init_locked_r(unsigned int monid)
{
	const uintptr_t upmpcm_off = 0x1010;

	*(uint64_t *)(acc_impl[monid] + upmpcm_off) = uncmon_get_pmi_mask(monid);
	uncmon_set_counting_locked_r(monid,
	    mt_uncore_enabled ? uncore_active_ctrs : 0);
}

#endif /* UNCORE_PER_CLUSTER */

/*
 * Initialize the uncore device for monotonic.
 */
static int
uncore_init(__unused mt_device_t dev)
{
#if HAS_UNCORE_CTRS
	assert(MT_NDEVS > 0);
	mt_devices[MT_NDEVS - 1].mtd_nmonitors = (uint8_t)uncore_nmonitors();
#endif

#if DEVELOPMENT || DEBUG
	/*
	 * Development and debug kernels observe the `uncore_pmi_mask` boot-arg,
	 * allowing PMIs to be routed to the CPUs present in the supplied bitmap.
	 * Do some sanity checks on the value provided.
	 */
	bool parsed_arg = PE_parse_boot_argn("uncore_pmi_mask", &uncore_pmi_mask,
	    sizeof(uncore_pmi_mask));
	if (parsed_arg) {
#if UNCORE_PER_CLUSTER
		if (__builtin_popcount(uncore_pmi_mask) != (int)uncore_nmonitors()) {
			panic("monotonic: invalid uncore PMI mask 0x%x", uncore_pmi_mask);
		}
		for (unsigned int i = 0; i < uncore_nmonitors(); i++) {
			if (__builtin_popcountll(uncmon_get_pmi_mask(i)) != 1) {
				panic("monotonic: invalid uncore PMI CPU for cluster %d in mask 0x%x",
				    i, uncore_pmi_mask);
			}
		}
#else /* UNCORE_PER_CLUSTER */
		if (__builtin_popcount(uncore_pmi_mask) != 1) {
			panic("monotonic: invalid uncore PMI mask 0x%x", uncore_pmi_mask);
		}
#endif /* !UNCORE_PER_CLUSTER */
	} else
#endif /* DEVELOPMENT || DEBUG */
	{
		/* arbitrarily route to core 0 in each cluster */
		uncore_pmi_mask |= 1;
	}
	assert(uncore_pmi_mask != 0);

	for (unsigned int monid = 0; monid < uncore_nmonitors(); monid++) {
#if UNCORE_PER_CLUSTER
		ml_topology_cluster_t *cluster = &topology_info->clusters[monid];
		cpm_impl[monid] = (uintptr_t)cluster->cpm_IMPL_regs;
		acc_impl[monid] = (uintptr_t)cluster->acc_IMPL_regs;
		assert(cpm_impl[monid] != 0 && acc_impl[monid] != 0);
#endif /* UNCORE_PER_CLUSTER */

		struct uncore_monitor *mon = &uncore_monitors[monid];
		lck_spin_init(&mon->um_lock, &mt_lock_grp, LCK_ATTR_NULL);
	}

	mt_uncore_initted = true;

	return 0;
}

/*
 * Support for monotonic's mtd_read function.
 */

static void
uncmon_read_all_counters(unsigned int monid, uint64_t ctr_mask, uint64_t *counts)
{
	struct uncore_monitor *mon = &uncore_monitors[monid];

	int intrs_en = uncmon_lock(mon);

	for (unsigned int ctr = 0; ctr < UNCORE_NCTRS; ctr++) {
		if (ctr_mask & (1ULL << ctr)) {
			if (!mon->um_sleeping) {
				uncmon_update_locked(monid, ctr);
			}
			counts[ctr] = mon->um_counts[ctr];
		}
	}
#if MACH_ASSERT
	mon->um_read_since_sleep = true;
#endif /* MACH_ASSERT */

	uncmon_unlock(mon, intrs_en);
}

/*
 * Read all monitor's counters.
 */
static int
uncore_read(uint64_t ctr_mask, uint64_t *counts_out)
{
	assert(ctr_mask != 0);
	assert(counts_out != NULL);

	if (!uncore_active_ctrs) {
		return EPWROFF;
	}
	if (ctr_mask & ~uncore_active_ctrs) {
		return EINVAL;
	}

	for (unsigned int monid = 0; monid < uncore_nmonitors(); monid++) {
		/*
		 * Find this monitor's starting offset into the `counts_out` array.
		 */
		uint64_t *counts = counts_out + (UNCORE_NCTRS * monid);
		uncmon_read_all_counters(monid, ctr_mask, counts);
	}

	return 0;
}

/*
 * Support for monotonic's mtd_add function.
 */

/*
 * Add an event to the current uncore configuration.  This doesn't take effect
 * until the counters are enabled again, so there's no need to involve the
 * monitors.
 */
static int
uncore_add(struct monotonic_config *config, uint32_t *ctr_out)
{
	if (mt_uncore_enabled) {
		return EBUSY;
	}

	uint8_t selector = (uint8_t)config->event;
	uint32_t available = ~uncore_active_ctrs & config->allowed_ctr_mask;

	if (available == 0) {
		return ENOSPC;
	}

	if (!cpc_event_allowed(CPC_HW_UPMU, selector)) {
		return EPERM;
	}

	uint32_t valid_ctrs = (UINT32_C(1) << UNCORE_NCTRS) - 1;
	if ((available & valid_ctrs) == 0) {
		return E2BIG;
	}
	/*
	 * Clear the UPMCs the first time an event is added.
	 */
	if (uncore_active_ctrs == 0) {
		/*
		 * Suspend powerdown until the next reset.
		 */
		assert(!mt_uncore_suspended_cpd);
		suspend_cluster_powerdown();
		mt_uncore_suspended_cpd = true;

		for (unsigned int monid = 0; monid < uncore_nmonitors(); monid++) {
			struct uncore_monitor *mon = &uncore_monitors[monid];

			int intrs_en = uncmon_lock(mon);
			bool remote = uncmon_is_remote(monid);

			if (!mon->um_sleeping) {
				for (unsigned int ctr = 0; ctr < UNCORE_NCTRS; ctr++) {
					if (remote) {
#if UNCORE_PER_CLUSTER
						uncmon_write_counter_locked_r(monid, ctr, 0);
#endif /* UNCORE_PER_CLUSTER */
					} else {
						uncmon_write_counter_locked_l(monid, ctr, 0);
					}
				}
			}
			memset(&mon->um_snaps, 0, sizeof(mon->um_snaps));
			memset(&mon->um_counts, 0, sizeof(mon->um_counts));
			uncmon_unlock(mon, intrs_en);
		}
	}

	uint32_t ctr = __builtin_ffsll(available) - 1;

	uncore_active_ctrs |= UINT64_C(1) << ctr;
	uncore_config.uc_events.uce_ctrs[ctr] = selector;
	uint64_t cpu_mask = UINT64_MAX;
	if (config->cpu_mask != 0) {
		cpu_mask = config->cpu_mask;
	}
	for (unsigned int i = 0; i < uncore_nmonitors(); i++) {
#if UNCORE_PER_CLUSTER
		const unsigned int shift = topology_info->clusters[i].first_cpu_id;
#else /* UNCORE_PER_CLUSTER */
		const unsigned int shift = 0;
#endif /* !UNCORE_PER_CLUSTER */
		uncore_config.uc_cpu_masks[i].uccm_masks[ctr] = (uint16_t)(cpu_mask >> shift);
	}

	*ctr_out = ctr;
	return 0;
}

/*
 * Support for monotonic's mtd_reset function.
 */

/*
 * Reset all configuration and disable the counters if they're currently
 * counting.
 */
static void
uncore_reset(void)
{
	mt_uncore_enabled = false;

	if (!mt_uncore_suspended_cpd) {
		/* If we haven't already suspended CPD, we need to do so now to ensure we can issue remote reads
		 * to every cluster. */
		suspend_cluster_powerdown();
		mt_uncore_suspended_cpd = true;
	}

	if (mt_owns_counters()) {
		for (unsigned int monid = 0; monid < uncore_nmonitors(); monid++) {
			struct uncore_monitor *mon = &uncore_monitors[monid];

			int intrs_en = uncmon_lock(mon);
			bool remote = uncmon_is_remote(monid);
			if (!mon->um_sleeping) {
				if (remote) {
#if UNCORE_PER_CLUSTER
					uncmon_set_counting_locked_r(monid, 0);
#endif /* UNCORE_PER_CLUSTER */
				} else {
					uncmon_set_counting_locked_l(monid, 0);
				}

				for (int ctr = 0; ctr < UNCORE_NCTRS; ctr++) {
					if (uncore_active_ctrs & (1U << ctr)) {
						if (remote) {
#if UNCORE_PER_CLUSTER
							uncmon_write_counter_locked_r(monid, ctr, 0);
#endif /* UNCORE_PER_CLUSTER */
						} else {
							uncmon_write_counter_locked_l(monid, ctr, 0);
						}
					}
				}
			}

			memset(&mon->um_snaps, 0, sizeof(mon->um_snaps));
			memset(&mon->um_counts, 0, sizeof(mon->um_counts));
			if (!mon->um_sleeping) {
				if (remote) {
#if UNCORE_PER_CLUSTER
					uncmon_clear_int_locked_r(monid);
#endif /* UNCORE_PER_CLUSTER */
				} else {
					uncmon_clear_int_locked_l(monid);
				}
			}

			uncmon_unlock(mon, intrs_en);
		}
	}

	uncore_active_ctrs = 0;
	memset(&uncore_config, 0, sizeof(uncore_config));

	if (mt_owns_counters()) {
		for (unsigned int monid = 0; monid < uncore_nmonitors(); monid++) {
			struct uncore_monitor *mon = &uncore_monitors[monid];

			int intrs_en = uncmon_lock(mon);
			bool remote = uncmon_is_remote(monid);
			if (!mon->um_sleeping) {
				if (remote) {
	#if UNCORE_PER_CLUSTER
					uncmon_program_events_locked_r(monid);
	#endif /* UNCORE_PER_CLUSTER */
				} else {
					uncmon_program_events_locked_l(monid);
				}
			}
			uncmon_unlock(mon, intrs_en);
		}
	}

	/* After reset, no counters should be active, so we can allow powerdown again */
	if (mt_uncore_suspended_cpd) {
		resume_cluster_powerdown();
		mt_uncore_suspended_cpd = false;
	}
}

/*
 * Support for monotonic's mtd_enable function.
 */

static void
uncmon_set_enabled_l_locked(unsigned int monid, bool enable)
{
	struct uncore_monitor *mon = &uncore_monitors[monid];
#pragma unused(mon)
	LCK_SPIN_ASSERT(&mon->um_lock, LCK_ASSERT_OWNED);

	if (enable) {
		uncmon_init_locked_l(monid);
		uncmon_program_events_locked_l(monid);
		uncmon_set_counting_locked_l(monid, uncore_active_ctrs);
	} else {
		uncmon_set_counting_locked_l(monid, 0);
	}
}

#if UNCORE_PER_CLUSTER

static void
uncmon_set_enabled_r_locked(unsigned int monid, bool enable)
{
	struct uncore_monitor *mon = &uncore_monitors[monid];
#pragma unused(mon)
	LCK_SPIN_ASSERT(&mon->um_lock, LCK_ASSERT_OWNED);

	if (!mon->um_sleeping) {
		if (enable) {
			uncmon_init_locked_r(monid);
			uncmon_program_events_locked_r(monid);
			uncmon_set_counting_locked_r(monid, uncore_active_ctrs);
		} else {
			uncmon_set_counting_locked_r(monid, 0);
		}
	}
}

#endif /* UNCORE_PER_CLUSTER */

static void
uncore_set_enabled(bool enable)
{
	mt_uncore_enabled = enable;

	for (unsigned int monid = 0; monid < uncore_nmonitors(); monid++) {
		struct uncore_monitor *mon = &uncore_monitors[monid];
		int intrs_en = uncmon_lock(mon);
		if (uncmon_is_remote(monid)) {
#if UNCORE_PER_CLUSTER
			uncmon_set_enabled_r_locked(monid, enable);
#endif /* UNCORE_PER_CLUSTER */
		} else {
			uncmon_set_enabled_l_locked(monid, enable);
		}
		uncmon_unlock(mon, intrs_en);
	}
}

/*
 * Hooks in the machine layer.
 */

static void
uncore_fiq(uint64_t upmsr)
{
	/*
	 * Determine which counters overflowed.
	 */
	uint64_t disable_ctr_mask = (upmsr & UPMSR_OVF_MASK) >> UPMSR_OVF_POS;
	/* should not receive interrupts from inactive counters */
	assert(!(disable_ctr_mask & ~uncore_active_ctrs));

	if (uncore_active_ctrs == 0) {
		return;
	}

	unsigned int monid = uncmon_get_curid();
	struct uncore_monitor *mon = &uncore_monitors[monid];

	int intrs_en = uncmon_lock(mon);

	/*
	 * Disable any counters that overflowed.
	 */
	uncmon_set_counting_locked_l(monid,
	    uncore_active_ctrs & ~disable_ctr_mask);

	/*
	 * With the overflowing counters disabled, capture their counts and reset
	 * the UPMCs and their snapshots to 0.
	 */
	for (unsigned int ctr = 0; ctr < UNCORE_NCTRS; ctr++) {
		if (UPMSR_OVF(upmsr, ctr)) {
			uncmon_update_locked(monid, ctr);
			mon->um_snaps[ctr] = 0;
			uncmon_write_counter_locked_l(monid, ctr, 0);
		}
	}

	/*
	 * Acknowledge the interrupt, now that any overflowed PMCs have been reset.
	 */
	uncmon_clear_int_locked_l(monid);

	/*
	 * Re-enable all active counters.
	 */
	uncmon_set_counting_locked_l(monid, uncore_active_ctrs);

	uncmon_unlock(mon, intrs_en);
}

static void
uncore_save(void)
{
	if (!uncore_active_ctrs) {
		return;
	}

	for (unsigned int monid = 0; monid < uncore_nmonitors(); monid++) {
		struct uncore_monitor *mon = &uncore_monitors[monid];
		int intrs_en = uncmon_lock(mon);

		if (mt_uncore_enabled) {
			if (uncmon_is_remote(monid)) {
#if UNCORE_PER_CLUSTER
				uncmon_set_counting_locked_r(monid, 0);
#endif /* UNCORE_PER_CLUSTER */
			} else {
				uncmon_set_counting_locked_l(monid, 0);
			}
		}

		for (unsigned int ctr = 0; ctr < UNCORE_NCTRS; ctr++) {
			if (uncore_active_ctrs & (1U << ctr)) {
				uncmon_update_locked(monid, ctr);
				mon->um_snaps[ctr] = 0;
				uncmon_write_counter_locked_l(monid, ctr, 0);
			}
		}

		mon->um_sleeping = true;
		uncmon_unlock(mon, intrs_en);
	}
}

static void
uncore_restore(void)
{
	if (!uncore_active_ctrs) {
		return;
	}
	/* Ensure interrupts disabled before reading uncmon_get_curid */
	bool intr = ml_set_interrupts_enabled(false);
	unsigned int curmonid = uncmon_get_curid();

	struct uncore_monitor *mon = &uncore_monitors[curmonid];
	int intrs_en = uncmon_lock(mon);
	if (!mon->um_sleeping) {
		goto out;
	}

	for (unsigned int ctr = 0; ctr < UNCORE_NCTRS; ctr++) {
		if (uncore_active_ctrs & (1U << ctr)) {
			uncmon_write_counter_locked_l(curmonid, ctr, mon->um_snaps[ctr]);
		}
	}
	uncmon_program_events_locked_l(curmonid);
	uncmon_init_locked_l(curmonid);
	mon->um_sleeping = false;
#if MACH_ASSERT
	mon->um_read_since_sleep = false;
#endif /* MACH_ASSERT */

out:
	uncmon_unlock(mon, intrs_en);
	ml_set_interrupts_enabled(intr);
}

#endif /* HAS_UNCORE_CTRS */

#pragma mark common hooks

void
mt_early_init(void)
{
	topology_info = ml_get_topology_info();
}

void
mt_cpu_idle(cpu_data_t *cpu)
{
	core_idle(cpu);
}

void
mt_cpu_run(cpu_data_t *cpu)
{
	struct mt_cpu *mtc;

	assert(cpu != NULL);
	assert(ml_get_interrupts_enabled() == FALSE);

	mtc = &cpu->cpu_monotonic;

	for (int i = 0; i < MT_CORE_NFIXED; i++) {
		mt_core_set_snap(i, mtc->mtc_snaps[i]);
	}

	/* re-enable the counters */
	core_init_execution_modes();

	core_set_enabled();
}

void
mt_cpu_down(cpu_data_t *cpu)
{
	mt_cpu_idle(cpu);
}

void
mt_cpu_up(cpu_data_t *cpu)
{
	mt_cpu_run(cpu);
}

void
mt_sleep(void)
{
#if HAS_UNCORE_CTRS
	uncore_save();
#endif /* HAS_UNCORE_CTRS */
}

void
mt_wake_per_core(void)
{
#if HAS_UNCORE_CTRS
	if (mt_uncore_initted) {
		uncore_restore();
	}
#endif /* HAS_UNCORE_CTRS */
}

uint64_t
mt_count_pmis(void)
{
	uint64_t npmis = 0;
	for (unsigned int i = 0; i < topology_info->num_cpus; i++) {
		cpu_data_t *cpu = (cpu_data_t *)CpuDataEntries[topology_info->cpus[i].cpu_id].cpu_data_vaddr;
		npmis += cpu->cpu_monotonic.mtc_npmis;
	}
	return npmis;
}

static void
mt_cpu_pmi(cpu_data_t *cpu, uint64_t pmcr0)
{
	assert(cpu != NULL);
	assert(ml_get_interrupts_enabled() == FALSE);

	__builtin_arm_wsr64("PMCR0_EL1", PMCR0_INIT);
	/*
	 * Ensure the CPMU has flushed any increments at this point, so PMSR is up
	 * to date.
	 */
	__builtin_arm_isb(ISB_SY);

	cpu->cpu_monotonic.mtc_npmis += 1;
	cpu->cpu_stat.pmi_cnt_wake += 1;

#if MONOTONIC_DEBUG
	if (!PMCR0_PMI(pmcr0)) {
		kprintf("monotonic: mt_cpu_pmi but no PMI (PMCR0 = %#llx)\n",
		    pmcr0);
	}
#else /* MONOTONIC_DEBUG */
#pragma unused(pmcr0)
#endif /* !MONOTONIC_DEBUG */

	uint64_t pmsr = __builtin_arm_rsr64("PMSR_EL1");

#if MONOTONIC_DEBUG
	printf("monotonic: cpu = %d, PMSR = 0x%llx, PMCR0 = 0x%llx\n",
	    cpu_number(), pmsr, pmcr0);
#endif /* MONOTONIC_DEBUG */

#if MACH_ASSERT
	uint64_t handled = 0;
#endif /* MACH_ASSERT */

	/*
	 * monotonic handles any fixed counter PMIs.
	 */
	for (unsigned int i = 0; i < MT_CORE_NFIXED; i++) {
		if ((pmsr & PMSR_OVF(i)) == 0) {
			continue;
		}

#if MACH_ASSERT
		handled |= 1ULL << i;
#endif /* MACH_ASSERT */
		uint64_t count = mt_cpu_update_count(cpu, i);
		cpu->cpu_monotonic.mtc_counts[i] += count;
		mt_core_set_snap(i, mt_core_reset_values[i]);
		cpu->cpu_monotonic.mtc_snaps[i] = mt_core_reset_values[i];

		if (mt_microstackshots && mt_microstackshot_ctr == i) {
			bool user_mode = false;
			arm_saved_state_t *state = get_user_regs(current_thread());
			if (state) {
				user_mode = PSR64_IS_USER(get_saved_state_cpsr(state));
			}
			KDBG_RELEASE(KDBG_EVENTID(DBG_MONOTONIC, DBG_MT_DEBUG, 1),
			    mt_microstackshot_ctr, user_mode);
			mt_microstackshot_pmi_handler(user_mode, mt_microstackshot_ctx);
		} else if (mt_debug) {
			KDBG_RELEASE(KDBG_EVENTID(DBG_MONOTONIC, DBG_MT_DEBUG, 2),
			    i, count);
		}
	}

	/*
	 * KPC handles the configurable counter PMIs.
	 */
	for (unsigned int i = MT_CORE_NFIXED; i < CORE_NCTRS; i++) {
		if (pmsr & PMSR_OVF(i)) {
#if MACH_ASSERT
			handled |= 1ULL << i;
#endif /* MACH_ASSERT */
			extern void kpc_pmi_handler(unsigned int ctr);
			kpc_pmi_handler(i);
		}
	}

#if MACH_ASSERT
	uint64_t pmsr_after_handling = __builtin_arm_rsr64("PMSR_EL1");
	if (pmsr_after_handling != 0) {
		unsigned int first_ctr_ovf = __builtin_ffsll(pmsr_after_handling) - 1;
		uint64_t count = 0;
		const char *extra = "";
		if (first_ctr_ovf >= CORE_NCTRS) {
			extra = " (invalid counter)";
		} else {
			count = mt_core_snap(first_ctr_ovf);
		}

		panic("monotonic: PMI status not cleared on exit from handler, "
		    "PMSR = 0x%llx HANDLE -> -> 0x%llx, handled 0x%llx, "
		    "PMCR0 = 0x%llx, PMC%d = 0x%llx%s", pmsr, pmsr_after_handling,
		    handled, __builtin_arm_rsr64("PMCR0_EL1"), first_ctr_ovf, count, extra);
	}
#endif /* MACH_ASSERT */

	core_set_enabled();
}

#if CPMU_AIC_PMI
void
mt_cpmu_aic_pmi(cpu_id_t source)
{
	struct cpu_data *curcpu = getCpuDatap();
	if (source != curcpu->interrupt_nub) {
		panic("monotonic: PMI from IOCPU %p delivered to %p", source,
		    curcpu->interrupt_nub);
	}
	mt_cpu_pmi(curcpu, __builtin_arm_rsr64("PMCR0_EL1"));
}
#endif /* CPMU_AIC_PMI */

void
mt_fiq(void *cpu, uint64_t pmcr0, uint64_t upmsr)
{
#if CPMU_AIC_PMI
#pragma unused(cpu, pmcr0)
#else /* CPMU_AIC_PMI */
	mt_cpu_pmi(cpu, pmcr0);
#endif /* !CPMU_AIC_PMI */

#if HAS_UNCORE_CTRS
	if (upmsr != 0) {
		uncore_fiq(upmsr);
	}
#else /* HAS_UNCORE_CTRS */
#pragma unused(upmsr)
#endif /* !HAS_UNCORE_CTRS */
}

void
mt_ownership_change(bool available)
{
#if HAS_UNCORE_CTRS
	/*
	 * No need to take the lock here, as this is only manipulated in the UPMU
	 * when the current task already owns the counters and is on its way out.
	 */
	if (!available && uncore_active_ctrs) {
		uncore_reset();
	}
#else
#pragma unused(available)
#endif /* HAS_UNCORE_CTRS */
}

static uint32_t mt_xc_sync;

static void
mt_microstackshot_start_remote(__unused void *arg)
{
	cpu_data_t *cpu = getCpuDatap();

	__builtin_arm_wsr64("PMCR0_EL1", PMCR0_INIT);

	for (int i = 0; i < MT_CORE_NFIXED; i++) {
		uint64_t count = mt_cpu_update_count(cpu, i);
		cpu->cpu_monotonic.mtc_counts[i] += count;
		mt_core_set_snap(i, mt_core_reset_values[i]);
		cpu->cpu_monotonic.mtc_snaps[i] = mt_core_reset_values[i];
	}

	core_set_enabled();

	if (os_atomic_dec(&mt_xc_sync, relaxed) == 0) {
		thread_wakeup((event_t)&mt_xc_sync);
	}
}

int
mt_microstackshot_start_arch(uint64_t period)
{
	uint64_t reset_value = 0;
	int ovf = os_sub_overflow(CTR_MAX, period, &reset_value);
	if (ovf) {
		return ERANGE;
	}

	mt_core_reset_values[mt_microstackshot_ctr] = reset_value;
	cpu_broadcast_xcall(&mt_xc_sync, TRUE, mt_microstackshot_start_remote,
	    mt_microstackshot_start_remote /* cannot pass NULL */);
	return 0;
}

#pragma mark dev nodes

struct mt_device mt_devices[] = {
	[0] = {
		.mtd_name = "core",
		.mtd_init = core_init,
	},
#if HAS_UNCORE_CTRS
	[1] = {
		.mtd_name = "uncore",
		.mtd_init = uncore_init,
		.mtd_add = uncore_add,
		.mtd_reset = uncore_reset,
		.mtd_enable = uncore_set_enabled,
		.mtd_read = uncore_read,

		.mtd_ncounters = UNCORE_NCTRS,
	}
#endif /* HAS_UNCORE_CTRS */
};

static_assert(
	(sizeof(mt_devices) / sizeof(mt_devices[0])) == MT_NDEVS,
	"MT_NDEVS macro should be same as the length of mt_devices");
