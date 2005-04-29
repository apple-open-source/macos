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
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_pcb.h>
#include <netinet/ip.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <netinet/kpi_ipfilter.h>
#include <netinet/if_ether.h>
#include <net/if_arp.h>
#include <machine/spl.h>
#include <kern/thread.h>
#include <net/kpi_interfacefilter.h>

#include "SharedIP.h"
#include "sip.h"

#include <sys/syslog.h>

#define IPDEST_INTERFACE (1)
#define IPDEST_X (2)
#define IPDEST_BLUE (4)

/* In support of port sharing */
extern struct inpcbinfo udbinfo;
extern struct inpcbinfo tcbinfo;

static void ipv4_detach(void *cookie);

static u_char	bogus_ether_addr[ETHER_ADDR_LEN] = {0x4, 0x1, 0x1, 0x1, 0x1, 0x1};

/*
 * Get user's filtering info; prep for state check in caller.  We
 *  can modify state by registering for new ownership id's.
 * Requested addresses are validated against those registered for
 *  the requested interface.
 */
__private_extern__ int
enable_ipv4(
	struct BlueFilter	*bf,
	void				*data,
	struct blueCtlBlock	*ifb)
{
    int retval;
    struct sockaddr_in inet_sockaddr;
    ifnet_t ifp;

    retval = copyin(CAST_USER_ADDR_T(data), &inet_sockaddr,
                    sizeof(struct sockaddr_in));
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
        if ((retval = in_pcb_new_share_client(&udbinfo,
            &ifb->udp_blue_owned)) != 0)
            return (-retval);
        if ((retval = in_pcb_new_share_client(&tcbinfo,
            &ifb->tcp_blue_owned)) != 0) {
            if (in_pcb_rem_share_client(&udbinfo,
                ifb->udp_blue_owned) == 0)
                ifb->udp_blue_owned = 0;
            return (-retval);
        }
    } else
        return(-EOPNOTSUPP);
	sip_ifp(ifb, &ifp);
	if (ifnet_type(ifp) == IFT_ETHER) {
		char temp[6];
		ifb->media_addr_size = 6;
		sip_get_ether_addr(ifb, temp);
	}
	else if (ifnet_type(ifp) == IFT_PPP) {
		ifb->media_addr_size = 0;
	}
	else {
        if (in_pcb_rem_share_client(&tcbinfo,
            ifb->udp_blue_owned) == 0)
            ifb->udp_blue_owned = 0;
        if (in_pcb_rem_share_client(&tcbinfo,
            ifb->tcp_blue_owned) == 0)
            ifb->tcp_blue_owned = 0;
		return -ENOTSUP;
	}
    
	if ((retval = ipv4_attach_protofltr(ifp, ifb)) != 0) {
#if SIP_DEBUG_ERR
		log(LOG_WARNING,
			"enable_ipv4: failed, ifb=%d retval=%d\n",
			ifb, retval);
#endif
        if (in_pcb_rem_share_client(&tcbinfo,
            ifb->udp_blue_owned) == 0)
            ifb->udp_blue_owned = 0;
        if (in_pcb_rem_share_client(&tcbinfo,
            ifb->tcp_blue_owned) == 0)
            ifb->tcp_blue_owned = 0;
		return(-retval);
	}
	
	return BFS_IP;
}

/*
 * SharedIP fragments packets in two cases.
 *
 * When the loopback interface is used for Classic<->X communication,
 * it has a much higher MTU than Classic's interface. We must fragment
 * packets.
 * 
 * When packets are received, X's stack is responsible for reassembling
 * them. If they are larger that the MTU for the Classic interface, we
 * must re-fragment them to get them to Classic.
 */
