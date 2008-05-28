/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All rights reserved.
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

#ifndef _WEBDAV_REQUESTQUEUE_H_INCLUDE
#define _WEBDAV_REQUESTQUEUE_H_INCLUDE

#include <sys/types.h>
#include <pthread.h>
#include <mach/boolean.h>
#include <unistd.h>

#include "webdav_cache.h"
#include "webdav_network.h"

/* Functions */
#define WEBDAV_CONNECTION_UP 1
#define WEBDAV_CONNECTION_DOWN 0
extern int get_connectionstate(void);
extern void set_connectionstate(int bad);

extern int requestqueue_init(void);
extern int requestqueue_enqueue_request(int socket);
extern int requestqueue_enqueue_download(
			struct node_entry *node,			/* the node */
			struct ReadStreamRec *readStreamRecPtr); /* the ReadStreamRec */
extern int requestqueue_enqueue_server_ping(u_int32_t delay);
extern int requestqueue_purge_cache_files(void);
extern int requestqueue_enqueue_seqwrite_manager(struct stream_put_ctx *);

#endif
