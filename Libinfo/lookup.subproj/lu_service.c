/*
 * Copyright (c) 1999-2002 Apple Computer, Inc. All rights reserved.
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
/*
 * Services file lookup
 * Copyright (C) 1989 by NeXT, Inc.
 */
#include <stdlib.h>
#include <mach/mach.h>
#include <stdio.h>
#include <string.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>

#include "_lu_types.h"
#include "lookup.h"
#include "lu_utils.h"

static pthread_mutex_t _service_lock = PTHREAD_MUTEX_INITIALIZER;

#define S_GET_NAME 1
#define S_GET_PORT 2
#define S_GET_ENT 3

extern struct servent *_old_getservbyport();
extern struct servent *_old_getservbyname();
extern struct servent *_old_getservent();
extern void _old_setservent();
extern void _old_endservent();
extern void _old_setservfile();

static void
free_service_data(struct servent *s)
{
	char **aliases;

	if (s == NULL) return;

	if (s->s_name != NULL) free(s->s_name);
	if (s->s_proto != NULL) free(s->s_proto);

	aliases = s->s_aliases;
	if (aliases != NULL)
	{
		while (*aliases != NULL) free(*aliases++);
		free(s->s_aliases);
	}
}

static void
free_service(struct servent *s)
{
	if (s == NULL) return;
	free_service_data(s);
	free(s);
}

static void
free_lu_thread_info_service(void *x)
{
	struct lu_thread_info *tdata;

	if (x == NULL) return;

	tdata = (struct lu_thread_info *)x;
	
	if (tdata->lu_entry != NULL)
	{
		free_service((struct servent *)tdata->lu_entry);
		tdata->lu_entry = NULL;
	}

	_lu_data_free_vm_xdr(tdata);

	free(tdata);
}

static struct servent *
extract_service(XDR *xdr, const char *proto)
{
	struct servent *s;
	int i, j, nvals, nkeys, status;
	char *key, **vals;

	if (xdr == NULL) return NULL;

	if (!xdr_int(xdr, &nkeys)) return NULL;

	s = (struct servent *)calloc(1, sizeof(struct servent));

	for (i = 0; i < nkeys; i++)
	{
		key = NULL;
		vals = NULL;
		nvals = 0;

		status = _lu_xdr_attribute(xdr, &key, &vals, &nvals);
		if (status < 0)
		{
			free_service(s);
			return NULL;
		}

		if (nvals == 0)
		{
			free(key);
			continue;
		}

		j = 0;

		if ((s->s_name == NULL) && (!strcmp("name", key)))
		{
			s->s_name = vals[0];
			if (nvals > 1)
			{
				s->s_aliases = (char **)calloc(nvals, sizeof(char *));
				for (j = 1; j < nvals; j++) s->s_aliases[j-1] = vals[j];
			}
			j = nvals;
		}		
		else if ((s->s_proto == NULL) && (!strcmp("protocol", key)))
		{
			if ((proto == NULL) || (proto[0] == '\0'))
			{
				s->s_proto = vals[0];
				j = 1;
			}
			else
			{
				s->s_proto = strdup(proto);
			}
		}
		else if ((s->s_port == 0) && (!strcmp("port", key)))
		{
			s->s_port = htons(atoi(vals[0]));
		}
		
		free(key);
		if (vals != NULL)
		{
			for (; j < nvals; j++) free(vals[j]);
			free(vals);
		}
	}

	if (s->s_name == NULL) s->s_name = strdup("");
	if (s->s_proto == NULL) s->s_proto = strdup("");
	if (s->s_aliases == NULL) s->s_aliases = (char **)calloc(1, sizeof(char *));

	return s;
}

static struct servent *
copy_service(struct servent *in)
{
	int i, len;
	struct servent *s;

	if (in == NULL) return NULL;

	s = (struct servent *)calloc(1, sizeof(struct servent));

	s->s_name = LU_COPY_STRING(in->s_name);

	len = 0;
	if (in->s_aliases != NULL)
	{
		for (len = 0; in->s_aliases[len] != NULL; len++);
	}

	s->s_aliases = (char **)calloc(len + 1, sizeof(char *));
	for (i = 0; i < len; i++)
	{
		s->s_aliases[i] = strdup(in->s_aliases[i]);
	}

	s->s_proto = LU_COPY_STRING(in->s_proto);
	s->s_port = in->s_port;

	return s;
}

