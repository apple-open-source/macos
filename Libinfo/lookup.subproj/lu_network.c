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
 * network lookup
 * Copyright (C) 1989 by NeXT, Inc.
 */
#include <stdlib.h>
#include <mach/mach.h>
#include <stdio.h>
#include <string.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "_lu_types.h"
#include "lookup.h"
#include "lu_utils.h"

static pthread_mutex_t _network_lock = PTHREAD_MUTEX_INITIALIZER;

#define N_GET_NAME 1
#define N_GET_ADDR 2
#define N_GET_ENT 3

extern struct netent *_res_getnetbyaddr();
extern struct netent *_res_getnetbyname();
extern struct netent *_old_getnetbyaddr();
extern struct netent *_old_getnetbyname();
extern struct netent *_old_getnetent();
extern void _old_setnetent();
extern void _old_endnetent();

extern mach_port_t _lu_port;
extern int _lu_running(void);

static void
free_network_data(struct netent *n)
{
	char **aliases;

	if (n == NULL) return;

	free(n->n_name);

	aliases = n->n_aliases;
	if (aliases != NULL)
	{
		while (*aliases != NULL) free(*aliases++);
		free(n->n_aliases);
	}
}

static void
free_network(struct netent *n)
{
	if (n == NULL) return;
	free_network_data(n);
	free(n);
}

static void
free_lu_thread_info_network(void *x)
{
	struct lu_thread_info *tdata;

	if (x == NULL) return;

	tdata = (struct lu_thread_info *)x;
	
	if (tdata->lu_entry != NULL)
	{
		free_network((struct netent *)tdata->lu_entry);
		tdata->lu_entry = NULL;
	}

	_lu_data_free_vm_xdr(tdata);

	free(tdata);
}

static struct netent *
extract_network(XDR *xdr)
{
	struct netent *n;
	int i, j, nvals, nkeys, status;
	char *key, **vals;

	if (xdr == NULL) return NULL;

	if (!xdr_int(xdr, &nkeys)) return NULL;

	n = (struct netent *)calloc(1, sizeof(struct netent));

	n->n_addrtype = AF_INET;

	for (i = 0; i < nkeys; i++)
	{
		key = NULL;
		vals = NULL;
		nvals = 0;

		status = _lu_xdr_attribute(xdr, &key, &vals, &nvals);
		if (status < 0)
		{
			free_network(n);
			return NULL;
		}

		if (nvals == 0)
		{
			free(key);
			continue;
		}

		j = 0;

		if ((n->n_name == NULL) && (!strcmp("name", key)))
		{
			n->n_name = vals[0];
			if (nvals > 1)
			{
				n->n_aliases = (char **)calloc(nvals, sizeof(char *));
				for  (j = 1; j < nvals; j++) n->n_aliases[j-1] = vals[j];
			}
			j = nvals;
		}
		else if (!strcmp("address", key))
		{
			n->n_net = inet_network(vals[0]);
		}
	
		free(key);
		if (vals != NULL)
		{
			for (; j < nvals; j++) free(vals[j]);
			free(vals);
		}
	}

	if (n->n_name == NULL) n->n_name = strdup("");
	if (n->n_aliases == NULL) n->n_aliases = (char **)calloc(1, sizeof(char *));

	return n;
}

static struct netent *
copy_network(struct netent *in)
{
	int i, len;
	struct netent *n;

	if (in == NULL) return NULL;

	n = (struct netent *)calloc(1, sizeof(struct netent));

	n->n_name = LU_COPY_STRING(in->n_name);

	len = 0;
	if (in->n_aliases != NULL)
	{
		for (len = 0; in->n_aliases[len] != NULL; len++);
	}

	n->n_aliases = (char **)calloc(len + 1, sizeof(char *));
	for (i = 0; i < len; i++)
	{
		n->n_aliases[i] = strdup(in->n_aliases[i]);
	}

	n->n_addrtype = in->n_addrtype;
	n->n_net = in->n_net;

	return n;
}

static void
recycle_network(struct lu_thread_info *tdata, struct netent *in)
{
	struct netent *n;

	if (tdata == NULL) return;
	n = (struct netent *)tdata->lu_entry;

	if (in == NULL)
	{
		free_network(n);
		tdata->lu_entry = NULL;
	}

	if (tdata->lu_entry == NULL)
	{
		tdata->lu_entry = in;
		return;
	}

	free_network_data(n);

	n->n_name = in->n_name;
	n->n_aliases = in->n_aliases;
	n->n_addrtype = in->n_addrtype;
	n->n_net = in->n_net;

	free(in);
}

