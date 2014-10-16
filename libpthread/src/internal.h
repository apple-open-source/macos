/*
 * Copyright (c) 2000-2013 Apple Inc. All rights reserved.
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
/*
 * Copyright 1996 1995 by Open Software Foundation, Inc. 1997 1996 1995 1994 1993 1992 1991  
 *              All Rights Reserved 
 *  
 * Permission to use, copy, modify, and distribute this software and 
 * its documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation. 
 *  
 * OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE. 
 *  
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR 
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT, 
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION 
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 * 
 */
/*
 * MkLinux
 */

/*
 * POSIX Threads - IEEE 1003.1c
 */

#ifndef _POSIX_PTHREAD_INTERNALS_H
#define _POSIX_PTHREAD_INTERNALS_H

#define _PTHREAD_BUILDING_PTHREAD_

// suppress pthread_attr_t typedef in sys/signal.h
#define _PTHREAD_ATTR_T
struct _pthread_attr_t; /* forward reference */
typedef struct _pthread_attr_t pthread_attr_t;

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <TargetConditionals.h>
#include <libkern/OSAtomic.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <os/once_private.h>
#include <sys/queue.h>

#ifndef __POSIX_LIB__
#define __POSIX_LIB__
#endif

#ifndef PTHREAD_LAYOUT_SPI
#define PTHREAD_LAYOUT_SPI 1
#endif

#include "posix_sched.h"
#include "tsd_private.h"
#include "spinlock_private.h"

#if TARGET_IPHONE_SIMULATOR
#error Unsupported target
#endif

// List of all pthreads in the process.
TAILQ_HEAD(__pthread_list, _pthread);
extern struct __pthread_list __pthread_head;

// Lock protects access to above list.
extern pthread_lock_t _pthread_list_lock;

extern int __is_threaded;

/*
 * Compiled-in limits
 */
#if TARGET_OS_EMBEDDED
#define _EXTERNAL_POSIX_THREAD_KEYS_MAX 256
#define _INTERNAL_POSIX_THREAD_KEYS_MAX 256
#define _INTERNAL_POSIX_THREAD_KEYS_END 512
#else
#define _EXTERNAL_POSIX_THREAD_KEYS_MAX 512
#define _INTERNAL_POSIX_THREAD_KEYS_MAX 256
#define _INTERNAL_POSIX_THREAD_KEYS_END 768
#endif

#define MAXTHREADNAMESIZE	64
#define _PTHREAD_T
typedef struct _pthread {
	//
	// ABI - These fields are externally known as struct _opaque_pthread_t.
	//
	long sig; // _PTHREAD_SIG
	struct __darwin_pthread_handler_rec *__cleanup_stack;

	//
	// SPI - These fields are private.
	//
	// these fields are globally protected by _pthread_list_lock:
	uint32_t childrun:1,
			parentcheck:1,
			childexit:1,
			pad3:29;
	
	pthread_lock_t lock; // protect access to everything below
	uint32_t detached:8,
			inherit:8,
			policy:8,
			kernalloc:1,
			schedset:1,
			wqthread:1,
			wqkillset:1,
			pad:4;

#if __LP64__
	uint32_t pad0;
#endif
	uint64_t thread_id;	// 64-bit unique thread id

	void *(*fun)(void*);	// thread start routine
	void *arg;		// thread start routine argument
	void *exit_value;	// thread exit value storage

	semaphore_t joiner_notify;	// pthread_join notification

	int max_tsd_key;
	int cancel_state;	// whether the thread can be cancelled
	int cancel_error;

#ifdef __i386__
	// i386 err_no must be at a 68 byte offset
	// See <rdar://problem/13249323>
	uint32_t __13249323_pad[3];
#endif
	int err_no;		// thread-local errno

	struct _pthread *joiner;

	struct sched_param param;	// [aligned]

	TAILQ_ENTRY(_pthread) plist;	// global thread list [aligned]

	char pthread_name[MAXTHREADNAMESIZE];	// includes NUL [aligned]
	
	void *stackaddr;	// base of the stack (page aligned)
	size_t stacksize;	// size of stack (page multiple and >= PTHREAD_STACK_MIN)

	void* freeaddr;		// stack/thread allocation base address
	size_t freesize;	// stack/thread allocation size
	size_t guardsize;	// guard page size in bytes

	// thread specific data
	void *tsd[_EXTERNAL_POSIX_THREAD_KEYS_MAX + _INTERNAL_POSIX_THREAD_KEYS_MAX];
} *pthread_t;


