/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

/* gconnectionstate_lock used to make gconnectionstate thread safe */
pthread_mutex_t gconnectionstate_lock;
/* gconnectionstate is set to either WEBDAV_CONNECTION_UP or WEBDAV_CONNECTION_DOWN */
int gconnectionstate;

/*****************************************************************************/

/* initialize gconnectionstate_lock and gconnectionstate */
int gconnectionstate_init(void)
{
	pthread_mutexattr_t mutexattr;
	int error;
	
	/* set up the lock on gconnectionbad */
	
	gconnectionstate = WEBDAV_CONNECTION_UP;
	
	error = pthread_mutexattr_init(&mutexattr);
	if (error)
	{
		syslog(LOG_ERR, "gconnectionstate_init: pthread_mutexattr_init() failed: %s", strerror(error));
		goto done;
	}

	error = pthread_mutex_init(&gconnectionstate_lock, &mutexattr);
	if (error)
	{
		syslog(LOG_ERR, "gconnectionstate_init: pthread_mutex_init() failed: %s", strerror(error));
		goto done;
	}
	
done:
	
	return ( error );
}

/*****************************************************************************/

/* get the gconnectionstate */
int get_gconnectionstate(void)
{
	int error;
	int result = 1; /* return bad if we cannot lock the mutex */
	
	error = pthread_mutex_lock(&gconnectionstate_lock);
	if (error)
	{
		syslog(LOG_ERR, "get_gconnectionstate: pthread_mutex_lock(): %s", strerror(error));
		goto exit;
	}
	
	result = gconnectionstate;
	
	error = pthread_mutex_unlock(&gconnectionstate_lock);
	if (error)
	{
		syslog(LOG_ERR, "get_gconnectionstate: pthread_mutex_unlock(): %s", strerror(error));
	}
	
exit:
	
	return ( result );
}

/*****************************************************************************/

/* set the gconnectionstate */
void set_gconnectionstate(int state)
{
	int error;
	
	error = pthread_mutex_lock(&gconnectionstate_lock);
	if (error)
	{
		syslog(LOG_ERR, "set_gconnectionstate: pthread_mutex_lock(): %s", strerror(error));
		goto exit;
	}
	
	gconnectionstate = state;
	
	error = pthread_mutex_unlock(&gconnectionstate_lock);
	if (error)
	{
		syslog(LOG_ERR, "set_gconnectionstate: pthread_mutex_unlock(): %s", strerror(error));
	}
	
exit:
	
	return;
}

/*****************************************************************************/

int webdav_requestqueue_init()
{
	pthread_mutexattr_t mutexattr;
	int	index;
	int error;
	
	/* Initialize webdav_threadsockets  */
	for ( index = 0; index < WEBDAV_REQUEST_THREADS; ++index )
	{
		webdav_threadsockets[index].socket = -1;	/* closed */
		webdav_threadsockets[index].inuse = 0;	/* not in use */
	}
				
	bzero(&gwaiting_requests, sizeof(gwaiting_requests));

	error = pthread_cond_init(&gcondvar, NULL);
	if (error)
	{
		syslog(LOG_ERR, "webdav_requestqueue_init: pthread_cond_init() failed: %s", strerror(error));
		goto done;
	}

	/* set up the lock on the queues */
	error = pthread_mutexattr_init(&mutexattr);
	if (error)
	{
		syslog(LOG_ERR, "webdav_requestqueue_init: pthread_mutexattr_init() failed: %s", strerror(error));
		goto done;
	}

	error = pthread_mutex_init(&grequests_lock, &mutexattr);
	if (error)
	{
		syslog(LOG_ERR, "webdav_requestqueue_init: pthread_mutex_init() failed: %s", strerror(error));
		goto done;
	}
	
done:
	
	return ( error );
}

/*****************************************************************************/

/* webdav_requestqueue_enqueue_request
 * caller exits on errors.
 */
int webdav_requestqueue_enqueue_request(int proxy_ok, int socket)
{
	int error, unlock_error;
	webdav_requestqueue_element_t * request_element_ptr;

	error = pthread_mutex_lock(&grequests_lock);
	if (error)
	{
		syslog(LOG_ERR, "webdav_requestqueue_enqueue_request: pthread_mutex_lock(): %s", strerror(error));
		return (error);
	}

	request_element_ptr = malloc(sizeof(webdav_requestqueue_element_t));
	if (!request_element_ptr)
	{
		syslog(LOG_ERR, "webdav_requestqueue_enqueue_request: error allocating request_element_ptr");
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
		syslog(LOG_ERR, "webdav_requestqueue_enqueue_request: pthread_cond_signal(): %s", strerror(error));
		goto unlock;
	}

unlock:

	unlock_error = pthread_mutex_unlock(&grequests_lock);
	if (unlock_error)
	{
		syslog(LOG_ERR, "webdav_requestqueue_enqueue_request: pthread_mutex_unlock(): %s", strerror(unlock_error));
		if (!(error))
		{
			return (unlock_error);
		}
	}

	return (error);
}

/*****************************************************************************/

