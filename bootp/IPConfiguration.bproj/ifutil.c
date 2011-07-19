/*
 * Copyright (c) 2000-2011 Apple Inc. All rights reserved.
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
 * ifutil.c
 * - network interface utility routines
 */

/* 
 * Modification History
 *
 * June 23, 2009	Dieter Siegmund (dieter@apple.com)
 * - split out from ipconfigd.c
 */


#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/icmp6.h>
#include <net/if_media.h>
#include <net/if_dl.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/errno.h>
#define KERNEL_PRIVATE
#include <sys/ioctl.h>
#undef KERNEL_PRIVATE
#include "ifutil.h"
#include "rtutil.h"
#include "symbol_scope.h"
#include "mylog.h"

PRIVATE_EXTERN int
inet_dgram_socket()
{
    return (socket(AF_INET, SOCK_DGRAM, 0));
}

static int
ifflags_set(int s, const char * name, short flags)
{
    struct ifreq	ifr;
    int 		ret;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    ret = ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr);
    if (ret < 0) {
	return (ret);
    }
    ifr.ifr_flags |= flags;
    return (ioctl(s, SIOCSIFFLAGS, &ifr));
}

STATIC int
siocprotoattach(int s, const char * name)
{
    struct ifreq	ifr;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    return (ioctl(s, SIOCPROTOATTACH, &ifr));
}

PRIVATE_EXTERN int
inet_attach_interface(const char * ifname)
{
    int ret = 0;
    int s = inet_dgram_socket();

    if (s < 0) {
	ret = errno;
	goto done;
    }

    if (siocprotoattach(s, ifname) < 0) {
	ret = errno;
	if (ret != EEXIST && ret != ENXIO) {
	    my_log(LOG_DEBUG, "siocprotoattach(%s) failed, %s (%d)", 
		   ifname, strerror(errno), errno);
	}
    }
    (void)ifflags_set(s, ifname, IFF_UP);
    close(s);

 done:
    return (ret);
}

STATIC int
siocprotodetach(int s, const char * name)
{
    struct ifreq	ifr;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    return (ioctl(s, SIOCPROTODETACH, &ifr));
}

PRIVATE_EXTERN int
inet_detach_interface(const char * ifname)
{
    int ret = 0;
    int s = inet_dgram_socket();

    if (s < 0) {
	ret = errno;
	goto done;
    }
    if (siocprotodetach(s, ifname) < 0) {
	ret = errno;
	if (ret != ENXIO) {
	    my_log(LOG_ERR, "siocprotodetach(%s) failed, %s (%d)", 
		   ifname, strerror(errno), errno);
	}
    }
    close(s);

 done:
    return (ret);
}

STATIC int
siocautoaddr(int s, const char * name, int value)
{
    struct ifreq	ifr;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    ifr.ifr_data = (caddr_t)(intptr_t)value;
    return (ioctl(s, SIOCAUTOADDR, &ifr));
}

PRIVATE_EXTERN int
inet_set_autoaddr(const char * ifname, int val)
{
    int 		s = inet_dgram_socket();
    int			ret = 0;

    if (s < 0) {
	ret = errno;
	my_log(LOG_ERR, 
	       "inet_set_autoaddr(%s, %d): socket() failed, %s (%d)",
	       ifname, val, strerror(errno), errno);
    }
    else {
	if (siocautoaddr(s, ifname, val) < 0) {
	    ret = errno;
	    if (ret != ENXIO) {
		my_log(LOG_ERR, "inet_set_autoaddr(%s, %d) failed, %s (%d)", 
		       ifname, val, strerror(errno), errno);
	    }
	}
	close(s);
    }
    return (ret);
}

STATIC void
set_sockaddr_in(struct sockaddr_in * sin_p, struct in_addr addr)
{
    sin_p->sin_len = sizeof(struct sockaddr_in);
    sin_p->sin_family = AF_INET;
    sin_p->sin_addr = addr;
    return;
}

PRIVATE_EXTERN int
inet_difaddr(int s, const char * name, const struct in_addr addr)
{
    struct ifreq	ifr;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    set_sockaddr_in((struct sockaddr_in *)&ifr.ifr_addr, addr);
    return (ioctl(s, SIOCDIFADDR, &ifr));
}

