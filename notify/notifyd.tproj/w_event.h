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

#ifndef _W_EVENT_H_
#define _W_EVENT_H_

#include <stdint.h>

#define EVENT_NULL 0
#define EVENT_FILE_ADD 1
#define EVENT_FILE_DELETE 2
#define EVENT_FILE_MOD_DATA 3
#define EVENT_FILE_MOD_UID 4
#define EVENT_FILE_MOD_GID 5
#define EVENT_FILE_MOD_STICKY 6
#define EVENT_FILE_MOD_ACCESS 7

typedef struct w_event_s
{
	char *name;
	uint32_t type;
	uint32_t flags;
	void *private;
	uint32_t refcount;
	struct w_event_s *next;
} w_event_t;

w_event_t *w_event_new(const char *name, uint32_t type, uint32_t flags);
w_event_t *w_event_retain(w_event_t *e);
void w_event_release(w_event_t *e);
w_event_t *w_event_find(w_event_t *list, char *name);

#endif /* _W_EVENT_H_ */
