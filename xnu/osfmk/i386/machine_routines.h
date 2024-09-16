/*
 * Copyright (c) 2000-2019 Apple Inc. All rights reserved.
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

#ifndef _I386_MACHINE_ROUTINES_H_
#define _I386_MACHINE_ROUTINES_H_

#include <mach/mach_types.h>
#include <mach/boolean.h>
#include <kern/kern_types.h>
#include <pexpert/pexpert.h>

#include <sys/cdefs.h>
#include <sys/appleapiopts.h>

#include <stdarg.h>

__BEGIN_DECLS

#ifdef XNU_KERNEL_PRIVATE

/* are we a 64 bit platform ? */

boolean_t ml_is64bit(void);

/* is this a 64bit thread? */

boolean_t ml_thread_is64bit(thread_t);

/* is this a 64bit thread? */

boolean_t ml_state_is64bit(void *);

/* set state of fpu save area for signal handling */

void    ml_fp_setvalid(boolean_t);

void    ml_cpu_set_ldt(int);

/* Interrupt handling */

/* Initialize Interrupts */
void    ml_init_interrupt(void);

/* Generate a fake interrupt */
void ml_cause_interrupt(void);

/* Initialize Interrupts */
void ml_install_interrupt_handler(
	void *nub,
	int source,
	void *target,
	IOInterruptHandler handler,
	void *refCon);

uint64_t ml_get_timebase(void);
uint64_t ml_get_timebase_entropy(void);

#if MACH_KERNEL_PRIVATE
/**
 * Issue a barrier that guarantees all prior memory accesses will complete
 * before any subsequent timebase reads.
 */
static inline void
ml_memory_to_timebase_fence(void)
{
	/*
	 * No-op on x86.  mach_absolute_time() & co. have load and lfence
	 * instructions that already guarantee this ordering.
	 */
}

/**
 * Issue a barrier that guarantees all prior timebase reads will
 * be ordered before any subsequent memory accesses.
 */
static inline void
ml_timebase_to_memory_fence(void)
{
}
#endif /* MACH_KERNEL_PRIVATE */

void ml_init_delay_spin_threshold(int);

boolean_t ml_delay_should_spin(uint64_t interval);

extern void ml_delay_on_yield(void);

vm_offset_t
    ml_static_ptovirt(
	vm_offset_t);

void ml_static_mfree(
	vm_offset_t,
	vm_size_t);

kern_return_t
ml_static_protect(
	vm_offset_t start,
	vm_size_t size,
	vm_prot_t new_prot);

kern_return_t
ml_static_verify_page_protections(
	uint64_t base, uint64_t size, vm_prot_t prot);

/* virtual to physical on wired pages */
vm_offset_t ml_vtophys(
	vm_offset_t vaddr);

vm_size_t ml_nofault_copy(
	vm_offset_t virtsrc, vm_offset_t virtdst, vm_size_t size);

boolean_t ml_validate_nofault(
	vm_offset_t virtsrc, vm_size_t size);

/* Machine topology info */
typedef enum {
	CLUSTER_TYPE_SMP,
	MAX_CPU_TYPES,
} cluster_type_t;

uint64_t ml_cpu_cache_size(unsigned int level);

/* Set the maximum number of CPUs */
void ml_set_max_cpus(
	unsigned int max_cpus);

extern void     ml_cpu_init_completed(void);
extern void     ml_cpu_up(void);
extern void     ml_cpu_down(void);
extern void     ml_cpu_up_update_counts(int cpu_id);
extern void     ml_cpu_down_update_counts(int cpu_id);

void bzero_phys_nc(
	addr64_t phys_address,
	uint32_t length);
extern uint32_t interrupt_timer_coalescing_enabled;
extern uint32_t idle_entry_timer_processing_hdeadline_threshold;

#if TCOAL_INSTRUMENT
#define TCOAL_DEBUG KERNEL_DEBUG_CONSTANT
#else
#define TCOAL_DEBUG(x, a, b, c, d, e) do { } while(0)
#endif /* TCOAL_INSTRUMENT */

#if     defined(PEXPERT_KERNEL_PRIVATE) || defined(MACH_KERNEL_PRIVATE)
/* IO memory map services */

extern vm_offset_t      io_map(
	vm_map_offset_t         phys_addr,
	vm_size_t               size,
	unsigned int            flags,
	vm_prot_t               prot,
	bool                    unmappable);

/* Map memory map IO space */
vm_offset_t ml_io_map(
	vm_offset_t phys_addr,
	vm_size_t size);

vm_offset_t ml_io_map_wcomb(
	vm_offset_t phys_addr,
	vm_size_t size);

vm_offset_t ml_io_map_unmappable(
	vm_offset_t phys_addr,
	vm_size_t size,
	unsigned int flags);

