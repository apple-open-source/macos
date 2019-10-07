/*
 * Copyright (c) 2015-2016 Apple Inc. All rights reserved.
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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <assert.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/proc_info.h>
#include <sys/param.h>
#include <pthread/pthread.h>
#include <mach/message.h>
#define PRIVATE
#include <libproc.h>
#undef PRIVATE
#include <os/assumes.h>
#include <os/overflow.h>

#include "common.h"

#define ARRAYLEN(x) (sizeof((x))/sizeof((x[0])))

/* command line options */
static int verbose;
static int all_pids;
static int ignore_empty;
static int raw;

static char *self = "lskq";

static inline const char *
filt_name(int16_t filt)
{
	static char unkn_filt[32];
	int idx = -filt;
	if (idx >= 0 && idx < ARRAYLEN(filt_strs)) {
		return filt_strs[idx];
	} else {
		snprintf(unkn_filt, sizeof(unkn_filt), "%i (?)", idx);
		return unkn_filt;
	}
}

static inline const char *
fdtype_str(uint32_t type)
{
	static char unkn_fdtype[32];
	if (type < ARRAYLEN(fdtype_strs)) {
		return fdtype_strs[type];
	} else {
		snprintf(unkn_fdtype, sizeof(unkn_fdtype), "%i (?)", type);
		return unkn_fdtype;
	}
}

static char *
fflags_build(struct kevent_extinfo *info, char *str, int len)
{
	unsigned ff = info->kqext_sfflags;

	switch (info->kqext_kev.filter) {

	case EVFILT_READ: {
		snprintf(str, len, "%c      ",
			(ff & NOTE_LOWAT) ? 'l' : '-'
		);
		break;
	}

	case EVFILT_MACHPORT: {
		snprintf(str, len, "%c      ",
			(ff & MACH_RCV_MSG) ? 'r' : '-'
		);
		break;
	}

	case EVFILT_VNODE: {
		snprintf(str, len, "%c%c%c%c%c%c%c",
			(ff & NOTE_DELETE) ? 'd' : '-',
			(ff & NOTE_WRITE)  ? 'w' : '-',
			(ff & NOTE_EXTEND) ? 'e' : '-',
			(ff & NOTE_ATTRIB) ? 'a' : '-',
			(ff & NOTE_LINK)   ? 'l' : '-',
			(ff & NOTE_RENAME) ? 'r' : '-',
			(ff & NOTE_REVOKE) ? 'v' : '-'
		);
		break;
	}

	case EVFILT_PROC: {
/* NOTE_REAP is deprecated, but we still want to show if it's used */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
		snprintf(str, len, "%c%c%c%c%c%c%c",
			(ff & NOTE_EXIT)       ? 'x' : '-',
			(ff & NOTE_EXITSTATUS) ? 't' : '-',
			(ff & NOTE_EXIT_DETAIL)? 'd' : '-',
			(ff & NOTE_FORK)       ? 'f' : '-',
			(ff & NOTE_EXEC)       ? 'e' : '-',
			(ff & NOTE_SIGNAL)     ? 's' : '-',
			(ff & NOTE_REAP)       ? 'r' : '-'
		);
		break;
#pragma clang diagnostic pop
	}

	case EVFILT_TIMER: {
		snprintf(str, len, "%c%c%c%c%c  ",
			(ff & NOTE_SECONDS)              ? 's' :
			(ff & NOTE_USECONDS)             ? 'u' :
			(ff & NOTE_NSECONDS)             ? 'n' :
			(ff & NOTE_MACHTIME)             ? 'm' : '?',
			(ff & NOTE_ABSOLUTE)             ? 'a' :
			(ff & NOTE_MACH_CONTINUOUS_TIME) ? 'A' : '-',
			(ff & NOTE_CRITICAL)             ? 'c' : '-',
			(ff & NOTE_BACKGROUND)           ? 'b' : '-',
			(ff & NOTE_LEEWAY)               ? 'l' : '-'
		);
		break;
	}

	case EVFILT_USER:
		snprintf(str, len, "%c%c%c    ",
			(ff & NOTE_TRIGGER) ? 't' : '-',
			(ff & NOTE_FFAND)   ? 'a' : '-',
			(ff & NOTE_FFOR)    ? 'o' : '-'
		);
		break;

	case EVFILT_WORKLOOP:
		snprintf(str, len, "%c%c%c%c%c  ",
			(ff & NOTE_WL_THREAD_REQUEST) ? 't' :
			(ff & NOTE_WL_SYNC_WAIT)      ? 'w' :
			(ff & NOTE_WL_SYNC_IPC)       ? 'i' : '-',
			(ff & NOTE_WL_SYNC_WAKE)      ? 'W' : '-',
			(ff & NOTE_WL_UPDATE_QOS)     ? 'q' : '-',
			(ff & NOTE_WL_DISCOVER_OWNER) ? 'o' : '-',
			(ff & NOTE_WL_IGNORE_ESTALE)  ? 'e' : '-'
		);
		break;

	default:
		snprintf(str, len, "");
		break;
	};

	return str;
}


