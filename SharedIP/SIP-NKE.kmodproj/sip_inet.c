/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* Copyright (c) 1997-2000 Apple Computer, Inc. All Rights Reserved */
/*
 * The Darwin (Apple Public Source) license specifies the terms
 * and conditions for redistribution.
 *
 * Support for networking from Classic (and other 'users'):
 *  AppleTalk: OT and X have separate stacks and addresses
 *  IPv4: OT and X have separate stacks, but share the same IP address(es)
 * This is the IPv4 support module
 *
 * Justin Walker, 991112
 * 000824: separate device-specific functions to support multiple
 *	   device families (for now, ethernet, PPP)
 */

/*
 * TODO:
 *  - Simplify/remove the 'ifb' silliness
 *  - Deal with frags that are smaller than we expect
 */
#include <sys/kdebug.h>
#if KDEBUG

#define DBG_SPLT_BFCHK  DRVDBG_CODE(DBG_DRVSPLT, 0)
#define DBG_SPLT_APPND  DRVDBG_CODE(DBG_DRVSPLT, 1)
#define DBG_SPLT_MBUF   DRVDBG_CODE(DBG_DRVSPLT, 2)
#define DBG_SPLT_DUP    DRVDBG_CODE(DBG_DRVSPLT, 3)
#define DBG_SPLT_PAD    DRVDBG_CODE(DBG_DRVSPLT, 4)

#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/socketvar.h>
#include <sys/fcntl.h>
#include <sys/queue.h>
#include <sys/domain.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <net/route.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/ndrv.h>
#include <net/kext_net.h>
#include <net/dlil.h>
#include <net/ethernet.h>
#include <net/netisr.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_pcb.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <net/if_arp.h>
#include <machine/spl.h>
#include <kern/thread.h>

#include "SharedIP.h"
#include "sip.h"

#include <sys/syslog.h>

#define DEBUG_FRAGS 0
#if DEBUG_FRAGS
int frag_header_count = 0;
int frag_count = 0;
int total_frag_header_count = 0;
#endif

/*
 * IP Fragment handling:
 * Maintain a tailq of fraghead structures, each of which
 *  heads a list of mbuf chains which are the fragments
 *  of an IP datagram.
 * The 'fraghead' includes an offset indicator, to be used as a
 *  possible optimization to avoid timeouts on a completed fragment
 *  list.
 */
struct fraghead
{	TAILQ_ENTRY(fraghead) fh_link;
	TAILQ_HEAD(mblist, mbuf) fh_frags; /* The packets */
	struct in_addr fh_laddr, fh_faddr; /* The IP addrs */
	int fh_offset;		/* Last-seen contigous offset, 0, or -1 */
	int fh_fsize;		/* Frame header size */
	char *fh_frame;		/* The frame header */
	struct inpcbinfo *fh_pcbinfo; /* udbinfo, tcbinfo, or null */
	unsigned short fh_id;	/* ip_id from frags */
	unsigned char fh_ttl;	/* time remaining for this fragged dgram */
	unsigned char fh_proto;	/* IPPROTO_XXX */
	unsigned char fh_owner;	/* Who gets the frags */
};

/* In support of port sharing */
extern struct inpcbinfo udbinfo;
extern struct inpcbinfo tcbinfo;

extern int hz;		/* in clock.c (yuck) */

int check_icmp(struct icmp *);
int not4us(struct ip *, struct ifnet *);
int validate_addrs(struct BlueFilter *, struct blueCtlBlock *);
int handle_frags(struct mbuf *, char *, int, struct blueCtlBlock *, int);
void sip_fragtimer(void *);
extern struct if_proto *dlttoproto(u_long);

/*
 * Get user's filtering info; prep for state check in caller.  We
 *  can modify state by registering for new ownership id's.
 * Requested addresses are validated against those registered for
 *  the requested interface.
 */
int
enable_ipv4(struct BlueFilter *bf, void *data, struct blueCtlBlock *ifb)
{	int retval;
        struct sockaddr_in inet_sockaddr;
	struct ifnet *ifp;
	struct ifaddr *ifa;
	struct sockaddr_dl *sd;

	retval = copyin(data, &inet_sockaddr,
			sizeof (struct sockaddr_in));
	if (retval)
		return (-retval);

	bf->BF_address = inet_sockaddr.sin_addr.s_addr;
#if SIP_DEBUG_INFO
	log(LOG_WARNING, "ENABLE IP: %x\n", bf->BF_address);
#endif
	/*
	 * Get the current IP Filter
	 * Note: old "y adapter" way
	 *
	 * For SIP_PROTO_RCV_SHARED, inet_sockaddr.sin_addr.s_addr
	 *  is not useful.
	 */

	/* registration of ownership for BlueBox TCP and UDP port use */

        if (bf->BF_flags & SIP_PROTO_RCV_SHARED) {
	    if (ifb->udp_blue_owned || ifb->tcp_blue_owned)
		return(-EBUSY);
	    if (validate_addrs(bf, ifb))
		return(-EINVAL);
            if ((retval = in_pcb_new_share_client(&udbinfo,
						 &ifb->udp_blue_owned)) != 0)
		return (-retval);
            if ((retval = in_pcb_new_share_client(&tcbinfo,
						 &ifb->tcp_blue_owned)) != 0)
	    {	if (in_pcb_rem_share_client(&tcbinfo,
					    ifb->udp_blue_owned) == 0)
		    ifb->udp_blue_owned = 0;
		return (-retval);
	    }
        } else
	    return(-EOPNOTSUPP);

	ifp = ((struct ndrv_cb *)ifb->ifb_so->so_pcb)->nd_if;
	ifa = ifp->if_addrhead.tqh_first;
	while (ifa)
	{	if ((sd = (struct sockaddr_dl *)ifa->ifa_addr)
		     && sd->sdl_family == AF_LINK)
		{	register unsigned char *p;
			if (sd->sdl_type == IFT_ETHER)
			{	MALLOC(p, char *, 6, M_TEMP, M_WAITOK);
				bcopy(&sd->sdl_data[sd->sdl_nlen], p, 6);
				ifb->media_addr_size = 6;
				ifb->dev_media_addr = p;
				break;
			}

			if (sd->sdl_type == IFT_PPP)
			{	ifb->media_addr_size = 0;
				/* No media address */
				ifb->dev_media_addr = NULL;
				break;
			}
		}
		ifa = ifa->ifa_link.tqe_next;
	}
	if (ifa)
	{	int s;

		s = splnet();
		if ((retval = ipv4_attach_protofltr(ifp, ifb)) != 0)
		{
#if SIP_DEBUG_ERR
			log(LOG_WARNING,
			    "enable_ipv4: failed, ifb=%d retval=%d\n",
			    ifb, retval);
#endif
			splx(s);
			return(-retval);
		}
		return(BFS_IP);
	} else
	{	if (in_pcb_rem_share_client(&tcbinfo,
					    ifb->udp_blue_owned) == 0)
			ifb->udp_blue_owned = 0;
		if (in_pcb_rem_share_client(&tcbinfo,
					    ifb->tcp_blue_owned) == 0)
			ifb->tcp_blue_owned = 0;
		return(-EINVAL);
	}
}

/*
 * This filter function intercepts incoming packets being delivered to
 *  IPv4 (from a PPP interface) and decides if they should be sent to
 *  the Client.
 * Note: the packet is "fully formed", i.e., has a media header.
 * NB: Assumes the X stack is operational
 */
int  ipv4_ppp_infltr(caddr_t cookie,
		 struct mbuf   **m_orig,
		 char          **pppheader,
		 struct ifnet  **ifnet_ptr)
{	register struct blueCtlBlock *ifb = (struct blueCtlBlock *)cookie;
	register struct BlueFilter *bf;
	register unsigned char *p;
	struct mbuf *m0 = NULL, *m = *m_orig;
	unsigned int lcookie, hlen, total;
	int retval;
	unsigned char owner;
	struct ip *ip;

	if ( m->m_pkthdr.len < FILTER_LEN)
		return(0);

	while ((m->m_len < FILTER_LEN) && m->m_next) {
		total = m->m_len + (m->m_next)->m_len;
		if ((m = m_pullup(m, min(FILTER_LEN, total))) == 0)
			return(EJUSTRETURN);
	}

	bf = &ifb->filter[BFS_IP];  /* find v4 filter */

