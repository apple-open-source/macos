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
#include <pthread.h>
#include <sys/acl.h>
#include <membership.h>
#include "daemon.h"
#include <dispatch/private.h>

#define _PATH_WALL "/usr/bin/wall"
#define _PATH_ASL_CONF "/etc/asl.conf"
#define MY_ID "asl_action"

#define MAX_FAILURES 5

#define ACTION_NONE      0
#define ACTION_IGNORE    1
#define ACTION_NOTIFY    2
#define ACTION_BROADCAST 3
#define ACTION_ACCESS    4
#define ACTION_ASL_STORE 5 /* Save in main ASL Database */
#define ACTION_ASL_FILE  6 /* Save in an ASL format data file */
#define ACTION_ASL_DIR   7 /* Save in an ASL directory */
#define ACTION_FILE      8
#define ACTION_FORWARD   9

#define forever for(;;)

#define ACT_FLAG_HAS_LOGGED   0x80000000
#define ACT_FLAG_CLEAR_LOGGED 0x7fffffff

#define ACT_STORE_FLAG_STAY_OPEN 0x00000001
#define ACT_STORE_FLAG_CONTINUE  0x00000002

#define ACT_FILE_FLAG_DUP_SUPRESS 0x00000001
#define ACT_FILE_FLAG_ROTATE      0x00000002

static dispatch_queue_t asl_action_queue;
static time_t last_file_day;

typedef struct action_rule_s
{
	asl_msg_t *query;
	int action;
	char *options;
	void *data;
	struct action_rule_s *next;
} action_rule_t;

struct store_data
{
	asl_file_t *store;
	FILE *storedata;
	char *dir;
	char *path;
	mode_t mode;
	uid_t uid;
	gid_t gid;
	uint64_t next_id;
	uint32_t fails;
	uint32_t flags;
	uint32_t refcount;
	uint32_t p_year;
	uint32_t p_month;
	uint32_t p_day;
};

struct file_data
{
	int fd;
	char *path;
	char *fmt;
	const char *tfmt;
	mode_t mode;
	uid_t *uid;
	uint32_t nuid;
	gid_t *gid;
	uint32_t ngid;
	size_t max_size;
	uint32_t fails;
	uint32_t flags;
	uint32_t refcount;
	time_t stamp;
	uint32_t last_hash;
	uint32_t last_count;
	time_t last_time;
	dispatch_source_t dup_timer;
	char *last_msg;
};

static action_rule_t *asl_action_rule = NULL;
static action_rule_t *asl_datastore_rule = NULL;

static int _parse_config_file(const char *);
extern void db_save_message(aslmsg m);

/* forward */
int _act_file_open(struct file_data *fdata);
static void _act_file_init(action_rule_t *r);
static void _act_store_init(action_rule_t *r);

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

/*
 * Config File format:
 * Set parameter rule - initializes a parameter.
 *		= param args...
 * Query rule - if a message matches the query, then the action is invoked.
 * The rule may be identified by either "?" or "Q".
 *		? [k v] [k v] ... action args...
 *		Q [k v] [k v] ... action args...
 * Universal match rule - the action is invoked for all messages
 *		* action args...
 */

