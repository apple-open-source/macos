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

#include "_lu_types.h"
#include "lookup.h"
#include "lu_utils.h"
#include "lu_overrides.h"

static lookup_state r_state = LOOKUP_CACHE;
static struct rpcent global_r;
static int global_free = 1;
static char *r_data = NULL;
static unsigned r_datalen;
static int r_nentries;
static int r_start = 1;
static XDR r_xdr;

static void
freeold(void)
{
	char **aliases;

	if (global_free == 1) return;

	free(global_r.r_name);

	aliases = global_r.r_aliases;
	if (aliases != NULL)
	{
		while (*aliases != NULL) free(*aliases++);
		free(global_r.r_aliases);
	}

	global_free = 1;
}

static void
convert_r(_lu_rpcent *lu_r)
{
	int i, len;

	freeold();

	global_r.r_name = strdup(lu_r->r_names.r_names_val[0]);

	len = lu_r->r_names.r_names_len - 1;
	global_r.r_aliases = (char **)malloc((len + 1) * sizeof(char *));

	for (i = 0; i < len; i++)
	{
		global_r.r_aliases[i] = strdup(lu_r->r_names.r_names_val[i+1]);
	}

	global_r.r_aliases[len] = NULL;

	global_r.r_number = lu_r->r_number;

	global_free = 0;
}



static struct rpcent *
lu_getrpcbynumber(long number)
{
	unsigned datalen;
	_lu_rpcent_ptr lu_r;
	XDR xdr;
	static int proc = -1;
	unit lookup_buf[MAX_INLINE_UNITS];
	
	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "getrpcbynumber", &proc) != KERN_SUCCESS)
		{
			return (NULL);
		}
	}

	number = htonl(number);
	datalen = MAX_INLINE_UNITS;
	if (_lookup_one(_lu_port, proc, (unit *)&number, 1, lookup_buf, &datalen)
		!= KERN_SUCCESS)
	{
		return (NULL);
	}

	datalen *= BYTES_PER_XDR_UNIT;
	xdrmem_create(&xdr, lookup_buf, datalen, XDR_DECODE);
	lu_r = NULL;
	if (!xdr__lu_rpcent_ptr(&xdr, &lu_r) || lu_r == NULL)
	{
		xdr_destroy(&xdr);
		return (NULL);
	}

	xdr_destroy(&xdr);

	convert_r(lu_r);
	xdr_free(xdr__lu_rpcent_ptr, &lu_r);
	return (&global_r);
}

static struct rpcent *
lu_getrpcbyname(const char *name)
{
	unsigned datalen;
	char namebuf[_LU_MAXLUSTRLEN + BYTES_PER_XDR_UNIT];
	XDR outxdr;
	XDR inxdr;
	_lu_rpcent_ptr lu_r;
	static int proc = -1;
	unit lookup_buf[MAX_INLINE_UNITS];

	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "getrpcbyname", &proc) != KERN_SUCCESS)
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
	lu_r = NULL;
	if (!xdr__lu_rpcent_ptr(&inxdr, &lu_r) || (lu_r == NULL))
	{
		xdr_destroy(&inxdr);
		return (NULL);
	}

	xdr_destroy(&inxdr);

	convert_r(lu_r);
	xdr_free(xdr__lu_rpcent_ptr, &lu_r);
	return (&global_r);
}

static void
lu_endrpcent(void)
{
	r_nentries = 0;
	if (r_data != NULL)
	{
		freeold();
		vm_deallocate(mach_task_self(), (vm_address_t)r_data, r_datalen);
		r_data = NULL;
	}
}

static int
lu_setrpcent(int stayopen)
{
	lu_endrpcent();
	r_start = 1;
	return (1);
}

static struct rpcent *
lu_getrpcent()
{
	static int proc = -1;
	_lu_rpcent lu_r;

	if (r_start == 1)
	{
		r_start = 0;

		if (proc < 0)
		{
			if (_lookup_link(_lu_port, "getrpcent", &proc) != KERN_SUCCESS)
			{
				lu_endrpcent();
				return (NULL);
			}
		}

		if (_lookup_all(_lu_port, proc, NULL, 0, &r_data, &r_datalen)
			!= KERN_SUCCESS)
		{
			lu_endrpcent();
			return (NULL);
		}

#ifdef NOTDEF
/* NOTDEF because OOL buffers are counted in bytes with untyped IPC */
		r_datalen *= BYTES_PER_XDR_UNIT;
#endif

		xdrmem_create(&r_xdr, r_data, r_datalen,
			XDR_DECODE);
		if (!xdr_int(&r_xdr, &r_nentries))
		{
			xdr_destroy(&r_xdr);
			lu_endrpcent();
			return (NULL);
		}
	}

	if (r_nentries == 0)
	{
		xdr_destroy(&r_xdr);
		lu_endrpcent();
		return (NULL);
	}

	bzero(&lu_r, sizeof(lu_r));
	if (!xdr__lu_rpcent(&r_xdr, &lu_r))
	{
		xdr_destroy(&r_xdr);
		lu_endrpcent();
		return (NULL);
	}

	r_nentries--;
	convert_r(&lu_r);
	xdr_free(xdr__lu_rpcent, &lu_r);
	return (&global_r);
}

struct rpcent *
getrpcbynumber(long number)
{
	LOOKUP1(lu_getrpcbynumber, _old_getrpcbynumber, number, struct rpcent);
}

struct rpcent *
getrpcbyname(const char *name)
{
	LOOKUP1(lu_getrpcbyname, _old_getrpcbyname, name, struct rpcent);
}

struct rpcent *
getrpcent(void)
{
	GETENT(lu_getrpcent, _old_getrpcent, &r_state, struct rpcent);
}

void
setrpcent(int stayopen)
{
	SETSTATE(lu_setrpcent, _old_setrpcent, &r_state, stayopen);
}

void
endrpcent(void)
{
	UNSETSTATE(lu_endrpcent, _old_endrpcent, &r_state);
}
