/*
 * Copyright (c) 2006-2018 Apple Inc. All rights reserved.
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
 *
 */

#include <sys/kern_event.h>
#include <kern/sched_prim.h>
#include <kern/assert.h>
#include <kern/debug.h>
#include <kern/locks.h>
#include <kern/task.h>
#include <kern/thread.h>
#include <kern/thread_call.h>
#include <kern/host.h>
#include <kern/policy_internal.h>
#include <kern/thread_group.h>

#include <IOKit/IOBSD.h>

#include <libkern/libkern.h>
#include <libkern/coreanalytics/coreanalytics.h>
#include <mach/coalition.h>
#include <mach/clock_types.h>
#include <mach/mach_time.h>
#include <mach/task.h>
#include <mach/host_priv.h>
#include <mach/mach_host.h>
#include <os/log.h>
#include <pexpert/pexpert.h>
#include <sys/coalition.h>
#include <sys/kern_event.h>
#include <sys/proc.h>
#include <sys/proc_info.h>
#include <sys/reason.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/tree.h>
#include <sys/priv.h>
#include <vm/vm_pageout_xnu.h>
#include <vm/vm_protos.h>
#include <vm/vm_purgeable_xnu.h>
#include <mach/machine/sdt.h>
#include <libkern/section_keywords.h>
#include <stdatomic.h>

#if CONFIG_FREEZE
#include <vm/vm_map.h>
#endif /* CONFIG_FREEZE */

#include <kern/kern_memorystatus_internal.h>
#include <sys/kern_memorystatus.h>
#include <sys/kern_memorystatus_notify.h>

/*
 * Memorystatus klist structures
 */
struct klist memorystatus_klist;
static lck_mtx_t memorystatus_klist_mutex;
static void memorystatus_klist_lock(void);
static void memorystatus_klist_unlock(void);

/*
 * Memorystatus kevent filter routines
 */
static int filt_memorystatusattach(struct knote *kn, struct kevent_qos_s *kev);
static void filt_memorystatusdetach(struct knote *kn);
static int filt_memorystatus(struct knote *kn, long hint);
static int filt_memorystatustouch(struct knote *kn, struct kevent_qos_s *kev);
static int filt_memorystatusprocess(struct knote *kn, struct kevent_qos_s *kev);

SECURITY_READ_ONLY_EARLY(struct filterops) memorystatus_filtops = {
	.f_attach = filt_memorystatusattach,
	.f_detach = filt_memorystatusdetach,
	.f_event = filt_memorystatus,
	.f_touch = filt_memorystatustouch,
	.f_process = filt_memorystatusprocess,
};

/*
 * Memorystatus notification events
 */
enum {
	kMemorystatusNoPressure = 0x1,
	kMemorystatusPressure = 0x2,
	kMemorystatusLowSwap = 0x4,
	kMemorystatusProcLimitWarn = 0x8,
	kMemorystatusProcLimitCritical = 0x10
};

#define INTER_NOTIFICATION_DELAY    (250000)    /* .25 second */
#define VM_PRESSURE_DECREASED_SMOOTHING_PERIOD        5000    /* milliseconds */
#define WARNING_NOTIFICATION_RESTING_PERIOD        25    /* seconds */
#define CRITICAL_NOTIFICATION_RESTING_PERIOD        25    /* seconds */

/*
 * Memorystatus notification helper routines
 */
static vm_pressure_level_t convert_internal_pressure_level_to_dispatch_level(vm_pressure_level_t);
static boolean_t is_knote_registered_modify_task_pressure_bits(struct knote*, int, task_t, vm_pressure_level_t, vm_pressure_level_t);
static void memorystatus_klist_reset_all_for_level(vm_pressure_level_t pressure_level_to_clear);
static struct knote *vm_pressure_select_optimal_candidate_to_notify(struct klist *candidate_list, int level, boolean_t target_foreground_process, uint64_t *next_telemetry_update);
static void vm_dispatch_memory_pressure(void);
kern_return_t memorystatus_update_vm_pressure(boolean_t target_foreground_process);

#if VM_PRESSURE_EVENTS

/*
 * This value is the threshold that a process must meet to be considered for scavenging.
 */
#if XNU_TARGET_OS_OSX
#define VM_PRESSURE_MINIMUM_RSIZE        10    /* MB */
#else /* XNU_TARGET_OS_OSX */
#define VM_PRESSURE_MINIMUM_RSIZE        6    /* MB */
#endif /* XNU_TARGET_OS_OSX */

static uint32_t vm_pressure_task_footprint_min = VM_PRESSURE_MINIMUM_RSIZE;

#if DEVELOPMENT || DEBUG
SYSCTL_UINT(_kern, OID_AUTO, memorystatus_vm_pressure_task_footprint_min, CTLFLAG_RW | CTLFLAG_LOCKED, &vm_pressure_task_footprint_min, 0, "");
#endif /* DEVELOPMENT || DEBUG */

vm_pressure_level_t memorystatus_vm_pressure_level = kVMPressureNormal;

/*
 * We use this flag to signal if we have any HWM offenders
 * on the system. This way we can reduce the number of wakeups
 * of the memorystatus_thread when the system is between the
 * "pressure" and "critical" threshold.
 *
 * The (re-)setting of this variable is done without any locks
 * or synchronization simply because it is not possible (currently)
 * to keep track of HWM offenders that drop down below their memory
 * limit and/or exit. So, we choose to burn a couple of wasted wakeups
 * by allowing the unguarded modification of this variable.
 */
boolean_t memorystatus_hwm_candidates = 0;

#endif /* VM_PRESSURE_EVENTS */

#if CONFIG_JETSAM

extern unsigned int memorystatus_available_pages;
extern unsigned int memorystatus_available_pages_pressure;
extern unsigned int memorystatus_available_pages_critical;
extern unsigned int memorystatus_available_pages_critical_base;
extern unsigned int memorystatus_available_pages_critical_idle_offset;

#else /* CONFIG_JETSAM */

extern uint64_t memorystatus_available_pages;
extern uint64_t memorystatus_available_pages_pressure;
extern uint64_t memorystatus_available_pages_critical;

#endif /* CONFIG_JETSAM */

uint32_t memorystatus_jetsam_fg_band_waiters = 0;
uint32_t memorystatus_jetsam_bg_band_waiters = 0;
static uint64_t memorystatus_jetsam_fg_band_timestamp_ns = 0; /* nanosec */
static uint64_t memorystatus_jetsam_bg_band_timestamp_ns = 0; /* nanosec */
static uint64_t memorystatus_jetsam_notification_delay_ns = 5ull * 1000 * 1000 * 1000; /* nanosec */

#if DEVELOPMENT || DEBUG
SYSCTL_QUAD(_kern, OID_AUTO, memorystatus_jetsam_notification_delay_ns, CTLFLAG_RW | CTLFLAG_LOCKED,
    &memorystatus_jetsam_notification_delay_ns, "");
#endif

static int
filt_memorystatusattach(struct knote *kn, __unused struct kevent_qos_s *kev)
{
	int error;

	kn->kn_flags |= EV_CLEAR; /* automatically set */
	kn->kn_sdata = 0;         /* incoming data is ignored */
	memset(&kn->kn_ext, 0, sizeof(kn->kn_ext));

	error = memorystatus_knote_register(kn);
	if (error) {
		knote_set_error(kn, error);
	}
	return 0;
}

static void
filt_memorystatusdetach(struct knote *kn)
{
	memorystatus_knote_unregister(kn);
}

static int
filt_memorystatus(struct knote *kn __unused, long hint)
{
	if (hint) {
		switch (hint) {
		case kMemorystatusNoPressure:
			if (kn->kn_sfflags & NOTE_MEMORYSTATUS_PRESSURE_NORMAL) {
				kn->kn_fflags = NOTE_MEMORYSTATUS_PRESSURE_NORMAL;
			}
			break;
		case kMemorystatusPressure:
			if (memorystatus_vm_pressure_level == kVMPressureWarning || memorystatus_vm_pressure_level == kVMPressureUrgent) {
				if (kn->kn_sfflags & NOTE_MEMORYSTATUS_PRESSURE_WARN) {
					kn->kn_fflags = NOTE_MEMORYSTATUS_PRESSURE_WARN;
				}
			} else if (memorystatus_vm_pressure_level == kVMPressureCritical) {
				if (kn->kn_sfflags & NOTE_MEMORYSTATUS_PRESSURE_CRITICAL) {
					kn->kn_fflags = NOTE_MEMORYSTATUS_PRESSURE_CRITICAL;
				}
			}
			break;
		case kMemorystatusLowSwap:
			if (kn->kn_sfflags & NOTE_MEMORYSTATUS_LOW_SWAP) {
				kn->kn_fflags = NOTE_MEMORYSTATUS_LOW_SWAP;
			}
			break;

		case kMemorystatusProcLimitWarn:
			if (kn->kn_sfflags & NOTE_MEMORYSTATUS_PROC_LIMIT_WARN) {
				kn->kn_fflags = NOTE_MEMORYSTATUS_PROC_LIMIT_WARN;
			}
			break;

		case kMemorystatusProcLimitCritical:
			if (kn->kn_sfflags & NOTE_MEMORYSTATUS_PROC_LIMIT_CRITICAL) {
				kn->kn_fflags = NOTE_MEMORYSTATUS_PROC_LIMIT_CRITICAL;
			}
			break;

		default:
			break;
		}
	}

#if 0
	if (kn->kn_fflags != 0) {
		proc_t knote_proc = knote_get_kq(kn)->kq_p;
		pid_t knote_pid = proc_getpid(knote_proc);

		printf("filt_memorystatus: sending kn 0x%lx (event 0x%x) for pid (%d)\n",
		    (unsigned long)kn, kn->kn_fflags, knote_pid);
	}
#endif

	return kn->kn_fflags != 0;
}