/* Skip over query */
static char *
_find_action(char *s)
{
	char *p;

	p = s;
	if (p == NULL) return NULL;
	if ((*p != 'Q') && (*p != '?') && (*p != '*')) return NULL;

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
_parse_query_action(char *s)
{
	char *act, *p;
	action_rule_t *out, *rule;

	act = _find_action(s);
	if (act == NULL) return -1;

	out = (action_rule_t *)calloc(1, sizeof(action_rule_t));
	if (out == NULL) return -1;

	p = strchr(act, ' ');
	if (p != NULL) *p = '\0';

	if (!strcasecmp(act, "ignore"))               out->action = ACTION_IGNORE;
	else if (!strcasecmp(act, "notify"))          out->action = ACTION_NOTIFY;
	else if (!strcasecmp(act, "broadcast"))       out->action = ACTION_BROADCAST;
	else if (!strcasecmp(act, "access"))          out->action = ACTION_ACCESS;
	else if (!strcasecmp(act, "store"))           out->action = ACTION_ASL_STORE;
	else if (!strcasecmp(act, "save"))            out->action = ACTION_ASL_STORE;
	else if (!strcasecmp(act, "store_file"))      out->action = ACTION_ASL_FILE;
	else if (!strcasecmp(act, "store_directory")) out->action = ACTION_ASL_DIR;
	else if (!strcasecmp(act, "store_dir"))       out->action = ACTION_ASL_DIR;
	else if (!strcasecmp(act, "file"))            out->action = ACTION_FILE;
	else if (!strcasecmp(act, "forward"))         out->action = ACTION_FORWARD;

	if (p != NULL)
	{
		out->options = strdup(p+1);

		if (out->options == NULL)
		{
			free(out);
			return -1;
		}
	}

	p = act - 1;

	*p = '\0';

	if (s[0] == '*') out->query = asl_msg_new(ASL_TYPE_QUERY);
	else
	{
		s[0] = 'Q';
		out->query = asl_msg_from_string(s);
	}

	if (out->query == NULL)
	{
		asldebug("out->query is NULL (ERROR)\n");
		free(out->options);
		free(out);
		return -1;
	}

	/* store /some/path means save to a file */
	if ((out->action == ACTION_ASL_STORE) && (out->options != NULL)) out->action = ACTION_ASL_FILE;

	if (out->action == ACTION_FILE) _act_file_init(out);
	else if ((out->action == ACTION_ASL_FILE) || (out->action == ACTION_ASL_DIR)) _act_store_init(out);

	if (out->action == ACTION_ASL_STORE)
	{
		asldebug("action = ACTION_ASL_STORE\n");
		if (asl_datastore_rule == NULL) asl_datastore_rule = out;
		else
		{
			for (rule = asl_datastore_rule; rule->next != NULL; rule = rule->next);
			rule->next = out;
		}
	}
	else
	{
		asldebug("action = %d options = %s\n", out->action, out->options);
		if (asl_action_rule == NULL) asl_action_rule = out;
		else
		{
			for (rule = asl_action_rule; rule->next != NULL; rule = rule->next);
			rule->next = out;
		}
	}

	return 0;
}

static int
_parse_line(char *s)
{
	char *str;
	int status;

	if (s == NULL) return -1;
	while ((*s == ' ') || (*s == '\t')) s++;

	/* First non-whitespace char is the rule type */
	switch (*s)
	{
		case '\0':
		case '#':
		{
			/* Blank Line or Comment */
			return 0;
		}
		case 'Q':
		case '?':
		case '*':
		{
			/* Query-match action */
			status = _parse_query_action(s);
			break;
		}
		case '=':
		{
			/* Set parameter */
			status = control_set_param(s);
			break;
		}
		default:
		{
			status = -1;
			break;
		}
	}

	if (status != 0)
	{
		str = NULL;
		asprintf(&str, "[%s syslogd] [%s %u] [%s %u] [%s Ignoring unrecognized entry in %s: %s] [%s 0] [%s 0] [Facility syslog]",
				 ASL_KEY_SENDER,
				 ASL_KEY_LEVEL, ASL_LEVEL_ERR,
				 ASL_KEY_PID, getpid(),
				 ASL_KEY_MSG, _PATH_ASL_CONF, s,
				 ASL_KEY_UID, ASL_KEY_GID);

		internal_log_message(str);
		free(str);
	}

	return status;
}

static void
_act_notify(action_rule_t *r)
{
	if (r == NULL) return;
	if (r->options == NULL) return;

	notify_post(r->options);
}

static void
_act_broadcast(action_rule_t *r, aslmsg msg)
{
#ifndef CONFIG_IPHONE
	FILE *pw;
	const char *val;

	if (r == NULL) return;
	if (msg == NULL) return;

	val = r->options;
	if (val == NULL) val = asl_get(msg, ASL_KEY_MSG);
	if (val == NULL) return;

	pw = popen(_PATH_WALL, "w");
	if (pw < 0)
	{
		asldebug("%s: error sending wall message: %s\n", MY_ID, strerror(errno));
		return;
	}

	fprintf(pw, "%s", val);
	pclose(pw);
#endif
}

static void
_act_access_control(action_rule_t *r, aslmsg msg)
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

	if (ruid != -1) asl_set(msg, ASL_KEY_READ_UID, r->options);
	if (p != NULL)
	{
		if (rgid != -1) asl_set(msg, ASL_KEY_READ_GID, p);
		p--;
		*p = ' ';
	}
}

static uint32_t
_act_store_file_setup(struct store_data *sd)
{
	uint32_t status;

	if (sd == NULL) return ASL_STATUS_INVALID_STORE;
	if (sd->store == NULL) return ASL_STATUS_INVALID_STORE;
	if (sd->store->store == NULL) return ASL_STATUS_INVALID_STORE;

	status = asl_file_read_set_position(sd->store, ASL_FILE_POSITION_LAST);
	if (status != ASL_STATUS_OK) return status;

	sd->next_id = sd->store->cursor_xid + 1;
	if (fseek(sd->store->store, 0, SEEK_END) != 0) return ASL_STATUS_ACCESS_DENIED;

	return ASL_STATUS_OK;
}

