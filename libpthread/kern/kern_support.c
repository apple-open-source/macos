/*
 * Copyright (c) 2000-2012 Apple Inc. All rights reserved.
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
/* Copyright (c) 1995-2005 Apple Computer, Inc. All Rights Reserved */
/*
 *	pthread_synch.c
 */

#define  _PTHREAD_CONDATTR_T
#define  _PTHREAD_COND_T
#define _PTHREAD_MUTEXATTR_T
#define _PTHREAD_MUTEX_T
#define _PTHREAD_RWLOCKATTR_T
#define _PTHREAD_RWLOCK_T

#undef pthread_mutexattr_t
#undef pthread_mutex_t
#undef pthread_condattr_t
#undef pthread_cond_t
#undef pthread_rwlockattr_t
#undef pthread_rwlock_t

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/resourcevar.h>
//#include <sys/proc_internal.h>
#include <sys/kauth.h>
#include <sys/systm.h>
#include <sys/timeb.h>
#include <sys/times.h>
#include <sys/acct.h>
#include <sys/kernel.h>
#include <sys/wait.h>
#include <sys/signalvar.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/stat.h>
#include <sys/lock.h>
#include <sys/kdebug.h>
//#include <sys/sysproto.h>
#include <sys/vm.h>
#include <sys/user.h>		/* for coredump */
#include <sys/proc_info.h>	/* for fill_procworkqueue */


#include <mach/mach_port.h>
#include <mach/mach_types.h>
#include <mach/semaphore.h>
#include <mach/sync_policy.h>
#include <mach/task.h>
#include <mach/vm_prot.h>
#include <kern/kern_types.h>
#include <kern/task.h>
#include <kern/clock.h>
#include <mach/kern_return.h>
#include <kern/thread.h>
#include <kern/sched_prim.h>
#include <kern/kalloc.h>
#include <kern/sched_prim.h>	/* for thread_exception_return */
#include <kern/processor.h>
#include <kern/assert.h>
#include <mach/mach_vm.h>
#include <mach/mach_param.h>
#include <mach/thread_status.h>
#include <mach/thread_policy.h>
#include <mach/message.h>
#include <mach/port.h>
//#include <vm/vm_protos.h>
#include <vm/vm_fault.h>
#include <vm/vm_map.h>
#include <mach/thread_act.h> /* for thread_resume */
#include <machine/machine_routines.h>

#include <libkern/OSAtomic.h>

#include <sys/pthread_shims.h>
#include "kern_internal.h"

uint32_t pthread_debug_tracing = 0;

SYSCTL_INT(_kern, OID_AUTO, pthread_debug_tracing, CTLFLAG_RW | CTLFLAG_LOCKED,
		   &pthread_debug_tracing, 0, "")

// XXX: Dirty import for sys/signarvar.h that's wrapped in BSD_KERNEL_PRIVATE
#define sigcantmask (sigmask(SIGKILL) | sigmask(SIGSTOP))

lck_grp_attr_t   *pthread_lck_grp_attr;
lck_grp_t    *pthread_lck_grp;
lck_attr_t   *pthread_lck_attr;

extern void thread_set_cthreadself(thread_t thread, uint64_t pself, int isLP64);
extern void workqueue_thread_yielded(void);

static boolean_t workqueue_run_nextreq(proc_t p, struct workqueue *wq, thread_t th, boolean_t force_oc,
					boolean_t  overcommit, pthread_priority_t oc_prio);

static boolean_t workqueue_run_one(proc_t p, struct workqueue *wq, boolean_t overcommit, pthread_priority_t priority);

static void wq_runreq(proc_t p, boolean_t overcommit, pthread_priority_t priority, thread_t th, struct threadlist *tl,
		       int reuse_thread, int wake_thread, int return_directly);

static int _setup_wqthread(proc_t p, thread_t th, boolean_t overcommit, pthread_priority_t priority, int reuse_thread, struct threadlist *tl);

static void wq_unpark_continue(void);
static void wq_unsuspend_continue(void);

static boolean_t workqueue_addnewthread(struct workqueue *wq, boolean_t oc_thread);
static void workqueue_removethread(struct threadlist *tl, int fromexit);
static void workqueue_lock_spin(proc_t);
static void workqueue_unlock(proc_t);

int proc_settargetconc(pid_t pid, int queuenum, int32_t targetconc);
int proc_setalltargetconc(pid_t pid, int32_t * targetconcp);

#define WQ_MAXPRI_MIN	0	/* low prio queue num */
#define WQ_MAXPRI_MAX	2	/* max  prio queuenum */
#define WQ_PRI_NUM	3	/* number of prio work queues */

#define C_32_STK_ALIGN          16
#define C_64_STK_ALIGN          16
#define C_64_REDZONE_LEN        128
#define TRUNC_DOWN32(a,c)       ((((uint32_t)a)-(c)) & ((uint32_t)(-(c))))
#define TRUNC_DOWN64(a,c)       ((((uint64_t)a)-(c)) & ((uint64_t)(-(c))))

/*
 * Flags filed passed to bsdthread_create and back in pthread_start 
31  <---------------------------------> 0
_________________________________________
| flags(8) | policy(8) | importance(16) |
-----------------------------------------
*/

#define PTHREAD_START_CUSTOM	0x01000000
#define PTHREAD_START_SETSCHED	0x02000000
#define PTHREAD_START_DETACHED	0x04000000
#define PTHREAD_START_QOSCLASS	0x08000000
#define PTHREAD_START_QOSCLASS_MASK 0xffffff
#define PTHREAD_START_POLICY_BITSHIFT 16
#define PTHREAD_START_POLICY_MASK 0xff
#define PTHREAD_START_IMPORTANCE_MASK 0xffff

#define SCHED_OTHER      POLICY_TIMESHARE
#define SCHED_FIFO       POLICY_FIFO
#define SCHED_RR         POLICY_RR

int
_bsdthread_create(struct proc *p, user_addr_t user_func, user_addr_t user_funcarg, user_addr_t user_stack, user_addr_t user_pthread, uint32_t flags, user_addr_t *retval)
{
	kern_return_t kret;
	void * sright;
	int error = 0;
	int allocated = 0;
	mach_vm_offset_t stackaddr;
	mach_vm_size_t th_allocsize = 0;
	mach_vm_size_t user_stacksize;
	mach_vm_size_t th_stacksize;
	mach_vm_size_t th_guardsize;
	mach_vm_offset_t th_stackaddr;
	mach_vm_offset_t th_stack;
	mach_vm_offset_t th_pthread;
	mach_port_name_t th_thport;
	thread_t th;
	vm_map_t vmap = pthread_kern->current_map();
	task_t ctask = current_task();
	unsigned int policy, importance;
	
	int isLP64 = 0;

	if (pthread_kern->proc_get_register(p) == 0) {
		return EINVAL;
	}

	PTHREAD_TRACE(TRACE_pthread_thread_create | DBG_FUNC_START, flags, 0, 0, 0, 0);

	isLP64 = proc_is64bit(p);
	th_guardsize = vm_map_page_size(vmap);

#if defined(__i386__) || defined(__x86_64__)
	stackaddr = 0xB0000000;
#else
#error Need to define a stack address hint for this architecture
#endif
	kret = pthread_kern->thread_create(ctask, &th);
	if (kret != KERN_SUCCESS)
		return(ENOMEM);
	thread_reference(th);

	sright = (void *)pthread_kern->convert_thread_to_port(th);
	th_thport = pthread_kern->ipc_port_copyout_send(sright, pthread_kern->task_get_ipcspace(ctask));

	if ((flags & PTHREAD_START_CUSTOM) == 0) {		
		th_stacksize = (mach_vm_size_t)user_stack;		/* if it is custom them it is stacksize */
		th_allocsize = th_stacksize + th_guardsize + pthread_kern->proc_get_pthsize(p);

		kret = mach_vm_map(vmap, &stackaddr,
    				th_allocsize,
    				page_size-1,
    				VM_MAKE_TAG(VM_MEMORY_STACK)| VM_FLAGS_ANYWHERE , NULL,
    				0, FALSE, VM_PROT_DEFAULT, VM_PROT_ALL,
    				VM_INHERIT_DEFAULT);
    		if (kret != KERN_SUCCESS)
    			kret = mach_vm_allocate(vmap,
    					&stackaddr, th_allocsize,
    					VM_MAKE_TAG(VM_MEMORY_STACK)| VM_FLAGS_ANYWHERE);
    		if (kret != KERN_SUCCESS) {
			error = ENOMEM;
			goto out;
    		}

		PTHREAD_TRACE(TRACE_pthread_thread_create|DBG_FUNC_NONE, th_allocsize, stackaddr, 0, 2, 0);

		th_stackaddr = stackaddr;
		allocated = 1;
     		/*
		 * The guard page is at the lowest address
     		 * The stack base is the highest address
		 */
		kret = mach_vm_protect(vmap,  stackaddr, th_guardsize, FALSE, VM_PROT_NONE);

    		if (kret != KERN_SUCCESS) { 
			error = ENOMEM;
			goto out1;
    		}
		th_stack = (stackaddr + th_stacksize + th_guardsize);
		th_pthread = (stackaddr + th_stacksize + th_guardsize);
		user_stacksize = th_stacksize;
		
	       /*
		* Pre-fault the first page of the new thread's stack and the page that will
		* contain the pthread_t structure.
		*/	
		vm_fault( vmap,
		  vm_map_trunc_page_mask(th_stack - PAGE_SIZE_64, vm_map_page_mask(vmap)),
		  VM_PROT_READ | VM_PROT_WRITE,
		  FALSE, 
		  THREAD_UNINT, NULL, 0);
		
		vm_fault( vmap,
		  vm_map_trunc_page_mask(th_pthread, vm_map_page_mask(vmap)),
		  VM_PROT_READ | VM_PROT_WRITE,
		  FALSE, 
		  THREAD_UNINT, NULL, 0);
	} else {
		th_stack = user_stack;
		user_stacksize = user_stack;
		th_pthread = user_pthread;

		PTHREAD_TRACE(TRACE_pthread_thread_create|DBG_FUNC_NONE, 0, 0, 0, 3, 0);
	}
	
#if defined(__i386__) || defined(__x86_64__)
	/*
	 * Set up i386 registers & function call.
	 */
	if (isLP64 == 0) {
		x86_thread_state32_t state;
		x86_thread_state32_t *ts = &state;

		ts->eip = (unsigned int)pthread_kern->proc_get_threadstart(p);
		ts->eax = (unsigned int)th_pthread;
		ts->ebx = (unsigned int)th_thport;
		ts->ecx = (unsigned int)user_func;
		ts->edx = (unsigned int)user_funcarg;
		ts->edi = (unsigned int)user_stacksize;
		ts->esi = (unsigned int)flags;
		/*
		 * set stack pointer
		 */
		ts->esp = (int)((vm_offset_t)(th_stack-C_32_STK_ALIGN));

		error = pthread_kern->thread_set_wq_state32(th, (thread_state_t)ts);
		if (error != KERN_SUCCESS) {
			error = EINVAL;
			goto out;
		}
	} else {
		x86_thread_state64_t state64;
		x86_thread_state64_t *ts64 = &state64;

		ts64->rip = (uint64_t)pthread_kern->proc_get_threadstart(p);
		ts64->rdi = (uint64_t)th_pthread;
		ts64->rsi = (uint64_t)(th_thport);
		ts64->rdx = (uint64_t)user_func;
		ts64->rcx = (uint64_t)user_funcarg;
		ts64->r8 = (uint64_t)user_stacksize;
		ts64->r9 = (uint64_t)flags;
		/*
		 * set stack pointer aligned to 16 byte boundary
		 */
		ts64->rsp = (uint64_t)(th_stack - C_64_REDZONE_LEN);

		error = pthread_kern->thread_set_wq_state64(th, (thread_state_t)ts64);
		if (error != KERN_SUCCESS) {
			error = EINVAL;
			goto out;
		}
		
	}
#elif defined(__arm__)
	arm_thread_state_t state;
	arm_thread_state_t *ts = &state;
	
	ts->pc = (int)pthread_kern->proc_get_threadstart(p);
	ts->r[0] = (unsigned int)th_pthread;
	ts->r[1] = (unsigned int)th_thport;
	ts->r[2] = (unsigned int)user_func;
	ts->r[3] = (unsigned int)user_funcarg;
	ts->r[4] = (unsigned int)user_stacksize;
	ts->r[5] = (unsigned int)flags;

	/* Set r7 & lr to 0 for better back tracing */
	ts->r[7] = 0;
	ts->lr = 0;

	/*	
	 * set stack pointer
	 */
	ts->sp = (int)((vm_offset_t)(th_stack-C_32_STK_ALIGN));

	(void) pthread_kern->thread_set_wq_state32(th, (thread_state_t)ts);

#else
#error bsdthread_create  not defined for this architecture
#endif

	if ((flags & PTHREAD_START_SETSCHED) != 0) {
		/* Set scheduling parameters if needed */
		thread_extended_policy_data_t    extinfo;
		thread_precedence_policy_data_t   precedinfo;

		importance = (flags & PTHREAD_START_IMPORTANCE_MASK);
		policy = (flags >> PTHREAD_START_POLICY_BITSHIFT) & PTHREAD_START_POLICY_MASK;

		if (policy == SCHED_OTHER) {
			extinfo.timeshare = 1;
		} else {
			extinfo.timeshare = 0;
		}

		thread_policy_set(th, THREAD_EXTENDED_POLICY, (thread_policy_t)&extinfo, THREAD_EXTENDED_POLICY_COUNT);

#define BASEPRI_DEFAULT 31
		precedinfo.importance = (importance - BASEPRI_DEFAULT);
		thread_policy_set(th, THREAD_PRECEDENCE_POLICY, (thread_policy_t)&precedinfo, THREAD_PRECEDENCE_POLICY_COUNT);
	} else if ((flags & PTHREAD_START_QOSCLASS) != 0) {
		/* Set thread QoS class if requested. */
		pthread_priority_t priority = (pthread_priority_t)(flags & PTHREAD_START_QOSCLASS_MASK);

		thread_qos_policy_data_t qos;
		qos.qos_tier = pthread_priority_get_qos_class(priority);
		qos.tier_importance = (qos.qos_tier == QOS_CLASS_UNSPECIFIED) ? 0 :
				_pthread_priority_get_relpri(priority);

		pthread_kern->thread_policy_set_internal(th, THREAD_QOS_POLICY, (thread_policy_t)&qos, THREAD_QOS_POLICY_COUNT);
	}

	kret = pthread_kern->thread_resume(th);
	if (kret != KERN_SUCCESS) {
		error = EINVAL;
		goto out1;
	}
	thread_deallocate(th);	/* drop the creator reference */

	PTHREAD_TRACE(TRACE_pthread_thread_create|DBG_FUNC_END, error, th_pthread, 0, 0, 0);

	*retval = th_pthread;

	return(0);

out1:
	if (allocated != 0) {
		(void)mach_vm_deallocate(vmap,  stackaddr, th_allocsize);
	}
out:
	(void)pthread_kern->mach_port_deallocate(pthread_kern->task_get_ipcspace(ctask), th_thport);
	(void)thread_terminate(th);
	(void)thread_deallocate(th);
	return(error);
}

