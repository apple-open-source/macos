/*
 * interface.c -- implements fetchmail 'interface' and 'monitor' commands
 *
 * This module was implemented by George M. Sipe <gsipe@pobox.com>
 * or <gsipe@acm.org> and is:
 *
 *	Copyright (c) 1996,1997 by George M. Sipe
 *
 *      FreeBSD specific portions written by and Copyright (c) 1999 
 *      Andy Doran <ad@psn.ie>.
 *
 * For license terms, see the file COPYING in this directory.
 */

#include "fetchmail.h"
#ifdef CAN_MONITOR

#include <sys/types.h>
#include <sys/param.h>

#if defined(linux)
#include <sys/utsname.h>
#endif

#include <stdio.h>
#include <string.h>
#if defined(STDC_HEADERS)
#include <stdlib.h>
#endif
#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#if defined(__FreeBSD__)
#if defined __FreeBSD_USE_KVM
#if __FreeBSD_version >= 300001
#include <net/if_var.h>
#endif
#include <kvm.h>
#include <nlist.h>
#include <sys/fcntl.h>
#else
#include <sys/sysctl.h>
#include <net/route.h>
#include <net/if_dl.h>
#endif
#endif
#include "socket.h"
#include "i18n.h"
#include "tunable.h"

typedef struct {
	struct in_addr addr, dstaddr, netmask;
	int rx_packets, tx_packets;
} ifinfo_t;

struct interface_pair_s {
	struct in_addr interface_address;
	struct in_addr interface_mask;
} *interface_pair;

/*
 * Count of packets to see on an interface before monitor considers it up.
 * Needed because when pppd shuts down the link, the packet counts go up
 * by two (one rx and one tx?, maybe).  A value of 2 seems to do the trick,
 * but we'll give it some extra.
 */
#define MONITOR_SLOP		5

#ifdef linux
#define have_interface_init

static char *netdevfmt;

void interface_init(void)
/* figure out which /proc/net/dev format to use */
{
    struct utsname utsname;

    /* Linux 2.2 -- transmit packet count in 10th field */
    netdevfmt = "%d %d %*d %*d %*d %d %*d %*d %*d %d %*d %*d %d";

    if (uname(&utsname) < 0)
        return;
    else
    {
	int major, minor;

	if (sscanf(utsname.release, "%d.%d.%*d", &major, &minor) >= 2
					&& (major < 2 || (major == 2 && minor < 2)))
	    /* pre-linux-2.2 format -- transmit packet count in 8th field */
	    netdevfmt = "%d %d %*d %*d %*d %d %*d %d %*d %*d %*d %*d %d";
    }
}

static int _get_ifinfoGT_(int socket_fd, FILE *stats_file, const char *ifname,
		ifinfo_t *ifinfo)
/* get active network interface information - return non-zero upon success */
{
	int namelen = strlen(ifname);
	struct ifreq request;
	char *cp, buffer[256];
	int found = 0;
	int counts[4];

	/* initialize result */
	memset((char *) ifinfo, 0, sizeof(ifinfo_t));

	/* get the packet I/O counts */
	while (fgets(buffer, sizeof(buffer) - 1, stats_file)) {
		for (cp = buffer; *cp && *cp == ' '; ++cp);
		if (!strncmp(cp, ifname, namelen) &&
				cp[namelen] == ':') {
			cp += namelen + 1;
			if (sscanf(cp, netdevfmt,
				   counts, counts+1, counts+2, 
				   counts+3,&found)>4) { /* found = dummy */
			        /* newer kernel with byte counts */
			        ifinfo->rx_packets=counts[1];
			        ifinfo->tx_packets=counts[3];
			} else {
			        /* older kernel, no byte counts */
			        ifinfo->rx_packets=counts[0];
			        ifinfo->tx_packets=counts[2];
			}
                        found = 1;
		}
	}
        if (!found) return (FALSE);

	/* see if the interface is up */
	strcpy(request.ifr_name, ifname);
	if (ioctl(socket_fd, SIOCGIFFLAGS, &request) < 0)
		return(FALSE);
	if (!(request.ifr_flags & IFF_RUNNING))
		return(FALSE);

	/* get the (local) IP address */
	strcpy(request.ifr_name, ifname);
	if (ioctl(socket_fd, SIOCGIFADDR, &request) < 0)
		return(FALSE);
	ifinfo->addr = ((struct sockaddr_in *) (&request.ifr_addr))->sin_addr;

	/* get the PPP destination (remote) IP address */
	ifinfo->dstaddr.s_addr = 0;
	strcpy(request.ifr_name, ifname);
	if (ioctl(socket_fd, SIOCGIFDSTADDR, &request) >= 0)
		ifinfo->dstaddr = ((struct sockaddr_in *)
					(&request.ifr_dstaddr))->sin_addr;

	/* get the netmask */
	strcpy(request.ifr_name, ifname);
	if (ioctl(socket_fd, SIOCGIFNETMASK, &request) >= 0) {
          ifinfo->netmask = ((struct sockaddr_in *)
                             (&request.ifr_netmask))->sin_addr;
          return (TRUE);
        }

	return(FALSE);
}