static uint32_t
_act_store_dir_setup(struct store_data *sd, time_t tick)
{
	struct tm ctm;
	char *path;
	struct stat sb;
	uint64_t xid;
	int status;
	mode_t mask;

	if (sd == NULL) return ASL_STATUS_INVALID_STORE;
	if (sd->dir == NULL) return ASL_STATUS_INVALID_STORE;

	/* get / set message id from StoreData file */
	xid = 0;

	if (sd->storedata == NULL)
	{
		memset(&sb, 0, sizeof(struct stat));
		status = stat(sd->dir, &sb);
		if (status == 0)
		{
			/* must be a directory */
			if (!S_ISDIR(sb.st_mode)) return ASL_STATUS_INVALID_STORE;
		}
		else if (errno == ENOENT)
		{
			/* doesn't exist - create it */
			mask = umask(0);
			status = mkdir(sd->dir, sd->mode);
			umask(mask);

			if (status != 0) return ASL_STATUS_WRITE_FAILED;
			if (chown(sd->dir, sd->uid, sd->gid) != 0) return ASL_STATUS_WRITE_FAILED;
		}
		else
		{
			/* Unexpected stat error */
			return ASL_STATUS_FAILED;
		}

		path = NULL;
		asprintf(&path, "%s/%s", sd->dir, FILE_ASL_STORE_DATA);
		if (path == NULL) return ASL_STATUS_NO_MEMORY;

		memset(&sb, 0, sizeof(struct stat));
		status = stat(path, &sb);
		if (status == 0)
		{
			/* StoreData exists: open and read last xid */
			sd->storedata = fopen(path, "r+");
			if (sd->storedata == NULL)
			{
				free(path);
				return ASL_STATUS_FAILED;
			}

			if (fread(&xid, sizeof(uint64_t), 1, sd->storedata) != 1)
			{
				free(path);
				fclose(sd->storedata);
				sd->storedata = NULL;
				return ASL_STATUS_READ_FAILED;
			}
		}
		else if (errno == ENOENT)
		{
			/* StoreData does not exist: create it */
			sd->storedata = fopen(path, "w");
			if (sd->storedata == NULL)
			{
				free(path);
				return ASL_STATUS_FAILED;
			}

			if (chown(path, sd->uid, sd->gid) != 0)
			{
				free(path);
				return ASL_STATUS_WRITE_FAILED;
			}
		}
		else
		{
			/* Unexpected stat error */
			free(path);
			return ASL_STATUS_FAILED;
		}

		free(path);
	}
	else
	{
		rewind(sd->storedata);
		if (fread(&xid, sizeof(uint64_t), 1, sd->storedata) != 1)
		{
			fclose(sd->storedata);
			sd->storedata = NULL;
			return ASL_STATUS_READ_FAILED;
		}
	}

	xid = asl_core_ntohq(xid);
	xid++;
	sd->next_id = xid;

	xid = asl_core_htonq(xid);
	rewind(sd->storedata);
	status = fwrite(&xid, sizeof(uint64_t), 1, sd->storedata);
	if (status != 1)
	{
		fclose(sd->storedata);
		sd->storedata = NULL;
		return ASL_STATUS_WRITE_FAILED;
	}

	if ((sd->flags & ACT_STORE_FLAG_STAY_OPEN) == 0)
	{
		fclose(sd->storedata);
		sd->storedata = NULL;
	}

	memset(&ctm, 0, sizeof(struct tm));

	if (localtime_r((const time_t *)&tick, &ctm) == NULL) return ASL_STATUS_FAILED;
	if ((sd->p_year == ctm.tm_year) && (sd->p_month == ctm.tm_mon) && (sd->p_day == ctm.tm_mday) && (sd->path != NULL)) return ASL_STATUS_OK;

	if (sd->store != NULL) asl_file_close(sd->store);
	sd->store = NULL;

	sd->p_year = 0;
	sd->p_month = 0;
	sd->p_day = 0;

	free(sd->path);
	sd->path = NULL;

	asprintf(&(sd->path), "%s/%d.%02d.%02d.asl", sd->dir, ctm.tm_year + 1900, ctm.tm_mon + 1, ctm.tm_mday);
	if (sd->path == NULL) return ASL_STATUS_NO_MEMORY;

	sd->p_year = ctm.tm_year;
	sd->p_month = ctm.tm_mon;
	sd->p_day = ctm.tm_mday;

	return ASL_STATUS_OK;
}

static void
_act_store_init(action_rule_t *r)
{
	struct store_data *sd, *xd;
	char *str, *opts, *p, *path;
	action_rule_t *x;

	/* check if the store data is already set up */
	if (r->data != NULL) return;

	opts = r->options;
	path = _next_word(&opts);

	if ((path == NULL) || (path[0] != '/'))
	{
		str = NULL;
		asprintf(&str, "[%s syslogd] [%s %u] [%s %u] [Facility syslog] [%s Invalid path for \"%s\" action: %s]",
				 ASL_KEY_SENDER,
				 ASL_KEY_LEVEL, ASL_LEVEL_ERR,
				 ASL_KEY_PID, getpid(),
				 ASL_KEY_MSG,
				 (r->action == ACTION_ASL_FILE) ? "store" : "store_directory",
				 (path == NULL) ? "no path specified" : path);

		internal_log_message(str);
		free(str);
		r->action = ACTION_NONE;
		free(path);
		return;
	}

	/* check if a previous rule has set up this path (ACTION_ASL_FILE) or dir (ACTION_ASL_DIR) */
	for (x = asl_action_rule; x != NULL; x = x->next)
	{
		if ((x->action == r->action) && (x->data != NULL))
		{
			xd = (struct store_data *)x->data;
			p = xd->path;
			if (r->action == ACTION_ASL_DIR) p = xd->dir;

			if ((p != NULL) && (!strcmp(path, p)))
			{
				free(path);
				xd->refcount++;
				r->data = x->data;
				return;
			}
		}
	}

	/* set up store data */
	sd = (struct store_data *)calloc(1, sizeof(struct store_data));
	if (sd == NULL) return;

	sd->refcount = 1;
	sd->mode = 0755;
	sd->next_id = 0;
	sd->uid = 0;
	sd->gid = 0;
	sd->flags = 0;

	if (r->action == ACTION_ASL_DIR) sd->dir = path;
	else sd->path = path;

	while (NULL != (p = _next_word(&opts)))
	{
		if (!strcmp(p, "stayopen"))
		{
			sd->flags |= ACT_STORE_FLAG_STAY_OPEN;
		}
		else if (!strcmp(p, "continue"))
		{
			sd->flags |= ACT_STORE_FLAG_CONTINUE;
		}
		else if (!strncmp(p, "mode=", 5)) sd->mode = strtol(p+5, NULL, 0);
		else if (!strncmp(p, "uid=", 4)) sd->uid = atoi(p+4);
		else if (!strncmp(p, "gid=", 4)) sd->gid = atoi(p+4);

		free(p);
		p = NULL;
	}

	r->data = sd;
}

/*
 * Save a message to an ASL format file (ACTION_ASL_FILE)
 * or to an ASL directory (ACTION_ASL_DIR).
 */
