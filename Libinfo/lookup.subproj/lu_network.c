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
 * network lookup
 * Copyright (C) 1989 by NeXT, Inc.
 */
#include <stdlib.h>
#include <mach/mach.h>
#include <stdio.h>
#include <string.h>
#include "lookup.h"
#include <rpc/types.h>
#include <rpc/xdr.h>
#include "_lu_types.h"
#include <netdb.h>
#include "lu_utils.h"
#include <sys/socket.h>
#import <netinet/in.h>

extern struct netent *_res_getnetbyaddr();
extern struct netent *_res_getnetbyname();
extern struct netent *_old_getnetbyaddr();
extern struct netent *_old_getnetbyname();
extern struct netent *_old_getnetent();
extern void _old_setnetent();
extern void _old_endnetent();

static lookup_state n_state = LOOKUP_CACHE;
static struct netent global_n;
static int global_free = 1;
static char *n_data = NULL;
static unsigned n_datalen;
static int n_nentries;
static int n_start = 1;
static XDR n_xdr;

static void
freeold(void)
{
	char **aliases;

	if (global_free == 1) return;

	free(global_n.n_name);

	aliases = global_n.n_aliases;
	if (aliases != NULL)
	{
		while (*aliases != NULL) free(*aliases++);
		free(global_n.n_aliases);
	}

	global_free = 1;
}

static void
convert_n(_lu_netent *lu_n)
{
	int i, len;

	freeold();

	global_n.n_name = strdup(lu_n->n_names.n_names_val[0]);

	len = lu_n->n_names.n_names_len - 1;
	global_n.n_aliases = malloc((len + 1) * sizeof(char *));

	for (i = 0; i < len; i++)
	{
		global_n.n_aliases[i] = strdup(lu_n->n_names.n_names_val[i + 1]);
	}

	global_n.n_aliases[len] = NULL;

	global_n.n_addrtype = AF_INET;
	global_n.n_net = lu_n->n_net;

	global_free = 0;
}

static struct netent *
lu_getnetbyaddr(long addr, int type)
{
	unsigned datalen;
	_lu_netent_ptr lu_n;
	XDR xdr;
	static int proc = -1;
	unit lookup_buf[MAX_INLINE_UNITS];
	
	if (type != AF_INET)
	{
		return (NULL);
	}

	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "getnetbyaddr", &proc) != KERN_SUCCESS)
		{
			return (NULL);
		}
	}

	addr = htonl(addr);
	datalen = MAX_INLINE_UNITS;
	if (_lookup_one(_lu_port, proc, (unit *)&addr, 1, lookup_buf, &datalen)
		!= KERN_SUCCESS)
	{
		return (NULL);
	}

	datalen *= BYTES_PER_XDR_UNIT;
	xdrmem_create(&xdr, lookup_buf, datalen, XDR_DECODE);
	lu_n = NULL;
	if (!xdr__lu_netent_ptr(&xdr, &lu_n) || (lu_n == NULL))
	{
		xdr_destroy(&xdr);
		return (NULL);
	}

	xdr_destroy(&xdr);

	convert_n(lu_n);
	xdr_free(xdr__lu_netent_ptr, &lu_n);
	return (&global_n);
}

static struct netent *
lu_getnetbyname(const char *name)
{
	unsigned datalen;
	char namebuf[_LU_MAXLUSTRLEN + BYTES_PER_XDR_UNIT];
	XDR outxdr;
	XDR inxdr;
	_lu_netent_ptr lu_n;
	static int proc = -1;
	unit lookup_buf[MAX_INLINE_UNITS];

	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "getnetbyname", &proc) != KERN_SUCCESS)
		{
			return (NULL);
		}
	}

	xdrmem_create(&outxdr, namebuf, sizeof(namebuf), XDR_ENCODE);
	if (!xdr__lu_string(&outxdr, &name))
	{
		xdr_destroy(&outxdr);
		return (NULL);
	}

	datalen = MAX_INLINE_UNITS;
	if (_lookup_one(_lu_port, proc, (unit *)namebuf,
		xdr_getpos(&outxdr) / BYTES_PER_XDR_UNIT, lookup_buf, &datalen)
		!= KERN_SUCCESS)
	{
		xdr_destroy(&outxdr);
		return (NULL);
	}

	xdr_destroy(&outxdr);

	datalen *= BYTES_PER_XDR_UNIT;
	xdrmem_create(&inxdr, lookup_buf, datalen,
		XDR_DECODE);
	lu_n = NULL;
	if (!xdr__lu_netent_ptr(&inxdr, &lu_n) || (lu_n == NULL))
	{
		xdr_destroy(&inxdr);
		return (NULL);
	}

	xdr_destroy(&inxdr);

	convert_n(lu_n);
	xdr_free(xdr__lu_netent_ptr, &lu_n);
	return (&global_n);
}

static void
lu_endnetent()
{
	n_nentries = 0;
	if (n_data != NULL)
	{
		freeold();
		vm_deallocate(mach_task_self(), (vm_address_t)n_data, n_datalen);
		n_data = NULL;
	}
}

static void
lu_setnetent()
{
	lu_endnetent();
	n_start = 1;
}

static struct netent *
lu_getnetent()
{
	static int proc = -1;
	_lu_netent lu_n;

	if (n_start == 1)
	{
		n_start = 0;

		if (proc < 0)
		{
			if (_lookup_link(_lu_port, "getnetent", &proc) != KERN_SUCCESS)
			{
				lu_endnetent();
				return (NULL);
			}
		}

		if (_lookup_all(_lu_port, proc, NULL, 0, &n_data, &n_datalen)
			!= KERN_SUCCESS)
		{
			lu_endnetent();
			return (NULL);
		}

#ifdef NOTDEF
/* NOTDEF because OOL buffers are counted in bytes with untyped IPC */
		n_datalen *= BYTES_PER_XDR_UNIT;
#endif
		xdrmem_create(&n_xdr, n_data, n_datalen,
			XDR_DECODE);
		if (!xdr_int(&n_xdr, &n_nentries))
		{
			xdr_destroy(&n_xdr);
			lu_endnetent();
			return (NULL);
		}
	}

	if (n_nentries == 0)
	{
		xdr_destroy(&n_xdr);
		lu_endnetent();
		return (NULL);
	}

	bzero(&lu_n, sizeof(lu_n));
	if (!xdr__lu_netent(&n_xdr, &lu_n))
	{
		xdr_destroy(&n_xdr);
		lu_endnetent();
		return (NULL);
	}

	n_nentries--;
	convert_n(&lu_n);
	xdr_free(xdr__lu_netent, &lu_n);
	return (&global_n);
}

struct netent *
getnetbyaddr(long addr, int type)
{
    struct netent *res;

    if (_lu_running())
	{
		res = lu_getnetbyaddr(addr, type);
    }
	else
	{
		res = _res_getnetbyaddr(addr, type);
		if (res == NULL) res = _old_getnetbyaddr(addr, type);
    }

    return res;
}

struct netent *
getnetbyname(const char *name)
{
    struct netent *res;

    if (_lu_running())
	{
		res = lu_getnetbyname(name);
    }
	else
	{
		res = _res_getnetbyname(name);
		if (res == NULL) res = _old_getnetbyname(name);
    }

    return res;
}

struct netent *
getnetent(void)
{
	GETENT(lu_getnetent, _old_getnetent, &n_state, struct netent);
}

void
setnetent(int stayopen)
{
	SETSTATE(lu_setnetent, _old_setnetent, &n_state, stayopen);
}

void
endnetent(void)
{
	UNSETSTATE(lu_endnetent, _old_endnetent, &n_state);
}
