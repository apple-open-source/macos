/*
 * Copyright (c) 2007-2024 Apple Inc. All rights reserved.
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
#include <debug.h>
#include <mach/mach_types.h>
#include <mach/kern_return.h>
#include <mach/thread_status.h>
#include <kern/thread.h>
#include <kern/kalloc.h>
#include <arm/vmparam.h>
#include <arm/cpu_data_internal.h>
#include <arm/misc_protos.h>
#include <arm64/machine_machdep.h>
#include <arm64/proc_reg.h>
#include <sys/random.h>
#if __has_feature(ptrauth_calls)
#include <ptrauth.h>
#endif

#include <libkern/coreanalytics/coreanalytics.h>


struct arm_vfpv2_state {
	__uint32_t __r[32];
	__uint32_t __fpscr;
};

typedef struct arm_vfpv2_state arm_vfpv2_state_t;

#define ARM_VFPV2_STATE_COUNT \
	((mach_msg_type_number_t)(sizeof (arm_vfpv2_state_t)/sizeof(uint32_t)))

/*
 * Forward definitions
 */
void thread_set_child(thread_t child, int pid);
static void free_debug_state(thread_t thread);
user_addr_t thread_get_sigreturn_token(thread_t thread);
uint32_t thread_get_sigreturn_diversifier(thread_t thread);

/*
 * Maps state flavor to number of words in the state:
 */
/* __private_extern__ */
unsigned int _MachineStateCount[THREAD_STATE_FLAVORS] = {
	[ARM_UNIFIED_THREAD_STATE] = ARM_UNIFIED_THREAD_STATE_COUNT,
	[ARM_VFP_STATE] = ARM_VFP_STATE_COUNT,
	[ARM_EXCEPTION_STATE] = ARM_EXCEPTION_STATE_COUNT,
	[ARM_DEBUG_STATE] = ARM_DEBUG_STATE_COUNT,
	[ARM_THREAD_STATE64] = ARM_THREAD_STATE64_COUNT,
	[ARM_EXCEPTION_STATE64] = ARM_EXCEPTION_STATE64_COUNT,
	[ARM_EXCEPTION_STATE64_V2] = ARM_EXCEPTION_STATE64_V2_COUNT,
	[ARM_THREAD_STATE32] = ARM_THREAD_STATE32_COUNT,
	[ARM_DEBUG_STATE32] = ARM_DEBUG_STATE32_COUNT,
	[ARM_DEBUG_STATE64] = ARM_DEBUG_STATE64_COUNT,
	[ARM_NEON_STATE] = ARM_NEON_STATE_COUNT,
	[ARM_NEON_STATE64] = ARM_NEON_STATE64_COUNT,
	[ARM_PAGEIN_STATE] = ARM_PAGEIN_STATE_COUNT,
};

extern zone_t ads_zone;

#if __arm64__
/*
 * Copy values from saved_state to ts64.
 */
void
saved_state_to_thread_state64(const arm_saved_state_t * saved_state,
    arm_thread_state64_t *    ts64)
{
	uint32_t i;

	assert(is_saved_state64(saved_state));

	ts64->fp = get_saved_state_fp(saved_state);
	ts64->lr = get_saved_state_lr(saved_state);
	ts64->sp = get_saved_state_sp(saved_state);
	ts64->pc = get_saved_state_pc(saved_state);
	ts64->cpsr = get_saved_state_cpsr(saved_state);
	for (i = 0; i < 29; i++) {
		ts64->x[i] = get_saved_state_reg(saved_state, i);
	}
}

/*
 * Copy values from ts64 to saved_state.
 *
 * For safety, CPSR is sanitized as follows:
 *
 * - ts64->cpsr.{N,Z,C,V} are copied as-is into saved_state->cpsr
 * - ts64->cpsr.M is ignored, and saved_state->cpsr.M is reset to EL0
 * - All other saved_state->cpsr bits are preserved as-is
 */
void
thread_state64_to_saved_state(const arm_thread_state64_t * ts64,
    arm_saved_state_t *          saved_state)
{
	uint32_t i;
#if __has_feature(ptrauth_calls)
	uint64_t intr = ml_pac_safe_interrupts_disable();
#endif /* __has_feature(ptrauth_calls) */

	assert(is_saved_state64(saved_state));

	const uint32_t CPSR_COPY_MASK = PSR64_USER_MASK;
	const uint32_t CPSR_ZERO_MASK = PSR64_MODE_MASK;
	const uint32_t CPSR_PRESERVE_MASK = ~(CPSR_COPY_MASK | CPSR_ZERO_MASK);
#if __has_feature(ptrauth_calls)
	/* BEGIN IGNORE CODESTYLE */
	MANIPULATE_SIGNED_USER_THREAD_STATE(saved_state,
		"and	w2, w2, %w[preserve_mask]"	"\n"
		"mov	w6, %w[cpsr]"			"\n"
		"and	w6, w6, %w[copy_mask]"		"\n"
		"orr	w2, w2, w6"			"\n"
		"str	w2, [x0, %[SS64_CPSR]]"		"\n",
		[cpsr] "r"(ts64->cpsr),
		[preserve_mask] "i"(CPSR_PRESERVE_MASK),
		[copy_mask] "i"(CPSR_COPY_MASK)
	);
	/* END IGNORE CODESTYLE */
	/*
	 * Make writes to ts64->cpsr visible first, since it's useful as a
	 * canary to detect thread-state corruption.
	 */
	__builtin_arm_dmb(DMB_ST);
#else
	uint32_t new_cpsr = get_saved_state_cpsr(saved_state);
	new_cpsr &= CPSR_PRESERVE_MASK;
	new_cpsr |= (ts64->cpsr & CPSR_COPY_MASK);
	set_user_saved_state_cpsr(saved_state, new_cpsr);
#endif /* __has_feature(ptrauth_calls) */
	set_saved_state_fp(saved_state, ts64->fp);
	set_user_saved_state_lr(saved_state, ts64->lr);
	set_saved_state_sp(saved_state, ts64->sp);
	set_user_saved_state_pc(saved_state, ts64->pc);
	for (i = 0; i < 29; i++) {
		set_user_saved_state_reg(saved_state, i, ts64->x[i]);
	}

#if __has_feature(ptrauth_calls)
	ml_pac_safe_interrupts_restore(intr);
#endif /* __has_feature(ptrauth_calls) */
}

#endif /* __arm64__ */

static kern_return_t
handle_get_arm32_thread_state(thread_state_t            tstate,
    mach_msg_type_number_t *  count,
    const arm_saved_state_t * saved_state)
{
	if (*count < ARM_THREAD_STATE32_COUNT) {
		return KERN_INVALID_ARGUMENT;
	}
	if (!is_saved_state32(saved_state)) {
		return KERN_INVALID_ARGUMENT;
	}

	(void)saved_state_to_thread_state32(saved_state, (arm_thread_state32_t *)tstate);
	*count = ARM_THREAD_STATE32_COUNT;
	return KERN_SUCCESS;
}

static kern_return_t
handle_get_arm64_thread_state(thread_state_t            tstate,
    mach_msg_type_number_t *  count,
    const arm_saved_state_t * saved_state)
{
	if (*count < ARM_THREAD_STATE64_COUNT) {
		return KERN_INVALID_ARGUMENT;
	}
	if (!is_saved_state64(saved_state)) {
		return KERN_INVALID_ARGUMENT;
	}

	(void)saved_state_to_thread_state64(saved_state, (arm_thread_state64_t *)tstate);
	*count = ARM_THREAD_STATE64_COUNT;
	return KERN_SUCCESS;
}


static kern_return_t
handle_get_arm_thread_state(thread_state_t            tstate,
    mach_msg_type_number_t *  count,
    const arm_saved_state_t * saved_state)
{
	/* In an arm64 world, this flavor can be used to retrieve the thread
	 * state of a 32-bit or 64-bit thread into a unified structure, but we
	 * need to support legacy clients who are only aware of 32-bit, so
	 * check the count to see what the client is expecting.
	 */
	if (*count < ARM_UNIFIED_THREAD_STATE_COUNT) {
		return handle_get_arm32_thread_state(tstate, count, saved_state);
	}

	arm_unified_thread_state_t *unified_state = (arm_unified_thread_state_t *) tstate;
	bzero(unified_state, sizeof(*unified_state));
#if __arm64__
	if (is_saved_state64(saved_state)) {
		unified_state->ash.flavor = ARM_THREAD_STATE64;
		unified_state->ash.count = ARM_THREAD_STATE64_COUNT;
		(void)saved_state_to_thread_state64(saved_state, thread_state64(unified_state));
	} else
#endif
	{
		unified_state->ash.flavor = ARM_THREAD_STATE32;
		unified_state->ash.count = ARM_THREAD_STATE32_COUNT;
		(void)saved_state_to_thread_state32(saved_state, thread_state32(unified_state));
	}
	*count = ARM_UNIFIED_THREAD_STATE_COUNT;
	return KERN_SUCCESS;
}


static kern_return_t
handle_set_arm32_thread_state(const thread_state_t   tstate,
    mach_msg_type_number_t count,
    arm_saved_state_t *    saved_state)
{
	if (count != ARM_THREAD_STATE32_COUNT) {
		return KERN_INVALID_ARGUMENT;
	}

	(void)thread_state32_to_saved_state((const arm_thread_state32_t *)tstate, saved_state);
	return KERN_SUCCESS;
}

static kern_return_t
handle_set_arm64_thread_state(const thread_state_t   tstate,
    mach_msg_type_number_t count,
    arm_saved_state_t *    saved_state)
{
	if (count != ARM_THREAD_STATE64_COUNT) {
		return KERN_INVALID_ARGUMENT;
	}

	(void)thread_state64_to_saved_state((const arm_thread_state64_t *)tstate, saved_state);
	return KERN_SUCCESS;
}


