/*
 * Copyright (c) 2004-2013 Apple Inc. All rights reserved.
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

#include <TargetConditionals.h>

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
#include <dirent.h>
#include <time.h>
#include <membership.h>
#include <configuration_profile.h>
#include "daemon.h"
#include <xpc/private.h>

#define _PATH_WALL "/usr/bin/wall"
#define NOTIFY_PATH_SERVICE "com.apple.system.notify.service.path:0x87:"

#define MY_ID "asl_action"

/* XXX add to asl.h */
#define ASL_KEY_MODULE "ASLModule"

#define MAX_FAILURES 5

#define ACTION_STATUS_ERROR  -1
#define ACTION_STATUS_OK      0

#define IDLE_CLOSE 300

#define forever for(;;)

static dispatch_queue_t asl_action_queue;
static dispatch_source_t checkpoint_timer;
static time_t sweep_time = 0;

#if TARGET_OS_EMBEDDED
static dispatch_queue_t crashlog_queue;
static dispatch_source_t crashlog_sentinel_src;
static int crashlog_sentinel_fd = -1;
static time_t crashmover_state = 0;
static int crashmover_token = -1;
#endif

typedef struct store_data
{
	asl_file_t *store;
	FILE *storedata;
	uint64_t next_id;
	time_t last_time;
	uint32_t p_year;
	uint32_t p_month;
	uint32_t p_day;
	dispatch_source_t monitor;
} asl_action_store_data_t;

typedef struct file_data
{
	int fd;
	uint32_t last_hash;
	uint32_t last_count;
	time_t last_time;
	char *last_msg;
	dispatch_source_t dup_timer;
	dispatch_source_t monitor;
} asl_action_file_data_t;

typedef struct set_param_data
{
	int token;
} asl_action_set_param_data_t;

static int action_asl_store_count;
static bool store_has_logged;

extern void db_save_message(aslmsg m);

/* forward */
static int _act_file_checkpoint_all(uint32_t force);
static void _asl_action_post_process_rule(asl_out_module_t *m, asl_out_rule_t *r);
static void _asl_action_close_idle_files(time_t idle_time);

static void
_act_out_set_param(asl_out_module_t *m, char *x, bool eval)
{
	char *s = x;
	char **l;
	uint32_t count, intval;

	l = explode(s, " \t");
	if (l == NULL) return;

	for (count = 0; l[count] != NULL; count++);
	if (count == 0)
	{
		free_string_list(l);
		return;
	}

	if (!strcasecmp(l[0], "enable"))
	{
		/* = enable [1|0] */
		if (count < 2) intval = 1;
		else intval = atoi(l[1]);

		if (!eval) intval = (intval == 0) ? 1 : 0;

		if (intval == 0) m->flags &= ~MODULE_FLAG_ENABLED;
		else m->flags|= MODULE_FLAG_ENABLED;
		return;
	}
	else if (!strcasecmp(l[0], "disable"))
	{
		/* = disable [1|0] */
		if (count < 2) intval = 1;
		else intval = atoi(l[1]);

		if (!eval) intval = (intval == 0) ? 1 : 0;

		if (intval != 0) m->flags &= ~MODULE_FLAG_ENABLED;
		else m->flags|= MODULE_FLAG_ENABLED;
		return;
	}

	free_string_list(l);

	if (!strcmp(m->name, ASL_MODULE_NAME))
	{
		/* Other parameters may be set by com.apple.asl module */
		control_set_param(x, eval);
	}
}

static void
_act_notify(asl_out_module_t *m, asl_out_rule_t *r)
{
	if (m == NULL) return;
	if ((m->flags & MODULE_FLAG_ENABLED) == 0) return;

	if (r == NULL) return;
	if (r->options == NULL) return;

	notify_post(r->options);
}

static void
_act_broadcast(asl_out_module_t *m, asl_out_rule_t *r, aslmsg msg)
{
#if !TARGET_OS_EMBEDDED
	FILE *pw;
	const char *val;

	if (m == NULL) return;
	if ((m->flags & MODULE_FLAG_ENABLED) == 0) return;

	if (m->name == NULL) return;
	if (r == NULL) return;
	if (msg == NULL) return;

	/* only base module (asl.conf) may broadcast */
	if (strcmp(m->name, ASL_MODULE_NAME)) return;

	val = r->options;
	if (val == NULL) val = asl_get(msg, ASL_KEY_MSG);
	if (val == NULL) return;

	pw = popen(_PATH_WALL, "w");
	if (pw == NULL)
	{
		asldebug("%s: error sending wall message: %s\n", MY_ID, strerror(errno));
		return;
	}

	fprintf(pw, "%s", val);
	pclose(pw);
#endif
}

