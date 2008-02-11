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

#include "w_event.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

w_event_t *
w_event_new(const char *name, uint32_t type, uint32_t flags)
{
	w_event_t *e;

	e = (w_event_t *)calloc(1, sizeof(w_event_t));
	if (e == NULL) return NULL;

	if (name != NULL) e->name = strdup(name);
	e->type = type;
	e->flags = flags;

	e->refcount = 1;

	return e;
}

w_event_t *
w_event_retain(w_event_t *e)
{
	if (e == NULL) return NULL;
	e->refcount++;
	return e;
}

void
w_event_release(w_event_t *e)
{
	if (e == NULL) return;
	if (e->refcount > 0) e->refcount--;
	if (e->refcount > 0) return;

	if (e->name != NULL) free(e->name);
	free(e);
}

w_event_t *
w_event_find(w_event_t *list, char *name)
{
	w_event_t *e;

	if (list == NULL) return NULL;
	if (name == NULL) return NULL;

	for (e = list; e != NULL; e = e->next)
	{
		if (e->name == NULL) continue;
		if (!strcmp(name, e->name)) return e;
	}

	return NULL;
}
