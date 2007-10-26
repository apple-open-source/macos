/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1992, 1993 Theo de Raadt <deraadt@fsa.ca>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Theo de Raadt.
 * 4. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef LINT
__unused static char rcsid[] = "$Id: ypwhich.c,v 1.2 2006/02/07 06:23:03 lindak Exp $";
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <ctype.h>
#include <netdb.h>
#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpcsvc/yp.h>
//#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

extern bool_t xdr_domainname();

struct ypalias {
	char *alias, *name;
} ypaliases[] = {
	{ "passwd", "passwd.byname" },
	{ "group", "group.byname" },
	{ "networks", "networks.byaddr" },
	{ "hosts", "hosts.byaddr" },
	{ "protocols", "protocols.bynumber" },
	{ "services", "services.byname" },
	{ "aliases", "mail.aliases" },
	{ "ethers", "ethers.byname" },
};
static int n_aliases = 8;

void
usage()
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "\typwhich [-d domain] [[-t] -m [mname] | host]\n");
	fprintf(stderr, "\typwhich -x\n");
	exit(1);
}


/*
 * Like yp_bind except can query a specific host
 */
int
bind_host(char *dom, struct sockaddr_in *sin)
{
	struct hostent *hent = NULL;
	struct ypbind_resp ypbr;
	struct timeval tv;
	CLIENT *client;
	int sock, r;
	struct in_addr addr;

	sock = RPC_ANYSOCK;
	tv.tv_sec = 15;
	tv.tv_usec = 0;
	client = clntudp_create(sin, YPBINDPROG, YPBINDVERS, tv, &sock);
	if (client==NULL)
	{
		fprintf(stderr, "can't clntudp_create: %s\n",
			yperr_string(YPERR_YPBIND));
		return YPERR_YPBIND;
	}

	tv.tv_sec = 5;
	tv.tv_usec = 0;
	r = clnt_call(client, YPBINDPROC_DOMAIN,
		(xdrproc_t)xdr_domainname, &dom, (xdrproc_t)xdr_ypbind_resp, &ypbr, tv);

	if (r != RPC_SUCCESS)
	{
		fprintf(stderr, "can't clnt_call: %s\n", yperr_string(YPERR_YPBIND));
		clnt_destroy(client);
		return YPERR_YPBIND;
	}
	else
	{
		if (ypbr.ypbind_status != YPBIND_SUCC_VAL)
		{
			fprintf(stderr, "can't yp_bind: Reason: %s\n",
				yperr_string(ypbr.ypbind_status));
			clnt_destroy(client);
			return r;
		}
	}
	
	clnt_destroy(client);

	memmove(&addr, ypbr.ypbind_resp_u.ypbind_bindinfo.ypbind_binding_addr, 4);

	hent = gethostbyaddr((char *)&(addr.s_addr), sizeof(u_long), AF_INET);
	if (hent != NULL) printf("%s\n", hent->h_name);
	else printf("%s\n", inet_ntoa(addr));
	return 0;
}
	
int
main(int argc, char *argv[])
{
	char *domainname, *master, *map;
	ypmaplist *ypml, *y;
	extern char *optarg;
	extern int optind;
	struct hostent *hent;
	struct sockaddr_in sin;
	int notrans, mode, getmap;
	int c, r, i;

	yp_get_default_domain(&domainname);

	map = NULL;
	getmap = notrans = mode = 0;
	while ((c = getopt(argc, argv, "xd:mt")) != -1)
	{
		switch(c)
		{
			case 'x':
				for (i = 0; i < n_aliases; i++)
				{
					printf("Use \"%s\" for \"%s\"\n",
						ypaliases[i].alias,
						ypaliases[i].name);
				}
				exit(0);

			case 'd':
				domainname = optarg;
				break;

			case 't':
				notrans++;
				break;

			case 'm':
				mode++;
				break;

			default:
				usage();
		}
	}

	if (mode == 0)
	{
		switch(argc-optind)
		{
			case 0:
				bzero(&sin, sizeof sin);
				sin.sin_family = AF_INET;
				sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

				if (bind_host(domainname, &sin)) exit(1);
				break;

			case 1:
				bzero(&sin, sizeof sin);
				sin.sin_family = AF_INET;
				sin.sin_addr.s_addr = inet_addr(argv[optind]);
				if (sin.sin_addr.s_addr == -1)
				{
					hent = gethostbyname(argv[optind]);
					if (hent == NULL)
					{
						fprintf(stderr, "ypwhich: host %s unknown\n",
							argv[optind]);
						exit(1);
					}

					bcopy((char *)hent->h_addr_list[0],
						(char *)&sin.sin_addr, sizeof sin.sin_addr);
				}

				if (bind_host(domainname, &sin)) exit(1);
				break;
				
			default:
				usage();
		}
		
		exit(0);
	}

	if (argc-optind > 1) usage();

	if (argv[optind])
	{
		map = argv[optind];
		for (i = 0; (!notrans) && (i < n_aliases); i++)
		{
			if (!strcmp(map, ypaliases[i].alias)) map = ypaliases[i].name;
		}
		
		r = yp_master(domainname, map, &master);
		switch(r)
		{
			case 0:
				printf("%s\n", master);
				free(master);
				break;
			case YPERR_YPBIND:
				fprintf(stderr, "ypwhich: not running ypbind\n");
				exit(1);
			default:
				fprintf(stderr, "Can't find master for map %s. Reason: %s\n",
					map, yperr_string(r));
				exit(1);
			}
			exit(0);
		}

		ypml = NULL;
		r = yp_maplist(domainname, &ypml);
		switch(r)
		{
			case 0:
				while (ypml != NULL)
				{
					r = yp_master(domainname, ypml->map, &master);
					switch(r)
					{
						case 0:
							printf("%s %s\n", ypml->map, master);
							free(master);
							break;
						default:
							fprintf(stderr,
								"YP: can't find the master of %s: Reason: %s\n",
								ypml->map, yperr_string(r));
							break;
					}

					y = ypml;
					ypml = ypml->next;
					free(y);
				}
				break;

			case YPERR_YPBIND:
				fprintf(stderr, "ypwhich: not running ypbind\n");
				exit(1);
				
			default:
				fprintf(stderr, "Can't get map list for domain %s. Reason: %s\n",
					domainname, yperr_string(r));
				exit(1);
		}

	exit(0);
}
