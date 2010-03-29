/*
 * Copyright (c) 2004-2009 Apple Inc. All rights reserved.
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ucred.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define SYSLOG_NAMES
#include <syslog.h>
#include <sys/fslog.h>
#include <vproc.h>
#include <pthread.h>
#include <vproc_priv.h>
#include <mach/mach.h>
#include <assert.h>
#include <libkern/OSAtomic.h>
#include "daemon.h"

#define LIST_SIZE_DELTA 256

#define streq(A,B) (strcmp(A,B)==0)
#define forever for(;;)
#define IndexNull ((uint32_t)-1)

#define ASL_MSG_TYPE_MASK 0x0000000f
#define ASL_TYPE_ERROR 2

#define ASL_KEY_FACILITY "Facility"

#define FACILITY_USER "user"
#define FACILITY_CONSOLE "com.apple.console"
#define SYSTEM_RESERVED "com.apple.system"
#define SYSTEM_RESERVED_LEN 16

#define VERIFY_STATUS_OK 0
#define VERIFY_STATUS_INVALID_MESSAGE 1
#define VERIFY_STATUS_EXCEEDED_QUOTA 2

extern void disaster_message(asl_msg_t *m);
static char myname[MAXHOSTNAMELEN + 1] = {0};

static OSSpinLock count_lock = 0;

#ifndef CONFIG_IPHONE
static vproc_transaction_t vproc_trans = {0};
#endif

#define QUOTA_TABLE_SIZE 8192
#define QUOTA_TABLE_SLOTS 8

#define QUOTA_EXCEEDED_MESSAGE "*** process %d exceeded %d log message per second limit  -  remaining messages this second discarded ***"
#define QUOTA_EXCEEDED_LEVEL "3"

static time_t quota_table_time = 0;
static pid_t quota_table_pid[QUOTA_TABLE_SIZE];
static int32_t quota_table_quota[QUOTA_TABLE_SIZE];

static const char *kern_notify_key[] = 
{
	"com.apple.system.log.kernel.emergency",
	"com.apple.system.log.kernel.alert",
	"com.apple.system.log.kernel.critical",
	"com.apple.system.log.kernel.error",
	"com.apple.system.log.kernel.warning",
	"com.apple.system.log.kernel.notice",
	"com.apple.system.log.kernel.info",
	"com.apple.system.log.kernel.debug"
};

struct asloutput
{
	aslsendmsgfn sendmsg;
	const char *outid;
	TAILQ_ENTRY(asloutput) entries;
};

struct aslmatch
{
	char *outid;
	asl_msg_t *query;
	TAILQ_ENTRY(aslmatch) entries;
};

TAILQ_HEAD(ae, aslevent) Eventq;
TAILQ_HEAD(ao, asloutput) Outq;
TAILQ_HEAD(am, aslmatch) Matchq;

static char **
_insertString(char *s, char **l, uint32_t x)
{
	int i, len;

	if (s == NULL) return l;
	if (l == NULL) 
	{
		l = (char **)malloc(2 * sizeof(char *));
		if (l == NULL) return NULL;

		l[0] = strdup(s);
		if (l[0] == NULL)
		{
			free(l);
			return NULL;
		}

		l[1] = NULL;
		return l;
	}

	for (i = 0; l[i] != NULL; i++);
	len = i + 1; /* count the NULL on the end of the list too! */

	l = (char **)reallocf(l, (len + 1) * sizeof(char *));
	if (l == NULL) return NULL;

	if ((x >= (len - 1)) || (x == IndexNull))
	{
		l[len - 1] = strdup(s);
		if (l[len - 1] == NULL)
		{
			free(l);
			return NULL;
		}

		l[len] = NULL;
		return l;
	}

	for (i = len; i > x; i--) l[i] = l[i - 1];
	l[x] = strdup(s);
	if (l[x] == NULL) return NULL;

	return l;
}

char **
explode(const char *s, const char *delim)
{
	char **l = NULL;
	const char *p;
	char *t, quote;
	int i, n;

	if (s == NULL) return NULL;

	quote = '\0';

	p = s;
	while (p[0] != '\0')
	{
		/* scan forward */
		for (i = 0; p[i] != '\0'; i++)
		{
			if (quote == '\0')
			{
				/* not inside a quoted string: check for delimiters and quotes */
				if (strchr(delim, p[i]) != NULL) break;
				else if (p[i] == '\'') quote = p[i];
				else if (p[i] == '"') quote = p[i];
			}
			else
			{
				/* inside a quoted string - look for matching quote */
				if (p[i] == quote) quote = '\0';
			}
		}

		n = i;
		t = malloc(n + 1);
		if (t == NULL) return NULL;

		for (i = 0; i < n; i++) t[i] = p[i];
		t[n] = '\0';
		l = _insertString(t, l, IndexNull);
		free(t);
		t = NULL;
		if (p[i] == '\0') return l;
		if (p[i + 1] == '\0') l = _insertString("", l, IndexNull);
		p = p + i + 1;
	}

	return l;
}

void
freeList(char **l)
{
	int i;

	if (l == NULL) return;
	for (i = 0; l[i] != NULL; i++) free(l[i]);
	free(l);
}

