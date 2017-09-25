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
 * POSIX Pthread Library
 */

#include "resolver.h"
#include "internal.h"

#include <stdio.h>	/* For printf(). */
#include <stdlib.h>
#include <errno.h>	/* For __mach_errno_addr() prototype. */
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/queue.h>
#include <machine/vmparam.h>
#include <mach/vm_statistics.h>

extern int __unix_conforming;
extern int _pthread_cond_wait(pthread_cond_t *cond,
			pthread_mutex_t *mutex,
			const struct timespec *abstime,
			int isRelative,
			int isconforming);
extern int __sigwait(const sigset_t *set, int *sig);
extern int __pthread_sigmask(int, const sigset_t *, sigset_t *);
extern int __pthread_markcancel(mach_port_t);
extern int __pthread_canceled(int);

#ifdef VARIANT_CANCELABLE
extern int __semwait_signal(int cond_sem, int mutex_sem, int timeout, int relative, __int64_t tv_sec, __int32_t tv_nsec);
#else
extern int __semwait_signal(int cond_sem, int mutex_sem, int timeout, int relative, __int64_t tv_sec, __int32_t tv_nsec)  __asm__("___semwait_signal_nocancel");
#endif

PTHREAD_NOEXPORT
int _pthread_join(pthread_t thread, void **value_ptr, int conforming,
		int (*_semwait_signal)(int, int, int, int, __int64_t, __int32_t));

#ifndef VARIANT_CANCELABLE

PTHREAD_ALWAYS_INLINE
static inline int
_pthread_update_cancel_state(pthread_t thread, int mask, int state)
{
	int oldstate, newstate;
	os_atomic_rmw_loop2o(thread, cancel_state, oldstate, newstate, seq_cst, {
		newstate = oldstate;
		newstate &= ~mask;
		newstate |= state;
	});
	return oldstate;
}

/*
 * Cancel a thread
 */
PTHREAD_NOEXPORT_VARIANT
int
pthread_cancel(pthread_t thread)
{
#if __DARWIN_UNIX03
	if (__unix_conforming == 0)
		__unix_conforming = 1;
#endif /* __DARWIN_UNIX03 */

	if (!_pthread_is_valid(thread, 0, NULL)) {
		return(ESRCH);
	}

	/* if the thread is a workqueue thread, then return error */
	if (thread->wqthread != 0) {
		return(ENOTSUP);
	}
#if __DARWIN_UNIX03
	int state = os_atomic_or2o(thread, cancel_state, _PTHREAD_CANCEL_PENDING, relaxed);
	if (state & PTHREAD_CANCEL_ENABLE) {
		mach_port_t kport = _pthread_kernel_thread(thread);
		if (kport) __pthread_markcancel(kport);
	}
#else /* __DARWIN_UNIX03 */
	thread->cancel_state |= _PTHREAD_CANCEL_PENDING;
#endif /* __DARWIN_UNIX03 */
	return (0);
}


void
pthread_testcancel(void)
{
	pthread_t self = pthread_self();

#if __DARWIN_UNIX03
	if (__unix_conforming == 0)
		__unix_conforming = 1;
	_pthread_testcancel(self, 1);
#else /* __DARWIN_UNIX03 */
	_pthread_testcancel(self, 0);
#endif /* __DARWIN_UNIX03 */
}

#ifndef BUILDING_VARIANT /* [ */

PTHREAD_NOEXPORT_VARIANT
void
_pthread_exit_if_canceled(int error)
{
	if (((error & 0xff) == EINTR) && __unix_conforming && (__pthread_canceled(0) == 0)) {
		pthread_t self = pthread_self();
		if (self != NULL) {
			self->cancel_error = error;
		}
		pthread_exit(PTHREAD_CANCELED);
	}
}


PTHREAD_NOEXPORT_VARIANT
void
_pthread_testcancel(pthread_t thread, int isconforming)
{
	const int flags = (PTHREAD_CANCEL_ENABLE|_PTHREAD_CANCEL_PENDING);

	int state = os_atomic_load2o(thread, cancel_state, seq_cst);
	if ((state & flags) == flags) {
		pthread_exit(isconforming ? PTHREAD_CANCELED : 0);
	}
}

PTHREAD_NOEXPORT
void
_pthread_markcancel_if_canceled(pthread_t thread, mach_port_t kport)
{
	const int flags = (PTHREAD_CANCEL_ENABLE|_PTHREAD_CANCEL_PENDING);

	int state = os_atomic_or2o(thread, cancel_state,
			_PTHREAD_CANCEL_INITIALIZED, relaxed);
	if ((state & flags) == flags && __unix_conforming) {
		__pthread_markcancel(kport);
	}
}

