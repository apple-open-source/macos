/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 2003 Apple Computer, Inc.  All Rights
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <fcntl.h>
#include <dirent.h>
#include "notify.h"
#include "daemon.h"
#include "service.h"
#include "file_watcher.h"

uint32_t
service_type(const char *name)
{
	name_info_t *n;
	uint32_t len;
	svc_info_t *svc;

	n = (name_info_t *)_nc_table_find(ns->name_table, name);
	if ((n != NULL) && (n->private != NULL))
	{
		svc = (svc_info_t *)n->private;
		return svc->type;
	}

	len = NOTIFY_SERVICE_NAME_LEN;
	if (strncmp(name, NOTIFY_SERVICE_NAME, len))
		return SERVICE_TYPE_NONE;

	if (strncmp(name + len, NOTIFY_SERVICE_PREFIX, NOTIFY_SERVICE_PREFIX_LEN))
		return SERVICE_TYPE_NONE;

	len += NOTIFY_SERVICE_PREFIX_LEN;

	if (!strncmp(name + len, NOTIFY_FILE_SERVICE, NOTIFY_FILE_SERVICE_LEN))
	return SERVICE_TYPE_FILE;

	return SERVICE_TYPE_NONE;
}

w_event_t *
service_get_event(svc_info_t *s)
{
	w_event_t *e;

	if (s == NULL) return NULL;
	if (s->type != SERVICE_TYPE_EVENT) return NULL;

	e = (w_event_t *)s->private;
	if (e == NULL) return NULL;
	if (e->type == EVENT_NULL) return NULL;

	s->private = e->next;
	return e;
}

static int
service_stat(const char *path, uid_t uid, gid_t gid)
{
	struct stat sb;
	gid_t g;
	int status;

	g = getegid();

	seteuid(uid);
	setegid(gid);

	status = stat(path, &sb);

	seteuid(0);
	setegid(g);

	if (status == 0) return 0;
	if (errno == EACCES) return NOTIFY_STATUS_NOT_AUTHORIZED;
	return NOTIFY_STATUS_INVALID_FILE;
}

static int
service_check_access(const char *path, int ftype, uid_t uid, gid_t gid)
{
	struct stat sb;
	char *p, *str;
	int status;

	if (uid == 0) return 0;
	if (path == NULL) return NOTIFY_STATUS_INVALID_REQUEST;

	/* Paths must be absolute */
	if (path[0] != '/') return NOTIFY_STATUS_INVALID_REQUEST;

	/* Root dir is readable */
	if (path[1] == '\0') return 0;

	if (ftype == FS_TYPE_NONE)
	{
		memset(&sb, 0, sizeof(struct stat));

		status = lstat(path, &sb);
		if (status != 0)
		{
			str = strdup(path);
			p = strrchr(str, '/');
			if (p == NULL)
			{
				free(str);
				return NOTIFY_STATUS_INVALID_REQUEST;
			}
			*p = '\0';
			status = service_check_access(str, FS_TYPE_DIR, uid, gid);
			free(str);
			return status;
		}
		else if ((sb.st_mode & S_IFMT) == S_IFDIR) ftype = FS_TYPE_DIR;
		else if ((sb.st_mode & S_IFMT) == S_IFREG) ftype = FS_TYPE_FILE;
		else if ((sb.st_mode & S_IFMT) == S_IFLNK) ftype = FS_TYPE_LINK;
	}

	if (ftype == FS_TYPE_NONE) return NOTIFY_STATUS_INVALID_REQUEST;

	if (ftype == FS_TYPE_FILE)
	{
		status = service_stat(path, uid, gid);
		return status;
	}

	if (ftype == FS_TYPE_DIR)
	{
		str = NULL;
		asprintf(&str, "%s/.", path);
		status = service_stat(str, uid, gid);
		free(str);
		return status;
	}

	if (ftype == FS_TYPE_LINK)
	{
		/* Allow monitoring a symlink if the user has access to the link's directory */
		
		str = strdup(path);
		p = strrchr(str, '/');
		if (p == NULL)
		{
			free(str);
			return NOTIFY_STATUS_INVALID_REQUEST;
		}
		*p = '\0';
		status = service_check_access(str, FS_TYPE_DIR, uid, gid);
		free(str);
		return status;
	}

	return NOTIFY_STATUS_INVALID_REQUEST;
}