static int
filt_memorystatustouch(struct knote *kn, struct kevent_qos_s *kev)
{
	int res;
	int prev_kn_sfflags = 0;

	memorystatus_klist_lock();

	/*
	 * copy in new kevent settings
	 * (saving the "desired" data and fflags).
	 */

	prev_kn_sfflags = kn->kn_sfflags;
	kn->kn_sfflags = (kev->fflags & EVFILT_MEMORYSTATUS_ALL_MASK);

#if XNU_TARGET_OS_OSX
	/*
	 * Only on desktop do we restrict notifications to
	 * one per active/inactive state (soft limits only).
	 */
	if (kn->kn_sfflags & NOTE_MEMORYSTATUS_PROC_LIMIT_WARN) {
		/*
		 * Is there previous state to preserve?
		 */
		if (prev_kn_sfflags & NOTE_MEMORYSTATUS_PROC_LIMIT_WARN) {
			/*
			 * This knote was previously interested in proc_limit_warn,
			 * so yes, preserve previous state.
			 */
			if (prev_kn_sfflags & NOTE_MEMORYSTATUS_PROC_LIMIT_WARN_ACTIVE) {
				kn->kn_sfflags |= NOTE_MEMORYSTATUS_PROC_LIMIT_WARN_ACTIVE;
			}
			if (prev_kn_sfflags & NOTE_MEMORYSTATUS_PROC_LIMIT_WARN_INACTIVE) {
				kn->kn_sfflags |= NOTE_MEMORYSTATUS_PROC_LIMIT_WARN_INACTIVE;
			}
		} else {
			/*
			 * This knote was not previously interested in proc_limit_warn,
			 * but it is now.  Set both states.
			 */
			kn->kn_sfflags |= NOTE_MEMORYSTATUS_PROC_LIMIT_WARN_ACTIVE;
			kn->kn_sfflags |= NOTE_MEMORYSTATUS_PROC_LIMIT_WARN_INACTIVE;
		}
	}

	if (kn->kn_sfflags & NOTE_MEMORYSTATUS_PROC_LIMIT_CRITICAL) {
		/*
		 * Is there previous state to preserve?
		 */
		if (prev_kn_sfflags & NOTE_MEMORYSTATUS_PROC_LIMIT_CRITICAL) {
			/*
			 * This knote was previously interested in proc_limit_critical,
			 * so yes, preserve previous state.
			 */
			if (prev_kn_sfflags & NOTE_MEMORYSTATUS_PROC_LIMIT_CRITICAL_ACTIVE) {
				kn->kn_sfflags |= NOTE_MEMORYSTATUS_PROC_LIMIT_CRITICAL_ACTIVE;
			}
			if (prev_kn_sfflags & NOTE_MEMORYSTATUS_PROC_LIMIT_CRITICAL_INACTIVE) {
				kn->kn_sfflags |= NOTE_MEMORYSTATUS_PROC_LIMIT_CRITICAL_INACTIVE;
			}
		} else {
			/*
			 * This knote was not previously interested in proc_limit_critical,
			 * but it is now.  Set both states.
			 */
			kn->kn_sfflags |= NOTE_MEMORYSTATUS_PROC_LIMIT_CRITICAL_ACTIVE;
			kn->kn_sfflags |= NOTE_MEMORYSTATUS_PROC_LIMIT_CRITICAL_INACTIVE;
		}
	}
#endif /* XNU_TARGET_OS_OSX */

	/*
	 * reset the output flags based on a
	 * combination of the old events and
	 * the new desired event list.
	 */
	//kn->kn_fflags &= kn->kn_sfflags;

	res = (kn->kn_fflags != 0);

	memorystatus_klist_unlock();

	return res;
}

static int
filt_memorystatusprocess(struct knote *kn, struct kevent_qos_s *kev)
{
	int res = 0;

	memorystatus_klist_lock();
	if (kn->kn_fflags) {
		knote_fill_kevent(kn, kev, 0);
		res = 1;
	}
	memorystatus_klist_unlock();

	return res;
}

static void
memorystatus_klist_lock(void)
{
	lck_mtx_lock(&memorystatus_klist_mutex);
}

static void
memorystatus_klist_unlock(void)
{
	lck_mtx_unlock(&memorystatus_klist_mutex);
}

void
memorystatus_kevent_init(lck_grp_t *grp, lck_attr_t *attr)
{
	lck_mtx_init(&memorystatus_klist_mutex, grp, attr);
	klist_init(&memorystatus_klist);
}

int
memorystatus_knote_register(struct knote *kn)
{
	int error = 0;

	memorystatus_klist_lock();

	/*
	 * Support only userspace visible flags.
	 */
	if ((kn->kn_sfflags & EVFILT_MEMORYSTATUS_ALL_MASK) == (unsigned int) kn->kn_sfflags) {
#if XNU_TARGET_OS_OSX
		if (kn->kn_sfflags & NOTE_MEMORYSTATUS_PROC_LIMIT_WARN) {
			kn->kn_sfflags |= NOTE_MEMORYSTATUS_PROC_LIMIT_WARN_ACTIVE;
			kn->kn_sfflags |= NOTE_MEMORYSTATUS_PROC_LIMIT_WARN_INACTIVE;
		}

		if (kn->kn_sfflags & NOTE_MEMORYSTATUS_PROC_LIMIT_CRITICAL) {
			kn->kn_sfflags |= NOTE_MEMORYSTATUS_PROC_LIMIT_CRITICAL_ACTIVE;
			kn->kn_sfflags |= NOTE_MEMORYSTATUS_PROC_LIMIT_CRITICAL_INACTIVE;
		}
#endif /* XNU_TARGET_OS_OSX */

		KNOTE_ATTACH(&memorystatus_klist, kn);
	} else {
		error = ENOTSUP;
	}

	memorystatus_klist_unlock();

	return error;
}

void
memorystatus_knote_unregister(struct knote *kn __unused)
{
	memorystatus_klist_lock();
	KNOTE_DETACH(&memorystatus_klist, kn);
	memorystatus_klist_unlock();
}

#if VM_PRESSURE_EVENTS

#if CONFIG_JETSAM

static thread_call_t sustained_pressure_handler_thread_call;
int memorystatus_should_kill_on_sustained_pressure = 1;
/* Count the number of sustained pressure kills we've done since boot. */
uint64_t memorystatus_kill_on_sustained_pressure_count = 0;
uint64_t memorystatus_kill_on_sustained_pressure_window_s = 60 * 10; /* 10 Minutes */
uint64_t memorystatus_kill_on_sustained_pressure_delay_ms = 500; /* .5 seconds */

#if DEVELOPMENT || DEBUG
SYSCTL_INT(_kern, OID_AUTO, memorystatus_should_kill_on_sustained_pressure, CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_LOCKED, &memorystatus_should_kill_on_sustained_pressure, 0, "");
#endif /* DEVELOPMENT || DEBUG */
SYSCTL_QUAD(_kern, OID_AUTO, memorystatus_kill_on_sustained_pressure_count, CTLFLAG_RD | CTLFLAG_LOCKED, &memorystatus_kill_on_sustained_pressure_count, "");
SYSCTL_QUAD(_kern, OID_AUTO, memorystatus_kill_on_sustained_pressure_window_s, CTLFLAG_RW | CTLFLAG_LOCKED, &memorystatus_kill_on_sustained_pressure_window_s, "");
SYSCTL_QUAD(_kern, OID_AUTO, memorystatus_kill_on_sustained_pressure_delay_ms, CTLFLAG_RW | CTLFLAG_LOCKED, &memorystatus_kill_on_sustained_pressure_delay_ms, "");

static void sustained_pressure_handler(void*, void*);
#endif /* CONFIG_JETSAM */
static thread_call_t memorystatus_notify_update_telemetry_thread_call;
static void update_footprints_for_telemetry(void*, void*);


void
memorystatus_notify_init()
{
#if CONFIG_JETSAM
	sustained_pressure_handler_thread_call = thread_call_allocate_with_options(sustained_pressure_handler, NULL, THREAD_CALL_PRIORITY_KERNEL_HIGH, THREAD_CALL_OPTIONS_ONCE);
#endif /* CONFIG_JETSAM */
	memorystatus_notify_update_telemetry_thread_call = thread_call_allocate_with_options(update_footprints_for_telemetry, NULL, THREAD_CALL_PRIORITY_USER, THREAD_CALL_OPTIONS_ONCE);
}

#if CONFIG_MEMORYSTATUS

inline int
memorystatus_send_note(int event_code, void *data, uint32_t data_length)
{
	int ret;
	struct kev_msg ev_msg;

	ev_msg.vendor_code    = KEV_VENDOR_APPLE;
	ev_msg.kev_class      = KEV_SYSTEM_CLASS;
	ev_msg.kev_subclass   = KEV_MEMORYSTATUS_SUBCLASS;

	ev_msg.event_code     = event_code;

	ev_msg.dv[0].data_length = data_length;
	ev_msg.dv[0].data_ptr = data;
	ev_msg.dv[1].data_length = 0;

	ret = kev_post_msg(&ev_msg);
	if (ret) {
		memorystatus_log_error("%s: kev_post_msg() failed, err %d\n", __func__, ret);
	}

	return ret;
}

