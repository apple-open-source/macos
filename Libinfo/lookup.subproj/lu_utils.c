/*
 * Copyright (c) 1999-2006 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <pthread.h>
#include <errno.h>
#include <notify.h>
#include <syslog.h>
#include <unistd.h>
#ifdef DEBUG
#include <asl.h>
#endif
#include "lu_utils.h"
#include "netdb_async.h"
#include "DSlibinfoMIG.h"
#include "DSlibinfoMIGAsyncReply.h"

#define MAX_LOOKUP_ATTEMPTS 10
#define _LU_MAXLUSTRLEN 256
#define QBUF_SIZE 16384
#define KVBUF_START_SIZE 128

#define LI_MESSAGE_SEND_ID  4241776
#define LI_MESSAGE_REPLY_ID 4241876

#define ILS_MAGIC_SIZE 8
#define ILS_MAGIC "ILSMAGIC"

#define L1_CACHE_NOTIFICATION_KEY_GLOBAL  "com.apple.system.DirectoryService.InvalidateCache"
#define L1_CACHE_NOTIFICATION_KEY_GROUP   "com.apple.system.DirectoryService.InvalidateCache.group"
#define L1_CACHE_NOTIFICATION_KEY_HOST    "com.apple.system.DirectoryService.InvalidateCache.host"
#define L1_CACHE_NOTIFICATION_KEY_SERVICE "com.apple.system.DirectoryService.InvalidateCache.service"
#define L1_CACHE_NOTIFICATION_KEY_USER    "com.apple.system.DirectoryService.InvalidateCache.user"

/* GLOBAL */
uint32_t gL1CacheEnabled = 1;

static const uint32_t align_32[] = { 0, 1, 2, 0, 4, 0, 0, 0, 4 };
static const uint32_t align_64[] = { 0, 1, 2, 0, 4, 0, 0, 0, 8 };

static pthread_key_t _info_key = 0;
static pthread_once_t _info_key_initialized = PTHREAD_ONCE_INIT;

static pthread_mutex_t _notify_lock = PTHREAD_MUTEX_INITIALIZER;
static int _L1_notify_token[] =
{
	-1, /* global */
	-1, /* group */
	-1, /* host */
	-1, /* service */
	-1  /* user */
};

struct _li_data_s
{
	uint32_t icount;
	uint32_t *ikey;
	void **idata;
};

typedef struct _li_async_request_s
{
	mach_port_t reply_port;
	uint32_t retry;
	uint32_t proc;
	void *context;
	void *callback;
	char request[MAX_MIG_INLINE_DATA];
	mach_msg_type_number_t requestCnt;
	char reply[MAX_MIG_INLINE_DATA];
	mach_msg_type_number_t replyCnt;
	vm_address_t ooreply;
	mach_msg_type_number_t ooreplyCnt;
	security_token_t token;
	struct _li_async_request_s *next;
} _li_async_request_t;

static pthread_mutex_t _li_worklist_lock = PTHREAD_MUTEX_INITIALIZER;
static _li_async_request_t *_li_worklist = NULL;

/* Send an asynchronous query message. */
static kern_return_t
_LI_async_send(_li_async_request_t *r)
{
	mach_msg_return_t status;
	mach_vm_address_t cb;

	if (r == NULL) return KERN_FAILURE;

	if (r->retry == 0) return MIG_SERVER_DIED;
	r->retry--;

	cb = (mach_vm_address_t)(r->callback);
	status = libinfoDSmig_Query_async(_ds_port, r->reply_port, r->proc, r->request, r->requestCnt, cb);

	if (status == MACH_SEND_INVALID_REPLY)
	{
		mach_port_mod_refs(mach_task_self(), r->reply_port, MACH_PORT_RIGHT_RECEIVE, -1);
		r->reply_port = MACH_PORT_NULL;
	}

	return status;
}

static _li_async_request_t *
_LI_worklist_remove(mach_port_t p)
{
	_li_async_request_t *r, *n;

	if (p == MACH_PORT_NULL) return NULL;
	if (_li_worklist == NULL) return NULL;

	pthread_mutex_lock(&_li_worklist_lock);

	if (_li_worklist->reply_port == p)
	{
		r = _li_worklist;
		_li_worklist = r->next;
		pthread_mutex_unlock(&_li_worklist_lock);
		return r;
	}

	for (r = _li_worklist; r != NULL; r = r->next)
	{
		n = r->next;
		if (n == NULL) break;

		if (n->reply_port == p)
		{
			r->next = n->next;
			pthread_mutex_unlock(&_li_worklist_lock);
			return n;
		}
	}

	pthread_mutex_unlock(&_li_worklist_lock);
	return NULL;
}

static _li_async_request_t *
_LI_worklist_find(mach_port_t p)
{
	_li_async_request_t *r;

	if (p == MACH_PORT_NULL) return NULL;
	if (_li_worklist == NULL) return NULL;

	pthread_mutex_lock(&_li_worklist_lock);

	for (r = _li_worklist; r != NULL; r = r->next)
	{
		if (r->reply_port == p)
		{
			pthread_mutex_unlock(&_li_worklist_lock);
			return r;
		}
	}

	pthread_mutex_unlock(&_li_worklist_lock);
	return NULL;
}

static void
_LI_free_request(_li_async_request_t *r)
{
	if (r == NULL) return;

	if (r->reply_port != MACH_PORT_NULL) mach_port_mod_refs(mach_task_self(), r->reply_port, MACH_PORT_RIGHT_RECEIVE, -1);
	r->reply_port = MACH_PORT_NULL;

	free(r);
}

/*
 * This is a callback for DSLibinfoMIGAsyncReplyServer.c
 */
__private_extern__ kern_return_t
libinfoDSmig_do_Response_async(mach_port_t server, char *reply, mach_msg_type_number_t replyCnt, vm_offset_t ooreply, mach_msg_type_number_t ooreplyCnt, mach_vm_address_t callbackAddr, security_token_t servertoken)
{
	_li_async_request_t *r;

	r = _LI_worklist_find(server);

	if (r != NULL)
	{
		r->ooreply = ooreply;
		r->ooreplyCnt = ooreplyCnt;
		if (replyCnt > 0) memcpy(r->reply, reply, replyCnt);
		r->replyCnt = replyCnt;
		r->token = servertoken;
	}
	else if (ooreplyCnt != 0)
	{
		vm_deallocate(mach_task_self(), ooreply, ooreplyCnt);
	}

	return KERN_SUCCESS;
}

/* Receive an asynchronous reply message. */
kern_return_t
LI_async_receive(mach_port_t p, kvarray_t **reply)
{
	kern_return_t status;
	_li_async_request_t *r;
	kvbuf_t *out;
	int flags;

	flags = MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT) | MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0);

	/* use mach_msg_server_once to do the work here */
	status = mach_msg_server_once(DSlibinfoMIGAsyncReply_server, 65536, p, flags);

	if (status != KERN_SUCCESS) return status;

	r = _LI_worklist_remove(p);
	if (r == NULL) return KERN_FAILURE;

	out = (kvbuf_t *)calloc(1, sizeof(kvbuf_t));
	if (out == NULL)
	{
		if (r->ooreplyCnt > 0) vm_deallocate(mach_task_self(), r->ooreply, r->ooreplyCnt);
		return KERN_FAILURE;
	}

	if (r->ooreplyCnt > 0)
	{
		out->datalen = r->ooreplyCnt;
		out->databuf = malloc(r->ooreplyCnt);
		if (out->databuf == NULL)
		{
			free(out);
			*reply = NULL;
			vm_deallocate(mach_task_self(), r->ooreply, r->ooreplyCnt);
			return KERN_FAILURE;
		}

		memcpy(out->databuf, (char *)r->ooreply, r->ooreplyCnt);
		vm_deallocate(mach_task_self(), r->ooreply, r->ooreplyCnt);
	}
	else if (r->replyCnt > 0)
	{
		out->datalen = r->replyCnt;
		out->databuf = malloc(r->replyCnt);
		if (out->databuf == NULL)
		{
			free(out);
			*reply = NULL;
			return KERN_FAILURE;
		}

		memcpy(out->databuf, r->reply, r->replyCnt);
	}

	*reply = kvbuf_decode(out);
	if (*reply == NULL)
	{
		/* DS returned no data */
		free(out->databuf);
		free(out);
	}
	
	_LI_free_request(r);

	return KERN_SUCCESS;
}