static void
_act_store(action_rule_t *r, aslmsg msg)
{
	struct store_data *sd;
	asl_file_t *s;
	uint32_t status;
	uint64_t mid;
	mode_t mask;
	char *str, *opts;
	const char *val;
	time_t tick;

	s = NULL;

	if (r->data == NULL) return;

	sd = (struct store_data *)r->data;

	if (sd->flags & ACT_FLAG_HAS_LOGGED) return;
	sd->flags |= ACT_FLAG_HAS_LOGGED;

	if (r->action == ACTION_ASL_DIR)
	{
		val = asl_get(msg, ASL_KEY_TIME);
		if (val == NULL) return;

		tick = atol(val);
		status = _act_store_dir_setup(sd, tick);
		if (status != ASL_STATUS_OK)
		{
			asldebug("_act_store_dir_setup %s failed: %s\n", sd->path, asl_core_error(status));

			sd->fails++;

			/* disable further activity after multiple failures */
			if (sd->fails > MAX_FAILURES)
			{
				char *str = NULL;
				asprintf(&str, "[%s syslogd] [%s %u] [%s %u] [Facility syslog] [%s Disabling writes to path %s following %u failures (%s)]",
						 ASL_KEY_SENDER,
						 ASL_KEY_LEVEL, ASL_LEVEL_ERR,
						 ASL_KEY_PID, getpid(),
						 ASL_KEY_MSG, sd->path, sd->fails, asl_core_error(status));

				internal_log_message(str);
				free(str);

				asl_file_close(sd->store);
				sd->store = NULL;
				r->action = ACTION_NONE;
				return;
			}
		}
		else
		{
			sd->fails = 0;
		}
	}

	if (sd->store == NULL)
	{
		s = NULL;

		mask = umask(0);
		status = asl_file_open_write(sd->path, (sd->mode & 0666), sd->uid, sd->gid, &s);
		umask(mask);

		if ((status != ASL_STATUS_OK) || (s == NULL))
		{
			asldebug("asl_file_open_write %s failed: %s\n", sd->path, asl_core_error(status));

			sd->fails++;

			/* disable further activity after multiple failures */
			if (sd->fails > MAX_FAILURES)
			{
				char *str = NULL;
				asprintf(&str, "[%s syslogd] [%s %u] [%s %u] [Facility syslog] [%s Disabling writes to path %s following %u failures (%s)]",
						 ASL_KEY_SENDER,
						 ASL_KEY_LEVEL, ASL_LEVEL_ERR,
						 ASL_KEY_PID, getpid(),
						 ASL_KEY_MSG, sd->path, sd->fails, asl_core_error(status));

				internal_log_message(str);
				free(str);

				asl_file_close(sd->store);
				sd->store = NULL;
				r->action = ACTION_NONE;
				return;
			}
		}
		else if (status == ASL_STATUS_OK)
		{
			sd->fails = 0;
		}

		sd->store = s;
	}

	if (r->action != ACTION_ASL_DIR)
	{
		status = _act_store_file_setup(sd);
		if (status != ASL_STATUS_OK)
		{
			asldebug("_act_store_file_setup %s failed: %s\n", sd->path, asl_core_error(status));

			sd->fails++;

			/* disable further activity after multiple failures */
			if (sd->fails > MAX_FAILURES)
			{
				char *str = NULL;
				asprintf(&str, "[%s syslogd] [%s %u] [%s %u] [Facility syslog] [%s Disabling writes to path %s following %u failures (%s)]",
						 ASL_KEY_SENDER,
						 ASL_KEY_LEVEL, ASL_LEVEL_ERR,
						 ASL_KEY_PID, getpid(),
						 ASL_KEY_MSG, sd->path, sd->fails, asl_core_error(status));

				internal_log_message(str);
				free(str);

				asl_file_close(sd->store);
				sd->store = NULL;
				r->action = ACTION_NONE;
				return;
			}
		}
		else
		{
			sd->fails = 0;
		}
	}

	mid = sd->next_id;

	status = asl_file_save(sd->store, msg, &mid);
	if (status != ASL_STATUS_OK)
	{
		asldebug("asl_file_save %s failed: %s\n", sd->path, asl_core_error(status));

		sd->fails++;

		/* disable further activity after multiple failures */
		if (sd->fails > MAX_FAILURES)
		{
			char *str = NULL;
			asprintf(&str, "[%s syslogd] [%s %u] [%s %u] [Facility syslog] [%s Disabling writes to path %s following %u failures (%s)]",
					 ASL_KEY_SENDER,
					 ASL_KEY_LEVEL, ASL_LEVEL_ERR,
					 ASL_KEY_PID, getpid(),
					 ASL_KEY_MSG, sd->path, sd->fails, asl_core_error(status));

			internal_log_message(str);
			free(str);

			asl_file_close(sd->store);
			sd->store = NULL;
			r->action = ACTION_NONE;
			return;
		}
	}
	else
	{
		sd->fails = 0;
	}

	if ((sd->flags & ACT_STORE_FLAG_STAY_OPEN) == 0)
	{
		asl_file_close(sd->store);
		sd->store = NULL;
	}

	if ((sd->flags & ACT_STORE_FLAG_CONTINUE) == 0)
	{
		opts = (char *)asl_get(msg, ASL_KEY_OPTION);
		if (opts == NULL)
		{
			asl_set(msg, ASL_KEY_OPTION, ASL_OPT_IGNORE);
		}
		else
		{
			str = NULL;
			asprintf(&str, "%s %s", ASL_OPT_IGNORE, opts);
			if (str != NULL)
			{
				asl_set(msg, ASL_KEY_OPTION, str);
				free(str);
			}
		}
	}
}