static void
recycle_service(struct lu_thread_info *tdata, struct servent *in)
{
	struct servent *s;

	if (tdata == NULL) return;
	s = (struct servent *)tdata->lu_entry;

	if (in == NULL)
	{
		free_service(s);
		tdata->lu_entry = NULL;
	}

	if (tdata->lu_entry == NULL)
	{
		tdata->lu_entry = in;
		return;
	}

	free_service_data(s);

	s->s_name = in->s_name;
	s->s_aliases = in->s_aliases;
	s->s_proto = in->s_proto;
	s->s_port = in->s_port;

	free(in);
}

static struct servent *
lu_getservbyport(int port, const char *proto)
{
	struct servent *s;
	unsigned int datalen;
	XDR outxdr, inxdr;
	static int proc = -1;
	char output_buf[_LU_MAXLUSTRLEN + 3 * BYTES_PER_XDR_UNIT];
	char *lookup_buf;
	int count;

	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "getservbyport", &proc) != KERN_SUCCESS)
		{
			return NULL;
		}
	}

	/* Encode NULL for xmission to lookupd. */
	if (proto == NULL) proto = "";	

	xdrmem_create(&outxdr, output_buf, sizeof(output_buf), XDR_ENCODE);
	if (!xdr_int(&outxdr, &port) || !xdr__lu_string(&outxdr, (_lu_string *)&proto))
	{
		xdr_destroy(&outxdr);
		return NULL;
	}

	datalen = 0;
	lookup_buf = NULL;

	if (_lookup_all(_lu_port, proc, (unit *)output_buf, 
		xdr_getpos(&outxdr) / BYTES_PER_XDR_UNIT, &lookup_buf, &datalen)
		!= KERN_SUCCESS)
	{
		xdr_destroy(&outxdr);
		return NULL;
	}

	xdr_destroy(&outxdr);

	datalen *= BYTES_PER_XDR_UNIT;
	if ((lookup_buf == NULL) || (datalen == 0)) return NULL;

	xdrmem_create(&inxdr, lookup_buf, datalen, XDR_DECODE);

	count = 0;
	if (!xdr_int(&inxdr, &count))
	{
		xdr_destroy(&inxdr);
		vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);
		return NULL;
	}

	if (count == 0)
	{
		xdr_destroy(&inxdr);
		vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);
		return NULL;
	}

	/*
	 * lookupd will only send back a reply for a service with the protocol specified
	 * if it finds a match.  We pass the protocol name to extract_service, which
	 * copies the requested protocol name into the returned servent.  This is a
	 * bit of a kludge, but since NetInfo / lookupd treat services as single entities
	 * with multiple protocols, we are forced to do some special-case handling. 
	 */
	s = extract_service(&inxdr, proto);
	xdr_destroy(&inxdr);
	vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);

	return s;
}

static struct servent *
lu_getservbyname(const char *name, const char *proto)
{
	struct servent *s;
	unsigned int datalen;
	char *lookup_buf;
	char output_buf[2 * (_LU_MAXLUSTRLEN + BYTES_PER_XDR_UNIT)];
	XDR outxdr, inxdr;
	static int proc = -1;
	int count;

	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "getservbyname", &proc) != KERN_SUCCESS)
		{
		    return NULL;
		}
	}

	/* Encode NULL for xmission to lookupd. */
	if (proto == NULL) proto = "";

	xdrmem_create(&outxdr, output_buf, sizeof(output_buf), XDR_ENCODE);
	if (!xdr__lu_string(&outxdr, (_lu_string *)&name) ||
	    !xdr__lu_string(&outxdr, (_lu_string *)&proto))
	{
		xdr_destroy(&outxdr);
		return NULL;
	}

	datalen = 0;
	lookup_buf = NULL;

	if (_lookup_all(_lu_port, proc, (unit *)output_buf,
		xdr_getpos(&outxdr) / BYTES_PER_XDR_UNIT, &lookup_buf, &datalen)
		!= KERN_SUCCESS)
	{
		xdr_destroy(&outxdr);
		return NULL;
	}

	xdr_destroy(&outxdr);

	datalen *= BYTES_PER_XDR_UNIT;
	if ((lookup_buf == NULL) || (datalen == 0)) return NULL;

	xdrmem_create(&inxdr, lookup_buf, datalen, XDR_DECODE);

	count = 0;
	if (!xdr_int(&inxdr, &count))
	{
		xdr_destroy(&inxdr);
		vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);
		return NULL;
	}

	if (count == 0)
	{
		xdr_destroy(&inxdr);
		vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);
		return NULL;
	}

	/*
	 * lookupd will only send back a reply for a service with the protocol specified
	 * if it finds a match.  We pass the protocol name to extract_service, which
	 * copies the requested protocol name into the returned servent.  This is a
	 * bit of a kludge, but since NetInfo / lookupd treat services as single entities
	 * with multiple protocols, we are forced to do some special-case handling. 
	 */
	s = extract_service(&inxdr, proto);
	xdr_destroy(&inxdr);
	vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);

	return s;
}