static void
_LI_worklist_append(_li_async_request_t *r)
{
	_li_async_request_t *p;

	if (r == NULL) return;

	pthread_mutex_lock(&_li_worklist_lock);

	if (_li_worklist == NULL)
	{
		_li_worklist = r;
		pthread_mutex_unlock(&_li_worklist_lock);
		return;
	}

	for (p = _li_worklist; p->next != NULL; p = p->next);
	p->next = r;

	pthread_mutex_unlock(&_li_worklist_lock);
}

void
LI_async_call_cancel(mach_port_t p, void **context)
{
	_li_async_request_t *req;

	req = _LI_worklist_remove(p);

	if (req != NULL)
	{
		if (context != NULL) *context = req->context;
		_LI_free_request(req);
	}
	else if (p != MACH_PORT_NULL)
	{
		mach_port_mod_refs(mach_task_self(), p, MACH_PORT_RIGHT_RECEIVE, -1);
	}
}

void
lu_async_call_cancel(mach_port_t p)
{
	LI_async_call_cancel(p, NULL);
}

static _li_async_request_t *
_LI_create_request(uint32_t proc, kvbuf_t *query, void *callback, void *context)
{
	_li_async_request_t *r;
	kern_return_t status;
	mach_port_t target;

	if (_ds_running() == 0) return NULL;
	if (query == NULL) return NULL;
	if (query->datalen > MAX_MIG_INLINE_DATA) return NULL;

	r = (_li_async_request_t *)calloc(1, sizeof(_li_async_request_t));
	if (r == NULL) return NULL;

	status = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &(r->reply_port));
	if (status != KERN_SUCCESS)
	{
		_LI_free_request(r);
		return NULL;
	}

	target = MACH_PORT_NULL;

	/* Request no-senders notification so we can tell when server dies */
	mach_port_request_notification(mach_task_self(), r->reply_port, MACH_NOTIFY_NO_SENDERS, 1, r->reply_port, MACH_MSG_TYPE_MAKE_SEND_ONCE, &target);

	r->retry = MAX_LOOKUP_ATTEMPTS;

	r->context = context;
	r->callback = callback;
	r->proc = proc;

	memcpy(r->request, query->databuf, query->datalen);
	r->requestCnt = query->datalen;

	r->next = NULL;

	return r;
}

kern_return_t
LI_async_start(mach_port_t *p, uint32_t proc, kvbuf_t *query, void *callback, void *context)
{
	_li_async_request_t *r;
	kern_return_t status;
	uint32_t retry;

	if (p == NULL) return KERN_FAILURE;

	*p = MACH_PORT_NULL;

	if (_ds_running() == 0) return KERN_FAILURE;
	if (_ds_port == MACH_PORT_NULL) return KERN_FAILURE;

	/* Make a request struct to keep track */
	r = _LI_create_request(proc, query, callback, context);
	if (r == NULL) return KERN_FAILURE;

	status = MIG_SERVER_DIED;
	for (retry = 0; (status == MIG_SERVER_DIED) && (retry < MAX_LOOKUP_ATTEMPTS); retry++)
	{
		status = _LI_async_send(r);
	}

	if (status != KERN_SUCCESS)
	{
		_LI_free_request(r);
		return status;
	}

	/* Add request to worklist */
	_LI_worklist_append(r);

	*p = r->reply_port;

	return KERN_SUCCESS;
}

kern_return_t
LI_async_send(mach_port_t *p, uint32_t proc, kvbuf_t *query)
{
	return LI_async_start(p, proc, query, NULL, NULL);
}

kern_return_t
LI_async_handle_reply(mach_msg_header_t *msg, kvarray_t **reply, void **callback, void **context)
{
	_li_async_request_t *req;
	kvbuf_t *out;
	kern_return_t status;
	uint32_t retry;
	mig_reply_error_t *bufReply;

	if (msg == NULL) return -1;

	/* If reply status was an error, resend */
	if (msg->msgh_id == MACH_NOTIFY_NO_SENDERS)
	{
		/* if server died */
		req = _LI_worklist_find(msg->msgh_local_port);
		if (req == NULL) return -1;

		status = MIG_SERVER_DIED;
		for (retry = 0; (status == MIG_SERVER_DIED) && (retry < MAX_LOOKUP_ATTEMPTS); retry++)
		{
			/* send message */
			status = _LI_async_send(req);
		}

		if (status != KERN_SUCCESS) return -1;

		return MIG_REPLY_MISMATCH;
	}

	/* need to implement the msg_server_once type code here */
	mach_msg_size_t reply_alloc = round_page(65536 + MAX_TRAILER_SIZE);

	status = vm_allocate(mach_task_self(), (vm_address_t *) &bufReply, reply_alloc, VM_MAKE_TAG(VM_MEMORY_MACH_MSG) | TRUE);
	if (status != KERN_SUCCESS) return status;

	status = DSlibinfoMIGAsyncReply_server(msg, (mach_msg_header_t *)bufReply);

	/* we just destroy the reply, because there isn't one */
	if (bufReply->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) mach_msg_destroy(&bufReply->Head);

	vm_deallocate(mach_task_self(), (vm_address_t) bufReply, reply_alloc);

	if (status == FALSE) return KERN_FAILURE;

	req = _LI_worklist_remove(msg->msgh_local_port);
	if (req == NULL) return KERN_FAILURE;

	*callback = req->callback;
	*context = req->context;
 
	out = (kvbuf_t *)calloc(1, sizeof(kvbuf_t));
	if (out == NULL)
	{
		if (req->ooreplyCnt > 0) vm_deallocate(mach_task_self(), req->ooreply, req->ooreplyCnt);
		return KERN_FAILURE;
	}

	if (req->ooreplyCnt > 0)
	{
		out->datalen = req->ooreplyCnt;
		out->databuf = malloc(req->ooreplyCnt);
		if (out->databuf == NULL)
		{
			free(out);
			*reply = NULL;
			vm_deallocate(mach_task_self(), req->ooreply, req->ooreplyCnt);
			return KERN_FAILURE;
		}

		memcpy(out->databuf, (char *)req->ooreply, req->ooreplyCnt);
		vm_deallocate(mach_task_self(), req->ooreply, req->ooreplyCnt);
	}
	else if (req->replyCnt > 0)
	{
		out->datalen = req->replyCnt;
		out->databuf = malloc(req->replyCnt);
		if (out->databuf == NULL)
		{
			free(out);
			*reply = NULL;
			return KERN_FAILURE;
		}

		memcpy(out->databuf, req->reply, req->replyCnt);
	}

	*reply = kvbuf_decode(out);
	if (*reply == NULL)
	{
		/* DS returned no data */
		free(out->databuf);
		free(out);
	}
	
	_LI_free_request(req);

	return KERN_SUCCESS;
}

static void
_LI_thread_info_free(void *x)
{
	struct li_thread_info *tdata;

	if (x == NULL) return;

	tdata = (struct li_thread_info *)x;
	LI_ils_free(tdata->li_entry, tdata->li_entry_size);
	LI_data_free_kvarray(tdata);

	free(tdata);
}

static void
_LI_data_free(void *x)
{
	struct _li_data_s *t;
	int i;

	if (x == NULL) return;

	t = (struct _li_data_s *)x;

	for (i = 0; i < t->icount; i++)
	{
		_LI_thread_info_free(t->idata[i]);
		t->idata[i] = NULL;
	}

	if (t->ikey != NULL) free(t->ikey);
	t->ikey = NULL;

	if (t->idata != NULL) free(t->idata);
	t->idata = NULL;

	free(t);
}

static void
_LI_data_init()
{
	pthread_key_create(&_info_key, _LI_data_free);
	return;
}

static struct _li_data_s *
_LI_data_get()
{
	struct _li_data_s *libinfo_data;

	/*
	 * Only one thread should create the _info_key
	 */
	pthread_once(&_info_key_initialized, _LI_data_init);

	/* Check if this thread already created libinfo_data */
	libinfo_data = pthread_getspecific(_info_key);
	if (libinfo_data != NULL) return libinfo_data;

	libinfo_data = (struct _li_data_s *)calloc(1, sizeof(struct _li_data_s));
	if (libinfo_data == NULL) return NULL;

	pthread_setspecific(_info_key, libinfo_data);
	return libinfo_data;
}

__private_extern__ void *
LI_data_find_key(uint32_t key)
{
	struct _li_data_s *libinfo_data;
	uint32_t i;

	libinfo_data = _LI_data_get();
	if (libinfo_data == NULL) return NULL;

	for (i = 0; i < libinfo_data->icount; i++)
	{
		if (libinfo_data->ikey[i] == key) return libinfo_data->idata[i];
	}

	return NULL;
}

