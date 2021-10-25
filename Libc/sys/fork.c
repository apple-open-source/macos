/*
 * Copyright (c) 2010 Apple Computer, Inc. All rights reserved.
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
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <TargetConditionals.h>
#include <stdbool.h>

#include "_libc_init.h" // for libc_atfork_helper

extern pid_t __fork(void);
extern pid_t __vfork(void);

static void (*_libSystem_atfork_prepare)(void) = 0;
static void (*_libSystem_atfork_parent)(void) = 0;
static void (*_libSystem_atfork_child)(void) = 0;
static void (*_libSystem_atfork_prepare_v2)(unsigned int flags, ...) = 0;
static void (*_libSystem_atfork_parent_v2)(unsigned int flags, ...) = 0;
static void (*_libSystem_atfork_child_v2)(unsigned int flags, ...) = 0;

__private_extern__
void _libc_fork_init(const struct _libc_functions *funcs)
{
	if (funcs->version >= 2) {
		_libSystem_atfork_prepare_v2 = funcs->atfork_prepare_v2;
		_libSystem_atfork_parent_v2 = funcs->atfork_parent_v2;
		_libSystem_atfork_child_v2 = funcs->atfork_child_v2;
	} else {
		_libSystem_atfork_prepare = funcs->atfork_prepare;
		_libSystem_atfork_parent = funcs->atfork_parent;
		_libSystem_atfork_child = funcs->atfork_child;
	}
}

static inline __attribute__((always_inline))
pid_t
_do_fork(bool libsystem_atfork_handlers_only)
{
	int ret;

	int flags = libsystem_atfork_handlers_only ? LIBSYSTEM_ATFORK_HANDLERS_ONLY_FLAG : 0;

	if (_libSystem_atfork_prepare_v2) {
		_libSystem_atfork_prepare_v2(flags);
	} else {
		_libSystem_atfork_prepare();
	}
	// Reader beware: this __fork() call is yet another wrapper around the actual syscall
	// and lives inside libsyscall. The fork syscall needs some cuddling by asm before it's
	// allowed to see the big wide C world.
	ret = __fork();
	if (-1 == ret)
	{
		// __fork already set errno for us
		if (_libSystem_atfork_parent_v2) {
			_libSystem_atfork_parent_v2(flags);
		} else {
			_libSystem_atfork_parent();
		}
		return ret;
	}

	if (0 == ret)
	{
		// We're the child in this part.
		if (_libSystem_atfork_child_v2) {
			_libSystem_atfork_child_v2(flags);
		} else {
			_libSystem_atfork_child();
		}
		return 0;
	}

	if (_libSystem_atfork_parent_v2) {
		_libSystem_atfork_parent_v2(flags);
	} else {
		_libSystem_atfork_parent();
	}
	return ret;
}

pid_t
fork(void)
{
	return _do_fork(false);
}

pid_t
vfork(void)
{
	// vfork() is now just fork().
	// Skip the API pthread_atfork handlers, but do call our own
	// Libsystem_atfork handlers. People are abusing vfork in ways where
	// it matters, e.g. tcsh does all kinds of stuff after the vfork. Sigh.
	return _do_fork(true);
}