	/*
	 * We have ptrs to the beginning of the proto packet
	 *  and the media header; we need to prep 'm' for
	 *  the possibility of duplication, since the user
	 *  will be expecting the same *&^%$#@ media header.
	 */
	p = mtod(m, unsigned char *); /* Ptr to "pkt" hdr */
	//	MDATA_PPP_START(m);
	if (bf->BF_flags)	/* Filtering IP */
	{	/* We know it's an IP packet */

		ip = (struct ip *)p;
		hlen = ip->ip_hl << 2;
	
		/* First, check for fragmented packet */
		if (ip->ip_off & (IP_MF | IP_OFFMASK | IP_RF))
		{	if ((retval =
			    handle_frags(m, NULL, 2, ifb, 0)) != 0)
				return(retval);
			goto done1; /* Ack!!! */
		}
		if (((struct ip *)p)->ip_p == IPPROTO_TCP)
		{	struct tcphdr *tp;
	
			tp = (struct tcphdr *)(p+hlen);


#ifdef DONT_DO_LOG
log(LOG_WARNING, "TCP IN: Looking up %x\n", tp->th_dport);
#endif
			owner = in_pcb_get_owner (&tcbinfo,
						  ip->ip_dst,
						  tp->th_dport,
						  ip->ip_src,
						  tp->th_sport,
						  &lcookie);
			/*
			 * TCP ports are not shared across
			 *  worlds.  If it's owned by our
			 *  user, let him have it.  Otherwise
			 *  hand it on for further processing.
			 */
			if (owner == ifb->tcp_blue_owned)
			{	m0 = m;
				m = NULL;
			} else if (owner == INPCB_NO_OWNER)
			{
#if SIP_DEBUG_INFO	
				log(LOG_WARNING,
				    "TCP IN: No owner for %x:%x <- %x:%x\n",
				    ip->ip_dst,
				    tp->th_dport,
				    ip->ip_src,
				    tp->th_sport);
#endif			
			}
#ifdef DONT_DO_LOG
log(LOG_WARNING, "TCP IN: Owner sez %x/%x (%x)\n",
    owner, lcookie, ifb->tcp_blue_owned);
#endif
		} else if (((struct ip *)p)->ip_p == IPPROTO_UDP)
		{	struct udphdr *up;
		
			up = (struct udphdr *)(p+hlen);

#ifdef DONT_DO_LOG
log(LOG_WARNING, "UDP IN: Looking up %x\n", up->uh_dport);
#endif
			owner = in_pcb_get_owner (&udbinfo,
						  ip->ip_dst,
						  up->uh_dport,
						  ip->ip_src,
						  up->uh_sport,
						  &lcookie);
			/*
			 * UDP ports can be shared (multicast)
			 * If only our user then give it to
			 *  him; otherwise, if we're one
			 *  of the owners, dup; else
			 *  let the X-side deal with it.
			 */
			if (owner == ifb->udp_blue_owned)
			{	m0 = m;
				m = NULL;
			} else if (owner & ifb->udp_blue_owned)
				m0 = m_dup(m, M_NOWAIT);
			else if (owner == INPCB_NO_OWNER)
			{
#if SIP_DEBUG_INFO	
				log(LOG_WARNING,
				    "UDP IN: No owner for %x:%x <- %x:%x\n",
				    ip->ip_dst,
				    up->uh_dport,
				    ip->ip_src,
				    up->uh_sport);
#endif			
			}
#ifdef DONT_DO_LOG
log(LOG_WARNING, "UDP IN: Owner sez %x, %x, %x\n",
    owner, who, ifb->udp_blue_owned);
#endif
		} else if (((struct ip *)p)->ip_p == IPPROTO_ICMP)
		{	struct icmp *ih;
		
			ih = (struct icmp *)(p + hlen);
		
			if (ih->icmp_type == ICMP_ECHO ||
			    ih->icmp_type == ICMP_IREQ ||
			    ih->icmp_type == ICMP_MASKREQ ||
			    ih->icmp_type == ICMP_TSTAMP)
				; /* For X */
			else		/* Both get it! */
				m0 = m_dup(m, M_NOWAIT);
		} else		/* Both get it! */
			m0 = m_dup(m, M_NOWAIT);
	}
	if (m0)
		blue_inject(ifb, m0);

	if (m == NULL)
		return(EJUSTRETURN);
 done1:
	//	MDATA_PPP_END(m);
	*m_orig = m;
	return (0);
}

int
ipv4_ppp_outfltr(caddr_t cookie,
	     struct mbuf   **m_orig,
	     struct ifnet  **ifnet_ptr,
	     struct sockaddr **dest,
	     char *dest_linkaddr,
	     char *frame_type)
{
	register struct blueCtlBlock *ifb = (struct blueCtlBlock *)cookie;
	register struct BlueFilter *bf;
	register unsigned char *p;
	struct mbuf * m0 = NULL, *m = *m_orig;
	int total, retval;
	unsigned int lcookie, hlen;
	unsigned char owner;
	struct ip *ip;

	if (FILTER_LEN > m->m_pkthdr.len)
		return(0);

	while ((FILTER_LEN > m->m_len) && m->m_next) {
		total = m->m_len + (m->m_next)->m_len;
		if ((m = m_pullup(m, min(FILTER_LEN, total))) == 0)
			return(EJUSTRETURN);
	}

	bf = &ifb->filter[BFS_IP];  /* find IPv4 filter */

	if (bf->BF_flags)	/* Filtering IPv4 */
	{	p = (unsigned char *)m->m_data;
		/* Know we're dealilng with an IP packet */
		ip = (struct ip *)p;
		hlen = ip->ip_hl << 2;
		/*
		 * NB: Bailing out here!
		 */
		if (not4us(ip, *ifnet_ptr))
		{	*m_orig = m;
			return(0);
		}
		if (IN_CLASSD(ip->ip_dst.s_addr)) /* Mcast */
			m0 = m_dup(m, M_NOWAIT);
		else if (ip->ip_off & (IP_MF | IP_OFFMASK | IP_RF))
		{	if ((retval = handle_frags(m, dest_linkaddr, 2, ifb, 1)) != 0)
				return(retval);
			goto done2;
		} else if (ip->ip_p == IPPROTO_TCP)
		{	struct tcphdr *tp;
		
			tp = (struct tcphdr *)((char *)ip+hlen);

#ifdef DONT_DO_LOG
log(LOG_WARNING, "TCP OUT: Looking up %x\n", tp->th_dport);
#endif
			/*
			 * TCP ports are not shared across
			 *  worlds.  If it's owned by our
			 *  user, let him have it.  Otherwise
			 *  hand it on for further processing.
			 */
			owner = in_pcb_get_owner (&tcbinfo,
						  ip->ip_dst,
						  tp->th_dport,
						  ip->ip_src,
						  tp->th_sport,
						  &lcookie);
			if (owner == ifb->tcp_blue_owned)
			{
#if 0
				unsigned long *l = (unsigned long *)ip;
#endif

				m0 = m;
				m = NULL;
	
#if 0
				log(LOG_WARNING, "O1: %x %x %x %x %x\n",
				    *(l+4), *(l+5), *(l+6),
				    *(l+7), *(l+8)&0xff0000);
#endif
			} else if (owner == INPCB_NO_OWNER)
			{
#if SIP_DEBUG_INFO
				log(LOG_WARNING,
				    "TCP OUT: No owner for %x:%x <- %x:%x\n",
				    ip->ip_dst,
				    tp->th_dport,
				    ip->ip_src,
				    tp->th_sport);
#endif			
			}
#ifdef DONT_DO_LOG
log(LOG_WARNING, "TCP OUT: Owner sez %x/%x (%x)\n",
    owner, lcookie, ifb->tcp_blue_owned);
#endif
		} else if (ip->ip_p == IPPROTO_UDP)
		{	struct udphdr *up;
		
			up = (struct udphdr *)((char *)ip+hlen);

#ifdef DONT_DO_LOG
log(LOG_WARNING, "UDP OUT: Looking up %x\n", up->uh_dport);
#endif
			owner = in_pcb_get_owner (&udbinfo,
						  ip->ip_dst,
						  up->uh_dport,
						  ip->ip_src,
						  up->uh_sport,
						  &lcookie);
		
			/*
			 * UDP ports can be shared (multicast)
			 * If only our user then give it to
			 *  him; otherwise, if we're one
			 *  of the owners, dup; else
			 *  let the X-side deal with it.
			 * In the other cases, give it to both
			 */
			if (owner == ifb->udp_blue_owned)
			{	m0 = m;
				m = NULL;
			} else {
				m0 = m_dup(m, M_NOWAIT);
#if SIP_DEBUG_INFO
				log(LOG_WARNING,
				    "UDP OUT: Sending to both %x:%x <- %x:%x\n",
				    ip->ip_dst,
				    up->uh_dport,
				    ip->ip_src,
				    up->uh_sport);
#endif			
			}
#ifdef DONT_DO_LOG
log(LOG_WARNING, "UDP OUT: Owner sez %x, %x\n", owner, ifb->udp_blue_owned);
#endif
		} else if (ip->ip_p == IPPROTO_ICMP)
		{	struct icmp *ih;
		
			ih = (struct icmp *)(p + hlen);
		
			if (ih->icmp_type == ICMP_ECHO ||
			    ih->icmp_type == ICMP_IREQ ||
			    ih->icmp_type == ICMP_MASKREQ ||
			    ih->icmp_type == ICMP_TSTAMP ||
			    ((ih->icmp_type == ICMP_ECHOREPLY) &&
				 (ip->ip_dst.s_addr != ip->ip_src.s_addr)))
				; /* For Wire */
			else		/* Both get it! */
				m0 = m_dup(m, M_NOWAIT);
		} else          /* Both get it! */
			m0 = m_dup(m, M_NOWAIT);

		if (m0)
		{	blue_inject(ifb, m0); /* !!! Need PPP header */
		}
		if (m == NULL)
			return(EJUSTRETURN);
	}
	/* this is for MacOS X, DLIL will hand the mbuf to IPv4 stack. */
	/* put the packet back together as expected */

 done2:
	*m_orig = m;
	return (0);
}

/*
 * This filter function intercepts incoming packets being delivered to
 *  IPv4 (from an ethernet interface) and decides if they should be
 *  sent to the Client.
 * Note: the packet is "fully formed", i.e., has a media header.
 * NB: Assumes the X stack is operational
 */