static inline int
filter_is_fd_type(int filter)
{
	switch (filter) {
	case EVFILT_VNODE ... EVFILT_READ:
	case EVFILT_SOCK:
	case EVFILT_NW_CHANNEL:
		return 1;
	default:
		return 0;
	}
}

static const char *
thread_qos_name(uint8_t th_qos)
{
	switch (th_qos) {
	case 0: return "--";
	case 1: return "MT";
	case 2: return "BG";
	case 3: return "UT";
	case 4: return "DF";
	case 5: return "IN";
	case 6: return "UI";
	case 7: return "MG";
	default: return "??";
	}
}

/*
 * find index of fd in a list of fdinfo of length nfds
 */
static inline int
fd_list_getfd(struct proc_fdinfo *fds, int nfds, int fd)
{
	int i;
	for (i = 0; i < nfds; i++) {
		if (fds[i].proc_fd == fd) {
			return i;
		}
	}

	return -1;
}

/*
 * left truncate URL-form process names
 */
static const char *
shorten_procname(const char *proc, int width)
{
	if (strcasestr(proc, "com.") == proc) {
		long len = strlen(proc);
		if (len > width) {
			return &proc[len - width];
		} else {
			return proc;
		}
	} else {
		return proc;
	}
}

/*
 * stringify knote ident where possible (signals, processes)
 */
static void
print_ident(uint64_t ident, int16_t filter, int width)
{
	if (raw) {
		printf("%#*llx ", width, ident);
		return;
	}

	switch (filter) {

	case EVFILT_SIGNAL:
	case EVFILT_PROC: {
		char str[128] = "";
		char num[128];
		char out[128];
		int numlen = sprintf(num, "%llu", ident);
		int strwidth = width - numlen - 1; // add room for a space

		if (filter == EVFILT_SIGNAL) {
			if (ident < ARRAYLEN(sig_strs)) {
				snprintf(str, strwidth + 1, "%s", sig_strs[ident]);
			}
		} else {
			/* FIXME: this should be cached */
			struct proc_bsdinfo bsdinfo;
			int ret = proc_pidinfo((int)ident, PROC_PIDTBSDINFO, 0, &bsdinfo, sizeof(bsdinfo));
			if (ret == sizeof(bsdinfo)) {
				char *procname = bsdinfo.pbi_name;
				if (strlen(procname) == 0) {
					procname = bsdinfo.pbi_comm;
				}
				snprintf(str, strwidth + 1, "%s", shorten_procname(procname, strwidth));
			}
		}

		if (str[0] != '\0') {
			snprintf(out, width + 1, "%-*s %s", strwidth, str, num);
		} else {
			snprintf(out, width + 1, "%s", num);
		}

		printf("%*s ", width, out);
		break;
	}

	case EVFILT_MACHPORT:
	case EVFILT_TIMER:
		/* hex, to match lsmp */
		printf("%#*llx ", width, ident);
		break;

	case EVFILT_WORKLOOP:
		printf("%#*llx ", width, ident);
		break;

	default:
		printf("%*llu ", width, ident);
		break;
	}

}

static void
print_kqid(int state, uint64_t kqid)
{
	if (state & KQ_WORKQ) {
		printf("%18s ", "wq");
	} else if (state & KQ_WORKLOOP) {
		printf("%#18" PRIx64 " ", kqid);
	} else {
		printf("fd %15" PRIi64 " ", kqid);
	}
}

#define PROCNAME_WIDTH 20