struct _pthread_attr_t {
	long sig;
	pthread_lock_t lock;
	uint32_t detached:8,
		inherit:8,
		policy:8,
		fastpath:1,
		schedset:1,
		qosset:1,
		unused:5;
	struct sched_param param; // [aligned]
	void *stackaddr; // stack base; vm_page_size aligned
	size_t stacksize; // stack size; multiple of vm_page_size and >= PTHREAD_STACK_MIN
	size_t guardsize; // size in bytes of stack overflow guard area
	unsigned long qosclass;
#if defined(__LP64__)
	uint32_t _reserved[2];
#else
	uint32_t _reserved[1];
#endif
};

/*
 * Mutex attributes
 */
#define _PTHREAD_MUTEX_POLICY_NONE		0
#define _PTHREAD_MUTEX_POLICY_FAIRSHARE		1
#define _PTHREAD_MUTEX_POLICY_FIRSTFIT		2
#define _PTHREAD_MUTEX_POLICY_REALTIME		3
#define _PTHREAD_MUTEX_POLICY_ADAPTIVE		4
#define _PTHREAD_MUTEX_POLICY_PRIPROTECT	5
#define _PTHREAD_MUTEX_POLICY_PRIINHERIT	6

#define _PTHREAD_MUTEXATTR_T
typedef struct {
	long sig;
	int prioceiling;
	uint32_t protocol:2,
		type:2,
		pshared:2,
		policy:3,
		unused:23;
} pthread_mutexattr_t;

struct _pthread_mutex_options {
	uint32_t protocol:2,
		type:2,
		pshared:2,
		policy:3,
		hold:2,
		misalign:1,
		notify:1,
		mutex:1,
		unused:2,
		lock_count:16;
};

typedef struct {
	long sig;
	pthread_lock_t lock;
	union {
		uint32_t value;
		struct _pthread_mutex_options options;
	} mtxopts;
	int16_t prioceiling;
	int16_t priority;
#if defined(__LP64__)
	uint32_t _pad;
#endif
	uint32_t m_tid[2]; // thread id of thread that has mutex locked, misaligned locks may span to first field of m_seq
	uint32_t m_seq[3];
#if defined(__LP64__)
	uint32_t _reserved;
#endif
	void *reserved2[2];
} _pthread_mutex;


#define _PTHREAD_CONDATTR_T
typedef struct {
	long sig;
	uint32_t pshared:2,
		unsupported:30;
} pthread_condattr_t;


typedef struct {
	long sig;
	pthread_lock_t lock;
	uint32_t unused:29,
		misalign:1,
		pshared:2;
	_pthread_mutex *busy;
	uint32_t c_seq[3];
#if defined(__LP64__)
	uint32_t _reserved[3];
#endif
} _pthread_cond;


#define _PTHREAD_ONCE_T
typedef struct {
	long sig;
	os_once_t once;
} pthread_once_t;


#define _PTHREAD_RWLOCKATTR_T
typedef struct {
	long sig;
	int pshared;
#if defined(__LP64__)
	uint32_t _reserved[3];
#else
	uint32_t _reserved[2];
#endif
} pthread_rwlockattr_t;