int  ipv4_eth_infltr(caddr_t cookie,
		 struct mbuf   **m_orig,
		 char          **etherheader,
		 struct ifnet  **ifnet_ptr)
{	register struct blueCtlBlock *ifb = (struct blueCtlBlock *)cookie;
	register struct BlueFilter *bf;
	register unsigned char *p;
	struct mbuf *m0 = NULL, *m = *m_orig;
	unsigned int lcookie, hlen, total;
	int retval;
	unsigned char owner;
	struct ip *ip;

	if ( m->m_pkthdr.len < FILTER_LEN)
		return(0);

	while ((m->m_len < FILTER_LEN) && m->m_next) {
		total = m->m_len + (m->m_next)->m_len;
		if ((m = m_pullup(m, min(FILTER_LEN, total))) == 0)
			return(EJUSTRETURN);
	}

	bf = &ifb->filter[BFS_IP];  /* find v4 filter */

	/*
	 * We have ptrs to the beginning of the proto packet
	 *  and the media header; we need to prep 'm' for
	 *  the possibility of duplication, since the user
	 *  will be expecting the same *&^%$#@ media header.
	 */
	p = mtod(m, unsigned char *); /* Ptr to "pkt" hdr */
	MDATA_ETHER_START(m);
	if (bf->BF_flags)	/* Filtering IP */
	{	struct ether_header *eh = (struct ether_header *)*etherheader;

#ifdef DONT_DO_LOG
log(LOG_WARNING, "EI: ether type %x\n", eh->ether_type);
#endif

		if (eh->ether_type == ETHERTYPE_ARP)
		{	struct arphdr *ah = (struct arphdr *)p;

			/*
			 * For ARP, we just check for replies,
			 *  which we give to our 'user' and let
			 *  the X-side deal with as well.
			 * Other ops are handled by the X-side alone.
			 */
#ifdef DONT_DO_LOG
log(LOG_WARNING, "ARP: %x, %x, %x: %d\n",
    eh->ether_type, ah->ar_pro, ah->ar_op);
#endif
			if (ah->ar_pro == ETHERTYPE_IP)
			{	if (ah->ar_op == ARPOP_REPLY)
				{	/* Both will want to see this... */
					m0 = m_dup(m, M_NOWAIT);
					ifb->no_bufs2++;
				}
			}
		} else if (eh->ether_type == ETHERTYPE_IP)
		{	if (**etherheader & 0x01) /* DST is Mcast/Bcast */
				m0 = m_dup(m, M_NOWAIT);
			else
			{	ip = (struct ip *)p;
				hlen = ip->ip_hl << 2;

				/* First, check for fragmented packet */
				if (ip->ip_off & (IP_MF | IP_OFFMASK | IP_RF))
				{	if ((retval =
					    handle_frags(m, NULL, sizeof (struct ether_header), ifb, 0)) != 0)
						return(retval);
					goto done1; /* Ack!!! */
				}
				if (((struct ip *)p)->ip_p == IPPROTO_TCP)
				{	struct tcphdr *tp;

					tp = (struct tcphdr *)(p+hlen);


#ifdef DONT_DO_LOG
log(LOG_WARNING, "TCP IN: Looking up %x\n", tp->th_dport);
#endif
					owner = in_pcb_get_owner (&tcbinfo,
								  ip->ip_dst,
								  tp->th_dport,
								  ip->ip_src,
								  tp->th_sport,
								  &lcookie);
					/*
					 * TCP ports are not shared across
					 *  worlds.  If it's owned by our
					 *  user, let him have it.  Otherwise
					 *  hand it on for further processing.
					 */
					if (owner == ifb->tcp_blue_owned)
					{	m0 = m;
						m = NULL;
					} else if (owner == INPCB_NO_OWNER)
					{
#if SIP_DEBUG_INFO
						log(LOG_WARNING,
						    "TCP IN: No owner for %x:%x <- %x:%x\n",
						    ip->ip_dst,
						    tp->th_dport,
						    ip->ip_src,
						    tp->th_sport);
#endif
					}
#ifdef DONT_DO_LOG
log(LOG_WARNING, "TCP IN: Owner sez %x/%x (%x)\n",
    owner, lcookie, ifb->tcp_blue_owned);
#endif
				} else if (((struct ip *)p)->ip_p == IPPROTO_UDP)
				{	struct udphdr *up;

					up = (struct udphdr *)(p+hlen);

#ifdef DONT_DO_LOG
log(LOG_WARNING, "UDP IN: Looking up %x\n", up->uh_dport);
#endif
					owner = in_pcb_get_owner (&udbinfo,
								  ip->ip_dst,
								  up->uh_dport,
								  ip->ip_src,
								  up->uh_sport,
								  &lcookie);
					/*
					 * UDP ports can be shared (multicast)
					 * If only our user then give it to
					 *  him; otherwise, if we're one
					 *  of the owners, dup; else
					 *  let the X-side deal with it.
					 */
					if (owner == ifb->udp_blue_owned)
					{	m0 = m;
						m = NULL;
					} else if (owner & ifb->udp_blue_owned)
						m0 = m_dup(m, M_NOWAIT);
					else if (owner == INPCB_NO_OWNER)
					{
#if SIP_DEBUG_INFO
						log(LOG_WARNING,
						    "UDP IN: No owner for %x:%x <- %x:%x\n",
						    ip->ip_dst,
						    up->uh_dport,
						    ip->ip_src,
						    up->uh_sport);
#endif
					}
#ifdef DONT_DO_LOG
log(LOG_WARNING, "UDP IN: Owner sez %x, %x, %x\n",
    owner, who, ifb->udp_blue_owned);
#endif
				} else if (((struct ip *)p)->ip_p == IPPROTO_ICMP)
				{	struct icmp *ih;

					ih = (struct icmp *)(p + hlen);

					if (ih->icmp_type == ICMP_ECHO ||
					    ih->icmp_type == ICMP_IREQ ||
					    ih->icmp_type == ICMP_MASKREQ ||
					    ih->icmp_type == ICMP_TSTAMP)
						; /* For X */
					else		/* Both get it! */
						m0 = m_dup(m, M_NOWAIT);
				} else		/* Both get it! */
					m0 = m_dup(m, M_NOWAIT);
			}
		} /* Else god knows, and we'll let the system sort it out */
		if (m0)
			blue_inject(ifb, m0);
	}

	if (m == NULL)
		return(EJUSTRETURN);
 done1:
	MDATA_ETHER_END(m);
	*m_orig = m;
	return (0);
}

/*
 * Outbound IPv4 ethernet filter: Packets from X's IP stack.  This
 *  filter gets called for output via the chosen ethernet interface.
 * We don't yet have a fully-formed packet, so we have to save
 *  the frame header if we siphon off the packet for our client.
 */