PRIVATE_EXTERN int
inet_aifaddr(int s, const char * name, struct in_addr addr,
	     const struct in_addr * mask,
	     const struct in_addr * broadaddr)
{
    struct in_aliasreq	ifra;

    bzero(&ifra, sizeof(ifra));
    strncpy(ifra.ifra_name, name, sizeof(ifra.ifra_name));
    set_sockaddr_in(&ifra.ifra_addr, addr);
    if (mask != NULL) {
	set_sockaddr_in(&ifra.ifra_mask, *mask);
    }
    if (broadaddr != NULL) {
	set_sockaddr_in(&ifra.ifra_broadaddr, *broadaddr);
    }
    return (ioctl(s, SIOCAIFADDR, &ifra));
}

#define KERNEL_PRIVATE
#include <netinet6/in6_var.h>
#undef KERNEL_PRIVATE
#include <netinet6/nd6.h>

static int
count_prefix_bits(void * val, int size)
{
    int		bit;
    int 	byte;
    uint8_t *	name = (uint8_t *)val;
    int		plen = 0;

    /* look for prefix bytes that have all bits set */
    for (byte = 0; byte < size; byte++, plen += 8) {
	if (name[byte] != 0xff) {
	    break;
	}
    }

    /* all of the bits were set */
    if (byte == size) {
	return (plen);
    }

    /* we have the prefix length when we seee the first bit that isn't set */
    for (bit = 7; bit != 0; bit--, plen++) {
	if (!(name[byte] & (1 << bit))) {
	    break;
	}
    }

    /* valididate that no bits are set after the last bit */
    for (; bit != 0; bit--) {
	if (name[byte] & (1 << bit)) {
	    /* not a simple prefix */
	    return (0);
	}
    }
    byte++;
    for (; byte < size; byte++) {
	if (name[byte]) {
	    /* not a simple prefix */
	    return (0);
	}
    }
    return (plen);
}

PRIVATE_EXTERN int
inet6_dgram_socket()
{
    return (socket(AF_INET6, SOCK_DGRAM, 0));
}

static int
siocprotoattach_in6(int s, const char * name)
{
    struct in6_aliasreq		ifra;

    bzero(&ifra, sizeof(ifra));
    strncpy(ifra.ifra_name, name, sizeof(ifra.ifra_name));
    return (ioctl(s, SIOCPROTOATTACH_IN6, &ifra));
}

static int
siocprotodetach_in6(int s, const char * name)
{
    struct in6_ifreq	ifr;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    return (ioctl(s, SIOCPROTODETACH_IN6, &ifr));
}

static int
siocll_start(int s, const char * name)
{
   struct in6_aliasreq		ifra_in6;

    bzero(&ifra_in6, sizeof(ifra_in6));
    strncpy(ifra_in6.ifra_name, name, sizeof(ifra_in6.ifra_name));
    return (ioctl(s, SIOCLL_START, &ifra_in6));
}

static int
siocll_stop(int s, const char * name)
{
    struct in6_ifreq		ifr;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    return (ioctl(s, SIOCLL_STOP, &ifr));
}

static void
set_sockaddr_in6(struct sockaddr_in6 * sin6_p, const struct in6_addr * addr)
{
    sin6_p->sin6_family = AF_INET6;
    sin6_p->sin6_len = sizeof(struct sockaddr_in6);
    sin6_p->sin6_addr = *addr;
    return;
}

STATIC int
siocgifaflag_in6(int s, const char * ifname, const struct in6_addr * in6_addr,
		 uint16_t * ret_flags)
{
    struct in6_ifreq	ifr6;

    bzero((char *)&ifr6, sizeof(ifr6));
    strncpy(ifr6.ifr_name, ifname, sizeof(ifr6.ifr_name));
    set_sockaddr_in6(&ifr6.ifr_addr, in6_addr);
    if (ioctl(s, SIOCGIFAFLAG_IN6, &ifr6) < 0) {
	return (-1);
    }
    *ret_flags = ifr6.ifr_ifru.ifru_flags6;
    return (0);
}

PRIVATE_EXTERN int
inet6_attach_interface(const char * ifname)
{
    int	ret = 0;
    int s = inet6_dgram_socket();

    if (s < 0) {
	ret = errno;
	my_log(LOG_ERR,
	       "inet6_attach_interface(%s): socket() failed, %s (%d)",
	       ifname, strerror(ret), ret);
	goto done;
    }
    if (siocprotoattach_in6(s, ifname) < 0) {
	ret = errno;
	if (ret != EEXIST && ret != ENXIO) {
	    my_log(LOG_DEBUG, "siocprotoattach_in6(%s) failed, %s (%d)",
		   ifname, strerror(errno), errno);
	}
    }
    (void)ifflags_set(s, ifname, IFF_UP);
    close(s);

 done:
    return (ret);
}

