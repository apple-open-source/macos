/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

#include <stdlib.h>
#include <asl.h>
#include <dirent.h>
#include <sys/param.h>
#include <sys/event.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include "file_watcher.h"
#include "daemon.h"

#define IDENT_INVAL ((uintptr_t)-1)

#define FS_CHECKED  0x01
#define FS_INITIAL  0x02
#define FS_REMOVED  0x04
#define FS_DEADLINK 0x08

void
file_watcher_printf(watcher_t *w, FILE *fp)
{
	file_watcher_t *f;
	w_event_t *e;

	if (w == NULL) return;

	if (w->type != WATCH_FILE) return;
	f = (file_watcher_t *)w->sub;
	if (f == NULL) return;

	fprintf(fp, "File Watcher 0x%08x\n", (uint32_t)f);
	if (f->ftype == FS_TYPE_FILE) fprintf(fp, "File: ");
	else if (f->ftype == FS_TYPE_DIR) fprintf(fp, "Dir: ");
	else if (f->ftype == FS_TYPE_LINK) fprintf(fp, "Link: ");
	else fprintf(fp, "Path: ");
	fprintf(fp, "%s\n", (f->path == NULL) ? "-nil-" : f->path);
	if (f->sb != NULL)
	{
		/* XXX print stat buf */
	}

	if (f->kqident != IDENT_INVAL) fprintf(fp, "KQ Ident: %u\n", (uint32_t)f->kqident);

	e = f->contents;
	if (e != NULL)
	{
		fprintf(fp, "Contents:\n");
		for (; e != NULL; e = e->next)
		{
			fprintf(fp, "\t%s %u %u\n", e->name, e->type, e->flags);
		}
	}

	if (f->parent != NULL)
		fprintf(fp, "Parent: %u 0x%08x\n", f->parent->wid, (uint32_t)f->parent);

	if (f->linktarget != NULL)
		fprintf(fp, "Link Target: %u 0x%08x\n", f->linktarget->wid, (uint32_t)f->linktarget);
}

static void
file_watcher_add_kq(file_watcher_t *f)
{
	kern_return_t status;
	struct kevent event;

	if (f == NULL) return;
	if (f->kqident != IDENT_INVAL) return;

#ifdef DEBUG
	log_message(ASL_LEVEL_DEBUG, "file_watcher_add_kq(%u, %s)", f->w->wid, f->path);
#endif

	event.ident = open(f->path, O_EVTONLY, 0);
	if (event.ident < 0) return;

	event.filter = EVFILT_VNODE;
	event.flags = EV_ADD | EV_CLEAR;
	event.fflags = NOTE_DELETE | NOTE_WRITE | NOTE_EXTEND | NOTE_ATTRIB | NOTE_LINK | NOTE_RENAME;
	event.data = NULL;
	event.udata = (void *)f->w->wid;

	f->kqident = event.ident;

	status = kevent(kq, &event, 1, NULL, 0, NULL);
	if (status != 0)
	{
		close(event.ident);
		return;
	}
}

static void
file_watcher_remove_kq(file_watcher_t *f)
{
	struct kevent event;
	kern_return_t status;

	if (f == NULL) return;
	if (f->kqident == IDENT_INVAL) return;

#ifdef DEBUG
	log_message(ASL_LEVEL_DEBUG, "file_watcher_remove_kq(%u, %s)", f->w->wid, f->path);
#endif

	event.ident = f->kqident;
	event.filter = EVFILT_VNODE;
	event.flags = EV_DELETE;
	event.fflags = 0;
	event.data = NULL;
	event.udata = NULL;

	status = kevent(kq, &event, 1, NULL, 0, NULL);
	if (status != 0)
	{
		log_message(ASL_LEVEL_ERR, "EV_DELETE failed for file watcher %u", f->w->wid);
	}

	close(f->kqident);
	f->kqident = IDENT_INVAL;
}	