int
ipv4_eth_outfltr(caddr_t cookie,
	     struct mbuf   **m_orig,
	     struct ifnet  **ifnet_ptr,
	     struct sockaddr **dest,
	     char *dest_linkaddr,
	     char *frame_type)
{	register struct blueCtlBlock *ifb = (struct blueCtlBlock *)cookie;
	register struct BlueFilter *bf;
	register unsigned char *p;
	struct mbuf * m0 = NULL, *m = *m_orig;
	int total, retval;
	unsigned int lcookie, hlen;
	unsigned char owner;
	struct ip *ip;

	if (FILTER_LEN > m->m_pkthdr.len)
		return(0);

	while ((FILTER_LEN > m->m_len) && m->m_next) {
		total = m->m_len + (m->m_next)->m_len;
		if ((m = m_pullup(m, min(FILTER_LEN, total))) == 0)
			return(EJUSTRETURN);
	}

	bf = &ifb->filter[BFS_IP];  /* find IPv4 filter */

	if (bf->BF_flags)	/* Filtering IPv4 */
	{	p = (unsigned char *)m->m_data;
		/* Check to avoid ARP pkts - they only go out. */
		if ((*dest)->sa_family == AF_INET)
		{	ip = (struct ip *)p;
			hlen = ip->ip_hl << 2;
			/*
			 * NB: Bailing out here!
			 */
			if (not4us(ip, *ifnet_ptr))
			{	*m_orig = m;
				return(0);
			}
			if (IN_CLASSD(ip->ip_dst.s_addr))
				m0 = m_dup(m, M_NOWAIT);
			else if (ip->ip_off & (IP_MF | IP_OFFMASK | IP_RF))
			{	if ((retval =
				    handle_frags(m, dest_linkaddr, sizeof (struct ether_header), ifb, 1)) != 0)
					return(retval);
				goto done2;
			} else if (ip->ip_p == IPPROTO_TCP)
			{	struct tcphdr *tp;

				tp = (struct tcphdr *)((char *)ip+hlen);

#ifdef DONT_DO_LOG
log(LOG_WARNING, "TCP OUT: Looking up %x\n", tp->th_dport);
#endif
				/*
				 * TCP ports are not shared across
				 *  worlds.  If it's owned by our
				 *  user, let him have it.  Otherwise
				 *  hand it on for further processing.
				 */
				owner = in_pcb_get_owner (&tcbinfo,
							  ip->ip_dst,
							  tp->th_dport,
							  ip->ip_src,
							  tp->th_sport,
							  &lcookie);
				if (owner == ifb->tcp_blue_owned)
				{	m0 = m;
					m = NULL;
				} else if (owner == INPCB_NO_OWNER)
				{
#if SIP_DEBUG_INFO
					log(LOG_WARNING,
					    "TCP OUT: No owner for %x:%x <- %x:%x\n",
					    ip->ip_dst,
					    tp->th_dport,
					    ip->ip_src,
					    tp->th_sport);
#endif
				}
#ifdef DONT_DO_LOG
log(LOG_WARNING, "TCP OUT: Owner sez %x/%x (%x)\n",
    owner, lcookie, ifb->tcp_blue_owned);
#endif
			} else if (ip->ip_p == IPPROTO_UDP)
			{	struct udphdr *up;

				up = (struct udphdr *)((char *)ip+hlen);

#ifdef DONT_DO_LOG
log(LOG_WARNING, "UDP OUT: Looking up %x\n", up->uh_dport);
#endif
				owner = in_pcb_get_owner (&udbinfo,
							  ip->ip_dst,
							  up->uh_dport,
							  ip->ip_src,
							  up->uh_sport,
							  &lcookie);

				/*
				 * UDP ports can be shared (multicast)
				 * If only our user then give it to
				 *  him; otherwise, if we're one
				 *  of the owners, dup; else
				 *  let the X-side deal with it.
				 * In the other cases, give it to both
				 */
				if (owner == ifb->udp_blue_owned)
				{	m0 = m;
					m = NULL;
				} else {
					m0 = m_dup(m, M_NOWAIT);
#if SIP_DEBUG_INFO
					log(LOG_WARNING,
					    "UDP OUT: Sending to both %x:%x <- %x:%x\n",
					    ip->ip_dst,
					    up->uh_dport,
					    ip->ip_src,
					    up->uh_sport);
#endif
				}
#ifdef DONT_DO_LOG
log(LOG_WARNING, "UDP OUT: Owner sez %x, %x\n", owner, ifb->udp_blue_owned);
#endif
			} else if (ip->ip_p == IPPROTO_ICMP)
			{	struct icmp *ih;

				ih = (struct icmp *)(p + hlen);

				if (ih->icmp_type == ICMP_ECHO ||
				    ih->icmp_type == ICMP_IREQ ||
				    ih->icmp_type == ICMP_MASKREQ ||
				    ih->icmp_type == ICMP_TSTAMP ||
				    ((ih->icmp_type == ICMP_ECHOREPLY) &&
					 (ip->ip_dst.s_addr != ip->ip_src.s_addr)))
					; /* For Wire */
				else		/* Both get it! */
					m0 = m_dup(m, M_NOWAIT);
			} else          /* Both get it! */
				m0 = m_dup(m, M_NOWAIT);
		}

		if (m0)
		{	struct ether_header *eh;

			M_PREPEND(m0, sizeof (struct ether_header), M_NOWAIT);
			eh = mtod(m0, struct ether_header *);
			eh->ether_type = *(unsigned short *)frame_type;
			bcopy(ifb->dev_media_addr, m0->m_data,
			      ifb->media_addr_size);
			blue_inject(ifb, m0);
		}
		if (m == NULL)
			return(EJUSTRETURN);
	}
	/* this is for MacOS X, DLIL will hand the mbuf to IPv4 stack. */
	/* put the packet back together as expected */

 done2:
	*m_orig = m;
	return (0);
}

/*
 * Outbound IPv4 filter: Packets from X's IP stack.  This filter
 *  gets called for output via loopback.
 * We don't yet have a fully-formed packet, so we have to save
 *  the frame header if we siphon off the packet for our client.
 * Note that loopback uses only the output filter (what's input
 *  is output, and what's output is input).
 */
int
ipv4_loop_outfltr(caddr_t cookie,
	     struct mbuf   **m_orig,
	     struct ifnet  **ifnet_ptr,
	     struct sockaddr **dest,
	     char *dest_linkaddr,
	     char *frame_type)
{	register struct blueCtlBlock *ifb = (struct blueCtlBlock *)cookie;
	register struct BlueFilter *bf;
	register unsigned char *p;
	struct mbuf * m0 = NULL, *m = *m_orig;
	int total, retval;
	unsigned int lcookie, hlen;
	unsigned char owner;
	struct ip *ip;
#if SIP_DEBUG_FLOW
	unsigned short *s;
#endif

	if (FILTER_LEN > m->m_pkthdr.len)
		return(0);

	while ((FILTER_LEN > m->m_len) && m->m_next) {
		total = m->m_len + (m->m_next)->m_len;
		if ((m = m_pullup(m, min(FILTER_LEN, total))) == 0)
			return(EJUSTRETURN);
	}

	bf = &ifb->filter[BFS_IP];  /* find IPv4 filter */

#if SIP_DEBUG_FLOW
	if (!bf->BF_flags)
		log(LOG_WARNING,
		    "ipv4_loop_outfltr: p0=%x %x, %x, ... %x, net %x\n",
		    p[0],s[6], s[7], s[10], s[13], p[30]);
#endif

	if (bf->BF_flags)	/* Filtering IPv4 */
	{	p = (unsigned char *)m->m_data;
		/* Check to avoid ARP pkts - they only go out. */
		if ((*dest)->sa_family == AF_INET)
		{	ip = (struct ip *)p;
			hlen = ip->ip_hl << 2;
			/*
			 * NB: Bailing out here!
			 */
			if (not4us(ip, *ifnet_ptr))
			{	*m_orig = m;
				return(0);
			}
			if (IN_CLASSD(ip->ip_dst.s_addr)) /* Mcast */
				m0 = m_dup(m, M_NOWAIT);
			else if (ip->ip_off & (IP_MF | IP_OFFMASK | IP_RF))
			{	if ((retval =
				    handle_frags(m, dest_linkaddr, 4*sizeof (unsigned long), ifb, 1)) != 0)
					return(retval);
				goto done3;
			} else if (ip->ip_p == IPPROTO_TCP)
			{	struct tcphdr *tp;

				tp = (struct tcphdr *)((char *)ip+hlen);

#ifdef DONT_DO_LOG
log(LOG_WARNING, "TCP OUT: Looking up %x\n", tp->th_dport);
#endif
				/*
				 * TCP ports are not shared across
				 *  worlds.  If it's owned by our
				 *  user, let him have it.  Otherwise
				 *  hand it on for further processing.
				 */
				owner = in_pcb_get_owner (&tcbinfo,
							  ip->ip_dst,
							  tp->th_dport,
							  ip->ip_src,
							  tp->th_sport,
							  &lcookie);
				if (owner == ifb->tcp_blue_owned)
				{	m0 = m;
					m = NULL;
				} else if (owner == INPCB_NO_OWNER)
				{
#if SIP_DEBUG_INFO
					log(LOG_WARNING,
					    "TCP OUT: No owner for %x:%x <- %x:%x\n",
					    ip->ip_dst,
					    tp->th_dport,
					    ip->ip_src,
					    tp->th_sport);
#endif
				}
#ifdef DONT_DO_LOG
log(LOG_WARNING, "TCP OUT: Owner sez %x/%x (%x)\n",
    owner, lcookie, ifb->tcp_blue_owned);
#endif
			} else if (ip->ip_p == IPPROTO_UDP)
			{	struct udphdr *up;

				up = (struct udphdr *)((char *)ip+hlen);

#ifdef DONT_DO_LOG
log(LOG_WARNING, "UDP OUT: Looking up %x\n", up->uh_dport);
#endif
				owner = in_pcb_get_owner (&udbinfo,
							  ip->ip_dst,
							  up->uh_dport,
							  ip->ip_src,
							  up->uh_sport,
							  &lcookie);

				/*
				 * UDP ports can be shared (multicast)
				 * If only our user then give it to
				 *  him; otherwise, if we're one
				 *  of the owners, dup; else
				 *  let the X-side deal with it.
				 * In the other cases, give it to both
				 */
				if (owner == ifb->udp_blue_owned)
				{	m0 = m;
					m = NULL;
				} else {
					m0 = m_dup(m, M_NOWAIT);
#if SIP_DEBUG_INFO
					log(LOG_WARNING,
					    "UDP OUT: Sending to both %x:%x <- %x:%x\n",
					    ip->ip_dst,
					    up->uh_dport,
					    ip->ip_src,
					    up->uh_sport);
#endif
				}
#ifdef DONT_DO_LOG
log(LOG_WARNING, "UDP OUT: Owner sez %x, %x\n", owner, ifb->udp_blue_owned);
#endif
			} else if (ip->ip_p == IPPROTO_ICMP)
			{	struct icmp *ih;

				ih = (struct icmp *)(p + hlen);

				if (ih->icmp_type == ICMP_ECHO ||
				    ih->icmp_type == ICMP_IREQ ||
				    ih->icmp_type == ICMP_MASKREQ ||
				    ih->icmp_type == ICMP_TSTAMP ||
				    ih->icmp_type == ICMP_ECHOREPLY)
					; /* For Wire */
				else		/* Both get it! */
					m0 = m_dup(m, M_NOWAIT);
			} else          /* Both get it! */
				m0 = m_dup(m, M_NOWAIT);
		}

		if (m0)
		{	struct ether_header *eh;

			M_PREPEND(m0, sizeof (struct ether_header), M_NOWAIT);
			eh = mtod(m0, struct ether_header *);
			eh->ether_type = *(unsigned short *)frame_type;
			bcopy(ifb->dev_media_addr, m0->m_data,
			      ifb->media_addr_size);
			blue_inject(ifb, m0);
		}
		if (m == NULL)
			return(EJUSTRETURN);
	}
	/* this is for MacOS X, DLIL will hand the mbuf to IPv4 stack. */
	/* put the packet back together as expected */

 done3:
	*m_orig = m;
	return (0);
}