PRIVATE_EXTERN int
inet6_detach_interface(const char * ifname)
{
    int ret = 0;
    int s = inet6_dgram_socket();

    if (s < 0) {
	ret = errno;
	my_log(LOG_ERR, 
	       "inet6_detach_interface(%s): socket() failed, %s (%d)",
	       ifname, strerror(ret), ret);
	goto done;
    }
    if (siocprotodetach_in6(s, ifname) < 0) {
	ret = errno;
	if (ret != ENXIO) {
	    my_log(LOG_DEBUG, "siocprotodetach_in6(%s) failed, %s (%d)",
		   ifname, strerror(errno), errno);
	}
    }
    close(s);

 done:
    return (ret);
}

PRIVATE_EXTERN int
inet6_linklocal_start(const char * ifname)
{
    int ret = 0;
    int s = inet6_dgram_socket();

    if (s < 0) {
	ret = errno;
	my_log(LOG_ERR, 
	       "inet6_linklocal_start(%s): socket() failed, %s (%d)",
	       ifname, strerror(ret), ret);
	goto done;
    }
    if (siocll_start(s, ifname) < 0) {
	ret = errno;
	if (errno != ENXIO) {
	    my_log(LOG_ERR, "siocll_start(%s) failed, %s (%d)",
		   ifname, strerror(errno), errno);
	}
    }
    close(s);
 done:
    return (ret);
}

PRIVATE_EXTERN int
inet6_linklocal_stop(const char * ifname)
{
    int ret = 0;
    int s = inet6_dgram_socket();

    if (s < 0) {
	ret = errno;
	my_log(LOG_ERR, 
	       "inet6_linklocal_stop(%s): socket() failed, %s (%d)",
	       ifname, strerror(ret), ret);
	goto done;
    }
    if (siocll_stop(s, ifname) < 0) {
	ret = errno;
	if (errno != ENXIO) {
	    my_log(LOG_ERR, "siocll_stop(%s) failed, %s (%d)",
		   ifname, strerror(errno), errno);
	}
    }
    close(s);

 done:
    return (ret);
}

static int
siocautoconf_start(int s, const char * if_name)
{
    struct in6_ifreq	ifr;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name));
    return (ioctl(s, SIOCAUTOCONF_START, &ifr));
}

static int
siocautoconf_stop(int s, const char * if_name)
{
    struct in6_ifreq	ifr;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name));
    return (ioctl(s, SIOCAUTOCONF_STOP, &ifr));
}

PRIVATE_EXTERN int
inet6_rtadv_enable(const char * if_name)
{
    int			ret = 0;
    int			s = inet6_dgram_socket();

    if (s < 0) {
	ret = errno;
	my_log(LOG_ERR, 
	       "inet6_rtadv_enable(%s): socket() failed, %s (%d)",
	       if_name, strerror(ret), ret);
	goto done;
    }
    if (siocautoconf_start(s, if_name) < 0) {
	ret = errno;
	if (errno != ENXIO) {
	    my_log(LOG_ERR, "siocautoconf_start(%s) failed, %s (%d)",
		   if_name, strerror(errno), errno);
	}
    }
    close(s);
 done:
    return (ret);
}

PRIVATE_EXTERN int
inet6_rtadv_disable(const char * if_name)
{
    int			ret = 0;
    int			s = inet6_dgram_socket();

    if (s < 0) {
	ret = errno;
	my_log(LOG_ERR, 
	       "inet6_rtadv_disable(%s): socket() failed, %s (%d)",
	       if_name, strerror(ret), ret);
	goto done;
    }
    if (siocautoconf_stop(s, if_name) < 0) {
	ret = errno;
	if (errno != ENXIO) {
	    my_log(LOG_ERR, "siocautoconf_stop(%s) failed, %s (%d)",
		   if_name, strerror(errno), errno);
	}
    }
    close(s);
 done:
    return (ret);
}

PRIVATE_EXTERN int
inet6_difaddr(int s, const char * name, const struct in6_addr * addr)
{
    struct in6_ifreq	ifr;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    if (addr != NULL) {
	set_sockaddr_in6(&ifr.ifr_ifru.ifru_addr, addr);
    }
    return (ioctl(s, SIOCDIFADDR_IN6, &ifr));
}

/*
 * from netinet6/in6.c 
 */
