/*
 * Copyright (c) 2019 Apple Computer, Inc. All rights reserved.
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

#include "exc_helpers.h"

#include <darwintest.h>
#include <ptrauth.h>
#include <stdbool.h>
#include <stdlib.h>

#if __arm64__
#define EXCEPTION_THREAD_STATE          ARM_THREAD_STATE64
#define EXCEPTION_THREAD_STATE_COUNT    ARM_THREAD_STATE64_COUNT
#elif __x86_64__
#define EXCEPTION_THREAD_STATE          x86_THREAD_STATE
#define EXCEPTION_THREAD_STATE_COUNT    x86_THREAD_STATE_COUNT
#else
#error Unsupported architecture
#endif

#define EXCEPTION_IDENTITY_PROTECTED 4

/**
 * mach_exc_server() is a MIG-generated function that verifies the message
 * that was received is indeed a mach exception and then calls
 * catch_mach_exception_raise_state() to handle the exception.
 */
extern boolean_t mach_exc_server(mach_msg_header_t *, mach_msg_header_t *);

extern kern_return_t
catch_mach_exception_raise(
	mach_port_t exception_port,
	mach_port_t thread,
	mach_port_t task,
	exception_type_t type,
	exception_data_t codes,
	mach_msg_type_number_t code_count);

extern kern_return_t
catch_mach_exception_raise_identity_protected(
	__unused mach_port_t      exception_port,
	uint64_t                  thread_id,
	mach_port_t               task_id_token,
	exception_type_t          exception,
	mach_exception_data_t     codes,
	mach_msg_type_number_t    codeCnt);

extern kern_return_t
catch_mach_exception_raise_backtrace(
	__unused mach_port_t exception_port,
	mach_port_t kcdata_object,
	exception_type_t exception,
	mach_exception_data_t codes,
	__unused mach_msg_type_number_t codeCnt);

extern kern_return_t
catch_mach_exception_raise_state(
	mach_port_t exception_port,
	exception_type_t type,
	exception_data_t codes,
	mach_msg_type_number_t code_count,
	int *flavor,
	thread_state_t in_state,
	mach_msg_type_number_t in_state_count,
	thread_state_t out_state,
	mach_msg_type_number_t *out_state_count);

extern kern_return_t
catch_mach_exception_raise_state_identity(
	mach_port_t exception_port,
	mach_port_t thread,
	mach_port_t task,
	exception_type_t type,
	exception_data_t codes,
	mach_msg_type_number_t code_count,
	int *flavor,
	thread_state_t in_state,
	mach_msg_type_number_t in_state_count,
	thread_state_t out_state,
	mach_msg_type_number_t *out_state_count);

static exc_handler_callback_t exc_handler_callback;
static exc_handler_protected_callback_t exc_handler_protected_callback;
static exc_handler_state_protected_callback_t exc_handler_state_protected_callback;
static exc_handler_backtrace_callback_t exc_handler_backtrace_callback;

/**
 * This has to be defined for linking purposes, but it's unused.
 */
kern_return_t
catch_mach_exception_raise(
	mach_port_t exception_port,
	mach_port_t thread,
	mach_port_t task,
	exception_type_t type,
	exception_data_t codes,
	mach_msg_type_number_t code_count)
{
#pragma unused(exception_port, thread, task, type, codes, code_count)
	T_FAIL("Triggered catch_mach_exception_raise() which shouldn't happen...");
	__builtin_unreachable();
}

