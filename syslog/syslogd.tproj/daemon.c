/*
 * Copyright (c) 2004-2011 Apple Inc. All rights reserved.
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

extern void disaster_message(aslmsg m);
extern int asl_action_reset(void);

static char myname[MAXHOSTNAMELEN + 1] = {0};
static int name_change_token = -1;

static OSSpinLock count_lock = 0;

#ifndef CONFIG_IPHONE
static vproc_transaction_t vproc_trans = {0};
#endif

#define QUOTA_TABLE_SIZE 8192
#define QUOTA_TABLE_SLOTS 8

#define QUOTA_EXCEEDED_MESSAGE "*** process %d exceeded %d log message per second limit  -  remaining messages this second discarded ***"
#define QUOTA_KERN_EXCEEDED_MESSAGE "*** kernel exceeded %d log message per second limit  -  remaining messages this second discarded ***"
#define QUOTA_EXCEEDED_LEVEL "3"

#define DEFAULT_DB_FILE_MAX 25600000
#define DEFAULT_DB_MEMORY_MAX 8192
#define DEFAULT_DB_MINI_MAX 256
#define DEFAULT_MPS_LIMIT 500
#define DEFAULT_REMOTE_DELAY 5000
#define DEFAULT_BSD_MAX_DUP_SEC 30
#define DEFAULT_MARK_SEC 0
#define DEFAULT_UTMP_TTL_SEC 31622400

static time_t quota_table_time = 0;
static pid_t quota_table_pid[QUOTA_TABLE_SIZE];
static int32_t quota_table_quota[QUOTA_TABLE_SIZE];
static int32_t kern_quota;
static int32_t kern_level;

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

static int kern_notify_token[8] = {-1, -1, -1, -1, -1, -1, -1, -1 };

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
quota_check(pid_t pid, time_t now, aslmsg msg, uint32_t level)
{
	int i, x, maxx, max;
	char *str, lstr[2];

	if (msg == NULL) return VERIFY_STATUS_INVALID_MESSAGE;
	if (global.mps_limit == 0) return VERIFY_STATUS_OK;

	if (quota_table_time != now)
	{
		memset(quota_table_pid, 0, sizeof(quota_table_pid));
		kern_quota = global.mps_limit;
		kern_level = 7;
		quota_table_time = now;
	}

	/* kernel gets it's own quota */
	if (pid == 0)
	{
		if (level < kern_level) kern_level = level;
		if (kern_quota > 0) kern_quota--;

		if (kern_quota > 0) return VERIFY_STATUS_OK;
		if (kern_quota < 0)	return VERIFY_STATUS_EXCEEDED_QUOTA;

		kern_quota = -1;

		str = NULL;
		asprintf(&str, QUOTA_KERN_EXCEEDED_MESSAGE, global.mps_limit);
		if (str != NULL)
		{
			asl_set(msg, ASL_KEY_MSG, str);
			free(str);
			lstr[0] = kern_level + '0';
			lstr[1] = 0;
			asl_set(msg, ASL_KEY_LEVEL, lstr);
		}

		return VERIFY_STATUS_OK;
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

				return VERIFY_STATUS_OK;
			}

			if (quota_table_quota[x] < 0)
			{
				return VERIFY_STATUS_EXCEEDED_QUOTA;
			}

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
	asldebug("Quotas: reused slot %d pid %d quota %d for new pid %d\n", maxx, (int)quota_table_pid[maxx], quota_table_quota[maxx], (int)pid);
	quota_table_pid[maxx] = pid;
	quota_table_quota[maxx] = global.mps_limit;

	return VERIFY_STATUS_OK;
}

int
asl_check_option(aslmsg msg, const char *opt)
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

