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
 * Printer lookup
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
#include "printerdb.h"
#include "lu_utils.h"

extern struct prdb_ent *_old_prdb_get();
extern struct prdb_ent *_old_prdb_getbyname();
extern void _old_prdb_set();
extern void _old_prdb_end();

static lookup_state prdb_state = LOOKUP_CACHE;
static struct prdb_ent global_prdb;
static int global_free = 1;
static char *prdb_data = NULL;
static unsigned prdb_datalen = 0;
static int prdb_nentries;
static int prdb_start = 1;
static XDR prdb_xdr = { 0 };

static void 
freeold(void)
{
	char **names;
	int i;

	if (global_free == 1) return;

	names = global_prdb.pe_name;
	if (names != NULL)
	{
		while (*names) free(*names++);
		free(global_prdb.pe_name);
	}

	for (i = 0; i < global_prdb.pe_nprops; i++)
	{
		free(global_prdb.pe_prop[i].pp_key);
		free(global_prdb.pe_prop[i].pp_value);
	}

	free(global_prdb.pe_prop);

	global_free = 1;
}


static void
convert_prdb(_lu_prdb_ent *lu_prdb)
{
	int i, len;

	freeold();

	len = lu_prdb->pe_names.pe_names_len;
	global_prdb.pe_name = (char **)malloc((len + 1) * sizeof(char *));
	for (i = 0; i < len; i++)
	{
		global_prdb.pe_name[i] = strdup(lu_prdb->pe_names.pe_names_val[i]);
	}

	global_prdb.pe_name[len] = NULL;

	len = lu_prdb->pe_props.pe_props_len;
	global_prdb.pe_prop = (prdb_property *)malloc(len * sizeof(prdb_property));
	for (i = 0; i < len; i++)
	{
		global_prdb.pe_prop[i].pp_key =
			strdup(lu_prdb->pe_props.pe_props_val[i].pp_key);

		global_prdb.pe_prop[i].pp_value =
			strdup(lu_prdb->pe_props.pe_props_val[i].pp_value);
	}

	global_prdb.pe_nprops = lu_prdb->pe_props.pe_props_len;

	global_free = 0;
}

static void
lu_prdb_end()
{
	prdb_nentries = 0;
	if (prdb_data != NULL)
	{
		freeold();
		vm_deallocate(mach_task_self(), (vm_address_t)prdb_data, prdb_datalen);
		prdb_data = NULL;
	}
}

static void
lu_prdb_set()
{
	lu_prdb_end();
	prdb_start = 1;
}

static struct prdb_ent *
lu_prdb_getbyname(const char *name)
{
	unsigned datalen;
	char namebuf[_LU_MAXLUSTRLEN + BYTES_PER_XDR_UNIT];
	XDR outxdr;
	XDR inxdr;
	_lu_prdb_ent_ptr lu_prdb;
	static int proc = -1;
	unit lookup_buf[MAX_INLINE_UNITS];

	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "prdb_getbyname", &proc) != KERN_SUCCESS)
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
	lu_prdb = NULL;
	if (!xdr__lu_prdb_ent_ptr(&inxdr, &lu_prdb) || (lu_prdb == NULL))
	{
		xdr_destroy(&inxdr);
		return (NULL);
	}

	xdr_destroy(&inxdr);

	convert_prdb(lu_prdb);
	xdr_free(xdr__lu_prdb_ent_ptr, &lu_prdb);
	return (&global_prdb);
}

static prdb_ent *
lu_prdb_get()
{
	static int proc = -1;
	_lu_prdb_ent lu_prdb;

	if (prdb_start == 1)
	{
		prdb_start = 0;

		if (proc < 0)
		{
			if (_lookup_link(_lu_port, "prdb_get", &proc) != KERN_SUCCESS)
			{
				lu_prdb_end();
				return (NULL);
			}
		}
		if (_lookup_all(_lu_port, proc, NULL, 0, &prdb_data, &prdb_datalen)
			!= KERN_SUCCESS)
		{
			lu_prdb_end();
			return (NULL);
		}

#ifdef NOTDEF
/* NOTDEF because OOL buffers are counted in bytes with untyped IPC */
		prdb_datalen *= BYTES_PER_XDR_UNIT;
#endif

		xdrmem_create(&prdb_xdr, prdb_data, prdb_datalen,
			XDR_DECODE);
		if (!xdr_int(&prdb_xdr, &prdb_nentries))
		{
			xdr_destroy(&prdb_xdr);
			lu_prdb_end();
			return (NULL);
		}
	}

	if (prdb_nentries == 0)
	{
		xdr_destroy(&prdb_xdr);
		lu_prdb_end();
		return (NULL);
	}

	bzero(&lu_prdb, sizeof(lu_prdb));
	if (!xdr__lu_prdb_ent(&prdb_xdr, &lu_prdb))
	{
		xdr_destroy(&prdb_xdr);
		lu_prdb_end();
		return (NULL);
	}

	prdb_nentries--;
	convert_prdb(&lu_prdb);
	xdr_free(xdr__lu_prdb_ent, &lu_prdb);
	return (&global_prdb);
}

const prdb_ent *
prdb_getbyname(const char *name)
{
	LOOKUP1(lu_prdb_getbyname, _old_prdb_getbyname, name, prdb_ent);
}

const prdb_ent *
prdb_get(void)
{
	GETENT(lu_prdb_get, _old_prdb_get, &prdb_state, prdb_ent);
}

void
prdb_set(const char *name)
{
	SETSTATE(lu_prdb_set, _old_prdb_set, &prdb_state, name);
}

void
prdb_end(void)
{
	UNSETSTATE(lu_prdb_end, _old_prdb_end, &prdb_state);
}