void    ml_get_bouncepool_info(
	vm_offset_t *phys_addr,
	vm_size_t   *size);
/* Indicates if spinlock, IPI and other timeouts should be suspended */
boolean_t machine_timeout_suspended(void);
void plctrace_disable(void);
#endif /* PEXPERT_KERNEL_PRIVATE || MACH_KERNEL_PRIVATE  */

/* Warm up a CPU to receive an interrupt */
kern_return_t ml_interrupt_prewarm(uint64_t deadline);

/* Machine layer routine for intercepting panics */
__printflike(1, 0)
void ml_panic_trap_to_debugger(const char *panic_format_str,
    va_list *panic_args,
    unsigned int reason,
    void *ctx,
    uint64_t panic_options_mask,
    unsigned long panic_caller,
    const char *panic_initiator);
#endif /* XNU_KERNEL_PRIVATE */

#ifdef KERNEL_PRIVATE

/* Type for the Time Base Enable function */
typedef void (*time_base_enable_t)(cpu_id_t cpu_id, boolean_t enable);

/* Type for the IPI Hander */
typedef void (*ipi_handler_t)(void);

/* Struct for ml_processor_register */
struct ml_processor_info {
	cpu_id_t                        cpu_id;
	boolean_t                       boot_cpu;
	vm_offset_t                     start_paddr;
	boolean_t                       supports_nap;
	unsigned long           l2cr_value;
	time_base_enable_t      time_base_enable;
};

typedef struct ml_processor_info ml_processor_info_t;


/* Register a processor */
kern_return_t
ml_processor_register(
	cpu_id_t        cpu_id,
	uint32_t        lapic_id,
	processor_t     *processor_out,
	boolean_t       boot_cpu,
	boolean_t       start );

/* PCI config cycle probing */
boolean_t ml_probe_read(
	vm_offset_t paddr,
	unsigned int *val);
boolean_t ml_probe_read_64(
	addr64_t paddr,
	unsigned int *val);

/* Read physical address byte */
unsigned int ml_phys_read_byte(
	vm_offset_t paddr);
unsigned int ml_phys_read_byte_64(
	addr64_t paddr);

/* Read physical address half word */
unsigned int ml_phys_read_half(
	vm_offset_t paddr);
unsigned int ml_phys_read_half_64(
	addr64_t paddr);

/* Read physical address word*/
unsigned int ml_phys_read(
	vm_offset_t paddr);
unsigned int ml_phys_read_64(
	addr64_t paddr);
unsigned int ml_phys_read_word(
	vm_offset_t paddr);
unsigned int ml_phys_read_word_64(
	addr64_t paddr);

/* Read physical address double word */
unsigned long long ml_phys_read_double(
	vm_offset_t paddr);
unsigned long long ml_phys_read_double_64(
	addr64_t paddr);

extern uint32_t ml_port_io_read(uint16_t ioport, int size);
extern uint8_t ml_port_io_read8(uint16_t ioport);
extern uint16_t ml_port_io_read16(uint16_t ioport);
extern uint32_t ml_port_io_read32(uint16_t ioport);
extern void ml_port_io_write(uint16_t ioport, uint32_t val, int size);
extern void ml_port_io_write8(uint16_t ioport, uint8_t val);
extern void ml_port_io_write16(uint16_t ioport, uint16_t val);
extern void ml_port_io_write32(uint16_t ioport, uint32_t val);

/* Write physical address byte */
void ml_phys_write_byte(
	vm_offset_t paddr, unsigned int data);
void ml_phys_write_byte_64(
	addr64_t paddr, unsigned int data);

/* Write physical address half word */
void ml_phys_write_half(
	vm_offset_t paddr, unsigned int data);
void ml_phys_write_half_64(
	addr64_t paddr, unsigned int data);

/* Write physical address word */
void ml_phys_write(
	vm_offset_t paddr, unsigned int data);
void ml_phys_write_64(
	addr64_t paddr, unsigned int data);
void ml_phys_write_word(
	vm_offset_t paddr, unsigned int data);
void ml_phys_write_word_64(
	addr64_t paddr, unsigned int data);

/* Write physical address double word */
void ml_phys_write_double(
	vm_offset_t paddr, unsigned long long data);
void ml_phys_write_double_64(
	addr64_t paddr, unsigned long long data);

/* Struct for ml_cpu_get_info */
struct ml_cpu_info {
	uint32_t        vector_unit;
	uint32_t        cache_line_size;
	uint32_t        l1_icache_size;
	uint32_t        l1_dcache_size;
	uint32_t        l2_settings;
	uint32_t        l2_cache_size;
	uint32_t        l3_settings;
	uint32_t        l3_cache_size;
};

typedef struct ml_cpu_info ml_cpu_info_t;