static void
in6_len2mask(struct in6_addr * mask, int len)
{
    int i;

    bzero(mask, sizeof(*mask));
    for (i = 0; i < len / 8; i++)
	mask->s6_addr[i] = 0xff;
    if (len % 8)
	mask->s6_addr[i] = (0xff00 >> (len % 8)) & 0xff;
}

static void
in6_maskaddr(struct in6_addr * addr, const struct in6_addr * mask)
{
    int i;

    for (i = 0; i < sizeof(addr->s6_addr); i++) {
	addr->s6_addr[i] &= mask->s6_addr[i];
    }
    return;
}

PRIVATE_EXTERN void
in6_netaddr(struct in6_addr * addr, int len)
{
    struct in6_addr	mask;

    in6_len2mask(&mask, len);
    in6_maskaddr(addr, &mask);
    return;
}

PRIVATE_EXTERN int
inet6_get_prefix_length(const struct in6_addr * addr, int if_index)
{
    char *		buf = NULL;
    size_t 		buf_len;
    struct in6_prefix *	end;
    int 		mib[] = {
	CTL_NET, PF_INET6, IPPROTO_ICMPV6, ICMPV6CTL_ND6_PRLIST
    };
    struct in6_prefix *	next;
    int			prefix_length = 0;
    struct in6_prefix *	scan;

    if (sysctl(mib, sizeof(mib) / sizeof(mib[0]), NULL, &buf_len, NULL, 0)
	< 0) {
	goto done;
    }
    buf_len += 1024;
    buf = malloc(buf_len);
    if (sysctl(mib, sizeof(mib) / sizeof(mib[0]), buf, &buf_len, NULL, 0)
	< 0) {
	goto done;
    }

    end = (struct in6_prefix *)(buf + buf_len);
    for (scan = (struct in6_prefix *)buf; scan < end; scan = next) {
	struct sockaddr_in6 *	advrtr;
	struct in6_addr		netaddr;

	advrtr = (struct sockaddr_in6 *)(scan + 1);
	next = (struct in6_prefix *)&advrtr[scan->advrtrs];

	if (if_index != 0 && if_index != scan->if_index) {
	    continue;
	}
	netaddr = *addr;
	in6_netaddr(&netaddr, scan->prefixlen);
	if (IN6_ARE_ADDR_EQUAL(&netaddr, &scan->prefix.sin6_addr)) {
	    prefix_length = scan->prefixlen;
	    break;
	}
    }

 done:
    if (buf != NULL) {
	free(buf);
    }
    return (prefix_length);
}

PRIVATE_EXTERN int
inet6_aifaddr(int s, const char * name, const struct in6_addr * addr,
	      const struct in6_addr * dstaddr, int prefix_length, 
	      u_int32_t valid_lifetime, u_int32_t preferred_lifetime)
{
    struct in6_aliasreq	ifra_in6;

    bzero(&ifra_in6, sizeof(ifra_in6));
    strncpy(ifra_in6.ifra_name, name, sizeof(ifra_in6.ifra_name));
    ifra_in6.ifra_lifetime.ia6t_vltime = valid_lifetime;
    ifra_in6.ifra_lifetime.ia6t_pltime = preferred_lifetime;
    if (addr != NULL) {
	set_sockaddr_in6(&ifra_in6.ifra_addr, addr);
    }
    if (dstaddr != NULL) {
	set_sockaddr_in6(&ifra_in6.ifra_dstaddr, dstaddr);
    }
    if (prefix_length != 0) {
	struct in6_addr		prefixmask;

	in6_len2mask(&prefixmask, prefix_length);
	set_sockaddr_in6(&ifra_in6.ifra_prefixmask, &prefixmask);
    }
    return (ioctl(s, SIOCAIFADDR_IN6, &ifra_in6));
}

static int
inet6_if_ioctl(const char * ifname, unsigned long request)
{
    struct in6_ifreq	ifr;
    int 		ret = 0;
    int			s;

    s = inet6_dgram_socket();
    if (s < 0) {
	ret = errno;
	goto done;
    }
    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));

    if (ioctl(s, request, &ifr) < 0) {
	ret = errno;
    }
 done:
    if (s >=0) {
	close(s);
    }
    return (ret);
}

PRIVATE_EXTERN int
inet6_flush_prefixes(const char * ifname)
{
    /* this currently has a global effect XXX */
    return (inet6_if_ioctl(ifname, SIOCSPFXFLUSH_IN6));
}

