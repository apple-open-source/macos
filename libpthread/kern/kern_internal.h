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

#ifndef _SYS_PTHREAD_INTERNAL_H_
#define _SYS_PTHREAD_INTERNAL_H_

#ifdef KERNEL
#include <kern/thread_call.h>
#include <sys/pthread_shims.h>
#include <sys/queue.h>
#endif

#include "kern/synch_internal.h"
#include "kern/workqueue_internal.h"
#include "kern/kern_trace.h"
#include "pthread/qos.h"
#include "private/qos_private.h"

/* pthread userspace SPI feature checking, these constants are returned from bsdthread_register,
 * as a bitmask, to inform userspace of the supported feature set. Old releases of OS X return
 * from this call either zero or -1, allowing us to return a positive number for feature bits.
 */
#define PTHREAD_FEATURE_DISPATCHFUNC	0x01		/* same as WQOPS_QUEUE_NEWSPISUPP, checks for dispatch function support */
#define PTHREAD_FEATURE_FINEPRIO		0x02		/* are fine grained prioirities available */
#define PTHREAD_FEATURE_BSDTHREADCTL	0x04		/* is the bsdthread_ctl syscall available */
#define PTHREAD_FEATURE_SETSELF			0x08		/* is the BSDTHREAD_CTL_SET_SELF command of bsdthread_ctl available */
#define PTHREAD_FEATURE_QOS_MAINTENANCE	0x10		/* is QOS_CLASS_MAINTENANCE available */
#define PTHREAD_FEATURE_QOS_DEFAULT		0x40000000	/* the kernel supports QOS_CLASS_DEFAULT */

/* pthread bsdthread_ctl sysctl commands */
#define BSDTHREAD_CTL_SET_QOS	0x10	/* bsdthread_ctl(BSDTHREAD_CTL_SET_QOS, thread_port, tsd_entry_addr, 0) */
#define BSDTHREAD_CTL_GET_QOS	0x20	/* bsdthread_ctl(BSDTHREAD_CTL_GET_QOS, thread_port, 0, 0) */
#define BSDTHREAD_CTL_QOS_OVERRIDE_START	0x40	/* bsdthread_ctl(BSDTHREAD_CTL_QOS_OVERRIDE_START, thread_port, priority, 0) */
#define BSDTHREAD_CTL_QOS_OVERRIDE_END		0x80	/* bsdthread_ctl(BSDTHREAD_CTL_QOS_OVERRIDE_END, thread_port, 0, 0) */
#define BSDTHREAD_CTL_SET_SELF	0x100	/* bsdthread_ctl(BSDTHREAD_CTL_SET_SELF, priority, voucher, flags) */
#define BSDTHREAD_CTL_QOS_OVERRIDE_RESET	0x200	/* bsdthread_ctl(BSDTHREAD_CTL_QOS_OVERRIDE_RESET, 0, 0, 0) */
#define BSDTHREAD_CTL_QOS_OVERRIDE_DISPATCH	0x400	/* bsdthread_ctl(BSDTHREAD_CTL_QOS_OVERRIDE_DISPATCH, thread_port, priority, 0) */

/* qos_class_t is mapped into one of these bits in the bitfield, this mapping now exists here because
 * libdispatch requires the QoS class mask of the pthread_priority_t to be a bitfield.
 */
#define __PTHREAD_PRIORITY_CBIT_USER_INTERACTIVE 0x20
#define __PTHREAD_PRIORITY_CBIT_USER_INITIATED 0x10
#define __PTHREAD_PRIORITY_CBIT_DEFAULT 0x8
#define __PTHREAD_PRIORITY_CBIT_UTILITY 0x4
#define __PTHREAD_PRIORITY_CBIT_BACKGROUND 0x2
#define __PTHREAD_PRIORITY_CBIT_MAINTENANCE 0x1
#define __PTHREAD_PRIORITY_CBIT_UNSPECIFIED 0x0