static kern_return_t
handle_set_arm_thread_state(const thread_state_t   tstate,
    mach_msg_type_number_t count,
    arm_saved_state_t *    saved_state)
{
	/* In an arm64 world, this flavor can be used to set the thread state of a
	 * 32-bit or 64-bit thread from a unified structure, but we need to support
	 * legacy clients who are only aware of 32-bit, so check the count to see
	 * what the client is expecting.
	 */
	if (count < ARM_UNIFIED_THREAD_STATE_COUNT) {
		if (!is_saved_state32(saved_state)) {
			return KERN_INVALID_ARGUMENT;
		}
		return handle_set_arm32_thread_state(tstate, count, saved_state);
	}

	const arm_unified_thread_state_t *unified_state = (const arm_unified_thread_state_t *) tstate;
#if __arm64__
	if (is_thread_state64(unified_state)) {
		if (!is_saved_state64(saved_state)) {
			return KERN_INVALID_ARGUMENT;
		}
		(void)thread_state64_to_saved_state(const_thread_state64(unified_state), saved_state);
	} else
#endif
	{
		if (!is_saved_state32(saved_state)) {
			return KERN_INVALID_ARGUMENT;
		}
		(void)thread_state32_to_saved_state(const_thread_state32(unified_state), saved_state);
	}

	return KERN_SUCCESS;
}


#if __has_feature(ptrauth_calls)

static inline uint32_t
thread_generate_sigreturn_token(
	void *ptr,
	thread_t thread)
{
	user64_addr_t token = (user64_addr_t)ptr;
	token ^= (user64_addr_t)thread_get_sigreturn_token(thread);
	token = (user64_addr_t)pmap_sign_user_ptr((void*)token,
	    ptrauth_key_process_independent_data, ptrauth_string_discriminator("nonce"),
	    thread->machine.jop_pid);
	token >>= 32;
	return (uint32_t)token;
}
#endif //__has_feature(ptrauth_calls)

/*
 * Translate thread state arguments to userspace representation
 */

kern_return_t
machine_thread_state_convert_to_user(
	thread_t thread,
	thread_flavor_t flavor,
	thread_state_t tstate,
	mach_msg_type_number_t *count,
	thread_set_status_flags_t tssf_flags)
{
#if __has_feature(ptrauth_calls)
	arm_thread_state64_t *ts64;
	bool preserve_flags = !!(tssf_flags & TSSF_PRESERVE_FLAGS);
	bool stash_sigreturn_token = !!(tssf_flags & TSSF_STASH_SIGRETURN_TOKEN);
	bool random_div = !!(tssf_flags & TSSF_RANDOM_USER_DIV);
	bool thread_div = !!(tssf_flags & TSSF_THREAD_USER_DIV);
	bool task_div = !!(tssf_flags & TSSF_TASK_USER_DIV);
	uint32_t old_flags;
	bool kernel_signed_pc = true;
	bool kernel_signed_lr = true;
	uint32_t userland_diversifier = 0;

	switch (flavor) {
	case ARM_THREAD_STATE:
	{
		arm_unified_thread_state_t *unified_state = (arm_unified_thread_state_t *)tstate;

		if (*count < ARM_UNIFIED_THREAD_STATE_COUNT || !is_thread_state64(unified_state)) {
			return KERN_SUCCESS;
		}
		ts64 = thread_state64(unified_state);
		break;
	}
	case ARM_THREAD_STATE64:
	{
		if (*count < ARM_THREAD_STATE64_COUNT) {
			return KERN_SUCCESS;
		}
		ts64 = (arm_thread_state64_t *)tstate;
		break;
	}
	default:
		return KERN_SUCCESS;
	}

	// Note that kernel threads never have disable_user_jop set
	if ((current_thread()->machine.arm_machine_flags & ARM_MACHINE_THREAD_DISABLE_USER_JOP) ||
	    !thread_is_64bit_addr(current_thread()) ||
	    (thread->machine.arm_machine_flags & ARM_MACHINE_THREAD_DISABLE_USER_JOP) || !thread_is_64bit_addr(thread)
	    ) {
		ts64->flags = __DARWIN_ARM_THREAD_STATE64_FLAGS_NO_PTRAUTH;
		return KERN_SUCCESS;
	}

	old_flags = ts64->flags;
	ts64->flags = 0;
	if (ts64->lr) {
		// lr might contain an IB-signed return address (strip is a no-op on unsigned addresses)
		uintptr_t stripped_lr = (uintptr_t)ptrauth_strip((void *)ts64->lr,
		    ptrauth_key_return_address);
		if (ts64->lr != stripped_lr) {
			// Need to allow already-signed lr value to round-trip as is
			ts64->flags |= __DARWIN_ARM_THREAD_STATE64_FLAGS_IB_SIGNED_LR;
		}
		// Note that an IB-signed return address that happens to have a 0 signature value
		// will round-trip correctly even if IA-signed again below (and IA-authd later)
	}

	if (arm_user_jop_disabled()) {
		return KERN_SUCCESS;
	}

	if (preserve_flags) {
		assert(random_div == false);
		assert(thread_div == false);

		/* Restore the diversifier and other opaque flags */
		ts64->flags |= (old_flags & __DARWIN_ARM_THREAD_STATE64_USER_DIVERSIFIER_MASK);
		userland_diversifier = old_flags & __DARWIN_ARM_THREAD_STATE64_USER_DIVERSIFIER_MASK;
		if (!(old_flags & __DARWIN_ARM_THREAD_STATE64_FLAGS_KERNEL_SIGNED_PC)) {
			kernel_signed_pc = false;
		}
		if (!(old_flags & __DARWIN_ARM_THREAD_STATE64_FLAGS_KERNEL_SIGNED_LR)) {
			kernel_signed_lr = false;
		}
	} else {
		/* Set a non zero userland diversifier */
		if (random_div || task_div) {
			/* Still use random div in case of task_div to avoid leaking the secret key */
			do {
				read_random(&userland_diversifier, sizeof(userland_diversifier));
				userland_diversifier &=
				    __DARWIN_ARM_THREAD_STATE64_USER_DIVERSIFIER_MASK;
			} while (userland_diversifier == 0);
		} else if (thread_div) {
			userland_diversifier = thread_get_sigreturn_diversifier(thread) &
			    __DARWIN_ARM_THREAD_STATE64_USER_DIVERSIFIER_MASK;
		}
		ts64->flags |= userland_diversifier;
	}

	if (kernel_signed_pc) {
		ts64->flags |= __DARWIN_ARM_THREAD_STATE64_FLAGS_KERNEL_SIGNED_PC;
	}

	if (kernel_signed_lr) {
		ts64->flags |= __DARWIN_ARM_THREAD_STATE64_FLAGS_KERNEL_SIGNED_LR;
	}


	if (ts64->pc) {
		uint64_t discriminator = ptrauth_string_discriminator("pc");
		if (!kernel_signed_pc && userland_diversifier != 0) {
			discriminator = ptrauth_blend_discriminator((void *)(long)userland_diversifier,
			    ptrauth_string_discriminator("pc"));
		}

		ts64->pc = (uintptr_t)pmap_sign_user_ptr((void*)ts64->pc,
		    ptrauth_key_process_independent_code, discriminator,
		    thread->machine.jop_pid);
	}
	if (ts64->lr && !(ts64->flags & __DARWIN_ARM_THREAD_STATE64_FLAGS_IB_SIGNED_LR)) {
		uint64_t discriminator = ptrauth_string_discriminator("lr");
		if (!kernel_signed_lr && userland_diversifier != 0) {
			discriminator = ptrauth_blend_discriminator((void *)(long)userland_diversifier,
			    ptrauth_string_discriminator("lr"));
		}

		ts64->lr = (uintptr_t)pmap_sign_user_ptr((void*)ts64->lr,
		    ptrauth_key_process_independent_code, discriminator,
		    thread->machine.jop_pid);
	}
	if (ts64->sp) {
		ts64->sp = (uintptr_t)pmap_sign_user_ptr((void*)ts64->sp,
		    ptrauth_key_process_independent_data, ptrauth_string_discriminator("sp"),
		    thread->machine.jop_pid);
	}
	if (ts64->fp) {
		ts64->fp = (uintptr_t)pmap_sign_user_ptr((void*)ts64->fp,
		    ptrauth_key_process_independent_data, ptrauth_string_discriminator("fp"),
		    thread->machine.jop_pid);
	}

	/* Stash the sigreturn token */
	if (stash_sigreturn_token) {
		if (kernel_signed_pc) {
			uint32_t token = thread_generate_sigreturn_token((void *)ts64->pc, thread);
			__DARWIN_ARM_THREAD_STATE64_SET_SIGRETURN_TOKEN(ts64, token,
			    __DARWIN_ARM_THREAD_STATE64_SIGRETURN_PC_MASK);
		}

		if (kernel_signed_lr) {
			uint32_t token = thread_generate_sigreturn_token((void *)ts64->lr, thread);
			__DARWIN_ARM_THREAD_STATE64_SET_SIGRETURN_TOKEN(ts64, token,
			    __DARWIN_ARM_THREAD_STATE64_SIGRETURN_LR_MASK);
		}
	}

	return KERN_SUCCESS;
#else
	// No conversion to userspace representation on this platform
	(void)thread; (void)flavor; (void)tstate; (void)count; (void)tssf_flags;
	return KERN_SUCCESS;
#endif /* __has_feature(ptrauth_calls) */
}

#if __has_feature(ptrauth_calls)
extern char *   proc_name_address(void *p);

CA_EVENT(pac_thread_state_exception_event,
    CA_STATIC_STRING(CA_PROCNAME_LEN), proc_name);

