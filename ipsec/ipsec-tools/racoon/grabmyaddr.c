/* $Id: grabmyaddr.c,v 1.23.4.2 2005/07/16 04:41:01 monas Exp $ */

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <net/if_var.h>
#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <net/route.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <netdb.h>
#ifdef HAVE_GETIFADDRS
#include <ifaddrs.h>
#include <net/if.h>
#endif 
#include <fcntl.h>

#include "var.h"
#include "misc.h"
#include "vmbuf.h"
#include "plog.h"
#include "sockmisc.h"
#include "debug.h"

#include "localconf.h"
#include "handler.h"
#include "grabmyaddr.h"
#include "sockmisc.h"
#include "isakmp_var.h"
#include "gcmalloc.h"
#include "nattraversal.h"

#ifndef HAVE_GETIFADDRS
static unsigned int if_maxindex __P((void));
#endif

static int suitable_ifaddr __P((const char *, const struct sockaddr *));
#ifdef INET6
static int suitable_ifaddr6 __P((const char *, const struct sockaddr *));
#endif

#ifndef HAVE_GETIFADDRS
static unsigned int
if_maxindex()
{
	struct if_nameindex *p, *p0;
	unsigned int max = 0;

	p0 = if_nameindex();
	for (p = p0; p && p->if_index && p->if_name; p++) {
		if (max < p->if_index)
			max = p->if_index;
	}
	if_freenameindex(p0);
	return max;
}
#endif


void
clear_myaddr()
{
	struct myaddrs *p, *next;

	for (p = lcconf->myaddrs; p; p = next) {
		next = p->next;

		delmyaddr(p);	
	}

	lcconf->myaddrs = NULL;

}


struct myaddrs *
find_myaddr(addr, udp_encap)
	struct sockaddr *addr;
	int udp_encap;
{
	struct myaddrs *q;
	char h1[NI_MAXHOST], h2[NI_MAXHOST];

	if (getnameinfo(addr, sysdep_sa_len(addr), h1, sizeof(h1), NULL, 0,
	    NI_NUMERICHOST | niflags) != 0)
		return NULL;

	for (q = lcconf->myaddrs; q; q = q->next) {
		if (!q->addr)
			continue;
		if (q->udp_encap && !udp_encap
			|| !q->udp_encap && udp_encap)
			continue;
		if (addr->sa_family != q->addr->ss_family)
			continue;
		if (getnameinfo((struct sockaddr *)q->addr, sysdep_sa_len((struct sockaddr *)q->addr), h2, sizeof(h2),
		    NULL, 0, NI_NUMERICHOST | niflags) != 0)
			return NULL;
		if (strcmp(h1, h2) == 0)
			return q;
	}

	return NULL;
}


