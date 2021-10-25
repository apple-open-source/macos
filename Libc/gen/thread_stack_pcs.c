/*
 * Copyright (c) 1999-2018 Apple Inc. All rights reserved.
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

/*	Bertrand from vmutils -> CF -> System */

#include <pthread.h>
#include <mach/mach.h>
#include <mach/vm_statistics.h>
#include <stdlib.h>
#include <pthread/private.h>
#include <pthread/stack_np.h>
#include "stack_logging.h"

#define	INSTACK(a)	((a) >= stackbot && (a) <= stacktop)
#if defined(__x86_64__)
#define	ISALIGNED(a)	((((uintptr_t)(a)) & 0xf) == 0)
#elif defined(__i386__)
#define	ISALIGNED(a)	((((uintptr_t)(a)) & 0xf) == 8)
#elif defined(__arm__) || defined(__arm64__)
#define	ISALIGNED(a)	((((uintptr_t)(a)) & 0x1) == 0)
#endif

// The Swift async ABI is not implemented on 32bit architectures.
#if __LP64__ || __ARM64_ARCH_8_32__
// Tests if a frame is part of an async extended stack.
// If an extended frame record is needed, the prologue of the function will
// store 3 pointers consecutively in memory:
//    [ AsyncContext, FP | (1 << 60), LR]
// and set the new FP to point to that second element. Bits 63:60 of that
// in-memory FP should be considered an ABI tag of some kind, and stack
// walkers can expect to see 3 different values in the wild:
//    * 0b0000 if there is an old-style frame (and still most non-Swift)
//             record with just [FP, LR].
//    * 0b0001 if there is one of these [Ctx, FP, LR] records.
//    * 0b1111 in current kernel code.
static uint32_t
__is_async_frame(uintptr_t frame)
{
	uint64_t stored_fp = *(uint64_t*)frame;
	if ((stored_fp >> 60) != 1)
		return 0;

	// The Swift runtime stores the async Task pointer in the 3rd Swift
	// private TSD.
	uintptr_t task_address = (uintptr_t)_pthread_getspecific_direct(__PTK_FRAMEWORK_SWIFT_KEY3);
	if (task_address == 0)
		return 0;
	// This offset is an ABI guarantee from the Swift runtime.
	int task_id_offset = 4 * sizeof(void *) + 4;
	uint32_t *task_id_address = (uint32_t *)(task_address + task_id_offset);
	// The TaskID is guaranteed to be non-zero.
	return *task_id_address;
}

// Given a frame pointer that points to an async frame on the stack,
// walk the list of async activations (as opposed to the OS stack) to
// gather the PCs of the successive async activations which led us to
// this point.
__attribute__((noinline))
static void
__thread_stack_async_pcs(vm_address_t *buffer, unsigned max, unsigned *nb, uintptr_t frame)
{
	// The async context pointer is stored right before the saved FP
	uint64_t async_context = *(uint64_t *)(frame - 8);
	uintptr_t resume_addr, next;

	do {
		// The async context starts with 2 pointers:
		// - the parent async context (morally equivalent to the parent
		//   async frame frame pointer)
		// - the resumption PC (morally equivalent to the return address)
		// We can just use pthread_stack_frame_decode_np() because it just
		// strips a data and a code pointer.
#if  __ARM64_ARCH_8_32__
		// On arm64_32, the stack layout is the same (64bit pointers), but
		// the regular pointers in the async context are still 32 bits.
		// Given arm64_32 never has PAC, we can just read them.
		next = *(uintptr_t*)(uintptr_t)async_context;
		resume_addr = *(uintptr_t*)(uintptr_t)(async_context+4);
#else
		next = pthread_stack_frame_decode_np(async_context, &resume_addr);
#endif
		if (!resume_addr)
			return;

		// The resume address for Swift async coroutines is at the beginnining
		// of a function. Most of the clients of backtraces unconditionally
		// apply -1 to the return addresses in order to symbolicate the call
		// site rather than the the return address, and thus symbolicate
		// something unrelated in this case. Mitigate the issue by applying
		// a one byte offset to the resume address before storing it.
		buffer[*nb] = resume_addr + 1;
		(*nb)++;

		if(!next || !ISALIGNED(next))
			return;

		async_context = next;
	} while (max--);
}
#endif