static errno_t
ipv4_fragment(
	mbuf_t	packet,
	int		maxsize)
{
	struct ip	*ip = mbuf_data(packet);
	int			payload_offset = 0;
	int			header_length = ip->ip_hl * 4;
	int			payload_length = ntohs(ip->ip_len) - header_length;
	mbuf_t		first = NULL;
	mbuf_t		last = NULL;
	
	/*
	 * Ignore DF bit, we may be getting a packet through the loopback
	 * interface. We could consider the communication between MacOS X
	 * and Classic another "link", but it wouldn't make sense to send
	 * an ICMP fragmentation required reponse. We will hit this case
	 * with TCP communication between Classic and X.
	 */
	ip->ip_off = ip->ip_off & ~IP_DF;
	
	maxsize &= ~7;
	if (maxsize < 8) {
		mbuf_freem(packet);
		return EINVAL;
	}
	
	/*
	 * Build the list of fragments, start with the second fragment
	 */
	for (payload_offset = maxsize; payload_offset < payload_length;
		 payload_offset += maxsize) {
		mbuf_t		new_pkt;
		mbuf_t		pkt_payload;
		struct ip	*new_ip;
		u_short		length;
		if (mbuf_gethdr(MBUF_WAITOK, MBUF_TYPE_HEADER, &new_pkt) != 0) {
			mbuf_freem(packet);
			if (first)
				mbuf_freem_list(first);
			return ENOBUFS;
		}
		
		/* Skip enough bytes for ethernet header */
		mbuf_setdata(new_pkt, ((u_char*)mbuf_datastart(new_pkt)) + 14, sizeof(*new_ip));
		
		/*
		 * Don't copy options, just copy the ip header.
		 */
		new_ip = mbuf_data(new_pkt);
		*new_ip = *ip;
		new_ip->ip_hl = sizeof(*new_ip) / 4;
		new_ip->ip_off = payload_offset / 8;
		if (payload_offset + maxsize < payload_length) {
			new_ip->ip_off |= IP_MF;
			length = maxsize;
		}
		else {
			length = payload_length - payload_offset;
		}
		new_ip->ip_len = htons(length + sizeof(*new_ip));
		new_ip->ip_off = htons(new_ip->ip_off);
		new_ip->ip_sum = 0;
		new_ip->ip_sum = in_cksum((struct mbuf*)new_pkt, sizeof(*new_ip));
		
		if (mbuf_copym(packet, payload_offset + header_length, length, MBUF_WAITOK, &pkt_payload) != 0) {
			mbuf_freem(packet);
			mbuf_freem(new_pkt);
			if (first)
				mbuf_freem_list(first);
			return ENOBUFS;
		}
		mbuf_setnext(new_pkt, pkt_payload);
		mbuf_pkthdr_setlen(new_pkt, mbuf_len(new_pkt) + mbuf_len(pkt_payload));
		mbuf_pkthdr_setrcvif(new_pkt, mbuf_pkthdr_rcvif(packet));
		if (last == NULL) {
			first = last = new_pkt;
		}
		else {
			mbuf_setnextpkt(last, new_pkt);
			last = new_pkt;
		}
	}
	
	/*
	 * Update first fragment by trimming what's been copied out
	 * and updating header, then send each fragment (in order).
	 */
	mbuf_adj(packet, maxsize - payload_length);
	mbuf_pkthdr_setlen(packet, maxsize + header_length);
	ip->ip_len = htons((u_short)mbuf_pkthdr_len(packet));
	ip->ip_off |= IP_MF;
	ip->ip_off = htons(ip->ip_off);
	ip->ip_sum = 0;
	ip->ip_sum = in_cksum((struct mbuf*)packet, header_length);
	
	mbuf_setnextpkt(packet, first);
	
	return 0;
}

static void
ipv4_inject_classic(
	mbuf_t				packet,
	struct blueCtlBlock	*ifb)
{
	ifnet_t		ifp;
	struct ip	*ip;
	int			maxlen;
	mbuf_t		nextpkt;
	
	ip = mbuf_data(packet);
	
	sip_ifp(ifb, &ifp);
	