boolean_t
memorystatus_warn_process(const proc_t p, __unused boolean_t is_active, __unused boolean_t is_fatal, boolean_t limit_exceeded)
{
	/*
	 * This function doesn't take a reference to p or lock it. So it better be the current process.
	 */
	assert(p == current_proc());
	pid_t pid = proc_getpid(p);
	boolean_t ret = FALSE;
	boolean_t found_knote = FALSE;
	struct knote *kn = NULL;
	int send_knote_count = 0;
	uint32_t platform;
	platform = proc_platform(p);

	/*
	 * See comment in sysctl_memorystatus_vm_pressure_send.
	 */

	memorystatus_klist_lock();

	SLIST_FOREACH(kn, &memorystatus_klist, kn_selnext) {
		proc_t knote_proc = knote_get_kq(kn)->kq_p;
		pid_t knote_pid = proc_getpid(knote_proc);

		if (knote_pid == pid) {
			/*
			 * By setting the "fflags" here, we are forcing
			 * a process to deal with the case where it's
			 * bumping up into its memory limits. If we don't
			 * do this here, we will end up depending on the
			 * system pressure snapshot evaluation in
			 * filt_memorystatus().
			 */

			/*
			 * The type of notification and the frequency are different between
			 * embedded and desktop.
			 *
			 * Embedded processes register for global pressure notifications
			 * (NOTE_MEMORYSTATUS_PRESSURE_WARN | NOTE_MEMORYSTATUS_PRESSURE_CRITICAL) via UIKit
			 * (see applicationDidReceiveMemoryWarning in UIKit). We'll warn them here if
			 * they are near there memory limit. filt_memorystatus() will warn them based
			 * on the system pressure level.
			 *
			 * On desktop, (NOTE_MEMORYSTATUS_PRESSURE_WARN | NOTE_MEMORYSTATUS_PRESSURE_CRITICAL)
			 * are only expected to fire for system level warnings. Desktop procesess
			 * register for NOTE_MEMORYSTATUS_PROC_LIMIT_WARN
			 * if they want to be warned when they approach their limit
			 * and for NOTE_MEMORYSTATUS_PROC_LIMIT_CRITICAL to be warned when they
			 * exceed their limit.
			 *
			 * On embedded we continuously warn processes that are approaching their
			 * memory limit. However on desktop, we only send one warning while
			 * the process is active/inactive if the limit is soft..
			 *
			 */
			if (platform == PLATFORM_MACOS || platform == PLATFORM_MACCATALYST || platform == PLATFORM_DRIVERKIT) {
				if (!limit_exceeded) {
					if (kn->kn_sfflags & NOTE_MEMORYSTATUS_PROC_LIMIT_WARN) {
						found_knote = TRUE;
						if (!is_fatal) {
							/*
							 * Restrict proc_limit_warn notifications when
							 * non-fatal (soft) limit is at play.
							 */
							if (is_active) {
								if (kn->kn_sfflags & NOTE_MEMORYSTATUS_PROC_LIMIT_WARN_ACTIVE) {
									/*
									 * Mark this knote for delivery.
									 */
									kn->kn_fflags = NOTE_MEMORYSTATUS_PROC_LIMIT_WARN;
									/*
									 * And suppress it from future notifications.
									 */
									kn->kn_sfflags &= ~NOTE_MEMORYSTATUS_PROC_LIMIT_WARN_ACTIVE;
									send_knote_count++;
								}
							} else {
								if (kn->kn_sfflags & NOTE_MEMORYSTATUS_PROC_LIMIT_WARN_INACTIVE) {
									/*
									 * Mark this knote for delivery.
									 */
									kn->kn_fflags = NOTE_MEMORYSTATUS_PROC_LIMIT_WARN;
									/*
									 * And suppress it from future notifications.
									 */
									kn->kn_sfflags &= ~NOTE_MEMORYSTATUS_PROC_LIMIT_WARN_INACTIVE;
									send_knote_count++;
								}
							}
						} else {
							/*
							 * No restriction on proc_limit_warn notifications when
							 * fatal (hard) limit is at play.
							 */
							kn->kn_fflags = NOTE_MEMORYSTATUS_PROC_LIMIT_WARN;
							send_knote_count++;
						}
					}
				} else {
					/*
					 * Send this notification when a process has exceeded a soft limit,
					 */

					if (kn->kn_sfflags & NOTE_MEMORYSTATUS_PROC_LIMIT_CRITICAL) {
						found_knote = TRUE;
						if (!is_fatal) {
							/*
							 * Restrict critical notifications for soft limits.
							 */

							if (is_active) {
								if (kn->kn_sfflags & NOTE_MEMORYSTATUS_PROC_LIMIT_CRITICAL_ACTIVE) {
									/*
									 * Suppress future proc_limit_critical notifications
									 * for the active soft limit.
									 */
									kn->kn_sfflags &= ~NOTE_MEMORYSTATUS_PROC_LIMIT_CRITICAL_ACTIVE;
									kn->kn_fflags = NOTE_MEMORYSTATUS_PROC_LIMIT_CRITICAL;
									send_knote_count++;
								}
							} else {
								if (kn->kn_sfflags & NOTE_MEMORYSTATUS_PROC_LIMIT_CRITICAL_INACTIVE) {
									/*
									 * Suppress future proc_limit_critical_notifications
									 * for the inactive soft limit.
									 */
									kn->kn_sfflags &= ~NOTE_MEMORYSTATUS_PROC_LIMIT_CRITICAL_INACTIVE;
									kn->kn_fflags = NOTE_MEMORYSTATUS_PROC_LIMIT_CRITICAL;
									send_knote_count++;
								}
							}
						} else {
							/*
							 * We should never be trying to send a critical notification for
							 * a hard limit... the process would be killed before it could be
							 * received.
							 */
							panic("Caught sending pid %d a critical warning for a fatal limit.", pid);
						}
					}
				}
			} else {
				if (!limit_exceeded) {
					/*
					 * Intentionally set either the unambiguous limit warning,
					 * the system-wide critical or the system-wide warning
					 * notification bit.
					 */

					if (kn->kn_sfflags & NOTE_MEMORYSTATUS_PROC_LIMIT_WARN) {
						kn->kn_fflags = NOTE_MEMORYSTATUS_PROC_LIMIT_WARN;
						found_knote = TRUE;
						send_knote_count++;
					} else if (kn->kn_sfflags & NOTE_MEMORYSTATUS_PRESSURE_CRITICAL) {
						kn->kn_fflags = NOTE_MEMORYSTATUS_PRESSURE_CRITICAL;
						found_knote = TRUE;
						send_knote_count++;
					} else if (kn->kn_sfflags & NOTE_MEMORYSTATUS_PRESSURE_WARN) {
						kn->kn_fflags = NOTE_MEMORYSTATUS_PRESSURE_WARN;
						found_knote = TRUE;
						send_knote_count++;
					}
				} else {
					/*
					 * Send this notification when a process has exceeded a soft limit.
					 */
					if (kn->kn_sfflags & NOTE_MEMORYSTATUS_PROC_LIMIT_CRITICAL) {
						kn->kn_fflags = NOTE_MEMORYSTATUS_PROC_LIMIT_CRITICAL;
						found_knote = TRUE;
						send_knote_count++;
					}
				}
			}
		}
	}

	if (found_knote) {
		if (send_knote_count > 0) {
			KNOTE(&memorystatus_klist, 0);
		}
		ret = TRUE;
	}

	memorystatus_klist_unlock();

	return ret;
}

/*
 * Can only be set by the current task on itself.
 */
int
memorystatus_low_mem_privileged_listener(uint32_t op_flags)
{
	boolean_t set_privilege = FALSE;
	/*
	 * Need an entitlement check here?
	 */
	if (op_flags == MEMORYSTATUS_CMD_PRIVILEGED_LISTENER_ENABLE) {
		set_privilege = TRUE;
	} else if (op_flags == MEMORYSTATUS_CMD_PRIVILEGED_LISTENER_DISABLE) {
		set_privilege = FALSE;
	} else {
		return EINVAL;
	}

	return task_low_mem_privileged_listener(current_task(), set_privilege, NULL);
}

int
memorystatus_send_pressure_note(pid_t pid)
{
	memorystatus_log_debug("memorystatus_send_pressure_note(): pid %d\n", pid);
	return memorystatus_send_note(kMemorystatusPressureNote, &pid, sizeof(pid));
}

boolean_t
memorystatus_is_foreground_locked(proc_t p)
{
	return (p->p_memstat_effectivepriority == JETSAM_PRIORITY_FOREGROUND) ||
	       (p->p_memstat_effectivepriority == JETSAM_PRIORITY_FOREGROUND_SUPPORT);
}

/*
 * This is meant for stackshot and kperf -- it does not take the proc_list_lock
 * to access the p_memstat_dirty field.
 */
void
memorystatus_proc_flags_unsafe(void * v, boolean_t *is_dirty, boolean_t *is_dirty_tracked, boolean_t *allow_idle_exit)
{
	if (!v) {
		*is_dirty = FALSE;
		*is_dirty_tracked = FALSE;
		*allow_idle_exit = FALSE;
	} else {
		proc_t p = (proc_t)v;
		*is_dirty = (p->p_memstat_dirty & P_DIRTY_IS_DIRTY) != 0;
		*is_dirty_tracked = (p->p_memstat_dirty & P_DIRTY_TRACK) != 0;
		*allow_idle_exit = (p->p_memstat_dirty & P_DIRTY_ALLOW_IDLE_EXIT) != 0;
	}
}

boolean_t
memorystatus_bg_pressure_eligible(proc_t p)
{
	boolean_t eligible = FALSE;

	proc_list_lock();

	memorystatus_log_debug("memorystatus_bg_pressure_eligible: pid %d, state 0x%x\n", proc_getpid(p), p->p_memstat_state);

	/* Foreground processes have already been dealt with at this point, so just test for eligibility */
	if (!(p->p_memstat_state & (P_MEMSTAT_TERMINATED | P_MEMSTAT_LOCKED | P_MEMSTAT_SUSPENDED | P_MEMSTAT_FROZEN))) {
		eligible = TRUE;
	}

	if (p->p_memstat_effectivepriority < JETSAM_PRIORITY_BACKGROUND_OPPORTUNISTIC) {
		/*
		 * IDLE and IDLE_DEFERRED bands contain processes
		 * that have dropped memory to be under their inactive
		 * memory limits. And so they can't really give back
		 * anything.
		 */
		eligible = FALSE;
	}

	proc_list_unlock();

	return eligible;
}

void
memorystatus_send_low_swap_note(void)
{
	struct knote *kn = NULL;

	memorystatus_klist_lock();
	SLIST_FOREACH(kn, &memorystatus_klist, kn_selnext) {
		/* We call is_knote_registered_modify_task_pressure_bits to check if the sfflags for the
		 * current note contain NOTE_MEMORYSTATUS_LOW_SWAP. Once we find one note in the memorystatus_klist
		 * that has the NOTE_MEMORYSTATUS_LOW_SWAP flags in its sfflags set, we call KNOTE with
		 * kMemoryStatusLowSwap as the hint to process and update all knotes on the memorystatus_klist accordingly. */
		if (is_knote_registered_modify_task_pressure_bits(kn, NOTE_MEMORYSTATUS_LOW_SWAP, NULL, 0, 0) == TRUE) {
			KNOTE(&memorystatus_klist, kMemorystatusLowSwap);
			break;
		}
	}

	memorystatus_klist_unlock();
}

#endif /* CONFIG_MEMORYSTATUS */

/*
 * Notification telemetry
 */
CA_EVENT(memorystatus_pressure_interval,
    CA_INT, num_processes_registered,
    CA_INT, num_notifications_sent,
    CA_INT, max_level,
    CA_INT, num_transitions,
    CA_INT, num_kills,
    CA_INT, duration);
static CA_EVENT_TYPE(memorystatus_pressure_interval) memorystatus_pressure_interval_telemetry;

