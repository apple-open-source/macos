/* webdav_requestqueue.h created by warner_c on Mon 24-Jul-2000 */

/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * "Portions Copyright (c) 2000 Apple Computer, Inc.  All Rights
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
/*		@(#)webdav_requestqueue.h	   *
 *		(c) 2000   Apple Computer, Inc.	 All Rights Reserved
 *
 *
 *		webdav_requestqueue.h -- Headers for webdavfs workerthread request queue
 *
 *		MODIFICATION HISTORY:
 *				24-JUL-2000		Clark Warner	  File Creation
 */

#include <stdio.h>
#include <sys/types.h>
#include <pthread.h>

/* structure */

typedef struct webdav_requestqueue_element_tag
{
	struct webdav_requestqueue_element_tag *next;
	int type;
	union
	{
		struct request
		{
			int proxy_ok;						/* ok to use proxy */
			int socket;							/* socket for connection */
		} request;								/* Struct used for requests from the kernel */

		struct download
		{
			int *download_status;				/* keeps track of background downloads */
			int remote;							/* remote socket */
			int local;							/* local file descriptor */
			off_t total_length;					/* size of the request */
			int chunked;						/* indicates if info is chunked */
		} download;								/* Struct used for download requests */
	} element;
} webdav_requestqueue_element_t;

typedef struct
{
	webdav_requestqueue_element_t *item_head;
	webdav_requestqueue_element_t *item_tail;
	int request_count;
} webdav_requestqueue_header_t;

struct ThreadSocket
{
	/* socket field MUST be first element */
	int	socket;		/* the socket number or -1 */
	int	inuse;		/* non-zero if ThreadSocket is in use */
};
typedef struct ThreadSocket ThreadSocket;

/* Definitions */
#define WEBDAV_REQUEST_TYPE 1
#define WEBDAV_DOWNLOAD_TYPE 2
#define WEBDAV_DOWNLOAD_LIMIT 4096

/* Functions */

extern void webdav_requestqueue_init();
extern int webdav_requestqueue_enqueue_request(int proxy_ok, int socket);
extern int webdav_requestqueue_enqueue_download(int *remote, int local, off_t total_length,
	int chunked, int *download_status);
extern int webdav_request_thread(void *arg);

/* Global structures */

extern webdav_requestqueue_header_t gwaiting_requests;
extern ThreadSocket webdav_threadsockets[WEBDAV_REQUEST_THREADS];
extern pthread_mutex_t grequests_lock;
extern pthread_cond_t gcondvar;