/*
 * We get a packet destined for the outside world, complete with ethernet
 *  header.  If it goes to X, we need to get rid of the header (but keep
 *  it around, if you know what I mean).
 */
int
si_send_eth_ipv4(register struct mbuf **m_orig, struct blueCtlBlock *ifb)
{	struct mbuf *m0 = NULL, *m = *m_orig;
	struct ifnet *ifp;
	unsigned char *p;
	unsigned char owner;
	unsigned int lcookie;
	register unsigned short *s;
	int total, hlen, retval;
	struct ip *ip;
#if SIP_DEBUG_FLOW
	register unsigned long *l;
#endif
	register struct BlueFilter *bf;

	if (FILTER_LEN > m->m_pkthdr.len)
		return(0);

	while ((FILTER_LEN > m->m_len) && m->m_next) {
		total = m->m_len + (m->m_next)->m_len;
		if ((m = m_pullup(m, min(FILTER_LEN, total))) == 0)
			return(EJUSTRETURN);
	}

	*m_orig = m;
	p = mtod(m, unsigned char *);   /* Point to destination media addr */
	s = (unsigned short *)p;
#if SIP_DEBUG_FLOW
l = (unsigned long *)&s[15];
log(LOG_WARNING, "si_send_ipv4 packet: p0=%x, len=%x, etype=%x snap? %x %x:%x, Add:%x.%x\n",
    p[0], m->m_len, s[6], s[7], *l, s[10], s[13], p[30]);
#endif
	if (s[6] == ETHERTYPE_ARP)
		return(0);	/* X doesn't need Blue's arp packets */

	bf = &ifb->filter[BFS_IP];  /* find user's IPv4 filter */
	ifp = ((struct ndrv_cb *)ifb->ifb_so->so_pcb)->nd_if;
	if  (bf->BF_flags&SIP_PROTO_RCV_SHARED)
	{	if (s[6] == ETHERTYPE_IP)
		{	ip = (struct ip *)(p + (sizeof (struct ether_header)));
			hlen = ip->ip_hl << 2;
			/*
			 * NB: Bailing out here!
			 */
			if (not4us(ip, ifp))
				return(0);
			if (IN_CLASSD(ip->ip_dst.s_addr)) /* Mcast */
				m0 = m_dup(m, M_NOWAIT);
			else if (ip->ip_off & (IP_MF | IP_OFFMASK | IP_RF))
			{	if ((retval =
				    handle_frags(m, NULL, sizeof (struct ether_header), ifb, -1)) != 0)
					return(retval);
				goto done4;
			} else if (ip->ip_p == IPPROTO_TCP)
			{	struct tcphdr *tp;

				tp = (struct tcphdr *)((char *)ip+hlen);

#ifdef DONT_DO_LOG
log(LOG_WARNING, "TCP OUT: Looking up %x\n", tp->th_dport);
#endif
				/*
				 * TCP ports are not shared across
				 *  worlds.  If it's owned by our
				 *  user, let him have it.  Otherwise
				 *  hand it on for further processing.
				 */
				owner = in_pcb_get_owner (&tcbinfo,
							   ip->ip_src,
							   tp->th_sport,
							   ip->ip_dst,
							   tp->th_dport,
							   &lcookie);
				if (owner == ifb->tcp_blue_owned)
				{	m0 = m;
					m = NULL;
				} else if (owner == INPCB_NO_OWNER)
				{
#if SIP_DEBUG_INFO
					log(LOG_WARNING,
					    "TCP OUT: No owner for %x:%x <- %x:%x\n",
					    ip->ip_dst,	tp->th_dport,
					    ip->ip_src, tp->th_sport);
#endif
				}
#ifdef DONT_DO_LOG
log(LOG_WARNING, "TCP OUT: Owner sez %x/%x (%x)\n",
    owner, lcookie, ifb->tcp_blue_owned);
#endif
			} else if (ip->ip_p == IPPROTO_UDP)
			{	struct udphdr *up;

				up = (struct udphdr *)((char *)ip+hlen);

#ifdef DONT_DO_LOG
log(LOG_WARNING, "UDP OUT: Looking up %x\n", up->uh_dport);
#endif
				owner = in_pcb_get_owner (&udbinfo,
							   ip->ip_src,
							   up->uh_sport,
							   ip->ip_dst,
							   up->uh_dport,
							   &lcookie);

				/*
				 * UDP ports can be shared (multicast)
				 * If only our user then give it to
				 *  him; otherwise, if we're one
				 *  of the owners, dup; else
				 *  let the X-side deal with it.
				 *  In the other cases, give it to both
				 */
				if (owner == ifb->udp_blue_owned)
				{	m0 = m;
					m = NULL;
				} else if (owner & ifb->udp_blue_owned)
					m0 = m_dup(m, M_NOWAIT);
				else if (owner == INPCB_NO_OWNER)
				{
#if SIP_DEBUG_INFO
					log(LOG_WARNING,
					    "UDP OUT: No owner for %x:%x <- %x:%x\n",
					    ip->ip_dst,
					    up->uh_dport,
					    ip->ip_src,
					    up->uh_sport);
#endif
				}
#ifdef DONT_DO_LOG
log(LOG_WARNING, "UDP OUT: Owner sez %x, %x\n", owner, ifb->udp_blue_owned);
#endif
			} else if (ip->ip_p == IPPROTO_ICMP)
			{	struct icmp *ih;

				ih = (struct icmp *)(p + hlen);

				if (ih->icmp_type == ICMP_ECHO ||
				    ih->icmp_type == ICMP_IREQ ||
				    ih->icmp_type == ICMP_MASKREQ ||
				    ih->icmp_type == ICMP_TSTAMP ||
				    ih->icmp_type == ICMP_ECHOREPLY)
					; /* For Wire */
				else		/* Both get it! */
					m0 = m_dup(m, M_NOWAIT);
			} else          /* Both get it! */
				m0 = m_dup(m, M_NOWAIT);
		}

		if (m0)
		{	/*
			 * Destination is X; pretend it came from
			 *  device...
			 */
			char *p;
			p = mtod(m0, char *);
			MDATA_ETHER_END(m0);
			m0->m_pkthdr.rcvif = ((struct ndrv_cb *)ifb->ifb_so->so_pcb)->nd_if;
			dlil_inject_pr_input(m0, p, ifb->ipv4_proto_filter_id);
		}
		if (m == NULL)
			return(EJUSTRETURN);
	}
 done4:
	*m_orig = m;
	return(0);
}

/*
 * We get a packet destined for the outside world, complete with PPP
 *  'header'.
 */