int
_bsdthread_terminate(__unused struct proc *p,
		     user_addr_t stackaddr,
		     size_t size,
		     uint32_t kthport,
		     uint32_t sem,
		     __unused int32_t *retval)
{
	mach_vm_offset_t freeaddr;
	mach_vm_size_t freesize;
	kern_return_t kret;

	freeaddr = (mach_vm_offset_t)stackaddr;
	freesize = size;

	PTHREAD_TRACE(TRACE_pthread_thread_terminate|DBG_FUNC_START, freeaddr, freesize, kthport, 0xff, 0);

	if ((freesize != (mach_vm_size_t)0) && (freeaddr != (mach_vm_offset_t)0)) {
		kret = mach_vm_deallocate(pthread_kern->current_map(), freeaddr, freesize);
		if (kret != KERN_SUCCESS) {
			PTHREAD_TRACE(TRACE_pthread_thread_terminate|DBG_FUNC_END, kret, 0, 0, 0, 0);
			return(EINVAL);
		}
	}
	
	(void) thread_terminate(current_thread());
	if (sem != MACH_PORT_NULL) {
		 kret = pthread_kern->semaphore_signal_internal_trap(sem);
		if (kret != KERN_SUCCESS) {
			PTHREAD_TRACE(TRACE_pthread_thread_terminate|DBG_FUNC_END, kret, 0, 0, 0, 0);
			return(EINVAL);
		}
	}
	
	if (kthport != MACH_PORT_NULL) {
		pthread_kern->mach_port_deallocate(pthread_kern->task_get_ipcspace(current_task()), kthport);
	}

	PTHREAD_TRACE(TRACE_pthread_thread_terminate|DBG_FUNC_END, 0, 0, 0, 0, 0);

	pthread_kern->thread_exception_return();
	panic("bsdthread_terminate: still running\n");

	PTHREAD_TRACE(TRACE_pthread_thread_terminate|DBG_FUNC_END, 0, 0xff, 0, 0, 0);

	return(0);
}

int
_bsdthread_register(struct proc *p,
		    user_addr_t threadstart,
		    user_addr_t wqthread,
		    int pthsize,
		    user_addr_t pthread_init_data,
		    user_addr_t targetconc_ptr,
		    uint64_t dispatchqueue_offset,
		    int32_t *retval)
{
	/* prevent multiple registrations */
	if (pthread_kern->proc_get_register(p) != 0) {
		return(EINVAL);
	}
	/* syscall randomizer test can pass bogus values */
	if (pthsize < 0 || pthsize > MAX_PTHREAD_SIZE) {
		return(EINVAL);
	}
	pthread_kern->proc_set_threadstart(p, threadstart);
	pthread_kern->proc_set_wqthread(p, wqthread);
	pthread_kern->proc_set_pthsize(p, pthsize);
	pthread_kern->proc_set_register(p);

	/* if we have pthread_init_data, then we use that and target_concptr (which is an offset) get data. */
	if (pthread_init_data != 0) {
		thread_qos_policy_data_t qos;

		struct _pthread_registration_data data;
		size_t pthread_init_sz = MIN(sizeof(struct _pthread_registration_data), (size_t)targetconc_ptr);

		kern_return_t kr = copyin(pthread_init_data, &data, pthread_init_sz);
		if (kr != KERN_SUCCESS) {
			return EINVAL;
		}

		/* Incoming data from the data structure */
		pthread_kern->proc_set_dispatchqueue_offset(p, data.dispatch_queue_offset);

		/* Outgoing data that userspace expects as a reply */
		if (pthread_kern->qos_main_thread_active()) {
			mach_msg_type_number_t nqos = THREAD_QOS_POLICY_COUNT;
			boolean_t gd = FALSE;

			kr = pthread_kern->thread_policy_get(current_thread(), THREAD_QOS_POLICY, (thread_policy_t)&qos, &nqos, &gd);
			if (kr != KERN_SUCCESS || qos.qos_tier == THREAD_QOS_UNSPECIFIED) {
				/* Unspecified threads means the kernel wants us to impose legacy upon the thread. */
				qos.qos_tier = THREAD_QOS_LEGACY;
				qos.tier_importance = 0;

				kr = pthread_kern->thread_policy_set_internal(current_thread(), THREAD_QOS_POLICY, (thread_policy_t)&qos, THREAD_QOS_POLICY_COUNT);
			}

			if (kr == KERN_SUCCESS) {
				data.main_qos = pthread_qos_class_get_priority(qos.qos_tier);
			} else {
				data.main_qos = _pthread_priority_make_newest(QOS_CLASS_UNSPECIFIED, 0, 0);
			}
		} else {
			data.main_qos = _pthread_priority_make_newest(QOS_CLASS_UNSPECIFIED, 0, 0);
		}

		kr = copyout(&data, pthread_init_data, pthread_init_sz);
		if (kr != KERN_SUCCESS) {
			return EINVAL;
		}
	} else {
		pthread_kern->proc_set_dispatchqueue_offset(p, dispatchqueue_offset);
		pthread_kern->proc_set_targconc(p, targetconc_ptr);
	}

	/* return the supported feature set as the return value. */
	*retval = PTHREAD_FEATURE_SUPPORTED;

	return(0);
}

int
_bsdthread_ctl_set_qos(struct proc *p, user_addr_t __unused cmd, mach_port_name_t kport, user_addr_t tsd_priority_addr, user_addr_t arg3, int *retval)
{
 	kern_return_t kr;
	thread_t th;

	pthread_priority_t priority;

	/* Unused parameters must be zero. */
	if (arg3 != 0) {
		return EINVAL;
	}

	/* QoS is stored in a given slot in the pthread TSD. We need to copy that in and set our QoS based on it. */
	if (proc_is64bit(p)) {
		uint64_t v;
		kr = copyin(tsd_priority_addr, &v, sizeof(v));
		if (kr != KERN_SUCCESS) {
			return kr;
		}
		priority = (int)(v & 0xffffffff);
	} else {
		uint32_t v;
		kr = copyin(tsd_priority_addr, &v, sizeof(v));
		if (kr != KERN_SUCCESS) {
			return kr;
		}
		priority = v;
	}

	if ((th = port_name_to_thread(kport)) == THREAD_NULL) {
		return ESRCH;
	}

	/* <rdar://problem/16211829> Disable pthread_set_qos_class_np() on threads other than pthread_self */
	if (th != current_thread()) {
		thread_deallocate(th);
		return EPERM;
	}

	int rv = _bsdthread_ctl_set_self(p, 0, priority, 0, _PTHREAD_SET_SELF_QOS_FLAG, retval);

	/* Static param the thread, we just set QoS on it, so its stuck in QoS land now. */
	/* pthread_kern->thread_static_param(th, TRUE); */ // see <rdar://problem/16433744>, for details

	thread_deallocate(th);

	return rv;
}

static inline struct threadlist *
util_get_thread_threadlist_entry(thread_t th)
{
	struct uthread *uth = pthread_kern->get_bsdthread_info(th);
	if (uth) {
		struct threadlist *tl = pthread_kern->uthread_get_threadlist(uth);
		return tl;
	}
	return NULL;
}

static inline void
wq_thread_override_reset(thread_t th)
{
	struct uthread *uth = pthread_kern->get_bsdthread_info(th);
	struct threadlist *tl = pthread_kern->uthread_get_threadlist(uth);

	if (tl) {
		/*
		 * Drop all outstanding overrides on this thread, done outside the wq lock
		 * because proc_usynch_thread_qos_remove_override takes a spinlock that
		 * could cause us to panic.
		 */
		uint32_t count = tl->th_dispatch_override_count;
		while (!OSCompareAndSwap(count, 0, &tl->th_dispatch_override_count)) {
			count = tl->th_dispatch_override_count;
		}

		PTHREAD_TRACE(TRACE_wq_override_reset | DBG_FUNC_NONE, tl->th_workq, count, 0, 0, 0);

		for (int i=count; i>0; i--) {
			pthread_kern->proc_usynch_thread_qos_remove_override(uth, 0);
		}
	}
}

