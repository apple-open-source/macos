/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
 * Protocol lookup
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

#include "_lu_types.h"
#include "lookup.h"
#include "lu_utils.h"

extern struct protoent *_old_getprotobynumber();
extern struct protoent *_old_getprotobyname();
extern struct protoent *_old_getprotoent();
extern void _old_setprotoent();
extern void _old_endprotoent();

#define PROTO_GET_NAME 1
#define PROTO_GET_NUM 2
#define PROTO_GET_ENT 3

static void
free_protocol_data(struct protoent *p)
{
	char **aliases;

	if (p == NULL) return;

	if (p->p_name != NULL) free(p->p_name);
	aliases = p->p_aliases;
	if (aliases != NULL)
	{
		while (*aliases != NULL) free(*aliases++);
		free(p->p_aliases);
	}
}

static void
free_protocol(struct protoent *p)
{
	if (p == NULL) return;
	free_protocol_data(p);
	free(p);
}

static void
free_lu_thread_info_protocol(void *x)
{
	struct lu_thread_info *tdata;

	if (x == NULL) return;

	tdata = (struct lu_thread_info *)x;
	
	if (tdata->lu_entry != NULL)
	{
		free_protocol((struct protoent *)tdata->lu_entry);
		tdata->lu_entry = NULL;
	}

	_lu_data_free_vm_xdr(tdata);

	free(tdata);
}

static struct protoent *
extract_protocol(XDR *xdr)
{
	struct protoent *p;
	int i, j, nvals, nkeys, status;
	char *key, **vals;

	if (xdr == NULL) return NULL;

	if (!xdr_int(xdr, &nkeys)) return NULL;

	p = (struct protoent *)calloc(1, sizeof(struct protoent));

	for (i = 0; i < nkeys; i++)
	{
		key = NULL;
		vals = NULL;
		nvals = 0;

		status = _lu_xdr_attribute(xdr, &key, &vals, &nvals);
		if (status < 0)
		{
			free_protocol(p);
			return NULL;
		}

		if (nvals == 0)
		{
			free(key);
			continue;
		}

		j = 0;

		if ((p->p_name == NULL) && (!strcmp("name", key)))
		{
			p->p_name = vals[0];
			if (nvals > 1)
			{
				p->p_aliases = (char **)calloc(nvals, sizeof(char *));
				for (j = 1; j < nvals; j++) p->p_aliases[j-1] = vals[j];
			}
			j = nvals;
		}
		else if (!strcmp("number", key))
		{
			p->p_proto = atoi(vals[0]);
		}

		free(key);
		if (vals != NULL)
		{
			for (; j < nvals; j++) free(vals[j]);
			free(vals);
		}
	}

	if (p->p_name == NULL) p->p_name = strdup("");
	if (p->p_aliases == NULL) p->p_aliases = (char **)calloc(1, sizeof(char *));

	return p;
}

static struct protoent *
copy_protocol(struct protoent *in)
{
	int i, len;
	struct protoent *p;

	if (in == NULL) return NULL;

	p = (struct protoent *)calloc(1, sizeof(struct protoent));

	p->p_proto = in->p_proto;
	p->p_name = LU_COPY_STRING(in->p_name);

	len = 0;
	if (in->p_aliases != NULL)
	{
		for (len = 0; in->p_aliases[len] != NULL; len++);
	}

	p->p_aliases = (char **)calloc(len + 1, sizeof(char *));
	for (i = 0; i < len; i++)
	{
		p->p_aliases[i] = strdup(in->p_aliases[i]);
	}

	return p;
}

static void
recycle_protocol(struct lu_thread_info *tdata, struct protoent *in)
{
	struct protoent *p;

	if (tdata == NULL) return;
	p = (struct protoent *)tdata->lu_entry;

	if (in == NULL)
	{
		free_protocol(p);
		tdata->lu_entry = NULL;
	}

	if (tdata->lu_entry == NULL)
	{
		tdata->lu_entry = in;
		return;
	}

	free_protocol_data(p);

	p->p_proto = in->p_proto;
	p->p_name = in->p_name;
	p->p_aliases = in->p_aliases;

	free(in);
}

