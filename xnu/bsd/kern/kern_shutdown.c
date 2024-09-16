/*
 * Copyright (c) 2000-2006 Apple Computer, Inc. All rights reserved.
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
/*
 *	File:	bsd/kern/kern_shutdown.c
 *
 *	Copyright (C) 1989, NeXT, Inc.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/vm.h>
#include <sys/proc_internal.h>
#include <sys/user.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/vnode_internal.h>
#include <sys/file_internal.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <sys/tty.h>
#include <kern/task.h>
#include <sys/quota.h>
#include <vm/vm_kern_xnu.h>
#include <mach/vm_param.h>
#include <sys/filedesc.h>
#include <mach/host_priv.h>
#include <mach/host_reboot.h>

#include <security/audit/audit.h>

#include <kern/sched_prim.h>            /* for thread_block() */
#include <kern/host.h>                  /* for host_priv_self() */
#include <net/if_var.h>                 /* for if_down_all() */
#include <sys/buf_internal.h>           /* for count_busy_buffers() */
#include <sys/mount_internal.h>         /* for vfs_unmountall() */
#include <mach/task.h>                  /* for task_suspend() */
#include <sys/sysproto.h>               /* abused for sync() */
#include <kern/clock.h>                 /* for delay_for_interval() */
#include <libkern/OSAtomic.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOMessage.h>

#include <sys/kdebug.h>

uint32_t system_inshutdown = 0;
uint32_t final_shutdown_stage = 0;

#if XNU_TARGET_OS_OSX
/* XXX should be in a header file somewhere, but isn't */
extern void (*unmountroot_pre_hook)(void);
#endif

unsigned int proc_shutdown_exitcount = 0;

static int  sd_openlog(vfs_context_t);
static int  sd_closelog(vfs_context_t);
static void sd_log(vfs_context_t, const char *, ...);
static void proc_shutdown(int only_non_dext);
static void zprint_panic_info(void);
extern void halt_log_enter(const char * what, const void * pc, uint64_t time);

#if DEVELOPMENT || DEBUG
extern boolean_t kdp_has_polled_corefile(void);
#endif /* DEVELOPMENT || DEBUG */

struct sd_filterargs {
	int delayterm;
	int shutdownstate;
	int only_non_dext;
};


struct sd_iterargs {
	int signo;              /* the signal to be posted */
	int setsdstate;         /* shutdown state to be set */
	int countproc;          /* count processes on action */
	int activecount;        /* number of processes on which action was done */
};

static vnode_t sd_logvp = NULLVP;
static off_t sd_log_offset = 0;


static int sd_filt1(proc_t, void *);
static int sd_filt2(proc_t, void *);
static int sd_callback1(proc_t p, void * arg);
static int sd_callback2(proc_t p, void * arg);
static int sd_callback3(proc_t p, void * arg);

extern bool panic_include_zprint;
extern mach_memory_info_t *panic_kext_memory_info;
extern vm_size_t panic_kext_memory_size;

static void
zprint_panic_info(void)
{
	unsigned int  num_sites;
	kern_return_t kr;

	panic_include_zprint = true;
	panic_kext_memory_info = NULL;
	panic_kext_memory_size = 0;

	num_sites = vm_page_diagnose_estimate();
	panic_kext_memory_size = num_sites * sizeof(panic_kext_memory_info[0]);

	kr = kmem_alloc(kernel_map, (vm_offset_t *)&panic_kext_memory_info,
	    round_page(panic_kext_memory_size), KMA_DATA | KMA_ZERO,
	    VM_KERN_MEMORY_OSFMK);
	if (kr != KERN_SUCCESS) {
		panic_kext_memory_info = NULL;
		return;
	}

	vm_page_diagnose(panic_kext_memory_info, num_sites, 0, false);
}

int
get_system_inshutdown()
{
	return system_inshutdown;
}