const char *
whatsmyhostname()
{
	static dispatch_once_t once;
	char *dot;
	int check, status;

	dispatch_once(&once, ^{
		snprintf(myname, sizeof(myname), "%s", "localhost");
		notify_register_check(kNotifySCHostNameChange, &name_change_token);
	});

	check = 1;
	status = 0;

	if (name_change_token >= 0) status = notify_check(name_change_token, &check);

	if ((status == 0) && (check == 0)) return (const char *)myname;

	if (gethostname(myname, MAXHOSTNAMELEN) < 0)
	{
		snprintf(myname, sizeof(myname), "%s", "localhost");
	}
	else
	{
		dot = strchr(myname, '.');
		if (dot != NULL) *dot = '\0';
	}

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
aslmsg_verify(aslmsg msg, uint32_t source, int32_t *kern_post_level, uid_t *uid_out)
{
	const char *val, *fac, *ruval, *rgval;
	char buf[64];
	time_t tick, now;
	uid_t uid;
	gid_t gid;
	uint32_t status, level, fnum;
	pid_t pid;

	if (msg == NULL) return VERIFY_STATUS_INVALID_MESSAGE;

	if (kern_post_level != NULL) *kern_post_level = -1;
	if (uid_out != NULL) *uid_out = -2;

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

	/* Time */
	now = time(NULL);

	/* Level */
	val = asl_get(msg, ASL_KEY_LEVEL);
	level = ASL_LEVEL_DEBUG;
	if ((val != NULL) && (val[1] == '\0') && (val[0] >= '0') && (val[0] <= '7')) level = val[0] - '0';
	snprintf(buf, sizeof(buf), "%d", level);
	asl_set(msg, ASL_KEY_LEVEL, buf);

	/*
	 * check quota if no processes are watching
	 */
	if (global.watchers_active == 0)
	{
		status = quota_check(pid, now, msg, level);
		if (status != VERIFY_STATUS_OK) return status;
	}

	tick = 0;
	val = asl_get(msg, ASL_KEY_TIME);
	if (val != NULL) tick = asl_parse_time(val);

	/* Set time to now if it is unset or from the future (not allowed!) */
	if ((tick == 0) || (tick > now)) tick = now;

	/* Canonical form: seconds since the epoch */
	snprintf(buf, sizeof(buf) - 1, "%lu", tick);
	asl_set(msg, ASL_KEY_TIME, buf);

	val = asl_get(msg, ASL_KEY_HOST);
	if (val == NULL) asl_set(msg, ASL_KEY_HOST, whatsmyhostname());

	uid = -2;
	val = asl_get(msg, ASL_KEY_UID);
	if (val != NULL)
	{
		uid = atoi(val);
		if ((uid == 0) && strcmp(val, "0")) uid = -2;
		if (uid_out != NULL) *uid_out = uid;
	}

	gid = -2;
	val = asl_get(msg, ASL_KEY_GID);
	if (val != NULL)
	{
		gid = atoi(val);
		if ((gid == 0) && strcmp(val, "0")) gid = -2;
	}

	/* UID  & GID */
	switch (source)
	{
		case SOURCE_KERN:
		case SOURCE_INTERNAL:
		{
			asl_set(msg, ASL_KEY_UID, "0");
			asl_set(msg, ASL_KEY_GID, "0");
			break;
		}
		case SOURCE_ASL_SOCKET:
		case SOURCE_ASL_MESSAGE:
		case SOURCE_LAUNCHD:
		{
			/* we trust the UID & GID in the message */
			break;
		}
		default:
		{
			/* we do not trust the UID 0 or GID 0 or 80 in the message */
			if (uid == 0) asl_set(msg, ASL_KEY_UID, "-2");
			if ((gid == 0) || (gid == 80)) asl_set(msg, ASL_KEY_GID, "-2");
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
	 * all other messages are admin-only readable unless they already
	 * have specific read access controls set.
	 */
	if (source == SOURCE_KERN)
	{
		asl_set(msg, ASL_KEY_READ_UID, "0");
		asl_set(msg, ASL_KEY_READ_GID, "80");
	}
	else
	{
		ruval = asl_get(msg, ASL_KEY_READ_UID);
		rgval = asl_get(msg, ASL_KEY_READ_GID);

		if ((ruval == NULL) && (rgval == NULL))
		{
			asl_set(msg, ASL_KEY_READ_GID, "80");
		}
	}

	/* Set DB Expire Time for com.apple.system.utmpx and lastlog */
	if ((!strcmp(fac, "com.apple.system.utmpx")) || (!strcmp(fac, "com.apple.system.lastlog")))
	{
		snprintf(buf, sizeof(buf), "%lu", tick + global.utmp_ttl);
		asl_set(msg, ASL_KEY_EXPIRE_TIME, buf);
	}

	/* Set DB Expire Time for Filesystem errors */
	if (!strcmp(fac, FSLOG_VAL_FACILITY))
	{
		snprintf(buf, sizeof(buf), "%lu", tick + FS_TTL_SEC);
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

void
list_append_msg(asl_search_result_t *list, aslmsg msg)
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

	list->msg[list->count] = (asl_msg_t *)msg;
	list->count++;
}

void
init_globals(void)
{
	OSSpinLockLock(&global.lock);

	global.debug = 0;
	free(global.debug_file);
	global.debug_file = NULL;

#ifdef CONFIG_IPHONE
	global.dbtype = DB_TYPE_MINI;
#else
	global.dbtype = DB_TYPE_FILE;
#endif
	global.db_file_max = DEFAULT_DB_FILE_MAX;
	global.db_memory_max = DEFAULT_DB_MEMORY_MAX;
	global.db_mini_max = DEFAULT_DB_MINI_MAX;
	global.mps_limit = DEFAULT_MPS_LIMIT;
	global.remote_delay_time = DEFAULT_REMOTE_DELAY;
	global.bsd_max_dup_time = DEFAULT_BSD_MAX_DUP_SEC;
	global.mark_time = DEFAULT_MARK_SEC;
	global.utmp_ttl = DEFAULT_UTMP_TTL_SEC;

	OSSpinLockUnlock(&global.lock);
}

/*
 * Used to set config parameters.
 * Line format "= name value"
 */
int
control_set_param(const char *s)
{
	char **l;
	uint32_t intval, count, v32a, v32b, v32c;

	if (s == NULL) return -1;
	if (s[0] == '\0') return 0;

	/* skip '=' and whitespace */
	s++;
	while ((*s == ' ') || (*s == '\t')) s++;

	l = explode(s, " \t");
	if (l == NULL) return -1;

	for (count = 0; l[count] != NULL; count++);

	/* name is required */
	if (count == 0)
	{
		freeList(l);
		return -1;
	}

	/* value is required */
	if (count == 1)
	{
		freeList(l);
		return -1;
	}

	if (!strcasecmp(l[0], "debug"))
	{
		/* = debug {0|1} [file] */
		intval = atoi(l[1]);
		config_debug(intval, l[2]);
	}
	else if (!strcasecmp(l[0], "mark_time"))
	{
		/* = mark_time seconds */
		OSSpinLockLock(&global.lock);
		global.mark_time = atoll(l[1]);
		OSSpinLockUnlock(&global.lock);
	}
	else if (!strcasecmp(l[0], "dup_delay"))
	{
		/* = bsd_max_dup_time seconds */
		OSSpinLockLock(&global.lock);
		global.bsd_max_dup_time = atoll(l[1]);
		OSSpinLockUnlock(&global.lock);
	}
	else if (!strcasecmp(l[0], "remote_delay"))
	{
		/* = remote_delay microseconds */
		OSSpinLockLock(&global.lock);
		global.remote_delay_time = atol(l[1]);
		OSSpinLockUnlock(&global.lock);
	}
	else if (!strcasecmp(l[0], "utmp_ttl"))
	{
		/* = utmp_ttl seconds */
		OSSpinLockLock(&global.lock);
		global.utmp_ttl = (time_t)atoll(l[1]);
		OSSpinLockUnlock(&global.lock);
	}
	else if (!strcasecmp(l[0], "mps_limit"))
	{
		/* = mps_limit number */
		OSSpinLockLock(&global.lock);
		global.mps_limit = (uint32_t)atol(l[1]);
		OSSpinLockUnlock(&global.lock);
	}
	else if (!strcasecmp(l[0], "max_file_size"))
	{
		/* = max_file_size bytes */
		pthread_mutex_lock(global.db_lock);

		if (global.dbtype & DB_TYPE_FILE)
		{
			asl_store_close(global.file_db);
			global.file_db = NULL;
			global.db_file_max = atoi(l[1]);
		}

		pthread_mutex_unlock(global.db_lock);
	}
	else if ((!strcasecmp(l[0], "db")) || (!strcasecmp(l[0], "database")) || (!strcasecmp(l[0], "datastore")))
	{
		/* NB this is private / unpublished */
		/* = db type [max]... */

		v32a = 0;
		v32b = 0;
		v32c = 0;

		if ((l[1][0] >= '0') && (l[1][0] <= '9'))
		{
			intval = atoi(l[1]);
			if ((count >= 3) && (strcmp(l[2], "-"))) v32a = atoi(l[2]);
			if ((count >= 4) && (strcmp(l[3], "-"))) v32b = atoi(l[3]);
			if ((count >= 5) && (strcmp(l[4], "-"))) v32c = atoi(l[4]);
		}
		else if (!strcasecmp(l[1], "file"))
		{
			intval = DB_TYPE_FILE;
			if ((count >= 3) && (strcmp(l[2], "-"))) v32a = atoi(l[2]);
		}
		else if (!strncasecmp(l[1], "mem", 3))
		{
			intval = DB_TYPE_MEMORY;
			if ((count >= 3) && (strcmp(l[2], "-"))) v32b = atoi(l[2]);
		}
		else if (!strncasecmp(l[1], "min", 3))
		{
			intval = DB_TYPE_MINI;
			if ((count >= 3) && (strcmp(l[2], "-"))) v32c = atoi(l[2]);
		}
		else
		{
			freeList(l);
			return -1;
		}

		if (v32a == 0) v32a = global.db_file_max;
		if (v32b == 0) v32b = global.db_memory_max;
		if (v32c == 0) v32c = global.db_mini_max;

		config_data_store(intval, v32a, v32b, v32c);
	}

	freeList(l);
	return 0;
}

static int
control_message(aslmsg msg)
{
	const char *str = asl_get(msg, ASL_KEY_MSG);

	if (str == NULL) return 0;

	if (!strncmp(str, "= reset", 7))
	{
		init_globals();
		return asl_action_reset();
	}
	else if (!strncmp(str, "= rotate", 8))
	{
		const char *p = str + 8;
		while ((*p == ' ') || (*p == '\t')) p++;
		if (*p == '\0') p = NULL;
		return asl_action_file_rotate(p);
	}
	else if (!strncmp(str, "= ", 2))
	{
		return control_set_param(str);
	}

	return 0;
}

void
process_message(aslmsg msg, uint32_t source)
{
	int32_t kplevel;
	uint32_t status;
	uid_t uid;

	if (msg == NULL) return;

	kplevel = -1;
	uid = -2;

	status = aslmsg_verify(msg, source, &kplevel, &uid);
	if (status == VERIFY_STATUS_OK)
	{
		if ((source == SOURCE_KERN) && (kplevel >= 0))
		{
			if (kplevel > 7) kplevel = 7;
			if (kern_notify_token[kplevel] < 0)
			{
				status = notify_register_plain(kern_notify_key[kplevel], &(kern_notify_token[kplevel]));
				if (status != 0) asldebug("notify_register_plain(%s) failed status %u\n", status);
			}

			notify_post(kern_notify_key[kplevel]);
		}

		if ((uid == 0) && asl_check_option(msg, ASL_OPT_CONTROL)) control_message(msg);

		/* send message to output modules */
		asl_out_message(msg);
		if (global.bsd_out_enabled) bsd_out_message(msg);
	}

	asl_free(msg);
}

int
internal_log_message(const char *str)
{
	aslmsg msg;

	if (str == NULL) return 1;

	msg = (aslmsg)asl_msg_from_string(str);
	if (msg == NULL) return 1;

	dispatch_async(global.work_queue, ^{ process_message(msg, SOURCE_INTERNAL); });

	return 0;
}

int
asldebug(const char *str, ...)
{
	va_list v;
	FILE *dfp = NULL;

	if (global.debug == 0) return 0;

	if (global.debug_file == NULL) dfp = fopen(_PATH_SYSLOGD_LOG, "a");
	else dfp = fopen(global.debug_file, "a");
	if (dfp == NULL) return 0;

	va_start(v, str);
	vfprintf(dfp, str, v);
	va_end(v);

	fclose(dfp);

	return 0;
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

	internal_log_message(str);
	if (str != NULL) free(str);
}

aslmsg 
asl_syslog_input_convert(const char *in, int len, char *rhost, uint32_t source)
{
	int pf, pri, index, n;
	char *p, *colon, *brace, *space, *tmp, *tval, *hval, *sval, *pval, *mval;
	char prival[8];
	const char *fval;
	aslmsg msg;
	struct tm time;
	time_t tick;

	if (in == NULL) return NULL;
	if (len <= 0) return NULL;

	pri = LOG_DEBUG;
	tval = NULL;
	hval = NULL;
	sval = NULL;
	pval = NULL;
	mval = NULL;
	fval = NULL;

	index = 0;
	p = (char *)in;

	/* skip leading whitespace */
	while ((index < len) && ((*p == ' ') || (*p == '\t')))
	{
		p++;
		index++;
	}

	if (index >= len) return NULL;

	/* parse "<NN>" priority (level and facility) */
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

	/* check if a timestamp is included */
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

	/* stop here for kernel messages */
	if (source == SOURCE_KERN)
	{
		msg = asl_new(ASL_TYPE_MSG);
		if (msg == NULL) return NULL;

		asl_set(msg, ASL_KEY_MSG, p);
		asl_set(msg, ASL_KEY_LEVEL, prival);
		asl_set(msg, ASL_KEY_PID, "0");

		return msg;
	}

	/* if message is from a network socket, hostname follows */
	if (source == SOURCE_UDP_SOCKET)
	{
		space = strchr(p, ' ');
		if (space != NULL)
		{
			n = space - p;
			hval = malloc(n + 1);
			if (hval == NULL) return NULL;

			memcpy(hval, p, n);
			hval[n] = '\0';

			p = space + 1;
			index += (n + 1);
		}
	}

	colon = strchr(p, ':');
	brace = strchr(p, '[');

	/* check for "sender:" or sender[pid]:"  */
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

	msg = asl_new(ASL_TYPE_MSG);
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
	else
	{
		asl_set(msg, ASL_KEY_PID, "-1");
	}

	if (mval != NULL)
	{
		asl_set(msg, ASL_KEY_MSG, mval);
		free(mval);
	}

	asl_set(msg, ASL_KEY_LEVEL, prival);
	asl_set(msg, ASL_KEY_UID, "-2");
	asl_set(msg, ASL_KEY_GID, "-2");

	if (hval != NULL)
	{
		asl_set(msg, ASL_KEY_HOST, hval);
		free(hval);
	}
	else if (rhost != NULL)
	{
		asl_set(msg, ASL_KEY_HOST, rhost);
	}

	return msg;
}

aslmsg 
asl_input_parse(const char *in, int len, char *rhost, uint32_t source)
{
	aslmsg msg;
	int status, x, legacy, off;

	asldebug("asl_input_parse: %s\n", (in == NULL) ? "NULL" : in);

	if (in == NULL) return NULL;

	legacy = 1;
	msg = NULL;

	/* calculate length if not provided */
	if (len == 0) len = strlen(in);

	/*
	 * Determine if the input is "old" syslog format or new ASL format.
	 * Old format lines should start with "<", but they can just be straight text.
	 * ASL input may start with a length (10 bytes) followed by a space and a '['.
	 * The length is optional, so ASL messages may also just start with '['.
	 */
	if ((in[0] != '<') && (len > 11))
	{
		status = sscanf(in, "%d ", &x);
		if ((status == 1) && (in[10] == ' ') && (in[11] == '[')) legacy = 0;
	}

	if (legacy == 1) return asl_syslog_input_convert(in, len, rhost, source);

	off = 11;
	if (in[0] == '[') off = 0;

	msg = (aslmsg)asl_msg_from_string(in + off);
	if (msg == NULL) return NULL;

	if (rhost != NULL) asl_set(msg, ASL_KEY_HOST, rhost);

	return msg;
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

	if (s[len - 1] != '\n') len++;
	s[len - 1] = '\0';
	return s;
}

void
launchd_callback(struct timeval *when, pid_t from_pid, pid_t about_pid, uid_t sender_uid, gid_t sender_gid, int priority, const char *from_name, const char *about_name, const char *session_name, const char *msg)
{
	aslmsg m;
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

	dispatch_async(global.work_queue, ^{ process_message(m, SOURCE_LAUNCHD); });
}