/*
 * Quotas are maintained using a very fast fixed-size table.
 * We hash into the pid table (quota_table_pid) using the last 10
 * bits of the pid, so the table has 1024 "buckets".  The table is
 * actually just an array with 8 entry slots (for collisions) per bucket.
 * If there are more than 8 pids that hash to the same bucket, we
 * re-use the one with the lowest message usage (highest remaining
 * quota).  This can lead to "generosity: if there are nine or more
 * pids with the same last 10 bits all logging like crazy, we may
 * end up allowing some of them to log more than their quota. 
 * That would be a remarkably rare occurrence.
 */
 
static uint32_t
quota_check(pid_t pid, time_t now, asl_msg_t *msg)
{
	int i, x, maxx, max;
	char *str;

	if (msg == NULL) return VERIFY_STATUS_INVALID_MESSAGE;
	if (global.mps_limit == 0) return VERIFY_STATUS_OK;

	OSSpinLockLock(&global.lock);

	if (quota_table_time != now)
	{
		memset(quota_table_pid, 0, sizeof(quota_table_pid));
		quota_table_time = now;
	}

	/* hash is last 10 bits of the pid, shifted up 3 bits to allow 8 slots per bucket */
	x = (pid & 0x000003ff) << 3;
	maxx = x;
	max = quota_table_quota[x];

	for (i = 0; i < QUOTA_TABLE_SLOTS; i++)
	{
		if (quota_table_pid[x] == 0)
		{
			quota_table_pid[x] = pid;
			quota_table_quota[x] = global.mps_limit;

			OSSpinLockUnlock(&global.lock);
			return VERIFY_STATUS_OK;
		}

		if (quota_table_pid[x] == pid)
		{
			quota_table_quota[x] = quota_table_quota[x] - 1;

			if (quota_table_quota[x] == 0)
			{
				quota_table_quota[x] = -1;

				str = NULL;
				asprintf(&str, QUOTA_EXCEEDED_MESSAGE, (int)pid, global.mps_limit);
				if (str != NULL)
				{
					asl_set(msg, ASL_KEY_MSG, str);
					free(str);
					asl_set(msg, ASL_KEY_LEVEL, QUOTA_EXCEEDED_LEVEL);
				}

				OSSpinLockUnlock(&global.lock);
				return VERIFY_STATUS_OK;
			}

			if (quota_table_quota[x] < 0)
			{
				OSSpinLockUnlock(&global.lock);
				return VERIFY_STATUS_EXCEEDED_QUOTA;
			}

			OSSpinLockUnlock(&global.lock);
			return VERIFY_STATUS_OK;
		}

		if (quota_table_quota[x] > max)
		{
			maxx = x;
			max = quota_table_quota[x];
		}

		x += 1;
	}

	/* can't find the pid and no slots were available - reuse slot with highest remaining quota */
	quota_table_pid[maxx] = pid;
	quota_table_quota[maxx] = global.mps_limit;

	OSSpinLockUnlock(&global.lock);
	return VERIFY_STATUS_OK;
}

int
asl_check_option(asl_msg_t *msg, const char *opt)
{
	const char *p;
	uint32_t len;

	if (msg == NULL) return 0;
	if (opt == NULL) return 0;

	len = strlen(opt);
	if (len == 0) return 0;

	p = asl_get(msg, ASL_KEY_OPTION);
	if (p == NULL) return 0;

	while (*p != '\0')
	{
		while ((*p == ' ') || (*p == '\t') || (*p == ',')) p++;
		if (*p == '\0') return 0;

		if (strncasecmp(p, opt, len) == 0)
		{
			p += len;
			if ((*p == ' ') || (*p == '\t') || (*p == ',') || (*p == '\0')) return 1;
		}

		while ((*p != ' ') && (*p != '\t') && (*p != ',') && (*p != '\0')) p++;
	}

	return 0;
}

int
aslevent_init(void)
{
	TAILQ_INIT(&Eventq);
	TAILQ_INIT(&Outq);
	TAILQ_INIT(&Matchq);

	return 0;
}

int
aslevent_log(asl_msg_t *msg, char *outid)
{
	struct asloutput *i;
	int status = -1;

	for (i = Outq.tqh_first; i != NULL; i = i->entries.tqe_next)
	{
		if ((outid != NULL) && (strcmp(i->outid, outid) == 0))
		{
			status = i->sendmsg(msg, outid);
		}
	}

	return status;
}

int
aslevent_addmatch(asl_msg_t *query, char *outid)
{
	struct aslmatch *tmp;

	if (query == NULL) return -1;
	if (outid == NULL) return -1;

	tmp = calloc(1, sizeof(struct aslmatch));
	if (tmp == NULL) return -1;

	tmp->query = query;
	tmp->outid = outid;
	TAILQ_INSERT_TAIL(&Matchq, tmp, entries);

	return 0;
}

void
asl_message_match_and_log(asl_msg_t *msg)
{
	struct aslmatch *i;

	if (msg == NULL) return;

	for (i = Matchq.tqh_first; i != NULL; i = i->entries.tqe_next)
	{
		if (asl_msg_cmp(i->query, msg) != 0)
		{
			aslevent_log(msg, i->outid);
		}
	}
}