static void
print_kq_info(int pid, const char *procname, uint64_t kqid, int state)
{
	if (raw) {
		printf("%5u ", pid);
		print_kqid(state, kqid);
		printf("%#10x ", state);
	} else {
		char tmpstr[PROCNAME_WIDTH+1];
		strlcpy(tmpstr, shorten_procname(procname, PROCNAME_WIDTH), PROCNAME_WIDTH+1);
		printf("%-*s ", PROCNAME_WIDTH, tmpstr);
		printf("%5u ", pid);
		print_kqid(state, kqid);
		printf(" %c%c%c ",
				(state & KQ_SLEEP)    ? 'k' : '-',
				(state & KQ_SEL)      ? 's' : '-',
				(state & KQ_WORKQ)    ? 'q' :
				(state & KQ_WORKLOOP) ? 'l' : '-'
			);
	}
}

enum kqtype {
	KQTYPE_FD,
	KQTYPE_DYNAMIC
};

#define POLICY_TIMESHARE        1
#define POLICY_RR               2
#define POLICY_FIFO             4

static int
process_kqueue(int pid, const char *procname, enum kqtype type, uint64_t kqid,
		struct proc_fdinfo *fdlist, int nfds)
{
	int ret, i, nknotes;
	char tmpstr[256];
	int maxknotes = 256; /* arbitrary starting point */
	int kq_state;
	bool is_kev_64, is_kev_qos;
	int err = 0;
	bool overflow = false;
	int fd;
	bool dynkq_printed = false;

	/*
	 * get the basic kqueue info
	 */
	struct kqueue_fdinfo kqfdinfo = {};
	struct kqueue_dyninfo kqinfo = {};
	switch (type) {
	case KQTYPE_FD:
		ret = proc_pidfdinfo(pid, (int)kqid, PROC_PIDFDKQUEUEINFO, &kqfdinfo, sizeof(kqfdinfo));
		fd = (int)kqid;
		break;
	case KQTYPE_DYNAMIC:
		ret = proc_piddynkqueueinfo(pid, PROC_PIDDYNKQUEUE_INFO, kqid, &kqinfo, sizeof(kqinfo));
		break;
	default:
		os_crash("invalid kqueue type");
	}

	if (type == KQTYPE_FD && (int)kqid != -1) {
		if (ret != sizeof(kqfdinfo)) {
		/* every proc has an implicit workq kqueue, dont warn if its unused */
			fprintf(stderr, "WARN: FD table changed (pid %i, kq %i)\n", pid,
					fd);
		}
	} else if (type == KQTYPE_DYNAMIC) {
		if (ret < sizeof(struct kqueue_info)) {
			fprintf(stderr, "WARN: kqueue missing (pid %i, kq %#" PRIx64 ")\n",
					pid, kqid);
		} else {
			kqfdinfo.kqueueinfo = kqinfo.kqdi_info;
		}
		if (verbose && ret >= sizeof(struct kqueue_dyninfo)) {
			print_kq_info(pid, procname, kqid, kqinfo.kqdi_info.kq_state);

			if (kqinfo.kqdi_owner) {
				printf("%#18llx ", kqinfo.kqdi_owner);    // ident
				printf("%-9s ", "WL owned"); // filter
			} else if (kqinfo.kqdi_servicer) {
				printf("%#18llx ", kqinfo.kqdi_servicer); // ident
				printf("%-9s ", "WL"); // filter
			} else {
				printf("%18s ", "-"); // ident
				printf("%-9s ", "WL"); // filter
			}
			dynkq_printed = true;

			if (raw) {
				printf("%-10s ", " "); // fflags
				printf("%-10s ", " "); // flags
				printf("%-10s ", " "); // evst
			} else {
				const char *reqstate = "???";

				switch (kqinfo.kqdi_request_state) {
				case WORKQ_TR_STATE_IDLE:
					reqstate = "";
					break;
				case WORKQ_TR_STATE_NEW:
					reqstate = "new";
					break;
				case WORKQ_TR_STATE_QUEUED:
					reqstate = "queued";
					break;
				case WORKQ_TR_STATE_CANCELED:
					reqstate = "canceled";
					break;
				case WORKQ_TR_STATE_BINDING:
					reqstate = "binding";
					break;
				case WORKQ_TR_STATE_BOUND:
					reqstate = "bound";
					break;
				}

				printf("%-8s ", reqstate); // fdtype
				char policy_type;
				switch (kqinfo.kqdi_pol) {
				case POLICY_RR:
					policy_type = 'R';
					break;
				case POLICY_FIFO:
					policy_type = 'F';
				case POLICY_TIMESHARE:
				case 0:
				default:
					policy_type = '-';
					break;
				}
				snprintf(tmpstr, 4, "%c%c%c", (kqinfo.kqdi_pri == 0)?'-':'P', policy_type, (kqinfo.kqdi_cpupercent == 0)?'-':'%');
				printf("%-7s ", tmpstr); // fflags
				printf("%-15s ", " "); // flags
				printf("%-15s ", " "); // evst
			}

			if (!raw && kqinfo.kqdi_pri != 0) {
				printf("%3d ", kqinfo.kqdi_pri); //qos
			} else {
				int qos = MAX(MAX(kqinfo.kqdi_events_qos, kqinfo.kqdi_async_qos),
					kqinfo.kqdi_sync_waiter_qos);
				printf("%3s ", thread_qos_name(qos)); //qos
			}
			printf("\n");
		}
	}