static struct protoent *
lu_getprotobynumber(long number)
{
	struct protoent *p;
	unsigned int datalen;
	XDR inxdr;
	static int proc = -1;
	char *lookup_buf;
	int count;

	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "getprotobynumber", &proc) != KERN_SUCCESS)
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

	p = extract_protocol(&inxdr);
	xdr_destroy(&inxdr);
	vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);

	return p;
}

static struct protoent *
lu_getprotobyname(const char *name)
{
	struct protoent *p;
	unsigned int datalen;
	char namebuf[_LU_MAXLUSTRLEN + BYTES_PER_XDR_UNIT];
	XDR outxdr;
	XDR inxdr;
	static int proc = -1;
	char *lookup_buf;
	int count;

	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "getprotobyname", &proc) != KERN_SUCCESS)
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

	p = extract_protocol(&inxdr);
	xdr_destroy(&inxdr);
	vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);

	return p;
}

static void
lu_endprotoent()
{
	struct lu_thread_info *tdata;

	tdata = _lu_data_create_key(_lu_data_key_protocol, free_lu_thread_info_protocol);
	_lu_data_free_vm_xdr(tdata);
}

static void
lu_setprotoent()
{
	lu_endprotoent();
}


static struct protoent *
lu_getprotoent()
{
	struct protoent *p;
	static int proc = -1;
	struct lu_thread_info *tdata;

	tdata = _lu_data_create_key(_lu_data_key_protocol, free_lu_thread_info_protocol);
	if (tdata == NULL)
	{
		tdata = (struct lu_thread_info *)calloc(1, sizeof(struct lu_thread_info));
		_lu_data_set_key(_lu_data_key_protocol, tdata);
	}

	if (tdata->lu_vm == NULL)
	{
		if (proc < 0)
		{
			if (_lookup_link(_lu_port, "getprotoent", &proc) != KERN_SUCCESS)
			{
				lu_endprotoent();
				return NULL;
			}
		}

		if (_lookup_all(_lu_port, proc, NULL, 0, &(tdata->lu_vm), &(tdata->lu_vm_length)) != KERN_SUCCESS)
		{
			lu_endprotoent();
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
			lu_endprotoent();
			return NULL;
		}
	}

	if (tdata->lu_vm_cursor == 0)
	{
		lu_endprotoent();
		return NULL;
	}

	p = extract_protocol(tdata->lu_xdr);
	if (p == NULL)
	{
		lu_endprotoent();
		return NULL;
	}

	tdata->lu_vm_cursor--;
	
	return p;
}

static struct protoent *
getproto(const char *name, int number, int source)
{
	struct protoent *res = NULL;
	struct lu_thread_info *tdata;

	tdata = _lu_data_create_key(_lu_data_key_protocol, free_lu_thread_info_protocol);
	if (tdata == NULL)
	{
		tdata = (struct lu_thread_info *)calloc(1, sizeof(struct lu_thread_info));
		_lu_data_set_key(_lu_data_key_protocol, tdata);
	}

	if (_lu_running())
	{
		switch (source)
		{
			case PROTO_GET_NAME:
				res = lu_getprotobyname(name);
				break;
			case PROTO_GET_NUM:
				res = lu_getprotobynumber(number);
				break;
			case PROTO_GET_ENT:
				res = lu_getprotoent();
				break;
			default: res = NULL;
		}
	}
	else
	{
		switch (source)
		{
			case PROTO_GET_NAME:
				res = copy_protocol(_old_getprotobyname(name));
				break;
			case PROTO_GET_NUM:
				res = copy_protocol(_old_getprotobynumber(number));
				break;
			case PROTO_GET_ENT:
				res = copy_protocol(_old_getprotoent());
				break;
			default: res = NULL;
		}
	}

	recycle_protocol(tdata, res);
	return (struct protoent *)tdata->lu_entry;
}

struct protoent *
getprotobyname(const char *name)
{
	return getproto(name, -2, PROTO_GET_NAME);
}

struct protoent *
getprotobynumber(int number)
{
	return getproto(NULL, number, PROTO_GET_NUM);
}

struct protoent *
getprotoent(void)
{
	return getproto(NULL, -2, PROTO_GET_ENT);
}

void
setprotoent(int stayopen)
{
	if (_lu_running()) lu_setprotoent();
	else _old_setprotoent(stayopen);
}

void
endprotoent(void)
{
	if (_lu_running()) lu_endprotoent();
	else _old_endprotoent();
}