CA_EVENT(memorystatus_proc_notification,
    CA_INT, footprint_before_notification,
    CA_INT, footprint_1_min_after_first_warning,
    CA_INT, footprint_5_min_after_first_warning,
    CA_INT, footprint_20_min_after_first_warning,
    CA_INT, footprint_1_min_after_first_critical,
    CA_INT, footprint_5_min_after_first_critical,
    CA_INT, footprint_20_min_after_first_critical,
    CA_INT, order_within_list,
    CA_INT, num_notifications_sent,
    CA_INT, time_between_warning_and_critical,
    CA_STATIC_STRING(CA_PROCNAME_LEN), proc_name);

/* The send timestamps for the first notifications are stored in the knote's kn_sdata field */
#define KNOTE_SEND_TIMESTAMP_WARNING_INDEX 0
#define KNOTE_SEND_TIMESTAMP_CRITICAL_INDEX 1

/* The footprint history for this task is stored in the knote's kn_ext array. */
struct knote_footprint_history {
	uint32_t kfh_starting_footprint;
	uint32_t kfh_footprint_after_warn_1; /* 1 minute after first warning notification */
	uint32_t kfh_footprint_after_warn_5; /* 5 minutes after first warning notification */
	uint32_t kfh_footprint_after_warn_20; /* 20 minutes after first warning notification */
	uint32_t kfh_footprint_after_critical_1; /* 1 minute after first critical notification */
	uint32_t kfh_footprint_after_critical_5; /* 5 minutes after first critical notification */
	uint32_t kfh_footprint_after_critical_20; /* 20 minutes after first critical notification */
	uint16_t kfh_num_notifications;
	uint16_t kfh_notification_order;
} __attribute__((packed));


static_assert(sizeof(struct knote_footprint_history) <= sizeof(uint64_t) * 4, "footprint history fits in knote extensions");

static void
mark_knote_send_time(struct knote *kn, task_t task, int knote_pressure_level, uint16_t order_within_list)
{
	uint32_t *timestamps;
	uint32_t index;
	uint64_t curr_ts, curr_ts_seconds;
	struct knote_footprint_history *footprint_history = (struct knote_footprint_history *)kn->kn_ext;
	if (knote_pressure_level != NOTE_MEMORYSTATUS_PRESSURE_NORMAL) {
		timestamps = (uint32_t *)&(kn->kn_sdata);
		index = knote_pressure_level == NOTE_MEMORYSTATUS_PRESSURE_WARN ?
		    KNOTE_SEND_TIMESTAMP_WARNING_INDEX : KNOTE_SEND_TIMESTAMP_CRITICAL_INDEX;
		if (timestamps[index] == 0) {
			/* First notification for this level since pressure elevated from normal. */
			curr_ts = mach_absolute_time();
			curr_ts_seconds = 0;
			absolutetime_to_nanoseconds(curr_ts, &curr_ts_seconds);
			curr_ts_seconds /= NSEC_PER_SEC;

			timestamps[index] = (uint32_t)MIN(UINT32_MAX, curr_ts_seconds);

			/* Record task initial footprint */
			if (timestamps[index == KNOTE_SEND_TIMESTAMP_WARNING_INDEX ? KNOTE_SEND_TIMESTAMP_CRITICAL_INDEX : KNOTE_SEND_TIMESTAMP_WARNING_INDEX] == 0) {
				/*
				 * First notification at any level since pressure elevated from normal.
				 * Record the footprint and our order in the notification list.
				 */
				footprint_history->kfh_starting_footprint = (uint32_t) MIN(UINT32_MAX, get_task_phys_footprint(task) / (2UL << 20));
				footprint_history->kfh_notification_order = order_within_list;
			}
		}
	}
	footprint_history->kfh_num_notifications++;
}

/*
 * Records the current footprint for this task in the knote telemetry.
 *
 * Returns the soonest absolutetime when this footprint history should be updated again.
 */
static uint64_t
update_knote_footprint_history(struct knote *kn, task_t task, uint64_t curr_ts)
{
	uint32_t *timestamps = (uint32_t *)&(kn->kn_sdata);
	struct knote_footprint_history *footprint_history = (struct knote_footprint_history *)kn->kn_ext;
	uint64_t warning_send_time, critical_send_time, minutes_since_warning = UINT64_MAX, minutes_since_critical = UINT64_MAX;
	warning_send_time = timestamps[KNOTE_SEND_TIMESTAMP_WARNING_INDEX];
	critical_send_time = timestamps[KNOTE_SEND_TIMESTAMP_CRITICAL_INDEX];
	uint32_t task_phys_footprint_mb = (uint32_t) MIN(UINT32_MAX, get_task_phys_footprint(task) / (2UL << 20));
	uint64_t next_run = UINT64_MAX, absolutetime_in_minute = 0, minutes_since_last_notification = 0, curr_ts_s;
	absolutetime_to_nanoseconds(curr_ts, &curr_ts_s);
	nanoseconds_to_absolutetime(60 * NSEC_PER_SEC, &absolutetime_in_minute);
	curr_ts_s /= NSEC_PER_SEC;

	if (warning_send_time != 0) {
		/* This task received a warning notification. */
		minutes_since_warning = (curr_ts_s - warning_send_time) / 60;
		if (footprint_history->kfh_footprint_after_warn_1 == 0 && minutes_since_warning >= 1) {
			footprint_history->kfh_footprint_after_warn_1 = task_phys_footprint_mb;
		}
		if (footprint_history->kfh_footprint_after_warn_5 == 0 && minutes_since_warning >= 5) {
			footprint_history->kfh_footprint_after_warn_5 = task_phys_footprint_mb;
		}
		if (footprint_history->kfh_footprint_after_warn_20 == 0 && minutes_since_warning >= 20) {
			footprint_history->kfh_footprint_after_warn_20 = task_phys_footprint_mb;
		}
	}
	if (critical_send_time != 0) {
		/* This task received a critical notification. */
		minutes_since_critical = (curr_ts_s - critical_send_time) / 60;
		if (footprint_history->kfh_footprint_after_critical_1 == 0 && minutes_since_critical >= 1) {
			footprint_history->kfh_footprint_after_critical_1 = task_phys_footprint_mb;
		}
		if (footprint_history->kfh_footprint_after_critical_5 == 0 && minutes_since_critical >= 5) {
			footprint_history->kfh_footprint_after_critical_5 = task_phys_footprint_mb;
		}
		if (footprint_history->kfh_footprint_after_critical_20 == 0 && minutes_since_critical >= 20) {
			footprint_history->kfh_footprint_after_critical_20 = task_phys_footprint_mb;
		}
	}

	minutes_since_last_notification = MIN(minutes_since_warning, minutes_since_critical);
	if (minutes_since_last_notification < 20) {
		if (minutes_since_last_notification < 5) {
			if (minutes_since_last_notification < 1) {
				next_run = curr_ts + absolutetime_in_minute;
			} else {
				next_run = curr_ts + (absolutetime_in_minute * 5);
			}
		} else {
			next_run = curr_ts + (absolutetime_in_minute * 20);
		}
	}

	return next_run;
}

extern char *proc_name_address(void *p);
/*
 * Attempt to send the given level telemetry event.
 * Finalizes the duration.
 * Clears the src_event struct.
 */
static void
memorystatus_pressure_interval_send(CA_EVENT_TYPE(memorystatus_pressure_interval) *src_event)
{
	uint64_t duration_nanoseconds = 0;
	uint64_t             curr_ts = mach_absolute_time();
	src_event->duration = curr_ts - src_event->duration;
	absolutetime_to_nanoseconds(src_event->duration, &duration_nanoseconds);
	src_event->duration = (int64_t) (duration_nanoseconds / NSEC_PER_SEC);

	/*
	 * Drop the event rather than block for memory. We should be in a normal pressure level now,
	 * but we don't want to end up blocked in page_wait if there's a sudden spike in pressure.
	 */
	ca_event_t event_wrapper = CA_EVENT_ALLOCATE_FLAGS(memorystatus_pressure_interval, Z_NOWAIT);
	if (event_wrapper) {
		memcpy(event_wrapper->data, src_event, sizeof(CA_EVENT_TYPE(memorystatus_pressure_interval)));
		CA_EVENT_SEND(event_wrapper);
	}
	src_event->num_processes_registered = 0;
	src_event->num_notifications_sent = 0;
	src_event->max_level = 0;
	src_event->num_transitions = 0;
	src_event->num_kills = 0;
	src_event->duration = 0;
}


/*
 * Attempt to send the per-proc telemetry events.
 * Clears the footprint histories on the knotes.
 */
static void
memorystatus_pressure_proc_telemetry_send(void)
{
	struct knote *kn = NULL;
	memorystatus_klist_lock();
	SLIST_FOREACH(kn, &memorystatus_klist, kn_selnext) {
		proc_t            p = PROC_NULL;
		struct knote_footprint_history *footprint_history = (struct knote_footprint_history *)kn->kn_ext;
		uint32_t *timestamps = (uint32_t *)&(kn->kn_sdata);
		uint32_t warning_send_time = timestamps[KNOTE_SEND_TIMESTAMP_WARNING_INDEX];
		uint32_t critical_send_time = timestamps[KNOTE_SEND_TIMESTAMP_CRITICAL_INDEX];
		CA_EVENT_TYPE(memorystatus_proc_notification) * event = NULL;
		if (warning_send_time != 0 || critical_send_time != 0) {
			/*
			 * Drop the event rather than block for memory. We should be in a normal pressure level now,
			 * but we don't want to end up blocked in page_wait if there's a sudden spike in pressure.
			 */
			ca_event_t event_wrapper = CA_EVENT_ALLOCATE_FLAGS(memorystatus_proc_notification, Z_NOWAIT | Z_ZERO);
			if (event_wrapper) {
				event = event_wrapper->data;

				event->footprint_before_notification = footprint_history->kfh_starting_footprint;
				event->footprint_1_min_after_first_warning = footprint_history->kfh_footprint_after_warn_1;
				event->footprint_5_min_after_first_warning = footprint_history->kfh_footprint_after_warn_5;
				event->footprint_20_min_after_first_warning = footprint_history->kfh_footprint_after_warn_20;
				event->footprint_1_min_after_first_critical = footprint_history->kfh_footprint_after_critical_1;
				event->footprint_5_min_after_first_critical = footprint_history->kfh_footprint_after_critical_5;
				event->footprint_20_min_after_first_critical = footprint_history->kfh_footprint_after_critical_20;
				event->num_notifications_sent = footprint_history->kfh_num_notifications;
				if (warning_send_time != 0 && critical_send_time != 0) {
					event->time_between_warning_and_critical = (critical_send_time - warning_send_time) / 60; // Minutes
				}
				event->order_within_list = footprint_history->kfh_notification_order;

				p = proc_ref(knote_get_kq(kn)->kq_p, false);
				if (p == NULL) {
					CA_EVENT_DEALLOCATE(event_wrapper);
					continue;
				}
				strlcpy(event->proc_name, proc_name_address(p), sizeof(event->proc_name));

				proc_rele(p);
				CA_EVENT_SEND(event_wrapper);
			}
		}
		memset(footprint_history, 0, sizeof(*footprint_history));
		timestamps[KNOTE_SEND_TIMESTAMP_WARNING_INDEX] = 0;
		timestamps[KNOTE_SEND_TIMESTAMP_CRITICAL_INDEX] = 0;
	}
	memorystatus_klist_unlock();
}

