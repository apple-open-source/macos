/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
/*		@(#)webdav_requestqueue.c	   *
 *		(c) 1999   Apple Computer, Inc.	 All Rights Reserved
 *
 *
 *		webdav_requestqueue.c -- routines for the webdavfs thread pool
 *
 *		MODIFICATION HISTORY:
 *				24-JUL-2000		Clark Warner	  File Creation
 */

#include <err.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include "http.h"
#include "webdavd.h"
#include "webdav_requestqueue.h"

/*****************************************************************************/

void webdav_requestqueue_init()
{
	pthread_mutexattr_t mutexattr;
	int	index;
	
	/* Initialize webdav_threadsockets  */
	for ( index = 0; index < WEBDAV_REQUEST_THREADS; ++index )
	{
		webdav_threadsockets[index].socket = -1;	/* closed */
		webdav_threadsockets[index].inuse = 0;	/* not in use */
	}
				
	bzero(&gwaiting_requests, sizeof(gwaiting_requests));

	if (pthread_cond_init(&gcondvar, NULL))
	{
		err(1, " pthread cond init - global cond var");
	}

	/* set up the lock on the queues */
	if (pthread_mutexattr_init(&mutexattr))
	{
		err(1, "mutex atrribute init - reqeust queue lock");
	}

	if (pthread_mutex_init(&grequests_lock, &mutexattr))
	{
		err(1, "mutex lock - request queue lock");
	}
}

/*****************************************************************************/

int webdav_requestqueue_enqueue_request(int proxy_ok, int socket)
{
	int error, unlock_error;
	webdav_requestqueue_element_t * request_element_ptr;

	error = pthread_mutex_lock(&grequests_lock);
	if (error)
	{
		return (error);
	}

	request_element_ptr = malloc(sizeof(webdav_requestqueue_element_t));
	if (!request_element_ptr)
	{
		error = ENOMEM;
		goto unlock;
	}

	request_element_ptr->type = WEBDAV_REQUEST_TYPE;
	request_element_ptr->element.request.socket = socket;
	request_element_ptr->element.request.proxy_ok = proxy_ok;
	request_element_ptr->next = 0;
	++(gwaiting_requests.request_count);

	if (!(gwaiting_requests.item_tail))
	{
		gwaiting_requests.item_head = gwaiting_requests.item_tail = request_element_ptr;
	}
	else
	{
		gwaiting_requests.item_tail->next = request_element_ptr;
		gwaiting_requests.item_tail = request_element_ptr;
	}

	error = pthread_cond_signal(&gcondvar);
	if (error)
	{
		goto unlock;
	}

unlock:

	unlock_error = pthread_mutex_unlock(&grequests_lock);
	if (unlock_error)
	{
		if (!(error))
		{
			return (unlock_error);
		}
	}

	return (error);
}

/*****************************************************************************/

int webdav_requestqueue_enqueue_download(int *remote, int local, off_t total_length,
	int chunked, int *download_status)
{
	int error, unlock_error;
	webdav_requestqueue_element_t * request_element_ptr;

	error = pthread_mutex_lock(&grequests_lock);
	if (error)
	{
		return (error);
	}

	request_element_ptr = malloc(sizeof(webdav_requestqueue_element_t));
	if (!request_element_ptr)
	{
		error = ENOMEM;
		goto unlock;
	}

	request_element_ptr->type = WEBDAV_DOWNLOAD_TYPE;
	request_element_ptr->element.download.remote = *remote;
	request_element_ptr->element.download.local = local;
	request_element_ptr->element.download.total_length = total_length;
	request_element_ptr->element.download.chunked = chunked;
	request_element_ptr->element.download.download_status = download_status;
	request_element_ptr->next = 0;
	++(gwaiting_requests.request_count);

	if (!(gwaiting_requests.item_tail))
	{
		gwaiting_requests.item_head = gwaiting_requests.item_tail = request_element_ptr;
	}
	else
	{
		gwaiting_requests.item_tail->next = request_element_ptr;
		gwaiting_requests.item_tail = request_element_ptr;
	}

	error = pthread_cond_signal(&gcondvar);
	if (error)
	{
		goto unlock;
	}

unlock:

	unlock_error = pthread_mutex_unlock(&grequests_lock);
	if (unlock_error)
	{
		if (!(error))
		{
			return (unlock_error);
		}
	}

	return (error);
}

/*****************************************************************************/