static void
dir_contents_update(file_watcher_t *f, w_event_t *e)
{
	w_event_t *p, *x;
	int comp;

	if (f == NULL) return;
	if (e == NULL) return;
	if (e->name == NULL) return;

	if (e->type == EVENT_FILE_ADD)
	{
		x = w_event_new(e->name, e->type, 0);
		if (f->contents == NULL)
		{
			f->contents = x;
			return;
		}

		p = f->contents;
		comp = strcmp(p->name, x->name);
		if (comp == 0) return;
		if (comp > 0)
		{
			x->next = f->contents;
			f->contents = x;
			return;
		}

		while (p->next != NULL)
		{
			comp = strcmp(p->next->name, x->name);
			if (comp == 0) return;
			if (comp > 0)
			{
				x->next = p->next;
				p->next = x;
				return;
			}

			p = p->next;
		}

		p->next = x;
		return;
	}

	if (e->type == EVENT_FILE_DELETE)
	{
		if (f->contents == NULL) return;
		p = f->contents;
		if (strcmp(p->name, e->name) == 0)
		{
			f->contents = p->next;
			w_event_release(p);
			return;
		}

		while (p->next != NULL)
		{
			if (strcmp(p->next->name, e->name) == 0)
			{
				x = p->next;
				p->next = x->next;
				w_event_release(x);
				return;
			}

			p = p->next;
		}

		return;
	}
}

static int32_t
file_watcher_check_file(file_watcher_t *f, struct stat *oldsb, w_event_t **delta)
{
	w_event_t *e;
	int32_t achange, cchange, mchange, status;
	
	achange = 0;
	cchange = 0;
	mchange = 0;
	status = 0;

	if ((oldsb->st_atimespec.tv_sec != f->sb->st_atimespec.tv_sec) || (oldsb->st_atimespec.tv_nsec != f->sb->st_atimespec.tv_nsec)) achange = 1;

	if ((oldsb->st_ctimespec.tv_sec != f->sb->st_ctimespec.tv_sec) || (oldsb->st_ctimespec.tv_nsec != f->sb->st_ctimespec.tv_nsec)) cchange = 1;

	if ((oldsb->st_mtimespec.tv_sec != f->sb->st_mtimespec.tv_sec) || (oldsb->st_mtimespec.tv_nsec != f->sb->st_mtimespec.tv_nsec)) mchange = 1;

	if ((cchange != 0) || (mchange != 0)) status = 1;

	if ((f->ftype == FS_TYPE_DIR) || (f->ftype == FS_TYPE_FILE) || (f->ftype == FS_TYPE_LINK))
	{
		if (cchange != 0)
		{
			e = w_event_new(f->path, EVENT_FILE_MOD_DATA, 0);
			e->next = *delta;
			*delta = e;
		}
	}
	
	if (oldsb->st_uid != f->sb->st_uid)
	{
		e = w_event_new(f->path, EVENT_FILE_MOD_UID, 0);
		e->next = *delta;
		*delta = e;
	}
	
	if (oldsb->st_gid != f->sb->st_gid)
	{
		e = w_event_new(f->path, EVENT_FILE_MOD_GID, 0);
		e->next = *delta;
		*delta = e;
	}
	
	if ((oldsb->st_mode & 07000) != (f->sb->st_mode & 07000))
	{
		e = w_event_new(f->path, EVENT_FILE_MOD_STICKY, 0);
		e->next = *delta;
		*delta = e;
	}
	
	if ((oldsb->st_mode & 0777) != (f->sb->st_mode & 0777))
	{
		e = w_event_new(f->path, EVENT_FILE_MOD_ACCESS, 0);
		e->next = *delta;
		*delta = e;
	}

	return status;
}