int
_bsdthread_ctl_set_self(struct proc *p, user_addr_t __unused cmd, pthread_priority_t priority, mach_port_name_t voucher, _pthread_set_flags_t flags, int __unused *retval)
{
	thread_qos_policy_data_t qos;
	mach_msg_type_number_t nqos = THREAD_QOS_POLICY_COUNT;
	boolean_t gd = FALSE;

	kern_return_t kr;
	int qos_rv = 0, voucher_rv = 0, fixedpri_rv = 0;

	if ((flags & _PTHREAD_SET_SELF_QOS_FLAG) != 0) {
		kr = pthread_kern->thread_policy_get(current_thread(), THREAD_QOS_POLICY, (thread_policy_t)&qos, &nqos, &gd);
		if (kr != KERN_SUCCESS) {
			qos_rv = EINVAL;
			goto voucher;
		}

		/* If we have main-thread QoS then we don't allow a thread to come out of QOS_CLASS_UNSPECIFIED. */
		if (pthread_kern->qos_main_thread_active() && qos.qos_tier == THREAD_QOS_UNSPECIFIED) {
			qos_rv = EPERM;
			goto voucher;
		}

		/* Get the work queue for tracing, also the threadlist for bucket manipluation. */
		struct workqueue *wq = NULL;
		struct threadlist *tl = util_get_thread_threadlist_entry(current_thread());
		if (tl) {
			wq = tl->th_workq;
		}

		PTHREAD_TRACE(TRACE_pthread_set_qos_self | DBG_FUNC_START, wq, qos.qos_tier, qos.tier_importance, 0, 0);

		qos.qos_tier = pthread_priority_get_qos_class(priority);
		qos.tier_importance = (qos.qos_tier == QOS_CLASS_UNSPECIFIED) ? 0 : _pthread_priority_get_relpri(priority);

		kr = pthread_kern->thread_policy_set_internal(current_thread(), THREAD_QOS_POLICY, (thread_policy_t)&qos, THREAD_QOS_POLICY_COUNT);
		if (kr != KERN_SUCCESS) {
			qos_rv = EINVAL;
			goto voucher;
		}

		/* If we're a workqueue, the threadlist item priority needs adjusting, along with the bucket we were running in. */
		if (tl) {
			workqueue_lock_spin(p);

			/* Fix up counters. */
			uint8_t old_bucket = tl->th_priority;
			uint8_t new_bucket = pthread_priority_get_class_index(priority);

			uint32_t old_active = OSAddAtomic(-1, &wq->wq_thactive_count[old_bucket]);
			OSAddAtomic(1, &wq->wq_thactive_count[new_bucket]);

			wq->wq_thscheduled_count[old_bucket]--;
			wq->wq_thscheduled_count[new_bucket]++;

			tl->th_priority = new_bucket;

			/* If we were at the ceiling of non-overcommitted threads for a given bucket, we have to
			 * reevaluate whether we should start more work.
			 */
			if (old_active == wq->wq_reqconc[old_bucket]) {
				/* workqueue_run_nextreq will drop the workqueue lock in all exit paths. */
				(void)workqueue_run_nextreq(p, wq, THREAD_NULL, FALSE, FALSE, 0);
			} else {
				workqueue_unlock(p);
			}
		}

		PTHREAD_TRACE(TRACE_pthread_set_qos_self | DBG_FUNC_END, wq, qos.qos_tier, qos.tier_importance, 0, 0);
	}

voucher:
	if ((flags & _PTHREAD_SET_SELF_VOUCHER_FLAG) != 0) {
		kr = pthread_kern->thread_set_voucher_name(voucher);
		if (kr != KERN_SUCCESS) {
			voucher_rv = ENOENT;
			goto fixedpri;
		}
	}

fixedpri:
	if ((flags & _PTHREAD_SET_SELF_FIXEDPRIORITY_FLAG) != 0) {
		thread_extended_policy_data_t extpol;
		thread_t thread = current_thread();
		
		extpol.timeshare = 0;
		
		struct threadlist *tl = util_get_thread_threadlist_entry(thread);
		if (tl) {
			/* Not allowed on workqueue threads, since there is no symmetric clear function */
			fixedpri_rv = ENOTSUP;
			goto done;
		}

		kr = pthread_kern->thread_policy_set_internal(thread, THREAD_EXTENDED_POLICY, (thread_policy_t)&extpol, THREAD_EXTENDED_POLICY_COUNT);
		if (kr != KERN_SUCCESS) {
			fixedpri_rv = EINVAL;
			goto done;
		}
	}
	
done:
	if (qos_rv && voucher_rv) {
		/* Both failed, give that a unique error. */
		return EBADMSG;
	}

	if (qos_rv) {
		return qos_rv;
	}

	if (voucher_rv) {
		return voucher_rv;
	}

	if (fixedpri_rv) {
		return fixedpri_rv;
	}
	
	return 0;
}

int
_bsdthread_ctl_qos_override_start(struct proc __unused *p, user_addr_t __unused cmd, mach_port_name_t kport, pthread_priority_t priority, user_addr_t arg3, int __unused *retval)
{
	thread_t th;
	int rv = 0;

	if (arg3 != 0) {
		return EINVAL;
	}

	if ((th = port_name_to_thread(kport)) == THREAD_NULL) {
		return ESRCH;
	}

	struct uthread *uth = pthread_kern->get_bsdthread_info(th);
	int override_qos = pthread_priority_get_qos_class(priority);

	struct threadlist *tl = util_get_thread_threadlist_entry(th);
	if (tl) {
		/* Workqueue threads count their overrides, so they can forcibly balance any outstanding
		 * overrides when they return to the kernel.
		 */
		uint32_t o = OSAddAtomic(1, &tl->th_override_count);
		PTHREAD_TRACE(TRACE_wq_override_start | DBG_FUNC_NONE, tl->th_workq, thread_tid(th), o+1, priority, 0);
	}

	/* The only failure case here is if we pass a tid and have it lookup the thread, we pass the uthread, so this all always succeeds. */
	pthread_kern->proc_usynch_thread_qos_add_override(uth, 0, override_qos, TRUE);

	thread_deallocate(th);
	return rv;
}

int
_bsdthread_ctl_qos_override_end(struct proc __unused *p, user_addr_t __unused cmd, mach_port_name_t kport, user_addr_t arg2, user_addr_t arg3, int __unused *retval)
{
	thread_t th;
	int rv = 0;

	if (arg2 != 0 || arg3 != 0) {
		return EINVAL;
	}

	if ((th = port_name_to_thread(kport)) == THREAD_NULL) {
		return ESRCH;
	}

	struct uthread *uth = pthread_kern->get_bsdthread_info(th);

	struct threadlist *tl = util_get_thread_threadlist_entry(th);
	if (tl) {
		uint32_t o = OSAddAtomic(-1, &tl->th_override_count);

		PTHREAD_TRACE(TRACE_wq_override_end | DBG_FUNC_NONE, tl->th_workq, thread_tid(th), o-1, 0, 0);

		if (o == 0) {
			/* underflow! */
			thread_deallocate(th);
			return EFAULT;
		}
	}

	pthread_kern->proc_usynch_thread_qos_remove_override(uth, 0);

	thread_deallocate(th);
	return rv;
}

int
_bsdthread_ctl_qos_override_dispatch(struct proc __unused *p, user_addr_t __unused cmd, mach_port_name_t kport, pthread_priority_t priority, user_addr_t arg3, int __unused *retval)
{
	thread_t th;
	int rv = 0;

	if (arg3 != 0) {
		return EINVAL;
	}

	if ((th = port_name_to_thread(kport)) == THREAD_NULL) {
		return ESRCH;
	}

	struct uthread *uth = pthread_kern->get_bsdthread_info(th);
	int override_qos = pthread_priority_get_qos_class(priority);

	struct threadlist *tl = util_get_thread_threadlist_entry(th);
	if (!tl) {
		thread_deallocate(th);
		return EPERM;
	}

	/* Workqueue threads count their overrides, so they can forcibly balance any outstanding
	 * overrides when they return to the kernel.
	 */
	uint32_t o = OSAddAtomic(1, &tl->th_dispatch_override_count);
	PTHREAD_TRACE(TRACE_wq_override_dispatch | DBG_FUNC_NONE, tl->th_workq, thread_tid(th), o+1, priority, 0);

	/* The only failure case here is if we pass a tid and have it lookup the thread, we pass the uthread, so this all always succeeds. */
	pthread_kern->proc_usynch_thread_qos_add_override(uth, 0, override_qos, TRUE);

	thread_deallocate(th);
	return rv;
}

int
_bsdthread_ctl_qos_override_reset(struct proc __unused *p, user_addr_t __unused cmd, user_addr_t arg1, user_addr_t arg2, user_addr_t arg3, int __unused *retval)
{
	thread_t th;
	struct threadlist *tl;
	int rv = 0;

	if (arg1 != 0 || arg2 != 0 || arg3 != 0) {
		return EINVAL;
	}

	th = current_thread();
	tl = util_get_thread_threadlist_entry(th);

	if (tl) {
		wq_thread_override_reset(th);
	} else {
		rv = EPERM;
	}

	return rv;
}

int
_bsdthread_ctl(struct proc *p, user_addr_t cmd, user_addr_t arg1, user_addr_t arg2, user_addr_t arg3, int *retval)
{
	switch (cmd) {
		case BSDTHREAD_CTL_SET_QOS:
			return _bsdthread_ctl_set_qos(p, cmd, (mach_port_name_t)arg1, arg2, arg3, retval);
		case BSDTHREAD_CTL_QOS_OVERRIDE_START:
			return _bsdthread_ctl_qos_override_start(p, cmd, (mach_port_name_t)arg1, (pthread_priority_t)arg2, arg3, retval);
		case BSDTHREAD_CTL_QOS_OVERRIDE_END:
			return _bsdthread_ctl_qos_override_end(p, cmd, (mach_port_name_t)arg1, arg2, arg3, retval);
		case BSDTHREAD_CTL_QOS_OVERRIDE_RESET:
			return _bsdthread_ctl_qos_override_reset(p, cmd, arg1, arg2, arg3, retval);
		case BSDTHREAD_CTL_QOS_OVERRIDE_DISPATCH:
			return _bsdthread_ctl_qos_override_dispatch(p, cmd, (mach_port_name_t)arg1, (pthread_priority_t)arg2, arg3, retval);
		case BSDTHREAD_CTL_SET_SELF:
			return _bsdthread_ctl_set_self(p, cmd, (pthread_priority_t)arg1, (mach_port_name_t)arg2, (_pthread_set_flags_t)arg3, retval);
		default:
			return EINVAL;
	}
}

uint32_t wq_yielded_threshold		= WQ_YIELDED_THRESHOLD;
uint32_t wq_yielded_window_usecs	= WQ_YIELDED_WINDOW_USECS;
uint32_t wq_stalled_window_usecs	= WQ_STALLED_WINDOW_USECS;
uint32_t wq_reduce_pool_window_usecs	= WQ_REDUCE_POOL_WINDOW_USECS;
uint32_t wq_max_timer_interval_usecs	= WQ_MAX_TIMER_INTERVAL_USECS;
uint32_t wq_max_threads			= WORKQUEUE_MAXTHREADS;
uint32_t wq_max_constrained_threads	= WORKQUEUE_MAXTHREADS / 8;


SYSCTL_INT(_kern, OID_AUTO, wq_yielded_threshold, CTLFLAG_RW | CTLFLAG_LOCKED,
	   &wq_yielded_threshold, 0, "");

SYSCTL_INT(_kern, OID_AUTO, wq_yielded_window_usecs, CTLFLAG_RW | CTLFLAG_LOCKED,
	   &wq_yielded_window_usecs, 0, "");

SYSCTL_INT(_kern, OID_AUTO, wq_stalled_window_usecs, CTLFLAG_RW | CTLFLAG_LOCKED,
	   &wq_stalled_window_usecs, 0, "");

SYSCTL_INT(_kern, OID_AUTO, wq_reduce_pool_window_usecs, CTLFLAG_RW | CTLFLAG_LOCKED,
	   &wq_reduce_pool_window_usecs, 0, "");

