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
 * Unix group lookup
 * Copyright (C) 1989 by NeXT, Inc.
 */

#include <stdlib.h>
#include <mach/mach.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <grp.h>
#include <netinet/in.h>
#include <sys/param.h>
#include <unistd.h>

#include "_lu_types.h"
#include "lookup.h"
#include "lu_utils.h"
#include "lu_overrides.h"

#define GROUP_SENTINEL	-99

static lookup_state gr_state = LOOKUP_CACHE;
static struct group global_gr;
static int global_free = 1;
static char *gr_data;
static unsigned gr_datalen = 0;
static int gr_nentries = 0;
static int gr_start = 1;
static XDR gr_xdr;

static void 
freeold(void)
{
	char **mem;

	if (global_free == 1) return;

	free(global_gr.gr_name);
	global_gr.gr_name = NULL;

	free(global_gr.gr_passwd);
	global_gr.gr_passwd = NULL;

	mem = global_gr.gr_mem;
	if (mem != NULL)
	{
		while (*mem != NULL) free(*mem++);
		free(global_gr.gr_mem);
		global_gr.gr_mem = NULL;
	}
 
	global_free = 1;
}

static void
convert_gr(_lu_group *lu_gr)
{
	int i, len;

	freeold();

	global_gr.gr_name = strdup(lu_gr->gr_name);
	global_gr.gr_passwd = strdup(lu_gr->gr_passwd);
	global_gr.gr_gid = lu_gr->gr_gid;

	len = lu_gr->gr_mem.gr_mem_len;
	global_gr.gr_mem = (char **)malloc((len + 1) * sizeof(char *));

	for (i = 0; i < len; i++)
	{
		global_gr.gr_mem[i] = strdup(lu_gr->gr_mem.gr_mem_val[i]);
	}

	global_gr.gr_mem[len] = NULL;

	global_free = 0;
}

static struct group *
lu_getgrgid(int gid)
{
	unsigned datalen;
	_lu_group_ptr lu_gr;
	XDR xdr;
	static int proc = -1;
	unit lookup_buf[MAX_INLINE_UNITS];
	
	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "getgrgid", &proc) != KERN_SUCCESS)
		{
			return (NULL);
		}
	}

	gid = htonl(gid);
	datalen = MAX_INLINE_UNITS;

	if (_lookup_one(_lu_port, proc, (unit *)&gid, 1, lookup_buf, &datalen)
		!= KERN_SUCCESS)
	{
		return (NULL);
	}

	datalen *= BYTES_PER_XDR_UNIT;
	xdrmem_create(&xdr, lookup_buf, datalen, XDR_DECODE);
	lu_gr = NULL;

	if (!xdr__lu_group_ptr(&xdr, &lu_gr) || lu_gr == NULL)
	{
		xdr_destroy(&xdr);
		return (NULL);
	}

	xdr_destroy(&xdr);

	convert_gr(lu_gr);
	xdr_free(xdr__lu_group_ptr, &lu_gr);
	return (&global_gr);
}

static struct group *
lu_getgrnam(const char *name)
{
	unsigned datalen;
	char namebuf[_LU_MAXLUSTRLEN + BYTES_PER_XDR_UNIT];
	XDR outxdr;
	XDR inxdr;
	_lu_group_ptr lu_gr;
	static int proc = -1;
	unit lookup_buf[MAX_INLINE_UNITS];

	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "getgrnam", &proc) != KERN_SUCCESS)
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
		return (NULL);
	}

	xdr_destroy(&outxdr);

	datalen *= BYTES_PER_XDR_UNIT;
	xdrmem_create(&inxdr, lookup_buf, datalen,
		XDR_DECODE);
	lu_gr = NULL;

	if (!xdr__lu_group_ptr(&inxdr, &lu_gr) || (lu_gr == NULL))
	{
		xdr_destroy(&inxdr);
		return (NULL);
	}

	xdr_destroy(&inxdr);

	convert_gr(lu_gr);
	xdr_free(xdr__lu_group_ptr, &lu_gr);
	return (&global_gr);
}