static uint32_t
file_watcher_update_dir(file_watcher_t *f, w_event_t **delta)
{
	DIR *dp;
	struct dirent *dent;
	w_event_t *e, *x;
	uint32_t do_notify;

#ifdef DEBUG
	log_message(ASL_LEVEL_DEBUG, "file_watcher_update_dir(%u, %s)", f->w->wid, f->path);
#endif

	dp = opendir(f->path);
	if (dp == NULL) return 0;

	do_notify = 0;

	for (dent = readdir(dp); dent != NULL; dent = readdir(dp))
	{
		if (!strcmp(dent->d_name, ".")) continue;
		if (!strcmp(dent->d_name, "..")) continue;

		e = w_event_find(f->contents, dent->d_name);
		if (e == NULL)
		{
			do_notify = 1;
			e = w_event_new(dent->d_name, EVENT_FILE_ADD, 0);
			e->next = *delta;
			*delta = e;
		}
		else
		{
			e->flags |= FS_CHECKED;
		}
	}

	closedir(dp);

	/*
	 * Walk through contents and see if all files have been checked.
	 * Unchecked files have been deleted.
	 */
	for (e = f->contents; e != NULL; e = e->next)
	{
		if ((e->flags & FS_CHECKED) == 0)
		{
			do_notify = 1;
			x = w_event_new(e->name, EVENT_FILE_DELETE, 0);
			x->next = *delta;
			*delta = x;
		}
		else
		{
			/* Clear flags */
			e->flags &= ~FS_CHECKED;
		}
	}

	/* update contents */
	for (e = *delta; e != NULL; e = e->next)
	{
		dir_contents_update(f, e);
	}

	return do_notify;
}

static void
file_append_history(file_watcher_t *f, w_event_t *delta)
{
	uint32_t tailref;
	w_event_t *e, *x, *l;

	if (f == NULL) return;
	if (delta == NULL) return;

	/* tail's refcount tells us how many clients are watching */
	tailref = f->tail->refcount - 1;
	if (tailref == 0)
	{
		/* nobody is watching - release the events */
		x = NULL;
		for (e = delta; e != NULL; e = x)
		{
			x = e->next;
			w_event_release(e);
		}

		return;
	}

	/* swap data from head of delta and current tail */
	f->tail->name = strdup(delta->name);
	f->tail->type = delta->type;
	f->tail->flags = delta->flags;
	f->tail->private = delta->private;
	f->tail->next = delta->next;
	f->tail->refcount = tailref;
	l = f->tail;

	e = delta->next;
	delta->next = NULL;
	
	w_event_release(delta);
	delta = e;

	/*
	 * find end of list
	 * set refcounts along the way
	 */
	for (e = delta; e != NULL; e = e->next)
	{
		l = e;
		e->refcount = tailref;
	}

	/* add a new tail */
	x = w_event_new(NULL, EVENT_NULL, 0);
	x->refcount = tailref + 1;
	l->next = x;

	f->tail = x;
}

static uint32_t
file_watcher_remove(file_watcher_t *f)
{
	w_event_t *e, *x, *delta;
	char *pdir, *p;

#ifdef DEBUG
	log_message(ASL_LEVEL_DEBUG, "file_watcher_remove(%u, %s)", f->w->wid, f->path);
#endif

	if (f == NULL) return 0;
	if (f->flags == FS_REMOVED) return 0;

	f->flags = FS_REMOVED;
	if (f->parent == NULL)
	{
		pdir = strdup(f->path);
		p = strrchr(pdir, '/');
		if (p == NULL)
		{
			/* this can't happen! */
		}
		else if (p == pdir)
		{
			/* hit root */
			p++;
			*p = '\0';
		}
		else
		{
			*p = '\0';
		}

		f->parent = file_watcher_new(pdir);
		if (pdir != NULL) free(pdir);
	}

	watcher_add_forward(f->parent, f->w->wid);

	delta = NULL;
	x = w_event_new(f->path, EVENT_FILE_DELETE, 0);

	/* If f is a directory, add events to remove all files */
	if (f->ftype == FS_TYPE_DIR)
	{
		for (e = f->contents; e != NULL; e = e->next)
		{
			e->type = EVENT_FILE_DELETE;
			if (e->next == NULL)
			{
				e->next = x;
				x = NULL;
			}
		}

		delta = f->contents;
		if (delta == NULL) delta = x;

		f->contents = NULL;
	}
	else
	{
		delta = x;
	}

	file_append_history(f, delta);
	
	/* If f is a link, remove linktarget watcher */
	if (f->ftype == FS_TYPE_LINK)
	{
		watcher_release_deferred(f->linktarget);
		f->linktarget = NULL;
		if (f->targetsb != NULL) free(f->targetsb);
		f->targetsb = NULL;
	}

	/* remove from kqueue */
	if ((f->ftype == FS_TYPE_FILE) || (f->ftype == FS_TYPE_DIR))
	{
		file_watcher_remove_kq(f);
	}

#ifdef DEBUG
	log_message(ASL_LEVEL_DEBUG, "file_watcher_remove %s - removed", f->path);
#endif

	f->ftype = FS_TYPE_NONE;
	return 1;
}

