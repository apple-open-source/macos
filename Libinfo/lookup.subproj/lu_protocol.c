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
 * Protocol lookup
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
#import <netinet/in.h>

extern struct protoent *_old_getprotobynumber();
extern struct protoent *_old_getprotobyname();
extern struct protoent *_old_getprotoent();
extern void _old_setprotoent();
extern void _old_endprotoent();

static lookup_state p_state = LOOKUP_CACHE;
static struct protoent global_p;
static int global_free = 1;
static char *p_data = NULL;
static unsigned p_datalen;
static int p_nentries;
static int p_start;
static XDR p_xdr;

static void
freeold(void)
{
	char **aliases;

	if (global_free == 1) return;

	free(global_p.p_name);
	aliases = global_p.p_aliases;
	if (aliases != NULL)
	{
		while (*aliases != NULL) free(*aliases++);
		free(global_p.p_aliases);
	}

	global_free = 1;
}

static void
convert_p(_lu_protoent *lu_p)
{
	int i, len;

	freeold();

	global_p.p_name = strdup(lu_p->p_names.p_names_val[0]);

	len = lu_p->p_names.p_names_len - 1;
	global_p.p_aliases = (char **)malloc((len + 1) * sizeof(char *));

	for (i = 0; i < len; i++)
	{
		global_p.p_aliases[i] = strdup(lu_p->p_names.p_names_val[i+1]);
	}

	global_p.p_aliases[len] = NULL;

	global_p.p_proto = lu_p->p_proto;

	global_free = 0;
}

static struct protoent *
lu_getprotobynumber(long number)
{
	unsigned datalen;
	_lu_protoent_ptr lu_p;
	XDR xdr;
	static int proc = -1;
	unit lookup_buf[MAX_INLINE_UNITS];
	
	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "getprotobynumber", &proc) != KERN_SUCCESS)
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
	xdrmem_create(&xdr, lookup_buf, datalen,
		      XDR_DECODE);
	lu_p = NULL;
	if (!xdr__lu_protoent_ptr(&xdr, &lu_p) || (lu_p == NULL))
	{
		xdr_destroy(&xdr);
		return (NULL);
	}

	xdr_destroy(&xdr);

	convert_p(lu_p);
	xdr_free(xdr__lu_protoent_ptr, &lu_p);
	return (&global_p);
}

static struct protoent *
lu_getprotobyname(const char *name)
{
	unsigned datalen;
	char namebuf[_LU_MAXLUSTRLEN + BYTES_PER_XDR_UNIT];
	XDR outxdr;
	XDR inxdr;
	_lu_protoent_ptr lu_p;
	static int proc = -1;
	unit lookup_buf[MAX_INLINE_UNITS];

	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "getprotobyname", &proc) != KERN_SUCCESS)
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
	lu_p = NULL;
	if (!xdr__lu_protoent_ptr(&inxdr, &lu_p) || (lu_p == NULL))
	{
		xdr_destroy(&inxdr);
		return (NULL);
	}

	xdr_destroy(&inxdr);

	convert_p(lu_p);
	xdr_free(xdr__lu_protoent_ptr, &lu_p);
	return (&global_p);
}

static void
lu_endprotoent()
{
	p_nentries = 0;
	if (p_data != NULL)
	{
		freeold();
		vm_deallocate(mach_task_self(), (vm_address_t)p_data, p_datalen);
		p_data = NULL;
	}
}

static void
lu_setprotoent()
{
	lu_endprotoent();
	p_start = 1;
}

static struct protoent *
lu_getprotoent()
{
	static int proc = -1;
	_lu_protoent lu_p;

	if (p_start == 1)
	{
		p_start = 0;

		if (proc < 0)
		{
			if (_lookup_link(_lu_port, "getprotoent", &proc) != KERN_SUCCESS)
			{
				lu_endprotoent();
				return (NULL);
			}
		}

		if (_lookup_all(_lu_port, proc, NULL, 0, &p_data, &p_datalen)
			!= KERN_SUCCESS)
		{
			lu_endprotoent();
			return (NULL);
		}

#ifdef NOTDEF 
/* NOTDEF because OOL buffers are counted in bytes with untyped IPC */  
		p_datalen *= BYTES_PER_XDR_UNIT;
#endif
		xdrmem_create(&p_xdr, p_data, p_datalen,
			XDR_DECODE);
		if (!xdr_int(&p_xdr, &p_nentries))
		{
			xdr_destroy(&p_xdr);
			lu_endprotoent();
			return (NULL);
		}
	}

	if (p_nentries == 0)
	{
		xdr_destroy(&p_xdr);
		lu_endprotoent();
		return (NULL);
	}

	bzero(&lu_p, sizeof(lu_p));
	if (!xdr__lu_protoent(&p_xdr, &lu_p))
	{
		xdr_destroy(&p_xdr);
		lu_endprotoent();
		return (NULL);
	}

	p_nentries--;
	convert_p(&lu_p);
	xdr_free(xdr__lu_protoent, &lu_p);
	return (&global_p);
}

struct protoent *
getprotobynumber(int number)
{
	LOOKUP1(lu_getprotobynumber, _old_getprotobynumber, number,
		struct protoent);
}

struct protoent *
getprotobyname(const char *name)
{
	LOOKUP1(lu_getprotobyname, _old_getprotobyname,  name, struct protoent);
}

struct protoent *
getprotoent(void)
{
	GETENT(lu_getprotoent, _old_getprotoent, &p_state, struct protoent);
}

void
setprotoent(int stayopen)
{
	SETSTATE(lu_setprotoent, _old_setprotoent, &p_state, stayopen);
}

void
endprotoent(void)
{
	UNSETSTATE(lu_endprotoent, _old_endprotoent, &p_state);
}
