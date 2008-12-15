/*
 * Copyright (c) 2007-2008 Apple Inc. All rights reserved.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/fcntl.h>
#include <sys/signal.h>
#include <sys/errno.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/event.h>
#include <servers/bootstrap.h>
#include <pthread.h>
#include <notify.h>
#include <sys/time.h>
#include <asl.h>
#include <asl_ipc.h>
#include <asl_core.h>
#include "daemon.h"

#define forever for(;;)

#define LIST_SIZE_DELTA 256

#define SEND_NOTIFICATION 0xfadefade

#define QUERY_FLAG_SEARCH_REVERSE 0x00000001
#define SEARCH_FORWARD 1
#define SEARCH_BACKWARD -1

static pthread_mutex_t db_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

extern char *asl_list_to_string(asl_search_result_t *list, uint32_t *outlen);
extern asl_search_result_t *asl_list_from_string(const char *buf);
extern boolean_t asl_ipc_server(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP);

static asl_search_result_t work_queue = {0, 0, NULL};

static time_t file_sweep_last = 0;

typedef union
{
	mach_msg_header_t head;
	union __RequestUnion__asl_ipc_subsystem request;
} asl_request_msg;

typedef union
{
	mach_msg_header_t head;
	union __ReplyUnion__asl_ipc_subsystem reply;
} asl_reply_msg;

void
list_append_msg(asl_search_result_t *list, asl_msg_t *msg, uint32_t retain)
{
	if (list == NULL) return;
	if (msg == NULL) return;

	/*
	 * NB: curr is the list size
	 * grow list if necessary
	 */
	if (list->count == list->curr)
	{
		if (list->curr == 0)
		{
			list->msg = (asl_msg_t **)calloc(LIST_SIZE_DELTA, sizeof(asl_msg_t *));
		}
		else
		{
			list->msg = (asl_msg_t **)reallocf(list->msg, (list->curr + LIST_SIZE_DELTA) * sizeof(asl_msg_t *));
		}

		if (list->msg == NULL)
		{
			list->curr = 0;
			list->count = 0;
			return;
		}

		list->curr += LIST_SIZE_DELTA;
	}

	if (retain != 0) asl_msg_retain(msg);
	list->msg[list->count] = msg;
	list->count++;
}

void
db_ping_store(time_t now)
{
	time_t delta;

	if ((global.dbtype & DB_TYPE_FILE) && (global.file_db != NULL))
	{
		delta = now - file_sweep_last; 
		if (delta >= global.asl_store_ping_time)
		{
			asl_store_sweep_file_cache(global.file_db);
			file_sweep_last = now;
		}
	}
}

void
db_enqueue(asl_msg_t *m)
{
	if (m == NULL) return;

	pthread_mutex_lock(&queue_lock);
	list_append_msg(&work_queue, m, 1);
	pthread_mutex_unlock(&queue_lock);
	pthread_cond_signal(&queue_cond);
}

asl_msg_t **
db_dequeue(uint32_t *count)
{
	asl_msg_t **work;

	pthread_mutex_lock(&queue_lock);
	pthread_cond_wait(&queue_cond, &queue_lock);

	work = NULL;
	*count = 0;

	if (work_queue.count == 0)
	{
		pthread_mutex_unlock(&queue_lock);
		return NULL;
	}

	work = work_queue.msg;
	*count = work_queue.count;

	work_queue.count = 0;
	work_queue.curr = 0;
	work_queue.msg = NULL;

	pthread_mutex_unlock(&queue_lock);
	return work;
}