static void
machine_thread_state_check_pac_state(
	arm_thread_state64_t *ts64,
	arm_thread_state64_t *old_ts64)
{
	bool send_event = false;
	task_t task = current_task();
	void *proc = get_bsdtask_info(task);
	char *proc_name = (char *) "unknown";

	if (((ts64->flags & __DARWIN_ARM_THREAD_STATE64_FLAGS_KERNEL_SIGNED_PC) &&
	    ts64->pc != old_ts64->pc) || (!(ts64->flags & __DARWIN_ARM_THREAD_STATE64_FLAGS_IB_SIGNED_LR) &&
	    (ts64->flags & __DARWIN_ARM_THREAD_STATE64_FLAGS_KERNEL_SIGNED_LR) && (ts64->lr != old_ts64->lr ||
	    (old_ts64->flags & __DARWIN_ARM_THREAD_STATE64_FLAGS_IB_SIGNED_LR)))) {
		send_event = true;
	}

	if (!send_event) {
		return;
	}

	proc_name = proc_name_address(proc);
	ca_event_t ca_event = CA_EVENT_ALLOCATE(pac_thread_state_exception_event);
	CA_EVENT_TYPE(pac_thread_state_exception_event) * pexc_event = ca_event->data;
	strlcpy(pexc_event->proc_name, proc_name, CA_PROCNAME_LEN);
	CA_EVENT_SEND(ca_event);
}

CA_EVENT(pac_thread_state_sigreturn_event,
    CA_STATIC_STRING(CA_PROCNAME_LEN), proc_name);

static bool
machine_thread_state_check_sigreturn_token(
	arm_thread_state64_t *ts64,
	thread_t thread)
{
	task_t task = current_task();
	void *proc = get_bsdtask_info(task);
	char *proc_name = (char *) "unknown";
	bool token_matched = true;
	bool kernel_signed_pc = !!(ts64->flags & __DARWIN_ARM_THREAD_STATE64_FLAGS_KERNEL_SIGNED_PC);
	bool kernel_signed_lr = !!(ts64->flags & __DARWIN_ARM_THREAD_STATE64_FLAGS_KERNEL_SIGNED_LR);

	if (kernel_signed_pc) {
		/* Compute the sigreturn token */
		uint32_t token = thread_generate_sigreturn_token((void *)ts64->pc, thread);
		if (!__DARWIN_ARM_THREAD_STATE64_CHECK_SIGRETURN_TOKEN(ts64, token,
		    __DARWIN_ARM_THREAD_STATE64_SIGRETURN_PC_MASK)) {
			token_matched = false;
		}
	}

	if (kernel_signed_lr) {
		/* Compute the sigreturn token */
		uint32_t token = thread_generate_sigreturn_token((void *)ts64->lr, thread);
		if (!__DARWIN_ARM_THREAD_STATE64_CHECK_SIGRETURN_TOKEN(ts64, token,
		    __DARWIN_ARM_THREAD_STATE64_SIGRETURN_LR_MASK)) {
			token_matched = false;
		}
	}

	if (token_matched) {
		return true;
	}

	proc_name = proc_name_address(proc);
	ca_event_t ca_event = CA_EVENT_ALLOCATE(pac_thread_state_sigreturn_event);
	CA_EVENT_TYPE(pac_thread_state_sigreturn_event) * psig_event = ca_event->data;
	strlcpy(psig_event->proc_name, proc_name, CA_PROCNAME_LEN);
	CA_EVENT_SEND(ca_event);
	return false;
}

#endif

/*
 * Translate thread state arguments from userspace representation
 */

kern_return_t
machine_thread_state_convert_from_user(
	thread_t thread,
	thread_flavor_t flavor,
	thread_state_t tstate,
	mach_msg_type_number_t count,
	thread_state_t old_tstate,
	mach_msg_type_number_t old_count,
	thread_set_status_flags_t tssf_flags)
{
	arm_thread_state64_t *ts64;
	arm_thread_state64_t *old_ts64 = NULL;
	bool only_set_pc = !!(tssf_flags & TSSF_ONLY_PC);

	switch (flavor) {
	case ARM_THREAD_STATE:
	{
		arm_unified_thread_state_t *unified_state = (arm_unified_thread_state_t *)tstate;

		if (count < ARM_UNIFIED_THREAD_STATE_COUNT || !is_thread_state64(unified_state)) {
			return KERN_SUCCESS;
		}
		ts64 = thread_state64(unified_state);

		arm_unified_thread_state_t *old_unified_state = (arm_unified_thread_state_t *)old_tstate;
		if (old_unified_state && old_count >= ARM_UNIFIED_THREAD_STATE_COUNT) {
			old_ts64 = thread_state64(old_unified_state);
		}
		break;
	}
	case ARM_THREAD_STATE64:
	{
		if (count != ARM_THREAD_STATE64_COUNT) {
			return KERN_SUCCESS;
		}
		ts64 = (arm_thread_state64_t *)tstate;

		if (old_count == ARM_THREAD_STATE64_COUNT) {
			old_ts64 = (arm_thread_state64_t *)old_tstate;
		}
		break;
	}
	default:
		return KERN_SUCCESS;
	}

	if (only_set_pc) {
		uint64_t new_pc = ts64->pc;
		uint64_t new_flags = ts64->flags;
		/* Only allow pc to be modified in new_state */
		memcpy(ts64, old_ts64, sizeof(arm_thread_state64_t));
		ts64->pc = new_pc;
		ts64->flags = new_flags;
	}

#if __has_feature(ptrauth_calls)

	void *userland_diversifier = NULL;
	bool kernel_signed_pc;
	bool kernel_signed_lr;
	bool random_div = !!(tssf_flags & TSSF_RANDOM_USER_DIV);
	bool thread_div = !!(tssf_flags & TSSF_THREAD_USER_DIV);
	bool task_div = !!(tssf_flags & TSSF_TASK_USER_DIV);

	// Note that kernel threads never have disable_user_jop set
	if ((current_thread()->machine.arm_machine_flags & ARM_MACHINE_THREAD_DISABLE_USER_JOP) ||
	    !thread_is_64bit_addr(current_thread())) {
		if ((thread->machine.arm_machine_flags & ARM_MACHINE_THREAD_DISABLE_USER_JOP) ||
		    !thread_is_64bit_addr(thread)) {
			ts64->flags = __DARWIN_ARM_THREAD_STATE64_FLAGS_NO_PTRAUTH;
			return KERN_SUCCESS;
		}
		// A JOP-disabled process must not set thread state on a JOP-enabled process
		return KERN_PROTECTION_FAILURE;
	}

	if (ts64->flags & __DARWIN_ARM_THREAD_STATE64_FLAGS_NO_PTRAUTH) {
		if ((thread->machine.arm_machine_flags & ARM_MACHINE_THREAD_DISABLE_USER_JOP) ||
		    !thread_is_64bit_addr(thread)
		    ) {
			return KERN_SUCCESS;
		}
		// Disallow setting unsigned thread state on JOP-enabled processes.
		// Ignore flag and treat thread state arguments as signed, ptrauth
		// poisoning will cause resulting thread state to be invalid
		ts64->flags &= ~__DARWIN_ARM_THREAD_STATE64_FLAGS_NO_PTRAUTH;
	}

	if (ts64->flags & __DARWIN_ARM_THREAD_STATE64_FLAGS_IB_SIGNED_LR) {
		// lr might contain an IB-signed return address (strip is a no-op on unsigned addresses)
		uintptr_t stripped_lr = (uintptr_t)ptrauth_strip((void *)ts64->lr,
		    ptrauth_key_return_address);
		if (ts64->lr == stripped_lr) {
			// Don't allow unsigned pointer to be passed through as is. Ignore flag and
			// treat as IA-signed below (where auth failure may poison the value).
			ts64->flags &= ~__DARWIN_ARM_THREAD_STATE64_FLAGS_IB_SIGNED_LR;
		}
		// Note that an IB-signed return address that happens to have a 0 signature value
		// will also have been IA-signed (without this flag being set) and so will IA-auth
		// correctly below.
	}

	if (arm_user_jop_disabled()) {
		return KERN_SUCCESS;
	}

	kernel_signed_pc = !!(ts64->flags & __DARWIN_ARM_THREAD_STATE64_FLAGS_KERNEL_SIGNED_PC);
	kernel_signed_lr = !!(ts64->flags & __DARWIN_ARM_THREAD_STATE64_FLAGS_KERNEL_SIGNED_LR);
	/*
	 * Replace pc/lr with old state if allow only
	 * user ptr flag is passed and ptrs are marked
	 * kernel signed.
	 */
	if ((tssf_flags & TSSF_CHECK_USER_FLAGS) &&
	    (kernel_signed_pc || kernel_signed_lr)) {
		if (old_ts64 && old_count == count) {
			/* Send a CA event if the thread state does not match */
			machine_thread_state_check_pac_state(ts64, old_ts64);

			/* Check if user ptrs needs to be replaced */
			if ((tssf_flags & TSSF_ALLOW_ONLY_USER_PTRS) &&
			    kernel_signed_pc) {
				ts64->pc = old_ts64->pc;
			}

			if ((tssf_flags & TSSF_ALLOW_ONLY_USER_PTRS) &&
			    !(ts64->flags & __DARWIN_ARM_THREAD_STATE64_FLAGS_IB_SIGNED_LR) &&
			    kernel_signed_lr) {
				ts64->lr = old_ts64->lr;
				if (old_ts64->flags & __DARWIN_ARM_THREAD_STATE64_FLAGS_IB_SIGNED_LR) {
					ts64->flags |= __DARWIN_ARM_THREAD_STATE64_FLAGS_IB_SIGNED_LR;
				} else {
					ts64->flags &= ~__DARWIN_ARM_THREAD_STATE64_FLAGS_IB_SIGNED_LR;
				}
			}
		}
	}

	/* Validate sigreturn token */
	if (tssf_flags & TSSF_CHECK_SIGRETURN_TOKEN) {
		bool token_matched = machine_thread_state_check_sigreturn_token(ts64, thread);
		if ((tssf_flags & TSSF_ALLOW_ONLY_MATCHING_TOKEN) && !token_matched) {
			return KERN_PROTECTION_FAILURE;
		}
	}

	/* Get the userland diversifier */
	if (random_div && old_ts64 && old_count == count) {
		/* Get the random diversifier from the old thread state */
		userland_diversifier = (void *)(long)(old_ts64->flags &
		    __DARWIN_ARM_THREAD_STATE64_USER_DIVERSIFIER_MASK);
	} else if (thread_div) {
		userland_diversifier = (void *)(long)(thread_get_sigreturn_diversifier(thread) &
		    __DARWIN_ARM_THREAD_STATE64_USER_DIVERSIFIER_MASK);
	} else if (task_div) {
		userland_diversifier =
		    (void *)(long)((get_threadtask(thread)->hardened_exception_action.signed_pc_key) &
		    __DARWIN_ARM_THREAD_STATE64_USER_DIVERSIFIER_MASK);
	}

	if (ts64->pc) {
		uint64_t discriminator = ptrauth_string_discriminator("pc");
		if (!kernel_signed_pc && userland_diversifier != 0) {
			discriminator = ptrauth_blend_discriminator(userland_diversifier,
			    ptrauth_string_discriminator("pc"));
		}
		ts64->pc = (uintptr_t)pmap_auth_user_ptr((void*)ts64->pc,
		    ptrauth_key_process_independent_code, discriminator,
		    thread->machine.jop_pid);
	}
	if (ts64->lr && !(ts64->flags & __DARWIN_ARM_THREAD_STATE64_FLAGS_IB_SIGNED_LR)) {
		uint64_t discriminator = ptrauth_string_discriminator("lr");
		if (!kernel_signed_lr && userland_diversifier != 0) {
			discriminator = ptrauth_blend_discriminator(userland_diversifier,
			    ptrauth_string_discriminator("lr"));
		}
		ts64->lr = (uintptr_t)pmap_auth_user_ptr((void*)ts64->lr,
		    ptrauth_key_process_independent_code, discriminator,
		    thread->machine.jop_pid);
	}
	if (ts64->sp) {
		ts64->sp = (uintptr_t)pmap_auth_user_ptr((void*)ts64->sp,
		    ptrauth_key_process_independent_data, ptrauth_string_discriminator("sp"),
		    thread->machine.jop_pid);
	}
	if (ts64->fp) {
		ts64->fp = (uintptr_t)pmap_auth_user_ptr((void*)ts64->fp,
		    ptrauth_key_process_independent_data, ptrauth_string_discriminator("fp"),
		    thread->machine.jop_pid);
	}

	return KERN_SUCCESS;
#else
	// No conversion from userspace representation on this platform
	(void)thread; (void)flavor; (void)tstate; (void)count;
	(void)old_tstate; (void)old_count; (void)tssf_flags;
	return KERN_SUCCESS;
#endif /* __has_feature(ptrauth_calls) */
}