// modified to avoid closing and opening sockets for
// all interfaces each time an interface change occurs.
// on return: 	addrcount = zero indicates address no longer used
//				sock = -1 indicates a new address - no socket opened yet.
void
grab_myaddrs()
{
#ifdef HAVE_GETIFADDRS
	struct myaddrs *p, *q;
	struct ifaddrs *ifa0, *ifap;

	char addr1[NI_MAXHOST];

	if (getifaddrs(&ifa0)) {
		plog(LLV_ERROR2, LOCATION, NULL,
			"getifaddrs failed: %s\n", strerror(errno));
		exit(1);
		/*NOTREACHED*/
	}

	// clear the in_use flag for each address in the list
	for (p = lcconf->myaddrs; p; p = p->next)
		p->in_use = 0;

	for (ifap = ifa0; ifap; ifap = ifap->ifa_next) {

		if (ifap->ifa_addr->sa_family != AF_INET
#ifdef INET6
		 && ifap->ifa_addr->sa_family != AF_INET6
#endif
		)
			continue;

		if (!suitable_ifaddr(ifap->ifa_name, ifap->ifa_addr)) {
			plog(LLV_DEBUG2, LOCATION, NULL,
				"unsuitable address: %s %s\n",
				ifap->ifa_name,
				saddrwop2str(ifap->ifa_addr));
			continue;
		}

		p = find_myaddr(ifap->ifa_addr, 0);
		if (p) {
			p->in_use = 1;
#ifdef ENABLE_NATT
			q = find_myaddr(ifap->ifa_addr, 1);
			if (q)
				q->in_use = 1;
#endif				
		} else {	
			p = newmyaddr();
			if (p == NULL) {
				plog(LLV_ERROR2, LOCATION, NULL,
					"unable to allocate space for addr.\n");
				exit(1);
				/*NOTREACHED*/
			}
			p->addr = dupsaddr(ifap->ifa_addr);
			if (p->addr == NULL) {
				plog(LLV_ERROR2, LOCATION, NULL,
					"unable to duplicate addr.\n");
				exit(1);
				/*NOTREACHED*/
			}
			p->ifname = racoon_strdup(ifap->ifa_name);
			if (p->ifname == NULL) {
				plog(LLV_ERROR2, LOCATION, NULL,
					"unable to duplicate ifname.\n");
				exit(1);
				/*NOTREACHED*/
			}
				
			p->sock = -1;
			p->in_use = 1;

			if (getnameinfo((struct sockaddr *)p->addr, p->addr->ss_len,
					addr1, sizeof(addr1),
					NULL, 0,
					NI_NUMERICHOST | niflags))
				strlcpy(addr1, "(invalid)", sizeof(addr1));
			plog(LLV_DEBUG, LOCATION, NULL,
				"my interface: %s (%s)\n",
				addr1, ifap->ifa_name);
		
			p->next = lcconf->myaddrs;
			lcconf->myaddrs = p;
			
#ifdef ENABLE_NATT
			if (natt_enabled_in_rmconf ()) {
				q = dupmyaddr(p);
				if (q == NULL) {
					plog(LLV_ERROR2, LOCATION, NULL,
						"unable to allocate space for natt addr.\n");
					exit(1);
				}
				q->udp_encap = 1;
			}
#endif

		}
	}

	freeifaddrs(ifa0);


#else /*!HAVE_GETIFADDRS*/
#error "NOT SUPPORTED"
#endif /*HAVE_GETIFADDRS*/
}


/*
 * check the interface is suitable or not
 */
static int
suitable_ifaddr(ifname, ifaddr)
	const char *ifname;
	const struct sockaddr *ifaddr;
{
#if 0 //we need to be able to do nested ipsec for BTMM... stub out ifdef ENABLE_HYBRID
	/* Exclude any address we got through ISAKMP mode config */
	if (exclude_cfg_addr(ifaddr) == 0)
		return 0;
#endif
	switch(ifaddr->sa_family) {
	case AF_INET:
		return 1;
#ifdef INET6
	case AF_INET6:
		return suitable_ifaddr6(ifname, ifaddr);
#endif
	default:
		return 0;
	}
	/*NOTREACHED*/
}

#ifdef INET6
static int
suitable_ifaddr6(ifname, ifaddr)
	const char *ifname;
	const struct sockaddr *ifaddr;
{
	struct in6_ifreq ifr6;
	int s;

	if (ifaddr->sa_family != AF_INET6)
		return 0;

	s = socket(PF_INET6, SOCK_DGRAM, 0);
	if (s == -1) {
		plog(LLV_ERROR, LOCATION, NULL,
			"socket(SOCK_DGRAM) failed:%s\n", strerror(errno));
		return 0;
	}

	if (fcntl(s, F_SETFL, O_NONBLOCK) == -1) {
		plog(LLV_ERROR, LOCATION, NULL,
			 "failed to put IPv6 socket in non-blocking mode\n");
	}

	memset(&ifr6, 0, sizeof(ifr6));
	strlcpy(ifr6.ifr_name, ifname, sizeof(ifr6.ifr_name));

	memcpy(&ifr6.ifr_addr, ifaddr, sizeof(struct sockaddr_in6));    // Wcast-align fix - copy instread of assign with cast

	if (ioctl(s, SIOCGIFAFLAG_IN6, &ifr6) < 0) {
		plog(LLV_ERROR, LOCATION, NULL,
			"ioctl(SIOCGIFAFLAG_IN6) failed:%s\n", strerror(errno));
		close(s);
		return 0;
	}

	close(s);

	if (ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_DUPLICATED
	 || ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_DETACHED
	 || ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_ANYCAST)
		return 0;

	/* suitable */
	return 1;
}
#endif

