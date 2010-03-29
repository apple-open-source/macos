/*
 * Copyright (c) 2007-2009 Apple Inc. All rights reserved.
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
#include <bsm/libbsm.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/event.h>
#include <servers/bootstrap.h>
#include <pthread.h>
#include <notify.h>
#include <sys/time.h>
#include <asl.h>
#include <asl_ipc.h>
#include <asl_ipc_server.h>
#include <asl_core.h>
#include "daemon.h"

#define forever for(;;)

#define LIST_SIZE_DELTA 256

#define SEND_NOTIFICATION 0xfadefade

#define QUERY_FLAG_SEARCH_REVERSE 0x00000001
#define SEARCH_FORWARD 1
#define SEARCH_BACKWARD -1

static pthread_mutex_t db_lock = PTHREAD_MUTEX_INITIALIZER;

extern char *asl_list_to_string(asl_search_result_t *list, uint32_t *outlen);
extern asl_search_result_t *asl_list_from_string(const char *buf);
extern boolean_t asl_ipc_server(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP);
extern uint32_t bb_convert(const char *name);

static task_name_t *client_tasks = NULL;
static uint32_t client_tasks_count = 0;

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
db_ping_store(void)
{
	if ((global.dbtype & DB_TYPE_FILE) && (global.file_db != NULL))
	{
		global.store_flags |= STORE_FLAGS_FILE_CACHE_SWEEP_REQUESTED;

		/* wake up the output worker thread */
		pthread_cond_signal(&global.work_queue_cond);
	}
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

		/*
		 * One-time store conversion from the old "LongTTL" style to the new "Best Before" style.
		 * bb_convert returns quickly if the store has already been converted.
		 */
		status = bb_convert(PATH_ASL_STORE);
		if (status != ASL_STATUS_OK)
		{
			asldebug("ASL data store conversion failed!: %s\n", asl_core_error(status));
		}

		status = asl_store_open_write(NULL, &(global.file_db));
		if (status != ASL_STATUS_OK)
		{
			asldebug("asl_store_open_write: %s\n", asl_core_error(status));
		}
		else
		{
			if (global.db_file_max != 0) asl_store_max_file_size(global.file_db, global.db_file_max);
			asl_store_signal_sweep(global.file_db);
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
 * Takes messages off the work queue and saves them.
 * Runs in it's own thread.
 */
void
output_worker()
{
	asl_msg_t **work;
	uint32_t i, count;
	mach_msg_empty_send_t *empty;
	kern_return_t kstatus;

	empty = (mach_msg_empty_send_t *)calloc(1, sizeof(mach_msg_empty_send_t));
	if (empty == NULL) return;

	forever
	{
		count = 0;

		/* blocks until work is available */
		work = asl_work_dequeue(&count);

		if (work == NULL)
		{
			if ((global.dbtype & DB_TYPE_FILE) && (global.store_flags & STORE_FLAGS_FILE_CACHE_SWEEP_REQUESTED))
			{
				asl_store_sweep_file_cache(global.file_db);
				global.store_flags &= ~STORE_FLAGS_FILE_CACHE_SWEEP_REQUESTED;
			}

			continue;
		}

		for (i = 0; i < count; i++)
		{
			asl_message_match_and_log(work[i]);
			asl_msg_release(work[i]);
		}

		free(work);

		if (global.store_flags & STORE_FLAGS_FILE_CACHE_SWEEP_REQUESTED)
		{
			asl_store_sweep_file_cache(global.file_db);
			global.store_flags &= ~STORE_FLAGS_FILE_CACHE_SWEEP_REQUESTED;
		}

		empty->header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSGH_BITS_ZERO);
		empty->header.msgh_remote_port = global.self_port;
		empty->header.msgh_local_port = MACH_PORT_NULL;
		empty->header.msgh_size = sizeof(mach_msg_empty_send_t);
		empty->header.msgh_id = SEND_NOTIFICATION;

		kstatus = mach_msg(&(empty->header), MACH_SEND_MSG | MACH_SEND_TIMEOUT, empty->header.msgh_size, 0, MACH_PORT_NULL, 2, MACH_PORT_NULL);
	}
}

/*
 * Called from asl_action.c to save messgaes to the ASL data store
 */