__abortlike
static void
panic_kernel(int howto, char *message)
{
	uint64_t opts = DEBUGGER_OPTION_USERSPACE_INITIATED_PANIC;

	if ((howto & RB_PANIC_ZPRINT) == RB_PANIC_ZPRINT) {
		zprint_panic_info();
	}

	if ((howto & RB_PANIC_FORCERESET) == RB_PANIC_FORCERESET) {
		opts |= DEBUGGER_OPTION_PANICLOGANDREBOOT;
	}

	panic_with_options(0, NULL, opts, "userspace panic: %s", message);
}

extern boolean_t compressor_store_stop_compaction;
extern lck_mtx_t vm_swap_data_lock;
extern int vm_swapfile_create_thread_running;
extern int vm_swapfile_gc_thread_running;
extern uint32_t cl_sparse_push_error;

int
reboot_kernel(int howto, char *message)
{
	int hostboot_option = 0;
	uint64_t startTime;

	if ((howto & (RB_PANIC | RB_QUICK)) == (RB_PANIC | RB_QUICK)) {
		panic_kernel(howto, message);
	}

	if (!OSCompareAndSwap(0, 1, &system_inshutdown)) {
		if ((howto & RB_QUICK) == RB_QUICK) {
			goto force_reboot;
		}
		return EBUSY;
	}

	lck_mtx_lock(&vm_swap_data_lock);

	/* Turn OFF future swapfile reclaimation / compaction etc.*/
	compressor_store_stop_compaction = TRUE;

	/* wait for any current swapfile work to end */
	while (vm_swapfile_create_thread_running || vm_swapfile_gc_thread_running) {
		assert_wait((event_t)&compressor_store_stop_compaction, THREAD_UNINT);

		lck_mtx_unlock(&vm_swap_data_lock);

		thread_block(THREAD_CONTINUE_NULL);

		lck_mtx_lock(&vm_swap_data_lock);
	}

	lck_mtx_unlock(&vm_swap_data_lock);

	/*
	 * Notify the power management root domain that the system will shut down.
	 */
	IOSystemShutdownNotification(howto, kIOSystemShutdownNotificationStageProcessExit);

	if ((howto & RB_QUICK) == RB_QUICK) {
		printf("Quick reboot...\n");
		if ((howto & RB_NOSYNC) == 0) {
			sync((proc_t)NULL, (void *)NULL, (int *)NULL);
		}
	} else if ((howto & RB_NOSYNC) == 0) {
		int iter, nbusy;

		printf("syncing disks... ");

		/*
		 * Release vnodes held by texts before sync.
		 */

		/* handle live procs (deallocate their root and current directories), suspend initproc */

		startTime = mach_absolute_time();
		proc_shutdown(TRUE);
		halt_log_enter("proc_shutdown", 0, mach_absolute_time() - startTime);

#if CONFIG_AUDIT
		startTime = mach_absolute_time();
		audit_shutdown();
		halt_log_enter("audit_shutdown", 0, mach_absolute_time() - startTime);
#endif

#if XNU_TARGET_OS_OSX
		if (unmountroot_pre_hook != NULL) {
			unmountroot_pre_hook();
		}
#endif

		startTime = mach_absolute_time();
		sync((proc_t)NULL, (void *)NULL, (int *)NULL);

		if (kdebug_enable) {
			startTime = mach_absolute_time();
			kdbg_dump_trace_to_file("/var/log/shutdown/shutdown.trace", true);
			halt_log_enter("shutdown.trace", 0, mach_absolute_time() - startTime);
		}

		IOSystemShutdownNotification(howto, kIOSystemShutdownNotificationStageRootUnmount);

		if (cl_sparse_push_error) {
			panic("system_shutdown cluster_push_err failed with ENOSPC %d times\n", cl_sparse_push_error);
		}

		/*
		 * Unmount filesystems
		 */

#if DEVELOPMENT || DEBUG
		if (!(howto & RB_PANIC) || !kdp_has_polled_corefile())
#endif /* DEVELOPMENT || DEBUG */
		{
			startTime = mach_absolute_time();
			vfs_unmountall(TRUE);
			halt_log_enter("vfs_unmountall", 0, mach_absolute_time() - startTime);
		}

		IOSystemShutdownNotification(howto, kIOSystemShutdownNotificationTerminateDEXTs);

		startTime = mach_absolute_time();
		proc_shutdown(FALSE);
		halt_log_enter("proc_shutdown", 0, mach_absolute_time() - startTime);

#if DEVELOPMENT || DEBUG
		if (!(howto & RB_PANIC) || !kdp_has_polled_corefile())
#endif /* DEVELOPMENT || DEBUG */
		{
			startTime = mach_absolute_time();
			vfs_unmountall(FALSE);
			halt_log_enter("vfs_unmountall", 0, mach_absolute_time() - startTime);
		}



		/* Wait for the buffer cache to clean remaining dirty buffers */
		startTime = mach_absolute_time();
		for (iter = 0; iter < 100; iter++) {
			nbusy = count_busy_buffers();
			if (nbusy == 0) {
				break;
			}
			printf("%d ", nbusy);
			delay_for_interval( 1 * nbusy, 1000 * 1000);
		}
		if (nbusy) {
			printf("giving up\n");
		} else {
			printf("done\n");
		}
		halt_log_enter("bufferclean", 0, mach_absolute_time() - startTime);
	}
#if NETWORKING
	/*
	 * Can't just use an splnet() here to disable the network
	 * because that will lock out softints which the disk
	 * drivers depend on to finish DMAs.
	 */
	startTime = mach_absolute_time();
	if_down_all();
	halt_log_enter("if_down_all", 0, mach_absolute_time() - startTime);
#endif /* NETWORKING */

force_reboot:

	if (howto & RB_PANIC) {
		panic_kernel(howto, message);
	}

	// Make sure an RB_QUICK reboot thread and another regular/RB_QUICK thread
	// do not race.
	if (!OSCompareAndSwap(0, 1, &final_shutdown_stage)) {
		return EBUSY;
	}

	if (howto & RB_HALT) {
		hostboot_option = HOST_REBOOT_HALT;
	}

	if (howto & RB_UPSDELAY) {
		hostboot_option = HOST_REBOOT_UPSDELAY;
	}

	host_reboot(host_priv_self(), hostboot_option);
	/*
	 * should not be reached
	 */
	return 0;
}