static uint32_t
file_watcher_update_link(file_watcher_t *f, w_event_t **delta)
{
	char *pdir, *p;
	char lpath[MAXPATHLEN + 1];
	int n;
	w_event_t *e;
	struct stat sb;
	file_watcher_t *t;

#ifdef DEBUG
	log_message(ASL_LEVEL_DEBUG, "file_watcher_update_link(%u, %s)", f->w->wid, f->path);
#endif

	n = readlink(f->path, lpath, MAXPATHLEN + 1);
	if (n <= 0) 
	{
		/* link doesn't exist! */
		file_watcher_remove(f);
		return -1;
	}
	lpath[n] = '\0';

	/* If lpath is is not absolute, it is relative to f->path parent dir.  Fix it! */
	if (lpath[0] != '/')
	{
		pdir = strdup(f->path);
		p = strrchr(pdir, '/');
		if (p == pdir)
		{
			/* parent is root dir */
			p = NULL;
			asprintf(&p, "/%s", lpath);
		}
		else
		{
			*p = '\0';
			p = NULL;
			asprintf(&p, "%s/%s", pdir, lpath);
		}
		memcpy(lpath, p, strlen(p) + 1);
		free(pdir);
		free(p);
	}

	if (f->linktarget != NULL) 
	{
		t = (file_watcher_t *)f->linktarget->sub;
		if (t == NULL) return 0; /* can't happen */

		if (strcmp(lpath, t->path))
		{
			/* Link changed */
			watcher_release_deferred(f->linktarget);
			f->linktarget = file_watcher_new(lpath);
			free(f->targetsb);
			f->targetsb = malloc(sizeof(struct stat));
			memcpy(f->targetsb, ((file_watcher_t *)(f->linktarget->sub))->sb, sizeof(struct stat));
			watcher_add_forward(f->linktarget, f->w->wid);
			return 1;
		}

		memset(&sb, 0, sizeof(struct stat));
		n = stat(f->path, &sb);
		if (n < 0) 
		{
			if (f->flags == FS_DEADLINK) return 0;
			f->flags = FS_DEADLINK;
			e = w_event_new(t->path, EVENT_FILE_DELETE, 0);
			e->next = *delta;
			*delta = e;
		}
		else if (f->flags == FS_DEADLINK)
		{
			f->flags = 0;
			e = w_event_new(t->path, EVENT_FILE_ADD, 0);
			e->next = *delta;
			*delta = e;
		}
		else if (f->targetsb != NULL)
		{
			file_watcher_check_file(t, f->targetsb, delta);
			free(f->targetsb);
			f->targetsb = malloc(sizeof(struct stat));
			memcpy(f->targetsb, t->sb, sizeof(struct stat));
		}
	}

	if ((f->parent != NULL) && (f->linktarget != NULL)) return 0;

	pdir = strdup(f->path);
	p = strrchr(pdir, '/');
	if (p == NULL)
	{
	}
	else if (p == pdir)
	{
		p++;
		*p = '\0';
	}
	else
	{
		*p = '\0';
	}

	if (f->parent == NULL)
	{
		f->parent = file_watcher_new(pdir);
		watcher_add_forward(f->parent, f->w->wid);
	}

	if (f->linktarget == NULL)
	{
		p = NULL;
		if (lpath[0] != '/')
		{
			if (!strcmp(pdir, "/")) asprintf(&p, "/%s", lpath);
			else asprintf(&p, "%s/%s", pdir, lpath);
		}
		else asprintf(&p, "%s", lpath);
	
		f->linktarget = file_watcher_new(p);
		free(f->targetsb);
		f->targetsb = malloc(sizeof(struct stat));
		memcpy(f->targetsb, ((file_watcher_t *)(f->linktarget->sub))->sb, sizeof(struct stat));
		if (p != NULL) free(p);

		watcher_add_forward(f->linktarget, f->w->wid);
	}

	if (pdir != NULL) free(pdir);
	return 0;
}
	