PRIVATE_EXTERN int
inet6_flush_routes(const char * ifname)
{
    /* this currently has a global effect XXX */
    return (inet6_if_ioctl(ifname, SIOCSRTRFLUSH_IN6));
}

static boolean_t
inet6_sysctl_get_int(int code, int * ret_value_p)
{
    int 	mib[] = { CTL_NET, PF_INET6, IPPROTO_IPV6, 0 };
    size_t 	size;

    mib[3] = code;
    size = sizeof(*ret_value_p);
    if (sysctl(mib, sizeof(mib)/sizeof(mib[0]), ret_value_p, &size, NULL, 0)
	< 0) {
	my_log(LOG_ERR, "inet6_sysctl_get_int(%d) failed, %m", code);
	return (FALSE);
    }
    return (TRUE);
}

PRIVATE_EXTERN boolean_t
inet6_forwarding_is_enabled(void)
{
    int		enabled = 0;

    if (inet6_sysctl_get_int(IPV6CTL_FORWARDING, &enabled) == FALSE) {
	return (FALSE);
    }
    return (enabled != 0);
}

static char *
get_if_info(int if_index, int af, int * ret_len_p)
{
    char *			buf = NULL;
    size_t			buf_len = 0;
    int				mib[6];

    mib[0] = CTL_NET;
    mib[1] = PF_ROUTE;
    mib[2] = 0;
    mib[3] = af;
    mib[4] = NET_RT_IFLIST;
    mib[5] = if_index;

    *ret_len_p = 0;
    if (sysctl(mib, 6, NULL, &buf_len, NULL, 0) < 0) {
	fprintf(stderr, "sysctl() size failed: %s", strerror(errno));
	goto failed;
    }
    buf_len *= 2; /* just in case something changes */
    buf = malloc(buf_len);
    if (sysctl(mib, 6, buf, &buf_len, NULL, 0) < 0) {
	free(buf);
	buf = NULL;
	fprintf(stderr, "sysctl() failed: %s", strerror(errno));
	goto failed;
    }
    *ret_len_p = buf_len;

 failed:
    return (buf);
}

PRIVATE_EXTERN void
inet6_addrlist_copy(inet6_addrlist_t * addr_list_p, int if_index)
{
    int				addr_index = 0;
    char *			buf = NULL;
    char *			buf_end;
    int				buf_len;
    int				count;
    int				error;
    int				i;
    char			ifname[IFNAMSIZ + 1];
    inet6_addrinfo_t *		list = NULL;
    char *			scan;
    struct rt_msghdr *		rtm;
    int				s = -1;

    buf = get_if_info(if_index, AF_INET6, &buf_len);
    if (buf == NULL) {
	goto done;
    }
    buf_end = buf + buf_len;

    /* figure out how many IPv6 addresses there are */
    count = 0;
    ifname[0] = '\0';
    for (scan = buf; scan < buf_end; scan += rtm->rtm_msglen) {
	struct if_msghdr * 	ifm;

	rtm = (struct rt_msghdr *)scan;
	if (rtm->rtm_version != RTM_VERSION) {
	    continue;
	}
	switch (rtm->rtm_type) {
	case RTM_IFINFO:
	    ifm = (struct if_msghdr *)rtm;
	    if (ifm->ifm_addrs & RTA_IFP) {
		struct sockaddr_dl *	dl_p;
		
		dl_p = (struct sockaddr_dl *)(ifm + 1);
		if (dl_p->sdl_nlen == 0 
		    || dl_p->sdl_nlen >= sizeof(ifname)) {
		    goto done;
		}
		bcopy(dl_p->sdl_data, ifname, dl_p->sdl_nlen);
		ifname[dl_p->sdl_nlen] = '\0';
	    }
	    break;
	case RTM_NEWADDR:
	    count++;
	    break;
	default:
	    break;
	}
    }
    if (ifname[0] == '\0') {
	goto done;
    }
    if (count == 0) {
	goto done;
    }
    if (count > INET6_ADDRLIST_N_STATIC) {
	list = (inet6_addrinfo_t *)malloc(sizeof(*list) * count);
	if (list == NULL) {
	    goto done;
	}
    }
    else {
	list = addr_list_p->list_static;
    }
    for (scan = buf; scan < buf_end; scan += rtm->rtm_msglen) {
	boolean_t		got_address = FALSE;
	struct ifa_msghdr *	ifam;
	struct rt_addrinfo	info;

	rtm = (struct rt_msghdr *)scan;
	if (rtm->rtm_version != RTM_VERSION) {
	    continue;
	}
	if (rtm->rtm_type == RTM_NEWADDR) {
	    ifam = (struct ifa_msghdr *)rtm;
	    info.rti_addrs = ifam->ifam_addrs;
	    error = rt_xaddrs((char *)(ifam + 1),
			      ((char *)ifam) + ifam->ifam_msglen,
			      &info);
	    if (error) {
		fprintf(stderr, "couldn't extract rt_addrinfo %s (%d)\n",
			strerror(error), error);
		goto done;
	    }
	    for (i = 0; i < RTAX_MAX; i++) {
		struct sockaddr_in6 *	sin6_p;

		sin6_p = (struct sockaddr_in6 *)info.rti_info[i];
		if (sin6_p == NULL
		    || sin6_p->sin6_len < sizeof(struct sockaddr_in6)) {
		    continue;
		}
		switch (i) {
		case RTAX_NETMASK:
		    list[addr_index].prefix_length 
			= count_prefix_bits(&sin6_p->sin6_addr,
					    sizeof(sin6_p->sin6_addr));
		    break;
		case RTAX_IFA:
		    list[addr_index].addr = sin6_p->sin6_addr;
		    got_address = TRUE;
		    break;
		default:
		    break;
		}
	    }
	    if (got_address) {
		if (s < 0) {
		    s = inet6_dgram_socket();
		}
		if (s >= 0) {
		    siocgifaflag_in6(s, ifname, 
				     &list[addr_index].addr,
				     &list[addr_index].addr_flags);
		}
		addr_index++;
	    }
	}
    }
    if (addr_index == 0) {
	if (list != addr_list_p->list_static) {
	    free(list);
	}
	list = NULL;
    }

 done:
    if (s >= 0) {
	close(s);
    }
    if (buf != NULL) {
	free(buf);
    }
    addr_list_p->list = list;
    addr_list_p->count = addr_index;
    return;
}