/*
 * Send all telemetry associated with the increased pressure interval.
 */
static void
memorystatus_pressure_telemetry_send(void)
{
	LCK_MTX_ASSERT(&memorystatus_klist_mutex, LCK_MTX_ASSERT_NOTOWNED);
	memorystatus_pressure_interval_send(&memorystatus_pressure_interval_telemetry);
	memorystatus_pressure_proc_telemetry_send();
}


/*
 * kn_max - knote
 *
 * knote_pressure_level - to check if the knote is registered for this notification level.
 *
 * task    - task whose bits we'll be modifying
 *
 * pressure_level_to_clear - if the task has been notified of this past level, clear that notification bit so that if/when we revert to that level, the task will be notified again.
 *
 * pressure_level_to_set - the task is about to be notified of this new level. Update the task's bit notification information appropriately.
 *
 */

static boolean_t
is_knote_registered_modify_task_pressure_bits(struct knote *kn_max, int knote_pressure_level, task_t task, vm_pressure_level_t pressure_level_to_clear, vm_pressure_level_t pressure_level_to_set)
{
	if (kn_max->kn_sfflags & knote_pressure_level) {
		if (pressure_level_to_clear && task_has_been_notified(task, pressure_level_to_clear) == TRUE) {
			task_clear_has_been_notified(task, pressure_level_to_clear);
		}

		task_mark_has_been_notified(task, pressure_level_to_set);
		return TRUE;
	}

	return FALSE;
}

static void
memorystatus_klist_reset_all_for_level(vm_pressure_level_t pressure_level_to_clear)
{
	struct knote *kn = NULL;

	memorystatus_klist_lock();

	SLIST_FOREACH(kn, &memorystatus_klist, kn_selnext) {
		proc_t p = knote_get_kq(kn)->kq_p;

		if (p == proc_ref(p, false)) {
			task_clear_has_been_notified(proc_task(p), pressure_level_to_clear);
			proc_rele(p);
		}
	}

	memorystatus_klist_unlock();
}

/*
 * Used by the vm_pressure_thread which is
 * signalled from within vm_pageout_scan().
 */

void
consider_vm_pressure_events(void)
{
	vm_dispatch_memory_pressure();
}

static void
vm_dispatch_memory_pressure(void)
{
	memorystatus_update_vm_pressure(FALSE);
}

static struct knote *
vm_pressure_select_optimal_candidate_to_notify(struct klist *candidate_list, int level, boolean_t target_foreground_process, uint64_t *next_telemetry_update)
{
	struct knote    *kn = NULL, *kn_max = NULL;
	uint64_t    resident_max = 0;/* MB */
	int        selected_task_importance = 0;
	static int    pressure_snapshot = -1;
	boolean_t    pressure_increase = FALSE;
	uint64_t     curr_ts = mach_absolute_time();
	*next_telemetry_update = UINT64_MAX;

	if (pressure_snapshot == -1) {
		/*
		 * Initial snapshot.
		 */
		pressure_snapshot = level;
		pressure_increase = TRUE;
	} else {
		if (level && (level >= pressure_snapshot)) {
			pressure_increase = TRUE;
		} else {
			pressure_increase = FALSE;
		}

		pressure_snapshot = level;
	}

	if (pressure_increase == TRUE) {
		/*
		 * We'll start by considering the largest
		 * unimportant task in our list.
		 */
		selected_task_importance = INT_MAX;
	} else {
		/*
		 * We'll start by considering the largest
		 * important task in our list.
		 */
		selected_task_importance = 0;
	}

	SLIST_FOREACH(kn, candidate_list, kn_selnext) {
		uint64_t        resident_size = 0;/* MB */
		proc_t            p = PROC_NULL;
		struct task*        t = TASK_NULL;
		int            curr_task_importance = 0;
		uint64_t         telemetry_update = 0;
		boolean_t        consider_knote = FALSE;
		boolean_t        privileged_listener = FALSE;

		p = proc_ref(knote_get_kq(kn)->kq_p, false);
		if (p == PROC_NULL) {
			continue;
		}

#if CONFIG_MEMORYSTATUS
		if (target_foreground_process == TRUE && !memorystatus_is_foreground_locked(p)) {
			/*
			 * Skip process not marked foreground.
			 */
			proc_rele(p);
			continue;
		}
#endif /* CONFIG_MEMORYSTATUS */

		t = (struct task *)(proc_task(p));
		telemetry_update = update_knote_footprint_history(kn, t, curr_ts);
		*next_telemetry_update = MIN(*next_telemetry_update, telemetry_update);

		vm_pressure_level_t dispatch_level = convert_internal_pressure_level_to_dispatch_level(level);

		if ((kn->kn_sfflags & dispatch_level) == 0) {
			proc_rele(p);
			continue;
		}

#if CONFIG_MEMORYSTATUS
		if (target_foreground_process == FALSE && !memorystatus_bg_pressure_eligible(p)) {
			VM_PRESSURE_DEBUG(1, "[vm_pressure] skipping process %d\n", proc_getpid(p));
			proc_rele(p);
			continue;
		}
#endif /* CONFIG_MEMORYSTATUS */

#if XNU_TARGET_OS_OSX
		curr_task_importance = task_importance_estimate(t);
#else /* XNU_TARGET_OS_OSX */
		curr_task_importance = p->p_memstat_effectivepriority;
#endif /* XNU_TARGET_OS_OSX */

		/*
		 * Privileged listeners are only considered in the multi-level pressure scheme
		 * AND only if the pressure is increasing.
		 */
		if (level > 0) {
			if (task_has_been_notified(t, level) == FALSE) {
				/*
				 * Is this a privileged listener?
				 */
				if (task_low_mem_privileged_listener(t, FALSE, &privileged_listener) == 0) {
					if (privileged_listener) {
						kn_max = kn;
						proc_rele(p);
						goto done_scanning;
					}
				}
			} else {
				proc_rele(p);
				continue;
			}
		} else if (level == 0) {
			/*
			 * Task wasn't notified when the pressure was increasing and so
			 * no need to notify it that the pressure is decreasing.
			 */
			if ((task_has_been_notified(t, kVMPressureWarning) == FALSE) && (task_has_been_notified(t, kVMPressureCritical) == FALSE)) {
				proc_rele(p);
				continue;
			}
		}

		/*
		 * We don't want a small process to block large processes from
		 * being notified again. <rdar://problem/7955532>
		 */
		resident_size = (get_task_phys_footprint(t)) / (1024 * 1024ULL); /* MB */

		if (resident_size >= vm_pressure_task_footprint_min) {
			if (level > 0) {
				/*
				 * Warning or Critical Pressure.
				 */
				if (pressure_increase) {
					if ((curr_task_importance < selected_task_importance) ||
					    ((curr_task_importance == selected_task_importance) && (resident_size > resident_max))) {
						/*
						 * We have found a candidate process which is:
						 * a) at a lower importance than the current selected process
						 * OR
						 * b) has importance equal to that of the current selected process but is larger
						 */

						consider_knote = TRUE;
					}
				} else {
					if ((curr_task_importance > selected_task_importance) ||
					    ((curr_task_importance == selected_task_importance) && (resident_size > resident_max))) {
						/*
						 * We have found a candidate process which is:
						 * a) at a higher importance than the current selected process
						 * OR
						 * b) has importance equal to that of the current selected process but is larger
						 */

						consider_knote = TRUE;
					}
				}
			} else if (level == 0) {
				/*
				 * Pressure back to normal.
				 */
				if ((curr_task_importance > selected_task_importance) ||
				    ((curr_task_importance == selected_task_importance) && (resident_size > resident_max))) {
					consider_knote = TRUE;
				}
			}

			if (consider_knote) {
				resident_max = resident_size;
				kn_max = kn;
				selected_task_importance = curr_task_importance;
				consider_knote = FALSE; /* reset for the next candidate */
			}
		} else {
			/* There was no candidate with enough resident memory to scavenge */
			VM_PRESSURE_DEBUG(0, "[vm_pressure] threshold failed for pid %d with %llu resident...\n", proc_getpid(p), resident_size);
		}
		proc_rele(p);
	}

done_scanning:
	if (kn_max) {
		VM_DEBUG_CONSTANT_EVENT(vm_pressure_event, DBG_VM_PRESSURE_EVENT, DBG_FUNC_NONE, proc_getpid(knote_get_kq(kn_max)->kq_p), resident_max, 0, 0);
		VM_PRESSURE_DEBUG(1, "[vm_pressure] sending event to pid %d with %llu resident\n", proc_getpid(knote_get_kq(kn_max)->kq_p), resident_max);
	}

	return kn_max;
}

/*
 * To avoid notification storms in a system with sawtooth behavior of pressure levels eg:
 * Normal -> warning (notify clients) -> critical (notify) -> warning (notify) -> critical (notify) -> warning (notify)...
 *
 * We have 'resting' periods: WARNING_NOTIFICATION_RESTING_PERIOD and CRITICAL_NOTIFICATION_RESTING_PERIOD
 *
 * So it would look like:-
 * Normal -> warning (notify) -> critical (notify) -> warning (notify if it has been RestPeriod since last warning) -> critical (notify if it has been RestPeriod since last critical) -> ...
 *
 * That's what these 2 timestamps below signify.
 */

uint64_t next_warning_notification_sent_at_ts = 0;
uint64_t next_critical_notification_sent_at_ts = 0;

boolean_t        memorystatus_manual_testing_on = FALSE;
vm_pressure_level_t    memorystatus_manual_testing_level = kVMPressureNormal;