static int
_act_file_send_repeat_msg(struct file_data *fdata)
{
	char vt[32], *msg;
	int len, status, closeit;
	time_t now = time(NULL);

	if (fdata == NULL) return -1;

	free(fdata->last_msg);
	fdata->last_msg = NULL;

	if (fdata->last_count == 0) return 0;

	/* stop the timer */
	dispatch_suspend(fdata->dup_timer);

	memset(vt, 0, sizeof(vt));
	ctime_r(&now, vt);
	vt[19] = '\0';

	msg = NULL;
	asprintf(&msg, "%s --- last message repeated %u time%s ---\n", vt + 4, fdata->last_count, (fdata->last_count == 1) ? "" : "s");
	fdata->last_count = 0;
	if (msg == NULL) return -1;

	closeit = 0;
	if (fdata->fd < 0)
	{
		closeit = 1;
		fdata->fd = _act_file_open(fdata);
		if (fdata->fd < 0)
		{
			asldebug("%s: error opening for repeat message (%s): %s\n", MY_ID, fdata->path, strerror(errno));
			return -1;
		}
	}

	len = strlen(msg);
	status = write(fdata->fd, msg, len);
	free(msg);
	if (closeit != 0)
	{
		close(fdata->fd);
		fdata->fd = -1;
	}

	if ((status < 0) || (status < len))
	{
		asldebug("%s: error writing repeat message (%s): %s\n", MY_ID, fdata->path, strerror(errno));
		return -1;
	}

	return 0;
}

/*
 * N.B. This is basic file rotation support.
 * More rotation options will be added in the future, along
 * with support in aslmanager for compression and deletion.
 */
static void
_act_file_rotate_file_data(struct file_data *fdata, time_t now)
{
	char str[MAXPATHLEN];
	size_t len;
	int width;

	if (now == 0) now = time(NULL);

	/* flush duplicates if pending */
	_act_file_send_repeat_msg(fdata);

	/* sleep to prevent a sub-second rotation */
	while (now == fdata->stamp)
	{
		sleep(1);
		now = time(NULL);
	}

	len = strlen(fdata->path);
	width = len - 4;
	if ((len > 4) && (!strcasecmp(fdata->path + width, ".log")))
	{
		/* ".log" rename: abc.log -> abc.timestamp.log */
		snprintf(str, sizeof(str), "%.*s.%lu.log", width, fdata->path, fdata->stamp);
	}
	else
	{
		snprintf(str, sizeof(str), "%s.%lu", fdata->path, fdata->stamp);
	}

	rename(fdata->path, str);

	fdata->stamp = now;
}

int
_act_file_open(struct file_data *fdata)
{
	acl_t acl;
	uuid_t uuid;
	acl_entry_t entry;
	acl_permset_t perms;
	int status;
	int fd = -1;
	mode_t mask;
	struct stat sb;
	uint32_t i;

	memset(&sb, 0, sizeof(struct stat));
	status = stat(fdata->path, &sb);
	if (status == 0)
	{
		/* must be a regular file */
		if (!S_ISREG(sb.st_mode)) return -1;

		/* use st_birthtimespec if stamp is zero */
		if (fdata->stamp == 0) fdata->stamp = sb.st_birthtimespec.tv_sec;

		/* rotate if over size limit */
		if ((fdata->max_size > 0) && (sb.st_size > fdata->max_size))
		{
			_act_file_rotate_file_data(fdata, 0);
		}
		else
		{
			/* open existing file */
			fd = open(fdata->path, O_RDWR | O_APPEND | O_EXCL, 0);
			return fd;
		}
	}
	else if (errno != ENOENT)
	{
		return -1;
	}

#if TARGET_OS_EMBEDDED
	return open(fdata->path, O_RDWR | O_CREAT | O_EXCL, (fdata->mode & 0666));
#else

	acl = acl_init(1);

	for (i = 0; i < fdata->ngid; i++)
	{
		status = mbr_gid_to_uuid(fdata->gid[i], uuid);
		if (status != 0)
		{
			char *str = NULL;
			asprintf(&str, "[%s syslogd] [%s %u] [%s %u] [Facility syslog] [%s Unknown GID %d for \"file\" action: %s]",
					 ASL_KEY_SENDER,
					 ASL_KEY_LEVEL, ASL_LEVEL_ERR,
					 ASL_KEY_PID, getpid(),
					 ASL_KEY_MSG, fdata->gid[i], fdata->path);

			internal_log_message(str);
			free(str);
			continue;
		}

		status = acl_create_entry_np(&acl, &entry, ACL_FIRST_ENTRY);
		if (status != 0) goto asl_file_create_return;

		status = acl_set_tag_type(entry, ACL_EXTENDED_ALLOW);
		if (status != 0) goto asl_file_create_return;

		status = acl_set_qualifier(entry, &uuid);
		if (status != 0) goto asl_file_create_return;

		status = acl_get_permset(entry, &perms);
		if (status != 0) goto asl_file_create_return;

		status = acl_add_perm(perms, ACL_READ_DATA);
		if (status != 0) goto asl_file_create_return;
	}

	for (i = 0; i < fdata->nuid; i++)
	{
		status = mbr_uid_to_uuid(fdata->uid[i], uuid);
		if (status != 0)
		{
			char *str = NULL;
			asprintf(&str, "[%s syslogd] [%s %u] [%s %u] [Facility syslog] [%s Unknown UID %d for \"file\" action: %s]",
					 ASL_KEY_SENDER,
					 ASL_KEY_LEVEL, ASL_LEVEL_ERR,
					 ASL_KEY_PID, getpid(),
					 ASL_KEY_MSG, fdata->uid[i], fdata->path);

			internal_log_message(str);
			free(str);
			continue;
		}

		status = acl_create_entry_np(&acl, &entry, ACL_FIRST_ENTRY);
		if (status != 0) goto asl_file_create_return;

		status = acl_set_tag_type(entry, ACL_EXTENDED_ALLOW);
		if (status != 0) goto asl_file_create_return;

		status = acl_set_qualifier(entry, &uuid);
		if (status != 0) goto asl_file_create_return;

		status = acl_get_permset(entry, &perms);
		if (status != 0) goto asl_file_create_return;

		status = acl_add_perm(perms, ACL_READ_DATA);
		if (status != 0) goto asl_file_create_return;
	}

	mask = umask(0);
	fd = open(fdata->path, O_RDWR | O_CREAT | O_EXCL, (fdata->mode & 0666));
	umask(mask);
	if (fd < 0) goto asl_file_create_return;

	errno = 0;
	status = acl_set_fd(fd, acl);

	if (status != 0)
	{
		close(fd);
		fd = -1;
		unlink(fdata->path);
	}

asl_file_create_return:

	acl_free(acl);
	return fd;
#endif
}

