/*
 * Copyright (c) 1999-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Bootp lookup - netinfo only
 * Copyright (C) 1989 by NeXT, Inc.
 */
#include <mach/mach.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <net/ethernet.h>
#include <pthread.h>

#include "_lu_types.h"
#include "lookup.h"
#include "lu_utils.h"

extern struct ether_addr *ether_aton(char *);

static pthread_mutex_t _bootp_lock = PTHREAD_MUTEX_INITIALIZER;

struct bootpent
{
	char *b_name;
	struct ether_addr b_enaddr;
	struct in_addr b_ipaddr;
	char *b_bootfile;
};

static void
free_bootp(struct bootpent *b)
{
	if (b == NULL) return;

	if (b->b_name != NULL) free(b->b_name);
	if (b->b_bootfile != NULL) free(b->b_bootfile);

	free(b);
}

static struct bootpent *
extract_bootp(XDR *xdr)
{
	struct bootpent *b;
	struct ether_addr *e;
	int i, j, nvals, nkeys, status;
	char *key, **vals;

	if (xdr == NULL) return NULL;

	if (!xdr_int(xdr, &nkeys)) return NULL;

	b = (struct bootpent *)calloc(1, sizeof(struct bootpent));

	for (i = 0; i < nkeys; i++)
	{
		key = NULL;
		vals = NULL;
		nvals = 0;

		status = _lu_xdr_attribute(xdr, &key, &vals, &nvals);
		if (status < 0)
		{
			free_bootp(b);
			return NULL;
		}

		if (nvals == 0)
		{
			free(key);
			continue;
		}

		j = 0;

		if ((b->b_name == NULL) && (!strcmp("name", key)))
		{
			b->b_name = vals[0];
			j = 1;
		}
		if ((b->b_name == NULL) && (!strcmp("bootfile", key)))
		{
			b->b_bootfile = vals[0];
			j = 1;
		}
		else if (!strcmp("ip_address", key))
		{
			b->b_ipaddr.s_addr = inet_addr(vals[0]);
		}
		else if (!strcmp("en_address", key))
		{
			pthread_mutex_lock(&_bootp_lock);
			e = ether_aton(vals[0]);
			if (e != NULL) memcpy(&(b->b_enaddr), e, sizeof(struct ether_addr));
			pthread_mutex_unlock(&_bootp_lock);
			j = 1;
		}

		free(key);
		if (vals != NULL)
		{
			for (; j < nvals; j++) free(vals[j]);
			free(vals);
		}
	}

	if (b->b_name == NULL) b->b_name = strdup("");
	if (b->b_bootfile == NULL) b->b_bootfile = strdup("");

	return b;
}

static int 
lu_bootp_getbyether(struct ether_addr *enaddr, char **name,
	struct in_addr *ipaddr, char **bootfile)
{
	unsigned datalen;
	XDR inxdr;
	struct bootpent *b;
	static int proc = -1;
	char *lookup_buf;
	int count;

	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "bootp_getbyether", &proc) != KERN_SUCCESS)
		{
			return 0;
		}
	}

	datalen = 0;
	lookup_buf = NULL;

	if (_lookup_all(_lu_port, proc, (unit *)enaddr, ((sizeof(*enaddr) + sizeof(unit) - 1) / sizeof(unit)), &lookup_buf, &datalen) != KERN_SUCCESS)
	{
		return 0;
	}

	datalen *= BYTES_PER_XDR_UNIT;
	if ((lookup_buf == NULL) || (datalen == 0)) return NULL;

	xdrmem_create(&inxdr, lookup_buf, datalen, XDR_DECODE);

	count = 0;
	if (!xdr_int(&inxdr, &count))
	{
		xdr_destroy(&inxdr);
		vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);
		return 0;
	}

	if (count == 0)
	{
		xdr_destroy(&inxdr);
		vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);
		return 0;
	}

	b = extract_bootp(&inxdr);
	xdr_destroy(&inxdr);

	*name = b->b_name;
	*bootfile = b->b_bootfile;
	ipaddr->s_addr = b->b_ipaddr.s_addr;

	free(b);
	vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);

	return 1;
}

static int 
lu_bootp_getbyip(struct ether_addr *enaddr, char **name,
	struct in_addr *ipaddr, char **bootfile)
{
	unsigned datalen;
	XDR inxdr;
	struct bootpent *b;
	static int proc = -1;
	char *lookup_buf;
	int count;
	
	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "bootp_getbyip", &proc) != KERN_SUCCESS)
		{
			return 0;
		}
	}

	datalen = 0;
	lookup_buf = NULL;

	if (_lookup_all(_lu_port, proc, (unit *)ipaddr, ((sizeof(*ipaddr) + sizeof(unit) - 1) / sizeof(unit)), &lookup_buf, &datalen) != KERN_SUCCESS)
	{
		return 0;
	}

	datalen *= BYTES_PER_XDR_UNIT;
	if ((lookup_buf == NULL) || (datalen == 0)) return NULL;

	xdrmem_create(&inxdr, lookup_buf, datalen, XDR_DECODE);

	count = 0;
	if (!xdr_int(&inxdr, &count))
	{
		xdr_destroy(&inxdr);
		vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);
		return 0;
	}

	if (count == 0)
	{
		xdr_destroy(&inxdr);
		vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);
		return 0;
	}

	b = extract_bootp(&inxdr);
	xdr_destroy(&inxdr);

	*name = b->b_name;
	*bootfile = b->b_bootfile;
	memcpy(enaddr, &(b->b_enaddr), sizeof(struct ether_addr));

	free(b);
	vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);

	return 1;
}

int
bootp_getbyether(struct ether_addr *enaddr, char **name,struct in_addr *ipaddr, char **bootfile)
{
	if (_lu_running())
	{
		return (lu_bootp_getbyether(enaddr, name, ipaddr, bootfile));
	}
	return 0;
}

int
bootp_getbyip(struct ether_addr *enaddr, char **name, struct in_addr *ipaddr, char **bootfile)
{
	if (_lu_running())
	{
		return (lu_bootp_getbyip(enaddr, name, ipaddr, bootfile));
	}
	return 0;
}

