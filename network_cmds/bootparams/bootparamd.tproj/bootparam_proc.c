/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
 * BOOTPARAMS server
 * Copyright 1998, Apple Computer Inc.  Unpublished.
 *
 * Written by Marc Majka
 */

#include <rpc/rpc.h>
#include "bootparam_prot.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <bootparams.h>
#include <syslog.h>

extern int debug;
extern unsigned long route_addr;
extern char domain_name[];

struct hostent *h;
static char hostname[MAXHOSTNAMELEN];

bp_whoami_res *
bootparamproc_whoami_1_svc(whoami, req)
bp_whoami_arg *whoami;
struct svc_req *req;
{
	long haddr;
	static bp_whoami_res res;
	struct bootparamsent *bp;

	if (debug)
	{
		fprintf(stderr,"whoami %d.%d.%d.%d\n", 
			255 & whoami->client_address.bp_address_u.ip_addr.net,
			255 & whoami->client_address.bp_address_u.ip_addr.host,
			255 & whoami->client_address.bp_address_u.ip_addr.lh,
			255 & whoami->client_address.bp_address_u.ip_addr.impno);
	}

	bcopy((char *)&whoami->client_address.bp_address_u.ip_addr,
		(char *)&haddr, sizeof(haddr));
	h = gethostbyaddr((char *)&haddr, sizeof(haddr), AF_INET);
	if (h == NULL)
	{
		if (debug) fprintf(stderr,"whoami failed: gethostbyaddr\n");
		return NULL;
	}

	/* check whether subsequent bpgetfile requests would succeed */
	bp = bootparams_getbyname(h->h_name);
	if (bp == NULL) {
	    	if (debug) 
		    fprintf(stderr, "whoami failed: bootparams_getbyname\n");
		return NULL;
	}

	sprintf(hostname, "%s", h->h_name);
	res.client_name = hostname;

	res.domain_name = domain_name;

	res.router_address.address_type = IP_ADDR_TYPE;
	bcopy(&route_addr, &res.router_address.bp_address_u.ip_addr, 4);

	if (debug)
	{
		fprintf(stderr, "whoami name=%s domain=%s router=%d.%d.%d.%d\n", 
			res.client_name,
			res.domain_name,
			255 & res.router_address.bp_address_u.ip_addr.net,
			255 & res.router_address.bp_address_u.ip_addr.host,
			255 & res.router_address.bp_address_u.ip_addr.lh,
			255 & res.router_address.bp_address_u.ip_addr.impno);
	}

	return (&res);
}

bp_getfile_res *
bootparamproc_getfile_1_svc(getfile, req)
bp_getfile_arg *getfile;
struct svc_req *req;
{
	static bp_getfile_res res;
	struct bootparamsent *bp;
	static char s[1024];
	char *p;
	int i, len;

	if (debug)
	{
		fprintf(stderr, "getfile %s %s\n",
			getfile->client_name, getfile->file_id);
	}

	bp = bootparams_getbyname(getfile->client_name);
	if (bp == NULL)
	{
		if (debug)
		{
			fprintf(stderr, "can't find bootparams for %s\n",
				getfile->client_name);
			fprintf(stderr, "getfile failed\n");
		}
		return NULL;
	}

	len = strlen(getfile->file_id) + 1;
	sprintf(s, "%s=", getfile->file_id);

	for (i = 0; bp->bp_bootparams[i] != NULL; i++)
	{
		if (!strncmp(s, bp->bp_bootparams[i], len)) break;
	}

	if (bp->bp_bootparams[i] == NULL)
	{
		if (debug)
		{
			fprintf(stderr, "can't find bootparam %s\n", getfile->file_id);
			fprintf(stderr, "getfile failed\n");
		}
		return NULL;
	}

	sprintf(s, bp->bp_bootparams[i] + len);
	p = strchr(s, ':');
	if (p == NULL)
	{
		hostname[0] = '\0';
		res.server_name = hostname;
		res.server_address.bp_address_u.ip_addr.net = 0;
		res.server_address.bp_address_u.ip_addr.host = 0;
		res.server_address.bp_address_u.ip_addr.lh = 0;
		res.server_address.bp_address_u.ip_addr.impno = 0;
		res.server_address.address_type = 1;
		res.server_path = s;

		if (debug)
		{
     		 fprintf(stderr, "getfile server=%s (%d.%d.%d.%d) path=%s\n",
	  		   res.server_name,
	  		   255 & res.server_address.bp_address_u.ip_addr.net,
	  		   255 & res.server_address.bp_address_u.ip_addr.host,
	   		  255 & res.server_address.bp_address_u.ip_addr.lh,
	   		  255 & res.server_address.bp_address_u.ip_addr.impno,
				res.server_path);
		}
		return (&res);
	}

	*p = '\0';
	p++;
	h = gethostbyname(s);
	if (h == NULL)
	{
		if (debug)
		{
			fprintf(stderr, "can't find server %s\n", s);
			fprintf(stderr, "getfile failed\n");
		}
		return NULL;
	}

 	res.server_name = s;
	res.server_path = p;
	bcopy(h->h_addr, &res.server_address.bp_address_u.ip_addr, h->h_length);
	res.server_address.address_type = IP_ADDR_TYPE;

    if (debug)
	{
      fprintf(stderr, "getfile server=%s (%d.%d.%d.%d) path=%s\n",
	     res.server_name,
	     255 & res.server_address.bp_address_u.ip_addr.net,
	     255 & res.server_address.bp_address_u.ip_addr.host,
	     255 & res.server_address.bp_address_u.ip_addr.lh,
	     255 & res.server_address.bp_address_u.ip_addr.impno,
		res.server_path);
	}

    return (&res);
}