int
si_send_ppp_ipv4(register struct mbuf **m_orig, struct blueCtlBlock *ifb)
{	struct mbuf *m0 = NULL, *m = *m_orig;
	struct ifnet *ifp;
	unsigned char *p;
	unsigned char owner;
	unsigned int lcookie;
	register unsigned short *s;
	int total, hlen, retval;
	struct ip *ip;
#if SIP_DEBUG_FLOW
	register unsigned long *l;
#endif
	register struct BlueFilter *bf;

	if (FILTER_LEN > m->m_pkthdr.len)
		return(0);

	while ((FILTER_LEN > m->m_len) && m->m_next) {
		total = m->m_len + (m->m_next)->m_len;
		if ((m = m_pullup(m, min(FILTER_LEN, total))) == 0)
			return(EJUSTRETURN);
	}

	*m_orig = m;
	p = mtod(m, unsigned char *);   /* Point to destination media addr */
	s = (unsigned short *)p;

	bf = &ifb->filter[BFS_IP];  /* find user's IPv4 filter */
	ifp = ((struct ndrv_cb *)ifb->ifb_so->so_pcb)->nd_if;
	if  (bf->BF_flags&SIP_PROTO_RCV_SHARED)
	{	ip = (struct ip *)(p + ifb->media_addr_size);
		hlen = ip->ip_hl << 2;
		/*
		 * NB: Bailing out here!
		 */
		if (not4us(ip, ifp))
			return(0);
		/*
		 * If multicast, both sides get it, so we don't need
		 *  to check for frags or UDP (TCP it can't be).
		 * But wait!  IP Broadcast???  Should that work?
		 */
		if (IN_CLASSD(ip->ip_dst.s_addr)) /* Mcast */
			m0 = m_dup(m, M_NOWAIT);
		else if (ip->ip_off & (IP_MF | IP_OFFMASK | IP_RF))
		{	if ((retval =
			    handle_frags(m, NULL, sizeof (struct ether_header), ifb, -1)) != 0)
				return(retval);
			goto done4;
		} else if (ip->ip_p == IPPROTO_TCP)
		{	struct tcphdr *tp;
		
			tp = (struct tcphdr *)((char *)ip+hlen);
		
#ifdef DONT_DO_LOG
log(LOG_WARNING,"TCP OUT: Looking up %x\n", tp->th_dport);
#endif		
			/*
			 * TCP ports are not shared across
			 *  worlds.  If it's owned by our
			 *  user, let him have it.  Otherwise
			 *  hand it on for further processing.
			 */
			owner = in_pcb_get_owner (&tcbinfo,
						   ip->ip_src,
						   tp->th_sport,
						   ip->ip_dst,
						   tp->th_dport,
						   &lcookie);
			if (owner == ifb->tcp_blue_owned)
			{	m0 = m;
				m = NULL;
			} else if (owner == INPCB_NO_OWNER)
			{
#if SIP_DEBUG_IN
				log(LOG_WARNING,
				    "TCP OUT: No owner for %x:%x <- %x:%x\n",
				    ip->ip_dst,	tp->th_dport,
				    ip->ip_src, tp->th_sport);
#endif		
			}
#ifdef DONT_DO_LOG
log(LOG_WARNING,"TCP OUT: Owner sez %x/%x (%x)\n",
    owner, lcookie, ifb->tcp_blue_owned);
#endif		
		} else if (ip->ip_p == IPPROTO_UDP)
		{	struct udphdr *up;
		
			up = (struct udphdr *)((char *)ip+hlen);
		
#ifdef DONT_DO_L
log(LOG_WARNING, "UDP OUT: Looking up %x\n", up->uh_dport);
#endif		
			owner = in_pcb_get_owner (&udbinfo,
						   ip->ip_src,
						   up->uh_sport,
						   ip->ip_dst,
						   up->uh_dport,
						   &lcookie);
			
			/*
			 * UDP ports can be shared (multicast)
			 * If only our user then give it to
			 *  him; otherwise, if we're one
			 *  of the owners, dup; else
			 *  let the X-side deal with it.
			 */
			if (owner == ifb->udp_blue_owned)
			{	m0 = m;
				m = NULL;
			} else if (owner & ifb->udp_blue_owned)
				m0 = m_dup(m, M_NOWAIT);
			else if (owner == INPCB_NO_OWNER)
			{
#if SIP_DEBUG_IN
				log(LOG_WARNING,
				    "UDP OUT: No owner for %x:%x <- %x:%x\n",
				    ip->ip_dst,
				    up->uh_dport,
				    ip->ip_src,
				    up->uh_sport);
#endif		
			}
#ifdef DONT_DO_L
log(LOG_WARNING, "UDP OUTT: Owner sez %x, %x\n", owner, ifb->udp_blue_owned);
#endif		
		} else if (ip->ip_p == IPPROTO_ICMP)
		{	struct icmp *ih;
		
			ih = (struct icmp *)(p + hlen);
		
			if (ih->icmp_type == ICMP_ECHO ||
			    ih->icmp_type == ICMP_IREQ ||
			    ih->icmp_type == ICMP_MASKREQ ||
			    ih->icmp_type == ICMP_TSTAMP ||
			    ih->icmp_type == ICMP_ECHOREPLY)
				; /* For Wire */
			else		/* Both get it! */
				m0 = m_dup(m, M_NOWAIT);
		} else          /* Both get it! */
			m0 = m_dup(m, M_NOWAIT);
		
		if (m0)
		{	/*
			 * Destination is X; pretend it came from
			 *  device...
			 */
			char *p;
			p = mtod(m0, char *);
			//			MDATA_PPP_END(m0); /* ??? */
			m0->m_pkthdr.rcvif =
			  ((struct ndrv_cb *)ifb->ifb_so->so_pcb)->nd_if;
			dlil_inject_pr_input(m0, p, ifb->ipv4_proto_filter_id);
		}
		if (m == NULL)
			return(EJUSTRETURN);
	}
 done4:
	*m_orig = m;
	return(0);
}

/*
 * There are three proto filters inserted here:
 *  - one on the input side, handling incoming from the chosen I/F
 *  - two on the output side, handling outbound from the loopback
 *    and chosen I/F's.
 * The filters for the interface depends on the interface type.
 */
int
ipv4_attach_protofltr(struct ifnet *ifp, struct blueCtlBlock *ifb)
{
	u_long ip4_dltag, lo_dltag;
	int retval=0;
	struct dlil_pr_flt_str filter1 =
	{	(caddr_t)ifb,
		0, 0,
		0,0,0
	};
	struct dlil_pr_flt_str lo_filter =
	{	(caddr_t)ifb,
		0,
		ipv4_loop_outfltr,
		0, 0, 0
	};

	/* Make sure we don't leave a dangling DLIL filter around */

	if (ifb->ipv4_proto_filter_id) 
		retval = ipv4_stop(ifb);
	if (retval)
		return (retval);

	if ((retval = dlil_find_dltag(ifp->if_family, ifp->if_unit,
				      PF_INET, &ip4_dltag)) == 0)
	{	if (ifp->if_type == IFT_ETHER)
		{	filter1.filter_dl_input = ipv4_eth_infltr;
			filter1.filter_dl_output = ipv4_eth_outfltr;
		} else if (ifp->if_type == IFT_PPP)
		{	filter1.filter_dl_input = ipv4_ppp_infltr;
			filter1.filter_dl_output = ipv4_ppp_outfltr;
		} /* else??? */

		retval= dlil_attach_protocol_filter(ip4_dltag, &filter1,
						    &ifb->ipv4_proto_filter_id,
						    DLIL_LAST_FILTER);
		if (retval == 0)
		{	retval = dlil_find_dltag(loif[0].if_family,
						 loif[0].if_unit,
						 PF_INET, &lo_dltag);
			if (retval == 0)
			{	retval =
				  dlil_attach_protocol_filter(lo_dltag,
							      &lo_filter,
							      &ifb->lo_proto_filter_id,
							      DLIL_LAST_FILTER);
			}
		}
	}
#if SIP_DEBUG_INFO
        log(LOG_WARNING, "Attach IP filter: %d\n", retval);
#endif
#if SIP_DEBUG
       log(LOG_WARNING, "ipv4_attach(%s%d): dltag=%d filter_id=%d\n",
	   ifp->if_name, ifp->if_unit,
	   ip4_dltag, ifb->ipv4_proto_filter_id);
       log(LOG_WARNING, "ipv4_attach(lo0): dltag=%d filter_id=%d retval=%d\n",
	   lo_dltag, ifb->lo_proto_filter_id, retval);
#endif
       return (retval);
}

int
ipv4_stop(struct blueCtlBlock *ifb)
{	struct fraghead *frag;
	struct mbuf *m, *m1;
        int retval = 0;

	if (ifb == NULL)
		return(0);

        /* deregister TCP & UDP port sharing if needed */

        if (ifb->tcp_blue_owned)
	{	if ((retval = in_pcb_rem_share_client(&tcbinfo, ifb->tcp_blue_owned)) != 0)
		{
#if SIP_DEBUG_ERR
			log(LOG_WARNING, "STOP/TCP: %d\n", retval);
#endif
			return (retval);
		}
	}
        ifb->tcp_blue_owned = 0;

        if (ifb->udp_blue_owned)
	{	if ((retval = in_pcb_rem_share_client(&udbinfo, ifb->udp_blue_owned)) != 0)
		{
#if SIP_DEBUG_ERR
			log(LOG_WARNING, "STOP/UDP: %d\n", retval);
#endif
			return (retval);
		}
	}
        ifb->udp_blue_owned = 0;

	if (ifb->ipv4_proto_filter_id)
	{
#if SIP_DEBUG
		log(LOG_WARNING,
		    "ipv4_stop: deregister IPv4 proto filter tag=%d\n",
		    ifb->ipv4_proto_filter_id);
#endif
		retval = dlil_detach_filter(ifb->ipv4_proto_filter_id);
		if (retval)
#if SIP_DEBUG_ERR
			log(LOG_WARNING, "STOP/FILTER1: %d\n", retval);
#endif
		else
		{	ifb->ipv4_proto_filter_id = 0;
			retval = dlil_detach_filter(ifb->lo_proto_filter_id);
			if (retval)
#if SIP_DEBUG_ERR
				log(LOG_WARNING, "STOP/FILTER2: %d\n", retval);
#endif
			else
				ifb->lo_proto_filter_id = 0;
		}
	}

	if (retval == 0 && ifb->fraglist_timer_on)
	{	ifb->ClosePending = 1;
		retval = EBUSY;	/* Assume retval isn't changed from here down */
                untimeout(sip_fragtimer, ifb);
	} else
		ifb->ClosePending = 0; /* In case... */

	/* Clean out frag list */
	while ((frag = TAILQ_FIRST(&ifb->fraglist)) != 0)
	{	TAILQ_REMOVE(&ifb->fraglist, frag, fh_link);
		m = TAILQ_FIRST(&(frag->fh_frags));
		while (m)
		{       m1 = m->m_nextpkt;
			m_freem(m); /* Free a chain */
			m = m1;
		}
		FREE(frag, M_TEMP);
	}
	return(retval);
}

