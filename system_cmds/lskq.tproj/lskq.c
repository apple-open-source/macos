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

	case EVFILT_USER: {
		snprintf(str, len, "%c%c%c    ",
			(ff & NOTE_TRIGGER) ? 't' : '-',
			(ff & NOTE_FFAND)   ? 'a' : '-',
			(ff & NOTE_FFOR)    ? 'o' : '-'
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

#define PROCNAME_WIDTH 20

static void
print_kq_info(int pid, const char *procname, int kqfd, int state)
{
	if (raw) {
		printf("%5u ", pid);
		print_kqfd(kqfd, 5);
		printf("%#10x ", state);
	} else {
		char tmpstr[PROCNAME_WIDTH+1];
		strlcpy(tmpstr, shorten_procname(procname, PROCNAME_WIDTH), PROCNAME_WIDTH+1);
		printf("%-*s ", PROCNAME_WIDTH, tmpstr);
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
}

static int
process_kqueue_on_fd(int pid, const char *procname, int kqfd, struct proc_fdinfo *fdlist, int nfds)
{
	int ret, i, nknotes;
	char tmpstr[256];
	int maxknotes = 256; /* arbitrary starting point */
	int err = 0;
	bool overflow = false;

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
	struct kevent_extinfo *kqextinfo = NULL;
 again:
	if (!kqextinfo) {
		kqextinfo = malloc(sizeof(struct kevent_extinfo) * maxknotes);
	}
	if (!kqextinfo) {
		perror("failed allocating memory");
		err = errno;
		goto out;
	}

	errno = 0;
	nknotes = proc_pidfdinfo(pid, kqfd, PROC_PIDFDKQUEUE_EXTINFO, kqextinfo,
			sizeof(struct kevent_extinfo) * maxknotes);
	if (nknotes <= 0) {
		if (errno == 0) {
			/* proc_*() can't distinguish between error and empty list */
		} else if (errno == EAGAIN) {
			goto again;
		} else if (errno == EBADF) {
			fprintf(stderr, "WARN: FD table changed (pid %i, kq %i)\n", pid, kqfd);
			goto out;
		} else {
			perror("failed to get extended kqueue info");
			err = errno;
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

	if (nknotes == 0) {
		if (!ignore_empty) {
			/* for empty kqueues, print a single empty entry */
			print_kq_info(pid, procname, kqfd, kqfdinfo.kqueueinfo.kq_state);
			printf("%18s \n", "-");
		}
		goto out;
	}

	for (i = 0; i < nknotes; i++) {
		struct kevent_extinfo *info = &kqextinfo[i];

		print_kq_info(pid, procname, kqfd, kqfdinfo.kqueueinfo.kq_state);
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
			printf("%c%c%c%c %c%c%c%c%c",
					(st & KN_ACTIVE)     ? 'a' : '-',
					(st & KN_QUEUED)     ? 'q' : '-',
					(st & KN_DISABLED)   ? 'd' : '-',
					(st & KN_STAYQUEUED) ? 's' : '-',

					(st & KN_DROPPING)   ? 'o' : '-',
					(st & KN_USEWAIT)    ? 'u' : '-',
					(st & KN_ATTACHING)  ? 'c' : '-',
					(st & KN_DEFERDROP)  ? 'f' : '-',
					(st & KN_TOUCH)      ? 't' : '-'
			);
		}

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

	if (overflow) {
		printf("   ***** output truncated (>=%i knotes on kq %i, proc %i) *****\n",
				nknotes, kqfd, pid);
	}

 out:
	if (kqextinfo) {
		free(kqextinfo);
		kqextinfo = NULL;
	}

	return err;
}

static int
process_pid(pid_t pid)
{
	int i, nfds;
	int ret = 0;
	int maxfds = 256; /* arbitrary starting point */
	struct proc_fdinfo *fdlist = NULL;

	/* enumerate file descriptors */
 again:
	if (!fdlist) {
		fdlist = malloc(sizeof(struct proc_fdinfo) * maxfds);
	}
	if (!fdlist) {
		perror("failed to allocate");
		ret = errno;
		goto out;
	}

	nfds = proc_pidinfo(pid, PROC_PIDLISTFDS, 0, fdlist,
			sizeof(struct proc_fdinfo) * maxfds);
	if (nfds <= 0) {
		fprintf(stderr, "%s: failed enumerating file descriptors of process %i: %s",
				self, pid, strerror(errno));
		if (errno == EPERM && geteuid() != 0) {
			fprintf(stderr, " (are you root?)");
		}
		fprintf(stderr, "\n");
		ret = errno;
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
	ret = process_kqueue_on_fd(pid, procname, -1, fdlist, nfds);
	if (ret) {
		goto out;
	}

	for (i = 0; i < nfds; i++) {
		if (fdlist[i].proc_fdtype == PROX_FDTYPE_KQUEUE) {
			ret = process_kqueue_on_fd(pid, procname, fdlist[i].proc_fd, fdlist, nfds);
			if (ret) {
				goto out;
			}
		}
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
			perror("failed enumerating pids");
			ret = errno;
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
			if (ret) {
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
	fprintf(stderr, "\nFilter-independent flags:\n\n\
\033[1mcommand                pid    kq kqst              ident filter    fdtype   fflags      flags          evst\033[0m\n\
\033[1m-------------------- ----- ----- ---- ------------------ --------- -------- ------- --------------- ----------\033[0m\n\
                                                                                              ┌ EV_UDATA_SPECIFIC\n\
                                                                                EV_DISPATCH ┐ │┌ EV_FLAG0 (EV_POLL)\n\
                                                                                  EV_CLEAR ┐│ ││┌ EV_FLAG1 (EV_OOBAND)\n\
                                                                               EV_ONESHOT ┐││ │││┌ EV_EOF\n\
                                                                              EV_RECEIPT ┐│││ ││││┌ EV_ERROR\n\
                                                                                         ││││ │││││\n\
\033[1mlaunchd                  1     4  ks- netbiosd       250 PROC               ------- andx r1cs upboe aqds oucft\033[0m \n\
                               │  │││                                               ││││            ││││ │││││\n\
        kqueue file descriptor ┘  │││                                        EV_ADD ┘│││  KN_ACTIVE ┘│││ ││││└ KN_TOUCH\n\
                         KQ_SLEEP ┘││                                      EV_ENABLE ┘││   KN_QUEUED ┘││ │││└ KN_DEFERDROP\n\
                            KQ_SEL ┘│                                      EV_DISABLE ┘│  KN_DISABLED ┘│ ││└ KN_ATTACHING\n\
                          KEV32 (3) ┤                                        EV_DELETE ┘ KN_STAYQUEUED ┘ │└ KN_USEWAIT\n\
                          KEV64 (6) ┤                                                                    └ KN_DROPPING\n\
                        KEV_QOS (q) ┘\n\
	\n");
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
		printf("  pid    kq       kqst              ident filter        fflags      flags       evst               data");
		if (verbose) {
			printf("              udata               ext0               ext1               ext2               ext3     xflags");
		}
		printf("\n");
		printf("----- ----- ---------- ------------------ --------- ---------- ---------- ---------- ------------------");

	} else {
		printf("command                pid    kq kqst              ident filter    fdtype   fflags       flags         evst                 data");
		if (verbose) {
			printf("              udata               ext0               ext1               ext2               ext3     xflags");
		}
		printf("\n");
		printf("-------------------- ----- ----- ---- ------------------ --------- -------- ------- --------------- ---------- -----------------");
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