	/*
	 * get extended kqueue info
	 */
	struct kevent_extinfo *kqextinfo = NULL;
 again:
	if (!kqextinfo) {
		kqextinfo = malloc(sizeof(struct kevent_extinfo) * maxknotes);
	}
	if (!kqextinfo) {
		err = errno;
		perror("failed allocating memory");
		goto out;
	}

	errno = 0;
	switch (type) {
	case KQTYPE_FD:
		nknotes = proc_pidfdinfo(pid, fd, PROC_PIDFDKQUEUE_EXTINFO,
				kqextinfo, sizeof(struct kevent_extinfo) * maxknotes);
		break;
	case KQTYPE_DYNAMIC:
		nknotes = proc_piddynkqueueinfo(pid, PROC_PIDDYNKQUEUE_EXTINFO, kqid,
				kqextinfo, sizeof(struct kevent_extinfo) * maxknotes);
		break;
	default:
		os_crash("invalid kqueue type");
	}

	if (nknotes <= 0) {
		if (errno == 0) {
			/* proc_*() can't distinguish between error and empty list */
		} else if (errno == EAGAIN) {
			goto again;
		} else if (errno == EBADF) {
			fprintf(stderr, "WARN: FD table changed (pid %i, kq %#" PRIx64 ")\n", pid, kqid);
			goto out;
		} else {
			err = errno;
			perror("failed to get extended kqueue info");
			goto out;
		}
	}

	if (nknotes > maxknotes) {
		maxknotes = nknotes + 16; /* arbitrary safety margin */
		free(kqextinfo);
		kqextinfo = NULL;
		goto again;
	}

	if (nknotes >= PROC_PIDFDKQUEUE_KNOTES_MAX) {
		overflow = true;
	}

	kq_state = kqfdinfo.kqueueinfo.kq_state;
	is_kev_64 = (kq_state & PROC_KQUEUE_64);
	is_kev_qos = (kq_state & PROC_KQUEUE_QOS);

	if (nknotes == 0) {
		if (!ignore_empty && !dynkq_printed) {
			/* for empty kqueues, print a single empty entry */
			print_kq_info(pid, procname, kqid, kq_state);
			printf("%18s \n", "-");
		}
		goto out;
	}

