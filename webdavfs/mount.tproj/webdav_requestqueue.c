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

#include "webdavd.h"

#include <sys/syslog.h>
#include <err.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include "webdav_requestqueue.h"
#include "webdav_network.h"

/*****************************************************************************/

/* structure */

typedef struct webdav_requestqueue_element_tag
{
	struct webdav_requestqueue_element_tag *next;
	int type;
	union
	{
		struct request
		{
			int socket;							/* socket for connection */
		} request;								/* Struct used for requests from the kernel */

		struct download
		{
			struct node_entry *node;			/* the node */
			struct ReadStreamRec *readStreamRecPtr; /* the ReadStreamRec */
		} download;								/* Struct used for download requests */
		
	} element;
} webdav_requestqueue_element_t;

typedef struct
{
	webdav_requestqueue_element_t *item_head;
	webdav_requestqueue_element_t *item_tail;
	int request_count;
} webdav_requestqueue_header_t;

/*****************************************************************************/

/* Definitions */
#define WEBDAV_REQUEST_TYPE 1
#define WEBDAV_DOWNLOAD_TYPE 2

/* connectionstate_lock used to make connectionstate thread safe */
static pthread_mutex_t connectionstate_lock;
/* connectionstate is set to either WEBDAV_CONNECTION_UP or WEBDAV_CONNECTION_DOWN */
static int connectionstate;

static pthread_mutex_t requests_lock;
static pthread_cond_t requests_condvar;
static webdav_requestqueue_header_t waiting_requests;

static pthread_mutex_t pulse_lock;
static pthread_cond_t pulse_condvar;
static int purge_cache_files;	/* TRUE if closed cache files should be immediately removed from file cache */

static
int handle_request_thread(void *arg);

/*****************************************************************************/

/* get the connectionstate */
int get_connectionstate(void)
{
	int error;
	int result = 1; /* return bad if we cannot lock the mutex */
	
	error = pthread_mutex_lock(&connectionstate_lock);
	require_noerr(error, pthread_mutex_lock);
	
	result = connectionstate;
	
	error = pthread_mutex_unlock(&connectionstate_lock);
	require_noerr(error, pthread_mutex_unlock);
	
pthread_mutex_unlock:
pthread_mutex_lock:

	return ( result );
}

/*****************************************************************************/

/* set the connectionstate */
void set_connectionstate(int state)
{
	int error;
	
	error = pthread_mutex_lock(&connectionstate_lock);
	require_noerr(error, pthread_mutex_lock);
	
	connectionstate = state;
	
	error = pthread_mutex_unlock(&connectionstate_lock);
	require_noerr(error, pthread_mutex_unlock);

pthread_mutex_unlock:
pthread_mutex_lock:
	
	return;
}

/*****************************************************************************/

static int get_request(int so, int *operation, void *key, size_t klen)
{
	int error;
	struct iovec iov[2];
	struct msghdr msg;
	int n;
	
	iov[0].iov_base = (caddr_t)operation;
	iov[0].iov_len = sizeof(int);
	iov[1].iov_base = key;
	iov[1].iov_len = klen;
	
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = iov;
	msg.msg_iovlen = 2;
	
	n = recvmsg(so, &msg, 0);
	
	if ( n >= (int)(sizeof(int) + sizeof(struct webdav_cred)) )
	{
		/* the message received is large enough to contain operation and webdav_cred */
		error = 0;
		/* terminate the string (if any) at the end of the key */
		n -= sizeof(int);
		((char *)key)[n] = '\0';
	}
	else if ( n < 0 )
	{
		/* error from recvmsg */
		error = errno;
		debug_string("recvmsg failed");
	}
	else
	{
		/* the message was too short */
		error = EINVAL;
		debug_string("short message");
	}
	
	return ( error );
}

/*****************************************************************************/

static void send_reply(int so, void *data, size_t size, int error)
{
	int n;
	struct iovec iov[2];
	struct msghdr msg;
	int send_error = error;
	
	/* if the connection is down, let the kernel know */
	if ( get_connectionstate() == WEBDAV_CONNECTION_DOWN )
	{
		send_error |= WEBDAV_CONNECTION_DOWN_MASK;
	}
	
	iov[0].iov_base = (caddr_t)&send_error;
	iov[0].iov_len = sizeof(send_error);
	if ( size != 0 )
	{
		iov[1].iov_base = (caddr_t)data;
		iov[1].iov_len = size;
	}
	
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = iov;
	
	if ( size != 0 )
	{
		msg.msg_iovlen = 2;
	}
	else
	{
		msg.msg_iovlen = 1;
	}
	
	n = sendmsg(so, &msg, 0);
	if (n < 0)
	{
		debug_string("sendmsg failed");
	}
}