int
aslevent_removefd(int fd)
{
	struct aslevent *e, *next;

	e = Eventq.tqh_first;

	while (e != NULL)
	{
		next = e->entries.tqe_next;
		if (fd == e->fd)
		{
			e->fd = -1;
			return 0;
		}

		e = next;
	}

	return -1;
}

const char *
whatsmyhostname()
{
	char *dot;

	if (gethostname(myname, MAXHOSTNAMELEN) < 0)
	{
		memset(myname, 0, sizeof(myname));
		return "localhost";
	}

	dot = strchr(myname, '.');
	if (dot != NULL) *dot = '\0';

	return (const char *)myname;
}

void
asl_client_count_increment()
{
	OSSpinLockLock(&count_lock);

#ifndef CONFIG_IPHONE
	if (global.client_count == 0) vproc_trans = vproc_transaction_begin(NULL);
#endif
	global.client_count++;
#ifdef DEBUG
	asldebug("global.client_count++ (%d)\n", global.client_count);
#endif

	OSSpinLockUnlock(&count_lock);
}

void
asl_client_count_decrement()
{
	OSSpinLockLock(&count_lock);

	if (global.client_count > 0) global.client_count--;
#ifndef CONFIG_IPHONE
	if (global.client_count == 0) vproc_transaction_end(NULL, vproc_trans);
#endif
#ifdef DEBUG
	asldebug("global.client_count-- (%d)\n", global.client_count);
#endif

	OSSpinLockUnlock(&count_lock);
}

int
aslevent_addfd(int source, int fd, uint32_t flags, aslreadfn readfn, aslwritefn writefn, aslexceptfn exceptfn)
{
	struct aslevent *e;
	int found = 0, status;
#ifdef LOCAL_PEERCRED
	struct xucred cr;
#endif
	socklen_t len;
	uid_t u;
	gid_t g;
	struct sockaddr_storage ss;
	char *sender, str[256];

	u = 99;
	g = 99;
	sender = NULL;

	memset(&ss, 0, sizeof(struct sockaddr_storage));
	memset(str, 0, sizeof(str));

	len = sizeof(struct sockaddr_storage);

	if (flags & ADDFD_FLAGS_LOCAL)
	{
		snprintf(str, sizeof(str), "localhost");
		sender = str;

#ifdef LOCAL_PEERCRED
		len = sizeof(cr);

		status = getsockopt(fd, LOCAL_PEERCRED, 1, &cr, &len);
		if (status == 0)
		{
			u = cr.cr_uid;
			g = cr.cr_gid;
		}
#endif
	}
	else
	{
		status = getpeername(fd, (struct sockaddr *)&ss, &len);
		if (status == 0)
		{
			if (len == 0)
			{
				/* UNIX Domain socket */
				snprintf(str, sizeof(str), "localhost");
				sender = str;
			}
			else
			{
				if (inet_ntop(ss.ss_family, (struct sockaddr *)&ss, str, 256) == NULL) sender = str;
			}
		}
	}

	asldebug("source %d fd %d   flags 0x%08x UID %d   GID %d   Sender %s\n", source, fd, flags, u, g, (sender == NULL) ? "NULL" : sender );

	for (e = Eventq.tqh_first; e != NULL; e = e->entries.tqe_next)
	{
		if (fd == e->fd)
		{
			e->readfn = readfn;
			e->writefn = writefn;
			e->exceptfn = exceptfn;
			if (e->sender != NULL) free(e->sender);
			e->sender = NULL;
			if (sender != NULL)
			{
				e->sender = strdup(sender);
				if (e->sender == NULL) return -1;
			}

			e->uid = u;
			e->gid = g;
			found = 1;
		}
	}

	if (found) return 0;

	e = calloc(1, sizeof(struct aslevent));
	if (e == NULL) return -1;

	e->source = source;
	e->fd = fd;
	e->readfn = readfn;
	e->writefn = writefn;
	e->exceptfn = exceptfn;
	e->sender = NULL;
	if (sender != NULL)
	{
		e->sender = strdup(sender);
		if (e->sender == NULL) return -1;
	}

	e->uid = u;
	e->gid = g;

	TAILQ_INSERT_TAIL(&Eventq, e, entries);

	return 0;
}

/*
 * Checks message content and sets attributes as required
 *
 * SOURCE_INTERNAL log messages sent by syslogd itself
 * SOURCE_ASL_SOCKET legacy asl(3) TCP socket
 * SOURCE_BSD_SOCKET legacy syslog(3) UDP socket
 * SOURCE_UDP_SOCKET from the network
 * SOURCE_KERN from the kernel
 * SOURCE_ASL_MESSAGE mach messages sent from Libc by asl(3) and syslog(3)
 * SOURCE_LAUNCHD forwarded from launchd
 */