	for (i = 0; i < nknotes; i++) {
		struct kevent_extinfo *info = &kqextinfo[i];

		print_kq_info(pid, procname, kqid, kqfdinfo.kqueueinfo.kq_state);
		print_ident(info->kqext_kev.ident, info->kqext_kev.filter, 18);
		printf("%-9s ", filt_name(info->kqext_kev.filter));

		if (raw) {
			printf("%#10x ", info->kqext_sfflags);
			printf("%#10x ", info->kqext_kev.flags);
			printf("%#10x ", info->kqext_status);
		} else {
			/* for kevents attached to file descriptors, print the type of FD (file, socket, etc) */
			const char *fdstr = "";
			if (filter_is_fd_type(info->kqext_kev.filter)) {
				fdstr = "<unkn>";
				int knfd = fd_list_getfd(fdlist, nfds, (int)info->kqext_kev.ident);
				if (knfd >= 0) {
					fdstr = fdtype_str(fdlist[knfd].proc_fdtype);
				}
			}
			printf("%-8s ", fdstr);

			/* print filter flags */
			printf("%7s ", fflags_build(info, tmpstr, sizeof(tmpstr)));

			/* print generic flags */
			unsigned flg = info->kqext_kev.flags;
			printf("%c%c%c%c %c%c%c%c %c%c%c%c%c ",
					(flg & EV_ADD)      ? 'a' : '-',
					(flg & EV_ENABLE)   ? 'n' : '-',
					(flg & EV_DISABLE)  ? 'd' : '-',
					(flg & EV_DELETE)   ? 'x' : '-',

					(flg & EV_RECEIPT)  ? 'r' : '-',
					(flg & EV_ONESHOT)  ? '1' : '-',
					(flg & EV_CLEAR)    ? 'c' : '-',
					(flg & EV_DISPATCH) ? 's' : '-',

					(flg & EV_UDATA_SPECIFIC) ? 'u' : '-',
					(flg & EV_FLAG0)    ? 'p' : '-',
					(flg & EV_FLAG1)    ? 'b' : '-',
					(flg & EV_EOF)      ? 'o' : '-',
					(flg & EV_ERROR)    ? 'e' : '-'
			);

			unsigned st = info->kqext_status;
			printf("%c%c%c%c%c %c%c%c%c %c%c%c ",
					(st & KN_ACTIVE)      ? 'a' : '-',
					(st & KN_QUEUED)      ? 'q' : '-',
					(st & KN_DISABLED)    ? 'd' : '-',
					(st & KN_SUPPRESSED)  ? 'p' : '-',
					(st & KN_STAYACTIVE)  ? 's' : '-',

					(st & KN_DROPPING)    ? 'd' : '-',
					(st & KN_LOCKED)      ? 'l' : '-',
					(st & KN_POSTING)     ? 'P' : '-',
					(st & KN_MERGE_QOS)   ? 'm' : '-',

					(st & KN_DEFERDELETE) ? 'D' : '-',
					(st & KN_REQVANISH)   ? 'v' : '-',
					(st & KN_VANISHED)    ? 'n' : '-'
			);
		}

		printf("%3s ", thread_qos_name(info->kqext_kev.qos));

		printf("%#18llx ", (unsigned long long)info->kqext_kev.data);

		if (verbose) {
			printf("%#18llx ", (unsigned long long)info->kqext_kev.udata);
			if (is_kev_qos || is_kev_64) {
				printf("%#18llx ", (unsigned long long)info->kqext_kev.ext[0]);
				printf("%#18llx ", (unsigned long long)info->kqext_kev.ext[1]);

				if (is_kev_qos) {
					printf("%#18llx ", (unsigned long long)info->kqext_kev.ext[2]);
					printf("%#18llx ", (unsigned long long)info->kqext_kev.ext[3]);
					printf("%#10lx ", (unsigned long)info->kqext_kev.xflags);
				}
			}
		}

		printf("\n");
	}

	if (overflow) {
		printf("   ***** output truncated (>=%i knotes on kq %" PRIu64 ", proc %i) *****\n",
				nknotes, kqid, pid);
	}

 out:
	if (kqextinfo) {
		free(kqextinfo);
		kqextinfo = NULL;
	}

	return err;
}

static int
pid_kqids(pid_t pid, kqueue_id_t **kqids_out)
{
	static int kqids_len = 256;
	static kqueue_id_t *kqids = NULL;
	static uint32_t kqids_size;

	int nkqids;

retry:
	if (os_mul_overflow(sizeof(kqueue_id_t), kqids_len, &kqids_size)) {
		assert(kqids_len > PROC_PIDDYNKQUEUES_MAX);
		kqids_len = PROC_PIDDYNKQUEUES_MAX;
		goto retry;
	}
	if (!kqids) {
		kqids = malloc(kqids_size);
		os_assert(kqids != NULL);
	}

	nkqids = proc_list_dynkqueueids(pid, kqids, kqids_size);
	if (nkqids > kqids_len && kqids_len < PROC_PIDDYNKQUEUES_MAX) {
		kqids_len *= 2;
		if (kqids_len > PROC_PIDDYNKQUEUES_MAX) {
			kqids_len = PROC_PIDDYNKQUEUES_MAX;
		}
		free(kqids);
		kqids = NULL;
		goto retry;
	}

	*kqids_out = kqids;
	return MIN(nkqids, kqids_len);
}