unsigned int memorystatus_sustained_pressure_maximum_band = JETSAM_PRIORITY_IDLE;
#if DEVELOPMENT || DEBUG
SYSCTL_INT(_kern, OID_AUTO, memorystatus_sustained_pressure_maximum_band, CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_LOCKED, &memorystatus_sustained_pressure_maximum_band, 0, "");
#endif /* DEVELOPMENT || DEBUG */

#if CONFIG_JETSAM

/*
 * TODO(jason): The memorystatus thread should be responsible for this
 * It can just check how long the pressure level has been at warning and the timestamp
 * of the last sustained pressure kill.
 */
static void
sustained_pressure_handler(void* arg0 __unused, void* arg1 __unused)
{
	int max_kills = 0, kill_count = 0;
	/*
	 * Pressure has been elevated for too long.
	 * We don't want to leave the system in this state as it can delay background
	 * work indefinitely & drain battery.
	 *
	 * Try to return the system to normal via jetsam.
	 * We'll run through the idle band up to 2 times.
	 * If the pressure hasn't been relieved by then, the problem is memory
	 * consumption in a higher band and this churn is probably doing more harm than good.
	 */
	max_kills = memorystatus_get_proccnt_upto_priority(memorystatus_sustained_pressure_maximum_band) * 2;
	memorystatus_log("memorystatus: Pressure level has been elevated for too long. killing up to %d idle processes\n", max_kills);
	while (memorystatus_vm_pressure_level != kVMPressureNormal && kill_count < max_kills) {
		boolean_t killed = memorystatus_kill_on_sustained_pressure();
		if (killed) {
			/*
			 * Pause before our next kill & see if pressure reduces.
			 */
			delay((int)(memorystatus_kill_on_sustained_pressure_delay_ms * NSEC_PER_MSEC / NSEC_PER_USEC));
			kill_count++;
			memorystatus_kill_on_sustained_pressure_count++;
			/* TODO(jason): Should use os_atomic but requires rdar://76310894. */
			memorystatus_pressure_interval_telemetry.num_kills++;
		} else {
			/* Nothing left to kill */
			break;
		}
	}
	if (memorystatus_vm_pressure_level != kVMPressureNormal) {
		memorystatus_log("memorystatus: Killed %d idle processes due to sustained pressure, but device didn't quiesce. Giving up.\n", kill_count);
	}
}

#endif /* CONFIG_JETSAM */

/*
 * Returns the number of processes registered for notifications at this level.
 */
static size_t
memorystatus_klist_length(int level)
{
	LCK_MTX_ASSERT(&memorystatus_klist_mutex, LCK_MTX_ASSERT_OWNED);
	struct knote *kn;
	size_t count = 0;
	int knote_pressure_level = convert_internal_pressure_level_to_dispatch_level(level);
	SLIST_FOREACH(kn, &memorystatus_klist, kn_selnext) {
		if (kn->kn_sfflags & knote_pressure_level) {
			count++;
		}
	}
	return count;
}

/*
 * Updates the footprint telemetry for procs that have received notifications.
 */
static void
update_footprints_for_telemetry(void* arg0 __unused, void* arg1 __unused)
{
	uint64_t curr_ts = mach_absolute_time(), next_telemetry_update = UINT64_MAX;
	struct knote *kn;

	memorystatus_klist_lock();
	SLIST_FOREACH(kn, &memorystatus_klist, kn_selnext) {
		proc_t            p = PROC_NULL;
		struct task*      t = TASK_NULL;
		uint64_t telemetry_update;

		p = proc_ref(knote_get_kq(kn)->kq_p, false);
		if (p == PROC_NULL) {
			continue;
		}
		t = (struct task *)(proc_task(p));
		proc_rele(p);
		p = PROC_NULL;
		telemetry_update = update_knote_footprint_history(kn, t, curr_ts);
		next_telemetry_update = MIN(next_telemetry_update, telemetry_update);
	}
	memorystatus_klist_unlock();
	if (next_telemetry_update != UINT64_MAX) {
		uint64_t next_update_seconds;
		absolutetime_to_nanoseconds(next_telemetry_update, &next_update_seconds);
		next_update_seconds /= NSEC_PER_SEC;
		thread_call_enter_delayed(memorystatus_notify_update_telemetry_thread_call, next_telemetry_update);
	}
}

kern_return_t
memorystatus_update_vm_pressure(boolean_t target_foreground_process)
{
	struct knote            *kn_max = NULL;
	struct knote            *kn_cur = NULL, *kn_temp = NULL;/* for safe list traversal */
	pid_t                target_pid = -1;
	struct klist            dispatch_klist = { NULL };
	proc_t                target_proc = PROC_NULL;
	struct task            *task = NULL;
	boolean_t            found_candidate = FALSE;

	static vm_pressure_level_t     level_snapshot = kVMPressureNormal;
	static vm_pressure_level_t    prev_level_snapshot = kVMPressureNormal;
	boolean_t            smoothing_window_started = FALSE;
	struct timeval            smoothing_window_start_tstamp = {0, 0};
	struct timeval            curr_tstamp = {0, 0};
	int64_t              elapsed_msecs = 0;
	uint64_t             curr_ts = mach_absolute_time(), next_telemetry_update = UINT64_MAX;


	uint64_t logging_now;
	absolutetime_to_nanoseconds(curr_ts, &logging_now);
#if !CONFIG_JETSAM
#define MAX_IDLE_KILLS 100    /* limit the number of idle kills allowed */

	int    idle_kill_counter = 0;

	/*
	 * On desktop we take this opportunity to free up memory pressure
	 * by immediately killing idle exitable processes. We use a delay
	 * to avoid overkill.  And we impose a max counter as a fail safe
	 * in case daemons re-launch too fast.
	 */
	while ((memorystatus_vm_pressure_level != kVMPressureNormal) && (idle_kill_counter < MAX_IDLE_KILLS)) {
		if (memorystatus_idle_exit_from_VM() == FALSE) {
			/* No idle exitable processes left to kill */
			break;
		}
		idle_kill_counter++;

		if (memorystatus_manual_testing_on == TRUE) {
			/*
			 * Skip the delay when testing
			 * the pressure notification scheme.
			 */
		} else {
			delay(1000000); /* 1 second */
		}
	}
#endif /* !CONFIG_JETSAM */

	if (level_snapshot != kVMPressureNormal) {
		/*
		 * Check to see if we are still in the 'resting' period
		 * after having notified all clients interested in
		 * a particular pressure level.
		 */

		level_snapshot = memorystatus_vm_pressure_level;

		if (level_snapshot == kVMPressureWarning || level_snapshot == kVMPressureUrgent) {
			if (next_warning_notification_sent_at_ts) {
				if (curr_ts < next_warning_notification_sent_at_ts) {
					delay(INTER_NOTIFICATION_DELAY * 4 /* 1 sec */);
					return KERN_SUCCESS;
				}

				next_warning_notification_sent_at_ts = 0;
				memorystatus_klist_reset_all_for_level(kVMPressureWarning);
			}
		} else if (level_snapshot == kVMPressureCritical) {
			if (next_critical_notification_sent_at_ts) {
				if (curr_ts < next_critical_notification_sent_at_ts) {
					delay(INTER_NOTIFICATION_DELAY * 4 /* 1 sec */);
					return KERN_SUCCESS;
				}
				next_critical_notification_sent_at_ts = 0;
				memorystatus_klist_reset_all_for_level(kVMPressureCritical);
			}
		}
	}

#if CONFIG_JETSAM
	if (memorystatus_vm_pressure_level == kVMPressureNormal && prev_level_snapshot != kVMPressureNormal) {
		if (memorystatus_should_kill_on_sustained_pressure) {
			memorystatus_log("memorystatus: Pressure has returned to level %d. Cancelling scheduled jetsam\n", memorystatus_vm_pressure_level);
			thread_call_cancel(sustained_pressure_handler_thread_call);
		}
	} else if (memorystatus_should_kill_on_sustained_pressure && memorystatus_vm_pressure_level != kVMPressureNormal && prev_level_snapshot == kVMPressureNormal) {
		/*
		 * Pressure has increased from normal.
		 * Hopefully the notifications will relieve it,
		 * but as a fail-safe we'll trigger jetsam
		 * after a configurable amount of time.
		 */
		memorystatus_log("memorystatus: Pressure level has increased from %d to %d. Scheduling jetsam.\n", prev_level_snapshot, memorystatus_vm_pressure_level);
		uint64_t kill_time;
		nanoseconds_to_absolutetime(memorystatus_kill_on_sustained_pressure_window_s * NSEC_PER_SEC, &kill_time);
		kill_time += mach_absolute_time();
		thread_call_enter_delayed(sustained_pressure_handler_thread_call, kill_time);
	}
#endif /* CONFIG_JETSAM */

	while (1) {
		/*
		 * There is a race window here. But it's not clear
		 * how much we benefit from having extra synchronization.
		 */
		level_snapshot = memorystatus_vm_pressure_level;

		if (prev_level_snapshot > level_snapshot) {
			/*
			 * Pressure decreased? Let's take a little breather
			 * and see if this condition stays.
			 */
			if (smoothing_window_started == FALSE) {
				smoothing_window_started = TRUE;
				microuptime(&smoothing_window_start_tstamp);
			}

			microuptime(&curr_tstamp);
			timevalsub(&curr_tstamp, &smoothing_window_start_tstamp);
			elapsed_msecs = curr_tstamp.tv_sec * 1000 + curr_tstamp.tv_usec / 1000;

			if (elapsed_msecs < VM_PRESSURE_DECREASED_SMOOTHING_PERIOD) {
				delay(INTER_NOTIFICATION_DELAY);
				continue;
			}
		}
		if (level_snapshot == kVMPressureNormal) {
			memorystatus_pressure_telemetry_send();
		}
		prev_level_snapshot = level_snapshot;
		smoothing_window_started = FALSE;
		memorystatus_klist_lock();

		if (level_snapshot > memorystatus_pressure_interval_telemetry.max_level) {
			memorystatus_pressure_interval_telemetry.num_processes_registered = memorystatus_klist_length(level_snapshot);
			memorystatus_pressure_interval_telemetry.max_level = level_snapshot;
			memorystatus_pressure_interval_telemetry.num_transitions++;
			if (memorystatus_pressure_interval_telemetry.duration == 0) {
				/* Set the start timestamp. Duration will be finalized when we send the event. */
				memorystatus_pressure_interval_telemetry.duration = curr_ts;
			}
		}

		kn_max = vm_pressure_select_optimal_candidate_to_notify(&memorystatus_klist, level_snapshot, target_foreground_process, &next_telemetry_update);

		if (kn_max == NULL) {
			memorystatus_klist_unlock();

			/*
			 * No more level-based clients to notify.
			 *
			 * Start the 'resting' window within which clients will not be re-notified.
			 */

			if (level_snapshot != kVMPressureNormal) {
				if (level_snapshot == kVMPressureWarning || level_snapshot == kVMPressureUrgent) {
					nanoseconds_to_absolutetime(WARNING_NOTIFICATION_RESTING_PERIOD * NSEC_PER_SEC, &curr_ts);

					/* Next warning notification (if nothing changes) won't be sent before...*/
					next_warning_notification_sent_at_ts = mach_absolute_time() + curr_ts;
				}

				if (level_snapshot == kVMPressureCritical) {
					nanoseconds_to_absolutetime(CRITICAL_NOTIFICATION_RESTING_PERIOD * NSEC_PER_SEC, &curr_ts);

					/* Next critical notification (if nothing changes) won't be sent before...*/
					next_critical_notification_sent_at_ts = mach_absolute_time() + curr_ts;
				}
			}
			absolutetime_to_nanoseconds(mach_absolute_time(), &logging_now);
			if (next_telemetry_update != UINT64_MAX) {
				thread_call_enter_delayed(memorystatus_notify_update_telemetry_thread_call, next_telemetry_update);
			} else {
				thread_call_cancel(memorystatus_notify_update_telemetry_thread_call);
			}
			return KERN_FAILURE;
		}

		target_proc = proc_ref(knote_get_kq(kn_max)->kq_p, false);
		if (target_proc == PROC_NULL) {
			memorystatus_klist_unlock();
			continue;
		}

		target_pid = proc_getpid(target_proc);

		task = (struct task *)(proc_task(target_proc));

		if (level_snapshot != kVMPressureNormal) {
			if (level_snapshot == kVMPressureWarning || level_snapshot == kVMPressureUrgent) {
				if (is_knote_registered_modify_task_pressure_bits(kn_max, NOTE_MEMORYSTATUS_PRESSURE_WARN, task, 0, kVMPressureWarning) == TRUE) {
					found_candidate = TRUE;
				}
			} else {
				if (level_snapshot == kVMPressureCritical) {
					if (is_knote_registered_modify_task_pressure_bits(kn_max, NOTE_MEMORYSTATUS_PRESSURE_CRITICAL, task, 0, kVMPressureCritical) == TRUE) {
						found_candidate = TRUE;
					}
				}
			}
		} else {
			if (kn_max->kn_sfflags & NOTE_MEMORYSTATUS_PRESSURE_NORMAL) {
				task_clear_has_been_notified(task, kVMPressureWarning);
				task_clear_has_been_notified(task, kVMPressureCritical);

				found_candidate = TRUE;
			}
		}

		if (found_candidate == FALSE) {
			proc_rele(target_proc);
			memorystatus_klist_unlock();
			continue;
		}

		SLIST_FOREACH_SAFE(kn_cur, &memorystatus_klist, kn_selnext, kn_temp) {
			int knote_pressure_level = convert_internal_pressure_level_to_dispatch_level(level_snapshot);

			if (is_knote_registered_modify_task_pressure_bits(kn_cur, knote_pressure_level, task, 0, level_snapshot) == TRUE) {
				proc_t knote_proc = knote_get_kq(kn_cur)->kq_p;
				pid_t knote_pid = proc_getpid(knote_proc);
				if (knote_pid == target_pid) {
					KNOTE_DETACH(&memorystatus_klist, kn_cur);
					KNOTE_ATTACH(&dispatch_klist, kn_cur);
				}
			}
		}
		if (level_snapshot != kVMPressureNormal) {
			mark_knote_send_time(kn_max, task, convert_internal_pressure_level_to_dispatch_level(level_snapshot),
			    (uint16_t) MIN(UINT16_MAX, memorystatus_pressure_interval_telemetry.num_notifications_sent));
			memorystatus_pressure_interval_telemetry.num_notifications_sent++;
		}

		KNOTE(&dispatch_klist, (level_snapshot != kVMPressureNormal) ? kMemorystatusPressure : kMemorystatusNoPressure);

		SLIST_FOREACH_SAFE(kn_cur, &dispatch_klist, kn_selnext, kn_temp) {
			KNOTE_DETACH(&dispatch_klist, kn_cur);
			KNOTE_ATTACH(&memorystatus_klist, kn_cur);
		}

		memorystatus_klist_unlock();

		microuptime(&target_proc->vm_pressure_last_notify_tstamp);
		proc_rele(target_proc);

		if (memorystatus_manual_testing_on == TRUE && target_foreground_process == TRUE) {
			break;
		}

		if (memorystatus_manual_testing_on == TRUE) {
			/*
			 * Testing out the pressure notification scheme.
			 * No need for delays etc.
			 */
		} else {
			uint32_t sleep_interval = INTER_NOTIFICATION_DELAY;
#if CONFIG_JETSAM
			unsigned int page_delta = 0;
			unsigned int skip_delay_page_threshold = 0;

			assert(memorystatus_available_pages_pressure >= memorystatus_available_pages_critical_base);

			page_delta = (memorystatus_available_pages_pressure - memorystatus_available_pages_critical_base) / 2;
			skip_delay_page_threshold = memorystatus_available_pages_pressure - page_delta;

			if (memorystatus_available_pages <= skip_delay_page_threshold) {
				/*
				 * We are nearing the critcal mark fast and can't afford to wait between
				 * notifications.
				 */
				sleep_interval = 0;
			}
#endif /* CONFIG_JETSAM */

			if (sleep_interval) {
				delay(sleep_interval);
			}
		}
	}

	return KERN_SUCCESS;
}