__private_extern__ void *
LI_data_create_key(uint32_t key, size_t esize)
{
	struct _li_data_s *libinfo_data;
	struct li_thread_info *tdata;
	uint32_t i, n;

	libinfo_data = _LI_data_get();
	if (libinfo_data == NULL) return NULL;

	for (i = 0; i < libinfo_data->icount; i++)
	{
		if (libinfo_data->ikey[i] == key) return libinfo_data->idata[i];
	}

	i = libinfo_data->icount;
	n = i + 1;

	if (i == 0)
	{
		libinfo_data->ikey = (uint32_t *)malloc(sizeof(uint32_t));
		libinfo_data->idata = (void **)malloc(sizeof(void *));
	}
	else
	{
		libinfo_data->ikey = (uint32_t *)reallocf(libinfo_data->ikey, n * sizeof(uint32_t));
		libinfo_data->idata = (void **)reallocf(libinfo_data->idata, n * sizeof(void *));
	}

	if ((libinfo_data->ikey == NULL) || (libinfo_data->idata == NULL))
	{
		if (libinfo_data->ikey != NULL) free(libinfo_data->ikey);
		libinfo_data->ikey = NULL;

		if (libinfo_data->idata != NULL) free(libinfo_data->idata);
		libinfo_data->idata = NULL;

		return NULL;
	}

	tdata = (struct li_thread_info *)calloc(1, sizeof(struct li_thread_info));
	if (tdata == NULL) return NULL;

	tdata->li_entry_size = esize;

	libinfo_data->ikey[i] = key;
	libinfo_data->idata[i] = tdata;
	libinfo_data->icount++;

	return tdata;
}

static uint32_t
_LI_data_index(uint32_t key, struct _li_data_s *libinfo_data)
{
	uint32_t i;

	if (libinfo_data == NULL) return (uint32_t)-1;

	for (i = 0; i < libinfo_data->icount; i++)
	{
		if (libinfo_data->ikey[i] == key) return i;
	}

	return (uint32_t)-1;
}

void
_LI_data_set_key(uint32_t key, void *data)
{
	struct _li_data_s *libinfo_data;
	uint32_t i;

	libinfo_data = _LI_data_get();
	if (libinfo_data == NULL) return;

	i = _LI_data_index(key, libinfo_data);
	if (i == (uint32_t)-1) return;

	libinfo_data->idata[i] = data;
}

void *
_LI_data_get_key(uint32_t key)
{
	struct _li_data_s *libinfo_data;
	uint32_t i;

	libinfo_data = _LI_data_get();
	if (libinfo_data == NULL) return NULL;

	i = _LI_data_index(key, libinfo_data);
	if (i == (uint32_t)-1) return NULL;

	return libinfo_data->idata[i];
}

__private_extern__ void
LI_data_free_kvarray(struct li_thread_info *tdata)
{
	if (tdata == NULL) return;
	if (tdata->li_vm == NULL) return;

	kvarray_free((kvarray_t *)tdata->li_vm);
	tdata->li_vm = NULL;
}

__private_extern__ void
LI_data_recycle(struct li_thread_info *tdata, void *entry, size_t entrysize)
{
	if (tdata == NULL) return;

	LI_ils_free(tdata->li_entry, entrysize);
	tdata->li_entry = entry;
}

#define KVBUF_CHUNK 256

/*
 * kvbuf_t is a list of key/value dictionaries.
 *
 * First 4 bytes are the number of dictionaries.
 * For each dictionary, first 4 bytes is the key / value list count.
 * For each value list, first 4 bytes is the list length.
 * Keys and values are a 4-byte length followed by a nul-terminated string
 *
 * When the databuf needs to grow, we add memory in KVBUF_CHUNK size
 * increments to reduce malloc / realloc activity.
 * The _size variable stores the actual allocated size.
 * The datalen variable stores the used data size.
 *
 * The _dict variable holds an offset from the start of the buffer
 * to the "current" dictionary.  kvbuf_reset() resets this,
 * and kvbuf_next_dict() bumps the offset so that databuf + _dict
 * points to the next dictionary.
 *
 * The _key variable holds an offset from the start of the buffer
 * to the "current" key.  kvbuf_reset() resets this, and
 * kvbuf_next_key() bumps the offset so that databuf + _key
 * points to the next key.
 *
 * The _val variable holds an offset from the start of the buffer
 * to the "current" value.  kvbuf_reset() resets this, and
 * kvbuf_next_val() bumps the offset so that databuf + _val
 * points to the next value.
 *
 * The cache_entry_list_to_kvbuf() routine contains the only
 * code that builds an array.
 * 
 */

/*
 * kvbuf_query is a simple utility for constructing a
 * kvbuf with a single dictionary.  The format string may
 * contain the chars "k", "s", "i", and "u".  "k" denotes a key
 * (keys are always strings), "s" denotes a string value,
 * "i" denotes a 32 bit signed int, and "u" denotes an unsigned.
 */
__private_extern__ kvbuf_t *
kvbuf_query(char *fmt, ...)
{
	va_list ap;
	char *arg, *f, str[32];
	int32_t iarg;
	uint32_t uarg;
	kvbuf_t *kv;

	if (fmt == NULL) return NULL;

	kv = kvbuf_new();
	if (kv == NULL) return NULL;

	kvbuf_add_dict(kv);

	va_start(ap, fmt);
	for (f = fmt; (*f) != '\0'; f++)
	{
		if (*f == 'k')
		{
			arg = va_arg(ap, char *);
			kvbuf_add_key(kv, arg);
		}
		else if (*f == 's')
		{
			arg = va_arg(ap, char *);
			kvbuf_add_val(kv, arg);
		}
		else if (*f == 'i')
		{
			iarg = va_arg(ap, int32_t);
			snprintf(str, sizeof(str), "%d", iarg);
			kvbuf_add_val(kv, str);
		}
		else if (*f == 'u')
		{
			uarg = va_arg(ap,uint32_t);
			snprintf(str, sizeof(str), "%u", uarg);
			kvbuf_add_val(kv, str);
		}
	}
	va_end(ap);

	return kv;
}

__private_extern__ kvbuf_t *
kvbuf_query_key_val(const char *key, const char *val)
{
	kvbuf_t *kv;
	uint32_t x, kl, vl, vc;
	char *p;

	if (key == NULL) return NULL;

	kl = strlen(key) + 1;

	vl = 0;
	vc = 0;

	if (val != NULL) 
	{
		vl = strlen(val) + 1;
		vc = 1;
	}

	kv = (kvbuf_t *)calloc(1, sizeof(kvbuf_t));
	if (kv == NULL) return NULL;

	kv->_size = (5 * sizeof(uint32_t)) + kl + vl;
	kv->datalen = kv->_size;

	kv->databuf = calloc(1, kv->_size);
	if (kv->databuf == NULL)
	{
		free(kv);
		return NULL;
	}

	p = kv->databuf;

	/* 1 dict */
	x = htonl(1);
	memcpy(p, &x, sizeof(uint32_t));
	p += sizeof(uint32_t);

	/* 1 key */
	memcpy(p, &x, sizeof(uint32_t));
	p += sizeof(uint32_t);

	/* key length */
	x = htonl(kl);
	memcpy(p, &x, sizeof(uint32_t));
	p += sizeof(uint32_t);

	/* key */
	memcpy(p, key, kl);
	p += kl;

	/* number of values */
	x = htonl(vc);
	memcpy(p, &x, sizeof(uint32_t));
	p += sizeof(uint32_t);

	if (vc > 0)
	{
		/* value length */
		x = htonl(vl);
		memcpy(p, &x, sizeof(uint32_t));
		p += sizeof(uint32_t);

		/* value */
		memcpy(p, val, vl);
	}

	return kv;
}

__private_extern__ kvbuf_t *
kvbuf_query_key_int(const char *key, int32_t i)
{
	char str[32];

	snprintf(str, sizeof(str), "%d", i);
	return kvbuf_query_key_val(key, str);
}

__private_extern__ kvbuf_t *
kvbuf_query_key_uint(const char *key, uint32_t u)
{
	char str[32];

	snprintf(str, sizeof(str), "%u", u);
	return kvbuf_query_key_val(key, str);
}