static int get_ifinfo(const char *ifname, ifinfo_t *ifinfo)
{
	int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
	FILE *stats_file = fopen("/proc/net/dev", "r");
	int result;

	if (socket_fd < 0 || !stats_file)
		result = FALSE;
	else
	{
	    char	*sp = strchr(ifname, '/');
	    /* hide slash and trailing info from ifname */
	    if (sp)
		*sp = '\0';
	    result = _get_ifinfoGT_(socket_fd, stats_file, ifname, ifinfo);
	    if (sp)
		*sp = '/';
	}
	if (socket_fd >= 0)
	    SockClose(socket_fd);
	if (stats_file)
	    fclose(stats_file);	/* not checking should be safe, mode was "r" */
	return(result);
}

#elif defined __FreeBSD__

#if defined __FreeBSD_USE_KVM

static kvm_t *kvmfd;
static struct nlist symbols[] = 
{
	{"_ifnet"},
	{NULL}
};
static u_long	ifnet_savedaddr;
static gid_t	if_rgid;
static gid_t	if_egid;

void 
interface_set_gids(gid_t egid, gid_t rgid)
{
	if_rgid = rgid;
	if_egid = egid;
}

static int 
openkvm(void)
{
	if ((kvmfd = kvm_open(NULL, NULL, NULL, O_RDONLY, NULL)) == NULL)
		return FALSE;
	
	if (kvm_nlist(kvmfd, symbols) < 0)
		return FALSE;
	   
	if (kvm_read(kvmfd, (unsigned long) symbols[0].n_value, &ifnet_savedaddr, sizeof(unsigned long)) == -1)
		return FALSE;
		
	return TRUE;
}

static int 
get_ifinfo(const char *ifname, ifinfo_t *ifinfo)
{
	char			tname[16];
	char			iname[16];
	struct ifnet		ifnet;
	unsigned long   	ifnet_addr = ifnet_savedaddr;
#if __FreeBSD_version >= 300001
	struct ifnethead	ifnethead;
	struct ifaddrhead	ifaddrhead;
#endif
	struct ifaddr		ifaddr;
	unsigned long		ifaddr_addr;
	struct sockaddr		sa;
	unsigned long		sa_addr;
	uint			i;
	
	if (if_egid)
		setegid(if_egid);
	
	for (i = 0; ifname[i] && ifname[i] != '/' && i < sizeof(iname) - 1; i++)
		iname[i] = ifname[i];
		
	iname[i] = '\0';
	
	if (!kvmfd)
	{
		if (!openkvm())
		{
			report(stderr, 0, GT_("Unable to open kvm interface. Make sure fetchmail is SGID kmem."));
			if (if_egid)
				setegid(if_rgid);
			exit(1);
		}
	}

#if __FreeBSD_version >= 300001
	kvm_read(kvmfd, ifnet_savedaddr, (char *) &ifnethead, sizeof ifnethead);
	ifnet_addr = (u_long) ifnethead.tqh_first;
#else
	ifnet_addr = ifnet_savedaddr;
#endif

	while (ifnet_addr)
	{
		kvm_read(kvmfd, ifnet_addr, &ifnet, sizeof(ifnet));
		kvm_read(kvmfd, (unsigned long) ifnet.if_name, tname, sizeof tname);
		snprintf(tname + strlen(tname), sizeof(tname) - strlen(tname), "%d", ifnet.if_unit);

		if (!strcmp(tname, iname))
		{
			if (!(ifnet.if_flags & IFF_UP))
			{
				if (if_egid)
					setegid(if_rgid);
				return 0;
			}
				
			ifinfo->rx_packets = ifnet.if_ipackets;
			ifinfo->tx_packets = ifnet.if_opackets;

#if __FreeBSD_version >= 300001
			ifaddr_addr = (u_long) ifnet.if_addrhead.tqh_first;
#else
			ifaddr_addr = (u_long) ifnet.if_addrlist;
#endif
			
			while(ifaddr_addr)
			{
				kvm_read(kvmfd, ifaddr_addr, &ifaddr, sizeof(ifaddr));
				kvm_read(kvmfd, (u_long)ifaddr.ifa_addr, &sa, sizeof(sa));
				
				if (sa.sa_family != AF_INET)
				{
#if __FreeBSD_version >= 300001
					ifaddr_addr = (u_long) ifaddr.ifa_link.tqe_next;
#else
					ifaddr_addr = (u_long) ifaddr.ifa_next;
#endif
					continue;
				}
			
				ifinfo->addr.s_addr = *(u_long *)(sa.sa_data + 2);
				kvm_read(kvmfd, (u_long)ifaddr.ifa_dstaddr, &sa, sizeof(sa));
				ifinfo->dstaddr.s_addr = *(u_long *)(sa.sa_data + 2);
				kvm_read(kvmfd, (u_long)ifaddr.ifa_netmask, &sa, sizeof(sa));
				ifinfo->netmask.s_addr = *(u_long *)(sa.sa_data + 2);

				if (if_egid)
					setegid(if_rgid);

				return 1;
			}
			
			if (if_egid)
				setegid(if_rgid);
			
			return 0;
		}

#if __FreeBSD_version >= 300001
		ifnet_addr = (u_long) ifnet.if_link.tqe_next;
#else
		ifnet_addr = (unsigned long) ifnet.if_next;
#endif
	}

	if (if_egid)
		setegid(if_rgid);
	
	return 0;
}