static void
_act_access_control(asl_out_module_t *m, asl_out_rule_t *r, aslmsg msg)
{
	int32_t ruid, rgid;
	char *p;

	if (m == NULL) return;
	if (m->name == NULL) return;
	if (r == NULL) return;
	if (msg == NULL) return;

	/* only base module (asl.conf) may set access controls */
	if (strcmp(m->name, ASL_MODULE_NAME)) return;

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

#if TARGET_OS_EMBEDDED
static void
_crashlog_sentinel_init(void)
{
	char path[MAXPATHLEN];

	if (crashlog_sentinel_src != NULL) return;

	snprintf(path, sizeof(path), "%s/com.apple.asl.%ld", _PATH_CRASHREPORTER, time(NULL));

	crashlog_sentinel_fd = open(path, O_WRONLY | O_CREAT);
	if (crashlog_sentinel_fd < 0)
	{
		char *str = NULL;
		asprintf(&str, "[Sender syslogd] [Level 3] [PID %u] [Facility syslog] [Message Sentinel %s create/open failed (%s)]", global.pid, path, strerror(errno));
		internal_log_message(str);
		free(str);
		return;
	}

	close(crashlog_sentinel_fd);

	crashlog_sentinel_fd = open(path, O_EVTONLY, 0);
	if (crashlog_sentinel_fd < 0)
	{
		char *str = NULL;
		asprintf(&str, "[Sender syslogd] [Level 3] [PID %u] [Facility syslog] [Message Sentinel %s event/open failed (%s)]", global.pid, path, strerror(errno));
		internal_log_message(str);
		free(str);
		return;
	}

	crashlog_sentinel_src = dispatch_source_create(DISPATCH_SOURCE_TYPE_VNODE, (uintptr_t)crashlog_sentinel_fd, DISPATCH_VNODE_DELETE, asl_action_queue);
	if (crashlog_sentinel_src == NULL)
	{
		char *str = NULL;
		asprintf(&str, "[Sender syslogd] [Level 3] [PID %u] [Facility syslog] [Message Sentinel %s dispatch_source_create failed]", global.pid, path);
		internal_log_message(str);
		free(str);
		close(crashlog_sentinel_fd);
		crashlog_sentinel_fd = -1;
		return;
	}

	dispatch_source_set_event_handler(crashlog_sentinel_src, ^{
		if (crashmover_state != 0)
		{
			asldebug("CrashMover inactive / sentinel deleted: resuming crashlog queue\n");
			dispatch_resume(crashlog_queue);
			crashmover_state = 0;
		}

		if (crashlog_sentinel_src != NULL)
		{
			dispatch_source_cancel(crashlog_sentinel_src);
			dispatch_release(crashlog_sentinel_src);
		}

		crashlog_sentinel_src = NULL;

		close(crashlog_sentinel_fd);
		crashlog_sentinel_fd = -1;
		_crashlog_sentinel_init();
	});

	dispatch_resume(crashlog_sentinel_src);
	asldebug("Created CrashLog Sentinel: %s\n", path);
}

static void
_crashlog_queue_check(void)
{
	/*
	 * Check whether the crashlog queue has been suspended for too long.
	 * We allow the crashlog quque to be suspended for 60 seconds.
	 * After that, we start logging again.  This prevents syslogd from
	 * filling memory due to a suspended queue.  CrashMover really shoud
	 * take no more than a second or two to finish.
	 */
	if (crashmover_state == 0) return;
	if ((time(NULL) - crashmover_state) <= 60) return;

	asldebug("CrashMover timeout: resuming crashlog queue\n");
	dispatch_resume(crashlog_queue);
	crashmover_state = 0;

	/*
	 * crashlog_sentinel_src should never be NULL, but if
	 * _crashlog_sentinel_init failed for some strange reason,
	 * it will be NULL here.
	 */
	if (crashlog_sentinel_src != NULL)
	{
		dispatch_source_cancel(crashlog_sentinel_src);
		dispatch_release(crashlog_sentinel_src);
	}

	crashlog_sentinel_src = NULL;

	close(crashlog_sentinel_fd);
	crashlog_sentinel_fd = -1;
	_crashlog_sentinel_init();
}
#endif

static void
_act_dst_close(asl_out_rule_t *r)
{
	if (r == NULL) return;
	if (r->dst == NULL) return;
	if (r->dst->private == NULL) return;

	if ((r->action == ACTION_ASL_DIR) || (r->action == ACTION_ASL_FILE))
	{
		asl_action_store_data_t *sdata = (asl_action_store_data_t *)r->dst->private;
		if (sdata->store != NULL) asl_file_close(sdata->store);
		sdata->store = NULL;

		if (sdata->storedata != NULL) fclose(sdata->storedata);
		sdata->storedata = NULL;

		if (sdata->monitor != NULL)
		{
			dispatch_source_cancel(sdata->monitor);
			dispatch_release(sdata->monitor);
			sdata->monitor = NULL;
		}
	}
	else if (r->action == ACTION_FILE)
	{
		asl_action_file_data_t *fdata = (asl_action_file_data_t *)r->dst->private;
		if (fdata->fd >= 0) close(fdata->fd);
		fdata->fd = -1;

		if (fdata->monitor != NULL)
		{
			dispatch_source_cancel(fdata->monitor);
			dispatch_release(fdata->monitor);
			fdata->monitor = NULL;
		}
	}
}

static uint32_t
_act_store_file_setup(asl_out_module_t *m, asl_out_rule_t *r)
{
	uint32_t status;
	asl_action_store_data_t *sdata;
	char dstpath[MAXPATHLEN];

	if (r == NULL) return ASL_STATUS_INVALID_STORE;
	if (r->dst == NULL) return ASL_STATUS_INVALID_STORE;
	if (r->dst->private == NULL) return ASL_STATUS_INVALID_STORE;

	sdata = (asl_action_store_data_t *)r->dst->private;
	if (sdata->store == NULL)
	{
		/* create path if necessary */
		asl_out_mkpath(r);

		int fd = asl_out_dst_file_create_open(r->dst);
		if (fd < 0)
		{
			asldebug("_act_store_file_setup: asl_out_dst_file_create_open failed %d %s\n", errno, strerror(errno));
			return ASL_STATUS_WRITE_FAILED;
		}
		close(fd);

		asl_make_dst_filename(r->dst, dstpath, sizeof(dstpath));
		status = asl_file_open_write(dstpath, 0, -1, -1, &(sdata->store));
		if (status != ASL_STATUS_OK) return status;

		sdata->monitor = dispatch_source_create(DISPATCH_SOURCE_TYPE_VNODE, fileno(sdata->store->store), DISPATCH_VNODE_DELETE, asl_action_queue);
		if (sdata->monitor != NULL)
		{
			dispatch_source_set_event_handler(sdata->monitor, ^{ _act_dst_close(r); });
			dispatch_resume(sdata->monitor);
		}

		status = asl_file_read_set_position(sdata->store, ASL_FILE_POSITION_LAST);
		if (status != ASL_STATUS_OK)
		{
			asldebug("_act_store_file_setup: asl_file_read_set_position failed %d %s\n", status, asl_core_error(status));
			return status;
		}

		sdata->next_id = sdata->store->cursor_xid + 1;
		if (fseek(sdata->store->store, 0, SEEK_END) != 0)
		{
			asldebug("_act_store_file_setup: fseek failed %d %s\n", errno, strerror(errno));
			return ASL_STATUS_WRITE_FAILED;
		}
	}
	else
	{
		sdata->next_id++;
	}

	return ASL_STATUS_OK;
}

/*
 * _act_store_dir_setup
 *
 * Creates store directory if it does not exist
 * Creates StoreData file if it does not exist
 * Reads ASL Message ID from StoreData file
 * Writes ASL Message ID + 1 to StoreData file
 * Opens current day file (e.g. "/foo/bar/2012.04.06.asl")
 */
static uint32_t
_act_store_dir_setup(asl_out_module_t *m, asl_out_rule_t *r, time_t tick)
{
	struct tm ctm;
	char *path;
	struct stat sb;
	uint64_t xid;
	int status;
	mode_t mask;
	asl_action_store_data_t *sdata;

	if (r == NULL) return ASL_STATUS_INVALID_STORE;
	if (r->dst == NULL) return ASL_STATUS_INVALID_STORE;
	if (r->dst->private == NULL) return ASL_STATUS_INVALID_STORE;
	if (r->dst->path == NULL) return ASL_STATUS_INVALID_STORE;

	sdata = (asl_action_store_data_t *)r->dst->private;

	/* get / set message id from StoreData file */
	xid = 0;

	if (sdata->storedata == NULL)
	{
		memset(&sb, 0, sizeof(struct stat));
		status = stat(r->dst->path, &sb);
		if (status == 0)
		{
			/* must be a directory */
			if (!S_ISDIR(sb.st_mode)) return ASL_STATUS_INVALID_STORE;
		}
		else if (errno == ENOENT)
		{
			/* doesn't exist - create it */
			mask = umask(S_IWGRP | S_IWOTH);
			status = mkpath_np(r->dst->path, 0755);
			if (status == 0) status = chmod(r->dst->path, r->dst->mode);
			umask(mask);

			if (status != 0) return ASL_STATUS_WRITE_FAILED;
#if !TARGET_IPHONE_SIMULATOR
			if (chown(r->dst->path, r->dst->uid[0], r->dst->gid[0]) != 0) return ASL_STATUS_WRITE_FAILED;
#endif
		}
		else
		{
			/* Unexpected stat error */
			return ASL_STATUS_FAILED;
		}

		path = NULL;
		asprintf(&path, "%s/%s", r->dst->path, FILE_ASL_STORE_DATA);
		if (path == NULL) return ASL_STATUS_NO_MEMORY;

		memset(&sb, 0, sizeof(struct stat));
		status = stat(path, &sb);
		if (status == 0)
		{
			/* StoreData exists: open and read last xid */
			sdata->storedata = fopen(path, "r+");
			if (sdata->storedata == NULL)
			{
				free(path);
				return ASL_STATUS_FAILED;
			}

			if (fread(&xid, sizeof(uint64_t), 1, sdata->storedata) != 1)
			{
				free(path);
				fclose(sdata->storedata);
				sdata->storedata = NULL;
				return ASL_STATUS_READ_FAILED;
			}
		}
		else if (errno == ENOENT)
		{
			/* StoreData does not exist: create it */
			sdata->storedata = fopen(path, "w");
			if (sdata->storedata == NULL)
			{
				free(path);
				return ASL_STATUS_FAILED;
			}

#if !TARGET_IPHONE_SIMULATOR
			if (chown(path, r->dst->uid[0], r->dst->gid[0]) != 0)
			{
				free(path);
				return ASL_STATUS_WRITE_FAILED;
			}
#endif
		}
		else
		{
			/* Unexpected stat error */
			free(path);
			return ASL_STATUS_FAILED;
		}

		sdata->monitor = dispatch_source_create(DISPATCH_SOURCE_TYPE_VNODE, fileno(sdata->storedata), DISPATCH_VNODE_DELETE, asl_action_queue);
		if (sdata->monitor != NULL)
		{
			dispatch_source_set_event_handler(sdata->monitor, ^{ _act_dst_close(r); });
			dispatch_resume(sdata->monitor);
		}

		free(path);
	}
	else
	{
		rewind(sdata->storedata);
		if (fread(&xid, sizeof(uint64_t), 1, sdata->storedata) != 1)
		{
			fclose(sdata->storedata);
			sdata->storedata = NULL;
			return ASL_STATUS_READ_FAILED;
		}
	}

	xid = asl_core_ntohq(xid);
	xid++;
	sdata->next_id = xid;

	xid = asl_core_htonq(xid);
	rewind(sdata->storedata);
	status = fwrite(&xid, sizeof(uint64_t), 1, sdata->storedata);
	if (status != 1)
	{
		fclose(sdata->storedata);
		sdata->storedata = NULL;
		return ASL_STATUS_WRITE_FAILED;
	}

	memset(&ctm, 0, sizeof(struct tm));

	if (localtime_r((const time_t *)&tick, &ctm) == NULL) return ASL_STATUS_FAILED;
	if ((sdata->store != NULL) && (sdata->p_year == ctm.tm_year) && (sdata->p_month == ctm.tm_mon) && (sdata->p_day == ctm.tm_mday))
	{
		return ASL_STATUS_OK;
	}

	if (sdata->store != NULL) asl_file_close(sdata->store);
	sdata->store = NULL;
	free(r->dst->fname);
	r->dst->fname = NULL;

    r->dst->stamp = tick;

	sdata->p_year = ctm.tm_year;
	sdata->p_month = ctm.tm_mon;
	sdata->p_day = ctm.tm_mday;

	asprintf(&(r->dst->fname), "%s/%d.%02d.%02d.asl", r->dst->path, ctm.tm_year + 1900, ctm.tm_mon + 1, ctm.tm_mday);
	if (r->dst->fname == NULL) return ASL_STATUS_NO_MEMORY;
	mask = umask(0);

	status = ASL_STATUS_OK;
	if (sdata->store == NULL) {
#if TARGET_IPHONE_SIMULATOR
		uid_t uid = -1;
		gid_t gid = -1;
#else
		uid_t uid = r->dst->uid[0];
		gid_t gid = r->dst->gid[0];
#endif
		status = asl_file_open_write(r->dst->fname, (r->dst->mode & 0666), uid, gid, &(sdata->store));
	}
	umask(mask);

	if (status != ASL_STATUS_OK) return status;

	if (fseek(sdata->store->store, 0, SEEK_END) != 0) return ASL_STATUS_FAILED;

	return ASL_STATUS_OK;
}

static void
_asl_action_store_data_free(asl_action_store_data_t *sdata)
{
	if (sdata == NULL) return;

	if (sdata->store != NULL) asl_file_close(sdata->store);
	sdata->store = NULL;

	if (sdata->storedata != NULL) fclose(sdata->storedata);
	sdata->storedata = NULL;

	free(sdata);
}

static void
_asl_action_file_data_free(asl_action_file_data_t *fdata)
{
	if (fdata == NULL) return;

	if (fdata->dup_timer != NULL)
	{
		if (fdata->last_count == 0)
		{
			/*
			 * The timer exists, but last_count is zero, so the timer is suspended.
			 * Sources must not be released in when suspended.
			 * So we resume it so that we can release it.
			 */
			dispatch_resume(fdata->dup_timer);
		}

		dispatch_release(fdata->dup_timer);
	}

	free(fdata->last_msg);
	if (fdata->fd >= 0) close(fdata->fd);
	free(fdata);
}

static void
_asl_action_set_param_data_free(asl_action_set_param_data_t *spdata)
{
	if (spdata != NULL) notify_cancel(spdata->token);
	free(spdata);
}

static void
_asl_action_save_failed(const char *where, asl_out_module_t *m, asl_out_rule_t *r, uint32_t status)
{
	if (r->dst->flags & MODULE_FLAG_SOFT_WRITE) return;

	r->dst->fails++;
	asldebug("%s: %s save to %s failed: %s\n", where, m->name, r->dst->path, asl_core_error(status));

	/* disable further activity after multiple failures */
	if (r->dst->fails > MAX_FAILURES)
	{
		char *str = NULL;
		asprintf(&str, "[Sender syslogd] [Level 3] [PID %u] [Facility syslog] [Message Disabling module %s writes to %s following %u failures (%s)]", global.pid, m->name, r->dst->path, r->dst->fails, asl_core_error(status));
		internal_log_message(str);
		free(str);

		if (r->action == ACTION_FILE) _asl_action_file_data_free((asl_action_file_data_t *)r->dst->private);
		else _asl_action_store_data_free((asl_action_store_data_t *)r->dst->private);

		r->dst->private = NULL;
		r->action = ACTION_NONE;
	}
}

static int
_act_store_file(asl_out_module_t *m, asl_out_rule_t *r, aslmsg msg)
{
	asl_action_store_data_t *sdata;
	uint32_t status;
	uint64_t mid;

	if (r == NULL) return ACTION_STATUS_ERROR;
	if (r->dst == NULL) return ACTION_STATUS_ERROR;
	if (r->dst->private == NULL) return ACTION_STATUS_ERROR;

	sdata = (asl_action_store_data_t *)r->dst->private;

	/* check dst for file_max & etc */
	if (r->dst->flags & MODULE_FLAG_ROTATE)
	{
		if (asl_out_dst_checkpoint(r->dst, CHECKPOINT_TEST) != 0)
		{
			_act_dst_close(r);
			asl_trigger_aslmanager();
		}
	}

	status = _act_store_file_setup(m, r);
	if (status == ASL_STATUS_OK)
	{
		sdata->last_time = time(NULL);

		r->dst->fails = 0;
		mid = sdata->next_id;

		/* save message to file and update dst size */
		status = asl_file_save(sdata->store, msg, &mid);
		if (status == ASL_STATUS_OK) r->dst->size = sdata->store->file_size;
	}

	if (status != ASL_STATUS_OK)
	{
		_asl_action_save_failed("_act_store_file", m, r, status);
		return ACTION_STATUS_ERROR;
	}

	return ACTION_STATUS_OK;
}

static int
_act_store_dir(asl_out_module_t *m, asl_out_rule_t *r, aslmsg msg)
{
	asl_action_store_data_t *sdata;
	uint32_t status;
	uint64_t mid;
	const char *val;
	time_t tick;

	if (r == NULL) return ACTION_STATUS_ERROR;
	if (r->dst == NULL) return ACTION_STATUS_ERROR;
	if (r->dst->private == NULL) return ACTION_STATUS_ERROR;

	sdata = (asl_action_store_data_t *)r->dst->private;

	val = asl_get(msg, ASL_KEY_TIME);
	if (val == NULL) return ACTION_STATUS_ERROR;

	/* check dst for file_max & etc */
	if (asl_out_dst_checkpoint(r->dst, CHECKPOINT_TEST) != 0)
	{
		_act_dst_close(r);
		asl_trigger_aslmanager();
	}

	tick = atol(val);

	status = _act_store_dir_setup(m, r, tick);
	if (status == ASL_STATUS_OK)
	{
		sdata->last_time = time(NULL);

		r->dst->fails = 0;
		mid = sdata->next_id;
		status = asl_file_save(sdata->store, msg, &mid);
		if (status == ASL_STATUS_OK) r->dst->size = sdata->store->file_size;
	}

	if (status != ASL_STATUS_OK)
	{
		_asl_action_save_failed("_act_store_dir", m, r, status);
		return ACTION_STATUS_ERROR;
	}

	return ACTION_STATUS_OK;
}

static void
_act_store_final(asl_out_module_t *m, asl_out_rule_t *r, aslmsg msg)
{
	if (r->action == ACTION_ASL_DIR) _act_store_dir(m, r, msg);
	else _act_store_file(m, r, msg);
}

/*
 * Save a message to an ASL format file (ACTION_ASL_FILE)
 * or to an ASL directory (ACTION_ASL_DIR).
 */
static void
_act_store(asl_out_module_t *m, asl_out_rule_t *r, aslmsg msg)
{
	if (r == NULL) return;
	if (msg == NULL) return;
	if (m == NULL) return;
	if ((m->flags & MODULE_FLAG_ENABLED) == 0) return;
	if (r->dst == NULL) return;

	if (r->dst->flags & MODULE_FLAG_HAS_LOGGED) return;
	r->dst->flags |= MODULE_FLAG_HAS_LOGGED;

#if TARGET_OS_EMBEDDED
	if (r->dst->flags & MODULE_FLAG_CRASHLOG)
	{
		_crashlog_queue_check();
		asl_msg_retain((asl_msg_t *)msg);
		dispatch_async(crashlog_queue, ^{
			_act_store_final(m, r, msg);
			asl_msg_release((asl_msg_t *)msg);
		});
		return;
	}
#endif

	_act_store_final(m, r, msg);
}

static int
_send_repeat_msg(asl_out_rule_t *r)
{
	asl_action_file_data_t *fdata;
	char vt[32], *msg;
	int len, status;
	time_t now = time(NULL);

	if (r == NULL) return -1;
	if (r->dst == NULL) return -1;
	if (r->dst->private == NULL) return -1;

	fdata = (asl_action_file_data_t *)r->dst->private;

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
	fdata->last_time = now;
	if (msg == NULL) return -1;

	if (fdata->fd < 0) fdata->fd = asl_out_dst_file_create_open(r->dst);

	len = strlen(msg);
	status = write(fdata->fd, msg, len);
	free(msg);

	if ((status < 0) || (status < len))
	{
		asldebug("%s: error writing repeat message (%s): %s\n", MY_ID, r->dst->path, strerror(errno));
		return -1;
	}

	return 0;
}

static int
_act_file_open(asl_out_module_t *m, asl_out_rule_t *r)
{
	asl_action_file_data_t *fdata;

	if (r == NULL) return -1;
	if (r->dst == NULL) return -1;
	if (r->dst->private == NULL) return -1;

	fdata = (asl_action_file_data_t *)r->dst->private;
	if (fdata->fd < 0)
	{
		fdata->fd = asl_out_dst_file_create_open(r->dst);
		if (fdata->fd < 0)
		{
			/*
			 * lazy path creation: create path and retry
			 * asl_out_dst_file_create_open doesn not create the path
			 * so we do it here.
			 */
			asl_out_mkpath(r);
			fdata->fd = asl_out_dst_file_create_open(r->dst);
		}

		if (fdata->fd >= 0)
		{
			fdata->monitor = dispatch_source_create(DISPATCH_SOURCE_TYPE_VNODE, fdata->fd, DISPATCH_VNODE_DELETE, asl_action_queue);
			if (fdata->monitor != NULL)
			{
				dispatch_source_set_event_handler(fdata->monitor, ^{ _act_dst_close(r); });
				dispatch_resume(fdata->monitor);
			}
		}
	}

	return fdata->fd;
}

static void
_start_cycling()
{
	struct timespec midnight;
	struct tm t;
	time_t x;

	x = time(NULL);

	if (checkpoint_timer != NULL) return;

	localtime_r(&x, &t);

	t.tm_sec = 0;
	t.tm_min = 0;
	t.tm_hour = 0;
	t.tm_mday++;

	x = mktime(&t);
	midnight.tv_sec = x;
	midnight.tv_nsec = 0;

	checkpoint_timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, asl_action_queue);
	dispatch_source_set_timer(checkpoint_timer, dispatch_walltime(&midnight, 0), NSEC_PER_SEC * SEC_PER_DAY, 0);
	dispatch_source_set_event_handler(checkpoint_timer, ^{ _act_file_checkpoint_all(CHECKPOINT_FORCE); });
	dispatch_resume(checkpoint_timer);
}

