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
 * Alias lookup
 * Copyright (C) 1989 by NeXT, Inc.
 */

#include <stdlib.h>
#include <mach/mach.h>
#include <stdio.h>
#include <string.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <aliasdb.h>
#include <pthread.h>

#include "_lu_types.h"
#include "lookup.h"
#include "lu_utils.h"
#include "lu_overrides.h"

static pthread_mutex_t _alias_lock = PTHREAD_MUTEX_INITIALIZER;

static void 
free_alias_data(struct aliasent *a)
{
	int i;

	if (a == NULL) return;

	if (a->alias_name != NULL) free(a->alias_name);
	for (i = 0; i < a->alias_members_len; i++) free(a->alias_members[i]);
	if (a->alias_members != NULL) free(a->alias_members);
}

static void 
free_alias(struct aliasent *a)
{
	if (a == NULL) return;
	free_alias_data(a);
	free(a);
}

static void
free_lu_thread_info_alias(void *x)
{
	struct lu_thread_info *tdata;

	if (x == NULL) return;

	tdata = (struct lu_thread_info *)x;
	
	if (tdata->lu_entry != NULL)
	{
		free_alias((struct aliasent *)tdata->lu_entry);
		tdata->lu_entry = NULL;
	}

	_lu_data_free_vm_xdr(tdata);

	free(tdata);
}

static struct aliasent *
extract_alias(XDR *xdr)
{
	int i, j, nkeys, nvals, status;
	char *key, **vals;
	struct aliasent *a;

	if (xdr == NULL) return NULL;

	if (!xdr_int(xdr, &nkeys)) return NULL;

	a = (struct aliasent *)calloc(1, sizeof(struct aliasent));

	for (i = 0; i < nkeys; i++)
	{
		key = NULL;
		vals = NULL;
		nvals = 0;

		status = _lu_xdr_attribute(xdr, &key, &vals, &nvals);
		if (status < 0)
		{
			free_alias(a);
			return NULL;
		}

		if (nvals == 0)
		{
			free(key);
			continue;
		}

		j = 0;

		if ((a->alias_name == NULL) && (!strcmp("name", key)))
		{
			a->alias_name = vals[0];
			j = 1;
		}
		else if (!strcmp("alias_local", key))
		{
			a->alias_local = atoi(vals[0]);
		}
		else if ((a->alias_members == NULL) && (!strcmp("members", key)))
		{
			a->alias_members_len = nvals;
			a->alias_members = vals;
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

	if (a->alias_name == NULL) a->alias_name = strdup("");
	if (a->alias_members == NULL) a->alias_members = (char **)calloc(1, sizeof(char *));

	return a;
}

static struct aliasent *
copy_alias(struct aliasent *in)
{
	int i;
	struct aliasent *a;

	if (in == NULL) return NULL;

	a = (struct aliasent *)calloc(1, sizeof(struct aliasent));

	a->alias_name = LU_COPY_STRING(in->alias_name);

	a->alias_members_len = in->alias_members_len;

	if (a->alias_members_len == 0)
	{
		a->alias_members = (char **)calloc(1, sizeof(char *));
	}
	else
	{
		a->alias_members = (char **)calloc(a->alias_members_len, sizeof(char *));
	}

	for (i = 0; i < a->alias_members_len; i++)
	{
		a->alias_members[i] = strdup(in->alias_members[i]);
	}

	a->alias_local = in->alias_local;

	return a;
}

static void
recycle_alias(struct lu_thread_info *tdata, struct aliasent *in)
{
	struct aliasent *a;

	if (tdata == NULL) return;
	a = (struct aliasent *)tdata->lu_entry;

	if (in == NULL)
	{
		free_alias(a);
		tdata->lu_entry = NULL;
	}

	if (tdata->lu_entry == NULL)
	{
		tdata->lu_entry = in;
		return;
	}

	free_alias_data(a);

	a->alias_name = in->alias_name;
	a->alias_members_len = in->alias_members_len;
	a->alias_members = in->alias_members;
	a->alias_local = in->alias_local;

	free(in);
}

static struct aliasent *
lu_alias_getbyname(const char *name)
{
	struct aliasent *a;
	unsigned int datalen;
	char namebuf[_LU_MAXLUSTRLEN + BYTES_PER_XDR_UNIT];
	char *lookup_buf;
	XDR outxdr;
	XDR inxdr;
	static int proc = -1;
	int count;

	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "alias_getbyname", &proc) != KERN_SUCCESS)
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

	a = extract_alias(&inxdr);
	xdr_destroy(&inxdr);
	vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);

	return a;
}

