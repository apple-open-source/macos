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

#include <stdlib.h>
#include <string.h>
#include <mach/mach.h>

#include "_lu_types.h"
#include "lookup.h"
#include "lu_utils.h"

#define LONG_STRING_LENGTH 8192
#define _LU_MAXLUSTRLEN 256

static ni_proplist *
lookupd_process_dictionary(XDR *inxdr)
{
	int i, nkeys, j, nvals;
	char *key, *val;
	ni_proplist *l;

	if (!xdr_int(inxdr, &nkeys)) return NULL;

	l = (ni_proplist *)malloc(sizeof(ni_proplist));
	NI_INIT(l);

	l->ni_proplist_len = nkeys;
	l->ni_proplist_val = NULL;
	if (nkeys > 0)
	{
		i = nkeys * sizeof(ni_property);
		l->ni_proplist_val = (ni_property *)malloc(i);
		memset(l->ni_proplist_val, 0, i);
	}

	for (i = 0; i < nkeys; i++)
	{
		key = NULL;

		if (!xdr_string(inxdr, &key, LONG_STRING_LENGTH))
		{
			ni_proplist_free(l);
			return NULL;
		}

		l->ni_proplist_val[i].nip_name = key;
	
		if (!xdr_int(inxdr, &nvals))
		{
			ni_proplist_free(l);
			return NULL;
		}
	
		l->ni_proplist_val[i].nip_val.ni_namelist_len = nvals;
		if (nvals > 0)
		{
			j = nvals * sizeof(ni_name);
			l->ni_proplist_val[i].nip_val.ni_namelist_val = (ni_name *)malloc(j);
			memset(l->ni_proplist_val[i].nip_val.ni_namelist_val, 0 , j);
		}
		
		for (j = 0; j < nvals; j++)
		{
			val = NULL;
			if (!xdr_string(inxdr, &val, LONG_STRING_LENGTH))
			{
				ni_proplist_free(l);
				return NULL;
			}

			l->ni_proplist_val[i].nip_val.ni_namelist_val[j] = val;
		}
	}

	return l;
}

int
lookupd_query(ni_proplist *l, ni_proplist ***out)
{
	unsigned datalen;
	XDR outxdr;
	XDR inxdr;
	int proc;
	char *listbuf;
	char databuf[_LU_MAXLUSTRLEN * BYTES_PER_XDR_UNIT];
	int n, i, j, na;
	kern_return_t status;
	ni_property *p;

	if (l == NULL) return 0;
	if (out == NULL) return 0;

	if (_lu_port == NULL) return 0;

	status = _lookup_link(_lu_port, "query", &proc);
	if (status != KERN_SUCCESS) return 0;

	xdrmem_create(&outxdr, databuf, sizeof(databuf), XDR_ENCODE);

	na = l->ni_proplist_len;

	/* Encode attribute count */
	if (!xdr_int(&outxdr, &na))
	{
		xdr_destroy(&outxdr);
		return 0;
	}

	for (i = 0; i < l->ni_proplist_len; i++)
	{
		p = &(l->ni_proplist_val[i]);
		if (!xdr_string(&outxdr, &(p->nip_name), _LU_MAXLUSTRLEN))
		{
			xdr_destroy(&outxdr);
			return 0;
		}

		if (!xdr_int(&outxdr, &(p->nip_val.ni_namelist_len)))
		{
			xdr_destroy(&outxdr);
			return 0;
		}

		for (j = 0; j < p->nip_val.ni_namelist_len; j++)
		{
			if (!xdr_string(&outxdr, &(p->nip_val.ni_namelist_val[j]), _LU_MAXLUSTRLEN))
			{
				xdr_destroy(&outxdr);
				return 0;
			}
		}
	}

	listbuf = NULL;
	datalen = 0;

	n = xdr_getpos(&outxdr) / BYTES_PER_XDR_UNIT;
	status = _lookup_all(_lu_port, proc, (unit *)databuf, n, &listbuf, &datalen);
	if (status != KERN_SUCCESS)
	{
		xdr_destroy(&outxdr);
		return 0;
	}

	xdr_destroy(&outxdr);

#ifdef NOTDEF
/* NOTDEF because OOL buffers are counted in bytes with untyped IPC */
	datalen *= BYTES_PER_XDR_UNIT;
#endif

	xdrmem_create(&inxdr, listbuf, datalen, XDR_DECODE);

	if (!xdr_int(&inxdr, &n))
	{
		xdr_destroy(&inxdr);
		return 0;
	}

	if (n == 0)
	{
		xdr_destroy(&inxdr);
		return 0;
	}

	*out = (ni_proplist **)malloc(n * sizeof(ni_proplist *));

	for (i = 0; i < n; i++)
	{
		(*out)[i] = lookupd_process_dictionary(&inxdr);
	}

	xdr_destroy(&inxdr);

	vm_deallocate(mach_task_self(), (vm_address_t)listbuf, datalen);
	
	return n;
}