SYSCTL_INT(_kern, OID_AUTO, wq_max_timer_interval_usecs, CTLFLAG_RW | CTLFLAG_LOCKED,
	   &wq_max_timer_interval_usecs, 0, "");

SYSCTL_INT(_kern, OID_AUTO, wq_max_threads, CTLFLAG_RW | CTLFLAG_LOCKED,
	   &wq_max_threads, 0, "");

SYSCTL_INT(_kern, OID_AUTO, wq_max_constrained_threads, CTLFLAG_RW | CTLFLAG_LOCKED,
	   &wq_max_constrained_threads, 0, "");


static uint32_t wq_init_constrained_limit = 1;


void
_workqueue_init_lock(proc_t p)
{
	lck_spin_init(pthread_kern->proc_get_wqlockptr(p), pthread_lck_grp, pthread_lck_attr);
	*(pthread_kern->proc_get_wqinitingptr(p)) = FALSE;
}

void
_workqueue_destroy_lock(proc_t p)
{
	lck_spin_destroy(pthread_kern->proc_get_wqlockptr(p), pthread_lck_grp);
}


static void
workqueue_lock_spin(proc_t p)
{
	lck_spin_lock(pthread_kern->proc_get_wqlockptr(p));
}

static void
workqueue_unlock(proc_t p)
{
	lck_spin_unlock(pthread_kern->proc_get_wqlockptr(p));
}


static void
workqueue_interval_timer_start(struct workqueue *wq)
{
	uint64_t deadline;

	if (wq->wq_timer_interval == 0) {
		wq->wq_timer_interval = wq_stalled_window_usecs;

	} else {
		wq->wq_timer_interval = wq->wq_timer_interval * 2;

		if (wq->wq_timer_interval > wq_max_timer_interval_usecs) {
			wq->wq_timer_interval = wq_max_timer_interval_usecs;
		}
	}
	clock_interval_to_deadline(wq->wq_timer_interval, 1000, &deadline);

	thread_call_enter_delayed(wq->wq_atimer_call, deadline);

	PTHREAD_TRACE(TRACE_wq_start_add_timer, wq, wq->wq_reqcount, wq->wq_flags, wq->wq_timer_interval, 0);
}


static boolean_t
wq_thread_is_busy(uint64_t cur_ts, uint64_t *lastblocked_tsp)
{
	clock_sec_t	secs;
	clock_usec_t	usecs;
	uint64_t lastblocked_ts;
	uint64_t elapsed;

	/*
	 * the timestamp is updated atomically w/o holding the workqueue lock
	 * so we need to do an atomic read of the 64 bits so that we don't see
	 * a mismatched pair of 32 bit reads... we accomplish this in an architecturally
	 * independent fashion by using OSCompareAndSwap64 to write back the
	 * value we grabbed... if it succeeds, then we have a good timestamp to
	 * evaluate... if it fails, we straddled grabbing the timestamp while it
	 * was being updated... treat a failed update as a busy thread since
	 * it implies we are about to see a really fresh timestamp anyway
	 */
	lastblocked_ts = *lastblocked_tsp;

	if ( !OSCompareAndSwap64((UInt64)lastblocked_ts, (UInt64)lastblocked_ts, lastblocked_tsp))
		return (TRUE);

	if (lastblocked_ts >= cur_ts) {
		/*
		 * because the update of the timestamp when a thread blocks isn't
		 * serialized against us looking at it (i.e. we don't hold the workq lock)
		 * it's possible to have a timestamp that matches the current time or
		 * that even looks to be in the future relative to when we grabbed the current
		 * time... just treat this as a busy thread since it must have just blocked.
		 */
		return (TRUE);
	}
	elapsed = cur_ts - lastblocked_ts;

	pthread_kern->absolutetime_to_microtime(elapsed, &secs, &usecs);

	if (secs == 0 && usecs < wq_stalled_window_usecs)
		return (TRUE);
	return (FALSE);
}


#define WQ_TIMER_NEEDED(wq, start_timer) do {		\
	int oldflags = wq->wq_flags;			\
							\
	if ( !(oldflags & (WQ_EXITING | WQ_ATIMER_RUNNING))) {	\
		if (OSCompareAndSwap(oldflags, oldflags | WQ_ATIMER_RUNNING, (UInt32 *)&wq->wq_flags)) \
			start_timer = TRUE;			\
	}							\
} while (0)



static void
workqueue_add_timer(struct workqueue *wq, __unused int param1)
{
	proc_t		p;
	boolean_t	start_timer = FALSE;
	boolean_t	retval;
	boolean_t	add_thread;
	uint32_t	busycount;
		
	PTHREAD_TRACE(TRACE_wq_add_timer | DBG_FUNC_START, wq, wq->wq_flags, wq->wq_nthreads, wq->wq_thidlecount, 0);

	p = wq->wq_proc;

	workqueue_lock_spin(p);

	/*
	 * because workqueue_callback now runs w/o taking the workqueue lock
	 * we are unsynchronized w/r to a change in state of the running threads...
	 * to make sure we always evaluate that change, we allow it to start up 
	 * a new timer if the current one is actively evalutating the state
	 * however, we do not need more than 2 timers fired up (1 active and 1 pending)
	 * and we certainly do not want 2 active timers evaluating the state
	 * simultaneously... so use WQL_ATIMER_BUSY to serialize the timers...
	 * note that WQL_ATIMER_BUSY is in a different flag word from WQ_ATIMER_RUNNING since
	 * it is always protected by the workq lock... WQ_ATIMER_RUNNING is evaluated
	 * and set atomimcally since the callback function needs to manipulate it
	 * w/o holding the workq lock...
	 *
	 * !WQ_ATIMER_RUNNING && !WQL_ATIMER_BUSY   ==   no pending timer, no active timer
	 * !WQ_ATIMER_RUNNING && WQL_ATIMER_BUSY    ==   no pending timer, 1 active timer
	 * WQ_ATIMER_RUNNING && !WQL_ATIMER_BUSY    ==   1 pending timer, no active timer
	 * WQ_ATIMER_RUNNING && WQL_ATIMER_BUSY     ==   1 pending timer, 1 active timer
	 */
	while (wq->wq_lflags & WQL_ATIMER_BUSY) {
		wq->wq_lflags |= WQL_ATIMER_WAITING;

		assert_wait((caddr_t)wq, (THREAD_UNINT));
		workqueue_unlock(p);

		thread_block(THREAD_CONTINUE_NULL);

		workqueue_lock_spin(p);
	}
	wq->wq_lflags |= WQL_ATIMER_BUSY;

	/*
	 * the workq lock will protect us from seeing WQ_EXITING change state, but we
	 * still need to update this atomically in case someone else tries to start
	 * the timer just as we're releasing it
	 */
	while ( !(OSCompareAndSwap(wq->wq_flags, (wq->wq_flags & ~WQ_ATIMER_RUNNING), (UInt32 *)&wq->wq_flags)));

again:
	retval = TRUE;
	add_thread = FALSE;

	if ( !(wq->wq_flags & WQ_EXITING)) {
		/*
		 * check to see if the stall frequency was beyond our tolerance
		 * or we have work on the queue, but haven't scheduled any 
		 * new work within our acceptable time interval because
		 * there were no idle threads left to schedule
		 */
		if (wq->wq_reqcount) {
			uint32_t	priclass;
			uint32_t	thactive_count;
			uint32_t	i;
			uint64_t	curtime;

			for (priclass = 0; priclass < WORKQUEUE_NUM_BUCKETS; priclass++) {
				if (wq->wq_requests[priclass])
					break;
			}
			assert(priclass < WORKQUEUE_NUM_BUCKETS);

			curtime = mach_absolute_time();
			busycount = 0;
			thactive_count = 0;

			/*
			 * check for conditions under which we would not add a thread, either
			 *   a) we've got as many running threads as we want in this priority
			 *      band and the priority bands above it
			 *
			 *   b) check to see if the priority group has blocked threads, if the
			 *      last blocked timestamp is old enough, we will have already passed
			 *      (a) where we would have stopped if we had enough active threads.
			 */
			for (i = 0; i <= priclass; i++) {
				
				thactive_count += wq->wq_thactive_count[i];

				if (wq->wq_thscheduled_count[i]) {
					if (wq_thread_is_busy(curtime, &wq->wq_lastblocked_ts[i]))
						busycount++;
				}
			}
			if (thactive_count + busycount < wq->wq_max_concurrency) {

				if (wq->wq_thidlecount == 0) {
					/*
					 * if we have no idle threads, try to add one
					 */
					retval = workqueue_addnewthread(wq, FALSE);
				}
				add_thread = TRUE;
			}

			if (wq->wq_reqcount) {
				/*
				 * as long as we have threads to schedule, and we successfully
				 * scheduled new work, keep trying
				 */
				while (wq->wq_thidlecount && !(wq->wq_flags & WQ_EXITING)) {
					/*
					 * workqueue_run_nextreq is responsible for
					 * dropping the workqueue lock in all cases
					 */
					retval = workqueue_run_nextreq(p, wq, THREAD_NULL, FALSE, FALSE, 0);
					workqueue_lock_spin(p);

					if (retval == FALSE)
						break;
				}
				if ( !(wq->wq_flags & WQ_EXITING) && wq->wq_reqcount) {

					if (wq->wq_thidlecount == 0 && retval == TRUE && add_thread == TRUE)
						goto again;

					if (wq->wq_thidlecount == 0 || busycount)
						WQ_TIMER_NEEDED(wq, start_timer);

					PTHREAD_TRACE(TRACE_wq_add_timer | DBG_FUNC_NONE, wq, wq->wq_reqcount, wq->wq_thidlecount, busycount, 0);
				}
			}
		}
	}
	if ( !(wq->wq_flags & WQ_ATIMER_RUNNING))
		wq->wq_timer_interval = 0;

	wq->wq_lflags &= ~WQL_ATIMER_BUSY;

	if ((wq->wq_flags & WQ_EXITING) || (wq->wq_lflags & WQL_ATIMER_WAITING)) {
		/*
		 * wakeup the thread hung up in workqueue_exit or workqueue_add_timer waiting for this timer
		 * to finish getting out of the way
		 */
		wq->wq_lflags &= ~WQL_ATIMER_WAITING;
		wakeup(wq);
	}

	PTHREAD_TRACE(TRACE_wq_add_timer | DBG_FUNC_END, wq, start_timer, wq->wq_nthreads, wq->wq_thidlecount, 0);

	workqueue_unlock(p);

        if (start_timer == TRUE)
	        workqueue_interval_timer_start(wq);
}


