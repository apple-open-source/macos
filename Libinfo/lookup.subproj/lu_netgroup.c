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

#define FIX(x) ((x == NULL) ? NULL : &(x))

static struct netgrent global_netgr;
static int global_free = 1;
static char *netgr_data = NULL;
static unsigned netgr_datalen;
static int netgr_nentries = 0;
static int netgr_start = 1;
static XDR netgr_xdr;

static void
freeold(void)
{
	if (global_free == 1) return;

	free(global_netgr.ng_host);
	free(global_netgr.ng_user);
	free(global_netgr.ng_domain);

	global_free = 1;
}

static void
convert_netgr(_lu_netgrent *lu_netgr)
{
	freeold();

	global_netgr.ng_host = strdup(lu_netgr->ng_host);
	global_netgr.ng_user = strdup(lu_netgr->ng_user);
	global_netgr.ng_domain = strdup(lu_netgr->ng_domain);

	global_free = 0;
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
	unit lookup_buf[MAX_INLINE_UNITS];

	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "innetgr", &proc) != KERN_SUCCESS)
		{
			return (0);
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
		return (0);
	}

	size = xdr_getpos(&xdr) / BYTES_PER_XDR_UNIT;
	xdr_destroy(&xdr);

	datalen = MAX_INLINE_UNITS;
	if (_lookup_one(_lu_port, proc, (unit *)namebuf, size, lookup_buf, 
		&datalen) != KERN_SUCCESS)
	{
		return (0);
	}

	datalen *= BYTES_PER_XDR_UNIT;
	xdrmem_create(&xdr, lookup_buf, datalen, XDR_DECODE);
	if (!xdr_int(&xdr, &res))
	{
		xdr_destroy(&xdr);
		return (0);
	}

	xdr_destroy(&xdr);
	return (res);
}

static void
lu_endnetgrent(void)
{
	netgr_nentries = 0;
	if (netgr_data != NULL)
	{
		freeold();
		vm_deallocate(mach_task_self(), (vm_address_t)netgr_data, netgr_datalen);
		netgr_data = NULL;
	}
}

/* 
 * This is different than the other setXXXent routines
 * since this is really more like getnetgrbyname() than
 * getnetgrent().
 */ 
static void
lu_setnetgrent(const char *group)
{
	unsigned datalen;
	char namebuf[_LU_MAXLUSTRLEN + BYTES_PER_XDR_UNIT];
	XDR outxdr;
	static int proc = -1;

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
	if (!xdr__lu_string(&outxdr, &group))
	{
		xdr_destroy(&outxdr);
		lu_endnetgrent();
		return;
	}
	
	datalen = MAX_INLINE_UNITS;
	if (_lookup_all(_lu_port, proc, (unit *)namebuf,
		xdr_getpos(&outxdr) / BYTES_PER_XDR_UNIT,
		&netgr_data, &netgr_datalen) != KERN_SUCCESS)
	{
		xdr_destroy(&outxdr);
		lu_endnetgrent();
		return;
	}

	xdr_destroy(&outxdr);

#ifdef NOTDEF
/* NOTDEF because OOL buffers are counted in bytes with untyped IPC */
	netgr_datalen *= BYTES_PER_XDR_UNIT;
#endif

	xdrmem_create(&netgr_xdr, netgr_data, 
		netgr_datalen, XDR_DECODE);
	if (!xdr_int(&netgr_xdr, &netgr_nentries))
	{
		xdr_destroy(&netgr_xdr);
		lu_endnetgrent();
	}
}


struct netgrent *
lu_getnetgrent(void)
{
	_lu_netgrent lu_netgr;

	if (netgr_nentries == 0)
	{
		xdr_destroy(&netgr_xdr);
		lu_endnetgrent();
		return (NULL);
	}

	bzero(&lu_netgr, sizeof(lu_netgr));
	if (!xdr__lu_netgrent(&netgr_xdr, &lu_netgr))
	{
		xdr_destroy(&netgr_xdr);
		lu_endnetgrent();
		return (NULL);
	}

	netgr_nentries--;
	convert_netgr(&lu_netgr);
	xdr_free(xdr__lu_netgrent, &lu_netgr);
	return (&global_netgr);
}

int 
innetgr(const char *group, const char *host, const char *user,
	const char *domain)
{
	if (_lu_running()) return (lu_innetgr(group, host, user, domain));
//	return (_old_innetgr(group, host, user, domain));
	return (0);
}

struct netgrent *
getnetgrent(void)
{
	if (_lu_running()) return (lu_getnetgrent());
//	return (_old_getnetgrent());
	return (NULL);
}

void
setnetgrent(const char *group)
{
	if (_lu_running()) lu_setnetgrent(group);
//	else _old_setnetgrent(group);
}

void
endnetgrent(void)
{
	if (_lu_running()) lu_endnetgrent();
//	else _old_endnetgrent();
}
