/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
 * Netgroup lookup
 * Copyright (C) 1989 by NeXT, Inc.
 */
#include <netgr.h>
#include <mach/mach.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <rpc/types.h>
#include <rpc/xdr.h>

#include "_lu_types.h"
#include "lookup.h"
#include "lu_utils.h"
#include "lu_overrides.h"

#define FIX(x) ((x == NULL) ? NULL : (_lu_string *)&(x))

struct lu_netgrent
{
	char *ng_host;
	char *ng_user;
	char *ng_domain;
};

static void 
free_netgroup_data(struct lu_netgrent *ng)
{
	if (ng == NULL) return;

	if (ng->ng_host != NULL) free(ng->ng_host);
	if (ng->ng_user != NULL) free(ng->ng_user);
	if (ng->ng_domain != NULL) free(ng->ng_domain);
}

static void 
free_netgroup(struct lu_netgrent *ng)
{
	if (ng == NULL) return;
	free_netgroup_data(ng);
	free(ng);
 }

static void
free_lu_thread_info_netgroup(void *x)
{
	struct lu_thread_info *tdata;

	if (x == NULL) return;

	tdata = (struct lu_thread_info *)x;
	
	if (tdata->lu_entry != NULL)
	{
		free_netgroup((struct lu_netgrent *)tdata->lu_entry);
		tdata->lu_entry = NULL;
	}

	_lu_data_free_vm_xdr(tdata);

	free(tdata);
}

static struct lu_netgrent *
extract_netgroup(XDR *xdr)
{
	char *h, *u, *d;
	struct lu_netgrent *ng;

	if (xdr == NULL) return NULL;

	h = NULL;
	u = NULL;
	d = NULL;

	if (!xdr_string(xdr, &h, LU_LONG_STRING_LENGTH))
	{
		return NULL;
	}

	if (!xdr_string(xdr, &u, LU_LONG_STRING_LENGTH))
	{
		free(h);
		return NULL;
	}

	if (!xdr_string(xdr, &d, LU_LONG_STRING_LENGTH))
	{
		free(h);
		free(u);
		return NULL;
	}

	ng = (struct lu_netgrent *)calloc(1, sizeof(struct lu_netgrent));

	ng->ng_host = h;
	ng->ng_user = u;
	ng->ng_domain = d;

	return ng;
}

#ifdef NOTDEF
static struct lu_netgrent *
copy_netgroup(struct lu_netgrent *in)
{
	struct lu_netgrent *ng;

	if (in == NULL) return NULL;

	ng = (struct group *)calloc(1, sizeof(struct lu_netgrent));

	ng->ng_host = LU_COPY_STRING(in->ng_host);
	ng->ng_user = LU_COPY_STRING(in->ng_user);
	ng->ng_domain = LU_COPY_STRING(in->ng_domain);

	return ng;
}
#endif

static void
recycle_netgroup(struct lu_thread_info *tdata, struct lu_netgrent *in)
{
	struct lu_netgrent *ng;

	if (tdata == NULL) return;
	ng = (struct lu_netgrent *)tdata->lu_entry;

	if (in == NULL)
	{
		free_netgroup(ng);
		tdata->lu_entry = NULL;
	}

	if (tdata->lu_entry == NULL)
	{
		tdata->lu_entry = in;
		return;
	}

	free_netgroup_data(ng);

	ng->ng_host = in->ng_host;
	ng->ng_user = in->ng_user;
	ng->ng_domain = in->ng_domain;

	free(in);
}


static int
lu_innetgr(const char *group, const char *host, const char *user,
	const char *domain)
{
	unsigned datalen;
	XDR xdr;
	char namebuf[4*_LU_MAXLUSTRLEN + 3*BYTES_PER_XDR_UNIT];
	static int proc = -1;
	int size;
	int res;
	_lu_innetgr_args args;
	char *lookup_buf;

	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "innetgr", &proc) != KERN_SUCCESS)
		{
			return 0;
		}
	}

	args.group = (char *)group;
	args.host = FIX(host);
	args.user = FIX(user);
	args.domain = FIX(domain);

	xdrmem_create(&xdr, namebuf, sizeof(namebuf), XDR_ENCODE);
	if (!xdr__lu_innetgr_args(&xdr, &args))
	{
		xdr_destroy(&xdr);
		return 0;
	}

	size = xdr_getpos(&xdr) / BYTES_PER_XDR_UNIT;
	xdr_destroy(&xdr);

	datalen = 0;
	lookup_buf = NULL;

	if (_lookup_all(_lu_port, proc, (unit *)namebuf, size, &lookup_buf, &datalen) != KERN_SUCCESS)
	{
		return 0;
	}

	datalen *= BYTES_PER_XDR_UNIT;
	if ((lookup_buf == NULL) || (datalen == 0)) return 0;

	xdrmem_create(&xdr, lookup_buf, datalen, XDR_DECODE);
	if (!xdr_int(&xdr, &res))
	{
		xdr_destroy(&xdr);
		vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);
		return 0;
	}

	xdr_destroy(&xdr);
	vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);

	return 1;
}