void
db_save_message(asl_msg_t *msg)
{
	uint64_t msgid;
	uint32_t status;

	pthread_mutex_lock(&db_lock);

	db_asl_open();

	if (global.dbtype & DB_TYPE_FILE)
	{
		status = asl_store_save(global.file_db, msg);
		if (status != ASL_STATUS_OK)
		{
			/* write failed - reopen & retry */
			asldebug("asl_store_save: %s\n", asl_core_error(status));
			asl_store_close(global.file_db);
			global.file_db = NULL;

			db_asl_open();
			status = asl_store_save(global.file_db, msg);
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
		status = asl_memory_save(global.memory_db, msg, &msgid);
		if (status != ASL_STATUS_OK)
		{
			/* save failed - reopen & retry*/
			asldebug("asl_memory_save: %s\n", asl_core_error(status));
			asl_memory_close(global.memory_db);
			global.memory_db = NULL;

			db_asl_open();
			msgid = 0;
			status = asl_memory_save(global.memory_db, msg, &msgid);
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
		status = asl_mini_memory_save(global.mini_db, msg, &msgid);
		if (status != ASL_STATUS_OK)
		{
			/* save failed - reopen & retry*/
			asldebug("asl_mini_memory_save: %s\n", asl_core_error(status));
			asl_mini_memory_close(global.mini_db);
			global.mini_db = NULL;

			db_asl_open();
			status = asl_mini_memory_save(global.mini_db, msg, &msgid);
			if (status != ASL_STATUS_OK)
			{
				asldebug("(retry) asl_memory_save: %s\n", asl_core_error(status));
				asl_mini_memory_close(global.mini_db);
				global.mini_db = NULL;
			}
		}
	}


	pthread_mutex_unlock(&db_lock);

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

static void
register_session(task_name_t task_name, pid_t pid)
{
	mach_port_t previous;
	uint32_t i;

	if (task_name == MACH_PORT_NULL) return;
	if (global.dead_session_port == MACH_PORT_NULL) return;

	for (i = 0; i < client_tasks_count; i++) if (task_name == client_tasks[i]) return;

	if (client_tasks_count == 0) client_tasks = (task_name_t *)calloc(1, sizeof(task_name_t));
	else client_tasks = (task_name_t *)reallocf(client_tasks, (client_tasks_count + 1) * sizeof(task_name_t));

	if (client_tasks == NULL) return;
	client_tasks[client_tasks_count] = task_name;
	client_tasks_count++;

	asldebug("register_session: %u   PID %d\n", (unsigned int)task_name, (int)pid);

	/* register for port death notification */
	mach_port_request_notification(mach_task_self(), task_name, MACH_NOTIFY_DEAD_NAME, 0, global.dead_session_port, MACH_MSG_TYPE_MAKE_SEND_ONCE, &previous);
	mach_port_deallocate(mach_task_self(), previous);

	asl_client_count_increment();
}

static void
cancel_session(task_name_t task_name)
{
	uint32_t i;

	for (i = 0; (i < client_tasks_count) && (task_name != client_tasks[i]); i++);

	if (i >= client_tasks_count) return;

	if (client_tasks_count == 1)
	{
		free(client_tasks);
		client_tasks = NULL;
		client_tasks_count = 0;
	}
	else
	{
		for (i++; i < client_tasks_count; i++) client_tasks[i-1] = client_tasks[i];
		client_tasks_count--;
		client_tasks = (task_name_t *)reallocf(client_tasks, client_tasks_count * sizeof(task_name_t));
	}

	asldebug("cancel_session: %u\n", (unsigned int)task_name);
	mach_port_destroy(mach_task_self(), task_name);
	asl_client_count_decrement();
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
	mach_dead_name_notification_t *deadname;

	send_time.tv_sec = 0;
	send_time.tv_usec = 0;

	rqs = sizeof(asl_request_msg) + MAX_TRAILER_SIZE;
	rps = sizeof(asl_reply_msg) + MAX_TRAILER_SIZE;
	reply = (asl_reply_msg *)calloc(1, rps);
	if (reply == NULL) return;

	rbits = MACH_RCV_MSG | MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT) | MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0);
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

		kstatus = mach_msg(&(request->head), flags, 0, rqs, global.listen_set, snooze, MACH_PORT_NULL);
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

		if (request->head.msgh_id == MACH_NOTIFY_DEAD_NAME)
		{
			deadname = (mach_dead_name_notification_t *)request;
			cancel_session(deadname->not_port);
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

kern_return_t
__asl_server_message
(
	mach_port_t server,
	caddr_t message,
	mach_msg_type_number_t messageCnt,
	audit_token_t token
)
{
	asl_msg_t *m;
	char tmp[64];
	uid_t uid;
	gid_t gid;
	pid_t pid;
	kern_return_t kstatus;
	mach_port_name_t client;

	asldebug("__asl_server_message: %s\n", (message == NULL) ? "NULL" : message);

	m = asl_msg_from_string(message);
	vm_deallocate(mach_task_self(), (vm_address_t)message, messageCnt);

	uid = (uid_t)-1;
	gid = (gid_t)-1;
	pid = (gid_t)-1;
	audit_token_to_au32(token, NULL, &uid, &gid, NULL, NULL, &pid, NULL, NULL);

	client = MACH_PORT_NULL;
	kstatus = task_name_for_pid(mach_task_self(), pid, &client);
	if (kstatus == KERN_SUCCESS) register_session(client, pid);

	if (m == NULL) return KERN_SUCCESS;

	snprintf(tmp, sizeof(tmp), "%d", uid);
	asl_set(m, ASL_KEY_UID, tmp);

	snprintf(tmp, sizeof(tmp), "%d", gid);
	asl_set(m, ASL_KEY_GID, tmp);

	snprintf(tmp, sizeof(tmp), "%d", pid);
	asl_set(m, ASL_KEY_PID, tmp);

	/* verify and enqueue for processing */
	asl_enqueue_message(SOURCE_ASL_MESSAGE, NULL, m);

	return KERN_SUCCESS;
}
