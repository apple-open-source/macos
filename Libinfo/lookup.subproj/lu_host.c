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
 * host lookup
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

extern struct hostent *_res_gethostbyaddr();
extern struct hostent *_res_gethostbyname();
extern struct hostent *_old_gethostbyaddr();
extern struct hostent *_old_gethostbyname();
extern struct hostent *_old_gethostent();
extern void _old_sethostent();
extern void _old_endhostent();
extern void _old_sethostfile();

extern int h_errno;

static lookup_state h_state = LOOKUP_CACHE;
/*
 * The static return value from get*ent functions
 */
static struct hostent global_h;
static int global_free = 1;
static char *h_data = NULL;
static unsigned h_datalen;
static int h_nentries;
static int h_start = 1;
static XDR h_xdr;

static void
freeold(void)
{
	char **aliases;
	int i;

	if (global_free == 1) return;

	free(global_h.h_name);
	aliases = global_h.h_aliases;
	if (aliases != NULL)
	{
		while (*aliases != NULL) free(*aliases++);
		free(global_h.h_aliases);
	}

	for (i = 0; global_h.h_addr_list[i] != NULL; i++)
		free(global_h.h_addr_list[i]);

	free(global_h.h_addr_list);

	global_free = 1;
}

static void
convert_h(_lu_hostent *lu_h)
{
	int i, len, addr_len;

	freeold();

	global_h.h_name = strdup(lu_h->h_names.h_names_val[0]);

	len = lu_h->h_names.h_names_len - 1;
	global_h.h_aliases = (char **)malloc((len + 1) * sizeof(char *));

	for (i = 0; i < len; i++)
	{
		global_h.h_aliases[i] = strdup(lu_h->h_names.h_names_val[i + 1]);
	}

	global_h.h_aliases[len] = NULL;

	global_h.h_addrtype = AF_INET;
	global_h.h_length = sizeof(long);

	len = lu_h->h_addrs.h_addrs_len;
	addr_len = sizeof(u_long *);

	global_h.h_addr_list = (char **)malloc((len + 1) * addr_len);

	for (i = 0; i < len; i++)
	{
		global_h.h_addr_list[i] = (char *)malloc(sizeof(long));
		bcopy((const void *)&(lu_h->h_addrs.h_addrs_val[i]),
			(void *)global_h.h_addr_list[i], sizeof(long));
	}

	global_h.h_addr_list[len] = NULL;

	global_free = 0;
}

static struct hostent *
lu_gethostbyaddr(const char *addr, int len, int type)
{
	unsigned datalen;
	_lu_hostent_ptr lu_h;
	XDR xdr;
	long address;
	static int proc = -1;
	unit lookup_buf[MAX_INLINE_UNITS];
	
	if (len != sizeof(long) || (type != AF_INET)) 
	{
		h_errno = HOST_NOT_FOUND;
		return (NULL);
	}

	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "gethostbyaddr", &proc) != KERN_SUCCESS)
		{
			h_errno = HOST_NOT_FOUND;
			return (NULL);
		}
	}

	bcopy(addr, &address, sizeof(address));
	address = htonl(address);
	datalen = MAX_INLINE_UNITS;
	if (_lookup_one(_lu_port, proc, (unit *)&address, 1, lookup_buf, &datalen)
		!= KERN_SUCCESS)
	{
		h_errno = HOST_NOT_FOUND;
		return (NULL);
	}

	datalen *= BYTES_PER_XDR_UNIT;
	xdrmem_create(&xdr, lookup_buf, datalen, XDR_DECODE);
	lu_h = NULL;
	h_errno = HOST_NOT_FOUND;
	if (!xdr__lu_hostent_ptr(&xdr, &lu_h) || 
	    !xdr_int(&xdr, &h_errno) || (lu_h == NULL))
	{
		xdr_destroy(&xdr);
		return (NULL);
	}

	xdr_destroy(&xdr);

	convert_h(lu_h);
	xdr_free(xdr__lu_hostent_ptr, &lu_h);
	return (&global_h);
}