#if __has_feature(ptrauth_calls)
bool
machine_thread_state_is_debug_flavor(int flavor)
{
	if (flavor == ARM_DEBUG_STATE ||
	    flavor == ARM_DEBUG_STATE64 ||
	    flavor == ARM_DEBUG_STATE32) {
		return true;
	}
	return false;
}
#endif /* __has_feature(ptrauth_calls) */

/*
 * Translate signal context data pointer to userspace representation
 */

kern_return_t
machine_thread_siguctx_pointer_convert_to_user(
	thread_t thread,
	user_addr_t *uctxp)
{
#if __has_feature(ptrauth_calls)
	if ((current_thread()->machine.arm_machine_flags & ARM_MACHINE_THREAD_DISABLE_USER_JOP) ||
	    !thread_is_64bit_addr(current_thread())) {
		assert((thread->machine.arm_machine_flags & ARM_MACHINE_THREAD_DISABLE_USER_JOP) || !thread_is_64bit_addr(thread));
		return KERN_SUCCESS;
	}

	if (arm_user_jop_disabled()) {
		return KERN_SUCCESS;
	}

	if (*uctxp) {
		*uctxp = (uintptr_t)pmap_sign_user_ptr((void*)*uctxp,
		    ptrauth_key_process_independent_data, ptrauth_string_discriminator("uctx"),
		    thread->machine.jop_pid);
	}

	return KERN_SUCCESS;
#else
	// No conversion to userspace representation on this platform
	(void)thread; (void)uctxp;
	return KERN_SUCCESS;
#endif /* __has_feature(ptrauth_calls) */
}

/*
 * Translate array of function pointer syscall arguments from userspace representation
 */

kern_return_t
machine_thread_function_pointers_convert_from_user(
	thread_t thread,
	user_addr_t *fptrs,
	uint32_t count)
{
#if __has_feature(ptrauth_calls)
	if ((current_thread()->machine.arm_machine_flags & ARM_MACHINE_THREAD_DISABLE_USER_JOP) ||
	    !thread_is_64bit_addr(current_thread())) {
		assert((thread->machine.arm_machine_flags & ARM_MACHINE_THREAD_DISABLE_USER_JOP) ||
		    !thread_is_64bit_addr(thread));
		return KERN_SUCCESS;
	}

	if (arm_user_jop_disabled()) {
		return KERN_SUCCESS;
	}

	while (count--) {
		if (*fptrs) {
			*fptrs = (uintptr_t)pmap_auth_user_ptr((void*)*fptrs,
			    ptrauth_key_function_pointer, 0, thread->machine.jop_pid);
		}
		fptrs++;
	}

	return KERN_SUCCESS;
#else
	// No conversion from userspace representation on this platform
	(void)thread; (void)fptrs; (void)count;
	return KERN_SUCCESS;
#endif /* __has_feature(ptrauth_calls) */
}

/*
 * Routine: machine_thread_get_state
 *
 */
kern_return_t
machine_thread_get_state(thread_t                 thread,
    thread_flavor_t          flavor,
    thread_state_t           tstate,
    mach_msg_type_number_t * count)
{
	switch (flavor) {
	case THREAD_STATE_FLAVOR_LIST:
		if (*count < 4) {
			return KERN_INVALID_ARGUMENT;
		}

		tstate[0] = ARM_THREAD_STATE;
		tstate[1] = ARM_VFP_STATE;
		tstate[2] = ARM_EXCEPTION_STATE;
		tstate[3] = ARM_DEBUG_STATE;
		*count = 4;
		break;

	case THREAD_STATE_FLAVOR_LIST_NEW:
		if (*count < 4) {
			return KERN_INVALID_ARGUMENT;
		}

		tstate[0] = ARM_THREAD_STATE;
		tstate[1] = ARM_VFP_STATE;
		tstate[2] = thread_is_64bit_data(thread) ? ARM_EXCEPTION_STATE64 : ARM_EXCEPTION_STATE;
		tstate[3] = thread_is_64bit_data(thread) ? ARM_DEBUG_STATE64 : ARM_DEBUG_STATE32;
		*count = 4;
		break;

	case THREAD_STATE_FLAVOR_LIST_10_15:
		if (*count < 5) {
			return KERN_INVALID_ARGUMENT;
		}

		tstate[0] = ARM_THREAD_STATE;
		tstate[1] = ARM_VFP_STATE;
		tstate[2] = thread_is_64bit_data(thread) ? ARM_EXCEPTION_STATE64 : ARM_EXCEPTION_STATE;
		tstate[3] = thread_is_64bit_data(thread) ? ARM_DEBUG_STATE64 : ARM_DEBUG_STATE32;
		tstate[4] = ARM_PAGEIN_STATE;
		*count = 5;
		break;

	case ARM_THREAD_STATE:
	{
		kern_return_t rn = handle_get_arm_thread_state(tstate, count, thread->machine.upcb);
		if (rn) {
			return rn;
		}
		break;
	}
	case ARM_THREAD_STATE32:
	{
		if (thread_is_64bit_data(thread)) {
			return KERN_INVALID_ARGUMENT;
		}

		kern_return_t rn = handle_get_arm32_thread_state(tstate, count, thread->machine.upcb);
		if (rn) {
			return rn;
		}
		break;
	}
#if __arm64__
	case ARM_THREAD_STATE64:
	{
		if (!thread_is_64bit_data(thread)) {
			return KERN_INVALID_ARGUMENT;
		}

		kern_return_t rn = handle_get_arm64_thread_state(tstate, count, thread->machine.upcb);
		if (rn) {
			return rn;
		}

		break;
	}
#endif
	case ARM_EXCEPTION_STATE:{
		struct arm_exception_state *state;
		struct arm_saved_state32 *saved_state;

		if (*count < ARM_EXCEPTION_STATE_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}
		if (thread_is_64bit_data(thread)) {
			return KERN_INVALID_ARGUMENT;
		}

		state = (struct arm_exception_state *) tstate;
		saved_state = saved_state32(thread->machine.upcb);

		state->exception = saved_state->exception;
		state->fsr = (uint32_t) saved_state->esr;
		state->far = saved_state->far;

		*count = ARM_EXCEPTION_STATE_COUNT;
		break;
	}
	case ARM_EXCEPTION_STATE64:{
		struct arm_exception_state64 *state;
		struct arm_saved_state64 *saved_state;

		if (*count < ARM_EXCEPTION_STATE64_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}
		if (!thread_is_64bit_data(thread)) {
			return KERN_INVALID_ARGUMENT;
		}

		state = (struct arm_exception_state64 *) tstate;
		saved_state = saved_state64(thread->machine.upcb);

		state->exception = 0;
		state->far = saved_state->far;
		state->esr = (uint32_t) saved_state->esr;

		*count = ARM_EXCEPTION_STATE64_COUNT;
		break;
	}
	case ARM_EXCEPTION_STATE64_V2:{
		struct arm_exception_state64_v2 *state;
		struct arm_saved_state64 *saved_state;

		if (*count < ARM_EXCEPTION_STATE64_V2_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}
		if (!thread_is_64bit_data(thread)) {
			return KERN_INVALID_ARGUMENT;
		}

		state = (struct arm_exception_state64_v2 *) tstate;
		saved_state = saved_state64(thread->machine.upcb);

		state->far = saved_state->far;
		state->esr = saved_state->esr;

		*count = ARM_EXCEPTION_STATE64_V2_COUNT;
		break;
	}
	case ARM_DEBUG_STATE:{
		arm_legacy_debug_state_t *state;
		arm_debug_state32_t *thread_state;

		if (*count < ARM_LEGACY_DEBUG_STATE_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}

		if (thread_is_64bit_data(thread)) {
			return KERN_INVALID_ARGUMENT;
		}

		state = (arm_legacy_debug_state_t *) tstate;
		thread_state = find_debug_state32(thread);

		if (thread_state == NULL) {
			bzero(state, sizeof(arm_legacy_debug_state_t));
		} else {
			bcopy(thread_state, state, sizeof(arm_legacy_debug_state_t));
		}

		*count = ARM_LEGACY_DEBUG_STATE_COUNT;
		break;
	}
	case ARM_DEBUG_STATE32:{
		arm_debug_state32_t *state;
		arm_debug_state32_t *thread_state;

		if (*count < ARM_DEBUG_STATE32_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}

		if (thread_is_64bit_data(thread)) {
			return KERN_INVALID_ARGUMENT;
		}

		state = (arm_debug_state32_t *) tstate;
		thread_state = find_debug_state32(thread);

		if (thread_state == NULL) {
			bzero(state, sizeof(arm_debug_state32_t));
		} else {
			bcopy(thread_state, state, sizeof(arm_debug_state32_t));
		}

		*count = ARM_DEBUG_STATE32_COUNT;
		break;
	}

	case ARM_DEBUG_STATE64:{
		arm_debug_state64_t *state;
		arm_debug_state64_t *thread_state;

		if (*count < ARM_DEBUG_STATE64_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}

		if (!thread_is_64bit_data(thread)) {
			return KERN_INVALID_ARGUMENT;
		}

		state = (arm_debug_state64_t *) tstate;
		thread_state = find_debug_state64(thread);

		if (thread_state == NULL) {
			bzero(state, sizeof(arm_debug_state64_t));
		} else {
			bcopy(thread_state, state, sizeof(arm_debug_state64_t));
		}

		*count = ARM_DEBUG_STATE64_COUNT;
		break;
	}

	case ARM_VFP_STATE:{
		struct arm_vfp_state *state;
		arm_neon_saved_state32_t *thread_state;
		unsigned int max;

		if (*count < ARM_VFP_STATE_COUNT) {
			if (*count < ARM_VFPV2_STATE_COUNT) {
				return KERN_INVALID_ARGUMENT;
			} else {
				*count =  ARM_VFPV2_STATE_COUNT;
			}
		}

		if (*count == ARM_VFPV2_STATE_COUNT) {
			max = 32;
		} else {
			max = 64;
		}

		state = (struct arm_vfp_state *) tstate;
		thread_state = neon_state32(thread->machine.uNeon);
		/* ARM64 TODO: set fpsr and fpcr from state->fpscr */

		bcopy(thread_state, state, (max + 1) * sizeof(uint32_t));
		*count = (max + 1);
		break;
	}
	case ARM_NEON_STATE:{
		arm_neon_state_t *state;
		arm_neon_saved_state32_t *thread_state;

		if (*count < ARM_NEON_STATE_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}

		if (thread_is_64bit_data(thread)) {
			return KERN_INVALID_ARGUMENT;
		}

		state = (arm_neon_state_t *)tstate;
		thread_state = neon_state32(thread->machine.uNeon);

		assert(sizeof(*thread_state) == sizeof(*state));
		bcopy(thread_state, state, sizeof(arm_neon_state_t));

		*count = ARM_NEON_STATE_COUNT;
		break;
	}

	case ARM_NEON_STATE64:{
		arm_neon_state64_t *state;
		arm_neon_saved_state64_t *thread_state;

		if (*count < ARM_NEON_STATE64_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}

		if (!thread_is_64bit_data(thread)) {
			return KERN_INVALID_ARGUMENT;
		}

		state = (arm_neon_state64_t *)tstate;
		thread_state = neon_state64(thread->machine.uNeon);

		/* For now, these are identical */
		assert(sizeof(*state) == sizeof(*thread_state));
		bcopy(thread_state, state, sizeof(arm_neon_state64_t));


		*count = ARM_NEON_STATE64_COUNT;
		break;
	}


	case ARM_PAGEIN_STATE: {
		arm_pagein_state_t *state;

		if (*count < ARM_PAGEIN_STATE_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}

		state = (arm_pagein_state_t *)tstate;
		state->__pagein_error = thread->t_pagein_error;

		*count = ARM_PAGEIN_STATE_COUNT;
		break;
	}


	default:
		return KERN_INVALID_ARGUMENT;
	}
	return KERN_SUCCESS;
}


