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
/*
 * RPC lookup
 * Copyright (C) 1989 by NeXT, Inc.
 */

#include <rpc/rpc.h>
#include <netdb.h>
#include <stdlib.h>
#include <mach/mach.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <pthread.h>

#include "_lu_types.h"
#include "lookup.h"
#include "lu_utils.h"
#include "lu_overrides.h"

static pthread_mutex_t _rpc_lock = PTHREAD_MUTEX_INITIALIZER;

#define RPC_GET_NAME 1
#define RPC_GET_NUM 2
#define RPC_GET_ENT 3

static void
free_rpc_data(struct rpcent *r)
{
	char **aliases;

	if (r == NULL) return;

	if (r->r_name != NULL) free(r->r_name);

	aliases = r->r_aliases;
	if (aliases != NULL)
	{
		while (*aliases != NULL) free(*aliases++);
		free(r->r_aliases);
	}
}

static void
free_rpc(struct rpcent *r)
{
	if (r == NULL) return;
	free_rpc_data(r);
	free(r);
}

static void
free_lu_thread_info_rpc(void *x)
{
	struct lu_thread_info *tdata;

	if (x == NULL) return;

	tdata = (struct lu_thread_info *)x;
	
	if (tdata->lu_entry != NULL)
	{
		free_rpc((struct rpcent *)tdata->lu_entry);
		tdata->lu_entry = NULL;
	}

	_lu_data_free_vm_xdr(tdata);

	free(tdata);
}

static struct rpcent *
extract_rpc(XDR *xdr)
{
	struct rpcent *r;
	int i, j, nvals, nkeys, status;
	char *key, **vals;

	if (xdr == NULL) return NULL;

	if (!xdr_int(xdr, &nkeys)) return NULL;

	r = (struct rpcent *)calloc(1, sizeof(struct rpcent));

	for (i = 0; i < nkeys; i++)
	{
		key = NULL;
		vals = NULL;
		nvals = 0;

		status = _lu_xdr_attribute(xdr, &key, &vals, &nvals);
		if (status < 0)
		{
			free_rpc(r);
			return NULL;
		}

		if (nvals == 0)
		{
			free(key);
			continue;
		}

		j = 0;

		if ((r->r_name == NULL) && (!strcmp("name", key)))
		{
			r->r_name = vals[0];
			if (nvals > 1)
			{
				r->r_aliases = (char **)calloc(nvals, sizeof(char *));
				for (j = 1; j < nvals; j++) r->r_aliases[j-1] = vals[j];
			}
			j = nvals;
		}		
		else if (!strcmp("number", key))
		{
			r->r_number= atoi(vals[0]);
		}

		free(key);
		if (vals != NULL)
		{
			for (; j < nvals; j++) free(vals[j]);
			free(vals);
		}
	}

	if (r->r_name == NULL) r->r_name = strdup("");
	if (r->r_aliases == NULL) r->r_aliases = (char **)calloc(1, sizeof(char *));

	return r;
}

static struct rpcent *
copy_rpc(struct rpcent *in)
{
	int i, len;
	struct rpcent *r;

	if (in == NULL) return NULL;

	r = (struct rpcent *)calloc(1, sizeof(struct rpcent));

	r->r_name = LU_COPY_STRING(in->r_name);

	len = 0;
	if (in->r_aliases != NULL)
	{
		for (len = 0; in->r_aliases[len] != NULL; len++);
	}

	r->r_aliases = (char **)calloc(len + 1, sizeof(char *));
	for (i = 0; i < len; i++)
	{
		r->r_aliases[i] = strdup(in->r_aliases[i]);
	}

	r->r_number = in->r_number;

	return r;
}

static void
recycle_rpc(struct lu_thread_info *tdata, struct rpcent *in)
{
	struct rpcent *r;

	if (tdata == NULL) return;
	r = (struct rpcent *)tdata->lu_entry;

	if (in == NULL)
	{
		free_rpc(r);
		tdata->lu_entry = NULL;
	}

	if (tdata->lu_entry == NULL)
	{
		tdata->lu_entry = in;
		return;
	}

	free_rpc_data(r);

	r->r_name = in->r_name;
	r->r_aliases = in->r_aliases;
	r->r_number = in->r_number;

	free(in);
}