static int
sd_openlog(vfs_context_t ctx)
{
	int error = 0;
	struct timeval tv;

	/* Open shutdown log */
	if ((error = vnode_open(PROC_SHUTDOWN_LOG, (O_CREAT | FWRITE | O_NOFOLLOW), 0644, 0, &sd_logvp, ctx))) {
		printf("Failed to open %s: error %d\n", PROC_SHUTDOWN_LOG, error);
		sd_logvp = NULLVP;
		return error;
	}

	vnode_setsize(sd_logvp, (off_t)0, 0, ctx);

	/* Write a little header */
	microtime(&tv);
	sd_log(ctx, "Process shutdown log.  Current time is %lu (in seconds).\n\n", tv.tv_sec);

	return 0;
}

static int
sd_closelog(vfs_context_t ctx)
{
	int error = 0;
	if (sd_logvp != NULLVP) {
		VNOP_FSYNC(sd_logvp, MNT_WAIT, ctx);
		error = vnode_close(sd_logvp, FWRITE, ctx);
		sd_logvp = NULLVP;
	}

	return error;
}

__printflike(2, 3)
static void
sd_log(vfs_context_t ctx, const char *fmt, ...)
{
	int resid, log_error, len;
	char logbuf[100];
	va_list arglist;

	/* If the log isn't open yet, open it */
	if (sd_logvp == NULLVP) {
		if (sd_openlog(ctx) != 0) {
			/* Couldn't open, we fail out */
			return;
		}
	}

	va_start(arglist, fmt);
	len = vsnprintf(logbuf, sizeof(logbuf), fmt, arglist);
	log_error = vn_rdwr(UIO_WRITE, sd_logvp, (caddr_t)logbuf, len, sd_log_offset,
	    UIO_SYSSPACE, IO_UNIT | IO_NOAUTH, vfs_context_ucred(ctx), &resid, vfs_context_proc(ctx));
	if (log_error == EIO || log_error == 0) {
		sd_log_offset += (len - resid);
	}

	va_end(arglist);
}