typedef struct {
	long sig;
	pthread_lock_t lock;
	uint32_t unused:29,
		misalign:1,
		pshared:2;
	uint32_t rw_flags;
#if defined(__LP64__)
	uint32_t _pad;
#endif
	volatile uint32_t rw_seq[4];
	struct _pthread *rw_owner;
	volatile uint32_t *rw_lcntaddr;
	volatile uint32_t *rw_seqaddr;
	volatile uint32_t *rw_ucntaddr;
#if defined(__LP64__)
	uint32_t _reserved[31];
#else
	uint32_t _reserved[19];
#endif
} _pthread_rwlock;

#include "pthread_spis.h"

// Internal references to pthread_self() use TSD slot 0 directly.
inline static pthread_t __attribute__((__pure__))
_pthread_self_direct(void)
{
	return _pthread_getspecific_direct(_PTHREAD_TSD_SLOT_PTHREAD_SELF);
}
#define pthread_self() _pthread_self_direct()

inline static pthread_t __attribute__((__pure__))
_pthread_selfid_direct(void)
{
	return (_pthread_self_direct())->thread_id;
}

#define _PTHREAD_DEFAULT_INHERITSCHED	PTHREAD_INHERIT_SCHED
#define _PTHREAD_DEFAULT_PROTOCOL	PTHREAD_PRIO_NONE
#define _PTHREAD_DEFAULT_PRIOCEILING	0
#define _PTHREAD_DEFAULT_POLICY		SCHED_OTHER
#define _PTHREAD_DEFAULT_STACKSIZE	0x80000	  /* 512K */
#define _PTHREAD_DEFAULT_PSHARED	PTHREAD_PROCESS_PRIVATE

#define _PTHREAD_NO_SIG			0x00000000
#define _PTHREAD_MUTEX_ATTR_SIG		0x4D545841  /* 'MTXA' */
#define _PTHREAD_MUTEX_SIG		0x4D555458  /* 'MUTX' */
#define _PTHREAD_MUTEX_SIG_init		0x32AAABA7  /* [almost] ~'MUTX' */
#define _PTHREAD_ERRORCHECK_MUTEX_SIG_init      0x32AAABA1
#define _PTHREAD_RECURSIVE_MUTEX_SIG_init       0x32AAABA2
#define _PTHREAD_FIRSTFIT_MUTEX_SIG_init       0x32AAABA3
#define _PTHREAD_MUTEX_SIG_init_MASK            0xfffffff0
#define _PTHREAD_MUTEX_SIG_CMP                  0x32AAABA0
#define _PTHREAD_COND_ATTR_SIG		0x434E4441  /* 'CNDA' */
#define _PTHREAD_COND_SIG		0x434F4E44  /* 'COND' */
#define _PTHREAD_COND_SIG_init		0x3CB0B1BB  /* [almost] ~'COND' */
#define _PTHREAD_ATTR_SIG		0x54484441  /* 'THDA' */
#define _PTHREAD_ONCE_SIG		0x4F4E4345  /* 'ONCE' */
#define _PTHREAD_ONCE_SIG_init		0x30B1BCBA  /* [almost] ~'ONCE' */
#define _PTHREAD_SIG			0x54485244  /* 'THRD' */
#define _PTHREAD_RWLOCK_ATTR_SIG	0x52574C41  /* 'RWLA' */
#define _PTHREAD_RWLOCK_SIG		0x52574C4B  /* 'RWLK' */
#define _PTHREAD_RWLOCK_SIG_init	0x2DA8B3B4  /* [almost] ~'RWLK' */


#define _PTHREAD_KERN_COND_SIG		0x12345678  /*  */
#define _PTHREAD_KERN_MUTEX_SIG		0x34567812  /*  */
#define _PTHREAD_KERN_RWLOCK_SIG	0x56781234  /*  */

#define _PTHREAD_CREATE_PARENT		4
#define _PTHREAD_EXITED			8
// 4597450: begin
#define _PTHREAD_WASCANCEL		0x10
// 4597450: end

#if defined(DEBUG)
#define _PTHREAD_MUTEX_OWNER_SELF	pthread_self()
#else
#define _PTHREAD_MUTEX_OWNER_SELF	(pthread_t)0x12141968
#endif
#define _PTHREAD_MUTEX_OWNER_SWITCHING	(pthread_t)(~0)