int
ipv4_control(struct socket *so, struct sopt_shared_port_param *psp,
	     struct kextcb *kp, int cmd)
{	int retval;
	struct inpcbinfo *pcbinfo;
	struct blueCtlBlock *ifb = (struct blueCtlBlock *)kp->e_fcb;
        u_char owner = 0;
        u_short lport;
	switch(cmd)
	{   case SO_PORT_RESERVE:
            {
#if SIP_DEBUG
                log(LOG_WARNING,
		    "ipv4_control: so=%x SO_PORT_RESERVE laddr=%x lport=%x proto=%x fport=%x faddr=%x\n",
		    so, psp->laddr.s_addr, psp->lport, psp->proto,
		    psp->fport, psp->faddr.s_addr);
#endif
#if SIP_DEBUG_INFO
log(LOG_WARNING,
    "SIP-NKE: so=%x SO_PORT_RESERVE laddr=%x lport=%x proto=%x\n",
    so, psp->laddr.s_addr, psp->lport, psp->proto);
#endif
                switch (psp->proto)
		{   case IPPROTO_UDP:
                        pcbinfo = &udbinfo;
                        owner = ifb->udp_blue_owned;
                        break;
                    case IPPROTO_TCP:
                        pcbinfo = &tcbinfo;
                        owner = ifb->tcp_blue_owned;
                        break;

                    default:
#if SIP_DEBUG_ERR
                        log(LOG_WARNING,
			    "ipv4_control: unsuported (%x) proto port request so=%x\n",
			    psp->proto, so);
#endif
                        return (EPROTONOSUPPORT);
                }

		/*
		 * Call the X TCP/IP stack with the releavant
		 * port information
		 */

                psp->faddr.s_addr	= 0; //### because value is not set in BBox
                psp->fport 		= 0; //### value is not set in bbox, bogus
                lport = psp->lport;

                retval = in_pcb_grab_port(pcbinfo, (u_short)psp->flags,
					  psp->laddr,  &lport,
					  psp->faddr, (u_short)psp->fport,
					  psp->cookie,  (u_char)owner);
#if SIP_DEBUG_INFO
log(LOG_WARNING,
    "SIP-NKE: so=%x SO_PORT_RESERVE laddr=%x lport=%x, retval=%d\n",
    so, psp->laddr.s_addr, lport, retval);
#endif
                if (retval) {
#if SIP_DEBUG_ERR
                    log(LOG_WARNING,
			"ipv4_control: in_pcb_grab_port retval=%d so=%x\n",
			retval, so);
#endif
                    return (retval);
                }
#if SIP_DEBUG
                log(LOG_WARNING,
		    "ipv4_control: in_pcb_grab_port: returned port %x for so=%x\n",
		    lport, so);
#endif
                psp->lport = lport;
                return(0);
                break;
            }

            case SO_PORT_RELEASE:
            {
#if SIP_DEBUG
                log(LOG_WARNING,
		    "ipv4_control: so=%x SO_PORT_RELEASE laddr=%x lport=%x proto=%x\n",
		    so, psp->laddr.s_addr, psp->lport, psp->proto);

#endif
#if SIP_DEBUG_INFO
                log(LOG_WARNING,
		    "SIP-NKE: so=%x SO_PORT_RELEASE laddr=%x lport=%x proto=%x\n",
                    so, psp->laddr.s_addr, psp->lport, psp->proto);
#endif
                switch (psp->proto) {
                    case IPPROTO_UDP:
                        pcbinfo = &udbinfo;
                        owner = ifb->udp_blue_owned;
                        break;
                    case IPPROTO_TCP:
                        pcbinfo = &tcbinfo;
                        owner = ifb->tcp_blue_owned;
                        break;
                    default:
#if SIP_DEBUG_ERR
                        log(LOG_WARNING,
			    "ipv4_control: unsuported (%x) proto port release so=%x\n",
			    psp->proto, so);
#endif
                        return (EPROTONOSUPPORT);
                }

                psp->faddr.s_addr = 0; //### because value is not set in BBox
                psp->fport 	  = 0; //### value is not set in bbox, bogus

                retval = in_pcb_letgo_port (pcbinfo, psp->laddr,
                                   (u_short)psp->lport, psp->faddr,
					    psp->fport, owner);
#if SIP_DEBUG_INFO
                log(LOG_WARNING,
		    "SIP-NKE: so=%x SO_PORT_RELEASE laddr=%x lport=%x retval=%d\n",
                    so, psp->laddr.s_addr, psp->lport, retval);
#endif
                if (retval) {
#if SIP_DEBUG_ERR
                    log(LOG_WARNING,
			"ipv4_control: in_pcb_letgo_port retval=%d so=%x\n",
			retval, so);
#endif
                    return (retval);
                }
                return(0);
                break;
            }

            case SO_PORT_LOOKUP:
            {
#if SIP_DEBUG
                log(LOG_WARNING,
		    "ipv4_control: so=%x SO_PORT_LOOKUP laddr=%x lport=%x proto=%x\n",
		    so, psp->laddr.s_addr, psp->lport, psp->proto);
#endif
#if SIP_DEBUG_INFO
                log(LOG_WARNING,
		    "SIP-NKE: so=%x SO_PORT_LOOKUP laddr=%x lport=%x proto=%x\n",
                    so, psp->laddr.s_addr, psp->lport, psp->proto);
#endif
                return(0);
                break;
            }

	    default:
	      break;
	}
	return(0);
}

/*
 * Verify that the address the client is using is one of the ones
 *  attached to its chosen interface.
 */
int
validate_addrs(struct BlueFilter *bf, struct blueCtlBlock *ifb)
{	struct ifnet *ifp;
	struct ifaddr *ifa;
	struct sockaddr_in *sin;

	ifp = ((struct ndrv_cb *)ifb->ifb_so->so_pcb)->nd_if;
	ifa = ifp->if_addrhead.tqh_first;
	while (ifa)
	{	if ((sin = (struct sockaddr_in *)ifa->ifa_addr)
		     && sin->sin_family == AF_INET)
			if (sin->sin_addr.s_addr == bf->BF_address)
				break;

		ifa = ifa->ifa_link.tqe_next;
	}
	if (ifa == 0)
	{
#ifdef SIP_DEBUG_ERR
		log(LOG_WARNING, "SO_PROTO_REGISTER/IP: %x not valid!",
		    bf->BF_address);
#endif
		return(1);
	}
	return(0);
}

/*
 * Verify that the packet is ours
 */
int
not4us(struct ip *ip, struct ifnet *ifp)
{	struct ifaddr *ifa;
	struct sockaddr_in *sin;

	ifa = ifp->if_addrhead.tqh_first;
	while (ifa)
	{	if ((sin = (struct sockaddr_in *)ifa->ifa_addr)
		     && sin->sin_family == AF_INET)
			if (sin->sin_addr.s_addr == ip->ip_dst.s_addr)
				break;

		ifa = ifa->ifa_link.tqe_next;
	}
	if (ifa)
		return(0);
	return(1);
}

/*
 * Init anything that needs to be initted
 */
void
init_ipv4(struct socket *so, struct kextcb *kp)
{
}

/*
 * This is a fragmented packet.  Deal with it.
 * 'source' tells us where the packet is coming from:
 *	0 -> from network device
 *	+1 -> from X stack
 *	-1 -> from client
 * Disposition, based on ownership:
 * 1st frag not seen: if TCP/UDP -> enqueue
 *		      else mark frag, send to all
 * 1st frag seen: if TCP/UDP
 *		    no owner -> send to X
 *		    else send to all owners
 * Cleanup issues:
 *  Since we don't keep frags around after we know who gets it, we
 *  use the fragment timer to do this.  Since we can dump a fragment
 *  list after the timeout, we can, by extension, dump the fragment header
 *  of a successfully matched fragement sequence at that time.  The
 *  real reassembly queues, maintained by the stacks, will deal with the
 *  problem of an incomplete packet for which we know the owner.
 * If 'fh' is null, the frame header is included in the mbuf chain.
 *  If not, the frame header is pointed to by 'fh'.  In this case, we
 *  won't have queued frags (comes from X), so we just pass this through.
 * Return values:
 *  (rv > 0) => error code;
 *  (rv == 0) => normal handling
 */