void
_workqueue_thread_yielded(void)
{
	struct workqueue *wq;
	proc_t p;

	p = current_proc();

	if ((wq = pthread_kern->proc_get_wqptr(p)) == NULL || wq->wq_reqcount == 0)
		return;
	
	workqueue_lock_spin(p);

	if (wq->wq_reqcount) {
		uint64_t	curtime;
		uint64_t	elapsed;
		clock_sec_t	secs;
		clock_usec_t	usecs;

		if (wq->wq_thread_yielded_count++ == 0)
			wq->wq_thread_yielded_timestamp = mach_absolute_time();

		if (wq->wq_thread_yielded_count < wq_yielded_threshold) {
			workqueue_unlock(p);
			return;
		}

		PTHREAD_TRACE(TRACE_wq_thread_yielded | DBG_FUNC_START, wq, wq->wq_thread_yielded_count, wq->wq_reqcount, 0, 0);

		wq->wq_thread_yielded_count = 0;

		curtime = mach_absolute_time();
		elapsed = curtime - wq->wq_thread_yielded_timestamp;
		pthread_kern->absolutetime_to_microtime(elapsed, &secs, &usecs);

		if (secs == 0 && usecs < wq_yielded_window_usecs) {

			if (wq->wq_thidlecount == 0) {
				workqueue_addnewthread(wq, TRUE);
				/*
				 * 'workqueue_addnewthread' drops the workqueue lock
				 * when creating the new thread and then retakes it before
				 * returning... this window allows other threads to process
				 * requests, so we need to recheck for available work
				 * if none found, we just return...  the newly created thread
				 * will eventually get used (if it hasn't already)...
				 */
				if (wq->wq_reqcount == 0) {
					workqueue_unlock(p);
					return;
				}
			}
			if (wq->wq_thidlecount) {
				uint32_t	priority;
				boolean_t	overcommit = FALSE;
				boolean_t	force_oc = FALSE;

				for (priority = 0; priority < WORKQUEUE_NUM_BUCKETS; priority++) {
					if (wq->wq_requests[priority]) {
						break;
					}
				}
				assert(priority < WORKQUEUE_NUM_BUCKETS);

				wq->wq_reqcount--;
				wq->wq_requests[priority]--;

				if (wq->wq_ocrequests[priority]) {
					wq->wq_ocrequests[priority]--;
					overcommit = TRUE;
				} else
					force_oc = TRUE;

				(void)workqueue_run_nextreq(p, wq, THREAD_NULL, force_oc, overcommit, pthread_priority_from_class_index(priority));
				/*
				 * workqueue_run_nextreq is responsible for
				 * dropping the workqueue lock in all cases
				 */
				PTHREAD_TRACE(TRACE_wq_thread_yielded | DBG_FUNC_END, wq, wq->wq_thread_yielded_count, wq->wq_reqcount, 1, 0);

				return;
			}
		}
		PTHREAD_TRACE(TRACE_wq_thread_yielded | DBG_FUNC_END, wq, wq->wq_thread_yielded_count, wq->wq_reqcount, 2, 0);
	}
	workqueue_unlock(p);
}



static void
workqueue_callback(int type, thread_t thread)
{
	struct uthread    *uth;
	struct threadlist *tl;
	struct workqueue  *wq;

	uth = pthread_kern->get_bsdthread_info(thread);
	tl = pthread_kern->uthread_get_threadlist(uth);
	wq = tl->th_workq;

	switch (type) {
	case SCHED_CALL_BLOCK: {
		uint32_t	old_activecount;
		boolean_t	start_timer = FALSE;

		old_activecount = OSAddAtomic(-1, &wq->wq_thactive_count[tl->th_priority]);

		if (old_activecount == wq->wq_reqconc[tl->th_priority]) {
			uint64_t	curtime;
			UInt64		*lastblocked_ptr;

			/*
			 * the number of active threads at this priority
			 * has fallen below the maximum number of concurrent
			 * threads that we're allowed to run
			 */
			lastblocked_ptr = (UInt64 *)&wq->wq_lastblocked_ts[tl->th_priority];
			curtime = mach_absolute_time();

			/*
			 * if we collide with another thread trying to update the last_blocked (really unlikely
			 * since another thread would have to get scheduled and then block after we start down
			 * this path), it's not a problem.  Either timestamp is adequate, so no need to retry
			 */

			OSCompareAndSwap64(*lastblocked_ptr, (UInt64)curtime, lastblocked_ptr);

			if (wq->wq_reqcount) {
				/*
				 * we have work to do so start up the timer
				 * if it's not running... we'll let it sort
				 * out whether we really need to start up
				 * another thread
				 */
				WQ_TIMER_NEEDED(wq, start_timer);
			}

			if (start_timer == TRUE) {
				workqueue_interval_timer_start(wq);
			}
		}
		PTHREAD_TRACE1(TRACE_wq_thread_block | DBG_FUNC_START, wq, old_activecount, tl->th_priority, start_timer, thread_tid(thread));
		break;
	}
	case SCHED_CALL_UNBLOCK:
		/*
		 * we cannot take the workqueue_lock here...
		 * an UNBLOCK can occur from a timer event which
		 * is run from an interrupt context... if the workqueue_lock
		 * is already held by this processor, we'll deadlock...
		 * the thread lock for the thread being UNBLOCKED
		 * is also held
		 */
		OSAddAtomic(1, &wq->wq_thactive_count[tl->th_priority]);
		
		PTHREAD_TRACE1(TRACE_wq_thread_block | DBG_FUNC_END, wq, wq->wq_threads_scheduled, tl->th_priority, 0, thread_tid(thread));
		
		break;
	}
}

sched_call_t
_workqueue_get_sched_callback(void)
{
	return workqueue_callback;
}

static void
workqueue_removethread(struct threadlist *tl, int fromexit)
{
	struct workqueue *wq;
	struct uthread * uth;

	/* 
	 * If fromexit is set, the call is from workqueue_exit(,
	 * so some cleanups are to be avoided.
	 */
	wq = tl->th_workq;

	TAILQ_REMOVE(&wq->wq_thidlelist, tl, th_entry);

	if (fromexit == 0) {
		wq->wq_nthreads--;
		wq->wq_thidlecount--;
	}

	/*
	 * Clear the threadlist pointer in uthread so 
	 * blocked thread on wakeup for termination will
	 * not access the thread list as it is going to be
	 * freed.
	 */
	pthread_kern->thread_sched_call(tl->th_thread, NULL);

	uth = pthread_kern->get_bsdthread_info(tl->th_thread);
	if (uth != (struct uthread *)0) {
		pthread_kern->uthread_set_threadlist(uth, NULL);
	}
	if (fromexit == 0) {
		/* during exit the lock is not held */
		workqueue_unlock(wq->wq_proc);
	}

	if ( (tl->th_flags & TH_LIST_SUSPENDED) ) {
		/*
		 * thread was created, but never used... 
		 * need to clean up the stack and port ourselves
		 * since we're not going to spin up through the
		 * normal exit path triggered from Libc
		 */
		if (fromexit == 0) {
			/* vm map is already deallocated when this is called from exit */
			(void)mach_vm_deallocate(wq->wq_map, tl->th_stackaddr, tl->th_allocsize);
		}
		(void)pthread_kern->mach_port_deallocate(pthread_kern->task_get_ipcspace(wq->wq_task), tl->th_thport);

		PTHREAD_TRACE1(TRACE_wq_thread_suspend | DBG_FUNC_END, wq, (uintptr_t)thread_tid(current_thread()), wq->wq_nthreads, 0xdead, thread_tid(tl->th_thread));
	} else {

		PTHREAD_TRACE1(TRACE_wq_thread_park | DBG_FUNC_END, wq, (uintptr_t)thread_tid(current_thread()), wq->wq_nthreads, 0xdead, thread_tid(tl->th_thread));
	}
	/*
	 * drop our ref on the thread
	 */
	thread_deallocate(tl->th_thread);

	kfree(tl, sizeof(struct threadlist));
}


/*
 * called with workq lock held
 * dropped and retaken around thread creation
 * return with workq lock held
 */
static boolean_t
workqueue_addnewthread(struct workqueue *wq, boolean_t oc_thread)
{
	struct threadlist *tl;
	struct uthread	*uth;
	kern_return_t	kret;
	thread_t	th;
	proc_t		p;
	void 	 	*sright;
	mach_vm_offset_t stackaddr;
	mach_vm_size_t guardsize;

	if ((wq->wq_flags & WQ_EXITING) == WQ_EXITING)
		return (FALSE);

	if (wq->wq_nthreads >= wq_max_threads || wq->wq_nthreads >= (pthread_kern->config_thread_max - 20)) {
		wq->wq_lflags |= WQL_EXCEEDED_TOTAL_THREAD_LIMIT;
		return (FALSE);
	}
	wq->wq_lflags &= ~WQL_EXCEEDED_TOTAL_THREAD_LIMIT;

	if (oc_thread == FALSE && wq->wq_constrained_threads_scheduled >= wq_max_constrained_threads) {
		/*
		 * if we're not creating this thread to service an overcommit request,
		 * then check the size of the constrained thread pool...  if we've already
		 * reached our max for threads scheduled from this pool, don't create a new
		 * one... the callers of this function are prepared for failure.
		 */
		wq->wq_lflags |= WQL_EXCEEDED_CONSTRAINED_THREAD_LIMIT;
		return (FALSE);
	}
	if (wq->wq_constrained_threads_scheduled < wq_max_constrained_threads)
		wq->wq_lflags &= ~WQL_EXCEEDED_CONSTRAINED_THREAD_LIMIT;

	wq->wq_nthreads++;

	p = wq->wq_proc;
	workqueue_unlock(p);

	kret = pthread_kern->thread_create_workq(wq->wq_task, (thread_continue_t)wq_unsuspend_continue, &th);
 	if (kret != KERN_SUCCESS) {
		goto failed;
	}

	tl = kalloc(sizeof(struct threadlist));
	bzero(tl, sizeof(struct threadlist));

#if defined(__i386__) || defined(__x86_64__)
	stackaddr = 0xB0000000;
#else
#error Need to define a stack address hint for this architecture
#endif
	
	guardsize = vm_map_page_size(wq->wq_map);
	tl->th_allocsize = PTH_DEFAULT_STACKSIZE + guardsize + pthread_kern->proc_get_pthsize(p);

	kret = mach_vm_map(wq->wq_map, &stackaddr,
    			tl->th_allocsize,
    			page_size-1,
    			VM_MAKE_TAG(VM_MEMORY_STACK)| VM_FLAGS_ANYWHERE , NULL,
    			0, FALSE, VM_PROT_DEFAULT, VM_PROT_ALL,
    			VM_INHERIT_DEFAULT);

	if (kret != KERN_SUCCESS) {
	        kret = mach_vm_allocate(wq->wq_map,
    					&stackaddr, tl->th_allocsize,
    					VM_MAKE_TAG(VM_MEMORY_STACK) | VM_FLAGS_ANYWHERE);
	}
	if (kret == KERN_SUCCESS) {
	        /*
		 * The guard page is at the lowest address
		 * The stack base is the highest address
		 */
	        kret = mach_vm_protect(wq->wq_map, stackaddr, guardsize, FALSE, VM_PROT_NONE);

		if (kret != KERN_SUCCESS)
		        (void) mach_vm_deallocate(wq->wq_map, stackaddr, tl->th_allocsize);
	}
	if (kret != KERN_SUCCESS) {
		(void) thread_terminate(th);
		thread_deallocate(th);

		kfree(tl, sizeof(struct threadlist));
		goto failed;
	}
	thread_reference(th);

	sright = (void *)pthread_kern->convert_thread_to_port(th);
	tl->th_thport = pthread_kern->ipc_port_copyout_send(sright, pthread_kern->task_get_ipcspace(wq->wq_task));

	pthread_kern->thread_static_param(th, TRUE);

	tl->th_flags = TH_LIST_INITED | TH_LIST_SUSPENDED;

	tl->th_thread = th;
	tl->th_workq = wq;
	tl->th_stackaddr = stackaddr;
	tl->th_priority = WORKQUEUE_NUM_BUCKETS;
	tl->th_policy = -1;

	uth = pthread_kern->get_bsdthread_info(tl->th_thread);

	workqueue_lock_spin(p);

	pthread_kern->uthread_set_threadlist(uth, tl);
	TAILQ_INSERT_TAIL(&wq->wq_thidlelist, tl, th_entry);

	wq->wq_thidlecount++;

	PTHREAD_TRACE1(TRACE_wq_thread_suspend | DBG_FUNC_START, wq, wq->wq_nthreads, 0, thread_tid(current_thread()), thread_tid(tl->th_thread));

	return (TRUE);

failed:
	workqueue_lock_spin(p);
	wq->wq_nthreads--;

	return (FALSE);
}


