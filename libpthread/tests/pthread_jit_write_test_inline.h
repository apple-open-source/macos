#ifndef __PTHREAD_JIT_WRITE_TEST_INLINE_H__
#define __PTHREAD_JIT_WRITE_TEST_INLINE_H__

// This is really less a header file and more a workaround for the darwintest
// Makefile making it miserable to share code between tests built into different
// executables, which pthread_jit_write_protection and
// pthread_jit_write_with_callback require because they must be entitled
// differently.

#include <darwintest.h>

#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <libkern/OSCacheControl.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <setjmp.h>

#include <mach/vm_param.h>
#include <pthread.h>

#include <pthread/jit_private.h>

#if __has_include(<ptrauth.h>)
#include <ptrauth.h>
#endif

#include <TargetConditionals.h>

#if !TARGET_OS_OSX
#error "These tests are only expected to run on macOS"
#endif // TARGET_OS_OSX

/* Enumerations */
typedef enum _access_type {
	ACCESS_WRITE,
	ACCESS_EXECUTE,
} access_type_t;

/* Structures */
typedef struct {
	bool fault_expected;
	jmp_buf fault_jmp;
} fault_state_t;

/* Globals */
static void * rwx_addr = NULL;
static pthread_key_t jit_test_fault_state_key;

/*
 * Return instruction encodings; a default value is given so that this test can
 * be built for an architecture that may not support the tested feature.
 */
#ifdef __arm__
static uint32_t ret_encoding = 0xe12fff1e;
#elif defined(__arm64__)
static uint32_t ret_encoding = 0xd65f03c0;
#elif defined(__x86_64__)
static uint32_t ret_encoding = 0x909090c3;
#else
#error "Unsupported architecture"
#endif

/* Allocate a fault_state_t, and associate it with the current thread. */
static fault_state_t *
fault_state_create(void)
{
	fault_state_t * fault_state = malloc(sizeof(fault_state_t));

	if (fault_state) {
		fault_state->fault_expected = false;

		if (pthread_setspecific(jit_test_fault_state_key, fault_state)) {
			free(fault_state);
			fault_state = NULL;
		}
	}

	return fault_state;
}

/* Disassociate the given fault state from the current thread, and destroy it. */
static void
fault_state_destroy(void * fault_state)
{
	if (fault_state == NULL) {
		T_ASSERT_FAIL("Attempted to fault_state_destroy NULL");
	}

	free(fault_state);
}

static fault_state_t *g_fault_state = NULL;
static bool key_created = false;

static void
pthread_jit_test_setup(void)
{
	T_SETUPBEGIN;

	/* Set up the necessary state for the test. */
	int err = pthread_key_create(&jit_test_fault_state_key, fault_state_destroy);

	T_ASSERT_POSIX_ZERO(err, 0, "Create pthread key");

	key_created = true;

	g_fault_state = fault_state_create();

	T_ASSERT_NOTNULL(g_fault_state, "Create fault state");

	/*
	 * Create a JIT enabled mapping that we can use to test restriction of
	 * RWX mappings.
	 */
	rwx_addr = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANON | MAP_PRIVATE | MAP_JIT, -1, 0);

	T_ASSERT_NE_PTR(rwx_addr, MAP_FAILED, "Map range as MAP_JIT");

	T_SETUPEND;
}

static void
pthread_jit_test_teardown(void)
{
	T_SETUPBEGIN;

	T_ASSERT_POSIX_SUCCESS(munmap(rwx_addr, PAGE_SIZE), "Unmap MAP_JIT mapping");

	if (g_fault_state) {
		T_ASSERT_POSIX_ZERO(pthread_setspecific(jit_test_fault_state_key, NULL), "Remove fault_state");

		fault_state_destroy(g_fault_state);
	}

	if (key_created) {
		T_ASSERT_POSIX_ZERO(pthread_key_delete(jit_test_fault_state_key), "Delete fault state key");
	}

	T_SETUPEND;
}

/*
 * A signal handler that attempts to resolve anticipated faults through use of
 * the pthread_jit_write_protect functions.
 */
static void
access_failed_handler(int signum)
{
	fault_state_t * fault_state;

	/* This handler should ONLY handle SIGBUS. */
	if (signum != SIGBUS) {
		T_ASSERT_FAIL("Unexpected signal sent to handler");
	}

	if (!(fault_state = pthread_getspecific(jit_test_fault_state_key))) {
		T_ASSERT_FAIL("Failed to retrieve fault state");
	}

	if (!(fault_state->fault_expected)) {
		T_ASSERT_FAIL("Unexpected fault taken");
	}

	longjmp(fault_state->fault_jmp, 1);
}

/*
 * Attempt the specified access; if the access faults, this will return true;
 * otherwise, it will return false.
 */
static bool
does_access_fault(access_type_t access_type, void * addr)
{
	fault_state_t * fault_state;

	struct sigaction old_action; /* Save area for any existing action. */
	struct sigaction new_action; /* The action we wish to install for SIGBUS. */

	bool faulted = false;

	void (*func)(void);

	new_action.sa_handler = access_failed_handler; /* A handler for write failures. */
	new_action.sa_mask    = 0;                     /* Don't modify the mask. */
	new_action.sa_flags   = 0;                     /* Flags?  Who needs those? */

	if (addr == NULL) {
		T_ASSERT_FAIL("Access attempted against NULL");
	}

	if (!(fault_state = pthread_getspecific(jit_test_fault_state_key))) {
		T_ASSERT_FAIL("Failed to retrieve fault state");
	}

	/* Install a handler so that we can catch SIGBUS. */
	sigaction(SIGBUS, &new_action, &old_action);

	int jmprc = setjmp(fault_state->fault_jmp);
	if (jmprc) {
		faulted = true;
		goto done;
	}

	/* Perform the requested operation. */
	switch (access_type) {
	case ACCESS_WRITE:
		fault_state->fault_expected = true;

		__sync_synchronize();

		/* Attempt to scrawl a return instruction to the given address. */
		*((volatile uint32_t *)addr) = ret_encoding;

		__sync_synchronize();

		fault_state->fault_expected = false;

		/* Invalidate the instruction cache line that we modified. */
		sys_cache_control(kCacheFunctionPrepareForExecution, addr, sizeof(ret_encoding));

		break;
	case ACCESS_EXECUTE:
		/* This is a request to branch to the given address. */
#if __has_feature(ptrauth_calls)
		func = ptrauth_sign_unauthenticated((void *)addr, ptrauth_key_function_pointer, 0);
#else
		func = (void (*)(void))addr;
#endif


		fault_state->fault_expected = true;

		__sync_synchronize();

		/* Branch. */
		func();

		__sync_synchronize();

		fault_state->fault_expected = false;

		break;
	}

done:
	/* Restore the old SIGBUS handler. */
	sigaction(SIGBUS, &old_action, NULL);

	return faulted;
}

#endif // __PTHREAD_JIT_WRITE_TEST_INLINE_H__