static int
sd_filt1(proc_t p, void * args)
{
	proc_t self = current_proc();
	struct sd_filterargs * sf = (struct sd_filterargs *)args;
	int delayterm = sf->delayterm;
	int shutdownstate = sf->shutdownstate;

	if (sf->only_non_dext && proc_is_driver(p)) {
		return 0;
	}

	if (((p->p_flag & P_SYSTEM) != 0) || (p->p_ppid == 0)
	    || (p == self) || (p->p_stat == SZOMB)
	    || (p->p_shutdownstate != shutdownstate)
	    || ((delayterm == 0) && ((p->p_lflag & P_LDELAYTERM) == P_LDELAYTERM))
	    || ((p->p_sigcatch & sigmask(SIGTERM)) == 0)) {
		return 0;
	} else {
		return 1;
	}
}


static int
sd_callback1(proc_t p, void * args)
{
	struct sd_iterargs * sd = (struct sd_iterargs *)args;
	int signo = sd->signo;
	int setsdstate = sd->setsdstate;
	int countproc = sd->countproc;

	proc_lock(p);
	p->p_shutdownstate = (char)setsdstate;
	if (p->p_stat != SZOMB) {
		proc_unlock(p);
		if (countproc != 0) {
			proc_list_lock();
			p->p_listflag |= P_LIST_EXITCOUNT;
			proc_shutdown_exitcount++;
			proc_list_unlock();
		}
		if (proc_is_driver(p)) {
			printf("lingering dext %s signal(%d)\n", p->p_name, signo);
		}
		psignal(p, signo);
		if (countproc != 0) {
			sd->activecount++;
		}
	} else {
		proc_unlock(p);
	}

	return PROC_RETURNED;
}

static int
sd_filt2(proc_t p, void * args)
{
	proc_t self = current_proc();
	struct sd_filterargs * sf = (struct sd_filterargs *)args;
	int delayterm = sf->delayterm;
	int shutdownstate = sf->shutdownstate;

	if (sf->only_non_dext && proc_is_driver(p)) {
		return 0;
	}

	if (((p->p_flag & P_SYSTEM) != 0) || (p->p_ppid == 0)
	    || (p == self) || (p->p_stat == SZOMB)
	    || (p->p_shutdownstate == shutdownstate)
	    || ((delayterm == 0) && ((p->p_lflag & P_LDELAYTERM) == P_LDELAYTERM))) {
		return 0;
	} else {
		return 1;
	}
}

static int
sd_callback2(proc_t p, void * args)
{
	struct sd_iterargs * sd = (struct sd_iterargs *)args;
	int signo = sd->signo;
	int setsdstate = sd->setsdstate;
	int countproc = sd->countproc;

	proc_lock(p);
	p->p_shutdownstate = (char)setsdstate;
	if (p->p_stat != SZOMB) {
		proc_unlock(p);
		if (countproc != 0) {
			proc_list_lock();
			p->p_listflag |= P_LIST_EXITCOUNT;
			proc_shutdown_exitcount++;
			proc_list_unlock();
		}
		if (proc_is_driver(p)) {
			printf("lingering dext %s signal(%d)\n", p->p_name, signo);
		}
		psignal(p, signo);
		if (countproc != 0) {
			sd->activecount++;
		}
	} else {
		proc_unlock(p);
	}

	return PROC_RETURNED;
}