/*****************************************************************************/

static void handle_filesystem_request(int so)
{
	int error;
	int operation;
	char key[(NAME_MAX + 1) + sizeof(union webdav_request)];
	size_t num_bytes;
	char *bytes;
	union webdav_reply reply;
	
	/* get the request from the socket */
	error = get_request(so, &operation, key, sizeof(key));
	if ( !error )
	{
#ifdef DEBUG	
		syslog(LOG_ERR, "handle_filesystem_request: %s(%d)",
				(operation==WEBDAV_LOOKUP) ? "LOOKUP" :
				(operation==WEBDAV_CREATE) ? "CREATE" :
				(operation==WEBDAV_OPEN) ? "OPEN" :
				(operation==WEBDAV_CLOSE) ? "CLOSE" :
				(operation==WEBDAV_GETATTR) ? "GETATTR" :
				(operation==WEBDAV_SETATTR) ? "SETATTR" :
				(operation==WEBDAV_READ) ? "READ" :
				(operation==WEBDAV_WRITE) ? "WRITE" :
				(operation==WEBDAV_FSYNC) ? "FSYNC" :
				(operation==WEBDAV_REMOVE) ? "REMOVE" :
				(operation==WEBDAV_RENAME) ? "RENAME" :
				(operation==WEBDAV_MKDIR) ? "MKDIR" :
				(operation==WEBDAV_RMDIR) ? "RMDIR" :
				(operation==WEBDAV_READDIR) ? "READDIR" :
				(operation==WEBDAV_STATFS) ? "STATFS" :
				(operation==WEBDAV_UNMOUNT) ? "UNMOUNT" :
				(operation==WEBDAV_INVALCACHES) ? "INVALCACHES" :
				"???",
				operation
				);
#endif
		bzero((void *)&reply, sizeof(union webdav_reply));
		
		/* call the function to handle the request */
		switch ( operation )
		{
			case WEBDAV_LOOKUP:
				error = filesystem_lookup((struct webdav_request_lookup *)key,
					(struct webdav_reply_lookup *)&reply);
				send_reply(so, (void *)&reply, sizeof(struct webdav_reply_lookup), error);
				break;

			case WEBDAV_CREATE:
				error = filesystem_create((struct webdav_request_create *)key,
					(struct webdav_reply_create *)&reply);
				send_reply(so, (void *)&reply, sizeof(struct webdav_reply_create), error);
				break;

			case WEBDAV_OPEN:
				error = filesystem_open((struct webdav_request_open *)key,
					(struct webdav_reply_open *)&reply);
				send_reply(so, (void *)&reply, sizeof(struct webdav_reply_open), error);
				break;

			case WEBDAV_CLOSE:
				error = filesystem_close((struct webdav_request_close *)key);
				send_reply(so, (void *)0, 0, error);
				break;

			case WEBDAV_GETATTR:
				error = filesystem_getattr((struct webdav_request_getattr *)key,
					(struct webdav_reply_getattr *)&reply);
				send_reply(so, (void *)&reply, sizeof(struct webdav_reply_getattr), error);
				break;

			case WEBDAV_READ:
				bytes = NULL;
				num_bytes = 0;
				error = filesystem_read((struct webdav_request_read *)key,
					&bytes, &num_bytes);
				send_reply(so, (void *)bytes, (int)num_bytes, error);
				if (bytes)
				{
					free(bytes);
				}
				break;

			case WEBDAV_FSYNC:
				error = filesystem_fsync((struct webdav_request_fsync *)key);
				send_reply(so, (void *)0, 0, error);
				break;

			case WEBDAV_REMOVE:
				error = filesystem_remove((struct webdav_request_remove *)key);
				send_reply(so, (void *)0, 0, error);
				break;

			case WEBDAV_RENAME:
				error = filesystem_rename((struct webdav_request_rename *)key);
				send_reply(so, (void *)0, 0, error);
				break;

			case WEBDAV_MKDIR:
				error = filesystem_mkdir((struct webdav_request_mkdir *)key,
					(struct webdav_reply_mkdir *)&reply);
				send_reply(so, (void *)&reply, sizeof(struct webdav_reply_mkdir), error);
				break;

			case WEBDAV_RMDIR:
				error = filesystem_rmdir((struct webdav_request_rmdir *)key);
				send_reply(so, (void *)0, 0, error);
				break;

			case WEBDAV_READDIR:
				error = filesystem_readdir((struct webdav_request_readdir *)key);
				send_reply(so, (void *)0, 0, error);
				break;

			case WEBDAV_STATFS:
				error = filesystem_statfs((struct webdav_request_statfs *)key,
					(struct webdav_reply_statfs *)&reply);
				send_reply(so, (void *)&reply, sizeof(struct webdav_reply_statfs), error);
				break;
			
			case WEBDAV_UNMOUNT:
				webdav_kill(-2);	/* tell the main select loop to exit */
				send_reply(so, (void *)0, 0, error);
				break;

			case WEBDAV_INVALCACHES:
				error = filesystem_invalidate_caches((struct webdav_request_invalcaches *)key);
				send_reply(so, (void *)0, 0, error);
				break;

			default:
				error = ENOTSUP;
				break;
		}

#ifdef DEBUG
		if (error)
		{
			syslog(LOG_ERR, "handle_filesystem_request: error %d, %s(%d)", error,
					(operation==WEBDAV_LOOKUP) ? "LOOKUP" :
					(operation==WEBDAV_CREATE) ? "CREATE" :
					(operation==WEBDAV_OPEN) ? "OPEN" :
					(operation==WEBDAV_CLOSE) ? "CLOSE" :
					(operation==WEBDAV_GETATTR) ? "GETATTR" :
					(operation==WEBDAV_SETATTR) ? "SETATTR" :
					(operation==WEBDAV_READ) ? "READ" :
					(operation==WEBDAV_WRITE) ? "WRITE" :
					(operation==WEBDAV_FSYNC) ? "FSYNC" :
					(operation==WEBDAV_REMOVE) ? "REMOVE" :
					(operation==WEBDAV_RENAME) ? "RENAME" :
					(operation==WEBDAV_MKDIR) ? "MKDIR" :
					(operation==WEBDAV_RMDIR) ? "RMDIR" :
					(operation==WEBDAV_READDIR) ? "READDIR" :
					(operation==WEBDAV_STATFS) ? "STATFS" :
					(operation==WEBDAV_UNMOUNT) ? "UNMOUNT" :
					(operation==WEBDAV_INVALCACHES) ? "INVALCACHES" :
					"???",
					operation
					);
		}
#endif
	}
	else
	{
		send_reply(so, NULL, 0, error);
	}

	close(so);
}

