/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 2004 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
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
#include "daemon.h"

#define streq(A,B) (strcmp(A,B)==0)

static char myname[MAXHOSTNAMELEN + 1] = {0};
static int gotname = 0;

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
	struct aslevent *i;
	int found = -1;

	for (i = Eventq.tqh_first; i != NULL; i = i->entries.tqe_next)
	{
		if (fd == i->fd)
		{
			asldebug("removing %d\n", i->fd);
			TAILQ_REMOVE(&Eventq, i, entries);
			found = 0;
			if (i->sender != NULL) free(i->sender);
			free(i);
			i = NULL;
			return 0;
		}
	}

	return -1;
}

const char *
whatsmyhostname()
{
	char *dot;

	if (gotname != 0) return (const char *)myname;

	if (gethostname(myname, MAXHOSTNAMELEN) < 0)
	{
		memset(myname, 0, sizeof(myname));
		return "localhost";
	}

	if (strcmp(myname, "localhost")) gotname = 1;

	dot = strchr(myname, '.');
	if (dot != NULL) *dot = '\0';

	return (const char *)myname;
}

int
aslevent_addfd(int fd, aslreadfn readfn, aslwritefn writefn, aslexceptfn exceptfn)
{
	struct aslevent *e;
	int found = 0;
#ifdef LOCAL_PEERCRED
	struct xucred cr;
#endif
	int len;
	uid_t u;
	gid_t g;
	struct sockaddr_storage ss;
	char *sender, str[256];

	u = 99;
	g = 99;
	sender = NULL;

	memset(&ss, 0, sizeof(struct sockaddr_storage));

	len = sizeof(struct sockaddr_storage);

	if (getpeername(fd, (struct sockaddr *)&ss, &len) == 0)
	{
		if (len == 0)
		{
			/* UNIX Domain socket */
			snprintf(str, sizeof(str), whatsmyhostname());
			sender = str;
		}
		else
		{
			if (inet_ntop(ss.ss_family, (struct sockaddr *)&ss, str, 256) == 0) sender = str;
		}
	}

#ifdef LOCAL_PEERCRED
	len = sizeof(cr);

	if (getsockopt(fd, LOCAL_PEERCRED, 1, &cr, &len) == 0)
	{
		u = cr.cr_uid;
		g = cr.cr_gid;
	}
#endif

	asldebug("fd %d   UID %d   GID %d   Sender %s\n", fd, u, g, (sender == NULL) ? "NULL" : sender );

	for (e = Eventq.tqh_first; e != NULL; e = e->entries.tqe_next)
	{
		if (fd == e->fd)
		{
			e->readfn = readfn;
			e->writefn = writefn;
			e->exceptfn = exceptfn;
			if (e->sender != NULL) free(e->sender);
			e->sender = NULL;
			if (sender != NULL) e->sender = strdup(sender);
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
	if (sender != NULL) e->sender = strdup(sender);
	e->uid = u;
	e->gid = g;

	TAILQ_INSERT_TAIL(&Eventq, e, entries);

	return 0;
}

int
aslmsg_verify(struct aslevent *e, asl_msg_t *msg)
{
	const char *val;
	char buf[32], *timestr;
	time_t tick;
	struct tm gtime;

	if (msg == NULL) return -1;

	/* Time */
	tick = 0;
	val = asl_get(msg, ASL_KEY_TIME);
	if (val != NULL) tick = asl_parse_time(val);

	if (tick == 0) tick = time(NULL);
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
	else if (e->sender != NULL) asl_set(msg, ASL_KEY_HOST, e->sender);

	/* Sender */
	val = asl_get(msg, ASL_KEY_SENDER);
	if (val == NULL) asl_set(msg, ASL_KEY_SENDER, "Unknown");

	/* PID */
	val = asl_get(msg, ASL_KEY_PID);
	if (val == NULL) asl_set(msg, ASL_KEY_PID, "0");

	/* UID */
	val = asl_get(msg, ASL_KEY_UID);
	if (val == NULL)
	{
		if (e == NULL) asl_set(msg, ASL_KEY_UID, "-2");
		else if (e->uid == 99) asl_set(msg, ASL_KEY_UID, "-2");
		else
		{
			snprintf(buf, sizeof(buf), "%d", e->uid);
			asl_set(msg, ASL_KEY_UID, buf);
		}
	}
	else if ((e != NULL) && (e->uid != 99))
	{
		snprintf(buf, sizeof(buf), "%d", e->uid);
		asl_set(msg, ASL_KEY_UID, buf);
	}

	/* GID */
	val = asl_get(msg, ASL_KEY_GID);
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

	/* Level */
	val = asl_get(msg, ASL_KEY_LEVEL);
	if (val == NULL) asl_set(msg, ASL_KEY_LEVEL, "7");

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

	asldebug("--> aslevent_fdsets\n");
	FD_ZERO(rd);
	FD_ZERO(wr);
	FD_ZERO(ex);

	for (e = Eventq.tqh_first; e != NULL; e = e->entries.tqe_next)
	{
		asldebug("adding fd %d\n", e->fd);
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

	asldebug("<--aslevent_fdsets\n");
	return status;
}

void
aslevent_handleevent(fd_set rd, fd_set wr, fd_set ex, char *errstr)
{
	struct aslevent *e;
	char *out = NULL;
	asl_msg_t *msg;

	asldebug("--> aslevent_handleevent\n");
	if (errstr) errstr = NULL;

	for (e = Eventq.tqh_first; e != NULL; e = e->entries.tqe_next)
	{
		if (FD_ISSET(e->fd, &rd) && (e->readfn != NULL))
		{
			asldebug("handling read event on %d\n", e->fd);
			msg = e->readfn(e->fd);
			if (msg == NULL)
			{
				asldebug("error reading message\n");
				continue;
			}

			if (aslmsg_verify(e, msg) < 0)
			{
				asl_free(msg);
				asldebug("recieved invalid message\n");
			}
			else
			{
				aslevent_match(msg);
				asl_free(msg);
			}
		}

		if (FD_ISSET(e->fd, &ex) && e->exceptfn)
		{
			asldebug("handling except event on %d\n", e->fd);
			out = e->exceptfn(e->fd);
			if (out == NULL) asldebug("error writing message\n");
		}
	}

	asldebug("<-- aslevent_handleevent\n");
}

int
asl_log_string(const char *str)
{
	asl_msg_t *msg;

	if (str == NULL) return 1;

	msg = asl_msg_from_string(str);
	if (aslmsg_verify(NULL, msg) < 0)
	{
		asl_free(msg);
		return -1;
	}

	aslevent_match(msg);
	asl_free(msg);
	return 0;
}

void
aslmark(void)
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

	asldebug("asl_syslog_input_convert: %s\n", in); 

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
		msg->type = ASL_TYPE_MSG;

		asl_set(msg, ASL_KEY_SENDER, "kernel");

		asl_set(msg, "Facility", "kern");
		if (tval != NULL)
		{
			asl_set(msg, ASL_KEY_TIME, tval);
			free(tval);
		}

		asl_set(msg, ASL_KEY_MSG, p);

		asl_set(msg, ASL_KEY_LEVEL, prival);

		asl_set(msg, ASL_KEY_PID, "0");

		asl_set(msg, ASL_KEY_UID, "0");

		asl_set(msg, ASL_KEY_GID, "0");

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
			memcpy(sval, p, n);
			sval[n] = '\0';

			n = colon - (brace + 1) - 1;
			pval = malloc(n + 1);
			memcpy(pval, (brace + 1), n);
			pval[n] = '\0';
		}
		else
		{
			n = colon - p;
			sval = malloc(n + 1);
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
		memcpy(mval, p, n);
		mval[n] = '\0';
	}

	if (fval == NULL) fval = asl_syslog_faciliy_num_to_name(LOG_USER);

	msg = (asl_msg_t *)calloc(1, sizeof(asl_msg_t));
	msg->type = ASL_TYPE_MSG;

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
		asl_free(msg);
		return NULL;
	}

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
	memcpy(s, out, len);

	s[len - 1] = '\0';
	return s;
}
