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
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <notify.h>
#include "daemon.h"

#define MY_ID "bsd_out"

#define _PATH_WALL "/usr/bin/wall"
#define ASL_KEY_FACILITY "Facility"
#define FACILITY_KERNEL "kern"
#define _PATH_CONSOLE "/dev/console"
#define IndexNull ((uint32_t)-1)

#define DST_TYPE_NONE 0
#define DST_TYPE_FILE 1
#define DST_TYPE_CONS 2
#define DST_TYPE_SOCK 3
#define DST_TYPE_WALL 4
#define DST_TYPE_NOTE 5

static asl_msg_t *query = NULL;
static int reset = 0;

struct config_rule
{
	uint32_t count;
	char *dst;
	int fd;
	int type;
	struct sockaddr *addr;
	char **facility;
	int *pri;
	TAILQ_ENTRY(config_rule) entries;
};

static TAILQ_HEAD(cr, config_rule) bsd_out_rule;

int bsd_out_close();
static int _parse_config_file(const char *);

static void
_do_reset(void)
{
	bsd_out_close();
	_parse_config_file(_PATH_SYSLOG_CONF);
}

static char **
_insertString(char *s, char **l, uint32_t x)
{
	int i, len;

	if (s == NULL) return l;
	if (l == NULL) 
	{
		l = (char **)malloc(2 * sizeof(char *));
		l[0] = strdup(s);
		l[1] = NULL;
		return l;
	}

	for (i = 0; l[i] != NULL; i++);
	len = i + 1; /* count the NULL on the end of the list too! */

	l = (char **)realloc(l, (len + 1) * sizeof(char *));

	if ((x >= (len - 1)) || (x == IndexNull))
	{
		l[len - 1] = strdup(s);
		l[len] = NULL;
		return l;
	}

	for (i = len; i > x; i--) l[i] = l[i - 1];
	l[x] = strdup(s);
	return l;
}