#else /* Do not use KVM on FreeBSD */

/*
 * Expand the compacted form of addresses as returned via the
 * configuration read via sysctl().
 */

static void
rt_xaddrs(caddr_t cp, caddr_t cplim, struct rt_addrinfo *rtinfo)
{
    struct sockaddr *sa;
    int i;

#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

    memset(rtinfo->rti_info, 0, sizeof(rtinfo->rti_info));
    for (i = 0; (i < RTAX_MAX) && (cp < cplim); i++) {
	if ((rtinfo->rti_addrs & (1 << i)) == 0)
	    continue;
	rtinfo->rti_info[i] = sa = (struct sockaddr *)cp;
	ADVANCE(cp, sa);
    }

#undef ROUNDUP
#undef ADVANCE
}

static int
get_ifinfo(const char *ifname, ifinfo_t *ifinfo)
{
    uint		i;
    int			rc = 0;
    int			ifindex = -1;
    size_t		needed;
    char		*buf = NULL;
    char		*lim = NULL;
    char		*next = NULL;
    struct if_msghdr 	*ifm;
    struct ifa_msghdr 	*ifam;
    struct sockaddr_in 	*sin;
    struct sockaddr_dl 	*sdl;
    struct rt_addrinfo 	info;
    char		iname[16];
    int			mib[6];

    memset(ifinfo, 0, sizeof(ifinfo));

    /* trim interface name */

    for (i = 0; i < sizeof(iname) && ifname[i] && ifname[i] != '/'; i++)
	iname[i] = ifname[i];
	
    if (i == 0 || i == sizeof(iname))
    {
	report(stderr, GT_("Unable to parse interface name from %s"), ifname);
	return 0;
    }

    iname[i] = 0;


    /* get list of existing interfaces */

    mib[0] = CTL_NET;
    mib[1] = PF_ROUTE;
    mib[2] = 0;
    mib[3] = AF_INET;		/* Only IP addresses please. */
    mib[4] = NET_RT_IFLIST;
    mib[5] = 0;			/* List all interfaces. */


    /* Get interface data. */

    if (sysctl(mib, 6, NULL, &needed, NULL, 0) == -1)
    {
 	report(stderr, 
	    GT_("get_ifinfo: sysctl (iflist estimate) failed"));
	exit(1);
    }
    if ((buf = (char *)malloc(needed)) == NULL)
    {
 	report(stderr, 
	    GT_("get_ifinfo: malloc failed"));
	exit(1);
    }
    if (sysctl(mib, 6, buf, &needed, NULL, 0) == -1)
    {
 	report(stderr, 
	    GT_("get_ifinfo: sysctl (iflist) failed"));
	exit(1);
    }

    lim = buf+needed;


    /* first look for the interface information */

    next = buf;
    while (next < lim)
    {
	ifm = (struct if_msghdr *)next;
	next += ifm->ifm_msglen;

	if (ifm->ifm_version != RTM_VERSION) 
	{
 	    report(stderr, 
		GT_("Routing message version %d not understood."),
		ifm->ifm_version);
	    exit(1);
	}

	if (ifm->ifm_type == RTM_IFINFO)
	{
	    sdl = (struct sockaddr_dl *)(ifm + 1);

	    if (!(strlen(iname) == sdl->sdl_nlen 
		&& strncmp(iname, sdl->sdl_data, sdl->sdl_nlen) == 0))
	    {
		continue;
	    }

	    if ( !(ifm->ifm_flags & IFF_UP) )
	    {
		/* the interface is down */
		goto get_ifinfo_end;
	    }

	    ifindex = ifm->ifm_index;
	    ifinfo->rx_packets = ifm->ifm_data.ifi_ipackets;
	    ifinfo->tx_packets = ifm->ifm_data.ifi_opackets;

	    break;
	}
    }

    if (ifindex < 0)
    {
	/* we did not find an interface with a matching name */
	report(stderr, GT_("No interface found with name %s"), iname);
	goto get_ifinfo_end;
    }

    /* now look for the interface's IP address */

    next = buf;
    while (next < lim)
    {
	ifam = (struct ifa_msghdr *)next;
	next += ifam->ifam_msglen;

	if (ifindex > 0
	    && ifam->ifam_type == RTM_NEWADDR
	    && ifam->ifam_index == ifindex)
	{
	    /* Expand the compacted addresses */
	    info.rti_addrs = ifam->ifam_addrs;
	    rt_xaddrs((char *)(ifam + 1), 
			ifam->ifam_msglen + (char *)ifam,
	  		&info);

	    /* Check for IPv4 address information only */
	    if (info.rti_info[RTAX_IFA]->sa_family != AF_INET)
	    {
		continue;
	    }

	    rc = 1;

	    sin = (struct sockaddr_in *)info.rti_info[RTAX_IFA];
	    if (sin)
	    {
		ifinfo->addr = sin->sin_addr;
	    }

	    sin = (struct sockaddr_in *)info.rti_info[RTAX_NETMASK];
	    if (!sin)
	    {
		ifinfo->netmask = sin->sin_addr;
	    }

	    /* note: RTAX_BRD contains the address at the other
	     * end of a point-to-point link or the broadcast address
	     * of non point-to-point link
	     */
	    sin = (struct sockaddr_in *)info.rti_info[RTAX_BRD];
	    if (!sin)
	    {
		ifinfo->dstaddr = sin->sin_addr;
	    }

	    break;
	}
    }

    if (rc == 0)
    {
	report(stderr, GT_("No IP address found for %s"), iname);
    }

get_ifinfo_end:
    free(buf);
    return rc;
}

