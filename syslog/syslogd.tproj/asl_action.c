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
#include <asl_store.h>
#include "daemon.h"

#define _PATH_ASL_CONF "/etc/asl.conf"
#define MY_ID "asl_action"

#define ASL_KEY_FACILITY "Facility"
#define IndexNull ((uint32_t)-1)
#define forever for(;;)

#define ACT_STORE_FLAG_STAY_OPEN     0x00000001
#define ACT_STORE_FLAG_EXCLUDE_ASLDB 0x00000002

static asl_msg_t *query = NULL;
static int reset = 0;

struct action_rule
{
	asl_msg_t *query;
	char *action;
	char *options;
	void *data;
	TAILQ_ENTRY(action_rule) entries;
};

struct store_data
{
	asl_store_t *store;
	char *path;
	uint32_t flags;
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

static char *
_next_word(char **s)
{
	char *a, *p, *e, *out;
	int quote, len;

	if (s == NULL) return NULL;
	if (*s == NULL) return NULL;

	quote = 0;

	p = *s;
	a = p;
	e = p;

	while (*p != '\0')
	{
		if (*p == '\\')
		{
			p++;
			e = p;

			if (*p == '\0')
			{
				p--;
				break;
			}

			p++;
			e = p;
			continue;
		}

		if (*p == '"')
		{
			if (quote == 0) quote = 1;
			else quote = 0;
		}

		if (((*p == ' ') || (*p == '\t')) && (quote == 0))
		{
			e = p + 1;
			break;
		}

		p++;
		e = p;
	}

	*s = e;

	len = p - a;
	if (len == 0) return NULL;

	out = malloc(len + 1);
	if (out == NULL) return NULL;

	memcpy(out, a, len);
	out[len] = '\0';
	return out;
}

static void 
_act_notify(struct action_rule *r)
{
	if (r == NULL) return;
	if (r->options == NULL) return;
	notify_post(r->options);
}

static void
_act_access_control(struct action_rule *r, asl_msg_t *msg)
{
	int32_t ruid, rgid;
	char *p;

	ruid = atoi(r->options);
	rgid = -1;
	p = strchr(r->options, ' ');
	if (p == NULL) p = strchr(r->options, '\t');
	if (p != NULL)
	{
		*p = '\0';
		p++;
		rgid = atoi(p);
	}

	if (ruid != -1) asl_set((aslmsg)msg, ASL_KEY_READ_UID, r->options);
	if (p != NULL)
	{
		if (rgid != -1) asl_set((aslmsg)msg, ASL_KEY_READ_GID, p);
		p--;
		*p = ' ';
	}
}

static void 
_act_store(struct action_rule *r, asl_msg_t *msg)
{
	struct store_data *sd;
	asl_store_t *s;
	char *p, *opts;
	uint32_t status;
	uint64_t msgid;

	if (r == NULL) return;
	if (r->options == NULL) return;
	if (r->data == NULL)
	{
		/* Set up store data */
		sd = (struct store_data *)calloc(1, sizeof(struct store_data));
		if (sd == NULL) return;

		opts = r->options;
		sd->store = NULL;
		sd->path = _next_word(&opts);
		if (sd->path == NULL)
		{
			free(sd);
			return;
		}

		sd->flags = 0;
		while (NULL != (p = _next_word(&opts)))
		{
			if (!strcmp(p, "stayopen")) sd->flags |= ACT_STORE_FLAG_STAY_OPEN;
			else if (!strcmp(p, "exclude_asldb")) sd->flags |= ACT_STORE_FLAG_EXCLUDE_ASLDB;
			free(p);
			p = NULL;
		}
	}
	else
	{
		sd = (struct store_data *)r->data;
	}

	if (sd->store == NULL)
	{
		s = NULL;
		status = asl_store_open(sd->path, 0, &s);
		if (status != ASL_STATUS_OK) return;
		if (s == NULL) return;
		sd->store = s;
	}

	asl_store_save(sd->store, msg, -1, -1, &msgid);
	if (!(sd->flags & ACT_STORE_FLAG_STAY_OPEN))
	{
		asl_store_close(sd->store);
		sd->store = NULL;
	}

	if (sd->flags & ACT_STORE_FLAG_EXCLUDE_ASLDB) asl_set(msg, ASL_KEY_IGNORE, "Yes");
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
			else if (!strcmp(r->action, "access")) _act_access_control(r, msg);
			else if (!strcmp(r->action, "notify")) _act_notify(r);
			else if (!strcmp(r->action, "store")) _act_store(r, msg);
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
	struct store_data *sd;

	n = NULL;
	for (r = asl_action_rule.tqh_first; r != NULL; r = n)
	{
		n = r->entries.tqe_next;

		if ((!strcmp(r->action, "store")) && (r->data != NULL))
		{
			sd = (struct store_data *)r->data;
			if (sd->store != NULL) asl_store_close(sd->store);
			if (sd->path != NULL) free(sd->path);
			free(r->data);
		}

		if (r->query != NULL) asl_free(r->query);
		if (r->action != NULL) free(r->action);
		if (r->options != NULL) free(r->options);

		TAILQ_REMOVE(&asl_action_rule, r, entries);
		free(r);
	}

	return 0;
}