/* check if a module path (mpath) matches a user path (upath) */
static bool
_act_file_equal(const char *mpath, const char *upath)
{
	const char *slash;

	/* NULL upath means user wants to match all files */
	if (upath == NULL) return true;

	if (mpath == NULL) return false;

	/* check for exact match */
	if (!strcmp(mpath, upath)) return true;

	/* upath may be the last component of mpath */
	slash = strrchr(mpath, '/');
	if (slash == NULL) return false;

	if (!strcmp(slash + 1, upath)) return true;
	return false;
}

static int
_act_file_checkpoint(asl_out_module_t *m, const char *path, uint32_t force)
{
	asl_out_rule_t *r;
	int did_checkpoint = 0;

	if (m == NULL) return 0;


	for (r = m->ruleset; r != NULL; r = r->next)
	{
		if ((r->action == ACTION_FILE) || (r->action == ACTION_ASL_FILE))
		{
			if (r->dst->flags & MODULE_FLAG_ROTATE)
			{
				if (_act_file_equal(r->dst->path, path))
				{
					if (force & CHECKPOINT_CRASH)
					{
						if (r->dst->flags & MODULE_FLAG_CRASHLOG)
						{
							if (asl_out_dst_checkpoint(r->dst, CHECKPOINT_FORCE) > 0)
							{
								did_checkpoint = 1;
								_act_dst_close(r);
							}
						}
					}
					else
					{
						if (asl_out_dst_checkpoint(r->dst, force) > 0)
						{
							did_checkpoint = 1;
							_act_dst_close(r);
						}
					}
				}
			}
		}
	}

	return did_checkpoint;
}

