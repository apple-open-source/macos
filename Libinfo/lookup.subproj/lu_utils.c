/*
 * Copyright (c) 1999-2002 Apple Computer, Inc. All rights reserved.
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

#include <stdlib.h>
#include <string.h>
#include <mach/mach.h>
#include <pthread.h>
#ifdef DEBUG
#include <syslog.h>
#endif
#include "_lu_types.h"
#include "lookup.h"
#include "lu_utils.h"
#include "netdb_async.h"

#define MAX_LOOKUP_ATTEMPTS 10
#define _LU_MAXLUSTRLEN 256
#define QBUF_SIZE 4096

#define LU_MESSAGE_SEND_ID 4241776
#define LU_MESSAGE_REPLY_ID 4241876

static pthread_key_t _info_key = NULL;
static pthread_once_t _info_key_initialized = PTHREAD_ONCE_INIT;

struct _lu_data_s
{
	unsigned int icount;
	unsigned int *ikey;
	void **idata;
	void (**idata_destructor)(void *);
};

typedef struct _lu_async_request_s
{
	mach_port_t reply_port;
	uint32_t retry;
	uint32_t proc;
	void *context;
	void *callback;
	ooline_data request_buffer;
	mach_msg_type_number_t request_buffer_len;
	struct _lu_async_request_s *next;
} _lu_async_request_t;

typedef struct
{
	mach_msg_header_t head;
	NDR_record_t NDR;
	int proc;
	mach_msg_type_number_t query_data_len;
	unit query_data[QBUF_SIZE];
} _lu_query_msg_t;

typedef struct
{
	mach_msg_header_t head;
	mach_msg_body_t msgh_body;
	mach_msg_ool_descriptor_t reply_data;
	NDR_record_t NDR;
	mach_msg_type_number_t reply_data_len;
	mach_msg_format_0_trailer_t trailer;
} _lu_reply_msg_t;

static pthread_mutex_t _lu_worklist_lock = PTHREAD_MUTEX_INITIALIZER;
static _lu_async_request_t *_lu_worklist = NULL;

/* Send an asynchronous query message to lookupd */
static kern_return_t
_lu_async_send(_lu_async_request_t *r)
{
	_lu_query_msg_t in;
	register _lu_query_msg_t *inp = &in;
	mach_msg_return_t status;
	unsigned int msgh_size;

	if (r == NULL) return KERN_FAILURE;

	if (r->retry == 0) return MIG_SERVER_DIED;
	r->retry--;

	if (r->request_buffer_len > QBUF_SIZE) return MIG_ARRAY_TOO_LARGE;

	msgh_size = (sizeof(_lu_query_msg_t) - 16384) + ((4 * r->request_buffer_len));
	inp->head.msgh_bits = MACH_MSGH_BITS(19, MACH_MSG_TYPE_MAKE_SEND_ONCE);
	inp->head.msgh_remote_port = _lu_port;
	inp->head.msgh_local_port = r->reply_port;
	inp->head.msgh_id = LU_MESSAGE_SEND_ID;
	inp->NDR = NDR_record;
	inp->proc = r->proc;
	inp->query_data_len = r->request_buffer_len;
	memcpy(inp->query_data, r->request_buffer, 4 * r->request_buffer_len);

	status = mach_msg(&inp->head, MACH_SEND_MSG, msgh_size, 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);	
	if (status == MACH_MSG_SUCCESS) return KERN_SUCCESS;
	
	if (status == MACH_SEND_INVALID_REPLY)
	{
		mach_port_mod_refs(mach_task_self(), r->reply_port, MACH_PORT_RIGHT_RECEIVE, -1);
		r->reply_port = MACH_PORT_NULL;
	}

	return status;
}

static _lu_async_request_t *
_lu_worklist_remove(mach_port_t p)
{
	_lu_async_request_t *r, *n;

	if (p == MACH_PORT_NULL) return NULL;
	if (_lu_worklist == NULL) return NULL;

	pthread_mutex_lock(&_lu_worklist_lock);

	if (_lu_worklist->reply_port == p)
	{
		r = _lu_worklist;
		_lu_worklist = r->next;
		pthread_mutex_unlock(&_lu_worklist_lock);
		return r;
	}

	for (r = _lu_worklist; r != NULL; r = r->next)
	{
		n = r->next;
		if (n == NULL) break;

		if (n->reply_port == p)
		{
			r->next = n->next;
			pthread_mutex_unlock(&_lu_worklist_lock);
			return n;
		}
	}

	pthread_mutex_unlock(&_lu_worklist_lock);
	return NULL;
}

