#ifndef _WEBDAV_MEMCACHE_H_INCLUDE
#define _WEBDAV_MEMCACHE_H_INCLUDE


/* webdav_memcache.h created by warner_c on Fri 12-Nov-1999 */

/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').	You may not use this file
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
/*		@(#)webdav_memchache.h		*
 *		(c) 1999   Apple Computer, Inc.	 All Rights Reserved
 *
 *
 *		webdav_memcache.h -- Headers for WebDAV in memory cache for stat info
 *
 *		MODIFICATION HISTORY:
 *				12-NOV-99	  Clark Warner		File Creation
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <pthread.h>


/* Definitions */

#define WEBDAV_NUMCACHEDUIDS 5
#define WEBDAV_MEMCACHE_TIMEOUT 60	/* Number of seconds webdav_memcache_element_tag is valid */

/* structure */

typedef struct webdav_memcache_element_tag
{
	struct webdav_memcache_element_tag *next;
	char *uri;
	int uri_length;
	time_t time_received;
	struct vattr *vap;
} webdav_memcache_element_t;

typedef struct
{
	int open_uids;
	int last_uid;
	pthread_mutex_t lock;
	struct
	{
		webdav_memcache_element_t *item_head;
		webdav_memcache_element_t *item_tail;
		uid_t uid;
	} uid_array[WEBDAV_NUMCACHEDUIDS];
} webdav_memcache_header_t;


/* functions */

extern int webdav_memcache_init(webdav_memcache_header_t *cache_header);
extern int webdav_memcache_insert(uid_t uid, char *uri, webdav_memcache_header_t *cache_header,
	struct vattr *vap);
extern int webdav_memcache_remove(uid_t uid, char *uri, webdav_memcache_header_t *cache_header);
extern int webdav_memcache_retrieve(uid_t uid, char *uri,
	webdav_memcache_header_t *cache_header, struct vattr *vap);

#endif
