/*
 * Copyright (c) 1999-2002 Apple Computer, Inc. All rights reserved.
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
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <pthread.h>

#include "_lu_types.h"
#include "lookup.h"
#include "printerdb.h"
#include "lu_utils.h"

static pthread_mutex_t _printer_lock = PTHREAD_MUTEX_INITIALIZER;

#define P_GET_NAME 1
#define P_GET_ENT 2

extern prdb_ent *_old_prdb_get();
extern prdb_ent *_old_prdb_getbyname();
extern void _old_prdb_set();
extern void _old_prdb_end();

static void 
free_printer_data(prdb_ent *p)
{
	char **names;
	int i;

	if (p == NULL) return;

	names = p->pe_name;
	if (names != NULL)
	{
		while (*names) free(*names++);
		free(p->pe_name);
	}

	for (i = 0; i < p->pe_nprops; i++)
	{
		free(p->pe_prop[i].pp_key);
		free(p->pe_prop[i].pp_value);
	}

	free(p->pe_prop);
}

static void 
free_printer(prdb_ent *p)
{
	if (p == NULL) return;
	free_printer_data(p);
	free(p);
}

static void
free_lu_thread_info_printer(void *x)
{
	struct lu_thread_info *tdata;

	if (x == NULL) return;

	tdata = (struct lu_thread_info *)x;
	
	if (tdata->lu_entry != NULL)
	{
		free_printer((prdb_ent *)tdata->lu_entry);
		tdata->lu_entry = NULL;
	}

	_lu_data_free_vm_xdr(tdata);

	free(tdata);
}

static prdb_ent *
extract_printer(XDR *xdr)
{
	prdb_ent *p;
	int i, j, nvals, nkeys, status;
	char *key, **vals;

	if (xdr == NULL) return NULL;

	if (!xdr_int(xdr, &nkeys)) return NULL;

	p = (prdb_ent *)calloc(1, sizeof(prdb_ent));

	for (i = 0; i < nkeys; i++)
	{
		key = NULL;
		vals = NULL;
		nvals = 0;

		status = _lu_xdr_attribute(xdr, &key, &vals, &nvals);
		if (status < 0)
		{
			free_printer(p);
			return NULL;
		}

		j = 0;

		if ((p->pe_name == NULL) && (!strcmp("name", key)))
		{
			free(key);
			p->pe_name = vals;
			j = nvals;
			vals = NULL;
		}
		else
		{
			if (p->pe_nprops == 0)
			{
				p->pe_prop = (prdb_property *)calloc(1, sizeof(prdb_property));
			}
			else
			{
				p->pe_prop = (prdb_property *)realloc(p->pe_prop, (p->pe_nprops + 1) * sizeof(prdb_property));
			}
			p->pe_prop[p->pe_nprops].pp_key = key;
			p->pe_prop[p->pe_nprops].pp_value = NULL;
			if (nvals > 0)
			{
				p->pe_prop[p->pe_nprops].pp_value = vals[0];
				j = 1;
			}
			else
			{
				p->pe_prop[p->pe_nprops].pp_value = strdup("");
			}
			p->pe_nprops++;
		}
	
		if (vals != NULL)
		{
			for (; j < nvals; j++) free(vals[j]);
			free(vals);
		}
	}

	if (p->pe_name == NULL) p->pe_name = (char **)calloc(1, sizeof(char *));
	if (p->pe_prop == NULL) p->pe_prop = (prdb_property *)calloc(1, sizeof(prdb_property));

	return p;
}

static prdb_ent *
copy_printer(prdb_ent *in)
{
	int i;
	prdb_ent *p;

	if (in == NULL) return NULL;

	p = (prdb_ent *)calloc(1, sizeof(prdb_ent));

	if (in->pe_name != NULL)
	{
		for (i = 0; in->pe_name[i] != NULL; i++);	
		p->pe_name = (char **)calloc(i, sizeof(char *));
		for (i = 0; p->pe_name[i] != NULL; i++) p->pe_name[i] = strdup(in->pe_name[i]);
	}
	else
	{
		p->pe_name = (char **)calloc(1, sizeof(char *));
	}

	if (in->pe_nprops > 0)
	{
		p->pe_prop = (struct prdb_property *)calloc(in->pe_nprops, sizeof(struct prdb_property));
		
		for (i = 0; in->pe_nprops; i++)
		{
			p->pe_prop[i].pp_key = strdup(in->pe_prop[i].pp_key);
			p->pe_prop[i].pp_value = NULL;
			if (in->pe_prop[i].pp_value != NULL) p->pe_prop[i].pp_value = strdup(in->pe_prop[i].pp_value);
		}
	}
	else
	{
		p->pe_prop = (prdb_property *)calloc(1, sizeof(prdb_property));
	}

	return p;
}

static void
recycle_printer(struct lu_thread_info *tdata, struct prdb_ent *in)
{
	struct prdb_ent *p;

	if (tdata == NULL) return;
	p = (struct prdb_ent *)tdata->lu_entry;

	if (in == NULL)
	{
		free_printer(p);
		tdata->lu_entry = NULL;
	}

	if (tdata->lu_entry == NULL)
	{
		tdata->lu_entry = in;
		return;
	}

	free_printer_data(p);

	p->pe_name = in->pe_name;
	p->pe_nprops = in->pe_nprops;
	p->pe_prop = in->pe_prop;

	free(in);
}

static prdb_ent *
lu_prdb_getbyname(const char *name)
{
	prdb_ent *p;
	unsigned datalen;
	char namebuf[_LU_MAXLUSTRLEN + BYTES_PER_XDR_UNIT];
	XDR outxdr;
	XDR inxdr;
	static int proc = -1;
	char *lookup_buf;
	int count;

	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "prdb_getbyname", &proc) != KERN_SUCCESS)
		{
			return NULL;
		}
	}

	xdrmem_create(&outxdr, namebuf, sizeof(namebuf), XDR_ENCODE);
	if (!xdr__lu_string(&outxdr, (_lu_string *)&name))
	{
		xdr_destroy(&outxdr);
		return NULL;
	}
	
	datalen = 0;
	lookup_buf = NULL;

	if (_lookup_all(_lu_port, proc, (unit *)namebuf, xdr_getpos(&outxdr) / BYTES_PER_XDR_UNIT, &lookup_buf, &datalen) != KERN_SUCCESS)
	{
		xdr_destroy(&outxdr);
		return NULL;
	}

	xdr_destroy(&outxdr);

	datalen *= BYTES_PER_XDR_UNIT;
	if ((lookup_buf == NULL) || (datalen == 0)) return NULL;

	xdrmem_create(&inxdr, lookup_buf, datalen, XDR_DECODE);

	count = 0;
	if (!xdr_int(&inxdr, &count))
	{
		xdr_destroy(&inxdr);
		vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);
		return NULL;
	}

	if (count == 0)
	{
		xdr_destroy(&inxdr);
		vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);
		return NULL;
	}

	p = extract_printer(&inxdr);
	xdr_destroy(&inxdr);
	vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);

	return p;
}

static void
lu_prdb_end()
{
	struct lu_thread_info *tdata;

	tdata = _lu_data_create_key(_lu_data_key_printer, free_lu_thread_info_printer);
	_lu_data_free_vm_xdr(tdata);
}

static void
lu_prdb_set()
{
	lu_prdb_end();
}

static prdb_ent *
lu_prdb_get()
{
	prdb_ent *p;
	static int proc = -1;
	struct lu_thread_info *tdata;

	tdata = _lu_data_create_key(_lu_data_key_printer, free_lu_thread_info_printer);
	if (tdata == NULL)
	{
		tdata = (struct lu_thread_info *)calloc(1, sizeof(struct lu_thread_info));
		_lu_data_set_key(_lu_data_key_printer, tdata);
	}

	if (tdata->lu_vm == NULL)
	{
		if (proc < 0)
		{
			if (_lookup_link(_lu_port, "prdb_get", &proc) != KERN_SUCCESS)
			{
				lu_prdb_end();
				return NULL;
			}
		}

		if (_lookup_all(_lu_port, proc, NULL, 0, &(tdata->lu_vm), &(tdata->lu_vm_length)) != KERN_SUCCESS)
		{
			lu_prdb_end();
			return NULL;
		}

		/* mig stubs measure size in words (4 bytes) */
		tdata->lu_vm_length *= 4;

		if (tdata->lu_xdr != NULL)
		{
			xdr_destroy(tdata->lu_xdr);
			free(tdata->lu_xdr);
		}
		tdata->lu_xdr = (XDR *)calloc(1, sizeof(XDR));

		xdrmem_create(tdata->lu_xdr, tdata->lu_vm, tdata->lu_vm_length, XDR_DECODE);
		if (!xdr_int(tdata->lu_xdr, &tdata->lu_vm_cursor))
		{
			lu_prdb_end();
			return NULL;
		}
	}

	if (tdata->lu_vm_cursor == 0)
	{
		lu_prdb_end();
		return NULL;
	}

	p = extract_printer(tdata->lu_xdr);
	if (p == NULL)
	{
		lu_prdb_end();
		return NULL;
	}

	tdata->lu_vm_cursor--;
	
	return p;
}

