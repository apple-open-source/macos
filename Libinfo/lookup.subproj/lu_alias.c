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

#include "lookup.h"
#include "_lu_types.h"
#include "lu_utils.h"
#include "lu_overrides.h"

static lookup_state alias_state = LOOKUP_CACHE;
static struct aliasent global_aliasent;
static int global_free = 1;
static char *alias_data = NULL;
static unsigned alias_datalen;
static int alias_nentries = 0;
static int alias_start = 1;
static XDR alias_xdr;

static void 
freeold(void)
{
	int i, len;

	if (global_free == 1) return;

	free(global_aliasent.alias_name);

	len = global_aliasent.alias_members_len;
	for (i = 0; i < len; i++)
		free(global_aliasent.alias_members[i]);

	free(global_aliasent.alias_members);

	global_free = 1;
}

static void
convert_aliasent(_lu_aliasent *lu_aliasent)
{
	int i, len;

	freeold();

	global_aliasent.alias_name = strdup(lu_aliasent->alias_name);

	len = lu_aliasent->alias_members.alias_members_len;
	global_aliasent.alias_members_len = len;
	global_aliasent.alias_members = (char **)malloc(len * sizeof(char *));

	for (i = 0; i < len; i++)
	{
		global_aliasent.alias_members[i] =
			strdup(lu_aliasent->alias_members.alias_members_val[i]);
	}

	global_aliasent.alias_local = lu_aliasent->alias_local;

	global_free = 0;
}

static struct aliasent *
lu_alias_getbyname(const char *name)
{
	unsigned datalen;
	char namebuf[_LU_MAXLUSTRLEN + BYTES_PER_XDR_UNIT];
	unit lookup_buf[MAX_INLINE_UNITS];
	XDR outxdr;
	XDR inxdr;
	_lu_aliasent_ptr lu_aliasent;
	static int proc = -1;

	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "alias_getbyname", &proc) != KERN_SUCCESS)
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
	lu_aliasent = NULL;
	if (!xdr__lu_aliasent_ptr(&inxdr, &lu_aliasent) || (lu_aliasent == NULL))
	{
		xdr_destroy(&inxdr);
		return (NULL);
	}

	xdr_destroy(&inxdr);

	convert_aliasent(lu_aliasent);
	xdr_free(xdr__lu_aliasent_ptr, &lu_aliasent);
	return (&global_aliasent);
}

static void
lu_alias_endent(void)
{
	alias_nentries = 0;
	if (alias_data != NULL)
	{
		freeold();
		vm_deallocate(mach_task_self(), (vm_address_t)alias_data, alias_datalen);
		alias_data = NULL;
	}
}

static void
lu_alias_setent(void)
{
	lu_alias_endent();
	alias_start = 1;
}

static struct aliasent *
lu_alias_getent(void)
{
	static int proc = -1;
	_lu_aliasent lu_aliasent;

	if (alias_start == 1)
	{
		alias_start = 0;

		if (proc < 0)
		{
			if (_lookup_link(_lu_port, "alias_getent", &proc) != KERN_SUCCESS)
			{
				lu_alias_endent();
				return (NULL);
			}
		}

		if (_lookup_all(_lu_port, proc, NULL, 0, &alias_data, &alias_datalen)
			!= KERN_SUCCESS)
		{
			lu_alias_endent();
			return (NULL);
		}

#ifdef NOTDEF
/* NOTDEF because OOL buffers are counted in bytes with untyped IPC */
		alias_datalen *= BYTES_PER_XDR_UNIT;
#endif
		xdrmem_create(&alias_xdr, alias_data,
			alias_datalen, XDR_DECODE);
		if (!xdr_int(&alias_xdr, &alias_nentries))
		{
			xdr_destroy(&alias_xdr);
			lu_alias_endent();
			return (NULL);
		}
	}

	if (alias_nentries == 0)
	{
		xdr_destroy(&alias_xdr);
		lu_alias_endent();
		return (NULL);
	}

	bzero(&lu_aliasent, sizeof(lu_aliasent));
	if (!xdr__lu_aliasent(&alias_xdr, &lu_aliasent))
	{
		xdr_destroy(&alias_xdr);
		lu_alias_endent();
		return (NULL);
	}

	alias_nentries--;
	convert_aliasent(&lu_aliasent);
	xdr_free(xdr__lu_aliasent, &lu_aliasent);
	return (&global_aliasent);
}

struct aliasent *
alias_getbyname(const char *name)
{
	LOOKUP1(lu_alias_getbyname, _old_alias_getbyname, name, struct aliasent);
}

struct aliasent *
alias_getent(void)
{
	GETENT(lu_alias_getent, _old_alias_getent, &alias_state, struct aliasent);
}

void
alias_setent(void)
{
	SETSTATEVOID(lu_alias_setent, _old_alias_setent, &alias_state);
}

void
alias_endent(void)
{
	UNSETSTATE(lu_alias_endent, _old_alias_endent, &alias_state);
}