	/* We may have to refragment before handing to SharedIP */
	if (mbuf_pkthdr_len(packet) > ifnet_mtu(ifp)) {
		maxlen = (ifnet_mtu(ifp) - (ip->ip_hl * 4)) & ~0x7;
		if (ipv4_fragment(packet, maxlen) != 0)
			return;
	}
		
	switch(ifnet_type(ifp)) {
		case IFT_ETHER: {
			while(packet) {
				nextpkt = mbuf_nextpkt(packet);
				mbuf_setnextpkt(packet, NULL);
				
				/* Prepend an ethernet header */
				struct ether_header *eh;
				
				if (mbuf_prepend(&packet, sizeof(*eh), M_WAITOK) != 0) {
					mbuf_freem(packet);
					break;
				}
				
				eh = mbuf_data(packet);
				ifnet_lladdr_copy_bytes(ifp, eh->ether_dhost, sizeof(eh->ether_dhost));
				if (ip->ip_src.s_addr == ifb->filter[BFS_IP].BF_address) {
					ifnet_lladdr_copy_bytes(ifp, eh->ether_shost, sizeof(eh->ether_shost));
				}
				else {
					bcopy(bogus_ether_addr, eh->ether_shost, sizeof(eh->ether_shost));
				}
				
				eh->ether_type = htons(ETHERTYPE_IP);
				
				blue_inject(ifb, packet);
				packet = nextpkt;
			}
		}
		break;
		
		case IFT_PPP:{
			while(packet) {
				nextpkt = mbuf_nextpkt(packet);
				mbuf_setnextpkt(packet, NULL);
				
				blue_inject(ifb, packet);
				packet = nextpkt;
			}
		}
		break;
		
		default:
			mbuf_freem_list(packet);
	}
}

/*
 * This filter function intercepts incoming packets being delivered to
 *  IPv4 (from an interface) and decides if they should be sent to
 *  X, Classic, or both.
 * Note: the packet is "fully formed", i.e., has a media header.
 * NB: Assumes the X stack is operational
 */