/* Added support for QOS_CLASS_MAINTENANCE */
static inline pthread_priority_t
_pthread_priority_make_newest(qos_class_t qc, int rel, unsigned long flags)
{
	pthread_priority_t cls;
	switch (qc) {
		case QOS_CLASS_USER_INTERACTIVE: cls = __PTHREAD_PRIORITY_CBIT_USER_INTERACTIVE; break;
		case QOS_CLASS_USER_INITIATED: cls = __PTHREAD_PRIORITY_CBIT_USER_INITIATED; break;
		case QOS_CLASS_DEFAULT: cls = __PTHREAD_PRIORITY_CBIT_DEFAULT; break;
		case QOS_CLASS_UTILITY: cls = __PTHREAD_PRIORITY_CBIT_UTILITY; break;
		case QOS_CLASS_BACKGROUND: cls = __PTHREAD_PRIORITY_CBIT_BACKGROUND; break;
		case QOS_CLASS_MAINTENANCE: cls = __PTHREAD_PRIORITY_CBIT_MAINTENANCE; break;
		case QOS_CLASS_UNSPECIFIED:
		default:
			cls = __PTHREAD_PRIORITY_CBIT_UNSPECIFIED;
			rel = 1; // results in priority bits == 0 <rdar://problem/16184900>
			break;
	}

	pthread_priority_t p =
		(flags & _PTHREAD_PRIORITY_FLAGS_MASK) |
		((cls << _PTHREAD_PRIORITY_QOS_CLASS_SHIFT) & _PTHREAD_PRIORITY_QOS_CLASS_MASK) |
		(((uint8_t)rel - 1) & _PTHREAD_PRIORITY_PRIORITY_MASK);

	return p;
}

/* Added support for QOS_CLASS_LEGACY and QOS_CLASS_INHERIT */
static inline pthread_priority_t
_pthread_priority_make_version2(qos_class_t qc, int rel, unsigned long flags)
{
	pthread_priority_t cls;
	switch (qc) {
		case QOS_CLASS_USER_INTERACTIVE: cls = __PTHREAD_PRIORITY_CBIT_USER_INTERACTIVE; break;
		case QOS_CLASS_USER_INITIATED: cls = __PTHREAD_PRIORITY_CBIT_USER_INITIATED; break;
		case QOS_CLASS_DEFAULT: cls = __PTHREAD_PRIORITY_CBIT_DEFAULT; break;
		case QOS_CLASS_UTILITY: cls = __PTHREAD_PRIORITY_CBIT_UTILITY; break;
		case QOS_CLASS_BACKGROUND: cls = __PTHREAD_PRIORITY_CBIT_BACKGROUND; break;
		case QOS_CLASS_UNSPECIFIED:
		default:
			cls = __PTHREAD_PRIORITY_CBIT_UNSPECIFIED;
			rel = 1; // results in priority bits == 0 <rdar://problem/16184900>
			break;
	}

	/*
	 * __PTHREAD_PRIORITY_CBIT_MAINTENANCE was defined as the 0th bit by shifting all the
	 * existing bits to the left by one.  So for backward compatiblity for kernels that does
	 * not support QOS_CLASS_MAINTENANCE, we have to make it up by shifting the cls bit to
	 * right by one.
	 */
	cls >>= 1;

	pthread_priority_t p =
		(flags & _PTHREAD_PRIORITY_FLAGS_MASK) |
		((cls << _PTHREAD_PRIORITY_QOS_CLASS_SHIFT) & _PTHREAD_PRIORITY_QOS_CLASS_MASK) |
		(((uint8_t)rel - 1) & _PTHREAD_PRIORITY_PRIORITY_MASK);

	return p;
}