static _lu_async_request_t *
_lu_worklist_find(mach_port_t p)
{
	_lu_async_request_t *r;

	if (p == MACH_PORT_NULL) return NULL;
	if (_lu_worklist == NULL) return NULL;

	pthread_mutex_lock(&_lu_worklist_lock);

	for (r = _lu_worklist; r != NULL; r = r->next)
	{
		if (r->reply_port == p)
		{
			pthread_mutex_unlock(&_lu_worklist_lock);
			return r;
		}
	}

	pthread_mutex_unlock(&_lu_worklist_lock);
	return NULL;
}

static void
_lu_free_request(_lu_async_request_t *r)
{
	if (r == NULL) return;

	if (r->request_buffer != NULL) free(r->request_buffer);
	r->request_buffer = NULL;

	if (r->reply_port != MACH_PORT_NULL) mach_port_destroy(mach_task_self(), r->reply_port);
	r->reply_port = MACH_PORT_NULL;

	free(r);
}

/* Receive an asynchronous reply message from lookupd */
kern_return_t
lu_async_receive(mach_port_t p, char **buf, uint32_t *len)
{
	_lu_reply_msg_t *r;
	kern_return_t status;
	uint32_t size;
	_lu_async_request_t *req;

	size = sizeof(_lu_reply_msg_t);

	r = (_lu_reply_msg_t *)calloc(1, size);
	if (r == NULL) return KERN_RESOURCE_SHORTAGE;

	r->head.msgh_local_port = p;
	r->head.msgh_size = size;
	status = mach_msg(&(r->head), MACH_RCV_MSG, 0, size, r->head.msgh_local_port, 0, MACH_PORT_NULL);
	if (status != KERN_SUCCESS)
	{
		free(r);
		return status;
	}

	req = _lu_worklist_remove(r->head.msgh_local_port);
	if (req == NULL)
	{
		free(r);
		return KERN_FAILURE;
	}

	*buf = r->reply_data.address;
	*len = r->reply_data.size;

	free(r);

	_lu_free_request(req);
	return KERN_SUCCESS;
}

static void
_lu_worklist_append(_lu_async_request_t *r)
{
	_lu_async_request_t *p;

	if (r == NULL) return;

	pthread_mutex_lock(&_lu_worklist_lock);

	if (_lu_worklist == NULL)
	{
		_lu_worklist = r;
		pthread_mutex_unlock(&_lu_worklist_lock);
		return;
	}

	for (p = _lu_worklist; p->next != NULL; p = p->next);
	p->next = r;

	pthread_mutex_unlock(&_lu_worklist_lock);
}

void
lu_async_call_cancel(mach_port_t p)
{
	_lu_async_request_t *req;

	req = _lu_worklist_remove(p);
	if (req != NULL) _lu_free_request(req);
	else if (p != MACH_PORT_NULL) mach_port_destroy(mach_task_self(), p);
}

static _lu_async_request_t *
_lu_create_request(uint32_t proc, const char *buf, uint32_t len, void *callback, void *context)
{
	_lu_async_request_t *r;
	kern_return_t status;

	if (_lu_port == NULL) return NULL;

	r = (_lu_async_request_t *)calloc(1, sizeof(_lu_async_request_t));
	if (r == NULL) return NULL;

	status = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &(r->reply_port));
	if (status != KERN_SUCCESS)
	{
		_lu_free_request(r);
		return NULL;
	}

	r->retry = MAX_LOOKUP_ATTEMPTS;

	r->context = context;
	r->callback = callback;
	r->proc = proc;

	r->request_buffer = malloc(len * BYTES_PER_XDR_UNIT);
	memcpy(r->request_buffer, buf, len * BYTES_PER_XDR_UNIT);
	r->request_buffer_len = len;

	r->next = NULL;

	return r;
}