static int
_act_file_checkpoint_all(uint32_t force)
{
	asl_out_module_t *m;
	int did_checkpoint = 0;

	for (m = global.asl_out_module; m != NULL; m = m->next)
	{
		if (_act_file_checkpoint(m, NULL, force) > 0) did_checkpoint = 1;
	}

	asl_trigger_aslmanager();

	return did_checkpoint;
}

static void
_act_file_final(asl_out_module_t *m, asl_out_rule_t *r, aslmsg msg)
{
	asl_action_file_data_t *fdata;
	int is_dup;
	uint32_t len, msg_hash = 0;
	char *str;
	time_t now;

	/*
	 * If print format is std, bsd, or msg, then skip messages with
	 * no ASL_KEY_MSG, or without a value for it.
	 */
	if (r->dst->flags & MODULE_FLAG_STD_BSD_MSG)
	{
		const char *msgval = NULL;
		if (asl_msg_lookup((asl_msg_t *)msg, ASL_KEY_MSG, &msgval, NULL) != 0) return;
		if (msgval == NULL) return;
	}

	fdata = (asl_action_file_data_t *)r->dst->private;

	now = time(NULL);

	is_dup = 0;

	str = asl_format_message((asl_msg_t *)msg, r->dst->fmt, r->dst->tfmt, ASL_ENCODE_SAFE, &len);

	if (r->dst->flags & MODULE_FLAG_COALESCE)
	{
		if (fdata->dup_timer == NULL)
		{
			/* create a timer to flush dups on this file */
			fdata->dup_timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, asl_action_queue);
			dispatch_source_set_event_handler(fdata->dup_timer, ^{ _send_repeat_msg(r); });
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
		if (_act_file_open(m, r) < 0)
		{
			_asl_action_save_failed("_act_file", m, r, ASL_STATUS_FAILED);
			free(str);
			return;
		}
		else
		{
			r->dst->fails = 0;
		}

		/*
		 * The current message is not a duplicate.  If fdata->last_count > 0
		 * we need to write a "last message repeated N times" log entry.
		 * _send_repeat_msg will free last_msg and do nothing if
		 * last_count == 0, but we test and free here to avoid a function call.
		 */
		if (fdata->last_count > 0)
		{
			_send_repeat_msg(r);
		}
		else
		{
			free(fdata->last_msg);
			fdata->last_msg = NULL;
		}

		/* check dst for file_max & etc */
		if (r->dst->flags & MODULE_FLAG_ROTATE)
		{
			int ckpt = asl_out_dst_checkpoint(r->dst, CHECKPOINT_TEST);
			if (ckpt != 0)
			{
				_act_dst_close(r);
				asl_trigger_aslmanager();

				if (_act_file_open(m, r) < 0)
				{
					_asl_action_save_failed("_act_file", m, r, ASL_STATUS_FAILED);
					free(str);
					return;
				}
				else
				{
					r->dst->fails = 0;
				}
			}
		}

		if (str != NULL) fdata->last_msg = strdup(str + 16);

		fdata->last_hash = msg_hash;
		fdata->last_count = 0;
		fdata->last_time = now;

		if ((str != NULL) && (len > 1))
		{
			/* write line to file and update dst size */
			size_t bytes = write(fdata->fd, str, len - 1);
			if (bytes > 0) r->dst->size += bytes;
		}
	}

	free(str);
}

