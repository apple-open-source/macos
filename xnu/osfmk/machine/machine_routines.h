/*
 * Copyright (c) 2000-2007 Apple Inc. All rights reserved.
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
#ifndef _MACHINE_MACHINE_ROUTINES_H
#define _MACHINE_MACHINE_ROUTINES_H

#include <sys/cdefs.h>
#include <stdint.h>

#if defined (__i386__) || defined(__x86_64__)
#include "i386/machine_routines.h"
#elif defined (__arm__) || defined (__arm64__)
#include "arm/machine_routines.h"
#else
#error architecture not supported
#endif

__BEGIN_DECLS

#ifdef XNU_KERNEL_PRIVATE
#pragma GCC visibility push(hidden)

/*!
 * @function      ml_cpu_can_exit
 * @brief         Check whether the platform code allows |cpu_id| to be
 *                shut down at runtime outside of system sleep.
 * @return        true if allowed, false otherwise
 */
bool ml_cpu_can_exit(int cpu_id);

/*!
 * @function      ml_cpu_begin_state_transition
 * @brief         Tell the platform code that processor_start() or
 *                processor_exit() is about to begin for |cpu_id|.  This
 *                can block.
 * @param cpu_id  CPU that is (potentially) going up or down
 */
void ml_cpu_begin_state_transition(int cpu_id);

/*!
 * @function      ml_cpu_end_state_transition
 * @brief         Tell the platform code that processor_start() or
 *                processor_exit() is finished for |cpu_id|.  This
 *                can block.  Can be called from a different thread from
 *                ml_cpu_begin_state_transition().
 * @param cpu_id  CPU that is (potentially) going up or down
 */
void ml_cpu_end_state_transition(int cpu_id);

/*!
 * @function      ml_cpu_begin_loop
 * @brief         Acquire a global lock that prevents processor_start() or
 *                processor_exit() from changing any CPU states for the
 *                duration of a loop.  This can block.
 */
void ml_cpu_begin_loop(void);

/*!
 * @function      ml_cpu_end_loop
 * @brief         Release the global lock acquired by ml_cpu_begin_loop().
 *                Must be called from the same thread as ml_cpu_begin_loop().
 */
void ml_cpu_end_loop(void);

/*!
 * @function      ml_early_cpu_max_number()
 * @brief         Returns an early maximum cpu number the kernel will ever use.
 *
 * @return        the maximum cpu number the kernel will ever use.
 *
 * @discussion
 * The value returned by this function might be an over-estimate,
 * but is more precise than @c MAX_CPUS.
 *
 * Unlike @c real_ncpus which is only initialized late in boot,
 * this can be called during startup after the @c STARTUP_SUB_TUNABLES
 * subsystem has been initialized.
 */
int ml_early_cpu_max_number(void);

/*!
 * @function        ml_cpu_power_enable
 * @abstract        Enable voltage rails to a CPU prior to bringing it up
 * @discussion      Called from the scheduler to enable any voltage rails
 *                  needed by a CPU.  This should happen before the
 *                  CPU_BOOT_REQUESTED broadcast.  This does not boot the
 *                  CPU and it may be a no-op on some platforms.  This must be
 *                  called from a schedulable context.
 * @param cpu_id    The logical CPU ID (from the topology) of the CPU to be booted
 */
void ml_cpu_power_enable(int cpu_id);

/*!
 * @function        ml_cpu_power_disable
 * @abstract        Disable voltage rails to a CPU after bringing it down
 * @discussion      Called from the scheduler to disable any voltage rails
 *                  that are no longer needed by an offlined CPU or cluster.
 *                  This should happen after the CPU_EXITED broadcast.
 *                  This does not halt the CPU and it may be a no-op on some
 *                  platforms.  This must be called from a schedulable context.
 * @param cpu_id    The logical CPU ID (from the topology) of the halted CPU
 */
void ml_cpu_power_disable(int cpu_id);

#pragma GCC visibility pop
#endif /* defined(XNU_KERNEL_PRIVATE) */

/*!
 * @enum     cpu_event
 * @abstract Broadcast events allowing clients to hook CPU state transitions.
 * @constant CPU_BOOT_REQUESTED      Called from processor_start(); may block.
 * @constant CPU_BOOTED              Called from platform code on the newly-booted CPU; may not block.
 * @constant CPU_ACTIVE              Called from scheduler code; may block.
 * @constant CLUSTER_ACTIVE          Called from platform code; may block.
 * @constant CPU_EXIT_REQUESTED      Called from processor_exit(); may block.
 * @constant CPU_DOWN                Called from platform code on the disabled CPU; may not block.
 * @constant CLUSTER_EXIT_REQUESTED  Called from platform code; may block.
 * @constant CPU_EXITED              Called after CPU is stopped; may block.
 */
enum cpu_event {
	CPU_BOOT_REQUESTED = 0,
	CPU_BOOTED,
	CPU_ACTIVE,
	CLUSTER_ACTIVE,
	CPU_EXIT_REQUESTED,
	CPU_DOWN,
	CLUSTER_EXIT_REQUESTED,
	CPU_EXITED,
};