/*****************************************************************************/

static void pulse_thread(void *arg)
{
	#pragma unused(arg)
	int error;
	struct node_entry *node;
	
	error = 0;
	while ( TRUE )
	{
		struct timespec pulsetime;

		error = pthread_mutex_lock(&pulse_lock);
		require_noerr(error, pthread_mutex_lock);
		
#ifdef DEBUG
		debug_string("Pulse thread running");
#endif
		
		node = nodecache_get_next_file_cache_node(TRUE);
		while ( node != NULL )
		{
			if ( NODE_FILE_IS_OPEN(node) )
			{
				/* open node */
				if ( !NODE_IS_DELETED(node) )
				{
					/* renew the lock if not deleted */
					(void) filesystem_lock(node);
				}
			}
			else
			{
				/* remove any closed nodes that are deleted, or that need to be aged out of the list */
				if ( NODE_IS_DELETED(node) || NODE_FILE_CACHE_INVALID(node) || purge_cache_files )
				{
					/* it's been closed for WEBDAV_CACHE_TIMEOUT seconds -- remove the node from the file cache */
					nodecache_remove_file_cache(node);
				}
			}
			node = nodecache_get_next_file_cache_node(FALSE);
		}
		
		/* now, remove any nodes in the deleted list that aren't cached */
		nodecache_free_nodes();
		
		purge_cache_files = FALSE; /* reset gPurgeCacheFiles (if it was set) */
		
		/* sleep for a while */
		pulsetime.tv_sec = time(NULL) + (gtimeout_val / 2);
		pulsetime.tv_nsec = 0;
		error = pthread_cond_timedwait(&pulse_condvar, &pulse_lock, &pulsetime);
		require((error == ETIMEDOUT || error == 0), pthread_cond_timedwait);

		/* Ok, unlock so that we can restart the loop */
		error = pthread_mutex_unlock(&pulse_lock);
		require_noerr(error, pthread_mutex_unlock);
	}

pthread_mutex_lock:
pthread_cond_timedwait:
pthread_mutex_unlock:

	if ( error )
	{
		webdav_kill(-1);	/* tell the main select loop to force unmount */
	}
}	

/*****************************************************************************/

