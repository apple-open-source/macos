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
 * Services file lookup
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

extern struct servent *_old_getservbyport();
extern struct servent *_old_getservbyname();
extern struct servent *_old_getservent();
extern void _old_setservent();
extern void _old_endservent();
extern void _old_setservfile();

static lookup_state s_state = LOOKUP_CACHE;
static struct servent global_s;
static int global_free = 1;
static char *s_data = NULL;
static unsigned s_datalen;
static int s_nentries;
static int s_start = 1;
static XDR s_xdr;

static void
freeold(void)
{
	char **aliases;

	if (global_free == 1) return;

	free(global_s.s_name);

	aliases = global_s.s_aliases;
	if (aliases != NULL)
	{
		while (*aliases != NULL) free(*aliases++);
		free(global_s.s_aliases);
	}

	global_free = 1;
}

static void
convert_s(_lu_servent *lu_s)
{
	int i, len;

	freeold();

	global_s.s_name = strdup(lu_s->s_names.s_names_val[0]);

	len = lu_s->s_names.s_names_len - 1;
	global_s.s_aliases = (char **)malloc((len + 1) * sizeof(char *));

	for (i = 0; i < len; i++)
	{
		global_s.s_aliases[i] = strdup(lu_s->s_names.s_names_val[i+1]);
	}

	global_s.s_aliases[len] = NULL;

	global_s.s_proto = lu_s->s_proto;
	global_s.s_port = lu_s->s_port;

	global_free = 0;
}

static struct servent *
lu_getservbyport(int port, const char *proto)
{
	unsigned datalen;
	_lu_servent_ptr lu_s;
	XDR xdr;
	static int proc = -1;
	char output_buf[_LU_MAXLUSTRLEN + 3 * BYTES_PER_XDR_UNIT];
	unit lookup_buf[MAX_INLINE_UNITS];
	XDR outxdr;

	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "getservbyport", &proc) != KERN_SUCCESS)
		{
			return (NULL);
		}
	}

	/* Encode NULL for xmission to lookupd. */
	if (!proto) proto = "";	

	xdrmem_create(&outxdr, output_buf, sizeof(output_buf), XDR_ENCODE);
	if (!xdr_int(&outxdr, &port) || !xdr__lu_string(&outxdr, &proto))
	{
		xdr_destroy(&outxdr);
		return (NULL);
	}

	datalen = MAX_INLINE_UNITS;
	if (_lookup_one(_lu_port, proc, (unit *)output_buf, 
		xdr_getpos(&outxdr) / BYTES_PER_XDR_UNIT,  lookup_buf, &datalen)
		!= KERN_SUCCESS)
	{
		xdr_destroy(&outxdr);
		return (NULL);
	}

	xdr_destroy(&outxdr);

	datalen *= BYTES_PER_XDR_UNIT;
	xdrmem_create(&xdr, lookup_buf, datalen, XDR_DECODE);
	lu_s = NULL;
	if (!xdr__lu_servent_ptr(&xdr, &lu_s) || (lu_s == NULL))
	{
		xdr_destroy(&xdr);
		return (NULL);
	}

	xdr_destroy(&xdr);

	convert_s(lu_s);
	xdr_free(xdr__lu_servent_ptr, &lu_s);
	return (&global_s);
}

static struct servent *
lu_getservbyname(const char *name, const char *proto)
{
	unsigned datalen;
	unit lookup_buf[MAX_INLINE_UNITS];
	char output_buf[2 * (_LU_MAXLUSTRLEN + BYTES_PER_XDR_UNIT)];
	XDR outxdr;
	XDR inxdr;
	_lu_servent_ptr lu_s;
	static int proc = -1;

	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "getservbyname", &proc) != KERN_SUCCESS)
		{
		    return (NULL);
		}
	}

	/* Encode NULL for xmission to lookupd. */
	if (!proto) proto = "";

	xdrmem_create(&outxdr, output_buf, sizeof(output_buf), XDR_ENCODE);
	if (!xdr__lu_string(&outxdr, &name) || !xdr__lu_string(&outxdr, &proto))
	{
		xdr_destroy(&outxdr);
		return (NULL);
	}

	datalen = MAX_INLINE_UNITS;
	if (_lookup_one(_lu_port, proc, (unit *)output_buf,
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
	lu_s = NULL;
	if (!xdr__lu_servent_ptr(&inxdr, &lu_s) || (lu_s == NULL))
	{
		xdr_destroy(&inxdr);
		return (NULL);
	}

	xdr_destroy(&inxdr);

	convert_s(lu_s);
	xdr_free(xdr__lu_servent_ptr, &lu_s);
	return (&global_s);
}

static void
lu_endservent()
{
	s_nentries = 0;
	if (s_data != NULL)
	{
		freeold();
		vm_deallocate(mach_task_self(), (vm_address_t)s_data, s_datalen);
		s_data = NULL;
	}
}

static void
lu_setservent()
{
	lu_endservent();
	s_start = 1;
}

static struct servent *
lu_getservent()
{
	static int proc = -1;
	_lu_servent lu_s;

	if (s_start == 1)
	{
		s_start = 0;

		if (proc < 0)
		{
			if (_lookup_link(_lu_port, "getservent", &proc) != KERN_SUCCESS)
			{
				lu_endservent();
				return (NULL);
			}
		}

		if (_lookup_all(_lu_port, proc, NULL, 0, &s_data, &s_datalen)
			!= KERN_SUCCESS)
		{
			lu_endservent();
			return (NULL);
		}

#ifdef NOTDEF
/* NOTDEF because OOL buffers are counted in bytes with untyped IPC */
		s_datalen *= BYTES_PER_XDR_UNIT;
#endif

		xdrmem_create(&s_xdr, s_data, s_datalen,
			XDR_DECODE);
		if (!xdr_int(&s_xdr, &s_nentries))
		{
			xdr_destroy(&s_xdr);
			lu_endservent();
			return (NULL);
		}
	}

	if (s_nentries == 0)
	{
		xdr_destroy(&s_xdr);
		lu_endservent();
		return (NULL);
	}

	bzero(&lu_s, sizeof(lu_s));
	if (!xdr__lu_servent(&s_xdr, &lu_s))
	{
		xdr_destroy(&s_xdr);
		lu_endservent();
		return (NULL);
	}

	s_nentries--;
	convert_s(&lu_s);
	xdr_free(xdr__lu_servent, &lu_s);
	return (&global_s);
}

struct servent *
getservbyport(int port, const char *proto)
{
	LOOKUP2(lu_getservbyport, _old_getservbyport, port, proto, struct servent);
}

struct servent *
getservbyname(const char *name, const char *proto)
{
	LOOKUP2(lu_getservbyname, _old_getservbyname, name, proto,
		struct servent);
}

struct servent *
getservent(void)
{
	GETENT(lu_getservent, _old_getservent, &s_state, struct servent);
}

void
setservent(int stayopen)
{
	SETSTATE(lu_setservent, _old_setservent, &s_state, stayopen);
}

void
endservent(void)
{
	UNSETSTATE(lu_endservent, _old_endservent, &s_state);
}