static struct hostent *
lu_gethostbyname(const char *name)
{
	unsigned datalen;
	char namebuf[_LU_MAXLUSTRLEN + BYTES_PER_XDR_UNIT];
	XDR outxdr;
	XDR inxdr;
	_lu_hostent_ptr lu_h;
	static int proc = -1;
	unit lookup_buf[MAX_INLINE_UNITS];

	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "gethostbyname", &proc) != KERN_SUCCESS)
		{
			h_errno = HOST_NOT_FOUND;
			return (NULL);
		}
	}

	xdrmem_create(&outxdr, namebuf, sizeof(namebuf), XDR_ENCODE);
	if (!xdr__lu_string(&outxdr, &name))
	{
		xdr_destroy(&outxdr);
		h_errno = HOST_NOT_FOUND;
		return (NULL);
	}

	datalen = MAX_INLINE_UNITS;
	if (_lookup_one(_lu_port, proc, (unit *)namebuf,
		xdr_getpos(&outxdr) / BYTES_PER_XDR_UNIT, lookup_buf, &datalen)
		!= KERN_SUCCESS)
	{
		xdr_destroy(&outxdr);
		h_errno = HOST_NOT_FOUND;
		return (NULL);
	}

	xdr_destroy(&outxdr);

	datalen *= BYTES_PER_XDR_UNIT;
	xdrmem_create(&inxdr, lookup_buf, datalen,
		XDR_DECODE);
	lu_h = NULL;
	h_errno = HOST_NOT_FOUND;
	if (!xdr__lu_hostent_ptr(&inxdr, &lu_h) || 
	    !xdr_int(&inxdr, &h_errno) || (lu_h == NULL))
	{
		xdr_destroy(&inxdr);
		return (NULL);
	}

	xdr_destroy(&inxdr);

	convert_h(lu_h);
	xdr_free(xdr__lu_hostent_ptr, &lu_h);
	return (&global_h);
}

static void
lu_endhostent()
{
	h_nentries = 0;
	if (h_data != NULL)
	{
		freeold();
		vm_deallocate(mach_task_self(), (vm_address_t)h_data, h_datalen);
		h_data = NULL;
	}
}

static void
lu_sethostent()
{
	lu_endhostent();
	h_start = 1;
}

static struct hostent *
lu_gethostent()
{
	static int proc = -1;
	_lu_hostent lu_h;

	if (h_start == 1)
	{
		h_start = 0;

		if (proc < 0)
		{
			if (_lookup_link(_lu_port, "gethostent", &proc) != KERN_SUCCESS)
			{
				lu_endhostent();
				return (NULL);
			}
		}

		if (_lookup_all(_lu_port, proc, NULL, 0, &h_data, &h_datalen)
			!= KERN_SUCCESS)
		{
			lu_endhostent();
			return (NULL);
		}

#ifdef NOTDEF
/* NOTDEF because OOL buffers are counted in bytes with untyped IPC */
		h_datalen *= BYTES_PER_XDR_UNIT;
#endif
		xdrmem_create(&h_xdr, h_data, h_datalen,
			XDR_DECODE);
		if (!xdr_int(&h_xdr, &h_nentries))
		{
			xdr_destroy(&h_xdr);
			lu_endhostent();
			return (NULL);
		}
	}

	if (h_nentries == 0)
	{
		xdr_destroy(&h_xdr);
		lu_endhostent();
		return (NULL);
	}

	bzero(&lu_h, sizeof(lu_h));
	if (!xdr__lu_hostent(&h_xdr, &lu_h))
	{
		xdr_destroy(&h_xdr);
		lu_endhostent();
		return (NULL);
	}

	h_nentries--;
	convert_h(&lu_h);
	xdr_free(xdr__lu_hostent, &lu_h);
	return (&global_h);
}

struct hostent *
gethostbyaddr(const char *addr, int len, int type)
{
	struct hostent *res;

	if (_lu_running())
	{
	    res = lu_gethostbyaddr(addr, len, type);
	}
	else
	{
	    res = _res_gethostbyaddr(addr, len, type);
	    if (res == NULL) res = _old_gethostbyaddr(addr, len, type);
	}

	return (res);
}

struct hostent *
gethostbyname(const char *name)
{
    struct hostent *res;
	struct in_addr addr;

	if (_lu_running())
	{
		res = lu_gethostbyname(name);
    }
	else
	{
		res = _res_gethostbyname(name);
		if (res == NULL) res = _old_gethostbyname(name);
	}

	if (res == NULL)
	{
		if (inet_aton(name, &addr) == 0) return NULL;
		return gethostbyaddr((char *)&addr, sizeof(addr), AF_INET);
	}

    return res;
}

struct hostent *
gethostent(void)
{
	GETENT(lu_gethostent, _old_gethostent, &h_state, struct hostent);
}

void
sethostent(int stayopen)
{
	SETSTATE(lu_sethostent, _old_sethostent, &h_state, stayopen);
}

void
endhostent(void)
{
	UNSETSTATE(lu_endhostent, _old_endhostent, &h_state);
}