static struct netent *
lu_getnetbyaddr(long addr, int type)
{
	struct netent *n;
	unsigned datalen;
	XDR inxdr;
	static int proc = -1;
	char *lookup_buf;
	int count;

	if (type != AF_INET) return NULL;

	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "getnetbyaddr", &proc) != KERN_SUCCESS)
		{
			return NULL;
		}
	}

	addr = htonl(addr);
	datalen = 0;
	lookup_buf = NULL;

	if (_lookup_all(_lu_port, proc, (unit *)&addr, 1, &lookup_buf, &datalen) != KERN_SUCCESS)
	{
		return NULL;
	}

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

	n = extract_network(&inxdr);
	xdr_destroy(&inxdr);
	vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);

	return n;
}

static struct netent *
lu_getnetbyname(const char *name)
{
	struct netent *n;
	unsigned datalen;
	char namebuf[_LU_MAXLUSTRLEN + BYTES_PER_XDR_UNIT];
	XDR outxdr;
	XDR inxdr;
	static int proc = -1;
	char *lookup_buf;
	int count;

	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "getnetbyname", &proc) != KERN_SUCCESS)
		{
			return NULL;
		}
	}

	xdrmem_create(&outxdr, namebuf, sizeof(namebuf), XDR_ENCODE);
	if (!xdr__lu_string(&outxdr, (_lu_string *)&name))
	{
		xdr_destroy(&outxdr);
		return NULL;
	}

	datalen = 0;
	lookup_buf = NULL;

	if (_lookup_all(_lu_port, proc, (unit *)namebuf,
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

	n = extract_network(&inxdr);
	xdr_destroy(&inxdr);
	vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);

	return n;
}

static void
lu_endnetent()
{
	struct lu_thread_info *tdata;

	tdata = _lu_data_create_key(_lu_data_key_network, free_lu_thread_info_network);
	_lu_data_free_vm_xdr(tdata);
}

static void
lu_setnetent()
{
	lu_endnetent();
}

static struct netent *
lu_getnetent()
{
	static int proc = -1;
	struct lu_thread_info *tdata;
	struct netent *n;

	tdata = _lu_data_create_key(_lu_data_key_network, free_lu_thread_info_network);
	if (tdata == NULL)
	{
		tdata = (struct lu_thread_info *)calloc(1, sizeof(struct lu_thread_info));
		_lu_data_set_key(_lu_data_key_network, tdata);
	}

	if (tdata->lu_vm == NULL)
	{
		if (proc < 0)
		{
			if (_lookup_link(_lu_port, "getnetent", &proc) != KERN_SUCCESS)
			{
				lu_endnetent();
				return NULL;
			}
		}

		if (_lookup_all(_lu_port, proc, NULL, 0, &(tdata->lu_vm), &(tdata->lu_vm_length)) != KERN_SUCCESS)
		{
			lu_endnetent();
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
			lu_endnetent();
			return NULL;
		}
	}

	if (tdata->lu_vm_cursor == 0)
	{
		lu_endnetent();
		return NULL;
	}

	n = extract_network(tdata->lu_xdr);
	if (n == NULL)
	{
		lu_endnetent();
		return NULL;
	}

	tdata->lu_vm_cursor--;
	
	return n;
}

static struct netent *
getnet(const char *name, long addr, int type, int source)
{
	struct netent *res = NULL;
	struct lu_thread_info *tdata;

	tdata = _lu_data_create_key(_lu_data_key_network, free_lu_thread_info_network);
	if (tdata == NULL)
	{
		tdata = (struct lu_thread_info *)calloc(1, sizeof(struct lu_thread_info));
		_lu_data_set_key(_lu_data_key_network, tdata);
	}

	if (_lu_running())
	{
		switch (source)
		{
			case N_GET_NAME:
				res = lu_getnetbyname(name);
				break;
			case N_GET_ADDR:
				res = lu_getnetbyaddr(addr, type);
				break;
			case N_GET_ENT:
				res = lu_getnetent();
				break;
			default: res = NULL;
		}
	}
	else
	{
		pthread_mutex_lock(&_network_lock);
		switch (source)
		{
			case N_GET_NAME:
				res = copy_network(_old_getnetbyname(name));
				break;
			case N_GET_ADDR:
				res = copy_network(_old_getnetbyaddr(addr, type));
				break;
			case N_GET_ENT:
				res = copy_network(_old_getnetent());
				break;
			default: res = NULL;
		}
		pthread_mutex_unlock(&_network_lock);
	}

	recycle_network(tdata, res);
	return (struct netent *)tdata->lu_entry;
}

struct netent *
getnetbyaddr(long addr, int type)
{
	return getnet(NULL, addr, type, N_GET_ADDR);
}

struct netent *
getnetbyname(const char *name)
{
	return getnet(name, 0, 0, N_GET_NAME);
}

struct netent *
getnetent(void)
{
	return getnet(NULL, 0, 0, N_GET_ENT);
}

void
setnetent(int stayopen)
{
	if (_lu_running()) lu_setnetent();
	else _old_setnetent(stayopen);
}

void
endnetent(void)
{
	if (_lu_running()) lu_endnetent();
	else _old_endnetent();
}