w_event_t *
file_watcher_history(watcher_t *w)
{
	file_watcher_t *f;
	w_event_t *n, *e, *l, *p;

	if (w == NULL) return NULL;
	if (w->type != WATCH_FILE) return NULL;

	f = (file_watcher_t *)w->sub;

	if (f == NULL) return NULL;

	l = NULL;
	p = NULL;

	/*
	 * start with a list of the current directory contents
	 */
	for (n = f->contents; n != NULL; n = n->next)
	{
		e = w_event_new(n->name, n->type, 0);
		if (l == NULL) l = e;
		else p->next = e;
		p = e;
	}

	e = f->tail;
	e->refcount++;

	if (l == NULL) l = e;
	else p->next = e;

	return l;
}

static int32_t
file_watcher_update(file_watcher_t *f, uint32_t flags, uint32_t level)
{
	struct stat *oldsb;
	w_event_t *e, *delta;
	int32_t status, do_notify, did_remove;

#ifdef DEBUG
	log_message(ASL_LEVEL_DEBUG, "file_watcher_update(%u, 0x%08x, %s)", f->w->wid, flags, f->path);
#endif

	if (f == NULL) return 0;

	did_remove = 0;
	if (flags & NOTE_DELETE)
	{
		file_watcher_remove(f);
		did_remove = 1;
	}

	oldsb = f->sb;
	f->sb = (struct stat *)calloc(1, sizeof(struct stat));

	status = -1;
	if (f->ftype == FS_TYPE_NONE)
	{
		/* new or previously removed file watcher */
		status = lstat(f->path, f->sb);
		if (status != 0) f->ftype = FS_TYPE_NONE;
		else if ((f->sb->st_mode & S_IFMT) == S_IFDIR) f->ftype = FS_TYPE_DIR;
		else if ((f->sb->st_mode & S_IFMT) == S_IFREG) f->ftype = FS_TYPE_FILE;
		else if ((f->sb->st_mode & S_IFMT) == S_IFLNK) f->ftype = FS_TYPE_LINK;
	}
	else if (f->ftype == FS_TYPE_LINK)
	{
		status = lstat(f->path, f->sb);
	}
	else
	{
		status = stat(f->path, f->sb);
	}

	if (status < 0)
	{
		free(oldsb);
		if (did_remove == 0) status = file_watcher_remove(f);
		return 1;
	}

	if ((f->flags == FS_REMOVED) || (f->flags == FS_INITIAL))
	{
		if ((f->ftype == FS_TYPE_FILE) || (f->ftype == FS_TYPE_DIR))
		{
			file_watcher_add_kq(f);
		}
	}

	if ((f->flags == FS_REMOVED) && (f->parent != NULL))
	{
		watcher_remove_forward(f->parent, f->w->wid);
		watcher_release_deferred(f->parent);
		f->parent = NULL;
	}

	delta = NULL;

	status = 1;

	/* Check for file modified or attrs modified */
	if ((f->flags != FS_REMOVED) && (f->flags != FS_INITIAL))
	{
		status = file_watcher_check_file(f, oldsb, &delta);
	}

	free(oldsb);

	/* if file_watcher_check_file determined there's no work to do, bail out now */
	if (status == 0) return 0;

	do_notify = 0;

	if (f->ftype == FS_TYPE_DIR) do_notify = file_watcher_update_dir(f, &delta);
	else if (f->ftype == FS_TYPE_LINK) do_notify = file_watcher_update_link(f, &delta);

	if ((do_notify >= 0) && (f->flags == FS_REMOVED))
	{
		e = w_event_new(f->path, EVENT_FILE_ADD, 0);
		e->next = delta;
		delta = e;
		f->flags = 0;
	}

	if (f->flags == FS_INITIAL) f->flags = 0;

	if ((do_notify < 0) || (delta != NULL)) do_notify = 1;

	/* append delta to history */
	file_append_history(f, delta);

	return do_notify;
}