static void
_act_file_rotate(const char *path)
{
	action_rule_t *r;
	struct file_data *fdata;
	time_t now = time(NULL);

	for (r = asl_action_rule; r != NULL; r = r->next)
	{
		if (r->action == ACTION_FILE)
		{
			fdata = (struct file_data *)r->data;
			if (fdata->flags & ACT_FILE_FLAG_ROTATE)
			{
				if ((path == NULL) || ((fdata->path != NULL) && !strcmp(fdata->path, path)))
				{
					_act_file_rotate_file_data(fdata, now);
				}
			}
		}
	}
}

static char *
_act_file_format_string(char *s)
{
	char *fmt;
	size_t i, len, n;

	if (s == NULL) return NULL;

	len = strlen(s);
	n = 0;
	for (i = 0; i < len; i++) if (s[i] == '\\') n++;

	fmt = malloc(1 + len - n);
	if (fmt == NULL) return NULL;

	for (i = 0, n = 0; i < len; i++) if (s[i] != '\\') fmt[n++] = s[i];
	fmt[n] = '\0';
	return fmt;
}

static size_t
_act_file_max_size(char *s)
{
	size_t len, n, max;
	char x;

	if (s == NULL) return 0;

	len = strlen(s);
	if (len == 0) return 0;

	n = 1;
	x = s[len - 1];
	if (x > 90) x -= 32;
	if (x == 'K') n = 1ll << 10;
	else if (x == 'M') n = 1ll << 20;
	else if (x == 'G') n = 1ll << 30;
	else if (x == 'T') n = 1ll << 40;

	max = atoll(s) * n;
	return max;
}

static void
_act_file_add_uid(struct file_data *fdata, char *s)
{
	if (fdata == NULL) return;
	if (s == NULL) return;

	fdata->uid = reallocf(fdata->uid, (fdata->nuid + 1) * sizeof(uid_t));
	if (fdata->uid == NULL)
	{
		fdata->nuid = 0;
		return;
	}

	fdata->uid[fdata->nuid++] = atoi(s);
}

static void
_act_file_add_gid(struct file_data *fdata, char *s)
{
	if (fdata == NULL) return;
	if (s == NULL) return;

	fdata->gid = reallocf(fdata->gid, (fdata->ngid + 1) * sizeof(gid_t));
	if (fdata->gid == NULL)
	{
		fdata->ngid = 0;
		return;
	}

	fdata->gid[fdata->ngid++] = atoi(s);
}

static void
_act_file_init(action_rule_t *r)
{
	struct file_data *fdata, *xdata;
	char *str, *opts, *p, *path;
	action_rule_t *x;

	/* check if the file data is already set up */
	if (r->data != NULL) return;

	/* requires at least a path */
	if (r->options == NULL) return;
	opts = r->options;
	path = _next_word(&opts);

	if ((path == NULL) || (path[0] != '/'))
	{
		str = NULL;
		asprintf(&str, "[%s syslogd] [%s %u] [%s %u] [Facility syslog] [%s Invalid path for \"file\" action: %s]",
				 ASL_KEY_SENDER,
				 ASL_KEY_LEVEL, ASL_LEVEL_ERR,
				 ASL_KEY_PID, getpid(),
				 ASL_KEY_MSG, (path == NULL) ? "no path specified" : path);

		internal_log_message(str);
		free(str);
		free(path);
		r->action = ACTION_NONE;
		return;
	}

	/* check if a previous rule has set up this path */
	for (x = asl_action_rule; x != NULL; x = x->next)
	{
		if ((x->action == ACTION_FILE) && (x->data != NULL))
		{
			xdata = (struct file_data *)x->data;
			if ((xdata->path != NULL) && (!strcmp(path, xdata->path)))
			{
				free(path);
				xdata->refcount++;
				r->data = x->data;
				return;
			}
		}
	}

	/* set up file data */
	fdata = (struct file_data *)calloc(1, sizeof(struct file_data));
	if (fdata == NULL) return;

	fdata->refcount = 1;
	fdata->path = path;

	/*
	 * options:
	 * mode=			set file creation mode
	 * uid=				user added to read ACL
	 * gid=				group added to read ACL
	 * format=			format string (also fmt=)
	 * no_dup_supress	no duplicate supression
	 *
	 * rotate			automatic daily rotation
	 *					this is basic rotation - more support is TBD
	 */
	fdata->mode = 0644;
	fdata->flags = ACT_FILE_FLAG_DUP_SUPRESS;

	while (NULL != (p = _next_word(&opts)))
	{
		if (!strncmp(p, "mode=", 5)) fdata->mode = strtol(p+5, NULL, 0);
		else if (!strncmp(p, "uid=", 4)) _act_file_add_uid(fdata, p+4);
		else if (!strncmp(p, "gid=", 4)) _act_file_add_gid(fdata, p+4);
		else if (!strncmp(p, "fmt=", 4)) fdata->fmt = _act_file_format_string(p+4);
		else if (!strncmp(p, "format=", 7)) fdata->fmt = _act_file_format_string(p+7);
		else if (!strncmp(p, "no_dup_supress", 14)) fdata->flags &= ~ACT_FILE_FLAG_DUP_SUPRESS;
		else if (!strncmp(p, "rotate", 6)) fdata->flags |= ACT_FILE_FLAG_ROTATE;
		else if (!strncmp(p, "max_size=", 9)) fdata->max_size = _act_file_max_size(p+9);

		free(p);
		p = NULL;
	}

	if (fdata->fmt == NULL) fdata->fmt = strdup("std");

	/* duplicate compression is only possible for std and bsd formats */
	if (strcmp(fdata->fmt, "std") && strcmp(fdata->fmt, "bsd")) fdata->flags &= ~ACT_FILE_FLAG_DUP_SUPRESS;

	/* set time format for raw output */
	if (!strcmp(fdata->fmt, "raw")) fdata->tfmt = "sec";

	r->data = fdata;
}