kern_return_t
catch_mach_exception_raise_state_identity_protected(
	mach_port_t exception_port __unused,
	uint64_t                  thread_id,
	mach_port_t               task_id_token,
	exception_type_t type,
	exception_data_t codes,
	mach_msg_type_number_t code_count,
	int *flavor,
	thread_state_t in_state,
	mach_msg_type_number_t in_state_count,
	thread_state_t out_state,
	mach_msg_type_number_t *out_state_count)
{
	T_LOG("Caught a mach exception!\n");
	/* There should only be two code values. */
	T_QUIET; T_ASSERT_EQ(code_count, 2, "Two code values were provided with the mach exception");

	/**
	 * The code values should be 64-bit since MACH_EXCEPTION_CODES was specified
	 * when setting the exception port.
	 */
	mach_exception_data_t codes_64 = (mach_exception_data_t)(void *)codes;
	T_LOG("Mach exception codes[0]: %#llx, codes[1]: %#llx\n", codes_64[0], codes_64[1]);

	/* Verify that we're receiving the expected thread state flavor. */
	T_QUIET; T_ASSERT_EQ(*flavor, EXCEPTION_THREAD_STATE, "The thread state flavor is EXCEPTION_THREAD_STATE");
	T_QUIET; T_ASSERT_EQ(in_state_count, EXCEPTION_THREAD_STATE_COUNT, "The thread state count is EXCEPTION_THREAD_STATE_COUNT");

	*out_state_count = in_state_count; /* size of state object in 32-bit words */
	memcpy((void*)out_state, (void*)in_state, in_state_count * 4);

	exc_handler_state_protected_callback(task_id_token, thread_id, type, codes_64, in_state,
	    in_state_count, out_state, out_state_count);

	/* Return KERN_SUCCESS to tell the kernel to keep running the victim thread. */
	return KERN_SUCCESS;
}


kern_return_t
catch_mach_exception_raise_identity_protected(
	__unused mach_port_t      exception_port,
	uint64_t                  thread_id,
	mach_port_t               task_id_token,
	exception_type_t          exception,
	mach_exception_data_t     codes,
	mach_msg_type_number_t    codeCnt)
{
	T_LOG("Caught a mach exception!\n");

	/* There should only be two code values. */
	T_QUIET; T_ASSERT_EQ(codeCnt, 2, "Two code values were provided with the mach exception");

	/**
	 * The code values should be 64-bit since MACH_EXCEPTION_CODES was specified
	 * when setting the exception port.
	 */
	mach_exception_data_t codes_64 = (mach_exception_data_t)(void *)codes;
	T_LOG("Mach exception codes[0]: %#llx, codes[1]: %#llx\n", codes_64[0], codes_64[1]);

	exc_handler_protected_callback(task_id_token, thread_id, exception, codes_64);

	T_LOG("Assuming the thread state modification was done in the callback, skipping it");

	/* Return KERN_SUCCESS to tell the kernel to keep running the victim thread. */
	return KERN_SUCCESS;
}

/**
 * This has to be defined for linking purposes, but it's unused.
 */
kern_return_t
catch_mach_exception_raise_state(
	mach_port_t exception_port,
	exception_type_t type,
	exception_data_t codes,
	mach_msg_type_number_t code_count,
	int *flavor,
	thread_state_t in_state,
	mach_msg_type_number_t in_state_count,
	thread_state_t out_state,
	mach_msg_type_number_t *out_state_count)
{
#pragma unused(exception_port, type, codes, code_count, flavor, in_state, in_state_count, out_state, out_state_count)
	T_FAIL("Triggered catch_mach_exception_raise_state() which shouldn't happen...");
	__builtin_unreachable();
}

/**
 * Called by mach_exc_server() to handle the exception. This will call the
 * test's exception-handler callback and will then modify
 * the thread state to move to the next instruction.
 */