int webdav_request_thread(arg)
	void *arg;
{
	int error, proxy_ok;
	int kernel_socket;
	ThreadSocket *myThreadSocket;	/* pointer to ThreadSocket this thread is using */
	webdav_requestqueue_element_t * myrequest;
	int last_chunk;

	myThreadSocket = NULL;
	
	while (TRUE)
	{
		error = pthread_mutex_lock(&grequests_lock);
		if (error)
		{
			goto die;
		}

		/* Check to see if there is a request to process */

		if (gwaiting_requests.request_count > 0)
		{
			/* There is a request so dequeue it */

			myrequest = gwaiting_requests.item_head;
			--(gwaiting_requests.request_count);
			if (gwaiting_requests.request_count > 0)
			{
				/* There was more than one item on */
				/* the queue so bump the pointers */
				gwaiting_requests.item_head = myrequest->next;
			}
			else
			{
				gwaiting_requests.item_head = gwaiting_requests.item_tail = 0;
			}
			
			switch (myrequest->type)
			{
				case WEBDAV_REQUEST_TYPE:
					{
						int	index;
						ThreadSocket *aThreadSocket; /* pointer to first free ThreadSocket */
						
						/* Find the first open ThreadSocket that isn't in use. If all open
						 * ThreadSockets are in use, use the first closed ThreadSocket
						 * that isn't in use. This has to be done while grequests_lock
						 * is locked.
						 */
						myThreadSocket = aThreadSocket = NULL;
						for ( index = 0; index < WEBDAV_REQUEST_THREADS; ++index )
						{
							if ( !webdav_threadsockets[index].inuse )
							{
								/* found one not in use */
								if ( webdav_threadsockets[index].socket >= 0 )
								{
									/* and it hasn't been closed, so grab it */
									myThreadSocket = &webdav_threadsockets[index]; /* get pointer to it */
									myThreadSocket->inuse = 1;	/* mark it in use */
									break;
								}
								else if ( aThreadSocket == NULL )
								{
									/* keep track of this closed one in case we don't find an open one*/
									aThreadSocket = &webdav_threadsockets[index];
								}
							}
						}
						if ( myThreadSocket == NULL )
						{
							/* this should never happen */
							if ( aThreadSocket == NULL )
							{
								syslog(LOG_INFO, "webdav_request_thread: aThreadSocket is NULL!\n");
								goto die;
							}
							
							/* we didn't find an open socket so use the first free ThreadSocket */
							myThreadSocket = aThreadSocket;
							myThreadSocket->inuse = 1;	/* mark it in use */
						}
					}
					break;

				default:
					break;
			}
			
			/* Ok, now unlock the queue and go about handling the request */
			error = pthread_mutex_unlock(&grequests_lock);
			if (error)
			{
				goto die;
			}

			switch (myrequest->type)
			{

				case WEBDAV_REQUEST_TYPE:
					kernel_socket = myrequest->element.request.socket;
					proxy_ok = myrequest->element.request.proxy_ok;
					activate(kernel_socket, proxy_ok, &myThreadSocket->socket);
					
					/* free up this ThreadSocket */
					myThreadSocket->inuse = 0;
					
					myThreadSocket = NULL;
					break;

				case WEBDAV_DOWNLOAD_TYPE:
					/* finish the download */
					if (myrequest->element.download.chunked)
					{
						error = http_read_chunked(&myrequest->element.download.remote,
							myrequest->element.download.local,
							myrequest->element.download.total_length,
							myrequest->element.download.download_status,
							&last_chunk);
					}
					else
					{
						error = http_read(&myrequest->element.download.remote, 
							myrequest->element.download.local, 
							myrequest->element.download.total_length, 
							myrequest->element.download.download_status);
					}

					if (error)
					{
						/* Set append to indicate that our download failed. It's a hack, but
						 * it should work.	Be sure to still mark the download as finished so
						 * that if we were terminated early the closer will be notified
						 */
						(void)fchflags(myrequest->element.download.local, UF_APPEND);
						*(myrequest->element.download.download_status) = WEBDAV_DOWNLOAD_ABORTED;
					}
					else
					{
						/* Clear flags to indicate that our download is complete. It's a hack, but
						 * it should work.	Be sure to still mark the download as finished so
						 * that if we were terminated early the closer will be notified
						 */
						(void)fchflags(myrequest->element.download.local, 0);
						*(myrequest->element.download.download_status) = WEBDAV_DOWNLOAD_FINISHED;
					}
					
					/* close the cache file */
					(void)close(myrequest->element.download.local);
					
					/* close the socket if needed */
					if ( myrequest->element.download.remote >= 0 )
					{
						(void)close(myrequest->element.download.remote);
					}
										
					break;

				default:
					/* nothing we can do, just get the next request */
					break;
			}

			free(myrequest);

		}
		else
		{
			/* There were no requests so just wait on the condition variable */
			error = pthread_cond_wait(&gcondvar, &grequests_lock);
			if (error)
			{
				goto die;
			}

			/* Ok, unlock so that we can restart the loop */
			error = pthread_mutex_unlock(&grequests_lock);
			if (error)
			{
				goto die;
			}
		}
	}

die:

	return error;
}

/*****************************************************************************/