static void
_act_file(asl_out_module_t *m, asl_out_rule_t *r, aslmsg msg)
{
	if (r == NULL) return;
	if (msg == NULL) return;
	if (m == NULL) return;
	if ((m->flags & MODULE_FLAG_ENABLED) == 0) return;
	if (r->dst == NULL) return;
	if (r->dst->private == NULL) return;

	if (r->dst->flags & MODULE_FLAG_HAS_LOGGED) return;

	r->dst->flags |= MODULE_FLAG_HAS_LOGGED;

#if TARGET_OS_EMBEDDED
	if (r->dst->flags & MODULE_FLAG_CRASHLOG)
	{
		_crashlog_queue_check();
		asl_msg_retain((asl_msg_t *)msg);
		dispatch_async(crashlog_queue, ^{
			_act_file_final(m, r, msg);
			asl_msg_release((asl_msg_t *)msg);
		});
		return;
	}
#endif

	_act_file_final(m, r, msg);
}

static void
_act_forward(asl_out_module_t *m, asl_out_rule_t *r, aslmsg msg)
{
	/* To do: <rdar://problem/6130747> Add a "forward" action to asl.conf */
}

static void
_act_control(asl_out_module_t *m, asl_out_rule_t *r, aslmsg msg)
{
	const char *p;

	if (m == NULL) return;
	if (r == NULL) return;
	p = asl_get(msg, ASL_KEY_MODULE);

	if (r->options == NULL) return;

	if (!strcmp(r->options, "enable"))
	{
		m->flags |= MODULE_FLAG_ENABLED;
	}
	else if (!strcmp(r->options, "disable"))
	{
		m->flags &= ~MODULE_FLAG_ENABLED;
	}
	else if ((!strcmp(r->options, "checkpoint")) || (!strcmp(r->options, "rotate")))
	{
		_act_file_checkpoint(m, NULL, CHECKPOINT_FORCE);
	}
}

static void
_send_to_asl_store(aslmsg msg)
{
	if ((global.asl_out_module != NULL) && ((global.asl_out_module->flags & MODULE_FLAG_ENABLED) == 0)) return;

	if (store_has_logged) return;
	store_has_logged = true;

	db_save_message(msg);
}

static int
_asl_out_process_message(asl_out_module_t *m, aslmsg msg)
{
	asl_out_rule_t *r;

	if (m == NULL) return 1;
	if (msg == NULL) return 1;

	/* reset flag bit used for duplicate avoidance */
	for (r = m->ruleset; r != NULL; r = r->next)
	{
		if ((r->action == ACTION_FILE) || (r->action == ACTION_ASL_DIR) || (r->action == ACTION_ASL_FILE))
		{
			if (r->dst != NULL) r->dst->flags &= MODULE_FLAG_CLEAR_LOGGED;
		}
	}

	for (r = m->ruleset; r != NULL; r = r->next)
	{
		if (r->query == NULL) continue;

		/* ACTION_SET_FILE, ACTION_SET_PLIST, and ACTION_SET_PROF are handled independently  */
		if ((r->action == ACTION_SET_FILE) || (r->action == ACTION_SET_PLIST) || (r->action == ACTION_SET_PROF)) continue;

		/*
		 * ACTION_CLAIM during processing is a filter.  It will only be here if the option "only"
		 * was supplied.  In this case we test the message against the query.  If it does not
		 * match, we skip the message.
		 */
		if (r->action == ACTION_CLAIM)
		{
			if ((asl_msg_cmp(r->query, (asl_msg_t *)msg) != 1)) return 0;
		}

		if ((asl_msg_cmp(r->query, (asl_msg_t *)msg) == 1))
		{
			if (r->action == ACTION_NONE) continue;
			else if (r->action == ACTION_IGNORE) return 1;
			else if (r->action == ACTION_SKIP) return 0;
			else if (r->action == ACTION_ASL_STORE) _send_to_asl_store(msg);
			else if (r->action == ACTION_ACCESS) _act_access_control(m, r, msg);
			else if (r->action == ACTION_NOTIFY) _act_notify(m, r);
			else if (r->action == ACTION_BROADCAST) _act_broadcast(m, r, msg);
			else if (r->action == ACTION_FORWARD) _act_forward(m, r, msg);
			else if (r->action == ACTION_CONTROL) _act_control(m, r, msg);
			else if (r->action == ACTION_SET_PARAM) _act_out_set_param(m, r->options, true);
			else if ((r->action == ACTION_ASL_FILE) || (r->action == ACTION_ASL_DIR)) _act_store(m, r, msg);
			else if (r->action == ACTION_FILE) _act_file(m, r, msg);
		}
	}

	return 0;
}

