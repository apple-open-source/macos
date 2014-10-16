//
//  pthread.c
//  pthread
//
//  Created by Matt Wright on 9/13/12.
//  Copyright (c) 2012 Matt Wright. All rights reserved.
//

#include <kern/thread.h>
#include <kern/debug.h>
#include "kern_internal.h"

kern_return_t pthread_start(kmod_info_t * ki, void *d);
kern_return_t pthread_stop(kmod_info_t *ki, void *d);

pthread_callbacks_t pthread_kern;

const struct pthread_functions_s pthread_internal_functions = {
	.pthread_init = _pthread_init,
	.fill_procworkqueue = _fill_procworkqueue,
	.workqueue_init_lock = _workqueue_init_lock,
	.workqueue_destroy_lock = _workqueue_destroy_lock,
	.workqueue_exit = _workqueue_exit,
	.workqueue_mark_exiting = _workqueue_mark_exiting,
	.workqueue_thread_yielded = _workqueue_thread_yielded,
	.workqueue_get_sched_callback = _workqueue_get_sched_callback,
	.pth_proc_hashinit = _pth_proc_hashinit,
	.pth_proc_hashdelete = _pth_proc_hashdelete,
	.bsdthread_create = _bsdthread_create,
	.bsdthread_register = _bsdthread_register,
	.bsdthread_terminate = _bsdthread_terminate,
	.bsdthread_ctl = _bsdthread_ctl,
	.thread_selfid = _thread_selfid,
	.workq_kernreturn = _workq_kernreturn,
	.workq_open = _workq_open,

	.psynch_mutexwait = _psynch_mutexwait,
	.psynch_mutexdrop = _psynch_mutexdrop,
	.psynch_cvbroad = _psynch_cvbroad,
	.psynch_cvsignal = _psynch_cvsignal,
	.psynch_cvwait = _psynch_cvwait,
	.psynch_cvclrprepost = _psynch_cvclrprepost,
	.psynch_rw_longrdlock = _psynch_rw_longrdlock,
	.psynch_rw_rdlock = _psynch_rw_rdlock,
	.psynch_rw_unlock = _psynch_rw_unlock,
	.psynch_rw_wrlock = _psynch_rw_wrlock,
	.psynch_rw_yieldwrlock = _psynch_rw_yieldwrlock,
};

kern_return_t pthread_start(__unused kmod_info_t * ki, __unused void *d)
{
	pthread_kext_register((pthread_functions_t)&pthread_internal_functions, &pthread_kern);
	return KERN_SUCCESS;
}

kern_return_t pthread_stop(__unused kmod_info_t *ki, __unused void *d)
{
	return KERN_FAILURE;
}

struct uthread*
current_uthread(void)
{
	thread_t th = current_thread();
	return pthread_kern->get_bsdthread_info(th);
}