#define _PTHREAD_CANCEL_STATE_MASK   0x01
#define _PTHREAD_CANCEL_TYPE_MASK    0x02
#define _PTHREAD_CANCEL_PENDING	     0x10  /* pthread_cancel() has been called for this thread */

extern boolean_t swtch_pri(int);

#define PTHREAD_EXPORT extern __attribute__((visibility("default")))
#define PTHREAD_EXTERN extern
#define PTHREAD_NOEXPORT __attribute__((visibility("hidden")))
#define PTHREAD_NORETURN __attribute__((__noreturn__))
#define PTHREAD_ALWAYS_INLINE __attribute__((always_inline))
#define PTHREAD_NOINLINE __attribute__((noinline))

#include "kern/kern_internal.h"

/* Prototypes. */

/* Internal globals. */
PTHREAD_NOEXPORT extern int __pthread_supported_features;

/* Functions defined in machine-dependent files. */
PTHREAD_NOEXPORT void _pthread_setup(pthread_t th, void (*f)(pthread_t), void *sp, int suspended, int needresume);

PTHREAD_NOEXPORT void _pthread_tsd_cleanup(pthread_t self);

PTHREAD_NOEXPORT int __mtx_droplock(_pthread_mutex *mutex, uint32_t * flagp, uint32_t ** pmtxp, uint32_t * mgenp, uint32_t * ugenp);

/* internally redirected upcalls. */
PTHREAD_NOEXPORT void* malloc(size_t);
PTHREAD_NOEXPORT void free(void*);

/* syscall interfaces */
extern uint32_t __psynch_mutexwait(pthread_mutex_t * mutex,  uint32_t mgen, uint32_t  ugen, uint64_t tid, uint32_t flags);
extern uint32_t __psynch_mutexdrop(pthread_mutex_t * mutex,  uint32_t mgen, uint32_t  ugen, uint64_t tid, uint32_t flags);

extern uint32_t __psynch_cvbroad(pthread_cond_t * cv, uint64_t cvlsgen, uint64_t cvudgen, uint32_t flags, pthread_mutex_t * mutex,  uint64_t mugen, uint64_t tid);
extern uint32_t __psynch_cvsignal(pthread_cond_t * cv, uint64_t cvlsgen, uint32_t cvugen, int thread_port, pthread_mutex_t * mutex,  uint64_t mugen, uint64_t tid, uint32_t flags);
extern uint32_t __psynch_cvwait(pthread_cond_t * cv, uint64_t cvlsgen, uint32_t cvugen, pthread_mutex_t * mutex,  uint64_t mugen, uint32_t flags, int64_t sec, uint32_t nsec);
extern uint32_t __psynch_cvclrprepost(void * cv, uint32_t cvgen, uint32_t cvugen, uint32_t cvsgen, uint32_t prepocnt, uint32_t preposeq, uint32_t flags);
extern uint32_t __psynch_rw_longrdlock(pthread_rwlock_t * rwlock, uint32_t lgenval, uint32_t ugenval, uint32_t rw_wc, int flags);
extern uint32_t __psynch_rw_yieldwrlock(pthread_rwlock_t * rwlock, uint32_t lgenval, uint32_t ugenval, uint32_t rw_wc, int flags);
extern int __psynch_rw_downgrade(pthread_rwlock_t * rwlock, uint32_t lgenval, uint32_t ugenval, uint32_t rw_wc, int flags);
extern uint32_t __psynch_rw_upgrade(pthread_rwlock_t * rwlock, uint32_t lgenval, uint32_t ugenval, uint32_t rw_wc, int flags);
extern uint32_t __psynch_rw_rdlock(pthread_rwlock_t * rwlock, uint32_t lgenval, uint32_t ugenval, uint32_t rw_wc, int flags);
extern uint32_t __psynch_rw_wrlock(pthread_rwlock_t * rwlock, uint32_t lgenval, uint32_t ugenval, uint32_t rw_wc, int flags);
extern uint32_t __psynch_rw_unlock(pthread_rwlock_t * rwlock, uint32_t lgenval, uint32_t ugenval, uint32_t rw_wc, int flags);
extern uint32_t __psynch_rw_unlock2(pthread_rwlock_t * rwlock, uint32_t lgenval, uint32_t ugenval, uint32_t rw_wc, int flags);
extern uint32_t __bsdthread_ctl(uintptr_t cmd, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3);

