/*
 * Copyright (c) 2012 Apple Computer, Inc. All rights reserved.
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

#include "config.h"
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <stdio.h>
#include <sys/kern_control.h>
#include <sys/sys_domain.h>
#include "racoon_types.h"
#include "plog.h"
#include <net/if.h>
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>

#include "var.h"

int ipsec_interface_create(char *name, int name_max_len, int *index, int flags)
{
    
	struct ctl_info kernctl_info;
	struct sockaddr_ctl kernctl_addr;
	u_int32_t optlen;
	int tunsock = -1;
    
	tunsock = socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
	if (tunsock == -1) {
		plog(ASL_LEVEL_ERR, "create_ipsec_interface: cannot create kernel control socket (errno = %d)", errno);
		goto fail;
	}
    
	bzero(&kernctl_info, sizeof(kernctl_info));
    strlcpy(kernctl_info.ctl_name, "com.apple.net.ipsec_control", sizeof(kernctl_info.ctl_name));
	if (ioctl(tunsock, CTLIOCGINFO, &kernctl_info)) {
		plog(ASL_LEVEL_ERR, "create_ipsec_interface: ioctl failed on kernel control socket (errno = %d)", errno);
		goto fail;
	}
	
	bzero(&kernctl_addr, sizeof(kernctl_addr)); // sets the sc_unit field to 0
	kernctl_addr.sc_len = sizeof(kernctl_addr);
	kernctl_addr.sc_family = AF_SYSTEM;
	kernctl_addr.ss_sysaddr = AF_SYS_CONTROL;
	kernctl_addr.sc_id = kernctl_info.ctl_id;
	kernctl_addr.sc_unit = 0; // we will get the unit number from getpeername
	if (connect(tunsock, (struct sockaddr *)&kernctl_addr, sizeof(kernctl_addr))) {
		plog(ASL_LEVEL_ERR, "create_ipsec_interface: connect failed on kernel control socket (errno = %d)", errno);
		goto fail;
	}
    
	optlen = name_max_len;
	if (getsockopt(tunsock, SYSPROTO_CONTROL, 2, name, &optlen)) {
		plog(ASL_LEVEL_ERR, "create_ipsec_interface: getsockopt ifname failed on kernel control socket (errno = %d)", errno);
		goto fail;
	}
    
	*index = if_nametoindex(name);
    
	if (flags) {
		int optflags = 0;
		optlen = sizeof(u_int32_t);
		if (getsockopt(tunsock, SYSPROTO_CONTROL, 1, &optflags, &optlen)) {
			plog(ASL_LEVEL_ERR, "create_ipsec_interface: getsockopt flags failed on kernel control socket (errno = %d)", errno);
			goto fail;
		}
        
		optflags |= flags;
		optlen = sizeof(u_int32_t);
		if (setsockopt(tunsock, SYSPROTO_CONTROL, 1, &optflags, optlen)) {
			plog(ASL_LEVEL_ERR, "create_ipsec_interface: setsockopt flags failed on kernel control socket (errno = %d)", errno);
			goto fail;
		}
	}
    
	return tunsock;
	
fail:
    if (tunsock != -1)
        close(tunsock);
	return -1;
	
}

int ipsec_interface_set_mtu(char *ifname, int mtu)
{
    struct ifreq ifr;
	int ip_sockfd;
	
    ip_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (ip_sockfd < 0) {
		plog(ASL_LEVEL_ERR, "sifmtu: cannot create ip socket, %s", strerror(errno));
		return 0;
	}
    
    strlcpy(ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
    ifr.ifr_mtu = mtu;
    ioctl(ip_sockfd, SIOCSIFMTU, (caddr_t) &ifr);
    
	close(ip_sockfd);
	return 1;
}

void
in6_len2mask(struct in6_addr *mask, int len)
{
    int i;
    bzero(mask, sizeof(*mask));
    for (i = 0; i < len / 8; i++)
        mask->s6_addr[i] = 0xff;
    if (len % 8)
        mask->s6_addr[i] = (0xff00 >> (len % 8)) & 0xff;
}

#define SET_SA_FAMILY(addr, family)		\
bzero((char *) &(addr), sizeof(addr));	\
addr.sa_family = (family); 			\
addr.sa_len = sizeof(addr);
int ipsec_interface_set_addr(char *ifname, struct sockaddr_storage *address, struct sockaddr_storage *netmask, int prefix)
{
	int ip_sockfd;
    
    int family = address->ss_family;
    
    if (family == AF_INET) {
        struct ifaliasreq ifra __attribute__ ((aligned (4)));   // Wcast-align fix - force alignment
        ip_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (ip_sockfd < 0) {
            plog(ASL_LEVEL_ERR, "Cannot create ip socket, %s", strerror(errno));
            return 0;
        }
        
        strlcpy(ifra.ifra_name, ifname, sizeof(ifra.ifra_name));
        
        SET_SA_FAMILY(ifra.ifra_addr, AF_INET);
        (ALIGNED_CAST(struct sockaddr_in *) &ifra.ifra_addr)->sin_addr.s_addr = ((struct sockaddr_in*)address)->sin_addr.s_addr;
        
        SET_SA_FAMILY(ifra.ifra_broadaddr, AF_INET);
        (ALIGNED_CAST(struct sockaddr_in *) &ifra.ifra_broadaddr)->sin_addr.s_addr = ((struct sockaddr_in*)address)->sin_addr.s_addr;
        
        if (netmask != 0) {
            SET_SA_FAMILY(ifra.ifra_mask, AF_INET);
            (ALIGNED_CAST(struct sockaddr_in *) &ifra.ifra_mask)->sin_addr.s_addr = ((struct sockaddr_in*)netmask)->sin_addr.s_addr;
        }
        else
            bzero(&ifra.ifra_mask, sizeof(ifra.ifra_mask));
        
        if (ioctl(ip_sockfd, SIOCAIFADDR, (caddr_t) &ifra) < 0) {
            if (errno != EEXIST) {
                plog(ASL_LEVEL_ERR, "Couldn't set interface address");
                close(ip_sockfd);
                return 0;
            }
            plog(ASL_LEVEL_ERR, "Couldn't set interface address, already exists");
        }
        close(ip_sockfd);
    } else if (family == AF_INET6) {
        struct in6_aliasreq addreq6;
        struct in6_addr mask;
        struct in6_addr *addr6 = &((struct sockaddr_in6*)address)->sin6_addr;
        
        ip_sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
        if (ip_sockfd < 0) {
            plog(ASL_LEVEL_ERR, "Cannot create IPv6 socket, %s", strerror(errno));
            return 0;
        }
        
        memset(&addreq6, 0, sizeof(addreq6));
        strlcpy(addreq6.ifra_name, ifname, sizeof(addreq6.ifra_name));
        /* my addr */
        addreq6.ifra_addr.sin6_family = AF_INET6;
        addreq6.ifra_addr.sin6_len = sizeof(struct sockaddr_in6);
        memcpy(&addreq6.ifra_addr.sin6_addr, addr6, sizeof(struct in6_addr));
        
        /* prefix mask: 128bit */
        addreq6.ifra_prefixmask.sin6_family = AF_INET6;
        addreq6.ifra_prefixmask.sin6_len = sizeof(struct sockaddr_in6);
        in6_len2mask(&mask, prefix);
        memcpy(&addreq6.ifra_prefixmask.sin6_addr, &mask, sizeof(struct in6_addr));
        
        /* address lifetime (infty) */
        addreq6.ifra_lifetime.ia6t_pltime = ND6_INFINITE_LIFETIME;
        addreq6.ifra_lifetime.ia6t_vltime = ND6_INFINITE_LIFETIME;
        if (IN6_IS_ADDR_LINKLOCAL(addr6)) {
            if (ioctl(ip_sockfd, SIOCLL_START, &addreq6) < 0) {
                plog(ASL_LEVEL_ERR, "Couldn't set link-local IPv6 address, %s", strerror(errno));
                close(ip_sockfd);
                return 0;
            }
        } else {
            if (ioctl(ip_sockfd, SIOCAIFADDR_IN6, &addreq6) < 0) {
               plog(ASL_LEVEL_ERR, "Couldn't set IPv6 address, %s", strerror(errno));
                close(ip_sockfd);
                return 0;
            }
        }
        close(ip_sockfd);
    } else {
        return 0;
    }

	return 1;
}
