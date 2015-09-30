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
#include <mach/message.h>
#include <libproc.h>

#include "common.h"

#define ARRAYLEN(x) (sizeof((x))/sizeof((x[0])))

/* command line options */
static int verbose;
static int all_pids;
static int ignore_empty;

static char *self = "lskq";

static inline const char *
filt_name(int16_t filt)
{
	int idx = -filt;
	if (idx >= 0 && idx < ARRAYLEN(filt_strs)) {
		return filt_strs[idx];
	} else {
		return "<inval>";
	}
}

static inline const char *
fdtype_str(uint32_t type)
{
	if (type < ARRAYLEN(fdtype_strs)) {
		return fdtype_strs[type];
	} else {
		return "<unknown>";
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
		snprintf(str, len, "%c%c%c%c%c%c ",
			(ff & NOTE_EXIT)       ? 'x' : '-',
			(ff & NOTE_EXITSTATUS) ? 't' : '-',
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
			(ff & NOTE_SECONDS)    ? 's' :
			(ff & NOTE_USECONDS)   ? 'u' :
			(ff & NOTE_NSECONDS)   ? 'n' : '?',
			(ff & NOTE_ABSOLUTE)   ? 'a' : '-',
			(ff & NOTE_CRITICAL)   ? 'c' : '-',
			(ff & NOTE_BACKGROUND) ? 'b' : '-',
			(ff & NOTE_LEEWAY)     ? 'l' : '-'
		);
		break;
	}

	default:
		snprintf(str, len, "");
		break;
	};

	return str;
}


static inline int
filter_is_fd_type(int filter)
{
	if (filter <= EVFILT_READ && filter >= EVFILT_VNODE) {
		return 1;
	} else {
		return 0;
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
	switch (filter) {

	case EVFILT_SIGNAL:
	case EVFILT_PROC: {
		char str[128] = "";
		char num[128];
		char out[128];
		int numlen = sprintf(num, "%llu", ident);
		int strwidth = width - numlen - 3; // add room for brackets and space

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
				snprintf(str, strwidth + 1, "%s",
						shorten_procname(procname, strwidth));
			}
		}

		if (str[0] != '\0') {
			snprintf(out, width + 1, "(%s) %s", str, num);
		} else {
			snprintf(out, width + 1, "%s", num);
		}

		printf("%*s ", width, out);
		break;
	}

	default:
		printf("%*llu ", width, ident);
		break;
	}

}

static void
print_kqfd(int kqfd, int width)
{
	if (kqfd == -1) {
		printf("%*s ", width, "wq");
	} else {
		printf("%*u ", width, kqfd);
	}
}

static void
print_kq_info(int pid, const char *procname, int kqfd, int state)
{
	char tmpstr[16];
	strlcpy(tmpstr, shorten_procname(procname, 10), 11);
	printf("%-10s ", tmpstr);
	printf("%5u ", pid);
	print_kqfd(kqfd, 5);
	printf(" %c%c%c ",
			(state & KQ_SLEEP)    ? 'k' : '-',
			(state & KQ_SEL)      ? 's' : '-',
			(state & KQ_KEV32)    ? '3' :
			(state & KQ_KEV64)    ? '6' :
			(state & KQ_KEV_QOS)  ? 'q' : '-'
		  );
}

#define MAXENTRIES 2048