#endif /* __FREEBSD_USE_SYSCTL_GET_IFFINFO */

#endif

#ifndef HAVE_INET_ATON
/*
 * Note: This is not a true replacement for inet_aton(), as it won't
 * do the right thing on "255.255.255.255" (which translates to -1 on
 * most machines).  Fortunately this code will be used only if you're
 * on an older Linux that lacks a real implementation.
 */
#ifdef HAVE_NETINET_IN_SYSTM_H
# include <sys/types.h>
# include <netinet/in_systm.h>
#endif

#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <string.h>

static int inet_aton(const char *cp, struct in_addr *inp) {
    long addr;

    addr = inet_addr(cp);
    if (addr == ((long) -1)) return 0;

    memcpy(inp, &addr, sizeof(addr));
    return 1;
}
#endif /* HAVE_INET_ATON */

void interface_parse(char *buf, struct hostdata *hp)
/* parse 'interface' specification */
{
	char *cp1, *cp2;

	hp->interface = xstrdup(buf);

	/* find and isolate just the IP address */
	if (!(cp1 = strchr(buf, '/')))
	{
		(void) report(stderr,
			      GT_("missing IP interface address\n"));
		exit(PS_SYNTAX);
	}
	*cp1++ = '\000';

	/* find and isolate just the netmask */
	if (!(cp2 = strchr(cp1, '/')))
		cp2 = "255.255.255.255";
	else
		*cp2++ = '\000';

	/* convert IP address and netmask */
	hp->interface_pair = (struct interface_pair_s *)xmalloc(sizeof(struct interface_pair_s));
	if (!inet_aton(cp1, &hp->interface_pair->interface_address))
	{
		(void) report(stderr,
			      GT_("invalid IP interface address\n"));
		exit(PS_SYNTAX);
	}
	if (!inet_aton(cp2, &hp->interface_pair->interface_mask))
	{
		(void) report(stderr,
			      GT_("invalid IP interface mask\n"));
		exit(PS_SYNTAX);
	}
	/* apply the mask now to the IP address (range) required */
	hp->interface_pair->interface_address.s_addr &=
		hp->interface_pair->interface_mask.s_addr;

	/* restore original interface string (for configuration dumper) */
	*--cp1 = '/';
	return;
}