static void
lu_endservent()
{
	struct lu_thread_info *tdata;

	tdata = _lu_data_create_key(_lu_data_key_service, free_lu_thread_info_service);
	_lu_data_free_vm_xdr(tdata);
}

static void
lu_setservent()
{
	lu_endservent();
}

static struct servent *
lu_getservent()
{
	struct servent *s;
	static int proc = -1;
	struct lu_thread_info *tdata;

	tdata = _lu_data_create_key(_lu_data_key_service, free_lu_thread_info_service);
	if (tdata == NULL)
	{
		tdata = (struct lu_thread_info *)calloc(1, sizeof(struct lu_thread_info));
		_lu_data_set_key(_lu_data_key_service, tdata);
	}

	if (tdata->lu_vm == NULL)
	{
		if (proc < 0)
		{
			if (_lookup_link(_lu_port, "getservent", &proc) != KERN_SUCCESS)
			{
				lu_endservent();
				return NULL;
			}
		}

		if (_lookup_all(_lu_port, proc, NULL, 0, &(tdata->lu_vm), &(tdata->lu_vm_length)) != KERN_SUCCESS)
		{
			lu_endservent();
			return NULL;
		}

		/* mig stubs measure size in words (4 bytes) */
		tdata->lu_vm_length *= 4;

		if (tdata->lu_xdr != NULL)
		{
			xdr_destroy(tdata->lu_xdr);
			free(tdata->lu_xdr);
		}
		tdata->lu_xdr = (XDR *)calloc(1, sizeof(XDR));

		xdrmem_create(tdata->lu_xdr, tdata->lu_vm, tdata->lu_vm_length, XDR_DECODE);
		if (!xdr_int(tdata->lu_xdr, &tdata->lu_vm_cursor))
		{
			lu_endservent();
			return NULL;
		}
	}

	if (tdata->lu_vm_cursor == 0)
	{
		lu_endservent();
		return NULL;
	}

	s = extract_service(tdata->lu_xdr, NULL);
	if (s == NULL)
	{
		lu_endservent();
		return NULL;
	}

	tdata->lu_vm_cursor--;
	
	return s;
}

static struct servent *
getserv(const char *name, const char *proto, int port, int source)
{
	struct servent *res = NULL;
	struct lu_thread_info *tdata;

	tdata = _lu_data_create_key(_lu_data_key_service, free_lu_thread_info_service);
	if (tdata == NULL)
	{
		tdata = (struct lu_thread_info *)calloc(1, sizeof(struct lu_thread_info));
		_lu_data_set_key(_lu_data_key_service, tdata);
	}

	if (_lu_running())
	{
		switch (source)
		{
			case S_GET_NAME:
				res = lu_getservbyname(name, proto);
				break;
			case S_GET_PORT:
				res = lu_getservbyport(port, proto);
				break;
			case S_GET_ENT:
				res = lu_getservent();
				break;
			default: res = NULL;
		}
	}
	else
	{
		pthread_mutex_lock(&_service_lock);
		switch (source)
		{
			case S_GET_NAME:
				res = copy_service(_old_getservbyname(name, proto));
				break;
			case S_GET_PORT:
				res = copy_service(_old_getservbyport(port, proto));
				break;
			case S_GET_ENT:
				res = copy_service(_old_getservent());
				break;
			default: res = NULL;
		}
		pthread_mutex_unlock(&_service_lock);
	}

	recycle_service(tdata, res);
	return (struct servent *)tdata->lu_entry;
}

struct servent *
getservbyport(int port, const char *proto)
{
	return getserv(NULL, proto, port, S_GET_PORT);
}

struct servent *
getservbyname(const char *name, const char *proto)
{
	return getserv(name, proto, 0, S_GET_NAME);
}

struct servent *
getservent(void)
{
	return getserv(NULL, NULL, 0, S_GET_ENT);
}

void
setservent(int stayopen)
{
	if (_lu_running()) lu_setservent();
	else _old_setservent();
}

void
endservent(void)
{
	if (_lu_running()) lu_endservent();
	else _old_endservent();
}
