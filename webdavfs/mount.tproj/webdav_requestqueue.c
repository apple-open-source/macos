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
#include "LogMessage.h"

#include <sys/syslog.h>
#include <err.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/sysctl.h>
#include "webdav_requestqueue.h"
#include "webdav_network.h"

/*****************************************************************************/

extern fsid_t	g_fsid;		/* file system id */
extern int g_vfc_typenum;
extern char g_mountPoint[MAXPATHLEN];	/* path to our mount point */

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
		
		struct serverping
		{
			u_int32_t delay;					/* used for backoff delay sending ping requests to the server */
		} serverping;

		struct seqwrite_read_rsp
		{
			struct stream_put_ctx *ctx;
		} seqwrite_read_rsp;
				
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
#define WEBDAV_SERVER_PING_TYPE 3
#define WEBDAV_SEQWRITE_MANAGER_TYPE 4

#define WEBDAV_MAX_IDLE_TIME 10		/* in seconds */


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

static int handle_request_thread(void *arg);

static int gCurrThreadCount = 0;
static int gIdleThreadCount = 0;
static pthread_attr_t gRequest_thread_attr;


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

static void notify_reconnected(void)
{
	int mib[5];
	struct statfs *buf;
	int i, count;
	size_t len;
	
	// lazily fetch our g_fsid
	if ( g_fsid.val[0] == -1 && g_fsid.val[1] == -1) {
		// Fetch mounted filesystem stats. Specify the MNT_NOWAIT flag to directly return the information
		// retained in the kernel to avoid delays caused by waiting 
		// for updated information from a file system.
		count = getmntinfo(&buf, MNT_NOWAIT);
		if (!count) {
			syslog(LOG_DEBUG, "%s: errno %d fetching mnt info", __FUNCTION__, errno);
			return;
		}
		
		len = (unsigned int)strlen(g_mountPoint);
		for (i = 0; i < count; i++)
		{		
			if ( (strcmp("webdav", buf[i].f_fstypename) == 0) &&
				(strlen(buf[i].f_mntonname) == len) &&
				(strncasecmp(buf[i].f_mntonname, g_mountPoint, len) == 0) ) {
				// found our fs
				g_fsid = buf[i].f_fsid;
				break;
			}
		}
	}
	
	if ( g_fsid.val[0] != -1 && g_fsid.val[1] != -1 ) {	
		/* setup mib for the request */
		mib[0] = CTL_VFS;
		mib[1] = g_vfc_typenum;
		mib[2] = WEBDAV_NOTIFY_RECONNECTED_SYSCTL;
		mib[3] = g_fsid.val[0];	// fsid byte 0 of reconnected file system
		mib[4] = g_fsid.val[1];	// fsid byte 1 of reconnected file system
	
		if (sysctl(mib, 5, NULL, NULL, NULL, 0) != 0)
			syslog(LOG_ERR, "%s: sysctl errno %d", __FUNCTION__, errno );
	}
	else {
		syslog(LOG_DEBUG, "%s: fsid not found for %s\n", __FUNCTION__, g_mountPoint);
	}		
}

/*****************************************************************************/

/* set the connectionstate */
void set_connectionstate(int state)
{
	int error;
	
	error = pthread_mutex_lock(&connectionstate_lock);
	require_noerr(error, pthread_mutex_lock);

	switch (state) {
		case WEBDAV_CONNECTION_DOWN:
			if (connectionstate == WEBDAV_CONNECTION_UP) {
			
				syslog(LOG_ERR, "WebDAV server is no longer responding, will keep retrying...");
				
				/* transition to DOWN state */
				connectionstate = WEBDAV_CONNECTION_DOWN;
				
				/* start pinging the server, specifying 0 delay for the 1st ping */
				requestqueue_enqueue_server_ping(0);
			}
		break;
		case WEBDAV_CONNECTION_UP:
			if (connectionstate == WEBDAV_CONNECTION_DOWN) {
				syslog(LOG_ERR, "WebDAV server is now responding normally");
				notify_reconnected();	// let the kext know the server is online
				connectionstate = WEBDAV_CONNECTION_UP;
			}
		break;
	
		default:
		break;
	}
	
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
	ssize_t n;
	
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
		LogMessage(kError, "get_request recvmsg failed error %d\n", error);
	}
	else
	{
		/* the message was too short */
		error = EINVAL;
		LogMessage(kError, "get_request got short message\n");
	}
	
	return ( error );
}

/*****************************************************************************/