kern_return_t
lu_async_start(mach_port_t *p, uint32_t proc, const char *buf, uint32_t len, void *callback, void *context)
{
	_lu_async_request_t *r;
	kern_return_t status;
	uint32_t retry;

	if (p == NULL) return KERN_FAILURE;

	*p = MACH_PORT_NULL;

	if (!_lu_running()) return KERN_FAILURE;

	/* Make a request struct to keep track */
	r = _lu_create_request(proc, buf, len, callback, context);
	if (r == NULL) return KERN_FAILURE;

	status = MIG_SERVER_DIED;
	for (retry = 0; (status == MIG_SERVER_DIED) && (retry < MAX_LOOKUP_ATTEMPTS); retry++)
	{
		/* send to lookupd */
		status = _lu_async_send(r);
	}

	if (status != KERN_SUCCESS)
	{
		_lu_free_request(r);
		return status;
	}

	/* Add request to worklist */
	_lu_worklist_append(r);

	*p = r->reply_port;
	return KERN_SUCCESS;
}

kern_return_t
lu_async_send(mach_port_t *p, uint32_t proc, const char *buf, uint32_t len)
{
	return lu_async_start(p, proc, buf, len, NULL, NULL);
}

int
lu_async_handle_reply(void *msg, char **buf, uint32_t *len, void **callback, void **context)
{
	_lu_reply_msg_t *r;
	_lu_async_request_t *req;
	kern_return_t status;
	uint32_t retry;

	if (msg == NULL) return -1;
	r = (_lu_reply_msg_t *)msg;

	/* If reply status was an error, resend */
	if (r->head.msgh_id != LU_MESSAGE_REPLY_ID)
	{
		if (r->head.msgh_id == MACH_NOTIFY_SEND_ONCE)
		{
			/* if MiG server (lookupd) died */
			req = _lu_worklist_find(r->head.msgh_local_port);
			if (req == NULL) return -1;

			status = MIG_SERVER_DIED;
			for (retry = 0; (status == MIG_SERVER_DIED) && (retry < MAX_LOOKUP_ATTEMPTS); retry++)
			{
				/* send to lookupd */
				status = _lu_async_send(req);
			}

			if (status != KERN_SUCCESS) return -1;
		}
		return MIG_REPLY_MISMATCH;
	}

	req = _lu_worklist_remove(r->head.msgh_local_port);
	if (req == NULL) return -1;

	*buf = r->reply_data.address;
	*len = r->reply_data.size;
	*callback = req->callback;
	*context = req->context;

	_lu_free_request(req);
	return 0;
}

ni_proplist *
_lookupd_xdr_dictionary(XDR *inxdr)
{
	int i, nkeys, j, nvals;
	char *key, *val;
	ni_proplist *l;

	if (!xdr_int(inxdr, &nkeys)) return NULL;

	l = (ni_proplist *)malloc(sizeof(ni_proplist));
	NI_INIT(l);

	l->ni_proplist_len = nkeys;
	l->ni_proplist_val = NULL;
	if (nkeys > 0)
	{
		i = nkeys * sizeof(ni_property);
		l->ni_proplist_val = (ni_property *)calloc(1, i);
	}

	for (i = 0; i < nkeys; i++)
	{
		key = NULL;
		if (!xdr_string(inxdr, &key, -1))
		{
			ni_proplist_free(l);
			return NULL;
		}

		l->ni_proplist_val[i].nip_name = key;
	
		if (!xdr_int(inxdr, &nvals))
		{
			ni_proplist_free(l);
			return NULL;
		}
	
		l->ni_proplist_val[i].nip_val.ni_namelist_len = nvals;
		if (nvals > 0)
		{
			j = nvals * sizeof(ni_name);
			l->ni_proplist_val[i].nip_val.ni_namelist_val = (ni_name *)calloc(1, j);
		}
		
		for (j = 0; j < nvals; j++)
		{
			val = NULL;
			if (!xdr_string(inxdr, &val, -1))
			{
				ni_proplist_free(l);
				return NULL;
			}

			l->ni_proplist_val[i].nip_val.ni_namelist_val[j] = val;
		}
	}

	return l;
}