static uint32_t
convert_internal_pressure_level_to_dispatch_level(vm_pressure_level_t internal_pressure_level)
{
	uint32_t dispatch_level = NOTE_MEMORYSTATUS_PRESSURE_NORMAL;

	switch (internal_pressure_level) {
	case kVMPressureNormal:
	{
		dispatch_level = NOTE_MEMORYSTATUS_PRESSURE_NORMAL;
		break;
	}

	case kVMPressureWarning:
	case kVMPressureUrgent:
	{
		dispatch_level = NOTE_MEMORYSTATUS_PRESSURE_WARN;
		break;
	}

	case kVMPressureCritical:
	{
		dispatch_level = NOTE_MEMORYSTATUS_PRESSURE_CRITICAL;
		break;
	}

	default:
		break;
	}

	return dispatch_level;
}

/*
 * Issue a wakeup to any threads listening for jetsam pressure via
 * `mach_vm_pressure_level_monitor`. Subscribers should respond to these
 * notifications by freeing cached memory.
 */
void
memorystatus_broadcast_jetsam_pressure(vm_pressure_level_t pressure_level)
{
	uint64_t now;
	uint32_t *waiters = NULL;
	uint64_t *last_notification_ns = NULL;

	switch (pressure_level) {
	case kVMPressureForegroundJetsam:
		waiters = &memorystatus_jetsam_fg_band_waiters;
		last_notification_ns = &memorystatus_jetsam_fg_band_timestamp_ns;
		break;
	case kVMPressureBackgroundJetsam:
		waiters = &memorystatus_jetsam_bg_band_waiters;
		last_notification_ns = &memorystatus_jetsam_bg_band_timestamp_ns;
		break;
	default:
		panic("Unexpected non-jetsam pressure level %d", pressure_level);
	}

	lck_mtx_lock(&memorystatus_jetsam_broadcast_lock);
	absolutetime_to_nanoseconds(mach_absolute_time(), &now);

	if (now - *last_notification_ns < memorystatus_jetsam_notification_delay_ns) {
		lck_mtx_unlock(&memorystatus_jetsam_broadcast_lock);
		return;
	}

	if (*waiters > 0) {
		memorystatus_log("memorystatus: issuing %s jetsam pressure notification to %d waiters",
		    pressure_level == kVMPressureForegroundJetsam ?
		    "foreground" : "background", *waiters);
		thread_wakeup((event_t)waiters);
		*waiters = 0;
		*last_notification_ns = now;
	}
	lck_mtx_unlock(&memorystatus_jetsam_broadcast_lock);
}

/*
 * Memorystatus notification debugging support
 */

#if DEVELOPMENT || DEBUG

static int
sysctl_memorystatus_broadcast_jetsam_pressure SYSCTL_HANDLER_ARGS
{
	int error = 0;
	vm_pressure_level_t pressure_level;

	error = SYSCTL_IN(req, &pressure_level, sizeof(pressure_level));
	if (error) {
		return error;
	}

	if (pressure_level == kVMPressureForegroundJetsam ||
	    pressure_level == kVMPressureBackgroundJetsam) {
		memorystatus_broadcast_jetsam_pressure(pressure_level);
	} else {
		return EINVAL;
	}

	return SYSCTL_OUT(req, &pressure_level, sizeof(pressure_level));
}

SYSCTL_PROC(_kern, OID_AUTO, memorystatus_broadcast_jetsam_pressure,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MASKED | CTLFLAG_LOCKED,
    0, 0, &sysctl_memorystatus_broadcast_jetsam_pressure, "I", "");

#endif /* DEVELOPMENT || DEBUG */

static int
sysctl_memorystatus_vm_pressure_level SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1, arg2, oidp)
#if !XNU_TARGET_OS_OSX
	int error = 0;

	error = priv_check_cred(kauth_cred_get(), PRIV_VM_PRESSURE, 0);
	if (error) {
		return error;
	}

#endif /* !XNU_TARGET_OS_OSX */
	uint32_t dispatch_level = convert_internal_pressure_level_to_dispatch_level(memorystatus_vm_pressure_level);

	return SYSCTL_OUT(req, &dispatch_level, sizeof(dispatch_level));
}

#if DEBUG || DEVELOPMENT

SYSCTL_PROC(_kern, OID_AUTO, memorystatus_vm_pressure_level, CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_LOCKED,
    0, 0, &sysctl_memorystatus_vm_pressure_level, "I", "");

#else /* DEBUG || DEVELOPMENT */

