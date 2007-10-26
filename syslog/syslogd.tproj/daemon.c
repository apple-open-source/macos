/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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
#include <sys/queue.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define SYSLOG_NAMES
#include <syslog.h>
#include <sys/fslog.h>
#include <vproc.h>
#include <pthread.h>
#include <vproc_priv.h>
#include <mach/mach.h>
#include "daemon.h"

#define streq(A,B) (strcmp(A,B)==0)
#define forever for(;;)

#define ASL_MSG_TYPE_MASK 0x0000000f
#define ASL_TYPE_ERROR 2

#define ASL_KEY_FACILITY "Facility"

#define FACILITY_USER "user"
#define FACILITY_CONSOLE "com.apple.console"
#define SYSTEM_RESERVED "com.apple.system"
#define SYSTEM_RESERVED_LEN 16

extern void disaster_message(asl_msg_t *m);

static char myname[MAXHOSTNAMELEN + 1] = {0};
extern const char *debug_file;
extern int debug;
extern time_t utmp_ttl;
extern time_t fs_ttl;
extern int kfd;

static pthread_mutex_t event_lock = PTHREAD_MUTEX_INITIALIZER;

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

struct aslevent
{
	int fd;
	unsigned char read:1; 
	unsigned char write:1; 
	unsigned char except:1;
	aslreadfn readfn;
	aslwritefn writefn;
	aslexceptfn exceptfn;
	char *sender;
	uid_t uid;
	gid_t gid;
	TAILQ_ENTRY(aslevent) entries;
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

static struct aslevent *launchd_event;

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
aslevent_match(asl_msg_t *msg)
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

int
aslevent_addfd(int fd, uint32_t flags, aslreadfn readfn, aslwritefn writefn, aslexceptfn exceptfn)
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