/* QOS_CLASS_MAINTENANCE is supported */
static inline qos_class_t
_pthread_priority_get_qos_newest(pthread_priority_t priority)
{
	qos_class_t qc;
	switch ((priority & _PTHREAD_PRIORITY_QOS_CLASS_MASK) >> _PTHREAD_PRIORITY_QOS_CLASS_SHIFT) {
		case __PTHREAD_PRIORITY_CBIT_USER_INTERACTIVE: qc = QOS_CLASS_USER_INTERACTIVE; break;
		case __PTHREAD_PRIORITY_CBIT_USER_INITIATED: qc = QOS_CLASS_USER_INITIATED; break;
		case __PTHREAD_PRIORITY_CBIT_DEFAULT: qc = QOS_CLASS_DEFAULT; break;
		case __PTHREAD_PRIORITY_CBIT_UTILITY: qc = QOS_CLASS_UTILITY; break;
		case __PTHREAD_PRIORITY_CBIT_BACKGROUND: qc = QOS_CLASS_BACKGROUND; break;
		case __PTHREAD_PRIORITY_CBIT_MAINTENANCE: qc = QOS_CLASS_MAINTENANCE; break;
		case __PTHREAD_PRIORITY_CBIT_UNSPECIFIED:
		default: qc = QOS_CLASS_UNSPECIFIED; break;
	}
	return qc;
}

/* QOS_CLASS_MAINTENANCE is not supported */
static inline qos_class_t
_pthread_priority_get_qos_version2(pthread_priority_t priority)
{
	qos_class_t qc;
	pthread_priority_t cls;

	cls = (priority & _PTHREAD_PRIORITY_QOS_CLASS_MASK) >> _PTHREAD_PRIORITY_QOS_CLASS_SHIFT;

	/*
	 * __PTHREAD_PRIORITY_CBIT_MAINTENANCE was defined as the 0th bit by shifting all the
	 * existing bits to the left by one.  So for backward compatiblity for kernels that does
	 * not support QOS_CLASS_MAINTENANCE, pthread_priority_make() shifted the cls bit to the
	 * right by one.  Therefore we have to shift it back during decoding the priority bit.
	 */
	cls <<= 1;

	switch (cls) {
		case __PTHREAD_PRIORITY_CBIT_USER_INTERACTIVE: qc = QOS_CLASS_USER_INTERACTIVE; break;
		case __PTHREAD_PRIORITY_CBIT_USER_INITIATED: qc = QOS_CLASS_USER_INITIATED; break;
		case __PTHREAD_PRIORITY_CBIT_DEFAULT: qc = QOS_CLASS_DEFAULT; break;
		case __PTHREAD_PRIORITY_CBIT_UTILITY: qc = QOS_CLASS_UTILITY; break;
		case __PTHREAD_PRIORITY_CBIT_BACKGROUND: qc = QOS_CLASS_BACKGROUND; break;
		case __PTHREAD_PRIORITY_CBIT_UNSPECIFIED:
		default: qc = QOS_CLASS_UNSPECIFIED; break;
	}
	return qc;
}

#define _pthread_priority_get_relpri(priority) \
	((int8_t)((priority & _PTHREAD_PRIORITY_PRIORITY_MASK) >> _PTHREAD_PRIORITY_PRIORITY_SHIFT) + 1)

#define _pthread_priority_get_flags(priority) \
	(priority & _PTHREAD_PRIORITY_FLAGS_MASK)

#define _pthread_priority_split_newest(priority, qos, relpri) \
	({ qos = _pthread_priority_get_qos_newest(priority); \
	   relpri = (qos == QOS_CLASS_UNSPECIFIED) ? 0 : \
		   _pthread_priority_get_relpri(priority); \
	})

#define _pthread_priority_split_version2(priority, qos, relpri) \
	({ qos = _pthread_priority_get_qos_version2(priority); \
	   relpri = (qos == QOS_CLASS_UNSPECIFIED) ? 0 : \
		   _pthread_priority_get_relpri(priority); \
	})

/* <rdar://problem/15969976> Required for backward compatibility on older kernels. */
#define _pthread_priority_make_version1(qos, relpri, flags) \
	(((flags >> 15) & 0xffff0000) | \
	((qos << 8) & 0x0000ff00) | \
	(((uint8_t)relpri - 1) & 0x000000ff))