static void
_act_file(action_rule_t *r, aslmsg msg)
{
	struct file_data *fdata;
	int is_dup;
	uint32_t len, msg_hash = 0;
	char *str;
	time_t now, today;
	struct tm ctm;

	if (r->data == NULL) return;

	fdata = (struct file_data *)r->data;

	now = time(NULL);
	today = now;

	memset(&ctm, 0, sizeof(struct tm));
	if (localtime_r((const time_t *)&now, &ctm) != NULL)
	{
		ctm.tm_sec = 0;
		ctm.tm_min = 0;
		ctm.tm_hour = 0;
		today = mktime(&ctm);
	}

	/* check for rotation */
	if ((last_file_day != 0) && (last_file_day != today))
	{
		_act_file_rotate(NULL);
	}

	last_file_day = today;


	/*
	 * asl.conf may contain multuple rules for the same file, eg:
	 *    ? [= Facility zippy] /var/log/abc.log
	 *    ? [= Color purple] /var/log/abc.log
	 *
	 * To prevent duplicates we set a flag bit when a message is logged
	 * to this file, and bail out if it has already been logged.
	 * Note that asl_out_message clears the flag bit in all file_data
	 * structures before processing each message.
	 */
	if (fdata->flags & ACT_FLAG_HAS_LOGGED) return;
	fdata->flags |= ACT_FLAG_HAS_LOGGED;

	is_dup = 0;

	str = asl_format_message((asl_msg_t *)msg, fdata->fmt, fdata->tfmt, ASL_ENCODE_SAFE, &len);

	if (fdata->flags & ACT_FILE_FLAG_DUP_SUPRESS)
	{
		if (fdata->dup_timer == NULL)
		{
			/* create a timer to flush dups on this file */
			fdata->dup_timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, asl_action_queue);
			dispatch_source_set_event_handler(fdata->dup_timer, ^{ _act_file_send_repeat_msg((struct file_data *)r->data); });
		}

		if ((global.bsd_max_dup_time > 0) && (str != NULL) && (fdata->last_msg != NULL))
		{
			msg_hash = asl_core_string_hash(str + 16, len - 16);
			if ((fdata->last_hash == msg_hash) && (!strcmp(fdata->last_msg, str + 16)))
			{
				if ((now - fdata->last_time) < global.bsd_max_dup_time) is_dup = 1;
			}
		}
	}

	if (is_dup == 1)
	{
		if (fdata->last_count == 0)
		{
			/* start the timer */
			dispatch_source_set_timer(fdata->dup_timer, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * global.bsd_max_dup_time), DISPATCH_TIME_FOREVER, 0);
			dispatch_resume(fdata->dup_timer);
		}

		fdata->last_count++;
	}
	else
	{
		fdata->fd = _act_file_open(fdata);
		if (fdata->fd < 0)
		{
			asldebug("_act_file_open %s failed: %s\n", fdata->path, strerror(errno));

			fdata->fails++;

			/* disable further activity after multiple failures */
			if (fdata->fails > MAX_FAILURES)
			{
				char *tmp = NULL;
				asprintf(&tmp, "[%s syslogd] [%s %u] [%s %u] [Facility syslog] [%s Disabling writes to path %s following %u failures (%s)]",
						 ASL_KEY_SENDER,
						 ASL_KEY_LEVEL, ASL_LEVEL_ERR,
						 ASL_KEY_PID, getpid(),
						 ASL_KEY_MSG, fdata->path, fdata->fails, strerror(errno));

				internal_log_message(tmp);
				free(tmp);

				r->action = ACTION_NONE;
				free(str);
				return;
			}
		}
		else
		{
			fdata->fails = 0;
		}

		/*
		 * The current message is not a duplicate.  If fdata->last_count > 0
		 * we need to write a "last message repeated N times" log entry.
		 * _act_file_send_repeat_msg will free last_msg and do nothing if
		 * last_count == 0, but we test and free here to avoid a function call.
		 */
		if (fdata->last_count > 0)
		{
			_act_file_send_repeat_msg(fdata);
		}
		else
		{
			free(fdata->last_msg);
			fdata->last_msg = NULL;
		}

		if (str != NULL) fdata->last_msg = strdup(str + 16);

		fdata->last_hash = msg_hash;
		fdata->last_count = 0;
		fdata->last_time = now;

		if ((str != NULL) && (len > 1)) write(fdata->fd, str, len - 1);
		close(fdata->fd);
		fdata->fd = -1;
	}

	free(str);
}