// Gather a maximum of `max` PCs of the current call-stack into `buffer`. If
// `allow_async` is true, then switch to gathering Swift async frames instead
// of the OS call-stack when an extended frame is encountered.
__attribute__((noinline))
static unsigned int
__thread_stack_pcs(vm_address_t *buffer, unsigned max, unsigned *nb,
		unsigned skip, void *startfp, bool allow_async)
{
	void *frame, *next;
	pthread_t self = pthread_self();
	void *stacktop = pthread_get_stackaddr_np(self);
	void *stackbot = stacktop - pthread_get_stacksize_np(self);
	unsigned int has_extended_frame = 0;
	*nb = 0;

	// Rely on the fact that our caller has an empty stackframe (no local vars)
	// to determine the minimum size of a stackframe (frame ptr & return addr)
	frame = __builtin_frame_address(0);
	next = (void*)pthread_stack_frame_decode_np((uintptr_t)frame, NULL);

	/* make sure return address is never out of bounds */
	stacktop -= (next - frame);

	if(!INSTACK(frame) || !ISALIGNED(frame))
		return 0;
	while (startfp || skip--) {
		if (startfp && startfp < next) break;
		if(!INSTACK(next) || !ISALIGNED(next) || next <= frame)
			return 0;
		frame = next;
		next = (void*)pthread_stack_frame_decode_np((uintptr_t)frame, NULL);
	}
	while (max--) {
		uintptr_t retaddr;

#if __LP64__ || __ARM64_ARCH_8_32__
		unsigned int async_task_id = __is_async_frame((uintptr_t)frame);
		if (async_task_id) {
			if (allow_async) {
				__thread_stack_async_pcs(buffer, max, nb, (uintptr_t)frame);
				return async_task_id;
			} else {
				has_extended_frame = 1;
			}
		}
#endif
		next = (void*)pthread_stack_frame_decode_np((uintptr_t)frame, &retaddr);
		buffer[*nb] = retaddr;
		(*nb)++;
		if(!INSTACK(next) || !ISALIGNED(next) || next <= frame)
			return has_extended_frame;
		frame = next;
	}

	return has_extended_frame;
}

// Note that callee relies on this function having a minimal stackframe
// to introspect (i.e. no tailcall and no local variables)
__private_extern__ __attribute__((disable_tail_calls))
void
_thread_stack_pcs(vm_address_t *buffer, unsigned max, unsigned *nb,
		unsigned skip, void *startfp)
{
	// skip this frame
	__thread_stack_pcs(buffer, max, nb, skip + 1, startfp, false);
}

__private_extern__ __attribute__((disable_tail_calls))
unsigned int
_thread_stack_async_pcs(vm_address_t *buffer, unsigned max, unsigned *nb,
		unsigned skip, void *startfp)
{
	// skip this frame
	return __thread_stack_pcs(buffer, max, nb, skip + 1, startfp, true);
}

// Prevent thread_stack_pcs() from getting tail-call-optimized into
// __thread_stack_pcs() on 64-bit environments, thus making the "number of hot
// frames to skip" be more predictable, giving more consistent backtraces.
//
// See <rdar://problem/5364825> "stack logging: frames keep getting truncated"
// for why this is necessary.
//
// Note that callee relies on this function having a minimal stackframe
// to introspect (i.e. no tailcall and no local variables)
__attribute__((disable_tail_calls))
unsigned int
thread_stack_pcs(vm_address_t *buffer, unsigned max, unsigned *nb)
{
	return __thread_stack_pcs(buffer, max, nb, 0, NULL, /* allow_async */false);
}

__attribute__((disable_tail_calls))
unsigned int
thread_stack_async_pcs(vm_address_t *buffer, unsigned max, unsigned *nb)
{
	return __thread_stack_pcs(buffer, max, nb, 0, NULL, /* allow_async */true);
}