int
update_myaddrs()
{   
    struct rtmessage {          // Wcast-align fix - force alignment
        struct rt_msghdr rtm;  
        char discard[BUFSIZ];
    } msg;
	
	int len;

	while((len = read(lcconf->rtsock, &msg, sizeof(msg))) < 0) {
		if (errno == EINTR)
			continue;
		plog(LLV_ERROR, LOCATION, NULL,
			"read(PF_ROUTE) failed: %s\n",
			strerror(errno));
		return 0;
	}
	if (len < msg.rtm.rtm_msglen) {
		plog(LLV_ERROR, LOCATION, NULL,
			"read(PF_ROUTE) short read\n");
		return 0;
	}
	if (msg.rtm.rtm_version != RTM_VERSION) {
		plog(LLV_ERROR, LOCATION, NULL,
			"routing socket version mismatch\n");
		close(lcconf->rtsock);
		lcconf->rtsock = -1;
		return 0;
	}
	switch (msg.rtm.rtm_type) {
	case RTM_NEWADDR:
	case RTM_DELADDR:
	case RTM_DELETE:
	case RTM_IFINFO:
		break;
	case RTM_MISS:
		/* ignore this message silently */
		return 0;
	default:
		//plog(LLV_DEBUG, LOCATION, NULL,
		//	"msg %d not interesting\n", msg.rtm.rtm_type);
		return 0;
	}
	/* XXX more filters here? */

	//plog(LLV_DEBUG, LOCATION, NULL,
	//	"caught rtm:%d, need update interface address list\n",
	//	msg.rtm.rtm_type);

	return 1;
}

/*
 * initialize default port for ISAKMP to send, if no "listen"
 * directive is specified in config file.
 *
 * DO NOT listen to wildcard addresses.  if you receive packets to
 * wildcard address, you'll be in trouble (DoS attack possible by
 * broadcast storm).
 */
int
autoconf_myaddrsport()
{
	struct myaddrs *p;
	int n;

	plog(LLV_DEBUG, LOCATION, NULL,
		"configuring default isakmp port.\n");

	for (p = lcconf->myaddrs, n = 0; p; p = p->next, n++) {
		set_port (p->addr, p->udp_encap ? lcconf->port_isakmp_natt : lcconf->port_isakmp);
	}
	plog(LLV_DEBUG, LOCATION, NULL,
		"%d addrs are configured successfully\n", n);

	return 0;
}

/*
 * get a port number to which racoon binded.
 * NOTE: network byte order returned.
 */
u_short
getmyaddrsport(local)
	struct sockaddr_storage *local;
{
	struct myaddrs *p, *bestmatch = NULL;
	u_short bestmatch_port = PORT_ISAKMP;

	/* get a relative port */
	for (p = lcconf->myaddrs; p; p = p->next) {
		if (!p->addr)
			continue;
		if (!cmpsaddrwop(local, p->addr)) {
			if (! bestmatch) {
				bestmatch = p;
				continue;
			}
			
			switch (p->addr->ss_family) {
			case AF_INET:
				if (((struct sockaddr_in *)p->addr)->sin_port == PORT_ISAKMP) {
					bestmatch = p;
					bestmatch_port = ((struct sockaddr_in *)p->addr)->sin_port;
					break;
				}
				break;
#ifdef INET6
			case AF_INET6:
				if (((struct sockaddr_in6 *)p->addr)->sin6_port == PORT_ISAKMP) {
					bestmatch = p;
					bestmatch_port = ((struct sockaddr_in6 *)p->addr)->sin6_port;
					break;
				}
				break;
#endif
			default:
				plog(LLV_ERROR, LOCATION, NULL,
				     "unsupported AF %d\n", p->addr->ss_family);
				continue;
			}
		}
	}