kvbuf_t *
kvbuf_new(void)
{
	kvbuf_t *kv;

	kv = (kvbuf_t *)calloc(1, sizeof(kvbuf_t));
	if (kv == NULL) return NULL;

	kv->_size = KVBUF_START_SIZE;
	kv->databuf = calloc(1, kv->_size);
	if (kv->databuf == NULL)
	{
		free(kv);
		return NULL;
	}

	kv->datalen = sizeof(uint32_t);
	kv->_dict = kv->datalen;

	return kv;
}

kvbuf_t *
kvbuf_init(char *buffer, uint32_t length)
{
	kvbuf_t *kv;

	kv = (kvbuf_t *)calloc(1, sizeof(kvbuf_t));
	if (kv == NULL) return NULL;

	kv->_size = length;
	kv->datalen = length;
	kv->databuf = calloc(1, length);
	if (kv->databuf == NULL)
	{
		free(kv);
		kv = NULL;
	}
	else
	{
		memcpy(kv->databuf, buffer, length);
	}

	return kv;
}

static void
kvbuf_grow(kvbuf_t *kv, uint32_t delta)
{
	uint32_t newlen, n;
	char *p;

	if (kv == NULL) return;
	if (delta == 0) return;

	if (kv->databuf == NULL) delta += sizeof(uint32_t);

	n = (delta + KVBUF_CHUNK - 1) / KVBUF_CHUNK;
	newlen = kv->datalen + (n * KVBUF_CHUNK);

	if (newlen <= kv->_size) return;

	kv->_size = newlen;

	if (kv->databuf == NULL)
	{
		kv->databuf = calloc(1, kv->_size);
		if (kv->databuf == NULL)
		{
			memset(kv, 0, sizeof(kvbuf_t));
			return;
		}

		kv->datalen = sizeof(uint32_t);
		kv->_dict = sizeof(uint32_t);
	}
	else
	{
		kv->databuf = reallocf(kv->databuf, kv->_size);
		if (kv->databuf == NULL)
		{
			memset(kv, 0, sizeof(kvbuf_t));
			return;
		}

		p = kv->databuf + kv->datalen;
		memset(p, 0, kv->_size - kv->datalen);
	}
}

void
kvbuf_add_dict(kvbuf_t *kv)
{
	char *p;
	uint32_t x, dict_count;

	if (kv == NULL) return;

	/* Add a key count */
	kvbuf_grow(kv, sizeof(uint32_t));
	if (kv->databuf == NULL) return;

	kv->_dict = kv->datalen;
	kv->datalen += sizeof(uint32_t);

	kv->_key = kv->datalen;
	kv->_vlist = 0;
	kv->_val = 0;

	/* increment and rewrite the dict count */
	p = kv->databuf;

	x = 0;
	memcpy(&x, p, sizeof(uint32_t));
	dict_count = ntohl(x);

	dict_count++;
	x = htonl(dict_count);
	memcpy(p, &x, sizeof(uint32_t));
}

void
kvbuf_add_key(kvbuf_t *kv, const char *key)
{
	uint32_t kl, x, key_count, delta;
	char *p;

	if (kv == NULL) return;
	if (key == NULL) return;

	kl = strlen(key) + 1;

	/* Grow to hold key len, key, and value list count. */
	delta = (2 * sizeof(uint32_t)) + kl;
	kvbuf_grow(kv, delta);

	if (kv->databuf == NULL) return;

	/* increment and rewrite the key count for the current dictionary */
	p = kv->databuf + kv->_dict;

	x = 0;
	memcpy(&x, p, sizeof(uint32_t));
	key_count = ntohl(x);

	if (key_count == 0) kv->_key = kv->_dict + sizeof(uint32_t);
	else kv->_key = kv->datalen;

	key_count++;
	x = htonl(key_count);
	memcpy(p, &x, sizeof(uint32_t));

	/* append key to data buffer */
	p = kv->databuf + kv->datalen;

	x = htonl(kl);
	memcpy(p, &x, sizeof(uint32_t));
	p += sizeof(uint32_t);
	memcpy(p, key, kl);
	p += kl;

	kv->_vlist = kv->datalen + sizeof(uint32_t) + kl;

	x = 0;
	memcpy(p, &x, sizeof(uint32_t));

	kv->datalen += delta;
	kv->_val = kv->datalen;
}

void
kvbuf_add_val_len(kvbuf_t *kv, const char *val, uint32_t len)
{
	uint32_t x, val_count, delta;
	char *p;

	if (kv == NULL) return;
	if (val == NULL) return;
	if (len == 0) return;

	/* Grow to hold val len and value. */
	delta = sizeof(uint32_t) + len;
	kvbuf_grow(kv, delta);

	if (kv->databuf == NULL) return;

	/* increment and rewrite the value count for the value_list dictionary */
	p = kv->databuf + kv->_vlist;

	x = 0;
	memcpy(&x, p, sizeof(uint32_t));
	val_count = ntohl(x);
	val_count++;
	x = htonl(val_count);
	memcpy(p, &x, sizeof(uint32_t));

	/* append val to data buffer */
	p = kv->databuf + kv->_val;

	x = htonl(len);
	memcpy(p, &x, sizeof(uint32_t));
	p += sizeof(uint32_t);
	memcpy(p, val, len);
	p += len;

	kv->datalen += delta;
	kv->_val = kv->datalen;
}

/* 
 * WARNING!  Kludge Alert!
 *
 * This call just looks for the buffer length encoded into a serialized kvbuf_t,
 * which preceeds a pointer to a key or value.  Obviously, calling it with anything
 * other than a pointer value which is embedded in a kvbuf_t is asking for trouble.
 */
uint32_t
kvbuf_get_len(const char *p)
{
	uint32_t x;

	x = 0;
	memcpy(&x, p - sizeof(uint32_t), sizeof(uint32_t));
	return ntohl(x);
}

void
kvbuf_add_val(kvbuf_t *kv, const char *val)
{
	if (kv == NULL) return;
	if (val == NULL) return;

	kvbuf_add_val_len(kv, val, strlen(val) + 1);
}

void
kvbuf_free(kvbuf_t *kv)
{
	if (kv == NULL) return;
	if (kv->databuf != NULL) free(kv->databuf);
	memset(kv, 0, sizeof(kvbuf_t));
	free(kv);
}

/* appends a kvbuf to an existing kvbuf */
void
kvbuf_append_kvbuf(kvbuf_t *kv, const kvbuf_t *kv2)
{
	uint32_t curr_count, new_count, temp;

	if (kv == NULL) return;
	if (kv2 == NULL) return;

	curr_count = 0;
	new_count = 0;

	memcpy(&temp, kv->databuf, sizeof(uint32_t));
	curr_count = ntohl(temp);

	memcpy(&temp, kv2->databuf, sizeof(uint32_t));
	new_count = ntohl(temp);

	/* nothing to do */
	if (new_count == 0) return;

	/* add the dictionary count to the current dictionary counts */
	curr_count += new_count; 

	temp = htonl(curr_count);
	memcpy(kv->databuf, &temp, sizeof(uint32_t));

	/* grow the current buffer so we can append the new buffer */
	temp = kv2->datalen - sizeof(uint32_t);

	kvbuf_grow(kv, temp);

	memcpy(kv->databuf + kv->datalen, kv2->databuf + sizeof(uint32_t), temp);
	kv->datalen += temp;
}

/* returns number of dictionaries */
uint32_t
kvbuf_reset(kvbuf_t *kv)
{
	uint32_t x;

	if (kv == NULL) return 0;
	if (kv->databuf == NULL) return 0;

	kv->_dict = 0;
	kv->_key = 0;
	kv->_vlist = 0;
	kv->_val = 0;

	if (kv->datalen < sizeof(uint32_t)) return 0;

	x = 0;
	memcpy(&x, kv->databuf, sizeof(uint32_t));
	return ntohl(x);
}

