/*
 * Copyright (c) 2004-2008 Apple Inc. All rights reserved.
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
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <notify.h>
#include "daemon.h"

#define MY_ID "bsd_out"

#define _PATH_WALL "/usr/bin/wall"
#define ASL_KEY_FACILITY "Facility"
#define FACILITY_KERNEL "kern"
#define _PATH_CONSOLE "/dev/console"

#define DST_TYPE_NONE 0
#define DST_TYPE_FILE 1
#define DST_TYPE_CONS 2
#define DST_TYPE_SOCK 3
#define DST_TYPE_WALL 4
#define DST_TYPE_NOTE 5

#define CLOSE_ON_IDLE_SEC 60

static asl_msg_t *query = NULL;
static int reset = RESET_NONE;
static pthread_mutex_t reset_lock = PTHREAD_MUTEX_INITIALIZER;

struct config_rule
{
	uint32_t count;
	char *dst;
	int fd;
	int type;
	struct sockaddr *addr;
	char **facility;
	int *pri;
	uint32_t last_hash;
	uint32_t last_count;
	time_t last_time;
	char *last_msg;
	TAILQ_ENTRY(config_rule) entries;
};

static TAILQ_HEAD(cr, config_rule) bsd_out_rule;

extern uint32_t asl_core_string_hash(const char *s, uint32_t inlen);

int bsd_out_close();
int bsd_out_network_reset(void);
static int _parse_config_file(const char *);

static void
_do_reset(void)
{
	pthread_mutex_lock(&reset_lock);
	if (reset == RESET_NONE)
	{
		pthread_mutex_unlock(&reset_lock);
		return;
	}

	if (reset == RESET_CONFIG)
	{
		bsd_out_close();
		_parse_config_file(_PATH_SYSLOG_CONF);
	}
	else if (reset == RESET_NETWORK)
	{
		bsd_out_network_reset();
	}

	reset = RESET_NONE;

	pthread_mutex_unlock(&reset_lock);
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
	if (r->fd != -1) return 0;

	if (r->dst[0] == '/')
	{
		r->fd = open(r->dst, O_WRONLY | O_APPEND | O_CREAT | O_NOCTTY, 0644);
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
		r->fd = -1;
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

			r->addr = (struct sockaddr *)malloc(ai->ai_addrlen);
			if (r->addr == NULL) return -1;

			memcpy(r->addr, ai->ai_addr, ai->ai_addrlen);

			break;
		}

		freeaddrinfo(gai);

		if (r->fd < 0)
		{
			asldebug("%s: connection failed for %s\n", MY_ID, (r->dst) + 1);
			free(r->addr);
			r->addr = NULL;
			return -1;
		}

		if (fcntl(r->fd, F_SETFL, O_NONBLOCK) < 0)
		{
			close(r->fd);
			r->fd = -1;
			asldebug("%s: couldn't set O_NONBLOCK for fd %d: %s\n", MY_ID, r->fd, strerror(errno));
			free(r->addr);
			r->addr = NULL;
			return -1;
		}

		r->type = DST_TYPE_SOCK;
		return 0;

	}

	if (strcmp(r->dst, "*") == 0)
	{
		r->type = DST_TYPE_WALL;
		r->fd = -1;
		return 0;
	}

	/* Can't deal with dst! */
	asldebug("%s: unsupported / unknown output name: %s\n", MY_ID, r->dst);
	return -1;
}

static void
_syslog_dst_close(struct config_rule *r)
{
	if (r == NULL) return;

	if (r->addr != NULL)
	{
		free(r->addr);
		r->addr = NULL;
	}

	switch (r->type)
	{
		case DST_TYPE_FILE:
		case DST_TYPE_CONS:
		{
			if (r->fd >= 0) close(r->fd);
			r->fd = -1;
			break;
		}

		case DST_TYPE_SOCK:
		{
			if (r->fd >= 0) close(r->fd);
			r->fd = -1;
			break;
		}

		case DST_TYPE_NONE:
		case DST_TYPE_WALL:
		case DST_TYPE_NOTE:
		default:
		{
			/* do nothing */
			return;
		}
	}
}