	return htons(bestmatch_port);
}

struct myaddrs *
newmyaddr()
{
	struct myaddrs *new;

	new = racoon_calloc(1, sizeof(*new));
	if (new == NULL) {
		plog(LLV_ERROR, LOCATION, NULL,
			"failed to allocate buffer for myaddrs.\n");
		return NULL;
	}

	new->next = NULL;
	new->addr = NULL;
#ifdef __APPLE_
	new->ifname = NULL;
#endif

	return new;
}

struct myaddrs *
dupmyaddr(struct myaddrs *old)
{
	struct myaddrs *new;

	new = racoon_calloc(1, sizeof(*new));
	if (new == NULL) {
		plog(LLV_ERROR, LOCATION, NULL,
			"failed to allocate buffer for myaddrs.\n");
		return NULL;
	}

	/* Copy the whole structure and set the differences.  */
	memcpy (new, old, sizeof (*new));
	new->addr = dupsaddr ((struct sockaddr *)old->addr);
	if (new->addr == NULL) {
		plog(LLV_ERROR, LOCATION, NULL,
			"failed to allocate buffer for duplicate addr.\n");
		racoon_free(new);
		return NULL;
	}
	if (old->ifname) {
		new->ifname = racoon_strdup(old->ifname);
		if (new->ifname == NULL) {
			plog(LLV_ERROR, LOCATION, NULL,
				"failed to allocate buffer for duplicate ifname.\n");
			racoon_free(new->addr);
			racoon_free(new);
			return NULL;
		}
	}
			
	new->next = old->next;
	old->next = new;

	return new;
}

void
insmyaddr(new, head)
	struct myaddrs *new;
	struct myaddrs **head;
{
	new->next = *head;
	*head = new;
}

void
delmyaddr(myaddr)
	struct myaddrs *myaddr;
{
	if (myaddr->addr)
		racoon_free(myaddr->addr);
	if (myaddr->ifname)
		racoon_free(myaddr->ifname);
	racoon_free(myaddr);
}

int
initmyaddr()
{
	/* initialize routing socket */
	lcconf->rtsock = socket(PF_ROUTE, SOCK_RAW, PF_UNSPEC);
	if (lcconf->rtsock < 0) {
		plog(LLV_ERROR, LOCATION, NULL,
			"socket(PF_ROUTE) failed: %s",
			strerror(errno));
		return -1;
	}
    
	if (fcntl(lcconf->rtsock, F_SETFL, O_NONBLOCK) == -1) {
		plog(LLV_ERROR, LOCATION, NULL,
			 "failed to put PF_ROUTE socket in non-blocking mode\n");
	}

	if (lcconf->myaddrs == NULL && lcconf->autograbaddr == 1) {
		grab_myaddrs();

		if (autoconf_myaddrsport() < 0)
			return -1;
	}

	return 0;
}

/* select the socket to be sent */
/* should implement other method. */
int
getsockmyaddr(my)
	struct sockaddr *my;
{
	struct myaddrs *p, *lastresort = NULL;

	for (p = lcconf->myaddrs; p; p = p->next) {
		if (p->addr == NULL)
			continue;
		if (my->sa_family == p->addr->ss_family) {
			lastresort = p;
		} else continue;
		if (sysdep_sa_len(my) == sysdep_sa_len((struct sockaddr *)p->addr)
		 && memcmp(my, p->addr, sysdep_sa_len(my)) == 0) {
			break;
		}
	}
	if (!p)
		p = lastresort;
	if (!p) {
		plog(LLV_ERROR, LOCATION, NULL,
			"no socket matches address family %d\n",
			my->sa_family);
		return -1;
	}

	return p->sock;
}
