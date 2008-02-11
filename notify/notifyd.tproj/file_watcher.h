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

#ifndef _FILE_WATCHER_H_
#define _FILE_WATCHER_H_

#include "watcher.h"
#include "w_event.h"
#include <sys/types.h>
#include <sys/stat.h>

#define FS_TYPE_NONE  0
#define FS_TYPE_FILE  1
#define FS_TYPE_DIR   2
#define FS_TYPE_LINK  3

typedef struct
{
	watcher_t *w;
	uintptr_t kqident;
	char *path;
	uint32_t flags;
	uint32_t ftype;
	struct stat *sb;
	struct stat *targetsb;
	watcher_t *parent;
	watcher_t *linktarget;
	w_event_t *contents;
	w_event_t *tail;
} file_watcher_t;

watcher_t *file_watcher_for_path(const char *p);
watcher_t *file_watcher_new(const char *p);
int32_t file_watcher_trigger(watcher_t *w, uint32_t flags, uint32_t level);
void file_watcher_free(watcher_t *w);
w_event_t *file_watcher_history(watcher_t *w);

#endif /* _FILE_WATCHER_H_ */