void
db_asl_open()
{
	uint32_t status;
	struct stat sb;

	if ((global.dbtype & DB_TYPE_FILE) && (global.file_db == NULL))
	{
		memset(&sb, 0, sizeof(struct stat));
		if (stat(PATH_ASL_STORE, &sb) == 0)
		{
			/* must be a directory */
			if ((sb.st_mode & S_IFDIR) == 0)
			{
				asldebug("error: %s is not a directory", PATH_ASL_STORE);
				return;
			}
		}
		else
		{
			if (errno == ENOENT)
			{
				/* /var/log/asl doesn't exist - create it */
				if (mkdir(PATH_ASL_STORE, 0755) != 0)
				{
					asldebug("error: can't create data store %s: %s\n", PATH_ASL_STORE, strerror(errno));
					return;
				}				
			}
			else
			{
				/* stat failed for some other reason */
				asldebug("error: can't stat data store %s: %s\n", PATH_ASL_STORE, strerror(errno));
				return;
			}
		}

		status = asl_store_open_write(NULL, &(global.file_db));
		if (status != ASL_STATUS_OK)
		{
			asldebug("asl_store_open_write: %s\n", asl_core_error(status));
		}
		else
		{
			if (global.db_file_max != 0) asl_store_max_file_size(global.file_db, global.db_file_max);
		}

		if (global.did_store_sweep == 0)
		{
			status = asl_store_signal_sweep(global.file_db);
			if (status == ASL_STATUS_OK) global.did_store_sweep = 1;
		}
	}

	if ((global.dbtype & DB_TYPE_MEMORY) && (global.memory_db == NULL))
	{
		status = asl_memory_open(global.db_memory_max, &(global.memory_db));
		if (status != ASL_STATUS_OK)
		{
			asldebug("asl_memory_open: %s\n", asl_core_error(status));
		}
	}

	if ((global.dbtype & DB_TYPE_MINI) && (global.mini_db == NULL))
	{
		status = asl_mini_memory_open(global.db_mini_max, &(global.mini_db));
		if (status != ASL_STATUS_OK)
		{
			asldebug("asl_mini_memory_open: %s\n", asl_core_error(status));
		}
	}
}

/*
 * Takes messages off the work queue and saves them in the database.
 * Runs in it's own thread.
 */
void
db_worker()
{
	asl_msg_t **work;
	uint64_t msgid;
	uint32_t i, count, status;
	mach_msg_empty_send_t *msg;
	kern_return_t kstatus;

	msg = (mach_msg_empty_send_t *)calloc(1, sizeof(mach_msg_empty_send_t));
	if (msg == NULL) return;

	msg->header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSGH_BITS_ZERO);
	msg->header.msgh_remote_port = global.server_port;
	msg->header.msgh_local_port = MACH_PORT_NULL;
	msg->header.msgh_size = sizeof(mach_msg_empty_send_t);
	msg->header.msgh_id = SEND_NOTIFICATION;

	forever
	{
		count = 0;
		work = db_dequeue(&count);

		if (work == NULL) continue;

		pthread_mutex_lock(&db_lock);

		db_asl_open();

		for (i = 0; i < count; i++)
		{
			if (global.dbtype & DB_TYPE_FILE)
			{
				status = asl_store_save(global.file_db, work[i]);
				if (status != ASL_STATUS_OK)
				{
					/* write failed - reopen & retry */
					asldebug("asl_store_save: %s\n", asl_core_error(status));
					asl_store_close(global.file_db);
					global.file_db = NULL;

					db_asl_open();
					status = asl_store_save(global.file_db, work[i]);
					if (status != ASL_STATUS_OK)
					{
						asldebug("(retry) asl_store_save: %s\n", asl_core_error(status));
						asl_store_close(global.file_db);
						global.file_db = NULL;

						global.dbtype |= DB_TYPE_MEMORY;
						if (global.memory_db == NULL)
						{
							status = asl_memory_open(global.db_memory_max, &(global.memory_db));
							if (status != ASL_STATUS_OK)
							{
								asldebug("asl_memory_open: %s\n", asl_core_error(status));
							}
						}
					}
				}
			}

			if (global.dbtype & DB_TYPE_MEMORY)
			{
				msgid = 0;
				status = asl_memory_save(global.memory_db, work[i], &msgid);
				if (status != ASL_STATUS_OK)
				{
					/* save failed - reopen & retry*/
					asldebug("asl_memory_save: %s\n", asl_core_error(status));
					asl_memory_close(global.memory_db);
					global.memory_db = NULL;

					db_asl_open();
					msgid = 0;
					status = asl_memory_save(global.memory_db, work[i], &msgid);
					if (status != ASL_STATUS_OK)
					{
						asldebug("(retry) asl_memory_save: %s\n", asl_core_error(status));
						asl_memory_close(global.memory_db);
						global.memory_db = NULL;
					}
				}
			}

			if (global.dbtype & DB_TYPE_MINI)
			{
				status = asl_mini_memory_save(global.mini_db, work[i], &msgid);
				if (status != ASL_STATUS_OK)
				{
					/* save failed - reopen & retry*/
					asldebug("asl_mini_memory_save: %s\n", asl_core_error(status));
					asl_mini_memory_close(global.mini_db);
					global.mini_db = NULL;

					db_asl_open();
					status = asl_mini_memory_save(global.mini_db, work[i], &msgid);
					if (status != ASL_STATUS_OK)
					{
						asldebug("(retry) asl_memory_save: %s\n", asl_core_error(status));
						asl_mini_memory_close(global.mini_db);
						global.mini_db = NULL;
					}
				}
			}

			if ((i % 500) == 499)
			{
				pthread_mutex_unlock(&db_lock);
				pthread_mutex_lock(&db_lock);
			}
		}

		pthread_mutex_unlock(&db_lock);

		for (i = 0; i < count; i++) asl_msg_release(work[i]);
		free(work);

		kstatus = mach_msg(&(msg->header), MACH_SEND_MSG, msg->header.msgh_size, 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
	}
}