watcher_t *
file_watcher_for_path(const char *p)
{
	watcher_t *w;
	file_watcher_t *f;
	list_t *n;

	if (p == NULL) return NULL;

	for (n = watch_list; n != NULL; n = _nc_list_next(n))
	{
		w = _nc_list_data(n);
		if (w == NULL) continue;
		if (w->type != WATCH_FILE) continue;
		f = (file_watcher_t *)w->sub;
		if (f == NULL) continue;
		if (f->path == NULL) continue;
		if (strcmp(p, f->path) == 0) return watcher_retain(w);
	}

	return NULL;
}

watcher_t *
file_watcher_new(const char *p)
{
	watcher_t *w;
	file_watcher_t *f;

	if (p == NULL) return NULL;

	w = file_watcher_for_path(p);
	if (w != NULL)
	{
		return w;
	}

	w = watcher_new();
	if (w == NULL) return NULL;

#ifdef DEBUG
	log_message(ASL_LEVEL_DEBUG, "file_watcher_new(%u, %s)", w->wid, p);
#endif

	f = (file_watcher_t *)calloc(1, sizeof(file_watcher_t));
	if (f == NULL)
	{
		watcher_release(w);
		return NULL;
	}


	f->sb = (struct stat *)calloc(1, sizeof(struct stat));
	if (f->sb == NULL)
	{
		watcher_release(w);
		free(f);
		return NULL;
	}

	f->tail = w_event_new(NULL, EVENT_NULL, 0);
	f->kqident = IDENT_INVAL;
	
	w->type = WATCH_FILE;
	w->sub = f;
	w->sub_trigger = file_watcher_trigger;
	w->sub_free = file_watcher_free;
	w->sub_printf = file_watcher_printf;

	f->w = w;
	f->path = strdup(p);

	f->flags = FS_INITIAL;
	file_watcher_update(f, 0, 0);

#ifdef DEBUG
	log_message(ASL_LEVEL_DEBUG, "file_watcher_new: %s type %u trigger %u", p, f->ftype, w->wid);
#endif

	return w;
} 	

int32_t
file_watcher_trigger(watcher_t *w, uint32_t flags, uint32_t level)
{
	file_watcher_t *f;

	if (w == NULL) return 0;
	if (w->sub == NULL) return 0;
	f = (file_watcher_t *)w->sub;

#ifdef DEBUG
	log_message(ASL_LEVEL_DEBUG, "file_watcher_trigger: %s type %u wid %u flags 0x%08x", f->path, f->ftype, w->wid, flags);
#endif

	return file_watcher_update(f, flags, level);
}

void
file_watcher_free(watcher_t *w)
{
	file_watcher_t *f;
	w_event_t *e, *n;

	if (w == NULL) return;
	if (w->sub == NULL) return;

	f = (file_watcher_t *)w->sub;

#ifdef DEBUG
	log_message(ASL_LEVEL_DEBUG, "file_watcher_free(%u, %s)", f->w->wid, f->path);
#endif

	if (f->path != NULL) free(f->path);
	if (f->sb != NULL) free(f->sb);
	if (f->targetsb != NULL) free(f->targetsb);

	if ((f->ftype == FS_TYPE_FILE) || (f->ftype == FS_TYPE_DIR))
	{
		file_watcher_remove_kq(f);
	}

	if (f->parent != NULL) watcher_release(f->parent);
	if (f->linktarget != NULL) watcher_release(f->linktarget);

	n = NULL;
	for (e = f->contents; e != NULL; e = n)
	{
		n = e->next;
		w_event_release(e);
	}
	
	w_event_release(f->tail);
	
	free(f);
}