static void
_act_forward(action_rule_t *r, aslmsg msg)
{
	/* To do: <rdar://problem/6130747> Add a "forward" action to asl.conf */
}

static void
_send_to_asl_store(aslmsg msg)
{
	int log_me;
	action_rule_t *r;

	/* ASLOption "store" forces a message to be saved */
	log_me = asl_check_option(msg, ASL_OPT_STORE);
	if (log_me == 1)
	{
		db_save_message(msg);
		return;
	}

	/* if there are no rules, save the message */
	if (asl_datastore_rule == NULL)
	{
		db_save_message(msg);
		return;
	}

	for (r = asl_datastore_rule; r != NULL; r = r->next)
	{
		if (asl_msg_cmp(r->query, (asl_msg_t *)msg) == 1)
		{
			/* if any rule matches, save the message (once!) */
			db_save_message(msg);
			return;
		}
	}
}

static void
_asl_action_message(aslmsg msg)
{
	action_rule_t *r;

	if (msg == NULL) return;

	/* reset flag bit used for file duplicate avoidance */
	for (r = asl_action_rule; r != NULL; r = r->next)
	{
		if ((r->action == ACTION_FILE) && (r->data != NULL))
		{
			((struct file_data *)(r->data))->flags &= ACT_FLAG_CLEAR_LOGGED;
		}
		else if (((r->action == ACTION_ASL_DIR) || (r->action == ACTION_ASL_FILE)) && (r->data != NULL))
		{
			((struct store_data *)(r->data))->flags &= ACT_FLAG_CLEAR_LOGGED;
		}
	}

	for (r = asl_action_rule; r != NULL; r = r->next)
	{
		if (asl_msg_cmp(r->query, (asl_msg_t *)msg) == 1)
		{
			if ((r->action == ACTION_ASL_FILE) || (r->action == ACTION_ASL_DIR))
			{
				_act_store(r, msg);
				if (asl_check_option(msg, ASL_OPT_IGNORE) != 0) return;
			}

			if (r->action == ACTION_NONE) continue;
			else if (r->action == ACTION_IGNORE) return;
			else if (r->action == ACTION_ACCESS) _act_access_control(r, msg);
			else if (r->action == ACTION_NOTIFY) _act_notify(r);
			else if (r->action == ACTION_BROADCAST) _act_broadcast(r, msg);
			else if (r->action == ACTION_FILE) _act_file(r, msg);
			else if (r->action == ACTION_FORWARD) _act_forward(r, msg);
		}
	}

	if (asl_check_option(msg, ASL_OPT_IGNORE) != 0) return;

	_send_to_asl_store(msg);
}

void
asl_out_message(aslmsg msg)
{
	dispatch_flush_continuation_cache();

	asl_msg_retain((asl_msg_t *)msg);

	dispatch_async(asl_action_queue, ^{
		_asl_action_message(msg);
		asl_msg_release((asl_msg_t *)msg);
	});
}

static int
_parse_config_file(const char *name)
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
	static dispatch_once_t once;

	asldebug("%s: init\n", MY_ID);
	_parse_config_file(_PATH_ASL_CONF);

	dispatch_once(&once, ^{
		asl_action_queue = dispatch_queue_create("ASL Action Queue", NULL);
	});

	return 0;
}

int
_asl_action_close_internal(void)
{
	action_rule_t *r, *n;
	struct store_data *sd;
	struct file_data *fdata;
	n = NULL;
	for (r = asl_action_rule; r != NULL; r = n)
	{
		n = r->next;
		if (r->data != NULL)
		{
			if (((r->action == ACTION_ASL_FILE) || (r->action == ACTION_ASL_DIR) || (r->action == ACTION_NONE)))
			{
				sd = (struct store_data *)r->data;
				if (sd->refcount > 0) sd->refcount--;
				if (sd->refcount == 0)
				{
					if (sd->store != NULL) asl_file_close(sd->store);
					if (sd->storedata != NULL) fclose(sd->storedata);

					free(sd->dir);
					free(sd->path);
					free(sd);
				}
			}

			if (r->action == ACTION_FILE)
			{
				fdata = (struct file_data *)r->data;
				if (fdata->refcount > 0) fdata->refcount--;
				if (fdata->refcount == 0)
				{
					_act_file_send_repeat_msg(fdata);

					if (fdata->dup_timer != NULL)
					{
						dispatch_source_cancel(fdata->dup_timer);
						dispatch_resume(fdata->dup_timer);
						dispatch_release(fdata->dup_timer);
					}

					free(fdata->path);
					free(fdata->fmt);
					free(fdata->uid);
					free(fdata->gid);
					free(fdata->last_msg);
					free(fdata);
				}
			}
		}

		if (r->query != NULL) asl_msg_release(r->query);
		free(r->options);

		free(r);
	}

	asl_action_rule = NULL;

	n = NULL;
	for (r = asl_datastore_rule; r != NULL; r = n)
	{
		n = r->next;

		if (r->query != NULL) asl_msg_release(r->query);
		free(r->options);

		free(r);
	}

	asl_datastore_rule = NULL;

	return 0;
}

int
asl_action_close(void)
{
	dispatch_async(asl_action_queue, ^{
		_asl_action_close_internal();
	});

	return 0;
}

int
asl_action_reset(void)
{
	dispatch_async(asl_action_queue, ^{
		_asl_action_close_internal();
		asl_action_init();
	});

	return 0;
}

int
asl_action_file_rotate(const char *path)
{
	/*
	 * The caller may want to know when the rotation has been completed,
	 * so this is synchronous. Also ensures the string stays intact while we work.
	 */
	dispatch_sync(asl_action_queue, ^{
		_act_file_rotate(path);
	});

	return 0;
}