static errno_t
ipv4_infltr(
	void				*cookie,
	mbuf_t				*data,
	int					offset,
	u_int8_t			protocol)
{
	struct ip			ip;
	struct blueCtlBlock *ifb = cookie;
	errno_t				error = 0;
	unsigned char		owner = 0;
	mbuf_csum_request_flags_t	sum_req = 0;
	u_int32_t					sum_value = 0;
	
	/* If this isn't TCP, UDP, or ICMP, bail out immediately */
	if (protocol != IPPROTO_TCP && protocol != IPPROTO_UDP &&
		protocol != IPPROTO_ICMP) {
		return 0;
	}
	
	/* Get a copy of the IP header */
	error = mbuf_copydata(*data, 0, sizeof(ip), &ip);
	
	/*
	 * Hack to generate checksums for loopback interface. (3945063)
	 *
	 * The loopback interface uses a special form of checksums. It suppresses
	 * the calculation of the checksum on transmit and then sets flags indicating
	 * that the checksum was calculated in hardware. This bypasses all checksum
	 * verifcation in the X stack. Unfortunately, these flags have no equl in
	 * the 9 stack, so we must manually generate the checksums for Classic.
	 */
	if (ifnet_type(mbuf_pkthdr_rcvif(*data)) == IFT_LOOP) {
		mbuf_csum_performed_flags_t	performed;
		u_int32_t					temp;
		
		if (mbuf_get_csum_performed(*data, &performed, &temp) == 0) {
			/* If loopback is spoofing ip checksum, we need to calculate it */
			if ((performed & MBUF_CSUM_DID_IP) != 0) {
				sum_req |= MBUF_CSUM_REQ_IP;
			}
			
			/* If loopback is spoofind TCP/UDP checksum, we need to calculate it */
			if ((performed & MBUF_CSUM_DID_DATA) != 0) {
				if (protocol == IPPROTO_TCP) {
					sum_req |= MBUF_CSUM_REQ_TCP;
					sum_value = offsetof(struct tcphdr, th_sum);
				}
				else if (protocol == IPPROTO_UDP) {
					sum_req |= MBUF_CSUM_REQ_UDP;
					sum_value = offsetof(struct udphdr, uh_sum);
				}
			}
		}
	}
	
	switch(protocol) {
		case IPPROTO_TCP: {
			struct tcphdr	tcp;
			unsigned int	lcookie;
			
			/* Get a copy of the TCP header */
			error = mbuf_copydata(*data, offset, sizeof(tcp), &tcp);
			if (error != 0)
				return 0;
			
			/* Check if this packet belongs to Classic */
			owner = in_pcb_get_owner(&tcbinfo, ip.ip_dst, tcp.th_dport,
									 ip.ip_src, tcp.th_sport, &lcookie);
			if (owner == ifb->tcp_blue_owned) {
				if (sum_req) {
					mbuf_set_csum_requested(*data, sum_req, sum_value);
					mbuf_outbound_finalize(*data, PF_INET, 0);
				}
				ipv4_inject_classic(*data, ifb);
				return EJUSTRETURN;
			}
		}
		break;
		
		case IPPROTO_UDP: {
			struct udphdr	udp;
			unsigned int	lcookie;
			
			/* Get a copy of the UDP header */
			error = mbuf_copydata(*data, offset, sizeof(udp), &udp);
			if (error != 0)
				return 0;
			
			/* Check if this packet belongs to Classic */
			owner = in_pcb_get_owner(&udbinfo, ip.ip_dst, udp.uh_dport,
									 ip.ip_src, udp.uh_sport, &lcookie);
			
			if (owner == ifb->tcp_blue_owned) {
				if (sum_req) {
					mbuf_set_csum_requested(*data, sum_req, sum_value);
					mbuf_outbound_finalize(*data, PF_INET, 0);
				}
				ipv4_inject_classic(*data, ifb);
				return EJUSTRETURN;
			}
			else if ((owner & ifb->udp_blue_owned) != 0) {
				mbuf_t	acopy;
				
				if (mbuf_dup(*data, M_WAITOK, &acopy) == 0) {
					if (sum_req) {
						mbuf_set_csum_requested(acopy, sum_req, sum_value);
						mbuf_outbound_finalize(acopy, PF_INET, 0);
					}
					ipv4_inject_classic(acopy, ifb);
				}
			}
		}
		break;
		
		case IPPROTO_ICMP: {
			u_char icmp_type;
			
			if (mbuf_copydata(*data, offset, sizeof(icmp_type), &icmp_type) != 0)
				return 0;
			
			switch (icmp_type) {
				case ICMP_ECHO:
				case ICMP_IREQ:
				case ICMP_MASKREQ:
				case ICMP_TSTAMP:
				break;
				default: {
					mbuf_t	acopy;
					
					if (mbuf_dup(*data, M_WAITOK, &acopy) == 0) {
						if (sum_req) {
							mbuf_set_csum_requested(*data, sum_req, sum_value);
							mbuf_outbound_finalize(*data, PF_INET, 0);
						}
						ipv4_inject_classic(*data, ifb);
						*data = acopy;
					}
				}
					break;
			}
		}
		break;
		
		default:
			break;
	}
	
	return 0;
}

/*
 * Assembles fragments from Classic in to one IP packet.
 *
 * Assumptions: Fragments will come in order, all fragments
 * will be received.
 */
static errno_t
ipv4_reassemble(
	mbuf_t				*data,
	struct blueCtlBlock	*ifb)
{
	struct ip	*ipnew;
	errno_t		result = EJUSTRETURN;
	
	if (mbuf_len(*data) < sizeof(*ipnew)) {
		mbuf_pullup(data, sizeof(*ipnew));
	}
	ipnew = mbuf_data(*data);
	