/* advance to next dictionary, returns key count */
uint32_t
kvbuf_next_dict(kvbuf_t *kv)
{
	uint32_t x, k, v, kcount, vcount, kl, vl;
	char *p;

	if (kv == NULL) return 0;
	if (kv->databuf == NULL) return 0;

	kv->_key = 0;
	kv->_vlist = 0;
	kv->_val = 0;

	if (kv->_dict == 0)
	{
		/* first dict */
		if (kv->datalen < sizeof(uint32_t)) return 0;
		kv->_dict = sizeof(uint32_t);

		if (kv->datalen < (kv->_dict + sizeof(uint32_t))) return 0;

		p = kv->databuf + kv->_dict;
		x = 0;
		memcpy(&x, p, sizeof(uint32_t));
		kcount = ntohl(x);

		return kcount;
	}

	p = kv->databuf + kv->_dict;

	x = 0;
	memcpy(&x, p, sizeof(uint32_t));
	p += sizeof(uint32_t);
	kv->_dict += sizeof(uint32_t);
	kcount = ntohl(x);

	for (k = 0; k < kcount; k++)
	{
		x = 0;
		memcpy(&x, p, sizeof(uint32_t));
		p += sizeof(uint32_t);
		kv->_dict += sizeof(uint32_t);
		kl = ntohl(x);
		p += kl;
		kv->_dict += kl;

		x = 0;
		memcpy(&x, p, sizeof(uint32_t));
		p += sizeof(uint32_t);
		kv->_dict += sizeof(uint32_t);
		vcount = ntohl(x);

		for (v = 0; v < vcount; v++)
		{
			x = 0;
			memcpy(&x, p, sizeof(uint32_t));
			p += sizeof(uint32_t);
			kv->_dict += sizeof(uint32_t);
			vl = ntohl(x);
			p += vl;
			kv->_dict += vl;
		}
	}

	if (kv->datalen < (kv->_dict + sizeof(uint32_t))) return 0;

	p = kv->databuf + kv->_dict;
	x = 0;
	memcpy(&x, p, sizeof(uint32_t));
	kcount = ntohl(x);

	return kcount;
}

/* advance to next key, returns key and sets val_count */
char *
kvbuf_next_key(kvbuf_t *kv, uint32_t *val_count)
{
	uint32_t x, kl, v, vl, vc;
	char *p, *out;

	if (kv == NULL) return NULL;
	if (val_count == NULL) return NULL;

	*val_count = 0;

	if (kv->databuf == NULL) return NULL;
	if (kv->_dict == 0) return NULL;

	kv->_vlist = 0;
	kv->_val = 0;

	if (kv->_key == 0)
	{
		/* first key */
		if (kv->datalen < (kv->_dict +  sizeof(uint32_t))) return NULL;
		kv->_key = kv->_dict + sizeof(uint32_t);
	}
	else
	{
		p = kv->databuf + kv->_key;

		x = 0;
		memcpy(&x, p, sizeof(uint32_t));
		kl = ntohl(x);

		if (kv->datalen < (kv->_key + sizeof(uint32_t) + kl)) return NULL;

		p += (sizeof(uint32_t) + kl);
		kv->_key += (sizeof(uint32_t) + kl);

		/* skip over values */
		if (kv->datalen < (kv->_key + sizeof(uint32_t))) return NULL;

		x = 0;
		memcpy(&x, p, sizeof(uint32_t));
		vc = ntohl(x);

		p += sizeof(uint32_t);
		kv->_key += sizeof(uint32_t);

		for (v = 0; v < vc; v++)
		{
			if (kv->datalen < (kv->_key + sizeof(uint32_t))) return NULL;

			x = 0;
			memcpy(&x, p, sizeof(uint32_t));
			vl = ntohl(x);

			if (kv->datalen < (kv->_key + kl)) return NULL;

			p += (sizeof(uint32_t) + vl);
			kv->_key += (sizeof(uint32_t) + vl);
		}
	}

	if (kv->datalen < (kv->_key + sizeof(uint32_t))) return NULL;

	p = kv->databuf + kv->_key;
	x = 0;
	memcpy(&x, p, sizeof(uint32_t));
	kl = ntohl(x);

	p += sizeof(uint32_t);
	out = p;

	kv->_vlist = kv->_key + sizeof(uint32_t) + kl;
	if (kv->datalen < (kv->_vlist + sizeof(uint32_t)))
	{
		kv->_vlist = 0;
		return NULL;
	}

	p = kv->databuf + kv->_vlist;
	x = 0;
	memcpy(&x, p, sizeof(uint32_t));
	*val_count = ntohl(x);

	return out;
}

char *
kvbuf_next_val(kvbuf_t *kv)
{
	return kvbuf_next_val_len(kv, NULL);
}

char *
kvbuf_next_val_len(kvbuf_t *kv, uint32_t *len)
{
	uint32_t x = 0;
	uint32_t vltemp = 0;
	char *p;

	if (kv == NULL) return NULL;
	if (kv->databuf == NULL) return NULL;
	if (kv->_vlist == 0) return NULL;

	if (kv->_val == 0)
	{
		/* first val */
		if (kv->datalen < (kv->_vlist +  sizeof(uint32_t))) return NULL;
		kv->_val = kv->_vlist + sizeof(uint32_t);

		p = kv->databuf + kv->_val;

		memcpy(&x, p, sizeof(uint32_t));
		vltemp = ntohl(x);
	}
	else
	{
		p = kv->databuf + kv->_val;

		memcpy(&x, p, sizeof(uint32_t));
		vltemp = ntohl(x);

		if (kv->datalen < (kv->_val + sizeof(uint32_t) + vltemp)) return NULL;

		p += (sizeof(uint32_t) + vltemp);
		kv->_val += (sizeof(uint32_t) + vltemp);
	}

	if (kv->datalen < (kv->_val + sizeof(uint32_t))) return NULL;

	if (len != NULL) (*len) = vltemp;
	p = kv->databuf + kv->_val + sizeof(uint32_t);
	return p;
}

/*
 * Builds a kvarray_t / kvdict_t structure on top of a kvbuf_t. 
 * It allocates the appropriate number of kvdict_t structures
 * for the array, sets all the counters, and fills in pointers
 * for keys and valuse.  The pointers are NOT to newly allocated
 * strings: they just point into the kvbuf data buffer.
 *
 * To dispose of the kvarray_t and all of the associated
 * memory AND to free the original kvbuf, clients only
 * need to call kvarray_free().
 */
kvarray_t *
kvbuf_decode(kvbuf_t *kv)
{
	kvarray_t *a;
	uint32_t x, d, k, v;
	char *p;

	if (kv == NULL) return NULL;
	if (kv->databuf == NULL) return NULL;

	if (kv->datalen < sizeof(uint32_t)) return NULL;

	p = kv->databuf;
	kv->_size = kv->datalen;

	/* array count */
	x = 0;
	memcpy(&x, p, sizeof(uint32_t));
	p += sizeof(uint32_t);
	kv->_size -= sizeof(uint32_t);
	x = ntohl(x);

	if (x == 0) return NULL;

	a = (kvarray_t *)calloc(1, sizeof(kvarray_t));
	if (a == NULL) return NULL;

	a->count = x;
	a->dict = (kvdict_t *)calloc(a->count, sizeof(kvdict_t));
	if (a->dict == NULL)
	{
		free(a);
		return NULL;
	}

	for (d = 0; d < a->count; d++)
	{
		if (kv->_size < sizeof(uint32_t))
		{
			kvarray_free(a);
			return NULL;
		}

		/* key count */
		x = 0;
		memcpy(&x, p, sizeof(uint32_t));
		p += sizeof(uint32_t);
		kv->_size -= sizeof(uint32_t);
		a->dict[d].kcount = ntohl(x);

		if (a->dict[d].kcount > 0)
		{
			a->dict[d].key = (const char **)calloc(a->dict[d].kcount, sizeof(const char *));
			if (a->dict[d].key == NULL)
			{
				kvarray_free(a);
				return NULL;
			}

			a->dict[d].vcount = (uint32_t *)calloc(a->dict[d].kcount, sizeof(uint32_t));
			if (a->dict[d].vcount == NULL)
			{
				kvarray_free(a);
				return NULL;
			}

			a->dict[d].val = (const char ***)calloc(a->dict[d].kcount, sizeof(char **));
			if (a->dict[d].val == NULL)
			{
				kvarray_free(a);
				return NULL;
			}
		}

		for (k = 0; k < a->dict[d].kcount; k++)
		{
			/* get key */
			if (kv->_size < sizeof(uint32_t))
			{
				kvarray_free(a);
				return NULL;
			}

			/* key length */
			x = 0;
			memcpy(&x, p, sizeof(uint32_t));
			p += sizeof(uint32_t);
			kv->_size -= sizeof(uint32_t);
			x = ntohl(x);

			if (kv->_size < x)
			{
				kvarray_free(a);
				return NULL;
			}

			/* key data */
			a->dict[d].key[k] = p;

			p += x;
			kv->_size -= x;

			if (kv->_size < sizeof(uint32_t))
			{
				kvarray_free(a);
				return NULL;
			}

			/* val count */
			x = 0;
			memcpy(&x, p, sizeof(uint32_t));
			p += sizeof(uint32_t);
			kv->_size -= sizeof(uint32_t);
			a->dict[d].vcount[k] = ntohl(x);

			if (a->dict[d].vcount[k] > 0)
			{
				/* N.B. we add a NULL pointer at the end of the list */
				a->dict[d].val[k] = (const char **)calloc(a->dict[d].vcount[k] + 1, sizeof(const char *));
				if (a->dict[d].val[k] == NULL)
				{
					kvarray_free(a);
					return NULL;
				}
			}

			for (v = 0; v < a->dict[d].vcount[k]; v++)
			{
				/* get val */
				if (kv->_size < sizeof(uint32_t))
				{
					kvarray_free(a);
					return NULL;
				}

				/* val length */
				x = 0;
				memcpy(&x, p, sizeof(uint32_t));
				p += sizeof(uint32_t);
				kv->_size -= sizeof(uint32_t);
				x = ntohl(x);

				if (kv->_size < x)
				{
					kvarray_free(a);
					return NULL;
				}

				/* val data */
				a->dict[d].val[k][v] = p;

				p += x;
				kv->_size -= x;
			}
		}
	}

	a->kv = kv;
	return a;
}

