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
 * Bootparams lookup - netinfo only
 * Copyright (C) 1989 by NeXT, Inc.
 */
#include <stdlib.h>
#include <mach/mach.h>
#include <stdio.h>
#include <string.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <bootparams.h>

#include "lookup.h"
#include "_lu_types.h"
#include "lu_utils.h"

static lookup_state bp_state = LOOKUP_CACHE;
static struct bootparamsent global_bp;
static int global_free = 1;
static char *bp_data = NULL;
static unsigned bp_datalen;
static int bp_nentries;
static int bp_start = 1;
static XDR bp_xdr;

static void 
freeold(void)
{
	int i;
	if (global_free == 1) return;

	free(global_bp.bp_name);

	for (i = 0; global_bp.bp_bootparams[i] != NULL; i++)
		free(global_bp.bp_bootparams[i]);

	global_free = 1;
}

static void
convert_bootparamsent(_lu_bootparams_ent *lu_bpent)
{
	int i, len;

	freeold();

	global_bp.bp_name = strdup(lu_bpent->bootparams_name);

	len = lu_bpent->bootparams_keyvalues.bootparams_keyvalues_len;
	global_bp.bp_bootparams = (char **)malloc((len + 1) * sizeof(char *));

	for (i = 0; i < len; i++)
	{
		global_bp.bp_bootparams[i] =
			strdup(lu_bpent->bootparams_keyvalues.bootparams_keyvalues_val[i]);
	}

	global_bp.bp_bootparams[len] = NULL;

	global_free = 0;
}

static struct bootparamsent *
lu_bootparams_getbyname(const char *name)
{
	unsigned datalen;
	char namebuf[_LU_MAXLUSTRLEN + BYTES_PER_XDR_UNIT];
	unit lookup_buf[MAX_INLINE_UNITS];
	XDR outxdr;
	XDR inxdr;
	int size;
	_lu_bootparams_ent_ptr lu_bpent;
	static int proc = -1;
	
	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "bootparams_getbyname", &proc)
			!= KERN_SUCCESS)
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

	size = xdr_getpos(&outxdr);
	xdr_destroy(&outxdr);

	datalen = MAX_INLINE_UNITS;
	if (_lookup_one(_lu_port, proc, (unit *)namebuf, size, lookup_buf, 
		&datalen) != KERN_SUCCESS)
	{
		return (NULL);
	}

	datalen *= BYTES_PER_XDR_UNIT;
	xdrmem_create(&inxdr, lookup_buf, datalen,
		XDR_DECODE);
	lu_bpent = NULL;
	if (!xdr__lu_bootparams_ent_ptr(&inxdr, &lu_bpent) || (lu_bpent == NULL))
	{
		xdr_destroy(&inxdr);
		return (NULL);
	}

	xdr_destroy(&inxdr);

	convert_bootparamsent(lu_bpent);
	xdr_free(xdr__lu_bootparams_ent_ptr, &lu_bpent);
	return (&global_bp);
}

static void
lu_bootparams_endent(void)
{
	bp_nentries = 0;
	if (bp_data != NULL)
	{
		freeold();
		vm_deallocate(mach_task_self(), (vm_address_t)bp_data, bp_datalen);
		bp_data = NULL;
	}
}

static void
lu_bootparams_setent(void)
{
	lu_bootparams_endent();
	bp_start = 1;
}

static struct bootparamsent *
lu_bootparams_getent(void)
{
	static int proc = -1;
	_lu_bootparams_ent lu_bpent;

	if (bp_start == 1)
	{
		bp_start = 0;

		if (proc < 0)
		{
			if (_lookup_link(_lu_port, "bootparams_getent", &proc)
				!= KERN_SUCCESS)
			{
				lu_bootparams_endent();
				return (NULL);
			}
		}

		if (_lookup_all(_lu_port, proc, NULL, 0, &bp_data, &bp_datalen)
			!= KERN_SUCCESS)
		{
			lu_bootparams_endent();
			return (NULL);
		}

#ifdef NOTDEF
/* NOTDEF because OOL buffers are counted in bytes with untyped IPC */
		bp_datalen *= BYTES_PER_XDR_UNIT;
#endif
		xdrmem_create(&bp_xdr, bp_data, bp_datalen,
			XDR_DECODE);
		if (!xdr_int(&bp_xdr, &bp_nentries))
		{
			xdr_destroy(&bp_xdr);
			lu_bootparams_endent();
			return (NULL);
		}
	}

	if (bp_nentries == 0)
	{
		xdr_destroy(&bp_xdr);
		lu_bootparams_endent();
		return (NULL);
	}

	bzero(&lu_bpent, sizeof(lu_bpent));
	if (!xdr__lu_bootparams_ent(&bp_xdr, &lu_bpent))
	{
		xdr_destroy(&bp_xdr);
		lu_bootparams_endent();
		return (NULL);
	}

	bp_nentries--;
	convert_bootparamsent(&lu_bpent);
	xdr_free(xdr__lu_bootparams_ent, &lu_bpent);
	return (&global_bp);
}

struct bootparamsent *
bootparams_getbyname(const char *name)
{
	if (_lu_running()) return (lu_bootparams_getbyname(name));
	return (NULL);
}

struct bootparamsent *
bootparams_getent(void)
{
	if (_lu_running()) return (lu_bootparams_getent());
	return (NULL);
}

void
bootparams_setent(void)
{
	if (_lu_running()) lu_bootparams_setent();
}

void
bootparams_endent(void)
{
	if (_lu_running()) lu_bootparams_endent();
}
