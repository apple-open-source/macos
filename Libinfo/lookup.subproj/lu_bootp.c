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
 * Bootp lookup - netinfo only
 * Copyright (C) 1989 by NeXT, Inc.
 */
#include <mach/mach.h>
#include <stdio.h>
#include <string.h>
#include "lookup.h"
#include <rpc/types.h>
#include <rpc/xdr.h>
#include "_lu_types.h"
#include "lu_utils.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/if_ether.h>

static int 
lu_bootp_getbyether(struct ether_addr *enaddr, char **name,
	struct in_addr *ipaddr, char **bootfile)
{
	unsigned datalen;
	XDR xdr;
	static _lu_bootp_ent_ptr bp;
	static int proc = -1;
	unit lookup_buf[MAX_INLINE_UNITS];
	
	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "bootp_getbyether", &proc) != KERN_SUCCESS)
		{
			return (0);
		}
	}

	datalen = MAX_INLINE_UNITS;
	if (_lookup_one(_lu_port, proc, (unit *)enaddr, 
		((sizeof(*enaddr) + sizeof(unit) - 1) / sizeof(unit)), lookup_buf, 
		&datalen) != KERN_SUCCESS)
	{
		return (0);
	}

	datalen *= BYTES_PER_XDR_UNIT;
	xdrmem_create(&xdr, lookup_buf, datalen, XDR_DECODE);
	xdr_free(xdr__lu_bootp_ent_ptr, &bp);
	if (!xdr__lu_bootp_ent_ptr(&xdr, &bp) || (bp == NULL))
	{
		xdr_destroy(&xdr);
		return (0);
	}

	xdr_destroy(&xdr);

	*name = bp->bootp_name;
	*bootfile = bp->bootp_bootfile;
	ipaddr->s_addr = bp->bootp_ipaddr;
	return (1);
}

static int 
lu_bootp_getbyip(struct ether_addr *enaddr, char **name,
	struct in_addr *ipaddr, char **bootfile)
{
	unsigned datalen;
	XDR xdr;
	static _lu_bootp_ent_ptr bp;
	static int proc = -1;
	unit lookup_buf[MAX_INLINE_UNITS];
	
	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "bootp_getbyip", &proc) != KERN_SUCCESS)
		{
			return (0);
		}
	}

	datalen = MAX_INLINE_UNITS;
	if (_lookup_one(_lu_port, proc, (unit *)ipaddr, 
		((sizeof(*ipaddr) + sizeof(unit) - 1) / sizeof(unit)), lookup_buf, 
		&datalen) != KERN_SUCCESS)
	{
		return (0);
	}

	datalen *= BYTES_PER_XDR_UNIT;
	xdrmem_create(&xdr, lookup_buf, datalen, XDR_DECODE);
	xdr_free(xdr__lu_bootp_ent_ptr, &bp);
	if (!xdr__lu_bootp_ent_ptr(&xdr, &bp) || (bp == NULL))
	{
		xdr_destroy(&xdr);
		return (0);
	}

	xdr_destroy(&xdr);

	*name = bp->bootp_name;
	*bootfile = bp->bootp_bootfile;
	bcopy(bp->bootp_enaddr, enaddr, sizeof(*enaddr));
	return (1);
}

int
bootp_getbyether(struct ether_addr *enaddr, char **name,
	struct in_addr *ipaddr, char **bootfile)
{
	if (_lu_running())
		return (lu_bootp_getbyether(enaddr, name, ipaddr, bootfile));
	return (0);
}

int
bootp_getbyip(struct ether_addr *enaddr, char **name,
	struct in_addr *ipaddr, char **bootfile)
{
	if (_lu_running())
		return (lu_bootp_getbyip(enaddr, name, ipaddr, bootfile));
	return (0);
}