void
kvarray_free(kvarray_t *a)
{
	uint32_t d, k;

	if (a == NULL) return;

	for (d = 0; d < a->count; d++)
	{
		for (k = 0; k < a->dict[d].kcount; k++)
		{
			if (a->dict[d].val == NULL) continue;
			if (a->dict[d].val[k] != NULL) free(a->dict[d].val[k]);
		}

		if (a->dict[d].key != NULL) free(a->dict[d].key);
		if (a->dict[d].vcount != NULL) free(a->dict[d].vcount);
		if (a->dict[d].val != NULL) free(a->dict[d].val);
	}

	a->count = 0;

	if (a->dict != NULL) free(a->dict);
	a->dict = NULL;

	if (a->kv != NULL) kvbuf_free(a->kv);
	a->kv = NULL;

	free(a);
}

kern_return_t
LI_DSLookupGetProcedureNumber(const char *name, int32_t *procno)
{
	kern_return_t status;
	security_token_t token;
	uint32_t n, len;

	if (name == NULL) return KERN_FAILURE;

	len = strlen(name) + 1;
	if (len == 1) return KERN_FAILURE;

	token.val[0] = -1;
	token.val[1] = -1;

	if (_ds_running() == 0) return KERN_FAILURE;
	if (_ds_port == MACH_PORT_NULL) return KERN_FAILURE;

	status = MIG_SERVER_DIED;
	for (n = 0; (_ds_port != MACH_PORT_NULL) && (status == MIG_SERVER_DIED) && (n < MAX_LOOKUP_ATTEMPTS); n++)
	{
		status = libinfoDSmig_GetProcedureNumber(_ds_port, (char *)name, procno, &token);

		if (status == MACH_SEND_INVALID_DEST)
		{
			mach_port_mod_refs(mach_task_self(), _ds_port, MACH_PORT_RIGHT_SEND, -1);
			status = bootstrap_look_up(bootstrap_port, kDSStdMachDSLookupPortName, &_ds_port);
			if ((status != BOOTSTRAP_SUCCESS) && (status != BOOTSTRAP_UNKNOWN_SERVICE)) _ds_port = MACH_PORT_NULL;
			status = MIG_SERVER_DIED;
		}
	}

	if (status != KERN_SUCCESS)
	{
#ifdef DEBUG
		asl_log(NULL, NULL, ASL_LEVEL_DEBUG, "_DSLookupGetProcedureNumber %s status %u", name, status);
#endif
		return status;
	}

	if (token.val[0] != 0)
	{
#ifdef DEBUG
		asl_log(NULL, NULL, ASL_LEVEL_DEBUG, "_DSLookupGetProcedureNumber %s auth failure uid=%d", name, token.val[0]);
#endif
		return KERN_FAILURE;
	}

#ifdef DEBUG
	asl_log(NULL, NULL, ASL_LEVEL_DEBUG, "_DSLookupGetProcedureNumber %s = %d", name, *procno);
#endif
	return status;
}

__private_extern__ kern_return_t
LI_DSLookupQuery(int32_t procno, kvbuf_t *request, kvarray_t **reply)
{
	kern_return_t status;
	security_token_t token;
	uint32_t n;
	mach_msg_type_number_t illen, oolen;
	char ilbuf[MAX_MIG_INLINE_DATA];
	vm_offset_t oobuf;
	kvbuf_t *out;

	if (reply == NULL) return KERN_FAILURE;
	if ((request != NULL) && ((request->databuf == NULL) || (request->datalen == 0))) return KERN_FAILURE;

	token.val[0] = -1;
	token.val[1] = -1;
	*reply = NULL;

	if (_ds_running() == 0) return KERN_FAILURE;
	if (_ds_port == MACH_PORT_NULL) return KERN_FAILURE;

	status = MIG_SERVER_DIED;
	for (n = 0; (_ds_port != MACH_PORT_NULL) && (status == MIG_SERVER_DIED) && (n < MAX_LOOKUP_ATTEMPTS); n++)
	{
		illen = 0;
		oolen = 0;
		oobuf = 0;

		if (request != NULL)
		{
			status = libinfoDSmig_Query(_ds_port, procno, request->databuf, request->datalen, ilbuf, &illen, &oobuf, &oolen, &token);
		}
		else
		{
			status = libinfoDSmig_Query(_ds_port, procno, "", 0, ilbuf, &illen, &oobuf, &oolen, &token);
		}

		if (status == MACH_SEND_INVALID_DEST)
		{
			mach_port_mod_refs(mach_task_self(), _ds_port, MACH_PORT_RIGHT_SEND, -1);
			status = bootstrap_look_up(bootstrap_port, kDSStdMachDSLookupPortName, &_ds_port);
			if ((status != BOOTSTRAP_SUCCESS) && (status != BOOTSTRAP_UNKNOWN_SERVICE)) _ds_port = MACH_PORT_NULL;
			status = MIG_SERVER_DIED;
		}
	}

	if (status != KERN_SUCCESS)
	{
#ifdef DEBUG
		asl_log(NULL, NULL, ASL_LEVEL_DEBUG, "_DSLookupQuery %d status %u", procno, status);
#endif
		return status;
	}

	if (token.val[0] != 0)
	{
#ifdef DEBUG
		asl_log(NULL, NULL, ASL_LEVEL_DEBUG, "_DSLookupQuery %d auth failure uid=%d", procno, token.val[0]);
#endif
		if (oolen > 0) vm_deallocate(mach_task_self(), (vm_address_t)oobuf, oolen);
		return KERN_FAILURE;
	}

	out = (kvbuf_t *)calloc(1, sizeof(kvbuf_t));
	if (out == NULL)
	{
		if (oolen > 0) vm_deallocate(mach_task_self(), (vm_address_t)oobuf, oolen);
		return KERN_FAILURE;
	}

	if (oolen > 0)
	{
		out->datalen = oolen;
		out->databuf = malloc(oolen);
		if (out->databuf == NULL)
		{
			free(out);
			*reply = NULL;
			vm_deallocate(mach_task_self(), (vm_address_t)oobuf, oolen);
			return KERN_FAILURE;
		}

		memcpy(out->databuf, (char *)oobuf, oolen);
		vm_deallocate(mach_task_self(), (vm_address_t)oobuf, oolen);
	}
	else if (illen > 0)
	{
		out->datalen = illen;
		out->databuf = malloc(illen);
		if (out->databuf == NULL)
		{
			free(out);
			*reply = NULL;
			return KERN_FAILURE;
		}

		memcpy(out->databuf, ilbuf, illen);
	}

	*reply = kvbuf_decode(out);
	if (*reply == NULL)
	{
		/* DS returned no data */
		free(out->databuf);
		free(out);
	}

#ifdef DEBUG
	asl_log(NULL, NULL, ASL_LEVEL_DEBUG, "_DSLookupQuery %d status OK", procno);
#endif
	return status;
}

/*
 * Get an entry from a kvarray.
 * Calls the system information daemon if the list doesn't exist (first call),
 * or extracts the next entry if the list has been fetched.
 */