static void
lu_endnetgrent(void)
{
	struct lu_thread_info *tdata;

	tdata = _lu_data_create_key(_lu_data_key_netgroup, free_lu_thread_info_netgroup);
	_lu_data_free_vm_xdr(tdata);
}


/* 
 * This is different than the other setXXXent routines
 * since this is really more like getnetgrbyname() than
 * getnetgrent().
 */ 
static void
lu_setnetgrent(const char *name)
{
	unsigned datalen;
	char namebuf[_LU_MAXLUSTRLEN + BYTES_PER_XDR_UNIT];
	XDR outxdr;
	static int proc = -1;
	struct lu_thread_info *tdata;

	tdata = _lu_data_create_key(_lu_data_key_netgroup, free_lu_thread_info_netgroup);
	if (tdata == NULL)
	{
		tdata = (struct lu_thread_info *)calloc(1, sizeof(struct lu_thread_info));
		_lu_data_set_key(_lu_data_key_netgroup, tdata);
	}

	lu_endnetgrent();

	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "getnetgrent", &proc) != KERN_SUCCESS)
		{
			lu_endnetgrent();
			return;
		}
	}

	xdrmem_create(&outxdr, namebuf, sizeof(namebuf), XDR_ENCODE);
	if (!xdr__lu_string(&outxdr, (_lu_string *)&name))
	{
		xdr_destroy(&outxdr);
		lu_endnetgrent();
		return;
	}
	
	datalen = xdr_getpos(&outxdr);
	xdr_destroy(&outxdr);
	if (_lookup_all(_lu_port, proc, (unit *)namebuf, datalen / BYTES_PER_XDR_UNIT, &(tdata->lu_vm), &(tdata->lu_vm_length)) != KERN_SUCCESS)
	{
		lu_endnetgrent();
		return;
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
	if (!xdr_int(tdata->lu_xdr, &tdata->lu_vm_cursor)) lu_endnetgrent();
}


static struct lu_netgrent *
lu_getnetgrent(void)
{
	struct lu_netgrent *ng;
	struct lu_thread_info *tdata;

	tdata = _lu_data_create_key(_lu_data_key_netgroup, free_lu_thread_info_netgroup);
	if (tdata == NULL) return NULL;

	if (tdata->lu_vm_cursor == 0)
	{
		lu_endnetgrent();
		return NULL;
	}

	ng = extract_netgroup(tdata->lu_xdr);
	if (ng == NULL)
	{
		lu_endnetgrent();
		return NULL;
	}

	tdata->lu_vm_cursor--;
	
	return ng;
}

int 
innetgr(const char *group, const char *host, const char *user,
	const char *domain)
{
	if (_lu_running()) return (lu_innetgr(group, host, user, domain));
	return 0;
}

int
getnetgrent(char **host, char **user, char **domain)
{
	struct lu_netgrent *res = NULL;
	struct lu_thread_info *tdata;

	tdata = _lu_data_create_key(_lu_data_key_netgroup, free_lu_thread_info_netgroup);
	if (tdata == NULL)
	{
		tdata = (struct lu_thread_info *)calloc(1, sizeof(struct lu_thread_info));
		_lu_data_set_key(_lu_data_key_netgroup, tdata);
	}

	res = NULL;
	if (_lu_running()) res = lu_getnetgrent();

	recycle_netgroup(tdata, res);
	if (res == NULL) return 0;

	if (host != NULL) *host = res->ng_host;
	if (user != NULL) *user = res->ng_user;
	if (domain != NULL) *domain = res->ng_domain;

	return 1;
}

void
setnetgrent(const char *name)
{
	if (_lu_running()) lu_setnetgrent(name);
}

void
endnetgrent(void)
{
	if (_lu_running()) lu_endnetgrent();
}