	if ((ipnew->ip_off & (IP_OFFMASK | IP_MF)) == 0)
		return 0;
	
	sip_lock();
	
	/* Build the list */
	if (ifb->frag_head == NULL) {
		if ((ipnew->ip_off & IP_OFFMASK) != 0)
			printf("SharedIP: Classic did not give us the first fragment first\n");
		ifb->frag_head = *data;
		ifb->frag_last = *data;
	}
	else {
		struct ip	*iphead;
		
		/*
		 * This is safe because we do a pullup before we place
		 * the packet on the list.
		 */
		iphead = mbuf_data(ifb->frag_head);
				
		/* Sanity check */
		if (ipnew->ip_id != iphead->ip_id ||
			ipnew->ip_src.s_addr != iphead->ip_src.s_addr ||
			ipnew->ip_dst.s_addr != iphead->ip_dst.s_addr ||
			(ipnew->ip_off & IP_OFFMASK) << 3 != (iphead->ip_len - (iphead->ip_hl * 4))) {
			printf("SharedIP: Classic broke our fragment assumptions\n");
			mbuf_freem(ifb->frag_head);
			
			if ((ipnew->ip_off & IP_OFFMASK) == 0) {
				ifb->frag_head = *data;
				ifb->frag_last = *data;
				goto done;
			}
			else {
				result = EINVAL;
				goto done;
			}
		}
		
		/* Append this IP fragment */
		iphead->ip_len += ipnew->ip_len - (ipnew->ip_hl << 2);
		mbuf_adj(*data, ipnew->ip_hl << 2);
		mbuf_setnext(ifb->frag_last, *data);
		mbuf_pkthdr_setlen(ifb->frag_head, iphead->ip_len);
		
		if ((ipnew->ip_off & IP_MF) == 0) {
			/* Last fragment */
			iphead->ip_off = 0;
			*data = ifb->frag_head;
			ifb->frag_head = NULL;
			ifb->frag_last = NULL;
			result = 0;
		} else {
			/* Move the last pointer to the last mbuf in the chain */
			ifb->frag_last = *data;
			while (mbuf_next(ifb->frag_last) != NULL) {
				ifb->frag_last = mbuf_next(ifb->frag_last);
			}
		}
	}

done:
	sip_unlock();
	return result;
}

/*
 * We get a packet destined for the outside world, complete with ethernet
 * header.  If it goes to X, we need to get rid of the header.
 *
 * This function will always consume the mbuf, regardless of return value.
 */
static errno_t
classic_out_filter_ip(
	mbuf_t data,
	struct blueCtlBlock *ifb,
    ifnet_t ifp)
{
	struct ip	ip;
	errno_t		error = 0;
	
	/* Get a copy of the IP header */
	error = mbuf_copydata(data, 0, sizeof(ip), &ip);
	if (error) {
		goto cleanup;
	}
	
	/* If this isn't TCP, UDP, or ICMP, bail out immediately */
	if (ip.ip_p != IPPROTO_TCP && ip.ip_p != IPPROTO_UDP &&
		ip.ip_p != IPPROTO_ICMP) {
		error = EAFNOSUPPORT;
		goto cleanup;
	}
	
	/* If this is a fragment, reassemble it */
	if ((ip.ip_off & (IP_MF | IP_OFFMASK)) != 0) {
		error = ipv4_reassemble(&data, ifb);
		
		/* ipv4_reassemble will return EJUSTRETURN if the fragments are incomplete */
		if (error != 0)
			goto cleanup;
		
		error = mbuf_copydata(data, 0, sizeof(ip), &ip);
		if (error)
			goto cleanup;
	}
	
	if (data) {
		error = ipf_inject_output(data, NULL, 0);
		if (error) {
			printf("SharedIP: error %d injecting packet\n", error);
		}
	}
	return error;
	
cleanup:
	if (data && error != EJUSTRETURN)
		mbuf_freem(data);
	
	return error;
}