PTHREAD_NOEXPORT
void *
_pthread_get_exit_value(pthread_t thread, int conforming)
{
	const int flags = (PTHREAD_CANCEL_ENABLE|_PTHREAD_CANCEL_PENDING);
	void *value = thread->exit_value;

	if (conforming) {
		int state = os_atomic_load2o(thread, cancel_state, seq_cst);
		if ((state & flags) == flags) {
			value = PTHREAD_CANCELED;
		}
	}
	return value;
}

/* When a thread exits set the cancellation state to DISABLE and DEFERRED */
PTHREAD_NOEXPORT
void
_pthread_setcancelstate_exit(pthread_t thread, void *value_ptr, int conforming)
{
	_pthread_update_cancel_state(thread,
			_PTHREAD_CANCEL_STATE_MASK | _PTHREAD_CANCEL_TYPE_MASK,
			PTHREAD_CANCEL_DISABLE | PTHREAD_CANCEL_DEFERRED);
	if (value_ptr == PTHREAD_CANCELED) {
		_PTHREAD_LOCK(thread->lock);
		thread->detached |= _PTHREAD_WASCANCEL; // 4597450
		_PTHREAD_UNLOCK(thread->lock);
	}
}

#endif /* !BUILDING_VARIANT ] */

/*
 * Query/update the cancelability 'state' of a thread
 */
PTHREAD_ALWAYS_INLINE
static inline int
_pthread_setcancelstate_internal(int state, int *oldstateptr, int conforming)
{
	pthread_t self;

	switch (state) {
		case PTHREAD_CANCEL_ENABLE:
			if (conforming) {
				__pthread_canceled(1);
			}
			break;
		case PTHREAD_CANCEL_DISABLE:
			if (conforming) {
				__pthread_canceled(2);
			}
			break;
		default:
			return EINVAL;
	}

	self = pthread_self();
	int oldstate = _pthread_update_cancel_state(self, _PTHREAD_CANCEL_STATE_MASK, state);
	if (oldstateptr) {
		*oldstateptr = oldstate & _PTHREAD_CANCEL_STATE_MASK;
	}
	if (!conforming) {
		_pthread_testcancel(self, 0);  /* See if we need to 'die' now... */
	}
	return 0;
}

PTHREAD_NOEXPORT_VARIANT
int
pthread_setcancelstate(int state, int *oldstate)
{
#if __DARWIN_UNIX03
	if (__unix_conforming == 0) {
		__unix_conforming = 1;
	}
	return (_pthread_setcancelstate_internal(state, oldstate, 1));
#else /* __DARWIN_UNIX03 */
	return (_pthread_setcancelstate_internal(state, oldstate, 0));
#endif /* __DARWIN_UNIX03 */
}

/*
 * Query/update the cancelability 'type' of a thread
 */
PTHREAD_NOEXPORT_VARIANT
int
pthread_setcanceltype(int type, int *oldtype)
{
	pthread_t self;

#if __DARWIN_UNIX03
	if (__unix_conforming == 0)
		__unix_conforming = 1;
#endif /* __DARWIN_UNIX03 */

	if ((type != PTHREAD_CANCEL_DEFERRED) &&
	    (type != PTHREAD_CANCEL_ASYNCHRONOUS))
		return EINVAL;
	self = pthread_self();
	int oldstate = _pthread_update_cancel_state(self, _PTHREAD_CANCEL_TYPE_MASK, type);
	if (oldtype) {
		*oldtype = oldstate & _PTHREAD_CANCEL_TYPE_MASK;
	}
#if !__DARWIN_UNIX03
	_pthread_testcancel(self, 0);  /* See if we need to 'die' now... */
#endif /* __DARWIN_UNIX03 */
	return (0);
}


int
pthread_sigmask(int how, const sigset_t * set, sigset_t * oset)
{
#if __DARWIN_UNIX03
	int err = 0;

	if (__pthread_sigmask(how, set, oset) == -1) {
		err = errno;
	}
	return(err);
#else /* __DARWIN_UNIX03 */
	return(__pthread_sigmask(how, set, oset));
#endif /* __DARWIN_UNIX03 */
}

#ifndef BUILDING_VARIANT /* [ */

static void
__posix_join_cleanup(void *arg)
{
	pthread_t thread = (pthread_t)arg;

	_PTHREAD_LOCK(thread->lock);
	/* leave another thread to join */
	thread->joiner = (struct _pthread *)NULL;
	_PTHREAD_UNLOCK(thread->lock);
}