static uint32_t
aslmsg_verify(uint32_t source, struct aslevent *e, asl_msg_t *msg, int32_t *kern_post_level)
{
	const char *val, *fac;
	char buf[64];
	time_t tick, now;
	uid_t uid;
	uint32_t status, level, fnum;
	pid_t pid;

	if (msg == NULL) return VERIFY_STATUS_INVALID_MESSAGE;

	if (kern_post_level != NULL) *kern_post_level = -1;

	/* Time */
	now = time(NULL);

	tick = 0;
	val = asl_get(msg, ASL_KEY_TIME);
	if (val != NULL) tick = asl_parse_time(val);

	/* Set time to now if it is unset or from the future (not allowed!) */
	if ((tick == 0) || (tick > now)) tick = now;

	/* Canonical form: seconds since the epoch */
	snprintf(buf, sizeof(buf) - 1, "%lu", tick);
	asl_set(msg, ASL_KEY_TIME, buf);

	/* Host */
	if (e == NULL) asl_set(msg, ASL_KEY_HOST, whatsmyhostname());
	else if (e->sender != NULL)
	{
		if (!strcmp(e->sender, "localhost")) asl_set(msg, ASL_KEY_HOST, whatsmyhostname());
		else asl_set(msg, ASL_KEY_HOST, e->sender);
	}

	/* PID */
	pid = 0;

	val = asl_get(msg, ASL_KEY_PID);
	if (val == NULL) asl_set(msg, ASL_KEY_PID, "0");
	else pid = (pid_t)atoi(val);

	/* if PID is 1 (launchd), use the refpid if there is one */
	if (pid == 1)
	{
		val = asl_get(msg, ASL_KEY_REF_PID);
		if (val != NULL) pid = (pid_t)atoi(val);
	}

	/* if quotas are enabled and pid > 1 (not kernel or launchd) check quota */
	if ((global.mps_limit > 0) && (pid > 1))
	{
		status = quota_check(pid, now, msg);
		if (status != VERIFY_STATUS_OK) return status;
	}

	/* UID */
	uid = -2;
	val = asl_get(msg, ASL_KEY_UID);

	switch (source)
	{
		case SOURCE_KERN:
		case SOURCE_INTERNAL:
		{
			/* we know the UID is 0 */
			uid = 0;
			asl_set(msg, ASL_KEY_UID, "0");
			break;
		}
		case SOURCE_ASL_SOCKET:
		case SOURCE_ASL_MESSAGE:
		case SOURCE_LAUNCHD:
		{
			/* we trust the UID in the message */
			if (val != NULL) uid = atoi(val);
			break;
		}
		case SOURCE_BSD_SOCKET:
		case SOURCE_UDP_SOCKET:
		{
			if (val == NULL)
			{
				if (e == NULL) asl_set(msg, ASL_KEY_UID, "-2");
				else if (e->uid == 99) asl_set(msg, ASL_KEY_UID, "-2");
				else
				{
					uid = e->uid;
					snprintf(buf, sizeof(buf), "%d", e->uid);
					asl_set(msg, ASL_KEY_UID, buf);
				}
			}
			else if ((e != NULL) && (e->uid != 99))
			{
				uid = e->uid;
				snprintf(buf, sizeof(buf), "%d", e->uid);
				asl_set(msg, ASL_KEY_UID, buf);
			}
		}
		default:
		{
			asl_set(msg, ASL_KEY_UID, "-2");
		}
	}

	/* GID */
	val = asl_get(msg, ASL_KEY_GID);

	switch (source)
	{
		case SOURCE_KERN:
		case SOURCE_INTERNAL:
		{
			/* we know the GID is 0 */
			asl_set(msg, ASL_KEY_GID, "0");
			break;
		}
		case SOURCE_ASL_SOCKET:
		case SOURCE_ASL_MESSAGE:
		case SOURCE_LAUNCHD:
		{
			/* we trust the GID in the message */
			break;
		}
		case SOURCE_BSD_SOCKET:
		case SOURCE_UDP_SOCKET:
		{
			if (val == NULL)
			{
				if (e == NULL) asl_set(msg, ASL_KEY_GID, "-2");
				else if (e->gid == 99) asl_set(msg, ASL_KEY_GID, "-2");
				else
				{
					snprintf(buf, sizeof(buf), "%d", e->gid);
					asl_set(msg, ASL_KEY_GID, buf);
				}
			}
			else if ((e != NULL) && (e->gid != 99))
			{
				snprintf(buf, sizeof(buf), "%d", e->gid);
				asl_set(msg, ASL_KEY_GID, buf);
			}
		}
		default:
		{
			asl_set(msg, ASL_KEY_GID, "-2");
		}
	}

	/* Sender */
	val = asl_get(msg, ASL_KEY_SENDER);
	if (val == NULL)
	{
		switch (source)
		{
			case SOURCE_KERN:
			{
				asl_set(msg, ASL_KEY_SENDER, "kernel");
				break;
			}
			case SOURCE_INTERNAL:
			{
				asl_set(msg, ASL_KEY_SENDER, "syslogd");
				break;
			}
			default:
			{
				asl_set(msg, ASL_KEY_SENDER, "Unknown");
			}
		}
	}
	else if ((source != SOURCE_KERN) && (uid != 0) && (!strcmp(val, "kernel")))
	{
		/* allow UID 0 to send messages with "Sender kernel", but nobody else */
		asl_set(msg, ASL_KEY_SENDER, "Unknown");
	}

	/* Level */
	val = asl_get(msg, ASL_KEY_LEVEL);
	level = ASL_LEVEL_DEBUG;
	if ((val != NULL) && (val[1] == '\0') && (val[0] >= '0') && (val[0] <= '7')) level = val[0] - '0';
	snprintf(buf, sizeof(buf), "%d", level);
	asl_set(msg, ASL_KEY_LEVEL, buf);

	/* Facility */
	fac = asl_get(msg, ASL_KEY_FACILITY);
	if (fac == NULL)
	{
		if (source == SOURCE_KERN) fac = "kern";
		else fac = "user";
		asl_set(msg, ASL_KEY_FACILITY, fac);
	}
	else if (fac[0] == '#')
	{
		fnum = LOG_USER;
		if ((fac[1] >= '0') && (fac[1] <= '9'))
		{
			fnum = atoi(fac + 1) << 3;
			if ((fnum == 0) && (strcmp(fac + 1, "0"))) fnum = LOG_USER;
		}

		fac = asl_syslog_faciliy_num_to_name(fnum);
		asl_set(msg, ASL_KEY_FACILITY, fac);
	}
	else if (!strncmp(fac, SYSTEM_RESERVED, SYSTEM_RESERVED_LEN))
	{
		/* only UID 0 may use "com.apple.system" */
		if (uid != 0) asl_set(msg, ASL_KEY_FACILITY, FACILITY_USER);
	}

	/*
	 * kernel messages are only readable by root and admin group.
	 */
	if (source == SOURCE_KERN)
	{
		asl_set(msg, ASL_KEY_READ_UID, "0");
		asl_set(msg, ASL_KEY_READ_GID, "80");
	}

	/*
	 * Access Control: only UID 0 may use facility com.apple.system (or anything with that prefix).
	 * N.B. kernel can use any facility name.
	 */

	/* Set DB Expire Time for com.apple.system.utmpx and lastlog */
	if ((!strcmp(fac, "com.apple.system.utmpx")) || (!strcmp(fac, "com.apple.system.lastlog")))
	{
		snprintf(buf, sizeof(buf), "%lu", tick + global.utmp_ttl);
		asl_set(msg, ASL_KEY_EXPIRE_TIME, buf);
	}

	/* Set DB Expire Time for Filestsrem errors */
	if (!strcmp(fac, FSLOG_VAL_FACILITY))
	{
		snprintf(buf, sizeof(buf), "%lu", tick + global.fs_ttl);
		asl_set(msg, ASL_KEY_EXPIRE_TIME, buf);
	}

	/*
	 * special case handling of kernel disaster messages
	 */
	if ((source == SOURCE_KERN) && (level <= KERN_DISASTER_LEVEL))
	{
		if (kern_post_level != NULL) *kern_post_level = level;
		disaster_message(msg);
	}

	return VERIFY_STATUS_OK;
}

