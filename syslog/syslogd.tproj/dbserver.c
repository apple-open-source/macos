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
#include <asl_store.h>
#include "daemon.h"

#define forever for(;;)

#define LIST_SIZE_DELTA 256
#define MAX_PRE_DISASTER_COUNT 64
#define MAX_DISASTER_COUNT LIST_SIZE_DELTA

#define SEND_NOTIFICATION 0xfadefade

#define QUERY_FLAG_SEARCH_REVERSE 0x00000001
#define SEARCH_FORWARD 1
#define SEARCH_BACKWARD -1

static asl_store_t *store = NULL;
static int disaster_occurred = 0;

extern mach_port_t server_port;
extern int archive_enable;
extern uint64_t db_curr_size;
extern uint64_t db_curr_empty;

static pthread_mutex_t db_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

extern char *asl_list_to_string(asl_search_result_t *list, uint32_t *outlen);
extern asl_search_result_t *asl_list_from_string(const char *buf);

static time_t last_archive_sod = 0;

static asl_search_result_t work_queue = {0, 0, NULL};
static asl_search_result_t disaster_log = {0, 0, NULL};

extern boolean_t asl_ipc_server
(
	mach_msg_header_t *InHeadP,
	mach_msg_header_t *OutHeadP
);

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
disaster_message(asl_msg_t *m)
{
	uint32_t i;

	if (disaster_occurred == 0)
	{
		/* retain last MAX_PRE_DISASTER_COUNT messages */
		while (disaster_log.count >= MAX_PRE_DISASTER_COUNT)
		{
			asl_msg_release(disaster_log.msg[0]);
			for (i = 1; i < disaster_log.count; i++) disaster_log.msg[i - 1] = disaster_log.msg[i];
			disaster_log.count--;
		}
	}

	if (disaster_log.count < MAX_DISASTER_COUNT) list_append_msg(&disaster_log, m, 1);
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
	msg->header.msgh_remote_port = server_port;
	msg->header.msgh_local_port = MACH_PORT_NULL;
	msg->header.msgh_size = sizeof(mach_msg_empty_send_t);
	msg->header.msgh_id = SEND_NOTIFICATION;

	forever
	{
		count = 0;
		work = db_dequeue(&count);

		if (work == NULL) continue;

		pthread_mutex_lock(&db_lock);

		if (store == NULL)
		{
			status = asl_store_open(_PATH_ASL_DB, 0, &store);
			if (status != ASL_STATUS_OK)
			{
				disaster_occurred = 1;
				store = NULL;
			}
		}

		for (i = 0; (i < count) && (store != NULL); i++)
		{
			msgid = 0;
			status = ASL_STATUS_OK;

			status = asl_store_save(store, work[i], -1, -1, &msgid);
			if (status != ASL_STATUS_OK)
			{
				/* write failed - reopen store */
				asl_store_close(store);
				store = NULL;

				status = asl_store_open(_PATH_ASL_DB, 0, &store);
				if (status != ASL_STATUS_OK)
				{
					disaster_occurred = 1;
					store = NULL;
				}

				/* if the store re-opened, retry the save */
				if (store != NULL)
				{
					status = asl_store_save(store, work[i], -1, -1, &msgid);
					if (status != ASL_STATUS_OK)
					{
						disaster_occurred = 1;
						store = NULL;
					}
				}
			}

			if ((i % 500) == 499)
			{
				pthread_mutex_unlock(&db_lock);
				pthread_mutex_lock(&db_lock);
			}
		}

		db_curr_size = 0;
		db_curr_empty = 0;

		if (store != NULL)
		{
			db_curr_size = (store->record_count + 1) * DB_RECORD_LEN;
			db_curr_empty = store->empty_count * DB_RECORD_LEN;
		}

		pthread_mutex_unlock(&db_lock);

		for (i = 0; i < count; i++) asl_msg_release(work[i]);
		free(work);

		kstatus = mach_msg(&(msg->header), MACH_SEND_MSG, msg->header.msgh_size, 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
	}
}

static char *
disaster_query(aslresponse query, uint32_t *outlen)
{
	asl_search_result_t res;
	uint32_t i, j, match;
	char *out;

	if (outlen == NULL) return NULL;
	*outlen = 0;

	if ((query == NULL) || ((query != NULL) && (query->count == 0))) return asl_list_to_string(&disaster_log, outlen);

	memset(&res, 0, sizeof(asl_search_result_t));

	for (i = 0; i < disaster_log.count; i++)
	{
		match = 1;

		for (j = 0; (j < query->count) && (match == 1); j++)
		{
			match = asl_msg_cmp(query->msg[j], disaster_log.msg[i]);
		}

		if (match == 1) list_append_msg(&res, disaster_log.msg[i], 0);
	}

	if (res.count == 0) return NULL;

	out = asl_list_to_string((aslresponse)&res, outlen);
	free(res.msg);
	return out;
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

	if (store == NULL)
	{
		status = asl_store_open(_PATH_ASL_DB, 0, &store);
		if (status != ASL_STATUS_OK) store = NULL;
	}

	status = asl_store_match(store, query, res, lastid, startid, ucount, dir, ruid, rgid);

	pthread_mutex_unlock(&db_lock);

	return status;
}

/*
 * Prune the database.
 */
uint32_t
db_prune(aslresponse query)
{
	uint32_t status;

	if (disaster_occurred == 1) return ASL_STATUS_FAILED;

	pthread_mutex_lock(&db_lock);

	if (store == NULL)
	{
		status = asl_store_open(_PATH_ASL_DB, 0, &store);
		if (status != ASL_STATUS_OK) store = NULL;
	}
	
	status = asl_store_prune(store, query);

	pthread_mutex_unlock(&db_lock);

	return status;
}

/*
 * Database archiver
 */
uint32_t
db_archive(time_t cut, uint64_t max_size)
{
	time_t sod;
	struct tm ctm;
	char *archive_file_name;
	uint32_t status;

	archive_file_name = NULL;
	memset(&ctm, 0, sizeof(struct tm));

	if (localtime_r((const time_t *)&cut, &ctm) == NULL) return ASL_STATUS_FAILED;

	if (archive_enable != 0)
	{
		asprintf(&archive_file_name, "%s/asl.%d.%02d.%02d.archive", _PATH_ASL_DIR, ctm.tm_year + 1900, ctm.tm_mon + 1, ctm.tm_mday);
		if (archive_file_name == NULL) return ASL_STATUS_NO_MEMORY;
	}

	ctm.tm_sec = 0;
	ctm.tm_min = 0;
	ctm.tm_hour = 0;

	sod = mktime(&ctm);

	/* if the day changed, archive message received before the start of the day */
	if (sod > last_archive_sod)
	{
		last_archive_sod = sod;
		status = db_archive(sod - 1, 0);
		/* NB return status not checked */
	}

	pthread_mutex_lock(&db_lock);

	if (store == NULL)
	{
		status = asl_store_open(_PATH_ASL_DB, 0, &store);
		if (status != ASL_STATUS_OK) store = NULL;
	}

	status = asl_store_archive(store, cut, archive_file_name);
	if (status == ASL_STATUS_OK) status = asl_store_compact(store);
	if ((status == ASL_STATUS_OK) && (max_size > 0)) status = asl_store_truncate(store, max_size, archive_file_name);

	db_curr_size = 0;
	db_curr_empty = 0;

	if (store != NULL)
	{
		db_curr_size = (store->record_count + 1) * DB_RECORD_LEN;
		db_curr_empty = store->empty_count * DB_RECORD_LEN;
	}
	
	pthread_mutex_unlock(&db_lock);

	if (archive_file_name != NULL) free(archive_file_name);

	return status;
}

uint32_t
db_compact(void)
{
	uint32_t status;

	pthread_mutex_lock(&db_lock);
	
	if (store == NULL)
	{
		status = asl_store_open(_PATH_ASL_DB, 0, &store);
		if (status != ASL_STATUS_OK)
		{
			pthread_mutex_unlock(&db_lock);
			return status;
		}
	}
	
	status = asl_store_compact(store);
	
	db_curr_size = (store->record_count + 1) * DB_RECORD_LEN;
	db_curr_empty = store->empty_count * DB_RECORD_LEN;
	
	pthread_mutex_unlock(&db_lock);

	return status;
}

/*
 * Receives messages on the "com.apple.system.logger" mach port.
 * Services database search and pruning requests.
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

		request->head.msgh_local_port = server_port;
		request->head.msgh_size = rqs;

		memset(reply, 0, rps);

		flags = rbits;
		if (snooze != 0) flags |= MACH_RCV_TIMEOUT;

		kstatus = mach_msg(&(request->head), flags, 0, rqs, server_port, snooze, MACH_PORT_NULL);
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

	/*
	 * If the database went offline (i.e. the filesystem died),
	 * just search the disaster_log messages.
	 */
	if (disaster_occurred == 1)
	{
		out = NULL;
		outlen = 0;
		out = disaster_query(query, &outlen);
		aslresponse_free(query);
	}
	else
	{
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
	}

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
	aslresponse query;

	*status = ASL_STATUS_OK;

	if (request == NULL) return KERN_SUCCESS;

	query = asl_list_from_string(request);

	vm_deallocate(mach_task_self(), (vm_address_t)request, requestCnt);

	/* only root may prune the database */
	if (token->val[0] != 0)
	{
		*status = ASL_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	*status = db_prune(query);
	aslresponse_free(query);

	return KERN_SUCCESS;
}
