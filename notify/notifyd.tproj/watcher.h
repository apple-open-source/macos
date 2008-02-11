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

#ifndef _WATCHER_H_
#define _WATCHER_H_

#include "table.h"
#include <stdio.h>

#define WATCH_GENERIC 0
#define WATCH_FILE 1

extern list_t *watch_list;

typedef struct watcher_s
{
	uint32_t wid;
	char **name;
	uint32_t type;
	uint32_t state;
	uint32_t count;
	uint32_t *fwd;
	int32_t (*sub_trigger)(struct watcher_s *w, uint32_t flags, uint32_t level);
	void (*sub_free)(struct watcher_s *w);
	void (*sub_printf)(struct watcher_s *w, FILE *f);
	void *sub;
	uint32_t refcount;
} watcher_t;

watcher_t *watcher_new();

watcher_t *watcher_retain(watcher_t *w);
void watcher_release(watcher_t *w);
void watcher_release_deferred(watcher_t *w);

void watcher_add_name(watcher_t *w, const char *name);
void watcher_remove_name(watcher_t *w, const char *name);

void watcher_trigger(uint32_t t, uint32_t flags, uint32_t level);

void watcher_add_forward(watcher_t *w, uint32_t t);
void watcher_remove_forward(watcher_t *w, uint32_t t);

void watcher_shutdown();

void watcher_printf(watcher_t *w, FILE *f);

#endif /* _WATCHER_H_ */