int
aslevent_addoutput(aslsendmsgfn fn, const char *outid)
{
	struct asloutput *tmp;

	tmp = calloc(1, sizeof(struct asloutput));
	if (tmp == NULL) return -1;

	tmp->sendmsg = fn;
	tmp->outid = outid;

	TAILQ_INSERT_TAIL(&Outq, tmp, entries);

	return 0;
}

int
aslevent_fdsets(fd_set *rd, fd_set *wr, fd_set *ex)
{
	struct aslevent *e;
	int status = 0;

//	asldebug("--> aslevent_fdsets\n");
	FD_ZERO(rd);
	FD_ZERO(wr);
	FD_ZERO(ex);

	for (e = Eventq.tqh_first; e != NULL; e = e->entries.tqe_next)
	{
		if (e->fd < 0) continue;

//		asldebug("adding fd %d\n", e->fd);
		if (e->readfn)
		{
			FD_SET(e->fd, rd);
			status = MAX(e->fd, status);
		}

		if (e->writefn)
		{
			FD_SET(e->fd, wr);
			status = MAX(e->fd, status);
		}

		if (e->exceptfn)
		{
			FD_SET(e->fd, ex);
			status = MAX(e->fd, status);
		}
	}

//	asldebug("<--aslevent_fdsets\n");
	return status;
}

void
aslevent_cleanup()
{
	struct aslevent *e, *next;

	e = Eventq.tqh_first;

	while (e != NULL)
	{
		next = e->entries.tqe_next;
		if (e->fd < 0)
		{
			TAILQ_REMOVE(&Eventq, e, entries);
			if (e->sender != NULL) free(e->sender);
			free(e);
		}

		e = next;
	}
}

