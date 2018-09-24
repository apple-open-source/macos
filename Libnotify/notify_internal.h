/*
 * Copyright (c) 2012 Apple Inc. All rights reserved.
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

#include <dispatch/dispatch.h>
#include <mach/mach.h>
#include <os/lock.h>
#include <stdatomic.h>
#include <stdint.h>
#include <TargetConditionals.h>

#include "libnotify.h"

struct notify_globals_s
{
	/* global lock */
	os_unfair_lock notify_lock;

	/* notify_check() lock */
	os_unfair_lock check_lock;
	pid_t notify_server_pid;

	atomic_uint_fast32_t client_opts;
	uint32_t saved_opts;

	/* last allocated name id */
	uint64_t name_id;

	dispatch_once_t self_state_once;
	notify_state_t *self_state;

	dispatch_once_t notify_server_port_once;
	mach_port_t notify_server_port;
	mach_port_t saved_server_port;
	
	mach_port_t notify_common_port;
	int notify_common_token;
	dispatch_source_t notify_dispatch_source;
	dispatch_source_t server_proc_source;
	
	dispatch_once_t internal_once;
	table_t *registration_table;
	table_t *name_table;
	atomic_uint_fast32_t token_id;

	dispatch_once_t make_background_send_queue_once;
	dispatch_queue_t background_send_queue;

	/* file descriptor list */
	uint32_t fd_count;
	int *fd_clnt;
	int *fd_srv;
	int *fd_refcount;
	
	/* mach port list */
	uint32_t mp_count;
	mach_port_t *mp_list;
	int *mp_refcount;
	int *mp_mine;
	
	/* shared memory base address */
	uint32_t *shm_base;
};

typedef struct notify_globals_s *notify_globals_t;

// When building xctests we link in the client side framework code so we
// simulate the server calls and can't use the libsystem initializer
#ifdef BUILDING_TESTS
extern kern_return_t _notify_server_register_mach_port_2(mach_port_t, caddr_t, int, mach_port_t);
extern kern_return_t _notify_server_cancel_2(mach_port_t, int);
extern kern_return_t _notify_server_post_2(mach_port_t, caddr_t, uint64_t *, int *, boolean_t);
extern kern_return_t _notify_server_post_3(mach_port_t, uint64_t, boolean_t);
extern kern_return_t _notify_server_post_4(mach_port_t, caddr_t, boolean_t);
extern kern_return_t _notify_server_register_plain_2(mach_port_t, caddr_t, int);
extern kern_return_t _notify_server_register_check_2(mach_port_t, caddr_t, int, int *, int *, uint64_t *, int *);
extern kern_return_t _notify_server_register_signal_2(mach_port_t, caddr_t, int, int);
extern kern_return_t _notify_server_register_file_descriptor_2(mach_port_t, caddr_t, int, mach_port_t);
extern kern_return_t _notify_server_register_plain(mach_port_t, caddr_t, int *, int *);
extern kern_return_t _notify_server_cancel(mach_port_t, int, int *);
extern kern_return_t _notify_server_get_state(mach_port_t, int, uint64_t *, int *);
extern kern_return_t _notify_server_checkin(mach_port_t, uint32_t *, uint32_t *, int *);
#define _NOTIFY_HAS_ALLOC_ONCE 0
#else
#if __has_include(<os/alloc_once_private.h>)
#include <os/alloc_once_private.h>
#if defined(OS_ALLOC_ONCE_KEY_LIBSYSTEM_NOTIFY)
#define _NOTIFY_HAS_ALLOC_ONCE 1
#endif
#endif
#endif // BUILDING_TESTS

__attribute__((visibility("hidden")))
void _notify_init_globals(void * /* notify_globals_t */ globals);

__attribute__((visibility("hidden")))
notify_globals_t _notify_globals_impl(void);

__attribute__((__pure__))
static inline notify_globals_t
_notify_globals(void)
{
#if _NOTIFY_HAS_ALLOC_ONCE
	return (notify_globals_t)os_alloc_once(OS_ALLOC_ONCE_KEY_LIBSYSTEM_NOTIFY,
		sizeof(struct notify_globals_s), &_notify_init_globals);
#else
	return _notify_globals_impl();
#endif
}

__private_extern__ uint32_t _notify_lib_peek(notify_state_t *ns, pid_t pid, int token, int *val);