	asldebug("fd %d   flags 0x%08x UID %d   GID %d   Sender %s\n", fd, flags, u, g, (sender == NULL) ? "NULL" : sender );

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

static int
aslmsg_verify(struct aslevent *e, asl_msg_t *msg, int32_t *kern_post_level)
{
	const char *val, *fac;
	char buf[32], *timestr;
	time_t tick, now;
	struct tm gtime;
	uid_t uid;
	uint32_t level, fnum, kern;
	int isreserved;

	if (msg == NULL) return -1;

	*kern_post_level = -1;

	kern = 0;
	if ((e != NULL) && (e->fd == kfd)) kern = 1;

	/* Time */
	now = time(NULL);

	tick = 0;
	val = asl_get(msg, ASL_KEY_TIME);
	if (val != NULL) tick = asl_parse_time(val);

	/* Set time to now if it is unset or from the future (not allowed!) */
	if ((tick == 0) || (tick > now)) tick = now;

	memset(&gtime, 0, sizeof(struct tm));
	gmtime_r(&tick, &gtime);

	/* Canonical form: YYYY.MM.DD hh:mm:ss UTC */
	asprintf(&timestr, "%d.%02d.%02d %02d:%02d:%02d UTC", gtime.tm_year + 1900, gtime.tm_mon + 1, gtime.tm_mday, gtime.tm_hour, gtime.tm_min, gtime.tm_sec);
	if (timestr != NULL)
	{
		asl_set(msg, ASL_KEY_TIME, timestr);
		free(timestr);
	}

	/* Host */
	if (e == NULL) asl_set(msg, ASL_KEY_HOST, whatsmyhostname());
	else if (e->sender != NULL)
	{
		if (!strcmp(e->sender, "localhost")) asl_set(msg, ASL_KEY_HOST, whatsmyhostname());
		else asl_set(msg, ASL_KEY_HOST, e->sender);
	}

	/* PID */
	val = asl_get(msg, ASL_KEY_PID);
	if (val == NULL) asl_set(msg, ASL_KEY_PID, "0");

	/* UID */
	uid = -2;
	val = asl_get(msg, ASL_KEY_UID);
	if (kern == 1)
	{
		uid = 0;
		asl_set(msg, ASL_KEY_UID, "0");
	}
	else if (val == NULL)
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

	/* GID */
	val = asl_get(msg, ASL_KEY_GID);
	if (kern == 1)
	{
		asl_set(msg, ASL_KEY_GID, "0");
	}
	else if (val == NULL)
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

	/* Sender */
	val = asl_get(msg, ASL_KEY_SENDER);
	if (val == NULL)
	{
		if (kern == 0) asl_set(msg, ASL_KEY_SENDER, "Unknown");
		else asl_set(msg, ASL_KEY_SENDER, "kernel");
	}
	else if ((kern == 0) && (uid != 0) && (!strcmp(val, "kernel")))
	{
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
		if (kern == 1) fac = "kern";
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

	/*
	 * Access Control: only UID 0 may use facility com.apple.system (or anything with that prefix).
	 * N.B. kernel can use any facility name.
	 */

	/* Set DB Expire Time for com.apple.system.utmpx and lastlog */
	if ((!strcmp(fac, "com.apple.system.utmpx")) || (!strcmp(fac, "com.apple.system.lastlog")))
	{
		snprintf(buf, sizeof(buf), "%lu", tick + utmp_ttl);
		asl_set(msg, ASL_KEY_EXPIRE_TIME, buf);
	}

	/* Set DB Expire Time for Filestsrem errors */
	if (!strcmp(fac, FSLOG_VAL_FACILITY))
	{
		snprintf(buf, sizeof(buf), "%lu", tick + fs_ttl);
		asl_set(msg, ASL_KEY_EXPIRE_TIME, buf);
	}

	if (e != NULL)
	{
		isreserved = 0;
		if (!strncmp(fac, SYSTEM_RESERVED, SYSTEM_RESERVED_LEN))
		{
			if (uid != 0) asl_set(msg, ASL_KEY_FACILITY, FACILITY_USER);
			else isreserved = 1;
		}
	}

	/*
	 * special case handling of kernel disaster messages
	 */
	if ((kern == 1) && (level <= KERN_DISASTER_LEVEL))
	{
		*kern_post_level = level;
		disaster_message(msg);
	}

	return 0;
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
aslevent_handleevent(fd_set *rd, fd_set *wr, fd_set *ex)
{
	struct aslevent *e;
	char *out = NULL;
	asl_msg_t *msg;
	int32_t kplevel, cleanup;

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

			/* type field is overloaded to provide retain/release inside syslogd */
			msg->type |= 0x10;

			pthread_mutex_lock(&event_lock);

			kplevel = -1;
			if (aslmsg_verify(e, msg, &kplevel) < 0)
			{
				asl_msg_release(msg);
				asldebug("recieved invalid message\n\n");
			}
			else
			{
				aslevent_match(msg);
				asl_msg_release(msg);
				if (kplevel >= 0) notify_post(kern_notify_key[kplevel]);
			}

			pthread_mutex_unlock(&event_lock);
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
	int32_t unused;

	if (str == NULL) return 1;

	msg = asl_msg_from_string(str);

	/* set retain count */
	msg->type |= 0x10;

	pthread_mutex_lock(&event_lock);

	unused = -1;
	if (aslmsg_verify(NULL, msg, &unused) < 0)
	{
		pthread_mutex_unlock(&event_lock);
		asl_msg_release(msg);
		return -1;
	}

	aslevent_match(msg);
	asl_msg_release(msg);

	pthread_mutex_unlock(&event_lock);

	return 0;
}

int
asldebug(const char *str, ...)
{
	va_list v;
	int status;
	FILE *dfp;

	if (debug == 0) return 0;

	dfp = stderr;
	if (debug_file != NULL) dfp = fopen(debug_file, "a");
	if (dfp == NULL) return 0;

	va_start(v, str);
	status = vfprintf(dfp, str, v);
	va_end(v);

	if (debug_file != NULL) fclose(dfp);
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
	uint32_t refcount;

	if (m == NULL) return;

	refcount = m->type >> 4;
	if (refcount > 0)
	{
		refcount--;
		m->type -= 0x10;
	}

	if (refcount > 0) return;
	asl_free(m);
}

asl_msg_t *
asl_msg_retain(asl_msg_t *m)
{
	if (m == NULL) return NULL;

	m->type += 0x10;
	return m;
}

void
launchd_callback(struct timeval *when, pid_t from_pid, pid_t about_pid, uid_t sender_uid, gid_t sender_gid, int priority, const char *from_name, const char *about_name, const char *session_name, const char *msg)
{
	asl_msg_t *m;
	char str[256];
	int32_t unused;
	int status;
	time_t now;

/*
	asldebug("launchd_callback Time %lu %lu PID %u RefPID %u UID %d GID %d PRI %d Sender %s Ref %s Session %s Message %s\n",
	when->tv_sec, when->tv_usec, from_pid, about_pid, sender_uid, sender_gid, priority, from_name, about_name, session_name, msg);
*/

	if (launchd_event != NULL)
	{
		launchd_event->uid = sender_uid;
		launchd_event->gid = sender_gid;
	}

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
	if (from_name != NULL) asl_set(m, ASL_KEY_SENDER, from_name);

	/* ReadUID */
	if (sender_uid != 0)
	{
		snprintf(str, sizeof(str), "%d", (int)sender_uid);
		asl_set(m, ASL_KEY_READ_UID, str);
	}

	/* Reference Process */
	if (about_name != NULL)
	{
		if ((from_name != NULL) && (strcmp(from_name, about_name) != 0)) asl_set(m, ASL_KEY_REF_PROC, about_name);
	}

	/* Session */
	if (session_name != NULL) asl_set(m, ASL_KEY_SESSION, session_name);

	/* Message */
	if (msg != NULL) asl_set(m, ASL_KEY_MSG, msg);

	/* set retain count */
	m->type |= 0x10;

	/* verify and push to receivers */
	status = aslmsg_verify(launchd_event, m, &unused);
	if (status >= 0) aslevent_match(m);

	asl_msg_release(m);
}

void
launchd_drain()
{
	launchd_event = (struct aslevent *)calloc(1, sizeof(struct aslevent));

	forever
	{
		_vprocmgr_log_drain(NULL, &event_lock, launchd_callback);
	}
}