static void send_reply(int so, void *data, size_t size, int error)
{
	ssize_t n;
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
		LogMessage(kError, "send_reply sendmsg failed\n");
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
	if ( !error ) {
#if DEBUG	
		LogMessage(kTrace, "handle_filesystem_request: %s(%d)\n",
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
		
		/* If the connection is down just return EBUSY, but always let UNMOUNT and INVALCACHES requests */
		/* go through regardless of the state of the connection. */
		if ( (get_connectionstate() == WEBDAV_CONNECTION_DOWN) && (operation != WEBDAV_UNMOUNT) &&
			(operation != WEBDAV_INVALCACHES) )
		{
			error = ETIMEDOUT;
			send_reply(so, (void *)&reply, sizeof(union webdav_reply), error);
		}
		else
		{
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

				case WEBDAV_WRITESEQ:
					error = filesystem_write_seq((struct webdav_request_writeseq *)key);
					send_reply(so, (void *)0, 0, error);
					break;
				
				default:
					error = ENOTSUP;
					break;
			}
		}

#if DEBUG
		LogMessage(kError, "handle_filesystem_request: error %d, %s(%d)\n", error,
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
	}
	else {
		LogMessage(kError, "handle_filesystem_request: get_request failed %d\n", error);
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
		
		LogMessage(kTrace, "pulse_thread running\n");
		
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

static int handle_request_thread(void *arg)
{
	#pragma unused(arg)
	int error;
	webdav_requestqueue_element_t * myrequest;
	struct timespec timeout;
	int idleRecheck = 0;

	while (TRUE) {
		error = pthread_mutex_lock(&requests_lock);
		require_noerr(error, pthread_mutex_lock);

		/* Check to see if there is a request to process */

		if (waiting_requests.request_count > 0) {
			/* There is a request so dequeue it */
			idleRecheck = 0;	/* reset this flag to indicate that we did find work to do */
			myrequest = waiting_requests.item_head;
			--(waiting_requests.request_count);
			if (waiting_requests.request_count > 0) {
				/* There was more than one item on */
				/* the queue so bump the pointers */
				waiting_requests.item_head = myrequest->next;
			}
			else {
				waiting_requests.item_head = waiting_requests.item_tail = 0;
			}
			
			/* Ok, now unlock the queue and go about handling the request */
			error = pthread_mutex_unlock(&requests_lock);
			require_noerr(error, pthread_mutex_unlock);

			switch (myrequest->type) {

				case WEBDAV_REQUEST_TYPE:
					handle_filesystem_request(myrequest->element.request.socket);
					break;

				case WEBDAV_DOWNLOAD_TYPE:
					/* finish the download */
					error = network_finish_download(myrequest->element.download.node, myrequest->element.download.readStreamRecPtr);
					if (error) {
						/* Set append to indicate that our download failed. It's a hack, but
						 * it should work.	Be sure to still mark the download as finished so
						 * that if we were terminated early the closer will be notified
						 */
						verify_noerr(fchflags(myrequest->element.download.node->file_fd, UF_APPEND));
						myrequest->element.download.node->file_status = WEBDAV_DOWNLOAD_ABORTED;
					}
					else {
						/* Clear flags to indicate that our download is complete. It's a hack, but
						 * it should work.	Be sure to still mark the download as finished so
						 * that if we were terminated early the closer will be notified
						 */
						verify_noerr(fchflags(myrequest->element.download.node->file_fd, 0));
						myrequest->element.download.node->file_status = WEBDAV_DOWNLOAD_FINISHED;
					}
					error = 0;
					break;

				case WEBDAV_SERVER_PING_TYPE:
					/* Send an OPTIONS request to the server. */
					network_server_ping(myrequest->element.serverping.delay);
				break;

				case WEBDAV_SEQWRITE_MANAGER_TYPE:
					/* Read the response stream of a sequential write sequence */
					network_seqwrite_manager(myrequest->element.seqwrite_read_rsp.ctx);
				break;
				
				default:
					/* nothing we can do, just get the next request */
					break;
			}

			free(myrequest);

		}
		else {
			/* There were no requests to handle.  If idleRecheck is set, then we just timed out waiting for work
			and we did one more paranoid check for work and still found no work, so its time to exit the thread.
			If idleRecheck is not set, then there is no work to do right now.  Wait on the condition variable
			and also have a max timeout to wait.  If we timeout and there was no work to do, then set idleRecheck
			flag and loop around one more time to look for work, just to be paranoid.
			
			The extra loop around with idleRecheck is for the case where a thread is waiting on condition variable,
			and it times out.  It then gets blocked waiting to acquire the request_lock while a request is being
			put on the work queue (and thus the idle thread count is greater than 0).  It then gets the request_lock
			and there is not work on the queue for it to do, but the time out has also expired. To be sure that the
			work gets picked up, we do the extra loop around to check for work. */
			if (idleRecheck == 1) {
				/* still no work to do after timing out and rechecking again for work, so exit thread */
				//LogMessage (kSysLog, "handle_request_thread - thread %d exiting since no work to do\n", pthread_self());
				gCurrThreadCount -= 1;
				error = pthread_mutex_unlock(&requests_lock);
				pthread_exit(NULL);
			}
			
			timeout.tv_sec = time(NULL) + WEBDAV_MAX_IDLE_TIME;		/* time out in seconds */
			timeout.tv_nsec = 0;

			//LogMessage (kSysLog, "handle_request_thread - thread %d idle and waiting for work\n", pthread_self());
			gIdleThreadCount += 1;				/* increment to indicate one idle thread */
			error = pthread_cond_timedwait(&requests_condvar, &requests_lock, &timeout);
			gIdleThreadCount -= 1;				/* decrement number of idle threads */
			require((error == ETIMEDOUT || error == 0), pthread_cond_wait);

			if (error == ETIMEDOUT) {
				/* time out has occurred and still no work to do.  Loop around one more time just to be paranoid that
				there is no work to do */
				//LogMessage (kSysLog, "handle_request_thread - thread %d timeout, doing one more check\n", pthread_self());
				idleRecheck = 1;
			}
			
			/* Unlock so that we can restart the loop */
			error = pthread_mutex_unlock(&requests_lock);
			require_noerr(error, pthread_mutex_unlock);
		}
	}

pthread_cond_wait:
pthread_mutex_unlock:
pthread_mutex_lock:

	/* errors coming out of this routine are fatal */
	if ( error ) {
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

	error = pthread_attr_init(&gRequest_thread_attr);
	require_noerr(error, pthread_attr_init);

	error = pthread_attr_setdetachstate(&gRequest_thread_attr, PTHREAD_CREATE_DETACHED);
	require_noerr(error, pthread_attr_setdetachstate);

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
	pthread_t request_thread;

	error = pthread_mutex_lock(&requests_lock);
	require_noerr(error, pthread_mutex_lock);

	request_element_ptr = malloc(sizeof(webdav_requestqueue_element_t));
	require_action(request_element_ptr != NULL, malloc_request_element_ptr, error = ENOMEM);

	request_element_ptr->type = WEBDAV_REQUEST_TYPE;
	request_element_ptr->element.request.socket = socket;
	request_element_ptr->next = 0;
	++(waiting_requests.request_count);

	if (!(waiting_requests.item_tail)) {
		waiting_requests.item_head = waiting_requests.item_tail = request_element_ptr;
	}
	else {
		waiting_requests.item_tail->next = request_element_ptr;
		waiting_requests.item_tail = request_element_ptr;
	}

	if (gIdleThreadCount > 0) {
		/* Already have one or more threads just waiting for work to do.  Just kick the requests_condvar to wake 
		up the threads */
		error = pthread_cond_signal(&requests_condvar);
		require_noerr(error, pthread_cond_signal);
	}
	else {
		/* No idle threads, so try to create one if we have not reached out maximum number of threads */
		if (gCurrThreadCount < WEBDAV_REQUEST_THREADS) {
			error = pthread_create(&request_thread, &gRequest_thread_attr, (void *) handle_request_thread, (void *) NULL);
			require_noerr(error, pthread_create_signal);

			gCurrThreadCount += 1;
		}
	}

pthread_create_signal:
pthread_cond_signal:
malloc_request_element_ptr:

	unlock_error = pthread_mutex_unlock(&requests_lock);
	require_noerr_action(unlock_error, pthread_mutex_unlock, error = (error == 0) ? unlock_error : error);

pthread_mutex_unlock:
pthread_mutex_lock:

	return (error);
}

/*****************************************************************************/

int requestqueue_enqueue_download(struct node_entry *node, struct ReadStreamRec *readStreamRecPtr)
{
	int error, error2;
	webdav_requestqueue_element_t * request_element_ptr;
	pthread_t request_thread;

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

	if ( waiting_requests.item_head == NULL ) {
		/* request queue was empty */
		waiting_requests.item_head = waiting_requests.item_tail = request_element_ptr;
	}
	else {
		/* this request is the new head */
		waiting_requests.item_head = request_element_ptr;
	}

	if (gIdleThreadCount > 0) {
		/* Already have one or more threads just waiting for work to do.  Just kick the requests_condvar to wake 
		up the threads */
		error = pthread_cond_signal(&requests_condvar);
		require_noerr(error, pthread_cond_signal);
	}
	else {
		/* No idle threads, so try to create one if we have not reached out maximum number of threads */
		if (gCurrThreadCount < WEBDAV_REQUEST_THREADS) {
			error = pthread_create(&request_thread, &gRequest_thread_attr, (void *) handle_request_thread, (void *) NULL);
			require_noerr(error, pthread_create_signal);

			gCurrThreadCount += 1;
		}
	}

pthread_create_signal:
pthread_cond_signal:
malloc_request_element_ptr:

	error2 = pthread_mutex_unlock(&requests_lock);
	require_noerr_action(error2, pthread_mutex_unlock, error = (error == 0) ? error2 : error; webdav_kill(-1));

pthread_mutex_unlock:
pthread_mutex_lock:

	return (error);
}

/*****************************************************************************/

int requestqueue_enqueue_server_ping(u_int32_t delay)
{
	int error, error2;
	webdav_requestqueue_element_t * request_element_ptr;
	pthread_t request_thread;

	error = pthread_mutex_lock(&requests_lock);
	require_noerr_action(error, pthread_mutex_lock, webdav_kill(-1));

	request_element_ptr = malloc(sizeof(webdav_requestqueue_element_t));
	require_action(request_element_ptr != NULL, malloc_request_element_ptr, error = EIO);

	request_element_ptr->type = WEBDAV_SERVER_PING_TYPE;
	request_element_ptr->element.serverping.delay = delay;
	
	/* Insert server pings at head of request queue. They must be executed immediately since they are */
	/* used to detect when connectivity to the host has been restored. */
	request_element_ptr->next = waiting_requests.item_head;
	++(waiting_requests.request_count);

	if ( waiting_requests.item_head == NULL ) {
		/* request queue was empty */
		waiting_requests.item_head = waiting_requests.item_tail = request_element_ptr;
	}
	else {
		/* this request is the new head */
		waiting_requests.item_head = request_element_ptr;
	}

	if (gIdleThreadCount > 0) {
		/* Already have one or more threads just waiting for work to do.  Just kick the requests_condvar to wake 
		up the threads */
		error = pthread_cond_signal(&requests_condvar);
		require_noerr(error, pthread_cond_signal);
	}
	else {
		/* No idle threads, so try to create one if we have not reached out maximum number of threads */
		if (gCurrThreadCount < WEBDAV_REQUEST_THREADS) {
			error = pthread_create(&request_thread, &gRequest_thread_attr, (void *) handle_request_thread, (void *) NULL);
			require_noerr(error, pthread_create_signal);

			gCurrThreadCount += 1;
		}
	}

pthread_create_signal:
pthread_cond_signal:
malloc_request_element_ptr:

	error2 = pthread_mutex_unlock(&requests_lock);
	require_noerr_action(error2, pthread_mutex_unlock, error = (error == 0) ? error2 : error; webdav_kill(-1));

pthread_mutex_unlock:
pthread_mutex_lock:

	return (error);
}

/*****************************************************************************/

int requestqueue_enqueue_seqwrite_manager(struct stream_put_ctx *ctx)
{
	int error, error2;
	webdav_requestqueue_element_t * request_element_ptr;
	pthread_t request_thread;

	error = pthread_mutex_lock(&requests_lock);
	require_noerr_action(error, pthread_mutex_lock, webdav_kill(-1));

	request_element_ptr = malloc(sizeof(webdav_requestqueue_element_t));
	require_action(request_element_ptr != NULL, malloc_request_element_ptr, error = EIO);

	request_element_ptr->type = WEBDAV_SEQWRITE_MANAGER_TYPE;
	request_element_ptr->element.seqwrite_read_rsp.ctx = ctx;
	
	request_element_ptr->next = waiting_requests.item_head;
	++(waiting_requests.request_count);

	if ( waiting_requests.item_head == NULL ) {
		/* request queue was empty */
		waiting_requests.item_head = waiting_requests.item_tail = request_element_ptr;
	}
	else {
		/* this request is the new head */
		waiting_requests.item_head = request_element_ptr;
	}

	if (gIdleThreadCount > 0) {
		/* Already have one or more threads just waiting for work to do.  Just kick the requests_condvar to wake 
		up the threads */
		error = pthread_cond_signal(&requests_condvar);
		require_noerr(error, pthread_cond_signal);
	}
	else {
		/* No idle threads, so try to create one if we have not reached out maximum number of threads */
		if (gCurrThreadCount < WEBDAV_REQUEST_THREADS) {
			error = pthread_create(&request_thread, &gRequest_thread_attr, (void *) handle_request_thread, (void *) NULL);
			require_noerr(error, pthread_create_signal);

			gCurrThreadCount += 1;
		}
	}

pthread_create_signal:
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