/* userspace <-> kernel registration struct, for passing data to/from the kext during main thread init. */
struct _pthread_registration_data {
	uint64_t version; /* copy-in */
	uint64_t dispatch_queue_offset; /* copy-in */
	pthread_priority_t main_qos; /* copy-out */
};

#ifdef KERNEL

/* The set of features, from the feature bits above, that we support. */
#define PTHREAD_FEATURE_SUPPORTED	( \
	PTHREAD_FEATURE_DISPATCHFUNC | \
	PTHREAD_FEATURE_FINEPRIO | \
	PTHREAD_FEATURE_BSDTHREADCTL | \
	PTHREAD_FEATURE_SETSELF | \
	PTHREAD_FEATURE_QOS_MAINTENANCE | \
	PTHREAD_FEATURE_QOS_DEFAULT)

extern pthread_callbacks_t pthread_kern;

struct ksyn_waitq_element {
	TAILQ_ENTRY(ksyn_waitq_element) kwe_list;	/* link to other list members */
	void *          kwe_kwqqueue;            	/* queue blocked on */
	uint32_t	kwe_state;			/* state */
	uint32_t        kwe_lockseq;			/* the sequence of the entry */
	uint32_t	kwe_count;			/* upper bound on number of matches still pending */
	uint32_t 	kwe_psynchretval;		/* thread retval */
	void		*kwe_uth;			/* uthread */
	uint64_t	kwe_tid;			/* tid of waiter */
};
typedef struct ksyn_waitq_element * ksyn_waitq_element_t;

pthread_priority_t pthread_qos_class_get_priority(int qos);
int pthread_priority_get_qos_class(pthread_priority_t priority);
int pthread_priority_get_class_index(pthread_priority_t priority);
pthread_priority_t pthread_priority_from_class_index(int index);

#define PTH_DEFAULT_STACKSIZE 512*1024
#define MAX_PTHREAD_SIZE 64*1024

/* exported from the kernel but not present in any headers. */
extern thread_t port_name_to_thread(mach_port_name_t port_name);

/* function declarations for pthread_kext.c */
void pthread_init(void);
void psynch_zoneinit(void);
void _pth_proc_hashinit(proc_t p);
void _pth_proc_hashdelete(proc_t p);
void pth_global_hashinit(void);
void psynch_wq_cleanup(void*, void*);

void _pthread_init(void);
int _fill_procworkqueue(proc_t p, struct proc_workqueueinfo * pwqinfo);
void _workqueue_init_lock(proc_t p);
void _workqueue_destroy_lock(proc_t p);
void _workqueue_exit(struct proc *p);
void _workqueue_mark_exiting(struct proc *p);
void _workqueue_thread_yielded(void);
sched_call_t _workqueue_get_sched_callback(void);