static int
process_pid(pid_t pid)
{
	int i, nfds, nkqids;
	kqueue_id_t *kqids;
	int ret = 0;
	int maxfds = 256; /* arbitrary starting point */
	struct proc_fdinfo *fdlist = NULL;

	/* enumerate file descriptors */
 again:
	if (!fdlist) {
		fdlist = malloc(sizeof(struct proc_fdinfo) * maxfds);
	}
	if (!fdlist) {
		ret = errno;
		perror("failed to allocate");
		goto out;
	}

	nfds = proc_pidinfo(pid, PROC_PIDLISTFDS, 0, fdlist,
			sizeof(struct proc_fdinfo) * maxfds);
	if (nfds <= 0) {
		ret = errno;
		fprintf(stderr, "%s: failed enumerating file descriptors of process %i: %s",
				self, pid, strerror(ret));
		if (ret == EPERM && geteuid() != 0) {
			fprintf(stderr, " (are you root?)");
		}
		fprintf(stderr, "\n");
		goto out;
	}

	nfds /= sizeof(struct proc_fdinfo);
	if (nfds >= maxfds) {
		maxfds = nfds + 16;
		free(fdlist);
		fdlist = NULL;
		goto again;
	}

	/* get bsdinfo for the process name */
	struct proc_bsdinfo bsdinfo;
	ret = proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &bsdinfo, sizeof(bsdinfo));
	if (ret != sizeof(bsdinfo)) {
		perror("failed retrieving process info");
		ret = -1;
		goto out;
	}

	char *procname = bsdinfo.pbi_name;
	if (strlen(procname) == 0) {
		procname = bsdinfo.pbi_comm;
	}

	/* handle the special workq kq */
	ret = process_kqueue(pid, procname, KQTYPE_FD, -1, fdlist, nfds);
	if (ret) {
		goto out;
	}

	for (i = 0; i < nfds; i++) {
		if (fdlist[i].proc_fdtype == PROX_FDTYPE_KQUEUE) {
			ret = process_kqueue(pid, procname, KQTYPE_FD,
					(uint64_t)fdlist[i].proc_fd, fdlist, nfds);
			if (ret) {
				goto out;
			}
		}
	}

	nkqids = pid_kqids(pid, &kqids);

	for (i = 0; i < nkqids; i++) {
		ret = process_kqueue(pid, procname, KQTYPE_DYNAMIC, kqids[i], fdlist, nfds);
		if (ret) {
			goto out;
		}
	}

	if (nkqids >= PROC_PIDDYNKQUEUES_MAX) {
		printf("   ***** output truncated (>=%i dynamic kqueues in proc %i) *****\n",
				nkqids, pid);
	}

 out:
	if (fdlist) {
		free(fdlist);
		fdlist = NULL;
	}

	return ret;
}

static int
process_all_pids(void)
{
	int i, npids;
	int ret = 0;
	int maxpids = 2048;
	int *pids = NULL;

 again:
	if (!pids) {
		pids = malloc(sizeof(int) * maxpids);
	}
	if (!pids) {
		perror("failed allocating pids[]");
		goto out;
	}

	errno = 0;
	npids = proc_listpids(PROC_ALL_PIDS, 0, pids, sizeof(int) * maxpids);
	if (npids <= 0) {
		if (errno == 0) {
			/* empty pid list */
		} else if (errno == EAGAIN) {
			goto again;
		} else {
			ret = errno;
			perror("failed enumerating pids");
			goto out;
		}
	}

	npids /= sizeof(int);
	if (npids >= maxpids) {
		maxpids = npids + 16;
		free(pids);
		pids = NULL;
		goto again;
	}

	for (i = 0; i < npids; i++) {
		/* listpids gives us pid 0 for some reason */
		if (pids[i]) {
			ret = process_pid(pids[i]);
			/* ignore races with processes exiting */
			if (ret && ret != ESRCH) {
				goto out;
			}
		}
	}

out:
	if (pids) {
		free(pids);
		pids = NULL;
	}

	return ret;
}