__private_extern__ void *
LI_getent(const char *procname, int *procnum, void *(*extract)(kvarray_t *), int tkey, size_t esize)
{
	void *entry;
	struct li_thread_info *tdata;
	kvarray_t *reply;
	kern_return_t status;

	tdata = LI_data_create_key(tkey, esize);
	if (tdata == NULL) return NULL;

	if (tdata->li_vm == NULL)
	{
		if (*procnum < 0)
		{
			status = LI_DSLookupGetProcedureNumber(procname, procnum);
			if (status != KERN_SUCCESS)
			{
				LI_data_free_kvarray(tdata);
				tdata->li_vm = NULL;
				return NULL;
			}
		}

		reply = NULL;
		status = LI_DSLookupQuery(*procnum, NULL, &reply);

		if (status != KERN_SUCCESS)
		{
			LI_data_free_kvarray(tdata);
			tdata->li_vm = NULL;
			return NULL;
		}

		tdata->li_vm = (char *)reply;
	}

	entry = extract((kvarray_t *)(tdata->li_vm));
	if (entry == NULL)
	{
		LI_data_free_kvarray(tdata);
		tdata->li_vm = NULL;
		return NULL;
	}

	return entry;
}

__private_extern__ void *
LI_getone(const char *procname, int *procnum, void *(*extract)(kvarray_t *), const char *key, const char *val)
{
	void *entry;
	kvbuf_t *request;
	kvarray_t *reply;
	kern_return_t status;

	if (*procnum < 0)
	{
		status = LI_DSLookupGetProcedureNumber(procname, procnum);
		if (status != KERN_SUCCESS) return NULL;
	}

	request = kvbuf_query_key_val(key, val);
	if (request == NULL) return NULL;

	reply = NULL;
	status = LI_DSLookupQuery(*procnum, request, &reply);
	kvbuf_free(request);

	if (status != KERN_SUCCESS) return NULL;

	entry = extract(reply);
	kvarray_free(reply);

	return entry;
}

__private_extern__
int LI_L1_cache_check(int tkey)
{
	int check, x;
	const char *notify_key;

	/* check if L1 cache is disabled */
	if (gL1CacheEnabled == 0) return LI_L1_CACHE_DISABLED;

	/* Initialize on first call */
	if (_L1_notify_token[0] == -1)
	{
		pthread_mutex_lock(&_notify_lock);
		if (_L1_notify_token[0] == -1) notify_register_check(L1_CACHE_NOTIFICATION_KEY_GLOBAL, &(_L1_notify_token[0]));
		pthread_mutex_unlock(&_notify_lock);
	}

	if (_L1_notify_token[0] == -1) return LI_L1_CACHE_FAILED;

	check = 1;
	if (notify_check(_L1_notify_token[0], &check) != 0) return LI_L1_CACHE_FAILED;
	if (check == 1) return LI_L1_CACHE_STALE;

	x = 0;
	notify_key = NULL;

	switch (tkey)
	{
		case _li_data_key_group:
		{
			x = 1;
			notify_key = L1_CACHE_NOTIFICATION_KEY_GROUP;
			break;
		}
		case _li_data_key_host:
		{
			x = 2;
			notify_key = L1_CACHE_NOTIFICATION_KEY_HOST;
			break;
		}
		case _li_data_key_service:
		{
			x = 3;
			notify_key = L1_CACHE_NOTIFICATION_KEY_SERVICE;
			break;
		}
		case _li_data_key_user:
		{
			x = 4;
			notify_key = L1_CACHE_NOTIFICATION_KEY_USER;
			break;
		}
		default: break;
	}

	if ((x != 0) && (notify_key != NULL))
	{
		/* Initialize on first call */
		if (_L1_notify_token[x] == -1)
		{
			pthread_mutex_lock(&_notify_lock);
			if (_L1_notify_token[x] == -1) notify_register_check(notify_key, &(_L1_notify_token[x]));
			pthread_mutex_unlock(&_notify_lock);
		}

		if (_L1_notify_token[x] == -1) return LI_L1_CACHE_FAILED;

		check = 1;
		if (notify_check(_L1_notify_token[x], &check) != 0) return LI_L1_CACHE_FAILED;
		if (check == 1) return LI_L1_CACHE_STALE;
	}

	return LI_L1_CACHE_OK;
}

static uint32_t
padsize(size_t curr, size_t item, const uint32_t *align)
{
	uint32_t na, diff;

	if (item > 8) item = 8;

	na = align[item];
	if (na == 0) return 0;

	diff = curr % na;
	if (diff == 0) return 0;

	return na - diff;
}


/*
 * Create a structure using in-line memory (i.e. all one blob).
 * This reduces malloc/free workload.
 *
 * Structutre components may be strings, 1, 2, 4, or 8-byte values,  
 * lists of strings, or lists of 4, 8, or 16-byte values.
 *
 * Format keys:
 *	s	NUL terminated string
 *	1	1 byte value
 *	2	2 byte value
 *	4	4 byte value
 *	8	8 byte value
 *	L	long (32 or 64 bits, depending on architecture)
 *	*	NULL-terminated list of strings
 *	a	NULL-terminated list of 4-byte values
 *	b	NULL-terminated list of 8-byte values
 *	c	NULL-terminated list of 16-byte values
 *
 */