static char *
_clean_facility_name(char *s)
{
	uint32_t len;
	char *p, *out;

	if (s == NULL) return NULL;
	len = strlen(s);
	if (len == 0) return NULL;

	p = s;

	if ((*s == '\'') || (*s == '"'))
	{
		len--;
		p++;
		if (p[len - 1] == *s) len --;
	}

	out = calloc(1, len + 1);
	if (out == NULL) return NULL;

	memcpy(out, p, len);
	return out;
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

	semi = explode(s, "; \t");

	if (semi == NULL) return -1;
	out = (struct config_rule *)calloc(1, sizeof(struct config_rule));
	if (out == NULL) return -1;
	out->fd = -1;

	n = 0;
	lasts = -1;
	for (i = 0; semi[i] != NULL; i++)
	{
		if (semi[i][0] == '\0') continue;
		n++;
		lasts = i;
	}

	out->dst = strdup(semi[lasts]);
	if (out->dst == NULL) return -1;

	for (i = 0; i < lasts; i++)
	{
		if (semi[i][0] == '\0') continue;
		comma = explode(semi[i], ",.");
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
				out->facility = (char **)reallocf(out->facility, (out->count + 1) * sizeof(char *));
				out->pri = (int *)reallocf(out->pri, (out->count + 1) * sizeof(int));
			}

			if (out->facility == NULL) return -1;
			if (out->pri == NULL) return -1;

			out->facility[out->count] = _clean_facility_name(comma[j]);
			if (out->facility[out->count] == NULL) return -1;

			out->pri[out->count] = pri;
			out->count++;
		}

		freeList(comma);
	}

	freeList(semi);

	TAILQ_INSERT_TAIL(&bsd_out_rule, out, entries);

	return 0;
}

static char *
bsd_log_string(const char *msg)
{
	uint32_t i, len, outlen;
	char *out, *q;
	uint8_t c;

	if (msg == NULL) return NULL;

	len = strlen(msg);
	while ((len > 0) && (msg[len - 1] == '\n')) len--;

	if (len == 0) return NULL;

	outlen = len + 1;
	for (i = 0; i < len; i++)
	{
		c = msg[i];
		if (isascii(c) && iscntrl(c) && (c != '\t')) outlen++;
	}

	out = malloc(outlen);
	if (out == NULL) return NULL;

	q = out;

	for (i = 0; i < len; i++)
	{
		c = msg[i];

		if (isascii(c) && iscntrl(c))
		{
			if (c == '\n')
			{
				*q++ = '\\';
				*q++ = 'n';
			}
			else if (c == '\t')
			{
				*q++ = c;
			}
			else
			{
				*q++ = '^';
				*q++ = c ^ 0100;
			}
		}
		else
		{
			*q++ = c;
		}
	}

	*q = '\0';

	return out;
}

static int
_syslog_send_repeat_msg(struct config_rule *r)
{
	char vt[32], *msg;
	time_t tick;
	int len, status;

	if (r == NULL) return -1;
	if (r->type != DST_TYPE_FILE) return 0;
	if (r->last_count == 0) return 0;

	tick = time(NULL);

	memset(vt, 0, sizeof(vt));
	ctime_r(&tick, vt);
	vt[19] = '\0';

	msg = NULL;
	asprintf(&msg, "%s: --- last message repeated %u time%s ---\n", vt + 4, r->last_count, (r->last_count == 1) ? "" : "s");
	if (msg == NULL) return -1;

	len = strlen(msg);
	status = write(r->fd, msg, len);
	if ((status < 0) || (status < len))
	{
		asldebug("%s: error writing repeat message (%s): %s\n", MY_ID, r->dst, strerror(errno));

		/* Try re-opening the file (once) and write again */
		close(r->fd);
		r->fd = open(r->dst, O_WRONLY | O_APPEND | O_CREAT | O_NOCTTY, 0644);
		if (r->fd < 0)
		{
			asldebug("%s: re-open failed for file: %s (%s)\n", MY_ID, r->dst, strerror(errno));
			free(msg);
			return -1;
		}

		status = write(r->fd, msg, len);
		if ((status < 0) || (status < len))
		{
			asldebug("%s: error re-writing message (%s): %s\n", MY_ID, r->dst, strerror(errno));
			free(msg);
			return -1;
		}
	}

	free(msg);
	return 0;
}

