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
 * Bootparams lookup - netinfo only
 * Copyright (C) 1989 by NeXT, Inc.
 */
#include <stdlib.h>
#include <mach/mach.h>
#include <stdio.h>
#include <string.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <bootparams.h>

#include "lookup.h"
#include "_lu_types.h"
#include "lu_utils.h"

static void 
free_bootparams_data(struct bootparamsent *b)
{
	char **param;

	if (b == NULL) return;

	if (b->bp_name != NULL) free(b->bp_name);

	param = b->bp_bootparams;
	if (param != NULL)
	{
		while (*param != NULL) free(*param++);
		free(b->bp_bootparams);
	}
}

static void 
free_bootparams(struct bootparamsent *b)
{
	if (b == NULL) return;
	free_bootparams_data(b);
	free(b);
}

static void
free_lu_thread_info_bootparams(void *x)
{
	struct lu_thread_info *tdata;

	if (x == NULL) return;

	tdata = (struct lu_thread_info *)x;
	
	if (tdata->lu_entry != NULL)
	{
		free_bootparams((struct bootparamsent *)tdata->lu_entry);
		tdata->lu_entry = NULL;
	}

	_lu_data_free_vm_xdr(tdata);

	free(tdata);
}

static struct bootparamsent *
extract_bootparams(XDR *xdr)
{
	int i, j, nkeys, nvals, status;
	char *key, **vals;
	struct bootparamsent *b;

	if (xdr == NULL) return NULL;

	if (!xdr_int(xdr, &nkeys)) return NULL;

	b = (struct bootparamsent *)calloc(1, sizeof(struct bootparamsent));

	for (i = 0; i < nkeys; i++)
	{
		key = NULL;
		vals = NULL;
		nvals = 0;

		status = _lu_xdr_attribute(xdr, &key, &vals, &nvals);
		if (status < 0)
		{
			free_bootparams(b);
			return NULL;
		}

		if (nvals == 0)
		{
			free(key);
			continue;
		}

		j = 0;
	
		if ((b->bp_name == NULL) && (!strcmp("name", key)))
		{
			b->bp_name = vals[0];
			j = 1;
		}
		else if ((b->bp_bootparams == NULL) && (!strcmp("bootparams", key)))
		{
			b->bp_bootparams = vals;
			j = nvals;
			vals = NULL;
		}

		free(key);
		if (vals != NULL)
		{
			for (; j < nvals; j++) free(vals[j]);
			free(vals);
		}
	}

	if (b->bp_name == NULL) b->bp_name = strdup("");
	if (b->bp_bootparams == NULL) b->bp_bootparams = (char **)calloc(1, sizeof(char *));

	return b;
}

static void 
recycle_bootparams(struct lu_thread_info *tdata, struct bootparamsent *in)
{
	struct bootparamsent *b;

	if (tdata == NULL) return;
	b = (struct bootparamsent *)tdata->lu_entry;

	if (in == NULL)
	{
		free_bootparams(b);
		tdata->lu_entry = NULL;
	}

	if (tdata->lu_entry == NULL)
	{
		tdata->lu_entry = in;
		return;
	}

	free_bootparams_data(b);

	b->bp_name = in->bp_name;
	b->bp_bootparams = in->bp_bootparams;

	free(in);
}

static struct bootparamsent *
lu_bootparams_getbyname(const char *name)
{
	struct bootparamsent *b;
	unsigned datalen;
	char namebuf[_LU_MAXLUSTRLEN + BYTES_PER_XDR_UNIT];
	char *lookup_buf;
	XDR outxdr;
	XDR inxdr;
	int size;
	static int proc = -1;
	int count;

	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "bootparams_getbyname", &proc)
			!= KERN_SUCCESS)
		{
			return (NULL);
		}
	}

	xdrmem_create(&outxdr, namebuf, sizeof(namebuf), XDR_ENCODE);
	if (!xdr__lu_string(&outxdr, (_lu_string *)&name))
	{
		xdr_destroy(&outxdr);
		return (NULL);
	}

	size = xdr_getpos(&outxdr);
	xdr_destroy(&outxdr);

	datalen = 0;
	lookup_buf = NULL;

	if (_lookup_all(_lu_port, proc, (unit *)namebuf, size, &lookup_buf, 
		&datalen) != KERN_SUCCESS)
	{
		return (NULL);
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

	b = extract_bootparams(&inxdr);
	xdr_destroy(&inxdr);
	vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);

	return b;
}

static void
lu_bootparams_endent(void)
{
	struct lu_thread_info *tdata;

	tdata = _lu_data_create_key(_lu_data_key_bootparams, free_lu_thread_info_bootparams);
	_lu_data_free_vm_xdr(tdata);
}

static void
lu_bootparams_setent(void)
{
	lu_bootparams_endent();
}

static struct bootparamsent *
lu_bootparams_getent(void)
{
	struct bootparamsent *b;
	static int proc = -1;
	struct lu_thread_info *tdata;

	tdata = _lu_data_create_key(_lu_data_key_bootparams, free_lu_thread_info_bootparams);
	if (tdata == NULL)
	{
		tdata = (struct lu_thread_info *)calloc(1, sizeof(struct lu_thread_info));
		_lu_data_set_key(_lu_data_key_bootparams, tdata);
	}

	if (tdata->lu_vm == NULL)
	{
		if (proc < 0)
		{
			if (_lookup_link(_lu_port, "bootparams_getent", &proc) != KERN_SUCCESS)
			{
				lu_bootparams_endent();
				return NULL;
			}
		}

		if (_lookup_all(_lu_port, proc, NULL, 0, &(tdata->lu_vm), &(tdata->lu_vm_length)) != KERN_SUCCESS)
		{
			lu_bootparams_endent();
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
			lu_bootparams_endent();
			return NULL;
		}
	}

	if (tdata->lu_vm_cursor == 0)
	{
		lu_bootparams_endent();
		return NULL;
	}

	b = extract_bootparams(tdata->lu_xdr);
	if (b == NULL)
	{
		lu_bootparams_endent();
		return NULL;
	}

	tdata->lu_vm_cursor--;

	return b;	
}

struct bootparamsent *
bootparams_getbyname(const char *name)
{
	struct bootparamsent *res;
	struct lu_thread_info *tdata;

	tdata = _lu_data_create_key(_lu_data_key_bootparams, free_lu_thread_info_bootparams);
	if (tdata == NULL)
	{
		tdata = (struct lu_thread_info *)calloc(1, sizeof(struct lu_thread_info));
		_lu_data_set_key(_lu_data_key_bootparams, tdata);
	}

	if (_lu_running())
	{
		res = lu_bootparams_getbyname(name);
		recycle_bootparams(tdata, res);
		return (struct bootparamsent *)tdata->lu_entry;
	}

	return NULL;
}

struct bootparamsent *
bootparams_getent(void)
{
	struct bootparamsent *res;
	struct lu_thread_info *tdata;

	tdata = _lu_data_create_key(_lu_data_key_bootparams, free_lu_thread_info_bootparams);
	if (tdata == NULL)
	{
		tdata = (struct lu_thread_info *)calloc(1, sizeof(struct lu_thread_info));
		_lu_data_set_key(_lu_data_key_bootparams, tdata);
	}

	if (_lu_running())
	{
		res = lu_bootparams_getent();
		recycle_bootparams(tdata, res);
		return (struct bootparamsent *)tdata->lu_entry;
	}

	return NULL;
}

void
bootparams_setent(void)
{
	if (_lu_running()) lu_bootparams_setent();
}

void
bootparams_endent(void)
{
	if (_lu_running()) lu_bootparams_endent();
}