/*
 * Routine: machine_thread_get_kern_state
 *
 */
kern_return_t
machine_thread_get_kern_state(thread_t                 thread,
    thread_flavor_t          flavor,
    thread_state_t           tstate,
    mach_msg_type_number_t * count)
{
	/*
	 * This works only for an interrupted kernel thread
	 */
	if (thread != current_thread() || getCpuDatap()->cpu_int_state == NULL) {
		return KERN_FAILURE;
	}

	switch (flavor) {
	case ARM_THREAD_STATE:
	{
		kern_return_t rn = handle_get_arm_thread_state(tstate, count, getCpuDatap()->cpu_int_state);
		if (rn) {
			return rn;
		}
		break;
	}
	case ARM_THREAD_STATE32:
	{
		kern_return_t rn = handle_get_arm32_thread_state(tstate, count, getCpuDatap()->cpu_int_state);
		if (rn) {
			return rn;
		}
		break;
	}
#if __arm64__
	case ARM_THREAD_STATE64:
	{
		kern_return_t rn = handle_get_arm64_thread_state(tstate, count, getCpuDatap()->cpu_int_state);
		if (rn) {
			return rn;
		}
		break;
	}
#endif
	default:
		return KERN_INVALID_ARGUMENT;
	}
	return KERN_SUCCESS;
}

void
machine_thread_switch_addrmode(thread_t thread)
{
	if (task_has_64Bit_data(get_threadtask(thread))) {
		thread->machine.upcb->ash.flavor = ARM_SAVED_STATE64;
		thread->machine.upcb->ash.count = ARM_SAVED_STATE64_COUNT;
		thread->machine.uNeon->nsh.flavor = ARM_NEON_SAVED_STATE64;
		thread->machine.uNeon->nsh.count = ARM_NEON_SAVED_STATE64_COUNT;

		/*
		 * Reinitialize the NEON state.
		 */
		bzero(&thread->machine.uNeon->uns, sizeof(thread->machine.uNeon->uns));
		thread->machine.uNeon->ns_64.fpcr = FPCR_DEFAULT;
	} else {
		thread->machine.upcb->ash.flavor = ARM_SAVED_STATE32;
		thread->machine.upcb->ash.count = ARM_SAVED_STATE32_COUNT;
		thread->machine.uNeon->nsh.flavor = ARM_NEON_SAVED_STATE32;
		thread->machine.uNeon->nsh.count = ARM_NEON_SAVED_STATE32_COUNT;

		/*
		 * Reinitialize the NEON state.
		 */
		bzero(&thread->machine.uNeon->uns, sizeof(thread->machine.uNeon->uns));
		thread->machine.uNeon->ns_32.fpcr = FPCR_DEFAULT_32;
	}
}

extern long long arm_debug_get(void);

/*
 * Routine: machine_thread_set_state
 *
 */