int
lookupd_query(ni_proplist *l, ni_proplist ***out)
{
	unsigned datalen;
	XDR outxdr;
	XDR inxdr;
	int proc;
	char *listbuf, *s;
	char databuf[_LU_MAXLUSTRLEN * BYTES_PER_XDR_UNIT];
	int n, i, j, na;
	kern_return_t status;
	ni_property *p;

	if (l == NULL) return 0;
	if (out == NULL) return 0;

	if (_lu_port == NULL) return 0;

	status = _lookup_link(_lu_port, "query", &proc);
	if (status != KERN_SUCCESS) return 0;

	xdrmem_create(&outxdr, databuf, sizeof(databuf), XDR_ENCODE);

	na = l->ni_proplist_len;

	/* Encode attribute count */
	if (!xdr_int(&outxdr, &na))
	{
		xdr_destroy(&outxdr);
		return 0;
	}

	for (i = 0; i < l->ni_proplist_len; i++)
	{
		p = &(l->ni_proplist_val[i]);
		s = NULL;
		if (!xdr_string(&outxdr, &s, _LU_MAXLUSTRLEN))
		{
			xdr_destroy(&outxdr);
			return 0;
		}
		p->nip_name = s;

		if (!xdr_int(&outxdr, &(p->nip_val.ni_namelist_len)))
		{
			xdr_destroy(&outxdr);
			return 0;
		}

		for (j = 0; j < p->nip_val.ni_namelist_len; j++)
		{
			s = NULL;
			if (!xdr_string(&outxdr, &s, _LU_MAXLUSTRLEN))
			{
				xdr_destroy(&outxdr);
				return 0;
			}
			p->nip_val.ni_namelist_val[j] = s;
		}
	}

	listbuf = NULL;
	datalen = 0;

	n = xdr_getpos(&outxdr) / BYTES_PER_XDR_UNIT;
	status = _lookup_all(_lu_port, proc, (unit *)databuf, n, &listbuf, &datalen);
	if (status != KERN_SUCCESS)
	{
		xdr_destroy(&outxdr);
		return 0;
	}

	xdr_destroy(&outxdr);

#ifdef NOTDEF
/* NOTDEF because OOL buffers are counted in bytes with untyped IPC */
	datalen *= BYTES_PER_XDR_UNIT;
#endif

	xdrmem_create(&inxdr, listbuf, datalen, XDR_DECODE);

	if (!xdr_int(&inxdr, &n))
	{
		xdr_destroy(&inxdr);
		return 0;
	}

	if (n == 0)
	{
		xdr_destroy(&inxdr);
		return 0;
	}

	*out = (ni_proplist **)malloc(n * sizeof(ni_proplist *));

	for (i = 0; i < n; i++)
	{
		(*out)[i] = _lookupd_xdr_dictionary(&inxdr);
	}

	xdr_destroy(&inxdr);

	vm_deallocate(mach_task_self(), (vm_address_t)listbuf, datalen);
	
	return n;
}

ni_proplist *
lookupd_make_query(char *cat, char *fmt, ...)
{
	va_list ap;
	char *arg, *f;
	int na, x;
	ni_proplist *l;
	ni_property *p;

	if (fmt == NULL) return NULL;
	if (fmt[0] != 'k') return NULL;

	l = (ni_proplist *)malloc(sizeof(ni_proplist));
	NI_INIT(l);

	na = 0;
	x = -1;

	if (cat != NULL)
	{
		l->ni_proplist_val = (ni_property *)malloc(sizeof(ni_property));
		p = &(l->ni_proplist_val[0]);
		arg = "_lookup_category";
		p->nip_name = strdup(arg);
		p->nip_val.ni_namelist_len = 1;
		p->nip_val.ni_namelist_val = (ni_name *)malloc(sizeof(ni_name));
		p->nip_val.ni_namelist_val[0] = strdup(cat);

		l->ni_proplist_len++;
		x++;
	}

	va_start(ap, fmt);
	for (f = fmt; *f != NULL; f++)
	{
		arg = va_arg(ap, char *);
		if (*f == 'k')
		{
			l->ni_proplist_val = (ni_property *)realloc(l->ni_proplist_val, (l->ni_proplist_len + 1) * sizeof(ni_property));

			p = &(l->ni_proplist_val[l->ni_proplist_len]);
			p->nip_name = strdup(arg);
			p->nip_val.ni_namelist_len = 0;
			p->nip_val.ni_namelist_val = NULL;

			l->ni_proplist_len++;
			x++;
		}
		else
		{
			p = &(l->ni_proplist_val[x]);
			if (p->nip_val.ni_namelist_len == 0)
			{
				p->nip_val.ni_namelist_val = (ni_name *)malloc(sizeof(ni_name));
			}
			else
			{
				p->nip_val.ni_namelist_val = (ni_name *)realloc(p->nip_val.ni_namelist_val, (p->nip_val.ni_namelist_len + 1) * sizeof(ni_name));
			}
			p->nip_val.ni_namelist_val[p->nip_val.ni_namelist_len] = strdup(arg);
			p->nip_val.ni_namelist_len++;
		}
	}
	va_end(ap);

	return l;
}