void
asl_out_message(aslmsg msg)
{
	OSAtomicIncrement32(&global.asl_queue_count);
	asl_msg_retain((asl_msg_t *)msg);

	dispatch_async(asl_action_queue, ^{
		int ignore = 0;
		const char *p;
		time_t now = time(NULL);
		asl_out_module_t *m = global.asl_out_module;

		store_has_logged = false;

		p = asl_get(msg, ASL_KEY_MODULE);
		if (p == NULL)
		{
			if ((action_asl_store_count == 0) || (asl_check_option(msg, ASL_OPT_STORE) == 1)) _send_to_asl_store(msg);

			ignore = _asl_out_process_message(m, msg);
			if (ignore == 0)
			{
				if (m != NULL) m = m->next;
				while (m != NULL)
				{
					_asl_out_process_message(m, msg);
					m = m->next;
				}
			}
		}
		else 
		{
			if (m != NULL) m = m->next;
			while (m != NULL)
			{
				if (!strcmp(p, m->name)) _asl_out_process_message(m, msg);
				m = m->next;
			}
		}

		asl_msg_release((asl_msg_t *)msg);
		OSAtomicDecrement32(&global.asl_queue_count);

		if ((now - sweep_time) >= IDLE_CLOSE)
		{
			_asl_action_close_idle_files(IDLE_CLOSE);
			sweep_time = now;
		}
	});
}

static char *
_asl_action_profile_test(asl_out_module_t *m, asl_out_rule_t *r)
{
	const char *ident;
	asl_msg_t *profile;
	bool eval;

	/* ident is first message key */
	asl_msg_fetch((asl_msg_t *)r->query, 0, &ident, NULL, NULL);
	if (ident == NULL)
	{
		r->action = ACTION_NONE;
		return NULL;
	}

	profile = configuration_profile_to_asl_msg(ident);
	eval = (asl_msg_cmp(r->query, profile) == 1);
	_act_out_set_param(m, r->options, eval);
	asl_msg_release(profile);

	return strdup(ident);
}

static const char *
_asl_action_file_test(asl_out_module_t *m, asl_out_rule_t *r)
{
	const char *path;
	struct stat sb;
	int status;
	bool eval;

	/* path is first message key */
	asl_msg_fetch((asl_msg_t *)r->query, 0, &path, NULL, NULL);
	if (path == NULL)
	{
		r->action = ACTION_NONE;
		return NULL;
	}

	memset(&sb, 0, sizeof(struct stat));
	status = stat(path, &sb);
	eval = (status == 0);
	_act_out_set_param(m, r->options, eval);

	return path;
}

static void
_asl_action_handle_file_change_notification(int t)
{
	asl_out_module_t *m;
	asl_out_rule_t *r;

	for (m = global.asl_out_module; m != NULL; m = m->next)
	{
		for (r = m->ruleset; r != NULL; r = r->next)
		{
			if (r->action == ACTION_SET_FILE)
			{
				asl_action_set_param_data_t *spdata = (asl_action_set_param_data_t *)r->private;
				if ((spdata != NULL) && (spdata->token == t))
				{
					_asl_action_file_test(m, r);
					return;
				}
			}
			else if (r->action == ACTION_SET_PLIST)
			{
				asl_action_set_param_data_t *spdata = (asl_action_set_param_data_t *)r->private;
				if ((spdata != NULL) && (spdata->token == t))
				{
					char *str = _asl_action_profile_test(m, r);
					free(str);
					return;
				}
			}
			else if (r->action == ACTION_SET_PROF)
			{
				asl_action_set_param_data_t *spdata = (asl_action_set_param_data_t *)r->private;
				if ((spdata != NULL) && (spdata->token == t))
				{
					char *str = _asl_action_profile_test(m, r);
					free(str);
					return;
				}
			}
		}
	}

	asl_out_module_free(m);
}

