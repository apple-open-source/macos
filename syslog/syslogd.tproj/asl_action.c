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
#include <asl_core.h>
#include "daemon.h"

#define _PATH_WALL "/usr/bin/wall"
#define _PATH_ASL_CONF "/etc/asl.conf"
#define MY_ID "asl_action"

#define ACTION_NONE      0
#define ACTION_IGNORE    1
#define ACTION_NOTIFY    2
#define ACTION_BROADCAST 3
#define ACTION_ACCESS    4
#define ACTION_STORE     5
#define ACTION_STORE_DIR 6
#define ACTION_FORWARD   7

#define IndexNull ((uint32_t)-1)
#define forever for(;;)

#define ACT_STORE_FLAG_STAY_OPEN     0x00000001
#define ACT_STORE_FLAG_EXCLUDE_ASLDB 0x00000002

static asl_msg_t *query = NULL;
static int reset = RESET_NONE;
static pthread_mutex_t reset_lock = PTHREAD_MUTEX_INITIALIZER;

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
	uint32_t flags;
	uint32_t p_year;
	uint32_t p_month;
	uint32_t p_day;
};

static action_rule_t *asl_action_rule = NULL;
static action_rule_t *asl_datastore_rule = NULL;
static int filter_token = -1;

int asl_action_close();
static int _parse_config_file(const char *);
extern void db_save_message(asl_msg_t *m);

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
_do_reset(void)
{
	pthread_mutex_lock(&reset_lock);

	asl_action_close();
	_parse_config_file(_PATH_ASL_CONF);
	reset = RESET_NONE;

	pthread_mutex_unlock(&reset_lock);
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
	else if (!strcasecmp(act, "store"))           out->action = ACTION_STORE;
	else if (!strcasecmp(act, "save"))            out->action = ACTION_STORE;
	else if (!strcasecmp(act, "store_directory")) out->action = ACTION_STORE_DIR;
	else if (!strcasecmp(act, "store_dir"))       out->action = ACTION_STORE_DIR;
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

	if (s[0] == '*') out->query = asl_new(ASL_TYPE_QUERY);
	else
	{
		s[0] = 'Q';
		out->query = asl_msg_from_string(s);
	}

	if (out->query == NULL)
	{
		asldebug("out->query is NULL (ERROR)\n");
		if (out->options != NULL) free(out->options);
		free(out);
		return -1;
	}

	if ((out->action == ACTION_STORE) && (out->options == NULL))
	{
		asldebug("action = ACTION_STORE options = NULL\n");
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

/*
 * Used to sed config parameters.
 * Line format "= name value"
 */
static int
_parse_set_param(char *s)
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
	else if (!strcasecmp(l[0], "cutoff"))
	{
		/* = cutoff level */
		intval = atoi(l[1]);
		if (intval > ASL_LEVEL_DEBUG) intval = ASL_LEVEL_DEBUG;
		global.asl_log_filter = ASL_FILTER_MASK_UPTO(intval);
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
	else if (!strcasecmp(l[0], "asl_store_ping_time"))
	{
		/* NB this is private / unpublished */
		/* = asl_store_ping_time seconds */
		OSSpinLockLock(&global.lock);
		global.asl_store_ping_time = atoll(l[1]);
		OSSpinLockUnlock(&global.lock);
	}
	else if (!strcasecmp(l[0], "utmp_ttl"))
	{
		/* = utmp_ttl seconds */
		OSSpinLockLock(&global.lock);
		global.utmp_ttl = (time_t)atoll(l[1]);
		OSSpinLockUnlock(&global.lock);
	}
	else if (!strcasecmp(l[0], "fs_ttl"))
	{
		/* = fs_ttl seconds */
		OSSpinLockLock(&global.lock);
		global.fs_ttl = (time_t)atoll(l[1]);
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
			status = _parse_set_param(s);
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

		asl_log_string(str);
		if (str != NULL) free(str);
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
_act_broadcast(action_rule_t *r, asl_msg_t *msg)
{
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
}

static void
_act_access_control(action_rule_t *r, asl_msg_t *msg)
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

	if (sd == NULL) return ASL_STATUS_INVALID_STORE;
	if (sd->dir == NULL) return ASL_STATUS_INVALID_STORE;

	/* get / set message id from StoreData file */
	xid = 0;

	if (sd->storedata == NULL)
	{
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
		else if (errno != ENOENT)
		{
			/* Unexpected stat error */
			free(path);
			return ASL_STATUS_FAILED;
		}
		else
		{
			/* StoreData does not exist: create it */
			sd->storedata = fopen(path, "w");
			if (sd->storedata == NULL)
			{
				free(path);
				return ASL_STATUS_FAILED;
			}
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

	sd->p_year = 0;
	sd->p_month = 0;
	sd->p_day = 0;

	if (sd->path != NULL) free(sd->path);
	sd->path = NULL;

	asprintf(&(sd->path), "%s/%d.%02d.%02d.asl", sd->dir, ctm.tm_year + 1900, ctm.tm_mon + 1, ctm.tm_mday);
	if (sd->path == NULL) return ASL_STATUS_NO_MEMORY;

	sd->p_year = ctm.tm_year;
	sd->p_month = ctm.tm_mon;
	sd->p_day = ctm.tm_mday;

	return ASL_STATUS_OK;
}

static void
_act_store(action_rule_t *r, asl_msg_t *msg)
{
	struct store_data *sd;
	asl_file_t *s;
	uint8_t x;
	uint32_t status;
	uint64_t mid;
	mode_t tmp_mode;
	char *str, *opts, *p;
	const char *val;
	time_t tick;

	s = NULL;

	/* _act_store is not used for the main ASL data store */
	if (r->options == NULL) return;

	if (r->data == NULL)
	{
		/* set up store data */
		sd = (struct store_data *)calloc(1, sizeof(struct store_data));
		if (sd == NULL) return;

		opts = r->options;
		sd->store = NULL;

		if (r->action == ACTION_STORE)
		{
			sd->path = _next_word(&opts);
			if (sd->path == NULL)
			{
				free(sd);
				r->action = ACTION_NONE;
				return;
			}
		}
		else if (r->action == ACTION_STORE_DIR)
		{
			sd->dir = _next_word(&opts);
			if (sd->dir == NULL)
			{
				free(sd);
				r->action = ACTION_NONE;
				return;
			}
		}

		sd->mode = 0644;
		sd->next_id = 0;
		sd->uid = 0;
		sd->gid = 0;
		sd->flags = 0;

		while (NULL != (p = _next_word(&opts)))
		{
			if (!strcmp(p, "stayopen")) sd->flags |= ACT_STORE_FLAG_STAY_OPEN;
			else if (!strcmp(p, "exclude_asldb")) sd->flags |= ACT_STORE_FLAG_EXCLUDE_ASLDB;
			else if (!strncmp(p, "mode=0", 6))
			{
				sd->mode = 0;
				x = *(p + 6);
				if ((x < '0') || (x > '7'))
				{
					free(p);
					if (sd->path != NULL) free(sd->path);
					if (sd->dir != NULL) free(sd->dir);
					free(sd);
					r->action = ACTION_NONE;
					return;
				}

				tmp_mode = x - '0';
				sd->mode += tmp_mode << 6;

				x = *(p + 7);
				if ((x < '0') || (x > '7'))
				{
					free(p);
					if (sd->path != NULL) free(sd->path);
					if (sd->dir != NULL) free(sd->dir);
					free(sd);
					r->action = ACTION_NONE;
					return;
				}

				tmp_mode = x - '0';
				sd->mode += tmp_mode << 3;

				x = *(p + 8);
				if ((x < '0') || (x > '7'))
				{
					free(p);
					if (sd->path != NULL) free(sd->path);
					if (sd->dir != NULL) free(sd->dir);
					free(sd);
					r->action = ACTION_NONE;
					return;
				}

				tmp_mode = x - '0';
				sd->mode += tmp_mode;
			}
			else if (!strncmp(p, "mode=", 5)) sd->mode = atoi(p+4);
			else if (!strncmp(p, "uid=", 4)) sd->uid = atoi(p+4);
			else if (!strncmp(p, "gid=", 4)) sd->gid = atoi(p+4);

			free(p);
			p = NULL;
		}

		r->data = sd;
	}
	else
	{
		sd = (struct store_data *)r->data;
	}

	if (r->action == ACTION_STORE_DIR)
	{
		val = asl_get(msg, ASL_KEY_TIME);
		if (val == NULL) return;

		tick = atol(val);
		status = _act_store_dir_setup(sd, tick);
		if (status != ASL_STATUS_OK)
		{
			asldebug("_act_store_dir_setup %s failed: %s\n", sd->path, asl_core_error(status));

			/* disable further activity */
			asl_file_close(sd->store);
			sd->store = NULL;
			r->action = ACTION_NONE;
			return;
		}
	}

	if (sd->store == NULL)
	{
		s = NULL;
		status = asl_file_open_write(sd->path, sd->mode, sd->uid, sd->gid, &s);
		if ((status != ASL_STATUS_OK) || (s == NULL))
		{
			asldebug("asl_file_open_write %s failed: %s\n", sd->path, asl_core_error(status));

			/* disable further activity */
			asl_file_close(sd->store);
			sd->store = NULL;
			r->action = ACTION_NONE;
			return;
		}

		sd->store = s;
	}

	if (r->action != ACTION_STORE_DIR)
	{
		status = _act_store_file_setup(sd);
		if (status != ASL_STATUS_OK)
		{
			asldebug("_act_store_file_setup %s failed: %s\n", sd->path, asl_core_error(status));

			/* disable further activity */
			asl_file_close(sd->store);
			sd->store = NULL;
			r->action = ACTION_NONE;
			return;
		}
	}

	mid = sd->next_id;

	status = asl_file_save(sd->store, msg, &mid);
	if (status != ASL_STATUS_OK)
	{
		asldebug("asl_file_save %s failed: %s\n", sd->path, asl_core_error(status));

		/* disable further activity on this file */
		asl_file_close(sd->store);
		sd->store = NULL;
		r->action = ACTION_NONE;
		return;
	}

	if ((sd->flags & ACT_STORE_FLAG_STAY_OPEN) == 0)
	{
		asl_file_close(sd->store);
		sd->store = NULL;
	}

	if (sd->flags & ACT_STORE_FLAG_EXCLUDE_ASLDB)
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

static void
_act_forward(action_rule_t *r, asl_msg_t *msg)
{
	/* To do: <rdar://problem/6130747> Add a "forward" action to asl.conf */
}

static void
send_to_asl_store(asl_msg_t *msg)
{
	const char *vlevel, *val;
	uint64_t v64;
	uint32_t status, level, lmask;
	int x, log_me;
	action_rule_t *r;

	/* ASLOption "ignore" keeps a message out of the ASL datastore */
	if (asl_check_option(msg, ASL_OPT_IGNORE) != 0) return;

	if (filter_token == -1)
	{
		/* set up com.apple.syslog.asl_filter */
		status = notify_register_check(NOTIFY_SYSTEM_ASL_FILTER, &filter_token);
		if (status != NOTIFY_STATUS_OK)
		{
			filter_token = -1;
		}
		else
		{
			status = notify_check(filter_token, &x);
			if (status == NOTIFY_STATUS_OK)
			{
				v64 = global.asl_log_filter;
				status = notify_set_state(filter_token, v64);
			}
			if (status != NOTIFY_STATUS_OK)
			{
				notify_cancel(filter_token);
				filter_token = -1;
			}
		}
	}

	/* ASLOption "store" forces a message to be saved */
	log_me = asl_check_option(msg, ASL_OPT_STORE);
	if (log_me == 1)
	{
		db_save_message(msg);
		return;
	}

	log_me = 0;
	if (filter_token >= 0)
	{
		x = 0;
		status = notify_check(filter_token, &x);
		if ((status == NOTIFY_STATUS_OK) && (x == 1))
		{
			v64 = 0;
			status = notify_get_state(filter_token, &v64);
			if ((status == NOTIFY_STATUS_OK) && (v64 != 0)) global.asl_log_filter = v64;
		}
	}

	/* PID 0 (kernel) or PID 1 (launchd) messages are saved */
	val = asl_get(msg, ASL_KEY_PID);
	if ((val != NULL) && (atoi(val) <= 1)) log_me = 1;
	else
	{
		vlevel = asl_get(msg, ASL_KEY_LEVEL);
		level = 7;
		if (vlevel != NULL) level = atoi(vlevel);
		lmask = ASL_FILTER_MASK(level);
		if ((lmask & global.asl_log_filter) != 0) log_me = 1;
	}

	if (log_me == 0) return;

	/* if there are no rules, save the message */
	if (asl_datastore_rule == NULL)
	{
		db_save_message(msg);
		return;
	}

	for (r = asl_datastore_rule; r != NULL; r = r->next)
	{
		if (asl_msg_cmp(r->query, msg) == 1)
		{
			/* if any rule matches, save the message (once!) */
			db_save_message(msg);
			return;
		}
	}
}

int
asl_action_sendmsg(asl_msg_t *msg, const char *outid)
{
	action_rule_t *r;

	if (reset == RESET_CONFIG) _do_reset();

	if (msg == NULL) return -1;

	for (r = asl_action_rule; r != NULL; r = r->next)
	{
		if (asl_msg_cmp(r->query, msg) == 1)
		{
			if (r->action == ACTION_NONE) continue;
			else if (r->action == ACTION_IGNORE) return 0;
			else if (r->action == ACTION_ACCESS) _act_access_control(r, msg);
			else if (r->action == ACTION_NOTIFY) _act_notify(r);
			else if (r->action == ACTION_STORE) _act_store(r, msg);
			else if (r->action == ACTION_STORE_DIR) _act_store(r, msg);
			else if (r->action == ACTION_BROADCAST) _act_broadcast(r, msg);
			else if (r->action == ACTION_FORWARD) _act_forward(r, msg);
		}
	}

	send_to_asl_store(msg);

	return 0;
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
	asldebug("%s: init\n", MY_ID);

	query = asl_new(ASL_TYPE_QUERY);
	aslevent_addmatch(query, MY_ID);
	aslevent_addoutput(asl_action_sendmsg, MY_ID);

	_parse_config_file(_PATH_ASL_CONF);
	return 0;
}

int
asl_action_reset(void)
{
	reset = global.reset;
	return 0;
}

int
asl_action_close(void)
{
	action_rule_t *r, *n;
	struct store_data *sd;
	n = NULL;
	for (r = asl_action_rule; r != NULL; r = n)
	{
		n = r->next;

		if (((r->action == ACTION_STORE) || (r->action == ACTION_STORE_DIR) || (r->action == ACTION_NONE)) && (r->data != NULL))
		{
			sd = (struct store_data *)r->data;
			if (sd->store != NULL) asl_file_close(sd->store);
			if (sd->storedata != NULL) fclose(sd->storedata);
			if (sd->path != NULL) free(sd->path);
			if (sd->dir != NULL) free(sd->dir);
			sd->store = NULL;
			free(sd);
		}

		if (r->query != NULL) asl_free(r->query);
		if (r->options != NULL) free(r->options);

		free(r);
	}

	asl_action_rule = NULL;

	n = NULL;
	for (r = asl_datastore_rule; r != NULL; r = n)
	{
		n = r->next;

		if (r->query != NULL) asl_free(r->query);
		if (r->options != NULL) free(r->options);

		free(r);
	}

	asl_datastore_rule = NULL;

	return 0;
}