typedef bool (*cpu_callback_t)(void *param, enum cpu_event event, unsigned int cpu_or_cluster);

/*!
 * @function              cpu_event_register_callback
 * @abstract              Register a function to be called on CPU state changes.
 * @param fn              Function to call on state change events.
 * @param param           Optional argument to be passed to the callback (e.g. object pointer).
 */
void cpu_event_register_callback(cpu_callback_t fn, void *param);

/*!
 * @function              cpu_event_unregister_callback
 * @abstract              Unregister a previously-registered callback function.
 * @param fn              Function pointer previously passed to cpu_event_register_callback().
 */
void cpu_event_unregister_callback(cpu_callback_t fn);

#if XNU_KERNEL_PRIVATE
/*!
 * @function              ml_broadcast_cpu_event
 * @abstract              Internal XNU function used to broadcast CPU state changes to callers.
 * @param event           CPU event that is occurring.
 * @param cpu_or_cluster  Logical CPU ID of the core (or cluster) affected by the event.
 */
void ml_broadcast_cpu_event(enum cpu_event event, unsigned int cpu_or_cluster);
#endif

/*!
 * @function      ml_io_read()
 * @brief         Perform an MMIO read access
 *
 * @return        The value resulting from the read.
 *
 */
unsigned long long ml_io_read(uintptr_t iovaddr, int iovsz);
unsigned int ml_io_read8(uintptr_t iovaddr);
unsigned int ml_io_read16(uintptr_t iovaddr);
unsigned int ml_io_read32(uintptr_t iovaddr);
unsigned long long ml_io_read64(uintptr_t iovaddr);

/*!
 * @function      ml_io_write()
 * @brief         Perform an MMIO write access
 *
 */
void ml_io_write(uintptr_t vaddr, uint64_t val, int size);
void ml_io_write8(uintptr_t vaddr, uint8_t val);
void ml_io_write16(uintptr_t vaddr, uint16_t val);
void ml_io_write32(uintptr_t vaddr, uint32_t val);
void ml_io_write64(uintptr_t vaddr, uint64_t val);

#if XNU_KERNEL_PRIVATE
/*
 * ml_io access timeouts and tracing.
 *
 * We are specific in what to compile in, in order to not burden
 * heavily used code with paths that will never be used on common
 * configurations.
 */

/* ml_io_read/write timeouts are generally enabled on macOS, because
 * they may help developers. */
#if  (XNU_TARGET_OS_OSX || DEVELOPMENT || DEBUG)

#define ML_IO_TIMEOUTS_ENABLED 1

/* Simulating stretched IO is only for DEVELOPMENT || DEBUG. */
#if DEVELOPMENT || DEBUG
#define ML_IO_SIMULATE_STRETCHED_ENABLED 1
#endif

/* We also check that the memory is mapped non-cacheable on x86 internally. */
#if defined(__x86_64__) && (DEVELOPMENT || DEBUG)
#define ML_IO_VERIFY_UNCACHEABLE 1
#endif

#endif /* (XNU_TARGET_OS_OSX || DEVELOPMENT || DEBUG) */
#endif /* XNU_KERNEL_PRIVATE */

#if KERNEL_PRIVATE

/*!
 * @function                    ml_io_increase_timeouts
 * @brief                       Increase the ml_io_read* and ml_io_write*
 *                              timeouts for a region of VA space
 *                              [`iovaddr_base', `iovaddr_base' + `size').
 * @discussion                  This function is intended for building an
 *                              allowlist of known-misbehaving register spaces
 *                              on specific peripherals.  `size' must be between
 *                              1 and 4096 inclusive, and the VA range must not
 *                              overlap with any ranges previously passed to
 *                              ml_io_increase_timeouts().
 * @note                        This function has no effect when the new timeouts are
 *                              shorter than the global timeouts.
 * @param iovaddr_base          Base VA of the target region
 * @param size                  Size of the target region, in bytes
 * @param read_timeout_us       New read timeout, in microseconds
 * @param write_timeout_us      New write timeout, in microseconds
 * @return                      0 if successful, or KERN_INVALID_ARGUMENT if either
 *                              the VA range or timeout is invalid.
 */
OS_WARN_RESULT
int ml_io_increase_timeouts(uintptr_t iovaddr_base, unsigned int size, uint32_t read_timeout_us, uint32_t write_timeout_us);

/*!
 * @function            ml_io_reset_timeouts
 * @brief               Unregister custom timeouts previously registered by
 *                      ml_io_increase_timeouts().
 * @discussion          The caller must use the exact `iovaddr_base' and `size'
 *                      range passed to a previous ml_io_increase_timeouts()
 *                      call.  Unregistering a smaller subrange is unsupported
 *                      and will return an error.
 * @param iovaddr_base  Base VA previously passed to ml_io_increase_timeouts()
 * @param size          Size previously passed to ml_io_increase_timeouts()
 * @return              0 if successful, or KERN_NOT_FOUND if the specfied range
 *                      does not match a previously-registered timeout.
 */
