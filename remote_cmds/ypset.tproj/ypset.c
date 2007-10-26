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
__unused static char rcsid[] = "ypset.c,v 1.3 1993/06/12 00:02:37 deraadt Exp";
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpcsvc/yp.h>
//#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#include <arpa/inet.h>

extern bool_t xdr_domainname();
extern int getrpcport(char *, int, int, int);

void
usage()
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "\typset [-h host ] [-d domain] server\n");
	exit(1);
}

int
bind_tohost(sin, dom, server)
struct sockaddr_in *sin;
char *dom, *server;
{
	ypbind_setdom ypsd;
	struct timeval tv;
	struct hostent *hp;
	CLIENT *client;
	int sock;
	u_short port;
	int r;
	unsigned long server_addr;
	
	if( (port=htons(getrpcport(server, YPPROG, YPPROC_NULL, IPPROTO_UDP))) == 0) {
		fprintf(stderr, "%s not running ypserv.\n", server);
		exit(1);
	}

	tv.tv_sec = 15;
	tv.tv_usec = 0;
	sock = RPC_ANYSOCK;
	client = clntudp_create(sin, YPBINDPROG, YPBINDVERS, tv, &sock);
	if (client==NULL) {
		fprintf(stderr, "RPC error: can't create YPBIND client\n");
		return YP_YPERR;
	}
	client->cl_auth = authunix_create_default();

	bzero(&ypsd, sizeof(struct ypbind_setdom));


	if( (hp = gethostbyname (server)) != NULL ) {
		/* is this the most compatible way?? */
		bcopy(hp->h_addr_list[0], &ypsd.ypsetdom_binding.ypbind_binding_addr, 4);
	} else if( (long)(server_addr = inet_addr (server)) == -1) {
		fprintf(stderr, "can't find address for %s\n", server);
		exit(1);
	} else
		bcopy (&server_addr, &ypsd.ypsetdom_binding.ypbind_binding_addr, 4);
		
	ypsd.ypsetdom_domain = dom;

	bcopy(&port, ypsd.ypsetdom_binding.ypbind_binding_port, 2);
	ypsd.ypsetdom_vers = YPVERS;
	
	r = clnt_call(client, YPBINDPROC_SETDOM,
		(xdrproc_t)xdr_ypbind_setdom, &ypsd, (xdrproc_t)xdr_void, NULL, tv);
	if (r != RPC_SUCCESS)
	{
		fprintf(stderr, "Can't ypset for domain %s: %s \n",
			dom, clnt_sperror(client, "setdomain"));
		clnt_destroy(client);
		return YP_YPERR;
	}
	clnt_destroy(client);
	return 0;
}

int
main(argc, argv)
char **argv;
{
	struct sockaddr_in sin;
	struct hostent *hent;
	extern char *optarg;
	extern int optind;
	char *domainname;
	int c;

	yp_get_default_domain(&domainname);

	bzero(&sin, sizeof sin);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(0x7f000001);

	while( (c=getopt(argc, argv, "h:d:")) != -1)
		switch(c) {
		case 'd':
			domainname = optarg;
			break;
		case 'h':
			if( (sin.sin_addr.s_addr=inet_addr(optarg)) == -1) {
				hent = gethostbyname(optarg);
				if(hent==NULL) {
					fprintf(stderr, "ypset: host %s unknown\n",
						optarg);
					exit(1);
				}
				bcopy(&hent->h_addr_list[0], &sin.sin_addr,
					sizeof sin.sin_addr);
			}
			break;
		default:
			usage();
		}

	if(optind + 1 != argc )
		usage();

	if (bind_tohost(&sin, domainname, argv[optind]))
		exit(1);
	exit(0);
}