struct ether_arp_packet {
	struct ether_header	eh;
	struct ether_arp	ea;
};

__private_extern__ errno_t
si_send_eth_ipv4(
	mbuf_t	*m_orig,
	struct blueCtlBlock *ifb,
	ifnet_t	ifp)
{
	struct ether_arp_packet	*eap = NULL;
	struct in_addr	*sip;
	struct in_addr	*tip;
	struct ether_header* enetHeader = NULL;
	
	if ((mbuf_len(*m_orig) >= sizeof(struct ether_header)) ||
		mbuf_pullup(m_orig, sizeof(struct ether_header)) == 0) {
		enetHeader = mbuf_data(*m_orig);
		if (enetHeader->ether_type == ETHERTYPE_ARP) {
			
			/*
			 * Generate a response, Mac OS X's stack
			 * will handle the real ARP.
			 */
			if (mbuf_len(*m_orig) >= sizeof(*eap) ||
				mbuf_pullup(m_orig, sizeof(*eap)) == 0) {
				eap = mbuf_data(*m_orig);
				
				sip = (struct in_addr*)eap->ea.arp_spa;
				tip = (struct in_addr*)eap->ea.arp_tpa;

#if 0
				printf("SharedIP: arp %x:%x:%x:%x:%x:%x (%s) -> ",
					eap->ea.arp_sha[0], eap->ea.arp_sha[1],
					eap->ea.arp_sha[2], eap->ea.arp_sha[3],
					eap->ea.arp_sha[4], eap->ea.arp_sha[5],
					inet_ntoa(*sip));
				printf("%x:%x:%x:%x:%x:%x (%s)\n",
					eap->ea.arp_tha[0], eap->ea.arp_tha[1],
					eap->ea.arp_tha[2], eap->ea.arp_tha[3],
					eap->ea.arp_tha[4], eap->ea.arp_tha[5],
					inet_ntoa(*tip));
#endif
				
				if (eap->ea.ea_hdr.ar_op == ARPOP_REQUEST &&
					tip->s_addr != sip->s_addr &&
					sip->s_addr != 0) {
					struct in_addr	temp_ip;
					eap = mbuf_data(*m_orig);
					
					/* Set the operation to a reply */
					eap->ea.ea_hdr.ar_op = ARPOP_REPLY;
					
					/* Copy the original source hardware to the target */
					bcopy(eap->eh.ether_shost, eap->eh.ether_dhost, sizeof(eap->eh.ether_dhost));
					bcopy(eap->ea.arp_sha, eap->ea.arp_tha, sizeof(eap->ea.arp_tha));
					
					/* Use bogus_ether_addr as the new source */
					bcopy(bogus_ether_addr, eap->eh.ether_shost, sizeof(bogus_ether_addr));
					bcopy(bogus_ether_addr, eap->ea.arp_sha, sizeof(bogus_ether_addr));
					
					/* Swap the source and target IP addresses */
					temp_ip = *sip;
					*sip = *tip;
					*tip = temp_ip;
					
#if 0
					printf("SharedIP: response %x:%x:%x:%x:%x:%x (%s) -> ",
						eap->ea.arp_sha[0], eap->ea.arp_sha[1],
						eap->ea.arp_sha[2], eap->ea.arp_sha[3],
						eap->ea.arp_sha[4], eap->ea.arp_sha[5],
						inet_ntoa(*sip));
					printf("%x:%x:%x:%x:%x:%x (%s)\n",
						eap->ea.arp_tha[0], eap->ea.arp_tha[1],
						eap->ea.arp_tha[2], eap->ea.arp_tha[3],
						eap->ea.arp_tha[4], eap->ea.arp_tha[5],
						inet_ntoa(*tip));
#endif

					blue_inject(ifb, *m_orig);
					return EJUSTRETURN;
				}
			}
		}
		else if (enetHeader->ether_type == ETHERTYPE_IP) {
			mbuf_adj(*m_orig, sizeof(struct ether_header));
			classic_out_filter_ip(*m_orig, ifb, ifp);
			return EJUSTRETURN;
		}
	}
	mbuf_freem(*m_orig);
	return EJUSTRETURN;
}