ni_proplist *
lookupd_make_query(char *cat, char *fmt, ...)
{
	va_list ap;
	char *arg, *f;
	int na, x;
	ni_proplist *l;
	ni_property *p;

	if (fmt == NULL) return NULL;
	if (fmt[0] != 'k') return NULL;

	l = (ni_proplist *)malloc(sizeof(ni_proplist));
	NI_INIT(l);

	na = 0;
	x = -1;

	if (cat != NULL)
	{
		l->ni_proplist_val = (ni_property *)malloc(sizeof(ni_property));
		p = &(l->ni_proplist_val[0]);
		arg = "_lookup_category";
		p->nip_name = strdup(arg);
		p->nip_val.ni_namelist_len = 1;
		p->nip_val.ni_namelist_val = (ni_name *)malloc(sizeof(ni_name));
		p->nip_val.ni_namelist_val[0] = strdup(cat);

		l->ni_proplist_len++;
		x++;
	}

	va_start(ap, fmt);
	for (f = fmt; *f != NULL; f++)
	{
		arg = va_arg(ap, char *);
		if (*f == 'k')
		{
			l->ni_proplist_val = (ni_property *)realloc(l->ni_proplist_val, (l->ni_proplist_len + 1) * sizeof(ni_property));

			p = &(l->ni_proplist_val[l->ni_proplist_len]);
			p->nip_name = strdup(arg);
			p->nip_val.ni_namelist_len = 0;
			p->nip_val.ni_namelist_val = NULL;

			l->ni_proplist_len++;
			x++;
		}
		else
		{
			p = &(l->ni_proplist_val[x]);
			if (p->nip_val.ni_namelist_len == 0)
			{
				p->nip_val.ni_namelist_val = (ni_name *)malloc(sizeof(ni_name));
			}
			else
			{
				p->nip_val.ni_namelist_val = (ni_name *)realloc(p->nip_val.ni_namelist_val, (p->nip_val.ni_namelist_len + 1) * sizeof(ni_name));
			}
			p->nip_val.ni_namelist_val[p->nip_val.ni_namelist_len] = strdup(arg);
			p->nip_val.ni_namelist_len++;
		}
	}
	va_end(ap);

	return l;
}

void
ni_property_merge(ni_property *a, ni_property *b)
{
	int i, j, addme;

	if (a == NULL) return;
	if (b == NULL) return;

	for (j = 0; j < b->nip_val.ni_namelist_len; j++)
	{
		addme = 1;
		for (i = 0; i < (a->nip_val.ni_namelist_len) && (addme == 1); i++)
		{
			if (!strcmp(a->nip_val.ni_namelist_val[i], b->nip_val.ni_namelist_val[j])) addme = 0;
		}

		if (addme == 1)
		{
			a->nip_val.ni_namelist_val = (ni_name *)realloc(a->nip_val.ni_namelist_val, (a->nip_val.ni_namelist_len + 1) * sizeof(ni_name));
			a->nip_val.ni_namelist_val[a->nip_val.ni_namelist_len] = strdup(b->nip_val.ni_namelist_val[j]);
			a->nip_val.ni_namelist_len++;
		}
	}
}

void
ni_proplist_merge(ni_proplist *a, ni_proplist *b)
{
	ni_index wa, wb;
	int addme;

	if (a == NULL) return;
	if (b == NULL) return;

	for (wb = 0; wb < b->ni_proplist_len; wb++)
	{
		addme = 1;
		for (wa = 0; (wa < a->ni_proplist_len) && (addme == 1) ; wa++)
		{
			if (!strcmp(a->ni_proplist_val[wa].nip_name, b->ni_proplist_val[wb].nip_name)) addme = 0;
		}
		if (addme == 1)
		{
			a->ni_proplist_val = (ni_property *)realloc(a->ni_proplist_val, (a->ni_proplist_len + 1) * sizeof(ni_property));
			a->ni_proplist_val[a->ni_proplist_len].nip_name = strdup(b->ni_proplist_val[wb].nip_name);
			a->ni_proplist_val[a->ni_proplist_len].nip_val.ni_namelist_len = 0;
			a->ni_proplist_val[a->ni_proplist_len].nip_val.ni_namelist_val = NULL;
			a->ni_proplist_len++;
		}
	}

	for (wb = 0; wb < b->ni_proplist_len; wb++)
	{
		for (wa = 0; wa < a->ni_proplist_len; wa++)
		{
			if (!strcmp(a->ni_proplist_val[wa].nip_name, b->ni_proplist_val[wb].nip_name))
			{
				ni_property_merge(&(a->ni_proplist_val[wa]), &(b->ni_proplist_val[wb]));
			}
		}
	}
}

