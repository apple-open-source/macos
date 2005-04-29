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

#define _PATH_ASL_CONF "/etc/asl.conf"
#define MY_ID "asl_action"

#define ASL_KEY_FACILITY "Facility"
#define IndexNull ((uint32_t)-1)
#define forever for(;;)

static asl_msg_t *query = NULL;
static int reset = 0;

struct action_rule
{
	asl_msg_t *query;
	char *action;
	char *options;
	TAILQ_ENTRY(action_rule) entries;
};

static TAILQ_HEAD(cr, action_rule) asl_action_rule;

int asl_action_close();
static int _parse_notify_file(const char *);

static void
_do_reset(void)
{
	asl_action_close();
	_parse_notify_file(_PATH_ASL_CONF);
}

/*
 * Config File format:
 * Q [k v] [k v] ... action args...
 */

/* Skip over query */
static char *
_find_action(char *s)
{
	char *p;

	p = s;
	if (p == NULL) return NULL;
	if (*p != 'Q') return NULL;

	p++;

	forever
	{
		/* Find next [ */
		while ((*p == ' ') || (*p == '\t')) p++;

		if (*p == '\0') return NULL;
		if (*p != '[') return p;

		/* skip to closing ] */
		while (*p != ']')
		{
			p++;
			if (*p == '\\')
			{
				p++;
				if (*p == ']') p++;
			}
		}

		if (*p == ']') p++;
	}

	return NULL;
}

static int
_parse_line(char *s)
{
	char *act, *p;
	struct action_rule *out;

	if (s == NULL) return -1;
	while ((*s == ' ') || (*s == '\t')) s++;
	if (*s == '#') return -1;

	act = _find_action(s);

	if (act == NULL) return -1;
	out = (struct action_rule *)calloc(1, sizeof(struct action_rule));
	if (out == NULL) return -1;

	p = strchr(act, ' ');
	if (p != NULL) *p = '\0';
	out->action = strdup(act);

	if (out->action == NULL)
	{
		free(out);
		return -1;
	}

	if (p != NULL)
	{
		out->options = strdup(p+1);

		if (out->options == NULL)
		{
			free(out->action);
			free(out);
			return -1;
		}
	}

	p = act - 1;

	*p = '\0';
	out->query = asl_msg_from_string(s);

	if (out->query == NULL)
	{
		free(out->action);
		if (out->options != NULL) free(out->options);
		free(out);
		return -1;
	}

	TAILQ_INSERT_TAIL(&asl_action_rule, out, entries);

	return 0;
}

static void 
_act_notify(struct action_rule *r)
{
	if (r == NULL) return;
	if (r->options == NULL) return;
	notify_post(r->options);
}

int
asl_action_sendmsg(asl_msg_t *msg, const char *outid)
{
	struct action_rule *r;


	if (reset != 0)
	{
		_do_reset();
		reset = 0;
	}

	if (msg == NULL) return -1;

	for (r = asl_action_rule.tqh_first; r != NULL; r = r->entries.tqe_next)
	{
		if (asl_msg_cmp(r->query, msg) == 1)
		{
			if (r->action == NULL) continue;
			if (!strcmp(r->action, "notify")) _act_notify(r);
		}
	}

	return 0;
}

static int
_parse_notify_file(const char *name)
{
	FILE *cf;
	char *line;

	cf = fopen(name, "r");
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
asl_action_init(void)
{
	asldebug("%s: init\n", MY_ID);

	TAILQ_INIT(&asl_action_rule);

	query = asl_new(ASL_TYPE_QUERY);
	aslevent_addmatch(query, MY_ID);
	aslevent_addoutput(asl_action_sendmsg, MY_ID);

	_parse_notify_file(_PATH_ASL_CONF);
	return 0;
}

int
asl_action_reset(void)
{
	reset = 1;
	return 0;
}

int
asl_action_close(void)
{
	struct action_rule *r, *n;

	n = NULL;
	for (r = asl_action_rule.tqh_first; r != NULL; r = n)
	{
		n = r->entries.tqe_next;

		if (r->query != NULL) asl_free(r->query);
		if (r->action != NULL) free(r->action);
		if (r->options != NULL) free(r->options);

		TAILQ_REMOVE(&asl_action_rule, r, entries);
		free(r);
	}

	return 0;
}