PRIVATE_EXTERN void
inet6_addrlist_print(const inet6_addrlist_t * addr_list_p)
{
    int			i;
    inet6_addrinfo_t *	scan;

    for (i = 0, scan = addr_list_p->list; i < addr_list_p->count; i++, scan++) {
	char 	ntopbuf[INET6_ADDRSTRLEN];

	printf("%s/%d flags 0x%04x\n",
	       inet_ntop(AF_INET6, &scan->addr,
			 ntopbuf, sizeof(ntopbuf)),
	       scan->prefix_length,
	       scan->addr_flags);
    }
    return;
}

PRIVATE_EXTERN void
inet6_addrlist_free(inet6_addrlist_t * addr_list_p)
{
    if (addr_list_p->list == NULL) {
	return;
    }
    if (addr_list_p->list != addr_list_p->list_static) {
	free(addr_list_p->list);
    }
    inet6_addrlist_init(addr_list_p);
    return;
}

PRIVATE_EXTERN void
inet6_addrlist_init(inet6_addrlist_t * addr_list_p)
{
    addr_list_p->list = NULL;
    addr_list_p->count = 0;
    return;
}

PRIVATE_EXTERN boolean_t
inet6_addrlist_contains_address(const inet6_addrlist_t * addr_list_p,
				const inet6_addrinfo_t * addr)
{
    int			i;
    inet6_addrinfo_t *	scan;

    for (i = 0, scan = addr_list_p->list; i < addr_list_p->count; i++, scan++) {
	if (IN6_ARE_ADDR_EQUAL(&scan->addr, &addr->addr)
	    && (scan->prefix_length == addr->prefix_length)) {
	    return (TRUE);
	}
    }
    return (FALSE);
}

#if TEST_INET6_ADDRLIST
#include <stdio.h>
#include <stdlib.h>

PRIVATE_EXTERN int G_IPConfiguration_verbose = 1;
int
main(int argc, char * argv[])
{
    inet6_addrlist_t 	addresses;
    int			if_index;

    if (argc < 2) {
	fprintf(stderr, "you must specify the interface\n");
	exit(1);
    }
    if_index = if_nametoindex(argv[1]);
    if (if_index == 0) {
	fprintf(stderr, "No such interface '%s'\n", argv[1]);
	exit(2);
    }
    inet6_addrlist_copy(&addresses, if_index);
    inet6_addrlist_print(&addresses);
    inet6_addrlist_free(&addresses);
    exit(0);
    return(0);
}
#endif /* TEST_INET6_ADDRLIST */