void interface_note_activity(struct hostdata *hp)
/* save interface I/O counts */
{
	ifinfo_t ifinfo;
	struct query *ctl;

	/* if not monitoring link, all done */
	if (!hp->monitor)
		return;

	/* get the current I/O stats for the monitored link */
	if (get_ifinfo(hp->monitor, &ifinfo))
		/* update this and preceeding host entries using the link
		   (they were already set during this pass but the I/O
		   count has now changed and they need to be re-updated)
		*/
		for (ctl = querylist; ctl; ctl = ctl->next) {
			if (ctl->server.monitor && !strcmp(hp->monitor, ctl->server.monitor))
				ctl->server.monitor_io =
					ifinfo.rx_packets + ifinfo.tx_packets;
			/* do NOT update host entries following this one */
			if (&ctl->server == hp)
				break;
		}

#ifdef	ACTIVITY_DEBUG
	(void) report(stdout, 
		      GT_("activity on %s -noted- as %d\n"), 
		      hp->monitor, hp->monitor_io);
#endif
}

int interface_approve(struct hostdata *hp, flag domonitor)
/* return TRUE if OK to poll, FALSE otherwise */
{
	ifinfo_t ifinfo;

	/* check interface IP address (range), if specified */
	if (hp->interface) {
		/* get interface info */
		if (!get_ifinfo(hp->interface, &ifinfo)) {
			(void) report(stdout, 
				      GT_("skipping poll of %s, %s down\n"),
				      hp->pollname, hp->interface);
			return(FALSE);
		}
		/* check the IP addresses (range) */
		if	(!(
				/* check remote IP address */
				((ifinfo.dstaddr.s_addr != 0) &&
				(ifinfo.dstaddr.s_addr &
				hp->interface_pair->interface_mask.s_addr) ==
				hp->interface_pair->interface_address.s_addr)
				||
				/* check local IP address */
				((ifinfo.addr.s_addr &
				hp->interface_pair->interface_mask.s_addr) ==
				hp->interface_pair->interface_address.s_addr)
			) )
		{
			(void) report(stdout,
				GT_("skipping poll of %s, %s IP address excluded\n"),
				hp->pollname, hp->interface);
			return(FALSE);
		}
	}

	/* if not monitoring link, all done */
	if (!domonitor || !hp->monitor)
		return(TRUE);

#ifdef	ACTIVITY_DEBUG
	(void) report(stdout, 
		      GT_("activity on %s checked as %d\n"), 
		      hp->monitor, hp->monitor_io);
#endif
	/* if monitoring, check link for activity if it is up */
	if (get_ifinfo(hp->monitor, &ifinfo))
	{
	    int diff = (ifinfo.rx_packets + ifinfo.tx_packets)
							- hp->monitor_io;

	    /*
	     * There are three cases here:
	     *
	     * (a) If the new packet count is less than the recorded one,
	     * probably pppd was restarted while fetchmail was running.
	     * Don't skip.
	     *
	     * (b) newpacket count is greater than the old packet count,
	     * but the difference is small and may just reflect the overhead
	     * of a link shutdown.  Skip.
	     *
	     * (c) newpacket count is greater than the old packet count,
	     * and the difference is large. Connection is live.  Don't skip.
	     */
	    if (diff >= 0 && diff <= MONITOR_SLOP)
	    {
		(void) report(stdout, 
			      GT_("skipping poll of %s, %s inactive\n"),
			      hp->pollname, hp->monitor);
		return(FALSE);
	    }
	}

#ifdef ACTIVITY_DEBUG
       report(stdout, GT_("activity on %s was %d, is %d\n"),
             hp->monitor, hp->monitor_io,
             ifinfo.rx_packets + ifinfo.tx_packets);
#endif

	return(TRUE);
}
#endif /* CAN_MONITOR */

#ifndef have_interface_init
void interface_init(void) {}
#endif