SYSCTL_PROC(_kern, OID_AUTO, memorystatus_vm_pressure_level, CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_LOCKED | CTLFLAG_MASKED,
    0, 0, &sysctl_memorystatus_vm_pressure_level, "I", "");

#endif /* DEBUG || DEVELOPMENT */

/*
 * Trigger levels to test the mechanism.
 * Can be used via a sysctl.
 */
#define TEST_LOW_MEMORY_TRIGGER_ONE        1
#define TEST_LOW_MEMORY_TRIGGER_ALL        2
#define TEST_PURGEABLE_TRIGGER_ONE        3
#define TEST_PURGEABLE_TRIGGER_ALL        4
#define TEST_LOW_MEMORY_PURGEABLE_TRIGGER_ONE    5
#define TEST_LOW_MEMORY_PURGEABLE_TRIGGER_ALL    6

static int
sysctl_memorypressure_manual_trigger SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1, arg2)

	int level = 0;
	int error = 0;
	int pressure_level = 0;
	int trigger_request = 0;
	int force_purge;

	error = sysctl_handle_int(oidp, &level, 0, req);
	if (error || !req->newptr) {
		return error;
	}

	memorystatus_manual_testing_on = TRUE;

	trigger_request = (level >> 16) & 0xFFFF;
	pressure_level = (level & 0xFFFF);

	if (trigger_request < TEST_LOW_MEMORY_TRIGGER_ONE ||
	    trigger_request > TEST_LOW_MEMORY_PURGEABLE_TRIGGER_ALL) {
		return EINVAL;
	}
	switch (pressure_level) {
	case NOTE_MEMORYSTATUS_PRESSURE_NORMAL:
	case NOTE_MEMORYSTATUS_PRESSURE_WARN:
	case NOTE_MEMORYSTATUS_PRESSURE_CRITICAL:
		break;
	default:
		return EINVAL;
	}

	/*
	 * The pressure level is being set from user-space.
	 * And user-space uses the constants in sys/event.h
	 * So we translate those events to our internal levels here.
	 */
	if (pressure_level == NOTE_MEMORYSTATUS_PRESSURE_NORMAL) {
		memorystatus_manual_testing_level = kVMPressureNormal;
		force_purge = 0;
	} else if (pressure_level == NOTE_MEMORYSTATUS_PRESSURE_WARN) {
		memorystatus_manual_testing_level = kVMPressureWarning;
		force_purge = vm_pageout_state.memorystatus_purge_on_warning;
	} else if (pressure_level == NOTE_MEMORYSTATUS_PRESSURE_CRITICAL) {
		memorystatus_manual_testing_level = kVMPressureCritical;
		force_purge = vm_pageout_state.memorystatus_purge_on_critical;
	}

	memorystatus_vm_pressure_level = memorystatus_manual_testing_level;

	/* purge according to the new pressure level */
	switch (trigger_request) {
	case TEST_PURGEABLE_TRIGGER_ONE:
	case TEST_LOW_MEMORY_PURGEABLE_TRIGGER_ONE:
		if (force_purge == 0) {
			/* no purging requested */
			break;
		}
		vm_purgeable_object_purge_one_unlocked(force_purge);
		break;
	case TEST_PURGEABLE_TRIGGER_ALL:
	case TEST_LOW_MEMORY_PURGEABLE_TRIGGER_ALL:
		if (force_purge == 0) {
			/* no purging requested */
			break;
		}
		while (vm_purgeable_object_purge_one_unlocked(force_purge)) {
			;
		}
		break;
	}

	if ((trigger_request == TEST_LOW_MEMORY_TRIGGER_ONE) ||
	    (trigger_request == TEST_LOW_MEMORY_PURGEABLE_TRIGGER_ONE)) {
		memorystatus_update_vm_pressure(TRUE);
	}

	if ((trigger_request == TEST_LOW_MEMORY_TRIGGER_ALL) ||
	    (trigger_request == TEST_LOW_MEMORY_PURGEABLE_TRIGGER_ALL)) {
		while (memorystatus_update_vm_pressure(FALSE) == KERN_SUCCESS) {
			continue;
		}
	}

	if (pressure_level == NOTE_MEMORYSTATUS_PRESSURE_NORMAL) {
		memorystatus_manual_testing_on = FALSE;
	}

	return 0;
}

SYSCTL_PROC(_kern, OID_AUTO, memorypressure_manual_trigger, CTLTYPE_INT | CTLFLAG_WR | CTLFLAG_LOCKED | CTLFLAG_MASKED,
    0, 0, &sysctl_memorypressure_manual_trigger, "I", "");


SYSCTL_INT(_kern, OID_AUTO, memorystatus_purge_on_warning, CTLFLAG_RW | CTLFLAG_LOCKED, &vm_pageout_state.memorystatus_purge_on_warning, 0, "");
SYSCTL_INT(_kern, OID_AUTO, memorystatus_purge_on_urgent, CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_LOCKED, &vm_pageout_state.memorystatus_purge_on_urgent, 0, "");
SYSCTL_INT(_kern, OID_AUTO, memorystatus_purge_on_critical, CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_LOCKED, &vm_pageout_state.memorystatus_purge_on_critical, 0, "");

extern int vm_pressure_level_transition_threshold;
SYSCTL_INT(_kern, OID_AUTO, vm_pressure_level_transition_threshold, CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_LOCKED, &vm_pressure_level_transition_threshold, 0, "");

#if DEBUG || DEVELOPMENT
SYSCTL_UINT(_kern, OID_AUTO, memorystatus_vm_pressure_events_enabled, CTLFLAG_RW | CTLFLAG_LOCKED, &vm_pressure_events_enabled, 0, "");

#if 0
#if CONFIG_JETSAM && VM_PRESSURE_EVENTS
static boolean_t
memorystatus_issue_pressure_kevent(boolean_t pressured)
{
	memorystatus_klist_lock();
	KNOTE(&memorystatus_klist, pressured ? kMemorystatusPressure : kMemorystatusNoPressure);
	memorystatus_klist_unlock();
	return TRUE;
}
#endif /* CONFIG_JETSAM && VM_PRESSURE_EVENTS */
#endif /* 0 */

/*
 * This routine is used for targeted notifications regardless of system memory pressure
 * and regardless of whether or not the process has already been notified.
 * It bypasses and has no effect on the only-one-notification per soft-limit policy.
 *
 * "memnote" is the current user.
 */

static int
sysctl_memorystatus_vm_pressure_send SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1, arg2)
	/* Need to be root or have memorystatus entitlement */
	if (!kauth_cred_issuser(kauth_cred_get()) && !IOCurrentTaskHasEntitlement(MEMORYSTATUS_ENTITLEMENT)) {
		return EPERM;
	}

	int error = 0, pid = 0;
	struct knote *kn = NULL;
	boolean_t found_knote = FALSE;
	int fflags = 0;    /* filter flags for EVFILT_MEMORYSTATUS */
	uint64_t value = 0;

	error = sysctl_handle_quad(oidp, &value, 0, req);
	if (error || !req->newptr) {
		return error;
	}

	/*
	 * Find the pid in the low 32 bits of value passed in.
	 */
	pid = (int)(value & 0xFFFFFFFF);

	/*
	 * Find notification in the high 32 bits of the value passed in.
	 */
	fflags = (int)((value >> 32) & 0xFFFFFFFF);

	/*
	 * For backwards compatibility, when no notification is
	 * passed in, default to the NOTE_MEMORYSTATUS_PRESSURE_WARN
	 */
	if (fflags == 0) {
		fflags = NOTE_MEMORYSTATUS_PRESSURE_WARN;
		// printf("memorystatus_vm_pressure_send: using default notification [0x%x]\n", fflags);
	}

	/* wake up everybody waiting for kVMPressureForegroundJetsam */
	if (fflags == NOTE_MEMORYSTATUS_JETSAM_FG_BAND) {
		memorystatus_broadcast_jetsam_pressure(kVMPressureForegroundJetsam);
		return error;
	}

	/*
	 * See event.h ... fflags for EVFILT_MEMORYSTATUS
	 */
	if (!((fflags == NOTE_MEMORYSTATUS_PRESSURE_NORMAL) ||
	    (fflags == NOTE_MEMORYSTATUS_PRESSURE_WARN) ||
	    (fflags == NOTE_MEMORYSTATUS_PRESSURE_CRITICAL) ||
	    (fflags == NOTE_MEMORYSTATUS_LOW_SWAP) ||
	    (fflags == NOTE_MEMORYSTATUS_PROC_LIMIT_WARN) ||
	    (fflags == NOTE_MEMORYSTATUS_PROC_LIMIT_CRITICAL) ||
	    (((fflags & NOTE_MEMORYSTATUS_MSL_STATUS) != 0 &&
	    ((fflags & ~NOTE_MEMORYSTATUS_MSL_STATUS) == 0))))) {
		memorystatus_log_error("memorystatus_vm_pressure_send: notification [0x%x] not supported\n", fflags);
		error = 1;
		return error;
	}

	/*
	 * Forcibly send pid a memorystatus notification.
	 */

	memorystatus_klist_lock();

	SLIST_FOREACH(kn, &memorystatus_klist, kn_selnext) {
		proc_t knote_proc = knote_get_kq(kn)->kq_p;
		pid_t knote_pid = proc_getpid(knote_proc);

		if (knote_pid == pid) {
			/*
			 * Forcibly send this pid a memorystatus notification.
			 */
			kn->kn_fflags = fflags;
			found_knote = TRUE;
		}
	}

	if (found_knote) {
		KNOTE(&memorystatus_klist, 0);
		memorystatus_log_debug("memorystatus_vm_pressure_send: (value 0x%llx) notification [0x%x] sent to process [%d]\n", value, fflags, pid);
		error = 0;
	} else {
		memorystatus_log_error("memorystatus_vm_pressure_send: (value 0x%llx) notification [0x%x] not sent to process [%d] (none registered?)\n", value, fflags, pid);
		error = 1;
	}

	memorystatus_klist_unlock();

	return error;
}

SYSCTL_PROC(_kern, OID_AUTO, memorystatus_vm_pressure_send, CTLTYPE_QUAD | CTLFLAG_WR | CTLFLAG_LOCKED | CTLFLAG_MASKED | CTLFLAG_ANYBODY,
    0, 0, &sysctl_memorystatus_vm_pressure_send, "Q", "");

#endif /* DEBUG || DEVELOPMENT */

#endif /* VM_PRESSURE_EVENTS */
