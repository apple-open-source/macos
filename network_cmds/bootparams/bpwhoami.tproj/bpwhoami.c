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
 * Copyright (c) 1997 Apple Computer, Inc. All Rights Reserved
 *
 * bpwhoami.c
 * - do a bootparams whoami call and echo the results to stdout
 *
 * The output is of the form:
 * host_name=<hostname>
 * nis_domain=<nis domain name>
 * router=<router ip address>
 * server_name=<server host name>
 * server_ip_address=<server ip address>
 *
 * The program will exit with the following codes:
 * 0	Successfully retrieved info
 * 1	RPC timed out while attempting to retrieve info
 * 2	Unrecoverable error ie. don't bother trying to call again
 *
 * Modification History:
 * Aug  5, 1997		Dieter Siegund (dieter@apple.com)
 * - lifted code from the old hostname -AUTOMATIC- source
 */


#include <sys/param.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void usage __P((void));

#include <netdb.h>
#include <signal.h>
#include <mach/boolean.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/ioctl_compat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <net/if.h>
#include <rpc/rpc.h>
#include <arpa/inet.h>
#include "bootparam_prot.h"

struct in_addr		ip_address;
struct in_addr		net_mask;

static __inline__
u_long iptohl(struct in_addr ip)
{
    return (ntohl(ip.s_addr));
}

static __inline__ boolean_t
in_subnet(struct in_addr netaddr, struct in_addr netmask, struct in_addr ip)
{
    if ((iptohl(ip) & iptohl(netmask)) 
	!= (iptohl(netaddr) & iptohl(netmask))) {
	return (FALSE);
    }
    return (TRUE);
}

bool_t
each_whoresult(result, from)
     bp_whoami_res		*result;
     struct sockaddr_in	*from;
{
	if (result) {
	    struct hostent *	host;
	    struct in_addr *	router;

	    /*
	     * guard against bogus router replies by making sure
	     * that the default router is on the same subnet
	     */
	    router = (struct in_addr *)
		&result->router_address.bp_address_u.ip_addr;
	    if (in_subnet(*router, net_mask, ip_address)) {
		if (result->client_name && result->client_name[0])
		    printf("host_name=%s\n", result->client_name); 
		if (result->domain_name && result->domain_name[0])
		    printf("nis_domain=%s\n", result->domain_name);
		printf("router=%s\n", inet_ntoa(*router));
		host = gethostbyaddr((char *) &from->sin_addr,
				     sizeof (from->sin_addr), AF_INET);
		if (host) {
		    printf("server_name=%s\n", host->h_name);
		}
		printf("server_ip_address=%s\n", inet_ntoa(from->sin_addr));
		return(TRUE);
	    }
	}
	return(FALSE);
}

#define MAX_IF		16

static boolean_t
getFirstInterface(struct ifreq * ret_p)
{
    struct ifconf 	ifconf;	/* points to ifreq */
    struct ifreq  *	ifreq = NULL;
    struct ifreq *	ifrp;
    int			size = sizeof(struct ifreq) * MAX_IF;
    int			sockfd;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	fprintf(stderr, "bpwhoami: socket call failed\n");
	return (FALSE);
    }

    while (1) {
	if (ifreq != NULL)
	    ifreq = (struct ifreq *)realloc(ifreq, size);
	else
	    ifreq = (struct ifreq *)malloc(size);

	if (ifreq == NULL)
	    goto err;

	ifconf.ifc_len = size;
	ifconf.ifc_req = ifreq;
	if (ioctl(sockfd, SIOCGIFCONF, (caddr_t)&ifconf) < 0
	    || ifconf.ifc_len <= 0) {
	    fprintf(stderr, "bpwhoami: ioctl SIOCGIFCONF failed\n");
	    goto err;
	}
	if ((ifconf.ifc_len + sizeof(struct ifreq)) < size)
	    break;
	size *= 2;
    }
#define IFR_NEXT(ifr)	\
    ((struct ifreq *) ((char *) (ifr) + sizeof(*(ifr)) + \
		       MAX(0, (int) (ifr)->ifr_addr.sa_len \
			   - (int) sizeof((ifr)->ifr_addr))))
    for (ifrp = (struct ifreq *) ifconf.ifc_buf;
	 (char *) ifrp < &ifconf.ifc_buf[ifconf.ifc_len];
	 ifrp = IFR_NEXT(ifrp)) {
	if (ifrp->ifr_addr.sa_family == AF_INET) {
	    struct ifreq	ifr;

	    strncpy(ifr.ifr_name, ifrp->ifr_name, sizeof(ifr.ifr_name));
	    if (ioctl(sockfd, SIOCGIFFLAGS, (caddr_t)&ifr) < 0)
		;
	    else if ((ifr.ifr_flags & IFF_LOOPBACK)
		     || !(ifr.ifr_flags & IFF_UP))
		;
	    else if (ioctl(sockfd, SIOCGIFNETMASK, (caddr_t)&ifr) < 0) 
		;
	    else {
		net_mask = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
		*ret_p = *ifrp;
		close(sockfd);
		if (ifreq)
		    free(ifreq);
		return (TRUE);
	    }
	}
    }
  err:
    close(sockfd);
    if (ifreq)
	free(ifreq);
    return (FALSE);
}

/*
 * Routine: bp_whoami
 * Function:
 *	Do a BOOTPARAMS WHOAMI RPC to find out hostname, domain,
 *	bootparams server, default router.
 */
int
bp_whoami()
{
	extern enum clnt_stat	clnt_broadcast();
	struct ifreq		ifr;
	struct sockaddr_in	*sockin;
	enum clnt_stat		stat;
	struct bp_whoami_arg	who_arg;
	struct bp_whoami_res	who_res;

	if (getFirstInterface(&ifr) == FALSE)
	    return (2);

	sockin = (struct sockaddr_in *) &ifr.ifr_addr;
	ip_address = sockin->sin_addr;
	who_arg.client_address.bp_address_u.ip_addr =
	    *((ip_addr_t *)&sockin->sin_addr);
	who_arg.client_address.address_type = IP_ADDR_TYPE;
	bzero(&who_res, sizeof (who_res));

	/*
	 * Broadcast the whoami.
	 */
	stat = clnt_broadcast(BOOTPARAMPROG, BOOTPARAMVERS,
			      BOOTPARAMPROC_WHOAMI, xdr_bp_whoami_arg,
			      &who_arg, xdr_bp_whoami_res, &who_res,
			      each_whoresult);

	if (stat == RPC_SUCCESS) {
	    return (0);
	}
	if (stat == RPC_TIMEDOUT)
	    return (1);
	fprintf(stderr, "bpwhoami: ");
	clnt_perrno(stat);
	return (2);
}

int
main(argc,argv)
     int argc;
     char *argv[];
{
    exit(bp_whoami());
}