PTHREAD_NOEXPORT PTHREAD_NOINLINE
int
_pthread_join(pthread_t thread, void **value_ptr, int conforming,
		int (*_semwait_signal)(int, int, int, int, __int64_t, __int32_t))
{
	int res = 0;
	pthread_t self = pthread_self();
	kern_return_t kern_res;
	semaphore_t joinsem, death = (semaphore_t)os_get_cached_semaphore();

	if (!_pthread_is_valid(thread, PTHREAD_IS_VALID_LOCK_THREAD, NULL)) {
		res = ESRCH;
		goto out;
	}

	if (thread->sig != _PTHREAD_SIG) {
		res = ESRCH;
	} else if ((thread->detached & PTHREAD_CREATE_DETACHED) ||
			!(thread->detached & PTHREAD_CREATE_JOINABLE) ||
			(thread->joiner != NULL)) {
		res = EINVAL;
	} else if (thread == self || (self != NULL && self->joiner == thread)) {
		res = EDEADLK;
	}
	if (res != 0) {
		_PTHREAD_UNLOCK(thread->lock);
		goto out;
	}

	joinsem = thread->joiner_notify;
	if (joinsem == SEMAPHORE_NULL) {
		thread->joiner_notify = joinsem = death;
		death = MACH_PORT_NULL;
	}
	thread->joiner = self;
	_PTHREAD_UNLOCK(thread->lock);

	if (conforming) {
		/* Wait for it to signal... */
		pthread_cleanup_push(__posix_join_cleanup, (void *)thread);
		do {
			res = _semwait_signal(joinsem, 0, 0, 0, 0, 0);
		} while ((res < 0) && (errno == EINTR));
		pthread_cleanup_pop(0);
	} else {
		/* Wait for it to signal... */
		kern_return_t (*_semaphore_wait)(semaphore_t) =
				(void*)_semwait_signal;
		do {
			kern_res = _semaphore_wait(joinsem);
		} while (kern_res != KERN_SUCCESS);
	}

	os_put_cached_semaphore((os_semaphore_t)joinsem);
	res = _pthread_join_cleanup(thread, value_ptr, conforming);

out:
	if (death) {
		os_put_cached_semaphore(death);
	}
	return res;
}

#endif /* !BUILDING_VARIANT ] */
#endif /* VARIANT_CANCELABLE */

/*
 * Wait for a thread to terminate and obtain its exit value.
 */
int
pthread_join(pthread_t thread, void **value_ptr)
{
#if __DARWIN_UNIX03
	if (__unix_conforming == 0)
		__unix_conforming = 1;

#ifdef VARIANT_CANCELABLE
	_pthread_testcancel(pthread_self(), 1);
#endif /* VARIANT_CANCELABLE */
	return _pthread_join(thread, value_ptr, 1, __semwait_signal);
#else
	return _pthread_join(thread, value_ptr, 0, (void*)semaphore_wait);
#endif /* __DARWIN_UNIX03 */

}

int
pthread_cond_wait(pthread_cond_t *cond,
		  pthread_mutex_t *mutex)
{
	int conforming;
#if __DARWIN_UNIX03

	if (__unix_conforming == 0)
		__unix_conforming = 1;

#ifdef VARIANT_CANCELABLE
	conforming = 1;
#else /* !VARIANT_CANCELABLE */
	conforming = -1;
#endif /* VARIANT_CANCELABLE */
#else /* __DARWIN_UNIX03 */
	conforming = 0;
#endif /* __DARWIN_UNIX03 */
	return (_pthread_cond_wait(cond, mutex, (struct timespec *)NULL, 0, conforming));
}

int
pthread_cond_timedwait(pthread_cond_t *cond,
		       pthread_mutex_t *mutex,
		       const struct timespec *abstime)
{
	int conforming;
#if __DARWIN_UNIX03
	if (__unix_conforming == 0)
		__unix_conforming = 1;

#ifdef VARIANT_CANCELABLE
	conforming = 1;
#else /* !VARIANT_CANCELABLE */
	conforming = -1;
#endif /* VARIANT_CANCELABLE */
#else /* __DARWIN_UNIX03 */
        conforming = 0;
#endif /* __DARWIN_UNIX03 */

	return (_pthread_cond_wait(cond, mutex, abstime, 0, conforming));
}

int
sigwait(const sigset_t * set, int * sig)
{
#if __DARWIN_UNIX03
	int err = 0;

	if (__unix_conforming == 0)
		__unix_conforming = 1;

#ifdef VARIANT_CANCELABLE
	_pthread_testcancel(pthread_self(), 1);
#endif /* VARIANT_CANCELABLE */

	if (__sigwait(set, sig) == -1) {
		err = errno;

#ifdef VARIANT_CANCELABLE
		_pthread_testcancel(pthread_self(), 1);
#endif /* VARIANT_CANCELABLE */

		/*
		 * EINTR that isn't a result of pthread_cancel()
		 * is translated to 0.
		 */
		if (err == EINTR) {
			err = 0;
		}
	}
	return(err);
#else /* __DARWIN_UNIX03 */
	if (__sigwait(set, sig) == -1) {
		/*
		 * EINTR that isn't a result of pthread_cancel()
		 * is translated to 0.
		 */
		if (errno != EINTR) {
			return -1;
		}
	}

	return 0;
#endif /* __DARWIN_UNIX03 */
}