int
handle_frags(struct mbuf *m0,
	     char *fh,
	     int fsize,
	     struct blueCtlBlock *ifb,
	     int source)
{	struct fraghead *frag;
	struct mbuf *m1 = NULL;
	struct ip *ip;
	int found = 0;
	struct in_addr laddr, faddr;

	if (fh)
		ip = mtod(m0, struct ip *);
	else
		ip = (struct ip *)(mtod(m0, char *)+fsize);
	/*
	 * First, see if it's in our stash.
	 * Always based on the destination as the "local addr"
	 *  for port lookup.
	 */
	laddr = ip->ip_dst;
	faddr = ip->ip_src;

#if DEBUG_FRAGS
log(LOG_WARNING, "Frag: %x/%x\n", ip->ip_id, ip->ip_p);
#endif
	TAILQ_FOREACH(frag, &ifb->fraglist, fh_link)
	{	if (frag->fh_id == ip->ip_id &&
		    frag->fh_proto == ip->ip_p &&
		    frag->fh_laddr.s_addr == laddr.s_addr &&
		    frag->fh_faddr.s_addr == faddr.s_addr)
		{	found++;
#if DEBUG_FRAGS
log(LOG_WARNING, "Frag: found\n");
#endif 
			break;
		}
	}
	if (!found)		/* Create new head */
	{	MALLOC(frag, struct fraghead *, sizeof (struct fraghead),
		       M_TEMP, M_NOWAIT);
		if (frag == NULL)
			return(0); /* Let the caller deal with this */

		bzero((char *)frag, sizeof (*frag));
		frag->fh_ttl = 120; /* 60 seconds, just like the big boys */
		frag->fh_proto = ip->ip_p;
		frag->fh_id = ip->ip_id;
#if DEBUG_FRAGS
log(LOG_WARNING, "Frag: setting ID: %x\n", frag->fh_id);
#endif
		frag->fh_owner = INPCB_NO_OWNER;
		TAILQ_INIT(&frag->fh_frags);
		frag->fh_laddr = laddr;
		frag->fh_faddr = faddr;
		frag->fh_frame = fh;
		frag->fh_fsize = fsize;
		if (frag->fh_proto == IPPROTO_UDP)
			frag->fh_pcbinfo = &udbinfo;
		else if (frag->fh_proto == IPPROTO_TCP)
			frag->fh_pcbinfo = &tcbinfo;
		else
			frag->fh_pcbinfo = NULL;
		if (ifb->fraglist_timer_on == 0)
		{	ifb->fraglist_timer_on = 1;
			timeout(sip_fragtimer, ifb, hz/2);
		}
		TAILQ_INSERT_TAIL(&ifb->fraglist, frag, fh_link);
	}

	/* If initial frag (offset == 0), determine owner */
	if ((ip->ip_off & IP_OFFMASK) == 0)
	{	unsigned char owner;
		struct udphdr *up;	/* Ports are ports; we'll fake this */
		unsigned int lcookie;
		int hlen, lport, fport;

		if (frag->fh_proto == IPPROTO_UDP ||
		    frag->fh_proto == IPPROTO_TCP)
		{	hlen = ip->ip_hl << 2;
			up = (struct udphdr *)((char *)ip+hlen);

			lport = up->uh_dport;
			fport = up->uh_sport;
			owner = in_pcb_get_owner (frag->fh_pcbinfo,
						  frag->fh_laddr, lport,
						  frag->fh_faddr, fport,
						  &lcookie);
#if SIP_DEBUG_INFO
log(LOG_WARNING, "Lookup of %x, %x, %x, %x: %x\n", frag->fh_laddr, lport,
    frag->fh_faddr, fport, owner);
#endif 
			frag->fh_owner = owner;
		} else
			frag->fh_owner = INPCB_ALL_OWNERS;
		/* Now, hand up the packets */
	}

	/* Check for known owner */
	if (frag->fh_owner != INPCB_NO_OWNER)
	{	unsigned char blue_owner = 0;

#if DEBUG_FRAGS
log(LOG_WARNING, "Frag: owner %x\n", frag->fh_owner);
#endif
		/* Note: only have queue for frags from "the device" */
		if (frag->fh_proto == IPPROTO_UDP)
			blue_owner = ifb->udp_blue_owned;
		else if (frag->fh_proto == IPPROTO_TCP)
			blue_owner = ifb->tcp_blue_owned;
		if (!TAILQ_EMPTY(&frag->fh_frags))
		{	register struct mbuf *m, *mn;
			register unsigned char* p;

#ifdef DEBUG_FRAGS
			log(LOG_WARNING, "Unloading out-of-order frags\n");
#endif
			if (blue_owner == 0)	/* Can't distinguish proto */
			{	for (m = frag->fh_frags.tqh_first; m;)
				{	mn = m->m_nextpkt;
					m->m_nextpkt = NULL;
					m1 = m_dup(m, M_NOWAIT);
					if (m1)
						blue_inject(ifb, m1);
					p = mtod(m, unsigned char *);
					MDATA_ETHER_END(m);
					dlil_inject_pr_input(m, p, ifb->ipv4_proto_filter_id);
					m = mn;
				}
			} else
			{	for (m = frag->fh_frags.tqh_first; m;)
				{	mn = m->m_nextpkt;
					m->m_nextpkt = NULL;
					if (frag->fh_owner & blue_owner)
					{	if (frag->fh_owner !=
						    blue_owner)
							m1 = m_dup(m, M_NOWAIT);
						else
						{	m1 = m;
							m = NULL;
						}
						if (m1)
							blue_inject(ifb, m1);
					}
					if (m &&
					    (frag->fh_owner & INPCB_OWNED_BY_X))
					{	p = mtod(m, unsigned char *);
						MDATA_ETHER_END(m);
						dlil_inject_pr_input(m, p, ifb->ipv4_proto_filter_id);
					}
					m = mn;
				}
			}
			TAILQ_INIT(&frag->fh_frags); /* We're empty now */
		}
		/*
		 * Now, handle new frag.
		 * For source == 0, we have the media header in
		 *  the mbuf chain.
		 * For source == -1, we have the media header in
		 *  the mbuf chain
		 * For source == 1, we have the media header in 'fh'
		 */
		if (source == 0)
		{	if (frag->fh_owner & blue_owner)
			{	if (frag->fh_owner != blue_owner)
					m1 = m_dup(m0, M_NOWAIT);
				else
				{	m1 = m0;
					m0 = NULL;
				}
				if (m1)
					blue_inject(ifb, m1);
			}
			if (m0 == NULL)
				return(EJUSTRETURN);
			else
				return(0);
		} else if (source == -1)	/* From Client */
		{	unsigned char *p;

			p = mtod(m0, unsigned char *);
			if (IN_CLASSD(ip->ip_dst.s_addr)) /* For both */
			{	m1 = m_dup(m0, M_NOWAIT);
				if (m1)
				{	MDATA_ETHER_END(m1); /* Dump eh */
					m0->m_pkthdr.rcvif = ((struct ndrv_cb *)ifb->ifb_so->so_pcb)->nd_if;
					dlil_inject_pr_input(m1, p, ifb->ipv4_proto_filter_id);
				}
				return(0);
			}
			if (frag->fh_owner == INPCB_OWNED_BY_X)
			{	MDATA_ETHER_END(m0); /* Dump eh */
				m0->m_pkthdr.rcvif = ((struct ndrv_cb *)ifb->ifb_so->so_pcb)->nd_if;
				dlil_inject_pr_input(m0, p, ifb->ipv4_proto_filter_id);
				return(EJUSTRETURN);
			}
		} else if (source == 1)	/* From X */
		{	struct ether_header *eh;
			if (IN_CLASSD(ip->ip_dst.s_addr)) /* For both */
			{	m1 = m_dup(m0, M_NOWAIT);
				if (m1)
				{	M_PREPEND(m1, sizeof (*eh), M_NOWAIT);
					if (m1)
					{	eh = mtod(m1, struct ether_header *);
						eh->ether_type = ETHERTYPE_IP;
						bcopy(ifb->dev_media_addr,
						      m1->m_data,
						      ifb->media_addr_size);
						blue_inject(ifb, m1);
					}
				}
			} else if (frag->fh_owner == blue_owner)
			{	M_PREPEND(m0, sizeof (*eh), M_NOWAIT);
				if (m0)
				{	eh = mtod(m0, struct ether_header *);
					eh->ether_type = ETHERTYPE_IP;
					bcopy(ifb->dev_media_addr,
					      m0->m_data,
					      ifb->media_addr_size);
					blue_inject(ifb, m0);
				}
				return(EJUSTRETURN);
			}
		}
		return(0);
	} else			/* Have to enqueue it */
	{	m0->m_nextpkt = NULL;
		*(frag->fh_frags.tqh_last) = m0;
		frag->fh_frags.tqh_last = &m0->m_nextpkt;
	}
	return(EJUSTRETURN);
}

/*
 * IP Frag queue processing:
 *  if a timer expires on a frag list, discard it.
 * Start this when we enter the first frag header into the list
 * Restart after every sweep.
 * Don't restart if list is empty (will not cancel, since races make
 *  it problematic).
 */
void
sip_fragtimer(void *arg)
{	int s;
	/* Will use this once the frag list is made non-global */
	struct blueCtlBlock *ifb = (struct blueCtlBlock *)arg;
	struct fraghead *frag, *frag1;
	int i = 0;
	boolean_t funnel_state;

	funnel_state = thread_funnel_set(network_flock, TRUE);

	s = splnet();
        if (ifb->ClosePending)
        {	release_ifb(ifb);
		splx(s);
		(void) thread_funnel_set(network_flock, funnel_state);
                return;
        }
	frag1 = TAILQ_FIRST(&ifb->fraglist);
	while ((frag = frag1) != 0)
	{	frag1 = TAILQ_NEXT(frag, fh_link);
		if (--frag->fh_ttl == 0)
		{	struct mbuf *m, *m1;
#if DEBUG_FRAGS
			log(LOG_WARNING, "frag: %x, frag1: %x\n", frag, frag1);
			if (frag->fh_frags.tqh_first == 0)
				log(LOG_WARNING, "Killing completed frag list (%x)\n", frag->fh_id);
			else
				log(LOG_WARNING, "Killing incomplete frag list (%x)\n", frag->fh_id);
#endif

			TAILQ_REMOVE(&ifb->fraglist, frag, fh_link);
			for (m1 = frag->fh_frags.tqh_first; (m = m1) != 0;
			     m1 = m->m_nextpkt)
				m_freem(m); /* Free a chain */
			FREE(frag, M_TEMP);
		} else
			i++;	/* Count remaining entries */
	}
	if (i)
		timeout(sip_fragtimer, arg, hz/2);
	else
		ifb->fraglist_timer_on = 0;
	splx(s);

	(void) thread_funnel_set(network_flock, funnel_state);
}