kern_return_t
machine_thread_set_state(thread_t               thread,
    thread_flavor_t        flavor,
    thread_state_t         tstate,
    mach_msg_type_number_t count)
{
	kern_return_t rn;

	switch (flavor) {
	case ARM_THREAD_STATE:
		rn = handle_set_arm_thread_state(tstate, count, thread->machine.upcb);
		if (rn) {
			return rn;
		}
		break;

	case ARM_THREAD_STATE32:
		if (thread_is_64bit_data(thread)) {
			return KERN_INVALID_ARGUMENT;
		}

		rn = handle_set_arm32_thread_state(tstate, count, thread->machine.upcb);
		if (rn) {
			return rn;
		}
		break;

#if __arm64__
	case ARM_THREAD_STATE64:
		if (!thread_is_64bit_data(thread)) {
			return KERN_INVALID_ARGUMENT;
		}


		rn = handle_set_arm64_thread_state(tstate, count, thread->machine.upcb);
		if (rn) {
			return rn;
		}
		break;
#endif
	case ARM_EXCEPTION_STATE:{
		if (count != ARM_EXCEPTION_STATE_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}
		if (thread_is_64bit_data(thread)) {
			return KERN_INVALID_ARGUMENT;
		}

		break;
	}
	case ARM_EXCEPTION_STATE64:{
		if (count != ARM_EXCEPTION_STATE64_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}
		if (!thread_is_64bit_data(thread)) {
			return KERN_INVALID_ARGUMENT;
		}

		break;
	}
	case ARM_EXCEPTION_STATE64_V2:{
		if (count != ARM_EXCEPTION_STATE64_V2_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}
		if (!thread_is_64bit_data(thread)) {
			return KERN_INVALID_ARGUMENT;
		}

		break;
	}
	case ARM_DEBUG_STATE:
	{
		arm_legacy_debug_state_t *state;
		boolean_t enabled = FALSE;
		unsigned int    i;

		if (count != ARM_LEGACY_DEBUG_STATE_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}
		if (thread_is_64bit_data(thread)) {
			return KERN_INVALID_ARGUMENT;
		}

		state = (arm_legacy_debug_state_t *) tstate;

		for (i = 0; i < 16; i++) {
			/* do not allow context IDs to be set */
			if (((state->bcr[i] & ARM_DBGBCR_TYPE_MASK) != ARM_DBGBCR_TYPE_IVA)
			    || ((state->bcr[i] & ARM_DBG_CR_LINKED_MASK) != ARM_DBG_CR_LINKED_UNLINKED)
			    || ((state->wcr[i] & ARM_DBGBCR_TYPE_MASK) != ARM_DBGBCR_TYPE_IVA)
			    || ((state->wcr[i] & ARM_DBG_CR_LINKED_MASK) != ARM_DBG_CR_LINKED_UNLINKED)) {
				return KERN_PROTECTION_FAILURE;
			}
			if ((((state->bcr[i] & ARM_DBG_CR_ENABLE_MASK) == ARM_DBG_CR_ENABLE_ENABLE))
			    || ((state->wcr[i] & ARM_DBG_CR_ENABLE_MASK) == ARM_DBG_CR_ENABLE_ENABLE)) {
				enabled = TRUE;
			}
		}

		if (!enabled) {
			free_debug_state(thread);
		} else {
			arm_debug_state32_t *thread_state = find_or_allocate_debug_state32(thread);

			if (thread_state == NULL) {
				return KERN_FAILURE;
			}

			for (i = 0; i < 16; i++) {
				/* set appropriate privilege; mask out unknown bits */
				thread_state->bcr[i] = (state->bcr[i] & (ARM_DBG_CR_ADDRESS_MASK_MASK
				    | ARM_DBGBCR_MATCH_MASK
				    | ARM_DBG_CR_BYTE_ADDRESS_SELECT_MASK
				    | ARM_DBG_CR_ENABLE_MASK))
				    | ARM_DBGBCR_TYPE_IVA
				    | ARM_DBG_CR_LINKED_UNLINKED
				    | ARM_DBG_CR_SECURITY_STATE_BOTH
				    | ARM_DBG_CR_MODE_CONTROL_USER;
				thread_state->bvr[i] = state->bvr[i] & ARM_DBG_VR_ADDRESS_MASK;
				thread_state->wcr[i] = (state->wcr[i] & (ARM_DBG_CR_ADDRESS_MASK_MASK
				    | ARM_DBGWCR_BYTE_ADDRESS_SELECT_MASK
				    | ARM_DBGWCR_ACCESS_CONTROL_MASK
				    | ARM_DBG_CR_ENABLE_MASK))
				    | ARM_DBG_CR_LINKED_UNLINKED
				    | ARM_DBG_CR_SECURITY_STATE_BOTH
				    | ARM_DBG_CR_MODE_CONTROL_USER;
				thread_state->wvr[i] = state->wvr[i] & ARM_DBG_VR_ADDRESS_MASK;
			}

			thread_state->mdscr_el1 = 0ULL;         // Legacy customers issuing ARM_DEBUG_STATE dont drive single stepping.
		}

		if (thread == current_thread()) {
			arm_debug_set32(thread->machine.DebugData);
		}

		break;
	}
	case ARM_DEBUG_STATE32:
		/* ARM64_TODO  subtle bcr/wcr semantic differences e.g. wcr and ARM_DBGBCR_TYPE_IVA */
	{
		arm_debug_state32_t *state;
		boolean_t enabled = FALSE;
		unsigned int    i;

		if (count != ARM_DEBUG_STATE32_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}
		if (thread_is_64bit_data(thread)) {
			return KERN_INVALID_ARGUMENT;
		}

		state = (arm_debug_state32_t *) tstate;

		if (state->mdscr_el1 & MDSCR_SS) {
			enabled = TRUE;
		}

		for (i = 0; i < 16; i++) {
			/* do not allow context IDs to be set */
			if (((state->bcr[i] & ARM_DBGBCR_TYPE_MASK) != ARM_DBGBCR_TYPE_IVA)
			    || ((state->bcr[i] & ARM_DBG_CR_LINKED_MASK) != ARM_DBG_CR_LINKED_UNLINKED)
			    || ((state->wcr[i] & ARM_DBGBCR_TYPE_MASK) != ARM_DBGBCR_TYPE_IVA)
			    || ((state->wcr[i] & ARM_DBG_CR_LINKED_MASK) != ARM_DBG_CR_LINKED_UNLINKED)) {
				return KERN_PROTECTION_FAILURE;
			}
			if ((((state->bcr[i] & ARM_DBG_CR_ENABLE_MASK) == ARM_DBG_CR_ENABLE_ENABLE))
			    || ((state->wcr[i] & ARM_DBG_CR_ENABLE_MASK) == ARM_DBG_CR_ENABLE_ENABLE)) {
				enabled = TRUE;
			}
		}

		if (!enabled) {
			free_debug_state(thread);
		} else {
			arm_debug_state32_t * thread_state = find_or_allocate_debug_state32(thread);

			if (thread_state == NULL) {
				return KERN_FAILURE;
			}

			if (state->mdscr_el1 & MDSCR_SS) {
				thread_state->mdscr_el1 |= MDSCR_SS;
			} else {
				thread_state->mdscr_el1 &= ~MDSCR_SS;
			}

			for (i = 0; i < 16; i++) {
				/* set appropriate privilege; mask out unknown bits */
				thread_state->bcr[i] = (state->bcr[i] & (ARM_DBG_CR_ADDRESS_MASK_MASK
				    | ARM_DBGBCR_MATCH_MASK
				    | ARM_DBG_CR_BYTE_ADDRESS_SELECT_MASK
				    | ARM_DBG_CR_ENABLE_MASK))
				    | ARM_DBGBCR_TYPE_IVA
				    | ARM_DBG_CR_LINKED_UNLINKED
				    | ARM_DBG_CR_SECURITY_STATE_BOTH
				    | ARM_DBG_CR_MODE_CONTROL_USER;
				thread_state->bvr[i] = state->bvr[i] & ARM_DBG_VR_ADDRESS_MASK;
				thread_state->wcr[i] = (state->wcr[i] & (ARM_DBG_CR_ADDRESS_MASK_MASK
				    | ARM_DBGWCR_BYTE_ADDRESS_SELECT_MASK
				    | ARM_DBGWCR_ACCESS_CONTROL_MASK
				    | ARM_DBG_CR_ENABLE_MASK))
				    | ARM_DBG_CR_LINKED_UNLINKED
				    | ARM_DBG_CR_SECURITY_STATE_BOTH
				    | ARM_DBG_CR_MODE_CONTROL_USER;
				thread_state->wvr[i] = state->wvr[i] & ARM_DBG_VR_ADDRESS_MASK;
			}
		}

		if (thread == current_thread()) {
			arm_debug_set32(thread->machine.DebugData);
		}

		break;
	}

	case ARM_DEBUG_STATE64:
	{
		arm_debug_state64_t *state;
		boolean_t enabled = FALSE;
		unsigned int i;

		if (count != ARM_DEBUG_STATE64_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}
		if (!thread_is_64bit_data(thread)) {
			return KERN_INVALID_ARGUMENT;
		}

		state = (arm_debug_state64_t *) tstate;

		if (state->mdscr_el1 & MDSCR_SS) {
			enabled = TRUE;
		}

		for (i = 0; i < 16; i++) {
			/* do not allow context IDs to be set */
			if (((state->bcr[i] & ARM_DBGBCR_TYPE_MASK) != ARM_DBGBCR_TYPE_IVA)
			    || ((state->bcr[i] & ARM_DBG_CR_LINKED_MASK) != ARM_DBG_CR_LINKED_UNLINKED)
			    || ((state->wcr[i] & ARM_DBG_CR_LINKED_MASK) != ARM_DBG_CR_LINKED_UNLINKED)) {
				return KERN_PROTECTION_FAILURE;
			}
			if ((((state->bcr[i] & ARM_DBG_CR_ENABLE_MASK) == ARM_DBG_CR_ENABLE_ENABLE))
			    || ((state->wcr[i] & ARM_DBG_CR_ENABLE_MASK) == ARM_DBG_CR_ENABLE_ENABLE)) {
				enabled = TRUE;
			}
		}

		if (!enabled) {
			free_debug_state(thread);
		} else {
			arm_debug_state64_t *thread_state = find_or_allocate_debug_state64(thread);

			if (thread_state == NULL) {
				return KERN_FAILURE;
			}

			if (state->mdscr_el1 & MDSCR_SS) {
				thread_state->mdscr_el1 |= MDSCR_SS;
			} else {
				thread_state->mdscr_el1 &= ~MDSCR_SS;
			}

			for (i = 0; i < 16; i++) {
				/* set appropriate privilege; mask out unknown bits */
				thread_state->bcr[i] = (state->bcr[i] & (0         /* Was ARM_DBG_CR_ADDRESS_MASK_MASK deprecated in v8 */
				    | 0                             /* Was ARM_DBGBCR_MATCH_MASK, ignored in AArch64 state */
				    | ARM_DBG_CR_BYTE_ADDRESS_SELECT_MASK
				    | ARM_DBG_CR_ENABLE_MASK))
				    | ARM_DBGBCR_TYPE_IVA
				    | ARM_DBG_CR_LINKED_UNLINKED
				    | ARM_DBG_CR_SECURITY_STATE_BOTH
				    | ARM_DBG_CR_MODE_CONTROL_USER;
				thread_state->bvr[i] = state->bvr[i] & ARM_DBG_VR_ADDRESS_MASK64;
				thread_state->wcr[i] = (state->wcr[i] & (ARM_DBG_CR_ADDRESS_MASK_MASK
				    | ARM_DBGWCR_BYTE_ADDRESS_SELECT_MASK
				    | ARM_DBGWCR_ACCESS_CONTROL_MASK
				    | ARM_DBG_CR_ENABLE_MASK))
				    | ARM_DBG_CR_LINKED_UNLINKED
				    | ARM_DBG_CR_SECURITY_STATE_BOTH
				    | ARM_DBG_CR_MODE_CONTROL_USER;
				thread_state->wvr[i] = state->wvr[i] & ARM_DBG_VR_ADDRESS_MASK64;
			}
		}

		if (thread == current_thread()) {
			arm_debug_set64(thread->machine.DebugData);
		}

		break;
	}

	case ARM_VFP_STATE:{
		struct arm_vfp_state *state;
		arm_neon_saved_state32_t *thread_state;
		unsigned int    max;

		if (count != ARM_VFP_STATE_COUNT && count != ARM_VFPV2_STATE_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}

		if (count == ARM_VFPV2_STATE_COUNT) {
			max = 32;
		} else {
			max = 64;
		}

		state = (struct arm_vfp_state *) tstate;
		thread_state = neon_state32(thread->machine.uNeon);
		/* ARM64 TODO: combine fpsr and fpcr into state->fpscr */

		bcopy(state, thread_state, (max + 1) * sizeof(uint32_t));

		thread->machine.uNeon->nsh.flavor = ARM_NEON_SAVED_STATE32;
		thread->machine.uNeon->nsh.count = ARM_NEON_SAVED_STATE32_COUNT;
		break;
	}

	case ARM_NEON_STATE:{
		arm_neon_state_t *state;
		arm_neon_saved_state32_t *thread_state;

		if (count != ARM_NEON_STATE_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}

		if (thread_is_64bit_data(thread)) {
			return KERN_INVALID_ARGUMENT;
		}

		state = (arm_neon_state_t *)tstate;
		thread_state = neon_state32(thread->machine.uNeon);

		assert(sizeof(*state) == sizeof(*thread_state));
		bcopy(state, thread_state, sizeof(arm_neon_state_t));

		thread->machine.uNeon->nsh.flavor = ARM_NEON_SAVED_STATE32;
		thread->machine.uNeon->nsh.count = ARM_NEON_SAVED_STATE32_COUNT;
		break;
	}

	case ARM_NEON_STATE64:{
		arm_neon_state64_t *state;
		arm_neon_saved_state64_t *thread_state;

		if (count != ARM_NEON_STATE64_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}

		if (!thread_is_64bit_data(thread)) {
			return KERN_INVALID_ARGUMENT;
		}

		state = (arm_neon_state64_t *)tstate;
		thread_state = neon_state64(thread->machine.uNeon);

		assert(sizeof(*state) == sizeof(*thread_state));
		bcopy(state, thread_state, sizeof(arm_neon_state64_t));


		thread->machine.uNeon->nsh.flavor = ARM_NEON_SAVED_STATE64;
		thread->machine.uNeon->nsh.count = ARM_NEON_SAVED_STATE64_COUNT;
		break;
	}


	default:
		return KERN_INVALID_ARGUMENT;
	}
	return KERN_SUCCESS;
}

