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
#include "watcher.h"
#include "daemon.h"

list_t *watch_list = NULL;
uint32_t watcher_id = 0;

watcher_t *
watcher_new()
{
	watcher_t *w;
	list_t *n;

	w = (watcher_t *)calloc(1, sizeof(watcher_t));
	if (w == NULL) return NULL;

	w->wid = ++watcher_id;
	w->refcount = 1;

	n = _nc_list_new(w);
	watch_list = _nc_list_prepend(watch_list, n);

	return w;
}

watcher_t *
watcher_retain(watcher_t *w)
{
	if (w == NULL) return NULL;

	w->refcount++;

	return w;
}

void
watcher_release_deferred(watcher_t *w)
{
	if (w == NULL) return;
	if (w->refcount > 0) w->refcount--;
}

void
watcher_release(watcher_t *w)
{
	int i;

	if (w == NULL) return;

	if (w->refcount > 0) w->refcount--;
	if (w->refcount > 0) return;

	watch_list = _nc_list_find_release(watch_list, w);

	if (w->sub_free != NULL) w->sub_free(w);

	if (w->name != NULL)
	{
		for (i = 0; w->name[i] != NULL; i++) free(w->name[i]);
		free(w->name);
	}

	if (w->fwd != NULL) free(w->fwd);
	free(w);
}

void
watcher_add_name(watcher_t *w, const char *name)
{
	int i;

	if (w == NULL) return;
	if (name == NULL) return;

	if (w->name == NULL)
	{
		w->name = (char **)calloc(2, sizeof(char *));
		w->name[0] = strdup(name);
		return;
	}

	for (i = 0; w->name[i] != NULL; i++)
	{
		if (!strcmp(w->name[i], name)) return;
	}

	w->name = (char **)realloc(w->name, (i + 2) * sizeof(char *));
	w->name[i] = strdup(name);
	w->name[i + 1] = NULL;
}

void
watcher_remove_name(watcher_t *w, const char *name)
{
	int i, j;

	if (w == NULL) return;
	if (name == NULL) return;
	if (w->name == NULL) return;

	for (i = 0; w->name[i] != NULL; i++)
	{
		if (!strcmp(w->name[i], name)) 
		{
			free(w->name[i]);
			for (j = i + 1; w->name[j] != NULL; j++)
				w->name[j - 1] = w->name[j];

			w->name[j - 1] = NULL;

			w->name = (char **)realloc(w->name, j * sizeof(char *));
			return;
		}
	}
}

static void
watcher_internal_trigger(watcher_t *w, uint32_t t, uint32_t flags, uint32_t level)
{
	uint32_t i, postit;

	if (w == NULL) return;
	if (w->wid != t) return;

#ifdef DEBUG
	log_message(ASL_LEVEL_DEBUG, "watcher_trigger %u level %u\n", t, level);
#endif

	postit = 0;
	if (w->sub_trigger != NULL) postit = w->sub_trigger(w, flags, level);

	if ((w->name != NULL) && (postit != 0))
	{
		for (i = 0; w->name[i] != NULL; i++) daemon_post(w->name[i], 0, 0);
	}

	for (i = 0; i < w->count; i++) watcher_trigger(w->fwd[i], 0, level + 1);
}

void
watcher_add_forward(watcher_t *w, uint32_t t)
{
	uint32_t i;

	if (w == NULL) return;

	for (i = 0; i < w->count; i++)
	{
		if (w->fwd[i] == t) return;
	}

	if (w->count == 0)
	{
		w->fwd = (uint32_t *)malloc(sizeof(uint32_t));
	}
	else
	{
		w->fwd = (uint32_t *)realloc(w->fwd, (w->count + 1) * sizeof(uint32_t));
	}

	w->fwd[w->count] = t;
	w->count++;
}

void
watcher_remove_forward(watcher_t *w, uint32_t t)
{
	uint32_t i, x;

	if (w->count == 0) return;

	x = w->count + 1;

	for (i = 0; i < w->count; i++)
	{
		if (w->fwd[i] == t)
		{
			x = i;
			break;
		}
	}
	
	if (x > w->count) return;

	if (w->count == 1)
	{
		free(w->fwd);
		w->fwd = NULL;
		w->count = 0;
		return;
	}

	for (i = x + 1; i < w->count; i++) w->fwd[i - 1] = w->fwd[i];

	w->count--;
	w->fwd = (uint32_t *)realloc(w->fwd, w->count * sizeof(uint32_t));
}

void
watcher_trigger(uint32_t t, uint32_t flags, uint32_t level)
{
	watcher_t *w;
	list_t *n, *x;

	for (n = watch_list; n != NULL; n = _nc_list_next(n))
	{
		w = _nc_list_data(n);
		watcher_internal_trigger(w, t, flags, level);
	}

	if (level == 0)
	{
		for (n = watch_list; n != NULL; n = x)
		{
			x = _nc_list_next(n);
			w = _nc_list_data(n);
			if (w->refcount == 0) watcher_release(w);
		}
	}
}

void
watcher_shutdown()
{
	watcher_t *w;
	list_t *n;

	for (n = watch_list; n != NULL; n = _nc_list_next(n))
	{
		w = _nc_list_data(n);
		watcher_release(w);
	}

	_nc_list_release_list(watch_list);
}

void
watcher_printf(watcher_t *w, FILE *f)
{
	uint32_t i;

	if (w == NULL)
	{
		fprintf(f, "-nil-\n");
		return;
	}

	fprintf(f, "Watcher %u\n", w->wid);
	fprintf(f, "Name:");
	if (w->name == NULL) fprintf(f, " -nil-");
	else for (i = 0; w->name[i] != NULL; i++) fprintf(f, " %s", w->name[i]);
	fprintf(f, "\n");
	fprintf(f, "Type: %u\n", w->type);
	fprintf(f, "Refcount: %u\n", w->refcount);
	fprintf(f, "Forward (%u):", w->count);
	for (i = 0; i < w->count; i++) fprintf(f, " %u", w->fwd[i]);
	fprintf(f, "\n");
	if (w->sub_printf != NULL) w->sub_printf(w, f);
}
