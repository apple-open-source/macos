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

#ifndef _NOTIFY_SERVICE_H_
#define _NOTIFY_SERVICE_H_

#include "w_event.h"

#define SERVICE_TYPE_NONE 0
#define SERVICE_TYPE_FILE 1
#define SERVICE_TYPE_EVENT 2

typedef struct
{
	uint32_t type;
	void *private;
} svc_info_t;

int service_open_file(int client_id, const char *name, const char *path, int flags, uint32_t uid, uint32_t gid);
int service_open(int client_id, const char *name, int flags, uint32_t uid, uint32_t gid);
void service_close(svc_info_t *s, const char *name);
w_event_t *service_get_event(svc_info_t *s);

#endif /* _NOTIFY_SERVICE_H_ */