void
disaster_message(asl_msg_t *msg)
{
	uint64_t msgid;
	uint32_t status;

	msgid = 0;

	if ((global.dbtype & DB_TYPE_MINI) == 0)
	{
		if (global.mini_db == NULL)
		{
			status = asl_mini_memory_open(global.db_mini_max, &(global.mini_db));
			if (status != ASL_STATUS_OK) asldebug("asl_mini_memory_open: %s\n", asl_core_error(status));
			else asl_mini_memory_save(global.mini_db, msg, &msgid);
		}
	}
}

/*
 * Do a database search.
 */
uint32_t
db_query(aslresponse query, aslresponse *res, uint64_t startid, int count, int flags, uint64_t *lastid, int32_t ruid, int32_t rgid)
{
	uint32_t status, ucount;
	int32_t dir;

	ucount = count;
	dir = SEARCH_FORWARD;
	if (flags & QUERY_FLAG_SEARCH_REVERSE) dir = SEARCH_BACKWARD;

	pthread_mutex_lock(&db_lock);

	status = ASL_STATUS_FAILED;

	if (global.dbtype & DB_TYPE_MEMORY) status = asl_memory_match(global.memory_db, query, res, lastid, startid, ucount, dir, ruid, rgid);
	else status = asl_mini_memory_match(global.mini_db, query, res, lastid, startid, ucount, dir);

	pthread_mutex_unlock(&db_lock);

	return status;
}

/*
 * Receives messages on the "com.apple.system.logger" mach port.
 * Services database search requests.
 * Runs in it's own thread.
 */