static int
sd_callback3(proc_t p, void * args)
{
	struct sd_iterargs * sd = (struct sd_iterargs *)args;
	vfs_context_t ctx = vfs_context_current();

	int setsdstate = sd->setsdstate;

	proc_lock(p);
	p->p_shutdownstate = (char)setsdstate;
	if (p->p_stat != SZOMB) {
		/*
		 * NOTE: following code ignores sig_lock and plays
		 * with exit_thread correctly.  This is OK unless we
		 * are a multiprocessor, in which case I do not
		 * understand the sig_lock.  This needs to be fixed.
		 * XXX
		 */
		if (p->exit_thread) {   /* someone already doing it */
			proc_unlock(p);
			/* give him a chance */
			thread_block(THREAD_CONTINUE_NULL);
		} else {
			p->exit_thread = current_thread();
			printf(".");

			sd_log(ctx, "%s[%d] had to be forced closed with exit1().\n", p->p_comm, proc_getpid(p));

			proc_unlock(p);
			KERNEL_DEBUG_CONSTANT(BSDDBG_CODE(DBG_BSD_PROC, BSD_PROC_FRCEXIT) | DBG_FUNC_NONE,
			    proc_getpid(p), 0, 1, 0, 0);
			sd->activecount++;
			exit1(p, 1, (int *)NULL);
		}
	} else {
		proc_unlock(p);
	}

	return PROC_RETURNED;
}


/*
 * proc_shutdown()
 *
 *	Shutdown down proc system (release references to current and root
 *	dirs for each process).
 *
 * POSIX modifications:
 *
 *	For POSIX fcntl() file locking call vno_lockrelease() on
 *	the file to release all of its record locks, if any.
 */