mach_vm_address_t
machine_thread_pc(thread_t thread)
{
	struct arm_saved_state *ss = get_user_regs(thread);
	return (mach_vm_address_t)get_saved_state_pc(ss);
}

void
machine_thread_reset_pc(thread_t thread, mach_vm_address_t pc)
{
	set_user_saved_state_pc(get_user_regs(thread), (register_t)pc);
}

/*
 * Routine: machine_thread_state_initialize
 *
 */
void
machine_thread_state_initialize(thread_t thread)
{
	arm_context_t *context = thread->machine.contextData;

	/*
	 * Should always be set up later. For a kernel thread, we don't care
	 * about this state. For a user thread, we'll set the state up in
	 * setup_wqthread, bsdthread_create, load_main(), or load_unixthread().
	 */

	if (context != NULL) {
		bzero(&context->ss.uss, sizeof(context->ss.uss));
		bzero(&context->ns.uns, sizeof(context->ns.uns));

		if (context->ns.nsh.flavor == ARM_NEON_SAVED_STATE64) {
			context->ns.ns_64.fpcr = FPCR_DEFAULT;
		} else {
			context->ns.ns_32.fpcr = FPCR_DEFAULT_32;
		}
		context->ss.ss_64.cpsr = PSR64_USER64_DEFAULT;
	}

	thread->machine.DebugData = NULL;

#if defined(HAS_APPLE_PAC)
	/* Sign the initial user-space thread state */
	if (thread->machine.upcb != NULL) {
		uint64_t intr = ml_pac_safe_interrupts_disable();
		asm volatile (
                        "mov	x0, %[iss]"             "\n"
                        "mov	x1, #0"                 "\n"
                        "mov	w2, %w[usr]"            "\n"
                        "mov	x3, #0"                 "\n"
                        "mov	x4, #0"                 "\n"
                        "mov	x5, #0"                 "\n"
                        "msr	SPSel, #1"              "\n"
                        VERIFY_USER_THREAD_STATE_INSTR  "\n"
                        "mov	x6, lr"                 "\n"
                        "bl     _ml_sign_thread_state"  "\n"
                        "msr	SPSel, #0"              "\n"
                        "mov	lr, x6"                 "\n"
                        :
                        : [iss] "r"(thread->machine.upcb), [usr] "r"(thread->machine.upcb->ss_64.cpsr),
                          VERIFY_USER_THREAD_STATE_INPUTS
                        : "x0", "x1", "x2", "x3", "x4", "x5", "x6"
                );
		ml_pac_safe_interrupts_restore(intr);
	}
#endif /* defined(HAS_APPLE_PAC) */
}

/*
 * Routine: machine_thread_dup
 *
 */
kern_return_t
machine_thread_dup(thread_t self,
    thread_t target,
    __unused boolean_t is_corpse)
{
	struct arm_saved_state *self_saved_state;
	struct arm_saved_state *target_saved_state;

	target->machine.cthread_self = self->machine.cthread_self;

	self_saved_state = self->machine.upcb;
	target_saved_state = target->machine.upcb;
	bcopy(self_saved_state, target_saved_state, sizeof(struct arm_saved_state));
#if defined(HAS_APPLE_PAC)
	if (!is_corpse && is_saved_state64(self_saved_state)) {
		check_and_sign_copied_user_thread_state(target_saved_state, self_saved_state);
	}
#endif /* defined(HAS_APPLE_PAC) */

	arm_neon_saved_state_t *self_neon_state = self->machine.uNeon;
	arm_neon_saved_state_t *target_neon_state = target->machine.uNeon;
	bcopy(self_neon_state, target_neon_state, sizeof(*target_neon_state));

#if HAVE_MACHINE_THREAD_MATRIX_STATE
	if (self->machine.umatrix_hdr) {
		machine_thread_matrix_state_dup(target);
	}
#endif

	return KERN_SUCCESS;
}

/*
 * Routine: get_user_regs
 *
 */
struct arm_saved_state *
get_user_regs(thread_t thread)
{
	return thread->machine.upcb;
}

arm_neon_saved_state_t *
get_user_neon_regs(thread_t thread)
{
	return thread->machine.uNeon;
}

/*
 * Routine: find_user_regs
 *
 */
struct arm_saved_state *
find_user_regs(thread_t thread)
{
	return thread->machine.upcb;
}

/*
 * Routine: find_kern_regs
 *
 */
struct arm_saved_state *
find_kern_regs(thread_t thread)
{
	/*
	 * This works only for an interrupted kernel thread
	 */
	if (thread != current_thread() || getCpuDatap()->cpu_int_state == NULL) {
		return (struct arm_saved_state *) NULL;
	} else {
		return getCpuDatap()->cpu_int_state;
	}
}

arm_debug_state32_t *
find_debug_state32(thread_t thread)
{
	if (thread && thread->machine.DebugData) {
		return &(thread->machine.DebugData->uds.ds32);
	} else {
		return NULL;
	}
}

arm_debug_state64_t *
find_debug_state64(thread_t thread)
{
	if (thread && thread->machine.DebugData) {
		return &(thread->machine.DebugData->uds.ds64);
	} else {
		return NULL;
	}
}

os_refgrp_decl(static, dbg_refgrp, "arm_debug_state", NULL);

/**
 *  Finds the debug state for the given 64 bit thread, allocating one if it
 *  does not exist.
 *
 *  @param thread 64 bit thread to find or allocate debug state for
 *
 *  @returns A pointer to the given thread's 64 bit debug state or a null
 *           pointer if the given thread is null or the allocation of a new
 *           debug state fails.
 */
arm_debug_state64_t *
find_or_allocate_debug_state64(thread_t thread)
{
	arm_debug_state64_t *thread_state = find_debug_state64(thread);
	if (thread != NULL && thread_state == NULL) {
		thread->machine.DebugData = zalloc_flags(ads_zone,
		    Z_WAITOK | Z_NOFAIL);
		bzero(thread->machine.DebugData, sizeof *(thread->machine.DebugData));
		thread->machine.DebugData->dsh.flavor = ARM_DEBUG_STATE64;
		thread->machine.DebugData->dsh.count = ARM_DEBUG_STATE64_COUNT;
		os_ref_init(&thread->machine.DebugData->ref, &dbg_refgrp);
		thread_state = find_debug_state64(thread);
	}
	return thread_state;
}

/**
 *  Finds the debug state for the given 32 bit thread, allocating one if it
 *  does not exist.
 *
 *  @param thread 32 bit thread to find or allocate debug state for
 *
 *  @returns A pointer to the given thread's 32 bit debug state or a null
 *           pointer if the given thread is null or the allocation of a new
 *           debug state fails.
 */
arm_debug_state32_t *
find_or_allocate_debug_state32(thread_t thread)
{
	arm_debug_state32_t *thread_state = find_debug_state32(thread);
	if (thread != NULL && thread_state == NULL) {
		thread->machine.DebugData = zalloc_flags(ads_zone,
		    Z_WAITOK | Z_NOFAIL);
		bzero(thread->machine.DebugData, sizeof *(thread->machine.DebugData));
		thread->machine.DebugData->dsh.flavor = ARM_DEBUG_STATE32;
		thread->machine.DebugData->dsh.count = ARM_DEBUG_STATE32_COUNT;
		os_ref_init(&thread->machine.DebugData->ref, &dbg_refgrp);
		thread_state = find_debug_state32(thread);
	}
	return thread_state;
}

/**
 *	Frees a thread's debug state if allocated. Otherwise does nothing.
 *
 *  @param thread thread to free the debug state of
 */
static inline void
free_debug_state(thread_t thread)
{
	if (thread != NULL && thread->machine.DebugData != NULL) {
		arm_debug_state_t *pTmp = thread->machine.DebugData;
		thread->machine.DebugData = NULL;

		if (os_ref_release(&pTmp->ref) == 0) {
			zfree(ads_zone, pTmp);
		}
	}
}