void
database_server()
{
	kern_return_t kstatus;
	asl_request_msg *request;
	asl_reply_msg *reply;
	uint32_t rqs, rps;
	uint32_t rbits, sbits;
	uint32_t flags, snooze;
	struct timeval now, send_time;

	send_time.tv_sec = 0;
	send_time.tv_usec = 0;

	rqs = sizeof(asl_request_msg) + MAX_TRAILER_SIZE;
	rps = sizeof(asl_reply_msg) + MAX_TRAILER_SIZE;
	reply = (asl_reply_msg *)calloc(1, rps);
	if (reply == NULL) return;

	rbits = MACH_RCV_MSG | MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_SENDER) | MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0);
	sbits = MACH_SEND_MSG | MACH_SEND_TIMEOUT;

	forever
	{
		snooze = 0;
		now.tv_sec = 0;
		now.tv_usec = 0;

		/* Check if it's time to post a database change notification */
		if (send_time.tv_sec != 0)
		{
			gettimeofday(&now, NULL);
			if ((now.tv_sec > send_time.tv_sec) || ((now.tv_sec == send_time.tv_sec) && (now.tv_usec > send_time.tv_usec)))
			{
				notify_post(ASL_DB_NOTIFICATION);
				notify_post(SELF_DB_NOTIFICATION);
				send_time.tv_sec = 0;
				send_time.tv_usec = 0;
				snooze = 0;
			}
			else
			{
				/* mach_msg timeout is in milliseconds */
				snooze = ((send_time.tv_sec - now.tv_sec) * 1000) + ((send_time.tv_usec - now.tv_usec) / 1000);
			}
		}

		request = (asl_request_msg *)calloc(1, rqs);
		if (request == NULL) continue;

		request->head.msgh_local_port = global.server_port;
		request->head.msgh_size = rqs;

		memset(reply, 0, rps);

		flags = rbits;
		if (snooze != 0) flags |= MACH_RCV_TIMEOUT;

		kstatus = mach_msg(&(request->head), flags, 0, rqs, global.server_port, snooze, MACH_PORT_NULL);
		if (request->head.msgh_id == SEND_NOTIFICATION)
		{
			if (send_time.tv_sec == 0)
			{
				gettimeofday(&send_time, NULL);
				send_time.tv_sec += 1;
			}

			free(request);
			continue;
		}

		kstatus = asl_ipc_server(&(request->head), &(reply->head));
		kstatus = mach_msg(&(reply->head), sbits, reply->head.msgh_size, 0, MACH_PORT_NULL, 10, MACH_PORT_NULL);
		if (kstatus == MACH_SEND_INVALID_DEST)
		{
			mach_port_destroy(mach_task_self(), request->head.msgh_remote_port);
		}

		free(request);
	}
}

kern_return_t
__asl_server_query
(
 mach_port_t server,
 caddr_t request,
 mach_msg_type_number_t requestCnt,
 uint64_t startid,
 int count,
 int flags,
 caddr_t *reply,
 mach_msg_type_number_t *replyCnt,
 uint64_t *lastid,
 int *status,
 security_token_t *token
)
{
	aslresponse query;
	aslresponse res;
	char *out, *vmbuffer;
	uint32_t outlen;
	kern_return_t kstatus;

	*status = ASL_STATUS_OK;
	query = asl_list_from_string(request);
	vm_deallocate(mach_task_self(), (vm_address_t)request, requestCnt);
	res = NULL;

	*status = db_query(query, &res, startid, count, flags, lastid, token->val[0], token->val[1]);

	aslresponse_free(query);
	if (*status != ASL_STATUS_OK)
	{
		if (res != NULL) aslresponse_free(res);
		return KERN_SUCCESS;
	}

	out = NULL;
	outlen = 0;
	out = asl_list_to_string((asl_search_result_t *)res, &outlen);
	aslresponse_free(res);

	if ((out == NULL) || (outlen == 0)) return KERN_SUCCESS;

	kstatus = vm_allocate(mach_task_self(), (vm_address_t *)&vmbuffer, outlen, TRUE);
	if (kstatus != KERN_SUCCESS)
	{
		free(out);
		return kstatus;
	}

	memmove(vmbuffer, out, outlen);
	free(out);

	*reply = vmbuffer;
	*replyCnt = outlen;

	return KERN_SUCCESS;
}


kern_return_t
__asl_server_query_timeout
(
 mach_port_t server,
 caddr_t request,
 mach_msg_type_number_t requestCnt,
 uint64_t startid,
 int count,
 int flags,
 caddr_t *reply,
 mach_msg_type_number_t *replyCnt,
 uint64_t *lastid,
 int *status,
 security_token_t *token
 )
{
	return __asl_server_query(server, request, requestCnt, startid, count, flags, reply, replyCnt, lastid, status, token);
}

kern_return_t
__asl_server_prune
(
	mach_port_t server,
	caddr_t request,
	mach_msg_type_number_t requestCnt,
	int *status,
	security_token_t *token
)
{
	return KERN_SUCCESS;
}