static void
lu_alias_endent(void)
{
	struct lu_thread_info *tdata;

	tdata = _lu_data_create_key(_lu_data_key_alias, free_lu_thread_info_alias);
	_lu_data_free_vm_xdr(tdata);
}

static void
lu_alias_setent(void)
{
	lu_alias_endent();
}

static struct aliasent *
lu_alias_getent(void)
{
	static int proc = -1;
	struct lu_thread_info *tdata;
	struct aliasent *a;

	tdata = _lu_data_create_key(_lu_data_key_alias, free_lu_thread_info_alias);
	if (tdata == NULL)
	{
		tdata = (struct lu_thread_info *)calloc(1, sizeof(struct lu_thread_info));
		_lu_data_set_key(_lu_data_key_alias, tdata);
	}

	if (tdata->lu_vm == NULL)
	{
		if (proc < 0)
		{
			if (_lookup_link(_lu_port, "alias_getent", &proc) != KERN_SUCCESS)
			{
				lu_alias_endent();
				return NULL;
			}
		}

		if (_lookup_all(_lu_port, proc, NULL, 0, &(tdata->lu_vm), &(tdata->lu_vm_length)) != KERN_SUCCESS)
		{
			lu_alias_endent();
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
			lu_alias_endent();
			return NULL;
		}
	}

	if (tdata->lu_vm_cursor == 0)
	{
		lu_alias_endent();
		return NULL;
	}


	a = extract_alias(tdata->lu_xdr);
	if (a == NULL)
	{
		lu_alias_endent();
		return NULL;
	}

	tdata->lu_vm_cursor--;
	
	return a;
}

struct aliasent *
alias_getbyname(const char *name)
{
	struct aliasent *res = NULL;
	struct lu_thread_info *tdata;

	tdata = _lu_data_create_key(_lu_data_key_alias, free_lu_thread_info_alias);
	if (tdata == NULL)
	{
		tdata = (struct lu_thread_info *)calloc(1, sizeof(struct lu_thread_info));
		_lu_data_set_key(_lu_data_key_alias, tdata);
	}

	if (_lu_running())
	{
		res = lu_alias_getbyname(name);
	}
	else
	{
		pthread_mutex_lock(&_alias_lock);
		res = copy_alias(_old_alias_getbyname(name));
		pthread_mutex_unlock(&_alias_lock);
	}

	recycle_alias(tdata, res);
	return (struct aliasent *)tdata->lu_entry;

}

struct aliasent *
alias_getent(void)
{
	struct aliasent *res = NULL;
	struct lu_thread_info *tdata;

	tdata = _lu_data_create_key(_lu_data_key_alias, free_lu_thread_info_alias);
	if (tdata == NULL)
	{
		tdata = (struct lu_thread_info *)calloc(1, sizeof(struct lu_thread_info));
		_lu_data_set_key(_lu_data_key_alias, tdata);
	}

	if (_lu_running())
	{
		res = lu_alias_getent();
	}
	else
	{
		pthread_mutex_lock(&_alias_lock);
		res = copy_alias(_old_alias_getent());
		pthread_mutex_unlock(&_alias_lock);
	}

	recycle_alias(tdata, res);
	return (struct aliasent *)tdata->lu_entry;

}

void
alias_setent(void)
{
	if (_lu_running()) lu_alias_setent();
	else _old_alias_setent();
}

void
alias_endent(void)
{
	if (_lu_running()) lu_alias_endent();
	else _old_alias_endent();
}