__private_extern__ void *
LI_ils_create(char *fmt, ...)
{
	va_list ap;
	char *arg, *f;
	char **list;
	void *hp, *dp, *lp, *ils;
	uint8_t u8;
	uint16_t u16;
	uint32_t u32, i, pad;
	uint64_t u64;
	unsigned long l;
	size_t memsize, hsize, csize, slen, largest;
	const uint32_t *align;

	if (fmt == NULL) return NULL;

	largest = 0;
	align = align_32;
	if (sizeof(char *) == 8) align = align_64;

	/* first pass: calculate size */
	memsize = ILS_MAGIC_SIZE;
	hsize = 0;

	va_start(ap, fmt);

	for (f = fmt; (*f) != '\0'; f++)
	{
		csize = 0;
		slen = 0;

		if (*f == 's')
		{
			if (largest < sizeof(char *)) largest = sizeof(char *);

			csize = sizeof(char *) + padsize(hsize, sizeof(char *), align);
			arg = va_arg(ap, char *);
			if (arg != NULL) slen = strlen(arg) + 1;
		}
		else if (*f == '1')
		{
			if (largest < 1) largest = 1;

			csize = 1;
			u8 = va_arg(ap, int);
		}
		else if (*f == '2')
		{
			if (largest < 2) largest = 2;

			csize = 2 + padsize(hsize, 2, align);
			u16 = va_arg(ap, int);
		}
		else if (*f == '4')
		{
			if (largest < 4) largest = 4;

			csize = 4 + padsize(hsize, 4, align);
			u32 = va_arg(ap, uint32_t);
		}
		else if (*f == '8')
		{
			if (largest < 8) largest = 8;

			csize = 8 + padsize(hsize, 8, align);
			u64 = va_arg(ap, uint64_t);
		}
		else if (*f == 'L')
		{
			if (largest < sizeof(unsigned long)) largest = sizeof(unsigned long);

			csize = sizeof(unsigned long) + padsize(hsize, sizeof(unsigned long), align);
			l = va_arg(ap, unsigned long);
		}
		else if (*f == '*')
		{
			/* NULL-terminated list of strings */
			if (largest < sizeof(char *)) largest = sizeof(char *);

			csize = sizeof(char *) + padsize(hsize, sizeof(char *), align);
			list = va_arg(ap, char **);
			if (list != NULL)
			{
				for (i = 0; list[i] != NULL; i++)
				{
					slen += sizeof(char *);
					slen += (strlen(list[i]) + 1);
				}

				slen += sizeof(char *);
			}
		}
		else if (*f == 'a')
		{
			/* NULL-terminated list of 4-byte values */
			if (largest < sizeof(char *)) largest = sizeof(char *);

			csize = sizeof(char *) + padsize(hsize, sizeof(char *), align);
			list = va_arg(ap, char **);
			if (list != NULL)
			{
				for (i = 0; list[i] != NULL; i++)
				{
					slen += sizeof(char *);
					slen += 4;
				}

				slen += sizeof(char *);
			}
		}
		else if (*f == 'b')
		{
			/* NULL-terminated list of 8-byte values */
			if (largest < sizeof(char *)) largest = sizeof(char *);

			csize = sizeof(char *) + padsize(hsize, sizeof(char *), align);
			list = va_arg(ap, char **);
			if (list != NULL)
			{
				for (i = 0; list[i] != NULL; i++)
				{
					slen += sizeof(char *);
					slen += 8;
				}

				slen += sizeof(char *);
			}
		}
		else if (*f == 'c')
		{
			/* NULL-terminated list of 16-byte values */
			if (largest < sizeof(char *)) largest = sizeof(char *);

			csize = sizeof(char *) + padsize(hsize, sizeof(char *), align);
			list = va_arg(ap, char **);
			if (list != NULL)
			{
				for (i = 0; list[i] != NULL; i++)
				{
					slen += sizeof(char *);
					slen += 16;
				}

				slen += sizeof(char *);
			}
		}
		else return NULL;

		memsize += csize;
		memsize += slen;
		hsize += csize;
	}

	va_end(ap);

	pad = padsize(hsize, largest, align);
	memsize += pad;
	hsize += pad;

	ils = malloc(memsize);
	if (ils == NULL)
	{
		errno = ENOMEM;
		return NULL;
	}

	/* insert magic cookie */
	dp = ils + hsize;
	memcpy(dp, ILS_MAGIC, ILS_MAGIC_SIZE);
	dp += ILS_MAGIC_SIZE;

	hp = ils;
	hsize = 0;

	/* second pass: copy data */
	va_start(ap, fmt);
	for (f = fmt; (*f) != '\0'; f++)
	{
		if (*f == 's')
		{
			pad = padsize(hsize, sizeof(char *), align);
			if (pad != 0)
			{
				memset(hp, 0, pad);
				hp += pad;
				hsize += pad;
			}

			arg = va_arg(ap, char *);
			if (arg == NULL)
			{
				memset(hp, 0, sizeof(char *));
			}
			else
			{
				memcpy(hp, &dp, sizeof(char *));
				slen = strlen(arg) + 1;
				memcpy(dp, arg, slen);
				dp += slen;
			}

			hp += sizeof(char *);
			hsize += sizeof(char *);
		}
		else if (*f == '1')
		{
			u8 = va_arg(ap, int);
			memcpy(hp, &u8, sizeof(uint8_t));
			hp += sizeof(uint8_t);
		}
		else if (*f == '2')
		{
			pad = padsize(hsize, 2, align);
			if (pad != 0)
			{
				memset(hp, 0, pad);
				hp += pad;
				hsize += pad;
			}

			u16 = va_arg(ap, int);
			memcpy(hp, &u16, sizeof(uint16_t));

			hp += sizeof(uint16_t);
			hsize += sizeof(uint16_t);
		}
		else if (*f == '4')
		{
			pad = padsize(hsize, 4, align);
			if (pad != 0)
			{
				memset(hp, 0, pad);
				hp += pad;
				hsize += pad;
			}

			u32 = va_arg(ap, uint32_t);
			memcpy(hp, &u32, sizeof(uint32_t));

			hp += sizeof(uint32_t);
			hsize += sizeof(uint32_t);
		}
		else if (*f == '8')
		{
			pad = padsize(hsize, 8, align);
			if (pad != 0)
			{
				memset(hp, 0, pad);
				hp += pad;
				hsize += pad;
			}

			u64 = va_arg(ap, uint64_t);
			memcpy(hp, &u64, sizeof(uint64_t));

			hp += sizeof(uint64_t);
			hsize += sizeof(uint64_t);
		}
		else if (*f == 'L')
		{
			pad = padsize(hsize, sizeof(unsigned long), align);
			if (pad != 0)
			{
				memset(hp, 0, pad);
				hp += pad;
				hsize += pad;
			}

			l = va_arg(ap, unsigned long);
			memcpy(hp, &l, sizeof(unsigned long));

			hp += sizeof(unsigned long);
			hsize += sizeof(unsigned long);
		}
		else if (*f == '*')
		{
			pad = padsize(hsize, sizeof(char *), align);
			if (pad != 0)
			{
				memset(hp, 0, pad);
				hp += pad;
				hsize += pad;
			}

			list = va_arg(ap, char **);

			if (list == NULL)
			{
				memset(hp, 0, sizeof(char *));
			}
			else
			{
				memcpy(hp, &dp, sizeof(char *));

				for (i = 0; list[i] != NULL; i++);

				lp = dp;
				dp += ((i + 1) * sizeof(char *));

				for (i = 0; list[i] != NULL; i++)
				{
					memcpy(lp, &dp, sizeof(char *));
					lp += sizeof(char *);
					slen = strlen(list[i]) + 1;
					memcpy(dp, list[i], slen);
					dp += slen;
				}

				memset(lp, 0, sizeof(char *));
			}

			hp += sizeof(char *);
			hsize += sizeof(char *);
		}
		else if (*f == 'a')
		{
			pad = padsize(hsize, sizeof(char *), align);
			if (pad != 0)
			{
				memset(hp, 0, pad);
				hp += pad;
				hsize += pad;
			}

			list = va_arg(ap, char **);

			if (list == NULL)
			{
				memset(hp, 0, sizeof(char *));
			}
			else
			{
				memcpy(hp, &dp, sizeof(char *));

				for (i = 0; list[i] != NULL; i++);

				lp = dp;
				dp += ((i + 1) * sizeof(char *));

				for (i = 0; list[i] != NULL; i++)
				{
					memcpy(lp, &dp, sizeof(char *));
					lp += sizeof(char *);
					slen = 4;
					memcpy(dp, list[i], slen);
					dp += slen;
				}

				memset(lp, 0, sizeof(char *));
			}

			hp += sizeof(char *);
			hsize += sizeof(char *);
		}
		else if (*f == 'b')
		{
			pad = padsize(hsize, sizeof(char *), align);
			if (pad != 0)
			{
				memset(hp, 0, pad);
				hp += pad;
				hsize += pad;
			}

			list = va_arg(ap, char **);

			if (list == NULL)
			{
				memset(hp, 0, sizeof(char *));
			}
			else
			{
				memcpy(hp, &dp, sizeof(char *));

				for (i = 0; list[i] != NULL; i++);

				lp = dp;
				dp += ((i + 1) * sizeof(char *));

				for (i = 0; list[i] != NULL; i++)
				{
					memcpy(lp, &dp, sizeof(char *));
					lp += sizeof(char *);
					slen = 8;
					memcpy(dp, list[i], slen);
					dp += slen;
				}

				memset(lp, 0, sizeof(char *));
			}

			hp += sizeof(char *);
			hsize += sizeof(char *);
		}
		else if (*f == 'c')
		{
			pad = padsize(hsize, sizeof(char *), align);
			if (pad != 0)
			{
				memset(hp, 0, pad);
				hp += pad;
				hsize += pad;
			}

			list = va_arg(ap, char **);

			if (list == NULL)
			{
				memset(hp, 0, sizeof(char *));
			}
			else
			{
				memcpy(hp, &dp, sizeof(char *));

				for (i = 0; list[i] != NULL; i++);

				lp = dp;
				dp += ((i + 1) * sizeof(char *));

				for (i = 0; list[i] != NULL; i++)
				{
					memcpy(lp, &dp, sizeof(char *));
					lp += sizeof(char *);
					slen = 16;
					memcpy(dp, list[i], slen);
					dp += slen;
				}

				memset(lp, 0, sizeof(char *));
			}

			hp += sizeof(char *);
			hsize += sizeof(char *);
		}
	}

	va_end(ap);

	pad = padsize(hsize, largest, align);
	if (pad > 0) memset(hp, 0, pad);

	return ils;
}

__private_extern__ int
LI_ils_free(void *ils, size_t len)
{
	char *p;

	if (ils == NULL) return 0;

	p = ils + len;
	if (memcmp(p, ILS_MAGIC, ILS_MAGIC_SIZE) != 0) return -1;

	free(ils);

	return 0;
}

kern_return_t 
_lookup_link(mach_port_t server, char *name, int *procno)
{
	syslog(LOG_ERR, "RED ALERT!   lookupd call %s from pid %u", name, getpid());
	return KERN_FAILURE;
}

kern_return_t 
_lookup_one(mach_port_t server, int proc, char *indata, mach_msg_type_number_t indataCnt, char *outdata, mach_msg_type_number_t *outdataCnt)
{
	return KERN_FAILURE;
}

kern_return_t 
_lookup_all(mach_port_t server, int proc, char *indata, mach_msg_type_number_t indataCnt, char **outdata, mach_msg_type_number_t *outdataCnt)
{
	return KERN_FAILURE;
}

kern_return_t 
_lookup_ooall(mach_port_t server, int proc, char *indata, mach_msg_type_number_t indataCnt, char **outdata, mach_msg_type_number_t *outdataCnt)
{
	return KERN_FAILURE;
}