int
_workq_open(struct proc *p, __unused int32_t *retval)
{
	struct workqueue * wq;
	int wq_size;
	char * ptr;
	uint32_t i;
	uint32_t num_cpus;
	int error = 0;
	boolean_t need_wakeup = FALSE;

	if (pthread_kern->proc_get_register(p) == 0) {
		return EINVAL;
	}

	num_cpus = pthread_kern->ml_get_max_cpus();

	if (wq_init_constrained_limit) {
		uint32_t limit;
		/*
		 * set up the limit for the constrained pool
		 * this is a virtual pool in that we don't
		 * maintain it on a separate idle and run list
		 */
		limit = num_cpus * WORKQUEUE_CONSTRAINED_FACTOR;

		if (limit > wq_max_constrained_threads)
			wq_max_constrained_threads = limit;

		wq_init_constrained_limit = 0;
	}
	workqueue_lock_spin(p);

	if (pthread_kern->proc_get_wqptr(p) == NULL) {

		while (*pthread_kern->proc_get_wqinitingptr(p) == TRUE) {

			assert_wait((caddr_t)pthread_kern->proc_get_wqinitingptr(p), THREAD_UNINT);
			workqueue_unlock(p);

			thread_block(THREAD_CONTINUE_NULL);

			workqueue_lock_spin(p);
		}
		if (pthread_kern->proc_get_wqptr(p) != NULL) {
			goto out;
		}

		*(pthread_kern->proc_get_wqinitingptr(p)) = TRUE;

		workqueue_unlock(p);

		wq_size = sizeof(struct workqueue);

		ptr = (char *)kalloc(wq_size);
		bzero(ptr, wq_size);

		wq = (struct workqueue *)ptr;
		wq->wq_flags = WQ_LIST_INITED;
		wq->wq_proc = p;
		wq->wq_max_concurrency = num_cpus;
		wq->wq_task = current_task();
		wq->wq_map  = pthread_kern->current_map();

		for (i = 0; i < WORKQUEUE_NUM_BUCKETS; i++)
			wq->wq_reqconc[i] = (uint16_t)wq->wq_max_concurrency;

		TAILQ_INIT(&wq->wq_thrunlist);
		TAILQ_INIT(&wq->wq_thidlelist);

		wq->wq_atimer_call = thread_call_allocate((thread_call_func_t)workqueue_add_timer, (thread_call_param_t)wq);

		workqueue_lock_spin(p);

		pthread_kern->proc_set_wqptr(p, wq);
		pthread_kern->proc_set_wqsize(p, wq_size);

		*(pthread_kern->proc_get_wqinitingptr(p)) = FALSE;
		need_wakeup = TRUE;
	}
out:
	workqueue_unlock(p);

	if (need_wakeup == TRUE) {
		wakeup(pthread_kern->proc_get_wqinitingptr(p));
	}
	return(error);
}


int
_workq_kernreturn(struct proc *p,
		  int options,
		  __unused user_addr_t item,
		  int arg2,
		  int arg3,
		  __unused int32_t *retval)
{
	struct workqueue *wq;
	int error	= 0;

	if (pthread_kern->proc_get_register(p) == 0) {
		return EINVAL;
	}

	switch (options) {
	case WQOPS_QUEUE_NEWSPISUPP: {
		/*
		 * arg2 = offset of serialno into dispatch queue
		 */
		int offset = arg2;

		pthread_kern->proc_set_dispatchqueue_serialno_offset(p, (uint64_t)offset);
		break;
	}
	case WQOPS_QUEUE_REQTHREADS: {
		/*
		 * arg2 = number of threads to start
		 * arg3 = priority
		 */
		boolean_t overcommit = FALSE;
		int reqcount	     = arg2;
		pthread_priority_t priority = arg3;
		int class;

		overcommit = (_pthread_priority_get_flags(priority) & _PTHREAD_PRIORITY_OVERCOMMIT_FLAG) != 0;
		class = pthread_priority_get_class_index(priority);

		if ((reqcount <= 0) || (class < 0) || (class >= WORKQUEUE_NUM_BUCKETS)) {
			error = EINVAL;
			break;
		}

		workqueue_lock_spin(p);

		if ((wq = (struct workqueue *)pthread_kern->proc_get_wqptr(p)) == NULL) {
			workqueue_unlock(p);

			error = EINVAL;
			break;
		}

		if (!overcommit) {
			wq->wq_reqcount += reqcount;
			wq->wq_requests[class] += reqcount;

			PTHREAD_TRACE(TRACE_wq_req_threads | DBG_FUNC_NONE, wq, priority, wq->wq_requests[class], reqcount, 0);

			while (wq->wq_reqcount) {
				if (!workqueue_run_one(p, wq, overcommit, priority))
					break;
			}
		} else {
			PTHREAD_TRACE(TRACE_wq_req_octhreads | DBG_FUNC_NONE, wq, priority, wq->wq_requests[class], reqcount, 0);

			while (reqcount) {
				if (!workqueue_run_one(p, wq, overcommit, priority))
					break;
				reqcount--;
			}
			if (reqcount) {
				/*
				 * we need to delay starting some of the overcommit requests...
				 * we should only fail to create the overcommit threads if
				 * we're at the max thread limit... as existing threads
				 * return to the kernel, we'll notice the ocrequests
				 * and spin them back to user space as the overcommit variety
				 */
				wq->wq_reqcount += reqcount;
				wq->wq_requests[class] += reqcount;
				wq->wq_ocrequests[class] += reqcount;

				PTHREAD_TRACE(TRACE_wq_delay_octhreads | DBG_FUNC_NONE, wq, priority, wq->wq_requests[class], reqcount, 0);
			}
		}
		workqueue_unlock(p);
		break;
	}

	case WQOPS_THREAD_RETURN: {
		thread_t th = current_thread();
		struct uthread *uth = pthread_kern->get_bsdthread_info(th);
		struct threadlist *tl = util_get_thread_threadlist_entry(th);

		/* reset signal mask on the workqueue thread to default state */
		if (pthread_kern->uthread_get_sigmask(uth) != (sigset_t)(~workq_threadmask)) {
			pthread_kern->proc_lock(p);
			pthread_kern->uthread_set_sigmask(uth, ~workq_threadmask);
			pthread_kern->proc_unlock(p);
		}

		/* dropping WQ override counts has to be done outside the wq lock. */
		wq_thread_override_reset(th);

		workqueue_lock_spin(p);

		if ((wq = (struct workqueue *)pthread_kern->proc_get_wqptr(p)) == NULL || !tl) {
			workqueue_unlock(p);
			
			error = EINVAL;
			break;
		}
		PTHREAD_TRACE(TRACE_wq_runitem | DBG_FUNC_END, wq, 0, 0, 0, 0);


		(void)workqueue_run_nextreq(p, wq, th, FALSE, FALSE, 0);
		/*
		 * workqueue_run_nextreq is responsible for
		 * dropping the workqueue lock in all cases
		 */
		break;
	}

	default:
		error = EINVAL;
		break;
	}
	return (error);
}

/*
 * Routine:	workqueue_mark_exiting
 *
 * Function:	Mark the work queue such that new threads will not be added to the
 *		work queue after we return.  
 *
 * Conditions:	Called against the current process.
 */
void
_workqueue_mark_exiting(struct proc *p)
{
	struct workqueue *wq = pthread_kern->proc_get_wqptr(p);

	if (wq != NULL) {

		PTHREAD_TRACE(TRACE_wq_pthread_exit|DBG_FUNC_START, wq, 0, 0, 0, 0);

		workqueue_lock_spin(p);

		/*
		 * we now arm the timer in the callback function w/o holding the workq lock...
		 * we do this by setting  WQ_ATIMER_RUNNING via OSCompareAndSwap in order to 
		 * insure only a single timer if running and to notice that WQ_EXITING has
		 * been set (we don't want to start a timer once WQ_EXITING is posted)
		 *
		 * so once we have successfully set WQ_EXITING, we cannot fire up a new timer...
		 * therefor no need to clear the timer state atomically from the flags
		 *
		 * since we always hold the workq lock when dropping WQ_ATIMER_RUNNING
		 * the check for and sleep until clear is protected
		 */
		while (!(OSCompareAndSwap(wq->wq_flags, (wq->wq_flags | WQ_EXITING), (UInt32 *)&wq->wq_flags)));

		if (wq->wq_flags & WQ_ATIMER_RUNNING) {
			if (thread_call_cancel(wq->wq_atimer_call) == TRUE) {
				wq->wq_flags &= ~WQ_ATIMER_RUNNING;
			}
		}
		while ((wq->wq_flags & WQ_ATIMER_RUNNING) || (wq->wq_lflags & WQL_ATIMER_BUSY)) {
			assert_wait((caddr_t)wq, (THREAD_UNINT));
			workqueue_unlock(p);

			thread_block(THREAD_CONTINUE_NULL);

			workqueue_lock_spin(p);
		}
		workqueue_unlock(p);

		PTHREAD_TRACE(TRACE_wq_pthread_exit|DBG_FUNC_END, 0, 0, 0, 0, 0);
	}
}

/*
 * Routine:	workqueue_exit
 *
 * Function:	clean up the work queue structure(s) now that there are no threads
 *		left running inside the work queue (except possibly current_thread).
 *
 * Conditions:	Called by the last thread in the process.
 *		Called against current process.
 */
void
_workqueue_exit(struct proc *p)
{
	struct workqueue  * wq;
	struct threadlist  * tl, *tlist;
	struct uthread	*uth;
	int wq_size = 0;

	wq = pthread_kern->proc_get_wqptr(p);
	if (wq != NULL) {

		PTHREAD_TRACE(TRACE_wq_workqueue_exit|DBG_FUNC_START, wq, 0, 0, 0, 0);

		wq_size = pthread_kern->proc_get_wqsize(p);
		pthread_kern->proc_set_wqptr(p, NULL);
		pthread_kern->proc_set_wqsize(p, 0);

		/*
		 * Clean up workqueue data structures for threads that exited and
		 * didn't get a chance to clean up after themselves.
		 */
		TAILQ_FOREACH_SAFE(tl, &wq->wq_thrunlist, th_entry, tlist) {
			pthread_kern->thread_sched_call(tl->th_thread, NULL);

			uth = pthread_kern->get_bsdthread_info(tl->th_thread);
			if (uth != (struct uthread *)0) {
				pthread_kern->uthread_set_threadlist(uth, NULL);
			}
			TAILQ_REMOVE(&wq->wq_thrunlist, tl, th_entry);

			/*
			 * drop our last ref on the thread
			 */
			thread_deallocate(tl->th_thread);

			kfree(tl, sizeof(struct threadlist));
		}
		TAILQ_FOREACH_SAFE(tl, &wq->wq_thidlelist, th_entry, tlist) {
			workqueue_removethread(tl, 1);
		}
		thread_call_free(wq->wq_atimer_call);

		kfree(wq, wq_size);

		PTHREAD_TRACE(TRACE_wq_workqueue_exit|DBG_FUNC_END, 0, 0, 0, 0, 0);
	}
}


static boolean_t
workqueue_run_one(proc_t p, struct workqueue *wq, boolean_t overcommit, pthread_priority_t priority)
{
	boolean_t	ran_one;

	if (wq->wq_thidlecount == 0) {
		if (overcommit == FALSE) {
			if (wq->wq_constrained_threads_scheduled < wq->wq_max_concurrency)
				workqueue_addnewthread(wq, overcommit);
		} else {
			workqueue_addnewthread(wq, overcommit);

			if (wq->wq_thidlecount == 0)
				return (FALSE);
		}
	}
	ran_one = workqueue_run_nextreq(p, wq, THREAD_NULL, FALSE, overcommit, priority);
	/*
	 * workqueue_run_nextreq is responsible for
	 * dropping the workqueue lock in all cases
	 */
	workqueue_lock_spin(p);

	return (ran_one);
}



/*
 * workqueue_run_nextreq:
 *   called with the workqueue lock held...
 *   responsible for dropping it in all cases
 */