void
ni_property_merge(ni_property *a, ni_property *b)
{
	int i, j, addme;

	if (a == NULL) return;
	if (b == NULL) return;

	for (j = 0; j < b->nip_val.ni_namelist_len; j++)
	{
		addme = 1;
		for (i = 0; i < (a->nip_val.ni_namelist_len) && (addme == 1); i++)
		{
			if (!strcmp(a->nip_val.ni_namelist_val[i], b->nip_val.ni_namelist_val[j])) addme = 0;
		}

		if (addme == 1)
		{
			a->nip_val.ni_namelist_val = (ni_name *)realloc(a->nip_val.ni_namelist_val, (a->nip_val.ni_namelist_len + 1) * sizeof(ni_name));
			a->nip_val.ni_namelist_val[a->nip_val.ni_namelist_len] = strdup(b->nip_val.ni_namelist_val[j]);
			a->nip_val.ni_namelist_len++;
		}
	}
}

void
ni_proplist_merge(ni_proplist *a, ni_proplist *b)
{
	ni_index wa, wb;
	int addme;

	if (a == NULL) return;
	if (b == NULL) return;

	for (wb = 0; wb < b->ni_proplist_len; wb++)
	{
		addme = 1;
		for (wa = 0; (wa < a->ni_proplist_len) && (addme == 1) ; wa++)
		{
			if (!strcmp(a->ni_proplist_val[wa].nip_name, b->ni_proplist_val[wb].nip_name)) addme = 0;
		}
		if (addme == 1)
		{
			a->ni_proplist_val = (ni_property *)realloc(a->ni_proplist_val, (a->ni_proplist_len + 1) * sizeof(ni_property));
			a->ni_proplist_val[a->ni_proplist_len].nip_name = strdup(b->ni_proplist_val[wb].nip_name);
			a->ni_proplist_val[a->ni_proplist_len].nip_val.ni_namelist_len = 0;
			a->ni_proplist_val[a->ni_proplist_len].nip_val.ni_namelist_val = NULL;
			a->ni_proplist_len++;
		}
	}

	for (wb = 0; wb < b->ni_proplist_len; wb++)
	{
		for (wa = 0; wa < a->ni_proplist_len; wa++)
		{
			if (!strcmp(a->ni_proplist_val[wa].nip_name, b->ni_proplist_val[wb].nip_name))
			{
				ni_property_merge(&(a->ni_proplist_val[wa]), &(b->ni_proplist_val[wb]));
			}
		}
	}
}

static void
_lu_data_free(void *x)
{
	struct _lu_data_s *t;
	int i;

	if (x == NULL) return;

	t = (struct _lu_data_s *)x;

	for (i = 0; i < t->icount; i++)
	{		
		if ((t->idata[i] != NULL) && (t->idata_destructor[i] != NULL))
		{
			(*(t->idata_destructor[i]))(t->idata[i]);
		}

		t->idata[i] = NULL;
		t->idata_destructor[i] = NULL;
	}

	if (t->ikey != NULL) free(t->ikey);
	t->ikey = NULL;

	if (t->idata != NULL) free(t->idata);
	t->idata = NULL;

	if (t->idata_destructor != NULL) free(t->idata_destructor);
	t->idata_destructor = NULL;

	free(t);
}

static void
_lu_data_init()
{
	pthread_key_create(&_info_key, _lu_data_free);
	return;
}

static struct _lu_data_s *
_lu_data_get()
{
	struct _lu_data_s *libinfo_data;

	/*
	 * Only one thread should create the _info_key
	 */
	pthread_once(&_info_key_initialized, _lu_data_init);

	/* Check if this thread already created libinfo_data */
	libinfo_data = pthread_getspecific(_info_key);
	if (libinfo_data != NULL) return libinfo_data;

	libinfo_data = (struct _lu_data_s *)calloc(1, sizeof(struct _lu_data_s));

	pthread_setspecific(_info_key, libinfo_data);
	return libinfo_data;
}