void
list_append_msg(asl_search_result_t *list, asl_msg_t *msg)
{
	if (list == NULL) return;
	if (msg == NULL) return;

	/*
	 * NB: curr is the list size
	 * grow list if necessary
	 */
	if (list->count == list->curr)
	{
		if (list->curr == 0)
		{
			list->msg = (asl_msg_t **)calloc(LIST_SIZE_DELTA, sizeof(asl_msg_t *));
		}
		else
		{
			list->msg = (asl_msg_t **)reallocf(list->msg, (list->curr + LIST_SIZE_DELTA) * sizeof(asl_msg_t *));
		}

		if (list->msg == NULL)
		{
			list->curr = 0;
			list->count = 0;
			return;
		}

		list->curr += LIST_SIZE_DELTA;
	}

	list->msg[list->count] = msg;
	list->count++;
}

void
work_enqueue(asl_msg_t *m)
{
	pthread_mutex_lock(global.work_queue_lock);
	list_append_msg(global.work_queue, m);
	pthread_mutex_unlock(global.work_queue_lock);
	pthread_cond_signal(&global.work_queue_cond);
}

void
asl_enqueue_message(uint32_t source, struct aslevent *e, asl_msg_t *msg)
{
	int32_t kplevel;
	uint32_t status;

	if (msg == NULL) return;

	/* set retain count to 1 */
	msg->type |= 0x10;

	kplevel = -1;
	status = aslmsg_verify(source, e, msg, &kplevel);
	if (status == VERIFY_STATUS_OK)
	{
		if ((source == SOURCE_KERN) && (kplevel >= 0)) notify_post(kern_notify_key[kplevel]);
		work_enqueue(msg);
	}
	else
	{
		asl_msg_release(msg);
	}
}

asl_msg_t **
asl_work_dequeue(uint32_t *count)
{
	asl_msg_t **work;

	pthread_mutex_lock(global.work_queue_lock);
	pthread_cond_wait(&global.work_queue_cond, global.work_queue_lock);

	work = NULL;
	*count = 0;

	if (global.work_queue->count == 0)
	{
		pthread_mutex_unlock(global.work_queue_lock);
		return NULL;
	}

	work = global.work_queue->msg;
	*count = global.work_queue->count;

	global.work_queue->count = 0;
	global.work_queue->curr = 0;
	global.work_queue->msg = NULL;

	pthread_mutex_unlock(global.work_queue_lock);
	return work;
}

void
aslevent_handleevent(fd_set *rd, fd_set *wr, fd_set *ex)
{
	struct aslevent *e;
	char *out = NULL;
	asl_msg_t *msg;
	int32_t cleanup;

//	asldebug("--> aslevent_handleevent\n");

	cleanup = 0;

	for (e = Eventq.tqh_first; e != NULL; e = e->entries.tqe_next)
	{
		if (e->fd < 0)
		{
			cleanup = 1;
			continue;
		}

		if (FD_ISSET(e->fd, rd) && (e->readfn != NULL))
		{
//			asldebug("handling read event on %d\n", e->fd);
			msg = e->readfn(e->fd);
			if (msg == NULL) continue;

			asl_enqueue_message(e->source, e, msg);
		}

		if (FD_ISSET(e->fd, ex) && e->exceptfn)
		{
			asldebug("handling except event on %d\n", e->fd);
			out = e->exceptfn(e->fd);
			if (out == NULL) asldebug("error writing message\n\n");
		}
	}

	if (cleanup != 0) aslevent_cleanup();

//	asldebug("<-- aslevent_handleevent\n");
}

int
asl_log_string(const char *str)
{
	asl_msg_t *msg;

	if (str == NULL) return 1;

	msg = asl_msg_from_string(str);
	if (msg == NULL) return 1;

	asl_enqueue_message(SOURCE_INTERNAL, NULL, msg);

	return 0;
}

int
asldebug(const char *str, ...)
{
	va_list v;
	int status;
	FILE *dfp;

	OSSpinLockLock(&global.lock);
	if (global.debug == 0)
	{
		OSSpinLockUnlock(&global.lock);
		return 0;
	}

	dfp = stderr;
	if (global.debug_file != NULL) dfp = fopen(global.debug_file, "a");
	if (dfp == NULL)
	{
		OSSpinLockUnlock(&global.lock);
		return 0;
	}

	va_start(v, str);
	status = vfprintf(dfp, str, v);
	va_end(v);

	if (global.debug_file != NULL) fclose(dfp);
	OSSpinLockUnlock(&global.lock);

	return status;
}

void
asl_mark(void)
{
	char *str;

	str = NULL;
	asprintf(&str, "[%s syslogd] [%s %u] [%s %u] [%s -- MARK --] [%s 0] [%s 0] [Facility syslog]",
			 ASL_KEY_SENDER,
			 ASL_KEY_LEVEL, ASL_LEVEL_INFO,
			 ASL_KEY_PID, getpid(),
			 ASL_KEY_MSG, ASL_KEY_UID, ASL_KEY_GID);

	asl_log_string(str);
	if (str != NULL) free(str);
}