static int
process_kqueue_on_fd(int pid, const char *procname, int kqfd, struct proc_fdinfo *fdlist, int nfds)
{
	int ret, i, nknotes;
	char tmpstr[256];

	/*
	 * get the basic kqueue info
	 */
	struct kqueue_fdinfo kqfdinfo = {};
	ret = proc_pidfdinfo(pid, kqfd, PROC_PIDFDKQUEUEINFO, &kqfdinfo, sizeof(kqfdinfo));
	if (ret != sizeof(kqfdinfo) && kqfd != -1) {
		/* every proc has an implicit workq kqueue, dont warn if its unused */
		fprintf(stderr, "WARN: FD table changed (pid %i, kq %i)\n", pid, kqfd);
	}

	/*
	 * get extended kqueue info
	 */
	struct kevent_extinfo kqextinfo[MAXENTRIES];
 again:
	errno = 0;
	nknotes = proc_pidfdinfo(pid, kqfd, PROC_PIDFDKQUEUE_EXTINFO, kqextinfo, sizeof(kqextinfo));
	if (nknotes <= 0) {
		if (errno == 0) {
			/* proc_*() can't distinguish between error and empty list */
		} else if (errno == EAGAIN) {
			goto again;
		} else if (errno == EBADF) {
			fprintf(stderr, "WARN: FD table changed (pid %i, kq %i)\n", pid, kqfd);
			return 0;
		} else {
			perror("failed to get extended kqueue info");
			return errno;
		}
	}

	if (nknotes > MAXENTRIES) {
		fprintf(stderr, "WARN: truncated knote list (pid %i, kq %i)\n", pid, kqfd);
		nknotes = MAXENTRIES;
	}

	if (nknotes == 0) {
		if (!ignore_empty) {
			/* for empty kqueues, print a single empty entry */
			print_kq_info(pid, procname, kqfd, kqfdinfo.kqueueinfo.kq_state);
			printf("%20s \n", "-");
		}
		return 0;
	}

	for (i = 0; i < nknotes; i++) {
		struct kevent_extinfo *info = &kqextinfo[i];

		print_kq_info(pid, procname, kqfd, kqfdinfo.kqueueinfo.kq_state);
		print_ident(info->kqext_kev.ident, info->kqext_kev.filter, 20);
		printf("%-9s ", filt_name(info->kqext_kev.filter));

		/* for kevents attached to file descriptors, print the type of FD (file, socket, etc) */
		const char *fdstr = "";
		if (filter_is_fd_type(info->kqext_kev.filter)) {
			fdstr = "<UNKN>";
			int knfd = (info->kqext_kev.ident < nfds)
					? fd_list_getfd(fdlist, nfds, (int)info->kqext_kev.ident)
					: -1;
			if (knfd >= 0) {
				fdstr = fdtype_str(fdlist[knfd].proc_fdtype);
			}
		}
		printf("%-8s ", fdstr);

		/* print filter flags */
		printf("%7s ", fflags_build(info, tmpstr, sizeof(tmpstr)));

		/* print generic flags */
		unsigned flg = info->kqext_kev.flags;
		printf("%c%c%c%c%c%c%c%c%c ",
				(flg & EV_ADD)     ? 'a' : '-',
				(flg & EV_ENABLE)  ? 'n' : '-',
				(flg & EV_DISABLE) ? 'd' : '-',
				(flg & EV_DELETE)  ? 'x' : '-',
				(flg & EV_RECEIPT) ? 'r' : '-',
				(flg & EV_ONESHOT) ? '1' : '-',
				(flg & EV_CLEAR)   ? 'c' : '-',
				(flg & EV_EOF)     ? 'o' : '-',
				(flg & EV_ERROR)   ? 'e' : '-'
		);

		unsigned st = info->kqext_status;
		printf("%c%c%c%c ",
				(st & KN_ACTIVE)     ? 'a' : '-',
				(st & KN_QUEUED)     ? 'q' : '-',
				(st & KN_DISABLED)   ? 'd' : '-',
				(st & KN_STAYQUEUED) ? 's' : '-'
		);

		printf("%#18llx ", (unsigned long long)info->kqext_kev.data);

		if (verbose) {
			printf("%#18llx ", (unsigned long long)info->kqext_kev.udata);
			if (kqfdinfo.kqueueinfo.kq_state & (KQ_KEV64|KQ_KEV_QOS)) {
				printf("%#18llx ", (unsigned long long)info->kqext_kev.ext[0]);
				printf("%#18llx ", (unsigned long long)info->kqext_kev.ext[1]);
			}
			if (kqfdinfo.kqueueinfo.kq_state & KQ_KEV_QOS) {
				printf("%#18llx ", (unsigned long long)info->kqext_kev.ext[2]);
				printf("%#18llx ", (unsigned long long)info->kqext_kev.ext[3]);
				printf("%#10lx ", (unsigned long)info->kqext_kev.xflags);
			}
		}

		printf("\n");
	}

	return 0;
}

static int
process_pid(pid_t pid)
{
	int i, ret, nfds;

	/* enumerate file descriptors */
	struct proc_fdinfo fdlist[MAXENTRIES];
	nfds = proc_pidinfo(pid, PROC_PIDLISTFDS, 0, fdlist, sizeof(fdlist));
	if (nfds <= 0) {
		fprintf(stderr, "%s: failed enumerating file descriptors of process %i: %s",
				self, pid, strerror(errno));
		if (errno == EPERM && geteuid() != 0) {
			fprintf(stderr, " (are you root?)");
		}
		fprintf(stderr, "\n");
		return 1;
	}

	nfds /= sizeof(struct proc_fdinfo);
	if (nfds > MAXENTRIES) {
		fprintf(stderr, "WARN: truncated FD list (proc %i)\n", pid);
		nfds = MAXENTRIES;
	}

	/* get bsdinfo for the process name */
	struct proc_bsdinfo bsdinfo;
	ret = proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &bsdinfo, sizeof(bsdinfo));
	if (ret != sizeof(bsdinfo)) {
		perror("failed retrieving process info");
		return 1;
	}

	char *procname = bsdinfo.pbi_name;
	if (strlen(procname) == 0) {
		procname = bsdinfo.pbi_comm;
	}

	/* handle the special workq kq */
	ret = process_kqueue_on_fd(pid, procname, -1, fdlist, nfds);
	if (ret) {
		return ret;
	}

	for (i = 0; i < nfds; i++) {
		if (fdlist[i].proc_fdtype == PROX_FDTYPE_KQUEUE) {
			ret = process_kqueue_on_fd(pid, procname, fdlist[i].proc_fd, fdlist, nfds);
			if (ret) {
				return ret;
			}
		}
	}

	return 0;
}

#define MAXPIDS 4096

static int
process_all_pids(void)
{
	int i, npids, ret;
	int pids[MAXPIDS];

	npids = proc_listpids(PROC_ALL_PIDS, 0, pids, sizeof(pids));
	if (npids <= 0) {
		perror("failed enumerating pids");
		return 1;
	}
	npids /= sizeof(int);

	for (i = 0; i < npids; i++) {
		/* listpids gives us pid 0 for some reason */
		if (pids[i]) {
			ret = process_pid(pids[i]);
			if (ret) {
				return ret;
			}
		}
	}

	return 0;
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-vhe] [-a | -p <pid>]\n", self);
}

int main(int argc, char *argv[])
{
	pid_t pid = 0;
	int opt;

	if (argc > 0) {
		self = argv[0];
	}

	while ((opt = getopt(argc, argv, "eahvp:")) != -1) {
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
			return 0;
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

	printf("command      pid    kq kqst                ident filter    fdtype   fflags    flags   evst               data");
	if (verbose) {
		printf("              udata               ext0               ext1               ext2               ext3     xflags");
	}
	printf("\n");
	printf("---------- ----- ----- ---- -------------------- --------- -------- ------- --------- ---- ------------------");
	if (verbose) {
		printf(" ------------------ ------------------ ------------------ ------------------ ------------------ ----------");
	}
	printf("\n");

	if (all_pids) {
		return process_all_pids();
	} else {
		return process_pid(pid);
	}

	return 0;
}