static void
proc_shutdown(int only_non_dext)
{
	vfs_context_t ctx = vfs_context_current();
	struct proc *p, *self;
	int delayterm = 0;
	struct sd_filterargs sfargs;
	struct sd_iterargs sdargs;
	int error = 0;
	struct timespec ts;

	/*
	 *	Kill as many procs as we can.  (Except ourself...)
	 */
	self = (struct proc *)current_proc();

	/*
	 * Signal the init with SIGTERM so that he does not launch
	 * new processes
	 */
	p = proc_find(1);
	if (p && p != self) {
		psignal(p, SIGTERM);
	}
	proc_rele(p);

	printf("Killing all processes ");

sigterm_loop:
	/*
	 * send SIGTERM to those procs interested in catching one
	 */
	sfargs.delayterm = delayterm;
	sfargs.shutdownstate = 0;
	sfargs.only_non_dext = only_non_dext;
	sdargs.signo = SIGTERM;
	sdargs.setsdstate = 1;
	sdargs.countproc = 1;
	sdargs.activecount = 0;

	error = 0;
	/* post a SIGTERM to all that catch SIGTERM and not marked for delay */
	proc_rebootscan(sd_callback1, (void *)&sdargs, sd_filt1, (void *)&sfargs);

	if (sdargs.activecount != 0 && proc_shutdown_exitcount != 0) {
		proc_list_lock();
		if (proc_shutdown_exitcount != 0) {
			/*
			 * now wait for up to 3 seconds to allow those procs catching SIGTERM
			 * to digest it
			 * as soon as these procs have exited, we'll continue on to the next step
			 */
			ts.tv_sec = 3;
			ts.tv_nsec = 0;
			error = msleep(&proc_shutdown_exitcount, &proc_list_mlock, PWAIT, "shutdownwait", &ts);
			if (error != 0) {
				for (p = allproc.lh_first; p; p = p->p_list.le_next) {
					if ((p->p_listflag & P_LIST_EXITCOUNT) == P_LIST_EXITCOUNT) {
						p->p_listflag &= ~P_LIST_EXITCOUNT;
					}
				}
				for (p = zombproc.lh_first; p; p = p->p_list.le_next) {
					if ((p->p_listflag & P_LIST_EXITCOUNT) == P_LIST_EXITCOUNT) {
						p->p_listflag &= ~P_LIST_EXITCOUNT;
					}
				}
			}
		}
		proc_list_unlock();
	}
	if (error == ETIMEDOUT) {
		/*
		 * log the names of the unresponsive tasks
		 */

		proc_list_lock();

		for (p = allproc.lh_first; p; p = p->p_list.le_next) {
			if (p->p_shutdownstate == 1) {
				printf("%s[%d]: didn't act on SIGTERM\n", p->p_comm, proc_getpid(p));
				sd_log(ctx, "%s[%d]: didn't act on SIGTERM\n", p->p_comm, proc_getpid(p));
			}
		}

		proc_list_unlock();
	}

	/*
	 * send a SIGKILL to all the procs still hanging around
	 */
	sfargs.delayterm = delayterm;
	sfargs.shutdownstate = 2;
	sdargs.signo = SIGKILL;
	sdargs.setsdstate = 2;
	sdargs.countproc = 1;
	sdargs.activecount = 0;

	/* post a SIGKILL to all that catch SIGTERM and not marked for delay */
	proc_rebootscan(sd_callback2, (void *)&sdargs, sd_filt2, (void *)&sfargs);

	error = 0;

	if (sdargs.activecount != 0 && proc_shutdown_exitcount != 0) {
		proc_list_lock();
		if (proc_shutdown_exitcount != 0) {
			/*
			 * wait for up to 60 seconds to allow these procs to exit normally
			 *
			 * History:	The delay interval was changed from 100 to 200
			 *		for NFS requests in particular.
			 */
			ts.tv_sec = 10;
			ts.tv_nsec = 0;
			error = msleep(&proc_shutdown_exitcount, &proc_list_mlock, PWAIT, "shutdownwait", &ts);
			if (error != 0) {
				for (p = allproc.lh_first; p; p = p->p_list.le_next) {
					if ((p->p_listflag & P_LIST_EXITCOUNT) == P_LIST_EXITCOUNT) {
						p->p_listflag &= ~P_LIST_EXITCOUNT;
					}
				}
				for (p = zombproc.lh_first; p; p = p->p_list.le_next) {
					if ((p->p_listflag & P_LIST_EXITCOUNT) == P_LIST_EXITCOUNT) {
						p->p_listflag &= ~P_LIST_EXITCOUNT;
					}
				}
			}
		}
		proc_list_unlock();
	}

	if (error == ETIMEDOUT) {
		/*
		 * log the names of the unresponsive tasks
		 */

		proc_list_lock();

		for (p = allproc.lh_first; p; p = p->p_list.le_next) {
			if (p->p_shutdownstate == 2) {
				printf("%s[%d]: didn't act on SIGKILL\n", p->p_comm, proc_getpid(p));
				sd_log(ctx, "%s[%d]: didn't act on SIGKILL\n", p->p_comm, proc_getpid(p));
			}
		}

		proc_list_unlock();
	}

	/*
	 * if we still have procs that haven't exited, then brute force 'em
	 */
	sfargs.delayterm = delayterm;
	sfargs.shutdownstate = 3;
	sdargs.signo = 0;
	sdargs.setsdstate = 3;
	sdargs.countproc = 0;
	sdargs.activecount = 0;



	/* post a SIGTERM to all that catch SIGTERM and not marked for delay */
	proc_rebootscan(sd_callback3, (void *)&sdargs, sd_filt2, (void *)&sfargs);
	printf("\n");

	/* Now start the termination of processes that are marked for delayed termn */
	if (delayterm == 0) {
		delayterm = 1;
		goto  sigterm_loop;
	}

	sd_closelog(ctx);

	if (only_non_dext) {
		return;
	}

	/*
	 * Now that all other processes have been terminated, suspend init
	 */
	task_suspend_internal(proc_task(initproc));

	/* drop the ref on initproc */
	proc_rele(initproc);
	printf("continuing\n");
}
