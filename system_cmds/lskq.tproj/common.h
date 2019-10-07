/*
 * Copyright (c) 2015 Apple Inc. All rights reserved.
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

#ifndef _LSKQ_COMMON_H_
#define _LSKQ_COMMON_H_

#include <stdint.h>

/*
 * This file must be kept in sync with xnu headers
 */

/*
 * bsd/sys/event.h
 */
__options_decl(kn_status_t, uint16_t /* 12 bits really */, {
	KN_ACTIVE         = 0x001,  /* event has been triggered */
	KN_QUEUED         = 0x002,  /* event is on queue */
	KN_DISABLED       = 0x004,  /* event is disabled */
	KN_DROPPING       = 0x008,  /* knote is being dropped */
	KN_LOCKED         = 0x010,  /* knote is locked (kq_knlocks) */
	KN_POSTING        = 0x020,  /* f_event() in flight */
	KN_STAYACTIVE     = 0x040,  /* force event to stay active */
	KN_DEFERDELETE    = 0x080,  /* defer delete until re-enabled */
	KN_MERGE_QOS      = 0x100,  /* f_event() / f_* ran concurrently and overrides must merge */
	KN_REQVANISH      = 0x200,  /* requested EV_VANISH */
	KN_VANISHED       = 0x400,  /* has vanished */
	KN_SUPPRESSED     = 0x800,  /* event is suppressed during delivery */
});

/*
 * bsd/sys/eventvar.h
 */
__options_decl(kq_state_t, uint16_t, {
	KQ_SEL            = 0x0001, /* select was recorded for kq */
	KQ_SLEEP          = 0x0002, /* thread is waiting for events */
	KQ_PROCWAIT       = 0x0004, /* thread waiting for processing */
	KQ_KEV32          = 0x0008, /* kq is used with 32-bit events */
	KQ_KEV64          = 0x0010, /* kq is used with 64-bit events */
	KQ_KEV_QOS        = 0x0020, /* kq events carry QoS info */
	KQ_WORKQ          = 0x0040, /* KQ is bound to process workq */
	KQ_WORKLOOP       = 0x0080, /* KQ is part of a workloop */
	KQ_PROCESSING     = 0x0100, /* KQ is being processed */
	KQ_DRAIN          = 0x0200, /* kq is draining */
	KQ_WAKEUP         = 0x0400, /* kq awakened while processing */
	KQ_DYNAMIC        = 0x0800, /* kqueue is dynamically managed */
	KQ_R2K_ARMED      = 0x1000, /* ast notification armed */
	KQ_HAS_TURNSTILE  = 0x2000, /* this kqueue has a turnstile */
});

/*
 * bsd/pthread/workqueue_internal.h
 */
__enum_decl(workq_tr_state_t, uint8_t, {
	WORKQ_TR_STATE_IDLE        = 0, /* request isn't in flight       */
	WORKQ_TR_STATE_NEW         = 1, /* request is being initiated    */
	WORKQ_TR_STATE_QUEUED      = 2, /* request is being queued       */
	WORKQ_TR_STATE_CANCELED    = 3, /* request is canceled           */
	WORKQ_TR_STATE_BINDING     = 4, /* request is preposted for bind */
	WORKQ_TR_STATE_BOUND       = 5, /* request is bound to a thread  */
});


/*
 * bsd/sys/signal.h
 */
static const char *
sig_strs[] = {
	[0]  = "<UNKN>",
	[1]  = "SIGHUP",
	[2]  = "SIGINT",
	[3]  = "SIGQUIT",
	[4]  = "SIGILL",
	[5]  = "SIGTRAP",
	[6]  = "SIGABRT",
	[7]  = "SIGEMT",
	[8]  = "SIGFPE",
	[9]  = "SIGKILL",
	[10] = "SIGBUS",
	[11] = "SIGSEGV",
	[12] = "SIGSYS",
	[13] = "SIGPIPE",
	[14] = "SIGALRM",
	[15] = "SIGTERM",
	[16] = "SIGURG",
	[17] = "SIGSTOP",
	[18] = "SIGTSTP",
	[19] = "SIGCONT",
	[20] = "SIGCHLD",
	[21] = "SIGTTIN",
	[22] = "SIGTTOU",
	[23] = "SIGIO",
	[24] = "SIGXCPU",
	[25] = "SIGXFSZ",
	[26] = "SIGVTALRM",
	[27] = "SIGPROF",
	[28] = "SIGWINCH",
	[29] = "SIGINFO",
	[30] = "SIGUSR1",
	[31] = "SIGUSR2"
};

/*
 * bsd/sys/event.h: EVFILT_*
 */
static const char *
filt_strs[] = {
	NULL,
	"READ",
	"WRITE",
	"AIO",
	"VNODE",
	"PROC",
	"SIGNAL",
	"TIMER",
	"MACHPORT",
	"FS",
	"USER",
	"<inval>",
	"VM",
	"SOCK",
	"MEMSTATUS",
	"EXCEPT",
	"CHANNEL",
	"WORKLOOP",
};

/*
 * bsd/sys/proc_info.h: PROX_FDTYPE_*
 */
static const char *
fdtype_strs[] = {
	"ATALK",
	"VNODE",
	"SOCKET",
	"PSHM",
	"PSEM",
	"KQUEUE",
	"PIPE",
	"FSEVENTS",
	"ATALK",
	"POLICY",
	"CHANNEL",
	"NEXUS",
};

#endif /* _LSKQ_COMMON_H_ */