int webdav_requestqueue_enqueue_download(int remote, int local, off_t total_length,
	int chunked, int *download_status, int connection_close)
{
	int error, error2;
	webdav_requestqueue_element_t * request_element_ptr;

	error = pthread_mutex_lock(&grequests_lock);
	if (error)
	{
		syslog(LOG_ERR, "webdav_requestqueue_enqueue_download: pthread_mutex_lock(): %s", strerror(error));
		webdav_kill(-1);	/* tell the main select loop to force unmount */
		return (error);
	}

	request_element_ptr = malloc(sizeof(webdav_requestqueue_element_t));
	if (!request_element_ptr)
	{
		syslog(LOG_ERR, "webdav_requestqueue_enqueue_download: allocate request_element_ptr failed");
		error = EIO;
		goto unlock;
	}

	request_element_ptr->type = WEBDAV_DOWNLOAD_TYPE;
	request_element_ptr->element.download.remote = remote;
	request_element_ptr->element.download.local = local;
	request_element_ptr->element.download.total_length = total_length;
	request_element_ptr->element.download.chunked = chunked;
	request_element_ptr->element.download.download_status = download_status;
	request_element_ptr->element.download.connection_close = connection_close;
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
		syslog(LOG_ERR, "webdav_requestqueue_enqueue_download: pthread_cond_signal(): %s", strerror(error));
		error = EIO;
		goto unlock;
	}

unlock:

	error2 = pthread_mutex_unlock(&grequests_lock);
	if (error2)
	{
		syslog(LOG_ERR, "webdav_requestqueue_enqueue_download: pthread_mutex_lock(): %s", strerror(error2));
		webdav_kill(-1);	/* tell the main select loop to force unmount */
		if (!error)
		{
			error = error2;
		}
	}

	return (error);
}

/*****************************************************************************/

int webdav_request_thread(void *arg)
{
	#pragma unused(arg)
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
			syslog(LOG_ERR, "webdav_request_thread: pthread_mutex_lock(): %s", strerror(error));
			goto die;
		}

		/* Check to see if there is a request to process */

		if (gwaiting_requests.request_count > 0)
		{
			int	index;
			ThreadSocket *aThreadSocket; /* pointer to a free ThreadSocket */
			
			myThreadSocket = aThreadSocket = NULL;
			
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
						
						/* Find the first open ThreadSocket that isn't in use. If all open
						 * ThreadSockets are in use, use the first closed ThreadSocket
						 * that isn't in use. This has to be done while grequests_lock
						 * is locked.
						 */
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
								syslog(LOG_ERR, "webdav_request_thread: aThreadSocket is NULL");
								goto die;
							}
							
							/* we didn't find an open socket so use the first free ThreadSocket */
							myThreadSocket = aThreadSocket;
							myThreadSocket->inuse = 1;	/* mark it in use */
						}
					}
					break;

				case WEBDAV_DOWNLOAD_TYPE:
					{
						/* Find the first closed ThreadSocket that isn't in use. If all closed
						 * ThreadSockets are in use, use the last open ThreadSocket
						 * that isn't in use. This has to be done while grequests_lock
						 * is locked.
						 */
						for ( index = 0; index < WEBDAV_REQUEST_THREADS; ++index )
						{
							if ( !webdav_threadsockets[index].inuse )
							{
								/* found one not in use */
								if ( (webdav_threadsockets[index].socket < 0) )
								{
									/*  and it is closed, so grab it */
									myThreadSocket = &webdav_threadsockets[index];
									myThreadSocket->inuse = 1;	/* mark it in use */
									break;
								}
								else
								{
									/* keep track of this open one in case we don't find an closed one*/
									aThreadSocket = &webdav_threadsockets[index];
								}
							}
						}
						if ( myThreadSocket == NULL )
						{
							/* this should never happen */
							if ( aThreadSocket == NULL )
							{
								syslog(LOG_ERR, "webdav_request_thread: aThreadSocket is NULL");
								goto die;
							}
							
							/* we didn't find an closed socket so use the last free ThreadSocket */
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
				syslog(LOG_ERR, "webdav_request_thread: pthread_mutex_unlock(): %s", strerror(error));
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
					error = 0;
					
					/* close the cache file */
					(void)close(myrequest->element.download.local);
					
					if ( myrequest->element.download.remote >= 0 )
					{
						/* close the socket if needed */
						if ( myrequest->element.download.connection_close )
						{
							(void)close(myrequest->element.download.remote);
						}
						else
						{
							/* We have a socket that shouldn't be closed */
							
							/* if the myThreadSocket we found when this thread was started
							 * was still open, close it before saving the socket we just
							 * finished using.
							 */
							if ( myThreadSocket->socket >= 0 )
							{
								(void)close(myThreadSocket->socket);
							}
							/* save the socket we just finished using */
							myThreadSocket->socket = myrequest->element.download.remote;
						}
					}
					
					/* free up this ThreadSocket */
					myThreadSocket->inuse = 0;
					
					myThreadSocket = NULL;
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
				syslog(LOG_ERR, "webdav_request_thread: pthread_cond_wait(): %s", strerror(error));
				goto die;
			}

			/* Ok, unlock so that we can restart the loop */
			error = pthread_mutex_unlock(&grequests_lock);
			if (error)
			{
				syslog(LOG_ERR, "webdav_request_thread: pthread_mutex_unlock(): %s", strerror(error));
				goto die;
			}
		}
	}

die:

	/* errors coming out of this routine are fatal */
	if ( error )
	{
		syslog(LOG_ERR, "webdav_request_thread: pthread_mutex_lock(): %s", strerror(error));
		webdav_kill(-1);	/* tell the main select loop to force unmount */
	}

	return error;
}

/*****************************************************************************/