static int
_syslog_send(asl_msg_t *msg, struct config_rule *r, char **out, char **fwd, time_t now)
{
	char vt[16], tstr[32], *so, *sf, *outmsg;
	const char *vtime, *vhost, *vident, *vpid, *vmsg, *vlevel, *vfacility, *vrefproc, *vrefpid;
	size_t outlen, n;
	time_t tick;
	int pf, fc, status, is_dup, do_write;
	FILE *pw;
	uint32_t msg_hash;

	if (out == NULL) return -1;
	if (fwd == NULL) return -1;
	if (r == NULL) return -1;

	if (r->type == DST_TYPE_NOTE)
	{
		notify_post(r->dst+1);
		return 0;
	}

	msg_hash = 0;
	outmsg = NULL;

	/* Build output string if it hasn't been built by a previous rule-match */
	if (*out == NULL)
	{
		tick = now;
		vtime = asl_get(msg, ASL_KEY_TIME);
		if (vtime != NULL)
		{
			/* aslmsg_verify converts time to seconds, but use current time if something went sour */
			tick = atol(vtime);
			if (tick == 0) tick = now;
		}

		memset(tstr, 0, sizeof(tstr));
		ctime_r(&tick, tstr);
		memcpy(vt, tstr+4, 15);
		vt[15] = '\0';

		vhost = asl_get(msg, ASL_KEY_HOST);
		if (vhost == NULL) vhost = "localhost";

		vident = asl_get(msg, ASL_KEY_SENDER);
		if ((vident != NULL) && (!strcmp(vident, "Unknown"))) vident = NULL;

		vpid = asl_get(msg, ASL_KEY_PID);
		if ((vpid != NULL) && (!strcmp(vpid, "-1"))) vpid = NULL;

		if ((vpid != NULL) && (vident == NULL)) vident = "Unknown";

		vrefproc = asl_get(msg, ASL_KEY_REF_PROC);
		vrefpid = asl_get(msg, ASL_KEY_REF_PID);

		vmsg = asl_get(msg, ASL_KEY_MSG);
		if (vmsg != NULL) outmsg = bsd_log_string(vmsg);

		n = 0;
		/* Time + " " */
		n += (strlen(vt) + 1);

		/* Host + " " */
		if (vhost != NULL) n += (strlen(vhost) + 1);

		/* Sender */
		if (vident != NULL) n += strlen(vident);

		/* "[" PID "]" */
		if (vpid != NULL) n += (strlen(vpid) + 2);

		/* " (" */
		if ((vrefproc != NULL) || (vrefpid != NULL)) n += 2;

		/* RefProc */
		if (vrefproc != NULL) n += strlen(vrefproc);

		/* "[" RefPID "]" */
		if (vrefpid != NULL) n += (strlen(vrefpid) + 2);

		/* ")" */
		if ((vrefproc != NULL) || (vrefpid != NULL)) n += 1;

		/* ": " */
		n += 2;

		/* Message */
		if (outmsg != NULL) n += strlen(outmsg);

		if (n == 0) return -1;

		/* "\n" + nul */
		n += 2;

		so = calloc(1, n);
		if (so == NULL) return -1;

		strcat(so, vt);
		strcat(so, " ");

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

		if ((vrefproc != NULL) || (vrefpid != NULL))
		{
			strcat(so, " (");

			if (vrefproc != NULL) strcat(so, vrefproc);

			if (vrefpid != NULL)
			{
				strcat(so, "[");
				strcat(so, vrefpid);
				strcat(so, "]");
			}

			strcat(so, ")");
		}

		strcat(so, ": ");

		if (outmsg != NULL)
		{
			strcat(so, outmsg);
			free(outmsg);
		}

		strcat(so, "\n");

		*out = so;
	}

	/* check if message is a duplicate of the last message, and inside the dup time window */
	is_dup = 0;
	if ((global.bsd_max_dup_time > 0) && (*out != NULL) && (r->last_msg != NULL))
	{
		msg_hash = asl_core_string_hash(*out + 16, strlen(*out + 16));
		if ((r->last_hash == msg_hash) && (!strcmp(r->last_msg, *out + 16)))
		{
			if ((now - r->last_time) < global.bsd_max_dup_time) is_dup = 1;
		}
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

	_syslog_dst_open(r);

	if ((r->type == DST_TYPE_FILE) || (r->type == DST_TYPE_CONS))
	{
		/*
		 * If current message is NOT a duplicate and r->last_count > 0
		 * we need to write a "last message was repeated N times" log entry
		 */
		if ((r->type == DST_TYPE_FILE) && (is_dup == 0) && (r->last_count > 0)) _syslog_send_repeat_msg(r);

		do_write = 1;

		/*
		 * Special case for kernel messages.
		 * Don't write kernel messages to /dev/console.
		 * The kernel printf routine already sends them to /dev/console
		 * so writing them here would cause duplicates.
		 */
		vfacility = asl_get(msg, ASL_KEY_FACILITY);
		if ((vfacility != NULL) && (!strcmp(vfacility, FACILITY_KERNEL)) && (r->type == DST_TYPE_CONS)) do_write = 0;
		if ((do_write == 1) && (r->type == DST_TYPE_FILE) && (is_dup == 1)) do_write = 0;

		if (do_write == 0) status = outlen;
		else status = write(r->fd, *out, outlen);

		if ((status < 0) || (status < outlen))
		{
			asldebug("%s: error writing message (%s): %s\n", MY_ID, r->dst, strerror(errno));

			/* Try re-opening the file (once) and write again */
			close(r->fd);
			r->fd = open(r->dst, O_WRONLY | O_APPEND | O_CREAT | O_NOCTTY, 0644);
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
	else if ((r->type == DST_TYPE_SOCK) && (r->addr != NULL))
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

		fprintf(pw, "%s", *out);
		pclose(pw);
	}

	if (is_dup == 1)
	{
		r->last_count++;
	}
	else
	{
		if (r->last_msg != NULL) free(r->last_msg);
		r->last_msg = NULL;

		if (*out != NULL) r->last_msg = strdup(*out + 16);

		r->last_hash = msg_hash;
		r->last_count = 0;
		r->last_time = now;
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
	time_t tick;
	uint64_t delta;

	if (reset != RESET_NONE) _do_reset();

	if (msg == NULL) return -1;

	out = NULL;
	fwd = NULL;

	tick = time(NULL);
	global.bsd_flush_time = 0;

	for (r = bsd_out_rule.tqh_first; r != NULL; r = r->entries.tqe_next)
	{
		if (_syslog_rule_match(msg, r) == 1) _syslog_send(msg, r, &out, &fwd, tick);
		if ((r->type == DST_TYPE_FILE) && (r->last_count > 0))
		{
			delta = tick - r->last_time;
			if (delta < global.bsd_max_dup_time)
			{
				delta = global.bsd_max_dup_time - delta;
				if (global.bsd_flush_time == 0) global.bsd_flush_time = delta;
				else if (delta < global.bsd_flush_time) global.bsd_flush_time = delta;
			}
		}
	}

	if (out != NULL) free(out);
	if (fwd != NULL) free(fwd);

	return 0;
}

void
bsd_close_idle_files(time_t now)
{
	struct config_rule *r;
	uint64_t delta;

	for (r = bsd_out_rule.tqh_first; r != NULL; r = r->entries.tqe_next)
	{
		/* only applies to files */
		if (r->type != DST_TYPE_FILE) continue;

		/*
		 * If the last message repeat count is non-zero, a bsd_flush_duplicates()
		 * call will occur within 30 seconds.  Don't bother closing the file.
		 */
		if (r->last_count > 0) continue;

		delta = now - r->last_time;
		if (delta > CLOSE_ON_IDLE_SEC) _syslog_dst_close(r);
	}
}

void
bsd_flush_duplicates(time_t now)
{
	struct config_rule *r;
	uint64_t delta;

	global.bsd_flush_time = 0;

	for (r = bsd_out_rule.tqh_first; r != NULL; r = r->entries.tqe_next)
	{
		if (r->type != DST_TYPE_FILE) continue;

		if (r->last_count > 0)
		{
			delta = now - r->last_time;
			if (delta < global.bsd_max_dup_time)
			{
				delta = global.bsd_max_dup_time - delta;
				if (global.bsd_flush_time == 0) global.bsd_flush_time = delta;
				else if (delta < global.bsd_flush_time) global.bsd_flush_time = delta;
			}
			else
			{
				_syslog_dst_open(r);
				_syslog_send_repeat_msg(r);

				r->last_count = 0;
			}
		}
	}
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
	reset = global.reset;
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
		if (r->last_msg != NULL) free(r->last_msg);
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

int
bsd_out_network_reset(void)
{
	struct config_rule *r;

	for (r = bsd_out_rule.tqh_first; r != NULL; r = r->entries.tqe_next)
	{
		if (r->type == DST_TYPE_SOCK) 
		{
			close(r->fd);
			r->fd = -1;
		}
	}

	return 0;
}