static struct rpcent *
lu_getrpcbynumber(long number)
{
	struct rpcent *r;
	unsigned datalen;
	XDR inxdr;
	static int proc = -1;
	char *lookup_buf;
	int count;

	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "getrpcbynumber", &proc) != KERN_SUCCESS)
		{
			return NULL;
		}
	}

	number = htonl(number);
	datalen = 0;
	lookup_buf = NULL;

	if (_lookup_all(_lu_port, proc, (unit *)&number, 1, &lookup_buf, &datalen)
		!= KERN_SUCCESS)
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

	r = extract_rpc(&inxdr);
	xdr_destroy(&inxdr);
	vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);

	return r;
}

static struct rpcent *
lu_getrpcbyname(const char *name)
{
	struct rpcent *r;
	unsigned datalen;
	char namebuf[_LU_MAXLUSTRLEN + BYTES_PER_XDR_UNIT];
	XDR outxdr, inxdr;
	static int proc = -1;
	char *lookup_buf;
	int count;

	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "getrpcbyname", &proc) != KERN_SUCCESS)
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

	r = extract_rpc(&inxdr);
	xdr_destroy(&inxdr);
	vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);

	return r;
}

static void
lu_endrpcent(void)
{
	struct lu_thread_info *tdata;

	tdata = _lu_data_create_key(_lu_data_key_rpc, free_lu_thread_info_rpc);
	_lu_data_free_vm_xdr(tdata);
}

static void
lu_setrpcent()
{
	lu_endrpcent();
}

static struct rpcent *
lu_getrpcent()
{
	struct rpcent *r;
	static int proc = -1;
	struct lu_thread_info *tdata;

	tdata = _lu_data_create_key(_lu_data_key_rpc, free_lu_thread_info_rpc);
	if (tdata == NULL)
	{
		tdata = (struct lu_thread_info *)calloc(1, sizeof(struct lu_thread_info));
		_lu_data_set_key(_lu_data_key_rpc, tdata);
	}

	if (tdata->lu_vm == NULL)
	{
		if (proc < 0)
		{
			if (_lookup_link(_lu_port, "getrpcent", &proc) != KERN_SUCCESS)
			{
				lu_endrpcent();
				return NULL;
			}
		}

		if (_lookup_all(_lu_port, proc, NULL, 0, &(tdata->lu_vm), &(tdata->lu_vm_length)) != KERN_SUCCESS)
		{
			lu_endrpcent();
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
			lu_endrpcent();
			return NULL;
		}
	}

	if (tdata->lu_vm_cursor == 0)
	{
		lu_endrpcent();
		return NULL;
	}

	r = extract_rpc(tdata->lu_xdr);
	if (r == NULL)
	{
		lu_endrpcent();
		return NULL;
	}

	tdata->lu_vm_cursor--;
	
	return r;
}

static struct rpcent *
getrpc(const char *name, long number, int source)
{
	struct rpcent *res = NULL;
	struct lu_thread_info *tdata;

	tdata = _lu_data_create_key(_lu_data_key_rpc, free_lu_thread_info_rpc);
	if (tdata == NULL)
	{
		tdata = (struct lu_thread_info *)calloc(1, sizeof(struct lu_thread_info));
		_lu_data_set_key(_lu_data_key_rpc, tdata);
	}

	if (_lu_running())
	{
		switch (source)
		{
			case RPC_GET_NAME:
				res = lu_getrpcbyname(name);
				break;
			case RPC_GET_NUM:
				res = lu_getrpcbynumber(number);
				break;
			case RPC_GET_ENT:
				res = lu_getrpcent();
				break;
			default: res = NULL;
		}
	}
	else
	{
		pthread_mutex_lock(&_rpc_lock);
		switch (source)
		{
			case RPC_GET_NAME:
				res = copy_rpc(_old_getrpcbyname(name));
				break;
			case RPC_GET_NUM:
				res = copy_rpc(_old_getrpcbynumber(number));
				break;
			case RPC_GET_ENT:
				res = copy_rpc(_old_getrpcent());
				break;
			default: res = NULL;
		}
		pthread_mutex_unlock(&_rpc_lock);
	}

	recycle_rpc(tdata, res);
	return (struct rpcent *)tdata->lu_entry;
}

struct rpcent *
getrpcbyname(const char *name)
{
	return getrpc(name, -2, RPC_GET_NAME);
}

struct rpcent *
getrpcbynumber(long number)
{
	return getrpc(NULL, number, RPC_GET_NUM);
}

struct rpcent *
getrpcent(void)
{
	return getrpc(NULL, -2, RPC_GET_ENT);
}

void
setrpcent(int stayopen)
{
	if (_lu_running()) lu_setrpcent();
	else _old_setrpcent(stayopen);
}

void
endrpcent(void)
{
	if (_lu_running()) lu_endrpcent();
	else _old_endrpcent();
}
