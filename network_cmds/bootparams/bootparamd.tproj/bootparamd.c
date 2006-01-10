/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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

#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include "bootparam_prot.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int debug = 0;
char *progname;

unsigned long route_addr;
char domain_name[MAXHOSTNAMELEN];

int _rpcsvcdirty;
extern void bootparamprog_1();

extern int getdomainname(char*, int);

void
usage(void)
{
	fprintf(stderr, "usage: %s [-d] [-r router]\n", progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	SVCXPRT *transp;
	int i;
	struct hostent *h;
	char str[256];
	FILE *fp;
	
	progname = strrchr(argv[0],'/');
	if (progname) progname++;
	else progname = argv[0];

	route_addr = 0;

	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-d"))
		{
			debug = 1;
		}
		else if (!strcmp(argv[i], "-r"))
		{
			i++;
			if (i >= argc) usage();

			route_addr = inet_addr(argv[i]);
			if (route_addr == -1)
			{
				h = gethostbyname(argv[i]);
				if (h == NULL)
				{
					fprintf(stderr, "%s: Can't find host %s\n",
						progname, argv[i]);
					exit(1);
				}

				bcopy(h->h_addr, (char *)&route_addr, sizeof(route_addr));
			}
		}
		else usage();
	}

	if (route_addr == 0)
	{
		fp = popen("/usr/sbin/netstat -r -n", "r");
		while (NULL != fgets(str, 256, fp))
		{
			if (strncmp(str, "default ", 8)) continue;
			if (!strncmp(str + 17, "link", 4)) continue;
			route_addr = inet_addr(str+17);
			break;
		}
		pclose(fp);
	}

	domain_name[0] = '\0';
	if (getdomainname(domain_name, sizeof(domain_name)) != 0)
	    domain_name[0] = '\0';

	if (debug == 0) daemon(0,0);		

	openlog(progname, 0, LOG_DAEMON);
	pmap_unset(BOOTPARAMPROG, BOOTPARAMVERS);

	transp = svcudp_create(RPC_ANYSOCK);
	if (transp == NULL)
	{
		syslog(LOG_ERR, "Can't create udp service.");
		exit(1);
	}

	if (!svc_register(transp, BOOTPARAMPROG, BOOTPARAMVERS, bootparamprog_1, IPPROTO_UDP))
	{
		syslog(LOG_ERR, "Can't register service.");
		exit(1);
	}

	svc_run();
	
	syslog(LOG_ERR, "svc_run returned");
	exit(1);
}