static char **
_explode(char *s, char *delim)
{
	char **l = NULL;
	char *p, *t;
	int i, n;

	if (s == NULL) return NULL;

	p = s;
	while (p[0] != '\0')
	{
		for (i = 0; ((p[i] != '\0') && (strchr(delim, p[i]) == NULL)); i++);
		n = i;
		t = malloc(n + 1);
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

static void
_freeList(char **l)
{
	int i;

	if (l == NULL) return;
	for (i = 0; l[i] != NULL; i++) free(l[i]);
	free(l);
}

static int
_level_for_name(const char *name)
{
	if (name == NULL) return -1;

	if (!strcasecmp(name, "emerg")) return ASL_LEVEL_EMERG;
	if (!strcasecmp(name, "panic")) return ASL_LEVEL_EMERG;
	if (!strcasecmp(name, "alert")) return ASL_LEVEL_ALERT;
	if (!strcasecmp(name, "crit")) return ASL_LEVEL_CRIT;
	if (!strcasecmp(name, "err")) return ASL_LEVEL_ERR;
	if (!strcasecmp(name, "error")) return ASL_LEVEL_ERR;
	if (!strcasecmp(name, "warn")) return ASL_LEVEL_WARNING;
	if (!strcasecmp(name, "warning")) return ASL_LEVEL_WARNING;
	if (!strcasecmp(name, "notice")) return ASL_LEVEL_NOTICE;
	if (!strcasecmp(name, "info")) return ASL_LEVEL_INFO;
	if (!strcasecmp(name, "debug")) return ASL_LEVEL_DEBUG;
	if (!strcmp(name, "*")) return ASL_LEVEL_DEBUG;

	/* special case */
	if (!strcasecmp(name, "none")) return -2;

	return -1;
}

static int
_syslog_dst_open(struct config_rule *r)
{
	int i;
	char *node, *serv;
	struct addrinfo hints, *gai, *ai;

	if (r == NULL) return -1;

	r->fd = -1;

	if (r->dst[0] == '/')
	{
		r->fd = open(r->dst, O_WRONLY | O_APPEND | O_CREAT, 0644);
		if (r->fd < 0)
		{
			asldebug("%s: open failed for file: %s (%s)\n", MY_ID, r->dst, strerror(errno));
			return -1;
		}

		r->type = DST_TYPE_FILE;
		if (!strcmp(r->dst, _PATH_CONSOLE)) r->type = DST_TYPE_CONS;

		return 0;
	}

	if (r->dst[0] == '!')
	{
		r->type = DST_TYPE_NOTE;
		return 0;
	}

	if (r->dst[0] == '@')
	{
		node = strdup(r->dst + 1);
		if (node == NULL) return -1;

		serv = NULL;
		serv = strrchr(node, ':');
		if (serv != NULL) *serv++ = '\0';
		else serv = "syslog";

		memset(&hints, 0, sizeof(hints));
		hints.ai_family = PF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM;
		i = getaddrinfo(node, serv, &hints, &gai);
		free(node);
		if (i != 0)
		{
			asldebug("%s: getaddrinfo failed for node %s service %s: (%s)\n", MY_ID, node, serv, gai_strerror(i));
			return -1;
		}

		for (ai = gai; ai != NULL; ai = ai->ai_next)
		{
			r->fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
			if (r->fd < 0) continue;

			r->addr = (struct sockaddr *)calloc(1, ai->ai_addrlen);
			memcpy(r->addr, ai->ai_addr, ai->ai_addrlen);

			break;
		}

		freeaddrinfo(gai);

		if (r->fd < 0)
		{
			asldebug("%s: connection failed for %s\n", MY_ID, (r->dst) + 1);
			return -1;
		}

		if (fcntl(r->fd, F_SETFL, O_NONBLOCK) < 0)
		{
			close(r->fd);
			r->fd = -1;
			asldebug("%s: couldn't set O_NONBLOCK for fd %d: %s\n", MY_ID, r->fd, strerror(errno));
			return -1;
		}

		r->type = DST_TYPE_SOCK;
		return 0;

	}

	if (strcmp(r->dst, "*") == 0)
	{
		r->type = DST_TYPE_WALL;
		return 0;
	}

	/* Can't deal with dst! */
	asldebug("%s: unsupported / unknown output name: %s\n", MY_ID, r->dst);
	return -1;
}

static int
_parse_line(char *s)
{
	char **semi, **comma;
	int i, j, n, lasts, lastc, pri;
	struct config_rule *out;

	if (s == NULL) return -1;
	while ((*s == ' ') || (*s == '\t')) s++;
	if (*s == '#') return -1;

	semi = _explode(s, "; \t");

	if (semi == NULL) return -1;
	out = (struct config_rule *)calloc(1, sizeof(struct config_rule));
	if (out == NULL) return -1;

	n = 0;
	lasts = -1;
	for (i = 0; semi[i] != NULL; i++)
	{
		if (semi[i][0] == '\0') continue;
		n++;
		lasts = i;
	}

	out->dst = strdup(semi[lasts]);
	_syslog_dst_open(out);

	for (i = 0; i < lasts; i++)
	{
		if (semi[i][0] == '\0') continue;
		comma = _explode(semi[i], ",.");
		lastc = -1;
		for (j = 0; comma[j] != NULL; j++)
		{
			if (comma[j][0] == '\0') continue;
			lastc = j;
		}

		for (j = 0; j < lastc; j++)
		{
			if (comma[j][0] == '\0') continue;
			pri = _level_for_name(comma[lastc]);
			if (pri == -1) continue;

			if (out->count == 0)
			{
				out->facility = (char **)calloc(1, sizeof(char *));
				out->pri = (int *)calloc(1, sizeof(int));
			}
			else
			{
				out->facility = (char **)realloc(out->facility, (out->count + 1) * sizeof(char *));
				out->pri = (int *)realloc(out->pri, (out->count + 1) * sizeof(int));
			}
			out->facility[out->count] = strdup(comma[j]);
			out->pri[out->count] = pri;
			out->count++;
		}

		_freeList(comma);
	}

	_freeList(semi);

	TAILQ_INSERT_TAIL(&bsd_out_rule, out, entries);

	return 0;
}

static int
_syslog_send(asl_msg_t *msg, struct config_rule *r, char **out, char **fwd)
{
	char *so, *sf, *vt, *p;
	const char *vtime, *vhost, *vident, *vpid, *vmsg, *vlevel, *vfacility;
	size_t outlen, n;
	int pf, fc, status;
	FILE *pw;
	time_t tick;

	if (out == NULL) return -1;
	if (fwd == NULL) return -1;

	if (r->type == DST_TYPE_NOTE)
	{
		notify_post(r->dst+1);
		return 0;
	}

	vt = NULL;

	/* Build output string if it hasn't been built by a previous rule-match */
	if (*out == NULL)
	{
		vtime = asl_get(msg, ASL_KEY_TIME);
		if (vtime != NULL)
		{
			tick = asl_parse_time(vtime);
			if (tick != (time_t)-1)
			{
				p = ctime(&tick);
				vt = malloc(16);
				if (vt == NULL) return -1;
				memcpy(vt, p+4, 15);
				vt[15] = '\0';
			}
		}
		else if (strlen(vtime) < 24) vt = strdup(vtime);
		else
		{
			vt = malloc(16);
			if (vt == NULL) return -1;
			memcpy(vt, vtime+4, 15);
			vt[15] = '\0';
		}

		if (vt == NULL)
		{
			tick = time(NULL);
			p = ctime(&tick);
			vt = malloc(16);
			if (vt == NULL) return -1;
			memcpy(vt, p+4, 15);
			vt[15] = '\0';
		}

		vhost = asl_get(msg, ASL_KEY_HOST);
		if (vhost == NULL) vhost = "localhost";

		vident = asl_get(msg, ASL_KEY_SENDER);
		if ((vident != NULL) && (!strcmp(vident, "Unknown"))) vident = NULL;

		vpid = asl_get(msg, ASL_KEY_PID);
		if ((vpid != NULL) && (!strcmp(vpid, "-1"))) vpid = NULL;

		if ((vpid != NULL) && (vident == NULL)) vident = "Unknown";

		vmsg = asl_get(msg, ASL_KEY_MSG);

		n = 0;
		if (vt != NULL) n += (strlen(vt) + 1);
		if (vhost != NULL) n += (strlen(vhost) + 1);
		if (vident != NULL) n += strlen(vident);
		n += 2;
		if (vpid != NULL) n += (strlen(vpid) + 2);
		if (vmsg != NULL) n += strlen(vmsg);

		if (n == 0) return -1;
		n += 2;

		so = calloc(1, n);
		if (so == NULL) return -1;

		if (vt != NULL)
		{
			strcat(so, vt);
			strcat(so, " ");
		}

		if (vhost != NULL)
		{
			strcat(so, vhost);
			strcat(so, " ");
		}

		if (vident != NULL)
		{
			strcat(so, vident);
			if (vpid != NULL)
			{
				strcat(so, "[");
				strcat(so, vpid);
				strcat(so, "]");
			}
		}

		strcat(so, ": ");

		if (vmsg != NULL)
		{
			strcat(so, vmsg);
			strcat(so, "\n");
		}

		free(vt);
		*out = so;
	}

	if ((*fwd == NULL) && (r->type == DST_TYPE_SOCK))
	{
		pf = 7;
		vlevel = asl_get(msg, ASL_KEY_LEVEL);
		if (vlevel != NULL) pf = atoi(vlevel);

		fc = asl_syslog_faciliy_name_to_num(asl_get(msg, ASL_KEY_FACILITY));
		if (fc > 0) pf |= fc;

		sf = NULL;
		asprintf(&sf, "<%d>%s", pf, *out);
		if (sf == NULL) return -1;
		*fwd = sf;
	}

	outlen = 0;
	if (r->type == DST_TYPE_SOCK) outlen = strlen(*fwd);
	else outlen = strlen(*out);

	if ((r->type == DST_TYPE_FILE) || (r->type == DST_TYPE_CONS))
	{
		/*
		 * Special case for kernel messages.
		 * Don't write kernel messages to /dev/console.
		 * The kernel printf routine already sends them to /dev/console
		 * so writing them here would cause duplicates.
		 */
		vfacility = asl_get(msg, ASL_KEY_FACILITY);
		if ((vfacility != NULL) && (!strcmp(vfacility, FACILITY_KERNEL)) && (r->type == DST_TYPE_CONS)) return 0;

		status = write(r->fd, *out, outlen);
		if ((status < 0) || (status < outlen))
		{
			asldebug("%s: error writing message (%s): %s\n", MY_ID, r->dst, strerror(errno));

			/* Try re-opening the file (once) and write again */
			close(r->fd);
			r->fd = open(r->dst, O_WRONLY | O_APPEND | O_CREAT, 0644);
			if (r->fd < 0)
			{
				asldebug("%s: re-open failed for file: %s (%s)\n", MY_ID, r->dst, strerror(errno));
				return -1;
			}

			status = write(r->fd, *out, outlen);
			if ((status < 0) || (status < outlen))
			{
				asldebug("%s: error re-writing message (%s): %s\n", MY_ID, r->dst, strerror(errno));
			}
		}
	}
	else if (r->type == DST_TYPE_SOCK)
	{
		status = sendto(r->fd, *fwd, outlen, 0, r->addr, r->addr->sa_len);
		if (status < 0) asldebug("%s: error sending message (%s): %s\n", MY_ID, r->dst, strerror(errno));
	}
	else if (r->type == DST_TYPE_WALL)
	{
		pw = popen(_PATH_WALL, "w");
		if (pw < 0)
		{
			asldebug("%s: error sending wall message: %s\n", MY_ID, strerror(errno));
			return -1;
		}

		fprintf(pw, *out);
		pclose(pw);
	}

	return 0;
}

static int
_syslog_rule_match(asl_msg_t *msg, struct config_rule *r)
{
	uint32_t i, test, f, pri;
	const char *val;

	if (msg == NULL) return 0;
	if (r == NULL) return 0;
	if (r->count == 0) return 0;

	test = 0;

	for (i = 0; i < r->count; i++)
	{
		if (r->pri[i] == -1) continue;

		if ((test == 1) && (r->pri[i] >= 0)) continue;
		if ((test == 0) && (r->pri[i] == -2)) continue;

		f = 0;
		if (strcmp(r->facility[i], "*") == 0) f = 1;
		else
		{
			val = asl_get(msg, ASL_KEY_FACILITY);
			if ((val != NULL) && (strcasecmp(r->facility[i], val) == 0)) f = 1;
		}

		if (f == 0) continue;

		/* Turn off matching facility with priority "none" */
		if (r->pri[i] == -2)
		{
			test = 0;
			continue;
		}

		val = asl_get(msg, ASL_KEY_LEVEL);
		if (val == NULL) continue;

		pri = atoi(val);
		if (pri < 0) continue;

		if (pri <= r->pri[i]) test = 1;
	}

	return test;
}

int
bsd_out_sendmsg(asl_msg_t *msg, const char *outid)
{
	struct config_rule *r;
	char *out, *fwd;

	if (reset != 0)
	{
		_do_reset();
		reset = 0;
	}

	if (msg == NULL) return -1;

	out = NULL;
	fwd = NULL;

	for (r = bsd_out_rule.tqh_first; r != NULL; r = r->entries.tqe_next)
	{
		if (_syslog_rule_match(msg, r) == 1) _syslog_send(msg, r, &out, &fwd);
	}

	if (out != NULL) free(out);
	if (fwd != NULL) free(fwd);

	return 0;
}

static int
_parse_config_file(const char *confname)
{
	FILE *cf;
	char *line;

	cf = fopen(confname, "r");
	if (cf == NULL) return 1;

	while (NULL != (line = get_line_from_file(cf)))
	{
		_parse_line(line);
		free(line);
	}

	fclose(cf);

	return 0;
}

int
bsd_out_init(void)
{
	asldebug("%s: init\n", MY_ID);

	TAILQ_INIT(&bsd_out_rule);

	query = asl_new(ASL_TYPE_QUERY);
	aslevent_addmatch(query, MY_ID);
	aslevent_addoutput(bsd_out_sendmsg, MY_ID);

	_parse_config_file(_PATH_SYSLOG_CONF);
	return 0;
}

int
bsd_out_reset(void)
{
	reset = 1;
	return 0;
}

int
bsd_out_close(void)
{
	struct config_rule *r, *n;
	int i;

	n = NULL;
	for (r = bsd_out_rule.tqh_first; r != NULL; r = n)
	{
		n = r->entries.tqe_next;
		
		if (r->dst != NULL) free(r->dst);
		if (r->fd > 0) close(r->fd);
		if (r->addr != NULL) free(r->addr);
		if (r->facility != NULL)
		{
			for (i = 0; i < r->count; i++) 
			{
				if (r->facility[i] != NULL) free(r->facility[i]);
			}
			free(r->facility);
		}
		if (r->pri != NULL) free(r->pri);

		TAILQ_REMOVE(&bsd_out_rule, r, entries);
		free(r);
	}

	return 0;
}