asl_msg_t *
asl_syslog_input_convert(const char *in, int len, char *rhost, int kern)
{
	int pf, pri, index, n;
	char *p, *colon, *brace, *tmp, *tval, *sval, *pval, *mval;
	char prival[8];
	const char *fval;
	asl_msg_t *msg;
	struct tm time;
	time_t tick;

	if (in == NULL) return NULL;
	if (len <= 0) return NULL;

	pri = LOG_DEBUG;
	tval = NULL;
	sval = NULL;
	pval = NULL;
	mval = NULL;
	fval = NULL;

	index = 0;
	p = (char *)in;

	while ((index < len) && ((*p == ' ') || (*p == '\t')))
	{
		p++;
		index++;
	}

	if (index >= len) return NULL;

	if (*p == '<')
	{
		p++;
		index++;

		n = sscanf(p, "%d", &pf);
		if (n == 1)
		{
			pri = pf & 0x7;
			if (pf > 0x7) fval = asl_syslog_faciliy_num_to_name(pf & LOG_FACMASK);
		}

		while ((index < len) && (*p != '>'))
		{
			p++;
			index++;
		}

		if (index < len)
		{
			p++;
			index++;
		}
	}

	snprintf(prival, sizeof(prival), "%d", pri);

	if (((len - index) > 15) && (p[9] == ':') && (p[12] == ':') && (p[15] == ' '))
	{
		tmp = malloc(16);
		if (tmp == NULL) return NULL;

		memcpy(tmp, p, 15);
		tmp[15] = '\0';

		tick = asl_parse_time(tmp);
		if (tick == (time_t)-1)
		{
			tval = tmp;
		}
		else
		{
			free(tmp);
			gmtime_r(&tick, &time);
			asprintf(&tval, "%d.%02d.%02d %02d:%02d:%02d UTC", time.tm_year + 1900, time.tm_mon + 1, time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec);
		}

		p += 16;
		index += 16;
	}

	if (kern != 0)
	{
		msg = (asl_msg_t *)calloc(1, sizeof(asl_msg_t));
		if (msg == NULL) return NULL;


		asl_set(msg, ASL_KEY_MSG, p);

		asl_set(msg, ASL_KEY_LEVEL, prival);

		asl_set(msg, ASL_KEY_PID, "0");

		asl_set(msg, ASL_KEY_HOST, whatsmyhostname());

		return msg;
	}

	colon = strchr(p, ':');
	brace = strchr(p, '[');

	if (colon != NULL)
	{
		if ((brace != NULL) && (brace < colon))
		{
			n = brace - p;
			sval = malloc(n + 1);
			if (sval == NULL) return NULL;

			memcpy(sval, p, n);
			sval[n] = '\0';

			n = colon - (brace + 1) - 1;
			pval = malloc(n + 1);
			if (pval == NULL) return NULL;

			memcpy(pval, (brace + 1), n);
			pval[n] = '\0';
		}
		else
		{
			n = colon - p;
			sval = malloc(n + 1);
			if (sval == NULL) return NULL;

			memcpy(sval, p, n);
			sval[n] = '\0';
		}

		n = colon - p;
		p = colon + 1;
		index += (n + 1);
	}

	if (*p == ' ')
	{
		p++;
		index++;
	}

	n = len - index;
	if (n > 0)
	{
		mval = malloc(n + 1);
		if (mval == NULL) return NULL;

		memcpy(mval, p, n);
		mval[n] = '\0';
	}

	if (fval == NULL) fval = asl_syslog_faciliy_num_to_name(LOG_USER);

	msg = (asl_msg_t *)calloc(1, sizeof(asl_msg_t));
	if (msg == NULL) return NULL;

	if (tval != NULL)
	{
		asl_set(msg, ASL_KEY_TIME, tval);
		free(tval);
	}

	if (fval != NULL) asl_set(msg, "Facility", fval);
	else asl_set(msg, "Facility", "user");

	if (sval != NULL)
	{
		asl_set(msg, ASL_KEY_SENDER, sval);
		free(sval);
	}

	if (pval != NULL)
	{
		asl_set(msg, ASL_KEY_PID, pval);
		free(pval);
	}
	else asl_set(msg, ASL_KEY_PID, "-1");

	if (mval != NULL)
	{
		asl_set(msg, ASL_KEY_MSG, mval);
		free(mval);
	}

	asl_set(msg, ASL_KEY_LEVEL, prival);
	asl_set(msg, ASL_KEY_UID, "-2");
	asl_set(msg, ASL_KEY_GID, "-2");

	if (rhost == NULL) asl_set(msg, ASL_KEY_HOST, whatsmyhostname());
	else asl_set(msg, ASL_KEY_HOST, rhost);

	if (msg->count == 0)
	{
		asl_msg_release(msg);
		return NULL;
	}

	return msg;
}

asl_msg_t *
asl_input_parse(const char *in, int len, char *rhost, int kern)
{
	asl_msg_t *m;
	int status, x, legacy;

	asldebug("asl_input_parse: %s\n", (in == NULL) ? "NULL" : in);

	if (in == NULL) return NULL;

	legacy = 1;
	m = NULL;

	/* calculate length if not provided */
	if (len == 0) len = strlen(in);

	/*
	 * Determine if the input is "old" syslog format or new ASL format.
	 * Old format lines should start with "<", but they can just be straight text.
	 * ASL input starts with a length (10 bytes) followed by a space and a '['.
	 */
	if ((in[0] != '<') && (len > 11))
	{
		status = sscanf(in, "%d ", &x);
		if ((status == 1) && (in[10] == ' ') && (in[11] == '[')) legacy = 0;
	}

	if (legacy == 1) return asl_syslog_input_convert(in, len, rhost, kern);

	m = asl_msg_from_string(in + 11);
	if (m == NULL) return NULL;

	if (rhost != NULL) asl_set(m, ASL_KEY_HOST, rhost);

	return m;
}