OS_WARN_RESULT
int ml_io_reset_timeouts(uintptr_t iovaddr_base, unsigned int size);

/*!
 * @function                    ml_io_increase_timeouts_phys
 * @brief                       Increase the ml_io_read* and ml_io_write*
 *                              timeouts for a region of PA space
 *                              [`iopaddr_base', `iopaddr_base' + `size').
 * @discussion                  This function is intended for building an
 *                              allowlist of known-misbehaving register spaces
 *                              on specific peripherals.  `size' must be between
 *                              1 and 4096 inclusive, and the PA range must not
 *                              overlap with any ranges previously passed to
 *                              ml_io_increase_timeouts().
 * @note                        This function has no effect when the new timeouts are
 *                              shorter than the global timeouts. In addition to
 *                              global timeouts a larger timeout may be applied
 *                              to regions of memory which may be susceptible to
 *                              PCIe CTOs.
 *                              For IOs performed through virtual addresses, the
 *                              larger of the VA timeout (if one is set) and
 *                              this timeout is used.
 * @param iopaddr_base          Base PA of the target region
 * @param size                  Size of the target region, in bytes
 * @param read_timeout_us       New read timeout, in microseconds
 * @param write_timeout_us      New write timeout, in microseconds
 * @return                      0 if successful, or KERN_INVALID_ARGUMENT if either
 *                              the PA range or timeout is invalid.
 */
OS_WARN_RESULT
int ml_io_increase_timeouts_phys(vm_offset_t iopaddr_base, unsigned int size,
    uint32_t read_timeout_us, uint32_t write_timeout_us);

/*!
 * @function            ml_io_reset_timeouts_phys
 * @brief               Unregister custom timeouts previously registered by
 *                      ml_io_increase_timeouts_phys().
 * @discussion          The caller must use the exact `iopaddr_base' and `size'
 *                      range passed to a previous ml_io_increase_timeouts_phys()
 *                      call.  Unregistering a smaller subrange is unsupported
 *                      and will return an error.
 * @param iopaddr_base  Base PA previously passed to ml_io_increase_timeouts_phys()
 * @param size          Size previously passed to ml_io_increase_timeouts_phys()
 * @return              0 if successful, or KERN_NOT_FOUND if the specfied range
 *                      does not match a previously-registered timeout.
 */
OS_WARN_RESULT
int ml_io_reset_timeouts_phys(vm_offset_t iopaddr_base, unsigned int size);

#endif /* KERNEL_PRIVATE */

#if XNU_KERNEL_PRIVATE

#if ML_IO_TIMEOUTS_ENABLED
#if !defined(__x86_64__)
/* x86 does not have the MACHINE_TIMEOUTs types, and the variables are
 * declared elsewhere. */
extern machine_timeout_t report_phy_read_delay_to;
extern machine_timeout_t report_phy_write_delay_to;
extern machine_timeout_t report_phy_read_delay_to;
extern machine_timeout_t trace_phy_read_delay_to;
extern machine_timeout_t trace_phy_write_delay_to;
#endif /* !defined(__x86_64__) */
extern void override_io_timeouts(uintptr_t vaddr, uint64_t paddr,
    uint64_t *read_timeout, uint64_t *write_timeout);
#endif /* ML_IO_TIMEOUTS_ENABLED */

void ml_get_cluster_type_name(cluster_type_t cluster_type, char *name,
    size_t name_size);

unsigned int ml_get_cluster_count(void);

/**
 * Depending on the system, it's possible that a kernel backtrace could contain
 * stack frames from both XNU and non-XNU-owned stacks. This function can be
 * used to determine whether an address is pointing to one of these non-XNU
 * stacks.
 *
 * @param addr The virtual address to check.
 *
 * @return True if the address is within the bounds of a non-XNU stack. False
 *         otherwise.
 */
bool ml_addr_in_non_xnu_stack(uintptr_t addr);

#endif /* XNU_KERNEL_PRIVATE */

#if MACH_KERNEL_PRIVATE

/*!
 * @func          ml_map_cpus_to_clusters
 * @brief         Populate the logical CPU -> logical cluster ID table at address addr.
 *
 * @param table   array to write to
 */
void ml_map_cpus_to_clusters(uint8_t *table);

#endif /* MACH_KERNEL_PRIVATE */

#if MACH_KERNEL_PRIVATE
/*!
 * @func          ml_task_post_signature_processing_hook
 * @brief         Platform-specific hook called on the main thread of a new task
 *                after process_signature() is completed by the parent and before
 *                the main thread returns to EL0.
 *
 * @param task    The new task whose signature has been processed
 */
void ml_task_post_signature_processing_hook(task_t task);
#endif /* MACH_KERNEL_PRIVATE */

__END_DECLS

#endif /* _MACHINE_MACHINE_ROUTINES_H */