static prdb_ent *
getprinter(const char *name, int source)
{
	prdb_ent *res = NULL;
	struct lu_thread_info *tdata;

	tdata = _lu_data_create_key(_lu_data_key_printer, free_lu_thread_info_printer);
	if (tdata == NULL)
	{
		tdata = (struct lu_thread_info *)calloc(1, sizeof(struct lu_thread_info));
		_lu_data_set_key(_lu_data_key_printer, tdata);
	}

	if (_lu_running())
	{
		switch (source)
		{
			case P_GET_NAME:
				res = lu_prdb_getbyname(name);
				break;
			case P_GET_ENT:
				res = lu_prdb_get();
				break;
			default: res = NULL;
		}
	}
	else
	{
		pthread_mutex_lock(&_printer_lock);
		switch (source)
		{
			case P_GET_NAME:
				res = copy_printer(_old_prdb_getbyname(name));
				break;
			case P_GET_ENT:
				res = copy_printer(_old_prdb_get());
				break;
			default: res = NULL;
		}
		pthread_mutex_unlock(&_printer_lock);
	}

	recycle_printer(tdata, res);
	return (prdb_ent *)tdata->lu_entry;
}

const prdb_ent *
prdb_getbyname(const char *name)
{
	return (const prdb_ent *)getprinter(name, P_GET_NAME);
}

const prdb_ent *
prdb_get(void)
{
	return (const prdb_ent *)getprinter(NULL, P_GET_ENT);
}

void
prdb_set(const char *name)
{
	if (_lu_running()) lu_prdb_set();
	else _old_prdb_set();
}

void
prdb_end(void)
{
	if (_lu_running()) lu_prdb_end();
	else _old_prdb_end();
}