/* Get processor info */
void ml_cpu_get_info(ml_cpu_info_t *ml_cpu_info);

void ml_thread_policy(
	thread_t thread,
	unsigned policy_id,
	unsigned policy_info);

#define MACHINE_GROUP                                   0x00000001
#define MACHINE_NETWORK_GROUP                   0x10000000
#define MACHINE_NETWORK_WORKLOOP                0x00000001
#define MACHINE_NETWORK_NETISR                  0x00000002

/* Return the maximum number of CPUs set by ml_set_max_cpus(), blocking if necessary */
unsigned int ml_wait_max_cpus(
	void);

/*
 * The following are in pmCPU.c not machine_routines.c.
 */
extern void ml_set_maxsnoop(uint32_t maxdelay);
extern unsigned ml_get_maxsnoop(void);
extern void ml_set_maxbusdelay(uint32_t mdelay);
extern uint32_t ml_get_maxbusdelay(void);
extern void ml_set_maxintdelay(uint64_t mdelay);
extern uint64_t ml_get_maxintdelay(void);
extern boolean_t ml_get_interrupt_prewake_applicable(void);


extern uint64_t tmrCvt(uint64_t time, uint64_t conversion);

extern uint64_t ml_cpu_int_event_time(void);

#endif /* KERNEL_PRIVATE */

/* Get Interrupts Enabled */
boolean_t ml_get_interrupts_enabled(void);

/* Set Interrupts Enabled */
boolean_t ml_set_interrupts_enabled(boolean_t enable);
boolean_t ml_early_set_interrupts_enabled(boolean_t enable);

/* Check if running at interrupt context */
boolean_t ml_at_interrupt_context(void);

#ifdef XNU_KERNEL_PRIVATE

bool ml_did_interrupt_userspace(void);

extern boolean_t ml_is_quiescing(void);
extern void ml_set_is_quiescing(boolean_t);
extern uint64_t ml_get_booter_memory_size(void);
unsigned int ml_cpu_cache_sharing(unsigned int level, cluster_type_t cluster_type, bool include_all_cpu_types);
void ml_cpu_get_info_type(ml_cpu_info_t * ml_cpu_info, cluster_type_t cluster_type);
unsigned int ml_get_cpu_number_type(cluster_type_t cluster_type, bool logical, bool available);
unsigned int ml_get_cluster_number_type(cluster_type_t cluster_type);
unsigned int ml_get_cpu_types(void);
#endif

/* Zero bytes starting at a physical address */
void bzero_phys(
	addr64_t phys_address,
	uint32_t length);

/* Bytes available on current stack */
vm_offset_t ml_stack_remaining(void);

#if defined(MACH_KERNEL_PRIVATE)
__private_extern__ uint64_t ml_phys_read_data(uint64_t paddr, int psz);
__private_extern__ void ml_phys_write_data(uint64_t paddr,
    unsigned long long data, int size);
__private_extern__ uintptr_t
pmap_verify_noncacheable(uintptr_t vaddr);
void machine_lockdown(void);
#endif /* MACH_KERNEL_PRIVATE */
#ifdef  XNU_KERNEL_PRIVATE

boolean_t ml_fpu_avx_enabled(void);
boolean_t ml_fpu_avx512_enabled(void);

void interrupt_latency_tracker_setup(void);
void interrupt_reset_latency_stats(void);
void interrupt_populate_latency_stats(char *, unsigned);
void ml_get_power_state(boolean_t *, boolean_t *);

void timer_queue_expire_rescan(void*);
void ml_timer_evaluate(void);
boolean_t ml_timer_forced_evaluation(void);

void ml_gpu_stat_update(uint64_t);
uint64_t ml_gpu_stat(thread_t);
boolean_t ml_recent_wake(void);

#ifdef MACH_KERNEL_PRIVATE
struct i386_cpu_info;
struct machine_thread;
/* LBR support */
void i386_lbr_init(struct i386_cpu_info *info_p, bool is_master);
int i386_filtered_lbr_state_to_mach_thread_state(thread_t thr_act, last_branch_state_t *machlbrp, boolean_t from_userspace);
void i386_lbr_synch(thread_t thr);
void i386_lbr_enable(void);
void i386_lbr_disable(void);
extern lbr_modes_t last_branch_enabled_modes;
#endif

extern uint64_t report_phy_read_delay;
extern uint64_t report_phy_write_delay;
extern uint32_t phy_read_panic;
extern uint32_t phy_write_panic;
extern uint64_t trace_phy_read_delay;
extern uint64_t trace_phy_write_delay;

void ml_hibernate_active_pre(void);
void ml_hibernate_active_post(void);

int ml_page_protection_type(void);

#endif /* XNU_KERNEL_PRIVATE */

__END_DECLS

#endif /* _I386_MACHINE_ROUTINES_H_ */
