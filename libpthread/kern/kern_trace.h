/*
 * Copyright (c) 2000-2003 Apple Computer, Inc. All rights reserved.
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

#ifndef _KERN_TRACE_H_
#define _KERN_TRACE_H_

/* pthread kext, or userspace, kdebug trace points. Defined here and output to
 * /usr/share/misc/pthread.codes during build.
 */

// pthread tracing subclasses
# define _TRACE_SUB_DEFAULT 0
# define _TRACE_SUB_WORKQUEUE 1
# define _TRACE_SUB_MUTEX 2

#ifndef _PTHREAD_BUILDING_CODES_

#include <sys/kdebug.h>

#ifndef DBG_PTHREAD
#define DBG_PTHREAD DBG_WORKQUEUE
#endif

#if KERNEL
extern uint32_t pthread_debug_tracing;

# define PTHREAD_TRACE(x,a,b,c,d,e) \
	{ if (pthread_debug_tracing) { KERNEL_DEBUG_CONSTANT(x, a, b, c, d, e); } }

# define PTHREAD_TRACE1(x,a,b,c,d,e) \
	{ if (pthread_debug_tracing) { KERNEL_DEBUG_CONSTANT1(x, a, b, c, d, e); } }
#endif

# define TRACE_CODE(name, subclass, code) \
	static const int TRACE_##name = KDBG_CODE(DBG_PTHREAD, subclass, code)

#else
/* When not included as a header, this file is pre-processed into perl source to generate
 * the pthread.codes file during build.
 */
# define DBG_PTHREAD 9
# define STR(x) #x

# define TRACE_CODE(name, subclass, code) \
	printf("0x%x\t%s\n", ((DBG_PTHREAD << 24) | ((subclass & 0xff) << 16) | ((code & 0x3fff) << 2)), STR(name))
#endif

/* These defines translate into TRACE_<name> when used in source code, and are
 * pre-processed out to a codes file by the build system.
 */

// "default" trace points
TRACE_CODE(pthread_thread_create, _TRACE_SUB_DEFAULT, 0x10);
TRACE_CODE(pthread_thread_terminate, _TRACE_SUB_DEFAULT, 0x20);
TRACE_CODE(pthread_set_qos_self, _TRACE_SUB_DEFAULT, 0x30);

// workqueue trace points
TRACE_CODE(wq_pthread_exit, _TRACE_SUB_WORKQUEUE, 0x01);
TRACE_CODE(wq_workqueue_exit, _TRACE_SUB_WORKQUEUE, 0x02);
TRACE_CODE(wq_run_nextitem, _TRACE_SUB_WORKQUEUE, 0x03);
TRACE_CODE(wq_runitem, _TRACE_SUB_WORKQUEUE, 0x04);
TRACE_CODE(wq_req_threads, _TRACE_SUB_WORKQUEUE, 0x05);
TRACE_CODE(wq_req_octhreads, _TRACE_SUB_WORKQUEUE, 0x06);
TRACE_CODE(wq_thread_suspend, _TRACE_SUB_WORKQUEUE, 0x07);
TRACE_CODE(wq_thread_park, _TRACE_SUB_WORKQUEUE, 0x08);
TRACE_CODE(wq_thread_block, _TRACE_SUB_WORKQUEUE, 0x9);
TRACE_CODE(wq_new_max_scheduled, _TRACE_SUB_WORKQUEUE, 0xa);
TRACE_CODE(wq_add_timer, _TRACE_SUB_WORKQUEUE, 0xb);
TRACE_CODE(wq_start_add_timer, _TRACE_SUB_WORKQUEUE, 0x0c);
TRACE_CODE(wq_stalled, _TRACE_SUB_WORKQUEUE, 0x0d);
TRACE_CODE(wq_reset_priority, _TRACE_SUB_WORKQUEUE, 0x0e);
TRACE_CODE(wq_thread_yielded, _TRACE_SUB_WORKQUEUE, 0x0f);
TRACE_CODE(wq_delay_octhreads, _TRACE_SUB_WORKQUEUE, 0x10);
TRACE_CODE(wq_overcommitted, _TRACE_SUB_WORKQUEUE, 0x11);
TRACE_CODE(wq_override_start, _TRACE_SUB_WORKQUEUE, 0x12);
TRACE_CODE(wq_override_end, _TRACE_SUB_WORKQUEUE, 0x13);
TRACE_CODE(wq_override_dispatch, _TRACE_SUB_WORKQUEUE, 0x14);
TRACE_CODE(wq_override_reset, _TRACE_SUB_WORKQUEUE, 0x15);

// synch trace points
TRACE_CODE(psynch_mutex_ulock, _TRACE_SUB_MUTEX, 0x0);
TRACE_CODE(psynch_mutex_utrylock_failed, _TRACE_SUB_MUTEX, 0x1);
TRACE_CODE(psynch_mutex_uunlock, _TRACE_SUB_MUTEX, 0x2);
TRACE_CODE(psynch_ksyn_incorrect_owner, _TRACE_SUB_MUTEX, 0x3);

#endif // _KERN_TRACE_H_