static
int handle_request_thread(void *arg)
{
	#pragma unused(arg)
	int error;
	webdav_requestqueue_element_t * myrequest;

	while (TRUE)
	{
		error = pthread_mutex_lock(&requests_lock);
		require_noerr(error, pthread_mutex_lock);

		/* Check to see if there is a request to process */

		if (waiting_requests.request_count > 0)
		{
			/* There is a request so dequeue it */
			myrequest = waiting_requests.item_head;
			--(waiting_requests.request_count);
			if (waiting_requests.request_count > 0)
			{
				/* There was more than one item on */
				/* the queue so bump the pointers */
				waiting_requests.item_head = myrequest->next;
			}
			else
			{
				waiting_requests.item_head = waiting_requests.item_tail = 0;
			}
			
			/* Ok, now unlock the queue and go about handling the request */
			error = pthread_mutex_unlock(&requests_lock);
			require_noerr(error, pthread_mutex_unlock);

			switch (myrequest->type)
			{

				case WEBDAV_REQUEST_TYPE:
					handle_filesystem_request(myrequest->element.request.socket);
					break;

				case WEBDAV_DOWNLOAD_TYPE:
					/* finish the download */
					error = network_finish_download(myrequest->element.download.node, myrequest->element.download.readStreamRecPtr);
					if (error)
					{
						/* Set append to indicate that our download failed. It's a hack, but
						 * it should work.	Be sure to still mark the download as finished so
						 * that if we were terminated early the closer will be notified
						 */
						verify_noerr(fchflags(myrequest->element.download.node->file_fd, UF_APPEND));
						myrequest->element.download.node->file_status = WEBDAV_DOWNLOAD_ABORTED;
					}
					else
					{
						/* Clear flags to indicate that our download is complete. It's a hack, but
						 * it should work.	Be sure to still mark the download as finished so
						 * that if we were terminated early the closer will be notified
						 */
						verify_noerr(fchflags(myrequest->element.download.node->file_fd, 0));
						myrequest->element.download.node->file_status = WEBDAV_DOWNLOAD_FINISHED;
					}
					error = 0;
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
			error = pthread_cond_wait(&requests_condvar, &requests_lock);
			require_noerr(error, pthread_cond_wait);

			/* Ok, unlock so that we can restart the loop */
			error = pthread_mutex_unlock(&requests_lock);
			require_noerr(error, pthread_mutex_unlock);
		}
	}

pthread_cond_wait:
pthread_mutex_unlock:
pthread_mutex_lock:

	/* errors coming out of this routine are fatal */
	if ( error )
	{
		webdav_kill(-1);	/* tell the main select loop to force unmount */
	}

	return error;
}

/*****************************************************************************/

int requestqueue_init()
{
	int error;
	pthread_mutexattr_t mutexattr;
	pthread_t the_pulse_thread;
	pthread_attr_t the_pulse_thread_attr;
	pthread_t request_thread;
	pthread_attr_t request_thread_attr;
	int i;
	
	/* set up the lock for connectionstate */
	connectionstate = WEBDAV_CONNECTION_UP;
	
	error = pthread_mutexattr_init(&mutexattr);
	require_noerr(error, pthread_mutexattr_init);

	error = pthread_mutex_init(&connectionstate_lock, &mutexattr);
	require_noerr(error, pthread_mutex_init);
	
	/* initialize requestqueue */
	bzero(&waiting_requests, sizeof(waiting_requests));

	error = pthread_cond_init(&requests_condvar, NULL);
	require_noerr(error, pthread_cond_init);

	/* set up the lock on the queues */
	error = pthread_mutexattr_init(&mutexattr);
	require_noerr(error, pthread_mutexattr_init);

	error = pthread_mutex_init(&requests_lock, &mutexattr);
	require_noerr(error, pthread_mutex_init);

	for (i = 0; i < WEBDAV_REQUEST_THREADS; ++i)
	{
		error = pthread_attr_init(&request_thread_attr);
		require_noerr(error, pthread_attr_init);

		error = pthread_attr_setdetachstate(&request_thread_attr, PTHREAD_CREATE_DETACHED);
		require_noerr(error, pthread_attr_setdetachstate);

		error = pthread_create(&request_thread, &request_thread_attr,
			(void *)handle_request_thread, (void *)NULL);
		require_noerr(error, pthread_create);
	}

	/*
	 * Start the pulse thread
	 */
	purge_cache_files = FALSE;
	
	error = pthread_mutexattr_init(&mutexattr);
	require_noerr(error, pthread_mutexattr_init);

	error = pthread_mutex_init(&pulse_lock, &mutexattr);
	require_noerr(error, pthread_mutex_init);
	
	error = pthread_cond_init(&pulse_condvar, NULL);
	require_noerr(error, pthread_cond_init);
	
	error = pthread_attr_init(&the_pulse_thread_attr);
	require_noerr(error, pthread_attr_init);

	error = pthread_attr_setdetachstate(&the_pulse_thread_attr, PTHREAD_CREATE_DETACHED);
	require_noerr(error, pthread_attr_setdetachstate);

	error = pthread_create(&the_pulse_thread, &the_pulse_thread_attr, (void *)pulse_thread, (void *)NULL);
	require_noerr(error, pthread_create);

pthread_create:
pthread_attr_setdetachstate:
pthread_attr_init:
pthread_cond_init:
pthread_mutex_init:
pthread_mutexattr_init:

	return ( error );
}

/*****************************************************************************/

/* requestqueue_enqueue_request
 * caller exits on errors.
 */
int requestqueue_enqueue_request(int socket)
{
	int error, unlock_error;
	webdav_requestqueue_element_t * request_element_ptr;

	error = pthread_mutex_lock(&requests_lock);
	require_noerr(error, pthread_mutex_lock);

	request_element_ptr = malloc(sizeof(webdav_requestqueue_element_t));
	require_action(request_element_ptr != NULL, malloc_request_element_ptr, error = ENOMEM);

	request_element_ptr->type = WEBDAV_REQUEST_TYPE;
	request_element_ptr->element.request.socket = socket;
	request_element_ptr->next = 0;
	++(waiting_requests.request_count);

	if (!(waiting_requests.item_tail))
	{
		waiting_requests.item_head = waiting_requests.item_tail = request_element_ptr;
	}
	else
	{
		waiting_requests.item_tail->next = request_element_ptr;
		waiting_requests.item_tail = request_element_ptr;
	}

	error = pthread_cond_signal(&requests_condvar);
	require_noerr(error, pthread_cond_signal);

pthread_cond_signal:
malloc_request_element_ptr:

	unlock_error = pthread_mutex_unlock(&requests_lock);
	require_noerr_action(unlock_error, pthread_mutex_unlock, error = (error == 0) ? unlock_error : error);

pthread_mutex_unlock:
pthread_mutex_lock:

	return (error);
}

/*****************************************************************************/

int requestqueue_enqueue_download(
			struct node_entry *node,			/* the node */
			struct ReadStreamRec *readStreamRecPtr) /* the ReadStreamRec */
{
	int error, error2;
	webdav_requestqueue_element_t * request_element_ptr;

	error = pthread_mutex_lock(&requests_lock);
	require_noerr_action(error, pthread_mutex_lock, webdav_kill(-1));

	request_element_ptr = malloc(sizeof(webdav_requestqueue_element_t));
	require_action(request_element_ptr != NULL, malloc_request_element_ptr, error = EIO);

	request_element_ptr->type = WEBDAV_DOWNLOAD_TYPE;
	request_element_ptr->element.download.node = node;
	request_element_ptr->element.download.readStreamRecPtr = readStreamRecPtr;
	
	/* Insert downloads at head of request queue. They must be executed immediately since the download is holding a stream reference. */
	request_element_ptr->next = waiting_requests.item_head;
	++(waiting_requests.request_count);

	if ( waiting_requests.item_head == NULL )
	{
		/* request queue was empty */
		waiting_requests.item_head = waiting_requests.item_tail = request_element_ptr;
	}
	else
	{
		/* this request is the new head */
		waiting_requests.item_head = request_element_ptr;
	}

	error = pthread_cond_signal(&requests_condvar);
	require_noerr_action(error, pthread_cond_signal, error = EIO);

pthread_cond_signal:
malloc_request_element_ptr:

	error2 = pthread_mutex_unlock(&requests_lock);
	require_noerr_action(error2, pthread_mutex_unlock, error = (error == 0) ? error2 : error; webdav_kill(-1));

pthread_mutex_unlock:
pthread_mutex_lock:

	return (error);
}

/*****************************************************************************/

int requestqueue_purge_cache_files(void)
{
	int error;
	
	error = pthread_mutex_lock(&pulse_lock);
	require_noerr(error, pthread_mutex_lock);
	
	/* set up for a purge */
	purge_cache_files = TRUE;
	
	/* wake up pulse_thread to do the work */
	error = pthread_cond_signal(&pulse_condvar);
	require_noerr(error, pthread_cond_signal);
	
pthread_cond_signal:

	error = pthread_mutex_unlock(&pulse_lock);

pthread_mutex_lock:
	
	return ( error );
}

/*****************************************************************************/