static void
cheatsheet(void)
{
	const char *bold = "\033[1m";
	const char *reset = "\033[0m";
	if (!isatty(STDERR_FILENO)) {
		bold = reset = "";
	}

	fprintf(stderr, "\nFilter-independent flags:\n\n\
%s\
command                pid                 kq kqst               knid filter    fdtype   fflags       flags           evst      qos%s\n%s\
-------------------- ----- ------------------ ---- ------------------ --------- -------- ------- --------------- -------------- ---%s\n\
                                                                                                           ┌ EV_UDATA_SPECIFIC\n\
                                                                                             EV_DISPATCH ┐ │┌ EV_FLAG0 (EV_POLL)\n\
                                                                                               EV_CLEAR ┐│ ││┌ EV_FLAG1 (EV_OOBAND)\n\
                                                                                            EV_ONESHOT ┐││ │││┌ EV_EOF\n\
                                                                                           EV_RECEIPT ┐│││ ││││┌ EV_ERROR\n\
                                                                                                      ││││ │││││\n%s\
launchd                  1                  4  ks- netbiosd       250 PROC               ------- andx r1cs upboe aqdps dlPm Dvn  IN%s\n\
                                            │  │││                                               ││││            │││││ ││││ │││\n\
          kqueue file descriptor/dynamic ID ┘  │││                                        EV_ADD ┘│││  KN_ACTIVE ┘││││ ││││ ││└ KN_VANISHED\n\
                                      KQ_SLEEP ┘││                                      EV_ENABLE ┘││   KN_QUEUED ┘│││ ││││ │└ KN_REQVANISH\n\
                                         KQ_SEL ┘│                                      EV_DISABLE ┘│  KN_DISABLED ┘││ ││││ └ KN_DEFERDELETE\n\
                                    KQ_WORKQ (q) ┤                                        EV_DELETE ┘ KN_SUPPRESSED ┘│ ││││\n\
                                 KQ_WORKLOOP (l) ┘                                                     KN_STAYACTIVE ┘ ││││\n\
                                                                                                                       ││││\n\
                                                                                                           KN_DROPPING ┘││└ KN_MERGE_QOS\n\
                                                                                                              KN_LOCKED ┘└ KN_POSTING\n\
	\n", bold, reset, bold, reset, bold, reset);
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-vher] [-a | -p <pid>]\n", self);
}

static void
print_header(void)
{
	if (raw) {
		printf("  pid                 kq       kqst               knid filter        fflags      flags       evst qos               data");
	} else {
		printf("command                pid                 kq kqst               knid filter    fdtype   fflags       flags           evst      qos               data");
	}

	if (verbose) {
		printf("              udata               ext0               ext1               ext2               ext3     xflags");
	}

	printf("\n");

	if (raw) {
		printf("----- ------------------ ---------- ------------------ --------- ---------- ---------- ---------- --- ------------------");
	} else {
		printf("-------------------- ----- ------------------ ---- ------------------ --------- -------- ------- --------------- -------------- --- ------------------");
	}

	if (verbose) {
		printf(" ------------------ ------------------ ------------------ ------------------ ------------------ ----------");
	}
	printf("\n");
}

int
main(int argc, char *argv[])
{
	pid_t pid = 0;
	int opt;

	setlinebuf(stdout);

	if (argc > 0) {
		self = argv[0];
	}

	while ((opt = getopt(argc, argv, "eahvrp:")) != -1) {
		switch (opt) {
		case 'a':
			all_pids = 1;
			break;
		case 'v':
			verbose++;
			break;
		case 'p':
			pid = atoi(optarg);
			break;
		case 'e':
			ignore_empty = 1;
			break;
		case 'h':
			usage();
			cheatsheet();
			return 0;
		case 'r':
			raw = 1;
			break;
		case '?':
		default:
			usage();
			return 1;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 1) {
		/* also allow lskq <pid> */
		if (pid || all_pids) {
			usage();
			return 1;
		}

		pid = atoi(argv[0]);
	} else if (argc > 1) {
		usage();
		return 1;
	}

	/* exactly one of -p or -a is required */
	if (!pid && !all_pids) {
		usage();
		return 1;
	} else if (pid && all_pids) {
		usage();
		return 1;
	}

	print_header();

	if (all_pids) {
		return process_all_pids();
	} else {
		return process_pid(pid);
	}

	return 0;
}