void *
_lu_data_create_key(unsigned int key, void (*destructor)(void *))
{
	struct _lu_data_s *libinfo_data;
	unsigned int i, n;

	libinfo_data = _lu_data_get();

	for (i = 0; i < libinfo_data->icount; i++)
	{
		if (libinfo_data->ikey[i] == key) return libinfo_data->idata[i];
	}

	i = libinfo_data->icount;
	n = i + 1;

	if (i == 0)
	{
		libinfo_data->ikey = (unsigned int *)malloc(sizeof(unsigned int));
		libinfo_data->idata = (void **)malloc(sizeof(void *));
		libinfo_data->idata_destructor = (void (**)(void *))malloc(sizeof(void (*)(void *)));
	}
	else
	{
		libinfo_data->ikey = (unsigned int *)realloc(libinfo_data->ikey, n * sizeof(unsigned int));
		libinfo_data->idata = (void **)realloc(libinfo_data->idata, n * sizeof(void *));
		libinfo_data->idata_destructor = (void (**)(void *))realloc(libinfo_data->idata_destructor, n * sizeof(void (*)(void *)));
	}

	libinfo_data->ikey[i] = key;
	libinfo_data->idata[i] = NULL;
	libinfo_data->idata_destructor[i] = destructor;
	libinfo_data->icount++;

	return NULL;
}

static unsigned int
_lu_data_index(unsigned int key, struct _lu_data_s *libinfo_data)
{
	unsigned int i;

	if (libinfo_data == NULL) return (unsigned int)-1;

	for (i = 0; i < libinfo_data->icount; i++)
	{
		if (libinfo_data->ikey[i] == key) return i;
	}

	return (unsigned int)-1;
}

void
_lu_data_set_key(unsigned int key, void *data)
{
	struct _lu_data_s *libinfo_data;
	unsigned int i;

	libinfo_data = _lu_data_get();

	i = _lu_data_index(key, libinfo_data);
	if (i == (unsigned int)-1) return;

	libinfo_data->idata[i] = data;
}

void *
_lu_data_get_key(unsigned int key)
{
	struct _lu_data_s *libinfo_data;
	unsigned int i;

	libinfo_data = _lu_data_get();

	i = _lu_data_index(key, libinfo_data);
	if (i == (unsigned int)-1) return NULL;

	return libinfo_data->idata[i];
}

void
_lu_data_free_vm_xdr(struct lu_thread_info *tdata)
{
	if (tdata == NULL) return;

	if (tdata->lu_vm != NULL)
	{
		vm_deallocate(mach_task_self(), (vm_address_t)tdata->lu_vm, tdata->lu_vm_length);
		tdata->lu_vm = NULL;
	}
	tdata->lu_vm_length = 0;
	tdata->lu_vm_cursor = 0;

	if (tdata->lu_xdr != NULL)
	{
		xdr_destroy(tdata->lu_xdr);
		free(tdata->lu_xdr);
		tdata->lu_xdr = NULL;
	}
}

int
_lu_xdr_attribute(XDR *xdr, char **key, char ***val, unsigned int *count)
{
	unsigned int i, j, len;
	char **x, *s;

	if (xdr == NULL) return -1;
	if (key == NULL) return -1;
	if (val == NULL) return -1;
	if (count == NULL) return -1;

	*key = NULL;
	*val = NULL;
	*count = 0;

	if (!xdr_string(xdr, key, -1)) return -1;

	if (!xdr_int(xdr, &len))
	{
		free(*key);
		*key = NULL;
		return -1;
	}

	if (len == 0) return 0;
	*count = len;

	x = (char **)calloc(len + 1, sizeof(char *));
	*val = x;

	for (i = 0; i < len; i++)
	{
		s = NULL;
		if (!xdr_string(xdr, &s, -1))
		{
			for (j = 0; j < i; j++) free(x[j]);
			free(x);
			*val = NULL;
			free(*key);
			*key = NULL;
			*count = 0;
			return -1;
		}
		x[i] = s;
	}

	x[len] = NULL;

	return 0;
}