int _bsdthread_create(struct proc *p, user_addr_t user_func, user_addr_t user_funcarg, user_addr_t user_stack, user_addr_t user_pthread, uint32_t flags, user_addr_t *retval);
int _bsdthread_register(struct proc *p, user_addr_t threadstart, user_addr_t wqthread, int pthsize, user_addr_t dummy_value, user_addr_t targetconc_ptr, uint64_t dispatchqueue_offset, int32_t *retval);
int _bsdthread_terminate(struct proc *p, user_addr_t stackaddr, size_t size, uint32_t kthport, uint32_t sem, int32_t *retval);
int _bsdthread_ctl_set_qos(struct proc *p, user_addr_t cmd, mach_port_name_t kport, user_addr_t tsd_priority_addr, user_addr_t arg3, int *retval);
int _bsdthread_ctl_set_self(struct proc *p, user_addr_t cmd, pthread_priority_t priority, mach_port_name_t voucher, _pthread_set_flags_t flags, int *retval);
int _bsdthread_ctl_qos_override_start(struct proc *p, user_addr_t cmd, mach_port_name_t kport, pthread_priority_t priority, user_addr_t arg3, int *retval);
int _bsdthread_ctl_qos_override_end(struct proc *p, user_addr_t cmd, mach_port_name_t kport, user_addr_t arg2, user_addr_t arg3, int *retval);
int _bsdthread_ctl_qos_override_dispatch(struct proc __unused *p, user_addr_t __unused cmd, mach_port_name_t kport, pthread_priority_t priority, user_addr_t arg3, int __unused *retval);
int _bsdthread_ctl_qos_override_reset(struct proc __unused *p, user_addr_t __unused cmd, user_addr_t arg1, user_addr_t arg2, user_addr_t arg3, int __unused *retval);
int _bsdthread_ctl(struct proc *p, user_addr_t cmd, user_addr_t arg1, user_addr_t arg2, user_addr_t arg3, int *retval);
int _thread_selfid(__unused struct proc *p, uint64_t *retval);
int _workq_kernreturn(struct proc *p, int options, user_addr_t item, int arg2, int arg3, int32_t *retval);
int _workq_open(struct proc *p, int32_t *retval);

int _psynch_mutexwait(proc_t p, user_addr_t mutex,  uint32_t mgen, uint32_t  ugen, uint64_t tid, uint32_t flags, uint32_t * retval);
int _psynch_mutexdrop(proc_t p, user_addr_t mutex,  uint32_t mgen, uint32_t  ugen, uint64_t tid, uint32_t flags, uint32_t * retval);
int _psynch_cvbroad(proc_t p, user_addr_t cv, uint64_t cvlsgen, uint64_t cvudgen, uint32_t flags, user_addr_t mutex,  uint64_t mugen, uint64_t tid, uint32_t *retval);
int _psynch_cvsignal(proc_t p, user_addr_t cv, uint64_t cvlsgen, uint32_t cvugen, int thread_port, user_addr_t mutex,  uint64_t mugen, uint64_t tid, uint32_t flags, uint32_t * retval);
int _psynch_cvwait(proc_t p, user_addr_t cv, uint64_t cvlsgen, uint32_t cvugen, user_addr_t mutex,  uint64_t mugen, uint32_t flags, int64_t sec, uint32_t nsec, uint32_t * retval);
int _psynch_cvclrprepost(proc_t p, user_addr_t cv, uint32_t cvgen, uint32_t cvugen, uint32_t cvsgen, uint32_t prepocnt, uint32_t preposeq, uint32_t flags, int *retval);
int _psynch_rw_longrdlock(proc_t p, user_addr_t rwlock, uint32_t lgenval, uint32_t ugenval, uint32_t rw_wc, int flags, uint32_t * retval);
int _psynch_rw_rdlock(proc_t p, user_addr_t rwlock, uint32_t lgenval, uint32_t ugenval, uint32_t rw_wc, int flags, uint32_t *retval);
int _psynch_rw_unlock(proc_t p, user_addr_t rwlock, uint32_t lgenval, uint32_t ugenval, uint32_t rw_wc, int flags, uint32_t *retval);
int _psynch_rw_wrlock(proc_t p, user_addr_t rwlock, uint32_t lgenval, uint32_t ugenval, uint32_t rw_wc, int flags, uint32_t *retval);
int _psynch_rw_yieldwrlock(proc_t p, user_addr_t rwlock, uint32_t lgenval, uint32_t ugenval, uint32_t rw_wc, int flags, uint32_t *retval);

extern lck_grp_attr_t *pthread_lck_grp_attr;
extern lck_grp_t *pthread_lck_grp;
extern lck_attr_t *pthread_lck_attr;
extern lck_mtx_t *pthread_list_mlock;
extern thread_call_t psynch_thcall;

struct uthread* current_uthread(void);

#endif // KERNEL

#endif /* _SYS_PTHREAD_INTERNAL_H_ */