__private_extern__ errno_t
si_send_ppp_ipv4(
	mbuf_t	*m_orig,
	struct blueCtlBlock *ifb,
	ifnet_t	ifp)
{
	u_int16_t	*ppp_type;
	
	if (mbuf_len(*m_orig) >= sizeof(*ppp_type) ||
		mbuf_pullup(m_orig, sizeof(*ppp_type)) == 0) {
		ppp_type = mbuf_data(*m_orig);
		if (*ppp_type == htons(0x21)) {
			mbuf_adj(*m_orig, sizeof(*ppp_type));
			classic_out_filter_ip(*m_orig, ifb, ifp);
			return EJUSTRETURN;
		}
	}
	
	mbuf_freem(*m_orig);
	return EJUSTRETURN;
}

/*
 * There are three proto filters inserted here:
 *  - one on the input side, handling incoming from the chosen I/F
 *  - two on the output side, handling outbound from the loopback
 *    and chosen I/F's.
 * The filters for the interface depends on the interface type.
 */
int
ipv4_attach_protofltr(ifnet_t ifp, struct blueCtlBlock *ifb)
{
	struct ipf_filter	ip_filter;
	errno_t				error = 0;
	
	if (ifb->ip_filter)
		return EBUSY;
	
	bzero(&ip_filter, sizeof(ip_filter));
	ip_filter.cookie = (caddr_t)ifb;
	ip_filter.name = "com.apple.nke.SharedIP";
	ip_filter.ipf_input = ipv4_infltr;
	ip_filter.ipf_detach = ipv4_detach;
	ifb_reference(ifb);
	
	error = ipf_addv4(&ip_filter, &ifb->ip_filter);
	if (error != 0)
		ifb_release(ifb);
	
	return error;
}

int
ipv4_stop(struct blueCtlBlock *ifb)
{
    int retval = 0;

    if (ifb == NULL)
        return(0);
    
    ifb->ipv4_stopping = 1;
    /* deregister TCP & UDP port sharing if needed */
    if (ifb->tcp_blue_owned) {
        if ((retval = in_pcb_rem_share_client(&tcbinfo, ifb->tcp_blue_owned)) != 0) {
#if SIP_DEBUG_ERR
            log(LOG_WARNING, "STOP/TCP: %d\n", retval);
#endif
            goto error;
        }
    }
    ifb->tcp_blue_owned = 0;
    if (ifb->udp_blue_owned) {
        if ((retval = in_pcb_rem_share_client(&udbinfo, ifb->udp_blue_owned)) != 0) {
#if SIP_DEBUG_ERR
            log(LOG_WARNING, "STOP/UDP: %d\n", retval);
#endif
            goto error;
        }
    }
    ifb->udp_blue_owned = 0;
    if (ifb->ip_filter) {
#if SIP_DEBUG
        log(LOG_WARNING,
            "ipv4_stop: deregister IPv4 filter ref=%x\n",
            ifb->ip_filter);
#endif
        ipf_remove(ifb->ip_filter);
    }

    /* Fall through */
error:
    ifb->ipv4_stopping = 0;
    return retval;
}