kern_return_t
catch_mach_exception_raise_state_identity(
	mach_port_t exception_port __unused,
	mach_port_t thread,
	mach_port_t task,
	exception_type_t type,
	exception_data_t codes,
	mach_msg_type_number_t code_count,
	int *flavor,
	thread_state_t in_state,
	mach_msg_type_number_t in_state_count,
	thread_state_t out_state,
	mach_msg_type_number_t *out_state_count)
{
	T_LOG("Caught a mach exception!\n");

	/* There should only be two code values. */
	T_QUIET; T_ASSERT_EQ(code_count, 2, "Two code values were provided with the mach exception");

	/**
	 * The code values should be 64-bit since MACH_EXCEPTION_CODES was specified
	 * when setting the exception port.
	 */
	mach_exception_data_t codes_64 = (mach_exception_data_t)(void *)codes;
	T_LOG("Mach exception codes[0]: %#llx, codes[1]: %#llx\n", codes_64[0], codes_64[1]);

	/* Verify that we're receiving the expected thread state flavor. */
	T_QUIET; T_ASSERT_EQ(*flavor, EXCEPTION_THREAD_STATE, "The thread state flavor is EXCEPTION_THREAD_STATE");
	T_QUIET; T_ASSERT_EQ(in_state_count, EXCEPTION_THREAD_STATE_COUNT, "The thread state count is EXCEPTION_THREAD_STATE_COUNT");

	size_t advance_pc = exc_handler_callback(task, thread, type, codes_64);

	/**
	 * Increment the PC by the requested amount so the thread doesn't cause
	 * another exception when it resumes.
	 */
	*out_state_count = in_state_count; /* size of state object in 32-bit words */
	memcpy((void*)out_state, (void*)in_state, in_state_count * 4);

#if __arm64__
	arm_thread_state64_t *state = (arm_thread_state64_t*)(void *)out_state;

	void *pc = (void*)(arm_thread_state64_get_pc(*state) + advance_pc);
	/* Have to sign the new PC value when pointer authentication is enabled. */
	pc = ptrauth_sign_unauthenticated(pc, ptrauth_key_function_pointer, 0);
	arm_thread_state64_set_pc_fptr(*state, pc);
#else
	(void)advance_pc;
	T_FAIL("catch_mach_exception_raise_state() not fully implemented on this architecture");
	__builtin_unreachable();
#endif

	/* Return KERN_SUCCESS to tell the kernel to keep running the victim thread. */
	return KERN_SUCCESS;
}

kern_return_t
catch_mach_exception_raise_backtrace(
	__unused mach_port_t exception_port,
	mach_port_t kcdata_object,
	exception_type_t exception,
	mach_exception_data_t codes,
	__unused mach_msg_type_number_t codeCnt)
{
	return exc_handler_backtrace_callback(kcdata_object, exception, codes);
}

mach_port_t
create_exception_port(exception_mask_t exception_mask)
{
	return create_exception_port_behavior64(exception_mask, EXCEPTION_STATE_IDENTITY);
}

mach_port_t
create_exception_port_behavior64(exception_mask_t exception_mask, exception_behavior_t behavior)
{
	mach_port_t exc_port = MACH_PORT_NULL;
	mach_port_t task = mach_task_self();
	mach_port_t thread = mach_thread_self();
	kern_return_t kr = KERN_SUCCESS;

	if (((unsigned int)behavior & ~MACH_EXCEPTION_MASK) != EXCEPTION_STATE_IDENTITY &&
	    ((unsigned int)behavior & ~MACH_EXCEPTION_MASK) != EXCEPTION_IDENTITY_PROTECTED) {
		T_FAIL("Passed behavior (%d) is not supported by exc_helpers.", behavior);
	}

	behavior |= MACH_EXCEPTION_CODES;

	/* Create the mach port the exception messages will be sent to. */
	kr = mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE, &exc_port);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "Allocated mach exception port");

	/**
	 * Insert a send right into the exception port that the kernel will use to
	 * send the exception thread the exception messages.
	 */
	kr = mach_port_insert_right(task, exc_port, exc_port, MACH_MSG_TYPE_MAKE_SEND);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "Inserted a SEND right into the exception port");

	/* Tell the kernel what port to send exceptions to. */
	kr = thread_set_exception_ports(
		thread,
		exception_mask,
		exc_port,
		(exception_behavior_t)((unsigned int)behavior),
		EXCEPTION_THREAD_STATE);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "Set the exception port to my custom handler");

	return exc_port;
}