kern_return_t 
_lookup_link(mach_port_t server, lookup_name name, int *procno)
{
	kern_return_t status;
	security_token_t token;
	unsigned int n;

	token.val[0] = -1;
	token.val[1] = -1;

	status = MIG_SERVER_DIED;
	for (n = 0; (status == MIG_SERVER_DIED) && (n < MAX_LOOKUP_ATTEMPTS); n++)
	{
		status = _lookup_link_secure(server, name, procno, &token);
	}

	if (status != KERN_SUCCESS)
	{
#ifdef DEBUG
		syslog(LOG_DEBUG, "pid %u _lookup_link %s status %u", getpid(), name, status);
#endif
		return status;
	}

	if (token.val[0] != 0)
	{
#ifdef DEBUG
		syslog(LOG_DEBUG, "pid %u _lookup_link %s auth failure uid=%d", getpid(), name, token.val[0]);
#endif
		return KERN_FAILURE;
	}

#ifdef DEBUG
	syslog(LOG_DEBUG, "pid %u _lookup_link %s = %d", getpid(), name, *procno);
#endif
	return status;
}

kern_return_t 
_lookup_one(mach_port_t server, int proc, inline_data indata, mach_msg_type_number_t indataCnt, inline_data outdata, mach_msg_type_number_t *outdataCnt)
{
	kern_return_t status;
	security_token_t token;
	unsigned int n;

	token.val[0] = -1;
	token.val[1] = -1;

	status = MIG_SERVER_DIED;
	for (n = 0; (status == MIG_SERVER_DIED) && (n < MAX_LOOKUP_ATTEMPTS); n++)
	{
		status = _lookup_one_secure(server, proc, indata, indataCnt, outdata, outdataCnt, &token);
	}

	if (status != KERN_SUCCESS)
	{
#ifdef DEBUG
		syslog(LOG_DEBUG, "pid %u _lookup_one %d status %u", getpid(), proc, status);
#endif
		return status;
	}

	if (token.val[0] != 0)
	{
#ifdef DEBUG
		syslog(LOG_DEBUG, "pid %u _lookup_one %d auth failure uid=%d", getpid(), proc, token.val[0]);
#endif
		return KERN_FAILURE;
	}

#ifdef DEBUG
	syslog(LOG_DEBUG, "pid %u _lookup_one %d", getpid(), proc);
#endif
	return status;
}

kern_return_t 
_lookup_all(mach_port_t server, int proc, inline_data indata, mach_msg_type_number_t indataCnt, ooline_data *outdata, mach_msg_type_number_t *outdataCnt)
{
	kern_return_t status;
	security_token_t token;
	unsigned int n;

	token.val[0] = -1;
	token.val[1] = -1;

	status = MIG_SERVER_DIED;
	for (n = 0; (status == MIG_SERVER_DIED) && (n < MAX_LOOKUP_ATTEMPTS); n++)
	{
		status = _lookup_all_secure(server, proc, indata, indataCnt, outdata, outdataCnt, &token);
	}

	if (status != KERN_SUCCESS)
	{
#ifdef DEBUG
		syslog(LOG_DEBUG, "pid %u _lookup_all %d status %u", getpid(), proc, status);
#endif
		return status;
	}

	if (token.val[0] != 0)
	{
#ifdef DEBUG
		syslog(LOG_DEBUG, "pid %u _lookup_all %d auth failure uid=%d", getpid(), proc, token.val[0]);
#endif
		return KERN_FAILURE;
	}

#ifdef DEBUG
	syslog(LOG_DEBUG, "pid %u _lookup_all %d", getpid(), proc);
#endif
	return status;
}

kern_return_t 
_lookup_ooall(mach_port_t server, int proc, ooline_data indata, mach_msg_type_number_t indataCnt, ooline_data *outdata, mach_msg_type_number_t *outdataCnt)
{
	kern_return_t status;
	security_token_t token;
	unsigned int n;

	token.val[0] = -1;
	token.val[1] = -1;

	status = MIG_SERVER_DIED;
	for (n = 0; (status == MIG_SERVER_DIED) && (n < MAX_LOOKUP_ATTEMPTS); n++)
	{
		status = _lookup_ooall_secure(server, proc, indata, indataCnt, outdata, outdataCnt, &token);
	}

	if (status != KERN_SUCCESS)
	{
#ifdef DEBUG
		syslog(LOG_DEBUG, "pid %u _lookup_ooall %d status %u", getpid(), proc, status);
#endif
		return status;
	}

	if (token.val[0] != 0)
	{
#ifdef DEBUG
		syslog(LOG_DEBUG, "pid %u _lookup_ooall %d auth failure uid=%d", getpid(), proc, token.val[0]);
#endif
		return KERN_FAILURE;
	}

#ifdef DEBUG
	syslog(LOG_DEBUG, "pid %u _lookup_ooall %d", getpid(), proc);
#endif
	return status;
}