/*
 * Routine: thread_userstack
 *
 */
kern_return_t
thread_userstack(__unused thread_t  thread,
    int                flavor,
    thread_state_t     tstate,
    unsigned int       count,
    mach_vm_offset_t * user_stack,
    int *              customstack,
    boolean_t          is_64bit_data
    )
{
	register_t sp;

	switch (flavor) {
	case ARM_THREAD_STATE:
		if (count == ARM_UNIFIED_THREAD_STATE_COUNT) {
#if __arm64__
			if (is_64bit_data) {
				sp = ((arm_unified_thread_state_t *)tstate)->ts_64.sp;
			} else
#endif
			{
				sp = ((arm_unified_thread_state_t *)tstate)->ts_32.sp;
			}

			break;
		}

		/* INTENTIONAL FALL THROUGH (see machine_thread_set_state) */
		OS_FALLTHROUGH;
	case ARM_THREAD_STATE32:
		if (count != ARM_THREAD_STATE32_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}
		if (is_64bit_data) {
			return KERN_INVALID_ARGUMENT;
		}

		sp = ((arm_thread_state32_t *)tstate)->sp;
		break;
#if __arm64__
	case ARM_THREAD_STATE64:
		if (count != ARM_THREAD_STATE64_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}
		if (!is_64bit_data) {
			return KERN_INVALID_ARGUMENT;
		}

		sp = ((arm_thread_state32_t *)tstate)->sp;
		break;
#endif
	default:
		return KERN_INVALID_ARGUMENT;
	}

	if (sp) {
		*user_stack = CAST_USER_ADDR_T(sp);
		if (customstack) {
			*customstack = 1;
		}
	} else {
		*user_stack = CAST_USER_ADDR_T(USRSTACK64);
		if (customstack) {
			*customstack = 0;
		}
	}

	return KERN_SUCCESS;
}

/*
 * thread_userstackdefault:
 *
 * Return the default stack location for the
 * thread, if otherwise unknown.
 */
kern_return_t
thread_userstackdefault(mach_vm_offset_t * default_user_stack,
    boolean_t          is64bit)
{
	if (is64bit) {
		*default_user_stack = USRSTACK64;
	} else {
		*default_user_stack = USRSTACK;
	}

	return KERN_SUCCESS;
}

/*
 * Routine: thread_setuserstack
 *
 */
void
thread_setuserstack(thread_t          thread,
    mach_vm_address_t user_stack)
{
	struct arm_saved_state *sv;

	sv = get_user_regs(thread);

	set_saved_state_sp(sv, user_stack);

	return;
}

/*
 * Routine: thread_adjuserstack
 *
 */
user_addr_t
thread_adjuserstack(thread_t thread,
    int      adjust)
{
	struct arm_saved_state *sv;
	uint64_t sp;

	sv = get_user_regs(thread);

	sp = get_saved_state_sp(sv);
	sp += adjust;
	set_saved_state_sp(sv, sp);

	return sp;
}


/*
 * Routine: thread_setentrypoint
 *
 */
void
thread_setentrypoint(thread_t         thread,
    mach_vm_offset_t entry)
{
	struct arm_saved_state *sv;

#if HAS_APPLE_PAC
	uint64_t intr = ml_pac_safe_interrupts_disable();
#endif

	sv = get_user_regs(thread);

	set_user_saved_state_pc(sv, entry);

#if HAS_APPLE_PAC
	ml_pac_safe_interrupts_restore(intr);
#endif

	return;
}

/*
 * Routine: thread_entrypoint
 *
 */
kern_return_t
thread_entrypoint(__unused thread_t  thread,
    int                flavor,
    thread_state_t     tstate,
    unsigned int       count,
    mach_vm_offset_t * entry_point
    )
{
	switch (flavor) {
	case ARM_THREAD_STATE:
	{
		struct arm_thread_state *state;

		if (count != ARM_THREAD_STATE_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}

		state = (struct arm_thread_state *) tstate;

		/*
		 * If a valid entry point is specified, use it.
		 */
		if (state->pc) {
			*entry_point = CAST_USER_ADDR_T(state->pc);
		} else {
			*entry_point = CAST_USER_ADDR_T(VM_MIN_ADDRESS);
		}
	}
	break;

	case ARM_THREAD_STATE64:
	{
		struct arm_thread_state64 *state;

		if (count != ARM_THREAD_STATE64_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}

		state = (struct arm_thread_state64*) tstate;

		/*
		 * If a valid entry point is specified, use it.
		 */
		if (state->pc) {
			*entry_point = CAST_USER_ADDR_T(state->pc);
		} else {
			*entry_point = CAST_USER_ADDR_T(VM_MIN_ADDRESS);
		}

		break;
	}
	default:
		return KERN_INVALID_ARGUMENT;
	}

	return KERN_SUCCESS;
}


/*
 * Routine: thread_set_child
 *
 */
void
thread_set_child(thread_t child,
    int      pid)
{
	struct arm_saved_state *child_state;

	child_state = get_user_regs(child);

	set_user_saved_state_reg(child_state, 0, pid);
	set_user_saved_state_reg(child_state, 1, 1ULL);
}


struct arm_act_context {
	struct arm_unified_thread_state ss;
#if __ARM_VFP__
	struct arm_neon_saved_state ns;
#endif
};

/*
 * Routine: act_thread_csave
 *
 */
void *
act_thread_csave(void)
{
	struct arm_act_context *ic;
	kern_return_t   kret;
	unsigned int    val;
	thread_t thread = current_thread();

	ic = kalloc_type(struct arm_act_context, Z_WAITOK);
	if (ic == (struct arm_act_context *) NULL) {
		return (void *) 0;
	}

	val = ARM_UNIFIED_THREAD_STATE_COUNT;
	kret = machine_thread_get_state(thread, ARM_THREAD_STATE, (thread_state_t)&ic->ss, &val);
	if (kret != KERN_SUCCESS) {
		kfree_type(struct arm_act_context, ic);
		return (void *) 0;
	}

#if __ARM_VFP__
	if (thread_is_64bit_data(thread)) {
		val = ARM_NEON_STATE64_COUNT;
		kret = machine_thread_get_state(thread,
		    ARM_NEON_STATE64,
		    (thread_state_t)&ic->ns,
		    &val);
	} else {
		val = ARM_NEON_STATE_COUNT;
		kret = machine_thread_get_state(thread,
		    ARM_NEON_STATE,
		    (thread_state_t)&ic->ns,
		    &val);
	}
	if (kret != KERN_SUCCESS) {
		kfree_type(struct arm_act_context, ic);
		return (void *) 0;
	}
#endif
	return ic;
}

/*
 * Routine: act_thread_catt
 *
 */
void
act_thread_catt(void * ctx)
{
	struct arm_act_context *ic;
	kern_return_t   kret;
	thread_t thread = current_thread();

	ic = (struct arm_act_context *) ctx;
	if (ic == (struct arm_act_context *) NULL) {
		return;
	}

	kret = machine_thread_set_state(thread, ARM_THREAD_STATE, (thread_state_t)&ic->ss, ARM_UNIFIED_THREAD_STATE_COUNT);
	if (kret != KERN_SUCCESS) {
		goto out;
	}

#if __ARM_VFP__
	if (thread_is_64bit_data(thread)) {
		kret = machine_thread_set_state(thread,
		    ARM_NEON_STATE64,
		    (thread_state_t)&ic->ns,
		    ARM_NEON_STATE64_COUNT);
	} else {
		kret = machine_thread_set_state(thread,
		    ARM_NEON_STATE,
		    (thread_state_t)&ic->ns,
		    ARM_NEON_STATE_COUNT);
	}
	if (kret != KERN_SUCCESS) {
		goto out;
	}
#endif
out:
	kfree_type(struct arm_act_context, ic);
}

/*
 * Routine: act_thread_catt
 *
 */
void
act_thread_cfree(void *ctx)
{
	kfree_type(struct arm_act_context, ctx);
}

kern_return_t
thread_set_wq_state32(thread_t       thread,
    thread_state_t tstate)
{
	arm_thread_state_t *state;
	struct arm_saved_state *saved_state;
	struct arm_saved_state32 *saved_state_32;
	thread_t curth = current_thread();
	spl_t s = 0;

	assert(!thread_is_64bit_data(thread));

	saved_state = thread->machine.upcb;
	saved_state_32 = saved_state32(saved_state);

	state = (arm_thread_state_t *)tstate;

	if (curth != thread) {
		s = splsched();
		thread_lock(thread);
	}

	/*
	 * do not zero saved_state, it can be concurrently accessed
	 * and zero is not a valid state for some of the registers,
	 * like sp.
	 */
	thread_state32_to_saved_state(state, saved_state);
	saved_state_32->cpsr = PSR64_USER32_DEFAULT;

	if (curth != thread) {
		thread_unlock(thread);
		splx(s);
	}

	return KERN_SUCCESS;
}

kern_return_t
thread_set_wq_state64(thread_t       thread,
    thread_state_t tstate)
{
	arm_thread_state64_t *state;
	struct arm_saved_state *saved_state;
	struct arm_saved_state64 *saved_state_64;
	thread_t curth = current_thread();
	spl_t s = 0;

	assert(thread_is_64bit_data(thread));

	saved_state = thread->machine.upcb;
	saved_state_64 = saved_state64(saved_state);
	state = (arm_thread_state64_t *)tstate;

	if (curth != thread) {
		s = splsched();
		thread_lock(thread);
	}

	/*
	 * do not zero saved_state, it can be concurrently accessed
	 * and zero is not a valid state for some of the registers,
	 * like sp.
	 */
	thread_state64_to_saved_state(state, saved_state);
	set_user_saved_state_cpsr(saved_state, PSR64_USER64_DEFAULT);

	if (curth != thread) {
		thread_unlock(thread);
		splx(s);
	}

	return KERN_SUCCESS;
}