struct thread_params {
	mach_port_t exc_port;
	bool run_once;
};

/**
 * Thread to handle the mach exception.
 *
 * @param arg The exception port to wait for a message on.
 */
static void *
exc_server_thread(void *arg)
{
	struct thread_params *params = arg;
	mach_port_t exc_port = params->exc_port;
	bool run_once = params->run_once;
	free(params);

	/**
	 * mach_msg_server_once is a helper function provided by libsyscall that
	 * handles creating mach messages, blocks waiting for a message on the
	 * exception port, calls mach_exc_server() to handle the exception, and
	 * sends a reply based on the return value of mach_exc_server().
	 */
#define MACH_MSG_REPLY_SIZE 4096
	kern_return_t kr;
	if (run_once) {
		kr = mach_msg_server_once(mach_exc_server, MACH_MSG_REPLY_SIZE, exc_port, 0);
	} else {
		kr = mach_msg_server(mach_exc_server, MACH_MSG_REPLY_SIZE, exc_port, 0);
	}
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "Received mach exception message");

	pthread_exit((void*)0);
	__builtin_unreachable();
}

static void
_run_exception_handler(mach_port_t exc_port, void *preferred_callback, void *callback, bool run_once, exception_behavior_t behavior)
{
	if (behavior & MACH_EXCEPTION_BACKTRACE_PREFERRED) {
		T_QUIET; T_ASSERT_NE(NULL, preferred_callback, "Require a preferred callback");
		exc_handler_backtrace_callback = (exc_handler_backtrace_callback_t)preferred_callback;
	}

	behavior &= ~MACH_EXCEPTION_MASK;

	switch (behavior) {
	case EXCEPTION_STATE_IDENTITY:
		exc_handler_callback = (exc_handler_callback_t)callback;
		break;
	case EXCEPTION_STATE_IDENTITY_PROTECTED:
		exc_handler_state_protected_callback = (exc_handler_state_protected_callback_t)callback;
		break;
	case EXCEPTION_IDENTITY_PROTECTED:
		exc_handler_protected_callback = (exc_handler_protected_callback_t)callback;
		break;
	default:
		T_FAIL("Unsupported behavior");
		break;
	}

	pthread_t exc_thread;

	/* Spawn the exception server's thread. */
	struct thread_params *params = malloc(sizeof(*params));
	params->exc_port = exc_port;
	params->run_once = run_once;
	int err = pthread_create(&exc_thread, (pthread_attr_t*)0, exc_server_thread, params);
	T_QUIET; T_ASSERT_POSIX_ZERO(err, "Spawned exception server thread");

	/* No need to wait for the exception server to be joined when it exits. */
	pthread_detach(exc_thread);
}

void
run_exception_handler(mach_port_t exc_port, exc_handler_callback_t callback)
{
	run_exception_handler_behavior64(exc_port, NULL, (void *)callback, EXCEPTION_STATE_IDENTITY, true);
}

void
run_exception_handler_behavior64(mach_port_t exc_port, void *preferred_callback,
    void *callback, exception_behavior_t behavior, bool run_once)
{
	if (((unsigned int)behavior & ~MACH_EXCEPTION_MASK) != EXCEPTION_STATE_IDENTITY &&
	    ((unsigned int)behavior & ~MACH_EXCEPTION_MASK) != EXCEPTION_IDENTITY_PROTECTED &&
	    ((unsigned int)behavior & ~MACH_EXCEPTION_MASK) != EXCEPTION_STATE_IDENTITY_PROTECTED) {
		T_FAIL("Passed behavior (%d) is not supported by exc_helpers.", behavior);
	}

	_run_exception_handler(exc_port, (void *)preferred_callback, (void *)callback, run_once, behavior);
}

void
repeat_exception_handler(mach_port_t exc_port, exc_handler_callback_t callback)
{
	_run_exception_handler(exc_port, NULL, (void *)callback, false, EXCEPTION_STATE_IDENTITY);
}
