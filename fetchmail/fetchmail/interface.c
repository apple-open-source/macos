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
 * This is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; version 2, or (at your option) any later version.
 */
#include <sys/types.h>
#include <sys/param.h>

#if (defined(linux) && !defined(INET6)) || defined(__FreeBSD__)

#include "config.h"
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
#if __FreeBSD_version >= 300001
#include <net/if_var.h>
#endif
#include <kvm.h>
#include <nlist.h>
#include <sys/fcntl.h>
#endif
#include "config.h"
#include "fetchmail.h"
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

static char *netdevfmt;

/*
 * Count of packets to see on an interface before monitor considers it up.
 * Needed because when pppd shuts down the link, the packet counts go up
 * by two (one rx and one tx?, maybe).  A value of 2 seems to do the trick,
 * but we'll give it some extra.
 */
#define MONITOR_SLOP		5

#if defined(linux)

void interface_init(void)
/* figure out which /proc/dev/net format to use */
{
    FILE *fp = popen("uname -r", "r");	/* still wins if /proc is out */

    /* pre-linux-2.2 format -- transmit packet count in 8th field */
    netdevfmt = "%d %d %*d %*d %*d %d %*d %d %*d %*d %*d %*d %d";

    if (!fp)
	return;
    else
    {
	int major, minor;

	if (fscanf(fp, "%d.%d.%*d", &major, &minor) != 2)
	    return;

	if (major >= 2 && minor >= 2)
	    /* Linux 2.2 -- transmit packet count in 10th field */
	    netdevfmt = "%d %d %*d %*d %*d %d %*d %*d %*d %d %*d %*d %d";
    }
}

static int _get_ifinfo_(int socket_fd, FILE *stats_file, const char *ifname,
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

	/* get the IP address */
	strcpy(request.ifr_name, ifname);
	if (ioctl(socket_fd, SIOCGIFADDR, &request) < 0)
		return(FALSE);
	ifinfo->addr = ((struct sockaddr_in *) (&request.ifr_addr))->sin_addr;

	/* get the PPP destination IP address */
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

	    if (sp)
		*sp = '\0';
	    result = _get_ifinfo_(socket_fd, stats_file, ifname, ifinfo);
	    if (sp)
		*sp = '/';
	}
	if (socket_fd >= 0)
		close(socket_fd);
	if (stats_file)
		fclose(stats_file);
	return(result);
}

#elif defined __FreeBSD__

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
	char            	tname[16];
	char			iname[16];
	struct ifnet   	        ifnet;
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
	
	for (i = 0; ifname[i] && ifname[i] != '/'; i++)
		iname[i] = ifname[i];
		
	iname[i] = '\0';
	
	if (!kvmfd)
	{
		if (!openkvm())
		{
			report(stderr, 0, _("Unable to open kvm interface. Make sure fetchmail is SGID kmem."));
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
		snprintf(tname, sizeof tname - 1, "%s%d", tname, ifnet.if_unit);

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
#endif /* defined __FreeBSD__ */


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
			      _("missing IP interface address\n"));
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
			      _("invalid IP interface address\n"));
		exit(PS_SYNTAX);
	}
	if (!inet_aton(cp2, &hp->interface_pair->interface_mask))
	{
		(void) report(stderr,
			      _("invalid IP interface mask\n"));
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
		      _("activity on %s -noted- as %d\n"), 
		      hp->monitor, hp->monitor_io);
#endif
}

int interface_approve(struct hostdata *hp)
/* return TRUE if OK to poll, FALSE otherwise */
{
	ifinfo_t ifinfo;

	/* check interface IP address (range), if specified */
	if (hp->interface) {
		/* get interface info */
		if (!get_ifinfo(hp->interface, &ifinfo)) {
			(void) report(stdout, 
				      _("skipping poll of %s, %s down\n"),
				      hp->pollname, hp->interface);
			return(FALSE);
		}
		/* check the IP address (range) */
		if ((ifinfo.addr.s_addr &
				hp->interface_pair->interface_mask.s_addr) !=
				hp->interface_pair->interface_address.s_addr) {
			(void) report(stdout,
				_("skipping poll of %s, %s IP address excluded\n"),
				hp->pollname, hp->interface);
			return(FALSE);
		}
	}

	/* if not monitoring link, all done */
	if (!hp->monitor)
		return(TRUE);

#ifdef	ACTIVITY_DEBUG
	(void) report(stdout, 
		      _("activity on %s checked as %d\n"), 
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
			      _("skipping poll of %s, %s inactive\n"),
			      hp->pollname, hp->monitor);
		return(FALSE);
	    }
	}

#ifdef ACTIVITY_DEBUG
       report(stdout, _("activity on %s was %d, is %d\n"),
             hp->monitor, hp->monitor_io,
             ifinfo.rx_packets + ifinfo.tx_packets);
#endif

	return(TRUE);
}
#endif /* (defined(linux) && !defined(INET6)) || defined(__FreeBSD__) */