int
service_open_file(int client_id, const char *name, const char *path, int flags, uid_t uid, gid_t gid)
{
	name_info_t *n;
	client_t *c;
	svc_info_t *s;
	watcher_t *w;
	file_watcher_t *f;
	int status;

	/*
	 * NB path may be NULL, since this routine may be used to switch
	 * the event stream for a client on or off (flags parameter)
	 */
	 
	/* Check access */
	if (path != NULL)
	{
		status = service_check_access(path, FS_TYPE_NONE, uid, gid);
		if (status != 0) return status;
	}

	/* Get watcher for this path if there is one */
	w = file_watcher_for_path(path);

	/* See if this name already has a file service */
	n = (name_info_t *)_nc_table_find(ns->name_table, name);
	if (n == NULL) return NOTIFY_STATUS_INVALID_NAME;
	if (n->private == NULL)
	{
		/* New service for this name */
		if (path == NULL) return NOTIFY_STATUS_INVALID_REQUEST;
		if (w == NULL) w = file_watcher_new(path);
		watcher_add_name(w, name);
		daemon_set_state(name, w->state);
		s = (svc_info_t *)calloc(1, sizeof(svc_info_t));
		s->type = SERVICE_TYPE_FILE;
		s->private = w;
		n->private = s;
	}
	else
	{
		s = (svc_info_t *)n->private;
		if (s->type != SERVICE_TYPE_FILE) return NOTIFY_STATUS_INVALID_REQUEST;

		w = (watcher_t *)s->private;
		f = (file_watcher_t *)w->sub;
		if (f == NULL) return NOTIFY_STATUS_FAILED;
		if (f->path == NULL) return NOTIFY_STATUS_FAILED;
		if ((path != NULL) && (strcmp(path, f->path))) return NOTIFY_STATUS_INVALID_REQUEST;
	}		

	if (client_id == 0) return NOTIFY_STATUS_OK;

	c = _nc_table_find_n(ns->client_table, client_id);
	if (c == NULL) return NOTIFY_STATUS_INVALID_REQUEST;

	if (c->info == NULL) return NOTIFY_STATUS_INVALID_REQUEST;

	if (flags == 0)
	{
		if (c->info->private == NULL) return NOTIFY_STATUS_OK;

		service_close((svc_info_t *)c->info->private, n->name);
		c->info->private = NULL;

		return NOTIFY_STATUS_OK;
	}

	if (c->info->private != NULL) return NOTIFY_STATUS_INVALID_REQUEST;

	s = (svc_info_t *)calloc(1, sizeof(svc_info_t));
	c->info->private = s;
	s->type = SERVICE_TYPE_EVENT;
	s->private = file_watcher_history(w);

	return NOTIFY_STATUS_OK;
}

int
service_open(int client_id, const char *name, int flags, uint32_t uid, uint32_t gid)
{
	uint32_t t;
    char *p;
 
	t = service_type(name);

	switch (t)
	{
		case SERVICE_TYPE_NONE:
			return NOTIFY_STATUS_OK;

		case SERVICE_TYPE_FILE:
			p = strchr(name, ':');
			if (p != NULL) p++;
			return service_open_file(client_id, name, p, flags, uid, gid);

		default: 
			return NOTIFY_STATUS_INVALID_REQUEST;
	}

	return NOTIFY_STATUS_INVALID_REQUEST;
}

void
service_close(svc_info_t *s, const char *name)
{
	watcher_t *w;
	w_event_t *e, *n;

	if (s == NULL) return;

	if (s->type == SERVICE_TYPE_NONE)
	{
		free(s);
		return;
	}

	if (s->type == SERVICE_TYPE_FILE)
	{
		w = (watcher_t *)s->private;
		watcher_remove_name(w, name);
		watcher_release(w);
		free(s);
		return;
	}

	if (s->type == SERVICE_TYPE_EVENT)
	{
		e = (w_event_t *)s->private;
		while (e != NULL)
		{
			n = e->next;
			w_event_release(e);
			e = n;
		}

		free(s);
		return;
	}
}