static boolean_t
workqueue_run_nextreq(proc_t p, struct workqueue *wq, thread_t thread,
		      boolean_t force_oc, boolean_t overcommit, pthread_priority_t oc_prio)
{
	thread_t th_to_run = THREAD_NULL;
	thread_t th_to_park = THREAD_NULL;
	int wake_thread = 0;
	int reuse_thread = WQ_FLAG_THREAD_REUSE;
	uint32_t priclass, orig_class;
	uint32_t us_to_wait;
	struct threadlist *tl = NULL;
	struct uthread *uth = NULL;
	boolean_t start_timer = FALSE;
	boolean_t adjust_counters = TRUE;
	uint64_t	curtime;
	uint32_t	thactive_count;
	uint32_t	busycount;

	PTHREAD_TRACE(TRACE_wq_run_nextitem|DBG_FUNC_START, wq, thread, wq->wq_thidlecount, wq->wq_reqcount, 0);

	if (thread != THREAD_NULL) {
		uth = pthread_kern->get_bsdthread_info(thread);

		if ((tl = pthread_kern->uthread_get_threadlist(uth)) == NULL) {
			panic("wq thread with no threadlist");
		}
	}

	/*
	 * from here until we drop the workq lock
	 * we can't be pre-empted since we hold 
	 * the lock in spin mode... this is important
	 * since we have to independently update the priority that 
	 * the thread is associated with and the priorty based
	 * counters that "workqueue_callback" also changes and bases
	 * decisons on.
	 */
dispatch_overcommit:

	if (overcommit || force_oc) {
		priclass = pthread_priority_get_class_index(oc_prio);

		if (thread != THREAD_NULL) {
			th_to_run = thread;
			goto pick_up_work;
		}
		goto grab_idle_thread;
	}
	if (wq->wq_reqcount) {
		for (priclass = 0; priclass < WORKQUEUE_NUM_BUCKETS; priclass++) {
			if (wq->wq_requests[priclass])
				break;
		}
		assert(priclass < WORKQUEUE_NUM_BUCKETS);

		if (wq->wq_ocrequests[priclass] && (thread != THREAD_NULL || wq->wq_thidlecount)) {
			/*
			 * handle delayed overcommit request...
			 * they have priority over normal requests
			 * within a given priority level
			 */
			wq->wq_reqcount--;
			wq->wq_requests[priclass]--;
			wq->wq_ocrequests[priclass]--;

			oc_prio = pthread_priority_from_class_index(priclass);
			overcommit = TRUE;

			goto dispatch_overcommit;
		}
	}
	/*
	 * if we get here, the work should be handled by a constrained thread
	 */
	if (wq->wq_reqcount == 0 || wq->wq_constrained_threads_scheduled >= wq_max_constrained_threads) {
		/*
		 * no work to do, or we're already at or over the scheduling limit for
		 * constrained threads...  just return or park the thread...
		 * do not start the timer for this condition... if we don't have any work,
		 * we'll check again when new work arrives... if we're over the limit, we need 1 or more
		 * constrained threads to return to the kernel before we can dispatch additional work
		 */
	        if ((th_to_park = thread) == THREAD_NULL)
		        goto out_of_work;
		goto parkit;
	}

	thactive_count = 0;
	busycount = 0;

	curtime = mach_absolute_time();

	thactive_count += wq->wq_thactive_count[priclass];

	if (wq->wq_thscheduled_count[priclass]) {
		if (wq_thread_is_busy(curtime, &wq->wq_lastblocked_ts[priclass])) {
			busycount++;
		}
	}

	if (thread != THREAD_NULL) {
		if (tl->th_priority == priclass) {
			/*
			 * dont't count this thread as currently active
			 */
			thactive_count--;
		}
	}
	if (thactive_count + busycount >= wq->wq_max_concurrency) {
		if (busycount) {
				/*
				 * we found at least 1 thread in the
				 * 'busy' state... make sure we start
				 * the timer because if they are the only
				 * threads keeping us from scheduling
				 * this work request, we won't get a callback
				 * to kick off the timer... we need to
				 * start it now...
				 */
				WQ_TIMER_NEEDED(wq, start_timer);
		}

		PTHREAD_TRACE(TRACE_wq_overcommitted|DBG_FUNC_NONE, wq, (start_timer ? 1<<7 : 0) | pthread_priority_from_class_index(priclass), thactive_count, busycount, 0);

		if ((th_to_park = thread) == THREAD_NULL) {
			goto out_of_work;
		}

		goto parkit;
	}

	if (thread != THREAD_NULL) {
		/*
		 * thread is non-NULL here when we return from userspace
		 * in workq_kernreturn, rather than trying to find a thread
		 * we pick up new work for this specific thread.
		 */
		th_to_run = thread;
		goto pick_up_work;
	}

grab_idle_thread:
	if (wq->wq_thidlecount == 0) {
		/*
		 * we have no additional threads waiting to pick up
		 * work, however, there is additional work to do.
		 */
		WQ_TIMER_NEEDED(wq, start_timer);

		PTHREAD_TRACE(TRACE_wq_stalled, wq, wq->wq_nthreads, start_timer, 0, 0);

		goto no_thread_to_run;
	}

	/*
	 * we already know there is both work available
	 * and an idle thread, so activate a thread and then
	 * fall into the code that pulls a new work request...
	 */
	tl = TAILQ_FIRST(&wq->wq_thidlelist);
	TAILQ_REMOVE(&wq->wq_thidlelist, tl, th_entry);
	wq->wq_thidlecount--;	

	TAILQ_INSERT_TAIL(&wq->wq_thrunlist, tl, th_entry);

	if ((tl->th_flags & TH_LIST_SUSPENDED) == TH_LIST_SUSPENDED) {
		tl->th_flags &= ~TH_LIST_SUSPENDED;
		reuse_thread = 0;

	} else if ((tl->th_flags & TH_LIST_BLOCKED) == TH_LIST_BLOCKED) {
		tl->th_flags &= ~TH_LIST_BLOCKED;
		wake_thread = 1;
	}
	tl->th_flags |= TH_LIST_RUNNING | TH_LIST_BUSY;

	wq->wq_threads_scheduled++;
	wq->wq_thscheduled_count[priclass]++;
	OSAddAtomic(1, &wq->wq_thactive_count[priclass]);

	adjust_counters = FALSE;
	th_to_run = tl->th_thread;

pick_up_work:
	if (!overcommit && !force_oc) {
		wq->wq_reqcount--;
		wq->wq_requests[priclass]--;

		if ( !(tl->th_flags & TH_LIST_CONSTRAINED)) {
			wq->wq_constrained_threads_scheduled++;
			tl->th_flags |= TH_LIST_CONSTRAINED;
		}
	} else {
		if (tl->th_flags & TH_LIST_CONSTRAINED) {
			wq->wq_constrained_threads_scheduled--;
			tl->th_flags &= ~TH_LIST_CONSTRAINED;
		}
	}

	orig_class = tl->th_priority;
	tl->th_priority = (uint8_t)priclass;

	if (adjust_counters && (orig_class != priclass)) {
		/*
		 * we need to adjust these counters based on this
		 * thread's new disposition w/r to priority
		 */
		OSAddAtomic(-1, &wq->wq_thactive_count[orig_class]);
		OSAddAtomic(1, &wq->wq_thactive_count[priclass]);

		wq->wq_thscheduled_count[orig_class]--;
		wq->wq_thscheduled_count[priclass]++;
	}
	wq->wq_thread_yielded_count = 0;

	workqueue_unlock(p);

	if (orig_class != priclass) {
		pthread_priority_t pri = pthread_priority_from_class_index(priclass);

		thread_qos_policy_data_t qosinfo;

		/* Set the QoS tier on the thread, along with the ceiling of max importance for this class. */
		qosinfo.qos_tier = pthread_priority_get_qos_class(pri);
		qosinfo.tier_importance = 0;

		PTHREAD_TRACE(TRACE_wq_reset_priority | DBG_FUNC_START, wq, thread_tid(tl->th_thread), pthread_priority_from_class_index(orig_class), 0, 0);

		/* All the previous implementation here now boils down to setting the QoS policy on the thread. */
		pthread_kern->thread_policy_set_internal(th_to_run, THREAD_QOS_POLICY, (thread_policy_t)&qosinfo, THREAD_QOS_POLICY_COUNT);

		PTHREAD_TRACE(TRACE_wq_reset_priority | DBG_FUNC_END, wq, thread_tid(tl->th_thread), pthread_priority_from_class_index(priclass), qosinfo.qos_tier, 0);
	}

	/*
	 * if current thread is reused for work request, does not return via unix_syscall
	 */
	wq_runreq(p, overcommit, pthread_priority_from_class_index(priclass), th_to_run, tl, reuse_thread, wake_thread, (thread == th_to_run));
	
	PTHREAD_TRACE(TRACE_wq_run_nextitem|DBG_FUNC_END, wq, thread_tid(th_to_run), overcommit, 1, 0);

	return (TRUE);

out_of_work:
	/*
	 * we have no work to do or we are fully booked
	 * w/r to running threads...
	 */
no_thread_to_run:
	workqueue_unlock(p);

	if (start_timer)
		workqueue_interval_timer_start(wq);

	PTHREAD_TRACE(TRACE_wq_run_nextitem|DBG_FUNC_END, wq, thread_tid(thread), start_timer, 2, 0);

	return (FALSE);

parkit:
	/*
	 * this is a workqueue thread with no more
	 * work to do... park it for now
	 */
	TAILQ_REMOVE(&wq->wq_thrunlist, tl, th_entry);
	tl->th_flags &= ~TH_LIST_RUNNING;

	tl->th_flags |= TH_LIST_BLOCKED;
	TAILQ_INSERT_HEAD(&wq->wq_thidlelist, tl, th_entry);

	pthread_kern->thread_sched_call(th_to_park, NULL);

	OSAddAtomic(-1, &wq->wq_thactive_count[tl->th_priority]);
	wq->wq_thscheduled_count[tl->th_priority]--;
	wq->wq_threads_scheduled--;

	if (tl->th_flags & TH_LIST_CONSTRAINED) {
		wq->wq_constrained_threads_scheduled--;
		wq->wq_lflags &= ~WQL_EXCEEDED_CONSTRAINED_THREAD_LIMIT;
		tl->th_flags &= ~TH_LIST_CONSTRAINED;
	}
	if (wq->wq_thidlecount < 100)
		us_to_wait = wq_reduce_pool_window_usecs - (wq->wq_thidlecount * (wq_reduce_pool_window_usecs / 100));
	else
		us_to_wait = wq_reduce_pool_window_usecs / 100;

	wq->wq_thidlecount++;
	wq->wq_lflags &= ~WQL_EXCEEDED_TOTAL_THREAD_LIMIT;

	assert_wait_timeout_with_leeway((caddr_t)tl, (THREAD_INTERRUPTIBLE),
			TIMEOUT_URGENCY_SYS_BACKGROUND|TIMEOUT_URGENCY_LEEWAY, us_to_wait,
			wq_reduce_pool_window_usecs, NSEC_PER_USEC);

	workqueue_unlock(p);

	if (start_timer)
		workqueue_interval_timer_start(wq);

	PTHREAD_TRACE1(TRACE_wq_thread_park | DBG_FUNC_START, wq, wq->wq_threads_scheduled, wq->wq_thidlecount, us_to_wait, thread_tid(th_to_park));
	PTHREAD_TRACE(TRACE_wq_run_nextitem | DBG_FUNC_END, wq, thread_tid(thread), 0, 3, 0);

	thread_block((thread_continue_t)wq_unpark_continue);
	/* NOT REACHED */

	return (FALSE);
}