static void
_asl_action_post_process_rule(asl_out_module_t *m, asl_out_rule_t *r)
{
	if ((m == NULL) || (r == NULL)) return;

	if (m != global.asl_out_module)
	{
		/* check if any previous module has used this destination */
		asl_out_module_t *n;
		bool search = true;

		if ((r->dst != NULL) && (r->dst->path != NULL))
		{
			for (n = global.asl_out_module; search && (n != NULL) && (n != m); n = n->next)
			{
				asl_out_rule_t *s;
				for (s = n->ruleset; search && (s != NULL); s = s->next)
				{
					if (s->action == ACTION_OUT_DEST)
					{
						if ((s->dst != NULL) && (s->dst->path != NULL) && (!strcmp(r->dst->path, s->dst->path)))
						{
							/* rule r of module m is using previously used dst of rule s of module n */
							asl_out_dst_data_release(r->dst);
							r->dst = NULL;

							if (r->action == ACTION_OUT_DEST)
							{
								char *str = NULL;
								asprintf(&str, "[Sender syslogd] [Level 5] [PID %u] [Message Configuration Notice:\nASL Module \"%s\" sharing output destination \"%s\" with ASL Module \"%s\".\nOutput parameters from ASL Module \"%s\" override any specified in ASL Module \"%s\".] [UID 0] [GID 0] [Facility syslog]", global.pid, m->name, s->dst->path, n->name, n->name, m->name);
								internal_log_message(str);
								free(str);
							}
							else
							{
								r->dst = asl_out_dst_data_retain(s->dst);
							}

							search = false;
						}
					}
				}
			}
		}
	}

	if (r->action == ACTION_SET_PARAM)
	{
		if (r->query == NULL) _act_out_set_param(m, r->options, true);
	}
	else if (r->action == ACTION_CLAIM)
	{
		/* becomes ACTION_SKIP in com.apple.asl config */
		if (m != global.asl_out_module)
		{
			asl_out_rule_t *rule = (asl_out_rule_t *)calloc(1, sizeof(asl_out_rule_t));
			if (rule != NULL)
			{
				char *str = NULL;
				asprintf(&str, "[Sender syslogd] [Level 5] [PID %u] [Message Configuration Notice:\nASL Module \"%s\" claims selected messages.\nThose messages may not appear in standard system log files or in the ASL database.] [UID 0] [GID 0] [Facility syslog]", global.pid, m->name);
				internal_log_message(str);
				free(str);

				rule->query = asl_msg_copy(r->query);
				rule->action = ACTION_SKIP;
				rule->next = global.asl_out_module->ruleset;
				global.asl_out_module->ruleset = rule;
			}

			/*
			 * After adding ACTION_SKIP to com.apple.asl module, the claim becomes a no-op in this module
			 * UNLESS the claim includes the option "only".  In that case, the claim becomes a filter:
			 * any messages that DO NOT match the claim are skipped by this module.
			 */
			if (r->options == NULL) r->action = ACTION_NONE;
			else if (strcmp(r->options, "only") != 0) r->action = ACTION_NONE;
		}
	}
	else if (r->action == ACTION_ASL_STORE)
	{
		action_asl_store_count++;
	}
	else if (r->action == ACTION_ASL_DIR)
	{
		if (r->dst->private == NULL) r->dst->private = (asl_action_store_data_t *)calloc(1, sizeof(asl_action_store_data_t));
	}
	else if (r->action == ACTION_ASL_FILE)
	{
		if (r->dst->private == NULL)r->dst->private = (asl_action_store_data_t *)calloc(1, sizeof(asl_action_store_data_t));
	}
	else if (r->action == ACTION_FILE)
	{
		if (r->dst->private == NULL) r->dst->private = (asl_action_file_data_t *)calloc(1, sizeof(asl_action_file_data_t));
		if (r->dst->private != NULL) ((asl_action_file_data_t *)(r->dst->private))->fd = -1;
	}
	else if (r->action == ACTION_SET_PLIST)
	{
		char *ident =_asl_action_profile_test(m, r);
		char *notify_key = configuration_profile_create_notification_key(ident);
		free(ident);

		if (notify_key != NULL)
		{
			int status, token;
			asl_action_set_param_data_t *spdata;

			status = notify_register_dispatch(notify_key, &token, asl_action_queue, ^(int t){
				_asl_action_handle_file_change_notification(t);
			});

			free(notify_key);

			spdata = (asl_action_set_param_data_t *)calloc(1, sizeof(asl_action_set_param_data_t));
			if (spdata == NULL)
			{
				notify_cancel(token);
			}
			else
			{
				spdata->token = token;
				r->private = spdata;
			}
		}
	}
	else if (r->action == ACTION_SET_PROF)
	{
		char *ident =_asl_action_profile_test(m, r);
		char *notify_key = configuration_profile_create_notification_key(ident);
		free(ident);

		if (notify_key != NULL)
		{
			int status, token;
			asl_action_set_param_data_t *spdata;

			status = notify_register_dispatch(notify_key, &token, asl_action_queue, ^(int t){
				_asl_action_handle_file_change_notification(t);
			});

			free(notify_key);

			spdata = (asl_action_set_param_data_t *)calloc(1, sizeof(asl_action_set_param_data_t));
			if (spdata == NULL)
			{
				notify_cancel(token);
			}
			else
			{
				spdata->token = token;
				r->private = spdata;
			}
		}
	}
	else if (r->action == ACTION_SET_FILE)
	{
		char *notify_key;
		const char *path =_asl_action_file_test(m, r);

		if (path != NULL)
		{
			asprintf(&notify_key, "%s%s", NOTIFY_PATH_SERVICE, path);
			if (notify_key != NULL)
			{
				int status, token;
				asl_action_set_param_data_t *spdata;

				status = notify_register_dispatch(notify_key, &token, asl_action_queue, ^(int t){
					_asl_action_handle_file_change_notification(t);
				});

				free(notify_key);

				spdata = (asl_action_set_param_data_t *)calloc(1, sizeof(asl_action_set_param_data_t));
				if (spdata == NULL)
				{
					notify_cancel(token);
				}
				else
				{
					spdata->token = token;
					r->private = spdata;
				}
			}
		}
	}
}

static void
_asl_action_configure()
{
	asl_out_rule_t *r;
	asl_out_module_t *m;
	uint32_t flags = 0;

	if (global.asl_out_module == NULL) global.asl_out_module = asl_out_module_init();
	if (global.asl_out_module == NULL) return;

	if (global.debug != 0)
	{
		FILE *dfp;
		if (global.debug_file == NULL) dfp = fopen(_PATH_SYSLOGD_LOG, "a");
		else dfp = fopen(global.debug_file, "a");
		if (dfp != NULL)
		{
			for (m = global.asl_out_module; m != NULL; m = m->next)
			{
				fprintf(dfp, "module: %s%s\n", (m->name == NULL) ? "<unknown>" : m->name, (m->flags & MODULE_FLAG_LOCAL) ? " (local)" : "");
				asl_out_module_print(dfp, m);
				fprintf(dfp, "\n");
			}
			fclose(dfp);
		}
	}

	asldebug("%s: init\n", MY_ID);

	action_asl_store_count = 0;

	for (m = global.asl_out_module; m != NULL; m = m->next)
	{
		for (r = m->ruleset; r != NULL; r = r->next)
		{
			_asl_action_post_process_rule(m, r);
			if (r->dst != NULL) flags |= (r->dst->flags & (MODULE_FLAG_ROTATE | MODULE_FLAG_CRASHLOG));
		}
	}

	if (global.debug != 0)
	{
		FILE *dfp;
		if (global.debug_file == NULL) dfp = fopen(_PATH_SYSLOGD_LOG, "a");
		else dfp = fopen(global.debug_file, "a");
		if (dfp != NULL)
		{
			for (m = global.asl_out_module; m != NULL; m = m->next)
			{
				fprintf(dfp, "module: %s%s\n", (m->name == NULL) ? "<unknown>" : m->name, (m->flags & MODULE_FLAG_LOCAL) ? " (local)" : "");
				asl_out_module_print(dfp, m);
				fprintf(dfp, "\n");
			}
			fclose(dfp);
		}
	}

	sweep_time = time(NULL);

	if (flags & MODULE_FLAG_ROTATE)
	{
		_act_file_checkpoint_all(CHECKPOINT_TEST);
		if (checkpoint_timer == NULL) _start_cycling();
	}

#if TARGET_OS_EMBEDDED
	if (flags & MODULE_FLAG_CRASHLOG) _crashlog_sentinel_init();
#endif
}

int
asl_action_init(void)
{
	static dispatch_once_t once;

	dispatch_once(&once, ^{
		asl_action_queue = dispatch_queue_create("ASL Action Queue", NULL);
#if TARGET_OS_EMBEDDED
		crashlog_queue = dispatch_queue_create("iOS CrashLog Queue", NULL);
		notify_register_dispatch(CRASH_MOVER_WILL_START_NOTIFICATION, &crashmover_token, asl_action_queue, ^(int unused){
			if (crashmover_state == 0)
			{
				asldebug("CrashMover active: suspending crashlog queue and closing files\n");
				crashmover_state = time(NULL);
				dispatch_suspend(crashlog_queue);
				_asl_action_close_idle_files(0);
			}
		});
#endif
	});

	_asl_action_configure();

	return 0;
}

/*
 * Free a module.
 */