static int
lu_initgroups(const char *name, int basegid)
{
	unsigned datalen;
	XDR outxdr;
	XDR inxdr;
	static int proc = -1;
	unit lookup_buf[MAX_INLINE_UNITS];
	char namebuf[_LU_MAXLUSTRLEN + BYTES_PER_XDR_UNIT];
	int groups[NGROUPS];
	int ngroups = 1;
	int a_group;
	int count;

	groups[0] = basegid;
	
	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "initgroups", &proc) != KERN_SUCCESS)
		{
			return -1;
		}
	}

	xdrmem_create(&outxdr, namebuf, sizeof(namebuf), XDR_ENCODE);
	if (!xdr__lu_string(&outxdr, &name))
	{
		xdr_destroy(&outxdr);
		return -1;
	}

	datalen = MAX_INLINE_UNITS;
	if (_lookup_one(_lu_port, proc, (unit *)namebuf,
		xdr_getpos(&outxdr) / BYTES_PER_XDR_UNIT, lookup_buf, &datalen)
		!= KERN_SUCCESS)
	{
		xdr_destroy(&outxdr);
		return -1;
	}

	xdr_destroy(&outxdr);

	datalen *= BYTES_PER_XDR_UNIT;
	xdrmem_create(&inxdr, lookup_buf, datalen,
		XDR_DECODE);

	while (xdr_int(&inxdr, &a_group))
	{
		if (a_group == GROUP_SENTINEL) break;

		for (count = 0; count < ngroups; count++)
		{
			if (groups[count] == a_group) break;
		}

		if (count >= ngroups) groups[ngroups++] = a_group;
	}
	xdr_destroy(&inxdr);
	
	return setgroups(ngroups, groups);
}

static void
lu_endgrent(void)
{
	gr_nentries = 0;
	if (gr_data != NULL)
	{
		freeold();
		vm_deallocate(mach_task_self(), (vm_address_t)gr_data, gr_datalen);
		gr_data = NULL;
	}
}

static int
lu_setgrent(void)
{
	lu_endgrent();
	gr_start = 1;
	return (1);
}

static struct group *
lu_getgrent()
{
	static int proc = -1;
	_lu_group lu_gr;

	if (gr_start == 1)
	{
		gr_start = 0;

		if (proc < 0)
		{
			if (_lookup_link(_lu_port, "getgrent", &proc) != KERN_SUCCESS)
			{
				lu_endgrent();
				return (NULL);
			}
		}

		if (_lookup_all(_lu_port, proc, NULL, 0, &gr_data, &gr_datalen)
			!= KERN_SUCCESS)
		{
			lu_endgrent();
			return (NULL);
		}

#ifdef NOTDEF
/* NOTDEF because OOL buffers are counted in bytes with untyped IPC */
		gr_datalen *= BYTES_PER_XDR_UNIT;
#endif
		xdrmem_create(&gr_xdr, gr_data, gr_datalen,
			XDR_DECODE);
		if (!xdr_int(&gr_xdr, &gr_nentries))
		{
			xdr_destroy(&gr_xdr);
			lu_endgrent();
			return (NULL);
		}
	}

	if (gr_nentries == 0)
	{
		xdr_destroy(&gr_xdr);
		lu_endgrent();
		return (NULL);
	}

	bzero(&lu_gr, sizeof(lu_gr));
	if (!xdr__lu_group(&gr_xdr, &lu_gr))
	{
		xdr_destroy(&gr_xdr);
		lu_endgrent();
		return (NULL);
	}

	gr_nentries--;
	convert_gr(&lu_gr);
	xdr_free(xdr__lu_group, &lu_gr);
	return (&global_gr);
}

struct group *
getgrgid(gid_t gid)
{
	LOOKUP1(lu_getgrgid, _old_getgrgid, gid, struct group);
}

struct group *
getgrnam(const char *name)
{
	LOOKUP1(lu_getgrnam, _old_getgrnam, name, struct group);
}

int
initgroups(const char *name, int basegid)
{
	int res;

	if (name == NULL) return -1;

	if (_lu_running())
	{
		if ((res = lu_initgroups(name, basegid)))
		{
			res = _old_initgroups(name, basegid);
		}
	}
	else
	{
		res = _old_initgroups(name, basegid);
	}

	return (res);
}

struct group *
getgrent(void)
{
	GETENT(lu_getgrent, _old_getgrent, &gr_state, struct group);
}

int
setgrent(void)
{
	INTSETSTATEVOID(lu_setgrent, _old_setgrent, &gr_state);
}

void
endgrent(void)
{
	UNSETSTATE(lu_endgrent, _old_endgrent, &gr_state);
}