PTHREAD_EXTERN
int
__proc_info(int callnum, int pid, int flavor, uint64_t arg, void * buffer, int buffersize);

PTHREAD_NOEXPORT int _pthread_lookup_thread(pthread_t thread, mach_port_t * port, int only_joinable);
PTHREAD_NOEXPORT int _pthread_join_cleanup(pthread_t thread, void ** value_ptr, int conforming);

PTHREAD_NORETURN PTHREAD_NOEXPORT
void
__pthread_abort(void);

PTHREAD_NORETURN PTHREAD_NOEXPORT
void
__pthread_abort_reason(const char *fmt, ...);

PTHREAD_NOEXPORT
void
_pthread_set_main_qos(pthread_priority_t qos);

PTHREAD_EXPORT
void
_pthread_start(pthread_t self, mach_port_t kport, void *(*fun)(void *), void * funarg, size_t stacksize, unsigned int flags);

PTHREAD_EXPORT
void
_pthread_wqthread(pthread_t self, mach_port_t kport, void *stackaddr, void *unused, int reuse);

PTHREAD_NOEXPORT
void
__pthread_fork_child_internal(pthread_t p);

PTHREAD_EXPORT
void
_pthread_clear_qos_tsd(mach_port_t thread_port);

PTHREAD_EXPORT
void
_pthread_testcancel(pthread_t thread, int isconforming);

PTHREAD_EXPORT
void
_pthread_exit_if_canceled(int error);

PTHREAD_ALWAYS_INLINE
static inline mach_port_t
_pthread_kernel_thread(pthread_t t)
{
	return t->tsd[_PTHREAD_TSD_SLOT_MACH_THREAD_SELF];
}

PTHREAD_ALWAYS_INLINE
static inline void
_pthread_set_kernel_thread(pthread_t t, mach_port_t p)
{
	t->tsd[_PTHREAD_TSD_SLOT_MACH_THREAD_SELF] = p;
}

#define PTHREAD_ABORT(f,...) __pthread_abort_reason("%s:%s:%u: " f, __FILE__, __func__, __LINE__, ## __VA_ARGS__)

#define PTHREAD_ASSERT(b) do { if (!(b)) PTHREAD_ABORT("failed assertion `%s'", #b); } while (0)

#include <os/semaphore_private.h>
#include <os/alloc_once_private.h>

struct pthread_atfork_entry {
	void (*prepare)(void);
	void (*parent)(void);
	void (*child)(void);
};

#define PTHREAD_ATFORK_INLINE_MAX 10
#define PTHREAD_ATFORK_MAX (vm_page_size/sizeof(struct pthread_atfork_entry))

struct pthread_globals_s {
	// atfork.c
	pthread_t psaved_self;
	OSSpinLock psaved_self_global_lock;
	OSSpinLock pthread_atfork_lock;

	size_t atfork_count;
	struct pthread_atfork_entry atfork_storage[PTHREAD_ATFORK_INLINE_MAX];
	struct pthread_atfork_entry *atfork;
};
typedef struct pthread_globals_s *pthread_globals_t;

__attribute__((__pure__))
static inline pthread_globals_t
_pthread_globals(void)
{
	return os_alloc_once(OS_ALLOC_ONCE_KEY_LIBSYSTEM_PTHREAD,
			     sizeof(struct pthread_globals_s),
			     NULL);
}

#endif /* _POSIX_PTHREAD_INTERNALS_H */