static void
_asl_action_free_modules(asl_out_module_t *m)
{
	asl_out_rule_t *r;
	asl_out_module_t *x;

	/*
	 * asl_common frees a list of modules with asl_out_module_free.
	 * This loop frees the private data attached some modules.
	 */
	for (x = m; x != NULL; x = x->next)
	{
		for (r = x->ruleset; r != NULL; r = r->next)
		{
			if ((r->action == ACTION_ASL_FILE) || (r->action == ACTION_ASL_DIR))
			{
				if (r->dst != NULL)
				{
					_asl_action_store_data_free((asl_action_store_data_t *)r->dst->private);
					r->dst->private = NULL;
				}
			}
			else if (r->action == ACTION_FILE)
			{
				if (r->dst != NULL)
				{
					asl_action_file_data_t *fdata = (asl_action_file_data_t *)r->dst->private;
					if (fdata != NULL)
					{
						/* flush repeat message if necessary */
						if (fdata->last_count > 0) _send_repeat_msg(r);
						_asl_action_file_data_free(fdata);
						r->dst->private = NULL;
					}
				}
			}
			else if (r->action == ACTION_SET_PLIST)
			{
				_asl_action_set_param_data_free((asl_action_set_param_data_t *)r->private);
			}
			else if (r->action == ACTION_SET_PROF)
			{
				_asl_action_set_param_data_free((asl_action_set_param_data_t *)r->private);
			}
		}
	}

	asl_out_module_free(m);
}

static int
_asl_action_close_internal(void)
{
#if TARGET_OS_EMBEDDED
	dispatch_source_cancel(crashlog_sentinel_src);
	dispatch_release(crashlog_sentinel_src);
	crashlog_sentinel_src = NULL;
	close(crashlog_sentinel_fd);
	if (crashmover_state != 0)
	{
		dispatch_resume(crashlog_queue);
		crashmover_state = 0;
	}

	/* wait for the crashlog_queue to flush before _asl_action_free_modules() */
	dispatch_sync(crashlog_queue, ^{ crashlog_sentinel_fd = -1; });
#endif

	_asl_action_free_modules(global.asl_out_module);
	global.asl_out_module = NULL;
	sweep_time = time(NULL);

	return 0;
}

static void
_asl_action_close_idle_files(time_t idle_time)
{
	asl_out_module_t *m;
	time_t now = time(NULL);

	for (m = global.asl_out_module; m != NULL; m = m->next)
	{
		asl_out_rule_t *r;

		for (r = m->ruleset; r != NULL; r = r->next)
		{
			if (idle_time == 0)
			{
				if ((r->dst != NULL) && (r->dst->flags & MODULE_FLAG_CRASHLOG))
				{
					_act_dst_close(r);
					if (r->action != ACTION_ASL_DIR) asl_out_dst_checkpoint(r->dst, CHECKPOINT_FORCE);
				}
			}
			else if ((r->action == ACTION_ASL_DIR) || (r->action == ACTION_ASL_FILE))
			{
				if (r->dst != NULL)
				{
					asl_action_store_data_t *sdata = (asl_action_store_data_t *)r->dst->private;
					if ((sdata != NULL) && (sdata->store != NULL) && ((now - sdata->last_time) >= idle_time)) _act_dst_close(r);
				}
			}
			else if (r->action == ACTION_FILE)
			{
				if (r->dst != NULL)
				{
					asl_action_file_data_t *fdata = (asl_action_file_data_t *)r->dst->private;
					if ((fdata != NULL) && (fdata->fd >= 0) && ((now - fdata->last_time) >= idle_time)) _act_dst_close(r);
				}
			}
		}
	}
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

asl_out_module_t *
_asl_action_module_with_name(const char *name)
{
	asl_out_module_t *m;

	if (global.asl_out_module == NULL) return NULL;
	if (name == NULL) return global.asl_out_module;

	for (m = global.asl_out_module; m != NULL; m = m->next)
	{
		if ((m->name != NULL) && (!strcmp(m->name, name))) return m;
	}

	return NULL;
}

/*
 * called from control_message 
 * Used to control modules dynamically.
 * Line format "@ module param [value ...]"
 *
 * Note this is synchronous on asl_action queue.
 */
int
asl_action_control_set_param(const char *s)
{
	__block char **l;
	__block char *p;
	uint32_t count = 0;

	if (s == NULL) return -1;
	if (s[0] == '\0') return 0;

	/* skip '@' and whitespace */
	if (*s == '@') s++;
	while ((*s == ' ') || (*s == '\t')) s++;

	l = explode(s, " \t");
	if (l != NULL) for (count = 0; l[count] != NULL; count++);

	/* at least 2 parameters (l[0] = module, l[1] = param) required */
	if (count < 2) return -1;

	if (global.asl_out_module == NULL)
	{
		asldebug("asl_action_control_set_param: no modules loaded\n");
		return -1;
	}

	/* create / modify a module */
	if ((!strcasecmp(l[1], "define")) && (strcmp(l[0], "*")))
	{
		p = strdup(s);
		if (p == NULL)
		{
			asldebug("asl_action_control_set_param: memory allocation failed\n");
			return -1;
		}

		dispatch_sync(asl_action_queue, ^{
			asl_out_module_t *m;
			asl_out_rule_t *r;

			/* skip name, whitespace, "define" */
			while ((*p != ' ') && (*p != '\t')) p++;
			while ((*p == ' ') || (*p == '\t')) p++;
			while ((*p != ' ') && (*p != '\t')) p++;

			m = _asl_action_module_with_name(l[0]);
			if (m == NULL)
			{
				asl_out_module_t *x;

				m = asl_out_module_new(l[0]);
				for (x = global.asl_out_module; x->next != NULL; x = x->next);
				x->next = m;
			}

			r = asl_out_module_parse_line(m, p);
			if (r != NULL)
			{
				_asl_action_post_process_rule(m, r);
				if ((r->dst != NULL) && (r->dst->flags & MODULE_FLAG_ROTATE)) 
				{
					_act_file_checkpoint_all(CHECKPOINT_TEST);
					if (checkpoint_timer == NULL) _start_cycling();
				}
			}
		});

		free(p);
		free_string_list(l);
		return 0;
	}

	dispatch_sync(asl_action_queue, ^{
		uint32_t intval;
		int do_all = 0;
		asl_out_module_t *m;

		if (!strcmp(l[0], "*"))
		{
			do_all = 1;
			m = _asl_action_module_with_name(NULL);
		}
		else
		{
			m = _asl_action_module_with_name(l[0]);
		}

		while (m != NULL) 
		{
			if (!strcasecmp(l[1], "enable"))
			{
				intval = 1;

				/* don't do enable for ASL_MODULE_NAME if input name is "*" */
				if ((do_all == 0) || (strcmp(m->name, ASL_MODULE_NAME)))
				{
					/* @ module enable {0|1} */
					if (count > 2) intval = atoi(l[2]);

					if (intval == 0) m->flags &= ~MODULE_FLAG_ENABLED;
					else m->flags |= MODULE_FLAG_ENABLED;
				}
			}
			else if (!strcasecmp(l[1], "checkpoint"))
			{
				/* @ module checkpoint [file] */
				if (count > 2) _act_file_checkpoint(m, l[2], CHECKPOINT_FORCE);
				else _act_file_checkpoint(m, NULL, CHECKPOINT_FORCE);
			}

			if (do_all == 1) m = m->next;
			else m = NULL;
		}

	});

	free_string_list(l);
	return 0;
}

int
asl_action_file_checkpoint(const char *module, const char *path)
{
	/* Note this is synchronous on asl_action queue */
	dispatch_sync(asl_action_queue, ^{
		asl_out_module_t *m = _asl_action_module_with_name(module);
		_act_file_checkpoint(m, path, CHECKPOINT_FORCE);
	});

	return 0;
}