__private_extern__ int
ipv4_control(socket_t so, struct sopt_shared_port_param *psp,
	     struct blueCtlBlock *ifb, int cmd)
{
    int retval;
    struct inpcbinfo *pcbinfo;
    u_char owner = 0;
    u_short lport;
    switch(cmd) {
        case SO_PORT_RESERVE: {
#if SIP_DEBUG
            log(LOG_WARNING,
                "ipv4_control: so=%x SO_PORT_RESERVE laddr=%x lport=%d proto=%d fport=%d faddr=%x\n",
                so, psp->laddr.s_addr, psp->lport, psp->proto,
                psp->fport, psp->faddr.s_addr);
#endif
#if SIP_DEBUG_INFO
            log(LOG_WARNING,
                "SIP-NKE: so=%x SO_PORT_RESERVE laddr=%x lport=%d proto=%d\n",
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
                        "ipv4_control: unsuported (%x) proto port request so=%x\n",
                        psp->proto, so);
#endif
                    return (EPROTONOSUPPORT);
            }

            /*
             * Call the X TCP/IP stack with the releavant
             * port information
             */
            psp->faddr.s_addr	= 0;
            psp->fport 		= 0;
            lport = psp->lport;
            retval = in_pcb_grab_port(pcbinfo, (u_short)psp->flags,
                                        psp->laddr,  &lport,
                                        psp->faddr, (u_short)psp->fport,
                                        psp->cookie,  (u_char)owner);
#if SIP_DEBUG_INFO
            log(LOG_WARNING,
                "SIP-NKE: so=%x SO_PORT_RESERVE laddr=%x lport=%d, retval=%d\n",
                so, psp->laddr.s_addr, lport, retval);
#endif
            if (retval) {
#if SIP_DEBUG_ERR
				if (retval != EADDRINUSE) {
					log(LOG_WARNING,
						"ipv4_control: in_pcb_grab_port retval=%d so=%x\n",
						retval, so);
				}
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
        case SO_PORT_RELEASE: {
#if SIP_DEBUG
            log(LOG_WARNING,
                "ipv4_control: so=%x SO_PORT_RELEASE laddr=%x lport=%d proto=%d\n",
                so, psp->laddr.s_addr, psp->lport, psp->proto);
#endif
#if SIP_DEBUG_INFO
            log(LOG_WARNING,
                "SIP-NKE: so=%x SO_PORT_RELEASE laddr=%x lport=%d proto=%d\n",
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
            psp->faddr.s_addr = 0;
            psp->fport 	  = 0;
            retval = in_pcb_letgo_port (pcbinfo, psp->laddr,
                                (u_short)psp->lport, psp->faddr,
                                        psp->fport, owner);
#if SIP_DEBUG_INFO
            log(LOG_WARNING,
                "SIP-NKE: so=%x SO_PORT_RELEASE laddr=%x lport=%d retval=%d\n",
                so, psp->laddr.s_addr, psp->lport, retval);
#endif
            if (retval) {
#if SIP_DEBUG_ERR
				if (retval != 2) {
					log(LOG_WARNING,
						"ipv4_control: in_pcb_letgo_port retval=%d so=%x\n",
						retval, so);
				}
#endif
                return (retval);
            }
            return(0);
            break;
        }
        case SO_PORT_LOOKUP: {
#if SIP_DEBUG
            log(LOG_WARNING,
                "ipv4_control: so=%x SO_PORT_LOOKUP laddr=%x lport=%d proto=%d\n",
                so, psp->laddr.s_addr, psp->lport, psp->proto);
#endif
#if SIP_DEBUG_INFO
            log(LOG_WARNING,
                "SIP-NKE: so=%x SO_PORT_LOOKUP laddr=%x lport=%d proto=%d\n",
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

/* Handle our filter being detached */
static void
ipv4_detach(
	void	*cookie)
{
    struct blueCtlBlock *ifb = cookie;
    
	sip_lock();
    ifb->ip_filter = 0;
    if (ifb->frag_head) {
    	mbuf_freem(ifb->frag_head);
    	ifb->frag_head = NULL;
    	ifb->frag_last = NULL;
    }
	sip_unlock();
    
    if (!ifb->ipv4_stopping)
    {
        /*
         * We're being detached outside the context of
         * ipv4_stop, call ipv4_stop to cleanup.
         */
        ipv4_stop(ifb);
    }
	
	ifb_release(ifb);
}