char *
get_line_from_file(FILE *f)
{
	char *s, *out;
	size_t len;

	out = fgetln(f, &len);
	if (out == NULL) return NULL;
	if (len == 0) return NULL;

	s = malloc(len + 1);
	if (s == NULL) return NULL;

	memcpy(s, out, len);

	s[len - 1] = '\0';
	return s;
}

uint32_t
asl_msg_type(asl_msg_t *m)
{
	if (m == NULL) return ASL_TYPE_ERROR;
	return (m->type & ASL_MSG_TYPE_MASK);
}

void
asl_msg_release(asl_msg_t *m)
{
	int32_t newval;

	if (m == NULL) return;

	newval = OSAtomicAdd32(-0x10, (int32_t*)&m->type) >> 4;
	assert(newval >= 0);

	if (newval > 0) return;

	asl_free(m);
}

asl_msg_t *
asl_msg_retain(asl_msg_t *m)
{
	int32_t newval;

	if (m == NULL) return NULL;

	newval = OSAtomicAdd32(0x10, (int32_t*)&m->type) >> 4;
	assert(newval > 0);

	return m;
}

void
launchd_callback(struct timeval *when, pid_t from_pid, pid_t about_pid, uid_t sender_uid, gid_t sender_gid, int priority, const char *from_name, const char *about_name, const char *session_name, const char *msg)
{
	asl_msg_t *m;
	char str[256];
	time_t now;

/*
	asldebug("launchd_callback Time %lu %lu PID %u RefPID %u UID %d GID %d PRI %d Sender %s Ref %s Session %s Message %s\n",
	when->tv_sec, when->tv_usec, from_pid, about_pid, sender_uid, sender_gid, priority, from_name, about_name, session_name, msg);
*/

	m = asl_new(ASL_TYPE_MSG);
	if (m == NULL) return;

	/* Level */
	if (priority < ASL_LEVEL_EMERG) priority = ASL_LEVEL_EMERG;
	if (priority > ASL_LEVEL_DEBUG) priority = ASL_LEVEL_DEBUG;
	snprintf(str, sizeof(str), "%d", priority);

	asl_set(m, ASL_KEY_LEVEL, str);

	/* Time */
	if (when != NULL)
	{
		snprintf(str, sizeof(str), "%lu", when->tv_sec);
		asl_set(m, ASL_KEY_TIME, str);

		snprintf(str, sizeof(str), "%lu", 1000 * (unsigned long int)when->tv_usec);
		asl_set(m, ASL_KEY_TIME_NSEC, str);
	}
	else
	{
		now = time(NULL);
		snprintf(str, sizeof(str), "%lu", now);
		asl_set(m, ASL_KEY_TIME, str);
	}

	/* Host */
	asl_set(m, ASL_KEY_HOST, whatsmyhostname());

	/* Facility */
	asl_set(m, ASL_KEY_FACILITY, FACILITY_CONSOLE);

	/* UID */
	snprintf(str, sizeof(str), "%u", (unsigned int)sender_uid);
	asl_set(m, ASL_KEY_UID, str);

	/* GID */
	snprintf(str, sizeof(str), "%u", (unsigned int)sender_gid);
	asl_set(m, ASL_KEY_GID, str);

	/* PID */
	if (from_pid != 0)
	{
		snprintf(str, sizeof(str), "%u", (unsigned int)from_pid);
		asl_set(m, ASL_KEY_PID, str);
	}

	/* Reference PID */
	if ((about_pid > 0) && (about_pid != from_pid))
	{
		snprintf(str, sizeof(str), "%u", (unsigned int)about_pid);
		asl_set(m, ASL_KEY_REF_PID, str);
	}

	/* Sender */
	if (from_name != NULL)
	{
		asl_set(m, ASL_KEY_SENDER, from_name);
	}

	/* ReadUID */
	if (sender_uid != 0)
	{
		snprintf(str, sizeof(str), "%d", (int)sender_uid);
		asl_set(m, ASL_KEY_READ_UID, str);
	}

	/* Reference Process */
	if (about_name != NULL)
	{
		if ((from_name != NULL) && (strcmp(from_name, about_name) != 0))
		{
			asl_set(m, ASL_KEY_REF_PROC, about_name);
		}
	}

	/* Session */
	if (session_name != NULL)
	{
		asl_set(m, ASL_KEY_SESSION, session_name);
	}

	/* Message */
	if (msg != NULL)
	{
		asl_set(m, ASL_KEY_MSG, msg);
	}

	/* verify and push to receivers */
	asl_enqueue_message(SOURCE_LAUNCHD, NULL, m);
}

void
launchd_drain()
{
	forever
	{
		_vprocmgr_log_drain(NULL, NULL, launchd_callback);
	}
}