static void
wq_unsuspend_continue(void)
{
	struct uthread *uth = NULL;
	thread_t th_to_unsuspend;
	struct threadlist *tl;
	proc_t	p;

	th_to_unsuspend = current_thread();
	uth = pthread_kern->get_bsdthread_info(th_to_unsuspend);

	if (uth != NULL && (tl = pthread_kern->uthread_get_threadlist(uth)) != NULL) {
		
		if ((tl->th_flags & (TH_LIST_RUNNING | TH_LIST_BUSY)) == TH_LIST_RUNNING) {
			/*
			 * most likely a normal resume of this thread occurred...
			 * it's also possible that the thread was aborted after we
			 * finished setting it up so that it could be dispatched... if
			 * so, thread_bootstrap_return will notice the abort and put
			 * the thread on the path to self-destruction
			 */
normal_resume_to_user:
			pthread_kern->thread_sched_call(th_to_unsuspend, workqueue_callback);
			pthread_kern->thread_bootstrap_return();
		}
		/*
		 * if we get here, it's because we've been resumed due to
		 * an abort of this thread (process is crashing)
		 */
		p = current_proc();

		workqueue_lock_spin(p);

		if (tl->th_flags & TH_LIST_SUSPENDED) {
			/*
			 * thread has been aborted while still on our idle
			 * queue... remove it from our domain...
			 * workqueue_removethread consumes the lock
			 */
			workqueue_removethread(tl, 0);
			pthread_kern->thread_bootstrap_return();
		}
		while ((tl->th_flags & TH_LIST_BUSY)) {
			/*
			 * this thread was aborted after we started making
			 * it runnable, but before we finished dispatching it...
			 * we need to wait for that process to finish,
			 * and we need to ask for a wakeup instead of a
			 * thread_resume since the abort has already resumed us
			 */
			tl->th_flags |= TH_LIST_NEED_WAKEUP;

			assert_wait((caddr_t)tl, (THREAD_UNINT));

			workqueue_unlock(p);
			thread_block(THREAD_CONTINUE_NULL);
			workqueue_lock_spin(p);
		}
		workqueue_unlock(p);
		/*
		 * we have finished setting up the thread's context...
		 * thread_bootstrap_return will take us through the abort path
		 * where the thread will self destruct
		 */
		goto normal_resume_to_user;
	}
	pthread_kern->thread_bootstrap_return();
}


static void
wq_unpark_continue(void)
{
	struct uthread *uth = NULL;
	struct threadlist *tl;
	thread_t th_to_unpark;
	proc_t 	p;
				
	th_to_unpark = current_thread();
	uth = pthread_kern->get_bsdthread_info(th_to_unpark);

	if (uth != NULL) {
		if ((tl = pthread_kern->uthread_get_threadlist(uth)) != NULL) {

			if ((tl->th_flags & (TH_LIST_RUNNING | TH_LIST_BUSY)) == TH_LIST_RUNNING) {
				/*
				 * a normal wakeup of this thread occurred... no need 
				 * for any synchronization with the timer and wq_runreq
				 */
normal_return_to_user:			
				pthread_kern->thread_sched_call(th_to_unpark, workqueue_callback);

				PTHREAD_TRACE(0xefffd018 | DBG_FUNC_END, tl->th_workq, 0, 0, 0, 0);
	
				pthread_kern->thread_exception_return();
			}
			p = current_proc();

			workqueue_lock_spin(p);

			if ( !(tl->th_flags & TH_LIST_RUNNING)) {
				/*
				 * the timer popped us out and we've not
				 * been moved off of the idle list
				 * so we should now self-destruct
				 *
				 * workqueue_removethread consumes the lock
				 */
				workqueue_removethread(tl, 0);
				pthread_kern->thread_exception_return();
			}
			/*
			 * the timer woke us up, but we have already
			 * started to make this a runnable thread,
			 * but have not yet finished that process...
			 * so wait for the normal wakeup
			 */
			while ((tl->th_flags & TH_LIST_BUSY)) {

				assert_wait((caddr_t)tl, (THREAD_UNINT));

				workqueue_unlock(p);

				thread_block(THREAD_CONTINUE_NULL);

				workqueue_lock_spin(p);
			}
			/*
			 * we have finished setting up the thread's context
			 * now we can return as if we got a normal wakeup
			 */
			workqueue_unlock(p);

			goto normal_return_to_user;
		}
	}
	pthread_kern->thread_exception_return();
}



static void 
wq_runreq(proc_t p, boolean_t overcommit, pthread_priority_t priority, thread_t th, struct threadlist *tl,
	   int reuse_thread, int wake_thread, int return_directly)
{
	int ret = 0;
	boolean_t need_resume = FALSE;

	PTHREAD_TRACE1(TRACE_wq_runitem | DBG_FUNC_START, tl->th_workq, overcommit, priority, thread_tid(current_thread()), thread_tid(th));

	ret = _setup_wqthread(p, th, overcommit, priority, reuse_thread, tl);

	if (ret != 0)
		panic("setup_wqthread failed  %x\n", ret);

	if (return_directly) {
		PTHREAD_TRACE(TRACE_wq_run_nextitem|DBG_FUNC_END, tl->th_workq, 0, 0, 4, 0);

		pthread_kern->thread_exception_return();
		panic("wq_runreq: thread_exception_return returned ...\n");
	}
	if (wake_thread) {
		workqueue_lock_spin(p);
		
		tl->th_flags &= ~TH_LIST_BUSY;
		wakeup(tl);

		workqueue_unlock(p);
	} else {
	        PTHREAD_TRACE1(TRACE_wq_thread_suspend | DBG_FUNC_END, tl->th_workq, 0, 0, thread_tid(current_thread()), thread_tid(th));

		workqueue_lock_spin(p);

		if (tl->th_flags & TH_LIST_NEED_WAKEUP) {
			wakeup(tl);
		} else {
			need_resume = TRUE;
		}

		tl->th_flags &= ~(TH_LIST_BUSY | TH_LIST_NEED_WAKEUP);
		
		workqueue_unlock(p);

		if (need_resume) {
			/*
			 * need to do this outside of the workqueue spin lock
			 * since thread_resume locks the thread via a full mutex
			 */
			pthread_kern->thread_resume(th);
		}
	}
}


int
_setup_wqthread(proc_t p, thread_t th, boolean_t overcommit, pthread_priority_t priority, int reuse_thread, struct threadlist *tl)
{
	uint32_t flags = reuse_thread | WQ_FLAG_THREAD_NEWSPI;
	mach_vm_size_t guardsize = vm_map_page_size(tl->th_workq->wq_map);
	int error = 0;

	if (overcommit) {
		flags |= WQ_FLAG_THREAD_OVERCOMMIT;
	}

	/* Put the QoS class value into the lower bits of the reuse_thread register, this is where
	 * the thread priority used to be stored anyway.
	 */
	flags |= (_pthread_priority_get_qos_newest(priority) & WQ_FLAG_THREAD_PRIOMASK);

#if defined(__i386__) || defined(__x86_64__)
	int isLP64 = proc_is64bit(p);

	/*
	 * Set up i386 registers & function call.
	 */
	if (isLP64 == 0) {
		x86_thread_state32_t state;
		x86_thread_state32_t *ts = &state;

		ts->eip = (unsigned int)pthread_kern->proc_get_wqthread(p);
		ts->eax = (unsigned int)(tl->th_stackaddr + PTH_DEFAULT_STACKSIZE + guardsize);
		ts->ebx = (unsigned int)tl->th_thport;
		ts->ecx = (unsigned int)(tl->th_stackaddr + guardsize);
		ts->edx = (unsigned int)0;
		ts->edi = (unsigned int)flags;
		ts->esi = (unsigned int)0;
		/*
		 * set stack pointer
		 */
		ts->esp = (int)((vm_offset_t)((tl->th_stackaddr + PTH_DEFAULT_STACKSIZE + guardsize) - C_32_STK_ALIGN));

		(void)pthread_kern->thread_set_wq_state32(th, (thread_state_t)ts);

	} else {
		x86_thread_state64_t state64;
		x86_thread_state64_t *ts64 = &state64;

		ts64->rip = (uint64_t)pthread_kern->proc_get_wqthread(p);
		ts64->rdi = (uint64_t)(tl->th_stackaddr + PTH_DEFAULT_STACKSIZE + guardsize);
		ts64->rsi = (uint64_t)(tl->th_thport);
		ts64->rdx = (uint64_t)(tl->th_stackaddr + guardsize);
		ts64->rcx = (uint64_t)0;
		ts64->r8 = (uint64_t)flags;
		ts64->r9 = (uint64_t)0;

		/*
		 * set stack pointer aligned to 16 byte boundary
		 */
		ts64->rsp = (uint64_t)((tl->th_stackaddr + PTH_DEFAULT_STACKSIZE + guardsize) - C_64_REDZONE_LEN);

		error = pthread_kern->thread_set_wq_state64(th, (thread_state_t)ts64);
		if (error != KERN_SUCCESS) {
			error = EINVAL;
		}
	}
#else
#error setup_wqthread  not defined for this architecture
#endif

	return error;
}

int 
_fill_procworkqueue(proc_t p, struct proc_workqueueinfo * pwqinfo)
{
	struct workqueue * wq;
	int error = 0;
	int	activecount;
	uint32_t pri;

	workqueue_lock_spin(p);
	if ((wq = pthread_kern->proc_get_wqptr(p)) == NULL) {
		error = EINVAL;
		goto out;
	}
	activecount = 0;

	for (pri = 0; pri < WORKQUEUE_NUM_BUCKETS; pri++) {
		activecount += wq->wq_thactive_count[pri];
	}
	pwqinfo->pwq_nthreads = wq->wq_nthreads;
	pwqinfo->pwq_runthreads = activecount;
	pwqinfo->pwq_blockedthreads = wq->wq_threads_scheduled - activecount;
	pwqinfo->pwq_state = 0;

	if (wq->wq_lflags & WQL_EXCEEDED_CONSTRAINED_THREAD_LIMIT) {
		pwqinfo->pwq_state |= WQ_EXCEEDED_CONSTRAINED_THREAD_LIMIT;
	}

	if (wq->wq_lflags & WQL_EXCEEDED_TOTAL_THREAD_LIMIT) {
		pwqinfo->pwq_state |= WQ_EXCEEDED_TOTAL_THREAD_LIMIT;
	}

out:
	workqueue_unlock(p);
	return(error);
}

int 
_thread_selfid(__unused struct proc *p, uint64_t *retval)
{
	thread_t thread = current_thread();
	*retval = thread_tid(thread);
	return KERN_SUCCESS;
}

void
_pthread_init(void)
{
	pthread_lck_grp_attr = lck_grp_attr_alloc_init();
	pthread_lck_grp = lck_grp_alloc_init("pthread", pthread_lck_grp_attr);
	
	/*
	 * allocate the lock attribute for pthread synchronizers
	 */
	pthread_lck_attr = lck_attr_alloc_init();

	_workqueue_init_lock((proc_t)get_bsdtask_info(kernel_task));
	pthread_list_mlock = lck_mtx_alloc_init(pthread_lck_grp, pthread_lck_attr);
	
	pth_global_hashinit();
	psynch_thcall = thread_call_allocate(psynch_wq_cleanup, NULL);
	psynch_zoneinit();

	/*
	 * register sysctls
	 */
	sysctl_register_oid(&sysctl__kern_wq_yielded_threshold);
	sysctl_register_oid(&sysctl__kern_wq_yielded_window_usecs);
	sysctl_register_oid(&sysctl__kern_wq_stalled_window_usecs);
	sysctl_register_oid(&sysctl__kern_wq_reduce_pool_window_usecs);
	sysctl_register_oid(&sysctl__kern_wq_max_timer_interval_usecs);
	sysctl_register_oid(&sysctl__kern_wq_max_threads);
	sysctl_register_oid(&sysctl__kern_wq_max_constrained_threads);
	sysctl_register_oid(&sysctl__kern_pthread_debug_tracing);
}
